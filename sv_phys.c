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

#define	MOVE_EPSILON	0.01

void SV_Physics_Toss (prvm_edict_t *ent);

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

int SV_GenericHitSuperContentsMask(const prvm_edict_t *passedict)
{
	prvm_eval_t *val;
	if (passedict)
	{
		val = PRVM_EDICTFIELDVALUE(passedict, prog->fieldoffsets.dphitcontentsmask);
		if (val && val->_float)
			return (int)val->_float;
		else if (passedict->fields.server->solid == SOLID_SLIDEBOX)
		{
			if ((int)passedict->fields.server->flags & FL_MONSTER)
				return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_MONSTERCLIP;
			else
				return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_PLAYERCLIP;
		}
		else if (passedict->fields.server->solid == SOLID_CORPSE)
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY;
		else if (passedict->fields.server->solid == SOLID_TRIGGER)
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY;
		else
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_CORPSE;
	}
	else
		return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_CORPSE;
}

/*
==================
SV_TracePoint
==================
*/
trace_t SV_TracePoint(const vec3_t start, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
{
	int i, bodysupercontents;
	int passedictprog;
	float pitchsign = 1;
	prvm_edict_t *traceowner, *touch;
	trace_t trace;
	// bounding box of entire move area
	vec3_t clipboxmins, clipboxmaxs;
	// size when clipping against monsters
	vec3_t clipmins2, clipmaxs2;
	// start and end origin of move
	vec3_t clipstart;
	// trace results
	trace_t cliptrace;
	// matrices to transform into/out of other entity's space
	matrix4x4_t matrix, imatrix;
	// model of other entity
	dp_model_t *model;
	// list of entities to test for collisions
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];

	//return SV_TraceBox(start, vec3_origin, vec3_origin, end, type, passedict, hitsupercontentsmask);

	VectorCopy(start, clipstart);
	VectorClear(clipmins2);
	VectorClear(clipmaxs2);
#if COLLISIONPARANOID >= 3
	Con_Printf("move(%f %f %f)", clipstart[0], clipstart[1], clipstart[2]);
#endif

	// clip to world
	Collision_ClipPointToWorld(&cliptrace, sv.worldmodel, clipstart, hitsupercontentsmask);
	cliptrace.bmodelstartsolid = cliptrace.startsolid;
	if (cliptrace.startsolid || cliptrace.fraction < 1)
		cliptrace.ent = prog->edicts;
	if (type == MOVE_WORLDONLY)
		goto finished;

	if (type == MOVE_MISSILE)
	{
		// LordHavoc: modified this, was = -15, now -= 15
		for (i = 0;i < 3;i++)
		{
			clipmins2[i] -= 15;
			clipmaxs2[i] += 15;
		}
	}

	// create the bounding box of the entire move
	for (i = 0;i < 3;i++)
	{
		clipboxmins[i] = min(clipstart[i], cliptrace.endpos[i]) + clipmins2[i] - 1;
		clipboxmaxs[i] = max(clipstart[i], cliptrace.endpos[i]) + clipmaxs2[i] + 1;
	}

	// debug override to test against everything
	if (sv_debugmove.integer)
	{
		clipboxmins[0] = clipboxmins[1] = clipboxmins[2] = -999999999;
		clipboxmaxs[0] = clipboxmaxs[1] = clipboxmaxs[2] =  999999999;
	}

	// if the passedict is world, make it NULL (to avoid two checks each time)
	if (passedict == prog->edicts)
		passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = PRVM_EDICT_TO_PROG(passedict);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->fields.server->owner) : 0;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = World_EntitiesInBox(&sv.world, clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];

		if (touch->fields.server->solid < SOLID_BBOX)
			continue;
		if (type == MOVE_NOMONSTERS && touch->fields.server->solid != SOLID_BSP)
			continue;

		if (passedict)
		{
			// don't clip against self
			if (passedict == touch)
				continue;
			// don't clip owned entities against owner
			if (traceowner == touch)
				continue;
			// don't clip owner against owned entities
			if (passedictprog == touch->fields.server->owner)
				continue;
			// don't clip points against points (they can't collide)
			if (VectorCompare(touch->fields.server->mins, touch->fields.server->maxs) && (type != MOVE_MISSILE || !((int)touch->fields.server->flags & FL_MONSTER)))
				continue;
		}

		bodysupercontents = touch->fields.server->solid == SOLID_CORPSE ? SUPERCONTENTS_CORPSE : SUPERCONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if ((int) touch->fields.server->solid == SOLID_BSP || type == MOVE_HITMODEL)
		{
			unsigned int modelindex = (unsigned int)touch->fields.server->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if (modelindex > 0 && modelindex < MAX_MODELS)
				model = sv.models[(int)touch->fields.server->modelindex];
			//pitchsign = 1;
			if (
				((modelindex = (int)touch->fields.server->modelindex) >= 1 && modelindex < MAX_MODELS && (model = sv.models[(int)touch->fields.server->modelindex]))
				?
					model->type == mod_alias
				:
					(
						(((unsigned char)PRVM_EDICTFIELDVALUE(touch, prog->fieldoffsets.pflags)->_float) & PFLAGS_FULLDYNAMIC)
						||
						((gamemode == GAME_TENEBRAE) && ((unsigned int)touch->fields.server->effects & (16 | 32)))
					)
			)
				pitchsign = -1;
		}
		if (model)
			Matrix4x4_CreateFromQuakeEntity(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2], pitchsign * touch->fields.server->angles[0], touch->fields.server->angles[1], touch->fields.server->angles[2], 1);
		else
			Matrix4x4_CreateTranslate(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2]);
		Matrix4x4_Invert_Simple(&imatrix, &matrix);
		if (type == MOVE_MISSILE && (int)touch->fields.server->flags & FL_MONSTER)
			Collision_ClipToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins2, clipmaxs2, clipstart, hitsupercontentsmask);
		else
			Collision_ClipPointToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, hitsupercontentsmask);

		Collision_CombineTraces(&cliptrace, &trace, (void *)touch, touch->fields.server->solid == SOLID_BSP);
	}

finished:
	return cliptrace;
}

/*
==================
SV_TraceLine
==================
*/
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
trace_t SV_TraceLine(const vec3_t start, const vec3_t pEnd, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#else
trace_t SV_TraceLine(const vec3_t start, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#endif
{
	int i, bodysupercontents;
	int passedictprog;
	float pitchsign = 1;
	prvm_edict_t *traceowner, *touch;
	trace_t trace;
	// bounding box of entire move area
	vec3_t clipboxmins, clipboxmaxs;
	// size when clipping against monsters
	vec3_t clipmins2, clipmaxs2;
	// start and end origin of move
	vec3_t clipstart, clipend;
	// trace results
	trace_t cliptrace;
	// matrices to transform into/out of other entity's space
	matrix4x4_t matrix, imatrix;
	// model of other entity
	dp_model_t *model;
	// list of entities to test for collisions
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
	vec3_t end;
	vec_t len = 0;

	if(!VectorCompare(start, pEnd))
	{
		// TRICK: make the trace 1 qu longer!
		VectorSubtract(pEnd, start, end);
		len = VectorNormalizeLength(end);
		VectorAdd(pEnd, end, end);
	}
	else
		VectorCopy(pEnd, end);
#endif

	//return SV_TraceBox(start, vec3_origin, vec3_origin, end, type, passedict, hitsupercontentsmask);

	if (VectorCompare(start, end))
		return SV_TracePoint(start, type, passedict, hitsupercontentsmask);

	VectorCopy(start, clipstart);
	VectorCopy(end, clipend);
	VectorClear(clipmins2);
	VectorClear(clipmaxs2);
#if COLLISIONPARANOID >= 3
	Con_Printf("move(%f %f %f,%f %f %f)", clipstart[0], clipstart[1], clipstart[2], clipend[0], clipend[1], clipend[2]);
#endif

	// clip to world
	Collision_ClipLineToWorld(&cliptrace, sv.worldmodel, clipstart, clipend, hitsupercontentsmask);
	cliptrace.bmodelstartsolid = cliptrace.startsolid;
	if (cliptrace.startsolid || cliptrace.fraction < 1)
		cliptrace.ent = prog->edicts;
	if (type == MOVE_WORLDONLY)
		goto finished;

	if (type == MOVE_MISSILE)
	{
		// LordHavoc: modified this, was = -15, now -= 15
		for (i = 0;i < 3;i++)
		{
			clipmins2[i] -= 15;
			clipmaxs2[i] += 15;
		}
	}

	// create the bounding box of the entire move
	for (i = 0;i < 3;i++)
	{
		clipboxmins[i] = min(clipstart[i], cliptrace.endpos[i]) + clipmins2[i] - 1;
		clipboxmaxs[i] = max(clipstart[i], cliptrace.endpos[i]) + clipmaxs2[i] + 1;
	}

	// debug override to test against everything
	if (sv_debugmove.integer)
	{
		clipboxmins[0] = clipboxmins[1] = clipboxmins[2] = -999999999;
		clipboxmaxs[0] = clipboxmaxs[1] = clipboxmaxs[2] =  999999999;
	}

	// if the passedict is world, make it NULL (to avoid two checks each time)
	if (passedict == prog->edicts)
		passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = PRVM_EDICT_TO_PROG(passedict);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->fields.server->owner) : 0;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = World_EntitiesInBox(&sv.world, clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];

		if (touch->fields.server->solid < SOLID_BBOX)
			continue;
		if (type == MOVE_NOMONSTERS && touch->fields.server->solid != SOLID_BSP)
			continue;

		if (passedict)
		{
			// don't clip against self
			if (passedict == touch)
				continue;
			// don't clip owned entities against owner
			if (traceowner == touch)
				continue;
			// don't clip owner against owned entities
			if (passedictprog == touch->fields.server->owner)
				continue;
			// don't clip points against points (they can't collide)
			if (VectorCompare(touch->fields.server->mins, touch->fields.server->maxs) && (type != MOVE_MISSILE || !((int)touch->fields.server->flags & FL_MONSTER)))
				continue;
		}

		bodysupercontents = touch->fields.server->solid == SOLID_CORPSE ? SUPERCONTENTS_CORPSE : SUPERCONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if ((int) touch->fields.server->solid == SOLID_BSP || type == MOVE_HITMODEL)
		{
			unsigned int modelindex = (unsigned int)touch->fields.server->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if (modelindex > 0 && modelindex < MAX_MODELS)
				model = sv.models[(int)touch->fields.server->modelindex];
			//pitchsign = 1;
			if (
				((modelindex = (int)touch->fields.server->modelindex) >= 1 && modelindex < MAX_MODELS && (model = sv.models[(int)touch->fields.server->modelindex]))
				?
					model->type == mod_alias
				:
					(
						(((unsigned char)PRVM_EDICTFIELDVALUE(touch, prog->fieldoffsets.pflags)->_float) & PFLAGS_FULLDYNAMIC)
						||
						((gamemode == GAME_TENEBRAE) && ((unsigned int)touch->fields.server->effects & (16 | 32)))
					)
			)
				pitchsign = -1;
		}
		if (model)
			Matrix4x4_CreateFromQuakeEntity(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2], pitchsign * touch->fields.server->angles[0], touch->fields.server->angles[1], touch->fields.server->angles[2], 1);
		else
			Matrix4x4_CreateTranslate(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2]);
		Matrix4x4_Invert_Simple(&imatrix, &matrix);
		if (type == MOVE_MISSILE && (int)touch->fields.server->flags & FL_MONSTER)
			Collision_ClipToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins2, clipmaxs2, clipend, hitsupercontentsmask);
		else
			Collision_ClipLineToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipend, hitsupercontentsmask);

		Collision_CombineTraces(&cliptrace, &trace, (void *)touch, touch->fields.server->solid == SOLID_BSP);
	}

finished:
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
	if(!VectorCompare(start, pEnd))
		Collision_ShortenTrace(&cliptrace, len / (len + 1), pEnd);
#endif
	return cliptrace;
}

/*
==================
SV_Move
==================
*/
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
#if COLLISIONPARANOID >= 1
trace_t SV_TraceBox_(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t pEnd, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#else
trace_t SV_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t pEnd, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#endif
#else
#if COLLISIONPARANOID >= 1
trace_t SV_TraceBox_(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#else
trace_t SV_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
#endif
#endif
{
	vec3_t hullmins, hullmaxs;
	int i, bodysupercontents;
	int passedictprog;
	float pitchsign = 1;
	qboolean pointtrace;
	prvm_edict_t *traceowner, *touch;
	trace_t trace;
	// bounding box of entire move area
	vec3_t clipboxmins, clipboxmaxs;
	// size of the moving object
	vec3_t clipmins, clipmaxs;
	// size when clipping against monsters
	vec3_t clipmins2, clipmaxs2;
	// start and end origin of move
	vec3_t clipstart, clipend;
	// trace results
	trace_t cliptrace;
	// matrices to transform into/out of other entity's space
	matrix4x4_t matrix, imatrix;
	// model of other entity
	dp_model_t *model;
	// list of entities to test for collisions
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
	vec3_t end;
	vec_t len = 0;

	if(!VectorCompare(start, pEnd))
	{
		// TRICK: make the trace 1 qu longer!
		VectorSubtract(pEnd, start, end);
		len = VectorNormalizeLength(end);
		VectorAdd(pEnd, end, end);
	}
	else
		VectorCopy(pEnd, end);
#endif

	if (VectorCompare(mins, maxs))
	{
		vec3_t shiftstart, shiftend;
		VectorAdd(start, mins, shiftstart);
		VectorAdd(end, mins, shiftend);
		if (VectorCompare(start, end))
			trace = SV_TracePoint(shiftstart, type, passedict, hitsupercontentsmask);
		else
			trace = SV_TraceLine(shiftstart, shiftend, type, passedict, hitsupercontentsmask);
		VectorSubtract(trace.endpos, mins, trace.endpos);
		return trace;
	}

	VectorCopy(start, clipstart);
	VectorCopy(end, clipend);
	VectorCopy(mins, clipmins);
	VectorCopy(maxs, clipmaxs);
	VectorCopy(mins, clipmins2);
	VectorCopy(maxs, clipmaxs2);
#if COLLISIONPARANOID >= 3
	Con_Printf("move(%f %f %f,%f %f %f)", clipstart[0], clipstart[1], clipstart[2], clipend[0], clipend[1], clipend[2]);
#endif

	// clip to world
	Collision_ClipToWorld(&cliptrace, sv.worldmodel, clipstart, clipmins, clipmaxs, clipend, hitsupercontentsmask);
	cliptrace.bmodelstartsolid = cliptrace.startsolid;
	if (cliptrace.startsolid || cliptrace.fraction < 1)
		cliptrace.ent = prog->edicts;
	if (type == MOVE_WORLDONLY)
		goto finished;

	if (type == MOVE_MISSILE)
	{
		// LordHavoc: modified this, was = -15, now -= 15
		for (i = 0;i < 3;i++)
		{
			clipmins2[i] -= 15;
			clipmaxs2[i] += 15;
		}
	}

	// get adjusted box for bmodel collisions if the world is q1bsp or hlbsp
	if (sv.worldmodel && sv.worldmodel->brush.RoundUpToHullSize)
		sv.worldmodel->brush.RoundUpToHullSize(sv.worldmodel, clipmins, clipmaxs, hullmins, hullmaxs);
	else
	{
		VectorCopy(clipmins, hullmins);
		VectorCopy(clipmaxs, hullmaxs);
	}

	// create the bounding box of the entire move
	for (i = 0;i < 3;i++)
	{
		clipboxmins[i] = min(clipstart[i], cliptrace.endpos[i]) + min(hullmins[i], clipmins2[i]) - 1;
		clipboxmaxs[i] = max(clipstart[i], cliptrace.endpos[i]) + max(hullmaxs[i], clipmaxs2[i]) + 1;
	}

	// debug override to test against everything
	if (sv_debugmove.integer)
	{
		clipboxmins[0] = clipboxmins[1] = clipboxmins[2] = -999999999;
		clipboxmaxs[0] = clipboxmaxs[1] = clipboxmaxs[2] =  999999999;
	}

	// if the passedict is world, make it NULL (to avoid two checks each time)
	if (passedict == prog->edicts)
		passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = PRVM_EDICT_TO_PROG(passedict);
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare(clipmins, clipmaxs);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->fields.server->owner) : 0;

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	numtouchedicts = World_EntitiesInBox(&sv.world, clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];

		if (touch->fields.server->solid < SOLID_BBOX)
			continue;
		if (type == MOVE_NOMONSTERS && touch->fields.server->solid != SOLID_BSP)
			continue;

		if (passedict)
		{
			// don't clip against self
			if (passedict == touch)
				continue;
			// don't clip owned entities against owner
			if (traceowner == touch)
				continue;
			// don't clip owner against owned entities
			if (passedictprog == touch->fields.server->owner)
				continue;
			// don't clip points against points (they can't collide)
			if (pointtrace && VectorCompare(touch->fields.server->mins, touch->fields.server->maxs) && (type != MOVE_MISSILE || !((int)touch->fields.server->flags & FL_MONSTER)))
				continue;
		}

		bodysupercontents = touch->fields.server->solid == SOLID_CORPSE ? SUPERCONTENTS_CORPSE : SUPERCONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if ((int) touch->fields.server->solid == SOLID_BSP || type == MOVE_HITMODEL)
		{
			unsigned int modelindex = (unsigned int)touch->fields.server->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if (modelindex > 0 && modelindex < MAX_MODELS)
				model = sv.models[(int)touch->fields.server->modelindex];
			//pitchsign = 1;
			if (
				((modelindex = (int)touch->fields.server->modelindex) >= 1 && modelindex < MAX_MODELS && (model = sv.models[(int)touch->fields.server->modelindex]))
				?
					model->type == mod_alias
				:
					(
						(((unsigned char)PRVM_EDICTFIELDVALUE(touch, prog->fieldoffsets.pflags)->_float) & PFLAGS_FULLDYNAMIC)
						||
						((gamemode == GAME_TENEBRAE) && ((unsigned int)touch->fields.server->effects & (16 | 32)))
					)
			)
				pitchsign = -1;
		}
		if (model)
			Matrix4x4_CreateFromQuakeEntity(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2], pitchsign * touch->fields.server->angles[0], touch->fields.server->angles[1], touch->fields.server->angles[2], 1);
		else
			Matrix4x4_CreateTranslate(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2]);
		Matrix4x4_Invert_Simple(&imatrix, &matrix);
		if (type == MOVE_MISSILE && (int)touch->fields.server->flags & FL_MONSTER)
			Collision_ClipToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins2, clipmaxs2, clipend, hitsupercontentsmask);
		else
			Collision_ClipToGenericEntity(&trace, model, (int) touch->fields.server->frame, touch->fields.server->mins, touch->fields.server->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins, clipmaxs, clipend, hitsupercontentsmask);

		Collision_CombineTraces(&cliptrace, &trace, (void *)touch, touch->fields.server->solid == SOLID_BSP);
	}

finished:
#ifdef COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND
	if(!VectorCompare(start, pEnd))
		Collision_ShortenTrace(&cliptrace, len / (len + 1), pEnd);
#endif
	return cliptrace;
}

#if COLLISIONPARANOID >= 1
trace_t SV_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask)
{
	int endstuck;
	trace_t trace;
	vec3_t temp;
	trace = SV_TraceBox_(start, mins, maxs, end, type, passedict, hitsupercontentsmask);
	if (passedict)
	{
		VectorCopy(trace.endpos, temp);
		endstuck = SV_TraceBox_(temp, mins, maxs, temp, type, passedict, hitsupercontentsmask).startsolid;
#if COLLISIONPARANOID < 3
		if (trace.startsolid || endstuck)
#endif
			Con_Printf("%s{e%i:%f %f %f:%f %f %f:%f:%f %f %f%s%s}\n", (trace.startsolid || endstuck) ? "^3" : "", passedict ? (int)(passedict - prog->edicts) : -1, passedict->fields.server->origin[0], passedict->fields.server->origin[1], passedict->fields.server->origin[2], end[0] - passedict->fields.server->origin[0], end[1] - passedict->fields.server->origin[1], end[2] - passedict->fields.server->origin[2], trace.fraction, trace.endpos[0] - passedict->fields.server->origin[0], trace.endpos[1] - passedict->fields.server->origin[1], trace.endpos[2] - passedict->fields.server->origin[2], trace.startsolid ? " startstuck" : "", endstuck ? " endstuck" : "");
	}
	return trace;
}
#endif

int SV_PointSuperContents(const vec3_t point)
{
	int supercontents = 0;
	int i;
	prvm_edict_t *touch;
	vec3_t transformed;
	// matrices to transform into/out of other entity's space
	matrix4x4_t matrix, imatrix;
	// model of other entity
	dp_model_t *model;
	unsigned int modelindex;
	int frame;
	// list of entities to test for collisions
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];

	// get world supercontents at this point
	if (sv.worldmodel && sv.worldmodel->PointSuperContents)
		supercontents = sv.worldmodel->PointSuperContents(sv.worldmodel, 0, point);

	// if sv_gameplayfix_swiminbmodels is off we're done
	if (!sv_gameplayfix_swiminbmodels.integer)
		return supercontents;

	// get list of entities at this point
	numtouchedicts = World_EntitiesInBox(&sv.world, point, point, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];

		// we only care about SOLID_BSP for pointcontents
		if (touch->fields.server->solid != SOLID_BSP)
			continue;

		// might interact, so do an exact clip
		modelindex = (unsigned int)touch->fields.server->modelindex;
		if (modelindex >= MAX_MODELS)
			continue;
		model = sv.models[(int)touch->fields.server->modelindex];
		if (!model || !model->PointSuperContents)
			continue;
		Matrix4x4_CreateFromQuakeEntity(&matrix, touch->fields.server->origin[0], touch->fields.server->origin[1], touch->fields.server->origin[2], touch->fields.server->angles[0], touch->fields.server->angles[1], touch->fields.server->angles[2], 1);
		Matrix4x4_Invert_Simple(&imatrix, &matrix);
		Matrix4x4_Transform(&imatrix, point, transformed);
		frame = (int)touch->fields.server->frame;
		supercontents |= model->PointSuperContents(model, bound(0, frame, (model->numframes - 1)), transformed);
	}

	return supercontents;
}

/*
===============================================================================

Linking entities into the world culling system

===============================================================================
*/

void SV_LinkEdict_TouchAreaGrid(prvm_edict_t *ent)
{
	int i, numtouchedicts, old_self, old_other;
	prvm_edict_t *touch, *touchedicts[MAX_EDICTS];

	if (ent == prog->edicts)
		return;		// don't add the world

	if (ent->priv.server->free)
		return;

	if (ent->fields.server->solid == SOLID_NOT)
		return;

	// build a list of edicts to touch, because the link loop can be corrupted
	// by IncreaseEdicts called during touch functions
	numtouchedicts = World_EntitiesInBox(&sv.world, ent->priv.server->areamins, ent->priv.server->areamaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}

	old_self = prog->globals.server->self;
	old_other = prog->globals.server->other;
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];
		if (touch != ent && (int)touch->fields.server->solid == SOLID_TRIGGER && touch->fields.server->touch)
		{
			prvm_eval_t *val;
			prog->globals.server->self = PRVM_EDICT_TO_PROG(touch);
			prog->globals.server->other = PRVM_EDICT_TO_PROG(ent);
			prog->globals.server->time = sv.time;
			prog->globals.server->trace_allsolid = false;
			prog->globals.server->trace_startsolid = false;
			prog->globals.server->trace_fraction = 1;
			prog->globals.server->trace_inwater = false;
			prog->globals.server->trace_inopen = true;
			VectorCopy (touch->fields.server->origin, prog->globals.server->trace_endpos);
			VectorSet (prog->globals.server->trace_plane_normal, 0, 0, 1);
			prog->globals.server->trace_plane_dist = 0;
			prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(ent);
			if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dpstartcontents)))
				val->_float = 0;
			if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitcontents)))
				val->_float = 0;
			if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitq3surfaceflags)))
				val->_float = 0;
			if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphittexturename)))
				val->string = 0;
			PRVM_ExecuteProgram (touch->fields.server->touch, "QC function self.touch is missing");
		}
	}
	prog->globals.server->self = old_self;
	prog->globals.server->other = old_other;
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (prvm_edict_t *ent)
{
	dp_model_t *model;
	vec3_t mins, maxs;

	if (ent == prog->edicts)
		return;		// don't add the world

	if (ent->priv.server->free)
		return;

// set the abs box

	if (ent->fields.server->solid == SOLID_BSP)
	{
		int modelindex = (int)ent->fields.server->modelindex;
		if (modelindex < 0 || modelindex >= MAX_MODELS)
		{
			Con_Printf("edict %i: SOLID_BSP with invalid modelindex!\n", PRVM_NUM_FOR_EDICT(ent));
			modelindex = 0;
		}
		model = sv.models[modelindex];
		if (model != NULL)
		{
			if (!model->TraceBox && developer.integer >= 1)
				Con_Printf("edict %i: SOLID_BSP with non-collidable model\n", PRVM_NUM_FOR_EDICT(ent));

			if (ent->fields.server->angles[0] || ent->fields.server->angles[2] || ent->fields.server->avelocity[0] || ent->fields.server->avelocity[2])
			{
				VectorAdd(ent->fields.server->origin, model->rotatedmins, mins);
				VectorAdd(ent->fields.server->origin, model->rotatedmaxs, maxs);
			}
			else if (ent->fields.server->angles[1] || ent->fields.server->avelocity[1])
			{
				VectorAdd(ent->fields.server->origin, model->yawmins, mins);
				VectorAdd(ent->fields.server->origin, model->yawmaxs, maxs);
			}
			else
			{
				VectorAdd(ent->fields.server->origin, model->normalmins, mins);
				VectorAdd(ent->fields.server->origin, model->normalmaxs, maxs);
			}
		}
		else
		{
			// SOLID_BSP with no model is valid, mainly because some QC setup code does so temporarily
			VectorAdd(ent->fields.server->origin, ent->fields.server->mins, mins);
			VectorAdd(ent->fields.server->origin, ent->fields.server->maxs, maxs);
		}
	}
	else
	{
		VectorAdd(ent->fields.server->origin, ent->fields.server->mins, mins);
		VectorAdd(ent->fields.server->origin, ent->fields.server->maxs, maxs);
	}

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->fields.server->flags & FL_ITEM)
	{
		mins[0] -= 15;
		mins[1] -= 15;
		mins[2] -= 1;
		maxs[0] += 15;
		maxs[1] += 15;
		maxs[2] += 1;
	}
	else
	{
		// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		mins[0] -= 1;
		mins[1] -= 1;
		mins[2] -= 1;
		maxs[0] += 1;
		maxs[1] += 1;
		maxs[2] += 1;
	}

	VectorCopy(mins, ent->fields.server->absmin);
	VectorCopy(maxs, ent->fields.server->absmax);

	World_LinkEdict(&sv.world, ent, mins, maxs);
}

/*
===============================================================================

Utility functions

===============================================================================
*/

/*
============
SV_TestEntityPosition

returns true if the entity is in solid currently
============
*/
static int SV_TestEntityPosition (prvm_edict_t *ent, vec3_t offset)
{
	int contents;
	vec3_t org;
	trace_t trace;
	contents = SV_GenericHitSuperContentsMask(ent);
	VectorAdd(ent->fields.server->origin, offset, org);
	trace = SV_TraceBox(org, ent->fields.server->mins, ent->fields.server->maxs, ent->fields.server->origin, MOVE_NOMONSTERS, ent, contents);
	if (trace.startsupercontents & contents)
		return true;
	else
	{
		if (sv.worldmodel->brushq1.numclipnodes && !VectorCompare(ent->fields.server->mins, ent->fields.server->maxs))
		{
			// q1bsp/hlbsp use hulls and if the entity does not exactly match
			// a hull size it is incorrectly tested, so this code tries to
			// 'fix' it slightly...
			// FIXME: this breaks entities larger than the hull size
			int i;
			vec3_t v, m1, m2, s;
			VectorAdd(org, ent->fields.server->mins, m1);
			VectorAdd(org, ent->fields.server->maxs, m2);
			VectorSubtract(m2, m1, s);
#define EPSILON (1.0f / 32.0f)
			if (s[0] >= EPSILON*2) {m1[0] += EPSILON;m2[0] -= EPSILON;}
			if (s[1] >= EPSILON*2) {m1[1] += EPSILON;m2[1] -= EPSILON;}
			if (s[2] >= EPSILON*2) {m1[2] += EPSILON;m2[2] -= EPSILON;}
			for (i = 0;i < 8;i++)
			{
				v[0] = (i & 1) ? m2[0] : m1[0];
				v[1] = (i & 2) ? m2[1] : m1[1];
				v[2] = (i & 4) ? m2[2] : m1[2];
				if (SV_PointSuperContents(v) & contents)
					return true;
			}
		}
	}
	// if the trace found a better position for the entity, move it there
	if (VectorDistance2(trace.endpos, ent->fields.server->origin) >= 0.0001)
	{
#if 0
		// please switch back to this code when trace.endpos sometimes being in solid bug is fixed
		VectorCopy(trace.endpos, ent->fields.server->origin);
#else
		// verify if the endpos is REALLY outside solid
		VectorCopy(trace.endpos, org);
		trace = SV_TraceBox(org, ent->fields.server->mins, ent->fields.server->maxs, org, MOVE_NOMONSTERS, ent, contents);
		if(trace.startsolid)
			Con_Printf("SV_TestEntityPosition: trace.endpos detected to be in solid. NOT using it.\n");
		else
			VectorCopy(org, ent->fields.server->origin);
#endif
	}
	return false;
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

		if (SV_TestEntityPosition (check, vec3_origin))
			Con_Print("entity in invalid position\n");
	}
}

// DRESK - Support for Entity Contents Transition Event
/*
================
SV_CheckContentsTransition

returns true if entity had a valid contentstransition function call
================
*/
int SV_CheckContentsTransition(prvm_edict_t *ent, const int nContents)
{
	int bValidFunctionCall;
	prvm_eval_t *contentstransition;

	// Default Valid Function Call to False
	bValidFunctionCall = false;

	if(ent->fields.server->watertype != nContents)
	{ // Changed Contents
		// Acquire Contents Transition Function from QC
		contentstransition = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.contentstransition);

		if(contentstransition->function)
		{ // Valid Function; Execute
			// Assign Valid Function
			bValidFunctionCall = true;
			// Prepare Parameters (Original Contents, New Contents)
				// Original Contents
				PRVM_G_FLOAT(OFS_PARM0) = ent->fields.server->watertype;
				// New Contents
				PRVM_G_FLOAT(OFS_PARM1) = nContents;
				// Assign Self
				prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
			// Execute VM Function
			PRVM_ExecuteProgram(contentstransition->function, "contentstransition: NULL function");
		}
	}

	// Return if Function Call was Valid
	return bValidFunctionCall;
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
			Con_Printf("Got a NaN velocity on entity #%i (%s)\n", PRVM_NUM_FOR_EDICT(ent), PRVM_GetString(ent->fields.server->classname));
			ent->fields.server->velocity[i] = 0;
		}
		if (IS_NAN(ent->fields.server->origin[i]))
		{
			Con_Printf("Got a NaN origin on entity #%i (%s)\n", PRVM_NUM_FOR_EDICT(ent), PRVM_GetString(ent->fields.server->classname));
			ent->fields.server->origin[i] = 0;
		}
	}

	// LordHavoc: a hack to ensure that the (rather silly) id1 quakec
	// player_run/player_stand1 does not horribly malfunction if the
	// velocity becomes a denormalized float
	if (VectorLength2(ent->fields.server->velocity) < 0.0001)
		VectorClear(ent->fields.server->velocity);

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
	int iterations;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (ent->fields.server->nextthink <= 0 || ent->fields.server->nextthink > sv.time + sv.frametime)
		return true;

	for (iterations = 0;iterations < 128  && !ent->priv.server->free;iterations++)
	{
		prog->globals.server->time = max(sv.time, ent->fields.server->nextthink);
		ent->fields.server->nextthink = 0;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (ent->fields.server->think, "QC function self.think is missing");
		// mods often set nextthink to time to cause a think every frame,
		// we don't want to loop in that case, so exit if the new nextthink is
		// <= the time the qc was told, also exit if it is past the end of the
		// frame
		if (ent->fields.server->nextthink <= prog->globals.server->time || ent->fields.server->nextthink > sv.time + sv.frametime || !sv_gameplayfix_multiplethinksperframe.integer)
			break;
	}
	return !ent->priv.server->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
returns true if the impact kept the origin of the touching entity intact
==================
*/
extern void VM_SetTraceGlobals(const trace_t *trace);
extern sizebuf_t vm_tempstringsbuf;
qboolean SV_Impact (prvm_edict_t *e1, trace_t *trace)
{
	int restorevm_tempstringsbuf_cursize;
	int old_self, old_other;
	vec3_t org;
	prvm_edict_t *e2 = (prvm_edict_t *)trace->ent;
	prvm_eval_t *val;

	old_self = prog->globals.server->self;
	old_other = prog->globals.server->other;
	restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;

	VectorCopy(e1->fields.server->origin, org);

	VM_SetTraceGlobals(trace);

	prog->globals.server->time = sv.time;
	if (!e1->priv.server->free && !e2->priv.server->free && e1->fields.server->touch && e1->fields.server->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e2);
		PRVM_ExecuteProgram (e1->fields.server->touch, "QC function self.touch is missing");
	}

	if (!e1->priv.server->free && !e2->priv.server->free && e2->fields.server->touch && e2->fields.server->solid != SOLID_NOT)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e1);
		VectorCopy(e2->fields.server->origin, prog->globals.server->trace_endpos);
		VectorNegate(trace->plane.normal, prog->globals.server->trace_plane_normal);
		prog->globals.server->trace_plane_dist = -trace->plane.dist;
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(e1);
		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dpstartcontents)))
			val->_float = 0;
		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitcontents)))
			val->_float = 0;
		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitq3surfaceflags)))
			val->_float = 0;
		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphittexturename)))
			val->string = 0;
		PRVM_ExecuteProgram (e2->fields.server->touch, "QC function self.touch is missing");
	}

	prog->globals.server->self = old_self;
	prog->globals.server->other = old_other;
	vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;

	return VectorCompare(e1->fields.server->origin, org);
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
8 = teleported by touch method
If stepnormal is not NULL, the plane normal of any vertical wall hit will be stored
============
*/
static float SV_Gravity (prvm_edict_t *ent);
static qboolean SV_PushEntity (trace_t *trace, prvm_edict_t *ent, vec3_t push, qboolean failonbmodelstartsolid, qboolean dolink);
#define MAX_CLIP_PLANES 5
static int SV_FlyMove (prvm_edict_t *ent, float time, qboolean applygravity, float *stepnormal, int hitsupercontentsmask)
{
	int blocked, bumpcount;
	int i, j, numplanes;
	float d, time_left, gravity;
	vec3_t dir, push, planes[MAX_CLIP_PLANES], primal_velocity, original_velocity, new_velocity;
#if 0
	vec3_t end;
#endif
	trace_t trace;
	if (time <= 0)
		return 0;
	gravity = 0;
	if (applygravity)
	{
		if (sv_gameplayfix_gravityunaffectedbyticrate.integer)
		{
			gravity = SV_Gravity(ent) * 0.5f;
			ent->fields.server->velocity[2] -= gravity;
		}
		else
		{
			applygravity = false;
			ent->fields.server->velocity[2] -= SV_Gravity(ent);
		}
	}
	blocked = 0;
	VectorCopy(ent->fields.server->velocity, original_velocity);
	VectorCopy(ent->fields.server->velocity, primal_velocity);
	numplanes = 0;
	time_left = time;
	for (bumpcount = 0;bumpcount < MAX_CLIP_PLANES;bumpcount++)
	{
		if (!ent->fields.server->velocity[0] && !ent->fields.server->velocity[1] && !ent->fields.server->velocity[2])
			break;

		VectorScale(ent->fields.server->velocity, time_left, push);
#if 0
		VectorAdd(ent->fields.server->origin, push, end);
#endif
		if(!SV_PushEntity(&trace, ent, push, false, false))
		{
			// we got teleported by a touch function
			// let's abort the move
			blocked |= 8;
			break;
		}

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
			testtrace = SV_TraceBox(start, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent, hitsupercontentsmask);
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
				testtrace = SV_TraceBox(start, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent, hitsupercontentsmask);
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

#if 0
		if (trace.bmodelstartsolid)
		{
			// LordHavoc: note: this code is what makes entities stick in place
			// if embedded in world only (you can walk through other objects if
			// stuck)
			// entity is trapped in another solid
			VectorClear(ent->fields.server->velocity);
			return 3;
		}
#endif

		if (trace.fraction == 1)
			break;
		if (trace.plane.normal[2])
		{
			if (trace.plane.normal[2] > 0.7)
			{
				// floor
				blocked |= 1;

				if (!trace.ent)
				{
					Con_Printf ("SV_FlyMove: !trace.ent");
					trace.ent = prog->edicts;
				}

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
			VectorCopy(ent->fields.server->velocity, original_velocity);
			numplanes = 0;
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
		trace = SV_TraceBox(ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent, hitsupercontentsmask);
	}
	*/

	// LordHavoc: this came from QW and allows you to get out of water more easily
	if (sv_gameplayfix_easierwaterjump.integer && ((int)ent->fields.server->flags & FL_WATERJUMP) && !(blocked & 8))
		VectorCopy(primal_velocity, ent->fields.server->velocity);
	if (applygravity && !((int)ent->fields.server->flags & FL_ONGROUND))
		ent->fields.server->velocity[2] -= gravity;
	return blocked;
}

/*
============
SV_Gravity

============
*/
static float SV_Gravity (prvm_edict_t *ent)
{
	float ent_gravity;
	prvm_eval_t *val;

	val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.gravity);
	if (val!=0 && val->_float)
		ent_gravity = val->_float;
	else
		ent_gravity = 1.0;
	return ent_gravity * sv_gravity.value * sv.frametime;
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
The trace struct is filled with the trace that has been done.
Returns true if the push did not result in the entity being teleported by QC code.
============
*/
static qboolean SV_PushEntity (trace_t *trace, prvm_edict_t *ent, vec3_t push, qboolean failonbmodelstartsolid, qboolean dolink)
{
	int type;
	vec3_t end;

	VectorAdd (ent->fields.server->origin, push, end);

	if (ent->fields.server->movetype == MOVETYPE_FLYMISSILE)
		type = MOVE_MISSILE;
	else if (ent->fields.server->solid == SOLID_TRIGGER || ent->fields.server->solid == SOLID_NOT)
		type = MOVE_NOMONSTERS; // only clip against bmodels
	else
		type = MOVE_NORMAL;

	*trace = SV_TraceBox(ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, type, ent, SV_GenericHitSuperContentsMask(ent));
	if (trace->bmodelstartsolid && failonbmodelstartsolid)
		return true;

	VectorCopy (trace->endpos, ent->fields.server->origin);
	SV_LinkEdict(ent);

#if 0
	if(!trace->startsolid)
	if(SV_TraceBox(ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, ent->fields.server->origin, type, ent, SV_GenericHitSuperContentsMask(ent)).startsolid)
	{
		Con_Printf("something eeeeevil happened\n");
	}
#endif

	if (dolink)
		SV_LinkEdict_TouchAreaGrid(ent);

	if((ent->fields.server->solid >= SOLID_TRIGGER && trace->ent && (!((int)ent->fields.server->flags & FL_ONGROUND) || ent->fields.server->groundentity != PRVM_EDICT_TO_PROG(trace->ent))))
		return SV_Impact (ent, trace);

	return true;
}


/*
============
SV_PushMove

============
*/
void SV_PushMove (prvm_edict_t *pusher, float movetime)
{
	int i, e, index;
	int pusherowner, pusherprog;
	int checkcontents;
	qboolean rotated;
	float savesolid, movetime2, pushltime;
	vec3_t mins, maxs, move, move1, moveangle, pushorig, pushang, a, forward, left, up, org;
	int num_moved;
	int numcheckentities;
	static prvm_edict_t *checkentities[MAX_EDICTS];
	dp_model_t *pushermodel;
	trace_t trace, trace2;
	matrix4x4_t pusherfinalmatrix, pusherfinalimatrix;
	unsigned short moved_edicts[MAX_EDICTS];

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
		SV_LinkEdict(pusher);
		return;
	default:
		Con_Printf("SV_PushMove: entity #%i, unrecognized solid type %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->fields.server->solid);
		return;
	}
	index = (int) pusher->fields.server->modelindex;
	if (index < 1 || index >= MAX_MODELS)
	{
		Con_Printf("SV_PushMove: entity #%i has an invalid modelindex %f\n", PRVM_NUM_FOR_EDICT(pusher), pusher->fields.server->modelindex);
		return;
	}
	pushermodel = sv.models[index];
	pusherowner = pusher->fields.server->owner;
	pusherprog = PRVM_EDICT_TO_PROG(pusher);

	rotated = VectorLength2(pusher->fields.server->angles) + VectorLength2(pusher->fields.server->avelocity) > 0;

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
	SV_LinkEdict(pusher);

	pushermodel = NULL;
	if (pusher->fields.server->modelindex >= 1 && pusher->fields.server->modelindex < MAX_MODELS)
		pushermodel = sv.models[(int)pusher->fields.server->modelindex];
	Matrix4x4_CreateFromQuakeEntity(&pusherfinalmatrix, pusher->fields.server->origin[0], pusher->fields.server->origin[1], pusher->fields.server->origin[2], pusher->fields.server->angles[0], pusher->fields.server->angles[1], pusher->fields.server->angles[2], 1);
	Matrix4x4_Invert_Simple(&pusherfinalimatrix, &pusherfinalmatrix);

	savesolid = pusher->fields.server->solid;

// see if any solid entities are inside the final position
	num_moved = 0;

	numcheckentities = World_EntitiesInBox(&sv.world, mins, maxs, MAX_EDICTS, checkentities);
	for (e = 0;e < numcheckentities;e++)
	{
		prvm_edict_t *check = checkentities[e];
		if (check->fields.server->movetype == MOVETYPE_NONE
		 || check->fields.server->movetype == MOVETYPE_PUSH
		 || check->fields.server->movetype == MOVETYPE_FOLLOW
		 || check->fields.server->movetype == MOVETYPE_NOCLIP
		 || check->fields.server->movetype == MOVETYPE_FAKEPUSH)
			continue;

		if (check->fields.server->owner == pusherprog)
			continue;

		if (pusherowner == PRVM_EDICT_TO_PROG(check))
			continue;

		//Con_Printf("%i %s ", PRVM_NUM_FOR_EDICT(check), PRVM_GetString(check->fields.server->classname));

		// tell any MOVETYPE_STEP entity that it may need to check for water transitions
		check->priv.server->waterposition_forceupdate = true;

		checkcontents = SV_GenericHitSuperContentsMask(check);

		// if the entity is standing on the pusher, it will definitely be moved
		// if the entity is not standing on the pusher, but is in the pusher's
		// final position, move it
		if (!((int)check->fields.server->flags & FL_ONGROUND) || PRVM_PROG_TO_EDICT(check->fields.server->groundentity) != pusher)
		{
			Collision_ClipToGenericEntity(&trace, pushermodel, (int) pusher->fields.server->frame, pusher->fields.server->mins, pusher->fields.server->maxs, SUPERCONTENTS_BODY, &pusherfinalmatrix, &pusherfinalimatrix, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, checkcontents);
			//trace = SV_TraceBox(check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, MOVE_NOMONSTERS, check, checkcontents);
			if (!trace.startsolid)
			{
				//Con_Printf("- not in solid\n");
				continue;
			}
		}

		if (rotated)
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

		//Con_Printf("- pushing %f %f %f\n", move[0], move[1], move[2]);

		VectorCopy (check->fields.server->origin, check->priv.server->moved_from);
		VectorCopy (check->fields.server->angles, check->priv.server->moved_fromangles);
		moved_edicts[num_moved++] = PRVM_NUM_FOR_EDICT(check);

		// try moving the contacted entity
		pusher->fields.server->solid = SOLID_NOT;
		if(!SV_PushEntity (&trace, check, move, true, true))
		{
			// entity "check" got teleported
			check->fields.server->angles[1] += trace.fraction * moveangle[1];
			pusher->fields.server->solid = savesolid; // was SOLID_BSP
			continue; // pushed enough
		}
		// FIXME: turn players specially
		check->fields.server->angles[1] += trace.fraction * moveangle[1];
		pusher->fields.server->solid = savesolid; // was SOLID_BSP
		//Con_Printf("%s:%d frac %f startsolid %d bmodelstartsolid %d allsolid %d\n", __FILE__, __LINE__, trace.fraction, trace.startsolid, trace.bmodelstartsolid, trace.allsolid);

		// this trace.fraction < 1 check causes items to fall off of pushers
		// if they pass under or through a wall
		// the groundentity check causes items to fall off of ledges
		if (check->fields.server->movetype != MOVETYPE_WALK && (trace.fraction < 1 || PRVM_PROG_TO_EDICT(check->fields.server->groundentity) != pusher))
			check->fields.server->flags = (int)check->fields.server->flags & ~FL_ONGROUND;

		// if it is still inside the pusher, block
		Collision_ClipToGenericEntity(&trace, pushermodel, (int) pusher->fields.server->frame, pusher->fields.server->mins, pusher->fields.server->maxs, SUPERCONTENTS_BODY, &pusherfinalmatrix, &pusherfinalimatrix, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, checkcontents);
		if (trace.startsolid)
		{
			// try moving the contacted entity a tiny bit further to account for precision errors
			vec3_t move2;
			pusher->fields.server->solid = SOLID_NOT;
			VectorScale(move, 1.1, move2);
			VectorCopy (check->priv.server->moved_from, check->fields.server->origin);
			VectorCopy (check->priv.server->moved_fromangles, check->fields.server->angles);
			if(!SV_PushEntity (&trace2, check, move2, true, true))
			{
				// entity "check" got teleported
				continue;
			}
			pusher->fields.server->solid = savesolid;
			Collision_ClipToGenericEntity(&trace, pushermodel, (int) pusher->fields.server->frame, pusher->fields.server->mins, pusher->fields.server->maxs, SUPERCONTENTS_BODY, &pusherfinalmatrix, &pusherfinalimatrix, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, checkcontents);
			if (trace.startsolid)
			{
				// try moving the contacted entity a tiny bit less to account for precision errors
				pusher->fields.server->solid = SOLID_NOT;
				VectorScale(move, 0.9, move2);
				VectorCopy (check->priv.server->moved_from, check->fields.server->origin);
				VectorCopy (check->priv.server->moved_fromangles, check->fields.server->angles);
				if(!SV_PushEntity (&trace2, check, move2, true, true))
				{
					// entity "check" got teleported
					continue;
				}
				pusher->fields.server->solid = savesolid;
				Collision_ClipToGenericEntity(&trace, pushermodel, (int) pusher->fields.server->frame, pusher->fields.server->mins, pusher->fields.server->maxs, SUPERCONTENTS_BODY, &pusherfinalmatrix, &pusherfinalimatrix, check->fields.server->origin, check->fields.server->mins, check->fields.server->maxs, check->fields.server->origin, checkcontents);
				if (trace.startsolid)
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
					SV_LinkEdict(pusher);

					// move back any entities we already moved
					for (i = 0;i < num_moved;i++)
					{
						prvm_edict_t *ed = PRVM_EDICT_NUM(moved_edicts[i]);
						VectorCopy (ed->priv.server->moved_from, ed->fields.server->origin);
						VectorCopy (ed->priv.server->moved_fromangles, ed->fields.server->angles);
						SV_LinkEdict(ed);
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

static float unstickoffsets[] =
{
	// poutting -/+z changes first as they are least weird
	 0,  0,  -1,
	 0,  0,  1,
	 // x or y changes
	-1,  0,  0,
	 1,  0,  0,
	 0, -1,  0,
	 0,  1,  0,
	 // x and y changes
	-1, -1,  0,
	 1, -1,  0,
	-1,  1,  0,
	 1,  1,  0,
};

typedef enum unstickresult_e
{
	UNSTICK_STUCK = 0,
	UNSTICK_GOOD = 1,
	UNSTICK_UNSTUCK = 2
}
unstickresult_t;

unstickresult_t SV_UnstickEntityReturnOffset (prvm_edict_t *ent, vec3_t offset)
{
	int i, maxunstick;

	// if not stuck in a bmodel, just return
	if (!SV_TestEntityPosition(ent, vec3_origin))
		return UNSTICK_GOOD;

	for (i = 0;i < (int)(sizeof(unstickoffsets) / sizeof(unstickoffsets[0]));i += 3)
	{
		if (!SV_TestEntityPosition(ent, unstickoffsets + i))
		{
			VectorCopy(unstickoffsets + i, offset);
			SV_LinkEdict(ent);
			//SV_LinkEdict_TouchAreaGrid(ent);
			return UNSTICK_UNSTUCK;
		}
	}

	maxunstick = (int) ((ent->fields.server->maxs[2] - ent->fields.server->mins[2]) * 0.36);
	// magic number 0.36 allows unsticking by up to 17 units with the largest supported bbox

	for(i = 2; i <= maxunstick; ++i)
	{
		VectorClear(offset);
		offset[2] = -i;
		if (!SV_TestEntityPosition(ent, offset))
		{
			SV_LinkEdict(ent);
			//SV_LinkEdict_TouchAreaGrid(ent);
			return UNSTICK_UNSTUCK;
		}
		offset[2] = i;
		if (!SV_TestEntityPosition(ent, offset))
		{
			SV_LinkEdict(ent);
			//SV_LinkEdict_TouchAreaGrid(ent);
			return UNSTICK_UNSTUCK;
		}
	}

	return UNSTICK_STUCK;
}

qboolean SV_UnstickEntity (prvm_edict_t *ent)
{
	vec3_t offset;
	switch(SV_UnstickEntityReturnOffset(ent, offset))
	{
		case UNSTICK_GOOD:
			return true;
		case UNSTICK_UNSTUCK:
			Con_DPrintf("Unstuck entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.server->classname), offset[0], offset[1], offset[2]);
			return true;
		case UNSTICK_STUCK:
			if (developer.integer >= 100)
				Con_Printf("Stuck entity %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.server->classname));
			return false;
		default:
			Con_Printf("SV_UnstickEntityReturnOffset returned a value outside its enum.\n");
			return false;
	}
}

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck (prvm_edict_t *ent)
{
	vec3_t offset;

	switch(SV_UnstickEntityReturnOffset(ent, offset))
	{
		case UNSTICK_GOOD:
			VectorCopy (ent->fields.server->origin, ent->fields.server->oldorigin);
			break;
		case UNSTICK_UNSTUCK:
			Con_DPrintf("Unstuck player entity %i (classname \"%s\") with offset %f %f %f.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.server->classname), offset[0], offset[1], offset[2]);
			break;
		case UNSTICK_STUCK:
			VectorSubtract(ent->fields.server->oldorigin, ent->fields.server->origin, offset);
			if (!SV_TestEntityPosition(ent, offset))
			{
				Con_DPrintf("Unstuck player entity %i (classname \"%s\") by restoring oldorigin.\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.server->classname));
				SV_LinkEdict(ent);
				//SV_LinkEdict_TouchAreaGrid(ent);
			}
			else
				Con_DPrintf("Stuck player entity %i (classname \"%s\").\n", (int)PRVM_EDICT_TO_PROG(ent), PRVM_GetString(ent->fields.server->classname));
			break;
		default:
			Con_Printf("SV_UnstickEntityReturnOffset returned a value outside its enum.\n");
	}
}


/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater (prvm_edict_t *ent)
{
	int cont;
	int nNativeContents;
	vec3_t point;

	point[0] = ent->fields.server->origin[0];
	point[1] = ent->fields.server->origin[1];
	point[2] = ent->fields.server->origin[2] + ent->fields.server->mins[2] + 1;

	// DRESK - Support for Entity Contents Transition Event
	// NOTE: Some logic needed to be slightly re-ordered
	// to not affect performance and allow for the feature.

	// Acquire Super Contents Prior to Resets
	cont = SV_PointSuperContents(point);
	// Acquire Native Contents Here
	nNativeContents = Mod_Q1BSP_NativeContentsFromSuperContents(NULL, cont);

	// DRESK - Support for Entity Contents Transition Event
	if(ent->fields.server->watertype)
		// Entity did NOT Spawn; Check
		SV_CheckContentsTransition(ent, nNativeContents);


	ent->fields.server->waterlevel = 0;
	ent->fields.server->watertype = CONTENTS_EMPTY;
	cont = SV_PointSuperContents(point);
	if (cont & (SUPERCONTENTS_LIQUIDSMASK))
	{
		ent->fields.server->watertype = nNativeContents;
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

#if 0
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

		SV_PushEntity (&trace, ent, dir, false, true);

		// retry the original move
		ent->fields.server->velocity[0] = oldvel[0];
		ent->fields.server->velocity[1] = oldvel[1];
		ent->fields.server->velocity[2] = 0;
		clip = SV_FlyMove (ent, 0.1, NULL, SV_GenericHitSuperContentsMask(ent));

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
#endif

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove (prvm_edict_t *ent)
{
	int clip, oldonground, originalmove_clip, originalmove_flags, originalmove_groundentity, hitsupercontentsmask;
	vec3_t upmove, downmove, start_origin, start_velocity, stepnormal, originalmove_origin, originalmove_velocity;
	trace_t downtrace, trace;
	qboolean applygravity;

	// if frametime is 0 (due to client sending the same timestamp twice),
	// don't move
	if (sv.frametime <= 0)
		return;

	SV_CheckStuck (ent);

	applygravity = !SV_CheckWater (ent) && ent->fields.server->movetype == MOVETYPE_WALK && ! ((int)ent->fields.server->flags & FL_WATERJUMP);

	hitsupercontentsmask = SV_GenericHitSuperContentsMask(ent);

	SV_CheckVelocity(ent);

	// do a regular slide move unless it looks like you ran into a step
	oldonground = (int)ent->fields.server->flags & FL_ONGROUND;

	VectorCopy (ent->fields.server->origin, start_origin);
	VectorCopy (ent->fields.server->velocity, start_velocity);

	clip = SV_FlyMove (ent, sv.frametime, applygravity, NULL, hitsupercontentsmask);

	// if the move did not hit the ground at any point, we're not on ground
	if (!(clip & 1))
		ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;

	SV_CheckVelocity(ent);
	SV_LinkEdict(ent);
	SV_LinkEdict_TouchAreaGrid(ent);

	if(clip & 8) // teleport
		return;

	if ((int)ent->fields.server->flags & FL_WATERJUMP)
		return;

	if (sv_nostep.integer)
		return;

	VectorCopy(ent->fields.server->origin, originalmove_origin);
	VectorCopy(ent->fields.server->velocity, originalmove_velocity);
	originalmove_clip = clip;
	originalmove_flags = (int)ent->fields.server->flags;
	originalmove_groundentity = ent->fields.server->groundentity;

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
		if(!SV_PushEntity(&trace, ent, upmove, false, true))
		{
			// we got teleported when upstepping... must abort the move
			return;
		}

		// move forward
		ent->fields.server->velocity[2] = 0;
		clip = SV_FlyMove (ent, sv.frametime, applygravity, stepnormal, hitsupercontentsmask);
		ent->fields.server->velocity[2] += start_velocity[2];
		if(clip & 8)
		{
			// we got teleported when upstepping... must abort the move
			// note that z velocity handling may not be what QC expects here, but we cannot help it
			return;
		}

		SV_CheckVelocity(ent);
		SV_LinkEdict(ent);
		SV_LinkEdict_TouchAreaGrid(ent);

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
	// don't do the down move if stepdown is disabled, moving upward, not in water, or the move started offground or ended onground
	else if (!sv_gameplayfix_stepdown.integer || ent->fields.server->waterlevel >= 3 || start_velocity[2] >= (1.0 / 32.0) || !oldonground || ((int)ent->fields.server->flags & FL_ONGROUND))
		return;

	// move down
	VectorClear (downmove);
	downmove[2] = -sv_stepheight.value + start_velocity[2]*sv.frametime;
	if(!SV_PushEntity (&downtrace, ent, downmove, false, true))
	{
		// we got teleported when downstepping... must abort the move
		return;
	}

	if (downtrace.fraction < 1 && downtrace.plane.normal[2] > 0.7)
	{
		// this has been disabled so that you can't jump when you are stepping
		// up while already jumping (also known as the Quake2 double jump bug)
#if 0
		// LordHavoc: disabled this check so you can walk on monsters/players
		//if (ent->fields.server->solid == SOLID_BSP)
		{
			//Con_Printf("onground\n");
			ent->fields.server->flags =	(int)ent->fields.server->flags | FL_ONGROUND;
			ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(downtrace.ent);
		}
#endif
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
	SV_LinkEdict(ent);
	SV_LinkEdict_TouchAreaGrid(ent);
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
	SV_LinkEdict(ent);
	//SV_LinkEdict_TouchAreaGrid(ent);
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

	// DRESK - Support for Entity Contents Transition Event
	// NOTE: Call here BEFORE updating the watertype below,
	// and suppress watersplash sound if a valid function
	// call was made to allow for custom "splash" sounds.
	if( !SV_CheckContentsTransition(ent, cont) )
	{ // Contents Transition Function Invalid; Potentially Play Water Sound
		// check if the entity crossed into or out of water
		if (sv_sound_watersplash.string && ((ent->fields.server->watertype == CONTENTS_WATER || ent->fields.server->watertype == CONTENTS_SLIME) != (cont == CONTENTS_WATER || cont == CONTENTS_SLIME)))
			SV_StartSound (ent, 0, sv_sound_watersplash.string, 255, 1);
	}

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
	vec_t movetime;
	int bump;

// if onground, return without moving
	if ((int)ent->fields.server->flags & FL_ONGROUND)
	{
		if (ent->fields.server->velocity[2] >= (1.0 / 32.0) && sv_gameplayfix_upwardvelocityclearsongroundflag.integer)
		{
			// don't stick to ground if onground and moving upward
			ent->fields.server->flags -= FL_ONGROUND;
		}
		else if (!ent->fields.server->groundentity || !sv_gameplayfix_noairborncorpse.integer)
		{
			// we can trust FL_ONGROUND if groundentity is world because it never moves
			return;
		}
		else if (ent->priv.server->suspendedinairflag && PRVM_PROG_TO_EDICT(ent->fields.server->groundentity)->priv.server->free)
		{
			// if ent was supported by a brush model on previous frame,
			// and groundentity is now freed, set groundentity to 0 (world)
			// which leaves it suspended in the air
			ent->fields.server->groundentity = 0;
			if (sv_gameplayfix_noairborncorpse_allowsuspendeditems.integer)
				return;
		}
	}
	ent->priv.server->suspendedinairflag = false;

	SV_CheckVelocity (ent);

// add gravity
	if (ent->fields.server->movetype == MOVETYPE_TOSS || ent->fields.server->movetype == MOVETYPE_BOUNCE)
		ent->fields.server->velocity[2] -= SV_Gravity(ent);

// move angles
	VectorMA (ent->fields.server->angles, sv.frametime, ent->fields.server->avelocity, ent->fields.server->angles);

	movetime = sv.frametime;
	for (bump = 0;bump < MAX_CLIP_PLANES && movetime > 0;bump++)
	{
	// move origin
		VectorScale (ent->fields.server->velocity, movetime, move);
		if(!SV_PushEntity (&trace, ent, move, true, true))
			return; // teleported
		if (ent->priv.server->free)
			return;
		if (trace.bmodelstartsolid)
		{
			// try to unstick the entity
			SV_UnstickEntity(ent);
			if(!SV_PushEntity (&trace, ent, move, false, true))
				return; // teleported
			if (ent->priv.server->free)
				return;
		}
		if (trace.fraction == 1)
			break;
		movetime *= 1 - min(1, trace.fraction);
		if (ent->fields.server->movetype == MOVETYPE_BOUNCEMISSILE)
		{
			prvm_eval_t *val;
			float bouncefactor = 1.0f;
			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.bouncefactor);
			if (val!=0 && val->_float)
				bouncefactor = val->_float;

			ClipVelocity (ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 1 + bouncefactor);
			ent->fields.server->flags = (int)ent->fields.server->flags & ~FL_ONGROUND;
		}
		else if (ent->fields.server->movetype == MOVETYPE_BOUNCE)
		{
			float d, ent_gravity;
			prvm_eval_t *val;
			float bouncefactor = 0.5f;
			float bouncestop = 60.0f / 800.0f;

			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.bouncefactor);
			if (val!=0 && val->_float)
				bouncefactor = val->_float;

			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.bouncestop);
			if (val!=0 && val->_float)
				bouncestop = val->_float;

			ClipVelocity (ent->fields.server->velocity, trace.plane.normal, ent->fields.server->velocity, 1 + bouncefactor);
			// LordHavoc: fixed grenades not bouncing when fired down a slope
			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.gravity);
			if (val!=0 && val->_float)
				ent_gravity = val->_float;
			else
				ent_gravity = 1.0;
			if (sv_gameplayfix_grenadebouncedownslopes.integer)
			{
				d = DotProduct(trace.plane.normal, ent->fields.server->velocity);
				if (trace.plane.normal[2] > 0.7 && fabs(d) < sv_gravity.value * bouncestop * ent_gravity)
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
				if (trace.plane.normal[2] > 0.7 && ent->fields.server->velocity[2] < sv_gravity.value * bouncestop * ent_gravity)
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
		if (!sv_gameplayfix_slidemoveprojectiles.integer || (ent->fields.server->movetype != MOVETYPE_BOUNCE && ent->fields.server->movetype == MOVETYPE_BOUNCEMISSILE) || ((int)ent->fields.server->flags & FL_ONGROUND))
			break;
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

	// DRESK
	// Backup Velocity in the event that movetypesteplandevent is called,
	// to provide a parameter with the entity's velocity at impact.
	prvm_eval_t *movetypesteplandevent;
	vec3_t backupVelocity;
	VectorCopy(ent->fields.server->velocity, backupVelocity);
	// don't fall at all if fly/swim
	if (!(flags & (FL_FLY | FL_SWIM)))
	{
		if (flags & FL_ONGROUND)
		{
			// freefall if onground and moving upward
			// freefall if not standing on a world surface (it may be a lift or trap door)
			if ((ent->fields.server->velocity[2] >= (1.0 / 32.0) && sv_gameplayfix_upwardvelocityclearsongroundflag.integer) || ent->fields.server->groundentity)
			{
				ent->fields.server->flags -= FL_ONGROUND;
				SV_CheckVelocity(ent);
				SV_FlyMove(ent, sv.frametime, true, NULL, SV_GenericHitSuperContentsMask(ent));
				SV_LinkEdict(ent);
				SV_LinkEdict_TouchAreaGrid(ent);
				ent->priv.server->waterposition_forceupdate = true;
			}
		}
		else
		{
			// freefall if not onground
			int hitsound = ent->fields.server->velocity[2] < sv_gravity.value * -0.1;

			SV_CheckVelocity(ent);
			SV_FlyMove(ent, sv.frametime, true, NULL, SV_GenericHitSuperContentsMask(ent));
			SV_LinkEdict(ent);
			SV_LinkEdict_TouchAreaGrid(ent);

			// just hit ground
			if (hitsound && (int)ent->fields.server->flags & FL_ONGROUND)
			{
				// DRESK - Check for Entity Land Event Function
				movetypesteplandevent = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.movetypesteplandevent);

				if(movetypesteplandevent->function)
				{ // Valid Function; Execute
					// Prepare Parameters
						// Assign Velocity at Impact
						PRVM_G_VECTOR(OFS_PARM0)[0] = backupVelocity[0];
						PRVM_G_VECTOR(OFS_PARM0)[1] = backupVelocity[1];
						PRVM_G_VECTOR(OFS_PARM0)[2] = backupVelocity[2];
						// Assign Self
						prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
					// Execute VM Function
					PRVM_ExecuteProgram(movetypesteplandevent->function, "movetypesteplandevent: NULL function");
				}
				else
				// Check for Engine Landing Sound
				if(sv_sound_land.string)
					SV_StartSound(ent, 0, sv_sound_land.string, 255, 1);
			}
			ent->priv.server->waterposition_forceupdate = true;
		}
	}

// regular thinking
	if (!SV_RunThink(ent))
		return;

	if (ent->priv.server->waterposition_forceupdate || !VectorCompare(ent->fields.server->origin, ent->priv.server->waterposition_origin))
	{
		ent->priv.server->waterposition_forceupdate = false;
		VectorCopy(ent->fields.server->origin, ent->priv.server->waterposition_origin);
		SV_CheckWaterTransition(ent);
	}
}

//============================================================================

static void SV_Physics_Entity (prvm_edict_t *ent)
{
	// don't run think/move on newly spawned projectiles as it messes up
	// movement interpolation and rocket trails, and is inconsistent with
	// respect to entities spawned in the same frame
	// (if an ent spawns a higher numbered ent, it moves in the same frame,
	//  but if it spawns a lower numbered ent, it doesn't - this never moves
	//  ents in the first frame regardless)
	qboolean runmove = ent->priv.server->move;
	ent->priv.server->move = true;
	if (!runmove && sv_gameplayfix_delayprojectiles.integer > 0)
		return;
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
		SV_LinkEdict(ent);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_WALK:
		if (SV_RunThink (ent))
			SV_WalkMove (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCEMISSILE:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_FLY:
		// regular thinking
		if (SV_RunThink (ent))
			SV_Physics_Toss (ent);
		break;
	default:
		Con_Printf ("SV_Physics: bad movetype %i\n", (int)ent->fields.server->movetype);
		break;
	}
}

void SV_Physics_ClientMove(void)
{
	prvm_edict_t *ent;
	ent = host_client->edict;

	// call player physics, this needs the proper frametime
	prog->globals.server->frametime = sv.frametime;
	SV_ClientThink();

	// call standard client pre-think, with frametime = 0
	prog->globals.server->time = sv.time;
	prog->globals.server->frametime = 0;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPreThink, "QC function PlayerPreThink is missing");
	prog->globals.server->frametime = sv.frametime;

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);

	// perform MOVETYPE_WALK behavior
	SV_WalkMove (ent);

	// call standard player post-think, with frametime = 0
	prog->globals.server->time = sv.time;
	prog->globals.server->frametime = 0;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.server->PlayerPostThink, "QC function PlayerPostThink is missing");
	prog->globals.server->frametime = sv.frametime;

	if(ent->fields.server->fixangle)
	{
		// angle fixing was requested by physics code...
		// so store the current angles for later use
		memcpy(host_client->fixangle_angles, ent->fields.server->angles, sizeof(host_client->fixangle_angles));
		host_client->fixangle_angles_set = TRUE;

		// and clear fixangle for the next frame
		ent->fields.server->fixangle = 0;
	}
}

static void SV_Physics_ClientEntity_PreThink(prvm_edict_t *ent)
{
	// don't do physics on disconnected clients, FrikBot relies on this
	if (!host_client->spawned)
		return;

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);

	// don't run physics here if running asynchronously
	if (host_client->clmovement_inputtimeout <= 0)
	{
		SV_ClientThink();
		//host_client->cmd.time = max(host_client->cmd.time, sv.time);
	}

	// make sure the velocity is still sane (not a NaN)
	SV_CheckVelocity(ent);

	// call standard client pre-think
	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram(prog->globals.server->PlayerPreThink, "QC function PlayerPreThink is missing");

	// make sure the velocity is still sane (not a NaN)
	SV_CheckVelocity(ent);
}

static void SV_Physics_ClientEntity_PostThink(prvm_edict_t *ent)
{
	// don't do physics on disconnected clients, FrikBot relies on this
	if (!host_client->spawned)
		return;

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);

	// call standard player post-think
	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram(prog->globals.server->PlayerPostThink, "QC function PlayerPostThink is missing");

	// make sure the velocity is still sane (not a NaN)
	SV_CheckVelocity(ent);

	if(ent->fields.server->fixangle)
	{
		// angle fixing was requested by physics code...
		// so store the current angles for later use
		memcpy(host_client->fixangle_angles, ent->fields.server->angles, sizeof(host_client->fixangle_angles));
		host_client->fixangle_angles_set = TRUE;

		// and clear fixangle for the next frame
		ent->fields.server->fixangle = 0;
	}

	// decrement the countdown variable used to decide when to go back to
	// synchronous physics
	if (host_client->clmovement_inputtimeout > sv.frametime)
		host_client->clmovement_inputtimeout -= sv.frametime;
	else
		host_client->clmovement_inputtimeout = 0;
}

static void SV_Physics_ClientEntity(prvm_edict_t *ent)
{
	// don't do physics on disconnected clients, FrikBot relies on this
	if (!host_client->spawned)
	{
		memset(&host_client->cmd, 0, sizeof(host_client->cmd));
		return;
	}

	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(ent);

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
		SV_RunThink(ent);
		SV_CheckWater(ent);
		VectorMA(ent->fields.server->origin, sv.frametime, ent->fields.server->velocity, ent->fields.server->origin);
		VectorMA(ent->fields.server->angles, sv.frametime, ent->fields.server->avelocity, ent->fields.server->angles);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_WALK:
		SV_RunThink (ent);
		// don't run physics here if running asynchronously
		if (host_client->clmovement_inputtimeout <= 0)
			SV_WalkMove (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_BOUNCEMISSILE:
	case MOVETYPE_FLYMISSILE:
		// regular thinking
		SV_RunThink (ent);
		SV_Physics_Toss (ent);
		break;
	case MOVETYPE_FLY:
		SV_RunThink (ent);
		SV_WalkMove (ent);
		break;
	default:
		Con_Printf ("SV_Physics_ClientEntity: bad movetype %i\n", (int)ent->fields.server->movetype);
		break;
	}

	SV_CheckVelocity (ent);

	SV_LinkEdict(ent);
	SV_LinkEdict_TouchAreaGrid(ent);

	SV_CheckVelocity (ent);
}

/*
================
SV_Physics

================
*/
void SV_Physics (void)
{
	int i;
	prvm_edict_t *ent;

// let the progs know that a new frame has started
	prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.server->time = sv.time;
	prog->globals.server->frametime = sv.frametime;
	PRVM_ExecuteProgram (prog->globals.server->StartFrame, "QC function StartFrame is missing");

//
// treat each object in turn
//

	// if force_retouch, relink all the entities
	if (prog->globals.server->force_retouch > 0)
		for (i = 1, ent = PRVM_EDICT_NUM(i);i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
			if (!ent->priv.server->free)
				SV_LinkEdict_TouchAreaGrid(ent); // force retouch even for stationary

	if (sv_gameplayfix_consistentplayerprethink.integer)
	{
		// run physics on the client entities in 3 stages
		for (i = 1, ent = PRVM_EDICT_NUM(i), host_client = svs.clients;i <= svs.maxclients;i++, ent = PRVM_NEXT_EDICT(ent), host_client++)
			if (!ent->priv.server->free)
				SV_Physics_ClientEntity_PreThink(ent);

		for (i = 1, ent = PRVM_EDICT_NUM(i), host_client = svs.clients;i <= svs.maxclients;i++, ent = PRVM_NEXT_EDICT(ent), host_client++)
			if (!ent->priv.server->free)
				SV_Physics_ClientEntity(ent);

		for (i = 1, ent = PRVM_EDICT_NUM(i), host_client = svs.clients;i <= svs.maxclients;i++, ent = PRVM_NEXT_EDICT(ent), host_client++)
			if (!ent->priv.server->free)
				SV_Physics_ClientEntity_PostThink(ent);
	}
	else
	{
		// run physics on the client entities
		for (i = 1, ent = PRVM_EDICT_NUM(i), host_client = svs.clients;i <= svs.maxclients;i++, ent = PRVM_NEXT_EDICT(ent), host_client++)
		{
			if (!ent->priv.server->free)
			{
				SV_Physics_ClientEntity_PreThink(ent);
				SV_Physics_ClientEntity(ent);
				SV_Physics_ClientEntity_PostThink(ent);
			}
		}
	}

	// run physics on all the non-client entities
	if (!sv_freezenonclients.integer)
	{
		for (;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
			if (!ent->priv.server->free)
				SV_Physics_Entity(ent);
		// make a second pass to see if any ents spawned this frame and make
		// sure they run their move/think
		if (sv_gameplayfix_delayprojectiles.integer < 0)
			for (i = svs.maxclients + 1, ent = PRVM_EDICT_NUM(i);i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
				if (!ent->priv.server->move && !ent->priv.server->free)
					SV_Physics_Entity(ent);
	}

	if (prog->globals.server->force_retouch > 0)
		prog->globals.server->force_retouch = max(0, prog->globals.server->force_retouch - 1);

	// LordHavoc: endframe support
	if (prog->funcoffsets.EndFrame)
	{
		prog->globals.server->self = PRVM_EDICT_TO_PROG(prog->edicts);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
		prog->globals.server->time = sv.time;
		PRVM_ExecuteProgram (prog->funcoffsets.EndFrame, "QC function EndFrame is missing");
	}

	// decrement prog->num_edicts if the highest number entities died
	for (;PRVM_ED_CanAlloc(PRVM_EDICT_NUM(prog->num_edicts - 1));prog->num_edicts--);

	if (!sv_freezenonclients.integer)
		sv.time += sv.frametime;
}
