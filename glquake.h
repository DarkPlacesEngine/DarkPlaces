/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef GLQUAKE_H
#define GLQUAKE_H

#ifdef USE_GLES2
#ifdef __IPHONEOS__
#include <OpenGLES/ES2/gl.h>
#else
#include <SDL_opengles2.h>
#endif
// used in R_SetupShader_Generic calls, not actually passed to GL
#ifndef GL_MODULATE
#define GL_MODULATE				0x2100
#define GL_DECAL                          0x2101
#define GL_ADD                            0x0104
#endif
#endif

// disable data conversion warnings

#ifdef _MSC_VER
#pragma warning(disable : 4310) // LordHavoc: MSVC++ 2008 x86: cast truncates constant value
#pragma warning(disable : 4245) // LordHavoc: MSVC++ 2008 x86: 'initializing' : conversion from 'int' to 'unsigned char', signed/unsigned mismatch
#pragma warning(disable : 4204) // LordHavoc: MSVC++ 2008 x86: nonstandard extension used : non-constant aggregate initializer
#pragma warning(disable : 4267) // LordHavoc: MSVC++ 2008 x64, conversion from 'size_t' to 'int', possible loss of data
//#pragma warning(disable : 4244)     // LordHavoc: MSVC++ 4 x86, double/float
//#pragma warning(disable : 4305)		// LordHavoc: MSVC++ 6 x86, double/float
//#pragma warning(disable : 4706)		// LordHavoc: MSVC++ 2008 x86, assignment within conditional expression
//#pragma warning(disable : 4127)		// LordHavoc: MSVC++ 2008 x86, conditional expression is constant
//#pragma warning(disable : 4100)		// LordHavoc: MSVC++ 2008 x86, unreferenced formal parameter
//#pragma warning(disable : 4055)		// LordHavoc: MSVC++ 2008 x86, 'type cast' from data pointer   to function pointer
//#pragma warning(disable : 4054)		// LordHavoc: MSVC++ 2008 x86, 'type cast' from function pointer   to data pointer
#endif


//====================================================

#ifndef USE_GLES2
// wgl uses APIENTRY
#ifndef APIENTRY
#define APIENTRY
#endif

// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#ifndef GL_PROJECTION
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
// 1-byte signed
typedef signed char GLbyte;
// 2-byte signed
typedef short GLshort;
// 4-byte signed
typedef int GLint;
// 1-byte unsigned
typedef unsigned char GLubyte;
// 2-byte unsigned
typedef unsigned short GLushort;
// 4-byte unsigned
typedef unsigned int GLuint;
// 4-byte signed
typedef int GLsizei;
// single precision float
typedef float GLfloat;
// single precision float in [0,1]
typedef float GLclampf;
// double precision float
typedef double GLdouble;
// double precision float in [0,1]
typedef double GLclampd;
// int whose size is the same as a pointer (?)
typedef ptrdiff_t GLintptrARB;
// int whose size is the same as a pointer (?)
typedef ptrdiff_t GLsizeiptrARB;

#define GL_STEREO					0x0C33

#define GL_MODELVIEW				0x1700
#define GL_PROJECTION				0x1701
#define GL_TEXTURE				0x1702
#define GL_MATRIX_MODE				0x0BA0
#define GL_MODELVIEW_MATRIX			0x0BA6
#define GL_PROJECTION_MATRIX			0x0BA7
#define GL_TEXTURE_MATRIX			0x0BA8

#define GL_DONT_CARE				0x1100
#define GL_FASTEST					0x1101
#define GL_NICEST					0x1102

#define GL_DEPTH_TEST				0x0B71

#define GL_CULL_FACE				0x0B44

#define GL_BLEND				0x0BE2
#define GL_ALPHA_TEST			0x0BC0

#define GL_ZERO					0x0
#define GL_ONE					0x1
#define GL_SRC_COLOR				0x0300
#define GL_ONE_MINUS_SRC_COLOR			0x0301
#define GL_DST_COLOR				0x0306
#define GL_ONE_MINUS_DST_COLOR			0x0307
#define GL_SRC_ALPHA				0x0302
#define GL_ONE_MINUS_SRC_ALPHA			0x0303
#define GL_DST_ALPHA				0x0304
#define GL_ONE_MINUS_DST_ALPHA			0x0305
#define GL_SRC_ALPHA_SATURATE			0x0308
#define GL_CONSTANT_COLOR			0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR		0x8002
#define GL_CONSTANT_ALPHA			0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA		0x8004

#define GL_TEXTURE_ENV				0x2300
#define GL_TEXTURE_ENV_MODE			0x2200
#define GL_TEXTURE_1D				0x0DE0
#define GL_TEXTURE_2D				0x0DE1
#define GL_TEXTURE_WRAP_S			0x2802
#define GL_TEXTURE_WRAP_T			0x2803
#define GL_TEXTURE_WRAP_R			0x8072
#define GL_TEXTURE_BORDER_COLOR			0x1004
#define GL_TEXTURE_MAG_FILTER			0x2800
#define GL_TEXTURE_MIN_FILTER			0x2801
#define GL_PACK_ALIGNMENT			0x0D05
#define GL_UNPACK_ALIGNMENT			0x0CF5
#define GL_TEXTURE_BINDING_1D                   0x8068
#define GL_TEXTURE_BINDING_2D                   0x8069
#define GL_TEXTURE_INTERNAL_FORMAT		0x1003
#define GL_TEXTURE_MIN_LOD			0x813A
#define GL_TEXTURE_MAX_LOD			0x813B
#define GL_TEXTURE_BASE_LEVEL			0x813C
#define GL_TEXTURE_MAX_LEVEL			0x813D

#define GL_NEAREST				0x2600
#define GL_LINEAR				0x2601
#define GL_NEAREST_MIPMAP_NEAREST		0x2700
#define GL_NEAREST_MIPMAP_LINEAR		0x2702
#define GL_LINEAR_MIPMAP_NEAREST		0x2701
#define GL_LINEAR_MIPMAP_LINEAR			0x2703

#define GL_LINE					0x1B01
#define GL_FILL					0x1B02

#define GL_ADD					0x0104
#define GL_DECAL				0x2101
#define GL_MODULATE				0x2100

#define GL_REPEAT				0x2901
#define GL_CLAMP				0x2900

#define GL_POINTS				0x0000
#define GL_LINES				0x0001
#define GL_LINE_LOOP			0x0002
#define GL_LINE_STRIP			0x0003
#define GL_TRIANGLES			0x0004
#define GL_TRIANGLE_STRIP		0x0005
#define GL_TRIANGLE_FAN			0x0006
#define GL_QUADS				0x0007
#define GL_QUAD_STRIP			0x0008
#define GL_POLYGON				0x0009

#define GL_FALSE				0x0
#define GL_TRUE					0x1

#define GL_BYTE					0x1400
#define GL_UNSIGNED_BYTE			0x1401
#define GL_SHORT				0x1402
#define GL_UNSIGNED_SHORT			0x1403
#define GL_INT					0x1404
#define GL_UNSIGNED_INT				0x1405
#define GL_FLOAT				0x1406
#define GL_DOUBLE				0x140A
#define GL_2_BYTES				0x1407
#define GL_3_BYTES				0x1408
#define GL_4_BYTES				0x1409

#define GL_VERTEX_ARRAY				0x8074
#define GL_NORMAL_ARRAY				0x8075
#define GL_COLOR_ARRAY				0x8076
//#define GL_INDEX_ARRAY				0x8077
#define GL_TEXTURE_COORD_ARRAY			0x8078
//#define GL_EDGE_FLAG_ARRAY			0x8079

#define GL_NONE					0
#define GL_FRONT_LEFT			0x0400
#define GL_FRONT_RIGHT			0x0401
#define GL_BACK_LEFT			0x0402
#define GL_BACK_RIGHT			0x0403
#define GL_FRONT				0x0404
#define GL_BACK					0x0405
#define GL_LEFT					0x0406
#define GL_RIGHT				0x0407
#define GL_FRONT_AND_BACK		0x0408
#define GL_AUX0					0x0409
#define GL_AUX1					0x040A
#define GL_AUX2					0x040B
#define GL_AUX3					0x040C

#define GL_VENDOR				0x1F00
#define GL_RENDERER				0x1F01
#define GL_VERSION				0x1F02
#define GL_EXTENSIONS				0x1F03

#define GL_NO_ERROR 				0x0
#define GL_INVALID_VALUE			0x0501
#define GL_INVALID_ENUM				0x0500
#define GL_INVALID_OPERATION			0x0502
#define GL_STACK_OVERFLOW			0x0503
#define GL_STACK_UNDERFLOW			0x0504
#define GL_OUT_OF_MEMORY			0x0505

#define GL_DITHER				0x0BD0
#define GL_ALPHA				0x1906
#define GL_RGB					0x1907
#define GL_RGBA					0x1908

#define GL_MAX_TEXTURE_SIZE			0x0D33

#define GL_NEVER				0x0200
#define GL_LESS					0x0201
#define GL_EQUAL				0x0202
#define GL_LEQUAL				0x0203
#define GL_GREATER				0x0204
#define GL_NOTEQUAL				0x0205
#define GL_GEQUAL				0x0206
#define GL_ALWAYS				0x0207
#define GL_DEPTH_TEST				0x0B71

#define GL_RED_SCALE				0x0D14
#define GL_GREEN_SCALE				0x0D18
#define GL_BLUE_SCALE				0x0D1A
#define GL_ALPHA_SCALE				0x0D1C

#define GL_DEPTH_BUFFER_BIT			0x00000100
#define GL_ACCUM_BUFFER_BIT			0x00000200
#define GL_STENCIL_BUFFER_BIT			0x00000400
#define GL_COLOR_BUFFER_BIT			0x00004000

#define GL_STENCIL_TEST				0x0B90
#define GL_KEEP					0x1E00
#define GL_REPLACE				0x1E01
#define GL_INCR					0x1E02
#define GL_DECR					0x1E03

#define GL_POLYGON_OFFSET_FACTOR          0x8038
#define GL_POLYGON_OFFSET_UNITS           0x2A00
#define GL_POLYGON_OFFSET_POINT           0x2A01
#define GL_POLYGON_OFFSET_LINE            0x2A02
#define GL_POLYGON_OFFSET_FILL            0x8037

#define GL_POINT_SMOOTH                         0x0B10
#define GL_LINE_SMOOTH                          0x0B20
#define GL_POLYGON_SMOOTH                       0x0B41

#define GL_POLYGON_STIPPLE                0x0B42

#define GL_CLIP_PLANE0                    0x3000
#define GL_CLIP_PLANE1                    0x3001
#define GL_CLIP_PLANE2                    0x3002
#define GL_CLIP_PLANE3                    0x3003
#define GL_CLIP_PLANE4                    0x3004
#define GL_CLIP_PLANE5                    0x3005

#define GL_DEPTH_COMPONENT                0x1902
#define GL_VIEWPORT                       0x0BA2
#define GL_DRAW_BUFFER                    0x0C01
#define GL_READ_BUFFER                    0x0C02
#define GL_LUMINANCE                      0x1909
#define GL_INTENSITY                      0x8049

#endif

//GL_EXT_texture_filter_anisotropic
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT       0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT   0x84FF
#endif

// GL_ARB_depth_texture
#ifndef GL_DEPTH_COMPONENT32_ARB
#define GL_DEPTH_COMPONENT16_ARB          0x81A5
#define GL_DEPTH_COMPONENT24_ARB          0x81A6
#define GL_DEPTH_COMPONENT32_ARB          0x81A7
#define GL_TEXTURE_DEPTH_SIZE_ARB         0x884A
#define GL_DEPTH_TEXTURE_MODE_ARB         0x884B
#endif

// GL_ARB_shadow
#ifndef GL_TEXTURE_COMPARE_MODE_ARB
#define GL_TEXTURE_COMPARE_MODE_ARB       0x884C
#define GL_TEXTURE_COMPARE_FUNC_ARB       0x884D
#define GL_COMPARE_R_TO_TEXTURE_ARB       0x884E
#endif

// GL_ARB_multitexture
extern void (GLAPIENTRY *qglMultiTexCoord1f) (GLenum, GLfloat);
extern void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglMultiTexCoord3f) (GLenum, GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglMultiTexCoord4f) (GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglActiveTexture) (GLenum);
extern void (GLAPIENTRY *qglClientActiveTexture) (GLenum);
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE			0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE	0x84E1
#define GL_MAX_TEXTURE_UNITS		0x84E2
#define GL_TEXTURE0					0x84C0
#define GL_TEXTURE1					0x84C1
#define GL_TEXTURE2					0x84C2
#define GL_TEXTURE3					0x84C3
#define GL_TEXTURE4					0x84C4
#define GL_TEXTURE5					0x84C5
#define GL_TEXTURE6					0x84C6
#define GL_TEXTURE7					0x84C7
#define GL_TEXTURE8					0x84C8
#define GL_TEXTURE9					0x84C9
#define GL_TEXTURE10				0x84CA
#define GL_TEXTURE11				0x84CB
#define GL_TEXTURE12				0x84CC
#define GL_TEXTURE13				0x84CD
#define GL_TEXTURE14				0x84CE
#define GL_TEXTURE15				0x84CF
#define GL_TEXTURE16				0x84D0
#define GL_TEXTURE17				0x84D1
#define GL_TEXTURE18				0x84D2
#define GL_TEXTURE19				0x84D3
#define GL_TEXTURE20				0x84D4
#define GL_TEXTURE21				0x84D5
#define GL_TEXTURE22				0x84D6
#define GL_TEXTURE23				0x84D7
#define GL_TEXTURE24				0x84D8
#define GL_TEXTURE25				0x84D9
#define GL_TEXTURE26				0x84DA
#define GL_TEXTURE27				0x84DB
#define GL_TEXTURE28				0x84DC
#define GL_TEXTURE29				0x84DD
#define GL_TEXTURE30				0x84DE
#define GL_TEXTURE31				0x84DF
#endif

// GL_ARB_texture_env_combine
#ifndef GL_COMBINE
#define GL_COMBINE					0x8570
#define GL_COMBINE_RGB				0x8571
#define GL_COMBINE_ALPHA			0x8572
#define GL_SOURCE0_RGB				0x8580
#define GL_SOURCE1_RGB				0x8581
#define GL_SOURCE2_RGB				0x8582
#define GL_SOURCE0_ALPHA			0x8588
#define GL_SOURCE1_ALPHA			0x8589
#define GL_SOURCE2_ALPHA			0x858A
#define GL_OPERAND0_RGB				0x8590
#define GL_OPERAND1_RGB				0x8591
#define GL_OPERAND2_RGB				0x8592
#define GL_OPERAND0_ALPHA			0x8598
#define GL_OPERAND1_ALPHA			0x8599
#define GL_OPERAND2_ALPHA			0x859A
#define GL_RGB_SCALE				0x8573
#define GL_ADD_SIGNED				0x8574
#define GL_INTERPOLATE				0x8575
#define GL_SUBTRACT					0x84E7
#define GL_CONSTANT					0x8576
#define GL_PRIMARY_COLOR			0x8577
#define GL_PREVIOUS					0x8578
#endif

#ifndef GL_MAX_ELEMENTS_VERTICES
#define GL_MAX_ELEMENTS_VERTICES		0x80E8
#endif
#ifndef GL_MAX_ELEMENTS_INDICES
#define GL_MAX_ELEMENTS_INDICES			0x80E9
#endif


#ifndef GL_TEXTURE_3D
#define GL_PACK_SKIP_IMAGES			0x806B
#define GL_PACK_IMAGE_HEIGHT			0x806C
#define GL_UNPACK_SKIP_IMAGES			0x806D
#define GL_UNPACK_IMAGE_HEIGHT			0x806E
#define GL_TEXTURE_3D				0x806F
#define GL_PROXY_TEXTURE_3D			0x8070
#define GL_TEXTURE_DEPTH			0x8071
#define GL_TEXTURE_WRAP_R			0x8072
#define GL_MAX_3D_TEXTURE_SIZE			0x8073
#define GL_TEXTURE_BINDING_3D			0x806A
extern void (GLAPIENTRY *qglTexImage3D)(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#endif

#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_X
#define GL_NORMAL_MAP			    0x8511
#define GL_REFLECTION_MAP		    0x8512
#define GL_TEXTURE_CUBE_MAP		    0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP	    0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X     0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X     0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y     0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y     0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z     0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z     0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP	    0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE		0x851C
#endif

#ifndef GL_DEPTH_COMPONENT16_ARB
#define GL_DEPTH_COMPONENT16_ARB       0x81A5
#define GL_DEPTH_COMPONENT24_ARB       0x81A6
#define GL_DEPTH_COMPONENT32_ARB       0x81A7
#define GL_TEXTURE_DEPTH_SIZE_ARB      0x884A
#define GL_DEPTH_TEXTURE_MODE_ARB      0x884B
#endif

#ifndef GL_SCISSOR_TEST
#define GL_SCISSOR_TEST				0x0C11
#define GL_SCISSOR_BOX				0x0C10
#endif

// GL_SGIS_texture_edge_clamp or GL_EXT_texture_edge_clamp
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

//GL_ATI_separate_stencil
#ifndef GL_STENCIL_BACK_FUNC
#define GL_STENCIL_BACK_FUNC              0x8800
#define GL_STENCIL_BACK_FAIL              0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL   0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS   0x8803
#endif
extern void (GLAPIENTRY *qglStencilOpSeparate)(GLenum, GLenum, GLenum, GLenum);
extern void (GLAPIENTRY *qglStencilFuncSeparate)(GLenum, GLenum, GLint, GLuint);

//GL_EXT_stencil_two_side
#define GL_STENCIL_TEST_TWO_SIDE_EXT      0x8910
#define GL_ACTIVE_STENCIL_FACE_EXT        0x8911
extern void (GLAPIENTRY *qglActiveStencilFaceEXT)(GLenum);

//GL_EXT_blend_minmax
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD                   0x8006 // also supplied by GL_blend_subtract
#define GL_MIN                        0x8007
#define GL_MAX                        0x8008
#define GL_BLEND_EQUATION             0x8009 // also supplied by GL_blend_subtract
extern void (GLAPIENTRY *qglBlendEquationEXT)(GLenum); // also supplied by GL_blend_subtract
#endif

//GL_EXT_blend_subtract
#ifndef GL_FUNC_SUBTRACT
#define GL_FUNC_SUBTRACT              0x800A
#define GL_FUNC_REVERSE_SUBTRACT      0x800B
extern void (GLAPIENTRY *qglBlendEquationEXT)(GLenum); // also supplied by GL_blend_subtract
#endif

//GL_ARB_texture_non_power_of_two

//GL_ARB_vertex_buffer_object
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER               0x8892
#define GL_ELEMENT_ARRAY_BUFFER       0x8893
#define GL_ARRAY_BUFFER_BINDING       0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_STREAM_DRAW                0x88E0
#define GL_STREAM_READ                0x88E1
#define GL_STREAM_COPY                0x88E2
#define GL_STATIC_DRAW                0x88E4
#define GL_STATIC_READ                0x88E5
#define GL_STATIC_COPY                0x88E6
#define GL_DYNAMIC_DRAW               0x88E8
#define GL_DYNAMIC_READ               0x88E9
#define GL_DYNAMIC_COPY               0x88EA
#define GL_READ_ONLY                  0x88B8
#define GL_WRITE_ONLY                 0x88B9
#define GL_READ_WRITE                 0x88BA
#define GL_BUFFER_SIZE                0x8764
#define GL_BUFFER_USAGE               0x8765
#define GL_BUFFER_ACCESS              0x88BB
#define GL_BUFFER_MAPPED              0x88BC
#define GL_BUFFER_MAP_POINTER         0x88BD
#endif
extern void (GLAPIENTRY *qglBindBufferARB) (GLenum target, GLuint buffer);
extern void (GLAPIENTRY *qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
extern void (GLAPIENTRY *qglGenBuffersARB) (GLsizei n, GLuint *buffers);
extern GLboolean (GLAPIENTRY *qglIsBufferARB) (GLuint buffer);
extern GLvoid* (GLAPIENTRY *qglMapBufferARB) (GLenum target, GLenum access);
extern GLboolean (GLAPIENTRY *qglUnmapBufferARB) (GLenum target);
extern void (GLAPIENTRY *qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
extern void (GLAPIENTRY *qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

//GL_ARB_framebuffer_object
// (slight differences from GL_EXT_framebuffer_object as this integrates GL_EXT_packed_depth_stencil)
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER                     0x8D40
#define GL_READ_FRAMEBUFFER                0x8CA8
#define GL_DRAW_FRAMEBUFFER                0x8CA9
#define GL_RENDERBUFFER                    0x8D41
#define GL_STENCIL_INDEX1                  0x8D46
#define GL_STENCIL_INDEX4                  0x8D47
#define GL_STENCIL_INDEX8                  0x8D48
#define GL_STENCIL_INDEX16                 0x8D49
#define GL_RENDERBUFFER_WIDTH              0x8D42
#define GL_RENDERBUFFER_HEIGHT             0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT    0x8D44
#define GL_RENDERBUFFER_RED_SIZE           0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE         0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE          0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE         0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE         0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE       0x8D55
#define GL_RENDERBUFFER_SAMPLES            0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE            0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME            0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL          0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE  0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER          0x8CD4
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING         0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE         0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE               0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE             0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE              0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE             0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE             0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE           0x8217
#define GL_SRGB                                          0x8C40
#define GL_UNSIGNED_NORMALIZED                           0x8C17
#define GL_FRAMEBUFFER_DEFAULT                           0x8218
#define GL_INDEX                                         0x8222
#define GL_COLOR_ATTACHMENT0                0x8CE0
#define GL_COLOR_ATTACHMENT1                0x8CE1
#define GL_COLOR_ATTACHMENT2                0x8CE2
#define GL_COLOR_ATTACHMENT3                0x8CE3
#define GL_COLOR_ATTACHMENT4                0x8CE4
#define GL_COLOR_ATTACHMENT5                0x8CE5
#define GL_COLOR_ATTACHMENT6                0x8CE6
#define GL_COLOR_ATTACHMENT7                0x8CE7
#define GL_COLOR_ATTACHMENT8                0x8CE8
#define GL_COLOR_ATTACHMENT9                0x8CE9
#define GL_COLOR_ATTACHMENT10               0x8CEA
#define GL_COLOR_ATTACHMENT11               0x8CEB
#define GL_COLOR_ATTACHMENT12               0x8CEC
#define GL_COLOR_ATTACHMENT13               0x8CED
#define GL_COLOR_ATTACHMENT14               0x8CEE
#define GL_COLOR_ATTACHMENT15               0x8CEF
#define GL_DEPTH_ATTACHMENT                 0x8D00
#define GL_STENCIL_ATTACHMENT               0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT         0x821A
#define GL_MAX_SAMPLES                     0x8D57
#define GL_FRAMEBUFFER_COMPLETE                          0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT             0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT     0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER            0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER            0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED                       0x8CDD
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE            0x8D56
#define GL_FRAMEBUFFER_UNDEFINED                         0x8219
#define GL_FRAMEBUFFER_BINDING             0x8CA6 // alias DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING        0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING        0x8CAA
#define GL_RENDERBUFFER_BINDING            0x8CA7
#define GL_MAX_COLOR_ATTACHMENTS           0x8CDF
#define GL_MAX_RENDERBUFFER_SIZE           0x84E8
#define GL_INVALID_FRAMEBUFFER_OPERATION   0x0506
#define GL_DEPTH_STENCIL                              0x84F9
#define GL_UNSIGNED_INT_24_8                          0x84FA
#define GL_DEPTH24_STENCIL8                           0x88F0
#define GL_TEXTURE_STENCIL_SIZE                       0x88F1
#endif
extern GLboolean (GLAPIENTRY *qglIsRenderbuffer)(GLuint renderbuffer);
extern GLvoid (GLAPIENTRY *qglBindRenderbuffer)(GLenum target, GLuint renderbuffer);
extern GLvoid (GLAPIENTRY *qglDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
extern GLvoid (GLAPIENTRY *qglGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
extern GLvoid (GLAPIENTRY *qglRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern GLvoid (GLAPIENTRY *qglRenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
extern GLvoid (GLAPIENTRY *qglGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
extern GLboolean (GLAPIENTRY *qglIsFramebuffer)(GLuint framebuffer);
extern GLvoid (GLAPIENTRY *qglBindFramebuffer)(GLenum target, GLuint framebuffer);
extern GLvoid (GLAPIENTRY *qglDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
extern GLvoid (GLAPIENTRY *qglGenFramebuffers)(GLsizei n, GLuint *framebuffers);
extern GLenum (GLAPIENTRY *qglCheckFramebufferStatus)(GLenum target);
extern GLvoid (GLAPIENTRY *qglFramebufferTexture1D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLvoid (GLAPIENTRY *qglFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLvoid (GLAPIENTRY *qglFramebufferTexture3D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
extern GLvoid (GLAPIENTRY *qglFramebufferTextureLayer)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
extern GLvoid (GLAPIENTRY *qglFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern GLvoid (GLAPIENTRY *qglGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
extern GLvoid (GLAPIENTRY *qglBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern GLvoid (GLAPIENTRY *qglGenerateMipmap)(GLenum target);

// GL_ARB_draw_buffers
#ifndef GL_MAX_DRAW_BUFFERS_ARB
#define GL_MAX_DRAW_BUFFERS_ARB                              0x8824
#define GL_DRAW_BUFFER0_ARB                                  0x8825
#define GL_DRAW_BUFFER1_ARB                                  0x8826
#define GL_DRAW_BUFFER2_ARB                                  0x8827
#define GL_DRAW_BUFFER3_ARB                                  0x8828
#define GL_DRAW_BUFFER4_ARB                                  0x8829
#define GL_DRAW_BUFFER5_ARB                                  0x882A
#define GL_DRAW_BUFFER6_ARB                                  0x882B
#define GL_DRAW_BUFFER7_ARB                                  0x882C
#define GL_DRAW_BUFFER8_ARB                                  0x882D
#define GL_DRAW_BUFFER9_ARB                                  0x882E
#define GL_DRAW_BUFFER10_ARB                                 0x882F
#define GL_DRAW_BUFFER11_ARB                                 0x8830
#define GL_DRAW_BUFFER12_ARB                                 0x8831
#define GL_DRAW_BUFFER13_ARB                                 0x8832
#define GL_DRAW_BUFFER14_ARB                                 0x8833
#define GL_DRAW_BUFFER15_ARB                                 0x8834
#endif
extern void (GLAPIENTRY *qglDrawBuffersARB)(GLsizei n, const GLenum *bufs);

// GL_ARB_texture_float
#ifndef GL_RGBA32F_ARB
#define GL_RGBA32F_ARB                                       0x8814
#define GL_RGB32F_ARB                                        0x8815
#define GL_ALPHA32F_ARB                                      0x8816
#define GL_INTENSITY32F_ARB                                  0x8817
#define GL_LUMINANCE32F_ARB                                  0x8818
#define GL_LUMINANCE_ALPHA32F_ARB                            0x8819
#define GL_RGBA16F_ARB                                       0x881A
#define GL_RGB16F_ARB                                        0x881B
#define GL_ALPHA16F_ARB                                      0x881C
#define GL_INTENSITY16F_ARB                                  0x881D
#define GL_LUMINANCE16F_ARB                                  0x881E
#define GL_LUMINANCE_ALPHA16F_ARB                            0x881F
#endif

// GL_EXT_texture_sRGB
#ifndef GL_SRGB_EXT
#define GL_SRGB_EXT                                          0x8C40
#define GL_SRGB8_EXT                                         0x8C41
#define GL_SRGB_ALPHA_EXT                                    0x8C42
#define GL_SRGB8_ALPHA8_EXT                                  0x8C43
#define GL_SLUMINANCE_ALPHA_EXT                              0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT                            0x8C45
#define GL_SLUMINANCE_EXT                                    0x8C46
#define GL_SLUMINANCE8_EXT                                   0x8C47
#define GL_COMPRESSED_SRGB_EXT                               0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT                         0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT                         0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT                   0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT                     0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT               0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT               0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT               0x8C4F
#endif

// GL_ARB_uniform_buffer_object
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER                                    0x8A11
#define GL_UNIFORM_BUFFER_BINDING                            0x8A28
#define GL_UNIFORM_BUFFER_START                              0x8A29
#define GL_UNIFORM_BUFFER_SIZE                               0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS                         0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS                       0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS                       0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS                       0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS                       0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE                            0x8A30
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS            0x8A31
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS          0x8A32
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS          0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT                   0x8A34
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH              0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS                             0x8A36
#define GL_UNIFORM_TYPE                                      0x8A37
#define GL_UNIFORM_SIZE                                      0x8A38
#define GL_UNIFORM_NAME_LENGTH                               0x8A39
#define GL_UNIFORM_BLOCK_INDEX                               0x8A3A
#define GL_UNIFORM_OFFSET                                    0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE                              0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE                             0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR                              0x8A3E
#define GL_UNIFORM_BLOCK_BINDING                             0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE                           0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH                         0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS                     0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES              0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER         0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER       0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER       0x8A46
#define GL_INVALID_INDEX                                     0xFFFFFFFFu
#endif
extern void (GLAPIENTRY *qglGetUniformIndices)(GLuint program, GLsizei uniformCount, const char** uniformNames, GLuint* uniformIndices);
extern void (GLAPIENTRY *qglGetActiveUniformsiv)(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
extern void (GLAPIENTRY *qglGetActiveUniformName)(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, char* uniformName);
extern GLuint (GLAPIENTRY *qglGetUniformBlockIndex)(GLuint program, const char* uniformBlockName);
extern void (GLAPIENTRY *qglGetActiveUniformBlockiv)(GLuint program, GLuint uniformBlockIndex, GLenum pname,  GLint* params);
extern void (GLAPIENTRY *qglGetActiveUniformBlockName)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, char* uniformBlockName);
extern void (GLAPIENTRY *qglBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptrARB offset, GLsizeiptrARB size);
extern void (GLAPIENTRY *qglBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
extern void (GLAPIENTRY *qglGetIntegeri_v)(GLenum target, GLuint index, GLint* data);
extern void (GLAPIENTRY *qglUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

extern void (GLAPIENTRY *qglScissor)(GLint x, GLint y, GLsizei width, GLsizei height);

extern void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

extern void (GLAPIENTRY *qglClear)(GLbitfield mask);

extern void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
extern void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
extern void (GLAPIENTRY *qglCullFace)(GLenum mode);

extern void (GLAPIENTRY *qglDrawBuffer)(GLenum mode);
extern void (GLAPIENTRY *qglReadBuffer)(GLenum mode);
extern void (GLAPIENTRY *qglEnable)(GLenum cap);
extern void (GLAPIENTRY *qglDisable)(GLenum cap);
extern GLboolean (GLAPIENTRY *qglIsEnabled)(GLenum cap);

extern void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
extern void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

extern void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
extern void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
extern void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

extern GLenum (GLAPIENTRY *qglGetError)(void);
extern const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
extern void (GLAPIENTRY *qglFinish)(void);
extern void (GLAPIENTRY *qglFlush)(void);

extern void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
extern void (GLAPIENTRY *qglDepthFunc)(GLenum func);
extern void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
extern void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);
extern void (GLAPIENTRY *qglDepthRangef)(GLclampf near_val, GLclampf far_val);
extern void (GLAPIENTRY *qglColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

extern void (GLAPIENTRY *qglDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
extern void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern void (GLAPIENTRY *qglDrawArrays)(GLenum mode, GLint first, GLsizei count);
extern void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
extern void (GLAPIENTRY *qglArrayElement)(GLint i);

extern void (GLAPIENTRY *qglColor4ub)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
extern void (GLAPIENTRY *qglColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void (GLAPIENTRY *qglTexCoord1f)(GLfloat s);
extern void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
extern void (GLAPIENTRY *qglTexCoord3f)(GLfloat s, GLfloat t, GLfloat r);
extern void (GLAPIENTRY *qglTexCoord4f)(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
extern void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
extern void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
extern void (GLAPIENTRY *qglVertex4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
extern void (GLAPIENTRY *qglBegin)(GLenum mode);
extern void (GLAPIENTRY *qglEnd)(void);

extern void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
//extern void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
//extern void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
extern void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
//extern void (GLAPIENTRY *qglPushMatrix)(void);
//extern void (GLAPIENTRY *qglPopMatrix)(void);
extern void (GLAPIENTRY *qglLoadIdentity)(void);
//extern void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
extern void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
//extern void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
//extern void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
//extern void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
//extern void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
//extern void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
//extern void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
//extern void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
//extern void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

extern void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

extern void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
extern void (GLAPIENTRY *qglStencilMask)(GLuint mask);
extern void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
extern void (GLAPIENTRY *qglClearStencil)(GLint s);

extern void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglTexEnvfv)(GLenum target, GLenum pname, const GLfloat *params);
extern void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);
extern void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);
extern void (GLAPIENTRY *qglGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetTexLevelParameterfv)(GLenum target, GLint level, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
extern void (GLAPIENTRY *qglHint)(GLenum target, GLenum mode);

extern void (GLAPIENTRY *qglGenTextures)(GLsizei n, GLuint *textures);
extern void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
extern void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
//extern void (GLAPIENTRY *qglPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities);
//extern GLboolean (GLAPIENTRY *qglAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences);
//extern GLboolean (GLAPIENTRY *qglIsTexture)(GLuint texture);
//extern void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
extern void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

//extern void (GLAPIENTRY *qglTexImage1D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
//extern void (GLAPIENTRY *qglTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
extern void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
//extern void (GLAPIENTRY *qglCopyTexImage1D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
extern void (GLAPIENTRY *qglCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
//extern void (GLAPIENTRY *qglCopyTexSubImage1D)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
extern void (GLAPIENTRY *qglCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

extern void (GLAPIENTRY *qglPolygonOffset)(GLfloat factor, GLfloat units);
extern void (GLAPIENTRY *qglPolygonMode)(GLenum face, GLenum mode);

//extern void (GLAPIENTRY *qglClipPlane)(GLenum plane, const GLdouble *equation);
//extern void (GLAPIENTRY *qglGetClipPlane)(GLenum plane, GLdouble *equation);

//[515]: added on 29.07.2005
extern void (GLAPIENTRY *qglLineWidth)(GLfloat width);
extern void (GLAPIENTRY *qglPointSize)(GLfloat size);

// GL 2.0 shader objects
#ifndef GL_PROGRAM_OBJECT
// 1-byte character string
typedef char GLchar;
#endif
extern void (GLAPIENTRY *qglDeleteShader)(GLuint obj);
extern void (GLAPIENTRY *qglDeleteProgram)(GLuint obj);
//extern GLuint (GLAPIENTRY *qglGetHandle)(GLenum pname);
extern void (GLAPIENTRY *qglDetachShader)(GLuint containerObj, GLuint attachedObj);
extern GLuint (GLAPIENTRY *qglCreateShader)(GLenum shaderType);
extern void (GLAPIENTRY *qglShaderSource)(GLuint shaderObj, GLsizei count, const GLchar **string, const GLint *length);
extern void (GLAPIENTRY *qglCompileShader)(GLuint shaderObj);
extern GLuint (GLAPIENTRY *qglCreateProgram)(void);
extern void (GLAPIENTRY *qglAttachShader)(GLuint containerObj, GLuint obj);
extern void (GLAPIENTRY *qglLinkProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglUseProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglValidateProgram)(GLuint programObj);
extern void (GLAPIENTRY *qglUniform1f)(GLint location, GLfloat v0);
extern void (GLAPIENTRY *qglUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglUniform1i)(GLint location, GLint v0);
extern void (GLAPIENTRY *qglUniform2i)(GLint location, GLint v0, GLint v1);
extern void (GLAPIENTRY *qglUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
extern void (GLAPIENTRY *qglUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern void (GLAPIENTRY *qglUniform1fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
extern void (GLAPIENTRY *qglUniform1iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform2iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform3iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniform4iv)(GLint location, GLsizei count, const GLint *value);
extern void (GLAPIENTRY *qglUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (GLAPIENTRY *qglGetShaderiv)(GLuint obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetProgramiv)(GLuint obj, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetShaderInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (GLAPIENTRY *qglGetProgramInfoLog)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void (GLAPIENTRY *qglGetAttachedShaders)(GLuint containerObj, GLsizei maxCount, GLsizei *count, GLuint *obj);
extern GLint (GLAPIENTRY *qglGetUniformLocation)(GLuint programObj, const GLchar *name);
extern void (GLAPIENTRY *qglGetActiveUniform)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern void (GLAPIENTRY *qglGetUniformfv)(GLuint programObj, GLint location, GLfloat *params);
extern void (GLAPIENTRY *qglGetUniformiv)(GLuint programObj, GLint location, GLint *params);
extern void (GLAPIENTRY *qglGetShaderSource)(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *source);
extern void (GLAPIENTRY *qglPolygonStipple)(const GLubyte *mask);
#ifndef GL_PROGRAM_OBJECT
#define GL_PROGRAM_OBJECT					0x8B40
#define GL_DELETE_STATUS					0x8B80
#define GL_COMPILE_STATUS					0x8B81
#define GL_LINK_STATUS						0x8B82
#define GL_VALIDATE_STATUS					0x8B83
#define GL_INFO_LOG_LENGTH					0x8B84
#define GL_ATTACHED_SHADERS					0x8B85
#define GL_ACTIVE_UNIFORMS					0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH		0x8B87
#define GL_SHADER_SOURCE_LENGTH				0x8B88
#define GL_SHADER_OBJECT					0x8B48
#define GL_SHADER_TYPE						0x8B4F
#define GL_FLOAT							0x1406
#define GL_FLOAT_VEC2						0x8B50
#define GL_FLOAT_VEC3						0x8B51
#define GL_FLOAT_VEC4						0x8B52
#define GL_INT								0x1404
#define GL_INT_VEC2							0x8B53
#define GL_INT_VEC3							0x8B54
#define GL_INT_VEC4							0x8B55
#define GL_BOOL								0x8B56
#define GL_BOOL_VEC2						0x8B57
#define GL_BOOL_VEC3						0x8B58
#define GL_BOOL_VEC4						0x8B59
#define GL_FLOAT_MAT2						0x8B5A
#define GL_FLOAT_MAT3						0x8B5B
#define GL_FLOAT_MAT4						0x8B5C
#define GL_SAMPLER_1D						0x8B5D
#define GL_SAMPLER_2D						0x8B5E
#define GL_SAMPLER_3D						0x8B5F
#define GL_SAMPLER_CUBE						0x8B60
#define GL_SAMPLER_1D_SHADOW				0x8B61
#define GL_SAMPLER_2D_SHADOW				0x8B62
#define GL_SAMPLER_2D_RECT					0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW			0x8B64
#endif

// GL 2.0 vertex shader
extern void (GLAPIENTRY *qglVertexAttrib1f)(GLuint index, GLfloat v0);
extern void (GLAPIENTRY *qglVertexAttrib1s)(GLuint index, GLshort v0);
extern void (GLAPIENTRY *qglVertexAttrib1d)(GLuint index, GLdouble v0);
extern void (GLAPIENTRY *qglVertexAttrib2f)(GLuint index, GLfloat v0, GLfloat v1);
extern void (GLAPIENTRY *qglVertexAttrib2s)(GLuint index, GLshort v0, GLshort v1);
extern void (GLAPIENTRY *qglVertexAttrib2d)(GLuint index, GLdouble v0, GLdouble v1);
extern void (GLAPIENTRY *qglVertexAttrib3f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (GLAPIENTRY *qglVertexAttrib3s)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
extern void (GLAPIENTRY *qglVertexAttrib3d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
extern void (GLAPIENTRY *qglVertexAttrib4f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (GLAPIENTRY *qglVertexAttrib4s)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
extern void (GLAPIENTRY *qglVertexAttrib4d)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
extern void (GLAPIENTRY *qglVertexAttrib4Nub)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
extern void (GLAPIENTRY *qglVertexAttrib1fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib1sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib1dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib2fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib2sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib2dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib3fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib3sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib3dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib4fv)(GLuint index, const GLfloat *v);
extern void (GLAPIENTRY *qglVertexAttrib4sv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib4dv)(GLuint index, const GLdouble *v);
extern void (GLAPIENTRY *qglVertexAttrib4iv)(GLuint index, const GLint *v);
extern void (GLAPIENTRY *qglVertexAttrib4bv)(GLuint index, const GLbyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4ubv)(GLuint index, const GLubyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4usv)(GLuint index, const GLushort *v);
extern void (GLAPIENTRY *qglVertexAttrib4uiv)(GLuint index, const GLuint *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nbv)(GLuint index, const GLbyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nsv)(GLuint index, const GLshort *v);
extern void (GLAPIENTRY *qglVertexAttrib4Niv)(GLuint index, const GLint *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nubv)(GLuint index, const GLubyte *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nusv)(GLuint index, const GLushort *v);
extern void (GLAPIENTRY *qglVertexAttrib4Nuiv)(GLuint index, const GLuint *v);
extern void (GLAPIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
extern void (GLAPIENTRY *qglEnableVertexAttribArray)(GLuint index);
extern void (GLAPIENTRY *qglDisableVertexAttribArray)(GLuint index);
extern void (GLAPIENTRY *qglBindAttribLocation)(GLuint programObj, GLuint index, const GLchar *name);
extern void (GLAPIENTRY *qglBindFragDataLocation)(GLuint programObj, GLuint index, const GLchar *name);
extern void (GLAPIENTRY *qglGetActiveAttrib)(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
extern GLint (GLAPIENTRY *qglGetAttribLocation)(GLuint programObj, const GLchar *name);
extern void (GLAPIENTRY *qglGetVertexAttribdv)(GLuint index, GLenum pname, GLdouble *params);
extern void (GLAPIENTRY *qglGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
extern void (GLAPIENTRY *qglGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetVertexAttribPointerv)(GLuint index, GLenum pname, GLvoid **pointer);
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER						0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS		0x8B4A
#define GL_MAX_VARYING_FLOATS					0x8B4B
#define GL_MAX_VERTEX_ATTRIBS					0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS				0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS		0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS		0x8B4D
#define GL_MAX_TEXTURE_COORDS					0x8871
#define GL_VERTEX_PROGRAM_POINT_SIZE			0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE				0x8643
#define GL_ACTIVE_ATTRIBUTES					0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH			0x8B8A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED			0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE				0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE			0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE				0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED		0x886A
#define GL_CURRENT_VERTEX_ATTRIB				0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER			0x8645
#define GL_FLOAT								0x1406
#define GL_FLOAT_VEC2							0x8B50
#define GL_FLOAT_VEC3							0x8B51
#define GL_FLOAT_VEC4							0x8B52
#define GL_FLOAT_MAT2							0x8B5A
#define GL_FLOAT_MAT3							0x8B5B
#define GL_FLOAT_MAT4							0x8B5C
#endif

// GL 2.0 fragment shader
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER						0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS		0x8B49
#define GL_MAX_TEXTURE_COORDS					0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS				0x8872
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT		0x8B8B
#endif

// GL 2.0 shading language 100
#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION				0x8B8C
#endif

// GL_ARB_texture_compression
extern void (GLAPIENTRY *qglCompressedTexImage3DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexImage2DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border,  GLsizei imageSize, const void *data);
//extern void (GLAPIENTRY *qglCompressedTexImage1DARB)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexSubImage3DARB)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglCompressedTexSubImage2DARB)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
//extern void (GLAPIENTRY *qglCompressedTexSubImage1DARB)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
extern void (GLAPIENTRY *qglGetCompressedTexImageARB)(GLenum target, GLint lod, void *img);
#ifndef GL_COMPRESSED_RGB_ARB
#define GL_COMPRESSED_ALPHA_ARB						0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB					0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB			0x84EB
#define GL_COMPRESSED_INTENSITY_ARB					0x84EC
#define GL_COMPRESSED_RGB_ARB						0x84ED
#define GL_COMPRESSED_RGBA_ARB						0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB				0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB		0x86A0
#define GL_TEXTURE_COMPRESSED_ARB					0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB		0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB			0x86A3
#endif

// GL_EXT_texture_compression_s3tc
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3
#endif

// GL_ARB_occlusion_query
extern void (GLAPIENTRY *qglGenQueriesARB)(GLsizei n, GLuint *ids);
extern void (GLAPIENTRY *qglDeleteQueriesARB)(GLsizei n, const GLuint *ids);
extern GLboolean (GLAPIENTRY *qglIsQueryARB)(GLuint qid);
extern void (GLAPIENTRY *qglBeginQueryARB)(GLenum target, GLuint qid);
extern void (GLAPIENTRY *qglEndQueryARB)(GLenum target);
extern void (GLAPIENTRY *qglGetQueryivARB)(GLenum target, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectivARB)(GLuint qid, GLenum pname, GLint *params);
extern void (GLAPIENTRY *qglGetQueryObjectuivARB)(GLuint qid, GLenum pname, GLuint *params);
#ifndef GL_SAMPLES_PASSED_ARB
#define GL_SAMPLES_PASSED_ARB                             0x8914
#define GL_QUERY_COUNTER_BITS_ARB                         0x8864
#define GL_CURRENT_QUERY_ARB                              0x8865
#define GL_QUERY_RESULT_ARB                               0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB                     0x8867
#endif

// GL_EXT_bgr
#define GL_BGR					0x80E0

// GL_EXT_bgra
#define GL_BGRA					0x80E1

//GL_AMD_texture_texture4

//GL_ARB_texture_gather

//GL_ARB_multisample
#define GL_MULTISAMPLE_ARB              0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE_ARB 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_ARB      0x809F
#define GL_SAMPLE_COVERAGE_ARB          0x80A0
#define GL_SAMPLE_BUFFERS_ARB           0x80A8
#define GL_SAMPLES_ARB                  0x80A9
#define GL_SAMPLE_COVERAGE_VALUE_ARB    0x80AA
#define GL_SAMPLE_COVERAGE_INVERT_ARB   0x80AB
#define GL_MULTISAMPLE_BIT_ARB          0x20000000
extern void (GLAPIENTRY *qglSampleCoverageARB)(GLclampf value, GLboolean invert);

extern void (GLAPIENTRY *qglPointSize)(GLfloat size);

//GL_EXT_packed_depth_stencil
#define GL_DEPTH_STENCIL_EXT            0x84F9
#define GL_UNSIGNED_INT_24_8_EXT        0x84FA
#define GL_DEPTH24_STENCIL8_EXT         0x88F0

//GL_EXT_blend_func_separate
#ifndef GL_BLEND_DST_RGB
#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_ALPHA                0x80CA
#define GL_BLEND_SRC_ALPHA                0x80CB
#endif
extern void (GLAPIENTRY *qglBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

#endif

#define DEBUGGL

#ifdef DEBUGGL
#ifdef USE_GLES2
#define CHECKGLERROR {if (gl_paranoid.integer){if (gl_printcheckerror.integer) Con_Printf("CHECKGLERROR at %s:%d\n", __FILE__, __LINE__);errornumber = glGetError();if (errornumber) GL_PrintError(errornumber, __FILE__, __LINE__);}}
#else
#define CHECKGLERROR {if (gl_paranoid.integer){if (gl_printcheckerror.integer) Con_Printf("CHECKGLERROR at %s:%d\n", __FILE__, __LINE__);errornumber = qglGetError ? qglGetError() : 0;if (errornumber) GL_PrintError(errornumber, __FILE__, __LINE__);}}
#endif
extern int errornumber;
void GL_PrintError(int errornumber, const char *filename, int linenumber);
#else
#define CHECKGLERROR
#endif

#ifdef USE_GLES2
#define qglIsBufferARB glIsBuffer
#define qglIsEnabled glIsEnabled
#define qglIsFramebufferEXT glIsFramebuffer
//#define qglIsQueryARB glIsQuery
#define qglIsRenderbufferEXT glIsRenderbuffer
//#define qglUnmapBufferARB glUnmapBuffer
#define qglCheckFramebufferStatus glCheckFramebufferStatus
#define qglGetError glGetError
#define qglCreateProgram glCreateProgram
#define qglCreateShader glCreateShader
//#define qglGetHandleARB glGetHandle
#define qglGetAttribLocation glGetAttribLocation
#define qglGetUniformLocation glGetUniformLocation
//#define qglMapBufferARB glMapBuffer
#define qglGetString glGetString
//#define qglActiveStencilFaceEXT glActiveStencilFace
#define qglActiveTexture glActiveTexture
#define qglAlphaFunc glAlphaFunc
#define qglArrayElement glArrayElement
#define qglAttachShader glAttachShader
//#define qglBegin glBegin
//#define qglBeginQueryARB glBeginQuery
#define qglBindAttribLocation glBindAttribLocation
//#define qglBindFragDataLocation glBindFragDataLocation
#define qglBindBufferARB glBindBuffer
#define qglBindFramebuffer glBindFramebuffer
#define qglBindRenderbuffer glBindRenderbuffer
#define qglBindTexture glBindTexture
#define qglBlendEquationEXT glBlendEquation
#define qglBlendFunc glBlendFunc
#define qglBlendFuncSeparate glBlendFuncSeparate
#define qglBufferDataARB glBufferData
#define qglBufferSubDataARB glBufferSubData
#define qglClear glClear
#define qglClearColor glClearColor
#define qglClearDepthf glClearDepthf
#define qglClearStencil glClearStencil
#define qglClientActiveTexture glClientActiveTexture
#define qglColor4f glColor4f
#define qglColor4ub glColor4ub
#define qglColorMask glColorMask
#define qglColorPointer glColorPointer
#define qglCompileShader glCompileShader
#define qglCompressedTexImage2DARB glCompressedTexImage2D
#define qglCompressedTexImage3DARB glCompressedTexImage3D
#define qglCompressedTexSubImage2DARB glCompressedTexSubImage2D
#define qglCompressedTexSubImage3DARB glCompressedTexSubImage3D
#define qglCopyTexImage2D glCopyTexImage2D
#define qglCopyTexSubImage2D glCopyTexSubImage2D
#define qglCopyTexSubImage3D glCopyTexSubImage3D
#define qglCullFace glCullFace
#define qglDeleteBuffersARB glDeleteBuffers
#define qglDeleteFramebuffers glDeleteFramebuffers
#define qglDeleteProgram glDeleteProgram
#define qglDeleteShader glDeleteShader
//#define qglDeleteQueriesARB glDeleteQueries
#define qglDeleteRenderbuffers glDeleteRenderbuffers
#define qglDeleteTextures glDeleteTextures
#define qglDepthFunc glDepthFunc
#define qglDepthMask glDepthMask
#define qglDepthRangef glDepthRangef
#define qglDetachShader glDetachShader
#define qglDisable glDisable
#define qglDisableClientState glDisableClientState
#define qglDisableVertexAttribArray glDisableVertexAttribArray
#define qglDrawArrays glDrawArrays
//#define qglDrawBuffer glDrawBuffer
//#define qglDrawBuffersARB glDrawBuffers
#define qglDrawElements glDrawElements
//#define qglDrawRangeElements glDrawRangeElements
#define qglEnable glEnable
#define qglEnableClientState glEnableClientState
#define qglEnableVertexAttribArray glEnableVertexAttribArray
//#define qglEnd glEnd
//#define qglEndQueryARB glEndQuery
#define qglFinish glFinish
#define qglFlush glFlush
#define qglFramebufferRenderbuffer glFramebufferRenderbuffer
#define qglFramebufferTexture2D glFramebufferTexture2D
#define qglFramebufferTexture3DEXT glFramebufferTexture3D
#define qglGenBuffersARB glGenBuffers
#define qglGenFramebuffers glGenFramebuffers
//#define qglGenQueriesARB glGenQueries
#define qglGenRenderbuffers glGenRenderbuffers
#define qglGenTextures glGenTextures
#define qglGenerateMipmapEXT glGenerateMipmap
#define qglGetActiveAttrib glGetActiveAttrib
#define qglGetActiveUniform glGetActiveUniform
#define qglGetAttachedShaders glGetAttachedShaders
#define qglGetBooleanv glGetBooleanv
//#define qglGetCompressedTexImageARB glGetCompressedTexImage
#define qglGetDoublev glGetDoublev
#define qglGetFloatv glGetFloatv
#define qglGetFramebufferAttachmentParameterivEXT glGetFramebufferAttachmentParameteriv
#define qglGetProgramInfoLog glGetProgramInfoLog
#define qglGetShaderInfoLog glGetShaderInfoLog
#define qglGetIntegerv glGetIntegerv
#define qglGetShaderiv glGetShaderiv
#define qglGetProgramiv glGetProgramiv
//#define qglGetQueryObjectivARB glGetQueryObjectiv
//#define qglGetQueryObjectuivARB glGetQueryObjectuiv
//#define qglGetQueryivARB glGetQueryiv
#define qglGetRenderbufferParameterivEXT glGetRenderbufferParameteriv
#define qglGetShaderSource glGetShaderSource
#define qglGetTexImage glGetTexImage
#define qglGetTexLevelParameterfv glGetTexLevelParameterfv
#define qglGetTexLevelParameteriv glGetTexLevelParameteriv
#define qglGetTexParameterfv glGetTexParameterfv
#define qglGetTexParameteriv glGetTexParameteriv
#define qglGetUniformfv glGetUniformfv
#define qglGetUniformiv glGetUniformiv
#define qglHint glHint
#define qglLineWidth glLineWidth
#define qglLinkProgram glLinkProgram
#define qglLoadIdentity glLoadIdentity
#define qglLoadMatrixf glLoadMatrixf
#define qglMatrixMode glMatrixMode
#define qglMultiTexCoord1f glMultiTexCoord1f
#define qglMultiTexCoord2f glMultiTexCoord2f
#define qglMultiTexCoord3f glMultiTexCoord3f
#define qglMultiTexCoord4f glMultiTexCoord4f
#define qglNormalPointer glNormalPointer
#define qglPixelStorei glPixelStorei
#define qglPointSize glPointSize
//#define qglPolygonMode glPolygonMode
#define qglPolygonOffset glPolygonOffset
//#define qglPolygonStipple glPolygonStipple
#define qglReadBuffer glReadBuffer
#define qglReadPixels glReadPixels
#define qglRenderbufferStorage glRenderbufferStorage
#define qglScissor glScissor
#define qglShaderSource glShaderSource
#define qglStencilFunc glStencilFunc
#define qglStencilFuncSeparate glStencilFuncSeparate
#define qglStencilMask glStencilMask
#define qglStencilOp glStencilOp
#define qglStencilOpSeparate glStencilOpSeparate
#define qglTexCoord1f glTexCoord1f
#define qglTexCoord2f glTexCoord2f
#define qglTexCoord3f glTexCoord3f
#define qglTexCoord4f glTexCoord4f
#define qglTexCoordPointer glTexCoordPointer
#define qglTexEnvf glTexEnvf
#define qglTexEnvfv glTexEnvfv
#define qglTexEnvi glTexEnvi
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
#define qglUniformMatrix2fv glUniformMatrix2fv
#define qglUniformMatrix3fv glUniformMatrix3fv
#define qglUniformMatrix4fv glUniformMatrix4fv
#define qglUseProgram glUseProgram
#define qglValidateProgram glValidateProgram
#define qglVertex2f glVertex2f
#define qglVertex3f glVertex3f
#define qglVertex4f glVertex4f
#define qglVertexAttribPointer glVertexAttribPointer
#define qglVertexPointer glVertexPointer
#define qglViewport glViewport
#define qglVertexAttrib1f glVertexAttrib1f
//#define qglVertexAttrib1s glVertexAttrib1s
//#define qglVertexAttrib1d glVertexAttrib1d
#define qglVertexAttrib2f glVertexAttrib2f
//#define qglVertexAttrib2s glVertexAttrib2s
//#define qglVertexAttrib2d glVertexAttrib2d
#define qglVertexAttrib3f glVertexAttrib3f
//#define qglVertexAttrib3s glVertexAttrib3s
//#define qglVertexAttrib3d glVertexAttrib3d
#define qglVertexAttrib4f glVertexAttrib4f
//#define qglVertexAttrib4s glVertexAttrib4s
//#define qglVertexAttrib4d glVertexAttrib4d
//#define qglVertexAttrib4Nub glVertexAttrib4Nub
#define qglVertexAttrib1fv glVertexAttrib1fv
//#define qglVertexAttrib1sv glVertexAttrib1sv
//#define qglVertexAttrib1dv glVertexAttrib1dv
#define qglVertexAttrib2fv glVertexAttrib2fv
//#define qglVertexAttrib2sv glVertexAttrib2sv
//#define qglVertexAttrib2dv glVertexAttrib2dv
#define qglVertexAttrib3fv glVertexAttrib3fv
//#define qglVertexAttrib3sv glVertexAttrib3sv
//#define qglVertexAttrib3dv glVertexAttrib3dv
#define qglVertexAttrib4fv glVertexAttrib4fv
//#define qglVertexAttrib4sv glVertexAttrib4sv
//#define qglVertexAttrib4dv glVertexAttrib4dv
//#define qglVertexAttrib4iv glVertexAttrib4iv
//#define qglVertexAttrib4bv glVertexAttrib4bv
//#define qglVertexAttrib4ubv glVertexAttrib4ubv
//#define qglVertexAttrib4usv glVertexAttrib4usv
//#define qglVertexAttrib4uiv glVertexAttrib4uiv
//#define qglVertexAttrib4Nbv glVertexAttrib4Nbv
//#define qglVertexAttrib4Nsv glVertexAttrib4Nsv
//#define qglVertexAttrib4Niv glVertexAttrib4Niv
//#define qglVertexAttrib4Nubv glVertexAttrib4Nubv
//#define qglVertexAttrib4Nusv glVertexAttrib4Nusv
//#define qglVertexAttrib4Nuiv glVertexAttrib4Nuiv
//#define qglGetVertexAttribdv glGetVertexAttribdv
#define qglGetVertexAttribfv glGetVertexAttribfv
#define qglGetVertexAttribiv glGetVertexAttribiv
#define qglGetVertexAttribPointerv glGetVertexAttribPointerv
#endif

#endif
