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

rdlight_t r_dlight[MAX_DLIGHTS];
int r_numdlights = 0;

cvar_t r_lightmodels = {CVAR_SAVE, "r_lightmodels", "1"};
cvar_t r_vismarklights = {0, "r_vismarklights", "1"};
cvar_t r_lightmodelhardness = {CVAR_SAVE, "r_lightmodelhardness", "0.9"};

static rtexture_t *lightcorona;
static rtexturepool_t *lighttexturepool;

void r_light_start(void)
{
	float dx, dy;
	int x, y, a;
	byte pixels[32][32][4];
	lighttexturepool = R_AllocTexturePool();
	for (y = 0;y < 32;y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0;x < 32;x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = ((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / (1.0f + 0.2))) * 8.0f / (1.0f / (1.0f + 0.2));
			a = bound(0, a, 255);
			pixels[y][x][0] = 255;
			pixels[y][x][1] = 255;
			pixels[y][x][2] = 255;
			pixels[y][x][3] = a;
			/*
			// for testing the size of the corona textures
			if (a == 0)
			{
				pixels[y][x][0] = 255;
				pixels[y][x][1] = 0;
				pixels[y][x][2] = 0;
				pixels[y][x][3] = 255;
			}
			*/
		}
	}
	lightcorona = R_LoadTexture (lighttexturepool, "lightcorona", 32, 32, &pixels[0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_ALPHA);
}

void r_light_shutdown(void)
{
	lighttexturepool = NULL;
	lightcorona = NULL;
}

void r_light_newmap(void)
{
}

void R_Light_Init(void)
{
	Cvar_RegisterVariable(&r_lightmodels);
	Cvar_RegisterVariable(&r_lightmodelhardness);
	Cvar_RegisterVariable(&r_vismarklights);
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
		if (!cl_lightstyle[j].length)
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
	int			i;
	dlight_t	*cd;
	rdlight_t	*rd;

	r_numdlights = 0;
	c_dlights = 0;

	if (!r_dynamic.integer)
		return;

	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		cd = cl_dlights + i;
		if (cd->radius <= 0)
			continue;
		rd = &r_dlight[r_numdlights++];
		VectorCopy(cd->origin, rd->origin);
		VectorScale(cd->color, cd->radius * 128.0f, rd->light);
		rd->cullradius = (1.0f / 128.0f) * sqrt(DotProduct(rd->light, rd->light));
		// clamp radius to avoid overflowing division table in lightmap code
		if (rd->cullradius > 2048.0f)
			rd->cullradius = 2048.0f;
		rd->cullradius2 = rd->cullradius * rd->cullradius;
		rd->lightsubtract = 1.0f / rd->cullradius2;
		rd->ent = cd->ent;
		r_numdlights++;
		c_dlights++; // count every dlight in use
	}
}

static int coronapolyindex[6] = {0, 1, 2, 0, 2, 3};

void R_DrawCoronas(void)
{
	int i;
	rmeshinfo_t m;
	float tvxyz[4][4], tvst[4][2], scale, viewdist, diff[3], dist;
	rdlight_t *rd;
	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.depthdisable = true; // magic
	m.numtriangles = 2;
	m.numverts = 4;
	m.index = coronapolyindex;
	m.vertex = &tvxyz[0][0];
	m.vertexstep = sizeof(float[4]);
	m.tex[0] = R_GetTexture(lightcorona);
	m.texcoords[0] = &tvst[0][0];
	m.texcoordstep[0] = sizeof(float[2]);
	tvst[0][0] = 0;
	tvst[0][1] = 0;
	tvst[1][0] = 0;
	tvst[1][1] = 1;
	tvst[2][0] = 1;
	tvst[2][1] = 1;
	tvst[3][0] = 1;
	tvst[3][1] = 0;
	viewdist = DotProduct(r_origin, vpn);
	for (i = 0;i < r_numdlights;i++)
	{
		rd = r_dlight + i;
		dist = (DotProduct(rd->origin, vpn) - viewdist);
		if (dist >= 24.0f)
		{
			// trace to a point just barely closer to the eye
			VectorSubtract(rd->origin, vpn, diff);
			if (TraceLine(r_origin, diff, NULL, NULL, 0) == 1)
			{
				scale = 1.0f / 65536.0f;//64.0f / (dist * dist + 1024.0f);
				m.cr = rd->light[0] * scale;
				m.cg = rd->light[1] * scale;
				m.cb = rd->light[2] * scale;
				m.ca = 1;
				if (fogenabled)
				{
					VectorSubtract(rd->origin, r_origin, diff);
					m.ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
				}
				// make it larger in the distance to keep a consistent size
				//scale = 0.4f * dist;
				scale = 256.0f;
				tvxyz[0][0] = rd->origin[0] - vright[0] * scale - vup[0] * scale;
				tvxyz[0][1] = rd->origin[1] - vright[1] * scale - vup[1] * scale;
				tvxyz[0][2] = rd->origin[2] - vright[2] * scale - vup[2] * scale;
				tvxyz[1][0] = rd->origin[0] - vright[0] * scale + vup[0] * scale;
				tvxyz[1][1] = rd->origin[1] - vright[1] * scale + vup[1] * scale;
				tvxyz[1][2] = rd->origin[2] - vright[2] * scale + vup[2] * scale;
				tvxyz[2][0] = rd->origin[0] + vright[0] * scale + vup[0] * scale;
				tvxyz[2][1] = rd->origin[1] + vright[1] * scale + vup[1] * scale;
				tvxyz[2][2] = rd->origin[2] + vright[2] * scale + vup[2] * scale;
				tvxyz[3][0] = rd->origin[0] + vright[0] * scale - vup[0] * scale;
				tvxyz[3][1] = rd->origin[1] + vright[1] * scale - vup[1] * scale;
				tvxyz[3][2] = rd->origin[2] + vright[2] * scale - vup[2] * scale;
				R_Mesh_DrawDecal(&m);
			}
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
static void R_OldMarkLights (vec3_t lightorigin, rdlight_t *rd, int bit, int bitindex, mnode_t *node)
{
	float		ndist, maxdist;
	msurface_t	*surf;
	mleaf_t		*leaf;
	int			i;

	if (!r_dynamic.integer)
		return;

	// for comparisons to minimum acceptable light
	maxdist = rd->cullradius2;

loc0:
	if (node->contents < 0)
	{
		if (node->contents != CONTENTS_SOLID)
		{
			leaf = (mleaf_t *)node;
			if (leaf->dlightframe != r_framecount) // not dynamic until now
			{
				leaf->dlightbits[0] = leaf->dlightbits[1] = leaf->dlightbits[2] = leaf->dlightbits[3] = leaf->dlightbits[4] = leaf->dlightbits[5] = leaf->dlightbits[6] = leaf->dlightbits[7] = 0;
				leaf->dlightframe = r_framecount;
			}
			leaf->dlightbits[bitindex] |= bit;
		}
		return;
	}

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
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		int d, impacts, impactt;
		float dist, dist2, impact[3];
		if (surf->visframe != r_framecount)
			continue;
		dist = ndist;
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;

		if (dist < -0.25f && !(surf->flags & SURF_LIGHTBOTHSIDES))
			continue;

		dist2 = dist * dist;
		if (dist2 >= maxdist)
			continue;

		impact[0] = rd->origin[0] - surf->plane->normal[0] * dist;
		impact[1] = rd->origin[1] - surf->plane->normal[1] * dist;
		impact[2] = rd->origin[2] - surf->plane->normal[2] * dist;

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


		/*
		d = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];

		if (d < 0)
		{
			dist2 += d * d;
			if (dist2 >= maxdist)
				continue;
		}
		else
		{
			d -= surf->extents[0] + 16;
			if (d > 0)
			{
				dist2 += d * d;
				if (dist2 >= maxdist)
					continue;
			}
		}

		d = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		if (d < 0)
		{
			dist2 += d * d;
			if (dist2 >= maxdist)
				continue;
		}
		else
		{
			d -= surf->extents[1] + 16;
			if (d > 0)
			{
				dist2 += d * d;
				if (dist2 >= maxdist)
					continue;
			}
		}
		*/

		if (surf->dlightframe != r_framecount) // not dynamic until now
		{
			surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
			surf->dlightframe = r_framecount;
		}
		surf->dlightbits[bitindex] |= bit;

		/*
		if (((surf->flags & SURF_PLANEBACK) == 0) == ((PlaneDist(lightorigin, surf->plane)) >= surf->plane->dist))
		{
			if (surf->dlightframe != r_framecount) // not dynamic until now
			{
				surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
				surf->dlightframe = r_framecount;
			}
			surf->dlightbits[bitindex] |= bit;
		}
		*/
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_OldMarkLights (lightorigin, rd, bit, bitindex, node->children[0]);
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

/*
static void R_NoVisMarkLights (rdlight_t *rd, int bit, int bitindex)
{
	vec3_t lightorigin;
	softwareuntransform(rd->origin, lightorigin);

	R_OldMarkLights(lightorigin, rd, bit, bitindex, currentrenderentity->model->nodes + currentrenderentity->model->hulls[0].firstclipnode);
}
*/

static void R_VisMarkLights (rdlight_t *rd, int bit, int bitindex)
{
	static int lightframe = 0;
	mleaf_t *pvsleaf;
	vec3_t lightorigin;
	model_t *model;
	int		i, k, m, c, leafnum;
	msurface_t *surf, **mark;
	mleaf_t *leaf;
	byte	*in;
	int		row;
	float	low[3], high[3], dist, maxdist;

	if (!r_dynamic.integer)
		return;

	model = currentrenderentity->model;
	softwareuntransform(rd->origin, lightorigin);

	if (!r_vismarklights.integer)
	{
		R_OldMarkLights(lightorigin, rd, bit, bitindex, model->nodes + model->hulls[0].firstclipnode);
		return;
	}

	pvsleaf = Mod_PointInLeaf (lightorigin, model);
	if (pvsleaf == NULL)
	{
		Con_Printf("R_VisMarkLights: NULL leaf??\n");
		R_OldMarkLights(lightorigin, rd, bit, bitindex, model->nodes + model->hulls[0].firstclipnode);
		return;
	}

	in = pvsleaf->compressed_vis;
	if (!in)
	{
		// no vis info, so make all visible
		R_OldMarkLights(lightorigin, rd, bit, bitindex, model->nodes + model->hulls[0].firstclipnode);
		return;
	}

	lightframe++;

	low[0] = lightorigin[0] - rd->cullradius;low[1] = lightorigin[1] - rd->cullradius;low[2] = lightorigin[2] - rd->cullradius;
	high[0] = lightorigin[0] + rd->cullradius;high[1] = lightorigin[1] + rd->cullradius;high[2] = lightorigin[2] + rd->cullradius;

	// for comparisons to minimum acceptable light
	maxdist = rd->cullradius2;

	row = (model->numleafs+7)>>3;

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
					if (leafnum > model->numleafs)
						return;
					leaf = &model->leafs[leafnum];
					if (leaf->visframe != r_framecount
					 || leaf->contents == CONTENTS_SOLID
					 || leaf->mins[0] > high[0] || leaf->maxs[0] < low[0]
					 || leaf->mins[1] > high[1] || leaf->maxs[1] < low[1]
					 || leaf->mins[2] > high[2] || leaf->maxs[2] < low[2])
						continue;
					if (leaf->dlightframe != r_framecount)
					{
						// not dynamic until now
						leaf->dlightbits[0] = leaf->dlightbits[1] = leaf->dlightbits[2] = leaf->dlightbits[3] = leaf->dlightbits[4] = leaf->dlightbits[5] = leaf->dlightbits[6] = leaf->dlightbits[7] = 0;
						leaf->dlightframe = r_framecount;
					}
					leaf->dlightbits[bitindex] |= bit;
					if ((m = leaf->nummarksurfaces))
					{
						mark = leaf->firstmarksurface;
						do
						{
							surf = *mark++;
							// if not visible in current frame, or already marked because it was in another leaf we passed, skip
							if (surf->lightframe == lightframe)
								continue;
							surf->lightframe = lightframe;
							if (surf->visframe != r_framecount)
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

								impact[0] = rd->origin[0] - surf->plane->normal[0] * dist;
								impact[1] = rd->origin[1] - surf->plane->normal[1] * dist;
								impact[2] = rd->origin[2] - surf->plane->normal[2] * dist;

#if 0
								d = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
								if (d < 0)
								{
									dist2 += d * d;
									if (dist2 > maxdist)
										continue;
								}
								else
								{
									d -= surf->extents[0];
									if (d < 0)
									{
										dist2 += d * d;
										if (dist2 > maxdist)
											continue;
									}
								}

								d = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
								if (d < 0)
								{
									dist2 += d * d;
									if (dist2 > maxdist)
										continue;
								}
								else
								{
									d -= surf->extents[1];
									if (d < 0)
									{
										dist2 += d * d;
										if (dist2 > maxdist)
											continue;
									}
								}

#else

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

#endif

								if (surf->dlightframe != r_framecount) // not dynamic until now
								{
									surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
									surf->dlightframe = r_framecount;
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

void R_MarkLights(void)
{
	int i;
	for (i = 0;i < r_numdlights;i++)
		R_VisMarkLights (r_dlight + i, 1 << (i & 31), i >> 5);
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static int RecursiveLightPoint (vec3_t color, mnode_t *node, float x, float y, float startz, float endz)
{
	int		side, distz = endz - startz;
	float	front, back;
	float	mid;

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
//		mid = startz + (endz - startz) * (startz - node->plane->dist) / (startz - endz);
//		mid = startz + distz * (startz - node->plane->dist) / (-distz);
//		mid = startz + (-(startz - node->plane->dist));
//		mid = startz - (startz - node->plane->dist);
//		mid = startz + node->plane->dist - startz;
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
//		mid = startz + (endz - startz) * ((front - node->plane->dist) / ((front - node->plane->dist) - (back - node->plane->dist)));
//		mid = startz + (endz - startz) * ((front - node->plane->dist) / (front - back));
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

			surf = cl.worldmodel->surfaces + node->firstsurface;
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
					byte *lightmap;
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
					// LordHavoc: here's the readable version of the interpolation
					// code, not quite as easy for the compiler to optimize...

					// dsfrac is the X position in the lightmap pixel, * 16
					// dtfrac is the Y position in the lightmap pixel, * 16
					// r00 is top left corner, r01 is top right corner
					// r10 is bottom left corner, r11 is bottom right corner
					// g and b are the same layout.
					// r0 and r1 are the top and bottom intermediate results

					// first we interpolate the top two points, to get the top
					// edge sample
					r0 = (((r01-r00) * dsfrac) >> 4) + r00;
					g0 = (((g01-g00) * dsfrac) >> 4) + g00;
					b0 = (((b01-b00) * dsfrac) >> 4) + b00;
					// then we interpolate the bottom two points, to get the
					// bottom edge sample
					r1 = (((r11-r10) * dsfrac) >> 4) + r10;
					g1 = (((g11-g10) * dsfrac) >> 4) + g10;
					b1 = (((b11-b10) * dsfrac) >> 4) + b10;
					// then we interpolate the top and bottom samples to get the
					// middle sample (the one which was requested)
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
//		return RecursiveLightPoint (color, node->children[side ^ 1], x, y, mid, endz);
	}
}

void R_CompleteLightPoint (vec3_t color, vec3_t p, int dynamic, mleaf_t *leaf)
{
	int	 i, *dlightbits;
	vec3_t dist;
	float f;
	rdlight_t *rd;
	if (leaf == NULL)
		leaf = Mod_PointInLeaf(p, cl.worldmodel);

	if (leaf->contents == CONTENTS_SOLID)
	{
		color[0] = color[1] = color[2] = 0;
		return;
	}

	if (r_fullbright.integer || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 2;
		return;
	}

	color[0] = color[1] = color[2] = r_ambient.value * (2.0f / 128.0f);
	RecursiveLightPoint (color, cl.worldmodel->nodes, p[0], p[1], p[2], p[2] - 65536);

	if (dynamic && leaf->dlightframe == r_framecount)
	{
		dlightbits = leaf->dlightbits;
		for (i = 0;i < r_numdlights;i++)
		{
			if (!(dlightbits[i >> 5] & (1 << (i & 31))))
				continue;
			rd = r_dlight + i;
			VectorSubtract (p, rd->origin, dist);
			f = DotProduct(dist, dist) + LIGHTOFFSET;
			if (f < rd->cullradius2)
			{
				f = (1.0f / f) - rd->lightsubtract;
				if (f > 0)
					VectorMA(color, f, rd->light, color);
			}
		}
	}
}

void R_ModelLightPoint (vec3_t color, vec3_t p, int *dlightbits)
{
	mleaf_t *leaf;
	leaf = Mod_PointInLeaf(p, cl.worldmodel);
	if (leaf->contents == CONTENTS_SOLID)
	{
		color[0] = color[1] = color[2] = 0;
		dlightbits[0] = dlightbits[1] = dlightbits[2] = dlightbits[3] = dlightbits[4] = dlightbits[5] = dlightbits[6] = dlightbits[7] = 0;
		return;
	}

	if (r_fullbright.integer || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 2;
		dlightbits[0] = dlightbits[1] = dlightbits[2] = dlightbits[3] = dlightbits[4] = dlightbits[5] = dlightbits[6] = dlightbits[7] = 0;
		return;
	}

	color[0] = color[1] = color[2] = r_ambient.value * (2.0f / 128.0f);
	RecursiveLightPoint (color, cl.worldmodel->nodes, p[0], p[1], p[2], p[2] - 65536);

	if (leaf->dlightframe == r_framecount)
	{
		dlightbits[0] = leaf->dlightbits[0];
		dlightbits[1] = leaf->dlightbits[1];
		dlightbits[2] = leaf->dlightbits[2];
		dlightbits[3] = leaf->dlightbits[3];
		dlightbits[4] = leaf->dlightbits[4];
		dlightbits[5] = leaf->dlightbits[5];
		dlightbits[6] = leaf->dlightbits[6];
		dlightbits[7] = leaf->dlightbits[7];
	}
	else
		dlightbits[0] = dlightbits[1] = dlightbits[2] = dlightbits[3] = dlightbits[4] = dlightbits[5] = dlightbits[6] = dlightbits[7] = 0;
}

void R_LightModel(int numverts)
{
	int i, j, nearlights = 0;
	float color[3], basecolor[3], v[3], t, *av, *avn, *avc, a, number, f, hardness, hardnessoffset, dist2;
	struct
	{
		vec3_t origin;
		vec_t cullradius2;
		vec3_t light;
		vec_t lightsubtract;
	}
	nearlight[MAX_DLIGHTS], *nl;
	int modeldlightbits[8];
	//staticlight_t *sl;
	a = currentrenderentity->alpha;
	if (currentrenderentity->effects & EF_FULLBRIGHT)
		basecolor[0] = basecolor[1] = basecolor[2] = 1;
	else
	{
		if (r_lightmodels.integer)
		{
			R_ModelLightPoint(basecolor, currentrenderentity->origin, modeldlightbits);

			nl = &nearlight[0];
			/*
			// this code is unused for now
			for (i = 0, sl = staticlight;i < staticlights && nearlights < MAX_DLIGHTS;i++, sl++)
			{
				if (TraceLine(currentrenderentity->origin, sl->origin, NULL, NULL, 0) == 1)
				{
					nl->fadetype = sl->fadetype;
					nl->distancescale = sl->distancescale;
					nl->radius = sl->radius;
					VectorCopy(sl->origin, nl->origin);
					VectorCopy(sl->color, nl->light);
					nl->cullradius2 = 99999999;
					nl->lightsubtract = 0;
					nl++;
					nearlights++;
				}
			}
			*/
			for (i = 0;i < r_numdlights && nearlights < MAX_DLIGHTS;i++)
			{
				if (!(modeldlightbits[i >> 5] & (1 << (i & 31))))
					continue;
				if (currentrenderentity == r_dlight[i].ent)
				{
					f = (1.0f / LIGHTOFFSET) - nl->lightsubtract;
					if (f > 0)
						VectorMA(basecolor, f, r_dlight[i].light, basecolor);
				}
				else
				{
					// convert 0-255 radius coloring to 0-1, while also amplifying the brightness by 16
					//if (TraceLine(currentrenderentity->origin, r_dlight[i].origin, NULL, NULL, 0) == 1)
					{
						// transform the light into the model's coordinate system
						//if (gl_transform.integer)
						//	softwareuntransform(r_dlight[i].origin, nl->origin);
						//else
							VectorCopy(r_dlight[i].origin, nl->origin);
						nl->cullradius2 = r_dlight[i].cullradius2;
						VectorCopy(r_dlight[i].light, nl->light);
						nl->lightsubtract = r_dlight[i].lightsubtract;
						nl++;
						nearlights++;
					}
				}
			}
		}
		else
			R_CompleteLightPoint (basecolor, currentrenderentity->origin, true, NULL);
	}
	avc = aliasvertcolor;
	if (nearlights)
	{
		av = aliasvert;
		avn = aliasvertnorm;
		hardness = r_lightmodelhardness.value;
		hardnessoffset = (1.0f - hardness);
		for (i = 0;i < numverts;i++)
		{
			VectorCopy(basecolor, color);
			for (j = 0, nl = &nearlight[0];j < nearlights;j++, nl++)
			{
				// distance attenuation
				VectorSubtract(nl->origin, av, v);
				dist2 = DotProduct(v,v);
				if (dist2 < nl->cullradius2)
				{
					f = (1.0f / (dist2 + LIGHTOFFSET)) - nl->lightsubtract;
					if (f > 0)
					{
						// directional shading
						#if SLOWMATH
						t = 1.0f / sqrt(dist2);
						#else
						number = DotProduct(v, v);
						*((long *)&t) = 0x5f3759df - ((* (long *) &number) >> 1);
						t = t * (1.5f - (number * 0.5f * t * t));
						#endif
						// DotProduct(avn,v) * t is dotproduct with a normalized v,
						// the hardness variables are for backlighting/shinyness
						f *= DotProduct(avn,v) * t * hardness + hardnessoffset;
						if (f > 0)
							VectorMA(color, f, nl->light, color);
					}
				}
			}

			VectorCopy(color, avc);
			avc[3] = a;
			avc += 4;
			av += 3;
			avn += 3;
		}
	}
	else
	{
		for (i = 0;i < numverts;i++)
		{
			VectorCopy(basecolor, avc);
			avc[3] = a;
			avc += 4;
		}
	}
}
