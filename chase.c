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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t chase_back = {CVAR_SAVE, "chase_back", "48"};
cvar_t chase_up = {CVAR_SAVE, "chase_up", "48"};
cvar_t chase_active = {CVAR_SAVE, "chase_active", "0"};

void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_active);
}

void Chase_Reset (void)
{
	// for respawning and teleporting
//	start position 12 units behind head
}

int traceline_endcontents;

static entity_render_t *traceline_entity[MAX_EDICTS];
static int traceline_entities;

// builds list of entities for TraceLine to check later
void TraceLine_ScanForBModels(void)
{
	int i;
	entity_render_t *ent;
	model_t *model;
	traceline_entities = 0;
	for (i = 1;i < MAX_EDICTS;i++)
	{
		ent = &cl_entities[i].render;
		model = ent->model;
		// look for embedded brush models only
		if (model && model->name[0] == '*')
		{
			// this does nothing for * models currently...
			//Mod_CheckLoaded(model);
			if (model->type == mod_brush)
			{
				traceline_entity[traceline_entities++] = ent;
				if (ent->angles[0] || ent->angles[2])
				{
					// pitch or roll
					VectorAdd(ent->origin, model->rotatedmins, ent->mins);
					VectorAdd(ent->origin, model->rotatedmaxs, ent->maxs);
				}
				else if (ent->angles[1])
				{
					// yaw
					VectorAdd(ent->origin, model->yawmins, ent->mins);
					VectorAdd(ent->origin, model->yawmaxs, ent->maxs);
				}
				else
				{
					VectorAdd(ent->origin, model->normalmins, ent->mins);
					VectorAdd(ent->origin, model->normalmaxs, ent->maxs);
				}
			}
		}
	}
}

float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int contents, int hitbmodels)
{
	double maxfrac, startd[3], endd[3];
	trace_t trace;

// FIXME: broken, fix it
//	if (impact == NULL && normal == NULL && contents == 0)
//		return SV_TestLine (cl.worldmodel->hulls, 0, start, end);

	VectorCopy(start, startd);
	VectorCopy(end, endd);

	Mod_CheckLoaded(cl.worldmodel);
	memset (&trace, 0, sizeof(trace));
	VectorCopy (endd, trace.endpos);
	trace.fraction = 1;
	trace.startcontents = contents;
	VectorCopy(startd, RecursiveHullCheckInfo.start);
	VectorSubtract(endd, startd, RecursiveHullCheckInfo.dist);
	RecursiveHullCheckInfo.hull = cl.worldmodel->hulls;
	RecursiveHullCheckInfo.trace = &trace;
	SV_RecursiveHullCheck (0, 0, 1, startd, endd);
	if (impact)
		VectorCopy (trace.endpos, impact);
	if (normal)
		VectorCopy (trace.plane.normal, normal);
	traceline_endcontents = trace.endcontents;
	maxfrac = trace.fraction;

	if (hitbmodels && traceline_entities)
	{
		int n;
		entity_render_t *ent;
		double start2[3], end2[3], tracemins[3], tracemaxs[3];
		tracemins[0] = min(start[0], end[0]);
		tracemaxs[0] = max(start[0], end[0]);
		tracemins[1] = min(start[1], end[1]);
		tracemaxs[1] = max(start[1], end[1]);
		tracemins[2] = min(start[2], end[2]);
		tracemaxs[2] = max(start[2], end[2]);

		// look for embedded bmodels
		for (n = 0;n < traceline_entities;n++)
		{
			ent = traceline_entity[n];
			if (ent->mins[0] > tracemaxs[0] || ent->maxs[0] < tracemins[0]
			 || ent->mins[1] > tracemaxs[1] || ent->maxs[1] < tracemins[1]
			 || ent->mins[2] > tracemaxs[2] || ent->maxs[2] < tracemins[2])
			 	continue;

			softwaretransformforentity(ent);
			softwareuntransform(start, start2);
			softwareuntransform(end, end2);

			memset (&trace, 0, sizeof(trace));
			VectorCopy (end2, trace.endpos);
			trace.fraction = 1;
			trace.startcontents = contents;
			VectorCopy(start2, RecursiveHullCheckInfo.start);
			VectorSubtract(end2, start2, RecursiveHullCheckInfo.dist);
			RecursiveHullCheckInfo.hull = ent->model->hulls;
			RecursiveHullCheckInfo.trace = &trace;
			SV_RecursiveHullCheck (ent->model->hulls->firstclipnode, 0, 1, start2, end2);

			if (trace.allsolid || trace.startsolid || trace.fraction < maxfrac)
			{
				maxfrac = trace.fraction;
				if (impact)
				{
					softwaretransform(trace.endpos, impact);
				}
				if (normal)
				{
					softwaretransformdirection(trace.plane.normal, normal);
				}
				traceline_endcontents = trace.endcontents;
			}
		}
	}
	return maxfrac;
}

void Chase_Update (void)
{
	vec3_t	forward, stop, chase_dest, normal;
	float	dist;

	chase_back.value = bound(0, chase_back.value, 128);
	chase_up.value = bound(-48, chase_up.value, 96);

	AngleVectors (cl.viewangles, forward, NULL, NULL);

	dist = -chase_back.value - 8;
	chase_dest[0] = r_refdef.vieworg[0] + forward[0] * dist;
	chase_dest[1] = r_refdef.vieworg[1] + forward[1] * dist;
	chase_dest[2] = r_refdef.vieworg[2] + forward[2] * dist + chase_up.value;

	TraceLine (r_refdef.vieworg, chase_dest, stop, normal, 0, true);
	chase_dest[0] = stop[0] + forward[0] * 8 + normal[0] * 4;
	chase_dest[1] = stop[1] + forward[1] * 8 + normal[1] * 4;
	chase_dest[2] = stop[2] + forward[2] * 8 + normal[2] * 4;

	VectorCopy (chase_dest, r_refdef.vieworg);
}

