
#include "quakedef.h"
#include "image.h"

cvar_t gl_mesh_maxverts = {0, "gl_mesh_maxverts", "1024"};
cvar_t gl_mesh_floatcolors = {0, "gl_mesh_floatcolors", "1"};
cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1"};

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
float *varray_vertex;
float *varray_color;
float *varray_texcoord[MAX_TEXTUREUNITS];
int mesh_maxverts;

static matrix4x4_t backend_viewmatrix;
static matrix4x4_t backend_modelmatrix;
static matrix4x4_t backend_modelviewmatrix;
static matrix4x4_t backend_glmodelviewmatrix;
static matrix4x4_t backend_projectmatrix;

static int backendunits, backendactive;
static qbyte *varray_bcolor;
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

int polygonelements[768];

void GL_Backend_AllocArrays(void)
{
	int i;

	for (i = 0;i < POLYGONELEMENTS_MAXPOINTS - 2;i++)
	{
		polygonelements[i * 3 + 0] = 0;
		polygonelements[i * 3 + 1] = i + 1;
		polygonelements[i * 3 + 2] = i + 2;
	}

	if (!gl_backend_mempool)
		gl_backend_mempool = Mem_AllocPool("GL_Backend");

	varray_vertex = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[4]));
	varray_color = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[4]));
	varray_bcolor = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(qbyte[4]));
	for (i = 0;i < backendunits;i++)
		varray_texcoord[i] = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[4]));
	for (;i < MAX_TEXTUREUNITS;i++)
		varray_texcoord[i] = NULL;
}

void GL_Backend_FreeArrays(int resizingbuffers)
{
	int i;
	if (resizingbuffers)
		Mem_EmptyPool(gl_backend_mempool);
	else
		Mem_FreePool(&gl_backend_mempool);
	varray_vertex = NULL;
	varray_color = NULL;
	varray_bcolor = NULL;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
		varray_texcoord[i] = NULL;
}

static void gl_backend_start(void)
{
	Con_Printf("OpenGL Backend started with gl_mesh_maxverts %i\n", gl_mesh_maxverts.integer);
	if (qglDrawRangeElements != NULL)
	{
		qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
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

	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	backendunits = 0;
	backendactive = false;

	Con_Printf("OpenGL Backend shutting down\n");

	GL_Backend_FreeArrays(false);
}

void GL_Backend_CheckCvars(void)
{
	// 21760 is (65536 / 3) rounded off to a multiple of 128
	if (gl_mesh_maxverts.integer < 1024)
		Cvar_SetValueQuick(&gl_mesh_maxverts, 1024);
	if (gl_mesh_maxverts.integer > 21760)
		Cvar_SetValueQuick(&gl_mesh_maxverts, 21760);
}

void GL_Backend_ResizeArrays(int numtriangles)
{
	Cvar_SetValueQuick(&gl_mesh_maxverts, numtriangles);
	GL_Backend_CheckCvars();
	mesh_maxverts = gl_mesh_maxverts.integer;
	GL_Backend_FreeArrays(true);
	GL_Backend_AllocArrays();
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

	Cvar_RegisterVariable(&gl_mesh_maxverts);
	Cvar_RegisterVariable(&gl_mesh_floatcolors);
	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
	GL_Backend_CheckCvars();
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
	qglLoadIdentity ();CHECKGLERROR
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
	double nudge, m[16];

	if (!r_render.integer)
		return;

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity ();CHECKGLERROR
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
	qglLoadMatrixd(m);
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
	qglLoadIdentity ();CHECKGLERROR
	qglOrtho(x1, x2, y2, y1, zNear, zFar);
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
}

typedef struct gltextureunit_s
{
	unsigned int t1d, t2d, t3d, tcubemap;
	unsigned int arrayenabled;
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
		unit->rgbscale = 1;
		unit->alphascale = 1;
		unit->combinergb = GL_MODULATE;
		unit->combinealpha = GL_MODULATE;
		unit->arrayenabled = false;
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		if (gl_texture3d)
		{
			qglTexCoordPointer(3, GL_FLOAT, sizeof(float[4]), varray_texcoord[i]);CHECKGLERROR
		}
		else
		{
			qglTexCoordPointer(2, GL_FLOAT, sizeof(float[4]), varray_texcoord[i]);CHECKGLERROR
		}
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

	qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglCullFace(GL_FRONT);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
	qglDisable(GL_BLEND);CHECKGLERROR
	qglDepthMask(gl_state.depthmask);CHECKGLERROR
	qglVertexPointer(3, GL_FLOAT, sizeof(float[4]), varray_vertex);CHECKGLERROR
	qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
	if (gl_mesh_floatcolors.integer)
	{
		qglColorPointer(4, GL_FLOAT, sizeof(float[4]), varray_color);CHECKGLERROR
	}
	else
	{
		qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(qbyte[4]), varray_bcolor);CHECKGLERROR
	}
	GL_Color(1, 1, 1, 1);

	GL_SetupTextureState();
}

void GL_UseColorArray(void)
{
	if (!gl_state.colorarray)
	{
		gl_state.colorarray = true;
		qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	}
}

void GL_Color(float cr, float cg, float cb, float ca)
{
	if (gl_state.colorarray)
	{
		gl_state.colorarray = false;
		qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	}
	qglColor4f(cr, cg, cb, ca);
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
	if (mesh_maxverts != gl_mesh_maxverts.integer)
		GL_Backend_ResizeArrays(gl_mesh_maxverts.integer);

	GL_Backend_ResetState();
}

int gl_backend_rebindtextures;

void GL_ConvertColorsFloatToByte(int numverts)
{
	int i, k, total;
	// LordHavoc: to avoid problems with aliasing (treating memory as two
	// different types - exactly what this is doing), these must be volatile
	// (or a union)
	volatile int *icolor;
	volatile float *fcolor;
	qbyte *bcolor;

	total = numverts * 4;

	// shift float to have 8bit fraction at base of number
	fcolor = varray_color;
	for (i = 0;i < total;)
	{
		fcolor[i    ] += 32768.0f;
		fcolor[i + 1] += 32768.0f;
		fcolor[i + 2] += 32768.0f;
		fcolor[i + 3] += 32768.0f;
		i += 4;
	}

	// then read as integer and kill float bits...
	icolor = (int *)varray_color;
	bcolor = varray_bcolor;
	for (i = 0;i < total;)
	{
		k = icolor[i    ] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i    ] = (qbyte) k;
		k = icolor[i + 1] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 1] = (qbyte) k;
		k = icolor[i + 2] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 2] = (qbyte) k;
		k = icolor[i + 3] & 0x7FFFFF;if (k > 255) k = 255;bcolor[i + 3] = (qbyte) k;
		i += 4;
	}
}

void GL_DrawRangeElements(int firstvert, int endvert, int indexcount, const int *index)
{
	int arraylocked = false;
	c_meshs++;
	c_meshelements += indexcount;
	if (indexcount == 0 || endvert == firstvert)
	{
		Con_Printf("GL_DrawRangeElements(%d, %d, %d, %08p);\n", firstvert, endvert, indexcount, index);
		return;
	}
	if (gl_supportslockarrays && gl_lockarrays.integer)
	{
		qglLockArraysEXT(firstvert, endvert - firstvert);
		CHECKGLERROR
		arraylocked = true;
	}
	if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
		qglDrawRangeElements(GL_TRIANGLES, firstvert, endvert, indexcount, GL_UNSIGNED_INT, (const GLuint *) index);
	else
		qglDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, (const GLuint *) index);
	CHECKGLERROR
	if (arraylocked)
	{
		qglUnlockArraysEXT();
		CHECKGLERROR
		arraylocked = false;
	}
}

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

// renders the mesh
void R_Mesh_Draw(int numverts, int numtriangles, const int *elements)
{
	BACKENDACTIVECHECK

	CHECKGLERROR

	if (gl_state.colorarray && !gl_mesh_floatcolors.integer)
		GL_ConvertColorsFloatToByte(numverts);
	if (!r_render.integer)
		return;
	GL_DrawRangeElements(0, numverts, numtriangles * 3, elements);
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	int i;
	BACKENDACTIVECHECK

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
		backend_modelmatrix = *matrix;
		Matrix4x4_Concat(&backend_modelviewmatrix, &backend_viewmatrix, matrix);
		Matrix4x4_Transpose(&backend_glmodelviewmatrix, &backend_modelviewmatrix);
		qglLoadMatrixf(&backend_glmodelviewmatrix.m[0][0]);
	}
}

// sets up the requested state
void R_Mesh_MainState(const rmeshstate_t *m)
{
	BACKENDACTIVECHECK

	if (gl_state.blendfunc1 != m->blendfunc1 || gl_state.blendfunc2 != m->blendfunc2)
	{
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
		gl_state.depthdisable = m->depthdisable;
		if (gl_state.depthdisable)
			qglDisable(GL_DEPTH_TEST);
		else
			qglEnable(GL_DEPTH_TEST);
	}
	if (gl_state.depthmask != (m->blendfunc2 == GL_ZERO || m->depthwrite))
	{
		qglDepthMask(gl_state.depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite));CHECKGLERROR
	}
}

void R_Mesh_TextureState(const rmeshstate_t *m)
{
	int i, combinergb, combinealpha;
	float scale;
	gltextureunit_t *unit;

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
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			if (m->tex1d[i] || m->tex[i] || m->tex3d[i] || m->texcubemap[i])
			{
				if (!unit->arrayenabled)
				{
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
					unit->arrayenabled = false;
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			combinergb = m->texcombinergb[i];
			combinealpha = m->texcombinealpha[i];
			if (!combinergb)
				combinergb = GL_MODULATE;
			if (!combinealpha)
				combinealpha = GL_MODULATE;
			if (unit->combinergb != combinergb)
			{
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
			if (unit->combinealpha != combinealpha)
			{
				unit->combinealpha = combinealpha;
				if (gl_combine.integer)
				{
					qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, unit->combinealpha);CHECKGLERROR
				}
			}
			if (unit->t1d != m->tex1d[i])
			{
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
		scale = max(m->texrgbscale[i], 1);
		if (gl_state.units[i].rgbscale != scale)
		{
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (gl_state.units[i].rgbscale = scale));CHECKGLERROR
		}
		scale = max(m->texalphascale[i], 1);
		if (gl_state.units[i].alphascale != scale)
		{
			if (gl_state.unit != i)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
			}
			qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, (gl_state.units[i].alphascale = scale));CHECKGLERROR
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
		qglClearDepth(1);CHECKGLERROR
		if (gl_stencil)
		{
			qglClearStencil(0);CHECKGLERROR
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
	VID_Finish ();

	R_TimeReport("finish");

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

	// tell driver to commit it's partially full geometry queue to the rendering queue
	// (this doesn't wait for the commands themselves to complete)
	qglFlush();
}

