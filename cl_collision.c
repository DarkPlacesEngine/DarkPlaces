
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
	float maxfrac;
	int n;
	entity_render_t *ent;
	float tracemins[3], tracemaxs[3];
	trace_t trace;
	matrix4x4_t matrix, imatrix;
	float tempnormal[3], starttransformed[3], endtransformed[3];

	if (hitent)
		*hitent = &cl_entities[0].render;
	Mod_CheckLoaded(cl.worldmodel);
	if (cl.worldmodel && cl.worldmodel->TraceBox)
		cl.worldmodel->TraceBox(cl.worldmodel, 0, &trace, start, start, end, end, hitsupercontentsmask);

	if (impact)
		VectorLerp(start, trace.fraction, end, impact);
	if (normal)
		VectorCopy(trace.plane.normal, normal);
	cl_traceline_startsupercontents = trace.startsupercontents;
	maxfrac = trace.fraction;

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
			ent = cl_brushmodel_entities[n];
			if (ent->mins[0] > tracemaxs[0] || ent->maxs[0] < tracemins[0]
			 || ent->mins[1] > tracemaxs[1] || ent->maxs[1] < tracemins[1]
			 || ent->mins[2] > tracemaxs[2] || ent->maxs[2] < tracemins[2])
				continue;

			Matrix4x4_CreateFromQuakeEntity(&matrix, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2], 1);
			Matrix4x4_Invert_Simple(&imatrix, &matrix);
			Matrix4x4_Transform(&imatrix, start, starttransformed);
			Matrix4x4_Transform(&imatrix, end, endtransformed);

			if (ent->model && ent->model->TraceBox)
				ent->model->TraceBox(ent->model, 0, &trace, starttransformed, starttransformed, endtransformed, endtransformed, hitsupercontentsmask);

			cl_traceline_startsupercontents |= trace.startsupercontents;
			if (maxfrac > trace.fraction)
			{
				if (hitent)
					*hitent = ent;
				maxfrac = trace.fraction;
				if (impact)
					VectorLerp(start, trace.fraction, end, impact);
				if (normal)
				{
					VectorCopy(trace.plane.normal, tempnormal);
					Matrix4x4_Transform3x3(&matrix, tempnormal, normal);
				}
			}
		}
	}
	if (maxfrac < 0 || maxfrac > 1) Con_Printf("fraction out of bounds %f %s:%d\n", maxfrac, __FILE__, __LINE__);
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

