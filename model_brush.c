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

#include "quakedef.h"
#include "image.h"
#include "r_shadow.h"
#include "winding.h"
#include "curves.h"

// note: model_shared.c sets up r_notexture, and r_surf_notexture

qbyte mod_q1bsp_novis[(MAX_MAP_LEAFS + 7)/ 8];

//cvar_t r_subdivide_size = {CVAR_SAVE, "r_subdivide_size", "128"};
cvar_t halflifebsp = {0, "halflifebsp", "0"};
cvar_t r_novis = {0, "r_novis", "0"};
cvar_t r_miplightmaps = {CVAR_SAVE, "r_miplightmaps", "0"};
cvar_t r_lightmaprgba = {0, "r_lightmaprgba", "1"};
cvar_t r_nosurftextures = {0, "r_nosurftextures", "0"};
cvar_t mod_q3bsp_curves_subdivide_level = {0, "mod_q3bsp_curves_subdivide_level", "2"};
cvar_t mod_q3bsp_curves_collisions = {0, "mod_q3bsp_curves_collisions", "1"};
cvar_t mod_q3bsp_optimizedtraceline = {0, "mod_q3bsp_optimizedtraceline", "1"};
cvar_t mod_q3bsp_debugtracebrush = {0, "mod_q3bsp_debugtracebrush", "0"};

static void Mod_Q1BSP_Collision_Init (void);
void Mod_BrushInit(void)
{
//	Cvar_RegisterVariable(&r_subdivide_size);
	Cvar_RegisterVariable(&halflifebsp);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_miplightmaps);
	Cvar_RegisterVariable(&r_lightmaprgba);
	Cvar_RegisterVariable(&r_nosurftextures);
	Cvar_RegisterVariable(&mod_q3bsp_curves_subdivide_level);
	Cvar_RegisterVariable(&mod_q3bsp_curves_collisions);
	Cvar_RegisterVariable(&mod_q3bsp_optimizedtraceline);
	Cvar_RegisterVariable(&mod_q3bsp_debugtracebrush);
	memset(mod_q1bsp_novis, 0xff, sizeof(mod_q1bsp_novis));
	Mod_Q1BSP_Collision_Init();
}

static mleaf_t *Mod_Q1BSP_PointInLeaf(model_t *model, const vec3_t p)
{
	mnode_t *node;

	if (model == NULL)
		return NULL;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];

	return (mleaf_t *)node;
}

static void Mod_Q1BSP_AmbientSoundLevelsForPoint(model_t *model, const vec3_t p, qbyte *out, int outsize)
{
	int i;
	mleaf_t *leaf;
	leaf = Mod_Q1BSP_PointInLeaf(model, p);
	if (leaf)
	{
		i = min(outsize, (int)sizeof(leaf->ambient_sound_level));;
		if (i)
		{
			memcpy(out, leaf->ambient_sound_level, i);
			out += i;
			outsize -= i;
		}
	}
	if (outsize)
		memset(out, 0, outsize);
}


static int Mod_Q1BSP_BoxTouchingPVS_RecursiveBSPNode(const model_t *model, const mnode_t *node, const qbyte *pvs, const vec3_t mins, const vec3_t maxs)
{
	int leafnum;
loc0:
	if (node->contents < 0)
	{
		// leaf
		if (node->contents == CONTENTS_SOLID)
			return false;
		leafnum = (mleaf_t *)node - model->brushq1.leafs - 1;
		return pvs[leafnum >> 3] & (1 << (leafnum & 7));
	}

	// node - recurse down the BSP tree
	switch (BoxOnPlaneSide(mins, maxs, node->plane))
	{
	case 1: // front
		node = node->children[0];
		goto loc0;
	case 2: // back
		node = node->children[1];
		goto loc0;
	default: // crossing
		if (node->children[0]->contents != CONTENTS_SOLID)
			if (Mod_Q1BSP_BoxTouchingPVS_RecursiveBSPNode(model, node->children[0], pvs, mins, maxs))
				return true;
		node = node->children[1];
		goto loc0;
	}
	// never reached
	return false;
}

int Mod_Q1BSP_BoxTouchingPVS(model_t *model, const qbyte *pvs, const vec3_t mins, const vec3_t maxs)
{
	return Mod_Q1BSP_BoxTouchingPVS_RecursiveBSPNode(model, model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode, pvs, mins, maxs);
}

/*
static int Mod_Q1BSP_PointContents(model_t *model, const vec3_t p)
{
	mnode_t *node;

	if (model == NULL)
		return CONTENTS_EMPTY;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];

	return ((mleaf_t *)node)->contents;
}
*/

typedef struct findnonsolidlocationinfo_s
{
	vec3_t center;
	vec_t radius;
	vec3_t nudge;
	vec_t bestdist;
	model_t *model;
}
findnonsolidlocationinfo_t;

#if 0
extern cvar_t samelevel;
#endif
static void Mod_Q1BSP_FindNonSolidLocation_r_Leaf(findnonsolidlocationinfo_t *info, mleaf_t *leaf)
{
	int i, surfnum, k, *tri, *mark;
	float dist, f, vert[3][3], edge[3][3], facenormal[3], edgenormal[3][3], point[3];
#if 0
	float surfnormal[3];
#endif
	msurface_t *surf;
	for (surfnum = 0, mark = leaf->firstmarksurface;surfnum < leaf->nummarksurfaces;surfnum++, mark++)
	{
		surf = info->model->brushq1.surfaces + *mark;
		if (surf->flags & SURF_SOLIDCLIP)
		{
#if 0
			VectorCopy(surf->plane->normal, surfnormal);
			if (surf->flags & SURF_PLANEBACK)
				VectorNegate(surfnormal, surfnormal);
#endif
			for (k = 0;k < surf->mesh.num_triangles;k++)
			{
				tri = surf->mesh.data_element3i + k * 3;
				VectorCopy((surf->mesh.data_vertex3f + tri[0] * 3), vert[0]);
				VectorCopy((surf->mesh.data_vertex3f + tri[1] * 3), vert[1]);
				VectorCopy((surf->mesh.data_vertex3f + tri[2] * 3), vert[2]);
				VectorSubtract(vert[1], vert[0], edge[0]);
				VectorSubtract(vert[2], vert[1], edge[1]);
				CrossProduct(edge[1], edge[0], facenormal);
				if (facenormal[0] || facenormal[1] || facenormal[2])
				{
					VectorNormalize(facenormal);
#if 0
					if (VectorDistance(facenormal, surfnormal) > 0.01f)
						Con_Printf("a2! %f %f %f != %f %f %f\n", facenormal[0], facenormal[1], facenormal[2], surfnormal[0], surfnormal[1], surfnormal[2]);
#endif
					f = DotProduct(info->center, facenormal) - DotProduct(vert[0], facenormal);
					if (f <= info->bestdist && f >= -info->bestdist)
					{
						VectorSubtract(vert[0], vert[2], edge[2]);
						VectorNormalize(edge[0]);
						VectorNormalize(edge[1]);
						VectorNormalize(edge[2]);
						CrossProduct(facenormal, edge[0], edgenormal[0]);
						CrossProduct(facenormal, edge[1], edgenormal[1]);
						CrossProduct(facenormal, edge[2], edgenormal[2]);
#if 0
						if (samelevel.integer & 1)
							VectorNegate(edgenormal[0], edgenormal[0]);
						if (samelevel.integer & 2)
							VectorNegate(edgenormal[1], edgenormal[1]);
						if (samelevel.integer & 4)
							VectorNegate(edgenormal[2], edgenormal[2]);
						for (i = 0;i < 3;i++)
							if (DotProduct(vert[0], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f
							 || DotProduct(vert[1], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f
							 || DotProduct(vert[2], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f)
								Con_Printf("a! %i : %f %f %f (%f %f %f)\n", i, edgenormal[i][0], edgenormal[i][1], edgenormal[i][2], facenormal[0], facenormal[1], facenormal[2]);
#endif
						// face distance
						if (DotProduct(info->center, edgenormal[0]) < DotProduct(vert[0], edgenormal[0])
						 && DotProduct(info->center, edgenormal[1]) < DotProduct(vert[1], edgenormal[1])
						 && DotProduct(info->center, edgenormal[2]) < DotProduct(vert[2], edgenormal[2]))
						{
							// we got lucky, the center is within the face
							dist = DotProduct(info->center, facenormal) - DotProduct(vert[0], facenormal);
							if (dist < 0)
							{
								dist = -dist;
								if (info->bestdist > dist)
								{
									info->bestdist = dist;
									VectorScale(facenormal, (info->radius - -dist), info->nudge);
								}
							}
							else
							{
								if (info->bestdist > dist)
								{
									info->bestdist = dist;
									VectorScale(facenormal, (info->radius - dist), info->nudge);
								}
							}
						}
						else
						{
							// check which edge or vertex the center is nearest
							for (i = 0;i < 3;i++)
							{
								f = DotProduct(info->center, edge[i]);
								if (f >= DotProduct(vert[0], edge[i])
								 && f <= DotProduct(vert[1], edge[i]))
								{
									// on edge
									VectorMA(info->center, -f, edge[i], point);
									dist = sqrt(DotProduct(point, point));
									if (info->bestdist > dist)
									{
										info->bestdist = dist;
										VectorScale(point, (info->radius / dist), info->nudge);
									}
									// skip both vertex checks
									// (both are further away than this edge)
									i++;
								}
								else
								{
									// not on edge, check first vertex of edge
									VectorSubtract(info->center, vert[i], point);
									dist = sqrt(DotProduct(point, point));
									if (info->bestdist > dist)
									{
										info->bestdist = dist;
										VectorScale(point, (info->radius / dist), info->nudge);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static void Mod_Q1BSP_FindNonSolidLocation_r(findnonsolidlocationinfo_t *info, mnode_t *node)
{
	if (node->contents)
	{
		if (((mleaf_t *)node)->nummarksurfaces)
			Mod_Q1BSP_FindNonSolidLocation_r_Leaf(info, (mleaf_t *)node);
	}
	else
	{
		float f = PlaneDiff(info->center, node->plane);
		if (f >= -info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[0]);
		if (f <= info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[1]);
	}
}

static void Mod_Q1BSP_FindNonSolidLocation(model_t *model, const vec3_t in, vec3_t out, float radius)
{
	int i;
	findnonsolidlocationinfo_t info;
	if (model == NULL)
	{
		VectorCopy(in, out);
		return;
	}
	VectorCopy(in, info.center);
	info.radius = radius;
	info.model = model;
	i = 0;
	do
	{
		VectorClear(info.nudge);
		info.bestdist = radius;
		Mod_Q1BSP_FindNonSolidLocation_r(&info, model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode);
		VectorAdd(info.center, info.nudge, info.center);
	}
	while (info.bestdist < radius && ++i < 10);
	VectorCopy(info.center, out);
}

int Mod_Q1BSP_SuperContentsFromNativeContents(model_t *model, int nativecontents)
{
	switch(nativecontents)
	{
		case CONTENTS_EMPTY:
			return 0;
		case CONTENTS_SOLID:
			return SUPERCONTENTS_SOLID;
		case CONTENTS_WATER:
			return SUPERCONTENTS_WATER;
		case CONTENTS_SLIME:
			return SUPERCONTENTS_SLIME;
		case CONTENTS_LAVA:
			return SUPERCONTENTS_LAVA;
		case CONTENTS_SKY:
			return SUPERCONTENTS_SKY;
	}
	return 0;
}

int Mod_Q1BSP_NativeContentsFromSuperContents(model_t *model, int supercontents)
{
	if (supercontents & SUPERCONTENTS_SOLID)
		return CONTENTS_SOLID;
	if (supercontents & SUPERCONTENTS_SKY)
		return CONTENTS_SKY;
	if (supercontents & SUPERCONTENTS_LAVA)
		return CONTENTS_LAVA;
	if (supercontents & SUPERCONTENTS_SLIME)
		return CONTENTS_SLIME;
	if (supercontents & SUPERCONTENTS_WATER)
		return CONTENTS_WATER;
	return CONTENTS_EMPTY;
}

typedef struct
{
	// the hull we're tracing through
	const hull_t *hull;

	// the trace structure to fill in
	trace_t *trace;

	// start, end, and end - start (in model space)
	double start[3];
	double end[3];
	double dist[3];
}
RecursiveHullCheckTraceInfo_t;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

static int Mod_Q1BSP_RecursiveHullCheck(RecursiveHullCheckTraceInfo_t *t, int num, double p1f, double p2f, double p1[3], double p2[3])
{
	// status variables, these don't need to be saved on the stack when
	// recursing...  but are because this should be thread-safe
	// (note: tracing against a bbox is not thread-safe, yet)
	int ret;
	mplane_t *plane;
	double t1, t2;

	// variables that need to be stored on the stack when recursing
	dclipnode_t *node;
	int side;
	double midf, mid[3];

	// LordHavoc: a goto!  everyone flee in terror... :)
loc0:
	// check for empty
	if (num < 0)
	{
		num = Mod_Q1BSP_SuperContentsFromNativeContents(NULL, num);
		if (!t->trace->startfound)
		{
			t->trace->startfound = true;
			t->trace->startsupercontents |= num;
		}
		if (num & SUPERCONTENTS_LIQUIDSMASK)
			t->trace->inwater = true;
		if (num == 0)
			t->trace->inopen = true;
		if (num & t->trace->hitsupercontentsmask)
		{
			// if the first leaf is solid, set startsolid
			if (t->trace->allsolid)
				t->trace->startsolid = true;
#if COLLISIONPARANOID >= 3
			Con_Printf("S");
#endif
			return HULLCHECKSTATE_SOLID;
		}
		else
		{
			t->trace->allsolid = false;
#if COLLISIONPARANOID >= 3
			Con_Printf("E");
#endif
			return HULLCHECKSTATE_EMPTY;
		}
	}

	// find the point distances
	node = t->hull->clipnodes + num;

	plane = t->hull->planes + node->planenum;
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

	if (t1 < 0)
	{
		if (t2 < 0)
		{
#if COLLISIONPARANOID >= 3
			Con_Printf("<");
#endif
			num = node->children[1];
			goto loc0;
		}
		side = 1;
	}
	else
	{
		if (t2 >= 0)
		{
#if COLLISIONPARANOID >= 3
			Con_Printf(">");
#endif
			num = node->children[0];
			goto loc0;
		}
		side = 0;
	}

	// the line intersects, find intersection point
	// LordHavoc: this uses the original trace for maximum accuracy
#if COLLISIONPARANOID >= 3
	Con_Printf("M");
#endif
	if (plane->type < 3)
	{
		t1 = t->start[plane->type] - plane->dist;
		t2 = t->end[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, t->start) - plane->dist;
		t2 = DotProduct (plane->normal, t->end) - plane->dist;
	}

	midf = t1 / (t1 - t2);
	midf = bound(p1f, midf, p2f);
	VectorMA(t->start, midf, t->dist, mid);

	// recurse both sides, front side first
	ret = Mod_Q1BSP_RecursiveHullCheck(t, node->children[side], p1f, midf, p1, mid);
	// if this side is not empty, return what it is (solid or done)
	if (ret != HULLCHECKSTATE_EMPTY)
		return ret;

	ret = Mod_Q1BSP_RecursiveHullCheck(t, node->children[side ^ 1], midf, p2f, mid, p2);
	// if other side is not solid, return what it is (empty or done)
	if (ret != HULLCHECKSTATE_SOLID)
		return ret;

	// front is air and back is solid, this is the impact point...
	if (side)
	{
		t->trace->plane.dist = -plane->dist;
		VectorNegate (plane->normal, t->trace->plane.normal);
	}
	else
	{
		t->trace->plane.dist = plane->dist;
		VectorCopy (plane->normal, t->trace->plane.normal);
	}

	// calculate the true fraction
	t1 = DotProduct(t->trace->plane.normal, t->start) - t->trace->plane.dist;
	t2 = DotProduct(t->trace->plane.normal, t->end) - t->trace->plane.dist;
	midf = t1 / (t1 - t2);
	t->trace->realfraction = bound(0, midf, 1);

	// calculate the return fraction which is nudged off the surface a bit
	midf = (t1 - DIST_EPSILON) / (t1 - t2);
	t->trace->fraction = bound(0, midf, 1);

#if COLLISIONPARANOID >= 3
	Con_Printf("D");
#endif
	return HULLCHECKSTATE_DONE;
}

#if COLLISIONPARANOID < 2
static int Mod_Q1BSP_RecursiveHullCheckPoint(RecursiveHullCheckTraceInfo_t *t, int num)
{
	while (num >= 0)
		num = t->hull->clipnodes[num].children[(t->hull->planes[t->hull->clipnodes[num].planenum].type < 3 ? t->start[t->hull->planes[t->hull->clipnodes[num].planenum].type] : DotProduct(t->hull->planes[t->hull->clipnodes[num].planenum].normal, t->start)) < t->hull->planes[t->hull->clipnodes[num].planenum].dist];
	num = Mod_Q1BSP_SuperContentsFromNativeContents(NULL, num);
	t->trace->startsupercontents |= num;
	if (num & SUPERCONTENTS_LIQUIDSMASK)
		t->trace->inwater = true;
	if (num == 0)
		t->trace->inopen = true;
	if (num & t->trace->hitsupercontentsmask)
	{
		t->trace->allsolid = t->trace->startsolid = true;
		return HULLCHECKSTATE_SOLID;
	}
	else
	{
		t->trace->allsolid = t->trace->startsolid = false;
		return HULLCHECKSTATE_EMPTY;
	}
}
#endif

static void Mod_Q1BSP_TraceBox(struct model_s *model, int frame, trace_t *trace, const vec3_t boxstartmins, const vec3_t boxstartmaxs, const vec3_t boxendmins, const vec3_t boxendmaxs, int hitsupercontentsmask)
{
	// this function currently only supports same size start and end
	double boxsize[3];
	RecursiveHullCheckTraceInfo_t rhc;

	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	rhc.trace = trace;
	rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
	rhc.trace->fraction = 1;
	rhc.trace->realfraction = 1;
	rhc.trace->allsolid = true;
	VectorSubtract(boxstartmaxs, boxstartmins, boxsize);
	if (boxsize[0] < 3)
		rhc.hull = &model->brushq1.hulls[0]; // 0x0x0
	else if (model->brush.ishlbsp)
	{
		// LordHavoc: this has to have a minor tolerance (the .1) because of
		// minor float precision errors from the box being transformed around
		if (boxsize[0] < 32.1)
		{
			if (boxsize[2] < 54) // pick the nearest of 36 or 72
				rhc.hull = &model->brushq1.hulls[3]; // 32x32x36
			else
				rhc.hull = &model->brushq1.hulls[1]; // 32x32x72
		}
		else
			rhc.hull = &model->brushq1.hulls[2]; // 64x64x64
	}
	else
	{
		// LordHavoc: this has to have a minor tolerance (the .1) because of
		// minor float precision errors from the box being transformed around
		if (boxsize[0] < 32.1)
			rhc.hull = &model->brushq1.hulls[1]; // 32x32x56
		else
			rhc.hull = &model->brushq1.hulls[2]; // 64x64x88
	}
	VectorSubtract(boxstartmins, rhc.hull->clip_mins, rhc.start);
	VectorSubtract(boxendmins, rhc.hull->clip_mins, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
#if COLLISIONPARANOID >= 2
	Con_Printf("t(%f %f %f,%f %f %f,%i %f %f %f)", rhc.start[0], rhc.start[1], rhc.start[2], rhc.end[0], rhc.end[1], rhc.end[2], rhc.hull - model->brushq1.hulls, rhc.hull->clip_mins[0], rhc.hull->clip_mins[1], rhc.hull->clip_mins[2]);
	Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	Con_Printf("\n");
#else
	if (DotProduct(rhc.dist, rhc.dist))
		Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	else
		Mod_Q1BSP_RecursiveHullCheckPoint(&rhc, rhc.hull->firstclipnode);
#endif
}

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

static void Mod_Q1BSP_Collision_Init (void)
{
	int		i;
	int		side;

	//Set up the planes and clipnodes so that the six floats of a bounding box
	//can just be stored out and get a proper hull_t structure.

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0;i < 6;i++)
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

void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int boxsupercontents)
{
#if 1
	colbrushf_t cbox;
	colplanef_t cbox_planes[6];
	cbox.supercontents = boxsupercontents;
	cbox.numplanes = 6;
	cbox.numpoints = 0;
	cbox.numtriangles = 0;
	cbox.planes = cbox_planes;
	cbox.points = NULL;
	cbox.elements = NULL;
	cbox.markframe = 0;
	cbox.mins[0] = 0;
	cbox.mins[1] = 0;
	cbox.mins[2] = 0;
	cbox.maxs[0] = 0;
	cbox.maxs[1] = 0;
	cbox.maxs[2] = 0;
	cbox_planes[0].normal[0] =  1;cbox_planes[0].normal[1] =  0;cbox_planes[0].normal[2] =  0;cbox_planes[0].dist = cmaxs[0] - mins[0];
	cbox_planes[1].normal[0] = -1;cbox_planes[1].normal[1] =  0;cbox_planes[1].normal[2] =  0;cbox_planes[1].dist = maxs[0] - cmins[0];
	cbox_planes[2].normal[0] =  0;cbox_planes[2].normal[1] =  1;cbox_planes[2].normal[2] =  0;cbox_planes[2].dist = cmaxs[1] - mins[1];
	cbox_planes[3].normal[0] =  0;cbox_planes[3].normal[1] = -1;cbox_planes[3].normal[2] =  0;cbox_planes[3].dist = maxs[1] - cmins[1];
	cbox_planes[4].normal[0] =  0;cbox_planes[4].normal[1] =  0;cbox_planes[4].normal[2] =  1;cbox_planes[4].dist = cmaxs[2] - mins[2];
	cbox_planes[5].normal[0] =  0;cbox_planes[5].normal[1] =  0;cbox_planes[5].normal[2] = -1;cbox_planes[5].dist = maxs[2] - cmins[2];
	memset(trace, 0, sizeof(trace_t));
	trace->hitsupercontentsmask = hitsupercontentsmask;
	trace->fraction = 1;
	trace->realfraction = 1;
	Collision_TraceLineBrushFloat(trace, start, end, &cbox, &cbox);
#else
	RecursiveHullCheckTraceInfo_t rhc;
	// fill in a default trace
	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	//To keep everything totally uniform, bounding boxes are turned into small
	//BSP trees instead of being compared directly.
	// create a temp hull from bounding box sizes
	box_planes[0].dist = cmaxs[0] - mins[0];
	box_planes[1].dist = cmins[0] - maxs[0];
	box_planes[2].dist = cmaxs[1] - mins[1];
	box_planes[3].dist = cmins[1] - maxs[1];
	box_planes[4].dist = cmaxs[2] - mins[2];
	box_planes[5].dist = cmins[2] - maxs[2];
#if COLLISIONPARANOID >= 3
	Con_Printf("box_planes %f:%f %f:%f %f:%f\ncbox %f %f %f:%f %f %f\nbox %f %f %f:%f %f %f\n", box_planes[0].dist, box_planes[1].dist, box_planes[2].dist, box_planes[3].dist, box_planes[4].dist, box_planes[5].dist, cmins[0], cmins[1], cmins[2], cmaxs[0], cmaxs[1], cmaxs[2], mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
#endif
	// trace a line through the generated clipping hull
	//rhc.boxsupercontents = boxsupercontents;
	rhc.hull = &box_hull;
	rhc.trace = trace;
	rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
	rhc.trace->fraction = 1;
	rhc.trace->realfraction = 1;
	rhc.trace->allsolid = true;
	VectorCopy(start, rhc.start);
	VectorCopy(end, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
	Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	//VectorMA(rhc.start, rhc.trace->fraction, rhc.dist, rhc.trace->endpos);
	if (rhc.trace->startsupercontents)
		rhc.trace->startsupercontents = boxsupercontents;
#endif
}

static int Mod_Q1BSP_LightPoint_RecursiveBSPNode(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const mnode_t *node, float x, float y, float startz, float endz)
{
	int side, distz = endz - startz;
	float front, back;
	float mid;

loc0:
	if (node->contents < 0)
		return false;		// didn't hit anything

	switch (node->plane->type)
	{
	case PLANE_X:
		node = node->children[x < node->plane->dist];
		goto loc0;
	case PLANE_Y:
		node = node->children[y < node->plane->dist];
		goto loc0;
	case PLANE_Z:
		side = startz < node->plane->dist;
		if ((endz < node->plane->dist) == side)
		{
			node = node->children[side];
			goto loc0;
		}
		// found an intersection
		mid = node->plane->dist;
		break;
	default:
		back = front = x * node->plane->normal[0] + y * node->plane->normal[1];
		front += startz * node->plane->normal[2];
		back += endz * node->plane->normal[2];
		side = front < node->plane->dist;
		if ((back < node->plane->dist) == side)
		{
			node = node->children[side];
			goto loc0;
		}
		// found an intersection
		mid = startz + distz * (front - node->plane->dist) / (front - back);
		break;
	}

	// go down front side
	if (node->children[side]->contents >= 0 && Mod_Q1BSP_LightPoint_RecursiveBSPNode(ambientcolor, diffusecolor, diffusenormal, node->children[side], x, y, startz, mid))
		return true;	// hit something
	else
	{
		// check for impact on this node
		if (node->numsurfaces)
		{
			int i, ds, dt;
			msurface_t *surf;

			surf = cl.worldmodel->brushq1.surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surf++)
			{
				if (!(surf->flags & SURF_LIGHTMAP))
					continue;	// no lightmaps

				ds = (int) (x * surf->texinfo->vecs[0][0] + y * surf->texinfo->vecs[0][1] + mid * surf->texinfo->vecs[0][2] + surf->texinfo->vecs[0][3]);
				dt = (int) (x * surf->texinfo->vecs[1][0] + y * surf->texinfo->vecs[1][1] + mid * surf->texinfo->vecs[1][2] + surf->texinfo->vecs[1][3]);

				if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
					continue;

				ds -= surf->texturemins[0];
				dt -= surf->texturemins[1];

				if (ds > surf->extents[0] || dt > surf->extents[1])
					continue;

				if (surf->samples)
				{
					qbyte *lightmap;
					int maps, line3, size3, dsfrac = ds & 15, dtfrac = dt & 15, scale = 0, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
					line3 = ((surf->extents[0]>>4)+1)*3;
					size3 = ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting

					lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

					for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
						r00 += lightmap[      0] * scale;g00 += lightmap[      1] * scale;b00 += lightmap[      2] * scale;
						r01 += lightmap[      3] * scale;g01 += lightmap[      4] * scale;b01 += lightmap[      5] * scale;
						r10 += lightmap[line3+0] * scale;g10 += lightmap[line3+1] * scale;b10 += lightmap[line3+2] * scale;
						r11 += lightmap[line3+3] * scale;g11 += lightmap[line3+4] * scale;b11 += lightmap[line3+5] * scale;
						lightmap += size3;
					}

/*
LordHavoc: here's the readable version of the interpolation
code, not quite as easy for the compiler to optimize...

dsfrac is the X position in the lightmap pixel, * 16
dtfrac is the Y position in the lightmap pixel, * 16
r00 is top left corner, r01 is top right corner
r10 is bottom left corner, r11 is bottom right corner
g and b are the same layout.
r0 and r1 are the top and bottom intermediate results

first we interpolate the top two points, to get the top
edge sample

	r0 = (((r01-r00) * dsfrac) >> 4) + r00;
	g0 = (((g01-g00) * dsfrac) >> 4) + g00;
	b0 = (((b01-b00) * dsfrac) >> 4) + b00;

then we interpolate the bottom two points, to get the
bottom edge sample

	r1 = (((r11-r10) * dsfrac) >> 4) + r10;
	g1 = (((g11-g10) * dsfrac) >> 4) + g10;
	b1 = (((b11-b10) * dsfrac) >> 4) + b10;

then we interpolate the top and bottom samples to get the
middle sample (the one which was requested)

	r = (((r1-r0) * dtfrac) >> 4) + r0;
	g = (((g1-g0) * dtfrac) >> 4) + g0;
	b = (((b1-b0) * dtfrac) >> 4) + b0;
*/

					ambientcolor[0] += (float) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)) * (1.0f / 32768.0f);
					ambientcolor[1] += (float) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)) * (1.0f / 32768.0f);
					ambientcolor[2] += (float) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)) * (1.0f / 32768.0f);
				}
				return true; // success
			}
		}

		// go down back side
		node = node->children[side ^ 1];
		startz = mid;
		distz = endz - startz;
		goto loc0;
	}
}

void Mod_Q1BSP_LightPoint(model_t *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal)
{
	Mod_Q1BSP_LightPoint_RecursiveBSPNode(ambientcolor, diffusecolor, diffusenormal, cl.worldmodel->brushq1.nodes + cl.worldmodel->brushq1.hulls[0].firstclipnode, p[0], p[1], p[2], p[2] - 65536);
}

static void Mod_Q1BSP_DecompressVis(const qbyte *in, const qbyte *inend, qbyte *out, qbyte *outend)
{
	int c;
	qbyte *outstart = out;
	while (out < outend)
	{
		if (in == inend)
		{
			Con_DPrintf("Mod_Q1BSP_DecompressVis: input underrun on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, out - outstart, outend - outstart);
			return;
		}
		c = *in++;
		if (c)
			*out++ = c;
		else
		{
			if (in == inend)
			{
				Con_DPrintf("Mod_Q1BSP_DecompressVis: input underrun (during zero-run) on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, out - outstart, outend - outstart);
				return;
			}
			for (c = *in++;c > 0;c--)
			{
				if (out == outend)
				{
					Con_DPrintf("Mod_Q1BSP_DecompressVis: output overrun on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, out - outstart, outend - outstart);
					return;
				}
				*out++ = 0;
			}
		}
	}
}

static void Mod_Q1BSP_LoadTextures(lump_t *l)
{
	int i, j, k, num, max, altmax, mtwidth, mtheight, *dofs, incomplete;
	miptex_t *dmiptex;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	qbyte *data, *mtdata;
	char name[256];

	loadmodel->brushq1.textures = NULL;

	// add two slots for notexture walls and notexture liquids
	if (l->filelen)
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong (m->nummiptex);
		loadmodel->brushq1.numtextures = m->nummiptex + 2;
	}
	else
	{
		m = NULL;
		loadmodel->brushq1.numtextures = 2;
	}

	loadmodel->brushq1.textures = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numtextures * sizeof(texture_t));

	// fill out all slots with notexture
	for (i = 0, tx = loadmodel->brushq1.textures;i < loadmodel->brushq1.numtextures;i++, tx++)
	{
		tx->number = i;
		strcpy(tx->name, "NO TEXTURE FOUND");
		tx->width = 16;
		tx->height = 16;
		tx->skin.base = r_notexture;
		tx->shader = &Cshader_wall_lightmap;
		tx->flags = SURF_SOLIDCLIP;
		if (i == loadmodel->brushq1.numtextures - 1)
		{
			tx->flags |= SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			tx->shader = &Cshader_water;
		}
		tx->currentframe = tx;
	}

	if (!m)
		return;

	// just to work around bounds checking when debugging with it (array index out of bounds error thing)
	dofs = m->dataofs;
	// LordHavoc: mostly rewritten map texture loader
	for (i = 0;i < m->nummiptex;i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1 || r_nosurftextures.integer)
			continue;
		dmiptex = (miptex_t *)((qbyte *)m + dofs[i]);

		// make sure name is no more than 15 characters
		for (j = 0;dmiptex->name[j] && j < 15;j++)
			name[j] = dmiptex->name[j];
		name[j] = 0;

		mtwidth = LittleLong(dmiptex->width);
		mtheight = LittleLong(dmiptex->height);
		mtdata = NULL;
		j = LittleLong(dmiptex->offsets[0]);
		if (j)
		{
			// texture included
			if (j < 40 || j + mtwidth * mtheight > l->filelen)
			{
				Con_Printf("Texture \"%s\" in \"%s\"is corrupt or incomplete\n", dmiptex->name, loadmodel->name);
				continue;
			}
			mtdata = (qbyte *)dmiptex + j;
		}

		if ((mtwidth & 15) || (mtheight & 15))
			Con_Printf("warning: texture \"%s\" in \"%s\" is not 16 aligned", dmiptex->name, loadmodel->name);

		// LordHavoc: force all names to lowercase
		for (j = 0;name[j];j++)
			if (name[j] >= 'A' && name[j] <= 'Z')
				name[j] += 'a' - 'A';

		tx = loadmodel->brushq1.textures + i;
		strcpy(tx->name, name);
		tx->width = mtwidth;
		tx->height = mtheight;

		if (!tx->name[0])
		{
			sprintf(tx->name, "unnamed%i", i);
			Con_Printf("warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		// LordHavoc: HL sky textures are entirely different than quake
		if (!loadmodel->brush.ishlbsp && !strncmp(tx->name, "sky", 3) && mtwidth == 256 && mtheight == 128)
		{
			if (loadmodel->isworldmodel)
			{
				data = loadimagepixels(tx->name, false, 0, 0);
				if (data)
				{
					if (image_width == 256 && image_height == 128)
					{
						R_InitSky(data, 4);
						Mem_Free(data);
					}
					else
					{
						Mem_Free(data);
						Con_Printf("Invalid replacement texture for sky \"%s\" in %\"%s\", must be 256x128 pixels\n", tx->name, loadmodel->name);
						if (mtdata != NULL)
							R_InitSky(mtdata, 1);
					}
				}
				else if (mtdata != NULL)
					R_InitSky(mtdata, 1);
			}
		}
		else
		{
			if (!Mod_LoadSkinFrame(&tx->skin, tx->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, false, true, true))
			{
				// did not find external texture, load it from the bsp or wad3
				if (loadmodel->brush.ishlbsp)
				{
					// internal texture overrides wad
					qbyte *pixels, *freepixels, *fogpixels;
					pixels = freepixels = NULL;
					if (mtdata)
						pixels = W_ConvertWAD3Texture(dmiptex);
					if (pixels == NULL)
						pixels = freepixels = W_GetTexture(tx->name);
					if (pixels != NULL)
					{
						tx->width = image_width;
						tx->height = image_height;
						tx->skin.base = tx->skin.merged = R_LoadTexture2D(loadmodel->texturepool, tx->name, image_width, image_height, pixels, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
						if (Image_CheckAlpha(pixels, image_width * image_height, true))
						{
							fogpixels = Mem_Alloc(tempmempool, image_width * image_height * 4);
							for (j = 0;j < image_width * image_height * 4;j += 4)
							{
								fogpixels[j + 0] = 255;
								fogpixels[j + 1] = 255;
								fogpixels[j + 2] = 255;
								fogpixels[j + 3] = pixels[j + 3];
							}
							tx->skin.fog = R_LoadTexture2D(loadmodel->texturepool, tx->name, image_width, image_height, pixels, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
							Mem_Free(fogpixels);
						}
					}
					if (freepixels)
						Mem_Free(freepixels);
				}
				else if (mtdata) // texture included
					Mod_LoadSkinFrame_Internal(&tx->skin, tx->name, TEXF_MIPMAP | TEXF_PRECACHE, false, true, tx->name[0] != '*' && r_fullbrights.integer, mtdata, tx->width, tx->height);
			}
		}
		if (tx->skin.base == NULL)
		{
			// no texture found
			tx->width = 16;
			tx->height = 16;
			tx->skin.base = r_notexture;
		}

		if (tx->name[0] == '*')
		{
			// turb does not block movement
			tx->flags &= ~SURF_SOLIDCLIP;
			tx->flags |= SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			// LordHavoc: some turbulent textures should be fullbright and solid
			if (!strncmp(tx->name,"*lava",5)
			 || !strncmp(tx->name,"*teleport",9)
			 || !strncmp(tx->name,"*rift",5)) // Scourge of Armagon texture
				tx->flags |= SURF_DRAWFULLBRIGHT | SURF_DRAWNOALPHA;
			else
				tx->flags |= SURF_WATERALPHA;
			tx->shader = &Cshader_water;
		}
		else if (tx->name[0] == 's' && tx->name[1] == 'k' && tx->name[2] == 'y')
		{
			tx->flags |= SURF_DRAWSKY;
			tx->shader = &Cshader_sky;
		}
		else
		{
			tx->flags |= SURF_LIGHTMAP;
			if (!tx->skin.fog)
				tx->flags |= SURF_SHADOWCAST | SURF_SHADOWLIGHT;
			tx->shader = &Cshader_wall_lightmap;
		}

		// start out with no animation
		tx->currentframe = tx;
	}

	// sequence the animations
	for (i = 0;i < m->nummiptex;i++)
	{
		tx = loadmodel->brushq1.textures + i;
		if (!tx || tx->name[0] != '+' || tx->name[1] == 0 || tx->name[2] == 0)
			continue;
		if (tx->anim_total[0] || tx->anim_total[1])
			continue;	// already sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		for (j = i;j < m->nummiptex;j++)
		{
			tx2 = loadmodel->brushq1.textures + j;
			if (!tx2 || tx2->name[0] != '+' || strcmp(tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= '0' && num <= '9')
				anims[num - '0'] = tx2;
			else if (num >= 'a' && num <= 'j')
				altanims[num - 'a'] = tx2;
			else
				Con_Printf("Bad animating texture %s\n", tx->name);
		}

		max = altmax = 0;
		for (j = 0;j < 10;j++)
		{
			if (anims[j])
				max = j + 1;
			if (altanims[j])
				altmax = j + 1;
		}
		//Con_Printf("linking animation %s (%i:%i frames)\n\n", tx->name, max, altmax);

		incomplete = false;
		for (j = 0;j < max;j++)
		{
			if (!anims[j])
			{
				Con_Printf("Missing frame %i of %s\n", j, tx->name);
				incomplete = true;
			}
		}
		for (j = 0;j < altmax;j++)
		{
			if (!altanims[j])
			{
				Con_Printf("Missing altframe %i of %s\n", j, tx->name);
				incomplete = true;
			}
		}
		if (incomplete)
			continue;

		if (altmax < 1)
		{
			// if there is no alternate animation, duplicate the primary
			// animation into the alternate
			altmax = max;
			for (k = 0;k < 10;k++)
				altanims[k] = anims[k];
		}

		// link together the primary animation
		for (j = 0;j < max;j++)
		{
			tx2 = anims[j];
			tx2->animated = true;
			tx2->anim_total[0] = max;
			tx2->anim_total[1] = altmax;
			for (k = 0;k < 10;k++)
			{
				tx2->anim_frames[0][k] = anims[k];
				tx2->anim_frames[1][k] = altanims[k];
			}
		}

		// if there really is an alternate anim...
		if (anims[0] != altanims[0])
		{
			// link together the alternate animation
			for (j = 0;j < altmax;j++)
			{
				tx2 = altanims[j];
				tx2->animated = true;
				// the primary/alternate are reversed here
				tx2->anim_total[0] = altmax;
				tx2->anim_total[1] = max;
				for (k = 0;k < 10;k++)
				{
					tx2->anim_frames[0][k] = altanims[k];
					tx2->anim_frames[1][k] = anims[k];
				}
			}
		}
	}
}

static void Mod_Q1BSP_LoadLighting(lump_t *l)
{
	int i;
	qbyte *in, *out, *data, d;
	char litfilename[1024];
	loadmodel->brushq1.lightdata = NULL;
	if (loadmodel->brush.ishlbsp) // LordHavoc: load the colored lighting data straight
	{
		loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, l->filelen);
		memcpy(loadmodel->brushq1.lightdata, mod_base + l->fileofs, l->filelen);
	}
	else // LordHavoc: bsp version 29 (normal white lighting)
	{
		// LordHavoc: hope is not lost yet, check for a .lit file to load
		strlcpy (litfilename, loadmodel->name, sizeof (litfilename));
		FS_StripExtension (litfilename, litfilename, sizeof (litfilename));
		strlcat (litfilename, ".lit", sizeof (litfilename));
		data = (qbyte*) FS_LoadFile(litfilename, false);
		if (data)
		{
			if (fs_filesize > 8 && data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Con_DPrintf("loaded %s\n", litfilename);
					loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, fs_filesize - 8);
					memcpy(loadmodel->brushq1.lightdata, data + 8, fs_filesize - 8);
					Mem_Free(data);
					return;
				}
				else
				{
					Con_Printf("Unknown .lit file version (%d)\n", i);
					Mem_Free(data);
				}
			}
			else
			{
				if (fs_filesize == 8)
					Con_Printf("Empty .lit file, ignoring\n");
				else
					Con_Printf("Corrupt .lit file (old version?), ignoring\n");
				Mem_Free(data);
			}
		}
		// LordHavoc: oh well, expand the white lighting data
		if (!l->filelen)
			return;
		loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, l->filelen*3);
		in = loadmodel->brushq1.lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
		out = loadmodel->brushq1.lightdata;
		memcpy(in, mod_base + l->fileofs, l->filelen);
		for (i = 0;i < l->filelen;i++)
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	}
}

static void Mod_Q1BSP_LoadLightList(void)
{
	int a, n, numlights;
	char lightsfilename[1024], *s, *t, *lightsstring;
	mlight_t *e;

	strlcpy (lightsfilename, loadmodel->name, sizeof (lightsfilename));
	FS_StripExtension (lightsfilename, lightsfilename, sizeof(lightsfilename));
	strlcat (lightsfilename, ".lights", sizeof (lightsfilename));
	s = lightsstring = (char *) FS_LoadFile(lightsfilename, false);
	if (s)
	{
		numlights = 0;
		while (*s)
		{
			while (*s && *s != '\n')
				s++;
			if (!*s)
			{
				Mem_Free(lightsstring);
				Host_Error("lights file must end with a newline\n");
			}
			s++;
			numlights++;
		}
		loadmodel->brushq1.lights = Mem_Alloc(loadmodel->mempool, numlights * sizeof(mlight_t));
		s = lightsstring;
		n = 0;
		while (*s && n < numlights)
		{
			t = s;
			while (*s && *s != '\n')
				s++;
			if (!*s)
			{
				Mem_Free(lightsstring);
				Host_Error("misparsed lights file!\n");
			}
			e = loadmodel->brushq1.lights + n;
			*s = 0;
			a = sscanf(t, "%f %f %f %f %f %f %f %f %f %f %f %f %f %d", &e->origin[0], &e->origin[1], &e->origin[2], &e->falloff, &e->light[0], &e->light[1], &e->light[2], &e->subtract, &e->spotdir[0], &e->spotdir[1], &e->spotdir[2], &e->spotcone, &e->distbias, &e->style);
			*s = '\n';
			if (a != 14)
			{
				Mem_Free(lightsstring);
				Host_Error("invalid lights file, found %d parameters on line %i, should be 14 parameters (origin[0] origin[1] origin[2] falloff light[0] light[1] light[2] subtract spotdir[0] spotdir[1] spotdir[2] spotcone distancebias style)\n", a, n + 1);
			}
			s++;
			n++;
		}
		if (*s)
		{
			Mem_Free(lightsstring);
			Host_Error("misparsed lights file!\n");
		}
		loadmodel->brushq1.numlights = numlights;
		Mem_Free(lightsstring);
	}
}

static void Mod_Q1BSP_LoadVisibility(lump_t *l)
{
	loadmodel->brushq1.num_compressedpvs = 0;
	loadmodel->brushq1.data_compressedpvs = NULL;
	if (!l->filelen)
		return;
	loadmodel->brushq1.num_compressedpvs = l->filelen;
	loadmodel->brushq1.data_compressedpvs = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brushq1.data_compressedpvs, mod_base + l->fileofs, l->filelen);
}

// used only for HalfLife maps
static void Mod_Q1BSP_ParseWadsFromEntityLump(const char *data)
{
	char key[128], value[4096];
	char wadname[128];
	int i, j, k;
	if (!data)
		return;
	if (!COM_ParseToken(&data, false))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseToken(&data, false))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseToken(&data, false))
			return; // error
		strcpy(value, com_token);
		if (!strcmp("wad", key)) // for HalfLife maps
		{
			if (loadmodel->brush.ishlbsp)
			{
				j = 0;
				for (i = 0;i < 4096;i++)
					if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
						break;
				if (value[i])
				{
					for (;i < 4096;i++)
					{
						// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
						if (value[i] == '\\' || value[i] == '/' || value[i] == ':')
							j = i+1;
						else if (value[i] == ';' || value[i] == 0)
						{
							k = value[i];
							value[i] = 0;
							strcpy(wadname, "textures/");
							strcat(wadname, &value[j]);
							W_LoadTextureWadFile(wadname, false);
							j = i+1;
							if (!k)
								break;
						}
					}
				}
			}
		}
	}
}

static void Mod_Q1BSP_LoadEntities(lump_t *l)
{
	loadmodel->brush.entities = NULL;
	if (!l->filelen)
		return;
	loadmodel->brush.entities = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	if (loadmodel->brush.ishlbsp)
		Mod_Q1BSP_ParseWadsFromEntityLump(loadmodel->brush.entities);
}


static void Mod_Q1BSP_LoadVertexes(lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadVertexes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.vertexes = out;
	loadmodel->brushq1.numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void Mod_Q1BSP_LoadSubmodels(lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadSubmodels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.submodels = out;
	loadmodel->brush.numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);
		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

static void Mod_Q1BSP_LoadEdges(lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq1.edges = out;
	loadmodel->brushq1.numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

static void Mod_Q1BSP_LoadTexinfo(lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadTexinfo: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq1.texinfo = out;
	loadmodel->brushq1.numtexinfo = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		for (k = 0;k < 2;k++)
			for (j = 0;j < 4;j++)
				out->vecs[k][j] = LittleFloat(in->vecs[k][j]);

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		out->texture = NULL;
		if (loadmodel->brushq1.textures)
		{
			if ((unsigned int) miptex >= (unsigned int) loadmodel->brushq1.numtextures)
				Con_Printf("error in model \"%s\": invalid miptex index %i(of %i)\n", loadmodel->name, miptex, loadmodel->brushq1.numtextures);
			else
				out->texture = loadmodel->brushq1.textures + miptex;
		}
		if (out->flags & TEX_SPECIAL)
		{
			// if texture chosen is NULL or the shader needs a lightmap,
			// force to notexture water shader
			if (out->texture == NULL || out->texture->shader->flags & SHADERFLAGS_NEEDLIGHTMAP)
				out->texture = loadmodel->brushq1.textures + (loadmodel->brushq1.numtextures - 1);
		}
		else
		{
			// if texture chosen is NULL, force to notexture
			if (out->texture == NULL)
				out->texture = loadmodel->brushq1.textures + (loadmodel->brushq1.numtextures - 2);
		}
	}
}

#if 0
void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0;i < numverts;i++)
	{
		for (j = 0;j < 3;j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

#define MAX_SUBDIVPOLYTRIANGLES 4096
#define MAX_SUBDIVPOLYVERTS(MAX_SUBDIVPOLYTRIANGLES * 3)

static int subdivpolyverts, subdivpolytriangles;
static int subdivpolyindex[MAX_SUBDIVPOLYTRIANGLES][3];
static float subdivpolyvert[MAX_SUBDIVPOLYVERTS][3];

static int subdivpolylookupvert(vec3_t v)
{
	int i;
	for (i = 0;i < subdivpolyverts;i++)
		if (subdivpolyvert[i][0] == v[0]
		 && subdivpolyvert[i][1] == v[1]
		 && subdivpolyvert[i][2] == v[2])
			return i;
	if (subdivpolyverts >= MAX_SUBDIVPOLYVERTS)
		Host_Error("SubDividePolygon: ran out of vertices in buffer, please increase your r_subdivide_size");
	VectorCopy(v, subdivpolyvert[subdivpolyverts]);
	return subdivpolyverts++;
}

static void SubdividePolygon(int numverts, float *verts)
{
	int		i, i1, i2, i3, f, b, c, p;
	vec3_t	mins, maxs, front[256], back[256];
	float	m, *pv, *cv, dist[256], frac;

	if (numverts > 250)
		Host_Error("SubdividePolygon: ran out of verts in buffer");

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0;i < 3;i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = r_subdivide_size.value * floor(m/r_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		for (cv = verts, c = 0;c < numverts;c++, cv += 3)
			dist[c] = cv[i] - m;

		f = b = 0;
		for (p = numverts - 1, c = 0, pv = verts + p * 3, cv = verts;c < numverts;p = c, c++, pv = cv, cv += 3)
		{
			if (dist[p] >= 0)
			{
				VectorCopy(pv, front[f]);
				f++;
			}
			if (dist[p] <= 0)
			{
				VectorCopy(pv, back[b]);
				b++;
			}
			if (dist[p] == 0 || dist[c] == 0)
				continue;
			if ((dist[p] > 0) != (dist[c] > 0) )
			{
				// clip point
				frac = dist[p] / (dist[p] - dist[c]);
				front[f][0] = back[b][0] = pv[0] + frac * (cv[0] - pv[0]);
				front[f][1] = back[b][1] = pv[1] + frac * (cv[1] - pv[1]);
				front[f][2] = back[b][2] = pv[2] + frac * (cv[2] - pv[2]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	i1 = subdivpolylookupvert(verts);
	i2 = subdivpolylookupvert(verts + 3);
	for (i = 2;i < numverts;i++)
	{
		if (subdivpolytriangles >= MAX_SUBDIVPOLYTRIANGLES)
		{
			Con_Printf("SubdividePolygon: ran out of triangles in buffer, please increase your r_subdivide_size\n");
			return;
		}

		i3 = subdivpolylookupvert(verts + i * 3);
		subdivpolyindex[subdivpolytriangles][0] = i1;
		subdivpolyindex[subdivpolytriangles][1] = i2;
		subdivpolyindex[subdivpolytriangles][2] = i3;
		i2 = i3;
		subdivpolytriangles++;
	}
}

//Breaks a polygon up along axial 64 unit
//boundaries so that turbulent and sky warps
//can be done reasonably.
static void Mod_Q1BSP_GenerateWarpMesh(msurface_t *surf)
{
	int i, j;
	surfvertex_t *v;
	surfmesh_t *mesh;

	subdivpolytriangles = 0;
	subdivpolyverts = 0;
	SubdividePolygon(surf->poly_numverts, surf->poly_verts);
	if (subdivpolytriangles < 1)
		Host_Error("Mod_Q1BSP_GenerateWarpMesh: no triangles?\n");

	surf->mesh = mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + subdivpolytriangles * sizeof(int[3]) + subdivpolyverts * sizeof(surfvertex_t));
	mesh->num_vertices = subdivpolyverts;
	mesh->num_triangles = subdivpolytriangles;
	mesh->vertex = (surfvertex_t *)(mesh + 1);
	mesh->index = (int *)(mesh->vertex + mesh->num_vertices);
	memset(mesh->vertex, 0, mesh->num_vertices * sizeof(surfvertex_t));

	for (i = 0;i < mesh->num_triangles;i++)
		for (j = 0;j < 3;j++)
			mesh->index[i*3+j] = subdivpolyindex[i][j];

	for (i = 0, v = mesh->vertex;i < subdivpolyverts;i++, v++)
	{
		VectorCopy(subdivpolyvert[i], v->v);
		v->st[0] = DotProduct(v->v, surf->texinfo->vecs[0]);
		v->st[1] = DotProduct(v->v, surf->texinfo->vecs[1]);
	}
}
#endif

static surfmesh_t *Mod_Q1BSP_AllocSurfMesh(int numverts, int numtriangles)
{
	surfmesh_t *mesh;
	mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + numtriangles * sizeof(int[6]) + numverts * (3 + 2 + 2 + 2 + 3 + 3 + 3 + 1) * sizeof(float));
	mesh->num_vertices = numverts;
	mesh->num_triangles = numtriangles;
	mesh->data_vertex3f = (float *)(mesh + 1);
	mesh->data_texcoordtexture2f = mesh->data_vertex3f + mesh->num_vertices * 3;
	mesh->data_texcoordlightmap2f = mesh->data_texcoordtexture2f + mesh->num_vertices * 2;
	mesh->data_texcoorddetail2f = mesh->data_texcoordlightmap2f + mesh->num_vertices * 2;
	mesh->data_svector3f = (float *)(mesh->data_texcoorddetail2f + mesh->num_vertices * 2);
	mesh->data_tvector3f = mesh->data_svector3f + mesh->num_vertices * 3;
	mesh->data_normal3f = mesh->data_tvector3f + mesh->num_vertices * 3;
	mesh->data_lightmapoffsets = (int *)(mesh->data_normal3f + mesh->num_vertices * 3);
	mesh->data_element3i = mesh->data_lightmapoffsets + mesh->num_vertices;
	mesh->data_neighbor3i = mesh->data_element3i + mesh->num_triangles * 3;
	return mesh;
}

static void Mod_Q1BSP_GenerateSurfacePolygon(msurface_t *surf, int firstedge, int numedges)
{
	int i, lindex, j;
	float *vec, *vert, mins[3], maxs[3], val, *v;
	mtexinfo_t *tex;

	// convert edges back to a normal polygon
	surf->poly_numverts = numedges;
	vert = surf->poly_verts = Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * numedges);
	for (i = 0;i < numedges;i++)
	{
		lindex = loadmodel->brushq1.surfedges[firstedge + i];
		if (lindex > 0)
			vec = loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[lindex].v[0]].position;
		else
			vec = loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[-lindex].v[1]].position;
		VectorCopy(vec, vert);
		vert += 3;
	}

	// calculate polygon bounding box and center
	vert = surf->poly_verts;
	VectorCopy(vert, mins);
	VectorCopy(vert, maxs);
	vert += 3;
	for (i = 1;i < surf->poly_numverts;i++, vert += 3)
	{
		if (mins[0] > vert[0]) mins[0] = vert[0];if (maxs[0] < vert[0]) maxs[0] = vert[0];
		if (mins[1] > vert[1]) mins[1] = vert[1];if (maxs[1] < vert[1]) maxs[1] = vert[1];
		if (mins[2] > vert[2]) mins[2] = vert[2];if (maxs[2] < vert[2]) maxs[2] = vert[2];
	}
	VectorCopy(mins, surf->poly_mins);
	VectorCopy(maxs, surf->poly_maxs);
	surf->poly_center[0] = (mins[0] + maxs[0]) * 0.5f;
	surf->poly_center[1] = (mins[1] + maxs[1]) * 0.5f;
	surf->poly_center[2] = (mins[2] + maxs[2]) * 0.5f;

	// generate surface extents information
	tex = surf->texinfo;
	mins[0] = maxs[0] = DotProduct(surf->poly_verts, tex->vecs[0]) + tex->vecs[0][3];
	mins[1] = maxs[1] = DotProduct(surf->poly_verts, tex->vecs[1]) + tex->vecs[1][3];
	for (i = 1, v = surf->poly_verts + 3;i < surf->poly_numverts;i++, v += 3)
	{
		for (j = 0;j < 2;j++)
		{
			val = DotProduct(v, tex->vecs[j]) + tex->vecs[j][3];
			if (mins[j] > val)
				mins[j] = val;
			if (maxs[j] < val)
				maxs[j] = val;
		}
	}
	for (i = 0;i < 2;i++)
	{
		surf->texturemins[i] = (int) floor(mins[i] / 16) * 16;
		surf->extents[i] = (int) ceil(maxs[i] / 16) * 16 - surf->texturemins[i];
	}
}

static void Mod_Q1BSP_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *surf;
	int i, count, surfnum, planenum, ssize, tsize, firstedge, numedges, totalverts, totaltris, totalmeshes;
	surfmesh_t *mesh;
	float s, t;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	loadmodel->brushq1.surfaces = Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_t));

	loadmodel->brushq1.numsurfaces = count;
	loadmodel->brushq1.surfacevisframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));
	loadmodel->brushq1.surfacepvsframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));
	loadmodel->brushq1.pvssurflist = Mem_Alloc(loadmodel->mempool, count * sizeof(int));

	for (surfnum = 0, surf = loadmodel->brushq1.surfaces, totalverts = 0, totaltris = 0, totalmeshes = 0;surfnum < count;surfnum++, totalverts += surf->poly_numverts, totaltris += surf->poly_numverts - 2, totalmeshes++, in++, surf++)
	{
		surf->number = surfnum;
		// FIXME: validate edges, texinfo, etc?
		firstedge = LittleLong(in->firstedge);
		numedges = LittleShort(in->numedges);
		if ((unsigned int) firstedge > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) firstedge + (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid edge range (firstedge %i, numedges %i, model edges %i)\n", firstedge, numedges, loadmodel->brushq1.numsurfedges);
		i = LittleShort(in->texinfo);
		if ((unsigned int) i >= (unsigned int) loadmodel->brushq1.numtexinfo)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid texinfo index %i(model has %i texinfos)\n", i, loadmodel->brushq1.numtexinfo);
		surf->texinfo = loadmodel->brushq1.texinfo + i;
		surf->flags = surf->texinfo->texture->flags;

		planenum = LittleShort(in->planenum);
		if ((unsigned int) planenum >= (unsigned int) loadmodel->brushq1.numplanes)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid plane index %i (model has %i planes)\n", planenum, loadmodel->brushq1.numplanes);

		if (LittleShort(in->side))
			surf->flags |= SURF_PLANEBACK;

		surf->plane = loadmodel->brushq1.planes + planenum;

		// clear lightmap (filled in later)
		surf->lightmaptexture = NULL;

		// force lightmap upload on first time seeing the surface
		surf->cached_dlight = true;

		Mod_Q1BSP_GenerateSurfacePolygon(surf, firstedge, numedges);

		ssize = (surf->extents[0] >> 4) + 1;
		tsize = (surf->extents[1] >> 4) + 1;

		// lighting info
		for (i = 0;i < MAXLIGHTMAPS;i++)
			surf->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			surf->samples = NULL;
		else if (loadmodel->brush.ishlbsp) // LordHavoc: HalfLife map (bsp version 30)
			surf->samples = loadmodel->brushq1.lightdata + i;
		else // LordHavoc: white lighting (bsp version 29)
			surf->samples = loadmodel->brushq1.lightdata + (i * 3);

		if (surf->texinfo->texture->shader == &Cshader_wall_lightmap)
		{
			if ((surf->extents[0] >> 4) + 1 > (256) || (surf->extents[1] >> 4) + 1 > (256))
				Host_Error("Bad surface extents");
			// stainmap for permanent marks on walls
			surf->stainsamples = Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
			// clear to white
			memset(surf->stainsamples, 255, ssize * tsize * 3);
		}
	}

	loadmodel->brushq1.entiremesh = Mod_Q1BSP_AllocSurfMesh(totalverts, totaltris);

	for (surfnum = 0, surf = loadmodel->brushq1.surfaces, totalverts = 0, totaltris = 0, totalmeshes = 0;surfnum < count;surfnum++, totalverts += surf->poly_numverts, totaltris += surf->poly_numverts - 2, totalmeshes++, surf++)
	{
		mesh = &surf->mesh;
		mesh->num_vertices = surf->poly_numverts;
		mesh->num_triangles = surf->poly_numverts - 2;
		mesh->data_vertex3f = loadmodel->brushq1.entiremesh->data_vertex3f + totalverts * 3;
		mesh->data_texcoordtexture2f = loadmodel->brushq1.entiremesh->data_texcoordtexture2f + totalverts * 2;
		mesh->data_texcoordlightmap2f = loadmodel->brushq1.entiremesh->data_texcoordlightmap2f + totalverts * 2;
		mesh->data_texcoorddetail2f = loadmodel->brushq1.entiremesh->data_texcoorddetail2f + totalverts * 2;
		mesh->data_svector3f = loadmodel->brushq1.entiremesh->data_svector3f + totalverts * 3;
		mesh->data_tvector3f = loadmodel->brushq1.entiremesh->data_tvector3f + totalverts * 3;
		mesh->data_normal3f = loadmodel->brushq1.entiremesh->data_normal3f + totalverts * 3;
		mesh->data_lightmapoffsets = loadmodel->brushq1.entiremesh->data_lightmapoffsets + totalverts;
		mesh->data_element3i = loadmodel->brushq1.entiremesh->data_element3i + totaltris * 3;
		mesh->data_neighbor3i = loadmodel->brushq1.entiremesh->data_neighbor3i + totaltris * 3;

		surf->lightmaptexturestride = 0;
		surf->lightmaptexture = NULL;

		for (i = 0;i < mesh->num_vertices;i++)
		{
			mesh->data_vertex3f[i * 3 + 0] = surf->poly_verts[i * 3 + 0];
			mesh->data_vertex3f[i * 3 + 1] = surf->poly_verts[i * 3 + 1];
			mesh->data_vertex3f[i * 3 + 2] = surf->poly_verts[i * 3 + 2];
			s = DotProduct((mesh->data_vertex3f + i * 3), surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			t = DotProduct((mesh->data_vertex3f + i * 3), surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			mesh->data_texcoordtexture2f[i * 2 + 0] = s / surf->texinfo->texture->width;
			mesh->data_texcoordtexture2f[i * 2 + 1] = t / surf->texinfo->texture->height;
			mesh->data_texcoorddetail2f[i * 2 + 0] = s * (1.0f / 16.0f);
			mesh->data_texcoorddetail2f[i * 2 + 1] = t * (1.0f / 16.0f);
			mesh->data_texcoordlightmap2f[i * 2 + 0] = 0;
			mesh->data_texcoordlightmap2f[i * 2 + 1] = 0;
			mesh->data_lightmapoffsets[i] = 0;
		}

		for (i = 0;i < mesh->num_triangles;i++)
		{
			mesh->data_element3i[i * 3 + 0] = 0;
			mesh->data_element3i[i * 3 + 1] = i + 1;
			mesh->data_element3i[i * 3 + 2] = i + 2;
		}

		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, mesh->data_vertex3f, mesh->data_texcoordtexture2f, mesh->data_element3i, mesh->data_svector3f, mesh->data_tvector3f, mesh->data_normal3f);

		if (surf->texinfo->texture->shader == &Cshader_wall_lightmap)
		{
			int i, iu, iv, smax, tmax;
			float u, v, ubase, vbase, uscale, vscale;

			smax = surf->extents[0] >> 4;
			tmax = surf->extents[1] >> 4;

			surf->flags |= SURF_LIGHTMAP;
			if (r_miplightmaps.integer)
			{
				surf->lightmaptexturestride = smax+1;
				surf->lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_MIPMAP | TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
			}
			else
			{
				surf->lightmaptexturestride = R_CompatibleFragmentWidth(smax+1, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, 0);
				surf->lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_FRAGMENT | TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
			}
			R_FragmentLocation(surf->lightmaptexture, NULL, NULL, &ubase, &vbase, &uscale, &vscale);
			uscale = (uscale - ubase) / (smax + 1);
			vscale = (vscale - vbase) / (tmax + 1);

			for (i = 0;i < mesh->num_vertices;i++)
			{
				u = ((DotProduct((mesh->data_vertex3f + i * 3), surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) + 8 - surf->texturemins[0]) * (1.0 / 16.0);
				v = ((DotProduct((mesh->data_vertex3f + i * 3), surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) + 8 - surf->texturemins[1]) * (1.0 / 16.0);
				mesh->data_texcoordlightmap2f[i * 2 + 0] = u * uscale + ubase;
				mesh->data_texcoordlightmap2f[i * 2 + 1] = v * vscale + vbase;
				// LordHavoc: calc lightmap data offset for vertex lighting to use
				iu = (int) u;
				iv = (int) v;
				mesh->data_lightmapoffsets[i] = (bound(0, iv, tmax) * (smax+1) + bound(0, iu, smax)) * 3;
			}
		}
	}
}

static void Mod_Q1BSP_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_Q1BSP_SetParent(node->children[0], node);
	Mod_Q1BSP_SetParent(node->children[1], node);
}

static void Mod_Q1BSP_LoadNodes(lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.nodes = out;
	loadmodel->brushq1.numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->brushq1.planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->brushq1.nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->brushq1.leafs + (-1 - p));
		}
	}

	Mod_Q1BSP_SetParent(loadmodel->brushq1.nodes, NULL);	// sets nodes and leafs
}

static void Mod_Q1BSP_LoadLeafs(lump_t *l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p, pvschainbytes;
	qbyte *pvs;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.leafs = out;
	loadmodel->brushq1.numleafs = count;
	// get visleafs from the submodel data
	pvschainbytes = (loadmodel->brushq1.submodels[0].visleafs+7)>>3;
	loadmodel->brushq1.data_decompressedpvs = pvs = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numleafs * pvschainbytes);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		// FIXME: this function could really benefit from some error checking

		out->contents = LittleLong(in->contents);

		out->firstmarksurface = loadmodel->brushq1.marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		if (out->firstmarksurface < 0 || LittleShort(in->firstmarksurface) + out->nummarksurfaces > loadmodel->brushq1.nummarksurfaces)
		{
			Con_Printf("Mod_Q1BSP_LoadLeafs: invalid marksurface range %i:%i outside range %i:%i\n", out->firstmarksurface, out->firstmarksurface + out->nummarksurfaces, 0, loadmodel->brushq1.nummarksurfaces);
			out->firstmarksurface = NULL;
			out->nummarksurfaces = 0;
		}

		out->pvsdata = pvs;
		memset(out->pvsdata, 0xFF, pvschainbytes);
		pvs += pvschainbytes;

		p = LittleLong(in->visofs);
		if (p >= 0 && i > 0) // ignore visofs errors on leaf 0 (solid)
		{
			if (p >= loadmodel->brushq1.num_compressedpvs)
				Con_Printf("Mod_Q1BSP_LoadLeafs: invalid visofs\n");
			else
				Mod_Q1BSP_DecompressVis(loadmodel->brushq1.data_compressedpvs + p, loadmodel->brushq1.data_compressedpvs + loadmodel->brushq1.num_compressedpvs, out->pvsdata, out->pvsdata + pvschainbytes);
		}

		for (j = 0;j < 4;j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// FIXME: Insert caustics here
	}
}

static void Mod_Q1BSP_LoadClipnodes(lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadClipnodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.clipnodes = out;
	loadmodel->brushq1.numclipnodes = count;

	if (loadmodel->brush.ishlbsp)
	{
		hull = &loadmodel->brushq1.hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -18;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 18;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);
	}
	else
	{
		hull = &loadmodel->brushq1.hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 64;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);
	}

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
		if (out->children[0] >= count || out->children[1] >= count)
			Host_Error("Corrupt clipping hull(out of range child)\n");
	}
}

//Duplicate the drawing hull structure as a clipping hull
static void Mod_Q1BSP_MakeHull0(void)
{
	mnode_t		*in;
	dclipnode_t *out;
	int			i;
	hull_t		*hull;

	hull = &loadmodel->brushq1.hulls[0];

	in = loadmodel->brushq1.nodes;
	out = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numnodes * sizeof(dclipnode_t));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = loadmodel->brushq1.numnodes - 1;
	hull->planes = loadmodel->brushq1.planes;

	for (i = 0;i < loadmodel->brushq1.numnodes;i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->brushq1.planes;
		out->children[0] = in->children[0]->contents < 0 ? in->children[0]->contents : in->children[0] - loadmodel->brushq1.nodes;
		out->children[1] = in->children[1]->contents < 0 ? in->children[1]->contents : in->children[1] - loadmodel->brushq1.nodes;
	}
}

static void Mod_Q1BSP_LoadMarksurfaces(lump_t *l)
{
	int i, j;
	short *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadMarksurfaces: funny lump size in %s",loadmodel->name);
	loadmodel->brushq1.nummarksurfaces = l->filelen / sizeof(*in);
	loadmodel->brushq1.marksurfaces = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.nummarksurfaces * sizeof(int));

	for (i = 0;i < loadmodel->brushq1.nummarksurfaces;i++)
	{
		j = (unsigned) LittleShort(in[i]);
		if (j >= loadmodel->brushq1.numsurfaces)
			Host_Error("Mod_Q1BSP_LoadMarksurfaces: bad surface number");
		loadmodel->brushq1.marksurfaces[i] = j;
	}
}

static void Mod_Q1BSP_LoadSurfedges(lump_t *l)
{
	int		i;
	int		*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadSurfedges: funny lump size in %s",loadmodel->name);
	loadmodel->brushq1.numsurfedges = l->filelen / sizeof(*in);
	loadmodel->brushq1.surfedges = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numsurfedges * sizeof(int));

	for (i = 0;i < loadmodel->brushq1.numsurfedges;i++)
		loadmodel->brushq1.surfedges[i] = LittleLong(in[i]);
}


static void Mod_Q1BSP_LoadPlanes(lump_t *l)
{
	int			i;
	mplane_t	*out;
	dplane_t 	*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadPlanes: funny lump size in %s", loadmodel->name);

	loadmodel->brushq1.numplanes = l->filelen / sizeof(*in);
	loadmodel->brushq1.planes = out = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numplanes * sizeof(*out));

	for (i = 0;i < loadmodel->brushq1.numplanes;i++, in++, out++)
	{
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
		out->dist = LittleFloat(in->dist);

		PlaneClassify(out);
	}
}

typedef struct portal_s
{
	mplane_t plane;
	mnode_t *nodes[2];		// [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;
	struct portal_s *chain; // all portals are linked into a list
}
portal_t;

static portal_t *portalchain;

/*
===========
AllocPortal
===========
*/
static portal_t *AllocPortal(void)
{
	portal_t *p;
	p = Mem_Alloc(loadmodel->mempool, sizeof(portal_t));
	p->chain = portalchain;
	portalchain = p;
	return p;
}

static void FreePortal(portal_t *p)
{
	Mem_Free(p);
}

static void Mod_Q1BSP_RecursiveRecalcNodeBBox(mnode_t *node)
{
	// calculate children first
	if (node->children[0]->contents >= 0)
		Mod_Q1BSP_RecursiveRecalcNodeBBox(node->children[0]);
	if (node->children[1]->contents >= 0)
		Mod_Q1BSP_RecursiveRecalcNodeBBox(node->children[1]);

	// make combined bounding box from children
	node->mins[0] = min(node->children[0]->mins[0], node->children[1]->mins[0]);
	node->mins[1] = min(node->children[0]->mins[1], node->children[1]->mins[1]);
	node->mins[2] = min(node->children[0]->mins[2], node->children[1]->mins[2]);
	node->maxs[0] = max(node->children[0]->maxs[0], node->children[1]->maxs[0]);
	node->maxs[1] = max(node->children[0]->maxs[1], node->children[1]->maxs[1]);
	node->maxs[2] = max(node->children[0]->maxs[2], node->children[1]->maxs[2]);
}

static void Mod_Q1BSP_FinalizePortals(void)
{
	int i, j, numportals, numpoints;
	portal_t *p, *pnext;
	mportal_t *portal;
	mvertex_t *point;
	mleaf_t *leaf, *endleaf;
	winding_t *w;

	// recalculate bounding boxes for all leafs(because qbsp is very sloppy)
	leaf = loadmodel->brushq1.leafs;
	endleaf = leaf + loadmodel->brushq1.numleafs;
	for (;leaf < endleaf;leaf++)
	{
		VectorSet(leaf->mins,  2000000000,  2000000000,  2000000000);
		VectorSet(leaf->maxs, -2000000000, -2000000000, -2000000000);
	}
	p = portalchain;
	while (p)
	{
		if (p->winding)
		{
			for (i = 0;i < 2;i++)
			{
				leaf = (mleaf_t *)p->nodes[i];
				w = p->winding;
				for (j = 0;j < w->numpoints;j++)
				{
					if (leaf->mins[0] > w->points[j][0]) leaf->mins[0] = w->points[j][0];
					if (leaf->mins[1] > w->points[j][1]) leaf->mins[1] = w->points[j][1];
					if (leaf->mins[2] > w->points[j][2]) leaf->mins[2] = w->points[j][2];
					if (leaf->maxs[0] < w->points[j][0]) leaf->maxs[0] = w->points[j][0];
					if (leaf->maxs[1] < w->points[j][1]) leaf->maxs[1] = w->points[j][1];
					if (leaf->maxs[2] < w->points[j][2]) leaf->maxs[2] = w->points[j][2];
				}
			}
		}
		p = p->chain;
	}

	Mod_Q1BSP_RecursiveRecalcNodeBBox(loadmodel->brushq1.nodes);

	// tally up portal and point counts
	p = portalchain;
	numportals = 0;
	numpoints = 0;
	while (p)
	{
		// note: this check must match the one below or it will usually corrupt memory
		// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
		if (p->winding && p->nodes[0] != p->nodes[1]
		 && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID
		 && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY)
		{
			numportals += 2;
			numpoints += p->winding->numpoints * 2;
		}
		p = p->chain;
	}
	loadmodel->brushq1.portals = Mem_Alloc(loadmodel->mempool, numportals * sizeof(mportal_t) + numpoints * sizeof(mvertex_t));
	loadmodel->brushq1.numportals = numportals;
	loadmodel->brushq1.portalpoints = (void *)((qbyte *) loadmodel->brushq1.portals + numportals * sizeof(mportal_t));
	loadmodel->brushq1.numportalpoints = numpoints;
	// clear all leaf portal chains
	for (i = 0;i < loadmodel->brushq1.numleafs;i++)
		loadmodel->brushq1.leafs[i].portals = NULL;
	// process all portals in the global portal chain, while freeing them
	portal = loadmodel->brushq1.portals;
	point = loadmodel->brushq1.portalpoints;
	p = portalchain;
	portalchain = NULL;
	while (p)
	{
		pnext = p->chain;

		if (p->winding)
		{
			// note: this check must match the one above or it will usually corrupt memory
			// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
			if (p->nodes[0] != p->nodes[1]
			 && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID
			 && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY)
			{
				// first make the back to front portal(forward portal)
				portal->points = point;
				portal->numpoints = p->winding->numpoints;
				portal->plane.dist = p->plane.dist;
				VectorCopy(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[1];
				portal->past = (mleaf_t *)p->nodes[0];
				// copy points
				for (j = 0;j < portal->numpoints;j++)
				{
					VectorCopy(p->winding->points[j], point->position);
					point++;
				}
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;

				// then make the front to back portal(backward portal)
				portal->points = point;
				portal->numpoints = p->winding->numpoints;
				portal->plane.dist = -p->plane.dist;
				VectorNegate(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[0];
				portal->past = (mleaf_t *)p->nodes[1];
				// copy points
				for (j = portal->numpoints - 1;j >= 0;j--)
				{
					VectorCopy(p->winding->points[j], point->position);
					point++;
				}
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;
			}
			Winding_Free(p->winding);
		}
		FreePortal(p);
		p = pnext;
	}
}

/*
=============
AddPortalToNodes
=============
*/
static void AddPortalToNodes(portal_t *p, mnode_t *front, mnode_t *back)
{
	if (!front)
		Host_Error("AddPortalToNodes: NULL front node");
	if (!back)
		Host_Error("AddPortalToNodes: NULL back node");
	if (p->nodes[0] || p->nodes[1])
		Host_Error("AddPortalToNodes: already included");
	// note: front == back is handled gracefully, because leaf 0 is the shared solid leaf, it can often have portals with the same leaf on both sides

	p->nodes[0] = front;
	p->next[0] = (portal_t *)front->portals;
	front->portals = (mportal_t *)p;

	p->nodes[1] = back;
	p->next[1] = (portal_t *)back->portals;
	back->portals = (mportal_t *)p;
}

/*
=============
RemovePortalFromNode
=============
*/
static void RemovePortalFromNodes(portal_t *portal)
{
	int i;
	mnode_t *node;
	void **portalpointer;
	portal_t *t;
	for (i = 0;i < 2;i++)
	{
		node = portal->nodes[i];

		portalpointer = (void **) &node->portals;
		while (1)
		{
			t = *portalpointer;
			if (!t)
				Host_Error("RemovePortalFromNodes: portal not in leaf");

			if (t == portal)
			{
				if (portal->nodes[0] == node)
				{
					*portalpointer = portal->next[0];
					portal->nodes[0] = NULL;
				}
				else if (portal->nodes[1] == node)
				{
					*portalpointer = portal->next[1];
					portal->nodes[1] = NULL;
				}
				else
					Host_Error("RemovePortalFromNodes: portal not bounding leaf");
				break;
			}

			if (t->nodes[0] == node)
				portalpointer = (void **) &t->next[0];
			else if (t->nodes[1] == node)
				portalpointer = (void **) &t->next[1];
			else
				Host_Error("RemovePortalFromNodes: portal not bounding leaf");
		}
	}
}

static void Mod_Q1BSP_RecursiveNodePortals(mnode_t *node)
{
	int side;
	mnode_t *front, *back, *other_node;
	mplane_t clipplane, *plane;
	portal_t *portal, *nextportal, *nodeportal, *splitportal, *temp;
	winding_t *nodeportalwinding, *frontwinding, *backwinding;

	// if a leaf, we're done
	if (node->contents)
		return;

	plane = node->plane;

	front = node->children[0];
	back = node->children[1];
	if (front == back)
		Host_Error("Mod_Q1BSP_RecursiveNodePortals: corrupt node hierarchy");

	// create the new portal by generating a polygon for the node plane,
	// and clipping it by all of the other portals(which came from nodes above this one)
	nodeportal = AllocPortal();
	nodeportal->plane = *plane;

	nodeportalwinding = Winding_NewFromPlane(nodeportal->plane.normal[0], nodeportal->plane.normal[1], nodeportal->plane.normal[2], nodeportal->plane.dist);
	side = 0;	// shut up compiler warning
	for (portal = (portal_t *)node->portals;portal;portal = portal->next[side])
	{
		clipplane = portal->plane;
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: portal has same node on both sides(1)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
		{
			clipplane.dist = -clipplane.dist;
			VectorNegate(clipplane.normal, clipplane.normal);
			side = 1;
		}
		else
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: mislinked portal");

		nodeportalwinding = Winding_Clip(nodeportalwinding, clipplane.normal[0], clipplane.normal[1], clipplane.normal[2], clipplane.dist, true);
		if (!nodeportalwinding)
		{
			Con_Printf("Mod_Q1BSP_RecursiveNodePortals: WARNING: new portal was clipped away\n");
			break;
		}
	}

	if (nodeportalwinding)
	{
		// if the plane was not clipped on all sides, there was an error
		nodeportal->winding = nodeportalwinding;
		AddPortalToNodes(nodeportal, front, back);
	}

	// split the portals of this node along this node's plane and assign them to the children of this node
	// (migrating the portals downward through the tree)
	for (portal = (portal_t *)node->portals;portal;portal = nextportal)
	{
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: portal has same node on both sides(2)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
			side = 1;
		else
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: mislinked portal");
		nextportal = portal->next[side];

		other_node = portal->nodes[!side];
		RemovePortalFromNodes(portal);

		// cut the portal into two portals, one on each side of the node plane
		Winding_Divide(portal->winding, plane->normal[0], plane->normal[1], plane->normal[2], plane->dist, &frontwinding, &backwinding);

		if (!frontwinding)
		{
			if (side == 0)
				AddPortalToNodes(portal, back, other_node);
			else
				AddPortalToNodes(portal, other_node, back);
			continue;
		}
		if (!backwinding)
		{
			if (side == 0)
				AddPortalToNodes(portal, front, other_node);
			else
				AddPortalToNodes(portal, other_node, front);
			continue;
		}

		// the winding is split
		splitportal = AllocPortal();
		temp = splitportal->chain;
		*splitportal = *portal;
		splitportal->chain = temp;
		splitportal->winding = backwinding;
		Winding_Free(portal->winding);
		portal->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes(portal, front, other_node);
			AddPortalToNodes(splitportal, back, other_node);
		}
		else
		{
			AddPortalToNodes(portal, other_node, front);
			AddPortalToNodes(splitportal, other_node, back);
		}
	}

	Mod_Q1BSP_RecursiveNodePortals(front);
	Mod_Q1BSP_RecursiveNodePortals(back);
}

static void Mod_Q1BSP_MakePortals(void)
{
	portalchain = NULL;
	Mod_Q1BSP_RecursiveNodePortals(loadmodel->brushq1.nodes);
	Mod_Q1BSP_FinalizePortals();
}

static void Mod_Q1BSP_BuildSurfaceNeighbors(msurface_t *surfaces, int numsurfaces, mempool_t *mempool)
{
#if 0
	int surfnum, vertnum, vertnum2, snum, vnum, vnum2;
	msurface_t *surf, *s;
	float *v0, *v1, *v2, *v3;
	for (surf = surfaces, surfnum = 0;surfnum < numsurfaces;surf++, surfnum++)
		surf->neighborsurfaces = Mem_Alloc(mempool, surf->poly_numverts * sizeof(msurface_t *));
	for (surf = surfaces, surfnum = 0;surfnum < numsurfaces;surf++, surfnum++)
	{
		for (vertnum = surf->poly_numverts - 1, vertnum2 = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = surf->poly_verts;vertnum2 < surf->poly_numverts;vertnum = vertnum2, vertnum2++, v0 = v1, v1 += 3)
		{
			if (surf->neighborsurfaces[vertnum])
				continue;
			surf->neighborsurfaces[vertnum] = NULL;
			for (s = surfaces, snum = 0;snum < numsurfaces;s++, snum++)
			{
				if (s->poly_mins[0] > (surf->poly_maxs[0] + 1) || s->poly_maxs[0] < (surf->poly_mins[0] - 1)
				 || s->poly_mins[1] > (surf->poly_maxs[1] + 1) || s->poly_maxs[1] < (surf->poly_mins[1] - 1)
				 || s->poly_mins[2] > (surf->poly_maxs[2] + 1) || s->poly_maxs[2] < (surf->poly_mins[2] - 1)
				 || s == surf)
					continue;
				for (vnum = 0;vnum < s->poly_numverts;vnum++)
					if (s->neighborsurfaces[vnum] == surf)
						break;
				if (vnum < s->poly_numverts)
					continue;
				for (vnum = s->poly_numverts - 1, vnum2 = 0, v2 = s->poly_verts + (s->poly_numverts - 1) * 3, v3 = s->poly_verts;vnum2 < s->poly_numverts;vnum = vnum2, vnum2++, v2 = v3, v3 += 3)
				{
					if (s->neighborsurfaces[vnum] == NULL
					 && ((v0[0] == v2[0] && v0[1] == v2[1] && v0[2] == v2[2] && v1[0] == v3[0] && v1[1] == v3[1] && v1[2] == v3[2])
					  || (v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2] && v0[0] == v3[0] && v0[1] == v3[1] && v0[2] == v3[2])))
					{
						surf->neighborsurfaces[vertnum] = s;
						s->neighborsurfaces[vnum] = surf;
						break;
					}
				}
				if (vnum < s->poly_numverts)
					break;
			}
		}
	}
#endif
}

static void Mod_Q1BSP_BuildLightmapUpdateChains(mempool_t *mempool, model_t *model)
{
	int i, j, stylecounts[256], totalcount, remapstyles[256];
	msurface_t *surf;
	memset(stylecounts, 0, sizeof(stylecounts));
	for (i = 0;i < model->brushq1.nummodelsurfaces;i++)
	{
		surf = model->brushq1.surfaces + model->brushq1.firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			stylecounts[surf->styles[j]]++;
	}
	totalcount = 0;
	model->brushq1.light_styles = 0;
	for (i = 0;i < 255;i++)
	{
		if (stylecounts[i])
		{
			remapstyles[i] = model->brushq1.light_styles++;
			totalcount += stylecounts[i] + 1;
		}
	}
	if (!totalcount)
		return;
	model->brushq1.light_style = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(qbyte));
	model->brushq1.light_stylevalue = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(int));
	model->brushq1.light_styleupdatechains = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(msurface_t **));
	model->brushq1.light_styleupdatechainsbuffer = Mem_Alloc(mempool, totalcount * sizeof(msurface_t *));
	model->brushq1.light_styles = 0;
	for (i = 0;i < 255;i++)
		if (stylecounts[i])
			model->brushq1.light_style[model->brushq1.light_styles++] = i;
	j = 0;
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		model->brushq1.light_styleupdatechains[i] = model->brushq1.light_styleupdatechainsbuffer + j;
		j += stylecounts[model->brushq1.light_style[i]] + 1;
	}
	for (i = 0;i < model->brushq1.nummodelsurfaces;i++)
	{
		surf = model->brushq1.surfaces + model->brushq1.firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			if (surf->styles[j] != 255)
				*model->brushq1.light_styleupdatechains[remapstyles[surf->styles[j]]]++ = surf;
	}
	j = 0;
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		*model->brushq1.light_styleupdatechains[i] = NULL;
		model->brushq1.light_styleupdatechains[i] = model->brushq1.light_styleupdatechainsbuffer + j;
		j += stylecounts[model->brushq1.light_style[i]] + 1;
	}
}

static void Mod_Q1BSP_BuildPVSTextureChains(model_t *model)
{
	int i, j;
	for (i = 0;i < model->brushq1.numtextures;i++)
		model->brushq1.pvstexturechainslength[i] = 0;
	for (i = 0, j = model->brushq1.firstmodelsurface;i < model->brushq1.nummodelsurfaces;i++, j++)
	{
		if (model->brushq1.surfacepvsframes[j] == model->brushq1.pvsframecount)
		{
			model->brushq1.pvssurflist[model->brushq1.pvssurflistlength++] = j;
			model->brushq1.pvstexturechainslength[model->brushq1.surfaces[j].texinfo->texture->number]++;
		}
	}
	for (i = 0, j = 0;i < model->brushq1.numtextures;i++)
	{
		if (model->brushq1.pvstexturechainslength[i])
		{
			model->brushq1.pvstexturechains[i] = model->brushq1.pvstexturechainsbuffer + j;
			j += model->brushq1.pvstexturechainslength[i] + 1;
		}
		else
			model->brushq1.pvstexturechains[i] = NULL;
	}
	for (i = 0, j = model->brushq1.firstmodelsurface;i < model->brushq1.nummodelsurfaces;i++, j++)
		if (model->brushq1.surfacepvsframes[j] == model->brushq1.pvsframecount)
			*model->brushq1.pvstexturechains[model->brushq1.surfaces[j].texinfo->texture->number]++ = model->brushq1.surfaces + j;
	for (i = 0;i < model->brushq1.numtextures;i++)
	{
		if (model->brushq1.pvstexturechainslength[i])
		{
			*model->brushq1.pvstexturechains[i] = NULL;
			model->brushq1.pvstexturechains[i] -= model->brushq1.pvstexturechainslength[i];
		}
	}
}

static void Mod_Q1BSP_FatPVS_RecursiveBSPNode(model_t *model, const vec3_t org, vec_t radius, qbyte *pvsbuffer, int pvsbytes, mnode_t *node)
{
	int i;
	float d;

	while (node->contents >= 0)
	{
		d = PlaneDiff(org, node->plane);
		if (d > radius)
			node = node->children[0];
		else if (d < -radius)
			node = node->children[1];
		else
		{
			// go down both sides
			Mod_Q1BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, pvsbytes, node->children[0]);
			node = node->children[1];
		}
	}
	// FIXME: code!
	// if this is a leaf, accumulate the pvs bits
	if (/*node->contents != CONTENTS_SOLID && */((mleaf_t *)node)->pvsdata)
		for (i = 0;i < pvsbytes;i++)
			pvsbuffer[i] |= ((mleaf_t *)node)->pvsdata[i];
}

//Calculates a PVS that is the inclusive or of all leafs within radius pixels
//of the given point.
static int Mod_Q1BSP_FatPVS(model_t *model, const vec3_t org, vec_t radius, qbyte *pvsbuffer, int pvsbufferlength)
{
	int bytes = ((model->brushq1.numleafs - 1) + 7) >> 3;
	bytes = min(bytes, pvsbufferlength);
	if (r_novis.integer)
	{
		memset(pvsbuffer, 0xFF, bytes);
		return bytes;
	}
	memset(pvsbuffer, 0, bytes);
	Mod_Q1BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, bytes, model->brushq1.nodes);
	return bytes;
}

//Returns PVS data for a given point
//(note: always returns valid data, never NULL)
static qbyte *Mod_Q1BSP_GetPVS(model_t *model, const vec3_t p)
{
	mnode_t *node;
	Mod_CheckLoaded(model);
	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];
	return ((mleaf_t *)node)->pvsdata;
}

static void Mod_Q1BSP_RoundUpToHullSize(model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs)
{
	vec3_t size;
	const hull_t *hull;

	VectorSubtract(inmaxs, inmins, size);
	if (cmodel->brush.ishlbsp)
	{
		if (size[0] < 3)
			hull = &cmodel->brushq1.hulls[0]; // 0x0x0
		else if (size[0] <= 32)
		{
			if (size[2] < 54) // pick the nearest of 36 or 72
				hull = &cmodel->brushq1.hulls[3]; // 32x32x36
			else
				hull = &cmodel->brushq1.hulls[1]; // 32x32x72
		}
		else
			hull = &cmodel->brushq1.hulls[2]; // 64x64x64
	}
	else
	{
		if (size[0] < 3)
			hull = &cmodel->brushq1.hulls[0]; // 0x0x0
		else if (size[0] <= 32)
			hull = &cmodel->brushq1.hulls[1]; // 32x32x56
		else
			hull = &cmodel->brushq1.hulls[2]; // 64x64x88
	}
	VectorCopy(inmins, outmins);
	VectorAdd(inmins, hull->clip_size, outmaxs);
}

extern void R_Model_Brush_DrawSky(entity_render_t *ent);
extern void R_Model_Brush_Draw(entity_render_t *ent);
extern void R_Model_Brush_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Brush_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_Q1BSP_Load(model_t *mod, void *buffer)
{
	int i, j, k;
	dheader_t *header;
	dmodel_t *bm;
	mempool_t *mainmempool;
	char *loadname;
	model_t *originalloadmodel;
	float dist, modelyawradius, modelradius, *vec;
	msurface_t *surf;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != BSPVERSION && i != 30)
		Host_Error("Mod_Q1BSP_Load: %s has wrong version number(%i should be %i(Quake) or 30(HalfLife))", mod->name, i, BSPVERSION);
	mod->brush.ishlbsp = i == 30;

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q1BSP_TraceBox;
	mod->brush.SuperContentsFromNativeContents = Mod_Q1BSP_SuperContentsFromNativeContents;
	mod->brush.NativeContentsFromSuperContents = Mod_Q1BSP_NativeContentsFromSuperContents;
	mod->brush.GetPVS = Mod_Q1BSP_GetPVS;
	mod->brush.FatPVS = Mod_Q1BSP_FatPVS;
	mod->brush.BoxTouchingPVS = Mod_Q1BSP_BoxTouchingPVS;
	mod->brush.LightPoint = Mod_Q1BSP_LightPoint;
	mod->brush.FindNonSolidLocation = Mod_Q1BSP_FindNonSolidLocation;
	mod->brush.AmbientSoundLevelsForPoint = Mod_Q1BSP_AmbientSoundLevelsForPoint;
	mod->brush.RoundUpToHullSize = Mod_Q1BSP_RoundUpToHullSize;
	mod->brushq1.PointInLeaf = Mod_Q1BSP_PointInLeaf;
	mod->brushq1.BuildPVSTextureChains = Mod_Q1BSP_BuildPVSTextureChains;

	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brush.ishlbsp);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i = 0;i < (int) sizeof(dheader_t) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

// load into heap

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	Mod_Q1BSP_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_Q1BSP_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_Q1BSP_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_Q1BSP_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_Q1BSP_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_Q1BSP_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_Q1BSP_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_Q1BSP_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_Q1BSP_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_Q1BSP_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_Q1BSP_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	// load submodels before leafs because they contain the number of vis leafs
	Mod_Q1BSP_LoadSubmodels(&header->lumps[LUMP_MODELS]);
	Mod_Q1BSP_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_Q1BSP_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_Q1BSP_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);

	if (mod->brushq1.data_compressedpvs)
		Mem_Free(mod->brushq1.data_compressedpvs);
	mod->brushq1.data_compressedpvs = NULL;
	mod->brushq1.num_compressedpvs = 0;

	Mod_Q1BSP_MakeHull0();
	Mod_Q1BSP_MakePortals();

	mod->numframes = 2;		// regular and alternate animation

	mainmempool = mod->mempool;
	loadname = mod->name;

	Mod_Q1BSP_LoadLightList();
	originalloadmodel = loadmodel;

//
// set up the submodels(FIXME: this is confusing)
//
	for (i = 0;i < mod->brush.numsubmodels;i++)
	{
		bm = &mod->brushq1.submodels[i];

		mod->brushq1.hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->brushq1.hulls[j].firstclipnode = bm->headnode[j];
			mod->brushq1.hulls[j].lastclipnode = mod->brushq1.numclipnodes - 1;
		}

		mod->brushq1.firstmodelsurface = bm->firstface;
		mod->brushq1.nummodelsurfaces = bm->numfaces;

		// this gets altered below if sky is used
		mod->DrawSky = NULL;
		mod->Draw = R_Model_Brush_Draw;
		mod->DrawShadowVolume = R_Model_Brush_DrawShadowVolume;
		mod->DrawLight = R_Model_Brush_DrawLight;
		mod->brushq1.pvstexturechains = Mem_Alloc(originalloadmodel->mempool, mod->brushq1.numtextures * sizeof(msurface_t **));
		mod->brushq1.pvstexturechainsbuffer = Mem_Alloc(originalloadmodel->mempool,(mod->brushq1.nummodelsurfaces + mod->brushq1.numtextures) * sizeof(msurface_t *));
		mod->brushq1.pvstexturechainslength = Mem_Alloc(originalloadmodel->mempool, mod->brushq1.numtextures * sizeof(int));
		Mod_Q1BSP_BuildPVSTextureChains(mod);
		Mod_Q1BSP_BuildLightmapUpdateChains(originalloadmodel->mempool, mod);
		if (mod->brushq1.nummodelsurfaces)
		{
			// LordHavoc: calculate bmodel bounding box rather than trusting what it says
			mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
			mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
			modelyawradius = 0;
			modelradius = 0;
			for (j = 0, surf = &mod->brushq1.surfaces[mod->brushq1.firstmodelsurface];j < mod->brushq1.nummodelsurfaces;j++, surf++)
			{
				// we only need to have a drawsky function if it is used(usually only on world model)
				if (surf->texinfo->texture->shader == &Cshader_sky)
					mod->DrawSky = R_Model_Brush_DrawSky;
				// LordHavoc: submodels always clip, even if water
				if (mod->brush.numsubmodels - 1)
					surf->flags |= SURF_SOLIDCLIP;
				// calculate bounding shapes
				for (k = 0, vec = surf->mesh.data_vertex3f;k < surf->mesh.num_vertices;k++, vec += 3)
				{
					if (mod->normalmins[0] > vec[0]) mod->normalmins[0] = vec[0];
					if (mod->normalmins[1] > vec[1]) mod->normalmins[1] = vec[1];
					if (mod->normalmins[2] > vec[2]) mod->normalmins[2] = vec[2];
					if (mod->normalmaxs[0] < vec[0]) mod->normalmaxs[0] = vec[0];
					if (mod->normalmaxs[1] < vec[1]) mod->normalmaxs[1] = vec[1];
					if (mod->normalmaxs[2] < vec[2]) mod->normalmaxs[2] = vec[2];
					dist = vec[0]*vec[0]+vec[1]*vec[1];
					if (modelyawradius < dist)
						modelyawradius = dist;
					dist += vec[2]*vec[2];
					if (modelradius < dist)
						modelradius = dist;
				}
			}
			modelyawradius = sqrt(modelyawradius);
			modelradius = sqrt(modelradius);
			mod->yawmins[0] = mod->yawmins[1] = - (mod->yawmaxs[0] = mod->yawmaxs[1] = modelyawradius);
			mod->yawmins[2] = mod->normalmins[2];
			mod->yawmaxs[2] = mod->normalmaxs[2];
			mod->rotatedmins[0] = mod->rotatedmins[1] = mod->rotatedmins[2] = -modelradius;
			mod->rotatedmaxs[0] = mod->rotatedmaxs[1] = mod->rotatedmaxs[2] = modelradius;
			mod->radius = modelradius;
			mod->radius2 = modelradius * modelradius;
		}
		else
		{
			// LordHavoc: empty submodel(lacrima.bsp has such a glitch)
			Con_Printf("warning: empty submodel *%i in %s\n", i+1, loadname);
		}
		Mod_Q1BSP_BuildSurfaceNeighbors(mod->brushq1.surfaces + mod->brushq1.firstmodelsurface, mod->brushq1.nummodelsurfaces, originalloadmodel->mempool);

		mod->brushq1.visleafs = bm->visleafs;

		// LordHavoc: only register submodels if it is the world
		// (prevents bsp models from replacing world submodels)
		if (loadmodel->isworldmodel && i < (mod->brush.numsubmodels - 1))
		{
			char	name[10];
			// duplicate the basic information
			sprintf(name, "*%i", i+1);
			loadmodel = Mod_FindName(name);
			*loadmodel = *mod;
			strcpy(loadmodel->name, name);
			// textures and memory belong to the main model
			loadmodel->texturepool = NULL;
			loadmodel->mempool = NULL;
			mod = loadmodel;
		}
	}

	loadmodel = originalloadmodel;
	//Mod_Q1BSP_ProcessLightList();

	if (developer.integer)
		Con_Printf("Some stats for q1bsp model \"%s\": %i faces, %i nodes, %i leafs, %i visleafs, %i visleafportals\n", loadmodel->name, loadmodel->brushq1.numsurfaces, loadmodel->brushq1.numnodes, loadmodel->brushq1.numleafs, loadmodel->brushq1.visleafs, loadmodel->brushq1.numportals);
}

static void Mod_Q2BSP_LoadEntities(lump_t *l)
{
}

static void Mod_Q2BSP_LoadPlanes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadVertices(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadVertices: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadVisibility(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadVisibility: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadNodes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadTexInfo(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadTexInfo: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadFaces(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLighting(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLighting: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafs(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafFaces(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafBrushes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadEdges(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadSurfEdges(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadSurfEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadBrushes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadBrushSides(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadBrushSides: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadAreas(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadAreas: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadAreaPortals(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadAreaPortals: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadModels(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadModels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

void static Mod_Q2BSP_Load(model_t *mod, void *buffer)
{
	int i;
	q2dheader_t *header;

	Host_Error("Mod_Q2BSP_Load: not yet implemented\n");

	mod->type = mod_brushq2;

	header = (q2dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q2BSPVERSION)
		Host_Error("Mod_Q2BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q2BSPVERSION);
	mod->brush.ishlbsp = false;
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brush.ishlbsp);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

	mod_base = (qbyte *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	Mod_Q2BSP_LoadEntities(&header->lumps[Q2LUMP_ENTITIES]);
	Mod_Q2BSP_LoadPlanes(&header->lumps[Q2LUMP_PLANES]);
	Mod_Q2BSP_LoadVertices(&header->lumps[Q2LUMP_VERTEXES]);
	Mod_Q2BSP_LoadVisibility(&header->lumps[Q2LUMP_VISIBILITY]);
	Mod_Q2BSP_LoadNodes(&header->lumps[Q2LUMP_NODES]);
	Mod_Q2BSP_LoadTexInfo(&header->lumps[Q2LUMP_TEXINFO]);
	Mod_Q2BSP_LoadFaces(&header->lumps[Q2LUMP_FACES]);
	Mod_Q2BSP_LoadLighting(&header->lumps[Q2LUMP_LIGHTING]);
	Mod_Q2BSP_LoadLeafs(&header->lumps[Q2LUMP_LEAFS]);
	Mod_Q2BSP_LoadLeafFaces(&header->lumps[Q2LUMP_LEAFFACES]);
	Mod_Q2BSP_LoadLeafBrushes(&header->lumps[Q2LUMP_LEAFBRUSHES]);
	Mod_Q2BSP_LoadEdges(&header->lumps[Q2LUMP_EDGES]);
	Mod_Q2BSP_LoadSurfEdges(&header->lumps[Q2LUMP_SURFEDGES]);
	Mod_Q2BSP_LoadBrushes(&header->lumps[Q2LUMP_BRUSHES]);
	Mod_Q2BSP_LoadBrushSides(&header->lumps[Q2LUMP_BRUSHSIDES]);
	Mod_Q2BSP_LoadAreas(&header->lumps[Q2LUMP_AREAS]);
	Mod_Q2BSP_LoadAreaPortals(&header->lumps[Q2LUMP_AREAPORTALS]);
	// LordHavoc: must go last because this makes the submodels
	Mod_Q2BSP_LoadModels(&header->lumps[Q2LUMP_MODELS]);
}

static int Mod_Q3BSP_SuperContentsFromNativeContents(model_t *model, int nativecontents);
static int Mod_Q3BSP_NativeContentsFromSuperContents(model_t *model, int supercontents);

static void Mod_Q3BSP_LoadEntities(lump_t *l)
{
	const char *data;
	char key[128], value[4096];
	float v[3];
	loadmodel->brushq3.num_lightgrid_cellsize[0] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[1] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[2] = 128;
	if (!l->filelen)
		return;
	loadmodel->brush.entities = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	data = loadmodel->brush.entities;
	// some Q3 maps override the lightgrid_cellsize with a worldspawn key
	if (data && COM_ParseToken(&data, false) && com_token[0] == '{')
	{
		while (1)
		{
			if (!COM_ParseToken(&data, false))
				break; // error
			if (com_token[0] == '}')
				break; // end of worldspawn
			if (com_token[0] == '_')
				strcpy(key, com_token + 1);
			else
				strcpy(key, com_token);
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken(&data, false))
				break; // error
			strcpy(value, com_token);
			if (!strcmp("gridsize", key))
			{
				if (sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]) == 3 && v[0] != 0 && v[1] != 0 && v[2] != 0)
					VectorCopy(v, loadmodel->brushq3.num_lightgrid_cellsize);
			}
		}
	}
}

static void Mod_Q3BSP_LoadTextures(lump_t *l)
{
	q3dtexture_t *in;
	q3mtexture_t *out;
	int i, count;
	int j, c;
	fssearch_t *search;
	char *f;
	const char *text;
	int flags;
	char shadername[Q3PATHLENGTH];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadTextures: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_textures = out;
	loadmodel->brushq3.num_textures = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->number = i;
		strlcpy (out->name, in->name, sizeof (out->name));
		out->surfaceflags = LittleLong(in->surfaceflags);
		out->nativecontents = LittleLong(in->contents);
		out->supercontents = Mod_Q3BSP_SuperContentsFromNativeContents(loadmodel, out->nativecontents);
		Mod_LoadSkinFrame(&out->skin, out->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, false, true, true);
		out->surfaceparms = -1;
	}

	// do a quick parse of shader files to get surfaceparms
	if ((search = FS_Search("scripts/*.shader", true, false)))
	{
		for (i = 0;i < search->numfilenames;i++)
		{
			if ((f = FS_LoadFile(search->filenames[i], false)))
			{
				text = f;
				while (COM_ParseToken(&text, false))
				{
					snprintf(shadername, sizeof(shadername), "%s", com_token);
					flags = 0;
					if (COM_ParseToken(&text, false) && !strcmp(com_token, "{"))
					{
						while (COM_ParseToken(&text, false))
						{
							if (!strcmp(com_token, "}"))
								break;
							else if (!strcmp(com_token, "{"))
							{
								while (COM_ParseToken(&text, false))
								{
									if (!strcmp(com_token, "}"))
										break;
								}
							}
							else if (!strcmp(com_token, "surfaceparm"))
							{
								if (COM_ParseToken(&text, true) && strcmp(com_token, "\n"))
								{
									if (!strcmp(com_token, "alphashadow"))
										flags |= Q3SURFACEPARM_ALPHASHADOW;
									else if (!strcmp(com_token, "areaportal"))
										flags |= Q3SURFACEPARM_AREAPORTAL;
									else if (!strcmp(com_token, "clusterportal"))
										flags |= Q3SURFACEPARM_CLUSTERPORTAL;
									else if (!strcmp(com_token, "detail"))
										flags |= Q3SURFACEPARM_DETAIL;
									else if (!strcmp(com_token, "donotenter"))
										flags |= Q3SURFACEPARM_DONOTENTER;
									else if (!strcmp(com_token, "fog"))
										flags |= Q3SURFACEPARM_FOG;
									else if (!strcmp(com_token, "lava"))
										flags |= Q3SURFACEPARM_LAVA;
									else if (!strcmp(com_token, "lightfilter"))
										flags |= Q3SURFACEPARM_LIGHTFILTER;
									else if (!strcmp(com_token, "metalsteps"))
										flags |= Q3SURFACEPARM_METALSTEPS;
									else if (!strcmp(com_token, "nodamage"))
										flags |= Q3SURFACEPARM_NODAMAGE;
									else if (!strcmp(com_token, "nodlight"))
										flags |= Q3SURFACEPARM_NODLIGHT;
									else if (!strcmp(com_token, "nodraw"))
										flags |= Q3SURFACEPARM_NODRAW;
									else if (!strcmp(com_token, "nodrop"))
										flags |= Q3SURFACEPARM_NODROP;
									else if (!strcmp(com_token, "noimpact"))
										flags |= Q3SURFACEPARM_NOIMPACT;
									else if (!strcmp(com_token, "nolightmap"))
										flags |= Q3SURFACEPARM_NOLIGHTMAP;
									else if (!strcmp(com_token, "nomarks"))
										flags |= Q3SURFACEPARM_NOMARKS;
									else if (!strcmp(com_token, "nomipmaps"))
										flags |= Q3SURFACEPARM_NOMIPMAPS;
									else if (!strcmp(com_token, "nonsolid"))
										flags |= Q3SURFACEPARM_NONSOLID;
									else if (!strcmp(com_token, "origin"))
										flags |= Q3SURFACEPARM_ORIGIN;
									else if (!strcmp(com_token, "playerclip"))
										flags |= Q3SURFACEPARM_PLAYERCLIP;
									else if (!strcmp(com_token, "sky"))
										flags |= Q3SURFACEPARM_SKY;
									else if (!strcmp(com_token, "slick"))
										flags |= Q3SURFACEPARM_SLICK;
									else if (!strcmp(com_token, "slime"))
										flags |= Q3SURFACEPARM_SLIME;
									else if (!strcmp(com_token, "structural"))
										flags |= Q3SURFACEPARM_STRUCTURAL;
									else if (!strcmp(com_token, "trans"))
										flags |= Q3SURFACEPARM_TRANS;
									else if (!strcmp(com_token, "water"))
										flags |= Q3SURFACEPARM_WATER;
									else
										Con_Printf("%s parsing warning: unknown surfaceparm \"%s\"\n", search->filenames[i], com_token);
									if (!COM_ParseToken(&text, true) || strcmp(com_token, "\n"))
									{
										Con_Printf("%s parsing error: surfaceparm only takes one parameter.\n", search->filenames[i]);
										goto parseerror;
									}
								}
								else
								{
									Con_Printf("%s parsing error: surfaceparm expects a parameter.\n", search->filenames[i]);
									goto parseerror;
								}
							}
							else
							{
								// look for linebreak or }
								while(COM_ParseToken(&text, true) && strcmp(com_token, "\n") && strcmp(com_token, "}"));
								// break out to top level if it was }
								if (!strcmp(com_token, "}"))
									break;
							}
						}
						// add shader to list (shadername and flags)
						// actually here we just poke into the texture settings
						for (j = 0, out = loadmodel->brushq3.data_textures;j < loadmodel->brushq3.num_textures;j++, out++)
							if (!strcmp(out->name, shadername))
								out->surfaceparms = flags;
					}
					else
					{
						Con_Printf("%s parsing error - expected \"{\", found \"%s\"\n", search->filenames[i], com_token);
						goto parseerror;
					}
				}
parseerror:
				Mem_Free(f);
			}
		}
	}

	c = 0;
	for (j = 0, out = loadmodel->brushq3.data_textures;j < loadmodel->brushq3.num_textures;j++, out++)
	{
		if (out->surfaceparms == -1)
		{
			c++;
			Con_DPrintf("%s: No shader found for texture \"%s\"\n", loadmodel->name, out->name);
			out->surfaceparms = 0;
			// these are defaults
			if (!strcmp(out->name, "caulk") || !strcmp(out->name, "common/caulk") || !strcmp(out->name, "textures/common/caulk")
			 || !strcmp(out->name, "nodraw") || !strcmp(out->name, "common/nodraw") || !strcmp(out->name, "textures/common/nodraw"))
				out->surfaceparms |= Q3SURFACEPARM_NODRAW;
			if (!strncmp(out->name, "textures/skies/", 15))
				out->surfaceparms |= Q3SURFACEPARM_SKY;
			if (R_TextureHasAlpha(out->skin.base))
				out->surfaceparms |= Q3SURFACEPARM_TRANS;
		}
	}
	Con_DPrintf("%s: %i textures missing shaders\n", loadmodel->name, c);
}

static void Mod_Q3BSP_LoadPlanes(lump_t *l)
{
	q3dplane_t *in;
	mplane_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_planes = out;
	loadmodel->brushq3.num_planes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->normal[0] = LittleLong(in->normal[0]);
		out->normal[1] = LittleLong(in->normal[1]);
		out->normal[2] = LittleLong(in->normal[2]);
		out->dist = LittleLong(in->dist);
		PlaneClassify(out);
	}
}

static void Mod_Q3BSP_LoadBrushSides(lump_t *l)
{
	q3dbrushside_t *in;
	q3mbrushside_t *out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushSides: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_brushsides = out;
	loadmodel->brushq3.num_brushsides = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brushq3.num_planes)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid planeindex %i (%i planes)\n", n, loadmodel->brushq3.num_planes);
		out->plane = loadmodel->brushq3.data_planes + n;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid textureindex %i (%i textures)\n", n, loadmodel->brushq3.num_textures);
		out->texture = loadmodel->brushq3.data_textures + n;
	}
}

static void Mod_Q3BSP_LoadBrushes(lump_t *l)
{
	q3dbrush_t *in;
	q3mbrush_t *out;
	int i, j, n, c, count, maxplanes;
	mplane_t *planes;
	winding_t *temp1, *temp2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_brushes = out;
	loadmodel->brushq3.num_brushes = count;

	temp1 = Winding_New(64);
	temp2 = Winding_New(64);

	maxplanes = 0;
	planes = NULL;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->firstbrushside);
		c = LittleLong(in->numbrushsides);
		if (n < 0 || n + c > loadmodel->brushq3.num_brushsides)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid brushside range %i : %i (%i brushsides)\n", n, n + c, loadmodel->brushq3.num_brushsides);
		out->firstbrushside = loadmodel->brushq3.data_brushsides + n;
		out->numbrushsides = c;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid textureindex %i (%i textures)\n", n, loadmodel->brushq3.num_textures);
		out->texture = loadmodel->brushq3.data_textures + n;

		// make a list of mplane_t structs to construct a colbrush from
		if (maxplanes < out->numbrushsides)
		{
			maxplanes = out->numbrushsides;
			if (planes)
				Mem_Free(planes);
			planes = Mem_Alloc(tempmempool, sizeof(mplane_t) * maxplanes);
		}
		for (j = 0;j < out->numbrushsides;j++)
		{
			VectorCopy(out->firstbrushside[j].plane->normal, planes[j].normal);
			planes[j].dist = out->firstbrushside[j].plane->dist;
		}
		// make the colbrush from the planes
		out->colbrushf = Collision_NewBrushFromPlanes(loadmodel->mempool, out->numbrushsides, planes, out->texture->supercontents, temp1, temp2);
	}
	if (planes)
		Mem_Free(planes);
	Winding_Free(temp1);
	Winding_Free(temp2);
}

static void Mod_Q3BSP_LoadEffects(lump_t *l)
{
	q3deffect_t *in;
	q3meffect_t *out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadEffects: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_effects = out;
	loadmodel->brushq3.num_effects = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		strlcpy (out->shadername, in->shadername, sizeof (out->shadername));
		n = LittleLong(in->brushindex);
		if (n < 0 || n >= loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadEffects: invalid brushindex %i (%i brushes)\n", n, loadmodel->brushq3.num_brushes);
		out->brush = loadmodel->brushq3.data_brushes + n;
		out->unknown = LittleLong(in->unknown);
	}
}

static void Mod_Q3BSP_LoadVertices(lump_t *l)
{
	q3dvertex_t *in;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadVertices: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_vertices = count = l->filelen / sizeof(*in);
	loadmodel->brushq3.data_vertex3f = Mem_Alloc(loadmodel->mempool, count * (sizeof(float) * (3 + 2 + 2 + 3 + 3 + 3 + 4)));
	loadmodel->brushq3.data_texcoordtexture2f = loadmodel->brushq3.data_vertex3f + count * 3;
	loadmodel->brushq3.data_texcoordlightmap2f = loadmodel->brushq3.data_texcoordtexture2f + count * 2;
	loadmodel->brushq3.data_svector3f = loadmodel->brushq3.data_texcoordlightmap2f + count * 2;
	loadmodel->brushq3.data_tvector3f = loadmodel->brushq3.data_svector3f + count * 3;
	loadmodel->brushq3.data_normal3f = loadmodel->brushq3.data_tvector3f + count * 3;
	loadmodel->brushq3.data_color4f = loadmodel->brushq3.data_normal3f + count * 3;

	for (i = 0;i < count;i++, in++)
	{
		loadmodel->brushq3.data_vertex3f[i * 3 + 0] = LittleFloat(in->origin3f[0]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 1] = LittleFloat(in->origin3f[1]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 2] = LittleFloat(in->origin3f[2]);
		loadmodel->brushq3.data_texcoordtexture2f[i * 2 + 0] = LittleFloat(in->texcoord2f[0]);
		loadmodel->brushq3.data_texcoordtexture2f[i * 2 + 1] = LittleFloat(in->texcoord2f[1]);
		loadmodel->brushq3.data_texcoordlightmap2f[i * 2 + 0] = LittleFloat(in->lightmap2f[0]);
		loadmodel->brushq3.data_texcoordlightmap2f[i * 2 + 1] = LittleFloat(in->lightmap2f[1]);
		// svector/tvector are calculated later in face loading
		loadmodel->brushq3.data_svector3f[i * 3 + 0] = 0;
		loadmodel->brushq3.data_svector3f[i * 3 + 1] = 0;
		loadmodel->brushq3.data_svector3f[i * 3 + 2] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 0] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 1] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 2] = 0;
		loadmodel->brushq3.data_normal3f[i * 3 + 0] = LittleFloat(in->normal3f[0]);
		loadmodel->brushq3.data_normal3f[i * 3 + 1] = LittleFloat(in->normal3f[1]);
		loadmodel->brushq3.data_normal3f[i * 3 + 2] = LittleFloat(in->normal3f[2]);
		loadmodel->brushq3.data_color4f[i * 4 + 0] = in->color4ub[0] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 1] = in->color4ub[1] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 2] = in->color4ub[2] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 3] = in->color4ub[3] * (1.0f / 255.0f);
	}
}

static void Mod_Q3BSP_LoadTriangles(lump_t *l)
{
	int *in;
	int *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(int[3]))
		Host_Error("Mod_Q3BSP_LoadTriangles: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out) * 2);

	loadmodel->brushq3.num_triangles = count / 3;
	loadmodel->brushq3.data_element3i = out;
	loadmodel->brushq3.data_neighbor3i = out + count;

	for (i = 0;i < count;i++, in++, out++)
	{
		*out = LittleLong(*in);
		if (*out < 0 || *out >= loadmodel->brushq3.num_vertices)
		{
			Con_Printf("Mod_Q3BSP_LoadTriangles: invalid vertexindex %i (%i vertices), setting to 0\n", *out, loadmodel->brushq3.num_vertices);
			*out = 0;
		}
	}
}

static void Mod_Q3BSP_LoadLightmaps(lump_t *l)
{
	q3dlightmap_t *in;
	rtexture_t **out;
	int i, count;

	if (!l->filelen)
		return;
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightmaps: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_lightmaps = out;
	loadmodel->brushq3.num_lightmaps = count;

	for (i = 0;i < count;i++, in++, out++)
		*out = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%04i", i), 128, 128, in->rgb, TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
}

static void Mod_Q3BSP_LoadFaces(lump_t *l)
{
	q3dface_t *in;
	q3mface_t *out;
	int i, j, n, count, invalidelements, patchsize[2], finalwidth, finalheight, xlevel, ylevel, row0, row1, x, y, *e, finalvertices, finaltriangles;
	//int *originalelement3i;
	//int *originalneighbor3i;
	float *originalvertex3f;
	//float *originalsvector3f;
	//float *originaltvector3f;
	//float *originalnormal3f;
	float *originalcolor4f;
	float *originaltexcoordtexture2f;
	float *originaltexcoordlightmap2f;
	float *v;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_faces = out;
	loadmodel->brushq3.num_faces = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		// check face type first
		out->type = LittleLong(in->type);
		if (out->type != Q3FACETYPE_POLYGON
		 && out->type != Q3FACETYPE_PATCH
		 && out->type != Q3FACETYPE_MESH
		 && out->type != Q3FACETYPE_FLARE)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i: unknown face type %i\n", i, out->type);
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0; // error
			continue;
		}

		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i: invalid textureindex %i (%i textures)\n", i, n, loadmodel->brushq3.num_textures);
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0; // error
			continue;
			n = 0;
		}
		out->texture = loadmodel->brushq3.data_textures + n;
		n = LittleLong(in->effectindex);
		if (n < -1 || n >= loadmodel->brushq3.num_effects)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid effectindex %i (%i effects)\n", i, out->texture->name, n, loadmodel->brushq3.num_effects);
			n = -1;
		}
		if (n == -1)
			out->effect = NULL;
		else
			out->effect = loadmodel->brushq3.data_effects + n;
		n = LittleLong(in->lightmapindex);
		if (n < -1 || n >= loadmodel->brushq3.num_lightmaps)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid lightmapindex %i (%i lightmaps)\n", i, out->texture->name, n, loadmodel->brushq3.num_lightmaps);
			n = -1;
		}
		if (n == -1)
			out->lightmaptexture = NULL;
		else
			out->lightmaptexture = loadmodel->brushq3.data_lightmaps[n];

		out->firstvertex = LittleLong(in->firstvertex);
		out->num_vertices = LittleLong(in->numvertices);
		out->firstelement = LittleLong(in->firstelement);
		out->num_triangles = LittleLong(in->numelements) / 3;
		if (out->num_triangles * 3 != LittleLong(in->numelements))
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): numelements %i is not a multiple of 3\n", i, out->texture->name, LittleLong(in->numelements));
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0; // error
			continue;
		}
		if (out->firstvertex < 0 || out->firstvertex + out->num_vertices > loadmodel->brushq3.num_vertices)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid vertex range %i : %i (%i vertices)\n", i, out->texture->name, out->firstvertex, out->firstvertex + out->num_vertices, loadmodel->brushq3.num_vertices);
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0; // error
			continue;
		}
		if (out->firstelement < 0 || out->firstelement + out->num_triangles * 3 > loadmodel->brushq3.num_triangles * 3)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid element range %i : %i (%i elements)\n", i, out->texture->name, out->firstelement, out->firstelement + out->num_triangles * 3, loadmodel->brushq3.num_triangles * 3);
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0; // error
			continue;
		}
		switch(out->type)
		{
		case Q3FACETYPE_POLYGON:
		case Q3FACETYPE_MESH:
			// no processing necessary
			out->data_vertex3f = loadmodel->brushq3.data_vertex3f + out->firstvertex * 3;
			out->data_texcoordtexture2f = loadmodel->brushq3.data_texcoordtexture2f + out->firstvertex * 2;
			out->data_texcoordlightmap2f = loadmodel->brushq3.data_texcoordlightmap2f + out->firstvertex * 2;
			out->data_svector3f = loadmodel->brushq3.data_svector3f + out->firstvertex * 3;
			out->data_tvector3f = loadmodel->brushq3.data_tvector3f + out->firstvertex * 3;
			out->data_normal3f = loadmodel->brushq3.data_normal3f + out->firstvertex * 3;
			out->data_color4f = loadmodel->brushq3.data_color4f + out->firstvertex * 4;
			out->data_element3i = loadmodel->brushq3.data_element3i + out->firstelement;
			out->data_neighbor3i = loadmodel->brushq3.data_neighbor3i + out->firstelement;
			break;
		case Q3FACETYPE_PATCH:
			patchsize[0] = LittleLong(in->specific.patch.patchsize[0]);
			patchsize[1] = LittleLong(in->specific.patch.patchsize[1]);
			if (patchsize[0] < 1 || patchsize[1] < 1)
			{
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid patchsize %ix%i\n", i, out->texture->name, patchsize[0], patchsize[1]);
				out->num_vertices = 0;
				out->num_triangles = 0;
				out->type = 0; // error
				continue;
			}
			// convert patch to Q3FACETYPE_MESH
			xlevel = mod_q3bsp_curves_subdivide_level.integer;
			ylevel = mod_q3bsp_curves_subdivide_level.integer;
			finalwidth = ((patchsize[0] - 1) << xlevel) + 1;
			finalheight = ((patchsize[1] - 1) << ylevel) + 1;
			finalvertices = finalwidth * finalheight;
			finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;
			originalvertex3f = loadmodel->brushq3.data_vertex3f + out->firstvertex * 3;
			//originalsvector3f = loadmodel->brushq3.data_svector3f + out->firstvertex * 3;
			//originaltvector3f = loadmodel->brushq3.data_tvector3f + out->firstvertex * 3;
			//originalnormal3f = loadmodel->brushq3.data_normal3f + out->firstvertex * 3;
			originaltexcoordtexture2f = loadmodel->brushq3.data_texcoordtexture2f + out->firstvertex * 2;
			originaltexcoordlightmap2f = loadmodel->brushq3.data_texcoordlightmap2f + out->firstvertex * 2;
			originalcolor4f = loadmodel->brushq3.data_color4f + out->firstvertex * 4;
			//originalelement3i = loadmodel->brushq3.data_element3i + out->firstelement;
			//originalneighbor3i = loadmodel->brushq3.data_neighbor3i + out->firstelement;
			/*
			originalvertex3f = out->data_vertex3f;
			//originalsvector3f = out->data_svector3f;
			//originaltvector3f = out->data_tvector3f;
			//originalnormal3f = out->data_normal3f;
			originalcolor4f = out->data_color4f;
			originaltexcoordtexture2f = out->data_texcoordtexture2f;
			originaltexcoordlightmap2f = out->data_texcoordlightmap2f;
			//originalelement3i = out->data_element3i;
			//originalneighbor3i = out->data_neighbor3i;
			*/
			out->data_vertex3f = Mem_Alloc(loadmodel->mempool, sizeof(float[20]) * finalvertices);
			out->data_svector3f = out->data_vertex3f + finalvertices * 3;
			out->data_tvector3f = out->data_svector3f + finalvertices * 3;
			out->data_normal3f = out->data_tvector3f + finalvertices * 3;
			out->data_color4f = out->data_normal3f + finalvertices * 3;
			out->data_texcoordtexture2f = out->data_color4f + finalvertices * 4;
			out->data_texcoordlightmap2f = out->data_texcoordtexture2f + finalvertices * 2;
			out->data_element3i = Mem_Alloc(loadmodel->mempool, sizeof(int[6]) * finaltriangles);
			out->data_neighbor3i = out->data_element3i + finaltriangles * 3;
			out->type = Q3FACETYPE_MESH;
			out->firstvertex = -1;
			out->num_vertices = finalvertices;
			out->firstelement = -1;
			out->num_triangles = finaltriangles;
			// generate geometry
			// (note: normals are skipped because they get recalculated)
			QuadraticSplinePatchSubdivideFloatBuffer(patchsize[0], patchsize[1], xlevel, ylevel, 3, originalvertex3f, out->data_vertex3f);
			QuadraticSplinePatchSubdivideFloatBuffer(patchsize[0], patchsize[1], xlevel, ylevel, 2, originaltexcoordtexture2f, out->data_texcoordtexture2f);
			QuadraticSplinePatchSubdivideFloatBuffer(patchsize[0], patchsize[1], xlevel, ylevel, 2, originaltexcoordlightmap2f, out->data_texcoordlightmap2f);
			QuadraticSplinePatchSubdivideFloatBuffer(patchsize[0], patchsize[1], xlevel, ylevel, 4, originalcolor4f, out->data_color4f);
			// generate elements
			e = out->data_element3i;
			for (y = 0;y < finalheight - 1;y++)
			{
				row0 = (y + 0) * finalwidth;
				row1 = (y + 1) * finalwidth;
				for (x = 0;x < finalwidth - 1;x++)
				{
					*e++ = row0;
					*e++ = row1;
					*e++ = row0 + 1;
					*e++ = row1;
					*e++ = row1 + 1;
					*e++ = row0 + 1;
					row0++;
					row1++;
				}
			}
			out->num_triangles = Mod_RemoveDegenerateTriangles(out->num_triangles, out->data_element3i, out->data_element3i, out->data_vertex3f);
			if (developer.integer)
			{
				if (out->num_triangles < finaltriangles)
					Con_Printf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles, %i degenerate triangles removed (leaving %i)\n", patchsize[0], patchsize[1], out->num_vertices, finaltriangles, finaltriangles - out->num_triangles, out->num_triangles);
				else
					Con_Printf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles\n", patchsize[0], patchsize[1], out->num_vertices, out->num_triangles);
			}
			// q3map does not put in collision brushes for curves... ugh
			out->collisions = true;
			break;
		case Q3FACETYPE_FLARE:
			Con_DPrintf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): Q3FACETYPE_FLARE not supported (yet)\n", i, out->texture->name);
			// don't render it
			out->num_vertices = 0;
			out->num_triangles = 0;
			out->type = 0;
			break;
		}
		for (j = 0, invalidelements = 0;j < out->num_triangles * 3;j++)
			if (out->data_element3i[j] < 0 || out->data_element3i[j] >= out->num_vertices)
				invalidelements++;
		if (invalidelements)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: Warning: face #%i has %i invalid elements, type = %i, texture->name = \"%s\", texture->surfaceflags = %i, texture->nativecontents = %i, firstvertex = %i, numvertices = %i, firstelement = %i, numelements = %i, elements list:\n", i, invalidelements, out->type, out->texture->name, out->texture->surfaceflags, out->texture->nativecontents, out->firstvertex, out->num_vertices, out->firstelement, out->num_triangles * 3);
			for (j = 0;j < out->num_triangles * 3;j++)
			{
				Con_Printf(" %i", out->data_element3i[j]);
				if (out->data_element3i[j] < 0 || out->data_element3i[j] >= out->num_vertices)
					out->data_element3i[j] = 0;
			}
			Con_Printf("\n");
		}
		// for shadow volumes
		Mod_BuildTriangleNeighbors(out->data_neighbor3i, out->data_element3i, out->num_triangles);
		// for per pixel lighting
		Mod_BuildTextureVectorsAndNormals(out->num_vertices, out->num_triangles, out->data_vertex3f, out->data_texcoordtexture2f, out->data_element3i, out->data_svector3f, out->data_tvector3f, out->data_normal3f);
		// calculate a bounding box
		VectorClear(out->mins);
		VectorClear(out->maxs);
		if (out->num_vertices)
		{
			VectorCopy(out->data_vertex3f, out->mins);
			VectorCopy(out->data_vertex3f, out->maxs);
			for (j = 1, v = out->data_vertex3f + 3;j < out->num_vertices;j++, v += 3)
			{
				out->mins[0] = min(out->mins[0], v[0]);
				out->maxs[0] = max(out->maxs[0], v[0]);
				out->mins[1] = min(out->mins[1], v[1]);
				out->maxs[1] = max(out->maxs[1], v[1]);
				out->mins[2] = min(out->mins[2], v[2]);
				out->maxs[2] = max(out->maxs[2], v[2]);
			}
			out->mins[0] -= 1.0f;
			out->mins[1] -= 1.0f;
			out->mins[2] -= 1.0f;
			out->maxs[0] += 1.0f;
			out->maxs[1] += 1.0f;
			out->maxs[2] += 1.0f;
		}
	}

	// LordHavoc: experimental array merger (disabled because it wastes time and uses 2x memory while merging)
	/*
	{
	int totalverts, totaltris;
	int originalnum_vertices;
	float *originaldata_vertex3f;
	float *originaldata_texcoordtexture2f;
	float *originaldata_texcoordlightmap2f;
	float *originaldata_svector3f;
	float *originaldata_tvector3f;
	float *originaldata_normal3f;
	float *originaldata_color4f;
	int originalnum_triangles;
	int *originaldata_element3i;
	int *originaldata_neighbor3i;

	totalverts = 0;
	totaltris = 0;
	for (i = 0, out = loadmodel->brushq3.data_faces;i < count;i++, out++)
	{
		if (!out->type)
			continue;
		totalverts += out->num_vertices;
		totaltris += out->num_triangles;
	}

	originalnum_vertices = loadmodel->brushq3.num_vertices;
	originaldata_vertex3f = loadmodel->brushq3.data_vertex3f;
	originaldata_texcoordtexture2f = loadmodel->brushq3.data_texcoordtexture2f;
	originaldata_texcoordlightmap2f = loadmodel->brushq3.data_texcoordlightmap2f;
	originaldata_svector3f = loadmodel->brushq3.data_svector3f;
	originaldata_tvector3f = loadmodel->brushq3.data_tvector3f;
	originaldata_normal3f = loadmodel->brushq3.data_normal3f;
	originaldata_color4f = loadmodel->brushq3.data_color4f;
	originalnum_triangles = loadmodel->brushq3.num_triangles;
	originaldata_element3i = loadmodel->brushq3.data_element3i;
	originaldata_neighbor3i = loadmodel->brushq3.data_neighbor3i;
	loadmodel->brushq3.num_vertices = totalverts;
	loadmodel->brushq3.data_vertex3f = Mem_Alloc(loadmodel->mempool, totalverts * (sizeof(float) * (3 + 2 + 2 + 3 + 3 + 3 + 4)) + totaltris * (sizeof(int) * (3 * 2)));
	loadmodel->brushq3.data_texcoordtexture2f = loadmodel->brushq3.data_vertex3f + totalverts * 3;
	loadmodel->brushq3.data_texcoordlightmap2f = loadmodel->brushq3.data_texcoordtexture2f + totalverts * 2;
	loadmodel->brushq3.data_svector3f = loadmodel->brushq3.data_texcoordlightmap2f + totalverts * 2;
	loadmodel->brushq3.data_tvector3f = loadmodel->brushq3.data_svector3f + totalverts * 3;
	loadmodel->brushq3.data_normal3f = loadmodel->brushq3.data_tvector3f + totalverts * 3;
	loadmodel->brushq3.data_color4f = loadmodel->brushq3.data_normal3f + totalverts * 3;
	loadmodel->brushq3.num_triangles = totaltris;
	loadmodel->brushq3.data_element3i = (int *)(loadmodel->brushq3.data_color4f + totalverts * 4);
	loadmodel->brushq3.data_neighbor3i = loadmodel->brushq3.data_element3i + totaltris * 3;
	totalverts = 0;
	totaltris = 0;
	for (i = 0, out = loadmodel->brushq3.data_faces;i < count;i++, out++)
	{
		if (!out->type)
			continue;
		Con_Printf("totalverts %i, totaltris %i\n", totalverts, totaltris);
		memcpy(loadmodel->brushq3.data_vertex3f + totalverts * 3, out->data_vertex3f, out->num_vertices * 3 * sizeof(float));
		memcpy(loadmodel->brushq3.data_texcoordtexture2f + totalverts * 2, out->data_texcoordtexture2f, out->num_vertices * 2 * sizeof(float));
		memcpy(loadmodel->brushq3.data_texcoordlightmap2f + totalverts * 2, out->data_texcoordlightmap2f, out->num_vertices * 2 * sizeof(float));
		memcpy(loadmodel->brushq3.data_svector3f + totalverts * 3, out->data_svector3f, out->num_vertices * 3 * sizeof(float));
		memcpy(loadmodel->brushq3.data_tvector3f + totalverts * 3, out->data_tvector3f, out->num_vertices * 3 * sizeof(float));
		memcpy(loadmodel->brushq3.data_normal3f + totalverts * 3, out->data_normal3f, out->num_vertices * 3 * sizeof(float));
		memcpy(loadmodel->brushq3.data_color4f + totalverts * 4, out->data_color4f, out->num_vertices * 4 * sizeof(float));
		memcpy(loadmodel->brushq3.data_element3i + totaltris * 3, out->data_element3i, out->num_triangles * 3 * sizeof(int));
		memcpy(loadmodel->brushq3.data_neighbor3i + totaltris * 3, out->data_neighbor3i, out->num_triangles * 3 * sizeof(int));
		if (out->firstvertex == -1)
			Mem_Free(out->data_vertex3f);
		if (out->firstelement == -1)
			Mem_Free(out->data_element3i);
		out->firstvertex = totalverts;
		out->data_vertex3f = loadmodel->brushq3.data_vertex3f + out->firstvertex * 3;
		out->data_texcoordtexture2f = loadmodel->brushq3.data_texcoordtexture2f + out->firstvertex * 2;
		out->data_texcoordlightmap2f = loadmodel->brushq3.data_texcoordlightmap2f + out->firstvertex * 2;
		out->data_svector3f = loadmodel->brushq3.data_svector3f + out->firstvertex * 3;
		out->data_tvector3f = loadmodel->brushq3.data_tvector3f + out->firstvertex * 3;
		out->data_normal3f = loadmodel->brushq3.data_normal3f + out->firstvertex * 3;
		out->data_color4f = loadmodel->brushq3.data_color4f + out->firstvertex * 4;
		out->firstelement = totaltris * 3;
		out->data_element3i = loadmodel->brushq3.data_element3i + out->firstelement;
		out->data_neighbor3i = loadmodel->brushq3.data_neighbor3i + out->firstelement;
		//for (j = 0;j < out->numtriangles * 3;j++)
		//	out->data_element3i[j] += totalverts - out->firstvertex;
		totalverts += out->num_vertices;
		totaltris += out->num_triangles;
	}
	Mem_Free(originaldata_vertex3f);
	Mem_Free(originaldata_element3i);
	}
	*/
}

static void Mod_Q3BSP_LoadModels(lump_t *l)
{
	q3dmodel_t *in;
	q3mmodel_t *out;
	int i, j, n, c, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadModels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_models = out;
	loadmodel->brushq3.num_models = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		for (j = 0;j < 3;j++)
		{
			out->mins[j] = LittleFloat(in->mins[j]);
			out->maxs[j] = LittleFloat(in->maxs[j]);
		}
		n = LittleLong(in->firstface);
		c = LittleLong(in->numfaces);
		if (n < 0 || n + c > loadmodel->brushq3.num_faces)
			Host_Error("Mod_Q3BSP_LoadModels: invalid face range %i : %i (%i faces)\n", n, n + c, loadmodel->brushq3.num_faces);
		out->firstface = loadmodel->brushq3.data_faces + n;
		out->numfaces = c;
		n = LittleLong(in->firstbrush);
		c = LittleLong(in->numbrushes);
		if (n < 0 || n + c > loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadModels: invalid brush range %i : %i (%i brushes)\n", n, n + c, loadmodel->brushq3.num_brushes);
		out->firstbrush = loadmodel->brushq3.data_brushes + n;
		out->numbrushes = c;
	}
}

static void Mod_Q3BSP_LoadLeafBrushes(lump_t *l)
{
	int *in;
	q3mbrush_t **out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leafbrushes = out;
	loadmodel->brushq3.num_leafbrushes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadLeafBrushes: invalid brush index %i (%i brushes)\n", n, loadmodel->brushq3.num_brushes);
		*out = loadmodel->brushq3.data_brushes + n;
	}
}

static void Mod_Q3BSP_LoadLeafFaces(lump_t *l)
{
	int *in;
	q3mface_t **out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leaffaces = out;
	loadmodel->brushq3.num_leaffaces = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->brushq3.num_faces)
			Host_Error("Mod_Q3BSP_LoadLeafFaces: invalid face index %i (%i faces)\n", n, loadmodel->brushq3.num_faces);
		*out = loadmodel->brushq3.data_faces + n;
	}
}

static void Mod_Q3BSP_LoadLeafs(lump_t *l)
{
	q3dleaf_t *in;
	q3mleaf_t *out;
	int i, j, n, c, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leafs = out;
	loadmodel->brushq3.num_leafs = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->parent = NULL;
		out->plane = NULL;
		out->clusterindex = LittleLong(in->clusterindex);
		out->areaindex = LittleLong(in->areaindex);
		for (j = 0;j < 3;j++)
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong(in->mins[j]) - 1;
			out->maxs[j] = LittleLong(in->maxs[j]) + 1;
		}
		n = LittleLong(in->firstleafface);
		c = LittleLong(in->numleaffaces);
		if (n < 0 || n + c > loadmodel->brushq3.num_leaffaces)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafface range %i : %i (%i leaffaces)\n", n, n + c, loadmodel->brushq3.num_leaffaces);
		out->firstleafface = loadmodel->brushq3.data_leaffaces + n;
		out->numleaffaces = c;
		n = LittleLong(in->firstleafbrush);
		c = LittleLong(in->numleafbrushes);
		if (n < 0 || n + c > loadmodel->brushq3.num_leafbrushes)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafbrush range %i : %i (%i leafbrushes)\n", n, n + c, loadmodel->brushq3.num_leafbrushes);
		out->firstleafbrush = loadmodel->brushq3.data_leafbrushes + n;
		out->numleafbrushes = c;
	}
}

static void Mod_Q3BSP_LoadNodes_RecursiveSetParent(q3mnode_t *node, q3mnode_t *parent)
{
	if (node->parent)
		Host_Error("Mod_Q3BSP_LoadNodes_RecursiveSetParent: runaway recursion\n");
	node->parent = parent;
	if (node->plane)
	{
		Mod_Q3BSP_LoadNodes_RecursiveSetParent(node->children[0], node);
		Mod_Q3BSP_LoadNodes_RecursiveSetParent(node->children[1], node);
	}
}

static void Mod_Q3BSP_LoadNodes(lump_t *l)
{
	q3dnode_t *in;
	q3mnode_t *out;
	int i, j, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_nodes = out;
	loadmodel->brushq3.num_nodes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->parent = NULL;
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brushq3.num_planes)
			Host_Error("Mod_Q3BSP_LoadNodes: invalid planeindex %i (%i planes)\n", n, loadmodel->brushq3.num_planes);
		out->plane = loadmodel->brushq3.data_planes + n;
		for (j = 0;j < 2;j++)
		{
			n = LittleLong(in->childrenindex[j]);
			if (n >= 0)
			{
				if (n >= loadmodel->brushq3.num_nodes)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child node index %i (%i nodes)\n", n, loadmodel->brushq3.num_nodes);
				out->children[j] = loadmodel->brushq3.data_nodes + n;
			}
			else
			{
				n = -1 - n;
				if (n >= loadmodel->brushq3.num_leafs)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child leaf index %i (%i leafs)\n", n, loadmodel->brushq3.num_leafs);
				out->children[j] = (q3mnode_t *)(loadmodel->brushq3.data_leafs + n);
			}
		}
		for (j = 0;j < 3;j++)
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong(in->mins[j]) - 1;
			out->maxs[j] = LittleLong(in->maxs[j]) + 1;
		}
	}

	// set the parent pointers
	Mod_Q3BSP_LoadNodes_RecursiveSetParent(loadmodel->brushq3.data_nodes, NULL);
}

static void Mod_Q3BSP_LoadLightGrid(lump_t *l)
{
	q3dlightgrid_t *in;
	q3dlightgrid_t *out;
	int count;

	if (l->filelen == 0)
		return;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightGrid: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_lightgrid_scale[0] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[0];
	loadmodel->brushq3.num_lightgrid_scale[1] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[1];
	loadmodel->brushq3.num_lightgrid_scale[2] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[2];
	loadmodel->brushq3.num_lightgrid_imins[0] = ceil(loadmodel->brushq3.data_models->mins[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imins[1] = ceil(loadmodel->brushq3.data_models->mins[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imins[2] = ceil(loadmodel->brushq3.data_models->mins[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_imaxs[0] = floor(loadmodel->brushq3.data_models->maxs[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imaxs[1] = floor(loadmodel->brushq3.data_models->maxs[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imaxs[2] = floor(loadmodel->brushq3.data_models->maxs[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_isize[0] = loadmodel->brushq3.num_lightgrid_imaxs[0] - loadmodel->brushq3.num_lightgrid_imins[0] + 1;
	loadmodel->brushq3.num_lightgrid_isize[1] = loadmodel->brushq3.num_lightgrid_imaxs[1] - loadmodel->brushq3.num_lightgrid_imins[1] + 1;
	loadmodel->brushq3.num_lightgrid_isize[2] = loadmodel->brushq3.num_lightgrid_imaxs[2] - loadmodel->brushq3.num_lightgrid_imins[2] + 1;
	count = loadmodel->brushq3.num_lightgrid_isize[0] * loadmodel->brushq3.num_lightgrid_isize[1] * loadmodel->brushq3.num_lightgrid_isize[2];
	if (l->filelen < count * (int)sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightGrid: invalid lightgrid lump size %i bytes, should be %i bytes (%ix%ix%i)\n", l->filelen, count * sizeof(*in), loadmodel->brushq3.num_lightgrid_dimensions[0], loadmodel->brushq3.num_lightgrid_dimensions[1], loadmodel->brushq3.num_lightgrid_dimensions[2]);
	if (l->filelen != count * (int)sizeof(*in))
		Con_Printf("Mod_Q3BSP_LoadLightGrid: Warning: calculated lightgrid size %i bytes does not match lump size %i\n", count * sizeof(*in), l->filelen);

	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));
	loadmodel->brushq3.data_lightgrid = out;
	loadmodel->brushq3.num_lightgrid = count;

	// no swapping or validation necessary
	memcpy(out, in, count * (int)sizeof(*out));

	Matrix4x4_CreateScale3(&loadmodel->brushq3.num_lightgrid_indexfromworld, loadmodel->brushq3.num_lightgrid_scale[0], loadmodel->brushq3.num_lightgrid_scale[1], loadmodel->brushq3.num_lightgrid_scale[2]);
	Matrix4x4_ConcatTranslate(&loadmodel->brushq3.num_lightgrid_indexfromworld, -loadmodel->brushq3.num_lightgrid_imins[0] * loadmodel->brushq3.num_lightgrid_cellsize[0], -loadmodel->brushq3.num_lightgrid_imins[1] * loadmodel->brushq3.num_lightgrid_cellsize[1], -loadmodel->brushq3.num_lightgrid_imins[2] * loadmodel->brushq3.num_lightgrid_cellsize[2]);
}

static void Mod_Q3BSP_LoadPVS(lump_t *l)
{
	q3dpvs_t *in;
	int totalchains;

	if (l->filelen == 0)
		return;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen < 9)
		Host_Error("Mod_Q3BSP_LoadPVS: funny lump size in %s",loadmodel->name);

	loadmodel->brushq3.num_pvsclusters = LittleLong(in->numclusters);
	loadmodel->brushq3.num_pvschainlength = LittleLong(in->chainlength);
	if (loadmodel->brushq3.num_pvschainlength < ((loadmodel->brushq3.num_pvsclusters + 7) / 8))
		Host_Error("Mod_Q3BSP_LoadPVS: (chainlength = %i) < ((numclusters = %i) + 7) / 8\n", loadmodel->brushq3.num_pvschainlength, loadmodel->brushq3.num_pvsclusters);
	totalchains = loadmodel->brushq3.num_pvschainlength * loadmodel->brushq3.num_pvsclusters;
	if (l->filelen < totalchains + (int)sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPVS: lump too small ((numclusters = %i) * (chainlength = %i) + sizeof(q3dpvs_t) == %i bytes, lump is %i bytes)\n", loadmodel->brushq3.num_pvsclusters, loadmodel->brushq3.num_pvschainlength, totalchains + sizeof(*in), l->filelen);

	loadmodel->brushq3.data_pvschains = Mem_Alloc(loadmodel->mempool, totalchains);
	memcpy(loadmodel->brushq3.data_pvschains, (qbyte *)(in + 1), totalchains);
}

static void Mod_Q3BSP_FindNonSolidLocation(model_t *model, const vec3_t in, vec3_t out, vec_t radius)
{
	// FIXME: finish this code
	VectorCopy(in, out);
}

static void Mod_Q3BSP_LightPoint(model_t *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal)
{
	int i, j, k, index[3];
	float transformed[3], blend1, blend2, blend, yaw, pitch, sinpitch;
	q3dlightgrid_t *a, *s;
	// FIXME: write this
	if (!model->brushq3.num_lightgrid)
	{
		ambientcolor[0] = 1;
		ambientcolor[1] = 1;
		ambientcolor[2] = 1;
		return;
	}
	Matrix4x4_Transform(&model->brushq3.num_lightgrid_indexfromworld, p, transformed);
	//Matrix4x4_Print(&model->brushq3.num_lightgrid_indexfromworld);
	//Con_Printf("%f %f %f transformed %f %f %f clamped ", p[0], p[1], p[2], transformed[0], transformed[1], transformed[2]);
	transformed[0] = bound(0, transformed[0], model->brushq3.num_lightgrid_isize[0] - 1);
	transformed[1] = bound(0, transformed[1], model->brushq3.num_lightgrid_isize[1] - 1);
	transformed[2] = bound(0, transformed[2], model->brushq3.num_lightgrid_isize[2] - 1);
	index[0] = (int)floor(transformed[0]);
	index[1] = (int)floor(transformed[1]);
	index[2] = (int)floor(transformed[2]);
	//Con_Printf("%f %f %f index %i %i %i:\n", transformed[0], transformed[1], transformed[2], index[0], index[1], index[2]);
	// now lerp the values
	VectorClear(diffusenormal);
	a = &model->brushq3.data_lightgrid[(index[2] * model->brushq3.num_lightgrid_isize[1] + index[1]) * model->brushq3.num_lightgrid_isize[0] + index[0]];
	for (k = 0;k < 2;k++)
	{
		blend1 = (k ? (transformed[2] - index[2]) : (1 - (transformed[2] - index[2])));
		if (blend1 < 0.001f || index[2] + k >= model->brushq3.num_lightgrid_isize[2])
			continue;
		for (j = 0;j < 2;j++)
		{
			blend2 = blend1 * (j ? (transformed[1] - index[1]) : (1 - (transformed[1] - index[1])));
			if (blend2 < 0.001f || index[1] + j >= model->brushq3.num_lightgrid_isize[1])
				continue;
			for (i = 0;i < 2;i++)
			{
				blend = blend2 * (i ? (transformed[0] - index[0]) : (1 - (transformed[0] - index[0])));
				if (blend < 0.001f || index[0] + i >= model->brushq3.num_lightgrid_isize[0])
					continue;
				s = a + (k * model->brushq3.num_lightgrid_isize[1] + j) * model->brushq3.num_lightgrid_isize[0] + i;
				VectorMA(ambientcolor, blend * (1.0f / 128.0f), s->ambientrgb, ambientcolor);
				VectorMA(diffusecolor, blend * (1.0f / 128.0f), s->diffusergb, diffusecolor);
				pitch = s->diffusepitch * M_PI / 128;
				yaw = s->diffuseyaw * M_PI / 128;
				sinpitch = sin(pitch);
				diffusenormal[0] += blend * (cos(yaw) * sinpitch);
				diffusenormal[1] += blend * (sin(yaw) * sinpitch);
				diffusenormal[2] += blend * (cos(pitch));
				//Con_Printf("blend %f: ambient %i %i %i, diffuse %i %i %i, diffusepitch %i diffuseyaw %i (%f %f, normal %f %f %f)\n", blend, s->ambientrgb[0], s->ambientrgb[1], s->ambientrgb[2], s->diffusergb[0], s->diffusergb[1], s->diffusergb[2], s->diffusepitch, s->diffuseyaw, pitch, yaw, (cos(yaw) * cospitch), (sin(yaw) * cospitch), (-sin(pitch)));
			}
		}
	}
	VectorNormalize(diffusenormal);
	//Con_Printf("result: ambient %f %f %f diffuse %f %f %f diffusenormal %f %f %f\n", ambientcolor[0], ambientcolor[1], ambientcolor[2], diffusecolor[0], diffusecolor[1], diffusecolor[2], diffusenormal[0], diffusenormal[1], diffusenormal[2]);
}

static void Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace_t *trace, q3mnode_t *node, const vec3_t point, int markframe)
{
	int i;
	q3mleaf_t *leaf;
	colbrushf_t *brush;
	// find which leaf the point is in
	while (node->plane)
		node = node->children[DotProduct(point, node->plane->normal) < node->plane->dist];
	// point trace the brushes
	leaf = (q3mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = leaf->firstleafbrush[i]->colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(point, point, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TracePointBrushFloat(trace, point, leaf->firstleafbrush[i]->colbrushf);
		}
	}
	// can't do point traces on curves (they have no thickness)
}

static void Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace_t *trace, q3mnode_t *node, const vec3_t start, const vec3_t end, vec_t startfrac, vec_t endfrac, const vec3_t linestart, const vec3_t lineend, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i, startside, endside;
	float dist1, dist2, midfrac, mid[3], nodesegmentmins[3], nodesegmentmaxs[3];
	q3mleaf_t *leaf;
	q3mface_t *face;
	colbrushf_t *brush;
	if (startfrac > trace->realfraction)
		return;
	// note: all line fragments past first impact fraction are ignored
	if (VectorCompare(start, end))
	{
		// find which leaf the point is in
		while (node->plane)
			node = node->children[DotProduct(start, node->plane->normal) < node->plane->dist];
	}
	else
	{
		// find which nodes the line is in and recurse for them
		while (node->plane)
		{
			// recurse down node sides
			dist1 = PlaneDiff(start, node->plane);
			dist2 = PlaneDiff(end, node->plane);
			startside = dist1 < 0;
			endside = dist2 < 0;
			if (startside == endside)
			{
				// most of the time the line fragment is on one side of the plane
				node = node->children[startside];
			}
			else
			{
				// line crosses node plane, split the line
				midfrac = dist1 / (dist1 - dist2);
				VectorLerp(start, midfrac, end, mid);
				// take the near side first
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, node->children[startside], start, mid, startfrac, midfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
				if (midfrac <= trace->realfraction)
					Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, node->children[endside], mid, end, midfrac, endfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
				return;
			}
		}
	}
	// hit a leaf
	nodesegmentmins[0] = min(start[0], end[0]);
	nodesegmentmins[1] = min(start[1], end[1]);
	nodesegmentmins[2] = min(start[2], end[2]);
	nodesegmentmaxs[0] = max(start[0], end[0]);
	nodesegmentmaxs[1] = max(start[1], end[1]);
	nodesegmentmaxs[2] = max(start[2], end[2]);
	// line trace the brushes
	leaf = (q3mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = leaf->firstleafbrush[i]->colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TraceLineBrushFloat(trace, linestart, lineend, leaf->firstleafbrush[i]->colbrushf, leaf->firstleafbrush[i]->colbrushf);
			if (startfrac > trace->realfraction)
				return;
		}
	}
	// can't do point traces on curves (they have no thickness)
	if (mod_q3bsp_curves_collisions.integer && !VectorCompare(start, end))
	{
		// line trace the curves
		for (i = 0;i < leaf->numleaffaces;i++)
		{
			face = leaf->firstleafface[i];
			if (face->collisions && face->collisionmarkframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, face->mins, face->maxs))
			{
				face->collisionmarkframe = markframe;
				Collision_TraceLineTriangleMeshFloat(trace, linestart, lineend, face->num_triangles, face->data_element3i, face->data_vertex3f, face->texture->supercontents, segmentmins, segmentmaxs);
				if (startfrac > trace->realfraction)
					return;
			}
		}
	}
}

static void Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace_t *trace, q3mnode_t *node, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i, sides;
	float nodesegmentmins[3], nodesegmentmaxs[3];
	q3mleaf_t *leaf;
	colbrushf_t *brush;
	q3mface_t *face;
	/*
		// find which nodes the line is in and recurse for them
		while (node->plane)
		{
			// recurse down node sides
			int startside, endside;
			float dist1near, dist1far, dist2near, dist2far;
			BoxPlaneCornerDistances(thisbrush_start->mins, thisbrush_start->maxs, node->plane, &dist1near, &dist1far);
			BoxPlaneCornerDistances(thisbrush_end->mins, thisbrush_end->maxs, node->plane, &dist2near, &dist2far);
			startside = dist1near < 0;
			startside = dist1near < 0 ? (dist1far < 0 ? 1 : 2) : (dist1far < 0 ? 2 : 0);
			endside = dist2near < 0 ? (dist2far < 0 ? 1 : 2) : (dist2far < 0 ? 2 : 0);
			if (startside == 2 || endside == 2)
			{
				// brushes cross plane
				// do not clip anything, just take both sides
				Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
				node = node->children[1];
				continue;
			}
			if (startside == 0)
			{
				if (endside == 0)
				{
					node = node->children[0];
					continue;
				}
				else
				{
					//midf0 = dist1near / (dist1near - dist2near);
					//midf1 = dist1far / (dist1far - dist2far);
					Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
					node = node->children[1];
					continue;
				}
			}
			else
			{
				if (endside == 0)
				{
					//midf0 = dist1near / (dist1near - dist2near);
					//midf1 = dist1far / (dist1far - dist2far);
					Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
					node = node->children[1];
					continue;
				}
				else
				{
					node = node->children[1];
					continue;
				}
			}

			if (dist1near <  0 && dist2near <  0 && dist1far <  0 && dist2far <  0){node = node->children[1];continue;}
			if (dist1near <  0 && dist2near <  0 && dist1far <  0 && dist2far >= 0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near <  0 && dist2near <  0 && dist1far >= 0 && dist2far <  0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near <  0 && dist2near <  0 && dist1far >= 0 && dist2far >= 0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near <  0 && dist2near >= 0 && dist1far <  0 && dist2far <  0){node = node->children[1];continue;}
			if (dist1near <  0 && dist2near >= 0 && dist1far <  0 && dist2far >= 0){}
			if (dist1near <  0 && dist2near >= 0 && dist1far >= 0 && dist2far <  0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near <  0 && dist2near >= 0 && dist1far >= 0 && dist2far >= 0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near >= 0 && dist2near <  0 && dist1far <  0 && dist2far <  0){node = node->children[1];continue;}
			if (dist1near >= 0 && dist2near <  0 && dist1far <  0 && dist2far >= 0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near >= 0 && dist2near <  0 && dist1far >= 0 && dist2far <  0){}
			if (dist1near >= 0 && dist2near <  0 && dist1far >= 0 && dist2far >= 0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near >= 0 && dist2near >= 0 && dist1far <  0 && dist2far <  0){Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);node = node->children[1];continue;}
			if (dist1near >= 0 && dist2near >= 0 && dist1far <  0 && dist2far >= 0){node = node->children[0];continue;}
			if (dist1near >= 0 && dist2near >= 0 && dist1far >= 0 && dist2far <  0){node = node->children[0];continue;}
			if (dist1near >= 0 && dist2near >= 0 && dist1far >= 0 && dist2far >= 0){node = node->children[0];continue;}
			{             
				if (dist2near < 0) // d1n<0 && d2n<0
				{
					if (dist2near < 0) // d1n<0 && d2n<0
					{
						if (dist2near < 0) // d1n<0 && d2n<0
						{
						}
						else // d1n<0 && d2n>0
						{
						}
					}
					else // d1n<0 && d2n>0
					{
						if (dist2near < 0) // d1n<0 && d2n<0
						{
						}
						else // d1n<0 && d2n>0
						{
						}
					}
				}
				else // d1n<0 && d2n>0
				{
				}
			}
			else // d1n>0
			{
				if (dist2near < 0) // d1n>0 && d2n<0
				{
				}
				else // d1n>0 && d2n>0
				{
				}
			}
			if (dist1near < 0 == dist1far < 0 == dist2near < 0 == dist2far < 0)
			{
				node = node->children[startside];
				continue;
			}
			if (dist1near < dist2near)
			{
				// out
				if (dist1near >= 0)
				{
					node = node->children[0];
					continue;
				}
				if (dist2far < 0)
				{
					node = node->children[1];
					continue;
				}
				// dist1near < 0 && dist2far >= 0
			}
			else
			{
				// in
			}
			startside = dist1near < 0 ? (dist1far < 0 ? 1 : 2) : (dist1far < 0 ? 2 : 0);
			endside = dist2near < 0 ? (dist2far < 0 ? 1 : 2) : (dist2far < 0 ? 2 : 0);
			if (startside == 2 || endside == 2)
			{
				// brushes cross plane
				// do not clip anything, just take both sides
				Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
				node = node->children[1];
			}
			else if (startside == endside)
				node = node->children[startside];
			else if (startside == 0) // endside = 1 (start infront, end behind)
			{
			}
			else // startside == 1 endside = 0 (start behind, end infront)
			{
			}
			== endside)
			{
				if (startside < 2)
					node = node->children[startside];
				else
				{
					// start and end brush cross plane
				}
			}
			else
			{
			}
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
				node = node->children[1];
			else if (dist1near < 0 && dist1far < 0 && dist2near >= 0 && dist2far >= 0)
			else if (dist1near >= 0 && dist1far >= 0 && dist2near < 0 && dist2far < 0)
			else if (dist1near >= 0 && dist1far >= 0 && dist2near >= 0 && dist2far >= 0)
				node = node->children[0];
			else
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
			{
			}
			else if (dist1near >= 0 && dist1far >= 0)
			{
			}
			else // mixed (lying on plane)
			{
			}
			{
				if (dist2near < 0 && dist2far < 0)
				{
				}
				else
					node = node->children[1];
			}
			if (dist1near < 0 && dist1far < 0 && dist2near < 0 && dist2far < 0)
				node = node->children[0];
			else if (dist1near >= 0 && dist1far >= 0 && dist2near >= 0 && dist2far >= 0)
				node = node->children[1];
			else
			{
				// both sides
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, node->children[startside], start, mid, startfrac, midfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
				node = node->children[1];
			}
			sides = dist1near || dist1near < 0 | dist1far < 0 | dist2near < 0 | dist
			startside = dist1 < 0;
			endside = dist2 < 0;
			if (startside == endside)
			{
				// most of the time the line fragment is on one side of the plane
				node = node->children[startside];
			}
			else
			{
				// line crosses node plane, split the line
				midfrac = dist1 / (dist1 - dist2);
				VectorLerp(start, midfrac, end, mid);
				// take the near side first
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, node->children[startside], start, mid, startfrac, midfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
				if (midfrac <= trace->fraction)
					Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, node->children[endside], mid, end, midfrac, endfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
				return;
			}
		}
	*/
	// FIXME: could be made faster by copying TraceLine code and making it use
	// box plane distances...  (variant on the BoxOnPlaneSide code)
	for (;;)
	{
		nodesegmentmins[0] = max(segmentmins[0], node->mins[0]);
		nodesegmentmins[1] = max(segmentmins[1], node->mins[1]);
		nodesegmentmins[2] = max(segmentmins[2], node->mins[2]);
		nodesegmentmaxs[0] = min(segmentmaxs[0], node->maxs[0]);
		nodesegmentmaxs[1] = min(segmentmaxs[1], node->maxs[1]);
		nodesegmentmaxs[2] = min(segmentmaxs[2], node->maxs[2]);
		if (nodesegmentmins[0] > nodesegmentmaxs[0] || nodesegmentmins[1] > nodesegmentmaxs[1] || nodesegmentmins[2] > nodesegmentmaxs[2])
			return;
		if (!node->plane)
			break;
		if (mod_q3bsp_debugtracebrush.integer == 2)
		{
			Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
			node = node->children[1];
		}
		else if (mod_q3bsp_debugtracebrush.integer == 1)
		{
			// recurse down node sides
			sides = BoxOnPlaneSide(segmentmins, segmentmaxs, node->plane);
			if (sides == 3)
			{
				// segment box crosses plane
				Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
				node = node->children[1];
			}
			else
			{
				// take whichever side the segment box is on
				node = node->children[sides - 1];
			}
		}
		else
		{
			// recurse down node sides
			sides = BoxOnPlaneSide(segmentmins, segmentmaxs, node->plane);
			if (sides == 3)
			{
				// segment box crosses plane
				// now check start and end brush boxes to handle a lot of 'diagonal' cases more efficiently...
				sides = BoxOnPlaneSide(thisbrush_start->mins, thisbrush_start->maxs, node->plane) | BoxOnPlaneSide(thisbrush_end->mins, thisbrush_end->maxs, node->plane);
				if (sides == 3)
				{
					Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
					sides = 2;
				}
			}
			// take whichever side the segment box is on
			node = node->children[sides - 1];
		}
	}
	// hit a leaf
	leaf = (q3mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = leaf->firstleafbrush[i]->colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, leaf->firstleafbrush[i]->colbrushf, leaf->firstleafbrush[i]->colbrushf);
		}
	}
	if (mod_q3bsp_curves_collisions.integer)
	{
		for (i = 0;i < leaf->numleaffaces;i++)
		{
			face = leaf->firstleafface[i];
			if (face->collisions && face->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, face->mins, face->maxs))
			{
				face->markframe = markframe;
				Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, face->num_triangles, face->data_element3i, face->data_vertex3f, face->texture->supercontents, segmentmins, segmentmaxs);
			}
		}
	}
}

static void Mod_Q3BSP_TraceBox(model_t *model, int frame, trace_t *trace, const vec3_t boxstartmins, const vec3_t boxstartmaxs, const vec3_t boxendmins, const vec3_t boxendmaxs, int hitsupercontentsmask)
{
	int i;
	float segmentmins[3], segmentmaxs[3];
	colbrushf_t *thisbrush_start, *thisbrush_end;
	matrix4x4_t startmatrix, endmatrix;
	static int markframe = 0;
	q3mface_t *face;
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	Matrix4x4_CreateIdentity(&startmatrix);
	Matrix4x4_CreateIdentity(&endmatrix);
	segmentmins[0] = min(boxstartmins[0], boxendmins[0]);
	segmentmins[1] = min(boxstartmins[1], boxendmins[1]);
	segmentmins[2] = min(boxstartmins[2], boxendmins[2]);
	segmentmaxs[0] = max(boxstartmaxs[0], boxendmaxs[0]);
	segmentmaxs[1] = max(boxstartmaxs[1], boxendmaxs[1]);
	segmentmaxs[2] = max(boxstartmaxs[2], boxendmaxs[2]);
	if (mod_q3bsp_optimizedtraceline.integer && VectorCompare(boxstartmins, boxstartmaxs) && VectorCompare(boxendmins, boxendmaxs))
	{
		if (VectorCompare(boxstartmins, boxendmins))
		{
			// point trace
			if (model->brushq3.submodel)
			{
				for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
					if (model->brushq3.data_thismodel->firstbrush[i].colbrushf)
						Collision_TracePointBrushFloat(trace, boxstartmins, model->brushq3.data_thismodel->firstbrush[i].colbrushf);
			}
			else
				Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace, model->brushq3.data_nodes, boxstartmins, ++markframe);
		}
		else
		{
			// line trace
			if (model->brushq3.submodel)
			{
				for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
					if (model->brushq3.data_thismodel->firstbrush[i].colbrushf)
						Collision_TraceLineBrushFloat(trace, boxstartmins, boxendmins, model->brushq3.data_thismodel->firstbrush[i].colbrushf, model->brushq3.data_thismodel->firstbrush[i].colbrushf);
				if (mod_q3bsp_curves_collisions.integer)
				{
					for (i = 0;i < model->brushq3.data_thismodel->numfaces;i++)
					{
						face = model->brushq3.data_thismodel->firstface + i;
						if (face->collisions)
							Collision_TraceLineTriangleMeshFloat(trace, boxstartmins, boxendmins, face->num_triangles, face->data_element3i, face->data_vertex3f, face->texture->supercontents, segmentmins, segmentmaxs);
					}
				}
			}
			else
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model->brushq3.data_nodes, boxstartmins, boxendmins, 0, 1, boxstartmins, boxendmins, ++markframe, segmentmins, segmentmaxs);
		}
	}
	else
	{
		// box trace, performed as brush trace
		thisbrush_start = Collision_BrushForBox(&startmatrix, boxstartmins, boxstartmaxs);
		thisbrush_end = Collision_BrushForBox(&endmatrix, boxendmins, boxendmaxs);
		if (model->brushq3.submodel)
		{
			for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
				if (model->brushq3.data_thismodel->firstbrush[i].colbrushf)
					Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, model->brushq3.data_thismodel->firstbrush[i].colbrushf, model->brushq3.data_thismodel->firstbrush[i].colbrushf);
			if (mod_q3bsp_curves_collisions.integer)
			{
				for (i = 0;i < model->brushq3.data_thismodel->numfaces;i++)
				{
					face = model->brushq3.data_thismodel->firstface + i;
					if (face->collisions)
						Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, face->num_triangles, face->data_element3i, face->data_vertex3f, face->texture->supercontents, segmentmins, segmentmaxs);
				}
			}
		}
		else
			Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, model->brushq3.data_nodes, thisbrush_start, thisbrush_end, ++markframe, segmentmins, segmentmaxs);
	}
}

static int Mod_Q3BSP_BoxTouchingPVS(model_t *model, const qbyte *pvs, const vec3_t mins, const vec3_t maxs)
{
	int clusterindex, side, nodestackindex = 0;
	q3mnode_t *node, *nodestack[1024];
	node = model->brushq3.data_nodes;
	for(;;)
	{
		if (node->plane)
		{
			// node - recurse down the BSP tree
			side = BoxOnPlaneSide(mins, maxs, node->plane) - 1;
			if (side < 2)
			{
				// box is on one side of plane, take that path
				node = node->children[side];
			}
			else
			{
				// box crosses plane, take one path and remember the other
				nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
			}
		}
		else
		{
			// leaf - check cluster bit
			clusterindex = ((q3mleaf_t *)node)->clusterindex;
			if (pvs[clusterindex >> 3] & (1 << (clusterindex & 7)))
			{
				// it is visible, return immediately with the news
				return true;
			}
			else
			{
				// nothing to see here, try another path we didn't take earlier
				if (nodestackindex == 0)
					break;
				node = nodestack[--nodestackindex];
			}
		}
	}
	// it is not visible
	return false;
}

//Returns PVS data for a given point
//(note: can return NULL)
static qbyte *Mod_Q3BSP_GetPVS(model_t *model, const vec3_t p)
{
	q3mnode_t *node;
	Mod_CheckLoaded(model);
	node = model->brushq3.data_nodes;
	while (node->plane)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];
	if (((q3mleaf_t *)node)->clusterindex >= 0)
		return model->brushq3.data_pvschains + ((q3mleaf_t *)node)->clusterindex * model->brushq3.num_pvschainlength;
	else
		return NULL;
}

static void Mod_Q3BSP_FatPVS_RecursiveBSPNode(model_t *model, const vec3_t org, vec_t radius, qbyte *pvsbuffer, int pvsbytes, q3mnode_t *node)
{
	int i;
	float d;
	qbyte *pvs;

	while (node->plane)
	{
		d = PlaneDiff(org, node->plane);
		if (d > radius)
			node = node->children[0];
		else if (d < -radius)
			node = node->children[1];
		else
		{
			// go down both sides
			Mod_Q3BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, pvsbytes, node->children[0]);
			node = node->children[1];
		}
	}
	// if this is a leaf with a pvs, accumulate the pvs bits
	if (((q3mleaf_t *)node)->clusterindex >= 0)
	{
		pvs = model->brushq3.data_pvschains + ((q3mleaf_t *)node)->clusterindex * model->brushq3.num_pvschainlength;
		for (i = 0;i < pvsbytes;i++)
			pvsbuffer[i] |= pvs[i];
	}
	else
		memset(pvsbuffer, 0xFF, pvsbytes);
	return;
}

//Calculates a PVS that is the inclusive or of all leafs within radius pixels
//of the given point.
static int Mod_Q3BSP_FatPVS(model_t *model, const vec3_t org, vec_t radius, qbyte *pvsbuffer, int pvsbufferlength)
{
	int bytes = model->brushq3.num_pvschainlength;
	bytes = min(bytes, pvsbufferlength);
	if (r_novis.integer)
	{
		memset(pvsbuffer, 0xFF, bytes);
		return bytes;
	}
	memset(pvsbuffer, 0, bytes);
	Mod_Q3BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, bytes, model->brushq3.data_nodes);
	return bytes;
}


static int Mod_Q3BSP_SuperContentsFromNativeContents(model_t *model, int nativecontents)
{
	int supercontents = 0;
	if (nativecontents & Q2CONTENTS_SOLID)
		supercontents |= SUPERCONTENTS_SOLID;
	if (nativecontents & Q2CONTENTS_WATER)
		supercontents |= SUPERCONTENTS_WATER;
	if (nativecontents & Q2CONTENTS_SLIME)
		supercontents |= SUPERCONTENTS_SLIME;
	if (nativecontents & Q2CONTENTS_LAVA)
		supercontents |= SUPERCONTENTS_LAVA;
	return supercontents;
}

static int Mod_Q3BSP_NativeContentsFromSuperContents(model_t *model, int supercontents)
{
	int nativecontents = 0;
	if (supercontents & SUPERCONTENTS_SOLID)
		nativecontents |= Q2CONTENTS_SOLID;
	if (supercontents & SUPERCONTENTS_WATER)
		nativecontents |= Q2CONTENTS_WATER;
	if (supercontents & SUPERCONTENTS_SLIME)
		nativecontents |= Q2CONTENTS_SLIME;
	if (supercontents & SUPERCONTENTS_LAVA)
		nativecontents |= Q2CONTENTS_LAVA;
	return nativecontents;
}

extern void R_Q3BSP_DrawSky(struct entity_render_s *ent);
extern void R_Q3BSP_Draw(struct entity_render_s *ent);
extern void R_Q3BSP_DrawShadowVolume(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Q3BSP_DrawLight(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_Q3BSP_Load(model_t *mod, void *buffer)
{
	int i, j;
	q3dheader_t *header;
	float corner[3], yawradius, modelradius;

	mod->type = mod_brushq3;
	mod->numframes = 1;
	mod->numskins = 1;

	header = (q3dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q3BSPVERSION)
		Host_Error("Mod_Q3BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q3BSPVERSION);
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", false);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q3BSP_TraceBox;
	mod->brush.SuperContentsFromNativeContents = Mod_Q3BSP_SuperContentsFromNativeContents;
	mod->brush.NativeContentsFromSuperContents = Mod_Q3BSP_NativeContentsFromSuperContents;
	mod->brush.GetPVS = Mod_Q3BSP_GetPVS;
	mod->brush.FatPVS = Mod_Q3BSP_FatPVS;
	mod->brush.BoxTouchingPVS = Mod_Q3BSP_BoxTouchingPVS;
	mod->brush.LightPoint = Mod_Q3BSP_LightPoint;
	mod->brush.FindNonSolidLocation = Mod_Q3BSP_FindNonSolidLocation;
	//mod->DrawSky = R_Q3BSP_DrawSky;
	mod->Draw = R_Q3BSP_Draw;
	mod->DrawShadowVolume = R_Q3BSP_DrawShadowVolume;
	mod->DrawLight = R_Q3BSP_DrawLight;

	mod_base = (qbyte *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	Mod_Q3BSP_LoadEntities(&header->lumps[Q3LUMP_ENTITIES]);
	Mod_Q3BSP_LoadTextures(&header->lumps[Q3LUMP_TEXTURES]);
	Mod_Q3BSP_LoadPlanes(&header->lumps[Q3LUMP_PLANES]);
	Mod_Q3BSP_LoadBrushSides(&header->lumps[Q3LUMP_BRUSHSIDES]);
	Mod_Q3BSP_LoadBrushes(&header->lumps[Q3LUMP_BRUSHES]);
	Mod_Q3BSP_LoadEffects(&header->lumps[Q3LUMP_EFFECTS]);
	Mod_Q3BSP_LoadVertices(&header->lumps[Q3LUMP_VERTICES]);
	Mod_Q3BSP_LoadTriangles(&header->lumps[Q3LUMP_TRIANGLES]);
	Mod_Q3BSP_LoadLightmaps(&header->lumps[Q3LUMP_LIGHTMAPS]);
	Mod_Q3BSP_LoadFaces(&header->lumps[Q3LUMP_FACES]);
	Mod_Q3BSP_LoadModels(&header->lumps[Q3LUMP_MODELS]);
	Mod_Q3BSP_LoadLeafBrushes(&header->lumps[Q3LUMP_LEAFBRUSHES]);
	Mod_Q3BSP_LoadLeafFaces(&header->lumps[Q3LUMP_LEAFFACES]);
	Mod_Q3BSP_LoadLeafs(&header->lumps[Q3LUMP_LEAFS]);
	Mod_Q3BSP_LoadNodes(&header->lumps[Q3LUMP_NODES]);
	Mod_Q3BSP_LoadLightGrid(&header->lumps[Q3LUMP_LIGHTGRID]);
	Mod_Q3BSP_LoadPVS(&header->lumps[Q3LUMP_PVS]);
	loadmodel->brush.numsubmodels = loadmodel->brushq3.num_models;

	for (i = 0;i < loadmodel->brushq3.num_models;i++)
	{
		if (i == 0)
			mod = loadmodel;
		else
		{
			char name[10];
			// LordHavoc: only register submodels if it is the world
			// (prevents bsp models from replacing world submodels)
			if (!loadmodel->isworldmodel)
				continue;
			// duplicate the basic information
			sprintf(name, "*%i", i);
			mod = Mod_FindName(name);
			*mod = *loadmodel;
			strcpy(mod->name, name);
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
		}
		mod->brushq3.data_thismodel = loadmodel->brushq3.data_models + i;
		mod->brushq3.submodel = i;

		VectorCopy(mod->brushq3.data_thismodel->mins, mod->normalmins);
		VectorCopy(mod->brushq3.data_thismodel->maxs, mod->normalmaxs);
		corner[0] = max(fabs(mod->normalmins[0]), fabs(mod->normalmaxs[0]));
		corner[1] = max(fabs(mod->normalmins[1]), fabs(mod->normalmaxs[1]));
		corner[2] = max(fabs(mod->normalmins[2]), fabs(mod->normalmaxs[2]));
		modelradius = sqrt(corner[0]*corner[0]+corner[1]*corner[1]+corner[2]*corner[2]);
		yawradius = sqrt(corner[0]*corner[0]+corner[1]*corner[1]);
		mod->rotatedmins[0] = mod->rotatedmins[1] = mod->rotatedmins[2] = -modelradius;
		mod->rotatedmaxs[0] = mod->rotatedmaxs[1] = mod->rotatedmaxs[2] = modelradius;
		mod->yawmaxs[0] = mod->yawmaxs[1] = yawradius;
		mod->yawmins[0] = mod->yawmins[1] = -yawradius;
		mod->yawmins[2] = mod->normalmins[2];
		mod->yawmaxs[2] = mod->normalmaxs[2];
		mod->radius = modelradius;
		mod->radius2 = modelradius * modelradius;

		for (j = 0;j < mod->brushq3.data_thismodel->numfaces;j++)
			if (mod->brushq3.data_thismodel->firstface[j].texture->surfaceflags & Q3SURFACEFLAG_SKY)
				break;
		if (j < mod->brushq3.data_thismodel->numfaces)
			mod->DrawSky = R_Q3BSP_DrawSky;
	}
}

void Mod_IBSP_Load(model_t *mod, void *buffer)
{
	int i = LittleLong(((int *)buffer)[1]);
	if (i == Q3BSPVERSION)
		Mod_Q3BSP_Load(mod,buffer);
	else if (i == Q2BSPVERSION)
		Mod_Q2BSP_Load(mod,buffer);
	else
		Host_Error("Mod_IBSP_Load: unknown/unsupported version %i\n", i);
}

void Mod_MAP_Load(model_t *mod, void *buffer)
{
	Host_Error("Mod_MAP_Load: not yet implemented\n");
}

