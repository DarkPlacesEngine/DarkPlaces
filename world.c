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
// world.c -- world query functions

#include "quakedef.h"

/*

entities never clip against themselves, or their owner

line of sight checks trace->inopen and trace->inwater, but bullets don't

*/

cvar_t sv_useareanodes = {CVAR_NOTIFY, "sv_useareanodes", "1"};
cvar_t sv_polygoncollisions = {CVAR_NOTIFY, "sv_polygoncollisions", "0"};

void SV_World_Init(void)
{
	Cvar_RegisterVariable(&sv_useareanodes);
	Cvar_RegisterVariable(&sv_polygoncollisions);
	Collision_Init();
}

typedef struct
{
	// bounding box of entire move area
	vec3_t boxmins, boxmaxs;

	// size of the moving object
	vec3_t mins, maxs;

	// size when clipping against monsters
	vec3_t mins2, maxs2;

	// size when clipping against brush models
	vec3_t hullmins, hullmaxs;

	// start and end origin of move
	vec3_t start, end;

	// trace results
	trace_t trace;

	// type of move (like ignoring monsters, or similar)
	int type;

	// the edict that is moving (if any)
	edict_t *passedict;
}
moveclip_t;

//#define EDICT_FROM_AREA(l) ((edict_t *)((qbyte *)l - (int)&(((edict_t *)0)->area)))
#define EDICT_FROM_AREA(l) ((edict_t *)l->entity)

//============================================================================

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->entity = NULL;
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before, void *ent)
{
	l->entity = ent;
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}


/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s
{
	// -1 = leaf node
	int axis;
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
}
areanode_t;

#define AREA_DEPTH 1
#define AREA_NODES (1 << (AREA_DEPTH + 1))

static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t *anode;
	vec3_t size, mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

typedef struct areagrid_s
{
	link_t trigger_edicts;
	link_t solid_edicts;
}
areagrid_t;

#define AREA_GRID 16
#define AREA_GRIDNODES (AREA_GRID * AREA_GRID)

static areagrid_t sv_areagrid[AREA_GRIDNODES];
static vec3_t sv_areagridbias, sv_areagridscale;

void SV_CreateAreaGrid (vec3_t mins, vec3_t maxs)
{
	int i;
	VectorNegate(mins, sv_areagridbias);
	sv_areagridscale[0] = AREA_GRID / (maxs[0] + sv_areagridbias[0]);
	sv_areagridscale[1] = AREA_GRID / (maxs[1] + sv_areagridbias[1]);
	sv_areagridscale[2] = AREA_GRID / (maxs[2] + sv_areagridbias[2]);
	for (i = 0;i < AREA_GRIDNODES;i++)
	{
		ClearLink (&sv_areagrid[i].trigger_edicts);
		ClearLink (&sv_areagrid[i].solid_edicts);
	}
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	memset(sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	Mod_CheckLoaded(sv.worldmodel);
	SV_CreateAreaNode(0, sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
	SV_CreateAreaGrid(sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	int i;
	for (i = 0;i < ENTITYGRIDAREAS;i++)
	{
		if (ent->areagrid[i].prev)
		{
			RemoveLink (&ent->areagrid[i]);
			ent->areagrid[i].prev = ent->areagrid[i].next = NULL;
		}
	}
	if (ent->area.prev)
	{
		RemoveLink (&ent->area);
		ent->area.prev = ent->area.next = NULL;
	}
}


/*
====================
SV_TouchAreaNodes
====================
*/
void SV_TouchAreaNodes ( edict_t *ent, areanode_t *node )
{
	link_t *l, *next;
	edict_t *touch;
	int old_self, old_other;

loc0:
// touch linked edicts
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent)
			continue;
		if (!touch->v->touch || touch->v->solid != SOLID_TRIGGER)
			continue;
		if (ent->v->absmin[0] > touch->v->absmax[0]
		 || ent->v->absmin[1] > touch->v->absmax[1]
		 || ent->v->absmin[2] > touch->v->absmax[2]
		 || ent->v->absmax[0] < touch->v->absmin[0]
		 || ent->v->absmax[1] < touch->v->absmin[1]
		 || ent->v->absmax[2] < touch->v->absmin[2])
			continue;
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram (touch->v->touch, "");

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (ent->v->absmax[node->axis] > node->dist)
	{
		if (ent->v->absmin[node->axis] < node->dist)
			SV_TouchAreaNodes(ent, node->children[1]); // order reversed to reduce code
		node = node->children[0];
		goto loc0;
	}
	else
	{
		if (ent->v->absmin[node->axis] < node->dist)
		{
			node = node->children[1];
			goto loc0;
		}
	}
}

void SV_TouchAreaGrid(edict_t *ent, areanode_t *node)
{
	link_t *l, *next;
	edict_t *touch;
	areagrid_t *grid;
	int old_self, old_other, igrid[3], igridmins[3], igridmaxs[3];

	igridmins[0] = (int) ((ent->v->absmin[0] + sv_areagridbias[0]) * sv_areagridscale[0]);
	igridmins[1] = (int) ((ent->v->absmin[1] + sv_areagridbias[1]) * sv_areagridscale[1]);
	//igridmins[2] = (int) ((ent->v->absmin[2] + sv_areagridbias[2]) * sv_areagridscale[2]);
	igridmaxs[0] = (int) ((ent->v->absmax[0] + sv_areagridbias[0]) * sv_areagridscale[0]) + 1;
	igridmaxs[1] = (int) ((ent->v->absmax[1] + sv_areagridbias[1]) * sv_areagridscale[1]) + 1;
	//igridmaxs[2] = (int) ((ent->v->absmax[2] + sv_areagridbias[2]) * sv_areagridscale[2]) + 1;
	igridmins[0] = max(0, igridmins[0]);
	igridmins[1] = max(0, igridmins[1]);
	//igridmins[2] = max(0, igridmins[2]);
	igridmaxs[0] = min(AREA_GRID, igridmaxs[0]);
	igridmaxs[1] = min(AREA_GRID, igridmaxs[1]);
	//igridmaxs[2] = min(AREA_GRID, igridmaxs[2]);

	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = sv_areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++)
		{
			for (l = grid->trigger_edicts.next;l != &grid->trigger_edicts;l = next)
			{
				next = l->next;
				touch = EDICT_FROM_AREA(l);
				if (touch == ent)
					continue;
				if (!touch->v->touch || touch->v->solid != SOLID_TRIGGER)
					continue;
				if (ent->v->absmin[0] > touch->v->absmax[0]
				 || ent->v->absmin[1] > touch->v->absmax[1]
				 || ent->v->absmin[2] > touch->v->absmax[2]
				 || ent->v->absmax[0] < touch->v->absmin[0]
				 || ent->v->absmax[1] < touch->v->absmin[1]
				 || ent->v->absmax[2] < touch->v->absmin[2])
					continue;
				old_self = pr_global_struct->self;
				old_other = pr_global_struct->other;

				pr_global_struct->self = EDICT_TO_PROG(touch);
				pr_global_struct->other = EDICT_TO_PROG(ent);
				pr_global_struct->time = sv.time;
				PR_ExecuteProgram (touch->v->touch, "");

				pr_global_struct->self = old_self;
				pr_global_struct->other = old_other;
			}
		}
	}
}

void SV_LinkEdict_AreaNode(edict_t *ent)
{
	areanode_t *node;
	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

	// link it in

	if (ent->v->solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts, ent);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts, ent);
}

int SV_LinkEdict_AreaGrid(edict_t *ent)
{
	areagrid_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3], gridnum;

	igridmins[0] = (int) ((ent->v->absmin[0] + sv_areagridbias[0]) * sv_areagridscale[0]);
	igridmins[1] = (int) ((ent->v->absmin[1] + sv_areagridbias[1]) * sv_areagridscale[1]);
	//igridmins[2] = (int) ((ent->v->absmin[2] + sv_areagridbias[2]) * sv_areagridscale[2]);
	igridmaxs[0] = (int) ((ent->v->absmax[0] + sv_areagridbias[0]) * sv_areagridscale[0]) + 1;
	igridmaxs[1] = (int) ((ent->v->absmax[1] + sv_areagridbias[1]) * sv_areagridscale[1]) + 1;
	//igridmaxs[2] = (int) ((ent->v->absmax[2] + sv_areagridbias[2]) * sv_areagridscale[2]) + 1;
	if (igridmins[0] < 0 || igridmaxs[0] > AREA_GRID || igridmins[1] < 0 || igridmaxs[1] > AREA_GRID || ((igridmaxs[0] - igridmins[0]) * (igridmaxs[1] - igridmins[1])) > ENTITYGRIDAREAS)
		return false;

	gridnum = 0;
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = sv_areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++, gridnum++)
		{
			if (ent->v->solid == SOLID_TRIGGER)
				InsertLinkBefore (&ent->areagrid[gridnum], &grid->trigger_edicts, ent);
			else
				InsertLinkBefore (&ent->areagrid[gridnum], &grid->solid_edicts, ent);
		}
	}
	return true;
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	model_t *model;

	if (ent->area.prev || ent->areagrid[0].prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->free)
		return;

// set the abs box

	if (ent->v->solid == SOLID_BSP)
	{
		if (ent->v->modelindex < 0 || ent->v->modelindex > MAX_MODELS)
			Host_Error("SOLID_BSP with invalid modelindex!\n");
		model = sv.models[(int) ent->v->modelindex];
		if (model != NULL)
		{
			if (model->type != mod_brush)
				Host_Error("SOLID_BSP with non-BSP model\n");

			if (ent->v->angles[0] || ent->v->angles[2] || ent->v->avelocity[0] || ent->v->avelocity[2])
			{
				VectorAdd(ent->v->origin, model->rotatedmins, ent->v->absmin);
				VectorAdd(ent->v->origin, model->rotatedmaxs, ent->v->absmax);
			}
			else if (ent->v->angles[1] || ent->v->avelocity[1])
			{
				VectorAdd(ent->v->origin, model->yawmins, ent->v->absmin);
				VectorAdd(ent->v->origin, model->yawmaxs, ent->v->absmax);
			}
			else
			{
				VectorAdd(ent->v->origin, model->normalmins, ent->v->absmin);
				VectorAdd(ent->v->origin, model->normalmaxs, ent->v->absmax);
			}
		}
		else
		{
			// SOLID_BSP with no model is valid, mainly because some QC setup code does so temporarily
			VectorAdd(ent->v->origin, ent->v->mins, ent->v->absmin);
			VectorAdd(ent->v->origin, ent->v->maxs, ent->v->absmax);
		}
	}
	else
	{
		VectorAdd(ent->v->origin, ent->v->mins, ent->v->absmin);
		VectorAdd(ent->v->origin, ent->v->maxs, ent->v->absmax);
	}

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v->flags & FL_ITEM)
	{
		ent->v->absmin[0] -= 15;
		ent->v->absmin[1] -= 15;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 15;
		ent->v->absmax[1] += 15;
		ent->v->absmax[2] += 1;
	}
	else
	{
		// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v->absmin[0] -= 1;
		ent->v->absmin[1] -= 1;
		ent->v->absmin[2] -= 1;
		ent->v->absmax[0] += 1;
		ent->v->absmax[1] += 1;
		ent->v->absmax[2] += 1;
	}

	if (ent->v->solid == SOLID_NOT)
		return;

	// try to link into areagrid, if that fails fall back on areanode
	if (!SV_LinkEdict_AreaGrid(ent))
		SV_LinkEdict_AreaNode(ent);

// if touch_triggers, touch all entities at this node and descend for more
	if (touch_triggers)
	{
		SV_TouchAreaNodes(ent, sv_areanodes);
		SV_TouchAreaGrid(ent, sv_areanodes);
	}
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
int SV_TestEntityPosition (edict_t *ent)
{
	return SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, ent->v->origin, MOVE_NORMAL, ent).startsolid;
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	int i;
	trace_t trace;
	model_t *model;

	i = ent->v->modelindex;
	if ((unsigned int) i >= MAX_MODELS)
		Host_Error("SV_ClipMoveToEntity: invalid modelindex\n");
	model = sv.models[i];
	if (i != 0 && model == NULL)
		Host_Error("SV_ClipMoveToEntity: invalid modelindex\n");

	if ((int) ent->v->solid == SOLID_BSP)
	{
		Mod_CheckLoaded(model);
		if (model->type != mod_brush)
		{
			Con_Printf ("SV_ClipMoveToEntity: SOLID_BSP with a non bsp model, entity dump:\n");
			ED_Print (ent);
			Host_Error ("SV_ClipMoveToEntity: SOLID_BSP with a non bsp model\n");
		}
		if (ent->v->movetype != MOVETYPE_PUSH)
			Host_Error ("SV_ClipMoveToEntity: SOLID_BSP without MOVETYPE_PUSH");
	}

	if (sv_polygoncollisions.integer && (mins[0] != maxs[0] || mins[1] != maxs[1] || mins[2] != maxs[2]))
		Collision_PolygonClipTrace(&trace, ent, model, ent->v->origin, ent->v->angles, ent->v->mins, ent->v->maxs, start, mins, maxs, end);
	else
		Collision_ClipTrace(&trace, ent, model, ent->v->origin, ent->v->angles, ent->v->mins, ent->v->maxs, start, mins, maxs, end);

	return trace;
}

//===========================================================================

/*
====================
SV_ClipToAreaNodes

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToAreaNodes ( areanode_t *node, moveclip_t *clip )
{
	link_t *l, *next;
	edict_t *touch;
	trace_t trace;

loc0:
	if (clip->trace.allsolid)
		return;
// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v->solid == SOLID_TRIGGER)
		{
			ED_Print(touch);
			Host_Error ("Trigger in clipping list");
		}

		if (clip->type == MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->v->absmax[0]
		 || clip->boxmaxs[0] < touch->v->absmin[0]
		 || clip->boxmins[1] > touch->v->absmax[1]
		 || clip->boxmaxs[1] < touch->v->absmin[1]
		 || clip->boxmins[2] > touch->v->absmax[2]
		 || clip->boxmaxs[2] < touch->v->absmin[2])
			continue;

		if (clip->passedict)
		{
			if (clip->passedict->v->size[0] && !touch->v->size[0])
				continue;	// points never interact
		 	if (PROG_TO_EDICT(touch->v->owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v->owner) == touch)
				continue;	// don't clip against owner
			// LordHavoc: corpse code
			if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
				continue;
			if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
				continue;
		}

		// might interact, so do an exact clip
		if (touch->v->solid == SOLID_BSP)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->hullmins, clip->hullmaxs, clip->end);
		else if ((int)touch->v->flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end);
		// LordHavoc: take the 'best' answers from the new trace and combine with existing data
		if (trace.allsolid)
			clip->trace.allsolid = true;
		if (trace.startsolid)
		{
			clip->trace.startsolid = true;
			if (!clip->trace.ent)
				clip->trace.ent = trace.ent;
		}
		if (trace.inopen)
			clip->trace.inopen = true;
		if (trace.inwater)
			clip->trace.inwater = true;
		if (trace.fraction < clip->trace.fraction)
		{
			clip->trace.fraction = trace.fraction;
			VectorCopy(trace.endpos, clip->trace.endpos);
			clip->trace.plane = trace.plane;
			clip->trace.endcontents = trace.endcontents;
			clip->trace.ent = trace.ent;
		}
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if (clip->boxmaxs[node->axis] > node->dist)
	{
		if (clip->boxmins[node->axis] < node->dist)
			SV_ClipToAreaNodes(node->children[1], clip);
		node = node->children[0];
		goto loc0;
	}
	else if (clip->boxmins[node->axis] < node->dist)
	{
		node = node->children[1];
		goto loc0;
	}
}

/*
====================
SV_ClipToAreaGrid

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToAreaGrid(moveclip_t *clip)
{
	link_t *l, *next;
	edict_t *touch;
	areagrid_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3];
	trace_t trace;

	if (clip->trace.allsolid)
		return;

	igridmins[0] = (int) ((clip->boxmins[0] + sv_areagridbias[0]) * sv_areagridscale[0]);
	igridmins[1] = (int) ((clip->boxmins[1] + sv_areagridbias[1]) * sv_areagridscale[1]);
	//igridmins[2] = (int) ((clip->boxmins[2] + sv_areagridbias[2]) * sv_areagridscale[2]);
	igridmaxs[0] = (int) ((clip->boxmaxs[0] + sv_areagridbias[0]) * sv_areagridscale[0]) + 1;
	igridmaxs[1] = (int) ((clip->boxmaxs[1] + sv_areagridbias[1]) * sv_areagridscale[1]) + 1;
	//igridmaxs[2] = (int) ((clip->boxmaxs[2] + sv_areagridbias[2]) * sv_areagridscale[2]) + 1;
	igridmins[0] = max(0, igridmins[0]);
	igridmins[1] = max(0, igridmins[1]);
	//igridmins[2] = max(0, igridmins[2]);
	igridmaxs[0] = min(AREA_GRID, igridmaxs[0]);
	igridmaxs[1] = min(AREA_GRID, igridmaxs[1]);
	//igridmaxs[2] = min(AREA_GRID, igridmaxs[2]);

	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = sv_areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++)
		{
			for (l = grid->solid_edicts.next;l != &grid->solid_edicts;l = next)
			{
				next = l->next;
				touch = EDICT_FROM_AREA(l);
				if (touch->v->solid == SOLID_NOT)
					continue;
				if (touch == clip->passedict)
					continue;
				if (touch->v->solid == SOLID_TRIGGER)
				{
					ED_Print(touch);
					Host_Error ("Trigger in clipping list");
				}

				if (clip->type == MOVE_NOMONSTERS && touch->v->solid != SOLID_BSP)
					continue;

				if (clip->boxmins[0] > touch->v->absmax[0]
				 || clip->boxmaxs[0] < touch->v->absmin[0]
				 || clip->boxmins[1] > touch->v->absmax[1]
				 || clip->boxmaxs[1] < touch->v->absmin[1]
				 || clip->boxmins[2] > touch->v->absmax[2]
				 || clip->boxmaxs[2] < touch->v->absmin[2])
					continue;

				if (clip->passedict)
				{
					if (clip->passedict->v->size[0] && !touch->v->size[0])
						continue;	// points never interact
					if (PROG_TO_EDICT(touch->v->owner) == clip->passedict)
						continue;	// don't clip against own missiles
					if (PROG_TO_EDICT(clip->passedict->v->owner) == touch)
						continue;	// don't clip against owner
					// LordHavoc: corpse code
					if (clip->passedict->v->solid == SOLID_CORPSE && (touch->v->solid == SOLID_SLIDEBOX || touch->v->solid == SOLID_CORPSE))
						continue;
					if (clip->passedict->v->solid == SOLID_SLIDEBOX && touch->v->solid == SOLID_CORPSE)
						continue;
				}

				// might interact, so do an exact clip
				if (touch->v->solid == SOLID_BSP)
					trace = SV_ClipMoveToEntity (touch, clip->start, clip->hullmins, clip->hullmaxs, clip->end);
				else if ((int)touch->v->flags & FL_MONSTER)
					trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end);
				else
					trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end);
				// LordHavoc: take the 'best' answers from the new trace and combine with existing data
				if (trace.allsolid)
					clip->trace.allsolid = true;
				if (trace.startsolid)
				{
					clip->trace.startsolid = true;
					if (!clip->trace.ent)
						clip->trace.ent = trace.ent;
				}
				if (trace.inopen)
					clip->trace.inopen = true;
				if (trace.inwater)
					clip->trace.inwater = true;
				if (trace.fraction < clip->trace.fraction)
				{
					clip->trace.fraction = trace.fraction;
					VectorCopy(trace.endpos, clip->trace.endpos);
					clip->trace.plane = trace.plane;
					clip->trace.endcontents = trace.endcontents;
					clip->trace.ent = trace.ent;
				}
				if (clip->trace.allsolid)
					return;
			}
		}
	}
}


/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	if (sv_useareanodes.integer)
	{
		int i;

		for (i=0 ; i<3 ; i++)
		{
			if (end[i] > start[i])
			{
				boxmins[i] = start[i] + mins[i] - 1;
				boxmaxs[i] = end[i] + maxs[i] + 1;
			}
			else
			{
				boxmins[i] = end[i] + mins[i] - 1;
				boxmaxs[i] = start[i] + maxs[i] + 1;
			}
		}
	}
	else
	{
		// debug to test against everything
		boxmins[0] = boxmins[1] = boxmins[2] = -999999999;
		boxmaxs[0] = boxmaxs[1] = boxmaxs[2] =  999999999;
	}
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t	clip;
	vec3_t		bigmins, bigmaxs;
	int			i;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	VectorCopy(start, clip.start);
	VectorCopy(end, clip.end);
	VectorCopy(mins, clip.mins);
	VectorCopy(maxs, clip.maxs);
	clip.type = type;
	clip.passedict = passedict;

	Collision_RoundUpToHullSize(sv.worldmodel, clip.mins, clip.maxs, clip.hullmins, clip.hullmaxs);

	if (type == MOVE_MISSILE)
	{
		// LordHavoc: modified this, was = -15, now = clip.mins[i] - 15
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = clip.mins[i] - 15;
			clip.maxs2[i] = clip.maxs[i] + 15;
		}
	}
	else
	{
		VectorCopy (clip.mins, clip.mins2);
		VectorCopy (clip.maxs, clip.maxs2);
	}

	bigmins[0] = min(clip.mins2[0], clip.hullmins[0]);
	bigmaxs[0] = max(clip.maxs2[0], clip.hullmaxs[0]);
	bigmins[1] = min(clip.mins2[1], clip.hullmins[1]);
	bigmaxs[1] = max(clip.maxs2[1], clip.hullmaxs[1]);
	bigmins[2] = min(clip.mins2[2], clip.hullmins[2]);
	bigmaxs[2] = max(clip.maxs2[2], clip.hullmaxs[2]);

	// clip to world
	clip.trace = SV_ClipMoveToEntity (sv.edicts, start, mins, maxs, end);

	// clip to entities
	// create the bounding box of the entire move
	SV_MoveBounds ( start, bigmins, bigmaxs, end, clip.boxmins, clip.boxmaxs );

	SV_ClipToAreaNodes(sv_areanodes, &clip);
	SV_ClipToAreaGrid(&clip);

	return clip.trace;
}

