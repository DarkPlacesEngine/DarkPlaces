
#include "quakedef.h"

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

int cl_traceline_endcontents;

float CL_TraceLine (const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int contents, int hitbmodels)
{
	double maxfrac;
	trace_t trace;

	Mod_CheckLoaded(cl.worldmodel);
	Collision_ClipTrace(&trace, NULL, cl.worldmodel, vec3_origin, vec3_origin, vec3_origin, vec3_origin, start, vec3_origin, vec3_origin, end);

	if (impact)
		VectorCopy (trace.endpos, impact);
	if (normal)
		VectorCopy (trace.plane.normal, normal);
	cl_traceline_endcontents = trace.endcontents;
	maxfrac = trace.fraction;

	if (hitbmodels && cl_num_brushmodel_entities)
	{
		int n;
		entity_render_t *ent;
		double tracemins[3], tracemaxs[3];
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

			Collision_ClipTrace(&trace, ent, ent->model, ent->origin, ent->angles, ent->mins, ent->maxs, start, vec3_origin, vec3_origin, end);

			if (trace.allsolid || trace.startsolid || trace.fraction < maxfrac)
			{
				maxfrac = trace.fraction;
				if (impact)
					VectorCopy(trace.endpos, impact);
				if (normal)
					VectorCopy(trace.plane.normal, normal);
				cl_traceline_endcontents = trace.endcontents;
			}
		}
	}
	return maxfrac;
}

