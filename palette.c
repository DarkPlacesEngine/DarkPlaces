
#include "quakedef.h"

unsigned int d_8to24table[256];
//byte d_15to8table[32768];
byte host_basepal[768];
byte qgamma[256];
static float vid_gamma = 1.0;

void Palette_Setup8to24()
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
void	Palette_Setup15to8()
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

void Palette_Gamma ()
{
	float	inf;
	int		i;

	vid_gamma = 1;
	if ((i = COM_CheckParm("-gamma")))
		vid_gamma = atof(com_argv[i+1]);

	if (vid_gamma == 1) // LordHavoc: dodge the math
	{
		for (i = 0;i < 256;i++)
			qgamma[i] = i;
	}
	else
	{
		for (i = 0;i < 256;i++)
		{
			inf = pow((i+1)/256.0, vid_gamma)*255 + 0.5;
			if (inf < 0) inf = 0;
			if (inf > 255) inf = 255;
			qgamma[i] = inf;
		}
	}
}

void Palette_Init()
{
	byte *pal;
	pal = (byte *)COM_LoadMallocFile ("gfx/palette.lmp", false);
	if (!pal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	memcpy(host_basepal, pal, 765);
	free(pal);
	host_basepal[765] = host_basepal[766] = host_basepal[767] = 0; // LordHavoc: force the transparent color to black
	Palette_Setup8to24();
//	Palette_Setup15to8();
	Palette_Gamma();
}
