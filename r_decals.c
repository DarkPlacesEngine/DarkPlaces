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

#include "quakedef.h"

#define MAX_DECALS 2048

typedef struct decal_s
{
	vec3_t		org;
	vec3_t		direction;
	vec3_t		vert[4];
	byte		color[4];
	rtexture_t	*tex;
	msurface_t	*surface;
	byte		*lightmapaddress;
	int			lightmapstep;
}
decal_t;

decal_t *decals;
int currentdecal; // wraps around in decal array, replacing old ones when a new one is needed

cvar_t r_drawdecals = {"r_drawdecals", "1"};
cvar_t r_decals_lighting = {"r_decals_lighting", "1"};

void r_decals_start()
{
	decals = (decal_t *) qmalloc(MAX_DECALS * sizeof(decal_t));
	memset(decals, 0, MAX_DECALS * sizeof(decal_t));
	currentdecal = 0;
}

void r_decals_shutdown()
{
	qfree(decals);
}

void r_decals_newmap()
{
	memset(decals, 0, MAX_DECALS * sizeof(decal_t));
	currentdecal = 0;
}

void R_Decals_Init()
{
	Cvar_RegisterVariable (&r_drawdecals);
	Cvar_RegisterVariable (&r_decals_lighting);

	R_RegisterModule("R_Decals", r_decals_start, r_decals_shutdown, r_decals_newmap);
}

void R_Decal(vec3_t org, rtexture_t *tex, float scale, int cred, int cgreen, int cblue, int alpha)
{
	int i, ds, dt, bestlightmapofs;
	float bestdist, dist;
	vec3_t impact, right, up;
	decal_t *decal;
//	mleaf_t *leaf;
	msurface_t *surf/*, **mark, **endmark*/, *bestsurf;

	if (alpha < 1)
		return;

//	leaf = Mod_PointInLeaf(org, cl.worldmodel);
//	if (!leaf->nummarksurfaces)
//		return;

//	mark = leaf->firstmarksurface;
//	endmark = mark + leaf->nummarksurfaces;

	// find the best surface to place the decal on
	bestsurf = NULL;
	bestdist = 16;
	bestlightmapofs = 0;
//	while(mark < endmark)
//	{
//		surf = *mark++;
	surf = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i = 0;i < cl.worldmodel->nummodelsurfaces;i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		dist = PlaneDiff(org, surf->plane);
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;
		if (dist < 0)
			continue;
		if (dist >= bestdist)
			continue;

		impact[0] = org[0] - surf->plane->normal[0] * dist;
		impact[1] = org[1] - surf->plane->normal[1] * dist;
		impact[2] = org[2] - surf->plane->normal[2] * dist;

		ds = (int) (DotProduct(impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
		dt = (int) (DotProduct(impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

		if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
			continue;
		
		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		bestsurf = surf;
		bestdist = dist;
		bestlightmapofs = (dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4);
	}
	// abort if no suitable surface was found
	if (bestsurf == NULL)
		return;

	// grab a decal from the array and advance to the next decal to replace, wrapping to replace an old decal if necessary
	decal = decals + currentdecal;
	currentdecal++;
	if (currentdecal >= MAX_DECALS)
		currentdecal = 0;
	decal->tex = tex;
	// reverse direction
	if (bestsurf->flags & SURF_PLANEBACK)
	{
		VectorCopy(bestsurf->plane->normal, decal->direction);
	}
	else
	{
		VectorNegate(bestsurf->plane->normal, decal->direction);
	}
	// - 0.25 to push it off the surface a bit
	decal->org[0] = impact[0] = org[0] + decal->direction[0] * (bestdist - 0.25f);
	decal->org[1] = impact[1] = org[1] + decal->direction[1] * (bestdist - 0.25f);
	decal->org[2] = impact[2] = org[2] + decal->direction[2] * (bestdist - 0.25f);
	// set up the 4 corners
	scale *= 0.5f;
	VectorVectors(decal->direction, right, up);
	decal->vert[0][0] = impact[0] - up[0] * scale - right[0] * scale;
	decal->vert[0][1] = impact[1] - up[1] * scale - right[1] * scale;
	decal->vert[0][2] = impact[2] - up[2] * scale - right[2] * scale;
	decal->vert[1][0] = impact[0] + up[0] * scale - right[0] * scale;
	decal->vert[1][1] = impact[1] + up[1] * scale - right[1] * scale;
	decal->vert[1][2] = impact[2] + up[2] * scale - right[2] * scale;
	decal->vert[2][0] = impact[0] + up[0] * scale + right[0] * scale;
	decal->vert[2][1] = impact[1] + up[1] * scale + right[1] * scale;
	decal->vert[2][2] = impact[2] + up[2] * scale + right[2] * scale;
	decal->vert[3][0] = impact[0] - up[0] * scale + right[0] * scale;
	decal->vert[3][1] = impact[1] - up[1] * scale + right[1] * scale;
	decal->vert[3][2] = impact[2] - up[2] * scale + right[2] * scale;
	// store the color
	decal->color[0] = (byte) bound(0, cred, 255);
	decal->color[1] = (byte) bound(0, cgreen, 255);
	decal->color[2] = (byte) bound(0, cblue, 255);
	decal->color[3] = (byte) bound(0, alpha, 255);
	// store the surface information for lighting
	decal->surface = bestsurf;
	decal->lightmapstep = ((bestsurf->extents[0]>>4)+1) * ((bestsurf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
	if (bestsurf->samples)
		decal->lightmapaddress = bestsurf->samples + bestlightmapofs * 3; // LordHavoc: *3 for colored lighitng
	else
		decal->lightmapaddress = NULL;
}

void R_DrawDecals (void)
{
	decal_t *p;
	int i, j, k, dynamiclight, ir, ig, ib, maps, bits;
	byte br, bg, bb, ba;
	float scale, fr, fg, fb, dist, rad, mindist;
	byte *lightmap;
	vec3_t v;
	msurface_t *surf;
	dlight_t *dl;

	if (!r_drawdecals.value)
		return;

	dynamiclight = (int) r_dynamic.value != 0 && (int) r_decals_lighting.value != 0;

	mindist = DotProduct(r_refdef.vieworg, vpn) + 4.0f;

	for (i = 0, p = decals;i < MAX_DECALS;i++, p++)
	{
		if (p->tex == NULL)
			break;

		// do not render if the decal is behind the view
		if (DotProduct(p->org, vpn) < mindist)
			continue;

		// do not render if the view origin is behind the decal
		VectorSubtract(p->org, r_refdef.vieworg, v);
		if (DotProduct(p->direction, v) < 0)
			continue;

		// get the surface lighting
		surf = p->surface;
		lightmap = p->lightmapaddress;
		fr = fg = fb = 0.0f;
		if (lightmap)
		{
			for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				fr += lightmap[0] * scale;
				fg += lightmap[1] * scale;
				fb += lightmap[2] * scale;
				lightmap += p->lightmapstep;
			}
		}
		fr *= (1.0f / 256.0f);
		fg *= (1.0f / 256.0f);
		fb *= (1.0f / 256.0f);
		// dynamic lighting
		if (dynamiclight)
		{
			if (surf->dlightframe == r_dlightframecount)
			{
				for (j = 0;j < 8;j++)
				{
					bits = surf->dlightbits[j];
					if (bits)
					{
						for (k = 0, dl = cl_dlights + j * 32;bits;k++, dl++)
						{
							if (bits & (1 << k))
							{
								bits -= 1 << k;
								VectorSubtract(p->org, dl->origin, v);
								dist = DotProduct(v, v) + LIGHTOFFSET;
								rad = dl->radius * dl->radius;
								if (dist < rad)
								{
									rad *= 128.0f / dist;
									fr += rad * dl->color[0];
									fg += rad * dl->color[1];
									fb += rad * dl->color[2];
								}
							}
						}
					}
				}
			}
		}
		// apply color to lighting
		ir = (int) (fr * p->color[0] * (1.0f / 128.0f));
		ig = (int) (fg * p->color[1] * (1.0f / 128.0f));
		ib = (int) (fb * p->color[2] * (1.0f / 128.0f));
		// compute byte color
		br = (byte) min(ir, 255);
		bg = (byte) min(ig, 255);
		bb = (byte) min(ib, 255);
		ba = p->color[3];
		// put into transpoly system for sorted drawing later
		transpolybegin(R_GetTexture(p->tex), 0, R_GetTexture(p->tex), TPOLYTYPE_ALPHA);
		transpolyvertub(p->vert[0][0], p->vert[0][1], p->vert[0][2], 0,1,br,bg,bb,ba);
		transpolyvertub(p->vert[1][0], p->vert[1][1], p->vert[1][2], 0,0,br,bg,bb,ba);
		transpolyvertub(p->vert[2][0], p->vert[2][1], p->vert[2][2], 1,0,br,bg,bb,ba);
		transpolyvertub(p->vert[3][0], p->vert[3][1], p->vert[3][2], 1,1,br,bg,bb,ba);
		transpolyend();
	}
}
