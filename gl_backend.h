
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define MAX_TEXTUREUNITS 8

extern int c_meshtris, c_meshs;

typedef struct
{
	//input to R_Mesh_Draw_GetBuffer
	int depthwrite; // force depth writing enabled even if polygon is not opaque
	int depthdisable; // disable depth read/write entirely
	int blendfunc1;
	int blendfunc2;
	int wantoverbright;
	int tex[MAX_TEXTUREUNITS];
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	matrix4x4_t matrix; // model to world transform matrix
}
rmeshstate_t;

// overbright rendering scale for the current state
extern float mesh_colorscale;
extern int *varray_element;
extern float *varray_vertex;
extern float *varray_color;
extern float *varray_texcoord[MAX_TEXTUREUNITS];
extern int mesh_maxverts;
extern int mesh_maxtris;

// adds console variables and registers the render module (only call from GL_Init)
void gl_backend_init(void);

// starts mesh rendering for the frame
void R_Mesh_Start(float farclip);

// ends mesh rendering for the frame
// (only valid after R_Mesh_Start)
void R_Mesh_Finish(void);

// clears depth buffer, used for masked sky rendering
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_ClearDepth(void);

// sets up the requested state
void R_Mesh_State(const rmeshstate_t *m);

// enlarges geometry buffers if they are too small
#define R_Mesh_ResizeCheck(numverts, numtriangles) if ((numverts) > mesh_maxverts || (numtriangles) > mesh_maxtris) _R_Mesh_ResizeCheck(numverts, numtriangles);
void _R_Mesh_ResizeCheck(int numverts, int numtriangles);

// renders the mesh in the varray_* buffers
void R_Mesh_Draw(int numverts, int numtriangles);

// saves a section of the rendered frame to a .tga file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen (void);

#endif

