
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define MAX_TEXTUREUNITS 8

#define POLYGONELEMENTS_MAXPOINTS 258
extern int polygonelements[768];

void GL_SetupView_ViewPort (int x, int y, int width, int height);
void GL_SetupView_Orientation_Identity (void);
void GL_SetupView_Orientation_FromEntity (vec3_t origin, vec3_t angles);
void GL_SetupView_Mode_Perspective (double fovx, double fovy, double zNear, double zFar);
void GL_SetupView_Mode_PerspectiveInfiniteFarClip (double fovx, double fovy, double zNear);
void GL_SetupView_Mode_Ortho (double x1, double y1, double x2, double y2, double zNear, double zFar);
void GL_UseColorArray(void);
void GL_Color(float cr, float cg, float cb, float ca);
void GL_TransformToScreen(const vec4_t in, vec4_t out);

extern cvar_t gl_lockarrays;

extern int c_meshelements, c_meshs;

//input to R_Mesh_State
typedef struct
{
	int depthwrite; // force depth writing enabled even if polygon is not opaque
	int depthdisable; // disable depth read/write entirely
	int blendfunc1;
	int blendfunc2;
	//int wantoverbright;
	int tex1d[MAX_TEXTUREUNITS];
	int tex[MAX_TEXTUREUNITS];
	int tex3d[MAX_TEXTUREUNITS];
	int texcubemap[MAX_TEXTUREUNITS];
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	int texalphascale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	int texcombinergb[MAX_TEXTUREUNITS]; // works with or without combine for some operations
	int texcombinealpha[MAX_TEXTUREUNITS]; // does nothing without combine
}
rmeshstate_t;

// overbright rendering scale for the current state
extern int r_lightmapscalebit;
extern float r_colorscale;
extern float *varray_vertex3f;
extern float *varray_color4f;
extern float *varray_texcoord3f[MAX_TEXTUREUNITS];
extern float *varray_texcoord2f[MAX_TEXTUREUNITS];
extern int mesh_maxverts;

// adds console variables and registers the render module (only call from GL_Init)
void gl_backend_init(void);

// starts mesh rendering for the frame
void R_Mesh_Start(void);

// ends mesh rendering for the frame
// (only valid after R_Mesh_Start)
void R_Mesh_Finish(void);

// sets up the requested transform matrix
void R_Mesh_Matrix(const matrix4x4_t *matrix);

// sets up the requested state
void R_Mesh_State(const rmeshstate_t *m);

// sets up the requested main state
void R_Mesh_MainState(const rmeshstate_t *m);

// sets up the requested texture state
void R_Mesh_TextureState(const rmeshstate_t *m);

// prepares varray_* buffers for rendering a mesh
void R_Mesh_GetSpace(int numverts);
// renders the mesh in the varray_* buffers
void R_Mesh_Draw(int numverts, int numtriangles, const int *elements);

// copies a vertex3f array into varray_vertex3f
void R_Mesh_CopyVertex3f(const float *vertex3f, int numverts);
// copies a texcoord2f array into varray_texcoord[tmu]
void R_Mesh_CopyTexCoord2f(int tmu, const float *texcoord2f, int numverts);
// copies a color4f array into varray_color4f
void R_Mesh_CopyColor4f(const float *color4f, int numverts);

// saves a section of the rendered frame to a .tga or .jpg file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height, qboolean jpeg);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen (void);

#endif

