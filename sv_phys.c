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
cvar_t sv_newflymove = {CVAR_NOTIFY, "sv_newflymove", "1"};
cvar_t sv_freezenonclients = {CVAR_NOTIFY, "sv_freezenonclients", "0"};

#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (edict_t *ent);

void SV_Phys_Init (void)
{
	Cvar_RegisterVariable(&sv_stepheight);
	Cvar_RegisterVariable(&sv_jumpstep);
	Cvar_RegisterVariable(&sv_wallfriction);
	Cvar_RegisterVariable(&sv_newflymove);
	Cvar_RegisterVariable(&sv_freezenonclients);
}

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts (void)
{
	int e;
	edict_t *check;

	// see if any solid entities are inside the final position
	check = NEXT_EDICT(sv.edicts);
	for (e = 1;e < sv.num_edicts;e++, check = NEXT_EDICT(check))
	{
		if (check->e->free)
			continue;
		if (check->v->movetype == MOVETYPE_PUSH
		 || check->v->movetype == MOVETYPE_NONE
		 || check->v->movetype == MOVETYPE_FOLLOW
		 || check->v->movetype == MOVETYPE_NOCLIP)
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
void SV_CheckVelocity (edict_t *ent)
{
	int i;
	float wishspeed;

//
// bound velocity
//
	for (i=0 ; i<3 ; i++)
	{
		if (IS_NAN(ent->v->velocity[i]))
		{
			Con_Printf("Got a NaN velocity on %s\n", PR_GetString(ent->v->classname));
			ent->v->velocity[i] = 0;
		}
		if (IS_NAN(ent->v->origin[i]))
		{
			Con_Printf("Got a NaN origin on %s\n", PR_GetString(ent->v->classname));
			ent->v->origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct(ent->v->velocity, ent->v->velocity);
	if (wishspeed > sv_maxvelocity.value * sv_maxvelocity.value)
	{
		wishspeed = sv_maxvelocity.value / sqrt(wishspeed);
		ent->v->velocity[0] *= wishspeed;
		ent->v->velocity[1] *= wishspeed;
		ent->v->velocity[2] *= wishspeed;
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
qboolean SV_RunThink (edict_t *ent)
{
	float thinktime;

	thinktime = ent->v->nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time)
		thinktime = sv.time;

	ent->v->nextthink = 0;
	pr_global_struct->time = thinktime;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	PR_ExecuteProgram (ent->v->think, "QC function self.think is missing");
	return !ent->e->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, edict_t *e2)
{
	int old_self, old_other;

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	pr_global_struct->time = sv.time;
	if (e1->v->touch && e1->v->solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		PR_ExecuteProgram (e1->v->touch, "QC function self.touch is missing");
	}

	if (e2->v->touch && e2->v->solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		PR_ExecuteProgram (e2->v->touch, "QC function self.touch is missing");
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
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
// LordHavoc: increased from 5 to 20
#define MAX_CLIP_PLANES 20
int SV_FlyMove (edict_t *ent, float time, float *stepnormal)
{
	int blocked, bumpcount;
	edict_t *hackongroundentity;
	int i, j, impact, numplanes;
	float d, time_left;
	vec3_t dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t trace;
	blocked = 0;
	VectorCopy(ent->v->velocity, original_velocity);
	VectorCopy(ent->v->velocity, primal_velocity);
	numplanes = 0;
	time_left = time;
	hackongroundentity = NULL;
	for (bumpcount = 0;bumpcount < 8;bumpcount++)
	{
		if (!ent->v->velocity[0] && !ent->v->velocity[1] && !ent->v->velocity[2])
			break;

		VectorMA(ent->v->origin, time_left, ent->v->velocity, end);
		trace = SV_Move(ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);
#if 0
		//if (trace.fraction < 0.002)
		{
#if 1
			vec3_t start;
			trace_t testtrace;
			VectorCopy(ent->v->origin, start);
			start[2] += 3;//0.03125;
			VectorMA(ent->v->origin, time_left, ent->v->velocity, end);
			end[2] += 3;//0.03125;
			testtrace = SV_Move(start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);
			if (trace.fraction < testtrace.fraction && !testtrace.startsolid && (testtrace.fraction == 1 || DotProduct(trace.plane.normal, ent->v->velocity) < DotProduct(testtrace.plane.normal, ent->v->velocity)))
			{
				Con_Printf("got further (new %f > old %f)\n", testtrace.fraction, trace.fraction);
				trace = testtrace;
			}
#endif
#if 0
			//j = -1;
			for (i = 0;i < numplanes;i++)
			{
				VectorCopy(ent->v->origin, start);
				VectorMA(ent->v->origin, time_left, ent->v->velocity, end);
				VectorMA(start, 3, planes[i], start);
				VectorMA(end, 3, planes[i], end);
				testtrace = SV_Move(start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);
				if (trace.fraction < testtrace.fraction)
				{
					trace = testtrace;
					VectorCopy(start, ent->v->origin);
					//j = i;
				}
			}
			//if (j >= 0)
			//	VectorAdd(ent->v->origin, planes[j], start);
#endif
		}
#endif

#if 0
		Con_Printf("entity %i bump %i: velocity %f %f %f trace %f", ent - sv.edicts, bumpcount, ent->v->velocity[0], ent->v->velocity[1], ent->v->velocity[2], trace.fraction);
		if (trace.fraction < 1)
			Con_Printf(" : %f %f %f", trace.plane.normal[0], trace.plane.normal[1], trace.plane.normal[2]);
		Con_Print("\n");
#endif

		/*
		if (trace.startsolid)
		{
			// LordHavoc: note: this code is what makes entities stick in place if embedded in another object (which can be the world)
			// entity is trapped in another solid
			VectorClear(ent->v->velocity);
			return 3;
		}
		*/

		// break if it moved the entire distance
		if (trace.fraction == 1)
		{
			VectorCopy(trace.endpos, ent->v->origin);
			break;
		}

		if (!trace.ent)
			Host_Error("SV_FlyMove: !trace.ent");

		if ((int) ent->v->flags & FL_ONGROUND)
		{
			if (ent->v->groundentity == EDICT_TO_PROG(trace.ent))
				impact = false;
			else
			{
				ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
				impact = true;
			}
		}
		else
			impact = true;

		if (trace.plane.normal[2])
		{
			if (trace.plane.normal[2] > 0.7)
			{
				// floor
				blocked |= 1;
				ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
				ent->v->groundentity = EDICT_TO_PROG(trace.ent);
			}
			else if (trace.fraction < 0.001)
				hackongroundentity = trace.ent;
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
			VectorCopy(trace.endpos, ent->v->origin);
			VectorCopy(ent->v->velocity, original_velocity);
			numplanes = 0;
		}

		// run the impact function
		if (impact)
		{
			SV_Impact(ent, trace.ent);

			// break if removed by the impact function
			if (ent->e->free)
				break;
		}

		time_left *= 1 - trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->v->velocity);
			blocked = 3;
			break;
		}

		/*
		for (i = 0;i < numplanes;i++)
			if (DotProduct(trace.plane.normal, planes[i]) > 0.99)
				break;
		if (i < numplanes)
		{
			VectorAdd(ent->v->velocity, trace.plane.normal, ent->v->velocity);
			continue;
		}
		*/

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		if (sv_newflymove.integer)
			ClipVelocity(ent->v->velocity, trace.plane.normal, ent->v->velocity, 1);
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
				VectorCopy(new_velocity, ent->v->velocity);
			}
			else
			{
				// go along the crease
				if (numplanes != 2)
				{
					VectorClear(ent->v->velocity);
					blocked = 7;
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				// LordHavoc: thanks to taniwha of QuakeForge for pointing out this fix for slowed falling in corners
				VectorNormalize(dir);
				d = DotProduct(dir, ent->v->velocity);
				VectorScale(dir, d, ent->v->velocity);
			}
		}

		// if original velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if (DotProduct(ent->v->velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->v->velocity);
			break;
		}
	}

	//Con_Printf("entity %i final: blocked %i velocity %f %f %f\n", ent - sv.edicts, blocked, ent->v->velocity[0], ent->v->velocity[1], ent->v->velocity[2]);

	/*
	// FIXME: this doesn't work well at all, find another solution
	// if player is ontop of a non-onground floor and made no progress,
	// set onground anyway (this tends to happen if standing in a wedge)
	if (bumpcount == 8 && hackongroundentity)
	{
		blocked |= 1;
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(hackongroundentity);
	}
	*/

	/*
	if ((blocked & 1) == 0 && bumpcount > 1)
	{
		// LordHavoc: fix the 'fall to your death in a wedge corner' glitch
		// flag ONGROUND if there's ground under it
		trace = SV_Move(ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);
	}
	*/
	return blocked;
}

int SV_SetOnGround (edict_t *ent)
{
	vec3_t end;
	trace_t trace;
	if ((int)ent->v->flags & FL_ONGROUND)
		return 1;
	VectorSet(end, ent->v->origin[0], ent->v->origin[1], ent->v->origin[2] - 1);
	trace = SV_Move(ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);
	if (trace.fraction < 1 && trace.plane.normal[2] >= 0.7)
	{
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(trace.ent);
		return 1;
	}
	return 0;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity (edict_t *ent)
{
	float ent_gravity;
	eval_t *val;

	val = GETEDICTFIELDVALUE(ent, eval_gravity);
	if (val!=0 && val->_float)
		ent_gravity = val->_float;
	else
		ent_gravity = 1.0;
	ent->v->velocity[2] -= ent_gravity * sv_gravity.value * sv.frametime;
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
trace_t SV_PushEntity (edict_t *ent, vec3_t push)
{
	int type;
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->v->origin, push, end);

	if (ent->v->movetype == MOVETYPE_FLYMISSILE)
		type = MOVE_MISSILE;
	else if (ent->v->solid == SOLID_TRIGGER || ent->v->solid == SOLID_NOT)
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else
		type = MOVE_NORMAL;

	trace = SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, end, type, ent);

	VectorCopy (trace.endpos, ent->v->origin);
	SV_LinkEdict (ent, true);

	if (trace.ent && (!((int)ent->v->flags & FL_ONGROUND) || ent->v->groundentity != EDICT_TO_PROG(trace.ent)))
		SV_Impact (ent, trace.ent);
	return trace;
}


/*
============
SV_PushMove

============
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
void SV_PushMove (edict_t *pusher, float movetime)
{
	int i, e, index;
	edict_t *check, *ed;
	float savesolid, movetime2, pushltime;
	vec3_t mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org, org2;
	int num_moved;
	int numcheckentities;
	static edict_t *checkentities[MAX_EDICTS];
	model_t *pushermodel;
	trace_t trace;

	if (!pusher->v->velocity[0] && !pusher->v->velocity[1] && !pusher->v->velocity[2] && !pusher->v->avelocity[0] && !pusher->v->avelocity[1] && !pusher->v->avelocity[2])
	{
		pusher->v->ltime += movetime;
		return;
	}

	switch ((int) pusher->v->solid)
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
		VectorMA (pusher->v->origin, movetime, pusher->v->velocity, pusher->v->origin);
		VectorMA (pusher->v->angles, movetime, pusher->v->avelocity, pusher->v->angles);
		pusher->v->angles[0] -= 360.0 * floor(pusher->v->angles[0] * (1.0 / 360.0));
		pusher->v->angles[1] -= 360.0 * floor(pusher->v->angles[1] * (1.0 / 360.0));
		pusher->v->angles[2] -= 360.0 * floor(pusher->v->angles[2] * (1.0 / 360.0));
		pusher->v->ltime += movetime;
		SV_LinkEdict (pusher, false);
		return;
	default:
		Con_Printf("SV_PushMove: unrecognized solid type %f\n", pusher->v->solid);
		return;
	}
	index = (int) pusher->v->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		Con_Printf("SV_PushMove: invalid modelindex %f\n", pusher->v->modelindex);
		return;
	}
	pushermodel = sv.models[index];

	movetime2 = movetime;
	VectorScale(pusher->v->velocity, movetime2, move1);
	VectorScale(pusher->v->avelocity, movetime2, moveangle);
	if (moveangle[0] || moveangle[2])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->v->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->v->origin[i] + 1;
			}
		}
	}
	else if (moveangle[1])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->yawmins[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->v->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->v->origin[i] + 1;
			}
		}
	}
	else
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->normalmins[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->v->origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->v->origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->v->origin[i] + 1;
			}
		}
	}

	VectorNegate (moveangle, a);
	AngleVectorsFLU (a, forward, left, up);

	VectorCopy (pusher->v->origin, pushorig);
	VectorCopy (pusher->v->angles, pushang);
	pushltime = pusher->v->ltime;

// move the pusher to it's final position

	VectorMA (pusher->v->origin, movetime, pusher->v->velocity, pusher->v->origin);
	VectorMA (pusher->v->angles, movetime, pusher->v->avelocity, pusher->v->angles);
	pusher->v->ltime += movetime;
	SV_LinkEdict (pusher, false);

	savesolid = pusher->v->solid;

// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = SV_EntitiesInBox(mins, maxs, MAX_EDICTS, checkentities);
	for (e = 1;e < numcheckentities;e++)
	{
		check = checkentities[e];
		if (check->v->movetype == MOVETYPE_PUSH
		 || check->v->movetype == MOVETYPE_NONE
		 || check->v->movetype == MOVETYPE_FOLLOW
		 || check->v->movetype == MOVETYPE_NOCLIP
		 || check->v->movetype == MOVETYPE_FAKEPUSH)
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if (!(((int)check->v->flags & FL_ONGROUND) && PROG_TO_EDICT(check->v->groundentity) == pusher))
			if (!SV_ClipMoveToEntity(pusher, check->v->origin, check->v->mins, check->v->maxs, check->v->origin).startsolid)
				continue;

		if (forward[0] != 1) // quick way to check if any rotation is used
		{
			VectorSubtract (check->v->origin, pusher->v->origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, left);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move);
			VectorAdd (move, move1, move);
		}
		else
			VectorCopy (move1, move);

		// remove the onground flag for non-players
		if (check->v->movetype != MOVETYPE_WALK)
			check->v->flags = (int)check->v->flags & ~FL_ONGROUND;

		VectorCopy (check->v->origin, check->e->moved_from);
		VectorCopy (check->v->angles, check->e->moved_fromangles);
		sv.moved_edicts[num_moved++] = check;

		// try moving the contacted entity
		pusher->v->solid = SOLID_NOT;
		trace = SV_PushEntity (check, move);
		// FIXME: turn players specially
		check->v->angles[1] += trace.fraction * moveangle[1];
		pusher->v->solid = savesolid; // was SOLID_BSP

		// if it is still inside the pusher, block
		if (SV_ClipMoveToEntity(pusher, check->v->origin, check->v->mins, check->v->maxs, check->v->origin).startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			pusher->v->solid = SOLID_NOT;
			VectorScale(move, 0.1, move);
			SV_PushEntity (check, move);
			pusher->v->solid = savesolid;
			if (SV_ClipMoveToEntity(pusher, check->v->origin, check->v->mins, check->v->maxs, check->v->origin).startsolid)
			{
				// still inside pusher, so it's really blocked

				// fail the move
				if (check->v->mins[0] == check->v->maxs[0])
					continue;
				if (check->v->solid == SOLID_NOT || check->v->solid == SOLID_TRIGGER)
				{
					// corpse
					check->v->mins[0] = check->v->mins[1] = 0;
					VectorCopy (check->v->mins, check->v->maxs);
					continue;
				}

				VectorCopy (pushorig, pusher->v->origin);
				VectorCopy (pushang, pusher->v->angles);
				pusher->v->ltime = pushltime;
				SV_LinkEdict (pusher, false);

				// move back any entities we already moved
				for (i = 0;i < num_moved;i++)
				{
					ed = sv.moved_edicts[i];
					VectorCopy (ed->e->moved_from, ed->v->origin);
					VectorCopy (ed->e->moved_fromangles, ed->v->angles);
					SV_LinkEdict (ed, false);
				}

				// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
				if (pusher->v->blocked)
				{
					pr_global_struct->self = EDICT_TO_PROG(pusher);
					pr_global_struct->other = EDICT_TO_PROG(check);
					PR_ExecuteProgram (pusher->v->blocked, "QC function self.blocked is missing");
				}
				break;
			}
		}
	}
	pusher->v->angles[0] -= 360.0 * floor(pusher->v->angles[0] * (1.0 / 360.0));
	pusher->v->angles[1] -= 360.0 * floor(pusher->v->angles[1] * (1.0 / 360.0));
	pusher->v->angles[2] -= 360.0 * floor(pusher->v->angles[2] * (1.0 / 360.0));
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	float thinktime, oldltime, movetime;

	oldltime = ent->v->ltime;

	thinktime = ent->v->nextthink;
	if (thinktime < ent->v->ltime + sv.frametime)
	{
		movetime = thinktime - ent->v->ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = sv.frametime;

	if (movetime)
		// advances ent->v->ltime if not blocked
		SV_PushMove (ent, movetime);

	if (thinktime > oldltime && thinktime <= ent->v->ltime)
	{
		ent->v->nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_ExecuteProgram (ent->v->think, "QC function self.think is missing");
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
void SV_CheckStuck (edict_t *ent)
{
	int i, j, z;
	vec3_t org;

	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy (ent->v->origin, ent->v->oldorigin);
		return;
	}

	VectorCopy (ent->v->origin, org);
	VectorCopy (ent->v->oldorigin, ent->v->origin);
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
				ent->v->origin[0] = org[0] + i;
				ent->v->origin[1] = org[1] + j;
				ent->v->origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_DPrint("Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}

	VectorCopy (org, ent->v->origin);
	Con_DPrint("player is stuck.\n");
}


/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater (edict_t *ent)
{
	int cont;
	vec3_t point;

	point[0] = ent->v->origin[0];
	point[1] = ent->v->origin[1];
	point[2] = ent->v->origin[2] + ent->v->mins[2] + 1;

	ent->v->waterlevel = 0;
	ent->v->watertype = CONTENTS_EMPTY;
	cont = SV_PointSuperContents(point);
	if (cont & (SUPERCONTENTS_LIQUIDSMASK))
	{
		ent->v->watertype = Mod_Q1BSP_NativeContentsFromSuperContents(NULL, cont);
		ent->v->waterlevel = 1;
		point[2] = ent->v->origin[2] + (ent->v->mins[2] + ent->v->maxs[2])*0.5;
		if (SV_PointSuperContents(point) & (SUPERCONTENTS_LIQUIDSMASK))
		{
			ent->v->waterlevel = 2;
			point[2] = ent->v->origin[2] + ent->v->view_ofs[2];
			if (SV_PointSuperContents(point) & (SUPERCONTENTS_LIQUIDSMASK))
				ent->v->waterlevel = 3;
		}
	}

	return ent->v->waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction (edict_t *ent, float *stepnormal)
{
	float d, i;
	vec3_t forward, into, side;

	AngleVectors (ent->v->v_angle, forward, NULL, NULL);
	if ((d = DotProduct (stepnormal, forward) + 0.5) < 0)
	{
		// cut the tangential velocity
		i = DotProduct (stepnormal, ent->v->velocity);
		VectorScale (stepnormal, i, into);
		VectorSubtract (ent->v->velocity, into, side);
		ent->v->velocity[0] = side[0] * (1 + d);
		ent->v->velocity[1] = side[1] * (1 + d);
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
int SV_TryUnstick (edict_t *ent, vec3_t oldvel)
{
	int i, clip;
	vec3_t oldorg, dir;

	VectorCopy (ent->v->origin, oldorg);
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
		ent->v->velocity[0] = oldvel[0];
		ent->v->velocity[1] = oldvel[1];
		ent->v->velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, NULL);

		if (fabs(oldorg[1] - ent->v->origin[1]) > 4
		 || fabs(oldorg[0] - ent->v->origin[0]) > 4)
		{
			Con_DPrint("TryUnstick - success.\n");
			return clip;
		}

		// go back to the original pos and try again
		VectorCopy (oldorg, ent->v->origin);
	}

	// still not moving
	VectorClear (ent->v->velocity);
	Con_DPrint("TryUnstick - failure.\n");
	return 7;
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove (edict_t *ent)
{
	int clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity;
	vec3_t upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;
	trace_t downtrace;

	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->v->flags & FL_ONGROUND;
	ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;

	VectorCopy (ent->v->origin, start_origin);
	VectorCopy (ent->v->velocity, start_velocity);

	clip = SV_FlyMove (ent, sv.frametime, NULL);

	SV_SetOnGround (ent);
	SV_CheckVelocity(ent);

	VectorCopy(ent->v->origin, originalmove_origin);
	VectorCopy(ent->v->velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->v->flags;
	originalmove_groundentity = ent->v->groundentity;

	if ((int)ent->v->flags & FL_WATERJUMP)
		return;

	if (sv_nostep.integer)
		return;

	// if move didn't block on a step, return
	if (clip & 2)
	{
		// if move was not trying to move into the step, return
		if (fabs(start_velocity[0]) < 0.03125 && fabs(start_velocity[1]) < 0.03125)
			return;

		if (ent->v->movetype != MOVETYPE_FLY)
		{
			// return if gibbed by a trigger
			if (ent->v->movetype != MOVETYPE_WALK)
				return;

			// only step up while jumping if that is enabled
			if (!(sv_jumpstep.integer && sv_gameplayfix_stepwhilejumping.integer))
				if (!oldonground && ent->v->waterlevel == 0)
					return;
		}

		// try moving up and forward to go up a step
		// back to start pos
		VectorCopy (start_origin, ent->v->origin);
		VectorCopy (start_velocity, ent->v->velocity);

		// move up
		VectorClear (upmove);
		upmove[2] = sv_stepheight.value;
		// FIXME: don't link?
		SV_PushEntity(ent, upmove);

		// move forward
		ent->v->velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, stepnormal);
		ent->v->velocity[2] += start_velocity[2];

		SV_CheckVelocity(ent);

		// check for stuckness, possibly due to the limited precision of floats
		// in the clipping hulls
		if (clip
		 && fabs(originalmove_origin[1] - ent->v->origin[1]) < 0.03125
		 && fabs(originalmove_origin[0] - ent->v->origin[0]) < 0.03125)
		{
			//Con_Printf("wall\n");
			// stepping up didn't make any progress, revert to original move
			VectorCopy(originalmove_origin, ent->v->origin);
			VectorCopy(originalmove_velocity, ent->v->velocity);
			//clip = originalmove_clip;
			ent->v->flags = originalmove_flags;
			ent->v->groundentity = originalmove_groundentity;
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
	else if (!(sv_gameplayfix_stepdown.integer && ent->v->waterlevel < 2 && start_velocity[2] < (1.0 / 32.0) && oldonground && !((int)ent->v->flags & FL_ONGROUND)))
		return;

	// move down
	VectorClear (downmove);
	downmove[2] = -sv_stepheight.value + start_velocity[2]*sv.frametime;
	// FIXME: don't link?
	downtrace = SV_PushEntity (ent, downmove);

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (ent->v->solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			ent->v->flags =	(int)ent->v->flags | FL_ONGROUND;
			ent->v->groundentity = EDICT_TO_PROG(downtrace.ent);
		}
	}
	else
	{
		//Con_Printf("slope\n");
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(originalmove_origin, ent->v->origin);
		VectorCopy(originalmove_velocity, ent->v->velocity);
		//clip = originalmove_clip;
		ent->v->flags = originalmove_flags;
		ent->v->groundentity = originalmove_groundentity;
	}

	SV_SetOnGround (ent);
	SV_CheckVelocity(ent);
}

//============================================================================

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_Physics_Follow (edict_t *ent)
{
	vec3_t vf, vr, vu, angles, v;
	edict_t *e;

	// regular thinking
	if (!SV_RunThink (ent))
		return;

	// LordHavoc: implemented rotation on MOVETYPE_FOLLOW objects
	e = PROG_TO_EDICT(ent->v->aiment);
	if (e->v->angles[0] == ent->v->punchangle[0] && e->v->angles[1] == ent->v->punchangle[1] && e->v->angles[2] == ent->v->punchangle[2])
	{
		// quick case for no rotation
		VectorAdd(e->v->origin, ent->v->view_ofs, ent->v->origin);
	}
	else
	{
		angles[0] = -ent->v->punchangle[0];
		angles[1] =  ent->v->punchangle[1];
		angles[2] =  ent->v->punchangle[2];
		AngleVectors (angles, vf, vr, vu);
		v[0] = ent->v->view_ofs[0] * vf[0] + ent->v->view_ofs[1] * vr[0] + ent->v->view_ofs[2] * vu[0];
		v[1] = ent->v->view_ofs[0] * vf[1] + ent->v->view_ofs[1] * vr[1] + ent->v->view_ofs[2] * vu[1];
		v[2] = ent->v->view_ofs[0] * vf[2] + ent->v->view_ofs[1] * vr[2] + ent->v->view_ofs[2] * vu[2];
		angles[0] = -e->v->angles[0];
		angles[1] =  e->v->angles[1];
		angles[2] =  e->v->angles[2];
		AngleVectors (angles, vf, vr, vu);
		ent->v->origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->v->origin[0];
		ent->v->origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->v->origin[1];
		ent->v->origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->v->origin[2];
	}
	VectorAdd (e->v->angles, ent->v->v_angle, ent->v->angles);
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
void SV_CheckWaterTransition (edict_t *ent)
{
	int cont;
	cont = SV_PointQ1Contents(ent->v->origin);
	if (!ent->v->watertype)
	{
		// just spawned here
		ent->v->watertype = cont;
		ent->v->waterlevel = 1;
		return;
	}

	// check if the entity crossed into or out of water
	if ((ent->v->watertype == CONTENTS_WATER || ent->v->watertype == CONTENTS_SLIME) != (cont == CONTENTS_WATER || cont == CONTENTS_SLIME))
		SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);

	if (cont <= CONTENTS_WATER)
	{
		ent->v->watertype = cont;
		ent->v->waterlevel = 1;
	}
	else
	{
		ent->v->watertype = CONTENTS_EMPTY;
		ent->v->waterlevel = 0;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (edict_t *ent)
{
	trace_t trace;
	vec3_t move;
	edict_t *groundentity;

	// don't stick to ground if onground and moving upward
	if (ent->v->velocity[2] >= (1.0 / 32.0) && ((int)ent->v->flags & FL_ONGROUND))
		ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;

// if onground, return without moving
	if ((int)ent->v->flags & FL_ONGROUND)
	{
		if (!sv_gameplayfix_noairborncorpse.integer)
			return;
		if (ent->v->groundentity == 0)
			return;
		// if ent was supported by a brush model on previous frame,
		// and groundentity is now freed, set groundentity to 0 (floating)
		groundentity = PROG_TO_EDICT(ent->v->groundentity);
		if (groundentity->v->solid == SOLID_BSP)
		{
			ent->e->suspendedinairflag = true;
			return;
		}
		else if (ent->e->suspendedinairflag && groundentity->e->free)
		{
			// leave it suspended in the air
			ent->v->groundentity = 0;
			ent->e->suspendedinairflag = false;
			return;
		}
	}
	ent->e->suspendedinairflag = false;

	SV_CheckVelocity (ent);

// add gravity
	if (ent->v->movetype == MOVETYPE_TOSS || ent->v->movetype == MOVETYPE_BOUNCE)
		SV_AddGravity (ent);

// move angles
	VectorMA (ent->v->angles, sv.frametime, ent->v->avelocity, ent->v->angles);

// move origin
	VectorScale (ent->v->velocity, sv.frametime, move);
	trace = SV_PushEntity (ent, move);
	if (ent->e->free)
		return;

	if (trace.fraction < 1)
	{
		if (ent->v->movetype == MOVETYPE_BOUNCEMISSILE)
		{
			ClipVelocity (ent->v->velocity, trace.plane.normal, ent->v->velocity, 2.0);
			ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
		}
		else if (ent->v->movetype == MOVETYPE_BOUNCE)
		{
			float d;
			ClipVelocity (ent->v->velocity, trace.plane.normal, ent->v->velocity, 1.5);
			// LordHavoc: fixed grenades not bouncing when fired down a slope
			if (sv_gameplayfix_grenadebouncedownslopes.integer)
			{
				d = DotProduct(trace.plane.normal, ent->v->velocity);
				if (trace.plane.normal[2] > 0.7 && fabs(d) < 60)
				{
					ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
					ent->v->groundentity = EDICT_TO_PROG(trace.ent);
					VectorClear (ent->v->velocity);
					VectorClear (ent->v->avelocity);
				}
				else
					ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
			}
			else
			{
				if (trace.plane.normal[2] > 0.7 && ent->v->velocity[2] < 60)
				{
					ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
					ent->v->groundentity = EDICT_TO_PROG(trace.ent);
					VectorClear (ent->v->velocity);
					VectorClear (ent->v->avelocity);
				}
				else
					ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
			}
		}
		else
		{
			ClipVelocity (ent->v->velocity, trace.plane.normal, ent->v->velocity, 1.0);
			if (trace.plane.normal[2] > 0.7)
			{
				ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
				ent->v->groundentity = EDICT_TO_PROG(trace.ent);
				VectorClear (ent->v->velocity);
				VectorClear (ent->v->avelocity);
			}
			else
				ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
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
void SV_Physics_Step (edict_t *ent)
{
	// don't stick to ground if onground and moving upward
	if (ent->v->velocity[2] >= (1.0 / 32.0) && ((int)ent->v->flags & FL_ONGROUND))
		ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;

	// freefall if not onground/fly/swim
	if (!((int)ent->v->flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		int hitsound = ent->v->velocity[2] < sv_gravity.value * -0.1;

		SV_AddGravity(ent);
		SV_CheckVelocity(ent);
		SV_FlyMove(ent, sv.frametime, NULL);
		SV_LinkEdict(ent, true);

		// just hit ground
		if (hitsound && (int)ent->v->flags & FL_ONGROUND && gamemode != GAME_NEXUIZ)
			SV_StartSound(ent, 0, "demon/dland2.wav", 255, 1);
	}

// regular thinking
	SV_RunThink(ent);

	SV_CheckWaterTransition(ent);
}

//============================================================================

/*
================
SV_Physics

================
*/
void SV_Physics (void)
{
	int i, newnum_edicts;
	edict_t *ent;
	qbyte runmove[MAX_EDICTS];

// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->time = sv.time;
	PR_ExecuteProgram (pr_global_struct->StartFrame, "QC function StartFrame is missing");

	newnum_edicts = 0;
	for (i = 0, ent = sv.edicts;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
		if ((runmove[i] = !ent->e->free))
			newnum_edicts = i + 1;
	sv.num_edicts = max(svs.maxclients + 1, newnum_edicts);

//
// treat each object in turn
//

	for (i = 0, ent = sv.edicts;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		if (ent->e->free)
			continue;

		if (pr_global_struct->force_retouch)
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i >= 1 && i <= svs.maxclients)
		{
			// don't do physics on disconnected clients, FrikBot relies on this
			if (!svs.clients[i-1].spawned)
				continue;
			// connected slot
			// call standard client pre-think
			SV_CheckVelocity (ent);
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			PR_ExecuteProgram (pr_global_struct->PlayerPreThink, "QC function PlayerPreThink is missing");
			SV_CheckVelocity (ent);
		}
		else if (sv_freezenonclients.integer)
			continue;

		// LordHavoc: merged client and normal entity physics
		switch ((int) ent->v->movetype)
		{
		case MOVETYPE_PUSH:
		case MOVETYPE_FAKEPUSH:
			SV_Physics_Pusher (ent);
			break;
		case MOVETYPE_NONE:
			// LordHavoc: manually inlined the thinktime check here because MOVETYPE_NONE is used on so many objects
			if (ent->v->nextthink > 0 && ent->v->nextthink <= sv.time + sv.frametime)
				SV_RunThink (ent);
			break;
		case MOVETYPE_FOLLOW:
			SV_Physics_Follow (ent);
			break;
		case MOVETYPE_NOCLIP:
			if (SV_RunThink(ent))
			{
				SV_CheckWater(ent);
				VectorMA(ent->v->origin, sv.frametime, ent->v->velocity, ent->v->origin);
				VectorMA(ent->v->angles, sv.frametime, ent->v->avelocity, ent->v->angles);
			}
			// relink normal entities here, players always get relinked so don't relink twice
			if (!(i > 0 && i <= svs.maxclients))
				SV_LinkEdict(ent, false);
			break;
		case MOVETYPE_STEP:
			SV_Physics_Step (ent);
			break;
		case MOVETYPE_WALK:
			if (SV_RunThink (ent))
			{
				if (!SV_CheckWater (ent) && ! ((int)ent->v->flags & FL_WATERJUMP) )
					SV_AddGravity (ent);
				SV_CheckStuck (ent);
				SV_WalkMove (ent);
				// relink normal entities here, players always get relinked so don't relink twice
				if (!(i > 0 && i <= svs.maxclients))
					SV_LinkEdict (ent, true);
			}
			break;
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_BOUNCEMISSILE:
		case MOVETYPE_FLYMISSILE:
			// regular thinking
			if (SV_RunThink (ent) && runmove[i])
				SV_Physics_Toss (ent);
			break;
		case MOVETYPE_FLY:
			if (SV_RunThink (ent) && runmove[i])
			{
				if (i > 0 && i <= svs.maxclients)
				{
					SV_CheckWater (ent);
					SV_WalkMove (ent);
				}
				else
					SV_Physics_Toss (ent);
			}
			break;
		default:
			Host_Error ("SV_Physics: bad movetype %i", (int)ent->v->movetype);
			break;
		}

		if (i >= 1 && i <= svs.maxclients)
		{
			SV_CheckVelocity (ent);

			// call standard player post-think
			SV_LinkEdict (ent, true);

			SV_CheckVelocity (ent);

			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			PR_ExecuteProgram (pr_global_struct->PlayerPostThink, "QC function PlayerPostThink is missing");
		}
	}

	if (pr_global_struct->force_retouch > 0)
		pr_global_struct->force_retouch = max(0, pr_global_struct->force_retouch - 1);

	// LordHavoc: endframe support
	if (EndFrameQC)
	{
		pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram ((func_t)(EndFrameQC - pr_functions), "QC function EndFrame is missing");
	}

	if (!sv_freezenonclients.integer)
		sv.time += sv.frametime;
}


trace_t SV_Trace_Toss (edict_t *tossent, edict_t *ignore)
{
	int i;
	float gravity, savesolid;
	vec3_t move, end;
	edict_t tempent, *tent;
	entvars_t vars;
	eval_t *val;
	trace_t trace;

	// copy the vars over
	memcpy(&vars, tossent->v, sizeof(entvars_t));
	// set up the temp entity to point to the copied vars
	tent = &tempent;
	tent->v = &vars;

	savesolid = tossent->v->solid;
	tossent->v->solid = SOLID_NOT;

	// this has to fetch the field from the original edict, since our copy is truncated
	val = GETEDICTFIELDVALUE(tossent, eval_gravity);
	if (val != NULL && val->_float != 0)
		gravity = val->_float;
	else
		gravity = 1.0;
	gravity *= sv_gravity.value * 0.05;

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		SV_CheckVelocity (tent);
		tent->v->velocity[2] -= gravity;
		VectorMA (tent->v->angles, 0.05, tent->v->avelocity, tent->v->angles);
		VectorScale (tent->v->velocity, 0.05, move);
		VectorAdd (tent->v->origin, move, end);
		trace = SV_Move (tent->v->origin, tent->v->mins, tent->v->maxs, end, MOVE_NORMAL, tent);
		VectorCopy (trace.endpos, tent->v->origin);

		if (trace.fraction < 1 && trace.ent && trace.ent != ignore)
			break;
	}
	tossent->v->solid = savesolid;
	trace.fraction = 0; // not relevant
	return trace;
}

