
#ifndef R_TEXTURES_H
#define R_TEXTURES_H

// transparent
#define TEXF_ALPHA 0x00000001
// mipmapped
#define TEXF_MIPMAP 0x00000002
// upload if r_textureprecache >= 1, otherwise defer loading until it is used
#define TEXF_PRECACHE 0x00000004
// upload immediately, never defer (ignore r_textureprecache)
#define TEXF_ALWAYSPRECACHE 0x00000008
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
// used for checking if textures mismatch
#define TEXF_IMPORTANTBITS (TEXF_ALPHA | TEXF_MIPMAP | TEXF_CLAMP | TEXF_FORCENEAREST | TEXF_FORCELINEAR | TEXF_PICMIP | TEXF_COMPRESS)

// 8bit paletted
#define TEXTYPE_PALETTE 1
// 32bit RGBA
#define TEXTYPE_RGBA 3
// 32bit BGRA (preferred format due to faster uploads on most hardware)
#define TEXTYPE_BGRA 4

// contents of this structure are mostly private to gl_textures.c
typedef struct rtexture_s
{
	// this is exposed (rather than private) for speed reasons only
	int texnum;
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
rtexture_t *R_LoadTexture1D(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, int textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const unsigned char *data, int textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const unsigned char *data, int textype, int flags, const unsigned int *palette);
rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const unsigned char *data, int textype, int flags, const unsigned int *palette);

// free a texture
void R_FreeTexture(rtexture_t *rt);

// update a portion of the image data of a texture, used by lightmap updates
// and procedural textures such as video playback.
void R_UpdateTexture(rtexture_t *rt, unsigned char *data, int x, int y, int width, int height);

// returns the renderer dependent texture slot number (call this before each
// use, as a texture might not have been precached)
#define R_GetTexture(rt) ((rt) ? ((rt)->texnum >= 0 ? (rt)->texnum : R_RealGetTexture(rt)) : r_texture_white->texnum)
int R_RealGetTexture (rtexture_t *rt);

// returns true if the texture is transparent (useful for rendering code)
int R_TextureHasAlpha(rtexture_t *rt);

// returns width of texture, as was specified when it was uploaded
int R_TextureWidth(rtexture_t *rt);

// returns height of texture, as was specified when it was uploaded
int R_TextureHeight(rtexture_t *rt);

// frees processing buffers each frame, and may someday animate procedural textures
void R_Textures_Frame(void);

// maybe rename this - sounds awful? [11/21/2007 Black]
void R_MarkDirtyTexture(rtexture_t *rt);
void R_MakeTextureDynamic(rtexture_t *rt, updatecallback_t updatecallback, void *data); 

#endif

