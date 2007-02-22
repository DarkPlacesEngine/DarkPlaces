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
#include "polygon.h"
#include "curves.h"
#include "wad.h"


//cvar_t r_subdivide_size = {CVAR_SAVE, "r_subdivide_size", "128", "how large water polygons should be (smaller values produce more polygons which give better warping effects)"};
cvar_t halflifebsp = {0, "halflifebsp", "0", "indicates the current map is hlbsp format (useful to know because of different bounding box sizes)"};
cvar_t mcbsp = {0, "mcbsp", "0", "indicates the current map is mcbsp format (useful to know because of different bounding box sizes)"};
cvar_t r_novis = {0, "r_novis", "0", "draws whole level, see also sv_cullentities_pvs 0"};
cvar_t r_lightmaprgba = {0, "r_lightmaprgba", "1", "whether to use RGBA (32bit) or RGB (24bit) lightmaps"};
cvar_t r_nosurftextures = {0, "r_nosurftextures", "0", "pretends there was no texture lump found in the q1bsp/hlbsp loading (useful for debugging this rare case)"};
cvar_t r_subdivisions_tolerance = {0, "r_subdivisions_tolerance", "4", "maximum error tolerance on curve subdivision for rendering purposes (in other words, the curves will be given as many polygons as necessary to represent curves at this quality)"};
cvar_t r_subdivisions_mintess = {0, "r_subdivisions_mintess", "1", "minimum number of subdivisions (values above 1 will smooth curves that don't need it)"};
cvar_t r_subdivisions_maxtess = {0, "r_subdivisions_maxtess", "1024", "maximum number of subdivisions (prevents curves beyond a certain detail level, limits smoothing)"};
cvar_t r_subdivisions_maxvertices = {0, "r_subdivisions_maxvertices", "65536", "maximum vertices allowed per subdivided curve"};
cvar_t r_subdivisions_collision_tolerance = {0, "r_subdivisions_collision_tolerance", "15", "maximum error tolerance on curve subdivision for collision purposes (usually a larger error tolerance than for rendering)"};
cvar_t r_subdivisions_collision_mintess = {0, "r_subdivisions_collision_mintess", "1", "minimum number of subdivisions (values above 1 will smooth curves that don't need it)"};
cvar_t r_subdivisions_collision_maxtess = {0, "r_subdivisions_collision_maxtess", "1024", "maximum number of subdivisions (prevents curves beyond a certain detail level, limits smoothing)"};
cvar_t r_subdivisions_collision_maxvertices = {0, "r_subdivisions_collision_maxvertices", "4225", "maximum vertices allowed per subdivided curve"};
cvar_t mod_q3bsp_curves_collisions = {0, "mod_q3bsp_curves_collisions", "1", "enables collisions with curves (SLOW)"};
cvar_t mod_q3bsp_optimizedtraceline = {0, "mod_q3bsp_optimizedtraceline", "1", "whether to use optimized traceline code for line traces (as opposed to tracebox code)"};
cvar_t mod_q3bsp_debugtracebrush = {0, "mod_q3bsp_debugtracebrush", "0", "selects different tracebrush bsp recursion algorithms (for debugging purposes only)"};
cvar_t mod_q3bsp_lightmapmergepower = {CVAR_SAVE, "mod_q3bsp_lightmapmergepower", "5", "merges the quake3 128x128 lightmap textures into larger lightmap group textures to speed up rendering, 1 = 256x256, 2 = 512x512, 3 = 1024x1024, 4 = 2048x2048, 5 = 4096x4096, ..."};

static texture_t mod_q1bsp_texture_solid;
static texture_t mod_q1bsp_texture_sky;
static texture_t mod_q1bsp_texture_lava;
static texture_t mod_q1bsp_texture_slime;
static texture_t mod_q1bsp_texture_water;

void Mod_BrushInit(void)
{
//	Cvar_RegisterVariable(&r_subdivide_size);
	Cvar_RegisterVariable(&halflifebsp);
	Cvar_RegisterVariable(&mcbsp);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_lightmaprgba);
	Cvar_RegisterVariable(&r_nosurftextures);
	Cvar_RegisterVariable(&r_subdivisions_tolerance);
	Cvar_RegisterVariable(&r_subdivisions_mintess);
	Cvar_RegisterVariable(&r_subdivisions_maxtess);
	Cvar_RegisterVariable(&r_subdivisions_maxvertices);
	Cvar_RegisterVariable(&r_subdivisions_collision_tolerance);
	Cvar_RegisterVariable(&r_subdivisions_collision_mintess);
	Cvar_RegisterVariable(&r_subdivisions_collision_maxtess);
	Cvar_RegisterVariable(&r_subdivisions_collision_maxvertices);
	Cvar_RegisterVariable(&mod_q3bsp_curves_collisions);
	Cvar_RegisterVariable(&mod_q3bsp_optimizedtraceline);
	Cvar_RegisterVariable(&mod_q3bsp_debugtracebrush);
	Cvar_RegisterVariable(&mod_q3bsp_lightmapmergepower);

	memset(&mod_q1bsp_texture_solid, 0, sizeof(mod_q1bsp_texture_solid));
	strlcpy(mod_q1bsp_texture_solid.name, "solid" , sizeof(mod_q1bsp_texture_solid.name));
	mod_q1bsp_texture_solid.surfaceflags = 0;
	mod_q1bsp_texture_solid.supercontents = SUPERCONTENTS_SOLID;

	mod_q1bsp_texture_sky = mod_q1bsp_texture_solid;
	strlcpy(mod_q1bsp_texture_sky.name, "sky", sizeof(mod_q1bsp_texture_sky.name));
	mod_q1bsp_texture_sky.surfaceflags = Q3SURFACEFLAG_SKY | Q3SURFACEFLAG_NOIMPACT | Q3SURFACEFLAG_NOMARKS | Q3SURFACEFLAG_NODLIGHT | Q3SURFACEFLAG_NOLIGHTMAP;
	mod_q1bsp_texture_sky.supercontents = SUPERCONTENTS_SKY | SUPERCONTENTS_NODROP;

	mod_q1bsp_texture_lava = mod_q1bsp_texture_solid;
	strlcpy(mod_q1bsp_texture_lava.name, "*lava", sizeof(mod_q1bsp_texture_lava.name));
	mod_q1bsp_texture_lava.surfaceflags = Q3SURFACEFLAG_NOMARKS;
	mod_q1bsp_texture_lava.supercontents = SUPERCONTENTS_LAVA | SUPERCONTENTS_NODROP;

	mod_q1bsp_texture_slime = mod_q1bsp_texture_solid;
	strlcpy(mod_q1bsp_texture_slime.name, "*slime", sizeof(mod_q1bsp_texture_slime.name));
	mod_q1bsp_texture_slime.surfaceflags = Q3SURFACEFLAG_NOMARKS;
	mod_q1bsp_texture_slime.supercontents = SUPERCONTENTS_SLIME;

	mod_q1bsp_texture_water = mod_q1bsp_texture_solid;
	strlcpy(mod_q1bsp_texture_water.name, "*water", sizeof(mod_q1bsp_texture_water.name));
	mod_q1bsp_texture_water.surfaceflags = Q3SURFACEFLAG_NOMARKS;
	mod_q1bsp_texture_water.supercontents = SUPERCONTENTS_WATER;
}

static mleaf_t *Mod_Q1BSP_PointInLeaf(model_t *model, const vec3_t p)
{
	mnode_t *node;

	if (model == NULL)
		return NULL;

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->plane)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];

	return (mleaf_t *)node;
}

static void Mod_Q1BSP_AmbientSoundLevelsForPoint(model_t *model, const vec3_t p, unsigned char *out, int outsize)
{
	int i;
	mleaf_t *leaf;
	leaf = Mod_Q1BSP_PointInLeaf(model, p);
	if (leaf)
	{
		i = min(outsize, (int)sizeof(leaf->ambient_sound_level));
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

static int Mod_Q1BSP_FindBoxClusters(model_t *model, const vec3_t mins, const vec3_t maxs, int maxclusters, int *clusterlist)
{
	int numclusters = 0;
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_pvsclusters)
		return -1;
	node = model->brush.data_nodes;
	for (;;)
	{
#if 1
		if (node->plane)
		{
			// node - recurse down the BSP tree
			int sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides < 3)
			{
				if (sides == 0)
					return -1; // ERROR: NAN bounding box!
				// box is on one side of plane, take that path
				node = node->children[sides-1];
			}
			else
			{
				// box crosses plane, take one path and remember the other
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
			}
			continue;
		}
		else
		{
			// leaf - add clusterindex to list
			if (numclusters < maxclusters)
				clusterlist[numclusters] = ((mleaf_t *)node)->clusterindex;
			numclusters++;
		}
#else
		if (BoxesOverlap(mins, maxs, node->mins, node->maxs))
		{
			if (node->plane)
			{
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
				continue;
			}
			else
			{
				// leaf - add clusterindex to list
				if (numclusters < maxclusters)
					clusterlist[numclusters] = ((mleaf_t *)node)->clusterindex;
				numclusters++;
			}
		}
#endif
		// try another path we didn't take earlier
		if (nodestackindex == 0)
			break;
		node = nodestack[--nodestackindex];
	}
	// return number of clusters found (even if more than the maxclusters)
	return numclusters;
}

static int Mod_Q1BSP_BoxTouchingPVS(model_t *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_pvsclusters)
		return true;
	node = model->brush.data_nodes;
	for (;;)
	{
#if 1
		if (node->plane)
		{
			// node - recurse down the BSP tree
			int sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides < 3)
			{
				if (sides == 0)
					return -1; // ERROR: NAN bounding box!
				// box is on one side of plane, take that path
				node = node->children[sides-1];
			}
			else
			{
				// box crosses plane, take one path and remember the other
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
			}
			continue;
		}
		else
		{
			// leaf - check cluster bit
			int clusterindex = ((mleaf_t *)node)->clusterindex;
			if (CHECKPVSBIT(pvs, clusterindex))
			{
				// it is visible, return immediately with the news
				return true;
			}
		}
#else
		if (BoxesOverlap(mins, maxs, node->mins, node->maxs))
		{
			if (node->plane)
			{
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
				continue;
			}
			else
			{
				// leaf - check cluster bit
				int clusterindex = ((mleaf_t *)node)->clusterindex;
				if (CHECKPVSBIT(pvs, clusterindex))
				{
					// it is visible, return immediately with the news
					return true;
				}
			}
		}
#endif
		// nothing to see here, try another path we didn't take earlier
		if (nodestackindex == 0)
			break;
		node = nodestack[--nodestackindex];
	}
	// it is not visible
	return false;
}

static int Mod_Q1BSP_BoxTouchingLeafPVS(model_t *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_leafs)
		return true;
	node = model->brush.data_nodes;
	for (;;)
	{
#if 1
		if (node->plane)
		{
			// node - recurse down the BSP tree
			int sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides < 3)
			{
				if (sides == 0)
					return -1; // ERROR: NAN bounding box!
				// box is on one side of plane, take that path
				node = node->children[sides-1];
			}
			else
			{
				// box crosses plane, take one path and remember the other
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
			}
			continue;
		}
		else
		{
			// leaf - check cluster bit
			int clusterindex = ((mleaf_t *)node) - model->brush.data_leafs;
			if (CHECKPVSBIT(pvs, clusterindex))
			{
				// it is visible, return immediately with the news
				return true;
			}
		}
#else
		if (BoxesOverlap(mins, maxs, node->mins, node->maxs))
		{
			if (node->plane)
			{
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
				continue;
			}
			else
			{
				// leaf - check cluster bit
				int clusterindex = ((mleaf_t *)node) - model->brush.data_leafs;
				if (CHECKPVSBIT(pvs, clusterindex))
				{
					// it is visible, return immediately with the news
					return true;
				}
			}
		}
#endif
		// nothing to see here, try another path we didn't take earlier
		if (nodestackindex == 0)
			break;
		node = nodestack[--nodestackindex];
	}
	// it is not visible
	return false;
}

static int Mod_Q1BSP_BoxTouchingVisibleLeafs(model_t *model, const unsigned char *visibleleafs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_leafs)
		return true;
	node = model->brush.data_nodes;
	for (;;)
	{
#if 1
		if (node->plane)
		{
			// node - recurse down the BSP tree
			int sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides < 3)
			{
				if (sides == 0)
					return -1; // ERROR: NAN bounding box!
				// box is on one side of plane, take that path
				node = node->children[sides-1];
			}
			else
			{
				// box crosses plane, take one path and remember the other
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
			}
			continue;
		}
		else
		{
			// leaf - check if it is visible
			if (visibleleafs[(mleaf_t *)node - model->brush.data_leafs])
			{
				// it is visible, return immediately with the news
				return true;
			}
		}
#else
		if (BoxesOverlap(mins, maxs, node->mins, node->maxs))
		{
			if (node->plane)
			{
				if (nodestackindex < 1024)
					nodestack[nodestackindex++] = node->children[0];
				node = node->children[1];
				continue;
			}
			else
			{
				// leaf - check if it is visible
				if (visibleleafs[(mleaf_t *)node - model->brush.data_leafs])
				{
					// it is visible, return immediately with the news
					return true;
				}
			}
		}
#endif
		// nothing to see here, try another path we didn't take earlier
		if (nodestackindex == 0)
			break;
		node = nodestack[--nodestackindex];
	}
	// it is not visible
	return false;
}

typedef struct findnonsolidlocationinfo_s
{
	vec3_t center;
	vec_t radius;
	vec3_t nudge;
	vec_t bestdist;
	model_t *model;
}
findnonsolidlocationinfo_t;

static void Mod_Q1BSP_FindNonSolidLocation_r_Leaf(findnonsolidlocationinfo_t *info, mleaf_t *leaf)
{
	int i, surfacenum, k, *tri, *mark;
	float dist, f, vert[3][3], edge[3][3], facenormal[3], edgenormal[3][3], point[3];
	msurface_t *surface;
	for (surfacenum = 0, mark = leaf->firstleafsurface;surfacenum < leaf->numleafsurfaces;surfacenum++, mark++)
	{
		surface = info->model->data_surfaces + *mark;
		if (surface->texture->supercontents & SUPERCONTENTS_SOLID)
		{
			for (k = 0;k < surface->num_triangles;k++)
			{
				tri = (info->model->surfmesh.data_element3i + 3 * surface->num_firsttriangle) + k * 3;
				VectorCopy((info->model->surfmesh.data_vertex3f + tri[0] * 3), vert[0]);
				VectorCopy((info->model->surfmesh.data_vertex3f + tri[1] * 3), vert[1]);
				VectorCopy((info->model->surfmesh.data_vertex3f + tri[2] * 3), vert[2]);
				VectorSubtract(vert[1], vert[0], edge[0]);
				VectorSubtract(vert[2], vert[1], edge[1]);
				CrossProduct(edge[1], edge[0], facenormal);
				if (facenormal[0] || facenormal[1] || facenormal[2])
				{
					VectorNormalize(facenormal);
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
	if (node->plane)
	{
		float f = PlaneDiff(info->center, node->plane);
		if (f >= -info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[0]);
		if (f <= info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[1]);
	}
	else
	{
		if (((mleaf_t *)node)->numleafsurfaces)
			Mod_Q1BSP_FindNonSolidLocation_r_Leaf(info, (mleaf_t *)node);
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
		Mod_Q1BSP_FindNonSolidLocation_r(&info, model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode);
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
			return SUPERCONTENTS_LAVA | SUPERCONTENTS_NODROP;
		case CONTENTS_SKY:
			return SUPERCONTENTS_SKY | SUPERCONTENTS_NODROP;
	}
	return 0;
}

int Mod_Q1BSP_NativeContentsFromSuperContents(model_t *model, int supercontents)
{
	if (supercontents & (SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY))
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

typedef struct RecursiveHullCheckTraceInfo_s
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

extern cvar_t collision_prefernudgedfraction;
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
		if (num & SUPERCONTENTS_SOLID)
			t->trace->hittexture = &mod_q1bsp_texture_solid;
		else if (num & SUPERCONTENTS_SKY)
			t->trace->hittexture = &mod_q1bsp_texture_sky;
		else if (num & SUPERCONTENTS_LAVA)
			t->trace->hittexture = &mod_q1bsp_texture_lava;
		else if (num & SUPERCONTENTS_SLIME)
			t->trace->hittexture = &mod_q1bsp_texture_slime;
		else
			t->trace->hittexture = &mod_q1bsp_texture_water;
		t->trace->hitq3surfaceflags = t->trace->hittexture->surfaceflags;
		t->trace->hitsupercontents = num;
		if (num & t->trace->hitsupercontentsmask)
		{
			// if the first leaf is solid, set startsolid
			if (t->trace->allsolid)
				t->trace->startsolid = true;
#if COLLISIONPARANOID >= 3
			Con_Print("S");
#endif
			return HULLCHECKSTATE_SOLID;
		}
		else
		{
			t->trace->allsolid = false;
#if COLLISIONPARANOID >= 3
			Con_Print("E");
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
			Con_Print("<");
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
			Con_Print(">");
#endif
			num = node->children[0];
			goto loc0;
		}
		side = 0;
	}

	// the line intersects, find intersection point
	// LordHavoc: this uses the original trace for maximum accuracy
#if COLLISIONPARANOID >= 3
	Con_Print("M");
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

	if (collision_prefernudgedfraction.integer)
		t->trace->realfraction = t->trace->fraction;

#if COLLISIONPARANOID >= 3
	Con_Print("D");
#endif
	return HULLCHECKSTATE_DONE;
}

//#if COLLISIONPARANOID < 2
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
//#endif

static void Mod_Q1BSP_TraceBox(struct model_s *model, int frame, trace_t *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask)
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
	VectorSubtract(boxmaxs, boxmins, boxsize);
	if (boxsize[0] < 3)
		rhc.hull = &model->brushq1.hulls[0]; // 0x0x0
	else if (model->brush.ismcbsp)
	{
		if (boxsize[2] < 48) // pick the nearest of 40 or 56
			rhc.hull = &model->brushq1.hulls[2]; // 16x16x40
		else
			rhc.hull = &model->brushq1.hulls[1]; // 16x16x56
	}
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
	VectorMAMAM(1, start, 1, boxmins, -1, rhc.hull->clip_mins, rhc.start);
	VectorMAMAM(1, end, 1, boxmins, -1, rhc.hull->clip_mins, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
#if COLLISIONPARANOID >= 2
	Con_Printf("t(%f %f %f,%f %f %f,%i %f %f %f)", rhc.start[0], rhc.start[1], rhc.start[2], rhc.end[0], rhc.end[1], rhc.end[2], rhc.hull - model->brushq1.hulls, rhc.hull->clip_mins[0], rhc.hull->clip_mins[1], rhc.hull->clip_mins[2]);
	Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	{

		double test[3];
		trace_t testtrace;
		VectorLerp(rhc.start, rhc.trace->fraction, rhc.end, test);
		memset(&testtrace, 0, sizeof(trace_t));
		rhc.trace = &testtrace;
		rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
		rhc.trace->fraction = 1;
		rhc.trace->realfraction = 1;
		rhc.trace->allsolid = true;
		VectorCopy(test, rhc.start);
		VectorCopy(test, rhc.end);
		VectorClear(rhc.dist);
		Mod_Q1BSP_RecursiveHullCheckPoint(&rhc, rhc.hull->firstclipnode);
		//Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, test, test);
		if (!trace->startsolid && testtrace.startsolid)
			Con_Printf(" - ended in solid!\n");
	}
	Con_Print("\n");
#else
	if (VectorLength2(rhc.dist))
		Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	else
		Mod_Q1BSP_RecursiveHullCheckPoint(&rhc, rhc.hull->firstclipnode);
#endif
}

void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int boxsupercontents, int boxq3surfaceflags, texture_t *boxtexture)
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
	cbox_planes[0].q3surfaceflags = boxq3surfaceflags;cbox_planes[0].texture = boxtexture;
	cbox_planes[1].q3surfaceflags = boxq3surfaceflags;cbox_planes[1].texture = boxtexture;
	cbox_planes[2].q3surfaceflags = boxq3surfaceflags;cbox_planes[2].texture = boxtexture;
	cbox_planes[3].q3surfaceflags = boxq3surfaceflags;cbox_planes[3].texture = boxtexture;
	cbox_planes[4].q3surfaceflags = boxq3surfaceflags;cbox_planes[4].texture = boxtexture;
	cbox_planes[5].q3surfaceflags = boxq3surfaceflags;cbox_planes[5].texture = boxtexture;
	memset(trace, 0, sizeof(trace_t));
	trace->hitsupercontentsmask = hitsupercontentsmask;
	trace->fraction = 1;
	trace->realfraction = 1;
	Collision_TraceLineBrushFloat(trace, start, end, &cbox, &cbox);
#else
	RecursiveHullCheckTraceInfo_t rhc;
	static hull_t box_hull;
	static dclipnode_t box_clipnodes[6];
	static mplane_t box_planes[6];
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

	if (box_hull.clipnodes == NULL)
	{
		int i, side;

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

static int Mod_Q1BSP_TraceLineOfSight_RecursiveNodeCheck(mnode_t *node, double p1[3], double p2[3])
{
	double t1, t2;
	double midf, mid[3];
	int ret, side;

	// check for empty
	while (node->plane)
	{
		// find the point distances
		mplane_t *plane = node->plane;
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
				node = node->children[1];
				continue;
			}
			side = 1;
		}
		else
		{
			if (t2 >= 0)
			{
				node = node->children[0];
				continue;
			}
			side = 0;
		}

		midf = t1 / (t1 - t2);
		VectorLerp(p1, midf, p2, mid);

		// recurse both sides, front side first
		// return 2 if empty is followed by solid (hit something)
		// do not return 2 if both are solid or both empty,
		// or if start is solid and end is empty
		// as these degenerate cases usually indicate the eye is in solid and
		// should see the target point anyway
		ret = Mod_Q1BSP_TraceLineOfSight_RecursiveNodeCheck(node->children[side    ], p1, mid);
		if (ret != 0)
			return ret;
		ret = Mod_Q1BSP_TraceLineOfSight_RecursiveNodeCheck(node->children[side ^ 1], mid, p2);
		if (ret != 1)
			return ret;
		return 2;
	}
	return ((mleaf_t *)node)->clusterindex < 0;
}

static qboolean Mod_Q1BSP_TraceLineOfSight(struct model_s *model, const vec3_t start, const vec3_t end)
{
	// this function currently only supports same size start and end
	double tracestart[3], traceend[3];
	VectorCopy(start, tracestart);
	VectorCopy(end, traceend);
	return Mod_Q1BSP_TraceLineOfSight_RecursiveNodeCheck(model->brush.data_nodes, tracestart, traceend) != 2;
}

static int Mod_Q1BSP_LightPoint_RecursiveBSPNode(model_t *model, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const mnode_t *node, float x, float y, float startz, float endz)
{
	int side;
	float front, back;
	float mid, distz = endz - startz;

loc0:
	if (!node->plane)
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
	if (node->children[side]->plane && Mod_Q1BSP_LightPoint_RecursiveBSPNode(model, ambientcolor, diffusecolor, diffusenormal, node->children[side], x, y, startz, mid))
		return true;	// hit something
	else
	{
		// check for impact on this node
		if (node->numsurfaces)
		{
			int i, ds, dt;
			msurface_t *surface;

			surface = model->data_surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surface++)
			{
				if (!(surface->texture->basematerialflags & MATERIALFLAG_WALL) || !surface->lightmapinfo->samples)
					continue;	// no lightmaps

				ds = (int) (x * surface->lightmapinfo->texinfo->vecs[0][0] + y * surface->lightmapinfo->texinfo->vecs[0][1] + mid * surface->lightmapinfo->texinfo->vecs[0][2] + surface->lightmapinfo->texinfo->vecs[0][3]) - surface->lightmapinfo->texturemins[0];
				dt = (int) (x * surface->lightmapinfo->texinfo->vecs[1][0] + y * surface->lightmapinfo->texinfo->vecs[1][1] + mid * surface->lightmapinfo->texinfo->vecs[1][2] + surface->lightmapinfo->texinfo->vecs[1][3]) - surface->lightmapinfo->texturemins[1];

				if (ds >= 0 && ds < surface->lightmapinfo->extents[0] && dt >= 0 && dt < surface->lightmapinfo->extents[1])
				{
					unsigned char *lightmap;
					int lmwidth, lmheight, maps, line3, size3, dsfrac = ds & 15, dtfrac = dt & 15, scale = 0, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
					lmwidth = ((surface->lightmapinfo->extents[0]>>4)+1);
					lmheight = ((surface->lightmapinfo->extents[1]>>4)+1);
					line3 = lmwidth * 3; // LordHavoc: *3 for colored lighting
					size3 = lmwidth * lmheight * 3; // LordHavoc: *3 for colored lighting

					lightmap = surface->lightmapinfo->samples + ((dt>>4) * lmwidth + (ds>>4))*3; // LordHavoc: *3 for colored lighting

					for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++)
					{
						scale = r_refdef.lightstylevalue[surface->lightmapinfo->styles[maps]];
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
					return true; // success
				}
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
	Mod_Q1BSP_LightPoint_RecursiveBSPNode(model, ambientcolor, diffusecolor, diffusenormal, model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode, p[0], p[1], p[2] + 0.125, p[2] - 65536);
	// pretend lighting is coming down from above (due to lack of a lightgrid to know primary lighting direction)
	VectorSet(diffusenormal, 0, 0, 1);
}

static void Mod_Q1BSP_DecompressVis(const unsigned char *in, const unsigned char *inend, unsigned char *out, unsigned char *outend)
{
	int c;
	unsigned char *outstart = out;
	while (out < outend)
	{
		if (in == inend)
		{
			Con_Printf("Mod_Q1BSP_DecompressVis: input underrun on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, (int)(out - outstart), (int)(outend - outstart));
			return;
		}
		c = *in++;
		if (c)
			*out++ = c;
		else
		{
			if (in == inend)
			{
				Con_Printf("Mod_Q1BSP_DecompressVis: input underrun (during zero-run) on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, (int)(out - outstart), (int)(outend - outstart));
				return;
			}
			for (c = *in++;c > 0;c--)
			{
				if (out == outend)
				{
					Con_Printf("Mod_Q1BSP_DecompressVis: output overrun on model \"%s\" (decompressed %i of %i output bytes)\n", loadmodel->name, (int)(out - outstart), (int)(outend - outstart));
					return;
				}
				*out++ = 0;
			}
		}
	}
}

/*
=============
R_Q1BSP_LoadSplitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_Q1BSP_LoadSplitSky (unsigned char *src, int width, int height, int bytesperpixel)
{
	int i, j;
	unsigned solidpixels[128*128], alphapixels[128*128];

	// if sky isn't the right size, just use it as a solid layer
	if (width != 256 || height != 128)
	{
		loadmodel->brush.solidskytexture = R_LoadTexture2D(loadmodel->texturepool, "sky_solidtexture", width, height, src, bytesperpixel == 4 ? TEXTYPE_RGBA : TEXTYPE_PALETTE, TEXF_PRECACHE, bytesperpixel == 1 ? palette_complete : NULL);
		loadmodel->brush.alphaskytexture = NULL;
		return;
	}

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				solidpixels[(i*128) + j] = ((unsigned *)src)[i*256+j+128];
				alphapixels[(i*128) + j] = ((unsigned *)src)[i*256+j];
			}
		}
	}
	else
	{
		// make an average value for the back to avoid
		// a fringe on the top level
		int p, r, g, b;
		union
		{
			unsigned int i;
			unsigned char b[4];
		}
		rgba;
		r = g = b = 0;
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				rgba.i = palette_complete[src[i*256 + j + 128]];
				r += rgba.b[0];
				g += rgba.b[1];
				b += rgba.b[2];
			}
		}
		rgba.b[0] = r/(128*128);
		rgba.b[1] = g/(128*128);
		rgba.b[2] = b/(128*128);
		rgba.b[3] = 0;
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				solidpixels[(i*128) + j] = palette_complete[src[i*256 + j + 128]];
				alphapixels[(i*128) + j] = (p = src[i*256 + j]) ? palette_complete[p] : rgba.i;
			}
		}
	}

	loadmodel->brush.solidskytexture = R_LoadTexture2D(loadmodel->texturepool, "sky_solidtexture", 128, 128, (unsigned char *) solidpixels, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	loadmodel->brush.alphaskytexture = R_LoadTexture2D(loadmodel->texturepool, "sky_alphatexture", 128, 128, (unsigned char *) alphapixels, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
}

static void Mod_Q1BSP_LoadTextures(lump_t *l)
{
	int i, j, k, num, max, altmax, mtwidth, mtheight, *dofs, incomplete;
	miptex_t *dmiptex;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	unsigned char *data, *mtdata;
	const char *s;
	char mapname[MAX_QPATH], name[MAX_QPATH];

	loadmodel->data_textures = NULL;

	// add two slots for notexture walls and notexture liquids
	if (l->filelen)
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong (m->nummiptex);
		loadmodel->num_textures = m->nummiptex + 2;
	}
	else
	{
		m = NULL;
		loadmodel->num_textures = 2;
	}

	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_textures * sizeof(texture_t));

	// fill out all slots with notexture
	for (i = 0, tx = loadmodel->data_textures;i < loadmodel->num_textures;i++, tx++)
	{
		strlcpy(tx->name, "NO TEXTURE FOUND", sizeof(tx->name));
		tx->width = 16;
		tx->height = 16;
		tx->numskinframes = 1;
		tx->skinframerate = 1;
		tx->currentskinframe = tx->skinframes;
		tx->skinframes[0].base = r_texture_notexture;
		tx->backgroundcurrentskinframe = tx->backgroundskinframes;
		tx->basematerialflags = 0;
		if (i == loadmodel->num_textures - 1)
		{
			tx->basematerialflags |= MATERIALFLAG_WATER | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_NOSHADOW;
			tx->supercontents = mod_q1bsp_texture_water.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_water.surfaceflags;
		}
		else
		{
			tx->basematerialflags |= MATERIALFLAG_WALL;
			tx->supercontents = mod_q1bsp_texture_solid.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_solid.surfaceflags;
		}
		tx->currentframe = tx;
	}

	if (!m)
		return;

	s = loadmodel->name;
	if (!strncasecmp(s, "maps/", 5))
		s += 5;
	FS_StripExtension(s, mapname, sizeof(mapname));

	// just to work around bounds checking when debugging with it (array index out of bounds error thing)
	dofs = m->dataofs;
	// LordHavoc: mostly rewritten map texture loader
	for (i = 0;i < m->nummiptex;i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1 || r_nosurftextures.integer)
			continue;
		dmiptex = (miptex_t *)((unsigned char *)m + dofs[i]);

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
			mtdata = (unsigned char *)dmiptex + j;
		}

		if ((mtwidth & 15) || (mtheight & 15))
			Con_Printf("warning: texture \"%s\" in \"%s\" is not 16 aligned\n", dmiptex->name, loadmodel->name);

		// LordHavoc: force all names to lowercase
		for (j = 0;name[j];j++)
			if (name[j] >= 'A' && name[j] <= 'Z')
				name[j] += 'a' - 'A';

		tx = loadmodel->data_textures + i;
		strlcpy(tx->name, name, sizeof(tx->name));
		tx->width = mtwidth;
		tx->height = mtheight;

		if (!tx->name[0])
		{
			sprintf(tx->name, "unnamed%i", i);
			Con_Printf("warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		if (cls.state != ca_dedicated)
		{
			// LordHavoc: HL sky textures are entirely different than quake
			if (!loadmodel->brush.ishlbsp && !strncmp(tx->name, "sky", 3) && mtwidth == 256 && mtheight == 128)
			{
				if (loadmodel->isworldmodel)
				{
					data = loadimagepixels(tx->name, false, 0, 0);
					if (data)
					{
						R_Q1BSP_LoadSplitSky(data, image_width, image_height, 4);
						Mem_Free(data);
					}
					else if (mtdata != NULL)
						R_Q1BSP_LoadSplitSky(mtdata, mtwidth, mtheight, 1);
				}
			}
			else
			{
				if (!Mod_LoadSkinFrame(&tx->skinframes[0], gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s/%s", mapname, tx->name), TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, false, true)
				 && !Mod_LoadSkinFrame(&tx->skinframes[0], gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s", tx->name), TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, false, true))
				{
					// did not find external texture, load it from the bsp or wad3
					if (loadmodel->brush.ishlbsp)
					{
						// internal texture overrides wad
						unsigned char *pixels, *freepixels;
						pixels = freepixels = NULL;
						if (mtdata)
							pixels = W_ConvertWAD3Texture(dmiptex);
						if (pixels == NULL)
							pixels = freepixels = W_GetTexture(tx->name);
						if (pixels != NULL)
						{
							tx->width = image_width;
							tx->height = image_height;
							Mod_LoadSkinFrame_Internal(&tx->skinframes[0], tx->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, false, false, pixels, image_width, image_height, 32, NULL, NULL);
						}
						if (freepixels)
							Mem_Free(freepixels);
					}
					else if (mtdata) // texture included
						Mod_LoadSkinFrame_Internal(&tx->skinframes[0], tx->name, TEXF_MIPMAP | TEXF_PRECACHE | TEXF_PICMIP, false, r_fullbrights.integer, mtdata, tx->width, tx->height, 8, NULL, NULL);
				}
			}
			if (tx->skinframes[0].base == NULL)
			{
				// no texture found
				tx->width = 16;
				tx->height = 16;
				tx->skinframes[0].base = r_texture_notexture;
			}
		}

		tx->basematerialflags = 0;
		if (tx->name[0] == '*')
		{
			// LordHavoc: some turbulent textures should not be affected by wateralpha
			if (strncmp(tx->name,"*lava",5)
			 && strncmp(tx->name,"*teleport",9)
			 && strncmp(tx->name,"*rift",5)) // Scourge of Armagon texture
				tx->basematerialflags |= MATERIALFLAG_WATERALPHA | MATERIALFLAG_NOSHADOW;
			if (!strncmp(tx->name, "*lava", 5))
			{
				tx->supercontents = mod_q1bsp_texture_lava.supercontents;
				tx->surfaceflags = mod_q1bsp_texture_lava.surfaceflags;
			}
			else if (!strncmp(tx->name, "*slime", 6))
			{
				tx->supercontents = mod_q1bsp_texture_slime.supercontents;
				tx->surfaceflags = mod_q1bsp_texture_slime.surfaceflags;
			}
			else
			{
				tx->supercontents = mod_q1bsp_texture_water.supercontents;
				tx->surfaceflags = mod_q1bsp_texture_water.surfaceflags;
			}
			tx->basematerialflags |= MATERIALFLAG_WATER | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_NOSHADOW;
		}
		else if (tx->name[0] == 's' && tx->name[1] == 'k' && tx->name[2] == 'y')
		{
			tx->supercontents = mod_q1bsp_texture_sky.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_sky.surfaceflags;
			tx->basematerialflags |= MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
		}
		else
		{
			tx->supercontents = mod_q1bsp_texture_solid.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_solid.surfaceflags;
			tx->basematerialflags |= MATERIALFLAG_WALL;
		}
		if (tx->skinframes[0].fog)
			tx->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;

		// start out with no animation
		tx->currentframe = tx;
	}

	// sequence the animations
	for (i = 0;i < m->nummiptex;i++)
	{
		tx = loadmodel->data_textures + i;
		if (!tx || tx->name[0] != '+' || tx->name[1] == 0 || tx->name[2] == 0)
			continue;
		if (tx->anim_total[0] || tx->anim_total[1])
			continue;	// already sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		for (j = i;j < m->nummiptex;j++)
		{
			tx2 = loadmodel->data_textures + j;
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
	unsigned char *in, *out, *data, d;
	char litfilename[MAX_QPATH];
	char dlitfilename[MAX_QPATH];
	fs_offset_t filesize;
	if (loadmodel->brush.ishlbsp) // LordHavoc: load the colored lighting data straight
	{
		loadmodel->brushq1.lightdata = (unsigned char *)Mem_Alloc(loadmodel->mempool, l->filelen);
		for (i=0; i<l->filelen; i++)
			loadmodel->brushq1.lightdata[i] = mod_base[l->fileofs+i] >>= 1;
	}
	else if (loadmodel->brush.ismcbsp)
	{
		loadmodel->brushq1.lightdata = (unsigned char *)Mem_Alloc(loadmodel->mempool, l->filelen);
		memcpy(loadmodel->brushq1.lightdata, mod_base + l->fileofs, l->filelen);
	}
	else // LordHavoc: bsp version 29 (normal white lighting)
	{
		// LordHavoc: hope is not lost yet, check for a .lit file to load
		strlcpy (litfilename, loadmodel->name, sizeof (litfilename));
		FS_StripExtension (litfilename, litfilename, sizeof (litfilename));
		strlcpy (dlitfilename, litfilename, sizeof (dlitfilename));
		strlcat (litfilename, ".lit", sizeof (litfilename));
		strlcat (dlitfilename, ".dlit", sizeof (dlitfilename));
		data = (unsigned char*) FS_LoadFile(litfilename, tempmempool, false, &filesize);
		if (data)
		{
			if (filesize == (fs_offset_t)(8 + l->filelen * 3) && data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Con_DPrintf("loaded %s\n", litfilename);
					loadmodel->brushq1.lightdata = (unsigned char *)Mem_Alloc(loadmodel->mempool, filesize - 8);
					memcpy(loadmodel->brushq1.lightdata, data + 8, filesize - 8);
					Mem_Free(data);
					data = (unsigned char*) FS_LoadFile(dlitfilename, tempmempool, false, &filesize);
					if (data)
					{
						if (filesize == (fs_offset_t)(8 + l->filelen * 3) && data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
						{
							i = LittleLong(((int *)data)[1]);
							if (i == 1)
							{
								Con_DPrintf("loaded %s\n", dlitfilename);
								loadmodel->brushq1.nmaplightdata = (unsigned char *)Mem_Alloc(loadmodel->mempool, filesize - 8);
								memcpy(loadmodel->brushq1.nmaplightdata, data + 8, filesize - 8);
								loadmodel->brushq3.deluxemapping_modelspace = false;
								loadmodel->brushq3.deluxemapping = true;
							}
						}
						Mem_Free(data);
						data = NULL;
					}
					return;
				}
				else
					Con_Printf("Unknown .lit file version (%d)\n", i);
			}
			else if (filesize == 8)
				Con_Print("Empty .lit file, ignoring\n");
			else
				Con_Printf("Corrupt .lit file (file size %i bytes, should be %i bytes), ignoring\n", (int) filesize, (int) (8 + l->filelen * 3));
			if (data)
			{
				Mem_Free(data);
				data = NULL;
			}
		}
		// LordHavoc: oh well, expand the white lighting data
		if (!l->filelen)
			return;
		loadmodel->brushq1.lightdata = (unsigned char *)Mem_Alloc(loadmodel->mempool, l->filelen*3);
		in = mod_base + l->fileofs;
		out = loadmodel->brushq1.lightdata;
		for (i = 0;i < l->filelen;i++)
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	}
}

static void Mod_Q1BSP_LoadVisibility(lump_t *l)
{
	loadmodel->brushq1.num_compressedpvs = 0;
	loadmodel->brushq1.data_compressedpvs = NULL;
	if (!l->filelen)
		return;
	loadmodel->brushq1.num_compressedpvs = l->filelen;
	loadmodel->brushq1.data_compressedpvs = (unsigned char *)Mem_Alloc(loadmodel->mempool, l->filelen);
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
	if (!COM_ParseTokenConsole(&data))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseTokenConsole(&data))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strlcpy(key, com_token + 1, sizeof(key));
		else
			strlcpy(key, com_token, sizeof(key));
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseTokenConsole(&data))
			return; // error
		dpsnprintf(value, sizeof(value), "%s", com_token);
		if (!strcmp("wad", key)) // for HalfLife maps
		{
			if (loadmodel->brush.ishlbsp)
			{
				j = 0;
				for (i = 0;i < (int)sizeof(value);i++)
					if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
						break;
				if (value[i])
				{
					for (;i < (int)sizeof(value);i++)
					{
						// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
						if (value[i] == '\\' || value[i] == '/' || value[i] == ':')
							j = i+1;
						else if (value[i] == ';' || value[i] == 0)
						{
							k = value[i];
							value[i] = 0;
							strlcpy(wadname, "textures/", sizeof(wadname));
							strlcat(wadname, &value[j], sizeof(wadname));
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
	loadmodel->brush.entities = (char *)Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	if (loadmodel->brush.ishlbsp)
		Mod_Q1BSP_ParseWadsFromEntityLump(loadmodel->brush.entities);
}


static void Mod_Q1BSP_LoadVertexes(lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadVertexes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mvertex_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.vertexes = out;
	loadmodel->brushq1.numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

// The following two functions should be removed and MSG_* or SZ_* function sets adjusted so they
// can be used for this
// REMOVEME
int SB_ReadInt (unsigned char **buffer)
{
	int	i;
	i = ((*buffer)[0]) + 256*((*buffer)[1]) + 65536*((*buffer)[2]) + 16777216*((*buffer)[3]);
	(*buffer) += 4;
	return i;
}

// REMOVEME
float SB_ReadFloat (unsigned char **buffer)
{
	union
	{
		int		i;
		float	f;
	} u;

	u.i = SB_ReadInt (buffer);
	return u.f;
}

static void Mod_Q1BSP_LoadSubmodels(lump_t *l, hullinfo_t *hullinfo)
{
	unsigned char		*index;
	dmodel_t	*out;
	int			i, j, count;

	index = (unsigned char *)(mod_base + l->fileofs);
	if (l->filelen % (48+4*hullinfo->filehulls))
		Host_Error ("Mod_Q1BSP_LoadSubmodels: funny lump size in %s", loadmodel->name);

	count = l->filelen / (48+4*hullinfo->filehulls);
	out = (dmodel_t *)Mem_Alloc (loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.submodels = out;
	loadmodel->brush.numsubmodels = count;

	for (i = 0; i < count; i++, out++)
	{
	// spread out the mins / maxs by a pixel
		out->mins[0] = SB_ReadFloat (&index) - 1;
		out->mins[1] = SB_ReadFloat (&index) - 1;
		out->mins[2] = SB_ReadFloat (&index) - 1;
		out->maxs[0] = SB_ReadFloat (&index) + 1;
		out->maxs[1] = SB_ReadFloat (&index) + 1;
		out->maxs[2] = SB_ReadFloat (&index) + 1;
		out->origin[0] = SB_ReadFloat (&index);
		out->origin[1] = SB_ReadFloat (&index);
		out->origin[2] = SB_ReadFloat (&index);
		for (j = 0; j < hullinfo->filehulls; j++)
			out->headnode[j] = SB_ReadInt (&index);
		out->visleafs = SB_ReadInt (&index);
		out->firstface = SB_ReadInt (&index);
		out->numfaces = SB_ReadInt (&index);
	}
}

static void Mod_Q1BSP_LoadEdges(lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (dedge_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (medge_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq1.edges = out;
	loadmodel->brushq1.numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
		if (out->v[0] >= loadmodel->brushq1.numvertexes || out->v[1] >= loadmodel->brushq1.numvertexes)
		{
			Con_Printf("Mod_Q1BSP_LoadEdges: %s has invalid vertex indices in edge %i (vertices %i %i >= numvertices %i)\n", loadmodel->name, i, out->v[0], out->v[1], loadmodel->brushq1.numvertexes);
			out->v[0] = 0;
			out->v[1] = 0;
		}
	}
}

static void Mod_Q1BSP_LoadTexinfo(lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;

	in = (texinfo_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadTexinfo: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mtexinfo_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

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
		if (loadmodel->data_textures)
		{
			if ((unsigned int) miptex >= (unsigned int) loadmodel->num_textures)
				Con_Printf("error in model \"%s\": invalid miptex index %i(of %i)\n", loadmodel->name, miptex, loadmodel->num_textures);
			else
				out->texture = loadmodel->data_textures + miptex;
		}
		if (out->flags & TEX_SPECIAL)
		{
			// if texture chosen is NULL or the shader needs a lightmap,
			// force to notexture water shader
			if (out->texture == NULL || out->texture->basematerialflags & MATERIALFLAG_WALL)
				out->texture = loadmodel->data_textures + (loadmodel->num_textures - 1);
		}
		else
		{
			// if texture chosen is NULL, force to notexture
			if (out->texture == NULL)
				out->texture = loadmodel->data_textures + (loadmodel->num_textures - 2);
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
			Con_Print("SubdividePolygon: ran out of triangles in buffer, please increase your r_subdivide_size\n");
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
static void Mod_Q1BSP_GenerateWarpMesh(msurface_t *surface)
{
	int i, j;
	surfvertex_t *v;
	surfmesh_t *mesh;

	subdivpolytriangles = 0;
	subdivpolyverts = 0;
	SubdividePolygon(surface->num_vertices, (surface->mesh->data_vertex3f + 3 * surface->num_firstvertex));
	if (subdivpolytriangles < 1)
		Host_Error("Mod_Q1BSP_GenerateWarpMesh: no triangles?");

	surface->mesh = mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + subdivpolytriangles * sizeof(int[3]) + subdivpolyverts * sizeof(surfvertex_t));
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
		v->st[0] = DotProduct(v->v, surface->lightmapinfo->texinfo->vecs[0]);
		v->st[1] = DotProduct(v->v, surface->lightmapinfo->texinfo->vecs[1]);
	}
}
#endif

static qboolean Mod_Q1BSP_AllocLightmapBlock(int *lineused, int totalwidth, int totalheight, int blockwidth, int blockheight, int *outx, int *outy)
{
	int y, x2, y2;
	int bestx = totalwidth, besty = 0;
	// find the left-most space we can find
	for (y = 0;y <= totalheight - blockheight;y++)
	{
		x2 = 0;
		for (y2 = 0;y2 < blockheight;y2++)
			x2 = max(x2, lineused[y+y2]);
		if (bestx > x2)
		{
			bestx = x2;
			besty = y;
		}
	}
	// if the best was not good enough, return failure
	if (bestx > totalwidth - blockwidth)
		return false;
	// we found a good spot
	if (outx)
		*outx = bestx;
	if (outy)
		*outy = besty;
	// now mark the space used
	for (y2 = 0;y2 < blockheight;y2++)
		lineused[besty+y2] = bestx + blockwidth;
	// return success
	return true;
}

static void Mod_Q1BSP_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *surface;
	int i, j, count, surfacenum, planenum, smax, tmax, ssize, tsize, firstedge, numedges, totalverts, totaltris, lightmapnumber;
	float texmins[2], texmaxs[2], val, lightmaptexcoordscale;
#define LIGHTMAPSIZE 256
	rtexture_t *lightmaptexture, *deluxemaptexture;
	int lightmap_lineused[LIGHTMAPSIZE];

	in = (dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	loadmodel->data_surfaces = (msurface_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_t));
	loadmodel->data_surfaces_lightmapinfo = (msurface_lightmapinfo_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_lightmapinfo_t));

	loadmodel->num_surfaces = count;

	totalverts = 0;
	totaltris = 0;
	for (surfacenum = 0, in = (dface_t *)(mod_base + l->fileofs);surfacenum < count;surfacenum++, in++)
	{
		numedges = LittleShort(in->numedges);
		totalverts += numedges;
		totaltris += numedges - 2;
	}

	Mod_AllocSurfMesh(loadmodel->mempool, totalverts, totaltris, true, false, false);

	lightmaptexture = NULL;
	deluxemaptexture = r_texture_blanknormalmap;
	lightmapnumber = 1;
	lightmaptexcoordscale = 1.0f / (float)LIGHTMAPSIZE;

	totalverts = 0;
	totaltris = 0;
	for (surfacenum = 0, in = (dface_t *)(mod_base + l->fileofs), surface = loadmodel->data_surfaces;surfacenum < count;surfacenum++, in++, surface++)
	{
		surface->lightmapinfo = loadmodel->data_surfaces_lightmapinfo + surfacenum;

		// FIXME: validate edges, texinfo, etc?
		firstedge = LittleLong(in->firstedge);
		numedges = LittleShort(in->numedges);
		if ((unsigned int) firstedge > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) firstedge + (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid edge range (firstedge %i, numedges %i, model edges %i)", firstedge, numedges, loadmodel->brushq1.numsurfedges);
		i = LittleShort(in->texinfo);
		if ((unsigned int) i >= (unsigned int) loadmodel->brushq1.numtexinfo)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid texinfo index %i(model has %i texinfos)", i, loadmodel->brushq1.numtexinfo);
		surface->lightmapinfo->texinfo = loadmodel->brushq1.texinfo + i;
		surface->texture = surface->lightmapinfo->texinfo->texture;

		planenum = LittleShort(in->planenum);
		if ((unsigned int) planenum >= (unsigned int) loadmodel->brush.num_planes)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid plane index %i (model has %i planes)", planenum, loadmodel->brush.num_planes);

		//surface->flags = surface->texture->flags;
		//if (LittleShort(in->side))
		//	surface->flags |= SURF_PLANEBACK;
		//surface->plane = loadmodel->brush.data_planes + planenum;

		surface->num_firstvertex = totalverts;
		surface->num_vertices = numedges;
		surface->num_firsttriangle = totaltris;
		surface->num_triangles = numedges - 2;
		totalverts += numedges;
		totaltris += numedges - 2;

		// convert edges back to a normal polygon
		for (i = 0;i < surface->num_vertices;i++)
		{
			int lindex = loadmodel->brushq1.surfedges[firstedge + i];
			float s, t;
			if (lindex > 0)
				VectorCopy(loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[lindex].v[0]].position, (loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3);
			else
				VectorCopy(loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[-lindex].v[1]].position, (loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3);
			s = DotProduct(((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3), surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3];
			t = DotProduct(((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3), surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3];
			(loadmodel->surfmesh.data_texcoordtexture2f + 2 * surface->num_firstvertex)[i * 2 + 0] = s / surface->texture->width;
			(loadmodel->surfmesh.data_texcoordtexture2f + 2 * surface->num_firstvertex)[i * 2 + 1] = t / surface->texture->height;
			(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * surface->num_firstvertex)[i * 2 + 0] = 0;
			(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * surface->num_firstvertex)[i * 2 + 1] = 0;
			(loadmodel->surfmesh.data_lightmapoffsets + surface->num_firstvertex)[i] = 0;
		}

		for (i = 0;i < surface->num_triangles;i++)
		{
			(loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle)[i * 3 + 0] = 0 + surface->num_firstvertex;
			(loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle)[i * 3 + 1] = i + 1 + surface->num_firstvertex;
			(loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle)[i * 3 + 2] = i + 2 + surface->num_firstvertex;
		}

		// compile additional data about the surface geometry
		Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, loadmodel->surfmesh.data_vertex3f, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle), loadmodel->surfmesh.data_normal3f, true);
		Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_texcoordtexture2f, loadmodel->surfmesh.data_normal3f, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle), loadmodel->surfmesh.data_svector3f, loadmodel->surfmesh.data_tvector3f, true);
		BoxFromPoints(surface->mins, surface->maxs, surface->num_vertices, (loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex));

		// generate surface extents information
		texmins[0] = texmaxs[0] = DotProduct((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex), surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3];
		texmins[1] = texmaxs[1] = DotProduct((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex), surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3];
		for (i = 1;i < surface->num_vertices;i++)
		{
			for (j = 0;j < 2;j++)
			{
				val = DotProduct((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3, surface->lightmapinfo->texinfo->vecs[j]) + surface->lightmapinfo->texinfo->vecs[j][3];
				texmins[j] = min(texmins[j], val);
				texmaxs[j] = max(texmaxs[j], val);
			}
		}
		for (i = 0;i < 2;i++)
		{
			surface->lightmapinfo->texturemins[i] = (int) floor(texmins[i] / 16.0) * 16;
			surface->lightmapinfo->extents[i] = (int) ceil(texmaxs[i] / 16.0) * 16 - surface->lightmapinfo->texturemins[i];
		}

		smax = surface->lightmapinfo->extents[0] >> 4;
		tmax = surface->lightmapinfo->extents[1] >> 4;
		ssize = (surface->lightmapinfo->extents[0] >> 4) + 1;
		tsize = (surface->lightmapinfo->extents[1] >> 4) + 1;

		// lighting info
		for (i = 0;i < MAXLIGHTMAPS;i++)
			surface->lightmapinfo->styles[i] = in->styles[i];
		surface->lightmaptexture = NULL;
		surface->deluxemaptexture = r_texture_blanknormalmap;
		i = LittleLong(in->lightofs);
		if (i == -1)
		{
			surface->lightmapinfo->samples = NULL;
			// give non-lightmapped water a 1x white lightmap
			if ((surface->texture->basematerialflags & MATERIALFLAG_WATER) && (surface->lightmapinfo->texinfo->flags & TEX_SPECIAL) && ssize <= 256 && tsize <= 256)
			{
				surface->lightmapinfo->samples = (unsigned char *)Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
				surface->lightmapinfo->styles[0] = 0;
				memset(surface->lightmapinfo->samples, 128, ssize * tsize * 3);
			}
		}
		else if (loadmodel->brush.ishlbsp) // LordHavoc: HalfLife map (bsp version 30)
			surface->lightmapinfo->samples = loadmodel->brushq1.lightdata + i;
		else // LordHavoc: white lighting (bsp version 29)
		{
			surface->lightmapinfo->samples = loadmodel->brushq1.lightdata + (i * 3);
			if (loadmodel->brushq1.nmaplightdata)
				surface->lightmapinfo->nmapsamples = loadmodel->brushq1.nmaplightdata + (i * 3);
		}

		// check if we should apply a lightmap to this
		if (!(surface->lightmapinfo->texinfo->flags & TEX_SPECIAL) || surface->lightmapinfo->samples)
		{
			int i, iu, iv, lightmapx, lightmapy;
			float u, v, ubase, vbase, uscale, vscale;

			if (ssize > 256 || tsize > 256)
				Host_Error("Bad surface extents");
			// force lightmap upload on first time seeing the surface
			surface->cached_dlight = true;
			// stainmap for permanent marks on walls
			surface->lightmapinfo->stainsamples = (unsigned char *)Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
			// clear to white
			memset(surface->lightmapinfo->stainsamples, 255, ssize * tsize * 3);

			// find a place for this lightmap
			if (!lightmaptexture || !Mod_Q1BSP_AllocLightmapBlock(lightmap_lineused, LIGHTMAPSIZE, LIGHTMAPSIZE, ssize, tsize, &lightmapx, &lightmapy))
			{
				// could not find room, make a new lightmap
				lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%i", lightmapnumber), LIGHTMAPSIZE, LIGHTMAPSIZE, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
				if (loadmodel->brushq1.nmaplightdata)
					deluxemaptexture = R_LoadTexture2D(loadmodel->texturepool, va("deluxemap%i", lightmapnumber), LIGHTMAPSIZE, LIGHTMAPSIZE, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
				lightmapnumber++;
				memset(lightmap_lineused, 0, sizeof(lightmap_lineused));
				Mod_Q1BSP_AllocLightmapBlock(lightmap_lineused, LIGHTMAPSIZE, LIGHTMAPSIZE, ssize, tsize, &lightmapx, &lightmapy);
			}

			surface->lightmaptexture = lightmaptexture;
			surface->deluxemaptexture = deluxemaptexture;
			surface->lightmapinfo->lightmaporigin[0] = lightmapx;
			surface->lightmapinfo->lightmaporigin[1] = lightmapy;

			ubase = lightmapx * lightmaptexcoordscale;
			vbase = lightmapy * lightmaptexcoordscale;
			uscale = lightmaptexcoordscale;
			vscale = lightmaptexcoordscale;

			for (i = 0;i < surface->num_vertices;i++)
			{
				u = ((DotProduct(((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3), surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3]) + 8 - surface->lightmapinfo->texturemins[0]) * (1.0 / 16.0);
				v = ((DotProduct(((loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + i * 3), surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3]) + 8 - surface->lightmapinfo->texturemins[1]) * (1.0 / 16.0);
				(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * surface->num_firstvertex)[i * 2 + 0] = u * uscale + ubase;
				(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * surface->num_firstvertex)[i * 2 + 1] = v * vscale + vbase;
				// LordHavoc: calc lightmap data offset for vertex lighting to use
				iu = (int) u;
				iv = (int) v;
				(loadmodel->surfmesh.data_lightmapoffsets + surface->num_firstvertex)[i] = (bound(0, iv, tmax) * ssize + bound(0, iu, smax)) * 3;
			}
		}
	}
}

static void Mod_Q1BSP_LoadNodes_RecursiveSetParent(mnode_t *node, mnode_t *parent)
{
	//if (node->parent)
	//	Host_Error("Mod_Q1BSP_LoadNodes_RecursiveSetParent: runaway recursion");
	node->parent = parent;
	if (node->plane)
	{
		// this is a node, recurse to children
		Mod_Q1BSP_LoadNodes_RecursiveSetParent(node->children[0], node);
		Mod_Q1BSP_LoadNodes_RecursiveSetParent(node->children[1], node);
		// combine supercontents of children
		node->combinedsupercontents = node->children[0]->combinedsupercontents | node->children[1]->combinedsupercontents;
	}
	else
	{
		int j;
		mleaf_t *leaf = (mleaf_t *)node;
		// if this is a leaf, calculate supercontents mask from all collidable
		// primitives in the leaf (brushes and collision surfaces)
		// also flag if the leaf contains any collision surfaces
		leaf->combinedsupercontents = 0;
		// combine the supercontents values of all brushes in this leaf
		for (j = 0;j < leaf->numleafbrushes;j++)
			leaf->combinedsupercontents |= loadmodel->brush.data_brushes[leaf->firstleafbrush[j]].texture->supercontents;
		// check if this leaf contains any collision surfaces (q3 patches)
		for (j = 0;j < leaf->numleafsurfaces;j++)
		{
			msurface_t *surface = loadmodel->data_surfaces + leaf->firstleafsurface[j];
			if (surface->num_collisiontriangles)
			{
				leaf->containscollisionsurfaces = true;
				leaf->combinedsupercontents |= surface->texture->supercontents;
			}
		}
	}
}

static void Mod_Q1BSP_LoadNodes(lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brush.data_nodes = out;
	loadmodel->brush.num_nodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->brush.data_planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->brush.data_nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->brush.data_leafs + (-1 - p));
		}
	}

	Mod_Q1BSP_LoadNodes_RecursiveSetParent(loadmodel->brush.data_nodes, NULL);	// sets nodes and leafs
}

static void Mod_Q1BSP_LoadLeafs(lump_t *l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brush.data_leafs = out;
	loadmodel->brush.num_leafs = count;
	// get visleafs from the submodel data
	loadmodel->brush.num_pvsclusters = loadmodel->brushq1.submodels[0].visleafs;
	loadmodel->brush.num_pvsclusterbytes = (loadmodel->brush.num_pvsclusters+7)>>3;
	loadmodel->brush.data_pvsclusters = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_pvsclusters * loadmodel->brush.num_pvsclusterbytes);
	memset(loadmodel->brush.data_pvsclusters, 0xFF, loadmodel->brush.num_pvsclusters * loadmodel->brush.num_pvsclusterbytes);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		// FIXME: this function could really benefit from some error checking

		out->contents = LittleLong(in->contents);

		out->firstleafsurface = loadmodel->brush.data_leafsurfaces + LittleShort(in->firstmarksurface);
		out->numleafsurfaces = LittleShort(in->nummarksurfaces);
		if (out->firstleafsurface < 0 || LittleShort(in->firstmarksurface) + out->numleafsurfaces > loadmodel->brush.num_leafsurfaces)
		{
			Con_Printf("Mod_Q1BSP_LoadLeafs: invalid leafsurface range %i:%i outside range %i:%i\n", (int)(out->firstleafsurface - loadmodel->brush.data_leafsurfaces), (int)(out->firstleafsurface + out->numleafsurfaces - loadmodel->brush.data_leafsurfaces), 0, loadmodel->brush.num_leafsurfaces);
			out->firstleafsurface = NULL;
			out->numleafsurfaces = 0;
		}

		out->clusterindex = i - 1;
		if (out->clusterindex >= loadmodel->brush.num_pvsclusters)
			out->clusterindex = -1;

		p = LittleLong(in->visofs);
		// ignore visofs errors on leaf 0 (solid)
		if (p >= 0 && out->clusterindex >= 0)
		{
			if (p >= loadmodel->brushq1.num_compressedpvs)
				Con_Print("Mod_Q1BSP_LoadLeafs: invalid visofs\n");
			else
				Mod_Q1BSP_DecompressVis(loadmodel->brushq1.data_compressedpvs + p, loadmodel->brushq1.data_compressedpvs + loadmodel->brushq1.num_compressedpvs, loadmodel->brush.data_pvsclusters + out->clusterindex * loadmodel->brush.num_pvsclusterbytes, loadmodel->brush.data_pvsclusters + (out->clusterindex + 1) * loadmodel->brush.num_pvsclusterbytes);
		}

		for (j = 0;j < 4;j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// FIXME: Insert caustics here
	}
}

qboolean Mod_Q1BSP_CheckWaterAlphaSupport(void)
{
	int i, j;
	mleaf_t *leaf;
	const unsigned char *pvs;
	// check all liquid leafs to see if they can see into empty leafs, if any
	// can we can assume this map supports r_wateralpha
	for (i = 0, leaf = loadmodel->brush.data_leafs;i < loadmodel->brush.num_leafs;i++, leaf++)
	{
		if ((leaf->contents == CONTENTS_WATER || leaf->contents == CONTENTS_SLIME) && (leaf->clusterindex >= 0 && loadmodel->brush.data_pvsclusters))
		{
			pvs = loadmodel->brush.data_pvsclusters + leaf->clusterindex * loadmodel->brush.num_pvsclusterbytes;
			for (j = 0;j < loadmodel->brush.num_leafs;j++)
				if (CHECKPVSBIT(pvs, loadmodel->brush.data_leafs[j].clusterindex) && loadmodel->brush.data_leafs[j].contents == CONTENTS_EMPTY)
					return true;
		}
	}
	return false;
}

static void Mod_Q1BSP_LoadClipnodes(lump_t *l, hullinfo_t *hullinfo)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadClipnodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (dclipnode_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.clipnodes = out;
	loadmodel->brushq1.numclipnodes = count;

	for (i = 1; i < hullinfo->numhulls; i++)
	{
		hull = &loadmodel->brushq1.hulls[i];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brush.data_planes;
		hull->clip_mins[0] = hullinfo->hullsizes[i][0][0];
		hull->clip_mins[1] = hullinfo->hullsizes[i][0][1];
		hull->clip_mins[2] = hullinfo->hullsizes[i][0][2];
		hull->clip_maxs[0] = hullinfo->hullsizes[i][1][0];
		hull->clip_maxs[1] = hullinfo->hullsizes[i][1][1];
		hull->clip_maxs[2] = hullinfo->hullsizes[i][1][2];
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);
	}

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
		if (out->planenum < 0 || out->planenum >= loadmodel->brush.num_planes)
			Host_Error("Corrupt clipping hull(out of range planenum)");
		if (out->children[0] >= count || out->children[1] >= count)
			Host_Error("Corrupt clipping hull(out of range child)");
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

	in = loadmodel->brush.data_nodes;
	out = (dclipnode_t *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_nodes * sizeof(dclipnode_t));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = loadmodel->brush.num_nodes - 1;
	hull->planes = loadmodel->brush.data_planes;

	for (i = 0;i < loadmodel->brush.num_nodes;i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->brush.data_planes;
		out->children[0] = in->children[0]->plane ? in->children[0] - loadmodel->brush.data_nodes : ((mleaf_t *)in->children[0])->contents;
		out->children[1] = in->children[1]->plane ? in->children[1] - loadmodel->brush.data_nodes : ((mleaf_t *)in->children[1])->contents;
	}
}

static void Mod_Q1BSP_LoadLeaffaces(lump_t *l)
{
	int i, j;
	short *in;

	in = (short *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadLeaffaces: funny lump size in %s",loadmodel->name);
	loadmodel->brush.num_leafsurfaces = l->filelen / sizeof(*in);
	loadmodel->brush.data_leafsurfaces = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_leafsurfaces * sizeof(int));

	for (i = 0;i < loadmodel->brush.num_leafsurfaces;i++)
	{
		j = (unsigned) LittleShort(in[i]);
		if (j >= loadmodel->num_surfaces)
			Host_Error("Mod_Q1BSP_LoadLeaffaces: bad surface number");
		loadmodel->brush.data_leafsurfaces[i] = j;
	}
}

static void Mod_Q1BSP_LoadSurfedges(lump_t *l)
{
	int		i;
	int		*in;

	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadSurfedges: funny lump size in %s",loadmodel->name);
	loadmodel->brushq1.numsurfedges = l->filelen / sizeof(*in);
	loadmodel->brushq1.surfedges = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numsurfedges * sizeof(int));

	for (i = 0;i < loadmodel->brushq1.numsurfedges;i++)
		loadmodel->brushq1.surfedges[i] = LittleLong(in[i]);
}


static void Mod_Q1BSP_LoadPlanes(lump_t *l)
{
	int			i;
	mplane_t	*out;
	dplane_t 	*in;

	in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadPlanes: funny lump size in %s", loadmodel->name);

	loadmodel->brush.num_planes = l->filelen / sizeof(*in);
	loadmodel->brush.data_planes = out = (mplane_t *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_planes * sizeof(*out));

	for (i = 0;i < loadmodel->brush.num_planes;i++, in++, out++)
	{
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
		out->dist = LittleFloat(in->dist);

		PlaneClassify(out);
	}
}

static void Mod_Q1BSP_LoadMapBrushes(void)
{
#if 0
// unfinished
	int submodel, numbrushes;
	qboolean firstbrush;
	char *text, *maptext;
	char mapfilename[MAX_QPATH];
	FS_StripExtension (loadmodel->name, mapfilename, sizeof (mapfilename));
	strlcat (mapfilename, ".map", sizeof (mapfilename));
	maptext = (unsigned char*) FS_LoadFile(mapfilename, tempmempool, false, NULL);
	if (!maptext)
		return;
	text = maptext;
	if (!COM_ParseTokenConsole(&data))
		return; // error
	submodel = 0;
	for (;;)
	{
		if (!COM_ParseTokenConsole(&data))
			break;
		if (com_token[0] != '{')
			return; // error
		// entity
		firstbrush = true;
		numbrushes = 0;
		maxbrushes = 256;
		brushes = Mem_Alloc(loadmodel->mempool, maxbrushes * sizeof(mbrush_t));
		for (;;)
		{
			if (!COM_ParseTokenConsole(&data))
				return; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '{')
			{
				// brush
				if (firstbrush)
				{
					if (submodel)
					{
						if (submodel > loadmodel->brush.numsubmodels)
						{
							Con_Printf("Mod_Q1BSP_LoadMapBrushes: .map has more submodels than .bsp!\n");
							model = NULL;
						}
						else
							model = loadmodel->brush.submodels[submodel];
					}
					else
						model = loadmodel;
				}
				for (;;)
				{
					if (!COM_ParseTokenConsole(&data))
						return; // error
					if (com_token[0] == '}')
						break; // end of brush
					// each brush face should be this format:
					// ( x y z ) ( x y z ) ( x y z ) texture scroll_s scroll_t rotateangle scale_s scale_t
					// FIXME: support hl .map format
					for (pointnum = 0;pointnum < 3;pointnum++)
					{
						COM_ParseTokenConsole(&data);
						for (componentnum = 0;componentnum < 3;componentnum++)
						{
							COM_ParseTokenConsole(&data);
							point[pointnum][componentnum] = atof(com_token);
						}
						COM_ParseTokenConsole(&data);
					}
					COM_ParseTokenConsole(&data);
					strlcpy(facetexture, com_token, sizeof(facetexture));
					COM_ParseTokenConsole(&data);
					//scroll_s = atof(com_token);
					COM_ParseTokenConsole(&data);
					//scroll_t = atof(com_token);
					COM_ParseTokenConsole(&data);
					//rotate = atof(com_token);
					COM_ParseTokenConsole(&data);
					//scale_s = atof(com_token);
					COM_ParseTokenConsole(&data);
					//scale_t = atof(com_token);
					TriangleNormal(point[0], point[1], point[2], planenormal);
					VectorNormalizeDouble(planenormal);
					planedist = DotProduct(point[0], planenormal);
					//ChooseTexturePlane(planenormal, texturevector[0], texturevector[1]);
				}
				continue;
			}
		}
	}
#endif
}


#define MAX_PORTALPOINTS 64

typedef struct portal_s
{
	mplane_t plane;
	mnode_t *nodes[2];		// [0] = front side of plane
	struct portal_s *next[2];
	int numpoints;
	double points[3*MAX_PORTALPOINTS];
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
	p = (portal_t *)Mem_Alloc(loadmodel->mempool, sizeof(portal_t));
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
	// process only nodes (leafs already had their box calculated)
	if (!node->plane)
		return;

	// calculate children first
	Mod_Q1BSP_RecursiveRecalcNodeBBox(node->children[0]);
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

	// tally up portal and point counts and recalculate bounding boxes for all
	// leafs (because qbsp is very sloppy)
	leaf = loadmodel->brush.data_leafs;
	endleaf = leaf + loadmodel->brush.num_leafs;
	for (;leaf < endleaf;leaf++)
	{
		VectorSet(leaf->mins,  2000000000,  2000000000,  2000000000);
		VectorSet(leaf->maxs, -2000000000, -2000000000, -2000000000);
	}
	p = portalchain;
	numportals = 0;
	numpoints = 0;
	while (p)
	{
		// note: this check must match the one below or it will usually corrupt memory
		// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
		if (p->numpoints >= 3 && p->nodes[0] != p->nodes[1] && ((mleaf_t *)p->nodes[0])->clusterindex >= 0 && ((mleaf_t *)p->nodes[1])->clusterindex >= 0)
		{
			numportals += 2;
			numpoints += p->numpoints * 2;
		}
		p = p->chain;
	}
	loadmodel->brush.data_portals = (mportal_t *)Mem_Alloc(loadmodel->mempool, numportals * sizeof(mportal_t) + numpoints * sizeof(mvertex_t));
	loadmodel->brush.num_portals = numportals;
	loadmodel->brush.data_portalpoints = (mvertex_t *)((unsigned char *) loadmodel->brush.data_portals + numportals * sizeof(mportal_t));
	loadmodel->brush.num_portalpoints = numpoints;
	// clear all leaf portal chains
	for (i = 0;i < loadmodel->brush.num_leafs;i++)
		loadmodel->brush.data_leafs[i].portals = NULL;
	// process all portals in the global portal chain, while freeing them
	portal = loadmodel->brush.data_portals;
	point = loadmodel->brush.data_portalpoints;
	p = portalchain;
	portalchain = NULL;
	while (p)
	{
		pnext = p->chain;

		if (p->numpoints >= 3 && p->nodes[0] != p->nodes[1])
		{
			// note: this check must match the one above or it will usually corrupt memory
			// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
			if (((mleaf_t *)p->nodes[0])->clusterindex >= 0 && ((mleaf_t *)p->nodes[1])->clusterindex >= 0)
			{
				// first make the back to front portal(forward portal)
				portal->points = point;
				portal->numpoints = p->numpoints;
				portal->plane.dist = p->plane.dist;
				VectorCopy(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[1];
				portal->past = (mleaf_t *)p->nodes[0];
				// copy points
				for (j = 0;j < portal->numpoints;j++)
				{
					VectorCopy(p->points + j*3, point->position);
					point++;
				}
				BoxFromPoints(portal->mins, portal->maxs, portal->numpoints, portal->points->position);
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;

				// then make the front to back portal(backward portal)
				portal->points = point;
				portal->numpoints = p->numpoints;
				portal->plane.dist = -p->plane.dist;
				VectorNegate(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[0];
				portal->past = (mleaf_t *)p->nodes[1];
				// copy points
				for (j = portal->numpoints - 1;j >= 0;j--)
				{
					VectorCopy(p->points + j*3, point->position);
					point++;
				}
				BoxFromPoints(portal->mins, portal->maxs, portal->numpoints, portal->points->position);
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;
			}
			// add the portal's polygon points to the leaf bounding boxes
			for (i = 0;i < 2;i++)
			{
				leaf = (mleaf_t *)p->nodes[i];
				for (j = 0;j < p->numpoints;j++)
				{
					if (leaf->mins[0] > p->points[j*3+0]) leaf->mins[0] = p->points[j*3+0];
					if (leaf->mins[1] > p->points[j*3+1]) leaf->mins[1] = p->points[j*3+1];
					if (leaf->mins[2] > p->points[j*3+2]) leaf->mins[2] = p->points[j*3+2];
					if (leaf->maxs[0] < p->points[j*3+0]) leaf->maxs[0] = p->points[j*3+0];
					if (leaf->maxs[1] < p->points[j*3+1]) leaf->maxs[1] = p->points[j*3+1];
					if (leaf->maxs[2] < p->points[j*3+2]) leaf->maxs[2] = p->points[j*3+2];
				}
			}
		}
		FreePortal(p);
		p = pnext;
	}
	// now recalculate the node bounding boxes from the leafs
	Mod_Q1BSP_RecursiveRecalcNodeBBox(loadmodel->brush.data_nodes);
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
			t = (portal_t *)*portalpointer;
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

#define PORTAL_DIST_EPSILON (1.0 / 32.0)
static void Mod_Q1BSP_RecursiveNodePortals(mnode_t *node)
{
	int i, side;
	mnode_t *front, *back, *other_node;
	mplane_t clipplane, *plane;
	portal_t *portal, *nextportal, *nodeportal, *splitportal, *temp;
	int numfrontpoints, numbackpoints;
	double frontpoints[3*MAX_PORTALPOINTS], backpoints[3*MAX_PORTALPOINTS];

	// if a leaf, we're done
	if (!node->plane)
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

	// TODO: calculate node bounding boxes during recursion and calculate a maximum plane size accordingly to improve precision (as most maps do not need 1 billion unit plane polygons)
	PolygonD_QuadForPlane(nodeportal->points, nodeportal->plane.normal[0], nodeportal->plane.normal[1], nodeportal->plane.normal[2], nodeportal->plane.dist, 1024.0*1024.0*1024.0);
	nodeportal->numpoints = 4;
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

		for (i = 0;i < nodeportal->numpoints*3;i++)
			frontpoints[i] = nodeportal->points[i];
		PolygonD_Divide(nodeportal->numpoints, frontpoints, clipplane.normal[0], clipplane.normal[1], clipplane.normal[2], clipplane.dist, PORTAL_DIST_EPSILON, MAX_PORTALPOINTS, nodeportal->points, &nodeportal->numpoints, 0, NULL, NULL, NULL);
		if (nodeportal->numpoints <= 0 || nodeportal->numpoints >= MAX_PORTALPOINTS)
			break;
	}

	if (nodeportal->numpoints < 3)
	{
		Con_Print("Mod_Q1BSP_RecursiveNodePortals: WARNING: new portal was clipped away\n");
		nodeportal->numpoints = 0;
	}
	else if (nodeportal->numpoints >= MAX_PORTALPOINTS)
	{
		Con_Print("Mod_Q1BSP_RecursiveNodePortals: WARNING: new portal has too many points\n");
		nodeportal->numpoints = 0;
	}

	AddPortalToNodes(nodeportal, front, back);

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
		if (!portal->numpoints)
			continue;

		other_node = portal->nodes[!side];
		RemovePortalFromNodes(portal);

		// cut the portal into two portals, one on each side of the node plane
		PolygonD_Divide(portal->numpoints, portal->points, plane->normal[0], plane->normal[1], plane->normal[2], plane->dist, PORTAL_DIST_EPSILON, MAX_PORTALPOINTS, frontpoints, &numfrontpoints, MAX_PORTALPOINTS, backpoints, &numbackpoints, NULL);

		if (!numfrontpoints)
		{
			if (side == 0)
				AddPortalToNodes(portal, back, other_node);
			else
				AddPortalToNodes(portal, other_node, back);
			continue;
		}
		if (!numbackpoints)
		{
			if (side == 0)
				AddPortalToNodes(portal, front, other_node);
			else
				AddPortalToNodes(portal, other_node, front);
			continue;
		}

		// the portal is split
		splitportal = AllocPortal();
		temp = splitportal->chain;
		*splitportal = *portal;
		splitportal->chain = temp;
		for (i = 0;i < numbackpoints*3;i++)
			splitportal->points[i] = backpoints[i];
		splitportal->numpoints = numbackpoints;
		for (i = 0;i < numfrontpoints*3;i++)
			portal->points[i] = frontpoints[i];
		portal->numpoints = numfrontpoints;

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
	Mod_Q1BSP_RecursiveNodePortals(loadmodel->brush.data_nodes);
	Mod_Q1BSP_FinalizePortals();
}

static void Mod_Q1BSP_BuildLightmapUpdateChains(mempool_t *mempool, model_t *model)
{
	int i, j, stylecounts[256], totalcount, remapstyles[256];
	msurface_t *surface;
	memset(stylecounts, 0, sizeof(stylecounts));
	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surface = model->data_surfaces + model->firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			stylecounts[surface->lightmapinfo->styles[j]]++;
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
	model->brushq1.light_style = (unsigned char *)Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(unsigned char));
	model->brushq1.light_stylevalue = (int *)Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(int));
	model->brushq1.light_styleupdatechains = (msurface_t ***)Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(msurface_t **));
	model->brushq1.light_styleupdatechainsbuffer = (msurface_t **)Mem_Alloc(mempool, totalcount * sizeof(msurface_t *));
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
	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surface = model->data_surfaces + model->firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			if (surface->lightmapinfo->styles[j] != 255)
				*model->brushq1.light_styleupdatechains[remapstyles[surface->lightmapinfo->styles[j]]]++ = surface;
	}
	j = 0;
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		*model->brushq1.light_styleupdatechains[i] = NULL;
		model->brushq1.light_styleupdatechains[i] = model->brushq1.light_styleupdatechainsbuffer + j;
		j += stylecounts[model->brushq1.light_style[i]] + 1;
	}
}

//Returns PVS data for a given point
//(note: can return NULL)
static unsigned char *Mod_Q1BSP_GetPVS(model_t *model, const vec3_t p)
{
	mnode_t *node;
	node = model->brush.data_nodes;
	while (node->plane)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];
	if (((mleaf_t *)node)->clusterindex >= 0)
		return model->brush.data_pvsclusters + ((mleaf_t *)node)->clusterindex * model->brush.num_pvsclusterbytes;
	else
		return NULL;
}

static void Mod_Q1BSP_FatPVS_RecursiveBSPNode(model_t *model, const vec3_t org, vec_t radius, unsigned char *pvsbuffer, int pvsbytes, mnode_t *node)
{
	while (node->plane)
	{
		float d = PlaneDiff(org, node->plane);
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
	// if this leaf is in a cluster, accumulate the pvs bits
	if (((mleaf_t *)node)->clusterindex >= 0)
	{
		int i;
		unsigned char *pvs = model->brush.data_pvsclusters + ((mleaf_t *)node)->clusterindex * model->brush.num_pvsclusterbytes;
		for (i = 0;i < pvsbytes;i++)
			pvsbuffer[i] |= pvs[i];
	}
}

//Calculates a PVS that is the inclusive or of all leafs within radius pixels
//of the given point.
static int Mod_Q1BSP_FatPVS(model_t *model, const vec3_t org, vec_t radius, unsigned char *pvsbuffer, int pvsbufferlength)
{
	int bytes = model->brush.num_pvsclusterbytes;
	bytes = min(bytes, pvsbufferlength);
	if (r_novis.integer || !model->brush.num_pvsclusters || !Mod_Q1BSP_GetPVS(model, org))
	{
		memset(pvsbuffer, 0xFF, bytes);
		return bytes;
	}
	memset(pvsbuffer, 0, bytes);
	Mod_Q1BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, bytes, model->brush.data_nodes);
	return bytes;
}

static void Mod_Q1BSP_RoundUpToHullSize(model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs)
{
	vec3_t size;
	const hull_t *hull;

	VectorSubtract(inmaxs, inmins, size);
	if (cmodel->brush.ismcbsp)
	{
		if (size[0] < 3)
			hull = &cmodel->brushq1.hulls[0]; // 0x0x0
		else if (size[2] < 48) // pick the nearest of 40 or 56
			hull = &cmodel->brushq1.hulls[2]; // 16x16x40
		else
			hull = &cmodel->brushq1.hulls[1]; // 16x16x56
	}
	else if (cmodel->brush.ishlbsp)
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

void Mod_Q1BSP_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, k;
	dheader_t *header;
	dmodel_t *bm;
	mempool_t *mainmempool;
	float dist, modelyawradius, modelradius, *vec;
	msurface_t *surface;
	int numshadowmeshtriangles;
	dheader_t _header;
	hullinfo_t hullinfo;

	mod->type = mod_brushq1;

	if (!memcmp (buffer, "MCBSPpad", 8))
	{
		unsigned char	*index;

		mod->brush.ismcbsp = true;
		mod->brush.ishlbsp = false;

		mod_base = (unsigned char*)buffer;

		index = mod_base;
		index += 8;
		i = SB_ReadInt (&index);
		if (i != MCBSPVERSION)
			Host_Error("Mod_Q1BSP_Load: %s has wrong version number(%i should be %i)", mod->name, i, MCBSPVERSION);

	// read hull info
		hullinfo.numhulls = LittleLong(*(int*)index); index += 4;
		hullinfo.filehulls = hullinfo.numhulls;
		VectorClear (hullinfo.hullsizes[0][0]);
		VectorClear (hullinfo.hullsizes[0][1]);
		for (i = 1; i < hullinfo.numhulls; i++)
		{
			hullinfo.hullsizes[i][0][0] = SB_ReadFloat (&index);
			hullinfo.hullsizes[i][0][1] = SB_ReadFloat (&index);
			hullinfo.hullsizes[i][0][2] = SB_ReadFloat (&index);
			hullinfo.hullsizes[i][1][0] = SB_ReadFloat (&index);
			hullinfo.hullsizes[i][1][1] = SB_ReadFloat (&index);
			hullinfo.hullsizes[i][1][2] = SB_ReadFloat (&index);
		}

	// read lumps
		_header.version = 0;
		for (i = 0; i < HEADER_LUMPS; i++)
		{
			_header.lumps[i].fileofs = SB_ReadInt (&index);
			_header.lumps[i].filelen = SB_ReadInt (&index);
		}

		header = &_header;
	}
	else
	{
		header = (dheader_t *)buffer;

		i = LittleLong(header->version);
		if (i != BSPVERSION && i != 30)
			Host_Error("Mod_Q1BSP_Load: %s has wrong version number(%i should be %i(Quake) or 30(HalfLife)", mod->name, i, BSPVERSION);
		mod->brush.ishlbsp = i == 30;
		mod->brush.ismcbsp = false;

	// fill in hull info
		VectorClear (hullinfo.hullsizes[0][0]);
		VectorClear (hullinfo.hullsizes[0][1]);
		if (mod->brush.ishlbsp)
		{
			hullinfo.numhulls = 4;
			hullinfo.filehulls = 4;
			VectorSet (hullinfo.hullsizes[1][0], -16, -16, -36);
			VectorSet (hullinfo.hullsizes[1][1], 16, 16, 36);
			VectorSet (hullinfo.hullsizes[2][0], -32, -32, -32);
			VectorSet (hullinfo.hullsizes[2][1], 32, 32, 32);
			VectorSet (hullinfo.hullsizes[3][0], -16, -16, -18);
			VectorSet (hullinfo.hullsizes[3][1], 16, 16, 18);
		}
		else
		{
			hullinfo.numhulls = 3;
			hullinfo.filehulls = 4;
			VectorSet (hullinfo.hullsizes[1][0], -16, -16, -24);
			VectorSet (hullinfo.hullsizes[1][1], 16, 16, 32);
			VectorSet (hullinfo.hullsizes[2][0], -32, -32, -24);
			VectorSet (hullinfo.hullsizes[2][1], 32, 32, 64);
		}

	// read lumps
		mod_base = (unsigned char*)buffer;
		for (i = 0; i < HEADER_LUMPS; i++)
		{
			header->lumps[i].fileofs = LittleLong(header->lumps[i].fileofs);
			header->lumps[i].filelen = LittleLong(header->lumps[i].filelen);
		}
	}

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q1BSP_TraceBox;
	mod->brush.TraceLineOfSight = Mod_Q1BSP_TraceLineOfSight;
	mod->brush.SuperContentsFromNativeContents = Mod_Q1BSP_SuperContentsFromNativeContents;
	mod->brush.NativeContentsFromSuperContents = Mod_Q1BSP_NativeContentsFromSuperContents;
	mod->brush.GetPVS = Mod_Q1BSP_GetPVS;
	mod->brush.FatPVS = Mod_Q1BSP_FatPVS;
	mod->brush.BoxTouchingPVS = Mod_Q1BSP_BoxTouchingPVS;
	mod->brush.BoxTouchingLeafPVS = Mod_Q1BSP_BoxTouchingLeafPVS;
	mod->brush.BoxTouchingVisibleLeafs = Mod_Q1BSP_BoxTouchingVisibleLeafs;
	mod->brush.FindBoxClusters = Mod_Q1BSP_FindBoxClusters;
	mod->brush.LightPoint = Mod_Q1BSP_LightPoint;
	mod->brush.FindNonSolidLocation = Mod_Q1BSP_FindNonSolidLocation;
	mod->brush.AmbientSoundLevelsForPoint = Mod_Q1BSP_AmbientSoundLevelsForPoint;
	mod->brush.RoundUpToHullSize = Mod_Q1BSP_RoundUpToHullSize;
	mod->brush.PointInLeaf = Mod_Q1BSP_PointInLeaf;

	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brush.ishlbsp);
		Cvar_SetValue("mcbsp", mod->brush.ismcbsp);
	}

// load into heap

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	mod->brush.qw_md4sum = 0;
	mod->brush.qw_md4sum2 = 0;
	for (i = 0;i < HEADER_LUMPS;i++)
	{
		if (i == LUMP_ENTITIES)
			continue;
		mod->brush.qw_md4sum ^= LittleLong(Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen));
		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		mod->brush.qw_md4sum2 ^= LittleLong(Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen));
	}

	Mod_Q1BSP_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_Q1BSP_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_Q1BSP_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_Q1BSP_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_Q1BSP_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_Q1BSP_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_Q1BSP_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_Q1BSP_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_Q1BSP_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_Q1BSP_LoadLeaffaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_Q1BSP_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	// load submodels before leafs because they contain the number of vis leafs
	Mod_Q1BSP_LoadSubmodels(&header->lumps[LUMP_MODELS], &hullinfo);
	Mod_Q1BSP_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_Q1BSP_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_Q1BSP_LoadClipnodes(&header->lumps[LUMP_CLIPNODES], &hullinfo);

	// check if the map supports transparent water rendering
	loadmodel->brush.supportwateralpha = Mod_Q1BSP_CheckWaterAlphaSupport();

	if (!mod->brushq1.lightdata)
		mod->brush.LightPoint = NULL;

	if (mod->brushq1.data_compressedpvs)
		Mem_Free(mod->brushq1.data_compressedpvs);
	mod->brushq1.data_compressedpvs = NULL;
	mod->brushq1.num_compressedpvs = 0;

	Mod_Q1BSP_MakeHull0();
	Mod_Q1BSP_MakePortals();

	mod->numframes = 2;		// regular and alternate animation
	mod->numskins = 1;

	mainmempool = mod->mempool;

	// make a single combined shadow mesh to allow optimized shadow volume creation
	numshadowmeshtriangles = 0;
	for (j = 0, surface = loadmodel->data_surfaces;j < loadmodel->num_surfaces;j++, surface++)
	{
		surface->num_firstshadowmeshtriangle = numshadowmeshtriangles;
		numshadowmeshtriangles += surface->num_triangles;
	}
	loadmodel->brush.shadowmesh = Mod_ShadowMesh_Begin(loadmodel->mempool, numshadowmeshtriangles * 3, numshadowmeshtriangles, NULL, NULL, NULL, false, false, true);
	for (j = 0, surface = loadmodel->data_surfaces;j < loadmodel->num_surfaces;j++, surface++)
		Mod_ShadowMesh_AddMesh(loadmodel->mempool, loadmodel->brush.shadowmesh, NULL, NULL, NULL, loadmodel->surfmesh.data_vertex3f, NULL, NULL, NULL, NULL, surface->num_triangles, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
	loadmodel->brush.shadowmesh = Mod_ShadowMesh_Finish(loadmodel->mempool, loadmodel->brush.shadowmesh, false, true);
	Mod_BuildTriangleNeighbors(loadmodel->brush.shadowmesh->neighbor3i, loadmodel->brush.shadowmesh->element3i, loadmodel->brush.shadowmesh->numtriangles);

	if (loadmodel->brush.numsubmodels)
		loadmodel->brush.submodels = (model_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brush.numsubmodels * sizeof(model_t *));

	if (loadmodel->isworldmodel)
	{
		// clear out any stale submodels or worldmodels lying around
		// if we did this clear before now, an error might abort loading and
		// leave things in a bad state
		Mod_RemoveStaleWorldModels(loadmodel);
	}

	// LordHavoc: to clear the fog around the original quake submodel code, I
	// will explain:
	// first of all, some background info on the submodels:
	// model 0 is the map model (the world, named maps/e1m1.bsp for example)
	// model 1 and higher are submodels (doors and the like, named *1, *2, etc)
	// now the weird for loop itself:
	// the loop functions in an odd way, on each iteration it sets up the
	// current 'mod' model (which despite the confusing code IS the model of
	// the number i), at the end of the loop it duplicates the model to become
	// the next submodel, and loops back to set up the new submodel.

	// LordHavoc: now the explanation of my sane way (which works identically):
	// set up the world model, then on each submodel copy from the world model
	// and set up the submodel with the respective model info.
	for (i = 0;i < mod->brush.numsubmodels;i++)
	{
		// LordHavoc: this code was originally at the end of this loop, but
		// has been transformed to something more readable at the start here.

		if (i > 0)
		{
			char name[10];
			// LordHavoc: only register submodels if it is the world
			// (prevents external bsp models from replacing world submodels with
			//  their own)
			if (!loadmodel->isworldmodel)
				continue;
			// duplicate the basic information
			sprintf(name, "*%i", i);
			mod = Mod_FindName(name);
			// copy the base model to this one
			*mod = *loadmodel;
			// rename the clone back to its proper name
			strlcpy(mod->name, name, sizeof(mod->name));
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
		}

		mod->brush.submodel = i;

		if (loadmodel->brush.submodels)
			loadmodel->brush.submodels[i] = mod;

		bm = &mod->brushq1.submodels[i];

		mod->brushq1.hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->brushq1.hulls[j].firstclipnode = bm->headnode[j];
			mod->brushq1.hulls[j].lastclipnode = mod->brushq1.numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		// make the model surface list (used by shadowing/lighting)
		mod->surfacelist = (int *)Mem_Alloc(loadmodel->mempool, mod->nummodelsurfaces * sizeof(*mod->surfacelist));
		for (j = 0;j < mod->nummodelsurfaces;j++)
			mod->surfacelist[j] = mod->firstmodelsurface + j;

		// this gets altered below if sky is used
		mod->DrawSky = NULL;
		mod->Draw = R_Q1BSP_Draw;
		mod->GetLightInfo = R_Q1BSP_GetLightInfo;
		mod->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
		mod->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
		mod->DrawLight = R_Q1BSP_DrawLight;
		if (i != 0)
		{
			mod->brush.TraceLineOfSight = NULL;
			mod->brush.GetPVS = NULL;
			mod->brush.FatPVS = NULL;
			mod->brush.BoxTouchingPVS = NULL;
			mod->brush.BoxTouchingLeafPVS = NULL;
			mod->brush.BoxTouchingVisibleLeafs = NULL;
			mod->brush.FindBoxClusters = NULL;
			mod->brush.LightPoint = NULL;
			mod->brush.AmbientSoundLevelsForPoint = NULL;
		}
		Mod_Q1BSP_BuildLightmapUpdateChains(loadmodel->mempool, mod);
		if (mod->nummodelsurfaces)
		{
			// LordHavoc: calculate bmodel bounding box rather than trusting what it says
			mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
			mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
			modelyawradius = 0;
			modelradius = 0;
			for (j = 0, surface = &mod->data_surfaces[mod->firstmodelsurface];j < mod->nummodelsurfaces;j++, surface++)
			{
				// we only need to have a drawsky function if it is used(usually only on world model)
				if (surface->texture->basematerialflags & MATERIALFLAG_SKY)
					mod->DrawSky = R_Q1BSP_DrawSky;
				// calculate bounding shapes
				for (k = 0, vec = (loadmodel->surfmesh.data_vertex3f + 3 * surface->num_firstvertex);k < surface->num_vertices;k++, vec += 3)
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
			Con_Printf("warning: empty submodel *%i in %s\n", i+1, loadmodel->name);
		}
		//mod->brushq1.num_visleafs = bm->visleafs;
	}

	Mod_Q1BSP_LoadMapBrushes();

	//Mod_Q1BSP_ProcessLightList();

	if (developer.integer >= 10)
		Con_Printf("Some stats for q1bsp model \"%s\": %i faces, %i nodes, %i leafs, %i visleafs, %i visleafportals\n", loadmodel->name, loadmodel->num_surfaces, loadmodel->brush.num_nodes, loadmodel->brush.num_leafs, mod->brush.num_pvsclusters, loadmodel->brush.num_portals);
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

void static Mod_Q2BSP_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i;
	q2dheader_t *header;

	Host_Error("Mod_Q2BSP_Load: not yet implemented");

	mod->type = mod_brushq2;

	header = (q2dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q2BSPVERSION)
		Host_Error("Mod_Q2BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q2BSPVERSION);
	mod->brush.ishlbsp = false;
	mod->brush.ismcbsp = false;
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brush.ishlbsp);
		Cvar_SetValue("mcbsp", mod->brush.ismcbsp);
	}

	mod_base = (unsigned char *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	mod->brush.qw_md4sum = 0;
	mod->brush.qw_md4sum2 = 0;
	for (i = 0;i < Q2HEADER_LUMPS;i++)
	{
		if (i == Q2LUMP_ENTITIES)
			continue;
		mod->brush.qw_md4sum ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		if (i == Q2LUMP_VISIBILITY || i == Q2LUMP_LEAFS || i == Q2LUMP_NODES)
			continue;
		mod->brush.qw_md4sum2 ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
	}

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
	char key[128], value[MAX_INPUTLINE];
	float v[3];
	loadmodel->brushq3.num_lightgrid_cellsize[0] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[1] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[2] = 128;
	if (!l->filelen)
		return;
	loadmodel->brush.entities = (char *)Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	data = loadmodel->brush.entities;
	// some Q3 maps override the lightgrid_cellsize with a worldspawn key
	if (data && COM_ParseTokenConsole(&data) && com_token[0] == '{')
	{
		while (1)
		{
			if (!COM_ParseTokenConsole(&data))
				break; // error
			if (com_token[0] == '}')
				break; // end of worldspawn
			if (com_token[0] == '_')
				strlcpy(key, com_token + 1, sizeof(key));
			else
				strlcpy(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseTokenConsole(&data))
				break; // error
			strlcpy(value, com_token, sizeof(value));
			if (!strcmp("gridsize", key))
			{
				if (sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]) == 3 && v[0] != 0 && v[1] != 0 && v[2] != 0)
					VectorCopy(v, loadmodel->brushq3.num_lightgrid_cellsize);
			}
		}
	}
}

// FIXME: make MAXSHADERS dynamic
#define Q3SHADER_MAXSHADERS 4096
#define Q3SHADER_MAXLAYERS 8

typedef struct q3shaderinfo_layer_s
{
	int alphatest;
	int clampmap;
	float framerate;
	int numframes;
	char texturename[TEXTURE_MAXFRAMES][Q3PATHLENGTH];
	int blendfunc[2];
	qboolean rgbgenvertex;
	qboolean alphagenvertex;
}
q3shaderinfo_layer_t;

typedef struct q3shaderinfo_s
{
	char name[Q3PATHLENGTH];
	int surfaceparms;
	int textureflags;
	int numlayers;
	qboolean lighting;
	qboolean vertexalpha;
	qboolean textureblendalpha;
	q3shaderinfo_layer_t *primarylayer, *backgroundlayer;
	q3shaderinfo_layer_t layers[Q3SHADER_MAXLAYERS];
	char skyboxname[Q3PATHLENGTH];
}
q3shaderinfo_t;

int q3shaders_numshaders;
q3shaderinfo_t q3shaders_shaders[Q3SHADER_MAXSHADERS];

static void Mod_Q3BSP_LoadShaders(void)
{
	int j;
	int fileindex;
	fssearch_t *search;
	char *f;
	const char *text;
	q3shaderinfo_t *shader;
	q3shaderinfo_layer_t *layer;
	int numparameters;
	char parameter[TEXTURE_MAXFRAMES + 4][Q3PATHLENGTH];
	search = FS_Search("scripts/*.shader", true, false);
	if (!search)
		return;
	q3shaders_numshaders = 0;
	for (fileindex = 0;fileindex < search->numfilenames;fileindex++)
	{
		text = f = (char *)FS_LoadFile(search->filenames[fileindex], tempmempool, false, NULL);
		if (!f)
			continue;
		while (COM_ParseToken(&text, false))
		{
			if (q3shaders_numshaders >= Q3SHADER_MAXSHADERS)
			{
				Con_Printf("Mod_Q3BSP_LoadShaders: too many shaders!\n");
				break;
			}
			shader = q3shaders_shaders + q3shaders_numshaders++;
			memset(shader, 0, sizeof(*shader));
			strlcpy(shader->name, com_token, sizeof(shader->name));
			if (!COM_ParseToken(&text, false) || strcasecmp(com_token, "{"))
			{
				Con_Printf("%s parsing error - expected \"{\", found \"%s\"\n", search->filenames[fileindex], com_token);
				break;
			}
			while (COM_ParseToken(&text, false))
			{
				if (!strcasecmp(com_token, "}"))
					break;
				if (!strcasecmp(com_token, "{"))
				{
					if (shader->numlayers < Q3SHADER_MAXLAYERS)
					{
						layer = shader->layers + shader->numlayers++;
						layer->rgbgenvertex = false;
						layer->alphagenvertex = false;
						layer->blendfunc[0] = GL_ONE;
						layer->blendfunc[1] = GL_ZERO;
					}
					else
						layer = NULL;
					while (COM_ParseToken(&text, false))
					{
						if (!strcasecmp(com_token, "}"))
							break;
						if (!strcasecmp(com_token, "\n"))
							continue;
						if (layer == NULL)
							continue;
						numparameters = 0;
						for (j = 0;strcasecmp(com_token, "\n") && strcasecmp(com_token, "}");j++)
						{
							if (j < TEXTURE_MAXFRAMES + 4)
							{
								strlcpy(parameter[j], com_token, sizeof(parameter[j]));
								numparameters = j + 1;
							}
							if (!COM_ParseToken(&text, true))
								break;
						}
						if (developer.integer >= 100)
						{
							Con_Printf("%s %i: ", shader->name, shader->numlayers - 1);
							for (j = 0;j < numparameters;j++)
								Con_Printf(" %s", parameter[j]);
							Con_Print("\n");
						}
						if (numparameters >= 2 && !strcasecmp(parameter[0], "blendfunc"))
						{
							if (numparameters == 2)
							{
								if (!strcasecmp(parameter[1], "add"))
								{
									layer->blendfunc[0] = GL_ONE;
									layer->blendfunc[1] = GL_ONE;
								}
								else if (!strcasecmp(parameter[1], "filter"))
								{
									layer->blendfunc[0] = GL_DST_COLOR;
									layer->blendfunc[1] = GL_ZERO;
								}
								else if (!strcasecmp(parameter[1], "blend"))
								{
									layer->blendfunc[0] = GL_SRC_ALPHA;
									layer->blendfunc[1] = GL_ONE_MINUS_SRC_ALPHA;
								}
							}
							else if (numparameters == 3)
							{
								int k;
								for (k = 0;k < 2;k++)
								{
									if (!strcasecmp(parameter[k+1], "GL_ONE"))
										layer->blendfunc[k] = GL_ONE;
									else if (!strcasecmp(parameter[k+1], "GL_ZERO"))
										layer->blendfunc[k] = GL_ZERO;
									else if (!strcasecmp(parameter[k+1], "GL_SRC_COLOR"))
										layer->blendfunc[k] = GL_SRC_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_SRC_ALPHA"))
										layer->blendfunc[k] = GL_SRC_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_DST_COLOR"))
										layer->blendfunc[k] = GL_DST_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_DST_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_SRC_COLOR"))
										layer->blendfunc[k] = GL_ONE_MINUS_SRC_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_SRC_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_SRC_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_DST_COLOR"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_DST_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_ALPHA;
									else
										layer->blendfunc[k] = GL_ONE; // default in case of parsing error
								}
							}
						}
						if (numparameters >= 2 && !strcasecmp(parameter[0], "alphafunc"))
							layer->alphatest = true;
						if (numparameters >= 2 && (!strcasecmp(parameter[0], "map") || !strcasecmp(parameter[0], "clampmap")))
						{
							if (!strcasecmp(parameter[0], "clampmap"))
								layer->clampmap = true;
							layer->numframes = 1;
							layer->framerate = 1;
							strlcpy(layer->texturename[0], parameter[1], sizeof(layer->texturename));
							if (!strcasecmp(parameter[1], "$lightmap"))
								shader->lighting = true;
						}
						else if (numparameters >= 3 && (!strcasecmp(parameter[0], "animmap") || !strcasecmp(parameter[0], "animclampmap")))
						{
							int i;
							layer->numframes = min(numparameters - 2, TEXTURE_MAXFRAMES);
							layer->framerate = atof(parameter[1]);
							for (i = 0;i < layer->numframes;i++)
								strlcpy(layer->texturename[i], parameter[i + 2], sizeof(layer->texturename));
						}
						else if (numparameters >= 2 && !strcasecmp(parameter[0], "rgbgen") && !strcasecmp(parameter[1], "vertex"))
							layer->rgbgenvertex = true;
						else if (numparameters >= 2 && !strcasecmp(parameter[0], "alphagen") && !strcasecmp(parameter[1], "vertex"))
							layer->alphagenvertex = true;
						// break out a level if it was }
						if (!strcasecmp(com_token, "}"))
							break;
					}
					if (layer->rgbgenvertex)
						shader->lighting = true;
					if (layer->alphagenvertex)
					{
						if (layer == shader->layers + 0)
						{
							// vertex controlled transparency
							shader->vertexalpha = true;
						}
						else
						{
							// multilayer terrain shader or similar
							shader->textureblendalpha = true;
						}
					}
					continue;
				}
				numparameters = 0;
				for (j = 0;strcasecmp(com_token, "\n") && strcasecmp(com_token, "}");j++)
				{
					if (j < TEXTURE_MAXFRAMES + 4)
					{
						strlcpy(parameter[j], com_token, sizeof(parameter[j]));
						numparameters = j + 1;
					}
					if (!COM_ParseToken(&text, true))
						break;
				}
				if (fileindex == 0 && !strcasecmp(com_token, "}"))
					break;
				if (developer.integer >= 100)
				{
					Con_Printf("%s: ", shader->name);
					for (j = 0;j < numparameters;j++)
						Con_Printf(" %s", parameter[j]);
					Con_Print("\n");
				}
				if (numparameters < 1)
					continue;
				if (!strcasecmp(parameter[0], "surfaceparm") && numparameters >= 2)
				{
					if (!strcasecmp(parameter[1], "alphashadow"))
						shader->surfaceparms |= Q3SURFACEPARM_ALPHASHADOW;
					else if (!strcasecmp(parameter[1], "areaportal"))
						shader->surfaceparms |= Q3SURFACEPARM_AREAPORTAL;
					else if (!strcasecmp(parameter[1], "botclip"))
						shader->surfaceparms |= Q3SURFACEPARM_BOTCLIP;
					else if (!strcasecmp(parameter[1], "clusterportal"))
						shader->surfaceparms |= Q3SURFACEPARM_CLUSTERPORTAL;
					else if (!strcasecmp(parameter[1], "detail"))
						shader->surfaceparms |= Q3SURFACEPARM_DETAIL;
					else if (!strcasecmp(parameter[1], "donotenter"))
						shader->surfaceparms |= Q3SURFACEPARM_DONOTENTER;
					else if (!strcasecmp(parameter[1], "dust"))
						shader->surfaceparms |= Q3SURFACEPARM_DUST;
					else if (!strcasecmp(parameter[1], "hint"))
						shader->surfaceparms |= Q3SURFACEPARM_HINT;
					else if (!strcasecmp(parameter[1], "fog"))
						shader->surfaceparms |= Q3SURFACEPARM_FOG;
					else if (!strcasecmp(parameter[1], "lava"))
						shader->surfaceparms |= Q3SURFACEPARM_LAVA;
					else if (!strcasecmp(parameter[1], "lightfilter"))
						shader->surfaceparms |= Q3SURFACEPARM_LIGHTFILTER;
					else if (!strcasecmp(parameter[1], "lightgrid"))
						shader->surfaceparms |= Q3SURFACEPARM_LIGHTGRID;
					else if (!strcasecmp(parameter[1], "metalsteps"))
						shader->surfaceparms |= Q3SURFACEPARM_METALSTEPS;
					else if (!strcasecmp(parameter[1], "nodamage"))
						shader->surfaceparms |= Q3SURFACEPARM_NODAMAGE;
					else if (!strcasecmp(parameter[1], "nodlight"))
						shader->surfaceparms |= Q3SURFACEPARM_NODLIGHT;
					else if (!strcasecmp(parameter[1], "nodraw"))
						shader->surfaceparms |= Q3SURFACEPARM_NODRAW;
					else if (!strcasecmp(parameter[1], "nodrop"))
						shader->surfaceparms |= Q3SURFACEPARM_NODROP;
					else if (!strcasecmp(parameter[1], "noimpact"))
						shader->surfaceparms |= Q3SURFACEPARM_NOIMPACT;
					else if (!strcasecmp(parameter[1], "nolightmap"))
						shader->surfaceparms |= Q3SURFACEPARM_NOLIGHTMAP;
					else if (!strcasecmp(parameter[1], "nomarks"))
						shader->surfaceparms |= Q3SURFACEPARM_NOMARKS;
					else if (!strcasecmp(parameter[1], "nomipmaps"))
						shader->surfaceparms |= Q3SURFACEPARM_NOMIPMAPS;
					else if (!strcasecmp(parameter[1], "nonsolid"))
						shader->surfaceparms |= Q3SURFACEPARM_NONSOLID;
					else if (!strcasecmp(parameter[1], "origin"))
						shader->surfaceparms |= Q3SURFACEPARM_ORIGIN;
					else if (!strcasecmp(parameter[1], "playerclip"))
						shader->surfaceparms |= Q3SURFACEPARM_PLAYERCLIP;
					else if (!strcasecmp(parameter[1], "sky"))
						shader->surfaceparms |= Q3SURFACEPARM_SKY;
					else if (!strcasecmp(parameter[1], "slick"))
						shader->surfaceparms |= Q3SURFACEPARM_SLICK;
					else if (!strcasecmp(parameter[1], "slime"))
						shader->surfaceparms |= Q3SURFACEPARM_SLIME;
					else if (!strcasecmp(parameter[1], "structural"))
						shader->surfaceparms |= Q3SURFACEPARM_STRUCTURAL;
					else if (!strcasecmp(parameter[1], "trans"))
						shader->surfaceparms |= Q3SURFACEPARM_TRANS;
					else if (!strcasecmp(parameter[1], "water"))
						shader->surfaceparms |= Q3SURFACEPARM_WATER;
					else if (!strcasecmp(parameter[1], "pointlight"))
						shader->surfaceparms |= Q3SURFACEPARM_POINTLIGHT;
					else if (!strcasecmp(parameter[1], "antiportal"))
						shader->surfaceparms |= Q3SURFACEPARM_ANTIPORTAL;
					else
						Con_DPrintf("%s parsing warning: unknown surfaceparm \"%s\"\n", search->filenames[fileindex], parameter[1]);
				}
				else if (!strcasecmp(parameter[0], "sky") && numparameters >= 2)
				{
					// some q3 skies don't have the sky parm set
					shader->surfaceparms |= Q3SURFACEPARM_SKY;
					strlcpy(shader->skyboxname, parameter[1], sizeof(shader->skyboxname));
				}
				else if (!strcasecmp(parameter[0], "skyparms") && numparameters >= 2)
				{
					// some q3 skies don't have the sky parm set
					shader->surfaceparms |= Q3SURFACEPARM_SKY;
					if (!atoi(parameter[1]) && strcasecmp(parameter[1], "-"))
						strlcpy(shader->skyboxname, parameter[1], sizeof(shader->skyboxname));
				}
				else if (!strcasecmp(parameter[0], "cull") && numparameters >= 2)
				{
					if (!strcasecmp(parameter[1], "disable") || !strcasecmp(parameter[1], "none") || !strcasecmp(parameter[1], "twosided"))
						shader->textureflags |= Q3TEXTUREFLAG_TWOSIDED;
				}
				else if (!strcasecmp(parameter[0], "nomipmaps"))
					shader->surfaceparms |= Q3SURFACEPARM_NOMIPMAPS;
				else if (!strcasecmp(parameter[0], "nopicmip"))
					shader->textureflags |= Q3TEXTUREFLAG_NOPICMIP;
				else if (!strcasecmp(parameter[0], "deformvertexes") && numparameters >= 2)
				{
					if (!strcasecmp(parameter[1], "autosprite") && numparameters == 2)
						shader->textureflags |= Q3TEXTUREFLAG_AUTOSPRITE;
					if (!strcasecmp(parameter[1], "autosprite2") && numparameters == 2)
						shader->textureflags |= Q3TEXTUREFLAG_AUTOSPRITE2;
				}
			}
			// identify if this is a blended terrain shader or similar
			if (shader->numlayers)
			{
				shader->primarylayer = shader->layers + 0;
				if ((shader->layers[1].blendfunc[0] == GL_SRC_ALPHA && shader->layers[1].blendfunc[1] == GL_ONE_MINUS_SRC_ALPHA) || shader->layers[1].alphatest)
				{
					// terrain blending or other effects
					shader->backgroundlayer = shader->layers + 0;
					shader->primarylayer = shader->layers + 1;
				}
				// now see if the lightmap came first, and if so choose the second texture instead
				if (!strcasecmp(shader->primarylayer->texturename[0], "$lightmap"))
					shader->primarylayer = shader->layers + 1;
			}
		}
		Mem_Free(f);
	}
}

q3shaderinfo_t *Mod_Q3BSP_LookupShader(const char *name)
{
	int i;
	for (i = 0;i < Q3SHADER_MAXSHADERS;i++)
		if (!strcasecmp(q3shaders_shaders[i].name, name))
			return q3shaders_shaders + i;
	return NULL;
}

static void Mod_Q3BSP_LoadTextures(lump_t *l)
{
	q3dtexture_t *in;
	texture_t *out;
	int i, count, c;

	in = (q3dtexture_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadTextures: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (texture_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->data_textures = out;
	loadmodel->num_textures = count;

	// parse the Q3 shader files
	Mod_Q3BSP_LoadShaders();

	c = 0;
	for (i = 0;i < count;i++, in++, out++)
	{
		q3shaderinfo_t *shader;
		strlcpy (out->name, in->name, sizeof (out->name));
		out->surfaceflags = LittleLong(in->surfaceflags);
		out->supercontents = Mod_Q3BSP_SuperContentsFromNativeContents(loadmodel, LittleLong(in->contents));
		shader = Mod_Q3BSP_LookupShader(out->name);
		if (shader)
		{
			out->surfaceparms = shader->surfaceparms;
			out->textureflags = shader->textureflags;
			out->basematerialflags = 0;
			if (shader->surfaceparms & Q3SURFACEPARM_SKY)
			{
				out->basematerialflags |= MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
				if (shader->skyboxname[0])
				{
					// quake3 seems to append a _ to the skybox name, so this must do so as well
					dpsnprintf(loadmodel->brush.skybox, sizeof(loadmodel->brush.skybox), "%s_", shader->skyboxname);
				}
			}
			else if ((out->surfaceflags & Q3SURFACEFLAG_NODRAW) || shader->numlayers == 0)
				out->basematerialflags |= MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
			else if (shader->surfaceparms & Q3SURFACEPARM_LAVA)
				out->basematerialflags |= MATERIALFLAG_WATER | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOSHADOW;
			else if (shader->surfaceparms & Q3SURFACEPARM_SLIME)
				out->basematerialflags |= MATERIALFLAG_WATER | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_WATERALPHA | MATERIALFLAG_NOSHADOW;
			else if (shader->surfaceparms & Q3SURFACEPARM_WATER)
				out->basematerialflags |= MATERIALFLAG_WATER | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_WATERALPHA | MATERIALFLAG_NOSHADOW;
			else
				out->basematerialflags |= MATERIALFLAG_WALL;
			if (shader->layers[0].alphatest)
				out->basematerialflags |= MATERIALFLAG_ALPHATEST | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;
			if (shader->textureflags & (Q3TEXTUREFLAG_TWOSIDED | Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
				out->basematerialflags |= MATERIALFLAG_NOSHADOW;
			out->customblendfunc[0] = GL_ONE;
			out->customblendfunc[1] = GL_ZERO;
			if (shader->numlayers > 0)
			{
				out->customblendfunc[0] = shader->layers[0].blendfunc[0];
				out->customblendfunc[1] = shader->layers[0].blendfunc[1];
/*
Q3 shader blendfuncs actually used in the game (* = supported by DP)
* additive               GL_ONE GL_ONE
  additive weird         GL_ONE GL_SRC_ALPHA
  additive weird 2       GL_ONE GL_ONE_MINUS_SRC_ALPHA
* alpha                  GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
  alpha inverse          GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
  brighten               GL_DST_COLOR GL_ONE
  brighten               GL_ONE GL_SRC_COLOR
  brighten weird         GL_DST_COLOR GL_ONE_MINUS_DST_ALPHA
  brighten weird 2       GL_DST_COLOR GL_SRC_ALPHA
* modulate               GL_DST_COLOR GL_ZERO
* modulate               GL_ZERO GL_SRC_COLOR
  modulate inverse       GL_ZERO GL_ONE_MINUS_SRC_COLOR
  modulate inverse alpha GL_ZERO GL_SRC_ALPHA
  modulate weird inverse GL_ONE_MINUS_DST_COLOR GL_ZERO
* modulate x2            GL_DST_COLOR GL_SRC_COLOR
* no blend               GL_ONE GL_ZERO
  nothing                GL_ZERO GL_ONE
*/
				// if not opaque, figure out what blendfunc to use
				if (shader->layers[0].blendfunc[0] != GL_ONE || shader->layers[0].blendfunc[1] != GL_ZERO)
				{
					if (shader->layers[0].blendfunc[0] == GL_ONE && shader->layers[0].blendfunc[1] == GL_ONE)
						out->basematerialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;
					else if (shader->layers[0].blendfunc[0] == GL_SRC_ALPHA && shader->layers[0].blendfunc[1] == GL_ONE)
						out->basematerialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;
					else if (shader->layers[0].blendfunc[0] == GL_SRC_ALPHA && shader->layers[0].blendfunc[1] == GL_ONE_MINUS_SRC_ALPHA)
						out->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;
					else
						out->basematerialflags |= MATERIALFLAG_CUSTOMBLEND | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_NOSHADOW;
				}
			}
			if (!shader->lighting)
				out->basematerialflags |= MATERIALFLAG_FULLBRIGHT;
			if (shader->primarylayer && cls.state != ca_dedicated)
			{
				int j;
				out->numskinframes = shader->primarylayer->numframes;
				out->skinframerate = shader->primarylayer->framerate;
				for (j = 0;j < shader->primarylayer->numframes;j++)
					if (!Mod_LoadSkinFrame(&out->skinframes[j], shader->primarylayer->texturename[j], ((shader->surfaceparms & Q3SURFACEPARM_NOMIPMAPS) ? 0 : TEXF_MIPMAP) | TEXF_ALPHA | TEXF_PRECACHE | (shader->textureflags & Q3TEXTUREFLAG_NOPICMIP ? 0 : TEXF_PICMIP) | (shader->primarylayer->clampmap ? TEXF_CLAMP : 0), false, true))
						Con_Printf("%s: could not load texture \"%s\" (frame %i) for shader \"%s\"\n", loadmodel->name, shader->primarylayer->texturename[j], j, out->name);
			}
			if (shader->backgroundlayer && cls.state != ca_dedicated)
			{
				int j;
				out->backgroundnumskinframes = shader->backgroundlayer->numframes;
				out->backgroundskinframerate = shader->backgroundlayer->framerate;
				for (j = 0;j < shader->backgroundlayer->numframes;j++)
					if (!Mod_LoadSkinFrame(&out->backgroundskinframes[j], shader->backgroundlayer->texturename[j], ((shader->surfaceparms & Q3SURFACEPARM_NOMIPMAPS) ? 0 : TEXF_MIPMAP) | TEXF_ALPHA | TEXF_PRECACHE | (shader->textureflags & Q3TEXTUREFLAG_NOPICMIP ? 0 : TEXF_PICMIP) | (shader->backgroundlayer->clampmap ? TEXF_CLAMP : 0), false, true))
						Con_Printf("%s: could not load texture \"%s\" (frame %i) for shader \"%s\"\n", loadmodel->name, shader->backgroundlayer->texturename[j], j, out->name);
			}
		}
		else if (!strcmp(out->name, "noshader"))
			out->surfaceparms = 0;
		else
		{
			c++;
			Con_DPrintf("%s: No shader found for texture \"%s\"\n", loadmodel->name, out->name);
			out->surfaceparms = 0;
			if (out->surfaceflags & Q3SURFACEFLAG_NODRAW)
				out->basematerialflags |= MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
			else if (out->surfaceflags & Q3SURFACEFLAG_SKY)
				out->basematerialflags |= MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
			else
				out->basematerialflags |= MATERIALFLAG_WALL;
			// these are defaults
			//if (!strncmp(out->name, "textures/skies/", 15))
			//	out->surfaceparms |= Q3SURFACEPARM_SKY;
			//if (!strcmp(out->name, "caulk") || !strcmp(out->name, "common/caulk") || !strcmp(out->name, "textures/common/caulk")
			// || !strcmp(out->name, "nodraw") || !strcmp(out->name, "common/nodraw") || !strcmp(out->name, "textures/common/nodraw"))
			//	out->surfaceparms |= Q3SURFACEPARM_NODRAW;
			//if (R_TextureHasAlpha(out->skinframes[0].base))
			//	out->surfaceparms |= Q3SURFACEPARM_TRANS;
			if (cls.state != ca_dedicated)
				if (!Mod_LoadSkinFrame(&out->skinframes[0], out->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, false, true))
					Con_Printf("%s: could not load texture for missing shader \"%s\"\n", loadmodel->name, out->name);
		}
		// init the animation variables
		out->currentframe = out;
		out->currentskinframe = &out->skinframes[0];
		out->backgroundcurrentskinframe = &out->backgroundskinframes[0];
	}
	if (c)
		Con_DPrintf("%s: %i textures missing shaders\n", loadmodel->name, c);
}

static void Mod_Q3BSP_LoadPlanes(lump_t *l)
{
	q3dplane_t *in;
	mplane_t *out;
	int i, count;

	in = (q3dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mplane_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_planes = out;
	loadmodel->brush.num_planes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
		out->dist = LittleFloat(in->dist);
		PlaneClassify(out);
	}
}

static void Mod_Q3BSP_LoadBrushSides(lump_t *l)
{
	q3dbrushside_t *in;
	q3mbrushside_t *out;
	int i, n, count;

	in = (q3dbrushside_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushSides: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (q3mbrushside_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_brushsides = out;
	loadmodel->brush.num_brushsides = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brush.num_planes)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid planeindex %i (%i planes)", n, loadmodel->brush.num_planes);
		out->plane = loadmodel->brush.data_planes + n;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid textureindex %i (%i textures)", n, loadmodel->num_textures);
		out->texture = loadmodel->data_textures + n;
	}
}

static void Mod_Q3BSP_LoadBrushes(lump_t *l)
{
	q3dbrush_t *in;
	q3mbrush_t *out;
	int i, j, n, c, count, maxplanes;
	colplanef_t *planes;

	in = (q3dbrush_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (q3mbrush_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_brushes = out;
	loadmodel->brush.num_brushes = count;

	maxplanes = 0;
	planes = NULL;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->firstbrushside);
		c = LittleLong(in->numbrushsides);
		if (n < 0 || n + c > loadmodel->brush.num_brushsides)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid brushside range %i : %i (%i brushsides)", n, n + c, loadmodel->brush.num_brushsides);
		out->firstbrushside = loadmodel->brush.data_brushsides + n;
		out->numbrushsides = c;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid textureindex %i (%i textures)", n, loadmodel->num_textures);
		out->texture = loadmodel->data_textures + n;

		// make a list of mplane_t structs to construct a colbrush from
		if (maxplanes < out->numbrushsides)
		{
			maxplanes = out->numbrushsides;
			if (planes)
				Mem_Free(planes);
			planes = (colplanef_t *)Mem_Alloc(tempmempool, sizeof(colplanef_t) * maxplanes);
		}
		for (j = 0;j < out->numbrushsides;j++)
		{
			VectorCopy(out->firstbrushside[j].plane->normal, planes[j].normal);
			planes[j].dist = out->firstbrushside[j].plane->dist;
			planes[j].q3surfaceflags = out->firstbrushside[j].texture->surfaceflags;
			planes[j].texture = out->firstbrushside[j].texture;
		}
		// make the colbrush from the planes
		out->colbrushf = Collision_NewBrushFromPlanes(loadmodel->mempool, out->numbrushsides, planes, out->texture->supercontents);
	}
	if (planes)
		Mem_Free(planes);
}

static void Mod_Q3BSP_LoadEffects(lump_t *l)
{
	q3deffect_t *in;
	q3deffect_t *out;
	int i, n, count;

	in = (q3deffect_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadEffects: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (q3deffect_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_effects = out;
	loadmodel->brushq3.num_effects = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		strlcpy (out->shadername, in->shadername, sizeof (out->shadername));
		n = LittleLong(in->brushindex);
		if (n >= loadmodel->brush.num_brushes)
		{
			Con_Printf("Mod_Q3BSP_LoadEffects: invalid brushindex %i (%i brushes), setting to -1\n", n, loadmodel->brush.num_brushes);
			n = -1;
		}
		out->brushindex = n;
		out->unknown = LittleLong(in->unknown);
	}
}

static void Mod_Q3BSP_LoadVertices(lump_t *l)
{
	q3dvertex_t *in;
	int i, count;

	in = (q3dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadVertices: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_vertices = count = l->filelen / sizeof(*in);
	loadmodel->brushq3.data_vertex3f = (float *)Mem_Alloc(loadmodel->mempool, count * (sizeof(float) * (3 + 3 + 2 + 2 + 4)));
	loadmodel->brushq3.data_normal3f = loadmodel->brushq3.data_vertex3f + count * 3;
	loadmodel->brushq3.data_texcoordtexture2f = loadmodel->brushq3.data_normal3f + count * 3;
	loadmodel->brushq3.data_texcoordlightmap2f = loadmodel->brushq3.data_texcoordtexture2f + count * 2;
	loadmodel->brushq3.data_color4f = loadmodel->brushq3.data_texcoordlightmap2f + count * 2;

	for (i = 0;i < count;i++, in++)
	{
		loadmodel->brushq3.data_vertex3f[i * 3 + 0] = LittleFloat(in->origin3f[0]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 1] = LittleFloat(in->origin3f[1]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 2] = LittleFloat(in->origin3f[2]);
		loadmodel->brushq3.data_normal3f[i * 3 + 0] = LittleFloat(in->normal3f[0]);
		loadmodel->brushq3.data_normal3f[i * 3 + 1] = LittleFloat(in->normal3f[1]);
		loadmodel->brushq3.data_normal3f[i * 3 + 2] = LittleFloat(in->normal3f[2]);
		loadmodel->brushq3.data_texcoordtexture2f[i * 2 + 0] = LittleFloat(in->texcoord2f[0]);
		loadmodel->brushq3.data_texcoordtexture2f[i * 2 + 1] = LittleFloat(in->texcoord2f[1]);
		loadmodel->brushq3.data_texcoordlightmap2f[i * 2 + 0] = LittleFloat(in->lightmap2f[0]);
		loadmodel->brushq3.data_texcoordlightmap2f[i * 2 + 1] = LittleFloat(in->lightmap2f[1]);
		// svector/tvector are calculated later in face loading
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

	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(int[3]))
		Host_Error("Mod_Q3BSP_LoadTriangles: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.num_triangles = count / 3;
	loadmodel->brushq3.data_element3i = out;

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

static void Mod_Q3BSP_LoadLightmaps(lump_t *l, lump_t *faceslump)
{
	q3dlightmap_t *in;
	int i, j, count, power, power2, mask, endlightmap;
	unsigned char *c;

	if (!l->filelen)
		return;
	if (cls.state == ca_dedicated)
		return;
	in = (q3dlightmap_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightmaps: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);

	// now check the surfaces to see if any of them index an odd numbered
	// lightmap, if so this is not a deluxemapped bsp file
	//
	// also check what lightmaps are actually used, because q3map2 sometimes
	// (always?) makes an unused one at the end, which
	// q3map2 sometimes (or always?) makes a second blank lightmap for no
	// reason when only one lightmap is used, which can throw off the
	// deluxemapping detection method, so check 2-lightmap bsp's specifically
	// to see if the second lightmap is blank, if so it is not deluxemapped.
	loadmodel->brushq3.deluxemapping = !(count & 1);
	loadmodel->brushq3.deluxemapping_modelspace = true;
	endlightmap = 0;
	if (loadmodel->brushq3.deluxemapping)
	{
		int facecount = faceslump->filelen / sizeof(q3dface_t);
		q3dface_t *faces = (q3dface_t *)(mod_base + faceslump->fileofs);
		for (i = 0;i < facecount;i++)
		{
			j = LittleLong(faces[i].lightmapindex);
			if (j >= 0)
			{
				endlightmap = max(endlightmap, j + 1);
				if ((j & 1) || j + 1 >= count)
				{
					loadmodel->brushq3.deluxemapping = false;
					break;
				}
			}
		}
	}
	if (endlightmap < 2)
		loadmodel->brushq3.deluxemapping = false;

	// q3map2 sometimes (or always?) makes a second blank lightmap for no
	// reason when only one lightmap is used, which can throw off the
	// deluxemapping detection method, so check 2-lightmap bsp's specifically
	// to see if the second lightmap is blank, if so it is not deluxemapped.
	if (endlightmap == 1 && count == 2)
	{
		c = in[1].rgb;
		for (i = 0;i < 128*128*3;i++)
			if (c[i])
				break;
		if (i == 128*128*3)
		{
			// all pixels in the unused lightmap were black...
			loadmodel->brushq3.deluxemapping = false;
		}
	}

	Con_DPrintf("%s is %sdeluxemapped\n", loadmodel->name, loadmodel->brushq3.deluxemapping ? "" : "not ");

	// figure out what the most reasonable merge power is within limits
	loadmodel->brushq3.num_lightmapmergepower = 0;
	for (power = 1;power <= mod_q3bsp_lightmapmergepower.integer && (1 << power) <= gl_max_texture_size && (1 << (power * 2)) < 4 * (count >> loadmodel->brushq3.deluxemapping);power++)
		loadmodel->brushq3.num_lightmapmergepower = power;
	loadmodel->brushq3.num_lightmapmerge = 1 << loadmodel->brushq3.num_lightmapmergepower;

	loadmodel->brushq3.num_lightmaps = ((count >> loadmodel->brushq3.deluxemapping) + (1 << (loadmodel->brushq3.num_lightmapmergepower * 2)) - 1) >> (loadmodel->brushq3.num_lightmapmergepower * 2);
	loadmodel->brushq3.data_lightmaps = (rtexture_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brushq3.num_lightmaps * sizeof(rtexture_t *));
	if (loadmodel->brushq3.deluxemapping)
		loadmodel->brushq3.data_deluxemaps = (rtexture_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brushq3.num_lightmaps * sizeof(rtexture_t *));

	j = 128 << loadmodel->brushq3.num_lightmapmergepower;
	if (loadmodel->brushq3.data_lightmaps)
		for (i = 0;i < loadmodel->brushq3.num_lightmaps;i++)
			loadmodel->brushq3.data_lightmaps[i] = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%04i", i), j, j, NULL, TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);

	if (loadmodel->brushq3.data_deluxemaps)
		for (i = 0;i < loadmodel->brushq3.num_lightmaps;i++)
			loadmodel->brushq3.data_deluxemaps[i] = R_LoadTexture2D(loadmodel->texturepool, va("deluxemap%04i", i), j, j, NULL, TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);

	power = loadmodel->brushq3.num_lightmapmergepower;
	power2 = power * 2;
	mask = (1 << power) - 1;
	for (i = 0;i < count;i++)
	{
		j = i >> loadmodel->brushq3.deluxemapping;
		if (loadmodel->brushq3.deluxemapping && (i & 1))
			R_UpdateTexture(loadmodel->brushq3.data_deluxemaps[j >> power2], in[i].rgb, (j & mask) * 128, ((j >> power) & mask) * 128, 128, 128);
		else
			R_UpdateTexture(loadmodel->brushq3.data_lightmaps [j >> power2], in[i].rgb, (j & mask) * 128, ((j >> power) & mask) * 128, 128, 128);
	}
}

static void Mod_Q3BSP_LoadFaces(lump_t *l)
{
	q3dface_t *in, *oldin;
	msurface_t *out, *oldout;
	int i, oldi, j, n, count, invalidelements, patchsize[2], finalwidth, finalheight, xtess, ytess, finalvertices, finaltriangles, firstvertex, firstelement, type, oldnumtriangles, oldnumtriangles2, meshvertices, meshtriangles, numvertices, numtriangles;
	float lightmaptcbase[2], lightmaptcscale;
	//int *originalelement3i;
	//int *originalneighbor3i;
	float *originalvertex3f;
	//float *originalsvector3f;
	//float *originaltvector3f;
	float *originalnormal3f;
	float *originalcolor4f;
	float *originaltexcoordtexture2f;
	float *originaltexcoordlightmap2f;
	float *v;

	in = (q3dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->data_surfaces = out;
	loadmodel->num_surfaces = count;

	i = 0;
	oldi = i;
	oldin = in;
	oldout = out;
	meshvertices = 0;
	meshtriangles = 0;
	for (;i < count;i++, in++, out++)
	{
		// check face type first
		type = LittleLong(in->type);
		if (type != Q3FACETYPE_POLYGON
		 && type != Q3FACETYPE_PATCH
		 && type != Q3FACETYPE_MESH
		 && type != Q3FACETYPE_FLARE)
		{
			Con_DPrintf("Mod_Q3BSP_LoadFaces: face #%i: unknown face type %i\n", i, type);
			continue;
		}

		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->num_textures)
		{
			Con_DPrintf("Mod_Q3BSP_LoadFaces: face #%i: invalid textureindex %i (%i textures)\n", i, n, loadmodel->num_textures);
			continue;
		}
		out->texture = loadmodel->data_textures + n;
		n = LittleLong(in->effectindex);
		if (n < -1 || n >= loadmodel->brushq3.num_effects)
		{
			if (developer.integer >= 100)
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid effectindex %i (%i effects)\n", i, out->texture->name, n, loadmodel->brushq3.num_effects);
			n = -1;
		}
		if (n == -1)
			out->effect = NULL;
		else
			out->effect = loadmodel->brushq3.data_effects + n;

		if (cls.state != ca_dedicated)
		{
			out->lightmaptexture = NULL;
			out->deluxemaptexture = r_texture_blanknormalmap;
			n = LittleLong(in->lightmapindex);
			if (n < 0)
				n = -1;
			else if (n >= (loadmodel->brushq3.num_lightmaps << (loadmodel->brushq3.num_lightmapmergepower * 2)))
			{
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid lightmapindex %i (%i lightmaps)\n", i, out->texture->name, n, loadmodel->brushq3.num_lightmaps);
				n = -1;
			}
			else
			{
				out->lightmaptexture = loadmodel->brushq3.data_lightmaps[n >> (loadmodel->brushq3.num_lightmapmergepower * 2 + loadmodel->brushq3.deluxemapping)];
				if (loadmodel->brushq3.deluxemapping)
					out->deluxemaptexture = loadmodel->brushq3.data_deluxemaps[n >> (loadmodel->brushq3.num_lightmapmergepower * 2 + loadmodel->brushq3.deluxemapping)];
			}
		}

		firstvertex = LittleLong(in->firstvertex);
		numvertices = LittleLong(in->numvertices);
		firstelement = LittleLong(in->firstelement);
		numtriangles = LittleLong(in->numelements) / 3;
		if (numtriangles * 3 != LittleLong(in->numelements))
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): numelements %i is not a multiple of 3\n", i, out->texture->name, LittleLong(in->numelements));
			continue;
		}
		if (firstvertex < 0 || firstvertex + numvertices > loadmodel->brushq3.num_vertices)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid vertex range %i : %i (%i vertices)\n", i, out->texture->name, firstvertex, firstvertex + numvertices, loadmodel->brushq3.num_vertices);
			continue;
		}
		if (firstelement < 0 || firstelement + numtriangles * 3 > loadmodel->brushq3.num_triangles * 3)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid element range %i : %i (%i elements)\n", i, out->texture->name, firstelement, firstelement + numtriangles * 3, loadmodel->brushq3.num_triangles * 3);
			continue;
		}
		switch(type)
		{
		case Q3FACETYPE_POLYGON:
		case Q3FACETYPE_MESH:
			// no processing necessary
			break;
		case Q3FACETYPE_PATCH:
			patchsize[0] = LittleLong(in->specific.patch.patchsize[0]);
			patchsize[1] = LittleLong(in->specific.patch.patchsize[1]);
			if (numvertices != (patchsize[0] * patchsize[1]) || patchsize[0] < 3 || patchsize[1] < 3 || !(patchsize[0] & 1) || !(patchsize[1] & 1) || patchsize[0] * patchsize[1] >= min(r_subdivisions_maxvertices.integer, r_subdivisions_collision_maxvertices.integer))
			{
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid patchsize %ix%i\n", i, out->texture->name, patchsize[0], patchsize[1]);
				continue;
			}
			originalvertex3f = loadmodel->brushq3.data_vertex3f + firstvertex * 3;
			// convert patch to Q3FACETYPE_MESH
			xtess = Q3PatchTesselationOnX(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_tolerance.value);
			ytess = Q3PatchTesselationOnY(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_tolerance.value);
			// bound to user settings
			xtess = bound(r_subdivisions_mintess.integer, xtess, r_subdivisions_maxtess.integer);
			ytess = bound(r_subdivisions_mintess.integer, ytess, r_subdivisions_maxtess.integer);
			// bound to sanity settings
			xtess = bound(1, xtess, 1024);
			ytess = bound(1, ytess, 1024);
			// bound to user limit on vertices
			while ((xtess > 1 || ytess > 1) && (((patchsize[0] - 1) * xtess) + 1) * (((patchsize[1] - 1) * ytess) + 1) > min(r_subdivisions_maxvertices.integer, 262144))
			{
				if (xtess > ytess)
					xtess--;
				else
					ytess--;
			}
			finalwidth = ((patchsize[0] - 1) * xtess) + 1;
			finalheight = ((patchsize[1] - 1) * ytess) + 1;
			numvertices = finalwidth * finalheight;
			numtriangles = (finalwidth - 1) * (finalheight - 1) * 2;
			break;
		case Q3FACETYPE_FLARE:
			if (developer.integer >= 100)
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): Q3FACETYPE_FLARE not supported (yet)\n", i, out->texture->name);
			// don't render it
			continue;
		}
		out->num_vertices = numvertices;
		out->num_triangles = numtriangles;
		meshvertices += out->num_vertices;
		meshtriangles += out->num_triangles;
	}

	i = oldi;
	in = oldin;
	out = oldout;
	Mod_AllocSurfMesh(loadmodel->mempool, meshvertices, meshtriangles, false, true, false);
	meshvertices = 0;
	meshtriangles = 0;
	for (;i < count && meshvertices + out->num_vertices <= loadmodel->surfmesh.num_vertices;i++, in++, out++)
	{
		if (out->num_vertices < 3 || out->num_triangles < 1)
			continue;

		type = LittleLong(in->type);
		firstvertex = LittleLong(in->firstvertex);
		firstelement = LittleLong(in->firstelement);
		out->num_firstvertex = meshvertices;
		out->num_firsttriangle = meshtriangles;
		switch(type)
		{
		case Q3FACETYPE_POLYGON:
		case Q3FACETYPE_MESH:
			// no processing necessary, except for lightmap merging
			for (j = 0;j < out->num_vertices;j++)
			{
				(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[j * 3 + 0] = loadmodel->brushq3.data_vertex3f[(firstvertex + j) * 3 + 0];
				(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[j * 3 + 1] = loadmodel->brushq3.data_vertex3f[(firstvertex + j) * 3 + 1];
				(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[j * 3 + 2] = loadmodel->brushq3.data_vertex3f[(firstvertex + j) * 3 + 2];
				(loadmodel->surfmesh.data_normal3f + 3 * out->num_firstvertex)[j * 3 + 0] = loadmodel->brushq3.data_normal3f[(firstvertex + j) * 3 + 0];
				(loadmodel->surfmesh.data_normal3f + 3 * out->num_firstvertex)[j * 3 + 1] = loadmodel->brushq3.data_normal3f[(firstvertex + j) * 3 + 1];
				(loadmodel->surfmesh.data_normal3f + 3 * out->num_firstvertex)[j * 3 + 2] = loadmodel->brushq3.data_normal3f[(firstvertex + j) * 3 + 2];
				(loadmodel->surfmesh.data_texcoordtexture2f + 2 * out->num_firstvertex)[j * 2 + 0] = loadmodel->brushq3.data_texcoordtexture2f[(firstvertex + j) * 2 + 0];
				(loadmodel->surfmesh.data_texcoordtexture2f + 2 * out->num_firstvertex)[j * 2 + 1] = loadmodel->brushq3.data_texcoordtexture2f[(firstvertex + j) * 2 + 1];
				(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * out->num_firstvertex)[j * 2 + 0] = loadmodel->brushq3.data_texcoordlightmap2f[(firstvertex + j) * 2 + 0];
				(loadmodel->surfmesh.data_texcoordlightmap2f + 2 * out->num_firstvertex)[j * 2 + 1] = loadmodel->brushq3.data_texcoordlightmap2f[(firstvertex + j) * 2 + 1];
				(loadmodel->surfmesh.data_lightmapcolor4f + 4 * out->num_firstvertex)[j * 4 + 0] = loadmodel->brushq3.data_color4f[(firstvertex + j) * 4 + 0];
				(loadmodel->surfmesh.data_lightmapcolor4f + 4 * out->num_firstvertex)[j * 4 + 1] = loadmodel->brushq3.data_color4f[(firstvertex + j) * 4 + 1];
				(loadmodel->surfmesh.data_lightmapcolor4f + 4 * out->num_firstvertex)[j * 4 + 2] = loadmodel->brushq3.data_color4f[(firstvertex + j) * 4 + 2];
				(loadmodel->surfmesh.data_lightmapcolor4f + 4 * out->num_firstvertex)[j * 4 + 3] = loadmodel->brushq3.data_color4f[(firstvertex + j) * 4 + 3];
			}
			for (j = 0;j < out->num_triangles*3;j++)
				(loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] = loadmodel->brushq3.data_element3i[firstelement + j] + out->num_firstvertex;
			break;
		case Q3FACETYPE_PATCH:
			patchsize[0] = LittleLong(in->specific.patch.patchsize[0]);
			patchsize[1] = LittleLong(in->specific.patch.patchsize[1]);
			originalvertex3f = loadmodel->brushq3.data_vertex3f + firstvertex * 3;
			originalnormal3f = loadmodel->brushq3.data_normal3f + firstvertex * 3;
			originaltexcoordtexture2f = loadmodel->brushq3.data_texcoordtexture2f + firstvertex * 2;
			originaltexcoordlightmap2f = loadmodel->brushq3.data_texcoordlightmap2f + firstvertex * 2;
			originalcolor4f = loadmodel->brushq3.data_color4f + firstvertex * 4;
			// convert patch to Q3FACETYPE_MESH
			xtess = Q3PatchTesselationOnX(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_tolerance.value);
			ytess = Q3PatchTesselationOnY(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_tolerance.value);
			// bound to user settings
			xtess = bound(r_subdivisions_mintess.integer, xtess, r_subdivisions_maxtess.integer);
			ytess = bound(r_subdivisions_mintess.integer, ytess, r_subdivisions_maxtess.integer);
			// bound to sanity settings
			xtess = bound(1, xtess, 1024);
			ytess = bound(1, ytess, 1024);
			// bound to user limit on vertices
			while ((xtess > 1 || ytess > 1) && (((patchsize[0] - 1) * xtess) + 1) * (((patchsize[1] - 1) * ytess) + 1) > min(r_subdivisions_maxvertices.integer, 262144))
			{
				if (xtess > ytess)
					xtess--;
				else
					ytess--;
			}
			finalwidth = ((patchsize[0] - 1) * xtess) + 1;
			finalheight = ((patchsize[1] - 1) * ytess) + 1;
			finalvertices = finalwidth * finalheight;
			finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;
			type = Q3FACETYPE_MESH;
			// generate geometry
			// (note: normals are skipped because they get recalculated)
			Q3PatchTesselateFloat(3, sizeof(float[3]), (loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex), patchsize[0], patchsize[1], sizeof(float[3]), originalvertex3f, xtess, ytess);
			Q3PatchTesselateFloat(3, sizeof(float[3]), (loadmodel->surfmesh.data_normal3f + 3 * out->num_firstvertex), patchsize[0], patchsize[1], sizeof(float[3]), originalnormal3f, xtess, ytess);
			Q3PatchTesselateFloat(2, sizeof(float[2]), (loadmodel->surfmesh.data_texcoordtexture2f + 2 * out->num_firstvertex), patchsize[0], patchsize[1], sizeof(float[2]), originaltexcoordtexture2f, xtess, ytess);
			Q3PatchTesselateFloat(2, sizeof(float[2]), (loadmodel->surfmesh.data_texcoordlightmap2f + 2 * out->num_firstvertex), patchsize[0], patchsize[1], sizeof(float[2]), originaltexcoordlightmap2f, xtess, ytess);
			Q3PatchTesselateFloat(4, sizeof(float[4]), (loadmodel->surfmesh.data_lightmapcolor4f + 4 * out->num_firstvertex), patchsize[0], patchsize[1], sizeof(float[4]), originalcolor4f, xtess, ytess);
			Q3PatchTriangleElements((loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle), finalwidth, finalheight, out->num_firstvertex);
			out->num_triangles = Mod_RemoveDegenerateTriangles(out->num_triangles, (loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle), (loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle), loadmodel->surfmesh.data_vertex3f);
			if (developer.integer >= 100)
			{
				if (out->num_triangles < finaltriangles)
					Con_Printf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles, %i degenerate triangles removed (leaving %i)\n", patchsize[0], patchsize[1], out->num_vertices, finaltriangles, finaltriangles - out->num_triangles, out->num_triangles);
				else
					Con_Printf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles\n", patchsize[0], patchsize[1], out->num_vertices, out->num_triangles);
			}
			// q3map does not put in collision brushes for curves... ugh
			// build the lower quality collision geometry
			xtess = Q3PatchTesselationOnX(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_collision_tolerance.value);
			ytess = Q3PatchTesselationOnY(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_collision_tolerance.value);
			// bound to user settings
			xtess = bound(r_subdivisions_collision_mintess.integer, xtess, r_subdivisions_collision_maxtess.integer);
			ytess = bound(r_subdivisions_collision_mintess.integer, ytess, r_subdivisions_collision_maxtess.integer);
			// bound to sanity settings
			xtess = bound(1, xtess, 1024);
			ytess = bound(1, ytess, 1024);
			// bound to user limit on vertices
			while ((xtess > 1 || ytess > 1) && (((patchsize[0] - 1) * xtess) + 1) * (((patchsize[1] - 1) * ytess) + 1) > min(r_subdivisions_collision_maxvertices.integer, 262144))
			{
				if (xtess > ytess)
					xtess--;
				else
					ytess--;
			}
			finalwidth = ((patchsize[0] - 1) * xtess) + 1;
			finalheight = ((patchsize[1] - 1) * ytess) + 1;
			finalvertices = finalwidth * finalheight;
			finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;

			out->data_collisionvertex3f = (float *)Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * finalvertices);
			out->data_collisionelement3i = (int *)Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * finaltriangles);
			out->num_collisionvertices = finalvertices;
			out->num_collisiontriangles = finaltriangles;
			Q3PatchTesselateFloat(3, sizeof(float[3]), out->data_collisionvertex3f, patchsize[0], patchsize[1], sizeof(float[3]), originalvertex3f, xtess, ytess);
			Q3PatchTriangleElements(out->data_collisionelement3i, finalwidth, finalheight, 0);

			//Mod_SnapVertices(3, out->num_vertices, (loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex), 0.25);
			Mod_SnapVertices(3, out->num_collisionvertices, out->data_collisionvertex3f, 1);

			oldnumtriangles = out->num_triangles;
			oldnumtriangles2 = out->num_collisiontriangles;
			out->num_collisiontriangles = Mod_RemoveDegenerateTriangles(out->num_collisiontriangles, out->data_collisionelement3i, out->data_collisionelement3i, out->data_collisionvertex3f);
			if (developer.integer >= 100)
				Con_Printf("Mod_Q3BSP_LoadFaces: %ix%i curve became %i:%i vertices / %i:%i triangles (%i:%i degenerate)\n", patchsize[0], patchsize[1], out->num_vertices, out->num_collisionvertices, oldnumtriangles, oldnumtriangles2, oldnumtriangles - out->num_triangles, oldnumtriangles2 - out->num_collisiontriangles);
			break;
		default:
			break;
		}
		meshvertices += out->num_vertices;
		meshtriangles += out->num_triangles;
		for (j = 0, invalidelements = 0;j < out->num_triangles * 3;j++)
			if ((loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] < out->num_firstvertex || (loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] >= out->num_firstvertex + out->num_vertices)
				invalidelements++;
		if (invalidelements)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: Warning: face #%i has %i invalid elements, type = %i, texture->name = \"%s\", texture->surfaceflags = %i, firstvertex = %i, numvertices = %i, firstelement = %i, numelements = %i, elements list:\n", i, invalidelements, type, out->texture->name, out->texture->surfaceflags, firstvertex, out->num_vertices, firstelement, out->num_triangles * 3);
			for (j = 0;j < out->num_triangles * 3;j++)
			{
				Con_Printf(" %i", (loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] - out->num_firstvertex);
				if ((loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] < out->num_firstvertex || (loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] >= out->num_firstvertex + out->num_vertices)
					(loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle)[j] = out->num_firstvertex;
			}
			Con_Print("\n");
		}
		// calculate a bounding box
		VectorClear(out->mins);
		VectorClear(out->maxs);
		if (out->num_vertices)
		{
			int lightmapindex = LittleLong(in->lightmapindex);
			if (lightmapindex >= 0 && cls.state != ca_dedicated)
			{
				lightmapindex >>= loadmodel->brushq3.deluxemapping;
				lightmaptcscale = 1.0f / loadmodel->brushq3.num_lightmapmerge;
				lightmaptcbase[0] = ((lightmapindex                                             ) & (loadmodel->brushq3.num_lightmapmerge - 1)) * lightmaptcscale;
				lightmaptcbase[1] = ((lightmapindex >> loadmodel->brushq3.num_lightmapmergepower) & (loadmodel->brushq3.num_lightmapmerge - 1)) * lightmaptcscale;
				// modify the lightmap texcoords to match this region of the merged lightmap
				for (j = 0, v = loadmodel->surfmesh.data_texcoordlightmap2f + 2 * out->num_firstvertex;j < out->num_vertices;j++, v += 2)
				{
					v[0] = v[0] * lightmaptcscale + lightmaptcbase[0];
					v[1] = v[1] * lightmaptcscale + lightmaptcbase[1];
				}
			}
			VectorCopy((loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex), out->mins);
			VectorCopy((loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex), out->maxs);
			for (j = 1, v = (loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex) + 3;j < out->num_vertices;j++, v += 3)
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
		// set lightmap styles for consistency with q1bsp
		//out->lightmapinfo->styles[0] = 0;
		//out->lightmapinfo->styles[1] = 255;
		//out->lightmapinfo->styles[2] = 255;
		//out->lightmapinfo->styles[3] = 255;
	}

	// for per pixel lighting
	Mod_BuildTextureVectorsFromNormals(0, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_texcoordtexture2f, loadmodel->surfmesh.data_normal3f, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.data_svector3f, loadmodel->surfmesh.data_tvector3f, true);

	// free the no longer needed vertex data
	loadmodel->brushq3.num_vertices = 0;
	if (loadmodel->brushq3.data_vertex3f)
		Mem_Free(loadmodel->brushq3.data_vertex3f);
	loadmodel->brushq3.data_vertex3f = NULL;
	loadmodel->brushq3.data_normal3f = NULL;
	loadmodel->brushq3.data_texcoordtexture2f = NULL;
	loadmodel->brushq3.data_texcoordlightmap2f = NULL;
	loadmodel->brushq3.data_color4f = NULL;
	// free the no longer needed triangle data
	loadmodel->brushq3.num_triangles = 0;
	if (loadmodel->brushq3.data_element3i)
		Mem_Free(loadmodel->brushq3.data_element3i);
	loadmodel->brushq3.data_element3i = NULL;
}

static void Mod_Q3BSP_LoadModels(lump_t *l)
{
	q3dmodel_t *in;
	q3dmodel_t *out;
	int i, j, n, c, count;

	in = (q3dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadModels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (q3dmodel_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

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
		if (n < 0 || n + c > loadmodel->num_surfaces)
			Host_Error("Mod_Q3BSP_LoadModels: invalid face range %i : %i (%i faces)", n, n + c, loadmodel->num_surfaces);
		out->firstface = n;
		out->numfaces = c;
		n = LittleLong(in->firstbrush);
		c = LittleLong(in->numbrushes);
		if (n < 0 || n + c > loadmodel->brush.num_brushes)
			Host_Error("Mod_Q3BSP_LoadModels: invalid brush range %i : %i (%i brushes)", n, n + c, loadmodel->brush.num_brushes);
		out->firstbrush = n;
		out->numbrushes = c;
	}
}

static void Mod_Q3BSP_LoadLeafBrushes(lump_t *l)
{
	int *in;
	int *out;
	int i, n, count;

	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_leafbrushes = out;
	loadmodel->brush.num_leafbrushes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->brush.num_brushes)
			Host_Error("Mod_Q3BSP_LoadLeafBrushes: invalid brush index %i (%i brushes)", n, loadmodel->brush.num_brushes);
		*out = n;
	}
}

static void Mod_Q3BSP_LoadLeafFaces(lump_t *l)
{
	int *in;
	int *out;
	int i, n, count;

	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (int *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_leafsurfaces = out;
	loadmodel->brush.num_leafsurfaces = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->num_surfaces)
			Host_Error("Mod_Q3BSP_LoadLeafFaces: invalid face index %i (%i faces)", n, loadmodel->num_surfaces);
		*out = n;
	}
}

static void Mod_Q3BSP_LoadLeafs(lump_t *l)
{
	q3dleaf_t *in;
	mleaf_t *out;
	int i, j, n, c, count;

	in = (q3dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mleaf_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_leafs = out;
	loadmodel->brush.num_leafs = count;

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
		if (n < 0 || n + c > loadmodel->brush.num_leafsurfaces)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafsurface range %i : %i (%i leafsurfaces)", n, n + c, loadmodel->brush.num_leafsurfaces);
		out->firstleafsurface = loadmodel->brush.data_leafsurfaces + n;
		out->numleafsurfaces = c;
		n = LittleLong(in->firstleafbrush);
		c = LittleLong(in->numleafbrushes);
		if (n < 0 || n + c > loadmodel->brush.num_leafbrushes)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafbrush range %i : %i (%i leafbrushes)", n, n + c, loadmodel->brush.num_leafbrushes);
		out->firstleafbrush = loadmodel->brush.data_leafbrushes + n;
		out->numleafbrushes = c;
	}
}

static void Mod_Q3BSP_LoadNodes(lump_t *l)
{
	q3dnode_t *in;
	mnode_t *out;
	int i, j, n, count;

	in = (q3dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mnode_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brush.data_nodes = out;
	loadmodel->brush.num_nodes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->parent = NULL;
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brush.num_planes)
			Host_Error("Mod_Q3BSP_LoadNodes: invalid planeindex %i (%i planes)", n, loadmodel->brush.num_planes);
		out->plane = loadmodel->brush.data_planes + n;
		for (j = 0;j < 2;j++)
		{
			n = LittleLong(in->childrenindex[j]);
			if (n >= 0)
			{
				if (n >= loadmodel->brush.num_nodes)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child node index %i (%i nodes)", n, loadmodel->brush.num_nodes);
				out->children[j] = loadmodel->brush.data_nodes + n;
			}
			else
			{
				n = -1 - n;
				if (n >= loadmodel->brush.num_leafs)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child leaf index %i (%i leafs)", n, loadmodel->brush.num_leafs);
				out->children[j] = (mnode_t *)(loadmodel->brush.data_leafs + n);
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
	Mod_Q1BSP_LoadNodes_RecursiveSetParent(loadmodel->brush.data_nodes, NULL);
}

static void Mod_Q3BSP_LoadLightGrid(lump_t *l)
{
	q3dlightgrid_t *in;
	q3dlightgrid_t *out;
	int count;

	in = (q3dlightgrid_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightGrid: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_lightgrid_scale[0] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[0];
	loadmodel->brushq3.num_lightgrid_scale[1] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[1];
	loadmodel->brushq3.num_lightgrid_scale[2] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[2];
	loadmodel->brushq3.num_lightgrid_imins[0] = (int)ceil(loadmodel->brushq3.data_models->mins[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imins[1] = (int)ceil(loadmodel->brushq3.data_models->mins[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imins[2] = (int)ceil(loadmodel->brushq3.data_models->mins[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_imaxs[0] = (int)floor(loadmodel->brushq3.data_models->maxs[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imaxs[1] = (int)floor(loadmodel->brushq3.data_models->maxs[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imaxs[2] = (int)floor(loadmodel->brushq3.data_models->maxs[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_isize[0] = loadmodel->brushq3.num_lightgrid_imaxs[0] - loadmodel->brushq3.num_lightgrid_imins[0] + 1;
	loadmodel->brushq3.num_lightgrid_isize[1] = loadmodel->brushq3.num_lightgrid_imaxs[1] - loadmodel->brushq3.num_lightgrid_imins[1] + 1;
	loadmodel->brushq3.num_lightgrid_isize[2] = loadmodel->brushq3.num_lightgrid_imaxs[2] - loadmodel->brushq3.num_lightgrid_imins[2] + 1;
	count = loadmodel->brushq3.num_lightgrid_isize[0] * loadmodel->brushq3.num_lightgrid_isize[1] * loadmodel->brushq3.num_lightgrid_isize[2];
	Matrix4x4_CreateScale3(&loadmodel->brushq3.num_lightgrid_indexfromworld, loadmodel->brushq3.num_lightgrid_scale[0], loadmodel->brushq3.num_lightgrid_scale[1], loadmodel->brushq3.num_lightgrid_scale[2]);
	Matrix4x4_ConcatTranslate(&loadmodel->brushq3.num_lightgrid_indexfromworld, -loadmodel->brushq3.num_lightgrid_imins[0] * loadmodel->brushq3.num_lightgrid_cellsize[0], -loadmodel->brushq3.num_lightgrid_imins[1] * loadmodel->brushq3.num_lightgrid_cellsize[1], -loadmodel->brushq3.num_lightgrid_imins[2] * loadmodel->brushq3.num_lightgrid_cellsize[2]);

	// if lump is empty there is nothing to load, we can deal with that in the LightPoint code
	if (l->filelen)
	{
		if (l->filelen < count * (int)sizeof(*in))
			Host_Error("Mod_Q3BSP_LoadLightGrid: invalid lightgrid lump size %i bytes, should be %i bytes (%ix%ix%i)", l->filelen, (int)(count * sizeof(*in)), loadmodel->brushq3.num_lightgrid_dimensions[0], loadmodel->brushq3.num_lightgrid_dimensions[1], loadmodel->brushq3.num_lightgrid_dimensions[2]);
		if (l->filelen != count * (int)sizeof(*in))
			Con_Printf("Mod_Q3BSP_LoadLightGrid: Warning: calculated lightgrid size %i bytes does not match lump size %i\n", (int)(count * sizeof(*in)), l->filelen);
		out = (q3dlightgrid_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));
		loadmodel->brushq3.data_lightgrid = out;
		loadmodel->brushq3.num_lightgrid = count;
		// no swapping or validation necessary
		memcpy(out, in, count * (int)sizeof(*out));
	}
}

static void Mod_Q3BSP_LoadPVS(lump_t *l)
{
	q3dpvs_t *in;
	int totalchains;

	if (l->filelen == 0)
	{
		int i;
		// unvised maps often have cluster indices even without pvs, so check
		// leafs to find real number of clusters
		loadmodel->brush.num_pvsclusters = 1;
		for (i = 0;i < loadmodel->brush.num_leafs;i++)
			loadmodel->brush.num_pvsclusters = max(loadmodel->brush.num_pvsclusters, loadmodel->brush.data_leafs[i].clusterindex + 1);

		// create clusters
		loadmodel->brush.num_pvsclusterbytes = (loadmodel->brush.num_pvsclusters + 7) / 8;
		totalchains = loadmodel->brush.num_pvsclusterbytes * loadmodel->brush.num_pvsclusters;
		loadmodel->brush.data_pvsclusters = (unsigned char *)Mem_Alloc(loadmodel->mempool, totalchains);
		memset(loadmodel->brush.data_pvsclusters, 0xFF, totalchains);
		return;
	}

	in = (q3dpvs_t *)(mod_base + l->fileofs);
	if (l->filelen < 9)
		Host_Error("Mod_Q3BSP_LoadPVS: funny lump size in %s",loadmodel->name);

	loadmodel->brush.num_pvsclusters = LittleLong(in->numclusters);
	loadmodel->brush.num_pvsclusterbytes = LittleLong(in->chainlength);
	if (loadmodel->brush.num_pvsclusterbytes < ((loadmodel->brush.num_pvsclusters + 7) / 8))
		Host_Error("Mod_Q3BSP_LoadPVS: (chainlength = %i) < ((numclusters = %i) + 7) / 8", loadmodel->brush.num_pvsclusterbytes, loadmodel->brush.num_pvsclusters);
	totalchains = loadmodel->brush.num_pvsclusterbytes * loadmodel->brush.num_pvsclusters;
	if (l->filelen < totalchains + (int)sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPVS: lump too small ((numclusters = %i) * (chainlength = %i) + sizeof(q3dpvs_t) == %i bytes, lump is %i bytes)", loadmodel->brush.num_pvsclusters, loadmodel->brush.num_pvsclusterbytes, (int)(totalchains + sizeof(*in)), l->filelen);

	loadmodel->brush.data_pvsclusters = (unsigned char *)Mem_Alloc(loadmodel->mempool, totalchains);
	memcpy(loadmodel->brush.data_pvsclusters, (unsigned char *)(in + 1), totalchains);
}

static void Mod_Q3BSP_LightPoint(model_t *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal)
{
	int i, j, k, index[3];
	float transformed[3], blend1, blend2, blend, yaw, pitch, sinpitch, stylescale;
	q3dlightgrid_t *a, *s;

	// scale lighting by lightstyle[0] so that darkmode in dpmod works properly
	stylescale = r_refdef.lightstylevalue[0] * (1.0f / 264.0f);

	if (!model->brushq3.num_lightgrid)
	{
		ambientcolor[0] = stylescale;
		ambientcolor[1] = stylescale;
		ambientcolor[2] = stylescale;
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
				blend = blend2 * (i ? (transformed[0] - index[0]) : (1 - (transformed[0] - index[0]))) * stylescale;
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

	// normalize the light direction before turning
	VectorNormalize(diffusenormal);
	//Con_Printf("result: ambient %f %f %f diffuse %f %f %f diffusenormal %f %f %f\n", ambientcolor[0], ambientcolor[1], ambientcolor[2], diffusecolor[0], diffusecolor[1], diffusecolor[2], diffusenormal[0], diffusenormal[1], diffusenormal[2]);
}

static void Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace_t *trace, model_t *model, mnode_t *node, const vec3_t point, int markframe)
{
	int i;
	mleaf_t *leaf;
	colbrushf_t *brush;
	// find which leaf the point is in
	while (node->plane)
		node = node->children[DotProduct(point, node->plane->normal) < node->plane->dist];
	// point trace the brushes
	leaf = (mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = model->brush.data_brushes[leaf->firstleafbrush[i]].colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(point, point, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TracePointBrushFloat(trace, point, brush);
		}
	}
	// can't do point traces on curves (they have no thickness)
}

static void Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace_t *trace, model_t *model, mnode_t *node, const vec3_t start, const vec3_t end, vec_t startfrac, vec_t endfrac, const vec3_t linestart, const vec3_t lineend, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i, startside, endside;
	float dist1, dist2, midfrac, mid[3], nodesegmentmins[3], nodesegmentmaxs[3];
	mleaf_t *leaf;
	msurface_t *surface;
	mplane_t *plane;
	colbrushf_t *brush;
	// walk the tree until we hit a leaf, recursing for any split cases
	while (node->plane)
	{
		// abort if this part of the bsp tree can not be hit by this trace
//		if (!(node->combinedsupercontents & trace->hitsupercontentsmask))
//			return;
		plane = node->plane;
		// axial planes are much more common than non-axial, so an optimized
		// axial case pays off here
		if (plane->type < 3)
		{
			dist1 = start[plane->type] - plane->dist;
			dist2 = end[plane->type] - plane->dist;
		}
		else
		{
			dist1 = DotProduct(start, plane->normal) - plane->dist;
			dist2 = DotProduct(end, plane->normal) - plane->dist;
		}
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
			dist1 = PlaneDiff(linestart, plane);
			dist2 = PlaneDiff(lineend, plane);
			midfrac = dist1 / (dist1 - dist2);
			VectorLerp(linestart, midfrac, lineend, mid);
			// take the near side first
			Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model, node->children[startside], start, mid, startfrac, midfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
			// if we found an impact on the front side, don't waste time
			// exploring the far side
			if (midfrac <= trace->realfraction)
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model, node->children[endside], mid, end, midfrac, endfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
			return;
		}
	}
	// abort if this part of the bsp tree can not be hit by this trace
//	if (!(node->combinedsupercontents & trace->hitsupercontentsmask))
//		return;
	// hit a leaf
	nodesegmentmins[0] = min(start[0], end[0]) - 1;
	nodesegmentmins[1] = min(start[1], end[1]) - 1;
	nodesegmentmins[2] = min(start[2], end[2]) - 1;
	nodesegmentmaxs[0] = max(start[0], end[0]) + 1;
	nodesegmentmaxs[1] = max(start[1], end[1]) + 1;
	nodesegmentmaxs[2] = max(start[2], end[2]) + 1;
	// line trace the brushes
	leaf = (mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = model->brush.data_brushes[leaf->firstleafbrush[i]].colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TraceLineBrushFloat(trace, linestart, lineend, brush, brush);
		}
	}
	// can't do point traces on curves (they have no thickness)
	if (leaf->containscollisionsurfaces && mod_q3bsp_curves_collisions.integer && !VectorCompare(start, end))
	{
		// line trace the curves
		for (i = 0;i < leaf->numleafsurfaces;i++)
		{
			surface = model->data_surfaces + leaf->firstleafsurface[i];
			if (surface->num_collisiontriangles && surface->collisionmarkframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs))
			{
				surface->collisionmarkframe = markframe;
				Collision_TraceLineTriangleMeshFloat(trace, linestart, lineend, surface->num_collisiontriangles, surface->data_collisionelement3i, surface->data_collisionvertex3f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
			}
		}
	}
}

static void Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace_t *trace, model_t *model, mnode_t *node, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i;
	int sides;
	mleaf_t *leaf;
	colbrushf_t *brush;
	msurface_t *surface;
	mplane_t *plane;
	float nodesegmentmins[3], nodesegmentmaxs[3];
	// walk the tree until we hit a leaf, recursing for any split cases
	while (node->plane)
	{
		// abort if this part of the bsp tree can not be hit by this trace
//		if (!(node->combinedsupercontents & trace->hitsupercontentsmask))
//			return;
		plane = node->plane;
		// axial planes are much more common than non-axial, so an optimized
		// axial case pays off here
		if (plane->type < 3)
		{
			// this is an axial plane, compare bounding box directly to it and
			// recurse sides accordingly
			// recurse down node sides
			// use an inlined axial BoxOnPlaneSide to slightly reduce overhead
			//sides = BoxOnPlaneSide(nodesegmentmins, nodesegmentmaxs, plane);
			//sides = ((segmentmaxs[plane->type] >= plane->dist) | ((segmentmins[plane->type] < plane->dist) << 1));
			sides = ((segmentmaxs[plane->type] >= plane->dist) + ((segmentmins[plane->type] < plane->dist) * 2));
		}
		else
		{
			// this is a non-axial plane, so check if the start and end boxes
			// are both on one side of the plane to handle 'diagonal' cases
			sides = BoxOnPlaneSide(thisbrush_start->mins, thisbrush_start->maxs, plane) | BoxOnPlaneSide(thisbrush_end->mins, thisbrush_end->maxs, plane);
		}
		if (sides == 3)
		{
			// segment crosses plane
			Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, model, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
			sides = 2;
		}
		// if sides == 0 then the trace itself is bogus (Not A Number values),
		// in this case we simply pretend the trace hit nothing
		if (sides == 0)
			return; // ERROR: NAN bounding box!
		// take whichever side the segment box is on
		node = node->children[sides - 1];
	}
	// abort if this part of the bsp tree can not be hit by this trace
//	if (!(node->combinedsupercontents & trace->hitsupercontentsmask))
//		return;
	nodesegmentmins[0] = max(segmentmins[0], node->mins[0] - 1);
	nodesegmentmins[1] = max(segmentmins[1], node->mins[1] - 1);
	nodesegmentmins[2] = max(segmentmins[2], node->mins[2] - 1);
	nodesegmentmaxs[0] = min(segmentmaxs[0], node->maxs[0] + 1);
	nodesegmentmaxs[1] = min(segmentmaxs[1], node->maxs[1] + 1);
	nodesegmentmaxs[2] = min(segmentmaxs[2], node->maxs[2] + 1);
	// hit a leaf
	leaf = (mleaf_t *)node;
	for (i = 0;i < leaf->numleafbrushes;i++)
	{
		brush = model->brush.data_brushes[leaf->firstleafbrush[i]].colbrushf;
		if (brush && brush->markframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, brush->mins, brush->maxs))
		{
			brush->markframe = markframe;
			Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, brush, brush);
		}
	}
	if (leaf->containscollisionsurfaces && mod_q3bsp_curves_collisions.integer)
	{
		for (i = 0;i < leaf->numleafsurfaces;i++)
		{
			surface = model->data_surfaces + leaf->firstleafsurface[i];
			if (surface->num_collisiontriangles && surface->collisionmarkframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs))
			{
				surface->collisionmarkframe = markframe;
				Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, surface->num_collisiontriangles, surface->data_collisionelement3i, surface->data_collisionvertex3f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
			}
		}
	}
}

static void Mod_Q3BSP_TraceBox(model_t *model, int frame, trace_t *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask)
{
	int i;
	float segmentmins[3], segmentmaxs[3];
	static int markframe = 0;
	msurface_t *surface;
	q3mbrush_t *brush;
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	if (mod_q3bsp_optimizedtraceline.integer && VectorLength2(boxmins) + VectorLength2(boxmaxs) == 0)
	{
		if (VectorCompare(start, end))
		{
			// point trace
			if (model->brush.submodel)
			{
				for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
					if (brush->colbrushf)
						Collision_TracePointBrushFloat(trace, start, brush->colbrushf);
			}
			else
				Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace, model, model->brush.data_nodes, start, ++markframe);
		}
		else
		{
			// line trace
			segmentmins[0] = min(start[0], end[0]) - 1;
			segmentmins[1] = min(start[1], end[1]) - 1;
			segmentmins[2] = min(start[2], end[2]) - 1;
			segmentmaxs[0] = max(start[0], end[0]) + 1;
			segmentmaxs[1] = max(start[1], end[1]) + 1;
			segmentmaxs[2] = max(start[2], end[2]) + 1;
			if (model->brush.submodel)
			{
				for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
					if (brush->colbrushf)
						Collision_TraceLineBrushFloat(trace, start, end, brush->colbrushf, brush->colbrushf);
				if (mod_q3bsp_curves_collisions.integer)
					for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
						if (surface->num_collisiontriangles)
							Collision_TraceLineTriangleMeshFloat(trace, start, end, surface->num_collisiontriangles, surface->data_collisionelement3i, surface->data_collisionvertex3f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
			}
			else
				Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model, model->brush.data_nodes, start, end, 0, 1, start, end, ++markframe, segmentmins, segmentmaxs);
		}
	}
	else
	{
		// box trace, performed as brush trace
		colbrushf_t *thisbrush_start, *thisbrush_end;
		vec3_t boxstartmins, boxstartmaxs, boxendmins, boxendmaxs;
		segmentmins[0] = min(start[0], end[0]) + boxmins[0] - 1;
		segmentmins[1] = min(start[1], end[1]) + boxmins[1] - 1;
		segmentmins[2] = min(start[2], end[2]) + boxmins[2] - 1;
		segmentmaxs[0] = max(start[0], end[0]) + boxmaxs[0] + 1;
		segmentmaxs[1] = max(start[1], end[1]) + boxmaxs[1] + 1;
		segmentmaxs[2] = max(start[2], end[2]) + boxmaxs[2] + 1;
		VectorAdd(start, boxmins, boxstartmins);
		VectorAdd(start, boxmaxs, boxstartmaxs);
		VectorAdd(end, boxmins, boxendmins);
		VectorAdd(end, boxmaxs, boxendmaxs);
		thisbrush_start = Collision_BrushForBox(&identitymatrix, boxstartmins, boxstartmaxs, 0, 0, NULL);
		thisbrush_end = Collision_BrushForBox(&identitymatrix, boxendmins, boxendmaxs, 0, 0, NULL);
		if (model->brush.submodel)
		{
			for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
				if (brush->colbrushf)
					Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, brush->colbrushf, brush->colbrushf);
			if (mod_q3bsp_curves_collisions.integer)
				for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
					if (surface->num_collisiontriangles)
						Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, surface->num_collisiontriangles, surface->data_collisionelement3i, surface->data_collisionvertex3f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
		}
		else
			Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, model, model->brush.data_nodes, thisbrush_start, thisbrush_end, ++markframe, segmentmins, segmentmaxs);
	}
}

static int Mod_Q3BSP_SuperContentsFromNativeContents(model_t *model, int nativecontents)
{
	int supercontents = 0;
	if (nativecontents & CONTENTSQ3_SOLID)
		supercontents |= SUPERCONTENTS_SOLID;
	if (nativecontents & CONTENTSQ3_WATER)
		supercontents |= SUPERCONTENTS_WATER;
	if (nativecontents & CONTENTSQ3_SLIME)
		supercontents |= SUPERCONTENTS_SLIME;
	if (nativecontents & CONTENTSQ3_LAVA)
		supercontents |= SUPERCONTENTS_LAVA;
	if (nativecontents & CONTENTSQ3_BODY)
		supercontents |= SUPERCONTENTS_BODY;
	if (nativecontents & CONTENTSQ3_CORPSE)
		supercontents |= SUPERCONTENTS_CORPSE;
	if (nativecontents & CONTENTSQ3_NODROP)
		supercontents |= SUPERCONTENTS_NODROP;
	if (nativecontents & CONTENTSQ3_PLAYERCLIP)
		supercontents |= SUPERCONTENTS_PLAYERCLIP;
	if (nativecontents & CONTENTSQ3_MONSTERCLIP)
		supercontents |= SUPERCONTENTS_MONSTERCLIP;
	if (nativecontents & CONTENTSQ3_DONOTENTER)
		supercontents |= SUPERCONTENTS_DONOTENTER;
	return supercontents;
}

static int Mod_Q3BSP_NativeContentsFromSuperContents(model_t *model, int supercontents)
{
	int nativecontents = 0;
	if (supercontents & SUPERCONTENTS_SOLID)
		nativecontents |= CONTENTSQ3_SOLID;
	if (supercontents & SUPERCONTENTS_WATER)
		nativecontents |= CONTENTSQ3_WATER;
	if (supercontents & SUPERCONTENTS_SLIME)
		nativecontents |= CONTENTSQ3_SLIME;
	if (supercontents & SUPERCONTENTS_LAVA)
		nativecontents |= CONTENTSQ3_LAVA;
	if (supercontents & SUPERCONTENTS_BODY)
		nativecontents |= CONTENTSQ3_BODY;
	if (supercontents & SUPERCONTENTS_CORPSE)
		nativecontents |= CONTENTSQ3_CORPSE;
	if (supercontents & SUPERCONTENTS_NODROP)
		nativecontents |= CONTENTSQ3_NODROP;
	if (supercontents & SUPERCONTENTS_PLAYERCLIP)
		nativecontents |= CONTENTSQ3_PLAYERCLIP;
	if (supercontents & SUPERCONTENTS_MONSTERCLIP)
		nativecontents |= CONTENTSQ3_MONSTERCLIP;
	if (supercontents & SUPERCONTENTS_DONOTENTER)
		nativecontents |= CONTENTSQ3_DONOTENTER;
	return nativecontents;
}

void Mod_Q3BSP_RecursiveFindNumLeafs(mnode_t *node)
{
	int numleafs;
	while (node->plane)
	{
		Mod_Q3BSP_RecursiveFindNumLeafs(node->children[0]);
		node = node->children[1];
	}
	numleafs = ((mleaf_t *)node - loadmodel->brush.data_leafs) + 1;
	if (loadmodel->brush.num_leafs < numleafs)
		loadmodel->brush.num_leafs = numleafs;
}

void Mod_Q3BSP_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, numshadowmeshtriangles;
	q3dheader_t *header;
	float corner[3], yawradius, modelradius;
	msurface_t *surface;

	mod->type = mod_brushq3;
	mod->numframes = 2; // although alternate textures are not supported it is annoying to complain about no such frame 1
	mod->numskins = 1;

	header = (q3dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q3BSPVERSION)
		Host_Error("Mod_Q3BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q3BSPVERSION);
	mod->brush.ishlbsp = false;
	mod->brush.ismcbsp = false;
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brush.ishlbsp);
		Cvar_SetValue("mcbsp", mod->brush.ismcbsp);
	}

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q3BSP_TraceBox;
	mod->brush.TraceLineOfSight = Mod_Q1BSP_TraceLineOfSight;
	mod->brush.SuperContentsFromNativeContents = Mod_Q3BSP_SuperContentsFromNativeContents;
	mod->brush.NativeContentsFromSuperContents = Mod_Q3BSP_NativeContentsFromSuperContents;
	mod->brush.GetPVS = Mod_Q1BSP_GetPVS;
	mod->brush.FatPVS = Mod_Q1BSP_FatPVS;
	mod->brush.BoxTouchingPVS = Mod_Q1BSP_BoxTouchingPVS;
	mod->brush.BoxTouchingLeafPVS = Mod_Q1BSP_BoxTouchingLeafPVS;
	mod->brush.BoxTouchingVisibleLeafs = Mod_Q1BSP_BoxTouchingVisibleLeafs;
	mod->brush.FindBoxClusters = Mod_Q1BSP_FindBoxClusters;
	mod->brush.LightPoint = Mod_Q3BSP_LightPoint;
	mod->brush.FindNonSolidLocation = Mod_Q1BSP_FindNonSolidLocation;
	mod->brush.PointInLeaf = Mod_Q1BSP_PointInLeaf;
	mod->Draw = R_Q1BSP_Draw;
	mod->GetLightInfo = R_Q1BSP_GetLightInfo;
	mod->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	mod->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	mod->DrawLight = R_Q1BSP_DrawLight;

	mod_base = (unsigned char *)header;

	// swap all the lumps
	header->ident = LittleLong(header->ident);
	header->version = LittleLong(header->version);
	for (i = 0;i < Q3HEADER_LUMPS;i++)
	{
		header->lumps[i].fileofs = LittleLong(header->lumps[i].fileofs);
		header->lumps[i].filelen = LittleLong(header->lumps[i].filelen);
	}

	mod->brush.qw_md4sum = 0;
	mod->brush.qw_md4sum2 = 0;
	for (i = 0;i < Q3HEADER_LUMPS;i++)
	{
		if (i == Q3LUMP_ENTITIES)
			continue;
		mod->brush.qw_md4sum ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		if (i == Q3LUMP_PVS || i == Q3LUMP_LEAFS || i == Q3LUMP_NODES)
			continue;
		mod->brush.qw_md4sum2 ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
	}

	Mod_Q3BSP_LoadEntities(&header->lumps[Q3LUMP_ENTITIES]);
	Mod_Q3BSP_LoadTextures(&header->lumps[Q3LUMP_TEXTURES]);
	Mod_Q3BSP_LoadPlanes(&header->lumps[Q3LUMP_PLANES]);
	Mod_Q3BSP_LoadBrushSides(&header->lumps[Q3LUMP_BRUSHSIDES]);
	Mod_Q3BSP_LoadBrushes(&header->lumps[Q3LUMP_BRUSHES]);
	Mod_Q3BSP_LoadEffects(&header->lumps[Q3LUMP_EFFECTS]);
	Mod_Q3BSP_LoadVertices(&header->lumps[Q3LUMP_VERTICES]);
	Mod_Q3BSP_LoadTriangles(&header->lumps[Q3LUMP_TRIANGLES]);
	Mod_Q3BSP_LoadLightmaps(&header->lumps[Q3LUMP_LIGHTMAPS], &header->lumps[Q3LUMP_FACES]);
	Mod_Q3BSP_LoadFaces(&header->lumps[Q3LUMP_FACES]);
	Mod_Q3BSP_LoadModels(&header->lumps[Q3LUMP_MODELS]);
	Mod_Q3BSP_LoadLeafBrushes(&header->lumps[Q3LUMP_LEAFBRUSHES]);
	Mod_Q3BSP_LoadLeafFaces(&header->lumps[Q3LUMP_LEAFFACES]);
	Mod_Q3BSP_LoadLeafs(&header->lumps[Q3LUMP_LEAFS]);
	Mod_Q3BSP_LoadNodes(&header->lumps[Q3LUMP_NODES]);
	Mod_Q3BSP_LoadLightGrid(&header->lumps[Q3LUMP_LIGHTGRID]);
	Mod_Q3BSP_LoadPVS(&header->lumps[Q3LUMP_PVS]);
	loadmodel->brush.numsubmodels = loadmodel->brushq3.num_models;

	// the MakePortals code works fine on the q3bsp data as well
	Mod_Q1BSP_MakePortals();

	// FIXME: shader alpha should replace r_wateralpha support in q3bsp
	loadmodel->brush.supportwateralpha = true;

	// make a single combined shadow mesh to allow optimized shadow volume creation
	numshadowmeshtriangles = 0;
	for (j = 0, surface = loadmodel->data_surfaces;j < loadmodel->num_surfaces;j++, surface++)
	{
		surface->num_firstshadowmeshtriangle = numshadowmeshtriangles;
		numshadowmeshtriangles += surface->num_triangles;
	}
	loadmodel->brush.shadowmesh = Mod_ShadowMesh_Begin(loadmodel->mempool, numshadowmeshtriangles * 3, numshadowmeshtriangles, NULL, NULL, NULL, false, false, true);
	for (j = 0, surface = loadmodel->data_surfaces;j < loadmodel->num_surfaces;j++, surface++)
		if (surface->num_triangles > 0)
			Mod_ShadowMesh_AddMesh(loadmodel->mempool, loadmodel->brush.shadowmesh, NULL, NULL, NULL, loadmodel->surfmesh.data_vertex3f, NULL, NULL, NULL, NULL, surface->num_triangles, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
	loadmodel->brush.shadowmesh = Mod_ShadowMesh_Finish(loadmodel->mempool, loadmodel->brush.shadowmesh, false, true);
	Mod_BuildTriangleNeighbors(loadmodel->brush.shadowmesh->neighbor3i, loadmodel->brush.shadowmesh->element3i, loadmodel->brush.shadowmesh->numtriangles);

	loadmodel->brush.num_leafs = 0;
	Mod_Q3BSP_RecursiveFindNumLeafs(loadmodel->brush.data_nodes);

	if (loadmodel->isworldmodel)
	{
		// clear out any stale submodels or worldmodels lying around
		// if we did this clear before now, an error might abort loading and
		// leave things in a bad state
		Mod_RemoveStaleWorldModels(loadmodel);
	}

	mod = loadmodel;
	for (i = 0;i < loadmodel->brush.numsubmodels;i++)
	{
		if (i > 0)
		{
			char name[10];
			// LordHavoc: only register submodels if it is the world
			// (prevents external bsp models from replacing world submodels with
			//  their own)
			if (!loadmodel->isworldmodel)
				continue;
			// duplicate the basic information
			sprintf(name, "*%i", i);
			mod = Mod_FindName(name);
			*mod = *loadmodel;
			strlcpy(mod->name, name, sizeof(mod->name));
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
			mod->brush.TraceLineOfSight = NULL;
			mod->brush.GetPVS = NULL;
			mod->brush.FatPVS = NULL;
			mod->brush.BoxTouchingPVS = NULL;
			mod->brush.BoxTouchingLeafPVS = NULL;
			mod->brush.BoxTouchingVisibleLeafs = NULL;
			mod->brush.FindBoxClusters = NULL;
			mod->brush.LightPoint = NULL;
			mod->brush.FindNonSolidLocation = Mod_Q1BSP_FindNonSolidLocation;
		}
		mod->brush.submodel = i;

		// make the model surface list (used by shadowing/lighting)
		mod->firstmodelsurface = mod->brushq3.data_models[i].firstface;
		mod->nummodelsurfaces = mod->brushq3.data_models[i].numfaces;
		mod->firstmodelbrush = mod->brushq3.data_models[i].firstbrush;
		mod->nummodelbrushes = mod->brushq3.data_models[i].numbrushes;
		mod->surfacelist = (int *)Mem_Alloc(loadmodel->mempool, mod->nummodelsurfaces * sizeof(*mod->surfacelist));
		for (j = 0;j < mod->nummodelsurfaces;j++)
			mod->surfacelist[j] = mod->firstmodelsurface + j;

		VectorCopy(mod->brushq3.data_models[i].mins, mod->normalmins);
		VectorCopy(mod->brushq3.data_models[i].maxs, mod->normalmaxs);
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

		for (j = 0;j < mod->nummodelsurfaces;j++)
			if (mod->data_surfaces[j + mod->firstmodelsurface].texture->surfaceflags & Q3SURFACEFLAG_SKY)
				break;
		if (j < mod->nummodelsurfaces)
			mod->DrawSky = R_Q1BSP_DrawSky;
		else
			mod->DrawSky = NULL;
	}
}

void Mod_IBSP_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i = LittleLong(((int *)buffer)[1]);
	if (i == Q3BSPVERSION)
		Mod_Q3BSP_Load(mod,buffer, bufferend);
	else if (i == Q2BSPVERSION)
		Mod_Q2BSP_Load(mod,buffer, bufferend);
	else
		Host_Error("Mod_IBSP_Load: unknown/unsupported version %i", i);
}

void Mod_MAP_Load(model_t *mod, void *buffer, void *bufferend)
{
	Host_Error("Mod_MAP_Load: not yet implemented");
}

