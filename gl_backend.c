
#include "quakedef.h"
#include "cl_collision.h"

cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1", "use glDrawRangeElements function if available instead of glDrawElements (for performance comparisons or bug testing)"};
cvar_t gl_mesh_testarrayelement = {0, "gl_mesh_testarrayelement", "0", "use glBegin(GL_TRIANGLES);glArrayElement();glEnd(); primitives instead of glDrawElements (useful to test for driver bugs with glDrawElements)"};
cvar_t gl_mesh_testmanualfeeding = {0, "gl_mesh_testmanualfeeding", "0", "use glBegin(GL_TRIANGLES);glTexCoord2f();glVertex3f();glEnd(); primitives instead of glDrawElements (useful to test for driver bugs with glDrawElements)"};
cvar_t gl_mesh_prefer_short_elements = {0, "gl_mesh_prefer_short_elements", "1", "use GL_UNSIGNED_SHORT element arrays instead of GL_UNSIGNED_INT"};
cvar_t gl_workaround_mac_texmatrix = {0, "gl_workaround_mac_texmatrix", "0", "if set to 1 this uses glLoadMatrixd followed by glLoadIdentity to clear a texture matrix (normally glLoadIdentity is sufficient by itself), if set to 2 it uses glLoadMatrixd without glLoadIdentity"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0", "enables OpenGL error checking and other tests"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0", "prints all OpenGL error checks, useful to identify location of driver crashes"};

cvar_t r_render = {0, "r_render", "1", "enables rendering calls (you want this on!)"};
cvar_t r_waterwarp = {CVAR_SAVE, "r_waterwarp", "1", "warp view while underwater"};
cvar_t gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1", "tints view while underwater, hurt, etc"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1", "enables OpenGL dithering (16bit looks bad with this off)"};
cvar_t gl_lockarrays = {0, "gl_lockarrays", "0", "enables use of glLockArraysEXT, may cause glitches with some broken drivers, and may be slower than normal"};
cvar_t gl_lockarrays_minimumvertices = {0, "gl_lockarrays_minimumvertices", "1", "minimum number of vertices required for use of glLockArraysEXT, setting this too low may reduce performance"};
cvar_t gl_vbo = {CVAR_SAVE, "gl_vbo", "1", "make use of GL_ARB_vertex_buffer_object extension to store static geometry in video memory for faster rendering"};

cvar_t v_flipped = {0, "v_flipped", "0", "mirror the screen (poor man's left handed mode)"};
qboolean v_flipped_state = false;

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

#define BACKENDACTIVECHECK if (!backendactive) Sys_Error("GL backend function called when backend is not active");

void SCR_ScreenShot_f (void);

static matrix4x4_t backend_viewmatrix;
static matrix4x4_t backend_modelmatrix;
static matrix4x4_t backend_modelviewmatrix;
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

unsigned short polygonelements[(POLYGONELEMENTS_MAXPOINTS-2)*3];
unsigned short quadelements[QUADELEMENTS_MAXQUADS*6];

void GL_Backend_AllocArrays(void)
{
}

void GL_Backend_FreeArrays(void)
{
}

void GL_VBOStats_f(void)
{
	GL_Mesh_ListVBOs(true);
}

typedef struct gl_bufferobjectinfo_s
{
	int target;
	int object;
	size_t size;
	char name[MAX_QPATH];
}
gl_bufferobjectinfo_t;

memexpandablearray_t gl_bufferobjectinfoarray;

static void gl_backend_start(void)
{
	CHECKGLERROR

	if (qglDrawRangeElements != NULL)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_maxdrawrangeelementsvertices);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_maxdrawrangeelementsindices);
		CHECKGLERROR
		Con_DPrintf("GL_MAX_ELEMENTS_VERTICES = %i\nGL_MAX_ELEMENTS_INDICES = %i\n", gl_maxdrawrangeelementsvertices, gl_maxdrawrangeelementsindices);
	}

	backendunits = bound(1, gl_textureunits, MAX_TEXTUREUNITS);
	backendimageunits = backendunits;
	backendarrayunits = backendunits;
	if (gl_support_fragment_shader)
	{
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, (int *)&backendimageunits);
		CHECKGLERROR
		qglGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, (int *)&backendarrayunits);
		CHECKGLERROR
		Con_DPrintf("GLSL shader support detected: texture units = %i texenv, %i image, %i array\n", backendunits, backendimageunits, backendarrayunits);
		backendimageunits = bound(1, backendimageunits, MAX_TEXTUREUNITS);
		backendarrayunits = bound(1, backendarrayunits, MAX_TEXTUREUNITS);
	}
	else
		Con_DPrintf("GL_MAX_TEXTUREUNITS = %i\n", backendunits);

	GL_Backend_AllocArrays();

	Mem_ExpandableArray_NewArray(&gl_bufferobjectinfoarray, r_main_mempool, sizeof(gl_bufferobjectinfo_t), 128);

	Con_DPrintf("OpenGL backend started.\n");

	CHECKGLERROR

	backendactive = true;
}

static void gl_backend_shutdown(void)
{
	backendunits = 0;
	backendimageunits = 0;
	backendarrayunits = 0;
	backendactive = false;

	Con_DPrint("OpenGL Backend shutting down\n");

	Mem_ExpandableArray_FreeArray(&gl_bufferobjectinfoarray);

	GL_Backend_FreeArrays();
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
	// elements for rendering a series of quads as triangles
	for (i = 0;i < QUADELEMENTS_MAXQUADS;i++)
	{
		quadelements[i * 6 + 0] = i * 4;
		quadelements[i * 6 + 1] = i * 4 + 1;
		quadelements[i * 6 + 2] = i * 4 + 2;
		quadelements[i * 6 + 3] = i * 4;
		quadelements[i * 6 + 4] = i * 4 + 2;
		quadelements[i * 6 + 5] = i * 4 + 3;
	}

	Cvar_RegisterVariable(&r_render);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&v_flipped);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_lockarrays);
	Cvar_RegisterVariable(&gl_lockarrays_minimumvertices);
	Cvar_RegisterVariable(&gl_vbo);
	Cvar_RegisterVariable(&gl_paranoid);
	Cvar_RegisterVariable(&gl_printcheckerror);
	Cvar_RegisterVariable(&gl_workaround_mac_texmatrix);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
	Cvar_RegisterVariable(&gl_mesh_testarrayelement);
	Cvar_RegisterVariable(&gl_mesh_testmanualfeeding);
	Cvar_RegisterVariable(&gl_mesh_prefer_short_elements);

	Cmd_AddCommand("gl_vbostats", GL_VBOStats_f, "prints a list of all buffer objects (vertex data and triangle elements) and total video memory used by them");

	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
}

void GL_SetMirrorState(qboolean state);

void GL_SetupView_Orientation_Identity (void)
{
	backend_viewmatrix = identitymatrix;
	GL_SetMirrorState(false);
	memset(&backend_modelmatrix, 0, sizeof(backend_modelmatrix));
}

void GL_SetupView_Orientation_FromEntity(const matrix4x4_t *matrix)
{
	matrix4x4_t tempmatrix, basematrix;
	Matrix4x4_Invert_Full(&tempmatrix, matrix);
	Matrix4x4_CreateRotate(&basematrix, -90, 1, 0, 0);
	Matrix4x4_ConcatRotate(&basematrix, 90, 0, 0, 1);
	Matrix4x4_Concat(&backend_viewmatrix, &basematrix, &tempmatrix);

	GL_SetMirrorState(v_flipped.integer);
	if(v_flipped_state)
	{
		Matrix4x4_Transpose(&basematrix, &backend_viewmatrix);
		Matrix4x4_ConcatScale3(&basematrix, -1, 1, 1);
		Matrix4x4_Transpose(&backend_viewmatrix, &basematrix);
	}

	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[2], 1, 0, 0);
	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[0], 0, 1, 0);
	//Matrix4x4_ConcatRotate(&backend_viewmatrix, -angles[1], 0, 0, 1);
	//Matrix4x4_ConcatTranslate(&backend_viewmatrix, -origin[0], -origin[1], -origin[2]);

	// force an update of the model matrix by copying it off, resetting it, and then calling the R_Mesh_Matrix function with it
	tempmatrix = backend_modelmatrix;
	memset(&backend_modelmatrix, 0, sizeof(backend_modelmatrix));
	R_Mesh_Matrix(&tempmatrix);
}

static void GL_BuildFrustum(double m[16], double left, double right, double bottom, double top, double nearVal, double farVal)
{
	m[0]  = 2 * nearVal / (right - left);
	m[1]  = 0;
	m[2]  = 0;
	m[3]  = 0;

	m[4]  = 0;
	m[5]  = 2 * nearVal / (top - bottom);
	m[6]  = 0;
	m[7]  = 0;

	m[8]  = (right + left) / (right - left);
	m[9]  = (top + bottom) / (top - bottom);
	m[10] = - (farVal + nearVal) / (farVal - nearVal);
	m[11] = -1;

	m[12] = 0;
	m[13] = 0;
	m[14] = - 2 * farVal * nearVal / (farVal - nearVal);
	m[15] = 0;
}

void GL_SetupView_Mode_Perspective (double frustumx, double frustumy, double zNear, double zFar)
{
	double m[16];

	// set up viewpoint
	CHECKGLERROR
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	// set view pyramid
#if 1
	// avoid glGetDoublev whenever possible, it may stall the render pipeline
	// in the tested cases (nvidia) no measurable fps difference, but it sure
	// makes a difference over a network line with GLX
	GL_BuildFrustum(m, -frustumx * zNear, frustumx * zNear, -frustumy * zNear, frustumy * zNear, zNear, zFar);
	qglLoadMatrixd(m);CHECKGLERROR
#else
	qglLoadIdentity();CHECKGLERROR
	qglFrustum(-frustumx * zNear, frustumx * zNear, -frustumy * zNear, frustumy * zNear, zNear, zFar);CHECKGLERROR
	qglGetDoublev(GL_PROJECTION_MATRIX, m);CHECKGLERROR
#endif
	Matrix4x4_FromArrayDoubleGL(&backend_projectmatrix, m);
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
	CHECKGLERROR
}

void GL_SetupView_Mode_PerspectiveInfiniteFarClip (double frustumx, double frustumy, double zNear)
{
	double nudge, m[16];

	// set up viewpoint
	CHECKGLERROR
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadIdentity();CHECKGLERROR
	// set view pyramid
	nudge = 1.0 - 1.0 / (1<<23);
	m[ 0] = 1.0 / frustumx;
	m[ 1] = 0;
	m[ 2] = 0;
	m[ 3] = 0;
	m[ 4] = 0;
	m[ 5] = 1.0 / frustumy;
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
	qglLoadMatrixd(m);CHECKGLERROR
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
	CHECKGLERROR
	Matrix4x4_FromArrayDoubleGL(&backend_projectmatrix, m);
}

static void GL_BuildOrtho(double m[16], double left, double right, double bottom, double top, double zNear, double zFar)
{
	m[0]  = 2/(right - left);
	m[1]  = 0;
	m[2]  = 0;
	m[3]  = 0;

	m[4]  = 0;
	m[5]  = 2/(top - bottom);
	m[6]  = 0;
	m[7]  = 0;

	m[8]  = 0;
	m[9]  = 0;
	m[10] = -2/(zFar - zNear);
	m[11] = 0;

	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom)/(top - bottom);
	m[14] = - (zFar + zNear)/(zFar - zNear);
	m[15] = 1;
}

void GL_SetupView_Mode_Ortho (double x1, double y1, double x2, double y2, double zNear, double zFar)
{
	double m[16];

	// set up viewpoint
	CHECKGLERROR
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
#if 1
	// avoid glGetDoublev whenever possible, it may stall the render pipeline
	// in the tested cases (nvidia) no measurable fps difference, but it sure
	// makes a difference over a network line with GLX
	GL_BuildOrtho(m, x1, x2, y2, y1, zNear, zFar);
	qglLoadMatrixd(m);CHECKGLERROR
#else
	qglLoadIdentity();CHECKGLERROR
	qglOrtho(x1, x2, y2, y1, zNear, zFar);CHECKGLERROR
	qglGetDoublev(GL_PROJECTION_MATRIX, m);CHECKGLERROR
#endif
	Matrix4x4_FromArrayDoubleGL(&backend_projectmatrix, m);
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	GL_SetupView_Orientation_Identity();
	CHECKGLERROR
}

void GL_SetupView_ApplyCustomNearClipPlane(double normalx, double normaly, double normalz, double dist)
{
	double matrix[16];
	double q[4];
	double d;
	float clipPlane[4], v3[3], v4[3];
	float normal[3];

	// This is Olique Depth Projection from http://www.terathon.com/code/oblique.php
	// modified to fit in this codebase.

	VectorSet(normal, normalx, normaly, normalz);
	Matrix4x4_Transform3x3(&backend_viewmatrix, normal, clipPlane);
	VectorScale(normal, dist, v3);
	Matrix4x4_Transform(&backend_viewmatrix, v3, v4);
	// FIXME: LordHavoc: I think this can be done more efficiently somehow but I can't remember the technique
	clipPlane[3] = -DotProduct(v4, clipPlane);

#if 0
{
	// testing code for comparing results
	float clipPlane2[4];
	VectorCopy4(clipPlane, clipPlane2);
	R_Mesh_Matrix(&identitymatrix);
	VectorSet(q, normal[0], normal[1], normal[2], -dist);
	qglClipPlane(GL_CLIP_PLANE0, q);
	qglGetClipPlane(GL_CLIP_PLANE0, q);
	VectorCopy4(q, clipPlane);
}
#endif

	// Calculate the clip-space corner point opposite the clipping plane
	// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
	// transform it into camera space by multiplying it
	// by the inverse of the projection matrix
	Matrix4x4_ToArrayDoubleGL(&backend_projectmatrix, matrix);

	q[0] = ((clipPlane[0] < 0.0f ? -1.0f : clipPlane[0] > 0.0f ? 1.0f : 0.0f) + matrix[8]) / matrix[0];
	q[1] = ((clipPlane[1] < 0.0f ? -1.0f : clipPlane[1] > 0.0f ? 1.0f : 0.0f) + matrix[9]) / matrix[5];
	q[2] = -1.0f;
	q[3] = (1.0f + matrix[10]) / matrix[14];

	// Calculate the scaled plane vector
	d = 2.0f / DotProduct4(clipPlane, q);

	// Replace the third row of the projection matrix
	matrix[2] = clipPlane[0] * d;
	matrix[6] = clipPlane[1] * d;
	matrix[10] = clipPlane[2] * d + 1.0f;
	matrix[14] = clipPlane[3] * d;

	// Load it back into OpenGL
	qglMatrixMode(GL_PROJECTION);CHECKGLERROR
	qglLoadMatrixd(matrix);CHECKGLERROR
	qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
	CHECKGLERROR
	Matrix4x4_FromArrayDoubleGL(&backend_projectmatrix, matrix);
}

typedef struct gltextureunit_s
{
	const void *pointer_texcoord;
	size_t pointer_texcoord_offset;
	int pointer_texcoord_buffer;
	int t1d, t2d, t3d, tcubemap;
	int arrayenabled;
	unsigned int arraycomponents;
	int rgbscale, alphascale;
	int combinergb, combinealpha;
	// FIXME: add more combine stuff
	// texmatrixenabled exists only to avoid unnecessary texmatrix compares
	int texmatrixenabled;
	matrix4x4_t matrix;
}
gltextureunit_t;

static struct gl_state_s
{
	int cullface;
	int cullfaceenable;
	int blendfunc1;
	int blendfunc2;
	int blend;
	GLboolean depthmask;
	int colormask; // stored as bottom 4 bits: r g b a (3 2 1 0 order)
	int depthtest;
	float depthrange[2];
	float polygonoffset[2];
	int alphatest;
	int scissortest;
	unsigned int unit;
	gltextureunit_t units[MAX_TEXTUREUNITS];
	float color4f[4];
	int lockrange_first;
	int lockrange_count;
	int vertexbufferobject;
	int elementbufferobject;
	qboolean pointer_color_enabled;
	const void *pointer_vertex;
	const void *pointer_color;
	size_t pointer_vertex_offset;
	size_t pointer_color_offset;
	int pointer_vertex_buffer;
	int pointer_color_buffer;
}
gl_state;

static void GL_BindVBO(int bufferobject)
{
	if (gl_state.vertexbufferobject != bufferobject)
	{
		gl_state.vertexbufferobject = bufferobject;
		CHECKGLERROR
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, bufferobject);
		CHECKGLERROR
	}
}

static void GL_BindEBO(int bufferobject)
{
	if (gl_state.elementbufferobject != bufferobject)
	{
		gl_state.elementbufferobject = bufferobject;
		CHECKGLERROR
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufferobject);
		CHECKGLERROR
	}
}

void GL_SetupTextureState(void)
{
	unsigned int i;
	gltextureunit_t *unit;
	CHECKGLERROR
	gl_state.unit = MAX_TEXTUREUNITS;
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
		unit->pointer_texcoord_buffer = 0;
		unit->pointer_texcoord_offset = 0;
		unit->rgbscale = 1;
		unit->alphascale = 1;
		unit->combinergb = GL_MODULATE;
		unit->combinealpha = GL_MODULATE;
		unit->texmatrixenabled = false;
		unit->matrix = identitymatrix;
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
		GL_ActiveTexture(i);
		GL_BindVBO(0);
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
		qglMatrixMode(GL_TEXTURE);CHECKGLERROR
		qglLoadIdentity();CHECKGLERROR
		qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
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
		CHECKGLERROR
	}
	CHECKGLERROR
}

void GL_Backend_ResetState(void)
{
	memset(&gl_state, 0, sizeof(gl_state));
	gl_state.depthtest = true;
	gl_state.alphatest = false;
	gl_state.blendfunc1 = GL_ONE;
	gl_state.blendfunc2 = GL_ZERO;
	gl_state.blend = false;
	gl_state.depthmask = GL_TRUE;
	gl_state.colormask = 15;
	gl_state.color4f[0] = gl_state.color4f[1] = gl_state.color4f[2] = gl_state.color4f[3] = 1;
	gl_state.lockrange_first = 0;
	gl_state.lockrange_count = 0;
	gl_state.cullface = v_flipped_state ? GL_BACK : GL_FRONT; // quake is backwards, this culls back faces
	gl_state.cullfaceenable = true;
	gl_state.polygonoffset[0] = 0;
	gl_state.polygonoffset[1] = 0;

	CHECKGLERROR

	qglColorMask(1, 1, 1, 1);
	qglAlphaFunc(GL_GEQUAL, 0.5);CHECKGLERROR
	qglDisable(GL_ALPHA_TEST);CHECKGLERROR
	qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
	qglDisable(GL_BLEND);CHECKGLERROR
	qglCullFace(gl_state.cullface);CHECKGLERROR
	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglDepthFunc(GL_LEQUAL);CHECKGLERROR
	qglEnable(GL_DEPTH_TEST);CHECKGLERROR
	qglDepthMask(gl_state.depthmask);CHECKGLERROR
	qglPolygonOffset(gl_state.polygonoffset[0], gl_state.polygonoffset[1]);

	if (gl_support_arb_vertex_buffer_object)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

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
			CHECKGLERROR
			qglActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
			qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
			CHECKGLERROR
		}
	}
}

void GL_BlendFunc(int blendfunc1, int blendfunc2)
{
	if (gl_state.blendfunc1 != blendfunc1 || gl_state.blendfunc2 != blendfunc2)
	{
		CHECKGLERROR
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
		CHECKGLERROR
		qglDepthMask(gl_state.depthmask = state);CHECKGLERROR
	}
}

void GL_DepthTest(int state)
{
	if (gl_state.depthtest != state)
	{
		gl_state.depthtest = state;
		CHECKGLERROR
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

void GL_DepthRange(float nearfrac, float farfrac)
{
	if (gl_state.depthrange[0] != nearfrac || gl_state.depthrange[1] != farfrac)
	{
		gl_state.depthrange[0] = nearfrac;
		gl_state.depthrange[1] = farfrac;
		qglDepthRange(nearfrac, farfrac);
	}
}

void GL_PolygonOffset(float planeoffset, float depthoffset)
{
	if (gl_state.polygonoffset[0] != planeoffset || gl_state.polygonoffset[1] != depthoffset)
	{
		gl_state.polygonoffset[0] = planeoffset;
		gl_state.polygonoffset[1] = depthoffset;
		qglPolygonOffset(planeoffset, depthoffset);
	}
}

void GL_SetMirrorState(qboolean state)
{
	if(!state != !v_flipped_state)
	{
		// change cull face mode!
		if(gl_state.cullface == GL_BACK)
			qglCullFace((gl_state.cullface = GL_FRONT));
		else if(gl_state.cullface == GL_FRONT)
			qglCullFace((gl_state.cullface = GL_BACK));
	}
	v_flipped_state = state;
}

void GL_CullFace(int state)
{
	CHECKGLERROR

	if(v_flipped_state)
	{
		if(state == GL_FRONT)
			state = GL_BACK;
		else if(state == GL_BACK)
			state = GL_FRONT;
	}

	if (state != GL_NONE)
	{
		if (!gl_state.cullfaceenable)
		{
			gl_state.cullfaceenable = true;
			qglEnable(GL_CULL_FACE);CHECKGLERROR
		}
		if (gl_state.cullface != state)
		{
			gl_state.cullface = state;
			qglCullFace(gl_state.cullface);CHECKGLERROR
		}
	}
	else
	{
		if (gl_state.cullfaceenable)
		{
			gl_state.cullfaceenable = false;
			qglDisable(GL_CULL_FACE);CHECKGLERROR
		}
	}
}

void GL_AlphaTest(int state)
{
	if (gl_state.alphatest != state)
	{
		gl_state.alphatest = state;
		CHECKGLERROR
		if (gl_state.alphatest)
		{
			qglEnable(GL_ALPHA_TEST);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_ALPHA_TEST);CHECKGLERROR
		}
	}
}

void GL_ColorMask(int r, int g, int b, int a)
{
	int state = r*8 + g*4 + b*2 + a*1;
	if (gl_state.colormask != state)
	{
		gl_state.colormask = state;
		CHECKGLERROR
		qglColorMask((GLboolean)r, (GLboolean)g, (GLboolean)b, (GLboolean)a);CHECKGLERROR
	}
}

void GL_Color(float cr, float cg, float cb, float ca)
{
	if (gl_state.pointer_color_enabled || gl_state.color4f[0] != cr || gl_state.color4f[1] != cg || gl_state.color4f[2] != cb || gl_state.color4f[3] != ca)
	{
		gl_state.color4f[0] = cr;
		gl_state.color4f[1] = cg;
		gl_state.color4f[2] = cb;
		gl_state.color4f[3] = ca;
		CHECKGLERROR
		qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);
		CHECKGLERROR
	}
}

void GL_LockArrays(int first, int count)
{
	if (count < gl_lockarrays_minimumvertices.integer)
	{
		first = 0;
		count = 0;
	}
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
	qglScissor(x, vid.height - (y + height),width,height);
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
	CHECKGLERROR
	qglClear(mask);CHECKGLERROR
}

void GL_TransformToScreen(const vec4_t in, vec4_t out)
{
	vec4_t temp;
	float iw;
	Matrix4x4_Transform4 (&backend_viewmatrix, in, temp);
	Matrix4x4_Transform4 (&backend_projectmatrix, temp, out);
	iw = 1.0f / out[3];
	out[0] = r_refdef.view.x + (out[0] * iw + 1.0f) * r_refdef.view.width * 0.5f;
	out[1] = r_refdef.view.y + r_refdef.view.height - (out[1] * iw + 1.0f) * r_refdef.view.height * 0.5f;
	out[2] = r_refdef.view.z + (out[2] * iw + 1.0f) * r_refdef.view.depth * 0.5f;
}

// called at beginning of frame
void R_Mesh_Start(void)
{
	BACKENDACTIVECHECK
	CHECKGLERROR
	if (gl_printcheckerror.integer && !gl_paranoid.integer)
	{
		Con_Printf("WARNING: gl_printcheckerror is on but gl_paranoid is off, turning it on...\n");
		Cvar_SetValueQuick(&gl_paranoid, 1);
	}
	GL_Backend_ResetState();
}

qboolean GL_Backend_CompileShader(int programobject, GLenum shadertypeenum, const char *shadertype, int numstrings, const char **strings)
{
	int shaderobject;
	int shadercompiled;
	char compilelog[MAX_INPUTLINE];
	shaderobject = qglCreateShaderObjectARB(shadertypeenum);CHECKGLERROR
	if (!shaderobject)
		return false;
	qglShaderSourceARB(shaderobject, numstrings, strings, NULL);CHECKGLERROR
	qglCompileShaderARB(shaderobject);CHECKGLERROR
	qglGetObjectParameterivARB(shaderobject, GL_OBJECT_COMPILE_STATUS_ARB, &shadercompiled);CHECKGLERROR
	qglGetInfoLogARB(shaderobject, sizeof(compilelog), NULL, compilelog);CHECKGLERROR
	if (compilelog[0] && developer.integer > 0)
	{
		int i, j, pretextlines = 0;
		for (i = 0;i < numstrings - 1;i++)
			for (j = 0;strings[i][j];j++)
				if (strings[i][j] == '\n')
					pretextlines++;
		Con_DPrintf("%s shader compile log:\n%s\n(line offset for any above warnings/errors: %i)\n", shadertype, compilelog, pretextlines);
	}
	if (!shadercompiled)
	{
		qglDeleteObjectARB(shaderobject);CHECKGLERROR
		return false;
	}
	qglAttachObjectARB(programobject, shaderobject);CHECKGLERROR
	qglDeleteObjectARB(shaderobject);CHECKGLERROR
	return true;
}

unsigned int GL_Backend_CompileProgram(int vertexstrings_count, const char **vertexstrings_list, int geometrystrings_count, const char **geometrystrings_list, int fragmentstrings_count, const char **fragmentstrings_list)
{
	GLint programlinked;
	GLuint programobject = 0;
	char linklog[MAX_INPUTLINE];
	CHECKGLERROR

	programobject = qglCreateProgramObjectARB();CHECKGLERROR
	if (!programobject)
		return 0;

	if (vertexstrings_count && !GL_Backend_CompileShader(programobject, GL_VERTEX_SHADER_ARB, "vertex", vertexstrings_count, vertexstrings_list))
		goto cleanup;

#ifdef GL_GEOMETRY_SHADER_ARB
	if (geometrystrings_count && !GL_Backend_CompileShader(programobject, GL_GEOMETRY_SHADER_ARB, "geometry", geometrystrings_count, geometrystrings_list))
		goto cleanup;
#endif

	if (fragmentstrings_count && !GL_Backend_CompileShader(programobject, GL_FRAGMENT_SHADER_ARB, "fragment", fragmentstrings_count, fragmentstrings_list))
		goto cleanup;

	qglLinkProgramARB(programobject);CHECKGLERROR
	qglGetObjectParameterivARB(programobject, GL_OBJECT_LINK_STATUS_ARB, &programlinked);CHECKGLERROR
	qglGetInfoLogARB(programobject, sizeof(linklog), NULL, linklog);CHECKGLERROR
	if (linklog[0])
	{
		Con_DPrintf("program link log:\n%s\n", linklog);
		// software vertex shader is ok but software fragment shader is WAY
		// too slow, fail program if so.
		// NOTE: this string might be ATI specific, but that's ok because the
		// ATI R300 chip (Radeon 9500-9800/X300) is the most likely to use a
		// software fragment shader due to low instruction and dependent
		// texture limits.
		if (strstr(linklog, "fragment shader will run in software"))
			programlinked = false;
	}
	if (!programlinked)
		goto cleanup;
	return programobject;
cleanup:
	qglDeleteObjectARB(programobject);CHECKGLERROR
	return 0;
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
void R_Mesh_Draw(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const unsigned short *element3s, int bufferobject3i, int bufferobject3s)
{
	unsigned int numelements = numtriangles * 3;
	if (numvertices < 3 || numtriangles < 1)
	{
		Con_Printf("R_Mesh_Draw(%d, %d, %d, %d, %8p, %8p, %i, %i);\n", firstvertex, numvertices, firsttriangle, numtriangles, element3i, element3s, bufferobject3i, bufferobject3s);
		return;
	}
	if (!gl_mesh_prefer_short_elements.integer)
	{
		if (element3i)
			element3s = NULL;
		if (bufferobject3i)
			bufferobject3s = 0;
	}
	if (element3i)
		element3i += firsttriangle * 3;
	if (element3s)
		element3s += firsttriangle * 3;
	switch (gl_vbo.integer)
	{
	default:
	case 0:
	case 2:
		bufferobject3i = bufferobject3s = 0;
		break;
	case 1:
		break;
	case 3:
		if (firsttriangle)
			bufferobject3i = bufferobject3s = 0;
		break;
	}
	CHECKGLERROR
	r_refdef.stats.meshes++;
	r_refdef.stats.meshes_elements += numelements;
	if (gl_paranoid.integer)
	{
		unsigned int i, j, size;
		const int *p;
		// note: there's no validation done here on buffer objects because it
		// is somewhat difficult to get at the data, and gl_paranoid can be
		// used without buffer objects if the need arises
		// (the data could be gotten using glMapBuffer but it would be very
		//  slow due to uncachable video memory reads)
		if (!qglIsEnabled(GL_VERTEX_ARRAY))
			Con_Print("R_Mesh_Draw: vertex array not enabled\n");
		CHECKGLERROR
		if (gl_state.pointer_vertex)
			for (j = 0, size = numvertices * 3, p = (int *)((float *)gl_state.pointer_vertex + firstvertex * 3);j < size;j++, p++)
				paranoidblah += *p;
		if (gl_state.pointer_color_enabled)
		{
			if (!qglIsEnabled(GL_COLOR_ARRAY))
				Con_Print("R_Mesh_Draw: color array set but not enabled\n");
			CHECKGLERROR
			if (gl_state.pointer_color && gl_state.pointer_color_enabled)
				for (j = 0, size = numvertices * 4, p = (int *)((float *)gl_state.pointer_color + firstvertex * 4);j < size;j++, p++)
					paranoidblah += *p;
		}
		for (i = 0;i < backendarrayunits;i++)
		{
			if (gl_state.units[i].arrayenabled)
			{
				GL_ActiveTexture(i);
				if (!qglIsEnabled(GL_TEXTURE_COORD_ARRAY))
					Con_Print("R_Mesh_Draw: texcoord array set but not enabled\n");
				CHECKGLERROR
				if (gl_state.units[i].pointer_texcoord && gl_state.units[i].arrayenabled)
					for (j = 0, size = numvertices * gl_state.units[i].arraycomponents, p = (int *)((float *)gl_state.units[i].pointer_texcoord + firstvertex * gl_state.units[i].arraycomponents);j < size;j++, p++)
						paranoidblah += *p;
			}
		}
		if (element3i)
		{
			for (i = 0;i < (unsigned int) numtriangles * 3;i++)
			{
				if (element3i[i] < firstvertex || element3i[i] >= firstvertex + numvertices)
				{
					Con_Printf("R_Mesh_Draw: invalid vertex index %i (outside range %i - %i) in element3i array\n", element3i[i], firstvertex, firstvertex + numvertices);
					return;
				}
			}
		}
		if (element3s)
		{
			for (i = 0;i < (unsigned int) numtriangles * 3;i++)
			{
				if (element3s[i] < firstvertex || element3s[i] >= firstvertex + numvertices)
				{
					Con_Printf("R_Mesh_Draw: invalid vertex index %i (outside range %i - %i) in element3s array\n", element3s[i], firstvertex, firstvertex + numvertices);
					return;
				}
			}
		}
		CHECKGLERROR
	}
	if (r_render.integer)
	{
		CHECKGLERROR
		if (gl_mesh_testmanualfeeding.integer)
		{
			unsigned int i, j, element;
			const GLfloat *p;
			qglBegin(GL_TRIANGLES);
			for (i = 0;i < (unsigned int) numtriangles * 3;i++)
			{
				element = element3i ? element3i[i] : element3s[i];
				for (j = 0;j < backendarrayunits;j++)
				{
					if (gl_state.units[j].pointer_texcoord && gl_state.units[j].arrayenabled)
					{
						if (backendarrayunits > 1)
						{
							if (gl_state.units[j].arraycomponents == 4)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 4;
								qglMultiTexCoord4f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.units[j].arraycomponents == 3)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 3;
								qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2]);
							}
							else if (gl_state.units[j].arraycomponents == 2)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 2;
								qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, p[0], p[1]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 1;
								qglMultiTexCoord1f(GL_TEXTURE0_ARB + j, p[0]);
							}
						}
						else
						{
							if (gl_state.units[j].arraycomponents == 4)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 4;
								qglTexCoord4f(p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.units[j].arraycomponents == 3)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 3;
								qglTexCoord3f(p[0], p[1], p[2]);
							}
							else if (gl_state.units[j].arraycomponents == 2)
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 2;
								qglTexCoord2f(p[0], p[1]);
							}
							else
							{
								p = ((const GLfloat *)(gl_state.units[j].pointer_texcoord)) + element * 1;
								qglTexCoord1f(p[0]);
							}
						}
					}
				}
				if (gl_state.pointer_color && gl_state.pointer_color_enabled)
				{
					p = ((const GLfloat *)(gl_state.pointer_color)) + element * 4;
					qglColor4f(p[0], p[1], p[2], p[3]);
				}
				p = ((const GLfloat *)(gl_state.pointer_vertex)) + element * 3;
				qglVertex3f(p[0], p[1], p[2]);
			}
			qglEnd();
			CHECKGLERROR
		}
		else if (gl_mesh_testarrayelement.integer)
		{
			int i;
			qglBegin(GL_TRIANGLES);
			if (element3i)
			{
				for (i = 0;i < numtriangles * 3;i++)
					qglArrayElement(element3i[i]);
			}
			else if (element3s)
			{
				for (i = 0;i < numtriangles * 3;i++)
					qglArrayElement(element3s[i]);
			}
			qglEnd();
			CHECKGLERROR
		}
		else if (bufferobject3s)
		{
			GL_BindEBO(bufferobject3s);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices, numelements, GL_UNSIGNED_SHORT, (void *)(firsttriangle * sizeof(unsigned short[3])));
				CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_SHORT, (void *)(firsttriangle * sizeof(unsigned short[3])));
				CHECKGLERROR
			}
		}
		else if (bufferobject3i)
		{
			GL_BindEBO(bufferobject3i);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices, numelements, GL_UNSIGNED_INT, (void *)(firsttriangle * sizeof(unsigned int[3])));
				CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, (void *)(firsttriangle * sizeof(unsigned int[3])));
				CHECKGLERROR
			}
		}
		else if (element3s)
		{
			GL_BindEBO(0);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices, numelements, GL_UNSIGNED_SHORT, element3s);
				CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_SHORT, element3s);
				CHECKGLERROR
			}
		}
		else if (element3i)
		{
			GL_BindEBO(0);
			if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
			{
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices, numelements, GL_UNSIGNED_INT, element3i);
				CHECKGLERROR
			}
			else
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, element3i);
				CHECKGLERROR
			}
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

int R_Mesh_CreateStaticBufferObject(unsigned int target, void *data, size_t size, const char *name)
{
	gl_bufferobjectinfo_t *info;
	GLuint bufferobject;

	if (!gl_vbo.integer)
		return 0;

	qglGenBuffersARB(1, &bufferobject);
	switch(target)
	{
	case GL_ELEMENT_ARRAY_BUFFER_ARB: GL_BindEBO(bufferobject);break;
	case GL_ARRAY_BUFFER_ARB: GL_BindVBO(bufferobject);break;
	default: Sys_Error("R_Mesh_CreateStaticBufferObject: unknown target type %i\n", target);return 0;
	}
	qglBufferDataARB(target, size, data, GL_STATIC_DRAW_ARB);

	info = Mem_ExpandableArray_AllocRecord(&gl_bufferobjectinfoarray);
	memset(info, 0, sizeof(*info));
	info->target = target;
	info->object = bufferobject;
	info->size = size;
	strlcpy(info->name, name, sizeof(info->name));

	return (int)bufferobject;
}

void R_Mesh_DestroyBufferObject(int bufferobject)
{
	int i, endindex;
	gl_bufferobjectinfo_t *info;

	qglDeleteBuffersARB(1, (GLuint *)&bufferobject);

	endindex = Mem_ExpandableArray_IndexRange(&gl_bufferobjectinfoarray);
	for (i = 0;i < endindex;i++)
	{
		info = Mem_ExpandableArray_RecordAtIndex(&gl_bufferobjectinfoarray, i);
		if (!info)
			continue;
		if (info->object == bufferobject)
		{
			Mem_ExpandableArray_FreeRecord(&gl_bufferobjectinfoarray, (void *)info);
			break;
		}
	}
}

void GL_Mesh_ListVBOs(qboolean printeach)
{
	int i, endindex;
	size_t ebocount = 0, ebomemory = 0;
	size_t vbocount = 0, vbomemory = 0;
	gl_bufferobjectinfo_t *info;
	endindex = Mem_ExpandableArray_IndexRange(&gl_bufferobjectinfoarray);
	for (i = 0;i < endindex;i++)
	{
		info = Mem_ExpandableArray_RecordAtIndex(&gl_bufferobjectinfoarray, i);
		if (!info)
			continue;
		switch(info->target)
		{
		case GL_ELEMENT_ARRAY_BUFFER_ARB: ebocount++;ebomemory += info->size;if (printeach) Con_Printf("EBO #%i %s = %i bytes\n", info->object, info->name, (int)info->size);break;
		case GL_ARRAY_BUFFER_ARB: vbocount++;vbomemory += info->size;if (printeach) Con_Printf("VBO #%i %s = %i bytes\n", info->object, info->name, (int)info->size);break;
		default: Con_Printf("gl_vbostats: unknown target type %i\n", info->target);break;
		}
	}
	Con_Printf("vertex buffers: %i element buffers totalling %i bytes (%.3f MB), %i vertex buffers totalling %i bytes (%.3f MB), combined %i bytes (%.3fMB)\n", (int)ebocount, (int)ebomemory, ebomemory / 1048576.0, (int)vbocount, (int)vbomemory, vbomemory / 1048576.0, (int)(ebomemory + vbomemory), (ebomemory + vbomemory) / 1048576.0);
}

void R_Mesh_Matrix(const matrix4x4_t *matrix)
{
	if (memcmp(matrix, &backend_modelmatrix, sizeof(matrix4x4_t)))
	{
		double glmatrix[16];
		backend_modelmatrix = *matrix;
		Matrix4x4_Concat(&backend_modelviewmatrix, &backend_viewmatrix, matrix);
		Matrix4x4_ToArrayDoubleGL(&backend_modelviewmatrix, glmatrix);
		CHECKGLERROR
		qglLoadMatrixd(glmatrix);CHECKGLERROR
	}
}

void R_Mesh_VertexPointer(const float *vertex3f, int bufferobject, size_t bufferoffset)
{
	if (!gl_vbo.integer)
		bufferobject = 0;
	if (gl_state.pointer_vertex != vertex3f || gl_state.pointer_vertex_buffer != bufferobject || gl_state.pointer_vertex_offset != bufferoffset)
	{
		gl_state.pointer_vertex = vertex3f;
		gl_state.pointer_vertex_buffer = bufferobject;
		gl_state.pointer_vertex_offset = bufferoffset;
		CHECKGLERROR
		GL_BindVBO(bufferobject);
		qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), bufferobject ? (void *)bufferoffset : vertex3f);CHECKGLERROR
	}
}

void R_Mesh_ColorPointer(const float *color4f, int bufferobject, size_t bufferoffset)
{
	// note: this can not rely on bufferobject to decide whether a color array
	// is supplied, because surfmesh_t shares one vbo for all arrays, which
	// means that a valid vbo may be supplied even if there is no color array.
	if (color4f)
	{
		if (!gl_vbo.integer)
			bufferobject = 0;
		// caller wants color array enabled
		if (!gl_state.pointer_color_enabled)
		{
			gl_state.pointer_color_enabled = true;
			CHECKGLERROR
			qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
		}
		if (gl_state.pointer_color != color4f || gl_state.pointer_color_buffer != bufferobject || gl_state.pointer_color_offset != bufferoffset)
		{
			gl_state.pointer_color = color4f;
			gl_state.pointer_color_buffer = bufferobject;
			gl_state.pointer_color_offset = bufferoffset;
			CHECKGLERROR
			GL_BindVBO(bufferobject);
			qglColorPointer(4, GL_FLOAT, sizeof(float[4]), bufferobject ? (void *)bufferoffset : color4f);CHECKGLERROR
		}
	}
	else
	{
		// caller wants color array disabled
		if (gl_state.pointer_color_enabled)
		{
			gl_state.pointer_color_enabled = false;
			CHECKGLERROR
			qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
			// when color array is on the glColor gets trashed, set it again
			qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);CHECKGLERROR
		}
	}
}

void R_Mesh_TexCoordPointer(unsigned int unitnum, unsigned int numcomponents, const float *texcoord, int bufferobject, size_t bufferoffset)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	// update array settings
	CHECKGLERROR
	// note: there is no need to check bufferobject here because all cases
	// that involve a valid bufferobject also supply a texcoord array
	if (texcoord)
	{
		if (!gl_vbo.integer)
			bufferobject = 0;
		// texture array unit is enabled, enable the array
		if (!unit->arrayenabled)
		{
			unit->arrayenabled = true;
			GL_ActiveTexture(unitnum);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
		// texcoord array
		if (unit->pointer_texcoord != texcoord || unit->pointer_texcoord_buffer != bufferobject || unit->pointer_texcoord_offset != bufferoffset || unit->arraycomponents != numcomponents)
		{
			unit->pointer_texcoord = texcoord;
			unit->pointer_texcoord_buffer = bufferobject;
			unit->pointer_texcoord_offset = bufferoffset;
			unit->arraycomponents = numcomponents;
			GL_ActiveTexture(unitnum);
			GL_BindVBO(bufferobject);
			qglTexCoordPointer(unit->arraycomponents, GL_FLOAT, sizeof(float) * unit->arraycomponents, bufferobject ? (void *)bufferoffset : texcoord);CHECKGLERROR
		}
	}
	else
	{
		// texture array unit is disabled, disable the array
		if (unit->arrayenabled)
		{
			unit->arrayenabled = false;
			GL_ActiveTexture(unitnum);
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
}

void R_Mesh_TexBindAll(unsigned int unitnum, int tex1d, int tex2d, int tex3d, int texcubemap)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendimageunits)
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
				{
					qglEnable(GL_TEXTURE_1D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t1d)
				{
					qglDisable(GL_TEXTURE_1D);CHECKGLERROR
				}
			}
		}
		unit->t1d = tex1d;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
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
				{
					qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t2d)
				{
					qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				}
			}
		}
		unit->t2d = tex2d;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
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
				{
					qglEnable(GL_TEXTURE_3D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t3d)
				{
					qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				}
			}
		}
		unit->t3d = tex3d;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
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
				{
					qglEnable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
			}
			else
			{
				if (unit->tcubemap)
				{
					qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
			}
		}
		unit->tcubemap = texcubemap;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
	}
}

void R_Mesh_TexBind1D(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendimageunits)
		return;
	// update 1d texture binding
	if (unit->t1d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (texnum)
			{
				if (unit->t1d == 0)
				{
					qglEnable(GL_TEXTURE_1D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t1d)
				{
					qglDisable(GL_TEXTURE_1D);CHECKGLERROR
				}
			}
		}
		unit->t1d = texnum;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t2d)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
		}
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t3d)
			{
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
			}
		}
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->tcubemap)
			{
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
			}
		}
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
	}
}

void R_Mesh_TexBind(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendimageunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t1d)
			{
				qglDisable(GL_TEXTURE_1D);CHECKGLERROR
			}
		}
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (texnum)
			{
				if (unit->t2d == 0)
				{
					qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t2d)
				{
					qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				}
			}
		}
		unit->t2d = texnum;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t3d)
			{
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
			}
		}
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != 0)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->tcubemap)
			{
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
			}
		}
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
	}
}

void R_Mesh_TexBind3D(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendimageunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t1d)
			{
				qglDisable(GL_TEXTURE_1D);CHECKGLERROR
			}
		}
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t2d)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
		}
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (texnum)
			{
				if (unit->t3d == 0)
				{
					qglEnable(GL_TEXTURE_3D);CHECKGLERROR
				}
			}
			else
			{
				if (unit->t3d)
				{
					qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				}
			}
		}
		unit->t3d = texnum;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != 0)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->tcubemap)
			{
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
			}
		}
		unit->tcubemap = 0;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
	}
}

void R_Mesh_TexBindCubeMap(unsigned int unitnum, int texnum)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= backendimageunits)
		return;
	// update 1d texture binding
	if (unit->t1d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t1d)
			{
				qglDisable(GL_TEXTURE_1D);CHECKGLERROR
			}
		}
		unit->t1d = 0;
		qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
	}
	// update 2d texture binding
	if (unit->t2d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t2d)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
		}
		unit->t2d = 0;
		qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
	}
	// update 3d texture binding
	if (unit->t3d)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (unit->t3d)
			{
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
			}
		}
		unit->t3d = 0;
		qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
	}
	// update cubemap texture binding
	if (unit->tcubemap != texnum)
	{
		GL_ActiveTexture(unitnum);
		if (unitnum < backendunits)
		{
			if (texnum)
			{
				if (unit->tcubemap == 0)
				{
					qglEnable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
			}
			else
			{
				if (unit->tcubemap)
				{
					qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				}
			}
		}
		unit->tcubemap = texnum;
		qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
	}
}

static const double gl_identitymatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (matrix->m[3][3])
	{
		// texmatrix specified, check if it is different
		if (!unit->texmatrixenabled || memcmp(&unit->matrix, matrix, sizeof(matrix4x4_t)))
		{
			double glmatrix[16];
			unit->texmatrixenabled = true;
			unit->matrix = *matrix;
			CHECKGLERROR
			Matrix4x4_ToArrayDoubleGL(&unit->matrix, glmatrix);
			GL_ActiveTexture(unitnum);
			qglMatrixMode(GL_TEXTURE);CHECKGLERROR
			if (gl_workaround_mac_texmatrix.integer & 4)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
				qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
			}
			qglLoadMatrixd(glmatrix);CHECKGLERROR
			qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		}
	}
	else
	{
		// no texmatrix specified, revert to identity
		if (unit->texmatrixenabled)
		{
			unit->texmatrixenabled = false;
			unit->matrix = identitymatrix;
			CHECKGLERROR
			GL_ActiveTexture(unitnum);
			qglMatrixMode(GL_TEXTURE);CHECKGLERROR
			qglLoadIdentity();CHECKGLERROR
			if (gl_workaround_mac_texmatrix.integer)
			{
				qglActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
				qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
				qglLoadMatrixd(gl_identitymatrix);CHECKGLERROR
				if (gl_workaround_mac_texmatrix.integer & 1)
					qglLoadIdentity();CHECKGLERROR
			}
			qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		}
	}
}

void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	CHECKGLERROR
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

void R_Mesh_TextureState(const rmeshstate_t *m)
{
	unsigned int i;

	BACKENDACTIVECHECK

	CHECKGLERROR
	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
		CHECKGLERROR
	}

	for (i = 0;i < backendimageunits;i++)
		R_Mesh_TexBindAll(i, m->tex1d[i], m->tex[i], m->tex3d[i], m->texcubemap[i]);
	for (i = 0;i < backendarrayunits;i++)
	{
		if (m->pointer_texcoord3f[i])
			R_Mesh_TexCoordPointer(i, 3, m->pointer_texcoord3f[i], m->pointer_texcoord_bufferobject[i], m->pointer_texcoord_bufferoffset[i]);
		else
			R_Mesh_TexCoordPointer(i, 2, m->pointer_texcoord[i], m->pointer_texcoord_bufferobject[i], m->pointer_texcoord_bufferoffset[i]);
	}
	for (i = 0;i < backendunits;i++)
	{
		R_Mesh_TexMatrix(i, &m->texmatrix[i]);
		R_Mesh_TexCombine(i, m->texcombinergb[i], m->texcombinealpha[i], m->texrgbscale[i], m->texalphascale[i]);
	}
	CHECKGLERROR
}

void R_Mesh_ResetTextureState(void)
{
	unsigned int unitnum;

	BACKENDACTIVECHECK

	CHECKGLERROR
	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
		CHECKGLERROR
	}

	for (unitnum = 0;unitnum < backendimageunits;unitnum++)
	{
		gltextureunit_t *unit = gl_state.units + unitnum;
		// update 1d texture binding
		if (unit->t1d)
		{
			GL_ActiveTexture(unitnum);
			if (unitnum < backendunits)
			{
				qglDisable(GL_TEXTURE_1D);CHECKGLERROR
			}
			unit->t1d = 0;
			qglBindTexture(GL_TEXTURE_1D, unit->t1d);CHECKGLERROR
		}
		// update 2d texture binding
		if (unit->t2d)
		{
			GL_ActiveTexture(unitnum);
			if (unitnum < backendunits)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			}
			unit->t2d = 0;
			qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
		}
		// update 3d texture binding
		if (unit->t3d)
		{
			GL_ActiveTexture(unitnum);
			if (unitnum < backendunits)
			{
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
			}
			unit->t3d = 0;
			qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
		}
		// update cubemap texture binding
		if (unit->tcubemap)
		{
			GL_ActiveTexture(unitnum);
			if (unitnum < backendunits)
			{
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
			}
			unit->tcubemap = 0;
			qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
		}
	}
	for (unitnum = 0;unitnum < backendarrayunits;unitnum++)
	{
		gltextureunit_t *unit = gl_state.units + unitnum;
		// texture array unit is disabled, disable the array
		if (unit->arrayenabled)
		{
			unit->arrayenabled = false;
			GL_ActiveTexture(unitnum);
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
	for (unitnum = 0;unitnum < backendunits;unitnum++)
	{
		gltextureunit_t *unit = gl_state.units + unitnum;
		// no texmatrix specified, revert to identity
		if (unit->texmatrixenabled)
		{
			unit->texmatrixenabled = false;
			unit->matrix = identitymatrix;
			CHECKGLERROR
			qglMatrixMode(GL_TEXTURE);CHECKGLERROR
			GL_ActiveTexture(unitnum);
			qglLoadIdentity();CHECKGLERROR
			qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		}
		if (gl_combine.integer)
		{
			// GL_ARB_texture_env_combine
			if (unit->combinergb != GL_MODULATE)
			{
				unit->combinergb = GL_MODULATE;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, unit->combinergb);CHECKGLERROR
			}
			if (unit->combinealpha != GL_MODULATE)
			{
				unit->combinealpha = GL_MODULATE;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, unit->combinealpha);CHECKGLERROR
			}
			if (unit->rgbscale != 1)
			{
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (unit->rgbscale = 1));CHECKGLERROR
			}
			if (unit->alphascale != 1)
			{
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, (unit->alphascale = 1));CHECKGLERROR
			}
		}
		else
		{
			// normal GL texenv
			if (unit->combinergb != GL_MODULATE)
			{
				unit->combinergb = GL_MODULATE;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combinergb);CHECKGLERROR
			}
		}
	}
}
