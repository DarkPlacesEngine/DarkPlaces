
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define MAX_TEXTUREUNITS 8

#define POLYGONELEMENTS_MAXPOINTS 258
extern int polygonelements[768];

void GL_DrawRangeElements(int firstvert, int endvert, int indexcount, int *index);

void GL_SetupView_ViewPort (int x, int y, int width, int height);
void GL_SetupView_Orientation_Identity (void);
void GL_SetupView_Orientation_FromEntity (vec3_t origin, vec3_t angles);
void GL_SetupView_Mode_Perspective (double aspect, double fovx, double fovy, double zNear, double zFar);
void GL_SetupView_Mode_Ortho (double x1, double y1, double x2, double y2, double zNear, double zFar);
void GL_UseColorArray(void);
void GL_Color(float cr, float cg, float cb, float ca);

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
	int tex[MAX_TEXTUREUNITS];
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
}
rmeshstate_t;

// overbright rendering scale for the current state
extern int r_lightmapscalebit;
extern float r_colorscale;
extern float *varray_vertex;
extern float *varray_color;
extern float *varray_texcoord[MAX_TEXTUREUNITS];
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

// enlarges vertex arrays if they are too small
#define R_Mesh_ResizeCheck(numverts) if ((numverts) > mesh_maxverts) _R_Mesh_ResizeCheck(numverts);
void _R_Mesh_ResizeCheck(int numverts);

// renders the mesh in the varray_* buffers
void R_Mesh_Draw(int numverts, int numtriangles, int *elements);

// saves a section of the rendered frame to a .tga file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen (void);

#endif

