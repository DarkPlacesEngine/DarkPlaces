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

line of sight checks trace->crosscontent, but bullets don't

*/

/*
typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;
*/


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
//#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

#define	EDICT_FROM_AREA(l) ((edict_t *)((byte *)l - (int)&(((edict_t *)0)->area)))

//============================================================================

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
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


typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
} moveclip_t;


/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i=0 ; i<6 ; i++)
	{
		box_clipnodes[i].planenum = i;
		
		side = i&1;
		
		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;
		
		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}
	
}


/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t	*SV_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}



/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity (edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset)
{
	model_t		*model;
	vec3_t		size;
	vec3_t		hullmins, hullmaxs;
	hull_t		*hull;

// decide which clipping hull to use, based on the size
	if (ent->v.solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		if (ent->v.movetype != MOVETYPE_PUSH)
			Host_Error ("SOLID_BSP without MOVETYPE_PUSH");

		model = sv.models[ (int)ent->v.modelindex ];
		Mod_CheckLoaded(model);

		// LordHavoc: fixed SOLID_BSP error message
		if (!model || model->type != mod_brush)
		{
			Con_Printf ("SOLID_BSP with a non bsp model, entity dump:\n");
			ED_Print (ent);
			Host_Error ("SOLID_BSP with a non bsp model\n");
		}

		VectorSubtract (maxs, mins, size);
		// LordHavoc: FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		if (sv.worldmodel->ishlbsp)
		{
			if (size[0] < 3)
				hull = &model->hulls[0]; // 0x0x0
			else if (size[0] <= 32)
			{
				if (size[2] < 54) // pick the nearest of 36 or 72
					hull = &model->hulls[3]; // 32x32x36
				else
					hull = &model->hulls[1]; // 32x32x72
			}
			else
				hull = &model->hulls[2]; // 64x64x64
		}
		else
		{
			if (size[0] < 3)
				hull = &model->hulls[0]; // 0x0x0
			else if (size[0] <= 32)
				hull = &model->hulls[1]; // 32x32x56
			else
				hull = &model->hulls[2]; // 64x64x88
		}

// calculate an offset value to center the origin
		VectorSubtract (hull->clip_mins, mins, offset);
		VectorAdd (offset, ent->v.origin, offset);
	}
	else
	{	// create a temp hull from bounding box sizes

		VectorSubtract (ent->v.mins, maxs, hullmins);
		VectorSubtract (ent->v.maxs, mins, hullmaxs);
		hull = SV_HullForBox (hullmins, hullmaxs);

		VectorCopy (ent->v.origin, offset);
	}


	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

static	areanode_t	sv_areanodes[AREA_NODES];
static	int			sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

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

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	SV_InitBoxHull ();

	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	Mod_CheckLoaded(sv.worldmodel);
	SV_CreateAreaNode (0, sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


/*
====================
SV_TouchLinks
====================
*/
void SV_TouchLinks ( edict_t *ent, areanode_t *node )
{
	link_t		*l, *next;
	edict_t		*touch;
	int			old_self, old_other;

loc0:
// touch linked edicts
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
		 || ent->v.absmin[1] > touch->v.absmax[1]
		 || ent->v.absmin[2] > touch->v.absmax[2]
		 || ent->v.absmax[0] < touch->v.absmin[0]
		 || ent->v.absmax[1] < touch->v.absmin[1]
		 || ent->v.absmax[2] < touch->v.absmin[2])
			continue;
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = sv.time;
		PR_ExecuteProgram (touch->v.touch, "");

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	// LordHavoc: optimized recursion
//	if (ent->v.absmax[node->axis] > node->dist) SV_TouchLinks (ent, node->children[0]);
//	if (ent->v.absmin[node->axis] < node->dist) SV_TouchLinks (ent, node->children[1]);
	if (ent->v.absmax[node->axis] > node->dist)
	{
		if (ent->v.absmin[node->axis] < node->dist)
			SV_TouchLinks(ent, node->children[1]); // order reversed to reduce code
		node = node->children[0];
		goto loc0;
	}
	else
	{
		if (ent->v.absmin[node->axis] < node->dist)
		{
			node = node->children[1];
			goto loc0;
		}
	}
}


/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;

	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position
		
	if (ent == sv.edicts)
		return;		// don't add the world

	if (ent->free)
		return;

// set the abs box

// LordHavoc: enabling rotating bmodels
	if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
	{
		// expand for rotation
		float		max, v;
		int			i;

		max = DotProduct(ent->v.mins, ent->v.mins);
		v = DotProduct(ent->v.maxs, ent->v.maxs);
		if (max < v)
			max = v;
		max = sqrt(max);
		/*
		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v = fabs(ent->v.mins[i]);
			if (max < v)
				max = v;
			v = fabs(ent->v.maxs[i]);
			if (max < v)
				max = v;
		}
		*/
		for (i=0 ; i<3 ; i++)
		{
			ent->v.absmin[i] = ent->v.origin[i] - max;
			ent->v.absmax[i] = ent->v.origin[i] + max;
		}
	}
	else
	{
		VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
		VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);
	}

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	}
	else
	{
		// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}

	if (ent->v.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}

// link it in

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);

// if touch_triggers, touch all entities at this node and descend for more
	if (touch_triggers)
		SV_TouchLinks ( ent, sv_areanodes );
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	while (num >= 0)
		num = hull->clipnodes[num].children[(hull->planes[hull->clipnodes[num].planenum].type < 3 ? p[hull->planes[hull->clipnodes[num].planenum].type] : DotProduct (hull->planes[hull->clipnodes[num].planenum].normal, p)) < hull->planes[hull->clipnodes[num].planenum].dist];

	return num;
}

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent);

	if (trace.startsolid)
		return sv.edicts;

	return NULL;
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

// LordHavoc: FIXME: this is not thread safe, if threading matters here, pass
// this as a struct to RecursiveHullCheck, RecursiveHullCheck_Impact, etc...
RecursiveHullCheckTraceInfo_t RecursiveHullCheckInfo;
#define RHC RecursiveHullCheckInfo

void SV_RecursiveHullCheck_Impact (mplane_t *plane, int side)
{
	// LordHavoc: using doubles for extra accuracy
	double t1, t2, frac;

	// LordHavoc: now that we have found the impact, recalculate the impact
	// point from scratch for maximum accuracy, with an epsilon bias on the
	// surface distance
	frac = plane->dist;
	if (side)
	{
		frac -= DIST_EPSILON;
		VectorNegate (plane->normal, RHC.trace->plane.normal);
		RHC.trace->plane.dist = -plane->dist;
	}
	else
	{
		frac += DIST_EPSILON;
		VectorCopy (plane->normal, RHC.trace->plane.normal);
		RHC.trace->plane.dist = plane->dist;
	}

	if (plane->type < 3)
	{
		t1 = RHC.start[plane->type] - frac;
		t2 = RHC.start[plane->type] + RHC.dist[plane->type] - frac;
	}
	else
	{
		t1 = plane->normal[0] * RHC.start[0] + plane->normal[1] * RHC.start[1] + plane->normal[2] * RHC.start[2] - frac;
		t2 = plane->normal[0] * (RHC.start[0] + RHC.dist[0]) + plane->normal[1] * (RHC.start[1] + RHC.dist[1]) + plane->normal[2] * (RHC.start[2] + RHC.dist[2]) - frac;
	}

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0);

	RHC.trace->fraction = frac;
	RHC.trace->endpos[0] = RHC.start[0] + frac * RHC.dist[0];
	RHC.trace->endpos[1] = RHC.start[1] + frac * RHC.dist[1];
	RHC.trace->endpos[2] = RHC.start[2] + frac * RHC.dist[2];
}

int SV_RecursiveHullCheck (int num, double p1f, double p2f, double p1[3], double p2[3])
{
	dclipnode_t	*node;
	int			side;
	double		midf, mid[3];
	// LordHavoc: FIXME: this is not thread safe...  if threading matters here,
	// remove the static prefixes
	static int ret;
	static mplane_t *plane;
	static double t1, t2, frac;

	// LordHavoc: a goto!  everyone flee in terror... :)
loc0:
	// check for empty
	if (num < 0)
	{
		RHC.trace->endcontents = num;
		if (RHC.trace->startcontents)
		{
			if (num == RHC.trace->startcontents)
				RHC.trace->allsolid = false;
			else
			{
				// if the first leaf is solid, set startsolid
				if (RHC.trace->allsolid)
					RHC.trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
		else
		{
			if (num != CONTENTS_SOLID)
			{
				RHC.trace->allsolid = false;
				if (num == CONTENTS_EMPTY)
					RHC.trace->inopen = true;
				else
					RHC.trace->inwater = true;
			}
			else
			{
				// if the first leaf is solid, set startsolid
				if (RHC.trace->allsolid)
					RHC.trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
	}

	// find the point distances
	node = RHC.hull->clipnodes + num;

	plane = RHC.hull->planes + node->planenum;
	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	// LordHavoc: rearranged the side/frac code
	if (t1 >= 0)
	{
		if (t2 >= 0)
		{
			num = node->children[0];
			goto loc0;
		}
		// put the crosspoint DIST_EPSILON pixels on the near side
		side = 0;
	}
	else
	{
		if (t2 < 0)
		{
			num = node->children[1];
			goto loc0;
		}
		// put the crosspoint DIST_EPSILON pixels on the near side
		side = 1;
	}

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0);

	midf = p1f + ((p2f - p1f) * frac);
	mid[0] = RHC.start[0] + midf * RHC.dist[0];
	mid[1] = RHC.start[1] + midf * RHC.dist[1];
	mid[2] = RHC.start[2] + midf * RHC.dist[2];

	// front side first
	ret = SV_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);
	if (ret != HULLCHECKSTATE_EMPTY)
		return ret; // solid or done
	ret = SV_RecursiveHullCheck (node->children[!side], midf, p2f, mid, p2);
	if (ret != HULLCHECKSTATE_SOLID)
		return ret; // empty or done

	// front is air and back is solid, this is the impact point...
	SV_RecursiveHullCheck_Impact(RHC.hull->planes + node->planenum, side);

	return HULLCHECKSTATE_DONE;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t trace;
	vec3_t offset, forward, left, up;
	double startd[3], endd[3], tempd[3];
	hull_t *hull;

// fill in a default trace
	memset (&trace, 0, sizeof(trace_t));
	trace.fraction = 1;
	trace.allsolid = true;

// get the clipping hull
	hull = SV_HullForEntity (ent, mins, maxs, offset);

	VectorSubtract(start, offset, startd);
	VectorSubtract(end, offset, endd);

	// rotate start and end into the models frame of reference
	if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
	{
		AngleVectorsFLU (ent->v.angles, forward, left, up);
		VectorCopy(startd, tempd);
		startd[0] = DotProduct (tempd, forward);
		startd[1] = DotProduct (tempd, left);
		startd[2] = DotProduct (tempd, up);
		VectorCopy(endd, tempd);
		endd[0] = DotProduct (tempd, forward);
		endd[1] = DotProduct (tempd, left);
		endd[2] = DotProduct (tempd, up);
	}

	VectorCopy(end, trace.endpos);

// trace a line through the appropriate clipping hull
	VectorCopy(startd, RecursiveHullCheckInfo.start);
	VectorSubtract(endd, startd, RecursiveHullCheckInfo.dist);
	RecursiveHullCheckInfo.hull = hull;
	RecursiveHullCheckInfo.trace = &trace;
	SV_RecursiveHullCheck (hull->firstclipnode, 0, 1, startd, endd);

	// if we hit, unrotate endpos and normal, and store the entity we hit
	if (trace.fraction != 1)
	{
		// rotate endpos back to world frame of reference
		if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
		{
			VectorNegate (ent->v.angles, offset);
			AngleVectorsFLU (offset, forward, left, up);

			VectorCopy (trace.endpos, tempd);
			trace.endpos[0] = DotProduct (tempd, forward);
			trace.endpos[1] = DotProduct (tempd, left);
			trace.endpos[2] = DotProduct (tempd, up);

			VectorCopy (trace.plane.normal, tempd);
			trace.plane.normal[0] = DotProduct (tempd, forward);
			trace.plane.normal[1] = DotProduct (tempd, left);
			trace.plane.normal[2] = DotProduct (tempd, up);
		}
		// fix offset
		VectorAdd (trace.endpos, offset, trace.endpos);
		trace.ent = ent;
	}
	else if (trace.startsolid)
		trace.ent = ent;

	return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;

	if (clip->trace.allsolid)
		return;
loc0:
// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v.solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (touch->v.solid == SOLID_TRIGGER)
			Host_Error ("Trigger in clipping list");

		if (clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->v.absmax[0]
		 || clip->boxmaxs[0] < touch->v.absmin[0]
		 || clip->boxmins[1] > touch->v.absmax[1]
		 || clip->boxmaxs[1] < touch->v.absmin[1]
		 || clip->boxmins[2] > touch->v.absmax[2]
		 || clip->boxmaxs[2] < touch->v.absmin[2])
			continue;

		if (clip->passedict)
		{
			if (clip->passedict->v.size[0] && !touch->v.size[0])
				continue;	// points never interact
		 	if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				continue;	// don't clip against owner
			// LordHavoc: corpse code
			if (clip->passedict->v.solid == SOLID_CORPSE && touch->v.solid == SOLID_SLIDEBOX)
				continue;
			if (clip->passedict->v.solid == SOLID_SLIDEBOX && touch->v.solid == SOLID_CORPSE)
				continue;
		}

		// might interact, so do an exact clip
		if ((int)touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity (touch, clip->start, clip->mins, clip->maxs, clip->end);
		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		if (clip->trace.allsolid)
			return;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	// LordHavoc: optimized recursion
//	if (clip->boxmaxs[node->axis] > node->dist) SV_ClipToLinks(node->children[0], clip);
//	if (clip->boxmins[node->axis] < node->dist) SV_ClipToLinks(node->children[1], clip);
	if (clip->boxmaxs[node->axis] > node->dist)
	{
		if (clip->boxmins[node->axis] < node->dist)
			SV_ClipToLinks(node->children[1], clip);
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
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;

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
#endif
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t	clip;
	int			i;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

	if (type == MOVE_MISSILE)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy (clip.mins, clip.mins2);
		VectorCopy (clip.maxs, clip.maxs2);
	}

	// clip to world
	clip.trace = SV_ClipMoveToEntity (sv.edicts, start, mins, maxs, end);

	// clip to entities
	if (!clip.trace.allsolid)
	{
		// create the bounding box of the entire move
		SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

		SV_ClipToLinks ( sv_areanodes, &clip );
	}

	return clip.trace;
}
