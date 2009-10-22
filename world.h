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
// world.h

#ifndef WORLD_H
#define WORLD_H

#include "collision.h"

#define MOVE_NORMAL     0
#define MOVE_NOMONSTERS 1
#define MOVE_MISSILE    2
#define MOVE_WORLDONLY  3
#define MOVE_HITMODEL   4

#define AREA_GRID 512
#define AREA_GRIDNODES (AREA_GRID * AREA_GRID)

typedef struct link_s
{
	int entitynumber;
	struct link_s	*prev, *next;
} link_t;

typedef struct world_physics_s
{
	// for ODE physics engine
	qboolean ode; // if true then ode is activated
	void *ode_world;
	void *ode_space;
	void *ode_contactgroup;
	// number of constraint solver iterations to use (for dWorldStepFast)
	int ode_iterations;
	// actual step (server frametime / ode_iterations)
	vec_t ode_step;
	// max velocity for a 1-unit radius object at current step to prevent
	// missed collisions
	vec_t ode_movelimit;
}
world_physics_t;

typedef struct world_s
{
	// convenient fields
	char filename[MAX_QPATH];
	vec3_t mins;
	vec3_t maxs;

	int areagrid_stats_calls;
	int areagrid_stats_nodechecks;
	int areagrid_stats_entitychecks;

	link_t areagrid[AREA_GRIDNODES];
	link_t areagrid_outside;
	vec3_t areagrid_bias;
	vec3_t areagrid_scale;
	vec3_t areagrid_mins;
	vec3_t areagrid_maxs;
	vec3_t areagrid_size;
	int areagrid_marknumber;

	// if the QC uses a physics engine, the data for it is here
	world_physics_t physics;
}
world_t;

struct prvm_edict_s;

// cyclic doubly-linked list functions
void World_ClearLink(link_t *l);
void World_RemoveLink(link_t *l);
void World_InsertLinkBefore(link_t *l, link_t *before, int entitynumber);

void World_Init(void);
void World_Shutdown(void);

/// called after the world model has been loaded, before linking any entities
void World_SetSize(world_t *world, const char *filename, const vec3_t mins, const vec3_t maxs);
/// unlinks all entities (used before reallocation of edicts)
void World_UnlinkAll(world_t *world);

void World_PrintAreaStats(world_t *world, const char *worldname);

/// call before removing an entity, and before trying to move one,
/// so it doesn't clip against itself
void World_UnlinkEdict(struct prvm_edict_s *ent);

/// Needs to be called any time an entity changes origin, mins, maxs
void World_LinkEdict(world_t *world, struct prvm_edict_s *ent, const vec3_t mins, const vec3_t maxs);

/// \returns list of entities touching a box
int World_EntitiesInBox(world_t *world, const vec3_t mins, const vec3_t maxs, int maxlist, struct prvm_edict_s **list);

void World_Start(world_t *world);
void World_End(world_t *world);

// update physics
// this is called by SV_Physics
void World_Physics_Frame(world_t *world, double frametime, double gravity);

// remove physics data from entity
// this is called by entity removal
struct prvm_edict_s;
void World_Physics_RemoveFromEntity(world_t *world, struct prvm_edict_s *ed);
void World_Physics_RemoveJointFromEntity(world_t *world, struct prvm_edict_s *ed);

#endif

