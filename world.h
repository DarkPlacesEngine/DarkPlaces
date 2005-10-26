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


// called after the world model has been loaded, before linking any entities
void SV_ClearWorld (void);

// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
void SV_UnlinkEdict (prvm_edict_t *ent);

// Needs to be called any time an entity changes origin, mins, maxs, or solid
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers
void SV_LinkEdict (prvm_edict_t *ent, qboolean touch_triggers);

// returns true if the entity is in solid currently
int SV_TestEntityPosition (prvm_edict_t *ent);

// returns list of entities touching a box
int SV_EntitiesInBox(vec3_t mins, vec3_t maxs, int maxlist, prvm_edict_t **list);

// mins and maxs are relative
// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)
trace_t SV_Move(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict);

#define SV_PointSuperContents(point) (SV_Move((point), vec3_origin, vec3_origin, (point), sv_gameplayfix_swiminbmodels.integer ? MOVE_NOMONSTERS : MOVE_WORLDONLY, NULL).startsupercontents)

#endif

