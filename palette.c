
#include "quakedef.h"

unsigned int palette_complete[256];
unsigned int palette_nofullbrights[256];
unsigned int palette_onlyfullbrights[256];
unsigned int palette_nocolormapnofullbrights[256];
unsigned int palette_pantsaswhite[256];
unsigned int palette_shirtaswhite[256];
unsigned int palette_alpha[256];
unsigned int palette_font[256];

qbyte host_basepal[768];

cvar_t v_gamma = {CVAR_SAVE, "v_gamma", "1"};
cvar_t v_contrast = {CVAR_SAVE, "v_contrast", "1"};
cvar_t v_brightness = {CVAR_SAVE, "v_brightness", "0"};
cvar_t v_overbrightbits = {CVAR_SAVE, "v_overbrightbits", "0"};
cvar_t v_hwgamma = {0, "v_hwgamma", "1"};

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
	colormap = COM_LoadFile("gfx/colormap.lmp", true);
	if (colormap && com_filesize >= 16385)
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

	memset(palette_nofullbrights, 0, sizeof(palette_nofullbrights));
	for (i = 0;i < fullbright_start;i++)
		palette_nofullbrights[i] = palette_complete[i];

	memset(palette_onlyfullbrights, 0, sizeof(palette_onlyfullbrights));
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_onlyfullbrights[i] = palette_complete[i];

	for (i = 0;i < 256;i++)
		palette_nocolormapnofullbrights[i] = palette_complete[i];
	for (i = pants_start;i < pants_end;i++)
		palette_nocolormapnofullbrights[i] = 0;
	for (i = shirt_start;i < shirt_end;i++)
		palette_nocolormapnofullbrights[i] = 0;
	for (i = fullbright_start;i < fullbright_end;i++)
		palette_nocolormapnofullbrights[i] = 0;

	memset(palette_pantsaswhite, 0, sizeof(palette_pantsaswhite));
	for (i = pants_start;i < pants_end;i++)
	{
		if (i >= reversed_start && i < reversed_end)
			palette_pantsaswhite[i] = 15 - (i - pants_start);
		else
			palette_pantsaswhite[i] = i - pants_start;
	}

	memset(palette_shirtaswhite, 0, sizeof(palette_shirtaswhite));
	for (i = shirt_start;i < shirt_end;i++)
	{
		if (i >= reversed_start && i < reversed_end)
			palette_shirtaswhite[i] = 15 - (i - shirt_start);
		else
			palette_shirtaswhite[i] = i - shirt_start;
	}

	memset(palette_alpha, 0, sizeof(palette_alpha));
	for (i = 0;i < 255;i++)
		palette_alpha[i] = 0xFFFFFFFF;

	memset(palette_font, 0, sizeof(palette_font));
	for (i = 1;i < 255;i++)
		palette_font[i] = palette_complete[i];
}


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
	int i;
	float gamma, scale, base;
	qbyte *pal;
	qbyte temp[256];
	pal = (qbyte *)COM_LoadFile ("gfx/palette.lmp", false);
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

