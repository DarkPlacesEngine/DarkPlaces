
#include "quakedef.h"
#include "cdaudio.h"

// global video state
viddef_t vid;

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// AK FIXME -> input_dest
qboolean in_client_mouse = true;

// AK where should it be placed ?
float in_mouse_x, in_mouse_y;

// value of GL_MAX_TEXTURE_<various>_SIZE
int gl_max_texture_size = 0;
int gl_max_3d_texture_size = 0;
int gl_max_cube_map_texture_size = 0;
// GL_ARB_multitexture
int gl_textureunits = 1;
// GL_ARB_texture_env_combine or GL_EXT_texture_env_combine
int gl_combine_extension = false;
// GL_EXT_compiled_vertex_array
int gl_supportslockarrays = false;
// GLX_SGI_swap_control or WGL_EXT_swap_control
int gl_videosyncavailable = false;
// stencil available
int gl_stencil = false;
// 3D textures available
int gl_texture3d = false;
// GL_ARB_texture_cubemap
int gl_texturecubemap = false;
// GL_ARB_texture_env_dot3
int gl_dot3arb = false;
// GL_SGIS_texture_edge_clamp
int gl_support_clamptoedge = false;
// GL_EXT_texture_filter_anisotropic
int gl_support_anisotropy = false;
int gl_max_anisotropy = 1;
// GL_EXT_stencil_two_side
int gl_support_stenciltwoside = false;
// GL_ARB_shader_objects
int gl_support_shader_objects = false;
// GL_ARB_shading_language_100
int gl_support_shading_language_100 = false;
// GL_ARB_vertex_shader
int gl_support_vertex_shader = false;
// GL_ARB_fragment_shader
int gl_support_fragment_shader = false;
// GL_NV_half_float
int gl_support_half_float = false;

// LordHavoc: if window is hidden, don't update screen
qboolean vid_hidden = true;
// LordHavoc: if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
qboolean vid_activewindow = true;

// we don't know until we try it!
cvar_t vid_hardwaregammasupported = {CVAR_READONLY,"vid_hardwaregammasupported","1", "indicates whether hardware gamma is supported (updated by attempts to set hardware gamma ramps)"};
// whether hardware gamma ramps are currently in effect
qboolean vid_usinghwgamma = false;

unsigned short vid_gammaramps[768];
unsigned short vid_systemgammaramps[768];

cvar_t vid_fullscreen = {CVAR_SAVE, "vid_fullscreen", "1", "use fullscreen (1) or windowed (0)"};
cvar_t vid_width = {CVAR_SAVE, "vid_width", "640", "resolution"};
cvar_t vid_height = {CVAR_SAVE, "vid_height", "480", "resolution"};
cvar_t vid_bitsperpixel = {CVAR_SAVE, "vid_bitsperpixel", "32", "how many bits per pixel to render at (32 or 16, 32 is recommended)"};
cvar_t vid_refreshrate = {CVAR_SAVE, "vid_refreshrate", "60", "refresh rate to use, in hz (higher values flicker less, if supported by your monitor)"};

cvar_t vid_vsync = {CVAR_SAVE, "vid_vsync", "0", "sync to vertical blank, prevents 'tearing' (seeing part of one frame and part of another on the screen at the same time), automatically disabled when doing timedemo benchmarks"};
cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1", "whether to use the mouse in windowed mode (fullscreen always does)"};
cvar_t vid_minwidth = {0, "vid_minwidth", "0", "minimum vid_width that is acceptable (to be set in default.cfg in mods)"};
cvar_t vid_minheight = {0, "vid_minheight", "0", "minimum vid_height that is acceptable (to be set in default.cfg in mods)"};
cvar_t gl_combine = {0, "gl_combine", "1", "faster rendering by using GL_ARB_texture_env_combine extension (part of OpenGL 1.3 and above)"};
cvar_t gl_finish = {0, "gl_finish", "0", "make the cpu wait for the graphics processor at the end of each frame render (can help with strange input or video lag problems on some machines)"};

cvar_t v_gamma = {CVAR_SAVE, "v_gamma", "1", "inverse gamma correction value, a brightness effect that does not affect white or black, and tends to make the image grey and dull"};
cvar_t v_contrast = {CVAR_SAVE, "v_contrast", "1", "brightness of white (values above 1 give a brighter image with increased color saturation, unlike v_gamma)"};
cvar_t v_brightness = {CVAR_SAVE, "v_brightness", "0", "brightness of black, useful for monitors that are too dark"};
cvar_t v_color_enable = {CVAR_SAVE, "v_color_enable", "0", "enables black-grey-white color correction curve controls"};
cvar_t v_color_black_r = {CVAR_SAVE, "v_color_black_r", "0", "desired color of black"};
cvar_t v_color_black_g = {CVAR_SAVE, "v_color_black_g", "0", "desired color of black"};
cvar_t v_color_black_b = {CVAR_SAVE, "v_color_black_b", "0", "desired color of black"};
cvar_t v_color_grey_r = {CVAR_SAVE, "v_color_grey_r", "0.5", "desired color of grey"};
cvar_t v_color_grey_g = {CVAR_SAVE, "v_color_grey_g", "0.5", "desired color of grey"};
cvar_t v_color_grey_b = {CVAR_SAVE, "v_color_grey_b", "0.5", "desired color of grey"};
cvar_t v_color_white_r = {CVAR_SAVE, "v_color_white_r", "1", "desired color of white"};
cvar_t v_color_white_g = {CVAR_SAVE, "v_color_white_g", "1", "desired color of white"};
cvar_t v_color_white_b = {CVAR_SAVE, "v_color_white_b", "1", "desired color of white"};
cvar_t v_hwgamma = {CVAR_SAVE, "v_hwgamma", "1", "enables use of hardware gamma correction ramps if available (note: does not work very well on Windows2000 and above)"};
cvar_t v_psycho = {0, "v_psycho", "0", "easter egg (does not work on Windows2000 or above)"};

// brand of graphics chip
const char *gl_vendor;
// graphics chip model and other information
const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
const char *gl_version;
// extensions list, space separated
const char *gl_extensions;
// WGL, GLX, or AGL
const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
char gl_driver[256];

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord1f) (GLenum, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord3f) (GLenum, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord4f) (GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

// GL_EXT_compiled_vertex_array
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);

// general GL functions

void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void (GLAPIENTRY *qglClear)(GLbitfield mask);

void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
void (GLAPIENTRY *qglCullFace)(GLenum mode);

void (GLAPIENTRY *qglDrawBuffer)(GLenum mode);
void (GLAPIENTRY *qglReadBuffer)(GLenum mode);
void (GLAPIENTRY *qglEnable)(GLenum cap);
void (GLAPIENTRY *qglDisable)(GLenum cap);
GLboolean (GLAPIENTRY *qglIsEnabled)(GLenum cap);

void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

GLenum (GLAPIENTRY *qglGetError)(void);
const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
void (GLAPIENTRY *qglFinish)(void);
void (GLAPIENTRY *qglFlush)(void);

void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
void (GLAPIENTRY *qglDepthFunc)(GLenum func);
void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

void (GLAPIENTRY *qglDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglArrayElement)(GLint i);

void (GLAPIENTRY *qglColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void (GLAPIENTRY *qglTexCoord1f)(GLfloat s);
void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
void (GLAPIENTRY *qglTexCoord3f)(GLfloat s, GLfloat t, GLfloat r);
void (GLAPIENTRY *qglTexCoord4f)(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglBegin)(GLenum mode);
void (GLAPIENTRY *qglEnd)(void);

void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
void (GLAPIENTRY *qglPushMatrix)(void);
void (GLAPIENTRY *qglPopMatrix)(void);
void (GLAPIENTRY *qglLoadIdentity)(void);
void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
void (GLAPIENTRY *qglStencilMask)(GLuint mask);
void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
void (GLAPIENTRY *qglClearStencil)(GLint s);

void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexEnvfv)(GLenum target, GLenum pname, const GLfloat *params);
void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);
void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);

void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
//void (GLAPIENTRY *qglPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities);
//GLboolean (GLAPIENTRY *qglAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences);
GLboolean (GLAPIENTRY *qglIsTexture)(GLuint texture);
void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

void (GLAPIENTRY *qglTexImage1D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglCopyTexImage1D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void (GLAPIENTRY *qglCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);


void (GLAPIENTRY *qglDrawRangeElementsEXT)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

//void (GLAPIENTRY *qglColorTableEXT)(int, int, int, int, int, const void *);

void (GLAPIENTRY *qglTexImage3D)(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);

//[515]: added on 29.07.2005
void (GLAPIENTRY *qglLineWidth)(GLfloat width);
void (GLAPIENTRY *qglPointSize)(GLfloat size);

void (GLAPIENTRY *qglActiveStencilFaceEXT)(GLenum);

void (GLAPIENTRY *qglDeleteObjectARB)(GLhandleARB obj);
GLhandleARB (GLAPIENTRY *qglGetHandleARB)(GLenum pname);
void (GLAPIENTRY *qglDetachObjectARB)(GLhandleARB containerObj, GLhandleARB attachedObj);
GLhandleARB (GLAPIENTRY *qglCreateShaderObjectARB)(GLenum shaderType);
void (GLAPIENTRY *qglShaderSourceARB)(GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length);
void (GLAPIENTRY *qglCompileShaderARB)(GLhandleARB shaderObj);
GLhandleARB (GLAPIENTRY *qglCreateProgramObjectARB)(void);
void (GLAPIENTRY *qglAttachObjectARB)(GLhandleARB containerObj, GLhandleARB obj);
void (GLAPIENTRY *qglLinkProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUseProgramObjectARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglValidateProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUniform1fARB)(GLint location, GLfloat v0);
void (GLAPIENTRY *qglUniform2fARB)(GLint location, GLfloat v0, GLfloat v1);
void (GLAPIENTRY *qglUniform3fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (GLAPIENTRY *qglUniform4fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (GLAPIENTRY *qglUniform1iARB)(GLint location, GLint v0);
void (GLAPIENTRY *qglUniform2iARB)(GLint location, GLint v0, GLint v1);
void (GLAPIENTRY *qglUniform3iARB)(GLint location, GLint v0, GLint v1, GLint v2);
void (GLAPIENTRY *qglUniform4iARB)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void (GLAPIENTRY *qglUniform1fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform2fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform3fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform4fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform1ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform2ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform3ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform4ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniformMatrix2fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix3fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix4fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglGetObjectParameterfvARB)(GLhandleARB obj, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetObjectParameterivARB)(GLhandleARB obj, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetInfoLogARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
void (GLAPIENTRY *qglGetAttachedObjectsARB)(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
GLint (GLAPIENTRY *qglGetUniformLocationARB)(GLhandleARB programObj, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveUniformARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
void (GLAPIENTRY *qglGetUniformfvARB)(GLhandleARB programObj, GLint location, GLfloat *params);
void (GLAPIENTRY *qglGetUniformivARB)(GLhandleARB programObj, GLint location, GLint *params);
void (GLAPIENTRY *qglGetShaderSourceARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);

//void (GLAPIENTRY *qglVertexAttrib1fARB)(GLuint index, GLfloat v0);
//void (GLAPIENTRY *qglVertexAttrib1sARB)(GLuint index, GLshort v0);
//void (GLAPIENTRY *qglVertexAttrib1dARB)(GLuint index, GLdouble v0);
//void (GLAPIENTRY *qglVertexAttrib2fARB)(GLuint index, GLfloat v0, GLfloat v1);
//void (GLAPIENTRY *qglVertexAttrib2sARB)(GLuint index, GLshort v0, GLshort v1);
//void (GLAPIENTRY *qglVertexAttrib2dARB)(GLuint index, GLdouble v0, GLdouble v1);
//void (GLAPIENTRY *qglVertexAttrib3fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
//void (GLAPIENTRY *qglVertexAttrib3sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
//void (GLAPIENTRY *qglVertexAttrib3dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
//void (GLAPIENTRY *qglVertexAttrib4fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
//void (GLAPIENTRY *qglVertexAttrib4sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
//void (GLAPIENTRY *qglVertexAttrib4dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
//void (GLAPIENTRY *qglVertexAttrib4NubARB)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
//void (GLAPIENTRY *qglVertexAttrib1fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib1svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib1dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib2fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib2svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib2dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib3fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib3svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib3dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib4svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4ivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4bvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4ubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4usvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4uivARB)(GLuint index, const GLuint *v);
//void (GLAPIENTRY *qglVertexAttrib4NbvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NsvARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4NivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4NubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NusvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4NuivARB)(GLuint index, const GLuint *v);
void (GLAPIENTRY *qglVertexAttribPointerARB)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
void (GLAPIENTRY *qglEnableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglDisableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglBindAttribLocationARB)(GLhandleARB programObj, GLuint index, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveAttribARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLint (GLAPIENTRY *qglGetAttribLocationARB)(GLhandleARB programObj, const GLcharARB *name);
//void (GLAPIENTRY *qglGetVertexAttribdvARB)(GLuint index, GLenum pname, GLdouble *params);
//void (GLAPIENTRY *qglGetVertexAttribfvARB)(GLuint index, GLenum pname, GLfloat *params);
//void (GLAPIENTRY *qglGetVertexAttribivARB)(GLuint index, GLenum pname, GLint *params);
//void (GLAPIENTRY *qglGetVertexAttribPointervARB)(GLuint index, GLenum pname, GLvoid **pointer);

int GL_CheckExtension(const char *name, const dllfunction_t *funcs, const char *disableparm, int silent)
{
	int failed = false;
	const dllfunction_t *func;

	Con_Printf("checking for %s...  ", name);

	for (func = funcs;func && func->name;func++)
		*func->funcvariable = NULL;

	if (disableparm && (COM_CheckParm(disableparm) || COM_CheckParm("-safe")))
	{
		Con_Print("disabled by commandline\n");
		return false;
	}

	if (strstr(gl_extensions, name) || strstr(gl_platformextensions, name) || (strncmp(name, "GL_", 3) && strncmp(name, "WGL_", 4) && strncmp(name, "GLX_", 4) && strncmp(name, "AGL_", 4)))
	{
		for (func = funcs;func && func->name != NULL;func++)
		{
			// functions are cleared before all the extensions are evaluated
			if (!(*func->funcvariable = (void *) GL_GetProcAddress(func->name)))
			{
				if (!silent)
					Con_Printf("OpenGL extension \"%s\" is missing function \"%s\" - broken driver!\n", name, func->name);
				failed = true;
			}
		}
		// delay the return so it prints all missing functions
		if (failed)
			return false;
		Con_Print("enabled\n");
		return true;
	}
	else
	{
		Con_Print("not detected\n");
		return false;
	}
}

static dllfunction_t opengl110funcs[] =
{
	{"glClearColor", (void **) &qglClearColor},
	{"glClear", (void **) &qglClear},
	{"glAlphaFunc", (void **) &qglAlphaFunc},
	{"glBlendFunc", (void **) &qglBlendFunc},
	{"glCullFace", (void **) &qglCullFace},
	{"glDrawBuffer", (void **) &qglDrawBuffer},
	{"glReadBuffer", (void **) &qglReadBuffer},
	{"glEnable", (void **) &qglEnable},
	{"glDisable", (void **) &qglDisable},
	{"glIsEnabled", (void **) &qglIsEnabled},
	{"glEnableClientState", (void **) &qglEnableClientState},
	{"glDisableClientState", (void **) &qglDisableClientState},
	{"glGetBooleanv", (void **) &qglGetBooleanv},
	{"glGetDoublev", (void **) &qglGetDoublev},
	{"glGetFloatv", (void **) &qglGetFloatv},
	{"glGetIntegerv", (void **) &qglGetIntegerv},
	{"glGetError", (void **) &qglGetError},
	{"glGetString", (void **) &qglGetString},
	{"glFinish", (void **) &qglFinish},
	{"glFlush", (void **) &qglFlush},
	{"glClearDepth", (void **) &qglClearDepth},
	{"glDepthFunc", (void **) &qglDepthFunc},
	{"glDepthMask", (void **) &qglDepthMask},
	{"glDepthRange", (void **) &qglDepthRange},
	{"glDrawElements", (void **) &qglDrawElements},
	{"glColorMask", (void **) &qglColorMask},
	{"glVertexPointer", (void **) &qglVertexPointer},
	{"glNormalPointer", (void **) &qglNormalPointer},
	{"glColorPointer", (void **) &qglColorPointer},
	{"glTexCoordPointer", (void **) &qglTexCoordPointer},
	{"glArrayElement", (void **) &qglArrayElement},
	{"glColor4f", (void **) &qglColor4f},
	{"glTexCoord1f", (void **) &qglTexCoord1f},
	{"glTexCoord2f", (void **) &qglTexCoord2f},
	{"glTexCoord3f", (void **) &qglTexCoord3f},
	{"glTexCoord4f", (void **) &qglTexCoord4f},
	{"glVertex2f", (void **) &qglVertex2f},
	{"glVertex3f", (void **) &qglVertex3f},
	{"glBegin", (void **) &qglBegin},
	{"glEnd", (void **) &qglEnd},
//[515]: added on 29.07.2005
	{"glLineWidth", (void**) &qglLineWidth},
	{"glPointSize", (void**) &qglPointSize},
//
	{"glMatrixMode", (void **) &qglMatrixMode},
	{"glOrtho", (void **) &qglOrtho},
	{"glFrustum", (void **) &qglFrustum},
	{"glViewport", (void **) &qglViewport},
	{"glPushMatrix", (void **) &qglPushMatrix},
	{"glPopMatrix", (void **) &qglPopMatrix},
	{"glLoadIdentity", (void **) &qglLoadIdentity},
	{"glLoadMatrixd", (void **) &qglLoadMatrixd},
	{"glLoadMatrixf", (void **) &qglLoadMatrixf},
	{"glMultMatrixd", (void **) &qglMultMatrixd},
	{"glMultMatrixf", (void **) &qglMultMatrixf},
	{"glRotated", (void **) &qglRotated},
	{"glRotatef", (void **) &qglRotatef},
	{"glScaled", (void **) &qglScaled},
	{"glScalef", (void **) &qglScalef},
	{"glTranslated", (void **) &qglTranslated},
	{"glTranslatef", (void **) &qglTranslatef},
	{"glReadPixels", (void **) &qglReadPixels},
	{"glStencilFunc", (void **) &qglStencilFunc},
	{"glStencilMask", (void **) &qglStencilMask},
	{"glStencilOp", (void **) &qglStencilOp},
	{"glClearStencil", (void **) &qglClearStencil},
	{"glTexEnvf", (void **) &qglTexEnvf},
	{"glTexEnvfv", (void **) &qglTexEnvfv},
	{"glTexEnvi", (void **) &qglTexEnvi},
	{"glTexParameterf", (void **) &qglTexParameterf},
	{"glTexParameterfv", (void **) &qglTexParameterfv},
	{"glTexParameteri", (void **) &qglTexParameteri},
	{"glPixelStoref", (void **) &qglPixelStoref},
	{"glPixelStorei", (void **) &qglPixelStorei},
	{"glGenTextures", (void **) &qglGenTextures},
	{"glDeleteTextures", (void **) &qglDeleteTextures},
	{"glBindTexture", (void **) &qglBindTexture},
//	{"glPrioritizeTextures", (void **) &qglPrioritizeTextures},
//	{"glAreTexturesResident", (void **) &qglAreTexturesResident},
	{"glIsTexture", (void **) &qglIsTexture},
	{"glTexImage1D", (void **) &qglTexImage1D},
	{"glTexImage2D", (void **) &qglTexImage2D},
	{"glTexSubImage1D", (void **) &qglTexSubImage1D},
	{"glTexSubImage2D", (void **) &qglTexSubImage2D},
	{"glCopyTexImage1D", (void **) &qglCopyTexImage1D},
	{"glCopyTexImage2D", (void **) &qglCopyTexImage2D},
	{"glCopyTexSubImage1D", (void **) &qglCopyTexSubImage1D},
	{"glCopyTexSubImage2D", (void **) &qglCopyTexSubImage2D},
	{"glScissor", (void **) &qglScissor},
	{"glPolygonOffset", (void **) &qglPolygonOffset},
	{NULL, NULL}
};

static dllfunction_t drawrangeelementsfuncs[] =
{
	{"glDrawRangeElements", (void **) &qglDrawRangeElements},
	{NULL, NULL}
};

static dllfunction_t drawrangeelementsextfuncs[] =
{
	{"glDrawRangeElementsEXT", (void **) &qglDrawRangeElementsEXT},
	{NULL, NULL}
};

static dllfunction_t multitexturefuncs[] =
{
	{"glMultiTexCoord1fARB", (void **) &qglMultiTexCoord1f},
	{"glMultiTexCoord2fARB", (void **) &qglMultiTexCoord2f},
	{"glMultiTexCoord3fARB", (void **) &qglMultiTexCoord3f},
	{"glMultiTexCoord4fARB", (void **) &qglMultiTexCoord4f},
	{"glActiveTextureARB", (void **) &qglActiveTexture},
	{"glClientActiveTextureARB", (void **) &qglClientActiveTexture},
	{NULL, NULL}
};

static dllfunction_t compiledvertexarrayfuncs[] =
{
	{"glLockArraysEXT", (void **) &qglLockArraysEXT},
	{"glUnlockArraysEXT", (void **) &qglUnlockArraysEXT},
	{NULL, NULL}
};

static dllfunction_t texture3dextfuncs[] =
{
	{"glTexImage3DEXT", (void **) &qglTexImage3D},
	{"glTexSubImage3DEXT", (void **) &qglTexSubImage3D},
	{"glCopyTexSubImage3DEXT", (void **) &qglCopyTexSubImage3D},
	{NULL, NULL}
};

static dllfunction_t stenciltwosidefuncs[] =
{
	{"glActiveStencilFaceEXT", (void **) &qglActiveStencilFaceEXT},
	{NULL, NULL}
};

static dllfunction_t shaderobjectsfuncs[] =
{
	{"glDeleteObjectARB", (void **) &qglDeleteObjectARB},
	{"glGetHandleARB", (void **) &qglGetHandleARB},
	{"glDetachObjectARB", (void **) &qglDetachObjectARB},
	{"glCreateShaderObjectARB", (void **) &qglCreateShaderObjectARB},
	{"glShaderSourceARB", (void **) &qglShaderSourceARB},
	{"glCompileShaderARB", (void **) &qglCompileShaderARB},
	{"glCreateProgramObjectARB", (void **) &qglCreateProgramObjectARB},
	{"glAttachObjectARB", (void **) &qglAttachObjectARB},
	{"glLinkProgramARB", (void **) &qglLinkProgramARB},
	{"glUseProgramObjectARB", (void **) &qglUseProgramObjectARB},
	{"glValidateProgramARB", (void **) &qglValidateProgramARB},
	{"glUniform1fARB", (void **) &qglUniform1fARB},
	{"glUniform2fARB", (void **) &qglUniform2fARB},
	{"glUniform3fARB", (void **) &qglUniform3fARB},
	{"glUniform4fARB", (void **) &qglUniform4fARB},
	{"glUniform1iARB", (void **) &qglUniform1iARB},
	{"glUniform2iARB", (void **) &qglUniform2iARB},
	{"glUniform3iARB", (void **) &qglUniform3iARB},
	{"glUniform4iARB", (void **) &qglUniform4iARB},
	{"glUniform1fvARB", (void **) &qglUniform1fvARB},
	{"glUniform2fvARB", (void **) &qglUniform2fvARB},
	{"glUniform3fvARB", (void **) &qglUniform3fvARB},
	{"glUniform4fvARB", (void **) &qglUniform4fvARB},
	{"glUniform1ivARB", (void **) &qglUniform1ivARB},
	{"glUniform2ivARB", (void **) &qglUniform2ivARB},
	{"glUniform3ivARB", (void **) &qglUniform3ivARB},
	{"glUniform4ivARB", (void **) &qglUniform4ivARB},
	{"glUniformMatrix2fvARB", (void **) &qglUniformMatrix2fvARB},
	{"glUniformMatrix3fvARB", (void **) &qglUniformMatrix3fvARB},
	{"glUniformMatrix4fvARB", (void **) &qglUniformMatrix4fvARB},
	{"glGetObjectParameterfvARB", (void **) &qglGetObjectParameterfvARB},
	{"glGetObjectParameterivARB", (void **) &qglGetObjectParameterivARB},
	{"glGetInfoLogARB", (void **) &qglGetInfoLogARB},
	{"glGetAttachedObjectsARB", (void **) &qglGetAttachedObjectsARB},
	{"glGetUniformLocationARB", (void **) &qglGetUniformLocationARB},
	{"glGetActiveUniformARB", (void **) &qglGetActiveUniformARB},
	{"glGetUniformfvARB", (void **) &qglGetUniformfvARB},
	{"glGetUniformivARB", (void **) &qglGetUniformivARB},
	{"glGetShaderSourceARB", (void **) &qglGetShaderSourceARB},
	{NULL, NULL}
};

static dllfunction_t vertexshaderfuncs[] =
{
//	{"glVertexAttrib1fARB", (void **) &qglVertexAttrib1fARB},
//	{"glVertexAttrib1sARB", (void **) &qglVertexAttrib1sARB},
//	{"glVertexAttrib1dARB", (void **) &qglVertexAttrib1dARB},
//	{"glVertexAttrib2fARB", (void **) &qglVertexAttrib2fARB},
//	{"glVertexAttrib2sARB", (void **) &qglVertexAttrib2sARB},
//	{"glVertexAttrib2dARB", (void **) &qglVertexAttrib2dARB},
//	{"glVertexAttrib3fARB", (void **) &qglVertexAttrib3fARB},
//	{"glVertexAttrib3sARB", (void **) &qglVertexAttrib3sARB},
//	{"glVertexAttrib3dARB", (void **) &qglVertexAttrib3dARB},
//	{"glVertexAttrib4fARB", (void **) &qglVertexAttrib4fARB},
//	{"glVertexAttrib4sARB", (void **) &qglVertexAttrib4sARB},
//	{"glVertexAttrib4dARB", (void **) &qglVertexAttrib4dARB},
//	{"glVertexAttrib4NubARB", (void **) &qglVertexAttrib4NubARB},
//	{"glVertexAttrib1fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib1svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib1dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib2fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib2svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib2dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib3fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib3svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib3dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib4svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib4dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4ivARB", (void **) &qglVertexAttrib1ivARB},
//	{"glVertexAttrib4bvARB", (void **) &qglVertexAttrib1bvARB},
//	{"glVertexAttrib4ubvARB", (void **) &qglVertexAttrib1ubvARB},
//	{"glVertexAttrib4usvARB", (void **) &qglVertexAttrib1usvARB},
//	{"glVertexAttrib4uivARB", (void **) &qglVertexAttrib1uivARB},
//	{"glVertexAttrib4NbvARB", (void **) &qglVertexAttrib1NbvARB},
//	{"glVertexAttrib4NsvARB", (void **) &qglVertexAttrib1NsvARB},
//	{"glVertexAttrib4NivARB", (void **) &qglVertexAttrib1NivARB},
//	{"glVertexAttrib4NubvARB", (void **) &qglVertexAttrib1NubvARB},
//	{"glVertexAttrib4NusvARB", (void **) &qglVertexAttrib1NusvARB},
//	{"glVertexAttrib4NuivARB", (void **) &qglVertexAttrib1NuivARB},
	{"glVertexAttribPointerARB", (void **) &qglVertexAttribPointerARB},
	{"glEnableVertexAttribArrayARB", (void **) &qglEnableVertexAttribArrayARB},
	{"glDisableVertexAttribArrayARB", (void **) &qglDisableVertexAttribArrayARB},
	{"glBindAttribLocationARB", (void **) &qglBindAttribLocationARB},
	{"glGetActiveAttribARB", (void **) &qglGetActiveAttribARB},
	{"glGetAttribLocationARB", (void **) &qglGetAttribLocationARB},
//	{"glGetVertexAttribdvARB", (void **) &qglGetVertexAttribdvARB},
//	{"glGetVertexAttribfvARB", (void **) &qglGetVertexAttribfvARB},
//	{"glGetVertexAttribivARB", (void **) &qglGetVertexAttribivARB},
//	{"glGetVertexAttribPointervARB", (void **) &qglGetVertexAttribPointervARB},
	{NULL, NULL}
};


void VID_CheckExtensions(void)
{
	gl_stencil = vid_bitsperpixel.integer == 32;

	// reset all the gl extension variables here
	// this should match the declarations
	gl_max_texture_size = 0;
	gl_max_3d_texture_size = 0;
	gl_max_cube_map_texture_size = 0;
	gl_textureunits = 1;
	gl_combine_extension = false;
	gl_supportslockarrays = false;
	gl_texture3d = false;
	gl_texturecubemap = false;
	gl_dot3arb = false;
	gl_support_clamptoedge = false;
	gl_support_anisotropy = false;
	gl_max_anisotropy = 1;
	gl_support_stenciltwoside = false;
	gl_support_shader_objects = false;
	gl_support_shading_language_100 = false;
	gl_support_vertex_shader = false;
	gl_support_fragment_shader = false;

	if (!GL_CheckExtension("OpenGL 1.1.0", opengl110funcs, NULL, false))
		Sys_Error("OpenGL 1.1.0 functions not found");

	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_Printf("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);

	Con_Print("Checking OpenGL extensions...\n");

// COMMANDLINEOPTION: GL: -nodrawrangeelements disables GL_EXT_draw_range_elements (renders faster)
	if (!GL_CheckExtension("glDrawRangeElements", drawrangeelementsfuncs, "-nodrawrangeelements", true))
		GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "-nodrawrangeelements", false);

// COMMANDLINEOPTION: GL: -nomtex disables GL_ARB_multitexture (required for faster map rendering)
	if (GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false))
	{
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
// COMMANDLINEOPTION: GL: -nocombine disables GL_ARB_texture_env_combine or GL_EXT_texture_env_combine (required for bumpmapping and faster map rendering)
		gl_combine_extension = GL_CheckExtension("GL_ARB_texture_env_combine", NULL, "-nocombine", false) || GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "-nocombine", false);
// COMMANDLINEOPTION: GL: -nodot3 disables GL_ARB_texture_env_dot3 (required for bumpmapping)
		if (gl_combine_extension)
			gl_dot3arb = GL_CheckExtension("GL_ARB_texture_env_dot3", NULL, "-nodot3", false);
	}

// COMMANDLINEOPTION: GL: -notexture3d disables GL_EXT_texture3D (required for spherical lights, otherwise they render as a column)
	if ((gl_texture3d = GL_CheckExtension("GL_EXT_texture3D", texture3dextfuncs, "-notexture3d", false)))
		qglGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &gl_max_3d_texture_size);
// COMMANDLINEOPTION: GL: -nocubemap disables GL_ARB_texture_cube_map (required for bumpmapping)
	if ((gl_texturecubemap = GL_CheckExtension("GL_ARB_texture_cube_map", NULL, "-nocubemap", false)))
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &gl_max_cube_map_texture_size);
// COMMANDLINEOPTION: GL: -nocva disables GL_EXT_compiled_vertex_array (renders faster)
	gl_supportslockarrays = GL_CheckExtension("GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "-nocva", false);
// COMMANDLINEOPTION: GL: -noedgeclamp disables GL_EXT_texture_edge_clamp or GL_SGIS_texture_edge_clamp (recommended, some cards do not support the other texture clamp method)
	gl_support_clamptoedge = GL_CheckExtension("GL_EXT_texture_edge_clamp", NULL, "-noedgeclamp", false) || GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, "-noedgeclamp", false);

// COMMANDLINEOPTION: GL: -noanisotropy disables GL_EXT_texture_filter_anisotropic (allows higher quality texturing)
	if ((gl_support_anisotropy = GL_CheckExtension("GL_EXT_texture_filter_anisotropic", NULL, "-noanisotropy", false)))
		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);

// COMMANDLINEOPTION: GL: -nostenciltwoside disables GL_EXT_stencil_two_side (accelerates shadow rendering)
	gl_support_stenciltwoside = GL_CheckExtension("GL_EXT_stencil_two_side", stenciltwosidefuncs, "-nostenciltwoside", false);

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if (qglDrawRangeElements == NULL)
		qglDrawRangeElements = qglDrawRangeElementsEXT;

// COMMANDLINEOPTION: GL: -noshaderobjects disables GL_ARB_shader_objects (required for vertex shader and fragment shader)
// COMMANDLINEOPTION: GL: -noshadinglanguage100 disables GL_ARB_shading_language_100 (required for vertex shader and fragment shader)
// COMMANDLINEOPTION: GL: -novertexshader disables GL_ARB_vertex_shader (allows vertex shader effects)
// COMMANDLINEOPTION: GL: -nofragmentshader disables GL_ARB_fragment_shader (allows pixel shader effects, can improve per pixel lighting performance and capabilities)
	if ((gl_support_shader_objects = GL_CheckExtension("GL_ARB_shader_objects", shaderobjectsfuncs, "-noshaderobjects", false)))
		if ((gl_support_shading_language_100 = GL_CheckExtension("GL_ARB_shading_language_100", NULL, "-noshadinglanguage100", false)))
			if ((gl_support_vertex_shader = GL_CheckExtension("GL_ARB_vertex_shader", vertexshaderfuncs, "-novertexshader", false)))
				gl_support_fragment_shader = GL_CheckExtension("GL_ARB_fragment_shader", NULL, "-nofragmentshader", false);

// COMMANDLINEOPTION: GL: -nohalffloat disables GL_NV_half_float extension
	gl_support_half_float = GL_CheckExtension("GL_NV_half_float", NULL, "-nohalffloat", false);
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

static float cachegamma, cachebrightness, cachecontrast, cacheblack[3], cachegrey[3], cachewhite[3];
static int cachecolorenable, cachehwgamma;
#define BOUNDCVAR(cvar, m1, m2) c = &(cvar);f = bound(m1, c->value, m2);if (c->value != f) Cvar_SetValueQuick(c, f);
void VID_UpdateGamma(qboolean force)
{
	cvar_t *c;
	float f;
	static int forcenextframe = false;

	// LordHavoc: don't mess with gamma tables if running dedicated
	if (cls.state == ca_dedicated)
		return;

	if (!force
	 && !forcenextframe
	 && !v_psycho.integer
	 && vid_usinghwgamma == (vid_activewindow && v_hwgamma.integer)
	 && v_gamma.value == cachegamma
	 && v_contrast.value == cachecontrast
	 && v_brightness.value == cachebrightness
	 && cachecolorenable == v_color_enable.integer
	 && cacheblack[0] == v_color_black_r.value
	 && cacheblack[1] == v_color_black_g.value
	 && cacheblack[2] == v_color_black_b.value
	 && cachegrey[0] == v_color_grey_r.value
	 && cachegrey[1] == v_color_grey_g.value
	 && cachegrey[2] == v_color_grey_b.value
	 && cachewhite[0] == v_color_white_r.value
	 && cachewhite[1] == v_color_white_g.value
	 && cachewhite[2] == v_color_white_b.value)
		return;

	forcenextframe = false;

	if (vid_activewindow && v_hwgamma.integer)
	{
		if (!vid_usinghwgamma)
		{
			vid_usinghwgamma = true;
			Cvar_SetValueQuick(&vid_hardwaregammasupported, VID_GetGamma(vid_systemgammaramps));
		}

		BOUNDCVAR(v_gamma, 0.1, 5);cachegamma = v_gamma.value;
		BOUNDCVAR(v_contrast, 1, 5);cachecontrast = v_contrast.value;
		BOUNDCVAR(v_brightness, 0, 0.8);cachebrightness = v_brightness.value;
		BOUNDCVAR(v_color_black_r, 0, 0.8);cacheblack[0] = v_color_black_r.value;
		BOUNDCVAR(v_color_black_g, 0, 0.8);cacheblack[1] = v_color_black_g.value;
		BOUNDCVAR(v_color_black_b, 0, 0.8);cacheblack[2] = v_color_black_b.value;
		BOUNDCVAR(v_color_grey_r, 0, 0.95);cachegrey[0] = v_color_grey_r.value;
		BOUNDCVAR(v_color_grey_g, 0, 0.95);cachegrey[1] = v_color_grey_g.value;
		BOUNDCVAR(v_color_grey_b, 0, 0.95);cachegrey[2] = v_color_grey_b.value;
		BOUNDCVAR(v_color_white_r, 1, 5);cachewhite[0] = v_color_white_r.value;
		BOUNDCVAR(v_color_white_g, 1, 5);cachewhite[1] = v_color_white_g.value;
		BOUNDCVAR(v_color_white_b, 1, 5);cachewhite[2] = v_color_white_b.value;
		cachecolorenable = v_color_enable.integer;
		cachehwgamma = v_hwgamma.integer;

		if (cachecolorenable)
		{
			BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[0]), cachewhite[0], cacheblack[0], vid_gammaramps);
			BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[1]), cachewhite[1], cacheblack[1], vid_gammaramps + 256);
			BuildGammaTable16(1.0f, invpow(0.5, 1 - cachegrey[2]), cachewhite[2], cacheblack[2], vid_gammaramps + 512);
		}
		else
		{
			BuildGammaTable16(1.0f, cachegamma, cachecontrast, cachebrightness, vid_gammaramps);
			BuildGammaTable16(1.0f, cachegamma, cachecontrast, cachebrightness, vid_gammaramps + 256);
			BuildGammaTable16(1.0f, cachegamma, cachecontrast, cachebrightness, vid_gammaramps + 512);
		}

		// LordHavoc: this code came from Ben Winslow and Zinx Verituse, I have
		// immensely butchered it to work with variable framerates and fit in with
		// the rest of darkplaces.
		if (v_psycho.integer)
		{
			int x, y;
			float t;
			static float n[3], nd[3], nt[3];
			static int init = true;
			unsigned short *ramp;
			forcenextframe = true;
			if (init)
			{
				init = false;
				for (x = 0;x < 3;x++)
				{
					n[x] = lhrandom(0, 1);
					nd[x] = (rand()&1)?-0.25:0.25;
					nt[x] = lhrandom(1, 8.2);
				}
			}

			for (x = 0;x < 3;x++)
			{
				nt[x] -= host_realframetime;
				if (nt[x] < 0)
				{
					nd[x] = -nd[x];
					nt[x] += lhrandom(1, 8.2);
				}
				n[x] += nd[x] * host_realframetime;
				n[x] -= floor(n[x]);
			}

			for (x = 0, ramp = vid_gammaramps;x < 3;x++)
				for (y = 0, t = n[x] - 0.75f;y < 256;y++, t += 0.75f * (2.0f / 256.0f))
					*ramp++ = cos(t*(M_PI*2.0)) * 32767.0f + 32767.0f;
		}

		Cvar_SetValueQuick(&vid_hardwaregammasupported, VID_SetGamma(vid_gammaramps));
		// if custom gamma ramps failed (Windows stupidity), restore to system gamma
		if(!vid_hardwaregammasupported.integer)
			VID_SetGamma(vid_systemgammaramps);
	}
	else
	{
		if (vid_usinghwgamma)
		{
			vid_usinghwgamma = false;
			Cvar_SetValueQuick(&vid_hardwaregammasupported, VID_SetGamma(vid_systemgammaramps));
		}
	}
}

void VID_RestoreSystemGamma(void)
{
	if (vid_usinghwgamma)
	{
		vid_usinghwgamma = false;
		VID_SetGamma(vid_systemgammaramps);
	}
}

void VID_Shared_Init(void)
{
	Cvar_RegisterVariable(&vid_hardwaregammasupported);
	Cvar_RegisterVariable(&v_gamma);
	Cvar_RegisterVariable(&v_brightness);
	Cvar_RegisterVariable(&v_contrast);

	Cvar_RegisterVariable(&v_color_enable);
	Cvar_RegisterVariable(&v_color_black_r);
	Cvar_RegisterVariable(&v_color_black_g);
	Cvar_RegisterVariable(&v_color_black_b);
	Cvar_RegisterVariable(&v_color_grey_r);
	Cvar_RegisterVariable(&v_color_grey_g);
	Cvar_RegisterVariable(&v_color_grey_b);
	Cvar_RegisterVariable(&v_color_white_r);
	Cvar_RegisterVariable(&v_color_white_g);
	Cvar_RegisterVariable(&v_color_white_b);

	Cvar_RegisterVariable(&v_hwgamma);

	Cvar_RegisterVariable(&v_psycho);

	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&vid_width);
	Cvar_RegisterVariable(&vid_height);
	Cvar_RegisterVariable(&vid_bitsperpixel);
	Cvar_RegisterVariable(&vid_refreshrate);
	Cvar_RegisterVariable(&vid_vsync);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&vid_minwidth);
	Cvar_RegisterVariable(&vid_minheight);
	Cvar_RegisterVariable(&gl_combine);
	Cvar_RegisterVariable(&gl_finish);
	Cmd_AddCommand("force_centerview", Force_CenterView_f, "recenters view (stops looking up/down)");
	Cmd_AddCommand("vid_restart", VID_Restart_f, "restarts video system (closes and reopens the window, restarts renderer)");
	if (gamemode == GAME_GOODVSBAD2)
		Cvar_Set("gl_combine", "0");
}

int VID_Mode(int fullscreen, int width, int height, int bpp, int refreshrate)
{
	Con_Printf("Video: %s %dx%dx%dx%dhz\n", fullscreen ? "fullscreen" : "window", width, height, bpp, refreshrate);
	if (VID_InitMode(fullscreen, width, height, bpp, refreshrate))
	{
		vid.fullscreen = fullscreen;
		vid.width = width;
		vid.height = height;
		vid.bitsperpixel = bpp;
		vid.refreshrate = refreshrate;
		Cvar_SetValueQuick(&vid_fullscreen, fullscreen);
		Cvar_SetValueQuick(&vid_width, width);
		Cvar_SetValueQuick(&vid_height, height);
		Cvar_SetValueQuick(&vid_bitsperpixel, bpp);
		Cvar_SetValueQuick(&vid_refreshrate, refreshrate);
		return true;
	}
	else
		return false;
}

static void VID_OpenSystems(void)
{
	R_Modules_Start();
	S_Startup();
}

static void VID_CloseSystems(void)
{
	S_Shutdown();
	R_Modules_Shutdown();
}

qboolean vid_commandlinecheck = true;

void VID_Restart_f(void)
{
	// don't crash if video hasn't started yet
	if (vid_commandlinecheck)
		return;

	Con_Printf("VID_Restart: changing from %s %dx%dx%dbpp, to %s %dx%dx%dbpp.\n",
		vid.fullscreen ? "fullscreen" : "window", vid.width, vid.height, vid.bitsperpixel,
		vid_fullscreen.integer ? "fullscreen" : "window", vid_width.integer, vid_height.integer, vid_bitsperpixel.integer);
	VID_CloseSystems();
	VID_Shutdown();
	if (!VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_refreshrate.integer))
	{
		Con_Print("Video mode change failed\n");
		if (!VID_Mode(vid.fullscreen, vid.width, vid.height, vid.bitsperpixel, vid.refreshrate))
			Sys_Error("Unable to restore to last working video mode");
	}
	VID_OpenSystems();
}

// this is only called once by Host_StartVideo
void VID_Start(void)
{
	int i, width, height, success;
	if (vid_commandlinecheck)
	{
		// interpret command-line parameters
		vid_commandlinecheck = false;
// COMMANDLINEOPTION: Video: -window performs +vid_fullscreen 0
		if (COM_CheckParm("-window") || COM_CheckParm("-safe"))
			Cvar_SetValueQuick(&vid_fullscreen, false);
// COMMANDLINEOPTION: Video: -fullscreen performs +vid_fullscreen 1
		if (COM_CheckParm("-fullscreen"))
			Cvar_SetValueQuick(&vid_fullscreen, true);
		width = 0;
		height = 0;
// COMMANDLINEOPTION: Video: -width <pixels> performs +vid_width <pixels> and also +vid_height <pixels*3/4> if only -width is specified (example: -width 1024 sets 1024x768 mode)
		if ((i = COM_CheckParm("-width")) != 0)
			width = atoi(com_argv[i+1]);
// COMMANDLINEOPTION: Video: -height <pixels> performs +vid_height <pixels> and also +vid_width <pixels*4/3> if only -height is specified (example: -height 768 sets 1024x768 mode)
		if ((i = COM_CheckParm("-height")) != 0)
			height = atoi(com_argv[i+1]);
		if (width == 0)
			width = height * 4 / 3;
		if (height == 0)
			height = width * 3 / 4;
		if (width)
			Cvar_SetValueQuick(&vid_width, width);
		if (height)
			Cvar_SetValueQuick(&vid_height, height);
// COMMANDLINEOPTION: Video: -bpp <bits> performs +vid_bitsperpixel <bits> (example -bpp 32 or -bpp 16)
		if ((i = COM_CheckParm("-bpp")) != 0)
			Cvar_SetQuick(&vid_bitsperpixel, com_argv[i+1]);
	}

	Con_Print("Starting video system\n");
	success = VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, vid_refreshrate.integer);
	if (!success)
	{
		Con_Print("Desired video mode fail, trying fallbacks...\n");
		success = VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, vid_bitsperpixel.integer, 60);
		if (!success && vid_bitsperpixel.integer > 16)
			success = VID_Mode(vid_fullscreen.integer, vid_width.integer, vid_height.integer, 16, 60);
		if (!success && (vid_width.integer > 640 || vid_height.integer > 480))
			success = VID_Mode(vid_fullscreen.integer, 640, 480, 16, 60);
		if (!success && vid_fullscreen.integer)
			success = VID_Mode(false, 640, 480, 16, 60);
		if (!success)
			Sys_Error("Video modes failed");
	}
	VID_OpenSystems();
}

