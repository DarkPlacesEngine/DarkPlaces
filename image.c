
#include "quakedef.h"

int		image_width;
int		image_height;

void Image_GammaRemapRGB(qbyte *in, qbyte *out, int pixels, qbyte *gammar, qbyte *gammag, qbyte *gammab)
{
	while (pixels--)
	{
		out[0] = gammar[in[0]];
		out[1] = gammag[in[1]];
		out[2] = gammab[in[2]];
		in += 3;
		out += 3;
	}
}

// note: pal must be 32bit color
void Image_Copy8bitRGBA(qbyte *in, qbyte *out, int pixels, int *pal)
{
	int *iout = (void *)out;
	while (pixels >= 8)
	{
		iout[0] = pal[in[0]];
		iout[1] = pal[in[1]];
		iout[2] = pal[in[2]];
		iout[3] = pal[in[3]];
		iout[4] = pal[in[4]];
		iout[5] = pal[in[5]];
		iout[6] = pal[in[6]];
		iout[7] = pal[in[7]];
		in += 8;
		iout += 8;
		pixels -= 8;
	}
	if (pixels & 4)
	{
		iout[0] = pal[in[0]];
		iout[1] = pal[in[1]];
		iout[2] = pal[in[2]];
		iout[3] = pal[in[3]];
		in += 4;
		iout += 4;
	}
	if (pixels & 2)
	{
		iout[0] = pal[in[0]];
		iout[1] = pal[in[1]];
		in += 2;
		iout += 2;
	}
	if (pixels & 1)
		iout[0] = pal[in[0]];
}

/*
=================================================================

  PCX Loading

=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
} pcx_t;

/*
============
LoadPCX
============
*/
qbyte* LoadPCX (qbyte *f, int matchwidth, int matchheight)
{
	pcx_t pcx;
	qbyte *palette, *a, *b, *image_rgba, *fin, *pbuf, *enddata;
	int x, y, x2, dataByte;

	if (loadsize < sizeof(pcx) + 768)
	{
		Con_Printf ("Bad pcx file\n");
		return NULL;
	}

	fin = f;

	memcpy(&pcx, fin, sizeof(pcx));
	fin += sizeof(pcx);

	// LordHavoc: big-endian support ported from QF newtree
	pcx.xmax = LittleShort (pcx.xmax);
	pcx.xmin = LittleShort (pcx.xmin);
	pcx.ymax = LittleShort (pcx.ymax);
	pcx.ymin = LittleShort (pcx.ymin);
	pcx.hres = LittleShort (pcx.hres);
	pcx.vres = LittleShort (pcx.vres);
	pcx.bytes_per_line = LittleShort (pcx.bytes_per_line);
	pcx.palette_type = LittleShort (pcx.palette_type);

	if (pcx.manufacturer != 0x0a || pcx.version != 5 || pcx.encoding != 1 || pcx.bits_per_pixel != 8 || pcx.xmax > 320 || pcx.ymax > 256)
	{
		Con_Printf ("Bad pcx file\n");
		return NULL;
	}

	if (matchwidth && (pcx.xmax+1) != matchwidth)
	{
		return NULL;
	}
	if (matchheight && (pcx.ymax+1) != matchheight)
	{
		return NULL;
	}

	image_width = pcx.xmax+1;
	image_height = pcx.ymax+1;

	palette = f + loadsize - 768;

	image_rgba = Mem_Alloc(tempmempool, image_width*image_height*4);
	if (!image_rgba)
	{
		Con_Printf("LoadPCX: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}
	pbuf = image_rgba + image_width*image_height*3;
	enddata = palette;

	for (y = 0;y < image_height && fin < enddata;y++)
	{
		a = pbuf + y * image_width;
		for (x = 0;x < image_width && fin < enddata;)
		{
			dataByte = *fin++;
			if(dataByte >= 0xC0)
			{
				if (fin >= enddata)
					break;
				x2 = x + (dataByte & 0x3F);
				dataByte = *fin++;
				if (x2 > image_width)
					x2 = image_width; // technically an error
				while(x < x2)
					a[x++] = dataByte;
			}
			else
				a[x++] = dataByte;
		}
		fin += pcx.bytes_per_line - image_width; // the number of bytes per line is always forced to an even number
		while(x < image_width)
			a[x++] = 0;
	}

	a = image_rgba;
	b = pbuf;

	for(x = 0;x < image_width*image_height;x++)
	{
		y = *b++ * 3;
		*a++ = palette[y];
		*a++ = palette[y+1];
		*a++ = palette[y+2];
		*a++ = 255;
	}

	return image_rgba;
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
}
TargaHeader;

TargaHeader		targa_header;


/*
=============
LoadTGA
=============
*/
qbyte *LoadTGA (qbyte *f, int matchwidth, int matchheight)
{
	int x, y, row_inc;
	unsigned char red, green, blue, alpha, run, runlen;
	qbyte *pixbuf, *image_rgba, *fin, *enddata;

	if (loadsize < 18+3)
		return NULL;
	targa_header.id_length = f[0];
	targa_header.colormap_type = f[1];
	targa_header.image_type = f[2];

	targa_header.colormap_index = f[3] + f[4] * 256;
	targa_header.colormap_length = f[5] + f[6] * 256;
	targa_header.colormap_size = f[7];
	targa_header.x_origin = f[8] + f[9] * 256;
	targa_header.y_origin = f[10] + f[11] * 256;
	targa_header.width = f[12] + f[13] * 256;
	targa_header.height = f[14] + f[15] * 256;
	if (matchwidth && targa_header.width != matchwidth)
		return NULL;
	if (matchheight && targa_header.height != matchheight)
		return NULL;
	targa_header.pixel_size = f[16];
	targa_header.attributes = f[17];

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
	{
		Con_Printf ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");
		return NULL;
	}

	if (targa_header.colormap_type != 0	|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
	{
		Con_Printf ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		return NULL;
	}

	enddata = f + loadsize;

	image_width = targa_header.width;
	image_height = targa_header.height;

	image_rgba = Mem_Alloc(tempmempool, image_width * image_height * 4);
	if (!image_rgba)
	{
		Con_Printf ("LoadTGA: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}

	fin = f + 18;
	if (targa_header.id_length != 0)
		fin += targa_header.id_length;  // skip TARGA image comment

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = image_rgba + (image_height - 1)*image_width*4;
		row_inc = -image_width*4*2;
	}
	else
	{
		pixbuf = image_rgba;
		row_inc = 0;
	}

	if (targa_header.image_type == 2)
	{
		// Uncompressed, RGB images
		if (targa_header.pixel_size == 24)
		{
			if (fin + image_width * image_height * 3 <= enddata)
			{
				for(y = 0;y < image_height;y++)
				{
					for(x = 0;x < image_width;x++)
					{
						*pixbuf++ = fin[2];
						*pixbuf++ = fin[1];
						*pixbuf++ = fin[0];
						*pixbuf++ = 255;
						fin += 3;
					}
					pixbuf += row_inc;
				}
			}
		}
		else
		{
			if (fin + image_width * image_height * 4 <= enddata)
			{
				for(y = 0;y < image_height;y++)
				{
					for(x = 0;x < image_width;x++)
					{
						*pixbuf++ = fin[2];
						*pixbuf++ = fin[1];
						*pixbuf++ = fin[0];
						*pixbuf++ = fin[3];
						fin += 4;
					}
					pixbuf += row_inc;
				}
			}
		}
	}
	else if (targa_header.image_type==10)
	{
		// Runlength encoded RGB images
		x = 0;
		y = 0;
		while (y < image_height && fin < enddata)
		{
			runlen = *fin++;
			if (runlen & 0x80)
			{
				// RLE compressed run
				runlen = 1 + (runlen & 0x7f);
				if (targa_header.pixel_size == 24)
				{
					if (fin + 3 > enddata)
						break;
					blue = *fin++;
					green = *fin++;
					red = *fin++;
					alpha = 255;
				}
				else
				{
					if (fin + 4 > enddata)
						break;
					blue = *fin++;
					green = *fin++;
					red = *fin++;
					alpha = *fin++;
				}

				while (runlen && y < image_height)
				{
					run = runlen;
					if (run > image_width - x)
						run = image_width - x;
					x += run;
					runlen -= run;
					while(run--)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alpha;
					}
					if (x == image_width)
					{
						// end of line, advance to next
						x = 0;
						y++;
						pixbuf += row_inc;
					}
				}
			}
			else
			{
				// RLE uncompressed run
				runlen = 1 + (runlen & 0x7f);
				while (runlen && y < image_height)
				{
					run = runlen;
					if (run > image_width - x)
						run = image_width - x;
					x += run;
					runlen -= run;
					if (targa_header.pixel_size == 24)
					{
						if (fin + run * 3 > enddata)
							break;
						while(run--)
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = 255;
							fin += 3;
						}
					}
					else
					{
						if (fin + run * 4 > enddata)
							break;
						while(run--)
						{
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = fin[3];
							fin += 4;
						}
					}
					if (x == image_width)
					{
						// end of line, advance to next
						x = 0;
						y++;
						pixbuf += row_inc;
					}
				}
			}
		}
	}
	return image_rgba;
}

/*
============
LoadLMP
============
*/
qbyte *LoadLMP (qbyte *f, int matchwidth, int matchheight)
{
	qbyte *image_rgba;
	int width, height;

	if (loadsize < 9)
	{
		Con_Printf("LoadLMP: invalid LMP file\n");
		return NULL;
	}

	// parse the very complicated header *chuckle*
	width = f[0] + f[1] * 256 + f[2] * 65536 + f[3] * 16777216;
	height = f[4] + f[5] * 256 + f[6] * 65536 + f[7] * 16777216;
	if ((unsigned) width > 4096 || (unsigned) height > 4096)
	{
		Con_Printf("LoadLMP: invalid size\n");
		return NULL;
	}
	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight))
		return NULL;

	if (loadsize < 8 + width * height)
	{
		Con_Printf("LoadLMP: invalid LMP file\n");
		return NULL;
	}

	image_width = width;
	image_height = height;

	image_rgba = Mem_Alloc(tempmempool, image_width * image_height * 4);
	if (!image_rgba)
	{
		Con_Printf("LoadLMP: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}
	Image_Copy8bitRGBA(f + 8, image_rgba, image_width * image_height, d_8to24table);
	return image_rgba;
}

/*
============
LoadLMP
============
*/
qbyte *LoadLMPAs8Bit (qbyte *f, int matchwidth, int matchheight)
{
	qbyte *image_8bit;
	int width, height;

	if (loadsize < 9)
	{
		Con_Printf("LoadLMPAs8Bit: invalid LMP file\n");
		return NULL;
	}

	// parse the very complicated header *chuckle*
	width = f[0] + f[1] * 256 + f[2] * 65536 + f[3] * 16777216;
	height = f[4] + f[5] * 256 + f[6] * 65536 + f[7] * 16777216;
	if ((unsigned) width > 4096 || (unsigned) height > 4096)
	{
		Con_Printf("LoadLMPAs8Bit: invalid size\n");
		return NULL;
	}
	if ((matchwidth && width != matchwidth) || (matchheight && height != matchheight))
		return NULL;

	if (loadsize < 8 + width * height)
	{
		Con_Printf("LoadLMPAs8Bit: invalid LMP file\n");
		return NULL;
	}

	image_width = width;
	image_height = height;

	image_8bit = Mem_Alloc(tempmempool, image_width * image_height);
	if (!image_8bit)
	{
		Con_Printf("LoadLMPAs8Bit: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}
	memcpy(image_8bit, f + 8, image_width * image_height);
	return image_8bit;
}

void Image_StripImageExtension (char *in, char *out)
{
	char *end, *temp;
	end = in + strlen(in);
	if ((end - in) >= 4)
	{
		temp = end - 4;
		if (strcmp(temp, ".tga") == 0 || strcmp(temp, ".pcx") == 0 || strcmp(temp, ".lmp") == 0)
			end = temp;
		while (in < end)
			*out++ = *in++;
		*out++ = 0;
	}
	else
		strcpy(out, in);
}

qbyte *loadimagepixels (char *filename, qboolean complain, int matchwidth, int matchheight)
{
	qbyte *f, *data;
	char basename[256], name[256], *c;
	Image_StripImageExtension(filename, basename); // strip .tga, .pcx and .lmp extensions to allow replacement by other types
	// replace *'s with #, so commandline utils don't get confused when dealing with the external files
	for (c = basename;*c;c++)
		if (*c == '*')
			*c = '#';
	sprintf (name, "textures/%s.tga", basename);
	f = COM_LoadFile(name, true);
	if (f)
	{
		data = LoadTGA (f, matchwidth, matchheight);
		Mem_Free(f);
		return data;
	}
	sprintf (name, "textures/%s.pcx", basename);
	f = COM_LoadFile(name, true);
	if (f)
	{
		data = LoadPCX (f, matchwidth, matchheight);
		Mem_Free(f);
		return data;
	}
	sprintf (name, "%s.tga", basename);
	f = COM_LoadFile(name, true);
	if (f)
	{
		data = LoadTGA (f, matchwidth, matchheight);
		Mem_Free(f);
		return data;
	}
	sprintf (name, "%s.pcx", basename);
	f = COM_LoadFile(name, true);
	if (f)
	{
		data = LoadPCX (f, matchwidth, matchheight);
		Mem_Free(f);
		return data;
	}
	sprintf (name, "%s.lmp", basename);
	f = COM_LoadFile(name, true);
	if (f)
	{
		data = LoadLMP (f, matchwidth, matchheight);
		Mem_Free(f);
		return data;
	}
	if (complain)
		Con_Printf ("Couldn't load %s.tga, .pcx, .lmp\n", filename);
	return NULL;
}

int image_makemask (qbyte *in, qbyte *out, int size)
{
	int		i, count;
	count = 0;
	for (i = 0;i < size;i++)
	{
		out[0] = out[1] = out[2] = 255;
		out[3] = in[3];
		if (in[3] != 255)
			count++;
		in += 4;
		out += 4;
	}
	return count;
}

qbyte* loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight)
{
	qbyte *in, *data;
	in = data = loadimagepixels(filename, complain, matchwidth, matchheight);
	if (!data)
		return NULL;
	if (image_makemask(data, data, image_width * image_height))
		return data; // some transparency
	else
	{
		Mem_Free(data);
		return NULL; // all opaque
	}
}

rtexture_t *loadtextureimage (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	qbyte *data;
	rtexture_t *rt;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (pool, filename, image_width, image_height, data, TEXTYPE_RGBA, TEXF_ALPHA | (mipmap ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	Mem_Free(data);
	return rt;
}

rtexture_t *loadtextureimagemask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	qbyte *data;
	rtexture_t *rt;
	if (!(data = loadimagepixelsmask (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (pool, filename, image_width, image_height, data, TEXTYPE_RGBA, TEXF_ALPHA | (mipmap ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	Mem_Free(data);
	return rt;
}

rtexture_t *image_masktex;
rtexture_t *loadtextureimagewithmask (rtexturepool_t *pool, char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	int count;
	qbyte *data;
	char *filename2;
	rtexture_t *rt;
	image_masktex = NULL;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (pool, filename, image_width, image_height, data, TEXTYPE_RGBA, TEXF_ALPHA | (mipmap ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	count = image_makemask(data, data, image_width * image_height);
	if (count)
	{
		filename2 = Mem_Alloc(tempmempool, strlen(filename) + 6);
		sprintf(filename2, "%s_mask", filename);
		image_masktex = R_LoadTexture (pool, filename2, image_width, image_height, data, TEXTYPE_RGBA, TEXF_ALPHA | (mipmap ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
		Mem_Free(filename2);
	}
	Mem_Free(data);
	return rt;
}

qboolean Image_WriteTGARGB_preflipped (char *filename, int width, int height, qbyte *data)
{
	qboolean ret;
	qbyte *buffer, *in, *out, *end;

	buffer = Mem_Alloc(tempmempool, width*height*3 + 18);

	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr
	in = data;
	out = buffer + 18;
	end = in + width*height*3;
	for (;in < end;in += 3)
	{
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
	}
	ret = COM_WriteFile (filename, buffer, width*height*3 + 18 );

	Mem_Free(buffer);
	return ret;
}

void Image_WriteTGARGB (char *filename, int width, int height, qbyte *data)
{
	int y;
	qbyte *buffer, *in, *out, *end;

	buffer = Mem_Alloc(tempmempool, width*height*3 + 18);

	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and flip upside down
	out = buffer + 18;
	for (y = height - 1;y >= 0;y--)
	{
		in = data + y * width * 3;
		end = in + width * 3;
		for (;in < end;in += 3)
		{
			*out++ = in[2];
			*out++ = in[1];
			*out++ = in[0];
		}
	}
	COM_WriteFile (filename, buffer, width*height*3 + 18 );

	Mem_Free(buffer);
}

void Image_WriteTGARGBA (char *filename, int width, int height, qbyte *data)
{
	int y;
	qbyte *buffer, *in, *out, *end;

	buffer = Mem_Alloc(tempmempool, width*height*4 + 18);

	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = 32;	// pixel size

	// swap rgba to bgra and flip upside down
	out = buffer + 18;
	for (y = height - 1;y >= 0;y--)
	{
		in = data + y * width * 4;
		end = in + width * 4;
		for (;in < end;in += 4)
		{
			*out++ = in[2];
			*out++ = in[1];
			*out++ = in[0];
			*out++ = in[3];
		}
	}
	COM_WriteFile (filename, buffer, width*height*4 + 18 );

	Mem_Free(buffer);
}

qboolean Image_CheckAlpha(qbyte *data, int size, qboolean rgba)
{
	qbyte *end;
	if (rgba)
	{
		// check alpha bytes
		for (end = data + size * 4, data += 3;data < end;data += 4)
			if (*data < 255)
				return 1;
	}
	else
	{
		// color 255 is transparent
		for (end = data + size;data < end;data++)
			if (*data == 255)
				return 1;
	}
	return 0;
}

static void Image_Resample32LerpLine (qbyte *in, qbyte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (qbyte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (qbyte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (qbyte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (qbyte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
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

static void Image_Resample24LerpLine (qbyte *in, qbyte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (qbyte) ((((in[3] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (qbyte) ((((in[4] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (qbyte) ((((in[5] - in[2]) * lerp) >> 16) + in[2]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

int resamplerowsize = 0;
qbyte *resamplerow1 = NULL;
qbyte *resamplerow2 = NULL;
mempool_t *resamplemempool = NULL;

#define LERPBYTE(i) r = resamplerow1[i];out[i] = (qbyte) ((((resamplerow2[i] - r) * lerp) >> 16) + r)
void Image_Resample32Lerp(void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth4 = inwidth*4, outwidth4 = outwidth*4;
	qbyte *inrow, *out;
	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
	Image_Resample32LerpLine (inrow + inwidth4, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
				Image_Resample32LerpLine (inrow + inwidth4, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, resamplerow1, outwidth4);
		}
	}
}

void Image_Resample32Nearest(void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
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
		while (j >= 0)
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out[2] = inrow[frac >> 16];frac += fracstep;
			out[3] = inrow[frac >> 16];frac += fracstep;
			out += 4;
			j -= 4;
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

void Image_Resample24Lerp(void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth3 = inwidth * 3, outwidth3 = outwidth * 3;
	qbyte *inrow, *out;
	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
	Image_Resample24LerpLine (inrow + inwidth3, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth3*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
				Image_Resample24LerpLine (inrow + inwidth3, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				out += 12;
				resamplerow1 += 12;
				resamplerow2 += 12;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				out += 6;
				resamplerow1 += 6;
				resamplerow2 += 6;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				out += 3;
				resamplerow1 += 3;
				resamplerow2 += 3;
			}
			resamplerow1 -= outwidth3;
			resamplerow2 -= outwidth3;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (qbyte *)indata + inwidth3*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, resamplerow1, outwidth3);
		}
	}
}

void Image_Resample24Nolerp(void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, f, inwidth3 = inwidth * 3;
	unsigned frac, fracstep;
	qbyte *inrow, *out;
	out = outdata;

	fracstep = inwidth*0x10000/outwidth;
	for (i = 0;i < outheight;i++)
	{
		inrow = (qbyte *)indata + inwidth3*(i*inheight/outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			j -= 4;
		}
		if (j & 2)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			out += 2;
		}
		if (j & 1)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			out += 1;
		}
	}
}

/*
================
Image_Resample
================
*/
void Image_Resample (void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality)
{
	if (indepth != 1 || outdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	if (resamplerowsize < outwidth*4)
	{
		if (resamplerow1)
			Mem_Free(resamplerow1);
		resamplerowsize = outwidth*4;
		if (!resamplemempool)
			resamplemempool = Mem_AllocPool("Image Scaling Buffer");
		resamplerow1 = Mem_Alloc(resamplemempool, resamplerowsize*2);
		resamplerow2 = resamplerow1 + resamplerowsize;
	}
	if (bytesperpixel == 4)
	{
		if (quality)
			Image_Resample32Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample32Nearest(indata, inwidth, inheight, outdata, outwidth, outheight);
	}
	else if (bytesperpixel == 3)
	{
		if (quality)
			Image_Resample24Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample24Nolerp(indata, inwidth, inheight, outdata, outwidth, outheight);
	}
	else
		Sys_Error("Image_Resample: unsupported bytesperpixel %i\n", bytesperpixel);
}

// in can be the same as out
void Image_MipReduce(qbyte *in, qbyte *out, int *width, int *height, int *depth, int destwidth, int destheight, int destdepth, int bytesperpixel)
{
	int x, y, nextrow;
	if (*depth != 1 || destdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	nextrow = *width * bytesperpixel;
	if (*width > destwidth)
	{
		*width >>= 1;
		if (*height > destheight)
		{
			// reduce both
			*height >>= 1;
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[4] + in[nextrow  ] + in[nextrow+4]) >> 2);
						out[1] = (qbyte) ((in[1] + in[5] + in[nextrow+1] + in[nextrow+5]) >> 2);
						out[2] = (qbyte) ((in[2] + in[6] + in[nextrow+2] + in[nextrow+6]) >> 2);
						out[3] = (qbyte) ((in[3] + in[7] + in[nextrow+3] + in[nextrow+7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow; // skip a line
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[3] + in[nextrow  ] + in[nextrow+3]) >> 2);
						out[1] = (qbyte) ((in[1] + in[4] + in[nextrow+1] + in[nextrow+4]) >> 2);
						out[2] = (qbyte) ((in[2] + in[5] + in[nextrow+2] + in[nextrow+5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow; // skip a line
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
		else
		{
			// reduce width
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[4]) >> 1);
						out[1] = (qbyte) ((in[1] + in[5]) >> 1);
						out[2] = (qbyte) ((in[2] + in[6]) >> 1);
						out[3] = (qbyte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[3]) >> 1);
						out[1] = (qbyte) ((in[1] + in[4]) >> 1);
						out[2] = (qbyte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
	}
	else
	{
		if (*height > destheight)
		{
			// reduce height
			*height >>= 1;
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[nextrow  ]) >> 1);
						out[1] = (qbyte) ((in[1] + in[nextrow+1]) >> 1);
						out[2] = (qbyte) ((in[2] + in[nextrow+2]) >> 1);
						out[3] = (qbyte) ((in[3] + in[nextrow+3]) >> 1);
						out += 4;
						in += 4;
					}
					in += nextrow; // skip a line
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (qbyte) ((in[0] + in[nextrow  ]) >> 1);
						out[1] = (qbyte) ((in[1] + in[nextrow+1]) >> 1);
						out[2] = (qbyte) ((in[2] + in[nextrow+2]) >> 1);
						out += 3;
						in += 3;
					}
					in += nextrow; // skip a line
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
		else
			Sys_Error("Image_MipReduce: desired size already achieved\n");
	}
}

