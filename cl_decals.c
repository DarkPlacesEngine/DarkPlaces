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
	entity_render_t *ent;
	int tex;
	model_t *model;
	int surface;
	float scale;
	vec3_t org;
	vec3_t dir;
	float color[4];
}
decal_t;

static decal_t *cl_decals;
static int cl_currentdecal; // wraps around in decal array, replacing old ones when a new one is needed

static renderdecal_t *cl_renderdecals;

static mempool_t *cl_decal_mempool;

void CL_Decals_Clear(void)
{
	memset(cl_decals, 0, MAX_DECALS * sizeof(decal_t));
	cl_currentdecal = 0;
}

void CL_Decals_Init(void)
{
	cl_decal_mempool = Mem_AllocPool("CL_Decals");
	cl_decals = (decal_t *) Mem_Alloc(cl_decal_mempool, MAX_DECALS * sizeof(decal_t));
	memset(cl_decals, 0, MAX_DECALS * sizeof(decal_t));
	cl_currentdecal = 0;

	// FIXME: r_refdef stuff should be allocated somewhere else?
	r_refdef.decals = cl_renderdecals = Mem_Alloc(cl_decal_mempool, MAX_DECALS * sizeof(renderdecal_t));
}


// these are static globals only to avoid putting unnecessary things on the stack
static vec3_t decalorg, decalbestorg;
static float decalbestdist;
static msurface_t *decalbestsurf;
static entity_render_t *decalbestent, *decalent;
static model_t *decalmodel;
void CL_RecursiveDecalSurface (mnode_t *node)
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
	surf = decalmodel->surfaces + node->firstsurface;
	endsurf = surf + node->numsurfaces;
	for (;surf < endsurf;surf++)
	{
		if (!(surf->flags & SURF_LIGHTMAP))
			continue;

		dist = PlaneDiff(decalorg, surf->plane);
		if (surf->flags & SURF_PLANEBACK)
			dist = -dist;
		if (dist < -1)
			continue;
		if (dist >= decalbestdist)
			continue;

		impact[0] = decalorg[0] - surf->plane->normal[0] * dist;
		impact[1] = decalorg[1] - surf->plane->normal[1] * dist;
		impact[2] = decalorg[2] - surf->plane->normal[2] * dist;

		ds = (int) (DotProduct(impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) - surf->texturemins[0];
		dt = (int) (DotProduct(impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) - surf->texturemins[1];

		if (ds < 0 || dt < 0 || ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		VectorCopy(decalorg, decalbestorg);
		decalbestent = decalent;
		decalbestsurf = surf;
		decalbestdist = dist;
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			CL_RecursiveDecalSurface (node->children[0]);
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

void CL_Decal(vec3_t origin, int tex, float scale, float red, float green, float blue, float alpha)
{
	int i;
	decal_t *decal;

	if (alpha < (1.0f / 255.0f))
		return;

	// find the best surface to place the decal on
	decalbestent = NULL;
	decalbestsurf = NULL;
	decalbestdist = 16;

	decalent = NULL;
	decalmodel = cl.worldmodel;
	Mod_CheckLoaded(decalmodel);
	VectorCopy(origin, decalorg);
	CL_RecursiveDecalSurface (decalmodel->nodes);

	for (i = 1;i < MAX_EDICTS;i++)
	{
		decalent = &cl_entities[i].render;
		decalmodel = decalent->model;
		if (decalmodel && decalmodel->name[0])
		{
			Mod_CheckLoaded(decalmodel);
			if (decalmodel->type == mod_brush)
			{
				softwaretransformforentity(decalent);
				softwareuntransform(origin, decalorg);
				CL_RecursiveDecalSurface (decalmodel->nodes);
			}
		}
	}

	// abort if no suitable surface was found
	if (decalbestsurf == NULL)
		return;

	// grab a decal from the array and advance to the next decal to replace, wrapping to replace an old decal if necessary
	decal = &cl_decals[cl_currentdecal++];
	if (cl_currentdecal >= MAX_DECALS)
		cl_currentdecal = 0;
	memset(decal, 0, sizeof(*decal));

	decal->ent = decalbestent;
	if (decal->ent)
		decal->model = decal->ent->model;
	else
		decal->model = cl.worldmodel;

	decal->tex = tex + 1; // our texture numbers are +1 to make 0 mean invisible
	VectorNegate(decalbestsurf->plane->normal, decal->dir);
	if (decalbestsurf->flags & SURF_PLANEBACK)
		VectorNegate(decal->dir, decal->dir);
	// 0.25 to push it off the surface a bit
	decalbestdist -= 0.25f;
	decal->org[0] = decalbestorg[0] + decal->dir[0] * decalbestdist;
	decal->org[1] = decalbestorg[1] + decal->dir[1] * decalbestdist;
	decal->org[2] = decalbestorg[2] + decal->dir[2] * decalbestdist;
	decal->scale = scale * 0.5f;
	// store the color
	decal->color[0] = red;
	decal->color[1] = green;
	decal->color[2] = blue;
	decal->color[3] = alpha;
	// store the surface information
	decal->surface = decalbestsurf - decal->model->surfaces;
}

void CL_UpdateDecals (void)
{
	int i;
	decal_t *p;
	renderdecal_t *r;

	for (i = 0, p = cl_decals, r = r_refdef.decals;i < MAX_DECALS;i++, p++)
	{
		if (p->tex == 0)
			continue;

		if (p->ent && p->ent->visframe == r_framecount && p->ent->model != p->model)
		{
			p->tex = 0;
			continue;
		}

		r->ent = p->ent;
		r->tex = p->tex - 1; // our texture numbers are +1 to make 0 mean invisible
		r->surface = p->surface;
		r->scale = p->scale;
		VectorCopy(p->org, r->org);
		VectorCopy(p->dir, r->dir);
		VectorCopy4(p->color, r->color);
		r++;
	}
	r_refdef.numdecals = r - r_refdef.decals;
}

