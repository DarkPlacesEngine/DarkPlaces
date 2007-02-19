
#include "quakedef.h"
#include "cl_collision.h"

float CL_SelectTraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int *hitent, entity_render_t *ignoreent)
{
	float maxfrac, maxrealfrac;
	int n;
	entity_render_t *ent;
	float tracemins[3], tracemaxs[3];
	trace_t trace;
	float tempnormal[3], starttransformed[3], endtransformed[3];

	memset (&trace, 0 , sizeof(trace_t));
	trace.fraction = 1;
	trace.realfraction = 1;
	VectorCopy (end, trace.endpos);

	if (hitent)
		*hitent = 0;
	if (cl.worldmodel && cl.worldmodel->TraceBox)
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &trace, start, vec3_origin, vec3_origin, end, SUPERCONTENTS_SOLID);

	if (normal)
		VectorCopy(trace.plane.normal, normal);
	maxfrac = trace.fraction;
	maxrealfrac = trace.realfraction;

	tracemins[0] = min(start[0], end[0]);
	tracemaxs[0] = max(start[0], end[0]);
	tracemins[1] = min(start[1], end[1]);
	tracemaxs[1] = max(start[1], end[1]);
	tracemins[2] = min(start[2], end[2]);
	tracemaxs[2] = max(start[2], end[2]);

	// look for embedded bmodels
	for (n = 0;n < cl.num_entities;n++)
	{
		if (!cl.entities_active[n])
			continue;
		ent = &cl.entities[n].render;
		if (!BoxesOverlap(ent->mins, ent->maxs, tracemins, tracemaxs))
			continue;
		if (!ent->model || !ent->model->TraceBox)
			continue;
		if ((ent->flags & RENDER_EXTERIORMODEL) && !chase_active.integer)
			continue;
		// if transparent and not selectable, skip entity
		if (!(cl.entities[n].state_current.effects & EF_SELECTABLE) && (ent->alpha < 1 || (ent->effects & (EF_ADDITIVE | EF_NODEPTHTEST))))
			continue;
		if (ent == ignoreent)
			continue;
		Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
		Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);

		//if (ent->model && ent->model->TraceBox)
			ent->model->TraceBox(ent->model, ent->frameblend[0].frame, &trace, starttransformed, vec3_origin, vec3_origin, endtransformed, SUPERCONTENTS_SOLID);

		if (maxrealfrac > trace.realfraction)
		{
			if (hitent)
				*hitent = n;
			maxfrac = trace.fraction;
			maxrealfrac = trace.realfraction;
			if (normal)
			{
				VectorCopy(trace.plane.normal, tempnormal);
				Matrix4x4_Transform3x3(&ent->matrix, tempnormal, normal);
			}
		}
	}
	maxfrac = bound(0, maxfrac, 1);
	maxrealfrac = bound(0, maxrealfrac, 1);
	//if (maxfrac < 0 || maxfrac > 1) Con_Printf("fraction out of bounds %f %s:%d\n", maxfrac, __FILE__, __LINE__);
	if (impact)
		VectorLerp(start, maxfrac, end, impact);
	return maxfrac;
}

void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius)
{
	// FIXME: check multiple brush models
	if (cl.worldmodel && cl.worldmodel->brush.FindNonSolidLocation)
		cl.worldmodel->brush.FindNonSolidLocation(cl.worldmodel, in, out, radius);
}

model_t *CL_GetModelByIndex(int modelindex)
{
	if(!modelindex)
		return NULL;
	if (modelindex < 0)
	{
		modelindex = -(modelindex+1);
		if (modelindex < MAX_MODELS)
			return cl.csqc_model_precache[modelindex];
	}
	else
	{
		if(modelindex < MAX_MODELS)
			return cl.model_precache[modelindex];
	}
	return NULL;
}

model_t *CL_GetModelFromEdict(prvm_edict_t *ed)
{
	if (!ed || ed->priv.server->free)
		return NULL;
	return CL_GetModelByIndex((int)ed->fields.client->modelindex);
}

void CL_LinkEdict(prvm_edict_t *ent)
{
	if (ent == prog->edicts)
		return;		// don't add the world

	if (ent->priv.server->free)
		return;

	VectorAdd(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->absmin);
	VectorAdd(ent->fields.client->origin, ent->fields.client->maxs, ent->fields.client->absmax);

	World_LinkEdict(&cl.world, ent, ent->fields.client->absmin, ent->fields.client->absmax);
}

int CL_GenericHitSuperContentsMask(const prvm_edict_t *passedict)
{
	prvm_eval_t *val;
	if (passedict)
	{
		val = PRVM_EDICTFIELDVALUE(passedict, prog->fieldoffsets.dphitcontentsmask);
		if (val && val->_float)
			return (int)val->_float;
		else if (passedict->fields.client->solid == SOLID_SLIDEBOX)
		{
			if ((int)passedict->fields.client->flags & FL_MONSTER)
				return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_MONSTERCLIP;
			else
				return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_PLAYERCLIP;
		}
		else if (passedict->fields.client->solid == SOLID_CORPSE)
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY;
		else if (passedict->fields.client->solid == SOLID_TRIGGER)
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY;
		else
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_CORPSE;
	}
	else
		return SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_CORPSE;
}

/*
==================
CL_Move
==================
*/
extern cvar_t sv_debugmove;
trace_t CL_Move(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask, qboolean hitnetworkbrushmodels, qboolean hitnetworkplayers, int *hitnetworkentity, qboolean hitcsqcentities)
{
	vec3_t hullmins, hullmaxs;
	int i, bodysupercontents;
	int passedictprog;
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
	model_t *model;
	// list of entities to test for collisions
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];

	if (hitnetworkentity)
		*hitnetworkentity = 0;

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
	Collision_ClipToWorld(&cliptrace, cl.worldmodel, clipstart, clipmins, clipmaxs, clipend, hitsupercontentsmask);
	cliptrace.bmodelstartsolid = cliptrace.startsolid;
	if (cliptrace.startsolid || cliptrace.fraction < 1)
		cliptrace.ent = prog ? prog->edicts : NULL;
	if (type == MOVE_WORLDONLY)
		return cliptrace;

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
	if (cl.worldmodel && cl.worldmodel->brush.RoundUpToHullSize)
		cl.worldmodel->brush.RoundUpToHullSize(cl.worldmodel, clipmins, clipmaxs, hullmins, hullmaxs);
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
	// this checks prog because this function is often called without a CSQC
	// VM context
	if (prog == NULL || passedict == prog->edicts)
		passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = prog != NULL ? PRVM_EDICT_TO_PROG(passedict) : 0;
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare(clipmins, clipmaxs);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->fields.client->owner) : 0;

	// collide against network entities
	if (hitnetworkbrushmodels)
	{
		for (i = 0;i < cl.num_brushmodel_entities;i++)
		{
			entity_render_t *ent = &cl.entities[cl.brushmodel_entities[i]].render;
			if (!BoxesOverlap(clipboxmins, clipboxmaxs, ent->mins, ent->maxs))
				continue;
			Collision_ClipToGenericEntity(&trace, ent->model, ent->frame, vec3_origin, vec3_origin, 0, &ent->matrix, &ent->inversematrix, start, mins, maxs, end, hitsupercontentsmask);
			if (cliptrace.realfraction > trace.realfraction && hitnetworkentity)
				*hitnetworkentity = cl.brushmodel_entities[i];
			Collision_CombineTraces(&cliptrace, &trace, NULL, true);
		}
	}

	// collide against player entities
	if (hitnetworkplayers)
	{
		vec3_t origin, entmins, entmaxs;
		matrix4x4_t entmatrix, entinversematrix;
		for (i = 1;i < cl.maxclients+1;i++)
		{
			entity_render_t *ent = &cl.entities[i].render;
			// don't hit ourselves
			if (i == cl.playerentity)
				continue;
			Matrix4x4_OriginFromMatrix(&ent->matrix, origin);
			VectorAdd(origin, cl.playerstandmins, entmins);
			VectorAdd(origin, cl.playerstandmaxs, entmaxs);
			if (!BoxesOverlap(clipboxmins, clipboxmaxs, entmins, entmaxs))
				continue;
			Matrix4x4_CreateTranslate(&entmatrix, origin[0], origin[1], origin[2]);
			Matrix4x4_CreateTranslate(&entinversematrix, -origin[0], -origin[1], -origin[2]);
			Collision_ClipToGenericEntity(&trace, NULL, 0, cl.playerstandmins, cl.playerstandmaxs, SUPERCONTENTS_BODY, &entmatrix, &entinversematrix, start, mins, maxs, end, hitsupercontentsmask);
			if (cliptrace.realfraction > trace.realfraction && hitnetworkentity)
				*hitnetworkentity = i;
			Collision_CombineTraces(&cliptrace, &trace, NULL, false);
		}
	}

	// clip to entities
	// because this uses World_EntitiestoBox, we know all entity boxes overlap
	// the clip region, so we can skip culling checks in the loop below
	// note: if prog is NULL then there won't be any linked entities
	numtouchedicts = 0;
	if (hitcsqcentities && prog != NULL)
	{
		numtouchedicts = World_EntitiesInBox(&cl.world, clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
		if (numtouchedicts > MAX_EDICTS)
		{
			// this never happens
			Con_Printf("CL_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
			numtouchedicts = MAX_EDICTS;
		}
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];

		if (touch->fields.client->solid < SOLID_BBOX)
			continue;
		if (type == MOVE_NOMONSTERS && touch->fields.client->solid != SOLID_BSP)
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
			if (passedictprog == touch->fields.client->owner)
				continue;
			// don't clip points against points (they can't collide)
			if (pointtrace && VectorCompare(touch->fields.client->mins, touch->fields.client->maxs) && (type != MOVE_MISSILE || !((int)touch->fields.client->flags & FL_MONSTER)))
				continue;
		}

		bodysupercontents = touch->fields.client->solid == SOLID_CORPSE ? SUPERCONTENTS_CORPSE : SUPERCONTENTS_BODY;

		// might interact, so do an exact clip
		model = NULL;
		if ((int) touch->fields.client->solid == SOLID_BSP || type == MOVE_HITMODEL)
		{
			unsigned int modelindex = (unsigned int)touch->fields.client->modelindex;
			// if the modelindex is 0, it shouldn't be SOLID_BSP!
			if (modelindex > 0 && modelindex < MAX_MODELS)
				model = sv.models[(int)touch->fields.client->modelindex];
		}
		if (model)
			Matrix4x4_CreateFromQuakeEntity(&matrix, touch->fields.client->origin[0], touch->fields.client->origin[1], touch->fields.client->origin[2], touch->fields.client->angles[0], touch->fields.client->angles[1], touch->fields.client->angles[2], 1);
		else
			Matrix4x4_CreateTranslate(&matrix, touch->fields.client->origin[0], touch->fields.client->origin[1], touch->fields.client->origin[2]);
		Matrix4x4_Invert_Simple(&imatrix, &matrix);
		if ((int)touch->fields.client->flags & FL_MONSTER)
			Collision_ClipToGenericEntity(&trace, model, touch->fields.client->frame, touch->fields.client->mins, touch->fields.client->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins2, clipmaxs2, clipend, hitsupercontentsmask);
		else
			Collision_ClipToGenericEntity(&trace, model, touch->fields.client->frame, touch->fields.client->mins, touch->fields.client->maxs, bodysupercontents, &matrix, &imatrix, clipstart, clipmins, clipmaxs, clipend, hitsupercontentsmask);

		if (cliptrace.realfraction > trace.realfraction && hitnetworkentity)
			*hitnetworkentity = 0;
		Collision_CombineTraces(&cliptrace, &trace, (void *)touch, touch->fields.client->solid == SOLID_BSP);
	}

	return cliptrace;
}
