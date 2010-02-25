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

cvar_t r_ambient = {0, "r_ambient", "0", "brightens map, value is 0-128"};
cvar_t r_lockpvs = {0, "r_lockpvs", "0", "disables pvs switching, allows you to walk around and inspect what is visible from a given location in the map (anything not visible from your current location will not be drawn)"};
cvar_t r_lockvisibility = {0, "r_lockvisibility", "0", "disables visibility updates, allows you to walk around and inspect what is visible from a given viewpoint in the map (anything offscreen at the moment this is enabled will not be drawn)"};
cvar_t r_useportalculling = {0, "r_useportalculling", "1", "improve framerate with r_novis 1 by using portal culling - still not as good as compiled visibility data in the map, but it helps (a value of 2 forces use of this even with vis data, which improves framerates in maps without too much complexity, but hurts in extremely complex maps, which is why 2 is not the default mode)"};
cvar_t r_q3bsp_renderskydepth = {0, "r_q3bsp_renderskydepth", "0", "draws sky depth masking in q3 maps (as in q1 maps), this means for example that sky polygons can hide other things"};

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (const entity_render_t *ent, msurface_t *surface)
{
	int smax, tmax, i, size, size3, maps, l;
	int *bl, scale;
	unsigned char *lightmap, *out, *stain;
	dp_model_t *model = ent->model;
	int *intblocklights;
	unsigned char *templight;

	smax = (surface->lightmapinfo->extents[0]>>4)+1;
	tmax = (surface->lightmapinfo->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;

	r_refdef.stats.lightmapupdatepixels += size;
	r_refdef.stats.lightmapupdates++;

	if (cl.buildlightmapmemorysize < size*sizeof(int[3]))
	{
		cl.buildlightmapmemorysize = size*sizeof(int[3]);
		if (cl.buildlightmapmemory)
			Mem_Free(cl.buildlightmapmemory);
		cl.buildlightmapmemory = (unsigned char *) Mem_Alloc(cls.levelmempool, cl.buildlightmapmemorysize);
	}

	// these both point at the same buffer, templight is only used for final
	// processing and can replace the intblocklights data as it goes
	intblocklights = (int *)cl.buildlightmapmemory;
	templight = (unsigned char *)cl.buildlightmapmemory;

	// update cached lighting info
	model->brushq1.lightmapupdateflags[surface - model->data_surfaces] = false;

	lightmap = surface->lightmapinfo->samples;

// set to full bright if no light data
	bl = intblocklights;
	if (!model->brushq1.lightdata)
	{
		for (i = 0;i < size3;i++)
			bl[i] = 128*256;
	}
	else
	{
// clear to no light
		memset(bl, 0, size3*sizeof(*bl));

// add all the lightmaps
		if (lightmap)
			for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++, lightmap += size3)
				for (scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[maps]], i = 0;i < size3;i++)
					bl[i] += lightmap[i] * scale;
	}

	stain = surface->lightmapinfo->stainsamples;
	bl = intblocklights;
	out = templight;
	// the >> 16 shift adjusts down 8 bits to account for the stainmap
	// scaling, and remaps the 0-65536 (2x overbright) to 0-256, it will
	// be doubled during rendering to achieve 2x overbright
	// (0 = 0.0, 128 = 1.0, 256 = 2.0)
	if (stain)
	{
		for (i = 0;i < size;i++, bl += 3, stain += 3, out += 4)
		{
			l = (bl[0] * stain[0]) >> 16;out[2] = min(l, 255);
			l = (bl[1] * stain[1]) >> 16;out[1] = min(l, 255);
			l = (bl[2] * stain[2]) >> 16;out[0] = min(l, 255);
			out[3] = 255;
		}
	}
	else
	{
		for (i = 0;i < size;i++, bl += 3, out += 4)
		{
			l = bl[0] >> 8;out[2] = min(l, 255);
			l = bl[1] >> 8;out[1] = min(l, 255);
			l = bl[2] >> 8;out[0] = min(l, 255);
			out[3] = 255;
		}
	}

	R_UpdateTexture(surface->lightmaptexture, templight, surface->lightmapinfo->lightmaporigin[0], surface->lightmapinfo->lightmaporigin[1], smax, tmax);

	// update the surface's deluxemap if it has one
	if (surface->deluxemaptexture != r_texture_blanknormalmap)
	{
		vec3_t n;
		unsigned char *normalmap = surface->lightmapinfo->nmapsamples;
		lightmap = surface->lightmapinfo->samples;
		// clear to no normalmap
		bl = intblocklights;
		memset(bl, 0, size3*sizeof(*bl));
		// add all the normalmaps
		if (lightmap && normalmap)
		{
			for (maps = 0;maps < MAXLIGHTMAPS && surface->lightmapinfo->styles[maps] != 255;maps++, lightmap += size3, normalmap += size3)
			{
				for (scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[maps]], i = 0;i < size;i++)
				{
					// add the normalmap with weighting proportional to the style's lightmap intensity
					l = (int)(VectorLength(lightmap + i*3) * scale);
					bl[i*3+0] += ((int)normalmap[i*3+0] - 128) * l;
					bl[i*3+1] += ((int)normalmap[i*3+1] - 128) * l;
					bl[i*3+2] += ((int)normalmap[i*3+2] - 128) * l;
				}
			}
		}
		bl = intblocklights;
		out = templight;
		// we simply renormalize the weighted normals to get a valid deluxemap
		for (i = 0;i < size;i++, bl += 3, out += 4)
		{
			VectorCopy(bl, n);
			VectorNormalize(n);
			l = (int)(n[0] * 128 + 128);out[2] = bound(0, l, 255);
			l = (int)(n[1] * 128 + 128);out[1] = bound(0, l, 255);
			l = (int)(n[2] * 128 + 128);out[0] = bound(0, l, 255);
			out[3] = 255;
		}
		R_UpdateTexture(surface->deluxemaptexture, templight, surface->lightmapinfo->lightmaporigin[0], surface->lightmapinfo->lightmaporigin[1], smax, tmax);
	}
}

void R_StainNode (mnode_t *node, dp_model_t *model, const vec3_t origin, float radius, const float fcolor[8])
{
	float ndist, a, ratio, maxdist, maxdist2, maxdist3, invradius, sdtable[256], td, dist2;
	msurface_t *surface, *endsurface;
	int i, s, t, smax, tmax, smax3, impacts, impactt, stained;
	unsigned char *bl;
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

			impacts = (int)(DotProduct (impact, surface->lightmapinfo->texinfo->vecs[0]) + surface->lightmapinfo->texinfo->vecs[0][3] - surface->lightmapinfo->texturemins[0]);
			impactt = (int)(DotProduct (impact, surface->lightmapinfo->texinfo->vecs[1]) + surface->lightmapinfo->texinfo->vecs[1][3] - surface->lightmapinfo->texturemins[1]);

			s = bound(0, impacts, smax * 16) - impacts;
			t = bound(0, impactt, tmax * 16) - impactt;
			i = (int)(s * s + t * t + dist2);
			if ((i > maxdist) || (smax > (int)(sizeof(sdtable)/sizeof(sdtable[0])))) // smax overflow fix from Andreas Dehmel
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
								bl[0] = (unsigned char) ((float) bl[0] + a * ((fcolor[0] + ratio * fcolor[4]) - (float) bl[0]));
								bl[1] = (unsigned char) ((float) bl[1] + a * ((fcolor[1] + ratio * fcolor[5]) - (float) bl[1]));
								bl[2] = (unsigned char) ((float) bl[2] + a * ((fcolor[2] + ratio * fcolor[6]) - (float) bl[2]));
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
				model->brushq1.lightmapupdateflags[surface - model->data_surfaces] = true;
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
	dp_model_t *model;
	vec3_t org;
	if (r_refdef.scene.worldmodel == NULL || !r_refdef.scene.worldmodel->brush.data_nodes || !r_refdef.scene.worldmodel->brushq1.lightdata)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(r_refdef.scene.worldmodel->brush.data_nodes + r_refdef.scene.worldmodel->brushq1.hulls[0].firstclipnode, r_refdef.scene.worldmodel, origin, radius, fcolor);

	// look for embedded bmodels
	for (n = 0;n < cl.num_brushmodel_entities;n++)
	{
		ent = &cl.entities[cl.brushmodel_entities[n]].render;
		model = ent->model;
		if (model && model->name[0] == '*')
		{
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

static void R_DrawPortal_Callback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	// due to the hacky nature of this function's parameters, this is never
	// called with a batch, so numsurfaces is always 1, and the surfacelist
	// contains only a leaf number for coloring purposes
	const mportal_t *portal = (mportal_t *)ent;
	qboolean isvis;
	int i, numpoints;
	float *v;
	float vertex3f[POLYGONELEMENTS_MAXPOINTS*3];
	CHECKGLERROR
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	GL_DepthTest(true);
	GL_CullFace(GL_NONE);
	R_EntityMatrix(&identitymatrix);

	numpoints = min(portal->numpoints, POLYGONELEMENTS_MAXPOINTS);

	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);

	isvis = (portal->here->clusterindex >= 0 && portal->past->clusterindex >= 0 && portal->here->clusterindex != portal->past->clusterindex);

	i = surfacelist[0] >> 1;
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 isvis ? 0.125f : 0.03125f);
	for (i = 0, v = vertex3f;i < numpoints;i++, v += 3)
		VectorCopy(portal->points[i].position, v);
	R_Mesh_Draw(0, numpoints, 0, numpoints - 2, polygonelement3i, polygonelement3s, 0, 0);
}

// LordHavoc: this is just a nice debugging tool, very slow
void R_DrawPortals(void)
{
	int i, leafnum;
	mportal_t *portal;
	float center[3], f;
	dp_model_t *model = r_refdef.scene.worldmodel;
	if (model == NULL)
		return;
	for (leafnum = 0;leafnum < r_refdef.scene.worldmodel->brush.num_leafs;leafnum++)
	{
		if (r_refdef.viewcache.world_leafvisible[leafnum])
		{
			//for (portalnum = 0, portal = model->brush.data_portals;portalnum < model->brush.num_portals;portalnum++, portal++)
			for (portal = r_refdef.scene.worldmodel->brush.data_leafs[leafnum].portals;portal;portal = portal->next)
			{
				if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
				if (!R_CullBox(portal->mins, portal->maxs))
				{
					VectorClear(center);
					for (i = 0;i < portal->numpoints;i++)
						VectorAdd(center, portal->points[i].position, center);
					f = ixtable[portal->numpoints];
					VectorScale(center, f, center);
					R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, (entity_render_t *)portal, leafnum, rsurface.rtlight);
				}
			}
		}
	}
}

void R_View_WorldVisibility(qboolean forcenovis)
{
	int i, j, *mark;
	mleaf_t *leaf;
	mleaf_t *viewleaf;
	dp_model_t *model = r_refdef.scene.worldmodel;

	if (!model)
		return;

	if (r_refdef.view.usecustompvs)
	{
		// clear the visible surface and leaf flags arrays
		memset(r_refdef.viewcache.world_surfacevisible, 0, model->num_surfaces);
		memset(r_refdef.viewcache.world_leafvisible, 0, model->brush.num_leafs);
		r_refdef.viewcache.world_novis = false;

		// simply cull each marked leaf to the frustum (view pyramid)
		for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
		{
			// if leaf is in current pvs and on the screen, mark its surfaces
			if (CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
			{
				r_refdef.stats.world_leafs++;
				r_refdef.viewcache.world_leafvisible[j] = true;
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_refdef.viewcache.world_surfacevisible[*mark] = true;
			}
		}
		return;
	}

	// if possible find the leaf the view origin is in
	viewleaf = model->brush.PointInLeaf ? model->brush.PointInLeaf(model, r_refdef.view.origin) : NULL;
	// if possible fetch the visible cluster bits
	if (!r_lockpvs.integer && model->brush.FatPVS)
		model->brush.FatPVS(model, r_refdef.view.origin, 2, r_refdef.viewcache.world_pvsbits, (r_refdef.viewcache.world_numclusters+7)>>3, false);

	if (!r_lockvisibility.integer)
	{
		// clear the visible surface and leaf flags arrays
		memset(r_refdef.viewcache.world_surfacevisible, 0, model->num_surfaces);
		memset(r_refdef.viewcache.world_leafvisible, 0, model->brush.num_leafs);

		r_refdef.viewcache.world_novis = false;

		// if floating around in the void (no pvs data available, and no
		// portals available), simply use all on-screen leafs.
		if (!viewleaf || viewleaf->clusterindex < 0 || forcenovis)
		{
			// no visibility method: (used when floating around in the void)
			// simply cull each leaf to the frustum (view pyramid)
			// similar to quake's RecursiveWorldNode but without cache misses
			r_refdef.viewcache.world_novis = true;
			for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
			{
				// if leaf is in current pvs and on the screen, mark its surfaces
				if (!R_CullBox(leaf->mins, leaf->maxs))
				{
					r_refdef.stats.world_leafs++;
					r_refdef.viewcache.world_leafvisible[j] = true;
					if (leaf->numleafsurfaces)
						for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
							r_refdef.viewcache.world_surfacevisible[*mark] = true;
				}
			}
		}
		// just check if each leaf in the PVS is on screen
		// (unless portal culling is enabled)
		else if (!model->brush.data_portals || r_useportalculling.integer < 1 || (r_useportalculling.integer < 2 && !r_novis.integer))
		{
			// pvs method:
			// simply check if each leaf is in the Potentially Visible Set,
			// and cull to frustum (view pyramid)
			// similar to quake's RecursiveWorldNode but without cache misses
			for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
			{
				// if leaf is in current pvs and on the screen, mark its surfaces
				if (CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
				{
					r_refdef.stats.world_leafs++;
					r_refdef.viewcache.world_leafvisible[j] = true;
					if (leaf->numleafsurfaces)
						for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
							r_refdef.viewcache.world_surfacevisible[*mark] = true;
				}
			}
		}
		// if desired use a recursive portal flow, culling each portal to
		// frustum and checking if the leaf the portal leads to is in the pvs
		else
		{
			int leafstackpos;
			mportal_t *p;
			mleaf_t *leafstack[8192];
			// simple-frustum portal method:
			// follows portals leading outward from viewleaf, does not venture
			// offscreen or into leafs that are not visible, faster than
			// Quake's RecursiveWorldNode and vastly better in unvised maps,
			// often culls some surfaces that pvs alone would miss
			// (such as a room in pvs that is hidden behind a wall, but the
			//  passage leading to the room is off-screen)
			leafstack[0] = viewleaf;
			leafstackpos = 1;
			while (leafstackpos)
			{
				leaf = leafstack[--leafstackpos];
				if (r_refdef.viewcache.world_leafvisible[leaf - model->brush.data_leafs])
					continue;
				r_refdef.stats.world_leafs++;
				r_refdef.viewcache.world_leafvisible[leaf - model->brush.data_leafs] = true;
				// mark any surfaces bounding this leaf
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_refdef.viewcache.world_surfacevisible[*mark] = true;
				// follow portals into other leafs
				// the checks are:
				// if viewer is behind portal (portal faces outward into the scene)
				// and the portal polygon's bounding box is on the screen
				// and the leaf has not been visited yet
				// and the leaf is visible in the pvs
				// (the first two checks won't cause as many cache misses as the leaf checks)
				for (p = leaf->portals;p;p = p->next)
				{
					r_refdef.stats.world_portals++;
					if (DotProduct(r_refdef.view.origin, p->plane.normal) < (p->plane.dist + 1)
					 && !r_refdef.viewcache.world_leafvisible[p->past - model->brush.data_leafs]
					 && CHECKPVSBIT(r_refdef.viewcache.world_pvsbits, p->past->clusterindex)
					 && !R_CullBox(p->mins, p->maxs)
					 && leafstackpos < (int)(sizeof(leafstack) / sizeof(leafstack[0])))
						leafstack[leafstackpos++] = p->past;
				}
			}
		}
	}
}

void R_Q1BSP_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (ent == r_refdef.scene.worldentity)
		R_DrawWorldSurfaces(true, true, false, false, false);
	else
		R_DrawModelSurfaces(ent, true, true, false, false, false);
}

extern void R_Water_AddWaterPlane(msurface_t *surface);
void R_Q1BSP_DrawAddWaterPlanes(entity_render_t *ent)
{
	int i, j, flagsmask;
	dp_model_t *model = ent->model;
	msurface_t *surfaces;
	if (model == NULL)
		return;

	if (ent == r_refdef.scene.worldentity)
		RSurf_ActiveWorldEntity();
	else
		RSurf_ActiveModelEntity(ent, false, false, false);

	surfaces = model->data_surfaces;
	flagsmask = MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION;

	// add visible surfaces to draw list
	if (ent == r_refdef.scene.worldentity)
	{
		for (i = 0;i < model->nummodelsurfaces;i++)
		{
			j = model->sortedmodelsurfaces[i];
			if (r_refdef.viewcache.world_surfacevisible[j])
				if (surfaces[j].texture->basematerialflags & flagsmask)
					R_Water_AddWaterPlane(surfaces + j);
		}
	}
	else
	{
		for (i = 0;i < model->nummodelsurfaces;i++)
		{
			j = model->sortedmodelsurfaces[i];
			if (surfaces[j].texture->basematerialflags & flagsmask)
				R_Water_AddWaterPlane(surfaces + j);
		}
	}
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
	dp_model_t *model = ent->model;
	if (model == NULL)
		return;
	if (ent == r_refdef.scene.worldentity)
		R_DrawWorldSurfaces(false, true, false, false, false);
	else
		R_DrawModelSurfaces(ent, false, true, false, false, false);
}

void R_Q1BSP_DrawDepth(entity_render_t *ent)
{
	dp_model_t *model = ent->model;
	if (model == NULL)
		return;
	GL_ColorMask(0,0,0,0);
	GL_Color(1,1,1,1);
	GL_DepthTest(true);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_AlphaTest(false);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_DepthOrShadow();
	if (ent == r_refdef.scene.worldentity)
		R_DrawWorldSurfaces(false, false, true, false, false);
	else
		R_DrawModelSurfaces(ent, false, false, true, false, false);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
}

void R_Q1BSP_DrawDebug(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (ent == r_refdef.scene.worldentity)
		R_DrawWorldSurfaces(false, false, false, true, false);
	else
		R_DrawModelSurfaces(ent, false, false, false, true, false);
}

void R_Q1BSP_DrawPrepass(entity_render_t *ent)
{
	dp_model_t *model = ent->model;
	if (model == NULL)
		return;
	if (ent == r_refdef.scene.worldentity)
		R_DrawWorldSurfaces(false, true, false, false, true);
	else
		R_DrawModelSurfaces(ent, false, true, false, false, true);
}

typedef struct r_q1bsp_getlightinfo_s
{
	dp_model_t *model;
	vec3_t relativelightorigin;
	float lightradius;
	int *outleaflist;
	unsigned char *outleafpvs;
	int outnumleafs;
	unsigned char *visitingleafpvs;
	int *outsurfacelist;
	unsigned char *outsurfacepvs;
	unsigned char *tempsurfacepvs;
	unsigned char *outshadowtrispvs;
	unsigned char *outlighttrispvs;
	int outnumsurfaces;
	vec3_t outmins;
	vec3_t outmaxs;
	vec3_t lightmins;
	vec3_t lightmaxs;
	const unsigned char *pvs;
	qboolean svbsp_active;
	qboolean svbsp_insertoccluder;
	int numfrustumplanes;
	const mplane_t *frustumplanes;
}
r_q1bsp_getlightinfo_t;

static void R_Q1BSP_RecursiveGetLightInfo(r_q1bsp_getlightinfo_t *info, mnode_t *node)
{
	int sides;
	mleaf_t *leaf;
	for (;;)
	{
		mplane_t *plane = node->plane;
		//if (!BoxesOverlap(info->lightmins, info->lightmaxs, node->mins, node->maxs))
		//	return;
		if (!plane)
			break;
		//if (!r_shadow_compilingrtlight && R_CullBoxCustomPlanes(node->mins, node->maxs, rtlight->cached_numfrustumplanes, rtlight->cached_frustumplanes))
		//	return;
		if (plane->type < 3)
		{
			if (info->lightmins[plane->type] > plane->dist)
				node = node->children[0];
			else if (info->lightmaxs[plane->type] < plane->dist)
				node = node->children[1];
			else if (info->relativelightorigin[plane->type] >= plane->dist)
			{
				R_Q1BSP_RecursiveGetLightInfo(info, node->children[0]);
				node = node->children[1];
			}
			else
			{
				R_Q1BSP_RecursiveGetLightInfo(info, node->children[1]);
				node = node->children[0];
			}
		}
		else
		{
			sides = BoxOnPlaneSide(info->lightmins, info->lightmaxs, plane);
			if (sides == 3)
			{
				// recurse front side first because the svbsp building prefers it
				if (PlaneDist(info->relativelightorigin, plane) >= 0)
				{
					R_Q1BSP_RecursiveGetLightInfo(info, node->children[0]);
					node = node->children[1];
				}
				else
				{
					R_Q1BSP_RecursiveGetLightInfo(info, node->children[1]);
					node = node->children[0];
				}
			}
			else if (sides == 0)
				return; // ERROR: NAN bounding box!
			else
				node = node->children[sides - 1];
		}
	}
	if (!r_shadow_compilingrtlight && R_CullBoxCustomPlanes(node->mins, node->maxs, info->numfrustumplanes, info->frustumplanes))
		return;
	leaf = (mleaf_t *)node;
	if (r_shadow_frontsidecasting.integer && info->pvs != NULL && !CHECKPVSBIT(info->pvs, leaf->clusterindex))
		return;
	if (info->svbsp_active)
	{
		int i;
		mportal_t *portal;
		static float points[128][3];
		for (portal = leaf->portals;portal;portal = portal->next)
		{
			for (i = 0;i < portal->numpoints;i++)
				VectorCopy(portal->points[i].position, points[i]);
			if (SVBSP_AddPolygon(&r_svbsp, portal->numpoints, points[0], false, NULL, NULL, 0) & 2)
				break;
		}
		if (portal == NULL)
			return; // no portals of this leaf visible
	}
	if (info->svbsp_insertoccluder)
	{
		int leafsurfaceindex;
		int surfaceindex;
		int triangleindex, t;
		int currentmaterialflags;
		qboolean castshadow;
		msurface_t *surface;
		const int *e;
		const vec_t *v[3];
		float v2[3][3];
		for (leafsurfaceindex = 0;leafsurfaceindex < leaf->numleafsurfaces;leafsurfaceindex++)
		{
			surfaceindex = leaf->firstleafsurface[leafsurfaceindex];
			if (!CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
			{
				surface = info->model->data_surfaces + surfaceindex;
				currentmaterialflags = R_GetCurrentTexture(surface->texture)->currentmaterialflags;
				castshadow = !(currentmaterialflags & MATERIALFLAG_NOSHADOW);
				if (castshadow && BoxesOverlap(info->lightmins, info->lightmaxs, surface->mins, surface->maxs))
				{
					qboolean insidebox = BoxInsideBox(surface->mins, surface->maxs, info->lightmins, info->lightmaxs);
					for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = info->model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
					{
						v[0] = info->model->brush.shadowmesh->vertex3f + e[0] * 3;
						v[1] = info->model->brush.shadowmesh->vertex3f + e[1] * 3;
						v[2] = info->model->brush.shadowmesh->vertex3f + e[2] * 3;
						VectorCopy(v[0], v2[0]);
						VectorCopy(v[1], v2[1]);
						VectorCopy(v[2], v2[2]);
						if (insidebox || TriangleOverlapsBox(v2[0], v2[1], v2[2], info->lightmins, info->lightmaxs))
							SVBSP_AddPolygon(&r_svbsp, 3, v2[0], true, NULL, NULL, 0);
					}
				}
			}
		}
		return;
	}
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
		int surfaceindex;
		int triangleindex, t;
		int currentmaterialflags;
		qboolean castshadow;
		msurface_t *surface;
		const int *e;
		const vec_t *v[3];
		float v2[3][3];
		for (leafsurfaceindex = 0;leafsurfaceindex < leaf->numleafsurfaces;leafsurfaceindex++)
		{
			surfaceindex = leaf->firstleafsurface[leafsurfaceindex];
			if (!CHECKPVSBIT(info->outsurfacepvs, surfaceindex))
			{
				surface = info->model->data_surfaces + surfaceindex;
				currentmaterialflags = R_GetCurrentTexture(surface->texture)->currentmaterialflags;
				castshadow = !(currentmaterialflags & MATERIALFLAG_NOSHADOW);
				if (BoxesOverlap(info->lightmins, info->lightmaxs, surface->mins, surface->maxs))
				{
					qboolean addedtris = false;
					qboolean insidebox = BoxInsideBox(surface->mins, surface->maxs, info->lightmins, info->lightmaxs);
					for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = info->model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
					{
						v[0] = info->model->brush.shadowmesh->vertex3f + e[0] * 3;
						v[1] = info->model->brush.shadowmesh->vertex3f + e[1] * 3;
						v[2] = info->model->brush.shadowmesh->vertex3f + e[2] * 3;
						VectorCopy(v[0], v2[0]);
						VectorCopy(v[1], v2[1]);
						VectorCopy(v[2], v2[2]);
						if (insidebox || TriangleOverlapsBox(v2[0], v2[1], v2[2], info->lightmins, info->lightmaxs))
						{
							if (info->svbsp_active)
							{
								if (!(SVBSP_AddPolygon(&r_svbsp, 3, v2[0], false, NULL, NULL, 0) & 2))
									continue;
							}
							if (currentmaterialflags & MATERIALFLAG_NOCULLFACE)
							{
								// if the material is double sided we
								// can't cull by direction
								SETPVSBIT(info->outlighttrispvs, t);
								addedtris = true;
								if (castshadow)
									SETPVSBIT(info->outshadowtrispvs, t);
							}
#if 0
							else if (r_shadow_frontsidecasting.integer)
							{
								// front side casting occludes backfaces,
								// so they are completely useless as both
								// casters and lit polygons
								if (!PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
									continue;
								SETPVSBIT(info->outlighttrispvs, t);
								addedtris = true;
								if (castshadow)
									SETPVSBIT(info->outshadowtrispvs, t);
							}
#endif
							else
							{
								// back side casting does not occlude
								// anything so we can't cull lit polygons
								SETPVSBIT(info->outlighttrispvs, t);
								addedtris = true;
								if (castshadow && !PointInfrontOfTriangle(info->relativelightorigin, v2[0], v2[1], v2[2]))
									SETPVSBIT(info->outshadowtrispvs, t);
							}
						}
					}
					if (addedtris)
					{
						SETPVSBIT(info->outsurfacepvs, surfaceindex);
						info->outsurfacelist[info->outnumsurfaces++] = surfaceindex;
					}
				}
			}
		}
	}
}

static void R_Q1BSP_CallRecursiveGetLightInfo(r_q1bsp_getlightinfo_t *info, qboolean use_svbsp)
{
	if (use_svbsp)
	{
		float origin[3];
		VectorCopy(info->relativelightorigin, origin);
		if (!r_svbsp.nodes)
		{
			r_svbsp.maxnodes = max(r_svbsp.maxnodes, 1<<18);
			r_svbsp.nodes = (svbsp_node_t*) Mem_Alloc(r_main_mempool, r_svbsp.maxnodes * sizeof(svbsp_node_t));
		}
		info->svbsp_active = true;
		info->svbsp_insertoccluder = true;
		for (;;)
		{
			SVBSP_Init(&r_svbsp, origin, r_svbsp.maxnodes, r_svbsp.nodes);
			R_Q1BSP_RecursiveGetLightInfo(info, info->model->brush.data_nodes);
			// if that failed, retry with more nodes
			if (r_svbsp.ranoutofnodes)
			{
				// an upper limit is imposed
				if (r_svbsp.maxnodes >= 2<<22)
					break;
				Mem_Free(r_svbsp.nodes);
				r_svbsp.maxnodes *= 2;
				r_svbsp.nodes = (svbsp_node_t*) Mem_Alloc(tempmempool, r_svbsp.maxnodes * sizeof(svbsp_node_t));
			}
			else
				break;
		}
		// now clear the surfacepvs array because we need to redo it
		//memset(info->outsurfacepvs, 0, (info->model->nummodelsurfaces + 7) >> 3);
		//info->outnumsurfaces = 0;
	}
	else
		info->svbsp_active = false;

	// we HAVE to mark the leaf the light is in as lit, because portals are
	// irrelevant to a leaf that the light source is inside of
	// (and they are all facing away, too)
	{
		mnode_t *node = info->model->brush.data_nodes;
		mleaf_t *leaf;
		while (node->plane)
			node = node->children[(node->plane->type < 3 ? info->relativelightorigin[node->plane->type] : DotProduct(info->relativelightorigin,node->plane->normal)) < node->plane->dist];
		leaf = (mleaf_t *)node;
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
	}

	info->svbsp_insertoccluder = false;
	R_Q1BSP_RecursiveGetLightInfo(info, info->model->brush.data_nodes);
	if (developer_extra.integer && use_svbsp)
	{
		Con_DPrintf("GetLightInfo: svbsp built with %i nodes, polygon stats:\n", r_svbsp.numnodes);
		Con_DPrintf("occluders: %i accepted, %i rejected, %i fragments accepted, %i fragments rejected.\n", r_svbsp.stat_occluders_accepted, r_svbsp.stat_occluders_rejected, r_svbsp.stat_occluders_fragments_accepted, r_svbsp.stat_occluders_fragments_rejected);
		Con_DPrintf("queries  : %i accepted, %i rejected, %i fragments accepted, %i fragments rejected.\n", r_svbsp.stat_queries_accepted, r_svbsp.stat_queries_rejected, r_svbsp.stat_queries_fragments_accepted, r_svbsp.stat_queries_fragments_rejected);
	}
}

void R_Q1BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outleaflist, unsigned char *outleafpvs, int *outnumleafspointer, int *outsurfacelist, unsigned char *outsurfacepvs, int *outnumsurfacespointer, unsigned char *outshadowtrispvs, unsigned char *outlighttrispvs, unsigned char *visitingleafpvs, int numfrustumplanes, const mplane_t *frustumplanes)
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
	info.visitingleafpvs = visitingleafpvs;
	info.outsurfacelist = outsurfacelist;
	info.outsurfacepvs = outsurfacepvs;
	info.outshadowtrispvs = outshadowtrispvs;
	info.outlighttrispvs = outlighttrispvs;
	info.outnumsurfaces = 0;
	info.numfrustumplanes = numfrustumplanes;
	info.frustumplanes = frustumplanes;
	VectorCopy(info.relativelightorigin, info.outmins);
	VectorCopy(info.relativelightorigin, info.outmaxs);
	memset(visitingleafpvs, 0, (info.model->brush.num_leafs + 7) >> 3);
	memset(outleafpvs, 0, (info.model->brush.num_leafs + 7) >> 3);
	memset(outsurfacepvs, 0, (info.model->nummodelsurfaces + 7) >> 3);
	if (info.model->brush.shadowmesh)
		memset(outshadowtrispvs, 0, (info.model->brush.shadowmesh->numtriangles + 7) >> 3);
	else
		memset(outshadowtrispvs, 0, (info.model->surfmesh.num_triangles + 7) >> 3);
	memset(outlighttrispvs, 0, (info.model->surfmesh.num_triangles + 7) >> 3);
	if (info.model->brush.GetPVS && r_shadow_frontsidecasting.integer)
		info.pvs = info.model->brush.GetPVS(info.model, info.relativelightorigin);
	else
		info.pvs = NULL;
	RSurf_ActiveWorldEntity();

	if (r_shadow_frontsidecasting.integer && r_shadow_compilingrtlight && r_shadow_realtime_world_compileportalculling.integer && info.model->brush.data_portals)
	{
		// use portal recursion for exact light volume culling, and exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, true, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs, info.outshadowtrispvs, info.outlighttrispvs, info.visitingleafpvs);
	}
	else if (r_shadow_frontsidecasting.integer && r_shadow_realtime_dlight_portalculling.integer && info.model->brush.data_portals)
	{
		// use portal recursion for exact light volume culling, but not the expensive exact surface checking
		Portal_Visibility(info.model, info.relativelightorigin, info.outleaflist, info.outleafpvs, &info.outnumleafs, info.outsurfacelist, info.outsurfacepvs, &info.outnumsurfaces, NULL, 0, r_shadow_realtime_dlight_portalculling.integer >= 2, info.lightmins, info.lightmaxs, info.outmins, info.outmaxs, info.outshadowtrispvs, info.outlighttrispvs, info.visitingleafpvs);
	}
	else
	{
		// recurse the bsp tree, checking leafs and surfaces for visibility
		// optionally using svbsp for exact culling of compiled lights
		// (or if the user enables dlight svbsp culling, which is mostly for
		//  debugging not actual use)
		R_Q1BSP_CallRecursiveGetLightInfo(&info, (r_shadow_compilingrtlight ? r_shadow_realtime_world_compilesvbsp.integer : r_shadow_realtime_dlight_svbspculling.integer) != 0);
	}

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity

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

void R_Q1BSP_CompileShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist)
{
	dp_model_t *model = ent->model;
	msurface_t *surface;
	int surfacelistindex;
	float projectdistance = relativelightdirection ? lightradius : lightradius + model->radius*2 + r_shadow_projectdistance.value;
	r_shadow_compilingrtlight->static_meshchain_shadow_zfail = Mod_ShadowMesh_Begin(r_main_mempool, 32768, 32768, NULL, NULL, NULL, false, false, true);
	R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		surface = model->data_surfaces + surfacelist[surfacelistindex];
		if (surface->texture->basematerialflags & MATERIALFLAG_NOSHADOW)
			continue;
		R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, relativelightdirection, r_shadow_compilingrtlight->cullmins, r_shadow_compilingrtlight->cullmaxs, surface->mins, surface->maxs);
	}
	R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, relativelightdirection, projectdistance, numshadowmark, shadowmarklist, ent->mins, ent->maxs);
	r_shadow_compilingrtlight->static_meshchain_shadow_zfail = Mod_ShadowMesh_Finish(r_main_mempool, r_shadow_compilingrtlight->static_meshchain_shadow_zfail, false, false, true);
}

extern cvar_t r_polygonoffset_submodel_factor;
extern cvar_t r_polygonoffset_submodel_offset;
void R_Q1BSP_DrawShadowVolume(entity_render_t *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
	dp_model_t *model = ent->model;
	const msurface_t *surface;
	int modelsurfacelistindex;
	float projectdistance = relativelightdirection ? lightradius : lightradius + model->radius*2 + r_shadow_projectdistance.value;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(model->normalmins, model->normalmaxs, lightmins, lightmaxs))
		return;
	if (ent->model->brush.submodel)
		GL_PolygonOffset(r_refdef.shadowpolygonfactor + r_polygonoffset_submodel_factor.value, r_refdef.shadowpolygonoffset + r_polygonoffset_submodel_offset.value);
	if (model->brush.shadowmesh)
	{
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		for (modelsurfacelistindex = 0;modelsurfacelistindex < modelnumsurfaces;modelsurfacelistindex++)
		{
			surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
			if (R_GetCurrentTexture(surface->texture)->currentmaterialflags & MATERIALFLAG_NOSHADOW)
				continue;
			R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, relativelightdirection, lightmins, lightmaxs, surface->mins, surface->maxs);
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, relativelightdirection, projectdistance, numshadowmark, shadowmarklist, ent->mins, ent->maxs);
	}
	else
	{
		projectdistance = lightradius + model->radius*2;
		R_Shadow_PrepareShadowMark(model->surfmesh.num_triangles);
		// identify lit faces within the bounding box
		for (modelsurfacelistindex = 0;modelsurfacelistindex < modelnumsurfaces;modelsurfacelistindex++)
		{
			surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
			rsurface.texture = R_GetCurrentTexture(surface->texture);
			if (rsurface.texture->currentmaterialflags & MATERIALFLAG_NOSHADOW)
				continue;
			RSurf_PrepareVerticesForBatch(false, false, 1, &surface);
			R_Shadow_MarkVolumeFromBox(surface->num_firsttriangle, surface->num_triangles, rsurface.vertex3f, rsurface.modelelement3i, relativelightorigin, relativelightdirection, lightmins, lightmaxs, surface->mins, surface->maxs);
		}
		R_Shadow_VolumeFromList(model->surfmesh.num_vertices, model->surfmesh.num_triangles, rsurface.vertex3f, model->surfmesh.data_element3i, model->surfmesh.data_neighbor3i, relativelightorigin, relativelightdirection, projectdistance, numshadowmark, shadowmarklist, ent->mins, ent->maxs);
	}
	if (ent->model->brush.submodel)
		GL_PolygonOffset(r_refdef.shadowpolygonfactor, r_refdef.shadowpolygonoffset);
}

void R_Q1BSP_CompileShadowMap(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist)
{
	dp_model_t *model = ent->model;
	msurface_t *surface;
	int surfacelistindex;
	int sidetotals[6] = { 0, 0, 0, 0, 0, 0 }, sidemasks = 0;
	int i;
	r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap = Mod_ShadowMesh_Begin(r_main_mempool, 32768, 32768, NULL, NULL, NULL, false, false, true);
	R_Shadow_PrepareShadowSides(model->brush.shadowmesh->numtriangles);
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		surface = model->data_surfaces + surfacelist[surfacelistindex];
		sidemasks |= R_Shadow_ChooseSidesFromBox(surface->num_firstshadowmeshtriangle, surface->num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, &r_shadow_compilingrtlight->matrix_worldtolight, relativelightorigin, relativelightdirection, r_shadow_compilingrtlight->cullmins, r_shadow_compilingrtlight->cullmaxs, surface->mins, surface->maxs, surface->texture->basematerialflags & MATERIALFLAG_NOSHADOW ? NULL : sidetotals);
	}
	R_Shadow_ShadowMapFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, numshadowsides, sidetotals, shadowsides, shadowsideslist);
	r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap = Mod_ShadowMesh_Finish(r_main_mempool, r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap, false, false, true);
	r_shadow_compilingrtlight->static_shadowmap_receivers &= sidemasks;
	for(i = 0;i<6;i++)
		if(!sidetotals[i])
			r_shadow_compilingrtlight->static_shadowmap_casters &= ~(1 << i);
}

#define RSURF_MAX_BATCHSURFACES 8192

static const msurface_t *batchsurfacelist[RSURF_MAX_BATCHSURFACES];

void R_Q1BSP_DrawShadowMap(int side, entity_render_t *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const unsigned char *surfacesides, const vec3_t lightmins, const vec3_t lightmaxs)
{
	dp_model_t *model = ent->model;
	const msurface_t *surface;
	int modelsurfacelistindex, batchnumsurfaces;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(model->normalmins, model->normalmaxs, lightmins, lightmaxs))
		return;
	// identify lit faces within the bounding box
	for (modelsurfacelistindex = 0;modelsurfacelistindex < modelnumsurfaces;modelsurfacelistindex++)
	{
		surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
		if (surfacesides && !(surfacesides[modelsurfacelistindex] && (1 << side)))
			continue;
		rsurface.texture = R_GetCurrentTexture(surface->texture);
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_NOSHADOW)
			continue;
		if (!BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs))
			continue;
		r_refdef.stats.lights_dynamicshadowtriangles += surface->num_triangles;
		r_refdef.stats.lights_shadowtriangles += surface->num_triangles;
		batchsurfacelist[0] = surface;
		batchnumsurfaces = 1;
		while(++modelsurfacelistindex < modelnumsurfaces && batchnumsurfaces < RSURF_MAX_BATCHSURFACES)
		{
			surface = model->data_surfaces + modelsurfacelist[modelsurfacelistindex];
			if (surfacesides && !(surfacesides[modelsurfacelistindex] & (1 << side)))
				continue;
			if (surface->texture != batchsurfacelist[0]->texture)
				break;
			if (!BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs))
				continue;
			r_refdef.stats.lights_dynamicshadowtriangles += surface->num_triangles;
			r_refdef.stats.lights_shadowtriangles += surface->num_triangles;
			batchsurfacelist[batchnumsurfaces++] = surface;
		}
		--modelsurfacelistindex;
		GL_CullFace(rsurface.texture->currentmaterialflags & MATERIALFLAG_NOCULLFACE ? GL_NONE : r_refdef.view.cullface_back);
		RSurf_PrepareVerticesForBatch(false, false, batchnumsurfaces, batchsurfacelist);
		RSurf_DrawBatch_Simple(batchnumsurfaces, batchsurfacelist);
	}
}

#define BATCHSIZE 1024

static void R_Q1BSP_DrawLight_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i, j, endsurface;
	texture_t *t;
	const msurface_t *surface;
	// note: in practice this never actually receives batches), oh well
	R_Shadow_RenderMode_Begin();
	R_Shadow_RenderMode_ActiveLight(rtlight);
	R_Shadow_RenderMode_Lighting(false, true, false);
	R_Shadow_SetupEntityLight(ent);
	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		surface = rsurface.modelsurfaces + surfacelist[i];
		t = surface->texture;
		rsurface.texture = R_GetCurrentTexture(t);
		endsurface = min(j + BATCHSIZE, numsurfaces);
		for (j = i;j < endsurface;j++)
		{
			surface = rsurface.modelsurfaces + surfacelist[j];
			if (t != surface->texture)
				break;
			RSurf_PrepareVerticesForBatch(true, true, 1, &surface);
			R_Shadow_RenderLighting(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, ent->model->surfmesh.data_element3i, ent->model->surfmesh.data_element3s, ent->model->surfmesh.ebo3i, ent->model->surfmesh.ebo3s);
		}
	}
	R_Shadow_RenderMode_End();
}

extern qboolean r_shadow_usingdeferredprepass;
void R_Q1BSP_DrawLight(entity_render_t *ent, int numsurfaces, const int *surfacelist, const unsigned char *lighttrispvs)
{
	dp_model_t *model = ent->model;
	const msurface_t *surface;
	int i, k, kend, l, m, mend, endsurface, batchnumsurfaces, batchnumtriangles, batchfirstvertex, batchlastvertex, batchfirsttriangle;
	const int *element3i;
	static int batchelements[BATCHSIZE*3];
	texture_t *tex;
	CHECKGLERROR
	element3i = rsurface.modelelement3i;
	// this is a double loop because non-visible surface skipping has to be
	// fast, and even if this is not the world model (and hence no visibility
	// checking) the input surface list and batch buffer are different formats
	// so some processing is necessary.  (luckily models have few surfaces)
	for (i = 0;i < numsurfaces;)
	{
		batchnumsurfaces = 0;
		endsurface = min(i + RSURF_MAX_BATCHSURFACES, numsurfaces);
		if (ent == r_refdef.scene.worldentity)
		{
			for (;i < endsurface;i++)
				if (r_refdef.viewcache.world_surfacevisible[surfacelist[i]])
					batchsurfacelist[batchnumsurfaces++] = model->data_surfaces + surfacelist[i];
		}
		else
		{
			for (;i < endsurface;i++)
				batchsurfacelist[batchnumsurfaces++] = model->data_surfaces + surfacelist[i];
		}
		if (!batchnumsurfaces)
			continue;
		for (k = 0;k < batchnumsurfaces;k = kend)
		{
			surface = batchsurfacelist[k];
			tex = surface->texture;
			rsurface.texture = R_GetCurrentTexture(tex);
			// gather surfaces into a batch range
			for (kend = k;kend < batchnumsurfaces && tex == batchsurfacelist[kend]->texture;kend++)
				;
			// now figure out what to do with this particular range of surfaces
			if (!(rsurface.texture->currentmaterialflags & MATERIALFLAG_WALL))
				continue;
			if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION)))
				continue;
			if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			{
				vec3_t tempcenter, center;
				for (l = k;l < kend;l++)
				{
					surface = batchsurfacelist[l];
					tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
					tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
					tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
					Matrix4x4_Transform(&rsurface.matrix, tempcenter, center);
					R_MeshQueue_AddTransparent(rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST ? r_refdef.view.origin : center, R_Q1BSP_DrawLight_TransparentCallback, ent, surface - rsurface.modelsurfaces, rsurface.rtlight);
				}
				continue;
			}
			if (r_shadow_usingdeferredprepass)
				continue;
			batchnumtriangles = 0;
			batchfirsttriangle = surface->num_firsttriangle;
			m = 0; // hush warning
			for (l = k;l < kend;l++)
			{
				surface = batchsurfacelist[l];
				RSurf_PrepareVerticesForBatch(true, true, 1, &surface);
				for (m = surface->num_firsttriangle, mend = m + surface->num_triangles;m < mend;m++)
				{
					if (lighttrispvs && r_test.integer && !CHECKPVSBIT(lighttrispvs, m))
						continue;
					if (batchnumtriangles >= BATCHSIZE)
					{
						r_refdef.stats.lights_lighttriangles += batchnumtriangles;
						Mod_VertexRangeFromElements(batchnumtriangles*3, batchelements, &batchfirstvertex, &batchlastvertex);
						// use the element buffer if all triangles are consecutive
						if (m == batchfirsttriangle + batchnumtriangles)
							R_Shadow_RenderLighting(batchfirstvertex, batchlastvertex + 1 - batchfirstvertex, batchfirsttriangle, batchnumtriangles, ent->model->surfmesh.data_element3i, ent->model->surfmesh.data_element3s, ent->model->surfmesh.ebo3i, ent->model->surfmesh.ebo3s);
						else
							R_Shadow_RenderLighting(batchfirstvertex, batchlastvertex + 1 - batchfirstvertex, 0, batchnumtriangles, batchelements, NULL, 0, 0);
						batchnumtriangles = 0;
						batchfirsttriangle = m;
					}
					batchelements[batchnumtriangles*3+0] = element3i[m*3+0];
					batchelements[batchnumtriangles*3+1] = element3i[m*3+1];
					batchelements[batchnumtriangles*3+2] = element3i[m*3+2];
					batchnumtriangles++;
				}
			}
			if (batchnumtriangles > 0)
			{
				r_refdef.stats.lights_lighttriangles += batchnumtriangles;
				Mod_VertexRangeFromElements(batchnumtriangles*3, batchelements, &batchfirstvertex, &batchlastvertex);
				// use the element buffer if all triangles are consecutive
				if (m == batchfirsttriangle + batchnumtriangles)
					R_Shadow_RenderLighting(batchfirstvertex, batchlastvertex + 1 - batchfirstvertex, batchfirsttriangle, batchnumtriangles, ent->model->surfmesh.data_element3i, ent->model->surfmesh.data_element3s, ent->model->surfmesh.ebo3i, ent->model->surfmesh.ebo3s);
				else
					R_Shadow_RenderLighting(batchfirstvertex, batchlastvertex + 1 - batchfirstvertex, 0, batchnumtriangles, batchelements, NULL, 0, 0);
			}
		}
	}
}

//Made by [515]
void R_ReplaceWorldTexture (void)
{
	dp_model_t		*m;
	texture_t	*t;
	int			i;
	const char	*r, *newt;
	skinframe_t *skinframe;
	if (!r_refdef.scene.worldmodel)
	{
		if (gamemode != GAME_BLOODOMNICIDE)
			Con_Printf("There is no worldmodel\n");
		return;
	}
	m = r_refdef.scene.worldmodel;

	if(Cmd_Argc() < 2)
	{
		Con_Print("r_replacemaptexture <texname> <newtexname> - replaces texture\n");
		Con_Print("r_replacemaptexture <texname> - switch back to default texture\n");
		return;
	}
	if(!cl.islocalgame || !cl.worldmodel)
	{
		if (gamemode != GAME_BLOODOMNICIDE)
			Con_Print("This command works only in singleplayer\n");
		return;
	}
	r = Cmd_Argv(1);
	newt = Cmd_Argv(2);
	if(!newt[0])
		newt = r;
	for(i=0,t=m->data_textures;i<m->num_textures;i++,t++)
	{
		if(/*t->width && !strcasecmp(t->name, r)*/ matchpattern( t->name, r, true ) )
		{
			if ((skinframe = R_SkinFrame_LoadExternal(newt, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PICMIP, true)))
			{
//				t->skinframes[0] = skinframe;
				t->currentskinframe = skinframe;
				t->currentskinframe = skinframe;
				if (gamemode != GAME_BLOODOMNICIDE)
					Con_Printf("%s replaced with %s\n", r, newt);
			}
			else
			{
				if (gamemode != GAME_BLOODOMNICIDE)
					Con_Printf("%s was not found\n", newt);
				return;
			}
		}
	}
}

//Made by [515]
void R_ListWorldTextures (void)
{
	dp_model_t		*m;
	texture_t	*t;
	int			i;
	if (!r_refdef.scene.worldmodel)
	{
		Con_Printf("There is no worldmodel\n");
		return;
	}
	m = r_refdef.scene.worldmodel;

	Con_Print("Worldmodel textures :\n");
	for(i=0,t=m->data_textures;i<m->num_textures;i++,t++)
		if (t->numskinframes)
			Con_Printf("%s\n", t->name);
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
	Cvar_RegisterVariable(&r_lockpvs);
	Cvar_RegisterVariable(&r_lockvisibility);
	Cvar_RegisterVariable(&r_useportalculling);
	Cvar_RegisterVariable(&r_q3bsp_renderskydepth);

	Cmd_AddCommand ("r_replacemaptexture", R_ReplaceWorldTexture, "override a map texture for testing purposes");
	Cmd_AddCommand ("r_listmaptextures", R_ListWorldTextures, "list all textures used by the current map");

	//R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

