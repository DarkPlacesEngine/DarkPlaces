
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"
#include "image_png.h"

cvar_t gl_max_size = {CVAR_SAVE, "gl_max_size", "2048", "maximum allowed texture size, can be used to reduce video memory usage, note: this is automatically reduced to match video card capabilities (such as 256 on 3Dfx cards before Voodoo4/5)"};
cvar_t gl_picmip = {CVAR_SAVE, "gl_picmip", "0", "reduces resolution of textures by powers of 2, for example 1 will halve width/height, reducing texture memory usage by 75%"};
cvar_t r_lerpimages = {CVAR_SAVE, "r_lerpimages", "1", "bilinear filters images when scaling them up to power of 2 size (mode 1), looks better than glquake (mode 0)"};
cvar_t r_precachetextures = {CVAR_SAVE, "r_precachetextures", "1", "0 = never upload textures until used, 1 = upload most textures before use (exceptions: rarely used skin colormap layers), 2 = upload all textures before use (can increase texture memory usage significantly)"};
cvar_t gl_texture_anisotropy = {CVAR_SAVE, "gl_texture_anisotropy", "1", "anisotropic filtering quality (if supported by hardware), 1 sample (no anisotropy) and 8 sample (8 tap anisotropy) are recommended values"};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_mag = GL_LINEAR;


static mempool_t *texturemempool;

// note: this must not conflict with TEXF_ flags in r_textures.h
// cleared when a texture is uploaded
#define GLTEXF_UPLOAD 0x00010000
// bitmask for mismatch checking
#define GLTEXF_IMPORTANTBITS (0)
// set when image is uploaded and freed
#define GLTEXF_DESTROYED 0x00040000

typedef struct textypeinfo_s
{
	int textype;
	int inputbytesperpixel;
	int internalbytesperpixel;
	int glformat;
	int glinternalformat;
}
textypeinfo_t;

static textypeinfo_t textype_palette       = {TEXTYPE_PALETTE, 1, 4, GL_RGBA   , 3};
static textypeinfo_t textype_rgb           = {TEXTYPE_RGB    , 3, 3, GL_RGB    , 3};
static textypeinfo_t textype_rgba          = {TEXTYPE_RGBA   , 4, 4, GL_RGBA   , 3};
static textypeinfo_t textype_palette_alpha = {TEXTYPE_PALETTE, 1, 4, GL_RGBA   , 4};
static textypeinfo_t textype_rgba_alpha    = {TEXTYPE_RGBA   , 4, 4, GL_RGBA   , 4};

#define GLTEXTURETYPE_1D 0
#define GLTEXTURETYPE_2D 1
#define GLTEXTURETYPE_3D 2
#define GLTEXTURETYPE_CUBEMAP 3

static int gltexturetypeenums[4] = {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARB};
static int gltexturetypebindingenums[4] = {GL_TEXTURE_BINDING_1D, GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_CUBE_MAP_ARB};
static int gltexturetypedimensions[4] = {1, 2, 3, 2};
static int cubemapside[6] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
};

typedef struct gltexture_s
{
	// this field is exposed to the R_GetTexture macro, for speed reasons
	// (must be identical in rtexture_t)
	int texnum; // GL texture slot number

	// pointer to texturepool (check this to see if the texture is allocated)
	struct gltexturepool_s *pool;
	// pointer to next texture in texturepool chain
	struct gltexture_s *chain;
	// name of the texture (this might be removed someday), no duplicates
	char identifier[32];
	// original data size in *inputtexels
	int inputwidth, inputheight, inputdepth;
	// copy of the original texture(s) supplied to the upload function, for
	// delayed uploads (non-precached)
	unsigned char *inputtexels;
	// original data size in *inputtexels
	int inputdatasize;
	// flags supplied to the LoadTexture function
	// (might be altered to remove TEXF_ALPHA), and GLTEXF_ private flags
	int flags;
	// pointer to one of the textype_ structs
	textypeinfo_t *textype;
	// one of the GLTEXTURETYPE_ values
	int texturetype;
	// palette if the texture is TEXTYPE_PALETTE
	const unsigned int *palette;
	// power of 2 size, after gl_picmip and gl_max_size are applied
	int tilewidth, tileheight, tiledepth;
	// 1 or 6 depending on texturetype
	int sides;
	// bytes per pixel
	int bytesperpixel;
	// GL_RGB or GL_RGBA
	int glformat;
	// 3 or 4
	int glinternalformat;
}
gltexture_t;

#define TEXTUREPOOL_SENTINEL 0xC0DEDBAD

typedef struct gltexturepool_s
{
	unsigned int sentinel;
	struct gltexture_s *gltchain;
	struct gltexturepool_s *next;
}
gltexturepool_t;

static gltexturepool_t *gltexturepoolchain = NULL;

static unsigned char *resizebuffer = NULL, *colorconvertbuffer;
static int resizebuffersize = 0;
static unsigned char *texturebuffer;
static int texturebuffersize = 0;

static textypeinfo_t *R_GetTexTypeInfo(int textype, int flags)
{
	if (flags & TEXF_ALPHA)
	{
		switch(textype)
		{
		case TEXTYPE_PALETTE:
			return &textype_palette_alpha;
		case TEXTYPE_RGB:
			Host_Error("R_GetTexTypeInfo: RGB format has no alpha, TEXF_ALPHA not allowed");
			return NULL;
		case TEXTYPE_RGBA:
			return &textype_rgba_alpha;
		default:
			Host_Error("R_GetTexTypeInfo: unknown texture format");
			return NULL;
		}
	}
	else
	{
		switch(textype)
		{
		case TEXTYPE_PALETTE:
			return &textype_palette;
		case TEXTYPE_RGB:
			return &textype_rgb;
		case TEXTYPE_RGBA:
			return &textype_rgba;
		default:
			Host_Error("R_GetTexTypeInfo: unknown texture format");
			return NULL;
		}
	}
}

static void R_UploadTexture(gltexture_t *t);

static void R_PrecacheTexture(gltexture_t *glt)
{
	int precache;
	precache = false;
	if (glt->flags & TEXF_ALWAYSPRECACHE)
		precache = true;
	else if (r_precachetextures.integer >= 2)
		precache = true;
	else if (r_precachetextures.integer >= 1)
		if (glt->flags & TEXF_PRECACHE)
			precache = true;

	if (precache)
		R_UploadTexture(glt);
}

int R_RealGetTexture(rtexture_t *rt)
{
	if (rt)
	{
		gltexture_t *glt;
		glt = (gltexture_t *)rt;
		if (glt->flags & GLTEXF_UPLOAD)
			R_UploadTexture(glt);
		return glt->texnum;
	}
	else
		return 0;
}

void R_FreeTexture(rtexture_t *rt)
{
	gltexture_t *glt, **gltpointer;

	glt = (gltexture_t *)rt;
	if (glt == NULL)
		Host_Error("R_FreeTexture: texture == NULL");

	for (gltpointer = &glt->pool->gltchain;*gltpointer && *gltpointer != glt;gltpointer = &(*gltpointer)->chain);
	if (*gltpointer == glt)
		*gltpointer = glt->chain;
	else
		Host_Error("R_FreeTexture: texture \"%s\" not linked in pool", glt->identifier);

	if (glt->texnum)
		qglDeleteTextures(1, (GLuint *)&glt->texnum);

	if (glt->inputtexels)
		Mem_Free(glt->inputtexels);
	Mem_Free(glt);
}

rtexturepool_t *R_AllocTexturePool(void)
{
	gltexturepool_t *pool;
	if (texturemempool == NULL)
		return NULL;
	pool = (gltexturepool_t *)Mem_Alloc(texturemempool, sizeof(gltexturepool_t));
	if (pool == NULL)
		return NULL;
	pool->next = gltexturepoolchain;
	gltexturepoolchain = pool;
	pool->sentinel = TEXTUREPOOL_SENTINEL;
	return (rtexturepool_t *)pool;
}

void R_FreeTexturePool(rtexturepool_t **rtexturepool)
{
	gltexturepool_t *pool, **poolpointer;
	if (rtexturepool == NULL)
		return;
	if (*rtexturepool == NULL)
		return;
	pool = (gltexturepool_t *)(*rtexturepool);
	*rtexturepool = NULL;
	if (pool->sentinel != TEXTUREPOOL_SENTINEL)
		Host_Error("R_FreeTexturePool: pool already freed");
	for (poolpointer = &gltexturepoolchain;*poolpointer && *poolpointer != pool;poolpointer = &(*poolpointer)->next);
	if (*poolpointer == pool)
		*poolpointer = pool->next;
	else
		Host_Error("R_FreeTexturePool: pool not linked");
	while (pool->gltchain)
		R_FreeTexture((rtexture_t *)pool->gltchain);
	Mem_Free(pool);
}


typedef struct glmode_s
{
	char *name;
	int minification, magnification;
}
glmode_t;

static glmode_t modes[6] =
{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

static void GL_TextureMode_f (void)
{
	int i;
	GLint oldbindtexnum;
	gltexture_t *glt;
	gltexturepool_t *pool;

	if (Cmd_Argc() == 1)
	{
		for (i = 0;i < 6;i++)
		{
			if (gl_filter_min == modes[i].minification)
			{
				Con_Printf("%s\n", modes[i].name);
				return;
			}
		}
		Con_Print("current filter is unknown???\n");
		return;
	}

	for (i = 0;i < 6;i++)
		if (!strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	if (i == 6)
	{
		Con_Print("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minification;
	gl_filter_mag = modes[i].magnification;

	// change all the existing mipmap texture objects
	// FIXME: force renderer(/client/something?) restart instead?
	for (pool = gltexturepoolchain;pool;pool = pool->next)
	{
		for (glt = pool->gltchain;glt;glt = glt->chain)
		{
			// only update already uploaded images
			if (!(glt->flags & (GLTEXF_UPLOAD | TEXF_FORCENEAREST | TEXF_FORCELINEAR)))
			{
				qglGetIntegerv(gltexturetypebindingenums[glt->texturetype], &oldbindtexnum);
				qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);
				if (glt->flags & TEXF_MIPMAP)
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_min);
				else
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_mag);
				qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAG_FILTER, gl_filter_mag);
				qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);
			}
		}
	}
}

static void GL_Texture_CalcImageSize(int texturetype, int flags, int inwidth, int inheight, int indepth, int *outwidth, int *outheight, int *outdepth)
{
	int picmip = 0, maxsize = 0, width2 = 1, height2 = 1, depth2 = 1;

	if (gl_max_size.integer > gl_max_texture_size)
		Cvar_SetValue("gl_max_size", gl_max_texture_size);

	switch (texturetype)
	{
	default:
	case GLTEXTURETYPE_1D:
	case GLTEXTURETYPE_2D:
		maxsize = gl_max_texture_size;
		break;
	case GLTEXTURETYPE_3D:
		maxsize = gl_max_3d_texture_size;
		break;
	case GLTEXTURETYPE_CUBEMAP:
		maxsize = gl_max_cube_map_texture_size;
		break;
	}

	if (flags & TEXF_PICMIP)
	{
		maxsize = min(maxsize, gl_max_size.integer);
		picmip = gl_picmip.integer;
	}

	if (outwidth)
	{
		for (width2 = 1;width2 < inwidth;width2 <<= 1);
		for (width2 >>= picmip;width2 > maxsize;width2 >>= 1);
		*outwidth = max(1, width2);
	}
	if (outheight)
	{
		for (height2 = 1;height2 < inheight;height2 <<= 1);
		for (height2 >>= picmip;height2 > maxsize;height2 >>= 1);
		*outheight = max(1, height2);
	}
	if (outdepth)
	{
		for (depth2 = 1;depth2 < indepth;depth2 <<= 1);
		for (depth2 >>= picmip;depth2 > maxsize;depth2 >>= 1);
		*outdepth = max(1, depth2);
	}
}


static int R_CalcTexelDataSize (gltexture_t *glt)
{
	int width2, height2, depth2, size;

	GL_Texture_CalcImageSize(glt->texturetype, glt->flags, glt->inputwidth, glt->inputheight, glt->inputdepth, &width2, &height2, &depth2);

	size = width2 * height2 * depth2;

	if (glt->flags & TEXF_MIPMAP)
	{
		while (width2 > 1 || height2 > 1 || depth2 > 1)
		{
			if (width2 > 1)
				width2 >>= 1;
			if (height2 > 1)
				height2 >>= 1;
			if (depth2 > 1)
				depth2 >>= 1;
			size += width2 * height2 * depth2;
		}
	}

	return size * glt->textype->internalbytesperpixel * glt->sides;
}

void R_TextureStats_Print(qboolean printeach, qboolean printpool, qboolean printtotal)
{
	int glsize;
	int isloaded;
	int pooltotal = 0, pooltotalt = 0, pooltotalp = 0, poolloaded = 0, poolloadedt = 0, poolloadedp = 0;
	int sumtotal = 0, sumtotalt = 0, sumtotalp = 0, sumloaded = 0, sumloadedt = 0, sumloadedp = 0;
	gltexture_t *glt;
	gltexturepool_t *pool;
	if (printeach)
		Con_Print("glsize input loaded mip alpha name\n");
	for (pool = gltexturepoolchain;pool;pool = pool->next)
	{
		pooltotal = 0;
		pooltotalt = 0;
		pooltotalp = 0;
		poolloaded = 0;
		poolloadedt = 0;
		poolloadedp = 0;
		for (glt = pool->gltchain;glt;glt = glt->chain)
		{
			glsize = R_CalcTexelDataSize(glt);
			isloaded = !(glt->flags & GLTEXF_UPLOAD);
			pooltotal++;
			pooltotalt += glsize;
			pooltotalp += glt->inputdatasize;
			if (isloaded)
			{
				poolloaded++;
				poolloadedt += glsize;
				poolloadedp += glt->inputdatasize;
			}
			if (printeach)
				Con_Printf("%c%4i%c%c%4i%c %s %s %s %s\n", isloaded ? '[' : ' ', (glsize + 1023) / 1024, isloaded ? ']' : ' ', glt->inputtexels ? '[' : ' ', (glt->inputdatasize + 1023) / 1024, glt->inputtexels ? ']' : ' ', isloaded ? "loaded" : "      ", (glt->flags & TEXF_MIPMAP) ? "mip" : "   ", (glt->flags & TEXF_ALPHA) ? "alpha" : "     ", glt->identifier);
		}
		if (printpool)
			Con_Printf("texturepool %10p total: %i (%.3fMB, %.3fMB original), uploaded %i (%.3fMB, %.3fMB original), upload on demand %i (%.3fMB, %.3fMB original)\n", pool, pooltotal, pooltotalt / 1048576.0, pooltotalp / 1048576.0, poolloaded, poolloadedt / 1048576.0, poolloadedp / 1048576.0, pooltotal - poolloaded, (pooltotalt - poolloadedt) / 1048576.0, (pooltotalp - poolloadedp) / 1048576.0);
		sumtotal += pooltotal;
		sumtotalt += pooltotalt;
		sumtotalp += pooltotalp;
		sumloaded += poolloaded;
		sumloadedt += poolloadedt;
		sumloadedp += poolloadedp;
	}
	if (printtotal)
		Con_Printf("textures total: %i (%.3fMB, %.3fMB original), uploaded %i (%.3fMB, %.3fMB original), upload on demand %i (%.3fMB, %.3fMB original)\n", sumtotal, sumtotalt / 1048576.0, sumtotalp / 1048576.0, sumloaded, sumloadedt / 1048576.0, sumloadedp / 1048576.0, sumtotal - sumloaded, (sumtotalt - sumloadedt) / 1048576.0, (sumtotalp - sumloadedp) / 1048576.0);
}

static void R_TextureStats_f(void)
{
	R_TextureStats_Print(true, true, true);
}

static void r_textures_start(void)
{
	// LordHavoc: allow any alignment
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	CHECKGLERROR

	texturemempool = Mem_AllocPool("texture management", 0, NULL);

	// Disable JPEG screenshots if the DLL isn't loaded
	if (! JPEG_OpenLibrary ())
		Cvar_SetValueQuick (&scr_screenshot_jpeg, 0);
	// TODO: support png screenshots?
	PNG_OpenLibrary ();
}

static void r_textures_shutdown(void)
{
	rtexturepool_t *temp;

	JPEG_CloseLibrary ();

	while(gltexturepoolchain)
	{
		temp = (rtexturepool_t *) gltexturepoolchain;
		R_FreeTexturePool(&temp);
	}

	resizebuffersize = 0;
	texturebuffersize = 0;
	resizebuffer = NULL;
	colorconvertbuffer = NULL;
	texturebuffer = NULL;
	Mem_FreePool(&texturemempool);
}

static void r_textures_newmap(void)
{
}

void R_Textures_Init (void)
{
	Cmd_AddCommand("gl_texturemode", &GL_TextureMode_f, "set texture filtering mode (GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, etc)");
	Cmd_AddCommand("r_texturestats", R_TextureStats_f, "print information about all loaded textures and some statistics");
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&r_lerpimages);
	Cvar_RegisterVariable (&r_precachetextures);
	Cvar_RegisterVariable (&gl_texture_anisotropy);

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap);
}

void R_Textures_Frame (void)
{
	static int old_aniso = 0;

	// could do procedural texture animation here, if we keep track of which
	// textures were accessed this frame...

	// free the resize buffers
	resizebuffersize = 0;
	if (resizebuffer)
	{
		Mem_Free(resizebuffer);
		resizebuffer = NULL;
	}
	if (colorconvertbuffer)
	{
		Mem_Free(colorconvertbuffer);
		colorconvertbuffer = NULL;
	}

	if (old_aniso != gl_texture_anisotropy.integer)
	{
		gltexture_t *glt;
		gltexturepool_t *pool;
		GLint oldbindtexnum;

		old_aniso = bound(1, gl_texture_anisotropy.integer, gl_max_anisotropy);

		Cvar_SetValueQuick(&gl_texture_anisotropy, old_aniso);

		for (pool = gltexturepoolchain;pool;pool = pool->next)
		{
			for (glt = pool->gltchain;glt;glt = glt->chain)
			{
				// only update already uploaded images
				if ((glt->flags & (GLTEXF_UPLOAD | TEXF_MIPMAP)) == TEXF_MIPMAP)
				{
					qglGetIntegerv(gltexturetypebindingenums[glt->texturetype], &oldbindtexnum);

					qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAX_ANISOTROPY_EXT, old_aniso);CHECKGLERROR

					qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);
				}
			}
		}
	}
}

void R_MakeResizeBufferBigger(int size)
{
	if (resizebuffersize < size)
	{
		resizebuffersize = size;
		if (resizebuffer)
			Mem_Free(resizebuffer);
		if (colorconvertbuffer)
			Mem_Free(colorconvertbuffer);
		resizebuffer = (unsigned char *)Mem_Alloc(texturemempool, resizebuffersize);
		colorconvertbuffer = (unsigned char *)Mem_Alloc(texturemempool, resizebuffersize);
		if (!resizebuffer || !colorconvertbuffer)
			Host_Error("R_Upload: out of memory");
	}
}

static void GL_SetupTextureParameters(int flags, int texturetype)
{
	int textureenum = gltexturetypeenums[texturetype];
	int wrapmode = ((flags & TEXF_CLAMP) && gl_support_clamptoedge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;

	CHECKGLERROR

	if (gl_support_anisotropy && (flags & TEXF_MIPMAP))
	{
		int aniso = bound(1, gl_texture_anisotropy.integer, gl_max_anisotropy);
		if (gl_texture_anisotropy.integer != aniso)
			Cvar_SetValueQuick(&gl_texture_anisotropy, aniso);
		qglTexParameteri(textureenum, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);CHECKGLERROR
	}
	qglTexParameteri(textureenum, GL_TEXTURE_WRAP_S, wrapmode);CHECKGLERROR
	qglTexParameteri(textureenum, GL_TEXTURE_WRAP_T, wrapmode);CHECKGLERROR
	if (gltexturetypedimensions[texturetype] >= 3)
	{
		qglTexParameteri(textureenum, GL_TEXTURE_WRAP_R, wrapmode);CHECKGLERROR
	}

	CHECKGLERROR
	if (flags & TEXF_FORCENEAREST)
	{
		if (flags & TEXF_MIPMAP)
		{
			qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);CHECKGLERROR
		}
		else
		{
			qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, GL_NEAREST);CHECKGLERROR
		}
		qglTexParameteri(textureenum, GL_TEXTURE_MAG_FILTER, GL_NEAREST);CHECKGLERROR
	}
	else if (flags & TEXF_FORCELINEAR)
	{
		if (flags & TEXF_MIPMAP)
		{
			if (gl_filter_min == GL_NEAREST_MIPMAP_LINEAR || gl_filter_min == GL_LINEAR_MIPMAP_LINEAR)
			{
				qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);CHECKGLERROR
			}
			else
			{
				qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);CHECKGLERROR
			}
		}
		else
		{
			qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, GL_LINEAR);CHECKGLERROR
		}
		qglTexParameteri(textureenum, GL_TEXTURE_MAG_FILTER, GL_LINEAR);CHECKGLERROR
	}
	else
	{
		if (flags & TEXF_MIPMAP)
		{
			qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, gl_filter_min);CHECKGLERROR
		}
		else
		{
			qglTexParameteri(textureenum, GL_TEXTURE_MIN_FILTER, gl_filter_mag);CHECKGLERROR
		}
		qglTexParameteri(textureenum, GL_TEXTURE_MAG_FILTER, gl_filter_mag);CHECKGLERROR
	}

	CHECKGLERROR
}

static void R_Upload(gltexture_t *glt, unsigned char *data, int fragx, int fragy, int fragz, int fragwidth, int fragheight, int fragdepth)
{
	int i, mip, width, height, depth;
	GLint oldbindtexnum;
	unsigned char *prevbuffer;
	prevbuffer = data;

	CHECKGLERROR

	// we need to restore the texture binding after finishing the upload
	qglGetIntegerv(gltexturetypebindingenums[glt->texturetype], &oldbindtexnum);
	qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);
	CHECKGLERROR

	R_MakeResizeBufferBigger(fragwidth * fragheight * fragdepth * glt->sides * glt->bytesperpixel);

	if (prevbuffer == NULL)
	{
		memset(resizebuffer, 0, fragwidth * fragheight * fragdepth * glt->bytesperpixel);
		prevbuffer = resizebuffer;
	}
	else if (glt->textype->textype == TEXTYPE_PALETTE)
	{
		// promote paletted to RGBA, so we only have to worry about RGB and
		// RGBA in the rest of this code
		Image_Copy8bitRGBA(prevbuffer, colorconvertbuffer, fragwidth * fragheight * fragdepth * glt->sides, glt->palette);
		prevbuffer = colorconvertbuffer;
	}

	// these are rounded up versions of the size to do better resampling
	for (width  = 1;width  < glt->inputwidth ;width  <<= 1);
	for (height = 1;height < glt->inputheight;height <<= 1);
	for (depth  = 1;depth  < glt->inputdepth ;depth  <<= 1);

	R_MakeResizeBufferBigger(width * height * depth * glt->sides * glt->bytesperpixel);

	if ((glt->flags & (TEXF_MIPMAP | TEXF_PICMIP | GLTEXF_UPLOAD)) == 0)
	{
		// update a portion of the image
		switch(glt->texturetype)
		{
		case GLTEXTURETYPE_1D:
			qglTexSubImage1D(GL_TEXTURE_1D, 0, fragx, fragwidth, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		case GLTEXTURETYPE_2D:
			qglTexSubImage2D(GL_TEXTURE_2D, 0, fragx, fragy, fragwidth, fragheight, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		case GLTEXTURETYPE_3D:
			qglTexSubImage3D(GL_TEXTURE_3D, 0, fragx, fragy, fragz, fragwidth, fragheight, fragdepth, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		default:
			Host_Error("R_Upload: partial update of type other than 1D, 2D, or 3D");
			break;
		}
	}
	else
	{
		if (fragx || fragy || fragz || glt->inputwidth != fragwidth || glt->inputheight != fragheight || glt->inputdepth != fragdepth)
			Host_Error("R_Upload: partial update not allowed on initial upload or in combination with PICMIP or MIPMAP\n");

		// upload the image for the first time
		glt->flags &= ~GLTEXF_UPLOAD;

		// cubemaps contain multiple images and thus get processed a bit differently
		if (glt->texturetype != GLTEXTURETYPE_CUBEMAP)
		{
			if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
			{
				Image_Resample(prevbuffer, glt->inputwidth, glt->inputheight, glt->inputdepth, resizebuffer, width, height, depth, glt->bytesperpixel, r_lerpimages.integer);
				prevbuffer = resizebuffer;
			}
			// picmip/max_size
			while (width > glt->tilewidth || height > glt->tileheight || depth > glt->tiledepth)
			{
				Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, glt->tilewidth, glt->tileheight, glt->tiledepth, glt->bytesperpixel);
				prevbuffer = resizebuffer;
			}
		}
		mip = 0;
		switch(glt->texturetype)
		{
		case GLTEXTURETYPE_1D:
			qglTexImage1D(GL_TEXTURE_1D, mip++, glt->glinternalformat, width, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage1D(GL_TEXTURE_1D, mip++, glt->glinternalformat, width, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_2D:
			qglTexImage2D(GL_TEXTURE_2D, mip++, glt->glinternalformat, width, height, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage2D(GL_TEXTURE_2D, mip++, glt->glinternalformat, width, height, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_3D:
			qglTexImage3D(GL_TEXTURE_3D, mip++, glt->glinternalformat, width, height, depth, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage3D(GL_TEXTURE_3D, mip++, glt->glinternalformat, width, height, depth, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_CUBEMAP:
			// convert and upload each side in turn,
			// from a continuous block of input texels
			texturebuffer = prevbuffer;
			for (i = 0;i < 6;i++)
			{
				prevbuffer = texturebuffer;
				texturebuffer += glt->inputwidth * glt->inputheight * glt->inputdepth * glt->textype->inputbytesperpixel;
				if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
				{
					Image_Resample(prevbuffer, glt->inputwidth, glt->inputheight, glt->inputdepth, resizebuffer, width, height, depth, glt->bytesperpixel, r_lerpimages.integer);
					prevbuffer = resizebuffer;
				}
				// picmip/max_size
				while (width > glt->tilewidth || height > glt->tileheight || depth > glt->tiledepth)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, glt->tilewidth, glt->tileheight, glt->tiledepth, glt->bytesperpixel);
					prevbuffer = resizebuffer;
				}
				mip = 0;
				qglTexImage2D(cubemapside[i], mip++, glt->glinternalformat, width, height, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
				CHECKGLERROR
				if (glt->flags & TEXF_MIPMAP)
				{
					while (width > 1 || height > 1 || depth > 1)
					{
						Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->bytesperpixel);
						prevbuffer = resizebuffer;
						qglTexImage2D(cubemapside[i], mip++, glt->glinternalformat, width, height, 0, glt->glformat, GL_UNSIGNED_BYTE, prevbuffer);
						CHECKGLERROR
					}
				}
			}
			break;
		}
		GL_SetupTextureParameters(glt->flags, glt->texturetype);
	}
	qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);
}

static void R_UploadTexture (gltexture_t *glt)
{
	if (!(glt->flags & GLTEXF_UPLOAD))
		return;

	qglGenTextures(1, (GLuint *)&glt->texnum);
	R_Upload(glt, glt->inputtexels, 0, 0, 0, glt->inputwidth, glt->inputheight, glt->inputdepth);
	if (glt->inputtexels)
	{
		Mem_Free(glt->inputtexels);
		glt->inputtexels = NULL;
		glt->flags |= GLTEXF_DESTROYED;
	}
	else if (glt->flags & GLTEXF_DESTROYED)
		Con_Printf("R_UploadTexture: Texture %s already uploaded and destroyed.  Can not upload original image again.  Uploaded blank texture.\n", glt->identifier);
}

static rtexture_t *R_SetupTexture(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, int sides, int flags, int textype, int texturetype, const unsigned char *data, const unsigned int *palette)
{
	int i, size;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo;

	if (cls.state == ca_dedicated)
		return NULL;

	if (texturetype == GLTEXTURETYPE_CUBEMAP && !gl_texturecubemap)
	{
		Con_Printf ("R_LoadTexture: cubemap texture not supported by driver\n");
		return NULL;
	}
	if (texturetype == GLTEXTURETYPE_3D && !gl_texture3d)
	{
		Con_Printf ("R_LoadTexture: 3d texture not supported by driver\n");
		return NULL;
	}

	texinfo = R_GetTexTypeInfo(textype, flags);
	size = width * height * depth * sides * texinfo->inputbytesperpixel;
	if (size < 1)
	{
		Con_Printf ("R_LoadTexture: bogus texture size (%dx%dx%dx%dbppx%dsides = %d bytes)\n", width, height, depth, texinfo->inputbytesperpixel * 8, sides);
		return NULL;
	}

	// clear the alpha flag if the texture has no transparent pixels
	switch(textype)
	{
	case TEXTYPE_PALETTE:
		if (flags & TEXF_ALPHA)
		{
			flags &= ~TEXF_ALPHA;
			if (data)
			{
				for (i = 0;i < size;i++)
				{
					if (((unsigned char *)&palette[data[i]])[3] < 255)
					{
						flags |= TEXF_ALPHA;
						break;
					}
				}
			}
		}
		break;
	case TEXTYPE_RGB:
		if (flags & TEXF_ALPHA)
			Host_Error("R_LoadTexture: RGB has no alpha, don't specify TEXF_ALPHA");
		break;
	case TEXTYPE_RGBA:
		if (flags & TEXF_ALPHA)
		{
			flags &= ~TEXF_ALPHA;
			if (data)
			{
				for (i = 3;i < size;i += 4)
				{
					if (data[i] < 255)
					{
						flags |= TEXF_ALPHA;
						break;
					}
				}
			}
		}
		break;
	default:
		Host_Error("R_LoadTexture: unknown texture type");
	}

	glt = (gltexture_t *)Mem_Alloc(texturemempool, sizeof(gltexture_t));
	if (identifier)
		strlcpy (glt->identifier, identifier, sizeof(glt->identifier));
	glt->pool = pool;
	glt->chain = pool->gltchain;
	pool->gltchain = glt;
	glt->inputwidth = width;
	glt->inputheight = height;
	glt->inputdepth = depth;
	glt->flags = flags | GLTEXF_UPLOAD;
	glt->textype = texinfo;
	glt->texturetype = texturetype;
	glt->inputdatasize = size;
	glt->palette = palette;
	glt->glinternalformat = texinfo->glinternalformat;
	glt->glformat = texinfo->glformat;
	glt->bytesperpixel = texinfo->internalbytesperpixel;
	glt->sides = glt->texturetype == GLTEXTURETYPE_CUBEMAP ? 6 : 1;
	glt->texnum = -1;

	if (data)
	{
		glt->inputtexels = (unsigned char *)Mem_Alloc(texturemempool, size);
		memcpy(glt->inputtexels, data, size);
	}
	else
		glt->inputtexels = NULL;

	GL_Texture_CalcImageSize(glt->texturetype, glt->flags, glt->inputwidth, glt->inputheight, glt->inputdepth, &glt->tilewidth, &glt->tileheight, &glt->tiledepth);
	R_PrecacheTexture(glt);

	return (rtexture_t *)glt;
}

rtexture_t *R_LoadTexture1D(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, 1, 1, 1, flags, textype, GLTEXTURETYPE_1D, data, palette);
}

rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, flags, textype, GLTEXTURETYPE_2D, data, palette);
}

rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, depth, 1, flags, textype, GLTEXTURETYPE_3D, data, palette);
}

rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, width, 1, 6, flags, textype, GLTEXTURETYPE_CUBEMAP, data, palette);
}

int R_TextureHasAlpha(rtexture_t *rt)
{
	return rt ? (((gltexture_t *)rt)->flags & TEXF_ALPHA) != 0 : false;
}

int R_TextureWidth(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->inputwidth : 0;
}

int R_TextureHeight(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->inputheight : 0;
}

void R_UpdateTexture(rtexture_t *rt, unsigned char *data, int x, int y, int width, int height)
{
	gltexture_t *glt;
	if (rt == NULL)
		Host_Error("R_UpdateTexture: no texture supplied");
	if (data == NULL)
		Host_Error("R_UpdateTexture: no data supplied");
	glt = (gltexture_t *)rt;

	// we need it to be uploaded before we can update a part of it
	if (glt->flags & GLTEXF_UPLOAD)
		R_UploadTexture(glt);

	// update part of the texture
	R_Upload(glt, data, x, y, 0, width, height, 1);
}

