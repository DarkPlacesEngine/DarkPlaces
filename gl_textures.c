#include "quakedef.h"

cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t		gl_lerpimages = {"gl_lerpimages", "1"};
cvar_t		r_upload = {"r_upload", "1"};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR; //NEAREST;
int		gl_filter_max = GL_LINEAR;


int		texels;

// 4096x4096
#define MAXMIPS 12

typedef struct
{
	int		texnum;
	byte	*texels[MAXMIPS];
	unsigned short texelsize[MAXMIPS][2];
	char	identifier[64];
	short	width, height;
// LordHavoc: CRC to identify cache mismatchs
	unsigned short crc;
	char	mipmap;
	char	alpha;
	char	bytesperpixel;
	char	lerped; // whether this texture was uploaded with or without interpolation
} gltexture_t;

#define	MAX_GLTEXTURES	4096
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	if (!r_upload.value)
		return;
	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			glBindTexture(GL_TEXTURE_2D, glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

extern int buildnumber;

char engineversion[40];

void GL_UploadTexture (gltexture_t *glt);
void gl_textures_start()
{
	int i;
	gltexture_t *glt;
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		GL_UploadTexture(glt);
}

void gl_textures_shutdown()
{
}

void GL_Textures_Init (void)
{
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&gl_lerpimages);
	Cvar_RegisterVariable (&r_upload);
#ifdef NORENDER
	r_upload.value = 0;
#endif

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) || strstr((char *)gl_renderer, "Glide"))
		Cvar_Set ("gl_max_size", "256");

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	R_RegisterModule("GL_Textures", gl_textures_start, gl_textures_shutdown);
}

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

extern byte qgamma[];

// LordHavoc: gamma correction and improved resampling
void GL_ResampleTextureLerpLine (byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, l1, l2, endx;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = (int) f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			l2 = f & 0xFFFF;
			l1 = 0x10000 - l2;
			*out++ = qgamma[(byte) ((in[0] * l1 + in[4] * l2) >> 16)];
			*out++ = qgamma[(byte) ((in[1] * l1 + in[5] * l2) >> 16)];
			*out++ = qgamma[(byte) ((in[2] * l1 + in[6] * l2) >> 16)];
			*out++ =        (byte) ((in[3] * l1 + in[7] * l2) >> 16) ;
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = qgamma[in[0]];
			*out++ = qgamma[in[1]];
			*out++ = qgamma[in[2]];
			*out++ =        in[3] ;
		}
	}
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,  int outwidth, int outheight)
{
	// LordHavoc: gamma correction and greatly improved resampling
	if (gl_lerpimages.value)
	{
		int		i, j, yi, oldy, f, fstep, l1, l2, endy = (inheight-1);
		byte	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep = (int) (inheight*65536.0f/outheight);

		row1 = malloc(outwidth*4);
		row2 = malloc(outwidth*4);
		inrow = indata;
		oldy = 0;
		GL_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
		GL_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
		for (i = 0, f = 0;i < outheight;i++,f += fstep)
		{
			yi = f >> 16;
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth*4*yi;
				if (yi == oldy+1)
					memcpy(row1, row2, outwidth*4);
				else
					GL_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
				if (yi < endy)
					GL_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
				else
					memcpy(row2, row1, outwidth*4);
				oldy = yi;
			}
			if (yi < endy)
			{
				l2 = f & 0xFFFF;
				l1 = 0x10000 - l2;
				for (j = 0;j < outwidth;j++)
				{
					*out++ = (byte) ((*row1++ * l1 + *row2++ * l2) >> 16);
					*out++ = (byte) ((*row1++ * l1 + *row2++ * l2) >> 16);
					*out++ = (byte) ((*row1++ * l1 + *row2++ * l2) >> 16);
					*out++ = (byte) ((*row1++ * l1 + *row2++ * l2) >> 16);
				}
				row1 -= outwidth*4;
				row2 -= outwidth*4;
			}
			else // last line has no pixels to lerp to
			{
				for (j = 0;j < outwidth;j++)
				{
					*out++ = *row1++;
					*out++ = *row1++;
					*out++ = *row1++;
					*out++ = *row1++;
				}
				row1 -= outwidth*4;
			}
		}
		free(row1);
		free(row2);
	}
	else
	{
		int		i, j;
		unsigned	frac, fracstep;
		byte	*inrow, *out, *inpix;
		out = outdata;

		fracstep = inwidth*0x10000/outwidth;
		for (i=0 ; i<outheight ; i++)
		{
			inrow = (byte *)indata + inwidth*(i*inheight/outheight)*4;
			frac = fracstep >> 1;
			for (j=0 ; j<outwidth ; j+=4)
			{
				inpix = inrow + ((frac >> 14) & ~3);*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ =       *inpix++ ;frac += fracstep;
				inpix = inrow + ((frac >> 14) & ~3);*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ =       *inpix++ ;frac += fracstep;
				inpix = inrow + ((frac >> 14) & ~3);*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ =       *inpix++ ;frac += fracstep;
				inpix = inrow + ((frac >> 14) & ~3);*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ = qgamma[*inpix++];*out++ =       *inpix++ ;frac += fracstep;
			}
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
/*
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j  ] = inrow[frac>>16];frac += fracstep;
			out[j+1] = inrow[frac>>16];frac += fracstep;
			out[j+2] = inrow[frac>>16];frac += fracstep;
			out[j+3] = inrow[frac>>16];frac += fracstep;
		}
	}
}
*/


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
/*
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}
*/

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
/*
void GL_MipMap8Bit (byte *in, int width, int height)
{
	int		i, j;
	unsigned short     r,g,b;
	byte	*out, *at1, *at2, *at3, *at4;

	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (byte *) (d_8to24table + in[0]);
			at2 = (byte *) (d_8to24table + in[1]);
			at3 = (byte *) (d_8to24table + in[width+0]);
			at4 = (byte *) (d_8to24table + in[width+1]);

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
	}
}
*/

/*
===============
GL_Upload32
===============
*/
/*
void GL_Upload32 (void *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int samples, scaled_width, scaled_height, i;
	byte *in, *out, *scaled;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (alpha)
	{
		alpha = false;
		in = data;
		for (i = 3;i < width*height*4;i += 4)
			if (in[i] != 255)
			{
				alpha = true;
				break;
			}
	}

	samples = alpha ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	scaled = malloc(scaled_width*scaled_height*4);
	if (scaled_width == width && scaled_height == height)
	{
		// LordHavoc: gamma correct while copying
		in = (byte *)data;
		out = (byte *)scaled;
		for (i = 0;i < width*height;i++)
		{
			*out++ = qgamma[*in++];
			*out++ = qgamma[*in++];
			*out++ = qgamma[*in++];
			*out++ = *in++;
		}
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}

	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	free(scaled);
}

void GL_Upload8_EXT (byte *data, int width, int height,  qboolean mipmap)
{
	int		scaled_width, scaled_height;
	byte	*scaled = NULL;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX , GL_UNSIGNED_BYTE, data);
			goto done;
		}
		scaled = malloc(scaled_width*scaled_height*4);
		memcpy (scaled, data, width*height);
	}
	else
	{
		scaled = malloc(scaled_width*scaled_height*4);
		GL_Resample8BitTexture (data, width, height, (void*) scaled, scaled_width, scaled_height);
	}

	glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap8Bit ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	free(scaled);
}
*/

extern qboolean VID_Is8bit();

/*
===============
GL_Upload8
===============
*/
/*
void GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	static	unsigned *trans;
	int			i, s;
	qboolean	noalpha;
	int			p;
	byte	*indata;
	int		*outdata;

	s = width*height;
	trans = malloc(s*4);
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p != 255)
				trans[i] = d_8to24table[p];
			else
			{
				trans[i] = 0; // force to black
				noalpha = false;
			}
		}

		if (noalpha)
		{
			if (VID_Is8bit() && (data!=scrap_texels[0]))
			{
 				GL_Upload8_EXT (data, width, height, mipmap);
				free(trans);
 				return;
			}
			alpha = false;
		}
	}
	else
	{
		// LordHavoc: dodge the copy if it will be uploaded as 8bit
	 	if (VID_Is8bit() && (data!=scrap_texels[0]))
		{
 			GL_Upload8_EXT (data, width, height, mipmap);
			free(trans);
 			return;
		}
		//if (s&3)
		//	Sys_Error ("GL_Upload8: s&3");
		indata = data;
		outdata = trans;
		if (s&1)
			*outdata++ = d_8to24table[*indata++];
		if (s&2)
		{
			*outdata++ = d_8to24table[*indata++];
			*outdata++ = d_8to24table[*indata++];
		}
		for (i = 0;i < s;i+=4)
		{
			*outdata++ = d_8to24table[*indata++];
			*outdata++ = d_8to24table[*indata++];
			*outdata++ = d_8to24table[*indata++];
			*outdata++ = d_8to24table[*indata++];
		}
	}

	GL_Upload32 (trans, width, height, mipmap, alpha);
	free(trans);
}
*/

void GL_AllocTexels(gltexture_t *glt, int width, int height, int mipmapped)
{
	int i, w, h, size, done;
	if (glt->texels[0])
		free(glt->texels[0]);
	glt->texelsize[0][0] = width;
	glt->texelsize[0][1] = height;
	if (mipmapped)
	{
		size = 0;
		w = width;h = height;
		i = 0;
		done = false;
		for (i = 0;i < MAXMIPS;i++)
		{
			glt->texelsize[i][0] = w;
			glt->texelsize[i][1] = h;
			glt->texels[i] = (void *)size;
			size += w*h*4;
			if (w > 1)
			{
				w >>= 1;
				if (h > 1)
					h >>= 1;
			}
			else if (h > 1)
				h >>= 1;
			else
			{
				i++;
				break;
			}
		}
		while (i < MAXMIPS)
			glt->texels[i++] = NULL;
		glt->texels[0] = malloc(size);
		for (i = 1;i < MAXMIPS && glt->texels[i];i++)
			glt->texels[i] += (int) glt->texels[0];
	}
	else
	{
		glt->texels[0] = malloc(width*height*4);
		for (i = 1;i < MAXMIPS;i++)
			glt->texels[i] = NULL;
	}
	if (!glt->texels[0])
		Sys_Error("GL_AllocTexels: out of memory\n");
}

// in can be the same as out
void GL_MipReduce(byte *in, byte *out, int width, int height, int destwidth, int destheight)
{
	int x, y, width2, height2, nextrow;
	if (width > destwidth)
	{
		if (height > destheight)
		{
			// reduce both
			width2 = width >> 1;
			height2 = height >> 1;
			nextrow = width << 2;
			for (y = 0;y < height2;y++)
			{
				for (x = 0;x < width2;x++)
				{
					out[0] = (byte) ((in[0] + in[4] + in[nextrow  ] + in[nextrow+4]) >> 2);
					out[1] = (byte) ((in[1] + in[5] + in[nextrow+1] + in[nextrow+5]) >> 2);
					out[2] = (byte) ((in[2] + in[6] + in[nextrow+2] + in[nextrow+6]) >> 2);
					out[3] = (byte) ((in[3] + in[7] + in[nextrow+3] + in[nextrow+7]) >> 2);
					out += 4;
					in += 8;
				}
				in += nextrow; // skip a line
			}
		}
		else
		{
			// reduce width
			width2 = width >> 1;
			for (y = 0;y < height;y++)
			{
				for (x = 0;x < width2;x++)
				{
					out[0] = (byte) ((in[0] + in[4]) >> 1);
					out[1] = (byte) ((in[1] + in[5]) >> 1);
					out[2] = (byte) ((in[2] + in[6]) >> 1);
					out[3] = (byte) ((in[3] + in[7]) >> 1);
					out += 4;
					in += 8;
				}
			}
		}
	}
	else
	{
		if (height > destheight)
		{
			// reduce height
			height2 = height >> 1;
			nextrow = width << 2;
			for (y = 0;y < height2;y++)
			{
				for (x = 0;x < width;x++)
				{
					out[0] = (byte) ((in[0] + in[nextrow  ]) >> 1);
					out[1] = (byte) ((in[1] + in[nextrow+1]) >> 1);
					out[2] = (byte) ((in[2] + in[nextrow+2]) >> 1);
					out[3] = (byte) ((in[3] + in[nextrow+3]) >> 1);
					out += 4;
					in += 4;
				}
				in += nextrow; // skip a line
			}
		}
		else
			Sys_Error("GL_MipReduce: desired size already achieved\n");
	}
}

void GL_UploadTexture (gltexture_t *glt)
{
	int mip, width, height;
	if (!r_upload.value)
		return;
	glBindTexture(GL_TEXTURE_2D, glt->texnum);
	width = glt->width;
	height = glt->height;
	for (mip = 0;mip < MAXMIPS && glt->texels[mip];mip++)
		glTexImage2D(GL_TEXTURE_2D, mip, glt->alpha ? 4 : 3, glt->texelsize[mip][0], glt->texelsize[mip][1], 0, GL_RGBA, GL_UNSIGNED_BYTE, glt->texels[mip]);
	if (glt->mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	unsigned short	crc;
	int				i, width2, height2, width3, height3, w, h, mip;
	gltexture_t		*glt;

	if (isDedicated)
		return 1;

	// LordHavoc: do a CRC to confirm the data really is the same as previous occurances.
	crc = CRC_Block(data, width*height*bytesperpixel);
	// see if the texture is already present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				// LordHavoc: everyone hates cache mismatchs, so I fixed it
				if (crc != glt->crc || width != glt->width || height != glt->height)
				{
					Con_DPrintf("GL_LoadTexture: cache mismatch, replacing old texture\n");
					goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
					//Sys_Error ("GL_LoadTexture: cache mismatch");
				}
				if ((gl_lerpimages.value != 0) != glt->lerped)
					goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
				return glt->texnum;
			}
		}
	}
	// LordHavoc: although this could be an else condition as it was in the original id code,
	//            it is more clear this way
	// LordHavoc: check if there are still slots available
	if (numgltextures >= MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: ran out of texture slots (%d)\n", MAX_GLTEXTURES);
	glt = &gltextures[numgltextures++];

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	texture_extension_number++;
// LordHavoc: label to drop out of the loop into the setup code
GL_LoadTexture_setup:
	// calculate power of 2 size
	width2 = 1;while (width2 < width) width2 <<= 1;
	height2 = 1;while (height2 < height) height2 <<= 1;
	// calculate final size (mipmapped downward to this)
	width3 = width2 >> (int) gl_picmip.value;
	height3 = height2 >> (int) gl_picmip.value;
	while (width3 > (int) gl_max_size.value) width3 >>= 1;
	while (height3 > (int) gl_max_size.value) height3 >>= 1;
	if (width3 < 1) width3 = 1;
	if (height3 < 1) height3 = 1;

	// final storage
	GL_AllocTexels(glt, width3, height3, mipmap);
	glt->crc = crc; // LordHavoc: used to verify textures are identical
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->bytesperpixel = bytesperpixel;
	glt->lerped = gl_lerpimages.value != 0;
	glt->alpha = false; // updated later
	if (width == width3 && height == height3) // perfect match
	{
		if (bytesperpixel == 1) // 8bit
			Image_Copy8bitRGBA(data, glt->texels[0], width*height, d_8to24table);
		else
			Image_CopyRGBAGamma(data, glt->texels[0], width*height);
	}
	else if (width == width2 && height == height2) // perfect match for top level, but needs to be reduced
	{
		byte *temptexels2;
		temptexels2 = malloc(width2*height2*4); // scaleup buffer
		if (bytesperpixel == 1) // 8bit
			Image_Copy8bitRGBA(data, temptexels2, width*height, d_8to24table);
		else
			Image_CopyRGBAGamma(data, temptexels2, width*height);
		while (width2 > width3 || height2 > height3)
		{
			w = width2;h = height2;
			if (width2 > width3) width2 >>= 1;
			if (height2 > height3) height2 >>= 1;
			if (width2 <= width3 && height2 <= height3) // size achieved
				GL_MipReduce(temptexels2, glt->texels[0], w, h, width3, height3);
			else
				GL_MipReduce(temptexels2, temptexels2, w, h, width3, height3);
		}
		free(temptexels2);
	}
	else // scaling...
	{
		byte *temptexels;
		// pre-scaleup buffer
		temptexels = malloc(width*height*4);
		if (bytesperpixel == 1) // 8bit
			Image_Copy8bitRGBA(data, temptexels, width*height, d_8to24table);
		else
			Image_CopyRGBAGamma(data, temptexels, width*height);
		if (width2 != width3 || height2 != height3) // reduced by gl_pic_mip or gl_max_size
		{
			byte *temptexels2;
			temptexels2 = malloc(width2*height2*4); // scaleup buffer
			GL_ResampleTexture(temptexels, width, height, temptexels2, width2, height2);
			while (width2 > width3 || height2 > height3)
			{
				w = width2;h = height2;
				if (width2 > width3) width2 >>= 1;
				if (height2 > height3) height2 >>= 1;
				if (width2 <= width3 && height2 <= height3) // size achieved
					GL_MipReduce(temptexels2, glt->texels[0], w, h, width3, height3);
				else
					GL_MipReduce(temptexels2, temptexels2, w, h, width3, height3);
			}
			free(temptexels2);
		}
		else // copy directly
			GL_ResampleTexture(temptexels, width, height, glt->texels[0], width2, height2);
		free(temptexels);
	}
	if (alpha)
	{
		byte	*in = glt->texels[0] + 3;
		for (i = 0;i < width*height;i++, in += 4)
			if (*in < 255)
			{
				glt->alpha = true;
				break;
			}
	}
	// this loop is skipped if there are no mipmaps to generate
	for (mip = 1;mip < MAXMIPS && glt->texels[mip];mip++)
		GL_MipReduce(glt->texels[mip-1], glt->texels[mip], glt->texelsize[mip-1][0], glt->texelsize[mip-1][1], 1, 1);
	GL_UploadTexture(glt);

//	if (bytesperpixel == 1) // 8bit
//		GL_Upload8 (data, width, height, mipmap, alpha);
//	else // 32bit
//		GL_Upload32 (data, width, height, mipmap, true);

	return glt->texnum;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, 1);
}

int GL_GetTextureSlots (int count)
{
	gltexture_t		*glt, *first;

	first = glt = &gltextures[numgltextures];
	while (count--)
	{
		glt->identifier[0] = 0;
		glt->texnum = texture_extension_number++;
		glt->crc = 0;
		glt->width = 0;
		glt->height = 0;
		glt->bytesperpixel = 0;
		glt++;
		numgltextures++;
	}
	return first->texnum;
}
