
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"
#include "image_png.h"
#include "intoverflow.h"

cvar_t gl_max_size = {CVAR_SAVE, "gl_max_size", "2048", "maximum allowed texture size, can be used to reduce video memory usage, limited by hardware capabilities (typically 2048, 4096, or 8192)"};
cvar_t gl_max_lightmapsize = {CVAR_SAVE, "gl_max_lightmapsize", "1024", "maximum allowed texture size for lightmap textures, use larger values to improve rendering speed, as long as there is enough video memory available (setting it too high for the hardware will cause very bad performance)"};
cvar_t gl_picmip = {CVAR_SAVE, "gl_picmip", "0", "reduces resolution of textures by powers of 2, for example 1 will halve width/height, reducing texture memory usage by 75%"};
cvar_t r_lerpimages = {CVAR_SAVE, "r_lerpimages", "1", "bilinear filters images when scaling them up to power of 2 size (mode 1), looks better than glquake (mode 0)"};
cvar_t gl_texture_anisotropy = {CVAR_SAVE, "gl_texture_anisotropy", "1", "anisotropic filtering quality (if supported by hardware), 1 sample (no anisotropy) and 8 sample (8 tap anisotropy) are recommended values"};
cvar_t gl_texturecompression = {CVAR_SAVE, "gl_texturecompression", "0", "whether to compress textures, a value of 0 disables compression (even if the individual cvars are 1), 1 enables fast (low quality) compression at startup, 2 enables slow (high quality) compression at startup"};
cvar_t gl_texturecompression_color = {CVAR_SAVE, "gl_texturecompression_color", "1", "whether to compress colormap (diffuse) textures"};
cvar_t gl_texturecompression_normal = {CVAR_SAVE, "gl_texturecompression_normal", "0", "whether to compress normalmap (normalmap) textures"};
cvar_t gl_texturecompression_gloss = {CVAR_SAVE, "gl_texturecompression_gloss", "1", "whether to compress glossmap (specular) textures"};
cvar_t gl_texturecompression_glow = {CVAR_SAVE, "gl_texturecompression_glow", "1", "whether to compress glowmap (luma) textures"};
cvar_t gl_texturecompression_2d = {CVAR_SAVE, "gl_texturecompression_2d", "0", "whether to compress 2d (hud/menu) textures other than the font"};
cvar_t gl_texturecompression_q3bsplightmaps = {CVAR_SAVE, "gl_texturecompression_q3bsplightmaps", "0", "whether to compress lightmaps in q3bsp format levels"};
cvar_t gl_texturecompression_q3bspdeluxemaps = {CVAR_SAVE, "gl_texturecompression_q3bspdeluxemaps", "0", "whether to compress deluxemaps in q3bsp format levels (only levels compiled with q3map2 -deluxe have these)"};
cvar_t gl_texturecompression_sky = {CVAR_SAVE, "gl_texturecompression_sky", "0", "whether to compress sky textures"};
cvar_t gl_texturecompression_lightcubemaps = {CVAR_SAVE, "gl_texturecompression_lightcubemaps", "1", "whether to compress light cubemaps (spotlights and other light projection images)"};
cvar_t gl_texturecompression_reflectmask = {CVAR_SAVE, "gl_texturecompression_reflectmask", "1", "whether to compress reflection cubemap masks (mask of which areas of the texture should reflect the generic shiny cubemap)"};
cvar_t gl_nopartialtextureupdates = {CVAR_SAVE, "gl_nopartialtextureupdates", "1", "use alternate path for dynamic lightmap updates that avoids a possibly slow code path in the driver"};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_mag = GL_LINEAR;


static mempool_t *texturemempool;
static memexpandablearray_t texturearray;

// note: this must not conflict with TEXF_ flags in r_textures.h
// bitmask for mismatch checking
#define GLTEXF_IMPORTANTBITS (0)
// dynamic texture (treat texnum == 0 differently)
#define GLTEXF_DYNAMIC		0x00080000

typedef struct textypeinfo_s
{
	textype_t textype;
	int inputbytesperpixel;
	int internalbytesperpixel;
	float glinternalbytesperpixel;
	int glinternalformat;
	int glformat;
	int gltype;
}
textypeinfo_t;


static textypeinfo_t textype_palette                = {TEXTYPE_PALETTE    , 1, 4, 4.0f, 3                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_palette_alpha          = {TEXTYPE_PALETTE    , 1, 4, 4.0f, 4                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba                   = {TEXTYPE_RGBA       , 4, 4, 4.0f, 3                               , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_alpha             = {TEXTYPE_RGBA       , 4, 4, 4.0f, 4                               , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_compress          = {TEXTYPE_RGBA       , 4, 4, 0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_alpha_compress    = {TEXTYPE_RGBA       , 4, 4, 1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra                   = {TEXTYPE_BGRA       , 4, 4, 4.0f, 3                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_alpha             = {TEXTYPE_BGRA       , 4, 4, 4.0f, 4                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_compress          = {TEXTYPE_BGRA       , 4, 4, 0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_alpha_compress    = {TEXTYPE_BGRA       , 4, 4, 1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_shadowmap16            = {TEXTYPE_SHADOWMAP  , 2, 2, 2.0f, GL_DEPTH_COMPONENT16_ARB        , GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
static textypeinfo_t textype_shadowmap24            = {TEXTYPE_SHADOWMAP  , 4, 4, 4.0f, GL_DEPTH_COMPONENT24_ARB        , GL_DEPTH_COMPONENT, GL_UNSIGNED_INT  };
static textypeinfo_t textype_alpha                  = {TEXTYPE_ALPHA      , 1, 4, 4.0f, GL_ALPHA                        , GL_ALPHA          , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_dxt1                   = {TEXTYPE_DXT1       , 4, 0, 0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT , 0                 , 0                };
static textypeinfo_t textype_dxt1a                  = {TEXTYPE_DXT1A      , 4, 0, 0.5f, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0                 , 0                };
static textypeinfo_t textype_dxt3                   = {TEXTYPE_DXT3       , 4, 0, 1.0f, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0                 , 0                };
static textypeinfo_t textype_dxt5                   = {TEXTYPE_DXT5       , 4, 0, 1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0                 , 0                };
static textypeinfo_t textype_colorbuffer            = {TEXTYPE_COLORBUFFER, 4, 4, 4.0f, 4                               , GL_BGRA           , GL_UNSIGNED_BYTE };


typedef enum gltexturetype_e
{
	GLTEXTURETYPE_2D,
	GLTEXTURETYPE_3D,
	GLTEXTURETYPE_CUBEMAP,
	GLTEXTURETYPE_RECTANGLE,
	GLTEXTURETYPE_TOTAL
}
gltexturetype_t;

static int gltexturetypeenums[GLTEXTURETYPE_TOTAL] = {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_RECTANGLE_ARB};
static int gltexturetypedimensions[GLTEXTURETYPE_TOTAL] = {2, 3, 2, 2};
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
	// this portion of the struct is exposed to the R_GetTexture macro for
	// speed reasons, must be identical in rtexture_t!
	int texnum; // GL texture slot number
	qboolean dirty; // indicates that R_RealGetTexture should be called
	int gltexturetypeenum; // used by R_Mesh_TexBind

	// dynamic texture stuff [11/22/2007 Black]
	updatecallback_t updatecallback;
	void *updatacallback_data;
	// --- [11/22/2007 Black]

	// stores backup copy of texture for deferred texture updates (gl_nopartialtextureupdates cvar)
	unsigned char *bufferpixels;
	qboolean buffermodified;

	// pointer to texturepool (check this to see if the texture is allocated)
	struct gltexturepool_s *pool;
	// pointer to next texture in texturepool chain
	struct gltexture_s *chain;
	// name of the texture (this might be removed someday), no duplicates
	char identifier[MAX_QPATH + 32];
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
	// actual stored texture size after gl_picmip and gl_max_size are applied
	// (power of 2 if vid.support.arb_texture_non_power_of_two is not supported)
	int tilewidth, tileheight, tiledepth;
	// 1 or 6 depending on texturetype
	int sides;
	// bytes per pixel
	int bytesperpixel;
	// GL_RGB or GL_RGBA or GL_DEPTH_COMPONENT
	int glformat;
	// 3 or 4
	int glinternalformat;
	// GL_UNSIGNED_BYTE or GL_UNSIGNED_INT or GL_UNSIGNED_SHORT or GL_FLOAT
	int gltype;
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
static const unsigned char *texturebuffer;

static textypeinfo_t *R_GetTexTypeInfo(textype_t textype, int flags)
{
	switch(textype)
	{
	case TEXTYPE_DXT1:
		return &textype_dxt1;
	case TEXTYPE_DXT1A:
		return &textype_dxt1a;
	case TEXTYPE_DXT3:
		return &textype_dxt3;
	case TEXTYPE_DXT5:
		return &textype_dxt5;
	case TEXTYPE_PALETTE:
		return (flags & TEXF_ALPHA) ? &textype_palette_alpha : &textype_palette;
	case TEXTYPE_RGBA:
		if ((flags & TEXF_COMPRESS) && gl_texturecompression.integer >= 1 && vid.support.arb_texture_compression)
			return (flags & TEXF_ALPHA) ? &textype_rgba_alpha_compress : &textype_rgba_compress;
		return (flags & TEXF_ALPHA) ? &textype_rgba_alpha : &textype_rgba;
	case TEXTYPE_BGRA:
		if ((flags & TEXF_COMPRESS) && gl_texturecompression.integer >= 1 && vid.support.arb_texture_compression)
			return (flags & TEXF_ALPHA) ? &textype_bgra_alpha_compress : &textype_bgra_compress;
		return (flags & TEXF_ALPHA) ? &textype_bgra_alpha : &textype_bgra;
	case TEXTYPE_ALPHA:
		return &textype_alpha;
	case TEXTYPE_SHADOWMAP:
		return (flags & TEXF_LOWPRECISION) ? &textype_shadowmap16 : &textype_shadowmap24;
	case TEXTYPE_COLORBUFFER:
		return &textype_colorbuffer;
	default:
		Host_Error("R_GetTexTypeInfo: unknown texture format");
		break;
	}
	return NULL;
}

// dynamic texture code [11/22/2007 Black]
void R_MarkDirtyTexture(rtexture_t *rt) {
	gltexture_t *glt = (gltexture_t*) rt;
	if( !glt ) {
		return;
	}

	// dont do anything if the texture is already dirty (and make sure this *is* a dynamic texture after all!)
	if (glt->flags & GLTEXF_DYNAMIC)
	{
		// mark it as dirty, so R_RealGetTexture gets called
		glt->dirty = true;
	}
}

void R_MakeTextureDynamic(rtexture_t *rt, updatecallback_t updatecallback, void *data) {
	gltexture_t *glt = (gltexture_t*) rt;
	if( !glt ) {
		return;
	}

	glt->flags |= GLTEXF_DYNAMIC;
	glt->updatecallback = updatecallback;
	glt->updatacallback_data = data;
}

static void R_UpdateDynamicTexture(gltexture_t *glt) {
	glt->dirty = false;
	if( glt->updatecallback ) {
		glt->updatecallback( (rtexture_t*) glt, glt->updatacallback_data );
	}
}

void R_PurgeTexture(rtexture_t *rt)
{
	if(rt && !(((gltexture_t*) rt)->flags & TEXF_PERSISTENT)) {
		R_FreeTexture(rt);
	}
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
	{
		CHECKGLERROR
		qglDeleteTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
	}

	if (glt->inputtexels)
		Mem_Free(glt->inputtexels);
	Mem_ExpandableArray_FreeRecord(&texturearray, glt);
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
	CHECKGLERROR
	GL_ActiveTexture(0);
	for (pool = gltexturepoolchain;pool;pool = pool->next)
	{
		for (glt = pool->gltchain;glt;glt = glt->chain)
		{
			// only update already uploaded images
			if (glt->texnum && !(glt->flags & (TEXF_FORCENEAREST | TEXF_FORCELINEAR)))
			{
				oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
				qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
				if (glt->flags & TEXF_MIPMAP)
				{
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_min);CHECKGLERROR
				}
				else
				{
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_mag);CHECKGLERROR
				}
				qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAG_FILTER, gl_filter_mag);CHECKGLERROR
				qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
			}
		}
	}
}

static void GL_Texture_CalcImageSize(int texturetype, int flags, int inwidth, int inheight, int indepth, int *outwidth, int *outheight, int *outdepth)
{
	int picmip = 0, maxsize = 0, width2 = 1, height2 = 1, depth2 = 1;

	switch (texturetype)
	{
	default:
	case GLTEXTURETYPE_2D:
		maxsize = vid.maxtexturesize_2d;
		if (flags & TEXF_PICMIP)
		{
			maxsize = bound(1, gl_max_size.integer, maxsize);
			picmip = gl_picmip.integer;
		}
		break;
	case GLTEXTURETYPE_3D:
		maxsize = vid.maxtexturesize_3d;
		break;
	case GLTEXTURETYPE_CUBEMAP:
		maxsize = vid.maxtexturesize_cubemap;
		break;
	}

	if (outwidth)
	{
		if (vid.support.arb_texture_non_power_of_two)
			width2 = min(inwidth >> picmip, maxsize);
		else
		{
			for (width2 = 1;width2 < inwidth;width2 <<= 1);
			for (width2 >>= picmip;width2 > maxsize;width2 >>= 1);
		}
		*outwidth = max(1, width2);
	}
	if (outheight)
	{
		if (vid.support.arb_texture_non_power_of_two)
			height2 = min(inheight >> picmip, maxsize);
		else
		{
			for (height2 = 1;height2 < inheight;height2 <<= 1);
			for (height2 >>= picmip;height2 > maxsize;height2 >>= 1);
		}
		*outheight = max(1, height2);
	}
	if (outdepth)
	{
		if (vid.support.arb_texture_non_power_of_two)
			depth2 = min(indepth >> picmip, maxsize);
		else
		{
			for (depth2 = 1;depth2 < indepth;depth2 <<= 1);
			for (depth2 >>= picmip;depth2 > maxsize;depth2 >>= 1);
		}
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

	return (int)(size * glt->textype->glinternalbytesperpixel) * glt->sides;
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
			isloaded = glt->texnum != 0;
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
			Con_Printf("texturepool %10p total: %i (%.3fMB, %.3fMB original), uploaded %i (%.3fMB, %.3fMB original), upload on demand %i (%.3fMB, %.3fMB original)\n", (void *)pool, pooltotal, pooltotalt / 1048576.0, pooltotalp / 1048576.0, poolloaded, poolloadedt / 1048576.0, poolloadedp / 1048576.0, pooltotal - poolloaded, (pooltotalt - poolloadedt) / 1048576.0, (pooltotalp - poolloadedp) / 1048576.0);
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
	CHECKGLERROR
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);CHECKGLERROR
	qglPixelStorei(GL_PACK_ALIGNMENT, 1);CHECKGLERROR

	texturemempool = Mem_AllocPool("texture management", 0, NULL);
	Mem_ExpandableArray_NewArray(&texturearray, texturemempool, sizeof(gltexture_t), 512);

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
	resizebuffer = NULL;
	colorconvertbuffer = NULL;
	texturebuffer = NULL;
	Mem_ExpandableArray_FreeArray(&texturearray);
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
	Cvar_RegisterVariable (&gl_max_lightmapsize);
	Cvar_RegisterVariable (&r_lerpimages);
	Cvar_RegisterVariable (&gl_texture_anisotropy);
	Cvar_RegisterVariable (&gl_texturecompression);
	Cvar_RegisterVariable (&gl_texturecompression_color);
	Cvar_RegisterVariable (&gl_texturecompression_normal);
	Cvar_RegisterVariable (&gl_texturecompression_gloss);
	Cvar_RegisterVariable (&gl_texturecompression_glow);
	Cvar_RegisterVariable (&gl_texturecompression_2d);
	Cvar_RegisterVariable (&gl_texturecompression_q3bsplightmaps);
	Cvar_RegisterVariable (&gl_texturecompression_q3bspdeluxemaps);
	Cvar_RegisterVariable (&gl_texturecompression_sky);
	Cvar_RegisterVariable (&gl_texturecompression_lightcubemaps);
	Cvar_RegisterVariable (&gl_texturecompression_reflectmask);
	Cvar_RegisterVariable (&gl_nopartialtextureupdates);

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

		old_aniso = bound(1, gl_texture_anisotropy.integer, (int)vid.max_anisotropy);

		Cvar_SetValueQuick(&gl_texture_anisotropy, old_aniso);

		CHECKGLERROR
		GL_ActiveTexture(0);
		for (pool = gltexturepoolchain;pool;pool = pool->next)
		{
			for (glt = pool->gltchain;glt;glt = glt->chain)
			{
				// only update already uploaded images
				if (glt->texnum && (glt->flags & TEXF_MIPMAP) == TEXF_MIPMAP)
				{
					oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);

					qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
					qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAX_ANISOTROPY_EXT, old_aniso);CHECKGLERROR

					qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
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

static void GL_SetupTextureParameters(int flags, textype_t textype, int texturetype)
{
	int textureenum = gltexturetypeenums[texturetype];
	int wrapmode = (flags & TEXF_CLAMP) ? GL_CLAMP_TO_EDGE : GL_REPEAT;

	CHECKGLERROR

	if (vid.support.ext_texture_filter_anisotropic && (flags & TEXF_MIPMAP))
	{
		int aniso = bound(1, gl_texture_anisotropy.integer, (int)vid.max_anisotropy);
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

	if (textype == TEXTYPE_SHADOWMAP)
	{
		if (vid.support.arb_shadow)
		{
			if (flags & TEXF_COMPARE)
			{
				qglTexParameteri(textureenum, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);CHECKGLERROR
			}
			else
			{
				qglTexParameteri(textureenum, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);CHECKGLERROR
			}
			qglTexParameteri(textureenum, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);CHECKGLERROR
		}
		qglTexParameteri(textureenum, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);CHECKGLERROR
	}

	CHECKGLERROR
}

static void R_Upload(gltexture_t *glt, const unsigned char *data, int fragx, int fragy, int fragz, int fragwidth, int fragheight, int fragdepth)
{
	int i, mip, width, height, depth;
	GLint oldbindtexnum;
	const unsigned char *prevbuffer;
	prevbuffer = data;

	CHECKGLERROR

	// we need to restore the texture binding after finishing the upload
	GL_ActiveTexture(0);
	oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
	qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR

	// these are rounded up versions of the size to do better resampling
	if (vid.support.arb_texture_non_power_of_two || glt->texturetype == GLTEXTURETYPE_RECTANGLE)
	{
		width = glt->inputwidth;
		height = glt->inputheight;
		depth = glt->inputdepth;
	}
	else
	{
		for (width  = 1;width  < glt->inputwidth ;width  <<= 1);
		for (height = 1;height < glt->inputheight;height <<= 1);
		for (depth  = 1;depth  < glt->inputdepth ;depth  <<= 1);
	}

	R_MakeResizeBufferBigger(width * height * depth * glt->sides * glt->bytesperpixel);
	R_MakeResizeBufferBigger(fragwidth * fragheight * fragdepth * glt->sides * glt->bytesperpixel);

	if (prevbuffer == NULL)
	{
		memset(resizebuffer, 0, fragwidth * fragheight * fragdepth * glt->bytesperpixel);
		prevbuffer = resizebuffer;
	}
	else if (glt->textype->textype == TEXTYPE_PALETTE)
	{
		// promote paletted to BGRA, so we only have to worry about BGRA in the rest of this code
		Image_Copy8bitBGRA(prevbuffer, colorconvertbuffer, fragwidth * fragheight * fragdepth * glt->sides, glt->palette);
		prevbuffer = colorconvertbuffer;
	}

	// upload the image - preferring to do only complete uploads (drivers do not really like partial updates)

	if ((glt->flags & (TEXF_MIPMAP | TEXF_PICMIP)) == 0 && glt->inputwidth == glt->tilewidth && glt->inputheight == glt->tileheight && glt->inputdepth == glt->tiledepth && (fragx != 0 || fragy != 0 || fragwidth != glt->tilewidth || fragheight != glt->tileheight))
	{
		// update a portion of the image
		switch(glt->texturetype)
		{
		case GLTEXTURETYPE_2D:
			qglTexSubImage2D(GL_TEXTURE_2D, 0, fragx, fragy, fragwidth, fragheight, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
			break;
		case GLTEXTURETYPE_3D:
			qglTexSubImage3D(GL_TEXTURE_3D, 0, fragx, fragy, fragz, fragwidth, fragheight, fragdepth, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
			break;
		default:
			Host_Error("R_Upload: partial update of type other than 2D");
			break;
		}
	}
	else
	{
		if (fragx || fragy || fragz || glt->inputwidth != fragwidth || glt->inputheight != fragheight || glt->inputdepth != fragdepth)
			Host_Error("R_Upload: partial update not allowed on initial upload or in combination with PICMIP or MIPMAP\n");

		// cubemaps contain multiple images and thus get processed a bit differently
		if (glt->texturetype != GLTEXTURETYPE_CUBEMAP)
		{
			if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
			{
				Image_Resample32(prevbuffer, glt->inputwidth, glt->inputheight, glt->inputdepth, resizebuffer, width, height, depth, r_lerpimages.integer);
				prevbuffer = resizebuffer;
			}
			// picmip/max_size
			while (width > glt->tilewidth || height > glt->tileheight || depth > glt->tiledepth)
			{
				Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, glt->tilewidth, glt->tileheight, glt->tiledepth);
				prevbuffer = resizebuffer;
			}
		}
		mip = 0;
		if (qglGetCompressedTexImageARB)
		{
			if (gl_texturecompression.integer >= 2)
				qglHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
			else
				qglHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_FASTEST);
			CHECKGLERROR
		}
		switch(glt->texturetype)
		{
		case GLTEXTURETYPE_2D:
			qglTexImage2D(GL_TEXTURE_2D, mip++, glt->glinternalformat, width, height, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
					prevbuffer = resizebuffer;
					qglTexImage2D(GL_TEXTURE_2D, mip++, glt->glinternalformat, width, height, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_3D:
			qglTexImage3D(GL_TEXTURE_3D, mip++, glt->glinternalformat, width, height, depth, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
					prevbuffer = resizebuffer;
					qglTexImage3D(GL_TEXTURE_3D, mip++, glt->glinternalformat, width, height, depth, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_CUBEMAP:
			// convert and upload each side in turn,
			// from a continuous block of input texels
			texturebuffer = (unsigned char *)prevbuffer;
			for (i = 0;i < 6;i++)
			{
				prevbuffer = texturebuffer;
				texturebuffer += glt->inputwidth * glt->inputheight * glt->inputdepth * glt->textype->inputbytesperpixel;
				if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
				{
					Image_Resample32(prevbuffer, glt->inputwidth, glt->inputheight, glt->inputdepth, resizebuffer, width, height, depth, r_lerpimages.integer);
					prevbuffer = resizebuffer;
				}
				// picmip/max_size
				while (width > glt->tilewidth || height > glt->tileheight || depth > glt->tiledepth)
				{
					Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, glt->tilewidth, glt->tileheight, glt->tiledepth);
					prevbuffer = resizebuffer;
				}
				mip = 0;
				qglTexImage2D(cubemapside[i], mip++, glt->glinternalformat, width, height, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
				if (glt->flags & TEXF_MIPMAP)
				{
					while (width > 1 || height > 1 || depth > 1)
					{
						Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
						prevbuffer = resizebuffer;
						qglTexImage2D(cubemapside[i], mip++, glt->glinternalformat, width, height, 0, glt->glformat, glt->gltype, prevbuffer);CHECKGLERROR
					}
				}
			}
			break;
		case GLTEXTURETYPE_RECTANGLE:
			qglTexImage2D(GL_TEXTURE_RECTANGLE_ARB, mip++, glt->glinternalformat, width, height, 0, glt->glformat, glt->gltype, NULL);CHECKGLERROR
			break;
		}
		GL_SetupTextureParameters(glt->flags, glt->textype->textype, glt->texturetype);
	}
	qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
}

static rtexture_t *R_SetupTexture(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, int sides, int flags, textype_t textype, int texturetype, const unsigned char *data, const unsigned int *palette)
{
	int i, size;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo, *texinfo2;

	if (cls.state == ca_dedicated)
		return NULL;

	if (texturetype == GLTEXTURETYPE_RECTANGLE && !vid.support.arb_texture_rectangle)
	{
		Con_Printf ("R_LoadTexture: rectangle texture not supported by driver\n");
		return NULL;
	}
	if (texturetype == GLTEXTURETYPE_CUBEMAP && !vid.support.arb_texture_cube_map)
	{
		Con_Printf ("R_LoadTexture: cubemap texture not supported by driver\n");
		return NULL;
	}
	if (texturetype == GLTEXTURETYPE_3D && !vid.support.ext_texture_3d)
	{
		Con_Printf ("R_LoadTexture: 3d texture not supported by driver\n");
		return NULL;
	}

	texinfo = R_GetTexTypeInfo(textype, flags);
	size = width * height * depth * sides * texinfo->inputbytesperpixel;
	if (size < 1)
	{
		Con_Printf ("R_LoadTexture: bogus texture size (%dx%dx%dx%dbppx%dsides = %d bytes)\n", width, height, depth, texinfo->inputbytesperpixel * 8, sides, size);
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
	case TEXTYPE_RGBA:
	case TEXTYPE_BGRA:
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
	case TEXTYPE_SHADOWMAP:
		break;
	case TEXTYPE_DXT1:
		break;
	case TEXTYPE_DXT1A:
	case TEXTYPE_DXT3:
	case TEXTYPE_DXT5:
		flags |= TEXF_ALPHA;
		break;
	case TEXTYPE_ALPHA:
		flags |= TEXF_ALPHA;
		break;
	case TEXTYPE_COLORBUFFER:
		flags |= TEXF_ALPHA;
		break;
	default:
		Host_Error("R_LoadTexture: unknown texture type");
	}

	texinfo2 = R_GetTexTypeInfo(textype, flags);
	if(size == width * height * depth * sides * texinfo->inputbytesperpixel)
		texinfo = texinfo2;
	else
		Con_Printf ("R_LoadTexture: input size changed after alpha fallback\n");

	glt = (gltexture_t *)Mem_ExpandableArray_AllocRecord(&texturearray);
	if (identifier)
		strlcpy (glt->identifier, identifier, sizeof(glt->identifier));
	glt->pool = pool;
	glt->chain = pool->gltchain;
	pool->gltchain = glt;
	glt->inputwidth = width;
	glt->inputheight = height;
	glt->inputdepth = depth;
	glt->flags = flags;
	glt->textype = texinfo;
	glt->texturetype = texturetype;
	glt->inputdatasize = size;
	glt->palette = palette;
	glt->glinternalformat = texinfo->glinternalformat;
	glt->glformat = texinfo->glformat;
	glt->gltype = texinfo->gltype;
	glt->bytesperpixel = texinfo->internalbytesperpixel;
	glt->sides = glt->texturetype == GLTEXTURETYPE_CUBEMAP ? 6 : 1;
	glt->texnum = 0;
	glt->dirty = false;
	glt->gltexturetypeenum = gltexturetypeenums[glt->texturetype];
	// init the dynamic texture attributes, too [11/22/2007 Black]
	glt->updatecallback = NULL;
	glt->updatacallback_data = NULL;

	GL_Texture_CalcImageSize(glt->texturetype, glt->flags, glt->inputwidth, glt->inputheight, glt->inputdepth, &glt->tilewidth, &glt->tileheight, &glt->tiledepth);

	// upload the texture
	// data may be NULL (blank texture for dynamic rendering)
	CHECKGLERROR
	qglGenTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
	R_Upload(glt, data, 0, 0, 0, glt->inputwidth, glt->inputheight, glt->inputdepth);
	if ((glt->flags & TEXF_ALLOWUPDATES) && gl_nopartialtextureupdates.integer)
		glt->bufferpixels = Mem_Alloc(texturemempool, glt->tilewidth*glt->tileheight*glt->tiledepth*glt->sides*glt->bytesperpixel);

	// texture converting and uploading can take a while, so make sure we're sending keepalives
	CL_KeepaliveMessage(false);

	return (rtexture_t *)glt;
}

rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, flags, textype, GLTEXTURETYPE_2D, data, palette);
}

rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, depth, 1, flags, textype, GLTEXTURETYPE_3D, data, palette);
}

rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, width, 1, 6, flags, textype, GLTEXTURETYPE_CUBEMAP, data, palette);
}

rtexture_t *R_LoadTextureRectangle(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, flags, textype, GLTEXTURETYPE_RECTANGLE, data, palette);
}

static int R_ShadowMapTextureFlags(int precision, qboolean filter)
{
	int flags = TEXF_CLAMP;
	if (filter)
		flags |= TEXF_FORCELINEAR | TEXF_COMPARE;
	else
		flags |= TEXF_FORCENEAREST;
	if (precision <= 16)
		flags |= TEXF_LOWPRECISION;
	return flags;
}

rtexture_t *R_LoadTextureShadowMapRectangle(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, R_ShadowMapTextureFlags(precision, filter), TEXTYPE_SHADOWMAP, GLTEXTURETYPE_RECTANGLE, NULL, NULL);
}

rtexture_t *R_LoadTextureShadowMap2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, R_ShadowMapTextureFlags(precision, filter), TEXTYPE_SHADOWMAP, GLTEXTURETYPE_2D, NULL, NULL);
}

rtexture_t *R_LoadTextureShadowMapCube(rtexturepool_t *rtexturepool, const char *identifier, int width, int precision, qboolean filter)
{
    return R_SetupTexture(rtexturepool, identifier, width, width, 1, 6, R_ShadowMapTextureFlags(precision, filter), TEXTYPE_SHADOWMAP, GLTEXTURETYPE_CUBEMAP, NULL, NULL);
}

int R_SaveTextureDDSFile(rtexture_t *rt, const char *filename, qboolean skipuncompressed)
{
	gltexture_t *glt = (gltexture_t *)rt;
	unsigned char *dds;
	int oldbindtexnum;
	int bytesperpixel = 0;
	int bytesperblock = 0;
	int dds_flags;
	int dds_format_flags;
	int dds_caps1;
	int dds_caps2;
	int ret;
	int mip;
	int mipmaps;
	int mipinfo[16][4];
	int ddssize = 128;
	GLint internalformat;
	const char *ddsfourcc;
	if (!rt)
		return -1; // NULL pointer
	if (!strcmp(gl_version, "2.0.5885 WinXP Release"))
		return -2; // broken driver - crashes on reading internal format
	if (!qglGetTexLevelParameteriv)
		return -2;
	GL_ActiveTexture(0);
	oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
	qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
	qglGetTexLevelParameteriv(gltexturetypeenums[glt->texturetype], 0, GL_TEXTURE_INTERNAL_FORMAT, &internalformat);
	switch(internalformat)
	{
	default: ddsfourcc = NULL;bytesperpixel = 4;break;
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: ddsfourcc = "DXT1";bytesperblock = 8;break;
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: ddsfourcc = "DXT3";bytesperblock = 16;break;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: ddsfourcc = "DXT5";bytesperblock = 16;break;
	}
	if (!bytesperblock && skipuncompressed)
		return -3; // skipped
	memset(mipinfo, 0, sizeof(mipinfo));
	mipinfo[0][0] = glt->tilewidth;
	mipinfo[0][1] = glt->tileheight;
	mipmaps = 1;
	if (glt->flags & TEXF_MIPMAP)
	{
		for (mip = 1;mip < 16;mip++)
		{
			mipinfo[mip][0] = mipinfo[mip-1][0] > 1 ? mipinfo[mip-1][0] >> 1 : 1;
			mipinfo[mip][1] = mipinfo[mip-1][1] > 1 ? mipinfo[mip-1][1] >> 1 : 1;
			if (mipinfo[mip][0] == 1 && mipinfo[mip][1] == 1)
			{
				mip++;
				break;
			}
		}
		mipmaps = mip;
	}
	for (mip = 0;mip < mipmaps;mip++)
	{
		mipinfo[mip][2] = bytesperblock ? ((mipinfo[mip][0]+3)/4)*((mipinfo[mip][1]+3)/4)*bytesperblock : mipinfo[mip][0]*mipinfo[mip][1]*bytesperpixel;
		mipinfo[mip][3] = ddssize;
		ddssize += mipinfo[mip][2];
	}
	dds = Mem_Alloc(tempmempool, ddssize);
	if (!dds)
		return -4;
	dds_caps1 = 0x1000; // DDSCAPS_TEXTURE
	dds_caps2 = 0;
	if (bytesperblock)
	{
		dds_flags = 0x81007; // DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_LINEARSIZE
		dds_format_flags = 0x4; // DDPF_FOURCC
	}
	else
	{
		dds_flags = 0x100F; // DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH
		dds_format_flags = 0x41; // DDPF_RGB | DDPF_ALPHAPIXELS
	}
	if (mipmaps)
	{
		dds_flags |= 0x20000; // DDSD_MIPMAPCOUNT
		dds_caps1 |= 0x400008; // DDSCAPS_MIPMAP | DDSCAPS_COMPLEX
	}
	memcpy(dds, "DDS ", 4);
	StoreLittleLong(dds+4, ddssize);
	StoreLittleLong(dds+8, dds_flags);
	StoreLittleLong(dds+12, mipinfo[0][1]); // height
	StoreLittleLong(dds+16, mipinfo[0][0]); // width
	StoreLittleLong(dds+24, 1); // depth
	StoreLittleLong(dds+28, mipmaps); // mipmaps
	StoreLittleLong(dds+76, 32); // format size
	StoreLittleLong(dds+80, dds_format_flags);
	StoreLittleLong(dds+108, dds_caps1);
	StoreLittleLong(dds+112, dds_caps2);
	if (bytesperblock)
	{
		StoreLittleLong(dds+20, mipinfo[0][2]); // linear size
		memcpy(dds+84, ddsfourcc, 4);
		for (mip = 0;mip < mipmaps;mip++)
		{
			qglGetCompressedTexImageARB(gltexturetypeenums[glt->texturetype], mip, dds + mipinfo[mip][3]);CHECKGLERROR
		}
	}
	else
	{
		StoreLittleLong(dds+20, mipinfo[0][0]*bytesperpixel); // pitch
		StoreLittleLong(dds+88, bytesperpixel*8); // bits per pixel
		dds[94] = dds[97] = dds[100] = dds[107] = 255; // bgra byte order masks
		for (mip = 0;mip < mipmaps;mip++)
		{
			qglGetTexImage(gltexturetypeenums[glt->texturetype], mip, GL_BGRA, GL_UNSIGNED_BYTE, dds + mipinfo[mip][3]);CHECKGLERROR
		}
	}
	qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
	ret = FS_WriteFile(filename, dds, ddssize);
	Mem_Free(dds);
	return ret ? ddssize : -5;
}

rtexture_t *R_LoadTextureDDSFile(rtexturepool_t *rtexturepool, const char *filename, int flags, qboolean *hasalphaflag, float *avgcolor)
{
	int i, size, dds_format_flags, dds_miplevels, dds_width, dds_height;
	//int dds_flags;
	textype_t textype;
	int bytesperblock, bytesperpixel;
	int mipcomplete;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo;
	int mip, mipwidth, mipheight, mipsize;
	unsigned int c;
	GLint oldbindtexnum;
	const unsigned char *mippixels, *ddspixels;
	unsigned char *dds;
	fs_offset_t ddsfilesize;
	unsigned int ddssize;

	if (cls.state == ca_dedicated)
		return NULL;

	dds = FS_LoadFile(filename, tempmempool, true, &ddsfilesize);
	ddssize = ddsfilesize;

	if (!dds)
	{
		Log_Printf("ddstexturefailures.log", "%s\n", filename);
		return NULL; // not found
	}

	if (ddsfilesize <= 128 || memcmp(dds, "DDS ", 4) || ddssize < (unsigned int)BuffLittleLong(dds+4) || BuffLittleLong(dds+76) != 32)
	{
		Mem_Free(dds);
		Con_Printf("^1%s: not a DDS image\n", filename);
		return NULL;
	}

	//dds_flags = BuffLittleLong(dds+8);
	dds_format_flags = BuffLittleLong(dds+80);
	dds_miplevels = (BuffLittleLong(dds+108) & 0x400000) ? BuffLittleLong(dds+28) : 1;
	dds_width = BuffLittleLong(dds+16);
	dds_height = BuffLittleLong(dds+12);
	ddspixels = dds + 128;

	flags &= ~TEXF_ALPHA;
	if ((dds_format_flags & 0x40) && BuffLittleLong(dds+88) == 32)
	{
		// very sloppy BGRA 32bit identification
		textype = TEXTYPE_BGRA;
		bytesperblock = 0;
		bytesperpixel = 4;
		size = INTOVERFLOW_MUL(INTOVERFLOW_MUL(dds_width, dds_height), bytesperpixel);
		if(INTOVERFLOW_ADD(128, size) > INTOVERFLOW_NORMALIZE(ddsfilesize))
		{
			Mem_Free(dds);
			Con_Printf("^1%s: invalid BGRA DDS image\n", filename);
			return NULL;
		}
		// check alpha
		for (i = 3;i < size;i += 4)
			if (ddspixels[i] < 255)
				break;
		if (i >= size)
			flags &= ~TEXF_ALPHA;
	}
	else if (!memcmp(dds+84, "DXT1", 4))
	{
		// we need to find out if this is DXT1 (opaque) or DXT1A (transparent)
		// LordHavoc: it is my belief that this does not infringe on the
		// patent because it is not decoding pixels...
		textype = TEXTYPE_DXT1;
		bytesperblock = 8;
		bytesperpixel = 0;
		//size = ((dds_width+3)/4)*((dds_height+3)/4)*bytesperblock;
		size = INTOVERFLOW_MUL(INTOVERFLOW_MUL(INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_width, 3), 4), INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_height, 3), 4)), bytesperblock);
		if(INTOVERFLOW_ADD(128, size) > INTOVERFLOW_NORMALIZE(ddsfilesize))
		{
			Mem_Free(dds);
			Con_Printf("^1%s: invalid DXT1 DDS image\n", filename);
			return NULL;
		}
		for (i = 0;i < size;i += bytesperblock)
			if (ddspixels[i+0] + ddspixels[i+1] * 256 <= ddspixels[i+2] + ddspixels[i+3] * 256)
				break;
		if (i < size)
			textype = TEXTYPE_DXT1A;
		else
			flags &= ~TEXF_ALPHA;
	}
	else if (!memcmp(dds+84, "DXT3", 4))
	{
		textype = TEXTYPE_DXT3;
		bytesperblock = 16;
		bytesperpixel = 0;
		size = INTOVERFLOW_MUL(INTOVERFLOW_MUL(INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_width, 3), 4), INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_height, 3), 4)), bytesperblock);
		if(INTOVERFLOW_ADD(128, size) > INTOVERFLOW_NORMALIZE(ddsfilesize))
		{
			Mem_Free(dds);
			Con_Printf("^1%s: invalid DXT3 DDS image\n", filename);
			return NULL;
		}
	}
	else if (!memcmp(dds+84, "DXT5", 4))
	{
		textype = TEXTYPE_DXT5;
		bytesperblock = 16;
		bytesperpixel = 0;
		size = INTOVERFLOW_MUL(INTOVERFLOW_MUL(INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_width, 3), 4), INTOVERFLOW_DIV(INTOVERFLOW_ADD(dds_height, 3), 4)), bytesperblock);
		if(INTOVERFLOW_ADD(128, size) > INTOVERFLOW_NORMALIZE(ddsfilesize))
		{
			Mem_Free(dds);
			Con_Printf("^1%s: invalid DXT5 DDS image\n", filename);
			return NULL;
		}
	}
	else
	{
		Mem_Free(dds);
		Con_Printf("^1%s: unrecognized/unsupported DDS format\n", filename);
		return NULL;
	}

	// return whether this texture is transparent
	if (hasalphaflag)
		*hasalphaflag = (flags & TEXF_ALPHA) != 0;

	// calculate average color if requested
	if (avgcolor)
	{
		float f;
		Vector4Clear(avgcolor);
		if (bytesperblock)
		{
			for (i = bytesperblock == 16 ? 8 : 0;i < size;i += bytesperblock)
			{
				c = ddspixels[i] + 256*ddspixels[i+1] + 65536*ddspixels[i+2] + 16777216*ddspixels[i+3];
				avgcolor[0] += ((c >> 11) & 0x1F) + ((c >> 27) & 0x1F);
				avgcolor[1] += ((c >>  5) & 0x3F) + ((c >> 21) & 0x3F);
				avgcolor[2] += ((c      ) & 0x1F) + ((c >> 16) & 0x1F);
			}
			f = (float)bytesperblock / size;
			avgcolor[0] *= (0.5f / 31.0f) * f;
			avgcolor[1] *= (0.5f / 63.0f) * f;
			avgcolor[2] *= (0.5f / 31.0f) * f;
			avgcolor[3] = 1; // too hard to calculate
		}
		else
		{
			for (i = 0;i < size;i += 4)
			{
				avgcolor[0] += ddspixels[i+2];
				avgcolor[1] += ddspixels[i+1];
				avgcolor[2] += ddspixels[i];
				avgcolor[3] += ddspixels[i+3];
			}
			f = (1.0f / 255.0f) * bytesperpixel / size;
			avgcolor[0] *= f;
			avgcolor[1] *= f;
			avgcolor[2] *= f;
			avgcolor[3] *= f;
		}
	}

	if (dds_miplevels > 1)
		flags |= TEXF_MIPMAP;
	else
		flags &= ~TEXF_MIPMAP;

	// if S3TC is not supported, there's very little we can do about it
	if (bytesperblock && !vid.support.ext_texture_compression_s3tc)
	{
		Mem_Free(dds);
		Con_Printf("^1%s: DDS file is compressed but OpenGL driver does not support S3TC\n", filename);
		return NULL;
	}

	texinfo = R_GetTexTypeInfo(textype, flags);

	glt = (gltexture_t *)Mem_ExpandableArray_AllocRecord(&texturearray);
	strlcpy (glt->identifier, filename, sizeof(glt->identifier));
	glt->pool = pool;
	glt->chain = pool->gltchain;
	pool->gltchain = glt;
	glt->inputwidth = dds_width;
	glt->inputheight = dds_height;
	glt->inputdepth = 1;
	glt->flags = flags;
	glt->textype = texinfo;
	glt->texturetype = GLTEXTURETYPE_2D;
	glt->inputdatasize = ddssize;
	glt->glinternalformat = texinfo->glinternalformat;
	glt->glformat = texinfo->glformat;
	glt->gltype = texinfo->gltype;
	glt->bytesperpixel = texinfo->internalbytesperpixel;
	glt->sides = 1;
	glt->gltexturetypeenum = gltexturetypeenums[glt->texturetype];
	glt->tilewidth = dds_width;
	glt->tileheight = dds_height;
	glt->tiledepth = 1;

	// texture uploading can take a while, so make sure we're sending keepalives
	CL_KeepaliveMessage(false);

	// upload the texture
	// we need to restore the texture binding after finishing the upload
	CHECKGLERROR
	GL_ActiveTexture(0);
	oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
	qglGenTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
	qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
	mippixels = ddspixels;
	mipwidth = dds_width;
	mipheight = dds_height;
	mipcomplete = false;
	for (mip = 0;mip < dds_miplevels+1;mip++)
	{
		mipsize = bytesperblock ? ((mipwidth+3)/4)*((mipheight+3)/4)*bytesperblock : mipwidth*mipheight*bytesperpixel;
		if (mippixels + mipsize > dds + ddssize)
			break;
		if (bytesperblock)
		{
			qglCompressedTexImage2DARB(GL_TEXTURE_2D, mip, glt->glinternalformat, mipwidth, mipheight, 0, mipsize, mippixels);CHECKGLERROR
		}
		else
		{
			qglTexImage2D(GL_TEXTURE_2D, mip, glt->glinternalformat, mipwidth, mipheight, 0, glt->glformat, glt->gltype, mippixels);CHECKGLERROR
		}
		mippixels += mipsize;
		if (mipwidth <= 1 && mipheight <= 1)
		{
			mipcomplete = true;
			break;
		}
		if (mipwidth > 1)
			mipwidth >>= 1;
		if (mipheight > 1)
			mipheight >>= 1;
	}
	if (dds_miplevels > 1 && !mipcomplete)
	{
		// need to set GL_TEXTURE_MAX_LEVEL
		qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAX_LEVEL, dds_miplevels - 1);CHECKGLERROR
	}
	GL_SetupTextureParameters(glt->flags, glt->textype->textype, glt->texturetype);
	qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR

	Mem_Free(dds);
	return (rtexture_t *)glt;
}

int R_TextureWidth(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->inputwidth : 0;
}

int R_TextureHeight(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->inputheight : 0;
}

void R_UpdateTexture(rtexture_t *rt, const unsigned char *data, int x, int y, int width, int height)
{
	gltexture_t *glt = (gltexture_t *)rt;
	if (data == NULL)
		Host_Error("R_UpdateTexture: no data supplied");
	if (glt == NULL)
		Host_Error("R_UpdateTexture: no texture supplied");
	if (!glt->texnum)
		Host_Error("R_UpdateTexture: texture has not been uploaded yet");
	// update part of the texture
	if (glt->bufferpixels)
	{
		int j;
		int bpp = glt->bytesperpixel;
		int inputskip = width*bpp;
		int outputskip = glt->tilewidth*bpp;
		const unsigned char *input = data;
		unsigned char *output = glt->bufferpixels;
		if (x < 0)
		{
			width += x;
			input -= x*bpp;
			x = 0;
		}
		if (y < 0)
		{
			height += y;
			input -= y*inputskip;
			y = 0;
		}
		if (width > glt->tilewidth - x)
			width = glt->tilewidth - x;
		if (height > glt->tileheight - y)
			height = glt->tileheight - y;
		if (width < 1 || height < 1)
			return;
		glt->dirty = true;
		glt->buffermodified = true;
		output += y*outputskip + x*bpp;
		for (j = 0;j < height;j++, output += outputskip, input += inputskip)
			memcpy(output, input, width*bpp);
	}
	else
		R_Upload(glt, data, x, y, 0, width, height, 1);
}

int R_RealGetTexture(rtexture_t *rt)
{
	if (rt)
	{
		gltexture_t *glt;
		glt = (gltexture_t *)rt;
		if (glt->flags & GLTEXF_DYNAMIC)
			R_UpdateDynamicTexture(glt);
		if (glt->buffermodified && glt->bufferpixels)
		{
			glt->buffermodified = false;
			R_Upload(glt, glt->bufferpixels, 0, 0, 0, glt->tilewidth, glt->tileheight, glt->tiledepth);
		}
		glt->dirty = false;
		return glt->texnum;
	}
	else
		return 0;
}

void R_ClearTexture (rtexture_t *rt)
{
	gltexture_t *glt = (gltexture_t *)rt;

	R_Upload( glt, NULL, 0, 0, 0, glt->tilewidth, glt->tileheight, glt->tiledepth );
}
