
#include <memory.h>
#include "dpsoftrast.h"
#include <stdio.h>
#include <math.h>

#undef true
#undef false
#ifndef __cplusplus
typedef enum bool {false, true} bool;
#endif

#define GL_NONE					0
#define GL_FRONT_LEFT			0x0400
#define GL_FRONT_RIGHT			0x0401
#define GL_BACK_LEFT			0x0402
#define GL_BACK_RIGHT			0x0403
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407
#define GL_FRONT_AND_BACK		0x0408
#define GL_AUX0					0x0409
#define GL_AUX1					0x040A
#define GL_AUX2					0x040B
#define GL_AUX3					0x040C

#define GL_NEVER				0x0200
#define GL_LESS					0x0201
#define GL_EQUAL				0x0202
#define GL_LEQUAL				0x0203
#define GL_GREATER				0x0204
#define GL_NOTEQUAL				0x0205
#define GL_GEQUAL				0x0206
#define GL_ALWAYS				0x0207

#define GL_ZERO					0x0
#define GL_ONE					0x1
#define GL_SRC_COLOR				0x0300
#define GL_ONE_MINUS_SRC_COLOR			0x0301
#define GL_DST_COLOR				0x0306
#define GL_ONE_MINUS_DST_COLOR			0x0307
#define GL_SRC_ALPHA				0x0302
#define GL_ONE_MINUS_SRC_ALPHA			0x0303
#define GL_DST_ALPHA				0x0304
#define GL_ONE_MINUS_DST_ALPHA			0x0305
#define GL_SRC_ALPHA_SATURATE			0x0308
#define GL_CONSTANT_COLOR			0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR		0x8002
#define GL_CONSTANT_ALPHA			0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA		0x8004

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

typedef struct DPSOFTRAST_State_Draw_Span_s
{
	int start; // pixel index
	int length; // pixel count
	int startx; // usable range (according to pixelmask)
	int endx; // usable range (according to pixelmask)
	int mip; // which mipmap of the texture(s) to use on this
	unsigned char *pixelmask; // true for pixels that passed depth test, false for others
	// [0][n][] is start interpolant values (projected)
	// [1][n][] is end interpolant values (projected)
	// [0][DPSOFTRAST_ARRAY_TOTAL][] is start screencoord4f
	// [1][DPSOFTRAST_ARRAY_TOTAL][] is end screencoord4f
	// NOTE: screencoord4f[3] is W (basically 1/Z), useful for depthbuffer
	float data[2][DPSOFTRAST_ARRAY_TOTAL+1][4];
}
DPSOFTRAST_State_Draw_Span;

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

typedef struct DPSOFTRAST_State_s
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
	float uniform4f[DPSOFTRAST_UNIFORM_TOTAL*4];
	int uniform1i[DPSOFTRAST_UNIFORM_TOTAL];

	// derived values (DPSOFTRAST_VALIDATE_FB)
	int fb_clearscissor[4];
	int fb_viewport[4];
	int fb_viewportscissor[4];
	float fb_viewportcenter[2];
	float fb_viewportscale[2];

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
DPSOFTRAST_State;

DPSOFTRAST_State dpsoftrast;

#define DPSOFTRAST_DEPTHSCALE (1024.0f*1048576.0f)
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
	dpsoftrast.fb_viewportcenter[0] = dpsoftrast.user.viewport[0] + 0.5f * dpsoftrast.user.viewport[2] - 0.5f;
	dpsoftrast.fb_viewportcenter[1] = dpsoftrast.fb_height - dpsoftrast.user.viewport[1] - 0.5f * dpsoftrast.user.viewport[3] - 0.5f;
	dpsoftrast.fb_viewportscale[0] = 0.5f * dpsoftrast.user.viewport[2];
	dpsoftrast.fb_viewportscale[1] = -0.5f * dpsoftrast.user.viewport[3];
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
	texture->bytes = calloc(1, size);

	return texnum;
}
void DPSOFTRAST_Texture_Free(int index)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (texture->bytes)
		free(texture->bytes);
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
				o =  texture->bytes + texture->mipmap[i  ][0] + 4*(texture->mipmap[i  ][3] * texture->mipmap[i  ][2] * z      + texture->mipmap[i  ][2] * y   );
				i0 = texture->bytes + texture->mipmap[i-1][0] + 4*(texture->mipmap[i-1][3] * texture->mipmap[i-1][2] * layer0 + texture->mipmap[i-1][2] * row0);
				i1 = texture->bytes + texture->mipmap[i-1][0] + 4*(texture->mipmap[i-1][3] * texture->mipmap[i-1][2] * layer0 + texture->mipmap[i-1][2] * row1);
				i2 = texture->bytes + texture->mipmap[i-1][0] + 4*(texture->mipmap[i-1][3] * texture->mipmap[i-1][2] * layer1 + texture->mipmap[i-1][2] * row0);
				i3 = texture->bytes + texture->mipmap[i-1][0] + 4*(texture->mipmap[i-1][3] * texture->mipmap[i-1][2] * layer1 + texture->mipmap[i-1][2] * row1);
				w = texture->mipmap[i][2];
				if (layer1 > layer0)
				{
					if (texture->mipmap[i-1][2] > 1)
					{
						// average 3D texture
						for (x = 0;x < w;x++)
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
						for (x = 0;x < w;x++)
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
						for (x = 0;x < w;x++)
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
						for (x = 0;x < w;x++)
						{
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
	return texture->width;
}
int DPSOFTRAST_Texture_GetHeight(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->height;
}
int DPSOFTRAST_Texture_GetDepth(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->depth;
}
unsigned char *DPSOFTRAST_Texture_GetPixelPointer(int index, int mip)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return 0;
	return texture->bytes;
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
			free(dpsoftrast.draw.in_array4f[0]);
		data = calloc(1, dpsoftrast.draw.maxvertices * sizeof(float[4])*(DPSOFTRAST_ARRAY_TOTAL*2 + 1));
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
	// TODO: SIMD
	float matrix[4][4];
	int i;
	memcpy(matrix, inmatrix16f, sizeof(float[16]));
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

void DPSOFTRAST_Draw_ProjectVertices(float *out4f, const float *in4f, int numitems)
{
	// NOTE: this is used both as a whole mesh transform function and a
	// per-triangle transform function (for clipped triangles), accordingly
	// it should not crash on divide by 0 but the result of divide by 0 is
	// unimportant...
	// TODO: SIMD
	int i;
	float w;
	float viewportcenter[4];
	float viewportscale[4];
	viewportscale[0] = dpsoftrast.fb_viewportscale[0];
	viewportscale[1] = dpsoftrast.fb_viewportscale[1];
	viewportscale[2] = 0.5f;
	viewportscale[3] = 0.0f;
	viewportcenter[0] = dpsoftrast.fb_viewportcenter[0];
	viewportcenter[1] = dpsoftrast.fb_viewportcenter[1];
	viewportcenter[2] = 0.5f;
	viewportcenter[3] = 0.0f;
	for (i = 0;i < numitems;i++)
	{
		if (!in4f[3])
		{
			out4f[0] = 0.0f;
			out4f[1] = 0.0f;
			out4f[2] = 0.0f;
			out4f[3] = 0.0f;
			continue;
		}
		w = 1.0f / in4f[3];
		out4f[0] = viewportcenter[0] + viewportscale[0] * in4f[0] * w;
		out4f[1] = viewportcenter[1] + viewportscale[1] * in4f[1] * w;
		out4f[2] = viewportcenter[2] + viewportscale[2] * in4f[2] * w;
		out4f[3] = viewportcenter[3] + viewportscale[3] * in4f[3] * w;
		out4f[3] = w;
		in4f += 4;
		out4f += 4;
	}
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

void DPSOFTRAST_Draw_Span_Begin(const DPSOFTRAST_State_Draw_Span *span, float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float w = span->data[0][DPSOFTRAST_ARRAY_TOTAL][3];
	float wslope = span->data[1][DPSOFTRAST_ARRAY_TOTAL][3];
	// TODO: optimize by approximating every 8 pixels?
	for (x = startx;x < endx;x++)
		zf[x] = 1.0f / (w + wslope * x);
}

void DPSOFTRAST_Draw_Span_Finish(const DPSOFTRAST_State_Draw_Span *span, const float *in4f)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int d[4];
	float a, b;
	unsigned char *pixelmask = span->pixelmask;
	unsigned char *pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0];
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
		break;
	}
}

void DPSOFTRAST_Draw_Span_Texture2DVarying(const DPSOFTRAST_State_Draw_Span *span, float *out4f, int texunitindex, int arrayindex, const float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int flags;
	float c[4];
	float data[4];
	float slope[4];
	float z;
	float tc[2];
	float frac[2];
	float ifrac[2];
	float lerp[4];
	float tcscale[2];
	int tci[2];
	int tci1[2];
	int tcimin[2];
	int tcimax[2];
	int tciwrapmask[2];
	int tciwidth;
	int filter;
	int mip;
	unsigned char *pixelbase;
	unsigned char *pixel[4];
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
	// if this mipmap of the texture is 1 pixel, just fill it with that color
	mip = span->mip;
	if (mip >= texture->mipmaps)
		mip = texture->mipmaps - 1;
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
	pixelbase = (unsigned char *)texture->bytes + texture->mipmap[mip][0];
	tcscale[0] = texture->mipmap[mip][2];
	tcscale[1] = texture->mipmap[mip][3];
	tciwidth = texture->mipmap[mip][2];
	tcimin[0] = 0;
	tcimin[1] = 0;
	tcimax[0] = texture->mipmap[mip][2]-1;
	tcimax[1] = texture->mipmap[mip][3]-1;
	tciwrapmask[0] = texture->mipmap[mip][2]-1;
	tciwrapmask[1] = texture->mipmap[mip][3]-1;
	if (filter)
	{
		if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
		{
			for (x = startx;x < endx;x++)
			{
				z = zf[x];
				tc[0] = (data[0] + slope[0]*x) * z * tcscale[0] - 0.5f;
				tc[1] = (data[1] + slope[1]*x) * z * tcscale[1] - 0.5f;
				tci[0] = (int)floor(tc[0]);
				tci[1] = (int)floor(tc[1]);
				tci1[0] = tci[0] + 1;
				tci1[1] = tci[1] + 1;
				frac[0] = tc[0] - tci[0];ifrac[0] = 1.0f - frac[0];
				frac[1] = tc[1] - tci[1];ifrac[1] = 1.0f - frac[1];
				lerp[0] = ifrac[0]*ifrac[1];
				lerp[1] =  frac[0]*ifrac[1];
				lerp[2] = ifrac[0]* frac[1];
				lerp[3] =  frac[0]* frac[1];
				tci[0] = tci[0] >= tcimin[0] ? (tci[0] <= tcimax[0] ? tci[0] : tcimax[0]) : tcimin[0];
				tci[1] = tci[1] >= tcimin[1] ? (tci[1] <= tcimax[1] ? tci[1] : tcimax[1]) : tcimin[1];
				tci1[0] = tci1[0] >= tcimin[0] ? (tci1[0] <= tcimax[0] ? tci1[0] : tcimax[0]) : tcimin[0];
				tci1[1] = tci1[1] >= tcimin[1] ? (tci1[1] <= tcimax[1] ? tci1[1] : tcimax[1]) : tcimin[1];
				pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
				pixel[1] = pixelbase + 4 * (tci[1]*tciwidth+tci1[0]);
				pixel[2] = pixelbase + 4 * (tci1[1]*tciwidth+tci[0]);
				pixel[3] = pixelbase + 4 * (tci1[1]*tciwidth+tci1[0]);
				c[0] = (pixel[0][2]*lerp[0]+pixel[1][2]*lerp[1]+pixel[2][2]*lerp[2]+pixel[3][2]*lerp[3]) * (1.0f / 255.0f);
				c[1] = (pixel[0][1]*lerp[0]+pixel[1][1]*lerp[1]+pixel[2][1]*lerp[2]+pixel[3][1]*lerp[3]) * (1.0f / 255.0f);
				c[2] = (pixel[0][0]*lerp[0]+pixel[1][0]*lerp[1]+pixel[2][0]*lerp[2]+pixel[3][0]*lerp[3]) * (1.0f / 255.0f);
				c[3] = (pixel[0][3]*lerp[0]+pixel[1][3]*lerp[1]+pixel[2][3]*lerp[2]+pixel[3][3]*lerp[3]) * (1.0f / 255.0f);
				out4f[x*4+0] = c[0];
				out4f[x*4+1] = c[1];
				out4f[x*4+2] = c[2];
				out4f[x*4+3] = c[3];
			}
		}
		else
		{
			for (x = startx;x < endx;x++)
			{
				z = zf[x];
				tc[0] = (data[0] + slope[0]*x) * z * tcscale[0] - 0.5f;
				tc[1] = (data[1] + slope[1]*x) * z * tcscale[1] - 0.5f;
				tci[0] = (int)floor(tc[0]);
				tci[1] = (int)floor(tc[1]);
				tci1[0] = tci[0] + 1;
				tci1[1] = tci[1] + 1;
				frac[0] = tc[0] - tci[0];ifrac[0] = 1.0f - frac[0];
				frac[1] = tc[1] - tci[1];ifrac[1] = 1.0f - frac[1];
				lerp[0] = ifrac[0]*ifrac[1];
				lerp[1] =  frac[0]*ifrac[1];
				lerp[2] = ifrac[0]* frac[1];
				lerp[3] =  frac[0]* frac[1];
				tci[0] &= tciwrapmask[0];
				tci[1] &= tciwrapmask[1];
				tci1[0] &= tciwrapmask[0];
				tci1[1] &= tciwrapmask[1];
				pixel[0] = pixelbase + 4 * (tci[1]*tciwidth+tci[0]);
				pixel[1] = pixelbase + 4 * (tci[1]*tciwidth+tci1[0]);
				pixel[2] = pixelbase + 4 * (tci1[1]*tciwidth+tci[0]);
				pixel[3] = pixelbase + 4 * (tci1[1]*tciwidth+tci1[0]);
				c[0] = (pixel[0][2]*lerp[0]+pixel[1][2]*lerp[1]+pixel[2][2]*lerp[2]+pixel[3][2]*lerp[3]) * (1.0f / 255.0f);
				c[1] = (pixel[0][1]*lerp[0]+pixel[1][1]*lerp[1]+pixel[2][1]*lerp[2]+pixel[3][1]*lerp[3]) * (1.0f / 255.0f);
				c[2] = (pixel[0][0]*lerp[0]+pixel[1][0]*lerp[1]+pixel[2][0]*lerp[2]+pixel[3][0]*lerp[3]) * (1.0f / 255.0f);
				c[3] = (pixel[0][3]*lerp[0]+pixel[1][3]*lerp[1]+pixel[2][3]*lerp[2]+pixel[3][3]*lerp[3]) * (1.0f / 255.0f);
				out4f[x*4+0] = c[0];
				out4f[x*4+1] = c[1];
				out4f[x*4+2] = c[2];
				out4f[x*4+3] = c[3];
			}
		}
	}
	else
	{
		if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
		{
			for (x = startx;x < endx;x++)
			{
				z = zf[x];
				tc[0] = (data[0] + slope[0]*x) * z * tcscale[0] - 0.5f;
				tc[1] = (data[1] + slope[1]*x) * z * tcscale[1] - 0.5f;
				tci[0] = (int)floor(tc[0]);
				tci[1] = (int)floor(tc[1]);
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
			for (x = startx;x < endx;x++)
			{
				z = zf[x];
				tc[0] = (data[0] + slope[0]*x) * z * tcscale[0] - 0.5f;
				tc[1] = (data[1] + slope[1]*x) * z * tcscale[1] - 0.5f;
				tci[0] = (int)floor(tc[0]);
				tci[1] = (int)floor(tc[1]);
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

void DPSOFTRAST_Draw_Span_MultiplyVarying(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *in4f, int arrayindex, const float *zf)
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

void DPSOFTRAST_Draw_Span_AddBloom(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *ina4f, const float *inb4f, const float *subcolor)
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

void DPSOFTRAST_Draw_Span_MixUniformColor(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *in4f, const float *color)
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

void DPSOFTRAST_Draw_Span_Lightmap(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *diffuse, const float *lightmap)
{
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
	Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
	Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
	Color_Diffuse[3] = 0.0f;
	for (x = startx;x < endx;x++)
	{
		out4f[x*4+0] = diffuse[x*4+0] * (Color_Ambient[0] + lightmap[x*4+0] * Color_Diffuse[0]);
		out4f[x*4+1] = diffuse[x*4+1] * (Color_Ambient[1] + lightmap[x*4+1] * Color_Diffuse[1]);
		out4f[x*4+2] = diffuse[x*4+2] * (Color_Ambient[2] + lightmap[x*4+2] * Color_Diffuse[2]);
		out4f[x*4+3] = diffuse[x*4+3] * (Color_Ambient[3] + lightmap[x*4+3] * Color_Diffuse[3]);
	}
}

void DPSOFTRAST_Draw_Span_VertexColor(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *diffuse, const float *zf)
{
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4];
	float c[4];
	float data[4];
	float slope[4];
	float z;
	int arrayindex = DPSOFTRAST_ARRAY_COLOR;
	data[0] = span->data[0][arrayindex][0];
	data[1] = span->data[0][arrayindex][1];
	data[2] = span->data[0][arrayindex][2];
	data[3] = span->data[0][arrayindex][3];
	slope[0] = span->data[1][arrayindex][0];
	slope[1] = span->data[1][arrayindex][1];
	slope[2] = span->data[1][arrayindex][2];
	slope[3] = span->data[1][arrayindex][3];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Diffuse[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
	Color_Diffuse[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
	Color_Diffuse[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
	Color_Diffuse[3] = 0.0f;
	for (x = startx;x < endx;x++)
	{
		z = zf[x];
		c[0] = (data[0] + slope[0]*x) * z;
		c[1] = (data[1] + slope[1]*x) * z;
		c[2] = (data[2] + slope[2]*x) * z;
		c[3] = (data[3] + slope[3]*x) * z;
		out4f[x*4+0] = diffuse[x*4+0] * (Color_Ambient[0] + c[0] * Color_Diffuse[0]);
		out4f[x*4+1] = diffuse[x*4+1] * (Color_Ambient[1] + c[1] * Color_Diffuse[1]);
		out4f[x*4+2] = diffuse[x*4+2] * (Color_Ambient[2] + c[2] * Color_Diffuse[2]);
		out4f[x*4+3] = diffuse[x*4+3] * (Color_Ambient[3] + c[3] * Color_Diffuse[3]);
	}
}

void DPSOFTRAST_Draw_Span_FlatColor(const DPSOFTRAST_State_Draw_Span *span, float *out4f, const float *diffuse)
{
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4];
	Color_Ambient[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	for (x = startx;x < endx;x++)
	{
		out4f[x*4+0] = diffuse[x*4+0] * Color_Ambient[0];
		out4f[x*4+1] = diffuse[x*4+1] * Color_Ambient[1];
		out4f[x*4+2] = diffuse[x*4+2] * Color_Ambient[2];
		out4f[x*4+3] = diffuse[x*4+3] * Color_Ambient[3];
	}
}

void DPSOFTRAST_Draw_VertexShader(void)
{
	DPSOFTRAST_Array_Transform(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_POSITION], dpsoftrast.draw.numvertices, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	switch(dpsoftrast.shader_mode)
	{
	case SHADERMODE_GENERIC: ///< (particles/HUD/etc) vertex color: optionally multiplied by one texture
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.numvertices);
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
		break;
	case SHADERMODE_POSTPROCESS: ///< postprocessing shader (r_glsl_postprocess)
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD1], dpsoftrast.draw.numvertices);
		break;
	case SHADERMODE_DEPTH_OR_SHADOW: ///< (depthfirst/shadows) vertex shader only
		break;
	case SHADERMODE_FLATCOLOR: ///< (lightmap) modulate texture by uniform color (q1bsp: q3bsp)
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
		break;
	case SHADERMODE_VERTEXCOLOR: ///< (lightmap) modulate texture by vertex colors (q3bsp)
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_COLOR], dpsoftrast.draw.numvertices);
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
		break;
	case SHADERMODE_LIGHTMAP: ///< (lightmap) modulate texture by lightmap texture (q1bsp: q3bsp)
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD0], dpsoftrast.draw.numvertices);
		DPSOFTRAST_Array_Copy(dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.in_array4f[DPSOFTRAST_ARRAY_TEXCOORD4], dpsoftrast.draw.numvertices);
		break;
	case SHADERMODE_FAKELIGHT: ///< (fakelight) modulate texture by "fake" lighting (no lightmaps: no nothing)
		break;
	case SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE: ///< (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
		break;
	case SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE: ///< (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
		break;
	case SHADERMODE_LIGHTDIRECTION: ///< (lightmap) use directional pixel shading from fixed light direction (q3bsp)
		break;
	case SHADERMODE_LIGHTSOURCE: ///< (lightsource) use directional pixel shading from light source (rtlight)
		break;
	case SHADERMODE_REFRACTION: ///< refract background (the material is rendered normally after this pass)
		break;
	case SHADERMODE_WATER: ///< refract background and reflection (the material is rendered normally after this pass)
		break;
	case SHADERMODE_SHOWDEPTH: ///< (debugging) renders depth as color
		break;
	case SHADERMODE_DEFERREDGEOMETRY: ///< (deferred) render material properties to screenspace geometry buffers
		break;
	case SHADERMODE_DEFERREDLIGHTSOURCE: ///< (deferred) use directional pixel shading from light source (rtlight) on screenspace geometry buffers
		break;
	case SHADERMODE_COUNT:
		break;
	}
}

void DPSOFTRAST_Draw_PixelShaderSpan(const DPSOFTRAST_State_Draw_Span *span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	float buffer_texture_color[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	float buffer_texture_lightmap[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	float buffer_FragColor[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	switch(dpsoftrast.shader_mode)
	{
	case SHADERMODE_GENERIC: ///< (particles/HUD/etc) vertex color: optionally multiplied by one texture
		DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_color, GL20TU_FIRST, 2, buffer_z);
		DPSOFTRAST_Draw_Span_MultiplyVarying(span, buffer_FragColor, buffer_texture_color, 1, buffer_z);
		DPSOFTRAST_Draw_Span_Finish(span, buffer_FragColor);
		break;
	case SHADERMODE_POSTPROCESS: ///< postprocessing shader (r_glsl_postprocess)
		// TODO: optimize!!  at the very least there is no reason to use texture sampling on the frame texture
		DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_FragColor, GL20TU_FIRST, 2, buffer_z);
		if (dpsoftrast.shader_permutation & SHADERPERMUTATION_BLOOM)
		{
			DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_color, GL20TU_SECOND, 3, buffer_z);
			DPSOFTRAST_Draw_Span_AddBloom(span, buffer_FragColor, buffer_FragColor, buffer_texture_color, dpsoftrast.uniform4f + DPSOFTRAST_UNIFORM_BloomColorSubtract * 4);
		}
		DPSOFTRAST_Draw_Span_MixUniformColor(span, buffer_FragColor, buffer_FragColor, dpsoftrast.uniform4f + DPSOFTRAST_UNIFORM_ViewTintColor * 4);
		if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SATURATION)
		{
			// TODO: implement saturation
		}
		if (dpsoftrast.shader_permutation & SHADERPERMUTATION_GAMMARAMPS)
		{
			// TODO: implement gammaramps
		}
		DPSOFTRAST_Draw_Span_Finish(span, buffer_FragColor);
		break;
	case SHADERMODE_DEPTH_OR_SHADOW: ///< (depthfirst/shadows) vertex shader only
		break;
	case SHADERMODE_FLATCOLOR: ///< (lightmap) modulate texture by uniform color (q1bsp: q3bsp)
		DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_color, GL20TU_COLOR, 2, buffer_z);
		DPSOFTRAST_Draw_Span_FlatColor(span, buffer_FragColor, buffer_texture_color);
		DPSOFTRAST_Draw_Span_Finish(span, buffer_FragColor);
		break;
	case SHADERMODE_VERTEXCOLOR: ///< (lightmap) modulate texture by vertex colors (q3bsp)
		DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_color, GL20TU_COLOR, 2, buffer_z);
		DPSOFTRAST_Draw_Span_VertexColor(span, buffer_FragColor, buffer_texture_color, buffer_z);
		DPSOFTRAST_Draw_Span_Finish(span, buffer_FragColor);
		break;
	case SHADERMODE_LIGHTMAP: ///< (lightmap) modulate texture by lightmap texture (q1bsp: q3bsp)
		DPSOFTRAST_Draw_Span_Begin(span, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_color, GL20TU_COLOR, 2, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVarying(span, buffer_texture_lightmap, GL20TU_LIGHTMAP, 6, buffer_z);
		DPSOFTRAST_Draw_Span_Lightmap(span, buffer_FragColor, buffer_texture_color, buffer_texture_lightmap);
		DPSOFTRAST_Draw_Span_Finish(span, buffer_FragColor);
		break;
	case SHADERMODE_FAKELIGHT: ///< (fakelight) modulate texture by "fake" lighting (no lightmaps: no nothing)
		break;
	case SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE: ///< (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
		break;
	case SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE: ///< (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
		break;
	case SHADERMODE_LIGHTDIRECTION: ///< (lightmap) use directional pixel shading from fixed light direction (q3bsp)
		break;
	case SHADERMODE_LIGHTSOURCE: ///< (lightsource) use directional pixel shading from light source (rtlight)
		break;
	case SHADERMODE_REFRACTION: ///< refract background (the material is rendered normally after this pass)
		break;
	case SHADERMODE_WATER: ///< refract background and reflection (the material is rendered normally after this pass)
		break;
	case SHADERMODE_SHOWDEPTH: ///< (debugging) renders depth as color
		break;
	case SHADERMODE_DEFERREDGEOMETRY: ///< (deferred) render material properties to screenspace geometry buffers
		break;
	case SHADERMODE_DEFERREDLIGHTSOURCE: ///< (deferred) use directional pixel shading from light source (rtlight) on screenspace geometry buffers
		break;
	case SHADERMODE_COUNT:
		break;
	}
}

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
			depth = (int)(w*DPSOFTRAST_DEPTHSCALE);
			depthslope = (int)(wslope*DPSOFTRAST_DEPTHSCALE);
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
				DPSOFTRAST_Draw_PixelShaderSpan(span);
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
				DPSOFTRAST_Draw_PixelShaderSpan(span);
			}
		}
	}
}

void DPSOFTRAST_Draw_ProcessTriangles(int firstvertex, int numvertices, int numtriangles, const int *element3i, const unsigned short *element3s, unsigned char *arraymask)
{
	int cullface = dpsoftrast.user.cullface;
	int width = dpsoftrast.fb_width;
	int height = dpsoftrast.fb_height;
	int i;
	int j;
	int k;
	int y;
	int mip;
	int e[3];
	int screenx[4];
	int screeny[4];
	int screenyless[4];
	int numpoints;
	int clipflags;
	int edge0p;
	int edge0n;
	int edge1p;
	int edge1n;
	int extent[4];
	int startx;
	int endx;
	float startxf;
	float endxf;
	float edge0ylerp;
	float edge0yilerp;
	float edge1ylerp;
	float edge1yilerp;
	float edge0xf;
	float edge1xf;
	float spanilength;
	float startxlerp;
	float yc;
	float w;
	float frac;
	float ifrac;
	float trianglearea2;
	float triangleedge[2][4];
	float trianglenormal[4];
	float clipdist[4];
	float clipped[DPSOFTRAST_ARRAY_TOTAL][4][4];
	float screen[4][4];
	float proj[DPSOFTRAST_ARRAY_TOTAL][4][4];
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
		triangleedge[0][0] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4+0] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+0];
		triangleedge[0][1] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4+1] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+1];
		triangleedge[0][2] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4+2] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+2];
		triangleedge[1][0] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4+0] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+0];
		triangleedge[1][1] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4+1] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+1];
		triangleedge[1][2] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4+2] - dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+2];
		trianglenormal[0] = triangleedge[0][1] * triangleedge[1][2] - triangleedge[0][2] * triangleedge[1][1];
		trianglenormal[1] = triangleedge[0][2] * triangleedge[1][0] - triangleedge[0][0] * triangleedge[1][2];
		trianglenormal[2] = triangleedge[0][0] * triangleedge[1][1] - triangleedge[0][1] * triangleedge[1][0];
		trianglearea2 = trianglenormal[0] * trianglenormal[0] + trianglenormal[1] * trianglenormal[1] + trianglenormal[2] * trianglenormal[2];
		// skip degenerate triangles, nothing good can come from them...
		if (trianglearea2 == 0.0f)
			continue;
		// apply current cullface mode (this culls many triangles)
		switch(cullface)
		{
		case GL_BACK:
			if (trianglenormal[2] < 0)
				continue;
			break;
		case GL_FRONT:
			if (trianglenormal[2] > 0)
				continue;
			break;
		}
		// calculate distance from nearplane
		clipdist[0] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[0]*4+2] + 1.0f;
		clipdist[1] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[1]*4+2] + 1.0f;
		clipdist[2] = dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION][e[2]*4+2] + 1.0f;
		clipflags = 0;
		if (clipdist[0] < 0.0f)
			clipflags |= 1;
		if (clipdist[1] < 0.0f)
			clipflags |= 2;
		if (clipdist[2] < 0.0f)
			clipflags |= 4;
		// clip triangle if necessary
		switch(clipflags)
		{
		case 0: /*000*/
			// triangle is entirely in front of nearplane

			// macros for clipping vertices
#define CLIPPEDVERTEXLERP(k,p1,p2) \
			frac = clipdist[p1] / (clipdist[p1] - clipdist[p2]);\
			ifrac = 1.0f - frac;\
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)\
			{\
				if (arraymask[j])\
				{\
					clipped[j][k][0] = dpsoftrast.draw.post_array4f[j][e[p1]*4+0]*ifrac+dpsoftrast.draw.post_array4f[j][e[p2]*4+0]*frac;\
					clipped[j][k][1] = dpsoftrast.draw.post_array4f[j][e[p1]*4+1]*ifrac+dpsoftrast.draw.post_array4f[j][e[p2]*4+1]*frac;\
					clipped[j][k][2] = dpsoftrast.draw.post_array4f[j][e[p1]*4+2]*ifrac+dpsoftrast.draw.post_array4f[j][e[p2]*4+2]*frac;\
					clipped[j][k][3] = dpsoftrast.draw.post_array4f[j][e[p1]*4+3]*ifrac+dpsoftrast.draw.post_array4f[j][e[p2]*4+3]*frac;\
				}\
			}\
			DPSOFTRAST_Draw_ProjectVertices(screen[k], clipped[DPSOFTRAST_ARRAY_POSITION][k], 1)
#define CLIPPEDVERTEXCOPY(k,p1) \
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)\
			{\
				if (arraymask[j])\
				{\
					clipped[j][k][0] = dpsoftrast.draw.post_array4f[j][e[p1]*4+0];\
					clipped[j][k][1] = dpsoftrast.draw.post_array4f[j][e[p1]*4+1];\
					clipped[j][k][2] = dpsoftrast.draw.post_array4f[j][e[p1]*4+2];\
					clipped[j][k][3] = dpsoftrast.draw.post_array4f[j][e[p1]*4+3];\
				}\
			}\
			screen[k][0] = dpsoftrast.draw.screencoord4f[e[p1]*4+0];\
			screen[k][1] = dpsoftrast.draw.screencoord4f[e[p1]*4+1];\
			screen[k][2] = dpsoftrast.draw.screencoord4f[e[p1]*4+2];\
			screen[k][3] = dpsoftrast.draw.screencoord4f[e[p1]*4+3];

			CLIPPEDVERTEXCOPY(0,0);
			CLIPPEDVERTEXCOPY(1,1);
			CLIPPEDVERTEXCOPY(2,2);
			numpoints = 3;
			break;
		case 1: /*100*/
			CLIPPEDVERTEXLERP(0,0,1);
			CLIPPEDVERTEXCOPY(1,1);
			CLIPPEDVERTEXCOPY(2,2);
			CLIPPEDVERTEXLERP(3,2,0);
			numpoints = 4;
			break;
		case 2: /*010*/
			CLIPPEDVERTEXCOPY(0,0);
			CLIPPEDVERTEXLERP(1,0,1);
			CLIPPEDVERTEXLERP(2,1,2);
			CLIPPEDVERTEXCOPY(3,2);
			numpoints = 4;
			break;
		case 3: /*110*/
			CLIPPEDVERTEXLERP(0,1,2);
			CLIPPEDVERTEXCOPY(1,2);
			CLIPPEDVERTEXLERP(2,2,0);
			numpoints = 3;
			break;
		case 4: /*001*/
			CLIPPEDVERTEXCOPY(0,0);
			CLIPPEDVERTEXCOPY(1,1);
			CLIPPEDVERTEXLERP(2,1,2);
			CLIPPEDVERTEXLERP(3,2,0);
			numpoints = 4;
			break;
		case 5: /*101*/
			CLIPPEDVERTEXLERP(0,0,1);
			CLIPPEDVERTEXCOPY(1,1);
			CLIPPEDVERTEXLERP(2,1,2);
			numpoints = 3;
			break;
		case 6: /*011*/
			CLIPPEDVERTEXCOPY(0,0);
			CLIPPEDVERTEXLERP(1,0,1);
			CLIPPEDVERTEXLERP(2,2,0);
			numpoints = 3;
			break;
		case 7: /*111*/
			// triangle is entirely behind nearplane
			continue;
		}
		// calculate integer y coords for triangle points
		screenx[0] = (int)(screen[0][0]);
		screeny[0] = (int)(screen[0][1]);
		screenx[1] = (int)(screen[1][0]);
		screeny[1] = (int)(screen[1][1]);
		screenx[2] = (int)(screen[2][0]);
		screeny[2] = (int)(screen[2][1]);
		screenx[3] = (int)(screen[3][0]);
		screeny[3] = (int)(screen[3][1]);
		// figure out the extents (bounding box) of the triangle
		extent[0] = screenx[0];
		extent[1] = screeny[0];
		extent[2] = screenx[0];
		extent[3] = screeny[0];
		for (j = 1;j < numpoints;j++)
		{
			if (extent[0] > screenx[j]) extent[0] = screenx[j];
			if (extent[1] > screeny[j]) extent[1] = screeny[j];
			if (extent[2] < screenx[j]) extent[2] = screenx[j];
			if (extent[3] < screeny[j]) extent[3] = screeny[j];
		}
		//extent[0]--;
		//extent[1]--;
		extent[2]++;
		extent[3]++;
		if (extent[0] < 0)
			extent[0] = 0;
		if (extent[1] < 0)
			extent[1] = 0;
		if (extent[2] > width)
			extent[2] = width;
		if (extent[3] > height)
			extent[3] = height;
		// skip offscreen triangles
		if (extent[2] <= extent[0] || extent[3] <= extent[1])
			continue;
		// TODO: adjust texture LOD by texture density
		mip = 0;
		// okay, this triangle is going to produce spans, we'd better project
		// the interpolants now (this is what gives perspective texturing),
		// this consists of simply multiplying all arrays by the W coord
		// (which is basically 1/Z), which will be undone per-pixel
		// (multiplying by Z again) to get the perspective-correct array
		// values
		for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
		{
			if (arraymask[j])
			{
				for (k = 0;k < numpoints;k++)
				{
					w = screen[k][3];
					proj[j][k][0] = clipped[j][k][0] * w;
					proj[j][k][1] = clipped[j][k][1] * w;
					proj[j][k][2] = clipped[j][k][2] * w;
					proj[j][k][3] = clipped[j][k][3] * w;
				}
			}
		}
		// iterate potential spans
		// TODO: optimize?  if we figured out the edge order beforehand, this
		//       could do loops over the edges in the proper order rather than
		//       selecting them for each span
		// TODO: optimize?  the edges could have data slopes calculated
		// TODO: optimize?  the data slopes could be calculated as a plane
		//       (2D slopes) to avoid any interpolation along edges at all
		for (y = extent[1];y < extent[3];y++)
		{
			// get center of pixel y
			yc = y;
			// do the compares all at once
			screenyless[0] = y <= screeny[0];
			screenyless[1] = y <= screeny[1];
			screenyless[2] = y <= screeny[2];
			screenyless[3] = y <= screeny[3];
			if (numpoints == 4)
			{
				switch(screenyless[0] + screenyless[1] * 2 + screenyless[2] * 4 + screenyless[3] * 8)
				{
				case  0: /*0000*/ continue;
				case  1: /*1000*/ edge0p = 3;edge0n = 0;edge1p = 0;edge1n = 1;break;
				case  2: /*0100*/ edge0p = 0;edge0n = 1;edge1p = 1;edge1n = 2;break;
				case  3: /*1100*/ edge0p = 3;edge0n = 0;edge1p = 1;edge1n = 2;break;
				case  4: /*0010*/ edge0p = 1;edge0n = 2;edge1p = 2;edge1n = 3;break;
				case  5: /*1010*/ edge0p = 1;edge0n = 2;edge1p = 2;edge1n = 3;break; // concave - nonsense
				case  6: /*0110*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 3;break;
				case  7: /*1110*/ edge0p = 3;edge0n = 0;edge1p = 2;edge1n = 3;break;
				case  8: /*0001*/ edge0p = 2;edge0n = 3;edge1p = 3;edge1n = 0;break;
				case  9: /*1001*/ edge0p = 2;edge0n = 3;edge1p = 0;edge1n = 1;break;
				case 10: /*0101*/ edge0p = 2;edge0n = 3;edge1p = 1;edge1n = 2;break; // concave - nonsense
				case 11: /*1101*/ edge0p = 2;edge0n = 3;edge1p = 1;edge1n = 2;break;
				case 12: /*0011*/ edge0p = 1;edge0n = 2;edge1p = 3;edge1n = 0;break;
				case 13: /*1011*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 1;break;
				case 14: /*0111*/ edge0p = 0;edge0n = 1;edge1p = 3;edge1n = 0;break;
				case 15: /*1111*/ continue;
				}
			}
			else
			{
				switch(screenyless[0] + screenyless[1] * 2 + screenyless[2] * 4)
				{
				case 0: /*000*/ continue;
				case 1: /*100*/ edge0p = 2;edge0n = 0;edge1p = 0;edge1n = 1;break;
				case 2: /*010*/ edge0p = 0;edge0n = 1;edge1p = 1;edge1n = 2;break;
				case 3: /*110*/ edge0p = 2;edge0n = 0;edge1p = 1;edge1n = 2;break;
				case 4: /*001*/ edge0p = 1;edge0n = 2;edge1p = 2;edge1n = 0;break;
				case 5: /*101*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 1;break;
				case 6: /*011*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 0;break;
				case 7: /*111*/ continue;
				}
			}
#if 0
		{
			int foundedges = 0;
			int cedge0p = 0;
			int cedge0n = 0;
			int cedge1p = 0;
			int cedge1n = 0;
			for (j = 0, k = numpoints-1;j < numpoints;k = j, j++)
			{
				if (screenyless[k] && !screenyless[j])
				{
					cedge1p = k;
					cedge1n = j;
					foundedges |= 1;
				}
				else if (screenyless[j] && !screenyless[k])
				{
					cedge0p = k;
					cedge0n = j;
					foundedges |= 2;
				}
			}
			if (foundedges != 3)
				continue;
			if (cedge0p != edge0p || cedge0n != edge0n || cedge1p != edge1p || cedge1n != edge1n)
			{
				if (numpoints == 4)
					printf("case %i%i%i%i is broken %i %i %i %i != %i %i %i %i\n", screenyless[0], screenyless[1], screenyless[2], screenyless[3], cedge0p, cedge0n, cedge1p, cedge1n, edge0p, edge0n, edge1p, edge1n);
				else
					printf("case %i%i%i is broken %i %i %i %i != %i %i %i %i\n", screenyless[0], screenyless[1], screenyless[2], cedge0p, cedge0n, cedge1p, cedge1n, edge0p, edge0n, edge1p, edge1n);
			}
		}
#endif
			edge0ylerp = (yc - screen[edge0p][1]) / (screen[edge0n][1] - screen[edge0p][1]);
			edge1ylerp = (yc - screen[edge1p][1]) / (screen[edge1n][1] - screen[edge1p][1]);
			if (edge0ylerp < 0 || edge0ylerp > 1 || edge1ylerp < 0 || edge1ylerp > 1)
				continue;
			edge0yilerp = 1.0f - edge0ylerp;
			edge1yilerp = 1.0f - edge1ylerp;
			edge0xf = screen[edge0p][0] * edge0yilerp + screen[edge0n][0] * edge0ylerp;
			edge1xf = screen[edge1p][0] * edge1yilerp + screen[edge1n][0] * edge1ylerp;
			if (edge0xf < edge1xf)
			{
				startxf = edge0xf;
				endxf = edge1xf;
			}
			else
			{
				startxf = edge1xf;
				endxf = edge0xf;
			}
			startx = (int)ceil(startxf);
			endx = (int)ceil(endxf);
			if (startx < 0)
				startx = 0;
			if (endx > width)
				endx = width;
			if (startx >= endx)
				continue;
			if (startxf > startx || endxf < endx-1) { printf("%s:%i X wrong (%i to %i is outside %f to %f)\n", __FILE__, __LINE__, startx, endx, startxf, endxf); }
			spanilength = 1.0f / (endxf - startxf);
			startxlerp = startx - startxf;
			span = &dpsoftrast.draw.spanqueue[dpsoftrast.draw.numspans++];
			span->mip = mip;
			span->start = y * width + startx;
			span->length = endx - startx;
			j = DPSOFTRAST_ARRAY_TOTAL;
			if (edge0xf < edge1xf)
			{
				span->data[0][j][0] = screen[edge0p][0] * edge0yilerp + screen[edge0n][0] * edge0ylerp;
				span->data[0][j][1] = screen[edge0p][1] * edge0yilerp + screen[edge0n][1] * edge0ylerp;
				span->data[0][j][2] = screen[edge0p][2] * edge0yilerp + screen[edge0n][2] * edge0ylerp;
				span->data[0][j][3] = screen[edge0p][3] * edge0yilerp + screen[edge0n][3] * edge0ylerp;
				span->data[1][j][0] = screen[edge1p][0] * edge1yilerp + screen[edge1n][0] * edge1ylerp;
				span->data[1][j][1] = screen[edge1p][1] * edge1yilerp + screen[edge1n][1] * edge1ylerp;
				span->data[1][j][2] = screen[edge1p][2] * edge1yilerp + screen[edge1n][2] * edge1ylerp;
				span->data[1][j][3] = screen[edge1p][3] * edge1yilerp + screen[edge1n][3] * edge1ylerp;
				for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
				{
					if (arraymask[j])
					{
						span->data[0][j][0] = proj[j][edge0p][0] * edge0yilerp + proj[j][edge0n][0] * edge0ylerp;
						span->data[0][j][1] = proj[j][edge0p][1] * edge0yilerp + proj[j][edge0n][1] * edge0ylerp;
						span->data[0][j][2] = proj[j][edge0p][2] * edge0yilerp + proj[j][edge0n][2] * edge0ylerp;
						span->data[0][j][3] = proj[j][edge0p][3] * edge0yilerp + proj[j][edge0n][3] * edge0ylerp;
						span->data[1][j][0] = proj[j][edge1p][0] * edge1yilerp + proj[j][edge1n][0] * edge1ylerp;
						span->data[1][j][1] = proj[j][edge1p][1] * edge1yilerp + proj[j][edge1n][1] * edge1ylerp;
						span->data[1][j][2] = proj[j][edge1p][2] * edge1yilerp + proj[j][edge1n][2] * edge1ylerp;
						span->data[1][j][3] = proj[j][edge1p][3] * edge1yilerp + proj[j][edge1n][3] * edge1ylerp;
					}
				}
			}
			else
			{
				span->data[0][j][0] = screen[edge1p][0] * edge1yilerp + screen[edge1n][0] * edge1ylerp;
				span->data[0][j][1] = screen[edge1p][1] * edge1yilerp + screen[edge1n][1] * edge1ylerp;
				span->data[0][j][2] = screen[edge1p][2] * edge1yilerp + screen[edge1n][2] * edge1ylerp;
				span->data[0][j][3] = screen[edge1p][3] * edge1yilerp + screen[edge1n][3] * edge1ylerp;
				span->data[1][j][0] = screen[edge0p][0] * edge0yilerp + screen[edge0n][0] * edge0ylerp;
				span->data[1][j][1] = screen[edge0p][1] * edge0yilerp + screen[edge0n][1] * edge0ylerp;
				span->data[1][j][2] = screen[edge0p][2] * edge0yilerp + screen[edge0n][2] * edge0ylerp;
				span->data[1][j][3] = screen[edge0p][3] * edge0yilerp + screen[edge0n][3] * edge0ylerp;
				for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
				{
					if (arraymask[j])
					{
						span->data[0][j][0] = proj[j][edge1p][0] * edge1yilerp + proj[j][edge1n][0] * edge1ylerp;
						span->data[0][j][1] = proj[j][edge1p][1] * edge1yilerp + proj[j][edge1n][1] * edge1ylerp;
						span->data[0][j][2] = proj[j][edge1p][2] * edge1yilerp + proj[j][edge1n][2] * edge1ylerp;
						span->data[0][j][3] = proj[j][edge1p][3] * edge1yilerp + proj[j][edge1n][3] * edge1ylerp;
						span->data[1][j][0] = proj[j][edge0p][0] * edge0yilerp + proj[j][edge0n][0] * edge0ylerp;
						span->data[1][j][1] = proj[j][edge0p][1] * edge0yilerp + proj[j][edge0n][1] * edge0ylerp;
						span->data[1][j][2] = proj[j][edge0p][2] * edge0yilerp + proj[j][edge0n][2] * edge0ylerp;
						span->data[1][j][3] = proj[j][edge0p][3] * edge0yilerp + proj[j][edge0n][3] * edge0ylerp;
					}
				}
			}
			// change data[1][n][] to be a data slope
			j = DPSOFTRAST_ARRAY_TOTAL;
			span->data[1][j][0] = (span->data[1][j][0] - span->data[0][j][0]) * spanilength;
			span->data[1][j][1] = (span->data[1][j][1] - span->data[0][j][1]) * spanilength;
			span->data[1][j][2] = (span->data[1][j][2] - span->data[0][j][2]) * spanilength;
			span->data[1][j][3] = (span->data[1][j][3] - span->data[0][j][3]) * spanilength;
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
			{
				if (arraymask[j])
				{
					span->data[1][j][0] = (span->data[1][j][0] - span->data[0][j][0]) * spanilength;
					span->data[1][j][1] = (span->data[1][j][1] - span->data[0][j][1]) * spanilength;
					span->data[1][j][2] = (span->data[1][j][2] - span->data[0][j][2]) * spanilength;
					span->data[1][j][3] = (span->data[1][j][3] - span->data[0][j][3]) * spanilength;
				}
			}
			// adjust the data[0][n][] to be correct for the pixel centers
			// this also handles horizontal clipping where a major part of the
			// span may be off the left side of the screen
			j = DPSOFTRAST_ARRAY_TOTAL;
			span->data[0][j][0] += span->data[1][j][0] * startxlerp;
			span->data[0][j][1] += span->data[1][j][1] * startxlerp;
			span->data[0][j][2] += span->data[1][j][2] * startxlerp;
			span->data[0][j][3] += span->data[1][j][3] * startxlerp;
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
			{
				if (arraymask[j])
				{
					span->data[0][j][0] += span->data[1][j][0] * startxlerp;
					span->data[0][j][1] += span->data[1][j][1] * startxlerp;
					span->data[0][j][2] += span->data[1][j][2] * startxlerp;
					span->data[0][j][3] += span->data[1][j][3] * startxlerp;
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
				span->data[0][j][0] += span->data[1][j][0] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
				span->data[0][j][1] += span->data[1][j][1] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
				span->data[0][j][2] += span->data[1][j][2] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
				span->data[0][j][3] += span->data[1][j][3] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
				for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL;j++)
				{
					if (arraymask[j])
					{
						span->data[0][j][0] += span->data[1][j][0] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
						span->data[0][j][1] += span->data[1][j][1] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
						span->data[0][j][2] += span->data[1][j][2] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
						span->data[0][j][3] += span->data[1][j][3] * DPSOFTRAST_DRAW_MAXSPANLENGTH;
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
		// draw outlines over triangle for debugging
	//	for (j = 0, k = numpoints-1;j < numpoints;k = j, j++)
	//		DPSOFTRAST_Draw_DebugEdgePoints(screen[k], screen[j]);
	}
	if (dpsoftrast.draw.numspans)
	{
		DPSOFTRAST_Draw_ProcessSpans();
		dpsoftrast.draw.numspans = 0;
	}
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
	DPSOFTRAST_Draw_VertexShader();
	DPSOFTRAST_Draw_ProjectVertices(dpsoftrast.draw.screencoord4f, dpsoftrast.draw.post_array4f[DPSOFTRAST_ARRAY_POSITION], numvertices);
	DPSOFTRAST_Draw_ProcessTriangles(firstvertex, numvertices, numtriangles, element3i, element3s, arraymask);
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
			free(dpsoftrast.texture[i].bytes);
	if (dpsoftrast.texture)
		free(dpsoftrast.texture);
	memset(&dpsoftrast, 0, sizeof(dpsoftrast));
}
