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
q3msurface_t *r_q3bsp_maxsurfacelist[65536];
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
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			m.pointer_vertex = surf->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
	GL_ColorMask(1,1,1,1);
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
		texture = texture->anim_frames[ent->frame != 0][(texture->anim_total[ent->frame != 0] >= 2) ? ((int) (cl.time * 5.0f) % texture->anim_total[ent->frame != 0]) : 0];
	currentalpha = ent->alpha;
	if (surf->flags & SURF_WATERALPHA)
		currentalpha *= r_wateralpha.value;

	GL_DepthTest(true);
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
		GL_DepthMask(true);
	}

	turb = (surf->flags & SURF_DRAWTURB) && r_waterscroll.value;
	fullbright = (ent->effects & EF_FULLBRIGHT) || (surf->flags & SURF_DRAWFULLBRIGHT) || !surf->samples;
	base = fullbright ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	if (surf->flags & SURF_DRAWTURB)
		base *= 0.5f;
	if ((surf->flags & SURF_DRAWTURB) && gl_textureshader && r_watershader.value && !fogenabled)
	{
		// NVIDIA Geforce3 distortion texture shader on water
		GL_Color(1, 1, 1, currentalpha);
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = surf->mesh.data_vertex3f;
		m.tex[0] = R_GetTexture(mod_shared_distorttexture[(int)(cl.time * 16)&63]);
		m.tex[1] = R_GetTexture(texture->skin.base);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_REPLACE;
		m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
		m.pointer_texcoord[1] = surf->mesh.data_texcoordtexture2f;
		Matrix4x4_CreateFromQuakeEntity(&m.texmatrix[0], 0, 0, 0, 0, 0, 0, r_watershader.value);
		Matrix4x4_CreateTranslate(&m.texmatrix[1], sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
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
			Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		}
		colorscale = 1;
		if (gl_combine.integer)
		{
			m.texrgbscale[0] = 4;
			colorscale *= 0.25f;
		}
		R_FillColors(varray_color4f, surf->mesh.num_vertices, base, base, base, currentalpha);
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
					Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
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
					Matrix4x4_CreateTranslate(&m.texmatrix[0], sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
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
			m.tex[1] = lightmaptexturenum;
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			m.pointer_texcoord[2] = surf->mesh.data_texcoorddetail2f;
			m.pointer_texcoord[3] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.tex[1] = lightmaptexturenum;
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			m.pointer_texcoord[2] = surf->mesh.data_texcoorddetail2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.tex[1] = lightmaptexturenum;
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.tex[0] = lightmaptexturenum;
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseAmbient(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	GL_Color(r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
	m.pointer_color = varray_color4f;
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			m.pointer_vertex = surf->mesh.data_vertex3f;
			if (m.tex[0])
				m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			RSurf_FogPassColors_Vertex3f_Color4f(surf->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], 1, 1, surf->mesh.num_vertices, modelorg);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoorddetail2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surf->mesh.num_vertices);
			R_Mesh_Draw(surf->mesh.num_vertices, surf->mesh.num_triangles, surf->mesh.data_element3i);
			GL_LockArrays(0, 0);
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
			m.tex[0] = lightmaptexturenum;
			m.pointer_vertex = surf->mesh.data_vertex3f;
			m.pointer_texcoord[0] = surf->mesh.data_texcoordlightmap2f;
			R_Mesh_State(&m);
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

void R_UpdateLightmapInfo(entity_render_t *ent)
{
	int i;
	msurface_t *surface, **surfacechain;
	model_t *model = ent->model;
	if (!model)
		return;
	if (r_dynamic.integer && !r_shadow_realtime_dlight.integer)
		R_MarkLights(ent);
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		if (model->brushq1.light_stylevalue[i] != d_lightstylevalue[model->brushq1.light_style[i]])
		{
			model->brushq1.light_stylevalue[i] = d_lightstylevalue[model->brushq1.light_style[i]];
			for (surfacechain = model->brushq1.light_styleupdatechains[i];(surface = *surfacechain);surfacechain++)
				surface->cached_dlight = true;
		}
	}
}

void R_PrepareSurfaces(entity_render_t *ent)
{
	int i, numsurfaces, *surfacevisframes;
	model_t *model;
	msurface_t *surf, *surfaces;
	vec3_t modelorg;

	if (!ent->model)
		return;

	model = ent->model;
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	numsurfaces = model->nummodelsurfaces;
	surfaces = model->brushq1.surfaces + model->firstmodelsurface;
	surfacevisframes = model->brushq1.surfacevisframes + model->firstmodelsurface;
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

void R_DrawSurfaces(entity_render_t *ent, int flagsmask)
{
	int texturenumber, f;
	vec3_t center;
	msurface_t *surf, **chain, **surfchain;
	texture_t *t, *texture;
	model_t *model = ent->model;
	if (model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	for (texturenumber = 0, t = model->brushq1.textures;texturenumber < model->brushq1.numtextures;texturenumber++, t++)
	{
		if ((f = t->flags & flagsmask) && (texture = t->currentframe) && (surfchain = model->brushq1.pvstexturechains[texturenumber]) != NULL)
		{
			if (texture->flags & SURF_LIGHTMAP)
			{
				if (gl_lightmaps.integer)
				{
					RSurfShader_OpaqueWall_Pass_BaseLightmapOnly(ent, texture, surfchain);
					if (fogenabled)
						RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
				}
				else if (texture->rendertype != SURFRENDER_OPAQUE)
				{
					// transparent vertex shaded from lightmap
					for (chain = surfchain;(surf = *chain) != NULL;chain++)
					{
						if (surf->visframe == r_framecount)
						{
							Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
							R_MeshQueue_AddTransparent(center, RSurfShader_Transparent_Callback, ent, surf - ent->model->brushq1.surfaces);
						}
					}
				}
				else
				{
					if (ent->effects & EF_FULLBRIGHT || r_fullbright.integer)
					{
						RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture, surfchain);
						if (r_detailtextures.integer)
							RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
						if (texture->skin.glow)
							RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
					}
					else if (r_textureunits.integer >= 4 && gl_combine.integer && r_detailtextures.integer && r_ambient.value <= 0)
						RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetailGlow(ent, texture, surfchain);
					else
					{
						if (r_textureunits.integer >= 3 && gl_combine.integer && r_detailtextures.integer && r_ambient.value <= 0)
							RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmapDetail(ent, texture, surfchain);
						else
						{
							if (r_textureunits.integer >= 2 && gl_combine.integer)
								RSurfShader_OpaqueWall_Pass_BaseCombine_TextureLightmap(ent, texture, surfchain);
							else
							{
								RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture, surfchain);
								RSurfShader_OpaqueWall_Pass_BaseLightmap(ent, texture, surfchain);
							}
							if (r_ambient.value > 0)
								RSurfShader_OpaqueWall_Pass_BaseAmbient(ent, texture, surfchain);
							if (r_detailtextures.integer)
								RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
						}
						if (texture->skin.glow)
							RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
					}
					if (fogenabled)
						RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
				}
			}
			else if (texture->flags & SURF_DRAWTURB)
			{
				for (chain = surfchain;(surf = *chain) != NULL;chain++)
				{
					if (surf->visframe == r_framecount)
					{
						if (texture->rendertype == SURFRENDER_OPAQUE)
							RSurfShader_Transparent_Callback(ent, surf - ent->model->brushq1.surfaces);
						else
						{
							Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
							R_MeshQueue_AddTransparent(center, RSurfShader_Transparent_Callback, ent, surf - ent->model->brushq1.surfaces);
						}
					}
				}
			}
			else if (texture->flags & SURF_DRAWSKY)
				RSurfShader_Sky(ent, texture, surfchain);
		}
	}
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

	memset(&m, 0, sizeof(m));
	m.pointer_vertex = varray_vertex3f;
	R_Mesh_State(&m);

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
	GL_LockArrays(0, portal->numpoints);
	R_Mesh_Draw(portal->numpoints, portal->numpoints - 2, polygonelements);
	GL_LockArrays(0, 0);
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
	numsurfaces = model->nummodelsurfaces;
	surf = model->brushq1.surfaces + model->firstmodelsurface;
	surfacevisframes = model->brushq1.surfacevisframes + model->firstmodelsurface;
	surfacepvsframes = model->brushq1.surfacepvsframes + model->firstmodelsurface;
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
	R_UpdateTextureInfo(ent);
	R_UpdateLightmapInfo(ent);
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
	surfacevisframes = model->brushq1.surfacevisframes + model->firstmodelsurface;
	surfacepvsframes = model->brushq1.surfacepvsframes + model->firstmodelsurface;
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

	if (r_drawportals.integer)
		R_DrawPortals(ent);
}

void R_Q1BSP_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (ent != &cl_entities[0].render)
		R_PrepareBrushModel(ent);
	R_PrepareSurfaces(ent);
	R_DrawSurfaces(ent, SURF_DRAWSKY);
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	c_bmodels++;
	if (ent != &cl_entities[0].render)
		R_PrepareBrushModel(ent);
	R_PrepareSurfaces(ent);
	R_UpdateTextureInfo(ent);
	R_UpdateLightmapInfo(ent);
	R_DrawSurfaces(ent, SURF_DRAWTURB | SURF_LIGHTMAP);
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
			if (!CHECKPVSBIT(outclusterpvs, leaf->clusterindex))
			{
				SETPVSBIT(outclusterpvs, leaf->clusterindex);
				outclusterlist[outnumclusters++] = leaf->clusterindex;
			}
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
	int surfacelistindex, j, t;
	const int *e;
	const float *v[3];
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
			for (j = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;j < surface->mesh.num_triangles;j++, t++, e += 3)
			{
				v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
				v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
				v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
				if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
					shadowmarklist[numshadowmark++] = t;
			}
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + r_shadow_projectdistance.value, numshadowmark, shadowmarklist);
	}
}

void R_Q1BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, int numsurfaces, const int *surfacelist)
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
			else if (ent != &cl_entities[0].render || surface->visframe == r_framecount)
			{
				t = surface->texinfo->texture->currentframe;
				if (t->flags & SURF_LIGHTMAP && t->rendertype == SURFRENDER_OPAQUE)
					R_Shadow_RenderLighting(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, t->skin.gloss, lightcubemap, LIGHTING_DIFFUSE | LIGHTING_SPECULAR);
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
	i = ((int)brush) / sizeof(colbrushf_t);
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, brush->numpoints);
	R_Mesh_Draw(brush->numpoints, brush->numtriangles, brush->elements);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawCollisionFace(entity_render_t *ent, q3msurface_t *face)
{
	int i;
	rmeshstate_t m;
	if (!face->num_collisiontriangles)
		return;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = face->data_collisionvertex3f;
	R_Mesh_State(&m);
	i = ((int)face) / sizeof(q3msurface_t);
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, face->num_collisionvertices);
	R_Mesh_Draw(face->num_collisionvertices, face->num_collisiontriangles, face->data_collisionelement3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawSkyFace(entity_render_t *ent, q3msurface_t *face)
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
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);

	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
	GL_ColorMask(1,1,1,1);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_OpaqueGlow(entity_render_t *ent, q3msurface_t *face)
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
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureLightmapCombine(entity_render_t *ent, q3msurface_t *face)
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
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Texture(entity_render_t *ent, q3msurface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	GL_Color(1, 1, 1, 1);
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Lightmap(entity_render_t *ent, q3msurface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->lightmaptexture);
	m.pointer_texcoord[0] = face->data_texcoordlightmap2f;
	GL_Color(1, 1, 1, 1);
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_LightmapOnly(entity_render_t *ent, q3msurface_t *face)
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
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_Glow(entity_render_t *ent, q3msurface_t *face)
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
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_TextureVertex(entity_render_t *ent, q3msurface_t *face)
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
		m.pointer_color = face->data_color4f;
	}
	else if (mul == 1)
		m.pointer_color = face->data_color4f;
	else
	{
		for (i = 0;i < face->num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->data_color4f[i*4+0] * mul;
			varray_color4f[i*4+1] = face->data_color4f[i*4+1] * mul;
			varray_color4f[i*4+2] = face->data_color4f[i*4+2] * mul;
			varray_color4f[i*4+3] = face->data_color4f[i*4+3];
		}
		m.pointer_color = varray_color4f;
	}
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_VertexOnly(entity_render_t *ent, q3msurface_t *face)
{
	int i;
	float mul;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	mul = 2.0f;
	if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value != 1)
		mul *= r_shadow_realtime_world_lightmaps.value;
	if (mul == 1)
		m.pointer_color = face->data_color4f;
	else
	{
		for (i = 0;i < face->num_vertices;i++)
		{
			varray_color4f[i*4+0] = face->data_color4f[i*4+0] * 2.0f;
			varray_color4f[i*4+1] = face->data_color4f[i*4+1] * 2.0f;
			varray_color4f[i*4+2] = face->data_color4f[i*4+2] * 2.0f;
			varray_color4f[i*4+3] = face->data_color4f[i*4+3];
		}
		m.pointer_color = varray_color4f;
	}
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_OpaqueWall_Pass_AddTextureAmbient(entity_render_t *ent, q3msurface_t *face)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	GL_Color(r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), r_ambient.value * (1.0f / 128.0f), 1);
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
}

void R_Q3BSP_DrawFace_TransparentCallback(const void *voident, int facenumber)
{
	const entity_render_t *ent = voident;
	q3msurface_t *face = ent->model->brushq3.data_faces + facenumber;
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
				m.pointer_color = face->data_color4f;
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
				m.pointer_color = varray_color4f;
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
		m.pointer_color = varray_color4f;
	}
	m.pointer_vertex = face->data_vertex3f;
	R_Mesh_State(&m);
	qglDisable(GL_CULL_FACE);
	GL_LockArrays(0, face->num_vertices);
	R_Mesh_Draw(face->num_vertices, face->num_triangles, face->data_element3i);
	GL_LockArrays(0, 0);
	qglEnable(GL_CULL_FACE);
}

void R_Q3BSP_DrawFace(entity_render_t *ent, q3msurface_t *face)
{
	if (!face->num_triangles)
		return;
	if (face->texture->surfaceflags && (face->texture->surfaceflags & (Q3SURFACEFLAG_SKY | Q3SURFACEFLAG_NODRAW)))
		return;
	c_faces++;
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

void R_Q3BSP_RecursiveWorldNode(q3mnode_t *node)
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
		R_Q3BSP_RecursiveWorldNode(node->children[0]);
		node = node->children[1];
	}
	leaf = (q3mleaf_t *)node;
	if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex))
	{
		c_leafs++;
		for (i = 0;i < leaf->numleaffaces;i++)
			leaf->firstleafface[i]->visframe = r_framecount;
	}
}

// FIXME: num_leafs needs to be recalculated at load time to include only
// node-referenced leafs, as some maps are incorrectly compiled with leafs for
// the submodels (which would render the submodels occasionally, as part of
// the world - not good)
void R_Q3BSP_MarkLeafPVS(void)
{
	int i, j;
	q3mleaf_t *leaf;
	for (j = 0, leaf = cl.worldmodel->brushq3.data_leafs;j < cl.worldmodel->brushq3.num_leafs;j++, leaf++)
	{
		if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex))
		{
			c_leafs++;
			for (i = 0;i < leaf->numleaffaces;i++)
				leaf->firstleafface[i]->visframe = r_framecount;
		}
	}
}

static int r_q3bsp_framecount = -1;

void R_Q3BSP_DrawSky(entity_render_t *ent)
{
	int i;
	q3msurface_t *face;
	vec3_t modelorg;
	model_t *model;
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	if (r_drawcollisionbrushes.integer < 2)
	{
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		if (ent == &cl_entities[0].render)
		{
			if (r_q3bsp_framecount != r_framecount)
			{
				r_q3bsp_framecount = r_framecount;
				R_Q3BSP_RecursiveWorldNode(model->brushq3.data_nodes);
				//R_Q3BSP_MarkLeafPVS();
			}
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if (face->visframe == r_framecount && (face->texture->surfaceflags & Q3SURFACEFLAG_SKY) && !R_CullBox(face->mins, face->maxs))
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
	q3msurface_t *face;
	vec3_t modelorg;
	model_t *model;
	qbyte *pvs;
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	if (r_drawcollisionbrushes.integer < 2)
	{
		Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
		if (ent == &cl_entities[0].render)
		{
			if (model->brush.num_pvsclusters && !r_novis.integer && (pvs = model->brush.GetPVS(model, modelorg)))
			if (r_q3bsp_framecount != r_framecount)
			{
				r_q3bsp_framecount = r_framecount;
				R_Q3BSP_RecursiveWorldNode(model->brushq3.data_nodes);
				//R_Q3BSP_MarkLeafPVS();
			}
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				if (face->visframe == r_framecount && !R_CullBox(face->mins, face->maxs))
					R_Q3BSP_DrawFace(ent, face);
		}
		else
			for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
				R_Q3BSP_DrawFace(ent, face);
	}
	if (r_drawcollisionbrushes.integer >= 1)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		qglPolygonOffset(r_drawcollisionbrushes_polygonfactor.value, r_drawcollisionbrushes_polygonoffset.value);
		for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
			if (model->brushq3.data_thismodel->firstbrush[i].colbrushf && model->brushq3.data_thismodel->firstbrush[i].colbrushf->numtriangles)
				R_DrawCollisionBrush(model->brushq3.data_thismodel->firstbrush[i].colbrushf);
		for (i = 0, face = model->brushq3.data_thismodel->firstface;i < model->brushq3.data_thismodel->numfaces;i++, face++)
			if (face->num_collisiontriangles)
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
			if (!CHECKPVSBIT(outclusterpvs, leaf->clusterindex))
			{
				SETPVSBIT(outclusterpvs, leaf->clusterindex);
				outclusterlist[outnumclusters++] = leaf->clusterindex;
			}
			for (marksurfaceindex = 0;marksurfaceindex < leaf->numleaffaces;marksurfaceindex++)
			{
				surface = leaf->firstleafface[marksurfaceindex];
				surfaceindex = surface - model->brushq3.data_faces;
				if (!CHECKPVSBIT(outsurfacepvs, surfaceindex))
				{
					if (BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs) && !(surface->texture->surfaceparms & Q3SURFACEPARM_TRANS) && !(surface->texture->surfaceflags & (Q3SURFACEFLAG_SKY | Q3SURFACEFLAG_NODRAW)))
					{
						for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->num_triangles;triangleindex++, t++, e += 3)
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
	int surfacelistindex, j, t;
	const int *e;
	const float *v[3];
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
			if (!(surface->texture->surfaceflags & Q3SURFACEFLAG_NODRAW) && surface->num_triangles && !surface->texture->skin.fog)
			{
				for (j = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;j < surface->num_triangles;j++, t++, e += 3)
				{
					v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
					v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
					v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
					if (PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]) && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
						shadowmarklist[numshadowmark++] = t;
				}
			}
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + r_shadow_projectdistance.value, numshadowmark, shadowmarklist);
	}
}

void R_Q3BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, int numsurfaces, const int *surfacelist)
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
				Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_light, surface->texture->skin.base, surface->texture->skin.gloss, surface->texture->skin.nmap, surface->data_vertex3f, surface->data_svector3f, surface->data_tvector3f, surface->data_normal3f, surface->data_texcoordtexture2f, surface->num_triangles, surface->data_element3i);
			}
			else if ((ent != &cl_entities[0].render || surface->visframe == r_framecount) && !(surface->texture->surfaceflags & Q3SURFACEFLAG_NODRAW) && surface->num_triangles)
				R_Shadow_RenderLighting(surface->num_vertices, surface->num_triangles, surface->data_element3i, surface->data_vertex3f, surface->data_svector3f, surface->data_tvector3f, surface->data_normal3f, surface->data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, surface->texture->skin.base, surface->texture->skin.nmap, surface->texture->skin.gloss, lightcubemap, LIGHTING_DIFFUSE | LIGHTING_SPECULAR);
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

