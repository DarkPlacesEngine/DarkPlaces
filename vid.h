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
// vid.h -- video driver defs

#ifndef VID_H
#define VID_H

#define ENGINE_ICON ( (gamemode == GAME_NEXUIZ) ? nexuiz_xpm : darkplaces_xpm )

extern int cl_available;

#define MAX_TEXTUREUNITS 16

typedef enum renderpath_e
{
	RENDERPATH_GL11,
	RENDERPATH_GL13,
	RENDERPATH_GL20,
	RENDERPATH_CGGL,
}
renderpath_t;

typedef struct viddef_support_s
{
	qboolean amd_texture_texture4;
	qboolean arb_depth_texture;
	qboolean arb_draw_buffers;
	qboolean arb_fragment_shader;
	qboolean arb_multitexture;
	qboolean arb_occlusion_query;
	qboolean arb_shader_objects;
	qboolean arb_shading_language_100;
	qboolean arb_shadow;
	qboolean arb_texture_compression;
	qboolean arb_texture_cube_map;
	qboolean arb_texture_env_combine;
	qboolean arb_texture_gather;
	qboolean arb_texture_non_power_of_two;
	qboolean arb_texture_rectangle;
	qboolean arb_vertex_buffer_object;
	qboolean arb_vertex_shader;
	qboolean ati_separate_stencil;
	qboolean ext_blend_minmax;
	qboolean ext_blend_subtract;
	qboolean ext_compiled_vertex_array;
	qboolean ext_draw_range_elements;
	qboolean ext_framebuffer_object;
	qboolean ext_stencil_two_side;
	qboolean ext_texture_3d;
	qboolean ext_texture_compression_s3tc;
	qboolean ext_texture_edge_clamp;
	qboolean ext_texture_filter_anisotropic;
}
viddef_support_t;

typedef struct viddef_mode_s
{
	int width;
	int height;
	int bitsperpixel;
	qboolean fullscreen;
	float refreshrate;
	qboolean userefreshrate;
	qboolean stereobuffer;
	int samples;
}
viddef_mode_t;

typedef struct viddef_s
{
	// these are set by VID_Mode
	viddef_mode_t mode;
	// used in many locations in the renderer
	int width;
	int height;
	int bitsperpixel;
	qboolean fullscreen;
	float refreshrate;
	qboolean userefreshrate;
	qboolean stereobuffer;
	int samples;
	qboolean stencil;

	void *cgcontext;

	renderpath_t renderpath;

	unsigned int texunits;
	unsigned int teximageunits;
	unsigned int texarrayunits;
	unsigned int drawrangeelements_maxvertices;
	unsigned int drawrangeelements_maxindices;

	unsigned int maxtexturesize_2d;
	unsigned int maxtexturesize_3d;
	unsigned int maxtexturesize_cubemap;
	unsigned int maxtexturesize_rectangle;
	unsigned int max_anisotropy;
	unsigned int maxdrawbuffers;

	viddef_support_t support;
} viddef_t;

// global video state
extern viddef_t vid;
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);

extern qboolean vid_hidden;
extern qboolean vid_activewindow;
extern cvar_t vid_hardwaregammasupported;
extern qboolean vid_usinghwgamma;
extern qboolean vid_supportrefreshrate;

extern cvar_t vid_fullscreen;
extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_bitsperpixel;
extern cvar_t vid_samples;
extern cvar_t vid_refreshrate;
extern cvar_t vid_userefreshrate;
extern cvar_t vid_vsync;
extern cvar_t vid_mouse;
extern cvar_t vid_grabkeyboard;
extern cvar_t vid_stick_mouse;
extern cvar_t vid_resizable;
extern cvar_t vid_minwidth;
extern cvar_t vid_minheight;

extern cvar_t gl_finish;

extern cvar_t v_gamma;
extern cvar_t v_contrast;
extern cvar_t v_brightness;
extern cvar_t v_color_enable;
extern cvar_t v_color_black_r;
extern cvar_t v_color_black_g;
extern cvar_t v_color_black_b;
extern cvar_t v_color_grey_r;
extern cvar_t v_color_grey_g;
extern cvar_t v_color_grey_b;
extern cvar_t v_color_white_r;
extern cvar_t v_color_white_g;
extern cvar_t v_color_white_b;
extern cvar_t v_hwgamma;

// brand of graphics chip
extern const char *gl_vendor;
// graphics chip model and other information
extern const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
extern const char *gl_version;
// extensions list, space separated
extern const char *gl_extensions;
// WGL, GLX, or AGL
extern const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
extern const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
extern char gl_driver[256];

// compatibility hacks
extern qboolean isG200;
extern qboolean isRagePro;

void *GL_GetProcAddress(const char *name);
int GL_CheckExtension(const char *minglver_or_ext, const dllfunction_t *funcs, const char *disableparm, int silent);

void VID_Shared_Init(void);

void GL_Init (void);

void VID_CheckExtensions(void);

void VID_Init (void);
// Called at startup

void VID_Shutdown (void);
// Called at shutdown

int VID_SetMode (int modenum);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

qboolean VID_InitMode(viddef_mode_t *mode);
// allocates and opens an appropriate OpenGL context (and its window)


// sets hardware gamma correction, returns false if the device does not
// support gamma control
// (ONLY called by VID_UpdateGamma and VID_RestoreSystemGamma)
int VID_SetGamma(unsigned short *ramps, int rampsize);
// gets hardware gamma correction, returns false if the device does not
// support gamma control
// (ONLY called by VID_UpdateGamma and VID_RestoreSystemGamma)
int VID_GetGamma(unsigned short *ramps, int rampsize);
// makes sure ramp arrays are big enough and calls VID_GetGamma/VID_SetGamma
// (ONLY to be called from VID_Finish!)
void VID_UpdateGamma(qboolean force, int rampsize);
// turns off hardware gamma ramps immediately
// (called from various shutdown/deactivation functions)
void VID_RestoreSystemGamma(void);

void VID_SetMouse (qboolean fullscreengrab, qboolean relative, qboolean hidecursor);
void VID_Finish (void);

void VID_Restart_f(void);

void VID_Start(void);

extern unsigned int vid_gammatables_serial; // so other subsystems can poll if gamma parameters have changed; this starts with 0 and gets increased by 1 each time the gamma parameters get changed and VID_BuildGammaTables should be called again
extern qboolean vid_gammatables_trivial; // this is set to true if all color control values are at default setting, and it therefore would make no sense to use the gamma table
void VID_BuildGammaTables(unsigned short *ramps, int rampsize); // builds the current gamma tables into an array (needs 3*rampsize items)

typedef struct
{
	int width, height, bpp, refreshrate;
	int pixelheight_num, pixelheight_denom;
}
vid_mode_t;
size_t VID_ListModes(vid_mode_t *modes, size_t maxcount);
size_t VID_SortModes(vid_mode_t *modes, size_t count, qboolean usebpp, qboolean userefreshrate, qboolean useaspect);
#endif

