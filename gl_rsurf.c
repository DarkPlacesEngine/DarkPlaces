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
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_floatbuildlightmap = {0, "r_floatbuildlightmap", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};

static int dlightdivtable[32768];

static int R_IntAddDynamicLights (msurface_t *surf)
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

		softwareuntransform(r_dlight[lnum].origin, local);
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

static int R_FloatAddDynamicLights (msurface_t *surf)
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

		softwareuntransform(r_dlight[lnum].origin, local);
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
				surf->cached_dlight = R_IntAddDynamicLights(surf);
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
			surf->cached_dlight = R_FloatAddDynamicLights(surf);
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

static void RSurf_CopyXYZ(const surfvertex_t *in, float *out, int numverts)
{
	int i;
	for (i = 0;i < numverts;i++, in++, out += 4)
	{
		VectorCopy(in->v, out);
		out[3] = 1;
	}
}

static void RSurf_CopyST(const surfvertex_t *in, float *out, int numverts)
{
	int i;
	for (i = 0;i < numverts;i++, in++, out += 2)
	{
		out[0] = in->st[0];
		out[1] = in->st[1];
	}
}

static void RSurf_CopyUV(const surfvertex_t *in, float *out, int numverts)
{
	int i;
	for (i = 0;i < numverts;i++, in++, out += 2)
	{
		out[0] = in->uv[0];
		out[1] = in->uv[1];
	}
}

static void RSurf_CopyAB(const surfvertex_t *in, float *out, int numverts)
{
	int i;
	for (i = 0;i < numverts;i++, in++, out += 2)
	{
		out[0] = in->ab[0];
		out[1] = in->ab[1];
	}
}

static void RSurf_AddLightmapToVertexColors(const surfvertex_t *in, float *c, int numverts, const qbyte *samples, int size3, const qbyte *styles)
{
	int i;
	float scale;
	const qbyte *lm;
	if (styles[0] != 255)
	{
		for (i = 0;i < numverts;i++, in++, c += 4)
		{
			lm = samples + in->lightmapoffset;
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
	rdlight_t *rd;
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
// and takes surf->mesh->vertex data
static int RSurf_LightCheck(const int *dlightbits, surfmesh_t *mesh)
{
	int i, l;
	rdlight_t *rd;
	vec3_t lightorigin;
	surfvertex_t *sv;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			rd = &r_dlight[l];
			softwareuntransform(rd->origin, lightorigin);
			for (i = 0, sv = mesh->vertex;i < mesh->numverts;i++, sv++)
				if (VectorDistance2(sv->v, lightorigin) < rd->cullradius2)
					return true;
		}
	}
	return false;
}

static void RSurfShader_Sky(const entity_render_t *ent, const msurface_t *firstsurf)
{
	const msurface_t *surf;
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;

	// LordHavoc: HalfLife maps have freaky skypolys...
	if (ent->model->ishlbsp)
		return;

	if (skyrendernow)
	{
		skyrendernow = false;
		if (skyrendermasked)
			R_Sky();
	}

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
	m.depthwrite = true;
	m.matrix = ent->matrix;
	for (surf = firstsurf;surf;surf = surf->chain)
	{
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			m.numtriangles = mesh->numtriangles;
			m.numverts = mesh->numverts;
			if (R_Mesh_Draw_GetBuffer(&m, false))
			{
				memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
				RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
				if (skyrendermasked)
					memset(m.color, 0, m.numverts * sizeof(float[4]));
				else
					R_FillColors(m.color, m.numverts, fogcolor[0] * m.colorscale, fogcolor[1] * m.colorscale, fogcolor[2] * m.colorscale, 1);
				R_Mesh_Render();
			}
		}
	}
}

static void RSurfShader_Water_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	msurface_t *surf = ent->model->surfaces + calldata2;
	float f;
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float alpha = ent->alpha * (surf->flags & SURF_DRAWNOALPHA ? 1 : r_wateralpha.value);
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);

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
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, true))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			f = surf->flags & SURF_DRAWFULLBRIGHT ? 1.0f : ((surf->flags & SURF_LIGHTMAP) ? 0 : 0.5f);
			R_FillColors(m.color, m.numverts, f, f, f, alpha);
			if (!(surf->flags & SURF_DRAWFULLBRIGHT || ent->effects & EF_FULLBRIGHT))
			{
				if (surf->dlightframe == r_framecount)
					RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, m.numverts, m.vertex, m.color);
				if (surf->flags & SURF_LIGHTMAP)
					RSurf_AddLightmapToVertexColors(mesh->vertex, m.color, m.numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
			}
			RSurf_FogColors(m.vertex, m.color, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}

	if (fogenabled)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
		m.tex[0] = R_GetTexture(surf->currenttexture->fogtexture);
		m.matrix = ent->matrix;
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			m.numtriangles = mesh->numtriangles;
			m.numverts = mesh->numverts;
			if (R_Mesh_Draw_GetBuffer(&m, false))
			{
				memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
				RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
				if (m.tex[0])
					RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
				RSurf_FogPassColors(m.vertex, m.color, fogcolor[0], fogcolor[1], fogcolor[2], alpha, m.colorscale, m.numverts, modelorg);
				R_Mesh_Render();
			}
		}
	}
}

static void RSurfShader_Water(const entity_render_t *ent, const msurface_t *firstsurf)
{
	const msurface_t *surf;
	vec3_t center;
	for (surf = firstsurf;surf;surf = surf->chain)
	{
		if ((r_wateralpha.value < 1 && !(surf->flags & SURF_DRAWNOALPHA)) || ent->effects & EF_ADDITIVE || surf->currenttexture->fogtexture)
		{
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Water_Callback, ent, surf - ent->model->surfaces);
		}
		else
			R_MeshQueue_Add(RSurfShader_Water_Callback, ent, surf - ent->model->surfaces);
	}
}

static void RSurfShader_Wall_Pass_BaseVertex(const entity_render_t *ent, const msurface_t *surf)
{
	float base;
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
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
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	base = ent->effects & EF_FULLBRIGHT ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, true))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			R_FillColors(m.color, m.numverts, base, base, base, ent->alpha);
			if (!(ent->effects & EF_FULLBRIGHT))
			{
				if (surf->dlightframe == r_framecount)
					RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, m.numverts, m.vertex, m.color);
				if (surf->flags & SURF_LIGHTMAP)
					RSurf_AddLightmapToVertexColors(mesh->vertex, m.color, m.numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
			}
			RSurf_FogColors(m.vertex, m.color, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_Wall_Pass_BaseFullbright(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
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
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_FoggedColors(m.vertex, m.color, 1, 1, 1, ent->alpha, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_Wall_Pass_Glow(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.tex[0] = R_GetTexture(surf->currenttexture->glowtexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_FoggedColors(m.vertex, m.color, 1, 1, 1, ent->alpha, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_Wall_Pass_Fog(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.matrix = ent->matrix;
	m.tex[0] = R_GetTexture(surf->currenttexture->fogtexture);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			if (m.tex[0])
				RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_FogPassColors(m.vertex, m.color, fogcolor[0], fogcolor[1], fogcolor[2], ent->alpha, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_TripleTexCombine(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.texrgbscale[0] = 1.0f;
	m.tex[1] = R_GetTexture(surf->lightmaptexture);
	m.texrgbscale[1] = 4.0f;
	m.tex[2] = R_GetTexture(surf->currenttexture->detailtexture);
	m.texrgbscale[2] = 2.0f;
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			cl = (float) (1 << lightscalebit) * m.colorscale;
			R_FillColors(m.color, m.numverts, cl, cl, cl, 1);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_CopyUV(mesh->vertex, m.texcoords[1], m.numverts);
			RSurf_CopyAB(mesh->vertex, m.texcoords[2], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseMTex(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.tex[1] = R_GetTexture(surf->lightmaptexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, true))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			cl = (float) (1 << lightscalebit) * m.colorscale;
			R_FillColors(m.color, m.numverts, cl, cl, cl, 1);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_CopyUV(mesh->vertex, m.texcoords[1], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTexture(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			cl = m.colorscale;
			R_FillColors(m.color, m.numverts, cl, cl, cl, 1);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseLightmap(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float cl;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ZERO;
	m.blendfunc2 = GL_SRC_COLOR;
	m.tex[0] = R_GetTexture(surf->lightmaptexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, true))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			cl = (float) (1 << lightscalebit) * m.colorscale;
			R_FillColors(m.color, m.numverts, cl, cl, cl, 1);
			RSurf_CopyUV(mesh->vertex, m.texcoords[0], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Light(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;

	if (surf->dlightframe != r_framecount)
		return;
	if (ent->effects & EF_FULLBRIGHT)
		return;

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.tex[0] = R_GetTexture(surf->currenttexture->texture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		if (RSurf_LightCheck(surf->dlightbits, mesh))
		{
			m.numtriangles = mesh->numtriangles;
			m.numverts = mesh->numverts;
			if (R_Mesh_Draw_GetBuffer(&m, true))
			{
				memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
				RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
				RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
				R_FillColors(m.color, m.numverts, 0, 0, 0, 1);
				RSurf_LightSeparate(&ent->inversematrix, surf->dlightbits, m.numverts, m.vertex, m.color);
				RSurf_ScaleColors(m.color, m.colorscale, m.numverts);
				R_Mesh_Render();
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Fog(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.tex[0] = R_GetTexture(surf->currenttexture->fogtexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			if (m.tex[0])
				RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			RSurf_FogPassColors(m.vertex, m.color, fogcolor[0], fogcolor[1], fogcolor[2], 1, m.colorscale, m.numverts, modelorg);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDetail(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_DST_COLOR;
	m.blendfunc2 = GL_SRC_COLOR;
	m.tex[0] = R_GetTexture(surf->currenttexture->detailtexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			R_FillColors(m.color, m.numverts, 1, 1, 1, 1);
			RSurf_CopyAB(mesh->vertex, m.texcoords[0], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Glow(const entity_render_t *ent, const msurface_t *surf)
{
	surfmesh_t *mesh;
	rmeshbufferinfo_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.tex[0] = R_GetTexture(surf->currenttexture->glowtexture);
	m.matrix = ent->matrix;
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		m.numtriangles = mesh->numtriangles;
		m.numverts = mesh->numverts;
		if (R_Mesh_Draw_GetBuffer(&m, false))
		{
			memcpy(m.index, mesh->index, m.numtriangles * sizeof(int[3]));
			RSurf_CopyXYZ(mesh->vertex, m.vertex, m.numverts);
			R_FillColors(m.color, m.numverts, m.colorscale, m.colorscale, m.colorscale, 1);
			RSurf_CopyST(mesh->vertex, m.texcoords[0], m.numverts);
			R_Mesh_Render();
		}
	}
}

static void RSurfShader_Wall_Fullbright_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->surfaces + calldata2;
	RSurfShader_Wall_Pass_BaseFullbright(ent, surf);
	if (surf->currenttexture->glowtexture)
		RSurfShader_Wall_Pass_Glow(ent, surf);
	if (fogenabled)
		RSurfShader_Wall_Pass_Fog(ent, surf);
}

static void RSurfShader_Wall_Fullbright(const entity_render_t *ent, const msurface_t *firstsurf)
{
	const msurface_t *surf;
	vec3_t center;
	if (ent->effects & EF_ADDITIVE || ent->alpha < 1)
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Fullbright_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			if (surf->currenttexture->fogtexture != NULL)
			{
				Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
				R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Fullbright_Callback, ent, surf - ent->model->surfaces);
			}
			else
				RSurfShader_Wall_Pass_BaseFullbright(ent, surf);
		}
		for (surf = firstsurf;surf;surf = surf->chain)
			if (surf->currenttexture->glowtexture)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Fog(ent, surf);
	}
}

static void RSurfShader_Wall_Vertex_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->surfaces + calldata2;
	RSurfShader_Wall_Pass_BaseVertex(ent, surf);
	if (surf->currenttexture->glowtexture)
		RSurfShader_Wall_Pass_Glow(ent, surf);
	if (fogenabled)
		RSurfShader_Wall_Pass_Fog(ent, surf);
}

static void RSurfShader_Wall_Vertex(const entity_render_t *ent, const msurface_t *firstsurf)
{
	const msurface_t *surf;
	vec3_t center;
	if (ent->effects & EF_ADDITIVE || ent->alpha < 1)
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			if (surf->currenttexture->fogtexture != NULL)
			{
				Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
				R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
			}
			else
				RSurfShader_Wall_Pass_BaseVertex(ent, surf);
		}
		for (surf = firstsurf;surf;surf = surf->chain)
			if (surf->currenttexture->glowtexture)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Fog(ent, surf);
	}
}

static void RSurfShader_Wall_Lightmap(const entity_render_t *ent, const msurface_t *firstsurf)
{
	const msurface_t *surf;
	vec3_t center;
	if (ent->alpha < 1 || ent->effects & EF_ADDITIVE)
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
			R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
		}
	}
	else if (r_vertexsurfaces.integer || ent->alpha < 1 || ent->effects & EF_ADDITIVE)
	{
		for (surf = firstsurf;surf;surf = surf->chain)
		{
			c_brush_polys++;
			if (surf->currenttexture->fogtexture != NULL)
			{
				Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
				R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
			}
			else
				RSurfShader_Wall_Pass_BaseVertex(ent, surf);
		}
		for (surf = firstsurf;surf;surf = surf->chain)
			if (surf->currenttexture->glowtexture)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_Wall_Pass_Fog(ent, surf);
	}
	else
	{
		if (r_textureunits.integer >= 2)
		{
			if (r_textureunits.integer >= 3 && gl_combine.integer && r_detailtextures.integer)
			{
				for (surf = firstsurf;surf;surf = surf->chain)
				{
					c_brush_polys++;
					if (surf->currenttexture->fogtexture != NULL)
					{
						Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
						R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
					}
					else
						RSurfShader_OpaqueWall_Pass_TripleTexCombine(ent, surf);
				}
			}
			else
			{
				for (surf = firstsurf;surf;surf = surf->chain)
				{
					c_brush_polys++;
					if (surf->currenttexture->fogtexture != NULL)
					{
						Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
						R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
					}
					else
						RSurfShader_OpaqueWall_Pass_BaseMTex(ent, surf);
				}
				if (r_detailtextures.integer)
					for (surf = firstsurf;surf;surf = surf->chain)
						if (surf->currenttexture->fogtexture == NULL)
							RSurfShader_OpaqueWall_Pass_BaseDetail(ent, surf);
			}
		}
		else
		{
			for (surf = firstsurf;surf;surf = surf->chain)
			{
				c_brush_polys++;
				if (surf->currenttexture->fogtexture != NULL)
				{
					Matrix4x4_Transform(&ent->matrix, surf->poly_center, center);
					R_MeshQueue_AddTransparent(center, RSurfShader_Wall_Vertex_Callback, ent, surf - ent->model->surfaces);
				}
				else
					RSurfShader_OpaqueWall_Pass_BaseTexture(ent, surf);
			}
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_OpaqueWall_Pass_BaseLightmap(ent, surf);
			if (r_detailtextures.integer)
				for (surf = firstsurf;surf;surf = surf->chain)
					if (surf->currenttexture->fogtexture == NULL)
						RSurfShader_OpaqueWall_Pass_BaseDetail(ent, surf);
		}
		if (!r_dlightmap.integer)
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->dlightframe == r_framecount)
					if (surf->currenttexture->fogtexture == NULL)
						RSurfShader_OpaqueWall_Pass_Light(ent, surf);
		for (surf = firstsurf;surf;surf = surf->chain)
			if (surf->currenttexture->glowtexture)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_OpaqueWall_Pass_Glow(ent, surf);
		if (fogenabled)
			for (surf = firstsurf;surf;surf = surf->chain)
				if (surf->currenttexture->fogtexture == NULL)
					RSurfShader_OpaqueWall_Pass_Fog(ent, surf);
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

static void R_SolidWorldNode (entity_render_t *ent)
{
	if (r_viewleaf->contents != CONTENTS_SOLID)
	{
		int portalstack;
		mportal_t *p, *pstack[8192];
		msurface_t *surf, **mark, **endmark;
		mleaf_t *leaf;
		// LordHavoc: portal-passage worldnode; follows portals leading
		// outward from viewleaf, if a portal leads offscreen it is not
		// followed, in indoor maps this can often cull a great deal of
		// geometry away when pvs data is not present (useful with pvs as well)

		leaf = r_viewleaf;
		leaf->worldnodeframe = r_framecount;
		portalstack = 0;
	loc0:
		c_leafs++;

		leaf->visframe = r_framecount;

		if (leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			endmark = mark + leaf->nummarksurfaces;
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
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
			while (mark < endmark);
		}

		// follow portals into other leafs
		p = leaf->portals;
		for (;p;p = p->next)
		{
			if (DotProduct(r_origin, p->plane.normal) < p->plane.dist)
			{
				leaf = p->past;
				if (leaf->worldnodeframe != r_framecount)
				{
					leaf->worldnodeframe = r_framecount;
					if (leaf->contents != CONTENTS_SOLID)
					{
						if (R_NotCulledBox(leaf->mins, leaf->maxs))
						{
							p->visframe = r_framecount;
							pstack[portalstack++] = p;
							goto loc0;

	loc1:
							p = pstack[--portalstack];
						}
					}
				}
			}
		}

		if (portalstack)
			goto loc1;
	}
	else
	{
		mnode_t *nodestack[8192], *node = cl.worldmodel->nodes;
		int nodestackpos = 0;
		// LordHavoc: recursive descending worldnode; if portals are not
		// available, this is a good last resort, can cull large amounts of
		// geometry, but is more time consuming than portal-passage and renders
		// things behind walls

loc2:
		if (R_NotCulledBox(node->mins, node->maxs))
		{
			if (node->numsurfaces)
			{
				msurface_t *surf = cl.worldmodel->surfaces + node->firstsurface, *surfend = surf + node->numsurfaces;
				if (PlaneDiff (r_origin, node->plane) < 0)
				{
					for (;surf < surfend;surf++)
					{
						if (surf->flags & SURF_PLANEBACK)
							surf->visframe = r_framecount;
					}
				}
				else
				{
					for (;surf < surfend;surf++)
					{
						if (!(surf->flags & SURF_PLANEBACK))
							surf->visframe = r_framecount;
					}
				}
			}

			// recurse down the children
			if (node->children[0]->contents >= 0)
			{
				if (node->children[1]->contents >= 0)
				{
					if (nodestackpos < 8192)
						nodestack[nodestackpos++] = node->children[1];
					node = node->children[0];
					goto loc2;
				}
				else
					((mleaf_t *)node->children[1])->visframe = r_framecount;
				node = node->children[0];
				goto loc2;
			}
			else
			{
				((mleaf_t *)node->children[0])->visframe = r_framecount;
				if (node->children[1]->contents >= 0)
				{
					node = node->children[1];
					goto loc2;
				}
				else if (nodestackpos > 0)
				{
					((mleaf_t *)node->children[1])->visframe = r_framecount;
					node = nodestack[--nodestackpos];
					goto loc2;
				}
			}
		}
		else if (nodestackpos > 0)
		{
			node = nodestack[--nodestackpos];
			goto loc2;
		}
	}
}

static void R_PVSWorldNode()
{
	int portalstack, i;
	mportal_t *p, *pstack[8192];
	msurface_t *surf, **mark, **endmark;
	mleaf_t *leaf;
	qbyte *worldvis;

	worldvis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	leaf = r_viewleaf;
	leaf->worldnodeframe = r_framecount;
	portalstack = 0;
loc0:
	c_leafs++;

	leaf->visframe = r_framecount;

	if (leaf->nummarksurfaces)
	{
		mark = leaf->firstmarksurface;
		endmark = mark + leaf->nummarksurfaces;
		do
		{
			surf = *mark++;
			// make sure surfaces are only processed once
			if (surf->worldnodeframe == r_framecount)
				continue;
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
		while (mark < endmark);
	}

	// follow portals into other leafs
	for (p = leaf->portals;p;p = p->next)
	{
		if (DotProduct(r_origin, p->plane.normal) < p->plane.dist)
		{
			leaf = p->past;
			if (leaf->worldnodeframe != r_framecount)
			{
				leaf->worldnodeframe = r_framecount;
				if (leaf->contents != CONTENTS_SOLID)
				{
					i = (leaf - cl.worldmodel->leafs) - 1;
					if (worldvis[i>>3] & (1<<(i&7)))
					{
						if (R_NotCulledBox(leaf->mins, leaf->maxs))
						{
							pstack[portalstack++] = p;
							goto loc0;

loc1:
							p = pstack[--portalstack];
						}
					}
				}
			}
		}
	}

	if (portalstack)
		goto loc1;
}

Cshader_t Cshader_wall_vertex = {{NULL, RSurfShader_Wall_Vertex}, NULL};
Cshader_t Cshader_wall_lightmap = {{NULL, RSurfShader_Wall_Lightmap}, NULL};
Cshader_t Cshader_wall_fullbright = {{NULL, RSurfShader_Wall_Fullbright}, NULL};
Cshader_t Cshader_water = {{NULL, RSurfShader_Water}, NULL};
Cshader_t Cshader_sky = {{RSurfShader_Sky, NULL}, NULL};

int Cshader_count = 5;
Cshader_t *Cshaders[5] =
{
	&Cshader_wall_vertex,
	&Cshader_wall_lightmap,
	&Cshader_wall_fullbright,
	&Cshader_water,
	&Cshader_sky
};

void R_PrepareSurfaces(entity_render_t *ent)
{
	int i, alttextures, texframe, framecount;
	texture_t *t;
	model_t *model;
	msurface_t *surf;

	for (i = 0;i < Cshader_count;i++)
		Cshaders[i]->chain = NULL;

	model = ent->model;
	alttextures = ent->frame != 0;
	texframe = (int)(cl.time * 5.0f);

	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surf = model->modelsortedsurfaces[i];
		if (surf->visframe == r_framecount)
		{
			if (surf->insertframe != r_framecount)
			{
				surf->insertframe = r_framecount;
				c_faces++;
				t = surf->texinfo->texture;
				if (t->animated)
				{
					framecount = t->anim_total[alttextures];
					if (framecount >= 2)
						surf->currenttexture = t->anim_frames[alttextures][texframe % framecount];
					else
						surf->currenttexture = t->anim_frames[alttextures][0];
				}
				else
					surf->currenttexture = t;
			}

			surf->chain = surf->shader->chain;
			surf->shader->chain = surf;
		}
	}
}

void R_DrawSurfaces (entity_render_t *ent, int type)
{
	int			i;
	Cshader_t	*shader;

	for (i = 0;i < Cshader_count;i++)
	{
		shader = Cshaders[i];
		if (shader->chain && shader->shaderfunc[type])
			shader->shaderfunc[type](ent, shader->chain);
	}
}

static void R_DrawPortal_Callback(const void *calldata1, int calldata2)
{
	int i;
	float *v;
	rmeshbufferinfo_t m;
	const entity_render_t *ent = calldata1;
	const mportal_t *portal = ent->model->portals + calldata2;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.numverts = portal->numpoints;
	m.numtriangles = portal->numpoints - 2;
	m.matrix = ent->matrix;
	if (R_Mesh_Draw_GetBuffer(&m, false))
	{
		for (i = 0;i < m.numtriangles;i++)
		{
			m.index[i * 3 + 0] = 0;
			m.index[i * 3 + 1] = i + 1;
			m.index[i * 3 + 2] = i + 2;
		}
		i = portal - ent->model->portals;
		R_FillColors(m.color, m.numverts,
			((i & 0x0007) >> 0) * (1.0f / 7.0f) * m.colorscale,
			((i & 0x0038) >> 3) * (1.0f / 7.0f) * m.colorscale,
			((i & 0x01C0) >> 6) * (1.0f / 7.0f) * m.colorscale,
			0.125f);
		if (PlaneDiff(r_origin, (&portal->plane)) > 0)
		{
			for (i = portal->numpoints - 1, v = m.vertex;i >= 0;i--, v += 4)
				VectorCopy(portal->points[i].position, v);
		}
		else
			for (i = 0, v = m.vertex;i < portal->numpoints;i++, v += 4)
				VectorCopy(portal->points[i].position, v);
		R_Mesh_Render();
	}
}

void R_DrawPortals(entity_render_t *ent)
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

void R_SetupForBModelRendering(entity_render_t *ent)
{
	int			i;
	msurface_t	*surf;
	model_t		*model;
	vec3_t		modelorg;

	// because bmodels can be reused, we have to decide which things to render
	// from scratch every time

	model = ent->model;

	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surf = model->modelsortedsurfaces[i];
		if (((surf->flags & SURF_PLANEBACK) == 0) == (PlaneDiff(modelorg, surf->plane) >= 0))
			surf->visframe = r_framecount;
		else
			surf->visframe = -1;
		surf->worldnodeframe = -1;
		surf->lightframe = -1;
		surf->dlightframe = -1;
		surf->insertframe = -1;
	}
}

void R_SurfMarkLights (entity_render_t *ent)
{
	int			i;
	msurface_t	*surf;

	if (r_dynamic.integer)
		R_MarkLights(ent);

	if (!r_vertexsurfaces.integer)
	{
		for (i = 0, surf = ent->model->surfaces + ent->model->firstmodelsurface;i < ent->model->nummodelsurfaces;i++, surf++)
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
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (entity_render_t *ent)
{
	// there is only one instance of the world, but it can be rendered in
	// multiple stages
	if (r_viewleaf->contents == CONTENTS_SOLID || r_novis.integer || r_viewleaf->compressed_vis == NULL)
		R_SolidWorldNode (ent);
	else
		R_PVSWorldNode (ent);
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelSky (entity_render_t *ent)
{
	R_SetupForBModelRendering(ent);

	R_PrepareSurfaces(ent);
	R_DrawSurfaces(ent, SHADERSTAGE_SKY);
}

void R_DrawBrushModelNormal (entity_render_t *ent)
{
	c_bmodels++;

	// have to flush queue because of possible lightmap reuse
	R_Mesh_Render();

	R_SetupForBModelRendering(ent);

	R_SurfMarkLights(ent);

	R_PrepareSurfaces(ent);

	if (!skyrendermasked)
		R_DrawSurfaces(ent, SHADERSTAGE_SKY);
	R_DrawSurfaces(ent, SHADERSTAGE_NORMAL);
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
	Cvar_RegisterVariable(&r_vertexsurfaces);
	Cvar_RegisterVariable(&r_dlightmap);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);
	Cvar_RegisterVariable(&r_floatbuildlightmap);
	Cvar_RegisterVariable(&r_detailtextures);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

