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
cvar_t r_vertexsurfaces = {0, "r_vertexsurfaces", "0"};
cvar_t r_dlightmap = {CVAR_SAVE, "r_dlightmap", "1"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_floatbuildlightmap = {0, "r_floatbuildlightmap", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "1"};
cvar_t r_drawcollisionbrushes_polygonoffset = {0, "r_drawcollisionbrushes_polygonoffset", "-4"};

static int dlightdivtable[32768];

static int R_IntAddDynamicLights (const matrix4x4_t *matrix, msurface_t *surf)
{
	int sdtable[256], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, red, green, blue, lit, dist2, impacts, impactt, subtract, k;
	unsigned int *bl;
	float dist, impact[3], local[3];

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
		red = r_dlight[lnum].light[0] * (1.0f / 128.0f);
		green = r_dlight[lnum].light[1] * (1.0f / 128.0f);
		blue = r_dlight[lnum].light[2] * (1.0f / 128.0f);
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
static void R_BuildLightMap (const entity_render_t *ent, msurface_t *surf)
{
	if (!r_floatbuildlightmap.integer)
	{
		int smax, tmax, i, j, size, size3, shift, maps, stride, l;
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
		if ((ent->effects & EF_FULLBRIGHT) || !ent->model->brushq1.lightdata)
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
		shift = 7 + r_lightmapscalebit + 8;
		if (ent->model->brushq1.lightmaprgba)
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

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		size = smax*tmax;
		size3 = size*3;
		lightmap = surf->samples;

	// set to full bright if no light data
		bl = floatblocklights;
		if ((ent->effects & EF_FULLBRIGHT) || !ent->model->brushq1.lightdata)
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
		scale = 1.0f / (1 << (7 + r_lightmapscalebit + 8));
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
	const rdlight_t *rd;
	vec3_t lightorigin;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			rd = &r_dlight[l];
			Matrix4x4_Transform(matrix, rd->origin, lightorigin);
			for (i = 0, v = vert, c = color;i < numverts;i++, v += 3, c += 4)
			{
				f = VectorDistance2(v, lightorigin) + LIGHTOFFSET;
				if (f < rd->cullradius2)
				{
					f = ((1.0f / f) - rd->subtract) * scale;
					VectorMA(c, f, rd->light, c);
					lit = true;
				}
			}
		}
	}
	return lit;
}

// note: this untransforms lights to do the checking
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
			for (i = 0, v = mesh->vertex3f;i < mesh->numverts;i++, v += 3)
				if (VectorDistance2(v, lightorigin) < rd->cullradius2)
					return true;
		}
	}
	return false;
}

static void RSurfShader_Sky(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
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

	GL_Color(fogcolor[0] * r_colorscale, fogcolor[1] * r_colorscale, fogcolor[2] * r_colorscale, 1);
	if (skyrendermasked)
	{
		// depth-only (masking)
		qglColorMask(0,0,0,0);
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
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
	qglColorMask(1,1,1,1);
}

static void RSurfShader_Water_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surf = ent->model->brushq1.surfaces + calldata2;
	float f, colorscale;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float alpha;
	float modelorg[3];
	texture_t *texture;
	matrix4x4_t tempmatrix;

	if (r_waterscroll.value)
	{
		// scrolling in texture matrix
		Matrix4x4_CreateTranslate(&tempmatrix, sin(cl.time) * 0.025 * r_waterscroll.value, sin(cl.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
		R_Mesh_TextureMatrix(0, &tempmatrix);
	}

	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);

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
	m.tex[0] = R_GetTexture(texture->skin.base);
	colorscale = r_colorscale;
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 4;
		colorscale *= 0.25f;
	}
	GL_DepthTest(true);
	GL_ColorPointer(varray_color4f);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		GL_VertexPointer(mesh->vertex3f);
		m.pointer_texcoord[0] = mesh->texcoordtexture2f;
		R_Mesh_State_Texture(&m);
		f = surf->flags & SURF_DRAWFULLBRIGHT ? 1.0f : ((surf->flags & SURF_LIGHTMAP) ? 0 : 0.5f);
		R_FillColors(varray_color4f, mesh->numverts, f, f, f, alpha);
		if (!(surf->flags & SURF_DRAWFULLBRIGHT || ent->effects & EF_FULLBRIGHT))
		{
			if (surf->dlightframe == r_framecount)
				RSurf_LightSeparate_Vertex3f_Color4f(&ent->inversematrix, surf->dlightbits, mesh->numverts, mesh->vertex3f, varray_color4f, 1);
			if (surf->flags & SURF_LIGHTMAP)
				RSurf_AddLightmapToVertexColors_Color4f(mesh->lightmapoffsets, varray_color4f, mesh->numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
		}
		RSurf_FogColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
	}

	if (fogenabled)
	{
		memset(&m, 0, sizeof(m));
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		m.tex[0] = R_GetTexture(texture->skin.fog);
		for (mesh = surf->mesh;mesh;mesh = mesh->chain)
		{
			GL_VertexPointer(mesh->vertex3f);
			m.pointer_texcoord[0] = mesh->texcoordtexture2f;
			GL_ColorPointer(varray_color4f);
			R_Mesh_State_Texture(&m);
			RSurf_FogPassColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], alpha, r_colorscale, mesh->numverts, modelorg);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
		}
	}

	if (r_waterscroll.value)
	{
		Matrix4x4_CreateIdentity(&tempmatrix);
		R_Mesh_TextureMatrix(0, &tempmatrix);
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
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
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
	colorscale = r_colorscale;
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 4;
		colorscale *= 0.25f;
	}
	base = ent->effects & EF_FULLBRIGHT ? 2.0f : r_ambient.value * (1.0f / 64.0f);
	GL_DepthTest(true);
	GL_ColorPointer(varray_color4f);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		GL_VertexPointer(mesh->vertex3f);
		m.pointer_texcoord[0] = mesh->texcoordtexture2f;
		R_Mesh_State_Texture(&m);
		R_FillColors(varray_color4f, mesh->numverts, base, base, base, currentalpha);
		if (!(ent->effects & EF_FULLBRIGHT))
		{
			if (surf->dlightframe == r_framecount)
				RSurf_LightSeparate_Vertex3f_Color4f(&ent->inversematrix, surf->dlightbits, mesh->numverts, mesh->vertex3f, varray_color4f, 1);
			if (surf->flags & SURF_LIGHTMAP)
				RSurf_AddLightmapToVertexColors_Color4f(mesh->lightmapoffsets, varray_color4f, mesh->numverts, surf->samples, ((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1)*3, surf->styles);
		}
		RSurf_FogColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
	}
}

static void RSurfShader_Wall_Pass_Glow(const entity_render_t *ent, const msurface_t *surf, const texture_t *texture, int rendertype, float currentalpha)
{
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	GL_ColorPointer(varray_color4f);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		GL_VertexPointer(mesh->vertex3f);
		if (m.tex[0])
			m.pointer_texcoord[0] = mesh->texcoordtexture2f;
		R_Mesh_State_Texture(&m);
		RSurf_FoggedColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, 1, 1, 1, currentalpha, r_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
	}
}

static void RSurfShader_Wall_Pass_Fog(const entity_render_t *ent, const msurface_t *surf, const texture_t *texture, int rendertype, float currentalpha)
{
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.fog);
	GL_ColorPointer(varray_color4f);
	for (mesh = surf->mesh;mesh;mesh = mesh->chain)
	{
		GL_VertexPointer(mesh->vertex3f);
		if (m.tex[0])
			m.pointer_texcoord[0] = mesh->texcoordtexture2f;
		R_Mesh_State_Texture(&m);
		RSurf_FogPassColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], currentalpha, r_colorscale, mesh->numverts, modelorg);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTripleTexCombine(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	float cl;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	m.tex[1] = R_GetTexture((**surfchain).lightmaptexture);
	m.tex[2] = R_GetTexture(texture->skin.detail);
	m.texrgbscale[0] = 1;
	m.texrgbscale[1] = 4;
	m.texrgbscale[2] = 2;
	cl = (float) (1 << r_lightmapscalebit) * r_colorscale;
	GL_Color(cl, cl, cl, 1);

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
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				m.pointer_texcoord[1] = mesh->texcoordlightmap2f;
				m.pointer_texcoord[2] = mesh->texcoorddetail2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDoubleTex(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	m.tex[1] = R_GetTexture((**surfchain).lightmaptexture);
	if (gl_combine.integer)
		m.texrgbscale[1] = 4;
	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
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
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				m.pointer_texcoord[1] = mesh->texcoordlightmap2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseTexture(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_DepthMask(true);
	GL_DepthTest(true);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	m.tex[0] = R_GetTexture(texture->skin.base);
	GL_Color(1, 1, 1, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseLightmap(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	int lightmaptexturenum;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ZERO, GL_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture((**surfchain).lightmaptexture);
	if (gl_combine.integer)
		m.texrgbscale[0] = 4;
	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
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
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordlightmap2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Light(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	float colorscale;
	rmeshstate_t m;

	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.base);
	colorscale = r_colorscale;
	if (gl_combine.integer)
	{
		m.texrgbscale[0] = 4;
		colorscale *= 0.25f;
	}
	GL_ColorPointer(varray_color4f);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount && surf->dlightframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				if (RSurf_LightCheck(&ent->inversematrix, surf->dlightbits, mesh))
				{
					GL_VertexPointer(mesh->vertex3f);
					m.pointer_texcoord[0] = mesh->texcoordtexture2f;
					R_FillColors(varray_color4f, mesh->numverts, 0, 0, 0, 1);
					R_Mesh_State_Texture(&m);
					RSurf_LightSeparate_Vertex3f_Color4f(&ent->inversematrix, surf->dlightbits, mesh->numverts, mesh->vertex3f, varray_color4f, colorscale);
					R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
				}
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Fog(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	float modelorg[3];
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	GL_ColorPointer(varray_color4f);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				if (m.tex[0])
					m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				R_Mesh_State_Texture(&m);
				RSurf_FogPassColors_Vertex3f_Color4f(mesh->vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], 1, r_colorscale, mesh->numverts, modelorg);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_BaseDetail(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
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
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoorddetail2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_Glow(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
		}
	}
}

static void RSurfShader_OpaqueWall_Pass_OpaqueGlow(const entity_render_t *ent, const texture_t *texture, msurface_t **surfchain)
{
	const msurface_t *surf;
	const surfmesh_t *mesh;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
	GL_DepthMask(true);
	m.tex[0] = R_GetTexture(texture->skin.glow);
	if (m.tex[0])
		GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	else
		GL_Color(0, 0, 0, 1);
	while((surf = *surfchain++) != NULL)
	{
		if (surf->visframe == r_framecount)
		{
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				GL_VertexPointer(mesh->vertex3f);
				m.pointer_texcoord[0] = mesh->texcoordtexture2f;
				R_Mesh_State_Texture(&m);
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			}
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
	if (texture->flags & SURF_WATERALPHA)
		currentalpha *= r_wateralpha.value;
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
	else if (r_shadow_realtime_world.integer)
	{
		// opaque base lighting
		RSurfShader_OpaqueWall_Pass_OpaqueGlow(ent, texture, surfchain);
		if (fogenabled)
			RSurfShader_OpaqueWall_Pass_Fog(ent, texture, surfchain);
	}
	else if (r_vertexsurfaces.integer)
	{
		// opaque vertex shaded from lightmap
		for (chain = surfchain;(surf = *chain) != NULL;chain++)
			if (surf->visframe == r_framecount)
				RSurfShader_Wall_Pass_BaseVertex(ent, surf, texture, texture->rendertype, texture->currentalpha);
		if (texture->skin.glow)
			for (chain = surfchain;(surf = *chain) != NULL;chain++)
				if (surf->visframe == r_framecount)
					RSurfShader_Wall_Pass_Glow(ent, surf, texture, texture->rendertype, texture->currentalpha);
		if (fogenabled)
			for (chain = surfchain;(surf = *chain) != NULL;chain++)
				if (surf->visframe == r_framecount)
					RSurfShader_Wall_Pass_Fog(ent, surf, texture, texture->rendertype, texture->currentalpha);
	}
	else
	{
		// opaque lightmapped
		if (r_textureunits.integer >= 2)
		{
			if (r_textureunits.integer >= 3 && gl_combine.integer && r_detailtextures.integer)
				RSurfShader_OpaqueWall_Pass_BaseTripleTexCombine(ent, texture, surfchain);
			else
			{
				RSurfShader_OpaqueWall_Pass_BaseDoubleTex(ent, texture, surfchain);
				if (r_detailtextures.integer)
					RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
			}
		}
		else
		{
			RSurfShader_OpaqueWall_Pass_BaseTexture(ent, texture, surfchain);
			RSurfShader_OpaqueWall_Pass_BaseLightmap(ent, texture, surfchain);
			if (r_detailtextures.integer)
				RSurfShader_OpaqueWall_Pass_BaseDetail(ent, texture, surfchain);
		}
		if (!r_dlightmap.integer && !(ent->effects & EF_FULLBRIGHT))
			RSurfShader_OpaqueWall_Pass_Light(ent, texture, surfchain);
		if (texture->skin.glow)
			RSurfShader_OpaqueWall_Pass_Glow(ent, texture, surfchain);
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
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
	numsurfaces = model->brushq1.nummodelsurfaces;
	surfaces = model->brushq1.surfaces + model->brushq1.firstmodelsurface;
	surfacevisframes = model->brushq1.surfacevisframes + model->brushq1.firstmodelsurface;

	R_UpdateTextureInfo(ent);

	if (r_dynamic.integer && !r_shadow_realtime_dlight.integer)
		R_MarkLights(ent);

	if (model->brushq1.light_ambient != r_ambient.value || model->brushq1.light_scalebit != r_lightmapscalebit)
	{
		model->brushq1.light_ambient = r_ambient.value;
		model->brushq1.light_scalebit = r_lightmapscalebit;
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
				if (surf->cached_dlight && surf->lightmaptexture != NULL && !r_vertexsurfaces.integer)
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
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f) * r_colorscale,
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f) * r_colorscale,
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f) * r_colorscale,
			 0.125f);
	if (PlaneDiff(r_origin, (&portal->plane)) > 0)
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
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
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
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);

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
	int c, leafstackpos, *mark, *surfacevisframes, bitnum;
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
	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
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
					bitnum = (leaf - ent->model->brushq1.leafs) - 1;
					if ((r_pvsbits[bitnum >> 3] & (1 << (bitnum & 7))) && !R_CullBox(leaf->mins, leaf->maxs))
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
			for (j = 0;j < model->brushq1.visleafs;j++)
			{
				if (r_pvsbits[j >> 3] & (1 << (j & 7)))
				{
					leaf = model->brushq1.leafs + j + 1;
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

	Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
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
	if (!ent->model->brushq1.numleafs && ent->model->Draw)
		ent->model->Draw(ent);
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
	int i;
	msurface_t *surf;
	float projectdistance, f, temp[3], lightradius2;
	surfmesh_t *mesh;
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	lightradius2 = lightradius * lightradius;
	R_UpdateTextureInfo(ent);
	projectdistance = 1000000000.0f;//lightradius + ent->model->radius;
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
					for (mesh = surf->mesh;mesh;mesh = mesh->chain)
						R_Shadow_Volume(mesh->numverts, mesh->numtriangles, mesh->vertex3f, mesh->element3i, mesh->neighbor3i, relativelightorigin, lightradius, projectdistance);
			}
		}
	}
}

void R_Model_Brush_DrawLightForSurfaceList(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, msurface_t **surflist, int numsurfaces, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz)
{
	int surfnum;
	msurface_t *surf;
	texture_t *t;
	surfmesh_t *mesh;
	if (ent->model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	R_UpdateTextureInfo(ent);
	for (surfnum = 0;surfnum < numsurfaces;surfnum++)
	{
		surf = surflist[surfnum];
		if (surf->visframe == r_framecount)
		{
			t = surf->texinfo->texture->currentframe;
			if (t->rendertype == SURFRENDER_OPAQUE && t->flags & SURF_SHADOWLIGHT)
			{
				for (mesh = surf->mesh;mesh;mesh = mesh->chain)
				{
					R_Shadow_DiffuseLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoordtexture2f, relativelightorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, NULL);
					R_Shadow_SpecularLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.gloss, t->skin.nmap, NULL);
				}
			}
		}
	}
}

void R_Model_Brush_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz)
{
	int surfnum;
	msurface_t *surf;
	texture_t *t;
	float f, lightmins[3], lightmaxs[3];
	surfmesh_t *mesh;
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
	for (surfnum = 0, surf = ent->model->brushq1.surfaces + ent->model->brushq1.firstmodelsurface;surfnum < ent->model->brushq1.nummodelsurfaces;surfnum++, surf++)
	{
		if ((ent != &cl_entities[0].render || surf->visframe == r_framecount) && BoxesOverlap(surf->poly_mins, surf->poly_maxs, lightmins, lightmaxs))
		{
			f = PlaneDiff(relativelightorigin, surf->plane);
			if (surf->flags & SURF_PLANEBACK)
				f = -f;
			if (f >= -0.1 && f < lightradius)
			{
				t = surf->texinfo->texture->currentframe;
				if (t->rendertype == SURFRENDER_OPAQUE && t->flags & SURF_SHADOWLIGHT)
				{
					for (mesh = surf->mesh;mesh;mesh = mesh->chain)
					{
						R_Shadow_DiffuseLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoordtexture2f, relativelightorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, NULL);
						R_Shadow_SpecularLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.gloss, t->skin.nmap, NULL);
					}
				}
			}
		}
	}
}

void R_DrawCollisionBrush(colbrushf_t *brush)
{
	int i;
	i = ((int)brush) / sizeof(colbrushf_t);
	GL_Color((i & 31) * (1.0f / 32.0f) * r_colorscale, ((i >> 5) & 31) * (1.0f / 32.0f) * r_colorscale, ((i >> 10) & 31) * (1.0f / 32.0f) * r_colorscale, 0.2f);
	GL_VertexPointer(brush->points->v);
	R_Mesh_Draw(brush->numpoints, brush->numtriangles, brush->elements);
}

void R_Q3BSP_DrawFace(entity_render_t *ent, q3mface_t *face)
{
	rmeshstate_t m;
	if ((face->texture->renderflags & Q3MTEXTURERENDERFLAGS_NODRAW) || !face->numtriangles || R_CullBox(face->mins, face->maxs))
		return;
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	m.tex[0] = R_GetTexture(face->texture->skin.base);
	m.pointer_texcoord[0] = face->data_texcoordtexture2f;
	if (face->lightmaptexture)
	{
		m.tex[1] = R_GetTexture(face->lightmaptexture);
		m.pointer_texcoord[1] = face->data_texcoordlightmap2f;
		m.texrgbscale[1] = 2;
		GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	}
	else
	{
		m.texrgbscale[0] = 2;
		GL_ColorPointer(face->data_color4f);
	}
	R_Mesh_State_Texture(&m);
	GL_VertexPointer(face->data_vertex3f);
	R_Mesh_Draw(face->numvertices, face->numtriangles, face->data_element3i);
}

/*
void R_Q3BSP_DrawSky(entity_render_t *ent)
{
}
*/

void R_Q3BSP_RecursiveWorldNode(entity_render_t *ent, q3mnode_t *node, const vec3_t modelorg, qbyte *pvs, int markframe)
{
	int i;
	q3mleaf_t *leaf;
	q3mface_t *face;
	while (node->isnode)
	{
		if (R_CullBox(node->mins, node->maxs))
			return;
		R_Q3BSP_RecursiveWorldNode(ent, node->children[0], modelorg, pvs, markframe);
		node = node->children[1];
	}
	if (R_CullBox(node->mins, node->maxs))
		return;
	leaf = (q3mleaf_t *)node;
	if (pvs[leaf->clusterindex >> 3] & (1 << (leaf->clusterindex & 7)))
	{
		for (i = 0;i < leaf->numleaffaces;i++)
		{
			face = leaf->firstleafface[i];
			if (face->markframe != markframe)
			{
				face->markframe = markframe;
				R_Q3BSP_DrawFace(ent, face);
			}
		}
	}
}



void R_Q3BSP_Draw(entity_render_t *ent)
{
	int i;
	q3mface_t *face;
	vec3_t modelorg;
	model_t *model;
	qbyte *pvs;
	static int markframe = 0;
	R_Mesh_Matrix(&ent->matrix);
	model = ent->model;
	if (r_drawcollisionbrushes.integer < 2)
	{
		qglPolygonOffset(1.0f, 0);
		Matrix4x4_Transform(&ent->inversematrix, r_origin, modelorg);
		if (ent == &cl_entities[0].render && model->brushq3.num_pvsclusters && !r_novis.integer && (pvs = model->brush.GetPVS(model, modelorg)))
			R_Q3BSP_RecursiveWorldNode(ent, model->brushq3.data_nodes, modelorg, pvs, ++markframe);
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
		for (i = 0;i < model->brushq3.data_thismodel->numbrushes;i++)
			if (model->brushq3.data_thismodel->firstbrush[i].colbrushf && model->brushq3.data_thismodel->firstbrush[i].colbrushf->numtriangles)
				R_DrawCollisionBrush(model->brushq3.data_thismodel->firstbrush[i].colbrushf);
	}
}

/*
void R_Q3BSP_DrawFakeShadow(entity_render_t *ent)
{
}
*/

/*
void R_Q3BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius)
{
}
*/

void R_Q3BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz)
{
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
	Cvar_RegisterVariable(&r_surfaceworldnode);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonoffset);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

