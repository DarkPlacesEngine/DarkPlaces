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

#define MAX_LIGHTMAP_SIZE 256

static unsigned int intblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting
static float floatblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting

static qbyte templight[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*4];

static mempool_t *r_surf_mempool = NULL;
static int r_surf_surfacevisiblelimit = 0;
static qbyte *r_surf_surfacevisible = NULL;

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_floatbuildlightmap = {0, "r_floatbuildlightmap", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "0"};
cvar_t r_drawcollisionbrushes_polygonfactor = {0, "r_drawcollisionbrushes_polygonfactor", "-1"};
cvar_t r_drawcollisionbrushes_polygonoffset = {0, "r_drawcollisionbrushes_polygonoffset", "0"};
cvar_t r_q3bsp_renderskydepth = {0, "r_q3bsp_renderskydepth", "0"};
cvar_t gl_lightmaps = {0, "gl_lightmaps", "0"};

/*
// FIXME: these arrays are huge!
int r_q1bsp_maxmarkleafs;
int r_q1bsp_nummarkleafs;
mleaf_t *r_q1bsp_maxleaflist[65536];
int r_q1bsp_maxmarksurfaces;
int r_q1bsp_nummarksurfaces;
msurface_t *r_q1bsp_maxsurfacelist[65536];

// FIXME: these arrays are huge!
int r_q3bsp_maxmarkleafs;
int r_q3bsp_nummarkleafs;
q3mleaf_t *r_q3bsp_maxleaflist[65536];
int r_q3bsp_maxmarksurfaces;
int r_q3bsp_nummarksurfaces;
q3msurface_t *r_q3bsp_maxsurfacelist[65536];
*/

void R_Surf_ClearSurfaceVisible(int num)
{
	if (r_surf_surfacevisiblelimit < num)
	{
		Mem_Free(r_surf_surfacevisible);
		r_surf_surfacevisiblelimit = num;
		r_surf_surfacevisible = Mem_Alloc(r_surf_mempool, r_surf_surfacevisiblelimit);
	}
	memset(r_surf_surfacevisible, 0, num);
}

static int dlightdivtable[32768];

static int R_IntAddDynamicLights (const matrix4x4_t *matrix, msurface_t *surf)
{
	int sdtable[256], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, red, green, blue, lit, dist2, impacts, impactt, subtract, k;
	unsigned int *bl;
	float dist, impact[3], local[3];
	dlight_t *light;

	lit = false;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	smax3 = smax * 3;

	for (lnum = 0, light = r_dlight;lnum < r_numdlights;lnum++, light++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		Matrix4x4_Transform(matrix, light->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) light->rtlight.lightmap_cullradius2 + LIGHTOFFSET;

		dist2 = dist * dist;
		dist2 += LIGHTOFFSET;
		if (dist2 >= maxdist)
			continue;

		if (surf->plane->type < 3)
		{
			VectorCopy(local, impact);
			impact[surf->plane->type] -= dist;
		}
		else
		{
			impact[0] = local[0] - surf->plane->normal[0] * dist;
			impact[1] = local[1] - surf->plane->normal[1] * dist;
			impact[2] = local[2] - surf->plane->normal[2] * dist;
		}

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		s = bound(0, impacts, smax * 16) - impacts;
		t = bound(0, impactt, tmax * 16) - impactt;
		i = s * s + t * t + dist2;
		if (i > maxdist)
			continue;

		// reduce calculations
		for (s = 0, i = impacts; s < smax; s++, i -= 16)
			sdtable[s] = i * i + dist2;

		maxdist3 = maxdist - dist2;

		// convert to 8.8 blocklights format
		red = light->rtlight.lightmap_light[0] * (1.0f / 128.0f);
		green = light->rtlight.lightmap_light[1] * (1.0f / 128.0f);
		blue = light->rtlight.lightmap_light[2] * (1.0f / 128.0f);
		subtract = (int) (light->rtlight.lightmap_subtract * 4194304.0f);
		bl = intblocklights;

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
						k = dlightdivtable[(sdtable[s] + td) >> 7] - subtract;
						if (k > 0)
						{
							bl[0] += (red   * k);
							bl[1] += (green * k);
							bl[2] += (blue  * k);
							lit = true;
						}
					}
					bl += 3;
				}
			}
			else // skip line
				bl += smax3;
		}
	}
	return lit;
}

static int R_FloatAddDynamicLights (const matrix4x4_t *matrix, msurface_t *surf)
{
	int lnum, s, t, smax, tmax, smax3, lit, impacts, impactt;
	float sdtable[256], *bl, k, dist, dist2, maxdist, maxdist2, maxdist3, td1, td, red, green, blue, impact[3], local[3], subtract;
	dlight_t *light;

	lit = false;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	smax3 = smax * 3;

	for (lnum = 0, light = r_dlight;lnum < r_numdlights;lnum++, light++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		Matrix4x4_Transform(matrix, light->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) light->rtlight.lightmap_cullradius2 + LIGHTOFFSET;

		dist2 = dist * dist;
		dist2 += LIGHTOFFSET;
		if (dist2 >= maxdist)
			continue;

		if (surf->plane->type < 3)
		{
			VectorCopy(local, impact);
			impact[surf->plane->type] -= dist;
		}
		else
		{
			impact[0] = local[0] - surf->plane->normal[0] * dist;
			impact[1] = local[1] - surf->plane->normal[1] * dist;
			impact[2] = local[2] - surf->plane->normal[2] * dist;
		}

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		td = bound(0, impacts, smax * 16) - impacts;
		td1 = bound(0, impactt, tmax * 16) - impactt;
		td = td * td + td1 * td1 + dist2;
		if (td > maxdist)
			continue;

		// reduce calculations
		for (s = 0, td1 = impacts; s < smax; s++, td1 -= 16.0f)
			sdtable[s] = td1 * td1 + dist2;

		maxdist3 = maxdist - dist2;

		// convert to 8.8 blocklights format
		red = light->rtlight.lightmap_light[0];
		green = light->rtlight.lightmap_light[1];
		blue = light->rtlight.lightmap_light[2];
		subtract = light->rtlight.lightmap_subtract * 32768.0f;
		bl = floatblocklights;

		td1 = impactt;
		for (t = 0;t < tmax;t++, td1 -= 16.0f)
		{
			td = td1 * td1;
			// make sure some part of it is visible on this line
			if (td < maxdist3)
			{
				maxdist2 = maxdist - td;
				for (s = 0;s < smax;s++)
				{
					if (sdtable[s] < maxdist2)
					{
						k = (32768.0f / (sdtable[s] + td)) - subtract;
						bl[0] += red   * k;
						bl[1] += green * k;
						bl[2] += blue  * k;
						lit = true;
					}
					bl += 3;
				}
			}
			else // skip line
				bl += smax3;
		}
	}
	return lit;
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void R_BuildLightMap (const entity_render_t *ent, msurface_t *surf)
{
	if (!r_floatbuildlightmap.integer)
	{
		int smax, tmax, i, j, size, size3, maps, stride, l;
		unsigned int *bl, scale;
		qbyte *lightmap, *out, *stain;

		// update cached lighting info
		surf->cached_dlight = 0;

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		size = smax*tmax;
		size3 = size*3;
		lightmap = surf->samples;

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

			if (surf->dlightframe == r_framecount)
			{
				surf->cached_dlight = R_IntAddDynamicLights(&ent->inversematrix, surf);
				if (surf->cached_dlight)
					c_light_polys++;
			}

	// add all the lightmaps
			if (lightmap)
			{
				bl = intblocklights;
				for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++, lightmap += size3)
					for (scale = d_lightstylevalue[surf->styles[maps]], i = 0;i < size3;i++)
						bl[i] += lightmap[i] * scale;
			}
		}

		stain = surf->stainsamples;
		bl = intblocklights;
		out = templight;
		// the >> 16 shift adjusts down 8 bits to account for the stainmap
		// scaling, and remaps the 0-65536 (2x overbright) to 0-256, it will
		// be doubled during rendering to achieve 2x overbright
		// (0 = 0.0, 128 = 1.0, 256 = 2.0)
		if (ent->model->brushq1.lightmaprgba)
		{
			stride = (surf->lightmaptexturestride - smax) * 4;
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
			stride = (surf->lightmaptexturestride - smax) * 3;
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

		R_UpdateTexture(surf->lightmaptexture, templight);
	}
	else
	{
		int smax, tmax, i, j, size, size3, maps, stride, l;
		float *bl, scale;
		qbyte *lightmap, *out, *stain;

		// update cached lighting info
		surf->cached_dlight = 0;

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		size = smax*tmax;
		size3 = size*3;
		lightmap = surf->samples;

	// set to full bright if no light data
		bl = floatblocklights;
		if (!ent->model->brushq1.lightdata)
		{
			for (i = 0;i < size3;i++)
				bl[i] = 255*256;
		}
		else
		{
			memset(bl, 0, size*3*sizeof(float));

			if (surf->dlightframe == r_framecount)
			{
				surf->cached_dlight = R_FloatAddDynamicLights(&ent->inversematrix, surf);
				if (surf->cached_dlight)
					c_light_polys++;
			}

			// add all the lightmaps
			if (lightmap)
			{
				bl = floatblocklights;
				for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++, lightmap += size3)
					for (scale = d_lightstylevalue[surf->styles[maps]], i = 0;i < size3;i++)
						bl[i] += lightmap[i] * scale;
			}
		}

		stain = surf->stainsamples;
		bl = floatblocklights;
		out = templight;
		// this scaling adjusts down 8 bits to account for the stainmap
		// scaling, and remaps the 0.0-2.0 (2x overbright) to 0-256, it will
		// be doubled during rendering to achieve 2x overbright
		// (0 = 0.0, 128 = 1.0, 256 = 2.0)
		scale = 1.0f / (1 << 16);
		if (ent->model->brushq1.lightmaprgba)
		{
			stride = (surf->lightmaptexturestride - smax) * 4;
			for (i = 0;i < tmax;i++, out += stride)
			{
				for (j = 0;j < smax;j++)
				{
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
					*out++ = 255;
				}
			}
		}
		else
		{
			stride = (surf->lightmaptexturestride - smax) * 3;
			for (i = 0;i < tmax;i++, out += stride)
			{
				for (j = 0;j < smax;j++)
				{
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
					l = *bl++ * *stain++ * scale;*out++ = min(l, 255);
				}
			}
		}

		R_UpdateTexture(surf->lightmaptexture, templight);
	}
}

void R_StainNode (mnode_t *node, model_t *model, const vec3_t origin, float radius, const float fcolor[8])
{
	float ndist, a, ratio, maxdist, maxdist2, maxdist3, invradius, sdtable[256], td, dist2;
	msurface_t *surf, *endsurf;
	int i, s, t, smax, tmax, smax3, impacts, impactt, stained;
	qbyte *bl;
	vec3_t impact;

	maxdist = radius * radius;
	invradius = 1.0f / radius;

loc0:
	if (node->contents < 0)
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

	for (surf = model->brushq1.surfaces + node->firstsurface, endsurf = surf + node->numsurfaces;surf < endsurf;surf++)
	{
		if (surf->stainsamples)
		{
			smax = (surf->extents[0] >> 4) + 1;
			tmax = (surf->extents[1] >> 4) + 1;

			impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
			impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

			s = bound(0, impacts, smax * 16) - impacts;
			t = bound(0, impactt, tmax * 16) - impactt;
			i = s * s + t * t + dist2;
			if (i > maxdist)
				continue;

			// reduce calculations
			for (s = 0, i = impacts; s < smax; s++, i -= 16)
				sdtable[s] = i * i + dist2;

			bl = surf->stainsamples;
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
				surf->cached_dlight = true;
		}
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
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
	else if (node->children[1]->contents >= 0)
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
	if (r_refdef.worldmodel == NULL || !r_refdef.worldmodel->brushq1.nodes)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(r_refdef.worldmodel->brushq1.nodes + r_refdef.worldmodel->brushq1.hulls[0].firstclipnode, r_refdef.worldmodel, origin, radius, fcolor);

	// look for embedded bmodels
	for (n = 0;n < cl_num_brushmodel_entities;n++)
	{
		ent = cl_brushmodel_entities[n];
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			Mod_CheckLoaded(model);
			if (model->brushq1.nodes)
			{
				Matrix4x4_Transform(&ent->inversematrix, origin, org);
				R_StainNode(model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode, model, org, radius, fcolor);
			}
		}
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

static void RSurf_AddLightmapToVertexColors_Color4f(const int *lightmapoffsets, float *c, int numverts, const qbyte *samples, int size3, const qbyte *styles)
{
	int i;
	float scale;
	const qbyte *lm;
	if (styles[0] != 255)
	{
		for (i = 0;i < numverts;i++, c += 4)
		{
			lm = samples + lightmapoffsets[i];
			scale = d_lightstylevalue[styles[0]] * (1.0f / 32768.0f);
			VectorMA(c, scale, lm, c);
			if (styles[1] != 255)
			{
				lm += size3;
				scale = d_lightstylevalue[styles[1]] * (1.0f / 32768.0f);
				VectorMA(c, scale, lm, c);
				if (styles[2] != 255)
				{
					lm += size3;
					scale = d_lightstylevalue[styles[2]] * (1.0f / 32768.0f);
					VectorMA(c, scale, lm, c);
					if (styles[3] != 255)
					{
						lm += size3;
						scale = d_lightstylevalue[styles[3]] * (1.0f / 32768.0f);
						VectorMA(c, scale, lm, c);
					}
				}
			}
		}
	}
}

static void RSurf_FogColors_Vertex3f_Color4f(const float *v, float *c, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	if (fogenabled)
	{
		for (i = 0;i < numverts;i++, v += 3, c += 4)
		{
			VectorSubtract(v, modelorg, diff);
			f = colorscale * (1 - exp(fogdensity/DotProduct(diff, diff)));
			VectorScale(c, f, c);
		}
	}
	else if (colorscale != 1)
		for (i = 0;i < numverts;i++, c += 4)
			VectorScale(c, colorscale, c);
}

static void RSurf_FoggedColors_Vertex3f_Color4f(const float *v, float *c, float r, float g, float b, float a, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	r *= colorscale;
	g *= colorscale;
	b *= colorscale;
	if (fogenabled)
	{
		for (i = 0;i < numverts;i++, v += 3, c += 4)
		{
			VectorSubtract(v, modelorg, diff);
			f = 1 - exp(fogdensity/DotProduct(diff, diff));
			c[0] = r * f;
			c[1] = g * f;
			c[2] = b * f;
			c[3] = a;
		}
	}
	else
	{
		for (i = 0;i < numverts;i++, c += 4)
		{
			c[0] = r;
			c[1] = g;
			c[2] = b;
			c[3] = a;
		}
	}
}

static void RSurf_FogPassColors_Vertex3f_Color4f(const float *v, float *c, float r, float g, float b, float a, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	r *= colorscale;
	g *= colorscale;
	b *= colorscale;
	for (i = 0;i < numverts;i++, v += 3, c += 4)
	{
		VectorSubtract(v, modelorg, diff);
		f = exp(fogdensity/DotProduct(diff, diff));
		c[0] = r;
		c[1] = g;
		c[2] = b;
		c[3] = a * f;
	}
}

static int RSurf_LightSeparate_Vertex3f_Color4f(const matrix4x4_t *matrix, const int *dlightbits, int numverts, const float *vert, float *color, float scale)
{
	float f;
	const float *v;
	float *c;
	int i, l, lit = false;
	const dlight_t *light;
	vec3_t lightorigin;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			light = &r_dlight[l];
			Matrix4x4_Transform(matrix, light->origin, lightorigin);
			for (i = 0, v = vert, c = color;i < numverts;i++, v += 3, c += 4)
			{
				f = VectorDistance2(v, lightorigin) + LIGHTOFFSET;
				if (f < light->rtlight.lightmap_cullradius2)
				{
					f = ((1.0f / f) - light->rtlight.lightmap_subtract) * scale;
					VectorMA(c, f, light->rtlight.lightmap_light, c);
					lit = true;
				}
			}
		}
	}
	return lit;
}

static void RSurfShader_Transparent_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->brushq1.surfaces + calldata2;
	rmeshstate_t m;
	float currentalpha;
	float base, colorscale;
	vec3_t modelorg;
	texture_t *texture;
	float args[4] = {0.05f,0,0,0.04f};
	int rendertype, turb, fullbright;

	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	texture = surf->texinfo->texture;
	if (texture->animated)
		texture = texture->anim_frames[ent->frame != 0][(texture->anim_total[ent->frame != 0] >= 2) ? ((int) (r_refdef.time * 5.0f) % texture->anim_total[ent->frame != 0]) : 0];
	currentalpha = ent->alpha;
	if (surf->flags & SURF_WATERALPHA)
		currentalpha *= r_wateralpha.value;

	GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
	if (ent->effects & EF_ADDITIVE)
	{
		rendertype = SURFRENDER_ADD;
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (currentalpha < 1 || texture->skin.fog != NULL)
	{
		rendertype = SURFRENDER_ALPHA;
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		rendertype = SURFRENDER_OPAQUE;
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(!(ent->effects & EF_NODEPTHTEST));
	}

	turb = (surf->flags & SURF_DRAWTURB) && r_waterscroll.value;
	fullbright = !(ent->flags & RENDER_LIGHT) || (surf->flags & SURF_DRAWFULLBRIGHT) || !surf->samples;
	base = fullbright ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	if (surf->flags & SURF_DRAWTURB)
		base *= 0.5f;
	if ((surf->flags & SURF_DRAWTURB) && gl_textureshader && r_watershader.value && !fogenabled && fullbright && ent->colormod[0] == 1 && ent->colormod[1] == 1 && ent->colormod[2] == 1)
	{
		// NVIDIA Geforce3 distortion texture shader on water
		GL_Color(1, 1, 1, currentalpha);
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = surf->mesh.data_vertex3f;
		m.tex[0] = R_GetTexture(mod_shared_distorttexture[(int)(r_refdef.time * 16)&63]);
		m.tex[1] = R_GetTexture(texture->skin.base);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_REPLACE;
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
		m.pointer_texcoord[1] = surf->mesh.data_texcoordtexture2f;
		Matrix4x4_CreateFromQuakeEntity(&m.texmatrix[0], 0, 0, 0, 0, 0, 0, r_watershader.value);
		Matrix4x4_CreateTranslate(&m.texmatrix[1], sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		R_Mesh_State(&m);

		GL_ActiveTexture(0);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(1);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);
		qglEnable(GL_TEXTURE_SHADER_NV);

		GL_LockArrays(0, surf->mesh.num_vertices);
		R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		GL_LockArrays(0, 0);

		qglDisable(GL_TEXTURE_SHADER_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(0);
	}
	else
	{
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = surf->mesh.data_vertex3f;
		m.pointer_color = varray_color4f;
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
		m.tex[0] = R_GetTexture(texture->skin.base);
		if (turb)
		{
			// scrolling in texture matrix
			Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		}
		colorscale = 1;
		if (gl_combine.integer)
		{
			m.texrgbscale[0] = 4;
			colorscale *= 0.25f;
		}
		R_FillColors(varray_color4f, surf->mesh.num_vertices, base * ent->colormod[0], base * ent->colormod[1], base * ent->colormod[2], currentalpha);
		if (!fullbright)
		{
			if (surf->dlightframe == r_framecount)
				RSurf_LightSeparate_Vertex3f_Color4f(&ent->inversematrix, surf->dlightbits, surf->mesh.num_vertices, surf->mesh.data_vertex3f, varray_color4f, 1);
			if (surf->samples)
				RSurf_AddLightmapToVertexColors_Color4f(surf->mesh.data_lightmapoffsets, varray_color4f,surf->mesh.num_vertices, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
		}
		RSurf_FogColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, colorscale, surf->mesh.num_vertices, modelorg);
		R_Mesh_State(&m);
		GL_LockArrays(0, surf->mesh.num_vertices);
		R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		GL_LockArrays(0, 0);
		if (texture->skin.glow)
		{
			memset(&m, 0, sizeof(m));
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			m.pointer_color = varray_color4f;
			m.tex[0] = R_GetTexture(texture->skin.glow);
			m.pointer_vertex = surf->mesh.data_vertex3f;
			if (m.tex[0])
			{
				m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
				if (turb)
				{
					// scrolling in texture matrix
					Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
				}
			}
			R_Mesh_State(&m);
			RSurf_FoggedColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, 1, 1, 1, currentalpha, 1, surf->mesh.num_vertices, modelorg);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
		if (fogenabled && rendertype != SURFRENDER_ADD)
		{
			memset(&m, 0, sizeof(m));
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			m.pointer_color = varray_color4f;
			m.tex[0] = R_GetTexture(texture->skin.fog);
			m.pointer_vertex = surf->mesh.data_vertex3f;
			if (m.tex[0])
			{
				m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
				if (turb)
				{
					// scrolling in texture matrix
					Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
				}
			}
			R_Mesh_State(&m);
			RSurf_FogPassColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], currentalpha, 1, surf->mesh.num_vertices, modelorg);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
}

void R_UpdateTextureInfo(entity_render_t *ent)
{
	int i, texframe, alttextures;
	texture_t *t;

	if (!ent->model)
		return;

	alttextures = ent->frame != 0;
	texframe = (int)(r_refdef.time * 5.0f);
	for (i = 0;i < ent->model->brushq1.numtextures;i++)
	{
		t = ent->model->brushq1.textures + i;
		t->currentalpha = ent->alpha;
		if (t->flags & SURF_WATERALPHA)
			t->currentalpha *= r_wateralpha.value;
		if (ent->effects & EF_ADDITIVE)
			t->rendertype = SURFRENDER_ADD;
		else if (t->currentalpha < 1 || t->skin.fog != NULL)
			t->rendertype = SURFRENDER_ALPHA;
		else
			t->rendertype = SURFRENDER_OPAQUE;
		// we don't need to set currentframe if t->animated is false because
		// it was already set up by the texture loader for non-animating
		if (t->animated)
			t->currentframe = t->anim_frames[alttextures][(t->anim_total[alttextures] >= 2) ? (texframe % t->anim_total[alttextures]) : 0];
	}
}

void R_DrawSurfaceList(entity_render_t *ent, texture_t *texture, int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	vec3_t center, modelorg;
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	rmeshstate_t m;
	if (gl_lightmaps.integer)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_Color(1, 1, 1, 1);
		memset(&m, 0, sizeof(m));
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			m.tex[0] = R_GetTexture(surface->lightmaptexture);
			m.pointer_vertex = surface->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surface->mesh.num_vertices);
			R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	else if (texture->rendertype != SURFRENDER_OPAQUE)
	{
		// transparent vertex shaded from lightmap
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			Matrix4x4_Transform(&ent->matrix, surface->poly_center, center);
			R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : center, RSurfShader_Transparent_Callback, ent, surface - ent->model->brushq1.surfaces);
		}
	}
	else if (texture->flags & SURF_LIGHTMAP)
	{
		qboolean dolightmap = (ent->flags & RENDER_LIGHT);
		qboolean dobase = true;
		qboolean doambient = r_ambient.value > 0;
		qboolean dodetail = r_detailtextures.integer != 0;
		qboolean doglow = texture->skin.glow != NULL;
		qboolean dofog = fogenabled;
		// multitexture cases
		if (r_textureunits.integer >= 2 && gl_combine.integer && dobase && dolightmap)
		{
			dobase = false;
			dolightmap = false;
			GL_BlendFunc(GL_ONE, GL_ZERO);
			GL_DepthMask(true);
			GL_DepthTest(true);
			GL_Color(1, 1, 1, 1);
			GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.base);
			m.texrgbscale[1] = 2;
			if (r_textureunits.integer >= 3 && !doambient && dodetail)
			{
				m.tex[2] = R_GetTexture(texture->skin.detail);
				m.texrgbscale[2] = 2;
				dodetail = false;
				if (r_textureunits.integer >= 3 && texture->skin.glow)
				{
					m.tex[3] = R_GetTexture(texture->skin.glow);
					m.texcombinergb[3] = GL_ADD;
					doglow = false;
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						m.tex[1] = R_GetTexture(surface->lightmaptexture);
						m.pointer_vertex = surface->mesh.data_vertex3f;
						m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
						m.pointer_texcoord[1] = surface->mesh.data_texcoordlightmap2f;
						m.pointer_texcoord[2] = surface->mesh.data_texcoorddetail2f;
						m.pointer_texcoord[3] = surface->mesh.data_texcoordtexture2f;
						R_Mesh_State(&m);
						GL_LockArrays(0, surface->mesh.num_vertices);
						R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
						GL_LockArrays(0, 0);
					}
				}
				else
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						m.tex[1] = R_GetTexture(surface->lightmaptexture);
						m.pointer_vertex = surface->mesh.data_vertex3f;
						m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
						m.pointer_texcoord[1] = surface->mesh.data_texcoordlightmap2f;
						m.pointer_texcoord[2] = surface->mesh.data_texcoorddetail2f;
						R_Mesh_State(&m);
						GL_LockArrays(0, surface->mesh.num_vertices);
						R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
						GL_LockArrays(0, 0);
					}
				}
			}
			else if (r_textureunits.integer >= 3 && !doambient && !dodetail && doglow)
			{
				m.tex[2] = R_GetTexture(texture->skin.glow);
				m.texcombinergb[2] = GL_ADD;
				doglow = false;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					m.tex[1] = R_GetTexture(surface->lightmaptexture);
					m.pointer_vertex = surface->mesh.data_vertex3f;
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
					m.pointer_texcoord[1] = surface->mesh.data_texcoordlightmap2f;
					m.pointer_texcoord[2] = surface->mesh.data_texcoordtexture2f;
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
		}
		// anything not handled above
		if (dobase)
		{
			GL_BlendFunc(GL_ONE, GL_ZERO);
			GL_DepthMask(true);
			GL_DepthTest(true);
			GL_Color(1, 1, 1, 1);
			if (ent->flags & RENDER_LIGHT)
				GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
			else
				GL_Color(ent->colormod[0], ent->colormod[1], ent->colormod[2], 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.base);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		GL_DepthMask(false);
		if (dolightmap)
		{
			GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			GL_DepthMask(false);
			GL_DepthTest(true);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.base);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.tex[0] = R_GetTexture(surface->lightmaptexture);
				m.pointer_vertex = surface->mesh.data_vertex3f;
				m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (doambient)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			GL_DepthTest(true);
			memset(&m, 0, sizeof(m));
			GL_Color(r_ambient.value * (1.0f / 128.0f) * ent->colormod[0], r_ambient.value * (1.0f / 128.0f) * ent->colormod[1], r_ambient.value * (1.0f / 128.0f) * ent->colormod[2], 1);
			m.tex[0] = R_GetTexture(texture->skin.base);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (dodetail)
		{
			GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			GL_DepthMask(false);
			GL_DepthTest(true);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.detail);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (doglow)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			GL_DepthTest(true);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.glow);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (dofog)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
			GL_DepthTest(true);
			memset(&m, 0, sizeof(m));
			m.pointer_color = varray_color4f;
			m.tex[0] = R_GetTexture(texture->skin.glow);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				if (m.tex[0])
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				R_Mesh_State(&m);
				RSurf_FogPassColors_Vertex3f_Color4f(surface->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], 1, 1, surface->mesh.num_vertices, modelorg);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
	}
	else if (texture->flags & SURF_DRAWTURB)
	{
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			RSurfShader_Transparent_Callback(ent, surface - ent->model->brushq1.surfaces);
		}
	}
	else if (texture->flags & SURF_DRAWSKY)
	{
		if (skyrendernow)
		{
			skyrendernow = false;
			if (skyrendermasked)
				R_Sky();
		}
		// LordHavoc: HalfLife maps have freaky skypolys...
		if (!ent->model->brush.ishlbsp)
		{
			R_Mesh_Matrix(&ent->matrix);
			GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], 1);
			if (skyrendermasked)
			{
				// depth-only (masking)
				GL_ColorMask(0,0,0,0);
				// just to make sure that braindead drivers don't draw anything
				// despite that colormask...
				GL_BlendFunc(GL_ZERO, GL_ONE);
			}
			else
			{
				// fog sky
				GL_BlendFunc(GL_ONE, GL_ZERO);
			}
			GL_DepthMask(true);
			GL_DepthTest(true);
			memset(&m, 0, sizeof(m));
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
		}
	}
}

void R_DrawSurfaces(entity_render_t *ent, qboolean skysurfaces)
{
	int i, j, f, *surfacevisframes, flagsmask;
	msurface_t *surface, **surfacechain;
	texture_t *t, *texture;
	model_t *model = ent->model;
	vec3_t modelorg;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	msurface_t *surfacelist[1024];
	if (model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	if (ent != r_refdef.worldentity)
	{
		// because bmodels can be reused, we have to decide which things to render
		// from scratch every time
		int *mark = model->brushq1.surfacevisframes + model->firstmodelsurface;
		surface = model->brushq1.surfaces + model->firstmodelsurface;
		for (i = 0;i < model->nummodelsurfaces;i++, mark++, surface++)
		{
			*mark = r_framecount;
			surface->dlightframe = -1;
		}
	}

	// update light styles
	if (!skysurfaces)
	{
		if (r_dynamic.integer && !r_rtdlight)
			R_MarkLights(ent);
		for (i = 0;i < model->brushq1.light_styles;i++)
		{
			if (model->brushq1.light_stylevalue[i] != d_lightstylevalue[model->brushq1.light_style[i]])
			{
				model->brushq1.light_stylevalue[i] = d_lightstylevalue[model->brushq1.light_style[i]];
				if ((surfacechain = model->brushq1.light_styleupdatechains[i]))
					for (;(surface = *surfacechain);surfacechain++)
						surface->cached_dlight = true;
			}
		}
	}

	R_UpdateTextureInfo(ent);
	surfacevisframes = model->brushq1.surfacevisframes;
	flagsmask = skysurfaces ? SURF_DRAWSKY : (SURF_DRAWTURB | SURF_LIGHTMAP);
	f = 0;
	t = NULL;
	numsurfacelist = 0;
	for (i = 0, j = model->firstmodelsurface;i < model->nummodelsurfaces;i++, j++)
	{
		if (surfacevisframes[j] == r_framecount)
		{
			surface = model->brushq1.surfaces + j;
			if (t != surface->texinfo->texture)
			{
				if (numsurfacelist)
				{
					R_DrawSurfaceList(ent, texture, numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
				t = surface->texinfo->texture;
				f = t->flags & flagsmask;
				texture = t->currentframe;
			}
			if (f)
			{
				// mark any backface surfaces as not visible
				if (PlaneDist(modelorg, surface->plane) < surface->plane->dist)
				{
					if (!(surface->flags & SURF_PLANEBACK))
					{
						surfacevisframes[j] = -1;
						continue;
					}
				}
				else
				{
					if ((surface->flags & SURF_PLANEBACK))
					{
						surfacevisframes[j] = -1;
						continue;
					}
				}
				// add face to draw list and update lightmap if necessary
				c_faces++;
				if (surface->cached_dlight && surface->lightmaptexture != NULL)
					R_BuildLightMap(ent, surface);
				surfacelist[numsurfacelist++] = surface;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_DrawSurfaceList(ent, texture, numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
			}
		}
	}
	if (numsurfacelist)
		R_DrawSurfaceList(ent, texture, numsurfacelist, surfacelist);
}

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
	R_Mesh_Draw(portal->numpoints, portal->numpoints - 2, polygonelements);
	GL_LockArrays(0, 0);
}

// LordHavoc: this is just a nice debugging tool, very slow
static void R_DrawPortals(void)
{
	int i, portalnum;
	mportal_t *portal;
	float center[3], f;
	model_t *model = r_refdef.worldmodel;
	if (model == NULL)
		return;
	for (portalnum = 0, portal = model->brushq1.portals;portalnum < model->brushq1.numportals;portalnum++, portal++)
	{
		if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
		if (!R_CullBox(portal->mins, portal->maxs))
		{
			VectorClear(center);
			for (i = 0;i < portal->numpoints;i++)
				VectorAdd(center, portal->points[i].position, center);
			f = ixtable[portal->numpoints];
			VectorScale(center, f, center);
			R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, portal, portalnum);
		}
	}
}

void R_WorldVisibility(void)
{
	int i, j, *mark;
	mleaf_t *leaf;
	mleaf_t *viewleaf;
	model_t *model = r_refdef.worldmodel;
	int *surfacevisframes = model->brushq1.surfacevisframes;
	int leafstackpos;
	mportal_t *p;
	mleaf_t *leafstack[8192];

	if (!model || !model->brushq1.PointInLeaf)
		return;

	viewleaf = model->brushq1.PointInLeaf(model, r_vieworigin);
	if (!viewleaf)
		return;

	if (viewleaf->contents == CONTENTS_SOLID || r_surfaceworldnode.integer)
	{
		// equivilant to quake's RecursiveWorldNode but faster and more effective
		for (j = 0, leaf = model->brushq1.data_leafs;j < model->brushq1.num_leafs;j++, leaf++)
		{
			if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex) && !R_CullBox (leaf->mins, leaf->maxs))
			{
				c_leafs++;
				leaf->visframe = r_framecount;
				if (leaf->nummarksurfaces)
					for (i = 0, mark = leaf->firstmarksurface;i < leaf->nummarksurfaces;i++, mark++)
						surfacevisframes[*mark] = r_framecount;
			}
		}
	}
	else
	{
		// LordHavoc: portal-passage worldnode with PVS;
		// follows portals leading outward from viewleaf, does not venture
		// offscreen or into leafs that are not visible, faster than Quake's
		// RecursiveWorldNode
		leafstack[0] = viewleaf;
		leafstackpos = 1;
		while (leafstackpos)
		{
			c_leafs++;
			leaf = leafstack[--leafstackpos];
			leaf->visframe = r_framecount;
			// draw any surfaces bounding this leaf
			if (leaf->nummarksurfaces)
				for (i = 0, mark = leaf->firstmarksurface;i < leaf->nummarksurfaces;i++, mark++)
					surfacevisframes[*mark] = r_framecount;
			// follow portals into other leafs
			for (p = leaf->portals;p;p = p->next)
				if (DotProduct(r_vieworigin, p->plane.normal) < (p->plane.dist + 1) && p->past->visframe != r_framecount && !R_CullBox(p->mins, p->maxs) && CHECKPVSBIT(r_pvsbits, p->past->clusterindex))
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
	R_DrawSurfaces(ent, true);
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	c_bmodels++;
	R_DrawSurfaces(ent, false);
}

void R_Q1BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outclusterlist, qbyte *outclusterpvs, int *outnumclusterspointer, int *outsurfacelist, qbyte *outsurfacepvs, int *outnumsurfacespointer)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs;
	int t, leafindex, marksurfaceindex, surfaceindex, triangleindex, outnumclusters = 0, outnumsurfaces = 0;
	const int *e;
	const float *v[3];
	msurface_t *surface;
	mleaf_t *leaf;
	const qbyte *pvs;
	lightmins[0] = relativelightorigin[0] - lightradius;
	lightmins[1] = relativelightorigin[1] - lightradius;
	lightmins[2] = relativelightorigin[2] - lightradius;
	lightmaxs[0] = relativelightorigin[0] + lightradius;
	lightmaxs[1] = relativelightorigin[1] + lightradius;
	lightmaxs[2] = relativelightorigin[2] + lightradius;
	*outnumclusterspointer = 0;
	*outnumsurfacespointer = 0;
	memset(outclusterpvs, 0, model->brush.num_pvsclusterbytes);
	memset(outsurfacepvs, 0, (model->nummodelsurfaces + 7) >> 3);
	if (model == NULL)
	{
		VectorCopy(lightmins, outmins);
		VectorCopy(lightmaxs, outmaxs);
		return;
	}
	VectorCopy(relativelightorigin, outmins);
	VectorCopy(relativelightorigin, outmaxs);
	if (model->brush.GetPVS)
		pvs = model->brush.GetPVS(model, relativelightorigin);
	else
		pvs = NULL;
	// FIXME: use BSP recursion as lights are often small
	for (leafindex = 0, leaf = model->brushq1.data_leafs;leafindex < model->brushq1.num_leafs;leafindex++, leaf++)
	{
		if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && (pvs == NULL || CHECKPVSBIT(pvs, leaf->clusterindex)))
		{
			outmins[0] = min(outmins[0], leaf->mins[0]);
			outmins[1] = min(outmins[1], leaf->mins[1]);
			outmins[2] = min(outmins[2], leaf->mins[2]);
			outmaxs[0] = max(outmaxs[0], leaf->maxs[0]);
			outmaxs[1] = max(outmaxs[1], leaf->maxs[1]);
			outmaxs[2] = max(outmaxs[2], leaf->maxs[2]);
			if (outclusterpvs)
			{
				if (!CHECKPVSBIT(outclusterpvs, leaf->clusterindex))
				{
					SETPVSBIT(outclusterpvs, leaf->clusterindex);
					outclusterlist[outnumclusters++] = leaf->clusterindex;
				}
			}
			if (outsurfacepvs)
			{
				for (marksurfaceindex = 0;marksurfaceindex < leaf->nummarksurfaces;marksurfaceindex++)
				{
					surfaceindex = leaf->firstmarksurface[marksurfaceindex];
					if (!CHECKPVSBIT(outsurfacepvs, surfaceindex))
					{
						surface = model->brushq1.surfaces + surfaceindex;
						if (BoxesOverlap(lightmins, lightmaxs, surface->poly_mins, surface->poly_maxs) && (surface->flags & SURF_LIGHTMAP) && !surface->texinfo->texture->skin.fog)
						{
							for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->mesh.num_triangles;triangleindex++, t++, e += 3)
							{
								v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
								v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
								v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
								if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
								{
									SETPVSBIT(outsurfacepvs, surfaceindex);
									outsurfacelist[outnumsurfaces++] = surfaceindex;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	// limit combined leaf box to light boundaries
	outmins[0] = max(outmins[0], lightmins[0]);
	outmins[1] = max(outmins[1], lightmins[1]);
	outmins[2] = max(outmins[2], lightmins[2]);
	outmaxs[0] = min(outmaxs[0], lightmaxs[0]);
	outmaxs[1] = min(outmaxs[1], lightmaxs[1]);
	outmaxs[2] = min(outmaxs[2], lightmaxs[2]);

	*outnumclusterspointer = outnumclusters;
	*outnumsurfacespointer = outnumsurfaces;
}

void R_Q1BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs;
	msurface_t *surface;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		R_Mesh_Matrix(&ent->matrix);
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brushq1.surfaces + surfacelist[surfacelistindex];
			R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->mesh.num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, lightmins, lightmaxs, surface->poly_mins, surface->poly_maxs);
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + r_shadow_projectdistance.value, numshadowmark, shadowmarklist);
	}
}

void R_Q1BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs, modelorg;
	msurface_t *surface;
	texture_t *t;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		R_Mesh_Matrix(&ent->matrix);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		R_UpdateTextureInfo(ent);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brushq1.surfaces + surfacelist[surfacelistindex];
			if (r_shadow_compilingrtlight)
			{
				// if compiling an rtlight, capture the mesh
				t = surface->texinfo->texture;
				if (t->flags & SURF_LIGHTMAP && t->skin.fog == NULL)
					Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_light, surface->texinfo->texture->skin.base, surface->texinfo->texture->skin.gloss, surface->texinfo->texture->skin.nmap, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, surface->mesh.num_triangles, surface->mesh.data_element3i);
			}
			else if (ent != r_refdef.worldentity || ent->model->brushq1.surfacevisframes[surface - ent->model->brushq1.surfaces] == r_framecount)
			{
				t = surface->texinfo->texture->currentframe;
				// FIXME: transparent surfaces need to be lit later
				if (t->flags & SURF_LIGHTMAP && t->rendertype == SURFRENDER_OPAQUE)
					R_Shadow_RenderLighting(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, t->skin.gloss, lightcubemap, ambientscale, diffusescale, specularscale);
			}
		}
	}
}

void R_DrawCollisionBrush(colbrushf_t *brush)
{
	int i;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = brush->points->v;
	R_Mesh_State(&m);
	i = (int)(((size_t)brush) / sizeof(colbrushf_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, brush->numpoints);
	R_Mesh_Draw(brush->numpoints, brush->numtriangles, brush->elements);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawCollisionFace(entity_render_t *ent, q3msurface_t *face)
{
	int i;
	rmeshstate_t m;
	if (!face->mesh.num_collisiontriangles)
		return;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = face->mesh.data_collisionvertex3f;
	R_Mesh_State(&m);
	i = (int)(((size_t)face) / sizeof(q3msurface_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, face->mesh.num_collisionvertices);
	R_Mesh_Draw(face->mesh.num_collisionvertices, face->mesh.num_collisiontriangles, face->mesh.data_collisionelement3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_TransparentCallback(const void *voident, int facenumber)
{
	const entity_render_t *ent = voident;
	q3msurface_t *face = ent->model->brushq3.data_faces + facenumber;
	rmeshstate_t m;
	R_Mesh_Matrix(&ent->matrix);
	memset(&m, 0, sizeof(m));
	if ((ent->effects & EF_ADDITIVE) || (face->texture->textureflags & Q3TEXTUREFLAG_ADDITIVE))
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
	// LordHavoc: quake3 was not able to do this; lit transparent surfaces
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 2;
		if (r_textureunits.integer >= 2)
		{
			m.tex[1] = R_GetTexture(face->lightmaptexture);
			m.pointer_texcoord[1] = face->mesh.data_texcoordlightmap2f;
			GL_Color(ent->colormod[0], ent->colormod[1], ent->colormod[2], ent->alpha);
		}
		else
		{
			if (ent->colormod[0] == 1 && ent->colormod[1] == 1 && ent->colormod[2] == 1 && ent->alpha == 1)
				m.pointer_color = face->mesh.data_lightmapcolor4f;
			else
			{
				int i;
				for (i = 0;i < face->mesh.num_vertices;i++)
				{
					varray_color4f[i*4+0] = face->mesh.data_lightmapcolor4f[i*4+0] * ent->colormod[0];
					varray_color4f[i*4+1] = face->mesh.data_lightmapcolor4f[i*4+1] * ent->colormod[1];
					varray_color4f[i*4+2] = face->mesh.data_lightmapcolor4f[i*4+2] * ent->colormod[2];
					varray_color4f[i*4+3] = face->mesh.data_lightmapcolor4f[i*4+3] * ent->alpha;
				}
				m.pointer_color = varray_color4f;
			}
		}
	}
	else
	{
		int i;
		for (i = 0;i < face->mesh.num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->mesh.data_lightmapcolor4f[i*4+0] * ent->colormod[0] * 2.0f;
			varray_color4f[i*4+1] = face->mesh.data_lightmapcolor4f[i*4+1] * ent->colormod[1] * 2.0f;
			varray_color4f[i*4+2] = face->mesh.data_lightmapcolor4f[i*4+2] * ent->colormod[2] * 2.0f;
			varray_color4f[i*4+3] = face->mesh.data_lightmapcolor4f[i*4+3] * ent->alpha;
		}
		m.pointer_color = varray_color4f;
	}
	if (face->texture->textureflags & (Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
	{
		int i, j;
		float center[3], center2[3], forward[3], right[3], up[3], v[4][3];
		matrix4x4_t matrix1, imatrix1;
		R_Mesh_Matrix(&r_identitymatrix);
		// a single autosprite surface can contain multiple sprites...
		for (j = 0;j < face->mesh.num_vertices - 3;j += 4)
		{
			VectorClear(center);
			for (i = 0;i < 4;i++)
				VectorAdd(center, face->mesh.data_vertex3f + (j+i) * 3, center);
			VectorScale(center, 0.25f, center);
			Matrix4x4_Transform(&ent->matrix, center, center2);
			// FIXME: calculate vectors from triangle edges instead of using texture vectors as an easy way out?
			Matrix4x4_FromVectors(&matrix1, face->mesh.data_normal3f + j*3, face->mesh.data_svector3f + j*3, face->mesh.data_tvector3f + j*3, center);
			Matrix4x4_Invert_Simple(&imatrix1, &matrix1);
			for (i = 0;i < 4;i++)
				Matrix4x4_Transform(&imatrix1, face->mesh.data_vertex3f + (j+i)*3, v[i]);
			if (face->texture->textureflags & Q3TEXTUREFLAG_AUTOSPRITE2)
			{
				forward[0] = r_vieworigin[0] - center2[0];
				forward[1] = r_vieworigin[1] - center2[1];
				forward[2] = 0;
				VectorNormalize(forward);
				right[0] = forward[1];
				right[1] = -forward[0];
				right[2] = 0;
				up[0] = 0;
				up[1] = 0;
				up[2] = 1;
			}
			else
			{
				VectorCopy(r_viewforward, forward);
				VectorCopy(r_viewright, right);
				VectorCopy(r_viewup, up);
			}
			for (i = 0;i < 4;i++)
				VectorMAMAMAM(1, center2, v[i][0], forward, v[i][1], right, v[i][2], up, varray_vertex3f + (i+j) * 3);
		}
		m.pointer_vertex = varray_vertex3f;
	}
	else
		m.pointer_vertex = face->mesh.data_vertex3f;
	R_Mesh_State(&m);
	if (face->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglDisable(GL_CULL_FACE);
	GL_LockArrays(0, face->mesh.num_vertices);
	R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
	GL_LockArrays(0, 0);
	if (face->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglEnable(GL_CULL_FACE);
}

void R_Q3BSP_DrawFaceList(entity_render_t *ent, q3mtexture_t *t, int texturenumfaces, q3msurface_t **texturefacelist)
{
	int i, texturefaceindex;
	rmeshstate_t m;
	if (!texturenumfaces)
		return;
	c_faces += texturenumfaces;
	// gl_lightmaps debugging mode skips normal texturing
	if (gl_lightmaps.integer)
	{
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		qglDisable(GL_CULL_FACE);
		memset(&m, 0, sizeof(m));
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.tex[0] = R_GetTexture(face->lightmaptexture);
			m.pointer_texcoord[0] = face->mesh.data_texcoordlightmap2f;
			if (face->lightmaptexture)
			{
				GL_Color(1, 1, 1, 1);
				m.pointer_color = NULL;
			}
			else
				m.pointer_color = face->mesh.data_lightmapcolor4f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
		qglEnable(GL_CULL_FACE);
		return;
	}
	// transparent surfaces get sorted for later drawing
	if ((t->surfaceparms & Q3SURFACEPARM_TRANS) || ent->alpha < 1 || (ent->effects & EF_ADDITIVE))
	{
		vec3_t facecenter, center;
		// drawing sky transparently would be too difficult
		if (t->surfaceparms & Q3SURFACEPARM_SKY)
			return;
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			facecenter[0] = (face->mins[0] + face->maxs[0]) * 0.5f;
			facecenter[1] = (face->mins[1] + face->maxs[1]) * 0.5f;
			facecenter[2] = (face->mins[2] + face->maxs[2]) * 0.5f;
			Matrix4x4_Transform(&ent->matrix, facecenter, center);
			R_MeshQueue_AddTransparent(center, R_Q3BSP_DrawFace_TransparentCallback, ent, face - ent->model->brushq3.data_faces);
		}
		return;
	}
	// sky surfaces draw sky if needed and render themselves as a depth mask
	if (t->surfaceparms & Q3SURFACEPARM_SKY)
	{
		if (skyrendernow)
		{
			skyrendernow = false;
			if (skyrendermasked)
				R_Sky();
		}
		if (!r_q3bsp_renderskydepth.integer)
			return;

		R_Mesh_Matrix(&ent->matrix);

		GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], 1);
		if (skyrendermasked)
		{
			// depth-only (masking)
			GL_ColorMask(0,0,0,0);
			// just to make sure that braindead drivers don't draw anything
			// despite that colormask...
			GL_BlendFunc(GL_ZERO, GL_ONE);
		}
		else
		{
			// fog sky
			GL_BlendFunc(GL_ONE, GL_ZERO);
		}
		GL_DepthMask(true);
		GL_DepthTest(true);

		memset(&m, 0, sizeof(m));
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
		GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
		return;
	}
	// anything else is a typical wall, lightmap * texture + glow
	qboolean dolightmap = (ent->flags & RENDER_LIGHT);
	qboolean dobase = true;
	qboolean doambient = r_ambient.value > 0;
	qboolean doglow = t->skin.glow != NULL;
	qboolean dofog = fogenabled;
	if (t->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglDisable(GL_CULL_FACE);
	if (!dolightmap && dobase)
	{
		dolightmap = false;
		dobase = false;
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(ent->colormod[0], ent->colormod[1], ent->colormod[2], 1);
		if (t->textureflags & Q3TEXTUREFLAG_TWOSIDED)
			qglDisable(GL_CULL_FACE);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.base);
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (r_lightmapintensity <= 0 && dolightmap && dobase)
	{
		dolightmap = false;
		dobase = false;
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(0, 0, 0, 1);
		memset(&m, 0, sizeof(m));
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (r_textureunits.integer >= 2 && gl_combine.integer && dolightmap && dobase)
	{
		// dualtexture combine
		dolightmap = false;
		dobase = false;
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.base);
		GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
		m.pointer_color = NULL;
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			if (!face->lightmaptexture)
				continue;
			m.tex[1] = R_GetTexture(face->lightmaptexture);
			m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = face->mesh.data_texcoordlightmap2f;
			m.texrgbscale[1] = 2;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
		if (r_lightmapintensity == 1 && ent->colormod[0] == 1 && ent->colormod[1] == 1 && ent->colormod[2] == 1)
		{
			for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
			{
				q3msurface_t *face = texturefacelist[texturefaceindex];
				if (face->lightmaptexture)
					continue;
				m.tex[1] = R_GetTexture(face->lightmaptexture);
				m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
				m.pointer_texcoord[1] = face->mesh.data_texcoordlightmap2f;
				m.texrgbscale[1] = 2;
				m.pointer_color = face->mesh.data_lightmapcolor4f;
				m.pointer_vertex = face->mesh.data_vertex3f;
				R_Mesh_State(&m);
				GL_LockArrays(0, face->mesh.num_vertices);
				R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		else
		{
			for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
			{
				q3msurface_t *face = texturefacelist[texturefaceindex];
				if (face->lightmaptexture)
					continue;
				m.tex[1] = R_GetTexture(face->lightmaptexture);
				m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
				m.pointer_texcoord[1] = face->mesh.data_texcoordlightmap2f;
				m.texrgbscale[1] = 2;
				m.pointer_color = varray_color4f;
				for (i = 0;i < face->mesh.num_vertices;i++)
				{
					varray_color4f[i*4+0] = face->mesh.data_lightmapcolor4f[i*4+0] * ent->colormod[0] * r_lightmapintensity;
					varray_color4f[i*4+1] = face->mesh.data_lightmapcolor4f[i*4+1] * ent->colormod[1] * r_lightmapintensity;
					varray_color4f[i*4+2] = face->mesh.data_lightmapcolor4f[i*4+2] * ent->colormod[2] * r_lightmapintensity;
					varray_color4f[i*4+3] = face->mesh.data_lightmapcolor4f[i*4+3];
				}
				m.pointer_vertex = face->mesh.data_vertex3f;
				R_Mesh_State(&m);
				GL_LockArrays(0, face->mesh.num_vertices);
				R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
	}
	// single texture
	if (dolightmap)
	{
		GL_DepthMask(true);
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		memset(&m, 0, sizeof(m));
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.tex[0] = R_GetTexture(face->lightmaptexture);
			m.pointer_texcoord[0] = face->mesh.data_texcoordlightmap2f;
			if (face->lightmaptexture)
				m.pointer_color = NULL;
			else
				m.pointer_color = face->mesh.data_lightmapcolor4f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (dobase)
	{
		GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		GL_DepthMask(false);
		GL_DepthTest(true);
		GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.base);
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (doambient)
	{
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		GL_Color(r_ambient.value * (1.0f / 128.0f) * ent->colormod[0], r_ambient.value * (1.0f / 128.0f) * ent->colormod[1], r_ambient.value * (1.0f / 128.0f) * ent->colormod[2], 1);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.base);
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (doglow)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		GL_Color(1, 1, 1, 1);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.glow);
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (dofog)
	{
		float modelorg[3];
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
		GL_DepthTest(true);
		GL_Color(1, 1, 1, 1);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(t->skin.fog);
		m.pointer_color = varray_color4f;
		for (texturefaceindex = 0;texturefaceindex < texturenumfaces;texturefaceindex++)
		{
			q3msurface_t *face = texturefacelist[texturefaceindex];
			if (m.tex[0])
				m.pointer_texcoord[0] = face->mesh.data_texcoordtexture2f;
			m.pointer_vertex = face->mesh.data_vertex3f;
			R_Mesh_State(&m);
			RSurf_FogPassColors_Vertex3f_Color4f(face->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], 1, 1, face->mesh.num_vertices, modelorg);
			GL_LockArrays(0, face->mesh.num_vertices);
			R_Mesh_Draw(face->mesh.num_vertices, face->mesh.num_triangles, face->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	if (t->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglEnable(GL_CULL_FACE);
}

void R_Q3BSP_DrawFaces(entity_render_t *ent, int skyfaces)
{
	int i, ti, flagsmask, flags;
	q3msurface_t *face;
	model_t *model;
	q3mtexture_t *t;
	const int maxfaces = 1024;
	int numfaces = 0;
	q3msurface_t *facelist[1024];
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	flagsmask = Q3SURFACEFLAG_NODRAW | Q3SURFACEFLAG_SKY;
	if (skyfaces)
		flags = Q3SURFACEFLAG_SKY;
	else
		flags = 0;
	if (ent == r_refdef.worldentity)
	{
		int j;
		q3mleaf_t *leaf;
		R_Surf_ClearSurfaceVisible(r_refdef.worldmodel->brushq3.num_faces);
		for (j = 0, leaf = r_refdef.worldmodel->brushq3.data_leafs;j < r_refdef.worldmodel->brushq3.num_leafs;j++, leaf++)
		{
			if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
			{
				c_leafs++;
				for (i = 0;i < leaf->numleaffaces;i++)
					r_surf_surfacevisible[leaf->firstleaffacenum[i]] = 1;
			}
		}
		for (ti = 0, t = model->brushq3.data_textures;ti < model->brushq3.num_textures;ti++, t++)
		{
			if ((t->surfaceflags & flagsmask) == flags)
			{
				numfaces = 0;
				for (i = 0;i < t->numfaces;i++)
				{
					if (r_surf_surfacevisible[t->facenumlist[i]])
					{
						face = t->facelist[i];
						//if (!R_CullBox(face->mins, face->maxs))
						if (face->mesh.num_triangles)
						{
							if (numfaces >= maxfaces)
							{
								if (numfaces)
									R_Q3BSP_DrawFaceList(ent, t, numfaces, facelist);
								numfaces = 0;
							}
							facelist[numfaces++] = face;
						}
					}
				}
				if (numfaces)
					R_Q3BSP_DrawFaceList(ent, t, numfaces, facelist);
			}
		}
	}
	else
	{
		t = NULL;
		numfaces = 0;
		for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
		{
			if ((face->texture->surfaceflags & flagsmask) == flags && face->mesh.num_triangles)
			{
				if (t != face->texture || numfaces >= maxfaces)
				{
					if (numfaces)
						R_Q3BSP_DrawFaceList(ent, t, numfaces, facelist);
					numfaces = 0;
					t = face->texture;
				}
				facelist[numfaces++] = face;
			}
		}
		if (numfaces)
			R_Q3BSP_DrawFaceList(ent, t, numfaces, facelist);
	}
}

void R_Q3BSP_DrawSky(entity_render_t *ent)
{
	if (r_drawcollisionbrushes.integer < 2)
		R_Q3BSP_DrawFaces(ent, true);
}

void R_Q3BSP_Draw(entity_render_t *ent)
{
	if (r_drawcollisionbrushes.integer < 2)
		R_Q3BSP_DrawFaces(ent, false);
	if (r_drawcollisionbrushes.integer >= 1)
	{
		int i;
		model_t *model = ent->model;
		q3msurface_t *face;
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		qglPolygonOffset(r_drawcollisionbrushes_polygonfactor.value, r_drawcollisionbrushes_polygonoffset.value);
		for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
			if (model->brushq3.data_thismodel->firstbrush[i].colbrushf && model->brushq3.data_thismodel->firstbrush[i].colbrushf->numtriangles)
				R_DrawCollisionBrush(model->brushq3.data_thismodel->firstbrush[i].colbrushf);
		for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
			if (face->mesh.num_collisiontriangles)
				R_Q3BSP_DrawCollisionFace(ent, face);
		qglPolygonOffset(0, 0);
	}
}

void R_Q3BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outclusterlist, qbyte *outclusterpvs, int *outnumclusterspointer, int *outsurfacelist, qbyte *outsurfacepvs, int *outnumsurfacespointer)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs;
	int t, leafindex, marksurfaceindex, surfaceindex, triangleindex, outnumclusters = 0, outnumsurfaces = 0;
	const int *e;
	const float *v[3];
	q3msurface_t *surface;
	q3mleaf_t *leaf;
	const qbyte *pvs;
	lightmins[0] = relativelightorigin[0] - lightradius;
	lightmins[1] = relativelightorigin[1] - lightradius;
	lightmins[2] = relativelightorigin[2] - lightradius;
	lightmaxs[0] = relativelightorigin[0] + lightradius;
	lightmaxs[1] = relativelightorigin[1] + lightradius;
	lightmaxs[2] = relativelightorigin[2] + lightradius;
	*outnumclusterspointer = 0;
	*outnumsurfacespointer = 0;
	memset(outclusterpvs, 0, model->brush.num_pvsclusterbytes);
	memset(outsurfacepvs, 0, (model->nummodelsurfaces + 7) >> 3);
	if (model == NULL)
	{
		VectorCopy(lightmins, outmins);
		VectorCopy(lightmaxs, outmaxs);
		return;
	}
	VectorCopy(relativelightorigin, outmins);
	VectorCopy(relativelightorigin, outmaxs);
	if (model->brush.GetPVS)
		pvs = model->brush.GetPVS(model, relativelightorigin);
	else
		pvs = NULL;
	// FIXME: use BSP recursion as lights are often small
	for (leafindex = 0, leaf = model->brushq3.data_leafs;leafindex < model->brushq3.num_leafs;leafindex++, leaf++)
	{
		if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && (pvs == NULL || CHECKPVSBIT(pvs, leaf->clusterindex)))
		{
			outmins[0] = min(outmins[0], leaf->mins[0]);
			outmins[1] = min(outmins[1], leaf->mins[1]);
			outmins[2] = min(outmins[2], leaf->mins[2]);
			outmaxs[0] = max(outmaxs[0], leaf->maxs[0]);
			outmaxs[1] = max(outmaxs[1], leaf->maxs[1]);
			outmaxs[2] = max(outmaxs[2], leaf->maxs[2]);
			if (outclusterpvs)
			{
				if (!CHECKPVSBIT(outclusterpvs, leaf->clusterindex))
				{
					SETPVSBIT(outclusterpvs, leaf->clusterindex);
					outclusterlist[outnumclusters++] = leaf->clusterindex;
				}
			}
			if (outsurfacepvs)
			{
				for (marksurfaceindex = 0;marksurfaceindex < leaf->numleaffaces;marksurfaceindex++)
				{
					surface = leaf->firstleafface[marksurfaceindex];
					surfaceindex = surface - model->brushq3.data_faces;
					if (!CHECKPVSBIT(outsurfacepvs, surfaceindex))
					{
						if (BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs) && !(surface->texture->surfaceparms & Q3SURFACEPARM_TRANS) && !(surface->texture->surfaceflags & (Q3SURFACEFLAG_SKY | Q3SURFACEFLAG_NODRAW)) && surface->mesh.num_triangles)
						{
							if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
							{
								for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->mesh.num_triangles;triangleindex++, t++, e += 3)
								{
									v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
									v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
									v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
									if (lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
									{
										SETPVSBIT(outsurfacepvs, surfaceindex);
										outsurfacelist[outnumsurfaces++] = surfaceindex;
										break;
									}
								}
							}
							else
							{
								for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->mesh.num_triangles;triangleindex++, t++, e += 3)
								{
									v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
									v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
									v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
									if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
									{
										SETPVSBIT(outsurfacepvs, surfaceindex);
										outsurfacelist[outnumsurfaces++] = surfaceindex;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// limit combined leaf box to light boundaries
	outmins[0] = max(outmins[0], lightmins[0]);
	outmins[1] = max(outmins[1], lightmins[1]);
	outmins[2] = max(outmins[2], lightmins[2]);
	outmaxs[0] = min(outmaxs[0], lightmaxs[0]);
	outmaxs[1] = min(outmaxs[1], lightmaxs[1]);
	outmaxs[2] = min(outmaxs[2], lightmaxs[2]);

	*outnumclusterspointer = outnumclusters;
	*outnumsurfacespointer = outnumsurfaces;
}

void R_Q3BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs;
	q3msurface_t *surface;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		R_Mesh_Matrix(&ent->matrix);
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brushq3.data_faces + surfacelist[surfacelistindex];
			// FIXME: check some manner of face->rendermode here?
			if (!(surface->texture->surfaceflags & Q3SURFACEFLAG_NODRAW) && !(surface->texture->surfaceparms & (Q3SURFACEPARM_SKY | Q3SURFACEPARM_TRANS)) && !(surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED))
				R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->mesh.num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, lightmins, lightmaxs, surface->mins, surface->maxs);
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + r_shadow_projectdistance.value, numshadowmark, shadowmarklist);
	}
}

void R_Q3BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs, modelorg;
	q3msurface_t *surface;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		R_Mesh_Matrix(&ent->matrix);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brushq3.data_faces + surfacelist[surfacelistindex];
			if (r_shadow_compilingrtlight)
			{
				// if compiling an rtlight, capture the mesh
				Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_light, surface->texture->skin.base, surface->texture->skin.gloss, surface->texture->skin.nmap, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, surface->mesh.num_triangles, surface->mesh.data_element3i);
			}
			else if (!(surface->texture->surfaceflags & Q3SURFACEFLAG_NODRAW) && surface->mesh.num_triangles)
			{
				if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
					qglDisable(GL_CULL_FACE);
				R_Shadow_RenderLighting(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, surface->texture->skin.base, surface->texture->skin.nmap, surface->texture->skin.gloss, lightcubemap, ambientscale, diffusescale, specularscale);
				if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
					qglEnable(GL_CULL_FACE);
			}
		}
	}
}

static void gl_surf_start(void)
{
	r_surf_mempool = Mem_AllocPool("gl_rsurf", 0, NULL);
	r_surf_surfacevisiblelimit = 65536;
	r_surf_surfacevisible = Mem_Alloc(r_surf_mempool, r_surf_surfacevisiblelimit);
}

static void gl_surf_shutdown(void)
{
	r_surf_surfacevisiblelimit = 0;
	r_surf_surfacevisible = NULL;
	Mem_FreePool(&r_surf_mempool);
}

static void gl_surf_newmap(void)
{
}

void GL_Surf_Init(void)
{
	int i;
	dlightdivtable[0] = 4194304;
	for (i = 1;i < 32768;i++)
		dlightdivtable[i] = 4194304 / (i << 7);

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);
	Cvar_RegisterVariable(&r_floatbuildlightmap);
	Cvar_RegisterVariable(&r_detailtextures);
	Cvar_RegisterVariable(&r_surfaceworldnode);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonfactor);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonoffset);
	Cvar_RegisterVariable(&r_q3bsp_renderskydepth);
	Cvar_RegisterVariable(&gl_lightmaps);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

