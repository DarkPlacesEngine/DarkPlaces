
#include "quakedef.h"
#include "cl_collision.h"

/*
// not yet used
typedef struct physentity_s
{
	// this may be a entity_t, or a prvm_edict_t, or whatever
	void *realentity;

	// can be NULL if it is a bbox object
	model_t *bmodel;

	// node this entity crosses
	// for avoiding unnecessary collisions
	physnode_t *node;

	// matrix for converting from model to world coordinates
	double modeltoworldmatrix[3][4];

	// matrix for converting from world to model coordinates
	double worldtomodelmatrix[3][4];

	// if this is a bmodel, this is used for culling it quickly
	// if this is not a bmodel, this is used for actual collisions
	double mins[3], maxs[3];
}
physentity_t;
*/

trace_t CL_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitbmodels, int *hitent, int hitsupercontentsmask, qboolean hitplayers)
{
	int n;
	entity_render_t *ent;
	vec3_t tracemins, tracemaxs;
	trace_t cliptrace, trace;
	vec3_t origin;
	vec3_t starttransformed, endtransformed;
	vec3_t entmins, entmaxs;
	vec_t *playermins, *playermaxs;

	memset (&cliptrace, 0 , sizeof(trace_t));
	cliptrace.fraction = 1;
	cliptrace.realfraction = 1;

	if (cl.worldmodel && cl.worldmodel->TraceBox)
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &cliptrace, start, mins, maxs, end, hitsupercontentsmask);

	if (hitent)
		*hitent = 0;

	if (hitbmodels && cl.num_brushmodel_entities)
	{
		tracemins[0] = min(start[0], end[0]) + mins[0];
		tracemaxs[0] = max(start[0], end[0]) + maxs[0];
		tracemins[1] = min(start[1], end[1]) + mins[1];
		tracemaxs[1] = max(start[1], end[1]) + maxs[1];
		tracemins[2] = min(start[2], end[2]) + mins[2];
		tracemaxs[2] = max(start[2], end[2]) + maxs[2];

		// look for embedded bmodels
		for (n = 0;n < cl.num_brushmodel_entities;n++)
		{
			ent = &cl.entities[cl.brushmodel_entities[n]].render;
			if (!BoxesOverlap(tracemins, tracemaxs, ent->mins, ent->maxs))
				continue;

			Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
			Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);

			memset (&trace, 0 , sizeof(trace_t));
			trace.fraction = 1;
			trace.realfraction = 1;

			if (ent->model && ent->model->TraceBox)
				ent->model->TraceBox(ent->model, 0, &trace, start, mins, maxs, endtransformed, hitsupercontentsmask);

			// LordHavoc: take the 'best' answers from the new trace and combine with existing data
			if (trace.allsolid)
				cliptrace.allsolid = true;
			if (trace.startsolid)
			{
				cliptrace.startsolid = true;
				if (cliptrace.realfraction == 1)
					if (hitent)
						*hitent = cl.brushmodel_entities[n];
			}
			// don't set this except on the world, because it can easily confuse
			// monsters underwater if there's a bmodel involved in the trace
			// (inopen && inwater is how they check water visibility)
			//if (trace.inopen)
			//	cliptrace.inopen = true;
			if (trace.inwater)
				cliptrace.inwater = true;
			if (trace.realfraction < cliptrace.realfraction)
			{
				cliptrace.fraction = trace.fraction;
				cliptrace.realfraction = trace.realfraction;
				cliptrace.plane = trace.plane;
				if (hitent)
					*hitent = cl.brushmodel_entities[n];
				Matrix4x4_Transform3x3(&ent->matrix, trace.plane.normal, cliptrace.plane.normal);
				cliptrace.hitsupercontents = trace.hitsupercontents;
				cliptrace.hitq3surfaceflags = trace.hitq3surfaceflags;
				cliptrace.hittexture = trace.hittexture;
			}
			cliptrace.startsupercontents |= trace.startsupercontents;
		}
	}
	if (hitplayers)
	{
		tracemins[0] = min(start[0], end[0]) + mins[0];
		tracemaxs[0] = max(start[0], end[0]) + maxs[0];
		tracemins[1] = min(start[1], end[1]) + mins[1];
		tracemaxs[1] = max(start[1], end[1]) + maxs[1];
		tracemins[2] = min(start[2], end[2]) + mins[2];
		tracemaxs[2] = max(start[2], end[2]) + maxs[2];

		for (n = 1;n < cl.maxclients+1;n++)
		{
			if (n != cl.playerentity)
			{
				ent = &cl.entities[n].render;
				// FIXME: crouch
				playermins = cl.playerstandmins;
				playermaxs = cl.playerstandmaxs;
				Matrix4x4_OriginFromMatrix(&ent->matrix, origin);
				VectorAdd(origin, playermins, entmins);
				VectorAdd(origin, playermaxs, entmaxs);
				if (!BoxesOverlap(tracemins, tracemaxs, entmins, entmaxs))
					continue;

				memset (&trace, 0 , sizeof(trace_t));
				trace.fraction = 1;
				trace.realfraction = 1;

				Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
				Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);
				Collision_ClipTrace_Box(&trace, playermins, playermaxs, starttransformed, mins, maxs, endtransformed, hitsupercontentsmask, SUPERCONTENTS_SOLID);

				// LordHavoc: take the 'best' answers from the new trace and combine with existing data
				if (trace.allsolid)
					cliptrace.allsolid = true;
				if (trace.startsolid)
				{
					cliptrace.startsolid = true;
					if (cliptrace.realfraction == 1)
						if (hitent)
							*hitent = n;
				}
				// don't set this except on the world, because it can easily confuse
				// monsters underwater if there's a bmodel involved in the trace
				// (inopen && inwater is how they check water visibility)
				//if (trace.inopen)
				//	cliptrace.inopen = true;
				if (trace.inwater)
					cliptrace.inwater = true;
				if (trace.realfraction < cliptrace.realfraction)
				{
					cliptrace.fraction = trace.fraction;
					cliptrace.realfraction = trace.realfraction;
					cliptrace.plane = trace.plane;
					if (hitent)
						*hitent = n;
					Matrix4x4_Transform3x3(&ent->matrix, trace.plane.normal, cliptrace.plane.normal);
					cliptrace.hitsupercontents = trace.hitsupercontents;
					cliptrace.hitq3surfaceflags = trace.hitq3surfaceflags;
					cliptrace.hittexture = trace.hittexture;
				}
				cliptrace.startsupercontents |= trace.startsupercontents;
			}
		}
	}
	cliptrace.fraction = bound(0, cliptrace.fraction, 1);
	cliptrace.realfraction = bound(0, cliptrace.realfraction, 1);
	VectorLerp(start, cliptrace.fraction, end, cliptrace.endpos);
	return cliptrace;
}

float CL_SelectTraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int *hitent, entity_render_t *ignoreent, qboolean csqcents)
{
	float maxfrac, maxrealfrac;
	int n, entsnum;
	entity_t *entlist;
	unsigned char *entactivelist;
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
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &trace, start, start, end, end, SUPERCONTENTS_SOLID);

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

	if(csqcents)
	{
		entlist = cl.csqcentities;
		entactivelist = cl.csqcentities_active;
		entsnum = cl.num_csqcentities;
	}
	else
	{
		entlist = cl.entities;
		entactivelist = cl.entities_active;
		entsnum = cl.num_entities;
	}

	// look for embedded bmodels
	for (n = 0;n < entsnum;n++)
	{
		if (!entactivelist[n])
			continue;
		ent = &entlist[n].render;
		if (!BoxesOverlap(ent->mins, ent->maxs, tracemins, tracemaxs))
			continue;
		if (!ent->model || !ent->model->TraceBox)
			continue;
		if ((ent->flags & RENDER_EXTERIORMODEL) && !chase_active.integer)
			continue;
		// if transparent and not selectable, skip entity
		if (!(entlist[n].state_current.effects & EF_SELECTABLE) && (ent->alpha < 1 || (ent->effects & (EF_ADDITIVE | EF_NODEPTHTEST))))
			continue;
		if (ent == ignoreent)
			continue;
		Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
		Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);

		if (ent->model && ent->model->TraceBox)
			ent->model->TraceBox(ent->model, ent->frameblend[0].frame, &trace, starttransformed, starttransformed, endtransformed, endtransformed, SUPERCONTENTS_SOLID);

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

int CL_PointQ1Contents(const vec3_t p)
{
	return Mod_Q1BSP_NativeContentsFromSuperContents(NULL, CL_TraceBox(p, vec3_origin, vec3_origin, p, true, NULL, 0, false).startsupercontents);
	/*
	// FIXME: check multiple brush models
	if (cl.worldmodel && cl.worldmodel->brush.PointContentsQ1)
		return cl.worldmodel->brush.PointContentsQ1(cl.worldmodel, p);
	return 0;
	*/
}

int CL_PointSuperContents(const vec3_t p)
{
	return CL_TraceBox(p, vec3_origin, vec3_origin, p, true, NULL, 0, false).startsupercontents;
	/*
	// FIXME: check multiple brush models
	if (cl.worldmodel && cl.worldmodel->brush.PointContentsQ1)
		return cl.worldmodel->brush.PointContentsQ1(cl.worldmodel, p);
	return 0;
	*/
}

