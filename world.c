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

void World_Init(void)
{
	Collision_Init();
}

//============================================================================

/// World_ClearLink is used for new headnodes
void World_ClearLink (link_t *l)
{
	l->entitynumber = 0;
	l->prev = l->next = l;
}

void World_RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void World_InsertLinkBefore (link_t *l, link_t *before, int entitynumber)
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

void World_PrintAreaStats(world_t *world, const char *worldname)
{
	Con_Printf("%s areagrid check stats: %d calls %d nodes (%f per call) %d entities (%f per call)\n", worldname, world->areagrid_stats_calls, world->areagrid_stats_nodechecks, (double) world->areagrid_stats_nodechecks / (double) world->areagrid_stats_calls, world->areagrid_stats_entitychecks, (double) world->areagrid_stats_entitychecks / (double) world->areagrid_stats_calls);
	world->areagrid_stats_calls = 0;
	world->areagrid_stats_nodechecks = 0;
	world->areagrid_stats_entitychecks = 0;
}

/*
===============
World_SetSize

===============
*/
void World_SetSize(world_t *world, const char *filename, const vec3_t mins, const vec3_t maxs)
{
	int i;

	strlcpy(world->filename, filename, sizeof(world->filename));
	VectorCopy(mins, world->mins);
	VectorCopy(maxs, world->maxs);

	// the areagrid_marknumber is not allowed to be 0
	if (world->areagrid_marknumber < 1)
		world->areagrid_marknumber = 1;
	// choose either the world box size, or a larger box to ensure the grid isn't too fine
	world->areagrid_size[0] = max(world->areagrid_maxs[0] - world->areagrid_mins[0], AREA_GRID * sv_areagrid_mingridsize.value);
	world->areagrid_size[1] = max(world->areagrid_maxs[1] - world->areagrid_mins[1], AREA_GRID * sv_areagrid_mingridsize.value);
	world->areagrid_size[2] = max(world->areagrid_maxs[2] - world->areagrid_mins[2], AREA_GRID * sv_areagrid_mingridsize.value);
	// figure out the corners of such a box, centered at the center of the world box
	world->areagrid_mins[0] = (world->areagrid_mins[0] + world->areagrid_maxs[0] - world->areagrid_size[0]) * 0.5f;
	world->areagrid_mins[1] = (world->areagrid_mins[1] + world->areagrid_maxs[1] - world->areagrid_size[1]) * 0.5f;
	world->areagrid_mins[2] = (world->areagrid_mins[2] + world->areagrid_maxs[2] - world->areagrid_size[2]) * 0.5f;
	world->areagrid_maxs[0] = (world->areagrid_mins[0] + world->areagrid_maxs[0] + world->areagrid_size[0]) * 0.5f;
	world->areagrid_maxs[1] = (world->areagrid_mins[1] + world->areagrid_maxs[1] + world->areagrid_size[1]) * 0.5f;
	world->areagrid_maxs[2] = (world->areagrid_mins[2] + world->areagrid_maxs[2] + world->areagrid_size[2]) * 0.5f;
	// now calculate the actual useful info from that
	VectorNegate(world->areagrid_mins, world->areagrid_bias);
	world->areagrid_scale[0] = AREA_GRID / world->areagrid_size[0];
	world->areagrid_scale[1] = AREA_GRID / world->areagrid_size[1];
	world->areagrid_scale[2] = AREA_GRID / world->areagrid_size[2];
	World_ClearLink(&world->areagrid_outside);
	for (i = 0;i < AREA_GRIDNODES;i++)
		World_ClearLink(&world->areagrid[i]);
	if (developer.integer >= 10)
		Con_Printf("areagrid settings: divisions %ix%ix1 : box %f %f %f : %f %f %f size %f %f %f grid %f %f %f (mingrid %f)\n", AREA_GRID, AREA_GRID, world->areagrid_mins[0], world->areagrid_mins[1], world->areagrid_mins[2], world->areagrid_maxs[0], world->areagrid_maxs[1], world->areagrid_maxs[2], world->areagrid_size[0], world->areagrid_size[1], world->areagrid_size[2], 1.0f / world->areagrid_scale[0], 1.0f / world->areagrid_scale[1], 1.0f / world->areagrid_scale[2], sv_areagrid_mingridsize.value);
}

/*
===============
World_UnlinkAll

===============
*/
void World_UnlinkAll(world_t *world)
{
	int i;
	link_t *grid;
	// unlink all entities one by one
	grid = &world->areagrid_outside;
	while (grid->next != grid)
		World_UnlinkEdict(PRVM_EDICT_NUM(grid->next->entitynumber));
	for (i = 0, grid = world->areagrid;i < AREA_GRIDNODES;i++, grid++)
		while (grid->next != grid)
			World_UnlinkEdict(PRVM_EDICT_NUM(grid->next->entitynumber));
}

/*
===============

===============
*/
void World_UnlinkEdict(prvm_edict_t *ent)
{
	int i;
	for (i = 0;i < ENTITYGRIDAREAS;i++)
	{
		if (ent->priv.server->areagrid[i].prev)
		{
			World_RemoveLink (&ent->priv.server->areagrid[i]);
			ent->priv.server->areagrid[i].prev = ent->priv.server->areagrid[i].next = NULL;
		}
	}
}

int World_EntitiesInBox(world_t *world, const vec3_t mins, const vec3_t maxs, int maxlist, prvm_edict_t **list)
{
	int numlist;
	link_t *grid;
	link_t *l;
	prvm_edict_t *ent;
	int igrid[3], igridmins[3], igridmaxs[3];

	// FIXME: if areagrid_marknumber wraps, all entities need their
	// ent->priv.server->areagridmarknumber reset
	world->areagrid_stats_calls++;
	world->areagrid_marknumber++;
	igridmins[0] = (int) floor((mins[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]);
	igridmins[1] = (int) floor((mins[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]);
	//igridmins[2] = (int) ((mins[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]);
	igridmaxs[0] = (int) floor((maxs[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) floor((maxs[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) ((maxs[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]) + 1;
	igridmins[0] = max(0, igridmins[0]);
	igridmins[1] = max(0, igridmins[1]);
	//igridmins[2] = max(0, igridmins[2]);
	igridmaxs[0] = min(AREA_GRID, igridmaxs[0]);
	igridmaxs[1] = min(AREA_GRID, igridmaxs[1]);
	//igridmaxs[2] = min(AREA_GRID, igridmaxs[2]);

	numlist = 0;
	// add entities not linked into areagrid because they are too big or
	// outside the grid bounds
	if (world->areagrid_outside.next != &world->areagrid_outside)
	{
		grid = &world->areagrid_outside;
		for (l = grid->next;l != grid;l = l->next)
		{
			ent = PRVM_EDICT_NUM(l->entitynumber);
			if (ent->priv.server->areagridmarknumber != world->areagrid_marknumber)
			{
				ent->priv.server->areagridmarknumber = world->areagrid_marknumber;
				if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs))
				{
					if (numlist < maxlist)
						list[numlist] = ent;
					numlist++;
				}
				world->areagrid_stats_entitychecks++;
			}
		}
	}
	// add grid linked entities
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = world->areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++)
		{
			if (grid->next != grid)
			{
				for (l = grid->next;l != grid;l = l->next)
				{
					ent = PRVM_EDICT_NUM(l->entitynumber);
					if (ent->priv.server->areagridmarknumber != world->areagrid_marknumber)
					{
						ent->priv.server->areagridmarknumber = world->areagrid_marknumber;
						if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs))
						{
							if (numlist < maxlist)
								list[numlist] = ent;
							numlist++;
						}
						//Con_Printf("%d %f %f %f %f %f %f : %d : %f %f %f %f %f %f\n", BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs), ent->priv.server->areamins[0], ent->priv.server->areamins[1], ent->priv.server->areamins[2], ent->priv.server->areamaxs[0], ent->priv.server->areamaxs[1], ent->priv.server->areamaxs[2], PRVM_NUM_FOR_EDICT(ent), mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
					}
					world->areagrid_stats_entitychecks++;
				}
			}
		}
	}
	return numlist;
}

void World_LinkEdict_AreaGrid(world_t *world, prvm_edict_t *ent)
{
	link_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3], gridnum, entitynumber = PRVM_NUM_FOR_EDICT(ent);

	if (entitynumber <= 0 || entitynumber >= prog->max_edicts || PRVM_EDICT_NUM(entitynumber) != ent)
	{
		Con_Printf ("World_LinkEdict_AreaGrid: invalid edict %p (edicts is %p, edict compared to prog->edicts is %i)\n", (void *)ent, (void *)prog->edicts, entitynumber);
		return;
	}

	igridmins[0] = (int) floor((ent->priv.server->areamins[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]);
	igridmins[1] = (int) floor((ent->priv.server->areamins[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]);
	//igridmins[2] = (int) floor((ent->priv.server->areamins[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]);
	igridmaxs[0] = (int) floor((ent->priv.server->areamaxs[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) floor((ent->priv.server->areamaxs[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) floor((ent->priv.server->areamaxs[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]) + 1;
	if (igridmins[0] < 0 || igridmaxs[0] > AREA_GRID || igridmins[1] < 0 || igridmaxs[1] > AREA_GRID || ((igridmaxs[0] - igridmins[0]) * (igridmaxs[1] - igridmins[1])) > ENTITYGRIDAREAS)
	{
		// wow, something outside the grid, store it as such
		World_InsertLinkBefore (&ent->priv.server->areagrid[0], &world->areagrid_outside, entitynumber);
		return;
	}

	gridnum = 0;
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = world->areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++, gridnum++)
			World_InsertLinkBefore (&ent->priv.server->areagrid[gridnum], grid, entitynumber);
	}
}

/*
===============
World_LinkEdict

===============
*/
void World_LinkEdict(world_t *world, prvm_edict_t *ent, const vec3_t mins, const vec3_t maxs)
{
	// unlink from old position first
	if (ent->priv.server->areagrid[0].prev)
		World_UnlinkEdict(ent);

	// don't add the world
	if (ent == prog->edicts)
		return;

	// don't add free entities
	if (ent->priv.server->free)
		return;

	VectorCopy(mins, ent->priv.server->areamins);
	VectorCopy(maxs, ent->priv.server->areamaxs);
	World_LinkEdict_AreaGrid(world, ent);
}
