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
// r_light.c

#include "quakedef.h"
#include "cl_collision.h"
#include "r_shadow.h"

rdlight_t r_dlight[MAX_DLIGHTS];
int r_numdlights = 0;

cvar_t r_modellights = {CVAR_SAVE, "r_modellights", "4"};
cvar_t r_vismarklights = {0, "r_vismarklights", "1"};
cvar_t r_coronas = {CVAR_SAVE, "r_coronas", "1"};
cvar_t gl_flashblend = {CVAR_SAVE, "gl_flashblend", "1"};

static rtexture_t *lightcorona;
static rtexturepool_t *lighttexturepool;

void r_light_start(void)
{
	float dx, dy;
	int x, y, a;
	qbyte pixels[32][32][4];
	lighttexturepool = R_AllocTexturePool();
	for (y = 0;y < 32;y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0;x < 32;x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = ((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / (1.0f + 0.2))) * 32.0f / (1.0f / (1.0f + 0.2));
			a = bound(0, a, 255);
			pixels[y][x][0] = a;
			pixels[y][x][1] = a;
			pixels[y][x][2] = a;
			pixels[y][x][3] = 255;
		}
	}
	lightcorona = R_LoadTexture2D(lighttexturepool, "lightcorona", 32, 32, &pixels[0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
}

void r_light_shutdown(void)
{
	lighttexturepool = NULL;
	lightcorona = NULL;
}

void r_light_newmap(void)
{
	int i;
	for (i = 0;i < 256;i++)
		d_lightstylevalue[i] = 264;		// normal light value
}

void R_Light_Init(void)
{
	Cvar_RegisterVariable(&r_modellights);
	Cvar_RegisterVariable(&r_vismarklights);
	Cvar_RegisterVariable(&r_coronas);
	Cvar_RegisterVariable(&gl_flashblend);
	R_RegisterModule("R_Light", r_light_start, r_light_shutdown, r_light_newmap);
}

/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int i, j, k;

//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.time * 10);
	for (j = 0;j < MAX_LIGHTSTYLES;j++)
	{
		if (!cl_lightstyle || !cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k*22;
		d_lightstylevalue[j] = k;
	}
}


void R_BuildLightList(void)
{
	int i;
	dlight_t *cd;
	rdlight_t *rd;

	r_numdlights = 0;
	c_dlights = 0;

	if (!r_dynamic.integer || !cl_dlights)
		return;

	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		cd = cl_dlights + i;
		if (cd->radius <= 0)
			continue;
		rd = &r_dlight[r_numdlights++];
		VectorCopy(cd->origin, rd->origin);
		VectorScale(cd->color, cd->radius * 64.0f, rd->light);
		rd->cullradius2 = DotProduct(rd->light, rd->light) * (0.25f / (64.0f * 64.0f)) + 4096.0f;
		// clamp radius to avoid overflowing division table in lightmap code
		if (rd->cullradius2 > (2048.0f * 2048.0f))
			rd->cullradius2 = (2048.0f * 2048.0f);
		rd->cullradius = sqrt(rd->cullradius2);
		rd->subtract = 1.0f / rd->cullradius2;
		rd->ent = cd->ent;
		c_dlights++; // count every dlight in use
	}
}

void R_DrawCoronas(void)
{
	int i;
	float cscale, scale, viewdist, dist;
	rdlight_t *rd;
	if (!r_coronas.integer)
		return;
	R_Mesh_Matrix(&r_identitymatrix);
	viewdist = DotProduct(r_origin, vpn);
	for (i = 0;i < r_numdlights;i++)
	{
		rd = r_dlight + i;
		dist = (DotProduct(rd->origin, vpn) - viewdist);
		if (dist >= 24.0f && CL_TraceLine(rd->origin, r_origin, NULL, NULL, 0, true, NULL) == 1)
		{
			cscale = (1.0f / 131072.0f);
			scale = rd->cullradius * 0.25f;
			if (gl_flashblend.integer)
			{
				cscale *= 4.0f;
				scale *= 2.0f;
			}
			R_DrawSprite(GL_ONE, GL_ONE, lightcorona, true, rd->origin, vright, vup, scale, -scale, -scale, scale, rd->light[0] * cscale, rd->light[1] * cscale, rd->light[2] * cscale, 1);
		}
	}
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
static void R_OldMarkLights (entity_render_t *ent, vec3_t lightorigin, rdlight_t *rd, int bit, int bitindex, mnode_t *node)
{
	float ndist, maxdist;
	msurface_t *surf;
	int i, *surfacepvsframes;
	int d, impacts, impactt;
	float dist, dist2, impact[3];

	if (!r_dynamic.integer)
		return;

	// for comparisons to minimum acceptable light
	maxdist = rd->cullradius2;

	surfacepvsframes = ent->model->brushq1.surfacepvsframes;
loc0:
	if (node->contents < 0)
		return;

	ndist = PlaneDiff(lightorigin, node->plane);

	if (ndist > rd->cullradius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -rd->cullradius)
	{
		node = node->children[1];
		goto loc0;
	}

// mark the polygons
	surf = ent->model->brushq1.surfaces + node->firstsurface;
	for (i = 0;i < node->numsurfaces;i++, surf++)
	{
		if (surfacepvsframes[surf->number] != ent->model->brushq1.pvsframecount)
			continue;
		dist = ndist;
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;

		if (dist < -0.25f && !(surf->flags & SURF_LIGHTBOTHSIDES))
			continue;

		dist2 = dist * dist;
		if (dist2 >= maxdist)
			continue;

		if (node->plane->type < 3)
		{
			VectorCopy(lightorigin, impact);
			impact[node->plane->type] -= dist;
		}
		else
		{
			impact[0] = lightorigin[0] - surf->plane->normal[0] * dist;
			impact[1] = lightorigin[1] - surf->plane->normal[1] * dist;
			impact[2] = lightorigin[2] - surf->plane->normal[2] * dist;
		}

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];

		d = bound(0, impacts, surf->extents[0] + 16) - impacts;
		dist2 += d * d;
		if (dist2 > maxdist)
			continue;

		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		d = bound(0, impactt, surf->extents[1] + 16) - impactt;
		dist2 += d * d;
		if (dist2 > maxdist)
			continue;

		if (surf->dlightframe != r_framecount) // not dynamic until now
		{
			surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
			surf->dlightframe = r_framecount;
			if (r_dlightmap.integer)
				surf->cached_dlight = true;
		}
		surf->dlightbits[bitindex] |= bit;
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_OldMarkLights (ent, lightorigin, rd, bit, bitindex, node->children[0]);
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


static void R_VisMarkLights (entity_render_t *ent, rdlight_t *rd, int bit, int bitindex)
{
	static int lightframe = 0;
	mleaf_t *pvsleaf;
	vec3_t lightorigin;
	model_t *model;
	int i, k, m, c, leafnum, *surfacepvsframes, *mark;
	msurface_t *surf;
	mleaf_t *leaf;
	qbyte *in;
	int row;
	float low[3], high[3], dist, maxdist;

	if (!r_dynamic.integer || !ent->model)
		return;

	Matrix4x4_Transform(&ent->inversematrix, rd->origin, lightorigin);

	model = ent->model;
	pvsleaf = model->brushq1.PointInLeaf(model, lightorigin);
	if (pvsleaf == NULL)
		return;

	in = pvsleaf->compressed_vis;
	if (!r_vismarklights.integer || !in)
	{
		// told not to use pvs, or there's no pvs to use
		R_OldMarkLights(ent, lightorigin, rd, bit, bitindex, model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode);
		return;
	}

	lightframe++;

	low[0] = lightorigin[0] - rd->cullradius;low[1] = lightorigin[1] - rd->cullradius;low[2] = lightorigin[2] - rd->cullradius;
	high[0] = lightorigin[0] + rd->cullradius;high[1] = lightorigin[1] + rd->cullradius;high[2] = lightorigin[2] + rd->cullradius;

	// for comparisons to minimum acceptable light
	maxdist = rd->cullradius2;

	row = (model->brushq1.numleafs+7)>>3;
	surfacepvsframes = model->brushq1.surfacepvsframes;

	k = 0;
	while (k < row)
	{
		c = *in++;
		if (c)
		{
			for (i = 0;i < 8;i++)
			{
				if (c & (1<<i))
				{
					// warning to the clumsy: numleafs is one less than it should be, it only counts leafs with vis bits (skips leaf 0)
					leafnum = (k << 3)+i+1;
					if (leafnum > model->brushq1.numleafs)
						return;
					leaf = &model->brushq1.leafs[leafnum];
					if (leaf->mins[0] > high[0] || leaf->maxs[0] < low[0]
					 || leaf->mins[1] > high[1] || leaf->maxs[1] < low[1]
					 || leaf->mins[2] > high[2] || leaf->maxs[2] < low[2])
						continue;
					if ((m = leaf->nummarksurfaces))
					{
						mark = leaf->firstmarksurface;
						do
						{
							surf = model->brushq1.surfaces + *mark++;
							// if not visible in current frame, or already marked because it was in another leaf we passed, skip
							if (surf->lightframe == lightframe)
								continue;
							surf->lightframe = lightframe;
							if (surfacepvsframes[surf->number] != model->brushq1.pvsframecount)
								continue;
							dist = PlaneDiff(lightorigin, surf->plane);
							if (surf->flags & SURF_PLANEBACK)
								dist = -dist;
							// LordHavoc: make sure it is infront of the surface and not too far away
							if (dist < rd->cullradius && (dist > -0.25f || ((surf->flags & SURF_LIGHTBOTHSIDES) && dist > -rd->cullradius)))
							{
								int d;
								int impacts, impactt;
								float dist2, impact[3];

								dist2 = dist * dist;

								if (surf->plane->type < 3)
								{
									VectorCopy(lightorigin, impact);
									impact[surf->plane->type] -= dist;
								}
								else
								{
									impact[0] = lightorigin[0] - surf->plane->normal[0] * dist;
									impact[1] = lightorigin[1] - surf->plane->normal[1] * dist;
									impact[2] = lightorigin[2] - surf->plane->normal[2] * dist;
								}

								impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
								d = bound(0, impacts, surf->extents[0] + 16) - impacts;
								dist2 += d * d;
								if (dist2 > maxdist)
									continue;

								impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
								d = bound(0, impactt, surf->extents[1] + 16) - impactt;
								dist2 += d * d;
								if (dist2 > maxdist)
									continue;

								if (surf->dlightframe != r_framecount) // not dynamic until now
								{
									surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
									surf->dlightframe = r_framecount;
									if (r_dlightmap.integer)
										surf->cached_dlight = true;
								}
								surf->dlightbits[bitindex] |= bit;
							}
						}
						while (--m);
					}
				}
			}
			k++;
			continue;
		}

		k += *in++;
	}
}

void R_MarkLights(entity_render_t *ent)
{
	int i;
	if (!gl_flashblend.integer)
		for (i = 0;i < r_numdlights;i++)
			R_VisMarkLights (ent, r_dlight + i, 1 << (i & 31), i >> 5);
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static int RecursiveLightPoint (vec3_t color, const mnode_t *node, float x, float y, float startz, float endz)
{
	int side, distz = endz - startz;
	float front, back;
	float mid;

loc0:
	if (node->contents < 0)
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
	if (node->children[side]->contents >= 0 && RecursiveLightPoint (color, node->children[side], x, y, startz, mid))
		return true;	// hit something
	else
	{
		// check for impact on this node
		if (node->numsurfaces)
		{
			int i, ds, dt;
			msurface_t *surf;

			surf = cl.worldmodel->brushq1.surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surf++)
			{
				if (!(surf->flags & SURF_LIGHTMAP))
					continue;	// no lightmaps

				ds = (int) (x * surf->texinfo->vecs[0][0] + y * surf->texinfo->vecs[0][1] + mid * surf->texinfo->vecs[0][2] + surf->texinfo->vecs[0][3]);
				dt = (int) (x * surf->texinfo->vecs[1][0] + y * surf->texinfo->vecs[1][1] + mid * surf->texinfo->vecs[1][2] + surf->texinfo->vecs[1][3]);

				if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
					continue;

				ds -= surf->texturemins[0];
				dt -= surf->texturemins[1];

				if (ds > surf->extents[0] || dt > surf->extents[1])
					continue;

				if (surf->samples)
				{
					qbyte *lightmap;
					int maps, line3, size3, dsfrac = ds & 15, dtfrac = dt & 15, scale = 0, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
					line3 = ((surf->extents[0]>>4)+1)*3;
					size3 = ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting

					lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

					for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]];
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

					color[0] += (float) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)) * (1.0f / 32768.0f);
					color[1] += (float) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)) * (1.0f / 32768.0f);
					color[2] += (float) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)) * (1.0f / 32768.0f);
				}
				return true; // success
			}
		}

		// go down back side
		node = node->children[side ^ 1];
		startz = mid;
		distz = endz - startz;
		goto loc0;
	}
}

void R_CompleteLightPoint (vec3_t color, const vec3_t p, int dynamic, const mleaf_t *leaf)
{
	int i;
	vec3_t v;
	float f;
	rdlight_t *rd;
	mlight_t *sl;
	if (leaf == NULL && cl.worldmodel != NULL)
		leaf = cl.worldmodel->brushq1.PointInLeaf(cl.worldmodel, p);
	if (!leaf || leaf->contents == CONTENTS_SOLID || r_fullbright.integer || !cl.worldmodel->brushq1.lightdata)
	{
		color[0] = color[1] = color[2] = 1;
		return;
	}

	color[0] = color[1] = color[2] = r_ambient.value * (2.0f / 128.0f);
	if (cl.worldmodel->brushq1.numlights)
	{
		for (i = 0;i < cl.worldmodel->brushq1.numlights;i++)
		{
			sl = cl.worldmodel->brushq1.lights + i;
			if (d_lightstylevalue[sl->style] > 0)
			{
				VectorSubtract (p, sl->origin, v);
				f = ((1.0f / (DotProduct(v, v) * sl->falloff + sl->distbias)) - sl->subtract);
				if (f > 0 && CL_TraceLine(p, sl->origin, NULL, NULL, 0, false, NULL) == 1)
				{
					f *= d_lightstylevalue[sl->style] * (1.0f / 65536.0f);
					VectorMA(color, f, sl->light, color);
				}
			}
		}
	}
	else
		RecursiveLightPoint (color, cl.worldmodel->brushq1.nodes, p[0], p[1], p[2], p[2] - 65536);

	if (dynamic)
	{
		for (i = 0;i < r_numdlights;i++)
		{
			rd = r_dlight + i;
			VectorSubtract (p, rd->origin, v);
			f = DotProduct(v, v);
			if (f < rd->cullradius2 && CL_TraceLine(p, rd->origin, NULL, NULL, 0, false, NULL) == 1)
			{
				f = (1.0f / (f + LIGHTOFFSET)) - rd->subtract;
				VectorMA(color, f, rd->light, color);
			}
		}
	}
}

typedef struct
{
	vec3_t origin;
	//vec_t cullradius2;
	vec3_t light;
	// how much this light would contribute to ambient if replaced
	vec3_t ambientlight;
	vec_t subtract;
	vec_t falloff;
	vec_t offset;
	// used for choosing only the brightest lights
	vec_t intensity;
}
nearlight_t;

static int nearlights;
static nearlight_t nearlight[MAX_DLIGHTS];

int R_LightModel(float *ambient4f, const entity_render_t *ent, float colorr, float colorg, float colorb, float colora, int worldcoords)
{
	int i, j, maxnearlights;
	float v[3], f, mscale, stylescale, intensity, ambientcolor[3];
	nearlight_t *nl;
	mlight_t *sl;
	rdlight_t *rd;
	mleaf_t *leaf;

	nearlights = 0;
	maxnearlights = r_modellights.integer;
	if (r_fullbright.integer || (ent->effects & EF_FULLBRIGHT))
	{
		// highly rare
		ambient4f[0] = colorr;
		ambient4f[1] = colorg;
		ambient4f[2] = colorb;
		ambient4f[3] = colora;
		return false;
	}
	if (r_shadow_realtime_world.integer)
	{
		// user config choice
		ambient4f[0] = r_ambient.value * (2.0f / 128.0f) * colorr;
		ambient4f[1] = r_ambient.value * (2.0f / 128.0f) * colorg;
		ambient4f[2] = r_ambient.value * (2.0f / 128.0f) * colorb;
		ambient4f[3] = colora;
		return false;
	}
	if (maxnearlights == 0)
	{
		// user config choice
		R_CompleteLightPoint (ambient4f, ent->origin, true, NULL);
		ambient4f[0] *= colorr;
		ambient4f[1] *= colorg;
		ambient4f[2] *= colorb;
		ambient4f[3] = colora;
		return false;
	}
	leaf = cl.worldmodel ? cl.worldmodel->brushq1.PointInLeaf(cl.worldmodel, ent->origin) : NULL;
	if (!leaf || leaf->contents == CONTENTS_SOLID || !cl.worldmodel->brushq1.lightdata)
		ambient4f[0] = ambient4f[1] = ambient4f[2] = 1;
	else
	{
		ambient4f[0] = ambient4f[1] = ambient4f[2] = r_ambient.value * (2.0f / 128.0f);
		if (!cl.worldmodel->brushq1.numlights)
			RecursiveLightPoint (ambient4f, cl.worldmodel->brushq1.nodes, ent->origin[0], ent->origin[1], ent->origin[2], ent->origin[2] - 65536);
	}
	// scale of the model's coordinate space, to alter light attenuation to match
	// make the mscale squared so it can scale the squared distance results
	mscale = ent->scale * ent->scale;
	nl = &nearlight[0];
	for (i = 0;i < ent->numentlights;i++)
	{
		sl = cl.worldmodel->brushq1.lights + ent->entlights[i];
		stylescale = d_lightstylevalue[sl->style] * (1.0f / 65536.0f);
		VectorSubtract (ent->origin, sl->origin, v);
		f = ((1.0f / (DotProduct(v, v) * sl->falloff + sl->distbias)) - sl->subtract) * stylescale;
		VectorScale(sl->light, f, ambientcolor);
		intensity = DotProduct(ambientcolor, ambientcolor);
		if (f < 0)
			intensity *= -1.0f;
		if (nearlights < maxnearlights)
			j = nearlights++;
		else
		{
			for (j = 0;j < maxnearlights;j++)
			{
				if (nearlight[j].intensity < intensity)
				{
					if (nearlight[j].intensity > 0)
						VectorAdd(ambient4f, nearlight[j].ambientlight, ambient4f);
					break;
				}
			}
		}
		if (j >= maxnearlights)
		{
			// this light is less significant than all others,
			// add it to ambient
			if (intensity > 0)
				VectorAdd(ambient4f, ambientcolor, ambient4f);
		}
		else
		{
			nl = nearlight + j;
			nl->intensity = intensity;
			// transform the light into the model's coordinate system
			if (worldcoords)
				VectorCopy(sl->origin, nl->origin);
			else
				Matrix4x4_Transform(&ent->inversematrix, sl->origin, nl->origin);
			// integrate mscale into falloff, for maximum speed
			nl->falloff = sl->falloff * mscale;
			VectorCopy(ambientcolor, nl->ambientlight);
			nl->light[0] = sl->light[0] * stylescale * colorr * 4.0f;
			nl->light[1] = sl->light[1] * stylescale * colorg * 4.0f;
			nl->light[2] = sl->light[2] * stylescale * colorb * 4.0f;
			nl->subtract = sl->subtract;
			nl->offset = sl->distbias;
		}
	}
	if (!r_shadow_realtime_dlight.integer)
	{
		for (i = 0;i < r_numdlights;i++)
		{
			rd = r_dlight + i;
			VectorCopy(rd->origin, v);
			if (v[0] < ent->mins[0]) v[0] = ent->mins[0];if (v[0] > ent->maxs[0]) v[0] = ent->maxs[0];
			if (v[1] < ent->mins[1]) v[1] = ent->mins[1];if (v[1] > ent->maxs[1]) v[1] = ent->maxs[1];
			if (v[2] < ent->mins[2]) v[2] = ent->mins[2];if (v[2] > ent->maxs[2]) v[2] = ent->maxs[2];
			VectorSubtract (v, rd->origin, v);
			if (DotProduct(v, v) < rd->cullradius2)
			{
				if (CL_TraceLine(ent->origin, rd->origin, NULL, NULL, 0, false, NULL) != 1)
					continue;
				VectorSubtract (ent->origin, rd->origin, v);
				f = ((1.0f / (DotProduct(v, v) + LIGHTOFFSET)) - rd->subtract);
				VectorScale(rd->light, f, ambientcolor);
				intensity = DotProduct(ambientcolor, ambientcolor);
				if (f < 0)
					intensity *= -1.0f;
				if (nearlights < maxnearlights)
					j = nearlights++;
				else
				{
					for (j = 0;j < maxnearlights;j++)
					{
						if (nearlight[j].intensity < intensity)
						{
							if (nearlight[j].intensity > 0)
								VectorAdd(ambient4f, nearlight[j].ambientlight, ambient4f);
							break;
						}
					}
				}
				if (j >= maxnearlights)
				{
					// this light is less significant than all others,
					// add it to ambient
					if (intensity > 0)
						VectorAdd(ambient4f, ambientcolor, ambient4f);
				}
				else
				{
					nl = nearlight + j;
					nl->intensity = intensity;
					// transform the light into the model's coordinate system
					if (worldcoords)
						VectorCopy(rd->origin, nl->origin);
					else
					{
						Matrix4x4_Transform(&ent->inversematrix, rd->origin, nl->origin);
						/*
						Con_Printf("%i %s : %f %f %f : %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n"
						, rd - r_dlight, ent->model->name
						, rd->origin[0], rd->origin[1], rd->origin[2]
						, nl->origin[0], nl->origin[1], nl->origin[2]
						, ent->inversematrix.m[0][0], ent->inversematrix.m[0][1], ent->inversematrix.m[0][2], ent->inversematrix.m[0][3]
						, ent->inversematrix.m[1][0], ent->inversematrix.m[1][1], ent->inversematrix.m[1][2], ent->inversematrix.m[1][3]
						, ent->inversematrix.m[2][0], ent->inversematrix.m[2][1], ent->inversematrix.m[2][2], ent->inversematrix.m[2][3]
						, ent->inversematrix.m[3][0], ent->inversematrix.m[3][1], ent->inversematrix.m[3][2], ent->inversematrix.m[3][3]);
						*/
					}
					// integrate mscale into falloff, for maximum speed
					nl->falloff = mscale;
					VectorCopy(ambientcolor, nl->ambientlight);
					nl->light[0] = rd->light[0] * colorr * 4.0f;
					nl->light[1] = rd->light[1] * colorg * 4.0f;
					nl->light[2] = rd->light[2] * colorb * 4.0f;
					nl->subtract = rd->subtract;
					nl->offset = LIGHTOFFSET;
				}
			}
		}
	}
	ambient4f[0] *= colorr;
	ambient4f[1] *= colorg;
	ambient4f[2] *= colorb;
	ambient4f[3] = colora;
	return nearlights != 0;
}

void R_LightModel_CalcVertexColors(const float *ambientcolor4f, int numverts, const float *vertex3f, const float *normal3f, float *color4f)
{
	int i, j;
	float color[4], v[3], dot, dist2, f;
	nearlight_t *nl;
	// directional shading code here
	for (i = 0;i < numverts;i++, vertex3f += 3, normal3f += 3, color4f += 4)
	{
		VectorCopy4(ambientcolor4f, color);
		for (j = 0, nl = &nearlight[0];j < nearlights;j++, nl++)
		{
			VectorSubtract(vertex3f, nl->origin, v);
			// first eliminate negative lighting (back side)
			dot = DotProduct(normal3f, v);
			if (dot > 0)
			{
				// we'll need this again later to normalize the dotproduct
				dist2 = DotProduct(v,v);
				// do the distance attenuation math
				f = (1.0f / (dist2 * nl->falloff + nl->offset)) - nl->subtract;
				if (f > 0)
				{
					// we must divide dot by sqrt(dist2) to compensate for
					// the fact we did not normalize v before doing the
					// dotproduct, the result is in the range 0 to 1 (we
					// eliminated negative numbers already)
					f *= dot / sqrt(dist2);
					// blend in the lighting
					VectorMA(color, f, nl->light, color);
				}
			}
		}
		VectorCopy4(color, color4f);
	}
}

void R_UpdateEntLights(entity_render_t *ent)
{
	int i;
	const mlight_t *sl;
	vec3_t v;
	if (r_shadow_realtime_dlight.integer)
		return;
	VectorSubtract(ent->origin, ent->entlightsorigin, v);
	if (ent->entlightsframe != (r_framecount - 1) || (realtime > ent->entlightstime && DotProduct(v,v) >= 1.0f))
	{
		ent->entlightstime = realtime + 0.1;
		VectorCopy(ent->origin, ent->entlightsorigin);
		ent->numentlights = 0;
		if (cl.worldmodel)
			for (i = 0, sl = cl.worldmodel->brushq1.lights;i < cl.worldmodel->brushq1.numlights && ent->numentlights < MAXENTLIGHTS;i++, sl++)
				if (CL_TraceLine(ent->origin, sl->origin, NULL, NULL, 0, false, NULL) == 1)
					ent->entlights[ent->numentlights++] = i;
	}
	ent->entlightsframe = r_framecount;
}
