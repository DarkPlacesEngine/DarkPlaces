
#include "quakedef.h"

unsigned int d_8to24table[256];
//byte d_15to8table[32768];
byte host_basepal[768];
byte texgamma[256];

static float texture_gamma = 1.0;

cvar_t vid_gamma = {"vid_gamma", "1", true};
cvar_t vid_brightness = {"vid_brightness", "1", true};
cvar_t vid_contrast = {"vid_contrast", "1", true};

void Palette_Setup8to24(void)
{
	byte *in, *out;
	unsigned short i;

	in = host_basepal;
	out = (byte *) d_8to24table; // d_8to24table is accessed as 32bit for speed reasons, but is created as 8bit bytes
	for (i=0 ; i<255 ; i++)
	{
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 255;
	}
	d_8to24table[255] = 0; // completely transparent black
}

/*
void	Palette_Setup15to8(void)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		j,k,l;
	unsigned short i;

	for (i = 0;i < 32768;i++)
	{
		r = ((i & 0x001F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v = 0, k = 0, l = 1000000000;v < 256;v++, pal += 4)
		{
			r1 = r - pal[0];
			g1 = g - pal[1];
			b1 = b - pal[2];
			j = r1*r1+g1*g1+b1*b1;
			if (j < l)
			{
				k = v;
				l = j;
			}
		}
		d_15to8table[i] = k;
	}
}
*/

void BuildGammaTable8(float prescale, float gamma, float scale, float base, byte *out)
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

void Texture_Gamma (void)
{
	int i, adjusted;
	double invgamma;

	texture_gamma = 1;
	if ((i = COM_CheckParm("-gamma")))
		texture_gamma = atof(com_argv[i+1]);
	texture_gamma = bound(0.1, texture_gamma, 5.0);

	if (texture_gamma == 1) // LordHavoc: dodge the math
	{
		for (i = 0;i < 256;i++)
			texgamma[i] = i;
	}
	else
	{
		invgamma = 1.0 / texture_gamma;
		for (i = 0;i < 256;i++)
		{
			adjusted = (int) ((255.0 * pow((double) i / 255.0, invgamma)) + 0.5);
			texgamma[i] = bound(0, adjusted, 255);
		}
	}
}

qboolean hardwaregammasupported = false;
void VID_UpdateGamma(qboolean force)
{
	static float cachegamma = -1, cachebrightness = -1, cachecontrast = -1, cachelighthalf = -1;
	if (!force && vid_gamma.value == cachegamma && vid_brightness.value == cachebrightness && vid_contrast.value == cachecontrast && lighthalf == cachelighthalf)
		return;

	if (vid_gamma.value < 0.1)
		Cvar_SetValue("vid_gamma", 0.1);
	if (vid_gamma.value > 5.0)
		Cvar_SetValue("vid_gamma", 5.0);

	if (vid_brightness.value < 1.0)
		Cvar_SetValue("vid_brightness", 1.0);
	if (vid_brightness.value > 5.0)
		Cvar_SetValue("vid_brightness", 5.0);

	if (vid_contrast.value < 0.2)
		Cvar_SetValue("vid_contrast", 0.2);
	if (vid_contrast.value > 1)
		Cvar_SetValue("vid_contrast", 1);

	cachegamma = vid_gamma.value;
	cachebrightness = vid_brightness.value;
	cachecontrast = vid_contrast.value;
	cachelighthalf = lighthalf;

	hardwaregammasupported = VID_SetGamma((cachelighthalf ? 2.0f : 1.0f), cachegamma, cachebrightness * cachecontrast, 1 - cachecontrast);
	if (!hardwaregammasupported)
		Con_Printf("Hardware gamma not supported.\n");
}

void Gamma_Init(void)
{
	Cvar_RegisterVariable(&vid_gamma);
	Cvar_RegisterVariable(&vid_brightness);
	Cvar_RegisterVariable(&vid_contrast);
}

void Palette_Init(void)
{
	byte *pal;
	pal = (byte *)COM_LoadMallocFile ("gfx/palette.lmp", false);
	if (!pal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	memcpy(host_basepal, pal, 765);
	qfree(pal);
	host_basepal[765] = host_basepal[766] = host_basepal[767] = 0; // LordHavoc: force the transparent color to black
	Palette_Setup8to24();
//	Palette_Setup15to8();
	Texture_Gamma();
}
