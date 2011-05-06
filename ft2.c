/* FreeType 2 and UTF-8 encoding support for
 * DarkPlaces
 */
#include "quakedef.h"

#include "ft2.h"
#include "ft2_defs.h"
#include "ft2_fontdefs.h"
#include "image.h"

static int img_fontmap[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // shift+digit line
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // digits
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // caps
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // caps
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // small
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // small
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // specials
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // faces
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
================================================================================
CVars introduced with the freetype extension
================================================================================
*/

cvar_t r_font_disable_freetype = {CVAR_SAVE, "r_font_disable_freetype", "1", "disable freetype support for fonts entirely"};
cvar_t r_font_use_alpha_textures = {CVAR_SAVE, "r_font_use_alpha_textures", "0", "use alpha-textures for font rendering, this should safe memory"};
cvar_t r_font_size_snapping = {CVAR_SAVE, "r_font_size_snapping", "1", "stick to good looking font sizes whenever possible - bad when the mod doesn't support it!"};
cvar_t r_font_kerning = {CVAR_SAVE, "r_font_kerning", "1", "Use kerning if available"};
cvar_t r_font_diskcache = {CVAR_SAVE, "r_font_diskcache", "0", "save font textures to disk for future loading rather than generating them every time"};
cvar_t r_font_compress = {CVAR_SAVE, "r_font_compress", "0", "use texture compression on font textures to save video memory"};
cvar_t r_font_nonpoweroftwo = {CVAR_SAVE, "r_font_nonpoweroftwo", "1", "use nonpoweroftwo textures for font (saves memory, potentially slower)"};
cvar_t developer_font = {CVAR_SAVE, "developer_font", "0", "prints debug messages about fonts"};

/*
================================================================================
Function definitions. Taken from the freetype2 headers.
================================================================================
*/


FT_EXPORT( FT_Error )
(*qFT_Init_FreeType)( FT_Library  *alibrary );
FT_EXPORT( FT_Error )
(*qFT_Done_FreeType)( FT_Library  library );
/*
FT_EXPORT( FT_Error )
(*qFT_New_Face)( FT_Library   library,
		 const char*  filepathname,
		 FT_Long      face_index,
		 FT_Face     *aface );
*/
FT_EXPORT( FT_Error )
(*qFT_New_Memory_Face)( FT_Library      library,
			const FT_Byte*  file_base,
			FT_Long         file_size,
			FT_Long         face_index,
			FT_Face        *aface );
FT_EXPORT( FT_Error )
(*qFT_Done_Face)( FT_Face  face );
FT_EXPORT( FT_Error )
(*qFT_Select_Size)( FT_Face  face,
		    FT_Int   strike_index );
FT_EXPORT( FT_Error )
(*qFT_Request_Size)( FT_Face          face,
		     FT_Size_Request  req );
FT_EXPORT( FT_Error )
(*qFT_Set_Char_Size)( FT_Face     face,
		      FT_F26Dot6  char_width,
		      FT_F26Dot6  char_height,
		      FT_UInt     horz_resolution,
		      FT_UInt     vert_resolution );
FT_EXPORT( FT_Error )
(*qFT_Set_Pixel_Sizes)( FT_Face  face,
			FT_UInt  pixel_width,
			FT_UInt  pixel_height );
FT_EXPORT( FT_Error )
(*qFT_Load_Glyph)( FT_Face   face,
		   FT_UInt   glyph_index,
		   FT_Int32  load_flags );
FT_EXPORT( FT_Error )
(*qFT_Load_Char)( FT_Face   face,
		  FT_ULong  char_code,
		  FT_Int32  load_flags );
FT_EXPORT( FT_UInt )
(*qFT_Get_Char_Index)( FT_Face   face,
		       FT_ULong  charcode );
FT_EXPORT( FT_Error )
(*qFT_Render_Glyph)( FT_GlyphSlot    slot,
		     FT_Render_Mode  render_mode );
FT_EXPORT( FT_Error )
(*qFT_Get_Kerning)( FT_Face     face,
		    FT_UInt     left_glyph,
		    FT_UInt     right_glyph,
		    FT_UInt     kern_mode,
		    FT_Vector  *akerning );
FT_EXPORT( FT_Error )
(*qFT_Attach_Stream)( FT_Face        face,
		      FT_Open_Args*  parameters );
/*
================================================================================
Support for dynamically loading the FreeType2 library
================================================================================
*/

static dllfunction_t ft2funcs[] =
{
	{"FT_Init_FreeType",		(void **) &qFT_Init_FreeType},
	{"FT_Done_FreeType",		(void **) &qFT_Done_FreeType},
	//{"FT_New_Face",			(void **) &qFT_New_Face},
	{"FT_New_Memory_Face",		(void **) &qFT_New_Memory_Face},
	{"FT_Done_Face",		(void **) &qFT_Done_Face},
	{"FT_Select_Size",		(void **) &qFT_Select_Size},
	{"FT_Request_Size",		(void **) &qFT_Request_Size},
	{"FT_Set_Char_Size",		(void **) &qFT_Set_Char_Size},
	{"FT_Set_Pixel_Sizes",		(void **) &qFT_Set_Pixel_Sizes},
	{"FT_Load_Glyph",		(void **) &qFT_Load_Glyph},
	{"FT_Load_Char",		(void **) &qFT_Load_Char},
	{"FT_Get_Char_Index",		(void **) &qFT_Get_Char_Index},
	{"FT_Render_Glyph",		(void **) &qFT_Render_Glyph},
	{"FT_Get_Kerning",		(void **) &qFT_Get_Kerning},
	{"FT_Attach_Stream",		(void **) &qFT_Attach_Stream},
	{NULL, NULL}
};

/// Handle for FreeType2 DLL
static dllhandle_t ft2_dll = NULL;

/// Memory pool for fonts
static mempool_t *font_mempool= NULL;

/// FreeType library handle
static FT_Library font_ft2lib = NULL;

#define POSTPROCESS_MAXRADIUS 8
typedef struct
{
	unsigned char *buf, *buf2;
	int bufsize, bufwidth, bufheight, bufpitch;
	float blur, outline, shadowx, shadowy, shadowz;
	int padding_t, padding_b, padding_l, padding_r, blurpadding_lt, blurpadding_rb, outlinepadding_t, outlinepadding_b, outlinepadding_l, outlinepadding_r;
	unsigned char circlematrix[2*POSTPROCESS_MAXRADIUS+1][2*POSTPROCESS_MAXRADIUS+1];
	unsigned char gausstable[2*POSTPROCESS_MAXRADIUS+1];
}
font_postprocess_t;
static font_postprocess_t pp;

typedef struct fontfilecache_s
{
	unsigned char *buf;
	fs_offset_t len;
	int refcount;
	char path[MAX_QPATH];
}
fontfilecache_t;
#define MAX_FONTFILES 8
static fontfilecache_t fontfiles[MAX_FONTFILES];
static const unsigned char *fontfilecache_LoadFile(const char *path, qboolean quiet, fs_offset_t *filesizepointer)
{
	int i;
	unsigned char *buf;

	for(i = 0; i < MAX_FONTFILES; ++i)
	{
		if(fontfiles[i].refcount > 0)
			if(!strcmp(path, fontfiles[i].path))
			{
				*filesizepointer = fontfiles[i].len;
				++fontfiles[i].refcount;
				return fontfiles[i].buf;
			}
	}

	buf = FS_LoadFile(path, font_mempool, quiet, filesizepointer);
	if(buf)
	{
		for(i = 0; i < MAX_FONTFILES; ++i)
			if(fontfiles[i].refcount <= 0)
			{
				strlcpy(fontfiles[i].path, path, sizeof(fontfiles[i].path));
				fontfiles[i].len = *filesizepointer;
				fontfiles[i].buf = buf;
				fontfiles[i].refcount = 1;
				return buf;
			}
	}

	return buf;
}
static void fontfilecache_Free(const unsigned char *buf)
{
	int i;
	for(i = 0; i < MAX_FONTFILES; ++i)
	{
		if(fontfiles[i].refcount > 0)
			if(fontfiles[i].buf == buf)
			{
				if(--fontfiles[i].refcount <= 0)
				{
					Mem_Free(fontfiles[i].buf);
					fontfiles[i].buf = NULL;
				}
				return;
			}
	}
	// if we get here, it used regular allocation
	Mem_Free((void *) buf);
}
static void fontfilecache_FreeAll(void)
{
	int i;
	for(i = 0; i < MAX_FONTFILES; ++i)
	{
		if(fontfiles[i].refcount > 0)
			Mem_Free(fontfiles[i].buf);
		fontfiles[i].buf = NULL;
		fontfiles[i].refcount = 0;
	}
}

/*
====================
Font_CloseLibrary

Unload the FreeType2 DLL
====================
*/
void Font_CloseLibrary (void)
{
	fontfilecache_FreeAll();
	if (font_mempool)
		Mem_FreePool(&font_mempool);
	if (font_ft2lib && qFT_Done_FreeType)
	{
		qFT_Done_FreeType(font_ft2lib);
		font_ft2lib = NULL;
	}
	Sys_UnloadLibrary (&ft2_dll);
	pp.buf = NULL;
}

/*
====================
Font_OpenLibrary

Try to load the FreeType2 DLL
====================
*/
qboolean Font_OpenLibrary (void)
{
	const char* dllnames [] =
	{
#if defined(WIN32)
		"libfreetype-6.dll",
		"freetype6.dll",
#elif defined(MACOSX)
		"libfreetype.6.dylib",
		"libfreetype.dylib",
#else
		"libfreetype.so.6",
		"libfreetype.so",
#endif
		NULL
	};

	if (r_font_disable_freetype.integer)
		return false;

	// Already loaded?
	if (ft2_dll)
		return true;

	// Load the DLL
	if (!Sys_LoadLibrary (dllnames, &ft2_dll, ft2funcs))
		return false;
	return true;
}

/*
====================
Font_Init

Initialize the freetype2 font subsystem
====================
*/

void font_start(void)
{
	if (!Font_OpenLibrary())
		return;

	if (qFT_Init_FreeType(&font_ft2lib))
	{
		Con_Print("ERROR: Failed to initialize the FreeType2 library!\n");
		Font_CloseLibrary();
		return;
	}

	font_mempool = Mem_AllocPool("FONT", 0, NULL);
	if (!font_mempool)
	{
		Con_Print("ERROR: Failed to allocate FONT memory pool!\n");
		Font_CloseLibrary();
		return;
	}
}

void font_shutdown(void)
{
	int i;
	for (i = 0; i < dp_fonts.maxsize; ++i)
	{
		if (dp_fonts.f[i].ft2)
		{
			Font_UnloadFont(dp_fonts.f[i].ft2);
			dp_fonts.f[i].ft2 = NULL;
		}
	}
	Font_CloseLibrary();
}

void font_newmap(void)
{
}

void Font_Init(void)
{
	Cvar_RegisterVariable(&r_font_nonpoweroftwo);
	Cvar_RegisterVariable(&r_font_disable_freetype);
	Cvar_RegisterVariable(&r_font_use_alpha_textures);
	Cvar_RegisterVariable(&r_font_size_snapping);
	Cvar_RegisterVariable(&r_font_kerning);
	Cvar_RegisterVariable(&r_font_diskcache);
	Cvar_RegisterVariable(&r_font_compress);
	Cvar_RegisterVariable(&developer_font);

	// let's open it at startup already
	Font_OpenLibrary();
}

/*
================================================================================
Implementation of a more or less lazy font loading and rendering code.
================================================================================
*/

#include "ft2_fontdefs.h"

ft2_font_t *Font_Alloc(void)
{
	if (!ft2_dll)
		return NULL;
	return (ft2_font_t *)Mem_Alloc(font_mempool, sizeof(ft2_font_t));
}

qboolean Font_Attach(ft2_font_t *font, ft2_attachment_t *attachment)
{
	ft2_attachment_t *na;

	font->attachmentcount++;
	na = (ft2_attachment_t*)Mem_Alloc(font_mempool, sizeof(font->attachments[0]) * font->attachmentcount);
	if (na == NULL)
		return false;
	if (font->attachments && font->attachmentcount > 1)
	{
		memcpy(na, font->attachments, sizeof(font->attachments[0]) * (font->attachmentcount - 1));
		Mem_Free(font->attachments);
	}
	memcpy(na + sizeof(font->attachments[0]) * (font->attachmentcount - 1), attachment, sizeof(*attachment));
	font->attachments = na;
	return true;
}

float Font_VirtualToRealSize(float sz)
{
	int vh;
	//int vw;
	int si;
	float sn;
	if(sz < 0)
		return sz;
	//vw = ((vid.width > 0) ? vid.width : vid_width.value);
	vh = ((vid.height > 0) ? vid.height : vid_height.value);
	// now try to scale to our actual size:
	sn = sz * vh / vid_conheight.value;
	si = (int)sn;
	if ( sn - (float)si >= 0.5 )
		++si;
	return si;
}

float Font_SnapTo(float val, float snapwidth)
{
	return floor(val / snapwidth + 0.5f) * snapwidth;
}

static qboolean Font_LoadFile(const char *name, int _face, ft2_settings_t *settings, ft2_font_t *font);
static qboolean Font_LoadSize(ft2_font_t *font, float size, qboolean check_only);
qboolean Font_LoadFont(const char *name, dp_font_t *dpfnt)
{
	int s, count, i;
	ft2_font_t *ft2, *fbfont, *fb;

	ft2 = Font_Alloc();
	if (!ft2)
	{
		dpfnt->ft2 = NULL;
		return false;
	}

	// check if a fallback font has been specified, if it has been, and the
	// font fails to load, use the image font as main font
	for (i = 0; i < MAX_FONT_FALLBACKS; ++i)
	{
		if (dpfnt->fallbacks[i][0])
			break;
	}

	if (!Font_LoadFile(name, dpfnt->req_face, &dpfnt->settings, ft2))
	{
		if (i >= MAX_FONT_FALLBACKS)
		{
			dpfnt->ft2 = NULL;
			Mem_Free(ft2);
			return false;
		}
		strlcpy(ft2->name, name, sizeof(ft2->name));
		ft2->image_font = true;
		ft2->has_kerning = false;
	}
	else
	{
		ft2->image_font = false;
	}

	// attempt to load fallback fonts:
	fbfont = ft2;
	for (i = 0; i < MAX_FONT_FALLBACKS; ++i)
	{
		if (!dpfnt->fallbacks[i][0])
			break;
		if (! (fb = Font_Alloc()) )
		{
			Con_Printf("Failed to allocate font for fallback %i of font %s\n", i, name);
			break;
		}

		if (!Font_LoadFile(dpfnt->fallbacks[i], dpfnt->fallback_faces[i], &dpfnt->settings, fb))
		{
			if(!FS_FileExists(va("%s.tga", dpfnt->fallbacks[i])))
			if(!FS_FileExists(va("%s.png", dpfnt->fallbacks[i])))
			if(!FS_FileExists(va("%s.jpg", dpfnt->fallbacks[i])))
			if(!FS_FileExists(va("%s.pcx", dpfnt->fallbacks[i])))
				Con_Printf("Failed to load font %s for fallback %i of font %s\n", dpfnt->fallbacks[i], i, name);
			Mem_Free(fb);
			continue;
		}
		count = 0;
		for (s = 0; s < MAX_FONT_SIZES && dpfnt->req_sizes[s] >= 0; ++s)
		{
			if (Font_LoadSize(fb, Font_VirtualToRealSize(dpfnt->req_sizes[s]), true))
				++count;
		}
		if (!count)
		{
			Con_Printf("Failed to allocate font for fallback %i of font %s\n", i, name);
			Font_UnloadFont(fb);
			Mem_Free(fb);
			break;
		}
		// at least one size of the fallback font loaded successfully
		// link it:
		fbfont->next = fb;
		fbfont = fb;
	}

	if (fbfont == ft2 && ft2->image_font)
	{
		// no fallbacks were loaded successfully:
		dpfnt->ft2 = NULL;
		Mem_Free(ft2);
		return false;
	}

	count = 0;
	for (s = 0; s < MAX_FONT_SIZES && dpfnt->req_sizes[s] >= 0; ++s)
	{
		if (Font_LoadSize(ft2, Font_VirtualToRealSize(dpfnt->req_sizes[s]), false))
			++count;
	}
	if (!count)
	{
		// loading failed for every requested size
		Font_UnloadFont(ft2);
		Mem_Free(ft2);
		dpfnt->ft2 = NULL;
		return false;
	}

	//Con_Printf("%i sizes loaded\n", count);
	dpfnt->ft2 = ft2;
	return true;
}

static qboolean Font_LoadFile(const char *name, int _face, ft2_settings_t *settings, ft2_font_t *font)
{
	size_t namelen;
	char filename[MAX_QPATH];
	int status;
	size_t i;
	const unsigned char *data;
	fs_offset_t datasize;

	memset(font, 0, sizeof(*font));

	if (!Font_OpenLibrary())
	{
		if (!r_font_disable_freetype.integer)
		{
			Con_Printf("WARNING: can't open load font %s\n"
				   "You need the FreeType2 DLL to load font files\n",
				   name);
		}
		return false;
	}

	font->settings = settings;

	namelen = strlen(name);

	// try load direct file
	memcpy(filename, name, namelen+1);
	data = fontfilecache_LoadFile(filename, false, &datasize);
	// try load .ttf
	if (!data)
	{
		memcpy(filename + namelen, ".ttf", 5);
		data = fontfilecache_LoadFile(filename, false, &datasize);
	}
	// try load .otf
	if (!data)
	{
		memcpy(filename + namelen, ".otf", 5);
		data = fontfilecache_LoadFile(filename, false, &datasize);
	}
	// try load .pfb/afm
	if (!data)
	{
		ft2_attachment_t afm;

		memcpy(filename + namelen, ".pfb", 5);
		data = fontfilecache_LoadFile(filename, false, &datasize);

		if (data)
		{
			memcpy(filename + namelen, ".afm", 5);
			afm.data = fontfilecache_LoadFile(filename, false, &afm.size);

			if (afm.data)
				Font_Attach(font, &afm);
		}
	}
	if (!data)
	{
		// FS_LoadFile being not-quiet should print an error :)
		return false;
	}
	Con_DPrintf("Loading font %s face %i...\n", filename, _face);

	status = qFT_New_Memory_Face(font_ft2lib, (FT_Bytes)data, datasize, _face, (FT_Face*)&font->face);
	if (status && _face != 0)
	{
		Con_Printf("Failed to load face %i of %s. Falling back to face 0\n", _face, name);
		_face = 0;
		status = qFT_New_Memory_Face(font_ft2lib, (FT_Bytes)data, datasize, 0, (FT_Face*)&font->face);
	}
	font->data = data;
	if (status)
	{
		Con_Printf("ERROR: can't create face for %s\n"
			   "Error %i\n", // TODO: error strings
			   name, status);
		Font_UnloadFont(font);
		return false;
	}

	// add the attachments
	for (i = 0; i < font->attachmentcount; ++i)
	{
		FT_Open_Args args;
		memset(&args, 0, sizeof(args));
		args.flags = FT_OPEN_MEMORY;
		args.memory_base = (const FT_Byte*)font->attachments[i].data;
		args.memory_size = font->attachments[i].size;
		if (qFT_Attach_Stream((FT_Face)font->face, &args))
			Con_Printf("Failed to add attachment %u to %s\n", (unsigned)i, font->name);
	}

	memcpy(font->name, name, namelen+1);
	font->image_font = false;
	font->has_kerning = !!(((FT_Face)(font->face))->face_flags & FT_FACE_FLAG_KERNING);
	return true;
}

void Font_Postprocess_Update(ft2_font_t *fnt, int bpp, int w, int h)
{
	int needed, x, y;
	float gausstable[2*POSTPROCESS_MAXRADIUS+1];
	qboolean need_gauss  = (!pp.buf || pp.blur != fnt->settings->blur || pp.shadowz != fnt->settings->shadowz);
	qboolean need_circle = (!pp.buf || pp.outline != fnt->settings->outline || pp.shadowx != fnt->settings->shadowx || pp.shadowy != fnt->settings->shadowy);
	pp.blur = fnt->settings->blur;
	pp.outline = fnt->settings->outline;
	pp.shadowx = fnt->settings->shadowx;
	pp.shadowy = fnt->settings->shadowy;
	pp.shadowz = fnt->settings->shadowz;
	pp.outlinepadding_l = bound(0, ceil(pp.outline - pp.shadowx), POSTPROCESS_MAXRADIUS);
	pp.outlinepadding_r = bound(0, ceil(pp.outline + pp.shadowx), POSTPROCESS_MAXRADIUS);
	pp.outlinepadding_t = bound(0, ceil(pp.outline - pp.shadowy), POSTPROCESS_MAXRADIUS);
	pp.outlinepadding_b = bound(0, ceil(pp.outline + pp.shadowy), POSTPROCESS_MAXRADIUS);
	pp.blurpadding_lt = bound(0, ceil(pp.blur - pp.shadowz), POSTPROCESS_MAXRADIUS);
	pp.blurpadding_rb = bound(0, ceil(pp.blur + pp.shadowz), POSTPROCESS_MAXRADIUS);
	pp.padding_l = pp.blurpadding_lt + pp.outlinepadding_l;
	pp.padding_r = pp.blurpadding_rb + pp.outlinepadding_r;
	pp.padding_t = pp.blurpadding_lt + pp.outlinepadding_t;
	pp.padding_b = pp.blurpadding_rb + pp.outlinepadding_b;
	if(need_gauss)
	{
		float sum = 0;
		for(x = -POSTPROCESS_MAXRADIUS; x <= POSTPROCESS_MAXRADIUS; ++x)
			gausstable[POSTPROCESS_MAXRADIUS+x] = (pp.blur > 0 ? exp(-(pow(x + pp.shadowz, 2))/(pp.blur*pp.blur * 2)) : (floor(x + pp.shadowz + 0.5) == 0));
		for(x = -pp.blurpadding_rb; x <= pp.blurpadding_lt; ++x)
			sum += gausstable[POSTPROCESS_MAXRADIUS+x];
		for(x = -POSTPROCESS_MAXRADIUS; x <= POSTPROCESS_MAXRADIUS; ++x)
			pp.gausstable[POSTPROCESS_MAXRADIUS+x] = floor(gausstable[POSTPROCESS_MAXRADIUS+x] / sum * 255 + 0.5);
	}
	if(need_circle)
	{
		for(y = -POSTPROCESS_MAXRADIUS; y <= POSTPROCESS_MAXRADIUS; ++y)
			for(x = -POSTPROCESS_MAXRADIUS; x <= POSTPROCESS_MAXRADIUS; ++x)
			{
				float d = pp.outline + 1 - sqrt(pow(x + pp.shadowx, 2) + pow(y + pp.shadowy, 2));
				pp.circlematrix[POSTPROCESS_MAXRADIUS+y][POSTPROCESS_MAXRADIUS+x] = (d >= 1) ? 255 : (d <= 0) ? 0 : floor(d * 255 + 0.5);
			}
	}
	pp.bufwidth = w + pp.padding_l + pp.padding_r;
	pp.bufheight = h + pp.padding_t + pp.padding_b;
	pp.bufpitch = pp.bufwidth;
	needed = pp.bufwidth * pp.bufheight;
	if(!pp.buf || pp.bufsize < needed * 2)
	{
		if(pp.buf)
			Mem_Free(pp.buf);
		pp.bufsize = needed * 4;
		pp.buf = (unsigned char *)Mem_Alloc(font_mempool, pp.bufsize);
		pp.buf2 = pp.buf + needed;
	}
}

void Font_Postprocess(ft2_font_t *fnt, unsigned char *imagedata, int pitch, int bpp, int w, int h, int *pad_l, int *pad_r, int *pad_t, int *pad_b)
{
	int x, y;

	// calculate gauss table
	Font_Postprocess_Update(fnt, bpp, w, h);

	if(imagedata)
	{
		// enlarge buffer
		// perform operation, not exceeding the passed padding values,
		// but possibly reducing them
		*pad_l = min(*pad_l, pp.padding_l);
		*pad_r = min(*pad_r, pp.padding_r);
		*pad_t = min(*pad_t, pp.padding_t);
		*pad_b = min(*pad_b, pp.padding_b);

		// outline the font (RGBA only)
		if(bpp == 4 && (pp.outline > 0 || pp.blur > 0 || pp.shadowx != 0 || pp.shadowy != 0 || pp.shadowz != 0)) // we can only do this in BGRA
		{
			// this is like mplayer subtitle rendering
			// bbuffer, bitmap buffer: this is our font
			// abuffer, alpha buffer: this is pp.buf
			// tmp: this is pp.buf2

			// create outline buffer
			memset(pp.buf, 0, pp.bufwidth * pp.bufheight);
			for(y = -*pad_t; y < h + *pad_b; ++y)
				for(x = -*pad_l; x < w + *pad_r; ++x)
				{
					int x1 = max(-x, -pp.outlinepadding_r);
					int y1 = max(-y, -pp.outlinepadding_b);
					int x2 = min(pp.outlinepadding_l, w-1-x);
					int y2 = min(pp.outlinepadding_t, h-1-y);
					int mx, my;
					int cur = 0;
					int highest = 0;
					for(my = y1; my <= y2; ++my)
						for(mx = x1; mx <= x2; ++mx)
						{
							cur = pp.circlematrix[POSTPROCESS_MAXRADIUS+my][POSTPROCESS_MAXRADIUS+mx] * (int)imagedata[(x+mx) * bpp + pitch * (y+my) + (bpp - 1)];
							if(cur > highest)
								highest = cur;
						}
					pp.buf[((x + pp.padding_l) + pp.bufpitch * (y + pp.padding_t))] = (highest + 128) / 255;
				}

			// blur the outline buffer
			if(pp.blur > 0 || pp.shadowz != 0)
			{
				// horizontal blur
				for(y = 0; y < pp.bufheight; ++y)
					for(x = 0; x < pp.bufwidth; ++x)
					{
						int x1 = max(-x, -pp.blurpadding_rb);
						int x2 = min(pp.blurpadding_lt, pp.bufwidth-1-x);
						int mx;
						int blurred = 0;
						for(mx = x1; mx <= x2; ++mx)
							blurred += pp.gausstable[POSTPROCESS_MAXRADIUS+mx] * (int)pp.buf[(x+mx) + pp.bufpitch * y];
						pp.buf2[x + pp.bufpitch * y] = bound(0, blurred, 65025) / 255;
					}

				// vertical blur
				for(y = 0; y < pp.bufheight; ++y)
					for(x = 0; x < pp.bufwidth; ++x)
					{
						int y1 = max(-y, -pp.blurpadding_rb);
						int y2 = min(pp.blurpadding_lt, pp.bufheight-1-y);
						int my;
						int blurred = 0;
						for(my = y1; my <= y2; ++my)
							blurred += pp.gausstable[POSTPROCESS_MAXRADIUS+my] * (int)pp.buf2[x + pp.bufpitch * (y+my)];
						pp.buf[x + pp.bufpitch * y] = bound(0, blurred, 65025) / 255;
					}
			}

			// paste the outline below the font
			for(y = -*pad_t; y < h + *pad_b; ++y)
				for(x = -*pad_l; x < w + *pad_r; ++x)
				{
					unsigned char outlinealpha = pp.buf[(x + pp.padding_l) + pp.bufpitch * (y + pp.padding_t)];
					if(outlinealpha > 0)
					{
						unsigned char oldalpha = imagedata[x * bpp + pitch * y + (bpp - 1)];
						// a' = 1 - (1 - a1) (1 - a2)
						unsigned char newalpha = 255 - ((255 - (int)outlinealpha) * (255 - (int)oldalpha)) / 255; // this is >= oldalpha
						// c' = (a2 c2 - a1 a2 c1 + a1 c1) / a' = (a2 c2 + a1 (1 - a2) c1) / a'
						unsigned char oldfactor     = (255 * (int)oldalpha) / newalpha;
						//unsigned char outlinefactor = ((255 - oldalpha) * (int)outlinealpha) / newalpha;
						int i;
						for(i = 0; i < bpp-1; ++i)
						{
							unsigned char c = imagedata[x * bpp + pitch * y + i];
							c = (c * (int)oldfactor) / 255 /* + outlinecolor[i] * (int)outlinefactor */;
							imagedata[x * bpp + pitch * y + i] = c;
						}
						imagedata[x * bpp + pitch * y + (bpp - 1)] = newalpha;
					}
					//imagedata[x * bpp + pitch * y + (bpp - 1)] |= 0x80;
				}
		}
	}
	else if(pitch)
	{
		// perform operation, not exceeding the passed padding values,
		// but possibly reducing them
		*pad_l = min(*pad_l, pp.padding_l);
		*pad_r = min(*pad_r, pp.padding_r);
		*pad_t = min(*pad_t, pp.padding_t);
		*pad_b = min(*pad_b, pp.padding_b);
	}
	else
	{
		// just calculate parameters
		*pad_l = pp.padding_l;
		*pad_r = pp.padding_r;
		*pad_t = pp.padding_t;
		*pad_b = pp.padding_b;
	}
}

static float Font_SearchSize(ft2_font_t *font, FT_Face fontface, float size);
static qboolean Font_LoadMap(ft2_font_t *font, ft2_font_map_t *mapstart, Uchar _ch, ft2_font_map_t **outmap);
static qboolean Font_LoadSize(ft2_font_t *font, float size, qboolean check_only)
{
	int map_index;
	ft2_font_map_t *fmap, temp;
	int gpad_l, gpad_r, gpad_t, gpad_b;

	if (!(size > 0.001f && size < 1000.0f))
		size = 0;

	if (!size)
		size = 16;
	if (size < 2) // bogus sizes are not allowed - and they screw up our allocations
		return false;

	for (map_index = 0; map_index < MAX_FONT_SIZES; ++map_index)
	{
		if (!font->font_maps[map_index])
			break;
		// if a similar size has already been loaded, ignore this one
		//abs(font->font_maps[map_index]->size - size) < 4
		if (font->font_maps[map_index]->size == size)
			return true;
	}

	if (map_index >= MAX_FONT_SIZES)
		return false;

	if (check_only) {
		FT_Face fontface;
		if (font->image_font)
			fontface = (FT_Face)font->next->face;
		else
			fontface = (FT_Face)font->face;
		return (Font_SearchSize(font, fontface, size) > 0);
	}

	Font_Postprocess(font, NULL, 0, 4, size*2, size*2, &gpad_l, &gpad_r, &gpad_t, &gpad_b);

	memset(&temp, 0, sizeof(temp));
	temp.size = size;
	temp.glyphSize = size*2 + max(gpad_l + gpad_r, gpad_t + gpad_b);
	if (!(r_font_nonpoweroftwo.integer && vid.support.arb_texture_non_power_of_two))
		temp.glyphSize = CeilPowerOf2(temp.glyphSize);
	temp.sfx = (1.0/64.0)/(double)size;
	temp.sfy = (1.0/64.0)/(double)size;
	temp.intSize = -1; // negative value: LoadMap must search now :)
	if (!Font_LoadMap(font, &temp, 0, &fmap))
	{
		Con_Printf("ERROR: can't load the first character map for %s\n"
			   "This is fatal\n",
			   font->name);
		Font_UnloadFont(font);
		return false;
	}
	font->font_maps[map_index] = temp.next;

	fmap->sfx = temp.sfx;
	fmap->sfy = temp.sfy;

	// load the default kerning vector:
	if (font->has_kerning)
	{
		Uchar l, r;
		FT_Vector kernvec;
		for (l = 0; l < 256; ++l)
		{
			for (r = 0; r < 256; ++r)
			{
				FT_ULong ul, ur;
				ul = qFT_Get_Char_Index((FT_Face)font->face, l);
				ur = qFT_Get_Char_Index((FT_Face)font->face, r);
				if (qFT_Get_Kerning((FT_Face)font->face, ul, ur, FT_KERNING_DEFAULT, &kernvec))
				{
					fmap->kerning.kerning[l][r][0] = 0;
					fmap->kerning.kerning[l][r][1] = 0;
				}
				else
				{
					fmap->kerning.kerning[l][r][0] = Font_SnapTo((kernvec.x / 64.0) / fmap->size, 1 / fmap->size);
					fmap->kerning.kerning[l][r][1] = Font_SnapTo((kernvec.y / 64.0) / fmap->size, 1 / fmap->size);
				}
			}
		}
	}
	return true;
}

int Font_IndexForSize(ft2_font_t *font, float _fsize, float *outw, float *outh)
{
	int match = -1;
	float value = 1000000;
	float nval;
	int matchsize = -10000;
	int m;
	float fsize_x, fsize_y;
	ft2_font_map_t **maps = font->font_maps;

	fsize_x = fsize_y = _fsize * vid.height / vid_conheight.value;
	if(outw && *outw)
		fsize_x = *outw * vid.width / vid_conwidth.value;
	if(outh && *outh)
		fsize_y = *outh * vid.height / vid_conheight.value;

	if (fsize_x < 0)
	{
		if(fsize_y < 0)
			fsize_x = fsize_y = 16;
		else
			fsize_x = fsize_y;
	}
	else
	{
		if(fsize_y < 0)
			fsize_y = fsize_x;
	}

	for (m = 0; m < MAX_FONT_SIZES; ++m)
	{
		if (!maps[m])
			continue;
		// "round up" to the bigger size if two equally-valued matches exist
		nval = 0.5 * (fabs(maps[m]->size - fsize_x) + fabs(maps[m]->size - fsize_y));
		if (match == -1 || nval < value || (nval == value && matchsize < maps[m]->size))
		{
			value = nval;
			match = m;
			matchsize = maps[m]->size;
			if (value == 0) // there is no better match
				break;
		}
	}
	if (value <= r_font_size_snapping.value)
	{
		// do NOT keep the aspect for perfect rendering
		if (outh) *outh = maps[match]->size * vid_conheight.value / vid.height;
		if (outw) *outw = maps[match]->size * vid_conwidth.value / vid.width;
	}
	return match;
}

ft2_font_map_t *Font_MapForIndex(ft2_font_t *font, int index)
{
	if (index < 0 || index >= MAX_FONT_SIZES)
		return NULL;
	return font->font_maps[index];
}

static qboolean Font_SetSize(ft2_font_t *font, float w, float h)
{
	if (font->currenth == h &&
	    ((!w && (!font->currentw || font->currentw == font->currenth)) || // check if w==h when w is not set
	     font->currentw == w)) // same size has been requested
	{
		return true;
	}
	// sorry, but freetype doesn't seem to care about other sizes
	w = (int)w;
	h = (int)h;
	if (font->image_font)
	{
		if (qFT_Set_Char_Size((FT_Face)font->next->face, (FT_F26Dot6)(w*64), (FT_F26Dot6)(h*64), 72, 72))
			return false;
	}
	else
	{
		if (qFT_Set_Char_Size((FT_Face)font->face, (FT_F26Dot6)(w*64), (FT_F26Dot6)(h*64), 72, 72))
			return false;
	}
	font->currentw = w;
	font->currenth = h;
	return true;
}

qboolean Font_GetKerningForMap(ft2_font_t *font, int map_index, float w, float h, Uchar left, Uchar right, float *outx, float *outy)
{
	ft2_font_map_t *fmap;
	if (!font->has_kerning || !r_font_kerning.integer)
		return false;
	if (map_index < 0 || map_index >= MAX_FONT_SIZES)
		return false;
	fmap = font->font_maps[map_index];
	if (!fmap)
		return false;
	if (left < 256 && right < 256)
	{
		//Con_Printf("%g : %f, %f, %f :: %f\n", (w / (float)fmap->size), w, fmap->size, fmap->intSize, Font_VirtualToRealSize(w));
		// quick-kerning, be aware of the size: scale it
		if (outx) *outx = fmap->kerning.kerning[left][right][0];// * (w / (float)fmap->size);
		if (outy) *outy = fmap->kerning.kerning[left][right][1];// * (h / (float)fmap->size);
		return true;
	}
	else
	{
		FT_Vector kernvec;
		FT_ULong ul, ur;

		//if (qFT_Set_Pixel_Sizes((FT_Face)font->face, 0, fmap->size))
#if 0
		if (!Font_SetSize(font, w, h))
		{
			// this deserves an error message
			Con_Printf("Failed to get kerning for %s\n", font->name);
			return false;
		}
		ul = qFT_Get_Char_Index(font->face, left);
		ur = qFT_Get_Char_Index(font->face, right);
		if (qFT_Get_Kerning(font->face, ul, ur, FT_KERNING_DEFAULT, &kernvec))
		{
			if (outx) *outx = Font_SnapTo(kernvec.x * fmap->sfx, 1 / fmap->size);
			if (outy) *outy = Font_SnapTo(kernvec.y * fmap->sfy, 1 / fmap->size);
			return true;
		}
#endif
		if (!Font_SetSize(font, fmap->intSize, fmap->intSize))
		{
			// this deserves an error message
			Con_Printf("Failed to get kerning for %s\n", font->name);
			return false;
		}
		ul = qFT_Get_Char_Index((FT_Face)font->face, left);
		ur = qFT_Get_Char_Index((FT_Face)font->face, right);
		if (qFT_Get_Kerning((FT_Face)font->face, ul, ur, FT_KERNING_DEFAULT, &kernvec))
		{
			if (outx) *outx = Font_SnapTo(kernvec.x * fmap->sfx, 1 / fmap->size);// * (w / (float)fmap->size);
			if (outy) *outy = Font_SnapTo(kernvec.y * fmap->sfy, 1 / fmap->size);// * (h / (float)fmap->size);
			return true;
		}
		return false;
	}
}

qboolean Font_GetKerningForSize(ft2_font_t *font, float w, float h, Uchar left, Uchar right, float *outx, float *outy)
{
	return Font_GetKerningForMap(font, Font_IndexForSize(font, h, NULL, NULL), w, h, left, right, outx, outy);
}

static void UnloadMapRec(ft2_font_map_t *map)
{
	if (map->pic)
	{
		//Draw_FreePic(map->pic); // FIXME: refcounting needed...
		map->pic = NULL;
	}
	if (map->next)
		UnloadMapRec(map->next);
	Mem_Free(map);
}

void Font_UnloadFont(ft2_font_t *font)
{
	int i;

	// unload fallbacks
	if(font->next)
		Font_UnloadFont(font->next);

	if (font->attachments && font->attachmentcount)
	{
		for (i = 0; i < (int)font->attachmentcount; ++i) {
			if (font->attachments[i].data)
				fontfilecache_Free(font->attachments[i].data);
		}
		Mem_Free(font->attachments);
		font->attachmentcount = 0;
		font->attachments = NULL;
	}
	for (i = 0; i < MAX_FONT_SIZES; ++i)
	{
		if (font->font_maps[i])
		{
			UnloadMapRec(font->font_maps[i]);
			font->font_maps[i] = NULL;
		}
	}
	if (ft2_dll)
	{
		if (font->face)
		{
			qFT_Done_Face((FT_Face)font->face);
			font->face = NULL;
		}
	}
	if (font->data) {
	    fontfilecache_Free(font->data);
	    font->data = NULL;
	}
}

static float Font_SearchSize(ft2_font_t *font, FT_Face fontface, float size)
{
	float intSize = size;
	while (1)
	{
		if (!Font_SetSize(font, intSize, intSize))
		{
			Con_Printf("ERROR: can't set size for font %s: %f ((%f))\n", font->name, size, intSize);
			return -1;
		}
		if ((fontface->size->metrics.height>>6) <= size)
			return intSize;
		if (intSize < 2)
		{
			Con_Printf("ERROR: no appropriate size found for font %s: %f\n", font->name, size);
			return -1;
		}
		--intSize;
	}
}

static qboolean Font_LoadMap(ft2_font_t *font, ft2_font_map_t *mapstart, Uchar _ch, ft2_font_map_t **outmap)
{
	char map_identifier[MAX_QPATH];
	unsigned long mapidx = _ch / FONT_CHARS_PER_MAP;
	unsigned char *data = NULL;
	FT_ULong ch, mapch;
	int status;
	int tp;
	FT_Int32 load_flags;
	int gpad_l, gpad_r, gpad_t, gpad_b;

	int pitch;
	int gR, gC; // glyph position: row and column

	ft2_font_map_t *map, *next;
	ft2_font_t *usefont;

	FT_Face fontface;

	int bytesPerPixel = 4; // change the conversion loop too if you change this!

	if (outmap)
		*outmap = NULL;

	if (r_font_use_alpha_textures.integer)
		bytesPerPixel = 1;

	if (font->image_font)
		fontface = (FT_Face)font->next->face;
	else
		fontface = (FT_Face)font->face;

	switch(font->settings->antialias)
	{
		case 0:
			switch(font->settings->hinting)
			{
				case 0:
					load_flags = FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME;
					break;
				case 1:
				case 2:
					load_flags = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME;
					break;
				default:
				case 3:
					load_flags = FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME;
					break;
			}
			break;
		default:
		case 1:
			switch(font->settings->hinting)
			{
				case 0:
					load_flags = FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT | FT_LOAD_TARGET_NORMAL;
					break;
				case 1:
					load_flags = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT;
					break;
				case 2:
					load_flags = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL;
					break;
				default:
				case 3:
					load_flags = FT_LOAD_TARGET_NORMAL;
					break;
			}
			break;
	}

	//status = qFT_Set_Pixel_Sizes((FT_Face)font->face, /*size*/0, mapstart->size);
	//if (status)
	if (font->image_font && mapstart->intSize < 0)
		mapstart->intSize = mapstart->size;
	if (mapstart->intSize < 0)
	{
		/*
		mapstart->intSize = mapstart->size;
		while (1)
		{
			if (!Font_SetSize(font, mapstart->intSize, mapstart->intSize))
			{
				Con_Printf("ERROR: can't set size for font %s: %f ((%f))\n", font->name, mapstart->size, mapstart->intSize);
				return false;
			}
			if ((fontface->size->metrics.height>>6) <= mapstart->size)
				break;
			if (mapstart->intSize < 2)
			{
				Con_Printf("ERROR: no appropriate size found for font %s: %f\n", font->name, mapstart->size);
				return false;
			}
			--mapstart->intSize;
		}
		*/
		if ((mapstart->intSize = Font_SearchSize(font, fontface, mapstart->size)) <= 0)
			return false;
		Con_DPrintf("Using size: %f for requested size %f\n", mapstart->intSize, mapstart->size);
	}

	if (!font->image_font && !Font_SetSize(font, mapstart->intSize, mapstart->intSize))
	{
		Con_Printf("ERROR: can't set sizes for font %s: %f\n", font->name, mapstart->size);
		return false;
	}

	map = (ft2_font_map_t *)Mem_Alloc(font_mempool, sizeof(ft2_font_map_t));
	if (!map)
	{
		Con_Printf("ERROR: Out of memory when loading fontmap for %s\n", font->name);
		return false;
	}

	// create a totally unique name for this map, then we will use it to make a unique cachepic_t to avoid redundant textures
	dpsnprintf(map_identifier, sizeof(map_identifier),
		"%s_cache_%g_%d_%g_%g_%g_%g_%g_%u",
		font->name,
		(double) mapstart->intSize,
		(int) load_flags,
		(double) font->settings->blur,
		(double) font->settings->outline,
		(double) font->settings->shadowx,
		(double) font->settings->shadowy,
		(double) font->settings->shadowz,
		(unsigned) mapidx);

	// create a cachepic_t from the data now, or reuse an existing one
	map->pic = Draw_CachePic_Flags(map_identifier, CACHEPICFLAG_QUIET);
	if (developer_font.integer)
	{
		if (map->pic->tex == r_texture_notexture)
			Con_Printf("Generating font map %s (size: %.1f MB)\n", map_identifier, mapstart->glyphSize * (256 * 4 / 1048576.0) * mapstart->glyphSize);
		else
			Con_Printf("Using cached font map %s (size: %.1f MB)\n", map_identifier, mapstart->glyphSize * (256 * 4 / 1048576.0) * mapstart->glyphSize);
	}

	Font_Postprocess(font, NULL, 0, bytesPerPixel, mapstart->size*2, mapstart->size*2, &gpad_l, &gpad_r, &gpad_t, &gpad_b);

	// copy over the information
	map->size = mapstart->size;
	map->intSize = mapstart->intSize;
	map->glyphSize = mapstart->glyphSize;
	map->sfx = mapstart->sfx;
	map->sfy = mapstart->sfy;

	pitch = map->glyphSize * FONT_CHARS_PER_LINE * bytesPerPixel;
	if (map->pic->tex == r_texture_notexture)
	{
		data = (unsigned char *)Mem_Alloc(font_mempool, (FONT_CHAR_LINES * map->glyphSize) * pitch);
		if (!data)
		{
			Con_Printf("ERROR: Failed to allocate memory for font %s size %g\n", font->name, map->size);
			Mem_Free(map);
			return false;
		}
		// initialize as white texture with zero alpha
		tp = 0;
		while (tp < (FONT_CHAR_LINES * map->glyphSize) * pitch)
		{
			if (bytesPerPixel == 4)
			{
				data[tp++] = 0xFF;
				data[tp++] = 0xFF;
				data[tp++] = 0xFF;
			}
			data[tp++] = 0x00;
		}
	}

	memset(map->width_of, 0, sizeof(map->width_of));

	// insert the map
	map->start = mapidx * FONT_CHARS_PER_MAP;
	next = mapstart;
	while(next->next && next->next->start < map->start)
		next = next->next;
	map->next = next->next;
	next->next = map;

	gR = 0;
	gC = -1;
	for (ch = map->start;
	     ch < (FT_ULong)map->start + FONT_CHARS_PER_MAP;
	     ++ch)
	{
		FT_ULong glyphIndex;
		int w, h, x, y;
		FT_GlyphSlot glyph;
		FT_Bitmap *bmp;
		unsigned char *imagedata = NULL, *dst, *src;
		glyph_slot_t *mapglyph;
		FT_Face face;
		int pad_l, pad_r, pad_t, pad_b;

		mapch = ch - map->start;

		if (developer_font.integer)
			Con_DPrint("glyphinfo: ------------- GLYPH INFO -----------------\n");

		++gC;
		if (gC >= FONT_CHARS_PER_LINE)
		{
			gC -= FONT_CHARS_PER_LINE;
			++gR;
		}

		if (data)
		{
			imagedata = data + gR * pitch * map->glyphSize + gC * map->glyphSize * bytesPerPixel;
			imagedata += gpad_t * pitch + gpad_l * bytesPerPixel;
		}
		//status = qFT_Load_Char(face, ch, FT_LOAD_RENDER);
		// we need the glyphIndex
		face = (FT_Face)font->face;
		usefont = NULL;
		if (font->image_font && mapch == ch && img_fontmap[mapch])
		{
			map->glyphs[mapch].image = true;
			continue;
		}
		glyphIndex = qFT_Get_Char_Index(face, ch);
		if (glyphIndex == 0)
		{
			// by convention, 0 is the "missing-glyph"-glyph
			// try to load from a fallback font
			for(usefont = font->next; usefont != NULL; usefont = usefont->next)
			{
				if (!Font_SetSize(usefont, mapstart->intSize, mapstart->intSize))
					continue;
				// try that glyph
				face = (FT_Face)usefont->face;
				glyphIndex = qFT_Get_Char_Index(face, ch);
				if (glyphIndex == 0)
					continue;
				status = qFT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER | load_flags);
				if (status)
					continue;
				break;
			}
			if (!usefont)
			{
				//Con_Printf("failed to load fallback glyph for char %lx from font %s\n", (unsigned long)ch, font->name);
				// now we let it use the "missing-glyph"-glyph
				face = (FT_Face)font->face;
				glyphIndex = 0;
			}
		}

		if (!usefont)
		{
			usefont = font;
			face = (FT_Face)font->face;
			status = qFT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER | load_flags);
			if (status)
			{
				//Con_Printf("failed to load glyph %lu for %s\n", glyphIndex, font->name);
				Con_DPrintf("failed to load glyph for char %lx from font %s\n", (unsigned long)ch, font->name);
				continue;
			}
		}

		glyph = face->glyph;
		bmp = &glyph->bitmap;

		w = bmp->width;
		h = bmp->rows;

		if (w > (map->glyphSize - gpad_l - gpad_r) || h > (map->glyphSize - gpad_t - gpad_b)) {
			Con_Printf("WARNING: Glyph %lu is too big in font %s, size %g: %i x %i\n", ch, font->name, map->size, w, h);
			if (w > map->glyphSize)
				w = map->glyphSize - gpad_l - gpad_r;
			if (h > map->glyphSize)
				h = map->glyphSize;
		}

		if (imagedata)
		{
			switch (bmp->pixel_mode)
			{
			case FT_PIXEL_MODE_MONO:
				if (developer_font.integer)
					Con_DPrint("glyphinfo:   Pixel Mode: MONO\n");
				break;
			case FT_PIXEL_MODE_GRAY2:
				if (developer_font.integer)
					Con_DPrint("glyphinfo:   Pixel Mode: GRAY2\n");
				break;
			case FT_PIXEL_MODE_GRAY4:
				if (developer_font.integer)
					Con_DPrint("glyphinfo:   Pixel Mode: GRAY4\n");
				break;
			case FT_PIXEL_MODE_GRAY:
				if (developer_font.integer)
					Con_DPrint("glyphinfo:   Pixel Mode: GRAY\n");
				break;
			default:
				if (developer_font.integer)
					Con_DPrintf("glyphinfo:   Pixel Mode: Unknown: %i\n", bmp->pixel_mode);
				Mem_Free(data);
				Con_Printf("ERROR: Unrecognized pixel mode for font %s size %f: %i\n", font->name, mapstart->size, bmp->pixel_mode);
				return false;
			}
			for (y = 0; y < h; ++y)
			{
				dst = imagedata + y * pitch;
				src = bmp->buffer + y * bmp->pitch;

				switch (bmp->pixel_mode)
				{
				case FT_PIXEL_MODE_MONO:
					dst += bytesPerPixel - 1; // shift to alpha byte
					for (x = 0; x < bmp->width; x += 8)
					{
						unsigned char ch = *src++;
						*dst = 255 * !!((ch & 0x80) >> 7); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x40) >> 6); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x20) >> 5); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x10) >> 4); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x08) >> 3); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x04) >> 2); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x02) >> 1); dst += bytesPerPixel;
						*dst = 255 * !!((ch & 0x01) >> 0); dst += bytesPerPixel;
					}
					break;
				case FT_PIXEL_MODE_GRAY2:
					dst += bytesPerPixel - 1; // shift to alpha byte
					for (x = 0; x < bmp->width; x += 4)
					{
						unsigned char ch = *src++;
						*dst = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += bytesPerPixel;
						*dst = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += bytesPerPixel;
						*dst = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += bytesPerPixel;
						*dst = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += bytesPerPixel;
					}
					break;
				case FT_PIXEL_MODE_GRAY4:
					dst += bytesPerPixel - 1; // shift to alpha byte
					for (x = 0; x < bmp->width; x += 2)
					{
						unsigned char ch = *src++;
						*dst = ( ((ch & 0xF0) >> 4) * 0x11); dst += bytesPerPixel;
						*dst = ( ((ch & 0x0F) ) * 0x11); dst += bytesPerPixel;
					}
					break;
				case FT_PIXEL_MODE_GRAY:
					// in this case pitch should equal width
					for (tp = 0; tp < bmp->pitch; ++tp)
						dst[(bytesPerPixel - 1) + tp*bytesPerPixel] = src[tp]; // copy the grey value into the alpha bytes

					//memcpy((void*)dst, (void*)src, bmp->pitch);
					//dst += bmp->pitch;
					break;
				default:
					break;
				}
			}

			pad_l = gpad_l;
			pad_r = gpad_r;
			pad_t = gpad_t;
			pad_b = gpad_b;
			Font_Postprocess(font, imagedata, pitch, bytesPerPixel, w, h, &pad_l, &pad_r, &pad_t, &pad_b);
		}
		else
		{
			pad_l = gpad_l;
			pad_r = gpad_r;
			pad_t = gpad_t;
			pad_b = gpad_b;
			Font_Postprocess(font, NULL, pitch, bytesPerPixel, w, h, &pad_l, &pad_r, &pad_t, &pad_b);
		}


		// now fill map->glyphs[ch - map->start]
		mapglyph = &map->glyphs[mapch];

		{
			// old way
			// double advance = (double)glyph->metrics.horiAdvance * map->sfx;

			double bearingX = (glyph->metrics.horiBearingX / 64.0) / map->size;
			//double bearingY = (glyph->metrics.horiBearingY >> 6) / map->size;
			double advance = (glyph->advance.x / 64.0) / map->size;
			//double mWidth = (glyph->metrics.width >> 6) / map->size;
			//double mHeight = (glyph->metrics.height >> 6) / map->size;

			mapglyph->txmin = ( (double)(gC * map->glyphSize) + (double)(gpad_l - pad_l) ) / ( (double)(map->glyphSize * FONT_CHARS_PER_LINE) );
			mapglyph->txmax = mapglyph->txmin + (double)(bmp->width + pad_l + pad_r) / ( (double)(map->glyphSize * FONT_CHARS_PER_LINE) );
			mapglyph->tymin = ( (double)(gR * map->glyphSize) + (double)(gpad_r - pad_r) ) / ( (double)(map->glyphSize * FONT_CHAR_LINES) );
			mapglyph->tymax = mapglyph->tymin + (double)(bmp->rows + pad_t + pad_b) / ( (double)(map->glyphSize * FONT_CHAR_LINES) );
			//mapglyph->vxmin = bearingX;
			//mapglyph->vxmax = bearingX + mWidth;
			mapglyph->vxmin = (glyph->bitmap_left - pad_l) / map->size;
			mapglyph->vxmax = mapglyph->vxmin + (bmp->width + pad_l + pad_r) / map->size; // don't ask
			//mapglyph->vymin = -bearingY;
			//mapglyph->vymax = mHeight - bearingY;
			mapglyph->vymin = (-glyph->bitmap_top - pad_t) / map->size;
			mapglyph->vymax = mapglyph->vymin + (bmp->rows + pad_t + pad_b) / map->size;
			//Con_Printf("dpi = %f %f (%f %d) %d %d\n", bmp->width / (mapglyph->vxmax - mapglyph->vxmin), bmp->rows / (mapglyph->vymax - mapglyph->vymin), map->size, map->glyphSize, (int)fontface->size->metrics.x_ppem, (int)fontface->size->metrics.y_ppem);
			//mapglyph->advance_x = advance * usefont->size;
			//mapglyph->advance_x = advance;
			mapglyph->advance_x = Font_SnapTo(advance, 1 / map->size);
			mapglyph->advance_y = 0;

			if (developer_font.integer)
			{
				Con_DPrintf("glyphinfo:   Glyph: %lu   at (%i, %i)\n", (unsigned long)ch, gC, gR);
				Con_DPrintf("glyphinfo:   %f, %f, %lu\n", bearingX, map->sfx, (unsigned long)glyph->metrics.horiBearingX);
				if (ch >= 32 && ch <= 128)
					Con_DPrintf("glyphinfo:   Character: %c\n", (int)ch);
				Con_DPrintf("glyphinfo:   Vertex info:\n");
				Con_DPrintf("glyphinfo:     X: ( %f  --  %f )\n", mapglyph->vxmin, mapglyph->vxmax);
				Con_DPrintf("glyphinfo:     Y: ( %f  --  %f )\n", mapglyph->vymin, mapglyph->vymax);
				Con_DPrintf("glyphinfo:   Texture info:\n");
				Con_DPrintf("glyphinfo:     S: ( %f  --  %f )\n", mapglyph->txmin, mapglyph->txmax);
				Con_DPrintf("glyphinfo:     T: ( %f  --  %f )\n", mapglyph->tymin, mapglyph->tymax);
				Con_DPrintf("glyphinfo:   Advance: %f, %f\n", mapglyph->advance_x, mapglyph->advance_y);
			}
		}
		map->glyphs[mapch].image = false;
	}

	if (map->pic->tex == r_texture_notexture)
	{
		int w = map->glyphSize * FONT_CHARS_PER_LINE;
		int h = map->glyphSize * FONT_CHAR_LINES;
		rtexture_t *tex;
		// abuse the Draw_CachePic system to keep track of this texture
		tex = R_LoadTexture2D(drawtexturepool, map_identifier, w, h, data, r_font_use_alpha_textures.integer ? TEXTYPE_ALPHA : TEXTYPE_RGBA, TEXF_ALPHA | (r_font_compress.integer > 0 ? TEXF_COMPRESS : 0), -1, NULL);
		// if tex is NULL for any reason, the pic->tex will remain set to r_texture_notexture
		if (tex)
			map->pic->tex = tex;

		if (r_font_diskcache.integer >= 1)
		{
			// swap to BGRA for tga writing...
			int s = w * h;
			int x;
			int b;
			for (x = 0;x < s;x++)
			{
				b = data[x*4+0];
				data[x*4+0] = data[x*4+2];
				data[x*4+2] = b;
			}
			Image_WriteTGABGRA(va("%s.tga", map_identifier), w, h, data);
			if (r_font_compress.integer && qglGetCompressedTexImageARB && tex)
				R_SaveTextureDDSFile(tex, va("dds/%s.dds", map_identifier), r_texture_dds_save.integer < 2, true);
		}
	}

	if(data)
		Mem_Free(data);

	if (map->pic->tex == r_texture_notexture)
	{
		// if the first try isn't successful, keep it with a broken texture
		// otherwise we retry to load it every single frame where ft2 rendering is used
		// this would be bad...
		// only `data' must be freed
		Con_Printf("ERROR: Failed to generate texture for font %s size %f map %lu\n",
			   font->name, mapstart->size, mapidx);
		return false;
	}
	if (outmap)
		*outmap = map;
	return true;
}

qboolean Font_LoadMapForIndex(ft2_font_t *font, int map_index, Uchar _ch, ft2_font_map_t **outmap)
{
	if (map_index < 0 || map_index >= MAX_FONT_SIZES)
		return false;
	// the first map must have been loaded already
	if (!font->font_maps[map_index])
		return false;
	return Font_LoadMap(font, font->font_maps[map_index], _ch, outmap);
}

ft2_font_map_t *FontMap_FindForChar(ft2_font_map_t *start, Uchar ch)
{
	while (start && start->start + FONT_CHARS_PER_MAP <= ch)
		start = start->next;
	if (start && start->start > ch)
		return NULL;
	return start;
}
