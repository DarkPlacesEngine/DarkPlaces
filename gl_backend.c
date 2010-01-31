
#include "quakedef.h"
#include "cl_collision.h"

cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1", "use glDrawRangeElements function if available instead of glDrawElements (for performance comparisons or bug testing)"};
cvar_t gl_mesh_testarrayelement = {0, "gl_mesh_testarrayelement", "0", "use glBegin(GL_TRIANGLES);glArrayElement();glEnd(); primitives instead of glDrawElements (useful to test for driver bugs with glDrawElements)"};
cvar_t gl_mesh_testmanualfeeding = {0, "gl_mesh_testmanualfeeding", "0", "use glBegin(GL_TRIANGLES);glTexCoord2f();glVertex3f();glEnd(); primitives instead of glDrawElements (useful to test for driver bugs with glDrawElements)"};
cvar_t gl_mesh_prefer_short_elements = {0, "gl_mesh_prefer_short_elements", "1", "use GL_UNSIGNED_SHORT element arrays instead of GL_UNSIGNED_INT"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0", "enables OpenGL error checking and other tests"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0", "prints all OpenGL error checks, useful to identify location of driver crashes"};

cvar_t r_render = {0, "r_render", "1", "enables rendering 3D views (you want this on!)"};
cvar_t r_renderview = {0, "r_renderview", "1", "enables rendering 3D views (you want this on!)"};
cvar_t r_waterwarp = {CVAR_SAVE, "r_waterwarp", "1", "warp view while underwater"};
cvar_t gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1", "tints view while underwater, hurt, etc"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1", "enables OpenGL dithering (16bit looks bad with this off)"};
cvar_t gl_vbo = {CVAR_SAVE, "gl_vbo", "3", "make use of GL_ARB_vertex_buffer_object extension to store static geometry in video memory for faster rendering, 0 disables VBO allocation or use, 1 enables VBOs for vertex and triangle data, 2 only for vertex data, 3 for vertex data and triangle data of simple meshes (ones with only one surface)"};
cvar_t gl_fbo = {CVAR_SAVE, "gl_fbo", "1", "make use of GL_ARB_framebuffer_object extension to enable shadowmaps and other features using pixel formats different from the framebuffer"};

cvar_t v_flipped = {0, "v_flipped", "0", "mirror the screen (poor man's left handed mode)"};
qboolean v_flipped_state = false;

r_viewport_t gl_viewport;
matrix4x4_t gl_modelmatrix;
matrix4x4_t gl_viewmatrix;
matrix4x4_t gl_modelviewmatrix;
matrix4x4_t gl_projectionmatrix;
matrix4x4_t gl_modelviewprojectionmatrix;
float gl_modelview16f[16];
float gl_modelviewprojection16f[16];
qboolean gl_modelmatrixchanged;

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
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
	case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
		Con_Printf("GL_INVALID_FRAMEBUFFER_OPERATION at %s:%i\n", filename, linenumber);
		break;
#endif
	default:
		Con_Printf("GL UNKNOWN (%i) at %s:%i\n", errornumber, filename, linenumber);
		break;
	}
}
#endif

#define BACKENDACTIVECHECK if (!gl_state.active) Sys_Error("GL backend function called when backend is not active");

void SCR_ScreenShot_f (void);

typedef struct gl_bufferobjectinfo_s
{
	int target;
	int object;
	size_t size;
	char name[MAX_QPATH];
}
gl_bufferobjectinfo_t;

typedef struct gltextureunit_s
{
	const void *pointer_texcoord;
	size_t pointer_texcoord_offset;
	int pointer_texcoord_buffer;
	int t2d, t3d, tcubemap, trectangle;
	int arrayenabled;
	unsigned int arraycomponents;
	int rgbscale, alphascale;
	int combine;
	int combinergb, combinealpha;
	// texmatrixenabled exists only to avoid unnecessary texmatrix compares
	int texmatrixenabled;
	matrix4x4_t matrix;
}
gltextureunit_t;

typedef struct gl_state_s
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
	unsigned int clientunit;
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

	memexpandablearray_t bufferobjectinfoarray;

	qboolean active;
}
gl_state_t;

static gl_state_t gl_state;


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

int polygonelement3i[(POLYGONELEMENTS_MAXPOINTS-2)*3];
unsigned short polygonelement3s[(POLYGONELEMENTS_MAXPOINTS-2)*3];
int quadelement3i[QUADELEMENTS_MAXQUADS*6];
unsigned short quadelement3s[QUADELEMENTS_MAXQUADS*6];

void GL_VBOStats_f(void)
{
	GL_Mesh_ListVBOs(true);
}

static void GL_Backend_ResetState(void);

static void gl_backend_start(void)
{
	memset(&gl_state, 0, sizeof(gl_state));

	Mem_ExpandableArray_NewArray(&gl_state.bufferobjectinfoarray, r_main_mempool, sizeof(gl_bufferobjectinfo_t), 128);

	Con_DPrintf("OpenGL backend started.\n");

	CHECKGLERROR

	GL_Backend_ResetState();
}

static void gl_backend_shutdown(void)
{
	Con_DPrint("OpenGL Backend shutting down\n");

	Mem_ExpandableArray_FreeArray(&gl_state.bufferobjectinfoarray);

	memset(&gl_state, 0, sizeof(gl_state));
}

static void gl_backend_newmap(void)
{
}

void gl_backend_init(void)
{
	int i;

	for (i = 0;i < POLYGONELEMENTS_MAXPOINTS - 2;i++)
	{
		polygonelement3s[i * 3 + 0] = 0;
		polygonelement3s[i * 3 + 1] = i + 1;
		polygonelement3s[i * 3 + 2] = i + 2;
	}
	// elements for rendering a series of quads as triangles
	for (i = 0;i < QUADELEMENTS_MAXQUADS;i++)
	{
		quadelement3s[i * 6 + 0] = i * 4;
		quadelement3s[i * 6 + 1] = i * 4 + 1;
		quadelement3s[i * 6 + 2] = i * 4 + 2;
		quadelement3s[i * 6 + 3] = i * 4;
		quadelement3s[i * 6 + 4] = i * 4 + 2;
		quadelement3s[i * 6 + 5] = i * 4 + 3;
	}

	for (i = 0;i < (POLYGONELEMENTS_MAXPOINTS - 2)*3;i++)
		polygonelement3i[i] = polygonelement3s[i];
	for (i = 0;i < QUADELEMENTS_MAXQUADS*3;i++)
		quadelement3i[i] = quadelement3s[i];

	Cvar_RegisterVariable(&r_render);
	Cvar_RegisterVariable(&r_renderview);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&v_flipped);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_vbo);
	Cvar_RegisterVariable(&gl_paranoid);
	Cvar_RegisterVariable(&gl_printcheckerror);

	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
	Cvar_RegisterVariable(&gl_mesh_testarrayelement);
	Cvar_RegisterVariable(&gl_mesh_testmanualfeeding);
	Cvar_RegisterVariable(&gl_mesh_prefer_short_elements);

	Cmd_AddCommand("gl_vbostats", GL_VBOStats_f, "prints a list of all buffer objects (vertex data and triangle elements) and total video memory used by them");

	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
}

void GL_SetMirrorState(qboolean state);

void R_Viewport_TransformToScreen(const r_viewport_t *v, const vec4_t in, vec4_t out)
{
	vec4_t temp;
	float iw;
	Matrix4x4_Transform4 (&v->viewmatrix, in, temp);
	Matrix4x4_Transform4 (&v->projectmatrix, temp, out);
	iw = 1.0f / out[3];
	out[0] = v->x + (out[0] * iw + 1.0f) * v->width * 0.5f;
	out[1] = v->y + v->height - (out[1] * iw + 1.0f) * v->height * 0.5f;
	out[2] = v->z + (out[2] * iw + 1.0f) * v->depth * 0.5f;
}

static void R_Viewport_ApplyNearClipPlaneFloatGL(const r_viewport_t *v, float *m, float normalx, float normaly, float normalz, float dist)
{
	float q[4];
	float d;
	float clipPlane[4], v3[3], v4[3];
	float normal[3];

	// This is inspired by Oblique Depth Projection from http://www.terathon.com/code/oblique.php

	VectorSet(normal, normalx, normaly, normalz);
	Matrix4x4_Transform3x3(&v->viewmatrix, normal, clipPlane);
	VectorScale(normal, dist, v3);
	Matrix4x4_Transform(&v->viewmatrix, v3, v4);
	// FIXME: LordHavoc: I think this can be done more efficiently somehow but I can't remember the technique
	clipPlane[3] = -DotProduct(v4, clipPlane);

#if 0
{
	// testing code for comparing results
	float clipPlane2[4];
	VectorCopy4(clipPlane, clipPlane2);
	R_EntityMatrix(&identitymatrix);
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
	q[0] = ((clipPlane[0] < 0.0f ? -1.0f : clipPlane[0] > 0.0f ? 1.0f : 0.0f) + m[8]) / m[0];
	q[1] = ((clipPlane[1] < 0.0f ? -1.0f : clipPlane[1] > 0.0f ? 1.0f : 0.0f) + m[9]) / m[5];
	q[2] = -1.0f;
	q[3] = (1.0f + m[10]) / m[14];

	// Calculate the scaled plane vector
	d = 2.0f / DotProduct4(clipPlane, q);

	// Replace the third row of the projection matrix
	m[2] = clipPlane[0] * d;
	m[6] = clipPlane[1] * d;
	m[10] = clipPlane[2] * d + 1.0f;
	m[14] = clipPlane[3] * d;
}

void R_Viewport_InitOrtho(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float x1, float y1, float x2, float y2, float nearclip, float farclip, const float *nearplane)
{
	float left = x1, right = x2, bottom = y2, top = y1, zNear = nearclip, zFar = farclip;
	float m[16];
	memset(v, 0, sizeof(*v));
	v->type = R_VIEWPORTTYPE_ORTHO;
	v->cameramatrix = *cameramatrix;
	v->x = x;
	v->y = y;
	v->z = 0;
	v->width = width;
	v->height = height;
	v->depth = 1;
	memset(m, 0, sizeof(m));
	m[0]  = 2/(right - left);
	m[5]  = 2/(top - bottom);
	m[10] = -2/(zFar - zNear);
	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom)/(top - bottom);
	m[14] = - (zFar + zNear)/(zFar - zNear);
	m[15] = 1;
	v->screentodepth[0] = -farclip / (farclip - nearclip);
	v->screentodepth[1] = farclip * nearclip / (farclip - nearclip);

	Matrix4x4_Invert_Full(&v->viewmatrix, &v->cameramatrix);

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);

#if 0
	{
		vec4_t test1;
		vec4_t test2;
		Vector4Set(test1, (x1+x2)*0.5f, (y1+y2)*0.5f, 0.0f, 1.0f);
		R_Viewport_TransformToScreen(v, test1, test2);
		Con_Printf("%f %f %f -> %f %f %f\n", test1[0], test1[1], test1[2], test2[0], test2[1], test2[2]);
	}
#endif
}

void R_Viewport_InitPerspective(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float frustumx, float frustumy, float nearclip, float farclip, const float *nearplane)
{
	matrix4x4_t tempmatrix, basematrix;
	float m[16];
	memset(v, 0, sizeof(*v));

	if(v_flipped.integer)
		frustumx = -frustumx;

	v->type = R_VIEWPORTTYPE_PERSPECTIVE;
	v->cameramatrix = *cameramatrix;
	v->x = x;
	v->y = y;
	v->z = 0;
	v->width = width;
	v->height = height;
	v->depth = 1;
	memset(m, 0, sizeof(m));
	m[0]  = 1.0 / frustumx;
	m[5]  = 1.0 / frustumy;
	m[10] = -(farclip + nearclip) / (farclip - nearclip);
	m[11] = -1;
	m[14] = -2 * nearclip * farclip / (farclip - nearclip);
	v->screentodepth[0] = -farclip / (farclip - nearclip);
	v->screentodepth[1] = farclip * nearclip / (farclip - nearclip);

	Matrix4x4_Invert_Full(&tempmatrix, &v->cameramatrix);
	Matrix4x4_CreateRotate(&basematrix, -90, 1, 0, 0);
	Matrix4x4_ConcatRotate(&basematrix, 90, 0, 0, 1);
	Matrix4x4_Concat(&v->viewmatrix, &basematrix, &tempmatrix);

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

void R_Viewport_InitPerspectiveInfinite(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float frustumx, float frustumy, float nearclip, const float *nearplane)
{
	matrix4x4_t tempmatrix, basematrix;
	const float nudge = 1.0 - 1.0 / (1<<23);
	float m[16];
	memset(v, 0, sizeof(*v));

	if(v_flipped.integer)
		frustumx = -frustumx;

	v->type = R_VIEWPORTTYPE_PERSPECTIVE_INFINITEFARCLIP;
	v->cameramatrix = *cameramatrix;
	v->x = x;
	v->y = y;
	v->z = 0;
	v->width = width;
	v->height = height;
	v->depth = 1;
	memset(m, 0, sizeof(m));
	m[ 0] = 1.0 / frustumx;
	m[ 5] = 1.0 / frustumy;
	m[10] = -nudge;
	m[11] = -1;
	m[14] = -2 * nearclip * nudge;
	v->screentodepth[0] = (m[10] + 1) * 0.5 - 1;
	v->screentodepth[1] = m[14] * -0.5;

	Matrix4x4_Invert_Full(&tempmatrix, &v->cameramatrix);
	Matrix4x4_CreateRotate(&basematrix, -90, 1, 0, 0);
	Matrix4x4_ConcatRotate(&basematrix, 90, 0, 0, 1);
	Matrix4x4_Concat(&v->viewmatrix, &basematrix, &tempmatrix);

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

float cubeviewmatrix[6][16] =
{
    // standard cubemap projections
    { // +X
         0, 0,-1, 0,
         0,-1, 0, 0,
        -1, 0, 0, 0,
         0, 0, 0, 1,
    },
    { // -X
         0, 0, 1, 0,
         0,-1, 0, 0,
         1, 0, 0, 0,
         0, 0, 0, 1,
    },
    { // +Y
         1, 0, 0, 0,
         0, 0,-1, 0,
         0, 1, 0, 0,
         0, 0, 0, 1,
    },
    { // -Y
         1, 0, 0, 0,
         0, 0, 1, 0,
         0,-1, 0, 0,
         0, 0, 0, 1,
    },
    { // +Z
         1, 0, 0, 0,
         0,-1, 0, 0,
         0, 0,-1, 0,
         0, 0, 0, 1,
    },
    { // -Z
        -1, 0, 0, 0,
         0,-1, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1,
    },
};
float rectviewmatrix[6][16] =
{
    // sign-preserving cubemap projections
    { // +X
         0, 0,-1, 0,
         0, 1, 0, 0,
         1, 0, 0, 0,
         0, 0, 0, 1,
    },
    { // -X
         0, 0, 1, 0,
         0, 1, 0, 0,
         1, 0, 0, 0,
         0, 0, 0, 1,
    },
    { // +Y
         1, 0, 0, 0,
         0, 0,-1, 0,
         0, 1, 0, 0,
         0, 0, 0, 1,
    },
    { // -Y
         1, 0, 0, 0,
         0, 0, 1, 0,
         0, 1, 0, 0,
         0, 0, 0, 1,
    },
    { // +Z
         1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0,-1, 0,
         0, 0, 0, 1,
    },
    { // -Z
         1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1,
    },
};

void R_Viewport_InitCubeSideView(r_viewport_t *v, const matrix4x4_t *cameramatrix, int side, int size, float nearclip, float farclip, const float *nearplane)
{
	matrix4x4_t tempmatrix, basematrix;
	float m[16];
	memset(v, 0, sizeof(*v));
	v->type = R_VIEWPORTTYPE_PERSPECTIVECUBESIDE;
	v->cameramatrix = *cameramatrix;
	v->width = size;
	v->height = size;
	v->depth = 1;
	memset(m, 0, sizeof(m));
	m[0] = m[5] = 1.0f;
	m[10] = -(farclip + nearclip) / (farclip - nearclip);
	m[11] = -1;
	m[14] = -2 * nearclip * farclip / (farclip - nearclip);

	Matrix4x4_FromArrayFloatGL(&basematrix, cubeviewmatrix[side]);
	Matrix4x4_Invert_Simple(&tempmatrix, &v->cameramatrix);
	Matrix4x4_Concat(&v->viewmatrix, &basematrix, &tempmatrix);

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

void R_Viewport_InitRectSideView(r_viewport_t *v, const matrix4x4_t *cameramatrix, int side, int size, int border, float nearclip, float farclip, const float *nearplane)
{
	matrix4x4_t tempmatrix, basematrix;
	float m[16];
	memset(v, 0, sizeof(*v));
	v->type = R_VIEWPORTTYPE_PERSPECTIVECUBESIDE;
	v->cameramatrix = *cameramatrix;
	v->x = (side & 1) * size;
	v->y = (side >> 1) * size;
	v->width = size;
	v->height = size;
	v->depth = 1;
	memset(m, 0, sizeof(m));
	m[0] = m[5] = 1.0f * ((float)size - border) / size;
	m[10] = -(farclip + nearclip) / (farclip - nearclip);
	m[11] = -1;
	m[14] = -2 * nearclip * farclip / (farclip - nearclip);

	Matrix4x4_FromArrayFloatGL(&basematrix, rectviewmatrix[side]);
	Matrix4x4_Invert_Simple(&tempmatrix, &v->cameramatrix);
	Matrix4x4_Concat(&v->viewmatrix, &basematrix, &tempmatrix);

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

void R_SetViewport(const r_viewport_t *v)
{
	float m[16];
	gl_viewport = *v;

	CHECKGLERROR
	qglViewport(v->x, v->y, v->width, v->height);CHECKGLERROR

	// FIXME: v_flipped_state is evil, this probably breaks somewhere
	GL_SetMirrorState(v_flipped.integer && (v->type == R_VIEWPORTTYPE_PERSPECTIVE || v->type == R_VIEWPORTTYPE_PERSPECTIVE_INFINITEFARCLIP));

	// copy over the matrices to our state
	gl_viewmatrix = v->viewmatrix;
	gl_projectionmatrix = v->projectmatrix;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
//		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		// Load the projection matrix into OpenGL
		qglMatrixMode(GL_PROJECTION);CHECKGLERROR
		Matrix4x4_ToArrayFloatGL(&gl_projectionmatrix, m);
		qglLoadMatrixf(m);CHECKGLERROR
		qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		break;
	}

	// force an update of the derived matrices
	gl_modelmatrixchanged = true;
	R_EntityMatrix(&gl_modelmatrix);
}

void R_GetViewport(r_viewport_t *v)
{
	*v = gl_viewport;
}

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

static void GL_Backend_ResetState(void)
{
	unsigned int i;
	gl_state.active = true;
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

	if (vid.support.arb_vertex_buffer_object)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	if (vid.support.ext_framebuffer_object)
	{
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}

	qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), NULL);CHECKGLERROR
	qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

	qglColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL);CHECKGLERROR
	qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR

	GL_Color(0, 0, 0, 0);
	GL_Color(1, 1, 1, 1);

	gl_state.unit = MAX_TEXTUREUNITS;
	gl_state.clientunit = MAX_TEXTUREUNITS;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		for (i = 0;i < vid.teximageunits;i++)
		{
			GL_ActiveTexture(i);
			qglBindTexture(GL_TEXTURE_2D, 0);CHECKGLERROR
			if (vid.support.ext_texture_3d)
			{
				qglBindTexture(GL_TEXTURE_3D, 0);CHECKGLERROR
			}
			if (vid.support.arb_texture_cube_map)
			{
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);CHECKGLERROR
			}
			if (vid.support.arb_texture_rectangle)
			{
				qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);CHECKGLERROR
			}
		}

		for (i = 0;i < vid.texarrayunits;i++)
		{
			GL_ClientActiveTexture(i);
			GL_BindVBO(0);
			qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), NULL);CHECKGLERROR
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
		CHECKGLERROR
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		for (i = 0;i < vid.texunits;i++)
		{
			GL_ActiveTexture(i);
			GL_ClientActiveTexture(i);
			qglDisable(GL_TEXTURE_2D);CHECKGLERROR
			qglBindTexture(GL_TEXTURE_2D, 0);CHECKGLERROR
			if (vid.support.ext_texture_3d)
			{
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_3D, 0);CHECKGLERROR
			}
			if (vid.support.arb_texture_cube_map)
			{
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);CHECKGLERROR
			}
			if (vid.support.arb_texture_rectangle)
			{
				qglDisable(GL_TEXTURE_RECTANGLE_ARB);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);CHECKGLERROR
			}
			GL_BindVBO(0);
			qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), NULL);CHECKGLERROR
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			qglMatrixMode(GL_TEXTURE);CHECKGLERROR
			qglLoadIdentity();CHECKGLERROR
			qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);CHECKGLERROR
		}
		CHECKGLERROR
		break;
	}
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
			CHECKGLERROR
			qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.clientunit);
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

void GL_Scissor (int x, int y, int width, int height)
{
	CHECKGLERROR
	qglScissor(x, y,width,height);
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
	if (compilelog[0] && (strstr(compilelog, "error") || strstr(compilelog, "ERROR") || strstr(compilelog, "Error") || strstr(compilelog, "WARNING") || strstr(compilelog, "warning") || strstr(compilelog, "Warning")))
	{
		int i, j, pretextlines = 0;
		for (i = 0;i < numstrings - 1;i++)
			for (j = 0;strings[i][j];j++)
				if (strings[i][j] == '\n')
					pretextlines++;
		Con_Printf("%s shader compile log:\n%s\n(line offset for any above warnings/errors: %i)\n", shadertype, compilelog, pretextlines);
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
		if (strstr(linklog, "error") || strstr(linklog, "ERROR") || strstr(linklog, "Error") || strstr(linklog, "WARNING") || strstr(linklog, "warning") || strstr(linklog, "Warning"))
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
		if (numvertices < 0 || numtriangles < 0 || developer_extra.integer)
			Con_DPrintf("R_Mesh_Draw(%d, %d, %d, %d, %8p, %8p, %i, %i);\n", firstvertex, numvertices, firsttriangle, numtriangles, (void *)element3i, (void *)element3s, bufferobject3i, bufferobject3s);
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
		for (i = 0;i < vid.texarrayunits;i++)
		{
			if (gl_state.units[i].arrayenabled)
			{
				GL_ClientActiveTexture(i);
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
	if (r_render.integer || r_refdef.draw2dstage)
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
				for (j = 0;j < vid.texarrayunits;j++)
				{
					if (gl_state.units[j].pointer_texcoord && gl_state.units[j].arrayenabled)
					{
						if (vid.texarrayunits > 1)
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
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_SHORT, (void *)(firsttriangle * sizeof(unsigned short[3])));
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
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_INT, (void *)(firsttriangle * sizeof(unsigned int[3])));
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
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_SHORT, element3s);
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
				qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_INT, element3i);
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

	info = (gl_bufferobjectinfo_t *) Mem_ExpandableArray_AllocRecord(&gl_state.bufferobjectinfoarray);
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

	endindex = Mem_ExpandableArray_IndexRange(&gl_state.bufferobjectinfoarray);
	for (i = 0;i < endindex;i++)
	{
		info = (gl_bufferobjectinfo_t *) Mem_ExpandableArray_RecordAtIndex(&gl_state.bufferobjectinfoarray, i);
		if (!info)
			continue;
		if (info->object == bufferobject)
		{
			Mem_ExpandableArray_FreeRecord(&gl_state.bufferobjectinfoarray, (void *)info);
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
	endindex = Mem_ExpandableArray_IndexRange(&gl_state.bufferobjectinfoarray);
	for (i = 0;i < endindex;i++)
	{
		info = (gl_bufferobjectinfo_t *) Mem_ExpandableArray_RecordAtIndex(&gl_state.bufferobjectinfoarray, i);
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

void R_Mesh_VertexPointer(const float *vertex3f, int bufferobject, size_t bufferoffset)
{
	if (!gl_vbo.integer || gl_mesh_testarrayelement.integer)
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
		if (!gl_vbo.integer || gl_mesh_testarrayelement.integer)
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
		if (!gl_vbo.integer || gl_mesh_testarrayelement.integer)
			bufferobject = 0;
		// texture array unit is enabled, enable the array
		if (!unit->arrayenabled)
		{
			unit->arrayenabled = true;
			GL_ClientActiveTexture(unitnum);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
		// texcoord array
		if (unit->pointer_texcoord != texcoord || unit->pointer_texcoord_buffer != bufferobject || unit->pointer_texcoord_offset != bufferoffset || unit->arraycomponents != numcomponents)
		{
			unit->pointer_texcoord = texcoord;
			unit->pointer_texcoord_buffer = bufferobject;
			unit->pointer_texcoord_offset = bufferoffset;
			unit->arraycomponents = numcomponents;
			GL_ClientActiveTexture(unitnum);
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
			GL_ClientActiveTexture(unitnum);
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
		}
	}
}

int R_Mesh_TexBound(unsigned int unitnum, int id)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (unitnum >= vid.teximageunits)
		return 0;
	if (id == GL_TEXTURE_2D)
		return unit->t2d;
	if (id == GL_TEXTURE_3D)
		return unit->t3d;
	if (id == GL_TEXTURE_CUBE_MAP_ARB)
		return unit->tcubemap;
	if (id == GL_TEXTURE_RECTANGLE_ARB)
		return unit->trectangle;
	return 0;
}

void R_Mesh_CopyToTexture(rtexture_t *tex, int tx, int ty, int sx, int sy, int width, int height)
{
	R_Mesh_TexBind(0, tex);
	GL_ActiveTexture(0);CHECKGLERROR
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, tx, ty, sx, sy, width, height);CHECKGLERROR
}

void R_Mesh_TexBind(unsigned int unitnum, rtexture_t *tex)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	int tex2d, tex3d, texcubemap, texnum;
	if (unitnum >= vid.teximageunits)
		return;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		if (!tex)
			tex = r_texture_white;
		texnum = R_GetTexture(tex);
		switch(tex->gltexturetypeenum)
		{
		case GL_TEXTURE_2D: if (unit->t2d != texnum) {GL_ActiveTexture(unitnum);unit->t2d = texnum;qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR}break;
		case GL_TEXTURE_3D: if (unit->t3d != texnum) {GL_ActiveTexture(unitnum);unit->t3d = texnum;qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR}break;
		case GL_TEXTURE_CUBE_MAP_ARB: if (unit->tcubemap != texnum) {GL_ActiveTexture(unitnum);unit->tcubemap = texnum;qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR}break;
		case GL_TEXTURE_RECTANGLE_ARB: if (unit->trectangle != texnum) {GL_ActiveTexture(unitnum);unit->trectangle = texnum;qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, unit->trectangle);CHECKGLERROR}break;
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		tex2d = 0;
		tex3d = 0;
		texcubemap = 0;
		if (tex)
		{
			texnum = R_GetTexture(tex);
			switch(tex->gltexturetypeenum)
			{
			case GL_TEXTURE_2D:
				tex2d = texnum;
				break;
			case GL_TEXTURE_3D:
				tex3d = texnum;
				break;
			case GL_TEXTURE_CUBE_MAP_ARB:
				texcubemap = texnum;
				break;
			}
		}
		// update 2d texture binding
		if (unit->t2d != tex2d)
		{
			GL_ActiveTexture(unitnum);
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
			unit->t2d = tex2d;
			qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
		}
		// update 3d texture binding
		if (unit->t3d != tex3d)
		{
			GL_ActiveTexture(unitnum);
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
			unit->t3d = tex3d;
			qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
		}
		// update cubemap texture binding
		if (unit->tcubemap != texcubemap)
		{
			GL_ActiveTexture(unitnum);
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
			unit->tcubemap = texcubemap;
			qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
		}
		break;
	}
}

void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	if (matrix && matrix->m[3][3])
	{
		// texmatrix specified, check if it is different
		if (!unit->texmatrixenabled || memcmp(&unit->matrix, matrix, sizeof(matrix4x4_t)))
		{
			float glmatrix[16];
			unit->texmatrixenabled = true;
			unit->matrix = *matrix;
			CHECKGLERROR
			Matrix4x4_ToArrayFloatGL(&unit->matrix, glmatrix);
			GL_ActiveTexture(unitnum);
			qglMatrixMode(GL_TEXTURE);CHECKGLERROR
			qglLoadMatrixf(glmatrix);CHECKGLERROR
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
			qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		}
	}
}

void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	CHECKGLERROR
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		// do nothing
		break;
	case RENDERPATH_GL13:
		// GL_ARB_texture_env_combine
		if (!combinergb)
			combinergb = GL_MODULATE;
		if (!combinealpha)
			combinealpha = GL_MODULATE;
		if (!rgbscale)
			rgbscale = 1;
		if (!alphascale)
			alphascale = 1;
		if (combinergb != combinealpha || rgbscale != 1 || alphascale != 1)
		{
			if (combinergb == GL_DECAL)
				combinergb = GL_INTERPOLATE_ARB;
			if (unit->combine != GL_COMBINE_ARB)
			{
				unit->combine = GL_COMBINE_ARB;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);CHECKGLERROR
				qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);CHECKGLERROR // for GL_INTERPOLATE_ARB mode
			}
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
				unit->rgbscale = rgbscale;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, unit->rgbscale);CHECKGLERROR
			}
			if (unit->alphascale != alphascale)
			{
				unit->alphascale = alphascale;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, unit->alphascale);CHECKGLERROR
			}
		}
		else
		{
			if (unit->combine != combinergb)
			{
				unit->combine = combinergb;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combine);CHECKGLERROR
			}
		}
		break;
	case RENDERPATH_GL11:
		// normal GL texenv
		if (!combinergb)
			combinergb = GL_MODULATE;
		if (unit->combine != combinergb)
		{
			unit->combine = combinergb;
			GL_ActiveTexture(unitnum);
			qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combine);CHECKGLERROR
		}
		break;
	}
}

void R_Mesh_ResetTextureState(void)
{
	unsigned int unitnum;

	BACKENDACTIVECHECK

	CHECKGLERROR
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		for (unitnum = 0;unitnum < vid.teximageunits;unitnum++)
		{
			gltextureunit_t *unit = gl_state.units + unitnum;
			if (unit->t2d)
			{
				unit->t2d = 0;
				GL_ActiveTexture(unitnum);
				qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
			}
			if (unit->t3d)
			{
				unit->t3d = 0;
				GL_ActiveTexture(unitnum);
				qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
			}
			if (unit->tcubemap)
			{
				unit->tcubemap = 0;
				GL_ActiveTexture(unitnum);
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
			}
			if (unit->trectangle)
			{
				unit->trectangle = 0;
				GL_ActiveTexture(unitnum);
				qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, unit->trectangle);CHECKGLERROR
			}
		}
		for (unitnum = 0;unitnum < vid.texarrayunits;unitnum++)
		{
			gltextureunit_t *unit = gl_state.units + unitnum;
			if (unit->arrayenabled)
			{
				unit->arrayenabled = false;
				GL_ClientActiveTexture(unitnum);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
		}
		for (unitnum = 0;unitnum < vid.texunits;unitnum++)
		{
			gltextureunit_t *unit = gl_state.units + unitnum;
			if (unit->texmatrixenabled)
			{
				unit->texmatrixenabled = false;
				unit->matrix = identitymatrix;
				CHECKGLERROR
				GL_ActiveTexture(unitnum);
				qglMatrixMode(GL_TEXTURE);CHECKGLERROR
				qglLoadIdentity();CHECKGLERROR
				qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
			}
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		for (unitnum = 0;unitnum < vid.texunits;unitnum++)
		{
			gltextureunit_t *unit = gl_state.units + unitnum;
			if (unit->t2d)
			{
				unit->t2d = 0;
				GL_ActiveTexture(unitnum);
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR
			}
			if (unit->t3d)
			{
				unit->t3d = 0;
				GL_ActiveTexture(unitnum);
				qglDisable(GL_TEXTURE_3D);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR
			}
			if (unit->tcubemap)
			{
				unit->tcubemap = 0;
				GL_ActiveTexture(unitnum);
				qglDisable(GL_TEXTURE_CUBE_MAP_ARB);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR
			}
			if (unit->trectangle)
			{
				unit->trectangle = 0;
				GL_ActiveTexture(unitnum);
				qglDisable(GL_TEXTURE_RECTANGLE_ARB);CHECKGLERROR
				qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, unit->trectangle);CHECKGLERROR
			}
			if (unit->arrayenabled)
			{
				unit->arrayenabled = false;
				GL_ClientActiveTexture(unitnum);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			if (unit->texmatrixenabled)
			{
				unit->texmatrixenabled = false;
				unit->matrix = identitymatrix;
				CHECKGLERROR
				GL_ActiveTexture(unitnum);
				qglMatrixMode(GL_TEXTURE);CHECKGLERROR
				qglLoadIdentity();CHECKGLERROR
				qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
			}
			if (unit->combine != GL_MODULATE)
			{
				unit->combine = GL_MODULATE;
				GL_ActiveTexture(unitnum);
				qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, unit->combine);CHECKGLERROR
			}
		}
		break;
	}
}
