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
// disable data conversion warnings

#ifdef _MSC_VER
//#pragma warning(disable : 4244)     // MIPS
//#pragma warning(disable : 4136)     // X86
//#pragma warning(disable : 4051)     // ALPHA
#pragma warning(disable : 4244)     // LordHavoc: MSVC++ 4 x86, double/float
#pragma warning(disable : 4305)		// LordHavoc: MSVC++ 6 x86, double/float
#pragma warning(disable : 4018)		// LordHavoc: MSVC++ 4 x86, signed/unsigned mismatch
#endif

#ifdef _WIN32
#include <windows.h>
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#include <GL/gl.h>
//#include <GL/glu.h>

extern qboolean isG200;
extern qboolean isRagePro;

extern float gldepthmin, gldepthmax;

//====================================================

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;

#ifndef _WIN32
#define APIENTRY /* */
#endif

// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

// GL_ARB_multitexture
extern int gl_textureunits;
extern void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglActiveTexture) (GLenum);
extern void (GLAPIENTRY *qglClientActiveTexture) (GLenum);
#ifndef GL_ACTIVE_TEXTURE_ARB
#define GL_ACTIVE_TEXTURE_ARB			0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB	0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB		0x84E2
#define GL_TEXTURE0_ARB					0x84C0
#define GL_TEXTURE1_ARB					0x84C1
#define GL_TEXTURE2_ARB					0x84C2
#define GL_TEXTURE3_ARB					0x84C3
#define GL_TEXTURE4_ARB					0x84C4
#define GL_TEXTURE5_ARB					0x84C5
#define GL_TEXTURE6_ARB					0x84C6
#define GL_TEXTURE7_ARB					0x84C7
#define GL_TEXTURE8_ARB					0x84C8
#define GL_TEXTURE9_ARB					0x84C9
#define GL_TEXTURE10_ARB				0x84CA
#define GL_TEXTURE11_ARB				0x84CB
#define GL_TEXTURE12_ARB				0x84CC
#define GL_TEXTURE13_ARB				0x84CD
#define GL_TEXTURE14_ARB				0x84CE
#define GL_TEXTURE15_ARB				0x84CF
#define GL_TEXTURE16_ARB				0x84D0
#define GL_TEXTURE17_ARB				0x84D1
#define GL_TEXTURE18_ARB				0x84D2
#define GL_TEXTURE19_ARB				0x84D3
#define GL_TEXTURE20_ARB				0x84D4
#define GL_TEXTURE21_ARB				0x84D5
#define GL_TEXTURE22_ARB				0x84D6
#define GL_TEXTURE23_ARB				0x84D7
#define GL_TEXTURE24_ARB				0x84D8
#define GL_TEXTURE25_ARB				0x84D9
#define GL_TEXTURE26_ARB				0x84DA
#define GL_TEXTURE27_ARB				0x84DB
#define GL_TEXTURE28_ARB				0x84DC
#define GL_TEXTURE29_ARB				0x84DD
#define GL_TEXTURE30_ARB				0x84DE
#define GL_TEXTURE31_ARB				0x84DF
#endif

// GL_EXT_compiled_vertex_array
extern int gl_supportslockarrays;
extern void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
extern void (GLAPIENTRY *qglUnlockArraysEXT) (void);

// GL_ARB_texture_env_combine
extern int gl_combine_extension;
#ifndef GL_COMBINE_ARB
#define GL_COMBINE_ARB					0x8570
#define GL_COMBINE_RGB_ARB				0x8571
#define GL_COMBINE_ALPHA_ARB			0x8572
#define GL_SOURCE0_RGB_ARB				0x8580
#define GL_SOURCE1_RGB_ARB				0x8581
#define GL_SOURCE2_RGB_ARB				0x8582
#define GL_SOURCE0_ALPHA_ARB			0x8588
#define GL_SOURCE1_ALPHA_ARB			0x8589
#define GL_SOURCE2_ALPHA_ARB			0x858A
#define GL_OPERAND0_RGB_ARB				0x8590
#define GL_OPERAND1_RGB_ARB				0x8591
#define GL_OPERAND2_RGB_ARB				0x8592
#define GL_OPERAND0_ALPHA_ARB			0x8598
#define GL_OPERAND1_ALPHA_ARB			0x8599
#define GL_OPERAND2_ALPHA_ARB			0x859A
#define GL_RGB_SCALE_ARB				0x8573
#define GL_ADD_SIGNED_ARB				0x8574
#define GL_INTERPOLATE_ARB				0x8575
#define GL_SUBTRACT_ARB					0x84E7
#define GL_CONSTANT_ARB					0x8576
#define GL_PRIMARY_COLOR_ARB			0x8577
#define GL_PREVIOUS_ARB					0x8578
#endif

extern cvar_t gl_combine;

#ifdef WIN32
#else
//#ifndef MESA
//extern void (GLAPIENTRY *glColorTableEXT)(int, int, int, int, int, const void*);
//#endif

#endif

#define DEBUGGL

#ifdef DEBUGGL
#define CHECKGLERROR if ((errornumber = glGetError())) GL_PrintError(errornumber, __FILE__, __LINE__);
extern int errornumber;
void GL_PrintError(int errornumber, char *filename, int linenumber);
#else
#define CHECKGLERROR
#endif
