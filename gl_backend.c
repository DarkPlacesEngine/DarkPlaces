
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"

//#define MESH_VAR
#define MESH_BATCH

// 65536 is the max addressable on a Geforce 256 up until Geforce3
// (excluding MX), seems a reasonable number...
cvar_t gl_mesh_maxverts = {0, "gl_mesh_maxverts", "65536"};
cvar_t gl_mesh_floatcolors = {0, "gl_mesh_floatcolors", "1"};
cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1"};
#ifdef MESH_VAR
cvar_t gl_mesh_vertex_array_range = {0, "gl_mesh_vertex_array_range", "0"};
cvar_t gl_mesh_vertex_array_range_readfrequency = {0, "gl_mesh_vertex_array_range_readfrequency", "0.2"};
cvar_t gl_mesh_vertex_array_range_writefrequency = {0, "gl_mesh_vertex_array_range_writefrequency", "0.2"};
cvar_t gl_mesh_vertex_array_range_priority = {0, "gl_mesh_vertex_array_range_priority", "0.7"};
#endif
#ifdef MESH_BATCH
cvar_t gl_mesh_batching = {0, "gl_mesh_batching", "1"};
#endif
cvar_t gl_mesh_copyarrays = {0, "gl_mesh_copyarrays", "1"};
cvar_t gl_delayfinish = {CVAR_SAVE, "gl_delayfinish", "0"};

cvar_t r_render = {0, "r_render", "1"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1"}; // whether or not to use dithering
cvar_t gl_lockarrays = {0, "gl_lockarrays", "1"};

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

#define BACKENDACTIVECHECK if (!backendactive) Sys_Error("GL backend function called when backend is not active\n");

int c_meshs, c_meshelements;

void SCR_ScreenShot_f (void);

// these are externally accessible
int r_lightmapscalebit;
float r_colorscale;
GLfloat *varray_vertex3f, *varray_buf_vertex3f;
GLfloat *varray_color4f, *varray_buf_color4f;
GLfloat *varray_texcoord3f[MAX_TEXTUREUNITS], *varray_buf_texcoord3f[MAX_TEXTUREUNITS];
GLfloat *varray_texcoord2f[MAX_TEXTUREUNITS], *varray_buf_texcoord2f[MAX_TEXTUREUNITS];
static qbyte *varray_buf_color4b;
int mesh_maxverts;
#ifdef MESH_VAR
int mesh_var;
float mesh_var_readfrequency;
float mesh_var_writefrequency;
float mesh_var_priority;
#endif
int varray_offset = 0, varray_offsetnext = 0;
GLuint *varray_buf_elements3i;
int mesh_maxelements = 32768;
#ifdef MESH_BATCH
int gl_batchvertexfirst = 0;
int gl_batchvertexcount = 0;
int gl_batchelementcount = 0;
#endif

static matrix4x4_t backend_viewmatrix;
static matrix4x4_t backend_modelmatrix;
static matrix4x4_t backend_modelviewmatrix;
static matrix4x4_t backend_glmodelviewmatrix;
static matrix4x4_t backend_projectmatrix;

static int backendunits, backendactive;
static mempool_t *gl_backend_mempool;

/*
note: here's strip order for a terrain row:
0--1--2--3--4
|\ |\ |\ |\ |
| \| \| \| \|
A--B--C--D--E

A0B, 01B, B1C, 12C, C2D, 23D, D3E, 34E

*elements++ = i + row;
*elements++ = i;
*elements++ = i + row + 1;
*elements++ = i;
*elements++ = i + 1;
*elements++ = i + row + 1;
*/

void GL_Backend_AllocElementsArray(void)
{
	if (varray_buf_elements3i)
		Mem_Free(varray_buf_elements3i);
	varray_buf_elements3i = Mem_Alloc(gl_backend_mempool, mesh_maxelements * sizeof(GLuint));
}

void GL_Backend_FreeElementArray(void)
{
	if (varray_buf_elements3i)
		Mem_Free(varray_buf_elements3i);
	varray_buf_elements3i = NULL;
}

void GL_Backend_CheckCvars(void)
{
	if (gl_mesh_maxverts.integer < 1024)
		Cvar_SetValueQuick(&gl_mesh_maxverts, 1024);
	if (gl_mesh_maxverts.integer > 65536)
		Cvar_SetValueQuick(&gl_mesh_maxverts, 65536);
#ifdef MESH_VAR
	if (gl_mesh_vertex_array_range.integer && !gl_support_var)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range, 0);
	if (gl_mesh_vertex_array_range_readfrequency.value < 0)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_readfrequency, 0);
	if (gl_mesh_vertex_array_range_readfrequency.value > 1)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_readfrequency, 1);
	if (gl_mesh_vertex_array_range_writefrequency.value < 0)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_writefrequency, 0);
	if (gl_mesh_vertex_array_range_writefrequency.value > 1)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_writefrequency, 1);
	if (gl_mesh_vertex_array_range_priority.value < 0)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_priority, 0);
	if (gl_mesh_vertex_array_range_priority.value > 1)
		Cvar_SetValueQuick(&gl_mesh_vertex_array_range_priority, 1);
#endif
}

int polygonelements[768];

static void R_Mesh_CacheArray_Startup(void);
static void R_Mesh_CacheArray_Shutdown(void);
void GL_Backend_AllocArrays(void)
{
	int i, size;
	qbyte *data;

	if (!gl_backend_mempool)
	{
		gl_backend_mempool = Mem_AllocPool("GL_Backend");
		varray_buf_vertex3f = NULL;
		varray_buf_color4f = NULL;
		varray_buf_color4b = NULL;
		varray_buf_elements3i = NULL;
		for (i = 0;i < MAX_TEXTUREUNITS;i++)
			varray_buf_texcoord3f[i] = varray_buf_texcoord2f[i] = NULL;
	}

	if (varray_buf_vertex3f)
#ifdef MESH_VAR
		VID_FreeVertexArrays(varray_buf_vertex3f);
#else
		Mem_Free(varray_buf_vertex3f);
#endif
	varray_buf_vertex3f = NULL;
	varray_buf_color4f = NULL;
	varray_buf_color4b = NULL;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
		varray_buf_texcoord3f[i] = varray_buf_texcoord2f[i] = NULL;

	mesh_maxverts = gl_mesh_maxverts.integer;
	size = mesh_maxverts * (sizeof(float[3]) + sizeof(float[4]) + sizeof(qbyte[4]) + (sizeof(float[3]) + sizeof(float[2])) * backendunits);
#ifdef MESH_VAR
	mesh_var = gl_mesh_vertex_array_range.integer && gl_support_var;
	mesh_var_readfrequency = gl_mesh_vertex_array_range_readfrequency.value;
	mesh_var_writefrequency = gl_mesh_vertex_array_range_writefrequency.value;
	mesh_var_priority = gl_mesh_vertex_array_range_priority.value;
	data = VID_AllocVertexArrays(gl_backend_mempool, size, gl_mesh_vertex_array_range.integer, gl_mesh_vertex_array_range_readfrequency.value, gl_mesh_vertex_array_range_writefrequency.value, gl_mesh_vertex_array_range_priority.value);
#else
	data = Mem_Alloc(gl_backend_mempool, size);
#endif

	varray_buf_vertex3f = (void *)data;data += sizeof(float[3]) * mesh_maxverts;
	varray_buf_color4f = (void *)data;data += sizeof(float[4]) * mesh_maxverts;
	for (i = 0;i < backendunits;i++)
	{
		varray_buf_texcoord3f[i] = (void *)data;data += sizeof(float[3]) * mesh_maxverts;
		varray_buf_texcoord2f[i] = (void *)data;data += sizeof(float[2]) * mesh_maxverts;
	}
	for (;i < MAX_TEXTUREUNITS;i++)
		varray_buf_texcoord3f[i] = varray_buf_texcoord2f[i] = NULL;
	varray_buf_color4b = (void *)data;data += sizeof(qbyte[4]) * mesh_maxverts;

	GL_Backend_AllocElementsArray();

#ifdef MESH_VAR
	if (mesh_var)
	{
		CHECKGLERROR
		qglVertexArrayRangeNV(size, varray_buf_vertex3f);
		CHECKGLERROR
	}
#endif

	R_Mesh_CacheArray_Startup();
}

void GL_Backend_FreeArrays(void)
{
	int i;

	R_Mesh_CacheArray_Shutdown();

#ifdef MESH_VAR
	if (mesh_var)
	{
		CHECKGLERROR
		qglDisableClientState(GL_VERTEX_ARRAY_RANGE_NV);
		CHECKGLERROR
	}
#endif

	if (varray_buf_vertex3f)
#ifdef MESH_VAR
		VID_FreeVertexArrays(varray_buf_vertex3f);
#else
		Mem_Free(varray_buf_vertex3f);
#endif
	varray_buf_vertex3f = NULL;
	varray_buf_color4f = NULL;
	varray_buf_color4b = NULL;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
		varray_buf_texcoord3f[i] = varray_buf_texcoord2f[i] = NULL;
	varray_buf_elements3i = NULL;

	Mem_FreePool(&gl_backend_mempool);
}

static void gl_backend_start(void)
{
	GL_Backend_CheckCvars();

	Con_Printf("OpenGL Backend started with gl_mesh_maxverts %i\n", gl_mesh_maxverts.integer);
	if (qglDrawRangeElements != NULL)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_maxdrawrangeelementsindices);
		CHECKGLERROR
		Con_Printf("glDrawRangeElements detected (max vertices %i, max indices %i)\n", gl_maxdrawrangeelementsvertices, gl_maxdrawrangeelementsindices);
	}
	if (strstr(gl_renderer, "3Dfx"))
	{
		Con_Printf("3Dfx driver detected, forcing gl_mesh_floatcolors to 0 to prevent crashs\n");
		Cvar_SetValueQuick(&gl_mesh_floatcolors, 0);
	}

	backendunits = min(MAX_TEXTUREUNITS, gl_textureunits);

	GL_Backend_AllocArrays();

#ifdef MESH_VAR
	if (mesh_var)
	{
		CHECKGLERROR
		qglEnableClientState(GL_VERTEX_ARRAY_RANGE_NV);
		CHECKGLERROR
	}
#endif
	varray_offset = varray_offsetnext = 0;
#ifdef MESH_BATCH
	gl_batchvertexfirst = 0;
	gl_batchvertexcount = 0;
	gl_batchelementcount = 0;
#endif

	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	backendunits = 0;
	backendactive = false;

	Con_Printf("OpenGL Backend shutting down\n");

#ifdef MESH_VAR
	if (mesh_var)
	{
		CHECKGLERROR
		qglDisableClientState(GL_VERTEX_ARRAY_RANGE_NV);
		CHECKGLERROR
	}
#endif

	GL_Backend_FreeArrays();
}

void GL_Backend_ResizeArrays(int numvertices)
{
	Cvar_SetValueQuick(&gl_mesh_maxverts, numvertices);
	GL_Backend_CheckCvars();
	mesh_maxverts = gl_mesh_maxverts.integer;
	GL_Backend_AllocArrays();
}

static void gl_backend_newmap(void)
{
}

void gl_backend_init(void)
{
	int i;

	for (i = 0;i < POLYGONELEMENTS_MAXPOINTS - 2;i++)
	{
		polygonelements[i * 3 + 0] = 0;
		polygonelements[i * 3 + 1] = i + 1;
		polygonelements[i * 3 + 2] = i + 2;
	}

	Cvar_RegisterVariable(&r_render);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_lockarrays);
	Cvar_RegisterVariable(&gl_delayfinish);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

	Cvar_RegisterVariable(&gl_mesh_maxverts);
	Cvar_RegisterVariable(&gl_mesh_floatcolors);
	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
#ifdef MESH_VAR
	Cvar_RegisterVariable(&gl_mesh_vertex_array_range);
	Cvar_RegisterVariable(&gl_mesh_vertex_array_range_readfrequency);
	Cvar_RegisterVariable(&gl_mesh_vertex_array_range_writefrequency);
	Cvar_RegisterVariable(&gl_mesh_vertex_array_range_priority);
#endif
#ifdef MESH_BATCH
	Cvar_RegisterVariable(&gl_mesh_batching);
#endif
	Cvar_RegisterVariable(&gl_mesh_copyarrays);
	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
}

void GL_SetupView_ViewPort (int x, int y, int width, int height)
{
	if (!r_render.integer)
		return;

	// y is weird beause OpenGL is bottom to top, we use top to bottom
	qglViewport(x, vid.realheight - (y + height), width, height);
	CHECKGLERROR
}

void GL_SetupView_Orientation_Identity (void)
{
	Matrix4x4_CreateIdentity(&backend_viewmatrix);
	memset(&backend_modelmatrix, 0, sizeof(backend_modelmatrix));
}

void GL_SetupView_Orientation_FromEntity (vec3_t origin, vec3_t angles)
{
	Matrix4x4_CreateRotate(&backend_viewmatrix, -90, 1, 0, 0);
	Matrix4x4_ConcatRotate(&backend_viewmatrix, 90, 0, 0, 1);
	Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[2], 1, 0, 0);
	Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[0], 0, 1, 0);
	Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[1], 0, 0, 1);
	Matrix4x4_ConcatTranslate(&backend_viewmatrix, -origin[0], -origin[1], -origin[2]);
	memset(&backend_modelmatrix, 0, sizeof(backend_modelmatrix));
}

void GL_SetupView_Mode_Perspective (double fovx, double fovy, double zNear, double zFar)
{
	double xmax, ymax;

	if (!r_render.integer)
		return;

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity();CHECKGLERROR
	// pyramid slopes
	xmax = zNear * tan(fovx * M_PI / 360.0);
	ymax = zNear * tan(fovy * M_PI / 360.0);
	// set view pyramid
	qglFrustum(-xmax, xmax, -ymax, ymax, zNear, zFar);CHECKGLERROR
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
}

void GL_SetupView_Mode_PerspectiveInfiniteFarClip (double fovx, double fovy, double zNear)
{
	float nudge, m[16];

	if (!r_render.integer)
		return;

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity();CHECKGLERROR
	// set view pyramid
	nudge = 1.0 - 1.0 / (1<<23);
	m[ 0] = 1.0 / tan(fovx * M_PI / 360.0);
	m[ 1] = 0;
	m[ 2] = 0;
	m[ 3] = 0;
	m[ 4] = 0;
	m[ 5] = 1.0 / tan(fovy * M_PI / 360.0);
	m[ 6] = 0;
	m[ 7] = 0;
	m[ 8] = 0;
	m[ 9] = 0;
	m[10] = -1 * nudge;
	m[11] = -1 * nudge;
	m[12] = 0;
	m[13] = 0;
	m[14] = -2 * zNear * nudge;
	m[15] = 0;
	qglLoadMatrixf(m);
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
	backend_projectmatrix.m[0][0] = m[0];
	backend_projectmatrix.m[1][0] = m[1];
	backend_projectmatrix.m[2][0] = m[2];
	backend_projectmatrix.m[3][0] = m[3];
	backend_projectmatrix.m[0][1] = m[4];
	backend_projectmatrix.m[1][1] = m[5];
	backend_projectmatrix.m[2][1] = m[6];
	backend_projectmatrix.m[3][1] = m[7];
	backend_projectmatrix.m[0][2] = m[8];
	backend_projectmatrix.m[1][2] = m[9];
	backend_projectmatrix.m[2][2] = m[10];
	backend_projectmatrix.m[3][2] = m[11];
	backend_projectmatrix.m[0][3] = m[12];
	backend_projectmatrix.m[1][3] = m[13];
	backend_projectmatrix.m[2][3] = m[14];
	backend_projectmatrix.m[3][3] = m[15];
}

void GL_SetupView_Mode_Ortho (double x1, double y1, double x2, double y2, double zNear, double zFar)
{
	if (!r_render.integer)
		return;

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity();CHECKGLERROR
	qglOrtho(x1, x2, y2, y1, zNear, zFar);
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
}

typedef struct gltextureunit_s
{
	int t1d, t2d, t3d, tcubemap;
	int arrayenabled, arrayis3d;
	void *pointer_texcoord;
	float rgbscale, alphascale;
	int combinergb, combinealpha;
	// FIXME: add more combine stuff
}
gltextureunit_t;

static struct
{
	int blendfunc1;
	int blendfunc2;
	int blend;
	GLboolean depthmask;
	int depthdisable;
	int unit;
	int clientunit;
	gltextureunit_t units[MAX_TEXTUREUNITS];
	int colorarray;
	float color4f[4];
	int lockrange_first;
	int lockrange_count;
	int pointervertexcount;
	void *pointer_vertex;
	void *pointer_color;
}
gl_state;

void GL_SetupTextureState(void)
{
	int i;
	gltextureunit_t *unit;
	for (i = 0;i < backendunits;i++)
	{
		if (qglActiveTexture)
			qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
		if (qglClientActiveTexture)
			qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
		unit = gl_state.units + i;
		unit->t1d = 0;
		unit->t2d = 0;
		unit->t3d = 0;
		unit->tcubemap = 0;
		unit->arrayenabled = false;
		unit->arrayis3d = false;
		unit->pointer_texcoord = NULL;
		unit->rgbscale = 1;
		unit->alphascale = 1;
		unit->combinergb = GL_MODULATE;
		unit->combinealpha = GL_MODULATE;
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), varray_buf_texcoord2f[i]);CHECKGLERROR
		qglDisable(GL_TEXTURE_1D);CHECKGLERROR
		qglDisable(GL_TEXTURE_2D);CHECKGLERROR
		if (gl_texture3d)
		{
			qglDisable(GL_TEXTURE_3D);CHECKGLERROR
		}
		if (gl_texturecubemap)
		{
			qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
		}
		if (gl_combine.integer)
		{
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);CHECKGLERROR // for GL_INTERPOLATE_ARB mode
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
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);CHECKGLERROR
		}
		else
		{
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
		}
	}
}

void GL_Backend_ResetState(void)
{
	memset(&gl_state, 0, sizeof(gl_state));
	gl_state.depthdisable = false;
	gl_state.blendfunc1 = GL_ONE;
	gl_state.blendfunc2 = GL_ZERO;
	gl_state.blend = false;
	gl_state.depthmask = GL_TRUE;
	gl_state.colorarray = false;
	gl_state.color4f[0] = gl_state.color4f[1] = gl_state.color4f[2] = gl_state.color4f[3] = 1;
	gl_state.lockrange_first = 0;
	gl_state.lockrange_count = 0;
	gl_state.pointervertexcount = 0;
	gl_state.pointer_vertex = NULL;
	gl_state.pointer_color = NULL;

	CHECKGLERROR
	qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglCullFace(GL_FRONT);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
	qglDisable(GL_BLEND);CHECKGLERROR
	qglDepthMask(gl_state.depthmask);CHECKGLERROR
	qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), varray_buf_vertex3f);CHECKGLERROR
	qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
	if (gl_mesh_floatcolors.integer)
	{
		qglColorPointer(4, GL_FLOAT, sizeof(float[4]), varray_buf_color4f);CHECKGLERROR
	}
	else
	{
		qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(qbyte[4]), varray_buf_color4b);CHECKGLERROR
	}
	GL_Color(0, 0, 0, 0);
	GL_Color(1, 1, 1, 1);

	GL_SetupTextureState();
}

void GL_UseColorArray(void)
{
	if (!gl_state.colorarray)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.colorarray = true;
		qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	}
}

void GL_Color(float cr, float cg, float cb, float ca)
{
	if (gl_state.colorarray)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.colorarray = false;
		qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
		gl_state.color4f[0] = cr;
		gl_state.color4f[1] = cg;
		gl_state.color4f[2] = cb;
		gl_state.color4f[3] = ca;
		qglColor4f(cr, cg, cb, ca);
	}
	else
	{
		if (gl_state.color4f[0] != cr || gl_state.color4f[1] != cg || gl_state.color4f[2] != cb || gl_state.color4f[3] != ca)
		{
#ifdef MESH_BATCH
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
#endif
			gl_state.color4f[0] = cr;
			gl_state.color4f[1] = cg;
			gl_state.color4f[2] = cb;
			gl_state.color4f[3] = ca;
			qglColor4f(cr, cg, cb, ca);
		}
	}
}

void GL_LockArrays(int first, int count)
{
	if (gl_state.lockrange_count != count || gl_state.lockrange_first != first)
	{
		if (gl_state.lockrange_count)
		{
			gl_state.lockrange_count = 0;
			CHECKGLERROR
			qglUnlockArraysEXT();
			CHECKGLERROR
		}
		if (count && gl_supportslockarrays && gl_lockarrays.integer)
		{
			gl_state.lockrange_first = first;
			gl_state.lockrange_count = count;
			CHECKGLERROR
			qglLockArraysEXT(first, count);
			CHECKGLERROR
		}
	}
}

void GL_TransformToScreen(const vec4_t in, vec4_t out)
{
	vec4_t temp;
	float iw;
	Matrix4x4_Transform4 (&backend_viewmatrix, in, temp);
	Matrix4x4_Transform4 (&backend_projectmatrix, temp, out);
	iw = 1.0f / out[3];
	out[0] = r_refdef.x + (out[0] * iw + 1.0f) * r_refdef.width * 0.5f;
	out[1] = r_refdef.y + (out[1] * iw + 1.0f) * r_refdef.height * 0.5f;
	out[2] = out[2] * iw;
}

// called at beginning of frame
void R_Mesh_Start(void)
{
	BACKENDACTIVECHECK

	CHECKGLERROR

	GL_Backend_CheckCvars();
	if (mesh_maxverts != gl_mesh_maxverts.integer
#ifdef MESH_VAR
	 || mesh_var != (gl_mesh_vertex_array_range.integer && gl_support_var)
	 || mesh_var_readfrequency != gl_mesh_vertex_array_range_readfrequency.value
	 || mesh_var_writefrequency != gl_mesh_vertex_array_range_writefrequency.value
	 || mesh_var_priority != gl_mesh_vertex_array_range_priority.value
#endif
		)
		GL_Backend_ResizeArrays(gl_mesh_maxverts.integer);

	GL_Backend_ResetState();
#ifdef MESH_VAR
	if (!mesh_var)
	{
		gl_batchvertexfirst = gl_batchvertexcount = gl_batchelementcount = 0;
		varray_offset = varray_offsetnext = 0;
	}
#else
	varray_offset = varray_offsetnext = 0;
#endif
}

int gl_backend_rebindtextures;

void GL_ConvertColorsFloatToByte(int first, int count)
{
	int i, k;
	union {float f[4];int i[4];} *color4fi;
	struct {GLubyte c[4];} *color4b;

	// shift float to have 8bit fraction at base of number
	color4fi = (void *)(varray_buf_color4f + first * 4);
	for (i = 0;i < count;i++, color4fi++)
	{
		color4fi->f[0] += 32768.0f;
		color4fi->f[1] += 32768.0f;
		color4fi->f[2] += 32768.0f;
		color4fi->f[3] += 32768.0f;
	}

	// then read as integer and kill float bits...
	color4fi = (void *)(varray_buf_color4f + first * 4);
	color4b = (void *)(varray_buf_color4b + first * 4);
	for (i = 0;i < count;i++, color4fi++, color4b++)
	{
		k = color4fi->i[0] & 0x7FFFFF;color4b->c[0] = (GLubyte) min(k, 255);
		k = color4fi->i[1] & 0x7FFFFF;color4b->c[1] = (GLubyte) min(k, 255);
		k = color4fi->i[2] & 0x7FFFFF;color4b->c[2] = (GLubyte) min(k, 255);
		k = color4fi->i[3] & 0x7FFFFF;color4b->c[3] = (GLubyte) min(k, 255);
	}
}

/*
// enlarges geometry buffers if they are too small
void _R_Mesh_ResizeCheck(int numverts)
{
	if (numverts > mesh_maxverts)
	{
		BACKENDACTIVECHECK
		GL_Backend_ResizeArrays(numverts + 100);
		GL_Backend_ResetState();
	}
}
*/

void R_Mesh_EndBatch(void)
{
#ifdef MESH_BATCH
	if (gl_batchelementcount)
	{
		if (gl_state.pointervertexcount)
			Host_Error("R_Mesh_EndBatch: called with pointers enabled\n");

		if (gl_state.colorarray && !gl_mesh_floatcolors.integer && gl_state.pointer_color == NULL)
			GL_ConvertColorsFloatToByte(gl_batchvertexfirst, gl_batchvertexcount);
		if (r_render.integer)
		{
			//int i;for (i = 0;i < gl_batchelementcount;i++) if (varray_buf_elements3i[i] < gl_batchvertexfirst || varray_buf_elements3i[i] >= (gl_batchvertexfirst + gl_batchvertexcount)) Host_Error("R_Mesh_EndBatch: invalid element #%i (value %i) outside range %i-%i\n", i, varray_buf_elements3i[i], gl_batchvertexfirst, gl_batchvertexfirst + gl_batchvertexcount);
			CHECKGLERROR
			GL_LockArrays(gl_batchvertexfirst, gl_batchvertexcount);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, gl_batchvertexfirst, gl_batchvertexfirst + gl_batchvertexcount, gl_batchelementcount, GL_UNSIGNED_INT, (const GLuint *) varray_buf_elements3i);CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, gl_batchelementcount, GL_UNSIGNED_INT, (const GLuint *) varray_buf_elements3i);CHECKGLERROR
			}
			GL_LockArrays(0, 0);
		}
		gl_batchelementcount = 0;
		gl_batchvertexcount = 0;
	}
#endif
}

void GL_Backend_RenumberElements(int *out, int count, const int *in, int offset)
{
	int i;
	//if (offset)
		for (i = 0;i < count;i++)
			*out++ = *in++ + offset;
	//else
	//	memcpy(out, in, sizeof(*out) * count);
}

// gets vertex buffer space for use with a following R_Mesh_Draw
// (can be multiple Draw calls per GetSpace)
void R_Mesh_GetSpace(int numverts)
{
	int i;

	if (gl_state.pointervertexcount)
		Host_Error("R_Mesh_GetSpace: called with pointers enabled\n");
	if (gl_state.lockrange_count)
		Host_Error("R_Mesh_GetSpace: called with arrays locked\n");

	varray_offset = varray_offsetnext;
	if (varray_offset + numverts > mesh_maxverts)
	{
		//Con_Printf("R_Mesh_GetSpace: vertex buffer wrap\n");
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		varray_offset = 0;
#ifdef MESH_VAR
		if (mesh_var)
		{
			CHECKGLERROR
			qglFlushVertexArrayRangeNV();
			CHECKGLERROR
		}
#endif
		if (numverts > mesh_maxverts)
		{
			GL_Backend_ResizeArrays(numverts + 100);
			GL_Backend_ResetState();
		}
	}

	varray_vertex3f = varray_buf_vertex3f + varray_offset * 3;
	varray_color4f = varray_buf_color4f + varray_offset * 4;
	for (i = 0;i < backendunits;i++)
	{
		varray_texcoord3f[i] = varray_buf_texcoord3f[i] + varray_offset * 3;
		varray_texcoord2f[i] = varray_buf_texcoord2f[i] + varray_offset * 2;
	}

	varray_offsetnext = varray_offset + numverts;
}

// renders triangles using vertices from the most recent GetSpace call
// (can be multiple Draw calls per GetSpace)
void R_Mesh_Draw(int numverts, int numtriangles, const int *elements)
{
	int numelements = numtriangles * 3;
	if (numtriangles == 0 || numverts == 0)
	{
		Con_Printf("R_Mesh_Draw(%d, %d, %08p);\n", numverts, numtriangles, elements);
		return;
	}
	c_meshs++;
	c_meshelements += numelements;
	CHECKGLERROR
	if (gl_state.pointervertexcount)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		if (r_render.integer)
		{
			GL_LockArrays(0, gl_state.pointervertexcount);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, 0, gl_state.pointervertexcount, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
			}
			GL_LockArrays(0, 0);
		}
	}
#ifdef MESH_BATCH
	else if (gl_mesh_batching.integer)
	{
		if (mesh_maxelements < gl_batchelementcount + numelements)
		{
			//Con_Printf("R_Mesh_Draw: enlarging elements array\n");
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
			// round up to a multiple of 1024 and add another 1024 just for good measure
			mesh_maxelements = (gl_batchelementcount + numelements + 1024 + 1023) & ~1023;
			GL_Backend_AllocElementsArray();
		}
		if (varray_offset < gl_batchvertexfirst && gl_batchelementcount)
			R_Mesh_EndBatch();
		if (gl_batchelementcount == 0)
		{
			gl_batchvertexfirst = varray_offset;
			gl_batchvertexcount = 0;
		}
		if (gl_batchvertexcount < varray_offsetnext - gl_batchvertexfirst)
			gl_batchvertexcount = varray_offsetnext - gl_batchvertexfirst;
		GL_Backend_RenumberElements(varray_buf_elements3i + gl_batchelementcount, numelements, elements, varray_offset);
		//Con_Printf("off %i:%i, vertex %i:%i, element %i:%i\n", varray_offset, varray_offsetnext, gl_batchvertexfirst, gl_batchvertexfirst + gl_batchvertexcount, gl_batchelementcount, gl_batchelementcount + numelements);
		gl_batchelementcount += numelements;
		//{int i;for (i = 0;i < gl_batchelementcount;i++) if (varray_buf_elements3i[i] < gl_batchvertexfirst || varray_buf_elements3i[i] >= (gl_batchvertexfirst + gl_batchvertexcount)) Host_Error("R_Mesh_EndBatch: invalid element #%i (value %i) outside range %i-%i, there were previously %i elements and there are now %i elements, varray_offset is %i\n", i, varray_buf_elements3i[i], gl_batchvertexfirst, gl_batchvertexfirst + gl_batchvertexcount, gl_batchelementcount - numelements, gl_batchelementcount, varray_offset);}
	}
#endif
	else
	{
		GL_Backend_RenumberElements(varray_buf_elements3i, numelements, elements, varray_offset);
		if (r_render.integer)
		{
			GL_LockArrays(varray_offset, numverts);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, varray_offset, varray_offset + numverts, numelements, GL_UNSIGNED_INT, varray_buf_elements3i);CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, varray_buf_elements3i);CHECKGLERROR
			}
			GL_LockArrays(0, 0);
		}
	}
}

// renders triangles using vertices from the most recent GetSpace call
// (can be multiple Draw calls per GetSpace)
void R_Mesh_Draw_NoBatching(int numverts, int numtriangles, const int *elements)
{
	int numelements = numtriangles * 3;
	if (numtriangles == 0 || numverts == 0)
	{
		Con_Printf("R_Mesh_Draw_NoBatching(%d, %d, %08p);\n", numverts, numtriangles, elements);
		return;
	}
	c_meshs++;
	c_meshelements += numelements;
	CHECKGLERROR
	if (gl_state.pointervertexcount)
	{
		if (r_render.integer)
		{
			GL_LockArrays(0, gl_state.pointervertexcount);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, 0, gl_state.pointervertexcount, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
			}
			GL_LockArrays(0, 0);
		}
	}
	else
	{
		GL_Backend_RenumberElements(varray_buf_elements3i, numelements, elements, varray_offset);
		if (r_render.integer)
		{
			GL_LockArrays(varray_offset, numverts);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, varray_offset, varray_offset + numverts, numelements, GL_UNSIGNED_INT, varray_buf_elements3i);CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, varray_buf_elements3i);CHECKGLERROR
			}
			GL_LockArrays(0, 0);
		}
	}
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	int i;
	BACKENDACTIVECHECK
#ifdef MESH_BATCH
	if (gl_batchelementcount)
		R_Mesh_EndBatch();
#endif
	GL_LockArrays(0, 0);

	for (i = backendunits - 1;i >= 0;i--)
	{
		if (qglActiveTexture)
			qglActiveTexture(GL_TEXTURE0_ARB + i);CHECKGLERROR
		if (qglClientActiveTexture)
			qglClientActiveTexture(GL_TEXTURE0_ARB + i);CHECKGLERROR
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		qglDisable(GL_TEXTURE_1D);CHECKGLERROR
		qglDisable(GL_TEXTURE_2D);CHECKGLERROR
		if (gl_texture3d)
		{
			qglDisable(GL_TEXTURE_3D);CHECKGLERROR
		}
		if (gl_texturecubemap)
		{
			qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
		}
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
		if (gl_combine.integer)
		{
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);CHECKGLERROR
		}
	}
	qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

	qglDisable(GL_BLEND);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglDepthMask(GL_TRUE);CHECKGLERROR
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);CHECKGLERROR
}

void R_Mesh_Matrix(const matrix4x4_t *matrix)
{
	if (memcmp(matrix, &backend_modelmatrix, sizeof(matrix4x4_t)))
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		backend_modelmatrix = *matrix;
		Matrix4x4_Concat(&backend_modelviewmatrix, &backend_viewmatrix, matrix);
		Matrix4x4_Transpose(&backend_glmodelviewmatrix, &backend_modelviewmatrix);
		qglLoadMatrixf(&backend_glmodelviewmatrix.m[0][0]);
	}
}

// sets up the requested state
void R_Mesh_MainState(const rmeshstate_t *m)
{
	void *p;
	BACKENDACTIVECHECK

	if (gl_state.blendfunc1 != m->blendfunc1 || gl_state.blendfunc2 != m->blendfunc2)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		qglBlendFunc(gl_state.blendfunc1 = m->blendfunc1, gl_state.blendfunc2 = m->blendfunc2);CHECKGLERROR
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
	if (gl_state.depthdisable != m->depthdisable)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.depthdisable = m->depthdisable;
		if (gl_state.depthdisable)
			qglDisable(GL_DEPTH_TEST);
		else
			qglEnable(GL_DEPTH_TEST);
	}
	if (gl_state.depthmask != (m->blendfunc2 == GL_ZERO || m->depthwrite))
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		qglDepthMask(gl_state.depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite));CHECKGLERROR
	}

	if (gl_state.pointervertexcount != m->pointervertexcount)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.pointervertexcount = m->pointervertexcount;
	}

	p = gl_state.pointervertexcount ? m->pointer_vertex : NULL;
	if (gl_state.pointer_vertex != p)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.pointer_vertex = p;
		qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), p ? p : varray_buf_vertex3f);CHECKGLERROR
	}

	p = gl_state.pointervertexcount ? m->pointer_color : NULL;
	if (gl_state.pointer_color != p)
	{
#ifdef MESH_BATCH
		if (gl_batchelementcount)
			R_Mesh_EndBatch();
#endif
		gl_state.pointer_color = p;
		if (p || gl_mesh_floatcolors.integer)
			qglColorPointer(4, GL_FLOAT, sizeof(float[4]), p ? p : varray_buf_color4f);
		else
			qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GLubyte[4]), p ? p : varray_buf_color4b);
		CHECKGLERROR
	}
}

void R_Mesh_TextureState(const rmeshstate_t *m)
{
	int i, combinergb, combinealpha;
	float scale;
	gltextureunit_t *unit;
	void *p;

	BACKENDACTIVECHECK

	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
	}

	for (i = 0;i < backendunits;i++)
	{
		unit = gl_state.units + i;
		if (unit->t1d != m->tex1d[i] || unit->t2d != m->tex[i] || unit->t3d != m->tex3d[i] || unit->tcubemap != m->texcubemap[i])
		{
			if (m->tex3d[i] || m->texcubemap[i])
			{
				if (!unit->arrayis3d)
				{
#ifdef MESH_BATCH
					if (gl_batchelementcount)
						R_Mesh_EndBatch();
#endif
					unit->arrayis3d = true;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglTexCoordPointer(3, GL_FLOAT, sizeof(float[3]), varray_buf_texcoord3f[i]);
				}
				if (!unit->arrayenabled)
				{
#ifdef MESH_BATCH
					if (gl_batchelementcount)
						R_Mesh_EndBatch();
#endif
					unit->arrayenabled = true;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			else if (m->tex1d[i] || m->tex[i])
			{
				if (unit->arrayis3d)
				{
#ifdef MESH_BATCH
					if (gl_batchelementcount)
						R_Mesh_EndBatch();
#endif
					unit->arrayis3d = false;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), varray_buf_texcoord2f[i]);
				}
				if (!unit->arrayenabled)
				{
#ifdef MESH_BATCH
					if (gl_batchelementcount)
						R_Mesh_EndBatch();
#endif
					unit->arrayenabled = true;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			else
			{
				if (unit->arrayenabled)
				{
#ifdef MESH_BATCH
					if (gl_batchelementcount)
						R_Mesh_EndBatch();
#endif
					unit->arrayenabled = false;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			if (unit->t1d != m->tex1d[i])
			{
#ifdef MESH_BATCH
				if (gl_batchelementcount)
					R_Mesh_EndBatch();
#endif
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (m->tex1d[i])
				{
					if (unit->t1d == 0)
						qglEnable(GL_TEXTURE_1D);CHECKGLERROR
				}
				else
				{
					if (unit->t1d)
						qglDisable(GL_TEXTURE_1D);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_1D, (unit->t1d = m->tex1d[i]));CHECKGLERROR
			}
			if (unit->t2d != m->tex[i])
			{
#ifdef MESH_BATCH
				if (gl_batchelementcount)
					R_Mesh_EndBatch();
#endif
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (m->tex[i])
				{
					if (unit->t2d == 0)
						qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				}
				else
				{
					if (unit->t2d)
						qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_2D, (unit->t2d = m->tex[i]));CHECKGLERROR
			}
			if (unit->t3d != m->tex3d[i])
			{
#ifdef MESH_BATCH
				if (gl_batchelementcount)
					R_Mesh_EndBatch();
#endif
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (m->tex3d[i])
				{
					if (unit->t3d == 0)
						qglEnable(GL_TEXTURE_3D);CHECKGLERROR
				}
				else
				{
					if (unit->t3d)
						qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_3D, (unit->t3d = m->tex3d[i]));CHECKGLERROR
			}
			if (unit->tcubemap != m->texcubemap[i])
			{
#ifdef MESH_BATCH
				if (gl_batchelementcount)
					R_Mesh_EndBatch();
#endif
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (m->texcubemap[i])
				{
					if (unit->tcubemap == 0)
						qglEnable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
				else
				{
					if (unit->tcubemap)
						qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, (unit->tcubemap = m->texcubemap[i]));CHECKGLERROR
			}
		}
		combinergb = m->texcombinergb[i];
		if (!combinergb)
			combinergb = GL_MODULATE;
		if (unit->combinergb != combinergb)
		{
#ifdef MESH_BATCH
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
#endif
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			unit->combinergb = combinergb;
			if (gl_combine.integer)
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, unit->combinergb);CHECKGLERROR
			}
			else
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combinergb);CHECKGLERROR
			}
		}
		combinealpha = m->texcombinealpha[i];
		if (!combinealpha)
			combinealpha = GL_MODULATE;
		if (unit->combinealpha != combinealpha)
		{
#ifdef MESH_BATCH
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
#endif
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			unit->combinealpha = combinealpha;
			if (gl_combine.integer)
			{
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, unit->combinealpha);CHECKGLERROR
			}
		}
		scale = max(m->texrgbscale[i], 1);
		if (gl_state.units[i].rgbscale != scale)
		{
#ifdef MESH_BATCH
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
#endif
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (gl_state.units[i].rgbscale = scale));CHECKGLERROR
		}
		scale = max(m->texalphascale[i], 1);
		if (gl_state.units[i].alphascale != scale)
		{
#ifdef MESH_BATCH
			if (gl_batchelementcount)
				R_Mesh_EndBatch();
#endif
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, (gl_state.units[i].alphascale = scale));CHECKGLERROR
		}
		if (unit->arrayenabled)
		{
			p = gl_state.pointervertexcount ? m->pointer_texcoord[i] : NULL;
			if (unit->pointer_texcoord != p)
			{
#ifdef MESH_BATCH
				if (gl_batchelementcount)
					R_Mesh_EndBatch();
#endif
				unit->pointer_texcoord = p;
				if (gl_state.clientunit != i)
				{
					qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
				}
				if (unit->arrayis3d)
					qglTexCoordPointer(3, GL_FLOAT, sizeof(float[3]), p ? p : varray_buf_texcoord3f[i]);
				else
					qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), p ? p : varray_buf_texcoord2f[i]);
				CHECKGLERROR
			}
		}
	}
}

void R_Mesh_State(const rmeshstate_t *m)
{
	R_Mesh_MainState(m);
	R_Mesh_TextureState(m);
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height, qboolean jpeg)
{
	qboolean ret;
	int i, j;
	qbyte *buffer;

	if (!r_render.integer)
		return false;

	buffer = Mem_Alloc(tempmempool, width*height*3);
	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	CHECKGLERROR

	// LordHavoc: compensate for v_overbrightbits when using hardware gamma
	if (v_hwgamma.integer)
	{
		for (i = 0;i < width * height * 3;i++)
		{
			j = buffer[i] << v_overbrightbits.integer;
			buffer[i] = (qbyte) (bound(0, j, 255));
		}
	}

	if (jpeg)
		ret = JPEG_SaveImage_preflipped (filename, width, height, buffer);
	else
		ret = Image_WriteTGARGB_preflipped (filename, width, height, buffer);

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
		qglClearDepth(1);CHECKGLERROR
		if (gl_stencil)
		{
			// LordHavoc: we use a stencil centered around 128 instead of 0,
			// to avoid clamping interfering with strange shadow volume
			// drawing orders
			qglClearStencil(128);CHECKGLERROR
		}
		// clear the screen
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (gl_stencil ? GL_STENCIL_BUFFER_BIT : 0));CHECKGLERROR
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
	if (gl_delayfinish.integer)
	{
		VID_Finish ();

		R_TimeReport("finish");
	}

	if (r_textureunits.integer > gl_textureunits)
		Cvar_SetValueQuick(&r_textureunits, gl_textureunits);
	if (r_textureunits.integer < 1)
		Cvar_SetValueQuick(&r_textureunits, 1);

	if (gl_combine.integer && (!gl_combine_extension || r_textureunits.integer < 2))
		Cvar_SetValueQuick(&gl_combine, 0);

	// lighting scale
	r_colorscale = 1.0f / (float) (1 << v_overbrightbits.integer);

	// lightmaps only
	r_lightmapscalebit = v_overbrightbits.integer;
	if (gl_combine.integer && r_textureunits.integer > 1)
		r_lightmapscalebit += 2;

	R_TimeReport("setup");

	R_ClearScreen();

	R_TimeReport("clear");

	if (scr_conlines < vid.conheight && cls.signon == SIGNONS)
		R_RenderView();

	// draw 2D stuff
	R_DrawQueue();

	if (gl_delayfinish.integer)
	{
		// tell driver to commit it's partially full geometry queue to the rendering queue
		// (this doesn't wait for the commands themselves to complete)
		qglFlush();
	}
	else
	{
		VID_Finish ();

		R_TimeReport("finish");
	}
}

// utility functions

void R_Mesh_CopyVertex3f(const float *vertex3f, int numverts)
{
#ifdef MESH_VAR
	if (mesh_var)
	{
		float *out = varray_vertex3f;
		while (--numverts)
		{
			*out++ = *vertex3f++;
			*out++ = *vertex3f++;
			*out++ = *vertex3f++;
		}
	}
	else
#endif
		memcpy(varray_vertex3f, vertex3f, numverts * sizeof(float[3]));
}

void R_Mesh_CopyTexCoord2f(int tmu, const float *texcoord2f, int numverts)
{
#ifdef MESH_VAR
	if (mesh_var)
	{
		float *out = varray_texcoord2f[tmu];
		while (numverts--)
		{
			*out++ = *texcoord2f++;
			*out++ = *texcoord2f++;
		}
	}
	else
#endif
		memcpy(varray_texcoord2f[tmu], texcoord2f, numverts * sizeof(float[2]));
}

void R_Mesh_CopyColor4f(const float *color4f, int numverts)
{
#ifdef MESH_VAR
	if (mesh_var)
	{
		float *out = varray_color4f;
		while (numverts--)
		{
			*out++ = *color4f++;
			*out++ = *color4f++;
			*out++ = *color4f++;
			*out++ = *color4f++;
		}
	}
	else
#endif
		memcpy(varray_color4f, color4f, numverts * sizeof(float[4]));
}

void R_ScrollTexCoord2f (float *out2f, const float *in2f, int numverts, float s, float t)
{
	while (numverts--)
	{
		*out2f++ = *in2f++ + s;
		*out2f++ = *in2f++ + t;
	}
}

//===========================================================================
// vertex array caching subsystem
//===========================================================================

typedef struct rcachearraylink_s
{
	struct rcachearraylink_s *next, *prev;
	struct rcachearrayitem_s *data;
}
rcachearraylink_t;

typedef struct rcachearrayitem_s
{
	// the original request structure
	rcachearrayrequest_t request;
	// active
	int active;
	// offset into r_mesh_rcachedata
	int offset;
	// for linking this into the sequential list
	rcachearraylink_t sequentiallink;
	// for linking this into the lookup list
	rcachearraylink_t hashlink;
}
rcachearrayitem_t;

#define RCACHEARRAY_HASHSIZE 65536
#define RCACHEARRAY_ITEMS 4096
#define RCACHEARRAY_DEFAULTSIZE (4 << 20)

// all active items are linked into this chain in sorted order
static rcachearraylink_t r_mesh_rcachesequentialchain;
// all inactive items are linked into this chain in unknown order
static rcachearraylink_t r_mesh_rcachefreechain;
// all active items are also linked into these chains (using their hashlink)
static rcachearraylink_t r_mesh_rcachechain[RCACHEARRAY_HASHSIZE];

// all items are stored here, whether active or inactive
static rcachearrayitem_t r_mesh_rcacheitems[RCACHEARRAY_ITEMS];

// size of data buffer
static int r_mesh_rcachedata_size = RCACHEARRAY_DEFAULTSIZE;
// data buffer
static qbyte r_mesh_rcachedata[RCACHEARRAY_DEFAULTSIZE];

// current state
static int r_mesh_rcachedata_offset;
static rcachearraylink_t *r_mesh_rcachesequentialchain_current;

static void R_Mesh_CacheArray_Startup(void)
{
	int i;
	rcachearraylink_t *l;
	// prepare all the linked lists
	l = &r_mesh_rcachesequentialchain;l->next = l->prev = l;l->data = NULL;
	l = &r_mesh_rcachefreechain;l->next = l->prev = l;l->data = NULL;
	memset(&r_mesh_rcachechain, 0, sizeof(r_mesh_rcachechain));
	for (i = 0;i < RCACHEARRAY_HASHSIZE;i++)
	{
		l = &r_mesh_rcachechain[i];
		l->next = l->prev = l;
		l->data = NULL;
	}
	memset(&r_mesh_rcacheitems, 0, sizeof(r_mesh_rcacheitems));
	for (i = 0;i < RCACHEARRAY_ITEMS;i++)
	{
		r_mesh_rcacheitems[i].hashlink.data = r_mesh_rcacheitems[i].sequentiallink.data = &r_mesh_rcacheitems[i];
		l = &r_mesh_rcacheitems[i].sequentiallink;
		l->next = &r_mesh_rcachefreechain;
		l->prev = l->next->prev;
		l->next->prev = l->prev->next = l;
	}
	// clear other state
	r_mesh_rcachedata_offset = 0;
	r_mesh_rcachesequentialchain_current = &r_mesh_rcachesequentialchain;
}

static void R_Mesh_CacheArray_Shutdown(void)
{
}

/*
static void R_Mesh_CacheArray_ValidateState(int num)
{
	rcachearraylink_t *l, *lhead;
	lhead = &r_mesh_rcachesequentialchain;
	if (r_mesh_rcachesequentialchain_current == lhead)
		return;
	for (l = lhead->next;l != lhead;l = l->next)
		if (r_mesh_rcachesequentialchain_current == l)
			return;
	Sys_Error("%i", num);
}
*/

int R_Mesh_CacheArray(rcachearrayrequest_t *r)
{
	rcachearraylink_t *l, *lhead, *lnext;
	rcachearrayitem_t *d;
	int hashindex, offset, offsetend;

	//R_Mesh_CacheArray_ValidateState(3);
	// calculate a hashindex to choose a cache chain
	r->data = NULL;
	hashindex = CRC_Block((void *)r, sizeof(*r)) % RCACHEARRAY_HASHSIZE;

	// is it already cached?
	for (lhead = &r_mesh_rcachechain[hashindex], l = lhead->next;l != lhead;l = l->next)
	{
		if (!memcmp(&l->data->request, r, sizeof(l->data->request)))
		{
			// we have it cached already
			r->data = r_mesh_rcachedata + l->data->offset;
			return false;
		}
	}

	// we need to add a new cache item, this means finding a place for the new
	// data and making sure we have a free item available, lots of work...

	// check if buffer needs to wrap
	if (r_mesh_rcachedata_offset + r->data_size > r_mesh_rcachedata_size)
	{
		/*
		if (r->data_size * 10 > r_mesh_rcachedata_size)
		{
			// realloc whole cache
		}
		*/
		// reset back to start
		r_mesh_rcachedata_offset = 0;
		r_mesh_rcachesequentialchain_current = &r_mesh_rcachesequentialchain;
	}
	offset = r_mesh_rcachedata_offset;
	r_mesh_rcachedata_offset += r->data_size;
	offsetend = r_mesh_rcachedata_offset;
	//R_Mesh_CacheArray_ValidateState(4);

	/*
	{
		int n;
		for (lhead = &r_mesh_rcachesequentialchain, l = lhead->next, n = 0;l != lhead;l = l->next, n++);
		Con_Printf("R_Mesh_CacheArray: new data range %i:%i, %i items are already linked\n", offset, offsetend, n);
	}
	*/

	// make room for the new data (remove old items)
	lhead = &r_mesh_rcachesequentialchain;
	l = r_mesh_rcachesequentialchain_current;
	if (l == lhead)
		l = l->next;
	while (l != lhead && l->data->offset < offsetend && l->data->offset + l->data->request.data_size > offset)
	{
	//r_mesh_rcachesequentialchain_current = l;
	//R_Mesh_CacheArray_ValidateState(8);
		lnext = l->next;
		// if at the end of the chain, wrap around
		if (lnext == lhead)
			lnext = lnext->next;
	//r_mesh_rcachesequentialchain_current = lnext;
	//R_Mesh_CacheArray_ValidateState(10);

		// unlink from sequential chain
		l->next->prev = l->prev;
		l->prev->next = l->next;
	//R_Mesh_CacheArray_ValidateState(11);
		// link into free chain
		l->next = &r_mesh_rcachefreechain;
		l->prev = l->next->prev;
		l->next->prev = l->prev->next = l;
	//R_Mesh_CacheArray_ValidateState(12);

		l = &l->data->hashlink;
		// unlink from hash chain
		l->next->prev = l->prev;
		l->prev->next = l->next;

		l = lnext;
	//r_mesh_rcachesequentialchain_current = l;
	//R_Mesh_CacheArray_ValidateState(9);
	}
	//r_mesh_rcachesequentialchain_current = l;
	//R_Mesh_CacheArray_ValidateState(5);
	// gobble an extra item if we have no free items available
	if (r_mesh_rcachefreechain.next == &r_mesh_rcachefreechain)
	{
		lnext = l->next;

		// unlink from sequential chain
		l->next->prev = l->prev;
		l->prev->next = l->next;
		// link into free chain
		l->next = &r_mesh_rcachefreechain;
		l->prev = l->next->prev;
		l->next->prev = l->prev->next = l;

		l = &l->data->hashlink;
		// unlink from hash chain
		l->next->prev = l->prev;
		l->prev->next = l->next;

		l = lnext;
	}
	r_mesh_rcachesequentialchain_current = l;
	//R_Mesh_CacheArray_ValidateState(6);

	// now take an item from the free chain
	l = r_mesh_rcachefreechain.next;
	// set it up
	d = l->data;
	d->request = *r;
	d->offset = offset;
	// unlink
	l->next->prev = l->prev;
	l->prev->next = l->next;
	// relink to sequential
	l->next = r_mesh_rcachesequentialchain_current->prev;
	l->prev = l->next->prev;
	while (l->next->data && l->data && l->next->data->offset <= d->offset)
	{
		//Con_Printf(">\n");
		l->next = l->next->next;
		l->prev = l->prev->next;
	}
	while (l->prev->data && l->data && l->prev->data->offset >= d->offset)
	{
		//Con_Printf("<\n");
		l->prev = l->prev->prev;
		l->next = l->next->prev;
	}
	l->next->prev = l->prev->next = l;
	// also link into hash chain
	l = &l->data->hashlink;
	l->next = &r_mesh_rcachechain[hashindex];
	l->prev = l->next->prev;
	l->prev->next = l;
	l->next->prev = l->prev->next = l;


	//r_mesh_rcachesequentialchain_current = d->sequentiallink.next;

	//R_Mesh_CacheArray_ValidateState(7);
	// and finally set the data pointer
	r->data = r_mesh_rcachedata + d->offset;
	// and tell the caller to fill the array
	return true;
}

