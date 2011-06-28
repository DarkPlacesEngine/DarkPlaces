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
cvar_t mod_bsp_portalize = {0, "mod_bsp_portalize", "1", "enables portal generation from BSP tree (may take several seconds per map), used by r_drawportals, r_useportalculling, r_shadow_realtime_world_compileportalculling, sv_cullentities_portal"};
cvar_t r_novis = {0, "r_novis", "0", "draws whole level, see also sv_cullentities_pvs 0"};
cvar_t r_nosurftextures = {0, "r_nosurftextures", "0", "pretends there was no texture lump found in the q1bsp/hlbsp loading (useful for debugging this rare case)"};
cvar_t r_subdivisions_tolerance = {0, "r_subdivisions_tolerance", "4", "maximum error tolerance on curve subdivision for rendering purposes (in other words, the curves will be given as many polygons as necessary to represent curves at this quality)"};
cvar_t r_subdivisions_mintess = {0, "r_subdivisions_mintess", "0", "minimum number of subdivisions (values above 0 will smooth curves that don't need it)"};
cvar_t r_subdivisions_maxtess = {0, "r_subdivisions_maxtess", "1024", "maximum number of subdivisions (prevents curves beyond a certain detail level, limits smoothing)"};
cvar_t r_subdivisions_maxvertices = {0, "r_subdivisions_maxvertices", "65536", "maximum vertices allowed per subdivided curve"};
cvar_t r_subdivisions_collision_tolerance = {0, "r_subdivisions_collision_tolerance", "15", "maximum error tolerance on curve subdivision for collision purposes (usually a larger error tolerance than for rendering)"};
cvar_t r_subdivisions_collision_mintess = {0, "r_subdivisions_collision_mintess", "0", "minimum number of subdivisions (values above 0 will smooth curves that don't need it)"};
cvar_t r_subdivisions_collision_maxtess = {0, "r_subdivisions_collision_maxtess", "1024", "maximum number of subdivisions (prevents curves beyond a certain detail level, limits smoothing)"};
cvar_t r_subdivisions_collision_maxvertices = {0, "r_subdivisions_collision_maxvertices", "4225", "maximum vertices allowed per subdivided curve"};
cvar_t r_trippy = {0, "r_trippy", "0", "easter egg"};
cvar_t mod_noshader_default_offsetmapping = {CVAR_SAVE, "mod_noshader_default_offsetmapping", "1", "use offsetmapping by default on all surfaces that are not using q3 shader files"};
cvar_t mod_q3bsp_curves_collisions = {0, "mod_q3bsp_curves_collisions", "1", "enables collisions with curves (SLOW)"};
cvar_t mod_q3bsp_curves_collisions_stride = {0, "mod_q3bsp_curves_collisions_stride", "16", "collisions against curves: optimize performance by doing a combined collision check for this triangle amount first (-1 avoids any box tests)"};
cvar_t mod_q3bsp_curves_stride = {0, "mod_q3bsp_curves_stride", "16", "particle effect collisions against curves: optimize performance by doing a combined collision check for this triangle amount first (-1 avoids any box tests)"};
cvar_t mod_q3bsp_optimizedtraceline = {0, "mod_q3bsp_optimizedtraceline", "1", "whether to use optimized traceline code for line traces (as opposed to tracebox code)"};
cvar_t mod_q3bsp_debugtracebrush = {0, "mod_q3bsp_debugtracebrush", "0", "selects different tracebrush bsp recursion algorithms (for debugging purposes only)"};
cvar_t mod_q3bsp_lightmapmergepower = {CVAR_SAVE, "mod_q3bsp_lightmapmergepower", "4", "merges the quake3 128x128 lightmap textures into larger lightmap group textures to speed up rendering, 1 = 256x256, 2 = 512x512, 3 = 1024x1024, 4 = 2048x2048, 5 = 4096x4096, ..."};
cvar_t mod_q3bsp_nolightmaps = {CVAR_SAVE, "mod_q3bsp_nolightmaps", "0", "do not load lightmaps in Q3BSP maps (to save video RAM, but be warned: it looks ugly)"};
cvar_t mod_q3bsp_tracelineofsight_brushes = {0, "mod_q3bsp_tracelineofsight_brushes", "0", "enables culling of entities behind detail brushes, curves, etc"};
cvar_t mod_q3shader_default_offsetmapping = {CVAR_SAVE, "mod_q3shader_default_offsetmapping", "1", "use offsetmapping by default on all surfaces that are using q3 shader files"};
cvar_t mod_q3shader_default_polygonfactor = {0, "mod_q3shader_default_polygonfactor", "0", "biases depth values of 'polygonoffset' shaders to prevent z-fighting artifacts"};
cvar_t mod_q3shader_default_polygonoffset = {0, "mod_q3shader_default_polygonoffset", "-2", "biases depth values of 'polygonoffset' shaders to prevent z-fighting artifacts"};

cvar_t mod_q1bsp_polygoncollisions = {0, "mod_q1bsp_polygoncollisions", "0", "disables use of precomputed cliphulls and instead collides with polygons (uses Bounding Interval Hierarchy optimizations)"};
cvar_t mod_collision_bih = {0, "mod_collision_bih", "1", "enables use of generated Bounding Interval Hierarchy tree instead of compiled bsp tree in collision code"};
cvar_t mod_recalculatenodeboxes = {0, "mod_recalculatenodeboxes", "1", "enables use of generated node bounding boxes based on BSP tree portal reconstruction, rather than the node boxes supplied by the map compiler"};

static texture_t mod_q1bsp_texture_solid;
static texture_t mod_q1bsp_texture_sky;
static texture_t mod_q1bsp_texture_lava;
static texture_t mod_q1bsp_texture_slime;
static texture_t mod_q1bsp_texture_water;

void Mod_BrushInit(void)
{
//	Cvar_RegisterVariable(&r_subdivide_size);
	Cvar_RegisterVariable(&mod_bsp_portalize);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_nosurftextures);
	Cvar_RegisterVariable(&r_subdivisions_tolerance);
	Cvar_RegisterVariable(&r_subdivisions_mintess);
	Cvar_RegisterVariable(&r_subdivisions_maxtess);
	Cvar_RegisterVariable(&r_subdivisions_maxvertices);
	Cvar_RegisterVariable(&r_subdivisions_collision_tolerance);
	Cvar_RegisterVariable(&r_subdivisions_collision_mintess);
	Cvar_RegisterVariable(&r_subdivisions_collision_maxtess);
	Cvar_RegisterVariable(&r_subdivisions_collision_maxvertices);
	Cvar_RegisterVariable(&r_trippy);
	Cvar_RegisterVariable(&mod_noshader_default_offsetmapping);
	Cvar_RegisterVariable(&mod_q3bsp_curves_collisions);
	Cvar_RegisterVariable(&mod_q3bsp_curves_collisions_stride);
	Cvar_RegisterVariable(&mod_q3bsp_curves_stride);
	Cvar_RegisterVariable(&mod_q3bsp_optimizedtraceline);
	Cvar_RegisterVariable(&mod_q3bsp_debugtracebrush);
	Cvar_RegisterVariable(&mod_q3bsp_lightmapmergepower);
	Cvar_RegisterVariable(&mod_q3bsp_nolightmaps);
	Cvar_RegisterVariable(&mod_q3bsp_tracelineofsight_brushes);
	Cvar_RegisterVariable(&mod_q3shader_default_offsetmapping);
	Cvar_RegisterVariable(&mod_q3shader_default_polygonfactor);
	Cvar_RegisterVariable(&mod_q3shader_default_polygonoffset);
	Cvar_RegisterVariable(&mod_q1bsp_polygoncollisions);
	Cvar_RegisterVariable(&mod_collision_bih);
	Cvar_RegisterVariable(&mod_recalculatenodeboxes);

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

static mleaf_t *Mod_Q1BSP_PointInLeaf(dp_model_t *model, const vec3_t p)
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

static void Mod_Q1BSP_AmbientSoundLevelsForPoint(dp_model_t *model, const vec3_t p, unsigned char *out, int outsize)
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

static int Mod_Q1BSP_FindBoxClusters(dp_model_t *model, const vec3_t mins, const vec3_t maxs, int maxclusters, int *clusterlist)
{
	int numclusters = 0;
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_pvsclusters)
		return -1;
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
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

static int Mod_Q1BSP_BoxTouchingPVS(dp_model_t *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_pvsclusters)
		return true;
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
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

static int Mod_Q1BSP_BoxTouchingLeafPVS(dp_model_t *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_leafs)
		return true;
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
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

static int Mod_Q1BSP_BoxTouchingVisibleLeafs(dp_model_t *model, const unsigned char *visibleleafs, const vec3_t mins, const vec3_t maxs)
{
	int nodestackindex = 0;
	mnode_t *node, *nodestack[1024];
	if (!model->brush.num_leafs)
		return true;
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
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
	vec3_t absmin, absmax;
	vec_t radius;
	vec3_t nudge;
	vec_t bestdist;
	dp_model_t *model;
}
findnonsolidlocationinfo_t;

static void Mod_Q1BSP_FindNonSolidLocation_r_Triangle(findnonsolidlocationinfo_t *info, msurface_t *surface, int k)
{
	int i, *tri;
	float dist, f, vert[3][3], edge[3][3], facenormal[3], edgenormal[3][3], point[3];

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

static void Mod_Q1BSP_FindNonSolidLocation_r_Leaf(findnonsolidlocationinfo_t *info, mleaf_t *leaf)
{
	int surfacenum, k, *mark;
	msurface_t *surface;
	for (surfacenum = 0, mark = leaf->firstleafsurface;surfacenum < leaf->numleafsurfaces;surfacenum++, mark++)
	{
		surface = info->model->data_surfaces + *mark;
		if (surface->texture->supercontents & SUPERCONTENTS_SOLID)
		{
			if(surface->deprecatedq3num_bboxstride > 0)
			{
				int i, cnt, tri;
				cnt = (surface->num_triangles + surface->deprecatedq3num_bboxstride - 1) / surface->deprecatedq3num_bboxstride;
				for(i = 0; i < cnt; ++i)
				{
					if(BoxesOverlap(surface->deprecatedq3data_bbox6f + i * 6, surface->deprecatedq3data_bbox6f + i * 6 + 3, info->absmin, info->absmax))
					{
						for(k = 0; k < surface->deprecatedq3num_bboxstride; ++k)
						{
							tri = i * surface->deprecatedq3num_bboxstride + k;
							if(tri >= surface->num_triangles)
								break;
							Mod_Q1BSP_FindNonSolidLocation_r_Triangle(info, surface, tri);
						}
					}
				}
			}
			else
			{
				for (k = 0;k < surface->num_triangles;k++)
				{
					Mod_Q1BSP_FindNonSolidLocation_r_Triangle(info, surface, k);
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

static void Mod_Q1BSP_FindNonSolidLocation(dp_model_t *model, const vec3_t in, vec3_t out, float radius)
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
		VectorCopy(info.center, info.absmin);
		VectorCopy(info.center, info.absmax);
		info.absmin[0] -= info.radius + 1;
		info.absmin[1] -= info.radius + 1;
		info.absmin[2] -= info.radius + 1;
		info.absmax[0] += info.radius + 1;
		info.absmax[1] += info.radius + 1;
		info.absmax[2] += info.radius + 1;
		Mod_Q1BSP_FindNonSolidLocation_r(&info, model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode);
		VectorAdd(info.center, info.nudge, info.center);
	}
	while (info.bestdist < radius && ++i < 10);
	VectorCopy(info.center, out);
}

int Mod_Q1BSP_SuperContentsFromNativeContents(dp_model_t *model, int nativecontents)
{
	switch(nativecontents)
	{
		case CONTENTS_EMPTY:
			return 0;
		case CONTENTS_SOLID:
			return SUPERCONTENTS_SOLID | SUPERCONTENTS_OPAQUE;
		case CONTENTS_WATER:
			return SUPERCONTENTS_WATER;
		case CONTENTS_SLIME:
			return SUPERCONTENTS_SLIME;
		case CONTENTS_LAVA:
			return SUPERCONTENTS_LAVA | SUPERCONTENTS_NODROP;
		case CONTENTS_SKY:
			return SUPERCONTENTS_SKY | SUPERCONTENTS_NODROP | SUPERCONTENTS_OPAQUE; // to match behaviour of Q3 maps, let sky count as opaque
	}
	return 0;
}

int Mod_Q1BSP_NativeContentsFromSuperContents(dp_model_t *model, int supercontents)
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
	mclipnode_t *node;
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
	mplane_t *plane;
	mclipnode_t *nodes = t->hull->clipnodes;
	mplane_t *planes = t->hull->planes;
	vec3_t point;
	VectorCopy(t->start, point);
	while (num >= 0)
	{
		plane = planes + nodes[num].planenum;
		num = nodes[num].children[(plane->type < 3 ? point[plane->type] : DotProduct(plane->normal, point)) < plane->dist];
	}
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

static void Mod_Q1BSP_TracePoint(struct model_s *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, int hitsupercontentsmask)
{
	RecursiveHullCheckTraceInfo_t rhc;

	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	rhc.trace = trace;
	rhc.trace->fraction = 1;
	rhc.trace->realfraction = 1;
	rhc.trace->allsolid = true;
	rhc.hull = &model->brushq1.hulls[0]; // 0x0x0
	VectorCopy(start, rhc.start);
	VectorCopy(start, rhc.end);
	Mod_Q1BSP_RecursiveHullCheckPoint(&rhc, rhc.hull->firstclipnode);
}

static void Mod_Q1BSP_TraceLine(struct model_s *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	RecursiveHullCheckTraceInfo_t rhc;

	if (VectorCompare(start, end))
	{
		Mod_Q1BSP_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
		return;
	}

	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	rhc.trace = trace;
	rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
	rhc.trace->fraction = 1;
	rhc.trace->realfraction = 1;
	rhc.trace->allsolid = true;
	rhc.hull = &model->brushq1.hulls[0]; // 0x0x0
	VectorCopy(start, rhc.start);
	VectorCopy(end, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
#if COLLISIONPARANOID >= 2
	Con_Printf("t(%f %f %f,%f %f %f)", rhc.start[0], rhc.start[1], rhc.start[2], rhc.end[0], rhc.end[1], rhc.end[2]);
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

static void Mod_Q1BSP_TraceBox(struct model_s *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask)
{
	// this function currently only supports same size start and end
	double boxsize[3];
	RecursiveHullCheckTraceInfo_t rhc;

	if (VectorCompare(boxmins, boxmaxs))
	{
		if (VectorCompare(start, end))
			Mod_Q1BSP_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
		else
			Mod_Q1BSP_TraceLine(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask);
		return;
	}

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

static int Mod_Q1BSP_PointSuperContents(struct model_s *model, int frame, const vec3_t point)
{
	int num = model->brushq1.hulls[0].firstclipnode;
	mplane_t *plane;
	mclipnode_t *nodes = model->brushq1.hulls[0].clipnodes;
	mplane_t *planes = model->brushq1.hulls[0].planes;
	while (num >= 0)
	{
		plane = planes + nodes[num].planenum;
		num = nodes[num].children[(plane->type < 3 ? point[plane->type] : DotProduct(plane->normal, point)) < plane->dist];
	}
	return Mod_Q1BSP_SuperContentsFromNativeContents(NULL, num);
}

void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int boxsupercontents, int boxq3surfaceflags, const texture_t *boxtexture)
{
#if 1
	colbrushf_t cbox;
	colplanef_t cbox_planes[6];
	cbox.isaabb = true;
	cbox.hasaabbplanes = true;
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
	static mclipnode_t box_clipnodes[6];
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

void Collision_ClipTrace_Point(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, int hitsupercontentsmask, int boxsupercontents, int boxq3surfaceflags, const texture_t *boxtexture)
{
	memset(trace, 0, sizeof(trace_t));
	trace->fraction = 1;
	trace->realfraction = 1;
	if (BoxesOverlap(start, start, cmins, cmaxs))
	{
		trace->startsupercontents |= boxsupercontents;
		if (hitsupercontentsmask & boxsupercontents)
		{
			trace->startsolid = true;
			trace->allsolid = true;
		}
	}
}

static qboolean Mod_Q1BSP_TraceLineOfSight(struct model_s *model, const vec3_t start, const vec3_t end)
{
	trace_t trace;
	Mod_Q1BSP_TraceLine(model, NULL, NULL, &trace, start, end, SUPERCONTENTS_VISBLOCKERMASK);
	return trace.fraction == 1;
}

static int Mod_Q1BSP_LightPoint_RecursiveBSPNode(dp_model_t *model, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const mnode_t *node, float x, float y, float startz, float endz)
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
			int i, dsi, dti, lmwidth, lmheight;
			float ds, dt;
			msurface_t *surface;
			unsigned char *lightmap;
			int maps, line3, size3;
			float dsfrac;
			float dtfrac;
			float scale, w, w00, w01, w10, w11;

			surface = model->data_surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surface++)
			{
				if (!(surface->texture->basematerialflags & MATERIALFLAG_WALL) || !surface->lightmapinfo || !surface->lightmapinfo->samples)
					continue;	// no lightmaps

				// location we want to sample in the lightmap
				ds = ((x * surface->lightmapinfo->texinfo->vecs[0][0] + y * surface->lightmapinfo->texinfo->vecs[0][1] + mid * surface->lightmapinfo->texinfo->vecs[0][2] + surface->lightmapinfo->texinfo->vecs[0][3]) - surface->lightmapinfo->texturemins[0]) * 0.0625f;
				dt = ((x * surface->lightmapinfo->texinfo->vecs[1][0] + y * surface->lightmapinfo->texinfo->vecs[1][1] + mid * surface->lightmapinfo->texinfo->vecs[1][2] + surface->lightmapinfo->texinfo->vecs[1][3]) - surface->lightmapinfo->texturemins[1]) * 0.0625f;

				// check the bounds
				dsi = (int)ds;
				dti = (int)dt;
				lmwidth = ((surface->lightmapinfo->extents[0]>>4)+1);
				lmheight = ((surface->lightmapinfo->extents[1]>>4)+1);

				// is it in bounds?
				if (dsi >= 0 && dsi < lmwidth-1 && dti >= 0 && dti < lmheight-1)
				{
					// calculate bilinear interpolation factors
					// and also multiply by fixedpoint conversion factors
					dsfrac = ds - dsi;
					dtfrac = dt - dti;
					w00 = (1 - dsfrac) * (1 - dtfrac) * (1.0f / 32768.0f);
					w01 = (    dsfrac) * (1 - dtfrac) * (1.0f / 32768.0f);
					w10 = (1 - dsfrac) * (    dtfrac) * (1.0f / 32768.0f);
					w11 = (    dsfrac) * (    dtfrac) * (1.0f / 32768.0f);

					// values for pointer math
					line3 = lmwidth * 3; // LordHavoc: *3 for colored lighting
					size3 = lmwidth * lmheight * 3; // LordHavoc: *3 for colored lighting

					// look up the pixel
					lightmap = surface->lightmapinfo->samples + dti * line3 + dsi*3; // LordHavoc: *3 for colored lighting

					// bilinear filter each lightmap style, and sum them
					for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++)
					{
						scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[maps]];
						w = w00 * scale;VectorMA(ambientcolor, w, lightmap            , ambientcolor);
						w = w01 * scale;VectorMA(ambientcolor, w, lightmap + 3        , ambientcolor);
						w = w10 * scale;VectorMA(ambientcolor, w, lightmap + line3    , ambientcolor);
						w = w11 * scale;VectorMA(ambientcolor, w, lightmap + line3 + 3, ambientcolor);
						lightmap += size3;
					}

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

void Mod_Q1BSP_LightPoint(dp_model_t *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal)
{
	// pretend lighting is coming down from above (due to lack of a lightgrid to know primary lighting direction)
	VectorSet(diffusenormal, 0, 0, 1);

	if (!model->brushq1.lightdata)
	{
		VectorSet(ambientcolor, 1, 1, 1);
		VectorSet(diffusecolor, 0, 0, 0);
		return;
	}

	Mod_Q1BSP_LightPoint_RecursiveBSPNode(model, ambientcolor, diffusecolor, diffusenormal, model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode, p[0], p[1], p[2] + 0.125, p[2] - 65536);
}

static const texture_t *Mod_Q1BSP_TraceLineAgainstSurfacesFindTextureOnNode(RecursiveHullCheckTraceInfo_t *t, const dp_model_t *model, const mnode_t *node, double mid[3])
{
	int i;
	int j;
	int k;
	const msurface_t *surface;
	float normal[3];
	float v0[3];
	float v1[3];
	float edgedir[3];
	float edgenormal[3];
	float p[4];
	float midf;
	float t1;
	float t2;
	VectorCopy(mid, p);
	p[3] = 1;
	surface = model->data_surfaces + node->firstsurface;
	for (i = 0;i < node->numsurfaces;i++, surface++)
	{
		// skip surfaces whose bounding box does not include the point
//		if (!BoxesOverlap(mid, mid, surface->mins, surface->maxs))
//			continue;
		// skip faces with contents we don't care about
		if (!(t->trace->hitsupercontentsmask & surface->texture->supercontents))
			continue;
		// get the surface normal - since it is flat we know any vertex normal will suffice
		VectorCopy(model->surfmesh.data_normal3f + 3 * surface->num_firstvertex, normal);
		// skip backfaces
		if (DotProduct(t->dist, normal) > 0)
			continue;
		// iterate edges and see if the point is outside one of them
		for (j = 0, k = surface->num_vertices - 1;j < surface->num_vertices;k = j, j++)
		{
			VectorCopy(model->surfmesh.data_vertex3f + 3 * (surface->num_firstvertex + k), v0);
			VectorCopy(model->surfmesh.data_vertex3f + 3 * (surface->num_firstvertex + j), v1);
			VectorSubtract(v0, v1, edgedir);
			CrossProduct(edgedir, normal, edgenormal);
			if (DotProduct(edgenormal, p) > DotProduct(edgenormal, v0))
				break;
		}
		// if the point is outside one of the edges, it is not within the surface
		if (j < surface->num_vertices)
			continue;

		// we hit a surface, this is the impact point...
		VectorCopy(normal, t->trace->plane.normal);
		t->trace->plane.dist = DotProduct(normal, p);

		// calculate the true fraction
		t1 = DotProduct(t->start, t->trace->plane.normal) - t->trace->plane.dist;
		t2 = DotProduct(t->end, t->trace->plane.normal) - t->trace->plane.dist;
		midf = t1 / (t1 - t2);
		t->trace->realfraction = midf;

		// calculate the return fraction which is nudged off the surface a bit
		midf = (t1 - DIST_EPSILON) / (t1 - t2);
		t->trace->fraction = bound(0, midf, 1);

		if (collision_prefernudgedfraction.integer)
			t->trace->realfraction = t->trace->fraction;

		t->trace->hittexture = surface->texture->currentframe;
		t->trace->hitq3surfaceflags = t->trace->hittexture->surfaceflags;
		t->trace->hitsupercontents = t->trace->hittexture->supercontents;
		return surface->texture->currentframe;
	}
	return NULL;
}

static int Mod_Q1BSP_TraceLineAgainstSurfacesRecursiveBSPNode(RecursiveHullCheckTraceInfo_t *t, const dp_model_t *model, const mnode_t *node, const double p1[3], const double p2[3])
{
	const mplane_t *plane;
	double t1, t2;
	int side;
	double midf, mid[3];
	const mleaf_t *leaf;

	while (node->plane)
	{
		plane = node->plane;
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

		// the line intersects, find intersection point
		// LordHavoc: this uses the original trace for maximum accuracy
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
		VectorMA(t->start, midf, t->dist, mid);

		// recurse both sides, front side first, return if we hit a surface
		if (Mod_Q1BSP_TraceLineAgainstSurfacesRecursiveBSPNode(t, model, node->children[side], p1, mid) == HULLCHECKSTATE_DONE)
			return HULLCHECKSTATE_DONE;

		// test each surface on the node
		Mod_Q1BSP_TraceLineAgainstSurfacesFindTextureOnNode(t, model, node, mid);
		if (t->trace->hittexture)
			return HULLCHECKSTATE_DONE;

		// recurse back side
		return Mod_Q1BSP_TraceLineAgainstSurfacesRecursiveBSPNode(t, model, node->children[side ^ 1], mid, p2);
	}
	leaf = (const mleaf_t *)node;
	side = Mod_Q1BSP_SuperContentsFromNativeContents(NULL, leaf->contents);
	if (!t->trace->startfound)
	{
		t->trace->startfound = true;
		t->trace->startsupercontents |= side;
	}
	if (side & SUPERCONTENTS_LIQUIDSMASK)
		t->trace->inwater = true;
	if (side == 0)
		t->trace->inopen = true;
	if (side & t->trace->hitsupercontentsmask)
	{
		// if the first leaf is solid, set startsolid
		if (t->trace->allsolid)
			t->trace->startsolid = true;
		return HULLCHECKSTATE_SOLID;
	}
	else
	{
		t->trace->allsolid = false;
		return HULLCHECKSTATE_EMPTY;
	}
}

static void Mod_Q1BSP_TraceLineAgainstSurfaces(struct model_s *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	RecursiveHullCheckTraceInfo_t rhc;

	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	rhc.trace = trace;
	rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
	rhc.trace->fraction = 1;
	rhc.trace->realfraction = 1;
	rhc.trace->allsolid = true;
	rhc.hull = &model->brushq1.hulls[0]; // 0x0x0
	VectorCopy(start, rhc.start);
	VectorCopy(end, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
	Mod_Q1BSP_TraceLineAgainstSurfacesRecursiveBSPNode(&rhc, model, model->brush.data_nodes + rhc.hull->firstclipnode, rhc.start, rhc.end);
	VectorMA(rhc.start, rhc.trace->fraction, rhc.dist, rhc.trace->endpos);
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
	int x, y;
	int w = width/2;
	int h = height;
	unsigned int *solidpixels = (unsigned int *)Mem_Alloc(tempmempool, w*h*sizeof(unsigned char[4]));
	unsigned int *alphapixels = (unsigned int *)Mem_Alloc(tempmempool, w*h*sizeof(unsigned char[4]));

	// allocate a texture pool if we need it
	if (loadmodel->texturepool == NULL && cls.state != ca_dedicated)
		loadmodel->texturepool = R_AllocTexturePool();

	if (bytesperpixel == 4)
	{
		for (y = 0;y < h;y++)
		{
			for (x = 0;x < w;x++)
			{
				solidpixels[y*w+x] = ((unsigned *)src)[y*width+x+w];
				alphapixels[y*w+x] = ((unsigned *)src)[y*width+x];
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
		bgra;
		r = g = b = 0;
		for (y = 0;y < h;y++)
		{
			for (x = 0;x < w;x++)
			{
				p = src[x*width+y+w];
				r += palette_rgb[p][0];
				g += palette_rgb[p][1];
				b += palette_rgb[p][2];
			}
		}
		bgra.b[2] = r/(w*h);
		bgra.b[1] = g/(w*h);
		bgra.b[0] = b/(w*h);
		bgra.b[3] = 0;
		for (y = 0;y < h;y++)
		{
			for (x = 0;x < w;x++)
			{
				solidpixels[y*w+x] = palette_bgra_complete[src[y*width+x+w]];
				p = src[y*width+x];
				alphapixels[y*w+x] = p ? palette_bgra_complete[p] : bgra.i;
			}
		}
	}

	loadmodel->brush.solidskyskinframe = R_SkinFrame_LoadInternalBGRA("sky_solidtexture", 0         , (unsigned char *) solidpixels, w, h, vid.sRGB3D);
	loadmodel->brush.alphaskyskinframe = R_SkinFrame_LoadInternalBGRA("sky_alphatexture", TEXF_ALPHA, (unsigned char *) alphapixels, w, h, vid.sRGB3D);
	Mem_Free(solidpixels);
	Mem_Free(alphapixels);
}

static void Mod_Q1BSP_LoadTextures(lump_t *l)
{
	int i, j, k, num, max, altmax, mtwidth, mtheight, *dofs, incomplete;
	skinframe_t *skinframe;
	miptex_t *dmiptex;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	texture_t backuptex;
	dmiptexlump_t *m;
	unsigned char *data, *mtdata;
	const char *s;
	char mapname[MAX_QPATH], name[MAX_QPATH];
	unsigned char zeroopaque[4], zerotrans[4];
	Vector4Set(zeroopaque, 0, 0, 0, 255);
	Vector4Set(zerotrans, 0, 0, 0, 128);

	loadmodel->data_textures = NULL;

	// add two slots for notexture walls and notexture liquids
	if (l->filelen)
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong (m->nummiptex);
		loadmodel->num_textures = m->nummiptex + 2;
		loadmodel->num_texturesperskin = loadmodel->num_textures;
	}
	else
	{
		m = NULL;
		loadmodel->num_textures = 2;
		loadmodel->num_texturesperskin = loadmodel->num_textures;
	}

	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_textures * sizeof(texture_t));

	// fill out all slots with notexture
	if (cls.state != ca_dedicated)
		skinframe = R_SkinFrame_LoadMissing();
	else
		skinframe = NULL;
	for (i = 0, tx = loadmodel->data_textures;i < loadmodel->num_textures;i++, tx++)
	{
		strlcpy(tx->name, "NO TEXTURE FOUND", sizeof(tx->name));
		tx->width = 16;
		tx->height = 16;
		if (cls.state != ca_dedicated)
		{
			tx->numskinframes = 1;
			tx->skinframerate = 1;
			tx->skinframes[0] = skinframe;
			tx->currentskinframe = tx->skinframes[0];
		}
		tx->basematerialflags = MATERIALFLAG_WALL;
		if (i == loadmodel->num_textures - 1)
		{
			tx->basematerialflags |= MATERIALFLAG_WATERSCROLL | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_NOSHADOW;
			tx->supercontents = mod_q1bsp_texture_water.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_water.surfaceflags;
		}
		else
		{
			tx->supercontents = mod_q1bsp_texture_solid.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_solid.surfaceflags;
		}
		tx->currentframe = tx;

		// clear water settings
		tx->reflectmin = 0;
		tx->reflectmax = 1;
		tx->refractfactor = 1;
		Vector4Set(tx->refractcolor4f, 1, 1, 1, 1);
		tx->reflectfactor = 1;
		Vector4Set(tx->reflectcolor4f, 1, 1, 1, 1);
		tx->r_water_wateralpha = 1;
		tx->offsetmapping = OFFSETMAPPING_DEFAULT;
		tx->offsetscale = 1;
		tx->specularscalemod = 1;
		tx->specularpowermod = 1;
	}

	if (!m)
	{
		Con_Printf("%s: no miptex lump to load textures from\n", loadmodel->name);
		return;
	}

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
		if (r_nosurftextures.integer)
			continue;
		if (dofs[i] == -1)
		{
			Con_DPrintf("%s: miptex #%i missing\n", loadmodel->name, i);
			continue;
		}
		dmiptex = (miptex_t *)((unsigned char *)m + dofs[i]);

		// copy name, but only up to 16 characters
		// (the output buffer can hold more than this, but the input buffer is
		//  only 16)
		for (j = 0;j < 16 && dmiptex->name[j];j++)
			name[j] = dmiptex->name[j];
		name[j] = 0;

		if (!name[0])
		{
			dpsnprintf(name, sizeof(name), "unnamed%i", i);
			Con_DPrintf("%s: warning: renaming unnamed texture to %s\n", loadmodel->name, name);
		}

		mtwidth = LittleLong(dmiptex->width);
		mtheight = LittleLong(dmiptex->height);
		mtdata = NULL;
		j = LittleLong(dmiptex->offsets[0]);
		if (j)
		{
			// texture included
			if (j < 40 || j + mtwidth * mtheight > l->filelen)
			{
				Con_Printf("%s: Texture \"%s\" is corrupt or incomplete\n", loadmodel->name, dmiptex->name);
				continue;
			}
			mtdata = (unsigned char *)dmiptex + j;
		}

		if ((mtwidth & 15) || (mtheight & 15))
			Con_DPrintf("%s: warning: texture \"%s\" is not 16 aligned\n", loadmodel->name, dmiptex->name);

		// LordHavoc: force all names to lowercase
		for (j = 0;name[j];j++)
			if (name[j] >= 'A' && name[j] <= 'Z')
				name[j] += 'a' - 'A';

		// LordHavoc: backup the texture_t because q3 shader loading overwrites it
		backuptex = loadmodel->data_textures[i];
		if (dmiptex->name[0] && Mod_LoadTextureFromQ3Shader(loadmodel->data_textures + i, name, false, false, 0))
			continue;
		loadmodel->data_textures[i] = backuptex;

		tx = loadmodel->data_textures + i;
		strlcpy(tx->name, name, sizeof(tx->name));
		tx->width = mtwidth;
		tx->height = mtheight;

		if (tx->name[0] == '*')
		{
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
		}
		else if (!strncmp(tx->name, "sky", 3))
		{
			tx->supercontents = mod_q1bsp_texture_sky.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_sky.surfaceflags;
		}
		else
		{
			tx->supercontents = mod_q1bsp_texture_solid.supercontents;
			tx->surfaceflags = mod_q1bsp_texture_solid.surfaceflags;
		}

		if (cls.state != ca_dedicated)
		{
			// LordHavoc: HL sky textures are entirely different than quake
			if (!loadmodel->brush.ishlbsp && !strncmp(tx->name, "sky", 3) && mtwidth == mtheight * 2)
			{
				data = loadimagepixelsbgra(gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s/%s", mapname, tx->name), false, false, false, NULL);
				if (!data)
					data = loadimagepixelsbgra(gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s", tx->name), false, false, false, NULL);
				if (data && image_width == image_height * 2)
				{
					R_Q1BSP_LoadSplitSky(data, image_width, image_height, 4);
					Mem_Free(data);
				}
				else if (mtdata != NULL)
					R_Q1BSP_LoadSplitSky(mtdata, mtwidth, mtheight, 1);
			}
			else
			{
				skinframe = R_SkinFrame_LoadExternal(gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s/%s", mapname, tx->name), TEXF_ALPHA | TEXF_MIPMAP | TEXF_ISWORLD | TEXF_PICMIP | TEXF_COMPRESS, false);
				if (!skinframe)
					skinframe = R_SkinFrame_LoadExternal(gamemode == GAME_TENEBRAE ? tx->name : va("textures/%s", tx->name), TEXF_ALPHA | TEXF_MIPMAP | TEXF_ISWORLD | TEXF_PICMIP | TEXF_COMPRESS, false);
				if (skinframe)
					tx->offsetmapping = OFFSETMAPPING_DEFAULT; // allow offsetmapping on external textures without a q3 shader
				if (!skinframe)
				{
					// did not find external texture, load it from the bsp or wad3
					if (loadmodel->brush.ishlbsp)
					{
						// internal texture overrides wad
						unsigned char *pixels, *freepixels;
						pixels = freepixels = NULL;
						if (mtdata)
							pixels = W_ConvertWAD3TextureBGRA(dmiptex);
						if (pixels == NULL)
							pixels = freepixels = W_GetTextureBGRA(tx->name);
						if (pixels != NULL)
						{
							tx->width = image_width;
							tx->height = image_height;
							skinframe = R_SkinFrame_LoadInternalBGRA(tx->name, TEXF_ALPHA | TEXF_MIPMAP | TEXF_ISWORLD | TEXF_PICMIP, pixels, image_width, image_height, true);
						}
						if (freepixels)
							Mem_Free(freepixels);
					}
					else if (mtdata) // texture included
						skinframe = R_SkinFrame_LoadInternalQuake(tx->name, TEXF_MIPMAP | TEXF_ISWORLD | TEXF_PICMIP, false, r_fullbrights.integer, mtdata, tx->width, tx->height);
				}
				// if skinframe is still NULL the "missing" texture will be used
				if (skinframe)
					tx->skinframes[0] = skinframe;
			}
			// LordHavoc: some Tenebrae textures get replaced by black
			if (!strncmp(tx->name, "*glassmirror", 12)) // Tenebrae
				tx->skinframes[0] = R_SkinFrame_LoadInternalBGRA(tx->name, TEXF_MIPMAP | TEXF_ALPHA, zerotrans, 1, 1, false);
			else if (!strncmp(tx->name, "mirror", 6)) // Tenebrae
				tx->skinframes[0] = R_SkinFrame_LoadInternalBGRA(tx->name, 0, zeroopaque, 1, 1, false);
		}

		tx->basematerialflags = MATERIALFLAG_WALL;
		if (tx->name[0] == '*')
		{
			// LordHavoc: some turbulent textures should not be affected by wateralpha
			if (!strncmp(tx->name, "*glassmirror", 12)) // Tenebrae
				tx->basematerialflags |= MATERIALFLAG_NOSHADOW | MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_REFLECTION;
			else if (!strncmp(tx->name,"*lava",5)
			 || !strncmp(tx->name,"*teleport",9)
			 || !strncmp(tx->name,"*rift",5)) // Scourge of Armagon texture
				tx->basematerialflags |= MATERIALFLAG_WATERSCROLL | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_NOSHADOW;
			else
				tx->basematerialflags |= MATERIALFLAG_WATERSCROLL | MATERIALFLAG_LIGHTBOTHSIDES | MATERIALFLAG_NOSHADOW | MATERIALFLAG_WATERALPHA | MATERIALFLAG_WATERSHADER;
			if (tx->skinframes[0] && tx->skinframes[0]->hasalpha)
				tx->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
		}
		else if (!strncmp(tx->name, "mirror", 6)) // Tenebrae
		{
			// replace the texture with black
			tx->basematerialflags |= MATERIALFLAG_REFLECTION;
		}
		else if (!strncmp(tx->name, "sky", 3))
			tx->basematerialflags = MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
		else if (!strcmp(tx->name, "caulk"))
			tx->basematerialflags = MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
		else if (tx->skinframes[0] && tx->skinframes[0]->hasalpha)
			tx->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;

		// start out with no animation
		tx->currentframe = tx;
		tx->currentskinframe = tx->skinframes[0];
		tx->currentmaterialflags = tx->basematerialflags;
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
					if (developer_loading.integer)
						Con_Printf("loaded %s\n", litfilename);
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
								if (developer_loading.integer)
									Con_Printf("loaded %s\n", dlitfilename);
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
	int i, j, k;
	if (!data)
		return;
	if (!COM_ParseToken_Simple(&data, false, false))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseToken_Simple(&data, false, false))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strlcpy(key, com_token + 1, sizeof(key));
		else
			strlcpy(key, com_token, sizeof(key));
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseToken_Simple(&data, false, false))
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
							W_LoadTextureWadFile(&value[j], false);
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
	loadmodel->brush.entities = (char *)Mem_Alloc(loadmodel->mempool, l->filelen + 1);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	loadmodel->brush.entities[l->filelen] = 0;
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
			if(!loadmodel->brushq1.numvertexes)
				Host_Error("Mod_Q1BSP_LoadEdges: %s has edges but no vertexes, cannot fix\n", loadmodel->name);
				
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
			if (out->texture == NULL)
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

extern cvar_t gl_max_lightmapsize;
static void Mod_Q1BSP_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *surface;
	int i, j, count, surfacenum, planenum, smax, tmax, ssize, tsize, firstedge, numedges, totalverts, totaltris, lightmapnumber, lightmapsize, totallightmapsamples;
	float texmins[2], texmaxs[2], val;
	rtexture_t *lightmaptexture, *deluxemaptexture;

	in = (dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	loadmodel->data_surfaces = (msurface_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_t));
	loadmodel->data_surfaces_lightmapinfo = (msurface_lightmapinfo_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_lightmapinfo_t));

	loadmodel->num_surfaces = count;

	loadmodel->brushq1.firstrender = true;
	loadmodel->brushq1.lightmapupdateflags = (unsigned char *)Mem_Alloc(loadmodel->mempool, count*sizeof(unsigned char));

	totalverts = 0;
	totaltris = 0;
	for (surfacenum = 0, in = (dface_t *)(mod_base + l->fileofs);surfacenum < count;surfacenum++, in++)
	{
		numedges = (unsigned short)LittleShort(in->numedges);
		totalverts += numedges;
		totaltris += numedges - 2;
	}

	Mod_AllocSurfMesh(loadmodel->mempool, totalverts, totaltris, true, false, false);

	lightmaptexture = NULL;
	deluxemaptexture = r_texture_blanknormalmap;
	lightmapnumber = 0;
	lightmapsize = bound(256, gl_max_lightmapsize.integer, (int)vid.maxtexturesize_2d);
	totallightmapsamples = 0;

	totalverts = 0;
	totaltris = 0;
	for (surfacenum = 0, in = (dface_t *)(mod_base + l->fileofs), surface = loadmodel->data_surfaces;surfacenum < count;surfacenum++, in++, surface++)
	{
		surface->lightmapinfo = loadmodel->data_surfaces_lightmapinfo + surfacenum;

		// FIXME: validate edges, texinfo, etc?
		firstedge = LittleLong(in->firstedge);
		numedges = (unsigned short)LittleShort(in->numedges);
		if ((unsigned int) firstedge > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) firstedge + (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid edge range (firstedge %i, numedges %i, model edges %i)", firstedge, numedges, loadmodel->brushq1.numsurfedges);
		i = (unsigned short)LittleShort(in->texinfo);
		if ((unsigned int) i >= (unsigned int) loadmodel->brushq1.numtexinfo)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid texinfo index %i(model has %i texinfos)", i, loadmodel->brushq1.numtexinfo);
		surface->lightmapinfo->texinfo = loadmodel->brushq1.texinfo + i;
		surface->texture = surface->lightmapinfo->texinfo->texture;

		planenum = (unsigned short)LittleShort(in->planenum);
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
			// note: the q1bsp format does not allow a 0 surfedge (it would have no negative counterpart)
			if (lindex >= 0)
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
		Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, loadmodel->surfmesh.data_vertex3f, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle), loadmodel->surfmesh.data_normal3f, r_smoothnormals_areaweighting.integer != 0);
		Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_texcoordtexture2f, loadmodel->surfmesh.data_normal3f, (loadmodel->surfmesh.data_element3i + 3 * surface->num_firsttriangle), loadmodel->surfmesh.data_svector3f, loadmodel->surfmesh.data_tvector3f, r_smoothnormals_areaweighting.integer != 0);
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
#if 1
			// give non-lightmapped water a 1x white lightmap
			if (surface->texture->name[0] == '*' && (surface->lightmapinfo->texinfo->flags & TEX_SPECIAL) && ssize <= 256 && tsize <= 256)
			{
				surface->lightmapinfo->samples = (unsigned char *)Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
				surface->lightmapinfo->styles[0] = 0;
				memset(surface->lightmapinfo->samples, 128, ssize * tsize * 3);
			}
#endif
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
			if (ssize > 256 || tsize > 256)
				Host_Error("Bad surface extents");

			if (lightmapsize < ssize)
				lightmapsize = ssize;
			if (lightmapsize < tsize)
				lightmapsize = tsize;

			totallightmapsamples += ssize*tsize;

			// force lightmap upload on first time seeing the surface
			//
			// additionally this is used by the later code to see if a
			// lightmap is needed on this surface (rather than duplicating the
			// logic above)
			loadmodel->brushq1.lightmapupdateflags[surfacenum] = true;
			loadmodel->lit = true;
		}
	}

	// small maps (such as ammo boxes especially) don't need big lightmap
	// textures, so this code tries to guess a good size based on
	// totallightmapsamples (size of the lightmaps lump basically), as well as
	// trying to max out the size if there is a lot of lightmap data to store
	// additionally, never choose a lightmapsize that is smaller than the
	// largest surface encountered (as it would fail)
	i = lightmapsize;
	for (lightmapsize = 64; (lightmapsize < i) && (lightmapsize < bound(128, gl_max_lightmapsize.integer, (int)vid.maxtexturesize_2d)) && (totallightmapsamples > lightmapsize*lightmapsize); lightmapsize*=2)
		;

	// now that we've decided the lightmap texture size, we can do the rest
	if (cls.state != ca_dedicated)
	{
		int stainmapsize = 0;
		mod_alloclightmap_state_t allocState;

		Mod_AllocLightmap_Init(&allocState, lightmapsize, lightmapsize);
		for (surfacenum = 0, surface = loadmodel->data_surfaces;surfacenum < count;surfacenum++, surface++)
		{
			int i, iu, iv, lightmapx = 0, lightmapy = 0;
			float u, v, ubase, vbase, uscale, vscale;

			if (!loadmodel->brushq1.lightmapupdateflags[surfacenum])
				continue;

			smax = surface->lightmapinfo->extents[0] >> 4;
			tmax = surface->lightmapinfo->extents[1] >> 4;
			ssize = (surface->lightmapinfo->extents[0] >> 4) + 1;
			tsize = (surface->lightmapinfo->extents[1] >> 4) + 1;
			stainmapsize += ssize * tsize * 3;

			if (!lightmaptexture || !Mod_AllocLightmap_Block(&allocState, ssize, tsize, &lightmapx, &lightmapy))
			{
				// allocate a texture pool if we need it
				if (loadmodel->texturepool == NULL)
					loadmodel->texturepool = R_AllocTexturePool();
				// could not find room, make a new lightmap
				loadmodel->brushq3.num_mergedlightmaps = lightmapnumber + 1;
				loadmodel->brushq3.data_lightmaps = (rtexture_t **)Mem_Realloc(loadmodel->mempool, loadmodel->brushq3.data_lightmaps, loadmodel->brushq3.num_mergedlightmaps * sizeof(loadmodel->brushq3.data_lightmaps[0]));
				loadmodel->brushq3.data_deluxemaps = (rtexture_t **)Mem_Realloc(loadmodel->mempool, loadmodel->brushq3.data_deluxemaps, loadmodel->brushq3.num_mergedlightmaps * sizeof(loadmodel->brushq3.data_deluxemaps[0]));
				loadmodel->brushq3.data_lightmaps[lightmapnumber] = lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%i", lightmapnumber), lightmapsize, lightmapsize, NULL, TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_ALLOWUPDATES, -1, NULL);
				if (loadmodel->brushq1.nmaplightdata)
					loadmodel->brushq3.data_deluxemaps[lightmapnumber] = deluxemaptexture = R_LoadTexture2D(loadmodel->texturepool, va("deluxemap%i", lightmapnumber), lightmapsize, lightmapsize, NULL, TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_ALLOWUPDATES, -1, NULL);
				lightmapnumber++;
				Mod_AllocLightmap_Reset(&allocState);
				Mod_AllocLightmap_Block(&allocState, ssize, tsize, &lightmapx, &lightmapy);
			}
			surface->lightmaptexture = lightmaptexture;
			surface->deluxemaptexture = deluxemaptexture;
			surface->lightmapinfo->lightmaporigin[0] = lightmapx;
			surface->lightmapinfo->lightmaporigin[1] = lightmapy;

			uscale = 1.0f / (float)lightmapsize;
			vscale = 1.0f / (float)lightmapsize;
			ubase = lightmapx * uscale;
			vbase = lightmapy * vscale;

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

		if (cl_stainmaps.integer)
		{
			// allocate stainmaps for permanent marks on walls and clear white
			unsigned char *stainsamples = NULL;
			stainsamples = (unsigned char *)Mem_Alloc(loadmodel->mempool, stainmapsize);
			memset(stainsamples, 255, stainmapsize);
			// assign pointers
			for (surfacenum = 0, surface = loadmodel->data_surfaces;surfacenum < count;surfacenum++, surface++)
			{
				if (!loadmodel->brushq1.lightmapupdateflags[surfacenum])
					continue;
				ssize = (surface->lightmapinfo->extents[0] >> 4) + 1;
				tsize = (surface->lightmapinfo->extents[1] >> 4) + 1;
				surface->lightmapinfo->stainsamples = stainsamples;
				stainsamples += ssize * tsize * 3;
			}
		}
	}

	// generate ushort elements array if possible
	if (loadmodel->surfmesh.data_element3s)
		for (i = 0;i < loadmodel->surfmesh.num_triangles*3;i++)
			loadmodel->surfmesh.data_element3s[i] = loadmodel->surfmesh.data_element3i[i];
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
	if (count == 0)
		Host_Error("Mod_Q1BSP_LoadNodes: missing BSP tree in %s",loadmodel->name);
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

		out->firstsurface = (unsigned short)LittleShort(in->firstface);
		out->numsurfaces = (unsigned short)LittleShort(in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			// LordHavoc: this code supports broken bsp files produced by
			// arguire qbsp which can produce more than 32768 nodes, any value
			// below count is assumed to be a node number, any other value is
			// assumed to be a leaf number
			p = (unsigned short)LittleShort(in->children[j]);
			if (p < count)
			{
				if (p < loadmodel->brush.num_nodes)
					out->children[j] = loadmodel->brush.data_nodes + p;
				else
				{
					Con_Printf("Mod_Q1BSP_LoadNodes: invalid node index %i (file has only %i nodes)\n", p, loadmodel->brush.num_nodes);
					// map it to the solid leaf
					out->children[j] = (mnode_t *)loadmodel->brush.data_leafs;
				}
			}
			else
			{
				// note this uses 65535 intentionally, -1 is leaf 0
				p = 65535 - p;
				if (p < loadmodel->brush.num_leafs)
					out->children[j] = (mnode_t *)(loadmodel->brush.data_leafs + p);
				else
				{
					Con_Printf("Mod_Q1BSP_LoadNodes: invalid leaf index %i (file has only %i leafs)\n", p, loadmodel->brush.num_leafs);
					// map it to the solid leaf
					out->children[j] = (mnode_t *)loadmodel->brush.data_leafs;
				}
			}
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

		out->firstleafsurface = loadmodel->brush.data_leafsurfaces + (unsigned short)LittleShort(in->firstmarksurface);
		out->numleafsurfaces = (unsigned short)LittleShort(in->nummarksurfaces);
		if ((unsigned short)LittleShort(in->firstmarksurface) + out->numleafsurfaces > loadmodel->brush.num_leafsurfaces)
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
	// if there's no vis data, assume supported (because everything is visible all the time)
	if (!loadmodel->brush.data_pvsclusters)
		return true;
	// check all liquid leafs to see if they can see into empty leafs, if any
	// can we can assume this map supports r_wateralpha
	for (i = 0, leaf = loadmodel->brush.data_leafs;i < loadmodel->brush.num_leafs;i++, leaf++)
	{
		if ((leaf->contents == CONTENTS_WATER || leaf->contents == CONTENTS_SLIME) && leaf->clusterindex >= 0)
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
	dclipnode_t *in;
	mclipnode_t *out;
	int			i, count;
	hull_t		*hull;

	in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadClipnodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (mclipnode_t *)Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.clipnodes = out;
	loadmodel->brushq1.numclipnodes = count;

	for (i = 1; i < MAX_MAP_HULLS; i++)
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
		// LordHavoc: this code supports arguire qbsp's broken clipnodes indices (more than 32768 clipnodes), values above count are assumed to be contents values
		out->children[0] = (unsigned short)LittleShort(in->children[0]);
		out->children[1] = (unsigned short)LittleShort(in->children[1]);
		if (out->children[0] >= count)
			out->children[0] -= 65536;
		if (out->children[1] >= count)
			out->children[1] -= 65536;
		if (out->planenum < 0 || out->planenum >= loadmodel->brush.num_planes)
			Host_Error("Corrupt clipping hull(out of range planenum)");
	}
}

//Duplicate the drawing hull structure as a clipping hull
static void Mod_Q1BSP_MakeHull0(void)
{
	mnode_t		*in;
	mclipnode_t *out;
	int			i;
	hull_t		*hull;

	hull = &loadmodel->brushq1.hulls[0];

	in = loadmodel->brush.data_nodes;
	out = (mclipnode_t *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_nodes * sizeof(*out));

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
		j = (unsigned short) LittleShort(in[i]);
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
	if (!COM_ParseToken_Simple(&data, false, false))
		return; // error
	submodel = 0;
	for (;;)
	{
		if (!COM_ParseToken_Simple(&data, false, false))
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
			if (!COM_ParseToken_Simple(&data, false, false))
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
					if (!COM_ParseToken_Simple(&data, false, false))
						return; // error
					if (com_token[0] == '}')
						break; // end of brush
					// each brush face should be this format:
					// ( x y z ) ( x y z ) ( x y z ) texture scroll_s scroll_t rotateangle scale_s scale_t
					// FIXME: support hl .map format
					for (pointnum = 0;pointnum < 3;pointnum++)
					{
						COM_ParseToken_Simple(&data, false, false);
						for (componentnum = 0;componentnum < 3;componentnum++)
						{
							COM_ParseToken_Simple(&data, false, false);
							point[pointnum][componentnum] = atof(com_token);
						}
						COM_ParseToken_Simple(&data, false, false);
					}
					COM_ParseToken_Simple(&data, false, false);
					strlcpy(facetexture, com_token, sizeof(facetexture));
					COM_ParseToken_Simple(&data, false, false);
					//scroll_s = atof(com_token);
					COM_ParseToken_Simple(&data, false, false);
					//scroll_t = atof(com_token);
					COM_ParseToken_Simple(&data, false, false);
					//rotate = atof(com_token);
					COM_ParseToken_Simple(&data, false, false);
					//scale_s = atof(com_token);
					COM_ParseToken_Simple(&data, false, false);
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

static memexpandablearray_t portalarray;

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
	int i, j, numportals, numpoints, portalindex, portalrange = Mem_ExpandableArray_IndexRange(&portalarray);
	portal_t *p;
	mportal_t *portal;
	mvertex_t *point;
	mleaf_t *leaf, *endleaf;

	// tally up portal and point counts and recalculate bounding boxes for all
	// leafs (because qbsp is very sloppy)
	leaf = loadmodel->brush.data_leafs;
	endleaf = leaf + loadmodel->brush.num_leafs;
	if (mod_recalculatenodeboxes.integer)
	{
		for (;leaf < endleaf;leaf++)
		{
			VectorSet(leaf->mins,  2000000000,  2000000000,  2000000000);
			VectorSet(leaf->maxs, -2000000000, -2000000000, -2000000000);
		}
	}
	numportals = 0;
	numpoints = 0;
	for (portalindex = 0;portalindex < portalrange;portalindex++)
	{
		p = (portal_t*)Mem_ExpandableArray_RecordAtIndex(&portalarray, portalindex);
		if (!p)
			continue;
		// note: this check must match the one below or it will usually corrupt memory
		// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
		if (p->numpoints >= 3 && p->nodes[0] != p->nodes[1] && ((mleaf_t *)p->nodes[0])->clusterindex >= 0 && ((mleaf_t *)p->nodes[1])->clusterindex >= 0)
		{
			numportals += 2;
			numpoints += p->numpoints * 2;
		}
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
	for (portalindex = 0;portalindex < portalrange;portalindex++)
	{
		p = (portal_t*)Mem_ExpandableArray_RecordAtIndex(&portalarray, portalindex);
		if (!p)
			continue;
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
			if (mod_recalculatenodeboxes.integer)
			{
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
		}
	}
	// now recalculate the node bounding boxes from the leafs
	if (mod_recalculatenodeboxes.integer)
		Mod_Q1BSP_RecursiveRecalcNodeBBox(loadmodel->brush.data_nodes + loadmodel->brushq1.hulls[0].firstclipnode);
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
static double *portalpointsbuffer;
static int portalpointsbufferoffset;
static int portalpointsbuffersize;
static void Mod_Q1BSP_RecursiveNodePortals(mnode_t *node)
{
	int i, side;
	mnode_t *front, *back, *other_node;
	mplane_t clipplane, *plane;
	portal_t *portal, *nextportal, *nodeportal, *splitportal, *temp;
	int numfrontpoints, numbackpoints;
	double *frontpoints, *backpoints;

	// if a leaf, we're done
	if (!node->plane)
		return;

	// get some space for our clipping operations to use
	if (portalpointsbuffersize < portalpointsbufferoffset + 6*MAX_PORTALPOINTS)
	{
		portalpointsbuffersize = portalpointsbufferoffset * 2;
		portalpointsbuffer = (double *)Mem_Realloc(loadmodel->mempool, portalpointsbuffer, portalpointsbuffersize * sizeof(*portalpointsbuffer));
	}
	frontpoints = portalpointsbuffer + portalpointsbufferoffset;
	portalpointsbufferoffset += 3*MAX_PORTALPOINTS;
	backpoints = portalpointsbuffer + portalpointsbufferoffset;
	portalpointsbufferoffset += 3*MAX_PORTALPOINTS;

	plane = node->plane;

	front = node->children[0];
	back = node->children[1];
	if (front == back)
		Host_Error("Mod_Q1BSP_RecursiveNodePortals: corrupt node hierarchy");

	// create the new portal by generating a polygon for the node plane,
	// and clipping it by all of the other portals(which came from nodes above this one)
	nodeportal = (portal_t *)Mem_ExpandableArray_AllocRecord(&portalarray);
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
		splitportal = (portal_t *)Mem_ExpandableArray_AllocRecord(&portalarray);
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

	portalpointsbufferoffset -= 6*MAX_PORTALPOINTS;
}

static void Mod_Q1BSP_MakePortals(void)
{
	Mem_ExpandableArray_NewArray(&portalarray, loadmodel->mempool, sizeof(portal_t), 1020*1024/sizeof(portal_t));
	portalpointsbufferoffset = 0;
	portalpointsbuffersize = 6*MAX_PORTALPOINTS*128;
	portalpointsbuffer = (double *)Mem_Alloc(loadmodel->mempool, portalpointsbuffersize * sizeof(*portalpointsbuffer));
	Mod_Q1BSP_RecursiveNodePortals(loadmodel->brush.data_nodes + loadmodel->brushq1.hulls[0].firstclipnode);
	Mem_Free(portalpointsbuffer);
	portalpointsbuffer = NULL;
	portalpointsbufferoffset = 0;
	portalpointsbuffersize = 0;
	Mod_Q1BSP_FinalizePortals();
	Mem_ExpandableArray_FreeArray(&portalarray);
}

//Returns PVS data for a given point
//(note: can return NULL)
static unsigned char *Mod_Q1BSP_GetPVS(dp_model_t *model, const vec3_t p)
{
	mnode_t *node;
	node = model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->plane)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];
	if (((mleaf_t *)node)->clusterindex >= 0)
		return model->brush.data_pvsclusters + ((mleaf_t *)node)->clusterindex * model->brush.num_pvsclusterbytes;
	else
		return NULL;
}

static void Mod_Q1BSP_FatPVS_RecursiveBSPNode(dp_model_t *model, const vec3_t org, vec_t radius, unsigned char *pvsbuffer, int pvsbytes, mnode_t *node)
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
static int Mod_Q1BSP_FatPVS(dp_model_t *model, const vec3_t org, vec_t radius, unsigned char *pvsbuffer, int pvsbufferlength, qboolean merge)
{
	int bytes = model->brush.num_pvsclusterbytes;
	bytes = min(bytes, pvsbufferlength);
	if (r_novis.integer || r_trippy.integer || !model->brush.num_pvsclusters || !Mod_Q1BSP_GetPVS(model, org))
	{
		memset(pvsbuffer, 0xFF, bytes);
		return bytes;
	}
	if (!merge)
		memset(pvsbuffer, 0, bytes);
	Mod_Q1BSP_FatPVS_RecursiveBSPNode(model, org, radius, pvsbuffer, bytes, model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode);
	return bytes;
}

static void Mod_Q1BSP_RoundUpToHullSize(dp_model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs)
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

static int Mod_Q1BSP_CreateShadowMesh(dp_model_t *mod)
{
	int j;
	int numshadowmeshtriangles = 0;
	msurface_t *surface;
	if (cls.state == ca_dedicated)
		return 0;
	// make a single combined shadow mesh to allow optimized shadow volume creation

	for (j = 0, surface = mod->data_surfaces;j < mod->num_surfaces;j++, surface++)
	{
		surface->num_firstshadowmeshtriangle = numshadowmeshtriangles;
		numshadowmeshtriangles += surface->num_triangles;
	}
	mod->brush.shadowmesh = Mod_ShadowMesh_Begin(mod->mempool, numshadowmeshtriangles * 3, numshadowmeshtriangles, NULL, NULL, NULL, false, false, true);
	for (j = 0, surface = mod->data_surfaces;j < mod->num_surfaces;j++, surface++)
		if (surface->num_triangles > 0)
			Mod_ShadowMesh_AddMesh(mod->mempool, mod->brush.shadowmesh, NULL, NULL, NULL, mod->surfmesh.data_vertex3f, NULL, NULL, NULL, NULL, surface->num_triangles, (mod->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
	mod->brush.shadowmesh = Mod_ShadowMesh_Finish(mod->mempool, mod->brush.shadowmesh, false, r_enableshadowvolumes.integer != 0, false);
	if (mod->brush.shadowmesh && mod->brush.shadowmesh->neighbor3i)
		Mod_BuildTriangleNeighbors(mod->brush.shadowmesh->neighbor3i, mod->brush.shadowmesh->element3i, mod->brush.shadowmesh->numtriangles);

	return numshadowmeshtriangles;
}

void Mod_CollisionBIH_TraceLineAgainstSurfaces(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask);

void Mod_Q1BSP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int i, j, k;
	dheader_t *header;
	dmodel_t *bm;
	float dist, modelyawradius, modelradius;
	msurface_t *surface;
	hullinfo_t hullinfo;
	int totalstylesurfaces, totalstyles, stylecounts[256], remapstyles[256];
	model_brush_lightstyleinfo_t styleinfo[256];
	unsigned char *datapointer;

	mod->modeldatatypestring = "Q1BSP";

	mod->type = mod_brushq1;

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != BSPVERSION && i != 30)
		Host_Error("Mod_Q1BSP_Load: %s has wrong version number(%i should be %i(Quake) or 30(HalfLife)", mod->name, i, BSPVERSION);
	mod->brush.ishlbsp = i == 30;

// fill in hull info
	VectorClear (hullinfo.hullsizes[0][0]);
	VectorClear (hullinfo.hullsizes[0][1]);
	if (mod->brush.ishlbsp)
	{
		mod->modeldatatypestring = "HLBSP";

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

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q1BSP_TraceBox;
	if (sv_gameplayfix_q1bsptracelinereportstexture.integer)
		mod->TraceLine = Mod_Q1BSP_TraceLineAgainstSurfaces; // LordHavoc: use the surface-hitting version of TraceLine in all cases
	else
		mod->TraceLine = Mod_Q1BSP_TraceLine;
	mod->TracePoint = Mod_Q1BSP_TracePoint;
	mod->PointSuperContents = Mod_Q1BSP_PointSuperContents;
	mod->TraceLineAgainstSurfaces = Mod_Q1BSP_TraceLineAgainstSurfaces;
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
	mod->Draw = R_Q1BSP_Draw;
	mod->DrawDepth = R_Q1BSP_DrawDepth;
	mod->DrawDebug = R_Q1BSP_DrawDebug;
	mod->DrawPrepass = R_Q1BSP_DrawPrepass;
	mod->GetLightInfo = R_Q1BSP_GetLightInfo;
	mod->CompileShadowMap = R_Q1BSP_CompileShadowMap;
	mod->DrawShadowMap = R_Q1BSP_DrawShadowMap;
	mod->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	mod->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	mod->DrawLight = R_Q1BSP_DrawLight;

// load into heap

	mod->brush.qw_md4sum = 0;
	mod->brush.qw_md4sum2 = 0;
	for (i = 0;i < HEADER_LUMPS;i++)
	{
		int temp;
		if (i == LUMP_ENTITIES)
			continue;
		temp = Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		mod->brush.qw_md4sum ^= LittleLong(temp);
		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;
		temp = Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		mod->brush.qw_md4sum2 ^= LittleLong(temp);
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

	if (mod->brushq1.data_compressedpvs)
		Mem_Free(mod->brushq1.data_compressedpvs);
	mod->brushq1.data_compressedpvs = NULL;
	mod->brushq1.num_compressedpvs = 0;

	Mod_Q1BSP_MakeHull0();
	if (mod_bsp_portalize.integer)
		Mod_Q1BSP_MakePortals();

	mod->numframes = 2;		// regular and alternate animation
	mod->numskins = 1;

	// make a single combined shadow mesh to allow optimized shadow volume creation
	Mod_Q1BSP_CreateShadowMesh(loadmodel);

	if (loadmodel->brush.numsubmodels)
		loadmodel->brush.submodels = (dp_model_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brush.numsubmodels * sizeof(dp_model_t *));

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
	totalstylesurfaces = 0;
	totalstyles = 0;
	for (i = 0;i < mod->brush.numsubmodels;i++)
	{
		memset(stylecounts, 0, sizeof(stylecounts));
		for (k = 0;k < mod->brushq1.submodels[i].numfaces;k++)
		{
			surface = mod->data_surfaces + mod->brushq1.submodels[i].firstface + k;
			for (j = 0;j < MAXLIGHTMAPS;j++)
				stylecounts[surface->lightmapinfo->styles[j]]++;
		}
		for (k = 0;k < 255;k++)
		{
			totalstyles++;
			if (stylecounts[k])
				totalstylesurfaces += stylecounts[k];
		}
	}
	datapointer = (unsigned char *)Mem_Alloc(mod->mempool, mod->num_surfaces * sizeof(int) + totalstyles * sizeof(model_brush_lightstyleinfo_t) + totalstylesurfaces * sizeof(int *));
	for (i = 0;i < mod->brush.numsubmodels;i++)
	{
		// LordHavoc: this code was originally at the end of this loop, but
		// has been transformed to something more readable at the start here.

		if (i > 0)
		{
			char name[10];
			// duplicate the basic information
			dpsnprintf(name, sizeof(name), "*%i", i);
			mod = Mod_FindName(name, loadmodel->name);
			// copy the base model to this one
			*mod = *loadmodel;
			// rename the clone back to its proper name
			strlcpy(mod->name, name, sizeof(mod->name));
			mod->brush.parentmodel = loadmodel;
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
			mod->brush.GetPVS = NULL;
			mod->brush.FatPVS = NULL;
			mod->brush.BoxTouchingPVS = NULL;
			mod->brush.BoxTouchingLeafPVS = NULL;
			mod->brush.BoxTouchingVisibleLeafs = NULL;
			mod->brush.FindBoxClusters = NULL;
			mod->brush.LightPoint = NULL;
			mod->brush.AmbientSoundLevelsForPoint = NULL;
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

		// set node/leaf parents for this submodel
		Mod_Q1BSP_LoadNodes_RecursiveSetParent(mod->brush.data_nodes + mod->brushq1.hulls[0].firstclipnode, NULL);

		// make the model surface list (used by shadowing/lighting)
		mod->sortedmodelsurfaces = (int *)datapointer;datapointer += mod->nummodelsurfaces * sizeof(int);
		Mod_MakeSortedSurfaces(mod);

		// copy the submodel bounds, then enlarge the yaw and rotated bounds according to radius
		// (previously this code measured the radius of the vertices of surfaces in the submodel, but that broke submodels that contain only CLIP brushes, which do not produce surfaces)
		VectorCopy(bm->mins, mod->normalmins);
		VectorCopy(bm->maxs, mod->normalmaxs);
		dist = max(fabs(mod->normalmins[0]), fabs(mod->normalmaxs[0]));
		modelyawradius = max(fabs(mod->normalmins[1]), fabs(mod->normalmaxs[1]));
		modelyawradius = dist*dist+modelyawradius*modelyawradius;
		modelradius = max(fabs(mod->normalmins[2]), fabs(mod->normalmaxs[2]));
		modelradius = modelyawradius + modelradius * modelradius;
		modelyawradius = sqrt(modelyawradius);
		modelradius = sqrt(modelradius);
		mod->yawmins[0] = mod->yawmins[1] = -modelyawradius;
		mod->yawmins[2] = mod->normalmins[2];
		mod->yawmaxs[0] = mod->yawmaxs[1] =  modelyawradius;
		mod->yawmaxs[2] = mod->normalmaxs[2];
		mod->rotatedmins[0] = mod->rotatedmins[1] = mod->rotatedmins[2] = -modelradius;
		mod->rotatedmaxs[0] = mod->rotatedmaxs[1] = mod->rotatedmaxs[2] =  modelradius;
		mod->radius = modelradius;
		mod->radius2 = modelradius * modelradius;

		// this gets altered below if sky or water is used
		mod->DrawSky = NULL;
		mod->DrawAddWaterPlanes = NULL;

		// scan surfaces for sky and water and flag the submodel as possessing these features or not
		// build lightstyle lists for quick marking of dirty lightmaps when lightstyles flicker
		if (mod->nummodelsurfaces)
		{
			for (j = 0, surface = &mod->data_surfaces[mod->firstmodelsurface];j < mod->nummodelsurfaces;j++, surface++)
				if (surface->texture->basematerialflags & MATERIALFLAG_SKY)
					break;
			if (j < mod->nummodelsurfaces)
				mod->DrawSky = R_Q1BSP_DrawSky;

			for (j = 0, surface = &mod->data_surfaces[mod->firstmodelsurface];j < mod->nummodelsurfaces;j++, surface++)
				if (surface->texture->basematerialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA))
					break;
			if (j < mod->nummodelsurfaces)
				mod->DrawAddWaterPlanes = R_Q1BSP_DrawAddWaterPlanes;

			// build lightstyle update chains
			// (used to rapidly mark lightmapupdateflags on many surfaces
			// when d_lightstylevalue changes)
			memset(stylecounts, 0, sizeof(stylecounts));
			for (k = 0;k < mod->nummodelsurfaces;k++)
			{
				surface = mod->data_surfaces + mod->firstmodelsurface + k;
				for (j = 0;j < MAXLIGHTMAPS;j++)
					stylecounts[surface->lightmapinfo->styles[j]]++;
			}
			mod->brushq1.num_lightstyles = 0;
			for (k = 0;k < 255;k++)
			{
				if (stylecounts[k])
				{
					styleinfo[mod->brushq1.num_lightstyles].style = k;
					styleinfo[mod->brushq1.num_lightstyles].value = 0;
					styleinfo[mod->brushq1.num_lightstyles].numsurfaces = 0;
					styleinfo[mod->brushq1.num_lightstyles].surfacelist = (int *)datapointer;datapointer += stylecounts[k] * sizeof(int);
					remapstyles[k] = mod->brushq1.num_lightstyles;
					mod->brushq1.num_lightstyles++;
				}
			}
			for (k = 0;k < mod->nummodelsurfaces;k++)
			{
				surface = mod->data_surfaces + mod->firstmodelsurface + k;
				for (j = 0;j < MAXLIGHTMAPS;j++)
				{
					if (surface->lightmapinfo->styles[j] != 255)
					{
						int r = remapstyles[surface->lightmapinfo->styles[j]];
						styleinfo[r].surfacelist[styleinfo[r].numsurfaces++] = mod->firstmodelsurface + k;
					}
				}
			}
			mod->brushq1.data_lightstyleinfo = (model_brush_lightstyleinfo_t *)datapointer;datapointer += mod->brushq1.num_lightstyles * sizeof(model_brush_lightstyleinfo_t);
			memcpy(mod->brushq1.data_lightstyleinfo, styleinfo, mod->brushq1.num_lightstyles * sizeof(model_brush_lightstyleinfo_t));
		}
		else
		{
			// LordHavoc: empty submodel(lacrima.bsp has such a glitch)
			Con_Printf("warning: empty submodel *%i in %s\n", i+1, loadmodel->name);
		}
		//mod->brushq1.num_visleafs = bm->visleafs;

		// build a Bounding Interval Hierarchy for culling triangles in light rendering
		Mod_MakeCollisionBIH(mod, true, &mod->render_bih);

		if (mod_q1bsp_polygoncollisions.integer)
		{
			mod->collision_bih = mod->render_bih;
			// point traces and contents checks still use the bsp tree
			mod->TraceLine = Mod_CollisionBIH_TraceLine;
			mod->TraceBox = Mod_CollisionBIH_TraceBox;
			mod->TraceBrush = Mod_CollisionBIH_TraceBrush;
			mod->TraceLineAgainstSurfaces = Mod_CollisionBIH_TraceLineAgainstSurfaces;
		}

		// generate VBOs and other shared data before cloning submodels
		if (i == 0)
		{
			Mod_BuildVBOs();
			Mod_Q1BSP_LoadMapBrushes();
			//Mod_Q1BSP_ProcessLightList();
		}
	}

	Con_DPrintf("Stats for q1bsp model \"%s\": %i faces, %i nodes, %i leafs, %i visleafs, %i visleafportals, mesh: %i vertices, %i triangles, %i surfaces\n", loadmodel->name, loadmodel->num_surfaces, loadmodel->brush.num_nodes, loadmodel->brush.num_leafs, mod->brush.num_pvsclusters, loadmodel->brush.num_portals, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->num_surfaces);
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

void static Mod_Q2BSP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int i;
	q2dheader_t *header;

	Host_Error("Mod_Q2BSP_Load: not yet implemented");

	mod->modeldatatypestring = "Q2BSP";

	mod->type = mod_brushq2;

	header = (q2dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q2BSPVERSION)
		Host_Error("Mod_Q2BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q2BSPVERSION);

	mod_base = (unsigned char *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

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

static int Mod_Q3BSP_SuperContentsFromNativeContents(dp_model_t *model, int nativecontents);
static int Mod_Q3BSP_NativeContentsFromSuperContents(dp_model_t *model, int supercontents);

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
	loadmodel->brush.entities = (char *)Mem_Alloc(loadmodel->mempool, l->filelen + 1);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	loadmodel->brush.entities[l->filelen] = 0;
	data = loadmodel->brush.entities;
	// some Q3 maps override the lightgrid_cellsize with a worldspawn key
	// VorteX: q3map2 FS-R generates tangentspace deluxemaps for q3bsp and sets 'deluxeMaps' key
	loadmodel->brushq3.deluxemapping = false;
	if (data && COM_ParseToken_Simple(&data, false, false) && com_token[0] == '{')
	{
		while (1)
		{
			if (!COM_ParseToken_Simple(&data, false, false))
				break; // error
			if (com_token[0] == '}')
				break; // end of worldspawn
			if (com_token[0] == '_')
				strlcpy(key, com_token + 1, sizeof(key));
			else
				strlcpy(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken_Simple(&data, false, false))
				break; // error
			strlcpy(value, com_token, sizeof(value));
			if (!strcasecmp("gridsize", key)) // this one is case insensitive to 100% match q3map2
			{
#if _MSC_VER >= 1400
#define sscanf sscanf_s
#endif
#if 0
				if (sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]) == 3 && v[0] != 0 && v[1] != 0 && v[2] != 0)
					VectorCopy(v, loadmodel->brushq3.num_lightgrid_cellsize);
#else
				VectorSet(v, 64, 64, 128);
				if(sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]) != 3)
					Con_Printf("Mod_Q3BSP_LoadEntities: funny gridsize \"%s\" in %s, interpreting as \"%f %f %f\" to match q3map2's parsing\n", value, loadmodel->name, v[0], v[1], v[2]);
				if (v[0] != 0 && v[1] != 0 && v[2] != 0)
					VectorCopy(v, loadmodel->brushq3.num_lightgrid_cellsize);
#endif
			}
			else if (!strcmp("deluxeMaps", key))
			{
				if (!strcmp(com_token, "1"))
				{
					loadmodel->brushq3.deluxemapping = true;
					loadmodel->brushq3.deluxemapping_modelspace = true;
				}
				else if (!strcmp(com_token, "2"))
				{
					loadmodel->brushq3.deluxemapping = true;
					loadmodel->brushq3.deluxemapping_modelspace = false;
				}
			}
		}
	}
}

static void Mod_Q3BSP_LoadTextures(lump_t *l)
{
	q3dtexture_t *in;
	texture_t *out;
	int i, count;

	in = (q3dtexture_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadTextures: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (texture_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->data_textures = out;
	loadmodel->num_textures = count;
	loadmodel->num_texturesperskin = loadmodel->num_textures;

	for (i = 0;i < count;i++)
	{
		strlcpy (out[i].name, in[i].name, sizeof (out[i].name));
		out[i].surfaceflags = LittleLong(in[i].surfaceflags);
		out[i].supercontents = Mod_Q3BSP_SuperContentsFromNativeContents(loadmodel, LittleLong(in[i].contents));
		Mod_LoadTextureFromQ3Shader(out + i, out[i].name, true, true, TEXF_MIPMAP | TEXF_ISWORLD | TEXF_PICMIP | TEXF_COMPRESS);
		// restore the surfaceflags and supercontents
		out[i].surfaceflags = LittleLong(in[i].surfaceflags);
		out[i].supercontents = Mod_Q3BSP_SuperContentsFromNativeContents(loadmodel, LittleLong(in[i].contents));
	}
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

static void Mod_Q3BSP_LoadBrushSides_IG(lump_t *l)
{
	q3dbrushside_ig_t *in;
	q3mbrushside_t *out;
	int i, n, count;

	in = (q3dbrushside_ig_t *)(mod_base + l->fileofs);
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
	int i, j, n, c, count, maxplanes, q3surfaceflags;
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
		q3surfaceflags = 0;
		for (j = 0;j < out->numbrushsides;j++)
		{
			VectorCopy(out->firstbrushside[j].plane->normal, planes[j].normal);
			planes[j].dist = out->firstbrushside[j].plane->dist;
			planes[j].q3surfaceflags = out->firstbrushside[j].texture->surfaceflags;
			planes[j].texture = out->firstbrushside[j].texture;
			q3surfaceflags |= planes[j].q3surfaceflags;
		}
		// make the colbrush from the planes
		out->colbrushf = Collision_NewBrushFromPlanes(loadmodel->mempool, out->numbrushsides, planes, out->texture->supercontents, q3surfaceflags, out->texture, true);

		// this whole loop can take a while (e.g. on redstarrepublic4)
		CL_KeepaliveMessage(false);
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
		if(in->color4ub[0] != 255 || in->color4ub[1] != 255 || in->color4ub[2] != 255)
			loadmodel->lit = true;
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

	if(!loadmodel->brushq3.num_vertices)
	{
		if (count)
			Con_Printf("Mod_Q3BSP_LoadTriangles: %s has triangles but no vertexes, broken compiler, ignoring problem\n", loadmodel->name);
		loadmodel->brushq3.num_triangles = 0;
		return;
	}

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
	q3dlightmap_t *input_pointer;
	int i;
	int j;
	int k;
	int count;
	int powerx;
	int powery;
	int powerxy;
	int powerdxy;
	int endlightmap;
	int mergegoal;
	int lightmapindex;
	int realcount;
	int realindex;
	int mergedwidth;
	int mergedheight;
	int mergedcolumns;
	int mergedrows;
	int mergedrowsxcolumns;
	int size;
	int bytesperpixel;
	int rgbmap[3];
	unsigned char *c;
	unsigned char *mergedpixels;
	unsigned char *mergeddeluxepixels;
	unsigned char *mergebuf;
	char mapname[MAX_QPATH];
	qboolean external;
	unsigned char *inpixels[10000]; // max count q3map2 can output (it uses 4 digits)

	// defaults for q3bsp
	size = 128;
	bytesperpixel = 3;
	rgbmap[0] = 2;
	rgbmap[1] = 1;
	rgbmap[2] = 0;
	external = false;
	loadmodel->brushq3.lightmapsize = 128;

	if (cls.state == ca_dedicated)
		return;

	if(mod_q3bsp_nolightmaps.integer)
	{
		return;
	}
	else if(l->filelen)
	{
		// prefer internal LMs for compatibility (a BSP contains no info on whether external LMs exist)
		if (developer_loading.integer)
			Con_Printf("Using internal lightmaps\n");
		input_pointer = (q3dlightmap_t *)(mod_base + l->fileofs);
		if (l->filelen % sizeof(*input_pointer))
			Host_Error("Mod_Q3BSP_LoadLightmaps: funny lump size in %s",loadmodel->name);
		count = l->filelen / sizeof(*input_pointer);
		for(i = 0; i < count; ++i)
			inpixels[i] = input_pointer[i].rgb;
	}
	else
	{
		// no internal lightmaps
		// try external lightmaps
		if (developer_loading.integer)
			Con_Printf("Using external lightmaps\n");
		FS_StripExtension(loadmodel->name, mapname, sizeof(mapname));
		inpixels[0] = loadimagepixelsbgra(va("%s/lm_%04d", mapname, 0), false, false, false, NULL);
		if(!inpixels[0])
			return;

		// using EXTERNAL lightmaps instead
		if(image_width != (int) CeilPowerOf2(image_width) || image_width != image_height)
		{
			Mem_Free(inpixels[0]);
			Host_Error("Mod_Q3BSP_LoadLightmaps: invalid external lightmap size in %s",loadmodel->name);
		}

		size = image_width;
		bytesperpixel = 4;
		rgbmap[0] = 0;
		rgbmap[1] = 1;
		rgbmap[2] = 2;
		external = true;

		for(count = 1; ; ++count)
		{
			inpixels[count] = loadimagepixelsbgra(va("%s/lm_%04d", mapname, count), false, false, false, NULL);
			if(!inpixels[count])
				break; // we got all of them
			if(image_width != size || image_height != size)
			{
				Mem_Free(inpixels[count]);
				inpixels[count] = NULL;
				Con_Printf("Mod_Q3BSP_LoadLightmaps: mismatched lightmap size in %s - external lightmap %s/lm_%04d does not match earlier ones\n", loadmodel->name, mapname, count);
				break;
			}
		}
	}

	loadmodel->brushq3.lightmapsize = size;
	loadmodel->brushq3.num_originallightmaps = count;

	// now check the surfaces to see if any of them index an odd numbered
	// lightmap, if so this is not a deluxemapped bsp file
	//
	// also check what lightmaps are actually used, because q3map2 sometimes
	// (always?) makes an unused one at the end, which
	// q3map2 sometimes (or always?) makes a second blank lightmap for no
	// reason when only one lightmap is used, which can throw off the
	// deluxemapping detection method, so check 2-lightmap bsp's specifically
	// to see if the second lightmap is blank, if so it is not deluxemapped.
	// VorteX: autodetect only if previous attempt to find "deluxeMaps" key
	// in Mod_Q3BSP_LoadEntities was failed
	if (!loadmodel->brushq3.deluxemapping)
	{
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

		// q3map2 sometimes (or always?) makes a second blank lightmap for no
		// reason when only one lightmap is used, which can throw off the
		// deluxemapping detection method, so check 2-lightmap bsp's specifically
		// to see if the second lightmap is blank, if so it is not deluxemapped.
		//
		// further research has shown q3map2 sometimes creates a deluxemap and two
		// blank lightmaps, which must be handled properly as well
		if (endlightmap == 1 && count > 1)
		{
			c = inpixels[1];
			for (i = 0;i < size*size;i++)
			{
				if (c[bytesperpixel*i + rgbmap[0]])
					break;
				if (c[bytesperpixel*i + rgbmap[1]])
					break;
				if (c[bytesperpixel*i + rgbmap[2]])
					break;
			}
			if (i == size*size)
			{
				// all pixels in the unused lightmap were black...
				loadmodel->brushq3.deluxemapping = false;
			}
		}
	}

	Con_DPrintf("%s is %sdeluxemapped\n", loadmodel->name, loadmodel->brushq3.deluxemapping ? "" : "not ");

	// figure out what the most reasonable merge power is within limits

	// find the appropriate NxN dimensions to merge to, to avoid wasted space
	realcount = count >> (int)loadmodel->brushq3.deluxemapping;

	// figure out how big the merged texture has to be
	mergegoal = 128<<bound(0, mod_q3bsp_lightmapmergepower.integer, 6);
	mergegoal = bound(size, mergegoal, (int)vid.maxtexturesize_2d);
	while (mergegoal > size && mergegoal * mergegoal / 4 >= size * size * realcount)
		mergegoal /= 2;
	mergedwidth = mergegoal;
	mergedheight = mergegoal;
	// choose non-square size (2x1 aspect) if only half the space is used;
	// this really only happens when the entire set fits in one texture, if
	// there are multiple textures, we don't worry about shrinking the last
	// one to fit, because the driver prefers the same texture size on
	// consecutive draw calls...
	if (mergedwidth * mergedheight / 2 >= size*size*realcount)
		mergedheight /= 2;

	loadmodel->brushq3.num_lightmapmergedwidthpower = 0;
	loadmodel->brushq3.num_lightmapmergedheightpower = 0;
	while (mergedwidth > size<<loadmodel->brushq3.num_lightmapmergedwidthpower)
		loadmodel->brushq3.num_lightmapmergedwidthpower++;
	while (mergedheight > size<<loadmodel->brushq3.num_lightmapmergedheightpower)
		loadmodel->brushq3.num_lightmapmergedheightpower++;
	loadmodel->brushq3.num_lightmapmergedwidthheightdeluxepower = loadmodel->brushq3.num_lightmapmergedwidthpower + loadmodel->brushq3.num_lightmapmergedheightpower + (loadmodel->brushq3.deluxemapping ? 1 : 0);

	powerx = loadmodel->brushq3.num_lightmapmergedwidthpower;
	powery = loadmodel->brushq3.num_lightmapmergedheightpower;
	powerxy = powerx+powery;
	powerdxy = loadmodel->brushq3.deluxemapping + powerxy;

	mergedcolumns = 1 << powerx;
	mergedrows = 1 << powery;
	mergedrowsxcolumns = 1 << powerxy;

	loadmodel->brushq3.num_mergedlightmaps = (realcount + (1 << powerxy) - 1) >> powerxy;
	loadmodel->brushq3.data_lightmaps = (rtexture_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brushq3.num_mergedlightmaps * sizeof(rtexture_t *));
	if (loadmodel->brushq3.deluxemapping)
		loadmodel->brushq3.data_deluxemaps = (rtexture_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brushq3.num_mergedlightmaps * sizeof(rtexture_t *));

	// allocate a texture pool if we need it
	if (loadmodel->texturepool == NULL && cls.state != ca_dedicated)
		loadmodel->texturepool = R_AllocTexturePool();

	mergedpixels = (unsigned char *) Mem_Alloc(tempmempool, mergedwidth * mergedheight * 4);
	mergeddeluxepixels = loadmodel->brushq3.deluxemapping ? (unsigned char *) Mem_Alloc(tempmempool, mergedwidth * mergedheight * 4) : NULL;
	for (i = 0;i < count;i++)
	{
		// figure out which merged lightmap texture this fits into
		realindex = i >> (int)loadmodel->brushq3.deluxemapping;
		lightmapindex = i >> powerdxy;

		// choose the destination address
		mergebuf = (loadmodel->brushq3.deluxemapping && (i & 1)) ? mergeddeluxepixels : mergedpixels;
		mergebuf += 4 * (realindex & (mergedcolumns-1))*size + 4 * ((realindex >> powerx) & (mergedrows-1))*mergedwidth*size;
		if ((i & 1) == 0 || !loadmodel->brushq3.deluxemapping)
			Con_Printf("copying original lightmap %i (%ix%i) to %i (at %i,%i)\n", i, size, size, lightmapindex, (realindex & (mergedcolumns-1))*size, ((realindex >> powerx) & (mergedrows-1))*size);

		// convert pixels from RGB or BGRA while copying them into the destination rectangle
		for (j = 0;j < size;j++)
		for (k = 0;k < size;k++)
		{
			mergebuf[(j*mergedwidth+k)*4+0] = inpixels[i][(j*size+k)*bytesperpixel+rgbmap[0]];
			mergebuf[(j*mergedwidth+k)*4+1] = inpixels[i][(j*size+k)*bytesperpixel+rgbmap[1]];
			mergebuf[(j*mergedwidth+k)*4+2] = inpixels[i][(j*size+k)*bytesperpixel+rgbmap[2]];
			mergebuf[(j*mergedwidth+k)*4+3] = 255;
		}

		// upload texture if this was the last tile being written to the texture
		if (((realindex + 1) & (mergedrowsxcolumns - 1)) == 0 || (realindex + 1) == realcount)
		{
			if (loadmodel->brushq3.deluxemapping && (i & 1))
				loadmodel->brushq3.data_deluxemaps[lightmapindex] = R_LoadTexture2D(loadmodel->texturepool, va("deluxemap%04i", lightmapindex), mergedwidth, mergedheight, mergeddeluxepixels, TEXTYPE_BGRA, TEXF_FORCELINEAR | (gl_texturecompression_q3bspdeluxemaps.integer ? TEXF_COMPRESS : 0), -1, NULL);
			else
				loadmodel->brushq3.data_lightmaps [lightmapindex] = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%04i", lightmapindex), mergedwidth, mergedheight, mergedpixels, TEXTYPE_BGRA, TEXF_FORCELINEAR | (gl_texturecompression_q3bsplightmaps.integer ? TEXF_COMPRESS : 0), -1, NULL);
		}
	}

	if (mergeddeluxepixels)
		Mem_Free(mergeddeluxepixels);
	Mem_Free(mergedpixels);
	if(external)
	{
		for(i = 0; i < count; ++i)
			Mem_Free(inpixels[i]);
	}
}

static void Mod_Q3BSP_BuildBBoxes(const int *element3i, int num_triangles, const float *vertex3f, float **collisionbbox6f, int *collisionstride, int stride)
{
	int j, k, cnt, tri;
	float *mins, *maxs;
	const float *vert;
	*collisionstride = stride;
	if(stride > 0)
	{
		cnt = (num_triangles + stride - 1) / stride;
		*collisionbbox6f = (float *) Mem_Alloc(loadmodel->mempool, sizeof(float[6]) * cnt);
		for(j = 0; j < cnt; ++j)
		{
			mins = &((*collisionbbox6f)[6 * j + 0]);
			maxs = &((*collisionbbox6f)[6 * j + 3]);
			for(k = 0; k < stride; ++k)
			{
				tri = j * stride + k;
				if(tri >= num_triangles)
					break;
				vert = &(vertex3f[element3i[3 * tri + 0] * 3]);
				if(!k || vert[0] < mins[0]) mins[0] = vert[0];
				if(!k || vert[1] < mins[1]) mins[1] = vert[1];
				if(!k || vert[2] < mins[2]) mins[2] = vert[2];
				if(!k || vert[0] > maxs[0]) maxs[0] = vert[0];
				if(!k || vert[1] > maxs[1]) maxs[1] = vert[1];
				if(!k || vert[2] > maxs[2]) maxs[2] = vert[2];
				vert = &(vertex3f[element3i[3 * tri + 1] * 3]);
				if(vert[0] < mins[0]) mins[0] = vert[0];
				if(vert[1] < mins[1]) mins[1] = vert[1];
				if(vert[2] < mins[2]) mins[2] = vert[2];
				if(vert[0] > maxs[0]) maxs[0] = vert[0];
				if(vert[1] > maxs[1]) maxs[1] = vert[1];
				if(vert[2] > maxs[2]) maxs[2] = vert[2];
				vert = &(vertex3f[element3i[3 * tri + 2] * 3]);
				if(vert[0] < mins[0]) mins[0] = vert[0];
				if(vert[1] < mins[1]) mins[1] = vert[1];
				if(vert[2] < mins[2]) mins[2] = vert[2];
				if(vert[0] > maxs[0]) maxs[0] = vert[0];
				if(vert[1] > maxs[1]) maxs[1] = vert[1];
				if(vert[2] > maxs[2]) maxs[2] = vert[2];
			}
		}
	}
	else
		*collisionbbox6f = NULL;
}

typedef struct patchtess_s
{
	patchinfo_t info;

	// Auxiliary data used only by patch loading code in Mod_Q3BSP_LoadFaces
	int surface_id;
	float lodgroup[6];
	float *originalvertex3f;
} patchtess_t;

#define PATCHTESS_SAME_LODGROUP(a,b) \
	( \
		(a).lodgroup[0] == (b).lodgroup[0] && \
		(a).lodgroup[1] == (b).lodgroup[1] && \
		(a).lodgroup[2] == (b).lodgroup[2] && \
		(a).lodgroup[3] == (b).lodgroup[3] && \
		(a).lodgroup[4] == (b).lodgroup[4] && \
		(a).lodgroup[5] == (b).lodgroup[5] \
	)

static void Mod_Q3BSP_LoadFaces(lump_t *l)
{
	q3dface_t *in, *oldin;
	msurface_t *out, *oldout;
	int i, oldi, j, n, count, invalidelements, patchsize[2], finalwidth, finalheight, xtess, ytess, finalvertices, finaltriangles, firstvertex, firstelement, type, oldnumtriangles, oldnumtriangles2, meshvertices, meshtriangles, collisionvertices, collisiontriangles, numvertices, numtriangles, cxtess, cytess;
	float lightmaptcbase[2], lightmaptcscale[2];
	//int *originalelement3i;
	//int *originalneighbor3i;
	float *originalvertex3f;
	//float *originalsvector3f;
	//float *originaltvector3f;
	float *originalnormal3f;
	float *originalcolor4f;
	float *originaltexcoordtexture2f;
	float *originaltexcoordlightmap2f;
	float *surfacecollisionvertex3f;
	int *surfacecollisionelement3i;
	float *v;
	patchtess_t *patchtess = NULL;
	int patchtesscount = 0;
	qboolean again;

	in = (q3dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t *)Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->data_surfaces = out;
	loadmodel->num_surfaces = count;

	if(count > 0)
		patchtess = (patchtess_t*) Mem_Alloc(tempmempool, count * sizeof(*patchtess));

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
		if (type != Q3FACETYPE_FLAT
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
			if (developer_extra.integer)
				Con_DPrintf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid effectindex %i (%i effects)\n", i, out->texture->name, n, loadmodel->brushq3.num_effects);
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
			else if (n >= loadmodel->brushq3.num_originallightmaps)
			{
				if(loadmodel->brushq3.num_originallightmaps != 0)
					Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid lightmapindex %i (%i lightmaps)\n", i, out->texture->name, n, loadmodel->brushq3.num_originallightmaps);
				n = -1;
			}
			else
			{
				out->lightmaptexture = loadmodel->brushq3.data_lightmaps[n >> loadmodel->brushq3.num_lightmapmergedwidthheightdeluxepower];
				if (loadmodel->brushq3.deluxemapping)
					out->deluxemaptexture = loadmodel->brushq3.data_deluxemaps[n >> loadmodel->brushq3.num_lightmapmergedwidthheightdeluxepower];
				loadmodel->lit = true;
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
		case Q3FACETYPE_FLAT:
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
			xtess = bound(0, xtess, 1024);
			ytess = bound(0, ytess, 1024);

			// lower quality collision patches! Same procedure as before, but different cvars
			// convert patch to Q3FACETYPE_MESH
			cxtess = Q3PatchTesselationOnX(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_collision_tolerance.value);
			cytess = Q3PatchTesselationOnY(patchsize[0], patchsize[1], 3, originalvertex3f, r_subdivisions_collision_tolerance.value);
			// bound to user settings
			cxtess = bound(r_subdivisions_collision_mintess.integer, cxtess, r_subdivisions_collision_maxtess.integer);
			cytess = bound(r_subdivisions_collision_mintess.integer, cytess, r_subdivisions_collision_maxtess.integer);
			// bound to sanity settings
			cxtess = bound(0, cxtess, 1024);
			cytess = bound(0, cytess, 1024);

			// store it for the LOD grouping step
	 		patchtess[patchtesscount].info.xsize = patchsize[0];
	 		patchtess[patchtesscount].info.ysize = patchsize[1];
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_VISUAL].xtess = xtess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_VISUAL].ytess = ytess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_COLLISION].xtess = cxtess;
	 		patchtess[patchtesscount].info.lods[PATCH_LOD_COLLISION].ytess = cytess;
	
			patchtess[patchtesscount].surface_id = i;
			patchtess[patchtesscount].lodgroup[0] = LittleFloat(in->specific.patch.mins[0]);
			patchtess[patchtesscount].lodgroup[1] = LittleFloat(in->specific.patch.mins[1]);
			patchtess[patchtesscount].lodgroup[2] = LittleFloat(in->specific.patch.mins[2]);
			patchtess[patchtesscount].lodgroup[3] = LittleFloat(in->specific.patch.maxs[0]);
			patchtess[patchtesscount].lodgroup[4] = LittleFloat(in->specific.patch.maxs[1]);
			patchtess[patchtesscount].lodgroup[5] = LittleFloat(in->specific.patch.maxs[2]);
			patchtess[patchtesscount].originalvertex3f = originalvertex3f;
			++patchtesscount;
			break;
		case Q3FACETYPE_FLARE:
			if (developer_extra.integer)
				Con_DPrintf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): Q3FACETYPE_FLARE not supported (yet)\n", i, out->texture->name);
			// don't render it
			continue;
		}
		out->num_vertices = numvertices;
		out->num_triangles = numtriangles;
		meshvertices += out->num_vertices;
		meshtriangles += out->num_triangles;
	}

	// Fix patches tesselations so that they make no seams
	do
	{
		again = false;
		for(i = 0; i < patchtesscount; ++i)
		{
			for(j = i+1; j < patchtesscount; ++j)
			{
				if (!PATCHTESS_SAME_LODGROUP(patchtess[i], patchtess[j]))
					continue;

				if (Q3PatchAdjustTesselation(3, &patchtess[i].info, patchtess[i].originalvertex3f, &patchtess[j].info, patchtess[j].originalvertex3f) )
					again = true;
			}
		}
	}
	while (again);

	// Calculate resulting number of triangles
	collisionvertices = 0;
	collisiontriangles = 0;
	for(i = 0; i < patchtesscount; ++i)
	{
		finalwidth = Q3PatchDimForTess(patchtess[i].info.xsize, patchtess[i].info.lods[PATCH_LOD_VISUAL].xtess);
		finalheight = Q3PatchDimForTess(patchtess[i].info.ysize,patchtess[i].info.lods[PATCH_LOD_VISUAL].ytess);
		numvertices = finalwidth * finalheight;
		numtriangles = (finalwidth - 1) * (finalheight - 1) * 2;

		oldout[patchtess[i].surface_id].num_vertices = numvertices;
		oldout[patchtess[i].surface_id].num_triangles = numtriangles;
		meshvertices += oldout[patchtess[i].surface_id].num_vertices;
		meshtriangles += oldout[patchtess[i].surface_id].num_triangles;

		finalwidth = Q3PatchDimForTess(patchtess[i].info.xsize, patchtess[i].info.lods[PATCH_LOD_COLLISION].xtess);
		finalheight = Q3PatchDimForTess(patchtess[i].info.ysize,patchtess[i].info.lods[PATCH_LOD_COLLISION].ytess);
		numvertices = finalwidth * finalheight;
		numtriangles = (finalwidth - 1) * (finalheight - 1) * 2;

		oldout[patchtess[i].surface_id].num_collisionvertices = numvertices;
		oldout[patchtess[i].surface_id].num_collisiontriangles = numtriangles;
		collisionvertices += oldout[patchtess[i].surface_id].num_collisionvertices;
		collisiontriangles += oldout[patchtess[i].surface_id].num_collisiontriangles;
	}

	i = oldi;
	in = oldin;
	out = oldout;
	Mod_AllocSurfMesh(loadmodel->mempool, meshvertices, meshtriangles, false, true, false);
	if (collisiontriangles)
	{
		loadmodel->brush.data_collisionvertex3f = (float *)Mem_Alloc(loadmodel->mempool, collisionvertices * sizeof(float[3]));
		loadmodel->brush.data_collisionelement3i = (int *)Mem_Alloc(loadmodel->mempool, collisiontriangles * sizeof(int[3]));
	}
	meshvertices = 0;
	meshtriangles = 0;
	collisionvertices = 0;
	collisiontriangles = 0;
	for (;i < count && meshvertices + out->num_vertices <= loadmodel->surfmesh.num_vertices;i++, in++, out++)
	{
		if (out->num_vertices < 3 || out->num_triangles < 1)
			continue;

		type = LittleLong(in->type);
		firstvertex = LittleLong(in->firstvertex);
		firstelement = LittleLong(in->firstelement);
		out->num_firstvertex = meshvertices;
		out->num_firsttriangle = meshtriangles;
		out->num_firstcollisiontriangle = collisiontriangles;
		switch(type)
		{
		case Q3FACETYPE_FLAT:
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

			xtess = ytess = cxtess = cytess = -1;
			for(j = 0; j < patchtesscount; ++j)
				if(patchtess[j].surface_id == i)
				{
					xtess = patchtess[j].info.lods[PATCH_LOD_VISUAL].xtess;
					ytess = patchtess[j].info.lods[PATCH_LOD_VISUAL].ytess;
					cxtess = patchtess[j].info.lods[PATCH_LOD_COLLISION].xtess;
					cytess = patchtess[j].info.lods[PATCH_LOD_COLLISION].ytess;
					break;
				}
			if(xtess == -1)
			{
				Con_Printf("ERROR: patch %d isn't preprocessed?!?\n", i);
				xtess = ytess = cxtess = cytess = 0;
			}

			finalwidth = Q3PatchDimForTess(patchsize[0],xtess); //((patchsize[0] - 1) * xtess) + 1;
			finalheight = Q3PatchDimForTess(patchsize[1],ytess); //((patchsize[1] - 1) * ytess) + 1;
			finalvertices = finalwidth * finalheight;
			oldnumtriangles = finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;
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

			if (developer_extra.integer)
			{
				if (out->num_triangles < finaltriangles)
					Con_DPrintf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles, %i degenerate triangles removed (leaving %i)\n", patchsize[0], patchsize[1], out->num_vertices, finaltriangles, finaltriangles - out->num_triangles, out->num_triangles);
				else
					Con_DPrintf("Mod_Q3BSP_LoadFaces: %ix%i curve subdivided to %i vertices / %i triangles\n", patchsize[0], patchsize[1], out->num_vertices, out->num_triangles);
			}
			// q3map does not put in collision brushes for curves... ugh
			// build the lower quality collision geometry
			finalwidth = Q3PatchDimForTess(patchsize[0],cxtess); //((patchsize[0] - 1) * cxtess) + 1;
			finalheight = Q3PatchDimForTess(patchsize[1],cytess); //((patchsize[1] - 1) * cytess) + 1;
			finalvertices = finalwidth * finalheight;
			oldnumtriangles2 = finaltriangles = (finalwidth - 1) * (finalheight - 1) * 2;

			// legacy collision geometry implementation
			out->deprecatedq3data_collisionvertex3f = (float *)Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * finalvertices);
			out->deprecatedq3data_collisionelement3i = (int *)Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * finaltriangles);
			out->num_collisionvertices = finalvertices;
			out->num_collisiontriangles = finaltriangles;
			Q3PatchTesselateFloat(3, sizeof(float[3]), out->deprecatedq3data_collisionvertex3f, patchsize[0], patchsize[1], sizeof(float[3]), originalvertex3f, cxtess, cytess);
			Q3PatchTriangleElements(out->deprecatedq3data_collisionelement3i, finalwidth, finalheight, 0);

			//Mod_SnapVertices(3, out->num_vertices, (loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex), 0.25);
			Mod_SnapVertices(3, finalvertices, out->deprecatedq3data_collisionvertex3f, 1);

			out->num_collisiontriangles = Mod_RemoveDegenerateTriangles(finaltriangles, out->deprecatedq3data_collisionelement3i, out->deprecatedq3data_collisionelement3i, out->deprecatedq3data_collisionvertex3f);

			// now optimize the collision mesh by finding triangle bboxes...
			Mod_Q3BSP_BuildBBoxes(out->deprecatedq3data_collisionelement3i, out->num_collisiontriangles, out->deprecatedq3data_collisionvertex3f, &out->deprecatedq3data_collisionbbox6f, &out->deprecatedq3num_collisionbboxstride, mod_q3bsp_curves_collisions_stride.integer);
			Mod_Q3BSP_BuildBBoxes(loadmodel->surfmesh.data_element3i + 3 * out->num_firsttriangle, out->num_triangles, loadmodel->surfmesh.data_vertex3f, &out->deprecatedq3data_bbox6f, &out->deprecatedq3num_bboxstride, mod_q3bsp_curves_stride.integer);

			// store collision geometry for BIH collision tree
			surfacecollisionvertex3f = loadmodel->brush.data_collisionvertex3f + collisionvertices * 3;
			surfacecollisionelement3i = loadmodel->brush.data_collisionelement3i + collisiontriangles * 3;
			Q3PatchTesselateFloat(3, sizeof(float[3]), surfacecollisionvertex3f, patchsize[0], patchsize[1], sizeof(float[3]), originalvertex3f, cxtess, cytess);
			Q3PatchTriangleElements(surfacecollisionelement3i, finalwidth, finalheight, collisionvertices);
			Mod_SnapVertices(3, finalvertices, surfacecollisionvertex3f, 1);
#if 1
			// remove this once the legacy code is removed
			{
				int nc = out->num_collisiontriangles;
#endif
			out->num_collisiontriangles = Mod_RemoveDegenerateTriangles(finaltriangles, surfacecollisionelement3i, surfacecollisionelement3i, loadmodel->brush.data_collisionvertex3f);
#if 1
				if(nc != out->num_collisiontriangles)
				{
					Con_Printf("number of collision triangles differs between BIH and BSP. FAIL.\n");
				}
			}
#endif

			if (developer_extra.integer)
				Con_DPrintf("Mod_Q3BSP_LoadFaces: %ix%i curve became %i:%i vertices / %i:%i triangles (%i:%i degenerate)\n", patchsize[0], patchsize[1], out->num_vertices, out->num_collisionvertices, oldnumtriangles, oldnumtriangles2, oldnumtriangles - out->num_triangles, oldnumtriangles2 - out->num_collisiontriangles);

			collisionvertices += finalvertices;
			collisiontriangles += out->num_collisiontriangles;
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
			if (cls.state != ca_dedicated && out->lightmaptexture)
			{
				// figure out which part of the merged lightmap this fits into
				int lightmapindex = LittleLong(in->lightmapindex) >> (loadmodel->brushq3.deluxemapping ? 1 : 0);
				int mergewidth = R_TextureWidth(out->lightmaptexture) / loadmodel->brushq3.lightmapsize;
				int mergeheight = R_TextureHeight(out->lightmaptexture) / loadmodel->brushq3.lightmapsize;
				lightmapindex &= mergewidth * mergeheight - 1;
				lightmaptcscale[0] = 1.0f / mergewidth;
				lightmaptcscale[1] = 1.0f / mergeheight;
				lightmaptcbase[0] = (lightmapindex % mergewidth) * lightmaptcscale[0];
				lightmaptcbase[1] = (lightmapindex / mergewidth) * lightmaptcscale[1];
				// modify the lightmap texcoords to match this region of the merged lightmap
				for (j = 0, v = loadmodel->surfmesh.data_texcoordlightmap2f + 2 * out->num_firstvertex;j < out->num_vertices;j++, v += 2)
				{
					v[0] = v[0] * lightmaptcscale[0] + lightmaptcbase[0];
					v[1] = v[1] * lightmaptcscale[1] + lightmaptcbase[1];
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

	i = oldi;
	out = oldout;
	for (;i < count;i++, out++)
	{
		if(out->num_vertices && out->num_triangles)
			continue;
		if(out->num_vertices == 0)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: surface %d (texture %s) has no vertices, ignoring\n", i, out->texture ? out->texture->name : "(none)");
			if(out->num_triangles == 0)
				Con_Printf("Mod_Q3BSP_LoadFaces: surface %d (texture %s) has no triangles, ignoring\n", i, out->texture ? out->texture->name : "(none)");
		}
		else if(out->num_triangles == 0)
			Con_Printf("Mod_Q3BSP_LoadFaces: surface %d (texture %s, near %f %f %f) has no triangles, ignoring\n", i, out->texture ? out->texture->name : "(none)",
					(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[0 * 3 + 0],
					(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[1 * 3 + 0],
					(loadmodel->surfmesh.data_vertex3f + 3 * out->num_firstvertex)[2 * 3 + 0]);
	}

	// for per pixel lighting
	Mod_BuildTextureVectorsFromNormals(0, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_texcoordtexture2f, loadmodel->surfmesh.data_normal3f, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.data_svector3f, loadmodel->surfmesh.data_tvector3f, r_smoothnormals_areaweighting.integer != 0);

	// generate ushort elements array if possible
	if (loadmodel->surfmesh.data_element3s)
		for (i = 0;i < loadmodel->surfmesh.num_triangles*3;i++)
			loadmodel->surfmesh.data_element3s[i] = loadmodel->surfmesh.data_element3i[i];

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

	if(patchtess)
		Mem_Free(patchtess);
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
	if (count == 0)
		Host_Error("Mod_Q3BSP_LoadNodes: missing BSP tree in %s",loadmodel->name);
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
		{
			Con_Printf("Mod_Q3BSP_LoadLightGrid: invalid lightgrid lump size %i bytes, should be %i bytes (%ix%ix%i)", l->filelen, (int)(count * sizeof(*in)), loadmodel->brushq3.num_lightgrid_isize[0], loadmodel->brushq3.num_lightgrid_isize[1], loadmodel->brushq3.num_lightgrid_isize[2]);
			return; // ignore the grid if we cannot understand it
		}
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

static void Mod_Q3BSP_LightPoint(dp_model_t *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal)
{
	int i, j, k, index[3];
	float transformed[3], blend1, blend2, blend, stylescale = 1;
	q3dlightgrid_t *a, *s;

	// scale lighting by lightstyle[0] so that darkmode in dpmod works properly
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		// LordHavoc: FIXME: is this true?
		stylescale = 1; // added while render
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		stylescale = r_refdef.scene.rtlightstylevalue[0];
		break;
	}

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
				// this uses the mod_md3_sin table because the values are
				// already in the 0-255 range, the 64+ bias fetches a cosine
				// instead of a sine value
				diffusenormal[0] += blend * (mod_md3_sin[64 + s->diffuseyaw] * mod_md3_sin[s->diffusepitch]);
				diffusenormal[1] += blend * (mod_md3_sin[     s->diffuseyaw] * mod_md3_sin[s->diffusepitch]);
				diffusenormal[2] += blend * (mod_md3_sin[64 + s->diffusepitch]);
				//Con_Printf("blend %f: ambient %i %i %i, diffuse %i %i %i, diffusepitch %i diffuseyaw %i (%f %f, normal %f %f %f)\n", blend, s->ambientrgb[0], s->ambientrgb[1], s->ambientrgb[2], s->diffusergb[0], s->diffusergb[1], s->diffusergb[2], s->diffusepitch, s->diffuseyaw, pitch, yaw, (cos(yaw) * cospitch), (sin(yaw) * cospitch), (-sin(pitch)));
			}
		}
	}

	// normalize the light direction before turning
	VectorNormalize(diffusenormal);
	//Con_Printf("result: ambient %f %f %f diffuse %f %f %f diffusenormal %f %f %f\n", ambientcolor[0], ambientcolor[1], ambientcolor[2], diffusecolor[0], diffusecolor[1], diffusecolor[2], diffusenormal[0], diffusenormal[1], diffusenormal[2]);
}

static int Mod_Q3BSP_TraceLineOfSight_RecursiveNodeCheck(mnode_t *node, double p1[3], double p2[3])
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
		ret = Mod_Q3BSP_TraceLineOfSight_RecursiveNodeCheck(node->children[side    ], p1, mid);
		if (ret != 0)
			return ret;
		ret = Mod_Q3BSP_TraceLineOfSight_RecursiveNodeCheck(node->children[side ^ 1], mid, p2);
		if (ret != 1)
			return ret;
		return 2;
	}
	return ((mleaf_t *)node)->clusterindex < 0;
}

static qboolean Mod_Q3BSP_TraceLineOfSight(struct model_s *model, const vec3_t start, const vec3_t end)
{
	if (model->brush.submodel || mod_q3bsp_tracelineofsight_brushes.integer)
	{
		trace_t trace;
		model->TraceLine(model, NULL, NULL, &trace, start, end, SUPERCONTENTS_VISBLOCKERMASK);
		return trace.fraction == 1;
	}
	else
	{
		double tracestart[3], traceend[3];
		VectorCopy(start, tracestart);
		VectorCopy(end, traceend);
		return !Mod_Q3BSP_TraceLineOfSight_RecursiveNodeCheck(model->brush.data_nodes, tracestart, traceend);
	}
}

void Mod_CollisionBIH_TracePoint(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, int hitsupercontentsmask)
{
	const bih_t *bih;
	const bih_leaf_t *leaf;
	const bih_node_t *node;
	const colbrushf_t *brush;
	int axis;
	int nodenum;
	int nodestackpos = 0;
	int nodestack[1024];

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;

	bih = &model->collision_bih;
	if(!bih->nodes)
		return;

	nodenum = bih->rootnode;
	nodestack[nodestackpos++] = nodenum;
	while (nodestackpos)
	{
		nodenum = nodestack[--nodestackpos];
		node = bih->nodes + nodenum;
#if 1
		if (!BoxesOverlap(start, start, node->mins, node->maxs))
			continue;
#endif
		if (node->type <= BIH_SPLITZ && nodestackpos+2 <= 1024)
		{
			axis = node->type - BIH_SPLITX;
			if (start[axis] >= node->frontmin)
				nodestack[nodestackpos++] = node->front;
			if (start[axis] <= node->backmax)
				nodestack[nodestackpos++] = node->back;
		}
		else if (node->type == BIH_UNORDERED)
		{
			for (axis = 0;axis < BIH_MAXUNORDEREDCHILDREN && node->children[axis] >= 0;axis++)
			{
				leaf = bih->leafs + node->children[axis];
#if 1
				if (!BoxesOverlap(start, start, leaf->mins, leaf->maxs))
					continue;
#endif
				switch(leaf->type)
				{
				case BIH_BRUSH:
					brush = model->brush.data_brushes[leaf->itemindex].colbrushf;
					Collision_TracePointBrushFloat(trace, start, brush);
					break;
				case BIH_COLLISIONTRIANGLE:
					// collision triangle - skipped because they have no volume
					break;
				case BIH_RENDERTRIANGLE:
					// render triangle - skipped because they have no volume
					break;
				}
			}
		}
	}
}

void Mod_CollisionBIH_TraceLineShared(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask, const bih_t *bih)
{
	const bih_leaf_t *leaf;
	const bih_node_t *node;
	const colbrushf_t *brush;
	const int *e;
	const texture_t *texture;
	vec3_t nodebigmins, nodebigmaxs, nodestart, nodeend, sweepnodemins, sweepnodemaxs;
	vec_t d1, d2, d3, d4, f, nodestackline[1024][6];
	int axis, nodenum, nodestackpos = 0, nodestack[1024];

	if(!bih->nodes)
		return;

	if (VectorCompare(start, end))
	{
		Mod_CollisionBIH_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
		return;
	}

	nodenum = bih->rootnode;

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;

	// push first node
	nodestackline[nodestackpos][0] = start[0];
	nodestackline[nodestackpos][1] = start[1];
	nodestackline[nodestackpos][2] = start[2];
	nodestackline[nodestackpos][3] = end[0];
	nodestackline[nodestackpos][4] = end[1];
	nodestackline[nodestackpos][5] = end[2];
	nodestack[nodestackpos++] = nodenum;
	while (nodestackpos)
	{
		nodenum = nodestack[--nodestackpos];
		node = bih->nodes + nodenum;
		VectorCopy(nodestackline[nodestackpos], nodestart);
		VectorCopy(nodestackline[nodestackpos] + 3, nodeend);
		sweepnodemins[0] = min(nodestart[0], nodeend[0]); sweepnodemins[1] = min(nodestart[1], nodeend[1]); sweepnodemins[2] = min(nodestart[2], nodeend[2]); sweepnodemaxs[0] = max(nodestart[0], nodeend[0]); sweepnodemaxs[1] = max(nodestart[1], nodeend[1]); sweepnodemaxs[2] = max(nodestart[2], nodeend[2]);
		if (!BoxesOverlap(sweepnodemins, sweepnodemaxs, node->mins, node->maxs))
			continue;
		if (node->type <= BIH_SPLITZ && nodestackpos+2 <= 1024)
		{
			// recurse children of the split
			axis = node->type - BIH_SPLITX;
			d1 = node->backmax - nodestart[axis];
			d2 = node->backmax - nodeend[axis];
			d3 = nodestart[axis] - node->frontmin;
			d4 = nodeend[axis] - node->frontmin;
			switch((d1 < 0) | ((d2 < 0) << 1) | ((d3 < 0) << 2) | ((d4 < 0) << 3))
			{
			case  0: /* >>>> */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  1: /* <>>> */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  2: /* ><>> */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  3: /* <<>> */                                                                                                                                                                                                                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  4: /* >><> */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  5: /* <><> */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  6: /* ><<> */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  7: /* <<<> */                                                                                                                                                                                                  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  8: /* >>>< */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  9: /* <>>< */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 10: /* ><>< */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 11: /* <<>< */                                                                                                                                                                                                  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 12: /* >><< */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 13: /* <><< */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 14: /* ><<< */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 15: /* <<<< */                                                                                                                                                                                                                                                                                                                                                                                                   break;
			}
		}
		else if (node->type == BIH_UNORDERED)
		{
			// calculate sweep bounds for this node
			// copy node bounds into local variables
			VectorCopy(node->mins, nodebigmins);
			VectorCopy(node->maxs, nodebigmaxs);
			// clip line to this node bounds
			axis = 0; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			axis = 1; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			axis = 2; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			// some of the line intersected the enlarged node box
			// calculate sweep bounds for this node
			sweepnodemins[0] = min(nodestart[0], nodeend[0]); sweepnodemins[1] = min(nodestart[1], nodeend[1]); sweepnodemins[2] = min(nodestart[2], nodeend[2]); sweepnodemaxs[0] = max(nodestart[0], nodeend[0]); sweepnodemaxs[1] = max(nodestart[1], nodeend[1]); sweepnodemaxs[2] = max(nodestart[2], nodeend[2]);
			for (axis = 0;axis < BIH_MAXUNORDEREDCHILDREN && node->children[axis] >= 0;axis++)
			{
				leaf = bih->leafs + node->children[axis];
				if (!BoxesOverlap(sweepnodemins, sweepnodemaxs, leaf->mins, leaf->maxs))
					continue;
				switch(leaf->type)
				{
				case BIH_BRUSH:
					brush = model->brush.data_brushes[leaf->itemindex].colbrushf;
					Collision_TraceLineBrushFloat(trace, start, end, brush, brush);
					break;
				case BIH_COLLISIONTRIANGLE:
					if (!mod_q3bsp_curves_collisions.integer)
						continue;
					e = model->brush.data_collisionelement3i + 3*leaf->itemindex;
					texture = model->data_textures + leaf->textureindex;
					Collision_TraceLineTriangleFloat(trace, start, end, model->brush.data_collisionvertex3f + e[0] * 3, model->brush.data_collisionvertex3f + e[1] * 3, model->brush.data_collisionvertex3f + e[2] * 3, texture->supercontents, texture->surfaceflags, texture);
					break;
				case BIH_RENDERTRIANGLE:
					e = model->surfmesh.data_element3i + 3*leaf->itemindex;
					texture = model->data_textures + leaf->textureindex;
					Collision_TraceLineTriangleFloat(trace, start, end, model->surfmesh.data_vertex3f + e[0] * 3, model->surfmesh.data_vertex3f + e[1] * 3, model->surfmesh.data_vertex3f + e[2] * 3, texture->supercontents, texture->surfaceflags, texture);
					break;
				}
			}
		}
	}
}

void Mod_CollisionBIH_TraceLine(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	if (VectorCompare(start, end))
	{
		Mod_CollisionBIH_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
		return;
	}
	Mod_CollisionBIH_TraceLineShared(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask, &model->collision_bih);
}

void Mod_CollisionBIH_TraceBrush(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, colbrushf_t *thisbrush_start, colbrushf_t *thisbrush_end, int hitsupercontentsmask)
{
	const bih_t *bih;
	const bih_leaf_t *leaf;
	const bih_node_t *node;
	const colbrushf_t *brush;
	const int *e;
	const texture_t *texture;
	vec3_t start, end, startmins, startmaxs, endmins, endmaxs, mins, maxs;
	vec3_t nodebigmins, nodebigmaxs, nodestart, nodeend, sweepnodemins, sweepnodemaxs;
	vec_t d1, d2, d3, d4, f, nodestackline[1024][6];
	int axis, nodenum, nodestackpos = 0, nodestack[1024];

	if (mod_q3bsp_optimizedtraceline.integer && VectorCompare(thisbrush_start->mins, thisbrush_start->maxs) && VectorCompare(thisbrush_end->mins, thisbrush_end->maxs))
	{
		if (VectorCompare(thisbrush_start->mins, thisbrush_end->mins))
			Mod_CollisionBIH_TracePoint(model, frameblend, skeleton, trace, thisbrush_start->mins, hitsupercontentsmask);
		else
			Mod_CollisionBIH_TraceLine(model, frameblend, skeleton, trace, thisbrush_start->mins, thisbrush_end->mins, hitsupercontentsmask);
		return;
	}

	bih = &model->collision_bih;
	if(!bih->nodes)
		return;
	nodenum = bih->rootnode;

	// box trace, performed as brush trace
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;

	// calculate tracebox-like parameters for efficient culling
	VectorMAM(0.5f, thisbrush_start->mins, 0.5f, thisbrush_start->maxs, start);
	VectorMAM(0.5f, thisbrush_end->mins, 0.5f, thisbrush_end->maxs, end);
	VectorSubtract(thisbrush_start->mins, start, startmins);
	VectorSubtract(thisbrush_start->maxs, start, startmaxs);
	VectorSubtract(thisbrush_end->mins, end, endmins);
	VectorSubtract(thisbrush_end->maxs, end, endmaxs);
	mins[0] = min(startmins[0], endmins[0]);
	mins[1] = min(startmins[1], endmins[1]);
	mins[2] = min(startmins[2], endmins[2]);
	maxs[0] = max(startmaxs[0], endmaxs[0]);
	maxs[1] = max(startmaxs[1], endmaxs[1]);
	maxs[2] = max(startmaxs[2], endmaxs[2]);

	// push first node
	nodestackline[nodestackpos][0] = start[0];
	nodestackline[nodestackpos][1] = start[1];
	nodestackline[nodestackpos][2] = start[2];
	nodestackline[nodestackpos][3] = end[0];
	nodestackline[nodestackpos][4] = end[1];
	nodestackline[nodestackpos][5] = end[2];
	nodestack[nodestackpos++] = nodenum;
	while (nodestackpos)
	{
		nodenum = nodestack[--nodestackpos];
		node = bih->nodes + nodenum;
		VectorCopy(nodestackline[nodestackpos], nodestart);
		VectorCopy(nodestackline[nodestackpos] + 3, nodeend);
		sweepnodemins[0] = min(nodestart[0], nodeend[0]) + mins[0]; sweepnodemins[1] = min(nodestart[1], nodeend[1]) + mins[1]; sweepnodemins[2] = min(nodestart[2], nodeend[2]) + mins[2]; sweepnodemaxs[0] = max(nodestart[0], nodeend[0]) + maxs[0]; sweepnodemaxs[1] = max(nodestart[1], nodeend[1]) + maxs[1]; sweepnodemaxs[2] = max(nodestart[2], nodeend[2]) + maxs[2];
		if (!BoxesOverlap(sweepnodemins, sweepnodemaxs, node->mins, node->maxs))
			continue;
		if (node->type <= BIH_SPLITZ && nodestackpos+2 <= 1024)
		{
			// recurse children of the split
			axis = node->type - BIH_SPLITX;
			d1 = node->backmax - nodestart[axis] - mins[axis];
			d2 = node->backmax - nodeend[axis] - mins[axis];
			d3 = nodestart[axis] - node->frontmin + maxs[axis];
			d4 = nodeend[axis] - node->frontmin + maxs[axis];
			switch((d1 < 0) | ((d2 < 0) << 1) | ((d3 < 0) << 2) | ((d4 < 0) << 3))
			{
			case  0: /* >>>> */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  1: /* <>>> */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  2: /* ><>> */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  3: /* <<>> */                                                                                                                                                                                                                      VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  4: /* >><> */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  5: /* <><> */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  6: /* ><<> */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  7: /* <<<> */                                                                                                                                                                                                  f = d3 / (d3 - d4); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  8: /* >>>< */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case  9: /* <>>< */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 10: /* ><>< */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 11: /* <<>< */                                                                                                                                                                                                  f = d3 / (d3 - d4); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->front; break;
			case 12: /* >><< */                     VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 13: /* <><< */ f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos]); VectorCopy(              nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 14: /* ><<< */ f = d1 / (d1 - d2); VectorCopy(nodestart,             nodestackline[nodestackpos]); VectorLerp(nodestart, f, nodeend, nodestackline[nodestackpos] + 3); nodestack[nodestackpos++] = node->back;                                                                                                                                                                                                   break;
			case 15: /* <<<< */                                                                                                                                                                                                                                                                                                                                                                                                   break;
			}
		}
		else if (node->type == BIH_UNORDERED)
		{
			// calculate sweep bounds for this node
			// copy node bounds into local variables and expand to get Minkowski Sum of the two shapes
			VectorSubtract(node->mins, maxs, nodebigmins);
			VectorSubtract(node->maxs, mins, nodebigmaxs);
			// clip line to this node bounds
			axis = 0; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			axis = 1; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			axis = 2; d1 = nodestart[axis] - nodebigmins[axis]; d2 = nodeend[axis] - nodebigmins[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); } d1 = nodebigmaxs[axis] - nodestart[axis]; d2 = nodebigmaxs[axis] - nodeend[axis]; if (d1 < 0) { if (d2 < 0) continue; f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodestart); } else if (d2 < 0) { f = d1 / (d1 - d2); VectorLerp(nodestart, f, nodeend, nodeend); }
			// some of the line intersected the enlarged node box
			// calculate sweep bounds for this node
			sweepnodemins[0] = min(nodestart[0], nodeend[0]) + mins[0]; sweepnodemins[1] = min(nodestart[1], nodeend[1]) + mins[1]; sweepnodemins[2] = min(nodestart[2], nodeend[2]) + mins[2]; sweepnodemaxs[0] = max(nodestart[0], nodeend[0]) + maxs[0]; sweepnodemaxs[1] = max(nodestart[1], nodeend[1]) + maxs[1]; sweepnodemaxs[2] = max(nodestart[2], nodeend[2]) + maxs[2];
			for (axis = 0;axis < BIH_MAXUNORDEREDCHILDREN && node->children[axis] >= 0;axis++)
			{
				leaf = bih->leafs + node->children[axis];
				if (!BoxesOverlap(sweepnodemins, sweepnodemaxs, leaf->mins, leaf->maxs))
					continue;
				switch(leaf->type)
				{
				case BIH_BRUSH:
					brush = model->brush.data_brushes[leaf->itemindex].colbrushf;
					Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, brush, brush);
					break;
				case BIH_COLLISIONTRIANGLE:
					if (!mod_q3bsp_curves_collisions.integer)
						continue;
					e = model->brush.data_collisionelement3i + 3*leaf->itemindex;
					texture = model->data_textures + leaf->textureindex;
					Collision_TraceBrushTriangleFloat(trace, thisbrush_start, thisbrush_end, model->brush.data_collisionvertex3f + e[0] * 3, model->brush.data_collisionvertex3f + e[1] * 3, model->brush.data_collisionvertex3f + e[2] * 3, texture->supercontents, texture->surfaceflags, texture);
					break;
				case BIH_RENDERTRIANGLE:
					e = model->surfmesh.data_element3i + 3*leaf->itemindex;
					texture = model->data_textures + leaf->textureindex;
					Collision_TraceBrushTriangleFloat(trace, thisbrush_start, thisbrush_end, model->surfmesh.data_vertex3f + e[0] * 3, model->surfmesh.data_vertex3f + e[1] * 3, model->surfmesh.data_vertex3f + e[2] * 3, texture->supercontents, texture->surfaceflags, texture);
					break;
				}
			}
		}
	}
}

void Mod_CollisionBIH_TraceBox(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask)
{
	colboxbrushf_t thisbrush_start, thisbrush_end;
	vec3_t boxstartmins, boxstartmaxs, boxendmins, boxendmaxs;

	// box trace, performed as brush trace
	VectorAdd(start, boxmins, boxstartmins);
	VectorAdd(start, boxmaxs, boxstartmaxs);
	VectorAdd(end, boxmins, boxendmins);
	VectorAdd(end, boxmaxs, boxendmaxs);
	Collision_BrushForBox(&thisbrush_start, boxstartmins, boxstartmaxs, 0, 0, NULL);
	Collision_BrushForBox(&thisbrush_end, boxendmins, boxendmaxs, 0, 0, NULL);
	Mod_CollisionBIH_TraceBrush(model, frameblend, skeleton, trace, &thisbrush_start.brush, &thisbrush_end.brush, hitsupercontentsmask);
}


int Mod_CollisionBIH_PointSuperContents(struct model_s *model, int frame, const vec3_t point)
{
	trace_t trace;
	Mod_CollisionBIH_TracePoint(model, NULL, NULL, &trace, point, 0);
	return trace.startsupercontents;
}

void Mod_CollisionBIH_TracePoint_Mesh(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, int hitsupercontentsmask)
{
#if 0
	// broken - needs to be modified to count front faces and backfaces to figure out if it is in solid
	vec3_t end;
	int hitsupercontents;
	VectorSet(end, start[0], start[1], model->normalmins[2]);
#endif
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
#if 0
	Mod_CollisionBIH_TraceLine(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask);
	hitsupercontents = trace->hitsupercontents;
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	trace->startsupercontents = hitsupercontents;
#endif
}

int Mod_CollisionBIH_PointSuperContents_Mesh(struct model_s *model, int frame, const vec3_t start)
{
#if 0
	// broken - needs to be modified to count front faces and backfaces to figure out if it is in solid
	trace_t trace;
	vec3_t end;
	VectorSet(end, start[0], start[1], model->normalmins[2]);
	memset(&trace, 0, sizeof(trace));
	trace.fraction = 1;
	trace.realfraction = 1;
	trace.hitsupercontentsmask = 0;
	Mod_CollisionBIH_TraceLine(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask);
	return trace.hitsupercontents;
#else
	return 0;
#endif
}

static void Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace_t *trace, dp_model_t *model, mnode_t *node, const vec3_t point, int markframe)
{
	int i;
	mleaf_t *leaf;
	colbrushf_t *brush;
	// find which leaf the point is in
	while (node->plane)
		node = node->children[(node->plane->type < 3 ? point[node->plane->type] : DotProduct(point, node->plane->normal)) < node->plane->dist];
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

static void Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace_t *trace, dp_model_t *model, mnode_t *node, const vec3_t start, const vec3_t end, vec_t startfrac, vec_t endfrac, const vec3_t linestart, const vec3_t lineend, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
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
#if 0
		if (!BoxesOverlap(segmentmins, segmentmaxs, node->mins, node->maxs))
			return;
		Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model, node->children[0], start, end, startfrac, endfrac, linestart, lineend, markframe, segmentmins, segmentmaxs);
		node = node->children[1];
#else
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
#endif
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
#if 0
	if (!BoxesOverlap(segmentmins, segmentmaxs, leaf->mins, leaf->maxs))
		return;
#endif
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
			if (surface->num_collisiontriangles && surface->deprecatedq3collisionmarkframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs))
			{
				surface->deprecatedq3collisionmarkframe = markframe;
				Collision_TraceLineTriangleMeshFloat(trace, linestart, lineend, surface->num_collisiontriangles, surface->deprecatedq3data_collisionelement3i, surface->deprecatedq3data_collisionvertex3f, surface->deprecatedq3num_collisionbboxstride, surface->deprecatedq3data_collisionbbox6f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
			}
		}
	}
}

static void Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace_t *trace, dp_model_t *model, mnode_t *node, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int markframe, const vec3_t segmentmins, const vec3_t segmentmaxs)
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
#if 0
		if (!BoxesOverlap(segmentmins, segmentmaxs, node->mins, node->maxs))
			return;
		Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, model, node->children[0], thisbrush_start, thisbrush_end, markframe, segmentmins, segmentmaxs);
		node = node->children[1];
#else
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
#endif
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
#if 0
	if (!BoxesOverlap(segmentmins, segmentmaxs, leaf->mins, leaf->maxs))
		return;
#endif
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
			if (surface->num_collisiontriangles && surface->deprecatedq3collisionmarkframe != markframe && BoxesOverlap(nodesegmentmins, nodesegmentmaxs, surface->mins, surface->maxs))
			{
				surface->deprecatedq3collisionmarkframe = markframe;
				Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, surface->num_collisiontriangles, surface->deprecatedq3data_collisionelement3i, surface->deprecatedq3data_collisionvertex3f, surface->deprecatedq3num_collisionbboxstride, surface->deprecatedq3data_collisionbbox6f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
			}
		}
	}
}


static int markframe = 0;

static void Mod_Q3BSP_TracePoint(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, int hitsupercontentsmask)
{
	int i;
	q3mbrush_t *brush;
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	if (mod_collision_bih.integer)
		Mod_CollisionBIH_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
	else if (model->brush.submodel)
	{
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf)
				Collision_TracePointBrushFloat(trace, start, brush->colbrushf);
	}
	else
		Mod_Q3BSP_TracePoint_RecursiveBSPNode(trace, model, model->brush.data_nodes, start, ++markframe);
}

static void Mod_Q3BSP_TraceLine(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	int i;
	float segmentmins[3], segmentmaxs[3];
	msurface_t *surface;
	q3mbrush_t *brush;

	if (VectorCompare(start, end))
	{
		Mod_Q3BSP_TracePoint(model, frameblend, skeleton, trace, start, hitsupercontentsmask);
		return;
	}

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	segmentmins[0] = min(start[0], end[0]) - 1;
	segmentmins[1] = min(start[1], end[1]) - 1;
	segmentmins[2] = min(start[2], end[2]) - 1;
	segmentmaxs[0] = max(start[0], end[0]) + 1;
	segmentmaxs[1] = max(start[1], end[1]) + 1;
	segmentmaxs[2] = max(start[2], end[2]) + 1;
	if (mod_collision_bih.integer)
		Mod_CollisionBIH_TraceLine(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask);
	else if (model->brush.submodel)
	{
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && BoxesOverlap(segmentmins, segmentmaxs, brush->colbrushf->mins, brush->colbrushf->maxs))
				Collision_TraceLineBrushFloat(trace, start, end, brush->colbrushf, brush->colbrushf);
		if (mod_q3bsp_curves_collisions.integer)
			for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
				if (surface->num_collisiontriangles && BoxesOverlap(segmentmins, segmentmaxs, surface->mins, surface->maxs))
					Collision_TraceLineTriangleMeshFloat(trace, start, end, surface->num_collisiontriangles, surface->deprecatedq3data_collisionelement3i, surface->deprecatedq3data_collisionvertex3f, surface->deprecatedq3num_collisionbboxstride, surface->deprecatedq3data_collisionbbox6f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
	}
	else
		Mod_Q3BSP_TraceLine_RecursiveBSPNode(trace, model, model->brush.data_nodes, start, end, 0, 1, start, end, ++markframe, segmentmins, segmentmaxs);
}

void Mod_Q3BSP_TraceBrush(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, colbrushf_t *start, colbrushf_t *end, int hitsupercontentsmask)
{
	float segmentmins[3], segmentmaxs[3];
	int i;
	msurface_t *surface;
	q3mbrush_t *brush;

	if (mod_q3bsp_optimizedtraceline.integer && VectorCompare(start->mins, start->maxs) && VectorCompare(end->mins, end->maxs))
	{
		if (VectorCompare(start->mins, end->mins))
			Mod_Q3BSP_TracePoint(model, frameblend, skeleton, trace, start->mins, hitsupercontentsmask);
		else
			Mod_Q3BSP_TraceLine(model, frameblend, skeleton, trace, start->mins, end->mins, hitsupercontentsmask);
		return;
	}

	// box trace, performed as brush trace
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	segmentmins[0] = min(start->mins[0], end->mins[0]);
	segmentmins[1] = min(start->mins[1], end->mins[1]);
	segmentmins[2] = min(start->mins[2], end->mins[2]);
	segmentmaxs[0] = max(start->maxs[0], end->maxs[0]);
	segmentmaxs[1] = max(start->maxs[1], end->maxs[1]);
	segmentmaxs[2] = max(start->maxs[2], end->maxs[2]);
	if (mod_collision_bih.integer)
		Mod_CollisionBIH_TraceBrush(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask);
	else if (model->brush.submodel)
	{
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && BoxesOverlap(segmentmins, segmentmaxs, brush->colbrushf->mins, brush->colbrushf->maxs))
				Collision_TraceBrushBrushFloat(trace, start, end, brush->colbrushf, brush->colbrushf);
		if (mod_q3bsp_curves_collisions.integer)
			for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
				if (surface->num_collisiontriangles && BoxesOverlap(segmentmins, segmentmaxs, surface->mins, surface->maxs))
					Collision_TraceBrushTriangleMeshFloat(trace, start, end, surface->num_collisiontriangles, surface->deprecatedq3data_collisionelement3i, surface->deprecatedq3data_collisionvertex3f, surface->deprecatedq3num_collisionbboxstride, surface->deprecatedq3data_collisionbbox6f, surface->texture->supercontents, surface->texture->surfaceflags, surface->texture, segmentmins, segmentmaxs);
	}
	else
		Mod_Q3BSP_TraceBrush_RecursiveBSPNode(trace, model, model->brush.data_nodes, start, end, ++markframe, segmentmins, segmentmaxs);
}

static void Mod_Q3BSP_TraceBox(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask)
{
	colboxbrushf_t thisbrush_start, thisbrush_end;
	vec3_t boxstartmins, boxstartmaxs, boxendmins, boxendmaxs;

	// box trace, performed as brush trace
	VectorAdd(start, boxmins, boxstartmins);
	VectorAdd(start, boxmaxs, boxstartmaxs);
	VectorAdd(end, boxmins, boxendmins);
	VectorAdd(end, boxmaxs, boxendmaxs);
	Collision_BrushForBox(&thisbrush_start, boxstartmins, boxstartmaxs, 0, 0, NULL);
	Collision_BrushForBox(&thisbrush_end, boxendmins, boxendmaxs, 0, 0, NULL);
	Mod_Q3BSP_TraceBrush(model, frameblend, skeleton, trace, &thisbrush_start.brush, &thisbrush_end.brush, hitsupercontentsmask);
}

static int Mod_Q3BSP_PointSuperContents(struct model_s *model, int frame, const vec3_t point)
{
	int i;
	int supercontents = 0;
	q3mbrush_t *brush;
	if (mod_collision_bih.integer)
	{
		trace_t trace;
		Mod_Q3BSP_TracePoint(model, NULL, NULL, &trace, point, 0);
		supercontents = trace.startsupercontents;
	}
	// test if the point is inside each brush
	else if (model->brush.submodel)
	{
		// submodels are effectively one leaf
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && Collision_PointInsideBrushFloat(point, brush->colbrushf))
				supercontents |= brush->colbrushf->supercontents;
	}
	else
	{
		mnode_t *node = model->brush.data_nodes;
		mleaf_t *leaf;
		// find which leaf the point is in
		while (node->plane)
			node = node->children[(node->plane->type < 3 ? point[node->plane->type] : DotProduct(point, node->plane->normal)) < node->plane->dist];
		leaf = (mleaf_t *)node;
		// now check the brushes in the leaf
		for (i = 0;i < leaf->numleafbrushes;i++)
		{
			brush = model->brush.data_brushes + leaf->firstleafbrush[i];
			if (brush->colbrushf && Collision_PointInsideBrushFloat(point, brush->colbrushf))
				supercontents |= brush->colbrushf->supercontents;
		}
	}
	return supercontents;
}

void Mod_CollisionBIH_TraceLineAgainstSurfaces(dp_model_t *model, const frameblend_t *frameblend, const skeleton_t *skeleton, trace_t *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	Mod_CollisionBIH_TraceLineShared(model, frameblend, skeleton, trace, start, end, hitsupercontentsmask, &model->render_bih);
}


bih_t *Mod_MakeCollisionBIH(dp_model_t *model, qboolean userendersurfaces, bih_t *out)
{
	int j;
	int bihnumleafs;
	int bihmaxnodes;
	int brushindex;
	int triangleindex;
	int bihleafindex;
	int nummodelbrushes = model->nummodelbrushes;
	int nummodelsurfaces = model->nummodelsurfaces;
	const int *e;
	const int *collisionelement3i;
	const float *collisionvertex3f;
	const int *renderelement3i;
	const float *rendervertex3f;
	bih_leaf_t *bihleafs;
	bih_node_t *bihnodes;
	int *temp_leafsort;
	int *temp_leafsortscratch;
	const msurface_t *surface;
	const q3mbrush_t *brush;

	// find out how many BIH leaf nodes we need
	bihnumleafs = 0;
	if (userendersurfaces)
	{
		for (j = 0, surface = model->data_surfaces + model->firstmodelsurface;j < nummodelsurfaces;j++, surface++)
			bihnumleafs += surface->num_triangles;
	}
	else
	{
		for (brushindex = 0, brush = model->brush.data_brushes + brushindex+model->firstmodelbrush;brushindex < nummodelbrushes;brushindex++, brush++)
			if (brush->colbrushf)
				bihnumleafs++;
		for (j = 0, surface = model->data_surfaces + model->firstmodelsurface;j < nummodelsurfaces;j++, surface++)
		{
			if (surface->texture->basematerialflags & MATERIALFLAG_MESHCOLLISIONS)
				bihnumleafs += surface->num_triangles + surface->num_collisiontriangles;
			else
				bihnumleafs += surface->num_collisiontriangles;
		}
	}

	if (!bihnumleafs)
		return NULL;

	// allocate the memory for the BIH leaf nodes
	bihleafs = (bih_leaf_t *)Mem_Alloc(loadmodel->mempool, sizeof(bih_leaf_t) * bihnumleafs);

	// now populate the BIH leaf nodes
	bihleafindex = 0;

	// add render surfaces
	renderelement3i = model->surfmesh.data_element3i;
	rendervertex3f = model->surfmesh.data_vertex3f;
	for (j = 0, surface = model->data_surfaces + model->firstmodelsurface;j < nummodelsurfaces;j++, surface++)
	{
		for (triangleindex = 0, e = renderelement3i + 3*surface->num_firsttriangle;triangleindex < surface->num_triangles;triangleindex++, e += 3)
		{
			if (!userendersurfaces && !(surface->texture->basematerialflags & MATERIALFLAG_MESHCOLLISIONS))
				continue;
			bihleafs[bihleafindex].type = BIH_RENDERTRIANGLE;
			bihleafs[bihleafindex].textureindex = surface->texture - model->data_textures;
			bihleafs[bihleafindex].surfaceindex = surface - model->data_surfaces;
			bihleafs[bihleafindex].itemindex = triangleindex+surface->num_firsttriangle;
			bihleafs[bihleafindex].mins[0] = min(rendervertex3f[3*e[0]+0], min(rendervertex3f[3*e[1]+0], rendervertex3f[3*e[2]+0])) - 1;
			bihleafs[bihleafindex].mins[1] = min(rendervertex3f[3*e[0]+1], min(rendervertex3f[3*e[1]+1], rendervertex3f[3*e[2]+1])) - 1;
			bihleafs[bihleafindex].mins[2] = min(rendervertex3f[3*e[0]+2], min(rendervertex3f[3*e[1]+2], rendervertex3f[3*e[2]+2])) - 1;
			bihleafs[bihleafindex].maxs[0] = max(rendervertex3f[3*e[0]+0], max(rendervertex3f[3*e[1]+0], rendervertex3f[3*e[2]+0])) + 1;
			bihleafs[bihleafindex].maxs[1] = max(rendervertex3f[3*e[0]+1], max(rendervertex3f[3*e[1]+1], rendervertex3f[3*e[2]+1])) + 1;
			bihleafs[bihleafindex].maxs[2] = max(rendervertex3f[3*e[0]+2], max(rendervertex3f[3*e[1]+2], rendervertex3f[3*e[2]+2])) + 1;
			bihleafindex++;
		}
	}

	if (!userendersurfaces)
	{
		// add collision brushes
		for (brushindex = 0, brush = model->brush.data_brushes + brushindex+model->firstmodelbrush;brushindex < nummodelbrushes;brushindex++, brush++)
		{
			if (!brush->colbrushf)
				continue;
			bihleafs[bihleafindex].type = BIH_BRUSH;
			bihleafs[bihleafindex].textureindex = brush->texture - model->data_textures;
			bihleafs[bihleafindex].surfaceindex = -1;
			bihleafs[bihleafindex].itemindex = brushindex+model->firstmodelbrush;
			VectorCopy(brush->colbrushf->mins, bihleafs[bihleafindex].mins);
			VectorCopy(brush->colbrushf->maxs, bihleafs[bihleafindex].maxs);
			bihleafindex++;
		}

		// add collision surfaces
		collisionelement3i = model->brush.data_collisionelement3i;
		collisionvertex3f = model->brush.data_collisionvertex3f;
		for (j = 0, surface = model->data_surfaces + model->firstmodelsurface;j < nummodelsurfaces;j++, surface++)
		{
			for (triangleindex = 0, e = collisionelement3i + 3*surface->num_firstcollisiontriangle;triangleindex < surface->num_collisiontriangles;triangleindex++, e += 3)
			{
				bihleafs[bihleafindex].type = BIH_COLLISIONTRIANGLE;
				bihleafs[bihleafindex].textureindex = surface->texture - model->data_textures;
				bihleafs[bihleafindex].surfaceindex = surface - model->data_surfaces;
				bihleafs[bihleafindex].itemindex = triangleindex+surface->num_firstcollisiontriangle;
				bihleafs[bihleafindex].mins[0] = min(collisionvertex3f[3*e[0]+0], min(collisionvertex3f[3*e[1]+0], collisionvertex3f[3*e[2]+0])) - 1;
				bihleafs[bihleafindex].mins[1] = min(collisionvertex3f[3*e[0]+1], min(collisionvertex3f[3*e[1]+1], collisionvertex3f[3*e[2]+1])) - 1;
				bihleafs[bihleafindex].mins[2] = min(collisionvertex3f[3*e[0]+2], min(collisionvertex3f[3*e[1]+2], collisionvertex3f[3*e[2]+2])) - 1;
				bihleafs[bihleafindex].maxs[0] = max(collisionvertex3f[3*e[0]+0], max(collisionvertex3f[3*e[1]+0], collisionvertex3f[3*e[2]+0])) + 1;
				bihleafs[bihleafindex].maxs[1] = max(collisionvertex3f[3*e[0]+1], max(collisionvertex3f[3*e[1]+1], collisionvertex3f[3*e[2]+1])) + 1;
				bihleafs[bihleafindex].maxs[2] = max(collisionvertex3f[3*e[0]+2], max(collisionvertex3f[3*e[1]+2], collisionvertex3f[3*e[2]+2])) + 1;
				bihleafindex++;
			}
		}
	}

	// allocate buffers for the produced and temporary data
	bihmaxnodes = bihnumleafs + 1;
	bihnodes = (bih_node_t *)Mem_Alloc(loadmodel->mempool, sizeof(bih_node_t) * bihmaxnodes);
	temp_leafsort = (int *)Mem_Alloc(loadmodel->mempool, sizeof(int) * bihnumleafs * 2);
	temp_leafsortscratch = temp_leafsort + bihnumleafs;

	// now build it
	BIH_Build(out, bihnumleafs, bihleafs, bihmaxnodes, bihnodes, temp_leafsort, temp_leafsortscratch);

	// we're done with the temporary data
	Mem_Free(temp_leafsort);

	// resize the BIH nodes array if it over-allocated
	if (out->maxnodes > out->numnodes)
	{
		out->maxnodes = out->numnodes;
		out->nodes = (bih_node_t *)Mem_Realloc(loadmodel->mempool, out->nodes, out->numnodes * sizeof(bih_node_t));
	}

	return out;
}

static int Mod_Q3BSP_SuperContentsFromNativeContents(dp_model_t *model, int nativecontents)
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
	if (nativecontents & CONTENTSQ3_BOTCLIP)
		supercontents |= SUPERCONTENTS_BOTCLIP;
	if (!(nativecontents & CONTENTSQ3_TRANSLUCENT))
		supercontents |= SUPERCONTENTS_OPAQUE;
	return supercontents;
}

static int Mod_Q3BSP_NativeContentsFromSuperContents(dp_model_t *model, int supercontents)
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
	if (supercontents & SUPERCONTENTS_BOTCLIP)
		nativecontents |= CONTENTSQ3_BOTCLIP;
	if (!(supercontents & SUPERCONTENTS_OPAQUE))
		nativecontents |= CONTENTSQ3_TRANSLUCENT;
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

void Mod_Q3BSP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int i, j, lumps;
	q3dheader_t *header;
	float corner[3], yawradius, modelradius;

	mod->modeldatatypestring = "Q3BSP";

	mod->type = mod_brushq3;
	mod->numframes = 2; // although alternate textures are not supported it is annoying to complain about no such frame 1
	mod->numskins = 1;

	header = (q3dheader_t *)buffer;
	if((char *) bufferend < (char *) buffer + sizeof(q3dheader_t))
		Host_Error("Mod_Q3BSP_Load: %s is smaller than its header", mod->name);

	i = LittleLong(header->version);
	if (i != Q3BSPVERSION && i != Q3BSPVERSION_IG && i != Q3BSPVERSION_LIVE)
		Host_Error("Mod_Q3BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q3BSPVERSION);

	mod->soundfromcenter = true;
	mod->TraceBox = Mod_Q3BSP_TraceBox;
	mod->TraceBrush = Mod_Q3BSP_TraceBrush;
	mod->TraceLine = Mod_Q3BSP_TraceLine;
	mod->TracePoint = Mod_Q3BSP_TracePoint;
	mod->PointSuperContents = Mod_Q3BSP_PointSuperContents;
	mod->TraceLineAgainstSurfaces = Mod_CollisionBIH_TraceLine;
	mod->brush.TraceLineOfSight = Mod_Q3BSP_TraceLineOfSight;
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
	mod->brush.AmbientSoundLevelsForPoint = NULL;
	mod->brush.RoundUpToHullSize = NULL;
	mod->brush.PointInLeaf = Mod_Q1BSP_PointInLeaf;
	mod->Draw = R_Q1BSP_Draw;
	mod->DrawDepth = R_Q1BSP_DrawDepth;
	mod->DrawDebug = R_Q1BSP_DrawDebug;
	mod->DrawPrepass = R_Q1BSP_DrawPrepass;
	mod->GetLightInfo = R_Q1BSP_GetLightInfo;
	mod->CompileShadowMap = R_Q1BSP_CompileShadowMap;
	mod->DrawShadowMap = R_Q1BSP_DrawShadowMap;
	mod->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	mod->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	mod->DrawLight = R_Q1BSP_DrawLight;

	mod_base = (unsigned char *)header;

	// swap all the lumps
	header->ident = LittleLong(header->ident);
	header->version = LittleLong(header->version);
	lumps = (header->version == Q3BSPVERSION_LIVE) ? Q3HEADER_LUMPS_LIVE : Q3HEADER_LUMPS;
	for (i = 0;i < lumps;i++)
	{
		j = (header->lumps[i].fileofs = LittleLong(header->lumps[i].fileofs));
		if((char *) bufferend < (char *) buffer + j)
			Host_Error("Mod_Q3BSP_Load: %s has a lump that starts outside the file!", mod->name);
		j += (header->lumps[i].filelen = LittleLong(header->lumps[i].filelen));
		if((char *) bufferend < (char *) buffer + j)
			Host_Error("Mod_Q3BSP_Load: %s has a lump that ends outside the file!", mod->name);
	}
	/*
	 * NO, do NOT clear them!
	 * they contain actual data referenced by other stuff.
	 * Instead, before using the advertisements lump, check header->versio
	 * again!
	 * Sorry, but otherwise it breaks memory of the first lump.
	for (i = lumps;i < Q3HEADER_LUMPS_MAX;i++)
	{
		header->lumps[i].fileofs = 0;
		header->lumps[i].filelen = 0;
	}
	*/

	mod->brush.qw_md4sum = 0;
	mod->brush.qw_md4sum2 = 0;
	for (i = 0;i < lumps;i++)
	{
		if (i == Q3LUMP_ENTITIES)
			continue;
		mod->brush.qw_md4sum ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);
		if (i == Q3LUMP_PVS || i == Q3LUMP_LEAFS || i == Q3LUMP_NODES)
			continue;
		mod->brush.qw_md4sum2 ^= Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);

		// all this checksumming can take a while, so let's send keepalives here too
		CL_KeepaliveMessage(false);
	}

	Mod_Q3BSP_LoadEntities(&header->lumps[Q3LUMP_ENTITIES]);
	Mod_Q3BSP_LoadTextures(&header->lumps[Q3LUMP_TEXTURES]);
	Mod_Q3BSP_LoadPlanes(&header->lumps[Q3LUMP_PLANES]);
	if (header->version == Q3BSPVERSION_IG)
		Mod_Q3BSP_LoadBrushSides_IG(&header->lumps[Q3LUMP_BRUSHSIDES]);
	else
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
	if (mod_bsp_portalize.integer)
		Mod_Q1BSP_MakePortals();

	// FIXME: shader alpha should replace r_wateralpha support in q3bsp
	loadmodel->brush.supportwateralpha = true;

	// make a single combined shadow mesh to allow optimized shadow volume creation
	Mod_Q1BSP_CreateShadowMesh(loadmodel);

	loadmodel->brush.num_leafs = 0;
	Mod_Q3BSP_RecursiveFindNumLeafs(loadmodel->brush.data_nodes);

	if (loadmodel->brush.numsubmodels)
		loadmodel->brush.submodels = (dp_model_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brush.numsubmodels * sizeof(dp_model_t *));

	mod = loadmodel;
	for (i = 0;i < loadmodel->brush.numsubmodels;i++)
	{
		if (i > 0)
		{
			char name[10];
			// duplicate the basic information
			dpsnprintf(name, sizeof(name), "*%i", i);
			mod = Mod_FindName(name, loadmodel->name);
			// copy the base model to this one
			*mod = *loadmodel;
			// rename the clone back to its proper name
			strlcpy(mod->name, name, sizeof(mod->name));
			mod->brush.parentmodel = loadmodel;
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
			mod->brush.GetPVS = NULL;
			mod->brush.FatPVS = NULL;
			mod->brush.BoxTouchingPVS = NULL;
			mod->brush.BoxTouchingLeafPVS = NULL;
			mod->brush.BoxTouchingVisibleLeafs = NULL;
			mod->brush.FindBoxClusters = NULL;
			mod->brush.LightPoint = NULL;
			mod->brush.AmbientSoundLevelsForPoint = NULL;
		}
		mod->brush.submodel = i;
		if (loadmodel->brush.submodels)
			loadmodel->brush.submodels[i] = mod;

		// make the model surface list (used by shadowing/lighting)
		mod->firstmodelsurface = mod->brushq3.data_models[i].firstface;
		mod->nummodelsurfaces = mod->brushq3.data_models[i].numfaces;
		mod->firstmodelbrush = mod->brushq3.data_models[i].firstbrush;
		mod->nummodelbrushes = mod->brushq3.data_models[i].numbrushes;
		mod->sortedmodelsurfaces = (int *)Mem_Alloc(loadmodel->mempool, mod->nummodelsurfaces * sizeof(*mod->sortedmodelsurfaces));
		Mod_MakeSortedSurfaces(mod);

		VectorCopy(mod->brushq3.data_models[i].mins, mod->normalmins);
		VectorCopy(mod->brushq3.data_models[i].maxs, mod->normalmaxs);
		// enlarge the bounding box to enclose all geometry of this model,
		// because q3map2 sometimes lies (mostly to affect the lightgrid),
		// which can in turn mess up the farclip (as well as culling when
		// outside the level - an unimportant concern)

		//printf("Editing model %d... BEFORE re-bounding: %f %f %f - %f %f %f\n", i, mod->normalmins[0], mod->normalmins[1], mod->normalmins[2], mod->normalmaxs[0], mod->normalmaxs[1], mod->normalmaxs[2]);
		for (j = 0;j < mod->nummodelsurfaces;j++)
		{
			const msurface_t *surface = mod->data_surfaces + j + mod->firstmodelsurface;
			const float *v = mod->surfmesh.data_vertex3f + 3 * surface->num_firstvertex;
			int k;
			if (!surface->num_vertices)
				continue;
			for (k = 0;k < surface->num_vertices;k++, v += 3)
			{
				mod->normalmins[0] = min(mod->normalmins[0], v[0]);
				mod->normalmins[1] = min(mod->normalmins[1], v[1]);
				mod->normalmins[2] = min(mod->normalmins[2], v[2]);
				mod->normalmaxs[0] = max(mod->normalmaxs[0], v[0]);
				mod->normalmaxs[1] = max(mod->normalmaxs[1], v[1]);
				mod->normalmaxs[2] = max(mod->normalmaxs[2], v[2]);
			}
		}
		//printf("Editing model %d... AFTER re-bounding: %f %f %f - %f %f %f\n", i, mod->normalmins[0], mod->normalmins[1], mod->normalmins[2], mod->normalmaxs[0], mod->normalmaxs[1], mod->normalmaxs[2]);
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

		// this gets altered below if sky or water is used
		mod->DrawSky = NULL;
		mod->DrawAddWaterPlanes = NULL;

		for (j = 0;j < mod->nummodelsurfaces;j++)
			if (mod->data_surfaces[j + mod->firstmodelsurface].texture->basematerialflags & MATERIALFLAG_SKY)
				break;
		if (j < mod->nummodelsurfaces)
			mod->DrawSky = R_Q1BSP_DrawSky;

		for (j = 0;j < mod->nummodelsurfaces;j++)
			if (mod->data_surfaces[j + mod->firstmodelsurface].texture->basematerialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA))
				break;
		if (j < mod->nummodelsurfaces)
			mod->DrawAddWaterPlanes = R_Q1BSP_DrawAddWaterPlanes;

		Mod_MakeCollisionBIH(mod, false, &mod->collision_bih);
		Mod_MakeCollisionBIH(mod, true, &mod->render_bih);

		// generate VBOs and other shared data before cloning submodels
		if (i == 0)
			Mod_BuildVBOs();
	}

	Con_DPrintf("Stats for q3bsp model \"%s\": %i faces, %i nodes, %i leafs, %i clusters, %i clusterportals, mesh: %i vertices, %i triangles, %i surfaces\n", loadmodel->name, loadmodel->num_surfaces, loadmodel->brush.num_nodes, loadmodel->brush.num_leafs, mod->brush.num_pvsclusters, loadmodel->brush.num_portals, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->num_surfaces);
}

void Mod_IBSP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int i = LittleLong(((int *)buffer)[1]);
	if (i == Q3BSPVERSION || i == Q3BSPVERSION_IG || i == Q3BSPVERSION_LIVE)
		Mod_Q3BSP_Load(mod,buffer, bufferend);
	else if (i == Q2BSPVERSION)
		Mod_Q2BSP_Load(mod,buffer, bufferend);
	else
		Host_Error("Mod_IBSP_Load: unknown/unsupported version %i", i);
}

void Mod_MAP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	Host_Error("Mod_MAP_Load: not yet implemented");
}

typedef struct objvertex_s
{
	int nextindex;
	int submodelindex;
	int textureindex;
	float v[3];
	float vt[2];
	float vn[3];
}
objvertex_t;

static unsigned char nobsp_pvs[1] = {1};

void Mod_OBJ_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	const char *textbase = (char *)buffer, *text = textbase;
	char *s;
	char *argv[512];
	char line[1024];
	char materialname[MAX_QPATH];
	int i, j, l, numvertices, firstvertex, firsttriangle, elementindex, vertexindex, surfacevertices, surfacetriangles, surfaceelements, submodelindex = 0;
	int index1, index2, index3;
	objvertex_t vfirst, vprev, vcurrent;
	int argc;
	int linelen;
	int numtriangles = 0;
	int maxtriangles = 0;
	objvertex_t *vertices = NULL;
	int linenumber = 0;
	int maxtextures = 0, numtextures = 0, textureindex = 0;
	int maxv = 0, numv = 1;
	int maxvt = 0, numvt = 1;
	int maxvn = 0, numvn = 1;
	char *texturenames = NULL;
	float dist, modelradius, modelyawradius, yawradius;
	float *v = NULL;
	float *vt = NULL;
	float *vn = NULL;
	float mins[3];
	float maxs[3];
	float corner[3];
	objvertex_t *thisvertex = NULL;
	int vertexhashindex;
	int *vertexhashtable = NULL;
	objvertex_t *vertexhashdata = NULL;
	objvertex_t *vdata = NULL;
	int vertexhashsize = 0;
	int vertexhashcount = 0;
	skinfile_t *skinfiles = NULL;
	unsigned char *data = NULL;
	int *submodelfirstsurface;
	msurface_t *surface;
	msurface_t *tempsurfaces;

	memset(&vfirst, 0, sizeof(vfirst));
	memset(&vprev, 0, sizeof(vprev));
	memset(&vcurrent, 0, sizeof(vcurrent));

	dpsnprintf(materialname, sizeof(materialname), "%s", loadmodel->name);

	loadmodel->modeldatatypestring = "OBJ";

	loadmodel->type = mod_obj;
	loadmodel->soundfromcenter = true;
	loadmodel->TraceBox = Mod_CollisionBIH_TraceBox;
	loadmodel->TraceBrush = Mod_CollisionBIH_TraceBrush;
	loadmodel->TraceLine = Mod_CollisionBIH_TraceLine;
	loadmodel->TracePoint = Mod_CollisionBIH_TracePoint_Mesh;
	loadmodel->TraceLineAgainstSurfaces = Mod_CollisionBIH_TraceLine;
	loadmodel->PointSuperContents = Mod_CollisionBIH_PointSuperContents_Mesh;
	loadmodel->brush.TraceLineOfSight = NULL;
	loadmodel->brush.SuperContentsFromNativeContents = NULL;
	loadmodel->brush.NativeContentsFromSuperContents = NULL;
	loadmodel->brush.GetPVS = NULL;
	loadmodel->brush.FatPVS = NULL;
	loadmodel->brush.BoxTouchingPVS = NULL;
	loadmodel->brush.BoxTouchingLeafPVS = NULL;
	loadmodel->brush.BoxTouchingVisibleLeafs = NULL;
	loadmodel->brush.FindBoxClusters = NULL;
	loadmodel->brush.LightPoint = NULL;
	loadmodel->brush.FindNonSolidLocation = NULL;
	loadmodel->brush.AmbientSoundLevelsForPoint = NULL;
	loadmodel->brush.RoundUpToHullSize = NULL;
	loadmodel->brush.PointInLeaf = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->DrawDepth = R_Q1BSP_DrawDepth;
	loadmodel->DrawDebug = R_Q1BSP_DrawDebug;
	loadmodel->DrawPrepass = R_Q1BSP_DrawPrepass;
	loadmodel->GetLightInfo = R_Q1BSP_GetLightInfo;
	loadmodel->CompileShadowMap = R_Q1BSP_CompileShadowMap;
	loadmodel->DrawShadowMap = R_Q1BSP_DrawShadowMap;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;

	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;

	// make skinscenes for the skins (no groups)
	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

	VectorClear(mins);
	VectorClear(maxs);

	// we always have model 0, i.e. the first "submodel"
	loadmodel->brush.numsubmodels = 1;

	// parse the OBJ text now
	for(;;)
	{
		static char emptyarg[1] = "";
		if (!*text)
			break;
		linenumber++;
		linelen = 0;
		for (linelen = 0;text[linelen] && text[linelen] != '\r' && text[linelen] != '\n';linelen++)
			line[linelen] = text[linelen];
		line[linelen] = 0;
		for (argc = 0;argc < 4;argc++)
			argv[argc] = emptyarg;
		argc = 0;
		s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		while (*s)
		{
			argv[argc++] = s;
			while (*s > ' ')
				s++;
			if (!*s)
				break;
			*s++ = 0;
			while (*s == ' ' || *s == '\t')
				s++;
		}
		text += linelen;
		if (*text == '\r')
			text++;
		if (*text == '\n')
			text++;
		if (!argc)
			continue;
		if (argv[0][0] == '#')
			continue;
		if (!strcmp(argv[0], "v"))
		{
			if (maxv <= numv)
			{
				maxv = max(maxv * 2, 1024);
				v = (float *)Mem_Realloc(tempmempool, v, maxv * sizeof(float[3]));
			}
			v[numv*3+0] = atof(argv[1]);
			v[numv*3+2] = atof(argv[2]);
			v[numv*3+1] = atof(argv[3]);
			numv++;
		}
		else if (!strcmp(argv[0], "vt"))
		{
			if (maxvt <= numvt)
			{
				maxvt = max(maxvt * 2, 1024);
				vt = (float *)Mem_Realloc(tempmempool, vt, maxvt * sizeof(float[2]));
			}
			vt[numvt*2+0] = atof(argv[1]);
			vt[numvt*2+1] = 1-atof(argv[2]);
			numvt++;
		}
		else if (!strcmp(argv[0], "vn"))
		{
			if (maxvn <= numvn)
			{
				maxvn = max(maxvn * 2, 1024);
				vn = (float *)Mem_Realloc(tempmempool, vn, maxvn * sizeof(float[3]));
			}
			vn[numvn*3+0] = atof(argv[1]);
			vn[numvn*3+2] = atof(argv[2]);
			vn[numvn*3+1] = atof(argv[3]);
			numvn++;
		}
		else if (!strcmp(argv[0], "f"))
		{
			if (!numtextures)
			{
				if (maxtextures <= numtextures)
				{
					maxtextures = max(maxtextures * 2, 256);
					texturenames = (char *)Mem_Realloc(loadmodel->mempool, texturenames, maxtextures * MAX_QPATH);
				}
				textureindex = numtextures++;
				strlcpy(texturenames + textureindex*MAX_QPATH, loadmodel->name, MAX_QPATH);
			}
			for (j = 1;j < argc;j++)
			{
				index1 = atoi(argv[j]);
				while(argv[j][0] && argv[j][0] != '/')
					argv[j]++;
				if (argv[j][0])
					argv[j]++;
				index2 = atoi(argv[j]);
				while(argv[j][0] && argv[j][0] != '/')
					argv[j]++;
				if (argv[j][0])
					argv[j]++;
				index3 = atoi(argv[j]);
				// negative refers to a recent vertex
				// zero means not specified
				// positive means an absolute vertex index
				if (index1 < 0)
					index1 = numv - index1;
				if (index2 < 0)
					index2 = numvt - index2;
				if (index3 < 0)
					index3 = numvn - index3;
				vcurrent.nextindex = -1;
				vcurrent.textureindex = textureindex;
				vcurrent.submodelindex = submodelindex;
				if (v && index1 >= 0 && index1 < numv)
					VectorCopy(v + 3*index1, vcurrent.v);
				if (vt && index2 >= 0 && index2 < numvt)
					Vector2Copy(vt + 2*index2, vcurrent.vt);
				if (vn && index3 >= 0 && index3 < numvn)
					VectorCopy(vn + 3*index3, vcurrent.vn);
				if (numtriangles == 0)
				{
					VectorCopy(vcurrent.v, mins);
					VectorCopy(vcurrent.v, maxs);
				}
				else
				{
					mins[0] = min(mins[0], vcurrent.v[0]);
					mins[1] = min(mins[1], vcurrent.v[1]);
					mins[2] = min(mins[2], vcurrent.v[2]);
					maxs[0] = max(maxs[0], vcurrent.v[0]);
					maxs[1] = max(maxs[1], vcurrent.v[1]);
					maxs[2] = max(maxs[2], vcurrent.v[2]);
				}
				if (j == 1)
					vfirst = vcurrent;
				else if (j >= 3)
				{
					if (maxtriangles <= numtriangles)
					{
						maxtriangles = max(maxtriangles * 2, 32768);
						vertices = (objvertex_t*)Mem_Realloc(loadmodel->mempool, vertices, maxtriangles * sizeof(objvertex_t[3]));
					}
					vertices[numtriangles*3+0] = vfirst;
					vertices[numtriangles*3+1] = vprev;
					vertices[numtriangles*3+2] = vcurrent;
					numtriangles++;
				}
				vprev = vcurrent;
			}
		}
		else if (!strcmp(argv[0], "o") || !strcmp(argv[0], "g"))
		{
			submodelindex = atof(argv[1]);
			loadmodel->brush.numsubmodels = max(submodelindex + 1, loadmodel->brush.numsubmodels);
		}
		else if (!strcmp(argv[0], "usemtl"))
		{
			for (i = 0;i < numtextures;i++)
				if (!strcmp(texturenames+i*MAX_QPATH, argv[1]))
					break;
			if (i < numtextures)
				textureindex = i;
			else
			{
				if (maxtextures <= numtextures)
				{
					maxtextures = max(maxtextures * 2, 256);
					texturenames = (char *)Mem_Realloc(loadmodel->mempool, texturenames, maxtextures * MAX_QPATH);
				}
				textureindex = numtextures++;
				strlcpy(texturenames + textureindex*MAX_QPATH, argv[1], MAX_QPATH);
			}
		}
	}

	// now that we have the OBJ data loaded as-is, we can convert it

	// copy the model bounds, then enlarge the yaw and rotated bounds according to radius
	VectorCopy(mins, loadmodel->normalmins);
	VectorCopy(maxs, loadmodel->normalmaxs);
	dist = max(fabs(loadmodel->normalmins[0]), fabs(loadmodel->normalmaxs[0]));
	modelyawradius = max(fabs(loadmodel->normalmins[1]), fabs(loadmodel->normalmaxs[1]));
	modelyawradius = dist*dist+modelyawradius*modelyawradius;
	modelradius = max(fabs(loadmodel->normalmins[2]), fabs(loadmodel->normalmaxs[2]));
	modelradius = modelyawradius + modelradius * modelradius;
	modelyawradius = sqrt(modelyawradius);
	modelradius = sqrt(modelradius);
	loadmodel->yawmins[0] = loadmodel->yawmins[1] = -modelyawradius;
	loadmodel->yawmins[2] = loadmodel->normalmins[2];
	loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] =  modelyawradius;
	loadmodel->yawmaxs[2] = loadmodel->normalmaxs[2];
	loadmodel->rotatedmins[0] = loadmodel->rotatedmins[1] = loadmodel->rotatedmins[2] = -modelradius;
	loadmodel->rotatedmaxs[0] = loadmodel->rotatedmaxs[1] = loadmodel->rotatedmaxs[2] =  modelradius;
	loadmodel->radius = modelradius;
	loadmodel->radius2 = modelradius * modelradius;

	// allocate storage for triangles
	loadmodel->surfmesh.data_element3i = (int *)Mem_Alloc(loadmodel->mempool, numtriangles * sizeof(int[3]));
	// allocate vertex hash structures to build an optimal vertex subset
	vertexhashsize = numtriangles*2;
	vertexhashtable = (int *)Mem_Alloc(loadmodel->mempool, sizeof(int) * vertexhashsize);
	memset(vertexhashtable, 0xFF, sizeof(int) * vertexhashsize);
	vertexhashdata = (objvertex_t *)Mem_Alloc(loadmodel->mempool, sizeof(*vertexhashdata) * numtriangles*3);
	vertexhashcount = 0;

	// gather surface stats for assigning vertex/triangle ranges
	firstvertex = 0;
	firsttriangle = 0;
	elementindex = 0;
	loadmodel->num_surfaces = 0;
	// allocate storage for the worst case number of surfaces, later we resize
	tempsurfaces = (msurface_t *)Mem_Alloc(loadmodel->mempool, numtextures * loadmodel->brush.numsubmodels * sizeof(msurface_t));
	submodelfirstsurface = (int *)Mem_Alloc(loadmodel->mempool, (loadmodel->brush.numsubmodels+1) * sizeof(int));
	surface = tempsurfaces;
	for (submodelindex = 0;submodelindex < loadmodel->brush.numsubmodels;submodelindex++)
	{
		submodelfirstsurface[submodelindex] = loadmodel->num_surfaces;
		for (textureindex = 0;textureindex < numtextures;textureindex++)
		{
			for (vertexindex = 0;vertexindex < numtriangles*3;vertexindex++)
			{
				thisvertex = vertices + vertexindex;
				if (thisvertex->submodelindex == submodelindex && thisvertex->textureindex == textureindex)
					break;
			}
			// skip the surface creation if there are no triangles for it
			if (vertexindex == numtriangles*3)
				continue;
			// create a surface for these vertices
			surfacevertices = 0;
			surfaceelements = 0;
			// we hack in a texture index in the surface to be fixed up later...
			surface->texture = (texture_t *)((size_t)textureindex);
			// calculate bounds as we go
			VectorCopy(thisvertex->v, surface->mins);
			VectorCopy(thisvertex->v, surface->maxs);
			for (;vertexindex < numtriangles*3;vertexindex++)
			{
				thisvertex = vertices + vertexindex;
				if (thisvertex->submodelindex != submodelindex)
					continue;
				if (thisvertex->textureindex != textureindex)
					continue;
				// add vertex to surface bounds
				surface->mins[0] = min(surface->mins[0], thisvertex->v[0]);
				surface->mins[1] = min(surface->mins[1], thisvertex->v[1]);
				surface->mins[2] = min(surface->mins[2], thisvertex->v[2]);
				surface->maxs[0] = max(surface->maxs[0], thisvertex->v[0]);
				surface->maxs[1] = max(surface->maxs[1], thisvertex->v[1]);
				surface->maxs[2] = max(surface->maxs[2], thisvertex->v[2]);
				// add the vertex if it is not found in the merged set, and
				// get its index (triangle element) for the surface
				vertexhashindex = (unsigned int)(thisvertex->v[0] * 3571 + thisvertex->v[0] * 1777 + thisvertex->v[0] * 457) % (unsigned int)vertexhashsize;
				for (i = vertexhashtable[vertexhashindex];i >= 0;i = vertexhashdata[i].nextindex)
				{
					vdata = vertexhashdata + i;
					if (vdata->submodelindex == thisvertex->submodelindex && vdata->textureindex == thisvertex->textureindex && VectorCompare(thisvertex->v, vdata->v) && VectorCompare(thisvertex->vn, vdata->vn) && Vector2Compare(thisvertex->vt, vdata->vt))
						break;
				}
				if (i < 0)
				{
					i = vertexhashcount++;
					vdata = vertexhashdata + i;
					*vdata = *thisvertex;
					vdata->nextindex = vertexhashtable[vertexhashindex];
					vertexhashtable[vertexhashindex] = i;
					surfacevertices++;
				}
				loadmodel->surfmesh.data_element3i[elementindex++] = i;
				surfaceelements++;
			}
			surfacetriangles = surfaceelements / 3;
			surface->num_vertices = surfacevertices;
			surface->num_triangles = surfacetriangles;
			surface->num_firstvertex = firstvertex;
			surface->num_firsttriangle = firsttriangle;
			firstvertex += surface->num_vertices;
			firsttriangle += surface->num_triangles;
			surface++;
			loadmodel->num_surfaces++;
		}
	}
	submodelfirstsurface[submodelindex] = loadmodel->num_surfaces;
	numvertices = firstvertex;
	loadmodel->data_surfaces = (msurface_t *)Mem_Realloc(loadmodel->mempool, tempsurfaces, loadmodel->num_surfaces * sizeof(msurface_t));
	tempsurfaces = NULL;

	// allocate storage for final mesh data
	loadmodel->num_textures = numtextures * loadmodel->numskins;
	loadmodel->num_texturesperskin = numtextures;
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(int) + loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t) + numtriangles * sizeof(int[3]) + (numvertices <= 65536 ? numtriangles * sizeof(unsigned short[3]) : 0) + (r_enableshadowvolumes.integer ? numtriangles * sizeof(int[3]) : 0) + numvertices * sizeof(float[14]) + loadmodel->brush.numsubmodels * sizeof(dp_model_t *));
	loadmodel->brush.submodels = (dp_model_t **)data;data += loadmodel->brush.numsubmodels * sizeof(dp_model_t *);
	loadmodel->sortedmodelsurfaces = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->data_textures = (texture_t *)data;data += loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t);
	loadmodel->surfmesh.num_vertices = numvertices;
	loadmodel->surfmesh.num_triangles = numtriangles;
	if (r_enableshadowvolumes.integer)
		loadmodel->surfmesh.data_neighbor3i = (int *)data;data += numtriangles * sizeof(int[3]);
	loadmodel->surfmesh.data_vertex3f = (float *)data;data += numvertices * sizeof(float[3]);
	loadmodel->surfmesh.data_svector3f = (float *)data;data += numvertices * sizeof(float[3]);
	loadmodel->surfmesh.data_tvector3f = (float *)data;data += numvertices * sizeof(float[3]);
	loadmodel->surfmesh.data_normal3f = (float *)data;data += numvertices * sizeof(float[3]);
	loadmodel->surfmesh.data_texcoordtexture2f = (float *)data;data += numvertices * sizeof(float[2]);
	if (loadmodel->surfmesh.num_vertices <= 65536)
		loadmodel->surfmesh.data_element3s = (unsigned short *)data;data += loadmodel->surfmesh.num_triangles * sizeof(unsigned short[3]);

	for (j = 0;j < loadmodel->surfmesh.num_vertices;j++)
	{
		VectorCopy(vertexhashdata[j].v, loadmodel->surfmesh.data_vertex3f + 3*j);
		VectorCopy(vertexhashdata[j].vn, loadmodel->surfmesh.data_normal3f + 3*j);
		Vector2Copy(vertexhashdata[j].vt, loadmodel->surfmesh.data_texcoordtexture2f + 2*j);
	}

	// load the textures
	for (textureindex = 0;textureindex < numtextures;textureindex++)
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures + textureindex, skinfiles, texturenames + textureindex*MAX_QPATH, texturenames + textureindex*MAX_QPATH);
	Mod_FreeSkinFiles(skinfiles);

	// set the surface textures to their real values now that we loaded them...
	for (i = 0;i < loadmodel->num_surfaces;i++)
		loadmodel->data_surfaces[i].texture = loadmodel->data_textures + (size_t)loadmodel->data_surfaces[i].texture;

	// free data
	Mem_Free(vertices);
	Mem_Free(texturenames);
	Mem_Free(v);
	Mem_Free(vt);
	Mem_Free(vn);
	Mem_Free(vertexhashtable);
	Mem_Free(vertexhashdata);

	// make a single combined shadow mesh to allow optimized shadow volume creation
	Mod_Q1BSP_CreateShadowMesh(loadmodel);

	// compute all the mesh information that was not loaded from the file
	if (loadmodel->surfmesh.data_element3s)
		for (i = 0;i < loadmodel->surfmesh.num_triangles*3;i++)
			loadmodel->surfmesh.data_element3s[i] = loadmodel->surfmesh.data_element3i[i];
	Mod_ValidateElements(loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.num_triangles, 0, loadmodel->surfmesh.num_vertices, __FILE__, __LINE__);
	// generate normals if the file did not have them
	if (!VectorLength2(loadmodel->surfmesh.data_normal3f))
		Mod_BuildNormals(0, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.data_normal3f, r_smoothnormals_areaweighting.integer != 0);
	Mod_BuildTextureVectorsFromNormals(0, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.data_texcoordtexture2f, loadmodel->surfmesh.data_normal3f, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.data_svector3f, loadmodel->surfmesh.data_tvector3f, r_smoothnormals_areaweighting.integer != 0);
	if (loadmodel->surfmesh.data_neighbor3i)
		Mod_BuildTriangleNeighbors(loadmodel->surfmesh.data_neighbor3i, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.num_triangles);

	// if this is a worldmodel and has no BSP tree, create a fake one for the purpose
	loadmodel->brush.num_visleafs = 1;
	loadmodel->brush.num_leafs = 1;
	loadmodel->brush.num_nodes = 0;
	loadmodel->brush.num_leafsurfaces = loadmodel->num_surfaces;
	loadmodel->brush.data_leafs = (mleaf_t *)Mem_Alloc(loadmodel->mempool, loadmodel->brush.num_leafs * sizeof(mleaf_t));
	loadmodel->brush.data_nodes = (mnode_t *)loadmodel->brush.data_leafs;
	loadmodel->brush.num_pvsclusters = 1;
	loadmodel->brush.num_pvsclusterbytes = 1;
	loadmodel->brush.data_pvsclusters = nobsp_pvs;
	//if (loadmodel->num_nodes) loadmodel->data_nodes = (mnode_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_nodes * sizeof(mnode_t));
	//loadmodel->data_leafsurfaces = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->num_leafsurfaces * sizeof(int));
	loadmodel->brush.data_leafsurfaces = loadmodel->sortedmodelsurfaces;
	VectorCopy(loadmodel->normalmins, loadmodel->brush.data_leafs->mins);
	VectorCopy(loadmodel->normalmaxs, loadmodel->brush.data_leafs->maxs);
	loadmodel->brush.data_leafs->combinedsupercontents = 0; // FIXME?
	loadmodel->brush.data_leafs->clusterindex = 0;
	loadmodel->brush.data_leafs->areaindex = 0;
	loadmodel->brush.data_leafs->numleafsurfaces = loadmodel->brush.num_leafsurfaces;
	loadmodel->brush.data_leafs->firstleafsurface = loadmodel->brush.data_leafsurfaces;
	loadmodel->brush.data_leafs->numleafbrushes = 0;
	loadmodel->brush.data_leafs->firstleafbrush = NULL;
	loadmodel->brush.supportwateralpha = true;

	if (loadmodel->brush.numsubmodels)
		loadmodel->brush.submodels = (dp_model_t **)Mem_Alloc(loadmodel->mempool, loadmodel->brush.numsubmodels * sizeof(dp_model_t *));

	mod = loadmodel;
	for (i = 0;i < loadmodel->brush.numsubmodels;i++)
	{
		if (i > 0)
		{
			char name[10];
			// duplicate the basic information
			dpsnprintf(name, sizeof(name), "*%i", i);
			mod = Mod_FindName(name, loadmodel->name);
			// copy the base model to this one
			*mod = *loadmodel;
			// rename the clone back to its proper name
			strlcpy(mod->name, name, sizeof(mod->name));
			mod->brush.parentmodel = loadmodel;
			// textures and memory belong to the main model
			mod->texturepool = NULL;
			mod->mempool = NULL;
			mod->brush.GetPVS = NULL;
			mod->brush.FatPVS = NULL;
			mod->brush.BoxTouchingPVS = NULL;
			mod->brush.BoxTouchingLeafPVS = NULL;
			mod->brush.BoxTouchingVisibleLeafs = NULL;
			mod->brush.FindBoxClusters = NULL;
			mod->brush.LightPoint = NULL;
			mod->brush.AmbientSoundLevelsForPoint = NULL;
		}
		mod->brush.submodel = i;
		if (loadmodel->brush.submodels)
			loadmodel->brush.submodels[i] = mod;

		// make the model surface list (used by shadowing/lighting)
		mod->firstmodelsurface = submodelfirstsurface[i];
		mod->nummodelsurfaces = submodelfirstsurface[i+1] - submodelfirstsurface[i];
		mod->firstmodelbrush = 0;
		mod->nummodelbrushes = 0;
		mod->sortedmodelsurfaces = loadmodel->sortedmodelsurfaces + mod->firstmodelsurface;
		Mod_MakeSortedSurfaces(mod);

		VectorClear(mod->normalmins);
		VectorClear(mod->normalmaxs);
		l = false;
		for (j = 0;j < mod->nummodelsurfaces;j++)
		{
			const msurface_t *surface = mod->data_surfaces + j + mod->firstmodelsurface;
			const float *v = mod->surfmesh.data_vertex3f + 3 * surface->num_firstvertex;
			int k;
			if (!surface->num_vertices)
				continue;
			if (!l)
			{
				l = true;
				VectorCopy(v, mod->normalmins);
				VectorCopy(v, mod->normalmaxs);
			}
			for (k = 0;k < surface->num_vertices;k++, v += 3)
			{
				mod->normalmins[0] = min(mod->normalmins[0], v[0]);
				mod->normalmins[1] = min(mod->normalmins[1], v[1]);
				mod->normalmins[2] = min(mod->normalmins[2], v[2]);
				mod->normalmaxs[0] = max(mod->normalmaxs[0], v[0]);
				mod->normalmaxs[1] = max(mod->normalmaxs[1], v[1]);
				mod->normalmaxs[2] = max(mod->normalmaxs[2], v[2]);
			}
		}
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

		// this gets altered below if sky or water is used
		mod->DrawSky = NULL;
		mod->DrawAddWaterPlanes = NULL;

		for (j = 0;j < mod->nummodelsurfaces;j++)
			if (mod->data_surfaces[j + mod->firstmodelsurface].texture->basematerialflags & MATERIALFLAG_SKY)
				break;
		if (j < mod->nummodelsurfaces)
			mod->DrawSky = R_Q1BSP_DrawSky;

		for (j = 0;j < mod->nummodelsurfaces;j++)
			if (mod->data_surfaces[j + mod->firstmodelsurface].texture->basematerialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA))
				break;
		if (j < mod->nummodelsurfaces)
			mod->DrawAddWaterPlanes = R_Q1BSP_DrawAddWaterPlanes;

		Mod_MakeCollisionBIH(mod, true, &mod->collision_bih);
		mod->render_bih = mod->collision_bih;

		// generate VBOs and other shared data before cloning submodels
		if (i == 0)
			Mod_BuildVBOs();
	}
	mod = loadmodel;
	Mem_Free(submodelfirstsurface);

	Con_DPrintf("Stats for obj model \"%s\": %i faces, %i nodes, %i leafs, %i clusters, %i clusterportals, mesh: %i vertices, %i triangles, %i surfaces\n", loadmodel->name, loadmodel->num_surfaces, loadmodel->brush.num_nodes, loadmodel->brush.num_leafs, mod->brush.num_pvsclusters, loadmodel->brush.num_portals, loadmodel->surfmesh.num_vertices, loadmodel->surfmesh.num_triangles, loadmodel->num_surfaces);
}

qboolean Mod_CanSeeBox_Trace(int numsamples, float t, dp_model_t *model, vec3_t eye, vec3_t minsX, vec3_t maxsX)
{
	// we already have done PVS culling at this point...
	// so we don't need to do it again.

	int i;
	vec3_t testorigin, mins, maxs;

	testorigin[0] = (minsX[0] + maxsX[0]) * 0.5;
	testorigin[1] = (minsX[1] + maxsX[1]) * 0.5;
	testorigin[2] = (minsX[2] + maxsX[2]) * 0.5;

	if(model->brush.TraceLineOfSight(model, eye, testorigin))
		return 1;

	// expand the box a little
	mins[0] = (t+1) * minsX[0] - t * maxsX[0];
	maxs[0] = (t+1) * maxsX[0] - t * minsX[0];
	mins[1] = (t+1) * minsX[1] - t * maxsX[1];
	maxs[1] = (t+1) * maxsX[1] - t * minsX[1];
	mins[2] = (t+1) * minsX[2] - t * maxsX[2];
	maxs[2] = (t+1) * maxsX[2] - t * minsX[2];

	for(i = 0; i != numsamples; ++i)
	{
		testorigin[0] = lhrandom(mins[0], maxs[0]);
		testorigin[1] = lhrandom(mins[1], maxs[1]);
		testorigin[2] = lhrandom(mins[2], maxs[2]);

		if(model->brush.TraceLineOfSight(model, eye, testorigin))
			return 1;
	}

	return 0;
}

