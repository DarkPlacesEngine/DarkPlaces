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

dlight_t r_dlight[MAX_DLIGHTS];
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
R_UpdateLights
==================
*/
void R_UpdateLights(void)
{
	int i, j, k;

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

	r_numdlights = 0;
	c_dlights = 0;

	if (!r_dynamic.integer || !cl_dlights)
		return;

	// TODO: optimize to not scan whole cl_dlights array if possible
	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		if (cl_dlights[i].radius > 0)
		{
			R_RTLight_UpdateFromDLight(&cl_dlights[i].rtlight, &cl_dlights[i], false);
			// FIXME: use pointer instead of copy
			r_dlight[r_numdlights++] = cl_dlights[i];
			c_dlights++; // count every dlight in use
		}
	}
}

void R_DrawCoronas(void)
{
	int i, lnum;
	float cscale, scale, viewdist, dist;
	dlight_t *light;
	if (!r_coronas.integer)
		return;
	R_Mesh_Matrix(&r_identitymatrix);
	viewdist = DotProduct(r_vieworigin, r_viewforward);
	if (r_shadow_realtime_world.integer)
	{
		for (lnum = 0, light = r_shadow_worldlightchain;light;light = light->next, lnum++)
		{
			if (light->rtlight.corona * r_coronas.value > 0 && (r_shadow_debuglight.integer < 0 || r_shadow_debuglight.integer == lnum) && (dist = (DotProduct(light->rtlight.shadoworigin, r_viewforward) - viewdist)) >= 24.0f && CL_TraceLine(light->rtlight.shadoworigin, r_vieworigin, NULL, NULL, true, NULL, SUPERCONTENTS_SOLID) == 1)
			{
				cscale = light->rtlight.corona * r_coronas.value * 0.25f;
				scale = light->rtlight.radius * 0.25f;
				R_DrawSprite(GL_ONE, GL_ONE, lightcorona, true, light->rtlight.shadoworigin, r_viewright, r_viewup, scale, -scale, -scale, scale, light->rtlight.color[0] * cscale, light->rtlight.color[1] * cscale, light->rtlight.color[2] * cscale, 1);
			}
		}
	}
	for (i = 0, light = r_dlight;i < r_numdlights;i++, light++)
	{
		if (light->corona * r_coronas.value > 0 && (dist = (DotProduct(light->origin, r_viewforward) - viewdist)) >= 24.0f && CL_TraceLine(light->origin, r_vieworigin, NULL, NULL, true, NULL, SUPERCONTENTS_SOLID) == 1)
		{
			cscale = light->corona * r_coronas.value * 0.25f;
			scale = light->radius * 0.25f;
			if (gl_flashblend.integer)
			{
				cscale *= 4.0f;
				scale *= 2.0f;
			}
			R_DrawSprite(GL_ONE, GL_ONE, lightcorona, true, light->origin, r_viewright, r_viewup, scale, -scale, -scale, scale, light->color[0] * cscale, light->color[1] * cscale, light->color[2] * cscale, 1);
		}
	}
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

static int lightpvsbytes;
static qbyte lightpvs[(MAX_MAP_LEAFS+7)>>3];

/*
=============
R_MarkLights
=============
*/
static void R_RecursiveMarkLights(entity_render_t *ent, vec3_t lightorigin, dlight_t *light, int bit, int bitindex, mnode_t *node, qbyte *pvs, int pvsbits)
{
	int i;
	mleaf_t *leaf;
	float dist;

	// for comparisons to minimum acceptable light
	while(node->contents >= 0)
	{
		dist = PlaneDiff(lightorigin, node->plane);
		if (dist > light->rtlight.lightmap_cullradius)
			node = node->children[0];
		else
		{
			if (dist >= -light->rtlight.lightmap_cullradius)
				R_RecursiveMarkLights(ent, lightorigin, light, bit, bitindex, node->children[0], pvs, pvsbits);
			node = node->children[1];
		}
	}

	// check if leaf is visible according to pvs
	leaf = (mleaf_t *)node;
	i = leaf->clusterindex;
	if (leaf->nummarksurfaces && (i >= pvsbits || CHECKPVSBIT(pvs, i)))
	{
		int *surfacepvsframes, d, impacts, impactt;
		float sdist, maxdist, dist2, impact[3];
		msurface_t *surf;
		// mark the polygons
		maxdist = light->rtlight.lightmap_cullradius2;
		surfacepvsframes = ent->model->brushq1.surfacepvsframes;
		for (i = 0;i < leaf->nummarksurfaces;i++)
		{
			if (surfacepvsframes[leaf->firstmarksurface[i]] != ent->model->brushq1.pvsframecount)
				continue;
			surf = ent->model->brushq1.surfaces + leaf->firstmarksurface[i];
			dist = sdist = PlaneDiff(lightorigin, surf->plane);
			if (surf->flags & SURF_PLANEBACK)
				dist = -dist;

			if (dist < -0.25f && !(surf->flags & SURF_LIGHTBOTHSIDES))
				continue;

			dist2 = dist * dist;
			if (dist2 >= maxdist)
				continue;

			VectorCopy(lightorigin, impact);
			if (surf->plane->type >= 3)
				VectorMA(impact, -sdist, surf->plane->normal, impact);
			else
				impact[surf->plane->type] -= sdist;

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
				surf->cached_dlight = true;
			}
			surf->dlightbits[bitindex] |= bit;
		}
	}
}

void R_MarkLights(entity_render_t *ent)
{
	int i, bit, bitindex;
	dlight_t *light;
	vec3_t lightorigin;
	if (!gl_flashblend.integer && r_dynamic.integer && ent->model && ent->model->brushq1.num_leafs)
	{
		for (i = 0, light = r_dlight;i < r_numdlights;i++, light++)
		{
			bit = 1 << (i & 31);
			bitindex = i >> 5;
			Matrix4x4_Transform(&ent->inversematrix, light->origin, lightorigin);
			lightpvsbytes = 0;
			if (r_vismarklights.integer && ent->model->brush.FatPVS)
				lightpvsbytes = ent->model->brush.FatPVS(ent->model, lightorigin, 0, lightpvs, sizeof(lightpvs));
			R_RecursiveMarkLights(ent, lightorigin, light, bit, bitindex, ent->model->brushq1.nodes + ent->model->brushq1.hulls[0].firstclipnode, lightpvs, min(lightpvsbytes * 8, ent->model->brush.num_pvsclusters));
		}
	}
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic, const mleaf_t *leaf)
{
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);

	if (!r_fullbright.integer && cl.worldmodel && cl.worldmodel->brush.LightPoint)
	{
		ambientcolor[0] = ambientcolor[1] = ambientcolor[2] = r_ambient.value * (2.0f / 128.0f);
		cl.worldmodel->brush.LightPoint(cl.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	}
	else
		VectorSet(ambientcolor, 1, 1, 1);

	// FIXME: this .lights related stuff needs to be ported into the Mod_Q1BSP code
	if (cl.worldmodel->brushq1.numlights)
	{
		int i;
		vec3_t v;
		float f;
		mlight_t *sl;
		for (i = 0;i < cl.worldmodel->brushq1.numlights;i++)
		{
			sl = cl.worldmodel->brushq1.lights + i;
			if (d_lightstylevalue[sl->style] > 0)
			{
				VectorSubtract (p, sl->origin, v);
				f = ((1.0f / (DotProduct(v, v) * sl->falloff + sl->distbias)) - sl->subtract);
				if (f > 0 && CL_TraceLine(p, sl->origin, NULL, NULL, false, NULL, SUPERCONTENTS_SOLID) == 1)
				{
					f *= d_lightstylevalue[sl->style] * (1.0f / 65536.0f);
					VectorMA(ambientcolor, f, sl->light, ambientcolor);
				}
			}
		}
	}

	if (dynamic)
	{
		int i;
		float f, v[3];
		dlight_t *light;
		// FIXME: this really should handle dlights as diffusecolor/diffusenormal somehow
		for (i = 0;i < r_numdlights;i++)
		{
			light = r_dlight + i;
			VectorSubtract(p, light->origin, v);
			f = DotProduct(v, v);
			if (f < light->rtlight.lightmap_cullradius2 && CL_TraceLine(p, light->origin, NULL, NULL, false, NULL, SUPERCONTENTS_SOLID) == 1)
			{
				f = (1.0f / (f + LIGHTOFFSET)) - light->rtlight.lightmap_subtract;
				VectorMA(ambientcolor, f, light->rtlight.lightmap_light, ambientcolor);
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

int R_LightModel(float *ambient4f, float *diffusecolor, float *diffusenormal, const entity_render_t *ent, float colorr, float colorg, float colorb, float colora, int worldcoords)
{
	int i, j, maxnearlights;
	float v[3], f, mscale, stylescale, intensity, ambientcolor[3], tempdiffusenormal[3];
	nearlight_t *nl;
	mlight_t *sl;
	dlight_t *light;

	nearlights = 0;
	maxnearlights = r_modellights.integer;
	ambient4f[0] = ambient4f[1] = ambient4f[2] = r_ambient.value * (2.0f / 128.0f);
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);
	if (r_fullbright.integer || (ent->effects & EF_FULLBRIGHT))
	{
		// highly rare
		VectorSet(ambient4f, 1, 1, 1);
		maxnearlights = 0;
	}
	else if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value <= 0)
		maxnearlights = 0;
	else
	{
		if (cl.worldmodel && cl.worldmodel->brush.LightPoint)
		{
			cl.worldmodel->brush.LightPoint(cl.worldmodel, ent->origin, ambient4f, diffusecolor, tempdiffusenormal);
			Matrix4x4_Transform3x3(&ent->inversematrix, tempdiffusenormal, diffusenormal);
			VectorNormalize(diffusenormal);
		}
		else
			VectorSet(ambient4f, 1, 1, 1);
	}

	// scale of the model's coordinate space, to alter light attenuation to match
	// make the mscale squared so it can scale the squared distance results
	mscale = ent->scale * ent->scale;
	// FIXME: no support for .lights on non-Q1BSP?
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
			light = r_dlight + i;
			VectorCopy(light->origin, v);
			if (v[0] < ent->mins[0]) v[0] = ent->mins[0];if (v[0] > ent->maxs[0]) v[0] = ent->maxs[0];
			if (v[1] < ent->mins[1]) v[1] = ent->mins[1];if (v[1] > ent->maxs[1]) v[1] = ent->maxs[1];
			if (v[2] < ent->mins[2]) v[2] = ent->mins[2];if (v[2] > ent->maxs[2]) v[2] = ent->maxs[2];
			VectorSubtract (v, light->origin, v);
			if (DotProduct(v, v) < light->rtlight.lightmap_cullradius2)
			{
				if (CL_TraceLine(ent->origin, light->origin, NULL, NULL, false, NULL, SUPERCONTENTS_SOLID) != 1)
					continue;
				VectorSubtract (ent->origin, light->origin, v);
				f = ((1.0f / (DotProduct(v, v) + LIGHTOFFSET)) - light->rtlight.lightmap_subtract);
				VectorScale(light->rtlight.lightmap_light, f, ambientcolor);
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
						VectorCopy(light->origin, nl->origin);
					else
					{
						Matrix4x4_Transform(&ent->inversematrix, light->origin, nl->origin);
						/*
						Con_Printf("%i %s : %f %f %f : %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n"
						, rd - r_dlight, ent->model->name
						, light->origin[0], light->origin[1], light->origin[2]
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
					nl->light[0] = light->rtlight.lightmap_light[0] * colorr * 4.0f;
					nl->light[1] = light->rtlight.lightmap_light[1] * colorg * 4.0f;
					nl->light[2] = light->rtlight.lightmap_light[2] * colorb * 4.0f;
					nl->subtract = light->rtlight.lightmap_subtract;
					nl->offset = LIGHTOFFSET;
				}
			}
		}
	}
	ambient4f[0] *= colorr;
	ambient4f[1] *= colorg;
	ambient4f[2] *= colorb;
	ambient4f[3] = colora;
	diffusecolor[0] *= colorr;
	diffusecolor[1] *= colorg;
	diffusecolor[2] *= colorb;
	return nearlights != 0 || DotProduct(diffusecolor, diffusecolor) > 0;
}

void R_LightModel_CalcVertexColors(const float *ambientcolor4f, const float *diffusecolor, const float *diffusenormal, int numverts, const float *vertex3f, const float *normal3f, float *color4f)
{
	int i, j, usediffuse;
	float color[4], v[3], dot, dist2, f, dnormal[3];
	nearlight_t *nl;
	usediffuse = DotProduct(diffusecolor, diffusecolor) > 0;
	// negate the diffuse normal to avoid the need to negate the
	// dotproduct on each vertex
	VectorNegate(diffusenormal, dnormal);
	if (usediffuse)
		VectorNormalize(dnormal);
	// directional shading code here
	for (i = 0;i < numverts;i++, vertex3f += 3, normal3f += 3, color4f += 4)
	{
		VectorCopy4(ambientcolor4f, color);

		// silly directional diffuse shading
		if (usediffuse)
		{
			dot = DotProduct(normal3f, dnormal);
			if (dot > 0)
				VectorMA(color, dot, diffusecolor, color);
		}

		// pretty good lighting
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
	if (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value <= 0)
		return;
	VectorSubtract(ent->origin, ent->entlightsorigin, v);
	if (ent->entlightsframe != (r_framecount - 1) || (realtime > ent->entlightstime && DotProduct(v,v) >= 1.0f))
	{
		ent->entlightstime = realtime + 0.1;
		VectorCopy(ent->origin, ent->entlightsorigin);
		ent->numentlights = 0;
		if (cl.worldmodel)
			for (i = 0, sl = cl.worldmodel->brushq1.lights;i < cl.worldmodel->brushq1.numlights && ent->numentlights < MAXENTLIGHTS;i++, sl++)
				if (CL_TraceLine(ent->origin, sl->origin, NULL, NULL, false, NULL, SUPERCONTENTS_SOLID) == 1)
					ent->entlights[ent->numentlights++] = i;
	}
	ent->entlightsframe = r_framecount;
}

