
#include "quakedef.h"

unsigned int d_8to24table[256];
//qbyte d_15to8table[32768];
qbyte host_basepal[768];
qbyte texgamma[256];

cvar_t v_gamma = {CVAR_SAVE, "v_gamma", "1"};
cvar_t v_contrast = {CVAR_SAVE, "v_contrast", "1"};
cvar_t v_brightness = {CVAR_SAVE, "v_brightness", "0"};
cvar_t v_overbrightbits = {CVAR_SAVE, "v_overbrightbits", "0"};
cvar_t v_hwgamma = {0, "v_hwgamma", "1"};

void Palette_Setup8to24(void)
{
	qbyte *in, *out;
	unsigned short i;

	in = host_basepal;
	out = (qbyte *) d_8to24table; // d_8to24table is accessed as 32bit for speed reasons, but is created as 8bit bytes
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
	qbyte	*pal;
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

qboolean hardwaregammasupported = false;
void VID_UpdateGamma(qboolean force)
{
	static float cachegamma = -1, cachebrightness = -1, cachecontrast = -1;
	static int cacheoverbrightbits = -1, cachehwgamma = -1;

	// LordHavoc: don't mess with gamma tables if running dedicated
	if (cls.state == ca_dedicated)
		return;

	if (!force
	 && v_gamma.value == cachegamma
	 && v_contrast.value == cachecontrast
	 && v_brightness.value == cachebrightness
	 && v_overbrightbits.integer == cacheoverbrightbits
	 && v_hwgamma.value == cachehwgamma)
		return;

	if (v_gamma.value < 0.1)
		Cvar_SetValue("v_gamma", 0.1);
	if (v_gamma.value > 5.0)
		Cvar_SetValue("v_gamma", 5.0);

	if (v_contrast.value < 0.5)
		Cvar_SetValue("v_contrast", 0.5);
	if (v_contrast.value > 5.0)
		Cvar_SetValue("v_contrast", 5.0);

	if (v_brightness.value < 0)
		Cvar_SetValue("v_brightness", 0);
	if (v_brightness.value > 0.8)
		Cvar_SetValue("v_brightness", 0.8);

	cachegamma = v_gamma.value;
	cachecontrast = v_contrast.value;
	cachebrightness = v_brightness.value;
	cacheoverbrightbits = v_overbrightbits.integer;

	hardwaregammasupported = VID_SetGamma((float) (1 << cacheoverbrightbits), cachegamma, cachecontrast, cachebrightness);
	if (!hardwaregammasupported)
	{
		Con_Printf("Hardware gamma not supported.\n");
		Cvar_SetValue("v_hwgamma", 0);
	}
	cachehwgamma = v_hwgamma.integer;
}

void Gamma_Init(void)
{
	Cvar_RegisterVariable(&v_gamma);
	Cvar_RegisterVariable(&v_brightness);
	Cvar_RegisterVariable(&v_contrast);
	Cvar_RegisterVariable(&v_hwgamma);
	Cvar_RegisterVariable(&v_overbrightbits);
}

void Palette_Init(void)
{
	qbyte *pal;
	pal = (qbyte *)COM_LoadFile ("gfx/palette.lmp", false);
	if (!pal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	memcpy(host_basepal, pal, 765);
	Mem_Free(pal);
	host_basepal[765] = host_basepal[766] = host_basepal[767] = 0; // LordHavoc: force the transparent color to black
	Palette_Setup8to24();
//	Palette_Setup15to8();
}
