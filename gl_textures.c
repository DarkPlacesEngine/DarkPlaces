
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"

cvar_t	gl_max_size = {CVAR_SAVE, "gl_max_size", "2048"};
cvar_t	gl_max_scrapsize = {CVAR_SAVE, "gl_max_scrapsize", "256"};
cvar_t	gl_picmip = {CVAR_SAVE, "gl_picmip", "0"};
cvar_t	r_lerpimages = {CVAR_SAVE, "r_lerpimages", "1"};
cvar_t	r_precachetextures = {CVAR_SAVE, "r_precachetextures", "1"};
cvar_t  gl_texture_anisotropy = {CVAR_SAVE, "gl_texture_anisotropy", "1"};

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_mag = GL_LINEAR;


static mempool_t *texturemempool;

// note: this must not conflict with TEXF_ flags in r_textures.h
// cleared when a texture is uploaded
#define GLTEXF_UPLOAD 0x00010000
// bitmask for mismatch checking
#define GLTEXF_IMPORTANTBITS (0)
// set when image is uploaded and freed
#define GLTEXF_DESTROYED 0x00040000

// size of images which hold fragment textures, ignores picmip and max_size
static int block_size;

typedef struct
{
	int textype;
	int inputbytesperpixel;
	int internalbytesperpixel;
	int glformat;
	int glinternalformat;
}
textypeinfo_t;

static textypeinfo_t textype_palette       = {TEXTYPE_PALETTE, 1, 4, GL_RGBA   , 3};
static textypeinfo_t textype_rgb           = {TEXTYPE_RGB    , 3, 3, GL_RGB    , 3};
static textypeinfo_t textype_rgba          = {TEXTYPE_RGBA   , 4, 4, GL_RGBA   , 3};
static textypeinfo_t textype_palette_alpha = {TEXTYPE_PALETTE, 1, 4, GL_RGBA   , 4};
static textypeinfo_t textype_rgba_alpha    = {TEXTYPE_RGBA   , 4, 4, GL_RGBA   , 4};
static textypeinfo_t textype_dsdt          = {TEXTYPE_DSDT   , 2, 2, GL_DSDT_NV, GL_DSDT8_NV};

// a tiling texture (most common type)
#define GLIMAGETYPE_TILE 0
// a fragments texture (contains one or more fragment textures)
#define GLIMAGETYPE_FRAGMENTS 1

#define GLTEXTURETYPE_1D 0
#define GLTEXTURETYPE_2D 1
#define GLTEXTURETYPE_3D 2
#define GLTEXTURETYPE_CUBEMAP 3

static int gltexturetypeenums[4] = {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARB};
static int gltexturetypebindingenums[4] = {GL_TEXTURE_BINDING_1D, GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_CUBE_MAP_ARB};
static int gltexturetypedimensions[4] = {1, 2, 3, 2};
static int cubemapside[6] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
};

// a gltextureimage can have one (or more if fragments) gltextures inside
typedef struct gltextureimage_s
{
	struct gltextureimage_s *imagechain;
	int texturecount;
	int type; // one of the GLIMAGETYPE_ values
	int texturetype; // one of the GLTEXTURETYPE_ values
	int sides; // 1 or 6 depending on texturetype
	int texnum; // GL texture slot number
	int width, height, depth; // 3D texture support
	int bytesperpixel; // bytes per pixel
	int glformat; // GL_RGB or GL_RGBA
	int glinternalformat; // 3 or 4
	int flags;
	short *blockallocation; // fragment allocation (2D only)
}
gltextureimage_t;

typedef struct gltexture_s
{
	// this field is exposed to the R_GetTexture macro, for speed reasons
	// (must be identical in rtexture_t)
	int texnum; // GL texture slot number

	// pointer to texturepool (check this to see if the texture is allocated)
	struct gltexturepool_s *pool;
	// pointer to next texture in texturepool chain
	struct gltexture_s *chain;
	// pointer into gltextureimage array
	gltextureimage_t *image;
	// name of the texture (this might be removed someday), no duplicates
	char identifier[32];
	// location in the image, and size
	int x, y, z, width, height, depth;
	// copy of the original texture(s) supplied to the upload function, for
	// delayed uploads (non-precached)
	qbyte *inputtexels;
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
}
gltexture_t;

#define TEXTUREPOOL_SENTINEL 0xC0DEDBAD

typedef struct gltexturepool_s
{
	unsigned int sentinel;
	struct gltextureimage_s *imagechain;
	struct gltexture_s *gltchain;
	struct gltexturepool_s *next;
}
gltexturepool_t;

static gltexturepool_t *gltexturepoolchain = NULL;

static qbyte *resizebuffer = NULL, *colorconvertbuffer;
static int resizebuffersize = 0;
static qbyte *texturebuffer;
static int texturebuffersize = 0;

static int realmaxsize = 0;

static textypeinfo_t *R_GetTexTypeInfo(int textype, int flags)
{
	if (flags & TEXF_ALPHA)
	{
		switch(textype)
		{
		case TEXTYPE_PALETTE:
			return &textype_palette_alpha;
		case TEXTYPE_RGB:
			Host_Error("R_GetTexTypeInfo: RGB format has no alpha, TEXF_ALPHA not allowed\n");
			return NULL;
		case TEXTYPE_RGBA:
			return &textype_rgba_alpha;
		default:
			Host_Error("R_GetTexTypeInfo: unknown texture format\n");
			return NULL;
		}
	}
	else
	{
		switch(textype)
		{
		case TEXTYPE_PALETTE:
			return &textype_palette;
		case TEXTYPE_RGB:
			return &textype_rgb;
		case TEXTYPE_RGBA:
			return &textype_rgba;
		case TEXTYPE_DSDT:
			return &textype_dsdt;
		default:
			Host_Error("R_GetTexTypeInfo: unknown texture format\n");
			return NULL;
		}
	}
}

static void R_UploadTexture(gltexture_t *t);

static void R_PrecacheTexture(gltexture_t *glt)
{
	int precache;
	precache = false;
	if (glt->flags & TEXF_ALWAYSPRECACHE)
		precache = true;
	else if (r_precachetextures.integer >= 2)
		precache = true;
	else if (r_precachetextures.integer >= 1)
		if (glt->flags & TEXF_PRECACHE)
			precache = true;

	if (precache)
		R_UploadTexture(glt);
}

int R_RealGetTexture(rtexture_t *rt)
{
	if (rt)
	{
		gltexture_t *glt;
		glt = (gltexture_t *)rt;
		if (glt->flags & GLTEXF_UPLOAD)
			R_UploadTexture(glt);
		glt->texnum = glt->image->texnum;
		return glt->image->texnum;
	}
	else
		return 0;
}

void R_FreeTexture(rtexture_t *rt)
{
	gltexture_t *glt, **gltpointer;
	gltextureimage_t *image, **gltimagepointer;

	glt = (gltexture_t *)rt;
	if (glt == NULL)
		Host_Error("R_FreeTexture: texture == NULL\n");

	for (gltpointer = &glt->pool->gltchain;*gltpointer && *gltpointer != glt;gltpointer = &(*gltpointer)->chain);
	if (*gltpointer == glt)
		*gltpointer = glt->chain;
	else
		Host_Error("R_FreeTexture: texture \"%s\" not linked in pool\n", glt->identifier);

	// note: if freeing a fragment texture, this will not make the claimed
	// space available for new textures unless all other fragments in the
	// image are also freed
	if (glt->image)
	{
		image = glt->image;
		image->texturecount--;
		if (image->texturecount < 1)
		{
			for (gltimagepointer = &glt->pool->imagechain;*gltimagepointer && *gltimagepointer != image;gltimagepointer = &(*gltimagepointer)->imagechain);
			if (*gltimagepointer == image)
				*gltimagepointer = image->imagechain;
			else
				Host_Error("R_FreeTexture: image not linked in pool\n");
			if (image->texnum)
				qglDeleteTextures(1, &image->texnum);
			if (image->blockallocation)
				Mem_Free(image->blockallocation);
			Mem_Free(image);
		}
	}

	if (glt->inputtexels)
		Mem_Free(glt->inputtexels);
	Mem_Free(glt);
}

rtexturepool_t *R_AllocTexturePool(void)
{
	gltexturepool_t *pool;
	if (texturemempool == NULL)
		return NULL;
	pool = Mem_Alloc(texturemempool, sizeof(gltexturepool_t));
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
		Host_Error("R_FreeTexturePool: pool already freed\n");
	for (poolpointer = &gltexturepoolchain;*poolpointer && *poolpointer != pool;poolpointer = &(*poolpointer)->next);
	if (*poolpointer == pool)
		*poolpointer = pool->next;
	else
		Host_Error("R_FreeTexturePool: pool not linked\n");
	while (pool->gltchain)
		R_FreeTexture((rtexture_t *)pool->gltchain);
	if (pool->imagechain)
		Sys_Error("R_FreeTexturePool: not all images freed\n");
	Mem_Free(pool);
}


typedef struct
{
	char *name;
	int minification, magnification;
}
glmode_t;

static glmode_t modes[] =
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
	gltextureimage_t *image;
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
	for (pool = gltexturepoolchain;pool;pool = pool->next)
	{
		for (image = pool->imagechain;image;image = image->imagechain)
		{
			// only update already uploaded images
			if (!(image->flags & GLTEXF_UPLOAD) && !(image->flags & (TEXF_FORCENEAREST | TEXF_FORCELINEAR)))
			{
				qglGetIntegerv(gltexturetypebindingenums[image->texturetype], &oldbindtexnum);
				qglBindTexture(gltexturetypeenums[image->texturetype], image->texnum);
				if (image->flags & TEXF_MIPMAP)
					qglTexParameteri(gltexturetypeenums[image->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_min);
				else
					qglTexParameteri(gltexturetypeenums[image->texturetype], GL_TEXTURE_MIN_FILTER, gl_filter_mag);
				qglTexParameteri(gltexturetypeenums[image->texturetype], GL_TEXTURE_MAG_FILTER, gl_filter_mag);
				qglBindTexture(gltexturetypeenums[image->texturetype], oldbindtexnum);
			}
		}
	}
}

static int R_CalcTexelDataSize (gltexture_t *glt)
{
	int width2, height2, depth2, size, picmip;
	if (glt->flags & TEXF_FRAGMENT)
		size = glt->width * glt->height * glt->depth;
	else
	{
		picmip = 0;
		if (glt->flags & TEXF_PICMIP)
			picmip = gl_picmip.integer;
		if (gl_max_size.integer > realmaxsize)
			Cvar_SetValue("gl_max_size", realmaxsize);
		// calculate final size
		for (width2 = 1;width2 < glt->width;width2 <<= 1);
		for (height2 = 1;height2 < glt->height;height2 <<= 1);
		for (depth2 = 1;depth2 < glt->depth;depth2 <<= 1);
		for (width2 >>= picmip;width2 > gl_max_size.integer;width2 >>= 1);
		for (height2 >>= picmip;height2 > gl_max_size.integer;height2 >>= 1);
		for (depth2 >>= picmip;depth2 > gl_max_size.integer;depth2 >>= 1);
		if (width2 < 1) width2 = 1;
		if (height2 < 1) height2 = 1;
		if (depth2 < 1) depth2 = 1;

		size = 0;
		if (glt->flags & TEXF_MIPMAP)
		{
			while (width2 > 1 || height2 > 1 || depth2 > 1)
			{
				size += width2 * height2 * depth2;
				if (width2 > 1)
					width2 >>= 1;
				if (height2 > 1)
					height2 >>= 1;
				if (depth2 > 1)
					depth2 >>= 1;
			}
			size++; // count the last 1x1 mipmap
		}
		else
			size = width2 * height2 * depth2;
	}
	size *= glt->textype->internalbytesperpixel * glt->image->sides;

	return size;
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
			isloaded = !(glt->flags & GLTEXF_UPLOAD);
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
			Con_Printf("texturepool %10p total: %i (%.3fMB, %.3fMB original), uploaded %i (%.3fMB, %.3fMB original), upload on demand %i (%.3fMB, %.3fMB original)\n", pool, pooltotal, pooltotalt / 1048576.0, pooltotalp / 1048576.0, poolloaded, poolloadedt / 1048576.0, poolloadedp / 1048576.0, pooltotal - poolloaded, (pooltotalt - poolloadedt) / 1048576.0, (pooltotalp - poolloadedp) / 1048576.0);
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

char engineversion[40];

static void r_textures_start(void)
{
	// deal with size limits of various drivers (3dfx in particular)
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &realmaxsize);
	CHECKGLERROR
	// LordHavoc: allow any alignment
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	CHECKGLERROR

	// use the largest scrap texture size we can (not sure if this is really a good idea)
	for (block_size = 1;block_size < realmaxsize && block_size < gl_max_scrapsize.integer;block_size <<= 1);

	texturemempool = Mem_AllocPool("texture management", 0, NULL);

	// Disable JPEG screenshots if the DLL isn't loaded
	if (! JPEG_OpenLibrary ())
		Cvar_SetValueQuick (&scr_screenshot_jpeg, 0);
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
	texturebuffersize = 0;
	resizebuffer = NULL;
	colorconvertbuffer = NULL;
	texturebuffer = NULL;
	Mem_FreePool(&texturemempool);
}

static void r_textures_newmap(void)
{
}

void R_Textures_Init (void)
{
	Cmd_AddCommand("gl_texturemode", &GL_TextureMode_f);
	Cmd_AddCommand("r_texturestats", R_TextureStats_f);
	Cvar_RegisterVariable (&gl_max_scrapsize);
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&r_lerpimages);
	Cvar_RegisterVariable (&r_precachetextures);
	Cvar_RegisterVariable (&gl_texture_anisotropy);

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
		gltextureimage_t *image;
		gltexturepool_t *pool;
		GLint oldbindtexnum;

		old_aniso = bound(1, gl_texture_anisotropy.integer, gl_max_anisotropy);

		Cvar_SetValueQuick(&gl_texture_anisotropy, old_aniso);

		for (pool = gltexturepoolchain;pool;pool = pool->next)
		{
			for (image = pool->imagechain;image;image = image->imagechain)
			{
				// only update already uploaded images
				if (!(image->flags & GLTEXF_UPLOAD) && (image->flags & TEXF_MIPMAP))
				{
					qglGetIntegerv(gltexturetypebindingenums[image->texturetype], &oldbindtexnum);

					qglBindTexture(gltexturetypeenums[image->texturetype], image->texnum);
					qglTexParameteri(gltexturetypeenums[image->texturetype], GL_TEXTURE_MAX_ANISOTROPY_EXT, old_aniso);CHECKGLERROR

					qglBindTexture(gltexturetypeenums[image->texturetype], oldbindtexnum);
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
		resizebuffer = Mem_Alloc(texturemempool, resizebuffersize);
		colorconvertbuffer = Mem_Alloc(texturemempool, resizebuffersize);
		if (!resizebuffer || !colorconvertbuffer)
			Host_Error("R_Upload: out of memory\n");
	}
}

static void GL_SetupTextureParameters(int flags, int texturetype)
{
	int textureenum = gltexturetypeenums[texturetype];
	int wrapmode = ((flags & TEXF_CLAMP) && gl_support_clamptoedge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;

	CHECKGLERROR

	if (gl_support_anisotropy && (flags & TEXF_MIPMAP))
	{
		int aniso = bound(1, gl_texture_anisotropy.integer, gl_max_anisotropy);
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

	CHECKGLERROR
}

static void R_Upload(gltexture_t *glt, qbyte *data)
{
	int i, mip, width, height, depth;
	GLint oldbindtexnum;
	qbyte *prevbuffer;
	prevbuffer = data;

	CHECKGLERROR

	glt->texnum = glt->image->texnum;
	// we need to restore the texture binding after finishing the upload
	qglGetIntegerv(gltexturetypebindingenums[glt->image->texturetype], &oldbindtexnum);
	qglBindTexture(gltexturetypeenums[glt->image->texturetype], glt->image->texnum);
	CHECKGLERROR
	glt->flags &= ~GLTEXF_UPLOAD;

	if (glt->flags & TEXF_FRAGMENT)
	{
		if (glt->image->flags & GLTEXF_UPLOAD)
		{
			glt->image->flags &= ~GLTEXF_UPLOAD;
			Con_DPrint("uploaded new fragments image\n");
			R_MakeResizeBufferBigger(glt->image->width * glt->image->height * glt->image->depth * glt->image->bytesperpixel);
			memset(resizebuffer, 255, glt->image->width * glt->image->height * glt->image->depth * glt->image->bytesperpixel);
			switch(glt->image->texturetype)
			{
			case GLTEXTURETYPE_1D:
				qglTexImage1D(GL_TEXTURE_1D, 0, glt->image->glinternalformat, glt->image->width, 0, glt->image->glformat, GL_UNSIGNED_BYTE, resizebuffer);
				CHECKGLERROR
				break;
			case GLTEXTURETYPE_2D:
				qglTexImage2D(GL_TEXTURE_2D, 0, glt->image->glinternalformat, glt->image->width, glt->image->height, 0, glt->image->glformat, GL_UNSIGNED_BYTE, resizebuffer);
				CHECKGLERROR
				break;
			case GLTEXTURETYPE_3D:
				qglTexImage3D(GL_TEXTURE_3D, 0, glt->image->glinternalformat, glt->image->width, glt->image->height, glt->image->depth, 0, glt->image->glformat, GL_UNSIGNED_BYTE, resizebuffer);
				CHECKGLERROR
				break;
			default:
				Host_Error("R_Upload: fragment texture of type other than 1D, 2D, or 3D\n");
				break;
			}
			GL_SetupTextureParameters(glt->image->flags, glt->image->texturetype);
		}

		if (prevbuffer == NULL)
		{
			R_MakeResizeBufferBigger(glt->image->width * glt->image->height * glt->image->depth * glt->image->bytesperpixel);
			memset(resizebuffer, 0, glt->width * glt->height * glt->image->depth * glt->image->bytesperpixel);
			prevbuffer = resizebuffer;
		}
		else if (glt->textype->textype == TEXTYPE_PALETTE)
		{
			// promote paletted to RGBA, so we only have to worry about RGB and
			// RGBA in the rest of this code
			R_MakeResizeBufferBigger(glt->image->width * glt->image->height * glt->image->depth * glt->image->sides * glt->image->bytesperpixel);
			Image_Copy8bitRGBA(prevbuffer, colorconvertbuffer, glt->width * glt->height * glt->depth, glt->palette);
			prevbuffer = colorconvertbuffer;
		}

		switch(glt->image->texturetype)
		{
		case GLTEXTURETYPE_1D:
			qglTexSubImage1D(GL_TEXTURE_1D, 0, glt->x, glt->width, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		case GLTEXTURETYPE_2D:
			qglTexSubImage2D(GL_TEXTURE_2D, 0, glt->x, glt->y, glt->width, glt->height, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		case GLTEXTURETYPE_3D:
			qglTexSubImage3D(GL_TEXTURE_3D, 0, glt->x, glt->y, glt->z, glt->width, glt->height, glt->depth, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			break;
		default:
			Host_Error("R_Upload: fragment texture of type other than 1D, 2D, or 3D\n");
			break;
		}
	}
	else
	{
		glt->image->flags &= ~GLTEXF_UPLOAD;

		// these are rounded up versions of the size to do better resampling
		for (width  = 1;width  < glt->width ;width  <<= 1);
		for (height = 1;height < glt->height;height <<= 1);
		for (depth  = 1;depth  < glt->depth ;depth  <<= 1);

		R_MakeResizeBufferBigger(width * height * depth * glt->image->sides * glt->image->bytesperpixel);

		if (prevbuffer == NULL)
		{
			width = glt->image->width;
			height = glt->image->height;
			depth = glt->image->depth;
			memset(resizebuffer, 0, width * height * depth * glt->image->bytesperpixel);
			prevbuffer = resizebuffer;
		}
		else
		{
			if (glt->textype->textype == TEXTYPE_PALETTE)
			{
				// promote paletted to RGBA, so we only have to worry about RGB and
				// RGBA in the rest of this code
				Image_Copy8bitRGBA(prevbuffer, colorconvertbuffer, glt->width * glt->height * glt->depth * glt->image->sides, glt->palette);
				prevbuffer = colorconvertbuffer;
			}
		}

		// cubemaps contain multiple images and thus get processed a bit differently
		if (glt->image->texturetype != GLTEXTURETYPE_CUBEMAP)
		{
			if (glt->width != width || glt->height != height || glt->depth != depth)
			{
				Image_Resample(prevbuffer, glt->width, glt->height, glt->depth, resizebuffer, width, height, depth, glt->image->bytesperpixel, r_lerpimages.integer);
				prevbuffer = resizebuffer;
			}
			// picmip/max_size
			while (width > glt->image->width || height > glt->image->height || depth > glt->image->depth)
			{
				Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, glt->image->width, glt->image->height, glt->image->depth, glt->image->bytesperpixel);
				prevbuffer = resizebuffer;
			}
		}
		mip = 0;
		switch(glt->image->texturetype)
		{
		case GLTEXTURETYPE_1D:
			qglTexImage1D(GL_TEXTURE_1D, mip++, glt->image->glinternalformat, width, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->image->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage1D(GL_TEXTURE_1D, mip++, glt->image->glinternalformat, width, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_2D:
			qglTexImage2D(GL_TEXTURE_2D, mip++, glt->image->glinternalformat, width, height, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->image->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage2D(GL_TEXTURE_2D, mip++, glt->image->glinternalformat, width, height, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_3D:
			qglTexImage3D(GL_TEXTURE_3D, mip++, glt->image->glinternalformat, width, height, depth, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
			CHECKGLERROR
			if (glt->flags & TEXF_MIPMAP)
			{
				while (width > 1 || height > 1 || depth > 1)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->image->bytesperpixel);
					prevbuffer = resizebuffer;
					qglTexImage3D(GL_TEXTURE_3D, mip++, glt->image->glinternalformat, width, height, depth, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
					CHECKGLERROR
				}
			}
			break;
		case GLTEXTURETYPE_CUBEMAP:
			// convert and upload each side in turn,
			// from a continuous block of input texels
			texturebuffer = prevbuffer;
			for (i = 0;i < 6;i++)
			{
				prevbuffer = texturebuffer;
				texturebuffer += glt->width * glt->height * glt->depth * glt->textype->inputbytesperpixel;
				if (glt->width != width || glt->height != height || glt->depth != depth)
				{
					Image_Resample(prevbuffer, glt->width, glt->height, glt->depth, resizebuffer, width, height, depth, glt->image->bytesperpixel, r_lerpimages.integer);
					prevbuffer = resizebuffer;
				}
				// picmip/max_size
				while (width > glt->image->width || height > glt->image->height || depth > glt->image->depth)
				{
					Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, glt->image->width, glt->image->height, glt->image->depth, glt->image->bytesperpixel);
					prevbuffer = resizebuffer;
				}
				mip = 0;
				qglTexImage2D(cubemapside[i], mip++, glt->image->glinternalformat, width, height, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
				CHECKGLERROR
				if (glt->flags & TEXF_MIPMAP)
				{
					while (width > 1 || height > 1 || depth > 1)
					{
						Image_MipReduce(prevbuffer, resizebuffer, &width, &height, &depth, 1, 1, 1, glt->image->bytesperpixel);
						prevbuffer = resizebuffer;
						qglTexImage2D(cubemapside[i], mip++, glt->image->glinternalformat, width, height, 0, glt->image->glformat, GL_UNSIGNED_BYTE, prevbuffer);
						CHECKGLERROR
					}
				}
			}
			break;
		}
		GL_SetupTextureParameters(glt->image->flags, glt->image->texturetype);
	}
	qglBindTexture(gltexturetypeenums[glt->image->texturetype], oldbindtexnum);
}

static void R_FindImageForTexture(gltexture_t *glt)
{
	int i, j, best, best2, x, y, z, w, h, d, picmip;
	textypeinfo_t *texinfo;
	gltexturepool_t *pool;
	gltextureimage_t *image, **imagechainpointer;
	texinfo = glt->textype;
	pool = glt->pool;

	// remains -1 until uploaded
	glt->texnum = -1;

	x = 0;
	y = 0;
	z = 0;
	w = glt->width;
	h = glt->height;
	d = glt->depth;
	if (glt->flags & TEXF_FRAGMENT)
	{
		for (imagechainpointer = &pool->imagechain;*imagechainpointer;imagechainpointer = &(*imagechainpointer)->imagechain)
		{
			image = *imagechainpointer;
			if (image->type != GLIMAGETYPE_FRAGMENTS)
				continue;
			if (image->texturetype != glt->texturetype)
				continue;
			if ((image->flags ^ glt->flags) & (TEXF_MIPMAP | TEXF_ALPHA | TEXF_CLAMP | TEXF_FORCENEAREST | TEXF_FORCELINEAR))
				continue;
			if (image->glformat != texinfo->glformat || image->glinternalformat != texinfo->glinternalformat)
				continue;
			if (glt->width > image->width || glt->height > image->height || glt->depth > image->depth)
				continue;

			// got a fragments texture, find a place in it if we can
			for (best = image->width, i = 0;i < image->width - w;i++)
			{
				for (best2 = 0, j = 0;j < w;j++)
				{
					if (image->blockallocation[i+j] >= best)
						break;
					if (best2 < image->blockallocation[i+j])
						best2 = image->blockallocation[i+j];
				}
				if (j == w)
				{
					// this is a valid spot
					x = i;
					y = best = best2;
				}
			}

			if (best + h > image->height)
				continue;

			for (i = 0;i < w;i++)
				image->blockallocation[x + i] = best + h;

			glt->x = x;
			glt->y = y;
			glt->z = 0;
			glt->image = image;
			image->texturecount++;
			return;
		}

		image = Mem_Alloc(texturemempool, sizeof(gltextureimage_t));
		if (image == NULL)
			Sys_Error("R_FindImageForTexture: ran out of memory\n");
		image->type = GLIMAGETYPE_FRAGMENTS;
		// make sure the created image is big enough for the fragment
		for (image->width = block_size;image->width < glt->width;image->width <<= 1);
		image->height = 1;
		if (gltexturetypedimensions[glt->texturetype] >= 2)
			for (image->height = block_size;image->height < glt->height;image->height <<= 1);
		image->depth = 1;
		if (gltexturetypedimensions[glt->texturetype] >= 3)
			for (image->depth = block_size;image->depth < glt->depth;image->depth <<= 1);
		image->blockallocation = Mem_Alloc(texturemempool, image->width * sizeof(short));
		memset(image->blockallocation, 0, image->width * sizeof(short));

		x = 0;
		y = 0;
		z = 0;
		for (i = 0;i < w;i++)
			image->blockallocation[x + i] = y + h;
	}
	else
	{
		for (imagechainpointer = &pool->imagechain;*imagechainpointer;imagechainpointer = &(*imagechainpointer)->imagechain);

		image = Mem_Alloc(texturemempool, sizeof(gltextureimage_t));
		if (image == NULL)
			Sys_Error("R_FindImageForTexture: ran out of memory\n");
		image->type = GLIMAGETYPE_TILE;
		image->blockallocation = NULL;

		picmip = 0;
		if (glt->flags & TEXF_PICMIP)
			picmip = gl_picmip.integer;
		// calculate final size
		if (gl_max_size.integer > realmaxsize)
			Cvar_SetValue("gl_max_size", realmaxsize);
		for (image->width = 1;image->width < glt->width;image->width <<= 1);
		for (image->height = 1;image->height < glt->height;image->height <<= 1);
		for (image->depth = 1;image->depth < glt->depth;image->depth <<= 1);
		for (image->width >>= picmip;image->width > gl_max_size.integer;image->width >>= 1);
		for (image->height >>= picmip;image->height > gl_max_size.integer;image->height >>= 1);
		for (image->depth >>= picmip;image->depth > gl_max_size.integer;image->depth >>= 1);
		if (image->width < 1) image->width = 1;
		if (image->height < 1) image->height = 1;
		if (image->depth < 1) image->depth = 1;
	}
	image->texturetype = glt->texturetype;
	image->glinternalformat = texinfo->glinternalformat;
	image->glformat = texinfo->glformat;
	image->flags = (glt->flags & (TEXF_MIPMAP | TEXF_ALPHA | TEXF_CLAMP | TEXF_PICMIP | TEXF_FORCENEAREST | TEXF_FORCELINEAR)) | GLTEXF_UPLOAD;
	image->bytesperpixel = texinfo->internalbytesperpixel;
	image->sides = image->texturetype == GLTEXTURETYPE_CUBEMAP ? 6 : 1;
	// get a texture number to use
	qglGenTextures(1, &image->texnum);
	*imagechainpointer = image;
	image->texturecount++;

	glt->x = x;
	glt->y = y;
	glt->y = z;
	glt->image = image;
}

// note: R_FindImageForTexture must be called before this
static void R_UploadTexture (gltexture_t *glt)
{
	if (!(glt->flags & GLTEXF_UPLOAD))
		return;

	R_Upload(glt, glt->inputtexels);
	if (glt->inputtexels)
	{
		Mem_Free(glt->inputtexels);
		glt->inputtexels = NULL;
		glt->flags |= GLTEXF_DESTROYED;
	}
	else if (glt->flags & GLTEXF_DESTROYED)
		Con_Printf("R_UploadTexture: Texture %s already uploaded and destroyed.  Can not upload original image again.  Uploaded blank texture.\n", glt->identifier);
}

static rtexture_t *R_SetupTexture(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, int sides, int flags, int textype, int texturetype, const qbyte *data, const unsigned int *palette)
{
	int i, size;
	gltexture_t *glt;
	gltexturepool_t *pool = (gltexturepool_t *)rtexturepool;
	textypeinfo_t *texinfo;

	if (cls.state == ca_dedicated)
		return NULL;

	if (flags & TEXF_FRAGMENT && texturetype != GLTEXTURETYPE_2D)
		Sys_Error("R_LoadTexture: only 2D fragment textures implemented\n");
	if (texturetype == GLTEXTURETYPE_CUBEMAP && !gl_texturecubemap)
		Sys_Error("R_LoadTexture: cubemap texture not supported by driver\n");
	if (texturetype == GLTEXTURETYPE_3D && !gl_texture3d)
		Sys_Error("R_LoadTexture: 3d texture not supported by driver\n");

	texinfo = R_GetTexTypeInfo(textype, flags);
	size = width * height * depth * sides * texinfo->inputbytesperpixel;
	if (size < 1)
		Sys_Error("R_LoadTexture: bogus texture size (%dx%dx%dx%dbppx%dsides = %d bytes)\n", width, height, depth, texinfo->inputbytesperpixel * 8, sides);

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
					if (((qbyte *)&palette[data[i]])[3] < 255)
					{
						flags |= TEXF_ALPHA;
						break;
					}
				}
			}
		}
		break;
	case TEXTYPE_RGB:
		if (flags & TEXF_ALPHA)
			Host_Error("R_LoadTexture: RGB has no alpha, don't specify TEXF_ALPHA\n");
		break;
	case TEXTYPE_RGBA:
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
	case TEXTYPE_DSDT:
		break;
	default:
		Host_Error("R_LoadTexture: unknown texture type\n");
	}

	glt = Mem_Alloc(texturemempool, sizeof(gltexture_t));
	if (identifier)
		strlcpy (glt->identifier, identifier, sizeof(glt->identifier));
	glt->pool = pool;
	glt->chain = pool->gltchain;
	pool->gltchain = glt;
	glt->width = width;
	glt->height = height;
	glt->depth = depth;
	glt->flags = flags | GLTEXF_UPLOAD;
	glt->textype = texinfo;
	glt->texturetype = texturetype;
	glt->inputdatasize = size;
	glt->palette = palette;

	if (data)
	{
		glt->inputtexels = Mem_Alloc(texturemempool, size);
		if (glt->inputtexels == NULL)
			Sys_Error("R_LoadTexture: out of memory\n");
		memcpy(glt->inputtexels, data, size);
	}
	else
		glt->inputtexels = NULL;

	R_FindImageForTexture(glt);
	R_PrecacheTexture(glt);

	return (rtexture_t *)glt;
}

rtexture_t *R_LoadTexture1D(rtexturepool_t *rtexturepool, const char *identifier, int width, const qbyte *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, 1, 1, 1, flags, textype, GLTEXTURETYPE_1D, data, palette);
}

rtexture_t *R_LoadTexture2D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, const qbyte *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, 1, 1, flags, textype, GLTEXTURETYPE_2D, data, palette);
}

rtexture_t *R_LoadTexture3D(rtexturepool_t *rtexturepool, const char *identifier, int width, int height, int depth, const qbyte *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, height, depth, 1, flags, textype, GLTEXTURETYPE_3D, data, palette);
}

rtexture_t *R_LoadTextureCubeMap(rtexturepool_t *rtexturepool, const char *identifier, int width, const qbyte *data, int textype, int flags, const unsigned int *palette)
{
	return R_SetupTexture(rtexturepool, identifier, width, width, 1, 6, flags, textype, GLTEXTURETYPE_CUBEMAP, data, palette);
}

int R_TextureHasAlpha(rtexture_t *rt)
{
	return rt ? (((gltexture_t *)rt)->flags & TEXF_ALPHA) != 0 : false;
}

int R_TextureWidth(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->width : 0;
}

int R_TextureHeight(rtexture_t *rt)
{
	return rt ? ((gltexture_t *)rt)->height : 0;
}

void R_FragmentLocation3D(rtexture_t *rt, int *x, int *y, int *z, float *fx1, float *fy1, float *fz1, float *fx2, float *fy2, float *fz2)
{
	gltexture_t *glt;
	float iwidth, iheight, idepth;
	if (cls.state == ca_dedicated)
	{
		if (x)
			*x = 0;
		if (y)
			*y = 0;
		if (z)
			*z = 0;
		if (fx1 || fy1 || fx2 || fy2)
		{
			if (fx1)
				*fx1 = 0;
			if (fy1)
				*fy1 = 0;
			if (fz1)
				*fz1 = 0;
			if (fx2)
				*fx2 = 1;
			if (fy2)
				*fy2 = 1;
			if (fz2)
				*fz2 = 1;
		}
		return;
	}
	if (!rt)
		Host_Error("R_FragmentLocation: no texture supplied\n");
	glt = (gltexture_t *)rt;
	if (glt->flags & TEXF_FRAGMENT)
	{
		if (x)
			*x = glt->x;
		if (y)
			*y = glt->y;
		if (fx1 || fy1 || fx2 || fy2)
		{
			iwidth = 1.0f / glt->image->width;
			iheight = 1.0f / glt->image->height;
			idepth = 1.0f / glt->image->depth;
			if (fx1)
				*fx1 = glt->x * iwidth;
			if (fy1)
				*fy1 = glt->y * iheight;
			if (fz1)
				*fz1 = glt->z * idepth;
			if (fx2)
				*fx2 = (glt->x + glt->width) * iwidth;
			if (fy2)
				*fy2 = (glt->y + glt->height) * iheight;
			if (fz2)
				*fz2 = (glt->z + glt->depth) * idepth;
		}
	}
	else
	{
		if (x)
			*x = 0;
		if (y)
			*y = 0;
		if (z)
			*z = 0;
		if (fx1 || fy1 || fx2 || fy2)
		{
			if (fx1)
				*fx1 = 0;
			if (fy1)
				*fy1 = 0;
			if (fz1)
				*fz1 = 0;
			if (fx2)
				*fx2 = 1;
			if (fy2)
				*fy2 = 1;
			if (fz2)
				*fz2 = 1;
		}
	}
}

void R_FragmentLocation(rtexture_t *rt, int *x, int *y, float *fx1, float *fy1, float *fx2, float *fy2)
{
	R_FragmentLocation3D(rt, x, y, NULL, fx1, fy1, NULL, fx2, fy2, NULL);
}

int R_CompatibleFragmentWidth(int width, int textype, int flags)
{
	return width;
}

void R_UpdateTexture(rtexture_t *rt, qbyte *data)
{
	gltexture_t *glt;
	if (rt == NULL)
		Host_Error("R_UpdateTexture: no texture supplied\n");
	if (data == NULL)
		Host_Error("R_UpdateTexture: no data supplied\n");
	glt = (gltexture_t *)rt;

	// if it has not been uploaded yet, update the data that will be used when it is
	if (glt->inputtexels)
		memcpy(glt->inputtexels, data, glt->inputdatasize);
	else
		R_Upload(glt, data);
}

