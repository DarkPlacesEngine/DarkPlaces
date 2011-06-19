
#include "quakedef.h"
#ifdef SUPPORTD3D
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 vid_d3d9dev;
#endif
#include "image.h"
#include "jpeg.h"
#include "image_png.h"
#include "intoverflow.h"
#include "dpsoftrast.h"

cvar_t gl_max_size = {CVAR_SAVE, "gl_max_size", "2048", "maximum allowed texture size, can be used to reduce video memory usage, limited by hardware capabilities (typically 2048, 4096, or 8192)"};
cvar_t gl_max_lightmapsize = {CVAR_SAVE, "gl_max_lightmapsize", "1024", "maximum allowed texture size for lightmap textures, use larger values to improve rendering speed, as long as there is enough video memory available (setting it too high for the hardware will cause very bad performance)"};
cvar_t gl_picmip = {CVAR_SAVE, "gl_picmip", "0", "reduces resolution of textures by powers of 2, for example 1 will halve width/height, reducing texture memory usage by 75%"};
cvar_t gl_picmip_world = {CVAR_SAVE, "gl_picmip_world", "0", "extra picmip level for world textures (may be negative, which will then reduce gl_picmip for these)"};
cvar_t r_picmipworld = {CVAR_SAVE, "r_picmipworld", "1", "whether gl_picmip shall apply to world textures too (setting this to 0 is a shorthand for gl_picmip_world -9999999)"};
cvar_t gl_picmip_sprites = {CVAR_SAVE, "gl_picmip_sprites", "0", "extra picmip level for sprite textures (may be negative, which will then reduce gl_picmip for these)"};
cvar_t r_picmipsprites = {CVAR_SAVE, "r_picmipsprites", "1", "make gl_picmip affect sprites too (saves some graphics memory in sprite heavy games) (setting this to 0 is a shorthand for gl_picmip_sprites -9999999)"};
cvar_t gl_picmip_other = {CVAR_SAVE, "gl_picmip_other", "0", "extra picmip level for other textures (may be negative, which will then reduce gl_picmip for these)"};
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
cvar_t gl_texturecompression_sprites = {CVAR_SAVE, "gl_texturecompression_sprites", "1", "whether to compress sprites"};
cvar_t gl_nopartialtextureupdates = {CVAR_SAVE, "gl_nopartialtextureupdates", "0", "use alternate path for dynamic lightmap updates that avoids a possibly slow code path in the driver"};
cvar_t r_texture_dds_load_alphamode = {0, "r_texture_dds_load_alphamode", "1", "0: trust DDPF_ALPHAPIXELS flag, 1: texture format and brute force search if ambiguous, 2: texture format only"};
cvar_t r_texture_dds_load_logfailure = {0, "r_texture_dds_load_logfailure", "0", "log missing DDS textures to ddstexturefailures.log"};
cvar_t r_texture_dds_swdecode = {0, "r_texture_dds_swdecode", "0", "0: don't software decode DDS, 1: software decode DDS if unsupported, 2: always software decode DDS"};

qboolean	gl_filter_force = false;
int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_mag = GL_LINEAR;
DPSOFTRAST_TEXTURE_FILTER dpsoftrast_filter_mipmap = DPSOFTRAST_TEXTURE_FILTER_LINEAR_MIPMAP_TRIANGLE;
DPSOFTRAST_TEXTURE_FILTER dpsoftrast_filter_nomipmap = DPSOFTRAST_TEXTURE_FILTER_LINEAR;

#ifdef SUPPORTD3D
int d3d_filter_flatmin = D3DTEXF_LINEAR;
int d3d_filter_flatmag = D3DTEXF_LINEAR;
int d3d_filter_flatmix = D3DTEXF_POINT;
int d3d_filter_mipmin = D3DTEXF_LINEAR;
int d3d_filter_mipmag = D3DTEXF_LINEAR;
int d3d_filter_mipmix = D3DTEXF_LINEAR;
int d3d_filter_nomip = false;
#endif


static mempool_t *texturemempool;
static memexpandablearray_t texturearray;

// note: this must not conflict with TEXF_ flags in r_textures.h
// bitmask for mismatch checking
#define GLTEXF_IMPORTANTBITS (0)
// dynamic texture (treat texnum == 0 differently)
#define GLTEXF_DYNAMIC		0x00080000

typedef struct textypeinfo_s
{
	const char *name;
	textype_t textype;
	int inputbytesperpixel;
	int internalbytesperpixel;
	float glinternalbytesperpixel;
	int glinternalformat;
	int glformat;
	int gltype;
}
textypeinfo_t;

// framebuffer texture formats
static textypeinfo_t textype_shadowmap16                 = {"shadowmap16",              TEXTYPE_SHADOWMAP     ,  2,  2,  2.0f, GL_DEPTH_COMPONENT16_ARB              , GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
static textypeinfo_t textype_shadowmap24                 = {"shadowmap24",              TEXTYPE_SHADOWMAP     ,  4,  4,  4.0f, GL_DEPTH_COMPONENT24_ARB              , GL_DEPTH_COMPONENT, GL_UNSIGNED_INT  };
static textypeinfo_t textype_colorbuffer                 = {"colorbuffer",              TEXTYPE_COLORBUFFER   ,  4,  4,  4.0f, GL_RGBA                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_colorbuffer16f              = {"colorbuffer16f",           TEXTYPE_COLORBUFFER16F,  8,  8,  8.0f, GL_RGBA16F_ARB                        , GL_RGBA           , GL_FLOAT         };
static textypeinfo_t textype_colorbuffer32f              = {"colorbuffer32f",           TEXTYPE_COLORBUFFER32F, 16, 16, 16.0f, GL_RGBA32F_ARB                        , GL_RGBA           , GL_FLOAT         };

// image formats:
static textypeinfo_t textype_alpha                       = {"alpha",                    TEXTYPE_ALPHA         ,  1,  4,  4.0f, GL_ALPHA                              , GL_ALPHA          , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_palette                     = {"palette",                  TEXTYPE_PALETTE       ,  1,  4,  4.0f, GL_RGB                                , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_palette_alpha               = {"palette_alpha",            TEXTYPE_PALETTE       ,  1,  4,  4.0f, GL_RGBA                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba                        = {"rgba",                     TEXTYPE_RGBA          ,  4,  4,  4.0f, GL_RGB                                , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_alpha                  = {"rgba_alpha",               TEXTYPE_RGBA          ,  4,  4,  4.0f, GL_RGBA                               , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_compress               = {"rgba_compress",            TEXTYPE_RGBA          ,  4,  4,  0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT       , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_rgba_alpha_compress         = {"rgba_alpha_compress",      TEXTYPE_RGBA          ,  4,  4,  1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT      , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra                        = {"bgra",                     TEXTYPE_BGRA          ,  4,  4,  4.0f, GL_RGB                                , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_alpha                  = {"bgra_alpha",               TEXTYPE_BGRA          ,  4,  4,  4.0f, GL_RGBA                               , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_compress               = {"bgra_compress",            TEXTYPE_BGRA          ,  4,  4,  0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT       , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_bgra_alpha_compress         = {"bgra_alpha_compress",      TEXTYPE_BGRA          ,  4,  4,  1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT      , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_dxt1                        = {"dxt1",                     TEXTYPE_DXT1          ,  4,  0,  0.5f, GL_COMPRESSED_RGB_S3TC_DXT1_EXT       , 0                 , 0                };
static textypeinfo_t textype_dxt1a                       = {"dxt1a",                    TEXTYPE_DXT1A         ,  4,  0,  0.5f, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT      , 0                 , 0                };
static textypeinfo_t textype_dxt3                        = {"dxt3",                     TEXTYPE_DXT3          ,  4,  0,  1.0f, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT      , 0                 , 0                };
static textypeinfo_t textype_dxt5                        = {"dxt5",                     TEXTYPE_DXT5          ,  4,  0,  1.0f, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT      , 0                 , 0                };
static textypeinfo_t textype_sRGB_palette                = {"sRGB_palette",             TEXTYPE_PALETTE       ,  1,  4,  4.0f, GL_SRGB_EXT                           , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_palette_alpha          = {"sRGB_palette_alpha",       TEXTYPE_PALETTE       ,  1,  4,  4.0f, GL_SRGB_ALPHA_EXT                     , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_rgba                   = {"sRGB_rgba",                TEXTYPE_RGBA          ,  4,  4,  4.0f, GL_SRGB_EXT                           , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_rgba_alpha             = {"sRGB_rgba_alpha",          TEXTYPE_RGBA          ,  4,  4,  4.0f, GL_SRGB_ALPHA_EXT                     , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_rgba_compress          = {"sRGB_rgba_compress",       TEXTYPE_RGBA          ,  4,  4,  0.5f, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT      , GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_rgba_alpha_compress    = {"sRGB_rgba_alpha_compress", TEXTYPE_RGBA          ,  4,  4,  1.0f, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_bgra                   = {"sRGB_bgra",                TEXTYPE_BGRA          ,  4,  4,  4.0f, GL_SRGB_EXT                           , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_bgra_alpha             = {"sRGB_bgra_alpha",          TEXTYPE_BGRA          ,  4,  4,  4.0f, GL_SRGB_ALPHA_EXT                     , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_bgra_compress          = {"sRGB_bgra_compress",       TEXTYPE_BGRA          ,  4,  4,  0.5f, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT      , GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_bgra_alpha_compress    = {"sRGB_bgra_alpha_compress", TEXTYPE_BGRA          ,  4,  4,  1.0f, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_BGRA           , GL_UNSIGNED_BYTE };
static textypeinfo_t textype_sRGB_dxt1                   = {"sRGB_dxt1",                TEXTYPE_DXT1          ,  4,  0,  0.5f, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT      , 0                 , 0                };
static textypeinfo_t textype_sRGB_dxt1a                  = {"sRGB_dxt1a",               TEXTYPE_DXT1A         ,  4,  0,  0.5f, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0                 , 0                };
static textypeinfo_t textype_sRGB_dxt3                   = {"sRGB_dxt3",                TEXTYPE_DXT3          ,  4,  0,  1.0f, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0                 , 0                };
static textypeinfo_t textype_sRGB_dxt5                   = {"sRGB_dxt5",                TEXTYPE_DXT5          ,  4,  0,  1.0f, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0                 , 0                };

typedef enum gltexturetype_e
{
	GLTEXTURETYPE_2D,
	GLTEXTURETYPE_3D,
	GLTEXTURETYPE_CUBEMAP,
	GLTEXTURETYPE_TOTAL
}
gltexturetype_t;

static int gltexturetypeenums[GLTEXTURETYPE_TOTAL] = {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARB};
static int gltexturetypedimensions[GLTEXTURETYPE_TOTAL] = {2, 3, 2};
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
	// d3d stuff the backend needs
	void *d3dtexture;
#ifdef SUPPORTD3D
	qboolean d3disdepthsurface; // for depth/stencil surfaces
	int d3dformat;
	int d3dusage;
	int d3dpool;
	int d3daddressu;
	int d3daddressv;
	int d3daddressw;
	int d3dmagfilter;
	int d3dminfilter;
	int d3dmipfilter;
	int d3dmaxmiplevelfilter;
	int d3dmipmaplodbias;
	int d3dmaxmiplevel;
#endif

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
	// picmip level
	int miplevel;
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
	// how many mipmap levels in this texture
	int miplevels;
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
	case TEXTYPE_DXT1: return &textype_dxt1;
	case TEXTYPE_DXT1A: return &textype_dxt1a;
	case TEXTYPE_DXT3: return &textype_dxt3;
	case TEXTYPE_DXT5: return &textype_dxt5;
	case TEXTYPE_PALETTE: return (flags & TEXF_ALPHA) ? &textype_palette_alpha : &textype_palette;
	case TEXTYPE_RGBA: return ((flags & TEXF_COMPRESS) && vid.support.ext_texture_compression_s3tc) ? ((flags & TEXF_ALPHA) ? &textype_rgba_alpha_compress : &textype_rgba_compress) : ((flags & TEXF_ALPHA) ? &textype_rgba_alpha : &textype_rgba);
	case TEXTYPE_BGRA: return ((flags & TEXF_COMPRESS) && vid.support.ext_texture_compression_s3tc) ? ((flags & TEXF_ALPHA) ? &textype_bgra_alpha_compress : &textype_bgra_compress) : ((flags & TEXF_ALPHA) ? &textype_bgra_alpha : &textype_bgra);
	case TEXTYPE_ALPHA: return &textype_alpha;
	case TEXTYPE_SHADOWMAP: return (flags & TEXF_LOWPRECISION) ? &textype_shadowmap16 : &textype_shadowmap24;
	case TEXTYPE_COLORBUFFER: return &textype_colorbuffer;
	case TEXTYPE_COLORBUFFER16F: return &textype_colorbuffer16f;
	case TEXTYPE_COLORBUFFER32F: return &textype_colorbuffer32f;
	case TEXTYPE_SRGB_DXT1: return &textype_sRGB_dxt1;
	case TEXTYPE_SRGB_DXT1A: return &textype_sRGB_dxt1a;
	case TEXTYPE_SRGB_DXT3: return &textype_sRGB_dxt3;
	case TEXTYPE_SRGB_DXT5: return &textype_sRGB_dxt5;
	case TEXTYPE_SRGB_PALETTE: return (flags & TEXF_ALPHA) ? &textype_sRGB_palette_alpha : &textype_sRGB_palette;
	case TEXTYPE_SRGB_RGBA: return ((flags & TEXF_COMPRESS) && vid.support.ext_texture_compression_s3tc) ? ((flags & TEXF_ALPHA) ? &textype_sRGB_rgba_alpha_compress : &textype_sRGB_rgba_compress) : ((flags & TEXF_ALPHA) ? &textype_sRGB_rgba_alpha : &textype_sRGB_rgba);
	case TEXTYPE_SRGB_BGRA: return ((flags & TEXF_COMPRESS) && vid.support.ext_texture_compression_s3tc) ? ((flags & TEXF_ALPHA) ? &textype_sRGB_bgra_alpha_compress : &textype_sRGB_bgra_compress) : ((flags & TEXF_ALPHA) ? &textype_sRGB_bgra_alpha : &textype_sRGB_bgra);
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

	R_Mesh_ClearBindingsForTexture(glt->texnum);

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (glt->texnum)
		{
			CHECKGLERROR
			qglDeleteTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		if (glt->d3disdepthsurface)
			IDirect3DSurface9_Release((IDirect3DSurface9 *)glt->d3dtexture);
		else if (glt->tiledepth > 1)
			IDirect3DVolumeTexture9_Release((IDirect3DVolumeTexture9 *)glt->d3dtexture);
		else if (glt->sides == 6)
			IDirect3DCubeTexture9_Release((IDirect3DCubeTexture9 *)glt->d3dtexture);
		else
			IDirect3DTexture9_Release((IDirect3DTexture9 *)glt->d3dtexture);
		glt->d3dtexture = NULL;
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (glt->texnum)
			DPSOFTRAST_Texture_Free(glt->texnum);
		break;
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
	const char *name;
	int minification, magnification;
	DPSOFTRAST_TEXTURE_FILTER dpsoftrastfilter_mipmap, dpsoftrastfilter_nomipmap;
}
glmode_t;

static glmode_t modes[6] =
{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST, DPSOFTRAST_TEXTURE_FILTER_NEAREST, DPSOFTRAST_TEXTURE_FILTER_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR, DPSOFTRAST_TEXTURE_FILTER_LINEAR, DPSOFTRAST_TEXTURE_FILTER_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, DPSOFTRAST_TEXTURE_FILTER_NEAREST_MIPMAP_TRIANGLE, DPSOFTRAST_TEXTURE_FILTER_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR, DPSOFTRAST_TEXTURE_FILTER_LINEAR_MIPMAP_TRIANGLE, DPSOFTRAST_TEXTURE_FILTER_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST, DPSOFTRAST_TEXTURE_FILTER_NEAREST_MIPMAP_TRIANGLE, DPSOFTRAST_TEXTURE_FILTER_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, DPSOFTRAST_TEXTURE_FILTER_LINEAR_MIPMAP_TRIANGLE, DPSOFTRAST_TEXTURE_FILTER_LINEAR}
};

#ifdef SUPPORTD3D
typedef struct d3dmode_s
{
	const char *name;
	int m1, m2;
}
d3dmode_t;

static d3dmode_t d3dmodes[6] =
{
	{"GL_NEAREST", D3DTEXF_POINT, D3DTEXF_POINT},
	{"GL_LINEAR", D3DTEXF_LINEAR, D3DTEXF_POINT},
	{"GL_NEAREST_MIPMAP_NEAREST", D3DTEXF_POINT, D3DTEXF_POINT},
	{"GL_LINEAR_MIPMAP_NEAREST", D3DTEXF_LINEAR, D3DTEXF_POINT},
	{"GL_NEAREST_MIPMAP_LINEAR", D3DTEXF_POINT, D3DTEXF_LINEAR},
	{"GL_LINEAR_MIPMAP_LINEAR", D3DTEXF_LINEAR, D3DTEXF_LINEAR}
};
#endif

static void GL_TextureMode_f (void)
{
	int i;
	GLint oldbindtexnum;
	gltexture_t *glt;
	gltexturepool_t *pool;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("Texture mode is %sforced\n", gl_filter_force ? "" : "not ");
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

	for (i = 0;i < (int)(sizeof(modes)/sizeof(*modes));i++)
		if (!strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	if (i == 6)
	{
		Con_Print("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minification;
	gl_filter_mag = modes[i].magnification;
	gl_filter_force = ((Cmd_Argc() > 2) && !strcasecmp(Cmd_Argv(2), "force"));

	dpsoftrast_filter_mipmap = modes[i].dpsoftrastfilter_mipmap;
	dpsoftrast_filter_nomipmap = modes[i].dpsoftrastfilter_nomipmap;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		// change all the existing mipmap texture objects
		// FIXME: force renderer(/client/something?) restart instead?
		CHECKGLERROR
		GL_ActiveTexture(0);
		for (pool = gltexturepoolchain;pool;pool = pool->next)
		{
			for (glt = pool->gltchain;glt;glt = glt->chain)
			{
				// only update already uploaded images
				if (glt->texnum && (gl_filter_force || !(glt->flags & (TEXF_FORCENEAREST | TEXF_FORCELINEAR))))
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
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		d3d_filter_flatmin = d3dmodes[i].m1;
		d3d_filter_flatmag = d3dmodes[i].m1;
		d3d_filter_flatmix = D3DTEXF_POINT;
		d3d_filter_mipmin = d3dmodes[i].m1;
		d3d_filter_mipmag = d3dmodes[i].m1;
		d3d_filter_mipmix = d3dmodes[i].m2;
		d3d_filter_nomip = i < 2;
		if (gl_texture_anisotropy.integer > 1 && i == 5)
			d3d_filter_mipmin = d3d_filter_mipmag = D3DTEXF_ANISOTROPIC;
		for (pool = gltexturepoolchain;pool;pool = pool->next)
		{
			for (glt = pool->gltchain;glt;glt = glt->chain)
			{
				// only update already uploaded images
				if (glt->d3dtexture && !glt->d3disdepthsurface && (gl_filter_force || !(glt->flags & (TEXF_FORCENEAREST | TEXF_FORCELINEAR))))
				{
					if (glt->flags & TEXF_MIPMAP)
					{
						glt->d3dminfilter = d3d_filter_mipmin;
						glt->d3dmagfilter = d3d_filter_mipmag;
						glt->d3dmipfilter = d3d_filter_mipmix;
						glt->d3dmaxmiplevelfilter = 0;
					}
					else
					{
						glt->d3dminfilter = d3d_filter_flatmin;
						glt->d3dmagfilter = d3d_filter_flatmag;
						glt->d3dmipfilter = d3d_filter_flatmix;
						glt->d3dmaxmiplevelfilter = 0;
					}
				}
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		// change all the existing texture objects
		for (pool = gltexturepoolchain;pool;pool = pool->next)
			for (glt = pool->gltchain;glt;glt = glt->chain)
				if (glt->texnum && (gl_filter_force || !(glt->flags & (TEXF_FORCENEAREST | TEXF_FORCELINEAR))))
					DPSOFTRAST_Texture_Filter(glt->texnum, (glt->flags & TEXF_MIPMAP) ? dpsoftrast_filter_mipmap : dpsoftrast_filter_nomipmap);
		break;
	}
}

static void GL_Texture_CalcImageSize(int texturetype, int flags, int miplevel, int inwidth, int inheight, int indepth, int *outwidth, int *outheight, int *outdepth, int *outmiplevels)
{
	int picmip = 0, maxsize = 0, width2 = 1, height2 = 1, depth2 = 1, miplevels = 1;

	switch (texturetype)
	{
	default:
	case GLTEXTURETYPE_2D:
		maxsize = vid.maxtexturesize_2d;
		if (flags & TEXF_PICMIP)
		{
			maxsize = bound(1, gl_max_size.integer, maxsize);
			picmip = miplevel;
		}
		break;
	case GLTEXTURETYPE_3D:
		maxsize = vid.maxtexturesize_3d;
		break;
	case GLTEXTURETYPE_CUBEMAP:
		maxsize = vid.maxtexturesize_cubemap;
		break;
	}

	if (vid.support.arb_texture_non_power_of_two)
	{
		width2 = min(inwidth >> picmip, maxsize);
		height2 = min(inheight >> picmip, maxsize);
		depth2 = min(indepth >> picmip, maxsize);
	}
	else
	{
		for (width2 = 1;width2 < inwidth;width2 <<= 1);
		for (width2 >>= picmip;width2 > maxsize;width2 >>= 1);
		for (height2 = 1;height2 < inheight;height2 <<= 1);
		for (height2 >>= picmip;height2 > maxsize;height2 >>= 1);
		for (depth2 = 1;depth2 < indepth;depth2 <<= 1);
		for (depth2 >>= picmip;depth2 > maxsize;depth2 >>= 1);
	}

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
#if 0
		// for some reason the REF rasterizer (and hence the PIX debugger) does not like small textures...
		if (texturetype == GLTEXTURETYPE_2D)
		{
			width2 = max(width2, 2);
			height2 = max(height2, 2);
		}
#endif
		break;
	}

	miplevels = 1;
	if (flags & TEXF_MIPMAP)
	{
		int extent = max(width2, max(height2, depth2));
		while(extent >>= 1)
			miplevels++;
	}

	if (outwidth)
		*outwidth = max(1, width2);
	if (outheight)
		*outheight = max(1, height2);
	if (outdepth)
		*outdepth = max(1, depth2);
	if (outmiplevels)
		*outmiplevels = miplevels;
}


static int R_CalcTexelDataSize (gltexture_t *glt)
{
	int width2, height2, depth2, size;

	GL_Texture_CalcImageSize(glt->texturetype, glt->flags, glt->miplevel, glt->inputwidth, glt->inputheight, glt->inputdepth, &width2, &height2, &depth2, NULL);

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
				Con_Printf("%c%4i%c%c%4i%c %-24s %s %s %s %s\n", isloaded ? '[' : ' ', (glsize + 1023) / 1024, isloaded ? ']' : ' ', glt->inputtexels ? '[' : ' ', (glt->inputdatasize + 1023) / 1024, glt->inputtexels ? ']' : ' ', glt->textype->name, isloaded ? "loaded" : "      ", (glt->flags & TEXF_MIPMAP) ? "mip" : "   ", (glt->flags & TEXF_ALPHA) ? "alpha" : "     ", glt->identifier);
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
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		// LordHavoc: allow any alignment
		CHECKGLERROR
		qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);CHECKGLERROR
		qglPixelStorei(GL_PACK_ALIGNMENT, 1);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}

	texturemempool = Mem_AllocPool("texture management", 0, NULL);
	Mem_ExpandableArray_NewArray(&texturearray, texturemempool, sizeof(gltexture_t), 512);

	// Disable JPEG screenshots if the DLL isn't loaded
	if (! JPEG_OpenLibrary ())
		Cvar_SetValueQuick (&scr_screenshot_jpeg, 0);
	if (! PNG_OpenLibrary ())
		Cvar_SetValueQuick (&scr_screenshot_png, 0);
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

static void r_textures_devicelost(void)
{
	int i, endindex;
	gltexture_t *glt;
	endindex = Mem_ExpandableArray_IndexRange(&texturearray);
	for (i = 0;i < endindex;i++)
	{
		glt = (gltexture_t *) Mem_ExpandableArray_RecordAtIndex(&texturearray, i);
		if (!glt || !(glt->flags & TEXF_RENDERTARGET))
			continue;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			if (glt->d3disdepthsurface)
				IDirect3DSurface9_Release((IDirect3DSurface9 *)glt->d3dtexture);
			else if (glt->tiledepth > 1)
				IDirect3DVolumeTexture9_Release((IDirect3DVolumeTexture9 *)glt->d3dtexture);
			else if (glt->sides == 6)
				IDirect3DCubeTexture9_Release((IDirect3DCubeTexture9 *)glt->d3dtexture);
			else
				IDirect3DTexture9_Release((IDirect3DTexture9 *)glt->d3dtexture);
			glt->d3dtexture = NULL;
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			break;
		}
	}
}

static void r_textures_devicerestored(void)
{
	int i, endindex;
	gltexture_t *glt;
	endindex = Mem_ExpandableArray_IndexRange(&texturearray);
	for (i = 0;i < endindex;i++)
	{
		glt = (gltexture_t *) Mem_ExpandableArray_RecordAtIndex(&texturearray, i);
		if (!glt || !(glt->flags & TEXF_RENDERTARGET))
			continue;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			{
				HRESULT d3dresult;
				if (glt->d3disdepthsurface)
				{
					if (FAILED(d3dresult = IDirect3DDevice9_CreateDepthStencilSurface(vid_d3d9dev, glt->tilewidth, glt->tileheight, (D3DFORMAT)glt->d3dformat, D3DMULTISAMPLE_NONE, 0, false, (IDirect3DSurface9 **)&glt->d3dtexture, NULL)))
						Sys_Error("IDirect3DDevice9_CreateDepthStencilSurface failed!");
				}
				else if (glt->tiledepth > 1)
				{
					if (FAILED(d3dresult = IDirect3DDevice9_CreateVolumeTexture(vid_d3d9dev, glt->tilewidth, glt->tileheight, glt->tiledepth, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DVolumeTexture9 **)&glt->d3dtexture, NULL)))
						Sys_Error("IDirect3DDevice9_CreateVolumeTexture failed!");
				}
				else if (glt->sides == 6)
				{
					if (FAILED(d3dresult = IDirect3DDevice9_CreateCubeTexture(vid_d3d9dev, glt->tilewidth, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DCubeTexture9 **)&glt->d3dtexture, NULL)))
						Sys_Error("IDirect3DDevice9_CreateCubeTexture failed!");
				}
				else
				{
					if (FAILED(d3dresult = IDirect3DDevice9_CreateTexture(vid_d3d9dev, glt->tilewidth, glt->tileheight, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DTexture9 **)&glt->d3dtexture, NULL)))
						Sys_Error("IDirect3DDevice9_CreateTexture failed!");
				}
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			break;
		}
	}
}


void R_Textures_Init (void)
{
	Cmd_AddCommand("gl_texturemode", &GL_TextureMode_f, "set texture filtering mode (GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, etc); an additional argument 'force' forces the texture mode even in cases where it may not be appropriate");
	Cmd_AddCommand("r_texturestats", R_TextureStats_f, "print information about all loaded textures and some statistics");
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&gl_picmip_world);
	Cvar_RegisterVariable (&r_picmipworld);
	Cvar_RegisterVariable (&gl_picmip_sprites);
	Cvar_RegisterVariable (&r_picmipsprites);
	Cvar_RegisterVariable (&gl_picmip_other);
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
	Cvar_RegisterVariable (&gl_texturecompression_sprites);
	Cvar_RegisterVariable (&gl_nopartialtextureupdates);
	Cvar_RegisterVariable (&r_texture_dds_load_alphamode);
	Cvar_RegisterVariable (&r_texture_dds_load_logfailure);
	Cvar_RegisterVariable (&r_texture_dds_swdecode);

	R_RegisterModule("R_Textures", r_textures_start, r_textures_shutdown, r_textures_newmap, r_textures_devicelost, r_textures_devicerestored);
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

		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
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
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
			break;
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
	if (!gl_filter_force && flags & TEXF_FORCENEAREST)
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
	else if (!gl_filter_force && flags & TEXF_FORCELINEAR)
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

static void R_UploadPartialTexture(gltexture_t *glt, const unsigned char *data, int fragx, int fragy, int fragz, int fragwidth, int fragheight, int fragdepth)
{
	if (data == NULL)
		Sys_Error("R_UploadPartialTexture \"%s\": partial update with NULL pixels", glt->identifier);

	if (glt->texturetype != GLTEXTURETYPE_2D)
		Sys_Error("R_UploadPartialTexture \"%s\": partial update of type other than 2D", glt->identifier);

	if (glt->textype->textype == TEXTYPE_PALETTE)
		Sys_Error("R_UploadPartialTexture \"%s\": partial update of paletted texture", glt->identifier);

	if (glt->flags & (TEXF_MIPMAP | TEXF_PICMIP))
		Sys_Error("R_UploadPartialTexture \"%s\": partial update not supported with MIPMAP or PICMIP flags", glt->identifier);

	if (glt->inputwidth != glt->tilewidth || glt->inputheight != glt->tileheight || glt->tiledepth != 1)
		Sys_Error("R_UploadPartialTexture \"%s\": partial update not supported with stretched or special textures", glt->identifier);

	// update a portion of the image

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		{
			int oldbindtexnum;
			CHECKGLERROR
			// we need to restore the texture binding after finishing the upload
			GL_ActiveTexture(0);
			oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
			qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
			qglTexSubImage2D(GL_TEXTURE_2D, 0, fragx, fragy, fragwidth, fragheight, glt->glformat, glt->gltype, data);CHECKGLERROR
			qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			RECT d3drect;
			D3DLOCKED_RECT d3dlockedrect;
			int y;
			memset(&d3drect, 0, sizeof(d3drect));
			d3drect.left = fragx;
			d3drect.top = fragy;
			d3drect.right = fragx+fragwidth;
			d3drect.bottom = fragy+fragheight;
			if (IDirect3DTexture9_LockRect((IDirect3DTexture9*)glt->d3dtexture, 0, &d3dlockedrect, &d3drect, 0) == D3D_OK && d3dlockedrect.pBits)
			{
				for (y = 0;y < fragheight;y++)
					memcpy((unsigned char *)d3dlockedrect.pBits + d3dlockedrect.Pitch * y, data + fragwidth*glt->bytesperpixel * y, fragwidth*glt->bytesperpixel);
				IDirect3DTexture9_UnlockRect((IDirect3DTexture9*)glt->d3dtexture, 0);
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_Texture_UpdatePartial(glt->texnum, 0, data, fragx, fragy, fragwidth, fragheight);
		break;
	}
}

static void R_UploadFullTexture(gltexture_t *glt, const unsigned char *data)
{
	int i, mip = 0, width, height, depth;
	GLint oldbindtexnum = 0;
	const unsigned char *prevbuffer;
	prevbuffer = data;

	// error out if a stretch is needed on special texture types
	if (glt->texturetype != GLTEXTURETYPE_2D && (glt->tilewidth != glt->inputwidth || glt->tileheight != glt->inputheight || glt->tiledepth != glt->inputdepth))
		Sys_Error("R_UploadFullTexture \"%s\": stretch uploads allowed only on 2D textures\n", glt->identifier);

	// when picmip or maxsize is applied, we scale up to a power of 2 multiple
	// of the target size and then use the mipmap reduction function to get
	// high quality supersampled results
	for (width  = glt->tilewidth;width  < glt->inputwidth ;width  <<= 1);
	for (height = glt->tileheight;height < glt->inputheight;height <<= 1);
	for (depth  = glt->tiledepth;depth  < glt->inputdepth ;depth  <<= 1);
	R_MakeResizeBufferBigger(width * height * depth * glt->sides * glt->bytesperpixel);

	if (prevbuffer == NULL)
	{
		width = glt->tilewidth;
		height = glt->tileheight;
		depth = glt->tiledepth;
//		memset(resizebuffer, 0, width * height * depth * glt->sides * glt->bytesperpixel);
//		prevbuffer = resizebuffer;
	}
	else
	{
		if (glt->textype->textype == TEXTYPE_PALETTE)
		{
			// promote paletted to BGRA, so we only have to worry about BGRA in the rest of this code
			Image_Copy8bitBGRA(prevbuffer, colorconvertbuffer, glt->inputwidth * glt->inputheight * glt->inputdepth * glt->sides, glt->palette);
			prevbuffer = colorconvertbuffer;
		}
		if (glt->flags & TEXF_RGBMULTIPLYBYALPHA)
		{
			// multiply RGB channels by A channel before uploading
			int alpha;
			for (i = 0;i < glt->inputwidth*glt->inputheight*glt->inputdepth*4;i += 4)
			{
				alpha = prevbuffer[i+3];
				colorconvertbuffer[i] = (prevbuffer[i] * alpha) >> 8;
				colorconvertbuffer[i+1] = (prevbuffer[i+1] * alpha) >> 8;
				colorconvertbuffer[i+2] = (prevbuffer[i+2] * alpha) >> 8;
				colorconvertbuffer[i+3] = alpha;
			}
			prevbuffer = colorconvertbuffer;
		}
		// scale up to a power of 2 size (if appropriate)
		if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
		{
			Image_Resample32(prevbuffer, glt->inputwidth, glt->inputheight, glt->inputdepth, resizebuffer, width, height, depth, r_lerpimages.integer);
			prevbuffer = resizebuffer;
		}
		// apply mipmap reduction algorithm to get down to picmip/max_size
		while (width > glt->tilewidth || height > glt->tileheight || depth > glt->tiledepth)
		{
			Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, glt->tilewidth, glt->tileheight, glt->tiledepth);
			prevbuffer = resizebuffer;
		}
	}

	// do the appropriate upload type...
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR

		// we need to restore the texture binding after finishing the upload
		GL_ActiveTexture(0);
		oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
		qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR

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
		}
		GL_SetupTextureParameters(glt->flags, glt->textype->textype, glt->texturetype);
		qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		if (!(glt->flags & TEXF_RENDERTARGET))
		{
			D3DLOCKED_RECT d3dlockedrect;
			D3DLOCKED_BOX d3dlockedbox;
			switch(glt->texturetype)
			{
			case GLTEXTURETYPE_2D:
				if (IDirect3DTexture9_LockRect((IDirect3DTexture9*)glt->d3dtexture, mip, &d3dlockedrect, NULL, 0) == D3D_OK && d3dlockedrect.pBits)
				{
					if (prevbuffer)
						memcpy(d3dlockedrect.pBits, prevbuffer, width*height*glt->bytesperpixel);
					else
						memset(d3dlockedrect.pBits, 255, width*height*glt->bytesperpixel);
					IDirect3DTexture9_UnlockRect((IDirect3DTexture9*)glt->d3dtexture, mip);
				}
				mip++;
				if ((glt->flags & TEXF_MIPMAP) && prevbuffer)
				{
					while (width > 1 || height > 1 || depth > 1)
					{
						Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
						prevbuffer = resizebuffer;
						if (IDirect3DTexture9_LockRect((IDirect3DTexture9*)glt->d3dtexture, mip, &d3dlockedrect, NULL, 0) == D3D_OK && d3dlockedrect.pBits)
						{
							memcpy(d3dlockedrect.pBits, prevbuffer, width*height*glt->bytesperpixel);
							IDirect3DTexture9_UnlockRect((IDirect3DTexture9*)glt->d3dtexture, mip);
						}
						mip++;
					}
				}
				break;
			case GLTEXTURETYPE_3D:
				if (IDirect3DVolumeTexture9_LockBox((IDirect3DVolumeTexture9*)glt->d3dtexture, mip, &d3dlockedbox, NULL, 0) == D3D_OK && d3dlockedbox.pBits)
				{
					// we are not honoring the RowPitch or SlicePitch, hopefully this works with all sizes
					memcpy(d3dlockedbox.pBits, prevbuffer, width*height*depth*glt->bytesperpixel);
					IDirect3DVolumeTexture9_UnlockBox((IDirect3DVolumeTexture9*)glt->d3dtexture, mip);
				}
				mip++;
				if (glt->flags & TEXF_MIPMAP)
				{
					while (width > 1 || height > 1 || depth > 1)
					{
						Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
						prevbuffer = resizebuffer;
						if (IDirect3DVolumeTexture9_LockBox((IDirect3DVolumeTexture9*)glt->d3dtexture, mip, &d3dlockedbox, NULL, 0) == D3D_OK && d3dlockedbox.pBits)
						{
							// we are not honoring the RowPitch or SlicePitch, hopefully this works with all sizes
							memcpy(d3dlockedbox.pBits, prevbuffer, width*height*depth*glt->bytesperpixel);
							IDirect3DVolumeTexture9_UnlockBox((IDirect3DVolumeTexture9*)glt->d3dtexture, mip);
						}
						mip++;
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
					if (IDirect3DCubeTexture9_LockRect((IDirect3DCubeTexture9*)glt->d3dtexture, (D3DCUBEMAP_FACES)i, mip, &d3dlockedrect, NULL, 0) == D3D_OK && d3dlockedrect.pBits)
					{
						memcpy(d3dlockedrect.pBits, prevbuffer, width*height*glt->bytesperpixel);
						IDirect3DCubeTexture9_UnlockRect((IDirect3DCubeTexture9*)glt->d3dtexture, (D3DCUBEMAP_FACES)i, mip);
					}
					mip++;
					if (glt->flags & TEXF_MIPMAP)
					{
						while (width > 1 || height > 1 || depth > 1)
						{
							Image_MipReduce32(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1);
							prevbuffer = resizebuffer;
							if (IDirect3DCubeTexture9_LockRect((IDirect3DCubeTexture9*)glt->d3dtexture, (D3DCUBEMAP_FACES)i, mip, &d3dlockedrect, NULL, 0) == D3D_OK && d3dlockedrect.pBits)
							{
								memcpy(d3dlockedrect.pBits, prevbuffer, width*height*glt->bytesperpixel);
								IDirect3DCubeTexture9_UnlockRect((IDirect3DCubeTexture9*)glt->d3dtexture, (D3DCUBEMAP_FACES)i, mip);
							}
							mip++;
						}
					}
				}
				break;
			}
		}
		glt->d3daddressw = 0;
		if (glt->flags & TEXF_CLAMP)
		{
			glt->d3daddressu = D3DTADDRESS_CLAMP;
			glt->d3daddressv = D3DTADDRESS_CLAMP;
			if (glt->tiledepth > 1)
				glt->d3daddressw = D3DTADDRESS_CLAMP;
		}
		else
		{
			glt->d3daddressu = D3DTADDRESS_WRAP;
			glt->d3daddressv = D3DTADDRESS_WRAP;
			if (glt->tiledepth > 1)
				glt->d3daddressw = D3DTADDRESS_WRAP;
		}
		glt->d3dmipmaplodbias = 0;
		glt->d3dmaxmiplevel = 0;
		glt->d3dmaxmiplevelfilter = d3d_filter_nomip ? 0 : glt->d3dmaxmiplevel;
		if (glt->flags & TEXF_FORCELINEAR)
		{
			glt->d3dminfilter = D3DTEXF_LINEAR;
			glt->d3dmagfilter = D3DTEXF_LINEAR;
			glt->d3dmipfilter = D3DTEXF_POINT;
		}
		else if (glt->flags & TEXF_FORCENEAREST)
		{
			glt->d3dminfilter = D3DTEXF_POINT;
			glt->d3dmagfilter = D3DTEXF_POINT;
			glt->d3dmipfilter = D3DTEXF_POINT;
		}
		else if (glt->flags & TEXF_MIPMAP)
		{
			glt->d3dminfilter = d3d_filter_mipmin;
			glt->d3dmagfilter = d3d_filter_mipmag;
			glt->d3dmipfilter = d3d_filter_mipmix;
		}
		else
		{
			glt->d3dminfilter = d3d_filter_flatmin;
			glt->d3dmagfilter = d3d_filter_flatmag;
			glt->d3dmipfilter = d3d_filter_flatmix;
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		switch(glt->texturetype)
		{
		case GLTEXTURETYPE_2D:
			DPSOFTRAST_Texture_UpdateFull(glt->texnum, prevbuffer);
			break;
		case GLTEXTURETYPE_3D:
			DPSOFTRAST_Texture_UpdateFull(glt->texnum, prevbuffer);
			break;
		case GLTEXTURETYPE_CUBEMAP:
			if (glt->inputwidth != width || glt->inputheight != height || glt->inputdepth != depth)
			{
				unsigned char *combinedbuffer = (unsigned char *)Mem_Alloc(tempmempool, glt->tilewidth*glt->tileheight*glt->tiledepth*glt->sides*glt->bytesperpixel);
				// convert and upload each side in turn,
				// from a continuous block of input texels
				// copy the results into combinedbuffer
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
					memcpy(combinedbuffer + i*glt->tilewidth*glt->tileheight*glt->tiledepth*glt->bytesperpixel, prevbuffer, glt->tilewidth*glt->tileheight*glt->tiledepth*glt->bytesperpixel);
				}
				DPSOFTRAST_Texture_UpdateFull(glt->texnum, combinedbuffer);
				Mem_Free(combinedbuffer);
			}
			else
				DPSOFTRAST_Texture_UpdateFull(glt->texnum, prevbuffer);
			break;
		}
		if (glt->flags & TEXF_FORCELINEAR)
			DPSOFTRAST_Texture_Filter(glt->texnum, DPSOFTRAST_TEXTURE_FILTER_LINEAR);
		else if (glt->flags & TEXF_FORCENEAREST)
			DPSOFTRAST_Texture_Filter(glt->texnum, DPSOFTRAST_TEXTURE_FILTER_NEAREST);
		else if (glt->flags & TEXF_MIPMAP)
			DPSOFTRAST_Texture_Filter(glt->texnum, dpsoftrast_filter_mipmap);
		else
			DPSOFTRAST_Texture_Filter(glt->texnum, dpsoftrast_filter_nomipmap);
		break;
	}
}

static rtexture_t *R_SetupTexture(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, int sides, int flags, int miplevel, textype_t textype, int texturetype, const unsigned char *data, const unsigned int *palette)
{
	int i, size;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo, *texinfo2;
	unsigned char *temppixels = NULL;
	qboolean swaprb;

	if (cls.state == ca_dedicated)
		return NULL;

	// see if we need to swap red and blue (BGRA <-> RGBA conversion)
	swaprb = false;
	switch(textype)
	{
	case TEXTYPE_RGBA: if (vid.forcetextype == TEXTYPE_BGRA) {swaprb = true;textype = TEXTYPE_BGRA;} break;
	case TEXTYPE_BGRA: if (vid.forcetextype == TEXTYPE_RGBA) {swaprb = true;textype = TEXTYPE_RGBA;} break;
	case TEXTYPE_SRGB_RGBA: if (vid.forcetextype == TEXTYPE_BGRA) {swaprb = true;textype = TEXTYPE_SRGB_BGRA;} break;
	case TEXTYPE_SRGB_BGRA: if (vid.forcetextype == TEXTYPE_RGBA) {swaprb = true;textype = TEXTYPE_SRGB_RGBA;} break;
	default: break;
	}
	if (swaprb)
	{
		// swap bytes
		static int rgbaswapindices[4] = {2, 1, 0, 3};
		size = width * height * depth * sides * 4;
		temppixels = (unsigned char *)Mem_Alloc(tempmempool, size);
		Image_CopyMux(temppixels, data, width, height*depth*sides, false, false, false, 4, 4, rgbaswapindices);
		data = temppixels;
	}

	// if sRGB texture formats are not supported, convert input to linear and upload as normal types
	if (!vid.support.ext_texture_srgb)
	{
		qboolean convertsRGB = false;
		switch(textype)
		{
		case TEXTYPE_SRGB_DXT1:    textype = TEXTYPE_DXT1   ;convertsRGB = true;break;
		case TEXTYPE_SRGB_DXT1A:   textype = TEXTYPE_DXT1A  ;convertsRGB = true;break;
		case TEXTYPE_SRGB_DXT3:    textype = TEXTYPE_DXT3   ;convertsRGB = true;break;
		case TEXTYPE_SRGB_DXT5:    textype = TEXTYPE_DXT5   ;convertsRGB = true;break;
		case TEXTYPE_SRGB_PALETTE: textype = TEXTYPE_PALETTE;convertsRGB = true;break;
		case TEXTYPE_SRGB_RGBA:    textype = TEXTYPE_RGBA   ;convertsRGB = true;break;
		case TEXTYPE_SRGB_BGRA:    textype = TEXTYPE_BGRA   ;convertsRGB = true;break;
		default:
			break;
		}
		if (convertsRGB && data)
		{
			size = width * height * depth * sides * 4;
			if (!temppixels)
			{
				temppixels = (unsigned char *)Mem_Alloc(tempmempool, size);
				memcpy(temppixels, data, size);
			}
			Image_MakeLinearColorsFromsRGB(temppixels, temppixels, width*height*depth*sides);
		}
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
	case TEXTYPE_SRGB_PALETTE:
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
	case TEXTYPE_SRGB_RGBA:
	case TEXTYPE_SRGB_BGRA:
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
	case TEXTYPE_SRGB_DXT1:
		break;
	case TEXTYPE_DXT1A:
	case TEXTYPE_SRGB_DXT1A:
	case TEXTYPE_DXT3:
	case TEXTYPE_SRGB_DXT3:
	case TEXTYPE_DXT5:
	case TEXTYPE_SRGB_DXT5:
		flags |= TEXF_ALPHA;
		break;
	case TEXTYPE_ALPHA:
		flags |= TEXF_ALPHA;
		break;
	case TEXTYPE_COLORBUFFER:
	case TEXTYPE_COLORBUFFER16F:
	case TEXTYPE_COLORBUFFER32F:
		flags |= TEXF_ALPHA;
		break;
	default:
		Sys_Error("R_LoadTexture: unknown texture type");
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
	glt->miplevel = (miplevel < 0) ? R_PicmipForFlags(flags) : miplevel; // note: if miplevel is -1, we know the texture is in original size and we can picmip it normally
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

	GL_Texture_CalcImageSize(glt->texturetype, glt->flags, glt->miplevel, glt->inputwidth, glt->inputheight, glt->inputdepth, &glt->tilewidth, &glt->tileheight, &glt->tiledepth, &glt->miplevels);

	// upload the texture
	// data may be NULL (blank texture for dynamic rendering)
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		qglGenTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			D3DFORMAT d3dformat;
			D3DPOOL d3dpool;
			DWORD d3dusage;
			HRESULT d3dresult;
			d3dusage = 0;
			d3dpool = D3DPOOL_MANAGED;
			if (flags & TEXF_RENDERTARGET)
			{
				d3dusage |= D3DUSAGE_RENDERTARGET;
				d3dpool = D3DPOOL_DEFAULT;
			}
			switch(textype)
			{
			case TEXTYPE_PALETTE: d3dformat = (flags & TEXF_ALPHA) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;break;
			case TEXTYPE_RGBA: d3dformat = (flags & TEXF_ALPHA) ? D3DFMT_A8B8G8R8 : D3DFMT_X8B8G8R8;break;
			case TEXTYPE_BGRA: d3dformat = (flags & TEXF_ALPHA) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;break;
			case TEXTYPE_COLORBUFFER: d3dformat = D3DFMT_A8R8G8B8;break;
			case TEXTYPE_COLORBUFFER16F: d3dformat = D3DFMT_A16B16G16R16F;break;
			case TEXTYPE_COLORBUFFER32F: d3dformat = D3DFMT_A32B32G32R32F;break;
			case TEXTYPE_SHADOWMAP: d3dformat = D3DFMT_D16;d3dusage = D3DUSAGE_DEPTHSTENCIL;break; // note: can not use D3DUSAGE_RENDERTARGET here
			case TEXTYPE_ALPHA: d3dformat = D3DFMT_A8;break;
			default: d3dformat = D3DFMT_A8R8G8B8;Sys_Error("R_LoadTexture: unsupported texture type %i when picking D3DFMT", (int)textype);break;
			}
			glt->d3dformat = d3dformat;
			glt->d3dusage = d3dusage;
			glt->d3dpool = d3dpool;
			glt->d3disdepthsurface = textype == TEXTYPE_SHADOWMAP;
			if (glt->d3disdepthsurface)
			{
				if (FAILED(d3dresult = IDirect3DDevice9_CreateDepthStencilSurface(vid_d3d9dev, glt->tilewidth, glt->tileheight, (D3DFORMAT)glt->d3dformat, D3DMULTISAMPLE_NONE, 0, false, (IDirect3DSurface9 **)&glt->d3dtexture, NULL)))
					Sys_Error("IDirect3DDevice9_CreateDepthStencilSurface failed!");
			}
			else if (glt->tiledepth > 1)
			{
				if (FAILED(d3dresult = IDirect3DDevice9_CreateVolumeTexture(vid_d3d9dev, glt->tilewidth, glt->tileheight, glt->tiledepth, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DVolumeTexture9 **)&glt->d3dtexture, NULL)))
					Sys_Error("IDirect3DDevice9_CreateVolumeTexture failed!");
			}
			else if (glt->sides == 6)
			{
				if (FAILED(d3dresult = IDirect3DDevice9_CreateCubeTexture(vid_d3d9dev, glt->tilewidth, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DCubeTexture9 **)&glt->d3dtexture, NULL)))
					Sys_Error("IDirect3DDevice9_CreateCubeTexture failed!");
			}
			else
			{
				if (FAILED(d3dresult = IDirect3DDevice9_CreateTexture(vid_d3d9dev, glt->tilewidth, glt->tileheight, glt->miplevels, glt->d3dusage, (D3DFORMAT)glt->d3dformat, (D3DPOOL)glt->d3dpool, (IDirect3DTexture9 **)&glt->d3dtexture, NULL)))
					Sys_Error("IDirect3DDevice9_CreateTexture failed!");
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		{
			int tflags = 0;
			switch(textype)
			{
			case TEXTYPE_PALETTE: tflags = DPSOFTRAST_TEXTURE_FORMAT_BGRA8;break;
			case TEXTYPE_RGBA: tflags = DPSOFTRAST_TEXTURE_FORMAT_RGBA8;break;
			case TEXTYPE_BGRA: tflags = DPSOFTRAST_TEXTURE_FORMAT_BGRA8;break;
			case TEXTYPE_COLORBUFFER: tflags = DPSOFTRAST_TEXTURE_FORMAT_BGRA8;break;
			case TEXTYPE_COLORBUFFER16F: tflags = DPSOFTRAST_TEXTURE_FORMAT_RGBA16F;break;
			case TEXTYPE_COLORBUFFER32F: tflags = DPSOFTRAST_TEXTURE_FORMAT_RGBA32F;break;
			case TEXTYPE_SHADOWMAP: tflags = DPSOFTRAST_TEXTURE_FORMAT_DEPTH;break;
			case TEXTYPE_ALPHA: tflags = DPSOFTRAST_TEXTURE_FORMAT_ALPHA8;break;
			default: Sys_Error("R_LoadTexture: unsupported texture type %i when picking DPSOFTRAST_TEXTURE_FLAGS", (int)textype);
			}
			if (glt->miplevels > 1) tflags |= DPSOFTRAST_TEXTURE_FLAG_MIPMAP;
			if (flags & TEXF_ALPHA) tflags |= DPSOFTRAST_TEXTURE_FLAG_USEALPHA;
			if (glt->sides == 6) tflags |= DPSOFTRAST_TEXTURE_FLAG_CUBEMAP;
			if (glt->flags & TEXF_CLAMP) tflags |= DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE;
			glt->texnum = DPSOFTRAST_Texture_New(tflags, glt->tilewidth, glt->tileheight, glt->tiledepth);
		}
		break;
	}

	R_UploadFullTexture(glt, data);
	if ((glt->flags & TEXF_ALLOWUPDATES) && gl_nopartialtextureupdates.integer)
		glt->bufferpixels = (unsigned char *)Mem_Alloc(texturemempool, glt->tilewidth*glt->tileheight*glt->tiledepth*glt->sides*glt->bytesperpixel);

	// free any temporary processing buffer we allocated...
	if (temppixels)
		Mem_Free(temppixels);

	// texture converting and uploading can take a while, so make sure we're sending keepalives
	// FIXME: this causes rendering during R_Shadow_DrawLights
//	CL_KeepaliveMessage(false);

	return (rtexture_t *)glt;
}

rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, flags, miplevel, textype, GLTEXTURETYPE_2D, data, palette);
}

rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, depth, 1, flags, miplevel, textype, GLTEXTURETYPE_3D, data, palette);
}

rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, width, 1, 6, flags, miplevel, textype, GLTEXTURETYPE_CUBEMAP, data, palette);
}

static int R_ShadowMapTextureFlags(int precision, qboolean filter)
{
	int flags = TEXF_RENDERTARGET | TEXF_CLAMP;
	if (filter)
		flags |= TEXF_FORCELINEAR | TEXF_COMPARE;
	else
		flags |= TEXF_FORCENEAREST;
	if (precision <= 16)
		flags |= TEXF_LOWPRECISION;
	return flags;
}

rtexture_t *R_LoadTextureShadowMap2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, R_ShadowMapTextureFlags(precision, filter), -1, TEXTYPE_SHADOWMAP, GLTEXTURETYPE_2D, NULL, NULL);
}

int R_SaveTextureDDSFile(rtexture_t *rt, const char *filename, qboolean skipuncompressed, qboolean hasalpha)
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
	// if premultiplied alpha, say so in the DDS file
	if(glt->flags & TEXF_RGBMULTIPLYBYALPHA)
	{
		switch(internalformat)
		{
			case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: ddsfourcc = "DXT2";break;
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: ddsfourcc = "DXT4";break;
		}
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
	dds = (unsigned char *)Mem_Alloc(tempmempool, ddssize);
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
		dds_format_flags = 0x40; // DDPF_RGB
	}
	if (mipmaps)
	{
		dds_flags |= 0x20000; // DDSD_MIPMAPCOUNT
		dds_caps1 |= 0x400008; // DDSCAPS_MIPMAP | DDSCAPS_COMPLEX
	}
	if(hasalpha)
		dds_format_flags |= 0x1; // DDPF_ALPHAPIXELS
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

rtexture_t *R_LoadTextureDDSFile(rtexturepool_t *rtexturepool, const char *filename, int flags, qboolean *hasalphaflag, float *avgcolor, int miplevel) // DDS textures are opaque, so miplevel isn't a pointer but just seen as a hint
{
	int i, size, dds_format_flags, dds_miplevels, dds_width, dds_height;
	//int dds_flags;
	textype_t textype;
	int bytesperblock, bytesperpixel;
	int mipcomplete;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo;
	int mip, mipwidth, mipheight, mipsize, mipsize_total;
	unsigned int c;
	GLint oldbindtexnum = 0;
	const unsigned char *mippixels, *ddspixels, *mippixels_start;
	unsigned char *dds;
	fs_offset_t ddsfilesize;
	unsigned int ddssize;
	qboolean force_swdecode = (r_texture_dds_swdecode.integer > 1);

	if (cls.state == ca_dedicated)
		return NULL;

	dds = FS_LoadFile(filename, tempmempool, true, &ddsfilesize);
	ddssize = ddsfilesize;

	if (!dds)
	{
		if(r_texture_dds_load_logfailure.integer)
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

	if(r_texture_dds_load_alphamode.integer == 0)
		if(!(dds_format_flags & 0x1)) // DDPF_ALPHAPIXELS
			flags &= ~TEXF_ALPHA;

	//flags &= ~TEXF_ALPHA; // disabled, as we DISABLE TEXF_ALPHA in the alpha detection, not enable it!
	if ((dds_format_flags & 0x40) && BuffLittleLong(dds+88) == 32)
	{
		// very sloppy BGRA 32bit identification
		textype = TEXTYPE_BGRA;
		flags &= ~TEXF_COMPRESS; // don't let the textype be wrong
		bytesperblock = 0;
		bytesperpixel = 4;
		size = INTOVERFLOW_MUL(INTOVERFLOW_MUL(dds_width, dds_height), bytesperpixel);
		if(INTOVERFLOW_ADD(128, size) > INTOVERFLOW_NORMALIZE(ddsfilesize))
		{
			Mem_Free(dds);
			Con_Printf("^1%s: invalid BGRA DDS image\n", filename);
			return NULL;
		}
		if((r_texture_dds_load_alphamode.integer == 1) && (flags & TEXF_ALPHA))
		{
			// check alpha
			for (i = 3;i < size;i += 4)
				if (ddspixels[i] < 255)
					break;
			if (i >= size)
				flags &= ~TEXF_ALPHA;
		}
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
		if(r_texture_dds_load_alphamode.integer && (flags & TEXF_ALPHA))
		{
			if(r_texture_dds_load_alphamode.integer == 1)
			{
				// check alpha
				for (i = 0;i < size;i += bytesperblock)
					if (ddspixels[i+0] + ddspixels[i+1] * 256 <= ddspixels[i+2] + ddspixels[i+3] * 256)
					{
						// NOTE: this assumes sizeof(unsigned int) == 4
						unsigned int data = * (unsigned int *) &(ddspixels[i+4]);
						// check if data, in base 4, contains a digit 3 (DXT1: transparent pixel)
						if(data & (data<<1) & 0xAAAAAAAA)//rgh
							break;
					}
				if (i < size)
					textype = TEXTYPE_DXT1A;
				else
					flags &= ~TEXF_ALPHA;
			}
			else
			{
				flags &= ~TEXF_ALPHA;
			}
		}
	}
	else if (!memcmp(dds+84, "DXT3", 4) || !memcmp(dds+84, "DXT2", 4))
	{
		if(!memcmp(dds+84, "DXT2", 4))
		{
			if(!(flags & TEXF_RGBMULTIPLYBYALPHA))
			{
				Con_Printf("^1%s: expecting DXT3 image without premultiplied alpha, got DXT2 image with premultiplied alpha\n", filename);
			}
		}
		else
		{
			if(flags & TEXF_RGBMULTIPLYBYALPHA)
			{
				Con_Printf("^1%s: expecting DXT2 image without premultiplied alpha, got DXT3 image without premultiplied alpha\n", filename);
			}
		}
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
		// we currently always assume alpha
	}
	else if (!memcmp(dds+84, "DXT5", 4) || !memcmp(dds+84, "DXT4", 4))
	{
		if(!memcmp(dds+84, "DXT4", 4))
		{
			if(!(flags & TEXF_RGBMULTIPLYBYALPHA))
			{
				Con_Printf("^1%s: expecting DXT5 image without premultiplied alpha, got DXT4 image with premultiplied alpha\n", filename);
			}
		}
		else
		{
			if(flags & TEXF_RGBMULTIPLYBYALPHA)
			{
				Con_Printf("^1%s: expecting DXT4 image without premultiplied alpha, got DXT5 image without premultiplied alpha\n", filename);
			}
		}
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
		// we currently always assume alpha
	}
	else
	{
		Mem_Free(dds);
		Con_Printf("^1%s: unrecognized/unsupported DDS format\n", filename);
		return NULL;
	}

	force_swdecode = false;
	if(bytesperblock)
	{
		if(vid.support.arb_texture_compression && vid.support.ext_texture_compression_s3tc)
		{
			if(r_texture_dds_swdecode.integer > 1)
				force_swdecode = true;
		}
		else
		{
			if(r_texture_dds_swdecode.integer < 1)
			{
				// unsupported
				Mem_Free(dds);
				return NULL;
			}
			force_swdecode = true;
		}
	}

	// return whether this texture is transparent
	if (hasalphaflag)
		*hasalphaflag = (flags & TEXF_ALPHA) != 0;

	// if we SW decode, choose 2 sizes bigger
	if(force_swdecode)
	{
		// this is quarter res, so do not scale down more than we have to
		miplevel -= 2;

		if(miplevel < 0)
			Con_DPrintf("WARNING: fake software decoding of compressed texture %s degraded quality\n", filename);
	}

	// this is where we apply gl_picmip
	mippixels_start = ddspixels;
	mipwidth = dds_width;
	mipheight = dds_height;
	while(miplevel >= 1 && dds_miplevels >= 1)
	{
		if (mipwidth <= 1 && mipheight <= 1)
			break;
		mipsize = bytesperblock ? ((mipwidth+3)/4)*((mipheight+3)/4)*bytesperblock : mipwidth*mipheight*bytesperpixel;
		mippixels_start += mipsize; // just skip
		--dds_miplevels;
		--miplevel;
		if (mipwidth > 1)
			mipwidth >>= 1;
		if (mipheight > 1)
			mipheight >>= 1;
	}
	mipsize_total = ddssize - 128 - (mippixels_start - ddspixels);
	mipsize = bytesperblock ? ((mipwidth+3)/4)*((mipheight+3)/4)*bytesperblock : mipwidth*mipheight*bytesperpixel;

	// from here on, we do not need the ddspixels and ddssize any more (apart from the statistics entry in glt)

	// fake decode S3TC if needed
	if(force_swdecode)
	{
		int mipsize_new = mipsize_total / bytesperblock * 4;
		unsigned char *mipnewpixels = (unsigned char *) Mem_Alloc(tempmempool, mipsize_new);
		unsigned char *p = mipnewpixels;
		for (i = bytesperblock == 16 ? 8 : 0;i < (int)mipsize_total;i += bytesperblock, p += 4)
		{
			c = mippixels_start[i] + 256*mippixels_start[i+1] + 65536*mippixels_start[i+2] + 16777216*mippixels_start[i+3];
			p[2] = (((c >> 11) & 0x1F) + ((c >> 27) & 0x1F)) * (0.5f / 31.0f * 255.0f);
			p[1] = (((c >>  5) & 0x3F) + ((c >> 21) & 0x3F)) * (0.5f / 63.0f * 255.0f);
			p[0] = (((c      ) & 0x1F) + ((c >> 16) & 0x1F)) * (0.5f / 31.0f * 255.0f);
			if(textype == TEXTYPE_DXT5)
				p[3] = (0.5 * mippixels_start[i-8] + 0.5 * mippixels_start[i-7]);
			else if(textype == TEXTYPE_DXT3)
				p[3] = (
					  (mippixels_start[i-8] & 0x0F)
					+ (mippixels_start[i-8] >> 4)
					+ (mippixels_start[i-7] & 0x0F)
					+ (mippixels_start[i-7] >> 4)
					+ (mippixels_start[i-6] & 0x0F)
					+ (mippixels_start[i-6] >> 4)
					+ (mippixels_start[i-5] & 0x0F)
					+ (mippixels_start[i-5] >> 4)
				       ) * (0.125f / 15.0f * 255.0f);
			else
				p[3] = 255;
		}

		textype = TEXTYPE_BGRA;
		bytesperblock = 0;
		bytesperpixel = 4;

		// as each block becomes a pixel, we must use pixel count for this
		mipwidth = (mipwidth + 3) / 4;
		mipheight = (mipheight + 3) / 4;
		mipsize = bytesperpixel * mipwidth * mipheight;
		mippixels_start = mipnewpixels;
		mipsize_total = mipsize_new;
	}

	// start mip counting
	mippixels = mippixels_start;

	// calculate average color if requested
	if (avgcolor)
	{
		float f;
		Vector4Clear(avgcolor);
		if (bytesperblock)
		{
			for (i = bytesperblock == 16 ? 8 : 0;i < mipsize;i += bytesperblock)
			{
				c = mippixels[i] + 256*mippixels[i+1] + 65536*mippixels[i+2] + 16777216*mippixels[i+3];
				avgcolor[0] += ((c >> 11) & 0x1F) + ((c >> 27) & 0x1F);
				avgcolor[1] += ((c >>  5) & 0x3F) + ((c >> 21) & 0x3F);
				avgcolor[2] += ((c      ) & 0x1F) + ((c >> 16) & 0x1F);
				if(textype == TEXTYPE_DXT5)
					avgcolor[3] += (0.5 * mippixels[i-8] + 0.5 * mippixels[i-7]);
				else if(textype == TEXTYPE_DXT3)
					avgcolor[3] += (
						  (mippixels_start[i-8] & 0x0F)
						+ (mippixels_start[i-8] >> 4)
						+ (mippixels_start[i-7] & 0x0F)
						+ (mippixels_start[i-7] >> 4)
						+ (mippixels_start[i-6] & 0x0F)
						+ (mippixels_start[i-6] >> 4)
						+ (mippixels_start[i-5] & 0x0F)
						+ (mippixels_start[i-5] >> 4)
					       ) * (0.125f / 15.0f * 255.0f);
				else
					avgcolor[3] += 255;
			}
			f = (float)bytesperblock / size;
			avgcolor[0] *= (0.5f / 31.0f) * f;
			avgcolor[1] *= (0.5f / 63.0f) * f;
			avgcolor[2] *= (0.5f / 31.0f) * f;
			avgcolor[3] *= f;
		}
		else
		{
			for (i = 0;i < mipsize;i += 4)
			{
				avgcolor[0] += mippixels[i+2];
				avgcolor[1] += mippixels[i+1];
				avgcolor[2] += mippixels[i];
				avgcolor[3] += mippixels[i+3];
			}
			f = (1.0f / 255.0f) * bytesperpixel / size;
			avgcolor[0] *= f;
			avgcolor[1] *= f;
			avgcolor[2] *= f;
			avgcolor[3] *= f;
		}
	}

	// when not requesting mipmaps, do not load them
	if(!(flags & TEXF_MIPMAP))
		dds_miplevels = 0;

	if (dds_miplevels >= 1)
		flags |= TEXF_MIPMAP;
	else
		flags &= ~TEXF_MIPMAP;

	texinfo = R_GetTexTypeInfo(textype, flags);

	glt = (gltexture_t *)Mem_ExpandableArray_AllocRecord(&texturearray);
	strlcpy (glt->identifier, filename, sizeof(glt->identifier));
	glt->pool = pool;
	glt->chain = pool->gltchain;
	pool->gltchain = glt;
	glt->inputwidth = mipwidth;
	glt->inputheight = mipheight;
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
	glt->tilewidth = mipwidth;
	glt->tileheight = mipheight;
	glt->tiledepth = 1;
	glt->miplevels = dds_miplevels;

	// texture uploading can take a while, so make sure we're sending keepalives
	CL_KeepaliveMessage(false);

	// create the texture object
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		GL_ActiveTexture(0);
		oldbindtexnum = R_Mesh_TexBound(0, gltexturetypeenums[glt->texturetype]);
		qglGenTextures(1, (GLuint *)&glt->texnum);CHECKGLERROR
		qglBindTexture(gltexturetypeenums[glt->texturetype], glt->texnum);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			D3DFORMAT d3dformat;
			D3DPOOL d3dpool;
			DWORD d3dusage;
			switch(textype)
			{
			case TEXTYPE_BGRA: d3dformat = (flags & TEXF_ALPHA) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;break;
			case TEXTYPE_DXT1: case TEXTYPE_DXT1A: d3dformat = D3DFMT_DXT1;break;
			case TEXTYPE_DXT3: d3dformat = D3DFMT_DXT3;break;
			case TEXTYPE_DXT5: d3dformat = D3DFMT_DXT5;break;
			default: d3dformat = D3DFMT_A8R8G8B8;Host_Error("R_LoadTextureDDSFile: unsupported texture type %i when picking D3DFMT", (int)textype);break;
			}
			d3dusage = 0;
			d3dpool = D3DPOOL_MANAGED;
			IDirect3DDevice9_CreateTexture(vid_d3d9dev, glt->tilewidth, glt->tileheight, glt->miplevels, d3dusage, d3dformat, d3dpool, (IDirect3DTexture9 **)&glt->d3dtexture, NULL);
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		glt->texnum = DPSOFTRAST_Texture_New(((glt->flags & TEXF_CLAMP) ? DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE : 0) | (dds_miplevels > 1 ? DPSOFTRAST_TEXTURE_FLAG_MIPMAP : 0), glt->tilewidth, glt->tileheight, glt->tiledepth);
		break;
	}

	// upload the texture
	// we need to restore the texture binding after finishing the upload
	mipcomplete = false;

	for (mip = 0;mip <= dds_miplevels;mip++) // <= to include the not-counted "largest" miplevel
	{
		mipsize = bytesperblock ? ((mipwidth+3)/4)*((mipheight+3)/4)*bytesperblock : mipwidth*mipheight*bytesperpixel;
		if (mippixels + mipsize > mippixels_start + mipsize_total)
			break;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			if (bytesperblock)
			{
				qglCompressedTexImage2DARB(GL_TEXTURE_2D, mip, glt->glinternalformat, mipwidth, mipheight, 0, mipsize, mippixels);CHECKGLERROR
			}
			else
			{
				qglTexImage2D(GL_TEXTURE_2D, mip, glt->glinternalformat, mipwidth, mipheight, 0, glt->glformat, glt->gltype, mippixels);CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			{
				D3DLOCKED_RECT d3dlockedrect;
				if (IDirect3DTexture9_LockRect((IDirect3DTexture9*)glt->d3dtexture, mip, &d3dlockedrect, NULL, 0) == D3D_OK && d3dlockedrect.pBits)
				{
					memcpy(d3dlockedrect.pBits, mippixels, mipsize);
					IDirect3DTexture9_UnlockRect((IDirect3DTexture9*)glt->d3dtexture, mip);
				}
				break;
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			if (bytesperblock)
				Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			else
				DPSOFTRAST_Texture_UpdateFull(glt->texnum, mippixels);
			// DPSOFTRAST calculates its own mipmaps
			mip = dds_miplevels;
			break;
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

	// after upload we have to set some parameters...
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (dds_miplevels >= 1 && !mipcomplete)
		{
			// need to set GL_TEXTURE_MAX_LEVEL
			qglTexParameteri(gltexturetypeenums[glt->texturetype], GL_TEXTURE_MAX_LEVEL, dds_miplevels - 1);CHECKGLERROR
		}
		GL_SetupTextureParameters(glt->flags, glt->textype->textype, glt->texturetype);
		qglBindTexture(gltexturetypeenums[glt->texturetype], oldbindtexnum);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		glt->d3daddressw = 0;
		if (glt->flags & TEXF_CLAMP)
		{
			glt->d3daddressu = D3DTADDRESS_CLAMP;
			glt->d3daddressv = D3DTADDRESS_CLAMP;
			if (glt->tiledepth > 1)
				glt->d3daddressw = D3DTADDRESS_CLAMP;
		}
		else
		{
			glt->d3daddressu = D3DTADDRESS_WRAP;
			glt->d3daddressv = D3DTADDRESS_WRAP;
			if (glt->tiledepth > 1)
				glt->d3daddressw = D3DTADDRESS_WRAP;
		}
		glt->d3dmipmaplodbias = 0;
		glt->d3dmaxmiplevel = 0;
		glt->d3dmaxmiplevelfilter = 0;
		if (glt->flags & TEXF_MIPMAP)
		{
			glt->d3dminfilter = d3d_filter_mipmin;
			glt->d3dmagfilter = d3d_filter_mipmag;
			glt->d3dmipfilter = d3d_filter_mipmix;
		}
		else
		{
			glt->d3dminfilter = d3d_filter_flatmin;
			glt->d3dmagfilter = d3d_filter_flatmag;
			glt->d3dmipfilter = d3d_filter_flatmix;
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (glt->flags & TEXF_FORCELINEAR)
			DPSOFTRAST_Texture_Filter(glt->texnum, DPSOFTRAST_TEXTURE_FILTER_LINEAR);
		else if (glt->flags & TEXF_FORCENEAREST)
			DPSOFTRAST_Texture_Filter(glt->texnum, DPSOFTRAST_TEXTURE_FILTER_NEAREST);
		else if (glt->flags & TEXF_MIPMAP)
			DPSOFTRAST_Texture_Filter(glt->texnum, dpsoftrast_filter_mipmap);
		else
			DPSOFTRAST_Texture_Filter(glt->texnum, dpsoftrast_filter_nomipmap);
		break;
	}

	Mem_Free(dds);
	if(force_swdecode)
		Mem_Free((unsigned char *) mippixels_start);
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

void R_UpdateTexture(rtexture_t *rt, const unsigned char *data, int x, int y, int z, int width, int height, int depth)
{
	gltexture_t *glt = (gltexture_t *)rt;
	if (data == NULL)
		Host_Error("R_UpdateTexture: no data supplied");
	if (glt == NULL)
		Host_Error("R_UpdateTexture: no texture supplied");
	if (!glt->texnum && !glt->d3dtexture)
	{
		Con_DPrintf("R_UpdateTexture: texture %p \"%s\" in pool %p has not been uploaded yet\n", (void *)glt, glt->identifier, (void *)glt->pool);
		return;
	}
	// update part of the texture
	if (glt->bufferpixels)
	{
		int j;
		int bpp = glt->bytesperpixel;
		int inputskip = width*bpp;
		int outputskip = glt->tilewidth*bpp;
		const unsigned char *input = data;
		unsigned char *output = glt->bufferpixels;
		if (glt->inputdepth != 1 || glt->sides != 1)
			Sys_Error("R_UpdateTexture on buffered texture that is not 2D\n");
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
	else if (x || y || z || width != glt->inputwidth || height != glt->inputheight || depth != glt->inputdepth)
		R_UploadPartialTexture(glt, data, x, y, z, width, height, depth);
	else
		R_UploadFullTexture(glt, data);
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
			R_UploadFullTexture(glt, glt->bufferpixels);
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

	R_UploadFullTexture(glt, NULL);
}

int R_PicmipForFlags(int flags)
{
	int miplevel = 0;
	if(flags & TEXF_PICMIP)
	{
		miplevel += gl_picmip.integer;
		if (flags & TEXF_ISWORLD)
		{
			if (r_picmipworld.integer)
				miplevel += gl_picmip_world.integer;
			else
				miplevel = 0;
		}
		else if (flags & TEXF_ISSPRITE)
		{
			if (r_picmipsprites.integer)
				miplevel += gl_picmip_sprites.integer;
			else
				miplevel = 0;
		}
		else
			miplevel += gl_picmip_other.integer;
	}
	return max(0, miplevel);
}
