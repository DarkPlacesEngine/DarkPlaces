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

typedef struct
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct
{
	// if true, the entire trace was in solid
	qboolean	allsolid;
	// if true, the initial point was in solid
	qboolean	startsolid;
	// if true, the trace passed through empty somewhere
	qboolean	inopen;
	// if true, the trace passed through water somewhere
	qboolean	inwater;
	// fraction of the total distance that was traveled before impact
	// (1.0 = did not hit anything)
	double		fraction;
	// final position
	double		endpos[3];
	// surface normal at impact
	plane_t		plane;
	// entity the surface is on
	edict_t		*ent;
	// if not zero, treats this value as empty, and all others as solid (impact
	// on content change)
	int			startcontents;
	// the contents that was hit at the end or impact point
	int			endcontents;
}
trace_t;


#define	MOVE_NORMAL		0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2


void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v.modified

void SV_LinkEdict (edict_t *ent, qboolean touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v.modified
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls prog functions for the intersected triggers

extern int SV_HullPointContents (hull_t *hull, int num, vec3_t p);
// LordHavoc: waste of time to wrap it
//int SV_PointContents (vec3_t p);
#define SV_PointContents(testpoint) SV_HullPointContents(&sv.worldmodel->hulls[0], 0, (testpoint))
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all
// the non-true version remaps the water current contents to content_water

edict_t	*SV_TestEntityPosition (edict_t *ent);

trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict);
// mins and maxs are reletive

// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)

int SV_RecursiveHullCheck (int num, double p1f, double p2f, double p1[3], double p2[3]);

typedef struct
{
	hull_t *hull;
	trace_t *trace;
	double start[3];
	double dist[3];
}
RecursiveHullCheckTraceInfo_t;

// LordHavoc: FIXME: this is not thread safe, if threading matters here, pass
// this as a struct to RecursiveHullCheck, RecursiveHullCheck_Impact, etc...
extern RecursiveHullCheckTraceInfo_t RecursiveHullCheckInfo;
