
#include "quakedef.h"
#include "cl_collision.h"
#include "dpsoftrast.h"
#ifdef SUPPORTD3D
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 vid_d3d9dev;
extern D3DCAPS9 vid_d3d9caps;
#endif

#define MAX_RENDERTARGETS 4

cvar_t gl_mesh_drawrangeelements = {0, "gl_mesh_drawrangeelements", "1", "use glDrawRangeElements function if available instead of glDrawElements (for performance comparisons or bug testing)"};
cvar_t gl_mesh_testmanualfeeding = {0, "gl_mesh_testmanualfeeding", "0", "use glBegin(GL_TRIANGLES);glTexCoord2f();glVertex3f();glEnd(); primitives instead of glDrawElements (useful to test for driver bugs with glDrawElements)"};
cvar_t gl_mesh_prefer_short_elements = {CVAR_SAVE, "gl_mesh_prefer_short_elements", "1", "use GL_UNSIGNED_SHORT element arrays instead of GL_UNSIGNED_INT"};
cvar_t gl_paranoid = {0, "gl_paranoid", "0", "enables OpenGL error checking and other tests"};
cvar_t gl_printcheckerror = {0, "gl_printcheckerror", "0", "prints all OpenGL error checks, useful to identify location of driver crashes"};

cvar_t r_render = {0, "r_render", "1", "enables rendering 3D views (you want this on!)"};
cvar_t r_renderview = {0, "r_renderview", "1", "enables rendering 3D views (you want this on!)"};
cvar_t r_waterwarp = {CVAR_SAVE, "r_waterwarp", "1", "warp view while underwater"};
cvar_t gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1", "tints view while underwater, hurt, etc"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1", "enables OpenGL dithering (16bit looks bad with this off)"};
cvar_t gl_vbo = {CVAR_SAVE, "gl_vbo", "3", "make use of GL_ARB_vertex_buffer_object extension to store static geometry in video memory for faster rendering, 0 disables VBO allocation or use, 1 enables VBOs for vertex and triangle data, 2 only for vertex data, 3 for vertex data and triangle data of simple meshes (ones with only one surface)"};
cvar_t gl_vbo_dynamicvertex = {CVAR_SAVE, "gl_vbo_dynamicvertex", "0", "make use of GL_ARB_vertex_buffer_object extension when rendering dynamic (animated/procedural) geometry such as text and particles"};
cvar_t gl_vbo_dynamicindex = {CVAR_SAVE, "gl_vbo_dynamicindex", "0", "make use of GL_ARB_vertex_buffer_object extension when rendering dynamic (animated/procedural) geometry such as text and particles"};
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

void GL_PrintError(int errornumber, const char *filename, int linenumber)
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

typedef struct gltextureunit_s
{
	int pointer_texcoord_components;
	int pointer_texcoord_gltype;
	size_t pointer_texcoord_stride;
	const void *pointer_texcoord_pointer;
	const r_meshbuffer_t *pointer_texcoord_vertexbuffer;
	size_t pointer_texcoord_offset;

	rtexture_t *texture;
	int t2d, t3d, tcubemap;
	int arrayenabled;
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
	qboolean blend;
	GLboolean depthmask;
	int colormask; // stored as bottom 4 bits: r g b a (3 2 1 0 order)
	int depthtest;
	int depthfunc;
	float depthrange[2];
	float polygonoffset[2];
	int alphatest;
	int alphafunc;
	float alphafuncvalue;
	qboolean alphatocoverage;
	int scissortest;
	unsigned int unit;
	unsigned int clientunit;
	gltextureunit_t units[MAX_TEXTUREUNITS];
	float color4f[4];
	int lockrange_first;
	int lockrange_count;
	int vertexbufferobject;
	int elementbufferobject;
	int framebufferobject;
	int defaultframebufferobject; // deal with platforms that use a non-zero default fbo
	qboolean pointer_color_enabled;

	int pointer_vertex_components;
	int pointer_vertex_gltype;
	size_t pointer_vertex_stride;
	const void *pointer_vertex_pointer;
	const r_meshbuffer_t *pointer_vertex_vertexbuffer;
	size_t pointer_vertex_offset;

	int pointer_color_components;
	int pointer_color_gltype;
	size_t pointer_color_stride;
	const void *pointer_color_pointer;
	const r_meshbuffer_t *pointer_color_vertexbuffer;
	size_t pointer_color_offset;

	void *preparevertices_tempdata;
	size_t preparevertices_tempdatamaxsize;
	r_meshbuffer_t *preparevertices_dynamicvertexbuffer;
	r_vertexgeneric_t *preparevertices_vertexgeneric;
	r_vertexmesh_t *preparevertices_vertexmesh;
	int preparevertices_numvertices;

	r_meshbuffer_t *draw_dynamicindexbuffer;

	qboolean usevbo_staticvertex;
	qboolean usevbo_staticindex;
	qboolean usevbo_dynamicvertex;
	qboolean usevbo_dynamicindex;

	memexpandablearray_t meshbufferarray;

	qboolean active;

#ifdef SUPPORTD3D
//	rtexture_t *d3drt_depthtexture;
//	rtexture_t *d3drt_colortextures[MAX_RENDERTARGETS];
	IDirect3DSurface9 *d3drt_depthsurface;
	IDirect3DSurface9 *d3drt_colorsurfaces[MAX_RENDERTARGETS];
	IDirect3DSurface9 *d3drt_backbufferdepthsurface;
	IDirect3DSurface9 *d3drt_backbuffercolorsurface;
	void *d3dvertexbuffer;
	void *d3dvertexdata;
	size_t d3dvertexsize;
#endif
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

static void R_Mesh_InitVertexDeclarations(void);
static void R_Mesh_DestroyVertexDeclarations(void);

static void R_Mesh_SetUseVBO(void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
		gl_state.usevbo_staticvertex = (vid.support.arb_vertex_buffer_object && gl_vbo.integer) || vid.forcevbo;
		gl_state.usevbo_staticindex = (vid.support.arb_vertex_buffer_object && (gl_vbo.integer == 1 || gl_vbo.integer == 3)) || vid.forcevbo;
		gl_state.usevbo_dynamicvertex = (vid.support.arb_vertex_buffer_object && gl_vbo_dynamicvertex.integer) || vid.forcevbo;
		gl_state.usevbo_dynamicindex = (vid.support.arb_vertex_buffer_object && gl_vbo_dynamicindex.integer) || vid.forcevbo;
		break;
	case RENDERPATH_D3D9:
		gl_state.usevbo_staticvertex = gl_state.usevbo_staticindex = (vid.support.arb_vertex_buffer_object && gl_vbo.integer) || vid.forcevbo;
		gl_state.usevbo_dynamicvertex = gl_state.usevbo_dynamicindex = (vid.support.arb_vertex_buffer_object && gl_vbo_dynamicvertex.integer && gl_vbo_dynamicindex.integer) || vid.forcevbo;
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		gl_state.usevbo_staticvertex = false;
		gl_state.usevbo_staticindex = false;
		gl_state.usevbo_dynamicvertex = false;
		gl_state.usevbo_dynamicindex = false;
		break;
	case RENDERPATH_GLES2:
		gl_state.usevbo_staticvertex = (vid.support.arb_vertex_buffer_object && gl_vbo.integer) || vid.forcevbo;
		gl_state.usevbo_staticindex = false;
		gl_state.usevbo_dynamicvertex = (vid.support.arb_vertex_buffer_object && gl_vbo_dynamicvertex.integer) || vid.forcevbo;
		gl_state.usevbo_dynamicindex = false;
		break;
	}
}

static void gl_backend_start(void)
{
	memset(&gl_state, 0, sizeof(gl_state));

	R_Mesh_InitVertexDeclarations();

	R_Mesh_SetUseVBO();
	Mem_ExpandableArray_NewArray(&gl_state.meshbufferarray, r_main_mempool, sizeof(r_meshbuffer_t), 128);

	Con_DPrintf("OpenGL backend started.\n");

	CHECKGLERROR

	GL_Backend_ResetState();

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		// fetch current fbo here (default fbo is not 0 on some GLES devices)
		if (vid.support.ext_framebuffer_object)
			qglGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &gl_state.defaultframebufferobject);
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_GetDepthStencilSurface(vid_d3d9dev, &gl_state.d3drt_backbufferdepthsurface);
		IDirect3DDevice9_GetRenderTarget(vid_d3d9dev, 0, &gl_state.d3drt_backbuffercolorsurface);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}
}

static void gl_backend_shutdown(void)
{
	Con_DPrint("OpenGL Backend shutting down\n");

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DSurface9_Release(gl_state.d3drt_backbufferdepthsurface);
		IDirect3DSurface9_Release(gl_state.d3drt_backbuffercolorsurface);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}

	if (gl_state.preparevertices_tempdata)
		Mem_Free(gl_state.preparevertices_tempdata);
	if (gl_state.preparevertices_dynamicvertexbuffer)
		R_Mesh_DestroyMeshBuffer(gl_state.preparevertices_dynamicvertexbuffer);

	Mem_ExpandableArray_FreeArray(&gl_state.meshbufferarray);

	R_Mesh_DestroyVertexDeclarations();

	memset(&gl_state, 0, sizeof(gl_state));
}

static void gl_backend_newmap(void)
{
}

static void gl_backend_devicelost(void)
{
	int i, endindex;
	r_meshbuffer_t *buffer;
#ifdef SUPPORTD3D
	gl_state.d3dvertexbuffer = NULL;
#endif
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DSurface9_Release(gl_state.d3drt_backbufferdepthsurface);
		IDirect3DSurface9_Release(gl_state.d3drt_backbuffercolorsurface);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}
	endindex = Mem_ExpandableArray_IndexRange(&gl_state.meshbufferarray);
	for (i = 0;i < endindex;i++)
	{
		buffer = (r_meshbuffer_t *) Mem_ExpandableArray_RecordAtIndex(&gl_state.meshbufferarray, i);
		if (!buffer || !buffer->isdynamic)
			continue;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_SOFT:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			if (buffer->devicebuffer)
			{
				if (buffer->isindexbuffer)
					IDirect3DIndexBuffer9_Release((IDirect3DIndexBuffer9*)buffer->devicebuffer);
				else
					IDirect3DVertexBuffer9_Release((IDirect3DVertexBuffer9*)buffer->devicebuffer);
				buffer->devicebuffer = NULL;
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		}
	}
}

static void gl_backend_devicerestored(void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_GetDepthStencilSurface(vid_d3d9dev, &gl_state.d3drt_backbufferdepthsurface);
		IDirect3DDevice9_GetRenderTarget(vid_d3d9dev, 0, &gl_state.d3drt_backbuffercolorsurface);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}
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
	for (i = 0;i < QUADELEMENTS_MAXQUADS*6;i++)
		quadelement3i[i] = quadelement3s[i];

	Cvar_RegisterVariable(&r_render);
	Cvar_RegisterVariable(&r_renderview);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&v_flipped);
	Cvar_RegisterVariable(&gl_dither);
	Cvar_RegisterVariable(&gl_vbo);
	Cvar_RegisterVariable(&gl_vbo_dynamicvertex);
	Cvar_RegisterVariable(&gl_vbo_dynamicindex);
	Cvar_RegisterVariable(&gl_paranoid);
	Cvar_RegisterVariable(&gl_printcheckerror);

	Cvar_RegisterVariable(&gl_mesh_drawrangeelements);
	Cvar_RegisterVariable(&gl_mesh_testmanualfeeding);
	Cvar_RegisterVariable(&gl_mesh_prefer_short_elements);

	Cmd_AddCommand("gl_vbostats", GL_VBOStats_f, "prints a list of all buffer objects (vertex data and triangle elements) and total video memory used by them");

	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap, gl_backend_devicelost, gl_backend_devicerestored);
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

	// for an odd reason, inverting this is wrong for R_Shadow_ScissorForBBox (we then get badly scissored lights)
	//out[1] = v->y + v->height - (out[1] * iw + 1.0f) * v->height * 0.5f;
	out[1] = v->y + (out[1] * iw + 1.0f) * v->height * 0.5f;

	out[2] = v->z + (out[2] * iw + 1.0f) * v->depth * 0.5f;
}

void GL_Finish(void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		qglFinish();
		break;
	case RENDERPATH_D3D9:
		//Con_DPrintf("FIXME D3D9 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_Finish();
		break;
	}
}

static int bboxedges[12][2] =
{
	// top
	{0, 1}, // +X
	{0, 2}, // +Y
	{1, 3}, // Y, +X
	{2, 3}, // X, +Y
	// bottom
	{4, 5}, // +X
	{4, 6}, // +Y
	{5, 7}, // Y, +X
	{6, 7}, // X, +Y
	// verticals
	{0, 4}, // +Z
	{1, 5}, // X, +Z
	{2, 6}, // Y, +Z
	{3, 7}, // XY, +Z
};

qboolean R_ScissorForBBox(const float *mins, const float *maxs, int *scissor)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;
	vec4_t v, v2;
	float vertex[20][3];
	int j, k;
	vec4_t plane4f;
	int numvertices;
	float corner[8][4];
	float dist[8];
	int sign[8];
	float f;

	scissor[0] = r_refdef.view.viewport.x;
	scissor[1] = r_refdef.view.viewport.y;
	scissor[2] = r_refdef.view.viewport.width;
	scissor[3] = r_refdef.view.viewport.height;

	// if view is inside the box, just say yes it's visible
	if (BoxesOverlap(r_refdef.view.origin, r_refdef.view.origin, mins, maxs))
		return false;

	x1 = y1 = x2 = y2 = 0;

	// transform all corners that are infront of the nearclip plane
	VectorNegate(r_refdef.view.frustum[4].normal, plane4f);
	plane4f[3] = r_refdef.view.frustum[4].dist;
	numvertices = 0;
	for (i = 0;i < 8;i++)
	{
		Vector4Set(corner[i], (i & 1) ? maxs[0] : mins[0], (i & 2) ? maxs[1] : mins[1], (i & 4) ? maxs[2] : mins[2], 1);
		dist[i] = DotProduct4(corner[i], plane4f);
		sign[i] = dist[i] > 0;
		if (!sign[i])
		{
			VectorCopy(corner[i], vertex[numvertices]);
			numvertices++;
		}
	}
	// if some points are behind the nearclip, add clipped edge points to make
	// sure that the scissor boundary is complete
	if (numvertices > 0 && numvertices < 8)
	{
		// add clipped edge points
		for (i = 0;i < 12;i++)
		{
			j = bboxedges[i][0];
			k = bboxedges[i][1];
			if (sign[j] != sign[k])
			{
				f = dist[j] / (dist[j] - dist[k]);
				VectorLerp(corner[j], f, corner[k], vertex[numvertices]);
				numvertices++;
			}
		}
	}

	// if we have no points to check, it is behind the view plane
	if (!numvertices)
		return true;

	// if we have some points to transform, check what screen area is covered
	x1 = y1 = x2 = y2 = 0;
	v[3] = 1.0f;
	//Con_Printf("%i vertices to transform...\n", numvertices);
	for (i = 0;i < numvertices;i++)
	{
		VectorCopy(vertex[i], v);
		R_Viewport_TransformToScreen(&r_refdef.view.viewport, v, v2);
		//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
		if (i)
		{
			if (x1 > v2[0]) x1 = v2[0];
			if (x2 < v2[0]) x2 = v2[0];
			if (y1 > v2[1]) y1 = v2[1];
			if (y2 < v2[1]) y2 = v2[1];
		}
		else
		{
			x1 = x2 = v2[0];
			y1 = y2 = v2[1];
		}
	}

	// now convert the scissor rectangle to integer screen coordinates
	ix1 = (int)(x1 - 1.0f);
	//iy1 = vid.height - (int)(y2 - 1.0f);
	//iy1 = r_refdef.view.viewport.width + 2 * r_refdef.view.viewport.x - (int)(y2 - 1.0f);
	iy1 = (int)(y1 - 1.0f);
	ix2 = (int)(x2 + 1.0f);
	//iy2 = vid.height - (int)(y1 + 1.0f);
	//iy2 = r_refdef.view.viewport.height + 2 * r_refdef.view.viewport.y - (int)(y1 + 1.0f);
	iy2 = (int)(y2 + 1.0f);
	//Con_Printf("%f %f %f %f\n", x1, y1, x2, y2);

	// clamp it to the screen
	if (ix1 < r_refdef.view.viewport.x) ix1 = r_refdef.view.viewport.x;
	if (iy1 < r_refdef.view.viewport.y) iy1 = r_refdef.view.viewport.y;
	if (ix2 > r_refdef.view.viewport.x + r_refdef.view.viewport.width) ix2 = r_refdef.view.viewport.x + r_refdef.view.viewport.width;
	if (iy2 > r_refdef.view.viewport.y + r_refdef.view.viewport.height) iy2 = r_refdef.view.viewport.y + r_refdef.view.viewport.height;

	// if it is inside out, it's not visible
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;

	// the light area is visible, set up the scissor rectangle
	scissor[0] = ix1;
	scissor[1] = iy1;
	scissor[2] = ix2 - ix1;
	scissor[3] = iy2 - iy1;

	// D3D Y coordinate is top to bottom, OpenGL is bottom to top, fix the D3D one
	switch(vid.renderpath)
	{
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		scissor[1] = vid.height - scissor[1] - scissor[3];
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	}

	return false;
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
	VectorScale(normal, -dist, v3);
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
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		m[10] = -1/(zFar - zNear);
		m[14] = -zNear/(zFar-zNear);
		break;
	}
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

	if(v_flipped.integer)
	{
		m[0] = -m[0];
		m[4] = -m[4];
		m[8] = -m[8];
		m[12] = -m[12];
	}

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

void R_Viewport_InitPerspectiveInfinite(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float frustumx, float frustumy, float nearclip, const float *nearplane)
{
	matrix4x4_t tempmatrix, basematrix;
	const float nudge = 1.0 - 1.0 / (1<<23);
	float m[16];
	memset(v, 0, sizeof(*v));

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

	if(v_flipped.integer)
	{
		m[0] = -m[0];
		m[4] = -m[4];
		m[8] = -m[8];
		m[12] = -m[12];
	}

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

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
		m[5] *= -1;
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}

	if (nearplane)
		R_Viewport_ApplyNearClipPlaneFloatGL(v, m, nearplane[0], nearplane[1], nearplane[2], nearplane[3]);

	Matrix4x4_FromArrayFloatGL(&v->projectmatrix, m);
}

void R_SetViewport(const r_viewport_t *v)
{
	float m[16];
	gl_viewport = *v;

	// FIXME: v_flipped_state is evil, this probably breaks somewhere
	GL_SetMirrorState(v_flipped.integer && (v->type == R_VIEWPORTTYPE_PERSPECTIVE || v->type == R_VIEWPORTTYPE_PERSPECTIVE_INFINITEFARCLIP));

	// copy over the matrices to our state
	gl_viewmatrix = v->viewmatrix;
	gl_projectionmatrix = v->projectmatrix;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
	case RENDERPATH_GLES1:
		CHECKGLERROR
		qglViewport(v->x, v->y, v->width, v->height);CHECKGLERROR
		// Load the projection matrix into OpenGL
		qglMatrixMode(GL_PROJECTION);CHECKGLERROR
		Matrix4x4_ToArrayFloatGL(&gl_projectionmatrix, m);
		qglLoadMatrixf(m);CHECKGLERROR
		qglMatrixMode(GL_MODELVIEW);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			D3DVIEWPORT9 d3dviewport;
			d3dviewport.X = gl_viewport.x;
			d3dviewport.Y = gl_viewport.y;
			d3dviewport.Width = gl_viewport.width;
			d3dviewport.Height = gl_viewport.height;
			d3dviewport.MinZ = gl_state.depthrange[0];
			d3dviewport.MaxZ = gl_state.depthrange[1];
			IDirect3DDevice9_SetViewport(vid_d3d9dev, &d3dviewport);
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_Viewport(v->x, v->y, v->width, v->height);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		qglViewport(v->x, v->y, v->width, v->height);CHECKGLERROR
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
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, bufferobject);CHECKGLERROR
	}
}

static void GL_BindEBO(int bufferobject)
{
	if (gl_state.elementbufferobject != bufferobject)
	{
		gl_state.elementbufferobject = bufferobject;
		CHECKGLERROR
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufferobject);CHECKGLERROR
	}
}

int R_Mesh_CreateFramebufferObject(rtexture_t *depthtexture, rtexture_t *colortexture, rtexture_t *colortexture2, rtexture_t *colortexture3, rtexture_t *colortexture4)
{
	int temp;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (!vid.support.ext_framebuffer_object)
			return 0;
		qglGenFramebuffersEXT(1, (GLuint*)&temp);CHECKGLERROR
		R_Mesh_SetRenderTargets(temp, NULL, NULL, NULL, NULL, NULL);
		if (depthtexture) qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, depthtexture->gltexturetypeenum, R_GetTexture(depthtexture), 0);CHECKGLERROR
		if (colortexture) qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, colortexture->gltexturetypeenum, R_GetTexture(colortexture), 0);CHECKGLERROR
		if (colortexture2) qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, colortexture2->gltexturetypeenum, R_GetTexture(colortexture2), 0);CHECKGLERROR
		if (colortexture3) qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, colortexture3->gltexturetypeenum, R_GetTexture(colortexture3), 0);CHECKGLERROR
		if (colortexture4) qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT3_EXT, colortexture4->gltexturetypeenum, R_GetTexture(colortexture4), 0);CHECKGLERROR
		return temp;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		return 1;
	case RENDERPATH_SOFT:
		return 1;
	}
	return 0;
}

void R_Mesh_DestroyFramebufferObject(int fbo)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (fbo)
			qglDeleteFramebuffersEXT(1, (GLuint*)&fbo);
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		break;
	}
}

#ifdef SUPPORTD3D
void R_Mesh_SetRenderTargetsD3D9(IDirect3DSurface9 *depthsurface, IDirect3DSurface9 *colorsurface0, IDirect3DSurface9 *colorsurface1, IDirect3DSurface9 *colorsurface2, IDirect3DSurface9 *colorsurface3)
{
// LordHavoc: for some weird reason the redundant SetDepthStencilSurface calls are necessary (otherwise the lights fail depth test, as if they were using the shadowmap depth surface and render target still)
	if (gl_state.d3drt_depthsurface == depthsurface && gl_state.d3drt_colorsurfaces[0] == colorsurface0 && gl_state.d3drt_colorsurfaces[1] == colorsurface1 && gl_state.d3drt_colorsurfaces[2] == colorsurface2 && gl_state.d3drt_colorsurfaces[3] == colorsurface3)
		return;

	gl_state.framebufferobject = depthsurface != gl_state.d3drt_backbufferdepthsurface || colorsurface0 != gl_state.d3drt_backbuffercolorsurface;
	if (gl_state.d3drt_depthsurface != depthsurface)
	{
		gl_state.d3drt_depthsurface = depthsurface;
		IDirect3DDevice9_SetDepthStencilSurface(vid_d3d9dev, gl_state.d3drt_depthsurface);
	}
	if (gl_state.d3drt_colorsurfaces[0] != colorsurface0)
	{
		gl_state.d3drt_colorsurfaces[0] = colorsurface0;
		IDirect3DDevice9_SetRenderTarget(vid_d3d9dev, 0, gl_state.d3drt_colorsurfaces[0]);
	}
	if (gl_state.d3drt_colorsurfaces[1] != colorsurface1)
	{
		gl_state.d3drt_colorsurfaces[1] = colorsurface1;
		IDirect3DDevice9_SetRenderTarget(vid_d3d9dev, 1, gl_state.d3drt_colorsurfaces[1]);
	}
	if (gl_state.d3drt_colorsurfaces[2] != colorsurface2)
	{
		gl_state.d3drt_colorsurfaces[2] = colorsurface2;
		IDirect3DDevice9_SetRenderTarget(vid_d3d9dev, 2, gl_state.d3drt_colorsurfaces[2]);
	}
	if (gl_state.d3drt_colorsurfaces[3] != colorsurface3)
	{
		gl_state.d3drt_colorsurfaces[3] = colorsurface3;
		IDirect3DDevice9_SetRenderTarget(vid_d3d9dev, 3, gl_state.d3drt_colorsurfaces[3]);
	}
}
#endif

void R_Mesh_ResetRenderTargets(void)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (gl_state.framebufferobject)
		{
			gl_state.framebufferobject = 0;
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gl_state.defaultframebufferobject);
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		R_Mesh_SetRenderTargetsD3D9(gl_state.d3drt_backbufferdepthsurface, gl_state.d3drt_backbuffercolorsurface, NULL, NULL, NULL);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetRenderTargets(vid.width, vid.height, vid.softdepthpixels, vid.softpixels, NULL, NULL, NULL);
		break;
	}
}

void R_Mesh_SetRenderTargets(int fbo, rtexture_t *depthtexture, rtexture_t *colortexture, rtexture_t *colortexture2, rtexture_t *colortexture3, rtexture_t *colortexture4)
{
	unsigned int i;
	unsigned int j;
	rtexture_t *textures[5];
	Vector4Set(textures, colortexture, colortexture2, colortexture3, colortexture4);
	textures[4] = depthtexture;
	// unbind any matching textures immediately, otherwise D3D will complain about a bound texture being used as a render target
	for (j = 0;j < 5;j++)
		if (textures[j])
			for (i = 0;i < vid.teximageunits;i++)
				if (gl_state.units[i].texture == textures[j])
					R_Mesh_TexBind(i, NULL);
	// set up framebuffer object or render targets for the active rendering API
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (gl_state.framebufferobject != fbo)
		{
			gl_state.framebufferobject = fbo;
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gl_state.framebufferobject ? gl_state.framebufferobject : gl_state.defaultframebufferobject);
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		// set up the new render targets, a NULL depthtexture intentionally binds nothing
		// TODO: optimize: keep surface pointer around in rtexture_t until texture is freed or lost
		if (fbo)
		{
			IDirect3DSurface9 *colorsurfaces[4];
			for (i = 0;i < 4;i++)
			{
				colorsurfaces[i] = NULL;
				if (textures[i])
					IDirect3DTexture9_GetSurfaceLevel((IDirect3DTexture9 *)textures[i]->d3dtexture, 0, &colorsurfaces[i]);
			}
			// set the render targets for real
			R_Mesh_SetRenderTargetsD3D9(depthtexture ? (IDirect3DSurface9 *)depthtexture->d3dtexture : NULL, colorsurfaces[0], colorsurfaces[1], colorsurfaces[2], colorsurfaces[3]);
			// release the texture surface levels (they won't be lost while bound...)
			for (i = 0;i < 4;i++)
				if (textures[i])
					IDirect3DSurface9_Release(colorsurfaces[i]);
		}
		else
			R_Mesh_SetRenderTargetsD3D9(gl_state.d3drt_backbufferdepthsurface, gl_state.d3drt_backbuffercolorsurface, NULL, NULL, NULL);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (fbo)
		{
			int width, height;
			unsigned int *pointers[5];
			memset(pointers, 0, sizeof(pointers));
			for (i = 0;i < 5;i++)
				pointers[i] = textures[i] ? (unsigned int *)DPSOFTRAST_Texture_GetPixelPointer(textures[i]->texnum, 0) : NULL;
			width = DPSOFTRAST_Texture_GetWidth(textures[0] ? textures[0]->texnum : textures[4]->texnum, 0);
			height = DPSOFTRAST_Texture_GetHeight(textures[0] ? textures[0]->texnum : textures[4]->texnum, 0);
			DPSOFTRAST_SetRenderTargets(width, height, pointers[4], pointers[0], pointers[1], pointers[2], pointers[3]);
		}
		else
			DPSOFTRAST_SetRenderTargets(vid.width, vid.height, vid.softdepthpixels, vid.softpixels, NULL, NULL, NULL);
		break;
	}
}

#ifdef SUPPORTD3D
static int d3dcmpforglfunc(int f)
{
	switch(f)
	{
	case GL_NEVER: return D3DCMP_NEVER;
	case GL_LESS: return D3DCMP_LESS;
	case GL_EQUAL: return D3DCMP_EQUAL;
	case GL_LEQUAL: return D3DCMP_LESSEQUAL;
	case GL_GREATER: return D3DCMP_GREATER;
	case GL_NOTEQUAL: return D3DCMP_NOTEQUAL;
	case GL_GEQUAL: return D3DCMP_GREATEREQUAL;
	case GL_ALWAYS: return D3DCMP_ALWAYS;
	default: Con_DPrintf("Unknown GL_DepthFunc\n");return D3DCMP_ALWAYS;
	}
}

static int d3dstencilopforglfunc(int f)
{
	switch(f)
	{
	case GL_KEEP: return D3DSTENCILOP_KEEP;
	case GL_INCR: return D3DSTENCILOP_INCR; // note: GL_INCR is clamped, D3DSTENCILOP_INCR wraps
	case GL_DECR: return D3DSTENCILOP_DECR; // note: GL_DECR is clamped, D3DSTENCILOP_DECR wraps
	default: Con_DPrintf("Unknown GL_StencilFunc\n");return D3DSTENCILOP_KEEP;
	}
}
#endif

extern cvar_t r_transparent_alphatocoverage;

static void GL_Backend_ResetState(void)
{
	unsigned int i;
	gl_state.active = true;
	gl_state.depthtest = true;
	gl_state.alphatest = false;
	gl_state.alphafunc = GL_GEQUAL;
	gl_state.alphafuncvalue = 0.5f;
	gl_state.alphatocoverage = false;
	gl_state.blendfunc1 = GL_ONE;
	gl_state.blendfunc2 = GL_ZERO;
	gl_state.blend = false;
	gl_state.depthmask = GL_TRUE;
	gl_state.colormask = 15;
	gl_state.color4f[0] = gl_state.color4f[1] = gl_state.color4f[2] = gl_state.color4f[3] = 1;
	gl_state.lockrange_first = 0;
	gl_state.lockrange_count = 0;
	gl_state.cullface = GL_FRONT;
	gl_state.cullfaceenable = false;
	gl_state.polygonoffset[0] = 0;
	gl_state.polygonoffset[1] = 0;
	gl_state.framebufferobject = 0;
	gl_state.depthfunc = GL_LEQUAL;

	switch(vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_COLORWRITEENABLE, gl_state.colormask);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CULLMODE, D3DCULL_NONE);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZFUNC, d3dcmpforglfunc(gl_state.depthfunc));
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZENABLE, gl_state.depthtest);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZWRITEENABLE, gl_state.depthmask);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_SLOPESCALEDEPTHBIAS, gl_state.polygonoffset[0]);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_DEPTHBIAS, gl_state.polygonoffset[1] * (1.0f / 16777216.0f));
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		CHECKGLERROR

		qglColorMask(1, 1, 1, 1);CHECKGLERROR
		qglAlphaFunc(gl_state.alphafunc, gl_state.alphafuncvalue);CHECKGLERROR
		qglDisable(GL_ALPHA_TEST);CHECKGLERROR
		qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
		qglDisable(GL_BLEND);CHECKGLERROR
		qglCullFace(gl_state.cullface);CHECKGLERROR
		qglDisable(GL_CULL_FACE);CHECKGLERROR
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
			//qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}

		qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), NULL);CHECKGLERROR
		qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR

		qglColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL);CHECKGLERROR
		qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
		qglColor4f(1, 1, 1, 1);CHECKGLERROR

		if (vid.support.ext_framebuffer_object)
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gl_state.framebufferobject);

		gl_state.unit = MAX_TEXTUREUNITS;
		gl_state.clientunit = MAX_TEXTUREUNITS;
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
	case RENDERPATH_SOFT:
		DPSOFTRAST_ColorMask(1,1,1,1);
		DPSOFTRAST_BlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);
		DPSOFTRAST_CullFace(gl_state.cullface);
		DPSOFTRAST_DepthFunc(gl_state.depthfunc);
		DPSOFTRAST_DepthMask(gl_state.depthmask);
		DPSOFTRAST_PolygonOffset(gl_state.polygonoffset[0], gl_state.polygonoffset[1]);
		DPSOFTRAST_SetRenderTargets(vid.width, vid.height, vid.softdepthpixels, vid.softpixels, NULL, NULL, NULL);
		DPSOFTRAST_Viewport(0, 0, vid.width, vid.height);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		qglColorMask(1, 1, 1, 1);CHECKGLERROR
		qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
		qglDisable(GL_BLEND);CHECKGLERROR
		qglCullFace(gl_state.cullface);CHECKGLERROR
		qglDisable(GL_CULL_FACE);CHECKGLERROR
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
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gl_state.defaultframebufferobject);
		qglEnableVertexAttribArray(GLSLATTRIB_POSITION);
		qglVertexAttribPointer(GLSLATTRIB_POSITION, 3, GL_FLOAT, false, sizeof(float[3]), NULL);CHECKGLERROR
		qglDisableVertexAttribArray(GLSLATTRIB_COLOR);
		qglVertexAttribPointer(GLSLATTRIB_COLOR, 4, GL_FLOAT, false, sizeof(float[4]), NULL);CHECKGLERROR
		qglVertexAttrib4f(GLSLATTRIB_COLOR, 1, 1, 1, 1);
		gl_state.unit = MAX_TEXTUREUNITS;
		gl_state.clientunit = MAX_TEXTUREUNITS;
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
		}
		for (i = 0;i < vid.texarrayunits;i++)
		{
			GL_BindVBO(0);
			qglVertexAttribPointer(i+GLSLATTRIB_TEXCOORD0, 2, GL_FLOAT, false, sizeof(float[2]), NULL);CHECKGLERROR
			qglDisableVertexAttribArray(i+GLSLATTRIB_TEXCOORD0);CHECKGLERROR
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
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			if (qglActiveTexture)
			{
				CHECKGLERROR
				qglActiveTexture(GL_TEXTURE0_ARB + gl_state.unit);
				CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
			break;
		case RENDERPATH_SOFT:
			break;
		}
	}
}

void GL_ClientActiveTexture(unsigned int num)
{
	if (gl_state.clientunit != num)
	{
		gl_state.clientunit = num;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
			if (qglActiveTexture)
			{
				CHECKGLERROR
				qglClientActiveTexture(GL_TEXTURE0_ARB + gl_state.clientunit);
				CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
			break;
		case RENDERPATH_SOFT:
			break;
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			break;
		}
	}
}

void GL_BlendFunc(int blendfunc1, int blendfunc2)
{
	if (gl_state.blendfunc1 != blendfunc1 || gl_state.blendfunc2 != blendfunc2)
	{
		qboolean blendenable;
		gl_state.blendfunc1 = blendfunc1;
		gl_state.blendfunc2 = blendfunc2;
		blendenable = (gl_state.blendfunc1 != GL_ONE || gl_state.blendfunc2 != GL_ZERO);
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			qglBlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);CHECKGLERROR
			if (gl_state.blend != blendenable)
			{
				gl_state.blend = blendenable;
				if (!gl_state.blend)
				{
					qglDisable(GL_BLEND);CHECKGLERROR
				}
				else
				{
					qglEnable(GL_BLEND);CHECKGLERROR
				}
			}
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			{
				int i;
				int glblendfunc[2];
				D3DBLEND d3dblendfunc[2];
				glblendfunc[0] = gl_state.blendfunc1;
				glblendfunc[1] = gl_state.blendfunc2;
				for (i = 0;i < 2;i++)
				{
					switch(glblendfunc[i])
					{
					case GL_ZERO: d3dblendfunc[i] = D3DBLEND_ZERO;break;
					case GL_ONE: d3dblendfunc[i] = D3DBLEND_ONE;break;
					case GL_SRC_COLOR: d3dblendfunc[i] = D3DBLEND_SRCCOLOR;break;
					case GL_ONE_MINUS_SRC_COLOR: d3dblendfunc[i] = D3DBLEND_INVSRCCOLOR;break;
					case GL_SRC_ALPHA: d3dblendfunc[i] = D3DBLEND_SRCALPHA;break;
					case GL_ONE_MINUS_SRC_ALPHA: d3dblendfunc[i] = D3DBLEND_INVSRCALPHA;break;
					case GL_DST_ALPHA: d3dblendfunc[i] = D3DBLEND_DESTALPHA;break;
					case GL_ONE_MINUS_DST_ALPHA: d3dblendfunc[i] = D3DBLEND_INVDESTALPHA;break;
					case GL_DST_COLOR: d3dblendfunc[i] = D3DBLEND_DESTCOLOR;break;
					case GL_ONE_MINUS_DST_COLOR: d3dblendfunc[i] = D3DBLEND_INVDESTCOLOR;break;
					}
				}
				IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_SRCBLEND, d3dblendfunc[0]);
				IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_DESTBLEND, d3dblendfunc[1]);
				if (gl_state.blend != blendenable)
				{
					gl_state.blend = blendenable;
					IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ALPHABLENDENABLE, gl_state.blend);
				}
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_BlendFunc(gl_state.blendfunc1, gl_state.blendfunc2);
			break;
		}
	}
}

void GL_DepthMask(int state)
{
	if (gl_state.depthmask != state)
	{
		gl_state.depthmask = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			qglDepthMask(gl_state.depthmask);CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZWRITEENABLE, gl_state.depthmask);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_DepthMask(gl_state.depthmask);
			break;
		}
	}
}

void GL_DepthTest(int state)
{
	if (gl_state.depthtest != state)
	{
		gl_state.depthtest = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			if (gl_state.depthtest)
			{
				qglEnable(GL_DEPTH_TEST);CHECKGLERROR
			}
			else
			{
				qglDisable(GL_DEPTH_TEST);CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZENABLE, gl_state.depthtest);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_DepthTest(gl_state.depthtest);
			break;
		}
	}
}

void GL_DepthFunc(int state)
{
	if (gl_state.depthfunc != state)
	{
		gl_state.depthfunc = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			qglDepthFunc(gl_state.depthfunc);CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_ZFUNC, d3dcmpforglfunc(gl_state.depthfunc));
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_DepthFunc(gl_state.depthfunc);
			break;
		}
	}
}

void GL_DepthRange(float nearfrac, float farfrac)
{
	if (gl_state.depthrange[0] != nearfrac || gl_state.depthrange[1] != farfrac)
	{
		gl_state.depthrange[0] = nearfrac;
		gl_state.depthrange[1] = farfrac;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			qglDepthRange(gl_state.depthrange[0], gl_state.depthrange[1]);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			{
				D3DVIEWPORT9 d3dviewport;
				d3dviewport.X = gl_viewport.x;
				d3dviewport.Y = gl_viewport.y;
				d3dviewport.Width = gl_viewport.width;
				d3dviewport.Height = gl_viewport.height;
				d3dviewport.MinZ = gl_state.depthrange[0];
				d3dviewport.MaxZ = gl_state.depthrange[1];
				IDirect3DDevice9_SetViewport(vid_d3d9dev, &d3dviewport);
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_DepthRange(gl_state.depthrange[0], gl_state.depthrange[1]);
			break;
		}
	}
}

void R_SetStencilSeparate(qboolean enable, int writemask, int frontfail, int frontzfail, int frontzpass, int backfail, int backzfail, int backzpass, int frontcompare, int backcompare, int comparereference, int comparemask)
{
	switch (vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		if (enable)
		{
			qglEnable(GL_STENCIL_TEST);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_STENCIL_TEST);CHECKGLERROR
		}
		if (vid.support.ati_separate_stencil)
		{
			qglStencilMask(writemask);CHECKGLERROR
			qglStencilOpSeparate(GL_FRONT, frontfail, frontzfail, frontzpass);CHECKGLERROR
			qglStencilOpSeparate(GL_BACK, backfail, backzfail, backzpass);CHECKGLERROR
			qglStencilFuncSeparate(frontcompare, backcompare, comparereference, comparereference);CHECKGLERROR
		}
		else if (vid.support.ext_stencil_two_side)
		{
			qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);CHECKGLERROR
			qglActiveStencilFaceEXT(GL_FRONT);CHECKGLERROR
			qglStencilMask(writemask);CHECKGLERROR
			qglStencilOp(frontfail, frontzfail, frontzpass);CHECKGLERROR
			qglStencilFunc(frontcompare, comparereference, comparemask);CHECKGLERROR
			qglActiveStencilFaceEXT(GL_BACK);CHECKGLERROR
			qglStencilMask(writemask);CHECKGLERROR
			qglStencilOp(backfail, backzfail, backzpass);CHECKGLERROR
			qglStencilFunc(backcompare, comparereference, comparemask);CHECKGLERROR
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_TWOSIDEDSTENCILMODE, true);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILENABLE, enable);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILWRITEMASK, writemask);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILFAIL, d3dstencilopforglfunc(frontfail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILZFAIL, d3dstencilopforglfunc(frontzfail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILPASS, d3dstencilopforglfunc(frontzpass));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILFUNC, d3dcmpforglfunc(frontcompare));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CCW_STENCILFAIL, d3dstencilopforglfunc(backfail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CCW_STENCILZFAIL, d3dstencilopforglfunc(backzfail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CCW_STENCILPASS, d3dstencilopforglfunc(backzpass));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CCW_STENCILFUNC, d3dcmpforglfunc(backcompare));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILREF, comparereference);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILMASK, comparemask);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		//Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}
}

void R_SetStencil(qboolean enable, int writemask, int fail, int zfail, int zpass, int compare, int comparereference, int comparemask)
{
	switch (vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		if (enable)
		{
			qglEnable(GL_STENCIL_TEST);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_STENCIL_TEST);CHECKGLERROR
		}
		if (vid.support.ext_stencil_two_side)
		{
			qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);CHECKGLERROR
		}
		qglStencilMask(writemask);CHECKGLERROR
		qglStencilOp(fail, zfail, zpass);CHECKGLERROR
		qglStencilFunc(compare, comparereference, comparemask);CHECKGLERROR
		CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		if (vid.support.ati_separate_stencil)
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_TWOSIDEDSTENCILMODE, true);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILENABLE, enable);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILWRITEMASK, writemask);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILFAIL, d3dstencilopforglfunc(fail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILZFAIL, d3dstencilopforglfunc(zfail));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILPASS, d3dstencilopforglfunc(zpass));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILFUNC, d3dcmpforglfunc(compare));
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILREF, comparereference);
		IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_STENCILMASK, comparemask);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		//Con_DPrintf("FIXME SOFT %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	}
}

void GL_PolygonOffset(float planeoffset, float depthoffset)
{
	if (gl_state.polygonoffset[0] != planeoffset || gl_state.polygonoffset[1] != depthoffset)
	{
		gl_state.polygonoffset[0] = planeoffset;
		gl_state.polygonoffset[1] = depthoffset;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			qglPolygonOffset(gl_state.polygonoffset[0], gl_state.polygonoffset[1]);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_SLOPESCALEDEPTHBIAS, gl_state.polygonoffset[0]);
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_DEPTHBIAS, gl_state.polygonoffset[1] * (1.0f / 16777216.0f));
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_PolygonOffset(gl_state.polygonoffset[0], gl_state.polygonoffset[1]);
			break;
		}
	}
}

void GL_SetMirrorState(qboolean state)
{
	if (v_flipped_state != state)
	{
		v_flipped_state = state;
		if (gl_state.cullface == GL_BACK)
			gl_state.cullface = GL_FRONT;
		else if (gl_state.cullface == GL_FRONT)
			gl_state.cullface = GL_BACK;
		else
			return;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			qglCullFace(gl_state.cullface);CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CULLMODE, gl_state.cullface == GL_FRONT ? D3DCULL_CCW : D3DCULL_CW);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_CullFace(gl_state.cullface);
			break;
		}
	}
}

void GL_CullFace(int state)
{
	if(v_flipped_state)
	{
		if(state == GL_FRONT)
			state = GL_BACK;
		else if(state == GL_BACK)
			state = GL_FRONT;
	}

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR

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
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		if (gl_state.cullface != state)
		{
			gl_state.cullface = state;
			switch(gl_state.cullface)
			{
			case GL_NONE:
				gl_state.cullfaceenable = false;
				IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CULLMODE, D3DCULL_NONE);
				break;
			case GL_FRONT:
				gl_state.cullfaceenable = true;
				IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CULLMODE, D3DCULL_CCW);
				break;
			case GL_BACK:
				gl_state.cullfaceenable = true;
				IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_CULLMODE, D3DCULL_CW);
				break;
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (gl_state.cullface != state)
		{
			gl_state.cullface = state;
			gl_state.cullfaceenable = state != GL_NONE ? true : false;
			DPSOFTRAST_CullFace(gl_state.cullface);
		}
		break;
	}
}

void GL_AlphaTest(int state)
{
	if (gl_state.alphatest != state)
	{
		gl_state.alphatest = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
			// only fixed function uses alpha test, other paths use pixel kill capability in shaders
			CHECKGLERROR
			if (gl_state.alphatest)
			{
				qglEnable(GL_ALPHA_TEST);CHECKGLERROR
			}
			else
			{
				qglDisable(GL_ALPHA_TEST);CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			break;
		}
	}
}

void GL_AlphaToCoverage(qboolean state)
{
	if (gl_state.alphatocoverage != state)
	{
		gl_state.alphatocoverage = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
			break;
		case RENDERPATH_GL20:
			// alpha to coverage turns the alpha value of the pixel into 0%, 25%, 50%, 75% or 100% by masking the multisample fragments accordingly
			CHECKGLERROR
			if (gl_state.alphatocoverage)
			{
				qglEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);CHECKGLERROR
//				qglEnable(GL_MULTISAMPLE_ARB);CHECKGLERROR
			}
			else
			{
				qglDisable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);CHECKGLERROR
//				qglDisable(GL_MULTISAMPLE_ARB);CHECKGLERROR
			}
			break;
		}
	}
}

void GL_ColorMask(int r, int g, int b, int a)
{
	// NOTE: this matches D3DCOLORWRITEENABLE_RED, GREEN, BLUE, ALPHA
	int state = (r ? 1 : 0) | (g ? 2 : 0) | (b ? 4 : 0) | (a ? 8 : 0);
	if (gl_state.colormask != state)
	{
		gl_state.colormask = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			qglColorMask((GLboolean)r, (GLboolean)g, (GLboolean)b, (GLboolean)a);CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_COLORWRITEENABLE, state);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_ColorMask(r, g, b, a);
			break;
		}
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
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
			CHECKGLERROR
			qglColor4f(gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);
			CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
			// no equivalent in D3D
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_Color4f(cr, cg, cb, ca);
			break;
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			qglVertexAttrib4f(GLSLATTRIB_COLOR, cr, cg, cb, ca);
			break;
		}
	}
}

void GL_Scissor (int x, int y, int width, int height)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		qglScissor(x, y,width,height);
		CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			RECT d3drect;
			d3drect.left = x;
			d3drect.top = y;
			d3drect.right = x + width;
			d3drect.bottom = y + height;
			IDirect3DDevice9_SetScissorRect(vid_d3d9dev, &d3drect);
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_Scissor(x, y, width, height);
		break;
	}
}

void GL_ScissorTest(int state)
{
	if (gl_state.scissortest != state)
	{
		gl_state.scissortest = state;
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			if(gl_state.scissortest)
				qglEnable(GL_SCISSOR_TEST);
			else
				qglDisable(GL_SCISSOR_TEST);
			CHECKGLERROR
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_SCISSORTESTENABLE, gl_state.scissortest);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_ScissorTest(gl_state.scissortest);
			break;
		}
	}
}

void GL_Clear(int mask, const float *colorvalue, float depthvalue, int stencilvalue)
{
	static const float blackcolor[4] = {0, 0, 0, 0};
	// prevent warnings when trying to clear a buffer that does not exist
	if (!colorvalue)
		colorvalue = blackcolor;
	if (!vid.stencil)
	{
		mask &= ~GL_STENCIL_BUFFER_BIT;
		stencilvalue = 0;
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		if (mask & GL_COLOR_BUFFER_BIT)
		{
			qglClearColor(colorvalue[0], colorvalue[1], colorvalue[2], colorvalue[3]);CHECKGLERROR
		}
		if (mask & GL_DEPTH_BUFFER_BIT)
		{
			qglClearDepth(depthvalue);CHECKGLERROR
		}
		if (mask & GL_STENCIL_BUFFER_BIT)
		{
			qglClearStencil(stencilvalue);CHECKGLERROR
		}
		qglClear(mask);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_Clear(vid_d3d9dev, 0, NULL, ((mask & GL_COLOR_BUFFER_BIT) ? D3DCLEAR_TARGET : 0) | ((mask & GL_STENCIL_BUFFER_BIT) ? D3DCLEAR_STENCIL : 0) | ((mask & GL_DEPTH_BUFFER_BIT) ? D3DCLEAR_ZBUFFER : 0), D3DCOLOR_COLORVALUE(colorvalue[0], colorvalue[1], colorvalue[2], colorvalue[3]), depthvalue, stencilvalue);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (mask & GL_COLOR_BUFFER_BIT)
			DPSOFTRAST_ClearColor(colorvalue[0], colorvalue[1], colorvalue[2], colorvalue[3]);
		if (mask & GL_DEPTH_BUFFER_BIT)
			DPSOFTRAST_ClearDepth(depthvalue);
		break;
	}
}

void GL_ReadPixelsBGRA(int x, int y, int width, int height, unsigned char *outpixels)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		qglReadPixels(x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, outpixels);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			// LordHavoc: we can't directly download the backbuffer because it may be
			// multisampled, and it may not be lockable, so we blit it to a lockable
			// surface of the same dimensions (but without multisample) to resolve the
			// multisample buffer to a normal image, and then lock that...
			IDirect3DSurface9 *stretchsurface = NULL;
			if (!FAILED(IDirect3DDevice9_CreateRenderTarget(vid_d3d9dev, vid.width, vid.height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE, &stretchsurface, NULL)))
			{
				D3DLOCKED_RECT lockedrect;
				if (!FAILED(IDirect3DDevice9_StretchRect(vid_d3d9dev, gl_state.d3drt_backbuffercolorsurface, NULL, stretchsurface, NULL, D3DTEXF_POINT)))
				{
					if (!FAILED(IDirect3DSurface9_LockRect(stretchsurface, &lockedrect, NULL, D3DLOCK_READONLY)))
					{
						int line;
						unsigned char *row = (unsigned char *)lockedrect.pBits + x * 4 + lockedrect.Pitch * (vid.height - 1 - y);
						for (line = 0;line < height;line++, row -= lockedrect.Pitch)
							memcpy(outpixels + line * width * 4, row, width * 4);
						IDirect3DSurface9_UnlockRect(stretchsurface);
					}
				}
				IDirect3DSurface9_Release(stretchsurface);
			}
			// code scraps
			//IDirect3DSurface9 *syssurface = NULL;
			//if (!FAILED(IDirect3DDevice9_CreateRenderTarget(vid_d3d9dev, vid.width, vid.height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &stretchsurface, NULL)))
			//if (!FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(vid_d3d9dev, vid.width, vid.height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &syssurface, NULL)))
			//IDirect3DDevice9_GetRenderTargetData(vid_d3d9dev, gl_state.d3drt_backbuffercolorsurface, syssurface);
			//if (!FAILED(IDirect3DDevice9_GetFrontBufferData(vid_d3d9dev, 0, syssurface)))
			//if (!FAILED(IDirect3DSurface9_LockRect(syssurface, &lockedrect, NULL, D3DLOCK_READONLY)))
			//IDirect3DSurface9_UnlockRect(syssurface);
			//IDirect3DSurface9_Release(syssurface);
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_GetPixelsBGRA(x, y, width, height, outpixels);
		break;
	}
}

// called at beginning of frame
void R_Mesh_Start(void)
{
	BACKENDACTIVECHECK
	R_Mesh_ResetRenderTargets();
	R_Mesh_SetUseVBO();
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
	shaderobject = qglCreateShader(shadertypeenum);CHECKGLERROR
	if (!shaderobject)
		return false;
	qglShaderSource(shaderobject, numstrings, strings, NULL);CHECKGLERROR
	qglCompileShader(shaderobject);CHECKGLERROR
	qglGetShaderiv(shaderobject, GL_COMPILE_STATUS, &shadercompiled);CHECKGLERROR
	qglGetShaderInfoLog(shaderobject, sizeof(compilelog), NULL, compilelog);CHECKGLERROR
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
		qglDeleteShader(shaderobject);CHECKGLERROR
		return false;
	}
	qglAttachShader(programobject, shaderobject);CHECKGLERROR
	qglDeleteShader(shaderobject);CHECKGLERROR
	return true;
}

unsigned int GL_Backend_CompileProgram(int vertexstrings_count, const char **vertexstrings_list, int geometrystrings_count, const char **geometrystrings_list, int fragmentstrings_count, const char **fragmentstrings_list)
{
	GLint programlinked;
	GLuint programobject = 0;
	char linklog[MAX_INPUTLINE];
	CHECKGLERROR

	programobject = qglCreateProgram();CHECKGLERROR
	if (!programobject)
		return 0;

	qglBindAttribLocation(programobject, GLSLATTRIB_POSITION , "Attrib_Position" );
	qglBindAttribLocation(programobject, GLSLATTRIB_COLOR    , "Attrib_Color"    );
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD0, "Attrib_TexCoord0");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD1, "Attrib_TexCoord1");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD2, "Attrib_TexCoord2");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD3, "Attrib_TexCoord3");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD4, "Attrib_TexCoord4");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD5, "Attrib_TexCoord5");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD6, "Attrib_TexCoord6");
	qglBindAttribLocation(programobject, GLSLATTRIB_TEXCOORD7, "Attrib_TexCoord7");
	if(vid.support.gl20shaders130)
		qglBindFragDataLocation(programobject, 0, "dp_FragColor");

	if (vertexstrings_count && !GL_Backend_CompileShader(programobject, GL_VERTEX_SHADER, "vertex", vertexstrings_count, vertexstrings_list))
		goto cleanup;

#ifdef GL_GEOMETRY_SHADER
	if (geometrystrings_count && !GL_Backend_CompileShader(programobject, GL_GEOMETRY_SHADER, "geometry", geometrystrings_count, geometrystrings_list))
		goto cleanup;
#endif

	if (fragmentstrings_count && !GL_Backend_CompileShader(programobject, GL_FRAGMENT_SHADER, "fragment", fragmentstrings_count, fragmentstrings_list))
		goto cleanup;

	qglLinkProgram(programobject);CHECKGLERROR
	qglGetProgramiv(programobject, GL_LINK_STATUS, &programlinked);CHECKGLERROR
	qglGetProgramInfoLog(programobject, sizeof(linklog), NULL, linklog);CHECKGLERROR
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
	qglDeleteProgram(programobject);CHECKGLERROR
	return 0;
}

void GL_Backend_FreeProgram(unsigned int prog)
{
	CHECKGLERROR
	qglDeleteProgram(prog);
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
void R_Mesh_Draw(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const r_meshbuffer_t *element3i_indexbuffer, size_t element3i_bufferoffset, const unsigned short *element3s, const r_meshbuffer_t *element3s_indexbuffer, size_t element3s_bufferoffset)
{
	unsigned int numelements = numtriangles * 3;
	int bufferobject3i;
	size_t bufferoffset3i;
	int bufferobject3s;
	size_t bufferoffset3s;
	if (numvertices < 3 || numtriangles < 1)
	{
		if (numvertices < 0 || numtriangles < 0 || developer_extra.integer)
			Con_DPrintf("R_Mesh_Draw(%d, %d, %d, %d, %8p, %8p, %8x, %8p, %8p, %8x);\n", firstvertex, numvertices, firsttriangle, numtriangles, (void *)element3i, (void *)element3i_indexbuffer, (int)element3i_bufferoffset, (void *)element3s, (void *)element3s_indexbuffer, (int)element3s_bufferoffset);
		return;
	}
	if (!gl_mesh_prefer_short_elements.integer)
	{
		if (element3i)
			element3s = NULL;
		if (element3i_indexbuffer)
			element3i_indexbuffer = NULL;
	}
	// adjust the pointers for firsttriangle
	if (element3i)
		element3i += firsttriangle * 3;
	if (element3i_indexbuffer)
		element3i_bufferoffset += firsttriangle * 3 * sizeof(*element3i);
	if (element3s)
		element3s += firsttriangle * 3;
	if (element3s_indexbuffer)
		element3s_bufferoffset += firsttriangle * 3 * sizeof(*element3s);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		// check if the user specified to ignore static index buffers
		if (!gl_state.usevbo_staticindex || (gl_vbo.integer == 3 && !vid.forcevbo && (element3i_bufferoffset || element3s_bufferoffset)))
		{
			element3i_indexbuffer = NULL;
			element3s_indexbuffer = NULL;
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		break;
	}
	// upload a dynamic index buffer if needed
	if (element3s)
	{
		if (!element3s_indexbuffer && gl_state.usevbo_dynamicindex)
		{
			if (gl_state.draw_dynamicindexbuffer)
				R_Mesh_UpdateMeshBuffer(gl_state.draw_dynamicindexbuffer, (void *)element3s, numelements * sizeof(*element3s));
			else
				gl_state.draw_dynamicindexbuffer = R_Mesh_CreateMeshBuffer((void *)element3s, numelements * sizeof(*element3s), "temporary", true, true, true);
			element3s_indexbuffer = gl_state.draw_dynamicindexbuffer;
			element3s_bufferoffset = 0;
		}
	}
	else if (element3i)
	{
		if (!element3i_indexbuffer && gl_state.usevbo_dynamicindex)
		{
			if (gl_state.draw_dynamicindexbuffer)
				R_Mesh_UpdateMeshBuffer(gl_state.draw_dynamicindexbuffer, (void *)element3i, numelements * sizeof(*element3i));
			else
				gl_state.draw_dynamicindexbuffer = R_Mesh_CreateMeshBuffer((void *)element3i, numelements * sizeof(*element3i), "temporary", true, true, false);
			element3i_indexbuffer = gl_state.draw_dynamicindexbuffer;
			element3i_bufferoffset = 0;
		}
	}
	bufferobject3i = element3i_indexbuffer ? element3i_indexbuffer->bufferobject : 0;
	bufferoffset3i = element3i_bufferoffset;
	bufferobject3s = element3s_indexbuffer ? element3s_indexbuffer->bufferobject : 0;
	bufferoffset3s = element3s_bufferoffset;
	r_refdef.stats.draws++;
	r_refdef.stats.draws_vertices += numvertices;
	r_refdef.stats.draws_elements += numelements;
	if (gl_paranoid.integer)
	{
		unsigned int i;
		// LordHavoc: disabled this - it needs to be updated to handle components and gltype and stride in each array
#if 0
		unsigned int j, size;
		const int *p;
		// note: there's no validation done here on buffer objects because it
		// is somewhat difficult to get at the data, and gl_paranoid can be
		// used without buffer objects if the need arises
		// (the data could be gotten using glMapBuffer but it would be very
		//  slow due to uncachable video memory reads)
		if (!qglIsEnabled(GL_VERTEX_ARRAY))
			Con_Print("R_Mesh_Draw: vertex array not enabled\n");
		CHECKGLERROR
		if (gl_state.pointer_vertex_pointer)
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
#endif
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
	}
	if (r_render.integer || r_refdef.draw2dstage)
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
			CHECKGLERROR
			if (gl_mesh_testmanualfeeding.integer)
			{
				unsigned int i, j, element;
				const GLfloat *p;
				qglBegin(GL_TRIANGLES);
				if(vid.renderpath == RENDERPATH_GL20)
				{
					for (i = 0;i < (unsigned int) numtriangles * 3;i++)
					{
						if (element3i)
							element = element3i[i];
						else if (element3s)
							element = element3s[i];
						else
							element = firstvertex + i;
						for (j = 0;j < vid.texarrayunits;j++)
						{
							if (gl_state.units[j].pointer_texcoord_pointer && gl_state.units[j].arrayenabled)
							{
								if (gl_state.units[j].pointer_texcoord_gltype == GL_FLOAT)
								{
									p = (const GLfloat *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (gl_state.units[j].pointer_texcoord_components == 4)
										qglVertexAttrib4f(GLSLATTRIB_TEXCOORD0 + j, p[0], p[1], p[2], p[3]);
									else if (gl_state.units[j].pointer_texcoord_components == 3)
										qglVertexAttrib3f(GLSLATTRIB_TEXCOORD0 + j, p[0], p[1], p[2]);
									else if (gl_state.units[j].pointer_texcoord_components == 2)
										qglVertexAttrib2f(GLSLATTRIB_TEXCOORD0 + j, p[0], p[1]);
									else
										qglVertexAttrib1f(GLSLATTRIB_TEXCOORD0 + j, p[0]);
								}
								else if (gl_state.units[j].pointer_texcoord_gltype == GL_SHORT)
								{
									const GLshort *s = (const GLshort *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (gl_state.units[j].pointer_texcoord_components == 4)
										qglVertexAttrib4f(GLSLATTRIB_TEXCOORD0 + j, s[0], s[1], s[2], s[3]);
									else if (gl_state.units[j].pointer_texcoord_components == 3)
										qglVertexAttrib3f(GLSLATTRIB_TEXCOORD0 + j, s[0], s[1], s[2]);
									else if (gl_state.units[j].pointer_texcoord_components == 2)
										qglVertexAttrib2f(GLSLATTRIB_TEXCOORD0 + j, s[0], s[1]);
									else if (gl_state.units[j].pointer_texcoord_components == 1)
										qglVertexAttrib1f(GLSLATTRIB_TEXCOORD0 + j, s[0]);
								}
								else if (gl_state.units[j].pointer_texcoord_gltype == GL_BYTE)
								{
									const GLbyte *sb = (const GLbyte *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (gl_state.units[j].pointer_texcoord_components == 4)
										qglVertexAttrib4f(GLSLATTRIB_TEXCOORD0 + j, sb[0], sb[1], sb[2], sb[3]);
									else if (gl_state.units[j].pointer_texcoord_components == 3)
										qglVertexAttrib3f(GLSLATTRIB_TEXCOORD0 + j, sb[0], sb[1], sb[2]);
									else if (gl_state.units[j].pointer_texcoord_components == 2)
										qglVertexAttrib2f(GLSLATTRIB_TEXCOORD0 + j, sb[0], sb[1]);
									else if (gl_state.units[j].pointer_texcoord_components == 1)
										qglVertexAttrib1f(GLSLATTRIB_TEXCOORD0 + j, sb[0]);
								}
							}
						}
						if (gl_state.pointer_color_pointer && gl_state.pointer_color_enabled && gl_state.pointer_color_components == 4)
						{
							if (gl_state.pointer_color_gltype == GL_FLOAT)
							{
								p = (const GLfloat *)((const unsigned char *)gl_state.pointer_color_pointer + element * gl_state.pointer_color_stride);
								qglVertexAttrib4f(GLSLATTRIB_COLOR, p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.pointer_color_gltype == GL_UNSIGNED_BYTE)
							{
								const GLubyte *ub = (const GLubyte *)((const unsigned char *)gl_state.pointer_color_pointer + element * gl_state.pointer_color_stride);
								qglVertexAttrib4Nub(GLSLATTRIB_COLOR, ub[0], ub[1], ub[2], ub[3]);
							}
						}
						if (gl_state.pointer_vertex_gltype == GL_FLOAT)
						{
							p = (const GLfloat *)((const unsigned char *)gl_state.pointer_vertex_pointer + element * gl_state.pointer_vertex_stride);
							if (gl_state.pointer_vertex_components == 4)
								qglVertexAttrib4f(GLSLATTRIB_POSITION, p[0], p[1], p[2], p[3]);
							else if (gl_state.pointer_vertex_components == 3)
								qglVertexAttrib3f(GLSLATTRIB_POSITION, p[0], p[1], p[2]);
							else
								qglVertexAttrib2f(GLSLATTRIB_POSITION, p[0], p[1]);
						}
					}
				}
				else
				{
					for (i = 0;i < (unsigned int) numtriangles * 3;i++)
					{
						if (element3i)
							element = element3i[i];
						else if (element3s)
							element = element3s[i];
						else
							element = firstvertex + i;
						for (j = 0;j < vid.texarrayunits;j++)
						{
							if (gl_state.units[j].pointer_texcoord_pointer && gl_state.units[j].arrayenabled)
							{
								if (gl_state.units[j].pointer_texcoord_gltype == GL_FLOAT)
								{
									p = (const GLfloat *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (vid.texarrayunits > 1)
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglMultiTexCoord4f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2], p[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, p[0], p[1], p[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, p[0], p[1]);
										else
											qglMultiTexCoord1f(GL_TEXTURE0_ARB + j, p[0]);
									}
									else
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglTexCoord4f(p[0], p[1], p[2], p[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglTexCoord3f(p[0], p[1], p[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglTexCoord2f(p[0], p[1]);
										else
											qglTexCoord1f(p[0]);
									}
								}
								else if (gl_state.units[j].pointer_texcoord_gltype == GL_SHORT)
								{
									const GLshort *s = (const GLshort *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (vid.texarrayunits > 1)
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglMultiTexCoord4f(GL_TEXTURE0_ARB + j, s[0], s[1], s[2], s[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, s[0], s[1], s[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, s[0], s[1]);
										else if (gl_state.units[j].pointer_texcoord_components == 1)
											qglMultiTexCoord1f(GL_TEXTURE0_ARB + j, s[0]);
									}
									else
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglTexCoord4f(s[0], s[1], s[2], s[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglTexCoord3f(s[0], s[1], s[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglTexCoord2f(s[0], s[1]);
										else if (gl_state.units[j].pointer_texcoord_components == 1)
											qglTexCoord1f(s[0]);
									}
								}
								else if (gl_state.units[j].pointer_texcoord_gltype == GL_BYTE)
								{
									const GLbyte *sb = (const GLbyte *)((const unsigned char *)gl_state.units[j].pointer_texcoord_pointer + element * gl_state.units[j].pointer_texcoord_stride);
									if (vid.texarrayunits > 1)
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglMultiTexCoord4f(GL_TEXTURE0_ARB + j, sb[0], sb[1], sb[2], sb[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglMultiTexCoord3f(GL_TEXTURE0_ARB + j, sb[0], sb[1], sb[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, sb[0], sb[1]);
										else if (gl_state.units[j].pointer_texcoord_components == 1)
											qglMultiTexCoord1f(GL_TEXTURE0_ARB + j, sb[0]);
									}
									else
									{
										if (gl_state.units[j].pointer_texcoord_components == 4)
											qglTexCoord4f(sb[0], sb[1], sb[2], sb[3]);
										else if (gl_state.units[j].pointer_texcoord_components == 3)
											qglTexCoord3f(sb[0], sb[1], sb[2]);
										else if (gl_state.units[j].pointer_texcoord_components == 2)
											qglTexCoord2f(sb[0], sb[1]);
										else if (gl_state.units[j].pointer_texcoord_components == 1)
											qglTexCoord1f(sb[0]);
									}
								}
							}
						}
						if (gl_state.pointer_color_pointer && gl_state.pointer_color_enabled && gl_state.pointer_color_components == 4)
						{
							if (gl_state.pointer_color_gltype == GL_FLOAT)
							{
								p = (const GLfloat *)((const unsigned char *)gl_state.pointer_color_pointer + element * gl_state.pointer_color_stride);
								qglColor4f(p[0], p[1], p[2], p[3]);
							}
							else if (gl_state.pointer_color_gltype == GL_UNSIGNED_BYTE)
							{
								const GLubyte *ub = (const GLubyte *)((const unsigned char *)gl_state.pointer_color_pointer + element * gl_state.pointer_color_stride);
								qglColor4ub(ub[0], ub[1], ub[2], ub[3]);
							}
						}
						if (gl_state.pointer_vertex_gltype == GL_FLOAT)
						{
							p = (const GLfloat *)((const unsigned char *)gl_state.pointer_vertex_pointer + element * gl_state.pointer_vertex_stride);
							if (gl_state.pointer_vertex_components == 4)
								qglVertex4f(p[0], p[1], p[2], p[3]);
							else if (gl_state.pointer_vertex_components == 3)
								qglVertex3f(p[0], p[1], p[2]);
							else
								qglVertex2f(p[0], p[1]);
						}
					}
				}
				qglEnd();
				CHECKGLERROR
			}
			else if (bufferobject3s)
			{
				GL_BindEBO(bufferobject3s);
				if (gl_mesh_drawrangeelements.integer && qglDrawRangeElements != NULL)
				{
					qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_SHORT, (void *)bufferoffset3s);
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
					qglDrawRangeElements(GL_TRIANGLES, firstvertex, firstvertex + numvertices - 1, numelements, GL_UNSIGNED_INT, (void *)bufferoffset3i);
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
			else
			{
				qglDrawArrays(GL_TRIANGLES, firstvertex, numvertices);
				CHECKGLERROR
			}
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			if (gl_state.d3dvertexbuffer && ((element3s && element3s_indexbuffer) || (element3i && element3i_indexbuffer)))
			{
				if (element3s_indexbuffer)
				{
					IDirect3DDevice9_SetIndices(vid_d3d9dev, (IDirect3DIndexBuffer9 *)element3s_indexbuffer->devicebuffer);
					IDirect3DDevice9_DrawIndexedPrimitive(vid_d3d9dev, D3DPT_TRIANGLELIST, 0, firstvertex, numvertices, element3s_bufferoffset>>1, numtriangles);
				}
				else if (element3i_indexbuffer)
				{
					IDirect3DDevice9_SetIndices(vid_d3d9dev, (IDirect3DIndexBuffer9 *)element3i_indexbuffer->devicebuffer);
					IDirect3DDevice9_DrawIndexedPrimitive(vid_d3d9dev, D3DPT_TRIANGLELIST, 0, firstvertex, numvertices, element3i_bufferoffset>>2, numtriangles);
				}
				else
					IDirect3DDevice9_DrawPrimitive(vid_d3d9dev, D3DPT_TRIANGLELIST, firstvertex, numvertices);
			}
			else
			{
				if (element3s)
					IDirect3DDevice9_DrawIndexedPrimitiveUP(vid_d3d9dev, D3DPT_TRIANGLELIST, firstvertex, numvertices, numtriangles, element3s, D3DFMT_INDEX16, gl_state.d3dvertexdata, gl_state.d3dvertexsize);
				else if (element3i)
					IDirect3DDevice9_DrawIndexedPrimitiveUP(vid_d3d9dev, D3DPT_TRIANGLELIST, firstvertex, numvertices, numtriangles, element3i, D3DFMT_INDEX32, gl_state.d3dvertexdata, gl_state.d3dvertexsize);
				else
					IDirect3DDevice9_DrawPrimitiveUP(vid_d3d9dev, D3DPT_TRIANGLELIST, numvertices, (void *)gl_state.d3dvertexdata, gl_state.d3dvertexsize);
			}
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_DrawTriangles(firstvertex, numvertices, numtriangles, element3i, element3s);
			break;
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			// GLES does not have glDrawRangeElements, and generally
			// underperforms with index buffers, so this code path is
			// relatively straightforward...
#if 0
			if (gl_paranoid.integer)
			{
				int r, prog, enabled, i;
				GLsizei 	attriblength;
				GLint 		attribsize;
				GLenum		attribtype;
				GLchar		attribname[1024];
				r = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
				if (r != GL_FRAMEBUFFER_COMPLETE_EXT)
					Con_DPrintf("fbo %i not complete (default %i)\n", gl_state.framebufferobject, gl_state.defaultframebufferobject);
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
				qglGetIntegerv(GL_CURRENT_PROGRAM, &r);CHECKGLERROR
				if (r < 0 || r > 10000)
					Con_DPrintf("GL_CURRENT_PROGRAM = %i\n", r);
				prog = r;
				for (i = 0;i < 8;i++)
				{
					qglGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &r);CHECKGLERROR
					if (!r)
						continue;
					qglGetActiveAttrib(prog, i, sizeof(attribname), &attriblength, &attribsize, &attribtype, attribname);CHECKGLERROR
					Con_DPrintf("prog %i position %i length %i size %04X type %i name \"%s\"\n", prog, i, (int)attriblength, (int)attribsize, (int)attribtype, (char *)attribname);
				}
			}
#endif
			if (element3s)
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_SHORT, element3s);
				CHECKGLERROR
			}
			else if (element3i)
			{
				qglDrawElements(GL_TRIANGLES, numelements, GL_UNSIGNED_INT, element3i);
				CHECKGLERROR
			}
			else
			{
				qglDrawArrays(GL_TRIANGLES, firstvertex, numvertices);
				CHECKGLERROR
			}
			break;
		}
	}
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	R_Mesh_ResetRenderTargets();
}

r_meshbuffer_t *R_Mesh_CreateMeshBuffer(const void *data, size_t size, const char *name, qboolean isindexbuffer, qboolean isdynamic, qboolean isindex16)
{
	r_meshbuffer_t *buffer;
	if (!(isdynamic ? (isindexbuffer ? gl_state.usevbo_dynamicindex : gl_state.usevbo_dynamicvertex) : (isindexbuffer ? gl_state.usevbo_staticindex : gl_state.usevbo_staticvertex)))
		return NULL;
	buffer = (r_meshbuffer_t *)Mem_ExpandableArray_AllocRecord(&gl_state.meshbufferarray);
	memset(buffer, 0, sizeof(*buffer));
	buffer->bufferobject = 0;
	buffer->devicebuffer = NULL;
	buffer->size = 0;
	buffer->isindexbuffer = isindexbuffer;
	buffer->isdynamic = isdynamic;
	buffer->isindex16 = isindex16;
	strlcpy(buffer->name, name, sizeof(buffer->name));
	R_Mesh_UpdateMeshBuffer(buffer, data, size);
	return buffer;
}

void R_Mesh_UpdateMeshBuffer(r_meshbuffer_t *buffer, const void *data, size_t size)
{
	if (!buffer)
		return;
	if (buffer->isindexbuffer)
	{
		r_refdef.stats.indexbufferuploadcount++;
		r_refdef.stats.indexbufferuploadsize += size;
	}
	else
	{
		r_refdef.stats.vertexbufferuploadcount++;
		r_refdef.stats.vertexbufferuploadsize += size;
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (!buffer->bufferobject)
			qglGenBuffersARB(1, (GLuint *)&buffer->bufferobject);
		if (buffer->isindexbuffer)
			GL_BindEBO(buffer->bufferobject);
		else
			GL_BindVBO(buffer->bufferobject);
		qglBufferDataARB(buffer->isindexbuffer ? GL_ELEMENT_ARRAY_BUFFER_ARB : GL_ARRAY_BUFFER_ARB, size, data, buffer->isdynamic ? GL_STREAM_DRAW_ARB : GL_STATIC_DRAW_ARB);
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			int result;
			void *datapointer = NULL;
			if (buffer->isindexbuffer)
			{
				IDirect3DIndexBuffer9 *d3d9indexbuffer = (IDirect3DIndexBuffer9 *)buffer->devicebuffer;
				if (size > buffer->size || !buffer->devicebuffer)
				{
					if (buffer->devicebuffer)
						IDirect3DIndexBuffer9_Release((IDirect3DIndexBuffer9*)buffer->devicebuffer);
					buffer->devicebuffer = NULL;
					if (FAILED(result = IDirect3DDevice9_CreateIndexBuffer(vid_d3d9dev, size, buffer->isdynamic ? D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC : 0, buffer->isindex16 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, buffer->isdynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &d3d9indexbuffer, NULL)))
						Sys_Error("IDirect3DDevice9_CreateIndexBuffer(%p, %d, %x, %x, %x, %p, NULL) returned %x\n", vid_d3d9dev, (int)size, buffer->isdynamic ? (int)D3DUSAGE_DYNAMIC : 0, buffer->isindex16 ? (int)D3DFMT_INDEX16 : (int)D3DFMT_INDEX32, buffer->isdynamic ? (int)D3DPOOL_DEFAULT : (int)D3DPOOL_MANAGED, &d3d9indexbuffer, (int)result);
					buffer->devicebuffer = (void *)d3d9indexbuffer;
					buffer->size = size;
				}
				if (!FAILED(IDirect3DIndexBuffer9_Lock(d3d9indexbuffer, 0, 0, &datapointer, buffer->isdynamic ? D3DLOCK_DISCARD : 0)))
				{
					if (data)
						memcpy(datapointer, data, size);
					else
						memset(datapointer, 0, size);
					IDirect3DIndexBuffer9_Unlock(d3d9indexbuffer);
				}
			}
			else
			{
				IDirect3DVertexBuffer9 *d3d9vertexbuffer = (IDirect3DVertexBuffer9 *)buffer->devicebuffer;
				if (size > buffer->size || !buffer->devicebuffer)
				{
					if (buffer->devicebuffer)
						IDirect3DVertexBuffer9_Release((IDirect3DVertexBuffer9*)buffer->devicebuffer);
					buffer->devicebuffer = NULL;
					if (FAILED(result = IDirect3DDevice9_CreateVertexBuffer(vid_d3d9dev, size, buffer->isdynamic ? D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC : 0, 0, buffer->isdynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &d3d9vertexbuffer, NULL)))
						Sys_Error("IDirect3DDevice9_CreateVertexBuffer(%p, %d, %x, %x, %x, %p, NULL) returned %x\n", vid_d3d9dev, (int)size, buffer->isdynamic ? (int)D3DUSAGE_DYNAMIC : 0, 0, buffer->isdynamic ? (int)D3DPOOL_DEFAULT : (int)D3DPOOL_MANAGED, &d3d9vertexbuffer, (int)result);
					buffer->devicebuffer = (void *)d3d9vertexbuffer;
					buffer->size = size;
				}
				if (!FAILED(IDirect3DVertexBuffer9_Lock(d3d9vertexbuffer, 0, 0, &datapointer, buffer->isdynamic ? D3DLOCK_DISCARD : 0)))
				{
					if (data)
						memcpy(datapointer, data, size);
					else
						memset(datapointer, 0, size);
					IDirect3DVertexBuffer9_Unlock(d3d9vertexbuffer);
				}
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}
}

void R_Mesh_DestroyMeshBuffer(r_meshbuffer_t *buffer)
{
	if (!buffer)
		return;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		qglDeleteBuffersARB(1, (GLuint *)&buffer->bufferobject);
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		if (gl_state.d3dvertexbuffer == (void *)buffer)
			gl_state.d3dvertexbuffer = NULL;
		if (buffer->devicebuffer)
		{
			if (buffer->isindexbuffer)
				IDirect3DIndexBuffer9_Release((IDirect3DIndexBuffer9 *)buffer->devicebuffer);
			else
				IDirect3DVertexBuffer9_Release((IDirect3DVertexBuffer9 *)buffer->devicebuffer);
			buffer->devicebuffer = NULL;
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}
	Mem_ExpandableArray_FreeRecord(&gl_state.meshbufferarray, (void *)buffer);
}

void GL_Mesh_ListVBOs(qboolean printeach)
{
	int i, endindex;
	size_t ebocount = 0, ebomemory = 0;
	size_t vbocount = 0, vbomemory = 0;
	r_meshbuffer_t *buffer;
	endindex = Mem_ExpandableArray_IndexRange(&gl_state.meshbufferarray);
	for (i = 0;i < endindex;i++)
	{
		buffer = (r_meshbuffer_t *) Mem_ExpandableArray_RecordAtIndex(&gl_state.meshbufferarray, i);
		if (!buffer)
			continue;
		if (buffer->isindexbuffer) {ebocount++;ebomemory += buffer->size;if (printeach) Con_Printf("indexbuffer #%i %s = %i bytes%s\n", i, buffer->name, (int)buffer->size, buffer->isdynamic ? " (dynamic)" : " (static)");}
		else                       {vbocount++;vbomemory += buffer->size;if (printeach) Con_Printf("vertexbuffer #%i %s = %i bytes%s\n", i, buffer->name, (int)buffer->size, buffer->isdynamic ? " (dynamic)" : " (static)");}
	}
	Con_Printf("vertex buffers: %i indexbuffers totalling %i bytes (%.3f MB), %i vertexbuffers totalling %i bytes (%.3f MB), combined %i bytes (%.3fMB)\n", (int)ebocount, (int)ebomemory, ebomemory / 1048576.0, (int)vbocount, (int)vbomemory, vbomemory / 1048576.0, (int)(ebomemory + vbomemory), (ebomemory + vbomemory) / 1048576.0);
}



void R_Mesh_VertexPointer(int components, int gltype, size_t stride, const void *pointer, const r_meshbuffer_t *vertexbuffer, size_t bufferoffset)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (gl_state.pointer_vertex_components != components || gl_state.pointer_vertex_gltype != gltype || gl_state.pointer_vertex_stride != stride || gl_state.pointer_vertex_pointer != pointer || gl_state.pointer_vertex_vertexbuffer != vertexbuffer || gl_state.pointer_vertex_offset != bufferoffset)
		{
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			gl_state.pointer_vertex_components = components;
			gl_state.pointer_vertex_gltype = gltype;
			gl_state.pointer_vertex_stride = stride;
			gl_state.pointer_vertex_pointer = pointer;
			gl_state.pointer_vertex_vertexbuffer = vertexbuffer;
			gl_state.pointer_vertex_offset = bufferoffset;
			CHECKGLERROR
			GL_BindVBO(bufferobject);
			qglVertexPointer(components, gltype, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
		}
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (gl_state.pointer_vertex_components != components || gl_state.pointer_vertex_gltype != gltype || gl_state.pointer_vertex_stride != stride || gl_state.pointer_vertex_pointer != pointer || gl_state.pointer_vertex_vertexbuffer != vertexbuffer || gl_state.pointer_vertex_offset != bufferoffset)
		{
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			gl_state.pointer_vertex_components = components;
			gl_state.pointer_vertex_gltype = gltype;
			gl_state.pointer_vertex_stride = stride;
			gl_state.pointer_vertex_pointer = pointer;
			gl_state.pointer_vertex_vertexbuffer = vertexbuffer;
			gl_state.pointer_vertex_offset = bufferoffset;
			CHECKGLERROR
			GL_BindVBO(bufferobject);
			qglVertexAttribPointer(GLSLATTRIB_POSITION, components, gltype, false, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	}
}

void R_Mesh_ColorPointer(int components, int gltype, size_t stride, const void *pointer, const r_meshbuffer_t *vertexbuffer, size_t bufferoffset)
{
	// note: vertexbuffer may be non-NULL even if pointer is NULL, so check
	// the pointer only.
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		CHECKGLERROR
		if (pointer)
		{
			// caller wants color array enabled
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			if (!gl_state.pointer_color_enabled)
			{
				gl_state.pointer_color_enabled = true;
				CHECKGLERROR
				qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
			}
			if (gl_state.pointer_color_components != components || gl_state.pointer_color_gltype != gltype || gl_state.pointer_color_stride != stride || gl_state.pointer_color_pointer != pointer || gl_state.pointer_color_vertexbuffer != vertexbuffer || gl_state.pointer_color_offset != bufferoffset)
			{
				gl_state.pointer_color_components = components;
				gl_state.pointer_color_gltype = gltype;
				gl_state.pointer_color_stride = stride;
				gl_state.pointer_color_pointer = pointer;
				gl_state.pointer_color_vertexbuffer = vertexbuffer;
				gl_state.pointer_color_offset = bufferoffset;
				CHECKGLERROR
				GL_BindVBO(bufferobject);
				qglColorPointer(components, gltype, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
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
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		if (pointer)
		{
			// caller wants color array enabled
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			if (!gl_state.pointer_color_enabled)
			{
				gl_state.pointer_color_enabled = true;
				CHECKGLERROR
				qglEnableVertexAttribArray(GLSLATTRIB_COLOR);CHECKGLERROR
			}
			if (gl_state.pointer_color_components != components || gl_state.pointer_color_gltype != gltype || gl_state.pointer_color_stride != stride || gl_state.pointer_color_pointer != pointer || gl_state.pointer_color_vertexbuffer != vertexbuffer || gl_state.pointer_color_offset != bufferoffset)
			{
				gl_state.pointer_color_components = components;
				gl_state.pointer_color_gltype = gltype;
				gl_state.pointer_color_stride = stride;
				gl_state.pointer_color_pointer = pointer;
				gl_state.pointer_color_vertexbuffer = vertexbuffer;
				gl_state.pointer_color_offset = bufferoffset;
				CHECKGLERROR
				GL_BindVBO(bufferobject);
				qglVertexAttribPointer(GLSLATTRIB_COLOR, components, gltype, false, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
			}
		}
		else
		{
			// caller wants color array disabled
			if (gl_state.pointer_color_enabled)
			{
				gl_state.pointer_color_enabled = false;
				CHECKGLERROR
				qglDisableVertexAttribArray(GLSLATTRIB_COLOR);CHECKGLERROR
				// when color array is on the glColor gets trashed, set it again
				qglVertexAttrib4f(GLSLATTRIB_COLOR, gl_state.color4f[0], gl_state.color4f[1], gl_state.color4f[2], gl_state.color4f[3]);CHECKGLERROR
			}
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	}
}

void R_Mesh_TexCoordPointer(unsigned int unitnum, int components, int gltype, size_t stride, const void *pointer, const r_meshbuffer_t *vertexbuffer, size_t bufferoffset)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	// update array settings
	// note: there is no need to check bufferobject here because all cases
	// that involve a valid bufferobject also supply a texcoord array
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		CHECKGLERROR
		if (pointer)
		{
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			// texture array unit is enabled, enable the array
			if (!unit->arrayenabled)
			{
				unit->arrayenabled = true;
				GL_ClientActiveTexture(unitnum);
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			// texcoord array
			if (unit->pointer_texcoord_components != components || unit->pointer_texcoord_gltype != gltype || unit->pointer_texcoord_stride != stride || unit->pointer_texcoord_pointer != pointer || unit->pointer_texcoord_vertexbuffer != vertexbuffer || unit->pointer_texcoord_offset != bufferoffset)
			{
				unit->pointer_texcoord_components = components;
				unit->pointer_texcoord_gltype = gltype;
				unit->pointer_texcoord_stride = stride;
				unit->pointer_texcoord_pointer = pointer;
				unit->pointer_texcoord_vertexbuffer = vertexbuffer;
				unit->pointer_texcoord_offset = bufferoffset;
				GL_ClientActiveTexture(unitnum);
				GL_BindVBO(bufferobject);
				qglTexCoordPointer(components, gltype, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
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
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		CHECKGLERROR
		if (pointer)
		{
			int bufferobject = vertexbuffer ? vertexbuffer->bufferobject : 0;
			// texture array unit is enabled, enable the array
			if (!unit->arrayenabled)
			{
				unit->arrayenabled = true;
				qglEnableVertexAttribArray(unitnum+GLSLATTRIB_TEXCOORD0);CHECKGLERROR
			}
			// texcoord array
			if (unit->pointer_texcoord_components != components || unit->pointer_texcoord_gltype != gltype || unit->pointer_texcoord_stride != stride || unit->pointer_texcoord_pointer != pointer || unit->pointer_texcoord_vertexbuffer != vertexbuffer || unit->pointer_texcoord_offset != bufferoffset)
			{
				unit->pointer_texcoord_components = components;
				unit->pointer_texcoord_gltype = gltype;
				unit->pointer_texcoord_stride = stride;
				unit->pointer_texcoord_pointer = pointer;
				unit->pointer_texcoord_vertexbuffer = vertexbuffer;
				unit->pointer_texcoord_offset = bufferoffset;
				GL_BindVBO(bufferobject);
				qglVertexAttribPointer(unitnum+GLSLATTRIB_TEXCOORD0, components, gltype, false, stride, bufferobject ? (void *)bufferoffset : pointer);CHECKGLERROR
			}
		}
		else
		{
			// texture array unit is disabled, disable the array
			if (unit->arrayenabled)
			{
				unit->arrayenabled = false;
				qglDisableVertexAttribArray(unitnum+GLSLATTRIB_TEXCOORD0);CHECKGLERROR
			}
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
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
	return 0;
}

void R_Mesh_CopyToTexture(rtexture_t *tex, int tx, int ty, int sx, int sy, int width, int height)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		R_Mesh_TexBind(0, tex);
		GL_ActiveTexture(0);CHECKGLERROR
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, tx, ty, sx, sy, width, height);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			IDirect3DSurface9 *currentsurface = NULL;
			IDirect3DSurface9 *texturesurface = NULL;
			RECT sourcerect;
			RECT destrect;
			sourcerect.left = sx;
			sourcerect.top = sy;
			sourcerect.right = sx + width;
			sourcerect.bottom = sy + height;
			destrect.left = tx;
			destrect.top = ty;
			destrect.right = tx + width;
			destrect.bottom = ty + height;
			if (!FAILED(IDirect3DTexture9_GetSurfaceLevel(((IDirect3DTexture9 *)tex->d3dtexture), 0, &texturesurface)))
			{
				if (!FAILED(IDirect3DDevice9_GetRenderTarget(vid_d3d9dev, 0, &currentsurface)))
				{
					IDirect3DDevice9_StretchRect(vid_d3d9dev, currentsurface, &sourcerect, texturesurface, &destrect, D3DTEXF_NONE);
					IDirect3DSurface9_Release(currentsurface);
				}
				IDirect3DSurface9_Release(texturesurface);
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_CopyRectangleToTexture(tex->texnum, 0, tx, ty, sx, sy, width, height);
		break;
	}
}

#ifdef SUPPORTD3D
int d3drswrap[16] = {D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15};
#endif

void R_Mesh_ClearBindingsForTexture(int texnum)
{
	gltextureunit_t *unit;
	unsigned int unitnum;
	// this doesn't really unbind the texture, but it does prevent a mistaken "do nothing" behavior on the next time this same texnum is bound on the same unit as the same type (this mainly affects r_shadow_bouncegrid because 3D textures are so rarely used)
	for (unitnum = 0;unitnum < vid.teximageunits;unitnum++)
	{
		unit = gl_state.units + unitnum;
		if (unit->t2d == texnum)
			unit->t2d = -1;
		if (unit->t3d == texnum)
			unit->t3d = -1;
		if (unit->tcubemap == texnum)
			unit->tcubemap = -1;
	}
}

void R_Mesh_TexBind(unsigned int unitnum, rtexture_t *tex)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	int tex2d, tex3d, texcubemap, texnum;
	if (unitnum >= vid.teximageunits)
		return;
//	if (unit->texture == tex)
//		return;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (!tex)
		{
			tex = r_texture_white;
			// not initialized enough yet...
			if (!tex)
				return;
		}
		unit->texture = tex;
		texnum = R_GetTexture(tex);
		switch(tex->gltexturetypeenum)
		{
		case GL_TEXTURE_2D: if (unit->t2d != texnum) {GL_ActiveTexture(unitnum);unit->t2d = texnum;qglBindTexture(GL_TEXTURE_2D, unit->t2d);CHECKGLERROR}break;
		case GL_TEXTURE_3D: if (unit->t3d != texnum) {GL_ActiveTexture(unitnum);unit->t3d = texnum;qglBindTexture(GL_TEXTURE_3D, unit->t3d);CHECKGLERROR}break;
		case GL_TEXTURE_CUBE_MAP_ARB: if (unit->tcubemap != texnum) {GL_ActiveTexture(unitnum);unit->tcubemap = texnum;qglBindTexture(GL_TEXTURE_CUBE_MAP_ARB, unit->tcubemap);CHECKGLERROR}break;
		}
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		unit->texture = tex;
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
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			extern cvar_t gl_texture_anisotropy;
			if (!tex)
			{
				tex = r_texture_white;
				// not initialized enough yet...
				if (!tex)
					return;
			}
			// upload texture if needed
			R_GetTexture(tex);
			if (unit->texture == tex)
				return;
			unit->texture = tex;
			IDirect3DDevice9_SetTexture(vid_d3d9dev, unitnum, (IDirect3DBaseTexture9*)tex->d3dtexture);
			//IDirect3DDevice9_SetRenderState(vid_d3d9dev, d3drswrap[unitnum], (tex->flags & TEXF_CLAMP) ? (D3DWRAPCOORD_0 | D3DWRAPCOORD_1 | D3DWRAPCOORD_2) : 0);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_ADDRESSU, tex->d3daddressu);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_ADDRESSV, tex->d3daddressv);
			if (tex->d3daddressw)
				IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_ADDRESSW,  tex->d3daddressw);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MAGFILTER, tex->d3dmagfilter);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MINFILTER, tex->d3dminfilter);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MIPFILTER, tex->d3dmipfilter);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MIPMAPLODBIAS, tex->d3dmipmaplodbias);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MAXMIPLEVEL, tex->d3dmaxmiplevelfilter);
			IDirect3DDevice9_SetSamplerState(vid_d3d9dev, unitnum, D3DSAMP_MAXANISOTROPY, gl_texture_anisotropy.integer);
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		if (!tex)
		{
			tex = r_texture_white;
			// not initialized enough yet...
			if (!tex)
				return;
		}
		texnum = R_GetTexture(tex);
		if (unit->texture == tex)
			return;
		unit->texture = tex;
		DPSOFTRAST_SetTexture(unitnum, texnum);
		break;
	}
}

void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
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
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		break;
	}
}

void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale)
{
	gltextureunit_t *unit = gl_state.units + unitnum;
	CHECKGLERROR
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		// do nothing
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		break;
	}
}

void R_Mesh_ResetTextureState(void)
{
	unsigned int unitnum;

	BACKENDACTIVECHECK

	for (unitnum = 0;unitnum < vid.teximageunits;unitnum++)
		R_Mesh_TexBind(unitnum, NULL);
	for (unitnum = 0;unitnum < vid.texarrayunits;unitnum++)
		R_Mesh_TexCoordPointer(unitnum, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		for (unitnum = 0;unitnum < vid.texunits;unitnum++)
		{
			R_Mesh_TexCombine(unitnum, GL_MODULATE, GL_MODULATE, 1, 1);
			R_Mesh_TexMatrix(unitnum, NULL);
		}
		break;
	}
}



#ifdef SUPPORTD3D
//#define r_vertex3f_d3d9fvf (D3DFVF_XYZ)
//#define r_vertexgeneric_d3d9fvf (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)
//#define r_vertexmesh_d3d9fvf (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5 | D3DFVF_TEXCOORDSIZE1(3) | D3DFVF_TEXCOORDSIZE2(3) | D3DFVF_TEXCOORDSIZE3(3))

D3DVERTEXELEMENT9 r_vertex3f_d3d9elements[] =
{
	{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	D3DDECL_END()
};

D3DVERTEXELEMENT9 r_vertexgeneric_d3d9elements[] =
{
	{0, (int)((size_t)&((r_vertexgeneric_t *)0)->vertex3f  ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, (int)((size_t)&((r_vertexgeneric_t *)0)->color4f   ), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	{0, (int)((size_t)&((r_vertexgeneric_t *)0)->texcoord2f), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};

D3DVERTEXELEMENT9 r_vertexmesh_d3d9elements[] =
{
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->vertex3f          ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->color4f           ), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->texcoordtexture2f ), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->svector3f         ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->tvector3f         ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->normal3f          ), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
	{0, (int)((size_t)&((r_vertexmesh_t *)0)->texcoordlightmap2f), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4},
	D3DDECL_END()
};

IDirect3DVertexDeclaration9 *r_vertex3f_d3d9decl;
IDirect3DVertexDeclaration9 *r_vertexgeneric_d3d9decl;
IDirect3DVertexDeclaration9 *r_vertexmesh_d3d9decl;
#endif

static void R_Mesh_InitVertexDeclarations(void)
{
#ifdef SUPPORTD3D
	r_vertex3f_d3d9decl = NULL;
	r_vertexgeneric_d3d9decl = NULL;
	r_vertexmesh_d3d9decl = NULL;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
		IDirect3DDevice9_CreateVertexDeclaration(vid_d3d9dev, r_vertex3f_d3d9elements, &r_vertex3f_d3d9decl);
		IDirect3DDevice9_CreateVertexDeclaration(vid_d3d9dev, r_vertexgeneric_d3d9elements, &r_vertexgeneric_d3d9decl);
		IDirect3DDevice9_CreateVertexDeclaration(vid_d3d9dev, r_vertexmesh_d3d9elements, &r_vertexmesh_d3d9decl);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}
#endif
}

static void R_Mesh_DestroyVertexDeclarations(void)
{
#ifdef SUPPORTD3D
	if (r_vertex3f_d3d9decl)
		IDirect3DVertexDeclaration9_Release(r_vertex3f_d3d9decl);
	r_vertex3f_d3d9decl = NULL;
	if (r_vertexgeneric_d3d9decl)
		IDirect3DVertexDeclaration9_Release(r_vertexgeneric_d3d9decl);
	r_vertexgeneric_d3d9decl = NULL;
	if (r_vertexmesh_d3d9decl)
		IDirect3DVertexDeclaration9_Release(r_vertexmesh_d3d9decl);
	r_vertexmesh_d3d9decl = NULL;
#endif
}

void R_Mesh_PrepareVertices_Vertex3f(int numvertices, const float *vertex3f, const r_meshbuffer_t *vertexbuffer)
{
	// upload temporary vertexbuffer for this rendering
	if (!gl_state.usevbo_staticvertex)
		vertexbuffer = NULL;
	if (!vertexbuffer && gl_state.usevbo_dynamicvertex)
	{
		if (gl_state.preparevertices_dynamicvertexbuffer)
			R_Mesh_UpdateMeshBuffer(gl_state.preparevertices_dynamicvertexbuffer, vertex3f, numvertices * sizeof(float[3]));
		else
			gl_state.preparevertices_dynamicvertexbuffer = R_Mesh_CreateMeshBuffer(vertex3f, numvertices * sizeof(float[3]), "temporary", false, true, false);
		vertexbuffer = gl_state.preparevertices_dynamicvertexbuffer;
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(3, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		else
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(3, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		else
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		break;
	case RENDERPATH_GL11:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		else
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, vertexbuffer, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_SetVertexDeclaration(vid_d3d9dev, r_vertex3f_d3d9decl);
		if (vertexbuffer)
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, (IDirect3DVertexBuffer9*)vertexbuffer->devicebuffer, 0, sizeof(float[3]));
		else
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, NULL, 0, 0);
		gl_state.d3dvertexbuffer = (void *)vertexbuffer;
		gl_state.d3dvertexdata = (void *)vertex3f;
		gl_state.d3dvertexsize = sizeof(float[3]);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetVertexPointer(vertex3f, sizeof(float[3]));
		DPSOFTRAST_SetColorPointer(NULL, 0);
		DPSOFTRAST_SetTexCoordPointer(0, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(1, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(2, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(3, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(4, 2, sizeof(float[2]), NULL);
		break;
	}
}



r_vertexgeneric_t *R_Mesh_PrepareVertices_Generic_Lock(int numvertices)
{
	size_t size;
	size = sizeof(r_vertexgeneric_t) * numvertices;
	if (gl_state.preparevertices_tempdatamaxsize < size)
	{
		gl_state.preparevertices_tempdatamaxsize = size;
		gl_state.preparevertices_tempdata = Mem_Realloc(r_main_mempool, gl_state.preparevertices_tempdata, gl_state.preparevertices_tempdatamaxsize);
	}
	gl_state.preparevertices_vertexgeneric = (r_vertexgeneric_t *)gl_state.preparevertices_tempdata;
	gl_state.preparevertices_numvertices = numvertices;
	return gl_state.preparevertices_vertexgeneric;
}

qboolean R_Mesh_PrepareVertices_Generic_Unlock(void)
{
	R_Mesh_PrepareVertices_Generic(gl_state.preparevertices_numvertices, gl_state.preparevertices_vertexgeneric, NULL);
	gl_state.preparevertices_vertexgeneric = NULL;
	gl_state.preparevertices_numvertices = 0;
	return true;
}

void R_Mesh_PrepareVertices_Generic_Arrays(int numvertices, const float *vertex3f, const float *color4f, const float *texcoord2f)
{
	int i;
	r_vertexgeneric_t *vertex;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (!vid.useinterleavedarrays)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, NULL, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), color4f, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), texcoord2f, NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(3, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			return;
		}
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (!vid.useinterleavedarrays)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, NULL, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), color4f, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), texcoord2f, NULL, 0);
			if (vid.texunits >= 2)
				R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			if (vid.texunits >= 3)
				R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			return;
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetVertexPointer(vertex3f, sizeof(float[3]));
		DPSOFTRAST_SetColorPointer(color4f, sizeof(float[4]));
		DPSOFTRAST_SetTexCoordPointer(0, 2, sizeof(float[2]), texcoord2f);
		DPSOFTRAST_SetTexCoordPointer(1, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(2, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(3, 2, sizeof(float[2]), NULL);
		DPSOFTRAST_SetTexCoordPointer(4, 2, sizeof(float[2]), NULL);
		return;
	}

	// no quick path for this case, convert to vertex structs
	vertex = R_Mesh_PrepareVertices_Generic_Lock(numvertices);
	for (i = 0;i < numvertices;i++)
		VectorCopy(vertex3f + 3*i, vertex[i].vertex3f);
	if (color4f)
	{
		for (i = 0;i < numvertices;i++)
			Vector4Copy(color4f + 4*i, vertex[i].color4f);
	}
	else
	{
		for (i = 0;i < numvertices;i++)
			Vector4Copy(gl_state.color4f, vertex[i].color4f);
	}
	if (texcoord2f)
		for (i = 0;i < numvertices;i++)
			Vector2Copy(texcoord2f + 2*i, vertex[i].texcoord2f);
	R_Mesh_PrepareVertices_Generic_Unlock();
	R_Mesh_PrepareVertices_Generic(numvertices, vertex, NULL);
}

void R_Mesh_PrepareVertices_Generic(int numvertices, const r_vertexgeneric_t *vertex, const r_meshbuffer_t *vertexbuffer)
{
	// upload temporary vertexbuffer for this rendering
	if (!gl_state.usevbo_staticvertex)
		vertexbuffer = NULL;
	if (!vertexbuffer && gl_state.usevbo_dynamicvertex)
	{
		if (gl_state.preparevertices_dynamicvertexbuffer)
			R_Mesh_UpdateMeshBuffer(gl_state.preparevertices_dynamicvertexbuffer, vertex, numvertices * sizeof(*vertex));
		else
			gl_state.preparevertices_dynamicvertexbuffer = R_Mesh_CreateMeshBuffer(vertex, numvertices * sizeof(*vertex), "temporary", false, true, false);
		vertexbuffer = gl_state.preparevertices_dynamicvertexbuffer;
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , vertexbuffer, (int)((unsigned char *)vertex->texcoord2f         - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(3, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(3, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , vertexbuffer, (int)((unsigned char *)vertex->texcoord2f         - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
		}
		break;
	case RENDERPATH_GL11:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , vertexbuffer, (int)((unsigned char *)vertex->texcoord2f         - (unsigned char *)vertex));
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoord2f        , NULL, 0);
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_SetVertexDeclaration(vid_d3d9dev, r_vertexgeneric_d3d9decl);
		if (vertexbuffer)
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, (IDirect3DVertexBuffer9*)vertexbuffer->devicebuffer, 0, sizeof(*vertex));
		else
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, NULL, 0, 0);
		gl_state.d3dvertexbuffer = (void *)vertexbuffer;
		gl_state.d3dvertexdata = (void *)vertex;
		gl_state.d3dvertexsize = sizeof(*vertex);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetVertexPointer(vertex->vertex3f, sizeof(*vertex));
		DPSOFTRAST_SetColorPointer(vertex->color4f, sizeof(*vertex));
		DPSOFTRAST_SetTexCoordPointer(0, 2, sizeof(*vertex), vertex->texcoord2f);
		DPSOFTRAST_SetTexCoordPointer(1, 2, sizeof(*vertex), NULL);
		DPSOFTRAST_SetTexCoordPointer(2, 2, sizeof(*vertex), NULL);
		DPSOFTRAST_SetTexCoordPointer(3, 2, sizeof(*vertex), NULL);
		DPSOFTRAST_SetTexCoordPointer(4, 2, sizeof(*vertex), NULL);
		break;
	}
}



r_vertexmesh_t *R_Mesh_PrepareVertices_Mesh_Lock(int numvertices)
{
	size_t size;
	size = sizeof(r_vertexmesh_t) * numvertices;
	if (gl_state.preparevertices_tempdatamaxsize < size)
	{
		gl_state.preparevertices_tempdatamaxsize = size;
		gl_state.preparevertices_tempdata = Mem_Realloc(r_main_mempool, gl_state.preparevertices_tempdata, gl_state.preparevertices_tempdatamaxsize);
	}
	gl_state.preparevertices_vertexmesh = (r_vertexmesh_t *)gl_state.preparevertices_tempdata;
	gl_state.preparevertices_numvertices = numvertices;
	return gl_state.preparevertices_vertexmesh;
}

qboolean R_Mesh_PrepareVertices_Mesh_Unlock(void)
{
	R_Mesh_PrepareVertices_Mesh(gl_state.preparevertices_numvertices, gl_state.preparevertices_vertexmesh, NULL);
	gl_state.preparevertices_vertexmesh = NULL;
	gl_state.preparevertices_numvertices = 0;
	return true;
}

void R_Mesh_PrepareVertices_Mesh_Arrays(int numvertices, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *color4f, const float *texcoordtexture2f, const float *texcoordlightmap2f)
{
	int i;
	r_vertexmesh_t *vertex;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (!vid.useinterleavedarrays)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, NULL, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), color4f, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), texcoordtexture2f, NULL, 0);
			R_Mesh_TexCoordPointer(1, 3, GL_FLOAT, sizeof(float[3]), svector3f, NULL, 0);
			R_Mesh_TexCoordPointer(2, 3, GL_FLOAT, sizeof(float[3]), tvector3f, NULL, 0);
			R_Mesh_TexCoordPointer(3, 3, GL_FLOAT, sizeof(float[3]), normal3f, NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), texcoordlightmap2f, NULL, 0);
			return;
		}
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (!vid.useinterleavedarrays)
		{
			R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), vertex3f, NULL, 0);
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), color4f, NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), texcoordtexture2f, NULL, 0);
			if (vid.texunits >= 2)
				R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), texcoordlightmap2f, NULL, 0);
			if (vid.texunits >= 3)
				R_Mesh_TexCoordPointer(2, 2, GL_FLOAT, sizeof(float[2]), NULL, NULL, 0);
			return;
		}
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetVertexPointer(vertex3f, sizeof(float[3]));
		DPSOFTRAST_SetColorPointer(color4f, sizeof(float[4]));
		DPSOFTRAST_SetTexCoordPointer(0, 2, sizeof(float[2]), texcoordtexture2f);
		DPSOFTRAST_SetTexCoordPointer(1, 3, sizeof(float[3]), svector3f);
		DPSOFTRAST_SetTexCoordPointer(2, 3, sizeof(float[3]), tvector3f);
		DPSOFTRAST_SetTexCoordPointer(3, 3, sizeof(float[3]), normal3f);
		DPSOFTRAST_SetTexCoordPointer(4, 2, sizeof(float[2]), texcoordlightmap2f);
		return;
	}

	vertex = R_Mesh_PrepareVertices_Mesh_Lock(numvertices);
	for (i = 0;i < numvertices;i++)
		VectorCopy(vertex3f + 3*i, vertex[i].vertex3f);
	if (svector3f)
		for (i = 0;i < numvertices;i++)
			VectorCopy(svector3f + 3*i, vertex[i].svector3f);
	if (tvector3f)
		for (i = 0;i < numvertices;i++)
			VectorCopy(tvector3f + 3*i, vertex[i].tvector3f);
	if (normal3f)
		for (i = 0;i < numvertices;i++)
			VectorCopy(normal3f + 3*i, vertex[i].normal3f);
	if (color4f)
	{
		for (i = 0;i < numvertices;i++)
			Vector4Copy(color4f + 4*i, vertex[i].color4f);
	}
	else
	{
		for (i = 0;i < numvertices;i++)
			Vector4Copy(gl_state.color4f, vertex[i].color4f);
	}
	if (texcoordtexture2f)
		for (i = 0;i < numvertices;i++)
			Vector2Copy(texcoordtexture2f + 2*i, vertex[i].texcoordtexture2f);
	if (texcoordlightmap2f)
		for (i = 0;i < numvertices;i++)
			Vector2Copy(texcoordlightmap2f + 2*i, vertex[i].texcoordlightmap2f);
	R_Mesh_PrepareVertices_Mesh_Unlock();
	R_Mesh_PrepareVertices_Mesh(numvertices, vertex, NULL);
}

void R_Mesh_PrepareVertices_Mesh(int numvertices, const r_vertexmesh_t *vertex, const r_meshbuffer_t *vertexbuffer)
{
	// upload temporary vertexbuffer for this rendering
	if (!gl_state.usevbo_staticvertex)
		vertexbuffer = NULL;
	if (!vertexbuffer && gl_state.usevbo_dynamicvertex)
	{
		if (gl_state.preparevertices_dynamicvertexbuffer)
			R_Mesh_UpdateMeshBuffer(gl_state.preparevertices_dynamicvertexbuffer, vertex, numvertices * sizeof(*vertex));
		else
			gl_state.preparevertices_dynamicvertexbuffer = R_Mesh_CreateMeshBuffer(vertex, numvertices * sizeof(*vertex), "temporary", false, true, false);
		vertexbuffer = gl_state.preparevertices_dynamicvertexbuffer;
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , vertexbuffer, (int)((unsigned char *)vertex->texcoordtexture2f  - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(1, 3, GL_FLOAT        , sizeof(*vertex), vertex->svector3f         , vertexbuffer, (int)((unsigned char *)vertex->svector3f          - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(2, 3, GL_FLOAT        , sizeof(*vertex), vertex->tvector3f         , vertexbuffer, (int)((unsigned char *)vertex->tvector3f          - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(3, 3, GL_FLOAT        , sizeof(*vertex), vertex->normal3f          , vertexbuffer, (int)((unsigned char *)vertex->normal3f           - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordlightmap2f, vertexbuffer, (int)((unsigned char *)vertex->texcoordlightmap2f - (unsigned char *)vertex));
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , NULL, 0);
			R_Mesh_TexCoordPointer(1, 3, GL_FLOAT        , sizeof(*vertex), vertex->svector3f         , NULL, 0);
			R_Mesh_TexCoordPointer(2, 3, GL_FLOAT        , sizeof(*vertex), vertex->tvector3f         , NULL, 0);
			R_Mesh_TexCoordPointer(3, 3, GL_FLOAT        , sizeof(*vertex), vertex->normal3f          , NULL, 0);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordlightmap2f, NULL, 0);
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , vertexbuffer, (int)((unsigned char *)vertex->texcoordtexture2f  - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordlightmap2f, vertexbuffer, (int)((unsigned char *)vertex->texcoordlightmap2f - (unsigned char *)vertex));
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , NULL, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordlightmap2f, NULL, 0);
		}
		break;
	case RENDERPATH_GL11:
		if (vertexbuffer)
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , vertexbuffer, (int)((unsigned char *)vertex->vertex3f           - (unsigned char *)vertex));
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , vertexbuffer, (int)((unsigned char *)vertex->color4f            - (unsigned char *)vertex));
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , vertexbuffer, (int)((unsigned char *)vertex->texcoordtexture2f  - (unsigned char *)vertex));
		}
		else
		{
			R_Mesh_VertexPointer(     3, GL_FLOAT        , sizeof(*vertex), vertex->vertex3f          , NULL, 0);
			R_Mesh_ColorPointer(      4, GL_FLOAT        , sizeof(*vertex), vertex->color4f           , NULL, 0);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT        , sizeof(*vertex), vertex->texcoordtexture2f , NULL, 0);
		}
		break;
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		IDirect3DDevice9_SetVertexDeclaration(vid_d3d9dev, r_vertexmesh_d3d9decl);
		if (vertexbuffer)
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, (IDirect3DVertexBuffer9*)vertexbuffer->devicebuffer, 0, sizeof(*vertex));
		else
			IDirect3DDevice9_SetStreamSource(vid_d3d9dev, 0, NULL, 0, 0);
		gl_state.d3dvertexbuffer = (void *)vertexbuffer;
		gl_state.d3dvertexdata = (void *)vertex;
		gl_state.d3dvertexsize = sizeof(*vertex);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		DPSOFTRAST_SetVertexPointer(vertex->vertex3f, sizeof(*vertex));
		DPSOFTRAST_SetColorPointer(vertex->color4f, sizeof(*vertex));
		DPSOFTRAST_SetTexCoordPointer(0, 2, sizeof(*vertex), vertex->texcoordtexture2f);
		DPSOFTRAST_SetTexCoordPointer(1, 3, sizeof(*vertex), vertex->svector3f);
		DPSOFTRAST_SetTexCoordPointer(2, 3, sizeof(*vertex), vertex->tvector3f);
		DPSOFTRAST_SetTexCoordPointer(3, 3, sizeof(*vertex), vertex->normal3f);
		DPSOFTRAST_SetTexCoordPointer(4, 2, sizeof(*vertex), vertex->texcoordlightmap2f);
		break;
	}
}

void GL_BlendEquationSubtract(qboolean negated)
{
	if(negated)
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			qglBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_BlendSubtract(true);
			break;
		}
	}
	else
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			qglBlendEquationEXT(GL_FUNC_ADD_EXT);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			IDirect3DDevice9_SetRenderState(vid_d3d9dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_BlendSubtract(false);
			break;
		}
	}
}
