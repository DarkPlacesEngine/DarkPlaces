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

#ifndef PROGS_H
#define PROGS_H
#include "pr_comp.h"			// defs shared with qcc

#define ENTITYGRIDAREAS 16
#define MAX_ENTITYCLUSTERS 16

#define JOINTTYPE_POINT 1
#define JOINTTYPE_HINGE 2
#define JOINTTYPE_SLIDER 3
#define JOINTTYPE_UNIVERSAL 4
#define JOINTTYPE_HINGE2 5
#define JOINTTYPE_FIXED -1

#define ODEFUNC_ENABLE			1
#define ODEFUNC_DISABLE			2
#define ODEFUNC_RELFORCEATPOS	3
#define ODEFUNC_RELTORQUE		4

typedef struct edict_odefunc_s
{
	int type;
	vec3_t v1;
	vec3_t v2;
	struct edict_odefunc_s *next;
}edict_odefunc_t;

typedef struct edict_engineprivate_s
{
	// true if this edict is unused
	qboolean free;
	// sv.time when the object was freed (to prevent early reuse which could
	// mess up client interpolation or obscure severe QuakeC bugs)
	float freetime;
	// mark for the leak detector
	int mark;
	// place in the code where it was allocated (for the leak detector)
	const char *allocation_origin;
	// initially false to prevent projectiles from moving on their first frame
	// (even if they were spawned by an synchronous client think)
	qboolean move;

	// cached cluster links for quick stationary object visibility checking
	vec3_t cullmins, cullmaxs;
	int pvs_numclusters;
	int pvs_clusterlist[MAX_ENTITYCLUSTERS];

	// physics grid areas this edict is linked into
	link_t areagrid[ENTITYGRIDAREAS];
	// since the areagrid can have multiple references to one entity,
	// we should avoid extensive checking on entities already encountered
	int areagridmarknumber;
	// mins/maxs passed to World_LinkEdict
	vec3_t areamins, areamaxs;

	// PROTOCOL_QUAKE, PROTOCOL_QUAKEDP, PROTOCOL_NEHAHRAMOVIE, PROTOCOL_QUAKEWORLD
	// baseline values
	entity_state_t baseline;

	// LordHavoc: gross hack to make floating items still work
	int suspendedinairflag;

	// cached position to avoid redundant SV_CheckWaterTransition calls on monsters
	qboolean waterposition_forceupdate; // force an update on this entity (set by SV_PushMove code for moving water entities)
	vec3_t waterposition_origin; // updates whenever this changes

	// used by PushMove to keep track of where objects were before they were
	// moved, in case they need to be moved back
	vec3_t moved_from;
	vec3_t moved_fromangles;

	framegroupblend_t framegroupblend[MAX_FRAMEGROUPBLENDS];
	frameblend_t frameblend[MAX_FRAMEBLENDS];
	skeleton_t skeleton;

	// physics parameters
	qboolean ode_physics;
	void *ode_body;
	void *ode_geom;
	void *ode_joint;
	float *ode_vertex3f;
	int *ode_element3i;
	int ode_numvertices;
	int ode_numtriangles;
	edict_odefunc_t *ode_func;
	vec3_t ode_mins;
	vec3_t ode_maxs;
	vec_t ode_mass;
	vec3_t ode_origin;
	vec3_t ode_velocity;
	vec3_t ode_angles;
	vec3_t ode_avelocity;
	qboolean ode_gravity;
	int ode_modelindex;
	vec_t ode_movelimit; // smallest component of (maxs[]-mins[])
	matrix4x4_t ode_offsetmatrix;
	matrix4x4_t ode_offsetimatrix;
	int ode_joint_type;
	int ode_joint_enemy;
	int ode_joint_aiment;
	vec3_t ode_joint_origin; // joint anchor
	vec3_t ode_joint_angles; // joint axis
	vec3_t ode_joint_velocity; // second joint axis
	vec3_t ode_joint_movedir; // parameters
	void *ode_massbuf;
}
edict_engineprivate_t;

#endif
