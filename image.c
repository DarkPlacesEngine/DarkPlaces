
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
	int columns, rows, row, column;
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

	columns = targa_header.width;
	rows = targa_header.height;

	image_rgba = Mem_Alloc(tempmempool, columns * rows * 4);
	if (!image_rgba)
	{
		Con_Printf ("LoadTGA: not enough memory for %i by %i image\n", columns, rows);
		return NULL;
	}

	fin = f + 18;
	if (targa_header.id_length != 0)
		fin += targa_header.id_length;  // skip TARGA image comment

	if (targa_header.image_type == 2)
	{
		// Uncompressed, RGB images
		for(row = rows - 1;row >= 0;row--)
		{
			pixbuf = image_rgba + row*columns*4;
			for(column = 0;column < columns;column++)
			{
				switch (targa_header.pixel_size)
				{
				case 24:
					if (fin + 3 > enddata)
						break;
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = 255;
					fin += 3;
					break;
				case 32:
					if (fin + 4 > enddata)
						break;
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = fin[3];
					fin += 4;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type==10)
	{
		// Runlength encoded RGB images
		unsigned char red = 0, green = 0, blue = 0, alphabyte = 0, packetHeader, packetSize, j;
		for(row = rows - 1;row >= 0;row--)
		{
			pixbuf = image_rgba + row * columns * 4;
			for(column = 0;column < columns;)
			{
				if (fin >= enddata)
					goto outofdata;
				packetHeader = *fin++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{
					// run-length packet
					switch (targa_header.pixel_size)
					{
					case 24:
						if (fin + 3 > enddata)
							goto outofdata;
						blue = *fin++;
						green = *fin++;
						red = *fin++;
						alphabyte = 255;
						break;
					case 32:
						if (fin + 4 > enddata)
							goto outofdata;
						blue = *fin++;
						green = *fin++;
						red = *fin++;
						alphabyte = *fin++;
						break;
					}

					for(j = 0;j < packetSize;j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns)
						{
							// run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row * columns * 4;
						}
					}
				}
				else
				{
					// non run-length packet
					for(j = 0;j < packetSize;j++)
					{
						switch (targa_header.pixel_size)
						{
						case 24:
							if (fin + 3 > enddata)
								goto outofdata;
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = 255;
							fin += 3;
							break;
						case 32:
							if (fin + 4 > enddata)
								goto outofdata;
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = fin[3];
							fin += 4;
							break;
						}
						column++;
						if (column == columns)
						{
							// pixel packet run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row * columns * 4;
						}
					}
				}
			}
			breakOut:;
		}
	}
outofdata:;

	image_width = columns;
	image_height = rows;
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

void Image_WriteTGARGB_preflipped (char *filename, int width, int height, qbyte *data)
{
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
	COM_WriteFile (filename, buffer, width*height*3 + 18 );

	Mem_Free(buffer);
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
