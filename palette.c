
#include "quakedef.h"

unsigned int palette_complete[256];
unsigned int palette_font[256];
unsigned int palette_alpha[256];
unsigned int palette_nocolormap[256];
unsigned int palette_nocolormapnofullbrights[256];
unsigned int palette_nofullbrights[256];
unsigned int palette_onlyfullbrights[256];
unsigned int palette_pantsaswhite[256];
unsigned int palette_shirtaswhite[256];
unsigned int palette_transparent[256];

// John Carmack said the quake palette.lmp can be considered public domain because it is not an important asset to id, so I include it here as a fallback if no external palette file is found.
unsigned char host_quakepal[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,171,171,187,187,187,203,203,203,219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,43,23,63,47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,27,27,39,39,39,51,47,47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,
139,107,107,151,115,115,163,123,123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,7,75,75,11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,0,0,87,0,0,95,0,0,103,0,0,111,0,0,119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,
55,0,75,59,7,87,67,7,95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,87,43,23,99,47,31,115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,223,171,39,239,203,31,255,243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,95,183,135,107,195,147,123,211,163,139,227,179,151,
171,139,163,159,127,151,147,115,135,139,103,123,127,91,111,119,83,99,107,75,87,95,63,75,87,55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,115,159,175,107,143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,19,19,23,11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,123,151,123,111,135,111,95,123,99,83,107,87,71,95,75,59,83,63,
51,67,51,39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,103,87,107,95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,243,27,239,223,23,219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,7,107,83,0,91,71,0,75,55,0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,11,239,19,19,223,27,27,207,35,35,191,43,
43,175,47,47,159,47,47,143,47,47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,7,147,31,7,163,39,11,183,51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,247,211,139,167,123,59,183,155,55,199,195,55,231,227,87,127,191,255,171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,243,147,255,247,199,255,255,255,159,91,83
};

void Palette_SetupSpecialPalettes(void)
{
	int i;
	int fullbright_start, fullbright_end;
	int pants_start, pants_end;
	int shirt_start, shirt_end;
	int reversed_start, reversed_end;
	int transparentcolor;
	unsigned char *colormap;
	fs_offset_t filesize;

	colormap = FS_LoadFile("gfx/colormap.lmp", tempmempool, true, &filesize);
	if (colormap && filesize >= 16385)
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
	transparentcolor = 255;

	for (i = 0;i < 256;i++)
		palette_transparent[i] = palette_complete[i];
	palette_transparent[transparentcolor] = 0;

	for (i = 0;i < fullbright_start;i++)
		palette_nofullbrights[i] = palette_complete[i];
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_nofullbrights[i] = palette_complete[0];

	for (i = 0;i < 256;i++)
		palette_onlyfullbrights[i] = palette_complete[0];
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_onlyfullbrights[i] = palette_complete[i];

	for (i = 0;i < 256;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[i];
	for (i = pants_start;i < pants_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];
	for (i = shirt_start;i < shirt_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[0];

	for (i = 0;i < 256;i++)
		palette_nocolormap[i] = palette_complete[i];
	for (i = pants_start;i < pants_end;i++)
		palette_nocolormap[i] = palette_complete[0];
	for (i = shirt_start;i < shirt_end;i++)
		palette_nocolormap[i] = palette_complete[0];

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

	for (i = 0;i < 256;i++)
		palette_alpha[i] = 0xFFFFFFFF;
	palette_alpha[transparentcolor] = 0;

	for (i = 0;i < 256;i++)
		palette_font[i] = palette_complete[i];
	palette_font[0] = 0;
}

void BuildGammaTable8(float prescale, float gamma, float scale, float base, unsigned char *out, int rampsize)
{
	int i, adjusted;
	double invgamma;

	invgamma = 1.0 / gamma;
	prescale /= (double) (rampsize - 1);
	for (i = 0;i < rampsize;i++)
	{
		adjusted = (int) (255.0 * (pow((double) i * prescale, invgamma) * scale + base) + 0.5);
		out[i] = bound(0, adjusted, 255);
	}
}

void BuildGammaTable16(float prescale, float gamma, float scale, float base, unsigned short *out, int rampsize)
{
	int i, adjusted;
	double invgamma;

	invgamma = 1.0 / gamma;
	prescale /= (double) (rampsize - 1);
	for (i = 0;i < rampsize;i++)
	{
		adjusted = (int) (65535.0 * (pow((double) i * prescale, invgamma) * scale + base) + 0.5);
		out[i] = bound(0, adjusted, 65535);
	}
}

void Palette_Init(void)
{
	int i;
	float gamma, scale, base;
	fs_offset_t filesize;
	unsigned char *in, *out, *palfile;
	unsigned char texturegammaramp[256];

	gamma = 1;
	scale = 1;
	base = 0;
// COMMANDLINEOPTION: Client: -texgamma <number> sets the quake palette gamma, allowing you to make quake textures brighter/darker, not recommended
	i = COM_CheckParm("-texgamma");
	if (i)
		gamma = atof(com_argv[i + 1]);
// COMMANDLINEOPTION: Client: -texcontrast <number> sets the quake palette contrast, allowing you to make quake textures brighter/darker, not recommended
	i = COM_CheckParm("-texcontrast");
	if (i)
		scale = atof(com_argv[i + 1]);
// COMMANDLINEOPTION: Client: -texbrightness <number> sets the quake palette brightness (brightness of black), allowing you to make quake textures brighter/darker, not recommended
	i = COM_CheckParm("-texbrightness");
	if (i)
		base = atof(com_argv[i + 1]);
	gamma = bound(0.01, gamma, 10.0);
	scale = bound(0.01, scale, 10.0);
	base = bound(0, base, 0.95);

	BuildGammaTable8(1.0f, gamma, scale, base, texturegammaramp, 256);

	palfile = (unsigned char *)FS_LoadFile ("gfx/palette.lmp", tempmempool, false, &filesize);
	if (palfile && filesize >= 768)
		in = palfile;
	else
	{
		Con_DPrint("Couldn't load gfx/palette.lmp, falling back on internal palette\n");
		in = host_quakepal;
	}
	out = (unsigned char *) palette_complete; // palette is accessed as 32bit for speed reasons, but is created as 8bit bytes
	for (i = 0;i < 256;i++)
	{
		*out++ = texturegammaramp[*in++];
		*out++ = texturegammaramp[*in++];
		*out++ = texturegammaramp[*in++];
		*out++ = 255;
	}
	if (palfile)
		Mem_Free(palfile);

	Palette_SetupSpecialPalettes();
}

