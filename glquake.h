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
extern qboolean gl_mtexable;
extern qboolean gl_supportslockarrays;

extern void GL_BeginRendering (int *x, int *y, int *width, int *height);
extern void GL_EndRendering (void);

extern	float	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern glvert_t glv;

extern	int glx, gly, glwidth, glheight;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


extern void R_TimeRefresh_f (void);

//====================================================


extern	entity_t	r_worldentity;
extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg, r_entorigin;
extern	entity_t	*currententity;
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;

extern	int	skytexturenum;		// index in cl.loadmodel, not gl texture object

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_waterripple;

extern	float	r_world_matrix[16];

extern	const char *gl_vendor;
extern	const char *gl_renderer;
extern	const char *gl_version;
extern	const char *gl_extensions;

// Multitexture
#define    TEXTURE0_SGIS				0x835E
#define    TEXTURE1_SGIS				0x835F

#ifndef _WIN32
#define APIENTRY /* */
#endif

// LordHavoc: ARB multitexure support
extern int		gl_mtex_enum;

// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

// multitexture
extern void (GLAPIENTRY *qglMTexCoord2f) (GLenum, GLfloat, GLfloat);
extern void (GLAPIENTRY *qglSelectTexture) (GLenum);
extern void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
extern void (GLAPIENTRY *qglUnlockArraysEXT) (void);


#ifndef GL_ACTIVE_TEXTURE_ARB
// multitexture
#define GL_ACTIVE_TEXTURE_ARB			0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB	0x84E1
#define GL_MAX_TEXTURES_UNITS_ARB		0x84E2
#define GL_TEXTURE0_ARB					0x84C0
#define GL_TEXTURE1_ARB					0x84C1
#define GL_TEXTURE2_ARB					0x84C2
#define GL_TEXTURE3_ARB					0x84C3
// LordHavoc: ARB supports 32+ texture units, but hey I only use 2 anyway...
#endif

#ifdef WIN32
#else
//#ifndef MESA
//extern void (GLAPIENTRY *glColorTableEXT)(int, int, int, int, int, const void*);
//#endif

#endif

// LordHavoc: vertex transform
#include "transform.h"

// LordHavoc: transparent polygon system
#include "gl_poly.h"

#define gl_solid_format 3
#define gl_alpha_format 4

//#define PARANOID

// LordHavoc: was a major time waster
#define R_CullBox(mins,maxs) (frustum[0].BoxOnPlaneSideFunc(mins, maxs, &frustum[0]) == 2 || frustum[1].BoxOnPlaneSideFunc(mins, maxs, &frustum[1]) == 2 || frustum[2].BoxOnPlaneSideFunc(mins, maxs, &frustum[2]) == 2 || frustum[3].BoxOnPlaneSideFunc(mins, maxs, &frustum[3]) == 2)
#define R_NotCulledBox(mins,maxs) (frustum[0].BoxOnPlaneSideFunc(mins, maxs, &frustum[0]) != 2 && frustum[1].BoxOnPlaneSideFunc(mins, maxs, &frustum[1]) != 2 && frustum[2].BoxOnPlaneSideFunc(mins, maxs, &frustum[2]) != 2 && frustum[3].BoxOnPlaneSideFunc(mins, maxs, &frustum[3]) != 2)

extern qboolean fogenabled;
extern vec3_t fogcolor;
extern vec_t fogdensity;
//#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_refdef.vieworg[0]) * vpn[0] + ((v)[1] - r_refdef.vieworg[1]) * vpn[1] + ((v)[2] - r_refdef.vieworg[2]) * vpn[2])*(((v)[0] - r_refdef.vieworg[0]) * vpn[0] + ((v)[1] - r_refdef.vieworg[1]) * vpn[1] + ((v)[2] - r_refdef.vieworg[2]) * vpn[2]))))
#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_refdef.vieworg[0])*((v)[0] - r_refdef.vieworg[0])+((v)[1] - r_refdef.vieworg[1])*((v)[1] - r_refdef.vieworg[1])+((v)[2] - r_refdef.vieworg[2])*((v)[2] - r_refdef.vieworg[2])))))
#define calcfogbyte(v) ((byte) (bound(0, ((int) ((float) (calcfog((v)) * 255.0f))), 255)))

#include "r_modules.h"

extern qboolean lighthalf;

#include "r_lerpanim.h"

void GL_LockArray(int first, int count);
void GL_UnlockArray();

void R_DrawAliasModel (entity_t *ent, int cull, float alpha, model_t *clmodel, frameblend_t *blend, int skin, vec3_t org, vec3_t angles, vec_t scale, int effects, int flags, int colormap);

extern cvar_t r_render;
extern cvar_t r_upload;
#include "image.h"
