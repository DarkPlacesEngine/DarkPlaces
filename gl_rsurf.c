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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_shadow.h"
#include "portals.h"

#define MAX_LIGHTMAP_SIZE 256

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "0"};
cvar_t r_drawcollisionbrushes_polygonfactor = {0, "r_drawcollisionbrushes_polygonfactor", "-1"};
cvar_t r_drawcollisionbrushes_polygonoffset = {0, "r_drawcollisionbrushes_polygonoffset", "0"};
cvar_t r_q3bsp_renderskydepth = {0, "r_q3bsp_renderskydepth", "0"};

// flag arrays used for visibility checking on world model
// (all other entities have no per-surface/per-leaf visibility checks)
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_clusters
qbyte r_pvsbits[(32768+7)>>3];
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_leafs
qbyte r_worldleafvisible[32768];
// TODO: dynamic resize according to r_refdef.worldmodel->num_surfaces
qbyte r_worldsurfacevisible[262144];

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (const entity_render_t *ent, msurface_t *surface)
{
	int smax, tmax, i, j, size, size3, maps, stride, l;
	unsigned int *bl, scale;
	qbyte *lightmap, *out, *stain;
	static unsigned int intblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting
	static qbyte templight[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*4];

	// update cached lighting info
	surface->cached_dlight = 0;

	smax = (surface->lightmapinfo->extents[0]>>4)+1;
	tmax = (surface->lightmapinfo->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;
	lightmap = surface->lightmapinfo->samples;

// set to full bright if no light data
	bl = intblocklights;
	if (!ent->model->brushq1.lightdata)
	{
		for (i = 0;i < size3;i++)
			bl[i] = 255*256;
	}
	else
	{
// clear to no light
		memset(bl, 0, size*3*sizeof(unsigned int));

// add all the lightmaps
		if (lightmap)
		{
			bl = intblocklights;
			for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++, lightmap += size3)
				for (scale = d_lightstylevalue[surface->lightmapinfo->styles[maps]], i = 0;i < size3;i++)
					bl[i] += lightmap[i] * scale;
		}
	}

	stain = surface->lightmapinfo->stainsamples;
	bl = intblocklights;
	out = templight;
	// the >> 16 shift adjusts down 8 bits to account for the stainmap
	// scaling, and remaps the 0-65536 (2x overbright) to 0-256, it will
	// be doubled during rendering to achieve 2x overbright
	// (0 = 0.0, 128 = 1.0, 256 = 2.0)
	if (ent->model->brushq1.lightmaprgba)
	{
		stride = (surface->lightmapinfo->lightmaptexturestride - smax) * 4;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				*out++ = 255;
			}
		}
	}
	else
	{
		stride = (surface->lightmapinfo->lightmaptexturestride - smax) * 3;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
			}
		}
	}

	R_UpdateTexture(surface->lightmaptexture, templight);
}

void R_StainNode (mnode_t *node, model_t *model, const vec3_t origin, float radius, const float fcolor[8])
{
	float ndist, a, ratio, maxdist, maxdist2, maxdist3, invradius, sdtable[256], td, dist2;
	msurface_t *surface, *endsurface;
	int i, s, t, smax, tmax, smax3, impacts, impactt, stained;
	qbyte *bl;
	vec3_t impact;

	maxdist = radius * radius;
	invradius = 1.0f / radius;

loc0:
	if (!node->plane)
		return;
	ndist = PlaneDiff(origin, node->plane);
	if (ndist > radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -radius)
	{
		node = node->children[1];
		goto loc0;
	}

	dist2 = ndist * ndist;
	maxdist3 = maxdist - dist2;

	if (node->plane->type < 3)
	{
		VectorCopy(origin, impact);
		impact[node->plane->type] -= ndist;
	}
	else
	{
		impact[0] = origin[0] - node->plane->normal[0] * ndist;
		impact[1] = origin[1] - node->plane->normal[1] * ndist;
		impact[2] = origin[2] - node->plane->normal[2] * ndist;
	}

	for (surface = model->data_surfaces + node->firstsurface, endsurface = surface + node->numsurfaces;surface < endsurface;surface++)
	{
		if (surface->lightmapinfo->stainsamples)
		{
			smax = (surface->lightmapinfo->extents[0] >> 4) + 1;
			tmax = (surface->lightmapinfo->extents[1] >> 4) + 1;

			impacts = DotProduct (impact, surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3] - surface->lightmapinfo->texturemins[0];
			impactt = DotProduct (impact, surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3] - surface->lightmapinfo->texturemins[1];

			s = bound(0, impacts, smax * 16) - impacts;
			t = bound(0, impactt, tmax * 16) - impactt;
			i = s * s + t * t + dist2;
			if (i > maxdist)
				continue;

			// reduce calculations
			for (s = 0, i = impacts; s < smax; s++, i -= 16)
				sdtable[s] = i * i + dist2;

			bl = surface->lightmapinfo->stainsamples;
			smax3 = smax * 3;
			stained = false;

			i = impactt;
			for (t = 0;t < tmax;t++, i -= 16)
			{
				td = i * i;
				// make sure some part of it is visible on this line
				if (td < maxdist3)
				{
					maxdist2 = maxdist - td;
					for (s = 0;s < smax;s++)
					{
						if (sdtable[s] < maxdist2)
						{
							ratio = lhrandom(0.0f, 1.0f);
							a = (fcolor[3] + ratio * fcolor[7]) * (1.0f - sqrt(sdtable[s] + td) * invradius);
							if (a >= (1.0f / 64.0f))
							{
								if (a > 1)
									a = 1;
								bl[0] = (qbyte) ((float) bl[0] + a * ((fcolor[0] + ratio * fcolor[4]) - (float) bl[0]));
								bl[1] = (qbyte) ((float) bl[1] + a * ((fcolor[1] + ratio * fcolor[5]) - (float) bl[1]));
								bl[2] = (qbyte) ((float) bl[2] + a * ((fcolor[2] + ratio * fcolor[6]) - (float) bl[2]));
								stained = true;
							}
						}
						bl += 3;
					}
				}
				else // skip line
					bl += smax3;
			}
			// force lightmap upload
			if (stained)
				surface->cached_dlight = true;
		}
	}

	if (node->children[0]->plane)
	{
		if (node->children[1]->plane)
		{
			R_StainNode(node->children[0], model, origin, radius, fcolor);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->plane)
	{
		node = node->children[1];
		goto loc0;
	}
}

void R_Stain (const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	int n;
	float fcolor[8];
	entity_render_t *ent;
	model_t *model;
	vec3_t org;
	if (r_refdef.worldmodel == NULL || !r_refdef.worldmodel->brush.data_nodes || !r_refdef.worldmodel->brushq1.lightdata)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(r_refdef.worldmodel->brush.data_nodes + r_refdef.worldmodel->brushq1.hulls[0].firstclipnode, r_refdef.worldmodel, origin, radius, fcolor);

	// look for embedded bmodels
	for (n = 0;n < cl_num_brushmodel_entities;n++)
	{
		ent = &cl_entities[cl_brushmodel_entities[n]].render;
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			Mod_CheckLoaded(model);
			if (model->brush.data_nodes)
			{
				Matrix4x4_Transform(&ent->inversematrix, origin, org);
				R_StainNode(model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode, model, org, radius, fcolor);
			}
		}
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

static void R_DrawPortal_Callback(const void *calldata1, int calldata2)
{
	int i;
	float *v;
	rmeshstate_t m;
	const mportal_t *portal = calldata1;
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&r_identitymatrix);

	memset(&m, 0, sizeof(m));
	m.pointer_vertex = varray_vertex3f;
	R_Mesh_State(&m);

	i = calldata2;
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f),
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f),
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f),
			 0.125f);
	if (PlaneDiff(r_vieworigin, (&portal->plane)) < 0)
	{
		for (i = portal->numpoints - 1, v = varray_vertex3f;i >= 0;i--, v += 3)
			VectorCopy(portal->points[i].position, v);
	}
	else
		for (i = 0, v = varray_vertex3f;i < portal->numpoints;i++, v += 3)
			VectorCopy(portal->points[i].position, v);
	GL_LockArrays(0, portal->numpoints);
	R_Mesh_Draw(0, portal->numpoints, portal->numpoints - 2, polygonelements);
	GL_LockArrays(0, 0);
}

// LordHavoc: this is just a nice debugging tool, very slow
static void R_DrawPortals(void)
{
	int i, leafnum;//, portalnum;
	mportal_t *portal;
	float center[3], f;
	model_t *model = r_refdef.worldmodel;
	if (model == NULL)
		return;
	for (leafnum = 0;leafnum < r_refdef.worldmodel->brush.num_leafs;leafnum++)
	{
		if (r_worldleafvisible[leafnum])
		{
			//for (portalnum = 0, portal = model->brush.data_portals;portalnum < model->brush.num_portals;portalnum++, portal++)
			for (portal = r_refdef.worldmodel->brush.data_leafs[leafnum].portals;portal;portal = portal->next)
			{
				if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
				if (!R_CullBox(portal->mins, portal->maxs))
				{
					VectorClear(center);
					for (i = 0;i < portal->numpoints;i++)
						VectorAdd(center, portal->points[i].position, center);
					f = ixtable[portal->numpoints];
					VectorScale(center, f, center);
					//R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, portal, portalnum);
					R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, portal, leafnum);
				}
			}
		}
	}
}

static void R_DrawCollisionBrush(colbrushf_t *brush)
{
	int i;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = brush->points->v;
	R_Mesh_State(&m);
	i = (int)(((size_t)brush) / sizeof(colbrushf_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, brush->numpoints);
	R_Mesh_Draw(0, brush->numpoints, brush->numtriangles, brush->elements);
	GL_LockArrays(0, 0);
}

static void R_DrawCollisionSurface(entity_render_t *ent, msurface_t *surface)
{
	int i;
	rmeshstate_t m;
	if (!surface->num_collisiontriangles)
		return;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = surface->data_collisionvertex3f;
	R_Mesh_State(&m);
	i = (int)(((size_t)surface) / sizeof(msurface_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, surface->num_collisionvertices);
	R_Mesh_Draw(0, surface->num_collisionvertices, surface->num_collisiontriangles, surface->data_collisionelement3i);
	GL_LockArrays(0, 0);
}

void R_WorldVisibility(void)
{
	int i, j, *mark;
	mleaf_t *leaf;
	mleaf_t *viewleaf;
	model_t *model = r_refdef.worldmodel;

	if (!model)
		return;

	// if possible find the leaf the view origin is in
	viewleaf = model->brush.PointInLeaf ? model->brush.PointInLeaf(model, r_vieworigin) : NULL;
	// if possible fetch the visible cluster bits
	if (model->brush.FatPVS)
		model->brush.FatPVS(model, r_vieworigin, 2, r_pvsbits, sizeof(r_pvsbits));

	// clear the visible surface and leaf flags arrays
	memset(r_worldsurfacevisible, 0, model->num_surfaces);
	memset(r_worldleafvisible, 0, model->brush.num_leafs);

	// if the user prefers surfaceworldnode (testing?) or the viewleaf could
	// not be found, or the viewleaf is not part of the visible world
	// (floating around in the void), use the pvs method
	if (r_surfaceworldnode.integer || !viewleaf || viewleaf->clusterindex < 0)
	{
		// pvs method:
		// similar to quake's RecursiveWorldNode but without cache misses
		for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
		{
			// if leaf is in current pvs and on the screen, mark its surfaces
			if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
			{
				c_leafs++;
				r_worldleafvisible[j] = true;
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_worldsurfacevisible[*mark] = true;
			}
		}
	}
	else
	{
		int leafstackpos;
		mportal_t *p;
		mleaf_t *leafstack[8192];
		// portal method:
		// follows portals leading outward from viewleaf, does not venture
		// offscreen or into leafs that are not visible, faster than Quake's
		// RecursiveWorldNode and vastly better in unvised maps, often culls a
		// lot of surface that pvs alone would miss
		leafstack[0] = viewleaf;
		leafstackpos = 1;
		while (leafstackpos)
		{
			c_leafs++;
			leaf = leafstack[--leafstackpos];
			r_worldleafvisible[leaf - model->brush.data_leafs] = true;
			// mark any surfaces bounding this leaf
			if (leaf->numleafsurfaces)
				for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
					r_worldsurfacevisible[*mark] = true;
			// follow portals into other leafs
			// the checks are:
			// if viewer is behind portal (portal faces outward into the scene)
			// and the portal polygon's bounding box is on the screen
			// and the leaf has not been visited yet
			// and the leaf is visible in the pvs
			// (the first two checks won't cause as many cache misses as the leaf checks)
			for (p = leaf->portals;p;p = p->next)
				if (DotProduct(r_vieworigin, p->plane.normal) < (p->plane.dist + 1) && !R_CullBox(p->mins, p->maxs) && !r_worldleafvisible[p->past - model->brush.data_leafs] && CHECKPVSBIT(r_pvsbits, p->past->clusterindex))
					leafstack[leafstackpos++] = p->past;
		}
	}

	if (r_drawportals.integer)
		R_DrawPortals();
}

void R_Q1BSP_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (r_drawcollisionbrushes.integer < 2)
		R_DrawSurfaces(ent, true);
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	c_bmodels++;
	if (r_drawcollisionbrushes.integer < 2)
		R_DrawSurfaces(ent, false);
	if (r_drawcollisionbrushes.integer >= 1 && ent->model->brush.num_brushes)
	{
		int i;
		model_t *model = ent->model;
		msurface_t *surface;
		q3mbrush_t *brush;
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		qglPolygonOffset(r_drawcollisionbrushes_polygonfactor.value, r_drawcollisionbrushes_polygonoffset.value);
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && brush->colbrushf->numtriangles)
				R_DrawCollisionBrush(brush->colbrushf);
		for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
			if (surface->num_collisiontriangles)
				R_DrawCollisionSurface(ent, surface);
		qglPolygonOffset(0, 0);
	}
}

typedef struct r_q1bsp_getlightinfo_s
{
	model_t *model;
	vec3_t relativelightorigin;
	float lightradius;
	int *outleaflist;
	qbyte *outleafpvs;
	int outnumleafs;
	int *outsurfacelist;
	qbyte *outsurfacepvs;
	int outnumsurfaces;
	vec3_t outmins;
	vec3_t outmaxs;
	vec3_t lightmins;
	vec3_t lightmaxs;
	const qbyte *pvs;
}
r_q1bsp_getlightinfo_t;

void R_Q1BSP_RecursiveGetLightInfo(r_q1bsp_getlightinfo_t *info, mnode_t *node)
{
	int sides;
	mleaf_t *leaf;
	for (;;)
	{
		if (!BoxesOverlap(info->lightmins, info->lightmaxs, node->mins, node->maxs))
			return;
		if (!node->plane)
			break;
		sides = BoxOnPlaneSide(info->lightmins, info->lightmaxs, node->plane) - 1;
		if (sides == 2)
		{
			R_Q1BSP_RecursiveGetLightInfo(info, node->children[0]);
			node = node->children[1];
		}
		else
			node = node->children[sides];
	}
	leaf = (mleaf_t *)node;
	if (info->pvs == NULL || CHECKPVSBIT(info->pvs, leaf->clusterindex))
	{
		info->outmins[0] = min(info->outmins[0], leaf->mins[0]);
		info->outmins[1] = min(info->outmins[1], leaf->mins[1]);
		info->outmins[2] = min(info->outmins[2], leaf->mins[2]);
		info->outmaxs[0] = max(info->outmaxs[0], leaf->maxs[0]);
		info->outmaxs[1] = max(info->outmaxs[1], leaf->maxs[1]);
		info->outmaxs[2] = max(info->outmaxs[2], leaf->maxs[2]);
		if (info->outleafpvs)
		{
			int leafindex = leaf - info->model->brush.data_leafs;
			if (!CHECKPVSBIT(info->outleafpvs, leafindex))
			{
				SETPVSBIT(info->outleafpvs, leafindex);
				info->outleaflist[info->outnumleafs++] = leafindex;
			}
		}
		if (info->outsurfacepvs)
		{
			int leafsurfaceindex;
			for (leafsurfaceindex = 0;leafsurfaceindex < leaf->numleafsurfaces;leafsurfaceindex++)
			{
				int surfaceindex = leaf->firstleafsurface[leafsurfaceindex];
				if (!CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
				{
					msurface_t *surface = info->model->data_surfaces + surfaceindex;
					if (BoxesOverlap(info->lightmins, info->lightmaxs, surface->mins, surface->maxs))
					if ((surface->texture->currentmaterialflags & (MATERIALFLAG_WALL | MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT)) == MATERIALFLAG_WALL)
					{
						int triangleindex, t;
						const int *e;
						const vec_t *v[3];
						for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = info->model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
						{
							v[0] = info->model->brush.shadowmesh->vertex3f + e[0] * 3;
							v[1] = info->model->brush.shadowmesh->vertex3f + e[1] * 3;
							v[2] = info->model->brush.shadowmesh->vertex3f + e[2] * 3;
							if (PointInfrontOfTriangle(info->relativelightorigin, v[0], v[1], v[2]) && info->lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && info->lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && info->lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && info->lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && info->lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && info->lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
							{
								SETPVSBIT(info->outsurfacepvs, surfaceindex);
								info->outsurfacelist[info->outnumsurfaces++] = surfaceindex;
								break;
							}
						}
					}
				}
			}
		}
	}
}

void R_Q1BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outleaflist, qbyte *outleafpvs, int *outnumleafspointer, int *outsurfacelist, qbyte *outsurfacepvs, int *outnumsurfacespointer)
{
	r_q1bsp_getlightinfo_t info;
	VectorCopy(relativelightorigin, info.relativelightorigin);
	info.lightradius = lightradius;
	info.lightmins[0] = info.relativelightorigin[0] - info.lightradius;
	info.lightmins[1] = info.relativelightorigin[1] - info.lightradius;
	info.lightmins[2] = info.relativelightorigin[2] - info.lightradius;
	info.lightmaxs[0] = info.relativelightorigin[0] + info.lightradius;
	info.lightmaxs[1] = info.relativelightorigin[1] + info.lightradius;
	info.lightmaxs[2] = info.relativelightorigin[2] + info.lightradius;
	if (ent->model == NULL)
	{
		VectorCopy(info.lightmins, outmins);
		VectorCopy(info.lightmaxs, outmaxs);
		*outnumleafspointer = 0;
		*outnumsurfacespointer = 0;
		return;
	}
	info.model = ent->model;
	info.outleaflist = outleaflist;
	info.outleafpvs = outleafpvs;
	info.outnumleafs = 0;
	info.outsurfacelist = outsurfacelist;
	info.outsurfacepvs = outsurfacepvs;
	info.outnumsurfaces = 0;
	VectorCopy(info.relativelightorigin, info.outmins);
	VectorCopy(info.relativelightorigin, info.outmaxs);
	memset(outleafpvs, 0, (info.model->brush.num_leafs + 7) >> 3);
	memset(outsurfacepvs, 0, (info.model->nummodelsurfaces + 7) >> 3);
	if (info.model->brush.GetPVS)
		info.pvs = info.model->brush.GetPVS(info.model, info.relativelightorigin);
	else
		info.pvs = NULL;
	R_UpdateAllTextureInfo(ent);
	if (r_shadow_compilingrtlight)
	{
		// use portal recursion for exact light volume culling, and exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, true, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs);
	}
	else if (r_shadow_realtime_dlight_portalculling.integer)
	{
		// use portal recursion for exact light volume culling, but not the expensive exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, r_shadow_realtime_dlight_portalculling.integer >= 2, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs);
	}
	else
	{
		// use BSP recursion as lights are often small
		R_Q1BSP_RecursiveGetLightInfo(&info, info.model->brush.data_nodes);
	}

	// limit combined leaf box to light boundaries
	outmins[0] = max(info.outmins[0] - 1, info.lightmins[0]);
	outmins[1] = max(info.outmins[1] - 1, info.lightmins[1]);
	outmins[2] = max(info.outmins[2] - 1, info.lightmins[2]);
	outmaxs[0] = min(info.outmaxs[0] + 1, info.lightmaxs[0]);
	outmaxs[1] = min(info.outmaxs[1] + 1, info.lightmaxs[1]);
	outmaxs[2] = min(info.outmaxs[2] + 1, info.lightmaxs[2]);

	*outnumleafspointer = info.outnumleafs;
	*outnumsurfacespointer = info.outnumsurfaces;
}

extern float *rsurface_vertex3f;
extern float *rsurface_svector3f;
extern float *rsurface_tvector3f;
extern float *rsurface_normal3f;
extern void RSurf_SetVertexPointer(const entity_render_t *ent, const texture_t *texture, const msurface_t *surface, const vec3_t modelorg);

void R_Q1BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
	model_t *model = ent->model;
	msurface_t *surface;
	int surfacelistindex;
	float projectdistance = lightradius + model->radius + r_shadow_projectdistance.value;
	vec3_t modelorg;
	texture_t *texture;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(ent->model->normalmins, ent->model->normalmaxs, lightmins, lightmaxs))
		return;
	if (r_drawcollisionbrushes.integer >= 2)
		return;
	if (!r_shadow_compilingrtlight)
		R_UpdateAllTextureInfo(ent);
	if (model->brush.shadowmesh)
	{
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		if (r_shadow_compilingrtlight)
		{
			for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
			{
				surface = model->data_surfaces + surfacelist[surfacelistindex];
				texture = surface->texture;
				if ((texture->basematerialflags & (MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_WALL)) != MATERIALFLAG_WALL)
					continue;
				if (texture->textureflags & (Q3TEXTUREFLAG_TWOSIDED | Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
					continue;
				R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, lightmins, lightmaxs, surface->mins, surface->maxs);
			}
		}
		else
		{
			for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
			{
				surface = model->data_surfaces + surfacelist[surfacelistindex];
				texture = surface->texture->currentframe;
				if ((texture->currentmaterialflags & (MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_WALL)) != MATERIALFLAG_WALL)
					continue;
				if (texture->textureflags & (Q3TEXTUREFLAG_TWOSIDED | Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
					continue;
				R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, lightmins, lightmaxs, surface->mins, surface->maxs);
			}
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + projectdistance, numshadowmark, shadowmarklist);
	}
	else
	{
		projectdistance = lightradius + ent->model->radius;
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->data_surfaces + surfacelist[surfacelistindex];
			// FIXME: get current skin
			texture = surface->texture;//R_FetchAliasSkin(ent, surface->groupmesh);
			if (texture->currentmaterialflags & (MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT) || !surface->num_triangles)
				continue;
			RSurf_SetVertexPointer(ent, texture, surface, modelorg);
			// identify lit faces within the bounding box
			R_Shadow_PrepareShadowMark(surface->groupmesh->num_triangles);
			R_Shadow_MarkVolumeFromBox(surface->num_firsttriangle, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_element3i, relativelightorigin, lightmins, lightmaxs, surface->mins, surface->maxs);
			R_Shadow_VolumeFromList(surface->groupmesh->num_vertices, surface->groupmesh->num_triangles, rsurface_vertex3f, surface->groupmesh->data_element3i, surface->groupmesh->data_neighbor3i, relativelightorigin, projectdistance, numshadowmark, shadowmarklist);
		}
	}
}

void R_Q1BSP_DrawLight(entity_render_t *ent, float *lightcolor, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	msurface_t *surface;
	texture_t *texture;
	int surfacelistindex;
	vec3_t modelorg;
	if (r_drawcollisionbrushes.integer >= 2)
		return;
	if (r_shadow_compilingrtlight)
	{
		// if compiling an rtlight, capture the meshes
		int tri;
		int *e;
		float *lightmins, *lightmaxs, *v[3], *vertex3f;
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->data_surfaces + surfacelist[surfacelistindex];
			texture = surface->texture;
			if ((texture->basematerialflags & (MATERIALFLAG_WALL | MATERIALFLAG_TRANSPARENT)) != MATERIALFLAG_WALL || !surface->num_triangles)
				continue;
			e = surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle;
			vertex3f = surface->groupmesh->data_vertex3f;
			lightmins = r_shadow_compilingrtlight->cullmins;
			lightmaxs = r_shadow_compilingrtlight->cullmaxs;
			for (tri = 0;tri < surface->num_triangles;tri++, e += 3)
			{
				v[0] = vertex3f + e[0] * 3;
				v[1] = vertex3f + e[1] * 3;
				v[2] = vertex3f + e[2] * 3;
				if (PointInfrontOfTriangle(r_shadow_compilingrtlight->shadoworigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
					Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_light, surface->texture->skin.base, surface->texture->skin.gloss, surface->texture->skin.nmap, surface->groupmesh->data_vertex3f, surface->groupmesh->data_svector3f, surface->groupmesh->data_tvector3f, surface->groupmesh->data_normal3f, surface->groupmesh->data_texcoordtexture2f, 1, e);
			}
		}
	}
	else
	{
		R_UpdateAllTextureInfo(ent);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			if (ent == r_refdef.worldentity && !r_worldsurfacevisible[surfacelist[surfacelistindex]])
				continue;
			surface = model->data_surfaces + surfacelist[surfacelistindex];
			texture = surface->texture->currentframe;
			// FIXME: transparent surfaces need to be lit later
			if ((texture->currentmaterialflags & (MATERIALFLAG_WALL | MATERIALFLAG_TRANSPARENT)) != MATERIALFLAG_WALL || !surface->num_triangles)
				continue;
			if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
				qglDisable(GL_CULL_FACE);
			RSurf_SetVertexPointer(ent, texture, surface, modelorg);
			if (!rsurface_svector3f)
			{
				rsurface_svector3f = varray_svector3f;
				rsurface_tvector3f = varray_tvector3f;
				rsurface_normal3f = varray_normal3f;
				Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f);
			}
			// FIXME: add colormapping
			R_Shadow_RenderLighting(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle), rsurface_vertex3f, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f, surface->groupmesh->data_texcoordtexture2f, lightcolor, vec3_origin, vec3_origin, texture->skin.base, NULL, NULL, texture->skin.nmap, texture->skin.gloss);
			if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
				qglEnable(GL_CULL_FACE);
		}
	}
}

#if 0
static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
}

static void gl_surf_newmap(void)
{
}
#endif

void GL_Surf_Init(void)
{

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);
	Cvar_RegisterVariable(&r_detailtextures);
	Cvar_RegisterVariable(&r_surfaceworldnode);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonfactor);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonoffset);
	Cvar_RegisterVariable(&r_q3bsp_renderskydepth);

	//R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

