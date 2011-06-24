/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "image.h"
#include "wad.h"

#include "cl_video.h"
#include "cl_dyntexture.h"

#include "ft2.h"
#include "ft2_fontdefs.h"

dp_fonts_t dp_fonts;
static mempool_t *fonts_mempool = NULL;

cvar_t r_textshadow = {CVAR_SAVE, "r_textshadow", "0", "draws a shadow on all text to improve readability (note: value controls offset, 1 = 1 pixel, 1.5 = 1.5 pixels, etc)"};
cvar_t r_textbrightness = {CVAR_SAVE, "r_textbrightness", "0", "additional brightness for text color codes (0 keeps colors as is, 1 makes them all white)"};
cvar_t r_textcontrast = {CVAR_SAVE, "r_textcontrast", "1", "additional contrast for text color codes (1 keeps colors as is, 0 makes them all black)"};

cvar_t r_font_postprocess_blur = {CVAR_SAVE, "r_font_postprocess_blur", "0", "font blur amount"};
cvar_t r_font_postprocess_outline = {CVAR_SAVE, "r_font_postprocess_outline", "0", "font outline amount"};
cvar_t r_font_postprocess_shadow_x = {CVAR_SAVE, "r_font_postprocess_shadow_x", "0", "font shadow X shift amount, applied during outlining"};
cvar_t r_font_postprocess_shadow_y = {CVAR_SAVE, "r_font_postprocess_shadow_y", "0", "font shadow Y shift amount, applied during outlining"};
cvar_t r_font_postprocess_shadow_z = {CVAR_SAVE, "r_font_postprocess_shadow_z", "0", "font shadow Z shift amount, applied during blurring"};
cvar_t r_font_hinting = {CVAR_SAVE, "r_font_hinting", "3", "0 = no hinting, 1 = light autohinting, 2 = full autohinting, 3 = full hinting"};
cvar_t r_font_antialias = {CVAR_SAVE, "r_font_antialias", "1", "0 = monochrome, 1 = grey" /* , 2 = rgb, 3 = bgr" */};

extern cvar_t v_glslgamma;

//=============================================================================
/* Support Routines */

#define FONT_FILESIZE 13468
static cachepic_t *cachepichash[CACHEPICHASHSIZE];
static cachepic_t cachepics[MAX_CACHED_PICS];
static int numcachepics;

rtexturepool_t *drawtexturepool;

static const unsigned char concharimage[FONT_FILESIZE] =
{
#include "lhfont.h"
};

static rtexture_t *draw_generateconchars(void)
{
	int i;
	unsigned char *data;
	double random;
	rtexture_t *tex;

	data = LoadTGA_BGRA (concharimage, FONT_FILESIZE, NULL);
// Gold numbers
	for (i = 0;i < 8192;i++)
	{
		random = lhrandom (0.0,1.0);
		data[i*4+3] = data[i*4+0];
		data[i*4+2] = 83 + (unsigned char)(random * 64);
		data[i*4+1] = 71 + (unsigned char)(random * 32);
		data[i*4+0] = 23 + (unsigned char)(random * 16);
	}
// White chars
	for (i = 8192;i < 32768;i++)
	{
		random = lhrandom (0.0,1.0);
		data[i*4+3] = data[i*4+0];
		data[i*4+2] = 95 + (unsigned char)(random * 64);
		data[i*4+1] = 95 + (unsigned char)(random * 64);
		data[i*4+0] = 95 + (unsigned char)(random * 64);
	}
// Gold numbers
	for (i = 32768;i < 40960;i++)
	{
		random = lhrandom (0.0,1.0);
		data[i*4+3] = data[i*4+0];
		data[i*4+2] = 83 + (unsigned char)(random * 64);
		data[i*4+1] = 71 + (unsigned char)(random * 32);
		data[i*4+0] = 23 + (unsigned char)(random * 16);
	}
// Red chars
	for (i = 40960;i < 65536;i++)
	{
		random = lhrandom (0.0,1.0);
		data[i*4+3] = data[i*4+0];
		data[i*4+2] = 96 + (unsigned char)(random * 64);
		data[i*4+1] = 43 + (unsigned char)(random * 32);
		data[i*4+0] = 27 + (unsigned char)(random * 32);
	}

#if 0
	Image_WriteTGABGRA ("gfx/generated_conchars.tga", 256, 256, data);
#endif

	tex = R_LoadTexture2D(drawtexturepool, "conchars", 256, 256, data, TEXTYPE_BGRA, TEXF_ALPHA, -1, NULL);
	Mem_Free(data);
	return tex;
}

static rtexture_t *draw_generateditherpattern(void)
{
	int x, y;
	unsigned char pixels[8][8];
	for (y = 0;y < 8;y++)
		for (x = 0;x < 8;x++)
			pixels[y][x] = ((x^y) & 4) ? 254 : 0;
	return R_LoadTexture2D(drawtexturepool, "ditherpattern", 8, 8, pixels[0], TEXTYPE_PALETTE, TEXF_FORCENEAREST, -1, palette_bgra_transparent);
}

typedef struct embeddedpic_s
{
	const char *name;
	int width;
	int height;
	const char *pixels;
}
embeddedpic_t;

static const embeddedpic_t embeddedpics[] =
{
	{
	"gfx/prydoncursor001", 16, 16,
	"477777774......."
	"77.....6........"
	"7.....6........."
	"7....6.........."
	"7.....6........."
	"7..6...6........"
	"7.6.6...6......."
	"76...6...6......"
	"4.....6.6......."
	".......6........"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	},
	{
	"ui/mousepointer", 16, 16,
	"477777774......."
	"77.....6........"
	"7.....6........."
	"7....6.........."
	"7.....6........."
	"7..6...6........"
	"7.6.6...6......."
	"76...6...6......"
	"4.....6.6......."
	".......6........"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	},
	{
	"gfx/crosshair1", 16, 16,
	"................"
	"................"
	"................"
	"...33......33..."
	"...355....553..."
	"....577..775...."
	".....77..77....."
	"................"
	"................"
	".....77..77....."
	"....577..775...."
	"...355....553..."
	"...33......33..."
	"................"
	"................"
	"................"
	},
	{
	"gfx/crosshair2", 16, 16,
	"................"
	"................"
	"................"
	"...3........3..."
	"....5......5...."
	".....7....7....."
	"......7..7......"
	"................"
	"................"
	"......7..7......"
	".....7....7....."
	"....5......5...."
	"...3........3..."
	"................"
	"................"
	"................"
	},
	{
	"gfx/crosshair3", 16, 16,
	"................"
	".......77......."
	".......77......."
	"................"
	"................"
	".......44......."
	".......44......."
	".77..44..44..77."
	".77..44..44..77."
	".......44......."
	".......44......."
	"................"
	"................"
	".......77......."
	".......77......."
	"................"
	},
	{
	"gfx/crosshair4", 16, 16,
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"........7777777."
	"........752....."
	"........72......"
	"........7......."
	"........7......."
	"........7......."
	"........7......."
	"................"
	},
	{
	"gfx/crosshair5", 8, 8,
	"........"
	"........"
	"....7..."
	"........"
	"..7.7.7."
	"........"
	"....7..."
	"........"
	},
	{
	"gfx/crosshair6", 2, 2,
	"77"
	"77"
	},
	{
	"gfx/crosshair7", 16, 16,
	"................"
	".3............3."
	"..5...2332...5.."
	"...7.3....3.7..."
	"....7......7...."
	"...3.7....7.3..."
	"..2...7..7...2.."
	"..3..........3.."
	"..3..........3.."
	"..2...7..7...2.."
	"...3.7....7.3..."
	"....7......7...."
	"...7.3....3.7..."
	"..5...2332...5.."
	".3............3."
	"................"
	},
	{NULL, 0, 0, NULL}
};

static rtexture_t *draw_generatepic(const char *name, qboolean quiet)
{
	const embeddedpic_t *p;
	for (p = embeddedpics;p->name;p++)
		if (!strcmp(name, p->name))
			return R_LoadTexture2D(drawtexturepool, p->name, p->width, p->height, (const unsigned char *)p->pixels, TEXTYPE_PALETTE, TEXF_ALPHA, -1, palette_bgra_embeddedpic);
	if (!strcmp(name, "gfx/conchars"))
		return draw_generateconchars();
	if (!strcmp(name, "gfx/colorcontrol/ditherpattern"))
		return draw_generateditherpattern();
	if (!quiet)
		Con_DPrintf("Draw_CachePic: failed to load %s\n", name);
	return r_texture_notexture;
}

int draw_frame = 1;

/*
================
Draw_CachePic
================
*/
// FIXME: move this to client somehow
cachepic_t *Draw_CachePic_Flags(const char *path, unsigned int cachepicflags)
{
	int crc, hashkey;
	unsigned char *pixels = NULL;
	cachepic_t *pic;
	fs_offset_t lmpsize;
	unsigned char *lmpdata;
	char lmpname[MAX_QPATH];
	int texflags;
	int j;
	qboolean ddshasalpha;
	float ddsavgcolor[4];
	qboolean loaded = false;

	texflags = TEXF_ALPHA;
	if (!(cachepicflags & CACHEPICFLAG_NOCLAMP))
		texflags |= TEXF_CLAMP;
	if (!(cachepicflags & CACHEPICFLAG_NOCOMPRESSION) && gl_texturecompression_2d.integer && gl_texturecompression.integer)
		texflags |= TEXF_COMPRESS;

	// check whether the picture has already been cached
	crc = CRC_Block((unsigned char *)path, strlen(path));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
	{
		if (!strcmp (path, pic->name))
		{
			// if it was created (or replaced) by Draw_NewPic, just return it
			if(pic->flags & CACHEPICFLAG_NEWPIC)
				return pic;
			if (!((pic->texflags ^ texflags) & ~(TEXF_COMPRESS))) // ignore TEXF_COMPRESS when comparing, because fallback pics remove the flag
			{
				if(!(cachepicflags & CACHEPICFLAG_NOTPERSISTENT))
				{
					if(pic->tex)
						pic->autoload = false; // persist it
					else
						goto reload; // load it below, and then persist
				}
				return pic;
			}
		}
	}

	if (numcachepics == MAX_CACHED_PICS)
	{
		Con_Printf ("Draw_CachePic: numcachepics == MAX_CACHED_PICS\n");
		// FIXME: support NULL in callers?
		return cachepics; // return the first one
	}
	pic = cachepics + (numcachepics++);
	strlcpy (pic->name, path, sizeof(pic->name));
	// link into list
	pic->chain = cachepichash[hashkey];
	cachepichash[hashkey] = pic;

reload:
	// check whether it is an dynamic texture (if so, we can directly use its texture handler)
	pic->flags = cachepicflags;
	pic->tex = CL_GetDynTexture( path );
	// if so, set the width/height, too
	if( pic->tex ) {
		pic->width = R_TextureWidth(pic->tex);
		pic->height = R_TextureHeight(pic->tex);
		// we're done now (early-out)
		return pic;
	}

	pic->hasalpha = true; // assume alpha unless we know it has none
	pic->texflags = texflags;
	pic->autoload = (cachepicflags & CACHEPICFLAG_NOTPERSISTENT);
	pic->lastusedframe = draw_frame;

	// load a high quality image from disk if possible
	if (!loaded && r_texture_dds_load.integer != 0 && (pic->tex = R_LoadTextureDDSFile(drawtexturepool, va("dds/%s.dds", pic->name), pic->texflags, &ddshasalpha, ddsavgcolor, 0)))
	{
		// note this loads even if autoload is true, otherwise we can't get the width/height
		loaded = true;
		pic->hasalpha = ddshasalpha;
		pic->width = R_TextureWidth(pic->tex);
		pic->height = R_TextureHeight(pic->tex);
	}
	if (!loaded && ((pixels = loadimagepixelsbgra(pic->name, false, true, false, NULL)) || (!strncmp(pic->name, "gfx/", 4) && (pixels = loadimagepixelsbgra(pic->name+4, false, true, false, NULL)))))
	{
		loaded = true;
		pic->hasalpha = false;
		if (pic->texflags & TEXF_ALPHA)
		{
			for (j = 3;j < image_width * image_height * 4;j += 4)
			{
				if (pixels[j] < 255)
				{
					pic->hasalpha = true;
					break;
				}
			}
		}

		pic->width = image_width;
		pic->height = image_height;
		if (!pic->autoload)
		{
			pic->tex = R_LoadTexture2D(drawtexturepool, pic->name, image_width, image_height, pixels, vid.sRGB2D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, pic->texflags & (pic->hasalpha ? ~0 : ~TEXF_ALPHA), -1, NULL);
			if (r_texture_dds_save.integer && qglGetCompressedTexImageARB && pic->tex)
				R_SaveTextureDDSFile(pic->tex, va("dds/%s.dds", pic->name), r_texture_dds_save.integer < 2, pic->hasalpha);
		}
	}
	if (!loaded)
	{
		pic->autoload = false;
		// never compress the fallback images
		pic->texflags &= ~TEXF_COMPRESS;
	}

	// now read the low quality version (wad or lmp file), and take the pic
	// size from that even if we don't upload the texture, this way the pics
	// show up the right size in the menu even if they were replaced with
	// higher or lower resolution versions
	dpsnprintf(lmpname, sizeof(lmpname), "%s.lmp", pic->name);
	if (!strncmp(pic->name, "gfx/", 4) && (lmpdata = FS_LoadFile(lmpname, tempmempool, false, &lmpsize)))
	{
		if (developer_loading.integer)
			Con_Printf("loading lump \"%s\"\n", pic->name);

		if (lmpsize >= 9)
		{
			pic->width = lmpdata[0] + lmpdata[1] * 256 + lmpdata[2] * 65536 + lmpdata[3] * 16777216;
			pic->height = lmpdata[4] + lmpdata[5] * 256 + lmpdata[6] * 65536 + lmpdata[7] * 16777216;
			// if no high quality replacement image was found, upload the original low quality texture
			if (!loaded)
			{
				loaded = true;
				pic->tex = R_LoadTexture2D(drawtexturepool, pic->name, pic->width, pic->height, lmpdata + 8, vid.sRGB2D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, pic->texflags, -1, palette_bgra_transparent);
			}
		}
		Mem_Free(lmpdata);
	}
	else if ((lmpdata = W_GetLumpName (pic->name + 4)))
	{
		if (developer_loading.integer)
			Con_Printf("loading gfx.wad lump \"%s\"\n", pic->name + 4);

		if (!strcmp(pic->name, "gfx/conchars"))
		{
			// conchars is a raw image and with color 0 as transparent instead of 255
			pic->width = 128;
			pic->height = 128;
			// if no high quality replacement image was found, upload the original low quality texture
			if (!loaded)
			{
				loaded = true;
				pic->tex = R_LoadTexture2D(drawtexturepool, pic->name, 128, 128, lmpdata, vid.sRGB2D != 0 ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, pic->texflags, -1, palette_bgra_font);
			}
		}
		else
		{
			pic->width = lmpdata[0] + lmpdata[1] * 256 + lmpdata[2] * 65536 + lmpdata[3] * 16777216;
			pic->height = lmpdata[4] + lmpdata[5] * 256 + lmpdata[6] * 65536 + lmpdata[7] * 16777216;
			// if no high quality replacement image was found, upload the original low quality texture
			if (!loaded)
			{
				loaded = true;
				pic->tex = R_LoadTexture2D(drawtexturepool, pic->name, pic->width, pic->height, lmpdata + 8, vid.sRGB2D != 0 ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, pic->texflags, -1, palette_bgra_transparent);
			}
		}
	}

	if (pixels)
	{
		Mem_Free(pixels);
		pixels = NULL;
	}
	if (!loaded)
	{
		// if it's not found on disk, generate an image
		pic->tex = draw_generatepic(pic->name, (cachepicflags & CACHEPICFLAG_QUIET) != 0);
		pic->width = R_TextureWidth(pic->tex);
		pic->height = R_TextureHeight(pic->tex);
	}

	return pic;
}

cachepic_t *Draw_CachePic (const char *path)
{
	return Draw_CachePic_Flags (path, 0); // default to persistent!
}

rtexture_t *Draw_GetPicTexture(cachepic_t *pic)
{
	if (pic->autoload && !pic->tex)
	{
		if (pic->tex == NULL && r_texture_dds_load.integer != 0)
		{
			qboolean ddshasalpha;
			float ddsavgcolor[4];
			pic->tex = R_LoadTextureDDSFile(drawtexturepool, va("dds/%s.dds", pic->name), pic->texflags, &ddshasalpha, ddsavgcolor, 0);
		}
		if (pic->tex == NULL)
		{
			pic->tex = loadtextureimage(drawtexturepool, pic->name, false, pic->texflags, true, vid.sRGB2D);
			if (r_texture_dds_save.integer && qglGetCompressedTexImageARB && pic->tex)
				R_SaveTextureDDSFile(pic->tex, va("dds/%s.dds", pic->name), r_texture_dds_save.integer < 2, pic->hasalpha);
		}
		if (pic->tex == NULL && !strncmp(pic->name, "gfx/", 4))
		{
			pic->tex = loadtextureimage(drawtexturepool, pic->name+4, false, pic->texflags, true, vid.sRGB2D);
			if (r_texture_dds_save.integer && qglGetCompressedTexImageARB && pic->tex)
				R_SaveTextureDDSFile(pic->tex, va("dds/%s.dds", pic->name), r_texture_dds_save.integer < 2, pic->hasalpha);
		}
		if (pic->tex == NULL)
			pic->tex = draw_generatepic(pic->name, true);
	}
	pic->lastusedframe = draw_frame;
	return pic->tex;
}

void Draw_Frame(void)
{
	int i;
	cachepic_t *pic;
	static double nextpurgetime;
	if (nextpurgetime > realtime)
		return;
	nextpurgetime = realtime + 0.05;
	for (i = 0, pic = cachepics;i < numcachepics;i++, pic++)
	{
		if (pic->autoload && pic->tex && pic->lastusedframe < draw_frame)
		{
			R_FreeTexture(pic->tex);
			pic->tex = NULL;
		}
	}
	draw_frame++;
}

cachepic_t *Draw_NewPic(const char *picname, int width, int height, int alpha, unsigned char *pixels_bgra)
{
	int crc, hashkey;
	cachepic_t *pic;

	crc = CRC_Block((unsigned char *)picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
		if (!strcmp (picname, pic->name))
			break;

	if (pic)
	{
		if (pic->flags == CACHEPICFLAG_NEWPIC && pic->tex && pic->width == width && pic->height == height)
		{
			R_UpdateTexture(pic->tex, pixels_bgra, 0, 0, 0, width, height, 1);
			return pic;
		}
	}
	else
	{
		if (pic == NULL)
		{
			if (numcachepics == MAX_CACHED_PICS)
			{
				Con_Printf ("Draw_NewPic: numcachepics == MAX_CACHED_PICS\n");
				// FIXME: support NULL in callers?
				return cachepics; // return the first one
			}
			pic = cachepics + (numcachepics++);
			strlcpy (pic->name, picname, sizeof(pic->name));
			// link into list
			pic->chain = cachepichash[hashkey];
			cachepichash[hashkey] = pic;
		}
	}

	pic->flags = CACHEPICFLAG_NEWPIC; // disable texflags checks in Draw_CachePic
	pic->width = width;
	pic->height = height;
	if (pic->tex)
		R_FreeTexture(pic->tex);
	pic->tex = R_LoadTexture2D(drawtexturepool, picname, width, height, pixels_bgra, TEXTYPE_BGRA, (alpha ? TEXF_ALPHA : 0), -1, NULL);
	return pic;
}

void Draw_FreePic(const char *picname)
{
	int crc;
	int hashkey;
	cachepic_t *pic;
	// this doesn't really free the pic, but does free it's texture
	crc = CRC_Block((unsigned char *)picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
	{
		if (!strcmp (picname, pic->name) && pic->tex)
		{
			R_FreeTexture(pic->tex);
			pic->tex = NULL;
			pic->width = 0;
			pic->height = 0;
			return;
		}
	}
}

static float snap_to_pixel_x(float x, float roundUpAt);
extern int con_linewidth; // to force rewrapping
void LoadFont(qboolean override, const char *name, dp_font_t *fnt, float scale, float voffset)
{
	int i, ch;
	float maxwidth;
	char widthfile[MAX_QPATH];
	char *widthbuf;
	fs_offset_t widthbufsize;

	if(override || !fnt->texpath[0])
	{
		strlcpy(fnt->texpath, name, sizeof(fnt->texpath));
		// load the cvars when the font is FIRST loader
		fnt->settings.scale = scale;
		fnt->settings.voffset = voffset;
		fnt->settings.antialias = r_font_antialias.integer;
		fnt->settings.hinting = r_font_hinting.integer;
		fnt->settings.outline = r_font_postprocess_outline.value;
		fnt->settings.blur = r_font_postprocess_blur.value;
		fnt->settings.shadowx = r_font_postprocess_shadow_x.value;
		fnt->settings.shadowy = r_font_postprocess_shadow_y.value;
		fnt->settings.shadowz = r_font_postprocess_shadow_z.value;
	}
	// fix bad scale
	if (fnt->settings.scale <= 0)
		fnt->settings.scale = 1;

	if(drawtexturepool == NULL)
		return; // before gl_draw_start, so will be loaded later

	if(fnt->ft2)
	{
		// clear freetype font
		Font_UnloadFont(fnt->ft2);
		Mem_Free(fnt->ft2);
		fnt->ft2 = NULL;
	}

	if(fnt->req_face != -1)
	{
		if(!Font_LoadFont(fnt->texpath, fnt))
			Con_DPrintf("Failed to load font-file for '%s', it will not support as many characters.\n", fnt->texpath);
	}

	fnt->tex = Draw_CachePic_Flags(fnt->texpath, CACHEPICFLAG_QUIET | CACHEPICFLAG_NOCOMPRESSION)->tex;
	if(fnt->tex == r_texture_notexture)
	{
		for (i = 0; i < MAX_FONT_FALLBACKS; ++i)
		{
			if (!fnt->fallbacks[i][0])
				break;
			fnt->tex = Draw_CachePic_Flags(fnt->fallbacks[i], CACHEPICFLAG_QUIET | CACHEPICFLAG_NOCOMPRESSION)->tex;
			if(fnt->tex != r_texture_notexture)
				break;
		}
		if(fnt->tex == r_texture_notexture)
		{
			fnt->tex = Draw_CachePic_Flags("gfx/conchars", CACHEPICFLAG_NOCOMPRESSION)->tex;
			strlcpy(widthfile, "gfx/conchars.width", sizeof(widthfile));
		}
		else
			dpsnprintf(widthfile, sizeof(widthfile), "%s.width", fnt->fallbacks[i]);
	}
	else
		dpsnprintf(widthfile, sizeof(widthfile), "%s.width", fnt->texpath);

	// unspecified width == 1 (base width)
	for(ch = 0; ch < 256; ++ch)
		fnt->width_of[ch] = 1;

	// FIXME load "name.width", if it fails, fill all with 1
	if((widthbuf = (char *) FS_LoadFile(widthfile, tempmempool, true, &widthbufsize)))
	{
		float extraspacing = 0;
		const char *p = widthbuf;

		ch = 0;
		while(ch < 256)
		{
			if(!COM_ParseToken_Simple(&p, false, false))
				return;

			switch(*com_token)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '+':
				case '-':
				case '.':
					fnt->width_of[ch] = atof(com_token) + extraspacing;
					ch++;
					break;
				default:
					if(!strcmp(com_token, "extraspacing"))
					{
						if(!COM_ParseToken_Simple(&p, false, false))
							return;
						extraspacing = atof(com_token);
					}
					else if(!strcmp(com_token, "scale"))
					{
						if(!COM_ParseToken_Simple(&p, false, false))
							return;
						fnt->settings.scale = atof(com_token);
					}
					else
					{
						Con_Printf("Warning: skipped unknown font property %s\n", com_token);
						if(!COM_ParseToken_Simple(&p, false, false))
							return;
					}
					break;
			}
		}

		Mem_Free(widthbuf);
	}

	if(fnt->ft2)
	{
		for (i = 0; i < MAX_FONT_SIZES; ++i)
		{
			ft2_font_map_t *map = Font_MapForIndex(fnt->ft2, i);
			if (!map)
				break;
			for(ch = 0; ch < 256; ++ch)
				map->width_of[ch] = Font_SnapTo(fnt->width_of[ch], 1/map->size);
		}
	}

	maxwidth = fnt->width_of[0];
	for(i = 1; i < 256; ++i)
		maxwidth = max(maxwidth, fnt->width_of[i]);
	fnt->maxwidth = maxwidth;

	// fix up maxwidth for overlap
	fnt->maxwidth *= fnt->settings.scale;

	if(fnt == FONT_CONSOLE)
		con_linewidth = -1; // rewrap console in next frame
}

extern cvar_t developer_font;
dp_font_t *FindFont(const char *title, qboolean allocate_new)
{
	int i, oldsize;

	// find font
	for(i = 0; i < dp_fonts.maxsize; ++i)
		if(!strcmp(dp_fonts.f[i].title, title))
			return &dp_fonts.f[i];
	// if not found - try allocate
	if (allocate_new)
	{
		// find any font with empty title
		for(i = 0; i < dp_fonts.maxsize; ++i)
		{
			if(!strcmp(dp_fonts.f[i].title, ""))
			{
				strlcpy(dp_fonts.f[i].title, title, sizeof(dp_fonts.f[i].title));
				return &dp_fonts.f[i];
			}
		}
		// if no any 'free' fonts - expand buffer
		oldsize = dp_fonts.maxsize;
		dp_fonts.maxsize = dp_fonts.maxsize + FONTS_EXPAND;
		if (developer_font.integer)
			Con_Printf("FindFont: enlarging fonts buffer (%i -> %i)\n", oldsize, dp_fonts.maxsize);
		dp_fonts.f = (dp_font_t *)Mem_Realloc(fonts_mempool, dp_fonts.f, sizeof(dp_font_t) * dp_fonts.maxsize);
		// relink ft2 structures
		for(i = 0; i < oldsize; ++i)
			if (dp_fonts.f[i].ft2)
				dp_fonts.f[i].ft2->settings = &dp_fonts.f[i].settings;
		// register a font in first expanded slot
		strlcpy(dp_fonts.f[oldsize].title, title, sizeof(dp_fonts.f[oldsize].title));
		return &dp_fonts.f[oldsize];
	}
	return NULL;
}

static float snap_to_pixel_x(float x, float roundUpAt)
{
	float pixelpos = x * vid.width / vid_conwidth.value;
	int snap = (int) pixelpos;
	if (pixelpos - snap >= roundUpAt) ++snap;
	return ((float)snap * vid_conwidth.value / vid.width);
	/*
	x = (int)(x * vid.width / vid_conwidth.value);
	x = (x * vid_conwidth.value / vid.width);
	return x;
	*/
}

static float snap_to_pixel_y(float y, float roundUpAt)
{
	float pixelpos = y * vid.height / vid_conheight.value;
	int snap = (int) pixelpos;
	if (pixelpos - snap > roundUpAt) ++snap;
	return ((float)snap * vid_conheight.value / vid.height);
	/*
	y = (int)(y * vid.height / vid_conheight.value);
	y = (y * vid_conheight.value / vid.height);
	return y;
	*/
}

static void LoadFont_f(void)
{
	dp_font_t *f;
	int i, sizes;
	const char *filelist, *c, *cm;
	float sz, scale, voffset;
	char mainfont[MAX_QPATH];

	if(Cmd_Argc() < 2)
	{
		Con_Printf("Available font commands:\n");
		for(i = 0; i < dp_fonts.maxsize; ++i)
			if (dp_fonts.f[i].title[0])
				Con_Printf("  loadfont %s gfx/tgafile[...] [custom switches] [sizes...]\n", dp_fonts.f[i].title);
		Con_Printf("A font can simply be gfx/tgafile, or alternatively you\n"
			   "can specify multiple fonts and faces\n"
			   "Like this: gfx/vera-sans:2,gfx/fallback:1\n"
			   "to load face 2 of the font gfx/vera-sans and use face 1\n"
			   "of gfx/fallback as fallback font.\n"
			   "You can also specify a list of font sizes to load, like this:\n"
			   "loadfont console gfx/conchars,gfx/fallback 8 12 16 24 32\n"
			   "In many cases, 8 12 16 24 32 should be a good choice.\n"
			   "custom switches:\n"
			   " scale x : scale all characters by this amount when rendering (doesnt change line height)\n"
			   " voffset x : offset all chars vertical when rendering, this is multiplied to character height\n"
			);
		return;
	}
	f = FindFont(Cmd_Argv(1), true);
	if(f == NULL)
	{
		Con_Printf("font function not found\n");
		return;
	}

	if(Cmd_Argc() < 3)
		filelist = "gfx/conchars";
	else
		filelist = Cmd_Argv(2);

	memset(f->fallbacks, 0, sizeof(f->fallbacks));
	memset(f->fallback_faces, 0, sizeof(f->fallback_faces));

	// first font is handled "normally"
	c = strchr(filelist, ':');
	cm = strchr(filelist, ',');
	if(c && (!cm || c < cm))
		f->req_face = atoi(c+1);
	else
	{
		f->req_face = 0;
		c = cm;
	}

	if(!c || (c - filelist) > MAX_QPATH)
		strlcpy(mainfont, filelist, sizeof(mainfont));
	else
	{
		memcpy(mainfont, filelist, c - filelist);
		mainfont[c - filelist] = 0;
	}

	for(i = 0; i < MAX_FONT_FALLBACKS; ++i)
	{
		c = strchr(filelist, ',');
		if(!c)
			break;
		filelist = c + 1;
		if(!*filelist)
			break;
		c = strchr(filelist, ':');
		cm = strchr(filelist, ',');
		if(c && (!cm || c < cm))
			f->fallback_faces[i] = atoi(c+1);
		else
		{
			f->fallback_faces[i] = 0; // f->req_face; could make it stick to the default-font's face index
			c = cm;
		}
		if(!c || (c-filelist) > MAX_QPATH)
		{
			strlcpy(f->fallbacks[i], filelist, sizeof(mainfont));
		}
		else
		{
			memcpy(f->fallbacks[i], filelist, c - filelist);
			f->fallbacks[i][c - filelist] = 0;
		}
	}

	// for now: by default load only one size: the default size
	f->req_sizes[0] = 0;
	for(i = 1; i < MAX_FONT_SIZES; ++i)
		f->req_sizes[i] = -1;

	scale = 1;
	voffset = 0;
	if(Cmd_Argc() >= 4)
	{
		for(sizes = 0, i = 3; i < Cmd_Argc(); ++i)
		{
			// special switches
			if (!strcmp(Cmd_Argv(i), "scale"))
			{
				i++;
				if (i < Cmd_Argc())
					scale = atof(Cmd_Argv(i));
				continue;
			}
			if (!strcmp(Cmd_Argv(i), "voffset"))
			{
				i++;
				if (i < Cmd_Argc())
					voffset = atof(Cmd_Argv(i));
				continue;
			}

			if (sizes == -1)
				continue; // no slot for other sizes

			// parse one of sizes
			sz = atof(Cmd_Argv(i));
			if (sz > 0.001f && sz < 1000.0f) // do not use crap sizes
			{
				// search for duplicated sizes
				int j;
				for (j=0; j<sizes; j++)
					if (f->req_sizes[j] == sz)
						break;
				if (j != sizes)
					continue; // sz already in req_sizes, don't add it again

				if (sizes == MAX_FONT_SIZES)
				{
					Con_Printf("Warning: specified more than %i different font sizes, exceding ones are ignored\n", MAX_FONT_SIZES);
					sizes = -1;
					continue;
				}
				f->req_sizes[sizes] = sz;
				sizes++;
			}
		}
	}

	LoadFont(true, mainfont, f, scale, voffset);
}

/*
===============
Draw_Init
===============
*/
static void gl_draw_start(void)
{
	int i;
	drawtexturepool = R_AllocTexturePool();

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));

	font_start();

	// load default font textures
	for(i = 0; i < dp_fonts.maxsize; ++i)
		if (dp_fonts.f[i].title[0])
			LoadFont(false, va("gfx/font_%s", dp_fonts.f[i].title), &dp_fonts.f[i], 1, 0);

	// draw the loading screen so people have something to see in the newly opened window
	SCR_UpdateLoadingScreen(true);
}

static void gl_draw_shutdown(void)
{
	font_shutdown();

	R_FreeTexturePool(&drawtexturepool);

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));
}

static void gl_draw_newmap(void)
{
	font_newmap();
}

void GL_Draw_Init (void)
{
	int i, j;

	Cvar_RegisterVariable(&r_font_postprocess_blur);
	Cvar_RegisterVariable(&r_font_postprocess_outline);
	Cvar_RegisterVariable(&r_font_postprocess_shadow_x);
	Cvar_RegisterVariable(&r_font_postprocess_shadow_y);
	Cvar_RegisterVariable(&r_font_postprocess_shadow_z);
	Cvar_RegisterVariable(&r_font_hinting);
	Cvar_RegisterVariable(&r_font_antialias);
	Cvar_RegisterVariable(&r_textshadow);
	Cvar_RegisterVariable(&r_textbrightness);
	Cvar_RegisterVariable(&r_textcontrast);

	// allocate fonts storage
	fonts_mempool = Mem_AllocPool("FONTS", 0, NULL);
	dp_fonts.maxsize = MAX_FONTS;
	dp_fonts.f = (dp_font_t *)Mem_Alloc(fonts_mempool, sizeof(dp_font_t) * dp_fonts.maxsize);
	memset(dp_fonts.f, 0, sizeof(dp_font_t) * dp_fonts.maxsize);

	// assign starting font names
	strlcpy(FONT_DEFAULT->title, "default", sizeof(FONT_DEFAULT->title));
	strlcpy(FONT_DEFAULT->texpath, "gfx/conchars", sizeof(FONT_DEFAULT->texpath));
	strlcpy(FONT_CONSOLE->title, "console", sizeof(FONT_CONSOLE->title));
	strlcpy(FONT_SBAR->title, "sbar", sizeof(FONT_SBAR->title));
	strlcpy(FONT_NOTIFY->title, "notify", sizeof(FONT_NOTIFY->title));
	strlcpy(FONT_CHAT->title, "chat", sizeof(FONT_CHAT->title));
	strlcpy(FONT_CENTERPRINT->title, "centerprint", sizeof(FONT_CENTERPRINT->title));
	strlcpy(FONT_INFOBAR->title, "infobar", sizeof(FONT_INFOBAR->title));
	strlcpy(FONT_MENU->title, "menu", sizeof(FONT_MENU->title));
	for(i = 0, j = 0; i < MAX_USERFONTS; ++i)
		if(!FONT_USER(i)->title[0])
			dpsnprintf(FONT_USER(i)->title, sizeof(FONT_USER(i)->title), "user%d", j++);

	Cmd_AddCommand ("loadfont",LoadFont_f, "loadfont function tganame loads a font; example: loadfont console gfx/veramono; loadfont without arguments lists the available functions");
	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap, NULL, NULL);
}

static void _DrawQ_Setup(void)
{
	r_viewport_t viewport;
	if (r_refdef.draw2dstage == 1)
		return;
	r_refdef.draw2dstage = 1;
	CHECKGLERROR
	R_Viewport_InitOrtho(&viewport, &identitymatrix, r_refdef.view.x, vid.height - r_refdef.view.y - r_refdef.view.height, r_refdef.view.width, r_refdef.view.height, 0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100, NULL);
	R_Mesh_ResetRenderTargets();
	R_SetViewport(&viewport);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_DepthFunc(GL_LEQUAL);
	GL_PolygonOffset(0,0);
	GL_CullFace(GL_NONE);
	R_EntityMatrix(&identitymatrix);

	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(false);
	GL_Color(1,1,1,1);
}

qboolean r_draw2d_force = false;
void _DrawQ_SetupAndProcessDrawFlag(int flags, cachepic_t *pic, float alpha)
{
	_DrawQ_Setup();
	CHECKGLERROR
	if(!r_draw2d.integer && !r_draw2d_force)
		return;
	DrawQ_ProcessDrawFlag(flags, (alpha < 1) || (pic && pic->hasalpha));
}
void DrawQ_ProcessDrawFlag(int flags, qboolean alpha)
{
	if(flags == DRAWFLAG_ADDITIVE)
	{
		GL_DepthMask(false);
		GL_BlendFunc(alpha ? GL_SRC_ALPHA : GL_ONE, GL_ONE);
	}
	else if(flags == DRAWFLAG_MODULATE)
	{
		GL_DepthMask(false);
		GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
	}
	else if(flags == DRAWFLAG_2XMODULATE)
	{
		GL_DepthMask(false);
		GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	}
	else if(flags == DRAWFLAG_SCREEN)
	{
		GL_DepthMask(false);
		GL_BlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
	}
	else if(alpha)
	{
		GL_DepthMask(false);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
	}
}

void DrawQ_Pic(float x, float y, cachepic_t *pic, float width, float height, float red, float green, float blue, float alpha, int flags)
{
	float floats[36];

	_DrawQ_SetupAndProcessDrawFlag(flags, pic, alpha);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

//	R_Mesh_ResetTextureState();
	floats[12] = 0.0f;floats[13] = 0.0f;
	floats[14] = 1.0f;floats[15] = 0.0f;
	floats[16] = 1.0f;floats[17] = 1.0f;
	floats[18] = 0.0f;floats[19] = 1.0f;
	floats[20] = floats[24] = floats[28] = floats[32] = red;
	floats[21] = floats[25] = floats[29] = floats[33] = green;
	floats[22] = floats[26] = floats[30] = floats[34] = blue;
	floats[23] = floats[27] = floats[31] = floats[35] = alpha;
	if (pic)
	{
		if (width == 0)
			width = pic->width;
		if (height == 0)
			height = pic->height;
		R_SetupShader_Generic(Draw_GetPicTexture(pic), NULL, GL_MODULATE, 1, true);

#if 0
      // AK07: lets be texel correct on the corners
      {
         float horz_offset = 0.5f / pic->width;
         float vert_offset = 0.5f / pic->height;

		   floats[12] = 0.0f + horz_offset;floats[13] = 0.0f + vert_offset;
		   floats[14] = 1.0f - horz_offset;floats[15] = 0.0f + vert_offset;
		   floats[16] = 1.0f - horz_offset;floats[17] = 1.0f - vert_offset;
		   floats[18] = 0.0f + horz_offset;floats[19] = 1.0f - vert_offset;
      }
#endif
	}
	else
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

	floats[2] = floats[5] = floats[8] = floats[11] = 0;
	floats[0] = floats[9] = x;
	floats[1] = floats[4] = y;
	floats[3] = floats[6] = x + width;
	floats[7] = floats[10] = y + height;

	R_Mesh_PrepareVertices_Generic_Arrays(4, floats, floats + 20, floats + 12);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
}

void DrawQ_RotPic(float x, float y, cachepic_t *pic, float width, float height, float org_x, float org_y, float angle, float red, float green, float blue, float alpha, int flags)
{
	float floats[36];
	float af = DEG2RAD(-angle); // forward
	float ar = DEG2RAD(-angle + 90); // right
	float sinaf = sin(af);
	float cosaf = cos(af);
	float sinar = sin(ar);
	float cosar = cos(ar);

	_DrawQ_SetupAndProcessDrawFlag(flags, pic, alpha);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

//	R_Mesh_ResetTextureState();
	if (pic)
	{
		if (width == 0)
			width = pic->width;
		if (height == 0)
			height = pic->height;
		R_SetupShader_Generic(Draw_GetPicTexture(pic), NULL, GL_MODULATE, 1, true);
	}
	else
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

	floats[2] = floats[5] = floats[8] = floats[11] = 0;

// top left
	floats[0] = x - cosaf*org_x - cosar*org_y;
	floats[1] = y - sinaf*org_x - sinar*org_y;

// top right
	floats[3] = x + cosaf*(width-org_x) - cosar*org_y;
	floats[4] = y + sinaf*(width-org_x) - sinar*org_y;

// bottom right
	floats[6] = x + cosaf*(width-org_x) + cosar*(height-org_y);
	floats[7] = y + sinaf*(width-org_x) + sinar*(height-org_y);

// bottom left
	floats[9]  = x - cosaf*org_x + cosar*(height-org_y);
	floats[10] = y - sinaf*org_x + sinar*(height-org_y);

	floats[12] = 0.0f;floats[13] = 0.0f;
	floats[14] = 1.0f;floats[15] = 0.0f;
	floats[16] = 1.0f;floats[17] = 1.0f;
	floats[18] = 0.0f;floats[19] = 1.0f;
	floats[20] = floats[24] = floats[28] = floats[32] = red;
	floats[21] = floats[25] = floats[29] = floats[33] = green;
	floats[22] = floats[26] = floats[30] = floats[34] = blue;
	floats[23] = floats[27] = floats[31] = floats[35] = alpha;

	R_Mesh_PrepareVertices_Generic_Arrays(4, floats, floats + 20, floats + 12);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
}

void DrawQ_Fill(float x, float y, float width, float height, float red, float green, float blue, float alpha, int flags)
{
	float floats[36];

	_DrawQ_SetupAndProcessDrawFlag(flags, NULL, alpha);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

	floats[2] = floats[5] = floats[8] = floats[11] = 0;
	floats[0] = floats[9] = x;
	floats[1] = floats[4] = y;
	floats[3] = floats[6] = x + width;
	floats[7] = floats[10] = y + height;
	floats[12] = 0.0f;floats[13] = 0.0f;
	floats[14] = 1.0f;floats[15] = 0.0f;
	floats[16] = 1.0f;floats[17] = 1.0f;
	floats[18] = 0.0f;floats[19] = 1.0f;
	floats[20] = floats[24] = floats[28] = floats[32] = red;
	floats[21] = floats[25] = floats[29] = floats[33] = green;
	floats[22] = floats[26] = floats[30] = floats[34] = blue;
	floats[23] = floats[27] = floats[31] = floats[35] = alpha;

	R_Mesh_PrepareVertices_Generic_Arrays(4, floats, floats + 20, floats + 12);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
}

/// color tag printing
static const vec4_t string_colors[] =
{
	// Quake3 colors
	// LordHavoc: why on earth is cyan before magenta in Quake3?
	// LordHavoc: note: Doom3 uses white for [0] and [7]
	{0.0, 0.0, 0.0, 1.0}, // black
	{1.0, 0.0, 0.0, 1.0}, // red
	{0.0, 1.0, 0.0, 1.0}, // green
	{1.0, 1.0, 0.0, 1.0}, // yellow
	{0.0, 0.0, 1.0, 1.0}, // blue
	{0.0, 1.0, 1.0, 1.0}, // cyan
	{1.0, 0.0, 1.0, 1.0}, // magenta
	{1.0, 1.0, 1.0, 1.0}, // white
	// [515]'s BX_COLOREDTEXT extension
	{1.0, 1.0, 1.0, 0.5}, // half transparent
	{0.5, 0.5, 0.5, 1.0}  // half brightness
	// Black's color table
	//{1.0, 1.0, 1.0, 1.0},
	//{1.0, 0.0, 0.0, 1.0},
	//{0.0, 1.0, 0.0, 1.0},
	//{0.0, 0.0, 1.0, 1.0},
	//{1.0, 1.0, 0.0, 1.0},
	//{0.0, 1.0, 1.0, 1.0},
	//{1.0, 0.0, 1.0, 1.0},
	//{0.1, 0.1, 0.1, 1.0}
};

#define STRING_COLORS_COUNT	(sizeof(string_colors) / sizeof(vec4_t))

static void DrawQ_GetTextColor(float color[4], int colorindex, float r, float g, float b, float a, qboolean shadow)
{
	float C = r_textcontrast.value;
	float B = r_textbrightness.value;
	if (colorindex & 0x10000) // that bit means RGB color
	{
		color[0] = ((colorindex >> 12) & 0xf) / 15.0;
		color[1] = ((colorindex >> 8) & 0xf) / 15.0;
		color[2] = ((colorindex >> 4) & 0xf) / 15.0;
		color[3] = (colorindex & 0xf) / 15.0;
	}
	else
		Vector4Copy(string_colors[colorindex], color);
	Vector4Set(color, color[0] * r * C + B, color[1] * g * C + B, color[2] * b * C + B, color[3] * a);
	if (shadow)
	{
		float shadowalpha = (color[0]+color[1]+color[2]) * 0.8;
		Vector4Set(color, 0, 0, 0, color[3] * bound(0, shadowalpha, 1));
	}
}

// NOTE: this function always draws exactly one character if maxwidth <= 0
float DrawQ_TextWidth_UntilWidth_TrackColors_Scale(const char *text, size_t *maxlen, float w, float h, float sw, float sh, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth)
{
	const char *text_start = text;
	int colorindex = STRING_COLOR_DEFAULT;
	size_t i;
	float x = 0;
	Uchar ch, mapch, nextch;
	Uchar prevch = 0; // used for kerning
	int tempcolorindex;
	float kx;
	int map_index = 0;
	size_t bytes_left;
	ft2_font_map_t *fontmap = NULL;
	ft2_font_map_t *map = NULL;
	//ft2_font_map_t *prevmap = NULL;
	ft2_font_t *ft2 = fnt->ft2;
	// float ftbase_x;
	qboolean snap = true;
	qboolean least_one = false;
	float dw; // display w
	//float dh; // display h
	const float *width_of;

	if (!h) h = w;
	if (!h) {
		w = h = 1;
		snap = false;
	}
	// do this in the end
	w *= fnt->settings.scale;
	h *= fnt->settings.scale;

	// find the most fitting size:
	if (ft2 != NULL)
	{
		if (snap)
			map_index = Font_IndexForSize(ft2, h, &w, &h);
		else
			map_index = Font_IndexForSize(ft2, h, NULL, NULL);
		fontmap = Font_MapForIndex(ft2, map_index);
	}

	dw = w * sw;
	//dh = h * sh;

	if (*maxlen < 1)
		*maxlen = 1<<30;

	if (!outcolor || *outcolor == -1)
		colorindex = STRING_COLOR_DEFAULT;
	else
		colorindex = *outcolor;

	// maxwidth /= fnt->scale; // w and h are multiplied by it already
	// ftbase_x = snap_to_pixel_x(0);
	
	if(maxwidth <= 0)
	{
		least_one = true;
		maxwidth = -maxwidth;
	}

	//if (snap)
	//	x = snap_to_pixel_x(x, 0.4); // haha, it's 0 anyway

	if (fontmap)
		width_of = fontmap->width_of;
	else
		width_of = fnt->width_of;

	for (i = 0;((bytes_left = *maxlen - (text - text_start)) > 0) && *text;)
	{
		size_t i0 = i;
		nextch = ch = u8_getnchar(text, &text, bytes_left);
		i = text - text_start;
		if (!ch)
			break;
		if (ch == ' ' && !fontmap)
		{
			if(!least_one || i0) // never skip the first character
			if(x + width_of[(int) ' '] * dw > maxwidth)
			{
				i = i0;
				break; // oops, can't draw this
			}
			x += width_of[(int) ' '] * dw;
			continue;
		}
		// i points to the char after ^
		if (ch == STRING_COLOR_TAG && !ignorecolorcodes && i < *maxlen)
		{
			ch = *text; // colors are ascii, so no u8_ needed
			if (ch <= '9' && ch >= '0') // ^[0-9] found
			{
				colorindex = ch - '0';
				++text;
				++i;
				continue;
			}
			// i points to the char after ^...
			// i+3 points to 3 in ^x123
			// i+3 == *maxlen would mean that char is missing
			else if (ch == STRING_COLOR_RGB_TAG_CHAR && i + 3 < *maxlen ) // ^x found
			{
				// building colorindex...
				ch = tolower(text[1]);
				tempcolorindex = 0x10000; // binary: 1,0000,0000,0000,0000
				if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 12;
				else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 12;
				else tempcolorindex = 0;
				if (tempcolorindex)
				{
					ch = tolower(text[2]);
					if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 8;
					else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 8;
					else tempcolorindex = 0;
					if (tempcolorindex)
					{
						ch = tolower(text[3]);
						if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 4;
						else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 4;
						else tempcolorindex = 0;
						if (tempcolorindex)
						{
							colorindex = tempcolorindex | 0xf;
							// ...done! now colorindex has rgba codes (1,rrrr,gggg,bbbb,aaaa)
							i+=4;
							text += 4;
							continue;
						}
					}
				}
			}
			else if (ch == STRING_COLOR_TAG) // ^^ found, ignore the first ^ and go to print the second
			{
				i++;
				text++;
			}
			i--;
		}
		ch = nextch;

		if (!fontmap || (ch <= 0xFF && fontmap->glyphs[ch].image) || (ch >= 0xE000 && ch <= 0xE0FF))
		{
			if (ch > 0xE000)
				ch -= 0xE000;
			if (ch > 0xFF)
				continue;
			if (fontmap)
				map = ft2_oldstyle_map;
			prevch = 0;
			if(!least_one || i0) // never skip the first character
			if(x + width_of[ch] * dw > maxwidth)
			{
				i = i0;
				break; // oops, can't draw this
			}
			x += width_of[ch] * dw;
		} else {
			if (!map || map == ft2_oldstyle_map || ch < map->start || ch >= map->start + FONT_CHARS_PER_MAP)
			{
				map = FontMap_FindForChar(fontmap, ch);
				if (!map)
				{
					if (!Font_LoadMapForIndex(ft2, map_index, ch, &map))
						break;
					if (!map)
						break;
				}
			}
			mapch = ch - map->start;
			if (prevch && Font_GetKerningForMap(ft2, map_index, w, h, prevch, ch, &kx, NULL))
				x += kx * dw;
			x += map->glyphs[mapch].advance_x * dw;
			//prevmap = map;
			prevch = ch;
		}
	}

	*maxlen = i;

	if (outcolor)
		*outcolor = colorindex;

	return x;
}

float DrawQ_Color[4];
float DrawQ_String_Scale(float startx, float starty, const char *text, size_t maxlen, float w, float h, float sw, float sh, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
	int shadow, colorindex = STRING_COLOR_DEFAULT;
	size_t i;
	float x = startx, y, s, t, u, v, thisw;
	float *av, *at, *ac;
	int batchcount;
	static float vertex3f[QUADELEMENTS_MAXQUADS*4*3];
	static float texcoord2f[QUADELEMENTS_MAXQUADS*4*2];
	static float color4f[QUADELEMENTS_MAXQUADS*4*4];
	Uchar ch, mapch, nextch;
	Uchar prevch = 0; // used for kerning
	int tempcolorindex;
	int map_index = 0;
	//ft2_font_map_t *prevmap = NULL; // the previous map
	ft2_font_map_t *map = NULL;     // the currently used map
	ft2_font_map_t *fontmap = NULL; // the font map for the size
	float ftbase_y;
	const char *text_start = text;
	float kx, ky;
	ft2_font_t *ft2 = fnt->ft2;
	qboolean snap = true;
	float pix_x, pix_y;
	size_t bytes_left;
	float dw, dh;
	const float *width_of;

	int tw, th;
	tw = R_TextureWidth(fnt->tex);
	th = R_TextureHeight(fnt->tex);

	if (!h) h = w;
	if (!h) {
		h = w = 1;
		snap = false;
	}

	starty -= (fnt->settings.scale - 1) * h * 0.5 - fnt->settings.voffset*h; // center & offset
	w *= fnt->settings.scale;
	h *= fnt->settings.scale;

	if (ft2 != NULL)
	{
		if (snap)
			map_index = Font_IndexForSize(ft2, h, &w, &h);
		else
			map_index = Font_IndexForSize(ft2, h, NULL, NULL);
		fontmap = Font_MapForIndex(ft2, map_index);
	}

	dw = w * sw;
	dh = h * sh;

	// draw the font at its baseline when using freetype
	//ftbase_x = 0;
	ftbase_y = dh * (4.5/6.0);

	if (maxlen < 1)
		maxlen = 1<<30;

	_DrawQ_SetupAndProcessDrawFlag(flags, NULL, 0);
	if(!r_draw2d.integer && !r_draw2d_force)
		return startx + DrawQ_TextWidth_UntilWidth_TrackColors_Scale(text, &maxlen, w, h, sw, sh, NULL, ignorecolorcodes, fnt, 1000000000);

//	R_Mesh_ResetTextureState();
	if (!fontmap)
		R_Mesh_TexBind(0, fnt->tex);
	R_SetupShader_Generic(fnt->tex, NULL, GL_MODULATE, 1, true);

	ac = color4f;
	at = texcoord2f;
	av = vertex3f;
	batchcount = 0;

	//ftbase_x = snap_to_pixel_x(ftbase_x);
	if(snap)
	{
		startx = snap_to_pixel_x(startx, 0.4);
		starty = snap_to_pixel_y(starty, 0.4);
		ftbase_y = snap_to_pixel_y(ftbase_y, 0.3);
	}

	pix_x = vid.width / vid_conwidth.value;
	pix_y = vid.height / vid_conheight.value;

	if (fontmap)
		width_of = fontmap->width_of;
	else
		width_of = fnt->width_of;

	for (shadow = r_textshadow.value != 0 && basealpha > 0;shadow >= 0;shadow--)
	{
		prevch = 0;
		text = text_start;

		if (!outcolor || *outcolor == -1)
			colorindex = STRING_COLOR_DEFAULT;
		else
			colorindex = *outcolor;

		DrawQ_GetTextColor(DrawQ_Color, colorindex, basered, basegreen, baseblue, basealpha, shadow != 0);

		x = startx;
		y = starty;
		/*
		if (shadow)
		{
			x += r_textshadow.value * vid.width / vid_conwidth.value;
			y += r_textshadow.value * vid.height / vid_conheight.value;
		}
		*/
		for (i = 0;((bytes_left = maxlen - (text - text_start)) > 0) && *text;)
		{
			nextch = ch = u8_getnchar(text, &text, bytes_left);
			i = text - text_start;
			if (!ch)
				break;
			if (ch == ' ' && !fontmap)
			{
				x += width_of[(int) ' '] * dw;
				continue;
			}
			if (ch == STRING_COLOR_TAG && !ignorecolorcodes && i < maxlen)
			{
				ch = *text; // colors are ascii, so no u8_ needed
				if (ch <= '9' && ch >= '0') // ^[0-9] found
				{
					colorindex = ch - '0';
					DrawQ_GetTextColor(DrawQ_Color, colorindex, basered, basegreen, baseblue, basealpha, shadow != 0);
					++text;
					++i;
					continue;
				}
				else if (ch == STRING_COLOR_RGB_TAG_CHAR && i+3 < maxlen ) // ^x found
				{
					// building colorindex...
					ch = tolower(text[1]);
					tempcolorindex = 0x10000; // binary: 1,0000,0000,0000,0000
					if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 12;
					else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 12;
					else tempcolorindex = 0;
					if (tempcolorindex)
					{
						ch = tolower(text[2]);
						if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 8;
						else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 8;
						else tempcolorindex = 0;
						if (tempcolorindex)
						{
							ch = tolower(text[3]);
							if (ch <= '9' && ch >= '0') tempcolorindex |= (ch - '0') << 4;
							else if (ch >= 'a' && ch <= 'f') tempcolorindex |= (ch - 87) << 4;
							else tempcolorindex = 0;
							if (tempcolorindex)
							{
								colorindex = tempcolorindex | 0xf;
								// ...done! now colorindex has rgba codes (1,rrrr,gggg,bbbb,aaaa)
								//Con_Printf("^1colorindex:^7 %x\n", colorindex);
								DrawQ_GetTextColor(DrawQ_Color, colorindex, basered, basegreen, baseblue, basealpha, shadow != 0);
								i+=4;
								text+=4;
								continue;
							}
						}
					}
				}
				else if (ch == STRING_COLOR_TAG)
				{
					i++;
					text++;
				}
				i--;
			}
			// get the backup
			ch = nextch;
			// using a value of -1 for the oldstyle map because NULL means uninitialized...
			// this way we don't need to rebind fnt->tex for every old-style character
			// E000..E0FF: emulate old-font characters (to still have smileys and such available)
			if (shadow)
			{
				x += 1.0/pix_x * r_textshadow.value;
				y += 1.0/pix_y * r_textshadow.value;
			}
			if (!fontmap || (ch <= 0xFF && fontmap->glyphs[ch].image) || (ch >= 0xE000 && ch <= 0xE0FF))
			{
				if (ch >= 0xE000)
					ch -= 0xE000;
				if (ch > 0xFF)
					goto out;
				if (fontmap)
				{
					if (map != ft2_oldstyle_map)
					{
						if (batchcount)
						{
							// switching from freetype to non-freetype rendering
							R_Mesh_PrepareVertices_Generic_Arrays(batchcount * 4, vertex3f, color4f, texcoord2f);
							R_Mesh_Draw(0, batchcount * 4, 0, batchcount * 2, quadelement3i, NULL, 0, quadelement3s, NULL, 0);
							batchcount = 0;
							ac = color4f;
							at = texcoord2f;
							av = vertex3f;
						}
						R_SetupShader_Generic(fnt->tex, NULL, GL_MODULATE, 1, true);
						map = ft2_oldstyle_map;
					}
				}
				prevch = 0;
				//num = (unsigned char) text[i];
				//thisw = fnt->width_of[num];
				thisw = fnt->width_of[ch];
				// FIXME make these smaller to just include the occupied part of the character for slightly faster rendering
				s = (ch & 15)*0.0625f + (0.5f / tw);
				t = (ch >> 4)*0.0625f + (0.5f / th);
				u = 0.0625f * thisw - (1.0f / tw);
				v = 0.0625f - (1.0f / th);
				ac[ 0] = DrawQ_Color[0];ac[ 1] = DrawQ_Color[1];ac[ 2] = DrawQ_Color[2];ac[ 3] = DrawQ_Color[3];
				ac[ 4] = DrawQ_Color[0];ac[ 5] = DrawQ_Color[1];ac[ 6] = DrawQ_Color[2];ac[ 7] = DrawQ_Color[3];
				ac[ 8] = DrawQ_Color[0];ac[ 9] = DrawQ_Color[1];ac[10] = DrawQ_Color[2];ac[11] = DrawQ_Color[3];
				ac[12] = DrawQ_Color[0];ac[13] = DrawQ_Color[1];ac[14] = DrawQ_Color[2];ac[15] = DrawQ_Color[3];
				at[ 0] = s		; at[ 1] = t	;
				at[ 2] = s+u	; at[ 3] = t	;
				at[ 4] = s+u	; at[ 5] = t+v	;
				at[ 6] = s		; at[ 7] = t+v	;
				av[ 0] = x			; av[ 1] = y	; av[ 2] = 10;
				av[ 3] = x+dw*thisw	; av[ 4] = y	; av[ 5] = 10;
				av[ 6] = x+dw*thisw	; av[ 7] = y+dh	; av[ 8] = 10;
				av[ 9] = x			; av[10] = y+dh	; av[11] = 10;
				ac += 16;
				at += 8;
				av += 12;
				batchcount++;
				if (batchcount >= QUADELEMENTS_MAXQUADS)
				{
					R_Mesh_PrepareVertices_Generic_Arrays(batchcount * 4, vertex3f, color4f, texcoord2f);
					R_Mesh_Draw(0, batchcount * 4, 0, batchcount * 2, quadelement3i, NULL, 0, quadelement3s, NULL, 0);
					batchcount = 0;
					ac = color4f;
					at = texcoord2f;
					av = vertex3f;
				}
				x += width_of[ch] * dw;
			} else {
				if (!map || map == ft2_oldstyle_map || ch < map->start || ch >= map->start + FONT_CHARS_PER_MAP)
				{
					// new charmap - need to render
					if (batchcount)
					{
						// we need a different character map, render what we currently have:
						R_Mesh_PrepareVertices_Generic_Arrays(batchcount * 4, vertex3f, color4f, texcoord2f);
						R_Mesh_Draw(0, batchcount * 4, 0, batchcount * 2, quadelement3i, NULL, 0, quadelement3s, NULL, 0);
						batchcount = 0;
						ac = color4f;
						at = texcoord2f;
						av = vertex3f;
					}
					// find the new map
					map = FontMap_FindForChar(fontmap, ch);
					if (!map)
					{
						if (!Font_LoadMapForIndex(ft2, map_index, ch, &map))
						{
							shadow = -1;
							break;
						}
						if (!map)
						{
							// this shouldn't happen
							shadow = -1;
							break;
						}
					}
					R_SetupShader_Generic(map->pic->tex, NULL, GL_MODULATE, 1, true);
				}

				mapch = ch - map->start;
				thisw = map->glyphs[mapch].advance_x;

				//x += ftbase_x;
				y += ftbase_y;
				if (prevch && Font_GetKerningForMap(ft2, map_index, w, h, prevch, ch, &kx, &ky))
				{
					x += kx * dw;
					y += ky * dh;
				}
				else
					kx = ky = 0;
				ac[ 0] = DrawQ_Color[0]; ac[ 1] = DrawQ_Color[1]; ac[ 2] = DrawQ_Color[2]; ac[ 3] = DrawQ_Color[3];
				ac[ 4] = DrawQ_Color[0]; ac[ 5] = DrawQ_Color[1]; ac[ 6] = DrawQ_Color[2]; ac[ 7] = DrawQ_Color[3];
				ac[ 8] = DrawQ_Color[0]; ac[ 9] = DrawQ_Color[1]; ac[10] = DrawQ_Color[2]; ac[11] = DrawQ_Color[3];
				ac[12] = DrawQ_Color[0]; ac[13] = DrawQ_Color[1]; ac[14] = DrawQ_Color[2]; ac[15] = DrawQ_Color[3];
				at[0] = map->glyphs[mapch].txmin; at[1] = map->glyphs[mapch].tymin;
				at[2] = map->glyphs[mapch].txmax; at[3] = map->glyphs[mapch].tymin;
				at[4] = map->glyphs[mapch].txmax; at[5] = map->glyphs[mapch].tymax;
				at[6] = map->glyphs[mapch].txmin; at[7] = map->glyphs[mapch].tymax;
				av[ 0] = x + dw * map->glyphs[mapch].vxmin; av[ 1] = y + dh * map->glyphs[mapch].vymin; av[ 2] = 10;
				av[ 3] = x + dw * map->glyphs[mapch].vxmax; av[ 4] = y + dh * map->glyphs[mapch].vymin; av[ 5] = 10;
				av[ 6] = x + dw * map->glyphs[mapch].vxmax; av[ 7] = y + dh * map->glyphs[mapch].vymax; av[ 8] = 10;
				av[ 9] = x + dw * map->glyphs[mapch].vxmin; av[10] = y + dh * map->glyphs[mapch].vymax; av[11] = 10;
				//x -= ftbase_x;
				y -= ftbase_y;

				x += thisw * dw;
				ac += 16;
				at += 8;
				av += 12;
				batchcount++;
				if (batchcount >= QUADELEMENTS_MAXQUADS)
				{
					R_Mesh_PrepareVertices_Generic_Arrays(batchcount * 4, vertex3f, color4f, texcoord2f);
					R_Mesh_Draw(0, batchcount * 4, 0, batchcount * 2, quadelement3i, NULL, 0, quadelement3s, NULL, 0);
					batchcount = 0;
					ac = color4f;
					at = texcoord2f;
					av = vertex3f;
				}

				//prevmap = map;
				prevch = ch;
			}
out:
			if (shadow)
			{
				x -= 1.0/pix_x * r_textshadow.value;
				y -= 1.0/pix_y * r_textshadow.value;
			}
		}
	}
	if (batchcount > 0)
	{
		R_Mesh_PrepareVertices_Generic_Arrays(batchcount * 4, vertex3f, color4f, texcoord2f);
		R_Mesh_Draw(0, batchcount * 4, 0, batchcount * 2, quadelement3i, NULL, 0, quadelement3s, NULL, 0);
	}

	if (outcolor)
		*outcolor = colorindex;
	
	// note: this relies on the proper text (not shadow) being drawn last
	return x;
}

float DrawQ_String(float startx, float starty, const char *text, size_t maxlen, float w, float h, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
	return DrawQ_String_Scale(startx, starty, text, maxlen, w, h, 1, 1, basered, basegreen, baseblue, basealpha, flags, outcolor, ignorecolorcodes, fnt);
}

float DrawQ_TextWidth_UntilWidth_TrackColors(const char *text, size_t *maxlen, float w, float h, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth)
{
	return DrawQ_TextWidth_UntilWidth_TrackColors_Scale(text, maxlen, w, h, 1, 1, outcolor, ignorecolorcodes, fnt, maxwidth);
}

float DrawQ_TextWidth(const char *text, size_t maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt)
{
	return DrawQ_TextWidth_UntilWidth(text, &maxlen, w, h, ignorecolorcodes, fnt, 1000000000);
}

float DrawQ_TextWidth_UntilWidth(const char *text, size_t *maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxWidth)
{
	return DrawQ_TextWidth_UntilWidth_TrackColors(text, maxlen, w, h, NULL, ignorecolorcodes, fnt, maxWidth);
}

#if 0
// not used
// no ^xrgb management
static int DrawQ_BuildColoredText(char *output2c, size_t maxoutchars, const char *text, int maxreadchars, qboolean ignorecolorcodes, int *outcolor)
{
	int color, numchars = 0;
	char *outputend2c = output2c + maxoutchars - 2;
	if (!outcolor || *outcolor == -1)
		color = STRING_COLOR_DEFAULT;
	else
		color = *outcolor;
	if (!maxreadchars)
		maxreadchars = 1<<30;
	textend = text + maxreadchars;
	while (text != textend && *text)
	{
		if (*text == STRING_COLOR_TAG && !ignorecolorcodes && text + 1 != textend)
		{
			if (text[1] == STRING_COLOR_TAG)
				text++;
			else if (text[1] >= '0' && text[1] <= '9')
			{
				color = text[1] - '0';
				text += 2;
				continue;
			}
		}
		if (output2c >= outputend2c)
			break;
		*output2c++ = *text++;
		*output2c++ = color;
		numchars++;
	}
	output2c[0] = output2c[1] = 0;
	if (outcolor)
		*outcolor = color;
	return numchars;
}
#endif

void DrawQ_SuperPic(float x, float y, cachepic_t *pic, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags)
{
	float floats[36];

	_DrawQ_SetupAndProcessDrawFlag(flags, pic, a1*a2*a3*a4);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

//	R_Mesh_ResetTextureState();
	if (pic)
	{
		if (width == 0)
			width = pic->width;
		if (height == 0)
			height = pic->height;
		R_SetupShader_Generic(Draw_GetPicTexture(pic), NULL, GL_MODULATE, 1, true);
	}
	else
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

	floats[2] = floats[5] = floats[8] = floats[11] = 0;
	floats[0] = floats[9] = x;
	floats[1] = floats[4] = y;
	floats[3] = floats[6] = x + width;
	floats[7] = floats[10] = y + height;
	floats[12] = s1;floats[13] = t1;
	floats[14] = s2;floats[15] = t2;
	floats[16] = s4;floats[17] = t4;
	floats[18] = s3;floats[19] = t3;
	floats[20] = r1;floats[21] = g1;floats[22] = b1;floats[23] = a1;
	floats[24] = r2;floats[25] = g2;floats[26] = b2;floats[27] = a2;
	floats[28] = r4;floats[29] = g4;floats[30] = b4;floats[31] = a4;
	floats[32] = r3;floats[33] = g3;floats[34] = b3;floats[35] = a3;

	R_Mesh_PrepareVertices_Generic_Arrays(4, floats, floats + 20, floats + 12);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
}

void DrawQ_Mesh (drawqueuemesh_t *mesh, int flags, qboolean hasalpha)
{
	_DrawQ_Setup();
	CHECKGLERROR
	if(!r_draw2d.integer && !r_draw2d_force)
		return;
	DrawQ_ProcessDrawFlag(flags, hasalpha);

//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(mesh->texture, NULL, GL_MODULATE, 1, true);

	R_Mesh_PrepareVertices_Generic_Arrays(mesh->num_vertices, mesh->data_vertex3f, mesh->data_color4f, mesh->data_texcoord2f);
	R_Mesh_Draw(0, mesh->num_vertices, 0, mesh->num_triangles, mesh->data_element3i, NULL, 0, mesh->data_element3s, NULL, 0);
}

void DrawQ_LineLoop (drawqueuemesh_t *mesh, int flags)
{
	int num;

	_DrawQ_SetupAndProcessDrawFlag(flags, NULL, 1);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

	GL_Color(1,1,1,1);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
		CHECKGLERROR
		qglBegin(GL_LINE_LOOP);
		for (num = 0;num < mesh->num_vertices;num++)
		{
			if (mesh->data_color4f)
				GL_Color(mesh->data_color4f[num*4+0], mesh->data_color4f[num*4+1], mesh->data_color4f[num*4+2], mesh->data_color4f[num*4+3]);
			qglVertex2f(mesh->data_vertex3f[num*3+0], mesh->data_vertex3f[num*3+1]);
		}
		qglEnd();
		CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
		//Con_DPrintf("FIXME D3D9 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		//Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		//Con_DPrintf("FIXME GLES2 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
}

//[515]: this is old, delete
void DrawQ_Line (float width, float x1, float y1, float x2, float y2, float r, float g, float b, float alpha, int flags)
{
	_DrawQ_SetupAndProcessDrawFlag(flags, NULL, alpha);
	if(!r_draw2d.integer && !r_draw2d_force)
		return;

	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
		CHECKGLERROR

		//qglLineWidth(width);CHECKGLERROR

		GL_Color(r,g,b,alpha);
		CHECKGLERROR
		qglBegin(GL_LINES);
		qglVertex2f(x1, y1);
		qglVertex2f(x2, y2);
		qglEnd();
		CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
		//Con_DPrintf("FIXME D3D9 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		//Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		//Con_DPrintf("FIXME GLES2 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
}

void DrawQ_Lines (float width, int numlines, const float *vertex3f, const float *color4f, int flags)
{
	int i;
	qboolean hasalpha = false;
	for (i = 0;i < numlines*2;i++)
		if (color4f[i*4+3] < 1.0f)
			hasalpha = true;

	_DrawQ_SetupAndProcessDrawFlag(flags, NULL, hasalpha ? 0.5f : 1.0f);

	if(!r_draw2d.integer && !r_draw2d_force)
		return;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
		CHECKGLERROR

		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);

		//qglLineWidth(width);CHECKGLERROR

		CHECKGLERROR
		R_Mesh_PrepareVertices_Generic_Arrays(numlines*2, vertex3f, color4f, NULL);
		qglDrawArrays(GL_LINES, 0, numlines*2);
		CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
		//Con_DPrintf("FIXME D3D9 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		//Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		//Con_DPrintf("FIXME GLES2 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
}

void DrawQ_SetClipArea(float x, float y, float width, float height)
{
	int ix, iy, iw, ih;
	_DrawQ_Setup();

	// We have to convert the con coords into real coords
	// OGL uses top to bottom
	ix = (int)(0.5 + x * ((float)vid.width / vid_conwidth.integer));
	iy = (int)(0.5 + y * ((float) vid.height / vid_conheight.integer));
	iw = (int)(0.5 + (x+width) * ((float)vid.width / vid_conwidth.integer)) - ix;
	ih = (int)(0.5 + (y+height) * ((float) vid.height / vid_conheight.integer)) - iy;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
	case RENDERPATH_SOFT:
		GL_Scissor(ix, vid.height - iy - ih, iw, ih);
		break;
	case RENDERPATH_D3D9:
		GL_Scissor(ix, iy, iw, ih);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}

	GL_ScissorTest(true);
}

void DrawQ_ResetClipArea(void)
{
	_DrawQ_Setup();
	GL_ScissorTest(false);
}

void DrawQ_Finish(void)
{
	r_refdef.draw2dstage = 0;
}

void DrawQ_RecalcView(void)
{
	if(r_refdef.draw2dstage)
		r_refdef.draw2dstage = -1; // next draw call will set viewport etc. again
}

static float blendvertex3f[9] = {-5000, -5000, 10, 10000, -5000, 10, -5000, 10000, 10};
void R_DrawGamma(void)
{
	float c[4];
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_GLES2:
		if (vid_usinghwgamma || v_glslgamma.integer)
			return;
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
		if (vid_usinghwgamma)
			return;
		break;
	case RENDERPATH_GLES1:
	case RENDERPATH_SOFT:
		return;
	}
	// all the blends ignore depth
//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);
	GL_DepthMask(true);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(false);

	// interpretation of brightness and contrast:
	//   color range := brightness .. (brightness + contrast)
	// i.e. "c *= contrast; c += brightness"
	// plausible values for brightness thus range from -contrast to 1

	// apply pre-brightness (subtractive brightness, for where contrast was >= 1)
	if (vid.support.ext_blend_subtract)
	{
		if (v_color_enable.integer)
		{
			c[0] = -v_color_black_r.value / v_color_white_r.value;
			c[1] = -v_color_black_g.value / v_color_white_g.value;
			c[2] = -v_color_black_b.value / v_color_white_b.value;
		}
		else
			c[0] = c[1] = c[2] = -v_brightness.value / v_contrast.value;
		if (c[0] >= 0.01f || c[1] >= 0.01f || c[2] >= 0.01f)
		{
			// need SUBTRACTIVE blending to do this!
			GL_BlendEquationSubtract(true);
			GL_BlendFunc(GL_ONE, GL_ONE);
			GL_Color(c[0], c[1], c[2], 1);
			R_Mesh_PrepareVertices_Generic_Arrays(3, blendvertex3f, NULL, NULL);
			R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
			GL_BlendEquationSubtract(false);
		}
	}

	// apply contrast
	if (v_color_enable.integer)
	{
		c[0] = v_color_white_r.value;
		c[1] = v_color_white_g.value;
		c[2] = v_color_white_b.value;
	}
	else
		c[0] = c[1] = c[2] = v_contrast.value;
	if (c[0] >= 1.003f || c[1] >= 1.003f || c[2] >= 1.003f)
	{
		GL_BlendFunc(GL_DST_COLOR, GL_ONE);
		while (c[0] >= 1.003f || c[1] >= 1.003f || c[2] >= 1.003f)
		{
			float cc[4];
			cc[0] = bound(0, c[0] - 1, 1);
			cc[1] = bound(0, c[1] - 1, 1);
			cc[2] = bound(0, c[2] - 1, 1);
			GL_Color(cc[0], cc[1], cc[2], 1);
			R_Mesh_PrepareVertices_Generic_Arrays(3, blendvertex3f, NULL, NULL);
			R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
			c[0] /= 1 + cc[0];
			c[1] /= 1 + cc[1];
			c[2] /= 1 + cc[2];
		}
	}
	if (c[0] <= 0.997f || c[1] <= 0.997f || c[2] <= 0.997f)
	{
		GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		GL_Color(c[0], c[1], c[2], 1);
		R_Mesh_PrepareVertices_Generic_Arrays(3, blendvertex3f, NULL, NULL);
		R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
	}

	// apply post-brightness (additive brightness, for where contrast was <= 1)
	if (v_color_enable.integer)
	{
		c[0] = v_color_black_r.value;
		c[1] = v_color_black_g.value;
		c[2] = v_color_black_b.value;
	}
	else
		c[0] = c[1] = c[2] = v_brightness.value;
	if (c[0] >= 0.01f || c[1] >= 0.01f || c[2] >= 0.01f)
	{
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_Color(c[0], c[1], c[2], 1);
		R_Mesh_PrepareVertices_Generic_Arrays(3, blendvertex3f, NULL, NULL);
		R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
	}
}

