
#include "quakedef.h"

int		image_width;
int		image_height;

// note: pal must be 32bit color
void Image_Copy8bitRGBA(byte *in, byte *out, int pixels, int *pal)
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

void Image_CopyRGBAGamma(byte *in, byte *out, int pixels)
{
	while (pixels--)
	{
		out[0] = texgamma[in[0]];
		out[1] = texgamma[in[1]];
		out[2] = texgamma[in[2]];
		out[3] =          in[3] ;
		in += 4;
		out += 4;
	}
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
byte* LoadPCX (byte *f, int matchwidth, int matchheight)
{
	pcx_t	pcx;
	byte	*palette, *a, *b, *image_rgba, *fin, *pbuf;
	int		x, y, dataByte, runLength;

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
		qfree(f);
		return NULL;
	}

	if (matchwidth && (pcx.xmax+1) != matchwidth)
	{
		qfree(f);
		return NULL;
	}
	if (matchheight && (pcx.ymax+1) != matchheight)
	{
		qfree(f);
		return NULL;
	}

	image_width = pcx.xmax+1;
	image_height = pcx.ymax+1;

	image_rgba = qmalloc(image_width*image_height*4);
	pbuf = image_rgba + image_width*image_height*3;

	for (y = 0;y < image_height;y++)
	{
		a = pbuf + y * image_width;
		for (x = 0;x < image_width;)
		{
			dataByte = *fin++;
			if(dataByte >= 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *fin++;
				if (runLength)
				{
					x += runLength;
					while(runLength--)
						*a++ = dataByte;
				}
			}
			else
			{
				x++;
				*a++ = dataByte;
			}
		}
	}

	palette = fin;
	a = pbuf;
	b = image_rgba;

	for(x = 0;x < image_width*image_height;x++)
	{
		y = *b++ * 3;
		*a++ = palette[y];
		*a++ = palette[y+1];
		*a++ = palette[y+2];
		*a++ = 255;
	}

	qfree(f);
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
byte* LoadTGA (byte *f, int matchwidth, int matchheight)
{
	int columns, rows, numPixels, row, column;
	byte *pixbuf, *image_rgba, *fin;

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
		Host_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0	|| (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Host_Error ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	image_rgba = qmalloc(numPixels*4);

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
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					*pixbuf++ = 255;
					fin += 3;
					break;
				case 32:
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
				packetHeader = *fin++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{
					// run-length packet
					switch (targa_header.pixel_size)
					{
					case 24:
						blue = *fin++;
						green = *fin++;
						red = *fin++;
						alphabyte = 255;
						break;
					case 32:
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
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							*pixbuf++ = 255;
							fin += 3;
							break;
						case 32:
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
	
	image_width = columns;
	image_height = rows;
	free(f);
	return image_rgba;
}

/*
============
LoadLMP
============
*/
byte* LoadLMP (byte *f, int matchwidth, int matchheight)
{
	byte	*image_rgba;
	int		width, height;

	// parse the very complicated header *chuckle*
	width = LittleLong(((int *)f)[0]);
	height = LittleLong(((int *)f)[1]);
	if ((unsigned) width > 4096 || (unsigned) height > 4096)
	{
		qfree(f);
		Host_Error("LoadLMP: invalid size\n");
	}
	if (matchwidth && width != matchwidth)
	{
		qfree(f);
		return NULL;
	}
	if (matchheight && height != matchheight)
	{
		qfree(f);
		return NULL;
	}

	image_rgba = qmalloc(width*height*4);
	Image_Copy8bitRGBA(f + 8, image_rgba, width*height, d_8to24table);
	image_width = width;
	image_height = height;
	qfree(f);
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

byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight)
{
	byte	*f;
	char	basename[256], name[256];
	byte	*c;
	Image_StripImageExtension(filename, basename); // strip .tga, .pcx and .lmp extensions to allow replacement by other types
	// replace *'s with #, so commandline utils don't get confused when dealing with the external files
	for (c = basename;*c;c++)
		if (*c == '*')
			*c = '#';
	sprintf (name, "textures/%s.tga", basename);
	f = COM_LoadMallocFile(name, true);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	sprintf (name, "textures/%s.pcx", basename);
	f = COM_LoadMallocFile(name, true);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	sprintf (name, "%s.tga", basename);
	f = COM_LoadMallocFile(name, true);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	sprintf (name, "%s.pcx", basename);
	f = COM_LoadMallocFile(name, true);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	sprintf (name, "%s.lmp", basename);
	f = COM_LoadMallocFile(name, true);
	if (f)
		return LoadLMP (f, matchwidth, matchheight);
	if (complain)
		Con_Printf ("Couldn't load %s.tga, .pcx, .lmp\n", filename);
	return NULL;
}

int image_makemask (byte *in, byte *out, int size)
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

byte* loadimagepixelsmask (char* filename, qboolean complain, int matchwidth, int matchheight)
{
	byte	*in, *data;
	in = data = loadimagepixels(filename, complain, matchwidth, matchheight);
	if (!data)
		return NULL;
	if (image_makemask(data, data, image_width * image_height))
		return data; // some transparency
	else
	{
		qfree(data);
		return NULL; // all opaque
	}
}

rtexture_t *loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	byte *data;
	rtexture_t *rt;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (filename, image_width, image_height, data, TEXF_ALPHA | TEXF_RGBA | (mipmap ? TEXF_MIPMAP : 0) | (mipmap ? TEXF_PRECACHE : 0));
	qfree(data);
	return rt;
}

rtexture_t *loadtextureimagemask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	byte *data;
	rtexture_t *rt;
	if (!(data = loadimagepixelsmask (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (filename, image_width, image_height, data, TEXF_ALPHA | TEXF_RGBA | (mipmap ? TEXF_MIPMAP : 0) | (mipmap ? TEXF_PRECACHE : 0));
	qfree(data);
	return rt;
}

rtexture_t *image_masktex;
rtexture_t *loadtextureimagewithmask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean precache)
{
	int count;
	byte *data;
	char *filename2;
	rtexture_t *rt;
	image_masktex = NULL;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	rt = R_LoadTexture (filename, image_width, image_height, data, TEXF_ALPHA | TEXF_RGBA | (mipmap ? TEXF_MIPMAP : 0) | (mipmap ? TEXF_PRECACHE : 0));
	count = image_makemask(data, data, image_width * image_height);
	if (count)
	{
		filename2 = qmalloc(strlen(filename) + 6);
		sprintf(filename2, "%s_mask", filename);
		image_masktex = R_LoadTexture (filename2, image_width, image_height, data, TEXF_ALPHA | TEXF_RGBA | (mipmap ? TEXF_MIPMAP : 0) | (mipmap ? TEXF_PRECACHE : 0));
		qfree(filename2);
	}
	qfree(data);
	return rt;
}

void Image_WriteTGARGB_preflipped (char *filename, int width, int height, byte *data)
{
	byte *buffer, *in, *out, *end;

	buffer = qmalloc(width*height*3 + 18);

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

	qfree(buffer);
}

void Image_WriteTGARGB (char *filename, int width, int height, byte *data)
{
	int y;
	byte *buffer, *in, *out, *end;

	buffer = qmalloc(width*height*3 + 18);

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

	qfree(buffer);
}

void Image_WriteTGARGBA (char *filename, int width, int height, byte *data)
{
	int y;
	byte *buffer, *in, *out, *end;

	buffer = qmalloc(width*height*4 + 18);

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

	qfree(buffer);
}

qboolean Image_CheckAlpha(byte *data, int size, qboolean rgba)
{
	byte *end;
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
