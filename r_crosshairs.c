
#include "quakedef.h"
#include "cl_collision.h"

cvar_t crosshair_brightness = {CVAR_SAVE, "crosshair_brightness", "1"};
cvar_t crosshair_alpha = {CVAR_SAVE, "crosshair_alpha", "1"};
cvar_t crosshair_flashspeed = {CVAR_SAVE, "crosshair_flashspeed", "2"};
cvar_t crosshair_flashrange = {CVAR_SAVE, "crosshair_flashrange", "0.1"};
cvar_t crosshair_size = {CVAR_SAVE, "crosshair_size", "1"};
cvar_t crosshair_static = {CVAR_SAVE, "crosshair_static", "1"};

// must match NUMCROSSHAIRS in gl_draw.c
#define NUMCROSSHAIRS 6

void R_Crosshairs_Init(void)
{
	Cvar_RegisterVariable(&crosshair_brightness);
	Cvar_RegisterVariable(&crosshair_alpha);
	Cvar_RegisterVariable(&crosshair_flashspeed);
	Cvar_RegisterVariable(&crosshair_flashrange);
	Cvar_RegisterVariable(&crosshair_size);
	Cvar_RegisterVariable(&crosshair_static);
}

void R_GetCrosshairColor(float *out)
{
	int i;
	qbyte *color;
	float scale, base;
	if (cl.viewentity >= 1 && cl.viewentity <= cl.maxclients)
	{
		i = (cl.scores[cl.viewentity-1].colors & 0xF) << 4;
		if (i >= 208 && i < 224) // blue
			i += 8;
		else if (i < 128 || i >= 224) // 128-224 are backwards ranges (bright to dark, rather than dark to bright)
			i += 15;
	}
	else
		i = 15;
	color = (qbyte *) &palette_complete[i];
	if (crosshair_flashspeed.value >= 0.01f)
		base = (sin(realtime * crosshair_flashspeed.value * (M_PI*2.0f)) * crosshair_flashrange.value);
	else
		base = 0.0f;
	scale = crosshair_brightness.value * (1.0f / 255.0f);
	out[0] = color[0] * scale + base;
	out[1] = color[1] * scale + base;
	out[2] = color[2] * scale + base;
	out[3] = crosshair_alpha.value;

	// clamp the colors and alpha
	out[0] = bound(0, out[0], 1);
	out[1] = bound(0, out[1], 1);
	out[2] = bound(0, out[2], 1);
	out[3] = bound(0, out[3], 1.0f);
}

void R_DrawWorldCrosshair(void)
{
	int num;
	cachepic_t *pic;
	vec3_t v1, v2, spriteorigin;
	vec_t spritescale;
	vec4_t color;
	trace_t trace;
	if (r_letterbox.value)
		return;
	if (crosshair_static.integer)
		return;
	num = crosshair.integer;
	if (num < 1 || num > NUMCROSSHAIRS || cl.intermission)
		return;
	if (!cl.viewentity || !cl_entities[cl.viewentity].state_current.active)
		return;
	pic = Draw_CachePic(va("gfx/crosshair%i.tga", num), true);
	if (!pic)
		return;
	R_GetCrosshairColor(color);

	// trace the shot path up to a certain distance
	VectorCopy(cl_entities[cl.viewentity].render.origin, v1);
	v1[2] += 16; // HACK: this depends on the QC

	// get the forward vector for the gun (not the view)
	AngleVectors(cl.viewangles, v2, NULL, NULL);
	//VectorCopy(r_vieworigin, v1);
	VectorMA(v1, 8192, v2, v2);
	trace = CL_TraceBox(v1, vec3_origin, vec3_origin, v2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY, false);
	spritescale = trace.fraction * (8192.0f / 40.0f) * crosshair_size.value;
	VectorCopy(trace.endpos, spriteorigin);

	// draw the sprite
	R_DrawSprite(GL_ONE, GL_ONE, pic->tex, NULL, true, spriteorigin, r_viewright, r_viewup, spritescale, -spritescale, -spritescale, spritescale, color[0], color[1], color[2], color[3]);
}

void R_Draw2DCrosshair(void)
{
	int num;
	cachepic_t *pic;
	vec4_t color;
	if (r_letterbox.value)
		return;
	if (!crosshair_static.integer)
		return;
	num = crosshair.integer;
	if (num < 1 || num > NUMCROSSHAIRS || cl.intermission)
		return;
	if (!cl.viewentity || !cl_entities[cl.viewentity].state_current.active)
		return;
	pic = Draw_CachePic(va("gfx/crosshair%i.tga", num), true);
	if (pic)
	{
		R_GetCrosshairColor(color);
		DrawQ_Pic((vid_conwidth.integer - pic->width * crosshair_size.value) * 0.5f, (vid_conheight.integer - pic->height * crosshair_size.value) * 0.5f, pic->name, pic->width * crosshair_size.value, pic->height * crosshair_size.value, color[0], color[1], color[2], color[3], 0);
	}
}




