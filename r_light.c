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

cvar_t r_coronas = {CVAR_SAVE, "r_coronas", "1", "brightness of corona flare effects around certain lights, 0 disables corona effects"};
cvar_t gl_flashblend = {CVAR_SAVE, "gl_flashblend", "0", "render bright coronas for dynamic lights instead of actual lighting, fast but ugly"};

static rtexture_t *lightcorona;
static rtexturepool_t *lighttexturepool;

void r_light_start(void)
{
	float dx, dy;
	int x, y, a;
	unsigned char pixels[32][32][4];
	lighttexturepool = R_AllocTexturePool();
	for (y = 0;y < 32;y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0;x < 32;x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = (int)(((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / (1.0f + 0.2))) * 32.0f / (1.0f / (1.0f + 0.2)));
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
		r_refdef.lightstylevalue[i] = 264;		// normal light value
}

void R_Light_Init(void)
{
	Cvar_RegisterVariable(&r_coronas);
	Cvar_RegisterVariable(&gl_flashblend);
	R_RegisterModule("R_Light", r_light_start, r_light_shutdown, r_light_newmap);
}

void R_DrawCoronas(void)
{
	int i, lnum, flag;
	float cscale, scale, viewdist, dist;
	dlight_t *light;
	if (r_coronas.value < 0.01)
		return;
	R_Mesh_Matrix(&identitymatrix);
	viewdist = DotProduct(r_view.origin, r_view.forward);
	flag = r_refdef.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	for (lnum = 0, light = r_shadow_worldlightchain;light;light = light->next, lnum++)
	{
		if ((light->flags & flag) && light->corona * r_coronas.value > 0 && (r_shadow_debuglight.integer < 0 || r_shadow_debuglight.integer == lnum) && (dist = (DotProduct(light->rtlight.shadoworigin, r_view.forward) - viewdist)) >= 24.0f && CL_TraceBox(light->rtlight.shadoworigin, vec3_origin, vec3_origin, r_view.origin, true, NULL, SUPERCONTENTS_SOLID, false).fraction == 1)
		{
			cscale = light->rtlight.corona * r_coronas.value * 0.25f;
			scale = light->rtlight.radius * light->rtlight.coronasizescale;
			R_DrawSprite(GL_ONE, GL_ONE, lightcorona, NULL, true, light->rtlight.shadoworigin, r_view.right, r_view.up, scale, -scale, -scale, scale, light->rtlight.color[0] * cscale, light->rtlight.color[1] * cscale, light->rtlight.color[2] * cscale, 1);
		}
	}
	for (i = 0;i < r_refdef.numlights;i++)
	{
		light = r_refdef.lights[i];
		if ((light->flags & flag) && light->corona * r_coronas.value > 0 && (dist = (DotProduct(light->origin, r_view.forward) - viewdist)) >= 24.0f && CL_TraceBox(light->origin, vec3_origin, vec3_origin, r_view.origin, true, NULL, SUPERCONTENTS_SOLID, false).fraction == 1)
		{
			cscale = light->corona * r_coronas.value * 0.25f;
			scale = light->rtlight.radius * light->rtlight.coronasizescale;
			if (gl_flashblend.integer)
			{
				cscale *= 4.0f;
				scale *= 2.0f;
			}
			R_DrawSprite(GL_ONE, GL_ONE, lightcorona, NULL, true, light->origin, r_view.right, r_view.up, scale, -scale, -scale, scale, light->color[0] * cscale, light->color[1] * cscale, light->color[2] * cscale, 1);
		}
	}
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic)
{
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);

	if (!r_fullbright.integer && r_refdef.worldmodel && r_refdef.worldmodel->brush.LightPoint)
	{
		ambientcolor[0] = ambientcolor[1] = ambientcolor[2] = r_ambient.value * (2.0f / 128.0f);
		r_refdef.worldmodel->brush.LightPoint(r_refdef.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	}
	else
		VectorSet(ambientcolor, 1, 1, 1);

	if (dynamic)
	{
		int i;
		float f, v[3];
		dlight_t *light;
		for (i = 0;i < r_refdef.numlights;i++)
		{
			light = r_refdef.lights[i];
			Matrix4x4_Transform(&light->rtlight.matrix_worldtolight, p, v);
			f = 1 - VectorLength2(v);
			if (f > 0 && CL_TraceBox(p, vec3_origin, vec3_origin, light->origin, false, NULL, SUPERCONTENTS_SOLID, false).fraction == 1)
				VectorMA(ambientcolor, f, light->rtlight.currentcolor, ambientcolor);
		}
	}
}
