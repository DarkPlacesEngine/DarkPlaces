
#include "quakedef.h"

cvar_t gl_mesh_maxtriangles = {0, "gl_mesh_maxtriangles", "1024"};
cvar_t gl_mesh_floatcolors = {0, "gl_mesh_floatcolors", "1"};
cvar_t gl_mesh_drawmode = {CVAR_SAVE, "gl_mesh_drawmode", "3"};

cvar_t r_render = {0, "r_render", "1"};
cvar_t gl_dither = {CVAR_SAVE, "gl_dither", "1"}; // whether or not to use dithering
cvar_t gl_lockarrays = {0, "gl_lockarrays", "1"};

// this is used to increase gl_mesh_maxtriangles automatically if a mesh was
// too large for the buffers in the previous frame
int overflowedverts = 0;

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

int c_meshs, c_meshtris;

int lightscalebit;
float lightscale;
float overbrightscale;

void SCR_ScreenShot_f (void);

// these are externally accessible
float mesh_colorscale;
int *varray_element;
float *varray_vertex;
float *varray_color;
float *varray_texcoord[MAX_TEXTUREUNITS];
int mesh_maxtris;
int mesh_maxverts; // always mesh_maxtris * 3

static matrix4x4_t backendmatrix;

static int backendunits, backendactive;
static qbyte *varray_bcolor;
static mempool_t *gl_backend_mempool;

void GL_Backend_AllocArrays(void)
{
	int i;

	if (!gl_backend_mempool)
		gl_backend_mempool = Mem_AllocPool("GL_Backend");

	mesh_maxverts = mesh_maxtris * 3;

	varray_element = Mem_Alloc(gl_backend_mempool, mesh_maxtris * sizeof(int[3]));
	varray_vertex = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[4]));
	varray_color = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[4]));
	varray_bcolor = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(qbyte[4]));
	for (i = 0;i < backendunits;i++)
		varray_texcoord[i] = Mem_Alloc(gl_backend_mempool, mesh_maxverts * sizeof(float[2]));
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
	varray_element = NULL;
	varray_vertex = NULL;
	varray_color = NULL;
	varray_bcolor = NULL;
	for (i = 0;i < MAX_TEXTUREUNITS;i++)
		varray_texcoord[i] = NULL;
}

static void gl_backend_start(void)
{
	Con_Printf("OpenGL Backend started with gl_mesh_maxtriangles %i\n", gl_mesh_maxtriangles.integer);
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
	if (gl_mesh_drawmode.integer < 0)
		Cvar_SetValueQuick(&gl_mesh_drawmode, 0);
	if (gl_mesh_drawmode.integer > 3)
		Cvar_SetValueQuick(&gl_mesh_drawmode, 3);

	// change drawmode 3 to 2 if 3 won't work
	if (gl_mesh_drawmode.integer >= 3 && qglDrawRangeElements == NULL)
		Cvar_SetValueQuick(&gl_mesh_drawmode, 2);

	// 21760 is (65536 / 3) rounded off to a multiple of 128
	if (gl_mesh_maxtriangles.integer < 1024)
		Cvar_SetValueQuick(&gl_mesh_maxtriangles, 1024);
	if (gl_mesh_maxtriangles.integer > 21760)
		Cvar_SetValueQuick(&gl_mesh_maxtriangles, 21760);
}

void GL_Backend_ResizeArrays(int numtriangles)
{
	Cvar_SetValueQuick(&gl_mesh_maxtriangles, numtriangles);
	GL_Backend_CheckCvars();
	mesh_maxtris = gl_mesh_maxtriangles.integer;
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

	Cvar_RegisterVariable(&gl_mesh_maxtriangles);
	Cvar_RegisterVariable(&gl_mesh_floatcolors);
	Cvar_RegisterVariable(&gl_mesh_drawmode);
	GL_Backend_CheckCvars();
	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
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
	if (zFar < 64)
		zFar = 64;

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
	int depthdisable;
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
				qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), varray_texcoord[i]);CHECKGLERROR
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
			qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), varray_texcoord[0]);CHECKGLERROR
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

int usedarrays;
void GL_Backend_ResetState(void)
{
	int i;
	gl_state.unit = 0;
	gl_state.clientunit = 0;

	for (i = 0;i < backendunits;i++)
	{
		gl_state.texture[i] = 0;
		gl_state.texturergbscale[i] = 1;
	}

	qglEnable(GL_CULL_FACE);CHECKGLERROR
	qglCullFace(GL_FRONT);CHECKGLERROR

	gl_state.depthdisable = false;
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
		qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
	}

	GL_SetupTextureState();
}

// called at beginning of frame
void R_Mesh_Start(float farclip)
{
	if (!backendactive)
		Sys_Error("R_Mesh_Clear: called when backend is not active\n");

	CHECKGLERROR

	r_mesh_farclip = farclip;
	viewdist = DotProduct(r_origin, vpn);
	vpnbit0 = vpn[0] < 0;
	vpnbit1 = vpn[1] < 0;
	vpnbit2 = vpn[2] < 0;

	c_meshs = 0;
	c_meshtris = 0;

	GL_Backend_CheckCvars();
	if (mesh_maxtris != gl_mesh_maxtriangles.integer)
		GL_Backend_ResizeArrays(gl_mesh_maxtriangles.integer);

	GL_SetupFrame();

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

void GL_TransformVertices(int numverts)
{
	int i;
	float m[12], tempv[4], *v;
	m[0] = backendmatrix.m[0][0];
	m[1] = backendmatrix.m[0][1];
	m[2] = backendmatrix.m[0][2];
	m[3] = backendmatrix.m[0][3];
	m[4] = backendmatrix.m[1][0];
	m[5] = backendmatrix.m[1][1];
	m[6] = backendmatrix.m[1][2];
	m[7] = backendmatrix.m[1][3];
	m[8] = backendmatrix.m[2][0];
	m[9] = backendmatrix.m[2][1];
	m[10] = backendmatrix.m[2][2];
	m[11] = backendmatrix.m[2][3];
	for (i = 0, v = varray_vertex;i < numverts;i++, v += 4)
	{
		VectorCopy(v, tempv);
		v[0] = tempv[0] * m[0] + tempv[1] * m[1] + tempv[2] * m[2] + m[3];
		v[1] = tempv[0] * m[4] + tempv[1] * m[5] + tempv[2] * m[6] + m[7];
		v[2] = tempv[0] * m[8] + tempv[1] * m[9] + tempv[2] * m[10] + m[11];
	}
}

void GL_DrawRangeElements(int firstvert, int endvert, int indexcount, GLuint *index)
{
	unsigned int i, j, in;
	qbyte *c;
	float *v;
	GL_LockArray(firstvert, endvert - firstvert);
	if (gl_mesh_drawmode.integer >= 3/* && (endvert - firstvert) <= gl_maxdrawrangeelementsvertices && (indexcount) <= gl_maxdrawrangeelementsindices*/)
	{
		// GL 1.2 or GL 1.1 with extension
		qglDrawRangeElements(GL_TRIANGLES, firstvert, endvert, indexcount, GL_UNSIGNED_INT, index);
		CHECKGLERROR
	}
	else if (gl_mesh_drawmode.integer >= 2)
	{
		// GL 1.1
		qglDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, index);
		CHECKGLERROR
	}
	else if (gl_mesh_drawmode.integer >= 1)
	{
		// GL 1.1
		// feed it manually using glArrayElement
		qglBegin(GL_TRIANGLES);
		for (i = 0;i < indexcount;i++)
			qglArrayElement(index[i]);
		qglEnd();
		CHECKGLERROR
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
				c = varray_bcolor + in * 4;
				qglColor4ub(c[0], c[1], c[2], c[3]);
				for (j = 0;j < backendunits;j++)
				{
					if (gl_state.texture[j])
					{
						v = varray_texcoord[j] + in * 2;
						qglMultiTexCoord2f(GL_TEXTURE0_ARB + j, v[0], v[1]);
					}
				}
				v = varray_vertex + in * 4;
				qglVertex3f(v[0], v[1], v[2]);
			}
		}
		else
		{
			for (i = 0;i < indexcount;i++)
			{
				in = index[i];
				c = varray_bcolor + in * 4;
				qglColor4ub(c[0], c[1], c[2], c[3]);
				if (gl_state.texture[0])
				{
					v = varray_texcoord[j] + in * 2;
					qglTexCoord2f(v[0], v[1]);
				}
				v = varray_vertex + in * 4;
				qglVertex3f(v[0], v[1], v[2]);
			}
		}
		qglEnd();
		CHECKGLERROR
	}
	GL_UnlockArray();
}

// enlarges geometry buffers if they are too small
void _R_Mesh_ResizeCheck(int numverts, int numtriangles)
{
	if (numtriangles > mesh_maxtris || numverts > mesh_maxverts)
	{
		if (!backendactive)
			Sys_Error("R_Mesh_Begin: called when backend is not active\n");
		GL_Backend_ResizeArrays(max(numtriangles, (numverts + 2) / 3) + 100);
		GL_Backend_ResetState();
	}
}

// renders the mesh
void R_Mesh_Draw(int numverts, int numtriangles)
{
	if (!backendactive)
		Sys_Error("R_Mesh_End: called when backend is not active\n");

	c_meshs++;
	c_meshtris += numtriangles;

	CHECKGLERROR

	// drawmode 0 always uses byte colors
	if (!gl_mesh_floatcolors.integer || gl_mesh_drawmode.integer <= 0)
		GL_ConvertColorsFloatToByte(numverts);
	GL_TransformVertices(numverts);
	if (!r_render.integer)
		return;
	GL_DrawRangeElements(0, numverts, numtriangles * 3, varray_element);
}

// restores backend state, used when done with 3D rendering
void R_Mesh_Finish(void)
{
	int i;
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
	R_Mesh_Finish();
	qglClear(GL_DEPTH_BUFFER_BIT);
	R_Mesh_Start(r_mesh_farclip);
}

// sets up the requested state
void R_Mesh_State(const rmeshstate_t *m)
{
	int i, overbright;
	int texturergbscale[MAX_TEXTUREUNITS];
	float scaler;

	if (!backendactive)
		Sys_Error("R_Mesh_State: called when backend is not active\n");

	if (gl_backend_rebindtextures)
	{
		gl_backend_rebindtextures = false;
		GL_SetupTextureState();
	}

	backendmatrix = m->matrix; // this copies the struct

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
			overbright = m->wantoverbright && gl_combine.integer;
			if (overbright)
				scaler *= 0.25f;
		}
		scaler *= overbrightscale;
	}
	mesh_colorscale = scaler;

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

	for (i = 0;i < backendunits;i++)
	{
		if (m->texrgbscale[i])
			texturergbscale[i] = m->texrgbscale[i];
		else
			texturergbscale[i] = 1;
	}
	if (overbright)
		for (i = backendunits - 1;i >= 0;i--)
			if (m->tex[i])
				texturergbscale[i] = 4;

	if (backendunits > 1)
	{
		for (i = 0;i < backendunits;i++)
		{
			if (gl_state.texture[i] != m->tex[i])
			{
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				if (gl_state.texture[i] == 0)
				{
					qglEnable(GL_TEXTURE_2D);CHECKGLERROR
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
				qglBindTexture(GL_TEXTURE_2D, (gl_state.texture[i] = m->tex[i]));CHECKGLERROR
				if (gl_state.texture[i] == 0)
				{
					qglDisable(GL_TEXTURE_2D);CHECKGLERROR
					if (gl_state.clientunit != i)
					{
						qglClientActiveTexture(GL_TEXTURE0_ARB + (gl_state.clientunit = i));CHECKGLERROR
					}
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				}
			}
			if (gl_state.texturergbscale[i] != texturergbscale[i])
			{
				if (gl_state.unit != i)
				{
					qglActiveTexture(GL_TEXTURE0_ARB + (gl_state.unit = i));CHECKGLERROR
				}
				qglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (gl_state.texturergbscale[i] = texturergbscale[i]));CHECKGLERROR
			}
		}
	}
	else
	{
		if (gl_state.texture[0] != m->tex[0])
		{
			if (gl_state.texture[0] == 0)
			{
				qglEnable(GL_TEXTURE_2D);CHECKGLERROR
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
			qglBindTexture(GL_TEXTURE_2D, (gl_state.texture[0] = m->tex[0]));CHECKGLERROR
			if (gl_state.texture[0] == 0)
			{
				qglDisable(GL_TEXTURE_2D);CHECKGLERROR
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			}
		}
	}
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

	if (scr_conlines < vid.conheight && cls.signon == SIGNONS)
		R_RenderView();

	// draw 2D stuff
	R_DrawQueue();

	// tell driver to commit it's partially full geometry queue to the rendering queue
	// (this doesn't wait for the commands themselves to complete)
	qglFlush();
}

