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

int SV_RecursiveLightPoint (vec3_t color, mnode_t *node, float x, float y, float startz, float endz)
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
	if (node->children[side]->contents >= 0 && SV_RecursiveLightPoint (color, node->children[side], x, y, startz, mid))
		return true;	// hit something
	else
	{
		// check for impact on this node
		if (node->numsurfaces)
		{
			int i, ds, dt;
			msurface_t *surf;

			surf = sv.worldmodel->surfaces + node->firstsurface;
			for (i = 0;i < node->numsurfaces;i++, surf++)
			{
				if (!(surf->flags & SURF_LIGHTMAP))
					continue;

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
						scale = 256; // FIXME: server doesn't know what light styles are doing
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

// LordHavoc: added light checking to the server
void SV_LightPoint (vec3_t color, vec3_t p)
{
	Mod_CheckLoaded(sv.worldmodel);
	if (!sv.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}

	color[0] = color[1] = color[2] = 0;
	SV_RecursiveLightPoint (color, sv.worldmodel->nodes, p[0], p[1], p[2], p[2] - 65536);
}
