
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define MAX_TEXTUREUNITS 8

extern int c_meshtris, c_meshs, c_transtris, c_transmeshs;

typedef struct
{
	//input to R_Mesh_Draw_GetBuffer
	int transparent;
	int depthwrite; // force depth writing enabled even if polygon is not opaque
	int depthdisable; // disable depth read/write entirely
	int blendfunc1;
	int blendfunc2;
	int numtriangles;
	int numverts;
	int tex[MAX_TEXTUREUNITS];
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	// model to world transform matrix
	matrix4x4_t matrix;

	// output
	int *index;
	float *vertex;
	float *color;
	float colorscale;
	float *texcoords[MAX_TEXTUREUNITS];
}
rmeshbufferinfo_t;

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

// renders current batch of meshs
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_Render(void);

// renders the queued transparent meshs
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_AddTransparent(void);

// allocates space in geometry buffers, and fills in pointers to the buffers in passsed struct
// (it is up to the caller to fill in the geometry data)
// (make sure you scale your colors by the colorscale field)
// (only valid between R_Mesh_Start and R_Mesh_Finish)
int R_Mesh_Draw_GetBuffer(rmeshbufferinfo_t *m, int wantoverbright);

// saves a section of the rendered frame to a .tga file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen (void);

#endif

