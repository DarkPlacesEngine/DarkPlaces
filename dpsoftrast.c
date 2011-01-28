
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "quakedef.h"
#include "dpsoftrast.h"

#ifndef __cplusplus
typedef qboolean bool;
#endif

#if defined(__GNUC__)
#define ALIGN(var) var __attribute__((__aligned__(16)))
#elif defined(_MSC_VER)
#define ALIGN(var) __declspec(align(16)) var
#else
#define ALIGN(var) var
#endif

#ifdef SSE2_PRESENT
#include <emmintrin.h>

#define MM_MALLOC(size) _mm_malloc(size, 16)

static void *MM_CALLOC(size_t nmemb, size_t size)
{
	void *ptr = _mm_malloc(nmemb*size, 16);
	if(ptr != NULL) memset(ptr, 0, nmemb*size);
	return ptr;
}

#define MM_FREE _mm_free
#else
#define MM_MALLOC(size) malloc(size)
#define MM_CALLOC(nmemb, size) calloc(nmemb, size)
#define MM_FREE free
#endif

typedef enum DPSOFTRAST_ARRAY_e
{
	DPSOFTRAST_ARRAY_POSITION,
	DPSOFTRAST_ARRAY_COLOR,
	DPSOFTRAST_ARRAY_TEXCOORD0,
	DPSOFTRAST_ARRAY_TEXCOORD1,
	DPSOFTRAST_ARRAY_TEXCOORD2,
	DPSOFTRAST_ARRAY_TEXCOORD3,
	DPSOFTRAST_ARRAY_TEXCOORD4,
	DPSOFTRAST_ARRAY_TEXCOORD5,
	DPSOFTRAST_ARRAY_TEXCOORD6,
	DPSOFTRAST_ARRAY_TEXCOORD7,
	DPSOFTRAST_ARRAY_TOTAL
}
DPSOFTRAST_ARRAY;

typedef struct DPSOFTRAST_Texture_s
{
	int flags;
	int width;
	int height;
	int depth;
	int sides;
	DPSOFTRAST_TEXTURE_FILTER filter;
	int mipmaps;
	int size;
	unsigned char *bytes;
	int mipmap[DPSOFTRAST_MAXMIPMAPS][5];
}
DPSOFTRAST_Texture;

typedef struct DPSOFTRAST_State_User_s
{
	int colormask[4];
	int blendfunc[2];
	int blendsubtract;
	int depthmask;
	int depthtest;
	int depthfunc;
	int scissortest;
	int cullface;
	int alphatest;
	int alphafunc;
	float alphavalue;
	int scissor[4];
	int viewport[4];
	float depthrange[2];
	float polygonoffset[2];
	float color[4];
}
DPSOFTRAST_State_User;

#define DPSOFTRAST_MAXSUBSPAN 16

typedef ALIGN(struct DPSOFTRAST_State_Draw_Span_s
{
	int start; // pixel index
	int length; // pixel count
	int startx; // usable range (according to pixelmask)
	int endx; // usable range (according to pixelmask)
	unsigned char mip[DPSOFTRAST_MAXTEXTUREUNITS]; // texcoord to screen space density values (for picking mipmap of textures)
	unsigned char *pixelmask; // true for pixels that passed depth test, false for others
	// [0][n][] is start interpolant values (projected)
	// [1][n][] is end interpolant values (projected)
	// [0][DPSOFTRAST_ARRAY_TOTAL][] is start screencoord4f
	// [1][DPSOFTRAST_ARRAY_TOTAL][] is end screencoord4f
	// NOTE: screencoord4f[3] is W (basically 1/Z), useful for depthbuffer
	ALIGN(float data[2][DPSOFTRAST_ARRAY_TOTAL+1][4]);
}
DPSOFTRAST_State_Draw_Span);

#define DPSOFTRAST_DRAW_MAXSPANQUEUE 1024

typedef struct DPSOFTRAST_State_Draw_s
{
	int numvertices;
	int maxvertices;
	float *in_array4f[DPSOFTRAST_ARRAY_TOTAL];
	float *post_array4f[DPSOFTRAST_ARRAY_TOTAL];
	float *screencoord4f;

	// spans are queued in this structure for dispatch to the pixel shader,
	// partly to improve cache locality, partly for batching purposes, spans
	// are flushed before DrawTriangles returns to caller
	int numspans;
	DPSOFTRAST_State_Draw_Span spanqueue[DPSOFTRAST_DRAW_MAXSPANQUEUE];
}
DPSOFTRAST_State_Draw;

#define DPSOFTRAST_VALIDATE_FB 1
#define DPSOFTRAST_VALIDATE_DEPTHFUNC 2
#define DPSOFTRAST_VALIDATE_BLENDFUNC 4
#define DPSOFTRAST_VALIDATE_DRAW (DPSOFTRAST_VALIDATE_FB | DPSOFTRAST_VALIDATE_DEPTHFUNC | DPSOFTRAST_VALIDATE_BLENDFUNC)

typedef enum DPSOFTRAST_BLENDMODE_e
{
	DPSOFTRAST_BLENDMODE_OPAQUE,
	DPSOFTRAST_BLENDMODE_ALPHA,
	DPSOFTRAST_BLENDMODE_ADDALPHA,
	DPSOFTRAST_BLENDMODE_ADD,
	DPSOFTRAST_BLENDMODE_INVMOD,
	DPSOFTRAST_BLENDMODE_MUL,
	DPSOFTRAST_BLENDMODE_MUL2,
	DPSOFTRAST_BLENDMODE_SUBALPHA,
	DPSOFTRAST_BLENDMODE_PSEUDOALPHA,
	DPSOFTRAST_BLENDMODE_TOTAL
}
DPSOFTRAST_BLENDMODE;

typedef ALIGN(struct DPSOFTRAST_State_s
{
	// DPSOFTRAST_VALIDATE_ flags
	int validate;

	int fb_colormask;
	int fb_width;
	int fb_height;
	unsigned int *fb_depthpixels;
	unsigned int *fb_colorpixels[4];

	const float *pointer_vertex3f;
	const float *pointer_color4f;
	const unsigned char *pointer_color4ub;
	const float *pointer_texcoordf[DPSOFTRAST_MAXTEXCOORDARRAYS];
	int stride_vertex;
	int stride_color;
	int stride_texcoord[DPSOFTRAST_MAXTEXCOORDARRAYS];
	int components_texcoord[DPSOFTRAST_MAXTEXCOORDARRAYS];
	DPSOFTRAST_Texture *texbound[DPSOFTRAST_MAXTEXTUREUNITS];

	int shader_mode;
	int shader_permutation;
	ALIGN(float uniform4f[DPSOFTRAST_UNIFORM_TOTAL*4]);
	int uniform1i[DPSOFTRAST_UNIFORM_TOTAL];

	// derived values (DPSOFTRAST_VALIDATE_FB)
	int fb_clearscissor[4];
	int fb_viewport[4];
	int fb_viewportscissor[4];
	ALIGN(float fb_viewportcenter[4]);
	ALIGN(float fb_viewportscale[4]);

	// derived values (DPSOFTRAST_VALIDATE_DEPTHFUNC)
	int fb_depthfunc;

	// derived values (DPSOFTRAST_VALIDATE_BLENDFUNC)
	int fb_blendmode;

	int texture_max;
	int texture_end;
	int texture_firstfree;
	DPSOFTRAST_Texture *texture;

	int bigendian;

	// error reporting
	const char *errorstring;

	DPSOFTRAST_State_User user;

	DPSOFTRAST_State_Draw draw;
}
DPSOFTRAST_State);

DPSOFTRAST_State dpsoftrast;

extern int dpsoftrast_test;

#define DPSOFTRAST_DEPTHSCALE (1024.0f*1048576.0f)
#define DPSOFTRAST_DEPTHOFFSET (128.0f)
#define DPSOFTRAST_BGRA8_FROM_RGBA32F(r,g,b,a) (((int)(r * 255.0f + 0.5f) << 16) | ((int)(g * 255.0f + 0.5f) << 8) | (int)(b * 255.0f + 0.5f) | ((int)(a * 255.0f + 0.5f) << 24))
#define DPSOFTRAST_DEPTH32_FROM_DEPTH32F(d) ((int)(DPSOFTRAST_DEPTHSCALE * (1-d)))
#define DPSOFTRAST_DRAW_MAXSPANLENGTH 256

void DPSOFTRAST_RecalcFB(void)
{
	// calculate framebuffer scissor, viewport, viewport clipped by scissor,
	// and viewport projection values
	int x1, x2, x3, x4, x5, x6;
	int y1, y2, y3, y4, y5, y6;
	x1 = dpsoftrast.user.scissor[0];
	x2 = dpsoftrast.user.scissor[0] + dpsoftrast.user.scissor[2];
	x3 = dpsoftrast.user.viewport[0];
	x4 = dpsoftrast.user.viewport[0] + dpsoftrast.user.viewport[2];
	y1 = dpsoftrast.fb_height - dpsoftrast.user.scissor[1] - dpsoftrast.user.scissor[3];
	y2 = dpsoftrast.fb_height - dpsoftrast.user.scissor[1];
	y3 = dpsoftrast.fb_height - dpsoftrast.user.viewport[1] - dpsoftrast.user.viewport[3];
	y4 = dpsoftrast.fb_height - dpsoftrast.user.viewport[1];
	if (!dpsoftrast.user.scissortest) {x1 = 0;y1 = 0;x2 = dpsoftrast.fb_width;y2 = dpsoftrast.fb_height;}
	if (x1 < 0) x1 = 0;
	if (x2 > dpsoftrast.fb_width) x2 = dpsoftrast.fb_width;
	if (x3 < 0) x1 = 0;
	if (x4 > dpsoftrast.fb_width) x4 = dpsoftrast.fb_width;
	if (y1 < 0) y1 = 0;
	if (y2 > dpsoftrast.fb_height) y2 = dpsoftrast.fb_height;
	if (y3 < 0) y1 = 0;
	if (y4 > dpsoftrast.fb_height) y4 = dpsoftrast.fb_height;
	x5 = x1;if (x5 < x3) x5 = x3;
	x6 = x2;if (x6 > x4) x4 = x4;
	y5 = y1;if (y5 < y3) y5 = y3;
	y6 = y2;if (y6 > y4) y6 = y4;
	dpsoftrast.fb_clearscissor[0] = x1;
	dpsoftrast.fb_clearscissor[1] = y1;
	dpsoftrast.fb_clearscissor[2] = x2 - x1;
	dpsoftrast.fb_clearscissor[3] = y2 - y1;
	dpsoftrast.fb_viewport[0] = x3;
	dpsoftrast.fb_viewport[1] = y3;
	dpsoftrast.fb_viewport[2] = x4 - x3;
	dpsoftrast.fb_viewport[3] = y4 - y3;
	dpsoftrast.fb_viewportscissor[0] = x5;
	dpsoftrast.fb_viewportscissor[1] = y5;
	dpsoftrast.fb_viewportscissor[2] = x6 - x5;
	dpsoftrast.fb_viewportscissor[3] = y6 - y5;
	dpsoftrast.fb_viewportcenter[1] = dpsoftrast.user.viewport[0] + 0.5f * dpsoftrast.user.viewport[2] - 0.5f;
	dpsoftrast.fb_viewportcenter[2] = dpsoftrast.fb_height - dpsoftrast.user.viewport[1] - 0.5f * dpsoftrast.user.viewport[3] - 0.5f;
	dpsoftrast.fb_viewportcenter[3] = 0.5f;
	dpsoftrast.fb_viewportcenter[0] = 0.0f;
	dpsoftrast.fb_viewportscale[1] = 0.5f * dpsoftrast.user.viewport[2];
	dpsoftrast.fb_viewportscale[2] = -0.5f * dpsoftrast.user.viewport[3];
	dpsoftrast.fb_viewportscale[3] = 0.5f;
	dpsoftrast.fb_viewportscale[0] = 1.0f;
}

void DPSOFTRAST_RecalcDepthFunc(void)
{
	dpsoftrast.fb_depthfunc = dpsoftrast.user.depthtest ? dpsoftrast.user.depthfunc : GL_ALWAYS;
}

int blendmodetable[][4] = 
{
	{DPSOFTRAST_BLENDMODE_OPAQUE, GL_ONE, GL_ZERO, false},
	{DPSOFTRAST_BLENDMODE_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, false},
	{DPSOFTRAST_BLENDMODE_ADDALPHA, GL_SRC_ALPHA, GL_ONE, false},
	{DPSOFTRAST_BLENDMODE_ADD, GL_ONE, GL_ONE, false},
	{DPSOFTRAST_BLENDMODE_INVMOD, GL_ZERO, GL_ONE_MINUS_SRC_COLOR, false},
	{DPSOFTRAST_BLENDMODE_MUL, GL_ZERO, GL_SRC_COLOR, false},
	{DPSOFTRAST_BLENDMODE_MUL, GL_DST_COLOR, GL_ZERO, false},
	{DPSOFTRAST_BLENDMODE_MUL2, GL_DST_COLOR, GL_SRC_COLOR, false},
	{DPSOFTRAST_BLENDMODE_PSEUDOALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, false},
	{DPSOFTRAST_BLENDMODE_SUBALPHA, GL_SRC_COLOR, GL_ONE, true}
};

void DPSOFTRAST_RecalcBlendFunc(void)
{
	int i;
	dpsoftrast.fb_blendmode = DPSOFTRAST_BLENDMODE_OPAQUE;
	for (i = 0;i < (int)(sizeof(blendmodetable) / sizeof(blendmodetable[0]));i++)
	{
		if (dpsoftrast.user.blendfunc[0] == blendmodetable[i][1] && dpsoftrast.user.blendfunc[1] == blendmodetable[i][2] && dpsoftrast.user.blendsubtract == blendmodetable[i][3])
		{
			dpsoftrast.fb_blendmode = blendmodetable[i][0];
			break;
		}
	}
}

#define DPSOFTRAST_ValidateQuick(f) ((dpsoftrast.validate & (f)) ? (DPSOFTRAST_Validate(f), 0) : 0)

void DPSOFTRAST_Validate(int mask)
{
	mask &= dpsoftrast.validate;
	if (!mask)
		return;
	if (mask & DPSOFTRAST_VALIDATE_FB)
	{
		dpsoftrast.validate &= ~DPSOFTRAST_VALIDATE_FB;
		DPSOFTRAST_RecalcFB();
	}
	if (mask & DPSOFTRAST_VALIDATE_DEPTHFUNC)
	{
		dpsoftrast.validate &= ~DPSOFTRAST_VALIDATE_DEPTHFUNC;
		DPSOFTRAST_RecalcDepthFunc();
	}
	if (mask & DPSOFTRAST_VALIDATE_BLENDFUNC)
	{
		dpsoftrast.validate &= ~DPSOFTRAST_VALIDATE_BLENDFUNC;
		DPSOFTRAST_RecalcBlendFunc();
	}
}

DPSOFTRAST_Texture *DPSOFTRAST_Texture_GetByIndex(int index)
{
	if (index >= 1 && index < dpsoftrast.texture_end && dpsoftrast.texture[index].bytes)
		return &dpsoftrast.texture[index];
	return NULL;
}

int DPSOFTRAST_Texture_New(int flags, int width, int height, int depth)
{
	int w;
	int h;
	int d;
	int size;
	int s;
	int texnum;
	int mipmaps;
	int sides = (flags & DPSOFTRAST_TEXTURE_FLAG_CUBEMAP) ? 6 : 1;
	int texformat = flags & DPSOFTRAST_TEXTURE_FORMAT_COMPAREMASK;
	DPSOFTRAST_Texture *texture;
	if (width*height*depth < 1)
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: width, height or depth is less than 1";
		return 0;
	}
	if (width > DPSOFTRAST_TEXTURE_MAXSIZE || height > DPSOFTRAST_TEXTURE_MAXSIZE || depth > DPSOFTRAST_TEXTURE_MAXSIZE)
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: texture size is too large";
		return 0;
	}
	switch(texformat)
	{
	case DPSOFTRAST_TEXTURE_FORMAT_BGRA8:
	case DPSOFTRAST_TEXTURE_FORMAT_RGBA8:
	case DPSOFTRAST_TEXTURE_FORMAT_ALPHA8:
		break;
	case DPSOFTRAST_TEXTURE_FORMAT_DEPTH:
		if (flags & DPSOFTRAST_TEXTURE_FLAG_CUBEMAP)
		{
			dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FORMAT_DEPTH only permitted on 2D textures";
			return 0;
		}
		if (depth != 1)
		{
			dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FORMAT_DEPTH only permitted on 2D textures";
			return 0;
		}
		if ((flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP) && (texformat == DPSOFTRAST_TEXTURE_FORMAT_DEPTH))
		{
			dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FORMAT_DEPTH does not permit mipmaps";
			return 0;
		}
		break;
	}
	if (depth != 1 && (flags & DPSOFTRAST_TEXTURE_FLAG_CUBEMAP))
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FLAG_CUBEMAP can not be used on 3D textures";
		return 0;
	}
	if (depth != 1 && (flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP))
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FLAG_MIPMAP can not be used on 3D textures";
		return 0;
	}
	if (depth != 1 && (flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP))
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FLAG_MIPMAP can not be used on 3D textures";
		return 0;
	}
	if ((flags & DPSOFTRAST_TEXTURE_FLAG_CUBEMAP) && (flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP))
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: DPSOFTRAST_TEXTURE_FLAG_MIPMAP can not be used on cubemap textures";
		return 0;
	}
	if ((width & (width-1)) || (height & (height-1)) || (depth & (depth-1)))
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_New: dimensions are not power of two";
		return 0;
	}
	// find first empty slot in texture array
	for (texnum = dpsoftrast.texture_firstfree;texnum < dpsoftrast.texture_end;texnum++)
		if (!dpsoftrast.texture[texnum].bytes)
			break;
	dpsoftrast.texture_firstfree = texnum + 1;
	if (dpsoftrast.texture_max <= texnum)
	{
		// expand texture array as needed
		if (dpsoftrast.texture_max < 1024)
			dpsoftrast.texture_max = 1024;
		else
			dpsoftrast.texture_max *= 2;
		dpsoftrast.texture = (DPSOFTRAST_Texture *)realloc(dpsoftrast.texture, dpsoftrast.texture_max * sizeof(DPSOFTRAST_Texture));
	}
	if (dpsoftrast.texture_end <= texnum)
		dpsoftrast.texture_end = texnum + 1;
	texture = &dpsoftrast.texture[texnum];
	memset(texture, 0, sizeof(*texture));
	texture->flags = flags;
	texture->width = width;
	texture->height = height;
	texture->depth = depth;
	texture->sides = sides;
	w = width;
	h = height;
	d = depth;
	size = 0;
	mipmaps = 0;
	w = width;
	h = height;
	d = depth;
	for (;;)
	{
		s = w * h * d * sides * 4;
		texture->mipmap[mipmaps][0] = size;
		texture->mipmap[mipmaps][1] = s;
		texture->mipmap[mipmaps][2] = w;
		texture->mipmap[mipmaps][3] = h;
		texture->mipmap[mipmaps][4] = d;
		size += s;
		mipmaps++;
		if (w * h * d == 1 || !(flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP))
			break;
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
		if (d > 1) d >>= 1;
	}
	texture->mipmaps = mipmaps;
	texture->size = size;

	// allocate the pixels now
	texture->bytes = (unsigned char *)MM_CALLOC(1, size);

	return texnum;
}
void DPSOFTRAST_Texture_Free(int index)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (texture->bytes)
		MM_FREE(texture->bytes);
	texture->bytes = NULL;
	memset(texture, 0, sizeof(*texture));
	// adjust the free range and used range
	if (dpsoftrast.texture_firstfree > index)
		dpsoftrast.texture_firstfree = index;
	while (dpsoftrast.texture_end > 0 && dpsoftrast.texture[dpsoftrast.texture_end-1].bytes == NULL)
		dpsoftrast.texture_end--;
}
void DPSOFTRAST_Texture_CalculateMipmaps(int index)
{
	int i, x, y, z, w, layer0, layer1, row0, row1;
	unsigned char *o, *i0, *i1, *i2, *i3;
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (texture->mipmaps <= 1)
		return;
	for (i = 1;i < texture->mipmaps;i++)
	{
		for (z = 0;z < texture->mipmap[i][4];z++)
		{
			layer0 = z*2;
			layer1 = z*2+1;
			if (layer1 >= texture->mipmap[i-1][4])
				layer1 = texture->mipmap[i-1][4]-1;
			for (y = 0;y < texture->mipmap[i][3];y++)
			{
				row0 = y*2;
				row1 = y*2+1;
				if (row1 >= texture->mipmap[i-1][3])
					row1 = texture->mipmap[i-1][3]-1;
				o =  texture->bytes + texture->mipmap[i  ][0] + 4*((texture->mipmap[i  ][3] * z      + y   ) * texture->mipmap[i  ][2]);
				i0 = texture->bytes + texture->mipmap[i-1][0] + 4*((texture->mipmap[i-1][3] * layer0 + row0) * texture->mipmap[i-1][2]);
				i1 = texture->bytes + texture->mipmap[i-1][0] + 4*((texture->mipmap[i-1][3] * layer0 + row1) * texture->mipmap[i-1][2]);
				i2 = texture->bytes + texture->mipmap[i-1][0] + 4*((texture->mipmap[i-1][3] * layer1 + row0) * texture->mipmap[i-1][2]);
				i3 = texture->bytes + texture->mipmap[i-1][0] + 4*((texture->mipmap[i-1][3] * layer1 + row1) * texture->mipmap[i-1][2]);
				w = texture->mipmap[i][2];
				if (layer1 > layer0)
				{
					if (texture->mipmap[i-1][2] > 1)
					{
						// average 3D texture
						for (x = 0;x < w;x++, o += 4, i0 += 8, i1 += 8, i2 += 8, i3 += 8)
						{
							o[0] = (i0[0] + i0[4] + i1[0] + i1[4] + i2[0] + i2[4] + i3[0] + i3[4] + 4) >> 3;
							o[1] = (i0[1] + i0[5] + i1[1] + i1[5] + i2[1] + i2[5] + i3[1] + i3[5] + 4) >> 3;
							o[2] = (i0[2] + i0[6] + i1[2] + i1[6] + i2[2] + i2[6] + i3[2] + i3[6] + 4) >> 3;
							o[3] = (i0[3] + i0[7] + i1[3] + i1[7] + i2[3] + i2[7] + i3[3] + i3[7] + 4) >> 3;
						}
					}
					else
					{
						// average 3D mipmap with parent width == 1
						for (x = 0;x < w;x++, o += 4, i0 += 8, i1 += 8)
						{
							o[0] = (i0[0] + i1[0] + i2[0] + i3[0] + 2) >> 2;
							o[1] = (i0[1] + i1[1] + i2[1] + i3[1] + 2) >> 2;
							o[2] = (i0[2] + i1[2] + i2[2] + i3[2] + 2) >> 2;
							o[3] = (i0[3] + i1[3] + i2[3] + i3[3] + 2) >> 2;
						}
					}
				}
				else
				{
					if (texture->mipmap[i-1][2] > 1)
					{
						// average 2D texture (common case)
						for (x = 0;x < w;x++, o += 4, i0 += 8, i1 += 8)
						{
							o[0] = (i0[0] + i0[4] + i1[0] + i1[4] + 2) >> 2;
							o[1] = (i0[1] + i0[5] + i1[1] + i1[5] + 2) >> 2;
							o[2] = (i0[2] + i0[6] + i1[2] + i1[6] + 2) >> 2;
							o[3] = (i0[3] + i0[7] + i1[3] + i1[7] + 2) >> 2;
						}
					}
					else
					{
						// 2D texture with parent width == 1
						o[0] = (i0[0] + i1[0] + 1) >> 1;
						o[1] = (i0[1] + i1[1] + 1) >> 1;
						o[2] = (i0[2] + i1[2] + 1) >> 1;
						o[3] = (i0[3] + i1[3] + 1) >> 1;
					}
				}
			}
		}
	}
}
void DPSOFTRAST_Texture_UpdatePartial(int index, int mip, const unsigned char *pixels, int blockx, int blocky, int blockwidth, int blockheight)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;

	// FIXME IMPLEMENT

	dpsoftrast.errorstring = "DPSOFTRAST_Texture_UpdatePartial: Not implemented.";
}
void DPSOFTRAST_Texture_UpdateFull(int index, const unsigned char *pixels)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;

	memcpy(texture->bytes, pixels, texture->mipmap[0][1]);
	DPSOFTRAST_Texture_CalculateMipmaps(index);
}
int DPSOFTRAST_Texture_GetWidth(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->mipmap[mip][2];
}
int DPSOFTRAST_Texture_GetHeight(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->mipmap[mip][3];
}
int DPSOFTRAST_Texture_GetDepth(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->mipmap[mip][4];
}
unsigned char *DPSOFTRAST_Texture_GetPixelPointer(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->bytes + texture->mipmap[mip][0];
}
void DPSOFTRAST_Texture_Filter(int index, DPSOFTRAST_TEXTURE_FILTER filter)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (!(texture->flags & DPSOFTRAST_TEXTURE_FLAG_MIPMAP) && filter > DPSOFTRAST_TEXTURE_FILTER_LINEAR)
	{
		dpsoftrast.errorstring = "DPSOFTRAST_Texture_Filter: requested filter mode requires mipmaps";
		return;
	}
	texture->filter = filter;
}

void DPSOFTRAST_SetRenderTargets(int width, int height, unsigned int *depthpixels, unsigned int *colorpixels0, unsigned int *colorpixels1, unsigned int *colorpixels2, unsigned int *colorpixels3)
{
	dpsoftrast.fb_width = width;
	dpsoftrast.fb_height = height;
	dpsoftrast.fb_depthpixels = depthpixels;
	dpsoftrast.fb_colorpixels[0] = colorpixels0;
	dpsoftrast.fb_colorpixels[1] = colorpixels1;
	dpsoftrast.fb_colorpixels[2] = colorpixels2;
	dpsoftrast.fb_colorpixels[3] = colorpixels3;
}
void DPSOFTRAST_Viewport(int x, int y, int width, int height)
{
	dpsoftrast.user.viewport[0] = x;
	dpsoftrast.user.viewport[1] = y;
	dpsoftrast.user.viewport[2] = width;
	dpsoftrast.user.viewport[3] = height;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_ClearColor(float r, float g, float b, float a)
{
	int i, x1, y1, x2, y2, w, h, x, y;
	unsigned int *p;
	unsigned int c;
	DPSOFTRAST_Validate(DPSOFTRAST_VALIDATE_FB);
	x1 = dpsoftrast.fb_clearscissor[0];
	y1 = dpsoftrast.fb_clearscissor[1];
	x2 = dpsoftrast.fb_clearscissor[2];
	y2 = dpsoftrast.fb_clearscissor[1] + dpsoftrast.fb_clearscissor[3];
	w = x2 - x1;
	h = y2 - y1;
	if (w < 1 || h < 1)
		return;
	// FIXME: honor dpsoftrast.fb_colormask?
	c = DPSOFTRAST_BGRA8_FROM_RGBA32F(r,g,b,a);
	for (i = 0;i < 4;i++)
	{
		if (!dpsoftrast.fb_colorpixels[i])
			continue;
		for (y = y1;y < y2;y++)
		{
			p = dpsoftrast.fb_colorpixels[i] + y * dpsoftrast.fb_width;
			for (x = x1;x < x2;x++)
				p[x] = c;
		}
	}
}
void DPSOFTRAST_ClearDepth(float d)
{
	int x1, y1, x2, y2, w, h, x, y;
	unsigned int *p;
	unsigned int c;
	DPSOFTRAST_Validate(DPSOFTRAST_VALIDATE_FB);
	x1 = dpsoftrast.fb_clearscissor[0];
	y1 = dpsoftrast.fb_clearscissor[1];
	x2 = dpsoftrast.fb_clearscissor[2];
	y2 = dpsoftrast.fb_clearscissor[1] + dpsoftrast.fb_clearscissor[3];
	w = x2 - x1;
	h = y2 - y1;
	if (w < 1 || h < 1)
		return;
	c = DPSOFTRAST_DEPTH32_FROM_DEPTH32F(d);
	for (y = y1;y < y2;y++)
	{
		p = dpsoftrast.fb_depthpixels + y * dpsoftrast.fb_width;
		for (x = x1;x < x2;x++)
			p[x] = c;
	}
}
void DPSOFTRAST_ColorMask(int r, int g, int b, int a)
{
	dpsoftrast.user.colormask[0] = r != 0;
	dpsoftrast.user.colormask[1] = g != 0;
	dpsoftrast.user.colormask[2] = b != 0;
	dpsoftrast.user.colormask[3] = a != 0;
	dpsoftrast.fb_colormask = ((-dpsoftrast.user.colormask[0]) & 0x00FF0000) | ((-dpsoftrast.user.colormask[1]) & 0x0000FF00) | ((-dpsoftrast.user.colormask[2]) & 0x000000FF) | ((-dpsoftrast.user.colormask[3]) & 0xFF000000);
}
void DPSOFTRAST_DepthTest(int enable)
{
	dpsoftrast.user.depthtest = enable;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_DEPTHFUNC;
}
void DPSOFTRAST_ScissorTest(int enable)
{
	dpsoftrast.user.scissortest = enable;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_Scissor(float x, float y, float width, float height)
{
	dpsoftrast.user.scissor[0] = x;
	dpsoftrast.user.scissor[1] = y;
	dpsoftrast.user.scissor[2] = width;
	dpsoftrast.user.scissor[3] = height;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_FB;
}

void DPSOFTRAST_BlendFunc(int smodulate, int dmodulate)
{
	// FIXME: validate
	dpsoftrast.user.blendfunc[0] = smodulate;
	dpsoftrast.user.blendfunc[1] = dmodulate;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_BLENDFUNC;
}
void DPSOFTRAST_BlendSubtract(int enable)
{
	dpsoftrast.user.blendsubtract = enable != 0;
	dpsoftrast.validate |= DPSOFTRAST_VALIDATE_BLENDFUNC;
}
void DPSOFTRAST_DepthMask(int enable)
{
	dpsoftrast.user.depthmask = enable;
}
void DPSOFTRAST_DepthFunc(int comparemode)
{
	// FIXME: validate
	dpsoftrast.user.depthfunc = comparemode;
}
void DPSOFTRAST_DepthRange(float range0, float range1)
{
	dpsoftrast.user.depthrange[0] = range0;
	dpsoftrast.user.depthrange[1] = range1;
}
void DPSOFTRAST_PolygonOffset(float alongnormal, float intoview)
{
	dpsoftrast.user.polygonoffset[0] = alongnormal;
	dpsoftrast.user.polygonoffset[1] = intoview;
}
void DPSOFTRAST_CullFace(int mode)
{
	// FIXME: validate
	dpsoftrast.user.cullface = mode;
}
void DPSOFTRAST_AlphaTest(float enable)
{
	dpsoftrast.user.alphatest = enable;
}
void DPSOFTRAST_AlphaFunc(int alphafunc, float alphavalue)
{
	// FIXME: validate
	dpsoftrast.user.alphafunc = alphafunc;
	dpsoftrast.user.alphavalue = alphavalue;
}
void DPSOFTRAST_Color4f(float r, float g, float b, float a)
{
	dpsoftrast.user.color[0] = r;
	dpsoftrast.user.color[1] = g;
	dpsoftrast.user.color[2] = b;
	dpsoftrast.user.color[3] = a;
}
void DPSOFTRAST_GetPixelsBGRA(int blockx, int blocky, int blockwidth, int blockheight, unsigned char *outpixels)
{
	int outstride = blockwidth * 4;
	int instride = dpsoftrast.fb_width * 4;
	int bx1 = blockx;
	int by1 = blocky;
	int bx2 = blockx + blockwidth;
	int by2 = blocky + blockheight;
	int bw;
	int bh;
	int x;
	int y;
	unsigned char *inpixels;
	unsigned char *b;
	unsigned char *o;
	if (bx1 < 0) bx1 = 0;
	if (by1 < 0) by1 = 0;
	if (bx2 > dpsoftrast.fb_width) bx2 = dpsoftrast.fb_width;
	if (by2 > dpsoftrast.fb_height) by2 = dpsoftrast.fb_height;
	bw = bx2 - bx1;
	bh = by2 - by1;
	inpixels = (unsigned char *)dpsoftrast.fb_colorpixels[0];
	if (dpsoftrast.bigendian)
	{
		for (y = by1;y < by2;y++)
		{
			b = (unsigned char *)inpixels + (dpsoftrast.fb_height - 1 - y) * instride + 4 * bx1;
			o = (unsigned char *)outpixels + (y - by1) * outstride;
			for (x = bx1;x < bx2;x++)
			{
				o[0] = b[3];
				o[1] = b[2];
				o[2] = b[1];
				o[3] = b[0];
				o += 4;
				b += 4;
			}
		}
	}
	else
	{
		for (y = by1;y < by2;y++)
		{
			b = (unsigned char *)inpixels + (dpsoftrast.fb_height - 1 - y) * instride + 4 * bx1;
			o = (unsigned char *)outpixels + (y - by1) * outstride;
			memcpy(o, b, bw*4);
		}
	}

}
void DPSOFTRAST_CopyRectangleToTexture(int index, int mip, int tx, int ty, int sx, int sy, int width, int height)
{
	int tx1 = tx;
	int ty1 = ty;
	int tx2 = tx + width;
	int ty2 = ty + height;
	int sx1 = sx;
	int sy1 = sy;
	int sx2 = sx + width;
	int sy2 = sy + height;
	int swidth;
	int sheight;
	int twidth;
	int theight;
	int sw;
	int sh;
	int tw;
	int th;
	int y;
	unsigned int *spixels;
	unsigned int *tpixels;
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (mip < 0 || mip >= texture->mipmaps) return;
	spixels = dpsoftrast.fb_colorpixels[0];
	swidth = dpsoftrast.fb_width;
	sheight = dpsoftrast.fb_height;
	tpixels = (unsigned int *)(texture->bytes + texture->mipmap[mip][0]);
	twidth = texture->mipmap[mip][2];
	theight = texture->mipmap[mip][3];
	if (tx1 < 0) tx1 = 0;
	if (ty1 < 0) ty1 = 0;
	if (tx2 > twidth) tx2 = twidth;
	if (ty2 > theight) ty2 = theight;
	if (sx1 < 0) sx1 = 0;
	if (sy1 < 0) sy1 = 0;
	if (sx2 > swidth) sx2 = swidth;
	if (sy2 > sheight) sy2 = sheight;
	tw = tx2 - tx1;
	th = ty2 - ty1;
	sw = sx2 - sx1;
	sh = sy2 - sy1;
	if (tw > sw) tw = sw;
	if (th > sh) th = sh;
	if (tw < 1 || th < 1)
		return;
	for (y = 0;y < th;y++)
		memcpy(tpixels + ((ty1 + y) * twidth + tx1), spixels + ((sy1 + y) * swidth + sx1), tw*4);
	if (texture->mipmaps > 1)
		DPSOFTRAST_Texture_CalculateMipmaps(index);
}
void DPSOFTRAST_SetTexture(int unitnum, int index)
{
	DPSOFTRAST_Texture *texture;
	if (unitnum < 0 || unitnum >= DPSOFTRAST_MAXTEXTUREUNITS)
	{
		dpsoftrast.errorstring = "DPSOFTRAST_SetTexture: invalid unit number";
		return;
	}
	texture = DPSOFTRAST_Texture_GetByIndex(index);
	if (index && !texture)
	{
		dpsoftrast.errorstring = "DPSOFTRAST_SetTexture: invalid texture handle";
		return;
	}
	dpsoftrast.texbound[unitnum] = texture;
}

void DPSOFTRAST_SetVertexPointer(const float *vertex3f, size_t stride)
{
	dpsoftrast.pointer_vertex3f = vertex3f;
	dpsoftrast.stride_vertex = stride;
}
void DPSOFTRAST_SetColorPointer(const float *color4f, size_t stride)
{
	dpsoftrast.pointer_color4f = color4f;
	dpsoftrast.pointer_color4ub = NULL;
	dpsoftrast.stride_color = stride;
}
void DPSOFTRAST_SetColorPointer4ub(const unsigned char *color4ub, size_t stride)
{
	dpsoftrast.pointer_color4f = NULL;
	dpsoftrast.pointer_color4ub = color4ub;
	dpsoftrast.stride_color = stride;
}
void DPSOFTRAST_SetTexCoordPointer(int unitnum, int numcomponents, size_t stride, const float *texcoordf)
{
	dpsoftrast.pointer_texcoordf[unitnum] = texcoordf;
	dpsoftrast.components_texcoord[unitnum] = numcomponents;
	dpsoftrast.stride_texcoord[unitnum] = stride;
}

void DPSOFTRAST_SetShader(unsigned int mode, unsigned int permutation)
{
	dpsoftrast.shader_mode = mode;
	dpsoftrast.shader_permutation = permutation;
}
void DPSOFTRAST_Uniform4fARB(DPSOFTRAST_UNIFORM index, float v0, float v1, float v2, float v3)
{
	dpsoftrast.uniform4f[index*4+0] = v0;
	dpsoftrast.uniform4f[index*4+1] = v1;
	dpsoftrast.uniform4f[index*4+2] = v2;
	dpsoftrast.uniform4f[index*4+3] = v3;
}
void DPSOFTRAST_Uniform4fvARB(DPSOFTRAST_UNIFORM index, const float *v)
{
	dpsoftrast.uniform4f[index*4+0] = v[0];
	dpsoftrast.uniform4f[index*4+1] = v[1];
	dpsoftrast.uniform4f[index*4+2] = v[2];
	dpsoftrast.uniform4f[index*4+3] = v[3];
}
void DPSOFTRAST_UniformMatrix4fvARB(DPSOFTRAST_UNIFORM index, int arraysize, int transpose, const float *v)
{
	int i;
	for (i = 0;i < arraysize;i++, index += 4, v += 16)
	{
		if (transpose)
		{
			dpsoftrast.uniform4f[index*4+0] = v[0];
			dpsoftrast.uniform4f[index*4+1] = v[4];
			dpsoftrast.uniform4f[index*4+2] = v[8];
			dpsoftrast.uniform4f[index*4+3] = v[12];
			dpsoftrast.uniform4f[index*4+4] = v[1];
			dpsoftrast.uniform4f[index*4+5] = v[5];
			dpsoftrast.uniform4f[index*4+6] = v[9];
			dpsoftrast.uniform4f[index*4+7] = v[13];
			dpsoftrast.uniform4f[index*4+8] = v[2];
			dpsoftrast.uniform4f[index*4+9] = v[6];
			dpsoftrast.uniform4f[index*4+10] = v[10];
			dpsoftrast.uniform4f[index*4+11] = v[14];
			dpsoftrast.uniform4f[index*4+12] = v[3];
			dpsoftrast.uniform4f[index*4+13] = v[7];
			dpsoftrast.uniform4f[index*4+14] = v[11];
			dpsoftrast.uniform4f[index*4+15] = v[15];
		}
		else
		{
			dpsoftrast.uniform4f[index*4+0] = v[0];
			dpsoftrast.uniform4f[index*4+1] = v[1];
			dpsoftrast.uniform4f[index*4+2] = v[2];
			dpsoftrast.uniform4f[index*4+3] = v[3];
			dpsoftrast.uniform4f[index*4+4] = v[4];
			dpsoftrast.uniform4f[index*4+5] = v[5];
			dpsoftrast.uniform4f[index*4+6] = v[6];
			dpsoftrast.uniform4f[index*4+7] = v[7];
			dpsoftrast.uniform4f[index*4+8] = v[8];
			dpsoftrast.uniform4f[index*4+9] = v[9];
			dpsoftrast.uniform4f[index*4+10] = v[10];
			dpsoftrast.uniform4f[index*4+11] = v[11];
			dpsoftrast.uniform4f[index*4+12] = v[12];
			dpsoftrast.uniform4f[index*4+13] = v[13];
			dpsoftrast.uniform4f[index*4+14] = v[14];
			dpsoftrast.uniform4f[index*4+15] = v[15];
		}
	}
}
void DPSOFTRAST_Uniform1iARB(DPSOFTRAST_UNIFORM index, int i0)
{
	dpsoftrast.uniform1i[index] = i0;
}

void DPSOFTRAST_Draw_LoadVertices(int firstvertex, int numvertices, bool needcolors)
{
	int i;
	int j;
	int stride;
	const float *v;
	float *p;
	float *data;
	const unsigned char *b;
	dpsoftrast.draw.numvertices = numvertices;
	if (dpsoftrast.draw.maxvertices < dpsoftrast.draw.numvertices)
	{
		if (dpsoftrast.draw.maxvertices < 4096)
			dpsoftrast.draw.maxvertices = 4096;
		while (dpsoftrast.draw.maxvertices < dpsoftrast.draw.numvertices)
			dpsoftrast.draw.maxvertices *= 2;
		if (dpsoftrast.draw.in_array4f[0])
			MM_FREE(dpsoftrast.draw.in_array4f[0]);
		data = (float *)MM_CALLOC(1, dpsoftrast.draw.maxvertices * sizeof(float[4])*(DPSOFTRAST_ARRAY_TOTAL*2 + 1));
		for (i = 0;i < DPSOFTRAST_ARRAY_TOTAL;i++, data += dpsoftrast.draw.maxvertices * 4)
			dpsoftrast.draw.in_array4f[i] = data;
		for (i = 0;i < DPSOFTRAST_ARRAY_TOTAL;i++, data += dpsoftrast.draw.maxvertices * 4)
			dpsoftrast.draw.post_array4f[i] = data;
		dpsoftrast.draw.screencoord4f = data;
		data += dpsoftrast.draw.maxvertices * 4;
	}
	stride = dpsoftrast.stride_vertex;
	v = (const float *)((unsigned char *)dpsoftrast.pointer_vertex3f + firstvertex * stride);
	p = dpsoftrast.draw.in_array4f[0];
	for (i = 0;i < numvertices;i++)
	{
		p[0] = v[0];
		p[1] = v[1];
		p[2] = v[2];
		p[3] = 1.0f;
		p += 4;
		v = (const float *)((const unsigned char *)v + stride);
	}
	if (needcolors)
	{
		if (dpsoftrast.pointer_color4f)
		{
			stride = dpsoftrast.stride_color;
			v = (const float *)((const unsigned char *)dpsoftrast.pointer_color4f + firstvertex * stride);
			p = dpsoftrast.draw.in_array4f[1];
			for (i = 0;i < numvertices;i++)
			{
				p[0] = v[0];
				p[1] = v[1];
				p[2] = v[2];
				p[3] = v[3];
				p += 4;
				v = (const float *)((const unsigned char *)v + stride);
			}
		}
		else if (dpsoftrast.pointer_color4ub)
		{
			stride = dpsoftrast.stride_color;
			b = (const unsigned char *)((const unsigned char *)dpsoftrast.pointer_color4ub + firstvertex * stride);
			p = dpsoftrast.draw.in_array4f[1];
			for (i = 0;i < numvertices;i++)
			{
				p[0] = b[0] * (1.0f / 255.0f);
				p[1] = b[1] * (1.0f / 255.0f);
				p[2] = b[2] * (1.0f / 255.0f);
				p[3] = b[3] * (1.0f / 255.0f);
				p += 4;
				b = (const unsigned char *)((const unsigned char *)b + stride);
			}
		}
		else
		{
			v = dpsoftrast.user.color;
			p = dpsoftrast.draw.in_array4f[1];
			for (i = 0;i < numvertices;i++)
			{
				p[0] = v[0];
				p[1] = v[1];
				p[2] = v[2];
				p[3] = v[3];
				p += 4;
			}
		}
	}
	for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL-2;j++)
	{
		if (dpsoftrast.pointer_texcoordf[j])
		{
			stride = dpsoftrast.stride_texcoord[j];
			v = (const float *)((const unsigned char *)dpsoftrast.pointer_texcoordf[j] + firstvertex * stride);
			p = dpsoftrast.draw.in_array4f[j+2];
			switch(dpsoftrast.components_texcoord[j])
			{
			case 2:
				for (i = 0;i < numvertices;i++)
				{
					p[0] = v[0];
					p[1] = v[1];
					p[2] = 0.0f;
					p[3] = 1.0f;
					p += 4;
					v = (const float *)((const unsigned char *)v + stride);
				}
				break;
			case 3:
				for (i = 0;i < numvertices;i++)
				{
					p[0] = v[0];
					p[1] = v[1];
					p[2] = v[2];
					p[3] = 1.0f;
					p += 4;
					v = (const float *)((const unsigned char *)v + stride);
				}
				break;
			case 4:
				for (i = 0;i < numvertices;i++)
				{
					p[0] = v[0];
					p[1] = v[1];
					p[2] = v[2];
					p[3] = v[3];
					p += 4;
					v = (const float *)((const unsigned char *)v + stride);
				}
				break;
			}
		}
	}
}

void DPSOFTRAST_Array_Transform(float *out4f, const float *in4f, int numitems, const float *inmatrix16f)
{
	static const float identitymatrix[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
	// TODO: SIMD
	float matrix[4][4];
	int i;
	memcpy(matrix, inmatrix16f, sizeof(float[16]));
	if (!memcmp(identitymatrix, matrix, sizeof(float[16])))
	{
		// fast case for identity matrix
		memcpy(out4f, in4f, numitems * sizeof(float[4]));
		return;
	}
	for (i = 0;i < numitems;i++, out4f += 4, in4f += 4)
	{
		out4f[0] = in4f[0] * matrix[0][0] + in4f[1] * matrix[1][0] + in4f[2] * matrix[2][0] + in4f[3] * matrix[3][0];
		out4f[1] = in4f[0] * matrix[0][1] + in4f[1] * matrix[1][1] + in4f[2] * matrix[2][1] + in4f[3] * matrix[3][1];
		out4f[2] = in4f[0] * matrix[0][2] + in4f[1] * matrix[1][2] + in4f[2] * matrix[2][2] + in4f[3] * matrix[3][2];
		out4f[3] = in4f[0] * matrix[0][3] + in4f[1] * matrix[1][3] + in4f[2] * matrix[2][3] + in4f[3] * matrix[3][3];
	}
}

void DPSOFTRAST_Array_Copy(float *out4f, const float *in4f, int numitems)
{
	memcpy(out4f, in4f, numitems * sizeof(float[4]));
}

#ifdef SSE2_PRESENT
static __m128 DPSOFTRAST_Draw_ProjectVertex(__m128 v)
{
	__m128 viewportcenter = _mm_load_ps(dpsoftrast.fb_viewportcenter), viewportscale = _mm_load_ps(dpsoftrast.fb_viewportscale);
	__m128 w = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
	v = _mm_move_ss(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 1, 0, 3)), _mm_set1_ps(1.0f));
	v = _mm_add_ps(viewportcenter, _mm_div_ps(_mm_mul_ps(viewportscale, v), w));
	v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 3, 2, 1));
	return v;
}
#endif

void DPSOFTRAST_Draw_ProjectVertices(float *out4f, const float *in4f, int numitems)
{
#ifdef SSE2_PRESENT
	// NOTE: this is used both as a whole mesh transform function and a
	// per-triangle transform function (for clipped triangles), accordingly
	// it should not crash on divide by 0 but the result of divide by 0 is
	// unimportant...
	// TODO: SIMD
	int i;
	__m128 viewportcenter = _mm_load_ps(dpsoftrast.fb_viewportcenter), viewportscale = _mm_load_ps(dpsoftrast.fb_viewportscale);
	for (i = 0;i < numitems;i++)
	{
		__m128 v = _mm_load_ps(in4f), w = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
		v = _mm_move_ss(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 1, 0, 3)), _mm_set1_ps(1.0f));
		v = _mm_add_ps(viewportcenter, _mm_div_ps(_mm_mul_ps(viewportscale, v), w));
		_mm_store_ps(out4f, _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 3, 2, 1)));
		in4f += 4;
		out4f += 4;
	}
#endif
}

void DPSOFTRAST_Draw_DebugEdgePoints(const float *screen0, const float *screen1)
{
	int i;
	int x;
	int y;
	int w = dpsoftrast.fb_width;
	int bounds[4];
	float v0[2], v1[2];
	unsigned int *pixels = dpsoftrast.fb_colorpixels[0];
	//const float *c4f;
	bounds[0] = dpsoftrast.fb_viewportscissor[0];
	bounds[1] = dpsoftrast.fb_viewportscissor[1];
	bounds[2] = dpsoftrast.fb_viewportscissor[0] + dpsoftrast.fb_viewportscissor[2];
	bounds[3] = dpsoftrast.fb_viewportscissor[1] + dpsoftrast.fb_viewportscissor[3];
	v0[0] = screen0[0];
	v0[1] = screen0[1];
	v1[0] = screen1[0];
	v1[1] = screen1[1];
	for (i = 0;i <= 128;i++)
	{
		// check nearclip
		//if (dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+3] != 1.0f)
		//	continue;
		x = (int)(v0[0] + (v1[0] - v0[0]) * (i/128.0f));
		y = (int)(v0[1] + (v1[1] - v0[1]) * (i/128.0f));
		if (x < bounds[0] || y < bounds[1] || x >= bounds[2] || y >= bounds[3])
			continue;
		//c4f = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR] + element0*4;
		//pixels[y*w+x] = DPSOFTRAST_BGRA8_FROM_RGBA32F(c4f[0], c4f[1], c4f[2], c4f[3]);
		pixels[y*w+x] = 0xFFFFFFFF;
	}
}

void DPSOFTRAST_Draw_Span_Begin(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float w = span->data[0][DPSOFTRAST_ARRAY_TOTAL][3];
	float wslope = span->data[1][DPSOFTRAST_ARRAY_TOTAL][3];
	float endz = 1.0f / (w + wslope * startx);
	for (x = startx;x < endx;)
	{
		int nextsub = x + DPSOFTRAST_MAXSUBSPAN, endsub = nextsub - 1;
		float z = endz, dz;
		if(nextsub >= endx) nextsub = endsub = endx-1;
		endz = 1.0f / (w + wslope * nextsub);
		dz = x < nextsub ? (endz - z) / (nextsub - x) : 0.0f;
		for (; x <= endsub; x++, z += dz)
			zf[x] = z;
	}
}

void DPSOFTRAST_Draw_Span_Finish(const DPSOFTRAST_State_Draw_Span * RESTRICT span, const float * RESTRICT in4f)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int d[4];
	float a, b;
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0];
	if (!pixel)
		return;
	pixel += span->start * 4;
	// handle alphatest now (this affects depth writes too)
	if (dpsoftrast.user.alphatest)
		for (x = startx;x < endx;x++)
			if (in4f[x*4+3] < 0.5f)
				pixelmask[x] = false;
	// FIXME: this does not handle bigendian
	switch(dpsoftrast.fb_blendmode)
	{
	case DPSOFTRAST_BLENDMODE_OPAQUE:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			d[0] = (int)(in4f[x*4+2]*255.0f);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*255.0f);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*255.0f);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*255.0f);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_ALPHA:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			a = in4f[x*4+3] * 255.0f;
			b = 1.0f - in4f[x*4+3];
			d[0] = (int)(in4f[x*4+2]*a+pixel[x*4+0]*b);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*a+pixel[x*4+1]*b);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*a+pixel[x*4+2]*b);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*a+pixel[x*4+3]*b);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_ADDALPHA:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			a = in4f[x*4+3] * 255.0f;
			d[0] = (int)(in4f[x*4+2]*a+pixel[x*4+0]);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*a+pixel[x*4+1]);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*a+pixel[x*4+2]);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*a+pixel[x*4+3]);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_ADD:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			d[0] = (int)(in4f[x*4+2]*255.0f+pixel[x*4+0]);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*255.0f+pixel[x*4+1]);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*255.0f+pixel[x*4+2]);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*255.0f+pixel[x*4+3]);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_INVMOD:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			d[0] = (int)((1.0f-in4f[x*4+2])*pixel[x*4+0]);if (d[0] > 255) d[0] = 255;
			d[1] = (int)((1.0f-in4f[x*4+1])*pixel[x*4+1]);if (d[1] > 255) d[1] = 255;
			d[2] = (int)((1.0f-in4f[x*4+0])*pixel[x*4+2]);if (d[2] > 255) d[2] = 255;
			d[3] = (int)((1.0f-in4f[x*4+3])*pixel[x*4+3]);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_MUL:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			d[0] = (int)(in4f[x*4+2]*pixel[x*4+0]);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*pixel[x*4+1]);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*pixel[x*4+2]);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*pixel[x*4+3]);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_MUL2:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			d[0] = (int)(in4f[x*4+2]*pixel[x*4+0]*2.0f);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*pixel[x*4+1]*2.0f);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*pixel[x*4+2]*2.0f);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*pixel[x*4+3]*2.0f);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_SUBALPHA:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			a = in4f[x*4+3] * -255.0f;
			d[0] = (int)(in4f[x*4+2]*a+pixel[x*4+0]);if (d[0] > 255) d[0] = 255;if (d[0] < 0) d[0] = 0;
			d[1] = (int)(in4f[x*4+1]*a+pixel[x*4+1]);if (d[1] > 255) d[1] = 255;if (d[1] < 0) d[1] = 0;
			d[2] = (int)(in4f[x*4+0]*a+pixel[x*4+2]);if (d[2] > 255) d[2] = 255;if (d[2] < 0) d[2] = 0;
			d[3] = (int)(in4f[x*4+3]*a+pixel[x*4+3]);if (d[3] > 255) d[3] = 255;if (d[3] < 0) d[3] = 0;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	case DPSOFTRAST_BLENDMODE_PSEUDOALPHA:
		for (x = startx;x < endx;x++)
		{
			if (!pixelmask[x])
				continue;
			a = 255.0f;
			b = 1.0f - in4f[x*4+3];
			d[0] = (int)(in4f[x*4+2]*a+pixel[x*4+0]*b);if (d[0] > 255) d[0] = 255;
			d[1] = (int)(in4f[x*4+1]*a+pixel[x*4+1]*b);if (d[1] > 255) d[1] = 255;
			d[2] = (int)(in4f[x*4+0]*a+pixel[x*4+2]*b);if (d[2] > 255) d[2] = 255;
			d[3] = (int)(in4f[x*4+3]*a+pixel[x*4+3]*b);if (d[3] > 255) d[3] = 255;
			pixel[x*4+0] = d[0];
			pixel[x*4+1] = d[1];
			pixel[x*4+2] = d[2];
			pixel[x*4+3] = d[3];
		}
		break;
	}
}

void DPSOFTRAST_Draw_Span_FinishBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, const unsigned char* RESTRICT in4ub)
{
#ifdef SSE2_PRESENT
	int x;
	int startx = span->startx;
	int endx = span->endx;
	const unsigned int * RESTRICT ini = (const unsigned int *)in4ub;
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0];
	unsigned int * RESTRICT pixeli = (unsigned int *)dpsoftrast.fb_colorpixels[0];
	if (!pixel)
		return;
	pixel += span->start * 4;
	pixeli += span->start;
	// handle alphatest now (this affects depth writes too)
	if (dpsoftrast.user.alphatest)
		for (x = startx;x < endx;x++)
			if (in4ub[x*4+3] < 0.5f)
				pixelmask[x] = false;
	// FIXME: this does not handle bigendian
	switch(dpsoftrast.fb_blendmode)
	{
	case DPSOFTRAST_BLENDMODE_OPAQUE:
		for (x = startx;x + 4 <= endx;)
		{
			if (*(const unsigned int *)&pixelmask[x] == 0x01010101)
			{
				_mm_storeu_si128((__m128i *)&pixeli[x], _mm_loadu_si128((const __m128i *)&ini[x]));
				x += 4;
			}
			else
			{
				if (pixelmask[x])
					pixeli[x] = ini[x];
				x++;
			}
		}
		for (;x < endx;x++)
			if (pixelmask[x])
				pixeli[x] = ini[x];
		break;
	case DPSOFTRAST_BLENDMODE_ALPHA:
	#define FINISHBLEND(blend2, blend1) \
		for (x = startx;x + 2 <= endx;x += 2) \
		{ \
			__m128i src, dst; \
			switch (*(const unsigned short*)&pixelmask[x]) \
			{ \
			case 0x0101: \
				src = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ini[x]), _mm_setzero_si128()); \
				dst = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixeli[x]), _mm_setzero_si128()); \
				blend2; \
				_mm_storel_epi64((__m128i *)&pixeli[x], _mm_packus_epi16(dst, dst)); \
				continue; \
			case 0x0100: \
				src = _mm_unpacklo_epi8(_mm_cvtsi32_si128(ini[x+1]), _mm_setzero_si128()); \
				dst = _mm_unpacklo_epi8(_mm_cvtsi32_si128(pixeli[x+1]), _mm_setzero_si128()); \
				blend1; \
				pixeli[x+1] = _mm_cvtsi128_si32(_mm_packus_epi16(dst, dst));  \
				continue; \
			case 0x0001: \
				src = _mm_unpacklo_epi8(_mm_cvtsi32_si128(ini[x]), _mm_setzero_si128()); \
				dst = _mm_unpacklo_epi8(_mm_cvtsi32_si128(pixeli[x]), _mm_setzero_si128()); \
				blend1; \
				pixeli[x] = _mm_cvtsi128_si32(_mm_packus_epi16(dst, dst)); \
				continue; \
			} \
			break; \
		} \
		for(;x < endx; x++) \
		{ \
			__m128i src, dst; \
			if (!pixelmask[x]) \
				continue; \
			src = _mm_unpacklo_epi8(_mm_cvtsi32_si128(ini[x]), _mm_setzero_si128()); \
			dst = _mm_unpacklo_epi8(_mm_cvtsi32_si128(pixeli[x]), _mm_setzero_si128()); \
			blend1; \
			pixeli[x] = _mm_cvtsi128_si32(_mm_packus_epi16(dst, dst)); \
		}

		FINISHBLEND({
			__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(dst, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(src, dst), 4), _mm_slli_epi16(blend, 4)));
		}, {
			__m128i blend = _mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(dst, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(src, dst), 4), _mm_slli_epi16(blend, 4)));
		});
		break;
	case DPSOFTRAST_BLENDMODE_ADDALPHA:
		FINISHBLEND({
			__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(src, blend), 8));
		}, {
			__m128i blend = _mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(src, blend), 8));
		});
		break;
	case DPSOFTRAST_BLENDMODE_ADD:
		FINISHBLEND({ dst = _mm_add_epi16(src, dst); }, { dst = _mm_add_epi16(src, dst); });
		break;
	case DPSOFTRAST_BLENDMODE_INVMOD:
		FINISHBLEND({
			dst = _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(dst, src), 8));
		}, {
			dst = _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(dst, src), 8));
		});
		break;
	case DPSOFTRAST_BLENDMODE_MUL:
		FINISHBLEND({ dst = _mm_srli_epi16(_mm_mullo_epi16(src, dst), 8); }, { dst = _mm_srli_epi16(_mm_mullo_epi16(src, dst), 8); });
		break;
	case DPSOFTRAST_BLENDMODE_MUL2:
		FINISHBLEND({ dst = _mm_srli_epi16(_mm_mullo_epi16(src, dst), 7); }, { dst = _mm_srli_epi16(_mm_mullo_epi16(src, dst), 7); });
		break;
	case DPSOFTRAST_BLENDMODE_SUBALPHA:
		FINISHBLEND({
			__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(src, blend), 8));
		}, {
			__m128i blend = _mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(src, blend), 8));
		});
		break;
	case DPSOFTRAST_BLENDMODE_PSEUDOALPHA:
		FINISHBLEND({
			__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(src, _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(dst, blend), 8)));
		}, {
			__m128i blend = _mm_shufflelo_epi16(src, _MM_SHUFFLE(3, 3, 3, 3));
			dst = _mm_add_epi16(src, _mm_sub_epi16(dst, _mm_srli_epi16(_mm_mullo_epi16(dst, blend), 8)));
		});
		break;
	}
#endif
}

void DPSOFTRAST_Draw_Span_Texture2DVarying(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float * RESTRICT out4f, int texunitindex, int arrayindex, const float * RESTRICT zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int flags;
	float c[4];
	float data[4];
	float slope[4];
	float tc[2], endtc[2];
	float tcscale[2];
	unsigned int tci[2];
	unsigned int tci1[2];
	unsigned int tcimin[2];
	unsigned int tcimax[2];
	int tciwrapmask[2];
	int tciwidth;
	int filter;
	int mip;
	const unsigned char * RESTRICT pixelbase;
	const unsigned char * RESTRICT pixel[4];
	DPSOFTRAST_Texture *texture = dpsoftrast.texbound[texunitindex];
	// if no texture is bound, just fill it with white
	if (!texture)
	{
		for (x = startx;x < endx;x++)
		{
			out4f[x*4+0] = 1.0f;
			out4f[x*4+1] = 1.0f;
			out4f[x*4+2] = 1.0f;
			out4f[x*4+3] = 1.0f;
		}
		return;
	}
	mip = span->mip[texunitindex];
	pixelbase = (unsigned char *)texture->bytes + texture->mipmap[mip][0];
	// if this mipmap of the texture is 1 pixel, just fill it with that color
	if (texture->mipmap[mip][1] == 4)
	{
		c[0] = texture->bytes[2] * (1.0f/255.0f);
		c[1] = texture->bytes[1] * (1.0f/255.0f);
		c[2] = texture->bytes[0] * (1.0f/255.0f);
		c[3] = texture->bytes[3] * (1.0f/255.0f);
		for (x = startx;x < endx;x++)
		{
			out4f[x*4+0] = c[0];
			out4f[x*4+1] = c[1];
			out4f[x*4+2] = c[2];
			out4f[x*4+3] = c[3];
		}
		return;
	}
	filter = texture->filter & DPSOFTRAST_TEXTURE_FILTER_LINEAR;
	data[0] = span->data[0][arrayindex][0];
	data[1] = span->data[0][arrayindex][1];
	data[2] = span->data[0][arrayindex][2];
	data[3] = span->data[0][arrayindex][3];
	slope[0] = span->data[1][arrayindex][0];
	slope[1] = span->data[1][arrayindex][1];
	slope[2] = span->data[1][arrayindex][2];
	slope[3] = span->data[1][arrayindex][3];
	flags = texture->flags;
	tcscale[0] = texture->mipmap[mip][2];
	tcscale[1] = texture->mipmap[mip][3];
	tciwidth = texture->mipmap[mip][2];
	tcimin[0] = 0;
	tcimin[1] = 0;
	tcimax[0] = texture->mipmap[mip][2]-1;
	tcimax[1] = texture->mipmap[mip][3]-1;
	tciwrapmask[0] = texture->mipmap[mip][2]-1;
	tciwrapmask[1] = texture->mipmap[mip][3]-1;
	endtc[0] = (data[0] + slope[0]*startx) * zf[startx] * tcscale[0] - 0.5f;
	endtc[1] = (data[1] + slope[1]*startx) * zf[startx] * tcscale[1] - 0.5f;
	for (x = startx;x < endx;)
	{
		unsigned int subtc[2];
		unsigned int substep[2];
		float subscale = 65536.0f/DPSOFTRAST_MAXSUBSPAN;
		int nextsub = x + DPSOFTRAST_MAXSUBSPAN, endsub = nextsub - 1;
		if(nextsub >= endx)
		{
			nextsub = endsub = endx-1;	
			if(x < nextsub) subscale = 65536.0f / (nextsub - x);
		}
		tc[0] = endtc[0];
		tc[1] = endtc[1];
		endtc[0] = (data[0] + slope[0]*nextsub) * zf[nextsub] * tcscale[0] - 0.5f;
		endtc[1] = (data[1] + slope[1]*nextsub) * zf[nextsub] * tcscale[1] - 0.5f;
		substep[0] = (endtc[0] - tc[0]) * subscale;
		substep[1] = (endtc[1] - tc[1]) * subscale;
		subtc[0] = tc[0] * (1<<16);
		subtc[1] = tc[1] * (1<<16);
		if(filter)
		{
			if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
			{
				for (; x <= endsub; x++, subtc[0] += substep[0], subtc[1] += substep[1])
				{
					unsigned int frac[2] = { subtc[0]&0xFFF, subtc[1]&0xFFF };
					unsigned int ifrac[2] = { 0x1000 - frac[0], 0x1000 - frac[1] };
					unsigned int lerp[4] = { ifrac[0]*ifrac[1], frac[0]*ifrac[1], ifrac[0]*frac[1], frac[0]*frac[1] };
					tci[0] = subtc[0]>>16;
					tci[1] = subtc[1]>>16;
					tci1[0] = tci[0] + 1;
					tci1[1] = tci[1] + 1;
					tci[0] = tci[0] >= tcimin[0] ? (tci[0] <= tcimax[0] ? tci[0] : tcimax[0]) : tcimin[0];
					tci[1] = tci[1] >= tcimin[1] ? (tci[1] <= tcimax[1] ? tci[1] : tcimax[1]) : tcimin[1];
					tci1[0] = tci1[0] >= tcimin[0] ? (tci1[0] <= tcimax[0] ? tci1[0] : tcimax[0]) : tcimin[0];
					tci1[1] = tci1[1] >= tcimin[1] ? (tci1[1] <= tcimax[1] ? tci1[1] : tcimax[1]) : tcimin[1];
					pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
					pixel[1] = pixelbase + 4 * (tci[1]*tciwidth+tci1[0]);
					pixel[2] = pixelbase + 4 * (tci1[1]*tciwidth+tci[0]);
					pixel[3] = pixelbase + 4 * (tci1[1]*tciwidth+tci1[0]);
					c[0] = (pixel[0][2]*lerp[0]+pixel[1][2]*lerp[1]+pixel[2][2]*lerp[2]+pixel[3][2]*lerp[3]) * (1.0f / 0xFF000000);
					c[1] = (pixel[0][1]*lerp[0]+pixel[1][1]*lerp[1]+pixel[2][1]*lerp[2]+pixel[3][1]*lerp[3]) * (1.0f / 0xFF000000);
					c[2] = (pixel[0][0]*lerp[0]+pixel[1][0]*lerp[1]+pixel[2][0]*lerp[2]+pixel[3][0]*lerp[3]) * (1.0f / 0xFF000000);
					c[3] = (pixel[0][3]*lerp[0]+pixel[1][3]*lerp[1]+pixel[2][3]*lerp[2]+pixel[3][3]*lerp[3]) * (1.0f / 0xFF000000);
					out4f[x*4+0] = c[0];
					out4f[x*4+1] = c[1];
					out4f[x*4+2] = c[2];
					out4f[x*4+3] = c[3];
				}
			}
			else
			{
				for (; x <= endsub; x++, subtc[0] += substep[0], subtc[1] += substep[1])
				{
					unsigned int frac[2] = { subtc[0]&0xFFF, subtc[1]&0xFFF };
					unsigned int ifrac[2] = { 0x1000 - frac[0], 0x1000 - frac[1] };
					unsigned int lerp[4] = { ifrac[0]*ifrac[1], frac[0]*ifrac[1], ifrac[0]*frac[1], frac[0]*frac[1] };
					tci[0] = subtc[0]>>16;
					tci[1] = subtc[1]>>16;
					tci1[0] = tci[0] + 1;
					tci1[1] = tci[1] + 1;
					tci[0] &= tciwrapmask[0];
					tci[1] &= tciwrapmask[1];
					tci1[0] &= tciwrapmask[0];
					tci1[1] &= tciwrapmask[1];
					pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
					pixel[1] = pixelbase + 4 * (tci[1]*tciwidth+tci1[0]);
					pixel[2] = pixelbase + 4 * (tci1[1]*tciwidth+tci[0]);
					pixel[3] = pixelbase + 4 * (tci1[1]*tciwidth+tci1[0]);
					c[0] = (pixel[0][2]*lerp[0]+pixel[1][2]*lerp[1]+pixel[2][2]*lerp[2]+pixel[3][2]*lerp[3]) * (1.0f / 0xFF000000);
					c[1] = (pixel[0][1]*lerp[0]+pixel[1][1]*lerp[1]+pixel[2][1]*lerp[2]+pixel[3][1]*lerp[3]) * (1.0f / 0xFF000000);
					c[2] = (pixel[0][0]*lerp[0]+pixel[1][0]*lerp[1]+pixel[2][0]*lerp[2]+pixel[3][0]*lerp[3]) * (1.0f / 0xFF000000);
					c[3] = (pixel[0][3]*lerp[0]+pixel[1][3]*lerp[1]+pixel[2][3]*lerp[2]+pixel[3][3]*lerp[3]) * (1.0f / 0xFF000000);
					out4f[x*4+0] = c[0];
					out4f[x*4+1] = c[1];
					out4f[x*4+2] = c[2];
					out4f[x*4+3] = c[3];
				}
			}
		}
		else if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
		{
			for (; x <= endsub; x++, subtc[0] += substep[0], subtc[1] += substep[1])
			{
				tci[0] = subtc[0]>>16;
				tci[1] = subtc[1]>>16;
				tci[0] = tci[0] >= tcimin[0] ? (tci[0] <= tcimax[0] ? tci[0] : tcimax[0]) : tcimin[0];
				tci[1] = tci[1] >= tcimin[1] ? (tci[1] <= tcimax[1] ? tci[1] : tcimax[1]) : tcimin[1];
				pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
				c[0] = pixel[0][2] * (1.0f / 255.0f);
				c[1] = pixel[0][1] * (1.0f / 255.0f);
				c[2] = pixel[0][0] * (1.0f / 255.0f);
				c[3] = pixel[0][3] * (1.0f / 255.0f);
				out4f[x*4+0] = c[0];
				out4f[x*4+1] = c[1];
				out4f[x*4+2] = c[2];
				out4f[x*4+3] = c[3];
			}
		}
		else
		{
			for (; x <= endsub; x++, subtc[0] += substep[0], subtc[1] += substep[1])
			{
				tci[0] = subtc[0]>>16;
				tci[1] = subtc[1]>>16;
				tci[0] &= tciwrapmask[0];
				tci[1] &= tciwrapmask[1];
				pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
				c[0] = pixel[0][2] * (1.0f / 255.0f);
				c[1] = pixel[0][1] * (1.0f / 255.0f);
				c[2] = pixel[0][0] * (1.0f / 255.0f);
				c[3] = pixel[0][3] * (1.0f / 255.0f);
				out4f[x*4+0] = c[0];
				out4f[x*4+1] = c[1];
				out4f[x*4+2] = c[2];
				out4f[x*4+3] = c[3];
			}
		}
	}
}

void DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char * RESTRICT out4ub, int texunitindex, int arrayindex, const float * RESTRICT zf)
{
#ifdef SSE2_PRESENT
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int flags;
	__m128 data, slope, tcscale;
	__m128i tcsize, tcmask, tcoffset, tcmax;
	__m128 tc, endtc;
	__m128i subtc, substep, endsubtc;
	int filter;
	int mip;
	unsigned int *outi = (unsigned int *)out4ub;
	const unsigned char * RESTRICT pixelbase;
	DPSOFTRAST_Texture *texture = dpsoftrast.texbound[texunitindex];
	// if no texture is bound, just fill it with white
	if (!texture)
	{
		memset(out4ub + startx*4, 255, span->length*4);
		return;
	}
	mip = span->mip[texunitindex];
	pixelbase = (const unsigned char *)texture->bytes + texture->mipmap[mip][0];
	// if this mipmap of the texture is 1 pixel, just fill it with that color
	if (texture->mipmap[mip][1] == 4)
	{
		unsigned int k = *((const unsigned int *)pixelbase);
		for (x = startx;x < endx;x++)
			outi[x] = k;
		return;
	}
	filter = texture->filter & DPSOFTRAST_TEXTURE_FILTER_LINEAR;
	data = _mm_load_ps(span->data[0][arrayindex]);
	slope = _mm_load_ps(span->data[1][arrayindex]);
	flags = texture->flags;
	tcsize = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i *)&texture->mipmap[mip][0]), _MM_SHUFFLE(3, 2, 3, 2));
	tcmask = _mm_sub_epi32(tcsize, _mm_set1_epi32(1));
	tcscale = _mm_cvtepi32_ps(tcsize);
	data = _mm_mul_ps(_mm_shuffle_ps(data, data, _MM_SHUFFLE(1, 0, 1, 0)), tcscale);
	slope = _mm_mul_ps(_mm_shuffle_ps(slope, slope, _MM_SHUFFLE(1, 0, 1, 0)), tcscale);
	endtc = _mm_sub_ps(_mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx))), _mm_set1_ps(zf[startx])), _mm_set1_ps(0.5f));
	endsubtc = _mm_cvtps_epi32(_mm_mul_ps(endtc, _mm_set1_ps(65536.0f)));
	tcoffset = _mm_add_epi32(_mm_slli_epi32(_mm_shuffle_epi32(tcsize, _MM_SHUFFLE(0, 0, 0, 0)), 18), _mm_set1_epi32(4));
	tcmax = filter ? _mm_packs_epi32(tcmask, tcmask) : _mm_slli_epi32(tcmask, 16);  
	for (x = startx;x < endx;)
	{
		int nextsub = x + DPSOFTRAST_MAXSUBSPAN, endsub = nextsub - 1;
		__m128 subscale = _mm_set1_ps(65536.0f/DPSOFTRAST_MAXSUBSPAN);
		if(nextsub >= endx)
		{
			nextsub = endsub = endx-1;
			if(x < nextsub) subscale = _mm_set1_ps(65536.0f / (nextsub - x));
		}	
		tc = endtc;
		subtc = endsubtc;
		endtc = _mm_sub_ps(_mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(nextsub))), _mm_set1_ps(zf[nextsub])), _mm_set1_ps(0.5f));
		substep = _mm_cvtps_epi32(_mm_mul_ps(_mm_sub_ps(endtc, tc), subscale));
		endsubtc = _mm_cvtps_epi32(_mm_mul_ps(endtc, _mm_set1_ps(65536.0f)));
		if (filter)
		{
			__m128i tcrange = _mm_srai_epi32(_mm_unpacklo_epi64(subtc, _mm_add_epi32(endsubtc, substep)), 16);
			if (_mm_movemask_epi8(_mm_andnot_si128(_mm_cmplt_epi32(tcrange, _mm_setzero_si128()), _mm_cmplt_epi32(tcrange, tcmask))) == 0xFFFF)
			{
				subtc = _mm_unpacklo_epi64(subtc, _mm_add_epi32(subtc, substep));
				substep = _mm_slli_epi32(substep, 1);
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, pix3, pix4, fracm;
					tci = _mm_madd_epi16(_mm_add_epi16(tci, _mm_setr_epi32(0, 0x10000, 0, 0x10000)), tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(tci)]), _mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))]), _mm_setzero_si128());
					pix3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]), _mm_setzero_si128());
					pix4 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))]), _mm_setzero_si128());
					fracm = _mm_srli_epi16(subtc, 1);
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shuffle_epi32(_mm_shufflelo_epi16(fracm, _MM_SHUFFLE(2, 2, 2, 2)), _MM_SHUFFLE(1, 0, 1, 0))));
					pix3 = _mm_add_epi16(pix3,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix4, pix3), 1),
														 _mm_shuffle_epi32(_mm_shufflehi_epi16(fracm, _MM_SHUFFLE(2, 2, 2, 2)), _MM_SHUFFLE(3, 2, 3, 2))));
					pix2 = _mm_unpacklo_epi64(pix1, pix3);
					pix4 = _mm_unpackhi_epi64(pix1, pix3);
					pix2 = _mm_add_epi16(pix2,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix4, pix2), 1),
														 _mm_shufflehi_epi16(_mm_shufflelo_epi16(fracm, _MM_SHUFFLE(0, 0, 0, 0)), _MM_SHUFFLE(0, 0, 0, 0))));
					_mm_storel_epi64((__m128i *)&outi[x], _mm_packus_epi16(pix2, _mm_shufflelo_epi16(pix2, _MM_SHUFFLE(3, 2, 3, 2))));
				}
				if (x <= endsub)
				{
					__m128i tci = _mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, fracm;
					tci = _mm_madd_epi16(_mm_add_epi16(tci, _mm_setr_epi32(0, 0x10000, 0, 0)), tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(tci)]), _mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))]), _mm_setzero_si128());
					fracm = _mm_srli_epi16(subtc, 1);
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shuffle_epi32(_mm_shufflelo_epi16(fracm, _MM_SHUFFLE(2, 2, 2, 2)), _MM_SHUFFLE(1, 0, 1, 0))));
					pix2 = _mm_shuffle_epi32(pix1, _MM_SHUFFLE(3, 2, 3, 2));
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shufflelo_epi16(fracm, _MM_SHUFFLE(0, 0, 0, 0))));
					outi[x] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
					x++;
				}
			}
			else if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
			{
				for (; x <= endsub; x++, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, fracm;
					tci = _mm_min_epi16(_mm_max_epi16(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), _mm_setzero_si128()), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]), 
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])), 
											_mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]), 
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])), 
											_mm_setzero_si128());
					fracm = _mm_srli_epi16(subtc, 1);
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shuffle_epi32(_mm_shufflelo_epi16(fracm, _MM_SHUFFLE(2, 2, 2, 2)), _MM_SHUFFLE(1, 0, 1, 0))));
					pix2 = _mm_shuffle_epi32(pix1, _MM_SHUFFLE(3, 2, 3, 2));
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shufflelo_epi16(fracm, _MM_SHUFFLE(0, 0, 0, 0))));
					outi[x] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
				}
			}
			else
			{
				for (; x <= endsub; x++, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, fracm;
					tci = _mm_and_si128(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]),											
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])),
											_mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])),
											_mm_setzero_si128());
					fracm = _mm_srli_epi16(subtc, 1);
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shuffle_epi32(_mm_shufflelo_epi16(fracm, _MM_SHUFFLE(2, 2, 2, 2)), _MM_SHUFFLE(1, 0, 1, 0))));
					pix2 = _mm_shuffle_epi32(pix1, _MM_SHUFFLE(3, 2, 3, 2));
					pix1 = _mm_add_epi16(pix1,
										 _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 1),
														 _mm_shufflelo_epi16(fracm, _MM_SHUFFLE(0, 0, 0, 0))));
					outi[x] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
				}
			}
		}
		else
		{
			subtc = _mm_unpacklo_epi64(subtc, _mm_add_epi32(subtc, substep));
			substep = _mm_slli_epi32(substep, 1);
			if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
			{
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_min_epi16(_mm_max_epi16(subtc, _mm_setzero_si128()), tcmax); 
					tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(tci, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					outi[x+1] = *(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))];
				}
				if (x <= endsub)
				{
					__m128i tci = _mm_min_epi16(_mm_max_epi16(subtc, _mm_setzero_si128()), tcmax);
					tci = _mm_shufflelo_epi16(tci, _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					x++;
				}
			}
			else
			{
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_and_si128(subtc, tcmax); 
					tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(tci, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					outi[x+1] = *(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))];
				}
				if (x <= endsub)
				{
					__m128i tci = _mm_and_si128(subtc, tcmax); 
					tci = _mm_shufflelo_epi16(tci, _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					x++;
				}
			}
		}
	}
#endif
}

void DPSOFTRAST_Draw_Span_TextureCubeVaryingBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char * RESTRICT out4ub, int texunitindex, int arrayindex, const float * RESTRICT zf)
{
	// TODO: IMPLEMENT
	memset(out4ub, 255, span->length*4);
}

float DPSOFTRAST_SampleShadowmap(const float *vector)
{
	// TODO: IMPLEMENT
	return 1.0f;
}

void DPSOFTRAST_Draw_Span_MultiplyVarying(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *in4f, int arrayindex, const float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float c[4];
	float data[4];
	float slope[4];
	float z;
	data[0] = span->data[0][arrayindex][0];
	data[1] = span->data[0][arrayindex][1];
	data[2] = span->data[0][arrayindex][2];
	data[3] = span->data[0][arrayindex][3];
	slope[0] = span->data[1][arrayindex][0];
	slope[1] = span->data[1][arrayindex][1];
	slope[2] = span->data[1][arrayindex][2];
	slope[3] = span->data[1][arrayindex][3];
	for (x = startx;x < endx;x++)
	{
		z = zf[x];
		c[0] = (data[0] + slope[0]*x) * z;
		c[1] = (data[1] + slope[1]*x) * z;
		c[2] = (data[2] + slope[2]*x) * z;
		c[3] = (data[3] + slope[3]*x) * z;
		out4f[x*4+0] = in4f[x*4+0] * c[0];
		out4f[x*4+1] = in4f[x*4+1] * c[1];
		out4f[x*4+2] = in4f[x*4+2] * c[2];
		out4f[x*4+3] = in4f[x*4+3] * c[3];
	}
}

void DPSOFTRAST_Draw_Span_Varying(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, int arrayindex, const float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float c[4];
	float data[4];
	float slope[4];
	float z;
	data[0] = span->data[0][arrayindex][0];
	data[1] = span->data[0][arrayindex][1];
	data[2] = span->data[0][arrayindex][2];
	data[3] = span->data[0][arrayindex][3];
	slope[0] = span->data[1][arrayindex][0];
	slope[1] = span->data[1][arrayindex][1];
	slope[2] = span->data[1][arrayindex][2];
	slope[3] = span->data[1][arrayindex][3];
	for (x = startx;x < endx;x++)
	{
		z = zf[x];
		c[0] = (data[0] + slope[0]*x) * z;
		c[1] = (data[1] + slope[1]*x) * z;
		c[2] = (data[2] + slope[2]*x) * z;
		c[3] = (data[3] + slope[3]*x) * z;
		out4f[x*4+0] = c[0];
		out4f[x*4+1] = c[1];
		out4f[x*4+2] = c[2];
		out4f[x*4+3] = c[3];
	}
}

void DPSOFTRAST_Draw_Span_AddBloom(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f, const float *subcolor)
{
	int x, startx = span->startx, endx = span->endx;
	float c[4], localcolor[4];
	localcolor[0] = subcolor[0];
	localcolor[1] = subcolor[1];
	localcolor[2] = subcolor[2];
	localcolor[3] = subcolor[3];
	for (x = startx;x < endx;x++)
	{
		c[0] = inb4f[x*4+0] - localcolor[0];if (c[0] < 0.0f) c[0] = 0.0f;
		c[1] = inb4f[x*4+1] - localcolor[1];if (c[1] < 0.0f) c[1] = 0.0f;
		c[2] = inb4f[x*4+2] - localcolor[2];if (c[2] < 0.0f) c[2] = 0.0f;
		c[3] = inb4f[x*4+3] - localcolor[3];if (c[3] < 0.0f) c[3] = 0.0f;
		out4f[x*4+0] = ina4f[x*4+0] + c[0];
		out4f[x*4+1] = ina4f[x*4+1] + c[1];
		out4f[x*4+2] = ina4f[x*4+2] + c[2];
		out4f[x*4+3] = ina4f[x*4+3] + c[3];
	}
}

void DPSOFTRAST_Draw_Span_MultiplyBuffers(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
{
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x < endx;x++)
	{
		out4f[x*4+0] = ina4f[x*4+0] * inb4f[x*4+0];
		out4f[x*4+1] = ina4f[x*4+1] * inb4f[x*4+1];
		out4f[x*4+2] = ina4f[x*4+2] * inb4f[x*4+2];
		out4f[x*4+3] = ina4f[x*4+3] * inb4f[x*4+3];
	}
}

void DPSOFTRAST_Draw_Span_AddBuffers(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
{
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x < endx;x++)
	{
		out4f[x*4+0] = ina4f[x*4+0] + inb4f[x*4+0];
		out4f[x*4+1] = ina4f[x*4+1] + inb4f[x*4+1];
		out4f[x*4+2] = ina4f[x*4+2] + inb4f[x*4+2];
		out4f[x*4+3] = ina4f[x*4+3] + inb4f[x*4+3];
	}
}

void DPSOFTRAST_Draw_Span_MixBuffers(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
{
	int x, startx = span->startx, endx = span->endx;
	float a, b;
	for (x = startx;x < endx;x++)
	{
		a = 1.0f - inb4f[x*4+3];
		b = inb4f[x*4+3];
		out4f[x*4+0] = ina4f[x*4+0] * a + inb4f[x*4+0] * b;
		out4f[x*4+1] = ina4f[x*4+1] * a + inb4f[x*4+1] * b;
		out4f[x*4+2] = ina4f[x*4+2] * a + inb4f[x*4+2] * b;
		out4f[x*4+3] = ina4f[x*4+3] * a + inb4f[x*4+3] * b;
	}
}

void DPSOFTRAST_Draw_Span_MixUniformColor(const DPSOFTRAST_State_Draw_Span * RESTRICT span, float *out4f, const float *in4f, const float *color)
{
	int x, startx = span->startx, endx = span->endx;
	float localcolor[4], ilerp, lerp;
	localcolor[0] = color[0];
	localcolor[1] = color[1];
	localcolor[2] = color[2];
	localcolor[3] = color[3];
	ilerp = 1.0f - localcolor[3];
	lerp = localcolor[3];
	for (x = startx;x < endx;x++)
	{
		out4f[x*4+0] = in4f[x*4+0] * ilerp + localcolor[0] * lerp;
		out4f[x*4+1] = in4f[x*4+1] * ilerp + localcolor[1] * lerp;
		out4f[x*4+2] = in4f[x*4+2] * ilerp + localcolor[2] * lerp;
		out4f[x*4+3] = in4f[x*4+3] * ilerp + localcolor[3] * lerp;
	}
}



void DPSOFTRAST_Draw_Span_MultiplyVaryingBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *in4ub, int arrayindex, const float *zf)
{
#ifdef SSE2_PRESENT
	int x;
	int startx = span->startx;
	int endx = span->endx;
	__m128 data = _mm_load_ps(span->data[0][arrayindex]), slope = _mm_load_ps(span->data[1][arrayindex]);
	data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 1, 2));
	slope = _mm_shuffle_ps(slope, slope, _MM_SHUFFLE(3, 0, 1, 2));
	data = _mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx)));
	data = _mm_mul_ps(data, _mm_set1_ps(256.0f));
	slope = _mm_mul_ps(slope, _mm_set1_ps(256.0f));
	for (x = startx;x+2 <= endx;x += 2, data = _mm_add_ps(data, slope))
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&in4ub[x*4]));
		__m128i mod = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x]))), mod2;
		data = _mm_add_ps(data, slope);
		mod2 = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x+1])));
		mod = _mm_unpacklo_epi64(_mm_packs_epi32(mod, mod), _mm_packs_epi32(mod2, mod2));
		pix = _mm_mulhi_epu16(pix, mod);
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
	}
	for (;x < endx;x++, data = _mm_add_ps(data, slope))
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&in4ub[x*4]));
		__m128i mod = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x])));
		mod = _mm_packs_epi32(mod, mod);
		pix = _mm_mulhi_epu16(pix, mod);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
#endif
}

void DPSOFTRAST_Draw_Span_VaryingBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, int arrayindex, const float *zf)
{
#ifdef SSE2_PRESENT
	int x;
	int startx = span->startx;
	int endx = span->endx;
	__m128 data = _mm_load_ps(span->data[0][arrayindex]), slope = _mm_load_ps(span->data[1][arrayindex]);
	data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 1, 2));
	slope = _mm_shuffle_ps(slope, slope, _MM_SHUFFLE(3, 0, 1, 2));
	data = _mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx)));
	data = _mm_mul_ps(data, _mm_set1_ps(255.0f));
	slope = _mm_mul_ps(slope, _mm_set1_ps(255.0f));
	for (x = startx;x+2 <= endx;x += 2, data = _mm_add_ps(data, slope))
	{
		__m128i pix = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x]))), pix2;
		data = _mm_add_ps(data, slope);
		pix2 = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x+1])));
		pix = _mm_unpacklo_epi64(_mm_packs_epi32(pix, pix), _mm_packs_epi32(pix2, pix2));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
	}
	for (;x < endx;x++, data = _mm_add_ps(data, slope))
	{
		__m128i pix = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&zf[x])));
		pix = _mm_packs_epi32(pix, pix);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
#endif
}

void DPSOFTRAST_Draw_Span_AddBloomBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub, const float *subcolor)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	__m128i localcolor = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(subcolor), _mm_set1_ps(255.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	localcolor = _mm_shuffle_epi32(_mm_packs_epi32(localcolor, localcolor), _MM_SHUFFLE(1, 0, 1, 0));
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, _mm_sub_epi16(pix2, localcolor));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if(x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, _mm_sub_epi16(pix2, localcolor));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MultiplyBuffersBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&inb4ub[x*4]));
		pix1 = _mm_mulhi_epu16(pix1, pix2);
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if(x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]));
		pix1 = _mm_mulhi_epu16(pix1, pix2);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_AddBuffersBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, pix2);
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if(x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, pix2);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_TintedAddBuffersBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub, const float *inbtintbgra)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	__m128i tint = _mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(inbtintbgra), _mm_set1_ps(256.0f)));
	tint = _mm_shuffle_epi32(_mm_packs_epi32(tint, tint), _MM_SHUFFLE(1, 0, 1, 0));
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&inb4ub[x*4]));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epu16(tint, pix2));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if(x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epu16(tint, pix2));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MixBuffersBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(pix2, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 4), _mm_slli_epi16(blend, 4)));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if(x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		__m128i blend = _mm_shufflelo_epi16(pix2, _MM_SHUFFLE(3, 3, 3, 3));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 4), _mm_slli_epi16(blend, 4)));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MixUniformColorBGRA8(const DPSOFTRAST_State_Draw_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *in4ub, const float *color)
{
#ifdef SSE2_PRESENT
	int x, startx = span->startx, endx = span->endx;
	__m128i localcolor = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(color), _mm_set1_ps(255.0f))), _MM_SHUFFLE(3, 0, 1, 2)), blend;
	localcolor = _mm_shuffle_epi32(_mm_packs_epi32(localcolor, localcolor), _MM_SHUFFLE(1, 0, 1, 0));
	blend = _mm_slli_epi16(_mm_shufflehi_epi16(_mm_shufflelo_epi16(localcolor, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)), 4);
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&in4ub[x*4]), _mm_setzero_si128());
		pix = _mm_add_epi16(pix, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(localcolor, pix), 4), blend));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
	}
	if(x < endx)
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&in4ub[x*4]), _mm_setzero_si128());
		pix = _mm_add_epi16(pix, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(localcolor, pix), 4), blend));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
#endif
}



void DPSOFTRAST_VertexShader_Generic(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.numvertices);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SPECULAR)
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.numvertices);
}

void DPSOFTRAST_PixelShader_Generic(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_lightmapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_FIRST, 2, buffer_z);
		DPSOFTRAST_Draw_Span_MultiplyVaryingBGRA8(span, buffer_FragColorbgra8, buffer_texture_colorbgra8, 1, buffer_z);
		if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SPECULAR)
		{
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_lightmapbgra8, GL20TU_SECOND, 2, buffer_z);
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				// multiply
				DPSOFTRAST_Draw_Span_MultiplyBuffersBGRA8(span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
			else if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				// add
				DPSOFTRAST_Draw_Span_AddBuffersBGRA8(span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
			else if (dpsoftrast.shader_permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
			{
				// alphablend
				DPSOFTRAST_Draw_Span_MixBuffersBGRA8(span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
		}
	}
	else
		DPSOFTRAST_Draw_Span_VaryingBGRA8(span, buffer_FragColorbgra8, 1, buffer_z);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_PostProcess(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.numvertices);
}

void DPSOFTRAST_PixelShader_PostProcess(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: optimize!!  at the very least there is no reason to use texture sampling on the frame texture
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_FragColorbgra8, GL20TU_FIRST, 2, buffer_z);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_BLOOM)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_SECOND, 3, buffer_z);
		DPSOFTRAST_Draw_Span_AddBloomBGRA8(span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_colorbgra8, dpsoftrast.uniform4f + DPSOFTRAST_UNIFORM_BloomColorSubtract * 4);
	}
	DPSOFTRAST_Draw_Span_MixUniformColorBGRA8(span, buffer_FragColorbgra8, buffer_FragColorbgra8, dpsoftrast.uniform4f + DPSOFTRAST_UNIFORM_ViewTintColor * 4);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SATURATION)
	{
		// TODO: implement saturation
	}
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GAMMARAMPS)
	{
		// TODO: implement gammaramps
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Depth_Or_Shadow(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_Depth_Or_Shadow(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// this is never called (because colormask is off when this shader is used)
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_FlatColor(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
}

void DPSOFTRAST_PixelShader_FlatColor(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	int x, startx = span->startx, endx = span->endx;
	int Color_Ambienti[4];
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	Color_Ambienti[2] = (int)(dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0]*256.0f);
	Color_Ambienti[1] = (int)(dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1]*256.0f);
	Color_Ambienti[0] = (int)(dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2]*256.0f);
	Color_Ambienti[3] = (int)(dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0]        *256.0f);
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_COLOR, 2, buffer_z);
	for (x = startx;x < endx;x++)
	{
		buffer_FragColorbgra8[x*4+0] = (buffer_texture_colorbgra8[x*4+0] * Color_Ambienti[0])>>8;
		buffer_FragColorbgra8[x*4+1] = (buffer_texture_colorbgra8[x*4+1] * Color_Ambienti[1])>>8;
		buffer_FragColorbgra8[x*4+2] = (buffer_texture_colorbgra8[x*4+2] * Color_Ambienti[2])>>8;
		buffer_FragColorbgra8[x*4+3] = (buffer_texture_colorbgra8[x*4+3] * Color_Ambienti[3])>>8;
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_VertexColor(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.numvertices);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
}

void DPSOFTRAST_PixelShader_VertexColor(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4];
	float data[4];
	float slope[4];
	float z;
	int arrayindex = DPSOFTRAST_ARRAY_COLOR;
	data[2] = span->data[0][arrayindex][0];
	data[1] = span->data[0][arrayindex][1];
	data[0] = span->data[0][arrayindex][2];
	data[3] = span->data[0][arrayindex][3];
	slope[2] = span->data[1][arrayindex][0];
	slope[1] = span->data[1][arrayindex][1];
	slope[0] = span->data[1][arrayindex][2];
	slope[3] = span->data[1][arrayindex][3];
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
	Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
	Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
	Color_Diffuse[3] = 0.0f;
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_COLOR, 2, buffer_z);
	for (x = startx;x < endx;x++)
	{
		z = buffer_z[x];
		buffer_FragColorbgra8[x*4+0] = (int)(buffer_texture_colorbgra8[x*4+0] * (Color_Ambient[0] + ((data[0] + slope[0]*x) * z) * Color_Diffuse[0]));
		buffer_FragColorbgra8[x*4+1] = (int)(buffer_texture_colorbgra8[x*4+1] * (Color_Ambient[1] + ((data[1] + slope[1]*x) * z) * Color_Diffuse[1]));
		buffer_FragColorbgra8[x*4+2] = (int)(buffer_texture_colorbgra8[x*4+2] * (Color_Ambient[2] + ((data[2] + slope[2]*x) * z) * Color_Diffuse[2]));
		buffer_FragColorbgra8[x*4+3] = (int)(buffer_texture_colorbgra8[x*4+3] * (Color_Ambient[3] + ((data[3] + slope[3]*x) * z) * Color_Diffuse[3]));
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Lightmap(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.numvertices);
}

void DPSOFTRAST_PixelShader_Lightmap(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
#ifdef SSE2_PRESENT
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0] + span->start * 4;
	int x, startx = span->startx, endx = span->endx;
	__m128i Color_Ambientm, Color_Diffusem, Color_Glowm, Color_AmbientGlowm;
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_lightmapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glowbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
	if (dpsoftrast.user.alphatest || dpsoftrast.fb_blendmode != DPSOFTRAST_BLENDMODE_OPAQUE)
		pixel = buffer_FragColorbgra8;
	Color_Ambientm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Ambientm = _mm_and_si128(Color_Ambientm, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Ambientm = _mm_or_si128(Color_Ambientm, _mm_setr_epi32(0, 0, 0, (int)(dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0]*255.0f)));
	Color_Ambientm = _mm_packs_epi32(Color_Ambientm, Color_Ambientm);
	Color_Diffusem = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Diffusem = _mm_and_si128(Color_Diffusem, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Diffusem = _mm_packs_epi32(Color_Diffusem, Color_Diffusem);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GLOW)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_glowbgra8, GL20TU_GLOW, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		Color_Glowm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
		Color_Glowm = _mm_and_si128(Color_Glowm, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
		Color_Glowm = _mm_packs_epi32(Color_Glowm, Color_Glowm);
		Color_AmbientGlowm = _mm_unpacklo_epi64(Color_Ambientm, Color_Glowm);
		for (x = startx;x < endx;x++)
		{
			__m128i color, lightmap, glow, pix;
			if (x + 4 <= endx && *(const unsigned int *)&pixelmask[x] == 0x01010101)
			{
				__m128i pix2;
				color = _mm_loadu_si128((const __m128i *)&buffer_texture_colorbgra8[x*4]);
				lightmap = _mm_loadu_si128((const __m128i *)&buffer_texture_lightmapbgra8[x*4]);
				glow = _mm_loadu_si128((const __m128i *)&buffer_texture_glowbgra8[x*4]);
				pix = _mm_add_epi16(_mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, _mm_unpacklo_epi8(_mm_setzero_si128(), lightmap)), Color_Ambientm), 
													_mm_unpacklo_epi8(_mm_setzero_si128(), color)),
									_mm_mulhi_epu16(Color_Glowm, _mm_unpacklo_epi8(_mm_setzero_si128(), glow)));
				pix2 = _mm_add_epi16(_mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, _mm_unpackhi_epi8(_mm_setzero_si128(), lightmap)), Color_Ambientm), 
													_mm_unpackhi_epi8(_mm_setzero_si128(), color)),
									_mm_mulhi_epu16(Color_Glowm, _mm_unpackhi_epi8(_mm_setzero_si128(), glow)));
				_mm_storeu_si128((__m128i *)&pixel[x*4], _mm_packus_epi16(pix, pix2));
				x += 3;
				continue;
			}
			if(!pixelmask[x])
				continue;
			color = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_colorbgra8[x*4]));
			lightmap = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_lightmapbgra8[x*4]));
			glow = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_glowbgra8[x*4]));
			pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, lightmap), Color_AmbientGlowm), _mm_unpacklo_epi64(color, glow));
			pix = _mm_add_epi16(pix, _mm_shuffle_epi32(pix, _MM_SHUFFLE(3, 2, 3, 2)));
			*(int *)&pixel[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
		}
	}
	else
	{
		for (x = startx;x < endx;x++)
		{
			__m128i color, lightmap, pix;
			if (x + 4 <= endx && *(const unsigned int *)&pixelmask[x] == 0x01010101)
			{
				__m128i pix2;
				color = _mm_loadu_si128((const __m128i *)&buffer_texture_colorbgra8[x*4]);
				lightmap = _mm_loadu_si128((const __m128i *)&buffer_texture_lightmapbgra8[x*4]);
				pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, _mm_unpacklo_epi8(_mm_setzero_si128(), lightmap)), Color_Ambientm), 
									  _mm_unpacklo_epi8(_mm_setzero_si128(), color));
				pix2 = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, _mm_unpackhi_epi8(_mm_setzero_si128(), lightmap)), Color_Ambientm),
									   _mm_unpackhi_epi8(_mm_setzero_si128(), color));
				_mm_storeu_si128((__m128i *)&pixel[x*4], _mm_packus_epi16(pix, pix2));
				x += 3;
				continue;
			}
			if(!pixelmask[x]) 
				continue;
			color = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_colorbgra8[x*4]));
			lightmap = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_lightmapbgra8[x*4]));
			pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(lightmap, Color_Diffusem), Color_Ambientm), color);
			*(int *)&pixel[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
		}
	}
	if(pixel == buffer_FragColorbgra8)
		DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
#endif
}



void DPSOFTRAST_VertexShader_FakeLight(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_FakeLight(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_LightDirectionMap_ModelSpace(void)
{
	DPSOFTRAST_VertexShader_Lightmap();
}

void DPSOFTRAST_PixelShader_LightDirectionMap_ModelSpace(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	DPSOFTRAST_PixelShader_Lightmap(span);
	// TODO: IMPLEMENT
}



void DPSOFTRAST_VertexShader_LightDirectionMap_TangentSpace(void)
{
	DPSOFTRAST_VertexShader_Lightmap();
}

void DPSOFTRAST_PixelShader_LightDirectionMap_TangentSpace(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	DPSOFTRAST_PixelShader_Lightmap(span);
	// TODO: IMPLEMENT
}



void DPSOFTRAST_VertexShader_LightDirection(void)
{
	int i;
	int numvertices = dpsoftrast.draw.numvertices;
	float LightDir[4];
	float LightVector[4];
	float EyePosition[4];
	float EyeVectorModelSpace[4];
	float EyeVector[4];
	float position[4];
	float svector[4];
	float tvector[4];
	float normal[4];
	LightDir[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightDir*4+0];
	LightDir[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightDir*4+1];
	LightDir[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightDir*4+2];
	LightDir[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightDir*4+3];
	EyePosition[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+0];
	EyePosition[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+1];
	EyePosition[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+2];
	EyePosition[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+3];
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	for (i = 0;i < numvertices;i++)
	{
		position[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0];
		position[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1];
		position[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+2];
		svector[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0];
		svector[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1];
		svector[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2];
		tvector[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0];
		tvector[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1];
		tvector[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2];
		normal[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+0];
		normal[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+1];
		normal[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+2];
		LightVector[0] = svector[0] * LightDir[0] + svector[1] * LightDir[1] + svector[2] * LightDir[2];
		LightVector[1] = tvector[0] * LightDir[0] + tvector[1] * LightDir[1] + tvector[2] * LightDir[2];
		LightVector[2] = normal[0] * LightDir[0] + normal[1] * LightDir[1] + normal[2] * LightDir[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0] = LightVector[0];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1] = LightVector[1];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2] = LightVector[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+3] = 0.0f;
		EyeVectorModelSpace[0] = EyePosition[0] - position[0];
		EyeVectorModelSpace[1] = EyePosition[1] - position[1];
		EyeVectorModelSpace[2] = EyePosition[2] - position[2];
		EyeVector[0] = svector[0] * EyeVectorModelSpace[0] + svector[1] * EyeVectorModelSpace[1] + svector[2] * EyeVectorModelSpace[2];
		EyeVector[1] = tvector[0] * EyeVectorModelSpace[0] + tvector[1] * EyeVectorModelSpace[1] + tvector[2] * EyeVectorModelSpace[2];
		EyeVector[2] = normal[0]  * EyeVectorModelSpace[0] + normal[1]  * EyeVectorModelSpace[1] + normal[2]  * EyeVectorModelSpace[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0] = EyeVector[0];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1] = EyeVector[1];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2] = EyeVector[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+3] = 0.0f;
	}
}

#define DPSOFTRAST_Min(a,b) ((a) < (b) ? (a) : (b))
#define DPSOFTRAST_Max(a,b) ((a) > (b) ? (a) : (b))
#define DPSOFTRAST_Vector3Dot(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define DPSOFTRAST_Vector3LengthSquared(v) (DPSOFTRAST_Vector3Dot((v),(v)))
#define DPSOFTRAST_Vector3Length(v) (sqrt(DPSOFTRAST_Vector3LengthSquared(v)))
#define DPSOFTRAST_Vector3Normalize(v)\
do\
{\
	float len = sqrt(DPSOFTRAST_Vector3Dot(v,v));\
	if (len)\
	{\
		len = 1.0f / len;\
		v[0] *= len;\
		v[1] *= len;\
		v[2] *= len;\
	}\
}\
while(0)

void DPSOFTRAST_PixelShader_LightDirection(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_normalbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glossbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glowbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_pantsbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_shirtbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4], Color_Specular[4], Color_Glow[4], Color_Pants[4], Color_Shirt[4], LightColor[4];
	float LightVectordata[4];
	float LightVectorslope[4];
	float EyeVectordata[4];
	float EyeVectorslope[4];
	float z;
	float diffusetex[4];
	float glosstex[4];
	float surfacenormal[4];
	float lightnormal[4];
	float eyenormal[4];
	float specularnormal[4];
	float diffuse;
	float specular;
	float SpecularPower;
	int d[4];
	Color_Glow[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+0];
	Color_Glow[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+1];
	Color_Glow[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+2];
	Color_Glow[3] = 0.0f;
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Pants[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+0];
	Color_Pants[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+1];
	Color_Pants[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+2];
	Color_Pants[3] = 0.0f;
	Color_Shirt[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+0];
	Color_Shirt[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+1];
	Color_Shirt[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+2];
	Color_Shirt[3] = 0.0f;
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_pantsbgra8, GL20TU_PANTS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_shirtbgra8, GL20TU_SHIRT, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GLOW)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_glowbgra8, GL20TU_GLOW, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SPECULAR)
	{
		Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
		Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
		Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
		Color_Diffuse[3] = 0.0f;
		LightColor[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
		LightColor[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
		LightColor[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
		LightColor[3] = 0.0f;
		LightVectordata[0]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][0];
		LightVectordata[1]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][1];
		LightVectordata[2]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][2];
		LightVectordata[3]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][3];
		LightVectorslope[0] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][0];
		LightVectorslope[1] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][1];
		LightVectorslope[2] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][2];
		LightVectorslope[3] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][3];
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		Color_Specular[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+0];
		Color_Specular[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+1];
		Color_Specular[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+2];
		Color_Specular[3] = 0.0f;
		SpecularPower = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_SpecularPower*4+0] * (1.0f / 255.0f);
		EyeVectordata[0]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][0];
		EyeVectordata[1]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][1];
		EyeVectordata[2]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][2];
		EyeVectordata[3]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][3];
		EyeVectorslope[0]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][0];
		EyeVectorslope[1]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][1];
		EyeVectorslope[2]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][2];
		EyeVectorslope[3]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][3];
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_glossbgra8, GL20TU_GLOSS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				diffusetex[0] += buffer_texture_pantsbgra8[x*4+0] * Color_Pants[0] + buffer_texture_shirtbgra8[x*4+0] * Color_Shirt[0];
				diffusetex[1] += buffer_texture_pantsbgra8[x*4+1] * Color_Pants[1] + buffer_texture_shirtbgra8[x*4+1] * Color_Shirt[1];
				diffusetex[2] += buffer_texture_pantsbgra8[x*4+2] * Color_Pants[2] + buffer_texture_shirtbgra8[x*4+2] * Color_Shirt[2];
				diffusetex[3] += buffer_texture_pantsbgra8[x*4+3] * Color_Pants[3] + buffer_texture_shirtbgra8[x*4+3] * Color_Shirt[3];
			}
			glosstex[0] = buffer_texture_glossbgra8[x*4+0];
			glosstex[1] = buffer_texture_glossbgra8[x*4+1];
			glosstex[2] = buffer_texture_glossbgra8[x*4+2];
			glosstex[3] = buffer_texture_glossbgra8[x*4+3];
			surfacenormal[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
			DPSOFTRAST_Vector3Normalize(surfacenormal);

			lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
			lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
			lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(lightnormal);

			eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
			eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
			eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(eyenormal);

			specularnormal[0] = lightnormal[0] + eyenormal[0];
			specularnormal[1] = lightnormal[1] + eyenormal[1];
			specularnormal[2] = lightnormal[2] + eyenormal[2];
			DPSOFTRAST_Vector3Normalize(specularnormal);

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;
			specular = DPSOFTRAST_Vector3Dot(surfacenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			specular = pow(specular, SpecularPower * glosstex[3]);
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GLOW)
			{
				d[0] = (int)(buffer_texture_glowbgra8[x*4+0] * Color_Glow[0] + diffusetex[0] * Color_Ambient[0] + (diffusetex[0] * Color_Diffuse[0] * diffuse + glosstex[0] * Color_Specular[0] * specular) * LightColor[0]);if (d[0] > 255) d[0] = 255;
				d[1] = (int)(buffer_texture_glowbgra8[x*4+1] * Color_Glow[1] + diffusetex[1] * Color_Ambient[1] + (diffusetex[1] * Color_Diffuse[1] * diffuse + glosstex[1] * Color_Specular[1] * specular) * LightColor[1]);if (d[1] > 255) d[1] = 255;
				d[2] = (int)(buffer_texture_glowbgra8[x*4+2] * Color_Glow[2] + diffusetex[2] * Color_Ambient[2] + (diffusetex[2] * Color_Diffuse[2] * diffuse + glosstex[2] * Color_Specular[2] * specular) * LightColor[2]);if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * Color_Ambient[3]);if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)(                                                  diffusetex[0] * Color_Ambient[0] + (diffusetex[0] * Color_Diffuse[0] * diffuse + glosstex[0] * Color_Specular[0] * specular) * LightColor[0]);if (d[0] > 255) d[0] = 255;
				d[1] = (int)(                                                  diffusetex[1] * Color_Ambient[1] + (diffusetex[1] * Color_Diffuse[1] * diffuse + glosstex[1] * Color_Specular[1] * specular) * LightColor[1]);if (d[1] > 255) d[1] = 255;
				d[2] = (int)(                                                  diffusetex[2] * Color_Ambient[2] + (diffusetex[2] * Color_Diffuse[2] * diffuse + glosstex[2] * Color_Specular[2] * specular) * LightColor[2]);if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * Color_Ambient[3]);if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	else if (dpsoftrast.shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
		Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
		Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
		Color_Diffuse[3] = 0.0f;
		LightColor[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
		LightColor[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
		LightColor[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
		LightColor[3] = 0.0f;
		LightVectordata[0]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][0];
		LightVectordata[1]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][1];
		LightVectordata[2]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][2];
		LightVectordata[3]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][3];
		LightVectorslope[0] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][0];
		LightVectorslope[1] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][1];
		LightVectorslope[2] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][2];
		LightVectorslope[3] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][3];
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			surfacenormal[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
			DPSOFTRAST_Vector3Normalize(surfacenormal);

			lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
			lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
			lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(lightnormal);

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GLOW)
			{
				d[0] = (int)(buffer_texture_glowbgra8[x*4+0] * Color_Glow[0] + diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse * LightColor[0]));if (d[0] > 255) d[0] = 255;
				d[1] = (int)(buffer_texture_glowbgra8[x*4+1] * Color_Glow[1] + diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse * LightColor[1]));if (d[1] > 255) d[1] = 255;
				d[2] = (int)(buffer_texture_glowbgra8[x*4+2] * Color_Glow[2] + diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse * LightColor[2]));if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * (Color_Ambient[3]                                             ));if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)(                                                + diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse * LightColor[0]));if (d[0] > 255) d[0] = 255;
				d[1] = (int)(                                                + diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse * LightColor[1]));if (d[1] > 255) d[1] = 255;
				d[2] = (int)(                                                + diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse * LightColor[2]));if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * (Color_Ambient[3]                                             ));if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	else
	{
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];

			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GLOW)
			{
				d[0] = (int)(buffer_texture_glowbgra8[x*4+0] * Color_Glow[0] + diffusetex[0] * Color_Ambient[0]);if (d[0] > 255) d[0] = 255;
				d[1] = (int)(buffer_texture_glowbgra8[x*4+1] * Color_Glow[1] + diffusetex[1] * Color_Ambient[1]);if (d[1] > 255) d[1] = 255;
				d[2] = (int)(buffer_texture_glowbgra8[x*4+2] * Color_Glow[2] + diffusetex[2] * Color_Ambient[2]);if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * Color_Ambient[3]);if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)(                                                  diffusetex[0] * Color_Ambient[0]);if (d[0] > 255) d[0] = 255;
				d[1] = (int)(                                                  diffusetex[1] * Color_Ambient[1]);if (d[1] > 255) d[1] = 255;
				d[2] = (int)(                                                  diffusetex[2] * Color_Ambient[2]);if (d[2] > 255) d[2] = 255;
				d[3] = (int)(                                                  diffusetex[3] * Color_Ambient[3]);if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_LightSource(void)
{
	int i;
	int numvertices = dpsoftrast.draw.numvertices;
	float LightPosition[4];
	float LightVector[4];
	float LightVectorModelSpace[4];
	float EyePosition[4];
	float EyeVectorModelSpace[4];
	float EyeVector[4];
	float position[4];
	float svector[4];
	float tvector[4];
	float normal[4];
	LightPosition[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightPosition*4+0];
	LightPosition[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightPosition*4+1];
	LightPosition[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightPosition*4+2];
	LightPosition[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightPosition*4+3];
	EyePosition[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+0];
	EyePosition[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+1];
	EyePosition[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+2];
	EyePosition[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+3];
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelToLightM1);
	DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.numvertices);
	for (i = 0;i < numvertices;i++)
	{
		position[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0];
		position[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1];
		position[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+2];
		svector[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0];
		svector[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1];
		svector[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2];
		tvector[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0];
		tvector[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1];
		tvector[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2];
		normal[0] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+0];
		normal[1] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+1];
		normal[2] = dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+2];
		LightVectorModelSpace[0] = LightPosition[0] - position[0];
		LightVectorModelSpace[1] = LightPosition[1] - position[1];
		LightVectorModelSpace[2] = LightPosition[2] - position[2];
		LightVector[0] = svector[0] * LightVectorModelSpace[0] + svector[1] * LightVectorModelSpace[1] + svector[2] * LightVectorModelSpace[2];
		LightVector[1] = tvector[0] * LightVectorModelSpace[0] + tvector[1] * LightVectorModelSpace[1] + tvector[2] * LightVectorModelSpace[2];
		LightVector[2] = normal[0]  * LightVectorModelSpace[0] + normal[1]  * LightVectorModelSpace[1] + normal[2]  * LightVectorModelSpace[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0] = LightVector[0];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1] = LightVector[1];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2] = LightVector[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+3] = 0.0f;
		EyeVectorModelSpace[0] = EyePosition[0] - position[0];
		EyeVectorModelSpace[1] = EyePosition[1] - position[1];
		EyeVectorModelSpace[2] = EyePosition[2] - position[2];
		EyeVector[0] = svector[0] * EyeVectorModelSpace[0] + svector[1] * EyeVectorModelSpace[1] + svector[2] * EyeVectorModelSpace[2];
		EyeVector[1] = tvector[0] * EyeVectorModelSpace[0] + tvector[1] * EyeVectorModelSpace[1] + tvector[2] * EyeVectorModelSpace[2];
		EyeVector[2] = normal[0]  * EyeVectorModelSpace[0] + normal[1]  * EyeVectorModelSpace[1] + normal[2]  * EyeVectorModelSpace[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0] = EyeVector[0];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1] = EyeVector[1];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2] = EyeVector[2];
		dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+3] = 0.0f;
	}
}

void DPSOFTRAST_PixelShader_LightSource(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_normalbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glossbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_cubebgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_pantsbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_shirtbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4], Color_Specular[4], Color_Glow[4], Color_Pants[4], Color_Shirt[4], LightColor[4];
	float CubeVectordata[4];
	float CubeVectorslope[4];
	float LightVectordata[4];
	float LightVectorslope[4];
	float EyeVectordata[4];
	float EyeVectorslope[4];
	float z;
	float diffusetex[4];
	float glosstex[4];
	float surfacenormal[4];
	float lightnormal[4];
	float eyenormal[4];
	float specularnormal[4];
	float diffuse;
	float specular;
	float SpecularPower;
	float CubeVector[4];
	float attenuation;
	int d[4];
	Color_Glow[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+0];
	Color_Glow[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+1];
	Color_Glow[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+2];
	Color_Glow[3] = 0.0f;
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
	Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
	Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
	Color_Diffuse[3] = 0.0f;
	Color_Specular[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+0];
	Color_Specular[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+1];
	Color_Specular[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+2];
	Color_Specular[3] = 0.0f;
	Color_Pants[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+0];
	Color_Pants[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+1];
	Color_Pants[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+2];
	Color_Pants[3] = 0.0f;
	Color_Shirt[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+0];
	Color_Shirt[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+1];
	Color_Shirt[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+2];
	Color_Shirt[3] = 0.0f;
	LightColor[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
	LightColor[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
	LightColor[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
	LightColor[3] = 0.0f;
	SpecularPower = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_SpecularPower*4+0] * (1.0f / 255.0f);
	EyeVectordata[0]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][0];
	EyeVectordata[1]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][1];
	EyeVectordata[2]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][2];
	EyeVectordata[3]    = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD2][3];
	EyeVectorslope[0]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][0];
	EyeVectorslope[1]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][1];
	EyeVectorslope[2]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][2];
	EyeVectorslope[3]   = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD2][3];
	LightVectordata[0]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][0];
	LightVectordata[1]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][1];
	LightVectordata[2]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][2];
	LightVectordata[3]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD1][3];
	LightVectorslope[0] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][0];
	LightVectorslope[1] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][1];
	LightVectorslope[2] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][2];
	LightVectorslope[3] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD1][3];
	CubeVectordata[0]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD3][0];
	CubeVectordata[1]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD3][1];
	CubeVectordata[2]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD3][2];
	CubeVectordata[3]  = span->data[0][DPSOFTRAST_ARRAY_TEXCOORD3][3];
	CubeVectorslope[0] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD3][0];
	CubeVectorslope[1] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD3][1];
	CubeVectorslope[2] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD3][2];
	CubeVectorslope[3] = span->data[1][DPSOFTRAST_ARRAY_TEXCOORD3][3];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8 + startx*4, 0, (endx-startx)*4); // clear first, because we skip writing black pixels, and there are a LOT of them...
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_pantsbgra8, GL20TU_PANTS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_shirtbgra8, GL20TU_SHIRT, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_CUBEFILTER)
		DPSOFTRAST_Draw_Span_TextureCubeVaryingBGRA8(span, buffer_texture_cubebgra8, GL20TU_CUBE, DPSOFTRAST_ARRAY_TEXCOORD3, buffer_z);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SPECULAR)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_glossbgra8, GL20TU_GLOSS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			CubeVector[0] = (CubeVectordata[0] + CubeVectorslope[0]*x) * z;
			CubeVector[1] = (CubeVectordata[1] + CubeVectorslope[1]*x) * z;
			CubeVector[2] = (CubeVectordata[2] + CubeVectorslope[2]*x) * z;
			attenuation = 1.0f - DPSOFTRAST_Vector3LengthSquared(CubeVector);
			if (attenuation < 0.01f)
				continue;
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				diffusetex[0] += buffer_texture_pantsbgra8[x*4+0] * Color_Pants[0] + buffer_texture_shirtbgra8[x*4+0] * Color_Shirt[0];
				diffusetex[1] += buffer_texture_pantsbgra8[x*4+1] * Color_Pants[1] + buffer_texture_shirtbgra8[x*4+1] * Color_Shirt[1];
				diffusetex[2] += buffer_texture_pantsbgra8[x*4+2] * Color_Pants[2] + buffer_texture_shirtbgra8[x*4+2] * Color_Shirt[2];
				diffusetex[3] += buffer_texture_pantsbgra8[x*4+3] * Color_Pants[3] + buffer_texture_shirtbgra8[x*4+3] * Color_Shirt[3];
			}
			glosstex[0] = buffer_texture_glossbgra8[x*4+0];
			glosstex[1] = buffer_texture_glossbgra8[x*4+1];
			glosstex[2] = buffer_texture_glossbgra8[x*4+2];
			glosstex[3] = buffer_texture_glossbgra8[x*4+3];
			surfacenormal[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
			DPSOFTRAST_Vector3Normalize(surfacenormal);

			lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
			lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
			lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(lightnormal);

			eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
			eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
			eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(eyenormal);

			specularnormal[0] = lightnormal[0] + eyenormal[0];
			specularnormal[1] = lightnormal[1] + eyenormal[1];
			specularnormal[2] = lightnormal[2] + eyenormal[2];
			DPSOFTRAST_Vector3Normalize(specularnormal);

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;
			specular = DPSOFTRAST_Vector3Dot(surfacenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			specular = pow(specular, SpecularPower * glosstex[3]);
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_CUBEFILTER)
			{
				// scale down the attenuation to account for the cubefilter multiplying everything by 255
				attenuation *= (1.0f / 255.0f);
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse) + glosstex[0] * Color_Specular[0] * specular) * LightColor[0] * buffer_texture_cubebgra8[x*4+0] * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse) + glosstex[1] * Color_Specular[1] * specular) * LightColor[1] * buffer_texture_cubebgra8[x*4+1] * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse) + glosstex[2] * Color_Specular[2] * specular) * LightColor[2] * buffer_texture_cubebgra8[x*4+2] * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                                                                                                );if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse) + glosstex[0] * Color_Specular[0] * specular) * LightColor[0]                                   * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse) + glosstex[1] * Color_Specular[1] * specular) * LightColor[1]                                   * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse) + glosstex[2] * Color_Specular[2] * specular) * LightColor[2]                                   * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                                                                                                );if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	else if (dpsoftrast.shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			CubeVector[0] = (CubeVectordata[0] + CubeVectorslope[0]*x) * z;
			CubeVector[1] = (CubeVectordata[1] + CubeVectorslope[1]*x) * z;
			CubeVector[2] = (CubeVectordata[2] + CubeVectorslope[2]*x) * z;
			attenuation = 1.0f - DPSOFTRAST_Vector3LengthSquared(CubeVector);
			if (attenuation < 0.01f)
				continue;
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				diffusetex[0] += buffer_texture_pantsbgra8[x*4+0] * Color_Pants[0] + buffer_texture_shirtbgra8[x*4+0] * Color_Shirt[0];
				diffusetex[1] += buffer_texture_pantsbgra8[x*4+1] * Color_Pants[1] + buffer_texture_shirtbgra8[x*4+1] * Color_Shirt[1];
				diffusetex[2] += buffer_texture_pantsbgra8[x*4+2] * Color_Pants[2] + buffer_texture_shirtbgra8[x*4+2] * Color_Shirt[2];
				diffusetex[3] += buffer_texture_pantsbgra8[x*4+3] * Color_Pants[3] + buffer_texture_shirtbgra8[x*4+3] * Color_Shirt[3];
			}
			surfacenormal[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
			surfacenormal[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
			DPSOFTRAST_Vector3Normalize(surfacenormal);

			lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
			lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
			lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
			DPSOFTRAST_Vector3Normalize(lightnormal);

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_CUBEFILTER)
			{
				// scale down the attenuation to account for the cubefilter multiplying everything by 255
				attenuation *= (1.0f / 255.0f);
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse)) * LightColor[0] * buffer_texture_cubebgra8[x*4+0] * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse)) * LightColor[1] * buffer_texture_cubebgra8[x*4+1] * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse)) * LightColor[2] * buffer_texture_cubebgra8[x*4+2] * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                                                   );if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0] + Color_Diffuse[0] * diffuse)) * LightColor[0]                                   * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1] + Color_Diffuse[1] * diffuse)) * LightColor[1]                                   * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2] + Color_Diffuse[2] * diffuse)) * LightColor[2]                                   * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                                                                                                );if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	else
	{
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			CubeVector[0] = (CubeVectordata[0] + CubeVectorslope[0]*x) * z;
			CubeVector[1] = (CubeVectordata[1] + CubeVectorslope[1]*x) * z;
			CubeVector[2] = (CubeVectordata[2] + CubeVectorslope[2]*x) * z;
			attenuation = 1.0f - DPSOFTRAST_Vector3LengthSquared(CubeVector);
			if (attenuation < 0.01f)
				continue;
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				diffusetex[0] += buffer_texture_pantsbgra8[x*4+0] * Color_Pants[0] + buffer_texture_shirtbgra8[x*4+0] * Color_Shirt[0];
				diffusetex[1] += buffer_texture_pantsbgra8[x*4+1] * Color_Pants[1] + buffer_texture_shirtbgra8[x*4+1] * Color_Shirt[1];
				diffusetex[2] += buffer_texture_pantsbgra8[x*4+2] * Color_Pants[2] + buffer_texture_shirtbgra8[x*4+2] * Color_Shirt[2];
				diffusetex[3] += buffer_texture_pantsbgra8[x*4+3] * Color_Pants[3] + buffer_texture_shirtbgra8[x*4+3] * Color_Shirt[3];
			}
			if (dpsoftrast.shader_permutation & SHADERPERMUTATION_CUBEFILTER)
			{
				// scale down the attenuation to account for the cubefilter multiplying everything by 255
				attenuation *= (1.0f / 255.0f);
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0])) * LightColor[0] * buffer_texture_cubebgra8[x*4+0] * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1])) * LightColor[1] * buffer_texture_cubebgra8[x*4+1] * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2])) * LightColor[2] * buffer_texture_cubebgra8[x*4+2] * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                      );if (d[3] > 255) d[3] = 255;
			}
			else
			{
				d[0] = (int)((diffusetex[0] * (Color_Ambient[0])) * LightColor[0]                                   * attenuation);if (d[0] > 255) d[0] = 255;
				d[1] = (int)((diffusetex[1] * (Color_Ambient[1])) * LightColor[1]                                   * attenuation);if (d[1] > 255) d[1] = 255;
				d[2] = (int)((diffusetex[2] * (Color_Ambient[2])) * LightColor[2]                                   * attenuation);if (d[2] > 255) d[2] = 255;
				d[3] = (int)( diffusetex[3]                                                                                                                                                                );if (d[3] > 255) d[3] = 255;
			}
			buffer_FragColorbgra8[x*4+0] = d[0];
			buffer_FragColorbgra8[x*4+1] = d[1];
			buffer_FragColorbgra8[x*4+2] = d[2];
			buffer_FragColorbgra8[x*4+3] = d[3];
		}
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Refraction(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_Refraction(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Water(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}


void DPSOFTRAST_PixelShader_Water(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_ShowDepth(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_ShowDepth(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_DeferredGeometry(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_DeferredGeometry(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_DeferredLightSource(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_DeferredLightSource(const DPSOFTRAST_State_Draw_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
	memset(buffer_FragColorbgra8, 0, span->length*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(span, buffer_FragColorbgra8);
}



typedef struct DPSOFTRAST_ShaderModeInfo_s
{
	int lodarrayindex;
	void (*Vertex)(void);
	void (*Span)(const DPSOFTRAST_State_Draw_Span * RESTRICT span);
}
DPSOFTRAST_ShaderModeInfo;

DPSOFTRAST_ShaderModeInfo DPSOFTRAST_ShaderModeTable[SHADERMODE_COUNT] =
{
	{2, DPSOFTRAST_VertexShader_Generic,                        DPSOFTRAST_PixelShader_Generic,                      },
	{2, DPSOFTRAST_VertexShader_PostProcess,                    DPSOFTRAST_PixelShader_PostProcess,                  },
	{2, DPSOFTRAST_VertexShader_Depth_Or_Shadow,                DPSOFTRAST_PixelShader_Depth_Or_Shadow,              },
	{2, DPSOFTRAST_VertexShader_FlatColor,                      DPSOFTRAST_PixelShader_FlatColor,                    },
	{2, DPSOFTRAST_VertexShader_VertexColor,                    DPSOFTRAST_PixelShader_VertexColor,                  },
	{2, DPSOFTRAST_VertexShader_Lightmap,                       DPSOFTRAST_PixelShader_Lightmap,                     },
	{2, DPSOFTRAST_VertexShader_FakeLight,                      DPSOFTRAST_PixelShader_FakeLight,                    },
	{2, DPSOFTRAST_VertexShader_LightDirectionMap_ModelSpace,   DPSOFTRAST_PixelShader_LightDirectionMap_ModelSpace, },
	{2, DPSOFTRAST_VertexShader_LightDirectionMap_TangentSpace, DPSOFTRAST_PixelShader_LightDirectionMap_TangentSpace},
	{2, DPSOFTRAST_VertexShader_LightDirection,                 DPSOFTRAST_PixelShader_LightDirection,               },
	{2, DPSOFTRAST_VertexShader_LightSource,                    DPSOFTRAST_PixelShader_LightSource,                  },
	{2, DPSOFTRAST_VertexShader_Refraction,                     DPSOFTRAST_PixelShader_Refraction,                   },
	{2, DPSOFTRAST_VertexShader_Water,                          DPSOFTRAST_PixelShader_Water,                        },
	{2, DPSOFTRAST_VertexShader_ShowDepth,                      DPSOFTRAST_PixelShader_ShowDepth,                    },
	{2, DPSOFTRAST_VertexShader_DeferredGeometry,               DPSOFTRAST_PixelShader_DeferredGeometry,             },
	{2, DPSOFTRAST_VertexShader_DeferredLightSource,            DPSOFTRAST_PixelShader_DeferredLightSource,          }
};



void DPSOFTRAST_Draw_ProcessSpans(void)
{
	int i;
	int x;
	int startx;
	int endx;
	int numspans = dpsoftrast.draw.numspans;
//	unsigned int c;
//	unsigned int *colorpixel;
	unsigned int *depthpixel;
	float w;
	float wslope;
	int depth;
	int depthslope;
	unsigned int d;
	DPSOFTRAST_State_Draw_Span *span = dpsoftrast.draw.spanqueue;
	unsigned char pixelmask[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	for (i = 0;i < numspans;i++, span++)
	{
		w = span->data[0][DPSOFTRAST_ARRAY_TOTAL][3];
		wslope = span->data[1][DPSOFTRAST_ARRAY_TOTAL][3];
		if (dpsoftrast.user.depthtest && dpsoftrast.fb_depthpixels)
		{
			depthslope = (int)(wslope*DPSOFTRAST_DEPTHSCALE);
			depth = (int)(w*DPSOFTRAST_DEPTHSCALE - DPSOFTRAST_DEPTHOFFSET*(dpsoftrast.user.polygonoffset[1] + fabs(wslope)*dpsoftrast.user.polygonoffset[0]));
			depthpixel = dpsoftrast.fb_depthpixels + span->start;
			switch(dpsoftrast.fb_depthfunc)
			{
			default:
			case GL_ALWAYS:  for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = true; break;
			case GL_LESS:    for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = depthpixel[x] < d; break;
			case GL_LEQUAL:  for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = depthpixel[x] <= d; break;
			case GL_EQUAL:   for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = depthpixel[x] == d; break;
			case GL_GEQUAL:  for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = depthpixel[x] >= d; break;
			case GL_GREATER: for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = depthpixel[x] > d; break;
			case GL_NEVER:   for (x = 0, d = depth;x < span->length;x++, d += depthslope) pixelmask[x] = false; break;
			}
			//colorpixel = dpsoftrast.fb_colorpixels[0] + span->start;
			//for (x = 0;x < span->length;x++)
			//	colorpixel[x] = (depthpixel[x] & 0xFF000000) ? (0x00FF0000) : (depthpixel[x] & 0x00FF0000);
			// if there is no color buffer, skip pixel shader
			startx = 0;
			endx = span->length;
			while (startx < endx && !pixelmask[startx])
				startx++;
			while (endx > startx && !pixelmask[endx-1])
				endx--;
			if (startx >= endx)
				continue; // no pixels to fill
			span->pixelmask = pixelmask;
			span->startx = startx;
			span->endx = endx;
			// run pixel shader if appropriate
			// do this before running depthmask code, to allow the pixelshader
			// to clear pixelmask values for alpha testing
			if (dpsoftrast.fb_colorpixels[0] && dpsoftrast.fb_colormask)
				DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].Span(span);
			if (dpsoftrast.user.depthmask)
				for (x = 0, d = depth;x < span->length;x++, d += depthslope)
					if (pixelmask[x])
						depthpixel[x] = d;
		}
		else
		{
			// no depth testing means we're just dealing with color...
			// if there is no color buffer, skip pixel shader
			if (dpsoftrast.fb_colorpixels[0] && dpsoftrast.fb_colormask)
			{
				memset(pixelmask, 1, span->length);
				span->pixelmask = pixelmask;
				span->startx = 0;
				span->endx = span->length;
				DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].Span(span);
			}
		}
	}
}

void DPSOFTRAST_Draw_ProcessTriangles(int firstvertex, int numtriangles, const int *element3i, const unsigned short *element3s, unsigned char *arraymask)
{
#ifdef SSE2_PRESENT
	int cullface = dpsoftrast.user.cullface;
	int width = dpsoftrast.fb_width;
	int height = dpsoftrast.fb_height;
	__m128i fbmax = _mm_sub_epi16(_mm_setr_epi16(width, height, width, height, width, height, width, height), _mm_set1_epi16(1));
	int i;
	int j;
	int k;
	int y;
	int e[3];
	__m128i screeny;
	int starty, endy;
	int numpoints;
	int edge0p;
	int edge0n;
	int edge1p;
	int edge1n;
	int startx;
	int endx;
	unsigned char mip[DPSOFTRAST_MAXTEXTUREUNITS];
	__m128 mipedgescale;
	float clipdist[4];
	__m128 clipped[DPSOFTRAST_ARRAY_TOTAL][4];
	__m128 screen[4];
	__m128 proj[DPSOFTRAST_ARRAY_TOTAL][4];
	DPSOFTRAST_Texture *texture;
	DPSOFTRAST_State_Draw_Span *span;
	DPSOFTRAST_State_Draw_Span *oldspan;
	for (i = 0;i < numtriangles;i++)
	{
		// generate the 3 edges of this triangle
		// generate spans for the triangle - switch based on left split or right split classification of triangle
		if (element3i)
		{
			e[0] = element3i[i*3+0] - firstvertex;
			e[1] = element3i[i*3+1] - firstvertex;
			e[2] = element3i[i*3+2] - firstvertex;
		}
		else if (element3s)
		{
			e[0] = element3s[i*3+0] - firstvertex;
			e[1] = element3s[i*3+1] - firstvertex;
			e[2] = element3s[i*3+2] - firstvertex;
		}
		else
		{
			e[0] = i*3+0;
			e[1] = i*3+1;
			e[2] = i*3+2;
		}

#define SKIPBACKFACE { \
			__m128 triangleedge[2], triangleorigin, trianglenormal; \
			triangleorigin = _mm_load_ps(&dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4]); \
			triangleedge[0] = _mm_sub_ps(_mm_load_ps(&dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4]), triangleorigin); \
			triangleedge[1] = _mm_sub_ps(_mm_load_ps(&dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4]), triangleorigin); \
			/* store normal in 2, 0, 1 order instead of 0, 1, 2 as it requires fewer shuffles and leaves z component accessible as scalar */ \
			trianglenormal = _mm_sub_ss(_mm_mul_ss(triangleedge[0], _mm_shuffle_ps(triangleedge[1], triangleedge[1], _MM_SHUFFLE(3, 0, 2, 1))), \
										_mm_mul_ss(_mm_shuffle_ps(triangleedge[0], triangleedge[0], _MM_SHUFFLE(3, 0, 2, 1)), triangleedge[1])); \
			/* apply current cullface mode (this culls many triangles) */ \
			switch(cullface) \
			{ \
			case GL_BACK: \
				if (_mm_ucomilt_ss(trianglenormal, _mm_setzero_ps())) \
					continue; \
				break; \
			case GL_FRONT: \
				if (_mm_ucomigt_ss(trianglenormal, _mm_setzero_ps())) \
					continue; \
				break; \
			} \
		}
			//trianglenormal = _mm_sub_ps(_mm_mul_ps(triangleedge[0], _mm_shuffle_ps(triangleedge[1], triangleedge[1], _MM_SHUFFLE(3, 0, 2, 1))),
			//						  _mm_mul_ps(_mm_shuffle_ps(triangleedge[0], triangleedge[0], _MM_SHUFFLE(3, 0, 2, 1)), triangleedge[1]));
			//trianglenormal[2] = triangleedge[0][0] * triangleedge[1][1] - triangleedge[0][1] * triangleedge[1][0];
			//trianglenormal[0] = triangleedge[0][1] * triangleedge[1][2] - triangleedge[0][2] * triangleedge[1][1];
			//trianglenormal[1] = triangleedge[0][2] * triangleedge[1][0] - triangleedge[0][0] * triangleedge[1][2];

			// macros for clipping vertices
#define CLIPPEDVERTEXLERP(k,p1,p2) { \
			__m128 frac = _mm_set1_ps(clipdist[p1] / (clipdist[p1] - clipdist[p2]));\
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)\
			{\
				/*if (arraymask[j])*/\
				{\
					__m128 v1 = _mm_load_ps(&dpsoftrast.draw.post_array4f[j][e[p1]*4]), v2 = _mm_load_ps(&dpsoftrast.draw.post_array4f[j][e[p2]*4]); \
					clipped[j][k] = _mm_add_ps(v1, _mm_mul_ps(_mm_sub_ps(v2, v1), frac)); \
				}\
			}\
			screen[k] = DPSOFTRAST_Draw_ProjectVertex(clipped[DPSOFTRAST_ARRAY_POSITION][k]); \
		}
#define CLIPPEDVERTEXCOPY(k,p1) \
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)\
			{\
				/*if (arraymask[j])*/\
				{\
					clipped[j][k] = _mm_load_ps(&dpsoftrast.draw.post_array4f[j][e[p1]*4]); \
				}\
			}\
			screen[k] = _mm_load_ps(&dpsoftrast.draw.screencoord4f[e[p1]*4]);

		// calculate distance from nearplane
		clipdist[0] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4+2] + 1.0f;
		clipdist[1] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+2] + 1.0f;
		clipdist[2] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4+2] + 1.0f;
		if (clipdist[0] >= 0.0f)
		{
			SKIPBACKFACE;
			if (clipdist[1] >= 0.0f)
			{
				CLIPPEDVERTEXCOPY(0,0);
				CLIPPEDVERTEXCOPY(1,1);
				if (clipdist[2] >= 0.0f)
				{
					// triangle is entirely in front of nearplane
					CLIPPEDVERTEXCOPY(2,2);
					numpoints = 3;
				}
				else
				{
					CLIPPEDVERTEXLERP(2,1,2);
					CLIPPEDVERTEXLERP(3,2,0);
					numpoints = 4;
				}
			}
			else 
			{
				CLIPPEDVERTEXCOPY(0,0);
				CLIPPEDVERTEXLERP(1,0,1);
				if (clipdist[2] >= 0.0f)
				{
					CLIPPEDVERTEXLERP(2,1,2);
					CLIPPEDVERTEXCOPY(3,2);
					numpoints = 4;
				}
				else
				{
					CLIPPEDVERTEXLERP(2,2,0);
		   			numpoints = 3;
				}
			}
		}			
		else if (clipdist[1] >= 0.0f)
		{
			SKIPBACKFACE;
			CLIPPEDVERTEXLERP(0,0,1);
			CLIPPEDVERTEXCOPY(1,1);
			if (clipdist[2] >= 0.0f)
			{
		   		CLIPPEDVERTEXCOPY(2,2);
				CLIPPEDVERTEXLERP(3,2,0);
				numpoints = 4;
			}
			else
			{
				CLIPPEDVERTEXLERP(2,1,2);
				numpoints = 3;
			}
		}
		else if (clipdist[2] >= 0.0f)
		{
			SKIPBACKFACE;
			CLIPPEDVERTEXLERP(0,1,2);
			CLIPPEDVERTEXCOPY(1,2);
			CLIPPEDVERTEXLERP(2,2,0);
			numpoints = 3;
		}
		else continue; // triangle is entirely behind nearpla

		{
			// calculate integer y coords for triangle points
			__m128i screeni = _mm_packs_epi32(_mm_cvttps_epi32(_mm_shuffle_ps(screen[0], screen[1], _MM_SHUFFLE(1, 0, 1, 0))),
										  _mm_cvttps_epi32(_mm_shuffle_ps(screen[2], numpoints <= 3 ? screen[2] : screen[3], _MM_SHUFFLE(1, 0, 1, 0)))),
					screenir = _mm_shuffle_epi32(screeni, _MM_SHUFFLE(1, 0, 3, 2)), 
					screenmin = _mm_min_epi16(screeni, screenir), 
					screenmax = _mm_max_epi16(screeni, screenir);
			screenmin = _mm_min_epi16(screenmin, _mm_shufflelo_epi16(screenmin, _MM_SHUFFLE(1, 0, 3, 2)));
			screenmax = _mm_max_epi16(screenmax, _mm_shufflelo_epi16(screenmax, _MM_SHUFFLE(1, 0, 3, 2)));
			screenmin = _mm_max_epi16(screenmin, _mm_setzero_si128());
			screenmax = _mm_min_epi16(screenmax, fbmax);
			// skip offscreen triangles
			if (_mm_cvtsi128_si32(_mm_cmplt_epi16(screenmax, screenmin)) == -1)
				continue;
			starty = _mm_extract_epi16(screenmin, 1);
			endy = _mm_extract_epi16(screenmax, 1)+1;
			screeny = _mm_srai_epi32(screeni, 16);
		}

		// okay, this triangle is going to produce spans, we'd better project
		// the interpolants now (this is what gives perspective texturing),
		// this consists of simply multiplying all arrays by the W coord
		// (which is basically 1/Z), which will be undone per-pixel
		// (multiplying by Z again) to get the perspective-correct array
		// values
		for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
		{
			//if (arraymask[j])
			{
				for (k = 0;k < numpoints;k++)
				{
					proj[j][k] = _mm_mul_ps(clipped[j][k], _mm_shuffle_ps(screen[k], screen[k], _MM_SHUFFLE(3, 3, 3, 3)));
				}
			}
		}
		// adjust texture LOD by texture density, in the simplest way possible...
		mipedgescale = _mm_sub_ps(_mm_shuffle_ps(screen[0], screen[2], _MM_SHUFFLE(1, 0, 1, 0)), _mm_shuffle_ps(screen[1], screen[1], _MM_SHUFFLE(1, 0, 1, 0)));
		mipedgescale = _mm_mul_ps(mipedgescale, mipedgescale);
		mipedgescale = _mm_div_ps(_mm_set1_ps(1.0f), _mm_add_ps(mipedgescale, _mm_shuffle_ps(mipedgescale, mipedgescale, _MM_SHUFFLE(2, 3, 0, 1))));
		for (j = 0;j < DPSOFTRAST_MAXTEXTUREUNITS;j++)
		{
			texture = dpsoftrast.texbound[j];
			if (texture)
			{
				__m128 mipedgetc;
				if (texture->filter <= DPSOFTRAST_TEXTURE_FILTER_LINEAR)
				{
					mip[j] = 0;
					continue;
				}
				k = DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].lodarrayindex;
				mipedgetc = _mm_sub_ps(_mm_shuffle_ps(clipped[k][0], clipped[k][2], _MM_SHUFFLE(1, 0, 1, 0)),
										_mm_shuffle_ps(clipped[k][1], clipped[k][1], _MM_SHUFFLE(1, 0, 1, 0)));
				mipedgetc = _mm_mul_ps(mipedgetc, _mm_cvtepi32_ps(_mm_shuffle_epi32(_mm_loadl_epi64((const __m128i *)&texture->mipmap[0][2]), _MM_SHUFFLE(1, 0, 1, 0))));
				mipedgetc = _mm_mul_ps(mipedgetc, mipedgetc);
				mipedgetc = _mm_add_ps(mipedgetc, _mm_shuffle_ps(mipedgetc, mipedgetc, _MM_SHUFFLE(2, 3, 0, 1)));
				mipedgetc = _mm_mul_ps(mipedgetc, mipedgescale);
				mipedgetc = _mm_min_ss(mipedgetc, _mm_shuffle_ps(mipedgetc, mipedgetc, _MM_SHUFFLE(2, 2, 2, 2)));	
				// this will be multiplied in the texturing routine by the texture resolution
				y = _mm_cvttss_si32(mipedgetc);
				if (y > 0) 
				{
					y = (int)(log(y)/M_LN2);
					if (y > texture->mipmaps - 1)
						y = texture->mipmaps - 1;
				}
				else y = 0;
				mip[j] = y;
			}
		}
		// iterate potential spans
		// TODO: optimize?  if we figured out the edge order beforehand, this
		//	   could do loops over the edges in the proper order rather than
		//	   selecting them for each span
		// TODO: optimize?  the edges could have data slopes calculated
		// TODO: optimize?  the data slopes could be calculated as a plane
		//	   (2D slopes) to avoid any interpolation along edges at all
		for (y = starty+1;y < endy;)
		{
			int nexty = -1;
			__m128 edge0lerp, edge1lerp, edge0scale, edge1scale;
			__m128i screenycc = _mm_cmpgt_epi32(_mm_set1_epi32(y), screeny);
			int screenymask = _mm_movemask_epi8(screenycc);
			if (numpoints == 4)
			{
				switch(screenymask)
				{
				default:
				case 0xFFFF: /*0000*/ y++; continue;
				case 0xFFF0: /*1000*/ edge0p = 3;edge0n = 0;edge1p = 1;edge1n = 0;break;
				case 0xFF0F: /*0100*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 1;break;
				case 0xFF00: /*1100*/ edge0p = 3;edge0n = 0;edge1p = 2;edge1n = 1;break;
				case 0xF0FF: /*0010*/ edge0p = 1;edge0n = 2;edge1p = 3;edge1n = 2;break;
				case 0xF0F0: /*1010*/ edge0p = 1;edge0n = 2;edge1p = 3;edge1n = 2;break; // concave - nonsense
				case 0xF00F: /*0110*/ edge0p = 0;edge0n = 1;edge1p = 3;edge1n = 2;break;
				case 0xF000: /*1110*/ edge0p = 3;edge0n = 0;edge1p = 3;edge1n = 2;break;
				case 0x0FFF: /*0001*/ edge0p = 2;edge0n = 3;edge1p = 0;edge1n = 3;break;
				case 0x0FF0: /*1001*/ edge0p = 2;edge0n = 3;edge1p = 1;edge1n = 0;break;
				case 0x0F0F: /*0101*/ edge0p = 2;edge0n = 3;edge1p = 1;edge1n = 2;break; // concave - nonsense
				case 0x0F00: /*1101*/ edge0p = 2;edge0n = 3;edge1p = 2;edge1n = 1;break;
				case 0x00FF: /*0011*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 3;break;
				case 0x00F0: /*1011*/ edge0p = 1;edge0n = 2;edge1p = 1;edge1n = 0;break;
				case 0x000F: /*0111*/ edge0p = 0;edge0n = 1;edge1p = 0;edge1n = 3;break;
				case 0x0000: /*1111*/ y++; continue;
				}
			}
			else
			{
				switch(screenymask)
				{
				default:
				case 0xFFFF: /*000*/ y++; continue;
				case 0xFFF0: /*100*/ edge0p = 2;edge0n = 0;edge1p = 1;edge1n = 0;break;
				case 0xFF0F: /*010*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 1;break;
				case 0xFF00: /*110*/ edge0p = 2;edge0n = 0;edge1p = 2;edge1n = 1;break;
				case 0x00FF: /*001*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 2;break;
				case 0x00F0: /*101*/ edge0p = 1;edge0n = 2;edge1p = 1;edge1n = 0;break;
				case 0x000F: /*011*/ edge0p = 0;edge0n = 1;edge1p = 0;edge1n = 2;break;
				case 0x0000: /*111*/ y++; continue;
				}
			}
			screenycc = _mm_max_epi16(_mm_srli_epi16(screenycc, 1), screeny);
			screenycc = _mm_min_epi16(screenycc, _mm_shuffle_epi32(screenycc, _MM_SHUFFLE(1, 0, 3, 2)));  
			screenycc = _mm_min_epi16(screenycc, _mm_shuffle_epi32(screenycc, _MM_SHUFFLE(2, 3, 0, 1)));
			nexty = _mm_extract_epi16(screenycc, 0);	
			if(nexty >= endy) nexty = endy-1;
			if (_mm_ucomigt_ss(_mm_max_ss(screen[edge0n], screen[edge0p]), _mm_min_ss(screen[edge1n], screen[edge1p])))
			{
				int tmp = edge0n;
				edge0n = edge1n;
				edge1n = tmp;
				tmp = edge0p;
				edge0p = edge1p;
				edge1p = tmp;
			} 	
			edge0lerp = _mm_shuffle_ps(screen[edge0p], screen[edge0p], _MM_SHUFFLE(1, 1, 1, 1));
			edge0scale = _mm_div_ss(_mm_set1_ps(1.0f), _mm_sub_ss(_mm_shuffle_ps(screen[edge0n], screen[edge0n], _MM_SHUFFLE(1, 1, 1, 1)), edge0lerp));
			edge0scale = _mm_shuffle_ps(edge0scale, edge0scale, _MM_SHUFFLE(0, 0, 0, 0));
			edge0lerp = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(y), edge0lerp), edge0scale);
			edge1lerp = _mm_shuffle_ps(screen[edge1p], screen[edge1p], _MM_SHUFFLE(1, 1, 1, 1));
   			edge1scale = _mm_div_ss(_mm_set1_ps(1.0f), _mm_sub_ss(_mm_shuffle_ps(screen[edge1n], screen[edge1n], _MM_SHUFFLE(1, 1, 1, 1)), edge1lerp));
			edge1scale = _mm_shuffle_ps(edge1scale, edge1scale, _MM_SHUFFLE(0, 0, 0, 0));
			edge1lerp = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(y), edge1lerp), edge1scale);
			for(; y <= nexty; y++, edge0lerp = _mm_add_ps(edge0lerp, edge0scale), edge1lerp = _mm_add_ps(edge1lerp, edge1scale))
			{
				__m128 data0, data1, spanilength, startxlerp;
#if 0
				if (_mm_ucomilt_ss(edge0lerp, _mm_setzero_ps()) || _mm_ucomilt_ss(edge1lerp, _mm_setzero_ps()) ||
					_mm_ucomigt_ss(edge0lerp, _mm_set1_ps(1)) || _mm_ucomigt_ss(edge1lerp, _mm_set1_ps(1)))
					continue;
#endif
				data0 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(screen[edge0n], screen[edge0p]), edge0lerp), screen[edge0p]);
				data1 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(screen[edge1n], screen[edge1p]), edge1lerp), screen[edge1p]);
				startx = _mm_cvtss_si32(_mm_add_ss(data0, _mm_set1_ps(0.5f)));
				endx = _mm_cvtss_si32(_mm_add_ss(data1, _mm_set1_ps(0.5f)));
				if (startx < 0) startx = 0;
				if (endx > width) endx = width;
				if (startx >= endx) continue;
#if 0
				_mm_store_ss(&startxf, data0);
				_mm_store_ss(&endxf, data1);
				if (startxf > startx || endxf < endx-1) { printf("%s:%i X wrong (%i to %i is outside %f to %f)\n", __FILE__, __LINE__, startx, endx, startxf, endxf); }
#endif
				spanilength = _mm_div_ss(_mm_set1_ps(1.0f), _mm_sub_ss(data1, data0));
				spanilength = _mm_shuffle_ps(spanilength, spanilength, _MM_SHUFFLE(0, 0, 0, 0));
				startxlerp = _mm_sub_ps(_mm_set1_ps(startx), _mm_shuffle_ps(data0, data0, _MM_SHUFFLE(0, 0, 0, 0)));
				span = &dpsoftrast.draw.spanqueue[dpsoftrast.draw.numspans++];
				memcpy(span->mip, mip, sizeof(span->mip));
				span->start = y * width + startx;
				span->length = endx - startx;
				j = DPSOFTRAST_ARRAY_TOTAL;
				data1 = _mm_mul_ps(_mm_sub_ps(data1, data0), spanilength);
				data0 = _mm_add_ps(data0, _mm_mul_ps(data1, startxlerp));
				_mm_store_ps(span->data[0][j], data0);
				_mm_store_ps(span->data[1][j], data1);
				for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
				{
					//if (arraymask[j])
					{
						data0 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(proj[j][edge0n], proj[j][edge0p]), edge0lerp), proj[j][edge0p]);
						data1 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(proj[j][edge1n], proj[j][edge1p]), edge1lerp), proj[j][edge1p]);
						data1 = _mm_mul_ps(_mm_sub_ps(data1, data0), spanilength);
						data0 = _mm_add_ps(data0, _mm_mul_ps(data1, startxlerp));
						_mm_store_ps(span->data[0][j], data0);
						_mm_store_ps(span->data[1][j], data1);
					}
				}
				// to keep the shader routines from needing more than a small
				// buffer for pixel intermediate data, we split long spans...
				while (span->length > DPSOFTRAST_DRAW_MAXSPANLENGTH)
				{
					span->length = DPSOFTRAST_DRAW_MAXSPANLENGTH;
					if (dpsoftrast.draw.numspans >= DPSOFTRAST_DRAW_MAXSPANQUEUE)
					{
						DPSOFTRAST_Draw_ProcessSpans();
						dpsoftrast.draw.numspans = 0;
					}
					oldspan = span;
					span = &dpsoftrast.draw.spanqueue[dpsoftrast.draw.numspans++];
					*span = *oldspan;
					startx += DPSOFTRAST_DRAW_MAXSPANLENGTH;
					span->start = y * width + startx;
					span->length = endx - startx;
					j = DPSOFTRAST_ARRAY_TOTAL;
					_mm_store_ps(span->data[0][j], _mm_add_ps(_mm_load_ps(span->data[0][j]), _mm_mul_ps(_mm_load_ps(span->data[1][j]), _mm_set1_ps(DPSOFTRAST_DRAW_MAXSPANLENGTH))));
					for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
					{
						//if (arraymask[j])
						{
							 _mm_store_ps(span->data[0][j], _mm_add_ps(_mm_load_ps(span->data[0][j]), _mm_mul_ps(_mm_load_ps(span->data[1][j]), _mm_set1_ps(DPSOFTRAST_DRAW_MAXSPANLENGTH))));
						}
					}
				}
				// after all that, we have a span suitable for the pixel shader...
				if (dpsoftrast.draw.numspans >= DPSOFTRAST_DRAW_MAXSPANQUEUE)
				{
					DPSOFTRAST_Draw_ProcessSpans();
					dpsoftrast.draw.numspans = 0;
				}
			}
		}
		// draw outlines over triangle for debugging
	//	for (j = 0, k = numpoints-1;j < numpoints;k = j, j++)
	//		DPSOFTRAST_Draw_DebugEdgePoints(screen[k], screen[j]);
	}
	if (dpsoftrast.draw.numspans)
	{
		DPSOFTRAST_Draw_ProcessSpans();
		dpsoftrast.draw.numspans = 0;
	}
#endif
}

void DPSOFTRAST_Draw_DebugPoints(void)
{
	int i;
	int x;
	int y;
	int numvertices = dpsoftrast.draw.numvertices;
	int w = dpsoftrast.fb_width;
	int bounds[4];
	unsigned int *pixels = dpsoftrast.fb_colorpixels[0];
	const float *c4f;
	bounds[0] = dpsoftrast.fb_viewportscissor[0];
	bounds[1] = dpsoftrast.fb_viewportscissor[1];
	bounds[2] = dpsoftrast.fb_viewportscissor[0] + dpsoftrast.fb_viewportscissor[2];
	bounds[3] = dpsoftrast.fb_viewportscissor[1] + dpsoftrast.fb_viewportscissor[3];
	for (i = 0;i < numvertices;i++)
	{
		// check nearclip
		//if (dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+3] != 1.0f)
		//	continue;
		x = (int)(dpsoftrast.draw.screencoord4f[i*4+0]);
		y = (int)(dpsoftrast.draw.screencoord4f[i*4+1]);
		//x = (int)(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0] + 0.5f);
		//y = (int)(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1] + 0.5f);
		//x = (int)((dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0] + 1.0f) * dpsoftrast.fb_width * 0.5f + 0.5f);
		//y = (int)((dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1] + 1.0f) * dpsoftrast.fb_height * 0.5f + 0.5f);
		if (x < bounds[0] || y < bounds[1] || x >= bounds[2] || y >= bounds[3])
			continue;
		c4f = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR] + i*4;
		pixels[y*w+x] = DPSOFTRAST_BGRA8_FROM_RGBA32F(c4f[0], c4f[1], c4f[2], c4f[3]);
	}
}

void DPSOFTRAST_DrawTriangles(int firstvertex, int numvertices, int numtriangles, const int *element3i, const unsigned short *element3s)
{
	unsigned char arraymask[DPSOFTRAST_ARRAY_TOTAL];
	arraymask[0] = true;
	arraymask[1] = dpsoftrast.fb_colorpixels[0] != NULL; // TODO: optimize (decide based on shadermode)
	arraymask[2] = dpsoftrast.pointer_texcoordf[0] != NULL;
	arraymask[3] = dpsoftrast.pointer_texcoordf[1] != NULL;
	arraymask[4] = dpsoftrast.pointer_texcoordf[2] != NULL;
	arraymask[5] = dpsoftrast.pointer_texcoordf[3] != NULL;
	arraymask[6] = dpsoftrast.pointer_texcoordf[4] != NULL;
	arraymask[7] = dpsoftrast.pointer_texcoordf[5] != NULL;
	arraymask[8] = dpsoftrast.pointer_texcoordf[6] != NULL;
	arraymask[9] = dpsoftrast.pointer_texcoordf[7] != NULL;
	DPSOFTRAST_Validate(DPSOFTRAST_VALIDATE_DRAW);
	DPSOFTRAST_Draw_LoadVertices(firstvertex, numvertices, true);
	DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].Vertex();
	DPSOFTRAST_Draw_ProjectVertices(dpsoftrast.draw.screencoord4f, dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], numvertices);
	DPSOFTRAST_Draw_ProcessTriangles(firstvertex, numtriangles, element3i, element3s, arraymask);
}

void DPSOFTRAST_Init(int width, int height, unsigned int *colorpixels, unsigned int *depthpixels)
{
	union
	{
		int i;
		unsigned char b[4];
	}
	u;
	u.i = 1;
	memset(&dpsoftrast, 0, sizeof(dpsoftrast));
	dpsoftrast.bigendian = u.b[3];
	dpsoftrast.fb_width = width;
	dpsoftrast.fb_height = height;
	dpsoftrast.fb_depthpixels = depthpixels;
	dpsoftrast.fb_colorpixels[0] = colorpixels;
	dpsoftrast.fb_colorpixels[1] = NULL;
	dpsoftrast.fb_colorpixels[1] = NULL;
	dpsoftrast.fb_colorpixels[1] = NULL;
	dpsoftrast.texture_firstfree = 1;
	dpsoftrast.texture_end = 1;
	dpsoftrast.texture_max = 0;
	dpsoftrast.user.colormask[0] = 1;
	dpsoftrast.user.colormask[1] = 1;
	dpsoftrast.user.colormask[2] = 1;
	dpsoftrast.user.colormask[3] = 1;
	dpsoftrast.user.blendfunc[0] = GL_ONE;
	dpsoftrast.user.blendfunc[1] = GL_ZERO;
	dpsoftrast.user.depthmask = true;
	dpsoftrast.user.depthtest = true;
	dpsoftrast.user.depthfunc = GL_LEQUAL;
	dpsoftrast.user.scissortest = false;
	dpsoftrast.user.cullface = GL_BACK;
	dpsoftrast.user.alphatest = false;
	dpsoftrast.user.alphafunc = GL_GREATER;
	dpsoftrast.user.alphavalue = 0.5f;
	dpsoftrast.user.scissor[0] = 0;
	dpsoftrast.user.scissor[1] = 0;
	dpsoftrast.user.scissor[2] = dpsoftrast.fb_width;
	dpsoftrast.user.scissor[3] = dpsoftrast.fb_height;
	dpsoftrast.user.viewport[0] = 0;
	dpsoftrast.user.viewport[1] = 0;
	dpsoftrast.user.viewport[2] = dpsoftrast.fb_width;
	dpsoftrast.user.viewport[3] = dpsoftrast.fb_height;
	dpsoftrast.user.depthrange[0] = 0;
	dpsoftrast.user.depthrange[1] = 1;
	dpsoftrast.user.polygonoffset[0] = 0;
	dpsoftrast.user.polygonoffset[1] = 0;
	dpsoftrast.user.color[0] = 1;
	dpsoftrast.user.color[1] = 1;
	dpsoftrast.user.color[2] = 1;
	dpsoftrast.user.color[3] = 1;
	dpsoftrast.validate = -1;
	DPSOFTRAST_Validate(-1);
	dpsoftrast.validate = 0;
}

void DPSOFTRAST_Shutdown(void)
{
	int i;
	for (i = 0;i < dpsoftrast.texture_end;i++)
		if (dpsoftrast.texture[i].bytes)
			MM_FREE(dpsoftrast.texture[i].bytes);
	if (dpsoftrast.texture)
		free(dpsoftrast.texture);
	memset(&dpsoftrast, 0, sizeof(dpsoftrast));
}

