
#include "quakedef.h"
#include "image.h"
#include "jpeg.h"
#include "cl_collision.h"

cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1"};
cvar_t gl_mesh_testarrayelement = {0, "gl_mesh_testarrayelement", "0"};
cvar_t gl_mesh_testmanualfeeding = {0, "gl_mesh_testmanualfeeding", "0"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0"};
cvar_t r_stereo_separation = {0, "r_stereo_separation", "4"};
cvar_t r_stereo_sidebyside = {0, "r_stereo_sidebyside", "0"};
cvar_t r_stereo_redblue = {0, "r_stereo_redblue", "0"};
cvar_t r_stereo_redcyan = {0, "r_stereo_redcyan", "0"};
cvar_t r_stereo_redgreen = {0, "r_stereo_redgreen", "0"};

cvar_t r_render = {0, "r_render", "1"};
cvar_t r_waterwarp = {CVAR_SAVE, "r_waterwarp", "1"};
cvar_t gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1"};
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

void SCR_ScreenShot_f (void);

static matrix4x4_t backend_viewmatrix;
static matrix4x4_t backend_modelmatrix;
static matrix4x4_t backend_modelviewmatrix;
static matrix4x4_t backend_glmodelviewmatrix;
static matrix4x4_t backend_projectmatrix;

static unsigned int backendunits, backendimageunits, backendarrayunits, backendactive;

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
	R_Mesh_CacheArray_Startup();
}

void GL_Backend_FreeArrays(void)
{
	R_Mesh_CacheArray_Shutdown();
}

static void gl_backend_start(void)
{
	Con_Print("OpenGL Backend starting...\n");
	CHECKGLERROR

	if (qglDrawRangeElements != NULL)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_maxdrawrangeelementsindices);
		CHECKGLERROR
		Con_Printf("glDrawRangeElements detected (max vertices %i, max indices %i)\n", gl_maxdrawrangeelementsvertices, gl_maxdrawrangeelementsindices);
	}

	backendunits = min(MAX_TEXTUREUNITS, gl_textureunits);
	backendimageunits = backendunits;
	backendarrayunits = backendunits;
	if (gl_support_fragment_shader)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &backendimageunits);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &backendarrayunits);
		CHECKGLERROR
		Con_Printf("GLSL shader support detected: texture units = %i texenv, %i image, %i array\n", backendunits, backendimageunits, backendarrayunits);
	}
	else if (backendunits > 1)
		Con_Printf("multitexture detected: texture units = %i\n", backendunits);
	else
		Con_Printf("singletexture\n");

	GL_Backend_AllocArrays();

	Con_Printf("OpenGL backend started.\n");

	CHECKGLERROR

	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	backendunits = 0;
	backendimageunits = 0;
	backendarrayunits = 0;
	backendactive = false;

	Con_Print("OpenGL Backend shutting down\n");

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
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&r_stereo_separation);
	Cvar_RegisterVariable(&r_stereo_sidebyside);
	Cvar_RegisterVariable(&r_stereo_redblue);
	Cvar_RegisterVariable(&r_stereo_redcyan);
	Cvar_RegisterVariable(&r_stereo_redgreen);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_lockarrays);
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
	double m[16];

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
	qglGetDoublev(GL_PROJECTION_MATRIX, m);
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
	double m[16];

	if (!r_render.integer)
		return;

	// set up viewpoint
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity();CHECKGLERROR
	qglOrtho(x1, x2, y2, y1, zNear, zFar);
	qglGetDoublev(GL_PROJECTION_MATRIX, m);
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
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
}

typedef struct gltextureunit_s
{
	int t1d, t2d, t3d, tcubemap;
	int arrayenabled;
	unsigned int arraycomponents;
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
	unsigned int unit;
	unsigned int clientunit;
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
	unsigned int i;
	gltextureunit_t *unit;
	CHECKGLERROR
	gl_state.unit = -1;
	gl_state.clientunit = -1;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
	{
		unit = gl_state.units + i;
		unit->t1d = 0;
		unit->t2d = 0;
		unit->t3d = 0;
		unit->tcubemap = 0;
		unit->arrayenabled = false;
		unit->arraycomponents = 0;
		unit->pointer_texcoord = NULL;
		unit->rgbscale = 1;
		unit->alphascale = 1;
		unit->combinergb = GL_MODULATE;
		unit->combinealpha = GL_MODULATE;
		unit->texmatrixenabled = false;
		unit->matrix = r_identitymatrix;
	}

	for (i = 0;i < backendimageunits;i++)
	{
		GL_ActiveTexture(i);
		qglBindTexture(GL_TEXTURE_1D, 0);CHECKGLERROR
		qglBindTexture(GL_TEXTURE_2D, 0);CHECKGLERROR
		if (gl_texture3d)
		{
			qglBindTexture(GL_TEXTURE_3D, 0);CHECKGLERROR
		}
		if (gl_texturecubemap)
		{
			qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);CHECKGLERROR
		}
	}

	for (i = 0;i < backendarrayunits;i++)
	{
		GL_ClientActiveTexture(i);
		qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), NULL);CHECKGLERROR
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
	}

	for (i = 0;i < backendunits;i++)
	{
		GL_ActiveTexture(i);
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
		qglMatrixMode(GL_TEXTURE);
		qglLoadIdentity();
		qglMatrixMode(GL_MODELVIEW);
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

void GL_ActiveTexture(unsigned int num)
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

void GL_ClientActiveTexture(unsigned int num)
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
	out[2] = r_view_z + (out[2] * iw + 1.0f) * r_view_depth * 0.5f;
}

// called at beginning of frame
void R_Mesh_Start(void)
{
	BACKENDACTIVECHECK
	CHECKGLERROR
	GL_Backend_ResetState();
}

unsigned int GL_Backend_CompileProgram(int vertexstrings_count, const char **vertexstrings_list, int fragmentstrings_count, const char **fragmentstrings_list)
{
	GLint vertexshadercompiled, fragmentshadercompiled, programlinked;
	GLuint vertexshaderobject, fragmentshaderobject, programobject = 0;
	char compilelog[4096];
	CHECKGLERROR

	programobject = qglCreateProgramObjectARB();
	CHECKGLERROR
	if (!programobject)
		return 0;

	if (vertexstrings_count)
	{
		CHECKGLERROR
		vertexshaderobject = qglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		if (!vertexshaderobject)
		{
			qglDeleteObjectARB(programobject);
			CHECKGLERROR
			return 0;
		}
		qglShaderSourceARB(vertexshaderobject, vertexstrings_count, vertexstrings_list, NULL);
		qglCompileShaderARB(vertexshaderobject);
		CHECKGLERROR
		qglGetObjectParameterivARB(vertexshaderobject, GL_OBJECT_COMPILE_STATUS_ARB, &vertexshadercompiled);
		qglGetInfoLogARB(vertexshaderobject, sizeof(compilelog), NULL, compilelog);
		if (compilelog[0])
			Con_Printf("vertex shader compile log:\n%s\n", compilelog);
		if (!vertexshadercompiled)
		{
			qglDeleteObjectARB(programobject);
			qglDeleteObjectARB(vertexshaderobject);
			CHECKGLERROR
			return 0;
		}
		qglAttachObjectARB(programobject, vertexshaderobject);
		qglDeleteObjectARB(vertexshaderobject);
		CHECKGLERROR
	}

	if (fragmentstrings_count)
	{
		CHECKGLERROR
		fragmentshaderobject = qglCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		if (!fragmentshaderobject)
		{
			qglDeleteObjectARB(programobject);
			CHECKGLERROR
			return 0;
		}
		qglShaderSourceARB(fragmentshaderobject, fragmentstrings_count, fragmentstrings_list, NULL);
		qglCompileShaderARB(fragmentshaderobject);
		CHECKGLERROR
		qglGetObjectParameterivARB(fragmentshaderobject, GL_OBJECT_COMPILE_STATUS_ARB, &fragmentshadercompiled);
		qglGetInfoLogARB(fragmentshaderobject, sizeof(compilelog), NULL, compilelog);
		if (compilelog[0])
			Con_Printf("fragment shader compile log:\n%s\n", compilelog);
		if (!fragmentshadercompiled)
		{
			qglDeleteObjectARB(programobject);
			qglDeleteObjectARB(fragmentshaderobject);
			CHECKGLERROR
			return 0;
		}
		qglAttachObjectARB(programobject, fragmentshaderobject);
		qglDeleteObjectARB(fragmentshaderobject);
		CHECKGLERROR
	}

	qglLinkProgramARB(programobject);
	CHECKGLERROR
	qglGetObjectParameterivARB(programobject, GL_OBJECT_LINK_STATUS_ARB, &programlinked);
	qglGetInfoLogARB(programobject, sizeof(compilelog), NULL, compilelog);
	if (compilelog[0])
	{
		Con_Printf("program link log:\n%s\n", compilelog);
		// software vertex shader is ok but software fragment shader is WAY
		// too slow, fail program if so.
		// NOTE: this string might be ATI specific, but that's ok because the
		// ATI R300 chip (Radeon 9500-9800/X300) is the most likely to use a
		// software fragment shader due to low instruction and dependent
		// texture limits.
		if (strstr(compilelog, "fragment shader will run in software"))
			programlinked = false;
	}
	CHECKGLERROR
	if (!programlinked)
	{
		qglDeleteObjectARB(programobject);
		return 0;
	}
	CHECKGLERROR
	return programobject;
}

void GL_Backend_FreeProgram(unsigned int prog)
{
	CHECKGLERROR
	qglDeleteObjectARB(prog);
	CHECKGLERROR
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
void R_Mesh_Draw(int firstvertex, int numvertices, int numtriangles, const int *elements)
{
	unsigned int numelements = numtriangles * 3;
	if (numvertices < 3 || numtriangles < 1)
	{
		Con_Printf("R_Mesh_Draw(%d, %d, %d, %08p);\n", firstvertex, numvertices, numtriangles, elements);
		return;
	}
	//CHECKGLERROR
	if (r_showtrispass)
	{
		R_Mesh_Draw_ShowTris(firstvertex, numvertices, numtriangles, elements);
		return;
	}
	c_meshs++;
	c_meshelements += numelements;
	if (gl_paranoid.integer)
	{
		unsigned int i, j, size;
		const int *p;
		if (!qglIsEnabled(GL_VERTEX_ARRAY))
			Con_Print("R_Mesh_Draw: vertex array not enabled\n");
		for (j = 0, size = numvertices * 3, p = (int *)((float *)gl_state.pointer_vertex + firstvertex * 3);j < size;j++, p++)
			paranoidblah += *p;
		if (gl_state.pointer_color)
		{
			if (!qglIsEnabled(GL_COLOR_ARRAY))
				Con_Print("R_Mesh_Draw: color array set but not enabled\n");
			for (j = 0, size = numvertices * 4, p = (int *)((float *)gl_state.pointer_color + firstvertex * 4);j < size;j++, p++)
				paranoidblah += *p;
		}
		for (i = 0;i < backendarrayunits;i++)
		{
			if (gl_state.units[i].arrayenabled)
			{
				GL_ClientActiveTexture(i);
				if (!qglIsEnabled(GL_TEXTURE_COORD_ARRAY))
					Con_Print("R_Mesh_Draw: texcoord array set but not enabled\n");
				for (j = 0, size = numvertices * gl_state.units[i].arraycomponents, p = (int *)((float *)gl_state.units[i].pointer_texcoord + firstvertex * gl_state.units[i].arraycomponents);j < size;j++, p++)
					paranoidblah += *p;
			}
		}
		for (i = 0;i < (unsigned int) numtriangles * 3;i++)
		{
			if (elements[i] < firstvertex || elements[i] >= firstvertex + numvertices)
			{
				Con_Printf("R_Mesh_Draw: invalid vertex index %i (outside range %i - %i) in elements list\n", elements[i], firstvertex, firstvertex + numvertices);
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
			unsigned int i, j;
			const GLfloat *p;
			qglBegin(GL_TRIANGLES);
			for (i = 0;i < (unsigned int) numtriangles * 3;i++)
			{
				for (j = 0;j < backendarrayunits;j++)
				{
					if (gl_state.units[j].pointer_texcoord)
					{
						if (backendarrayunits > 1)
						{
							if (gl_state.units[j].arraycomponents == 4)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 4;
								qglMultiTexCoord4f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.units[j].arraycomponents == 3)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 3;
								qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2]);
							}
							else if (gl_state.units[j].arraycomponents == 2)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 2;
								qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, p[0], p[1]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 1;
								qglMultiTexCoord1f(GL_TEXTURE0_ARB + j, p[0]);
							}
						}
						else
						{
							if (gl_state.units[j].arraycomponents == 4)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 4;
								qglTexCoord4f(p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.units[j].arraycomponents == 3)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 3;
								qglTexCoord3f(p[0], p[1], p[2]);
							}
							else if (gl_state.units[j].arraycomponents == 2)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 2;
								qglTexCoord2f(p[0], p[1]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + elements[i] * 1;
								qglTexCoord1f(p[0]);
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
			qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices, numelements, GL_UNSIGNED_INT, elements);
			CHECKGLERROR
		}
		else
		{
			qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, elements);
			CHECKGLERROR
		}
	}
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	unsigned int i;
	BACKENDACTIVECHECK
	CHECKGLERROR
	GL_LockArrays(0, 0);
	CHECKGLERROR

	for (i = 0;i < backendimageunits;i++)
	{
		GL_ActiveTexture(i);
		qglBindTexture(GL_TEXTURE_1D, 0);CHECKGLERROR
		qglBindTexture(GL_TEXTURE_2D, 0);CHECKGLERROR
		if (gl_texture3d)
		{
			qglBindTexture(GL_TEXTURE_3D, 0);CHECKGLERROR
		}
		if (gl_texturecubemap)
		{
			qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);CHECKGLERROR
		}
	}
	for (i = 0;i < backendarrayunits;i++)
	{
		GL_ActiveTexture(backendarrayunits - 1 - i);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
	}
	for (i = 0;i < backendunits;i++)
	{
		GL_ActiveTexture(backendunits - 1 - i);
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

void R_Mesh_VertexPointer(const float *vertex3f)
{
	if (gl_state.pointer_vertex != vertex3f)
	{
		gl_state.pointer_vertex = vertex3f;
		CHECKGLERROR
		qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), gl_state.pointer_vertex);
		CHECKGLERROR
	}
}

void R_Mesh_ColorPointer(const float *color4f)
{
	if (gl_state.pointer_color != color4f)
	{
		CHECKGLERROR
		if (!gl_state.pointer_color)
		{
			qglEnableClientState(GL_COLOR_ARRAY);
			CHECKGLERROR
		}
		else if (!color4f)
		{
			qglDisableClientState(GL_COLOR_ARRAY);
			CHECKGLERROR
			// when color array is on the glColor gets trashed, set it again
			qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);
			CHECKGLERROR
		}
		gl_state.pointer_color = color4f;
		qglColorPointer(4, GL_FLOAT, sizeof(float[4]), gl_state.pointer_color);
		CHECKGLERROR
	}
}

void R_Mesh_TexCoordPointer(unsigned int unitnum, unsigned int numcomponents, const float *texcoord)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	// update array settings
	if (texcoord)
	{
		// texcoord array
		if (unit->pointer_texcoord != texcoord || unit->arraycomponents != numcomponents)
		{
			unit->pointer_texcoord = texcoord;
			unit->arraycomponents = numcomponents;
			GL_ClientActiveTexture(unitnum);
			qglTexCoordPointer(unit->arraycomponents, GL_FLOAT, sizeof(float) * unit->arraycomponents, unit->pointer_texcoord);
			CHECKGLERROR
		}
		// texture array unit is enabled, enable the array
		if (!unit->arrayenabled)
		{
			unit->arrayenabled = true;
			GL_ClientActiveTexture(unitnum);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
	else
	{
		// texture array unit is disabled, disable the array
		if (unit->arrayenabled)
		{
			unit->arrayenabled = false;
			GL_ClientActiveTexture(unitnum);
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
}

void R_Mesh_TexBindAll(unsigned int unitnum, int tex1d, int tex2d, int tex3d, int texcubemap)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendunits)
		return;
	// update 1d texture binding
	if (unit->t1d != tex1d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (tex1d)
			{
				if (unit->t1d == 0)
					qglEnable(GL_TEXTURE_1D);
			}
			else
			{
				if (unit->t1d)
					qglDisable(GL_TEXTURE_1D);
			}
		}
		unit->t1d = tex1d;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);
		CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d != tex2d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (tex2d)
			{
				if (unit->t2d == 0)
					qglEnable(GL_TEXTURE_2D);
			}
			else
			{
				if (unit->t2d)
					qglDisable(GL_TEXTURE_2D);
			}
		}
		unit->t2d = tex2d;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);
		CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d != tex3d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (tex3d)
			{
				if (unit->t3d == 0)
					qglEnable(GL_TEXTURE_3D);
			}
			else
			{
				if (unit->t3d)
					qglDisable(GL_TEXTURE_3D);
			}
		}
		unit->t3d = tex3d;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);
		CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != texcubemap)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (texcubemap)
			{
				if (unit->tcubemap == 0)
					qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
			}
			else
			{
				if (unit->tcubemap)
					qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
			}
		}
		unit->tcubemap = texcubemap;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);
		CHECKGLERROR
	}
}

void R_Mesh_TexBind1D(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendunits)
		return;
	// update 1d texture binding
	if (unit->t1d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (texnum)
		{
			if (unit->t1d == 0)
				qglEnable(GL_TEXTURE_1D);
		}
		else
		{
			if (unit->t1d)
				qglDisable(GL_TEXTURE_1D);
		}
		unit->t1d = texnum;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);
		CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t2d)
			qglDisable(GL_TEXTURE_2D);
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);
		CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t3d)
			qglDisable(GL_TEXTURE_3D);
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);
		CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap)
	{
		GL_ActiveTexture(unitnum);
		if (unit->tcubemap)
			qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);
		CHECKGLERROR
	}
}

void R_Mesh_TexBind(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t1d)
			qglDisable(GL_TEXTURE_1D);
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);
		CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (texnum)
		{
			if (unit->t2d == 0)
				qglEnable(GL_TEXTURE_2D);
		}
		else
		{
			if (unit->t2d)
				qglDisable(GL_TEXTURE_2D);
		}
		unit->t2d = texnum;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);
		CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t3d)
			qglDisable(GL_TEXTURE_3D);
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);
		CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != 0)
	{
		GL_ActiveTexture(unitnum);
		if (unit->tcubemap)
			qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);
		CHECKGLERROR
	}
}

void R_Mesh_TexBind3D(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t1d)
			qglDisable(GL_TEXTURE_1D);
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);
		CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t2d)
			qglDisable(GL_TEXTURE_2D);
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);
		CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (texnum)
		{
			if (unit->t3d == 0)
				qglEnable(GL_TEXTURE_3D);
		}
		else
		{
			if (unit->t3d)
				qglDisable(GL_TEXTURE_3D);
		}
		unit->t3d = texnum;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);
		CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != 0)
	{
		GL_ActiveTexture(unitnum);
		if (unit->tcubemap)
			qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);
		CHECKGLERROR
	}
}

void R_Mesh_TexBindCubeMap(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t1d)
			qglDisable(GL_TEXTURE_1D);
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);
		CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t2d)
			qglDisable(GL_TEXTURE_2D);
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);
		CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unit->t3d)
			qglDisable(GL_TEXTURE_3D);
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);
		CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (texnum)
		{
			if (unit->tcubemap == 0)
				qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
		}
		else
		{
			if (unit->tcubemap)
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		}
		unit->tcubemap = texnum;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);
		CHECKGLERROR
	}
}

void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (matrix->m[3][3])
	{
		// texmatrix specified, check if it is different
		if (!unit->texmatrixenabled || memcmp(&unit->matrix, matrix, sizeof(matrix4x4_t)))
		{
			matrix4x4_t tempmatrix;
			unit->texmatrixenabled = true;
			unit->matrix = *matrix;
			Matrix4x4_Transpose(&tempmatrix, &unit->matrix);
			qglMatrixMode(GL_TEXTURE);
			GL_ActiveTexture(unitnum);
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
			GL_ActiveTexture(unitnum);
			qglLoadIdentity();
			qglMatrixMode(GL_MODELVIEW);
		}
	}
}

void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (gl_combine.integer)
	{
		// GL_ARB_texture_env_combine
		if (!combinergb)
			combinergb = GL_MODULATE;
		if (!combinealpha)
			combinealpha = GL_MODULATE;
		if (!rgbscale)
			rgbscale = 1;
		if (!alphascale)
			alphascale = 1;
		if (unit->combinergb != combinergb)
		{
			unit->combinergb = combinergb;
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, unit->combinergb);CHECKGLERROR
		}
		if (unit->combinealpha != combinealpha)
		{
			unit->combinealpha = combinealpha;
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, unit->combinealpha);CHECKGLERROR
		}
		if (unit->rgbscale != rgbscale)
		{
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (unit->rgbscale = rgbscale));CHECKGLERROR
		}
		if (unit->alphascale != alphascale)
		{
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, (unit->alphascale = alphascale));CHECKGLERROR
		}
	}
	else
	{
		// normal GL texenv
		if (!combinergb)
			combinergb = GL_MODULATE;
		if (unit->combinergb != combinergb)
		{
			unit->combinergb = combinergb;
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combinergb);CHECKGLERROR
		}
	}
}

void R_Mesh_State(const rmeshstate_t *m)
{
	unsigned int i;

	BACKENDACTIVECHECK

	R_Mesh_VertexPointer(m->pointer_vertex);

	if (r_showtrispass)
		return;

	R_Mesh_ColorPointer(m->pointer_color);

	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
	}

	for (i = 0;i < backendimageunits;i++)
		R_Mesh_TexBindAll(i, m->tex1d[i], m->tex[i], m->tex3d[i], m->texcubemap[i]);
	for (i = 0;i < backendarrayunits;i++)
	{
		if (m->pointer_texcoord3f[i])
			R_Mesh_TexCoordPointer(i, 3, m->pointer_texcoord3f[i]);
		else
			R_Mesh_TexCoordPointer(i, 2, m->pointer_texcoord[i]);
	}
	for (i = 0;i < backendunits;i++)
	{
		R_Mesh_TexMatrix(i, &m->texmatrix[i]);
		R_Mesh_TexCombine(i, m->texcombinergb[i], m->texcombinealpha[i], m->texrgbscale[i], m->texalphascale[i]);
	}
}

void R_Mesh_Draw_ShowTris(int firstvertex, int numvertices, int numtriangles, const int *elements)
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

qboolean SCR_ScreenShot(char *filename, qbyte *buffer1, qbyte *buffer2, qbyte *buffer3, int x, int y, int width, int height, qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean jpeg)
{
	int	indices[3] = {0,1,2};
	qboolean ret;

	if (!r_render.integer)
		return false;

	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer1);
	CHECKGLERROR

	if (scr_screenshot_gamma.value != 1)
	{
		int i;
		double igamma = 1.0 / scr_screenshot_gamma.value;
		unsigned char ramp[256];
		for (i = 0;i < 256;i++)
			ramp[i] = (unsigned char) (pow(i * (1.0 / 255.0), igamma) * 255.0);
		for (i = 0;i < width*height*3;i++)
			buffer1[i] = ramp[buffer1[i]];
	}

	Image_CopyMux (buffer2, buffer1, width, height, flipx, flipy, flipdiagonal, 3, 3, indices);

	if (jpeg)
		ret = JPEG_SaveImage_preflipped (filename, width, height, buffer2);
	else
		ret = Image_WriteTGARGB_preflipped (filename, width, height, buffer2, buffer3);

	return ret;
}

//=============================================================================

void R_ClearScreen(void)
{
	if (r_render.integer)
	{
		// clear to black
		if (fogenabled)
			qglClearColor(fogcolor[0],fogcolor[1],fogcolor[2],0);
		else
			qglClearColor(0,0,0,0);
		CHECKGLERROR
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
	return atan (((height/width)/vid_pixelaspect.value)*tan(fov_x/360.0*M_PI))*360.0/M_PI;
}

int r_stereo_side;

void SCR_DrawScreen (void)
{
	for (r_showtrispass = 0;r_showtrispass <= (r_showtris.value > 0);r_showtrispass++)
	{
		R_Mesh_Start();

		R_TimeReport("setup");

		if (r_showtrispass)
		{
			rmeshstate_t m;
			r_showtrispass = 0;
			GL_BlendFunc(GL_ONE, GL_ONE);
			GL_DepthTest(GL_FALSE);
			GL_DepthMask(GL_FALSE);
			memset(&m, 0, sizeof(m));
			R_Mesh_State(&m);
			//qglEnable(GL_LINE_SMOOTH);
			GL_ShowTrisColor(0.2,0.2,0.2,1);
			r_showtrispass = 1;
		}

		if (cls.signon == SIGNONS)
		{
			float size;

			size = scr_viewsize.value * (1.0 / 100.0);
			size = min(size, 1);

			if (r_stereo_sidebyside.integer)
			{
				r_refdef.width = vid.realwidth * size / 2.5;
				r_refdef.height = vid.realheight * size / 2.5 * (1 - bound(0, r_letterbox.value, 100) / 100);
				r_refdef.x = (vid.realwidth - r_refdef.width * 2.5) * 0.5;
				r_refdef.y = (vid.realheight - r_refdef.height)/2;
				if (r_stereo_side)
					r_refdef.x += r_refdef.width * 1.5;
			}
			else
			{
				r_refdef.width = vid.realwidth * size;
				r_refdef.height = vid.realheight * size * (1 - bound(0, r_letterbox.value, 100) / 100);
				r_refdef.x = (vid.realwidth - r_refdef.width)/2;
				r_refdef.y = (vid.realheight - r_refdef.height)/2;
			}

			// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
			r_refdef.fov_x = scr_fov.value * r_refdef.fovscale_x;
			r_refdef.fov_y = CalcFov (scr_fov.value, r_refdef.width, r_refdef.height) * r_refdef.fovscale_y;

			R_RenderView();

			if (scr_zoomwindow.integer)
			{
				float sizex = bound(10, scr_zoomwindow_viewsizex.value, 100) / 100.0;
				float sizey = bound(10, scr_zoomwindow_viewsizey.value, 100) / 100.0;
				r_refdef.width = vid.realwidth * sizex;
				r_refdef.height = vid.realheight * sizey;
				r_refdef.x = (vid.realwidth - r_refdef.width)/2;
				r_refdef.y = 0;
				r_refdef.fov_x = scr_zoomwindow_fov.value * r_refdef.fovscale_x;
				r_refdef.fov_y = CalcFov(scr_zoomwindow_fov.value, r_refdef.width, r_refdef.height) * r_refdef.fovscale_y;

				R_RenderView();
			}
		}

		if (!r_stereo_sidebyside.integer)
		{
			r_refdef.width = vid.realwidth;
			r_refdef.height = vid.realheight;
			r_refdef.x = 0;
			r_refdef.y = 0;
		}

		// draw 2D stuff
		R_DrawQueue();

		R_Mesh_Finish();

		R_TimeReport("meshfinish");
	}
	r_showtrispass = 0;
	//qglDisable(GL_LINE_SMOOTH);
}

void SCR_UpdateLoadingScreen (void)
{
	float x, y;
	cachepic_t *pic;
	rmeshstate_t m;
	// don't do anything if not initialized yet
	if (vid_hidden)
		return;
	r_showtrispass = 0;
	VID_GetWindowSize(&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);
	VID_UpdateGamma(false);
	qglViewport(0, 0, vid.realwidth, vid.realheight);
	//qglDisable(GL_SCISSOR_TEST);
	//qglDepthMask(1);
	qglColorMask(1,1,1,1);
	//qglClearColor(0,0,0,0);
	//qglClear(GL_COLOR_BUFFER_BIT);
	//qglCullFace(GL_FRONT);
	//qglDisable(GL_CULL_FACE);
	//R_ClearScreen();
	R_Textures_Frame();
	GL_SetupView_Mode_Ortho(0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100);
	R_Mesh_Start();
	R_Mesh_Matrix(&r_identitymatrix);
	// draw the loading plaque
	pic = Draw_CachePic("gfx/loading", false);
	x = (vid_conwidth.integer - pic->width)/2;
	y = (vid_conheight.integer - pic->height)/2;
	GL_Color(1,1,1,1);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthTest(false);
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = varray_vertex3f;
	m.pointer_texcoord[0] = varray_texcoord2f[0];
	m.tex[0] = R_GetTexture(pic->tex);
	R_Mesh_State(&m);
	varray_vertex3f[0] = varray_vertex3f[9] = x;
	varray_vertex3f[1] = varray_vertex3f[4] = y;
	varray_vertex3f[3] = varray_vertex3f[6] = x + pic->width;
	varray_vertex3f[7] = varray_vertex3f[10] = y + pic->height;
	varray_texcoord2f[0][0] = 0;varray_texcoord2f[0][1] = 0;
	varray_texcoord2f[0][2] = 1;varray_texcoord2f[0][3] = 0;
	varray_texcoord2f[0][4] = 1;varray_texcoord2f[0][5] = 1;
	varray_texcoord2f[0][6] = 0;varray_texcoord2f[0][7] = 1;
	GL_LockArrays(0, 4);
	R_Mesh_Draw(0, 4, 2, polygonelements);
	GL_LockArrays(0, 0);
	R_Mesh_Finish();
	// refresh
	VID_Finish();
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
	if (vid_hidden)
		return;

	if (r_textureunits.integer > gl_textureunits)
		Cvar_SetValueQuick(&r_textureunits, gl_textureunits);
	if (r_textureunits.integer < 1)
		Cvar_SetValueQuick(&r_textureunits, 1);

	if (gl_combine.integer && (!gl_combine_extension || r_textureunits.integer < 2))
		Cvar_SetValueQuick(&gl_combine, 0);

	CHECKGLERROR
	qglViewport(0, 0, vid.realwidth, vid.realheight);
	qglDisable(GL_SCISSOR_TEST);
	qglDepthMask(1);
	qglColorMask(1,1,1,1);
	qglClearColor(0,0,0,0);
	qglClear(GL_COLOR_BUFFER_BIT);
	CHECKGLERROR

	R_TimeReport("clear");

	if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer || r_stereo_sidebyside.integer)
	{
		matrix4x4_t originalmatrix = r_refdef.viewentitymatrix;
		r_refdef.viewentitymatrix.m[0][3] = originalmatrix.m[0][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[0][1];
		r_refdef.viewentitymatrix.m[1][3] = originalmatrix.m[1][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[1][1];
		r_refdef.viewentitymatrix.m[2][3] = originalmatrix.m[2][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[2][1];

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 0;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.colormask[0] = 1;
			r_refdef.colormask[1] = 0;
			r_refdef.colormask[2] = 0;
		}

		SCR_DrawScreen();

		r_refdef.viewentitymatrix.m[0][3] = originalmatrix.m[0][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[0][1];
		r_refdef.viewentitymatrix.m[1][3] = originalmatrix.m[1][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[1][1];
		r_refdef.viewentitymatrix.m[2][3] = originalmatrix.m[2][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[2][1];

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 1;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.colormask[0] = 0;
			r_refdef.colormask[1] = r_stereo_redcyan.integer || r_stereo_redgreen.integer;
			r_refdef.colormask[2] = r_stereo_redcyan.integer || r_stereo_redblue.integer;
		}

		SCR_DrawScreen();

		r_refdef.viewentitymatrix = originalmatrix;
	}
	else
	{
		r_showtrispass = false;
		SCR_DrawScreen();

		if (r_showtris.value > 0)
		{
			rmeshstate_t m;
			GL_BlendFunc(GL_ONE, GL_ONE);
			GL_DepthTest(GL_FALSE);
			GL_DepthMask(GL_FALSE);
			memset(&m, 0, sizeof(m));
			R_Mesh_State(&m);
			r_showtrispass = true;
			GL_ShowTrisColor(0.2,0.2,0.2,1);
			SCR_DrawScreen();
			r_showtrispass = false;
		}
	}

	VID_Finish();
	R_TimeReport("finish");
}


//===========================================================================
// dynamic vertex array buffer subsystem
//===========================================================================

// FIXME: someday this should be dynamically allocated and resized?
float varray_vertex3f[65536*3];
float varray_svector3f[65536*3];
float varray_tvector3f[65536*3];
float varray_normal3f[65536*3];
float varray_color4f[65536*4];
float varray_texcoord2f[4][65536*2];
float varray_texcoord3f[4][65536*3];
int earray_element3i[65536];
float varray_vertex3f2[65536*3];

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

