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

#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (edict_t *ent);

void SV_Phys_Init (void)
{
	Cvar_RegisterVariable(&sv_stepheight);
	Cvar_RegisterVariable(&sv_jumpstep);
	Cvar_RegisterVariable(&sv_wallfriction);
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
		if (check->free)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
		 || check->v.movetype == MOVETYPE_NONE
		 || check->v.movetype == MOVETYPE_FOLLOW
		 || check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		if (SV_TestEntityPosition (check))
			Con_Printf ("entity in invalid position\n");
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
		if (IS_NAN(ent->v.velocity[i]))
		{
			Con_Printf ("Got a NaN velocity on %s\n", pr_strings + ent->v.classname);
			ent->v.velocity[i] = 0;
		}
		if (IS_NAN(ent->v.origin[i]))
		{
			Con_Printf ("Got a NaN origin on %s\n", pr_strings + ent->v.classname);
			ent->v.origin[i] = 0;
		}
	}

	// LordHavoc: max velocity fix, inspired by Maddes's source fixes, but this is faster
	wishspeed = DotProduct(ent->v.velocity, ent->v.velocity);
	if (wishspeed > sv_maxvelocity.value * sv_maxvelocity.value)
	{
		wishspeed = sv_maxvelocity.value / sqrt(wishspeed);
		ent->v.velocity[0] *= wishspeed;
		ent->v.velocity[1] *= wishspeed;
		ent->v.velocity[2] *= wishspeed;
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

	thinktime = ent->v.nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time)
		thinktime = sv.time;

	ent->v.nextthink = 0;
	pr_global_struct->time = thinktime;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	PR_ExecuteProgram (ent->v.think, "NULL think function");
	return !ent->free;
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
	if (e1->v.touch && e1->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		PR_ExecuteProgram (e1->v.touch, "");
	}

	if (e2->v.touch && e2->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		PR_ExecuteProgram (e2->v.touch, "");
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
int ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int i, blocked;
	float backoff, change;

	blocked = 0;
	// flag if it's a floor
	if (normal[2] > 0)
		blocked |= 1;
	// flag if it's a step
	if (!normal[2])
		blocked |= 2;

	backoff = DotProduct (in, normal) * overbounce;

	for (i = 0;i < 3;i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}


/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
// LordHavoc: increased from 5 to 20
#define MAX_CLIP_PLANES 20
int SV_FlyMove (edict_t *ent, float time, trace_t *steptrace)
{
	int i, j, blocked, impact, numplanes, bumpcount, numbumps;
	float d, time_left;
	vec3_t dir, end, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
	trace_t trace;

	numbumps = 4;

	blocked = 0;
	VectorCopy (ent->v.velocity, original_velocity);
	VectorCopy (ent->v.velocity, primal_velocity);
	numplanes = 0;

	time_left = time;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		if (!ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2])
			break;

		for (i=0 ; i<3 ; i++)
			end[i] = ent->v.origin[i] + time_left * ent->v.velocity[i];

		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

		if (trace.allsolid)
		{
			// LordHavoc: note: this code is what makes entities stick in place if embedded in another object (which can be the world)
			// entity is trapped in another solid
			VectorClear(ent->v.velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{
			// actually covered some distance
			VectorCopy (trace.endpos, ent->v.origin);
			VectorCopy (ent->v.velocity, original_velocity);
			numplanes = 0;
		}

		// break if it moved the entire distance
		if (trace.fraction == 1)
			 break;

		if (!trace.ent)
			Host_Error ("SV_FlyMove: !trace.ent");

		if ((int) ent->v.flags & FL_ONGROUND)
		{
			if (ent->v.groundentity == EDICT_TO_PROG(trace.ent))
				impact = false;
			else
			{
				ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
				impact = true;
			}
		}
		else
			impact = true;

		if (trace.plane.normal[2] > 0.7)
		{
			// floor
			blocked |= 1;
			ent->v.flags =	(int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		}
		if (!trace.plane.normal[2])
		{
			// step
			blocked |= 2;
			// save the trace for player extrafriction
			if (steptrace)
				*steptrace = trace;
		}

		// run the impact function
		if (impact)
		{
			SV_Impact (ent, trace.ent);

			// break if removed by the impact function
			if (ent->free)
				break;
		}


		time_left -= time_left * trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorClear(ent->v.velocity);
			return 3;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i=0 ; i<numplanes ; i++)
		{
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);
			for (j=0 ; j<numplanes ; j++)
				if (j != i)
				{
					// not ok
					if (DotProduct (new_velocity, planes[j]) < 0)
						break;
				}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{
			// go along this plane
			VectorCopy (new_velocity, ent->v.velocity);
		}
		else
		{
			// go along the crease
			if (numplanes != 2)
			{
				VectorClear(ent->v.velocity);
				return 7;
			}
			CrossProduct (planes[0], planes[1], dir);
			// LordHavoc: thanks to taniwha of QuakeForge for pointing out this fix for slowed falling in corners
			VectorNormalize(dir);
			d = DotProduct (dir, ent->v.velocity);
			VectorScale (dir, d, ent->v.velocity);
		}

		// if original velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if (DotProduct (ent->v.velocity, primal_velocity) <= 0)
		{
			VectorClear(ent->v.velocity);
			return blocked;
		}
	}

	return blocked;
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
	ent->v.velocity[2] -= ent_gravity * sv_gravity.value * sv.frametime;
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
trace_t SV_PushEntity (edict_t *ent, vec3_t push, vec3_t pushangles)
{
	trace_t trace;
	vec3_t end;

	VectorAdd (ent->v.origin, push, end);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		// only clip against bmodels
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
	else
		trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	VectorCopy (trace.endpos, ent->v.origin);
	// FIXME: turn players specially
	ent->v.angles[1] += trace.fraction * pushangles[1];
	SV_LinkEdict (ent, true);

	if (trace.ent && (!((int)ent->v.flags & FL_ONGROUND) || ent->v.groundentity != EDICT_TO_PROG(trace.ent)))
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
	int			i, e, index;
	edict_t		*check;
	float		savesolid, movetime2, pushltime;
	vec3_t		mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org, org2;
	int			num_moved;
	edict_t		*moved_edict[MAX_EDICTS];
	vec3_t		moved_from[MAX_EDICTS], moved_fromangles[MAX_EDICTS];
	model_t		*pushermodel;
	trace_t		trace;

	switch ((int) pusher->v.solid)
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
		VectorMA (pusher->v.origin, movetime, pusher->v.velocity, pusher->v.origin);
		VectorMA (pusher->v.angles, movetime, pusher->v.avelocity, pusher->v.angles);
		pusher->v.ltime += movetime;
		SV_LinkEdict (pusher, false);
		return;
	default:
		Host_Error("SV_PushMove: unrecognized solid type %f\n", pusher->v.solid);
	}
	if (!pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2] && !pusher->v.avelocity[0] && !pusher->v.avelocity[1] && !pusher->v.avelocity[2])
	{
		pusher->v.ltime += movetime;
		return;
	}
	index = (int) pusher->v.modelindex;
	if (index < 1 || index >= MAX_MODELS)
		Host_Error("SV_PushMove: invalid modelindex %f\n", pusher->v.modelindex);
	pushermodel = sv.models[index];

	movetime2 = movetime;
	VectorScale(pusher->v.velocity, movetime2, move1);
	VectorScale(pusher->v.avelocity, movetime2, moveangle);
	if (moveangle[0] || moveangle[2])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->rotatedmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->rotatedmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->rotatedmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}
	else if (moveangle[1])
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->yawmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->yawmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->yawmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}
	else
	{
		for (i = 0;i < 3;i++)
		{
			if (move1[i] > 0)
			{
				mins[i] = pushermodel->normalmins[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + move1[i] + pusher->v.origin[i] + 1;
			}
			else
			{
				mins[i] = pushermodel->normalmins[i] + move1[i] + pusher->v.origin[i] - 1;
				maxs[i] = pushermodel->normalmaxs[i] + pusher->v.origin[i] + 1;
			}
		}
	}

	VectorNegate (moveangle, a);
	AngleVectorsFLU (a, forward, left, up);

	VectorCopy (pusher->v.origin, pushorig);
	VectorCopy (pusher->v.angles, pushang);
	pushltime = pusher->v.ltime;

// move the pusher to it's final position

	VectorMA (pusher->v.origin, movetime, pusher->v.velocity, pusher->v.origin);
	VectorMA (pusher->v.angles, movetime, pusher->v.avelocity, pusher->v.angles);
	pusher->v.ltime += movetime;
	SV_LinkEdict (pusher, false);

	savesolid = pusher->v.solid;

// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e = 1;e < sv.num_edicts;e++, check = NEXT_EDICT(check))
	{
		if (check->free)
			continue;
		if (check->v.movetype == MOVETYPE_PUSH
		 || check->v.movetype == MOVETYPE_NONE
		 || check->v.movetype == MOVETYPE_FOLLOW
		 || check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		// if the entity is standing on the pusher, it will definitely be moved
		if (!(((int)check->v.flags & FL_ONGROUND) && PROG_TO_EDICT(check->v.groundentity) == pusher))
		{
			if (check->v.absmin[0] >= maxs[0]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[2] <= mins[2])
				continue;

			trace = SV_ClipMoveToEntity (pusher, check->v.origin, check->v.mins, check->v.maxs, check->v.origin);
			if (!trace.startsolid)
				continue;
		}

		if (forward[0] < 0.999f) // quick way to check if any rotation is used
		{
			VectorSubtract (check->v.origin, pusher->v.origin, org);
			org2[0] = DotProduct (org, forward);
			org2[1] = DotProduct (org, left);
			org2[2] = DotProduct (org, up);
			VectorSubtract (org2, org, move);
			VectorAdd (move, move1, move);
		}
		else
			VectorCopy (move1, move);

		// remove the onground flag for non-players
		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (int)check->v.flags & ~FL_ONGROUND;

		VectorCopy (check->v.origin, moved_from[num_moved]);
		VectorCopy (check->v.angles, moved_fromangles[num_moved]);
		moved_edict[num_moved++] = check;

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		trace = SV_PushEntity (check, move, moveangle);
		pusher->v.solid = savesolid; // was SOLID_BSP

		// if it is still inside the pusher, block
		if (SV_TestEntityPosition (check))
		{
			// fail the move
			if (check->v.mins[0] == check->v.maxs[0])
				continue;
			if (check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER)
			{
				// corpse
				check->v.mins[0] = check->v.mins[1] = 0;
				VectorCopy (check->v.mins, check->v.maxs);
				continue;
			}

			VectorCopy (pushorig, pusher->v.origin);
			VectorCopy (pushang, pusher->v.angles);
			pusher->v.ltime = pushltime;
			SV_LinkEdict (pusher, false);

			// move back any entities we already moved
			for (i=0 ; i<num_moved ; i++)
			{
				VectorCopy (moved_from[i], moved_edict[i]->v.origin);
				VectorCopy (moved_fromangles[i], moved_edict[i]->v.angles);
				SV_LinkEdict (moved_edict[i], false);
			}

			// if the pusher has a "blocked" function, call it, otherwise just stay in place until the obstacle is gone
			if (pusher->v.blocked)
			{
				pr_global_struct->self = EDICT_TO_PROG(pusher);
				pr_global_struct->other = EDICT_TO_PROG(check);
				PR_ExecuteProgram (pusher->v.blocked, "");
			}
			return;
		}
	}
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	float thinktime, oldltime, movetime;

	oldltime = ent->v.ltime;

	thinktime = ent->v.nextthink;
	if (thinktime < ent->v.ltime + sv.frametime)
	{
		movetime = thinktime - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = sv.frametime;

	if (movetime)
		// advances ent->v.ltime if not blocked
		SV_PushMove (ent, movetime);

	if (thinktime > oldltime && thinktime <= ent->v.ltime)
	{
		ent->v.nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		PR_ExecuteProgram (ent->v.think, "NULL think function");
		if (ent->free)
			return;
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
		VectorCopy (ent->v.origin, ent->v.oldorigin);
		return;
	}

	VectorCopy (ent->v.origin, org);
	VectorCopy (ent->v.oldorigin, ent->v.origin);
	if (!SV_TestEntityPosition(ent))
	{
		Con_DPrintf ("Unstuck.\n");
		SV_LinkEdict (ent, true);
		return;
	}

	for (z=0 ; z< 18 ; z++)
		for (i=-1 ; i <= 1 ; i++)
			for (j=-1 ; j <= 1 ; j++)
			{
				ent->v.origin[0] = org[0] + i;
				ent->v.origin[1] = org[1] + j;
				ent->v.origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_DPrintf ("Unstuck.\n");
					SV_LinkEdict (ent, true);
					return;
				}
			}

	VectorCopy (org, ent->v.origin);
	Con_DPrintf ("player is stuck.\n");
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

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;
	cont = Mod_PointInLeaf(point, sv.worldmodel)->contents;
	if (cont <= CONTENTS_WATER)
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2])*0.5;
		cont = Mod_PointInLeaf(point, sv.worldmodel)->contents;
		if (cont <= CONTENTS_WATER)
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			cont = Mod_PointInLeaf(point, sv.worldmodel)->contents;
			if (cont <= CONTENTS_WATER)
				ent->v.waterlevel = 3;
		}
	}

	return ent->v.waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction (edict_t *ent, trace_t *trace)
{
	float d, i;
	vec3_t forward, into, side;

	AngleVectors (ent->v.v_angle, forward, NULL, NULL);
	d = DotProduct (trace->plane.normal, forward);

	d += 0.5;
	if (d >= 0)
		return;

	// cut the tangential velocity
	i = DotProduct (trace->plane.normal, ent->v.velocity);
	VectorScale (trace->plane.normal, i, into);
	VectorSubtract (ent->v.velocity, into, side);

	ent->v.velocity[0] = side[0] * (1 + d);
	ent->v.velocity[1] = side[1] * (1 + d);
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
	trace_t steptrace;

	VectorCopy (ent->v.origin, oldorg);
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

		SV_PushEntity (ent, dir, vec3_origin);

		// retry the original move
		ent->v.velocity[0] = oldvel[0];
		ent->v.velocity[1] = oldvel[1];
		ent->v.velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, &steptrace);

		if (fabs(oldorg[1] - ent->v.origin[1]) > 4
		 || fabs(oldorg[0] - ent->v.origin[0]) > 4)
			return clip;

		// go back to the original pos and try again
		VectorCopy (oldorg, ent->v.origin);
	}

	// still not moving
	VectorClear (ent->v.velocity);
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
	int clip, oldonground;
	vec3_t upmove, downmove, oldorg, oldvel, nosteporg, nostepvel;
	trace_t steptrace, downtrace;

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->v.flags & FL_ONGROUND;
	ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

	VectorCopy (ent->v.origin, oldorg);
	VectorCopy (ent->v.velocity, oldvel);

	clip = SV_FlyMove (ent, sv.frametime, &steptrace);

	// if move didn't block on a step, return
	if ( !(clip & 2) )
		return;

	if (ent->v.movetype != MOVETYPE_FLY)
	{
		if (!oldonground && ent->v.waterlevel == 0 && !sv_jumpstep.integer)
			// don't stair up while jumping
			return;

		if (ent->v.movetype != MOVETYPE_WALK)
			// gibbed by a trigger
			return;
	}

	if (sv_nostep.integer)
		return;

	if ( (int)sv_player->v.flags & FL_WATERJUMP )
		return;

	VectorCopy (ent->v.origin, nosteporg);
	VectorCopy (ent->v.velocity, nostepvel);

	// try moving up and forward to go up a step
	// back to start pos
	VectorCopy (oldorg, ent->v.origin);

	VectorClear (upmove);
	VectorClear (downmove);
	upmove[2] = sv_stepheight.value;
	downmove[2] = -sv_stepheight.value + oldvel[2]*sv.frametime;

	// move up
	// FIXME: don't link?
	SV_PushEntity (ent, upmove, vec3_origin);

	// move forward
	ent->v.velocity[0] = oldvel[0];
	ent->v.velocity[1] = oldvel[1];
	ent->v.velocity[2] = 0;
	clip = SV_FlyMove (ent, sv.frametime, &steptrace);
	ent->v.velocity[2] += oldvel[2];

	// check for stuckness, possibly due to the limited precision of floats
	// in the clipping hulls
	/*
	if (clip
	 && fabs(oldorg[1] - ent->v.origin[1]) < 0.03125
	 && fabs(oldorg[0] - ent->v.origin[0]) < 0.03125)
		// stepping up didn't make any progress
		clip = SV_TryUnstick (ent, oldvel);
	*/

	// extra friction based on view angle
	if (clip & 2 && sv_wallfriction.integer)
		SV_WallFriction (ent, &steptrace);

	// move down
	// FIXME: don't link?
	downtrace = SV_PushEntity (ent, downmove, vec3_origin);

	if (downtrace.plane.normal[2] > 0.7)
	{
		if (ent->v.solid == SOLID_BSP)
		{
			ent->v.flags =	(int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);
		}
	}
	else
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy (nosteporg, ent->v.origin);
		VectorCopy (nostepvel, ent->v.velocity);
	}
}


/*
================
SV_Physics_Client

Player character actions
================
*/
void SV_Physics_Client (edict_t	*ent, int num)
{
	if ( ! svs.clients[num-1].active )
		return;		// unconnected slot

//
// call standard client pre-think
//
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	PR_ExecuteProgram (pr_global_struct->PlayerPreThink, "QC function PlayerPreThink is missing");

//
// do a move
//
	SV_CheckVelocity (ent);

//
// decide which move function to call
//
	switch ((int)ent->v.movetype)
	{
	case MOVETYPE_NONE:
		if (!SV_RunThink (ent))
			return;
		break;

	case MOVETYPE_WALK:
		if (!SV_RunThink (ent))
			return;
		if (!SV_CheckWater (ent) && ! ((int)ent->v.flags & FL_WATERJUMP) )
			SV_AddGravity (ent);
		SV_CheckStuck (ent);
		SV_WalkMove (ent);
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_Physics_Toss (ent);
		break;

	case MOVETYPE_FLY:
		if (!SV_RunThink (ent))
			return;
		SV_CheckWater (ent);
		//SV_FlyMove (ent, sv.frametime, NULL);
		SV_WalkMove (ent);
		break;

	case MOVETYPE_NOCLIP:
		if (!SV_RunThink (ent))
			return;
		SV_CheckWater (ent);
		VectorMA (ent->v.origin, sv.frametime, ent->v.velocity, ent->v.origin);
		VectorMA (ent->v.angles, sv.frametime, ent->v.avelocity, ent->v.angles);
		break;

	default:
		Host_Error ("SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
	}

//
// call standard player post-think
//
	SV_LinkEdict (ent, true);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	PR_ExecuteProgram (pr_global_struct->PlayerPostThink, "QC function PlayerPostThink is missing");
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
	e = PROG_TO_EDICT(ent->v.aiment);
	if (e->v.angles[0] == ent->v.punchangle[0] && e->v.angles[1] == ent->v.punchangle[1] && e->v.angles[2] == ent->v.punchangle[2])
	{
		// quick case for no rotation
		VectorAdd(e->v.origin, ent->v.view_ofs, ent->v.origin);
	}
	else
	{
		angles[0] = -ent->v.punchangle[0];
		angles[1] =  ent->v.punchangle[1];
		angles[2] =  ent->v.punchangle[2];
		AngleVectors (angles, vf, vr, vu);
		v[0] = ent->v.view_ofs[0] * vf[0] + ent->v.view_ofs[1] * vr[0] + ent->v.view_ofs[2] * vu[0];
		v[1] = ent->v.view_ofs[0] * vf[1] + ent->v.view_ofs[1] * vr[1] + ent->v.view_ofs[2] * vu[1];
		v[2] = ent->v.view_ofs[0] * vf[2] + ent->v.view_ofs[1] * vr[2] + ent->v.view_ofs[2] * vu[2];
		angles[0] = -e->v.angles[0];
		angles[1] =  e->v.angles[1];
		angles[2] =  e->v.angles[2];
		AngleVectors (angles, vf, vr, vu);
		ent->v.origin[0] = v[0] * vf[0] + v[1] * vf[1] + v[2] * vf[2] + e->v.origin[0];
		ent->v.origin[1] = v[0] * vr[0] + v[1] * vr[1] + v[2] * vr[2] + e->v.origin[1];
		ent->v.origin[2] = v[0] * vu[0] + v[1] * vu[1] + v[2] * vu[2] + e->v.origin[2];
	}
	VectorAdd (e->v.angles, ent->v.v_angle, ent->v.angles);
	SV_LinkEdict (ent, true);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip (edict_t *ent)
{
	// regular thinking
	if (!SV_RunThink (ent))
		return;

	VectorMA (ent->v.angles, sv.frametime, ent->v.avelocity, ent->v.angles);
	VectorMA (ent->v.origin, sv.frametime, ent->v.velocity, ent->v.origin);

	SV_LinkEdict (ent, false);
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
	int		cont;
	cont = Mod_PointInLeaf(ent->v.origin, sv.worldmodel)->contents;
	if (!ent->v.watertype)
	{
		// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}

	if (cont <= CONTENTS_WATER)
	{
		if (ent->v.watertype == CONTENTS_EMPTY)
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);

		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
	}
	else
	{
		if (ent->v.watertype != CONTENTS_EMPTY)
			// just crossed into water
			SV_StartSound (ent, 0, "misc/h2ohit1.wav", 255, 1);

		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = cont;
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

	// regular thinking
	if (!SV_RunThink (ent))
		return;

// if onground, return without moving
	if ((int)ent->v.flags & FL_ONGROUND)
	{
		if (ent->v.groundentity == 0)
			return;
		groundentity = PROG_TO_EDICT(ent->v.groundentity);
		if (groundentity != NULL && groundentity->v.solid == SOLID_BSP)
			ent->suspendedinairflag = true;
		else if (ent->suspendedinairflag && (groundentity == NULL || groundentity->free))
		{
			// leave it suspended in the air
			ent->v.groundentity = 0;
			ent->suspendedinairflag = false;
			return;
		}
	}
	ent->suspendedinairflag = false;

	SV_CheckVelocity (ent);

// add gravity
	if (ent->v.movetype != MOVETYPE_FLY
	 && ent->v.movetype != MOVETYPE_BOUNCEMISSILE // LordHavoc: enabled MOVETYPE_BOUNCEMISSILE
	 && ent->v.movetype != MOVETYPE_FLYMISSILE)
		SV_AddGravity (ent);

// move angles
	VectorMA (ent->v.angles, sv.frametime, ent->v.avelocity, ent->v.angles);

// move origin
	VectorScale (ent->v.velocity, sv.frametime, move);
	trace = SV_PushEntity (ent, move, vec3_origin);
	if (ent->free)
		return;
	if (trace.fraction == 1)
		return;

	if (ent->v.movetype == MOVETYPE_BOUNCEMISSILE)
	{
		ClipVelocity (ent->v.velocity, trace.plane.normal, ent->v.velocity, 2.0);
		ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
	}
	else if (ent->v.movetype == MOVETYPE_BOUNCE)
	{
		ClipVelocity (ent->v.velocity, trace.plane.normal, ent->v.velocity, 1.5);
		// LordHavoc: fixed grenades not bouncing when fired down a slope
		if (trace.plane.normal[2] > 0.7 && DotProduct(trace.plane.normal, ent->v.velocity) < 60)
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			VectorClear (ent->v.velocity);
			VectorClear (ent->v.avelocity);
		}
		else
			ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
	}
	else
	{
		ClipVelocity (ent->v.velocity, trace.plane.normal, ent->v.velocity, 1.0);
		if (trace.plane.normal[2] > 0.7)
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			VectorClear (ent->v.velocity);
			VectorClear (ent->v.avelocity);
		}
		else
			ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;
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
	int flags, fall, hitsound;

	// freefall if not fly/swim
	fall = true;
	flags = (int)ent->v.flags;
	if (flags & (FL_FLY | FL_SWIM))
	{
		if (flags & FL_FLY)
			fall = false;
		else if ((flags & FL_SWIM) && Mod_PointInLeaf(ent->v.origin, sv.worldmodel)->contents != CONTENTS_EMPTY)
			fall = false;
	}
	if (fall && (flags & FL_ONGROUND) && ent->v.groundentity == 0)
		fall = false;

	if (fall)
	{
		if (ent->v.velocity[2] < sv_gravity.value*-0.1)
		{
			hitsound = true;
			if (flags & FL_ONGROUND)
				hitsound = false;
		}
		else
			hitsound = false;

		SV_AddGravity (ent);
		SV_CheckVelocity (ent);
		SV_FlyMove (ent, sv.frametime, NULL);
		SV_LinkEdict (ent, false);

		// just hit ground
		if ((int)ent->v.flags & FL_ONGROUND)
		{
			VectorClear(ent->v.velocity);
			if (hitsound)
				SV_StartSound (ent, 0, "demon/dland2.wav", 255, 1);
		}
	}

// regular thinking
	SV_RunThink (ent);

	SV_CheckWaterTransition (ent);
}

//============================================================================

/*
================
SV_Physics

================
*/
void SV_Physics (void)
{
	int i;
	edict_t *ent;

// let the progs know that a new frame has started
	pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
	pr_global_struct->time = sv.time;
	PR_ExecuteProgram (pr_global_struct->StartFrame, "QC function StartFrame is missing");

//
// treat each object in turn
//
	ent = sv.edicts;
	for (i=0 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;

		if (pr_global_struct->force_retouch)
			SV_LinkEdict (ent, true);	// force retouch even for stationary

		if (i > 0 && i <= svs.maxclients)
		{
			SV_Physics_Client (ent, i);
			continue;
		}

		switch ((int) ent->v.movetype)
		{
		case MOVETYPE_PUSH:
			SV_Physics_Pusher (ent);
			break;
		case MOVETYPE_NONE:
			// LordHavoc: manually inlined the thinktime check here because MOVETYPE_NONE is used on so many objects
			if (ent->v.nextthink > 0 && ent->v.nextthink <= sv.time + sv.frametime)
				SV_RunThink (ent);
			break;
		case MOVETYPE_FOLLOW:
			SV_Physics_Follow (ent);
			break;
		case MOVETYPE_NOCLIP:
			SV_Physics_Noclip (ent);
			break;
		case MOVETYPE_STEP:
			SV_Physics_Step (ent);
			break;
		// LordHavoc: added support for MOVETYPE_WALK on normal entities! :)
		case MOVETYPE_WALK:
			if (SV_RunThink (ent))
			{
				if (!SV_CheckWater (ent) && ! ((int)ent->v.flags & FL_WATERJUMP) )
					SV_AddGravity (ent);
				SV_CheckStuck (ent);
				SV_WalkMove (ent);
				SV_LinkEdict (ent, true);
			}
			break;
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_BOUNCEMISSILE:
		case MOVETYPE_FLY:
		case MOVETYPE_FLYMISSILE:
			SV_Physics_Toss (ent);
			break;
		default:
			Host_Error ("SV_Physics: bad movetype %i", (int)ent->v.movetype);
			break;
		}
	}

	if (pr_global_struct->force_retouch)
		pr_global_struct->force_retouch--;

	// LordHavoc: endframe support
	if (EndFrameQC)
	{
		pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
		pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram ((func_t)(EndFrameQC - pr_functions), "");
	}

	sv.time += sv.frametime;
}


trace_t SV_Trace_Toss (edict_t *tossent, edict_t *ignore)
{
	int i;
	float gravity, savesolid;
	vec3_t move, end;
	edict_t tempent, *tent;
	eval_t *val;
	trace_t trace;

	memcpy(&tempent, tossent, sizeof(edict_t));
	tent = &tempent;
	savesolid = tossent->v.solid;
	tossent->v.solid = SOLID_NOT;

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
		tent->v.velocity[2] -= gravity;
		VectorMA (tent->v.angles, 0.05, tent->v.avelocity, tent->v.angles);
		VectorScale (tent->v.velocity, 0.05, move);
		VectorAdd (tent->v.origin, move, end);
		trace = SV_Move (tent->v.origin, tent->v.mins, tent->v.maxs, end, MOVE_NORMAL, tent);
		VectorCopy (trace.endpos, tent->v.origin);

		if (trace.fraction < 1 && trace.ent)
			if (trace.ent != ignore)
				break;
	}
	tossent->v.solid = savesolid;
	trace.fraction = 0; // not relevant
	return trace;
}

