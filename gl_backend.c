
#include "quakedef.h"

cvar_t		r_render = {0, "r_render", "1"};
cvar_t		gl_dither = {CVAR_SAVE, "gl_dither", "1"}; // whether or not to use dithering

int			lightscalebit;
float		lightscale;
float		overbrightscale;

void SCR_ScreenShot_f (void);
static void R_Envmap_f (void);

static int max_meshs;
static int max_batch;
static int max_verts; // always max_meshs * 3
#define TRANSDEPTHRES 4096

//static cvar_t gl_mesh_maxtriangles = {0, "gl_mesh_maxtriangles", "21760"};
static cvar_t gl_mesh_maxtriangles = {0, "gl_mesh_maxtriangles", "8192"};
static cvar_t gl_mesh_batchtriangles = {0, "gl_mesh_batchtriangles", "1024"};
static cvar_t gl_mesh_merge = {0, "gl_mesh_merge", "1"};
static cvar_t gl_mesh_floatcolors = {0, "gl_mesh_floatcolors", "0"};

typedef struct buf_mesh_s
{
	//struct buf_mesh_s *next;
	int depthmask;
	int depthtest;
	int blendfunc1, blendfunc2;
	int textures[MAX_TEXTUREUNITS];
	float texturergbscale[MAX_TEXTUREUNITS];
	int firsttriangle;
	int triangles;
	int firstvert;
	int lastvert;
	struct buf_mesh_s *chain;
	struct buf_transtri_s *transchain;
	//struct buf_transtri_s **transchainpointer;
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
	byte c[4];
}
buf_bcolor_t;

typedef struct
{
	float t[2];
}
buf_texcoord_t;

static float meshfarclip;
static int currentmesh, currenttriangle, currentvertex, backendunits, backendactive, meshmerge, transranout;
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

	max_verts = max_meshs * 3;

	if (!gl_backend_mempool)
		gl_backend_mempool = Mem_AllocPool("GL_Backend");

#define BACKENDALLOC(var, count, sizeofstruct)\
	{\
		var = Mem_Alloc(gl_backend_mempool, count * sizeof(sizeofstruct));\
		if (var == NULL)\
			Sys_Error("gl_backend_start: unable to allocate memory\n");\
		memset(var, 0, count * sizeof(sizeofstruct));\
	}

	BACKENDALLOC(buf_mesh, max_meshs, buf_mesh_t)
	BACKENDALLOC(buf_tri, max_meshs, buf_tri_t)
	BACKENDALLOC(buf_vertex, max_verts, buf_vertex_t)
	BACKENDALLOC(buf_fcolor, max_verts, buf_fcolor_t)
	BACKENDALLOC(buf_bcolor, max_verts, buf_bcolor_t)

	BACKENDALLOC(buf_transmesh, max_meshs, buf_mesh_t)
	BACKENDALLOC(buf_sorttranstri, max_meshs, buf_transtri_t)
	BACKENDALLOC(buf_sorttranstri_list, TRANSDEPTHRES, buf_transtri_t *)
	BACKENDALLOC(buf_transtri, max_meshs, buf_tri_t)
	BACKENDALLOC(buf_transvertex, max_verts, buf_vertex_t)
	BACKENDALLOC(buf_transfcolor, max_verts, buf_fcolor_t)

	for (i = 0;i < MAX_TEXTUREUNITS;i++)
	{
		// only allocate as many texcoord arrays as we need
		if (i < gl_textureunits)
		{
			BACKENDALLOC(buf_texcoord[i], max_verts, buf_texcoord_t)
			BACKENDALLOC(buf_transtexcoord[i], max_verts, buf_texcoord_t)
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
	if (resizingbuffers)
		Mem_EmptyPool(gl_backend_mempool);
	else
		Mem_FreePool(&gl_backend_mempool);

	backendunits = 0;
	backendactive = false;
}

static void gl_backend_bufferchanges(int init)
{
	// 21760 is (65536 / 3) rounded off to a multiple of 128
	if (gl_mesh_maxtriangles.integer < 256)
		Cvar_SetValue("gl_mesh_maxtriangles", 256);
	if (gl_mesh_maxtriangles.integer > 21760)
		Cvar_SetValue("gl_mesh_maxtriangles", 21760);

	if (gl_mesh_batchtriangles.integer < 0)
		Cvar_SetValue("gl_mesh_batchtriangles", 0);
	if (gl_mesh_batchtriangles.integer > gl_mesh_maxtriangles.integer)
		Cvar_SetValue("gl_mesh_batchtriangles", gl_mesh_maxtriangles.integer);

	max_batch = gl_mesh_batchtriangles.integer;

	if (max_meshs != gl_mesh_maxtriangles.integer)
	{
		max_meshs = gl_mesh_maxtriangles.integer;

		if (!init)
		{
			resizingbuffers = true;
			gl_backend_shutdown();
			gl_backend_start();
			resizingbuffers = false;
		}
	}
}

float r_farclip, r_newfarclip;

static void gl_backend_newmap(void)
{
	r_farclip = r_newfarclip = 2048.0f;
}

int polyindexarray[768];

void gl_backend_init(void)
{
	int i;

	Cvar_RegisterVariable (&r_render);
	Cvar_RegisterVariable (&gl_dither);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);

	Cvar_RegisterVariable(&gl_mesh_maxtriangles);
	Cvar_RegisterVariable(&gl_mesh_batchtriangles);
	Cvar_RegisterVariable(&gl_mesh_merge);
	Cvar_RegisterVariable(&gl_mesh_floatcolors);
	R_RegisterModule("GL_Backend", gl_backend_start, gl_backend_shutdown, gl_backend_newmap);
	gl_backend_bufferchanges(true);
	for (i = 0;i < 256;i++)
	{
		polyindexarray[i*3+0] = 0;
		polyindexarray[i*3+1] = i + 1;
		polyindexarray[i*3+2] = i + 2;
	}
}

static void MYgluPerspective(GLdouble fovx, GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmax, ymax;

	xmax = zNear * tan( fovx * M_PI / 360.0 ) * aspect;
	ymax = zNear * tan( fovy * M_PI / 360.0 );

	glFrustum(-xmax, xmax, -ymax, ymax, zNear, zFar );
}


/*
=============
GL_SetupFrame
=============
*/
static void GL_SetupFrame (void)
{
	if (!r_render.integer)
		return;

//	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: moved to SCR_UpdateScreen
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);

	// update farclip based on previous frame
	r_farclip = r_newfarclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();

	// y is weird beause OpenGL is bottom to top, we use top to bottom
	glViewport(r_refdef.x, vid.realheight - (r_refdef.y + r_refdef.height), r_refdef.width, r_refdef.height);
//	yfov = 2*atan((float)r_refdef.height/r_refdef.width)*180/M_PI;
	MYgluPerspective (r_refdef.fov_x, r_refdef.fov_y, r_refdef.width/r_refdef.height, 4, r_farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
	glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
	glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

//	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
//	if (gl_cull.integer)
		glEnable(GL_CULL_FACE);
//	else
//		glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND); // was Disable
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
}

static float viewdist;

int c_meshs, c_meshtris, c_transmeshs, c_transtris;

// called at beginning of frame
void R_Mesh_Clear(void)
{
	if (!backendactive)
		Sys_Error("R_Mesh_Clear: called when backend is not active\n");

	gl_backend_bufferchanges(false);

	currentmesh = 0;
	currenttriangle = 0;
	currentvertex = 0;
	currenttransmesh = 0;
	currenttranstriangle = 0;
	currenttransvertex = 0;
	meshfarclip = 0;
	meshmerge = gl_mesh_merge.integer;
	transranout = false;
	viewdist = DotProduct(r_origin, vpn);

	c_meshs = 0;
	c_meshtris = 0;
	c_transmeshs = 0;
	c_transtris = 0;

	GL_SetupFrame();
}

#ifdef DEBUGGL
void GL_PrintError(int errornumber, char *filename, int linenumber)
{
	switch(errornumber)
	{
	case GL_INVALID_ENUM:
		Con_Printf("GL_INVALID_ENUM at %s:%i\n", filename, linenumber);
		break;
	case GL_INVALID_VALUE:
		Con_Printf("GL_INVALID_VALUE at %s:%i\n", filename, linenumber);
		break;
	case GL_INVALID_OPERATION:
		Con_Printf("GL_INVALID_OPERATION at %s:%i\n", filename, linenumber);
		break;
	case GL_STACK_OVERFLOW:
		Con_Printf("GL_STACK_OVERFLOW at %s:%i\n", filename, linenumber);
		break;
	case GL_STACK_UNDERFLOW:
		Con_Printf("GL_STACK_UNDERFLOW at %s:%i\n", filename, linenumber);
		break;
	case GL_OUT_OF_MEMORY:
		Con_Printf("GL_OUT_OF_MEMORY at %s:%i\n", filename, linenumber);
		break;
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

int errornumber = 0;
#endif

// renders mesh buffers, called to flush buffers when full
void R_Mesh_Render(void)
{
	int i, k, blendfunc1, blendfunc2, blend, depthmask, depthtest, unit = 0, clientunit = 0, firsttriangle, triangles, firstvert, lastvert, texture[MAX_TEXTUREUNITS];
	float farclip, texturergbscale[MAX_TEXTUREUNITS];
	buf_mesh_t *mesh;
	unsigned int *index;
	// float to byte color conversion
	int *icolor;
	float *fcolor;
	byte *bcolor;
	if (!backendactive)
		Sys_Error("R_Mesh_Render: called when backend is not active\n");
	if (!currentmesh)
		return;

CHECKGLERROR

	// push out farclip based on vertices
	for (i = 0;i < currentvertex;i++)
	{
		farclip = DotProduct(buf_vertex[i].v, vpn);
		if (meshfarclip < farclip)
			meshfarclip = farclip;
	}

	farclip = meshfarclip + 256.0f - viewdist; // + 256 just to be safe

	// push out farclip for next frame
	if (farclip > r_newfarclip)
		r_newfarclip = ceil((farclip + 255) / 256) * 256 + 256;

	for (i = 0;i < backendunits;i++)
		texturergbscale[i] = 1;

	glEnable(GL_CULL_FACE);
CHECKGLERROR
	glCullFace(GL_FRONT);
CHECKGLERROR
	depthtest = true;
	glEnable(GL_DEPTH_TEST);
CHECKGLERROR
	blendfunc1 = GL_ONE;
	blendfunc2 = GL_ZERO;
	glBlendFunc(blendfunc1, blendfunc2);
CHECKGLERROR
	blend = 0;
	glDisable(GL_BLEND);
CHECKGLERROR
	depthmask = true;
	glDepthMask((GLboolean) depthmask);
CHECKGLERROR

	glVertexPointer(3, GL_FLOAT, sizeof(buf_vertex_t), buf_vertex);
CHECKGLERROR
	glEnableClientState(GL_VERTEX_ARRAY);
CHECKGLERROR
	if (gl_mesh_floatcolors.integer)
	{
		glColorPointer(4, GL_FLOAT, sizeof(buf_fcolor_t), buf_fcolor);
CHECKGLERROR
	}
	else
	{
		// shift float to have 8bit fraction at base of number
		for (i = 0, fcolor = &buf_fcolor->c[0];i < currentvertex;i++)
		{
			*fcolor++ += 32768.0f;
			*fcolor++ += 32768.0f;
			*fcolor++ += 32768.0f;
			*fcolor++ += 32768.0f;
		}
		// then read as integer and kill float bits...
		for (i = 0, icolor = (int *)&buf_fcolor->c[0], bcolor = &buf_bcolor->c[0];i < currentvertex;i++)
		{
			k = (*icolor++) & 0x7FFFFF;*bcolor++ = k > 255 ? 255 : k;
			k = (*icolor++) & 0x7FFFFF;*bcolor++ = k > 255 ? 255 : k;
			k = (*icolor++) & 0x7FFFFF;*bcolor++ = k > 255 ? 255 : k;
			k = (*icolor++) & 0x7FFFFF;*bcolor++ = k > 255 ? 255 : k;
		}
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(buf_bcolor_t), buf_bcolor);
CHECKGLERROR
	}
	glEnableClientState(GL_COLOR_ARRAY);
CHECKGLERROR

	if (backendunits > 1)
	{
		for (i = 0;i < backendunits;i++)
		{
			qglActiveTexture(GL_TEXTURE0_ARB + (unit = i));
CHECKGLERROR
			glBindTexture(GL_TEXTURE_2D, (texture[i] = 0));
CHECKGLERROR
			glDisable(GL_TEXTURE_2D);
CHECKGLERROR
			if (gl_combine.integer)
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_CONSTANT_ARB);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_CONSTANT_ARB);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
CHECKGLERROR
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);
CHECKGLERROR
				glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
CHECKGLERROR
				glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);
CHECKGLERROR
			}
			else
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
CHECKGLERROR
			}

			qglClientActiveTexture(GL_TEXTURE0_ARB + (clientunit = i));
CHECKGLERROR
			glTexCoordPointer(2, GL_FLOAT, sizeof(buf_texcoord_t), buf_texcoord[i]);
CHECKGLERROR
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
		}
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, (texture[0] = 0));
CHECKGLERROR
		glDisable(GL_TEXTURE_2D);
CHECKGLERROR
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
CHECKGLERROR

		glTexCoordPointer(2, GL_FLOAT, sizeof(buf_texcoord_t), buf_texcoord[0]);
CHECKGLERROR
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
	}

	// lock as early as possible
	GL_LockArray(0, currentvertex);
CHECKGLERROR

	for (k = 0;k < currentmesh;)
	{
		mesh = &buf_mesh[k];

		if (backendunits > 1)
		{
//			int topunit = 0;
			for (i = 0;i < backendunits;i++)
			{
				if (texture[i] != mesh->textures[i])
				{
					if (unit != i)
					{
						qglActiveTexture(GL_TEXTURE0_ARB + (unit = i));
CHECKGLERROR
					}
					if (texture[i] == 0)
					{
						glEnable(GL_TEXTURE_2D);
CHECKGLERROR
						// have to disable texcoord array on disabled texture
						// units due to NVIDIA driver bug with
						// compiled_vertex_array
						if (clientunit != i)
						{
							qglClientActiveTexture(GL_TEXTURE0_ARB + (clientunit = i));
CHECKGLERROR
						}
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
					}
					glBindTexture(GL_TEXTURE_2D, (texture[i] = mesh->textures[i]));
CHECKGLERROR
					if (texture[i] == 0)
					{
						glDisable(GL_TEXTURE_2D);
CHECKGLERROR
						// have to disable texcoord array on disabled texture
						// units due to NVIDIA driver bug with
						// compiled_vertex_array
						if (clientunit != i)
						{
							qglClientActiveTexture(GL_TEXTURE0_ARB + (clientunit = i));
CHECKGLERROR
						}
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
					}
				}
				if (texturergbscale[i] != mesh->texturergbscale[i])
				{
					if (unit != i)
					{
						qglActiveTexture(GL_TEXTURE0_ARB + (unit = i));
CHECKGLERROR
					}
					glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, (texturergbscale[i] = mesh->texturergbscale[i]));
CHECKGLERROR
				}
//				if (texture[i])
//					topunit = i;
			}
//			if (unit != topunit)
//			{
//				qglActiveTexture(GL_TEXTURE0_ARB + (unit = topunit));
//CHECKGLERROR
//			}
		}
		else
		{
			if (texture[0] != mesh->textures[0])
			{
				if (texture[0] == 0)
				{
					glEnable(GL_TEXTURE_2D);
CHECKGLERROR
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
				}
				glBindTexture(GL_TEXTURE_2D, (texture[0] = mesh->textures[0]));
CHECKGLERROR
				if (texture[0] == 0)
				{
					glDisable(GL_TEXTURE_2D);
CHECKGLERROR
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
				}
			}
		}
		if (blendfunc1 != mesh->blendfunc1 || blendfunc2 != mesh->blendfunc2)
		{
			blendfunc1 = mesh->blendfunc1;
			blendfunc2 = mesh->blendfunc2;
			glBlendFunc(blendfunc1, blendfunc2);
CHECKGLERROR
			if (blendfunc2 == GL_ZERO)
			{
				if (blendfunc1 == GL_ONE)
				{
					if (blend)
					{
						blend = 0;
						glDisable(GL_BLEND);
CHECKGLERROR
					}
				}
				else
				{
					if (!blend)
					{
						blend = 1;
						glEnable(GL_BLEND);
CHECKGLERROR
					}
				}
			}
			else
			{
				if (!blend)
				{
					blend = 1;
					glEnable(GL_BLEND);
CHECKGLERROR
				}
			}
		}
		if (depthtest != mesh->depthtest)
		{
			depthtest = mesh->depthtest;
			if (depthtest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}
		if (depthmask != mesh->depthmask)
		{
			depthmask = mesh->depthmask;
			glDepthMask((GLboolean) depthmask);
CHECKGLERROR
		}

		firsttriangle = mesh->firsttriangle;
		triangles = mesh->triangles;
		firstvert = mesh->firstvert;
		lastvert = mesh->lastvert;
		mesh = &buf_mesh[++k];

		if (meshmerge)
		{
			#if MAX_TEXTUREUNITS != 4
			#error update this code
			#endif
			while (k < currentmesh
				&& mesh->blendfunc1 == blendfunc1
				&& mesh->blendfunc2 == blendfunc2
				&& mesh->depthtest == depthtest
				&& mesh->depthmask == depthmask
				&& mesh->textures[0] == texture[0]
				&& mesh->textures[1] == texture[1]
				&& mesh->textures[2] == texture[2]
				&& mesh->textures[3] == texture[3]
				&& mesh->texturergbscale[0] == texturergbscale[0]
				&& mesh->texturergbscale[1] == texturergbscale[1]
				&& mesh->texturergbscale[2] == texturergbscale[2]
				&& mesh->texturergbscale[3] == texturergbscale[3])
			{
				triangles += mesh->triangles;
				if (firstvert > mesh->firstvert)
					firstvert = mesh->firstvert;
				if (lastvert < mesh->lastvert)
					lastvert = mesh->lastvert;
				mesh = &buf_mesh[++k];
			}
		}

		index = (unsigned int *)&buf_tri[firsttriangle].index[0];
		for (i = 0;i < triangles * 3;i++)
			index[i] += firstvert;

#ifdef WIN32
		// FIXME: dynamic link to GL so we can get DrawRangeElements on WIN32
		glDrawElements(GL_TRIANGLES, triangles * 3, GL_UNSIGNED_INT, index);
#else
		glDrawRangeElements(GL_TRIANGLES, firstvert, lastvert + 1, triangles * 3, GL_UNSIGNED_INT, index);
#endif
CHECKGLERROR
	}

	currentmesh = 0;
	currenttriangle = 0;
	currentvertex = 0;

	GL_UnlockArray();
CHECKGLERROR

	if (backendunits > 1)
	{
		for (i = backendunits - 1;i >= 0;i--)
		{
			qglActiveTexture(GL_TEXTURE0_ARB + (unit = i));
CHECKGLERROR
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
CHECKGLERROR
			if (gl_combine.integer)
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
CHECKGLERROR
			}
			if (i > 0)
			{
				glDisable(GL_TEXTURE_2D);
CHECKGLERROR
			}
			else
			{
				glEnable(GL_TEXTURE_2D);
CHECKGLERROR
			}
			glBindTexture(GL_TEXTURE_2D, 0);
CHECKGLERROR

			qglClientActiveTexture(GL_TEXTURE0_ARB + (clientunit = i));
CHECKGLERROR
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
		}
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
CHECKGLERROR
		glEnable(GL_TEXTURE_2D);
CHECKGLERROR
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
CHECKGLERROR
	}
	glDisableClientState(GL_COLOR_ARRAY);
CHECKGLERROR
	glDisableClientState(GL_VERTEX_ARRAY);
CHECKGLERROR

	glDisable(GL_BLEND);
CHECKGLERROR
	glEnable(GL_DEPTH_TEST);
CHECKGLERROR
	glDepthMask(true);
CHECKGLERROR
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
CHECKGLERROR
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
	centerscaler = (TRANSDEPTHRES / r_farclip) * (1.0f / 3.0f);
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
		i = *((long *)&center) & 0x7FFFFF;
		i = min(i, (TRANSDEPTHRES - 1));
#endif
		tri->next = buf_sorttranstri_list[i];
		buf_sorttranstri_list[i] = tri;
		k++;
	}

	//if (currentmesh + k > max_meshs || currenttriangle + k > max_batch || currentvertex + currenttransvertex > max_verts)
	//	R_Mesh_Render();

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

	for (;transmesh;transmesh = transmesh->chain)
	{
		int numverts = transmesh->lastvert - transmesh->firstvert + 1;
		if (currentmesh >= max_meshs || currenttriangle + transmesh->triangles > max_batch || currentvertex + numverts > max_verts)
			R_Mesh_Render();

		memcpy(&buf_vertex[currentvertex], &buf_transvertex[transmesh->firstvert], numverts * sizeof(buf_vertex_t));
		memcpy(&buf_fcolor[currentvertex], &buf_transfcolor[transmesh->firstvert], numverts * sizeof(buf_fcolor_t));
		for (i = 0;i < backendunits && transmesh->textures[i];i++)
			memcpy(&buf_texcoord[i][currentvertex], &buf_transtexcoord[i][transmesh->firstvert], numverts * sizeof(buf_texcoord_t));

		mesh = &buf_mesh[currentmesh++];
		*mesh = *transmesh; // copy mesh properties
		mesh->firstvert = currentvertex;
		mesh->lastvert = currentvertex + numverts - 1;
		currentvertex += numverts;
		mesh->firsttriangle = currenttriangle;
		for (tri = transmesh->transchain;tri;tri = tri->meshsortchain)
		{
			buf_tri[currenttriangle].index[0] = tri->index[0];
			buf_tri[currenttriangle].index[1] = tri->index[1];
			buf_tri[currenttriangle].index[2] = tri->index[2];
			currenttriangle++;
		}
		mesh->triangles = currenttriangle - mesh->firsttriangle;
	}

	currenttransmesh = 0;
	currenttranstriangle = 0;
	currenttransvertex = 0;
}

void R_Mesh_Draw(const rmeshinfo_t *m)
{
	// these are static because gcc runs out of virtual registers otherwise
	static int i, j, overbright;
	static float *in, scaler;
	static float cr, cg, cb, ca;
	static buf_mesh_t *mesh;
	static buf_vertex_t *vert;
	static buf_fcolor_t *fcolor;
	static buf_texcoord_t *texcoord[MAX_TEXTUREUNITS];

	if (!backendactive)
		Sys_Error("R_Mesh_Draw: called when backend is not active\n");

	if (m->index == NULL
	 || !m->numtriangles
	 || m->vertex == NULL
	 || !m->numverts)
		Host_Error("R_Mesh_Draw: no triangles or verts\n");

	// ignore meaningless alpha meshs
	if (!m->depthwrite && m->blendfunc1 == GL_SRC_ALPHA && (m->blendfunc2 == GL_ONE || m->blendfunc2 == GL_ONE_MINUS_SRC_ALPHA))
	{
		if (m->color)
		{
			for (i = 0, in = m->color + 3;i < m->numverts;i++, (int)in += m->colorstep)
				if (*in >= 0.01f)
					break;
			if (i == m->numverts)
				return;
		}
		else if (m->ca < 0.01f)
			return;
	}

	if (!backendactive)
		Sys_Error("R_Mesh_Draw: called when backend is not active\n");

	scaler = 1;
	if (m->blendfunc2 == GL_SRC_COLOR)
	{
		if (m->blendfunc1 == GL_DST_COLOR) // 2x modulate with framebuffer
			scaler *= 0.5f;
	}
	else
	{
		if (m->tex[0])
		{
			overbright = gl_combine.integer;
			if (overbright)
				scaler *= 0.25f;
		}
		scaler *= overbrightscale;
	}

	if (m->transparent)
	{
		if (currenttransmesh >= max_meshs || (currenttranstriangle + m->numtriangles) > max_meshs || (currenttransvertex + m->numverts) > max_verts)
		{
			if (!transranout)
			{
				Con_Printf("R_Mesh_Draw: ran out of room for transparent meshs\n");
				transranout = true;
			}
			return;
		}

		c_transmeshs++;
		c_transtris += m->numtriangles;
		vert = &buf_transvertex[currenttransvertex];
		fcolor = &buf_transfcolor[currenttransvertex];
		for (i = 0;i < backendunits;i++)
			texcoord[i] = &buf_transtexcoord[i][currenttransvertex];

		// transmesh is only for storage of transparent meshs until they
		// are inserted into the main mesh array
		mesh = &buf_transmesh[currenttransmesh++];
		mesh->firsttriangle = currenttranstriangle;
		memcpy(&buf_transtri[currenttranstriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttranstriangle += m->numtriangles;

		mesh->firstvert = currenttransvertex;
		mesh->lastvert = currenttransvertex + m->numverts - 1;
		currenttransvertex += m->numverts;
	}
	else
	{
		if (m->numtriangles > max_meshs || m->numverts > max_verts)
		{
			Con_Printf("R_Mesh_Draw: mesh too big for buffers\n");
			return;
		}

		if (currentmesh >= max_meshs || (currenttriangle + m->numtriangles) > max_batch || (currentvertex + m->numverts) > max_verts)
			R_Mesh_Render();

		c_meshs++;
		c_meshtris += m->numtriangles;
		vert = &buf_vertex[currentvertex];
		fcolor = &buf_fcolor[currentvertex];
		for (i = 0;i < backendunits;i++)
			texcoord[i] = &buf_texcoord[i][currentvertex];

		// opaque meshs are rendered directly
		mesh = &buf_mesh[currentmesh++];
		mesh->firsttriangle = currenttriangle;
		memcpy(&buf_tri[currenttriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttriangle += m->numtriangles;

		mesh->firstvert = currentvertex;
		mesh->lastvert = currentvertex + m->numverts - 1;
		currentvertex += m->numverts;
	}

	// code shared for transparent and opaque meshs
	mesh->blendfunc1 = m->blendfunc1;
	mesh->blendfunc2 = m->blendfunc2;
	mesh->depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite);
	mesh->depthtest = !m->depthdisable;
	mesh->triangles = m->numtriangles;
	j = -1;
	for (i = 0;i < backendunits;i++)
	{
		if ((mesh->textures[i] = m->tex[i]))
			j = i;
		mesh->texturergbscale[i] = m->texrgbscale[i];
		if (mesh->texturergbscale[i] != 1 && mesh->texturergbscale[i] != 2 && mesh->texturergbscale[i] != 4)
			mesh->texturergbscale[i] = 1;
	}
	if (overbright && j >= 0)
		mesh->texturergbscale[j] = 4;

	if (m->vertexstep != sizeof(buf_vertex_t))
	{
		for (i = 0, in = m->vertex;i < m->numverts;i++, (int)in += m->vertexstep)
		{
			vert[i].v[0] = in[0];
			vert[i].v[1] = in[1];
			vert[i].v[2] = in[2];
		}
	}
	else
		memcpy(vert, m->vertex, m->numverts * sizeof(buf_vertex_t));

	if (m->color)
	{
		for (i = 0, in = m->color;i < m->numverts;i++, (int)in += m->colorstep)
		{
			fcolor[i].c[0] = in[0] * scaler;
			fcolor[i].c[1] = in[1] * scaler;
			fcolor[i].c[2] = in[2] * scaler;
			fcolor[i].c[3] = in[3];
		}
	}
	else
	{
		cr = m->cr * scaler;
		cg = m->cg * scaler;
		cb = m->cb * scaler;
		ca = m->ca;
		for (i = 0;i < m->numverts;i++)
		{
			fcolor[i].c[0] = cr;
			fcolor[i].c[1] = cg;
			fcolor[i].c[2] = cb;
			fcolor[i].c[3] = ca;
		}
	}

	for (j = 0;j < MAX_TEXTUREUNITS && m->tex[j];j++)
	{
		if (j >= backendunits)
			Sys_Error("R_Mesh_Draw: texture %i supplied when there are only %i texture units\n", j + 1, backendunits);
		if (m->texcoordstep[j] != sizeof(buf_texcoord_t))
		{
			for (i = 0, in = m->texcoords[j];i < m->numverts;i++, (int)in += m->texcoordstep[j])
			{
				texcoord[j][i].t[0] = in[0];
				texcoord[j][i].t[1] = in[1];
			}
		}
		else
			memcpy(&texcoord[j][0].t[0], m->texcoords[j], m->numverts * sizeof(buf_texcoord_t));
	}
	#if 0
	for (;j < backendunits;j++)
		memset(&texcoord[j][0].t[0], 0, m->numverts * sizeof(buf_texcoord_t));
	#endif

	if (currenttriangle >= max_batch)
		R_Mesh_Render();
}

void R_Mesh_Draw_NativeOnly(const rmeshinfo_t *m)
{
	// these are static because gcc runs out of virtual registers otherwise
	static int i, j, overbright;
	static float *in, scaler;
	static buf_mesh_t *mesh;
	static buf_vertex_t *vert;
	static buf_fcolor_t *fcolor;
	static buf_texcoord_t *texcoord[MAX_TEXTUREUNITS];

	if (!backendactive)
		Sys_Error("R_Mesh_Draw: called when backend is not active\n");

	if (m->index == NULL
	 || !m->numtriangles
	 || m->vertex == NULL
	 || !m->numverts)
		Host_Error("R_Mesh_Draw: no triangles or verts\n");

	// ignore meaningless alpha meshs
	if (!m->depthwrite && m->blendfunc1 == GL_SRC_ALPHA && (m->blendfunc2 == GL_ONE || m->blendfunc2 == GL_ONE_MINUS_SRC_ALPHA))
	{
		if (m->color)
		{
			for (i = 0, in = m->color + 3;i < m->numverts;i++, (int)in += m->colorstep)
				if (*in >= 0.01f)
					break;
			if (i == m->numverts)
				return;
		}
		else if (m->ca < 0.01f)
			return;
	}

	scaler = 1;
	if (m->blendfunc2 == GL_SRC_COLOR)
	{
		if (m->blendfunc1 == GL_DST_COLOR) // 2x modulate with framebuffer
			scaler *= 0.5f;
	}
	else
	{
		if (m->tex[0])
		{
			overbright = gl_combine.integer;
			if (overbright)
				scaler *= 0.25f;
		}
		scaler *= overbrightscale;
	}

	if (m->transparent)
	{
		if (currenttransmesh >= max_meshs || (currenttranstriangle + m->numtriangles) > max_meshs || (currenttransvertex + m->numverts) > max_verts)
		{
			if (!transranout)
			{
				Con_Printf("R_Mesh_Draw_NativeOnly: ran out of room for transparent meshs\n");
				transranout = true;
			}
			return;
		}

		c_transmeshs++;
		c_transtris += m->numtriangles;
		vert = &buf_transvertex[currenttransvertex];
		fcolor = &buf_transfcolor[currenttransvertex];
		for (i = 0;i < backendunits;i++)
			texcoord[i] = &buf_transtexcoord[i][currenttransvertex];

		// transmesh is only for storage of transparent meshs until they
		// are inserted into the main mesh array
		mesh = &buf_transmesh[currenttransmesh++];
		mesh->firsttriangle = currenttranstriangle;
		memcpy(&buf_transtri[currenttranstriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttranstriangle += m->numtriangles;

		mesh->firstvert = currenttransvertex;
		mesh->lastvert = currenttransvertex + m->numverts - 1;
		currenttransvertex += m->numverts;
	}
	else
	{
		if (m->numtriangles > max_meshs || m->numverts > max_verts)
		{
			Con_Printf("R_Mesh_Draw_NativeOnly: mesh too big for buffers\n");
			return;
		}

		if (currentmesh >= max_meshs || (currenttriangle + m->numtriangles) > max_batch || (currentvertex + m->numverts) > max_verts)
			R_Mesh_Render();

		c_meshs++;
		c_meshtris += m->numtriangles;
		vert = &buf_vertex[currentvertex];
		fcolor = &buf_fcolor[currentvertex];
		for (i = 0;i < backendunits;i++)
			texcoord[i] = &buf_texcoord[i][currentvertex];

		mesh = &buf_mesh[currentmesh++];
		// opaque meshs are rendered directly
		mesh->firsttriangle = currenttriangle;
		memcpy(&buf_tri[currenttriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttriangle += m->numtriangles;

		mesh->firstvert = currentvertex;
		mesh->lastvert = currentvertex + m->numverts - 1;
		currentvertex += m->numverts;
	}

	// code shared for transparent and opaque meshs
	mesh->blendfunc1 = m->blendfunc1;
	mesh->blendfunc2 = m->blendfunc2;
	mesh->depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite);
	mesh->depthtest = !m->depthdisable;
	mesh->triangles = m->numtriangles;
	j = -1;
	for (i = 0;i < backendunits;i++)
	{
		if ((mesh->textures[i] = m->tex[i]))
			j = i;
		mesh->texturergbscale[i] = m->texrgbscale[i];
		if (mesh->texturergbscale[i] != 1 && mesh->texturergbscale[i] != 2 && mesh->texturergbscale[i] != 4)
			mesh->texturergbscale[i] = 1;
	}
	if (overbright && j >= 0)
		mesh->texturergbscale[j] = 4;

	if (m->vertexstep != sizeof(buf_vertex_t))
		Host_Error("R_Mesh_Draw_NativeOnly: unsupported vertexstep\n");
	if (m->colorstep != sizeof(buf_fcolor_t))
		Host_Error("R_Mesh_Draw_NativeOnly: unsupported colorstep\n");
	if (m->color == NULL)
		Host_Error("R_Mesh_Draw_NativeOnly: must provide color array\n");
	for (j = 0;j < MAX_TEXTUREUNITS && m->tex[j];j++)
	{
		if (j >= backendunits)
			Sys_Error("R_Mesh_Draw_NativeOnly: texture %i supplied when there are only %i texture units\n", j + 1, backendunits);
		if (m->texcoordstep[j] != sizeof(buf_texcoord_t))
			Host_Error("R_Mesh_Draw_NativeOnly: unsupported texcoordstep\n");
	}

	memcpy(vert, m->vertex, m->numverts * sizeof(buf_vertex_t));
	for (j = 0;j < MAX_TEXTUREUNITS && m->tex[j];j++)
		memcpy(&texcoord[j][0].t[0], m->texcoords[j], m->numverts * sizeof(buf_texcoord_t));
	#if 0
	for (;j < backendunits;j++)
		memset(&texcoord[j][0].t[0], 0, m->numverts * sizeof(buf_texcoord_t));
	#endif

	memcpy(fcolor, m->color, m->numverts * sizeof(buf_fcolor_t));

	// do this as a second step because memcpy preloaded the cache, which we can't easily do
	if (scaler != 1)
	{
		for (i = 0;i < m->numverts;i++)
		{
			fcolor[i].c[0] *= scaler;
			fcolor[i].c[1] *= scaler;
			fcolor[i].c[2] *= scaler;
		}
	}

	if (currenttriangle >= max_batch)
		R_Mesh_Render();
}

/*
void R_Mesh_Draw_GetBuffer(volatile rmeshinfo_t *m)
{
	// these are static because gcc runs out of virtual registers otherwise
	static int i, j, *index, overbright;
	static float *in, scaler;

	if (!backendactive)
		Sys_Error("R_Mesh_Draw: called when backend is not active\n");

	if (!m->numtriangles
	 || !m->numverts)
		Host_Error("R_Mesh_Draw: no triangles or verts\n");

	scaler = 1;
	if (m->blendfunc2 == GL_SRC_COLOR)
	{
		if (m->blendfunc1 == GL_DST_COLOR) // 2x modulate with framebuffer
			scaler *= 0.5f;
	}
	else
	{
		if (m->tex[0])
		{
			overbright = gl_combine.integer;
			if (overbright)
				scaler *= 0.25f;
		}
		scaler *= overbrightscale;
	}

	if (m->transparent)
	{
		if (currenttransmesh >= max_meshs || (currenttranstriangle + m->numtriangles) > max_meshs || (currenttransvertex + m->numverts) > max_verts)
		{
			if (!transranout)
			{
				Con_Printf("R_Mesh_Draw: ran out of room for transparent meshs\n");
				transranout = true;
			}
			return;
		}

		c_transmeshs++;
		c_transtris += m->numtriangles;
		m->vertex = &buf_transvertex[currenttransvertex].v[0];
		m->color = &buf_transfcolor[currenttransvertex].c[0];
		for (i = 0;i < backendunits;i++)
			m->texcoords[i] = &buf_transtexcoord[i][currenttransvertex].tc[0];

		// transmesh is only for storage of transparent meshs until they
		// are inserted into the main mesh array
		mesh = &buf_transmesh[currenttransmesh++];
		mesh->firsttriangle = currenttranstriangle;
		memcpy(&buf_transtri[currenttranstriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttranstriangle += m->numtriangles;

		mesh->firstvert = currenttransvertex;
		mesh->lastvert = currenttransvertex + m->numverts - 1;
		currenttransvertex += m->numverts;
	}
	else
	{
		if (m->numtriangles > max_meshs || m->numverts > max_verts)
		{
			Con_Printf("R_Mesh_Draw_NativeOnly: mesh too big for buffers\n");
			return;
		}

		if (currentmesh >= max_meshs || (currenttriangle + m->numtriangles) > max_batch || (currentvertex + m->numverts) > max_verts)
			R_Mesh_Render();

		c_meshs++;
		c_meshtris += m->numtriangles;
		vert = &buf_vertex[currentvertex];
		fcolor = &buf_fcolor[currentvertex];
		for (i = 0;i < backendunits;i++)
			texcoord[i] = &buf_texcoord[i][currentvertex];

		// opaque meshs are rendered directly
		mesh = &buf_mesh[currentmesh++];
		mesh->firsttriangle = currenttriangle;
		memcpy(&buf_tri[currenttriangle].index[0], m->index, sizeof(int[3]) * m->numtriangles);
		currenttriangle += m->numtriangles;

		mesh->firstvert = currentvertex;
		mesh->lastvert = currentvertex + m->numverts - 1;
		currentvertex += m->numverts;
	}

	// code shared for transparent and opaque meshs
	mesh->blendfunc1 = m->blendfunc1;
	mesh->blendfunc2 = m->blendfunc2;
	mesh->depthmask = (m->blendfunc2 == GL_ZERO || m->depthwrite);
	mesh->depthtest = !m->depthdisable;
	mesh->triangles = m->numtriangles;
	j = -1;
	for (i = 0;i < backendunits;i++)
	{
		if ((mesh->textures[i] = m->tex[i]))
			j = i;
		mesh->texturergbscale[i] = m->texrgbscale[i];
		if (mesh->texturergbscale[i] != 1 && mesh->texturergbscale[i] != 2 && mesh->texturergbscale[i] != 4)
			mesh->texturergbscale[i] = 1;
	}
	if (overbright && j >= 0)
		mesh->texturergbscale[j] = 4;

	if (m->vertexstep != sizeof(buf_vertex_t))
		Host_Error("R_Mesh_Draw_NativeOnly: unsupported vertexstep\n");
	if (m->colorstep != sizeof(buf_fcolor_t))
		Host_Error("R_Mesh_Draw_NativeOnly: unsupported colorstep\n");
	if (m->color == NULL)
		Host_Error("R_Mesh_Draw_NativeOnly: must provide color array\n");
	for (j = 0;j < MAX_TEXTUREUNITS && m->tex[j];j++)
	{
		if (j >= backendunits)
			Sys_Error("R_Mesh_Draw_NativeOnly: texture %i supplied when there are only %i texture units\n", j + 1, backendunits);
		if (m->texcoordstep[j] != sizeof(buf_texcoord_t))
			Host_Error("R_Mesh_Draw_NativeOnly: unsupported texcoordstep\n");
	}

	memcpy(vert, m->vertex, m->numverts * sizeof(buf_vertex_t));
	for (j = 0;j < MAX_TEXTUREUNITS && m->tex[j];j++)
		memcpy(&texcoord[j][0].t[0], m->texcoords[j], m->numverts * sizeof(buf_texcoord_t));
	#if 0
	for (;j < backendunits;j++)
		memset(&texcoord[j][0].t[0], 0, m->numverts * sizeof(buf_texcoord_t));
	#endif

	memcpy(fcolor, m->color, m->numverts * sizeof(buf_fcolor_t));

	// do this as a second step because memcpy preloaded the cache, which we can't easily do
	if (scaler != 1)
	{
		for (i = 0;i < m->numverts;i++)
		{
			fcolor[i].c[0] *= scaler;
			fcolor[i].c[1] *= scaler;
			fcolor[i].c[2] *= scaler;
		}
	}

	if (currenttriangle >= max_batch)
		R_Mesh_Render();
}
*/

void R_Mesh_DrawPolygon(rmeshinfo_t *m, int numverts)
{
	m->index = polyindexarray;
	m->numverts = numverts;
	m->numtriangles = numverts - 2;
	if (m->numtriangles < 1)
	{
		Con_Printf("R_Mesh_DrawPolygon: invalid vertex count\n");
		return;
	}
	if (m->numtriangles >= 256)
	{
		Con_Printf("R_Mesh_DrawPolygon: only up to 256 triangles (258 verts) supported\n");
		return;
	}
	R_Mesh_Draw(m);
}

/*
// LordHavoc: this thing is evil, but necessary because decals account for so much overhead
void R_Mesh_DrawDecal(const rmeshinfo_t *m)
{
	// these are static because gcc runs out of virtual registers otherwise
	static int i, *index, overbright;
	static float scaler;
	static float cr, cg, cb, ca;
	static buf_mesh_t *mesh;
	static buf_vertex_t *vert;
	static buf_fcolor_t *fcolor;
	static buf_texcoord_t *texcoord;
	static buf_transtri_t *tri;

	if (!backendactive)
		Sys_Error("R_Mesh_Draw: called when backend is not active\n");

	scaler = 1;
	if (m->tex[0])
	{
		overbright = gl_combine.integer;
		if (overbright)
			scaler *= 0.25f;
	}
	scaler *= overbrightscale;

	if (m->transparent)
	{
		if (currenttransmesh >= max_meshs || (currenttranstriangle + 2) > max_meshs || (currenttransvertex + 4) > max_verts)
		{
			if (!transranout)
			{
				Con_Printf("R_Mesh_Draw: ran out of room for transparent meshs\n");
				transranout = true;
			}
			return;
		}

		c_transmeshs++;
		c_transtris += 2;
		vert = &buf_transvertex[currenttransvertex];
		fcolor = &buf_transfcolor[currenttransvertex];
		texcoord = &buf_transtexcoord[0][currenttransvertex];

		// transmesh is only for storage of transparent meshs until they
		// are inserted into the main mesh array
		mesh = &buf_transmesh[currenttransmesh++];
		mesh->blendfunc1 = m->blendfunc1;
		mesh->blendfunc2 = m->blendfunc2;
		mesh->depthmask = false;
		mesh->depthtest = true;
		mesh->firsttriangle = currenttranstriangle;
		mesh->triangles = 2;
		mesh->textures[0] = m->tex[0];
		mesh->texturergbscale[0] = overbright ? 4 : 1;
		for (i = 1;i < backendunits;i++)
		{
			mesh->textures[i] = 0;
			mesh->texturergbscale[i] = 1;
		}
		mesh->chain = NULL;

		index = &buf_transtri[currenttranstriangle].index[0];
		mesh->firstvert = currenttransvertex;
		mesh->lastvert = currenttransvertex + 3;
		currenttranstriangle += 2;
		currenttransvertex += 4;
	}
	else
	{
		if (2 > max_meshs || 4 > max_verts)
		{
			Con_Printf("R_Mesh_Draw: mesh too big for buffers\n");
			return;
		}

		if (currentmesh >= max_meshs || (currenttriangle + 2) > max_batch || (currentvertex + 4) > max_verts)
			R_Mesh_Render();

		c_meshs++;
		c_meshtris += 2;
		vert = &buf_vertex[currentvertex];
		fcolor = &buf_fcolor[currentvertex];
		texcoord = &buf_texcoord[0][currentvertex];

		mesh = &buf_mesh[currentmesh++];
		mesh->blendfunc1 = m->blendfunc1;
		mesh->blendfunc2 = m->blendfunc2;
		mesh->depthmask = false;
		mesh->depthtest = !m->depthdisable;
		mesh->firsttriangle = currenttriangle;
		mesh->triangles = 2;
		mesh->textures[0] = m->tex[0];
		mesh->texturergbscale[0] = overbright ? 4 : 1;
		for (i = 1;i < backendunits;i++)
		{
			mesh->textures[i] = 0;
			mesh->texturergbscale[i] = 1;
		}

		// opaque meshs are rendered directly
		index = &buf_tri[currenttriangle].index[0];
		mesh->firstvert = currentvertex;
		mesh->lastvert = currentvertex + 3;
		currenttriangle += 2;
		currentvertex += 4;
	}

	index[0] = 0;
	index[1] = 1;
	index[2] = 2;
	index[3] = 0;
	index[4] = 2;
	index[5] = 3;

	// buf_vertex_t must match the size of the decal vertex array (or vice versa)
	memcpy(vert, m->vertex, 4 * sizeof(buf_vertex_t));

	cr = m->cr * scaler;
	cg = m->cg * scaler;
	cb = m->cb * scaler;
	ca = m->ca;
	fcolor[0].c[0] = cr;
	fcolor[0].c[1] = cg;
	fcolor[0].c[2] = cb;
	fcolor[0].c[3] = ca;
	fcolor[1].c[0] = cr;
	fcolor[1].c[1] = cg;
	fcolor[1].c[2] = cb;
	fcolor[1].c[3] = ca;
	fcolor[2].c[0] = cr;
	fcolor[2].c[1] = cg;
	fcolor[2].c[2] = cb;
	fcolor[2].c[3] = ca;
	fcolor[3].c[0] = cr;
	fcolor[3].c[1] = cg;
	fcolor[3].c[2] = cb;
	fcolor[3].c[3] = ca;

	// buf_texcoord_t must be the same size as the decal texcoord array (or vice versa)
	memcpy(&texcoord[0].t[0], m->texcoords[0], 4 * sizeof(buf_texcoord_t));

	if (currenttriangle >= max_batch)
		R_Mesh_Render();
}
*/

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

float CalcFov (float fov_x, float width, float height);
void R_ClearScreen(void);

void SCR_ScreenShot(char *filename, int x, int y, int width, int height)
{
	int i;
	byte *buffer;

	buffer = Mem_Alloc(tempmempool, width*height*3);
	glReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	CHECKGLERROR

	// LordHavoc: compensate for v_overbrightbits when using hardware gamma
	if (v_hwgamma.integer)
		for (i = 0;i < width * height * 3;i++)
			buffer[i] <<= v_overbrightbits.integer;

	Image_WriteTGARGB_preflipped(filename, width, height, buffer);

	Mem_Free(buffer);
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	int i;
	char filename[16];
	char checkname[MAX_OSPATH];
//
// find a file name to save it to
//
	strcpy(filename, "dp0000.tga");

	for (i=0 ; i<=9999 ; i++)
	{
		filename[2] = (i/1000)%10 + '0';
		filename[3] = (i/ 100)%10 + '0';
		filename[4] = (i/  10)%10 + '0';
		filename[5] = (i/   1)%10 + '0';
		sprintf (checkname, "%s/%s", com_gamedir, filename);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	}
	if (i==10000)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
 	}

	SCR_ScreenShot(filename, vid.realx, vid.realy, vid.realwidth, vid.realheight);
	Con_Printf ("Wrote %s\n", filename);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
struct
{
	float angles[3];
	char *name;
}
envmapinfo[6] =
{
	{{  0,   0, 0}, "ft"},
	{{  0,  90, 0}, "rt"},
	{{  0, 180, 0}, "bk"},
	{{  0, 270, 0}, "lf"},
	{{-90,  90, 0}, "up"},
	{{ 90,  90, 0}, "dn"}
};
static void R_Envmap_f (void)
{
	int j, size;
	char filename[256], basename[256];

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("envmap <basename> <size>: save out 6 cubic environment map images, usable with loadsky, note that size must one of 128, 256, 512, or 1024 and can't be bigger than your current resolution\n");
		return;
	}

	if (!r_render.integer)
		return;

	strcpy(basename, Cmd_Argv(1));
	size = atoi(Cmd_Argv(2));
	if (size != 128 && size != 256 && size != 512 && size != 1024)
	{
		Con_Printf("envmap: size must be one of 128, 256, 512, or 1024\n");
		return;
	}
	if (size > vid.realwidth || size > vid.realheight)
	{
		Con_Printf("envmap: your resolution is not big enough to render that size\n");
		return;
	}

	envmap = true;

	r_refdef.x = 0;
	r_refdef.y = 0;
	r_refdef.width = size;
	r_refdef.height = size;

	r_refdef.fov_x = 90;
	r_refdef.fov_y = 90;

	for (j = 0;j < 6;j++)
	{
		sprintf(filename, "env/%s%s.tga", basename, envmapinfo[j].name);
		VectorCopy(envmapinfo[j].angles, r_refdef.viewangles);
		R_ClearScreen();
		R_RenderView ();
		SCR_ScreenShot(filename, vid.realx, vid.realy, size, size);
	}

	envmap = false;
}

//=============================================================================

void R_ClearScreen(void)
{
	if (r_render.integer)
	{
		glClearColor(0,0,0,0);
		CHECKGLERROR
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: clear the screen (around the view as well)
		CHECKGLERROR
		if (gl_dither.integer)
			glEnable(GL_DITHER);
		else
			glDisable(GL_DITHER);
		CHECKGLERROR
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
	//Mem_CheckSentinelsGlobal();
	//R_TimeReport("memtest");

	glFinish ();
	CHECKGLERROR

	VID_Finish ();

	R_TimeReport("finish");

	if (gl_combine.integer && !gl_combine_extension)
		Cvar_SetValue("gl_combine", 0);

	lightscalebit = v_overbrightbits.integer;
	if (gl_combine.integer && r_multitexture.integer)
		lightscalebit += 2;

	lightscale = 1.0f / (float) (1 << lightscalebit);
	overbrightscale = 1.0f / (float) (1 << v_overbrightbits.integer);

	R_TimeReport("setup");

	R_ClearScreen();

	R_TimeReport("clear");

	if (scr_conlines < vid.conheight)
		R_RenderView();

	// draw 2D stuff
	R_DrawQueue();
}
