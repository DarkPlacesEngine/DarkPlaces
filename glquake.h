#ifndef GLQUAKE_H
#define GLQUAKE_H

#include <stddef.h>

#ifdef USE_GLES2
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#endif

// disable data conversion warnings

#ifdef _MSC_VER
#pragma warning(disable : 4310) // LadyHavoc: MSVC++ 2008 x86: cast truncates constant value
#pragma warning(disable : 4245) // LadyHavoc: MSVC++ 2008 x86: 'initializing' : conversion from 'int' to 'unsigned char', signed/unsigned mismatch
#pragma warning(disable : 4204) // LadyHavoc: MSVC++ 2008 x86: nonstandard extension used : non-constant aggregate initializer
//#pragma warning(disable : 4267) // LadyHavoc: MSVC++ 2008 x64, conversion from 'size_t' to 'int', possible loss of data
//#pragma warning(disable : 4244) // LadyHavoc: MSVC++ 4 x86, double/float
//#pragma warning(disable : 4305) // LadyHavoc: MSVC++ 6 x86, double/float
//#pragma warning(disable : 4706) // LadyHavoc: MSVC++ 2008 x86, assignment within conditional expression
//#pragma warning(disable : 4127) // LadyHavoc: MSVC++ 2008 x86, conditional expression is constant
//#pragma warning(disable : 4100) // LadyHavoc: MSVC++ 2008 x86, unreferenced formal parameter
//#pragma warning(disable : 4055) // LadyHavoc: MSVC++ 2008 x86, 'type cast' from data pointer   to function pointer
//#pragma warning(disable : 4054) // LadyHavoc: MSVC++ 2008 x86, 'type cast' from function pointer   to data pointer
#endif


//====================================================

#define DEBUGGL

#ifdef DEBUGGL
#ifdef USE_GLES2
#define CHECKGLERROR {if (gl_paranoid.integer){if (gl_printcheckerror.integer) Con_Printf("CHECKGLERROR at %s:%d\n", __FILE__, __LINE__);gl_errornumber = glGetError();if (gl_errornumber) GL_PrintError(gl_errornumber, __FILE__, __LINE__);}}
#else
#define CHECKGLERROR {if (gl_paranoid.integer){if (gl_printcheckerror.integer) Con_Printf("CHECKGLERROR at %s:%d\n", __FILE__, __LINE__);gl_errornumber = qglGetError ? qglGetError() : 0;if (gl_errornumber) GL_PrintError(gl_errornumber, __FILE__, __LINE__);}}
#endif
extern int gl_errornumber;
void GL_PrintError(int errornumber, const char *filename, int linenumber);
#else
#define CHECKGLERROR
#endif

#ifndef USE_GLES2
extern GLboolean(GLAPIENTRY *qglIsBuffer) (GLuint buffer);
extern GLboolean(GLAPIENTRY *qglIsEnabled)(GLenum cap);
extern GLboolean(GLAPIENTRY *qglIsFramebuffer)(GLuint framebuffer);
extern GLboolean(GLAPIENTRY *qglIsQuery)(GLuint qid);
extern GLboolean(GLAPIENTRY *qglIsRenderbuffer)(GLuint renderbuffer);
extern GLboolean(GLAPIENTRY *qglUnmapBuffer) (GLenum target);
extern GLenum(GLAPIENTRY *qglCheckFramebufferStatus)(GLenum target);
extern GLenum(GLAPIENTRY *qglGetError)(void);
extern GLint(GLAPIENTRY *qglGetAttribLocation)(GLuint programObj, const GLchar *name);
extern GLint(GLAPIENTRY *qglGetUniformLocation)(GLuint programObj, const GLchar *name);
extern GLuint(GLAPIENTRY *qglCreateProgram)(void);
extern GLuint(GLAPIENTRY *qglCreateShader)(GLenum shaderType);
extern GLuint(GLAPIENTRY *qglGetDebugMessageLogARB)(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
extern GLuint(GLAPIENTRY *qglGetUniformBlockIndex)(GLuint program, const char* uniformBlockName);
extern GLvoid(GLAPIENTRY *qglBindFramebuffer)(GLenum target, GLuint framebuffer);
extern GLvoid(GLAPIENTRY *qglBindRenderbuffer)(GLenum target, GLuint renderbuffer);
extern GLvoid(GLAPIENTRY *qglBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern GLvoid(GLAPIENTRY *qglDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
extern GLvoid(GLAPIENTRY *qglDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
extern GLvoid(GLAPIENTRY *qglFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern GLvoid(GLAPIENTRY *qglFramebufferTexture1D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLvoid(GLAPIENTRY *qglFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLvoid(GLAPIENTRY *qglFramebufferTexture3D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
extern GLvoid(GLAPIENTRY *qglFramebufferTextureLayer)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
extern GLvoid(GLAPIENTRY *qglGenFramebuffers)(GLsizei n, GLuint *framebuffers);
extern GLvoid(GLAPIENTRY *qglGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
extern GLvoid(GLAPIENTRY *qglGenerateMipmap)(GLenum target);
extern GLvoid(GLAPIENTRY *qglGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
extern GLvoid(GLAPIENTRY *qglGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
extern GLvoid(GLAPIENTRY *qglRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern GLvoid(GLAPIENTRY *qglRenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
extern GLvoid* (GLAPIENTRY *qglMapBuffer) (GLenum target, GLenum access);
extern const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
extern void (GLAPIENTRY *qglActiveTexture)(GLenum texture);
extern void (GLAPIENTRY *qglAttachShader)(GLuint containerObj, GLuint obj);
extern void (GLAPIENTRY *qglBeginQuery)(GLenum target, GLuint qid);
extern void (GLAPIENTRY *qglBindAttribLocation)(GLuint programObj, GLuint index, const GLchar *name);
extern void (GLAPIENTRY *qglBindBuffer) (GLenum target, GLuint buffer);
extern void (GLAPIENTRY *qglBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
extern void (GLAPIENTRY *qglBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
extern void (GLAPIENTRY *qglBindFragDataLocation)(GLuint programObj, GLuint index, const GLchar *name);
extern void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
extern void (GLAPIENTRY *qglBlendEquation)(GLenum); // also supplied by GL_blend_subtract
extern void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (GLAPIENTRY *qglBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
extern void (GLAPIENTRY *qglBufferData) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
extern void (GLAPIENTRY *qglBufferSubData) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
extern void (GLAPIENTRY *qglClear)(GLbitfield mask);
extern void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
extern void (GLAPIENTRY *qglClearStencil)(GLint s);
extern void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern void (GLAPIENTRY *qglCompileShader)(GLuint shaderObj);
extern void (GLAPIENTRY *qglCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexImage3D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern void (GLAPIENTRY *qglCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern void (GLAPIENTRY *qglCullFace)(GLenum mode);
extern void (GLAPIENTRY *qglDebugMessageCallbackARB)(GLDEBUGPROCARB callback, const GLvoid* userParam);
extern void (GLAPIENTRY *qglDebugMessageControlARB)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
extern void (GLAPIENTRY *qglDebugMessageInsertARB)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
extern void (GLAPIENTRY *qglDeleteBuffers) (GLsizei n, const GLuint *buffers);
extern void (GLAPIENTRY *qglDeleteProgram)(GLuint obj);
extern void (GLAPIENTRY *qglDeleteQueries)(GLsizei n, const GLuint *ids);
extern void (GLAPIENTRY *qglDeleteShader)(GLuint obj);
extern void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
extern void (GLAPIENTRY *qglDepthFunc)(GLenum func);
extern void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
extern void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
extern void (GLAPIENTRY *qglDepthRangef)(GLclampf near_val, GLclampf far_val);
extern void (GLAPIENTRY *qglDetachShader)(GLuint containerObj, GLuint attachedObj);
extern void (GLAPIENTRY *qglDisable)(GLenum cap);
extern void (GLAPIENTRY *qglDisableVertexAttribArray)(GLuint index);
extern void (GLAPIENTRY *qglDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (GLAPIENTRY *qglDrawBuffer)(GLenum mode);
extern void (GLAPIENTRY *qglDrawBuffers)(GLsizei n, const GLenum *bufs);
extern void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern void (GLAPIENTRY *qglEnable)(GLenum cap);
extern void (GLAPIENTRY *qglEnableVertexAttribArray)(GLuint index);
extern void (GLAPIENTRY *qglEndQuery)(GLenum target);
extern void (GLAPIENTRY *qglFinish)(void);
extern void (GLAPIENTRY *qglFlush)(void);
extern void (GLAPIENTRY *qglGenBuffers) (GLsizei n, GLuint *buffers);
extern void (GLAPIENTRY *qglGenQueries)(GLsizei n, GLuint *ids);
extern void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
extern void (GLAPIENTRY *qglGetActiveAttrib)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern void (GLAPIENTRY *qglGetActiveUniform)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern void (GLAPIENTRY *qglGetActiveUniformBlockName)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, char* uniformBlockName);
extern void (GLAPIENTRY *qglGetActiveUniformBlockiv)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
extern void (GLAPIENTRY *qglGetActiveUniformName)(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, char* uniformName);
extern void (GLAPIENTRY *qglGetActiveUniformsiv)(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
extern void (GLAPIENTRY *qglGetAttachedShaders)(GLuint containerObj, GLsizei maxCount, GLsizei *count, GLuint *obj);
extern void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
extern void (GLAPIENTRY *qglGetCompressedTexImage)(GLenum target, GLint lod, void *img);
extern void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
extern void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetIntegeri_v)(GLenum target, GLuint index, GLint* data);
extern void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetPointerv)(GLenum pname, GLvoid** params);
extern void (GLAPIENTRY *qglGetProgramInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (GLAPIENTRY *qglGetProgramiv)(GLuint obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectiv)(GLuint qid, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectuiv)(GLuint qid, GLenum pname, GLuint *params);
extern void (GLAPIENTRY *qglGetQueryiv)(GLenum target, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetShaderInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (GLAPIENTRY *qglGetShaderSource)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *source);
extern void (GLAPIENTRY *qglGetShaderiv)(GLuint obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
extern void (GLAPIENTRY *qglGetTexLevelParameterfv)(GLenum target, GLint level, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetUniformIndices)(GLuint program, GLsizei uniformCount, const char** uniformNames, GLuint* uniformIndices);
extern void (GLAPIENTRY *qglGetUniformfv)(GLuint programObj, GLint location, GLfloat *params);
extern void (GLAPIENTRY *qglGetUniformiv)(GLuint programObj, GLint location, GLint *params);
extern void (GLAPIENTRY *qglGetVertexAttribPointerv)(GLuint index, GLenum pname, GLvoid **pointer);
extern void (GLAPIENTRY *qglGetVertexAttribdv)(GLuint index, GLenum pname, GLdouble *params);
extern void (GLAPIENTRY *qglGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglHint)(GLenum target, GLenum mode);
extern void (GLAPIENTRY *qglLinkProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);
extern void (GLAPIENTRY *qglPointSize)(GLfloat size);
extern void (GLAPIENTRY *qglPolygonMode)(GLenum face, GLenum mode);
extern void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);
extern void (GLAPIENTRY *qglReadBuffer)(GLenum mode);
extern void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
extern void (GLAPIENTRY *qglSampleCoverage)(GLclampf value, GLboolean invert);
extern void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
extern void (GLAPIENTRY *qglShaderSource)(GLuint shaderObj, GLsizei count, const GLchar **string, const GLint *length);
extern void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
extern void (GLAPIENTRY *qglStencilMask)(GLuint mask);
extern void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
extern void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexImage3D)(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglUniform1f)(GLint location, GLfloat v0);
extern void (GLAPIENTRY *qglUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform1i)(GLint location, GLint v0);
extern void (GLAPIENTRY *qglUniform1iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform2i)(GLint location, GLint v0, GLint v1);
extern void (GLAPIENTRY *qglUniform2iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
extern void (GLAPIENTRY *qglUniform3iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern void (GLAPIENTRY *qglUniform4iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
extern void (GLAPIENTRY *qglUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUseProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglValidateProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglVertexAttrib1d)(GLuint index, GLdouble v0);
extern void (GLAPIENTRY *qglVertexAttrib1dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib1f)(GLuint index, GLfloat v0);
extern void (GLAPIENTRY *qglVertexAttrib1fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib1s)(GLuint index, GLshort v0);
extern void (GLAPIENTRY *qglVertexAttrib1sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib2d)(GLuint index, GLdouble v0, GLdouble v1);
extern void (GLAPIENTRY *qglVertexAttrib2dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib2f)(GLuint index, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglVertexAttrib2fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib2s)(GLuint index, GLshort v0, GLshort v1);
extern void (GLAPIENTRY *qglVertexAttrib2sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib3d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
extern void (GLAPIENTRY *qglVertexAttrib3dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib3f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglVertexAttrib3fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib3s)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
extern void (GLAPIENTRY *qglVertexAttrib3sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nbv)(GLuint index, const GLbyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4Niv)(GLuint index, const GLint *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nsv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nub)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
extern void (GLAPIENTRY *qglVertexAttrib4Nubv)(GLuint index, const GLubyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nuiv)(GLuint index, const GLuint *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nusv)(GLuint index, const GLushort *v);
extern void (GLAPIENTRY *qglVertexAttrib4bv)(GLuint index, const GLbyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
extern void (GLAPIENTRY *qglVertexAttrib4dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib4f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglVertexAttrib4fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib4iv)(GLuint index, const GLint *v);
extern void (GLAPIENTRY *qglVertexAttrib4s)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
extern void (GLAPIENTRY *qglVertexAttrib4sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib4ubv)(GLuint index, const GLubyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4uiv)(GLuint index, const GLuint *v);
extern void (GLAPIENTRY *qglVertexAttrib4usv)(GLuint index, const GLushort *v);
extern void (GLAPIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
extern void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY *GLDEBUGPROCARB)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam);
#else
#define qglActiveTexture glActiveTexture
#define qglAttachShader glAttachShader
#define qglBeginQuery glBeginQuery
#define qglBindAttribLocation glBindAttribLocation
#define qglBindBuffer glBindBuffer
#define qglBindBufferBase glBindBufferBase
#define qglBindBufferRange glBindBufferRange
#define qglBindFragDataLocation glBindFragDataLocation
#define qglBindFramebuffer glBindFramebuffer
#define qglBindRenderbuffer glBindRenderbuffer
#define qglBindTexture glBindTexture
#define qglBlendEquation glBlendEquation
#define qglBlendFunc glBlendFunc
#define qglBlendFuncSeparate glBlendFuncSeparate
#define qglBlitFramebuffer glBlitFramebuffer
#define qglBufferData glBufferData
#define qglBufferSubData glBufferSubData
#define qglCheckFramebufferStatus glCheckFramebufferStatus
#define qglClear glClear
#define qglClearColor glClearColor
#define qglClearDepth glClearDepth
#define qglClearStencil glClearStencil
#define qglColorMask glColorMask
#define qglCompileShader glCompileShader
#define qglCompressedTexImage2D glCompressedTexImage2D
#define qglCompressedTexImage3D glCompressedTexImage3D
#define qglCompressedTexSubImage2D glCompressedTexSubImage2D
#define qglCompressedTexSubImage3D glCompressedTexSubImage3D
#define qglCopyTexImage2D glCopyTexImage2D
#define qglCopyTexSubImage2D glCopyTexSubImage2D
#define qglCopyTexSubImage3D glCopyTexSubImage3D
#define qglCreateProgram glCreateProgram
#define qglCreateShader glCreateShader
#define qglCullFace glCullFace
#define qglDebugMessageCallbackARB glDebugMessageCallbackARB
#define qglDebugMessageControlARB glDebugMessageControlARB
#define qglDebugMessageInsertARB glDebugMessageInsertARB
#define qglDeleteBuffers glDeleteBuffers
#define qglDeleteFramebuffers glDeleteFramebuffers
#define qglDeleteProgram glDeleteProgram
#define qglDeleteQueries glDeleteQueries
#define qglDeleteRenderbuffers glDeleteRenderbuffers
#define qglDeleteShader glDeleteShader
#define qglDeleteTextures glDeleteTextures
#define qglDepthFunc glDepthFunc
#define qglDepthMask glDepthMask
#define qglDepthRange glDepthRange
#define qglDepthRangef glDepthRangef
#define qglDetachShader glDetachShader
#define qglDisable glDisable
#define qglDisableVertexAttribArray glDisableVertexAttribArray
#define qglDrawArrays glDrawArrays
#define qglDrawBuffer glDrawBuffer
#define qglDrawBuffers glDrawBuffers
#define qglDrawElements glDrawElements
#define qglEnable glEnable
#define qglEnableVertexAttribArray glEnableVertexAttribArray
#define qglEndQuery glEndQuery
#define qglFinish glFinish
#define qglFlush glFlush
#define qglFramebufferRenderbuffer glFramebufferRenderbuffer
#define qglFramebufferTexture1D glFramebufferTexture1D
#define qglFramebufferTexture2D glFramebufferTexture2D
#define qglFramebufferTexture3D glFramebufferTexture3D
#define qglFramebufferTextureLayer glFramebufferTextureLayer
#define qglGenBuffers glGenBuffers
#define qglGenFramebuffers glGenFramebuffers
#define qglGenQueries glGenQueries
#define qglGenRenderbuffers glGenRenderbuffers
#define qglGenTextures glGenTextures
#define qglGenerateMipmap glGenerateMipmap
#define qglGetActiveAttrib glGetActiveAttrib
#define qglGetActiveUniform glGetActiveUniform
#define qglGetActiveUniformBlockName glGetActiveUniformBlockName
#define qglGetActiveUniformBlockiv glGetActiveUniformBlockiv
#define qglGetActiveUniformName glGetActiveUniformName
#define qglGetActiveUniformsiv glGetActiveUniformsiv
#define qglGetAttachedShaders glGetAttachedShaders
#define qglGetAttribLocation glGetAttribLocation
#define qglGetBooleanv glGetBooleanv
#define qglGetCompressedTexImage glGetCompressedTexImage
#define qglGetDebugMessageLogARB glGetDebugMessageLogARB
#define qglGetDoublev glGetDoublev
#define qglGetError glGetError
#define qglGetFloatv glGetFloatv
#define qglGetFramebufferAttachmentParameteriv glGetFramebufferAttachmentParameteriv
#define qglGetIntegeri_v glGetIntegeri_v
#define qglGetIntegerv glGetIntegerv
#define qglGetPointerv glGetPointerv
#define qglGetProgramInfoLog glGetProgramInfoLog
#define qglGetProgramiv glGetProgramiv
#define qglGetQueryObjectiv glGetQueryObjectiv
#define qglGetQueryObjectuiv glGetQueryObjectuiv
#define qglGetQueryiv glGetQueryiv
#define qglGetRenderbufferParameteriv glGetRenderbufferParameteriv
#define qglGetShaderInfoLog glGetShaderInfoLog
#define qglGetShaderSource glGetShaderSource
#define qglGetShaderiv glGetShaderiv
#define qglGetString glGetString
#define qglGetTexImage glGetTexImage
#define qglGetTexLevelParameterfv glGetTexLevelParameterfv
#define qglGetTexLevelParameteriv glGetTexLevelParameteriv
#define qglGetTexParameterfv glGetTexParameterfv
#define qglGetTexParameteriv glGetTexParameteriv
#define qglGetUniformBlockIndex glGetUniformBlockIndex
#define qglGetUniformIndices glGetUniformIndices
#define qglGetUniformLocation glGetUniformLocation
#define qglGetUniformfv glGetUniformfv
#define qglGetUniformiv glGetUniformiv
#define qglGetVertexAttribPointerv glGetVertexAttribPointerv
#define qglGetVertexAttribdv glGetVertexAttribdv
#define qglGetVertexAttribfv glGetVertexAttribfv
#define qglGetVertexAttribiv glGetVertexAttribiv
#define qglHint glHint
#define qglIsBuffer glIsBuffer
#define qglIsEnabled glIsEnabled
#define qglIsFramebuffer glIsFramebuffer
#define qglIsQuery glIsQuery
#define qglIsRenderbuffer glIsRenderbuffer
#define qglLinkProgram glLinkProgram
#define qglMapBuffer glMapBuffer
#define qglPixelStorei glPixelStorei
#define qglPointSize glPointSize
#define qglPolygonMode glPolygonMode
#define qglPolygonOffset glPolygonOffset
#define qglReadBuffer glReadBuffer
#define qglReadPixels glReadPixels
#define qglRenderbufferStorage glRenderbufferStorage
#define qglRenderbufferStorageMultisample glRenderbufferStorageMultisample
#define qglSampleCoverage glSampleCoverage
#define qglScissor glScissor
#define qglShaderSource glShaderSource
#define qglStencilFunc glStencilFunc
#define qglStencilMask glStencilMask
#define qglStencilOp glStencilOp
#define qglTexImage2D glTexImage2D
#define qglTexImage3D glTexImage3D
#define qglTexParameterf glTexParameterf
#define qglTexParameterfv glTexParameterfv
#define qglTexParameteri glTexParameteri
#define qglTexSubImage2D glTexSubImage2D
#define qglTexSubImage3D glTexSubImage3D
#define qglUniform1f glUniform1f
#define qglUniform1fv glUniform1fv
#define qglUniform1i glUniform1i
#define qglUniform1iv glUniform1iv
#define qglUniform2f glUniform2f
#define qglUniform2fv glUniform2fv
#define qglUniform2i glUniform2i
#define qglUniform2iv glUniform2iv
#define qglUniform3f glUniform3f
#define qglUniform3fv glUniform3fv
#define qglUniform3i glUniform3i
#define qglUniform3iv glUniform3iv
#define qglUniform4f glUniform4f
#define qglUniform4fv glUniform4fv
#define qglUniform4i glUniform4i
#define qglUniform4iv glUniform4iv
#define qglUniformBlockBinding glUniformBlockBinding
#define qglUniformMatrix2fv glUniformMatrix2fv
#define qglUniformMatrix3fv glUniformMatrix3fv
#define qglUniformMatrix4fv glUniformMatrix4fv
#define qglUnmapBuffer glUnmapBuffer
#define qglUseProgram glUseProgram
#define qglValidateProgram glValidateProgram
#define qglVertexAttrib1d glVertexAttrib1d
#define qglVertexAttrib1dv glVertexAttrib1dv
#define qglVertexAttrib1f glVertexAttrib1f
#define qglVertexAttrib1fv glVertexAttrib1fv
#define qglVertexAttrib1s glVertexAttrib1s
#define qglVertexAttrib1sv glVertexAttrib1sv
#define qglVertexAttrib2d glVertexAttrib2d
#define qglVertexAttrib2dv glVertexAttrib2dv
#define qglVertexAttrib2f glVertexAttrib2f
#define qglVertexAttrib2fv glVertexAttrib2fv
#define qglVertexAttrib2s glVertexAttrib2s
#define qglVertexAttrib2sv glVertexAttrib2sv
#define qglVertexAttrib3d glVertexAttrib3d
#define qglVertexAttrib3dv glVertexAttrib3dv
#define qglVertexAttrib3f glVertexAttrib3f
#define qglVertexAttrib3fv glVertexAttrib3fv
#define qglVertexAttrib3s glVertexAttrib3s
#define qglVertexAttrib3sv glVertexAttrib3sv
#define qglVertexAttrib4Nbv glVertexAttrib4Nbv
#define qglVertexAttrib4Niv glVertexAttrib4Niv
#define qglVertexAttrib4Nsv glVertexAttrib4Nsv
#define qglVertexAttrib4Nub glVertexAttrib4Nub
#define qglVertexAttrib4Nubv glVertexAttrib4Nubv
#define qglVertexAttrib4Nuiv glVertexAttrib4Nuiv
#define qglVertexAttrib4Nusv glVertexAttrib4Nusv
#define qglVertexAttrib4bv glVertexAttrib4bv
#define qglVertexAttrib4d glVertexAttrib4d
#define qglVertexAttrib4dv glVertexAttrib4dv
#define qglVertexAttrib4f glVertexAttrib4f
#define qglVertexAttrib4fv glVertexAttrib4fv
#define qglVertexAttrib4iv glVertexAttrib4iv
#define qglVertexAttrib4s glVertexAttrib4s
#define qglVertexAttrib4sv glVertexAttrib4sv
#define qglVertexAttrib4ubv glVertexAttrib4ubv
#define qglVertexAttrib4uiv glVertexAttrib4uiv
#define qglVertexAttrib4usv glVertexAttrib4usv
#define qglVertexAttribPointer glVertexAttribPointer
#define qglViewport glViewport
#endif

#endif
