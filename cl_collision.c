
#include "quakedef.h"
#include "cl_collision.h"

/*
// not yet used
typedef struct physentity_s
{
	// this may be a entity_t, or a edict_t, or whatever
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

int cl_traceline_startsupercontents;

float CL_TraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int hitbmodels, entity_render_t **hitent, int hitsupercontentsmask)
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
		*hitent = &cl_entities[0].render;
	Mod_CheckLoaded(cl.worldmodel);
	if (cl.worldmodel && cl.worldmodel->TraceBox)
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &trace, start, start, end, end, hitsupercontentsmask);

	if (normal)
		VectorCopy(trace.plane.normal, normal);
	cl_traceline_startsupercontents = trace.startsupercontents;
	maxfrac = trace.fraction;
	maxrealfrac = trace.realfraction;

	if (hitbmodels && cl_num_brushmodel_entities)
	{
		tracemins[0] = min(start[0], end[0]);
		tracemaxs[0] = max(start[0], end[0]);
		tracemins[1] = min(start[1], end[1]);
		tracemaxs[1] = max(start[1], end[1]);
		tracemins[2] = min(start[2], end[2]);
		tracemaxs[2] = max(start[2], end[2]);

		// look for embedded bmodels
		for (n = 0;n < cl_num_brushmodel_entities;n++)
		{
			ent = &cl_entities[cl_brushmodel_entities[n]].render;
			if (!BoxesOverlap(tracemins, tracemaxs, ent->mins, ent->maxs))
				continue;

			Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
			Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);

			if (ent->model && ent->model->TraceBox)
				ent->model->TraceBox(ent->model, 0, &trace, starttransformed, starttransformed, endtransformed, endtransformed, hitsupercontentsmask);

			cl_traceline_startsupercontents |= trace.startsupercontents;
			if (maxrealfrac > trace.realfraction)
			{
				if (hitent)
					*hitent = ent;
				maxfrac = trace.fraction;
				maxrealfrac = trace.realfraction;
				if (normal)
				{
					VectorCopy(trace.plane.normal, tempnormal);
					Matrix4x4_Transform3x3(&ent->matrix, tempnormal, normal);
				}
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
	Mod_CheckLoaded(cl.worldmodel);
	if (cl.worldmodel && cl.worldmodel->TraceBox)
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &trace, start, start, end, end, SUPERCONTENTS_SOLID);

	if (normal)
		VectorCopy(trace.plane.normal, normal);
	cl_traceline_startsupercontents = trace.startsupercontents;
	maxfrac = trace.fraction;
	maxrealfrac = trace.realfraction;

	tracemins[0] = min(start[0], end[0]);
	tracemaxs[0] = max(start[0], end[0]);
	tracemins[1] = min(start[1], end[1]);
	tracemaxs[1] = max(start[1], end[1]);
	tracemins[2] = min(start[2], end[2]);
	tracemaxs[2] = max(start[2], end[2]);

	// look for embedded bmodels
	for (n = 0;n < cl_num_entities;n++)
	{
		if (!cl_entities_active[n])
			continue;
		ent = &cl_entities[n].render;
		if (!BoxesOverlap(ent->mins, ent->maxs, tracemins, tracemaxs))
			continue;
		if (!ent->model || !ent->model->TraceBox)
			continue;
		// if transparent and not selectable, skip entity
		if (!(cl_entities[n].state_current.effects & EF_SELECTABLE) && (ent->alpha < 1 || (ent->effects & (EF_ADDITIVE | EF_NODEPTHTEST))))
			continue;
		if (ent == ignoreent)
			continue;
		Matrix4x4_Transform(&ent->inversematrix, start, starttransformed);
		Matrix4x4_Transform(&ent->inversematrix, end, endtransformed);

		if (ent->model && ent->model->TraceBox)
			ent->model->TraceBox(ent->model, ent->frameblend[0].frame, &trace, starttransformed, starttransformed, endtransformed, endtransformed, SUPERCONTENTS_SOLID);

		cl_traceline_startsupercontents |= trace.startsupercontents;
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
	CL_TraceLine(p, p, NULL, NULL, true, NULL, 0);
	return Mod_Q1BSP_NativeContentsFromSuperContents(NULL, cl_traceline_startsupercontents);
	/*
	// FIXME: check multiple brush models
	if (cl.worldmodel && cl.worldmodel->brush.PointContentsQ1)
		return cl.worldmodel->brush.PointContentsQ1(cl.worldmodel, p);
	return 0;
	*/
}

int CL_PointSuperContents(const vec3_t p)
{
	CL_TraceLine(p, p, NULL, NULL, true, NULL, 0);
	return cl_traceline_startsupercontents;
	/*
	// FIXME: check multiple brush models
	if (cl.worldmodel && cl.worldmodel->brush.PointContentsQ1)
		return cl.worldmodel->brush.PointContentsQ1(cl.worldmodel, p);
	return 0;
	*/
}

