
#include "quakedef.h"

unsigned int palette_complete[256];
unsigned int palette_nofullbrights[256];
unsigned int palette_onlyfullbrights[256];
unsigned int palette_nocolormapnofullbrights[256];
unsigned int palette_nocolormap[256];
unsigned int palette_pantsaswhite[256];
unsigned int palette_shirtaswhite[256];
unsigned int palette_alpha[256];
unsigned int palette_font[256];

qbyte host_basepal[768];

void Palette_Setup8to24(void)
{
	int i;
	int fullbright_start, fullbright_end;
	int pants_start, pants_end;
	int shirt_start, shirt_end;
	int reversed_start, reversed_end;
	qbyte *in, *out;
	qbyte *colormap;

	in = host_basepal;
	out = (qbyte *) palette_complete; // palette is accessed as 32bit for speed reasons, but is created as 8bit bytes
	for (i = 0;i < 255;i++)
	{
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 255;
	}
	palette_complete[255] = 0; // completely transparent black

	// FIXME: fullbright_start should be read from colormap.lmp
	colormap = FS_LoadFile("gfx/colormap.lmp", true);
	if (colormap && fs_filesize >= 16385)
		fullbright_start = 256 - colormap[16384];
	else
		fullbright_start = 256;
	if (colormap)
		Mem_Free(colormap);
	fullbright_end = 256;
	pants_start = 96;
	pants_end = 112;
	shirt_start = 16;
	shirt_end = 32;
	reversed_start = 128;
	reversed_end = 224;

	for (i = 0;i < fullbright_start;i++)
		palette_nofullbrights[i] = palette_complete[i];
	for (i = fullbright_start;i < 255;i++)
		palette_nofullbrights[i] = palette_complete[0];
	palette_nofullbrights[255] = 0;

	for (i = 0;i < 256;i++)
		palette_onlyfullbrights[i] = palette_complete[0];
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_onlyfullbrights[i] = palette_complete[i];
	palette_onlyfullbrights[255] = 0;

	for (i = 0;i < 256;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[i];
	for (i = pants_start;i < pants_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];
	for (i = shirt_start;i < shirt_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];
	palette_nocolormapnofullbrights[255] = 0;

	for (i = 0;i < 256;i++)
		palette_nocolormap[i] = palette_complete[i];
	for (i = pants_start;i < pants_end;i++)
		palette_nocolormap[i] = palette_complete[0];
	for (i = shirt_start;i < shirt_end;i++)
		palette_nocolormap[i] = palette_complete[0];
	palette_nocolormap[255] = 0;

	for (i = 0;i < 256;i++)
		palette_pantsaswhite[i] = palette_complete[0];
	for (i = pants_start;i < pants_end;i++)
	{
		if (i >= reversed_start && i < reversed_end)
			palette_pantsaswhite[i] = palette_complete[15 - (i - pants_start)];
		else
			palette_pantsaswhite[i] = palette_complete[i - pants_start];
	}

	for (i = 0;i < 256;i++)
		palette_shirtaswhite[i] = palette_complete[0];
	for (i = shirt_start;i < shirt_end;i++)
	{
		if (i >= reversed_start && i < reversed_end)
			palette_shirtaswhite[i] = palette_complete[15 - (i - shirt_start)];
		else
			palette_shirtaswhite[i] = palette_complete[i - shirt_start];
	}

	for (i = 0;i < 255;i++)
		palette_alpha[i] = 0xFFFFFFFF;
	palette_alpha[255] = 0;

	palette_font[0] = 0;
	for (i = 1;i < 255;i++)
		palette_font[i] = palette_complete[i];
	palette_font[255] = 0;
}

#if 0
void BuildGammaTable8(float prescale, float gamma, float scale, float base, qbyte *out)
{
	int i, adjusted;
	double invgamma, d;

	gamma = bound(0.1, gamma, 5.0);
	if (gamma == 1) // LordHavoc: dodge the math
	{
		for (i = 0;i < 256;i++)
		{
			adjusted = (int) (i * prescale * scale + base * 255.0);
			out[i] = bound(0, adjusted, 255);
		}
	}
	else
	{
		invgamma = 1.0 / gamma;
		prescale /= 255.0f;
		for (i = 0;i < 256;i++)
		{
			d = pow((double) i * prescale, invgamma) * scale + base;
			adjusted = (int) (255.0 * d);
			out[i] = bound(0, adjusted, 255);
		}
	}
}
#endif

void BuildGammaTable16(float prescale, float gamma, float scale, float base, unsigned short *out)
{
	int i, adjusted;
	double invgamma, d;

	gamma = bound(0.1, gamma, 5.0);
	if (gamma == 1) // LordHavoc: dodge the math
	{
		for (i = 0;i < 256;i++)
		{
			adjusted = (int) (i * 256.0 * prescale * scale + base * 65535.0);
			out[i] = bound(0, adjusted, 65535);
		}
	}
	else
	{
		invgamma = 1.0 / gamma;
		prescale /= 255.0f;
		for (i = 0;i < 256;i++)
		{
			d = pow((double) i * prescale, invgamma) * scale + base;
			adjusted = (int) (65535.0 * d);
			out[i] = bound(0, adjusted, 65535);
		}
	}
}

void Palette_Init(void)
{
	int i;
	float gamma, scale, base;
	qbyte *pal;
	qbyte temp[256];
	pal = (qbyte *)FS_LoadFile ("gfx/palette.lmp", false);
	if (!pal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	memcpy(host_basepal, pal, 765);
	Mem_Free(pal);
	host_basepal[765] = host_basepal[766] = host_basepal[767] = 0; // LordHavoc: force the transparent color to black

	gamma = 1;
	scale = 1;
	base = 0;
	i = COM_CheckParm("-texgamma");
	if (i)
		gamma = atof(com_argv[i + 1]);
	i = COM_CheckParm("-texcontrast");
	if (i)
		scale = atof(com_argv[i + 1]);
	i = COM_CheckParm("-texbrightness");
	if (i)
		base = atof(com_argv[i + 1]);
	gamma = bound(0.01, gamma, 10.0);
	scale = bound(0.01, scale, 10.0);
	base = bound(0, base, 0.95);

	BuildGammaTable8(1.0f, gamma, scale, base, temp);
	for (i = 3;i < 765;i++)
		host_basepal[i] = temp[host_basepal[i]];

	Palette_Setup8to24();
}

