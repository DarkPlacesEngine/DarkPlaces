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
	for (i = 0;i < MAX_LIGHTSTYLES;i++)
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
	float cscale, scale;
	dlight_t *light;
	rtlight_t *rtlight;
	if (r_coronas.value < (1.0f / 256.0f) && !gl_flashblend.integer)
		return;
	R_Mesh_Matrix(&identitymatrix);
	flag = r_refdef.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	// FIXME: these traces should scan all render entities instead of cl.world
	for (lnum = 0, light = r_shadow_worldlightchain;light;light = light->next, lnum++)
	{
		rtlight = &light->rtlight;
		if (!(rtlight->flags & flag))
			continue;
		if (rtlight->corona * r_coronas.value <= 0)
			continue;
		if (r_shadow_debuglight.integer >= 0 && r_shadow_debuglight.integer != lnum)
			continue;
		cscale = rtlight->corona * r_coronas.value* 0.25f;
		scale = rtlight->radius * rtlight->coronasizescale;
		if (VectorDistance2(rtlight->shadoworigin, r_view.origin) < 32.0f * 32.0f)
			continue;
		if (CL_Move(rtlight->shadoworigin, vec3_origin, vec3_origin, r_view.origin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction < 1)
			continue;
		R_DrawSprite(GL_ONE, GL_ONE, lightcorona, NULL, true, rtlight->shadoworigin, r_view.right, r_view.up, scale, -scale, -scale, scale, rtlight->color[0] * cscale, rtlight->color[1] * cscale, rtlight->color[2] * cscale, 1);
	}
	for (i = 0;i < r_refdef.numlights;i++)
	{
		rtlight = &r_refdef.lights[i];
		if (!(rtlight->flags & flag))
			continue;
		if (rtlight->corona <= 0)
			continue;
		if (VectorDistance2(rtlight->shadoworigin, r_view.origin) < 16.0f * 16.0f)
			continue;
		if (gl_flashblend.integer)
		{
			cscale = rtlight->corona * 1.0f;
			scale = rtlight->radius * rtlight->coronasizescale * 2.0f;
		}
		else
		{
			cscale = rtlight->corona * r_coronas.value* 0.25f;
			scale = rtlight->radius * rtlight->coronasizescale;
		}
		if (VectorLength(rtlight->color) * cscale < (1.0f / 256.0f))
			continue;
		if (CL_Move(rtlight->shadoworigin, vec3_origin, vec3_origin, r_view.origin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction < 1)
			continue;
		R_DrawSprite(GL_ONE, GL_ONE, lightcorona, NULL, true, rtlight->shadoworigin, r_view.right, r_view.up, scale, -scale, -scale, scale, rtlight->color[0] * cscale, rtlight->color[1] * cscale, rtlight->color[2] * cscale, 1);
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
		rtlight_t *light;
		for (i = 0;i < r_refdef.numlights;i++)
		{
			light = &r_refdef.lights[i];
			Matrix4x4_Transform(&light->matrix_worldtolight, p, v);
			f = 1 - VectorLength2(v);
			if (f > 0 && CL_Move(p, vec3_origin, vec3_origin, light->shadoworigin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction == 1)
				VectorMA(ambientcolor, f, light->currentcolor, ambientcolor);
		}
	}
}
