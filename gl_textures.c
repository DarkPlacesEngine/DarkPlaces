#include "quakedef.h"

cvar_t		r_max_size = {"r_max_size", "2048"};
cvar_t		r_picmip = {"r_picmip", "0"};
cvar_t		r_lerpimages = {"r_lerpimages", "1"};
cvar_t		r_upload = {"r_upload", "1"};
cvar_t		r_precachetextures = {"r_precachetextures", "1", true};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR; //NEAREST;
int		gl_filter_max = GL_LINEAR;


int		texels;

// 65536x65536
#define MAXMIPS 16

#define GLTEXF_LERPED 1
#define GLTEXF_UPLOADED 2

typedef struct
{
	char	identifier[64];
	int		texnum; // GL texture slot number
	int		texeldatasize; // computed memory usage of this texture (including mipmaps, expansion to 32bit, etc)
	byte	*inputtexels; // copy of the original texture supplied to the upload function, for re-uploading or deferred uploads (non-precached)
	int		inputtexeldatasize; // size of the original texture
	unsigned short width, height;
// LordHavoc: CRC to identify cache mismatchs
	unsigned short crc;
	int flags; // the requested flags when the texture was supplied to the upload function
	int internalflags; // internal notes (lerped, etc)
} gltexture_t;

#define	MAX_GLTEXTURES	4096
gltexture_t	*gltextures;
unsigned int numgltextures = 0, gl_texture_number = 1;

void GL_UploadTexture(gltexture_t *t);

int R_GetTexture(rtexture_t *rt)
{
	gltexture_t *glt;
	if (!rt)
		return 0;
	glt = (gltexture_t *)rt;
	if (!(glt->internalflags & GLTEXF_UPLOADED))
	{
		GL_UploadTexture(glt);
		if (!(glt->internalflags & GLTEXF_UPLOADED))
			Host_Error("R_GetTexture: unable to upload texture\n");
	}
	return glt->texnum;
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] =
{
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
		if (glt->flags & TEXF_MIPMAP)
		{
			glBindTexture(GL_TEXTURE_2D, glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

void GL_TextureStats_Print(char *name, int total, int total2, int loaded, int crc, int mip, int alpha, int total2valid)
{
	if (!name[0])
		name = "<unnamed>";
	Con_Printf("%5iK %c%5iK%c %04X %s %s %s %s\n", total, total2valid ? ' ' : '(', total2, total2valid ? ' ' : ')', crc, loaded ? "loaded" : "      ", mip ? "mip" : "   ", alpha ? "alpha" : "     ", name);
}

void GL_TextureStats_PrintTotal(void)
{
	int i, t = 0, p = 0, loaded = 0, loadedt = 0, loadedp = 0;
	gltexture_t *glt;
	for (i = 0, glt = gltextures;i < numgltextures;i++, glt++)
	{
		t += glt->texeldatasize;
		p += glt->inputtexeldatasize;
		if (glt->internalflags & GLTEXF_UPLOADED)
		{
			loaded++;
			loadedt += glt->texeldatasize;
			loadedp += glt->inputtexeldatasize;
		}
	}
	Con_Printf("total: %i (%.3fMB, %.3fMB original), uploaded %i (%.3fMB, %.3fMB original), upload on demand %i (%.3fMB, %.3fMB original)\n", numgltextures, t / 1048576.0, p / 1048576.0, loaded, loadedt / 1048576.0, loadedp / 1048576.0, numgltextures - loaded, (t - loadedt) / 1048576.0, (p - loadedp) / 1048576.0);
}

void GL_TextureStats_f(void)
{
	int i;
	gltexture_t *glt;
	Con_Printf("kbytes original crc  loaded mip alpha name\n");
	for (i = 0, glt = gltextures;i < numgltextures;i++, glt++)
		GL_TextureStats_Print(glt->identifier, (glt->texeldatasize + 1023) / 1024, (glt->inputtexeldatasize + 1023) / 1024, glt->internalflags & GLTEXF_UPLOADED, glt->crc, glt->flags & TEXF_MIPMAP, glt->flags & TEXF_ALPHA, glt->inputtexels != NULL);
	GL_TextureStats_PrintTotal();
}

char engineversion[40];

//void GL_UploadTexture (gltexture_t *glt);
void r_textures_start(void)
{
//	int i;
//	gltexture_t *glt;
//	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
//		GL_UploadTexture(glt);
}

void r_textures_shutdown(void)
{
}

void r_textures_newmap(void)
{
}

void R_Textures_Init (void)
{
	Cmd_AddCommand("r_texturestats", GL_TextureStats_f);
	Cvar_RegisterVariable (&r_max_size);
	Cvar_RegisterVariable (&r_picmip);
	Cvar_RegisterVariable (&r_lerpimages);
	Cvar_RegisterVariable (&r_upload);
	Cvar_RegisterVariable (&r_precachetextures);
#ifdef NORENDER
	r_upload.value = 0;
#endif

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) || strstr((char *)gl_renderer, "Glide"))
		Cvar_Set ("r_max_size", "256");

	gltextures = qmalloc(sizeof(gltexture_t) * MAX_GLTEXTURES);
	memset(gltextures, 0, sizeof(gltexture_t) * MAX_GLTEXTURES);
	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap);
}

/*
================
R_FindTexture
================
*/
int R_FindTexture (char *identifier)
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

void R_ResampleTextureLerpLine (byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx;
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
			int lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

/*
================
R_ResampleTexture
================
*/
void R_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,  int outwidth, int outheight)
{
	if (r_lerpimages.value)
	{
		int		i, j, yi, oldy, f, fstep, endy = (inheight-1);
		byte	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep = (int) (inheight*65536.0f/outheight);

		row1 = qmalloc(outwidth*4);
		row2 = qmalloc(outwidth*4);
		inrow = indata;
		oldy = 0;
		R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
		R_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
		for (i = 0, f = 0;i < outheight;i++,f += fstep)
		{
			yi = f >> 16;
			if (yi < endy)
			{
				int lerp = f & 0xFFFF;
				if (yi != oldy)
				{
					inrow = (byte *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
					R_ResampleTextureLerpLine (inrow + inwidth*4, row2, inwidth, outwidth);
					oldy = yi;
				}
				j = outwidth - 4;
				while(j >= 0)
				{
					out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
					out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
					out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
					out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
					out[ 4] = (byte) ((((row2[ 4] - row1[ 4]) * lerp) >> 16) + row1[ 4]);
					out[ 5] = (byte) ((((row2[ 5] - row1[ 5]) * lerp) >> 16) + row1[ 5]);
					out[ 6] = (byte) ((((row2[ 6] - row1[ 6]) * lerp) >> 16) + row1[ 6]);
					out[ 7] = (byte) ((((row2[ 7] - row1[ 7]) * lerp) >> 16) + row1[ 7]);
					out[ 8] = (byte) ((((row2[ 8] - row1[ 8]) * lerp) >> 16) + row1[ 8]);
					out[ 9] = (byte) ((((row2[ 9] - row1[ 9]) * lerp) >> 16) + row1[ 9]);
					out[10] = (byte) ((((row2[10] - row1[10]) * lerp) >> 16) + row1[10]);
					out[11] = (byte) ((((row2[11] - row1[11]) * lerp) >> 16) + row1[11]);
					out[12] = (byte) ((((row2[12] - row1[12]) * lerp) >> 16) + row1[12]);
					out[13] = (byte) ((((row2[13] - row1[13]) * lerp) >> 16) + row1[13]);
					out[14] = (byte) ((((row2[14] - row1[14]) * lerp) >> 16) + row1[14]);
					out[15] = (byte) ((((row2[15] - row1[15]) * lerp) >> 16) + row1[15]);
					out += 16;
					row1 += 16;
					row2 += 16;
					j -= 4;
				}
				if (j & 2)
				{
					out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
					out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
					out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
					out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
					out[ 4] = (byte) ((((row2[ 4] - row1[ 4]) * lerp) >> 16) + row1[ 4]);
					out[ 5] = (byte) ((((row2[ 5] - row1[ 5]) * lerp) >> 16) + row1[ 5]);
					out[ 6] = (byte) ((((row2[ 6] - row1[ 6]) * lerp) >> 16) + row1[ 6]);
					out[ 7] = (byte) ((((row2[ 7] - row1[ 7]) * lerp) >> 16) + row1[ 7]);
					out += 8;
					row1 += 8;
					row2 += 8;
				}
				if (j & 1)
				{
					out[ 0] = (byte) ((((row2[ 0] - row1[ 0]) * lerp) >> 16) + row1[ 0]);
					out[ 1] = (byte) ((((row2[ 1] - row1[ 1]) * lerp) >> 16) + row1[ 1]);
					out[ 2] = (byte) ((((row2[ 2] - row1[ 2]) * lerp) >> 16) + row1[ 2]);
					out[ 3] = (byte) ((((row2[ 3] - row1[ 3]) * lerp) >> 16) + row1[ 3]);
					out += 4;
					row1 += 4;
					row2 += 4;
				}
				row1 -= outwidth*4;
				row2 -= outwidth*4;
			}
			else
			{
				if (yi != oldy)
				{
					inrow = (byte *)indata + inwidth*4*yi;
					if (yi == oldy+1)
						memcpy(row1, row2, outwidth*4);
					else
						R_ResampleTextureLerpLine (inrow, row1, inwidth, outwidth);
					oldy = yi;
				}
				memcpy(out, row1, outwidth * 4);
			}
		}
		qfree(row1);
		qfree(row2);
	}
	else
	{
		int i, j;
		unsigned frac, fracstep;
		// relies on int being 4 bytes
		int *inrow, *out;
		out = outdata;

		fracstep = inwidth*0x10000/outwidth;
		for (i = 0;i < outheight;i++)
		{
			inrow = (int *)indata + inwidth*(i*inheight/outheight);
			frac = fracstep >> 1;
			j = outwidth - 4;
			while(j >= 0)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out[1] = inrow[frac >> 16];frac += fracstep;
				out[2] = inrow[frac >> 16];frac += fracstep;
				out[3] = inrow[frac >> 16];frac += fracstep;
				out += 4;
				j--;
			}
			if (j & 2)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out[1] = inrow[frac >> 16];frac += fracstep;
				out += 2;
			}
			if (j & 1)
			{
				out[0] = inrow[frac >> 16];frac += fracstep;
				out += 1;
			}
		}
	}
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

void GL_Upload32(int glslot, byte *data, int width, int height, int flags)
{
	int mip, width2, height2, width3, height3, internalformat;
	byte *gammadata, *buffer;

	if (!r_upload.value)
		return;

	// 3 and 4 are converted by the driver to it's preferred format for the current display mode
	internalformat = 3;
	if (flags & TEXF_ALPHA)
		internalformat = 4;

	// calculate power of 2 size
	width2 = 1;while (width2 < width) width2 <<= 1;
	height2 = 1;while (height2 < height) height2 <<= 1;
	// calculate final size (mipmapped downward to this)
	width3 = width2 >> (int) r_picmip.value;
	height3 = height2 >> (int) r_picmip.value;
	while (width3 > (int) r_max_size.value) width3 >>= 1;
	while (height3 > (int) r_max_size.value) height3 >>= 1;
	if (width3 < 1) width3 = 1;
	if (height3 < 1) height3 = 1;

	gammadata = qmalloc(width*height*4);
	buffer = qmalloc(width2*height2*4);
	if (!gammadata || !buffer)
		Host_Error("GL_Upload32: out of memory\n");

	Image_CopyRGBAGamma(data, gammadata, width*height);

	R_ResampleTexture(gammadata, width, height, buffer, width2, height2);

	qfree(gammadata);

	while (width2 > width3 || height2 > height3)
	{
		GL_MipReduce(buffer, buffer, width2, height2, width3, height3);

		if (width2 > width3)
			width2 >>= 1;
		if (height2 > height3)
			height2 >>= 1;
	}

	glBindTexture(GL_TEXTURE_2D, glslot);
	mip = 0;
	glTexImage2D(GL_TEXTURE_2D, mip++, internalformat, width2, height2, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	if (flags & TEXF_MIPMAP)
	{
		while (width2 > 1 || height2 > 1)
		{
			GL_MipReduce(buffer, buffer, width2, height2, 1, 1);

			if (width2 > 1)
				width2 >>= 1;
			if (height2 > 1)
				height2 >>= 1;

			glTexImage2D(GL_TEXTURE_2D, mip++, internalformat, width2, height2, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
		}

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	qfree(buffer);
}

void GL_Upload8 (int glslot, byte *data, int width, int height, int flags)
{
	byte *data32;
	data32 = qmalloc(width*height*4);
	Image_Copy8bitRGBA(data, data32, width*height, d_8to24table);
	GL_Upload32(glslot, data32, width, height, flags);
	qfree(data32);
}

void GL_UploadTexture (gltexture_t *glt)
{
	if (glt->inputtexels == NULL)
		return;
	if (glt->flags & TEXF_RGBA)
		GL_Upload32(glt->texnum, glt->inputtexels, glt->width, glt->height, glt->flags);
	else // 8bit
		GL_Upload8(glt->texnum, glt->inputtexels, glt->width, glt->height, glt->flags);
	glt->internalflags |= GLTEXF_UPLOADED;
	qfree(glt->inputtexels);
	glt->inputtexels = NULL;
}

int R_CalcTexelDataSize (int width, int height, int mipmapped)
{
	int width2, height2, size;
	width2 = 1;while (width2 < width) width2 <<= 1;
	height2 = 1;while (height2 < height) height2 <<= 1;
	// calculate final size (mipmapped downward to this)
	width2 >>= (int) r_picmip.value;
	height2 >>= (int) r_picmip.value;
	while (width2 > (int) r_max_size.value) width2 >>= 1;
	while (height2 > (int) r_max_size.value) height2 >>= 1;
	if (width2 < 1) width2 = 1;
	if (height2 < 1) height2 = 1;

	size = 0;
	if (mipmapped)
	{
		while (width2 > 1 || height2 > 1)
		{
			size += width2 * height2;
			if (width2 > 1)
				width2 >>= 1;
			if (height2 > 1)
				height2 >>= 1;
		}
		size++; // count the last 1x1 mipmap
	}
	else
		size = width2*height2;

	size *= 4; // RGBA

	return size;
}

/*
================
GL_LoadTexture
================
*/
rtexture_t *R_LoadTexture (char *identifier, int width, int height, byte *data, int flags)
{
	int				i, bytesperpixel, internalflags, precache;
	gltexture_t		*glt;
	unsigned short	crc;

	if (isDedicated)
		return NULL;

	if (!identifier[0])
		Host_Error("R_LoadTexture: no identifier\n");

	// clear the alpha flag if the texture has no transparent pixels
	if (flags & TEXF_ALPHA)
	{
		int alpha = false;
		if (flags & TEXF_RGBA)
		{
			for (i = 0;i < width * height;i++)
			{
				if (data[i * 4 + 3] < 255)
				{
					alpha = true;
					break;
				}
			}
		}
		else
		{
			for (i = 0;i < width * height;i++)
			{
				if (data[i] == 255)
				{
					alpha = true;
					break;
				}
			}
		}
		if (!alpha)
			flags &= ~TEXF_ALPHA;
	}

	if (flags & TEXF_RGBA)
		bytesperpixel = 4;
	else
		bytesperpixel = 1;

	internalflags = 0;
	if (r_lerpimages.value != 0)
		internalflags |= GLTEXF_LERPED;

	// LordHavoc: do a CRC to confirm the data really is the same as previous occurances.
	crc = CRC_Block(data, width*height*bytesperpixel);
	// see if the texture is already present
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
		{
			// LordHavoc: everyone hates cache mismatchs, so I fixed it
			if (crc != glt->crc || width != glt->width || height != glt->height || flags != glt->flags)
			{
				Con_DPrintf("GL_LoadTexture: cache mismatch, replacing old texture\n");
				goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
			}
			if (internalflags != glt->internalflags)
				goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
			return (rtexture_t *)glt;
		}
	}

/*
	if (freeglt)
	{
		glt = freeglt;
		strcpy (glt->identifier, identifier);
	}
	else
	{
*/
		// LordHavoc: check if there are still slots available
		if (numgltextures >= MAX_GLTEXTURES)
			Sys_Error ("GL_LoadTexture: ran out of texture slots (%d)\n", MAX_GLTEXTURES);
		glt = &gltextures[numgltextures++];
		glt->texnum = gl_texture_number++;
		strcpy (glt->identifier, identifier);
//	}

// LordHavoc: label to drop out of the loop into the setup code
GL_LoadTexture_setup:
	glt->crc = crc; // LordHavoc: used to verify textures are identical
	glt->width = width;
	glt->height = height;
	glt->flags = flags;
	glt->internalflags = internalflags;

	if (glt->inputtexels)
		qfree(glt->inputtexels);
	glt->inputtexeldatasize = width*height*bytesperpixel;
	glt->inputtexels = qmalloc(glt->inputtexeldatasize);

	memcpy(glt->inputtexels, data, glt->inputtexeldatasize);

	glt->texeldatasize = R_CalcTexelDataSize(width, height, flags & TEXF_MIPMAP);

	precache = false;
	if (r_precachetextures.value >= 1)
	{
		if (flags & TEXF_PRECACHE)
			precache = true;
		if (r_precachetextures.value >= 2)
			precache = true;
	}

	if (precache)
		GL_UploadTexture(glt);

	return (rtexture_t *)glt;
}

// only used for lightmaps
int R_GetTextureSlots(int count)
{
	int i;
	i = gl_texture_number;
	gl_texture_number += count;
	return i;
}
