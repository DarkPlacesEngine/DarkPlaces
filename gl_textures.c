#include "quakedef.h"

cvar_t		gl_max_size = {"gl_max_size", "2048"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t		gl_lerpimages = {"gl_lerpimages", "1"};
cvar_t		r_upload = {"r_upload", "1"};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR; //NEAREST;
int		gl_filter_max = GL_LINEAR;


int		texels;

// 65536x65536
#define MAXMIPS 16

typedef struct
{
	char	identifier[64];
	int		texnum;
	int		texeldatasize;
	byte	*texels[MAXMIPS];
	unsigned short texelsize[MAXMIPS][2];
	unsigned short width, height;
// LordHavoc: CRC to identify cache mismatchs
	unsigned short crc;
	char	mipmap;
	char	alpha;
	char	bytesperpixel;
	char	lerped; // whether this texture was uploaded with or without interpolation
	char	inuse; // cleared during texture purge when loading new level
	char	pad; // unused
} gltexture_t;

#define	MAX_GLTEXTURES	4096
gltexture_t	*gltextures;
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

void GL_TextureStats_Print(char *name, int total, int crc, int mip, int alpha)
{
	char n[64];
	int c = 0;
	if (!name[0])
		name = "<unnamed>";
	while (name[c] && c < 28)
		n[c++] = name[c];
	// no need to pad since the name was moved to last
//	while (c < 28)
//		n[c++] = ' ';
	n[c] = 0;
	Con_Printf("%5i %04X %s %s %s\n", total, crc, mip ? "yes" : "no ", alpha ? "yes  " : "no   ", n);
}

void GL_TextureStats_f(void)
{
	int i, s = 0, sc = 0, t = 0;
	gltexture_t *glt;
	Con_Printf("kbytes crc  mip alpha name\n");
	for (i = 0, glt = gltextures;i < numgltextures;i++, glt++)
	{
		GL_TextureStats_Print(glt->identifier, (glt->texeldatasize + 512) >> 10, glt->crc, glt->mipmap, glt->alpha);
		t += glt->texeldatasize;
		if (glt->identifier[0] == '&')
		{
			sc++;
			s += glt->texeldatasize;
		}
	}
	Con_Printf("%i textures, totalling %.3fMB, %i are (usually) unnecessary model skins totalling %.3fMB\n", numgltextures, t / 1048576.0, sc, s / 1048576.0);
}

void GL_TextureStats_PrintTotal(void)
{
	int i, s = 0, sc = 0, t = 0;
	gltexture_t *glt;
	for (i = 0, glt = gltextures;i < numgltextures;i++, glt++)
	{
		t += glt->texeldatasize;
		if (glt->identifier[0] == '&')
		{
			sc++;
			s += glt->texeldatasize;
		}
	}
	Con_Printf("%i textures, totalling %.3fMB, %i are (usually) unnecessary model skins totalling %.3fMB\n", numgltextures, t / 1048576.0, sc, s / 1048576.0);
}

char engineversion[40];

//void GL_UploadTexture (gltexture_t *glt);
void gl_textures_start()
{
//	int i;
//	gltexture_t *glt;
//	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
//		GL_UploadTexture(glt);
}

void gl_textures_shutdown()
{
}

void GL_Textures_Init (void)
{
	Cmd_AddCommand("r_texturestats", GL_TextureStats_f);
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

	gltextures = qmalloc(sizeof(gltexture_t) * MAX_GLTEXTURES);
	memset(gltextures, 0, sizeof(gltexture_t) * MAX_GLTEXTURES);
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
			*out++ = (byte) ((in[0] * l1 + in[4] * l2) >> 16);
			*out++ = (byte) ((in[1] * l1 + in[5] * l2) >> 16);
			*out++ = (byte) ((in[2] * l1 + in[6] * l2) >> 16);
			*out++ = (byte) ((in[3] * l1 + in[7] * l2) >> 16);
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
GL_ResampleTexture
================
*/
void GL_ResampleTexture (void *indata, int inwidth, int inheight, void *outdata,  int outwidth, int outheight)
{
	if (gl_lerpimages.value)
	{
		int		i, j, yi, oldy, f, fstep, l1, l2, endy = (inheight-1);
		byte	*inrow, *out, *row1, *row2;
		out = outdata;
		fstep = (int) (inheight*65536.0f/outheight);

		row1 = qmalloc(outwidth*4);
		row2 = qmalloc(outwidth*4);
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
		qfree(row1);
		qfree(row2);
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

void GL_FreeTexels(gltexture_t *glt)
{
	if (glt->texels[0])
		qfree(glt->texels[0]);
	glt->texels[0] = 0;
}

void GL_AllocTexels(gltexture_t *glt, int width, int height, int mipmapped)
{
	int i, w, h, size;
	if (glt->texels[0])
		GL_FreeTexels(glt);
	glt->texelsize[0][0] = width;
	glt->texelsize[0][1] = height;
	if (mipmapped)
	{
		size = 0;
		w = width;h = height;
		i = 0;
		while (i < MAXMIPS)
		{
			glt->texelsize[i][0] = w;
			glt->texelsize[i][1] = h;
			glt->texels[i++] = (void *)size;
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
				break;
		}
		glt->texeldatasize = size;
		while (i < MAXMIPS)
			glt->texels[i++] = NULL;
		glt->texels[0] = qmalloc(size);
		for (i = 1;i < MAXMIPS && glt->texels[i];i++)
			glt->texels[i] += (int) glt->texels[0];
	}
	else
	{
		size = width*height*4;
		glt->texeldatasize = size;
		glt->texels[0] = qmalloc(size);
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
	gltexture_t		*glt, *freeglt;
	// LordHavoc: texture caching, turned out to be a waste of time (and immense waste of diskspace)
	//char			cachefilename[1024], *cachefile;

	if (isDedicated)
		return 1;

	freeglt = NULL;

	// LordHavoc: do a CRC to confirm the data really is the same as previous occurances.
	crc = CRC_Block(data, width*height*bytesperpixel);
	// see if the texture is already present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->inuse)
			{
				if (!strcmp (identifier, glt->identifier))
				{
					// LordHavoc: everyone hates cache mismatchs, so I fixed it
					if (crc != glt->crc || width != glt->width || height != glt->height)
					{
						Con_DPrintf("GL_LoadTexture: cache mismatch, replacing old texture\n");
						goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
					}
					if ((gl_lerpimages.value != 0) != glt->lerped)
						goto GL_LoadTexture_setup; // drop out with glt pointing to the texture to replace
					return glt->texnum;
				}
			}
			else
				freeglt = glt;
		}
	}
	else
		i = 0;
	// LordHavoc: although this could be an else condition as it was in the original id code,
	//            it is more clear this way
	if (freeglt)
	{
		glt = freeglt;
		strcpy (glt->identifier, identifier);
	}
	else
	{
		// LordHavoc: check if there are still slots available
		if (numgltextures >= MAX_GLTEXTURES)
			Sys_Error ("GL_LoadTexture: ran out of texture slots (%d)\n", MAX_GLTEXTURES);
		glt = &gltextures[numgltextures++];
		glt->texnum = texture_extension_number;
		texture_extension_number++;
		strcpy (glt->identifier, identifier);
	}

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
	glt->inuse = true;
	/*
	// LordHavoc: texture caching, turned out to be a waste of time (and immense waste of diskspace)
	sprintf(cachefilename, "%s%x%x%x.texels", identifier, width3, height3, crc);
	for (i = 0;cachefilename[i];i++)
	{
		if (cachefilename[i] <= ' ' || cachefilename[i] >= 127 || cachefilename[i] == '/' || cachefilename[i] == '\\' || cachefilename[i] == ':' || cachefilename[i] == '*' || cachefilename[i] == '?')
			cachefilename[i] = '@';
		if (cachefilename[i] >= 'A' && cachefilename[i] <= 'Z')
			cachefilename[i] += 'a' - 'A';
	}
	cachefile = COM_LoadMallocFile(cachefilename, true);
	if (cachefile)
	{
		if (cachefile[0] == 'D' && cachefile[1] == 'P' && cachefile[2] == 'C' && cachefile[3] == 'T')
		{
			memcpy(glt->texels[0], cachefile + 4, width3*height3*4);
			qfree(cachefile);
//			Con_Printf("loaded cache texture %s\n", cachefilename);
			goto cacheloaded;
		}
		else
			qfree(cachefile);
	}
	*/
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
		temptexels2 = qmalloc(width2*height2*4); // scaleup buffer
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
		qfree(temptexels2);
	}
	else // scaling...
	{
		byte *temptexels;
		// pre-scaleup buffer
		temptexels = qmalloc(width*height*4);
		if (bytesperpixel == 1) // 8bit
			Image_Copy8bitRGBA(data, temptexels, width*height, d_8to24table);
		else
			Image_CopyRGBAGamma(data, temptexels, width*height);
		if (width2 != width3 || height2 != height3) // reduced by gl_pic_mip or gl_max_size
		{
			byte *temptexels2;
			temptexels2 = qmalloc(width2*height2*4); // scaleup buffer
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
			qfree(temptexels2);
		}
		else // copy directly
			GL_ResampleTexture(temptexels, width, height, glt->texels[0], width2, height2);
		qfree(temptexels);
	}
	/*
	// LordHavoc: texture caching, turned out to be a waste of time (and immense waste of diskspace)
	Con_Printf("writing cache texture %s\n", cachefilename);
	cachefile = qmalloc(width3*height3*4 + 4);
	cachefile[0] = 'D';
	cachefile[1] = 'P';
	cachefile[2] = 'C';
	cachefile[3] = 'T';
	memcpy(cachefile + 4, glt->texels[0], width3*height3*4);
	COM_WriteFile(cachefilename, cachefile, width3*height3*4 + 4);
	qfree(cachefile);
cacheloaded:
	*/
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
	GL_FreeTexels(glt);

//	if (bytesperpixel == 1) // 8bit
//		GL_Upload8 (data, width, height, mipmap, alpha);
//	else // 32bit
//		GL_Upload32 (data, width, height, mipmap, true);

	return glt->texnum;
}
