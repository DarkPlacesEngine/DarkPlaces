#include "quakedef.h"

cvar_t crosshair_brightness = {"crosshair_brightness", "1.0", true};
cvar_t crosshair_alpha = {"crosshair_alpha", "1.0", true};
cvar_t crosshair_flashspeed = {"crosshair_flashspeed", "2", true};
cvar_t crosshair_flashrange = {"crosshair_flashrange", "0.1", true};

#define NUMCROSSHAIRS 1

int crosshairtex[NUMCROSSHAIRS];

char crosshairtex1[16*16] =
	"0000000000000000"
	"0000000000000000"
	"0000000000000000"
	"0003000000003000"
	"0000500000050000"
	"0000070000700000"
	"0000007007000000"
	"0000000000000000"
	"0000000000000000"
	"0000007007000000"
	"0000070000700000"
	"0000500000050000"
	"0003000000003000"
	"0000000000000000"
	"0000000000000000"
	"0000000000000000"
;

void r_crosshairs_start()
{
	int i;
	byte data[64*64][4];
	for (i = 0;i < 16*16;i++)
	{
		data[i][0] = data[i][1] = data[i][2] = 255;
		data[i][3] = (crosshairtex1[i] - '0') * 255 / 7;
	}
	crosshairtex[0] = GL_LoadTexture("crosshair0", 16, 16, &data[0][0], false, true, 4);
}

void r_crosshairs_shutdown()
{
}

void R_Crosshairs_Init()
{
	Cvar_RegisterVariable(&crosshair_brightness);
	Cvar_RegisterVariable(&crosshair_alpha);
	Cvar_RegisterVariable(&crosshair_flashspeed);
	Cvar_RegisterVariable(&crosshair_flashrange);
	R_RegisterModule("R_Crosshairs", r_crosshairs_start, r_crosshairs_shutdown);
}

void DrawCrosshair(int num)
{
	byte *color;
	float scale, base;
//	Draw_Character (r_refdef.vrect.x + r_refdef.vrect.width/2, r_refdef.vrect.y + r_refdef.vrect.height/2, '+');
	if (num < 0 || num >= NUMCROSSHAIRS)
		num = 0;
	if (cl.viewentity)
	{
		int i = (cl.scores[cl.viewentity-1].colors & 0xF) << 4;
		if (i >= 208 && i < 224) // blue
			i += 8;
		else if (i < 128 || i >= 224) // 128-224 are backwards ranges (bright to dark, rather than dark to bright)
			i += 15;
		color = (byte *) &d_8to24table[i];
	}
	else
		color = (byte *) &d_8to24table[15];
	if (crosshair_flashspeed.value >= 0.01f)
//		scale = (sin(realtime * crosshair_flashspeed.value * (M_PI*2.0f)) * crosshair_flashrange.value + 1.0f) * (1.0f / 255.0f);
		base = (sin(realtime * crosshair_flashspeed.value * (M_PI*2.0f)) * crosshair_flashrange.value);
	else
		base = 0.0f;
	scale = crosshair_brightness.value / 255.0f;
	Draw_GenericPic(crosshairtex[num], color[0] * scale + base, color[1] * scale + base, color[2] * scale + base, crosshair_alpha.value, r_refdef.vrect.x + r_refdef.vrect.width * 0.5f - 8.0f, r_refdef.vrect.y + r_refdef.vrect.height * 0.5f - 8.0f, 16.0f, 16.0f);
}

