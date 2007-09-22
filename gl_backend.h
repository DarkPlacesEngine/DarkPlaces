
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

// how many texture units to track state on (backendunits/backendimageunits/backendarrayunits are limited to this value)
#define MAX_TEXTUREUNITS 64

#define POLYGONELEMENTS_MAXPOINTS 258
extern int polygonelements[(POLYGONELEMENTS_MAXPOINTS-2)*3];
#define QUADELEMENTS_MAXQUADS 128
extern int quadelements[QUADELEMENTS_MAXQUADS*6];

void GL_SetupView_Orientation_Identity(void);
void GL_SetupView_Orientation_FromEntity(const matrix4x4_t *matrix);
void GL_SetupView_Mode_Perspective(double frustumx, double frustumy, double zNear, double zFar);
void GL_SetupView_Mode_PerspectiveInfiniteFarClip(double frustumx, double frustumy, double zNear);
void GL_SetupView_Mode_Ortho(double x1, double y1, double x2, double y2, double zNear, double zFar);
void GL_SetupView_ApplyCustomNearClipPlane(double normalx, double normaly, double normalz, double dist);
void GL_BlendFunc(int blendfunc1, int blendfunc2);
void GL_DepthMask(int state);
void GL_DepthTest(int state);
void GL_DepthRange(float nearfrac, float farfrac);
void GL_PolygonOffset(float planeoffset, float depthoffset);
void GL_CullFace(int state);
void GL_AlphaTest(int state);
void GL_ColorMask(int r, int g, int b, int a);
void GL_Color(float cr, float cg, float cb, float ca);
void GL_TransformToScreen(const vec4_t in, vec4_t out);
void GL_LockArrays(int first, int count);
void GL_ActiveTexture(unsigned int num);
void GL_ClientActiveTexture(unsigned int num);
void GL_Scissor(int x, int y, int width, int height);
void GL_ScissorTest(int state);
void GL_Clear(int mask);

unsigned int GL_Backend_CompileProgram(int vertexstrings_count, const char **vertexstrings_list, int geometrystrings_count, const char **geometrystrings_list, int fragmentstrings_count, const char **fragmentstrings_list);
void GL_Backend_FreeProgram(unsigned int prog);

extern cvar_t gl_lockarrays;
extern cvar_t gl_mesh_copyarrays;
extern cvar_t gl_paranoid;
extern cvar_t gl_printcheckerror;

//input to R_Mesh_TextureState
typedef struct rmeshstate_s
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
	// matrices
	matrix4x4_t texmatrix[MAX_TEXTUREUNITS];
	// pointers
	const float *pointer_texcoord[MAX_TEXTUREUNITS]; // 2D
	const float *pointer_texcoord3f[MAX_TEXTUREUNITS]; // 3D
	int pointer_texcoord_bufferobject[MAX_TEXTUREUNITS]; // 2D and 3D
	size_t pointer_texcoord_bufferoffset[MAX_TEXTUREUNITS]; // 2D and 3D
}
rmeshstate_t;

// adds console variables and registers the render module (only call from GL_Init)
void gl_backend_init(void);

// starts mesh rendering for the frame
void R_Mesh_Start(void);

// ends mesh rendering for the frame
// (only valid after R_Mesh_Start)
void R_Mesh_Finish(void);

// allocates a static element array buffer object
// (storing triangle data in video memory)
int R_Mesh_CreateStaticEBO(void *data, size_t size);
// frees an element array buffer object
void R_Mesh_DestroyEBO(int bufferobject);
// allocates a static vertex/element array buffer object
// (storing vertex or element data in video memory)
// target is GL_ELEMENT_ARRAY_BUFFER_ARB (triangle elements)
// or GL_ARRAY_BUFFER_ARB (vertex data)
int R_Mesh_CreateStaticBufferObject(unsigned int target, void *data, size_t size, const char *name);
// frees a vertex/element array buffer object
void R_Mesh_DestroyBufferObject(int bufferobject);
void GL_Mesh_ListVBOs(qboolean printeach);

// sets up the requested vertex transform matrix
void R_Mesh_Matrix(const matrix4x4_t *matrix);
// sets the vertex array pointer
void R_Mesh_VertexPointer(const float *vertex3f, int bufferobject, size_t bufferoffset);
// sets the color array pointer (GL_Color only works when this is NULL)
void R_Mesh_ColorPointer(const float *color4f, int bufferobject, size_t bufferoffset);
// sets the texcoord array pointer for an array unit
void R_Mesh_TexCoordPointer(unsigned int unitnum, unsigned int numcomponents, const float *texcoord, int bufferobject, size_t bufferoffset);
// sets all textures bound to an image unit (multiple can be non-zero at once, according to OpenGL rules the highest one overrides the others)
void R_Mesh_TexBindAll(unsigned int unitnum, int tex1d, int tex2d, int tex3d, int texcubemap);
// sets these are like TexBindAll with only one of the texture indices non-zero
// (binds one texture type and unbinds all other types)
void R_Mesh_TexBind1D(unsigned int unitnum, int texnum);
void R_Mesh_TexBind(unsigned int unitnum, int texnum);
void R_Mesh_TexBind3D(unsigned int unitnum, int texnum);
void R_Mesh_TexBindCubeMap(unsigned int unitnum, int texnum);
// sets the texcoord matrix for a texenv unit
void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix);
// sets the combine state for a texenv unit
void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale);
// set up the requested texture state
void R_Mesh_TextureState(const rmeshstate_t *m);
// set up a blank texture state (faster/easier specialized version of R_Mesh_TextureState)
void R_Mesh_ResetTextureState(void);

// renders a mesh
void R_Mesh_Draw(int firstvertex, int numvertices, int numtriangles, const int *elements, int bufferobject, size_t bufferoffset);
// renders a mesh as lines
void R_Mesh_Draw_ShowTris(int firstvertex, int numvertices, int numtriangles, const int *elements);

// saves a section of the rendered frame to a .tga or .jpg file
qboolean SCR_ScreenShot(char *filename, unsigned char *buffer1, unsigned char *buffer2, unsigned char *buffer3, int x, int y, int width, int height, qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean jpeg, qboolean gammacorrect);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(void);
// invoke refresh of loading plaque (nothing else seen)
void SCR_UpdateLoadingScreen(qboolean clear);

#endif

