
#define MAX_TEXTUREUNITS 4

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
	float texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
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
	float texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present

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
// sets up mesh renderer for the frame
void R_Mesh_Clear(void);
// renders queued meshs
void R_Mesh_Render(void);
// queues a mesh to be rendered (invokes Render if queue is full)
void R_Mesh_Draw(const rmeshinfo_t *m);
// renders the queued transparent meshs
void R_Mesh_AddTransparent(void);
// ease-of-use frontend to R_Mesh_Draw, set up meshinfo, except for index and numtriangles and numverts, then call this
void R_Mesh_DrawPolygon(rmeshinfo_t *m, int numverts);
// same as normal, except for harsh format restrictions (vertex must be 4 float, color must be 4 float, texcoord must be 2 float, flat color not supported)
void R_Mesh_Draw_NativeOnly(const rmeshinfo_t *m);
// allocates space in geometry buffers, and fills in pointers to the buffers in passsed struct
// (this is used for very high speed rendering, no copying)
int R_Mesh_Draw_GetBuffer(rmeshbufferinfo_t *m);
