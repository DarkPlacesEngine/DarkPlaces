
#define TEXF_ALPHA 1 // transparent
#define TEXF_MIPMAP 2 // mipmapped
#define TEXF_RGBA 4 // 32bit RGBA, as opposed to 8bit paletted
#define TEXF_PRECACHE 8 // upload immediately, otherwise defer loading until it is used (r_textureprecache can override this)
#define TEXF_ALWAYSPRECACHE 16 // upload immediately, never defer (ignore r_textureprecache)

// contents of this structure are private to gl_textures.c
typedef struct rtexture_s
{
	int useless;
}
rtexture_t;

// uploads a texture
extern rtexture_t *R_LoadTexture (char *identifier, int width, int height, byte *data, int flags);
// returns the renderer dependent texture slot number (call this before each use, as a texture might not have been precached)
extern int R_GetTexture (rtexture_t *rt);
// returns a GL texture slot (only used for lightmaps)
extern int R_GetTextureSlots(int count);
extern int R_TextureHasAlpha(rtexture_t *rt);
