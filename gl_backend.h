
#define MAX_TEXTUREUNITS 8

extern int c_meshtris, c_meshs, c_transtris, c_transmeshs;

typedef struct
{
	int transparent;
	int depthwrite; // force depth writing enabled even if polygon is not opaque
	int depthdisable; // disable depth read/write entirely
	int blendfunc1;
	int blendfunc2;
	int numtriangles;
	int *index;
	int numverts;
	float *vertex;
	int vertexstep;
	float *color;
	int colorstep;
	float cr, cg, cb, ca; // if color is NULL, these are used for all vertices
	int tex[MAX_TEXTUREUNITS];
	float *texcoords[MAX_TEXTUREUNITS];
	int texcoordstep[MAX_TEXTUREUNITS];
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
}
rmeshinfo_t;

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
void R_Mesh_Start(void);

// ends mesh rendering for the frame
// (only valid after R_Mesh_Start)
void R_Mesh_Finish(void);

// clears depth buffer, used for masked sky rendering
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_ClearDepth(void);

// renders current batch of meshs
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_Render(void);

// queues a mesh to be rendered (invokes Render if queue is full)
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_Draw(const rmeshinfo_t *m);

// renders the queued transparent meshs
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_AddTransparent(void);

// ease-of-use frontend to R_Mesh_Draw, set up meshinfo, except for index and numtriangles and numverts, then call this
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_DrawPolygon(rmeshinfo_t *m, int numverts);

// same as normal, except for harsh format restrictions (vertex must be 4 float, color must be 4 float, texcoord must be 2 float, flat color not supported)
// (only valid between R_Mesh_Start and R_Mesh_Finish)
void R_Mesh_Draw_NativeOnly(const rmeshinfo_t *m);

// allocates space in geometry buffers, and fills in pointers to the buffers in passsed struct
// (this is used for very high speed rendering, no copying)
// (only valid between R_Mesh_Start and R_Mesh_Finish)
int R_Mesh_Draw_GetBuffer(rmeshbufferinfo_t *m);

// saves a section of the rendered frame to a .tga file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen (void);
