
#include "quakedef.h"

unsigned d_8to24table[256];
unsigned char d_15to8table[32768]; // LordHavoc: was 64k elements, now 32k like it should be

void	VID_SetPalette (unsigned char *palette)
{
	byte	*out;
	unsigned short i;

	out = (byte *) d_8to24table; // d_8to24table is accessed as 32bit for speed reasons, but is created as 8bit bytes
	for (i=0 ; i<255 ; i++)
	{
		*out++ = *palette++;
		*out++ = *palette++;
		*out++ = *palette++;
		*out++ = 255;
	}
	d_8to24table[255] = 0;
}

void	VID_Setup15to8Palette ()
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		j,k,l;
	unsigned short i;

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.
	for (i = 0;i < 32768;i++)
	{
		/* Maps
			000000000000000
			000000000011111 = Red  = 0x001F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x001F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v = 0, k = 0, l = 1000000000;v < 256;v++, pal += 4)
		{
			r1 = r - pal[0];
			g1 = g - pal[1];
			b1 = b - pal[2];
			j = (r1*r1*2)+(g1*g1*3)+(b1*b1); // LordHavoc: weighting to tune for human eye (added *2 and *3)
			if (j < l)
			{
				k = v;
				l = j;
			}
		}
		d_15to8table[i] = k;
	}
}

// LordHavoc: gamma correction does not belong in gl_vidnt.c
byte qgamma[256];
static float vid_gamma = 1.0;

void Check_Gamma (unsigned char *pal)
{
	float	inf;
	int		i;

	if ((i = COM_CheckParm("-gamma")))
		vid_gamma = atof(com_argv[i+1]);
	else
	{
//		if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
//			(gl_vendor && strstr(gl_vendor, "3Dfx")))
			vid_gamma = 1;
//		else if (gl_vendor && strstr(gl_vendor, "ATI"))
//			vid_gamma = 1;
//		else
//			vid_gamma = 0.7;
	}

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

	// gamma correct the palette
	//for (i=0 ; i<768 ; i++)
	//	pal[i] = qgamma[pal[i]];
	// note: 32bit uploads are corrected by the upload functions
}
