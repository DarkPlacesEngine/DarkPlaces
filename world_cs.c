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

extern cvar_t sv_debugmove;
extern cvar_t sv_areagrid_mingridsize;

trace_t CSSV_Move(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict);

void CSSV_AreaStats_f(void);

void CSSV_World_Init(void)
{
	Cvar_RegisterVariable(&sv_debugmove);
	Cvar_RegisterVariable(&sv_areagrid_mingridsize);
	Cmd_AddCommand("cssv_areastats", CSSV_AreaStats_f, "prints information on culling grid system");
	Collision_Init();
}

//============================================================================

// CSClearLink is used for new headnodes
static void CSClearLink (link_t *l)
{
	l->entitynumber = 0;
	l->prev = l->next = l;
}

static void CSRemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

static void CSInsertLinkBefore (link_t *l, link_t *before, int entitynumber)
{
	l->entitynumber = entitynumber;
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}


/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

int cssv_areagrid_stats_calls = 0;
int cssv_areagrid_stats_nodechecks = 0;
int cssv_areagrid_stats_entitychecks = 0;

void CSSV_AreaStats_f(void)
{
	Con_Printf("csareagrid check stats: %d calls %d nodes (%f per call) %d entities (%f per call)\n", cssv_areagrid_stats_calls, cssv_areagrid_stats_nodechecks, (double) cssv_areagrid_stats_nodechecks / (double) cssv_areagrid_stats_calls, cssv_areagrid_stats_entitychecks, (double) cssv_areagrid_stats_entitychecks / (double) cssv_areagrid_stats_calls);
	cssv_areagrid_stats_calls = 0;
	cssv_areagrid_stats_nodechecks = 0;
	cssv_areagrid_stats_entitychecks = 0;
}

typedef struct areagrid_s
{
	link_t edicts;
}
csareagrid_t;

#define CSAREA_GRID 512
#define CSAREA_GRIDNODES (CSAREA_GRID * CSAREA_GRID)

static csareagrid_t cssv_areagrid[CSAREA_GRIDNODES], cssv_areagrid_outside;
static vec3_t cssv_areagrid_bias, cssv_areagrid_scale, cssv_areagrid_mins, cssv_areagrid_maxs, cssv_areagrid_size;
static int cssv_areagrid_marknumber = 1;

void CSSV_CreateAreaGrid (vec3_t mins, vec3_t maxs)
{
	int i;
	CSClearLink (&cssv_areagrid_outside.edicts);
	// choose either the world box size, or a larger box to ensure the grid isn't too fine
	cssv_areagrid_size[0] = max(maxs[0] - mins[0], CSAREA_GRID * sv_areagrid_mingridsize.value);
	cssv_areagrid_size[1] = max(maxs[1] - mins[1], CSAREA_GRID * sv_areagrid_mingridsize.value);
	cssv_areagrid_size[2] = max(maxs[2] - mins[2], CSAREA_GRID * sv_areagrid_mingridsize.value);
	// figure out the corners of such a box, centered at the center of the world box
	cssv_areagrid_mins[0] = (mins[0] + maxs[0] - cssv_areagrid_size[0]) * 0.5f;
	cssv_areagrid_mins[1] = (mins[1] + maxs[1] - cssv_areagrid_size[1]) * 0.5f;
	cssv_areagrid_mins[2] = (mins[2] + maxs[2] - cssv_areagrid_size[2]) * 0.5f;
	cssv_areagrid_maxs[0] = (mins[0] + maxs[0] + cssv_areagrid_size[0]) * 0.5f;
	cssv_areagrid_maxs[1] = (mins[1] + maxs[1] + cssv_areagrid_size[1]) * 0.5f;
	cssv_areagrid_maxs[2] = (mins[2] + maxs[2] + cssv_areagrid_size[2]) * 0.5f;
	// now calculate the actual useful info from that
	VectorNegate(cssv_areagrid_mins, cssv_areagrid_bias);
	cssv_areagrid_scale[0] = CSAREA_GRID / cssv_areagrid_size[0];
	cssv_areagrid_scale[1] = CSAREA_GRID / cssv_areagrid_size[1];
	cssv_areagrid_scale[2] = CSAREA_GRID / cssv_areagrid_size[2];
	for (i = 0;i < CSAREA_GRIDNODES;i++)
	{
		CSClearLink (&cssv_areagrid[i].edicts);
	}
	Con_DPrintf("cssv_areagrid settings: divisions %ix%ix1 : box %f %f %f : %f %f %f size %f %f %f grid %f %f %f (mingrid %f)\n", CSAREA_GRID, CSAREA_GRID, cssv_areagrid_mins[0], cssv_areagrid_mins[1], cssv_areagrid_mins[2], cssv_areagrid_maxs[0], cssv_areagrid_maxs[1], cssv_areagrid_maxs[2], cssv_areagrid_size[0], cssv_areagrid_size[1], cssv_areagrid_size[2], 1.0f / cssv_areagrid_scale[0], 1.0f / cssv_areagrid_scale[1], 1.0f / cssv_areagrid_scale[2], sv_areagrid_mingridsize.value);
}

/*
===============
CSSV_ClearWorld

===============
*/
void CSSV_ClearWorld (void)
{
	CSSV_CreateAreaGrid(cl.worldmodel->normalmins, cl.worldmodel->normalmaxs);
}


/*
===============
CSSV_UnlinkEdict

===============
*/
void CSSV_UnlinkEdict (prvm_edict_t *ent)
{
	int i;
	for (i = 0;i < ENTITYGRIDAREAS;i++)
	{
		if (ent->priv.server->areagrid[i].prev)
		{
			CSRemoveLink (&ent->priv.server->areagrid[i]);
			ent->priv.server->areagrid[i].prev = ent->priv.server->areagrid[i].next = NULL;
		}
	}
}

int CSSV_EntitiesInBox(vec3_t mins, vec3_t maxs, int maxlist, prvm_edict_t **list)
{
	int numlist;
	csareagrid_t *grid;
	link_t *l;
	prvm_edict_t *ent;
	int igrid[3], igridmins[3], igridmaxs[3];

	cssv_areagrid_stats_calls++;
	cssv_areagrid_marknumber++;
	igridmins[0] = (int) ((mins[0] + cssv_areagrid_bias[0]) * cssv_areagrid_scale[0]);
	igridmins[1] = (int) ((mins[1] + cssv_areagrid_bias[1]) * cssv_areagrid_scale[1]);
	//igridmins[2] = (int) ((mins[2] + cssv_areagrid_bias[2]) * cssv_areagrid_scale[2]);
	igridmaxs[0] = (int) ((maxs[0] + cssv_areagrid_bias[0]) * cssv_areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) ((maxs[1] + cssv_areagrid_bias[1]) * cssv_areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) ((maxs[2] + cssv_areagrid_bias[2]) * cssv_areagrid_scale[2]) + 1;
	igridmins[0] = max(0, igridmins[0]);
	igridmins[1] = max(0, igridmins[1]);
	//igridmins[2] = max(0, igridmins[2]);
	igridmaxs[0] = min(CSAREA_GRID, igridmaxs[0]);
	igridmaxs[1] = min(CSAREA_GRID, igridmaxs[1]);
	//igridmaxs[2] = min(CSAREA_GRID, igridmaxs[2]);

	numlist = 0;
	// add entities not linked into areagrid because they are too big or
	// outside the grid bounds
	if (cssv_areagrid_outside.edicts.next != &cssv_areagrid_outside.edicts)
	{
		for (l = cssv_areagrid_outside.edicts.next;l != &cssv_areagrid_outside.edicts;l = l->next)
		{
			ent = PRVM_EDICT_NUM_UNSIGNED(l->entitynumber);
			if (ent->priv.server->areagridmarknumber != cssv_areagrid_marknumber)
			{
				ent->priv.server->areagridmarknumber = cssv_areagrid_marknumber;
				if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->fields.client->absmin, ent->fields.client->absmax))
				{
					if (numlist < maxlist)
						list[numlist] = ent;
					numlist++;
				}
				cssv_areagrid_stats_entitychecks++;
			}
		}
	}
	// add grid linked entities
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = cssv_areagrid + igrid[1] * CSAREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++)
		{
			if (grid->edicts.next != &grid->edicts)
			{
				for (l = grid->edicts.next;l != &grid->edicts;l = l->next)
				{
					ent = PRVM_EDICT_NUM_UNSIGNED(l->entitynumber);
					if (ent->priv.server->areagridmarknumber != cssv_areagrid_marknumber)
					{
						ent->priv.server->areagridmarknumber = cssv_areagrid_marknumber;
						if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->fields.client->absmin, ent->fields.client->absmax))
						{
							if (numlist < maxlist)
								list[numlist] = ent;
							numlist++;
						}
					}
					cssv_areagrid_stats_entitychecks++;
				}
			}
		}
	}
	return numlist;
}

void CSSV_TouchAreaGrid(prvm_edict_t *ent)
{
	int i, numtouchedicts, old_self, old_other;
	prvm_edict_t *touch, *touchedicts[MAX_EDICTS];

	// build a list of edicts to touch, because the link loop can be corrupted
	// by CSSV_IncreaseEdicts called during touch functions
	numtouchedicts = CSSV_EntitiesInBox(ent->fields.client->absmin, ent->fields.client->absmax, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("CSSV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}

	old_self = prog->globals.client->self;
	old_other = prog->globals.client->other;
	for (i = 0;i < numtouchedicts;i++)
	{
		touch = touchedicts[i];
		if (touch != ent && (int)touch->fields.client->solid == SOLID_TRIGGER && touch->fields.client->touch)
		{
			prog->globals.client->self = PRVM_EDICT_TO_PROG(touch);
			prog->globals.client->other = PRVM_EDICT_TO_PROG(ent);
			prog->globals.client->time = cl.time;
			PRVM_ExecuteProgram (touch->fields.client->touch, "QC function self.touch is missing");
		}
	}
	prog->globals.client->self = old_self;
	prog->globals.client->other = old_other;
}

void CSSV_LinkEdict_AreaGrid(prvm_edict_t *ent)
{
	csareagrid_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3], gridnum, entitynumber = PRVM_NUM_FOR_EDICT(ent);

	if (entitynumber <= 0 || entitynumber >= prog->max_edicts || PRVM_EDICT_NUM(entitynumber) != ent)
	{
		Con_Printf ("CSSV_LinkEdict_AreaGrid: invalid edict %p (edicts is %p, edict compared to prog->edicts is %i)\n", ent, prog->edicts, entitynumber);
		return;
	}

	igridmins[0] = (int) ((ent->fields.client->absmin[0] + cssv_areagrid_bias[0]) * cssv_areagrid_scale[0]);
	igridmins[1] = (int) ((ent->fields.client->absmin[1] + cssv_areagrid_bias[1]) * cssv_areagrid_scale[1]);
	//igridmins[2] = (int) ((ent->fields.client->absmin[2] + cssv_areagrid_bias[2]) * cssv_areagrid_scale[2]);
	igridmaxs[0] = (int) ((ent->fields.client->absmax[0] + cssv_areagrid_bias[0]) * cssv_areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) ((ent->fields.client->absmax[1] + cssv_areagrid_bias[1]) * cssv_areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) ((ent->fields.client->absmax[2] + cssv_areagrid_bias[2]) * cssv_areagrid_scale[2]) + 1;
	if (igridmins[0] < 0 || igridmaxs[0] > CSAREA_GRID || igridmins[1] < 0 || igridmaxs[1] > CSAREA_GRID || ((igridmaxs[0] - igridmins[0]) * (igridmaxs[1] - igridmins[1])) > ENTITYGRIDAREAS)
	{
		// wow, something outside the grid, store it as such
		CSInsertLinkBefore (&ent->priv.server->areagrid[0], &cssv_areagrid_outside.edicts, entitynumber);
		return;
	}

	gridnum = 0;
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = cssv_areagrid + igrid[1] * CSAREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++, gridnum++)
			CSInsertLinkBefore (&ent->priv.server->areagrid[gridnum], &grid->edicts, entitynumber);
	}
}

/*
===============
SV_LinkEdict

===============
*/
void CSSV_LinkEdict (prvm_edict_t *ent, qboolean touch_triggers)
{
	model_t *model;

	if (ent->priv.server->areagrid[0].prev)
		CSSV_UnlinkEdict (ent);	// unlink from old position

	if (ent == prog->edicts)
		return;		// don't add the world

	if (ent->priv.server->free)
		return;

// set the abs box

	if (ent->fields.client->solid == SOLID_BSP)
	{
		int modelindex = ent->fields.client->modelindex;
		if (modelindex < 0 || modelindex > MAX_MODELS)
		{
			Con_Printf("edict %i: SOLID_BSP with invalid modelindex!\n", PRVM_NUM_FOR_EDICT(ent));
			modelindex = 0;
		}
		model = cl.model_precache[modelindex];
		if (model != NULL)
		{
			if (!model->TraceBox)
				Con_Printf("edict %i: SOLID_BSP with non-collidable model\n", PRVM_NUM_FOR_EDICT(ent));

			if (ent->fields.client->angles[0] || ent->fields.client->angles[2] || ent->fields.client->avelocity[0] || ent->fields.client->avelocity[2])
			{
				VectorAdd(ent->fields.client->origin, model->rotatedmins, ent->fields.client->absmin);
				VectorAdd(ent->fields.client->origin, model->rotatedmaxs, ent->fields.client->absmax);
			}
			else if (ent->fields.client->angles[1] || ent->fields.client->avelocity[1])
			{
				VectorAdd(ent->fields.client->origin, model->yawmins, ent->fields.client->absmin);
				VectorAdd(ent->fields.client->origin, model->yawmaxs, ent->fields.client->absmax);
			}
			else
			{
				VectorAdd(ent->fields.client->origin, model->normalmins, ent->fields.client->absmin);
				VectorAdd(ent->fields.client->origin, model->normalmaxs, ent->fields.client->absmax);
			}
		}
		else
		{
			// SOLID_BSP with no model is valid, mainly because some QC setup code does so temporarily
			VectorAdd(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->absmin);
			VectorAdd(ent->fields.client->origin, ent->fields.client->maxs, ent->fields.client->absmax);
		}
	}
	else
	{
		VectorAdd(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->absmin);
		VectorAdd(ent->fields.client->origin, ent->fields.client->maxs, ent->fields.client->absmax);
	}

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->fields.client->flags & FL_ITEM)
	{
		ent->fields.client->absmin[0] -= 15;
		ent->fields.client->absmin[1] -= 15;
		ent->fields.client->absmin[2] -= 1;
		ent->fields.client->absmax[0] += 15;
		ent->fields.client->absmax[1] += 15;
		ent->fields.client->absmax[2] += 1;
	}
	else
	{
		// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->fields.client->absmin[0] -= 1;
		ent->fields.client->absmin[1] -= 1;
		ent->fields.client->absmin[2] -= 1;
		ent->fields.client->absmax[0] += 1;
		ent->fields.client->absmax[1] += 1;
		ent->fields.client->absmax[2] += 1;
	}

	if (ent->fields.client->solid == SOLID_NOT)
		return;

	CSSV_LinkEdict_AreaGrid(ent);

// if touch_triggers, touch all entities at this node and descend for more
	if (touch_triggers)
		CSSV_TouchAreaGrid(ent);
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
int CSSV_TestEntityPosition (prvm_edict_t *ent)
{
	return CSSV_Move (ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->maxs, ent->fields.client->origin, MOVE_NORMAL, ent).startsolid;
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
trace_t CSSV_ClipMoveToEntity(prvm_edict_t *ent, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int movetype, int hitsupercontents)
{
	trace_t trace;
	model_t *model = NULL;
	matrix4x4_t matrix, imatrix;
	float tempnormal[3], starttransformed[3], endtransformed[3];

	memset(&trace, 0, sizeof(trace));
	trace.fraction = trace.realfraction = 1;
	VectorCopy(end, trace.endpos);

	if ((int) ent->fields.client->solid == SOLID_BSP || movetype == MOVE_HITMODEL)
	{
		unsigned int modelindex = ent->fields.client->modelindex;
		// if the modelindex is 0, it shouldn't be SOLID_BSP!
		if (modelindex == 0)
		{
			Con_Printf("SV_ClipMoveToEntity: edict %i: SOLID_BSP with no model\n", PRVM_NUM_FOR_EDICT(ent));
			return trace;
		}
		if (modelindex >= MAX_MODELS)
		{
			Con_Printf("SV_ClipMoveToEntity: edict %i: SOLID_BSP with invalid modelindex\n", PRVM_NUM_FOR_EDICT(ent));
			return trace;
		}
		model = cl.model_precache[modelindex];
		if (modelindex != 0 && model == NULL)
		{
			Con_Printf("SV_ClipMoveToEntity: edict %i: SOLID_BSP with invalid modelindex\n", PRVM_NUM_FOR_EDICT(ent));
			return trace;
		}

		if ((int) ent->fields.client->solid == SOLID_BSP)
		{
			if (!model->TraceBox)
			{
				Con_Printf("SV_ClipMoveToEntity: edict %i: SOLID_BSP with a non-collidable model\n", PRVM_NUM_FOR_EDICT(ent));
				return trace;
			}
			//if (ent->fields.client->movetype != MOVETYPE_PUSH)
			//{
			//	Con_Printf("SV_ClipMoveToEntity: edict %i: SOLID_BSP without MOVETYPE_PUSH\n", PRVM_NUM_FOR_EDICT(ent));
			//	return trace;
			//}
		}
		Matrix4x4_CreateFromQuakeEntity(&matrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], 1);
	}
	else
		Matrix4x4_CreateTranslate(&matrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2]);

	Matrix4x4_Invert_Simple(&imatrix, &matrix);
	Matrix4x4_Transform(&imatrix, start, starttransformed);
	Matrix4x4_Transform(&imatrix, end, endtransformed);
#if COLLISIONPARANOID >= 3
	Con_Printf("trans(%f %f %f -> %f %f %f, %f %f %f -> %f %f %f)", start[0], start[1], start[2], starttransformed[0], starttransformed[1], starttransformed[2], end[0], end[1], end[2], endtransformed[0], endtransformed[1], endtransformed[2]);
#endif

	if (model && model->TraceBox)
	{
		int frame;
		frame = (int)ent->fields.client->frame;
		frame = bound(0, frame, (model->numframes - 1));
		model->TraceBox(model, frame, &trace, starttransformed, mins, maxs, endtransformed, hitsupercontents);
	}
	else
		Collision_ClipTrace_Box(&trace, ent->fields.client->mins, ent->fields.client->maxs, starttransformed, mins, maxs, endtransformed, hitsupercontents, SUPERCONTENTS_SOLID);
	trace.fraction = bound(0, trace.fraction, 1);
	trace.realfraction = bound(0, trace.realfraction, 1);

	if (trace.fraction < 1)
	{
		VectorLerp(start, trace.fraction, end, trace.endpos);
		VectorCopy(trace.plane.normal, tempnormal);
		Matrix4x4_Transform3x3(&matrix, tempnormal, trace.plane.normal);
		// FIXME: should recalc trace.plane.dist
	}
	else
		VectorCopy(end, trace.endpos);

	return trace;
}

//===========================================================================

/*
==================
SV_Move
==================
*/
#if COLLISIONPARANOID >= 1
trace_t CSSV_Move_(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict)
#else
trace_t CSSV_Move(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict)
#endif
{
	vec3_t hullmins, hullmaxs;
	int i;
	int hitsupercontentsmask;
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
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];

	VectorCopy(start, clipstart);
	VectorCopy(end, clipend);
	VectorCopy(mins, clipmins);
	VectorCopy(maxs, clipmaxs);
	VectorCopy(mins, clipmins2);
	VectorCopy(maxs, clipmaxs2);
#if COLLISIONPARANOID >= 3
	Con_Printf("move(%f %f %f,%f %f %f)", clipstart[0], clipstart[1], clipstart[2], clipend[0], clipend[1], clipend[2]);
#endif

	hitsupercontentsmask = SUPERCONTENTS_SOLID;
	if (passedict)
	{
		if (passedict->fields.client->solid == SOLID_SLIDEBOX)
			hitsupercontentsmask |= SUPERCONTENTS_PLAYERCLIP;
		if ((int)passedict->fields.client->flags & FL_MONSTER)
			hitsupercontentsmask |= SUPERCONTENTS_MONSTERCLIP;
	}

	// clip to world
	cliptrace = CSSV_ClipMoveToEntity(prog->edicts, clipstart, clipmins, clipmaxs, clipend, type, hitsupercontentsmask);
	cliptrace.bmodelstartsolid = cliptrace.startsolid;
	if (cliptrace.startsolid || cliptrace.fraction < 1)
		cliptrace.ent = prog->edicts;
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
	if (passedict == prog->edicts)
		passedict = NULL;
	// precalculate prog value for passedict for comparisons
	passedictprog = PRVM_EDICT_TO_PROG(passedict);
	// figure out whether this is a point trace for comparisons
	pointtrace = VectorCompare(clipmins, clipmaxs);
	// precalculate passedict's owner edict pointer for comparisons
	traceowner = passedict ? PRVM_PROG_TO_EDICT(passedict->fields.client->owner) : 0;

	// clip to enttiies
	numtouchedicts = CSSV_EntitiesInBox(clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("CSSV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
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
			// don't clip corpse against character
			if (passedict->fields.client->solid == SOLID_CORPSE && (touch->fields.client->solid == SOLID_SLIDEBOX || touch->fields.client->solid == SOLID_CORPSE))
				continue;
			// don't clip character against corpse
			if (passedict->fields.client->solid == SOLID_SLIDEBOX && touch->fields.client->solid == SOLID_CORPSE)
				continue;
		}

		// might interact, so do an exact clip
		if ((int)touch->fields.client->flags & FL_MONSTER)
			trace = CSSV_ClipMoveToEntity(touch, clipstart, clipmins2, clipmaxs2, clipend, type, hitsupercontentsmask);
		else
			trace = CSSV_ClipMoveToEntity(touch, clipstart, clipmins, clipmaxs, clipend, type, hitsupercontentsmask);
		// LordHavoc: take the 'best' answers from the new trace and combine with existing data
		if (trace.allsolid)
			cliptrace.allsolid = true;
		if (trace.startsolid)
		{
			if (touch->fields.client->solid == SOLID_BSP)
				cliptrace.bmodelstartsolid = true;
			cliptrace.startsolid = true;
			if (cliptrace.realfraction == 1)
				cliptrace.ent = touch;
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
			VectorCopy(trace.endpos, cliptrace.endpos);
			cliptrace.plane = trace.plane;
			cliptrace.ent = touch;
		}
		cliptrace.startsupercontents |= trace.startsupercontents;
	}

	return cliptrace;
}

#if COLLISIONPARANOID >= 1
trace_t CSSV_Move(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict)
{
	int endstuck;
	trace_t trace;
	vec3_t temp;
	trace = CSSV_Move_(start, mins, maxs, end, type, passedict);
	if (passedict)
	{
		VectorCopy(trace.endpos, temp);
		endstuck = CSSV_Move_(temp, mins, maxs, temp, type, passedict).startsolid;
#if COLLISIONPARANOID < 3
		if (trace.startsolid || endstuck)
#endif
			Con_Printf("%s{e%i:%f %f %f:%f %f %f:%f:%f %f %f%s%s}\n", (trace.startsolid || endstuck) ? "\002" : "", passedict ? passedict - prog->edicts : -1, passedict->fields.client->origin[0], passedict->fields.client->origin[1], passedict->fields.client->origin[2], end[0] - passedict->fields.client->origin[0], end[1] - passedict->fields.client->origin[1], end[2] - passedict->fields.client->origin[2], trace.fraction, trace.endpos[0] - passedict->fields.client->origin[0], trace.endpos[1] - passedict->fields.client->origin[1], trace.endpos[2] - passedict->fields.client->origin[2], trace.startsolid ? " startstuck" : "", endstuck ? " endstuck" : "");
	}
	return trace;
}
#endif

int CSSV_PointSuperContents(const vec3_t point)
{
	return CSSV_Move(point, vec3_origin, vec3_origin, point, sv_gameplayfix_swiminbmodels.integer ? MOVE_NOMONSTERS : MOVE_WORLDONLY, NULL).startsupercontents;
}

int CSSV_PointQ1Contents(const vec3_t point)
{
	return Mod_Q1BSP_NativeContentsFromSuperContents(NULL, CSSV_PointSuperContents(point));
}


