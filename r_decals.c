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
	vec2_t		texcoord[4];
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

cvar_t r_drawdecals = {0, "r_drawdecals", "1"};
cvar_t r_decals_lighting = {0, "r_decals_lighting", "1"};

void r_decals_start(void)
{
	decals = (decal_t *) qmalloc(MAX_DECALS * sizeof(decal_t));
	memset(decals, 0, MAX_DECALS * sizeof(decal_t));
	currentdecal = 0;
}

void r_decals_shutdown(void)
{
	qfree(decals);
}

void r_decals_newmap(void)
{
	memset(decals, 0, MAX_DECALS * sizeof(decal_t));
	currentdecal = 0;
}

void R_Decals_Init(void)
{
	Cvar_RegisterVariable (&r_drawdecals);
	Cvar_RegisterVariable (&r_decals_lighting);

	R_RegisterModule("R_Decals", r_decals_start, r_decals_shutdown, r_decals_newmap);
}

// these are static globals only to avoid putting unnecessary things on the stack
static vec3_t decalorg;
static float decalbestdist;
static msurface_t *decalbestsurf;
static int decalbestlightmapofs;
void R_RecursiveDecalSurface (mnode_t *node)
{
	// these are static because only one occurance of them need exist at once, so avoid putting them on the stack
	static float ndist, dist;
	static msurface_t *surf, *endsurf;
	static vec3_t impact;
	static int ds, dt;

loc0:
	if (node->contents < 0)
		return;

	ndist = PlaneDiff(decalorg, node->plane);

	if (ndist > 16)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -16)
	{
		node = node->children[1];
		goto loc0;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	endsurf = surf + node->numsurfaces;
	for (;surf < endsurf;surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		dist = PlaneDiff(decalorg, surf->plane);
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;
		if (dist < 0)
			continue;
		if (dist >= decalbestdist)
			continue;

		impact[0] = decalorg[0] - surf->plane->normal[0] * dist;
		impact[1] = decalorg[1] - surf->plane->normal[1] * dist;
		impact[2] = decalorg[2] - surf->plane->normal[2] * dist;

		ds = (int) (DotProduct(impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
		dt = (int) (DotProduct(impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

		if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
			continue;
		
		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		
		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		decalbestsurf = surf;
		decalbestdist = dist;
		decalbestlightmapofs = (dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4);
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_RecursiveDecalSurface (node->children[0]);
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

void R_Decal(vec3_t org, rtexture_t *tex, float s1, float t1, float s2, float t2, float scale, int cred, int cgreen, int cblue, int alpha)
{
	vec3_t center, right, up;
	decal_t *decal;

	if (alpha < 1)
		return;

	// find the best surface to place the decal on
	decalbestsurf = NULL;
	decalbestdist = 16;
	decalbestlightmapofs = 0;
	VectorCopy(org, decalorg);

	R_RecursiveDecalSurface (cl.worldmodel->nodes);

	// abort if no suitable surface was found
	if (decalbestsurf == NULL)
		return;

	// grab a decal from the array and advance to the next decal to replace, wrapping to replace an old decal if necessary
	decal = decals + currentdecal;
	currentdecal++;
	if (currentdecal >= MAX_DECALS)
		currentdecal = 0;
	decal->tex = tex;
	VectorCopy(decalbestsurf->plane->normal, decal->direction);
	// reverse direction
	if (decalbestsurf->flags & SURF_PLANEBACK)
		VectorNegate(decal->direction, decal->direction);
	VectorNegate(decal->direction, decal->direction);
	// 0.25 to push it off the surface a bit
	decalbestdist -= 0.25f;
	decal->org[0] = center[0] = org[0] + decal->direction[0] * decalbestdist;
	decal->org[1] = center[1] = org[1] + decal->direction[1] * decalbestdist;
	decal->org[2] = center[2] = org[2] + decal->direction[2] * decalbestdist;
	// set up the 4 corners
	scale *= 0.5f;
	VectorVectors(decal->direction, right, up);
	decal->texcoord[0][0] = s1;
	decal->texcoord[0][1] = t1;
	decal->vert[0][0] = center[0] - right[0] * scale - up[0] * scale;
	decal->vert[0][1] = center[1] - right[1] * scale - up[1] * scale;
	decal->vert[0][2] = center[2] - right[2] * scale - up[2] * scale;
	decal->texcoord[1][0] = s1;
	decal->texcoord[1][1] = t2;
	decal->vert[1][0] = center[0] - right[0] * scale + up[0] * scale;
	decal->vert[1][1] = center[1] - right[1] * scale + up[1] * scale;
	decal->vert[1][2] = center[2] - right[2] * scale + up[2] * scale;
	decal->texcoord[2][0] = s2;
	decal->texcoord[2][1] = t2;
	decal->vert[2][0] = center[0] + right[0] * scale + up[0] * scale;
	decal->vert[2][1] = center[1] + right[1] * scale + up[1] * scale;
	decal->vert[2][2] = center[2] + right[2] * scale + up[2] * scale;
	decal->texcoord[3][0] = s2;
	decal->texcoord[3][1] = t1;
	decal->vert[3][0] = center[0] + right[0] * scale - up[0] * scale;
	decal->vert[3][1] = center[1] + right[1] * scale - up[1] * scale;
	decal->vert[3][2] = center[2] + right[2] * scale - up[2] * scale;
	// store the color
	decal->color[0] = (byte) bound(0, cred, 255);
	decal->color[1] = (byte) bound(0, cgreen, 255);
	decal->color[2] = (byte) bound(0, cblue, 255);
	decal->color[3] = (byte) bound(0, alpha, 255);
	// store the surface information for lighting
	decal->surface = decalbestsurf;
	decal->lightmapstep = ((decalbestsurf->extents[0]>>4)+1) * ((decalbestsurf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
	if (decalbestsurf->samples)
		decal->lightmapaddress = decalbestsurf->samples + decalbestlightmapofs * 3; // LordHavoc: *3 for colored lighitng
	else
		decal->lightmapaddress = NULL;
}

void GL_DrawDecals (void)
{
	decal_t *p;
	int i, j, k, dynamiclight, bits, texnum, iscale, ir, ig, ib, lit, cr, cg, cb;
	float /*fscale, */fr, fg, fb, dist, rad, mindist;
	byte *lightmap;
	vec3_t v;
	msurface_t *surf;
	dlight_t *dl;

	if (!r_drawdecals.value)
		return;

	dynamiclight = (int) r_dynamic.value != 0 && (int) r_decals_lighting.value != 0;

	mindist = DotProduct(r_origin, vpn) + 4.0f;

	if (r_render.value)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_BLEND);
//		glShadeModel(GL_FLAT);
		glDepthMask(0); // disable zbuffer updates
		glDisable(GL_ALPHA_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	texnum = -1;

	for (i = 0, p = decals;i < MAX_DECALS;i++, p++)
	{
		if (p->tex == NULL)
			break;
		// skip decals on surfaces that aren't visible in this frame
		if (p->surface->visframe != r_framecount)
			continue;

		// do not render if the decal is behind the view
		if (DotProduct(p->org, vpn) < mindist)
			continue;

		// do not render if the view origin is behind the decal
		VectorSubtract(p->org, r_origin, v);
		if (DotProduct(p->direction, v) < 0)
			continue;

		// get the surface lighting
		surf = p->surface;
		lightmap = p->lightmapaddress;
		// dynamic lighting
		lit = false;
		if (dynamiclight)
		{
			fr = fg = fb = 0.0f;
			if (surf->dlightframe == r_framecount)
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
									lit = true;
								}
							}
						}
					}
				}
			}
		}
		if (lit)
		{
#if SLOWMATH
			ir = fr * 256.0f;
			ig = fg * 256.0f;
			ib = fb * 256.0f;
#else
			fr += 8388608.0f;
			fg += 8388608.0f;
			fb += 8388608.0f;
			ir = (*((long *)&fr) & 0x7FFFFF) << 8;
			ig = (*((long *)&fg) & 0x7FFFFF) << 8;
			ib = (*((long *)&fb) & 0x7FFFFF) << 8;
#endif
		}
		else
			ir = ig = ib = 0;
#if 1
		if (lightmap)
		{
			if (surf->styles[0] != 255)
			{
				iscale = d_lightstylevalue[surf->styles[0]];
				ir += lightmap[0] * iscale;
				ig += lightmap[1] * iscale;
				ib += lightmap[2] * iscale;
				if (surf->styles[1] != 255)
				{
					lightmap += p->lightmapstep;
					iscale = d_lightstylevalue[surf->styles[1]];
					ir += lightmap[0] * iscale;
					ig += lightmap[1] * iscale;
					ib += lightmap[2] * iscale;
					if (surf->styles[2] != 255)
					{
						lightmap += p->lightmapstep;
						iscale = d_lightstylevalue[surf->styles[2]];
						ir += lightmap[0] * iscale;
						ig += lightmap[1] * iscale;
						ib += lightmap[2] * iscale;
						if (surf->styles[3] != 255)
						{
							lightmap += p->lightmapstep;
							iscale = d_lightstylevalue[surf->styles[3]];
							ir += lightmap[0] * iscale;
							ig += lightmap[1] * iscale;
							ib += lightmap[2] * iscale;
						}
					}
				}
			}
		}
#else
		fr = fg = fb = 0.0f;
		if (lightmap)
		{
			if (surf->styles[0] != 255)
			{
				fscale = d_lightstylevalue[surf->styles[0]] * (1.0f / 256.0f);
				fr += lightmap[0] * fscale;
				fg += lightmap[1] * fscale;
				fb += lightmap[2] * fscale;
				if (surf->styles[1] != 255)
				{
					lightmap += p->lightmapstep;
					fscale = d_lightstylevalue[surf->styles[1]] * (1.0f / 256.0f);
					fr += lightmap[0] * fscale;
					fg += lightmap[1] * fscale;
					fb += lightmap[2] * fscale;
					if (surf->styles[2] != 255)
					{
						lightmap += p->lightmapstep;
						fscale = d_lightstylevalue[surf->styles[2]] * (1.0f / 256.0f);
						fr += lightmap[0] * fscale;
						fg += lightmap[1] * fscale;
						fb += lightmap[2] * fscale;
						if (surf->styles[3] != 255)
						{
							lightmap += p->lightmapstep;
							fscale = d_lightstylevalue[surf->styles[3]] * (1.0f / 256.0f);
							fr += lightmap[0] * fscale;
							fg += lightmap[1] * fscale;
							fb += lightmap[2] * fscale;
						}
					}
				}
			}
			/*
			for (j = 0;j < MAXLIGHTMAPS && surf->styles[j] != 255;j++)
			{
				fscale = d_lightstylevalue[surf->styles[j]] * (1.0f / 256.0f);
				fr += lightmap[0] * fscale;
				fg += lightmap[1] * fscale;
				fb += lightmap[2] * fscale;
				lightmap += p->lightmapstep;
			}
			*/
		}
#endif
		/*
		{
			int ir, ig, ib;
			byte br, bg, bb, ba;
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
		*/
		if (r_render.value)
		{
			j = R_GetTexture(p->tex);
			if (texnum != j)
			{
				glEnd();
				texnum = j;
				glBindTexture(GL_TEXTURE_2D, texnum);
				glBegin(GL_QUADS);
			}
			/*
			if (lighthalf)
				glColor4f(fr * p->color[0] * (1.0f / 255.0f / 256.0f), fg * p->color[1] * (1.0f / 255.0f / 256.0f), fb * p->color[2] * (1.0f / 255.0f / 256.0f), p->color[3] * (1.0f / 255.0f));
			else
				glColor4f(fr * p->color[0] * (1.0f / 255.0f / 128.0f), fg * p->color[1] * (1.0f / 255.0f / 128.0f), fb * p->color[2] * (1.0f / 255.0f / 128.0f), p->color[3] * (1.0f / 255.0f));
			*/
			if (lighthalf)
			{
				cr = (ir * p->color[0]) >> 16;
				cg = (ig * p->color[1]) >> 16;
				cb = (ib * p->color[2]) >> 16;
			}
			else
			{
				cr = (ir * p->color[0]) >> 15;
				cg = (ig * p->color[1]) >> 15;
				cb = (ib * p->color[2]) >> 15;
			}
			cr = min(cr, 255);
			cg = min(cg, 255);
			cb = min(cb, 255);
			glColor4ub(cr, cg, cb, p->color[3]);

			glTexCoord2f(p->texcoord[0][0], p->texcoord[0][1]);
			glVertex3fv(p->vert[0]);
			glTexCoord2f(p->texcoord[1][0], p->texcoord[1][1]);
			glVertex3fv(p->vert[1]);
			glTexCoord2f(p->texcoord[2][0], p->texcoord[2][1]);
			glVertex3fv(p->vert[2]);
			glTexCoord2f(p->texcoord[3][0], p->texcoord[3][1]);
			glVertex3fv(p->vert[3]);
		}
	}

	if (r_render.value)
	{
		glEnd();

		glDepthMask(1); // enable zbuffer updates
		glDisable(GL_ALPHA_TEST);
	}
}
