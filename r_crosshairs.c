#include "quakedef.h"

cvar_t crosshair_brightness = {CVAR_SAVE, "crosshair_brightness", "1"};
cvar_t crosshair_alpha = {CVAR_SAVE, "crosshair_alpha", "1"};
cvar_t crosshair_flashspeed = {CVAR_SAVE, "crosshair_flashspeed", "2"};
cvar_t crosshair_flashrange = {CVAR_SAVE, "crosshair_flashrange", "0.1"};
cvar_t crosshair_size = {CVAR_SAVE, "crosshair_size", "1"};

// must match NUMCROSSHAIRS in gl_draw.c
#define NUMCROSSHAIRS 5

void R_Crosshairs_Init(void)
{
	Cvar_RegisterVariable(&crosshair_brightness);
	Cvar_RegisterVariable(&crosshair_alpha);
	Cvar_RegisterVariable(&crosshair_flashspeed);
	Cvar_RegisterVariable(&crosshair_flashrange);
	Cvar_RegisterVariable(&crosshair_size);
}

void DrawCrosshair(int num)
{
	int i;
	byte *color;
	float scale, base;
	char *picname;
	cachepic_t *pic;
	if (num < 0 || num >= NUMCROSSHAIRS)
		num = 0;
	if (cl.viewentity)
	{
		i = (cl.scores[cl.viewentity-1].colors & 0xF) << 4;
		if (i >= 208 && i < 224) // blue
			i += 8;
		else if (i < 128 || i >= 224) // 128-224 are backwards ranges (bright to dark, rather than dark to bright)
			i += 15;
	}
	else
		i = 15;
	color = (byte *) &d_8to24table[i];
	if (crosshair_flashspeed.value >= 0.01f)
		base = (sin(realtime * crosshair_flashspeed.value * (M_PI*2.0f)) * crosshair_flashrange.value);
	else
		base = 0.0f;
	scale = crosshair_brightness.value * (1.0f / 255.0f);
	picname = va("gfx/crosshair%i.tga", num + 1);
	pic = Draw_CachePic(picname);
	if (pic)
		DrawQ_Pic((vid.conwidth - pic->width * crosshair_size.value) * 0.5f, (vid.conheight - pic->height * crosshair_size.value) * 0.5f, picname, pic->width * crosshair_size.value, pic->height * crosshair_size.value, color[0] * scale + base, color[1] * scale + base, color[2] * scale + base, crosshair_alpha.value, 0);
}

