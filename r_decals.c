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

cvar_t r_drawdecals = {0, "r_drawdecals", "1"};

static void r_decals_start(void)
{
}

static void r_decals_shutdown(void)
{
}

static void r_decals_newmap(void)
{
}

void R_Decals_Init(void)
{
	Cvar_RegisterVariable (&r_drawdecals);

	R_RegisterModule("R_Decals", r_decals_start, r_decals_shutdown, r_decals_newmap);
}

static int decalindexarray[2*3] =
{
	0, 1, 2,
	0, 2, 3,
};

void R_DrawDecals (void)
{
	renderdecal_t *r;
	int i, j, lightmapstep, ds, dt;
	float fscale, fr, fg, fb, dist, f, ifog, impact[3], v[3], org[3], dir[3], right[3], up[3], tvertex[4][5];
	particletexture_t *tex;
	byte *lightmap;
	msurface_t *surf;
	rdlight_t *rd;
	rmeshinfo_t m;

	if (!r_drawdecals.integer)
		return;

	ifog = 1;

	Mod_CheckLoaded(cl.worldmodel);

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.numtriangles = 2;
	m.numverts = 4;
	m.index = decalindexarray;
	m.vertex = &tvertex[0][0];
	m.vertexstep = sizeof(float[5]);
	m.tex[0] = R_GetTexture(particlefonttexture);
	m.texcoords[0] = &tvertex[0][3];
	m.texcoordstep[0] = sizeof(float[5]);

	for (i = 0, r = r_refdef.decals;i < r_refdef.numdecals;i++, r++)
	{
		if (r->ent)
		{
			if (r->ent->visframe != r_framecount)
				continue;

			Mod_CheckLoaded(r->ent->model);

			surf = r->ent->model->surfaces + r->surface;

			// skip decals on surfaces that aren't visible in this frame
			if (surf->visframe != r_framecount)
				continue;

			softwaretransformforentity(r->ent);
			softwaretransform(r->org, org);
			softwaretransformdirection(r->dir, dir);

			// do not render if the view origin is behind the decal
			VectorSubtract(org, r_origin, v);
			if (DotProduct(dir, v) < 0)
				continue;
		}
		else
		{
			surf = cl.worldmodel->surfaces + r->surface;

			// skip decals on surfaces that aren't visible in this frame
			if (surf->visframe != r_framecount)
				continue;

			// do not render if the view origin is behind the decal
			VectorSubtract(r->org, r_origin, v);
			if (DotProduct(r->dir, v) < 0)
				continue;

			VectorCopy(r->org, org);
			VectorCopy(r->dir, dir);
		}

		dist = -PlaneDiff(r->org, surf->plane);
		VectorMA(r->org, dist, surf->plane->normal, impact);

		ds = (int) (DotProduct(impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) - surf->texturemins[0];
		dt = (int) (DotProduct(impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) - surf->texturemins[1];

		if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1])
		{
			// this should never happen
			continue;
		}

		if (fogenabled)
			ifog = 1 - exp(fogdensity/DotProduct(v,v));

		tex = &particletexture[r->tex][0];
		VectorVectors(dir, right, up);
		VectorScale(right, r->scale, right);
		VectorScale(up, r->scale, up);
		tvertex[0][0] = org[0] - right[0] - up[0];
		tvertex[0][1] = org[1] - right[1] - up[1];
		tvertex[0][2] = org[2] - right[2] - up[2];
		tvertex[0][3] = tex->s1;
		tvertex[0][4] = tex->t1;
		tvertex[1][0] = org[0] - right[0] + up[0];
		tvertex[1][1] = org[1] - right[1] + up[1];
		tvertex[1][2] = org[2] - right[2] + up[2];
		tvertex[1][3] = tex->s1;
		tvertex[1][4] = tex->t2;
		tvertex[2][0] = org[0] + right[0] + up[0];
		tvertex[2][1] = org[1] + right[1] + up[1];
		tvertex[2][2] = org[2] + right[2] + up[2];
		tvertex[2][3] = tex->s2;
		tvertex[2][4] = tex->t2;
		tvertex[3][0] = org[0] + right[0] - up[0];
		tvertex[3][1] = org[1] + right[1] - up[1];
		tvertex[3][2] = org[2] + right[2] - up[2];
		tvertex[3][3] = tex->s2;
		tvertex[3][4] = tex->t1;

		// lighting
		fr = fg = fb = 0.0f;

		if ((lightmap = surf->samples))
		{
			if (surf->styles[0] != 255)
			{
				lightmap += ((dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4)) * 3;
				fscale = d_lightstylevalue[surf->styles[0]] * (1.0f / 32768.0f);
				fr += lightmap[0] * fscale;
				fg += lightmap[1] * fscale;
				fb += lightmap[2] * fscale;
				if (surf->styles[1] != 255)
				{
					lightmapstep = (((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1)) * 3;
					lightmap += lightmapstep;
					fscale = d_lightstylevalue[surf->styles[1]] * (1.0f / 32768.0f);
					fr += lightmap[0] * fscale;
					fg += lightmap[1] * fscale;
					fb += lightmap[2] * fscale;
					if (surf->styles[2] != 255)
					{
						lightmap += lightmapstep;
						fscale = d_lightstylevalue[surf->styles[2]] * (1.0f / 32768.0f);
						fr += lightmap[0] * fscale;
						fg += lightmap[1] * fscale;
						fb += lightmap[2] * fscale;
						if (surf->styles[3] != 255)
						{
							lightmap += lightmapstep;
							fscale = d_lightstylevalue[surf->styles[3]] * (1.0f / 32768.0f);
							fr += lightmap[0] * fscale;
							fg += lightmap[1] * fscale;
							fb += lightmap[2] * fscale;
						}
					}
				}
			}
		}

		if (surf->dlightframe == r_framecount)
		{
			for (j = 0;j < r_numdlights;j++)
			{
				if (surf->dlightbits[j >> 5] & (1 << (j & 31)))
				{
					rd = &r_dlight[j];
					VectorSubtract(r->org, rd->origin, v);
					dist = DotProduct(v, v) + LIGHTOFFSET;
					if (dist < rd->cullradius2)
					{
						f = (1.0f / dist) - rd->lightsubtract;
						if (f > 0)
						{
							fr += f * rd->light[0];
							fg += f * rd->light[1];
							fb += f * rd->light[2];
						}
					}
				}
			}
		}

		// if the surface is transparent, render as transparent
		m.transparent = !(surf->flags & SURF_CLIPSOLID);
		m.cr = r->color[0] * fr;
		m.cg = r->color[1] * fg;
		m.cb = r->color[2] * fb;
		m.ca = r->color[3];

		if (fogenabled)
		{
			m.cr *= ifog;
			m.cg *= ifog;
			m.cb *= ifog;
		}

		R_Mesh_Draw(&m);
	}

	if (!fogenabled)
		return;

	m.blendfunc2 = GL_ONE;
	m.cr = fogcolor[0];
	m.cg = fogcolor[1];
	m.cb = fogcolor[2];

	for (i = 0, r = r_refdef.decals;i < r_refdef.numdecals;i++, r++)
	{
		if (r->ent)
		{
			if (r->ent->visframe != r_framecount)
				continue;

			Mod_CheckLoaded(r->ent->model);

			surf = r->ent->model->surfaces + r->surface;

			// skip decals on surfaces that aren't visible in this frame
			if (surf->visframe != r_framecount)
				continue;

			softwaretransformforentity(r->ent);
			softwaretransform(r->org, org);
			softwaretransformdirection(r->dir, dir);

			// do not render if the view origin is behind the decal
			VectorSubtract(org, r_origin, v);
			if (DotProduct(dir, v) < 0)
				continue;
		}
		else
		{
			surf = cl.worldmodel->surfaces + r->surface;

			// skip decals on surfaces that aren't visible in this frame
			if (surf->visframe != r_framecount)
				continue;

			// do not render if the view origin is behind the decal
			VectorSubtract(r->org, r_origin, v);
			if (DotProduct(r->dir, v) < 0)
				continue;

			VectorCopy(r->org, org);
			VectorCopy(r->dir, dir);
		}

		m.ca = r->color[3] * exp(fogdensity/DotProduct(v,v));

		if (m.ca >= 0.01f)
		{
			dist = -PlaneDiff(r->org, surf->plane);
			VectorMA(r->org, dist, surf->plane->normal, impact);

			ds = (int) (DotProduct(impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) - surf->texturemins[0];
			dt = (int) (DotProduct(impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) - surf->texturemins[1];

			if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1])
			{
				// this should never happen
				continue;
			}

			tex = &particletexture[r->tex][1];
			VectorVectors(dir, right, up);
			VectorScale(right, r->scale, right);
			VectorScale(up, r->scale, up);
			tvertex[0][0] = org[0] - right[0] - up[0];
			tvertex[0][1] = org[1] - right[1] - up[1];
			tvertex[0][2] = org[2] - right[2] - up[2];
			tvertex[0][3] = tex->s1;
			tvertex[0][4] = tex->t1;
			tvertex[1][0] = org[0] - right[0] + up[0];
			tvertex[1][1] = org[1] - right[1] + up[1];
			tvertex[1][2] = org[2] - right[2] + up[2];
			tvertex[1][3] = tex->s1;
			tvertex[1][4] = tex->t2;
			tvertex[2][0] = org[0] + right[0] + up[0];
			tvertex[2][1] = org[1] + right[1] + up[1];
			tvertex[2][2] = org[2] + right[2] + up[2];
			tvertex[2][3] = tex->s2;
			tvertex[2][4] = tex->t2;
			tvertex[3][0] = org[0] + right[0] - up[0];
			tvertex[3][1] = org[1] + right[1] - up[1];
			tvertex[3][2] = org[2] + right[2] - up[2];
			tvertex[3][3] = tex->s2;
			tvertex[3][4] = tex->t1;

			// if the surface is transparent, render as transparent
			m.transparent = !(surf->flags & SURF_CLIPSOLID);
			R_Mesh_Draw(&m);
		}
	}
}

