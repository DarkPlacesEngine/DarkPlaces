
#include "quakedef.h"

cvar_t gl_mesh_maxtriangles = {0, "gl_mesh_maxtriangles", "1024"};
cvar_t gl_mesh_transtriangles = {0, "gl_mesh_transtriangles", "16384"};
cvar_t gl_mesh_floatcolors = {0, "gl_mesh_floatcolors", "1"};
cvar_t gl_mesh_drawmode = {CVAR_SAVE, "gl_mesh_drawmode", "3"};

cvar_t r_render = {0, "r_render", "1"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1"}; // whether or not to use dithering
cvar_t gl_lockarrays = {0, "gl_lockarrays", "1"};

// this is used to increase gl_mesh_maxtriangles automatically if a mesh was
// too large for the buffers in the previous frame
int overflowedverts = 0;
// increase transtriangles automatically too
int overflowedtransverts = 0;

int gl_maxdrawrangeelementsvertices;
int gl_maxdrawrangeelementsindices;

#ifdef DEBUGGL
int errornumber = 0;

void GL_PrintError(int errornumber, char *filename, int linenumber)
{
	switch(errornumber)
	{
#ifdef GL_INVALID_ENUM
	case GL_INVALID_ENUM:
		Con_Printf("GL_INVALID_ENUM at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_INVALID_VALUE
	case GL_INVALID_VALUE:
		Con_Printf("GL_INVALID_VALUE at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_INVALID_OPERATION
	case GL_INVALID_OPERATION:
		Con_Printf("GL_INVALID_OPERATION at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_STACK_OVERFLOW
	case GL_STACK_OVERFLOW:
		Con_Printf("GL_STACK_OVERFLOW at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_STACK_UNDERFLOW
	case GL_STACK_UNDERFLOW:
		Con_Printf("GL_STACK_UNDERFLOW at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_OUT_OF_MEMORY
	case GL_OUT_OF_MEMORY:
		Con_Printf("GL_OUT_OF_MEMORY at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_TABLE_TOO_LARGE
    case GL_TABLE_TOO_LARGE:
		Con_Printf("GL_TABLE_TOO_LARGE at %s:%i\n", filename, linenumber);
		break;
#endif
	default:
		Con_Printf("GL UNKNOWN (%i) at %s:%i\n", errornumber, filename, linenumber);
		break;
	}
}
#endif

float r_mesh_farclip;

static float viewdist;
// sign bits (true if negative) for vpn[] entries, so quick integer compares can be used instead of float compares
static int vpnbit0, vpnbit1, vpnbit2;

int c_meshs, c_meshtris, c_transmeshs, c_transtris;

int lightscalebit;
float lightscale;
float overbrightscale;

void SCR_ScreenShot_f (void);

static int max_meshs;
static int max_transmeshs;
static int max_verts; // always max_meshs * 3
static int max_transverts; // always max_transmeshs * 3
#define TRANSDEPTHRES 4096

typedef struct buf_mesh_s
{
	int depthmask;
	int depthtest;
	int blendfunc1, blendfunc2;
	int textures[MAX_TEXTUREUNITS];
	int texturergbscale[MAX_TEXTUREUNITS];
	int firsttriangle;
	int triangles;
	int firstvert;
	int verts;
	struct buf_mesh_s *chain;
	struct buf_transtri_s *transchain;
}
buf_mesh_t;

typedef struct buf_transtri_s
{
	struct buf_transtri_s *next;
	struct buf_transtri_s *meshsortchain;
	buf_mesh_t *mesh;
	int index[3];
}
buf_transtri_t;

typedef struct buf_tri_s
{
	int index[3];
}
buf_tri_t;

typedef struct
{
	float v[4];
}
buf_vertex_t;

typedef struct
{
	float c[4];
}
buf_fcolor_t;

typedef struct
{
	qbyte c[4];
}
buf_bcolor_t;

typedef struct
{
	float t[2];
}
buf_texcoord_t;

static int currentmesh, currenttriangle, currentvertex, backendunits, backendactive, transranout;
static buf_mesh_t *buf_mesh;
static buf_tri_t *buf_tri;
static buf_vertex_t *buf_vertex;
static buf_fcolor_t *buf_fcolor;
static buf_bcolor_t *buf_bcolor;
static buf_texcoord_t *buf_texcoord[MAX_TEXTUREUNITS];

static int currenttransmesh, currenttransvertex, currenttranstriangle;
static buf_mesh_t *buf_transmesh;
static buf_transtri_t *buf_sorttranstri;
static buf_transtri_t **buf_sorttranstri_list;
static buf_tri_t *buf_transtri;
static buf_vertex_t *buf_transvertex;
static buf_fcolor_t *buf_transfcolor;
static buf_texcoord_t *buf_transtexcoord[MAX_TEXTUREUNITS];

static mempool_t *gl_backend_mempool;
static int resizingbuffers = false;

static void gl_backend_start(void)
{
	int i;

	qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
	qglGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_maxdrawrangeelementsindices);

	Con_Printf("OpenGL Backend started with gl_mesh_maxtriangles %i, gl_mesh_transtriangles %i\n", gl_mesh_maxtriangles.integer, gl_mesh_transtriangles.integer);
	if (qglDrawRangeElements != NULL)
		Con_Printf("glDrawRangeElements detected (max vertices %i, max indices %i)\n", gl_maxdrawrangeelementsvertices, gl_maxdrawrangeelementsindices);
	if (strstr(gl_renderer, "3Dfx"))
	{
		Con_Printf("3Dfx driver detected, forcing gl_mesh_floatcolors to 0 to prevent crashs\n");
		Cvar_SetValueQuick(&gl_mesh_floatcolors, 0);
	}
	Con_Printf("\n");

	max_verts = max_meshs * 3;
	max_transverts = max_transmeshs * 3;

	if (!gl_backend_mempool)
		gl_backend_mempool = Mem_AllocPool("GL_Backend");

#define BACKENDALLOC(var, count, sizeofstruct, varname)\
	{\
		var = Mem_Alloc(gl_backend_mempool, count * sizeof(sizeofstruct));\
		if (var == NULL)\
			Sys_Error("gl_backend_start: unable to allocate memory for %s (%d bytes)\n", (varname), count * sizeof(sizeofstruct));\
		memset(var, 0, count * sizeof(sizeofstruct));\
	}

	BACKENDALLOC(buf_mesh, max_meshs, buf_mesh_t, "buf_mesh")
	BACKENDALLOC(buf_tri, max_meshs, buf_tri_t, "buf_tri")
	BACKENDALLOC(buf_vertex, max_verts, buf_vertex_t, "buf_vertex")
	BACKENDALLOC(buf_fcolor, max_verts, buf_fcolor_t, "buf_fcolor")
	BACKENDALLOC(buf_bcolor, max_verts, buf_bcolor_t, "buf_bcolor")

	BACKENDALLOC(buf_transmesh, max_transmeshs, buf_mesh_t, "buf_transmesh")
	BACKENDALLOC(buf_sorttranstri, max_transmeshs, buf_transtri_t, "buf_sorttranstri")
	BACKENDALLOC(buf_sorttranstri_list, TRANSDEPTHRES, buf_transtri_t *, "buf_sorttranstri_list")
	BACKENDALLOC(buf_transtri, max_transmeshs, buf_tri_t, "buf_transtri")
	BACKENDALLOC(buf_transvertex, max_transverts, buf_vertex_t, "buf_vertex")
	BACKENDALLOC(buf_transfcolor, max_transverts, buf_fcolor_t, "buf_fcolor")

	for (i = 0;i < MAX_TEXTUREUNITS;i++)
	{
		// only allocate as many texcoord arrays as we need
		if (i < gl_textureunits)
		{
			BACKENDALLOC(buf_texcoord[i], max_verts, buf_texcoord_t, va("buf_texcoord[%d]", i))
			BACKENDALLOC(buf_transtexcoord[i], max_transverts, buf_texcoord_t, va("buf_transtexcoord[%d]", i))
		}
		else
		{
			buf_texcoord[i] = NULL;
			buf_transtexcoord[i] = NULL;
		}
	}
	backendunits = min(MAX_TEXTUREUNITS, gl_textureunits);
	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	Con_Printf("OpenGL Backend shutting down\n");

	if (resizingbuffers)
		Mem_EmptyPool(gl_backend_mempool);
	else
		Mem_FreePool(&gl_backend_mempool);

	backendunits = 0;
	backendactive = false;
}

static void gl_backend_bufferchanges(int init)
{
	if (overflowedverts > gl_mesh_maxtriangles.integer * 3)
		Cvar_SetValueQuick(&gl_mesh_maxtriangles, (int) ((overflowedverts + 2) / 3));
	overflowedverts = 0;

	if (overflowedtransverts > gl_mesh_transtriangles.integer * 3)
		Cvar_SetValueQuick(&gl_mesh_transtriangles, (int) ((overflowedtransverts + 2) / 3));
	overflowedtransverts = 0;

	if (gl_mesh_drawmode.integer < 0)
		Cvar_SetValueQuick(&gl_mesh_drawmode, 0);
	if (gl_mesh_drawmode.integer > 3)
		Cvar_SetValueQuick(&gl_mesh_drawmode, 3);

	if (gl_mesh_drawmode.integer >= 3 && qglDrawRangeElements == NULL)
	{
		// change drawmode 3 to 2 if 3 won't work at all
		Cvar_SetValueQuick(&gl_mesh_drawmode, 2);
	}

	// 21760 is (65536 / 3) rounded off to a multiple of 128
	if (gl_mesh_maxtriangles.integer < 1024)
		Cvar_SetValueQuick(&gl_mesh_maxtriangles, 1024);
	if (gl_mesh_maxtriangles.integer > 21760)
		Cvar_SetValueQuick(&gl_mesh_maxtriangles, 21760);

	if (gl_mesh_transtriangles.integer < 1024)
		Cvar_SetValueQuick(&gl_mesh_transtriangles, 1024);
	if (gl_mesh_transtriangles.integer > 65536)
		Cvar_SetValueQuick(&gl_mesh_transtriangles, 65536);

	if (max_meshs != gl_mesh_maxtriangles.integer || max_transmeshs != gl_mesh_transtriangles.integer)
	{
		max_meshs = gl_mesh_maxtriangles.integer;
		max_transmeshs = gl_mesh_transtriangles.integer;

		if (!init)
		{
			resizingbuffers = true;
			gl_backend_shutdown();
			gl_backend_start();
			resizingbuffers = false;
		}
	}
}

static void gl_backend_newmap(void)
{
}

void gl_backend_init(void)
{
	Cvar_RegisterVariable(&r_render);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_lockarrays);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

	Cvar_RegisterVariable(&gl_mesh_maxtriangles);
	Cvar_RegisterVariable(&gl_mesh_transtriangles);
	Cvar_RegisterVariable(&gl_mesh_floatcolors);
	Cvar_RegisterVariable(&gl_mesh_drawmode);
	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
	gl_backend_bufferchanges(true);
}

int arraylocked = false;

void GL_LockArray(int first, int count)
{
	if (!arraylocked && gl_supportslockarrays && gl_lockarrays.integer && gl_mesh_drawmode.integer > 0)
	{
		qglLockArraysEXT(first, count);
		CHECKGLERROR
		arraylocked = true;
	}
}

void GL_UnlockArray(void)
{
	if (arraylocked)
	{
		qglUnlockArraysEXT();
		CHECKGLERROR
		arraylocked = false;
	}
}

/*
=============
GL_SetupFrame
=============
*/
static void GL_SetupFrame (void)
{
	double xmax, ymax;
	double fovx, fovy, zNear, zFar, aspect;

	if (!r_render.integer)
		return;

	qglDepthFunc (GL_LEQUAL);CHECKGLERROR

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity ();CHECKGLERROR

	// y is weird beause OpenGL is bottom to top, we use top to bottom
	qglViewport(r_refdef.x, vid.realheight - (r_refdef.y + r_refdef.height), r_refdef.width, r_refdef.height);CHECKGLERROR

	// depth range
	zNear = 1.0;
	zFar = r_mesh_farclip;

	// fov angles
	fovx = r_refdef.fov_x;
	fovy = r_refdef.fov_y;
	aspect = r_refdef.width / r_refdef.height;

	// pyramid slopes
	xmax = zNear * tan(fovx * M_PI / 360.0) * aspect;
	ymax = zNear * tan(fovy * M_PI / 360.0);

	// set view pyramid
	qglFrustum(-xmax, xmax, -ymax, ymax, zNear, zFar);CHECKGLERROR

	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	qglLoadIdentity ();CHECKGLERROR

	// put Z going up
	qglRotatef (-90,  1, 0, 0);CHECKGLERROR
	qglRotatef (90,  0, 0, 1);CHECKGLERROR
	// camera rotation
	qglRotatef (-r_refdef.viewangles[2],  1, 0, 0);CHECKGLERROR
	qglRotatef (-r_refdef.viewangles[0],  0, 1, 0);CHECKGLERROR
	qglRotatef (-r_refdef.viewangles[1],  0, 0, 1);CHECKGLERROR
	// camera location
	qglTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);CHECKGLERROR
}

static struct
{
	int blendfunc1;
	int blendfunc2;
	int blend;
	GLboolean depthmask;
	int depthtest;
	int unit;
	int clientunit;
	int texture[MAX_TEXTUREUNITS];
	float texturergbscale[MAX_TEXTUREUNITS];
}
gl_state;

void GL_SetupTextureState(void)
{
	int i;
	if (backendunits > 1)
	{
		for (i = 0;i < backendunits;i++)
		{
			qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			qglBindTexture(GL_TEXTURE_2D, gl_state.texture[i]);CHECKGLERROR
			if (gl_combine.integer)
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_CONSTANT_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_CONSTANT_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, gl_state.texturergbscale[i]);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);CHECKGLERROR
			}
			else
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
			}
			if (gl_state.texture[i])
			{
				qglEnable(GL_TEXTURE_2D);CHECKGLERROR
			}
			else
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
			if (gl_mesh_drawmode.integer > 0)
			{
				qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
				qglTexCoordPointer(2, GL_FLOAT, sizeof(buf_texcoord_t), buf_texcoord[i]);CHECKGLERROR
				if (gl_state.texture[i])
				{
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
				else
				{
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
		}
	}
	else
	{
		qglBindTexture(GL_TEXTURE_2D, gl_state.texture[0]);CHECKGLERROR
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
		if (gl_state.texture[0])
		{
			qglEnable(GL_TEXTURE_2D);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_TEXTURE_2D);CHECKGLERROR
		}
		if (gl_mesh_drawmode.integer > 0)
		{
			qglTexCoordPointer(2, GL_FLOAT, sizeof(buf_texcoord_t), buf_texcoord[0]);CHECKGLERROR
			if (gl_state.texture[0])
			{
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			else
			{
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
		}
	}
}

// called at beginning of frame
int usedarrays;
void R_Mesh_Start(float farclip)
{
	int i;
	if (!backendactive)
		Sys_Error("R_Mesh_Clear: called when backend is not active\n");

	CHECKGLERROR

	gl_backend_bufferchanges(false);

	currentmesh = 0;
	currenttriangle = 0;
	currentvertex = 0;
	currenttransmesh = 0;
	currenttranstriangle = 0;
	currenttransvertex = 0;
	transranout = false;
	r_mesh_farclip = farclip;
	viewdist = DotProduct(r_origin, vpn);
	vpnbit0 = vpn[0] < 0;
	vpnbit1 = vpn[1] < 0;
	vpnbit2 = vpn[2] < 0;

	c_meshs = 0;
	c_meshtris = 0;
	c_transmeshs = 0;
	c_transtris = 0;

	GL_SetupFrame();

	gl_state.unit = 0;
	gl_state.clientunit = 0;

	for (i = 0;i < backendunits;i++)
	{
		gl_state.texture[i] = 0;
		gl_state.texturergbscale[i] = 1;
	}

	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglCullFace(GL_FRONT);CHECKGLERROR

	gl_state.depthtest = true;
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR

	gl_state.blendfunc1 = GL_ONE;
	gl_state.blendfunc2 = GL_ZERO;
	qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR

	gl_state.blend = 0;
	qglDisable(GL_BLEND);CHECKGLERROR

	gl_state.depthmask = GL_TRUE;
	qglDepthMask(gl_state.depthmask);CHECKGLERROR

	usedarrays = false;
	if (gl_mesh_drawmode.integer > 0)
	{
		usedarrays = true;
		qglVertexPointer(3, GL_FLOAT, sizeof(buf_vertex_t), &buf_vertex[0].v[0]);CHECKGLERROR
		qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
		if (gl_mesh_floatcolors.integer)
		{
			qglColorPointer(4, GL_FLOAT, sizeof(buf_fcolor_t), &buf_fcolor[0].c[0]);CHECKGLERROR
		}
		else
		{
			qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(buf_bcolor_t), &buf_bcolor[0].c[0]);CHECKGLERROR
		}
		qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	}

	GL_SetupTextureState();
}

int gl_backend_rebindtextures;

void GL_ConvertColorsFloatToByte(void)
{
	int i, k, total;
	// LordHavoc: to avoid problems with aliasing (treating memory as two
	// different types - exactly what this is doing), these must be volatile
	// (or a union)
	volatile int *icolor;
	volatile float *fcolor;
	qbyte *bcolor;

	total = currentvertex * 4;

	// shift float to have 8bit fraction at base of number
	fcolor = &buf_fcolor->c[0];
	for (i = 0;i < total;)
	{
		fcolor[i    ] += 32768.0f;
		fcolor[i + 1] += 32768.0f;
		fcolor[i + 2] += 32768.0f;
		fcolor[i + 3] += 32768.0f;
		i += 4;
	}

	// then read as integer and kill float bits...
	icolor = (int *)&buf_fcolor->c[0];
	bcolor = &buf_bcolor->c[0];
	for (i = 0;i < total;)
	{
		k = icolor[i    ] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i    ] = (qbyte) k;
		k = icolor[i + 1] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 1] = (qbyte) k;
		k = icolor[i + 2] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 2] = (qbyte) k;
		k = icolor[i + 3] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 3] = (qbyte) k;
		i += 4;
	}
}

void GL_MeshState(buf_mesh_t *mesh)
{
	int i;
	if (backendunits > 1)
	{
		for (i = 0;i < backendunits;i++)
		{
			if (gl_state.texture[i] != mesh->textures[i])
			{
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (gl_state.texture[i] == 0)
				{
					qglEnable(GL_TEXTURE_2D);CHECKGLERROR
					// have to disable texcoord array on disabled texture
					// units due to NVIDIA driver bug with
					// compiled_vertex_array
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_2D, (gl_state.texture[i] = mesh->textures[i]));CHECKGLERROR
				if (gl_state.texture[i] == 0)
				{
					qglDisable(GL_TEXTURE_2D);CHECKGLERROR
					// have to disable texcoord array on disabled texture
					// units due to NVIDIA driver bug with
					// compiled_vertex_array
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			if (gl_state.texturergbscale[i] != mesh->texturergbscale[i])
			{
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (gl_state.texturergbscale[i] = mesh->texturergbscale[i]));CHECKGLERROR
			}
		}
	}
	else
	{
		if (gl_state.texture[0] != mesh->textures[0])
		{
			if (gl_state.texture[0] == 0)
			{
				qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			qglBindTexture(GL_TEXTURE_2D, (gl_state.texture[0] = mesh->textures[0]));CHECKGLERROR
			if (gl_state.texture[0] == 0)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
		}
	}
	if (gl_state.blendfunc1 != mesh->blendfunc1 || gl_state.blendfunc2 != mesh->blendfunc2)
	{
		qglBlendFunc(gl_state.blendfunc1 = mesh->blendfunc1, gl_state.blendfunc2 = mesh->blendfunc2);CHECKGLERROR
		if (gl_state.blendfunc2 == GL_ZERO)
		{
			if (gl_state.blendfunc1 == GL_ONE)
			{
				if (gl_state.blend)
				{
					gl_state.blend = 0;
					qglDisable(GL_BLEND);CHECKGLERROR
				}
			}
			else
			{
				if (!gl_state.blend)
				{
					gl_state.blend = 1;
					qglEnable(GL_BLEND);CHECKGLERROR
				}
			}
		}
		else
		{
			if (!gl_state.blend)
			{
				gl_state.blend = 1;
				qglEnable(GL_BLEND);CHECKGLERROR
			}
		}
	}
	if (gl_state.depthtest != mesh->depthtest)
	{
		gl_state.depthtest = mesh->depthtest;
		if (gl_state.depthtest)
			qglEnable(GL_DEPTH_TEST);
		else
			qglDisable(GL_DEPTH_TEST);
	}
	if (gl_state.depthmask != mesh->depthmask)
	{
		qglDepthMask(gl_state.depthmask = mesh->depthmask);CHECKGLERROR
	}
}

void GL_DrawRangeElements(int firstvert, int endvert, int indexcount, GLuint *index)
{
	unsigned int i, j, in;
	if (gl_mesh_drawmode.integer >= 3/* && (endvert - firstvert) <= gl_maxdrawrangeelementsvertices && (indexcount) <= gl_maxdrawrangeelementsindices*/)
	{
		// GL 1.2 or GL 1.1 with extension
		qglDrawRangeElements(GL_TRIANGLES, firstvert, endvert, indexcount, GL_UNSIGNED_INT, index);
	}
	else if (gl_mesh_drawmode.integer >= 2)
	{
		// GL 1.1
		qglDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, index);
	}
	else if (gl_mesh_drawmode.integer >= 1)
	{
		// GL 1.1
		// feed it manually using glArrayElement
		qglBegin(GL_TRIANGLES);
		for (i = 0;i < indexcount;i++)
			qglArrayElement(index[i]);
		qglEnd();
	}
	else
	{
		// GL 1.1 but not using vertex arrays - 3dfx glquake minigl driver
		// feed it manually
		qglBegin(GL_TRIANGLES);
		if (gl_state.texture[1]) // if the mesh uses multiple textures
		{
			// the minigl doesn't have this (because it does not have ARB_multitexture)
			for (i = 0;i < indexcount;i++)
			{
				in = index[i];
				qglColor4ub(buf_bcolor[in].c[0], buf_bcolor[in].c[1], buf_bcolor[in].c[2], buf_bcolor[in].c[3]);
				for (j = 0;j < backendunits;j++)
					if (gl_state.texture[j])
						qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, buf_texcoord[j][in].t[0], buf_texcoord[j][in].t[1]);
				qglVertex3f(buf_vertex[in].v[0], buf_vertex[in].v[1], buf_vertex[in].v[2]);
			}
		}
		else
		{
			for (i = 0;i < indexcount;i++)
			{
				in = index[i];
				qglColor4ub(buf_bcolor[in].c[0], buf_bcolor[in].c[1], buf_bcolor[in].c[2], buf_bcolor[in].c[3]);
				if (gl_state.texture[0])
					qglTexCoord2f(buf_texcoord[0][in].t[0], buf_texcoord[0][in].t[1]);
				qglVertex3f(buf_vertex[in].v[0], buf_vertex[in].v[1], buf_vertex[in].v[2]);
			}
		}
		qglEnd();
	}
}

// renders mesh buffers, called to flush buffers when full
void R_Mesh_Render(void)
{
	int k;
	buf_mesh_t *mesh;

	if (!backendactive)
		Sys_Error("R_Mesh_Render: called when backend is not active\n");

	if (!currentmesh)
		return;

	if (!r_render.integer)
	{
		currentmesh = 0;
		currenttriangle = 0;
		currentvertex = 0;
		return;
	}

	CHECKGLERROR

	// drawmode 0 always uses byte colors
	if (!gl_mesh_floatcolors.integer || gl_mesh_drawmode.integer <= 0)
		GL_ConvertColorsFloatToByte();

	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
	}

	GL_MeshState(buf_mesh);
	GL_LockArray(0, currentvertex);
	GL_DrawRangeElements(buf_mesh->firstvert, buf_mesh->firstvert + buf_mesh->verts, buf_mesh->triangles * 3, (unsigned int *)&buf_tri[buf_mesh->firsttriangle].index[0]);CHECKGLERROR

	if (currentmesh >= 2)
	{
		for (k = 1, mesh = buf_mesh + k;k < currentmesh;k++, mesh++)
		{
			GL_MeshState(mesh);
			GL_DrawRangeElements(mesh->firstvert, mesh->firstvert + mesh->verts, mesh->triangles * 3, buf_tri[mesh->firsttriangle].index);CHECKGLERROR
		}
	}

	currentmesh = 0;
	currenttriangle = 0;
	currentvertex = 0;

	GL_UnlockArray();CHECKGLERROR
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	int i;
	// flush any queued meshs
	if (currentmesh)
		R_Mesh_Render();

	if (backendunits > 1)
	{
		for (i = backendunits - 1;i >= 0;i--)
		{
			qglActiveTexture(GL_TEXTURE0_ARB + i);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
			if (gl_combine.integer)
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);CHECKGLERROR
			}
			if (i > 0)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
			else
			{
				qglEnable(GL_TEXTURE_2D);CHECKGLERROR
			}
			qglBindTexture(GL_TEXTURE_2D, 0);CHECKGLERROR

			if (usedarrays)
			{
				qglClientActiveTexture(GL_TEXTURE0_ARB + i);CHECKGLERROR
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
		}
	}
	else
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
		qglEnable(GL_TEXTURE_2D);CHECKGLERROR
		if (usedarrays)
		{
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
	if (usedarrays)
	{
		qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
		qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
	}

	qglDisable(GL_BLEND);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglDepthMask(GL_TRUE);CHECKGLERROR
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);CHECKGLERROR
}

void R_Mesh_ClearDepth(void)
{
	if (currenttransmesh)
		R_Mesh_AddTransparent();
	if (currentmesh)
		R_Mesh_Render();
	R_Mesh_Finish();
	qglClear(GL_DEPTH_BUFFER_BIT);
	R_Mesh_Start(r_mesh_farclip);
}

void R_Mesh_AddTransparent(void)
{
	int i, j, k, *index;
	float viewdistcompare, centerscaler, dist1, dist2, dist3, center, maxdist;
	buf_vertex_t *vert1, *vert2, *vert3;
	buf_transtri_t *tri;
	buf_mesh_t *mesh, *transmesh;

	if (!currenttransmesh)
		return;

	// convert index data to transtris for sorting
	for (j = 0;j < currenttransmesh;j++)
	{
		mesh = buf_transmesh + j;
		k = mesh->firsttriangle;
		index = &buf_transtri[k].index[0];
		for (i = 0;i < mesh->triangles;i++)
		{
			tri = &buf_sorttranstri[k++];
			tri->mesh = mesh;
			tri->index[0] = *index++;
			tri->index[1] = *index++;
			tri->index[2] = *index++;
		}
	}

	// map farclip to 0-4095 list range
	centerscaler = (TRANSDEPTHRES / r_mesh_farclip) * (1.0f / 3.0f);
	viewdistcompare = viewdist + 4.0f;

	memset(buf_sorttranstri_list, 0, TRANSDEPTHRES * sizeof(buf_transtri_t *));

	k = 0;
	for (j = 0;j < currenttranstriangle;j++)
	{
		tri = &buf_sorttranstri[j];
		i = tri->mesh->firstvert;

		vert1 = &buf_transvertex[tri->index[0] + i];
		vert2 = &buf_transvertex[tri->index[1] + i];
		vert3 = &buf_transvertex[tri->index[2] + i];

		dist1 = DotProduct(vert1->v, vpn);
		dist2 = DotProduct(vert2->v, vpn);
		dist3 = DotProduct(vert3->v, vpn);

		maxdist = max(dist1, max(dist2, dist3));
		if (maxdist < viewdistcompare)
			continue;

		center = (dist1 + dist2 + dist3) * centerscaler - viewdist;
#if SLOWMATH
		i = (int) center;
		i = bound(0, i, (TRANSDEPTHRES - 1));
#else
		if (center < 0.0f)
			center = 0.0f;
		center += 8388608.0f;
		i = *((int *)&center) & 0x7FFFFF;
		i = min(i, (TRANSDEPTHRES - 1));
#endif
		tri->next = buf_sorttranstri_list[i];
		buf_sorttranstri_list[i] = tri;
		k++;
	}

	for (i = 0;i < currenttransmesh;i++)
		buf_transmesh[i].transchain = NULL;
	transmesh = NULL;
	for (j = 0;j < TRANSDEPTHRES;j++)
	{
		if ((tri = buf_sorttranstri_list[j]))
		{
			for (;tri;tri = tri->next)
			{
				if (!tri->mesh->transchain)
				{
					tri->mesh->chain = transmesh;
					transmesh = tri->mesh;
				}
				tri->meshsortchain = tri->mesh->transchain;
				tri->mesh->transchain = tri;
			}
		}
	}

	R_Mesh_Render();
	for (;transmesh;transmesh = transmesh->chain)
	{
		mesh = &buf_mesh[currentmesh++];
		*mesh = *transmesh; // copy mesh properties

		mesh->firstvert = currentvertex;
		memcpy(&buf_vertex[currentvertex], &buf_transvertex[transmesh->firstvert], transmesh->verts * sizeof(buf_vertex_t));
		memcpy(&buf_fcolor[currentvertex], &buf_transfcolor[transmesh->firstvert], transmesh->verts * sizeof(buf_fcolor_t));
		for (i = 0;i < backendunits && transmesh->textures[i];i++)
			memcpy(&buf_texcoord[i][currentvertex], &buf_transtexcoord[i][transmesh->firstvert], transmesh->verts * sizeof(buf_texcoord_t));
		currentvertex += mesh->verts;

		mesh->firsttriangle = currenttriangle;
		for (tri = transmesh->transchain;tri;tri = tri->meshsortchain)
		{
			buf_tri[currenttriangle].index[0] = tri->index[0];
			buf_tri[currenttriangle].index[1] = tri->index[1];
			buf_tri[currenttriangle].index[2] = tri->index[2];
			currenttriangle++;
		}
		mesh->triangles = currenttriangle - mesh->firsttriangle;
		R_Mesh_Render();
	}

	currenttransmesh = 0;
	currenttranstriangle = 0;
	currenttransvertex = 0;
}

// allocates space in geometry buffers, and fills in pointers to the buffers in passsed struct
// (this is used for very high speed rendering, no copying)
int R_Mesh_Draw_GetBuffer(rmeshbufferinfo_t *m, int wantoverbright)
{
	// these are static because gcc runs out of virtual registers otherwise
	int i, j, overbright;
	float scaler;
	buf_mesh_t *mesh;

	if (!backendactive)
		Sys_Error("R_Mesh_Draw_GetBuffer: called when backend is not active\n");

	if (!m->numtriangles
	 || !m->numverts)
		Host_Error("R_Mesh_Draw_GetBuffer: no triangles or verts\n");

	// LordHavoc: removed this error condition because with floatcolors 0,
	// the 3DFX driver works with very large meshs
	// FIXME: we can work around this by falling back on non-array renderer if buffers are too big
	//if (m->numtriangles > 1024 || m->numverts > 3072)
	//{
	//	Con_Printf("R_Mesh_Draw_GetBuffer: mesh too big for 3DFX drivers, rejected\n");
	//	return false;
	//}

	i = max(m->numtriangles * 3, m->numverts);
	if (overflowedverts < i)
		overflowedverts = i;

	if (m->numtriangles > max_meshs || m->numverts > max_verts)
	{
		Con_Printf("R_Mesh_Draw_GetBuffer: mesh too big for current gl_mesh_maxtriangles setting, increasing limits\n");
		return false;
	}

	if (m->transparent)
	{
		overflowedtransverts += max(m->numtriangles * 3, m->numverts);
		if (currenttransmesh >= max_transmeshs || (currenttranstriangle + m->numtriangles) > max_transmeshs || (currenttransvertex + m->numverts) > max_transverts)
		{
			if (!transranout)
			{
				Con_Printf("R_Mesh_Draw_GetBuffer: ran out of room for transparent meshs\n");
				transranout = true;
			}
			return false;
		}

		c_transmeshs++;
		c_transtris += m->numtriangles;
		m->index = &buf_transtri[currenttranstriangle].index[0];
		m->vertex = &buf_transvertex[currenttransvertex].v[0];
		m->color = &buf_transfcolor[currenttransvertex].c[0];
		for (i = 0;i < backendunits;i++)
			m->texcoords[i] = &buf_transtexcoord[i][currenttransvertex].t[0];

		// transmesh is only for storage of transparent meshs until they
		// are inserted into the main mesh array
		mesh = &buf_transmesh[currenttransmesh++];
		mesh->firsttriangle = currenttranstriangle;
		mesh->firstvert = currenttransvertex;
		currenttranstriangle += m->numtriangles;
		currenttransvertex += m->numverts;
	}
	else
	{
		if (currentmesh)
		{
			R_Mesh_Render();
			Con_Printf("mesh queue not empty, flushing.\n");
		}

		c_meshs++;
		c_meshtris += m->numtriangles;
		m->index = &buf_tri[currenttriangle].index[0];
		m->vertex = &buf_vertex[currentvertex].v[0];
		m->color = &buf_fcolor[currentvertex].c[0];
		for (i = 0;i < backendunits;i++)
			m->texcoords[i] = &buf_texcoord[i][currentvertex].t[0];

		// opaque meshs are rendered directly
		mesh = &buf_mesh[currentmesh++];
		mesh->firsttriangle = currenttriangle;
		mesh->firstvert = currentvertex;
		currenttriangle += m->numtriangles;
		currentvertex += m->numverts;
	}

	// code shared for transparent and opaque meshs
	mesh->blendfunc1 = m->blendfunc1;
	mesh->blendfunc2 = m->blendfunc2;
	mesh->depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite);
	mesh->depthtest = !m->depthdisable;
	mesh->triangles = m->numtriangles;
	mesh->verts = m->numverts;

	overbright = false;
	scaler = 1;
	if (m->blendfunc1 == GL_DST_COLOR)
	{
		// check if it is a 2x modulate with framebuffer
		if (m->blendfunc2 == GL_SRC_COLOR)
			scaler *= 0.5f;
	}
	else if (m->blendfunc2 != GL_SRC_COLOR)
	{
		if (m->tex[0])
		{
			overbright = wantoverbright && gl_combine.integer;
			if (overbright)
				scaler *= 0.25f;
		}
		scaler *= overbrightscale;
	}
	m->colorscale = scaler;

	j = -1;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
	{
		if ((mesh->textures[i] = m->tex[i]))
		{
			j = i;
			if (i >= backendunits)
				Sys_Error("R_Mesh_Draw_GetBuffer: texture %i supplied when there are only %i texture units\n", j + 1, backendunits);
		}
		mesh->texturergbscale[i] = m->texrgbscale[i];
		if (mesh->texturergbscale[i] != 1 && mesh->texturergbscale[i] != 2 && mesh->texturergbscale[i] != 4)
			mesh->texturergbscale[i] = 1;
	}
	if (overbright && j >= 0)
		mesh->texturergbscale[j] = 4;

	return true;
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height)
{
	qboolean ret;
	int i;
	qbyte *buffer;

	if (!r_render.integer)
		return false;

	buffer = Mem_Alloc(tempmempool, width*height*3);
	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	CHECKGLERROR

	// LordHavoc: compensate for v_overbrightbits when using hardware gamma
	if (v_hwgamma.integer)
		for (i = 0;i < width * height * 3;i++)
			buffer[i] <<= v_overbrightbits.integer;

	ret = Image_WriteTGARGB_preflipped(filename, width, height, buffer);

	Mem_Free(buffer);
	return ret;
}

//=============================================================================

void R_ClearScreen(void)
{
	if (r_render.integer)
	{
		// clear to black
		qglClearColor(0,0,0,0);CHECKGLERROR
		// clear the screen
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);CHECKGLERROR
		// set dithering mode
		if (gl_dither.integer)
		{
			qglEnable(GL_DITHER);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_DITHER);CHECKGLERROR
		}
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	VID_Finish ();

	R_TimeReport("finish");

	if (r_textureunits.integer > gl_textureunits)
		Cvar_SetValueQuick(&r_textureunits, gl_textureunits);
	if (r_textureunits.integer < 1)
		Cvar_SetValueQuick(&r_textureunits, 1);

	if (gl_combine.integer && (!gl_combine_extension || r_textureunits.integer < 2))
		Cvar_SetValueQuick(&gl_combine, 0);

	// lighting scale
	overbrightscale = 1.0f / (float) (1 << v_overbrightbits.integer);

	// lightmaps only
	lightscalebit = v_overbrightbits.integer;
	if (gl_combine.integer && r_textureunits.integer > 1)
		lightscalebit += 2;
	lightscale = 1.0f / (float) (1 << lightscalebit);

	R_TimeReport("setup");

	R_ClearScreen();

	R_TimeReport("clear");

	if (scr_conlines < vid.conheight)
		R_RenderView();

	// draw 2D stuff
	R_DrawQueue();

	// tell driver to commit it's partially full geometry queue to the rendering queue
	// (this doesn't wait for the commands themselves to complete)
	qglFlush();
}

