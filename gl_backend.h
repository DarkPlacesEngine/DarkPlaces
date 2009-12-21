
#ifndef GL_BACKEND_H
#define GL_BACKEND_H

#define POLYGONELEMENTS_MAXPOINTS 258
extern int polygonelement3i[(POLYGONELEMENTS_MAXPOINTS-2)*3];
extern unsigned short polygonelement3s[(POLYGONELEMENTS_MAXPOINTS-2)*3];
#define QUADELEMENTS_MAXQUADS 128
extern int quadelement3i[QUADELEMENTS_MAXQUADS*6];
extern unsigned short quadelement3s[QUADELEMENTS_MAXQUADS*6];

void R_Viewport_TransformToScreen(const r_viewport_t *v, const vec4_t in, vec4_t out);
void R_Viewport_InitOrtho(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float x1, float y1, float x2, float y2, float zNear, float zFar, const float *nearplane);
void R_Viewport_InitPerspective(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float frustumx, float frustumy, float zNear, float zFar, const float *nearplane);
void R_Viewport_InitPerspectiveInfinite(r_viewport_t *v, const matrix4x4_t *cameramatrix, int x, int y, int width, int height, float frustumx, float frustumy, float zNear, const float *nearplane);
void R_Viewport_InitCubeSideView(r_viewport_t *v, const matrix4x4_t *cameramatrix, int side, int size, float nearclip, float farclip, const float *nearplane);
void R_Viewport_InitRectSideView(r_viewport_t *v, const matrix4x4_t *cameramatrix, int side, int size, int border, float nearclip, float farclip, const float *nearplane);
void R_SetViewport(const r_viewport_t *v);
void R_GetViewport(r_viewport_t *v);

void GL_BlendFunc(int blendfunc1, int blendfunc2);
void GL_DepthMask(int state);
void GL_DepthTest(int state);
void GL_DepthRange(float nearfrac, float farfrac);
void GL_PolygonOffset(float planeoffset, float depthoffset);
void GL_CullFace(int state);
void GL_AlphaTest(int state);
void GL_ColorMask(int r, int g, int b, int a);
void GL_Color(float cr, float cg, float cb, float ca);
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
// returns current texture bound to this identifier
int R_Mesh_TexBound(unsigned int unitnum, int id);
// sets all textures bound to an image unit (multiple can be non-zero at once, according to OpenGL rules the highest one overrides the others)
void R_Mesh_TexBindAll(unsigned int unitnum, int tex2d, int tex3d, int texcubemap, int texrectangle);
// equivalent to R_Mesh_TexBindAll(unitnum,tex2d,0,0,0)
void R_Mesh_TexBind(unsigned int unitnum, int texnum);
// sets the texcoord matrix for a texenv unit, can be NULL or blank (will use identity)
void R_Mesh_TexMatrix(unsigned int unitnum, const matrix4x4_t *matrix);
// sets the combine state for a texenv unit
void R_Mesh_TexCombine(unsigned int unitnum, int combinergb, int combinealpha, int rgbscale, int alphascale);
// set up a blank texture state (unbinds all textures, texcoord pointers, and resets combine settings)
void R_Mesh_ResetTextureState(void);

// renders a mesh
void R_Mesh_Draw(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const unsigned short *element3s, int bufferobject3i, int bufferobject3s);

// saves a section of the rendered frame to a .tga or .jpg file
qboolean SCR_ScreenShot(char *filename, unsigned char *buffer1, unsigned char *buffer2, unsigned char *buffer3, int x, int y, int width, int height, qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean jpeg, qboolean gammacorrect);
// used by R_Envmap_f and internally in backend, clears the frame
void R_ClearScreen(qboolean fogcolor);

#endif

