
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
// allocated as a fragment in a larger texture, mipmap is not allowed with
// this, mostly used for lightmaps
#define TEXF_FRAGMENT 0x00000010
// used for checking if textures mismatch
#define TEXF_IMPORTANTBITS (TEXF_ALPHA | TEXF_MIPMAP | TEXF_FRAGMENT)

// 8bit quake paletted
#define TEXTYPE_QPALETTE 1
// 24bit RGB
#define TEXTYPE_RGB 2
// 32bit RGBA
#define TEXTYPE_RGBA 3

// contents of this structure are mostly private to gl_textures.c
typedef struct
{
	// this is exposed (rather than private) for speed reasons only
	int texnum;
}
rtexture_t;

// contents of this structure are private to gl_textures.c
typedef struct
{
	int useless;
}
rtexturepool_t;

// allocate a texture pool, to be used with R_LoadTexture
rtexturepool_t *R_AllocTexturePool(void);
// free a texture pool (textures can not be freed individually)
void R_FreeTexturePool(rtexturepool_t **rtexturepool);

// important technical note:
// fragment textures must have a width that is compatible with the fragment
// update system, to get a compliant size, use R_CompatibleFragmentWidth
int R_CompatibleFragmentWidth(int width, int textype, int flags);

// add a texture to a pool and optionally precache (upload) it
// (note: data == NULL is perfectly acceptable)
rtexture_t *R_LoadTexture (rtexturepool_t *rtexturepool, char *identifier, int width, int height, qbyte *data, int textype, int flags);

// free a texture
void R_FreeTexture(rtexture_t *rt);

// update the image data of a texture, used by lightmap updates and
// procedural textures.
void R_UpdateTexture(rtexture_t *rt, qbyte *data);

// location of the fragment in the texture (note: any parameter except rt can
// be NULL)
void R_FragmentLocation(rtexture_t *rt, int *x, int *y, float *fx1, float *fy1, float *fx2, float *fy2);

// returns the renderer dependent texture slot number (call this before each
// use, as a texture might not have been precached)
#define R_GetTexture(rt) ((rt) ? ((rt)->texnum >= 0 ? (rt)->texnum : R_RealGetTexture(rt)) : 0)
int R_RealGetTexture (rtexture_t *rt);

// returns true if the texture is transparent (useful for rendering code)
int R_TextureHasAlpha(rtexture_t *rt);

// returns width of texture, as was specified when it was uploaded
int R_TextureWidth(rtexture_t *rt);

// returns height of texture, as was specified when it was uploaded
int R_TextureHeight(rtexture_t *rt);

#endif

