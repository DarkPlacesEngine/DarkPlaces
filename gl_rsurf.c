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

#define MAX_LIGHTMAP_SIZE 256

static unsigned int intblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting
static float floatblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting

static qbyte templight[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*4];

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_vertexsurfaces = {0, "r_vertexsurfaces", "0"};
cvar_t r_dlightmap = {CVAR_SAVE, "r_dlightmap", "1"};
//cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_floatbuildlightmap = {0, "r_floatbuildlightmap", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "0"};
cvar_t r_cullsurface = {0, "r_cullsurface", "0"};

static int dlightdivtable[32768];

// variables used by R_PVSUpdate
int r_pvsframecount = 0;
mleaf_t *r_pvsviewleaf = NULL;
int r_pvsviewleafnovis = 0;
msurface_t *r_pvsfirstsurface = NULL;

static int R_IntAddDynamicLights (const matrix4x4_t *matrix, msurface_t *surf)
{
	int sdtable[256], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, red, green, blue, lit, dist2, impacts, impactt, subtract;
	unsigned int *bl;
	float dist, impact[3], local[3];

	// LordHavoc: use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64     k;
#else
	long long   k;
#endif

	lit = false;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	smax3 = smax * 3;

	for (lnum = 0; lnum < r_numdlights; lnum++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		Matrix4x4_Transform(matrix, r_dlight[lnum].origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) r_dlight[lnum].cullradius2 + LIGHTOFFSET;

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
		red = r_dlight[lnum].light[0];
		green = r_dlight[lnum].light[1];
		blue = r_dlight[lnum].light[2];
		subtract = (int) (r_dlight[lnum].subtract * 4194304.0f);
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
							bl[0] += (red   * k) >> 7;
							bl[1] += (green * k) >> 7;
							bl[2] += (blue  * k) >> 7;
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

	lit = false;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	smax3 = smax * 3;

	for (lnum = 0; lnum < r_numdlights; lnum++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		Matrix4x4_Transform(matrix, r_dlight[lnum].origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) r_dlight[lnum].cullradius2 + LIGHTOFFSET;

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
		red = r_dlight[lnum].light[0];
		green = r_dlight[lnum].light[1];
		blue = r_dlight[lnum].light[2];
		subtract = r_dlight[lnum].subtract * 32768.0f;
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
static void R_BuildLightMap (const entity_render_t *ent, msurface_t *surf, int dlightchanged)
{
	if (!r_floatbuildlightmap.integer)
	{
		int smax, tmax, i, j, size, size3, shift, maps, stride, l;
		unsigned int *bl, scale;
		qbyte *lightmap, *out, *stain;

		// update cached lighting info
		surf->cached_dlight = 0;
		surf->cached_lightscalebit = lightscalebit;
		surf->cached_ambient = r_ambient.value;
		surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
		surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
		surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
		surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		size = smax*tmax;
		size3 = size*3;
		lightmap = surf->samples;

	// set to full bright if no light data
		bl = intblocklights;
		if ((ent->effects & EF_FULLBRIGHT) || !ent->model->lightdata)
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

			if (surf->dlightframe == r_framecount && r_dlightmap.integer)
			{
				surf->cached_dlight = R_IntAddDynamicLights(&ent->inversematrix, surf);
				if (surf->cached_dlight)
					c_light_polys++;
				else if (dlightchanged)
					return; // don't upload if only updating dlights and none mattered
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
		// deal with lightmap brightness scale
		shift = 7 + lightscalebit + 8;
		if (ent->model->lightmaprgba)
		{
			stride = (surf->lightmaptexturestride - smax) * 4;
			for (i = 0;i < tmax;i++, out += stride)
			{
				for (j = 0;j < smax;j++)
				{
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
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
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
					l = (*bl++ * *stain++) >> shift;*out++ = min(l, 255);
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
		surf->cached_lightscalebit = lightscalebit;
		surf->cached_ambient = r_ambient.value;
		surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
		surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
		surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
		surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		size = smax*tmax;
		size3 = size*3;
		lightmap = surf->samples;

	// set to full bright if no light data
		bl = floatblocklights;
		if ((ent->effects & EF_FULLBRIGHT) || !ent->model->lightdata)
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

		if (surf->dlightframe == r_framecount && r_dlightmap.integer)
		{
			surf->cached_dlight = R_FloatAddDynamicLights(&ent->inversematrix, surf);
			if (surf->cached_dlight)
				c_light_polys++;
			else if (dlightchanged)
				return; // don't upload if only updating dlights and none mattered
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
		// deal with lightmap brightness scale
		scale = 1.0f / (1 << (7 + lightscalebit + 8));
		if (ent->model->lightmaprgba)
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

void R_StainNode (mnode_t *node, model_t *model, vec3_t origin, float radius, int icolor[8])
{
	float ndist;
	msurface_t *surf, *endsurf;
	int sdtable[256], td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, dist2, impacts, impactt, subtract, a, stained, cr, cg, cb, ca, ratio;
	qbyte *bl;
	vec3_t impact;
	// LordHavoc: use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64     k;
#else
	long long   k;
#endif


	// for comparisons to minimum acceptable light
	// compensate for 256 offset
	maxdist = radius * radius + 256.0f;

	// clamp radius to avoid exceeding 32768 entry division table
	if (maxdist > 4194304)
		maxdist = 4194304;

	subtract = (int) ((1.0f / maxdist) * 4194304.0f);

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

	dist2 = ndist * ndist + 256.0f;
	if (dist2 < maxdist)
	{
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

		for (surf = model->surfaces + node->firstsurface, endsurf = surf + node->numsurfaces;surf < endsurf;surf++)
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

				// convert to 8.8 blocklights format
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
								k = dlightdivtable[(sdtable[s] + td) >> 7] - subtract;
								if (k > 0)
								{
									ratio = rand() & 255;
									ca = (((icolor[7] - icolor[3]) * ratio) >> 8) + icolor[3];
									a = (ca * k) >> 8;
									if (a > 0)
									{
										a = bound(0, a, 256);
										cr = (((icolor[4] - icolor[0]) * ratio) >> 8) + icolor[0];
										cg = (((icolor[5] - icolor[1]) * ratio) >> 8) + icolor[1];
										cb = (((icolor[6] - icolor[2]) * ratio) >> 8) + icolor[2];
										bl[0] = (qbyte) ((((cr - (int) bl[0]) * a) >> 8) + (int) bl[0]);
										bl[1] = (qbyte) ((((cg - (int) bl[1]) * a) >> 8) + (int) bl[1]);
										bl[2] = (qbyte) ((((cb - (int) bl[2]) * a) >> 8) + (int) bl[2]);
										stained = true;
									}
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
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_StainNode(node->children[0], model, origin, radius, icolor);
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

void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	int n, icolor[8];
	entity_render_t *ent;
	model_t *model;
	vec3_t org;
	icolor[0] = cr1;
	icolor[1] = cg1;
	icolor[2] = cb1;
	icolor[3] = ca1;
	icolor[4] = cr2;
	icolor[5] = cg2;
	icolor[6] = cb2;
	icolor[7] = ca2;

	model = cl.worldmodel;
	if (model)
		R_StainNode(model->nodes + model->hulls[0].firstclipnode, model, origin, radius, icolor);

	// look for embedded bmodels
	for (n = 0;n < cl_num_brushmodel_entities;n++)
	{
		ent = cl_brushmodel_entities[n];
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			Mod_CheckLoaded(model);
			if (model->type == mod_brush)
			{
				Matrix4x4_Transform(&ent->inversematrix, origin, org);
				R_StainNode(model->nodes + model->hulls[0].firstclipnode, model, org, radius, icolor);
			}
		}
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

static void RSurf_AddLightmapToVertexColors(const int *lightmapoffsets, float *c, int numverts, const qbyte *samples, int size3, const qbyte *styles)
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

static void RSurf_FogColors(const float *v, float *c, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	if (fogenabled)
	{
		for (i = 0;i < numverts;i++, v += 4, c += 4)
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

static void RSurf_FoggedColors(const float *v, float *c, float r, float g, float b, float a, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	r *= colorscale;
	g *= colorscale;
	b *= colorscale;
	if (fogenabled)
	{
		for (i = 0;i < numverts;i++, v += 4, c += 4)
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

static void RSurf_FogPassColors(const float *v, float *c, float r, float g, float b, float a, float colorscale, int numverts, const float *modelorg)
{
	int i;
	float diff[3], f;
	r *= colorscale;
	g *= colorscale;
	b *= colorscale;
	for (i = 0;i < numverts;i++, v += 4, c += 4)
	{
		VectorSubtract(v, modelorg, diff);
		f = exp(fogdensity/DotProduct(diff, diff));
		c[0] = r;
		c[1] = g;
		c[2] = b;
		c[3] = a * f;
	}
}

static void RSurf_ScaleColors(float *c, float scale, int numverts)
{
	int i;
	if (scale != 1)
		for (i = 0;i < numverts;i++, c += 4)
			VectorScale(c, scale, c);
}

static int RSurf_LightSeparate(const matrix4x4_t *matrix, const int *dlightbits, int numverts, const float *vert, float *color)
{
	float f;
	const float *v;
	float *c;
	int i, l, lit = false;
	const rdlight_t *rd;
	vec3_t lightorigin;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			rd = &r_dlight[l];
			Matrix4x4_Transform(matrix, rd->origin, lightorigin);
			for (i = 0, v = vert, c = color;i < numverts;i++, v += 4, c += 4)
			{
				f = VectorDistance2(v, lightorigin) + LIGHTOFFSET;
				if (f < rd->cullradius2)
				{
					f = (1.0f / f) - rd->subtract;
					VectorMA(c, f, rd->light, c);
					lit = true;
				}
			}
		}
	}
	return lit;
}

// note: this untransforms lights to do the checking,
// and takes surf->mesh->verts data
static int RSurf_LightCheck(const matrix4x4_t *matrix, const int *dlightbits, const surfmesh_t *mesh)
{
	int i, l;
	const rdlight_t *rd;
	vec3_t lightorigin;
	const float *v;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			rd = &r_dlight[l];
			Matrix4x4_Transform(matrix, rd->origin, lightorigin);
			for (i = 0, v = mesh->verts;i < mesh->numverts;i++, v += 4)
				if (VectorDistance2(v, lightorigin) < rd->cullradius2)
					return true;
		}
	}
	return false;
}

static void RSurfShader_Sky(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;

	// LordHavoc: HalfLife maps have freaky skypolys...
	if (ent->model->ishlbsp)
		return;

	if (skyrendernow)
	{
		skyrendernow = false;
		if (skyrendermasked)
			R_Sky();
	}

	R_Mesh_Matrix(&ent->matrix);

	// draw depth-only polys
	memset(&m, 0, sizeof(m));
	if (skyrendermasked)
	{
		m.blendfunc1 = GL_ZERO;
		m.blendfunc2 = GL_ONE;
	}
	else
	{
		// fog sky
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.wantoverbright = false;
	m.depthwrite = true;
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			if (skyrendermasked)
				memset(varray_color, 0, mesh->numverts * sizeof(float[4]));
			else
				R_FillColors(varray_color, mesh->numverts, fogcolor[0] * mesh_colorscale, fogcolor[1] * mesh_colorscale, fogcolor[2] * mesh_colorscale, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_Water_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->surfaces + calldata2;
	float f;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float alpha = ent->alpha * (surf->flags & SURF_DRAWNOALPHA ? 1 : r_wateralpha.value);
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);

	R_Mesh_Matrix(&ent->matrix);

	memset(&m, 0, sizeof(m));
	if (ent->effects & EF_ADDITIVE)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (surf->currenttexture->fogtexture != NULL || alpha < 1)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.wantoverbright = true;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	R_Mesh_State(&m);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
		memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
		f = surf->flags & SURF_DRAWFULLBRIGHT ? 1.0f : ((surf->flags & SURF_LIGHTMAP) ? 0 : 0.5f);
		R_FillColors(varray_color, mesh->numverts, f, f, f, alpha);
		if (!(surf->flags & SURF_DRAWFULLBRIGHT || ent->effects & EF_FULLBRIGHT))
		{
			if (surf->dlightframe == r_framecount)
				RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, mesh->numverts, varray_vertex, varray_color);
			if (surf->flags & SURF_LIGHTMAP)
				RSurf_AddLightmapToVertexColors(mesh->lightmapoffsets, varray_color, mesh->numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
		}
		RSurf_FogColors(varray_vertex, varray_color, mesh_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
	}

	if (fogenabled)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
		m.wantoverbright = false;
		m.tex[0] = R_GetTexture(surf->currenttexture->fogtexture);
		R_Mesh_State(&m);
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			if (m.tex[0])
				memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			RSurf_FogPassColors(varray_vertex, varray_color, fogcolor[0], fogcolor[1], fogcolor[2], alpha, mesh_colorscale, mesh->numverts, modelorg);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_Water(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	vec3_t center;
	if ((r_wateralpha.value < 1 && !(texture->flags & SURF_DRAWNOALPHA)) || ent->effects & EF_ADDITIVE || texture->fogtexture)
	{
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
		{
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Water_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
			RSurfShader_Water_Callback(ent, surf - ent->model->surfaces);
}

static void RSurfShader_Wall_Pass_BaseVertex(const entity_render_t *ent, const msurface_t *surf)
{
	float base;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	if (ent->effects & EF_ADDITIVE)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (surf->currenttexture->fogtexture != NULL || ent->alpha < 1)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.wantoverbright = true;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	base = ent->effects & EF_FULLBRIGHT ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	R_Mesh_State(&m);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
		memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
		R_FillColors(varray_color, mesh->numverts, base, base, base, ent->alpha);
		if (!(ent->effects & EF_FULLBRIGHT))
		{
			if (surf->dlightframe == r_framecount)
				RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, mesh->numverts, varray_vertex, varray_color);
			if (surf->flags & SURF_LIGHTMAP)
				RSurf_AddLightmapToVertexColors(mesh->lightmapoffsets, varray_color, mesh->numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
		}
		RSurf_FogColors(varray_vertex, varray_color, mesh_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
	}
}

static void RSurfShader_Wall_Pass_BaseFullbright(const entity_render_t *ent, const msurface_t *surf)
{
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	if (ent->effects & EF_ADDITIVE)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (surf->currenttexture->fogtexture != NULL || ent->alpha < 1)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.wantoverbright = false;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	R_Mesh_State(&m);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
		memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
		RSurf_FoggedColors(varray_vertex, varray_color, 1, 1, 1, ent->alpha, mesh_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
	}
}

static void RSurfShader_Wall_Pass_Glow(const entity_render_t *ent, const msurface_t *surf)
{
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.wantoverbright = false;
	m.tex[0] = R_GetTexture(surf->currenttexture->glowtexture);
	R_Mesh_State(&m);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
		memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
		RSurf_FoggedColors(varray_vertex, varray_color, 1, 1, 1, ent->alpha, mesh_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
	}
}

static void RSurfShader_Wall_Pass_Fog(const entity_render_t *ent, const msurface_t *surf)
{
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.wantoverbright = false;
	m.tex[0] = R_GetTexture(surf->currenttexture->fogtexture);
	R_Mesh_State(&m);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
		memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		if (m.tex[0])
			memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
		RSurf_FogPassColors(varray_vertex, varray_color, fogcolor[0], fogcolor[1], fogcolor[2], ent->alpha, mesh_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTripleTexCombine(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	//m.wantoverbright = false;
	m.tex[0] = R_GetTexture(texture->texture);
	m.tex[1] = R_GetTexture(texture->surfacechain->lightmaptexture);
	m.tex[2] = R_GetTexture(texture->detailtexture);
	m.texrgbscale[0] = 1;
	m.texrgbscale[1] = 4;
	m.texrgbscale[2] = 2;
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
		if (m.tex[1] != lightmaptexturenum)
		{
			m.tex[1] = lightmaptexturenum;
			R_Mesh_State(&m);
		}
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			memcpy(varray_texcoord[1], mesh->uv, mesh->numverts * sizeof(float[2]));
			memcpy(varray_texcoord[2], mesh->ab, mesh->numverts * sizeof(float[2]));
			cl = (float) (1 << lightscalebit) * mesh_colorscale;
			R_FillColors(varray_color, mesh->numverts, cl, cl, cl, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDoubleTex(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.wantoverbright = true;
	m.tex[0] = R_GetTexture(texture->texture);
	m.tex[1] = R_GetTexture(texture->surfacechain->lightmaptexture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
		if (m.tex[1] != lightmaptexturenum)
		{
			m.tex[1] = lightmaptexturenum;
			R_Mesh_State(&m);
		}
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			memcpy(varray_texcoord[1], mesh->uv, mesh->numverts * sizeof(float[2]));
			cl = (float) (1 << lightscalebit) * mesh_colorscale;
			R_FillColors(varray_color, mesh->numverts, cl, cl, cl, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTexture(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	//m.wantoverbright = false;
	m.tex[0] = R_GetTexture(texture->texture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			R_FillColors(varray_color, mesh->numverts, 1, 1, 1, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseLightmap(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ZERO;
	m.blendfunc2 = GL_SRC_COLOR;
	m.wantoverbright = true;
	m.tex[0] = R_GetTexture(texture->surfacechain->lightmaptexture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		lightmaptexturenum = R_GetTexture(surf->lightmaptexture);
		if (m.tex[0] != lightmaptexturenum)
		{
			m.tex[0] = lightmaptexturenum;
			R_Mesh_State(&m);
		}
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->uv, mesh->numverts * sizeof(float[2]));
			cl = (float) (1 << lightscalebit) * mesh_colorscale;
			R_FillColors(varray_color, mesh->numverts, cl, cl, cl, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Light(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.wantoverbright = true;
	m.tex[0] = R_GetTexture(texture->texture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		if (surf->dlightframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				if (RSurf_LightCheck(&ent->inversematrix, surf->dlightbits, mesh))
				{
					R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
					memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
					memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
					memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
					R_FillColors(varray_color, mesh->numverts, 0, 0, 0, 1);
					RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, mesh->numverts, varray_vertex, varray_color);
					RSurf_ScaleColors(varray_color, mesh_colorscale, mesh->numverts);
					R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
				}
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Fog(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	//m.wantoverbright = false;
	//m.tex[0] = 0;
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			if (m.tex[0])
				memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			RSurf_FogPassColors(varray_vertex, varray_color, fogcolor[0], fogcolor[1], fogcolor[2], 1, mesh_colorscale, mesh->numverts, modelorg);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDetail(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_DST_COLOR;
	m.blendfunc2 = GL_SRC_COLOR;
	//m.wantoverbright = false;
	m.tex[0] = R_GetTexture(texture->detailtexture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->ab, mesh->numverts * sizeof(float[2]));
			R_FillColors(varray_color, mesh->numverts, 1, 1, 1, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Glow(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	//m.wantoverbright = false;
	m.tex[0] = R_GetTexture(texture->glowtexture);
	R_Mesh_State(&m);
	for (surf = texture->surfacechain;surf;surf = surf->texturechain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			R_Mesh_ResizeCheck(mesh->numverts, mesh->numtriangles);
			memcpy(varray_element, mesh->index, mesh->numtriangles * sizeof(int[3]));
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			memcpy(varray_texcoord[0], mesh->st, mesh->numverts * sizeof(float[2]));
			R_FillColors(varray_color, mesh->numverts, mesh_colorscale, mesh_colorscale, mesh_colorscale, 1);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles);
		}
	}
}

static void RSurfShader_Wall_Fullbright_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->surfaces + calldata2;
	R_Mesh_Matrix(&ent->matrix);
	RSurfShader_Wall_Pass_BaseFullbright(ent, surf);
	if (surf->currenttexture->glowtexture)
		RSurfShader_Wall_Pass_Glow(ent, surf);
	if (fogenabled)
		RSurfShader_Wall_Pass_Fog(ent, surf);
}

static void RSurfShader_Wall_Fullbright(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	vec3_t center;
	if (ent->effects & EF_ADDITIVE || ent->alpha < 1 || texture->fogtexture != NULL)
	{
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
		{
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Fullbright_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else
	{
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
			RSurfShader_Wall_Pass_BaseFullbright(ent, surf);
		if (texture->glowtexture)
			for (surf = texture->surfacechain;surf;surf = surf->texturechain)
				RSurfShader_Wall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = texture->surfacechain;surf;surf = surf->texturechain)
				RSurfShader_Wall_Pass_Fog(ent, surf);
	}
}

static void RSurfShader_Wall_Vertex_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->surfaces + calldata2;
	R_Mesh_Matrix(&ent->matrix);
	RSurfShader_Wall_Pass_BaseVertex(ent, surf);
	if (surf->currenttexture->glowtexture)
		RSurfShader_Wall_Pass_Glow(ent, surf);
	if (fogenabled)
		RSurfShader_Wall_Pass_Fog(ent, surf);
}

static void RSurfShader_Wall_Lightmap(const entity_render_t *ent, const texture_t *texture)
{
	const msurface_t *surf;
	vec3_t center;
	if (ent->effects & EF_ADDITIVE || ent->alpha < 1 || texture->fogtexture != NULL)
	{
		// transparent vertex shaded from lightmap
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
		{
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else if (r_vertexsurfaces.integer)
	{
		// opaque vertex shaded from lightmap
		for (surf = texture->surfacechain;surf;surf = surf->texturechain)
			RSurfShader_Wall_Pass_BaseVertex(ent, surf);
		if (texture->glowtexture)
			for (surf = texture->surfacechain;surf;surf = surf->texturechain)
				RSurfShader_Wall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = texture->surfacechain;surf;surf = surf->texturechain)
				RSurfShader_Wall_Pass_Fog(ent, surf);
	}
	else
	{
		// opaque lightmapped
		if (r_textureunits.integer >= 2)
		{
			if (r_textureunits.integer >= 3 && gl_combine.integer && r_detailtextures.integer)
				RSurfShader_OpaqueWall_Pass_BaseTripleTexCombine(ent, texture);
			else
			{
				RSurfShader_OpaqueWall_Pass_BaseDoubleTex(ent, texture);
				if (r_detailtextures.integer)
					RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture);
			}
		}
		else
		{
			RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture);
			RSurfShader_OpaqueWall_Pass_BaseLightmap(ent, texture);
			if (r_detailtextures.integer)
				RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture);
		}
		if (!r_dlightmap.integer && !(ent->effects & EF_FULLBRIGHT))
			RSurfShader_OpaqueWall_Pass_Light(ent, texture);
		if (texture->glowtexture)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture);
	}
}

Cshader_t Cshader_wall_lightmap = {{NULL, RSurfShader_Wall_Lightmap}, SHADERFLAGS_NEEDLIGHTMAP};
Cshader_t Cshader_wall_fullbright = {{NULL, RSurfShader_Wall_Fullbright}, 0};
Cshader_t Cshader_water = {{NULL, RSurfShader_Water}, 0};
Cshader_t Cshader_sky = {{RSurfShader_Sky, NULL}, 0};

int Cshader_count = 4;
Cshader_t *Cshaders[4] =
{
	&Cshader_wall_lightmap,
	&Cshader_wall_fullbright,
	&Cshader_water,
	&Cshader_sky
};

void R_DrawSurfaces(entity_render_t *ent, int sky, int normal)
{
	int i, alttextures, texframe, framecount, numtextures, numsurfaces;
	texture_t *t, *textures;
	model_t *model;
	msurface_t *surf, *surfaces;
	vec3_t modelorg;

	if (!ent->model)
		return;

	R_Mesh_Matrix(&ent->matrix);

	model = ent->model;
	alttextures = ent->frame != 0;
	texframe = (int)(cl.time * 5.0f);

	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);

	textures = model->textures;
	numtextures = model->numtextures;
	surfaces = model->surfaces + model->firstmodelsurface;
	numsurfaces = model->nummodelsurfaces;

	for (i = 0;i < numtextures;i++)
		textures[i].surfacechain = NULL;

	for (i = 0, surf = surfaces;i < numsurfaces;i++, surf++)
	{
		if (surf->visframe == r_framecount)
		{
			// mark any backface surfaces as not visible
			if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
			{
				if (!(surf->flags & SURF_PLANEBACK))
					surf->visframe = -1;
			}
			else
			{
				if (surf->flags & SURF_PLANEBACK)
					surf->visframe = -1;
			}
			if (surf->visframe == r_framecount)
			{
				if (r_cullsurface.integer && R_CullBox (surf->poly_mins, surf->poly_maxs))
					surf->visframe = -1;
				else
				{
					c_faces++;
					t = surf->texinfo->texture;
					if (t->animated)
					{
						framecount = t->anim_total[alttextures];
						if (framecount >= 2)
							t = t->anim_frames[alttextures][texframe % framecount];
						else
							t = t->anim_frames[alttextures][0];
					}
					surf->currenttexture = t;
					surf->texturechain = t->surfacechain;
					t->surfacechain = surf;
				}
			}
		}
	}

	if (sky)
		for (i = 0, t = textures;i < numtextures;i++, t++)
			if (t->surfacechain && t->shader->shaderfunc[SHADERSTAGE_SKY])
				t->shader->shaderfunc[SHADERSTAGE_SKY](ent, t);

	if (normal)
	{
		if (r_dynamic.integer)
			R_MarkLights(ent);

		if (!r_vertexsurfaces.integer)
		{
			for (i = 0, surf = surfaces;i < numsurfaces;i++, surf++)
			{
				if (surf->visframe == r_framecount && surf->lightmaptexture != NULL)
				{
					if (surf->cached_dlight
					 || surf->cached_ambient != r_ambient.value
					 || surf->cached_lightscalebit != lightscalebit)
						R_BuildLightMap(ent, surf, false); // base lighting changed
					else if (r_dynamic.integer)
					{
						if  (surf->styles[0] != 255 && (d_lightstylevalue[surf->styles[0]] != surf->cached_light[0]
						 || (surf->styles[1] != 255 && (d_lightstylevalue[surf->styles[1]] != surf->cached_light[1]
						 || (surf->styles[2] != 255 && (d_lightstylevalue[surf->styles[2]] != surf->cached_light[2]
						 || (surf->styles[3] != 255 && (d_lightstylevalue[surf->styles[3]] != surf->cached_light[3]))))))))
							R_BuildLightMap(ent, surf, false); // base lighting changed
						else if (surf->dlightframe == r_framecount && r_dlightmap.integer)
							R_BuildLightMap(ent, surf, true); // only dlights
					}
				}
			}
		}

		for (i = 0, t = textures;i < numtextures;i++, t++)
			if (t->surfacechain && t->shader->shaderfunc[SHADERSTAGE_NORMAL])
				t->shader->shaderfunc[SHADERSTAGE_NORMAL](ent, t);
	}
}

/*
static void R_DrawPortal_Callback(const void *calldata1, int calldata2)
{
	int i;
	float *v;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	const mportal_t *portal = ent->model->portals + calldata2;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.wantoverbright = false;
	R_Mesh_Matrix(&ent->matrix);
	R_Mesh_State(&m);
	R_Mesh_ResizeCheck(portal->numpoints, portal->numpoints - 2);
	for (i = 0;i < mesh->numtriangles;i++)
	{
		varray_element[i * 3 + 0] = 0;
		varray_element[i * 3 + 1] = i + 1;
		varray_element[i * 3 + 2] = i + 2;
	}
	i = portal - ent->model->portals;
	R_FillColors(varray_color, mesh->numverts,
		((i & 0x0007) >> 0) * (1.0f / 7.0f) * mesh_colorscale,
		((i & 0x0038) >> 3) * (1.0f / 7.0f) * mesh_colorscale,
		((i & 0x01C0) >> 6) * (1.0f / 7.0f) * mesh_colorscale,
		0.125f);
	if (PlaneDiff(r_origin, (&portal->plane)) > 0)
	{
		for (i = portal->numpoints - 1, v = varray_vertex;i >= 0;i--, v += 4)
			VectorCopy(portal->points[i].position, v);
	}
	else
		for (i = 0, v = varray_vertex;i < portal->numpoints;i++, v += 4)
			VectorCopy(portal->points[i].position, v);
	R_Mesh_Draw(portal->numpoints, portal->numpoints - 2);
}

static void R_DrawPortals(entity_render_t *ent)
{
	int i;
	mportal_t *portal, *endportal;
	float temp[3], center[3], f;

	if (r_drawportals.integer < 1)
		return;

	for (portal = ent->model->portals, endportal = portal + ent->model->numportals;portal < endportal;portal++)
	{
		if (portal->here->visframe == r_framecount || portal->past->visframe == r_framecount)
		{
			VectorClear(temp);
			for (i = 0;i < portal->numpoints;i++)
				VectorAdd(temp, portal->points[i].position, temp);
			f = ixtable[portal->numpoints];
			VectorScale(temp, f, temp);
			Matrix4x4_Transform(&ent->matrix, temp, center);
			R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, ent, portal - ent->model->portals);
		}
	}
}
*/

void R_DrawBrushModel(entity_render_t *ent, int sky, int normal)
{
	int i;
	msurface_t *surf;
	model_t *model;
	vec3_t modelorg;

	// because bmodels can be reused, we have to decide which things to render
	// from scratch every time
	model = ent->model;
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surf = model->surfaces + model->firstmodelsurface + i;
		surf->visframe = r_framecount;
		surf->pvsframe = -1;
		surf->worldnodeframe = -1;
		surf->lightframe = -1;
		surf->dlightframe = -1;
	}
	R_DrawSurfaces(ent, sky, normal);
}

void R_SurfaceWorldNode (void)
{
	msurface_t *surf;
	for (surf = r_pvsfirstsurface;surf;surf = surf->pvschain)
		surf->visframe = r_framecount;
}

/*
static void R_PortalWorldNode(entity_render_t *ent, mleaf_t *viewleaf)
{
	int portalstack, i;
	mportal_t *p, *pstack[8192];
	msurface_t *surf, **mark, **endmark;
	mleaf_t *leaf;
	// LordHavoc: portal-passage worldnode with PVS;
	// follows portals leading outward from viewleaf, does not venture
	// offscreen or into leafs that are not visible, faster than Quake's
	// RecursiveWorldNode
	leaf = viewleaf;
	leaf->worldnodeframe = r_framecount;
	portalstack = 0;
loc0:
	c_leafs++;
	if (leaf->nummarksurfaces)
	{
		for (c = leaf->nummarksurfaces, mark = leaf->firstmarksurface;c;c--)
		{
			surf = *mark++;
			// make sure surfaces are only processed once
			if (surf->worldnodeframe != r_framecount)
			{
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(r_origin, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
						surf->visframe = r_framecount;
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						surf->visframe = r_framecount;
				}
			}
		}
	}
	// follow portals into other leafs
	for (p = leaf->portals;p;p = p->next)
	{
		leaf = p->past;
		if (leaf->worldnodeframe != r_framecount)
		{
			leaf->worldnodeframe = r_framecount;
			// FIXME: R_NotCulledBox is absolute, should be done relative
			if (leaf->pvsframe == r_pvsframecount && R_NotCulledBox(leaf->mins, leaf->maxs))
			{
				p->visframe = r_framecount;
				pstack[portalstack++] = p;
				goto loc0;
loc1:
				p = pstack[--portalstack];
			}
		}
	}
	if (portalstack)
		goto loc1;
}
*/

static void R_PortalWorldNode(entity_render_t *ent, mleaf_t *viewleaf)
{
	int c, leafstackpos;
	mleaf_t *leaf, *leafstack[8192];
	mportal_t *p;
	msurface_t **mark;
	vec3_t modelorg;
	// LordHavoc: portal-passage worldnode with PVS;
	// follows portals leading outward from viewleaf, does not venture
	// offscreen or into leafs that are not visible, faster than Quake's
	// RecursiveWorldNode
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	viewleaf->worldnodeframe = r_framecount;
	leafstack[0] = viewleaf;
	leafstackpos = 1;
	while (leafstackpos)
	{
		c_leafs++;
		leaf = leafstack[--leafstackpos];
		// only useful for drawing portals
		//leaf->visframe = r_framecount;
		// draw any surfaces bounding this leaf
		if (leaf->nummarksurfaces)
			for (c = leaf->nummarksurfaces, mark = leaf->firstmarksurface;c;c--)
				(*mark++)->visframe = r_framecount;
		// follow portals into other leafs
		for (p = leaf->portals;p;p = p->next)
		{
			leaf = p->past;
			if (leaf->worldnodeframe != r_framecount)
			{
				leaf->worldnodeframe = r_framecount;
				// FIXME: R_NotCulledBox is absolute, should be done relative
				if (leaf->pvsframe == r_pvsframecount && R_NotCulledBox(leaf->mins, leaf->maxs))
					leafstack[leafstackpos++] = leaf;
			}
		}
	}
	//if (r_drawportals.integer)
	//	R_DrawPortals(ent);
}


void R_PVSUpdate (mleaf_t *viewleaf)
{
	int i, j, l, c, bits;
	mleaf_t *leaf;
	qbyte *vis;
	msurface_t **mark, *surf;

	if (r_pvsviewleaf == viewleaf && r_pvsviewleafnovis == r_novis.integer)
		return;

	r_pvsframecount++;
	r_pvsviewleaf = viewleaf;
	r_pvsviewleafnovis = r_novis.integer;

	if (viewleaf)
	{
		vis = Mod_LeafPVS (viewleaf, cl.worldmodel);
		for (j = 0;j < cl.worldmodel->numleafs;j += 8)
		{
			bits = *vis++;
			if (bits)
			{
				l = cl.worldmodel->numleafs - j;
				if (l > 8)
					l = 8;
				for (i = 0;i < l;i++)
				{
					if (bits & (1 << i))
					{
						leaf = &cl.worldmodel->leafs[j + i + 1];
						leaf->pvsframe = r_pvsframecount;
						// mark surfaces bounding this leaf as visible
						for (c = leaf->nummarksurfaces, mark = leaf->firstmarksurface;c;c--)
							(*mark++)->pvsframe = r_pvsframecount;
					}
				}
			}
		}
		// build pvs surfacechain
		r_pvsfirstsurface = NULL;
		mark = &r_pvsfirstsurface;
		for (c = cl.worldmodel->nummodelsurfaces, surf = cl.worldmodel->surfaces + cl.worldmodel->firstmodelsurface;c;c--, surf++)
		{
			if (surf->pvsframe == r_pvsframecount)
			{
				*mark = surf;
				mark = &surf->pvschain;
			}
		}
		*mark = NULL;
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (entity_render_t *ent)
{
	mleaf_t *viewleaf;
	viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	R_PVSUpdate(viewleaf);
	if (!viewleaf)
		return;
	if (r_surfaceworldnode.integer || viewleaf->contents == CONTENTS_SOLID)
		R_SurfaceWorldNode ();
	else
		R_PortalWorldNode (ent, viewleaf);
	R_DrawSurfaces(ent, true, true);
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelSky (entity_render_t *ent)
{
	R_DrawBrushModel(ent, true, false);
}

void R_DrawBrushModelNormal (entity_render_t *ent)
{
	c_bmodels++;
	R_DrawBrushModel(ent, false, true);
}

static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
}

static void gl_surf_newmap(void)
{
	// reset pvs visibility variables so it will update on first frame
	r_pvsframecount = 1;
	r_pvsviewleaf = NULL;
	r_pvsviewleafnovis = false;
	r_pvsfirstsurface = NULL;
}

void GL_Surf_Init(void)
{
	int i;
	dlightdivtable[0] = 4194304;
	for (i = 1;i < 32768;i++)
		dlightdivtable[i] = 4194304 / (i << 7);

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_vertexsurfaces);
	Cvar_RegisterVariable(&r_dlightmap);
	//Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);
	Cvar_RegisterVariable(&r_floatbuildlightmap);
	Cvar_RegisterVariable(&r_detailtextures);
	Cvar_RegisterVariable(&r_surfaceworldnode);
	Cvar_RegisterVariable(&r_cullsurface);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

