
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define MAX_TEXTUREUNITS 8

#define POLYGONELEMENTS_MAXPOINTS 258
extern int polygonelements[768];

void GL_SetupView_Orientation_Identity(void);
void GL_SetupView_Orientation_FromEntity(matrix4x4_t *matrix);
void GL_SetupView_Mode_Perspective(double fovx, double fovy, double zNear, double zFar);
void GL_SetupView_Mode_PerspectiveInfiniteFarClip(double fovx, double fovy, double zNear);
void GL_SetupView_Mode_Ortho(double x1, double y1, double x2, double y2, double zNear, double zFar);
void GL_BlendFunc(int blendfunc1, int blendfunc2);
void GL_DepthMask(int state);
void GL_DepthTest(int state);
void GL_VertexPointer(const float *p);
void GL_ColorPointer(const float *p);
void GL_Color(float cr, float cg, float cb, float ca);
void GL_TransformToScreen(const vec4_t in, vec4_t out);
void GL_LockArrays(int first, int count);
void GL_ActiveTexture(int num);
void GL_ClientActiveTexture(int num);
void GL_Scissor(int x, int y, int width, int height); // AK for DRAWQUEUE_SETCLIP
void GL_ScissorTest(int state);	// AK for DRAWQUEUE_(RE)SETCLIP

extern cvar_t gl_lockarrays;
extern cvar_t gl_mesh_copyarrays;
extern cvar_t gl_paranoid;
extern cvar_t gl_printcheckerror;

extern int c_meshelements, c_meshs;

//input to R_Mesh_State
typedef struct
{
	// textures
	int tex1d[MAX_TEXTUREUNITS];
	int tex[MAX_TEXTUREUNITS];
	int tex3d[MAX_TEXTUREUNITS];
	int texcubemap[MAX_TEXTUREUNITS];
	// texture combine settings
	int texrgbscale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	int texalphascale[MAX_TEXTUREUNITS]; // used only if COMBINE is present
	int texcombinergb[MAX_TEXTUREUNITS]; // works with or without combine for some operations
	int texcombinealpha[MAX_TEXTUREUNITS]; // does nothing without combine
	// pointers
	const float *pointer_texcoord[MAX_TEXTUREUNITS];
}
rmeshstate_t;

// adds console variables and registers the render module (only call from GL_Init)
void gl_backend_init(void);

// starts mesh rendering for the frame
void R_Mesh_Start(void);

// ends mesh rendering for the frame
// (only valid after R_Mesh_Start)
void R_Mesh_Finish(void);

// sets up the requested transform matrix
void R_Mesh_Matrix(const matrix4x4_t *matrix);

// sets up the requested transform matrix
void R_Mesh_TextureMatrix(int unitnumber, const matrix4x4_t *matrix);

// set up the requested state
void R_Mesh_State_Texture(const rmeshstate_t *m);

// renders a mesh
void R_Mesh_Draw(int numverts, int numtriangles, const int *elements);
// renders a mesh as lines
void R_Mesh_Draw_ShowTris(int numverts, int numtriangles, int *elements);

// saves a section of the rendered frame to a .tga or .jpg file
qboolean SCR_ScreenShot(char *filename, int x, int y, int width, int height, qboolean jpeg);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of frame
void SCR_UpdateScreen(void);

// public structure
typedef struct rcachearrayrequest_s
{
	// for use by the code that is requesting the array, these are not
	// directly used but merely compared to determine if cache items are
	// identical
	const void *id_pointer1;
	const void *id_pointer2;
	const void *id_pointer3;
	int id_number1;
	int id_number2;
	int id_number3;
	// size of array data
	int data_size;
	// array data pointer
	void *data;
}
rcachearrayrequest_t;

int R_Mesh_CacheArray(rcachearrayrequest_t *r);

extern float varray_vertex3f[65536*3];
extern float varray_color4f[65536*4];
extern float varray_texcoord2f[4][65536*2];
extern float varray_texcoord3f[4][65536*3];
extern float varray_normal3f[65536*3];
extern int earray_element3i[65536];

#endif

