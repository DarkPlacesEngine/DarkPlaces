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

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_floatbuildlightmap = {0, "r_floatbuildlightmap", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "1"};
cvar_t r_drawcollisionbrushes_polygonfactor = {0, "r_drawcollisionbrushes_polygonfactor", "-1"};
cvar_t r_drawcollisionbrushes_polygonoffset = {0, "r_drawcollisionbrushes_polygonoffset", "0"};
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
q3mface_t *r_q3bsp_maxsurfacelist[65536];
*/

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
			j = r_ambient.value * 512.0f; // would be 128.0f logically, but using 512.0f to match winquake style
			if (j)
			{
				for (i = 0;i < size3;i++)
					*bl++ = j;
			}
			else
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
			j = 255*256;
		else
			j = r_ambient.value * 512.0f; // would be 128.0f logically, but using 512.0f to match winquake style

		// clear to no light
		if (j)
		{
			for (i = 0;i < size3;i++)
				*bl++ = j;
		}
		else
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
	if (cl.worldmodel == NULL || !cl.worldmodel->brushq1.nodes)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(cl.worldmodel->brushq1.nodes + cl.worldmodel->brushq1.hulls[0].firstclipnode, cl.worldmodel, origin, radius, fcolor);

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

static void RSurfShader_Sky(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;

	// LordHavoc: HalfLife maps have freaky skypolys...
	if (ent->model->brush.ishlbsp)
		return;

	if (skyrendernow)
	{
		skyrendernow = false;
		if (skyrendermasked)
			R_Sky();
	}

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
	R_Mesh_State_Texture(&m);

	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
	GL_ColorMask(1,1,1,1);
}

static void RSurfShader_Water_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->brushq1.surfaces + calldata2;
	rmeshstate_t m;
	float alpha;
	float modelorg[3];
	texture_t *texture;
	matrix4x4_t tempmatrix;
	float	args[4] = {0.05f,0,0,0.04f};

	if (gl_textureshader && r_watershader.value && !fogenabled)
	{
		Matrix4x4_CreateTranslate(&tempmatrix, sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		R_Mesh_TextureMatrix(1, &tempmatrix);
		Matrix4x4_CreateFromQuakeEntity(&tempmatrix, 0, 0, 0, 0, 0, 0, r_watershader.value);
		R_Mesh_TextureMatrix(0, &tempmatrix);
	}
	else if (r_waterscroll.value)
	{
		// scrolling in texture matrix
		Matrix4x4_CreateTranslate(&tempmatrix, sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		R_Mesh_TextureMatrix(0, &tempmatrix);
	}

	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	memset(&m, 0, sizeof(m));
	texture = surf->texinfo->texture->currentframe;
	alpha = texture->currentalpha;
	if (texture->rendertype == SURFRENDER_ADD)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (texture->rendertype == SURFRENDER_ALPHA)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
	}
	if (gl_textureshader && r_watershader.value && !fogenabled)
	{
		m.tex[0] = R_GetTexture(mod_shared_distorttexture[(int)(cl.time * 16)&63]);
		m.tex[1] = R_GetTexture(texture->skin.base);
	}
	else
		m.tex[0] = R_GetTexture(texture->skin.base);
	GL_DepthTest(true);
	if (fogenabled)
		GL_ColorPointer(varray_color4f);
	else
		GL_Color(1, 1, 1, alpha);
	if (gl_textureshader && r_watershader.value && !fogenabled)
	{
		GL_ActiveTexture (0);
		qglTexEnvi (GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture (1);
		qglTexEnvi (GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		qglTexEnvi (GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		qglTexEnvi (GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		qglTexEnvfv (GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);
		qglEnable (GL_TEXTURE_SHADER_NV);
	}

	GL_VertexPointer(surf->mesh.data_vertex3f);
	m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
	m.pointer_texcoord[1] = surf->mesh.data_texcoordtexture2f;
	m.texcombinergb[1] = GL_REPLACE;
	R_Mesh_State_Texture(&m);
	if (fogenabled)
	{
		R_FillColors(varray_color4f, surf->mesh.num_vertices, 1, 1, 1, alpha);
		RSurf_FogColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, 1, surf->mesh.num_vertices, modelorg);
	}
	R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);

	if (gl_textureshader && r_watershader.value && !fogenabled)
	{
		qglDisable (GL_TEXTURE_SHADER_NV);
		GL_ActiveTexture (0);
	}

	if (fogenabled)
	{
		memset(&m, 0, sizeof(m));
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		m.tex[0] = R_GetTexture(texture->skin.fog);
		GL_VertexPointer(surf->mesh.data_vertex3f);
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
		GL_ColorPointer(varray_color4f);
		R_Mesh_State_Texture(&m);
		RSurf_FogPassColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], alpha, 1, surf->mesh.num_vertices, modelorg);
		R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
	}

	if ((gl_textureshader && r_watershader.value && !fogenabled) || r_waterscroll.value)
	{
		Matrix4x4_CreateIdentity(&tempmatrix);
		R_Mesh_TextureMatrix(0, &tempmatrix);
		R_Mesh_TextureMatrix(1, &tempmatrix);
	}
}

static void RSurfShader_Water(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	msurface_t **chain;
	vec3_t center;
	if (texture->rendertype != SURFRENDER_OPAQUE)
	{
		for (chain = surfchain;(surf = *chain) != NULL;chain++)
		{
			if (surf->visframe == r_framecount)
			{
				Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
				R_MeshQueue_AddTransparent(center, RSurfShader_Water_Callback, ent, surf - ent->model->brushq1.surfaces);
			}
		}
	}
	else
		for (chain = surfchain;(surf = *chain) != NULL;chain++)
			if (surf->visframe == r_framecount)
				RSurfShader_Water_Callback(ent, surf - ent->model->brushq1.surfaces);
}

static void RSurfShader_Wall_Pass_BaseVertex(const entity_render_t *ent, const msurface_t *surf, const texture_t *texture, int rendertype, float currentalpha)
{
	float base, colorscale;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	memset(&m, 0, sizeof(m));
	if (rendertype == SURFRENDER_ADD)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (rendertype == SURFRENDER_ALPHA)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
	}
	m.tex[0] = R_GetTexture(texture->skin.base);
	colorscale = 1;
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 4;
		colorscale *= 0.25f;
	}
	base = ent->effects & EF_FULLBRIGHT ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	GL_DepthTest(true);
	GL_ColorPointer(varray_color4f);

	GL_VertexPointer(surf->mesh.data_vertex3f);
	m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
	R_Mesh_State_Texture(&m);
	R_FillColors(varray_color4f, surf->mesh.num_vertices, base, base, base, currentalpha);
	if (!(ent->effects & EF_FULLBRIGHT))
	{
		if (surf->dlightframe == r_framecount)
			RSurf_LightSeparate_Vertex3f_Color4f(&ent->inversematrix, surf->dlightbits, surf->mesh.num_vertices, surf->mesh.data_vertex3f, varray_color4f, 1);
		if (surf->flags & SURF_LIGHTMAP)
			RSurf_AddLightmapToVertexColors_Color4f(surf->mesh.data_lightmapoffsets, varray_color4f,surf->mesh.num_vertices, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
	}
	RSurf_FogColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, colorscale, surf->mesh.num_vertices, modelorg);
	R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
}

static void RSurfShader_Wall_Pass_Glow(const entity_render_t *ent, const msurface_t *surf, const texture_t *texture, int rendertype, float currentalpha)
{
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	GL_ColorPointer(varray_color4f);

	GL_VertexPointer(surf->mesh.data_vertex3f);
	if (m.tex[0])
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
	R_Mesh_State_Texture(&m);
	RSurf_FoggedColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, 1, 1, 1, currentalpha, 1, surf->mesh.num_vertices, modelorg);
	R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
}

static void RSurfShader_Wall_Pass_Fog(const entity_render_t *ent, const msurface_t *surf, const texture_t *texture, int rendertype, float currentalpha)
{
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.fog);
	GL_ColorPointer(varray_color4f);

	GL_VertexPointer(surf->mesh.data_vertex3f);
	if (m.tex[0])
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
	R_Mesh_State_Texture(&m);
	RSurf_FogPassColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], currentalpha, 1, surf->mesh.num_vertices, modelorg);
	R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
}

static void RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetailGlow(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	m.tex[1] = R_GetTexture((**surfchain).lightmaptexture);
	m.texrgbscale[1] = 2;
	m.tex[2] = R_GetTexture(texture->skin.detail);
	m.texrgbscale[2] = 2;
	if (texture->skin.glow)
	{
		m.tex[3] = R_GetTexture(texture->skin.glow);
		m.texcombinergb[3] = GL_ADD;
	}
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);

	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
			//if (m.tex[1] != lightmaptexturenum)
			//{
				m.tex[1] = lightmaptexturenum;
			//	R_Mesh_State_Texture(&m);
			//}
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			m.pointer_texcoord[2] = surf->mesh.data_texcoorddetail2f;
			m.pointer_texcoord[3] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetail(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	m.tex[1] = R_GetTexture((**surfchain).lightmaptexture);
	m.texrgbscale[1] = 2;
	m.tex[2] = R_GetTexture(texture->skin.detail);
	m.texrgbscale[2] = 2;
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);

	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
			//if (m.tex[1] != lightmaptexturenum)
			//{
				m.tex[1] = lightmaptexturenum;
			//	R_Mesh_State_Texture(&m);
			//}
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			m.pointer_texcoord[2] = surf->mesh.data_texcoorddetail2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmap(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	m.tex[1] = R_GetTexture((**surfchain).lightmaptexture);
	m.texrgbscale[1] = 2;
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
			//if (m.tex[1] != lightmaptexturenum)
			//{
				m.tex[1] = lightmaptexturenum;
			//	R_Mesh_State_Texture(&m);
			//}
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTexture(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_DepthMask(true);
	GL_DepthTest(true);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	m.tex[0] = R_GetTexture(texture->skin.base);
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseLightmap(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture((**surfchain).lightmaptexture);
	GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
			//if (m.tex[0] != lightmaptexturenum)
			//{
				m.tex[0] = lightmaptexturenum;
			//	R_Mesh_State_Texture(&m);
			//}
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Fog(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	GL_ColorPointer(varray_color4f);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			if (m.tex[0])
				m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State_Texture(&m);
			RSurf_FogPassColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], 1, 1, surf->mesh.num_vertices, modelorg);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDetail(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.detail);
	GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoorddetail2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Glow(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

/*
static void RSurfShader_OpaqueWall_Pass_OpaqueGlow(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
	GL_DepthMask(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	if (m.tex[0])
		GL_Color(1, 1, 1, 1);
	else
		GL_Color(0, 0, 0, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}
*/

static void RSurfShader_OpaqueWall_Pass_BaseLightmapOnly(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture((**surfchain).lightmaptexture);
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
			//if (m.tex[0] != lightmaptexturenum)
			//{
				m.tex[0] = lightmaptexturenum;
			//	R_Mesh_State_Texture(&m);
			//}
			GL_VertexPointer(surf->mesh.data_vertex3f);
			m.pointer_texcoord[0] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State_Texture(&m);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
		}
	}
}

static void RSurfShader_Wall_Vertex_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->brushq1.surfaces + calldata2;
	int rendertype;
	float currentalpha;
	texture_t *texture;
	R_Mesh_Matrix(&ent->matrix);

	texture = surf->texinfo->texture;
	if (texture->animated)
		texture = texture->anim_frames[ent->frame != 0][(texture->anim_total[ent->frame != 0] >= 2) ? ((int) (cl.time * 5.0f) % texture->anim_total[ent->frame != 0]) : 0];

	currentalpha = ent->alpha;
	if (ent->effects & EF_ADDITIVE)
		rendertype = SURFRENDER_ADD;
	else if (currentalpha < 1 || texture->skin.fog != NULL)
		rendertype = SURFRENDER_ALPHA;
	else
		rendertype = SURFRENDER_OPAQUE;

	RSurfShader_Wall_Pass_BaseVertex(ent, surf, texture, rendertype, currentalpha);
	if (texture->skin.glow)
		RSurfShader_Wall_Pass_Glow(ent, surf, texture, rendertype, currentalpha);
	if (fogenabled)
		RSurfShader_Wall_Pass_Fog(ent, surf, texture, rendertype, currentalpha);
}

static void RSurfShader_Wall_Lightmap(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	msurface_t **chain;
	vec3_t center;
	if (texture->rendertype != SURFRENDER_OPAQUE)
	{
		// transparent vertex shaded from lightmap
		for (chain = surfchain;(surf = *chain) != NULL;chain++)
		{
			if (surf->visframe == r_framecount)
			{
				Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
				R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->brushq1.surfaces);
			}
		}
	}
	else if (ent->effects & EF_FULLBRIGHT)
	{
		RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture, surfchain);
		if (r_detailtextures.integer)
			RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
		if (texture->skin.glow)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	/*
	// opaque base lighting
	else if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value <= 0)
	{
		if (r_ambient.value > 0)
		{
		}
		else
			RSurfShader_OpaqueWall_Pass_OpaqueGlow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	*/
	// opaque lightmapped
	else if (r_textureunits.integer >= 4 && gl_combine.integer && r_detailtextures.integer && !gl_lightmaps.integer)
	{
		RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetailGlow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	else if (r_textureunits.integer >= 3 && gl_combine.integer && r_detailtextures.integer && !gl_lightmaps.integer)
	{
		RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetail(ent, texture, surfchain);
		if (texture->skin.glow)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	else if (r_textureunits.integer >= 2 && gl_combine.integer && !gl_lightmaps.integer)
	{
		RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmap(ent, texture, surfchain);
		if (r_detailtextures.integer)
			RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
		if (texture->skin.glow)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	else if (!gl_lightmaps.integer)
	{
		RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture, surfchain);
		RSurfShader_OpaqueWall_Pass_BaseLightmap(ent, texture, surfchain);
		if (r_detailtextures.integer)
			RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
		if (texture->skin.glow)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	else
	{
		RSurfShader_OpaqueWall_Pass_BaseLightmapOnly(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
}

Cshader_t Cshader_wall_lightmap = {{NULL, RSurfShader_Wall_Lightmap}, SHADERFLAGS_NEEDLIGHTMAP};
Cshader_t Cshader_water = {{NULL, RSurfShader_Water}, 0};
Cshader_t Cshader_sky = {{RSurfShader_Sky, NULL}, 0};

int Cshader_count = 3;
Cshader_t *Cshaders[3] =
{
	&Cshader_wall_lightmap,
	&Cshader_water,
	&Cshader_sky
};

void R_UpdateTextureInfo(entity_render_t *ent)
{
	int i, texframe, alttextures;
	texture_t *t;

	if (!ent->model)
		return;

	alttextures = ent->frame != 0;
	texframe = (int)(cl.time * 5.0f);
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

void R_PrepareSurfaces(entity_render_t *ent)
{
	int i, numsurfaces, *surfacevisframes;
	model_t *model;
	msurface_t *surf, *surfaces, **surfchain;
	vec3_t modelorg;

	if (!ent->model)
		return;

	model = ent->model;
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	numsurfaces = model->brushq1.nummodelsurfaces;
	surfaces = model->brushq1.surfaces + model->brushq1.firstmodelsurface;
	surfacevisframes = model->brushq1.surfacevisframes + model->brushq1.firstmodelsurface;

	R_UpdateTextureInfo(ent);

	if (r_dynamic.integer && !r_shadow_realtime_dlight.integer)
		R_MarkLights(ent);

	if (model->brushq1.light_ambient != r_ambient.value)
	{
		model->brushq1.light_ambient = r_ambient.value;
		for (i = 0;i < model->brushq1.nummodelsurfaces;i++)
			model->brushq1.surfaces[i + model->brushq1.firstmodelsurface].cached_dlight = true;
	}
	else
	{
		for (i = 0;i < model->brushq1.light_styles;i++)
		{
			if (model->brushq1.light_stylevalue[i] != d_lightstylevalue[model->brushq1.light_style[i]])
			{
				model->brushq1.light_stylevalue[i] = d_lightstylevalue[model->brushq1.light_style[i]];
				for (surfchain = model->brushq1.light_styleupdatechains[i];*surfchain;surfchain++)
					(**surfchain).cached_dlight = true;
			}
		}
	}

	for (i = 0, surf = surfaces;i < numsurfaces;i++, surf++)
	{
		if (surfacevisframes[i] == r_framecount)
		{
#if !WORLDNODECULLBACKFACES
			// mark any backface surfaces as not visible
			if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
			{
				if (!(surf->flags & SURF_PLANEBACK))
					surfacevisframes[i] = -1;
			}
			else
			{
				if ((surf->flags & SURF_PLANEBACK))
					surfacevisframes[i] = -1;
			}
			if (surfacevisframes[i] == r_framecount)
#endif
			{
				c_faces++;
				surf->visframe = r_framecount;
				if (surf->cached_dlight && surf->lightmaptexture != NULL)
					R_BuildLightMap(ent, surf);
			}
		}
	}
}

void R_DrawSurfaces(entity_render_t *ent, int type, msurface_t ***chains)
{
	int i;
	texture_t *t;
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	for (i = 0, t = ent->model->brushq1.textures;i < ent->model->brushq1.numtextures;i++, t++)
		if (t->shader->shaderfunc[type] && t->currentframe && chains[i] != NULL)
			t->shader->shaderfunc[type](ent, t->currentframe, chains[i]);
}

static void R_DrawPortal_Callback(const void *calldata1, int calldata2)
{
	int i;
	float *v;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	const mportal_t *portal = ent->model->brushq1.portals + calldata2;
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&ent->matrix);
	GL_VertexPointer(varray_vertex3f);

	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);

	i = portal - ent->model->brushq1.portals;
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
	R_Mesh_Draw(portal->numpoints, portal->numpoints - 2, polygonelements);
}

// LordHavoc: this is just a nice debugging tool, very slow
static void R_DrawPortals(entity_render_t *ent)
{
	int i;
	mportal_t *portal, *endportal;
	float temp[3], center[3], f;
	if (ent->model == NULL)
		return;
	for (portal = ent->model->brushq1.portals, endportal = portal + ent->model->brushq1.numportals;portal < endportal;portal++)
	{
		if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
		{
			VectorClear(temp);
			for (i = 0;i < portal->numpoints;i++)
				VectorAdd(temp, portal->points[i].position, temp);
			f = ixtable[portal->numpoints];
			VectorScale(temp, f, temp);
			Matrix4x4_Transform(&ent->matrix, temp, center);
			R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, ent, portal - ent->model->brushq1.portals);
		}
	}
}

void R_PrepareBrushModel(entity_render_t *ent)
{
	int i, numsurfaces, *surfacevisframes, *surfacepvsframes;
	msurface_t *surf;
	model_t *model;
#if WORLDNODECULLBACKFACES
	vec3_t modelorg;
#endif

	// because bmodels can be reused, we have to decide which things to render
	// from scratch every time
	model = ent->model;
	if (model == NULL)
		return;
#if WORLDNODECULLBACKFACES
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
#endif
	numsurfaces = model->brushq1.nummodelsurfaces;
	surf = model->brushq1.surfaces + model->brushq1.firstmodelsurface;
	surfacevisframes = model->brushq1.surfacevisframes + model->brushq1.firstmodelsurface;
	surfacepvsframes = model->brushq1.surfacepvsframes + model->brushq1.firstmodelsurface;
	for (i = 0;i < numsurfaces;i++, surf++)
	{
#if WORLDNODECULLBACKFACES
		// mark any backface surfaces as not visible
		if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
		{
			if ((surf->flags & SURF_PLANEBACK))
				surfacevisframes[i] = r_framecount;
		}
		else if (!(surf->flags & SURF_PLANEBACK))
			surfacevisframes[i] = r_framecount;
#else
		surfacevisframes[i] = r_framecount;
#endif
		surf->dlightframe = -1;
	}
	R_PrepareSurfaces(ent);
}

void R_SurfaceWorldNode (entity_render_t *ent)
{
	int i, *surfacevisframes, *surfacepvsframes, surfnum;
	msurface_t *surf;
	mleaf_t *leaf;
	model_t *model;
	vec3_t modelorg;

	// equivilant to quake's RecursiveWorldNode but faster and more effective
	model = ent->model;
	if (model == NULL)
		return;
	surfacevisframes = model->brushq1.surfacevisframes + model->brushq1.firstmodelsurface;
	surfacepvsframes = model->brushq1.surfacepvsframes + model->brushq1.firstmodelsurface;
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	for (leaf = model->brushq1.pvsleafchain;leaf;leaf = leaf->pvschain)
	{
		if (!R_CullBox (leaf->mins, leaf->maxs))
		{
			c_leafs++;
			leaf->visframe = r_framecount;
		}
	}

	for (i = 0;i < model->brushq1.pvssurflistlength;i++)
	{
		surfnum = model->brushq1.pvssurflist[i];
		surf = model->brushq1.surfaces + surfnum;
#if WORLDNODECULLBACKFACES
		if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
		{
			if ((surf->flags & SURF_PLANEBACK) && !R_CullBox (surf->poly_mins, surf->poly_maxs))
				surfacevisframes[surfnum] = r_framecount;
		}
		else
		{
			if (!(surf->flags & SURF_PLANEBACK) && !R_CullBox (surf->poly_mins, surf->poly_maxs))
				surfacevisframes[surfnum] = r_framecount;
		}
#else
		if (!R_CullBox (surf->poly_mins, surf->poly_maxs))
			surfacevisframes[surfnum] = r_framecount;
#endif
	}
}

static void R_PortalWorldNode(entity_render_t *ent, mleaf_t *viewleaf)
{
	int c, leafstackpos, *mark, *surfacevisframes;
#if WORLDNODECULLBACKFACES
	int n;
	msurface_t *surf;
#endif
	mleaf_t *leaf, *leafstack[8192];
	mportal_t *p;
	vec3_t modelorg;
	msurface_t *surfaces;
	if (ent->model == NULL)
		return;
	// LordHavoc: portal-passage worldnode with PVS;
	// follows portals leading outward from viewleaf, does not venture
	// offscreen or into leafs that are not visible, faster than Quake's
	// RecursiveWorldNode
	surfaces = ent->model->brushq1.surfaces;
	surfacevisframes = ent->model->brushq1.surfacevisframes;
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	viewleaf->worldnodeframe = r_framecount;
	leafstack[0] = viewleaf;
	leafstackpos = 1;
	while (leafstackpos)
	{
		c_leafs++;
		leaf = leafstack[--leafstackpos];
		leaf->visframe = r_framecount;
		// draw any surfaces bounding this leaf
		if (leaf->nummarksurfaces)
		{
			for (c = leaf->nummarksurfaces, mark = leaf->firstmarksurface;c;c--)
			{
#if WORLDNODECULLBACKFACES
				n = *mark++;
				if (surfacevisframes[n] != r_framecount)
				{
					surf = surfaces + n;
					if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
					{
						if ((surf->flags & SURF_PLANEBACK))
							surfacevisframes[n] = r_framecount;
					}
					else
					{
						if (!(surf->flags & SURF_PLANEBACK))
							surfacevisframes[n] = r_framecount;
					}
				}
#else
				surfacevisframes[*mark++] = r_framecount;
#endif
			}
		}
		// follow portals into other leafs
		for (p = leaf->portals;p;p = p->next)
		{
			// LordHavoc: this DotProduct hurts less than a cache miss
			// (which is more likely to happen if backflowing through leafs)
			if (DotProduct(modelorg, p->plane.normal) < (p->plane.dist + 1))
			{
				leaf = p->past;
				if (leaf->worldnodeframe != r_framecount)
				{
					leaf->worldnodeframe = r_framecount;
					// FIXME: R_CullBox is absolute, should be done relative
					if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
						leafstack[leafstackpos++] = leaf;
				}
			}
		}
	}
}

void R_PVSUpdate (entity_render_t *ent, mleaf_t *viewleaf)
{
	int j, c, *surfacepvsframes, *mark;
	mleaf_t *leaf;
	model_t *model;

	model = ent->model;
	if (model && (model->brushq1.pvsviewleaf != viewleaf || model->brushq1.pvsviewleafnovis != r_novis.integer))
	{
		model->brushq1.pvsframecount++;
		model->brushq1.pvsviewleaf = viewleaf;
		model->brushq1.pvsviewleafnovis = r_novis.integer;
		model->brushq1.pvsleafchain = NULL;
		model->brushq1.pvssurflistlength = 0;
		if (viewleaf)
		{
			surfacepvsframes = model->brushq1.surfacepvsframes;
			for (j = 0, leaf = model->brushq1.data_leafs;j < model->brushq1.num_leafs;j++, leaf++)
			{
				if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex))
				{
					leaf->pvsframe = model->brushq1.pvsframecount;
					leaf->pvschain = model->brushq1.pvsleafchain;
					model->brushq1.pvsleafchain = leaf;
					// mark surfaces bounding this leaf as visible
					for (c = leaf->nummarksurfaces, mark = leaf->firstmarksurface;c;c--, mark++)
						surfacepvsframes[*mark] = model->brushq1.pvsframecount;
				}
			}
			model->brushq1.BuildPVSTextureChains(model);
		}
	}
}

void R_WorldVisibility(entity_render_t *ent)
{
	vec3_t modelorg;
	mleaf_t *viewleaf;

	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	viewleaf = (ent->model && ent->model->brushq1.PointInLeaf) ? ent->model->brushq1.PointInLeaf(ent->model, modelorg) : NULL;
	R_PVSUpdate(ent, viewleaf);

	if (!viewleaf)
		return;

	if (r_surfaceworldnode.integer || viewleaf->contents == CONTENTS_SOLID)
		R_SurfaceWorldNode (ent);
	else
		R_PortalWorldNode (ent, viewleaf);
}

void R_DrawWorld(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (!ent->model->brushq1.num_leafs)
	{
		if (ent->model->DrawSky)
			ent->model->DrawSky(ent);
		if (ent->model->Draw)
			ent->model->Draw(ent);
	}
	else
	{
		R_PrepareSurfaces(ent);
		R_DrawSurfaces(ent, SHADERSTAGE_SKY, ent->model->brushq1.pvstexturechains);
		R_DrawSurfaces(ent, SHADERSTAGE_NORMAL, ent->model->brushq1.pvstexturechains);
		if (r_drawportals.integer)
			R_DrawPortals(ent);
	}
}

void R_Model_Brush_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (ent != &cl_entities[0].render)
		R_PrepareBrushModel(ent);
	R_DrawSurfaces(ent, SHADERSTAGE_SKY, ent->model->brushq1.pvstexturechains);
}

void R_Model_Brush_Draw(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	c_bmodels++;
	if (ent != &cl_entities[0].render)
		R_PrepareBrushModel(ent);
	R_DrawSurfaces(ent, SHADERSTAGE_NORMAL, ent->model->brushq1.pvstexturechains);
}

void R_Model_Brush_DrawShadowVolume (entity_render_t *ent, vec3_t relativelightorigin, float lightradius)
{
#if 0
	int i;
	msurface_t *surf;
	float projectdistance, f, temp[3], lightradius2;
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	lightradius2 = lightradius * lightradius;
	R_UpdateTextureInfo(ent);
	projectdistance = lightradius + ent->model->radius;//projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
	//projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
	for (i = 0, surf = ent->model->brushq1.surfaces + ent->model->brushq1.firstmodelsurface;i < ent->model->brushq1.nummodelsurfaces;i++, surf++)
	{
		if (surf->texinfo->texture->rendertype == SURFRENDER_OPAQUE && surf->flags & SURF_SHADOWCAST)
		{
			f = PlaneDiff(relativelightorigin, surf->plane);
			if (surf->flags & SURF_PLANEBACK)
				f = -f;
			// draw shadows only for frontfaces and only if they are close
			if (f >= 0.1 && f < lightradius)
			{
				temp[0] = bound(surf->poly_mins[0], relativelightorigin[0], surf->poly_maxs[0]) - relativelightorigin[0];
				temp[1] = bound(surf->poly_mins[1], relativelightorigin[1], surf->poly_maxs[1]) - relativelightorigin[1];
				temp[2] = bound(surf->poly_mins[2], relativelightorigin[2], surf->poly_maxs[2]) - relativelightorigin[2];
				if (DotProduct(temp, temp) < lightradius2)
					R_Shadow_VolumeFromSphere(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_vertex3f, surf->mesh.data_element3i, surf->mesh.data_neighbor3i, relativelightorigin, projectdistance, lightradius);
			}
		}
	}
#else
	int t, leafnum, marksurfnum, trianglenum;
	const int *e;
	msurface_t *surf;
	mleaf_t *leaf;
	const qbyte *pvs;
	float projectdistance;
	const float *v[3];
	vec3_t lightmins, lightmaxs;
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	R_UpdateTextureInfo(ent);
	projectdistance = lightradius + ent->model->radius;//projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
	lightmins[0] = relativelightorigin[0] - lightradius;
	lightmins[1] = relativelightorigin[1] - lightradius;
	lightmins[2] = relativelightorigin[2] - lightradius;
	lightmaxs[0] = relativelightorigin[0] + lightradius;
	lightmaxs[1] = relativelightorigin[1] + lightradius;
	lightmaxs[2] = relativelightorigin[2] + lightradius;
	/*
	R_Shadow_PrepareShadowMark(ent->model->brush.shadowmesh->numtriangles);
	maxmarksurfaces = sizeof(surfacelist) / sizeof(surfacelist[0]);
	ent->model->brushq1.GetVisible(ent->model, relativelightorigin, lightmins, lightmaxs, 0, NULL, NULL, maxmarkleafs, markleaf, &nummarkleafs);
	for (marksurfacenum = 0;marksurfacenum < nummarksurfaces;marksurfacenum++)
	{
		surf = marksurface[marksurfacenum];
		if (surf->shadowmark != shadowmarkcount)
		{
			surf->shadowmark = shadowmarkcount;
			if (BoxesOverlap(lightmins, lightmaxs, surf->poly_mins, surf->poly_maxs) && surf->texinfo->texture->rendertype == SURFRENDER_OPAQUE && (surf->flags & SURF_SHADOWCAST))
			{
				for (trianglenum = 0, t = surf->num_firstshadowmeshtriangle, e = ent->model->brush.shadowmesh->element3i + t * 3;trianglenum < surf->mesh.num_triangles;trianglenum++, t++, e += 3)
				{
					v[0] = ent->model->brush.shadowmesh->vertex3f + e[0] * 3;
					v[1] = ent->model->brush.shadowmesh->vertex3f + e[1] * 3;
					v[2] = ent->model->brush.shadowmesh->vertex3f + e[2] * 3;
					if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
						shadowmarklist[numshadowmark++] = t;
				}
			}
		}
	}
	*/
	R_Shadow_PrepareShadowMark(ent->model->brush.shadowmesh->numtriangles);
	if (ent->model->brush.GetPVS && (pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin)))
	{
		pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin);
		// FIXME: use BSP recursion in q1bsp as dlights are often small
		for (leafnum = 0, leaf = ent->model->brushq1.data_leafs;leafnum < ent->model->brushq1.num_leafs;leafnum++, leaf++)
		{
			if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && CHECKPVSBIT(pvs, leaf->clusterindex))
			{
				for (marksurfnum = 0;marksurfnum < leaf->nummarksurfaces;marksurfnum++)
				{
					surf = ent->model->brushq1.surfaces + leaf->firstmarksurface[marksurfnum];
					if (surf->shadowmark != shadowmarkcount)
					{
						surf->shadowmark = shadowmarkcount;
						if (BoxesOverlap(lightmins, lightmaxs, surf->poly_mins, surf->poly_maxs) && surf->texinfo->texture->rendertype == SURFRENDER_OPAQUE && (surf->flags & SURF_SHADOWCAST))
						{
							for (trianglenum = 0, t = surf->num_firstshadowmeshtriangle, e = ent->model->brush.shadowmesh->element3i + t * 3;trianglenum < surf->mesh.num_triangles;trianglenum++, t++, e += 3)
							{
								v[0] = ent->model->brush.shadowmesh->vertex3f + e[0] * 3;
								v[1] = ent->model->brush.shadowmesh->vertex3f + e[1] * 3;
								v[2] = ent->model->brush.shadowmesh->vertex3f + e[2] * 3;
								if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
									shadowmarklist[numshadowmark++] = t;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		for (marksurfnum = 0, surf = ent->model->brushq1.surfaces + ent->model->brushq1.firstmodelsurface;marksurfnum < ent->model->brushq1.nummodelsurfaces;marksurfnum++, surf++)
		{
			if (BoxesOverlap(lightmins, lightmaxs, surf->poly_mins, surf->poly_maxs) && surf->texinfo->texture->rendertype == SURFRENDER_OPAQUE && (surf->flags & SURF_SHADOWCAST))
			{
				for (trianglenum = 0, t = surf->num_firstshadowmeshtriangle, e = ent->model->brush.shadowmesh->element3i + t * 3;trianglenum < surf->mesh.num_triangles;trianglenum++, t++, e += 3)
				{
					v[0] = ent->model->brush.shadowmesh->vertex3f + e[0] * 3;
					v[1] = ent->model->brush.shadowmesh->vertex3f + e[1] * 3;
					v[2] = ent->model->brush.shadowmesh->vertex3f + e[2] * 3;
					if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
						shadowmarklist[numshadowmark++] = t;
				}
			}
		}
	}
	R_Shadow_VolumeFromList(ent->model->brush.shadowmesh->numverts, ent->model->brush.shadowmesh->numtriangles, ent->model->brush.shadowmesh->vertex3f, ent->model->brush.shadowmesh->element3i, ent->model->brush.shadowmesh->neighbor3i, relativelightorigin, projectdistance, numshadowmark, shadowmarklist);
#endif
}

void R_Model_Brush_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap)
{
	int leafnum, marksurfnum;
	msurface_t *surf;
	mleaf_t *leaf;
	const qbyte *pvs;
	texture_t *t;
	float lightmins[3], lightmaxs[3];
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	lightmins[0] = relativelightorigin[0] - lightradius;
	lightmins[1] = relativelightorigin[1] - lightradius;
	lightmins[2] = relativelightorigin[2] - lightradius;
	lightmaxs[0] = relativelightorigin[0] + lightradius;
	lightmaxs[1] = relativelightorigin[1] + lightradius;
	lightmaxs[2] = relativelightorigin[2] + lightradius;
	R_UpdateTextureInfo(ent);
	shadowmarkcount++;
	if (ent->model->brush.GetPVS && (pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin)))
	{
		pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin);
		for (leafnum = 0, leaf = ent->model->brushq1.data_leafs;leafnum < ent->model->brushq1.num_leafs;leafnum++, leaf++)
		{
			if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && CHECKPVSBIT(pvs, leaf->clusterindex))
			{
				for (marksurfnum = 0;marksurfnum < leaf->nummarksurfaces;marksurfnum++)
				{
					surf = ent->model->brushq1.surfaces + leaf->firstmarksurface[marksurfnum];
					if (surf->shadowmark != shadowmarkcount)
					{
						surf->shadowmark = shadowmarkcount;
						if (BoxesOverlap(lightmins, lightmaxs, surf->poly_mins, surf->poly_maxs) && (ent != &cl_entities[0].render || surf->visframe == r_framecount))
						{
							t = surf->texinfo->texture->currentframe;
							if (t->rendertype == SURFRENDER_OPAQUE && t->flags & SURF_SHADOWLIGHT)
							{
								R_Shadow_DiffuseLighting(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i, surf->mesh.data_vertex3f, surf->mesh.data_svector3f, surf->mesh.data_tvector3f, surf->mesh.data_normal3f, surf->mesh.data_texcoordtexture2f, relativelightorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, lightcubemap);
								R_Shadow_SpecularLighting(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i, surf->mesh.data_vertex3f, surf->mesh.data_svector3f, surf->mesh.data_tvector3f, surf->mesh.data_normal3f, surf->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.gloss, t->skin.nmap, lightcubemap);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		for (marksurfnum = 0, surf = ent->model->brushq1.surfaces + ent->model->brushq1.firstmodelsurface;marksurfnum < ent->model->brushq1.nummodelsurfaces;marksurfnum++, surf++)
		{
			if (BoxesOverlap(lightmins, lightmaxs, surf->poly_mins, surf->poly_maxs) && (ent != &cl_entities[0].render || surf->visframe == r_framecount))
			{
				t = surf->texinfo->texture->currentframe;
				if (t->rendertype == SURFRENDER_OPAQUE && t->flags & SURF_SHADOWLIGHT)
				{
					R_Shadow_DiffuseLighting(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i, surf->mesh.data_vertex3f, surf->mesh.data_svector3f, surf->mesh.data_tvector3f, surf->mesh.data_normal3f, surf->mesh.data_texcoordtexture2f, relativelightorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, lightcubemap);
					R_Shadow_SpecularLighting(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i, surf->mesh.data_vertex3f, surf->mesh.data_svector3f, surf->mesh.data_tvector3f, surf->mesh.data_normal3f, surf->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.gloss, t->skin.nmap, lightcubemap);
				}
			}
		}
	}
}

void R_DrawCollisionBrush(colbrushf_t *brush)
{
	int i;
	i = ((int)brush) / sizeof(colbrushf_t);
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_VertexPointer(brush->points->v);
	R_Mesh_Draw(brush->numpoints, brush->numtriangles, brush->elements);
}

void R_Q3BSP_DrawSkyFace(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	if (!face->num_triangles)
		return;
	c_faces++;
	if (skyrendernow)
	{
		skyrendernow = false;
		if (skyrendermasked)
			R_Sky();
	}

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
	R_Mesh_State_Texture(&m);

	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_ColorMask(1,1,1,1);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_OpaqueGlow(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	if (face->texture->skin.glow)
	{
		m.tex[0] = R_GetTexture(face->texture->skin.glow);
		m.pointer_texcoord[0] = face->data_texcoordtexture2f;
		GL_Color(1, 1, 1, 1);
	}
	else
		GL_Color(0, 0, 0, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureLightmapCombine(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	m.tex[1] = R_GetTexture(face->lightmaptexture);
	m.pointer_texcoord[1] = face->data_texcoordlightmap2f;
	m.texrgbscale[1] = 2;
	GL_Color(1, 1, 1, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Texture(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	GL_Color(1, 1, 1, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Lightmap(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->lightmaptexture);
	m.pointer_texcoord[0] = face->data_texcoordlightmap2f;
	GL_Color(1, 1, 1, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_LightmapOnly(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->lightmaptexture);
	m.pointer_texcoord[0] = face->data_texcoordlightmap2f;
	if (r_shadow_realtime_world.integer)
		GL_Color(r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, r_shadow_realtime_world_lightmaps.value, 1);
	else
		GL_Color(1, 1, 1, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Glow(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	if (face->texture->skin.glow)
	{
		m.tex[0] = R_GetTexture(face->texture->skin.glow);
		m.pointer_texcoord[0] = face->data_texcoordtexture2f;
		GL_Color(1, 1, 1, 1);
	}
	else
		GL_Color(0, 0, 0, 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureVertex(entity_render_t *ent, q3mface_t *face)
{
	int i;
	float mul;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	mul = 2.0f;
	if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value != 1)
		mul *= r_shadow_realtime_world_lightmaps.value;
	if (mul == 2 && gl_combine.integer)
	{
		m.texrgbscale[0] = 2;
		GL_ColorPointer(face->data_color4f);
	}
	else if (mul == 1)
		GL_ColorPointer(face->data_color4f);
	else
	{
		for (i = 0;i < face->num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->data_color4f[i*4+0] * mul;
			varray_color4f[i*4+1] = face->data_color4f[i*4+1] * mul;
			varray_color4f[i*4+2] = face->data_color4f[i*4+2] * mul;
			varray_color4f[i*4+3] = face->data_color4f[i*4+3];
		}
		GL_ColorPointer(varray_color4f);
	}
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_VertexOnly(entity_render_t *ent, q3mface_t *face)
{
	int i;
	float mul;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	R_Mesh_State_Texture(&m);
	mul = 2.0f;
	if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value != 1)
		mul *= r_shadow_realtime_world_lightmaps.value;
	if (mul == 1)
		GL_ColorPointer(face->data_color4f);
	else
	{
		for (i = 0;i < face->num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->data_color4f[i*4+0] * 2.0f;
			varray_color4f[i*4+1] = face->data_color4f[i*4+1] * 2.0f;
			varray_color4f[i*4+2] = face->data_color4f[i*4+2] * 2.0f;
			varray_color4f[i*4+3] = face->data_color4f[i*4+3];
		}
		GL_ColorPointer(varray_color4f);
	}
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_AddTextureAmbient(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	GL_Color(r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), 1);
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
}

void R_Q3BSP_DrawFace_TransparentCallback(const void *voident, int facenumber)
{
	const entity_render_t *ent = voident;
	q3mface_t *face = ent->model->brushq3.data_faces + facenumber;
	rmeshstate_t m;
	R_Mesh_Matrix(&ent->matrix);
	memset(&m, 0, sizeof(m));
	if (ent->effects & EF_ADDITIVE)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	// LordHavoc: quake3 was not able to do this; lit transparent surfaces
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 2;
		if (r_textureunits.integer >= 2)
		{
			m.tex[1] = R_GetTexture(face->lightmaptexture);
			m.pointer_texcoord[1] = face->data_texcoordlightmap2f;
			GL_Color(1, 1, 1, ent->alpha);
		}
		else
		{
			if (ent->alpha == 1)
				GL_ColorPointer(face->data_color4f);
			else
			{
				int i;
				for (i = 0;i < face->num_vertices;i++)
				{
					varray_color4f[i*4+0] = face->data_color4f[i*4+0];
					varray_color4f[i*4+1] = face->data_color4f[i*4+1];
					varray_color4f[i*4+2] = face->data_color4f[i*4+2];
					varray_color4f[i*4+3] = face->data_color4f[i*4+3] * ent->alpha;
				}
				GL_ColorPointer(varray_color4f);
			}
		}
	}
	else
	{
		int i;
		for (i = 0;i < face->num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->data_color4f[i*4+0] * 2.0f;
			varray_color4f[i*4+1] = face->data_color4f[i*4+1] * 2.0f;
			varray_color4f[i*4+2] = face->data_color4f[i*4+2] * 2.0f;
			varray_color4f[i*4+3] = face->data_color4f[i*4+3] * ent->alpha;
		}
		GL_ColorPointer(varray_color4f);
	}
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	qglDisable(GL_CULL_FACE);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	qglEnable(GL_CULL_FACE);
}

void R_Q3BSP_DrawFace(entity_render_t *ent, q3mface_t *face)
{
	if (!face->num_triangles)
		return;
	if (face->texture->surfaceparms)
	{
		if (face->texture->surfaceflags & (Q3SURFACEFLAG_SKY | Q3SURFACEFLAG_NODRAW))
			return;
	}
	c_faces++;
	face->visframe = r_framecount;
	if ((face->texture->surfaceparms & Q3SURFACEPARM_TRANS) || ent->alpha < 1 || (ent->effects & EF_ADDITIVE))
	{
		vec3_t facecenter, center;
		facecenter[0] = (face->mins[0] + face->maxs[0]) * 0.5f;
		facecenter[1] = (face->mins[1] + face->maxs[1]) * 0.5f;
		facecenter[2] = (face->mins[2] + face->maxs[2]) * 0.5f;
		Matrix4x4_Transform(&ent->matrix, facecenter, center);
		R_MeshQueue_AddTransparent(center, R_Q3BSP_DrawFace_TransparentCallback, ent, face - ent->model->brushq3.data_faces);
		return;
	}
	R_Mesh_Matrix(&ent->matrix);
	if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value <= 0)
		R_Q3BSP_DrawFace_OpaqueWall_Pass_OpaqueGlow(ent, face);
	else if ((ent->effects & EF_FULLBRIGHT) || r_fullbright.integer)
	{
		R_Q3BSP_DrawFace_OpaqueWall_Pass_Texture(ent, face);
		if (face->texture->skin.glow)
			R_Q3BSP_DrawFace_OpaqueWall_Pass_Glow(ent, face);
	}
	else if (face->lightmaptexture)
	{
		if (gl_lightmaps.integer)
			R_Q3BSP_DrawFace_OpaqueWall_Pass_LightmapOnly(ent, face);
		else
		{
			if (r_textureunits.integer >= 2 && gl_combine.integer)
				R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureLightmapCombine(ent, face);
			else
			{
				R_Q3BSP_DrawFace_OpaqueWall_Pass_Texture(ent, face);
				R_Q3BSP_DrawFace_OpaqueWall_Pass_Lightmap(ent, face);
			}
			if (face->texture->skin.glow)
				R_Q3BSP_DrawFace_OpaqueWall_Pass_Glow(ent, face);
		}
	}
	else
	{
		if (gl_lightmaps.integer)
			R_Q3BSP_DrawFace_OpaqueWall_Pass_VertexOnly(ent, face);
		else
		{
			R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureVertex(ent, face);
			if (face->texture->skin.glow)
				R_Q3BSP_DrawFace_OpaqueWall_Pass_Glow(ent, face);
		}
	}
	if (r_ambient.value)
		R_Q3BSP_DrawFace_OpaqueWall_Pass_AddTextureAmbient(ent, face);
}

void R_Q3BSP_RecursiveWorldNode(entity_render_t *ent, q3mnode_t *node, const vec3_t modelorg, qbyte *pvs, int markframe)
{
	int i;
	q3mleaf_t *leaf;
	for (;;)
	{
		if (R_CullBox(node->mins, node->maxs))
			return;
		if (!node->plane)
			break;
		c_nodes++;
		R_Q3BSP_RecursiveWorldNode(ent, node->children[0], modelorg, pvs, markframe);
		node = node->children[1];
	}
	leaf = (q3mleaf_t *)node;
	if (CHECKPVSBIT(pvs, leaf->clusterindex))
	{
		c_leafs++;
		for (i = 0;i < leaf->numleaffaces;i++)
			leaf->firstleafface[i]->markframe = markframe;
	}
}

// FIXME: num_leafs needs to be recalculated at load time to include only
// node-referenced leafs, as some maps are incorrectly compiled with leafs for
// the submodels (which would render the submodels occasionally, as part of
// the world - not good)
void R_Q3BSP_MarkLeafPVS(entity_render_t *ent, qbyte *pvs, int markframe)
{
	int i, j;
	q3mleaf_t *leaf;
	for (j = 0, leaf = ent->model->brushq3.data_leafs;j < ent->model->brushq3.num_leafs;j++, leaf++)
	{
		if (CHECKPVSBIT(pvs, leaf->clusterindex))
		{
			c_leafs++;
			for (i = 0;i < leaf->numleaffaces;i++)
				leaf->firstleafface[i]->markframe = markframe;
		}
	}
}

static int r_q3bsp_framecount = -1;

void R_Q3BSP_DrawSky(entity_render_t *ent)
{
	int i;
	q3mface_t *face;
	vec3_t modelorg;
	model_t *model;
	qbyte *pvs;
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	if (r_drawcollisionbrushes.integer < 2)
	{
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		if (ent == &cl_entities[0].render && model->brush.num_pvsclusters && !r_novis.integer && (pvs = model->brush.GetPVS(model, modelorg)))
		{
			if (r_q3bsp_framecount != r_framecount)
			{
				r_q3bsp_framecount = r_framecount;
				R_Q3BSP_RecursiveWorldNode(ent, model->brushq3.data_nodes, modelorg, pvs, r_framecount);
				//R_Q3BSP_MarkLeafPVS(ent, pvs, r_framecount);
			}
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if (face->markframe == r_framecount && (face->texture->surfaceflags & Q3SURFACEFLAG_SKY) && !R_CullBox(face->mins, face->maxs))
					R_Q3BSP_DrawSkyFace(ent, face);
		}
		else
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if ((face->texture->surfaceflags & Q3SURFACEFLAG_SKY))
					R_Q3BSP_DrawSkyFace(ent, face);
	}
}

void R_Q3BSP_Draw(entity_render_t *ent)
{
	int i;
	q3mface_t *face;
	vec3_t modelorg;
	model_t *model;
	qbyte *pvs;
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	if (r_drawcollisionbrushes.integer < 2)
	{
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		if (ent == &cl_entities[0].render && model->brush.num_pvsclusters && !r_novis.integer && (pvs = model->brush.GetPVS(model, modelorg)))
		{
			if (r_q3bsp_framecount != r_framecount)
			{
				r_q3bsp_framecount = r_framecount;
				R_Q3BSP_RecursiveWorldNode(ent, model->brushq3.data_nodes, modelorg, pvs, r_framecount);
				//R_Q3BSP_MarkLeafPVS(ent, pvs, r_framecount);
			}
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if (face->markframe == r_framecount && !R_CullBox(face->mins, face->maxs))
					R_Q3BSP_DrawFace(ent, face);
		}
		else
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				R_Q3BSP_DrawFace(ent, face);
	}
	if (r_drawcollisionbrushes.integer >= 1)
	{
		rmeshstate_t m;
		memset(&m, 0, sizeof(m));
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		R_Mesh_State_Texture(&m);
		qglPolygonOffset(r_drawcollisionbrushes_polygonfactor.value, r_drawcollisionbrushes_polygonoffset.value);
		for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
			if (model->brushq3.data_thismodel->firstbrush[i].colbrushf && model->brushq3.data_thismodel->firstbrush[i].colbrushf->numtriangles)
				R_DrawCollisionBrush(model->brushq3.data_thismodel->firstbrush[i].colbrushf);
		qglPolygonOffset(0, 0);
	}
}

void R_Q3BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius)
{
#if 0
	int i;
	q3mface_t *face;
	vec3_t modelorg, lightmins, lightmaxs;
	model_t *model;
	float projectdistance;
	projectdistance = lightradius + ent->model->radius;//projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
	if (r_drawcollisionbrushes.integer < 2)
	{
		model = ent->model;
		R_Mesh_Matrix(&ent->matrix);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		//if (ent == &cl_entities[0].render && model->brush.num_pvsclusters && !r_novis.integer && (pvs = model->brush.GetPVS(model, modelorg)))
		//	R_Q3BSP_RecursiveWorldNode(ent, model->brushq3.data_nodes, modelorg, pvs, ++markframe);
		//else
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if (BoxesOverlap(lightmins, lightmaxs, face->mins, face->maxs))
					R_Shadow_VolumeFromSphere(face->num_vertices, face->num_triangles, face->data_vertex3f, face->data_element3i, face->data_neighbor3i, relativelightorigin, projectdistance, lightradius);
	}
#else
	int j, t, leafnum, marksurfnum;
	const int *e;
	const qbyte *pvs;
	const float *v[3];
	q3mface_t *face;
	q3mleaf_t *leaf;
	vec3_t modelorg, lightmins, lightmaxs;
	model_t *model;
	float projectdistance;
	projectdistance = lightradius + ent->model->radius;//projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
	if (r_drawcollisionbrushes.integer < 2)
	{
		model = ent->model;
		R_Mesh_Matrix(&ent->matrix);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		if (ent->model->brush.GetPVS && (pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin)))
		{	
			for (leafnum = 0, leaf = ent->model->brushq3.data_leafs;leafnum < ent->model->brushq3.num_leafs;leafnum++, leaf++)
			{
				if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && CHECKPVSBIT(pvs, leaf->clusterindex))
				{
					for (marksurfnum = 0;marksurfnum < leaf->numleaffaces;marksurfnum++)
					{
						face = leaf->firstleafface[marksurfnum];
						if (face->shadowmark != shadowmarkcount)
						{
							face->shadowmark = shadowmarkcount;
							if (BoxesOverlap(lightmins, lightmaxs, face->mins, face->maxs))
							{
								for (j = 0, t = face->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;j < face->num_triangles;j++, t++, e += 3)
								{
									v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
									v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
									v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
									if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
										shadowmarklist[numshadowmark++] = t;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			for (marksurfnum = 0, face = model->brushq3.data_thismodel->firstface;marksurfnum < model->brushq3.data_thismodel->numfaces;marksurfnum++, face++)
			{
				if (BoxesOverlap(lightmins, lightmaxs, face->mins, face->maxs))
				{
					for (j = 0, t = face->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;j < face->num_triangles;j++, t++, e += 3)
					{
						v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
						v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
						v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
						if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
							shadowmarklist[numshadowmark++] = t;
					}
				}
			}
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, projectdistance, numshadowmark, shadowmarklist);
	}
#endif
}

void R_Q3BSP_DrawFaceLight(entity_render_t *ent, q3mface_t *face, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap)
{
	if ((face->texture->surfaceflags & Q3SURFACEFLAG_NODRAW) || !face->num_triangles)
		return;
	R_Shadow_DiffuseLighting(face->num_vertices, face->num_triangles, face->data_element3i, face->data_vertex3f, face->data_svector3f, face->data_tvector3f, face->data_normal3f, face->data_texcoordtexture2f, relativelightorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, face->texture->skin.base, face->texture->skin.nmap, lightcubemap);
	R_Shadow_SpecularLighting(face->num_vertices, face->num_triangles, face->data_element3i, face->data_vertex3f, face->data_svector3f, face->data_tvector3f, face->data_normal3f, face->data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, face->texture->skin.gloss, face->texture->skin.nmap, lightcubemap);
}

void R_Q3BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap)
{
	int leafnum, marksurfnum;
	const qbyte *pvs;
	q3mface_t *face;
	q3mleaf_t *leaf;
	vec3_t modelorg, lightmins, lightmaxs;
	model_t *model;
	//qbyte *pvs;
	//static int markframe = 0;
	if (r_drawcollisionbrushes.integer < 2)
	{
		model = ent->model;
		R_Mesh_Matrix(&ent->matrix);
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		lightmins[0] = relativelightorigin[0] - lightradius;
		lightmins[1] = relativelightorigin[1] - lightradius;
		lightmins[2] = relativelightorigin[2] - lightradius;
		lightmaxs[0] = relativelightorigin[0] + lightradius;
		lightmaxs[1] = relativelightorigin[1] + lightradius;
		lightmaxs[2] = relativelightorigin[2] + lightradius;

		if (ent->model->brush.GetPVS && (pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin)))
		{	
			pvs = ent->model->brush.GetPVS(ent->model, relativelightorigin);
			for (leafnum = 0, leaf = ent->model->brushq3.data_leafs;leafnum < ent->model->brushq3.num_leafs;leafnum++, leaf++)
			{
				if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && CHECKPVSBIT(pvs, leaf->clusterindex))
				{
					for (marksurfnum = 0;marksurfnum < leaf->numleaffaces;marksurfnum++)
					{
						face = leaf->firstleafface[marksurfnum];
						if (face->shadowmark != shadowmarkcount)
						{
							face->shadowmark = shadowmarkcount;
							if (BoxesOverlap(lightmins, lightmaxs, face->mins, face->maxs) && (ent != &cl_entities[0].render || face->visframe == r_framecount))
								R_Q3BSP_DrawFaceLight(ent, face, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, lightcubemap);
						}
					}
				}
			}
		}
		else
		{
			for (marksurfnum = 0, face = model->brushq3.data_thismodel->firstface;marksurfnum < model->brushq3.data_thismodel->numfaces;marksurfnum++, face++)
				if (BoxesOverlap(lightmins, lightmaxs, face->mins, face->maxs) && (ent != &cl_entities[0].render || face->visframe == r_framecount))
					R_Q3BSP_DrawFaceLight(ent, face, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, lightcubemap);
		}
	}
}

static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
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
	Cvar_RegisterVariable(&gl_lightmaps);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

