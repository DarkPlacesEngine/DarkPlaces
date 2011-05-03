
#ifndef R_TEXTURES_H
#define R_TEXTURES_H

// transparent
#define TEXF_ALPHA 0x00000001
// mipmapped
#define TEXF_MIPMAP 0x00000002
// multiply RGB by A channel before uploading
#define TEXF_RGBMULTIPLYBYALPHA 0x00000004
// indicates texture coordinates should be clamped rather than wrapping
#define TEXF_CLAMP 0x00000020
// indicates texture should be uploaded using GL_NEAREST or GL_NEAREST_MIPMAP_NEAREST mode
#define TEXF_FORCENEAREST 0x00000040
// indicates texture should be uploaded using GL_LINEAR or GL_LINEAR_MIPMAP_NEAREST or GL_LINEAR_MIPMAP_LINEAR mode
#define TEXF_FORCELINEAR 0x00000080
// indicates texture should be affected by gl_picmip and gl_max_size cvar
#define TEXF_PICMIP 0x00000100
// indicates texture should be compressed if possible
#define TEXF_COMPRESS 0x00000200
// use this flag to block R_PurgeTexture from freeing a texture (only used by r_texture_white and similar which may be used in skinframe_t)
#define TEXF_PERSISTENT 0x00000400
// indicates texture should use GL_COMPARE_R_TO_TEXTURE mode
#define TEXF_COMPARE 0x00000800
// indicates texture should use lower precision where supported
#define TEXF_LOWPRECISION 0x00001000
// indicates texture should support R_UpdateTexture on small regions, actual uploads may be delayed until R_Mesh_TexBind if gl_nopartialtextureupdates is on
#define TEXF_ALLOWUPDATES 0x00002000
// indicates texture should be affected by gl_picmip_world and r_picmipworld (maybe others in the future) instead of gl_picmip_other
#define TEXF_ISWORLD 0x00004000
// indicates texture should be affected by gl_picmip_sprites and r_picmipsprites (maybe others in the future) instead of gl_picmip_other
#define TEXF_ISSPRITE 0x00008000
// indicates the texture will be used as a render target (D3D hint)
#define TEXF_RENDERTARGET 0x0010000
// used for checking if textures mismatch
#define TEXF_IMPORTANTBITS (TEXF_ALPHA | TEXF_MIPMAP | TEXF_RGBMULTIPLYBYALPHA | TEXF_CLAMP | TEXF_FORCENEAREST | TEXF_FORCELINEAR | TEXF_PICMIP | TEXF_COMPRESS | TEXF_COMPARE | TEXF_LOWPRECISION | TEXF_RENDERTARGET)

typedef enum textype_e
{
	// 8bit paletted
	TEXTYPE_PALETTE,
	// 32bit RGBA
	TEXTYPE_RGBA,
	// 32bit BGRA (preferred format due to faster uploads on most hardware)
	TEXTYPE_BGRA,
	// 8bit ALPHA (used for freetype fonts)
	TEXTYPE_ALPHA,
	// 4x4 block compressed 15bit color (4 bits per pixel)
	TEXTYPE_DXT1,
	// 4x4 block compressed 15bit color plus 1bit alpha (4 bits per pixel)
	TEXTYPE_DXT1A,
	// 4x4 block compressed 15bit color plus 8bit alpha (8 bits per pixel)
	TEXTYPE_DXT3,
	// 4x4 block compressed 15bit color plus 8bit alpha (8 bits per pixel)
	TEXTYPE_DXT5,

	// 8bit paletted in sRGB colorspace
	TEXTYPE_SRGB_PALETTE,
	// 32bit RGBA in sRGB colorspace
	TEXTYPE_SRGB_RGBA,
	// 32bit BGRA (preferred format due to faster uploads on most hardware) in sRGB colorspace
	TEXTYPE_SRGB_BGRA,
	// 4x4 block compressed 15bit color (4 bits per pixel) in sRGB colorspace
	TEXTYPE_SRGB_DXT1,
	// 4x4 block compressed 15bit color plus 1bit alpha (4 bits per pixel) in sRGB colorspace
	TEXTYPE_SRGB_DXT1A,
	// 4x4 block compressed 15bit color plus 8bit alpha (8 bits per pixel) in sRGB colorspace
	TEXTYPE_SRGB_DXT3,
	// 4x4 block compressed 15bit color plus 8bit alpha (8 bits per pixel) in sRGB colorspace
	TEXTYPE_SRGB_DXT5,

	// this represents the same format as the framebuffer, for fast copies
	TEXTYPE_COLORBUFFER,
	// this represents an RGBA half_float texture (4 16bit floats)
	TEXTYPE_COLORBUFFER16F,
	// this represents an RGBA float texture (4 32bit floats)
	TEXTYPE_COLORBUFFER32F,
	// 16bit D16 (16bit depth) or 32bit S8D24 (24bit depth, 8bit stencil unused)
	TEXTYPE_SHADOWMAP
}
textype_t;

/*
#ifdef WIN32
#define SUPPORTD3D
#define SUPPORTDIRECTX
#ifdef SUPPORTD3D
#include <d3d9.h>
#endif
#endif
*/

// contents of this structure are mostly private to gl_textures.c
typedef struct rtexture_s
{
	// this is exposed (rather than private) for speed reasons only
	int texnum;
	qboolean dirty;
	int gltexturetypeenum; // exposed for use in R_Mesh_TexBind
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
}
rtexture_t;

// contents of this structure are private to gl_textures.c
typedef struct rtexturepool_s
{
	int useless;
}
rtexturepool_t;

typedef void (*updatecallback_t)(rtexture_t *rt, void *data);

// allocate a texture pool, to be used with R_LoadTexture
rtexturepool_t *R_AllocTexturePool(void);
// free a texture pool (textures can not be freed individually)
void R_FreeTexturePool(rtexturepool_t **rtexturepool);

// the color/normal/etc cvars should be checked by callers of R_LoadTexture* functions to decide whether to add TEXF_COMPRESS to the flags
extern cvar_t gl_texturecompression;
extern cvar_t gl_texturecompression_color;
extern cvar_t gl_texturecompression_normal;
extern cvar_t gl_texturecompression_gloss;
extern cvar_t gl_texturecompression_glow;
extern cvar_t gl_texturecompression_2d;
extern cvar_t gl_texturecompression_q3bsplightmaps;
extern cvar_t gl_texturecompression_q3bspdeluxemaps;
extern cvar_t gl_texturecompression_sky;
extern cvar_t gl_texturecompression_lightcubemaps;
extern cvar_t gl_texturecompression_reflectmask;
extern cvar_t r_texture_dds_load;
extern cvar_t r_texture_dds_save;

// add a texture to a pool and optionally precache (upload) it
// (note: data == NULL is perfectly acceptable)
// (note: palette must not be NULL if using TEXTYPE_PALETTE)
rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette);
rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette);
rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, textype_t textype, int flags, int miplevel, const unsigned int *palette);
rtexture_t *R_LoadTextureShadowMap2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter);
rtexture_t *R_LoadTextureDDSFile(rtexturepool_t *rtexturepool, const char *filename, int flags, qboolean *hasalphaflag, float *avgcolor, int miplevel);

// saves a texture to a DDS file
int R_SaveTextureDDSFile(rtexture_t *rt, const char *filename, qboolean skipuncompressed, qboolean hasalpha);

// free a texture
void R_FreeTexture(rtexture_t *rt);

// update a portion of the image data of a texture, used by lightmap updates
// and procedural textures such as video playback, actual uploads may be
// delayed by gl_nopartialtextureupdates cvar until R_Mesh_TexBind uses it
void R_UpdateTexture(rtexture_t *rt, const unsigned char *data, int x, int y, int z, int width, int height, int depth);

// returns the renderer dependent texture slot number (call this before each
// use, as a texture might not have been precached)
#define R_GetTexture(rt) ((rt) ? ((rt)->dirty ? R_RealGetTexture(rt) : (rt)->texnum) : r_texture_white->texnum)
int R_RealGetTexture (rtexture_t *rt);

// returns width of texture, as was specified when it was uploaded
int R_TextureWidth(rtexture_t *rt);

// returns height of texture, as was specified when it was uploaded
int R_TextureHeight(rtexture_t *rt);

// only frees the texture if TEXF_PERSISTENT is not set
// also resets the variable
void R_PurgeTexture(rtexture_t *prt);

// frees processing buffers each frame, and may someday animate procedural textures
void R_Textures_Frame(void);

// maybe rename this - sounds awful? [11/21/2007 Black]
void R_MarkDirtyTexture(rtexture_t *rt);
void R_MakeTextureDynamic(rtexture_t *rt, updatecallback_t updatecallback, void *data);

// Clear the texture's contents
void R_ClearTexture (rtexture_t *rt);

// returns the desired picmip level for given TEXF_ flags
int R_PicmipForFlags(int flags);

#endif

