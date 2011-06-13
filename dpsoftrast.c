#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "quakedef.h"
#include "thread.h"
#include "dpsoftrast.h"

#ifdef _MSC_VER
#pragma warning(disable : 4324)
#endif

#ifndef __cplusplus
typedef qboolean bool;
#endif

#define ALIGN_SIZE 16
#define ATOMIC_SIZE 32

#ifdef SSE_POSSIBLE
	#if defined(__APPLE__)
		#include <libkern/OSAtomic.h>
		#define ALIGN(var) var __attribute__((__aligned__(16)))
		#define ATOMIC(var) var __attribute__((__aligned__(32)))
		#define MEMORY_BARRIER (_mm_sfence())
		#define ATOMIC_COUNTER volatile int32_t 
		#define ATOMIC_INCREMENT(counter) (OSAtomicIncrement32Barrier(&(counter)))
		#define ATOMIC_DECREMENT(counter) (OSAtomicDecrement32Barrier(&(counter)))
		#define ATOMIC_ADD(counter, val) ((void)OSAtomicAdd32Barrier((val), &(counter)))
	#elif defined(__GNUC__) && defined(WIN32)
		#define ALIGN(var) var __attribute__((__aligned__(16)))
		#define ATOMIC(var) var __attribute__((__aligned__(32)))
		#define MEMORY_BARRIER (_mm_sfence())
		//(__sync_synchronize())
		#define ATOMIC_COUNTER volatile LONG
		// this LONG * cast serves to fix an issue with broken mingw
		// packages on Ubuntu; these only declare the function to take
		// a LONG *, causing a compile error here. This seems to be
		// error- and warn-free on platforms that DO declare
		// InterlockedIncrement correctly, like mingw on Windows.
		#define ATOMIC_INCREMENT(counter) (InterlockedIncrement((LONG *) &(counter)))
		#define ATOMIC_DECREMENT(counter) (InterlockedDecrement((LONG *) &(counter)))
		#define ATOMIC_ADD(counter, val) ((void)InterlockedExchangeAdd((LONG *) &(counter), (val)))
	#elif defined(__GNUC__)
		#define ALIGN(var) var __attribute__((__aligned__(16)))
		#define ATOMIC(var) var __attribute__((__aligned__(32)))
		#define MEMORY_BARRIER (_mm_sfence())
		//(__sync_synchronize())
		#define ATOMIC_COUNTER volatile int
		#define ATOMIC_INCREMENT(counter) (__sync_add_and_fetch(&(counter), 1))
		#define ATOMIC_DECREMENT(counter) (__sync_add_and_fetch(&(counter), -1))
		#define ATOMIC_ADD(counter, val) ((void)__sync_fetch_and_add(&(counter), (val)))
	#elif defined(_MSC_VER)
		#define ALIGN(var) __declspec(align(16)) var
		#define ATOMIC(var) __declspec(align(32)) var
		#define MEMORY_BARRIER (_mm_sfence())
		//(MemoryBarrier())
		#define ATOMIC_COUNTER volatile LONG
		#define ATOMIC_INCREMENT(counter) (InterlockedIncrement(&(counter)))
		#define ATOMIC_DECREMENT(counter) (InterlockedDecrement(&(counter)))
		#define ATOMIC_ADD(counter, val) ((void)InterlockedExchangeAdd(&(counter), (val)))
	#endif
#endif

#ifndef ALIGN
#define ALIGN(var) var
#endif
#ifndef ATOMIC
#define ATOMIC(var) var
#endif
#ifndef MEMORY_BARRIER
#define MEMORY_BARRIER ((void)0)
#endif
#ifndef ATOMIC_COUNTER
#define ATOMIC_COUNTER int
#endif
#ifndef ATOMIC_INCREMENT
#define ATOMIC_INCREMENT(counter) (++(counter))
#endif
#ifndef ATOMIC_DECREMENT
#define ATOMIC_DECREMENT(counter) (--(counter))
#endif
#ifndef ATOMIC_ADD
#define ATOMIC_ADD(counter, val) ((void)((counter) += (val)))
#endif

#ifdef SSE_POSSIBLE
#include <emmintrin.h>

#if defined(__GNUC__) && (__GNUC < 4 || __GNUC_MINOR__ < 6) && !defined(__clang__)
	#define _mm_cvtss_f32(val) (__builtin_ia32_vec_ext_v4sf ((__v4sf)(val), 0))
#endif

#define MM_MALLOC(size) _mm_malloc(size, ATOMIC_SIZE)

static void *MM_CALLOC(size_t nmemb, size_t size)
{
	void *ptr = _mm_malloc(nmemb*size, ATOMIC_SIZE);
	if (ptr != NULL) memset(ptr, 0, nmemb*size);
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
	ATOMIC_COUNTER binds;
	unsigned char *bytes;
	int mipmap[DPSOFTRAST_MAXMIPMAPS][5];
}
DPSOFTRAST_Texture;

#define COMMAND_SIZE ALIGN_SIZE
#define COMMAND_ALIGN(var) ALIGN(var)

typedef COMMAND_ALIGN(struct DPSOFTRAST_Command_s
{
	unsigned char opcode;
	unsigned short commandsize;
}
DPSOFTRAST_Command);

enum { DPSOFTRAST_OPCODE_Reset = 0 };

#define DEFCOMMAND(opcodeval, name, fields) \
	enum { DPSOFTRAST_OPCODE_##name = opcodeval }; \
	typedef COMMAND_ALIGN(struct DPSOFTRAST_Command_##name##_s \
	{ \
		unsigned char opcode; \
		unsigned short commandsize; \
		fields \
	} DPSOFTRAST_Command_##name );

#define DPSOFTRAST_DRAW_MAXCOMMANDPOOL 2097152
#define DPSOFTRAST_DRAW_MAXCOMMANDSIZE 16384

typedef ATOMIC(struct DPSOFTRAST_State_Command_Pool_s
{
	int freecommand;
	int usedcommands;
	ATOMIC(unsigned char commands[DPSOFTRAST_DRAW_MAXCOMMANDPOOL]);
}
DPSOFTRAST_State_Command_Pool);

typedef ATOMIC(struct DPSOFTRAST_State_Triangle_s
{
	unsigned char mip[DPSOFTRAST_MAXTEXTUREUNITS]; // texcoord to screen space density values (for picking mipmap of textures)
	float w[3];
	ALIGN(float attribs[DPSOFTRAST_ARRAY_TOTAL][3][4]);
}
DPSOFTRAST_State_Triangle);

#define DPSOFTRAST_CALCATTRIB(triangle, span, data, slope, arrayindex) { \
	slope = _mm_load_ps((triangle)->attribs[arrayindex][0]); \
	data = _mm_add_ps(_mm_load_ps((triangle)->attribs[arrayindex][2]), \
					_mm_add_ps(_mm_mul_ps(_mm_set1_ps((span)->x), slope), \
								_mm_mul_ps(_mm_set1_ps((span)->y), _mm_load_ps((triangle)->attribs[arrayindex][1])))); \
}
#define DPSOFTRAST_CALCATTRIB4F(triangle, span, data, slope, arrayindex) { \
	slope[0] = (triangle)->attribs[arrayindex][0][0]; \
	slope[1] = (triangle)->attribs[arrayindex][0][1]; \
	slope[2] = (triangle)->attribs[arrayindex][0][2]; \
	slope[3] = (triangle)->attribs[arrayindex][0][3]; \
	data[0] = (triangle)->attribs[arrayindex][2][0] + (span->x)*slope[0] + (span->y)*(triangle)->attribs[arrayindex][1][0]; \
	data[1] = (triangle)->attribs[arrayindex][2][1] + (span->x)*slope[1] + (span->y)*(triangle)->attribs[arrayindex][1][1]; \
	data[2] = (triangle)->attribs[arrayindex][2][2] + (span->x)*slope[2] + (span->y)*(triangle)->attribs[arrayindex][1][2]; \
	data[3] = (triangle)->attribs[arrayindex][2][3] + (span->x)*slope[3] + (span->y)*(triangle)->attribs[arrayindex][1][3]; \
}
					
#define DPSOFTRAST_DRAW_MAXSUBSPAN 16

typedef ALIGN(struct DPSOFTRAST_State_Span_s
{
	int triangle; // triangle this span was generated by
	int x; // framebuffer x coord
	int y; // framebuffer y coord
	int startx; // usable range (according to pixelmask)
	int endx; // usable range (according to pixelmask)
	unsigned char *pixelmask; // true for pixels that passed depth test, false for others
	int depthbase; // depthbuffer value at x (add depthslope*startx to get first pixel's depthbuffer value)
	int depthslope; // depthbuffer value pixel delta
}
DPSOFTRAST_State_Span);

#define DPSOFTRAST_DRAW_MAXSPANS 1024
#define DPSOFTRAST_DRAW_MAXTRIANGLES 128
#define DPSOFTRAST_DRAW_MAXSPANLENGTH 256

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
	DPSOFTRAST_BLENDMODE_INVADD,
	DPSOFTRAST_BLENDMODE_TOTAL
}
DPSOFTRAST_BLENDMODE;

typedef ATOMIC(struct DPSOFTRAST_State_Thread_s
{
	void *thread;
	int index;
	
	int cullface;
	int colormask[4];
	int blendfunc[2];
	int blendsubtract;
	int depthmask;
	int depthtest;
	int depthfunc;
	int scissortest;
	int viewport[4];
	int scissor[4];
	float depthrange[2];
	float polygonoffset[2];
	float clipplane[4];
	ALIGN(float fb_clipplane[4]);

	int shader_mode;
	int shader_permutation;
	int shader_exactspecularmath;

	DPSOFTRAST_Texture *texbound[DPSOFTRAST_MAXTEXTUREUNITS];
	
	ALIGN(float uniform4f[DPSOFTRAST_UNIFORM_TOTAL*4]);
	int uniform1i[DPSOFTRAST_UNIFORM_TOTAL];

	// DPSOFTRAST_VALIDATE_ flags
	int validate;

	// derived values (DPSOFTRAST_VALIDATE_FB)
	int fb_colormask;
	int fb_scissor[4];
	ALIGN(float fb_viewportcenter[4]);
	ALIGN(float fb_viewportscale[4]);

	// derived values (DPSOFTRAST_VALIDATE_DEPTHFUNC)
	int fb_depthfunc;

	// derived values (DPSOFTRAST_VALIDATE_BLENDFUNC)
	int fb_blendmode;

	// band boundaries
	int miny1;
	int maxy1;
	int miny2;
	int maxy2;

	ATOMIC(volatile int commandoffset);

	volatile bool waiting;
	volatile bool starving;
	void *waitcond;
	void *drawcond;
	void *drawmutex;

	int numspans;
	int numtriangles;
	DPSOFTRAST_State_Span spans[DPSOFTRAST_DRAW_MAXSPANS];
	DPSOFTRAST_State_Triangle triangles[DPSOFTRAST_DRAW_MAXTRIANGLES];
	unsigned char pixelmaskarray[DPSOFTRAST_DRAW_MAXSPANLENGTH+4]; // LordHavoc: padded to allow some termination bytes
}
DPSOFTRAST_State_Thread);

typedef ATOMIC(struct DPSOFTRAST_State_s
{
	int fb_width;
	int fb_height;
	unsigned int *fb_depthpixels;
	unsigned int *fb_colorpixels[4];

	int viewport[4];
	ALIGN(float fb_viewportcenter[4]);
	ALIGN(float fb_viewportscale[4]);

	float color[4];
	ALIGN(float uniform4f[DPSOFTRAST_UNIFORM_TOTAL*4]);
	int uniform1i[DPSOFTRAST_UNIFORM_TOTAL];

	const float *pointer_vertex3f;
	const float *pointer_color4f;
	const unsigned char *pointer_color4ub;
	const float *pointer_texcoordf[DPSOFTRAST_MAXTEXCOORDARRAYS];
	int stride_vertex;
	int stride_color;
	int stride_texcoord[DPSOFTRAST_MAXTEXCOORDARRAYS];
	int components_texcoord[DPSOFTRAST_MAXTEXCOORDARRAYS];
	DPSOFTRAST_Texture *texbound[DPSOFTRAST_MAXTEXTUREUNITS];

	int firstvertex;
	int numvertices;
	float *post_array4f[DPSOFTRAST_ARRAY_TOTAL];
	float *screencoord4f;
	int drawstarty;
	int drawendy;
	int drawclipped;
	
	int shader_mode;
	int shader_permutation;
	int shader_exactspecularmath;

	int texture_max;
	int texture_end;
	int texture_firstfree;
	DPSOFTRAST_Texture *texture;

	int bigendian;

	// error reporting
	const char *errorstring;

	bool usethreads;
	int interlace;
	int numthreads;
	DPSOFTRAST_State_Thread *threads;

	ATOMIC(volatile int drawcommand);

	DPSOFTRAST_State_Command_Pool commandpool;
}
DPSOFTRAST_State);

DPSOFTRAST_State dpsoftrast;

#define DPSOFTRAST_DEPTHSCALE (1024.0f*1048576.0f)
#define DPSOFTRAST_DEPTHOFFSET (128.0f)
#define DPSOFTRAST_BGRA8_FROM_RGBA32F(r,g,b,a) (((int)(r * 255.0f + 0.5f) << 16) | ((int)(g * 255.0f + 0.5f) << 8) | (int)(b * 255.0f + 0.5f) | ((int)(a * 255.0f + 0.5f) << 24))
#define DPSOFTRAST_DEPTH32_FROM_DEPTH32F(d) ((int)(DPSOFTRAST_DEPTHSCALE * (1-d)))

static void DPSOFTRAST_Draw_DepthTest(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_State_Span *span);
static void DPSOFTRAST_Draw_DepthWrite(const DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Span *span);

static void DPSOFTRAST_RecalcViewport(const int *viewport, float *fb_viewportcenter, float *fb_viewportscale)
{
	fb_viewportcenter[1] = viewport[0] + 0.5f * viewport[2] - 0.5f;
	fb_viewportcenter[2] = dpsoftrast.fb_height - viewport[1] - 0.5f * viewport[3] - 0.5f;
	fb_viewportcenter[3] = 0.5f;
	fb_viewportcenter[0] = 0.0f;
	fb_viewportscale[1] = 0.5f * viewport[2];
	fb_viewportscale[2] = -0.5f * viewport[3];
	fb_viewportscale[3] = 0.5f;
	fb_viewportscale[0] = 1.0f;
}

static void DPSOFTRAST_RecalcThread(DPSOFTRAST_State_Thread *thread)
{
	if (dpsoftrast.interlace)
	{
		thread->miny1 = (thread->index*dpsoftrast.fb_height)/(2*dpsoftrast.numthreads);
		thread->maxy1 = ((thread->index+1)*dpsoftrast.fb_height)/(2*dpsoftrast.numthreads);
		thread->miny2 = ((dpsoftrast.numthreads+thread->index)*dpsoftrast.fb_height)/(2*dpsoftrast.numthreads);
		thread->maxy2 = ((dpsoftrast.numthreads+thread->index+1)*dpsoftrast.fb_height)/(2*dpsoftrast.numthreads);
	}
	else
	{
		thread->miny1 = thread->miny2 = (thread->index*dpsoftrast.fb_height)/dpsoftrast.numthreads;
		thread->maxy1 = thread->maxy2 = ((thread->index+1)*dpsoftrast.fb_height)/dpsoftrast.numthreads;
	}
}

static void DPSOFTRAST_RecalcClipPlane(DPSOFTRAST_State_Thread *thread)
{
	thread->fb_clipplane[0] = thread->clipplane[0] / thread->fb_viewportscale[1];
	thread->fb_clipplane[1] = thread->clipplane[1] / thread->fb_viewportscale[2];
	thread->fb_clipplane[2] = thread->clipplane[2] / thread->fb_viewportscale[3];
	thread->fb_clipplane[3] = thread->clipplane[3] / thread->fb_viewportscale[0];
	thread->fb_clipplane[3] -= thread->fb_viewportcenter[1]*thread->fb_clipplane[0] + thread->fb_viewportcenter[2]*thread->fb_clipplane[1] + thread->fb_viewportcenter[3]*thread->fb_clipplane[2] + thread->fb_viewportcenter[0]*thread->fb_clipplane[3];
}

static void DPSOFTRAST_RecalcFB(DPSOFTRAST_State_Thread *thread)
{
	// calculate framebuffer scissor, viewport, viewport clipped by scissor,
	// and viewport projection values
	int x1, x2;
	int y1, y2;
	x1 = thread->scissor[0];
	x2 = thread->scissor[0] + thread->scissor[2];
	y1 = dpsoftrast.fb_height - thread->scissor[1] - thread->scissor[3];
	y2 = dpsoftrast.fb_height - thread->scissor[1];
	if (!thread->scissortest) {x1 = 0;y1 = 0;x2 = dpsoftrast.fb_width;y2 = dpsoftrast.fb_height;}
	if (x1 < 0) x1 = 0;
	if (x2 > dpsoftrast.fb_width) x2 = dpsoftrast.fb_width;
	if (y1 < 0) y1 = 0;
	if (y2 > dpsoftrast.fb_height) y2 = dpsoftrast.fb_height;
	thread->fb_scissor[0] = x1;
	thread->fb_scissor[1] = y1;
	thread->fb_scissor[2] = x2 - x1;
	thread->fb_scissor[3] = y2 - y1;

	DPSOFTRAST_RecalcViewport(thread->viewport, thread->fb_viewportcenter, thread->fb_viewportscale);
	DPSOFTRAST_RecalcClipPlane(thread);
	DPSOFTRAST_RecalcThread(thread);
}

static void DPSOFTRAST_RecalcDepthFunc(DPSOFTRAST_State_Thread *thread)
{
	thread->fb_depthfunc = thread->depthtest ? thread->depthfunc : GL_ALWAYS;
}

static void DPSOFTRAST_RecalcBlendFunc(DPSOFTRAST_State_Thread *thread)
{
	if (thread->blendsubtract)
	{
		switch ((thread->blendfunc[0]<<16)|thread->blendfunc[1])
		{
		#define BLENDFUNC(sfactor, dfactor, blendmode) \
			case (sfactor<<16)|dfactor: thread->fb_blendmode = blendmode; break;
		BLENDFUNC(GL_SRC_ALPHA, GL_ONE, DPSOFTRAST_BLENDMODE_SUBALPHA)
		default: thread->fb_blendmode = DPSOFTRAST_BLENDMODE_OPAQUE; break;
		}
	}
	else
	{	
		switch ((thread->blendfunc[0]<<16)|thread->blendfunc[1])
		{
		BLENDFUNC(GL_ONE, GL_ZERO, DPSOFTRAST_BLENDMODE_OPAQUE)
		BLENDFUNC(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, DPSOFTRAST_BLENDMODE_ALPHA)
		BLENDFUNC(GL_SRC_ALPHA, GL_ONE, DPSOFTRAST_BLENDMODE_ADDALPHA)
		BLENDFUNC(GL_ONE, GL_ONE, DPSOFTRAST_BLENDMODE_ADD)
		BLENDFUNC(GL_ZERO, GL_ONE_MINUS_SRC_COLOR, DPSOFTRAST_BLENDMODE_INVMOD)
		BLENDFUNC(GL_ZERO, GL_SRC_COLOR, DPSOFTRAST_BLENDMODE_MUL)
		BLENDFUNC(GL_DST_COLOR, GL_ZERO, DPSOFTRAST_BLENDMODE_MUL)
		BLENDFUNC(GL_DST_COLOR, GL_SRC_COLOR, DPSOFTRAST_BLENDMODE_MUL2)
		BLENDFUNC(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, DPSOFTRAST_BLENDMODE_PSEUDOALPHA)
		BLENDFUNC(GL_ONE_MINUS_DST_COLOR, GL_ONE, DPSOFTRAST_BLENDMODE_INVADD)
		default: thread->fb_blendmode = DPSOFTRAST_BLENDMODE_OPAQUE; break;
		}
	}
}

#define DPSOFTRAST_ValidateQuick(thread, f) ((thread->validate & (f)) ? (DPSOFTRAST_Validate(thread, f), 0) : 0)

static void DPSOFTRAST_Validate(DPSOFTRAST_State_Thread *thread, int mask)
{
	mask &= thread->validate;
	if (!mask)
		return;
	if (mask & DPSOFTRAST_VALIDATE_FB)
	{
		thread->validate &= ~DPSOFTRAST_VALIDATE_FB;
		DPSOFTRAST_RecalcFB(thread);
	}
	if (mask & DPSOFTRAST_VALIDATE_DEPTHFUNC)
	{
		thread->validate &= ~DPSOFTRAST_VALIDATE_DEPTHFUNC;
		DPSOFTRAST_RecalcDepthFunc(thread);
	}
	if (mask & DPSOFTRAST_VALIDATE_BLENDFUNC)
	{
		thread->validate &= ~DPSOFTRAST_VALIDATE_BLENDFUNC;
		DPSOFTRAST_RecalcBlendFunc(thread);
	}
}

DPSOFTRAST_Texture *DPSOFTRAST_Texture_GetByIndex(int index)
{
	if (index >= 1 && index < dpsoftrast.texture_end && dpsoftrast.texture[index].bytes)
		return &dpsoftrast.texture[index];
	return NULL;
}

static void DPSOFTRAST_Texture_Grow(void)
{
	DPSOFTRAST_Texture *oldtexture = dpsoftrast.texture;
	DPSOFTRAST_State_Thread *thread;
	int i;
	int j;
	DPSOFTRAST_Flush();
	// expand texture array as needed
	if (dpsoftrast.texture_max < 1024)
		dpsoftrast.texture_max = 1024;
	else
		dpsoftrast.texture_max *= 2;
	dpsoftrast.texture = (DPSOFTRAST_Texture *)realloc(dpsoftrast.texture, dpsoftrast.texture_max * sizeof(DPSOFTRAST_Texture));
	for (i = 0; i < DPSOFTRAST_MAXTEXTUREUNITS; i++)
		if (dpsoftrast.texbound[i])
			dpsoftrast.texbound[i] = dpsoftrast.texture + (dpsoftrast.texbound[i] - oldtexture);
	for (j = 0; j < dpsoftrast.numthreads; j++)
	{
		thread = &dpsoftrast.threads[j];
		for (i = 0; i < DPSOFTRAST_MAXTEXTUREUNITS; i++)
			if (thread->texbound[i])
				thread->texbound[i] = dpsoftrast.texture + (thread->texbound[i] - oldtexture);
	}
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
		DPSOFTRAST_Texture_Grow();
	if (dpsoftrast.texture_end <= texnum)
		dpsoftrast.texture_end = texnum + 1;
	texture = &dpsoftrast.texture[texnum];
	memset(texture, 0, sizeof(*texture));
	texture->flags = flags;
	texture->width = width;
	texture->height = height;
	texture->depth = depth;
	texture->sides = sides;
	texture->binds = 0;
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
	if (texture->binds)
		DPSOFTRAST_Flush();
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
	unsigned char *dst;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (texture->binds)
		DPSOFTRAST_Flush();
	if (pixels)
	{
		dst = texture->bytes + (blocky * texture->mipmap[0][2] + blockx) * 4;
		while (blockheight > 0)
		{
			memcpy(dst, pixels, blockwidth * 4);
			pixels += blockwidth * 4;
			dst += texture->mipmap[0][2] * 4;
			blockheight--;
		}
	}
	DPSOFTRAST_Texture_CalculateMipmaps(index);
}
void DPSOFTRAST_Texture_UpdateFull(int index, const unsigned char *pixels)
{
	DPSOFTRAST_Texture *texture;
	texture = DPSOFTRAST_Texture_GetByIndex(index);if (!texture) return;
	if (texture->binds)
		DPSOFTRAST_Flush();
	if (pixels)
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
	if (texture->binds)
		DPSOFTRAST_Flush();
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
	if (texture->binds)
		DPSOFTRAST_Flush();
	texture->filter = filter;
}

static void DPSOFTRAST_Draw_FlushThreads(void);

static void DPSOFTRAST_Draw_SyncCommands(void)
{
	if(dpsoftrast.usethreads) MEMORY_BARRIER;
	dpsoftrast.drawcommand = dpsoftrast.commandpool.freecommand;
}

static void DPSOFTRAST_Draw_FreeCommandPool(int space)
{
	DPSOFTRAST_State_Thread *thread;
	int i;
	int freecommand = dpsoftrast.commandpool.freecommand;
	int usedcommands = dpsoftrast.commandpool.usedcommands;
	if (usedcommands <= DPSOFTRAST_DRAW_MAXCOMMANDPOOL-space)
		return;
	DPSOFTRAST_Draw_SyncCommands();
	for(;;)
	{
		int waitindex = -1;
		int commandoffset;
		usedcommands = 0;
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			thread = &dpsoftrast.threads[i]; 
			commandoffset = freecommand - thread->commandoffset;
			if (commandoffset < 0)
				commandoffset += DPSOFTRAST_DRAW_MAXCOMMANDPOOL;
			if (commandoffset > usedcommands)
			{
				waitindex = i;
				usedcommands = commandoffset;
			}
		}
		if (usedcommands <= DPSOFTRAST_DRAW_MAXCOMMANDPOOL-space || waitindex < 0)
			break;
		thread = &dpsoftrast.threads[waitindex];
		Thread_LockMutex(thread->drawmutex);
		if (thread->commandoffset != dpsoftrast.drawcommand)
		{
			thread->waiting = true;
			if (thread->starving) Thread_CondSignal(thread->drawcond);
			Thread_CondWait(thread->waitcond, thread->drawmutex);
			thread->waiting = false;
		}
		Thread_UnlockMutex(thread->drawmutex);
	}
	dpsoftrast.commandpool.usedcommands = usedcommands;
}

#define DPSOFTRAST_ALIGNCOMMAND(size) \
	((size) + ((COMMAND_SIZE - ((size)&(COMMAND_SIZE-1))) & (COMMAND_SIZE-1)))
#define DPSOFTRAST_ALLOCATECOMMAND(name) \
	((DPSOFTRAST_Command_##name *) DPSOFTRAST_AllocateCommand( DPSOFTRAST_OPCODE_##name , DPSOFTRAST_ALIGNCOMMAND(sizeof( DPSOFTRAST_Command_##name ))))

static void *DPSOFTRAST_AllocateCommand(int opcode, int size)
{
	DPSOFTRAST_Command *command;
	int freecommand = dpsoftrast.commandpool.freecommand;
	int usedcommands = dpsoftrast.commandpool.usedcommands;
	int extra = sizeof(DPSOFTRAST_Command);
	if (DPSOFTRAST_DRAW_MAXCOMMANDPOOL - freecommand < size)
		extra += DPSOFTRAST_DRAW_MAXCOMMANDPOOL - freecommand;
	if (usedcommands > DPSOFTRAST_DRAW_MAXCOMMANDPOOL - (size + extra))
	{
		if (dpsoftrast.usethreads)
			DPSOFTRAST_Draw_FreeCommandPool(size + extra);
		else
			DPSOFTRAST_Draw_FlushThreads();
		freecommand = dpsoftrast.commandpool.freecommand;
		usedcommands = dpsoftrast.commandpool.usedcommands;
	}
	if (DPSOFTRAST_DRAW_MAXCOMMANDPOOL - freecommand < size)
	{
		command = (DPSOFTRAST_Command *) &dpsoftrast.commandpool.commands[freecommand];
		command->opcode = DPSOFTRAST_OPCODE_Reset;
		usedcommands += DPSOFTRAST_DRAW_MAXCOMMANDPOOL - freecommand;
		freecommand = 0;
	}
	command = (DPSOFTRAST_Command *) &dpsoftrast.commandpool.commands[freecommand];
	command->opcode = opcode;
	command->commandsize = size;
	freecommand += size;
	if (freecommand >= DPSOFTRAST_DRAW_MAXCOMMANDPOOL)
		freecommand = 0;
	dpsoftrast.commandpool.freecommand = freecommand;
	dpsoftrast.commandpool.usedcommands = usedcommands + size;
	return command;
}

static void DPSOFTRAST_UndoCommand(int size)
{
	int freecommand = dpsoftrast.commandpool.freecommand;
	int usedcommands = dpsoftrast.commandpool.usedcommands;
	freecommand -= size;
	if (freecommand < 0)
		freecommand += DPSOFTRAST_DRAW_MAXCOMMANDPOOL;
	usedcommands -= size;
	dpsoftrast.commandpool.freecommand = freecommand;
	dpsoftrast.commandpool.usedcommands = usedcommands;
}
		
DEFCOMMAND(1, Viewport, int x; int y; int width; int height;)
static void DPSOFTRAST_Interpret_Viewport(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_Command_Viewport *command)
{
	thread->viewport[0] = command->x;
	thread->viewport[1] = command->y;
	thread->viewport[2] = command->width;
	thread->viewport[3] = command->height;
	thread->validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_Viewport(int x, int y, int width, int height)
{
	DPSOFTRAST_Command_Viewport *command = DPSOFTRAST_ALLOCATECOMMAND(Viewport);
	command->x = x;
	command->y = y;
	command->width = width;
	command->height = height;

	dpsoftrast.viewport[0] = x;
	dpsoftrast.viewport[1] = y;
	dpsoftrast.viewport[2] = width;
	dpsoftrast.viewport[3] = height;
	DPSOFTRAST_RecalcViewport(dpsoftrast.viewport, dpsoftrast.fb_viewportcenter, dpsoftrast.fb_viewportscale);
}

DEFCOMMAND(2, ClearColor, float r; float g; float b; float a;) 
static void DPSOFTRAST_Interpret_ClearColor(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_Command_ClearColor *command)
{
	int i, x1, y1, x2, y2, w, h, x, y;
	int miny1, maxy1, miny2, maxy2;
	int bandy;
	unsigned int *p;
	unsigned int c;
	DPSOFTRAST_Validate(thread, DPSOFTRAST_VALIDATE_FB);
	miny1 = thread->miny1;
	maxy1 = thread->maxy1;
	miny2 = thread->miny2;
	maxy2 = thread->maxy2;
	x1 = thread->fb_scissor[0];
	y1 = thread->fb_scissor[1];
	x2 = thread->fb_scissor[0] + thread->fb_scissor[2];
	y2 = thread->fb_scissor[1] + thread->fb_scissor[3];
	if (y1 < miny1) y1 = miny1;
	if (y2 > maxy2) y2 = maxy2;
	w = x2 - x1;
	h = y2 - y1;
	if (w < 1 || h < 1)
		return;
	// FIXME: honor fb_colormask?
	c = DPSOFTRAST_BGRA8_FROM_RGBA32F(command->r,command->g,command->b,command->a);
	for (i = 0;i < 4;i++)
	{
		if (!dpsoftrast.fb_colorpixels[i])
			continue;
		for (y = y1, bandy = min(y2, maxy1); y < y2; bandy = min(y2, maxy2), y = max(y, miny2))
		for (;y < bandy;y++)
		{
			p = dpsoftrast.fb_colorpixels[i] + y * dpsoftrast.fb_width;
			for (x = x1;x < x2;x++)
				p[x] = c;
		}
	}
}
void DPSOFTRAST_ClearColor(float r, float g, float b, float a)
{
	DPSOFTRAST_Command_ClearColor *command = DPSOFTRAST_ALLOCATECOMMAND(ClearColor);
	command->r = r;
	command->g = g;
	command->b = b;
	command->a = a;
}

DEFCOMMAND(3, ClearDepth, float depth;)
static void DPSOFTRAST_Interpret_ClearDepth(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_ClearDepth *command)
{
	int x1, y1, x2, y2, w, h, x, y;
	int miny1, maxy1, miny2, maxy2;
	int bandy;
	unsigned int *p;
	unsigned int c;
	DPSOFTRAST_Validate(thread, DPSOFTRAST_VALIDATE_FB);
	miny1 = thread->miny1;
	maxy1 = thread->maxy1;
	miny2 = thread->miny2;
	maxy2 = thread->maxy2;
	x1 = thread->fb_scissor[0];
	y1 = thread->fb_scissor[1];
	x2 = thread->fb_scissor[0] + thread->fb_scissor[2];
	y2 = thread->fb_scissor[1] + thread->fb_scissor[3];
	if (y1 < miny1) y1 = miny1;
	if (y2 > maxy2) y2 = maxy2;
	w = x2 - x1;
	h = y2 - y1;
	if (w < 1 || h < 1)
		return;
	c = DPSOFTRAST_DEPTH32_FROM_DEPTH32F(command->depth);
	for (y = y1, bandy = min(y2, maxy1); y < y2; bandy = min(y2, maxy2), y = max(y, miny2))
	for (;y < bandy;y++)
	{
		p = dpsoftrast.fb_depthpixels + y * dpsoftrast.fb_width;
		for (x = x1;x < x2;x++)
			p[x] = c;
	}
}
void DPSOFTRAST_ClearDepth(float d)
{
	DPSOFTRAST_Command_ClearDepth *command = DPSOFTRAST_ALLOCATECOMMAND(ClearDepth);
	command->depth = d;
}

DEFCOMMAND(4, ColorMask, int r; int g; int b; int a;)
static void DPSOFTRAST_Interpret_ColorMask(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_ColorMask *command)
{
	thread->colormask[0] = command->r != 0;
	thread->colormask[1] = command->g != 0;
	thread->colormask[2] = command->b != 0;
	thread->colormask[3] = command->a != 0;
	thread->fb_colormask = ((-thread->colormask[0]) & 0x00FF0000) | ((-thread->colormask[1]) & 0x0000FF00) | ((-thread->colormask[2]) & 0x000000FF) | ((-thread->colormask[3]) & 0xFF000000);
}
void DPSOFTRAST_ColorMask(int r, int g, int b, int a)
{
	DPSOFTRAST_Command_ColorMask *command = DPSOFTRAST_ALLOCATECOMMAND(ColorMask);
	command->r = r;
	command->g = g;
	command->b = b;
	command->a = a;
}

DEFCOMMAND(5, DepthTest, int enable;)
static void DPSOFTRAST_Interpret_DepthTest(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_DepthTest *command)
{
	thread->depthtest = command->enable;
	thread->validate |= DPSOFTRAST_VALIDATE_DEPTHFUNC;
}
void DPSOFTRAST_DepthTest(int enable)
{
	DPSOFTRAST_Command_DepthTest *command = DPSOFTRAST_ALLOCATECOMMAND(DepthTest);
	command->enable = enable;
}

DEFCOMMAND(6, ScissorTest, int enable;)
static void DPSOFTRAST_Interpret_ScissorTest(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_ScissorTest *command)
{
	thread->scissortest = command->enable;
	thread->validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_ScissorTest(int enable)
{
	DPSOFTRAST_Command_ScissorTest *command = DPSOFTRAST_ALLOCATECOMMAND(ScissorTest);
	command->enable = enable;
}

DEFCOMMAND(7, Scissor, float x; float y; float width; float height;)
static void DPSOFTRAST_Interpret_Scissor(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_Scissor *command)
{
	thread->scissor[0] = command->x;
	thread->scissor[1] = command->y;
	thread->scissor[2] = command->width;
	thread->scissor[3] = command->height;
	thread->validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_Scissor(float x, float y, float width, float height)
{
	DPSOFTRAST_Command_Scissor *command = DPSOFTRAST_ALLOCATECOMMAND(Scissor);
	command->x = x;
	command->y = y;
	command->width = width;
	command->height = height;
}

DEFCOMMAND(8, BlendFunc, int sfactor; int dfactor;)
static void DPSOFTRAST_Interpret_BlendFunc(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_BlendFunc *command)
{
	thread->blendfunc[0] = command->sfactor;
	thread->blendfunc[1] = command->dfactor;
	thread->validate |= DPSOFTRAST_VALIDATE_BLENDFUNC;
}
void DPSOFTRAST_BlendFunc(int sfactor, int dfactor)
{
	DPSOFTRAST_Command_BlendFunc *command = DPSOFTRAST_ALLOCATECOMMAND(BlendFunc);
	command->sfactor = sfactor;
	command->dfactor = dfactor;
}

DEFCOMMAND(9, BlendSubtract, int enable;)
static void DPSOFTRAST_Interpret_BlendSubtract(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_BlendSubtract *command)
{
	thread->blendsubtract = command->enable;
	thread->validate |= DPSOFTRAST_VALIDATE_BLENDFUNC;
}
void DPSOFTRAST_BlendSubtract(int enable)
{
	DPSOFTRAST_Command_BlendSubtract *command = DPSOFTRAST_ALLOCATECOMMAND(BlendSubtract);
	command->enable = enable;
}

DEFCOMMAND(10, DepthMask, int enable;)
static void DPSOFTRAST_Interpret_DepthMask(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_DepthMask *command)
{
	thread->depthmask = command->enable;
}
void DPSOFTRAST_DepthMask(int enable)
{
	DPSOFTRAST_Command_DepthMask *command = DPSOFTRAST_ALLOCATECOMMAND(DepthMask);
	command->enable = enable;
}

DEFCOMMAND(11, DepthFunc, int func;)
static void DPSOFTRAST_Interpret_DepthFunc(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_DepthFunc *command)
{
	thread->depthfunc = command->func;
}
void DPSOFTRAST_DepthFunc(int func)
{
	DPSOFTRAST_Command_DepthFunc *command = DPSOFTRAST_ALLOCATECOMMAND(DepthFunc);
	command->func = func;
}

DEFCOMMAND(12, DepthRange, float nearval; float farval;)
static void DPSOFTRAST_Interpret_DepthRange(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_DepthRange *command)
{
	thread->depthrange[0] = command->nearval;
	thread->depthrange[1] = command->farval;
}
void DPSOFTRAST_DepthRange(float nearval, float farval)
{
	DPSOFTRAST_Command_DepthRange *command = DPSOFTRAST_ALLOCATECOMMAND(DepthRange);
	command->nearval = nearval;
	command->farval = farval;
}

DEFCOMMAND(13, PolygonOffset, float alongnormal; float intoview;)
static void DPSOFTRAST_Interpret_PolygonOffset(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_PolygonOffset *command)
{
	thread->polygonoffset[0] = command->alongnormal;
	thread->polygonoffset[1] = command->intoview;
}
void DPSOFTRAST_PolygonOffset(float alongnormal, float intoview)
{
	DPSOFTRAST_Command_PolygonOffset *command = DPSOFTRAST_ALLOCATECOMMAND(PolygonOffset);
	command->alongnormal = alongnormal;
	command->intoview = intoview;
}

DEFCOMMAND(14, CullFace, int mode;)
static void DPSOFTRAST_Interpret_CullFace(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_CullFace *command)
{
	thread->cullface = command->mode;
}
void DPSOFTRAST_CullFace(int mode)
{
	DPSOFTRAST_Command_CullFace *command = DPSOFTRAST_ALLOCATECOMMAND(CullFace);
	command->mode = mode;
}

void DPSOFTRAST_Color4f(float r, float g, float b, float a)
{
	dpsoftrast.color[0] = r;
	dpsoftrast.color[1] = g;
	dpsoftrast.color[2] = b;
	dpsoftrast.color[3] = a;
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
	int x;
	int y;
	unsigned char *inpixels;
	unsigned char *b;
	unsigned char *o;
	DPSOFTRAST_Flush();
	if (bx1 < 0) bx1 = 0;
	if (by1 < 0) by1 = 0;
	if (bx2 > dpsoftrast.fb_width) bx2 = dpsoftrast.fb_width;
	if (by2 > dpsoftrast.fb_height) by2 = dpsoftrast.fb_height;
	bw = bx2 - bx1;
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
	DPSOFTRAST_Flush();
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
	sy1 = sheight - 1 - sy1;
	for (y = 0;y < th;y++)
		memcpy(tpixels + ((ty1 + y) * twidth + tx1), spixels + ((sy1 - y) * swidth + sx1), tw*4);
	if (texture->mipmaps > 1)
		DPSOFTRAST_Texture_CalculateMipmaps(index);
}

DEFCOMMAND(17, SetTexture, int unitnum; DPSOFTRAST_Texture *texture;)
static void DPSOFTRAST_Interpret_SetTexture(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_SetTexture *command)
{
	if (thread->texbound[command->unitnum])
		ATOMIC_DECREMENT(thread->texbound[command->unitnum]->binds);
	thread->texbound[command->unitnum] = command->texture;
}
void DPSOFTRAST_SetTexture(int unitnum, int index)
{
	DPSOFTRAST_Command_SetTexture *command;
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

	command = DPSOFTRAST_ALLOCATECOMMAND(SetTexture);
	command->unitnum = unitnum;
	command->texture = texture;

	dpsoftrast.texbound[unitnum] = texture;
	ATOMIC_ADD(texture->binds, dpsoftrast.numthreads);
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

DEFCOMMAND(18, SetShader, int mode; int permutation; int exactspecularmath;)
static void DPSOFTRAST_Interpret_SetShader(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_SetShader *command)
{
	thread->shader_mode = command->mode;
	thread->shader_permutation = command->permutation;
	thread->shader_exactspecularmath = command->exactspecularmath;
}
void DPSOFTRAST_SetShader(int mode, int permutation, int exactspecularmath)
{
	DPSOFTRAST_Command_SetShader *command = DPSOFTRAST_ALLOCATECOMMAND(SetShader);
	command->mode = mode;
	command->permutation = permutation;
	command->exactspecularmath = exactspecularmath;

	dpsoftrast.shader_mode = mode;
	dpsoftrast.shader_permutation = permutation;
	dpsoftrast.shader_exactspecularmath = exactspecularmath;
}

DEFCOMMAND(19, Uniform4f, DPSOFTRAST_UNIFORM index; float val[4];)
static void DPSOFTRAST_Interpret_Uniform4f(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_Uniform4f *command)
{
	memcpy(&thread->uniform4f[command->index*4], command->val, sizeof(command->val));
}
void DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM index, float v0, float v1, float v2, float v3)
{
	DPSOFTRAST_Command_Uniform4f *command = DPSOFTRAST_ALLOCATECOMMAND(Uniform4f);
	command->index = index;
	command->val[0] = v0;
	command->val[1] = v1;
	command->val[2] = v2;
	command->val[3] = v3;

	dpsoftrast.uniform4f[index*4+0] = v0;
	dpsoftrast.uniform4f[index*4+1] = v1;
	dpsoftrast.uniform4f[index*4+2] = v2;
	dpsoftrast.uniform4f[index*4+3] = v3;
}
void DPSOFTRAST_Uniform4fv(DPSOFTRAST_UNIFORM index, const float *v)
{
	DPSOFTRAST_Command_Uniform4f *command = DPSOFTRAST_ALLOCATECOMMAND(Uniform4f);
	command->index = index;
	memcpy(command->val, v, sizeof(command->val));

	memcpy(&dpsoftrast.uniform4f[index*4], v, sizeof(float[4]));
}

DEFCOMMAND(20, UniformMatrix4f, DPSOFTRAST_UNIFORM index; ALIGN(float val[16]);)
static void DPSOFTRAST_Interpret_UniformMatrix4f(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_UniformMatrix4f *command)
{
	memcpy(&thread->uniform4f[command->index*4], command->val, sizeof(command->val));
}
void DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM uniform, int arraysize, int transpose, const float *v)
{
#ifdef SSE_POSSIBLE
	int i, index;
	for (i = 0, index = (int)uniform;i < arraysize;i++, index += 4, v += 16)
	{
		__m128 m0, m1, m2, m3;
		DPSOFTRAST_Command_UniformMatrix4f *command = DPSOFTRAST_ALLOCATECOMMAND(UniformMatrix4f);
		command->index = (DPSOFTRAST_UNIFORM)index;
		if (((size_t)v)&(ALIGN_SIZE-1))
		{
			m0 = _mm_loadu_ps(v);
			m1 = _mm_loadu_ps(v+4);
			m2 = _mm_loadu_ps(v+8);
			m3 = _mm_loadu_ps(v+12);
		}
		else
		{
			m0 = _mm_load_ps(v);
			m1 = _mm_load_ps(v+4);
			m2 = _mm_load_ps(v+8);
			m3 = _mm_load_ps(v+12);
		}
		if (transpose)
		{
			__m128 t0, t1, t2, t3;
			t0 = _mm_unpacklo_ps(m0, m1);
			t1 = _mm_unpacklo_ps(m2, m3);
			t2 = _mm_unpackhi_ps(m0, m1);
			t3 = _mm_unpackhi_ps(m2, m3);
			m0 = _mm_movelh_ps(t0, t1);
			m1 = _mm_movehl_ps(t1, t0);
			m2 = _mm_movelh_ps(t2, t3);
			m3 = _mm_movehl_ps(t3, t2);			
		}
		_mm_store_ps(command->val, m0);
		_mm_store_ps(command->val+4, m1);
		_mm_store_ps(command->val+8, m2);
		_mm_store_ps(command->val+12, m3);
		_mm_store_ps(&dpsoftrast.uniform4f[index*4+0], m0);
		_mm_store_ps(&dpsoftrast.uniform4f[index*4+4], m1);
		_mm_store_ps(&dpsoftrast.uniform4f[index*4+8], m2);
		_mm_store_ps(&dpsoftrast.uniform4f[index*4+12], m3);
	}
#endif
}

DEFCOMMAND(21, Uniform1i, DPSOFTRAST_UNIFORM index; int val;)
static void DPSOFTRAST_Interpret_Uniform1i(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_Uniform1i *command)
{
	thread->uniform1i[command->index] = command->val;
}
void DPSOFTRAST_Uniform1i(DPSOFTRAST_UNIFORM index, int i0)
{
	DPSOFTRAST_Command_Uniform1i *command = DPSOFTRAST_ALLOCATECOMMAND(Uniform1i);
	command->index = index;
	command->val = i0;

	dpsoftrast.uniform1i[command->index] = i0;
}

DEFCOMMAND(24, ClipPlane, float clipplane[4];)
static void DPSOFTRAST_Interpret_ClipPlane(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_ClipPlane *command)
{
	memcpy(thread->clipplane, command->clipplane, 4*sizeof(float));
	thread->validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_ClipPlane(float x, float y, float z, float w)
{
	DPSOFTRAST_Command_ClipPlane *command = DPSOFTRAST_ALLOCATECOMMAND(ClipPlane);
	command->clipplane[0] = x;
	command->clipplane[1] = y;
	command->clipplane[2] = z;
	command->clipplane[3] = w;
}

#ifdef SSE_POSSIBLE
static void DPSOFTRAST_Load4fTo4f(float *dst, const unsigned char *src, int size, int stride)
{
	float *end = dst + size*4;
	if ((((size_t)src)|stride)&(ALIGN_SIZE - 1)) // check for alignment
	{
		while (dst < end)
		{
			_mm_store_ps(dst, _mm_loadu_ps((const float *)src));
			dst += 4;
			src += stride;
		}
	}
	else
	{
		while (dst < end)
		{
			_mm_store_ps(dst, _mm_load_ps((const float *)src));
			dst += 4;
			src += stride;
		}
	}
}

static void DPSOFTRAST_Load3fTo4f(float *dst, const unsigned char *src, int size, int stride)
{
	float *end = dst + size*4;
	if (stride == sizeof(float[3]))
	{
		float *end4 = dst + (size&~3)*4;	
		if (((size_t)src)&(ALIGN_SIZE - 1)) // check for alignment
		{
			while (dst < end4)
			{
				__m128 v1 = _mm_loadu_ps((const float *)src), v2 = _mm_loadu_ps((const float *)src + 4), v3 = _mm_loadu_ps((const float *)src + 8), dv; 
				dv = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(2, 1, 0, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_shuffle_ps(v1, v2, _MM_SHUFFLE(1, 0, 3, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 4, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_shuffle_ps(v2, v3, _MM_SHUFFLE(0, 0, 3, 2));
				dv = _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(2, 1, 0, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 8, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_move_ss(v3, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 12, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dst += 16;
				src += 4*sizeof(float[3]);
			}
		}
		else
		{
			while (dst < end4)
			{
				__m128 v1 = _mm_load_ps((const float *)src), v2 = _mm_load_ps((const float *)src + 4), v3 = _mm_load_ps((const float *)src + 8), dv;
				dv = _mm_shuffle_ps(v1, v1, _MM_SHUFFLE(2, 1, 0, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_shuffle_ps(v1, v2, _MM_SHUFFLE(1, 0, 3, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 4, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_shuffle_ps(v2, v3, _MM_SHUFFLE(0, 0, 3, 2));
				dv = _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(2, 1, 0, 3));
				dv = _mm_move_ss(dv, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 8, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dv = _mm_move_ss(v3, _mm_set_ss(1.0f));
				_mm_store_ps(dst + 12, _mm_shuffle_ps(dv, dv, _MM_SHUFFLE(0, 3, 2, 1)));
				dst += 16;
				src += 4*sizeof(float[3]);
			}
		}
	}
	if ((((size_t)src)|stride)&(ALIGN_SIZE - 1))
	{
		while (dst < end)
		{
			__m128 v = _mm_loadu_ps((const float *)src);
			v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 1, 0, 3));
			v = _mm_move_ss(v, _mm_set_ss(1.0f));
			v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 3, 2, 1));
			_mm_store_ps(dst, v);
			dst += 4;
			src += stride;
		}
	}
	else
	{
		while (dst < end)
		{
			__m128 v = _mm_load_ps((const float *)src);
			v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 1, 0, 3));
			v = _mm_move_ss(v, _mm_set_ss(1.0f));
			v = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 3, 2, 1));
			_mm_store_ps(dst, v);
			dst += 4;
			src += stride;
		}
	}
}

static void DPSOFTRAST_Load2fTo4f(float *dst, const unsigned char *src, int size, int stride)
{
	float *end = dst + size*4;
	__m128 v2 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
	if (stride == sizeof(float[2]))
	{
		float *end2 = dst + (size&~1)*4;
		if (((size_t)src)&(ALIGN_SIZE - 1)) // check for alignment
		{
			while (dst < end2)
			{
				__m128 v = _mm_loadu_ps((const float *)src);
				_mm_store_ps(dst, _mm_shuffle_ps(v, v2, _MM_SHUFFLE(3, 2, 1, 0)));
				_mm_store_ps(dst + 4, _mm_movehl_ps(v2, v));
				dst += 8;
				src += 2*sizeof(float[2]);
			}
		}
		else
		{
			while (dst < end2)
			{
				__m128 v = _mm_load_ps((const float *)src);
				_mm_store_ps(dst, _mm_shuffle_ps(v, v2, _MM_SHUFFLE(3, 2, 1, 0)));
				_mm_store_ps(dst + 4, _mm_movehl_ps(v2, v));
				dst += 8;
				src += 2*sizeof(float[2]);
			}
		}
	}
	while (dst < end)
	{
		_mm_store_ps(dst, _mm_loadl_pi(v2, (__m64 *)src));
		dst += 4;
		src += stride;
	}
}

static void DPSOFTRAST_Load4bTo4f(float *dst, const unsigned char *src, int size, int stride)
{
	float *end = dst + size*4;
	__m128 scale = _mm_set1_ps(1.0f/255.0f);
	if (stride == sizeof(unsigned char[4]))
	{
		float *end4 = dst + (size&~3)*4;
		if (((size_t)src)&(ALIGN_SIZE - 1)) // check for alignment
		{
			while (dst < end4)
			{
				__m128i v = _mm_loadu_si128((const __m128i *)src), v1 = _mm_unpacklo_epi8(v, _mm_setzero_si128()), v2 = _mm_unpackhi_epi8(v, _mm_setzero_si128());
				_mm_store_ps(dst, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(v1, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 4, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(v1, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 8, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(v2, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 12, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(v2, _mm_setzero_si128())), scale));
				dst += 16;
				src += 4*sizeof(unsigned char[4]);
			}
		}
		else
		{
			while (dst < end4)
			{
				__m128i v = _mm_load_si128((const __m128i *)src), v1 = _mm_unpacklo_epi8(v, _mm_setzero_si128()), v2 = _mm_unpackhi_epi8(v, _mm_setzero_si128());
				_mm_store_ps(dst, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(v1, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 4, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(v1, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 8, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(v2, _mm_setzero_si128())), scale));
				_mm_store_ps(dst + 12, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(v2, _mm_setzero_si128())), scale));
				dst += 16;
				src += 4*sizeof(unsigned char[4]);
			}
		}
	}
	while (dst < end)
	{
		__m128i v = _mm_cvtsi32_si128(*(const int *)src);
		_mm_store_ps(dst, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(_mm_unpacklo_epi8(v, _mm_setzero_si128()), _mm_setzero_si128())), scale));
		dst += 4;
		src += stride;
	}
}

static void DPSOFTRAST_Fill4f(float *dst, const float *src, int size)
{
	float *end = dst + 4*size;
	__m128 v = _mm_loadu_ps(src);
	while (dst < end)
	{
		_mm_store_ps(dst, v);
		dst += 4;
	}
}
#endif

void DPSOFTRAST_Vertex_Transform(float *out4f, const float *in4f, int numitems, const float *inmatrix16f)
{
#ifdef SSE_POSSIBLE
	static const float identitymatrix[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
	__m128 m0, m1, m2, m3;
	float *end;
	if (!memcmp(identitymatrix, inmatrix16f, sizeof(float[16])))
	{
		// fast case for identity matrix
		if (out4f != in4f) memcpy(out4f, in4f, numitems * sizeof(float[4]));
		return;
	}
	end = out4f + numitems*4;
	m0 = _mm_loadu_ps(inmatrix16f);
	m1 = _mm_loadu_ps(inmatrix16f + 4);
	m2 = _mm_loadu_ps(inmatrix16f + 8);
	m3 = _mm_loadu_ps(inmatrix16f + 12);
	if (((size_t)in4f)&(ALIGN_SIZE-1)) // check alignment
	{
		while (out4f < end)
		{
			__m128 v = _mm_loadu_ps(in4f);
			_mm_store_ps(out4f,
				_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0)), m0),
						_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)), m1),
							_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)), m2),
										_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)), m3)))));
			out4f += 4;
			in4f += 4;
		}
	}
	else
	{
		while (out4f < end)
		{
			__m128 v = _mm_load_ps(in4f);
			_mm_store_ps(out4f,
				_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0)), m0),
						_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)), m1),
							_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)), m2),
										_mm_mul_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)), m3)))));
			out4f += 4;
			in4f += 4;
		}
	}
#endif
}

void DPSOFTRAST_Vertex_Copy(float *out4f, const float *in4f, int numitems)
{
	memcpy(out4f, in4f, numitems * sizeof(float[4]));
}

#ifdef SSE_POSSIBLE
#define DPSOFTRAST_PROJECTVERTEX(out, in, viewportcenter, viewportscale) \
{ \
	__m128 p = (in), w = _mm_shuffle_ps(p, p, _MM_SHUFFLE(3, 3, 3, 3)); \
	p = _mm_move_ss(_mm_shuffle_ps(p, p, _MM_SHUFFLE(2, 1, 0, 3)), _mm_set_ss(1.0f)); \
	p = _mm_add_ps(viewportcenter, _mm_div_ps(_mm_mul_ps(viewportscale, p), w)); \
	out = _mm_shuffle_ps(p, p, _MM_SHUFFLE(0, 3, 2, 1)); \
}

#define DPSOFTRAST_PROJECTY(out, in, viewportcenter, viewportscale) \
{ \
	__m128 p = (in), w = _mm_shuffle_ps(p, p, _MM_SHUFFLE(3, 3, 3, 3)); \
	p = _mm_move_ss(_mm_shuffle_ps(p, p, _MM_SHUFFLE(2, 1, 0, 3)), _mm_set_ss(1.0f)); \
	p = _mm_add_ps(viewportcenter, _mm_div_ps(_mm_mul_ps(viewportscale, p), w)); \
	out = _mm_shuffle_ps(p, p, _MM_SHUFFLE(0, 3, 2, 1)); \
}

#define DPSOFTRAST_TRANSFORMVERTEX(out, in, m0, m1, m2, m3) \
{ \
	__m128 p = (in); \
	out = _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(p, p, _MM_SHUFFLE(0, 0, 0, 0)), m0), \
	  					  _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(p, p, _MM_SHUFFLE(1, 1, 1, 1)), m1), \
								_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(p, p, _MM_SHUFFLE(2, 2, 2, 2)), m2), \
											_mm_mul_ps(_mm_shuffle_ps(p, p, _MM_SHUFFLE(3, 3, 3, 3)), m3)))); \
}

static int DPSOFTRAST_Vertex_BoundY(int *starty, int *endy, const float *minposf, const float *maxposf, const float *inmatrix16f)
{
	int clipmask = 0xFF;
	__m128 viewportcenter = _mm_load_ps(dpsoftrast.fb_viewportcenter), viewportscale = _mm_load_ps(dpsoftrast.fb_viewportscale);
	__m128 bb[8], clipdist[8], minproj = _mm_set_ss(2.0f), maxproj = _mm_set_ss(-2.0f);
	__m128 m0 = _mm_loadu_ps(inmatrix16f), m1 = _mm_loadu_ps(inmatrix16f + 4), m2 = _mm_loadu_ps(inmatrix16f + 8), m3 = _mm_loadu_ps(inmatrix16f + 12);
	__m128 minpos = _mm_load_ps(minposf), maxpos = _mm_load_ps(maxposf);
	m0 = _mm_shuffle_ps(m0, m0, _MM_SHUFFLE(3, 2, 0, 1));
	m1 = _mm_shuffle_ps(m1, m1, _MM_SHUFFLE(3, 2, 0, 1));
	m2 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(3, 2, 0, 1));
	m3 = _mm_shuffle_ps(m3, m3, _MM_SHUFFLE(3, 2, 0, 1));
	#define BBFRONT(k, pos) \
	{ \
		DPSOFTRAST_TRANSFORMVERTEX(bb[k], pos, m0, m1, m2, m3); \
		clipdist[k] = _mm_add_ss(_mm_shuffle_ps(bb[k], bb[k], _MM_SHUFFLE(2, 2, 2, 2)), _mm_shuffle_ps(bb[k], bb[k], _MM_SHUFFLE(3, 3, 3, 3))); \
		if (_mm_ucomige_ss(clipdist[k], _mm_setzero_ps())) \
		{ \
			__m128 proj; \
			clipmask &= ~(1<<k); \
			proj = _mm_div_ss(bb[k], _mm_shuffle_ps(bb[k], bb[k], _MM_SHUFFLE(3, 3, 3, 3))); \
			minproj = _mm_min_ss(minproj, proj); \
			maxproj = _mm_max_ss(maxproj, proj); \
		} \
	}
	BBFRONT(0, minpos); 
	BBFRONT(1, _mm_move_ss(minpos, maxpos)); 
	BBFRONT(2, _mm_shuffle_ps(_mm_move_ss(maxpos, minpos), minpos, _MM_SHUFFLE(3, 2, 1, 0))); 
	BBFRONT(3, _mm_shuffle_ps(maxpos, minpos, _MM_SHUFFLE(3, 2, 1, 0))); 
	BBFRONT(4, _mm_shuffle_ps(minpos, maxpos, _MM_SHUFFLE(3, 2, 1, 0))); 
	BBFRONT(5, _mm_shuffle_ps(_mm_move_ss(minpos, maxpos), maxpos, _MM_SHUFFLE(3, 2, 1, 0))); 
	BBFRONT(6, _mm_move_ss(maxpos, minpos)); 
	BBFRONT(7, maxpos);
	#define BBCLIP(k) \
	{ \
		if (clipmask&(1<<k)) \
		{ \
			if (!(clipmask&(1<<(k^1)))) \
			{ \
				__m128 frac = _mm_div_ss(clipdist[k], _mm_sub_ss(clipdist[k], clipdist[k^1])); \
				__m128 proj = _mm_add_ps(bb[k], _mm_mul_ps(_mm_shuffle_ps(frac, frac, _MM_SHUFFLE(0, 0, 0, 0)), _mm_sub_ps(bb[k^1], bb[k]))); \
				proj = _mm_div_ss(proj, _mm_shuffle_ps(proj, proj, _MM_SHUFFLE(3, 3, 3, 3))); \
				minproj = _mm_min_ss(minproj, proj); \
				maxproj = _mm_max_ss(maxproj, proj); \
			} \
			if (!(clipmask&(1<<(k^2)))) \
			{ \
				__m128 frac = _mm_div_ss(clipdist[k], _mm_sub_ss(clipdist[k], clipdist[k^2])); \
				__m128 proj = _mm_add_ps(bb[k], _mm_mul_ps(_mm_shuffle_ps(frac, frac, _MM_SHUFFLE(0, 0, 0, 0)), _mm_sub_ps(bb[k^2], bb[k]))); \
				proj = _mm_div_ss(proj, _mm_shuffle_ps(proj, proj, _MM_SHUFFLE(3, 3, 3, 3))); \
				minproj = _mm_min_ss(minproj, proj); \
				maxproj = _mm_max_ss(maxproj, proj); \
			} \
			if (!(clipmask&(1<<(k^4)))) \
			{ \
				__m128 frac = _mm_div_ss(clipdist[k], _mm_sub_ss(clipdist[k], clipdist[k^4])); \
				__m128 proj = _mm_add_ps(bb[k], _mm_mul_ps(_mm_shuffle_ps(frac, frac, _MM_SHUFFLE(0, 0, 0, 0)), _mm_sub_ps(bb[k^4], bb[k]))); \
				proj = _mm_div_ss(proj, _mm_shuffle_ps(proj, proj, _MM_SHUFFLE(3, 3, 3, 3))); \
				minproj = _mm_min_ss(minproj, proj); \
				maxproj = _mm_max_ss(maxproj, proj); \
			} \
		} \
	}
	BBCLIP(0); BBCLIP(1); BBCLIP(2); BBCLIP(3); BBCLIP(4); BBCLIP(5); BBCLIP(6); BBCLIP(7);
	viewportcenter = _mm_shuffle_ps(viewportcenter, viewportcenter, _MM_SHUFFLE(0, 3, 1, 2));
	viewportscale = _mm_shuffle_ps(viewportscale, viewportscale, _MM_SHUFFLE(0, 3, 1, 2));
	minproj = _mm_max_ss(minproj, _mm_set_ss(-2.0f));
	maxproj = _mm_min_ss(maxproj, _mm_set_ss(2.0f));
	minproj = _mm_add_ss(viewportcenter, _mm_mul_ss(minproj, viewportscale));
	maxproj = _mm_add_ss(viewportcenter, _mm_mul_ss(maxproj, viewportscale));
	*starty = _mm_cvttss_si32(maxproj);
	*endy = _mm_cvttss_si32(minproj)+1;
	return clipmask;
}
	
static int DPSOFTRAST_Vertex_Project(float *out4f, float *screen4f, int *starty, int *endy, const float *in4f, int numitems)
{
	static const float identitymatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	float *end = out4f + numitems*4;
	__m128 viewportcenter = _mm_load_ps(dpsoftrast.fb_viewportcenter), viewportscale = _mm_load_ps(dpsoftrast.fb_viewportscale);
	__m128 minpos, maxpos;
	if (((size_t)in4f)&(ALIGN_SIZE-1)) // check alignment
	{
		minpos = maxpos = _mm_loadu_ps(in4f);
		while (out4f < end)
		{
			__m128 v = _mm_loadu_ps(in4f);
			minpos = _mm_min_ps(minpos, v);
			maxpos = _mm_max_ps(maxpos, v);
			_mm_store_ps(out4f, v);
			DPSOFTRAST_PROJECTVERTEX(v, v, viewportcenter, viewportscale);
			_mm_store_ps(screen4f, v);
			in4f += 4;
			out4f += 4;
			screen4f += 4;
		}
	}
	else
	{
		minpos = maxpos = _mm_load_ps(in4f);
		while (out4f < end)
		{
			__m128 v = _mm_load_ps(in4f);
			minpos = _mm_min_ps(minpos, v);
			maxpos = _mm_max_ps(maxpos, v);
			_mm_store_ps(out4f, v);
			DPSOFTRAST_PROJECTVERTEX(v, v, viewportcenter, viewportscale);
			_mm_store_ps(screen4f, v);
			in4f += 4;
			out4f += 4;
			screen4f += 4;
		}
	}
	if (starty && endy) 
	{
		ALIGN(float minposf[4]);
		ALIGN(float maxposf[4]);
		_mm_store_ps(minposf, minpos);
		_mm_store_ps(maxposf, maxpos);
		return DPSOFTRAST_Vertex_BoundY(starty, endy, minposf, maxposf, identitymatrix);
	}
	return 0;
}

static int DPSOFTRAST_Vertex_TransformProject(float *out4f, float *screen4f, int *starty, int *endy, const float *in4f, int numitems, const float *inmatrix16f)
{
	static const float identitymatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	__m128 m0, m1, m2, m3, viewportcenter, viewportscale, minpos, maxpos;
	float *end;
	if (!memcmp(identitymatrix, inmatrix16f, sizeof(float[16])))
		return DPSOFTRAST_Vertex_Project(out4f, screen4f, starty, endy, in4f, numitems);
	end = out4f + numitems*4;
	viewportcenter = _mm_load_ps(dpsoftrast.fb_viewportcenter);
	viewportscale = _mm_load_ps(dpsoftrast.fb_viewportscale);
	m0 = _mm_loadu_ps(inmatrix16f);
	m1 = _mm_loadu_ps(inmatrix16f + 4);
	m2 = _mm_loadu_ps(inmatrix16f + 8);
	m3 = _mm_loadu_ps(inmatrix16f + 12);
	if (((size_t)in4f)&(ALIGN_SIZE-1)) // check alignment
	{
		minpos = maxpos = _mm_loadu_ps(in4f);
		while (out4f < end)
		{
			__m128 v = _mm_loadu_ps(in4f);
			minpos = _mm_min_ps(minpos, v);
			maxpos = _mm_max_ps(maxpos, v);
			DPSOFTRAST_TRANSFORMVERTEX(v, v, m0, m1, m2, m3);
			_mm_store_ps(out4f, v);
			DPSOFTRAST_PROJECTVERTEX(v, v, viewportcenter, viewportscale);
			_mm_store_ps(screen4f, v);
			in4f += 4;
			out4f += 4;
			screen4f += 4;
		}
	}
	else
	{
		minpos = maxpos = _mm_load_ps(in4f);
		while (out4f < end)
		{
			__m128 v = _mm_load_ps(in4f);
			minpos = _mm_min_ps(minpos, v);
			maxpos = _mm_max_ps(maxpos, v);
			DPSOFTRAST_TRANSFORMVERTEX(v, v, m0, m1, m2, m3);
			_mm_store_ps(out4f, v);
			DPSOFTRAST_PROJECTVERTEX(v, v, viewportcenter, viewportscale);
			_mm_store_ps(screen4f, v);
			in4f += 4;
			out4f += 4;
			screen4f += 4;
		}
	}
	if (starty && endy) 
	{
		ALIGN(float minposf[4]);
		ALIGN(float maxposf[4]);
		_mm_store_ps(minposf, minpos);
		_mm_store_ps(maxposf, maxpos);
		return DPSOFTRAST_Vertex_BoundY(starty, endy, minposf, maxposf, inmatrix16f); 
	}
	return 0;
}
#endif

static float *DPSOFTRAST_Array_Load(int outarray, int inarray)
{
#ifdef SSE_POSSIBLE
	float *outf = dpsoftrast.post_array4f[outarray];
	const unsigned char *inb;
	int firstvertex = dpsoftrast.firstvertex;
	int numvertices = dpsoftrast.numvertices;
	int stride;
	switch(inarray)
	{
	case DPSOFTRAST_ARRAY_POSITION:
		stride = dpsoftrast.stride_vertex;
		inb = (unsigned char *)dpsoftrast.pointer_vertex3f + firstvertex * stride;
		DPSOFTRAST_Load3fTo4f(outf, inb, numvertices, stride);
		break;
	case DPSOFTRAST_ARRAY_COLOR:
		stride = dpsoftrast.stride_color;
		if (dpsoftrast.pointer_color4f)
		{
			inb = (const unsigned char *)dpsoftrast.pointer_color4f + firstvertex * stride;
			DPSOFTRAST_Load4fTo4f(outf, inb, numvertices, stride);
		}
		else if (dpsoftrast.pointer_color4ub)
		{
			stride = dpsoftrast.stride_color;
			inb = (const unsigned char *)dpsoftrast.pointer_color4ub + firstvertex * stride;
			DPSOFTRAST_Load4bTo4f(outf, inb, numvertices, stride);
		}
		else
		{
			DPSOFTRAST_Fill4f(outf, dpsoftrast.color, numvertices);
		}
		break;
	default:
		stride = dpsoftrast.stride_texcoord[inarray-DPSOFTRAST_ARRAY_TEXCOORD0];
		if (dpsoftrast.pointer_texcoordf[inarray-DPSOFTRAST_ARRAY_TEXCOORD0])
		{
			inb = (const unsigned char *)dpsoftrast.pointer_texcoordf[inarray-DPSOFTRAST_ARRAY_TEXCOORD0] + firstvertex * stride;
			switch(dpsoftrast.components_texcoord[inarray-DPSOFTRAST_ARRAY_TEXCOORD0])
			{
			case 2:
				DPSOFTRAST_Load2fTo4f(outf, inb, numvertices, stride);
				break;
			case 3:
				DPSOFTRAST_Load3fTo4f(outf, inb, numvertices, stride);
				break;
			case 4:
				DPSOFTRAST_Load4fTo4f(outf, inb, numvertices, stride);
				break;
			}
		}
		break;
	}
	return outf;
#else
	return NULL;
#endif
}

static float *DPSOFTRAST_Array_Transform(int outarray, int inarray, const float *inmatrix16f)
{
	float *data = inarray >= 0 ? DPSOFTRAST_Array_Load(outarray, inarray) : dpsoftrast.post_array4f[outarray];
	DPSOFTRAST_Vertex_Transform(data, data, dpsoftrast.numvertices, inmatrix16f);
	return data;
}

#if 0
static float *DPSOFTRAST_Array_Project(int outarray, int inarray)
{
#ifdef SSE_POSSIBLE
	float *data = inarray >= 0 ? DPSOFTRAST_Array_Load(outarray, inarray) : dpsoftrast.post_array4f[outarray];
	dpsoftrast.drawclipped = DPSOFTRAST_Vertex_Project(data, dpsoftrast.screencoord4f, &dpsoftrast.drawstarty, &dpsoftrast.drawendy, data, dpsoftrast.numvertices);
	return data;
#else
	return NULL;
#endif
}
#endif

static float *DPSOFTRAST_Array_TransformProject(int outarray, int inarray, const float *inmatrix16f)
{
#ifdef SSE_POSSIBLE
	float *data = inarray >= 0 ? DPSOFTRAST_Array_Load(outarray, inarray) : dpsoftrast.post_array4f[outarray];
	dpsoftrast.drawclipped = DPSOFTRAST_Vertex_TransformProject(data, dpsoftrast.screencoord4f, &dpsoftrast.drawstarty, &dpsoftrast.drawendy, data, dpsoftrast.numvertices, inmatrix16f);
	return data;
#else
	return NULL;
#endif
}

void DPSOFTRAST_Draw_Span_Begin(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float wslope = triangle->w[0];
	float w = triangle->w[2] + span->x*wslope + span->y*triangle->w[1];
	float endz = 1.0f / (w + wslope * startx);
	if (triangle->w[0] == 0)
	{
		// LordHavoc: fast flat polygons (HUD/menu)
		for (x = startx;x < endx;x++)
			zf[x] = endz;
		return;
	}
	for (x = startx;x < endx;)
	{
		int nextsub = x + DPSOFTRAST_DRAW_MAXSUBSPAN, endsub = nextsub - 1;
		float z = endz, dz;
		if (nextsub >= endx) nextsub = endsub = endx-1;
		endz = 1.0f / (w + wslope * nextsub);
		dz = x < nextsub ? (endz - z) / (nextsub - x) : 0.0f;
		for (; x <= endsub; x++, z += dz)
			zf[x] = z;
	}
}

void DPSOFTRAST_Draw_Span_FinishBGRA8(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, const unsigned char* RESTRICT in4ub)
{
#ifdef SSE_POSSIBLE
	int x;
	int startx = span->startx;
	int endx = span->endx;
	int maskx;
	int subx;
	const unsigned int * RESTRICT ini = (const unsigned int *)in4ub;
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0];
	unsigned int * RESTRICT pixeli = (unsigned int *)dpsoftrast.fb_colorpixels[0];
	if (!pixel)
		return;
	pixel += (span->y * dpsoftrast.fb_width + span->x) * 4;
	pixeli += span->y * dpsoftrast.fb_width + span->x;
	// handle alphatest now (this affects depth writes too)
	if (thread->shader_permutation & SHADERPERMUTATION_ALPHAKILL)
		for (x = startx;x < endx;x++)
			if (in4ub[x*4+3] < 128)
				pixelmask[x] = false;
	// LordHavoc: clear pixelmask for some pixels in alphablend cases, this
	// helps sprites, text and hud artwork
	switch(thread->fb_blendmode)
	{
	case DPSOFTRAST_BLENDMODE_ALPHA:
	case DPSOFTRAST_BLENDMODE_ADDALPHA:
	case DPSOFTRAST_BLENDMODE_SUBALPHA:
		maskx = startx;
		for (x = startx;x < endx;x++)
		{
			if (in4ub[x*4+3] >= 1)
			{
				startx = x;
				for (;;)
				{
					while (++x < endx && in4ub[x*4+3] >= 1) ;
					maskx = x;
					if (x >= endx) break;
					++x;
					while (++x < endx && in4ub[x*4+3] < 1) pixelmask[x] = false;
					if (x >= endx) break;
				}
				break;
			}
		}
		endx = maskx;
		break;
	case DPSOFTRAST_BLENDMODE_OPAQUE:
	case DPSOFTRAST_BLENDMODE_ADD:
	case DPSOFTRAST_BLENDMODE_INVMOD:
	case DPSOFTRAST_BLENDMODE_MUL:
	case DPSOFTRAST_BLENDMODE_MUL2:
	case DPSOFTRAST_BLENDMODE_PSEUDOALPHA:
	case DPSOFTRAST_BLENDMODE_INVADD:
		break;
	}
	// put some special values at the end of the mask to ensure the loops end
	pixelmask[endx] = 1;
	pixelmask[endx+1] = 0;
	// LordHavoc: use a double loop to identify subspans, this helps the
	// optimized copy/blend loops to perform at their best, most triangles
	// have only one run of pixels, and do the search using wide reads...
	x = startx;
	while (x < endx)
	{
		// if this pixel is masked off, it's probably not alone...
		if (!pixelmask[x])
		{
			x++;
#if 1
			if (x + 8 < endx)
			{
				// the 4-item search must be aligned or else it stalls badly
				if ((x & 3) && !pixelmask[x]) 
				{
					if(pixelmask[x]) goto endmasked;
					x++;
					if (x & 3)
					{
						if(pixelmask[x]) goto endmasked;
						x++;
						if (x & 3)
						{
							if(pixelmask[x]) goto endmasked;
							x++;
						}
					}
				}
				while (*(unsigned int *)&pixelmask[x] == 0x00000000)
					x += 4;
			}
#endif
			for (;!pixelmask[x];x++)
				;
			// rather than continue the loop, just check the end variable
			if (x >= endx)
				break;
		}
	endmasked:
		// find length of subspan
		subx = x + 1;
#if 1
		if (subx + 8 < endx)
		{
			if (subx & 3)
			{
				if(!pixelmask[subx]) goto endunmasked;
				subx++;
				if (subx & 3)
				{
					if(!pixelmask[subx]) goto endunmasked;
					subx++;
					if (subx & 3)
					{
						if(!pixelmask[subx]) goto endunmasked;
						subx++;
					}
				}
			}
			while (*(unsigned int *)&pixelmask[subx] == 0x01010101)
				subx += 4;
		}
#endif
		for (;pixelmask[subx];subx++)
			;
		// the checks can overshoot, so make sure to clip it...
		if (subx > endx)
			subx = endx;
	endunmasked:
		// now that we know the subspan length...  process!
		switch(thread->fb_blendmode)
		{
		case DPSOFTRAST_BLENDMODE_OPAQUE:
#if 0
			if (subx - x >= 16)
			{
				memcpy(pixeli + x, ini + x, (subx - x) * sizeof(pixeli[x]));
				x = subx;
			}
			else
#elif 1
			while (x + 16 <= subx)
			{
				_mm_storeu_si128((__m128i *)&pixeli[x], _mm_loadu_si128((const __m128i *)&ini[x]));
				_mm_storeu_si128((__m128i *)&pixeli[x+4], _mm_loadu_si128((const __m128i *)&ini[x+4]));
				_mm_storeu_si128((__m128i *)&pixeli[x+8], _mm_loadu_si128((const __m128i *)&ini[x+8]));
				_mm_storeu_si128((__m128i *)&pixeli[x+12], _mm_loadu_si128((const __m128i *)&ini[x+12]));
				x += 16;
			}
#endif
			{
				while (x + 4 <= subx)
				{
					_mm_storeu_si128((__m128i *)&pixeli[x], _mm_loadu_si128((const __m128i *)&ini[x]));
					x += 4;
				}
				if (x + 2 <= subx)
				{
					pixeli[x] = ini[x];
					pixeli[x+1] = ini[x+1];
					x += 2;
				}
				if (x < subx)
				{
					pixeli[x] = ini[x];
					x++;
				}
			}
			break;
		case DPSOFTRAST_BLENDMODE_ALPHA:
		#define FINISHBLEND(blend2, blend1) \
			for (;x + 1 < subx;x += 2) \
			{ \
				__m128i src, dst; \
				src = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ini[x]), _mm_setzero_si128()); \
				dst = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&pixeli[x]), _mm_setzero_si128()); \
				blend2; \
				_mm_storel_epi64((__m128i *)&pixeli[x], _mm_packus_epi16(dst, dst)); \
			} \
			if (x < subx) \
			{ \
				__m128i src, dst; \
				src = _mm_unpacklo_epi8(_mm_cvtsi32_si128(ini[x]), _mm_setzero_si128()); \
				dst = _mm_unpacklo_epi8(_mm_cvtsi32_si128(pixeli[x]), _mm_setzero_si128()); \
				blend1; \
				pixeli[x] = _mm_cvtsi128_si32(_mm_packus_epi16(dst, dst)); \
				x++; \
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
		case DPSOFTRAST_BLENDMODE_INVADD:
			FINISHBLEND({
				dst = _mm_add_epi16(dst, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(_mm_set1_epi16(255), dst), 4), _mm_slli_epi16(src, 4)));
			}, {
				dst = _mm_add_epi16(dst, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(_mm_set1_epi16(255), dst), 4), _mm_slli_epi16(src, 4)));
			});
			break;
		}
	}
#endif
}

static void DPSOFTRAST_Texture2DBGRA8(DPSOFTRAST_Texture *texture, int mip, float x, float y, unsigned char c[4])
	// warning: this is SLOW, only use if the optimized per-span functions won't do
{
	const unsigned char * RESTRICT pixelbase;
	const unsigned char * RESTRICT pixel[4];
	int width = texture->mipmap[mip][2], height = texture->mipmap[mip][3];
	int wrapmask[2] = { width-1, height-1 };
	pixelbase = (unsigned char *)texture->bytes + texture->mipmap[mip][0];
	if(texture->filter & DPSOFTRAST_TEXTURE_FILTER_LINEAR)
	{
		unsigned int tc[2] = { x * (width<<12) - 2048, y * (height<<12) - 2048};
		unsigned int frac[2] = { tc[0]&0xFFF, tc[1]&0xFFF };
		unsigned int ifrac[2] = { 0x1000 - frac[0], 0x1000 - frac[1] };
		unsigned int lerp[4] = { ifrac[0]*ifrac[1], frac[0]*ifrac[1], ifrac[0]*frac[1], frac[0]*frac[1] };
		int tci[2] = { tc[0]>>12, tc[1]>>12 };
		int tci1[2] = { tci[0] + 1, tci[1] + 1 };
		if (texture->flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
		{
			tci[0] = tci[0] >= 0 ? (tci[0] <= wrapmask[0] ? tci[0] : wrapmask[0]) : 0;
			tci[1] = tci[1] >= 0 ? (tci[1] <= wrapmask[1] ? tci[1] : wrapmask[1]) : 0;
			tci1[0] = tci1[0] >= 0 ? (tci1[0] <= wrapmask[0] ? tci1[0] : wrapmask[0]) : 0;
			tci1[1] = tci1[1] >= 0 ? (tci1[1] <= wrapmask[1] ? tci1[1] : wrapmask[1]) : 0;
		}
		else
		{
			tci[0] &= wrapmask[0];
			tci[1] &= wrapmask[1];
			tci1[0] &= wrapmask[0];
			tci1[1] &= wrapmask[1];
		}
		pixel[0] = pixelbase + 4 * (tci[1]*width+tci[0]);
		pixel[1] = pixelbase + 4 * (tci[1]*width+tci1[0]);
		pixel[2] = pixelbase + 4 * (tci1[1]*width+tci[0]);
		pixel[3] = pixelbase + 4 * (tci1[1]*width+tci1[0]);
		c[0] = (pixel[0][0]*lerp[0]+pixel[1][0]*lerp[1]+pixel[2][0]*lerp[2]+pixel[3][0]*lerp[3])>>24;
		c[1] = (pixel[0][1]*lerp[0]+pixel[1][1]*lerp[1]+pixel[2][1]*lerp[2]+pixel[3][1]*lerp[3])>>24;
		c[2] = (pixel[0][2]*lerp[0]+pixel[1][2]*lerp[1]+pixel[2][2]*lerp[2]+pixel[3][2]*lerp[3])>>24;
		c[3] = (pixel[0][3]*lerp[0]+pixel[1][3]*lerp[1]+pixel[2][3]*lerp[2]+pixel[3][3]*lerp[3])>>24;
	}
	else
	{
		int tci[2] = { x * width, y * height };
		if (texture->flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
		{
			tci[0] = tci[0] >= 0 ? (tci[0] <= wrapmask[0] ? tci[0] : wrapmask[0]) : 0;
			tci[1] = tci[1] >= 0 ? (tci[1] <= wrapmask[1] ? tci[1] : wrapmask[1]) : 0;
		}
		else
		{
			tci[0] &= wrapmask[0];
			tci[1] &= wrapmask[1];
		}
		pixel[0] = pixelbase + 4 * (tci[1]*width+tci[0]);
		c[0] = pixel[0][0];
		c[1] = pixel[0][1];
		c[2] = pixel[0][2];
		c[3] = pixel[0][3];
	}
}

void DPSOFTRAST_Draw_Span_Texture2DVarying(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float * RESTRICT out4f, int texunitindex, int arrayindex, const float * RESTRICT zf)
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
	DPSOFTRAST_Texture *texture = thread->texbound[texunitindex];
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
	mip = triangle->mip[texunitindex];
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
	DPSOFTRAST_CALCATTRIB4F(triangle, span, data, slope, arrayindex);
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
	endtc[0] = (data[0] + slope[0]*startx) * zf[startx] * tcscale[0];
	endtc[1] = (data[1] + slope[1]*startx) * zf[startx] * tcscale[1];
	if (filter)
	{
		endtc[0] -= 0.5f;
		endtc[1] -= 0.5f;
	}
	for (x = startx;x < endx;)
	{
		unsigned int subtc[2];
		unsigned int substep[2];
		float subscale = 4096.0f/DPSOFTRAST_DRAW_MAXSUBSPAN;
		int nextsub = x + DPSOFTRAST_DRAW_MAXSUBSPAN, endsub = nextsub - 1;
		if (nextsub >= endx)
		{
			nextsub = endsub = endx-1;	
			if (x < nextsub) subscale = 4096.0f / (nextsub - x);
		}
		tc[0] = endtc[0];
		tc[1] = endtc[1];
		endtc[0] = (data[0] + slope[0]*nextsub) * zf[nextsub] * tcscale[0];
		endtc[1] = (data[1] + slope[1]*nextsub) * zf[nextsub] * tcscale[1];
		if (filter)
		{
			endtc[0] -= 0.5f;
			endtc[1] -= 0.5f;
		}
		substep[0] = (endtc[0] - tc[0]) * subscale;
		substep[1] = (endtc[1] - tc[1]) * subscale;
		subtc[0] = tc[0] * (1<<12);
		subtc[1] = tc[1] * (1<<12);
		if (filter)
		{
			if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
			{
				for (; x <= endsub; x++, subtc[0] += substep[0], subtc[1] += substep[1])
				{
					unsigned int frac[2] = { subtc[0]&0xFFF, subtc[1]&0xFFF };
					unsigned int ifrac[2] = { 0x1000 - frac[0], 0x1000 - frac[1] };
					unsigned int lerp[4] = { ifrac[0]*ifrac[1], frac[0]*ifrac[1], ifrac[0]*frac[1], frac[0]*frac[1] };
					tci[0] = subtc[0]>>12;
					tci[1] = subtc[1]>>12;
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
					tci[0] = subtc[0]>>12;
					tci[1] = subtc[1]>>12;
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
				tci[0] = subtc[0]>>12;
				tci[1] = subtc[1]>>12;
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
				tci[0] = subtc[0]>>12;
				tci[1] = subtc[1]>>12;
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

void DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char * RESTRICT out4ub, int texunitindex, int arrayindex, const float * RESTRICT zf)
{
#ifdef SSE_POSSIBLE
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
	int affine; // LordHavoc: optimized affine texturing case
	unsigned int * RESTRICT outi = (unsigned int *)out4ub;
	const unsigned char * RESTRICT pixelbase;
	DPSOFTRAST_Texture *texture = thread->texbound[texunitindex];
	// if no texture is bound, just fill it with white
	if (!texture)
	{
		memset(out4ub + startx*4, 255, (span->endx - span->startx)*4);
		return;
	}
	mip = triangle->mip[texunitindex];
	pixelbase = (const unsigned char *)texture->bytes + texture->mipmap[mip][0];
	// if this mipmap of the texture is 1 pixel, just fill it with that color
	if (texture->mipmap[mip][1] == 4)
	{
		unsigned int k = *((const unsigned int *)pixelbase);
		for (x = startx;x < endx;x++)
			outi[x] = k;
		return;
	}
	affine = zf[startx] == zf[endx-1];
	filter = texture->filter & DPSOFTRAST_TEXTURE_FILTER_LINEAR;
	DPSOFTRAST_CALCATTRIB(triangle, span, data, slope, arrayindex);
	flags = texture->flags;
	tcsize = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i *)&texture->mipmap[mip][0]), _MM_SHUFFLE(3, 2, 3, 2));
	tcmask = _mm_sub_epi32(tcsize, _mm_set1_epi32(1));
	tcscale = _mm_cvtepi32_ps(tcsize);
	data = _mm_mul_ps(_mm_movelh_ps(data, data), tcscale);
	slope = _mm_mul_ps(_mm_movelh_ps(slope, slope), tcscale);
	endtc = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx))), _mm_load1_ps(&zf[startx]));
	if (filter)
		endtc = _mm_sub_ps(endtc, _mm_set1_ps(0.5f));
	endsubtc = _mm_cvtps_epi32(_mm_mul_ps(endtc, _mm_set1_ps(65536.0f)));
	tcoffset = _mm_add_epi32(_mm_slli_epi32(_mm_shuffle_epi32(tcsize, _MM_SHUFFLE(0, 0, 0, 0)), 18), _mm_set1_epi32(4));
	tcmax = _mm_packs_epi32(tcmask, tcmask);
	for (x = startx;x < endx;)
	{
		int nextsub = x + DPSOFTRAST_DRAW_MAXSUBSPAN, endsub = nextsub - 1;
		__m128 subscale = _mm_set1_ps(65536.0f/DPSOFTRAST_DRAW_MAXSUBSPAN);
		if (nextsub >= endx || affine)
		{
			nextsub = endsub = endx-1;
			if (x < nextsub) subscale = _mm_set1_ps(65536.0f / (nextsub - x));
		}	
		tc = endtc;
		subtc = endsubtc;
		endtc = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(nextsub))), _mm_load1_ps(&zf[nextsub]));
		if (filter)
			endtc = _mm_sub_ps(endtc, _mm_set1_ps(0.5f));
		substep = _mm_cvtps_epi32(_mm_mul_ps(_mm_sub_ps(endtc, tc), subscale));
		endsubtc = _mm_cvtps_epi32(_mm_mul_ps(endtc, _mm_set1_ps(65536.0f)));
		subtc = _mm_unpacklo_epi64(subtc, _mm_add_epi32(subtc, substep));
		substep = _mm_slli_epi32(substep, 1);
		if (filter)
		{
			__m128i tcrange = _mm_srai_epi32(_mm_unpacklo_epi64(subtc, _mm_add_epi32(endsubtc, substep)), 16);
			if (_mm_movemask_epi8(_mm_andnot_si128(_mm_cmplt_epi32(tcrange, _mm_setzero_si128()), _mm_cmplt_epi32(tcrange, tcmask))) == 0xFFFF)
			{
				int stride = _mm_cvtsi128_si32(tcoffset)>>16;
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					const unsigned char * RESTRICT ptr1, * RESTRICT ptr2;			
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, pix3, pix4, fracm;
					tci = _mm_madd_epi16(tci, tcoffset);
					ptr1 = pixelbase + _mm_cvtsi128_si32(tci);
					ptr2 = pixelbase + _mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)));
					pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)ptr1), _mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(ptr1 + stride)), _mm_setzero_si128());
					pix3 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)ptr2), _mm_setzero_si128());
					pix4 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(ptr2 + stride)), _mm_setzero_si128());
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
					const unsigned char * RESTRICT ptr1;
					__m128i tci = _mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), pix1, pix2, fracm;
					tci = _mm_madd_epi16(tci, tcoffset);
					ptr1 = pixelbase + _mm_cvtsi128_si32(tci);
					pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)ptr1), _mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)(ptr1 + stride)), _mm_setzero_si128());
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
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shuffle_epi32(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(1, 0, 1, 0)), pix1, pix2, pix3, pix4, fracm;
					tci = _mm_min_epi16(_mm_max_epi16(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), _mm_setzero_si128()), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])),
											_mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])),
											_mm_setzero_si128());
					tci = _mm_shuffle_epi32(_mm_shufflehi_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 2, 3, 2));
					tci = _mm_and_si128(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix3 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])),
											_mm_setzero_si128());
					pix4 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])),
											_mm_setzero_si128());
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
					__m128i tci = _mm_shuffle_epi32(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(1, 0, 1, 0)), pix1, pix2, fracm;
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
					x++;
				}
			}
			else
			{
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shuffle_epi32(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(1, 0, 1, 0)), pix1, pix2, pix3, pix4, fracm;
					tci = _mm_and_si128(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix1 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])),
											_mm_setzero_si128());
					pix2 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])),
											_mm_setzero_si128());
					tci = _mm_shuffle_epi32(_mm_shufflehi_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 2, 3, 2));
					tci = _mm_and_si128(_mm_add_epi16(tci, _mm_setr_epi32(0, 1, 0x10000, 0x10001)), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					pix3 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(tci)]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(1, 1, 1, 1)))])),
											_mm_setzero_si128());
					pix4 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))]),
																_mm_cvtsi32_si128(*(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(3, 3, 3, 3)))])),
											_mm_setzero_si128());
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
					__m128i tci = _mm_shuffle_epi32(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(1, 0, 1, 0)), pix1, pix2, fracm;
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
					x++;
				}
			}
		}
		else
		{
			if (flags & DPSOFTRAST_TEXTURE_FLAG_CLAMPTOEDGE)
			{
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_min_epi16(_mm_max_epi16(tci, _mm_setzero_si128()), tcmax); 
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					outi[x+1] = *(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))];
				}
				if (x <= endsub)
				{
					__m128i tci = _mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1));
					tci =_mm_min_epi16(_mm_max_epi16(tci, _mm_setzero_si128()), tcmax);
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					x++;
				}
			}
			else
			{
				for (; x + 1 <= endsub; x += 2, subtc = _mm_add_epi32(subtc, substep))
				{
					__m128i tci = _mm_shufflehi_epi16(_mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1)), _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_and_si128(tci, tcmax); 
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					outi[x+1] = *(const int *)&pixelbase[_mm_cvtsi128_si32(_mm_shuffle_epi32(tci, _MM_SHUFFLE(2, 2, 2, 2)))];
				}
				if (x <= endsub)
				{
					__m128i tci = _mm_shufflelo_epi16(subtc, _MM_SHUFFLE(3, 1, 3, 1));
					tci = _mm_and_si128(tci, tcmax); 
					tci = _mm_madd_epi16(tci, tcoffset);
					outi[x] = *(const int *)&pixelbase[_mm_cvtsi128_si32(tci)];
					x++;
				}
			}
		}
	}
#endif
}

void DPSOFTRAST_Draw_Span_TextureCubeVaryingBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char * RESTRICT out4ub, int texunitindex, int arrayindex, const float * RESTRICT zf)
{
	// TODO: IMPLEMENT
	memset(out4ub + span->startx*4, 255, (span->startx - span->endx)*4);
}

float DPSOFTRAST_SampleShadowmap(const float *vector)
{
	// TODO: IMPLEMENT
	return 1.0f;
}

void DPSOFTRAST_Draw_Span_MultiplyVarying(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *in4f, int arrayindex, const float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float c[4];
	float data[4];
	float slope[4];
	float z;
	DPSOFTRAST_CALCATTRIB4F(triangle, span, data, slope, arrayindex);
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

void DPSOFTRAST_Draw_Span_Varying(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, int arrayindex, const float *zf)
{
	int x;
	int startx = span->startx;
	int endx = span->endx;
	float c[4];
	float data[4];
	float slope[4];
	float z;
	DPSOFTRAST_CALCATTRIB4F(triangle, span, data, slope, arrayindex);
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

void DPSOFTRAST_Draw_Span_AddBloom(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f, const float *subcolor)
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

void DPSOFTRAST_Draw_Span_MultiplyBuffers(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
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

void DPSOFTRAST_Draw_Span_AddBuffers(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
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

void DPSOFTRAST_Draw_Span_MixBuffers(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *ina4f, const float *inb4f)
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

void DPSOFTRAST_Draw_Span_MixUniformColor(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, float *out4f, const float *in4f, const float *color)
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



void DPSOFTRAST_Draw_Span_MultiplyVaryingBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *in4ub, int arrayindex, const float *zf)
{
#ifdef SSE_POSSIBLE
	int x;
	int startx = span->startx;
	int endx = span->endx;
	__m128 data, slope;
	__m128 mod, endmod;
	__m128i submod, substep, endsubmod;
	DPSOFTRAST_CALCATTRIB(triangle, span, data, slope, arrayindex);
	data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 1, 2));
	slope = _mm_shuffle_ps(slope, slope, _MM_SHUFFLE(3, 0, 1, 2));
	endmod = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx))), _mm_load1_ps(&zf[startx]));
	endsubmod = _mm_cvtps_epi32(_mm_mul_ps(endmod, _mm_set1_ps(256.0f)));
	for (x = startx; x < endx;)
	{
		int nextsub = x + DPSOFTRAST_DRAW_MAXSUBSPAN, endsub = nextsub - 1;
		__m128 subscale = _mm_set1_ps(256.0f/DPSOFTRAST_DRAW_MAXSUBSPAN);
		if (nextsub >= endx)
		{
			nextsub = endsub = endx-1;
			if (x < nextsub) subscale = _mm_set1_ps(256.0f / (nextsub - x));
		}
		mod = endmod;
		submod = endsubmod;
		endmod = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(nextsub))), _mm_load1_ps(&zf[nextsub]));
		substep = _mm_cvtps_epi32(_mm_mul_ps(_mm_sub_ps(endmod, mod), subscale));
		endsubmod = _mm_cvtps_epi32(_mm_mul_ps(endmod, _mm_set1_ps(256.0f)));
		submod = _mm_packs_epi32(submod, _mm_add_epi32(submod, substep));
		substep = _mm_packs_epi32(substep, substep);
		for (; x + 1 <= endsub; x += 2, submod = _mm_add_epi16(submod, substep))
		{
			__m128i pix = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&in4ub[x*4]));
			pix = _mm_mulhi_epu16(pix, submod);
			_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
		}
		if (x <= endsub)
		{
			__m128i pix = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&in4ub[x*4]));
			pix = _mm_mulhi_epu16(pix, submod);
			*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
			x++;
		}
	}
#endif
}

void DPSOFTRAST_Draw_Span_VaryingBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, int arrayindex, const float *zf)
{
#ifdef SSE_POSSIBLE
	int x;
	int startx = span->startx;
	int endx = span->endx;
	__m128 data, slope;
	__m128 mod, endmod;
	__m128i submod, substep, endsubmod;
	DPSOFTRAST_CALCATTRIB(triangle, span, data, slope, arrayindex);
	data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 1, 2));
	slope = _mm_shuffle_ps(slope, slope, _MM_SHUFFLE(3, 0, 1, 2));
	endmod = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx))), _mm_load1_ps(&zf[startx]));
	endsubmod = _mm_cvtps_epi32(_mm_mul_ps(endmod, _mm_set1_ps(4095.0f)));
	for (x = startx; x < endx;)
	{
		int nextsub = x + DPSOFTRAST_DRAW_MAXSUBSPAN, endsub = nextsub - 1;
		__m128 subscale = _mm_set1_ps(4095.0f/DPSOFTRAST_DRAW_MAXSUBSPAN);
		if (nextsub >= endx)
		{
			nextsub = endsub = endx-1;
			if (x < nextsub) subscale = _mm_set1_ps(4095.0f / (nextsub - x));
		}
		mod = endmod;
		submod = endsubmod;
		endmod = _mm_mul_ps(_mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(nextsub))), _mm_load1_ps(&zf[nextsub]));
		substep = _mm_cvtps_epi32(_mm_mul_ps(_mm_sub_ps(endmod, mod), subscale));
		endsubmod = _mm_cvtps_epi32(_mm_mul_ps(endmod, _mm_set1_ps(4095.0f)));
		submod = _mm_packs_epi32(submod, _mm_add_epi32(submod, substep));
		substep = _mm_packs_epi32(substep, substep);
		for (; x + 1 <= endsub; x += 2, submod = _mm_add_epi16(submod, substep))
		{
			__m128i pix = _mm_srai_epi16(submod, 4);
			_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
		}
		if (x <= endsub)
		{
			__m128i pix = _mm_srai_epi16(submod, 4);
			*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
			x++;
		}
	}
#endif
}

void DPSOFTRAST_Draw_Span_AddBloomBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub, const float *subcolor)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	__m128i localcolor = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(subcolor), _mm_set1_ps(255.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	localcolor = _mm_packs_epi32(localcolor, localcolor);
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, _mm_subs_epu16(pix2, localcolor));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if (x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, _mm_subs_epu16(pix2, localcolor));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MultiplyBuffersBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&inb4ub[x*4]));
		pix1 = _mm_mulhi_epu16(pix1, pix2);
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if (x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]));
		pix1 = _mm_mulhi_epu16(pix1, pix2);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_AddBuffersBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, pix2);
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if (x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		pix1 = _mm_add_epi16(pix1, pix2);
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_TintedAddBuffersBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub, const float *inbtintbgra)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	__m128i tint = _mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(inbtintbgra), _mm_set1_ps(256.0f)));
	tint = _mm_packs_epi32(tint, tint);
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_loadl_epi64((const __m128i *)&inb4ub[x*4]));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epu16(tint, pix2));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if (x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epu16(tint, pix2));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MixBuffersBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *ina4ub, const unsigned char *inb4ub)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&inb4ub[x*4]), _mm_setzero_si128());
		__m128i blend = _mm_shufflehi_epi16(_mm_shufflelo_epi16(pix2, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 4), _mm_slli_epi16(blend, 4)));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix1, pix1));
	}
	if (x < endx)
	{
		__m128i pix1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&ina4ub[x*4]), _mm_setzero_si128());
		__m128i pix2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&inb4ub[x*4]), _mm_setzero_si128());
		__m128i blend = _mm_shufflelo_epi16(pix2, _MM_SHUFFLE(3, 3, 3, 3));
		pix1 = _mm_add_epi16(pix1, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(pix2, pix1), 4), _mm_slli_epi16(blend, 4)));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix1, pix1));
	}
#endif
}

void DPSOFTRAST_Draw_Span_MixUniformColorBGRA8(const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span, unsigned char *out4ub, const unsigned char *in4ub, const float *color)
{
#ifdef SSE_POSSIBLE
	int x, startx = span->startx, endx = span->endx;
	__m128i localcolor = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(color), _mm_set1_ps(255.0f))), _MM_SHUFFLE(3, 0, 1, 2)), blend;
	localcolor = _mm_packs_epi32(localcolor, localcolor);
	blend = _mm_slli_epi16(_mm_shufflehi_epi16(_mm_shufflelo_epi16(localcolor, _MM_SHUFFLE(3, 3, 3, 3)), _MM_SHUFFLE(3, 3, 3, 3)), 4);
	for (x = startx;x+2 <= endx;x+=2)
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)&in4ub[x*4]), _mm_setzero_si128());
		pix = _mm_add_epi16(pix, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(localcolor, pix), 4), blend));
		_mm_storel_epi64((__m128i *)&out4ub[x*4], _mm_packus_epi16(pix, pix));
	}
	if (x < endx)
	{
		__m128i pix = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)&in4ub[x*4]), _mm_setzero_si128());
		pix = _mm_add_epi16(pix, _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(localcolor, pix), 4), blend));
		*(int *)&out4ub[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
#endif
}



void DPSOFTRAST_VertexShader_Generic(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_COLOR, DPSOFTRAST_ARRAY_COLOR);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0);
	if (dpsoftrast.shader_permutation & SHADERPERMUTATION_SPECULAR)
		DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD1);
}

void DPSOFTRAST_PixelShader_Generic(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_lightmapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	if (thread->shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_FIRST, 2, buffer_z);
		DPSOFTRAST_Draw_Span_MultiplyVaryingBGRA8(triangle, span, buffer_FragColorbgra8, buffer_texture_colorbgra8, 1, buffer_z);
		if (thread->shader_permutation & SHADERPERMUTATION_SPECULAR)
		{
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_SECOND, 2, buffer_z);
			if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				// multiply
				DPSOFTRAST_Draw_Span_MultiplyBuffersBGRA8(triangle, span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
			else if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				// add
				DPSOFTRAST_Draw_Span_AddBuffersBGRA8(triangle, span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
			else if (thread->shader_permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
			{
				// alphablend
				DPSOFTRAST_Draw_Span_MixBuffersBGRA8(triangle, span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_lightmapbgra8);
			}
		}
	}
	else
		DPSOFTRAST_Draw_Span_VaryingBGRA8(triangle, span, buffer_FragColorbgra8, 1, buffer_z);
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_PostProcess(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD4);
}

void DPSOFTRAST_PixelShader_PostProcess(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	// TODO: optimize!!  at the very least there is no reason to use texture sampling on the frame texture
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_FragColorbgra8, GL20TU_FIRST, 2, buffer_z);
	if (thread->shader_permutation & SHADERPERMUTATION_BLOOM)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_SECOND, 3, buffer_z);
		DPSOFTRAST_Draw_Span_AddBloomBGRA8(triangle, span, buffer_FragColorbgra8, buffer_FragColorbgra8, buffer_texture_colorbgra8, thread->uniform4f + DPSOFTRAST_UNIFORM_BloomColorSubtract * 4);
	}
	DPSOFTRAST_Draw_Span_MixUniformColorBGRA8(triangle, span, buffer_FragColorbgra8, buffer_FragColorbgra8, thread->uniform4f + DPSOFTRAST_UNIFORM_ViewTintColor * 4);
	if (thread->shader_permutation & SHADERPERMUTATION_SATURATION)
	{
		// TODO: implement saturation
	}
	if (thread->shader_permutation & SHADERPERMUTATION_GAMMARAMPS)
	{
		// TODO: implement gammaramps
	}
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Depth_Or_Shadow(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_Depth_Or_Shadow(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	// this is never called (because colormask is off when this shader is used)
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	memset(buffer_FragColorbgra8 + span->startx*4, 0, (span->endx - span->startx)*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_FlatColor(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
}

void DPSOFTRAST_PixelShader_FlatColor(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
#ifdef SSE_POSSIBLE
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0] + (span->y * dpsoftrast.fb_width + span->x) * 4;
	int x, startx = span->startx, endx = span->endx;
	__m128i Color_Ambientm;
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_COLOR, 2, buffer_z);
	if ((thread->shader_permutation & SHADERPERMUTATION_ALPHAKILL) || thread->fb_blendmode != DPSOFTRAST_BLENDMODE_OPAQUE)
		pixel = buffer_FragColorbgra8;
	Color_Ambientm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Ambientm = _mm_and_si128(Color_Ambientm, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Ambientm = _mm_or_si128(Color_Ambientm, _mm_setr_epi32(0, 0, 0, (int)(thread->uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0]*255.0f)));
	Color_Ambientm = _mm_packs_epi32(Color_Ambientm, Color_Ambientm);
	for (x = startx;x < endx;x++)
	{
		__m128i color, pix;
		if (x + 4 <= endx && *(const unsigned int *)&pixelmask[x] == 0x01010101)
		{
			__m128i pix2;
			color = _mm_loadu_si128((const __m128i *)&buffer_texture_colorbgra8[x*4]);
			pix = _mm_mulhi_epu16(Color_Ambientm, _mm_unpacklo_epi8(_mm_setzero_si128(), color));
			pix2 = _mm_mulhi_epu16(Color_Ambientm, _mm_unpackhi_epi8(_mm_setzero_si128(), color));
			_mm_storeu_si128((__m128i *)&pixel[x*4], _mm_packus_epi16(pix, pix2));
			x += 3;
			continue;
		}
		if (!pixelmask[x])
			continue;
		color = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_colorbgra8[x*4]));
		pix = _mm_mulhi_epu16(Color_Ambientm, color);
		*(int *)&pixel[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
	if (pixel == buffer_FragColorbgra8)
		DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
#endif
}



void DPSOFTRAST_VertexShader_VertexColor(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_COLOR, DPSOFTRAST_ARRAY_COLOR);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
}

void DPSOFTRAST_PixelShader_VertexColor(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
#ifdef SSE_POSSIBLE
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0] + (span->y * dpsoftrast.fb_width + span->x) * 4;
	int x, startx = span->startx, endx = span->endx;
	__m128i Color_Ambientm, Color_Diffusem;
	__m128 data, slope;
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	int arrayindex = DPSOFTRAST_ARRAY_COLOR;
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_COLOR, 2, buffer_z);
	if ((thread->shader_permutation & SHADERPERMUTATION_ALPHAKILL) || thread->fb_blendmode != DPSOFTRAST_BLENDMODE_OPAQUE)
		pixel = buffer_FragColorbgra8;
	Color_Ambientm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Ambientm = _mm_and_si128(Color_Ambientm, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Ambientm = _mm_or_si128(Color_Ambientm, _mm_setr_epi32(0, 0, 0, (int)(thread->uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0]*255.0f)));
	Color_Ambientm = _mm_packs_epi32(Color_Ambientm, Color_Ambientm);
	Color_Diffusem = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4]), _mm_set1_ps(4096.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Diffusem = _mm_and_si128(Color_Diffusem, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Diffusem = _mm_packs_epi32(Color_Diffusem, Color_Diffusem);
	DPSOFTRAST_CALCATTRIB(triangle, span, data, slope, arrayindex);
	data = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 1, 2));
	slope = _mm_shuffle_ps(slope, slope, _MM_SHUFFLE(3, 0, 1, 2));
	data = _mm_add_ps(data, _mm_mul_ps(slope, _mm_set1_ps(startx)));
	data = _mm_mul_ps(data, _mm_set1_ps(4096.0f));
	slope = _mm_mul_ps(slope, _mm_set1_ps(4096.0f));
	for (x = startx;x < endx;x++, data = _mm_add_ps(data, slope))
	{
		__m128i color, mod, pix;
		if (x + 4 <= endx && *(const unsigned int *)&pixelmask[x] == 0x01010101)
		{
			__m128i pix2, mod2;
			__m128 z = _mm_loadu_ps(&buffer_z[x]);
			color = _mm_loadu_si128((const __m128i *)&buffer_texture_colorbgra8[x*4]);
			mod = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_shuffle_ps(z, z, _MM_SHUFFLE(0, 0, 0, 0))));
			data = _mm_add_ps(data, slope);
			mod = _mm_packs_epi32(mod, _mm_cvtps_epi32(_mm_mul_ps(data, _mm_shuffle_ps(z, z, _MM_SHUFFLE(1, 1, 1, 1)))));
			data = _mm_add_ps(data, slope);
			mod2 = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_shuffle_ps(z, z, _MM_SHUFFLE(2, 2, 2, 2))));
			data = _mm_add_ps(data, slope);
			mod2 = _mm_packs_epi32(mod2, _mm_cvtps_epi32(_mm_mul_ps(data, _mm_shuffle_ps(z, z, _MM_SHUFFLE(3, 3, 3, 3)))));
			pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, mod), Color_Ambientm),
								  _mm_unpacklo_epi8(_mm_setzero_si128(), color));
			pix2 = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(Color_Diffusem, mod2), Color_Ambientm),
								   _mm_unpackhi_epi8(_mm_setzero_si128(), color));
			_mm_storeu_si128((__m128i *)&pixel[x*4], _mm_packus_epi16(pix, pix2));
			x += 3;
			continue;
		}
		if (!pixelmask[x])
			continue;
		color = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_colorbgra8[x*4]));
		mod = _mm_cvtps_epi32(_mm_mul_ps(data, _mm_load1_ps(&buffer_z[x]))); 
		mod = _mm_packs_epi32(mod, mod);
		pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(mod, Color_Diffusem), Color_Ambientm), color);
		*(int *)&pixel[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
	}
	if (pixel == buffer_FragColorbgra8)
		DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
#endif
}



void DPSOFTRAST_VertexShader_Lightmap(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD4);
}

void DPSOFTRAST_PixelShader_Lightmap(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
#ifdef SSE_POSSIBLE
	unsigned char * RESTRICT pixelmask = span->pixelmask;
	unsigned char * RESTRICT pixel = (unsigned char *)dpsoftrast.fb_colorpixels[0] + (span->y * dpsoftrast.fb_width + span->x) * 4;
	int x, startx = span->startx, endx = span->endx;
	__m128i Color_Ambientm, Color_Diffusem, Color_Glowm, Color_AmbientGlowm;
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_lightmapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glowbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
	if ((thread->shader_permutation & SHADERPERMUTATION_ALPHAKILL) || thread->fb_blendmode != DPSOFTRAST_BLENDMODE_OPAQUE)
		pixel = buffer_FragColorbgra8;
	Color_Ambientm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Ambientm = _mm_and_si128(Color_Ambientm, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Ambientm = _mm_or_si128(Color_Ambientm, _mm_setr_epi32(0, 0, 0, (int)(thread->uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0]*255.0f)));
	Color_Ambientm = _mm_packs_epi32(Color_Ambientm, Color_Ambientm);
	Color_Diffusem = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
	Color_Diffusem = _mm_and_si128(Color_Diffusem, _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
	Color_Diffusem = _mm_packs_epi32(Color_Diffusem, Color_Diffusem);
	if (thread->shader_permutation & SHADERPERMUTATION_GLOW)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_glowbgra8, GL20TU_GLOW, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		Color_Glowm = _mm_shuffle_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_load_ps(&thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4]), _mm_set1_ps(256.0f))), _MM_SHUFFLE(3, 0, 1, 2));
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
			if (!pixelmask[x])
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
			if (!pixelmask[x]) 
				continue;
			color = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_colorbgra8[x*4]));
			lightmap = _mm_unpacklo_epi8(_mm_setzero_si128(), _mm_cvtsi32_si128(*(const int *)&buffer_texture_lightmapbgra8[x*4]));
			pix = _mm_mulhi_epu16(_mm_add_epi16(_mm_mulhi_epu16(lightmap, Color_Diffusem), Color_Ambientm), color);
			*(int *)&pixel[x*4] = _mm_cvtsi128_si32(_mm_packus_epi16(pix, pix));
		}
	}
	if (pixel == buffer_FragColorbgra8)
		DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
#endif
}


void DPSOFTRAST_VertexShader_LightDirection(void);
void DPSOFTRAST_PixelShader_LightDirection(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span);

void DPSOFTRAST_VertexShader_FakeLight(void)
{
	DPSOFTRAST_VertexShader_LightDirection();
}

void DPSOFTRAST_PixelShader_FakeLight(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	DPSOFTRAST_PixelShader_LightDirection(thread, triangle, span);
}



void DPSOFTRAST_VertexShader_LightDirectionMap_ModelSpace(void)
{
	DPSOFTRAST_VertexShader_LightDirection();
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD4);
}

void DPSOFTRAST_PixelShader_LightDirectionMap_ModelSpace(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	DPSOFTRAST_PixelShader_LightDirection(thread, triangle, span);
}



void DPSOFTRAST_VertexShader_LightDirectionMap_TangentSpace(void)
{
	DPSOFTRAST_VertexShader_LightDirection();
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD4);
}

void DPSOFTRAST_PixelShader_LightDirectionMap_TangentSpace(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	DPSOFTRAST_PixelShader_LightDirection(thread, triangle, span);
}



void DPSOFTRAST_VertexShader_LightDirection(void)
{
	int i;
	int numvertices = dpsoftrast.numvertices;
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
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD2);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD3);
	for (i = 0;i < numvertices;i++)
	{
		position[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0];
		position[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1];
		position[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+2];
		svector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0];
		svector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1];
		svector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2];
		tvector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0];
		tvector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1];
		tvector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2];
		normal[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+0];
		normal[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+1];
		normal[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+2];
		LightVector[0] = svector[0] * LightDir[0] + svector[1] * LightDir[1] + svector[2] * LightDir[2];
		LightVector[1] = tvector[0] * LightDir[0] + tvector[1] * LightDir[1] + tvector[2] * LightDir[2];
		LightVector[2] = normal[0] * LightDir[0] + normal[1] * LightDir[1] + normal[2] * LightDir[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD5][i*4+0] = LightVector[0];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD5][i*4+1] = LightVector[1];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD5][i*4+2] = LightVector[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD5][i*4+3] = 0.0f;
		EyeVectorModelSpace[0] = EyePosition[0] - position[0];
		EyeVectorModelSpace[1] = EyePosition[1] - position[1];
		EyeVectorModelSpace[2] = EyePosition[2] - position[2];
		EyeVector[0] = svector[0] * EyeVectorModelSpace[0] + svector[1] * EyeVectorModelSpace[1] + svector[2] * EyeVectorModelSpace[2];
		EyeVector[1] = tvector[0] * EyeVectorModelSpace[0] + tvector[1] * EyeVectorModelSpace[1] + tvector[2] * EyeVectorModelSpace[2];
		EyeVector[2] = normal[0]  * EyeVectorModelSpace[0] + normal[1]  * EyeVectorModelSpace[1] + normal[2]  * EyeVectorModelSpace[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+0] = EyeVector[0];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+1] = EyeVector[1];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+2] = EyeVector[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+3] = 0.0f;
	}
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, -1, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
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

void DPSOFTRAST_PixelShader_LightDirection(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_texture_colorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_normalbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glossbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_glowbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_pantsbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_shirtbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_deluxemapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_texture_lightmapbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	int x, startx = span->startx, endx = span->endx;
	float Color_Ambient[4], Color_Diffuse[4], Color_Specular[4], Color_Glow[4], Color_Pants[4], Color_Shirt[4], LightColor[4];
	float LightVectordata[4];
	float LightVectorslope[4];
	float EyeVectordata[4];
	float EyeVectorslope[4];
	float VectorSdata[4];
	float VectorSslope[4];
	float VectorTdata[4];
	float VectorTslope[4];
	float VectorRdata[4];
	float VectorRslope[4];
	float z;
	float diffusetex[4];
	float glosstex[4];
	float surfacenormal[4];
	float lightnormal[4];
	float lightnormal_modelspace[4];
	float eyenormal[4];
	float specularnormal[4];
	float diffuse;
	float specular;
	float SpecularPower;
	int d[4];
	Color_Glow[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+0];
	Color_Glow[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+1];
	Color_Glow[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+2];
	Color_Glow[3] = 0.0f;
	Color_Ambient[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Pants[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+0];
	Color_Pants[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+1];
	Color_Pants[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+2];
	Color_Pants[3] = 0.0f;
	Color_Shirt[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+0];
	Color_Shirt[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+1];
	Color_Shirt[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+2];
	Color_Shirt[3] = 0.0f;
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_pantsbgra8, GL20TU_PANTS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_shirtbgra8, GL20TU_SHIRT, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (thread->shader_permutation & SHADERPERMUTATION_GLOW)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_glowbgra8, GL20TU_GLOW, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (thread->shader_permutation & SHADERPERMUTATION_SPECULAR)
	{
		Color_Diffuse[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
		Color_Diffuse[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
		Color_Diffuse[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
		Color_Diffuse[3] = 0.0f;
		LightColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
		LightColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
		LightColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
		LightColor[3] = 0.0f;
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		Color_Specular[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+0];
		Color_Specular[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+1];
		Color_Specular[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+2];
		Color_Specular[3] = 0.0f;
		SpecularPower = thread->uniform4f[DPSOFTRAST_UNIFORM_SpecularPower*4+0] * (1.0f / 255.0f);
		DPSOFTRAST_CALCATTRIB4F(triangle, span, EyeVectordata, EyeVectorslope, DPSOFTRAST_ARRAY_TEXCOORD6);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_glossbgra8, GL20TU_GLOSS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);

		if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE)
		{
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorSdata, VectorSslope, DPSOFTRAST_ARRAY_TEXCOORD1);
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorTdata, VectorTslope, DPSOFTRAST_ARRAY_TEXCOORD2);
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorRdata, VectorRslope, DPSOFTRAST_ARRAY_TEXCOORD3);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_deluxemapbgra8, GL20TU_DELUXEMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
		}
		else if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE)
		{
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_deluxemapbgra8, GL20TU_DELUXEMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
		}
		else if(thread->shader_mode == SHADERMODE_FAKELIGHT)
		{
			// nothing of this needed
		}
		else
		{
			DPSOFTRAST_CALCATTRIB4F(triangle, span, LightVectordata, LightVectorslope, DPSOFTRAST_ARRAY_TEXCOORD5);
		}

		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
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

			if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE)
			{
				// myhalf3 lightnormal_modelspace = myhalf3(dp_texture2D(Texture_Deluxemap, TexCoordSurfaceLightmap.zw)) * 2.0 + myhalf3(-1.0, -1.0, -1.0);\n";
				lightnormal_modelspace[0] = buffer_texture_deluxemapbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
				lightnormal_modelspace[1] = buffer_texture_deluxemapbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
				lightnormal_modelspace[2] = buffer_texture_deluxemapbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;

				// lightnormal.x = dot(lightnormal_modelspace, myhalf3(VectorS));\n"
				lightnormal[0] = lightnormal_modelspace[0] * (VectorSdata[0] + VectorSslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorSdata[1] + VectorSslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorSdata[2] + VectorSslope[2] * x);

				// lightnormal.y = dot(lightnormal_modelspace, myhalf3(VectorT));\n"
				lightnormal[1] = lightnormal_modelspace[0] * (VectorTdata[0] + VectorTslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorTdata[1] + VectorTslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorTdata[2] + VectorTslope[2] * x);

				// lightnormal.z = dot(lightnormal_modelspace, myhalf3(VectorR));\n"
				lightnormal[2] = lightnormal_modelspace[0] * (VectorRdata[0] + VectorRslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorRdata[1] + VectorRslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorRdata[2] + VectorRslope[2] * x);

				// lightnormal = normalize(lightnormal); // VectorS/T/R are not always perfectly normalized, and EXACTSPECULARMATH is very picky about this\n"
				DPSOFTRAST_Vector3Normalize(lightnormal);

				// myhalf3 lightcolor = myhalf3(dp_texture2D(Texture_Lightmap, TexCoordSurfaceLightmap.zw));\n";
				{
					float f = 1.0f / (256.0f * max(0.25f, lightnormal[2]));
					LightColor[0] = buffer_texture_lightmapbgra8[x*4+0] * f;
					LightColor[1] = buffer_texture_lightmapbgra8[x*4+1] * f;
					LightColor[2] = buffer_texture_lightmapbgra8[x*4+2] * f;
				}
			}
			else if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE)
			{
				lightnormal[0] = buffer_texture_deluxemapbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
				lightnormal[1] = buffer_texture_deluxemapbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
				lightnormal[2] = buffer_texture_deluxemapbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
				{
					float f = 1.0f / 256.0f;
					LightColor[0] = buffer_texture_lightmapbgra8[x*4+0] * f;
					LightColor[1] = buffer_texture_lightmapbgra8[x*4+1] * f;
					LightColor[2] = buffer_texture_lightmapbgra8[x*4+2] * f;
				}
			}
			else if(thread->shader_mode == SHADERMODE_FAKELIGHT)
			{
				lightnormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				lightnormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				lightnormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(lightnormal);

				LightColor[0] = 1.0;
				LightColor[1] = 1.0;
				LightColor[2] = 1.0;
			}
			else
			{
				lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
				lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
				lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(lightnormal);
			}

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;

			if(thread->shader_exactspecularmath)
			{
				// reflect lightnormal at surfacenormal, take the negative of that
				// i.e. we want (2*dot(N, i) * N - I) for N=surfacenormal, I=lightnormal
				float f;
				f = DPSOFTRAST_Vector3Dot(lightnormal, surfacenormal);
				specularnormal[0] = 2*f*surfacenormal[0] - lightnormal[0];
				specularnormal[1] = 2*f*surfacenormal[1] - lightnormal[1];
				specularnormal[2] = 2*f*surfacenormal[2] - lightnormal[2];

				// dot of this and normalize(EyeVectorFogDepth.xyz)
				eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(eyenormal);

				specular = DPSOFTRAST_Vector3Dot(eyenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			}
			else
			{
				eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(eyenormal);

				specularnormal[0] = lightnormal[0] + eyenormal[0];
				specularnormal[1] = lightnormal[1] + eyenormal[1];
				specularnormal[2] = lightnormal[2] + eyenormal[2];
				DPSOFTRAST_Vector3Normalize(specularnormal);

				specular = DPSOFTRAST_Vector3Dot(surfacenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			}

			specular = pow(specular, SpecularPower * glosstex[3]);
			if (thread->shader_permutation & SHADERPERMUTATION_GLOW)
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
	else if (thread->shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		Color_Diffuse[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
		Color_Diffuse[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
		Color_Diffuse[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
		Color_Diffuse[3] = 0.0f;
		LightColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
		LightColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
		LightColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
		LightColor[3] = 0.0f;
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);

		if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE)
		{
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorSdata, VectorSslope, DPSOFTRAST_ARRAY_TEXCOORD1);
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorTdata, VectorTslope, DPSOFTRAST_ARRAY_TEXCOORD2);
			DPSOFTRAST_CALCATTRIB4F(triangle, span, VectorRdata, VectorRslope, DPSOFTRAST_ARRAY_TEXCOORD3);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_deluxemapbgra8, GL20TU_DELUXEMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
		}
		else if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE)
		{
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_lightmapbgra8, GL20TU_LIGHTMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
			DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_deluxemapbgra8, GL20TU_DELUXEMAP, DPSOFTRAST_ARRAY_TEXCOORD4, buffer_z);
		}
		else if(thread->shader_mode == SHADERMODE_FAKELIGHT)
		{
			DPSOFTRAST_CALCATTRIB4F(triangle, span, EyeVectordata, EyeVectorslope, DPSOFTRAST_ARRAY_TEXCOORD6);
		}
		else
		{
			DPSOFTRAST_CALCATTRIB4F(triangle, span, LightVectordata, LightVectorslope, DPSOFTRAST_ARRAY_TEXCOORD5);
		}

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

			if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE)
			{
				// myhalf3 lightnormal_modelspace = myhalf3(dp_texture2D(Texture_Deluxemap, TexCoordSurfaceLightmap.zw)) * 2.0 + myhalf3(-1.0, -1.0, -1.0);\n";
				lightnormal_modelspace[0] = buffer_texture_deluxemapbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
				lightnormal_modelspace[1] = buffer_texture_deluxemapbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
				lightnormal_modelspace[2] = buffer_texture_deluxemapbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;

				// lightnormal.x = dot(lightnormal_modelspace, myhalf3(VectorS));\n"
				lightnormal[0] = lightnormal_modelspace[0] * (VectorSdata[0] + VectorSslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorSdata[1] + VectorSslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorSdata[2] + VectorSslope[2] * x);

				// lightnormal.y = dot(lightnormal_modelspace, myhalf3(VectorT));\n"
				lightnormal[1] = lightnormal_modelspace[0] * (VectorTdata[0] + VectorTslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorTdata[1] + VectorTslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorTdata[2] + VectorTslope[2] * x);

				// lightnormal.z = dot(lightnormal_modelspace, myhalf3(VectorR));\n"
				lightnormal[2] = lightnormal_modelspace[0] * (VectorRdata[0] + VectorRslope[0] * x)
					       + lightnormal_modelspace[1] * (VectorRdata[1] + VectorRslope[1] * x)
					       + lightnormal_modelspace[2] * (VectorRdata[2] + VectorRslope[2] * x);

				// lightnormal = normalize(lightnormal); // VectorS/T/R are not always perfectly normalized, and EXACTSPECULARMATH is very picky about this\n"
				DPSOFTRAST_Vector3Normalize(lightnormal);

				// myhalf3 lightcolor = myhalf3(dp_texture2D(Texture_Lightmap, TexCoordSurfaceLightmap.zw));\n";
				{
					float f = 1.0f / (256.0f * max(0.25f, lightnormal[2]));
					LightColor[0] = buffer_texture_lightmapbgra8[x*4+0] * f;
					LightColor[1] = buffer_texture_lightmapbgra8[x*4+1] * f;
					LightColor[2] = buffer_texture_lightmapbgra8[x*4+2] * f;
				}
			}
			else if(thread->shader_mode == SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE)
			{
				lightnormal[0] = buffer_texture_deluxemapbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
				lightnormal[1] = buffer_texture_deluxemapbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
				lightnormal[2] = buffer_texture_deluxemapbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
				{
					float f = 1.0f / 256.0f;
					LightColor[0] = buffer_texture_lightmapbgra8[x*4+0] * f;
					LightColor[1] = buffer_texture_lightmapbgra8[x*4+1] * f;
					LightColor[2] = buffer_texture_lightmapbgra8[x*4+2] * f;
				}
			}
			else if(thread->shader_mode == SHADERMODE_FAKELIGHT)
			{
				lightnormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				lightnormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				lightnormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(lightnormal);

				LightColor[0] = 1.0;
				LightColor[1] = 1.0;
				LightColor[2] = 1.0;
			}
			else
			{
				lightnormal[0] = (LightVectordata[0] + LightVectorslope[0]*x) * z;
				lightnormal[1] = (LightVectordata[1] + LightVectorslope[1]*x) * z;
				lightnormal[2] = (LightVectordata[2] + LightVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(lightnormal);
			}

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;
			if (thread->shader_permutation & SHADERPERMUTATION_GLOW)
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

			if (thread->shader_permutation & SHADERPERMUTATION_GLOW)
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
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_LightSource(void)
{
	int i;
	int numvertices = dpsoftrast.numvertices;
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
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD2);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD3);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD4);
	for (i = 0;i < numvertices;i++)
	{
		position[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0];
		position[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1];
		position[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+2];
		svector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0];
		svector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1];
		svector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2];
		tvector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0];
		tvector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1];
		tvector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2];
		normal[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+0];
		normal[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+1];
		normal[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+2];
		LightVectorModelSpace[0] = LightPosition[0] - position[0];
		LightVectorModelSpace[1] = LightPosition[1] - position[1];
		LightVectorModelSpace[2] = LightPosition[2] - position[2];
		LightVector[0] = svector[0] * LightVectorModelSpace[0] + svector[1] * LightVectorModelSpace[1] + svector[2] * LightVectorModelSpace[2];
		LightVector[1] = tvector[0] * LightVectorModelSpace[0] + tvector[1] * LightVectorModelSpace[1] + tvector[2] * LightVectorModelSpace[2];
		LightVector[2] = normal[0]  * LightVectorModelSpace[0] + normal[1]  * LightVectorModelSpace[1] + normal[2]  * LightVectorModelSpace[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0] = LightVector[0];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1] = LightVector[1];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2] = LightVector[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+3] = 0.0f;
		EyeVectorModelSpace[0] = EyePosition[0] - position[0];
		EyeVectorModelSpace[1] = EyePosition[1] - position[1];
		EyeVectorModelSpace[2] = EyePosition[2] - position[2];
		EyeVector[0] = svector[0] * EyeVectorModelSpace[0] + svector[1] * EyeVectorModelSpace[1] + svector[2] * EyeVectorModelSpace[2];
		EyeVector[1] = tvector[0] * EyeVectorModelSpace[0] + tvector[1] * EyeVectorModelSpace[1] + tvector[2] * EyeVectorModelSpace[2];
		EyeVector[2] = normal[0]  * EyeVectorModelSpace[0] + normal[1]  * EyeVectorModelSpace[1] + normal[2]  * EyeVectorModelSpace[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0] = EyeVector[0];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1] = EyeVector[1];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2] = EyeVector[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+3] = 0.0f;
	}
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, -1, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelToLightM1);
}

void DPSOFTRAST_PixelShader_LightSource(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
#ifdef SSE_POSSIBLE
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
	Color_Glow[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+0];
	Color_Glow[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+1];
	Color_Glow[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Glow*4+2];
	Color_Glow[3] = 0.0f;
	Color_Ambient[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+0];
	Color_Ambient[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+1];
	Color_Ambient[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Ambient*4+2];
	Color_Ambient[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_Alpha*4+0];
	Color_Diffuse[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+0];
	Color_Diffuse[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+1];
	Color_Diffuse[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Diffuse*4+2];
	Color_Diffuse[3] = 0.0f;
	Color_Specular[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+0];
	Color_Specular[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+1];
	Color_Specular[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Specular*4+2];
	Color_Specular[3] = 0.0f;
	Color_Pants[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+0];
	Color_Pants[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+1];
	Color_Pants[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Pants*4+2];
	Color_Pants[3] = 0.0f;
	Color_Shirt[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+0];
	Color_Shirt[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+1];
	Color_Shirt[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_Color_Shirt*4+2];
	Color_Shirt[3] = 0.0f;
	LightColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+0];
	LightColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+1];
	LightColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_LightColor*4+2];
	LightColor[3] = 0.0f;
	SpecularPower = thread->uniform4f[DPSOFTRAST_UNIFORM_SpecularPower*4+0] * (1.0f / 255.0f);
	DPSOFTRAST_CALCATTRIB4F(triangle, span, LightVectordata, LightVectorslope, DPSOFTRAST_ARRAY_TEXCOORD1);
	DPSOFTRAST_CALCATTRIB4F(triangle, span, EyeVectordata, EyeVectorslope, DPSOFTRAST_ARRAY_TEXCOORD2);
	DPSOFTRAST_CALCATTRIB4F(triangle, span, CubeVectordata, CubeVectorslope, DPSOFTRAST_ARRAY_TEXCOORD3);
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	memset(buffer_FragColorbgra8 + startx*4, 0, (endx-startx)*4); // clear first, because we skip writing black pixels, and there are a LOT of them...
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_colorbgra8, GL20TU_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_pantsbgra8, GL20TU_PANTS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_shirtbgra8, GL20TU_SHIRT, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
	}
	if (thread->shader_permutation & SHADERPERMUTATION_CUBEFILTER)
		DPSOFTRAST_Draw_Span_TextureCubeVaryingBGRA8(triangle, span, buffer_texture_cubebgra8, GL20TU_CUBE, DPSOFTRAST_ARRAY_TEXCOORD3, buffer_z);
	if (thread->shader_permutation & SHADERPERMUTATION_SPECULAR)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_glossbgra8, GL20TU_GLOSS, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			CubeVector[0] = (CubeVectordata[0] + CubeVectorslope[0]*x) * z;
			CubeVector[1] = (CubeVectordata[1] + CubeVectorslope[1]*x) * z;
			CubeVector[2] = (CubeVectordata[2] + CubeVectorslope[2]*x) * z;
			attenuation = 1.0f - DPSOFTRAST_Vector3LengthSquared(CubeVector);
			if (attenuation < 0.01f)
				continue;
			if (thread->shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
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

			diffuse = DPSOFTRAST_Vector3Dot(surfacenormal, lightnormal);if (diffuse < 0.0f) diffuse = 0.0f;

			if(thread->shader_exactspecularmath)
			{
				// reflect lightnormal at surfacenormal, take the negative of that
				// i.e. we want (2*dot(N, i) * N - I) for N=surfacenormal, I=lightnormal
				float f;
				f = DPSOFTRAST_Vector3Dot(lightnormal, surfacenormal);
				specularnormal[0] = 2*f*surfacenormal[0] - lightnormal[0];
				specularnormal[1] = 2*f*surfacenormal[1] - lightnormal[1];
				specularnormal[2] = 2*f*surfacenormal[2] - lightnormal[2];

				// dot of this and normalize(EyeVectorFogDepth.xyz)
				eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(eyenormal);

				specular = DPSOFTRAST_Vector3Dot(eyenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			}
			else
			{
				eyenormal[0] = (EyeVectordata[0] + EyeVectorslope[0]*x) * z;
				eyenormal[1] = (EyeVectordata[1] + EyeVectorslope[1]*x) * z;
				eyenormal[2] = (EyeVectordata[2] + EyeVectorslope[2]*x) * z;
				DPSOFTRAST_Vector3Normalize(eyenormal);

				specularnormal[0] = lightnormal[0] + eyenormal[0];
				specularnormal[1] = lightnormal[1] + eyenormal[1];
				specularnormal[2] = lightnormal[2] + eyenormal[2];
				DPSOFTRAST_Vector3Normalize(specularnormal);

				specular = DPSOFTRAST_Vector3Dot(surfacenormal, specularnormal);if (specular < 0.0f) specular = 0.0f;
			}
			specular = pow(specular, SpecularPower * glosstex[3]);

			if (thread->shader_permutation & SHADERPERMUTATION_CUBEFILTER)
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
	else if (thread->shader_permutation & SHADERPERMUTATION_DIFFUSE)
	{
		DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);
		for (x = startx;x < endx;x++)
		{
			z = buffer_z[x];
			CubeVector[0] = (CubeVectordata[0] + CubeVectorslope[0]*x) * z;
			CubeVector[1] = (CubeVectordata[1] + CubeVectorslope[1]*x) * z;
			CubeVector[2] = (CubeVectordata[2] + CubeVectorslope[2]*x) * z;
			attenuation = 1.0f - DPSOFTRAST_Vector3LengthSquared(CubeVector);
			if (attenuation < 0.01f)
				continue;
			if (thread->shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
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
			if (thread->shader_permutation & SHADERPERMUTATION_CUBEFILTER)
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
			if (thread->shader_permutation & SHADERPERMUTATION_SHADOWMAP2D)
			{
				attenuation *= DPSOFTRAST_SampleShadowmap(CubeVector);
				if (attenuation < 0.01f)
					continue;
			}

			diffusetex[0] = buffer_texture_colorbgra8[x*4+0];
			diffusetex[1] = buffer_texture_colorbgra8[x*4+1];
			diffusetex[2] = buffer_texture_colorbgra8[x*4+2];
			diffusetex[3] = buffer_texture_colorbgra8[x*4+3];
			if (thread->shader_permutation & SHADERPERMUTATION_COLORMAPPING)
			{
				diffusetex[0] += buffer_texture_pantsbgra8[x*4+0] * Color_Pants[0] + buffer_texture_shirtbgra8[x*4+0] * Color_Shirt[0];
				diffusetex[1] += buffer_texture_pantsbgra8[x*4+1] * Color_Pants[1] + buffer_texture_shirtbgra8[x*4+1] * Color_Shirt[1];
				diffusetex[2] += buffer_texture_pantsbgra8[x*4+2] * Color_Pants[2] + buffer_texture_shirtbgra8[x*4+2] * Color_Shirt[2];
				diffusetex[3] += buffer_texture_pantsbgra8[x*4+3] * Color_Pants[3] + buffer_texture_shirtbgra8[x*4+3] * Color_Shirt[3];
			}
			if (thread->shader_permutation & SHADERPERMUTATION_CUBEFILTER)
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
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
#endif
}



void DPSOFTRAST_VertexShader_Refraction(void)
{
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_Refraction(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	float z;
	int x, startx = span->startx, endx = span->endx;

	// texture reads
	unsigned char buffer_texture_normalbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];

	// varyings
	float ModelViewProjectionPositiondata[4];
	float ModelViewProjectionPositionslope[4];

	// uniforms
	float ScreenScaleRefractReflect[2];
	float ScreenCenterRefractReflect[2];
	float DistortScaleRefractReflect[2];
	float RefractColor[4];

	DPSOFTRAST_Texture *texture = thread->texbound[GL20TU_REFRACTION];
	if(!texture) return;

	// read textures
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);

	// read varyings
	DPSOFTRAST_CALCATTRIB4F(triangle, span, ModelViewProjectionPositiondata, ModelViewProjectionPositionslope, DPSOFTRAST_ARRAY_TEXCOORD4);

	// read uniforms
	ScreenScaleRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+0];
	ScreenScaleRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+1];
	ScreenCenterRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+0];
	ScreenCenterRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+1];
	DistortScaleRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+0];
	DistortScaleRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+1];
	RefractColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+2];
	RefractColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+1];
	RefractColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+0];
	RefractColor[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+3];

	// do stuff
	for (x = startx;x < endx;x++)
	{
		float SafeScreenTexCoord[2];
		float ScreenTexCoord[2];
		float v[3];
		float iw;
		unsigned char c[4];

		z = buffer_z[x];

		// "	vec2 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect.xy * (1.0 / ModelViewProjectionPosition.w);\n"
		iw = 1.0f / (ModelViewProjectionPositiondata[3] + ModelViewProjectionPositionslope[3]*x); // / z

		// "	vec2 SafeScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
		SafeScreenTexCoord[0] = (ModelViewProjectionPositiondata[0] + ModelViewProjectionPositionslope[0]*x) * iw * ScreenScaleRefractReflect[0] + ScreenCenterRefractReflect[0]; // * z (disappears)
		SafeScreenTexCoord[1] = (ModelViewProjectionPositiondata[1] + ModelViewProjectionPositionslope[1]*x) * iw * ScreenScaleRefractReflect[1] + ScreenCenterRefractReflect[1]; // * z (disappears)

		// "	vec2 ScreenTexCoord = SafeScreenTexCoord + vec3(normalize(myhalf3(dp_texture2D(Texture_Normal, TexCoord)) - myhalf3(0.5))).xy * DistortScaleRefractReflect.zw;\n"
		v[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
		v[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
		v[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
		DPSOFTRAST_Vector3Normalize(v);
		ScreenTexCoord[0] = SafeScreenTexCoord[0] + v[0] * DistortScaleRefractReflect[0];
		ScreenTexCoord[1] = SafeScreenTexCoord[1] + v[1] * DistortScaleRefractReflect[1];

		// "	dp_FragColor = vec4(dp_texture2D(Texture_Refraction, ScreenTexCoord).rgb, 1.0) * RefractColor;\n"
		DPSOFTRAST_Texture2DBGRA8(texture, 0, ScreenTexCoord[0], ScreenTexCoord[1], c);

		buffer_FragColorbgra8[x*4+0] = c[0] * RefractColor[0];
		buffer_FragColorbgra8[x*4+1] = c[1] * RefractColor[1];
		buffer_FragColorbgra8[x*4+2] = c[2] * RefractColor[2];
		buffer_FragColorbgra8[x*4+3] = min(RefractColor[3] * 256, 255);
	}

	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_Water(void)
{
	int i;
	int numvertices = dpsoftrast.numvertices;
	float EyePosition[4];
	float EyeVectorModelSpace[4];
	float EyeVector[4];
	float position[4];
	float svector[4];
	float tvector[4];
	float normal[4];
	EyePosition[0] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+0];
	EyePosition[1] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+1];
	EyePosition[2] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+2];
	EyePosition[3] = dpsoftrast.uniform4f[DPSOFTRAST_UNIFORM_EyePosition*4+3];
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD1);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD2);
	DPSOFTRAST_Array_Load(DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD3);
	for (i = 0;i < numvertices;i++)
	{
		position[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+0];
		position[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+1];
		position[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION][i*4+2];
		svector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+0];
		svector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+1];
		svector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD1][i*4+2];
		tvector[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+0];
		tvector[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+1];
		tvector[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD2][i*4+2];
		normal[0] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+0];
		normal[1] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+1];
		normal[2] = dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD3][i*4+2];
		EyeVectorModelSpace[0] = EyePosition[0] - position[0];
		EyeVectorModelSpace[1] = EyePosition[1] - position[1];
		EyeVectorModelSpace[2] = EyePosition[2] - position[2];
		EyeVector[0] = svector[0] * EyeVectorModelSpace[0] + svector[1] * EyeVectorModelSpace[1] + svector[2] * EyeVectorModelSpace[2];
		EyeVector[1] = tvector[0] * EyeVectorModelSpace[0] + tvector[1] * EyeVectorModelSpace[1] + tvector[2] * EyeVectorModelSpace[2];
		EyeVector[2] = normal[0]  * EyeVectorModelSpace[0] + normal[1]  * EyeVectorModelSpace[1] + normal[2]  * EyeVectorModelSpace[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+0] = EyeVector[0];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+1] = EyeVector[1];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+2] = EyeVector[2];
		dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_TEXCOORD6][i*4+3] = 0.0f;
	}
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, -1, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
	DPSOFTRAST_Array_Transform(DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD0, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_TexMatrixM1);
}


void DPSOFTRAST_PixelShader_Water(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	float z;
	int x, startx = span->startx, endx = span->endx;

	// texture reads
	unsigned char buffer_texture_normalbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];

	// varyings
	float ModelViewProjectionPositiondata[4];
	float ModelViewProjectionPositionslope[4];
	float EyeVectordata[4];
	float EyeVectorslope[4];

	// uniforms
	float ScreenScaleRefractReflect[4];
	float ScreenCenterRefractReflect[4];
	float DistortScaleRefractReflect[4];
	float RefractColor[4];
	float ReflectColor[4];
	float ReflectFactor;
	float ReflectOffset;

	DPSOFTRAST_Texture *texture_refraction = thread->texbound[GL20TU_REFRACTION];
	DPSOFTRAST_Texture *texture_reflection = thread->texbound[GL20TU_REFLECTION];
	if(!texture_refraction || !texture_reflection) return;

	// read textures
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	DPSOFTRAST_Draw_Span_Texture2DVaryingBGRA8(thread, triangle, span, buffer_texture_normalbgra8, GL20TU_NORMAL, DPSOFTRAST_ARRAY_TEXCOORD0, buffer_z);

	// read varyings
	DPSOFTRAST_CALCATTRIB4F(triangle, span, ModelViewProjectionPositiondata, ModelViewProjectionPositionslope, DPSOFTRAST_ARRAY_TEXCOORD4);
	DPSOFTRAST_CALCATTRIB4F(triangle, span, EyeVectordata, EyeVectorslope, DPSOFTRAST_ARRAY_TEXCOORD6);

	// read uniforms
	ScreenScaleRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+0];
	ScreenScaleRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+1];
	ScreenScaleRefractReflect[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+2];
	ScreenScaleRefractReflect[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect*4+3];
	ScreenCenterRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+0];
	ScreenCenterRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+1];
	ScreenCenterRefractReflect[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+2];
	ScreenCenterRefractReflect[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect*4+3];
	DistortScaleRefractReflect[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+0];
	DistortScaleRefractReflect[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+1];
	DistortScaleRefractReflect[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+2];
	DistortScaleRefractReflect[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_DistortScaleRefractReflect*4+3];
	RefractColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+2];
	RefractColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+1];
	RefractColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+0];
	RefractColor[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_RefractColor*4+3];
	ReflectColor[0] = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectColor*4+2];
	ReflectColor[1] = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectColor*4+1];
	ReflectColor[2] = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectColor*4+0];
	ReflectColor[3] = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectColor*4+3];
	ReflectFactor = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectFactor*4+0];
	ReflectOffset = thread->uniform4f[DPSOFTRAST_UNIFORM_ReflectOffset*4+0];

	// do stuff
	for (x = startx;x < endx;x++)
	{
		float SafeScreenTexCoord[4];
		float ScreenTexCoord[4];
		float v[3];
		float iw;
		unsigned char c1[4];
		unsigned char c2[4];
		float Fresnel;

		z = buffer_z[x];

		// "    vec4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
		iw = 1.0f / (ModelViewProjectionPositiondata[3] + ModelViewProjectionPositionslope[3]*x); // / z

		// "    vec4 SafeScreenTexCoord = ModelViewProjectionPosition.xyxy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
		SafeScreenTexCoord[0] = (ModelViewProjectionPositiondata[0] + ModelViewProjectionPositionslope[0]*x) * iw * ScreenScaleRefractReflect[0] + ScreenCenterRefractReflect[0]; // * z (disappears)
		SafeScreenTexCoord[1] = (ModelViewProjectionPositiondata[1] + ModelViewProjectionPositionslope[1]*x) * iw * ScreenScaleRefractReflect[1] + ScreenCenterRefractReflect[1]; // * z (disappears)
		SafeScreenTexCoord[2] = (ModelViewProjectionPositiondata[0] + ModelViewProjectionPositionslope[0]*x) * iw * ScreenScaleRefractReflect[2] + ScreenCenterRefractReflect[2]; // * z (disappears)
		SafeScreenTexCoord[3] = (ModelViewProjectionPositiondata[1] + ModelViewProjectionPositionslope[1]*x) * iw * ScreenScaleRefractReflect[3] + ScreenCenterRefractReflect[3]; // * z (disappears)

		// "    vec4 ScreenTexCoord = SafeScreenTexCoord + vec2(normalize(vec3(dp_texture2D(Texture_Normal, TexCoord)) - vec3(0.5))).xyxy * DistortScaleRefractReflect;\n"
		v[0] = buffer_texture_normalbgra8[x*4+2] * (1.0f / 128.0f) - 1.0f;
		v[1] = buffer_texture_normalbgra8[x*4+1] * (1.0f / 128.0f) - 1.0f;
		v[2] = buffer_texture_normalbgra8[x*4+0] * (1.0f / 128.0f) - 1.0f;
		DPSOFTRAST_Vector3Normalize(v);
		ScreenTexCoord[0] = SafeScreenTexCoord[0] + v[0] * DistortScaleRefractReflect[0];
		ScreenTexCoord[1] = SafeScreenTexCoord[1] + v[1] * DistortScaleRefractReflect[1];
		ScreenTexCoord[2] = SafeScreenTexCoord[2] + v[0] * DistortScaleRefractReflect[2];
		ScreenTexCoord[3] = SafeScreenTexCoord[3] + v[1] * DistortScaleRefractReflect[3];

		// "    float Fresnel = pow(min(1.0, 1.0 - float(normalize(EyeVector).z)), 2.0) * ReflectFactor + ReflectOffset;\n"
		v[0] = (EyeVectordata[0] + EyeVectorslope[0] * x); // * z (disappears)
		v[1] = (EyeVectordata[1] + EyeVectorslope[1] * x); // * z (disappears)
		v[2] = (EyeVectordata[2] + EyeVectorslope[2] * x); // * z (disappears)
		DPSOFTRAST_Vector3Normalize(v);
		Fresnel = 1.0f - v[2];
		Fresnel = min(1.0f, Fresnel);
		Fresnel = Fresnel * Fresnel * ReflectFactor + ReflectOffset;

		// "	dp_FragColor = vec4(dp_texture2D(Texture_Refraction, ScreenTexCoord).rgb, 1.0) * RefractColor;\n"
		// "    dp_FragColor = mix(vec4(dp_texture2D(Texture_Refraction, ScreenTexCoord.xy).rgb, 1) * RefractColor, vec4(dp_texture2D(Texture_Reflection, ScreenTexCoord.zw).rgb, 1) * ReflectColor, Fresnel);\n"
		DPSOFTRAST_Texture2DBGRA8(texture_refraction, 0, ScreenTexCoord[0], ScreenTexCoord[1], c1);
		DPSOFTRAST_Texture2DBGRA8(texture_reflection, 0, ScreenTexCoord[2], ScreenTexCoord[3], c2);

		buffer_FragColorbgra8[x*4+0] = (c1[0] * RefractColor[0]) * (1.0f - Fresnel) + (c2[0] * ReflectColor[0]) * Fresnel;
		buffer_FragColorbgra8[x*4+1] = (c1[1] * RefractColor[1]) * (1.0f - Fresnel) + (c2[1] * ReflectColor[1]) * Fresnel;
		buffer_FragColorbgra8[x*4+2] = (c1[2] * RefractColor[2]) * (1.0f - Fresnel) + (c2[2] * ReflectColor[2]) * Fresnel;
		buffer_FragColorbgra8[x*4+3] = min((    RefractColor[3] *  (1.0f - Fresnel) +          ReflectColor[3]  * Fresnel) * 256, 255);
	}

	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_ShowDepth(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_ShowDepth(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	memset(buffer_FragColorbgra8 + span->startx*4, 0, (span->endx - span->startx)*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_DeferredGeometry(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_DeferredGeometry(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	memset(buffer_FragColorbgra8 + span->startx*4, 0, (span->endx - span->startx)*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



void DPSOFTRAST_VertexShader_DeferredLightSource(void)
{
	DPSOFTRAST_Array_TransformProject(DPSOFTRAST_ARRAY_POSITION, DPSOFTRAST_ARRAY_POSITION, dpsoftrast.uniform4f + 4*DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1);
}

void DPSOFTRAST_PixelShader_DeferredLightSource(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span)
{
	// TODO: IMPLEMENT
	float buffer_z[DPSOFTRAST_DRAW_MAXSPANLENGTH];
	unsigned char buffer_FragColorbgra8[DPSOFTRAST_DRAW_MAXSPANLENGTH*4];
	DPSOFTRAST_Draw_Span_Begin(thread, triangle, span, buffer_z);
	memset(buffer_FragColorbgra8 + span->startx*4, 0, (span->endx - span->startx)*4);
	DPSOFTRAST_Draw_Span_FinishBGRA8(thread, triangle, span, buffer_FragColorbgra8);
}



typedef struct DPSOFTRAST_ShaderModeInfo_s
{
	int lodarrayindex;
	void (*Vertex)(void);
	void (*Span)(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Triangle * RESTRICT triangle, const DPSOFTRAST_State_Span * RESTRICT span);
	unsigned char arrays[DPSOFTRAST_ARRAY_TOTAL];
	unsigned char texunits[DPSOFTRAST_MAXTEXTUREUNITS];
}
DPSOFTRAST_ShaderModeInfo;

static const DPSOFTRAST_ShaderModeInfo DPSOFTRAST_ShaderModeTable[SHADERMODE_COUNT] =
{
	{2, DPSOFTRAST_VertexShader_Generic,                        DPSOFTRAST_PixelShader_Generic,                        {DPSOFTRAST_ARRAY_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, ~0}, {GL20TU_FIRST, GL20TU_SECOND, ~0}},
	{2, DPSOFTRAST_VertexShader_PostProcess,                    DPSOFTRAST_PixelShader_PostProcess,                    {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, ~0}, {GL20TU_FIRST, GL20TU_SECOND, ~0}},
	{2, DPSOFTRAST_VertexShader_Depth_Or_Shadow,                DPSOFTRAST_PixelShader_Depth_Or_Shadow,                {~0}, {~0}},
	{2, DPSOFTRAST_VertexShader_FlatColor,                      DPSOFTRAST_PixelShader_FlatColor,                      {DPSOFTRAST_ARRAY_TEXCOORD0, ~0}, {GL20TU_COLOR, ~0}},
	{2, DPSOFTRAST_VertexShader_VertexColor,                    DPSOFTRAST_PixelShader_VertexColor,                    {DPSOFTRAST_ARRAY_COLOR, DPSOFTRAST_ARRAY_TEXCOORD0, ~0}, {GL20TU_COLOR, ~0}},
	{2, DPSOFTRAST_VertexShader_Lightmap,                       DPSOFTRAST_PixelShader_Lightmap,                       {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD4, ~0}, {GL20TU_COLOR, GL20TU_LIGHTMAP, GL20TU_GLOW, ~0}},
	{2, DPSOFTRAST_VertexShader_FakeLight,                      DPSOFTRAST_PixelShader_FakeLight,                      {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD5, DPSOFTRAST_ARRAY_TEXCOORD6, ~0}, {GL20TU_COLOR, GL20TU_PANTS, GL20TU_SHIRT, GL20TU_GLOW, GL20TU_NORMAL, GL20TU_GLOSS, ~0}},
	{2, DPSOFTRAST_VertexShader_LightDirectionMap_ModelSpace,   DPSOFTRAST_PixelShader_LightDirectionMap_ModelSpace,   {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD5, DPSOFTRAST_ARRAY_TEXCOORD6, ~0}, {GL20TU_COLOR, GL20TU_PANTS, GL20TU_SHIRT, GL20TU_GLOW, GL20TU_NORMAL, GL20TU_GLOSS, GL20TU_LIGHTMAP, GL20TU_DELUXEMAP, ~0}},
	{2, DPSOFTRAST_VertexShader_LightDirectionMap_TangentSpace, DPSOFTRAST_PixelShader_LightDirectionMap_TangentSpace, {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD5, DPSOFTRAST_ARRAY_TEXCOORD6, ~0}, {GL20TU_COLOR, GL20TU_PANTS, GL20TU_SHIRT, GL20TU_GLOW, GL20TU_NORMAL, GL20TU_GLOSS, GL20TU_LIGHTMAP, GL20TU_DELUXEMAP, ~0}},
	{2, DPSOFTRAST_VertexShader_LightDirection,                 DPSOFTRAST_PixelShader_LightDirection,                 {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD5, DPSOFTRAST_ARRAY_TEXCOORD6, ~0}, {GL20TU_COLOR, GL20TU_PANTS, GL20TU_SHIRT, GL20TU_GLOW, GL20TU_NORMAL, GL20TU_GLOSS, ~0}},
	{2, DPSOFTRAST_VertexShader_LightSource,                    DPSOFTRAST_PixelShader_LightSource,                    {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD4, ~0}, {GL20TU_COLOR, GL20TU_PANTS, GL20TU_SHIRT, GL20TU_GLOW, GL20TU_NORMAL, GL20TU_GLOSS, GL20TU_CUBE, ~0}},
	{2, DPSOFTRAST_VertexShader_Refraction,                     DPSOFTRAST_PixelShader_Refraction,                     {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD4, ~0}, {GL20TU_NORMAL, GL20TU_REFRACTION, ~0}},
	{2, DPSOFTRAST_VertexShader_Water,                          DPSOFTRAST_PixelShader_Water,                          {DPSOFTRAST_ARRAY_TEXCOORD0, DPSOFTRAST_ARRAY_TEXCOORD1, DPSOFTRAST_ARRAY_TEXCOORD2, DPSOFTRAST_ARRAY_TEXCOORD3, DPSOFTRAST_ARRAY_TEXCOORD4, DPSOFTRAST_ARRAY_TEXCOORD6, ~0}, {GL20TU_NORMAL, GL20TU_REFLECTION, GL20TU_REFRACTION, ~0}},
	{2, DPSOFTRAST_VertexShader_ShowDepth,                      DPSOFTRAST_PixelShader_ShowDepth,                      {~0}},
	{2, DPSOFTRAST_VertexShader_DeferredGeometry,               DPSOFTRAST_PixelShader_DeferredGeometry,               {~0}},
	{2, DPSOFTRAST_VertexShader_DeferredLightSource,            DPSOFTRAST_PixelShader_DeferredLightSource,            {~0}},
};

static void DPSOFTRAST_Draw_DepthTest(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_State_Span *span)
{
	int x;
	int startx;
	int endx;
	unsigned int *depthpixel;
	int depth;
	int depthslope;
	unsigned int d;
	unsigned char *pixelmask;
	DPSOFTRAST_State_Triangle *triangle;
	triangle = &thread->triangles[span->triangle];
	depthpixel = dpsoftrast.fb_depthpixels + span->y * dpsoftrast.fb_width + span->x;
	startx = span->startx;
	endx = span->endx;
	depth = span->depthbase;
	depthslope = span->depthslope;
	pixelmask = thread->pixelmaskarray;
	if (thread->depthtest && dpsoftrast.fb_depthpixels)
	{
		switch(thread->fb_depthfunc)
		{
		default:
		case GL_ALWAYS:  for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = true; break;
		case GL_LESS:    for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = depthpixel[x] < d; break;
		case GL_LEQUAL:  for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = depthpixel[x] <= d; break;
		case GL_EQUAL:   for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = depthpixel[x] == d; break;
		case GL_GEQUAL:  for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = depthpixel[x] >= d; break;
		case GL_GREATER: for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = depthpixel[x] > d; break;
		case GL_NEVER:   for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope) pixelmask[x] = false; break;
		}
		while (startx < endx && !pixelmask[startx])
			startx++;
		while (endx > startx && !pixelmask[endx-1])
			endx--;
	}
	else
	{
		// no depth testing means we're just dealing with color...
		memset(pixelmask + startx, 1, endx - startx);
	}
	span->pixelmask = pixelmask;
	span->startx = startx;
	span->endx = endx;
}

static void DPSOFTRAST_Draw_DepthWrite(const DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_State_Span *span)
{
	int x, d, depth, depthslope, startx, endx;
	const unsigned char *pixelmask;
	unsigned int *depthpixel;
	if (thread->depthmask && thread->depthtest && dpsoftrast.fb_depthpixels)
	{
		depth = span->depthbase;
		depthslope = span->depthslope;
		pixelmask = span->pixelmask;
		startx = span->startx;
		endx = span->endx;
		depthpixel = dpsoftrast.fb_depthpixels + span->y * dpsoftrast.fb_width + span->x;
		for (x = startx, d = depth + depthslope*startx;x < endx;x++, d += depthslope)
			if (pixelmask[x])
				depthpixel[x] = d;
	}
}

void DPSOFTRAST_Draw_ProcessSpans(DPSOFTRAST_State_Thread *thread)
{
	int i;
	DPSOFTRAST_State_Triangle *triangle;
	DPSOFTRAST_State_Span *span;
	for (i = 0; i < thread->numspans; i++)
	{
		span = &thread->spans[i];
		triangle = &thread->triangles[span->triangle];
		DPSOFTRAST_Draw_DepthTest(thread, span);
		if (span->startx >= span->endx)
			continue;
		// run pixel shader if appropriate
		// do this before running depthmask code, to allow the pixelshader
		// to clear pixelmask values for alpha testing
		if (dpsoftrast.fb_colorpixels[0] && thread->fb_colormask)
			DPSOFTRAST_ShaderModeTable[thread->shader_mode].Span(thread, triangle, span);
		DPSOFTRAST_Draw_DepthWrite(thread, span);
	}
	thread->numspans = 0;
}

DEFCOMMAND(22, Draw, int datasize; int starty; int endy; ATOMIC_COUNTER refcount; int clipped; int firstvertex; int numvertices; int numtriangles; float *arrays; int *element3i; unsigned short *element3s;);

static void DPSOFTRAST_Interpret_Draw(DPSOFTRAST_State_Thread *thread, DPSOFTRAST_Command_Draw *command)
{
#ifdef SSE_POSSIBLE
	int cullface = thread->cullface;
	int minx, maxx, miny, maxy;
	int miny1, maxy1, miny2, maxy2;
	__m128i fbmin, fbmax;
	__m128 viewportcenter, viewportscale;
	int firstvertex = command->firstvertex;
	int numvertices = command->numvertices;
	int numtriangles = command->numtriangles;
	const int *element3i = command->element3i;
	const unsigned short *element3s = command->element3s;
	int clipped = command->clipped;
	int i;
	int j;
	int k;
	int y;
	int e[3];
	__m128i screeny;
	int starty, endy, bandy;
	int numpoints;
	int clipcase;
	float clipdist[4];
	float clip0origin, clip0slope;
	int clip0dir;
	__m128 triangleedge1, triangleedge2, trianglenormal;
	__m128 clipfrac[3];
	__m128 screen[4];
	DPSOFTRAST_State_Triangle *triangle;
	DPSOFTRAST_Texture *texture;
	DPSOFTRAST_ValidateQuick(thread, DPSOFTRAST_VALIDATE_DRAW);
	miny = thread->fb_scissor[1];
	maxy = thread->fb_scissor[1] + thread->fb_scissor[3];
	miny1 = bound(miny, thread->miny1, maxy);
	maxy1 = bound(miny, thread->maxy1, maxy);
	miny2 = bound(miny, thread->miny2, maxy);
	maxy2 = bound(miny, thread->maxy2, maxy);
	if ((command->starty >= maxy1 || command->endy <= miny1) && (command->starty >= maxy2 || command->endy <= miny2))
	{
		if (!ATOMIC_DECREMENT(command->refcount))
		{
			if (command->commandsize <= DPSOFTRAST_ALIGNCOMMAND(sizeof(DPSOFTRAST_Command_Draw)))
				MM_FREE(command->arrays);
		}
		return;
	}
	minx = thread->fb_scissor[0];
	maxx = thread->fb_scissor[0] + thread->fb_scissor[2];
	fbmin = _mm_setr_epi16(minx, miny1, minx, miny1, minx, miny1, minx, miny1);
	fbmax = _mm_sub_epi16(_mm_setr_epi16(maxx, maxy2, maxx, maxy2, maxx, maxy2, maxx, maxy2), _mm_set1_epi16(1));
	viewportcenter = _mm_load_ps(thread->fb_viewportcenter);
	viewportscale = _mm_load_ps(thread->fb_viewportscale);
	screen[3] = _mm_setzero_ps();
	clipfrac[0] = clipfrac[1] = clipfrac[2] = _mm_setzero_ps();
	for (i = 0;i < numtriangles;i++)
	{
		const float *screencoord4f = command->arrays;
		const float *arrays = screencoord4f + numvertices*4;

		// generate the 3 edges of this triangle
		// generate spans for the triangle - switch based on left split or right split classification of triangle
		if (element3s)
		{
			e[0] = element3s[i*3+0] - firstvertex;
			e[1] = element3s[i*3+1] - firstvertex;
			e[2] = element3s[i*3+2] - firstvertex;
		}
		else if (element3i)
		{
			e[0] = element3i[i*3+0] - firstvertex;
			e[1] = element3i[i*3+1] - firstvertex;
			e[2] = element3i[i*3+2] - firstvertex;
		}
		else
		{
			e[0] = i*3+0;
			e[1] = i*3+1;
			e[2] = i*3+2;
		}

#define SKIPBACKFACE \
		triangleedge1 = _mm_sub_ps(screen[0], screen[1]); \
		triangleedge2 = _mm_sub_ps(screen[2], screen[1]); \
		/* store normal in 2, 0, 1 order instead of 0, 1, 2 as it requires fewer shuffles and leaves z component accessible as scalar */ \
		trianglenormal = _mm_sub_ss(_mm_mul_ss(triangleedge1, _mm_shuffle_ps(triangleedge2, triangleedge2, _MM_SHUFFLE(3, 0, 2, 1))), \
									_mm_mul_ss(_mm_shuffle_ps(triangleedge1, triangleedge1, _MM_SHUFFLE(3, 0, 2, 1)), triangleedge2)); \
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
		}

#define CLIPPEDVERTEXLERP(k,p1, p2) \
			clipfrac[p1] = _mm_set1_ps(clipdist[p1] / (clipdist[p1] - clipdist[p2])); \
			{ \
				__m128 v1 = _mm_load_ps(&arrays[e[p1]*4]), v2 = _mm_load_ps(&arrays[e[p2]*4]); \
				DPSOFTRAST_PROJECTVERTEX(screen[k], _mm_add_ps(v1, _mm_mul_ps(_mm_sub_ps(v2, v1), clipfrac[p1])), viewportcenter, viewportscale); \
			}
#define CLIPPEDVERTEXCOPY(k,p1) \
			screen[k] = _mm_load_ps(&screencoord4f[e[p1]*4]);

#define GENATTRIBCOPY(attrib, p1) \
		attrib = _mm_load_ps(&arrays[e[p1]*4]);
#define GENATTRIBLERP(attrib, p1, p2) \
		{ \
			__m128 v1 = _mm_load_ps(&arrays[e[p1]*4]), v2 = _mm_load_ps(&arrays[e[p2]*4]); \
			attrib = _mm_add_ps(v1, _mm_mul_ps(_mm_sub_ps(v2, v1), clipfrac[p1])); \
		}
#define GENATTRIBS(attrib0, attrib1, attrib2) \
		switch(clipcase) \
		{ \
		default: \
		case 0: GENATTRIBCOPY(attrib0, 0); GENATTRIBCOPY(attrib1, 1); GENATTRIBCOPY(attrib2, 2); break; \
		case 1: GENATTRIBCOPY(attrib0, 0); GENATTRIBCOPY(attrib1, 1); GENATTRIBLERP(attrib2, 1, 2); break; \
		case 2: GENATTRIBCOPY(attrib0, 0); GENATTRIBLERP(attrib1, 0, 1); GENATTRIBLERP(attrib2, 1, 2); break; \
		case 3: GENATTRIBCOPY(attrib0, 0); GENATTRIBLERP(attrib1, 0, 1); GENATTRIBLERP(attrib2, 2, 0); break; \
		case 4: GENATTRIBLERP(attrib0, 0, 1); GENATTRIBCOPY(attrib1, 1); GENATTRIBCOPY(attrib2, 2); break; \
		case 5: GENATTRIBLERP(attrib0, 0, 1); GENATTRIBCOPY(attrib1, 1); GENATTRIBLERP(attrib2, 1, 2); break; \
		case 6: GENATTRIBLERP(attrib0, 1, 2); GENATTRIBCOPY(attrib1, 2); GENATTRIBLERP(attrib2, 2, 0); break; \
		}

		if (! clipped)
			goto notclipped;

		// calculate distance from nearplane
		clipdist[0] = arrays[e[0]*4+2] + arrays[e[0]*4+3];
		clipdist[1] = arrays[e[1]*4+2] + arrays[e[1]*4+3];
		clipdist[2] = arrays[e[2]*4+2] + arrays[e[2]*4+3];
		if (clipdist[0] >= 0.0f)
		{
			if (clipdist[1] >= 0.0f)
			{
				if (clipdist[2] >= 0.0f)
				{
				notclipped:
					// triangle is entirely in front of nearplane
					CLIPPEDVERTEXCOPY(0,0); CLIPPEDVERTEXCOPY(1,1); CLIPPEDVERTEXCOPY(2,2);
					SKIPBACKFACE;
					numpoints = 3;
					clipcase = 0;
				}
				else
				{
					CLIPPEDVERTEXCOPY(0,0); CLIPPEDVERTEXCOPY(1,1); CLIPPEDVERTEXLERP(2,1,2); CLIPPEDVERTEXLERP(3,2,0);
					SKIPBACKFACE;
					numpoints = 4;
					clipcase = 1;
				}
			}
			else
			{
				if (clipdist[2] >= 0.0f)
				{
					CLIPPEDVERTEXCOPY(0,0); CLIPPEDVERTEXLERP(1,0,1); CLIPPEDVERTEXLERP(2,1,2); CLIPPEDVERTEXCOPY(3,2);
					SKIPBACKFACE;
					numpoints = 4;
					clipcase = 2;
				}
				else
				{
					CLIPPEDVERTEXCOPY(0,0); CLIPPEDVERTEXLERP(1,0,1); CLIPPEDVERTEXLERP(2,2,0);
					SKIPBACKFACE;
					numpoints = 3;
					clipcase = 3;
				}
			}
		}
		else if (clipdist[1] >= 0.0f)
		{
			if (clipdist[2] >= 0.0f)
			{
				CLIPPEDVERTEXLERP(0,0,1); CLIPPEDVERTEXCOPY(1,1); CLIPPEDVERTEXCOPY(2,2); CLIPPEDVERTEXLERP(3,2,0);
				SKIPBACKFACE;
				numpoints = 4;
				clipcase = 4;
			}
			else
			{
				CLIPPEDVERTEXLERP(0,0,1); CLIPPEDVERTEXCOPY(1,1); CLIPPEDVERTEXLERP(2,1,2);
				SKIPBACKFACE;
				numpoints = 3;
				clipcase = 5;
			}
		}
		else if (clipdist[2] >= 0.0f)
		{
			CLIPPEDVERTEXLERP(0,1,2); CLIPPEDVERTEXCOPY(1,2); CLIPPEDVERTEXLERP(2,2,0);
			SKIPBACKFACE;
			numpoints = 3;
			clipcase = 6;
		}
		else continue; // triangle is entirely behind nearplane

		{
			// calculate integer y coords for triangle points
			__m128i screeni = _mm_packs_epi32(_mm_cvttps_epi32(_mm_movelh_ps(screen[0], screen[1])), _mm_cvttps_epi32(_mm_movelh_ps(screen[2], numpoints > 3 ? screen[3] : screen[2]))),
					screenir = _mm_shuffle_epi32(screeni, _MM_SHUFFLE(1, 0, 3, 2)),
					screenmin = _mm_min_epi16(screeni, screenir),
					screenmax = _mm_max_epi16(screeni, screenir);
			screenmin = _mm_min_epi16(screenmin, _mm_shufflelo_epi16(screenmin, _MM_SHUFFLE(1, 0, 3, 2)));
			screenmax = _mm_max_epi16(screenmax, _mm_shufflelo_epi16(screenmax, _MM_SHUFFLE(1, 0, 3, 2)));
			screenmin = _mm_max_epi16(screenmin, fbmin);
			screenmax = _mm_min_epi16(screenmax, fbmax);
			// skip offscreen triangles
			if (_mm_cvtsi128_si32(_mm_cmplt_epi16(screenmax, screenmin)))
				continue;
			starty = _mm_extract_epi16(screenmin, 1);
			endy = _mm_extract_epi16(screenmax, 1)+1;
			if (starty >= maxy1 && endy <= miny2)
				continue;
			screeny = _mm_srai_epi32(screeni, 16);
		}

		triangle = &thread->triangles[thread->numtriangles];

		// calculate attribute plans for triangle data...
		// okay, this triangle is going to produce spans, we'd better project
		// the interpolants now (this is what gives perspective texturing),
		// this consists of simply multiplying all arrays by the W coord
		// (which is basically 1/Z), which will be undone per-pixel
		// (multiplying by Z again) to get the perspective-correct array
		// values
		{
			__m128 attribuvslope, attribuxslope, attribuyslope, attribvxslope, attribvyslope, attriborigin, attribedge1, attribedge2, attribxslope, attribyslope, w0, w1, w2, x1, y1;
			__m128 mipedgescale, mipdensity;
			attribuvslope = _mm_div_ps(_mm_movelh_ps(triangleedge1, triangleedge2), _mm_shuffle_ps(trianglenormal, trianglenormal, _MM_SHUFFLE(0, 0, 0, 0)));
			attribuxslope = _mm_shuffle_ps(attribuvslope, attribuvslope, _MM_SHUFFLE(3, 3, 3, 3));
			attribuyslope = _mm_shuffle_ps(attribuvslope, attribuvslope, _MM_SHUFFLE(2, 2, 2, 2));
			attribvxslope = _mm_shuffle_ps(attribuvslope, attribuvslope, _MM_SHUFFLE(1, 1, 1, 1));
			attribvyslope = _mm_shuffle_ps(attribuvslope, attribuvslope, _MM_SHUFFLE(0, 0, 0, 0));
			w0 = _mm_shuffle_ps(screen[0], screen[0], _MM_SHUFFLE(3, 3, 3, 3));
			w1 = _mm_shuffle_ps(screen[1], screen[1], _MM_SHUFFLE(3, 3, 3, 3));
			w2 = _mm_shuffle_ps(screen[2], screen[2], _MM_SHUFFLE(3, 3, 3, 3));
			attribedge1 = _mm_sub_ss(w0, w1);
			attribedge2 = _mm_sub_ss(w2, w1);
			attribxslope = _mm_sub_ss(_mm_mul_ss(attribuxslope, attribedge1), _mm_mul_ss(attribvxslope, attribedge2));
			attribyslope = _mm_sub_ss(_mm_mul_ss(attribvyslope, attribedge2), _mm_mul_ss(attribuyslope, attribedge1));
			x1 = _mm_shuffle_ps(screen[1], screen[1], _MM_SHUFFLE(0, 0, 0, 0));
			y1 = _mm_shuffle_ps(screen[1], screen[1], _MM_SHUFFLE(1, 1, 1, 1));
			attriborigin = _mm_sub_ss(w1, _mm_add_ss(_mm_mul_ss(attribxslope, x1), _mm_mul_ss(attribyslope, y1)));
			_mm_store_ss(&triangle->w[0], attribxslope);
			_mm_store_ss(&triangle->w[1], attribyslope);
			_mm_store_ss(&triangle->w[2], attriborigin);
			
			clip0origin = 0;
			clip0slope = 0;
			clip0dir = 0;
			if(thread->fb_clipplane[0] || thread->fb_clipplane[1] || thread->fb_clipplane[2])
			{
				float cliporigin, clipxslope, clipyslope;
				attriborigin = _mm_shuffle_ps(screen[1], screen[1], _MM_SHUFFLE(2, 2, 2, 2));
				attribedge1 = _mm_sub_ss(_mm_shuffle_ps(screen[0], screen[0], _MM_SHUFFLE(2, 2, 2, 2)), attriborigin);
				attribedge2 = _mm_sub_ss(_mm_shuffle_ps(screen[2], screen[2], _MM_SHUFFLE(2, 2, 2, 2)), attriborigin);
				attribxslope = _mm_sub_ss(_mm_mul_ss(attribuxslope, attribedge1), _mm_mul_ss(attribvxslope, attribedge2));
				attribyslope = _mm_sub_ss(_mm_mul_ss(attribvyslope, attribedge2), _mm_mul_ss(attribuyslope, attribedge1));
				attriborigin = _mm_sub_ss(attriborigin, _mm_add_ss(_mm_mul_ss(attribxslope, x1), _mm_mul_ss(attribyslope, y1)));
				cliporigin = _mm_cvtss_f32(attriborigin)*thread->fb_clipplane[2] + thread->fb_clipplane[3];
				clipxslope = thread->fb_clipplane[0] + _mm_cvtss_f32(attribxslope)*thread->fb_clipplane[2];
				clipyslope = thread->fb_clipplane[1] + _mm_cvtss_f32(attribyslope)*thread->fb_clipplane[2];
				if(clipxslope != 0)
				{
					clip0origin = -cliporigin/clipxslope;
					clip0slope = -clipyslope/clipxslope;
					clip0dir = clipxslope > 0 ? 1 : -1;
				}
				else if(clipyslope > 0)
				{
					clip0origin = dpsoftrast.fb_width*floor(cliporigin/clipyslope);
					clip0slope = dpsoftrast.fb_width;
					clip0dir = -1;
				}
				else if(clipyslope < 0)
				{
					clip0origin = dpsoftrast.fb_width*ceil(cliporigin/clipyslope);
					clip0slope = -dpsoftrast.fb_width;
					clip0dir = -1;
				}
				else if(clip0origin < 0) continue;
			}

			mipedgescale = _mm_setzero_ps();
			for (j = 0;j < DPSOFTRAST_ARRAY_TOTAL; j++)
			{
				__m128 attrib0, attrib1, attrib2;
				k = DPSOFTRAST_ShaderModeTable[thread->shader_mode].arrays[j];
				if (k >= DPSOFTRAST_ARRAY_TOTAL)
					break;
				arrays += numvertices*4;
				GENATTRIBS(attrib0, attrib1, attrib2);
				attriborigin = _mm_mul_ps(attrib1, w1);
				attribedge1 = _mm_sub_ps(_mm_mul_ps(attrib0, w0), attriborigin);
				attribedge2 = _mm_sub_ps(_mm_mul_ps(attrib2, w2), attriborigin);
				attribxslope = _mm_sub_ps(_mm_mul_ps(attribuxslope, attribedge1), _mm_mul_ps(attribvxslope, attribedge2));
				attribyslope = _mm_sub_ps(_mm_mul_ps(attribvyslope, attribedge2), _mm_mul_ps(attribuyslope, attribedge1));
				attriborigin = _mm_sub_ps(attriborigin, _mm_add_ps(_mm_mul_ps(attribxslope, x1), _mm_mul_ps(attribyslope, y1)));
				_mm_storeu_ps(triangle->attribs[k][0], attribxslope);
				_mm_storeu_ps(triangle->attribs[k][1], attribyslope);
				_mm_storeu_ps(triangle->attribs[k][2], attriborigin);
				if (k == DPSOFTRAST_ShaderModeTable[thread->shader_mode].lodarrayindex)
				{
					mipedgescale = _mm_movelh_ps(triangleedge1, triangleedge2);
					mipedgescale = _mm_mul_ps(mipedgescale, mipedgescale);
					mipedgescale = _mm_rsqrt_ps(_mm_add_ps(mipedgescale, _mm_shuffle_ps(mipedgescale, mipedgescale, _MM_SHUFFLE(2, 3, 0, 1))));
					mipedgescale = _mm_mul_ps(_mm_sub_ps(_mm_movelh_ps(attrib0, attrib2), _mm_movelh_ps(attrib1, attrib1)), mipedgescale);
				}
			}

			memset(triangle->mip, 0, sizeof(triangle->mip));
			for (j = 0;j < DPSOFTRAST_MAXTEXTUREUNITS;j++)
			{
				int texunit = DPSOFTRAST_ShaderModeTable[thread->shader_mode].texunits[j];
				if (texunit >= DPSOFTRAST_MAXTEXTUREUNITS)
					break;
				texture = thread->texbound[texunit];
				if (texture && texture->filter > DPSOFTRAST_TEXTURE_FILTER_LINEAR)
				{
					mipdensity = _mm_mul_ps(mipedgescale, _mm_cvtepi32_ps(_mm_shuffle_epi32(_mm_loadl_epi64((const __m128i *)&texture->mipmap[0][2]), _MM_SHUFFLE(1, 0, 1, 0))));
					mipdensity = _mm_mul_ps(mipdensity, mipdensity);
					mipdensity = _mm_add_ps(mipdensity, _mm_shuffle_ps(mipdensity, mipdensity, _MM_SHUFFLE(2, 3, 0, 1)));
					mipdensity = _mm_min_ss(mipdensity, _mm_shuffle_ps(mipdensity, mipdensity, _MM_SHUFFLE(2, 2, 2, 2)));
					// this will be multiplied in the texturing routine by the texture resolution
					y = _mm_cvtss_si32(mipdensity);
					if (y > 0)
					{
						y = (int)(log((float)y)*0.5f/M_LN2);
						if (y > texture->mipmaps - 1)
							y = texture->mipmaps - 1;
						triangle->mip[texunit] = y;
					}
				}
			}
		}
	
		for (y = starty, bandy = min(endy, maxy1); y < endy; bandy = min(endy, maxy2), y = max(y, miny2))
		for (; y < bandy;)
		{
			__m128 xcoords, xslope;
			__m128i ycc = _mm_cmpgt_epi32(_mm_set1_epi32(y), screeny);
			int yccmask = _mm_movemask_epi8(ycc);
			int edge0p, edge0n, edge1p, edge1n;
			int nexty;
			float w, wslope;
			float clip0;
			if (numpoints == 4)
			{
				switch(yccmask)
				{
				default:
				case 0xFFFF: /*0000*/ y = endy; continue;
				case 0xFFF0: /*1000*/ edge0p = 3;edge0n = 0;edge1p = 1;edge1n = 0;break;
				case 0xFF0F: /*0100*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 1;break;
				case 0xFF00: /*1100*/ edge0p = 3;edge0n = 0;edge1p = 2;edge1n = 1;break;
				case 0xF0FF: /*0010*/ edge0p = 1;edge0n = 2;edge1p = 3;edge1n = 2;break;
				case 0xF0F0: /*1010*/ edge0p = 1;edge0n = 2;edge1p = 3;edge1n = 2;break; // concave - nonsense
				case 0xF00F: /*0110*/ edge0p = 0;edge0n = 1;edge1p = 3;edge1n = 2;break;
				case 0xF000: /*1110*/ edge0p = 3;edge0n = 0;edge1p = 3;edge1n = 2;break;
				case 0x0FFF: /*0001*/ edge0p = 2;edge0n = 3;edge1p = 0;edge1n = 3;break;
				case 0x0FF0: /*1001*/ edge0p = 2;edge0n = 3;edge1p = 1;edge1n = 0;break;
				case 0x0F0F: /*0101*/ edge0p = 2;edge0n = 3;edge1p = 2;edge1n = 1;break; // concave - nonsense
				case 0x0F00: /*1101*/ edge0p = 2;edge0n = 3;edge1p = 2;edge1n = 1;break;
				case 0x00FF: /*0011*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 3;break;
				case 0x00F0: /*1011*/ edge0p = 1;edge0n = 2;edge1p = 1;edge1n = 0;break;
				case 0x000F: /*0111*/ edge0p = 0;edge0n = 1;edge1p = 0;edge1n = 3;break;
				case 0x0000: /*1111*/ y++; continue;
				}
			}
			else
			{
				switch(yccmask)
				{
				default:
				case 0xFFFF: /*000*/ y = endy; continue;
				case 0xFFF0: /*100*/ edge0p = 2;edge0n = 0;edge1p = 1;edge1n = 0;break;
				case 0xFF0F: /*010*/ edge0p = 0;edge0n = 1;edge1p = 2;edge1n = 1;break;
				case 0xFF00: /*110*/ edge0p = 2;edge0n = 0;edge1p = 2;edge1n = 1;break;
				case 0x00FF: /*001*/ edge0p = 1;edge0n = 2;edge1p = 0;edge1n = 2;break;
				case 0x00F0: /*101*/ edge0p = 1;edge0n = 2;edge1p = 1;edge1n = 0;break;
				case 0x000F: /*011*/ edge0p = 0;edge0n = 1;edge1p = 0;edge1n = 2;break;
				case 0x0000: /*111*/ y++; continue;
				}
			}
			ycc = _mm_max_epi16(_mm_srli_epi16(ycc, 1), screeny);
			ycc = _mm_min_epi16(ycc, _mm_shuffle_epi32(ycc, _MM_SHUFFLE(1, 0, 3, 2)));
			ycc = _mm_min_epi16(ycc, _mm_shuffle_epi32(ycc, _MM_SHUFFLE(2, 3, 0, 1)));
			nexty = _mm_extract_epi16(ycc, 0);
			if (nexty >= bandy) nexty = bandy-1;
			xslope = _mm_sub_ps(_mm_movelh_ps(screen[edge0n], screen[edge1n]), _mm_movelh_ps(screen[edge0p], screen[edge1p]));
			xslope = _mm_div_ps(xslope, _mm_shuffle_ps(xslope, xslope, _MM_SHUFFLE(3, 3, 1, 1)));
			xcoords = _mm_add_ps(_mm_movelh_ps(screen[edge0p], screen[edge1p]),
								_mm_mul_ps(xslope, _mm_sub_ps(_mm_set1_ps(y), _mm_shuffle_ps(screen[edge0p], screen[edge1p], _MM_SHUFFLE(1, 1, 1, 1)))));
			xcoords = _mm_add_ps(xcoords, _mm_set1_ps(0.5f));
			if (_mm_ucomigt_ss(xcoords, _mm_shuffle_ps(xcoords, xcoords, _MM_SHUFFLE(1, 0, 3, 2))))
			{
				xcoords = _mm_shuffle_ps(xcoords, xcoords, _MM_SHUFFLE(1, 0, 3, 2));
				xslope = _mm_shuffle_ps(xslope, xslope, _MM_SHUFFLE(1, 0, 3, 2));
			}
			clip0 = clip0origin + (y+0.5f)*clip0slope + 0.5f;
			for(; y <= nexty; y++, xcoords = _mm_add_ps(xcoords, xslope), clip0 += clip0slope)
			{
				int startx, endx, offset;
				startx = _mm_cvtss_si32(xcoords);
				endx = _mm_cvtss_si32(_mm_movehl_ps(xcoords, xcoords));
				if (startx < minx) startx = minx;
				if (endx > maxx) endx = maxx;
				if (startx >= endx) continue;

				if (clip0dir)
				{
					if (clip0dir > 0)
					{
						if (startx < clip0) 
						{
							if(endx <= clip0) continue;
							startx = (int)clip0;
						}
					}
					else if (endx > clip0) 
					{
						if(startx >= clip0) continue;
						endx = (int)clip0;
					}
				}
						
				for (offset = startx; offset < endx;offset += DPSOFTRAST_DRAW_MAXSPANLENGTH)
				{
					DPSOFTRAST_State_Span *span = &thread->spans[thread->numspans];
					span->triangle = thread->numtriangles;
					span->x = offset;
					span->y = y;
					span->startx = 0;
					span->endx = min(endx - offset, DPSOFTRAST_DRAW_MAXSPANLENGTH);
					if (span->startx >= span->endx)
						continue;
					wslope = triangle->w[0];
					w = triangle->w[2] + span->x*wslope + span->y*triangle->w[1];
					span->depthslope = (int)(wslope*DPSOFTRAST_DEPTHSCALE);
					span->depthbase = (int)(w*DPSOFTRAST_DEPTHSCALE - DPSOFTRAST_DEPTHOFFSET*(thread->polygonoffset[1] + fabs(wslope)*thread->polygonoffset[0]));
					if (++thread->numspans >= DPSOFTRAST_DRAW_MAXSPANS)
						DPSOFTRAST_Draw_ProcessSpans(thread);
				}
			}
		}

		if (++thread->numtriangles >= DPSOFTRAST_DRAW_MAXTRIANGLES)
		{
			DPSOFTRAST_Draw_ProcessSpans(thread);
			thread->numtriangles = 0;
		}
	}

	if (!ATOMIC_DECREMENT(command->refcount))
	{
		if (command->commandsize <= DPSOFTRAST_ALIGNCOMMAND(sizeof(DPSOFTRAST_Command_Draw)))
			MM_FREE(command->arrays);
	}

	if (thread->numspans > 0 || thread->numtriangles > 0)
	{
		DPSOFTRAST_Draw_ProcessSpans(thread);
		thread->numtriangles = 0;
	}
#endif
}

static DPSOFTRAST_Command_Draw *DPSOFTRAST_Draw_AllocateDrawCommand(int firstvertex, int numvertices, int numtriangles, const int *element3i, const unsigned short *element3s)
{
	int i;
	int j;
	int commandsize = DPSOFTRAST_ALIGNCOMMAND(sizeof(DPSOFTRAST_Command_Draw));
	int datasize = 2*numvertices*sizeof(float[4]);
	DPSOFTRAST_Command_Draw *command;
	unsigned char *data;
	for (i = 0; i < DPSOFTRAST_ARRAY_TOTAL; i++)
	{
		j = DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].arrays[i];
		if (j >= DPSOFTRAST_ARRAY_TOTAL)
			break;
		datasize += numvertices*sizeof(float[4]);
	}
	if (element3s)
		datasize += numtriangles*sizeof(unsigned short[3]);
	else if (element3i)
		datasize += numtriangles*sizeof(int[3]);
	datasize = DPSOFTRAST_ALIGNCOMMAND(datasize);
	if (commandsize + datasize > DPSOFTRAST_DRAW_MAXCOMMANDSIZE)
	{
		command = (DPSOFTRAST_Command_Draw *) DPSOFTRAST_AllocateCommand(DPSOFTRAST_OPCODE_Draw, commandsize);
		data = (unsigned char *)MM_CALLOC(datasize, 1);
	}
	else
	{
		command = (DPSOFTRAST_Command_Draw *) DPSOFTRAST_AllocateCommand(DPSOFTRAST_OPCODE_Draw, commandsize + datasize);
		data = (unsigned char *)command + commandsize;
	}
	command->firstvertex = firstvertex;
	command->numvertices = numvertices;
	command->numtriangles = numtriangles;
	command->arrays = (float *)data;
	memset(dpsoftrast.post_array4f, 0, sizeof(dpsoftrast.post_array4f));
	dpsoftrast.firstvertex = firstvertex;
	dpsoftrast.numvertices = numvertices;
	dpsoftrast.screencoord4f = (float *)data;
	data += numvertices*sizeof(float[4]);
	dpsoftrast.post_array4f[DPSOFTRAST_ARRAY_POSITION] = (float *)data;
	data += numvertices*sizeof(float[4]);
	for (i = 0; i < DPSOFTRAST_ARRAY_TOTAL; i++)
	{
		j = DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].arrays[i];
		if (j >= DPSOFTRAST_ARRAY_TOTAL)
			break;
		dpsoftrast.post_array4f[j] = (float *)data;
		data += numvertices*sizeof(float[4]);
	}
	command->element3i = NULL;
	command->element3s = NULL;
	if (element3s)
	{
		command->element3s = (unsigned short *)data;
		memcpy(command->element3s, element3s, numtriangles*sizeof(unsigned short[3]));
	}
	else if (element3i)
	{
		command->element3i = (int *)data;
		memcpy(command->element3i, element3i, numtriangles*sizeof(int[3]));
	}
	return command;
}

void DPSOFTRAST_DrawTriangles(int firstvertex, int numvertices, int numtriangles, const int *element3i, const unsigned short *element3s)
{
	DPSOFTRAST_Command_Draw *command = DPSOFTRAST_Draw_AllocateDrawCommand(firstvertex, numvertices, numtriangles, element3i, element3s);
	DPSOFTRAST_ShaderModeTable[dpsoftrast.shader_mode].Vertex();
	command->starty = bound(0, dpsoftrast.drawstarty, dpsoftrast.fb_height);
	command->endy = bound(0, dpsoftrast.drawendy, dpsoftrast.fb_height);
	if (command->starty >= command->endy)
	{
		if (command->commandsize <= DPSOFTRAST_ALIGNCOMMAND(sizeof(DPSOFTRAST_Command_Draw)))
			MM_FREE(command->arrays);
		DPSOFTRAST_UndoCommand(command->commandsize);
		return;
	}
	command->clipped = dpsoftrast.drawclipped;
	command->refcount = dpsoftrast.numthreads;

	if (dpsoftrast.usethreads)
	{
		int i;
		DPSOFTRAST_Draw_SyncCommands();
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			DPSOFTRAST_State_Thread *thread = &dpsoftrast.threads[i];
			if (((command->starty < thread->maxy1 && command->endy > thread->miny1) || (command->starty < thread->maxy2 && command->endy > thread->miny2)) && thread->starving)
				Thread_CondSignal(thread->drawcond);
		}
	}
	else
	{
		DPSOFTRAST_Draw_FlushThreads();
	}
}

DEFCOMMAND(23, SetRenderTargets, int width; int height;);
static void DPSOFTRAST_Interpret_SetRenderTargets(DPSOFTRAST_State_Thread *thread, const DPSOFTRAST_Command_SetRenderTargets *command)
{
	thread->validate |= DPSOFTRAST_VALIDATE_FB;
}
void DPSOFTRAST_SetRenderTargets(int width, int height, unsigned int *depthpixels, unsigned int *colorpixels0, unsigned int *colorpixels1, unsigned int *colorpixels2, unsigned int *colorpixels3)
{
	DPSOFTRAST_Command_SetRenderTargets *command;
	if (width != dpsoftrast.fb_width || height != dpsoftrast.fb_height || depthpixels != dpsoftrast.fb_depthpixels ||
		colorpixels0 != dpsoftrast.fb_colorpixels[0] || colorpixels1 != dpsoftrast.fb_colorpixels[1] ||
		colorpixels2 != dpsoftrast.fb_colorpixels[2] || colorpixels3 != dpsoftrast.fb_colorpixels[3])
		DPSOFTRAST_Flush();
	dpsoftrast.fb_width = width;
	dpsoftrast.fb_height = height;
	dpsoftrast.fb_depthpixels = depthpixels;
	dpsoftrast.fb_colorpixels[0] = colorpixels0;
	dpsoftrast.fb_colorpixels[1] = colorpixels1;
	dpsoftrast.fb_colorpixels[2] = colorpixels2;
	dpsoftrast.fb_colorpixels[3] = colorpixels3;
	DPSOFTRAST_RecalcViewport(dpsoftrast.viewport, dpsoftrast.fb_viewportcenter, dpsoftrast.fb_viewportscale);
	command = DPSOFTRAST_ALLOCATECOMMAND(SetRenderTargets);
	command->width = width;
	command->height = height;
}
 
static void DPSOFTRAST_Draw_InterpretCommands(DPSOFTRAST_State_Thread *thread, int endoffset)
{
	int commandoffset = thread->commandoffset;
	while (commandoffset != endoffset)
	{
		DPSOFTRAST_Command *command = (DPSOFTRAST_Command *)&dpsoftrast.commandpool.commands[commandoffset];
		switch (command->opcode)
		{
#define INTERPCOMMAND(name) \
		case DPSOFTRAST_OPCODE_##name : \
			DPSOFTRAST_Interpret_##name (thread, (DPSOFTRAST_Command_##name *)command); \
			commandoffset += DPSOFTRAST_ALIGNCOMMAND(sizeof( DPSOFTRAST_Command_##name )); \
			if (commandoffset >= DPSOFTRAST_DRAW_MAXCOMMANDPOOL) \
				commandoffset = 0; \
			break;
		INTERPCOMMAND(Viewport)
		INTERPCOMMAND(ClearColor)
		INTERPCOMMAND(ClearDepth)
		INTERPCOMMAND(ColorMask)
		INTERPCOMMAND(DepthTest)
		INTERPCOMMAND(ScissorTest)
		INTERPCOMMAND(Scissor)
		INTERPCOMMAND(BlendFunc)
		INTERPCOMMAND(BlendSubtract)
		INTERPCOMMAND(DepthMask)
		INTERPCOMMAND(DepthFunc)
		INTERPCOMMAND(DepthRange)
		INTERPCOMMAND(PolygonOffset)
		INTERPCOMMAND(CullFace)
		INTERPCOMMAND(SetTexture)
		INTERPCOMMAND(SetShader)
		INTERPCOMMAND(Uniform4f)
		INTERPCOMMAND(UniformMatrix4f)
		INTERPCOMMAND(Uniform1i)
		INTERPCOMMAND(SetRenderTargets)
		INTERPCOMMAND(ClipPlane)

		case DPSOFTRAST_OPCODE_Draw:
			DPSOFTRAST_Interpret_Draw(thread, (DPSOFTRAST_Command_Draw *)command);
			commandoffset += command->commandsize;
			if (commandoffset >= DPSOFTRAST_DRAW_MAXCOMMANDPOOL)
				commandoffset = 0;
			thread->commandoffset = commandoffset;
			break;

		case DPSOFTRAST_OPCODE_Reset:
			commandoffset = 0;
			break;
		}
	}
	thread->commandoffset = commandoffset;
}

static int DPSOFTRAST_Draw_Thread(void *data)
{
	DPSOFTRAST_State_Thread *thread = (DPSOFTRAST_State_Thread *)data;
	while(thread->index >= 0)
	{
		if (thread->commandoffset != dpsoftrast.drawcommand)
		{
			DPSOFTRAST_Draw_InterpretCommands(thread, dpsoftrast.drawcommand);	
		}
		else 
		{
			Thread_LockMutex(thread->drawmutex);
			if (thread->commandoffset == dpsoftrast.drawcommand && thread->index >= 0)
			{
				if (thread->waiting) Thread_CondSignal(thread->waitcond);
				thread->starving = true;
				Thread_CondWait(thread->drawcond, thread->drawmutex);
				thread->starving = false;
			}
			Thread_UnlockMutex(thread->drawmutex);
		}
	}   
	return 0;
}

static void DPSOFTRAST_Draw_FlushThreads(void)
{
	DPSOFTRAST_State_Thread *thread;
	int i;
	DPSOFTRAST_Draw_SyncCommands();
	if (dpsoftrast.usethreads) 
	{
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			thread = &dpsoftrast.threads[i];
			if (thread->commandoffset != dpsoftrast.drawcommand)
			{
				Thread_LockMutex(thread->drawmutex);
				if (thread->commandoffset != dpsoftrast.drawcommand && thread->starving)
					Thread_CondSignal(thread->drawcond);
				Thread_UnlockMutex(thread->drawmutex);
			}
		}
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			thread = &dpsoftrast.threads[i];
			if (thread->commandoffset != dpsoftrast.drawcommand)
			{
				Thread_LockMutex(thread->drawmutex);
				if (thread->commandoffset != dpsoftrast.drawcommand)
				{
					thread->waiting = true;
					Thread_CondWait(thread->waitcond, thread->drawmutex);
					thread->waiting = false;
				}
				Thread_UnlockMutex(thread->drawmutex);
			}
		}
	}
	else
	{
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			thread = &dpsoftrast.threads[i];
			if (thread->commandoffset != dpsoftrast.drawcommand)
				DPSOFTRAST_Draw_InterpretCommands(thread, dpsoftrast.drawcommand);
		}
	}
	dpsoftrast.commandpool.usedcommands = 0;
}

void DPSOFTRAST_Flush(void)
{
	DPSOFTRAST_Draw_FlushThreads();
}

void DPSOFTRAST_Finish(void)
{
	DPSOFTRAST_Flush();
}

int DPSOFTRAST_Init(int width, int height, int numthreads, int interlace, unsigned int *colorpixels, unsigned int *depthpixels)
{
	int i;
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
	dpsoftrast.viewport[0] = 0;
	dpsoftrast.viewport[1] = 0;
	dpsoftrast.viewport[2] = dpsoftrast.fb_width;
	dpsoftrast.viewport[3] = dpsoftrast.fb_height;
	DPSOFTRAST_RecalcViewport(dpsoftrast.viewport, dpsoftrast.fb_viewportcenter, dpsoftrast.fb_viewportscale);
	dpsoftrast.texture_firstfree = 1;
	dpsoftrast.texture_end = 1;
	dpsoftrast.texture_max = 0;
	dpsoftrast.color[0] = 1;
	dpsoftrast.color[1] = 1;
	dpsoftrast.color[2] = 1;
	dpsoftrast.color[3] = 1;
	dpsoftrast.usethreads = numthreads > 0 && Thread_HasThreads();
	dpsoftrast.interlace = dpsoftrast.usethreads ? bound(0, interlace, 1) : 0;
	dpsoftrast.numthreads = dpsoftrast.usethreads ? bound(1, numthreads, 64) : 1;
	dpsoftrast.threads = (DPSOFTRAST_State_Thread *)MM_CALLOC(dpsoftrast.numthreads, sizeof(DPSOFTRAST_State_Thread));
	for (i = 0; i < dpsoftrast.numthreads; i++)
	{
		DPSOFTRAST_State_Thread *thread = &dpsoftrast.threads[i];
		thread->index = i;
		thread->cullface = GL_BACK;
       	thread->colormask[0] = 1; 
		thread->colormask[1] = 1;
		thread->colormask[2] = 1;
		thread->colormask[3] = 1;
		thread->blendfunc[0] = GL_ONE;
		thread->blendfunc[1] = GL_ZERO;
		thread->depthmask = true;
		thread->depthtest = true;
		thread->depthfunc = GL_LEQUAL;
		thread->scissortest = false;
		thread->viewport[0] = 0;
		thread->viewport[1] = 0;
		thread->viewport[2] = dpsoftrast.fb_width;
		thread->viewport[3] = dpsoftrast.fb_height;
		thread->scissor[0] = 0;
		thread->scissor[1] = 0;
		thread->scissor[2] = dpsoftrast.fb_width;
		thread->scissor[3] = dpsoftrast.fb_height;
		thread->depthrange[0] = 0;
		thread->depthrange[1] = 1;
		thread->polygonoffset[0] = 0;
		thread->polygonoffset[1] = 0;
		thread->clipplane[0] = 0;
		thread->clipplane[1] = 0;
		thread->clipplane[2] = 0;
		thread->clipplane[3] = 1;
	
		thread->numspans = 0;
		thread->numtriangles = 0;
		thread->commandoffset = 0;
		thread->waiting = false;
		thread->starving = false;
	   
		thread->validate = -1;
		DPSOFTRAST_Validate(thread, -1);
 
		if (dpsoftrast.usethreads)
		{
			thread->waitcond = Thread_CreateCond();
			thread->drawcond = Thread_CreateCond();
			thread->drawmutex = Thread_CreateMutex();
			thread->thread = Thread_CreateThread(DPSOFTRAST_Draw_Thread, thread);
		}
	}
	return 0;
}

void DPSOFTRAST_Shutdown(void)
{
	int i;
	if (dpsoftrast.usethreads && dpsoftrast.numthreads > 0)
	{
		DPSOFTRAST_State_Thread *thread;
		for (i = 0; i < dpsoftrast.numthreads; i++)
		{
			thread = &dpsoftrast.threads[i];
			Thread_LockMutex(thread->drawmutex);
			thread->index = -1;
			Thread_CondSignal(thread->drawcond);
			Thread_UnlockMutex(thread->drawmutex);
			Thread_WaitThread(thread->thread, 0);
			Thread_DestroyCond(thread->waitcond);
			Thread_DestroyCond(thread->drawcond);
			Thread_DestroyMutex(thread->drawmutex);
		}
	}
	for (i = 0;i < dpsoftrast.texture_end;i++)
		if (dpsoftrast.texture[i].bytes)
			MM_FREE(dpsoftrast.texture[i].bytes);
	if (dpsoftrast.texture)
		free(dpsoftrast.texture);
	if (dpsoftrast.threads)
		MM_FREE(dpsoftrast.threads);
	memset(&dpsoftrast, 0, sizeof(dpsoftrast));
}

