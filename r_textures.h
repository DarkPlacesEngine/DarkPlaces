
#ifndef R_TEXTURES_H
#define R_TEXTURES_H

// transparent
#define TEXF_ALPHA 0x00000001
// mipmapped
#define TEXF_MIPMAP 0x00000002
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
// indicates texture should support R_UpdateTexture
#define TEXF_ALLOWUPDATES 0x00002000
// indicates texture should support R_FlushTexture (improving speed on multiple partial updates per draw)
#define TEXF_MANUALFLUSHUPDATES 0x00004000
// used for checking if textures mismatch
#define TEXF_IMPORTANTBITS (TEXF_ALPHA | TEXF_MIPMAP | TEXF_CLAMP | TEXF_FORCENEAREST | TEXF_FORCELINEAR | TEXF_PICMIP | TEXF_COMPRESS | TEXF_COMPARE | TEXF_LOWPRECISION)

typedef enum textype_e
{
	// 8bit paletted
	TEXTYPE_PALETTE,
	// 32bit RGBA
	TEXTYPE_RGBA,
	// 32bit BGRA (preferred format due to faster uploads on most hardware)
	TEXTYPE_BGRA,
	// 16bit D16 (16bit depth) or 32bit S8D24 (24bit depth, 8bit stencil unused)
	TEXTYPE_SHADOWMAP,
	// 8bit ALPHA (used for freetype fonts)
	TEXTYPE_ALPHA,
}
textype_t;

// contents of this structure are mostly private to gl_textures.c
typedef struct rtexture_s
{
	// this is exposed (rather than private) for speed reasons only
	int texnum;
	qboolean dirty;
	int gltexturetypeenum; // exposed for use in R_Mesh_TexBind
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

// add a texture to a pool and optionally precache (upload) it
// (note: data == NULL is perfectly acceptable)
// (note: palette must not be NULL if using TEXTYPE_PALETTE)
rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTextureRectangle(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, textype_t textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTextureShadowMapRectangle(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter);
rtexture_t *R_LoadTextureShadowMap2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int precision, qboolean filter);
rtexture_t *R_LoadTextureShadowMapCube(rtexturepool_t *rtexturepool, const char *identifier, int width, int precision, qboolean filter);

// free a texture
void R_FreeTexture(rtexture_t *rt);

// update a portion of the image data of a texture, used by lightmap updates
// and procedural textures such as video playback.
// if TEXF_MANUALFLUSHUPDATES is used, you MUST call R_FlushTexture to apply the updates
void R_UpdateTexture(rtexture_t *rt, const unsigned char *data, int x, int y, int width, int height);
// if TEXF_MANUALFLUSHUPDATES is used, call this to apply the updates,
// otherwise this function does nothing
void R_FlushTexture(rtexture_t *rt);

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

#endif

