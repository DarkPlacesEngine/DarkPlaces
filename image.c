
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
		out[0] = qgamma[in[0]];
		out[1] = qgamma[in[1]];
		out[2] = qgamma[in[2]];
		out[3] =        in[3] ;
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
    unsigned 	data;			// unbounded
} pcx_t;

/*
============
LoadPCX
============
*/
byte* LoadPCX (FILE *f, int matchwidth, int matchheight)
{
	pcx_t	*pcx, pcxbuf;
	byte	palette[768];
	byte	*pix, *image_rgba;
	int		x, y;
	int		dataByte, runLength;
	int		count;

//
// parse the PCX file
//
	fread (&pcxbuf, 1, sizeof(pcxbuf), f);

	pcx = &pcxbuf;

	// LordHavoc: big-endian support ported from QF newtree
	pcx->xmax = LittleShort (pcx->xmax);
	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);
	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 || pcx->bits_per_pixel != 8 || pcx->xmax > 320 || pcx->ymax > 256)
	{
		Con_Printf ("Bad pcx file\n");
		return NULL;
	}

	if (matchwidth && (pcx->xmax+1) != matchwidth)
		return NULL;
	if (matchheight && (pcx->ymax+1) != matchheight)
		return NULL;

	// seek to palette
	fseek (f, -768, SEEK_END);
	fread (palette, 1, 768, f);

	fseek (f, sizeof(pcxbuf) - 4, SEEK_SET);

	count = (pcx->xmax+1) * (pcx->ymax+1);
	image_rgba = malloc( count * 4);

	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = image_rgba + 4*y*(pcx->xmax+1);
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = fgetc(f);

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = fgetc(f);
				if (runLength)
				{
					x += runLength;
					while(runLength--)
					{
						pix[0] = palette[dataByte*3];
						pix[1] = palette[dataByte*3+1];
						pix[2] = palette[dataByte*3+2];
						pix[3] = 255;
						pix += 4;
					}
				}
			}
			else
			{
				x++;
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				pix += 4;
			}

		}
	}
	fclose(f);
	image_width = pcx->xmax+1;
	image_height = pcx->ymax+1;
	return image_rgba;
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


TargaHeader		targa_header;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}


/*
=============
LoadTGA
=============
*/
byte* LoadTGA (FILE *fin, int matchwidth, int matchheight)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*image_rgba;

	targa_header.id_length = fgetc(fin);
	targa_header.colormap_type = fgetc(fin);
	targa_header.image_type = fgetc(fin);
	
	targa_header.colormap_index = fgetLittleShort(fin);
	targa_header.colormap_length = fgetLittleShort(fin);
	targa_header.colormap_size = fgetc(fin);
	targa_header.x_origin = fgetLittleShort(fin);
	targa_header.y_origin = fgetLittleShort(fin);
	targa_header.width = fgetLittleShort(fin);
	targa_header.height = fgetLittleShort(fin);
	if (matchwidth && targa_header.width != matchwidth)
		return NULL;
	if (matchheight && targa_header.height != matchheight)
		return NULL;
	targa_header.pixel_size = fgetc(fin);
	targa_header.attributes = fgetc(fin);

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10) 
		Host_Error ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type !=0 
		|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		Host_Error ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	image_rgba = malloc (numPixels*4);
	
	if (targa_header.id_length != 0)
		fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment
	
	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--) {
			pixbuf = image_rgba + row*columns*4;
			for(column=0; column<columns; column++) {
				unsigned char red = 0,green = 0,blue = 0,alphabyte = 0;
				switch (targa_header.pixel_size) {
					case 24:
							
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							alphabyte = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red = 0,green = 0,blue = 0,alphabyte = 0,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--) {
			pixbuf = image_rgba + row*columns*4;
			for(column=0; column<columns; ) {
				packetHeader=getc(fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								alphabyte = 255;
								break;
						case 32:
								blue = getc(fin);
								green = getc(fin);
								red = getc(fin);
								alphabyte = getc(fin);
								break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = getc(fin);
									green = getc(fin);
									red = getc(fin);
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = getc(fin);
									green = getc(fin);
									red = getc(fin);
									alphabyte = getc(fin);
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = image_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}
	
	fclose(fin);
	image_width = columns;
	image_height = rows;
	return image_rgba;
}

/*
============
LoadLMP
============
*/
byte* LoadLMP (FILE *f, int matchwidth, int matchheight)
{
	byte	*image_rgba;
	int		width, height;

	// parse the very complicated header *chuckle*
	width = fgetLittleLong(f);
	height = fgetLittleLong(f);
	if ((unsigned) width > 4096 || (unsigned) height > 4096)
		Host_Error("LoadLMP: invalid size\n");
	if (matchwidth && width != matchwidth)
		return NULL;
	if (matchheight && height != matchheight)
		return NULL;

	image_rgba = malloc(width*height*4);
	fread(image_rgba + width*height*3, 1, width*height, f);
	fclose(f);

	Image_Copy8bitRGBA(image_rgba + width*height*3, image_rgba, width*height, d_8to24table);
	image_width = width;
	image_height = height;
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
	FILE	*f;
	char	basename[256], name[256];
	byte	*image_rgba, *c;
	Image_StripImageExtension(filename, basename); // strip .tga, .pcx and .lmp extensions to allow replacement by other types
	// replace *'s with #, so commandline utils don't get confused when dealing with the external files
	for (c = basename;*c;c++)
		if (*c == '*')
			*c = '#';
	sprintf (name, "textures/%s.tga", basename);
	COM_FOpenFile (name, &f, true);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	sprintf (name, "textures/%s.pcx", basename);
	COM_FOpenFile (name, &f, true);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f, true);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f, true);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	sprintf (name, "%s.lmp", basename);
	COM_FOpenFile (name, &f, true);
	if (f)
		return LoadLMP (f, matchwidth, matchheight);
	if ((image_rgba = W_GetTexture(basename, matchwidth, matchheight)))
		return image_rgba;
	COM_StripExtension(filename, basename); // do it again with a * this time
	if ((image_rgba = W_GetTexture(basename, matchwidth, matchheight)))
		return image_rgba;
	if (complain)
		Con_Printf ("Couldn't load %s.tga or .pcx\n", filename);
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
		free(data);
		return NULL; // all opaque
	}
}

int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int texnum;
	byte *data;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	texnum = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, 4);
	free(data);
	return texnum;
}

int loadtextureimagemask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int texnum;
	byte *data;
	if (!(data = loadimagepixelsmask (filename, complain, matchwidth, matchheight)))
		return 0;
	texnum = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, 4);
	free(data);
	return texnum;
}

int image_masktexnum;
int loadtextureimagewithmask (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int texnum, count;
	byte *data;
	char *filename2;
	image_masktexnum = 0;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight)))
		return 0;
	texnum = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, 4);
	count = image_makemask(data, data, image_width * image_height);
	if (count)
	{
		filename2 = malloc(strlen(filename) + 6);
		sprintf(filename2, "%s_mask", filename);
		image_masktexnum = GL_LoadTexture (filename2, image_width, image_height, data, mipmap, true, 4);
		free(filename2);
	}
	free(data);
	return texnum;
}

void Image_WriteTGARGB_preflipped (char *filename, int width, int height, byte *data)
{
	byte *buffer, *in, *out, *end;

	buffer = malloc(width*height*3 + 18);

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

	free(buffer);
}

void Image_WriteTGARGB (char *filename, int width, int height, byte *data)
{
	int y;
	byte *buffer, *in, *out, *end;

	buffer = malloc(width*height*3 + 18);

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

	free(buffer);
}

void Image_WriteTGARGBA (char *filename, int width, int height, byte *data)
{
	int y;
	byte *buffer, *in, *out, *end;

	buffer = malloc(width*height*4 + 18);

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

	free(buffer);
}
