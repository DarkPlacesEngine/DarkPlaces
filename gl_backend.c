
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"
#include "cl_collision.h"

cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1"};
cvar_t gl_mesh_testarrayelement = {0, "gl_mesh_testarrayelement", "0"};
cvar_t gl_mesh_testmanualfeeding = {0, "gl_mesh_testmanualfeeding", "0"};
cvar_t gl_delayfinish = {CVAR_SAVE, "gl_delayfinish", "0"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0"};

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
clockwise

A0B, 01B, B1C, 12C, C2D, 23D, D3E, 34E

*elements++ = i + row;
*elements++ = i;
*elements++ = i + row + 1;
*elements++ = i;
*elements++ = i + 1;
*elements++ = i + row + 1;


for (y = 0;y < rows - 1;y++)
{
	for (x = 0;x < columns - 1;x++)
	{
		i = y * rows + x;
		*elements++ = i + columns;
		*elements++ = i;
		*elements++ = i + columns + 1;
		*elements++ = i;
		*elements++ = i + 1;
		*elements++ = i + columns + 1;
	}
}

alternative:
0--1--2--3--4
| /| /|\ | /|
|/ |/ | \|/ |
A--B--C--D--E
counterclockwise

for (y = 0;y < rows - 1;y++)
{
	for (x = 0;x < columns - 1;x++)
	{
		i = y * rows + x;
		*elements++ = i;
		*elements++ = i + columns;
		*elements++ = i + columns + 1;
		*elements++ = i + columns;
		*elements++ = i + columns + 1;
		*elements++ = i + 1;
	}
}
*/

int polygonelements[768];

static void R_Mesh_CacheArray_Startup(void);
static void R_Mesh_CacheArray_Shutdown(void);
void GL_Backend_AllocArrays(void)
{
	if (!gl_backend_mempool)
		gl_backend_mempool = Mem_AllocPool("GL_Backend");
	R_Mesh_CacheArray_Startup();
}

void GL_Backend_FreeArrays(void)
{
	R_Mesh_CacheArray_Shutdown();
	Mem_FreePool(&gl_backend_mempool);
}

static void gl_backend_start(void)
{
	Con_DPrint("OpenGL Backend started\n");
	if (qglDrawRangeElements != NULL)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_maxdrawrangeelementsindices);
		CHECKGLERROR
		Con_DPrintf("glDrawRangeElements detected (max vertices %i, max indices %i)\n", gl_maxdrawrangeelementsvertices, gl_maxdrawrangeelementsindices);
	}

	backendunits = min(MAX_TEXTUREUNITS, gl_textureunits);

	GL_Backend_AllocArrays();

	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	backendunits = 0;
	backendactive = false;

	Con_DPrint("OpenGL Backend shutting down\n");

	GL_Backend_FreeArrays();
}

static void gl_backend_newmap(void)
{
}

cvar_t scr_zoomwindow = {CVAR_SAVE, "scr_zoomwindow", "0"};
cvar_t scr_zoomwindow_viewsizex = {CVAR_SAVE, "scr_zoomwindow_viewsizex", "20"};
cvar_t scr_zoomwindow_viewsizey = {CVAR_SAVE, "scr_zoomwindow_viewsizey", "20"};
cvar_t scr_zoomwindow_fov = {CVAR_SAVE, "scr_zoomwindow_fov", "20"};

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
	Cvar_RegisterVariable(&gl_paranoid);
	Cvar_RegisterVariable(&gl_printcheckerror);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
	Cvar_RegisterVariable(&gl_mesh_testarrayelement);
	Cvar_RegisterVariable(&gl_mesh_testmanualfeeding);

	Cvar_RegisterVariable(&scr_zoomwindow);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizex);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizey);
	Cvar_RegisterVariable(&scr_zoomwindow_fov);

	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
}

void GL_SetupView_Orientation_Identity (void)
{
	Matrix4x4_CreateIdentity(&backend_viewmatrix);
	memset(&backend_modelmatrix, 0, sizeof(backend_modelmatrix));
}

void GL_SetupView_Orientation_FromEntity(matrix4x4_t *matrix)
{
	matrix4x4_t tempmatrix, basematrix;
	Matrix4x4_Invert_Simple(&tempmatrix, matrix);
	Matrix4x4_CreateRotate(&basematrix, -90, 1, 0, 0);
	Matrix4x4_ConcatRotate(&basematrix, 90, 0, 0, 1);
	Matrix4x4_Concat(&backend_viewmatrix, &basematrix, &tempmatrix);
	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[2], 1, 0, 0);
	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[0], 0, 1, 0);
	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[1], 0, 0, 1);
	//Matrix4x4_ConcatTranslate(&backend_viewmatrix, -origin[0], -origin[1], -origin[2]);
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
	m[10] = -nudge;
	m[11] = -1;
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
	int arrayenabled;
	int arrayis3d;
	const void *pointer_texcoord;
	float rgbscale, alphascale;
	int combinergb, combinealpha;
	// FIXME: add more combine stuff
	// texmatrixenabled exists only to avoid unnecessary texmatrix compares
	int texmatrixenabled;
	matrix4x4_t matrix;
}
gltextureunit_t;

static struct
{
	int blendfunc1;
	int blendfunc2;
	int blend;
	GLboolean depthmask;
	int colormask; // stored as bottom 4 bits: r g b a (3 2 1 0 order)
	int depthtest;
	int scissortest;
	int unit;
	int clientunit;
	gltextureunit_t units[MAX_TEXTUREUNITS];
	float color4f[4];
	int lockrange_first;
	int lockrange_count;
	const void *pointer_vertex;
	const void *pointer_color;
}
gl_state;

void GL_SetupTextureState(void)
{
	int i;
	gltextureunit_t *unit;
	CHECKGLERROR
	gl_state.unit = -1;
	gl_state.clientunit = -1;
	for (i = 0;i < backendunits;i++)
	{
		GL_ActiveTexture(i);
		GL_ClientActiveTexture(i);
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
		unit->texmatrixenabled = false;
		unit->matrix = r_identitymatrix;
		qglMatrixMode(GL_TEXTURE);
		qglLoadIdentity();
		qglMatrixMode(GL_MODELVIEW);

		qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), NULL);CHECKGLERROR
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
	CHECKGLERROR
}

void GL_Backend_ResetState(void)
{
	memset(&gl_state, 0, sizeof(gl_state));
	gl_state.depthtest = true;
	gl_state.blendfunc1 = GL_ONE;
	gl_state.blendfunc2 = GL_ZERO;
	gl_state.blend = false;
	gl_state.depthmask = GL_TRUE;
	gl_state.colormask = 15;
	gl_state.color4f[0] = gl_state.color4f[1] = gl_state.color4f[2] = gl_state.color4f[3] = 1;
	gl_state.lockrange_first = 0;
	gl_state.lockrange_count = 0;
	gl_state.pointer_vertex = NULL;
	gl_state.pointer_color = NULL;

	CHECKGLERROR

	qglColorMask(1, 1, 1, 1);
	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglCullFace(GL_FRONT);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
	qglDisable(GL_BLEND);CHECKGLERROR
	qglDepthMask(gl_state.depthmask);CHECKGLERROR

	qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), NULL);CHECKGLERROR
	qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

	qglColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL);CHECKGLERROR
	qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR

	GL_Color(0, 0, 0, 0);
	GL_Color(1, 1, 1, 1);

	GL_SetupTextureState();
}

void GL_ActiveTexture(int num)
{
	if (gl_state.unit != num)
	{
		gl_state.unit = num;
		if (qglActiveTexture)
		{
			qglActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
			CHECKGLERROR
		}
	}
}

void GL_ClientActiveTexture(int num)
{
	if (gl_state.clientunit != num)
	{
		gl_state.clientunit = num;
		if (qglActiveTexture)
		{
			qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.clientunit);
			CHECKGLERROR
		}
	}
}

void GL_BlendFunc(int blendfunc1, int blendfunc2)
{
	if (gl_state.blendfunc1 != blendfunc1 || gl_state.blendfunc2 != blendfunc2)
	{
		if (r_showtrispass)
			return;
		qglBlendFunc(gl_state.blendfunc1 = blendfunc1, gl_state.blendfunc2 = blendfunc2);CHECKGLERROR
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
}

void GL_DepthMask(int state)
{
	if (gl_state.depthmask != state)
	{
		if (r_showtrispass)
			return;
		qglDepthMask(gl_state.depthmask = state);CHECKGLERROR
	}
}

void GL_DepthTest(int state)
{
	if (gl_state.depthtest != state)
	{
		if (r_showtrispass)
			return;
		gl_state.depthtest = state;
		if (gl_state.depthtest)
		{
			qglEnable(GL_DEPTH_TEST);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_DEPTH_TEST);CHECKGLERROR
		}
	}
}

void GL_ColorMask(int r, int g, int b, int a)
{
	int state = r*8 + g*4 + b*2 + a*1;
	if (gl_state.colormask != state)
	{
		if (r_showtrispass)
			return;
		gl_state.colormask = state;
		qglColorMask((GLboolean)r, (GLboolean)g, (GLboolean)b, (GLboolean)a);CHECKGLERROR
	}
}

void GL_Color(float cr, float cg, float cb, float ca)
{
	if (gl_state.pointer_color || gl_state.color4f[0] != cr || gl_state.color4f[1] != cg || gl_state.color4f[2] != cb || gl_state.color4f[3] != ca)
	{
		if (r_showtrispass)
			return;
		gl_state.color4f[0] = cr;
		gl_state.color4f[1] = cg;
		gl_state.color4f[2] = cb;
		gl_state.color4f[3] = ca;
		CHECKGLERROR
		qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);
		CHECKGLERROR
	}
}

void GL_ShowTrisColor(float cr, float cg, float cb, float ca)
{
	if (!r_showtrispass)
		return;
	r_showtrispass = false;
	GL_Color(cr * r_showtris.value, cg * r_showtris.value, cb * r_showtris.value, ca);
	r_showtrispass = true;
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
		if (count && gl_supportslockarrays && gl_lockarrays.integer && r_render.integer)
		{
			gl_state.lockrange_first = first;
			gl_state.lockrange_count = count;
			CHECKGLERROR
			qglLockArraysEXT(first, count);
			CHECKGLERROR
		}
	}
}

void GL_Scissor (int x, int y, int width, int height)
{
	CHECKGLERROR
	qglScissor(x, vid.realheight - (y + height),width,height);
	CHECKGLERROR
}

void GL_ScissorTest(int state)
{
	if(gl_state.scissortest == state)
		return;

	CHECKGLERROR
	if((gl_state.scissortest = state))
		qglEnable(GL_SCISSOR_TEST);
	else
		qglDisable(GL_SCISSOR_TEST);
	CHECKGLERROR
}

void GL_Clear(int mask)
{
	if (r_showtrispass)
		return;
	qglClear(mask);CHECKGLERROR
}

void GL_TransformToScreen(const vec4_t in, vec4_t out)
{
	vec4_t temp;
	float iw;
	Matrix4x4_Transform4 (&backend_viewmatrix, in, temp);
	Matrix4x4_Transform4 (&backend_projectmatrix, temp, out);
	iw = 1.0f / out[3];
	out[0] = r_view_x + (out[0] * iw + 1.0f) * r_view_width * 0.5f;
	out[1] = r_view_y + (out[1] * iw + 1.0f) * r_view_height * 0.5f;
	out[2] = out[2] * iw;
}

// called at beginning of frame
void R_Mesh_Start(void)
{
	BACKENDACTIVECHECK
	CHECKGLERROR
	GL_Backend_ResetState();
}

int gl_backend_rebindtextures;

void GL_Backend_RenumberElements(int *out, int count, const int *in, int offset)
{
	int i;
	if (offset)
	{
		for (i = 0;i < count;i++)
			*out++ = *in++ + offset;
	}
	else
		memcpy(out, in, sizeof(*out) * count);
}

// renders triangles using vertices from the active arrays
int paranoidblah = 0;
void R_Mesh_Draw(int numverts, int numtriangles, const int *elements)
{
	int numelements = numtriangles * 3;
	if (numverts == 0 || numtriangles == 0)
	{
		Con_Printf("R_Mesh_Draw(%d, %d, %08p);\n", numverts, numtriangles, elements);
		return;
	}
	CHECKGLERROR
	if (r_showtrispass)
	{
		R_Mesh_Draw_ShowTris(numverts, numtriangles, elements);
		return;
	}
	c_meshs++;
	c_meshelements += numelements;
	if (gl_paranoid.integer)
	{
		int i, j, size;
		const int *p;
		if (!qglIsEnabled(GL_VERTEX_ARRAY))
			Con_Print("R_Mesh_Draw: vertex array not enabled\n");
		for (j = 0, size = numverts * (int)sizeof(float[3]), p = gl_state.pointer_vertex;j < size;j += sizeof(int), p++)
			paranoidblah += *p;
		if (gl_state.pointer_color)
		{
			if (!qglIsEnabled(GL_COLOR_ARRAY))
				Con_Print("R_Mesh_Draw: color array set but not enabled\n");
			for (j = 0, size = numverts * (int)sizeof(float[4]), p = gl_state.pointer_color;j < size;j += sizeof(int), p++)
				paranoidblah += *p;
		}
		for (i = 0;i < backendunits;i++)
		{
			if (gl_state.units[i].t1d || gl_state.units[i].t2d || gl_state.units[i].t3d || gl_state.units[i].tcubemap || gl_state.units[i].arrayenabled)
			{
				if (gl_state.units[i].arrayenabled && !(gl_state.units[i].t1d || gl_state.units[i].t2d || gl_state.units[i].t3d || gl_state.units[i].tcubemap))
					Con_Print("R_Mesh_Draw: array enabled but no texture bound\n");
				GL_ClientActiveTexture(i);
				if (!qglIsEnabled(GL_TEXTURE_COORD_ARRAY))
					Con_Print("R_Mesh_Draw: texcoord array set but not enabled\n");
				for (j = 0, size = numverts * ((gl_state.units[i].t3d || gl_state.units[i].tcubemap) ? (int)sizeof(float[3]) : (int)sizeof(float[2])), p = gl_state.units[i].pointer_texcoord;j < size;j += sizeof(int), p++)
					paranoidblah += *p;
			}
		}
		for (i = 0;i < numtriangles * 3;i++)
		{
			if (elements[i] < 0 || elements[i] >= numverts)
			{
				Con_Printf("R_Mesh_Draw: invalid vertex index %i (outside range 0 - %i) in elements list\n", elements[i], numverts);
				return;
			}
		}
		CHECKGLERROR
	}
	if (r_render.integer)
	{
		CHECKGLERROR
		if (gl_mesh_testmanualfeeding.integer)
		{
			int i, j;
			const GLfloat *p;
			qglBegin(GL_TRIANGLES);
			for (i = 0;i < numtriangles * 3;i++)
			{
				for (j = 0;j < backendunits;j++)
				{
					if (gl_state.units[j].pointer_texcoord)
					{
						if (backendunits > 1)
						{
							if (gl_state.units[j].t3d || gl_state.units[j].tcubemap)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 3;
								qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 2;
								qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, p[0], p[1]);
							}
						}
						else
						{
							if (gl_state.units[j].t3d || gl_state.units[j].tcubemap)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 3;
								qglTexCoord3f(p[0], p[1], p[2]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 2;
								qglTexCoord2f(p[0], p[1]);
							}
						}
					}
				}
				if (gl_state.pointer_color)
				{
					p = ((const GLfloat *)(gl_state.pointer_color)) + elements[i] * 4;
					qglColor4f(p[0], p[1], p[2], p[3]);
				}
				p = ((const GLfloat *)(gl_state.pointer_vertex)) + elements[i] * 3;
				qglVertex3f(p[0], p[1], p[2]);
			}
			qglEnd();
			CHECKGLERROR
		}
		else if (gl_mesh_testarrayelement.integer)
		{
			int i;
			qglBegin(GL_TRIANGLES);
			for (i = 0;i < numtriangles * 3;i++)
			{
				qglArrayElement(elements[i]);
			}
			qglEnd();
			CHECKGLERROR
		}
		else if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
		{
			qglDrawRangeElements(GL_TRIANGLES, 0, numverts, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
		}
		else
		{
			qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, elements);CHECKGLERROR
		}
		CHECKGLERROR
	}
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	int i;
	BACKENDACTIVECHECK
	CHECKGLERROR
	GL_LockArrays(0, 0);
	CHECKGLERROR

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

void R_Mesh_State(const rmeshstate_t *m)
{
	int i, combinergb, combinealpha, scale;
	gltextureunit_t *unit;
	matrix4x4_t tempmatrix;

	BACKENDACTIVECHECK

	if (gl_state.pointer_vertex != m->pointer_vertex)
	{
		gl_state.pointer_vertex = m->pointer_vertex;
		CHECKGLERROR
		qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), gl_state.pointer_vertex);
		CHECKGLERROR
	}

	if (r_showtrispass)
		return;

	if (gl_state.pointer_color != m->pointer_color)
	{
		CHECKGLERROR
		if (!gl_state.pointer_color)
		{
			qglEnableClientState(GL_COLOR_ARRAY);
			CHECKGLERROR
		}
		else if (!m->pointer_color)
		{
			qglDisableClientState(GL_COLOR_ARRAY);
			CHECKGLERROR
			// when color array is on the glColor gets trashed, set it again
			qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);
			CHECKGLERROR
		}
		gl_state.pointer_color = m->pointer_color;
		qglColorPointer(4, GL_FLOAT, sizeof(float[4]), gl_state.pointer_color);
		CHECKGLERROR
	}

	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
	}

	for (i = 0, unit = gl_state.units;i < backendunits;i++, unit++)
	{
		// update 1d texture binding
		if (unit->t1d != m->tex1d[i])
		{
			if (m->tex1d[i])
			{
				if (unit->t1d == 0)
				{
					GL_ActiveTexture(i);
					qglEnable(GL_TEXTURE_1D);CHECKGLERROR
				}
				unit->t1d = m->tex1d[i];
				GL_ActiveTexture(i);
				qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
			}
			else
			{
				if (unit->t1d)
				{
					unit->t1d = 0;
					GL_ActiveTexture(i);
					qglDisable(GL_TEXTURE_1D);CHECKGLERROR
				}
			}
		}
		// update 2d texture binding
		if (unit->t2d != m->tex[i])
		{
			if (m->tex[i])
			{
				if (unit->t2d == 0)
				{
					GL_ActiveTexture(i);
					qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				}
				unit->t2d = m->tex[i];
				GL_ActiveTexture(i);
				qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
			}
			else
			{
				if (unit->t2d)
				{
					unit->t2d = 0;
					GL_ActiveTexture(i);
					qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				}
			}
		}
		// update 3d texture binding
		if (unit->t3d != m->tex3d[i])
		{
			if (m->tex3d[i])
			{
				if (unit->t3d == 0)
				{
					GL_ActiveTexture(i);
					qglEnable(GL_TEXTURE_3D);CHECKGLERROR
				}
				unit->t3d = m->tex3d[i];
				GL_ActiveTexture(i);
				qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
			}
			else
			{
				if (unit->t3d)
				{
					unit->t3d = 0;
					GL_ActiveTexture(i);
					qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				}
			}
		}
		// update cubemap texture binding
		if (unit->tcubemap != m->texcubemap[i])
		{
			if (m->texcubemap[i])
			{
				if (unit->tcubemap == 0)
				{
					GL_ActiveTexture(i);
					qglEnable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
				unit->tcubemap = m->texcubemap[i];
				GL_ActiveTexture(i);
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
			}
			else
			{
				if (unit->tcubemap)
				{
					unit->tcubemap = 0;
					GL_ActiveTexture(i);
					qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
			}
		}
		// update texture unit settings if the unit is enabled
		if (unit->t1d || unit->t2d || unit->t3d || unit->tcubemap)
		{
			// texture unit is enabled, enable the array
			if (!unit->arrayenabled)
			{
				unit->arrayenabled = true;
				GL_ClientActiveTexture(i);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			// update combine settings
			if (gl_combine.integer)
			{
				// GL_ARB_texture_env_combine
				combinergb = m->texcombinergb[i] ? m->texcombinergb[i] : GL_MODULATE;
				if (unit->combinergb != combinergb)
				{
					unit->combinergb = combinergb;
					GL_ActiveTexture(i);
					qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, unit->combinergb);CHECKGLERROR
				}
				combinealpha = m->texcombinealpha[i] ? m->texcombinealpha[i] : GL_MODULATE;
				if (unit->combinealpha != combinealpha)
				{
					unit->combinealpha = combinealpha;
					GL_ActiveTexture(i);
					qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, unit->combinealpha);CHECKGLERROR
				}
				scale = max(m->texrgbscale[i], 1);
				if (unit->rgbscale != scale)
				{
					GL_ActiveTexture(i);
					qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (unit->rgbscale = scale));CHECKGLERROR
				}
				scale = max(m->texalphascale[i], 1);
				if (unit->alphascale != scale)
				{
					GL_ActiveTexture(i);
					qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, (unit->alphascale = scale));CHECKGLERROR
				}
			}
			else
			{
				// normal GL texenv
				combinergb = m->texcombinergb[i] ? m->texcombinergb[i] : GL_MODULATE;
				if (unit->combinergb != combinergb)
				{
					unit->combinergb = combinergb;
					GL_ActiveTexture(i);
					qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combinergb);CHECKGLERROR
				}
			}
			// update array settings
			if (m->pointer_texcoord3f[i])
			{
				// 3d texcoord array
				if (unit->pointer_texcoord != m->pointer_texcoord3f[i] || !unit->arrayis3d)
				{
					unit->pointer_texcoord = m->pointer_texcoord3f[i];
					unit->arrayis3d = true;
					GL_ClientActiveTexture(i);
					qglTexCoordPointer(3, GL_FLOAT, sizeof(float[3]), unit->pointer_texcoord);
					CHECKGLERROR
				}
			}
			else
			{
				// 2d texcoord array
				if (unit->pointer_texcoord != m->pointer_texcoord[i] || unit->arrayis3d)
				{
					unit->pointer_texcoord = m->pointer_texcoord[i];
					unit->arrayis3d = false;
					GL_ClientActiveTexture(i);
					qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), unit->pointer_texcoord);
					CHECKGLERROR
				}
			}
			// update texmatrix
			if (m->texmatrix[i].m[3][3])
			{
				// texmatrix specified, check if it is different
				if (!unit->texmatrixenabled || memcmp(&unit->matrix, &m->texmatrix[i], sizeof(matrix4x4_t)))
				{
					unit->texmatrixenabled = true;
					unit->matrix = m->texmatrix[i];
					Matrix4x4_Transpose(&tempmatrix, &unit->matrix);
					qglMatrixMode(GL_TEXTURE);
					GL_ActiveTexture(i);
					qglLoadMatrixf(&tempmatrix.m[0][0]);
					qglMatrixMode(GL_MODELVIEW);
				}
			}
			else
			{
				// no texmatrix specified, revert to identity
				if (unit->texmatrixenabled)
				{
					unit->texmatrixenabled = false;
					qglMatrixMode(GL_TEXTURE);
					GL_ActiveTexture(i);
					qglLoadIdentity();
					qglMatrixMode(GL_MODELVIEW);
				}
			}
		}
		else
		{
			// texture unit is disabled, disable the array
			if (unit->arrayenabled)
			{
				unit->arrayenabled = false;
				GL_ClientActiveTexture(i);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			// no need to update most settings on a disabled texture unit
		}
	}
}

void R_Mesh_Draw_ShowTris(int numverts, int numtriangles, const int *elements)
{
	qglBegin(GL_LINES);
	for (;numtriangles;numtriangles--, elements += 3)
	{
		qglArrayElement(elements[0]);qglArrayElement(elements[1]);
		qglArrayElement(elements[1]);qglArrayElement(elements[2]);
		qglArrayElement(elements[2]);qglArrayElement(elements[0]);
	}
	qglEnd();
	CHECKGLERROR
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height, qboolean jpeg)
{
	qboolean ret;
	qbyte *buffer;

	if (!r_render.integer)
		return false;

	buffer = Mem_Alloc(tempmempool, width*height*3);
	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	CHECKGLERROR

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
		GL_Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (gl_stencil ? GL_STENCIL_BUFFER_BIT : 0));
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
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	// calculate vision size and alter by aspect, then convert back to angle
	return atan (height / (width / tan(fov_x/360*M_PI))) * 360 / M_PI;
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
		R_Mesh_Finish();
		R_TimeReport("meshfinish");
		VID_Finish();
		R_TimeReport("finish");
	}

	R_Mesh_Start();

	if (r_textureunits.integer > gl_textureunits)
		Cvar_SetValueQuick(&r_textureunits, gl_textureunits);
	if (r_textureunits.integer < 1)
		Cvar_SetValueQuick(&r_textureunits, 1);

	if (gl_combine.integer && (!gl_combine_extension || r_textureunits.integer < 2))
		Cvar_SetValueQuick(&gl_combine, 0);

showtris:
	R_TimeReport("setup");

	R_ClearScreen();

	R_TimeReport("clear");

	if (scr_conlines < vid.conheight && cls.signon == SIGNONS)
	{
		float size;
		int contents;

		// bound viewsize
		if (scr_viewsize.value < 30)
			Cvar_Set ("viewsize","30");
		if (scr_viewsize.value > 120)
			Cvar_Set ("viewsize","120");
		
		// bound field of view
		if (scr_fov.value < 1)
			Cvar_Set ("fov","1");
		if (scr_fov.value > 170)
			Cvar_Set ("fov","170");
	
		// intermission is always full screen
		if (cl.intermission)
		{
			size = 1;
			sb_lines = 0;
		}
		else
		{
			if (scr_viewsize.value >= 120)
				sb_lines = 0;		// no status bar at all
			else if (scr_viewsize.value >= 110)
				sb_lines = 24;		// no inventory
			else
				sb_lines = 24+16+8;
			size = scr_viewsize.value * (1.0 / 100.0);
			size = min(size, 1);
		}
	
		r_refdef.width = vid.realwidth * size;
		r_refdef.height = vid.realheight * size;
		r_refdef.x = (vid.realwidth - r_refdef.width)/2;
		r_refdef.y = (vid.realheight - r_refdef.height)/2;
	
		// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
		r_refdef.fov_x = scr_fov.value * cl.viewzoom;
		r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.width, r_refdef.height);
	
		if (cl.worldmodel)
		{
			Mod_CheckLoaded(cl.worldmodel);
			contents = CL_PointSuperContents(r_vieworigin);
			if (contents & SUPERCONTENTS_LIQUIDSMASK)
			{
				r_refdef.fov_x *= (sin(cl.time * 4.7) * 0.015 + 0.985);
				r_refdef.fov_y *= (sin(cl.time * 3.0) * 0.015 + 0.985);
			}
		}

		R_RenderView();

		if (scr_zoomwindow.integer)
		{
			float sizex = bound(10, scr_zoomwindow_viewsizex.value, 100) / 100.0;
			float sizey = bound(10, scr_zoomwindow_viewsizey.value, 100) / 100.0;
			r_refdef.width = vid.realwidth * sizex;
			r_refdef.height = vid.realheight * sizey;
			r_refdef.x = (vid.realwidth - r_refdef.width)/2;
			r_refdef.y = 0;
			r_refdef.fov_x = scr_zoomwindow_fov.value;
			r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.width, r_refdef.height);

			R_RenderView();
		}
	}

	// draw 2D stuff
	R_DrawQueue();

	if (r_showtrispass)
		r_showtrispass = false;
	else if (r_showtris.value > 0)
	{
		rmeshstate_t m;
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_DepthTest(GL_FALSE);
		GL_DepthMask(GL_FALSE);
		memset(&m, 0, sizeof(m));
		R_Mesh_State(&m);
		r_showtrispass = true;
		GL_ShowTrisColor(0.2,0.2,0.2,1);
		goto showtris;
	}

	if (gl_delayfinish.integer)
	{
		// tell driver to commit it's partially full geometry queue to the rendering queue
		// (this doesn't wait for the commands themselves to complete)
		qglFlush();
	}
	else
	{
		R_Mesh_Finish();
		R_TimeReport("meshfinish");
		VID_Finish();
		R_TimeReport("finish");
	}
}


//===========================================================================
// dynamic vertex array buffer subsystem
//===========================================================================

float varray_vertex3f[65536*3];
float varray_color4f[65536*4];
float varray_texcoord2f[4][65536*2];
float varray_texcoord3f[4][65536*3];
float varray_normal3f[65536*3];
int earray_element3i[65536];

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
		//Con_Print(">\n");
		l->next = l->next->next;
		l->prev = l->prev->next;
	}
	while (l->prev->data && l->data && l->prev->data->offset >= d->offset)
	{
		//Con_Print("<\n");
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

