
#define MAX_TEXTUREUNITS 4

extern int c_meshtris;

typedef struct
{
	int transparent;
	int depthwrite; // force depth writing enabled even if polygon is not opaque
	int blendfunc1;
	int blendfunc2;
	int numtriangles;
	int *index;
	int numverts;
	float *vertex;
	int vertexstep;
	float *color;
	int colorstep;
	// if color is NULL, these are used for all vertices
	float cr, cg, cb, ca;
	int tex[MAX_TEXTUREUNITS];
	float *texcoords[MAX_TEXTUREUNITS];
	int texcoordstep[MAX_TEXTUREUNITS];
	float texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
}
rmeshinfo_t;

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
// ease-of-use frontend to R_Mesh_Draw for particles, no speed gain
void R_Mesh_DrawParticle(vec3_t org, vec3_t right, vec3_t up, vec_t scale, int texnum, float cr, float cg, float cb, float ca, float s1, float t1, float s2, float t2, float fs1, float ft1, float fs2, float ft2);