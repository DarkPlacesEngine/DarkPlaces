/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_phys.c

#include "quakedef.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t sv_friction = {CVAR_NOTIFY, "sv_friction","4"};
cvar_t sv_stopspeed = {CVAR_NOTIFY, "sv_stopspeed","100"};
cvar_t sv_gravity = {CVAR_NOTIFY, "sv_gravity","800"};
cvar_t sv_maxvelocity = {CVAR_NOTIFY, "sv_maxvelocity","2000"};
cvar_t sv_nostep = {CVAR_NOTIFY, "sv_nostep","0"};
cvar_t sv_stepheight = {CVAR_NOTIFY, "sv_stepheight", "18"};
cvar_t sv_jumpstep = {CVAR_NOTIFY, "sv_jumpstep", "1"};
cvar_t sv_wallfriction = {CVAR_NOTIFY, "sv_wallfriction", "1"};
cvar_t sv_newflymove = {CVAR_NOTIFY, "sv_newflymove", "0"};
cvar_t sv_freezenonclients = {CVAR_NOTIFY, "sv_freezenonclients", "0"};
cvar_t sv_playerphysicsqc = {CVAR_NOTIFY, "sv_playerphysicsqc", "1"};

#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (prvm_edict_t *ent);

void SV_Phys_Init (void)
{
	Cvar_RegisterVariable(&sv_stepheight);
	Cvar_RegisterVariable(&sv_jumpstep);
	Cvar_RegisterVariable(&sv_wallfriction);
	Cvar_RegisterVariable(&sv_newflymove);
	Cvar_RegisterVariable(&sv_freezenonclients);

	Cvar_RegisterVariable(&sv_playerphysicsqc);
}

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts (void)
{
	int e;
	prvm_edict_t *check;

	// see if any solid entities are inside the final position
	check = PRVM_NEXT_EDICT(prog->edicts);
	for (e = 1;e < prog->num_edicts;e++, check = PRVM_NEXT_EDICT(check))
	{
		if (check->priv.server->free)
			continue;
		if (check->fields.server->movetype == MOVETYPE_PUSH
		 || check->fields.server->movetype == MOVETYPE_NONE
		 || check->fields.server->movetype == MOVETYPE_FOLLOW
		 || check->fields.server->movetype == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Con_Print("entity in invalid position\n");
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity (prvm_edict_t *ent)
{
	int i;
	float wishspeed;

//
// bound velocity
//
	for (i=0 ; i<3 ; i++)
	{
		if (IS_NAN(ent->fields.server->velocity[i]))
		{
			Con_Printf("Got a NaN velocity on %s\n", PRVM_GetString(ent->fields.server->classname));
			ent->fields.server->velocity[i] = 0;
		}
		if (IS_NAN(ent->fields.server->origin[i]))
		{
			Con_Printf("Got a NaN origin on %s\n", PRVM_GetString(ent->fields.server->classname));
			ent->fields.server->origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct(ent->fields.server->velocity, ent->fields.server->velocity);
	if (wishspeed > sv_maxvelocity.value * sv_maxvelocity.value)
	{
		wishspeed = sv_maxvelocity.value / sqrt(wishspeed);
		ent->fields.server->velocity[0] *= wishspeed;
		ent->fields.server->velocity[1] *= wishspeed;
		ent->fields.server->velocity[2] *= wishspeed;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean SV_RunThink (prvm_edict_t *ent)
{
	float thinktime;

	thinktime = ent->fields.server->nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time)
		thinktime = sv.time;

	ent->fields.server->nextthink = 0;
	prog->globals.server->time = thinktime;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	PRVM_ExecuteProgram (ent->fields.server->think, "QC function self.think is missing");
	return !ent->priv.server->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (prvm_edict_t *e1, prvm_edict_t *e2)
{
	int old_self, old_other;

	old_self = prog->globals.server->self;
	old_other = prog->globals.server->other;

	prog->globals.server->time = sv.time;
	if (e1->fields.server->touch && e1->fields.server->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e2);
		PRVM_ExecuteProgram (e1->fields.server->touch, "QC function self.touch is missing");
	}

	if (e2->fields.server->touch && e2->fields.server->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e1);
		PRVM_ExecuteProgram (e2->fields.server->touch, "QC function self.touch is missing");
	}

	prog->globals.server->self = old_self;
	prog->globals.server->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define STOP_EPSILON 0.1
void ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int i;
	float backoff;

	backoff = -DotProduct (in, normal) * overbounce;
	VectorMA(in, backoff, normal, out);

	for (i = 0;i < 3;i++)
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If stepnormal is not NULL, the plane normal of any vertical wall hit will be stored
============
*/
// LordHavoc: increased from 5 to 32
#define MAX_CLIP_PLANES 32
int SV_FlyMove (prvm_edict_t *ent, float time, float *stepnormal)
{
	int blocked, bumpcount;
	int i, j, impact, numplanes;
	float d, time_left;
	vec3_t dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t trace;
	blocked = 0;
	VectorCopy(ent->fields.server->velocity, original_velocity);
	VectorCopy(ent->fields.server->velocity, primal_velocity);
	numplanes = 0;
	time_left = time;
	for (bumpcount = 0;bumpcount < MAX_CLIP_PLANES;bumpcount++)
	{
		if (!ent->fields.server->velocity[0] && !ent->fields.server->velocity[1] && !ent->fields.server->velocity[2])
			break;

		VectorMA(ent->fields.server->origin, time_left, ent->fields.server->velocity, end);
		trace = SV_Move(ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent);
#if 0
		//if (trace.fraction < 0.002)
		{
#if 1
			vec3_t start;
			trace_t testtrace;
			VectorCopy(ent->fields.server->origin, start);
			start[2] += 3;//0.03125;
			VectorMA(ent->fields.server->origin, time_left, ent->fields.server->velocity, end);
			end[2] += 3;//0.03125;
			testtrace = SV_Move(start, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent);
			if (trace.fraction < testtrace.fraction && !testtrace.startsolid && (testtrace.fraction == 1 || DotProduct(trace.plane.normal, ent->fields.server->velocity) < DotProduct(testtrace.plane.normal, ent->fields.server->velocity)))
			{
				Con_Printf("got further (new %f > old %f)\n", testtrace.fraction, trace.fraction);
				trace = testtrace;
			}
#endif
#if 0
			//j = -1;
			for (i = 0;i < numplanes;i++)
			{
				VectorCopy(ent->fields.server->origin, start);
				VectorMA(ent->fields.server->origin, time_left, ent->fields.server->velocity, end);
				VectorMA(start, 3, planes[i], start);
				VectorMA(end, 3, planes[i], end);
				testtrace = SV_Move(start, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent);
				if (trace.fraction < testtrace.fraction)
				{
					trace = testtrace;
					VectorCopy(start, ent->fields.server->origin);
					//j = i;
				}
			}
			//if (j >= 0)
			//	VectorAdd(ent->fields.server->origin, planes[j], start);
#endif
		}
#endif

#if 0
		Con_Printf("entity %i bump %i: velocity %f %f %f trace %f", ent - prog->edicts, bumpcount, ent->fields.server->velocity[0], ent->fields.server->velocity[1], ent->fields.server->velocity[2], trace.fraction);
		if (trace.fraction < 1)
			Con_Printf(" : %f %f %f", trace.plane.normal[0], trace.plane.normal[1], trace.plane.normal[2]);
		Con_Print("\n");
#endif

		/*
		if (trace.startsolid)
		{
			// LordHavoc: note: this code is what makes entities stick in place if embedded in another object (which can be the world)
			// entity is trapped in another solid
			VectorClear(ent->fields.server->velocity);
			return 3;
		}
		*/

		// break if it moved the entire distance
		if (trace.fraction == 1)
		{
			VectorCopy(trace.endpos, ent->fields.server->origin);
			break;
		}

		if (!trace.ent)
		{
			Con_Printf ("SV_FlyMove: !trace.ent");
			trace.ent = prog->edicts;
		}

		if (((int) ent->fields.server->flags & FL_ONGROUND) && ent->fields.server->groundentity == PRVM_EDICT_TO_PROG(trace.ent))
			impact = false;
		else
		{
			ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
			impact = true;
		}

		if (trace.plane.normal[2])
		{
			if (trace.plane.normal[2] > 0.7)
			{
				// floor
				blocked |= 1;
				ent->fields.server->flags = (int)ent->fields.server->flags | FL_ONGROUND;
				ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
			}
		}
		else
		{
			// step
			blocked |= 2;
			// save the trace for player extrafriction
			if (stepnormal)
				VectorCopy(trace.plane.normal, stepnormal);
		}

		if (trace.fraction >= 0.001)
		{
			// actually covered some distance
			VectorCopy(trace.endpos, ent->fields.server->origin);
			VectorCopy(ent->fields.server->velocity, original_velocity);
			numplanes = 0;
		}

		// run the impact function
		if (impact)
		{
			SV_Impact(ent, (prvm_edict_t *)trace.ent);

			// break if removed by the impact function
			if (ent->priv.server->free)
				break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->fields.server->velocity);
			blocked = 3;
			break;
		}

		/*
		for (i = 0;i < numplanes;i++)
			if (DotProduct(trace.plane.normal, planes[i]) > 0.99)
				break;
		if (i < numplanes)
		{
			VectorAdd(ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity);
			continue;
		}
		*/

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		if (sv_newflymove.integer)
			ClipVelocity(ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 1);
		else
		{
			// modify original_velocity so it parallels all of the clip planes
			for (i = 0;i < numplanes;i++)
			{
				ClipVelocity(original_velocity, planes[i], new_velocity, 1);
				for (j = 0;j < numplanes;j++)
				{
					if (j != i)
					{
						// not ok
						if (DotProduct(new_velocity, planes[j]) < 0)
							break;
					}
				}
				if (j == numplanes)
					break;
			}

			if (i != numplanes)
			{
				// go along this plane
				VectorCopy(new_velocity, ent->fields.server->velocity);
			}
			else
			{
				// go along the crease
				if (numplanes != 2)
				{
					VectorClear(ent->fields.server->velocity);
					blocked = 7;
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				// LordHavoc: thanks to taniwha of QuakeForge for pointing out this fix for slowed falling in corners
				VectorNormalize(dir);
				d = DotProduct(dir, ent->fields.server->velocity);
				VectorScale(dir, d, ent->fields.server->velocity);
			}
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->fields.server->velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->fields.server->velocity);
			break;
		}
	}

	//Con_Printf("entity %i final: blocked %i velocity %f %f %f\n", ent - prog->edicts, blocked, ent->fields.server->velocity[0], ent->fields.server->velocity[1], ent->fields.server->velocity[2]);

	/*
	if ((blocked & 1) == 0 && bumpcount > 1)
	{
		// LordHavoc: fix the 'fall to your death in a wedge corner' glitch
		// flag ONGROUND if there's ground under it
		trace = SV_Move(ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent);
	}
	*/
	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity (prvm_edict_t *ent)
{
	float ent_gravity;
	prvm_eval_t *val;

	val = PRVM_GETEDICTFIELDVALUE(ent, eval_gravity);
	if (val!=0 && val->_float)
		ent_gravity = val->_float;
	else
		ent_gravity = 1.0;
	ent->fields.server->velocity[2] -= ent_gravity * sv_gravity.value * sv.frametime;
}


/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity (prvm_edict_t *ent, vec3_t push)
{
	int type;
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->fields.server->origin, push, end);

	if (ent->fields.server->movetype == MOVETYPE_FLYMISSILE)
		type = MOVE_MISSILE;
	else if (ent->fields.server->solid == SOLID_TRIGGER || ent->fields.server->solid == SOLID_NOT)
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else
		type = MOVE_NORMAL;

	trace = SV_Move (ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, type, ent);

	VectorCopy (trace.endpos, ent->fields.server->origin);
	SV_LinkEdict (ent, true);

	if (trace.ent && (!((int)ent->fields.server->flags & FL_ONGROUND) || ent->fields.server->groundentity != PRVM_EDICT_TO_PROG(trace.ent)))
		SV_Impact (ent, (prvm_edict_t *)trace.ent);
	return trace;
}


/*
============
SV_PushMove

============
*/
void SV_PushMove (prvm_edict_t *pusher, float movetime)
{
	int i, e, index;
	float savesolid, movetime2, pushltime;
	vec3_t mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org;
	int num_moved;
	int numcheckentities;
	static prvm_edict_t *checkentities[MAX_EDICTS];
	model_t *pushermodel;
	trace_t trace;

	if (!pusher->fields.server->velocity[0] && !pusher->fields.server->velocity[1] && !pusher->fields.server->velocity[2] && !pusher->fields.server->avelocity[0] && !pusher->fields.server->avelocity[1] && !pusher->fields.server->avelocity[2])
	{
		pusher->fields.server->ltime += movetime;
		return;
	}

	switch ((int) pusher->fields.server->solid)
	{
	// LordHavoc: valid pusher types
	case SOLID_BSP:
	case SOLID_BBOX:
	case SOLID_SLIDEBOX:
	case SOLID_CORPSE: // LordHavoc: this would be weird...
		break;
	// LordHavoc: no collisions
	case SOLID_NOT:
	case SOLID_TRIGGER:
		VectorMA (pusher->fields.server->origin, movetime, pusher->fields.server->velocity, pusher->fields.server->origin);
		VectorMA (pusher->fields.server->angles, movetime, pusher->fields.server->avelocity, pusher->fields.server->angles);
		pusher->fields.server->angles[0] -= 360.0 * floor(pusher->fields.server->angles[0] * (1.0 / 360.0));
		pusher->fields.server->angles[1] -= 360.0 * floor(pusher->fields.server->angles[1] * (1.0 / 360.0));
		pusher->fields.server->angles[2] -= 360.0 * floor(pusher->fields.server->angles[2] * (1.0 / 360.0));
		pusher->fields.server->ltime += movetime;
		SV_LinkEdict (pusher, false);
		return;
	default:
		Con_Printf("SV_PushMove: unrecognized solid type %f\n", pusher->fields.server->solid);
		return;
	}
	index = (int) pusher->fields.server->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		Con_Printf("SV_PushMove: invalid modelindex %f\n", pusher->fields.server->modelindex);
		return;
	}
	pushermodel = sv.models[index];

	movetime2 = movetime;
	VectorScale(pusher->fields.server->velocity, movetime2, move1);
	VectorScale(pusher->fields.server->avelocity, movetime2, moveangle);
	if (moveangle[0] || moveangle[2])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->fields.server->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->fields.server->origin[i] + 1;
			}
		}
	}
	else if (moveangle[1])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->yawmins[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->fields.server->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->fields.server->origin[i] + 1;
			}
		}
	}
	else
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->normalmins[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->fields.server->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->fields.server->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->fields.server->origin[i] + 1;
			}
		}
	}

	VectorNegate (moveangle, a);
	AngleVectorsFLU (a, forward, left, up);

	VectorCopy (pusher->fields.server->origin, pushorig);
	VectorCopy (pusher->fields.server->angles, pushang);
	pushltime = pusher->fields.server->ltime;

// move the pusher to its final position

	VectorMA (pusher->fields.server->origin, movetime, pusher->fields.server->velocity, pusher->fields.server->origin);
	VectorMA (pusher->fields.server->angles, movetime, pusher->fields.server->avelocity, pusher->fields.server->angles);
	pusher->fields.server->ltime += movetime;
	SV_LinkEdict (pusher, false);

	savesolid = pusher->fields.server->solid;

// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = SV_EntitiesInBox(mins, maxs, MAX_EDICTS, checkentities);
	for (e = 0;e < numcheckentities;e++)
	{
		prvm_edict_t *check = checkentities[e];
		if (check->fields.server->movetype == MOVETYPE_NONE
		 || check->fields.server->movetype == MOVETYPE_PUSH
		 || check->fields.server->movetype == MOVETYPE_FOLLOW
		 || check->fields.server->movetype == MOVETYPE_NOCLIP
		 || check->fields.server->movetype == MOVETYPE_FAKEPUSH)
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if (!(((int)check->fields.server->flags & FL_ONGROUND) && PRVM_PROG_TO_EDICT(check->fields.server->groundentity) == pusher))
		{
			// if the entity is not inside the pusher's final position, leave it alone
			if (!SV_ClipMoveToEntity(pusher, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, 0, SUPERCONTENTS_SOLID).startsolid)
				continue;
			// remove the onground flag for non-players
			if (check->fields.server->movetype != MOVETYPE_WALK)
				check->fields.server->flags = (int)check->fields.server->flags & ~FL_ONGROUND;
		}


		if (forward[0] != 1 || left[1] != 1) // quick way to check if any rotation is used
		{
			vec3_t org2;
			VectorSubtract (check->fields.server->origin, pusher->fields.server->origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, left);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move);
			VectorAdd (move, move1, move);
		}
		else
			VectorCopy (move1, move);

		VectorCopy (check->fields.server->origin, check->priv.server->moved_from);
		VectorCopy (check->fields.server->angles, check->priv.server->moved_fromangles);
		sv.moved_edicts[num_moved++] = check;

		// try moving the contacted entity
		pusher->fields.server->solid = SOLID_NOT;
		trace = SV_PushEntity (check, move);
		// FIXME: turn players specially
		check->fields.server->angles[1] += trace.fraction * moveangle[1];
		pusher->fields.server->solid = savesolid; // was SOLID_BSP

		// if it is still inside the pusher, block
		if (SV_ClipMoveToEntity(pusher, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, 0, SUPERCONTENTS_SOLID).startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->fields.server->solid = SOLID_NOT;
			VectorScale(move, 1.1, move2);
			VectorCopy (check->priv.server->moved_from, check->fields.server->origin);
			VectorCopy (check->priv.server->moved_fromangles, check->fields.server->angles);
			SV_PushEntity (check, move2);
			pusher->fields.server->solid = savesolid;
			if (SV_ClipMoveToEntity(pusher, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, 0, SUPERCONTENTS_SOLID).startsolid)
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->fields.server->solid = SOLID_NOT;
				VectorScale(move, 0.9, move2);
				VectorCopy (check->priv.server->moved_from, check->fields.server->origin);
				VectorCopy (check->priv.server->moved_fromangles, check->fields.server->angles);
				SV_PushEntity (check, move2);
				pusher->fields.server->solid = savesolid;
				if (SV_ClipMoveToEntity(pusher, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, 0, SUPERCONTENTS_SOLID).startsolid)
				{
					// still inside pusher, so it's really blocked

					// fail the move
					if (check->fields.server->mins[0] == check->fields.server->maxs[0])
						continue;
					if (check->fields.server->solid == SOLID_NOT || check->fields.server->solid == SOLID_TRIGGER)
					{
						// corpse
						check->fields.server->mins[0] = check->fields.server->mins[1] = 0;
						VectorCopy (check->fields.server->mins, check->fields.server->maxs);
						continue;
					}

					VectorCopy (pushorig, pusher->fields.server->origin);
					VectorCopy (pushang, pusher->fields.server->angles);
					pusher->fields.server->ltime = pushltime;
					SV_LinkEdict (pusher, false);

					// move back any entities we already moved
					for (i = 0;i < num_moved;i++)
					{
						prvm_edict_t *ed = sv.moved_edicts[i];
						VectorCopy (ed->priv.server->moved_from, ed->fields.server->origin);
						VectorCopy (ed->priv.server->moved_fromangles, ed->fields.server->angles);
						SV_LinkEdict (ed, false);
					}

					// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
					if (pusher->fields.server->blocked)
					{
						prog->globals.server->self = PRVM_EDICT_TO_PROG(pusher);
						prog->globals.server->other = PRVM_EDICT_TO_PROG(check);
						PRVM_ExecuteProgram (pusher->fields.server->blocked, "QC function self.blocked is missing");
					}
					break;
				}
			}
		}
	}
	pusher->fields.server->angles[0] -= 360.0 * floor(pusher->fields.server->angles[0] * (1.0 / 360.0));
	pusher->fields.server->angles[1] -= 360.0 * floor(pusher->fields.server->angles[1] * (1.0 / 360.0));
	pusher->fields.server->angles[2] -= 360.0 * floor(pusher->fields.server->angles[2] * (1.0 / 360.0));
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher (prvm_edict_t *ent)
{
	float thinktime, oldltime, movetime;

	oldltime = ent->fields.server->ltime;

	thinktime = ent->fields.server->nextthink;
	if (thinktime < ent->fields.server->ltime + sv.frametime)
	{
		movetime = thinktime - ent->fields.server->ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = sv.frametime;

	if (movetime)
		// advances ent->fields.server->ltime if not blocked
		SV_PushMove (ent, movetime);

	if (thinktime > oldltime && thinktime <= ent->fields.server->ltime)
	{
		ent->fields.server->nextthink = 0;
		prog->globals.server->time = sv.time;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (ent->fields.server->think, "QC function self.think is missing");
	}
}


/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck (prvm_edict_t *ent)
{
	int i, j, z;
	vec3_t org;

	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy (ent->fields.server->origin, ent->fields.server->oldorigin);
		return;
	}

	VectorCopy (ent->fields.server->origin, org);
	VectorCopy (ent->fields.server->oldorigin, ent->fields.server->origin);
	if (!SV_TestEntityPosition(ent))
	{
		Con_DPrint("Unstuck.\n");
		SV_LinkEdict (ent, true);
		return;
	}

	for (z=0 ; z< 18 ; z++)
		for (i=-1 ; i <= 1 ; i++)
			for (j=-1 ; j <= 1 ; j++)
			{
				ent->fields.server->origin[0] = org[0] + i;
				ent->fields.server->origin[1] = org[1] + j;
				ent->fields.server->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_DPrint("Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}

	VectorCopy (org, ent->fields.server->origin);
	Con_DPrint("player is stuck.\n");
}


/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater (prvm_edict_t *ent)
{
	int cont;
	vec3_t point;

	point[0] = ent->fields.server->origin[0];
	point[1] = ent->fields.server->origin[1];
	point[2] = ent->fields.server->origin[2] + ent->fields.server->mins[2] + 1;

	ent->fields.server->waterlevel = 0;
	ent->fields.server->watertype = CONTENTS_EMPTY;
	cont = SV_PointSuperContents(point);
	if (cont & (SUPERCONTENTS_LIQUIDSMASK))
	{
		ent->fields.server->watertype = Mod_Q1BSP_NativeContentsFromSuperContents(NULL, cont);
		ent->fields.server->waterlevel = 1;
		point[2] = ent->fields.server->origin[2] + (ent->fields.server->mins[2] + ent->fields.server->maxs[2])*0.5;
		if (SV_PointSuperContents(point) & (SUPERCONTENTS_LIQUIDSMASK))
		{
			ent->fields.server->waterlevel = 2;
			point[2] = ent->fields.server->origin[2] + ent->fields.server->view_ofs[2];
			if (SV_PointSuperContents(point) & (SUPERCONTENTS_LIQUIDSMASK))
				ent->fields.server->waterlevel = 3;
		}
	}

	return ent->fields.server->waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction (prvm_edict_t *ent, float *stepnormal)
{
	float d, i;
	vec3_t forward, into, side;

	AngleVectors (ent->fields.server->v_angle, forward, NULL, NULL);
	if ((d = DotProduct (stepnormal, forward) + 0.5) < 0)
	{
		// cut the tangential velocity
		i = DotProduct (stepnormal, ent->fields.server->velocity);
		VectorScale (stepnormal, i, into);
		VectorSubtract (ent->fields.server->velocity, into, side);
		ent->fields.server->velocity[0] = side[0] * (1 + d);
		ent->fields.server->velocity[1] = side[1] * (1 + d);
	}
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int SV_TryUnstick (prvm_edict_t *ent, vec3_t oldvel)
{
	int i, clip;
	vec3_t oldorg, dir;

	VectorCopy (ent->fields.server->origin, oldorg);
	VectorClear (dir);

	for (i=0 ; i<8 ; i++)
	{
		// try pushing a little in an axial direction
		switch (i)
		{
			case 0: dir[0] = 2; dir[1] = 0; break;
			case 1: dir[0] = 0; dir[1] = 2; break;
			case 2: dir[0] = -2; dir[1] = 0; break;
			case 3: dir[0] = 0; dir[1] = -2; break;
			case 4: dir[0] = 2; dir[1] = 2; break;
			case 5: dir[0] = -2; dir[1] = 2; break;
			case 6: dir[0] = 2; dir[1] = -2; break;
			case 7: dir[0] = -2; dir[1] = -2; break;
		}

		SV_PushEntity (ent, dir);

		// retry the original move
		ent->fields.server->velocity[0] = oldvel[0];
		ent->fields.server->velocity[1] = oldvel[1];
		ent->fields.server->velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, NULL);

		if (fabs(oldorg[1] - ent->fields.server->origin[1]) > 4
		 || fabs(oldorg[0] - ent->fields.server->origin[0]) > 4)
		{
			Con_DPrint("TryUnstick - success.\n");
			return clip;
		}

		// go back to the original pos and try again
		VectorCopy (oldorg, ent->fields.server->origin);
	}

	// still not moving
	VectorClear (ent->fields.server->velocity);
	Con_DPrint("TryUnstick - failure.\n");
	return 7;
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove (prvm_edict_t *ent)
{
	int clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity;
	vec3_t upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;
	trace_t downtrace;

	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->fields.server->flags & FL_ONGROUND;
	ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;

	VectorCopy (ent->fields.server->origin, start_origin);
	VectorCopy (ent->fields.server->velocity, start_velocity);

	clip = SV_FlyMove (ent, sv.frametime, NULL);

	SV_CheckVelocity(ent);

	VectorCopy(ent->fields.server->origin, originalmove_origin);
	VectorCopy(ent->fields.server->velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->fields.server->flags;
	originalmove_groundentity = ent->fields.server->groundentity;

	if ((int)ent->fields.server->flags & FL_WATERJUMP)
		return;

	if (sv_nostep.integer)
		return;

	// if move didn't block on a step, return
	if (clip & 2)
	{
		// if move was not trying to move into the step, return
		if (fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if (ent->fields.server->movetype != MOVETYPE_FLY)
		{
			// return if gibbed by a trigger
			if (ent->fields.server->movetype != MOVETYPE_WALK)
				return;

			// only step up while jumping if that is enabled
			if (!(sv_jumpstep.integer && sv_gameplayfix_stepwhilejumping.integer))
				if (!oldonground && ent->fields.server->waterlevel == 0)
					return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy (start_origin, ent->fields.server->origin);
		VectorCopy (start_velocity, ent->fields.server->velocity);

		// move up
		VectorClear (upmove);
		upmove[2] = sv_stepheight.value;
		// FIXME: don't link?
		SV_PushEntity(ent, upmove);

		// move forward
		ent->fields.server->velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, stepnormal);
		ent->fields.server->velocity[2] += start_velocity[2];

		SV_CheckVelocity(ent);

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if (clip
		 && fabs(originalmove_origin[1] - ent->fields.server->origin[1]) < 0.03125
		 && fabs(originalmove_origin[0] - ent->fields.server->origin[0]) < 0.03125)
		{
			//Con_Printf("wall\n");
			// stepping up didn't make any progress, revert to original move
			VectorCopy(originalmove_origin, ent->fields.server->origin);
			VectorCopy(originalmove_velocity, ent->fields.server->velocity);
			//clip = originalmove_clip;
			ent->fields.server->flags = originalmove_flags;
			ent->fields.server->groundentity = originalmove_groundentity;
			// now try to unstick if needed
			//clip = SV_TryUnstick (ent, oldvel);
			return;
		}

		//Con_Printf("step - ");

		// extra friction based on view angle
		if (clip & 2 && sv_wallfriction.integer)
			SV_WallFriction (ent, stepnormal);
	}
	// skip out if stepdown is enabled, moving downward, not in water, and the move started onground and ended offground
	else if (!(sv_gameplayfix_stepdown.integer && ent->fields.server->waterlevel < 2 && start_velocity[2] < (1.0 / 32.0) && oldonground && !((int)ent->fields.server->flags & FL_ONGROUND)))
		return;

	// move down
	VectorClear (downmove);
	downmove[2] = -sv_stepheight.value + start_velocity[2]*sv.frametime;
	// FIXME: don't link?
	downtrace = SV_PushEntity (ent, downmove);

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (ent->fields.server->solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			ent->fields.server->flags =	(int)ent->fields.server->flags | FL_ONGROUND;
			ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(downtrace.ent);
		}
	}
	else
	{
		//Con_Printf("slope\n");
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(originalmove_origin, ent->fields.server->origin);
		VectorCopy(originalmove_velocity, ent->fields.server->velocity);
		//clip = originalmove_clip;
		ent->fields.server->flags = originalmove_flags;
		ent->fields.server->groundentity = originalmove_groundentity;
	}

	SV_CheckVelocity(ent);
}

//============================================================================

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_Physics_Follow (prvm_edict_t *ent)
{
	vec3_t vf, vr, vu, angles, v;
	prvm_edict_t *e;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	// LordHavoc: implemented rotation on MOVETYPE_FOLLOW objects
	e = PRVM_PROG_TO_EDICT(ent->fields.server->aiment);
	if (e->fields.server->angles[0] == ent->fields.server->punchangle[0] && e->fields.server->angles[1] == ent->fields.server->punchangle[1] && e->fields.server->angles[2] == ent->fields.server->punchangle[2])
	{
		// quick case for no rotation
		VectorAdd(e->fields.server->origin, ent->fields.server->view_ofs, ent->fields.server->origin);
	}
	else
	{
		angles[0] = -ent->fields.server->punchangle[0];
		angles[1] =  ent->fields.server->punchangle[1];
		angles[2] =  ent->fields.server->punchangle[2];
		AngleVectors (angles, vf, vr, vu);
		v[0] = ent->fields.server->view_ofs[0] * vf[0] + ent->fields.server->view_ofs[1] * vr[0] + ent->fields.server->view_ofs[2] * vu[0];
		v[1] = ent->fields.server->view_ofs[0] * vf[1] + ent->fields.server->view_ofs[1] * vr[1] + ent->fields.server->view_ofs[2] * vu[1];
		v[2] = ent->fields.server->view_ofs[0] * vf[2] + ent->fields.server->view_ofs[1] * vr[2] + ent->fields.server->view_ofs[2] * vu[2];
		angles[0] = -e->fields.server->angles[0];
		angles[1] =  e->fields.server->angles[1];
		angles[2] =  e->fields.server->angles[2];
		AngleVectors (angles, vf, vr, vu);
		ent->fields.server->origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->fields.server->origin[0];
		ent->fields.server->origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->fields.server->origin[1];
		ent->fields.server->origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->fields.server->origin[2];
	}
	VectorAdd (e->fields.server->angles, ent->fields.server->v_angle, ent->fields.server->angles);
	SV_LinkEdict (ent, true);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition (prvm_edict_t *ent)
{
	int cont;
	cont = Mod_Q1BSP_NativeContentsFromSuperContents(NULL, SV_PointSuperContents(ent->fields.server->origin));
	if (!ent->fields.server->watertype)
	{
		// just spawned here
		ent->fields.server->watertype = cont;
		ent->fields.server->waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if (gamemode != GAME_NEXUIZ && ((ent->fields.server->watertype == CONTENTS_WATER || ent->fields.server->watertype == CONTENTS_SLIME) != (cont == CONTENTS_WATER || cont == CONTENTS_SLIME)))
		SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);

	if (cont <= CONTENTS_WATER)
	{
		ent->fields.server->watertype = cont;
		ent->fields.server->waterlevel = 1;
	}
	else
	{
		ent->fields.server->watertype = CONTENTS_EMPTY;
		ent->fields.server->waterlevel = 0;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (prvm_edict_t *ent)
{
	trace_t trace;
	vec3_t move;

// if onground, return without moving
	if ((int)ent->fields.server->flags & FL_ONGROUND)
	{
		// don't stick to ground if onground and moving upward
		if (ent->fields.server->velocity[2] >= (1.0 / 32.0))
			ent->fields.server->flags -= FL_ONGROUND;
		else
		{
			prvm_edict_t *ground = PRVM_PROG_TO_EDICT(ent->fields.server->groundentity);
			if (ground->fields.server->solid == SOLID_BSP || !sv_gameplayfix_noairborncorpse.integer)
				return;
			// if ent was supported by a brush model on previous frame,
			// and groundentity is now freed, set groundentity to 0 (floating)
			if (ent->priv.server->suspendedinairflag && ground->priv.server->free)
			{
				// leave it suspended in the air
				ent->fields.server->groundentity = 0;
				return;
			}
		}
	}
	ent->priv.server->suspendedinairflag = false;

	SV_CheckVelocity (ent);

// add gravity
	if (ent->fields.server->movetype == MOVETYPE_TOSS || ent->fields.server->movetype == MOVETYPE_BOUNCE)
		SV_AddGravity (ent);

// move angles
	VectorMA (ent->fields.server->angles, sv.frametime, ent->fields.server->avelocity, ent->fields.server->angles);

// move origin
	VectorScale (ent->fields.server->velocity, sv.frametime, move);
	trace = SV_PushEntity (ent, move);
	if (ent->priv.server->free)
		return;

	if (trace.fraction < 1)
	{
		if (ent->fields.server->movetype == MOVETYPE_BOUNCEMISSILE)
		{
			ClipVelocity (ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 2.0);
			ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
		}
		else if (ent->fields.server->movetype == MOVETYPE_BOUNCE)
		{
			float d;
			ClipVelocity (ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 1.5);
			// LordHavoc: fixed grenades not bouncing when fired down a slope
			if (sv_gameplayfix_grenadebouncedownslopes.integer)
			{
				d = DotProduct(trace.plane.normal, ent->fields.server->velocity);
				if (trace.plane.normal[2] > 0.7 && fabs(d) < 60)
				{
					ent->fields.server->flags = (int)ent->fields.server->flags | FL_ONGROUND;
					ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
					VectorClear (ent->fields.server->velocity);
					VectorClear (ent->fields.server->avelocity);
				}
				else
					ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
			}
			else
			{
				if (trace.plane.normal[2] > 0.7 && ent->fields.server->velocity[2] < 60)
				{
					ent->fields.server->flags = (int)ent->fields.server->flags | FL_ONGROUND;
					ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
					VectorClear (ent->fields.server->velocity);
					VectorClear (ent->fields.server->avelocity);
				}
				else
					ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
			}
		}
		else
		{
			ClipVelocity (ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 1.0);
			if (trace.plane.normal[2] > 0.7)
			{
				ent->fields.server->flags = (int)ent->fields.server->flags | FL_ONGROUND;
				ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				if (((prvm_edict_t *)trace.ent)->fields.server->solid == SOLID_BSP)
					ent->priv.server->suspendedinairflag = true;
				VectorClear (ent->fields.server->velocity);
				VectorClear (ent->fields.server->avelocity);
			}
			else
				ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
		}
	}

// check for in water
	SV_CheckWaterTransition (ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step (prvm_edict_t *ent)
{
	int flags = (int)ent->fields.server->flags;
	// don't fall at all if fly/swim
	if (!(flags & (FL_FLY | FL_SWIM)))
	{
		if (flags & FL_ONGROUND)
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift)
			prvm_edict_t *ground = PRVM_PROG_TO_EDICT(ent->fields.server->groundentity);
			if (ent->fields.server->velocity[2] >= (1.0 / 32.0) || (ground->fields.server->solid != SOLID_BSP && sv_gameplayfix_noairborncorpse.integer))
			{
				ent->fields.server->flags -= FL_ONGROUND;
				SV_AddGravity(ent);
				SV_CheckVelocity(ent);
				SV_FlyMove(ent, sv.frametime, NULL);
				SV_LinkEdict(ent, true);
			}
		}
		else
		{
			// freefall if not onground
			int hitsound = ent->fields.server->velocity[2] < sv_gravity.value * -0.1;

			SV_AddGravity(ent);
			SV_CheckVelocity(ent);
			SV_FlyMove(ent, sv.frametime, NULL);
			SV_LinkEdict(ent, true);

			// just hit ground
			if (hitsound && (int)ent->fields.server->flags & FL_ONGROUND && gamemode != GAME_NEXUIZ)
				SV_StartSound(ent, 0, "demon/dland2.wav", 255, 1);
		}
	}

// regular thinking
	SV_RunThink(ent);

	SV_CheckWaterTransition(ent);
}

//============================================================================

static void SV_Physics_Entity (prvm_edict_t *ent, qboolean runmove)
{
	switch ((int) ent->fields.server->movetype)
	{
	case MOVETYPE_PUSH:
	case MOVETYPE_FAKEPUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
		// LordHavoc: manually inlined the thinktime check here because MOVETYPE_NONE is used on so many objects
		if (ent->fields.server->nextthink > 0 && ent->fields.server->nextthink <= sv.time + sv.frametime)
			SV_RunThink (ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow (ent);
		break;
	case MOVETYPE_NOCLIP:
		if (SV_RunThink(ent))
		{
			SV_CheckWater(ent);
			VectorMA(ent->fields.server->origin, sv.frametime, ent->fields.server->velocity, ent->fields.server->origin);
			VectorMA(ent->fields.server->angles, sv.frametime, ent->fields.server->avelocity, ent->fields.server->angles);
		}
		SV_LinkEdict(ent, false);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_WALK:
		if (SV_RunThink (ent))
		{
			if (!SV_CheckWater (ent) && ! ((int)ent->fields.server->flags & FL_WATERJUMP) )
				SV_AddGravity (ent);
			SV_CheckStuck (ent);
			SV_WalkMove (ent);
			SV_LinkEdict (ent, true);
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCEMISSILE:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_FLY:
		// regular thinking
		if (SV_RunThink (ent) && runmove)
			SV_Physics_Toss (ent);
		break;
	default:
		Con_Printf ("SV_Physics: bad movetype %i\n", (int)ent->fields.server->movetype);
		break;
	}
}

void SV_Physics_ClientEntity (prvm_edict_t *ent)
{
	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);
	// LordHavoc: QuakeC replacement for SV_ClientThink (player movement)
	if (SV_PlayerPhysicsQC && sv_playerphysicsqc.integer)
	{
		prog->globals.server->time = sv.time;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
		PRVM_ExecuteProgram ((func_t)(SV_PlayerPhysicsQC - prog->functions), "QC function SV_PlayerPhysics is missing");
	}
	else
		SV_ClientThink ();
	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);
	// LordHavoc: a hack to ensure that the (rather silly) id1 quakec
	// player_run/player_stand1 does not horribly malfunction if the
	// velocity becomes a number that is both == 0 and != 0
	// (sounds to me like NaN but to be absolutely safe...)
	if (DotProduct(ent->fields.server->velocity, ent->fields.server->velocity) < 0.0001)
		VectorClear(ent->fields.server->velocity);
	// call standard client pre-think
	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPreThink, "QC function PlayerPreThink is missing");
	SV_CheckVelocity (ent);

	switch ((int) ent->fields.server->movetype)
	{
	case MOVETYPE_PUSH:
	case MOVETYPE_FAKEPUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
		// LordHavoc: manually inlined the thinktime check here because MOVETYPE_NONE is used on so many objects
		if (ent->fields.server->nextthink > 0 && ent->fields.server->nextthink <= sv.time + sv.frametime)
			SV_RunThink (ent);
		break;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow (ent);
		break;
	case MOVETYPE_NOCLIP:
		if (SV_RunThink(ent))
		{
			SV_CheckWater(ent);
			VectorMA(ent->fields.server->origin, sv.frametime, ent->fields.server->velocity, ent->fields.server->origin);
			VectorMA(ent->fields.server->angles, sv.frametime, ent->fields.server->avelocity, ent->fields.server->angles);
		}
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_WALK:
		if (SV_RunThink (ent))
		{
			if (!SV_CheckWater (ent) && ! ((int)ent->fields.server->flags & FL_WATERJUMP) )
				SV_AddGravity (ent);
			SV_CheckStuck (ent);
			SV_WalkMove (ent);
		}
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCEMISSILE:
	case MOVETYPE_FLYMISSILE:
		// regular thinking
		if (SV_RunThink (ent))
			SV_Physics_Toss (ent);
		break;
	case MOVETYPE_FLY:
		if (SV_RunThink (ent))
		{
			SV_CheckWater (ent);
			SV_WalkMove (ent);
		}
		break;
	default:
		Con_Printf ("SV_Physics_ClientEntity: bad movetype %i\n", (int)ent->fields.server->movetype);
		break;
	}

	SV_CheckVelocity (ent);

	// call standard player post-think
	SV_LinkEdict (ent, true);

	SV_CheckVelocity (ent);

	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPostThink, "QC function PlayerPostThink is missing");
}

/*
================
SV_Physics

================
*/
void SV_Physics (void)
{
	int i, newnum_edicts;
	prvm_edict_t *ent;
	unsigned char runmove[MAX_EDICTS];

// let the progs know that a new frame has started
	prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->time = sv.time;
	prog->globals.server->frametime = sv.frametime;
	PRVM_ExecuteProgram (prog->globals.server->StartFrame, "QC function StartFrame is missing");

	// don't run a move on newly spawned projectiles as it messes up movement
	// interpolation and rocket trails
	newnum_edicts = 0;
	for (i = 0, ent = prog->edicts;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
		if ((runmove[i] = !ent->priv.server->free))
			newnum_edicts = i + 1;
	prog->num_edicts = max(svs.maxclients + 1, newnum_edicts);

//
// treat each object in turn
//

	// if force_retouch, relink all the entities
	if (prog->globals.server->force_retouch > 0)
		for (i = 1, ent = PRVM_EDICT_NUM(i);i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
			if (!ent->priv.server->free)
				SV_LinkEdict (ent, true);	// force retouch even for stationary

	// run physics on the client entities
	for (i = 1, ent = PRVM_EDICT_NUM(i), host_client = svs.clients;i <= svs.maxclients;i++, ent = PRVM_NEXT_EDICT(ent), host_client++)
	{
		if (!ent->priv.server->free)
		{
			// don't do physics on disconnected clients, FrikBot relies on this
			if (!host_client->spawned)
				memset(&host_client->cmd, 0, sizeof(host_client->cmd));
			// don't run physics here if running asynchronously
			else if (!host_client->movesequence)
				SV_Physics_ClientEntity(ent);
		}
	}

	// run physics on all the non-client entities
	if (!sv_freezenonclients.integer)
		for (;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
			if (!ent->priv.server->free)
				SV_Physics_Entity(ent, runmove[i]);

	if (prog->globals.server->force_retouch > 0)
		prog->globals.server->force_retouch = max(0, prog->globals.server->force_retouch - 1);

	// LordHavoc: endframe support
	if (EndFrameQC)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		prog->globals.server->time = sv.time;
		PRVM_ExecuteProgram ((func_t)(EndFrameQC - prog->functions), "QC function EndFrame is missing");
	}

	if (!sv_freezenonclients.integer)
		sv.time += sv.frametime;
}


trace_t SV_Trace_Toss (prvm_edict_t *tossent, prvm_edict_t *ignore)
{
	int i;
	float gravity, savesolid;
	vec3_t move, end;
	prvm_edict_t tempent, *tent;
	entvars_t vars;
	prvm_eval_t *val;
	trace_t trace;

	// copy the vars over
	memcpy(&vars, tossent->fields.server, sizeof(entvars_t));
	// set up the temp entity to point to the copied vars
	tent = &tempent;
	tent->fields.server = &vars;

	savesolid = tossent->fields.server->solid;
	tossent->fields.server->solid = SOLID_NOT;

	// this has to fetch the field from the original edict, since our copy is truncated
	val = PRVM_GETEDICTFIELDVALUE(tossent, eval_gravity);
	if (val != NULL && val->_float != 0)
		gravity = val->_float;
	else
		gravity = 1.0;
	gravity *= sv_gravity.value * 0.05;

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		SV_CheckVelocity (tent);
		tent->fields.server->velocity[2] -= gravity;
		VectorMA (tent->fields.server->angles, 0.05, tent->fields.server->avelocity, tent->fields.server->angles);
		VectorScale (tent->fields.server->velocity, 0.05, move);
		VectorAdd (tent->fields.server->origin, move, end);
		trace = SV_Move (tent->fields.server->origin, tent->fields.server->mins, tent->fields.server->maxs, end, MOVE_NORMAL, tent);
		VectorCopy (trace.endpos, tent->fields.server->origin);

		if (trace.fraction < 1 && trace.ent && trace.ent != ignore)
			break;
	}
	tossent->fields.server->solid = savesolid;
	trace.fraction = 0; // not relevant
	return trace;
}

