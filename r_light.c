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

cvar_t r_lightmodels = {"r_lightmodels", "1"};

extern cvar_t gl_transform;

void r_light_start()
{
}

void r_light_shutdown()
{
}

void r_light_newmap()
{
}

void R_Light_Init()
{
	Cvar_RegisterVariable(&r_lightmodels);
	R_RegisterModule("R_Light", r_light_start, r_light_shutdown, r_light_newmap);
}

int	r_dlightframecount;

/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j,k;
	
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.time*10);
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
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
void R_OldMarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, mnode_t *node)
{
	float		ndist, maxdist;
	msurface_t	*surf;
	int			i;

	if (!r_dynamic.value)
		return;

	// for comparisons to minimum acceptable light
	maxdist = light->radius * light->radius;

	// clamp radius to avoid exceeding 32768 entry division table
	if (maxdist > 4194304)
		maxdist = 4194304;

loc0:
	if (node->contents < 0)
		return;

	ndist = PlaneDiff(lightorigin, node->plane);
	
	if (ndist > light->radius)
	{
		if (node->children[0]->contents >= 0) // LordHavoc: save some time by not pushing another stack frame
		{
			node = node->children[0];
			goto loc0;
		}
		return;
	}
	if (ndist < -light->radius)
	{
		if (node->children[1]->contents >= 0) // LordHavoc: save some time by not pushing another stack frame
		{
			node = node->children[1];
			goto loc0;
		}
		return;
	}

	if (node->dlightframe != r_dlightframecount) // not dynamic until now
	{
		node->dlightbits[0] = node->dlightbits[1] = node->dlightbits[2] = node->dlightbits[3] = node->dlightbits[4] = node->dlightbits[5] = node->dlightbits[6] = node->dlightbits[7] = 0;
		node->dlightframe = r_dlightframecount;
	}
	node->dlightbits[bitindex] |= bit;

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		int d;
		float dist, dist2, impact[3];
		dist = ndist;
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;

		if (dist < -0.25f && !(surf->flags & SURF_LIGHTBOTHSIDES))
			continue;

		dist2 = dist * dist;
		if (dist2 >= maxdist)
			continue;

		impact[0] = light->origin[0] - surf->plane->normal[0] * dist;
		impact[1] = light->origin[1] - surf->plane->normal[1] * dist;
		impact[2] = light->origin[2] - surf->plane->normal[2] * dist;

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

		if (surf->dlightframe != r_dlightframecount) // not dynamic until now
		{
			surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits[bitindex] |= bit;

		/*
		if (((surf->flags & SURF_PLANEBACK) == 0) == ((PlaneDist(lightorigin, surf->plane)) >= surf->plane->dist))
		{
			if (surf->dlightframe != r_dlightframecount) // not dynamic until now
			{
				surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
				surf->dlightframe = r_dlightframecount;
			}
			surf->dlightbits[bitindex] |= bit;
		}
		*/
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_OldMarkLights (lightorigin, light, bit, bitindex, node->children[0]);
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

void R_NoVisMarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, model_t *model)
{
	R_OldMarkLights(lightorigin, light, bit, bitindex, model->nodes + model->hulls[0].firstclipnode);
}

int lightframe = 0;
void R_VisMarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, model_t *model)
{
	mleaf_t *pvsleaf = Mod_PointInLeaf (lightorigin, model);

	if (!r_dynamic.value)
		return;

	if (!pvsleaf->compressed_vis)
	{	// no vis info, so make all visible
		R_OldMarkLights(lightorigin, light, bit, bitindex, model->nodes + model->hulls[0].firstclipnode);
		return;
	}
	else
	{
		int		i, k, l, m, c;
		msurface_t *surf, **mark;
		mleaf_t *leaf;
		byte	*in = pvsleaf->compressed_vis;
		int		row = (model->numleafs+7)>>3;
		float	low[3], high[3], radius, dist, maxdist;

		lightframe++;

		radius = light->radius * 2;

		// clamp radius to avoid exceeding 32768 entry division table
		if (radius > 2048)
			radius = 2048;

		low[0] = lightorigin[0] - radius;low[1] = lightorigin[1] - radius;low[2] = lightorigin[2] - radius;
		high[0] = lightorigin[0] + radius;high[1] = lightorigin[1] + radius;high[2] = lightorigin[2] + radius;

		// for comparisons to minimum acceptable light
		maxdist = radius*radius;

		k = 0;
		while (k < row)
		{
			c = *in++;
			if (c)
			{
				l = model->numleafs - (k << 3);
				if (l > 8)
					l = 8;
				for (i=0 ; i<l ; i++)
				{
					if (c & (1<<i))
					{
						leaf = &model->leafs[(k << 3)+i+1];
						leaf->lightframe = lightframe;
						if (leaf->visframe != r_visframecount)
							continue;
						if (leaf->contents == CONTENTS_SOLID)
							continue;
						// if out of the light radius, skip
						if (leaf->minmaxs[0] > high[0] || leaf->minmaxs[3] < low[0]
						 || leaf->minmaxs[1] > high[1] || leaf->minmaxs[4] < low[1]
						 || leaf->minmaxs[2] > high[2] || leaf->minmaxs[5] < low[2])
							continue;
						if (leaf->dlightframe != r_dlightframecount) // not dynamic until now
						{
							leaf->dlightbits[0] = leaf->dlightbits[1] = leaf->dlightbits[2] = leaf->dlightbits[3] = leaf->dlightbits[4] = leaf->dlightbits[5] = leaf->dlightbits[6] = leaf->dlightbits[7] = 0;
							leaf->dlightframe = r_dlightframecount;
						}
						leaf->dlightbits[bitindex] |= bit;
						if ((m = leaf->nummarksurfaces))
						{
							mark = leaf->firstmarksurface;
							do
							{
								surf = *mark++;
								if (surf->visframe != r_framecount || surf->lightframe == lightframe)
									continue;
								surf->lightframe = lightframe;
								dist = PlaneDiff(lightorigin, surf->plane);
								if (surf->flags & SURF_PLANEBACK)
									dist = -dist;
								// LordHavoc: make sure it is infront of the surface and not too far away
								if ((dist >= -0.25f || (surf->flags & SURF_LIGHTBOTHSIDES)) && dist < radius)
								{
									int d;
									float dist2, impact[3];

									dist2 = dist * dist;

									impact[0] = light->origin[0] - surf->plane->normal[0] * dist;
									impact[1] = light->origin[1] - surf->plane->normal[1] * dist;
									impact[2] = light->origin[2] - surf->plane->normal[2] * dist;

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

									if (surf->dlightframe != r_dlightframecount) // not dynamic until now
									{
										surf->dlightbits[0] = surf->dlightbits[1] = surf->dlightbits[2] = surf->dlightbits[3] = surf->dlightbits[4] = surf->dlightbits[5] = surf->dlightbits[6] = surf->dlightbits[7] = 0;
										surf->dlightframe = r_dlightframecount;
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
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	if (!r_dynamic.value)
		return;

	l = cl_dlights;

	for (i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (!l->radius)
			continue;
//		R_MarkLights (l->origin, l, 1<<(i&31), i >> 5, cl.worldmodel->nodes );
		R_VisMarkLights (l->origin, l, 1<<(i&31), i >> 5, cl.worldmodel);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;

extern cvar_t r_ambient;

/*
int RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	vec3_t		mid;

loc0:
	if (node->contents < 0)
		return false;		// didn't hit anything
	
// calculate mid point
	front = PlaneDiff (start, node->plane);
	back = PlaneDiff (end, node->plane);

	// LordHavoc: optimized recursion
	if ((back < 0) == (front < 0))
//		return RecursiveLightPoint (color, node->children[front < 0], start, end);
	{
		node = node->children[front < 0];
		goto loc0;
	}
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side
	if (RecursiveLightPoint (color, node->children[front < 0], start, mid))
		return true;	// hit something
	else
	{
		int i, ds, dt;
		msurface_t *surf;
	// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = node->plane;

		surf = cl.worldmodel->surfaces + node->firstsurface;
		for (i = 0;i < node->numsurfaces;i++, surf++)
		{
			if (surf->flags & SURF_DRAWTILED)
				continue;	// no lightmaps

			ds = (int) ((float) DotProduct (mid, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
			dt = (int) ((float) DotProduct (mid, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;
			
			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];
			
			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			if (surf->samples)
			{
				byte *lightmap;
				int maps, line3, dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float scale;
				line3 = ((surf->extents[0]>>4)+1)*3;

				lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

				for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
				{
					scale = (float) d_lightstylevalue[surf->styles[maps]] * 1.0 / 256.0;
					r00 += (float) lightmap[      0] * scale;g00 += (float) lightmap[      1] * scale;b00 += (float) lightmap[2] * scale;
					r01 += (float) lightmap[      3] * scale;g01 += (float) lightmap[      4] * scale;b01 += (float) lightmap[5] * scale;
					r10 += (float) lightmap[line3+0] * scale;g10 += (float) lightmap[line3+1] * scale;b10 += (float) lightmap[line3+2] * scale;
					r11 += (float) lightmap[line3+3] * scale;g11 += (float) lightmap[line3+4] * scale;b11 += (float) lightmap[line3+5] * scale;
					lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
				}

				color[0] += (float) ((int) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)));
				color[1] += (float) ((int) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)));
				color[2] += (float) ((int) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)));
			}
			return true; // success
		}

	// go down back side
		return RecursiveLightPoint (color, node->children[front >= 0], mid, end);
	}
}

void R_LightPoint (vec3_t color, vec3_t p)
{
	vec3_t		end;
	
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}

  	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	color[0] = color[1] = color[2] = r_ambient.value * 2.0f;
	RecursiveLightPoint (color, cl.worldmodel->nodes, p, end);
}

void SV_LightPoint (vec3_t color, vec3_t p)
{
	vec3_t		end;
	
	if (!sv.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	color[0] = color[1] = color[2] = 0;
	RecursiveLightPoint (color, sv.worldmodel->nodes, p, end);
}
*/

int RecursiveLightPoint (vec3_t color, mnode_t *node, float x, float y, float startz, float endz)
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
			lightspot[0] = x;
			lightspot[1] = y;
			lightspot[2] = mid;
			lightplane = node->plane;

			surf = cl.worldmodel->surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surf++)
			{
				if (surf->flags & SURF_DRAWTILED)
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

					color[0] += (float) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)) * (1.0f / 256.0f);
					color[1] += (float) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)) * (1.0f / 256.0f);
					color[2] += (float) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)) * (1.0f / 256.0f);
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

void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits)
{
	int		i, j, k;
	vec3_t	dist;
	float	brightness, r, f;

	if (!r_dynamic.value || (!dlightbits[0] && !dlightbits[1] && !dlightbits[2] && !dlightbits[3] && !dlightbits[4] && !dlightbits[5] && !dlightbits[6] && !dlightbits[7]))
		return;

	for (j = 0;j < (MAX_DLIGHTS >> 5);j++)
	{
		if (dlightbits[j])
		{
			for (i=0 ; i<32 ; i++)
			{
				if (!((1 << i) & dlightbits[j]))
					continue;
				k = (j<<5)+i;
				if (!cl_dlights[k].radius)
					continue;
				VectorSubtract (org, cl_dlights[k].origin, dist);
				f = DotProduct(dist, dist) + LIGHTOFFSET;
				r = cl_dlights[k].radius*cl_dlights[k].radius;
				if (f < r)
				{
					brightness = r * 128.0f / f;
					color[0] += brightness * cl_dlights[k].color[0];
					color[1] += brightness * cl_dlights[k].color[1];
					color[2] += brightness * cl_dlights[k].color[2];
				}
			}
		}
	}
}

void R_CompleteLightPoint (vec3_t color, vec3_t p, int dynamic)
{
	mleaf_t *leaf;
	leaf = Mod_PointInLeaf(p, cl.worldmodel);
	if (leaf->contents == CONTENTS_SOLID)
	{
		color[0] = color[1] = color[2] = 0;
		return;
	}

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}
	
	color[0] = color[1] = color[2] = r_ambient.value * 2.0f;
	RecursiveLightPoint (color, cl.worldmodel->nodes, p[0], p[1], p[2], p[2] - 65536);

	if (dynamic)
		R_DynamicLightPoint(color, p, leaf->dlightbits);
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

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		dlightbits[0] = dlightbits[1] = dlightbits[2] = dlightbits[3] = dlightbits[4] = dlightbits[5] = dlightbits[6] = dlightbits[7] = 0;
		return;
	}
	
	color[0] = color[1] = color[2] = r_ambient.value * 2.0f;
	RecursiveLightPoint (color, cl.worldmodel->nodes, p[0], p[1], p[2], p[2] - 65536);

	if (leaf->dlightframe == r_dlightframecount)
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

/* // not currently used
void R_DynamicLightPointNoMask(vec3_t color, vec3_t org)
{
	int		i;
	vec3_t	dist;
	float	brightness, r, f;

	if (!r_dynamic.value)
		return;

	for (i=0 ; i<MAX_DLIGHTS ; i++)
	{
		if (!cl_dlights[i].radius)
			continue;
		VectorSubtract (org, cl_dlights[i].origin, dist);
		f = DotProduct(dist, dist) + LIGHTOFFSET;
		r = cl_dlights[i].radius*cl_dlights[i].radius;
		if (f < r)
		{
			brightness = r * 256.0f / f;
			if (cl_dlights[i].dark)
				brightness = -brightness;
			color[0] += brightness * cl_dlights[i].color[0];
			color[1] += brightness * cl_dlights[i].color[1];
			color[2] += brightness * cl_dlights[i].color[2];
		}
	}
}
*/

extern float *aliasvert;
extern float *aliasvertnorm;
extern byte *aliasvertcolor;
extern float modelalpha;
void R_LightModel(entity_t *ent, int numverts, vec3_t center, vec3_t basecolor)
{
	// LordHavoc: warning: reliance on int being 4 bytes here (of course the d_8to24table relies on that too...)
	int i, j, nearlights = 0, color;
	vec3_t dist, mod;
	float t, t1, t2, t3, *avn;
	byte r,g,b,a, *avc;
	struct
	{
		vec3_t color;
		vec3_t origin;
	} nearlight[MAX_DLIGHTS];
	int modeldlightbits[8];
	avc = aliasvertcolor;
	avn = aliasvertnorm;
	a = (byte) bound((int) 0, (int) (modelalpha * 255.0f), (int) 255);
	if (lighthalf)
	{
		mod[0] = ent->colormod[0] * 0.5f;
		mod[1] = ent->colormod[1] * 0.5f;
		mod[2] = ent->colormod[2] * 0.5f;
	}
	else
	{
		mod[0] = ent->colormod[0];
		mod[1] = ent->colormod[1];
		mod[2] = ent->colormod[2];
	}
	if (ent->effects & EF_FULLBRIGHT)
	{
		((byte *)&color)[0] = (byte) (255.0f * mod[0]);
		((byte *)&color)[1] = (byte) (255.0f * mod[1]);
		((byte *)&color)[2] = (byte) (255.0f * mod[2]);
		((byte *)&color)[3] = a;
		for (i = 0;i < numverts;i++)
		{
			*((int *)avc) = color;
			avc += 4;
		}
		return;
	}
	R_ModelLightPoint(basecolor, center, modeldlightbits);

	basecolor[0] *= mod[0];
	basecolor[1] *= mod[1];
	basecolor[2] *= mod[2];
	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		if (!modeldlightbits[i >> 5])
		{
			i |= 31;
			continue;
		}
		if (!(modeldlightbits[i >> 5] & (1 << (i & 31))))
			continue;
		VectorSubtract (center, cl_dlights[i].origin, dist);
		t2 = DotProduct(dist,dist) + LIGHTOFFSET;
		t1 = cl_dlights[i].radius*cl_dlights[i].radius;
		if (t2 < t1)
		{
			// transform the light into the model's coordinate system
			if (gl_transform.value)
				softwareuntransform(cl_dlights[i].origin, nearlight[nearlights].origin);
			else
			{
				VectorCopy(cl_dlights[i].origin, nearlight[nearlights].origin);
			}
			nearlight[nearlights].color[0] = cl_dlights[i].color[0] * t1 * mod[0];
			nearlight[nearlights].color[1] = cl_dlights[i].color[1] * t1 * mod[1];
			nearlight[nearlights].color[2] = cl_dlights[i].color[2] * t1 * mod[2];
			if (r_lightmodels.value && (ent == NULL || ent != cl_dlights[i].ent))
				nearlights++;
			else
			{
				t1 = 1.0f / t2;
				basecolor[0] += nearlight[nearlights].color[0] * t1;
				basecolor[1] += nearlight[nearlights].color[1] * t1;
				basecolor[2] += nearlight[nearlights].color[2] * t1;
			}
		}
	}
	t1 = bound(0, basecolor[0], 255);r = (byte) t1;
	t1 = bound(0, basecolor[1], 255);g = (byte) t1;
	t1 = bound(0, basecolor[2], 255);b = (byte) t1;
	((byte *)&color)[0] = r;
	((byte *)&color)[1] = g;
	((byte *)&color)[2] = b;
	((byte *)&color)[3] = a;
	if (nearlights)
	{
		int temp;
		vec3_t v;
		float *av;
		av = aliasvert;
		if (nearlights == 1)
		{
			for (i = 0;i < numverts;i++)
			{
				VectorSubtract(nearlight[0].origin, av, v);
				t = DotProduct(avn,v);
				if (t > 0)
				{
					t /= (DotProduct(v,v) + LIGHTOFFSET);
					temp = (int) ((float) (basecolor[0] + nearlight[0].color[0] * t));
					avc[0] = bound(0, temp, 255);
					temp = (int) ((float) (basecolor[1] + nearlight[0].color[1] * t));
					avc[1] = bound(0, temp, 255);
					temp = (int) ((float) (basecolor[2] + nearlight[0].color[2] * t));
					avc[2] = bound(0, temp, 255);
					avc[3] = a;
				}
				else
					*((int *)avc) = color;
				avc += 4;
				av += 3;
				avn += 3;
			}
		}
		else
		{
			for (i = 0;i < numverts;i++)
			{
				int lit;
				t1 = basecolor[0];
				t2 = basecolor[1];
				t3 = basecolor[2];
				lit = false;
				for (j = 0;j < nearlights;j++)
				{
					VectorSubtract(nearlight[j].origin, av, v);
					t = DotProduct(avn,v);
					if (t > 0)
					{
						t /= (DotProduct(v,v) + LIGHTOFFSET);
						t1 += nearlight[j].color[0] * t;
						t2 += nearlight[j].color[1] * t;
						t3 += nearlight[j].color[2] * t;
						lit = true;
					}
				}
				if (lit)
				{
					int i1, i2, i3;
					i1 = (int) t1;
					avc[0] = bound(0, i1, 255);
					i2 = (int) t2;
					avc[1] = bound(0, i2, 255);
					i3 = (int) t3;
					avc[2] = bound(0, i3, 255);
					avc[3] = a;
				}
				else // dodge the costly float -> int conversions
					*((int *)avc) = color;
				avc += 4;
				av += 3;
				avn += 3;
			}
		}
	}
	else
	{
		for (i = 0;i < numverts;i++)
		{
			*((int *)avc) = color;
			avc += 4;
		}
	}
}
