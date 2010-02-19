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
// r_main.c

#include "quakedef.h"
#include "cl_dyntexture.h"
#include "r_shadow.h"
#include "polygon.h"
#include "image.h"
#include "ft2.h"

mempool_t *r_main_mempool;
rtexturepool_t *r_main_texturepool;

static int r_textureframe = 0; ///< used only by R_GetCurrentTexture

static qboolean r_loadnormalmap;
static qboolean r_loadgloss;
qboolean r_loadfog;
static qboolean r_loaddds;
static qboolean r_savedds;

//
// screen size info
//
r_refdef_t r_refdef;

cvar_t r_motionblur = {CVAR_SAVE, "r_motionblur", "0", "motionblur value scale - 0.5 recommended"};
cvar_t r_damageblur = {CVAR_SAVE, "r_damageblur", "0", "motionblur based on damage"};
cvar_t r_motionblur_vmin = {CVAR_SAVE, "r_motionblur_vmin", "300", "minimum influence from velocity"};
cvar_t r_motionblur_vmax = {CVAR_SAVE, "r_motionblur_vmax", "600", "maximum influence from velocity"};
cvar_t r_motionblur_bmin = {CVAR_SAVE, "r_motionblur_bmin", "0.5", "velocity at which there is no blur yet (may be negative to always have some blur)"};
cvar_t r_motionblur_vcoeff = {CVAR_SAVE, "r_motionblur_vcoeff", "0.05", "sliding average reaction time for velocity"};
cvar_t r_motionblur_maxblur = {CVAR_SAVE, "r_motionblur_maxblur", "0.88", "cap for motionblur alpha value"};
cvar_t r_motionblur_randomize = {CVAR_SAVE, "r_motionblur_randomize", "0.1", "randomizing coefficient to workaround ghosting"};

// TODO do we want a r_equalize_entities cvar that works on all ents, or would that be a cheat?
cvar_t r_equalize_entities_fullbright = {CVAR_SAVE, "r_equalize_entities_fullbright", "0", "render fullbright entities by equalizing their lightness, not by not rendering light"};
cvar_t r_equalize_entities_minambient = {CVAR_SAVE, "r_equalize_entities_minambient", "0.5", "light equalizing: ensure at least this ambient/diffuse ratio"};
cvar_t r_equalize_entities_by = {CVAR_SAVE, "r_equalize_entities_by", "0.7", "light equalizing: exponent of dynamics compression (0 = no compression, 1 = full compression)"};
cvar_t r_equalize_entities_to = {CVAR_SAVE, "r_equalize_entities_to", "0.8", "light equalizing: target light level"};

cvar_t r_depthfirst = {CVAR_SAVE, "r_depthfirst", "0", "renders a depth-only version of the scene before normal rendering begins to eliminate overdraw, values: 0 = off, 1 = world depth, 2 = world and model depth"};
cvar_t r_useinfinitefarclip = {CVAR_SAVE, "r_useinfinitefarclip", "1", "enables use of a special kind of projection matrix that has an extremely large farclip"};
cvar_t r_farclip_base = {0, "r_farclip_base", "65536", "farclip (furthest visible distance) for rendering when r_useinfinitefarclip is 0"};
cvar_t r_farclip_world = {0, "r_farclip_world", "2", "adds map size to farclip multiplied by this value"};
cvar_t r_nearclip = {0, "r_nearclip", "1", "distance from camera of nearclip plane" };
cvar_t r_showbboxes = {0, "r_showbboxes", "0", "shows bounding boxes of server entities, value controls opacity scaling (1 = 10%,  10 = 100%)"};
cvar_t r_showsurfaces = {0, "r_showsurfaces", "0", "1 shows surfaces as different colors, or a value of 2 shows triangle draw order (for analyzing whether meshes are optimized for vertex cache)"};
cvar_t r_showtris = {0, "r_showtris", "0", "shows triangle outlines, value controls brightness (can be above 1)"};
cvar_t r_shownormals = {0, "r_shownormals", "0", "shows per-vertex surface normals and tangent vectors for bumpmapped lighting"};
cvar_t r_showlighting = {0, "r_showlighting", "0", "shows areas lit by lights, useful for finding out why some areas of a map render slowly (bright orange = lots of passes = slow), a value of 2 disables depth testing which can be interesting but not very useful"};
cvar_t r_showshadowvolumes = {0, "r_showshadowvolumes", "0", "shows areas shadowed by lights, useful for finding out why some areas of a map render slowly (bright blue = lots of passes = slow), a value of 2 disables depth testing which can be interesting but not very useful"};
cvar_t r_showcollisionbrushes = {0, "r_showcollisionbrushes", "0", "draws collision brushes in quake3 maps (mode 1), mode 2 disables rendering of world (trippy!)"};
cvar_t r_showcollisionbrushes_polygonfactor = {0, "r_showcollisionbrushes_polygonfactor", "-1", "expands outward the brush polygons a little bit, used to make collision brushes appear infront of walls"};
cvar_t r_showcollisionbrushes_polygonoffset = {0, "r_showcollisionbrushes_polygonoffset", "0", "nudges brush polygon depth in hardware depth units, used to make collision brushes appear infront of walls"};
cvar_t r_showdisabledepthtest = {0, "r_showdisabledepthtest", "0", "disables depth testing on r_show* cvars, allowing you to see what hidden geometry the graphics card is processing"};
cvar_t r_drawportals = {0, "r_drawportals", "0", "shows portals (separating polygons) in world interior in quake1 maps"};
cvar_t r_drawentities = {0, "r_drawentities","1", "draw entities (doors, players, projectiles, etc)"};
cvar_t r_drawviewmodel = {0, "r_drawviewmodel","1", "draw your weapon model"};
cvar_t r_cullentities_trace = {0, "r_cullentities_trace", "1", "probabistically cull invisible entities"};
cvar_t r_cullentities_trace_samples = {0, "r_cullentities_trace_samples", "2", "number of samples to test for entity culling (in addition to center sample)"};
cvar_t r_cullentities_trace_tempentitysamples = {0, "r_cullentities_trace_tempentitysamples", "-1", "number of samples to test for entity culling of temp entities (including all CSQC entities), -1 disables trace culling on these entities to prevent flicker (pvs still applies)"};
cvar_t r_cullentities_trace_enlarge = {0, "r_cullentities_trace_enlarge", "0", "box enlargement for entity culling"};
cvar_t r_cullentities_trace_delay = {0, "r_cullentities_trace_delay", "1", "number of seconds until the entity gets actually culled"};
cvar_t r_speeds = {0, "r_speeds","0", "displays rendering statistics and per-subsystem timings"};
cvar_t r_fullbright = {0, "r_fullbright","0", "makes map very bright and renders faster"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1", "opacity of water polygons"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1", "enables dynamic lights (rocket glow and such)"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1", "enables glowing pixels in quake textures (changes need r_restart to take effect)"};
cvar_t r_shadows = {CVAR_SAVE, "r_shadows", "0", "casts fake stencil shadows from models onto the world (rtlights are unaffected by this); when set to 2, always cast the shadows in the direction set by r_shadows_throwdirection, otherwise use the model lighting."};
cvar_t r_shadows_darken = {CVAR_SAVE, "r_shadows_darken", "0.5", "how much shadowed areas will be darkened"};
cvar_t r_shadows_throwdistance = {CVAR_SAVE, "r_shadows_throwdistance", "500", "how far to cast shadows from models"};
cvar_t r_shadows_throwdirection = {CVAR_SAVE, "r_shadows_throwdirection", "0 0 -1", "override throwing direction for r_shadows 2"};
cvar_t r_shadows_drawafterrtlighting = {CVAR_SAVE, "r_shadows_drawafterrtlighting", "0", "draw fake shadows AFTER realtime lightning is drawn. May be useful for simulating fast sunlight on large outdoor maps with only one noshadow rtlight. The price is less realistic appearance of dynamic light shadows."};
cvar_t r_shadows_castfrombmodels = {CVAR_SAVE, "r_shadows_castfrombmodels", "0", "do cast shadows from bmodels"};
cvar_t r_shadows_focus = {CVAR_SAVE, "r_shadows_focus", "0 0 0", "offset the shadowed area focus"};
cvar_t r_shadows_shadowmapscale = {CVAR_SAVE, "r_shadows_shadowmapscale", "1", "increases shadowmap quality (multiply global shadowmap precision) for fake shadows. Needs shadowmapping ON."};
cvar_t r_q1bsp_skymasking = {0, "r_q1bsp_skymasking", "1", "allows sky polygons in quake1 maps to obscure other geometry"};
cvar_t r_polygonoffset_submodel_factor = {0, "r_polygonoffset_submodel_factor", "0", "biases depth values of world submodels such as doors, to prevent z-fighting artifacts in Quake maps"};
cvar_t r_polygonoffset_submodel_offset = {0, "r_polygonoffset_submodel_offset", "14", "biases depth values of world submodels such as doors, to prevent z-fighting artifacts in Quake maps"};
cvar_t r_polygonoffset_decals_factor = {0, "r_polygonoffset_decals_factor", "0", "biases depth values of decals to prevent z-fighting artifacts"};
cvar_t r_polygonoffset_decals_offset = {0, "r_polygonoffset_decals_offset", "-14", "biases depth values of decals to prevent z-fighting artifacts"};
cvar_t r_fog_exp2 = {0, "r_fog_exp2", "0", "uses GL_EXP2 fog (as in Nehahra) rather than realistic GL_EXP fog"};
cvar_t r_drawfog = {CVAR_SAVE, "r_drawfog", "1", "allows one to disable fog rendering"};
cvar_t r_transparentdepthmasking = {CVAR_SAVE, "r_transparentdepthmasking", "0", "enables depth writes on transparent meshes whose materially is normally opaque, this prevents seeing the inside of a transparent mesh"};

cvar_t gl_fogenable = {0, "gl_fogenable", "0", "nehahra fog enable (for Nehahra compatibility only)"};
cvar_t gl_fogdensity = {0, "gl_fogdensity", "0.25", "nehahra fog density (recommend values below 0.1) (for Nehahra compatibility only)"};
cvar_t gl_fogred = {0, "gl_fogred","0.3", "nehahra fog color red value (for Nehahra compatibility only)"};
cvar_t gl_foggreen = {0, "gl_foggreen","0.3", "nehahra fog color green value (for Nehahra compatibility only)"};
cvar_t gl_fogblue = {0, "gl_fogblue","0.3", "nehahra fog color blue value (for Nehahra compatibility only)"};
cvar_t gl_fogstart = {0, "gl_fogstart", "0", "nehahra fog start distance (for Nehahra compatibility only)"};
cvar_t gl_fogend = {0, "gl_fogend","0", "nehahra fog end distance (for Nehahra compatibility only)"};
cvar_t gl_skyclip = {0, "gl_skyclip", "4608", "nehahra farclip distance - the real fog end (for Nehahra compatibility only)"};

cvar_t r_texture_dds_load = {CVAR_SAVE, "r_texture_dds_load", "0", "load compressed dds/filename.dds texture instead of filename.tga, if the file exists (requires driver support)"};
cvar_t r_texture_dds_save = {CVAR_SAVE, "r_texture_dds_save", "0", "save compressed dds/filename.dds texture when filename.tga is loaded, so that it can be loaded instead next time"};

cvar_t r_texture_convertsRGB_2d = {0, "r_texture_convertsRGB_2d", "0", "load textures as sRGB and convert to linear for proper shading"};
cvar_t r_texture_convertsRGB_skin = {0, "r_texture_convertsRGB_skin", "0", "load textures as sRGB and convert to linear for proper shading"};
cvar_t r_texture_convertsRGB_cubemap = {0, "r_texture_convertsRGB_cubemap", "0", "load textures as sRGB and convert to linear for proper shading"};
cvar_t r_texture_convertsRGB_skybox = {0, "r_texture_convertsRGB_skybox", "0", "load textures as sRGB and convert to linear for proper shading"};
cvar_t r_texture_convertsRGB_particles = {0, "r_texture_convertsRGB_particles", "0", "load textures as sRGB and convert to linear for proper shading"};

cvar_t r_textureunits = {0, "r_textureunits", "32", "number of texture units to use in GL 1.1 and GL 1.3 rendering paths"};
static cvar_t gl_combine = {CVAR_READONLY, "gl_combine", "1", "indicates whether the OpenGL 1.3 rendering path is active"};
static cvar_t r_glsl = {CVAR_READONLY, "r_glsl", "1", "indicates whether the OpenGL 2.0 rendering path is active"};

cvar_t r_glsl_deluxemapping = {CVAR_SAVE, "r_glsl_deluxemapping", "1", "use per pixel lighting on deluxemap-compiled q3bsp maps (or a value of 2 forces deluxemap shading even without deluxemaps)"};
cvar_t r_glsl_offsetmapping = {CVAR_SAVE, "r_glsl_offsetmapping", "0", "offset mapping effect (also known as parallax mapping or virtual displacement mapping)"};
cvar_t r_glsl_offsetmapping_reliefmapping = {CVAR_SAVE, "r_glsl_offsetmapping_reliefmapping", "0", "relief mapping effect (higher quality)"};
cvar_t r_glsl_offsetmapping_scale = {CVAR_SAVE, "r_glsl_offsetmapping_scale", "0.04", "how deep the offset mapping effect is"};
cvar_t r_glsl_postprocess = {CVAR_SAVE, "r_glsl_postprocess", "0", "use a GLSL postprocessing shader"};
cvar_t r_glsl_postprocess_uservec1 = {CVAR_SAVE, "r_glsl_postprocess_uservec1", "0 0 0 0", "a 4-component vector to pass as uservec1 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec2 = {CVAR_SAVE, "r_glsl_postprocess_uservec2", "0 0 0 0", "a 4-component vector to pass as uservec2 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec3 = {CVAR_SAVE, "r_glsl_postprocess_uservec3", "0 0 0 0", "a 4-component vector to pass as uservec3 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec4 = {CVAR_SAVE, "r_glsl_postprocess_uservec4", "0 0 0 0", "a 4-component vector to pass as uservec4 to the postprocessing shader (only useful if default.glsl has been customized)"};

cvar_t r_water = {CVAR_SAVE, "r_water", "0", "whether to use reflections and refraction on water surfaces (note: r_wateralpha must be set below 1)"};
cvar_t r_water_clippingplanebias = {CVAR_SAVE, "r_water_clippingplanebias", "1", "a rather technical setting which avoids black pixels around water edges"};
cvar_t r_water_resolutionmultiplier = {CVAR_SAVE, "r_water_resolutionmultiplier", "0.5", "multiplier for screen resolution when rendering refracted/reflected scenes, 1 is full quality, lower values are faster"};
cvar_t r_water_refractdistort = {CVAR_SAVE, "r_water_refractdistort", "0.01", "how much water refractions shimmer"};
cvar_t r_water_reflectdistort = {CVAR_SAVE, "r_water_reflectdistort", "0.01", "how much water reflections shimmer"};

cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "0", "enables animation smoothing on sprites"};
cvar_t r_lerpmodels = {CVAR_SAVE, "r_lerpmodels", "1", "enables animation smoothing on models"};
cvar_t r_lerplightstyles = {CVAR_SAVE, "r_lerplightstyles", "0", "enable animation smoothing on flickering lights"};
cvar_t r_waterscroll = {CVAR_SAVE, "r_waterscroll", "1", "makes water scroll around, value controls how much"};

cvar_t r_bloom = {CVAR_SAVE, "r_bloom", "0", "enables bloom effect (makes bright pixels affect neighboring pixels)"};
cvar_t r_bloom_colorscale = {CVAR_SAVE, "r_bloom_colorscale", "1", "how bright the glow is"};
cvar_t r_bloom_brighten = {CVAR_SAVE, "r_bloom_brighten", "2", "how bright the glow is, after subtract/power"};
cvar_t r_bloom_blur = {CVAR_SAVE, "r_bloom_blur", "4", "how large the glow is"};
cvar_t r_bloom_resolution = {CVAR_SAVE, "r_bloom_resolution", "320", "what resolution to perform the bloom effect at (independent of screen resolution)"};
cvar_t r_bloom_colorexponent = {CVAR_SAVE, "r_bloom_colorexponent", "1", "how exagerated the glow is"};
cvar_t r_bloom_colorsubtract = {CVAR_SAVE, "r_bloom_colorsubtract", "0.125", "reduces bloom colors by a certain amount"};

cvar_t r_hdr = {CVAR_SAVE, "r_hdr", "0", "enables High Dynamic Range bloom effect (higher quality version of r_bloom)"};
cvar_t r_hdr_scenebrightness = {CVAR_SAVE, "r_hdr_scenebrightness", "1", "global rendering brightness"};
cvar_t r_hdr_glowintensity = {CVAR_SAVE, "r_hdr_glowintensity", "1", "how bright light emitting textures should appear"};
cvar_t r_hdr_range = {CVAR_SAVE, "r_hdr_range", "4", "how much dynamic range to render bloom with (equivilant to multiplying r_bloom_brighten by this value and dividing r_bloom_colorscale by this value)"};

cvar_t r_smoothnormals_areaweighting = {0, "r_smoothnormals_areaweighting", "1", "uses significantly faster (and supposedly higher quality) area-weighted vertex normals and tangent vectors rather than summing normalized triangle normals and tangents"};

cvar_t developer_texturelogging = {0, "developer_texturelogging", "0", "produces a textures.log file containing names of skins and map textures the engine tried to load"};

cvar_t gl_lightmaps = {0, "gl_lightmaps", "0", "draws only lightmaps, no texture (for level designers)"};

cvar_t r_test = {0, "r_test", "0", "internal development use only, leave it alone (usually does nothing anyway)"};
cvar_t r_batchmode = {0, "r_batchmode", "1", "selects method of rendering multiple surfaces with one driver call (values are 0, 1, 2, etc...)"};
cvar_t r_track_sprites = {CVAR_SAVE, "r_track_sprites", "1", "track SPR_LABEL* sprites by putting them as indicator at the screen border to rotate to"};
cvar_t r_track_sprites_flags = {CVAR_SAVE, "r_track_sprites_flags", "1", "1: Rotate sprites accodringly, 2: Make it a continuous rotation"};
cvar_t r_track_sprites_scalew = {CVAR_SAVE, "r_track_sprites_scalew", "1", "width scaling of tracked sprites"};
cvar_t r_track_sprites_scaleh = {CVAR_SAVE, "r_track_sprites_scaleh", "1", "height scaling of tracked sprites"};
cvar_t r_overheadsprites_perspective = {CVAR_SAVE, "r_overheadsprites_perspective", "0.15", "fake perspective effect for SPR_OVERHEAD sprites"};
cvar_t r_overheadsprites_pushback = {CVAR_SAVE, "r_overheadsprites_pushback", "16", "how far to pull the SPR_OVERHEAD sprites toward the eye (used to avoid intersections with 3D models)"};

cvar_t r_glsl_saturation = {CVAR_SAVE, "r_glsl_saturation", "1", "saturation multiplier (only working in glsl!)"};

cvar_t r_framedatasize = {CVAR_SAVE, "r_framedatasize", "1", "size of renderer data cache used during one frame (for skeletal animation caching, light processing, etc)"};

extern cvar_t v_glslgamma;

extern qboolean v_flipped_state;

static struct r_bloomstate_s
{
	qboolean enabled;
	qboolean hdr;

	int bloomwidth, bloomheight;

	int screentexturewidth, screentextureheight;
	rtexture_t *texture_screen; /// \note also used for motion blur if enabled!

	int bloomtexturewidth, bloomtextureheight;
	rtexture_t *texture_bloom;

	// arrays for rendering the screen passes
	float screentexcoord2f[8];
	float bloomtexcoord2f[8];
	float offsettexcoord2f[8];

	r_viewport_t viewport;
}
r_bloomstate;

r_waterstate_t r_waterstate;

/// shadow volume bsp struct with automatically growing nodes buffer
svbsp_t r_svbsp;

rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_grey128;
rtexture_t *r_texture_black;
rtexture_t *r_texture_notexture;
rtexture_t *r_texture_whitecube;
rtexture_t *r_texture_normalizationcube;
rtexture_t *r_texture_fogattenuation;
rtexture_t *r_texture_gammaramps;
unsigned int r_texture_gammaramps_serial;
//rtexture_t *r_texture_fogintensity;
rtexture_t *r_texture_reflectcube;

// TODO: hash lookups?
typedef struct cubemapinfo_s
{
	char basename[64];
	rtexture_t *texture;
}
cubemapinfo_t;

int r_texture_numcubemaps;
cubemapinfo_t r_texture_cubemaps[MAX_CUBEMAPS];

unsigned int r_queries[MAX_OCCLUSION_QUERIES];
unsigned int r_numqueries;
unsigned int r_maxqueries;

typedef struct r_qwskincache_s
{
	char name[MAX_QPATH];
	skinframe_t *skinframe;
}
r_qwskincache_t;

static r_qwskincache_t *r_qwskincache;
static int r_qwskincache_size;

/// vertex coordinates for a quad that covers the screen exactly
const float r_screenvertex3f[12] =
{
	0, 0, 0,
	1, 0, 0,
	1, 1, 0,
	0, 1, 0
};

void R_ModulateColors(float *in, float *out, int verts, float r, float g, float b)
{
	int i;
	for (i = 0;i < verts;i++)
	{
		out[0] = in[0] * r;
		out[1] = in[1] * g;
		out[2] = in[2] * b;
		out[3] = in[3];
		in += 4;
		out += 4;
	}
}

void R_FillColors(float *out, int verts, float r, float g, float b, float a)
{
	int i;
	for (i = 0;i < verts;i++)
	{
		out[0] = r;
		out[1] = g;
		out[2] = b;
		out[3] = a;
		out += 4;
	}
}

// FIXME: move this to client?
void FOG_clear(void)
{
	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_Set("gl_fogenable", "0");
		Cvar_Set("gl_fogdensity", "0.2");
		Cvar_Set("gl_fogred", "0.3");
		Cvar_Set("gl_foggreen", "0.3");
		Cvar_Set("gl_fogblue", "0.3");
	}
	r_refdef.fog_density = 0;
	r_refdef.fog_red = 0;
	r_refdef.fog_green = 0;
	r_refdef.fog_blue = 0;
	r_refdef.fog_alpha = 1;
	r_refdef.fog_start = 0;
	r_refdef.fog_end = 16384;
	r_refdef.fog_height = 1<<30;
	r_refdef.fog_fadedepth = 128;
}

static void R_BuildBlankTextures(void)
{
	unsigned char data[4];
	data[2] = 128; // normal X
	data[1] = 128; // normal Y
	data[0] = 255; // normal Z
	data[3] = 128; // height
	r_texture_blanknormalmap = R_LoadTexture2D(r_main_texturepool, "blankbump", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_texture_white = R_LoadTexture2D(r_main_texturepool, "blankwhite", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, NULL);
	data[0] = 128;
	data[1] = 128;
	data[2] = 128;
	data[3] = 255;
	r_texture_grey128 = R_LoadTexture2D(r_main_texturepool, "blankgrey128", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, NULL);
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 255;
	r_texture_black = R_LoadTexture2D(r_main_texturepool, "blankblack", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, NULL);
}

static void R_BuildNoTexture(void)
{
	int x, y;
	unsigned char pix[16][16][4];
	// this makes a light grey/dark grey checkerboard texture
	for (y = 0;y < 16;y++)
	{
		for (x = 0;x < 16;x++)
		{
			if ((y < 8) ^ (x < 8))
			{
				pix[y][x][0] = 128;
				pix[y][x][1] = 128;
				pix[y][x][2] = 128;
				pix[y][x][3] = 255;
			}
			else
			{
				pix[y][x][0] = 64;
				pix[y][x][1] = 64;
				pix[y][x][2] = 64;
				pix[y][x][3] = 255;
			}
		}
	}
	r_texture_notexture = R_LoadTexture2D(r_main_texturepool, "notexture", 16, 16, &pix[0][0][0], TEXTYPE_BGRA, TEXF_MIPMAP | TEXF_PERSISTENT, NULL);
}

static void R_BuildWhiteCube(void)
{
	unsigned char data[6*1*1*4];
	memset(data, 255, sizeof(data));
	r_texture_whitecube = R_LoadTextureCubeMap(r_main_texturepool, "whitecube", 1, data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_PERSISTENT, NULL);
}

static void R_BuildNormalizationCube(void)
{
	int x, y, side;
	vec3_t v;
	vec_t s, t, intensity;
#define NORMSIZE 64
	unsigned char *data;
	data = Mem_Alloc(tempmempool, 6*NORMSIZE*NORMSIZE*4);
	for (side = 0;side < 6;side++)
	{
		for (y = 0;y < NORMSIZE;y++)
		{
			for (x = 0;x < NORMSIZE;x++)
			{
				s = (x + 0.5f) * (2.0f / NORMSIZE) - 1.0f;
				t = (y + 0.5f) * (2.0f / NORMSIZE) - 1.0f;
				switch(side)
				{
				default:
				case 0:
					v[0] = 1;
					v[1] = -t;
					v[2] = -s;
					break;
				case 1:
					v[0] = -1;
					v[1] = -t;
					v[2] = s;
					break;
				case 2:
					v[0] = s;
					v[1] = 1;
					v[2] = t;
					break;
				case 3:
					v[0] = s;
					v[1] = -1;
					v[2] = -t;
					break;
				case 4:
					v[0] = s;
					v[1] = -t;
					v[2] = 1;
					break;
				case 5:
					v[0] = -s;
					v[1] = -t;
					v[2] = -1;
					break;
				}
				intensity = 127.0f / sqrt(DotProduct(v, v));
				data[((side*64+y)*64+x)*4+2] = (unsigned char)(128.0f + intensity * v[0]);
				data[((side*64+y)*64+x)*4+1] = (unsigned char)(128.0f + intensity * v[1]);
				data[((side*64+y)*64+x)*4+0] = (unsigned char)(128.0f + intensity * v[2]);
				data[((side*64+y)*64+x)*4+3] = 255;
			}
		}
	}
	r_texture_normalizationcube = R_LoadTextureCubeMap(r_main_texturepool, "normalcube", NORMSIZE, data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_PERSISTENT, NULL);
	Mem_Free(data);
}

static void R_BuildFogTexture(void)
{
	int x, b;
#define FOGWIDTH 256
	unsigned char data1[FOGWIDTH][4];
	//unsigned char data2[FOGWIDTH][4];
	double d, r, alpha;

	r_refdef.fogmasktable_start = r_refdef.fog_start;
	r_refdef.fogmasktable_alpha = r_refdef.fog_alpha;
	r_refdef.fogmasktable_range = r_refdef.fogrange;
	r_refdef.fogmasktable_density = r_refdef.fog_density;

	r = r_refdef.fogmasktable_range / FOGMASKTABLEWIDTH;
	for (x = 0;x < FOGMASKTABLEWIDTH;x++)
	{
		d = (x * r - r_refdef.fogmasktable_start);
		if(developer_extra.integer)
			Con_DPrintf("%f ", d);
		d = max(0, d);
		if (r_fog_exp2.integer)
			alpha = exp(-r_refdef.fogmasktable_density * r_refdef.fogmasktable_density * 0.0001 * d * d);
		else
			alpha = exp(-r_refdef.fogmasktable_density * 0.004 * d);
		if(developer_extra.integer)
			Con_DPrintf(" : %f ", alpha);
		alpha = 1 - (1 - alpha) * r_refdef.fogmasktable_alpha;
		if(developer_extra.integer)
			Con_DPrintf(" = %f\n", alpha);
		r_refdef.fogmasktable[x] = bound(0, alpha, 1);
	}

	for (x = 0;x < FOGWIDTH;x++)
	{
		b = (int)(r_refdef.fogmasktable[x * (FOGMASKTABLEWIDTH - 1) / (FOGWIDTH - 1)] * 255);
		data1[x][0] = b;
		data1[x][1] = b;
		data1[x][2] = b;
		data1[x][3] = 255;
		//data2[x][0] = 255 - b;
		//data2[x][1] = 255 - b;
		//data2[x][2] = 255 - b;
		//data2[x][3] = 255;
	}
	if (r_texture_fogattenuation)
	{
		R_UpdateTexture(r_texture_fogattenuation, &data1[0][0], 0, 0, FOGWIDTH, 1);
		//R_UpdateTexture(r_texture_fogattenuation, &data2[0][0], 0, 0, FOGWIDTH, 1);
	}
	else
	{
		r_texture_fogattenuation = R_LoadTexture2D(r_main_texturepool, "fogattenuation", FOGWIDTH, 1, &data1[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_PERSISTENT | TEXF_ALLOWUPDATES, NULL);
		//r_texture_fogintensity = R_LoadTexture2D(r_main_texturepool, "fogintensity", FOGWIDTH, 1, &data2[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_ALLOWUPDATES, NULL);
	}
}

//=======================================================================================================================================================

static const char *builtinshaderstring =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"// shadowmapping enhancements by Lee 'eihrul' Salzman\n"
"\n"
"#if defined(USEFOGINSIDE) || defined(USEFOGOUTSIDE)\n"
"# define USEFOG\n"
"#endif\n"
"#if defined(MODE_LIGHTMAP) || defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)\n"
"#define USELIGHTMAP\n"
"#endif\n"
"#if defined(USESPECULAR) || defined(USEOFFSETMAPPING) || defined(USEREFLECTCUBE)\n"
"#define USEEYEVECTOR\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(MODE_DEFERREDLIGHTSOURCE) || defined(USEDEFERREDLIGHTMAP)\n"
"# extension GL_ARB_texture_rectangle : enable\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAP2D\n"
"# ifdef GL_EXT_gpu_shader4\n"
"#   extension GL_EXT_gpu_shader4 : enable\n"
"# endif\n"
"# ifdef GL_ARB_texture_gather\n"
"#   extension GL_ARB_texture_gather : enable\n"
"# else\n"
"#   ifdef GL_AMD_texture_texture4\n"
"#     extension GL_AMD_texture_texture4 : enable\n"
"#   endif\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"# extension GL_EXT_gpu_shader4 : enable\n"
"#endif\n"
"\n"
"//#ifdef USESHADOWSAMPLER\n"
"//# extension GL_ARB_shadow : enable\n"
"//#endif\n"
"\n"
"//#ifdef __GLSL_CG_DATA_TYPES\n"
"//# define myhalf half\n"
"//# define myhalf2 half2\n"
"//# define myhalf3 half3\n"
"//# define myhalf4 half4\n"
"//#else\n"
"# define myhalf float\n"
"# define myhalf2 vec2\n"
"# define myhalf3 vec3\n"
"# define myhalf4 vec4\n"
"//#endif\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"uniform mat4 ModelViewProjectionMatrix;\n"
"#endif\n"
"\n"
"#ifdef MODE_DEPTH_OR_SHADOW\n"
"#ifdef VERTEX_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
"#endif\n"
"#else // !MODE_DEPTH_ORSHADOW\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_SHOWDEPTH\n"
"#ifdef VERTEX_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"	gl_FrontColor = vec4(gl_Position.z, gl_Position.z, gl_Position.z, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_FragColor = gl_Color;\n"
"}\n"
"#endif\n"
"#else // !MODE_SHOWDEPTH\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_POSTPROCESS\n"
"varying vec2 TexCoord1;\n"
"varying vec2 TexCoord2;\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"	TexCoord1 = gl_MultiTexCoord0.xy;\n"
"#ifdef USEBLOOM\n"
"	TexCoord2 = gl_MultiTexCoord1.xy;\n"
"#endif\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"uniform sampler2D Texture_First;\n"
"#ifdef USEBLOOM\n"
"uniform sampler2D Texture_Second;\n"
"#endif\n"
"#ifdef USEGAMMARAMPS\n"
"uniform sampler2D Texture_GammaRamps;\n"
"#endif\n"
"#ifdef USESATURATION\n"
"uniform float Saturation;\n"
"#endif\n"
"#ifdef USEVIEWTINT\n"
"uniform vec4 ViewTintColor;\n"
"#endif\n"
"//uncomment these if you want to use them:\n"
"uniform vec4 UserVec1;\n"
"// uniform vec4 UserVec2;\n"
"// uniform vec4 UserVec3;\n"
"// uniform vec4 UserVec4;\n"
"// uniform float ClientTime;\n"
"uniform vec2 PixelSize;\n"
"void main(void)\n"
"{\n"
"	gl_FragColor = texture2D(Texture_First, TexCoord1);\n"
"#ifdef USEBLOOM\n"
"	gl_FragColor += texture2D(Texture_Second, TexCoord2);\n"
"#endif\n"
"#ifdef USEVIEWTINT\n"
"	gl_FragColor = mix(gl_FragColor, ViewTintColor, ViewTintColor.a);\n"
"#endif\n"
"\n"
"#ifdef USEPOSTPROCESSING\n"
"// do r_glsl_dumpshader, edit glsl/default.glsl, and replace this by your own postprocessing if you want\n"
"// this code does a blur with the radius specified in the first component of r_glsl_postprocess_uservec1 and blends it using the second component\n"
"	gl_FragColor += texture2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*vec2(-0.987688, -0.156434)) * UserVec1.y;\n"
"	gl_FragColor += texture2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*vec2(-0.156434, -0.891007)) * UserVec1.y;\n"
"	gl_FragColor += texture2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*vec2( 0.891007, -0.453990)) * UserVec1.y;\n"
"	gl_FragColor += texture2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*vec2( 0.707107,  0.707107)) * UserVec1.y;\n"
"	gl_FragColor += texture2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*vec2(-0.453990,  0.891007)) * UserVec1.y;\n"
"	gl_FragColor /= (1 + 5 * UserVec1.y);\n"
"#endif\n"
"\n"
"#ifdef USESATURATION\n"
"	//apply saturation BEFORE gamma ramps, so v_glslgamma value does not matter\n"
"	float y = dot(gl_FragColor.rgb, vec3(0.299, 0.587, 0.114));\n"
"	//gl_FragColor = vec3(y) + (gl_FragColor.rgb - vec3(y)) * Saturation;\n"
"	gl_FragColor.rgb = mix(vec3(y), gl_FragColor.rgb, Saturation);\n"
"#endif\n"
"\n"
"#ifdef USEGAMMARAMPS\n"
"	gl_FragColor.r = texture2D(Texture_GammaRamps, vec2(gl_FragColor.r, 0)).r;\n"
"	gl_FragColor.g = texture2D(Texture_GammaRamps, vec2(gl_FragColor.g, 0)).g;\n"
"	gl_FragColor.b = texture2D(Texture_GammaRamps, vec2(gl_FragColor.b, 0)).b;\n"
"#endif\n"
"}\n"
"#endif\n"
"#else // !MODE_POSTPROCESS\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_GENERIC\n"
"#ifdef USEDIFFUSE\n"
"varying vec2 TexCoord1;\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"varying vec2 TexCoord2;\n"
"#endif\n"
"#ifdef VERTEX_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_FrontColor = gl_Color;\n"
"#ifdef USEDIFFUSE\n"
"	TexCoord1 = gl_MultiTexCoord0.xy;\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	TexCoord2 = gl_MultiTexCoord1.xy;\n"
"#endif\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"#ifdef USEDIFFUSE\n"
"uniform sampler2D Texture_First;\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"uniform sampler2D Texture_Second;\n"
"#endif\n"
"\n"
"void main(void)\n"
"{\n"
"	gl_FragColor = gl_Color;\n"
"#ifdef USEDIFFUSE\n"
"	gl_FragColor *= texture2D(Texture_First, TexCoord1);\n"
"#endif\n"
"\n"
"#ifdef USESPECULAR\n"
"	vec4 tex2 = texture2D(Texture_Second, TexCoord2);\n"
"# ifdef USECOLORMAPPING\n"
"	gl_FragColor *= tex2;\n"
"# endif\n"
"# ifdef USEGLOW\n"
"	gl_FragColor += tex2;\n"
"# endif\n"
"# ifdef USEVERTEXTEXTUREBLEND\n"
"	gl_FragColor = mix(gl_FragColor, tex2, tex2.a);\n"
"# endif\n"
"#endif\n"
"}\n"
"#endif\n"
"#else // !MODE_GENERIC\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_BLOOMBLUR\n"
"varying TexCoord;\n"
"#ifdef VERTEX_SHADER\n"
"void main(void)\n"
"{\n"
"	gl_FrontColor = gl_Color;\n"
"	TexCoord = gl_MultiTexCoord0.xy;\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"uniform sampler2D Texture_First;\n"
"uniform vec4 BloomBlur_Parameters;\n"
"\n"
"void main(void)\n"
"{\n"
"	int i;\n"
"	vec2 tc = TexCoord;\n"
"	vec3 color = texture2D(Texture_First, tc).rgb;\n"
"	tc += BloomBlur_Parameters.xy;\n"
"	for (i = 1;i < SAMPLES;i++)\n"
"	{\n"
"		color += texture2D(Texture_First, tc).rgb;\n"
"		tc += BloomBlur_Parameters.xy;\n"
"	}\n"
"	gl_FragColor = vec4(color * BloomBlur_Parameters.z + vec3(BloomBlur_Parameters.w), 1);\n"
"}\n"
"#endif\n"
"#else // !MODE_BLOOMBLUR\n"
"#ifdef MODE_REFRACTION\n"
"varying vec2 TexCoord;\n"
"varying vec4 ModelViewProjectionPosition;\n"
"uniform mat4 TexMatrix;\n"
"#ifdef VERTEX_SHADER\n"
"\n"
"void main(void)\n"
"{\n"
"	TexCoord = vec2(TexMatrix * gl_MultiTexCoord0);\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Refraction;\n"
"uniform sampler2D Texture_Reflection;\n"
"\n"
"uniform vec4 DistortScaleRefractReflect;\n"
"uniform vec4 ScreenScaleRefractReflect;\n"
"uniform vec4 ScreenCenterRefractReflect;\n"
"uniform vec4 RefractColor;\n"
"uniform vec4 ReflectColor;\n"
"uniform float ReflectFactor;\n"
"uniform float ReflectOffset;\n"
"\n"
"void main(void)\n"
"{\n"
"	vec2 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect.xy * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec2 ScreenTexCoord = (ModelViewProjectionPosition.xy + normalize(vec3(texture2D(Texture_Normal, TexCoord)) - vec3(0.5)).xy * DistortScaleRefractReflect.xy * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
"	vec2 SafeScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
"	vec2 ScreenTexCoord = SafeScreenTexCoord + vec2(normalize(vec3(texture2D(Texture_Normal, TexCoord)) - vec3(0.5))).xy * DistortScaleRefractReflect.xy;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord + vec2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord + vec2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord + vec2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord + vec2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord = mix(SafeScreenTexCoord, ScreenTexCoord, f);\n"
"	gl_FragColor = texture2D(Texture_Refraction, ScreenTexCoord) * RefractColor;\n"
"}\n"
"#endif\n"
"#else // !MODE_REFRACTION\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_WATER\n"
"varying vec2 TexCoord;\n"
"varying vec3 EyeVector;\n"
"varying vec4 ModelViewProjectionPosition;\n"
"#ifdef VERTEX_SHADER\n"
"uniform vec3 EyePosition;\n"
"uniform mat4 TexMatrix;\n"
"\n"
"void main(void)\n"
"{\n"
"	TexCoord = vec2(TexMatrix * gl_MultiTexCoord0);\n"
"	vec3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Refraction;\n"
"uniform sampler2D Texture_Reflection;\n"
"\n"
"uniform vec4 DistortScaleRefractReflect;\n"
"uniform vec4 ScreenScaleRefractReflect;\n"
"uniform vec4 ScreenCenterRefractReflect;\n"
"uniform vec4 RefractColor;\n"
"uniform vec4 ReflectColor;\n"
"uniform float ReflectFactor;\n"
"uniform float ReflectOffset;\n"
"\n"
"void main(void)\n"
"{\n"
"	vec4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(vec3(texture2D(Texture_Normal, TexCoord)) - vec3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	vec4 SafeScreenTexCoord = ModelViewProjectionPosition.xyxy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	//SafeScreenTexCoord = gl_FragCoord.xyxy * vec4(1.0 / 1920.0, 1.0 / 1200.0, 1.0 / 1920.0, 1.0 / 1200.0);\n"
"	vec4 ScreenTexCoord = SafeScreenTexCoord + vec2(normalize(vec3(texture2D(Texture_Normal, TexCoord)) - vec3(0.5))).xyxy * DistortScaleRefractReflect;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord.xy + vec2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord.xy + vec2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord.xy + vec2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Refraction, ScreenTexCoord.xy + vec2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord.xy = mix(SafeScreenTexCoord.xy, ScreenTexCoord.xy, f);\n"
"	f       = min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord.zw + vec2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord.zw + vec2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord.zw + vec2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord.zw + vec2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord.zw = mix(SafeScreenTexCoord.zw, ScreenTexCoord.zw, f);\n"
"	float Fresnel = pow(min(1.0, 1.0 - float(normalize(EyeVector).z)), 2.0) * ReflectFactor + ReflectOffset;\n"
"	gl_FragColor = mix(texture2D(Texture_Refraction, ScreenTexCoord.xy) * RefractColor, texture2D(Texture_Reflection, ScreenTexCoord.zw) * ReflectColor, Fresnel);\n"
"}\n"
"#endif\n"
"#else // !MODE_WATER\n"
"\n"
"\n"
"\n"
"\n"
"// common definitions between vertex shader and fragment shader:\n"
"\n"
"varying vec2 TexCoord;\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"varying vec2 TexCoord2;\n"
"#endif\n"
"#ifdef USELIGHTMAP\n"
"varying vec2 TexCoordLightmap;\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"varying vec3 CubeVector;\n"
"#endif\n"
"\n"
"#if (defined(MODE_LIGHTSOURCE) || defined(MODE_LIGHTDIRECTION)) && defined(USEDIFFUSE)\n"
"varying vec3 LightVector;\n"
"#endif\n"
"\n"
"#ifdef USEEYEVECTOR\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"#ifdef USEFOG\n"
"varying vec4 EyeVectorModelSpaceFogPlaneVertexDist;\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_DEFERREDGEOMETRY) || defined(USEREFLECTCUBE)\n"
"varying vec3 VectorS; // direction of S texcoord (sometimes crudely called tangent)\n"
"varying vec3 VectorT; // direction of T texcoord (sometimes crudely called binormal)\n"
"varying vec3 VectorR; // direction of R texcoord (surface normal)\n"
"#endif\n"
"\n"
"#ifdef USEREFLECTION\n"
"varying vec4 ModelViewProjectionPosition;\n"
"#endif\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"uniform vec3 LightPosition;\n"
"varying vec4 ModelViewPosition;\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform vec3 LightPosition;\n"
"#endif\n"
"uniform vec3 EyePosition;\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"uniform vec3 LightDir;\n"
"#endif\n"
"uniform vec4 FogPlane;\n"
"\n"
"#ifdef USESHADOWMAPORTHO\n"
"varying vec3 ShadowMapTC;\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"// TODO: get rid of tangentt (texcoord2) and use a crossproduct to regenerate it from tangents (texcoord1) and normal (texcoord3), this would require sending a 4 component texcoord1 with W as 1 or -1 according to which side the texcoord2 should be on\n"
"\n"
"// fragment shader specific:\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Color;\n"
"uniform sampler2D Texture_Gloss;\n"
"#ifdef USEGLOW\n"
"uniform sampler2D Texture_Glow;\n"
"#endif\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform sampler2D Texture_SecondaryNormal;\n"
"uniform sampler2D Texture_SecondaryColor;\n"
"uniform sampler2D Texture_SecondaryGloss;\n"
"#ifdef USEGLOW\n"
"uniform sampler2D Texture_SecondaryGlow;\n"
"#endif\n"
"#endif\n"
"#ifdef USECOLORMAPPING\n"
"uniform sampler2D Texture_Pants;\n"
"uniform sampler2D Texture_Shirt;\n"
"#endif\n"
"#ifdef USEFOG\n"
"uniform sampler2D Texture_FogMask;\n"
"#endif\n"
"#ifdef USELIGHTMAP\n"
"uniform sampler2D Texture_Lightmap;\n"
"#endif\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)\n"
"uniform sampler2D Texture_Deluxemap;\n"
"#endif\n"
"#ifdef USEREFLECTION\n"
"uniform sampler2D Texture_Reflection;\n"
"#endif\n"
"\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"uniform sampler2D Texture_ScreenDepth;\n"
"uniform sampler2D Texture_ScreenNormalMap;\n"
"#endif\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"uniform sampler2D Texture_ScreenDiffuse;\n"
"uniform sampler2D Texture_ScreenSpecular;\n"
"#endif\n"
"\n"
"uniform myhalf3 Color_Pants;\n"
"uniform myhalf3 Color_Shirt;\n"
"uniform myhalf3 FogColor;\n"
"\n"
"#ifdef USEFOG\n"
"uniform float FogRangeRecip;\n"
"uniform float FogPlaneViewDist;\n"
"uniform float FogHeightFade;\n"
"float FogVertex(void)\n"
"{\n"
"	vec3 EyeVectorModelSpace = EyeVectorModelSpaceFogPlaneVertexDist.xyz;\n"
"	float FogPlaneVertexDist = EyeVectorModelSpaceFogPlaneVertexDist.w;\n"
"	float fogfrac;\n"
"#ifdef USEFOGOUTSIDE\n"
"	fogfrac = min(0.0, FogPlaneVertexDist) / (FogPlaneVertexDist - FogPlaneViewDist) * min(1.0, min(0.0, FogPlaneVertexDist) * FogHeightFade);\n"
"#else\n"
"	fogfrac = FogPlaneViewDist / (FogPlaneViewDist - max(0.0, FogPlaneVertexDist)) * min(1.0, (min(0.0, FogPlaneVertexDist) + FogPlaneViewDist) * FogHeightFade);\n"
"#endif\n"
"	return float(texture2D(Texture_FogMask, myhalf2(length(EyeVectorModelSpace)*fogfrac*FogRangeRecip, 0.0)));\n"
"}\n"
"#endif\n"
"\n"
"#ifdef USEOFFSETMAPPING\n"
"uniform float OffsetMapping_Scale;\n"
"vec2 OffsetMapping(vec2 TexCoord)\n"
"{\n"
"#ifdef USEOFFSETMAPPING_RELIEFMAPPING\n"
"	// 14 sample relief mapping: linear search and then binary search\n"
"	// this basically steps forward a small amount repeatedly until it finds\n"
"	// itself inside solid, then jitters forward and back using decreasing\n"
"	// amounts to find the impact\n"
"	//vec3 OffsetVector = vec3(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMapping_Scale) * vec2(-1, 1), -1);\n"
"	//vec3 OffsetVector = vec3(normalize(EyeVector.xy) * OffsetMapping_Scale * vec2(-1, 1), -1);\n"
"	vec3 OffsetVector = vec3(normalize(EyeVector).xy * OffsetMapping_Scale * vec2(-1, 1), -1);\n"
"	vec3 RT = vec3(TexCoord, 1);\n"
"	OffsetVector *= 0.1;\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(texture2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector * (step(texture2D(Texture_Normal, RT.xy).a, RT.z)          - 0.5);\n"
"	RT += OffsetVector * (step(texture2D(Texture_Normal, RT.xy).a, RT.z) * 0.5    - 0.25);\n"
"	RT += OffsetVector * (step(texture2D(Texture_Normal, RT.xy).a, RT.z) * 0.25   - 0.125);\n"
"	RT += OffsetVector * (step(texture2D(Texture_Normal, RT.xy).a, RT.z) * 0.125  - 0.0625);\n"
"	RT += OffsetVector * (step(texture2D(Texture_Normal, RT.xy).a, RT.z) * 0.0625 - 0.03125);\n"
"	return RT.xy;\n"
"#else\n"
"	// 3 sample offset mapping (only 3 samples because of ATI Radeon 9500-9800/X300 limits)\n"
"	// this basically moves forward the full distance, and then backs up based\n"
"	// on height of samples\n"
"	//vec2 OffsetVector = vec2(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMapping_Scale) * vec2(-1, 1));\n"
"	//vec2 OffsetVector = vec2(normalize(EyeVector.xy) * OffsetMapping_Scale * vec2(-1, 1));\n"
"	vec2 OffsetVector = vec2(normalize(EyeVector).xy * OffsetMapping_Scale * vec2(-1, 1));\n"
"	TexCoord += OffsetVector;\n"
"	OffsetVector *= 0.333;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"	return TexCoord;\n"
"#endif\n"
"}\n"
"#endif // USEOFFSETMAPPING\n"
"\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_DEFERREDLIGHTSOURCE)\n"
"uniform sampler2D Texture_Attenuation;\n"
"uniform samplerCube Texture_Cube;\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_DEFERREDLIGHTSOURCE) || defined(USESHADOWMAPORTHO)\n"
"\n"
"#ifdef USESHADOWMAPRECT\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform sampler2DRectShadow Texture_ShadowMapRect;\n"
"# else\n"
"uniform sampler2DRect Texture_ShadowMapRect;\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAP2D\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform sampler2DShadow Texture_ShadowMap2D;\n"
"# else\n"
"uniform sampler2D Texture_ShadowMap2D;\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPVSDCT\n"
"uniform samplerCube Texture_CubeProjection;\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform samplerCubeShadow Texture_ShadowMapCube;\n"
"# else\n"
"uniform samplerCube Texture_ShadowMapCube;\n"
"# endif\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D) || defined(USESHADOWMAPCUBE)\n"
"uniform vec2 ShadowMap_TextureScale;\n"
"uniform vec4 ShadowMap_Parameters;\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D)\n"
"# ifdef USESHADOWMAPORTHO\n"
"#  define GetShadowMapTC2D(dir) (min(dir, ShadowMap_Parameters.xyz))\n"
"# else\n"
"#  ifdef USESHADOWMAPVSDCT\n"
"vec3 GetShadowMapTC2D(vec3 dir)\n"
"{\n"
"	vec3 adir = abs(dir);\n"
"	vec2 aparams = ShadowMap_Parameters.xy / max(max(adir.x, adir.y), adir.z);\n"
"	vec4 proj = textureCube(Texture_CubeProjection, dir);\n"
"	return vec3(mix(dir.xy, dir.zz, proj.xy) * aparams.x + proj.zw * ShadowMap_Parameters.z, aparams.y + ShadowMap_Parameters.w);\n"
"}\n"
"#  else\n"
"vec3 GetShadowMapTC2D(vec3 dir)\n"
"{\n"
"	vec3 adir = abs(dir);\n"
"	float ma = adir.z;\n"
"	vec4 proj = vec4(dir, 2.5);\n"
"	if (adir.x > ma) { ma = adir.x; proj = vec4(dir.zyx, 0.5); }\n"
"	if (adir.y > ma) { ma = adir.y; proj = vec4(dir.xzy, 1.5); }\n"
"	vec2 aparams = ShadowMap_Parameters.xy / ma;\n"
"	return vec3(proj.xy * aparams.x + vec2(proj.z < 0.0 ? 1.5 : 0.5, proj.w) * ShadowMap_Parameters.z, aparams.y + ShadowMap_Parameters.w);\n"
"}\n"
"#  endif\n"
"# endif\n"
"#endif // defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D)\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"vec4 GetShadowMapTCCube(vec3 dir)\n"
"{\n"
"	vec3 adir = abs(dir);\n"
"	return vec4(dir, ShadowMap_Parameters.z + ShadowMap_Parameters.w / max(max(adir.x, adir.y), adir.z));\n"
"}\n"
"#endif\n"
"\n"
"# ifdef USESHADOWMAPRECT\n"
"float ShadowMapCompare(vec3 dir)\n"
"{\n"
"	vec3 shadowmaptc = GetShadowMapTC2D(dir);\n"
"	float f;\n"
"#  ifdef USESHADOWSAMPLER\n"
"\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      define texval(x, y) shadow2DRect(Texture_ShadowMapRect, shadowmaptc + vec3(x, y, 0.0)).r\n"
"	f = dot(vec4(0.25), vec4(texval(-0.4, 1.0), texval(-1.0, -0.4), texval(0.4, -1.0), texval(1.0, 0.4)));\n"
"#    else\n"
"	f = shadow2DRect(Texture_ShadowMapRect, shadowmaptc).r;\n"
"#    endif\n"
"\n"
"#  else\n"
"\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      if USESHADOWMAPPCF > 1\n"
"#        define texval(x, y) texture2DRect(Texture_ShadowMapRect, center + vec2(x, y)).r\n"
"	vec2 center = shadowmaptc.xy - 0.5, offset = fract(center);\n"
"	vec4 row1 = step(shadowmaptc.z, vec4(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0), texval( 2.0, -1.0)));\n"
"	vec4 row2 = step(shadowmaptc.z, vec4(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0), texval( 2.0,  0.0)));\n"
"	vec4 row3 = step(shadowmaptc.z, vec4(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0), texval( 2.0,  1.0)));\n"
"	vec4 row4 = step(shadowmaptc.z, vec4(texval(-1.0,  2.0), texval( 0.0,  2.0), texval( 1.0,  2.0), texval( 2.0,  2.0)));\n"
"	vec4 cols = row2 + row3 + mix(row1, row4, offset.y);\n"
"	f = dot(mix(cols.xyz, cols.yzw, offset.x), vec3(1.0/9.0));\n"
"#      else\n"
"#        define texval(x, y) texture2DRect(Texture_ShadowMapRect, shadowmaptc.xy + vec2(x, y)).r\n"
"	vec2 offset = fract(shadowmaptc.xy);\n"
"	vec3 row1 = step(shadowmaptc.z, vec3(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0)));\n"
"	vec3 row2 = step(shadowmaptc.z, vec3(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0)));\n"
"	vec3 row3 = step(shadowmaptc.z, vec3(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0)));\n"
"	vec3 cols = row2 + mix(row1, row3, offset.y);\n"
"	f = dot(mix(cols.xy, cols.yz, offset.x), vec2(0.25));\n"
"#      endif\n"
"#    else\n"
"	f = step(shadowmaptc.z, texture2DRect(Texture_ShadowMapRect, shadowmaptc.xy).r);\n"
"#    endif\n"
"\n"
"#  endif\n"
"#  ifdef USESHADOWMAPORTHO\n"
"	return mix(ShadowMap_Parameters.w, 1.0, f);\n"
"#  else\n"
"	return f;\n"
"#  endif\n"
"}\n"
"# endif\n"
"\n"
"# ifdef USESHADOWMAP2D\n"
"float ShadowMapCompare(vec3 dir)\n"
"{\n"
"	vec3 shadowmaptc = GetShadowMapTC2D(dir);\n"
"	float f;\n"
"\n"
"#  ifdef USESHADOWSAMPLER\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      define texval(x, y) shadow2D(Texture_ShadowMap2D, vec3(center + vec2(x, y)*ShadowMap_TextureScale, shadowmaptc.z)).r  \n"
"	vec2 center = shadowmaptc.xy*ShadowMap_TextureScale;\n"
"	f = dot(vec4(0.25), vec4(texval(-0.4, 1.0), texval(-1.0, -0.4), texval(0.4, -1.0), texval(1.0, 0.4)));\n"
"#    else\n"
"	f = shadow2D(Texture_ShadowMap2D, vec3(shadowmaptc.xy*ShadowMap_TextureScale, shadowmaptc.z)).r;\n"
"#    endif\n"
"#  else\n"
"#    ifdef USESHADOWMAPPCF\n"
"#     if defined(GL_ARB_texture_gather) || defined(GL_AMD_texture_texture4)\n"
"#      ifdef GL_ARB_texture_gather\n"
"#        define texval(x, y) textureGatherOffset(Texture_ShadowMap2D, center, ivec2(x, y))\n"
"#      else\n"
"#        define texval(x, y) texture4(Texture_ShadowMap2D, center + vec2(x, y)*ShadowMap_TextureScale)\n"
"#      endif\n"
"	vec2 center = shadowmaptc.xy - 0.5, offset = fract(center);\n"
"	center *= ShadowMap_TextureScale;\n"
"	vec4 group1 = step(shadowmaptc.z, texval(-1.0, -1.0));\n"
"	vec4 group2 = step(shadowmaptc.z, texval( 1.0, -1.0));\n"
"	vec4 group3 = step(shadowmaptc.z, texval(-1.0,  1.0));\n"
"	vec4 group4 = step(shadowmaptc.z, texval( 1.0,  1.0));\n"
"	vec4 cols = vec4(group1.rg, group2.rg) + vec4(group3.ab, group4.ab) +\n"
"				mix(vec4(group1.ab, group2.ab), vec4(group3.rg, group4.rg), offset.y);\n"
"	f = dot(mix(cols.xyz, cols.yzw, offset.x), vec3(1.0/9.0));\n"
"#     else\n"
"#      ifdef GL_EXT_gpu_shader4\n"
"#        define texval(x, y) texture2DOffset(Texture_ShadowMap2D, center, ivec2(x, y)).r\n"
"#      else\n"
"#        define texval(x, y) texture2D(Texture_ShadowMap2D, center + vec2(x, y)*ShadowMap_TextureScale).r  \n"
"#      endif\n"
"#      if USESHADOWMAPPCF > 1\n"
"	vec2 center = shadowmaptc.xy - 0.5, offset = fract(center);\n"
"	center *= ShadowMap_TextureScale;\n"
"	vec4 row1 = step(shadowmaptc.z, vec4(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0), texval( 2.0, -1.0)));\n"
"	vec4 row2 = step(shadowmaptc.z, vec4(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0), texval( 2.0,  0.0)));\n"
"	vec4 row3 = step(shadowmaptc.z, vec4(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0), texval( 2.0,  1.0)));\n"
"	vec4 row4 = step(shadowmaptc.z, vec4(texval(-1.0,  2.0), texval( 0.0,  2.0), texval( 1.0,  2.0), texval( 2.0,  2.0)));\n"
"	vec4 cols = row2 + row3 + mix(row1, row4, offset.y);\n"
"	f = dot(mix(cols.xyz, cols.yzw, offset.x), vec3(1.0/9.0));\n"
"#      else\n"
"	vec2 center = shadowmaptc.xy*ShadowMap_TextureScale, offset = fract(shadowmaptc.xy);\n"
"	vec3 row1 = step(shadowmaptc.z, vec3(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0)));\n"
"	vec3 row2 = step(shadowmaptc.z, vec3(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0)));\n"
"	vec3 row3 = step(shadowmaptc.z, vec3(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0)));\n"
"	vec3 cols = row2 + mix(row1, row3, offset.y);\n"
"	f = dot(mix(cols.xy, cols.yz, offset.x), vec2(0.25));\n"
"#      endif\n"
"#     endif\n"
"#    else\n"
"	f = step(shadowmaptc.z, texture2D(Texture_ShadowMap2D, shadowmaptc.xy*ShadowMap_TextureScale).r);\n"
"#    endif\n"
"#  endif\n"
"#  ifdef USESHADOWMAPORTHO\n"
"	return mix(ShadowMap_Parameters.w, 1.0, f);\n"
"#  else\n"
"	return f;\n"
"#  endif\n"
"}\n"
"# endif\n"
"\n"
"# ifdef USESHADOWMAPCUBE\n"
"float ShadowMapCompare(vec3 dir)\n"
"{\n"
"	// apply depth texture cubemap as light filter\n"
"	vec4 shadowmaptc = GetShadowMapTCCube(dir);\n"
"	float f;\n"
"#  ifdef USESHADOWSAMPLER\n"
"	f = shadowCube(Texture_ShadowMapCube, shadowmaptc).r;\n"
"#  else\n"
"	f = step(shadowmaptc.w, textureCube(Texture_ShadowMapCube, shadowmaptc.xyz).r);\n"
"#  endif\n"
"	return f;\n"
"}\n"
"# endif\n"
"#endif // !defined(MODE_LIGHTSOURCE) && !defined(MODE_DEFERREDLIGHTSOURCE) && !defined(USESHADOWMAPORTHO)\n"
"#endif // FRAGMENT_SHADER\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_DEFERREDGEOMETRY\n"
"#ifdef VERTEX_SHADER\n"
"uniform mat4 TexMatrix;\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform mat4 BackgroundTexMatrix;\n"
"#endif\n"
"uniform mat4 ModelViewMatrix;\n"
"void main(void)\n"
"{\n"
"	TexCoord = vec2(TexMatrix * gl_MultiTexCoord0);\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	gl_FrontColor = gl_Color;\n"
"	TexCoord2 = vec2(BackgroundTexMatrix * gl_MultiTexCoord0);\n"
"#endif\n"
"\n"
"	// transform unnormalized eye direction into tangent space\n"
"#ifdef USEOFFSETMAPPING\n"
"	vec3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	VectorS = (ModelViewMatrix * vec4(gl_MultiTexCoord1.xyz, 0)).xyz;\n"
"	VectorT = (ModelViewMatrix * vec4(gl_MultiTexCoord2.xyz, 0)).xyz;\n"
"	VectorR = (ModelViewMatrix * vec4(gl_MultiTexCoord3.xyz, 0)).xyz;\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main(void)\n"
"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"#ifdef USEALPHAKILL\n"
"	if (texture2D(Texture_Color, TexCoord).a < 0.5)\n"
"		discard;\n"
"#endif\n"
"\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	float alpha = texture2D(Texture_Color, TexCoord).a;\n"
"	float terrainblend = clamp(float(gl_Color.a) * alpha * 2.0 - 0.5, float(0.0), float(1.0));\n"
"	//float terrainblend = min(float(gl_Color.a) * alpha * 2.0, float(1.0));\n"
"	//float terrainblend = float(gl_Color.a) * alpha > 0.5;\n"
"#endif\n"
"\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	vec3 surfacenormal = mix(vec3(texture2D(Texture_SecondaryNormal, TexCoord2)), vec3(texture2D(Texture_Normal, TexCoord)), terrainblend) - vec3(0.5, 0.5, 0.5);\n"
"	float a = mix(texture2D(Texture_SecondaryGloss, TexCoord2).a, texture2D(Texture_Gloss, TexCoord).a, terrainblend);\n"
"#else\n"
"	vec3 surfacenormal = vec3(texture2D(Texture_Normal, TexCoord)) - vec3(0.5, 0.5, 0.5);\n"
"	float a = texture2D(Texture_Gloss, TexCoord).a;\n"
"#endif\n"
"\n"
"	gl_FragColor = vec4(normalize(surfacenormal.x * VectorS + surfacenormal.y * VectorT + surfacenormal.z * VectorR) * 0.5 + vec3(0.5, 0.5, 0.5), a);\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"#else // !MODE_DEFERREDGEOMETRY\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"#ifdef VERTEX_SHADER\n"
"uniform mat4 ModelViewMatrix;\n"
"void main(void)\n"
"{\n"
"	ModelViewPosition = ModelViewMatrix * gl_Vertex;\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"uniform mat4 ViewToLight;\n"
"// ScreenToDepth = vec2(Far / (Far - Near), Far * Near / (Near - Far));\n"
"uniform vec2 ScreenToDepth;\n"
"uniform myhalf3 DeferredColor_Ambient;\n"
"uniform myhalf3 DeferredColor_Diffuse;\n"
"#ifdef USESPECULAR\n"
"uniform myhalf3 DeferredColor_Specular;\n"
"uniform myhalf SpecularPower;\n"
"#endif\n"
"uniform myhalf2 PixelToScreenTexCoord;\n"
"void main(void)\n"
"{\n"
"	// calculate viewspace pixel position\n"
"	vec2 ScreenTexCoord = gl_FragCoord.xy * PixelToScreenTexCoord;\n"
"	vec3 position;\n"
"	position.z = ScreenToDepth.y / (texture2D(Texture_ScreenDepth, ScreenTexCoord).r + ScreenToDepth.x);\n"
"	position.xy = ModelViewPosition.xy * (position.z / ModelViewPosition.z);\n"
"	// decode viewspace pixel normal\n"
"	myhalf4 normalmap = texture2D(Texture_ScreenNormalMap, ScreenTexCoord);\n"
"	myhalf3 surfacenormal = normalize(normalmap.rgb - myhalf3(0.5,0.5,0.5));\n"
"	// surfacenormal = pixel normal in viewspace\n"
"	// LightVector = pixel to light in viewspace\n"
"	// CubeVector = position in lightspace\n"
"	// eyevector = pixel to view in viewspace\n"
"	vec3 CubeVector = vec3(ViewToLight * vec4(position,1));\n"
"	myhalf fade = myhalf(texture2D(Texture_Attenuation, vec2(length(CubeVector), 0.0)));\n"
"#ifdef USEDIFFUSE\n"
"	// calculate diffuse shading\n"
"	myhalf3 lightnormal = myhalf3(normalize(LightPosition - position));\n"
"	myhalf diffuse = myhalf(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	// calculate directional shading\n"
"	vec3 eyevector = position * -1.0;\n"
"#  ifdef USEEXACTSPECULARMATH\n"
"	myhalf specular = pow(myhalf(max(float(dot(reflect(lightnormal, surfacenormal), normalize(eyevector)))*-1.0, 0.0)), SpecularPower * normalmap.a);\n"
"#  else\n"
"	myhalf3 specularnormal = normalize(lightnormal + myhalf3(normalize(eyevector)));\n"
"	myhalf specular = pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * normalmap.a);\n"
"#  endif\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAPCUBE) || defined(USESHADOWMAP2D)\n"
"	fade *= ShadowMapCompare(CubeVector);\n"
"#endif\n"
"\n"
"#ifdef USEDIFFUSE\n"
"	gl_FragData[0] = vec4((DeferredColor_Ambient + DeferredColor_Diffuse * diffuse) * fade, 1.0);\n"
"#else\n"
"	gl_FragData[0] = vec4(DeferredColor_Ambient * fade, 1.0);\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	gl_FragData[1] = vec4(DeferredColor_Specular * (specular * fade), 1.0);\n"
"#else\n"
"	gl_FragData[1] = vec4(0.0, 0.0, 0.0, 1.0);\n"
"#endif\n"
"\n"
"# ifdef USECUBEFILTER\n"
"	vec3 cubecolor = textureCube(Texture_Cube, CubeVector).rgb;\n"
"	gl_FragData[0].rgb *= cubecolor;\n"
"	gl_FragData[1].rgb *= cubecolor;\n"
"# endif\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"#else // !MODE_DEFERREDLIGHTSOURCE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"uniform mat4 TexMatrix;\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform mat4 BackgroundTexMatrix;\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform mat4 ModelToLight;\n"
"#endif\n"
"#ifdef USESHADOWMAPORTHO\n"
"uniform mat4 ShadowMapMatrix;\n"
"#endif\n"
"void main(void)\n"
"{\n"
"#if defined(MODE_VERTEXCOLOR) || defined(USEVERTEXTEXTUREBLEND)\n"
"	gl_FrontColor = gl_Color;\n"
"#endif\n"
"	// copy the surface texcoord\n"
"	TexCoord = vec2(TexMatrix * gl_MultiTexCoord0);\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	TexCoord2 = vec2(BackgroundTexMatrix * gl_MultiTexCoord0);\n"
"#endif\n"
"#ifdef USELIGHTMAP\n"
"	TexCoordLightmap = vec2(gl_MultiTexCoord4);\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// transform vertex position into light attenuation/cubemap space\n"
"	// (-1 to +1 across the light box)\n"
"	CubeVector = vec3(ModelToLight * gl_Vertex);\n"
"\n"
"# ifdef USEDIFFUSE\n"
"	// transform unnormalized light direction into tangent space\n"
"	// (we use unnormalized to ensure that it interpolates correctly and then\n"
"	//  normalize it per pixel)\n"
"	vec3 lightminusvertex = LightPosition - gl_Vertex.xyz;\n"
"	LightVector.x = dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n"
"# endif\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTDIRECTION) && defined(USEDIFFUSE)\n"
"	LightVector.x = dot(LightDir, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(LightDir, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(LightDir, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	// transform unnormalized eye direction into tangent space\n"
"#ifdef USEEYEVECTOR\n"
"	vec3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"#ifdef USEFOG\n"
"	EyeVectorModelSpaceFogPlaneVertexDist.xyz = EyePosition - gl_Vertex.xyz;\n"
"	EyeVectorModelSpaceFogPlaneVertexDist.w = dot(FogPlane, gl_Vertex);\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(USEREFLECTCUBE)\n"
"	VectorS = gl_MultiTexCoord1.xyz;\n"
"	VectorT = gl_MultiTexCoord2.xyz;\n"
"	VectorR = gl_MultiTexCoord3.xyz;\n"
"#endif\n"
"\n"
"	// transform vertex to camera space, using ftransform to match non-VS rendering\n"
"	gl_Position = ModelViewProjectionMatrix * gl_Vertex;\n"
"\n"
"#ifdef USESHADOWMAPORTHO\n"
"	ShadowMapTC = vec3(ShadowMapMatrix * gl_Position);\n"
"#endif\n"
"\n"
"#ifdef USEREFLECTION\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"#endif\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"uniform myhalf2 PixelToScreenTexCoord;\n"
"uniform myhalf3 DeferredMod_Diffuse;\n"
"uniform myhalf3 DeferredMod_Specular;\n"
"#endif\n"
"uniform myhalf3 Color_Ambient;\n"
"uniform myhalf3 Color_Diffuse;\n"
"uniform myhalf3 Color_Specular;\n"
"uniform myhalf SpecularPower;\n"
"#ifdef USEGLOW\n"
"uniform myhalf3 Color_Glow;\n"
"#endif\n"
"uniform myhalf Alpha;\n"
"#ifdef USEREFLECTION\n"
"uniform vec4 DistortScaleRefractReflect;\n"
"uniform vec4 ScreenScaleRefractReflect;\n"
"uniform vec4 ScreenCenterRefractReflect;\n"
"uniform myhalf4 ReflectColor;\n"
"#endif\n"
"#ifdef USEREFLECTCUBE\n"
"uniform mat4 ModelToReflectCube;\n"
"uniform sampler2D Texture_ReflectMask;\n"
"uniform samplerCube Texture_ReflectCube;\n"
"#endif\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"uniform myhalf3 LightColor;\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform myhalf3 LightColor;\n"
"#endif\n"
"void main(void)\n"
"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	// combine the diffuse textures (base, pants, shirt)\n"
"	myhalf4 color = myhalf4(texture2D(Texture_Color, TexCoord));\n"
"#ifdef USEALPHAKILL\n"
"	if (color.a < 0.5)\n"
"		discard;\n"
"#endif\n"
"	color.a *= Alpha;\n"
"#ifdef USECOLORMAPPING\n"
"	color.rgb += myhalf3(texture2D(Texture_Pants, TexCoord)) * Color_Pants + myhalf3(texture2D(Texture_Shirt, TexCoord)) * Color_Shirt;\n"
"#endif\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	myhalf terrainblend = clamp(myhalf(gl_Color.a) * color.a * 2.0 - 0.5, myhalf(0.0), myhalf(1.0));\n"
"	//myhalf terrainblend = min(myhalf(gl_Color.a) * color.a * 2.0, myhalf(1.0));\n"
"	//myhalf terrainblend = myhalf(gl_Color.a) * color.a > 0.5;\n"
"	color.rgb = mix(myhalf3(texture2D(Texture_SecondaryColor, TexCoord2)), color.rgb, terrainblend);\n"
"	color.a = 1.0;\n"
"	//color = mix(myhalf4(1, 0, 0, 1), color, terrainblend);\n"
"#endif\n"
"\n"
"	// get the surface normal\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	myhalf3 surfacenormal = normalize(mix(myhalf3(texture2D(Texture_SecondaryNormal, TexCoord2)), myhalf3(texture2D(Texture_Normal, TexCoord)), terrainblend) - myhalf3(0.5, 0.5, 0.5));\n"
"#else\n"
"	myhalf3 surfacenormal = normalize(myhalf3(texture2D(Texture_Normal, TexCoord)) - myhalf3(0.5, 0.5, 0.5));\n"
"#endif\n"
"\n"
"	// get the material colors\n"
"	myhalf3 diffusetex = color.rgb;\n"
"#if defined(USESPECULAR) || defined(USEDEFERREDLIGHTMAP)\n"
"# ifdef USEVERTEXTEXTUREBLEND\n"
"	myhalf4 glosstex = mix(myhalf4(texture2D(Texture_SecondaryGloss, TexCoord2)), myhalf4(texture2D(Texture_Gloss, TexCoord)), terrainblend);\n"
"# else\n"
"	myhalf4 glosstex = myhalf4(texture2D(Texture_Gloss, TexCoord));\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USEREFLECTCUBE\n"
"	vec3 TangentReflectVector = reflect(-EyeVector, surfacenormal);\n"
"	vec3 ModelReflectVector = TangentReflectVector.x * VectorS + TangentReflectVector.y * VectorT + TangentReflectVector.z * VectorR;\n"
"	vec3 ReflectCubeTexCoord = vec3(ModelToReflectCube * vec4(ModelReflectVector, 0));\n"
"	diffusetex += myhalf3(texture2D(Texture_ReflectMask, TexCoord)) * myhalf3(textureCube(Texture_ReflectCube, ReflectCubeTexCoord));\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// light source\n"
"#ifdef USEDIFFUSE\n"
"	myhalf3 lightnormal = myhalf3(normalize(LightVector));\n"
"	myhalf diffuse = myhalf(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"	color.rgb = diffusetex * (Color_Ambient + diffuse * Color_Diffuse);\n"
"#ifdef USESPECULAR\n"
"#ifdef USEEXACTSPECULARMATH\n"
"	myhalf specular = pow(myhalf(max(float(dot(reflect(lightnormal, surfacenormal), normalize(EyeVector)))*-1.0, 0.0)), SpecularPower * glosstex.a);\n"
"#else\n"
"	myhalf3 specularnormal = normalize(lightnormal + myhalf3(normalize(EyeVector)));\n"
"	myhalf specular = pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * glosstex.a);\n"
"#endif\n"
"	color.rgb += glosstex.rgb * (specular * Color_Specular);\n"
"#endif\n"
"#else\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"#endif\n"
"	color.rgb *= LightColor;\n"
"	color.rgb *= myhalf(texture2D(Texture_Attenuation, vec2(length(CubeVector), 0.0)));\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAPCUBE) || defined(USESHADOWMAP2D)\n"
"	color.rgb *= ShadowMapCompare(CubeVector);\n"
"#endif\n"
"# ifdef USECUBEFILTER\n"
"	color.rgb *= myhalf3(textureCube(Texture_Cube, CubeVector));\n"
"# endif\n"
"#endif // MODE_LIGHTSOURCE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"#define SHADING\n"
"#ifdef USEDIFFUSE\n"
"	myhalf3 lightnormal = myhalf3(normalize(LightVector));\n"
"#endif\n"
"#define lightcolor LightColor\n"
"#endif // MODE_LIGHTDIRECTION\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"#define SHADING\n"
"	// deluxemap lightmapping using light vectors in modelspace (q3map2 -light -deluxe)\n"
"	myhalf3 lightnormal_modelspace = myhalf3(texture2D(Texture_Deluxemap, TexCoordLightmap)) * 2.0 + myhalf3(-1.0, -1.0, -1.0);\n"
"	myhalf3 lightcolor = myhalf3(texture2D(Texture_Lightmap, TexCoordLightmap));\n"
"	// convert modelspace light vector to tangentspace\n"
"	myhalf3 lightnormal;\n"
"	lightnormal.x = dot(lightnormal_modelspace, myhalf3(VectorS));\n"
"	lightnormal.y = dot(lightnormal_modelspace, myhalf3(VectorT));\n"
"	lightnormal.z = dot(lightnormal_modelspace, myhalf3(VectorR));\n"
"	// calculate directional shading (and undoing the existing angle attenuation on the lightmap by the division)\n"
"	// note that q3map2 is too stupid to calculate proper surface normals when q3map_nonplanar\n"
"	// is used (the lightmap and deluxemap coords correspond to virtually random coordinates\n"
"	// on that luxel, and NOT to its center, because recursive triangle subdivision is used\n"
"	// to map the luxels to coordinates on the draw surfaces), which also causes\n"
"	// deluxemaps to be wrong because light contributions from the wrong side of the surface\n"
"	// are added up. To prevent divisions by zero or strong exaggerations, a max()\n"
"	// nudge is done here at expense of some additional fps. This is ONLY needed for\n"
"	// deluxemaps, tangentspace deluxemap avoid this problem by design.\n"
"	lightcolor *= 1.0 / max(0.25, lightnormal.z);\n"
"#endif // MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n"
"#define SHADING\n"
"	// deluxemap lightmapping using light vectors in tangentspace (hmap2 -light)\n"
"	myhalf3 lightnormal = myhalf3(texture2D(Texture_Deluxemap, TexCoordLightmap)) * 2.0 + myhalf3(-1.0, -1.0, -1.0);\n"
"	myhalf3 lightcolor = myhalf3(texture2D(Texture_Lightmap, TexCoordLightmap));\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTMAP\n"
"	color.rgb = diffusetex * (Color_Ambient + myhalf3(texture2D(Texture_Lightmap, TexCoordLightmap)) * Color_Diffuse);\n"
"#endif // MODE_LIGHTMAP\n"
"#ifdef MODE_VERTEXCOLOR\n"
"	color.rgb = diffusetex * (Color_Ambient + myhalf3(gl_Color.rgb) * Color_Diffuse);\n"
"#endif // MODE_VERTEXCOLOR\n"
"#ifdef MODE_FLATCOLOR\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"#endif // MODE_FLATCOLOR\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef SHADING\n"
"# ifdef USEDIFFUSE\n"
"	myhalf diffuse = myhalf(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"#  ifdef USESPECULAR\n"
"#   ifdef USEEXACTSPECULARMATH\n"
"	myhalf specular = pow(myhalf(max(float(dot(reflect(lightnormal, surfacenormal), normalize(EyeVector)))*-1.0, 0.0)), SpecularPower * glosstex.a);\n"
"#   else\n"
"	myhalf3 specularnormal = normalize(lightnormal + myhalf3(normalize(EyeVector)));\n"
"	myhalf specular = pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * glosstex.a);\n"
"#   endif\n"
"	color.rgb = diffusetex * Color_Ambient + (diffusetex * Color_Diffuse * diffuse + glosstex.rgb * Color_Specular * specular) * lightcolor;\n"
"#  else\n"
"	color.rgb = diffusetex * (Color_Ambient + Color_Diffuse * diffuse * lightcolor);\n"
"#  endif\n"
"# else\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPORTHO\n"
"	color.rgb *= ShadowMapCompare(ShadowMapTC);\n"
"#endif\n"
"\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"	vec2 ScreenTexCoord = gl_FragCoord.xy * PixelToScreenTexCoord;\n"
"	color.rgb += diffusetex * myhalf3(texture2D(Texture_ScreenDiffuse, ScreenTexCoord)) * DeferredMod_Diffuse;\n"
"	color.rgb += glosstex.rgb * myhalf3(texture2D(Texture_ScreenSpecular, ScreenTexCoord)) * DeferredMod_Specular;\n"
"#endif\n"
"\n"
"#ifdef USEGLOW\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	color.rgb += mix(myhalf3(texture2D(Texture_SecondaryGlow, TexCoord2)), myhalf3(texture2D(Texture_Glow, TexCoord)), terrainblend) * Color_Glow;\n"
"#else\n"
"	color.rgb += myhalf3(texture2D(Texture_Glow, TexCoord)) * Color_Glow;\n"
"#endif\n"
"#endif\n"
"\n"
"#ifdef USEFOG\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	color.rgb *= myhalf(FogVertex());\n"
"#else\n"
"	color.rgb = mix(FogColor, color.rgb, FogVertex());\n"
"#endif\n"
"#endif\n"
"\n"
"	// reflection must come last because it already contains exactly the correct fog (the reflection render preserves camera distance from the plane, it only flips the side) and ContrastBoost/SceneBrightness\n"
"#ifdef USEREFLECTION\n"
"	vec4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(myhalf3(texture2D(Texture_Normal, TexCoord)) - myhalf3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	vec2 SafeScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW.zw + ScreenCenterRefractReflect.zw;\n"
"	vec2 ScreenTexCoord = SafeScreenTexCoord + vec3(normalize(myhalf3(texture2D(Texture_Normal, TexCoord)) - myhalf3(0.5))).xy * DistortScaleRefractReflect.zw;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord + vec2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord + vec2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord + vec2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(texture2D(Texture_Reflection, ScreenTexCoord + vec2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord = mix(SafeScreenTexCoord, ScreenTexCoord, f);\n"
"	color.rgb = mix(color.rgb, myhalf3(texture2D(Texture_Reflection, ScreenTexCoord)) * ReflectColor.rgb, ReflectColor.a);\n"
"#endif\n"
"\n"
"	gl_FragColor = vec4(color);\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"\n"
"#endif // !MODE_DEFERREDLIGHTSOURCE\n"
"#endif // !MODE_DEFERREDGEOMETRY\n"
"#endif // !MODE_WATER\n"
"#endif // !MODE_REFRACTION\n"
"#endif // !MODE_BLOOMBLUR\n"
"#endif // !MODE_GENERIC\n"
"#endif // !MODE_POSTPROCESS\n"
"#endif // !MODE_SHOWDEPTH\n"
"#endif // !MODE_DEPTH_OR_SHADOW\n"
;

/*
=========================================================================================================================================================



=========================================================================================================================================================



=========================================================================================================================================================



=========================================================================================================================================================



=========================================================================================================================================================



=========================================================================================================================================================



=========================================================================================================================================================
*/

const char *builtincgshaderstring =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"// shadowmapping enhancements by Lee 'eihrul' Salzman\n"
"\n"
"#if defined(USEFOGINSIDE) || defined(USEFOGOUTSIDE)\n"
"# define USEFOG\n"
"#endif\n"
"#if defined(MODE_LIGHTMAP) || defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)\n"
"#define USELIGHTMAP\n"
"#endif\n"
"#if defined(USESPECULAR) || defined(USEOFFSETMAPPING) || defined(USEREFLECTCUBE)\n"
"#define USEEYEVECTOR\n"
"#endif\n"
"\n"
"#ifdef MODE_DEPTH_OR_SHADOW\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"out float4 gl_Position : POSITION\n"
")\n"
"{\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"}\n"
"#endif\n"
"#else // !MODE_DEPTH_ORSHADOW\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_SHOWDEPTH\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"out float4 gl_Position : POSITION,\n"
"out float4 gl_FrontColor : COLOR0\n"
")\n"
"{\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"	gl_FrontColor = float4(gl_Position.z, gl_Position.z, gl_Position.z, 1.0);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float4 gl_FrontColor : COLOR0,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	gl_FragColor = gl_FrontColor;\n"
"}\n"
"#endif\n"
"#else // !MODE_SHOWDEPTH\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_POSTPROCESS\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"float4 gl_MultiTexCoord1 : TEXCOORD1,\n"
"out float4 gl_Position : POSITION,\n"
"out float2 TexCoord1 : TEXCOORD0,\n"
"out float2 TexCoord2 : TEXCOORD1\n"
")\n"
"{\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"	TexCoord1 = gl_MultiTexCoord0.xy;\n"
"#ifdef USEBLOOM\n"
"	TexCoord2 = gl_MultiTexCoord1.xy;\n"
"#endif\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float2 TexCoord1 : TEXCOORD0,\n"
"float2 TexCoord2 : TEXCOORD1,\n"
"uniform sampler2D Texture_First,\n"
"#ifdef USEBLOOM\n"
"uniform sampler2D Texture_Second,\n"
"#endif\n"
"#ifdef USEGAMMARAMPS\n"
"uniform sampler2D Texture_GammaRamps,\n"
"#endif\n"
"#ifdef USESATURATION\n"
"uniform float Saturation,\n"
"#endif\n"
"#ifdef USEVIEWTINT\n"
"uniform float4 ViewTintColor,\n"
"#endif\n"
"uniform float4 UserVec1,\n"
"uniform float4 UserVec2,\n"
"uniform float4 UserVec3,\n"
"uniform float4 UserVec4,\n"
"uniform float ClientTime,\n"
"uniform float2 PixelSize,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	gl_FragColor = tex2D(Texture_First, TexCoord1);\n"
"#ifdef USEBLOOM\n"
"	gl_FragColor += tex2D(Texture_Second, TexCoord2);\n"
"#endif\n"
"#ifdef USEVIEWTINT\n"
"	gl_FragColor = lerp(gl_FragColor, ViewTintColor, ViewTintColor.a);\n"
"#endif\n"
"\n"
"#ifdef USEPOSTPROCESSING\n"
"// do r_glsl_dumpshader, edit glsl/default.glsl, and replace this by your own postprocessing if you want\n"
"// this code does a blur with the radius specified in the first component of r_glsl_postprocess_uservec1 and blends it using the second component\n"
"	gl_FragColor += tex2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*float2(-0.987688, -0.156434)) * UserVec1.y;\n"
"	gl_FragColor += tex2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*float2(-0.156434, -0.891007)) * UserVec1.y;\n"
"	gl_FragColor += tex2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*float2( 0.891007, -0.453990)) * UserVec1.y;\n"
"	gl_FragColor += tex2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*float2( 0.707107,  0.707107)) * UserVec1.y;\n"
"	gl_FragColor += tex2D(Texture_First, TexCoord1 + PixelSize*UserVec1.x*float2(-0.453990,  0.891007)) * UserVec1.y;\n"
"	gl_FragColor /= (1 + 5 * UserVec1.y);\n"
"#endif\n"
"\n"
"#ifdef USESATURATION\n"
"	//apply saturation BEFORE gamma ramps, so v_glslgamma value does not matter\n"
"	float y = dot(gl_FragColor.rgb, float3(0.299, 0.587, 0.114));\n"
"	//gl_FragColor = float3(y) + (gl_FragColor.rgb - float3(y)) * Saturation;\n"
"	gl_FragColor.rgb = lerp(float3(y), gl_FragColor.rgb, Saturation);\n"
"#endif\n"
"\n"
"#ifdef USEGAMMARAMPS\n"
"	gl_FragColor.r = tex2D(Texture_GammaRamps, float2(gl_FragColor.r, 0)).r;\n"
"	gl_FragColor.g = tex2D(Texture_GammaRamps, float2(gl_FragColor.g, 0)).g;\n"
"	gl_FragColor.b = tex2D(Texture_GammaRamps, float2(gl_FragColor.b, 0)).b;\n"
"#endif\n"
"}\n"
"#endif\n"
"#else // !MODE_POSTPROCESS\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_GENERIC\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"float4 gl_Color : COLOR0,\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"float4 gl_MultiTexCoord1 : TEXCOORD1,\n"
"out float4 gl_Position : POSITION,\n"
"out float4 gl_FrontColor : COLOR,\n"
"out float2 TexCoord1 : TEXCOORD0,\n"
"out float2 TexCoord2 : TEXCOORD1\n"
")\n"
"{\n"
"	gl_FrontColor = gl_Color;\n"
"#ifdef USEDIFFUSE\n"
"	TexCoord1 = gl_MultiTexCoord0.xy;\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	TexCoord2 = gl_MultiTexCoord1.xy;\n"
"#endif\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"void main\n"
"(\n"
"float4 gl_FrontColor : COLOR,\n"
"float2 TexCoord1 : TEXCOORD0,\n"
"float2 TexCoord2 : TEXCOORD1,\n"
"#ifdef USEDIFFUSE\n"
"uniform sampler2D Texture_First,\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"uniform sampler2D Texture_Second,\n"
"#endif\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	gl_FragColor = gl_FrontColor;\n"
"#ifdef USEDIFFUSE\n"
"	gl_FragColor *= tex2D(Texture_First, TexCoord1);\n"
"#endif\n"
"\n"
"#ifdef USESPECULAR\n"
"	float4 tex2 = tex2D(Texture_Second, TexCoord2);\n"
"# ifdef USECOLORMAPPING\n"
"	gl_FragColor *= tex2;\n"
"# endif\n"
"# ifdef USEGLOW\n"
"	gl_FragColor += tex2;\n"
"# endif\n"
"# ifdef USEVERTEXTEXTUREBLEND\n"
"	gl_FragColor = lerp(gl_FragColor, tex2, tex2.a);\n"
"# endif\n"
"#endif\n"
"}\n"
"#endif\n"
"#else // !MODE_GENERIC\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_BLOOMBLUR\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"out float4 gl_Position : POSITION,\n"
"out float2 TexCoord : TEXCOORD0\n"
")\n"
"{\n"
"	TexCoord = gl_MultiTexCoord0.xy;\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"void main\n"
"(\n"
"float2 TexCoord : TEXCOORD0,\n"
"uniform sampler2D Texture_First,\n"
"uniform float4 BloomBlur_Parameters,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	int i;\n"
"	float2 tc = TexCoord;\n"
"	float3 color = tex2D(Texture_First, tc).rgb;\n"
"	tc += BloomBlur_Parameters.xy;\n"
"	for (i = 1;i < SAMPLES;i++)\n"
"	{\n"
"		color += tex2D(Texture_First, tc).rgb;\n"
"		tc += BloomBlur_Parameters.xy;\n"
"	}\n"
"	gl_FragColor = float4(color * BloomBlur_Parameters.z + float3(BloomBlur_Parameters.w), 1);\n"
"}\n"
"#endif\n"
"#else // !MODE_BLOOMBLUR\n"
"#ifdef MODE_REFRACTION\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"uniform float4x4 TexMatrix,\n"
"uniform float3 EyePosition,\n"
"out float4 gl_Position : POSITION,\n"
"out float2 TexCoord : TEXCOORD0,\n"
"out float3 EyeVector : TEXCOORD1,\n"
"out float4 ModelViewProjectionPosition : TEXCOORD2\n"
")\n"
"{\n"
"	TexCoord = float2(mul(TexMatrix, gl_MultiTexCoord0));\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float2 TexCoord : TEXCOORD0,\n"
"float3 EyeVector : TEXCOORD1,\n"
"float4 ModelViewProjectionPosition : TEXCOORD2,\n"
"uniform sampler2D Texture_Normal,\n"
"uniform sampler2D Texture_Refraction,\n"
"uniform sampler2D Texture_Reflection,\n"
"uniform float4 DistortScaleRefractReflect,\n"
"uniform float4 ScreenScaleRefractReflect,\n"
"uniform float4 ScreenCenterRefractReflect,\n"
"uniform float4 RefractColor,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	float2 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect.xy * (1.0 / ModelViewProjectionPosition.w);\n"
"	//float2 ScreenTexCoord = (ModelViewProjectionPosition.xy + normalize(float3(tex2D(Texture_Normal, TexCoord)) - float3(0.5)).xy * DistortScaleRefractReflect.xy * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
"	float2 SafeScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
"	float2 ScreenTexCoord = SafeScreenTexCoord + float2(normalize(float3(tex2D(Texture_Normal, TexCoord)) - float3(0.5))).xy * DistortScaleRefractReflect.xy;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord + float2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord + float2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord + float2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord + float2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord = lerp(SafeScreenTexCoord, ScreenTexCoord, f);\n"
"	gl_FragColor = tex2D(Texture_Refraction, ScreenTexCoord) * RefractColor;\n"
"}\n"
"#endif\n"
"#else // !MODE_REFRACTION\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_WATER\n"
"#ifdef VERTEX_SHADER\n"
"\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"uniform float4x4 TexMatrix,\n"
"uniform float3 EyePosition,\n"
"out float4 gl_Position : POSITION,\n"
"out float2 TexCoord : TEXCOORD0,\n"
"out float3 EyeVector : TEXCOORD1,\n"
"out float4 ModelViewProjectionPosition : TEXCOORD2\n"
")\n"
"{\n"
"	TexCoord = float2(mul(TexMatrix, gl_MultiTexCoord0));\n"
"	float3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float2 TexCoord : TEXCOORD0,\n"
"float3 EyeVector : TEXCOORD1,\n"
"float4 ModelViewProjectionPosition : TEXCOORD2,\n"
"uniform sampler2D Texture_Normal,\n"
"uniform sampler2D Texture_Refraction,\n"
"uniform sampler2D Texture_Reflection,\n"
"uniform float4 DistortScaleRefractReflect,\n"
"uniform float4 ScreenScaleRefractReflect,\n"
"uniform float4 ScreenCenterRefractReflect,\n"
"uniform float4 RefractColor,\n"
"uniform float4 ReflectColor,\n"
"uniform float ReflectFactor,\n"
"uniform float ReflectOffset,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	float4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//float4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(float3(tex2D(Texture_Normal, TexCoord)) - float3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	float4 SafeScreenTexCoord = ModelViewProjectionPosition.xyxy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	float4 ScreenTexCoord = SafeScreenTexCoord + float2(normalize(float3(tex2D(Texture_Normal, TexCoord)) - float3(0.5))).xyxy * DistortScaleRefractReflect;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord.xy + float2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord.xy + float2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord.xy + float2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Refraction, ScreenTexCoord.xy + float2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord.xy = lerp(SafeScreenTexCoord.xy, ScreenTexCoord.xy, f);\n"
"	f       = min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord.zw + float2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord.zw + float2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord.zw + float2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord.zw + float2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord.zw = lerp(SafeScreenTexCoord.zw, ScreenTexCoord.zw, f);\n"
"	float Fresnel = pow(min(1.0, 1.0 - float(normalize(EyeVector).z)), 2.0) * ReflectFactor + ReflectOffset;\n"
"	gl_FragColor = lerp(tex2D(Texture_Refraction, ScreenTexCoord.xy) * RefractColor, tex2D(Texture_Reflection, ScreenTexCoord.zw) * ReflectColor, Fresnel);\n"
"}\n"
"#endif\n"
"#else // !MODE_WATER\n"
"\n"
"\n"
"\n"
"\n"
"// TODO: get rid of tangentt (texcoord2) and use a crossproduct to regenerate it from tangents (texcoord1) and normal (texcoord3), this would require sending a 4 component texcoord1 with W as 1 or -1 according to which side the texcoord2 should be on\n"
"\n"
"// fragment shader specific:\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"#ifdef USEFOG\n"
"float FogVertex(float3 EyeVectorModelSpace, float FogPlaneVertexDist, float FogRangeRecip, float FogPlaneViewDist, float FogHeightFade, sampler2D Texture_FogMask)\n"
"{\n"
"	float fogfrac;\n"
"#ifdef USEFOGOUTSIDE\n"
"	fogfrac = min(0.0, FogPlaneVertexDist) / (FogPlaneVertexDist - FogPlaneViewDist) * min(1.0, min(0.0, FogPlaneVertexDist) * FogHeightFade);\n"
"#else\n"
"	fogfrac = FogPlaneViewDist / (FogPlaneViewDist - max(0.0, FogPlaneVertexDist)) * min(1.0, (min(0.0, FogPlaneVertexDist) + FogPlaneViewDist) * FogHeightFade);\n"
"#endif\n"
"	return float(tex2D(Texture_FogMask, half2(length(EyeVectorModelSpace)*fogfrac*FogRangeRecip, 0.0)));\n"
"}\n"
"#endif\n"
"\n"
"#ifdef USEOFFSETMAPPING\n"
"float2 OffsetMapping(float2 TexCoord, float OffsetMapping_Scale, float3 EyeVector, sampler2D Texture_Normal)\n"
"{\n"
"#ifdef USEOFFSETMAPPING_RELIEFMAPPING\n"
"	// 14 sample relief mapping: linear search and then binary search\n"
"	// this basically steps forward a small amount repeatedly until it finds\n"
"	// itself inside solid, then jitters forward and back using decreasing\n"
"	// amounts to find the impact\n"
"	//float3 OffsetVector = float3(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMapping_Scale) * float2(-1, 1), -1);\n"
"	//float3 OffsetVector = float3(normalize(EyeVector.xy) * OffsetMapping_Scale * float2(-1, 1), -1);\n"
"	float3 OffsetVector = float3(normalize(EyeVector).xy * OffsetMapping_Scale * float2(-1, 1), -1);\n"
"	float3 RT = float3(TexCoord, 1);\n"
"	OffsetVector *= 0.1;\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector *  step(tex2D(Texture_Normal, RT.xy).a, RT.z);\n"
"	RT += OffsetVector * (step(tex2D(Texture_Normal, RT.xy).a, RT.z)          - 0.5);\n"
"	RT += OffsetVector * (step(tex2D(Texture_Normal, RT.xy).a, RT.z) * 0.5    - 0.25);\n"
"	RT += OffsetVector * (step(tex2D(Texture_Normal, RT.xy).a, RT.z) * 0.25   - 0.125);\n"
"	RT += OffsetVector * (step(tex2D(Texture_Normal, RT.xy).a, RT.z) * 0.125  - 0.0625);\n"
"	RT += OffsetVector * (step(tex2D(Texture_Normal, RT.xy).a, RT.z) * 0.0625 - 0.03125);\n"
"	return RT.xy;\n"
"#else\n"
"	// 3 sample offset mapping (only 3 samples because of ATI Radeon 9500-9800/X300 limits)\n"
"	// this basically moves forward the full distance, and then backs up based\n"
"	// on height of samples\n"
"	//float2 OffsetVector = float2(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMapping_Scale) * float2(-1, 1));\n"
"	//float2 OffsetVector = float2(normalize(EyeVector.xy) * OffsetMapping_Scale * float2(-1, 1));\n"
"	float2 OffsetVector = float2(normalize(EyeVector).xy * OffsetMapping_Scale * float2(-1, 1));\n"
"	TexCoord += OffsetVector;\n"
"	OffsetVector *= 0.333;\n"
"	TexCoord -= OffsetVector * tex2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * tex2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * tex2D(Texture_Normal, TexCoord).a;\n"
"	return TexCoord;\n"
"#endif\n"
"}\n"
"#endif // USEOFFSETMAPPING\n"
"\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_DEFERREDLIGHTSOURCE) || defined(USESHADOWMAPORTHO)\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D)\n"
"# ifdef USESHADOWMAPORTHO\n"
"#  define GetShadowMapTC2D(dir, ShadowMap_Parameters) (min(dir, ShadowMap_Parameters.xyz))\n"
"# else\n"
"#  ifdef USESHADOWMAPVSDCT\n"
"float3 GetShadowMapTC2D(float3 dir, float4 ShadowMap_Parameters, samplerCUBE Texture_CubeProjection)\n"
"{\n"
"	float3 adir = abs(dir);\n"
"	float2 aparams = ShadowMap_Parameters.xy / max(max(adir.x, adir.y), adir.z);\n"
"	float4 proj = texCUBEe(Texture_CubeProjection, dir);\n"
"	return float3(mix(dir.xy, dir.zz, proj.xy) * aparams.x + proj.zw * ShadowMap_Parameters.z, aparams.y + ShadowMap_Parameters.w);\n"
"}\n"
"#  else\n"
"float3 GetShadowMapTC2D(float3 dir, float4 ShadowMap_Parameters)\n"
"{\n"
"	float3 adir = abs(dir);\n"
"	float ma = adir.z;\n"
"	float4 proj = float4(dir, 2.5);\n"
"	if (adir.x > ma) { ma = adir.x; proj = float4(dir.zyx, 0.5); }\n"
"	if (adir.y > ma) { ma = adir.y; proj = float4(dir.xzy, 1.5); }\n"
"	float2 aparams = ShadowMap_Parameters.xy / ma;\n"
"	return float3(proj.xy * aparams.x + float2(proj.z < 0.0 ? 1.5 : 0.5, proj.w) * ShadowMap_Parameters.z, aparams.y + ShadowMap_Parameters.w);\n"
"}\n"
"#  endif\n"
"# endif\n"
"#endif // defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D) || defined(USESHADOWMAPORTHO)\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"float4 GetShadowMapTCCube(float3 dir, float4 ShadowMap_Parameters)\n"
"{\n"
"    float3 adir = abs(dir);\n"
"    return float4(dir, ShadowMap_Parameters.z + ShadowMap_Parameters.w / max(max(adir.x, adir.y), adir.z));\n"
"}\n"
"#endif\n"
"\n"
"# ifdef USESHADOWMAPRECT\n"
"#ifdef USESHADOWMAPVSDCT\n"
"float ShadowMapCompare(float3 dir, samplerRECT Texture_ShadowMapRect, float4 ShadowMap_Parameters, samplerCUBE Texture_CubeProjection)\n"
"#else\n"
"float ShadowMapCompare(float3 dir, samplerRECT Texture_ShadowMapRect, float4 ShadowMap_Parameters)\n"
"#endif\n"
"{\n"
"#ifdef USESHADOWMAPVSDCT\n"
"	float3 shadowmaptc = GetShadowMapTC2D(dir, ShadowMap_Parameters, Texture_CubeProjection);\n"
"#else\n"
"	float3 shadowmaptc = GetShadowMapTC2D(dir, ShadowMap_Parameters);\n"
"#endif\n"
"	float f;\n"
"#  ifdef USESHADOWSAMPLER\n"
"\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      define texval(x, y) shadow2DRect(Texture_ShadowMapRect, shadowmaptc + float3(x, y, 0.0)).r\n"
"    f = dot(float4(0.25), float4(texval(-0.4, 1.0), texval(-1.0, -0.4), texval(0.4, -1.0), texval(1.0, 0.4)));\n"
"#    else\n"
"    f = shadow2DRect(Texture_ShadowMapRect, shadowmaptc).r;\n"
"#    endif\n"
"\n"
"#  else\n"
"\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      if USESHADOWMAPPCF > 1\n"
"#        define texval(x, y) texRECT(Texture_ShadowMapRect, center + float2(x, y)).r\n"
"    float2 center = shadowmaptc.xy - 0.5, offset = frac(center);\n"
"    float4 row1 = step(shadowmaptc.z, float4(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0), texval( 2.0, -1.0)));\n"
"    float4 row2 = step(shadowmaptc.z, float4(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0), texval( 2.0,  0.0)));\n"
"    float4 row3 = step(shadowmaptc.z, float4(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0), texval( 2.0,  1.0)));\n"
"    float4 row4 = step(shadowmaptc.z, float4(texval(-1.0,  2.0), texval( 0.0,  2.0), texval( 1.0,  2.0), texval( 2.0,  2.0)));\n"
"    float4 cols = row2 + row3 + lerp(row1, row4, offset.y);\n"
"    f = dot(lerp(cols.xyz, cols.yzw, offset.x), float3(1.0/9.0));\n"
"#      else\n"
"#        define texval(x, y) texRECT(Texture_ShadowMapRect, shadowmaptc.xy + float2(x, y)).r\n"
"    float2 offset = frac(shadowmaptc.xy);\n"
"    float3 row1 = step(shadowmaptc.z, float3(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0)));\n"
"    float3 row2 = step(shadowmaptc.z, float3(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0)));\n"
"    float3 row3 = step(shadowmaptc.z, float3(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0)));\n"
"    float3 cols = row2 + lerp(row1, row3, offset.y);\n"
"    f = dot(lerp(cols.xy, cols.yz, offset.x), float2(0.25));\n"
"#      endif\n"
"#    else\n"
"    f = step(shadowmaptc.z, texRECT(Texture_ShadowMapRect, shadowmaptc.xy).r);\n"
"#    endif\n"
"\n"
"#  endif\n"
"#  ifdef USESHADOWMAPORTHO\n"
"	return lerp(ShadowMap_Parameters.w, 1.0, f);\n"
"#  else\n"
"	return f;\n"
"#  endif\n"
"}\n"
"# endif\n"
"\n"
"# ifdef USESHADOWMAP2D\n"
"#ifdef USESHADOWMAPVSDCT\n"
"float ShadowMapCompare(float3 dir, sampler2D Texture_ShadowMap2D, float4 ShadowMap_Parameters, float2 ShadowMap_TextureScale, samplerCUBE Texture_CubeProjection)\n"
"#else\n"
"float ShadowMapCompare(float3 dir, sampler2D Texture_ShadowMap2D, float4 ShadowMap_Parameters, float2 ShadowMap_TextureScale)\n"
"#endif\n"
"{\n"
"#ifdef USESHADOWMAPVSDCT\n"
"	float3 shadowmaptc = GetShadowMapTC2D(dir, ShadowMap_Parameters, Texture_CubeProjection);\n"
"#else\n"
"	float3 shadowmaptc = GetShadowMapTC2D(dir, ShadowMap_Parameters);\n"
"#endif\n"
"    float f;\n"
"\n"
"#  ifdef USESHADOWSAMPLER\n"
"#    ifdef USESHADOWMAPPCF\n"
"#      define texval(x, y) shadow2D(Texture_ShadowMap2D, float3(center + float2(x, y)*ShadowMap_TextureScale, shadowmaptc.z)).r  \n"
"    float2 center = shadowmaptc.xy*ShadowMap_TextureScale;\n"
"    f = dot(float4(0.25), float4(texval(-0.4, 1.0), texval(-1.0, -0.4), texval(0.4, -1.0), texval(1.0, 0.4)));\n"
"#    else\n"
"    f = shadow2D(Texture_ShadowMap2D, float3(shadowmaptc.xy*ShadowMap_TextureScale, shadowmaptc.z)).r;\n"
"#    endif\n"
"#  else\n"
"#    ifdef USESHADOWMAPPCF\n"
"#     if defined(GL_ARB_texture_gather) || defined(GL_AMD_texture_texture4)\n"
"#      ifdef GL_ARB_texture_gather\n"
"#        define texval(x, y) textureGatherOffset(Texture_ShadowMap2D, center, ivec(x, y))\n"
"#      else\n"
"#        define texval(x, y) texture4(Texture_ShadowMap2D, center + float2(x,y)*ShadowMap_TextureScale)\n"
"#      endif\n"
"    float2 center = shadowmaptc.xy - 0.5, offset = frac(center);\n"
"    center *= ShadowMap_TextureScale;\n"
"    float4 group1 = step(shadowmaptc.z, texval(-1.0, -1.0));\n"
"    float4 group2 = step(shadowmaptc.z, texval( 1.0, -1.0));\n"
"    float4 group3 = step(shadowmaptc.z, texval(-1.0,  1.0));\n"
"    float4 group4 = step(shadowmaptc.z, texval( 1.0,  1.0));\n"
"    float4 cols = float4(group1.rg, group2.rg) + float4(group3.ab, group4.ab) +\n"
"                lerp(float4(group1.ab, group2.ab), float4(group3.rg, group4.rg), offset.y);\n"
"    f = dot(lerp(cols.xyz, cols.yzw, offset.x), float3(1.0/9.0));\n"
"#     else\n"
"#        define texval(x, y) texDepth2D(Texture_ShadowMap2D, center + float2(x, y)*ShadowMap_TextureScale)  \n"
"#      if USESHADOWMAPPCF > 1\n"
"    float2 center = shadowmaptc.xy - 0.5, offset = frac(center);\n"
"    center *= ShadowMap_TextureScale;\n"
"    float4 row1 = step(shadowmaptc.z, float4(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0), texval( 2.0, -1.0)));\n"
"    float4 row2 = step(shadowmaptc.z, float4(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0), texval( 2.0,  0.0)));\n"
"    float4 row3 = step(shadowmaptc.z, float4(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0), texval( 2.0,  1.0)));\n"
"    float4 row4 = step(shadowmaptc.z, float4(texval(-1.0,  2.0), texval( 0.0,  2.0), texval( 1.0,  2.0), texval( 2.0,  2.0)));\n"
"    float4 cols = row2 + row3 + lerp(row1, row4, offset.y);\n"
"    f = dot(lerp(cols.xyz, cols.yzw, offset.x), float3(1.0/9.0));\n"
"#      else\n"
"    float2 center = shadowmaptc.xy*ShadowMap_TextureScale, offset = frac(shadowmaptc.xy);\n"
"    float3 row1 = step(shadowmaptc.z, float3(texval(-1.0, -1.0), texval( 0.0, -1.0), texval( 1.0, -1.0)));\n"
"    float3 row2 = step(shadowmaptc.z, float3(texval(-1.0,  0.0), texval( 0.0,  0.0), texval( 1.0,  0.0)));\n"
"    float3 row3 = step(shadowmaptc.z, float3(texval(-1.0,  1.0), texval( 0.0,  1.0), texval( 1.0,  1.0)));\n"
"    float3 cols = row2 + lerp(row1, row3, offset.y);\n"
"    f = dot(lerp(cols.xy, cols.yz, offset.x), float2(0.25));\n"
"#      endif\n"
"#     endif\n"
"#    else\n"
"    f = step(shadowmaptc.z, tex2D(Texture_ShadowMap2D, shadowmaptc.xy*ShadowMap_TextureScale).r);\n"
"#    endif\n"
"#  endif\n"
"#  ifdef USESHADOWMAPORTHO\n"
"	return lerp(ShadowMap_Parameters.w, 1.0, f);\n"
"#  else\n"
"	return f;\n"
"#  endif\n"
"}\n"
"# endif\n"
"\n"
"# ifdef USESHADOWMAPCUBE\n"
"float ShadowMapCompare(float3 dir, samplerCUBE Texture_ShadowMapCube, float4 ShadowMap_Parameters)\n"
"{\n"
"    // apply depth texture cubemap as light filter\n"
"    float4 shadowmaptc = GetShadowMapTCCube(dir, ShadowMap_Parameters);\n"
"    float f;\n"
"#  ifdef USESHADOWSAMPLER\n"
"    f = shadowCube(Texture_ShadowMapCube, shadowmaptc).r;\n"
"#  else\n"
"    f = step(shadowmaptc.w, texCUBE(Texture_ShadowMapCube, shadowmaptc.xyz).r);\n"
"#  endif\n"
"    return f;\n"
"}\n"
"# endif\n"
"#endif // !defined(MODE_LIGHTSOURCE) && !defined(MODE_DEFERREDLIGHTSOURCE)\n"
"#endif // FRAGMENT_SHADER\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_DEFERREDGEOMETRY\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"float4 gl_Color : COLOR0,\n"
"#endif\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"float4 gl_MultiTexCoord1 : TEXCOORD1,\n"
"float4 gl_MultiTexCoord2 : TEXCOORD2,\n"
"float4 gl_MultiTexCoord3 : TEXCOORD3,\n"
"uniform float4x4 TexMatrix,\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform float4x4 BackgroundTexMatrix,\n"
"#endif\n"
"uniform float4x4 ModelViewMatrix,\n"
"#ifdef USEOFFSETMAPPING\n"
"uniform float3 EyePosition,\n"
"#endif\n"
"out float4 gl_Position : POSITION,\n"
"out float4 gl_FrontColor : COLOR,\n"
"out float4 TexCoordBoth : TEXCOORD0,\n"
"#ifdef USEOFFSETMAPPING\n"
"out float3 EyeVector : TEXCOORD2,\n"
"#endif\n"
"out float3 VectorS : TEXCOORD5, // direction of S texcoord (sometimes crudely called tangent)\n"
"out float3 VectorT : TEXCOORD6, // direction of T texcoord (sometimes crudely called binormal)\n"
"out float3 VectorR : TEXCOORD7 // direction of R texcoord (surface normal)\n"
")\n"
"{\n"
"	TexCoordBoth = mul(TexMatrix, gl_MultiTexCoord0);\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	gl_FrontColor = gl_Color;\n"
"	TexCoordBoth.zw = float2(Backgroundmul(TexMatrix, gl_MultiTexCoord0));\n"
"#endif\n"
"\n"
"	// transform unnormalized eye direction into tangent space\n"
"#ifdef USEOFFSETMAPPING\n"
"	float3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	VectorS = mul(ModelViewMatrix, float4(gl_MultiTexCoord1.xyz, 0)).xyz;\n"
"	VectorT = mul(ModelViewMatrix, float4(gl_MultiTexCoord2.xyz, 0)).xyz;\n"
"	VectorR = mul(ModelViewMatrix, float4(gl_MultiTexCoord3.xyz, 0)).xyz;\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float4 TexCoordBoth : TEXCOORD0,\n"
"float3 EyeVector : TEXCOORD2,\n"
"float3 VectorS : TEXCOORD5, // direction of S texcoord (sometimes crudely called tangent)\n"
"float3 VectorT : TEXCOORD6, // direction of T texcoord (sometimes crudely called binormal)\n"
"float3 VectorR : TEXCOORD7, // direction of R texcoord (surface normal)\n"
"uniform sampler2D Texture_Normal,\n"
"#ifdef USEALPHAKILL\n"
"uniform sampler2D Texture_Color,\n"
"#endif\n"
"uniform sampler2D Texture_Gloss,\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform sampler2D Texture_SecondaryNormal,\n"
"uniform sampler2D Texture_SecondaryGloss,\n"
"#endif\n"
"#ifdef USEOFFSETMAPPING\n"
"uniform float OffsetMapping_Scale,\n"
"#endif\n"
"uniform half SpecularPower,\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	float2 TexCoord = TexCoordBoth.xy;\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	float2 TexCoordOffset = OffsetMapping(TexCoord, OffsetMapping_Scale, EyeVector, Texture_Normal);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"#ifdef USEALPHAKILL\n"
"	if (tex2D(Texture_Color, TexCoord).a < 0.5)\n"
"		discard;\n"
"#endif\n"
"\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	float alpha = tex2D(Texture_Color, TexCoord).a;\n"
"	float terrainblend = clamp(float(gl_FrontColor.a) * alpha * 2.0 - 0.5, float(0.0), float(1.0));\n"
"	//float terrainblend = min(float(gl_FrontColor.a) * alpha * 2.0, float(1.0));\n"
"	//float terrainblend = float(gl_FrontColor.a) * alpha > 0.5;\n"
"#endif\n"
"\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	float3 surfacenormal = lerp(float3(tex2D(Texture_SecondaryNormal, TexCoord2)), float3(tex2D(Texture_Normal, TexCoord)), terrainblend) - float3(0.5, 0.5, 0.5);\n"
"	float a = lerp(tex2D(Texture_SecondaryGloss, TexCoord2), tex2D(Texture_Gloss, TexCoord).a, terrainblend);\n"
"#else\n"
"	float3 surfacenormal = float3(tex2D(Texture_Normal, TexCoord)) - float3(0.5, 0.5, 0.5);\n"
"	float a = tex2D(Texture_Gloss, TexCoord).a;\n"
"#endif\n"
"\n"
"	gl_FragColor = float4(normalize(surfacenormal.x * VectorS + surfacenormal.y * VectorT + surfacenormal.z * VectorR) * 0.5 + float3(0.5, 0.5, 0.5), 1);\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"#else // !MODE_DEFERREDGEOMETRY\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"uniform float4x4 ModelViewMatrix,\n"
"out float4 gl_Position : POSITION,\n"
"out float4 ModelViewPosition : TEXCOORD0\n"
")\n"
"{\n"
"	ModelViewPosition = mul(ModelViewMatrix, gl_Vertex);\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"float2 Pixel : WPOS,\n"
"float4 ModelViewPosition : TEXCOORD0,\n"
"uniform float4x4 ViewToLight,\n"
"uniform float2 ScreenToDepth, // ScreenToDepth = float2(Far / (Far - Near), Far * Near / (Near - Far));\n"
"uniform float3 LightPosition,\n"
"uniform half2 PixelToScreenTexCoord,\n"
"uniform half3 DeferredColor_Ambient,\n"
"uniform half3 DeferredColor_Diffuse,\n"
"#ifdef USESPECULAR\n"
"uniform half3 DeferredColor_Specular,\n"
"uniform half SpecularPower,\n"
"#endif\n"
"uniform sampler2D Texture_Attenuation,\n"
"uniform sampler2D Texture_ScreenDepth,\n"
"uniform sampler2D Texture_ScreenNormalMap,\n"
"\n"
"#ifdef USESHADOWMAPRECT\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform samplerRECTShadow Texture_ShadowMapRect,\n"
"# else\n"
"uniform samplerRECT Texture_ShadowMapRect,\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAP2D\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform sampler2DShadow Texture_ShadowMap2D,\n"
"# else\n"
"uniform sampler2D Texture_ShadowMap2D,\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPVSDCT\n"
"uniform samplerCUBE Texture_CubeProjection,\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform samplerCUBEShadow Texture_ShadowMapCube,\n"
"# else\n"
"uniform samplerCUBE Texture_ShadowMapCube,\n"
"# endif\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D) || defined(USESHADOWMAPCUBE)\n"
"uniform float2 ShadowMap_TextureScale,\n"
"uniform float4 ShadowMap_Parameters,\n"
"#endif\n"
"\n"
"out float4 gl_FragData0 : COLOR0,\n"
"out float4 gl_FragData1 : COLOR1\n"
")\n"
"{\n"
"	// calculate viewspace pixel position\n"
"	float2 ScreenTexCoord = Pixel * PixelToScreenTexCoord;\n"
"	ScreenTexCoord.y = ScreenTexCoord.y * -1 + 1; // Cg is opposite?\n"
"	float3 position;\n"
"	position.z = ScreenToDepth.y / (texDepth2D(Texture_ScreenDepth, ScreenTexCoord) + ScreenToDepth.x);\n"
"	position.xy = ModelViewPosition.xy * (position.z / ModelViewPosition.z);\n"
"	// decode viewspace pixel normal\n"
"	half4 normalmap = tex2D(Texture_ScreenNormalMap, ScreenTexCoord);\n"
"	half3 surfacenormal = normalize(normalmap.rgb - half3(0.5,0.5,0.5));\n"
"	// surfacenormal = pixel normal in viewspace\n"
"	// LightVector = pixel to light in viewspace\n"
"	// CubeVector = position in lightspace\n"
"	// eyevector = pixel to view in viewspace\n"
"	float3 CubeVector = float3(mul(ViewToLight, float4(position,1)));\n"
"	half fade = half(tex2D(Texture_Attenuation, float2(length(CubeVector), 0.0)));\n"
"#ifdef USEDIFFUSE\n"
"	// calculate diffuse shading\n"
"	half3 lightnormal = half3(normalize(LightPosition - position));\n"
"	half diffuse = half(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	// calculate directional shading\n"
"	float3 eyevector = position * -1.0;\n"
"#  ifdef USEEXACTSPECULARMATH\n"
"	half specular = pow(half(max(float(dot(reflect(lightnormal, surfacenormal), normalize(eyevector)))*-1.0, 0.0)), SpecularPower * normalmap.a);\n"
"#  else\n"
"	half3 specularnormal = normalize(lightnormal + half3(normalize(eyevector)));\n"
"	half specular = pow(half(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * normalmap.a);\n"
"#  endif\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAP2D) || defined(USESHADOWMAPRECT) || defined(USESHADOWMAPCUBE)\n"
"	fade *= ShadowMapCompare(CubeVector,\n"
"# if defined(USESHADOWMAP2D)\n"
"Texture_ShadowMap2D, ShadowMap_Parameters, ShadowMap_TextureScale\n"
"# endif\n"
"# if defined(USESHADOWMAPRECT)\n"
"Texture_ShadowMapRect, ShadowMap_Parameters\n"
"# endif\n"
"# if defined(USESHADOWMAPCUBE)\n"
"Texture_ShadowMapCube, ShadowMap_Parameters\n"
"# endif\n"
"\n"
"#ifdef USESHADOWMAPVSDCT\n"
", Texture_CubeProjection\n"
"#endif\n"
"	);\n"
"#endif\n"
"\n"
"#ifdef USEDIFFUSE\n"
"	gl_FragData0 = float4((DeferredColor_Ambient + DeferredColor_Diffuse * diffuse) * fade, 1.0);\n"
"#else\n"
"	gl_FragData0 = float4(DeferredColor_Ambient * fade, 1.0);\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"	gl_FragData1 = float4(DeferredColor_Specular * (specular * fade), 1.0);\n"
"#else\n"
"	gl_FragData1 = float4(0.0, 0.0, 0.0, 1.0);\n"
"#endif\n"
"\n"
"# ifdef USECUBEFILTER\n"
"	float3 cubecolor = texCUBE(Texture_Cube, CubeVector).rgb;\n"
"	gl_FragData0.rgb *= cubecolor;\n"
"	gl_FragData1.rgb *= cubecolor;\n"
"# endif\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"#else // !MODE_DEFERREDLIGHTSOURCE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"void main\n"
"(\n"
"float4 gl_Vertex : POSITION,\n"
"uniform float4x4 ModelViewProjectionMatrix,\n"
"#if defined(USEVERTEXTEXTUREBLEND) || defined(MODE_VERTEXCOLOR)\n"
"float4 gl_Color : COLOR0,\n"
"#endif\n"
"float4 gl_MultiTexCoord0 : TEXCOORD0,\n"
"float4 gl_MultiTexCoord1 : TEXCOORD1,\n"
"float4 gl_MultiTexCoord2 : TEXCOORD2,\n"
"float4 gl_MultiTexCoord3 : TEXCOORD3,\n"
"float4 gl_MultiTexCoord4 : TEXCOORD4,\n"
"\n"
"uniform float3 EyePosition,\n"
"uniform float4x4 TexMatrix,\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform float4x4 BackgroundTexMatrix,\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform float4x4 ModelToLight,\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform float3 LightPosition,\n"
"#endif\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"uniform float3 LightDir,\n"
"#endif\n"
"uniform float4 FogPlane,\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"uniform float3 LightPosition,\n"
"#endif\n"
"#ifdef USESHADOWMAPORTHO\n"
"uniform float4x4 ShadowMapMatrix,\n"
"#endif\n"
"\n"
"out float4 gl_FrontColor : COLOR,\n"
"out float4 TexCoordBoth : TEXCOORD0,\n"
"#ifdef USELIGHTMAP\n"
"out float2 TexCoordLightmap : TEXCOORD1,\n"
"#endif\n"
"#ifdef USEEYEVECTOR\n"
"out float3 EyeVector : TEXCOORD2,\n"
"#endif\n"
"#ifdef USEREFLECTION\n"
"out float4 ModelViewProjectionPosition : TEXCOORD3,\n"
"#endif\n"
"#ifdef USEFOG\n"
"out float4 EyeVectorModelSpaceFogPlaneVertexDist : TEXCOORD4,\n"
"#endif\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_LIGHTDIRECTION)\n"
"out float3 LightVector : TEXCOORD5,\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"out float3 CubeVector : TEXCOORD3,\n"
"#endif\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_DEFERREDGEOMETRY) || defined(USEREFLECTCUBE)\n"
"out float3 VectorS : TEXCOORD5, // direction of S texcoord (sometimes crudely called tangent)\n"
"out float3 VectorT : TEXCOORD6, // direction of T texcoord (sometimes crudely called binormal)\n"
"out float3 VectorR : TEXCOORD7, // direction of R texcoord (surface normal)\n"
"#endif\n"
"#ifdef USESHADOWMAPORTHO\n"
"out float3 ShadowMapTC : TEXCOORD8,\n"
"#endif\n"
"out float4 gl_Position : POSITION\n"
")\n"
"{\n"
"#if defined(MODE_VERTEXCOLOR) || defined(USEVERTEXTEXTUREBLEND)\n"
"	gl_FrontColor = gl_Color;\n"
"#endif\n"
"	// copy the surface texcoord\n"
"	TexCoordBoth = mul(TexMatrix, gl_MultiTexCoord0);\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	TexCoordBoth.zw = mul(BackgroundTexMatrix, gl_MultiTexCoord0).xy;\n"
"#endif\n"
"#ifdef USELIGHTMAP\n"
"	TexCoordLightmap = float2(gl_MultiTexCoord4);\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// transform vertex position into light attenuation/cubemap space\n"
"	// (-1 to +1 across the light box)\n"
"	CubeVector = float3(mul(ModelToLight, gl_Vertex));\n"
"\n"
"# ifdef USEDIFFUSE\n"
"	// transform unnormalized light direction into tangent space\n"
"	// (we use unnormalized to ensure that it interpolates correctly and then\n"
"	//  normalize it per pixel)\n"
"	float3 lightminusvertex = LightPosition - gl_Vertex.xyz;\n"
"	LightVector.x = dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n"
"# endif\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTDIRECTION) && defined(USEDIFFUSE)\n"
"	LightVector.x = dot(LightDir, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(LightDir, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(LightDir, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	// transform unnormalized eye direction into tangent space\n"
"#ifdef USEEYEVECTOR\n"
"	float3 EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"#ifdef USEFOG\n"
"	EyeVectorModelSpaceFogPlaneVertexDist.xyz = EyePosition - gl_Vertex.xyz;\n"
"	EyeVectorModelSpaceFogPlaneVertexDist.w = dot(FogPlane, gl_Vertex);\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"	VectorS = gl_MultiTexCoord1.xyz;\n"
"	VectorT = gl_MultiTexCoord2.xyz;\n"
"	VectorR = gl_MultiTexCoord3.xyz;\n"
"#endif\n"
"\n"
"	// transform vertex to camera space, using ftransform to match non-VS rendering\n"
"	gl_Position = mul(ModelViewProjectionMatrix, gl_Vertex);\n"
"\n"
"#ifdef USESHADOWMAPORTHO\n"
"	ShadowMapTC = float3(mul(ShadowMapMatrix, gl_Position));\n"
"#endif\n"
"\n"
"#ifdef USEREFLECTION\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"#endif\n"
"}\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"void main\n"
"(\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"float2 Pixel : WPOS,\n"
"#endif\n"
"float4 gl_FrontColor : COLOR,\n"
"float4 TexCoordBoth : TEXCOORD0,\n"
"#ifdef USELIGHTMAP\n"
"float2 TexCoordLightmap : TEXCOORD1,\n"
"#endif\n"
"#ifdef USEEYEVECTOR\n"
"float3 EyeVector : TEXCOORD2,\n"
"#endif\n"
"#ifdef USEREFLECTION\n"
"float4 ModelViewProjectionPosition : TEXCOORD3,\n"
"#endif\n"
"#ifdef USEFOG\n"
"float4 EyeVectorModelSpaceFogPlaneVertexDist : TEXCOORD4,\n"
"#endif\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_LIGHTDIRECTION)\n"
"float3 LightVector : TEXCOORD5,\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"float3 CubeVector : TEXCOORD3,\n"
"#endif\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"float4 ModelViewPosition : TEXCOORD0,\n"
"#endif\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_DEFERREDGEOMETRY) || defined(USEREFLECTCUBE)\n"
"float3 VectorS : TEXCOORD5, // direction of S texcoord (sometimes crudely called tangent)\n"
"float3 VectorT : TEXCOORD6, // direction of T texcoord (sometimes crudely called binormal)\n"
"float3 VectorR : TEXCOORD7, // direction of R texcoord (surface normal)\n"
"#endif\n"
"#ifdef USESHADOWMAPORTHO\n"
"float3 ShadowMapTC : TEXCOORD8,\n"
"#endif\n"
"\n"
"uniform sampler2D Texture_Normal,\n"
"uniform sampler2D Texture_Color,\n"
"#if defined(USESPECULAR) || defined(USEDEFERREDLIGHTMAP)\n"
"uniform sampler2D Texture_Gloss,\n"
"#endif\n"
"#ifdef USEGLOW\n"
"uniform sampler2D Texture_Glow,\n"
"#endif\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"uniform sampler2D Texture_SecondaryNormal,\n"
"uniform sampler2D Texture_SecondaryColor,\n"
"#if defined(USESPECULAR) || defined(USEDEFERREDLIGHTMAP)\n"
"uniform sampler2D Texture_SecondaryGloss,\n"
"#endif\n"
"#ifdef USEGLOW\n"
"uniform sampler2D Texture_SecondaryGlow,\n"
"#endif\n"
"#endif\n"
"#ifdef USECOLORMAPPING\n"
"uniform sampler2D Texture_Pants,\n"
"uniform sampler2D Texture_Shirt,\n"
"#endif\n"
"#ifdef USEFOG\n"
"uniform sampler2D Texture_FogMask,\n"
"#endif\n"
"#ifdef USELIGHTMAP\n"
"uniform sampler2D Texture_Lightmap,\n"
"#endif\n"
"#if defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE) || defined(MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)\n"
"uniform sampler2D Texture_Deluxemap,\n"
"#endif\n"
"#ifdef USEREFLECTION\n"
"uniform sampler2D Texture_Reflection,\n"
"#endif\n"
"\n"
"#ifdef MODE_DEFERREDLIGHTSOURCE\n"
"uniform sampler2D Texture_ScreenDepth,\n"
"uniform sampler2D Texture_ScreenNormalMap,\n"
"#endif\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"uniform sampler2D Texture_ScreenDiffuse,\n"
"uniform sampler2D Texture_ScreenSpecular,\n"
"#endif\n"
"\n"
"#ifdef USECOLORMAPPING\n"
"uniform half3 Color_Pants,\n"
"uniform half3 Color_Shirt,\n"
"#endif\n"
"#ifdef USEFOG\n"
"uniform float3 FogColor,\n"
"uniform float FogRangeRecip,\n"
"uniform float FogPlaneViewDist,\n"
"uniform float FogHeightFade,\n"
"#endif\n"
"\n"
"#ifdef USEOFFSETMAPPING\n"
"uniform float OffsetMapping_Scale,\n"
"#endif\n"
"\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"uniform half2 PixelToScreenTexCoord,\n"
"uniform half3 DeferredMod_Diffuse,\n"
"uniform half3 DeferredMod_Specular,\n"
"#endif\n"
"uniform half3 Color_Ambient,\n"
"uniform half3 Color_Diffuse,\n"
"uniform half3 Color_Specular,\n"
"uniform half SpecularPower,\n"
"#ifdef USEGLOW\n"
"uniform half3 Color_Glow,\n"
"#endif\n"
"uniform half Alpha,\n"
"#ifdef USEREFLECTION\n"
"uniform float4 DistortScaleRefractReflect,\n"
"uniform float4 ScreenScaleRefractReflect,\n"
"uniform float4 ScreenCenterRefractReflect,\n"
"uniform half4 ReflectColor,\n"
"#endif\n"
"#ifdef USEREFLECTCUBE\n"
"uniform float4x4 ModelToReflectCube,\n"
"uniform sampler2D Texture_ReflectMask,\n"
"uniform samplerCUBE Texture_ReflectCube,\n"
"#endif\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"uniform half3 LightColor,\n"
"#endif\n"
"#ifdef MODE_LIGHTSOURCE\n"
"uniform half3 LightColor,\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_DEFERREDLIGHTSOURCE)\n"
"uniform sampler2D Texture_Attenuation,\n"
"uniform samplerCUBE Texture_Cube,\n"
"#endif\n"
"\n"
"#if defined(MODE_LIGHTSOURCE) || defined(MODE_DEFERREDLIGHTSOURCE) || defined(USESHADOWMAPORTHO)\n"
"\n"
"#ifdef USESHADOWMAPRECT\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform samplerRECTShadow Texture_ShadowMapRect,\n"
"# else\n"
"uniform samplerRECT Texture_ShadowMapRect,\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAP2D\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform sampler2DShadow Texture_ShadowMap2D,\n"
"# else\n"
"uniform sampler2D Texture_ShadowMap2D,\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPVSDCT\n"
"uniform samplerCUBE Texture_CubeProjection,\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPCUBE\n"
"# ifdef USESHADOWSAMPLER\n"
"uniform samplerCUBEShadow Texture_ShadowMapCube,\n"
"# else\n"
"uniform samplerCUBE Texture_ShadowMapCube,\n"
"# endif\n"
"#endif\n"
"\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAP2D) || defined(USESHADOWMAPCUBE)\n"
"uniform float2 ShadowMap_TextureScale,\n"
"uniform float4 ShadowMap_Parameters,\n"
"#endif\n"
"#endif // !defined(MODE_LIGHTSOURCE) && !defined(MODE_DEFERREDLIGHTSOURCE) && !defined(USESHADOWMAPORTHO)\n"
"\n"
"out float4 gl_FragColor : COLOR\n"
")\n"
"{\n"
"	float2 TexCoord = TexCoordBoth.xy;\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	float2 TexCoord2 = TexCoordBoth.zw;\n"
"#endif\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	float2 TexCoordOffset = OffsetMapping(TexCoord, OffsetMapping_Scale, EyeVector, Texture_Normal);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	// combine the diffuse textures (base, pants, shirt)\n"
"	half4 color = half4(tex2D(Texture_Color, TexCoord));\n"
"#ifdef USEALPHAKILL\n"
"	if (color.a < 0.5)\n"
"		discard;\n"
"#endif\n"
"	color.a *= Alpha;\n"
"#ifdef USECOLORMAPPING\n"
"	color.rgb += half3(tex2D(Texture_Pants, TexCoord)) * Color_Pants + half3(tex2D(Texture_Shirt, TexCoord)) * Color_Shirt;\n"
"#endif\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	float terrainblend = clamp(half(gl_FrontColor.a) * color.a * 2.0 - 0.5, half(0.0), half(1.0));\n"
"	//half terrainblend = min(half(gl_FrontColor.a) * color.a * 2.0, half(1.0));\n"
"	//half terrainblend = half(gl_FrontColor.a) * color.a > 0.5;\n"
"	color.rgb = half3(lerp(float3(tex2D(Texture_SecondaryColor, TexCoord2)), float3(color.rgb), terrainblend));\n"
"	color.a = 1.0;\n"
"	//color = lerp(half4(1, 0, 0, 1), color, terrainblend);\n"
"#endif\n"
"\n"
"	// get the surface normal\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	half3 surfacenormal = normalize(half3(lerp(float3(tex2D(Texture_SecondaryNormal, TexCoord2)), float3(tex2D(Texture_Normal, TexCoord)), terrainblend)) - half3(0.5, 0.5, 0.5));\n"
"#else\n"
"	half3 surfacenormal = normalize(half3(tex2D(Texture_Normal, TexCoord)) - half3(0.5, 0.5, 0.5));\n"
"#endif\n"
"\n"
"	// get the material colors\n"
"	half3 diffusetex = color.rgb;\n"
"#if defined(USESPECULAR) || defined(USEDEFERREDLIGHTMAP)\n"
"# ifdef USEVERTEXTEXTUREBLEND\n"
"	half4 glosstex = half4(lerp(float4(tex2D(Texture_SecondaryGloss, TexCoord2)), float4(tex2D(Texture_Gloss, TexCoord)), terrainblend));\n"
"# else\n"
"	half4 glosstex = half4(tex2D(Texture_Gloss, TexCoord));\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USEREFLECTCUBE\n"
"	float3 TangentReflectVector = reflect(-EyeVector, surfacenormal);\n"
"	float3 ModelReflectVector = TangentReflectVector.x * VectorS + TangentReflectVector.y * VectorT + TangentReflectVector.z * VectorR;\n"
"	float3 ReflectCubeTexCoord = float3(mul(ModelToReflectCube, float4(ModelReflectVector, 0)));\n"
"	diffusetex += half3(tex2D(Texture_ReflectMask, TexCoord)) * half3(texCUBE(Texture_ReflectCube, ReflectCubeTexCoord));\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// light source\n"
"#ifdef USEDIFFUSE\n"
"	half3 lightnormal = half3(normalize(LightVector));\n"
"	half diffuse = half(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"	color.rgb = diffusetex * (Color_Ambient + diffuse * Color_Diffuse);\n"
"#ifdef USESPECULAR\n"
"#ifdef USEEXACTSPECULARMATH\n"
"	half specular = pow(half(max(float(dot(reflect(lightnormal, surfacenormal), normalize(EyeVector)))*-1.0, 0.0)), SpecularPower * glosstex.a);\n"
"#else\n"
"	half3 specularnormal = normalize(lightnormal + half3(normalize(EyeVector)));\n"
"	half specular = pow(half(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * glosstex.a);\n"
"#endif\n"
"	color.rgb += glosstex.rgb * (specular * Color_Specular);\n"
"#endif\n"
"#else\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"#endif\n"
"	color.rgb *= LightColor;\n"
"	color.rgb *= half(tex2D(Texture_Attenuation, float2(length(CubeVector), 0.0)));\n"
"#if defined(USESHADOWMAPRECT) || defined(USESHADOWMAPCUBE) || defined(USESHADOWMAP2D)\n"
"	color.rgb *= ShadowMapCompare(CubeVector,\n"
"# if defined(USESHADOWMAP2D)\n"
"Texture_ShadowMap2D, ShadowMap_Parameters, ShadowMap_TextureScale\n"
"# endif\n"
"# if defined(USESHADOWMAPRECT)\n"
"Texture_ShadowMapRect, ShadowMap_Parameters\n"
"# endif\n"
"# if defined(USESHADOWMAPCUBE)\n"
"Texture_ShadowMapCube, ShadowMap_Parameters\n"
"# endif\n"
"\n"
"#ifdef USESHADOWMAPVSDCT\n"
", Texture_CubeProjection\n"
"#endif\n"
"	);\n"
"\n"
"#endif\n"
"# ifdef USECUBEFILTER\n"
"	color.rgb *= half3(texCUBE(Texture_Cube, CubeVector));\n"
"# endif\n"
"#endif // MODE_LIGHTSOURCE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"#define SHADING\n"
"#ifdef USEDIFFUSE\n"
"	half3 lightnormal = half3(normalize(LightVector));\n"
"#endif\n"
"#define lightcolor LightColor\n"
"#endif // MODE_LIGHTDIRECTION\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"#define SHADING\n"
"	// deluxemap lightmapping using light vectors in modelspace (q3map2 -light -deluxe)\n"
"	half3 lightnormal_modelspace = half3(tex2D(Texture_Deluxemap, TexCoordLightmap)) * 2.0 + half3(-1.0, -1.0, -1.0);\n"
"	half3 lightcolor = half3(tex2D(Texture_Lightmap, TexCoordLightmap));\n"
"	// convert modelspace light vector to tangentspace\n"
"	half3 lightnormal;\n"
"	lightnormal.x = dot(lightnormal_modelspace, half3(VectorS));\n"
"	lightnormal.y = dot(lightnormal_modelspace, half3(VectorT));\n"
"	lightnormal.z = dot(lightnormal_modelspace, half3(VectorR));\n"
"	// calculate directional shading (and undoing the existing angle attenuation on the lightmap by the division)\n"
"	// note that q3map2 is too stupid to calculate proper surface normals when q3map_nonplanar\n"
"	// is used (the lightmap and deluxemap coords correspond to virtually random coordinates\n"
"	// on that luxel, and NOT to its center, because recursive triangle subdivision is used\n"
"	// to map the luxels to coordinates on the draw surfaces), which also causes\n"
"	// deluxemaps to be wrong because light contributions from the wrong side of the surface\n"
"	// are added up. To prevent divisions by zero or strong exaggerations, a max()\n"
"	// nudge is done here at expense of some additional fps. This is ONLY needed for\n"
"	// deluxemaps, tangentspace deluxemap avoid this problem by design.\n"
"	lightcolor *= 1.0 / max(0.25, lightnormal.z);\n"
"#endif // MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n"
"#define SHADING\n"
"	// deluxemap lightmapping using light vectors in tangentspace (hmap2 -light)\n"
"	half3 lightnormal = half3(tex2D(Texture_Deluxemap, TexCoordLightmap)) * 2.0 + half3(-1.0, -1.0, -1.0);\n"
"	half3 lightcolor = half3(tex2D(Texture_Lightmap, TexCoordLightmap));\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTMAP\n"
"	color.rgb = diffusetex * (Color_Ambient + half3(tex2D(Texture_Lightmap, TexCoordLightmap)) * Color_Diffuse);\n"
"#endif // MODE_LIGHTMAP\n"
"#ifdef MODE_VERTEXCOLOR\n"
"	color.rgb = diffusetex * (Color_Ambient + half3(gl_FrontColor.rgb) * Color_Diffuse);\n"
"#endif // MODE_VERTEXCOLOR\n"
"#ifdef MODE_FLATCOLOR\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"#endif // MODE_FLATCOLOR\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef SHADING\n"
"# ifdef USEDIFFUSE\n"
"	half diffuse = half(max(float(dot(surfacenormal, lightnormal)), 0.0));\n"
"#  ifdef USESPECULAR\n"
"#   ifdef USEEXACTSPECULARMATH\n"
"	half specular = pow(half(max(float(dot(reflect(lightnormal, surfacenormal), normalize(EyeVector)))*-1.0, 0.0)), SpecularPower * glosstex.a);\n"
"#   else\n"
"	half3 specularnormal = normalize(lightnormal + half3(normalize(EyeVector)));\n"
"	half specular = pow(half(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower * glosstex.a);\n"
"#   endif\n"
"	color.rgb = diffusetex * Color_Ambient + (diffusetex * Color_Diffuse * diffuse + glosstex.rgb * Color_Specular * specular) * lightcolor;\n"
"#  else\n"
"	color.rgb = diffusetex * (Color_Ambient + Color_Diffuse * diffuse * lightcolor);\n"
"#  endif\n"
"# else\n"
"	color.rgb = diffusetex * Color_Ambient;\n"
"# endif\n"
"#endif\n"
"\n"
"#ifdef USESHADOWMAPORTHO\n"
"	color.rgb *= ShadowMapCompare(ShadowMapTC,\n"
"# if defined(USESHADOWMAP2D)\n"
"Texture_ShadowMap2D, ShadowMap_Parameters, ShadowMap_TextureScale\n"
"# endif\n"
"# if defined(USESHADOWMAPRECT)\n"
"Texture_ShadowMapRect, ShadowMap_Parameters\n"
"# endif\n"
"	);\n"
"#endif\n"
"\n"
"#ifdef USEDEFERREDLIGHTMAP\n"
"	float2 ScreenTexCoord = Pixel * PixelToScreenTexCoord;\n"
"	color.rgb += diffusetex * half3(tex2D(Texture_ScreenDiffuse, ScreenTexCoord)) * DeferredMod_Diffuse;\n"
"	color.rgb += glosstex.rgb * half3(tex2D(Texture_ScreenSpecular, ScreenTexCoord)) * DeferredMod_Specular;\n"
"#endif\n"
"\n"
"#ifdef USEGLOW\n"
"#ifdef USEVERTEXTEXTUREBLEND\n"
"	color.rgb += lerp(half3(tex2D(Texture_SecondaryGlow, TexCoord2)), half3(tex2D(Texture_Glow, TexCoord)), terrainblend) * Color_Glow;\n"
"#else\n"
"	color.rgb += half3(tex2D(Texture_Glow, TexCoord)) * Color_Glow;\n"
"#endif\n"
"#endif\n"
"\n"
"#ifdef USEFOG\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	color.rgb *= half(FogVertex(EyeVectorModelSpaceFogPlaneVertexDist.xyz, EyeVectorModelSpaceFogPlaneVertexDist.w, FogRangeRecip, FogPlaneViewDist, FogHeightFade, Texture_FogMask));\n"
"#else\n"
"	color.rgb = lerp(FogColor, float3(color.rgb), FogVertex(EyeVectorModelSpaceFogPlaneVertexDist.xyz, EyeVectorModelSpaceFogPlaneVertexDist.w, FogRangeRecip, FogPlaneViewDist, FogHeightFade, Texture_FogMask));\n"
"#endif\n"
"#endif\n"
"\n"
"	// reflection must come last because it already contains exactly the correct fog (the reflection render preserves camera distance from the plane, it only flips the side) and ContrastBoost/SceneBrightness\n"
"#ifdef USEREFLECTION\n"
"	float4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//float4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(half3(tex2D(Texture_Normal, TexCoord)) - half3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	float2 SafeScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW.zw + ScreenCenterRefractReflect.zw;\n"
"	float2 ScreenTexCoord = SafeScreenTexCoord + float3(normalize(half3(tex2D(Texture_Normal, TexCoord)) - half3(0.5))).xy * DistortScaleRefractReflect.zw;\n"
"	// FIXME temporary hack to detect the case that the reflection\n"
"	// gets blackened at edges due to leaving the area that contains actual\n"
"	// content.\n"
"	// Remove this 'ack once we have a better way to stop this thing from\n"
"	// 'appening.\n"
"	float f = min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord + float2(0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord + float2(0.01, -0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord + float2(-0.01, 0.01)).rgb) / 0.05);\n"
"	f      *= min(1.0, length(tex2D(Texture_Reflection, ScreenTexCoord + float2(-0.01, -0.01)).rgb) / 0.05);\n"
"	ScreenTexCoord = lerp(SafeScreenTexCoord, ScreenTexCoord, f);\n"
"	color.rgb = lerp(color.rgb, half3(tex2D(Texture_Reflection, ScreenTexCoord)) * ReflectColor.rgb, ReflectColor.a);\n"
"#endif\n"
"\n"
"	gl_FragColor = float4(color);\n"
"}\n"
"#endif // FRAGMENT_SHADER\n"
"\n"
"#endif // !MODE_DEFERREDLIGHTSOURCE\n"
"#endif // !MODE_DEFERREDGEOMETRY\n"
"#endif // !MODE_WATER\n"
"#endif // !MODE_REFRACTION\n"
"#endif // !MODE_BLOOMBLUR\n"
"#endif // !MODE_GENERIC\n"
"#endif // !MODE_POSTPROCESS\n"
"#endif // !MODE_SHOWDEPTH\n"
"#endif // !MODE_DEPTH_OR_SHADOW\n"
;

char *glslshaderstring = NULL;
char *cgshaderstring = NULL;

//=======================================================================================================================================================

typedef struct shaderpermutationinfo_s
{
	const char *pretext;
	const char *name;
}
shaderpermutationinfo_t;

typedef struct shadermodeinfo_s
{
	const char *vertexfilename;
	const char *geometryfilename;
	const char *fragmentfilename;
	const char *pretext;
	const char *name;
}
shadermodeinfo_t;

typedef enum shaderpermutation_e
{
	SHADERPERMUTATION_DIFFUSE = 1<<0, ///< (lightsource) whether to use directional shading
	SHADERPERMUTATION_VERTEXTEXTUREBLEND = 1<<1, ///< indicates this is a two-layer material blend based on vertex alpha (q3bsp)
	SHADERPERMUTATION_VIEWTINT = 1<<2, ///< view tint (postprocessing only)
	SHADERPERMUTATION_COLORMAPPING = 1<<3, ///< indicates this is a colormapped skin
	SHADERPERMUTATION_SATURATION = 1<<4, ///< saturation (postprocessing only)
	SHADERPERMUTATION_FOGINSIDE = 1<<5, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_FOGOUTSIDE = 1<<6, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_GAMMARAMPS = 1<<7, ///< gamma (postprocessing only)
	SHADERPERMUTATION_CUBEFILTER = 1<<8, ///< (lightsource) use cubemap light filter
	SHADERPERMUTATION_GLOW = 1<<9, ///< (lightmap) blend in an additive glow texture
	SHADERPERMUTATION_BLOOM = 1<<10, ///< bloom (postprocessing only)
	SHADERPERMUTATION_SPECULAR = 1<<11, ///< (lightsource or deluxemapping) render specular effects
	SHADERPERMUTATION_POSTPROCESSING = 1<<12, ///< user defined postprocessing (postprocessing only)
	SHADERPERMUTATION_EXACTSPECULARMATH = 1<<13, ///< (lightsource or deluxemapping) use exact reflection map for specular effects, as opposed to the usual OpenGL approximation
	SHADERPERMUTATION_REFLECTION = 1<<14, ///< normalmap-perturbed reflection of the scene infront of the surface, preformed as an overlay on the surface
	SHADERPERMUTATION_OFFSETMAPPING = 1<<15, ///< adjust texcoords to roughly simulate a displacement mapped surface
	SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING = 1<<16, ///< adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)
	SHADERPERMUTATION_SHADOWMAPRECT = 1<<17, ///< (lightsource) use shadowmap rectangle texture as light filter
	SHADERPERMUTATION_SHADOWMAPCUBE = 1<<18, ///< (lightsource) use shadowmap cubemap texture as light filter
	SHADERPERMUTATION_SHADOWMAP2D = 1<<19, ///< (lightsource) use shadowmap rectangle texture as light filter
	SHADERPERMUTATION_SHADOWMAPPCF = 1<<20, ///< (lightsource) use percentage closer filtering on shadowmap test results
	SHADERPERMUTATION_SHADOWMAPPCF2 = 1<<21, ///< (lightsource) use higher quality percentage closer filtering on shadowmap test results
	SHADERPERMUTATION_SHADOWSAMPLER = 1<<22, ///< (lightsource) use hardware shadowmap test
	SHADERPERMUTATION_SHADOWMAPVSDCT = 1<<23, ///< (lightsource) use virtual shadow depth cube texture for shadowmap indexing
	SHADERPERMUTATION_SHADOWMAPORTHO = 1<<24, //< (lightsource) use orthographic shadowmap projection
	SHADERPERMUTATION_DEFERREDLIGHTMAP = 1<<25, ///< (lightmap) read Texture_ScreenDiffuse/Specular textures and add them on top of lightmapping
	SHADERPERMUTATION_ALPHAKILL = 1<<26, ///< (deferredgeometry) discard pixel if diffuse texture alpha below 0.5
	SHADERPERMUTATION_REFLECTCUBE = 1<<27, ///< fake reflections using global cubemap (not HDRI light probe)
	SHADERPERMUTATION_LIMIT = 1<<28, ///< size of permutations array
	SHADERPERMUTATION_COUNT = 28 ///< size of shaderpermutationinfo array
}
shaderpermutation_t;

// NOTE: MUST MATCH ORDER OF SHADERPERMUTATION_* DEFINES!
shaderpermutationinfo_t shaderpermutationinfo[SHADERPERMUTATION_COUNT] =
{
	{"#define USEDIFFUSE\n", " diffuse"},
	{"#define USEVERTEXTEXTUREBLEND\n", " vertextextureblend"},
	{"#define USEVIEWTINT\n", " viewtint"},
	{"#define USECOLORMAPPING\n", " colormapping"},
	{"#define USESATURATION\n", " saturation"},
	{"#define USEFOGINSIDE\n", " foginside"},
	{"#define USEFOGOUTSIDE\n", " fogoutside"},
	{"#define USEGAMMARAMPS\n", " gammaramps"},
	{"#define USECUBEFILTER\n", " cubefilter"},
	{"#define USEGLOW\n", " glow"},
	{"#define USEBLOOM\n", " bloom"},
	{"#define USESPECULAR\n", " specular"},
	{"#define USEPOSTPROCESSING\n", " postprocessing"},
	{"#define USEEXACTSPECULARMATH\n", " exactspecularmath"},
	{"#define USEREFLECTION\n", " reflection"},
	{"#define USEOFFSETMAPPING\n", " offsetmapping"},
	{"#define USEOFFSETMAPPING_RELIEFMAPPING\n", " reliefmapping"},
	{"#define USESHADOWMAPRECT\n", " shadowmaprect"},
	{"#define USESHADOWMAPCUBE\n", " shadowmapcube"},
	{"#define USESHADOWMAP2D\n", " shadowmap2d"},
	{"#define USESHADOWMAPPCF 1\n", " shadowmappcf"},
	{"#define USESHADOWMAPPCF 2\n", " shadowmappcf2"},
	{"#define USESHADOWSAMPLER\n", " shadowsampler"},
	{"#define USESHADOWMAPVSDCT\n", " shadowmapvsdct"},
	{"#define USESHADOWMAPORTHO\n", " shadowmaportho"},
	{"#define USEDEFERREDLIGHTMAP\n", " deferredlightmap"},
	{"#define USEALPHAKILL\n", " alphakill"},
	{"#define USEREFLECTCUBE\n", " reflectcube"},
};

/// this enum is multiplied by SHADERPERMUTATION_MODEBASE
typedef enum shadermode_e
{
	SHADERMODE_GENERIC, ///< (particles/HUD/etc) vertex color, optionally multiplied by one texture
	SHADERMODE_POSTPROCESS, ///< postprocessing shader (r_glsl_postprocess)
	SHADERMODE_DEPTH_OR_SHADOW, ///< (depthfirst/shadows) vertex shader only
	SHADERMODE_FLATCOLOR, ///< (lightmap) modulate texture by uniform color (q1bsp, q3bsp)
	SHADERMODE_VERTEXCOLOR, ///< (lightmap) modulate texture by vertex colors (q3bsp)
	SHADERMODE_LIGHTMAP, ///< (lightmap) modulate texture by lightmap texture (q1bsp, q3bsp)
	SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE, ///< (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
	SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE, ///< (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
	SHADERMODE_LIGHTDIRECTION, ///< (lightmap) use directional pixel shading from fixed light direction (q3bsp)
	SHADERMODE_LIGHTSOURCE, ///< (lightsource) use directional pixel shading from light source (rtlight)
	SHADERMODE_REFRACTION, ///< refract background (the material is rendered normally after this pass)
	SHADERMODE_WATER, ///< refract background and reflection (the material is rendered normally after this pass)
	SHADERMODE_SHOWDEPTH, ///< (debugging) renders depth as color
	SHADERMODE_DEFERREDGEOMETRY, ///< (deferred) render material properties to screenspace geometry buffers
	SHADERMODE_DEFERREDLIGHTSOURCE, ///< (deferred) use directional pixel shading from light source (rtlight) on screenspace geometry buffers
	SHADERMODE_COUNT
}
shadermode_t;

// NOTE: MUST MATCH ORDER OF SHADERMODE_* ENUMS!
shadermodeinfo_t glslshadermodeinfo[SHADERMODE_COUNT] =
{
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_GENERIC\n", " generic"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_POSTPROCESS\n", " postprocess"},
	{"glsl/default.glsl", NULL, NULL               , "#define MODE_DEPTH_OR_SHADOW\n", " depth/shadow"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_FLATCOLOR\n", " flatcolor"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_VERTEXCOLOR\n", " vertexcolor"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTMAP\n", " lightmap"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n", " lightdirectionmap_modelspace"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n", " lightdirectionmap_tangentspace"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTDIRECTION\n", " lightdirection"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTSOURCE\n", " lightsource"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_REFRACTION\n", " refraction"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_WATER\n", " water"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_SHOWDEPTH\n", " showdepth"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_DEFERREDGEOMETRY\n", " deferredgeometry"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_DEFERREDLIGHTSOURCE\n", " deferredlightsource"},
};

#ifdef SUPPORTCG
shadermodeinfo_t cgshadermodeinfo[SHADERMODE_COUNT] =
{
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_GENERIC\n", " generic"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_POSTPROCESS\n", " postprocess"},
	{"cg/default.cg", NULL, NULL           , "#define MODE_DEPTH_OR_SHADOW\n", " depth"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_FLATCOLOR\n", " flatcolor"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_VERTEXCOLOR\n", " vertexcolor"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_LIGHTMAP\n", " lightmap"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n", " lightdirectionmap_modelspace"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n", " lightdirectionmap_tangentspace"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_LIGHTDIRECTION\n", " lightdirection"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_LIGHTSOURCE\n", " lightsource"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_REFRACTION\n", " refraction"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_WATER\n", " water"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_SHOWDEPTH\n", " showdepth"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_DEFERREDGEOMETRY\n", " deferredgeometry"},
	{"cg/default.cg", NULL, "cg/default.cg", "#define MODE_DEFERREDLIGHTSOURCE\n", " deferredlightsource"},
};
#endif

struct r_glsl_permutation_s;
typedef struct r_glsl_permutation_s
{
	/// hash lookup data
	struct r_glsl_permutation_s *hashnext;
	unsigned int mode;
	unsigned int permutation;

	/// indicates if we have tried compiling this permutation already
	qboolean compiled;
	/// 0 if compilation failed
	int program;
	/// locations of detected uniforms in program object, or -1 if not found
	int loc_Texture_First;
	int loc_Texture_Second;
	int loc_Texture_GammaRamps;
	int loc_Texture_Normal;
	int loc_Texture_Color;
	int loc_Texture_Gloss;
	int loc_Texture_Glow;
	int loc_Texture_SecondaryNormal;
	int loc_Texture_SecondaryColor;
	int loc_Texture_SecondaryGloss;
	int loc_Texture_SecondaryGlow;
	int loc_Texture_Pants;
	int loc_Texture_Shirt;
	int loc_Texture_FogMask;
	int loc_Texture_Lightmap;
	int loc_Texture_Deluxemap;
	int loc_Texture_Attenuation;
	int loc_Texture_Cube;
	int loc_Texture_Refraction;
	int loc_Texture_Reflection;
	int loc_Texture_ShadowMapRect;
	int loc_Texture_ShadowMapCube;
	int loc_Texture_ShadowMap2D;
	int loc_Texture_CubeProjection;
	int loc_Texture_ScreenDepth;
	int loc_Texture_ScreenNormalMap;
	int loc_Texture_ScreenDiffuse;
	int loc_Texture_ScreenSpecular;
	int loc_Texture_ReflectMask;
	int loc_Texture_ReflectCube;
	int loc_Alpha;
	int loc_BloomBlur_Parameters;
	int loc_ClientTime;
	int loc_Color_Ambient;
	int loc_Color_Diffuse;
	int loc_Color_Specular;
	int loc_Color_Glow;
	int loc_Color_Pants;
	int loc_Color_Shirt;
	int loc_DeferredColor_Ambient;
	int loc_DeferredColor_Diffuse;
	int loc_DeferredColor_Specular;
	int loc_DeferredMod_Diffuse;
	int loc_DeferredMod_Specular;
	int loc_DistortScaleRefractReflect;
	int loc_EyePosition;
	int loc_FogColor;
	int loc_FogHeightFade;
	int loc_FogPlane;
	int loc_FogPlaneViewDist;
	int loc_FogRangeRecip;
	int loc_LightColor;
	int loc_LightDir;
	int loc_LightPosition;
	int loc_OffsetMapping_Scale;
	int loc_PixelSize;
	int loc_ReflectColor;
	int loc_ReflectFactor;
	int loc_ReflectOffset;
	int loc_RefractColor;
	int loc_Saturation;
	int loc_ScreenCenterRefractReflect;
	int loc_ScreenScaleRefractReflect;
	int loc_ScreenToDepth;
	int loc_ShadowMap_Parameters;
	int loc_ShadowMap_TextureScale;
	int loc_SpecularPower;
	int loc_UserVec1;
	int loc_UserVec2;
	int loc_UserVec3;
	int loc_UserVec4;
	int loc_ViewTintColor;
	int loc_ViewToLight;
	int loc_ModelToLight;
	int loc_TexMatrix;
	int loc_BackgroundTexMatrix;
	int loc_ModelViewProjectionMatrix;
	int loc_ModelViewMatrix;
	int loc_PixelToScreenTexCoord;
	int loc_ModelToReflectCube;
	int loc_ShadowMapMatrix;	
}
r_glsl_permutation_t;

#define SHADERPERMUTATION_HASHSIZE 256

/// information about each possible shader permutation
r_glsl_permutation_t *r_glsl_permutationhash[SHADERMODE_COUNT][SHADERPERMUTATION_HASHSIZE];
/// currently selected permutation
r_glsl_permutation_t *r_glsl_permutation;
/// storage for permutations linked in the hash table
memexpandablearray_t r_glsl_permutationarray;

static r_glsl_permutation_t *R_GLSL_FindPermutation(unsigned int mode, unsigned int permutation)
{
	//unsigned int hashdepth = 0;
	unsigned int hashindex = (permutation * 0x1021) & (SHADERPERMUTATION_HASHSIZE - 1);
	r_glsl_permutation_t *p;
	for (p = r_glsl_permutationhash[mode][hashindex];p;p = p->hashnext)
	{
		if (p->mode == mode && p->permutation == permutation)
		{
			//if (hashdepth > 10)
			//	Con_Printf("R_GLSL_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
			return p;
		}
		//hashdepth++;
	}
	p = (r_glsl_permutation_t*)Mem_ExpandableArray_AllocRecord(&r_glsl_permutationarray);
	p->mode = mode;
	p->permutation = permutation;
	p->hashnext = r_glsl_permutationhash[mode][hashindex];
	r_glsl_permutationhash[mode][hashindex] = p;
	//if (hashdepth > 10)
	//	Con_Printf("R_GLSL_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
	return p;
}

static char *R_GLSL_GetText(const char *filename, qboolean printfromdisknotice)
{
	char *shaderstring;
	if (!filename || !filename[0])
		return NULL;
	if (!strcmp(filename, "glsl/default.glsl"))
	{
		if (!glslshaderstring)
		{
			glslshaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
			if (glslshaderstring)
				Con_DPrintf("Loading shaders from file %s...\n", filename);
			else
				glslshaderstring = (char *)builtinshaderstring;
		}
		shaderstring = (char *) Mem_Alloc(r_main_mempool, strlen(glslshaderstring) + 1);
		memcpy(shaderstring, glslshaderstring, strlen(glslshaderstring) + 1);
		return shaderstring;
	}
	shaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
	if (shaderstring)
	{
		if (printfromdisknotice)
			Con_DPrintf("from disk %s... ", filename);
		return shaderstring;
	}
	return shaderstring;
}

static void R_GLSL_CompilePermutation(r_glsl_permutation_t *p, unsigned int mode, unsigned int permutation)
{
	int i;
	shadermodeinfo_t *modeinfo = glslshadermodeinfo + mode;
	int vertstrings_count = 0;
	int geomstrings_count = 0;
	int fragstrings_count = 0;
	char *vertexstring, *geometrystring, *fragmentstring;
	const char *vertstrings_list[32+3];
	const char *geomstrings_list[32+3];
	const char *fragstrings_list[32+3];
	char permutationname[256];

	if (p->compiled)
		return;
	p->compiled = true;
	p->program = 0;

	permutationname[0] = 0;
	vertexstring   = R_GLSL_GetText(modeinfo->vertexfilename, true);
	geometrystring = R_GLSL_GetText(modeinfo->geometryfilename, false);
	fragmentstring = R_GLSL_GetText(modeinfo->fragmentfilename, false);

	strlcat(permutationname, modeinfo->vertexfilename, sizeof(permutationname));

	// the first pretext is which type of shader to compile as
	// (later these will all be bound together as a program object)
	vertstrings_list[vertstrings_count++] = "#define VERTEX_SHADER\n";
	geomstrings_list[geomstrings_count++] = "#define GEOMETRY_SHADER\n";
	fragstrings_list[fragstrings_count++] = "#define FRAGMENT_SHADER\n";

	// the second pretext is the mode (for example a light source)
	vertstrings_list[vertstrings_count++] = modeinfo->pretext;
	geomstrings_list[geomstrings_count++] = modeinfo->pretext;
	fragstrings_list[fragstrings_count++] = modeinfo->pretext;
	strlcat(permutationname, modeinfo->name, sizeof(permutationname));

	// now add all the permutation pretexts
	for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
	{
		if (permutation & (1<<i))
		{
			vertstrings_list[vertstrings_count++] = shaderpermutationinfo[i].pretext;
			geomstrings_list[geomstrings_count++] = shaderpermutationinfo[i].pretext;
			fragstrings_list[fragstrings_count++] = shaderpermutationinfo[i].pretext;
			strlcat(permutationname, shaderpermutationinfo[i].name, sizeof(permutationname));
		}
		else
		{
			// keep line numbers correct
			vertstrings_list[vertstrings_count++] = "\n";
			geomstrings_list[geomstrings_count++] = "\n";
			fragstrings_list[fragstrings_count++] = "\n";
		}
	}

	// now append the shader text itself
	vertstrings_list[vertstrings_count++] = vertexstring;
	geomstrings_list[geomstrings_count++] = geometrystring;
	fragstrings_list[fragstrings_count++] = fragmentstring;

	// if any sources were NULL, clear the respective list
	if (!vertexstring)
		vertstrings_count = 0;
	if (!geometrystring)
		geomstrings_count = 0;
	if (!fragmentstring)
		fragstrings_count = 0;

	// compile the shader program
	if (vertstrings_count + geomstrings_count + fragstrings_count)
		p->program = GL_Backend_CompileProgram(vertstrings_count, vertstrings_list, geomstrings_count, geomstrings_list, fragstrings_count, fragstrings_list);
	if (p->program)
	{
		CHECKGLERROR
		qglUseProgramObjectARB(p->program);CHECKGLERROR
		// look up all the uniform variable names we care about, so we don't
		// have to look them up every time we set them

		p->loc_Texture_First              = qglGetUniformLocationARB(p->program, "Texture_First");
		p->loc_Texture_Second             = qglGetUniformLocationARB(p->program, "Texture_Second");
		p->loc_Texture_GammaRamps         = qglGetUniformLocationARB(p->program, "Texture_GammaRamps");
		p->loc_Texture_Normal             = qglGetUniformLocationARB(p->program, "Texture_Normal");
		p->loc_Texture_Color              = qglGetUniformLocationARB(p->program, "Texture_Color");
		p->loc_Texture_Gloss              = qglGetUniformLocationARB(p->program, "Texture_Gloss");
		p->loc_Texture_Glow               = qglGetUniformLocationARB(p->program, "Texture_Glow");
		p->loc_Texture_SecondaryNormal    = qglGetUniformLocationARB(p->program, "Texture_SecondaryNormal");
		p->loc_Texture_SecondaryColor     = qglGetUniformLocationARB(p->program, "Texture_SecondaryColor");
		p->loc_Texture_SecondaryGloss     = qglGetUniformLocationARB(p->program, "Texture_SecondaryGloss");
		p->loc_Texture_SecondaryGlow      = qglGetUniformLocationARB(p->program, "Texture_SecondaryGlow");
		p->loc_Texture_Pants              = qglGetUniformLocationARB(p->program, "Texture_Pants");
		p->loc_Texture_Shirt              = qglGetUniformLocationARB(p->program, "Texture_Shirt");
		p->loc_Texture_FogMask            = qglGetUniformLocationARB(p->program, "Texture_FogMask");
		p->loc_Texture_Lightmap           = qglGetUniformLocationARB(p->program, "Texture_Lightmap");
		p->loc_Texture_Deluxemap          = qglGetUniformLocationARB(p->program, "Texture_Deluxemap");
		p->loc_Texture_Attenuation        = qglGetUniformLocationARB(p->program, "Texture_Attenuation");
		p->loc_Texture_Cube               = qglGetUniformLocationARB(p->program, "Texture_Cube");
		p->loc_Texture_Refraction         = qglGetUniformLocationARB(p->program, "Texture_Refraction");
		p->loc_Texture_Reflection         = qglGetUniformLocationARB(p->program, "Texture_Reflection");
		p->loc_Texture_ShadowMapRect      = qglGetUniformLocationARB(p->program, "Texture_ShadowMapRect");
		p->loc_Texture_ShadowMapCube      = qglGetUniformLocationARB(p->program, "Texture_ShadowMapCube");
		p->loc_Texture_ShadowMap2D        = qglGetUniformLocationARB(p->program, "Texture_ShadowMap2D");
		p->loc_Texture_CubeProjection     = qglGetUniformLocationARB(p->program, "Texture_CubeProjection");
		p->loc_Texture_ScreenDepth        = qglGetUniformLocationARB(p->program, "Texture_ScreenDepth");
		p->loc_Texture_ScreenNormalMap    = qglGetUniformLocationARB(p->program, "Texture_ScreenNormalMap");
		p->loc_Texture_ScreenDiffuse      = qglGetUniformLocationARB(p->program, "Texture_ScreenDiffuse");
		p->loc_Texture_ScreenSpecular     = qglGetUniformLocationARB(p->program, "Texture_ScreenSpecular");
		p->loc_Texture_ReflectMask        = qglGetUniformLocationARB(p->program, "Texture_ReflectMask");
		p->loc_Texture_ReflectCube        = qglGetUniformLocationARB(p->program, "Texture_ReflectCube");
		p->loc_Alpha                      = qglGetUniformLocationARB(p->program, "Alpha");
		p->loc_BloomBlur_Parameters       = qglGetUniformLocationARB(p->program, "BloomBlur_Parameters");
		p->loc_ClientTime                 = qglGetUniformLocationARB(p->program, "ClientTime");
		p->loc_Color_Ambient              = qglGetUniformLocationARB(p->program, "Color_Ambient");
		p->loc_Color_Diffuse              = qglGetUniformLocationARB(p->program, "Color_Diffuse");
		p->loc_Color_Specular             = qglGetUniformLocationARB(p->program, "Color_Specular");
		p->loc_Color_Glow                 = qglGetUniformLocationARB(p->program, "Color_Glow");
		p->loc_Color_Pants                = qglGetUniformLocationARB(p->program, "Color_Pants");
		p->loc_Color_Shirt                = qglGetUniformLocationARB(p->program, "Color_Shirt");
		p->loc_DeferredColor_Ambient      = qglGetUniformLocationARB(p->program, "DeferredColor_Ambient");
		p->loc_DeferredColor_Diffuse      = qglGetUniformLocationARB(p->program, "DeferredColor_Diffuse");
		p->loc_DeferredColor_Specular     = qglGetUniformLocationARB(p->program, "DeferredColor_Specular");
		p->loc_DeferredMod_Diffuse        = qglGetUniformLocationARB(p->program, "DeferredMod_Diffuse");
		p->loc_DeferredMod_Specular       = qglGetUniformLocationARB(p->program, "DeferredMod_Specular");
		p->loc_DistortScaleRefractReflect = qglGetUniformLocationARB(p->program, "DistortScaleRefractReflect");
		p->loc_EyePosition                = qglGetUniformLocationARB(p->program, "EyePosition");
		p->loc_FogColor                   = qglGetUniformLocationARB(p->program, "FogColor");
		p->loc_FogHeightFade              = qglGetUniformLocationARB(p->program, "FogHeightFade");
		p->loc_FogPlane                   = qglGetUniformLocationARB(p->program, "FogPlane");
		p->loc_FogPlaneViewDist           = qglGetUniformLocationARB(p->program, "FogPlaneViewDist");
		p->loc_FogRangeRecip              = qglGetUniformLocationARB(p->program, "FogRangeRecip");
		p->loc_LightColor                 = qglGetUniformLocationARB(p->program, "LightColor");
		p->loc_LightDir                   = qglGetUniformLocationARB(p->program, "LightDir");
		p->loc_LightPosition              = qglGetUniformLocationARB(p->program, "LightPosition");
		p->loc_OffsetMapping_Scale        = qglGetUniformLocationARB(p->program, "OffsetMapping_Scale");
		p->loc_PixelSize                  = qglGetUniformLocationARB(p->program, "PixelSize");
		p->loc_ReflectColor               = qglGetUniformLocationARB(p->program, "ReflectColor");
		p->loc_ReflectFactor              = qglGetUniformLocationARB(p->program, "ReflectFactor");
		p->loc_ReflectOffset              = qglGetUniformLocationARB(p->program, "ReflectOffset");
		p->loc_RefractColor               = qglGetUniformLocationARB(p->program, "RefractColor");
		p->loc_Saturation                 = qglGetUniformLocationARB(p->program, "Saturation");
		p->loc_ScreenCenterRefractReflect = qglGetUniformLocationARB(p->program, "ScreenCenterRefractReflect");
		p->loc_ScreenScaleRefractReflect  = qglGetUniformLocationARB(p->program, "ScreenScaleRefractReflect");
		p->loc_ScreenToDepth              = qglGetUniformLocationARB(p->program, "ScreenToDepth");
		p->loc_ShadowMap_Parameters       = qglGetUniformLocationARB(p->program, "ShadowMap_Parameters");
		p->loc_ShadowMap_TextureScale     = qglGetUniformLocationARB(p->program, "ShadowMap_TextureScale");
		p->loc_SpecularPower              = qglGetUniformLocationARB(p->program, "SpecularPower");
		p->loc_UserVec1                   = qglGetUniformLocationARB(p->program, "UserVec1");
		p->loc_UserVec2                   = qglGetUniformLocationARB(p->program, "UserVec2");
		p->loc_UserVec3                   = qglGetUniformLocationARB(p->program, "UserVec3");
		p->loc_UserVec4                   = qglGetUniformLocationARB(p->program, "UserVec4");
		p->loc_ViewTintColor              = qglGetUniformLocationARB(p->program, "ViewTintColor");
		p->loc_ViewToLight                = qglGetUniformLocationARB(p->program, "ViewToLight");
		p->loc_ModelToLight               = qglGetUniformLocationARB(p->program, "ModelToLight");
		p->loc_TexMatrix                  = qglGetUniformLocationARB(p->program, "TexMatrix");
		p->loc_BackgroundTexMatrix        = qglGetUniformLocationARB(p->program, "BackgroundTexMatrix");
		p->loc_ModelViewMatrix            = qglGetUniformLocationARB(p->program, "ModelViewMatrix");
		p->loc_ModelViewProjectionMatrix  = qglGetUniformLocationARB(p->program, "ModelViewProjectionMatrix");
		p->loc_PixelToScreenTexCoord      = qglGetUniformLocationARB(p->program, "PixelToScreenTexCoord");
		p->loc_ModelToReflectCube         = qglGetUniformLocationARB(p->program, "ModelToReflectCube");
		p->loc_ShadowMapMatrix            = qglGetUniformLocationARB(p->program, "ShadowMapMatrix");		
		// initialize the samplers to refer to the texture units we use
		if (p->loc_Texture_First           >= 0) qglUniform1iARB(p->loc_Texture_First          , GL20TU_FIRST);
		if (p->loc_Texture_Second          >= 0) qglUniform1iARB(p->loc_Texture_Second         , GL20TU_SECOND);
		if (p->loc_Texture_GammaRamps      >= 0) qglUniform1iARB(p->loc_Texture_GammaRamps     , GL20TU_GAMMARAMPS);
		if (p->loc_Texture_Normal          >= 0) qglUniform1iARB(p->loc_Texture_Normal         , GL20TU_NORMAL);
		if (p->loc_Texture_Color           >= 0) qglUniform1iARB(p->loc_Texture_Color          , GL20TU_COLOR);
		if (p->loc_Texture_Gloss           >= 0) qglUniform1iARB(p->loc_Texture_Gloss          , GL20TU_GLOSS);
		if (p->loc_Texture_Glow            >= 0) qglUniform1iARB(p->loc_Texture_Glow           , GL20TU_GLOW);
		if (p->loc_Texture_SecondaryNormal >= 0) qglUniform1iARB(p->loc_Texture_SecondaryNormal, GL20TU_SECONDARY_NORMAL);
		if (p->loc_Texture_SecondaryColor  >= 0) qglUniform1iARB(p->loc_Texture_SecondaryColor , GL20TU_SECONDARY_COLOR);
		if (p->loc_Texture_SecondaryGloss  >= 0) qglUniform1iARB(p->loc_Texture_SecondaryGloss , GL20TU_SECONDARY_GLOSS);
		if (p->loc_Texture_SecondaryGlow   >= 0) qglUniform1iARB(p->loc_Texture_SecondaryGlow  , GL20TU_SECONDARY_GLOW);
		if (p->loc_Texture_Pants           >= 0) qglUniform1iARB(p->loc_Texture_Pants          , GL20TU_PANTS);
		if (p->loc_Texture_Shirt           >= 0) qglUniform1iARB(p->loc_Texture_Shirt          , GL20TU_SHIRT);
		if (p->loc_Texture_FogMask         >= 0) qglUniform1iARB(p->loc_Texture_FogMask        , GL20TU_FOGMASK);
		if (p->loc_Texture_Lightmap        >= 0) qglUniform1iARB(p->loc_Texture_Lightmap       , GL20TU_LIGHTMAP);
		if (p->loc_Texture_Deluxemap       >= 0) qglUniform1iARB(p->loc_Texture_Deluxemap      , GL20TU_DELUXEMAP);
		if (p->loc_Texture_Attenuation     >= 0) qglUniform1iARB(p->loc_Texture_Attenuation    , GL20TU_ATTENUATION);
		if (p->loc_Texture_Cube            >= 0) qglUniform1iARB(p->loc_Texture_Cube           , GL20TU_CUBE);
		if (p->loc_Texture_Refraction      >= 0) qglUniform1iARB(p->loc_Texture_Refraction     , GL20TU_REFRACTION);
		if (p->loc_Texture_Reflection      >= 0) qglUniform1iARB(p->loc_Texture_Reflection     , GL20TU_REFLECTION);
		if (p->loc_Texture_ShadowMapRect   >= 0) qglUniform1iARB(p->loc_Texture_ShadowMapRect  , permutation & SHADERPERMUTATION_SHADOWMAPORTHO ? GL20TU_SHADOWMAPORTHORECT : GL20TU_SHADOWMAPRECT);
		if (p->loc_Texture_ShadowMapCube   >= 0) qglUniform1iARB(p->loc_Texture_ShadowMapCube  , GL20TU_SHADOWMAPCUBE);
		if (p->loc_Texture_ShadowMap2D     >= 0) qglUniform1iARB(p->loc_Texture_ShadowMap2D    , permutation & SHADERPERMUTATION_SHADOWMAPORTHO ? GL20TU_SHADOWMAPORTHO2D : GL20TU_SHADOWMAP2D);
		if (p->loc_Texture_CubeProjection  >= 0) qglUniform1iARB(p->loc_Texture_CubeProjection , GL20TU_CUBEPROJECTION);
		if (p->loc_Texture_ScreenDepth     >= 0) qglUniform1iARB(p->loc_Texture_ScreenDepth    , GL20TU_SCREENDEPTH);
		if (p->loc_Texture_ScreenNormalMap >= 0) qglUniform1iARB(p->loc_Texture_ScreenNormalMap, GL20TU_SCREENNORMALMAP);
		if (p->loc_Texture_ScreenDiffuse   >= 0) qglUniform1iARB(p->loc_Texture_ScreenDiffuse  , GL20TU_SCREENDIFFUSE);
		if (p->loc_Texture_ScreenSpecular  >= 0) qglUniform1iARB(p->loc_Texture_ScreenSpecular , GL20TU_SCREENSPECULAR);
		if (p->loc_Texture_ReflectMask     >= 0) qglUniform1iARB(p->loc_Texture_ReflectMask    , GL20TU_REFLECTMASK);
		if (p->loc_Texture_ReflectCube     >= 0) qglUniform1iARB(p->loc_Texture_ReflectCube    , GL20TU_REFLECTCUBE);
		CHECKGLERROR
		Con_DPrintf("^5GLSL shader %s compiled.\n", permutationname);
	}
	else
		Con_Printf("^1GLSL shader %s failed!  some features may not work properly.\n", permutationname);

	// free the strings
	if (vertexstring)
		Mem_Free(vertexstring);
	if (geometrystring)
		Mem_Free(geometrystring);
	if (fragmentstring)
		Mem_Free(fragmentstring);
}

void R_SetupShader_SetPermutationGLSL(unsigned int mode, unsigned int permutation)
{
	r_glsl_permutation_t *perm = R_GLSL_FindPermutation(mode, permutation);
	if (r_glsl_permutation != perm)
	{
		r_glsl_permutation = perm;
		if (!r_glsl_permutation->program)
		{
			if (!r_glsl_permutation->compiled)
				R_GLSL_CompilePermutation(perm, mode, permutation);
			if (!r_glsl_permutation->program)
			{
				// remove features until we find a valid permutation
				int i;
				for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
				{
					// reduce i more quickly whenever it would not remove any bits
					int j = 1<<(SHADERPERMUTATION_COUNT-1-i);
					if (!(permutation & j))
						continue;
					permutation -= j;
					r_glsl_permutation = R_GLSL_FindPermutation(mode, permutation);
					if (!r_glsl_permutation->compiled)
						R_GLSL_CompilePermutation(perm, mode, permutation);
					if (r_glsl_permutation->program)
						break;
				}
				if (i >= SHADERPERMUTATION_COUNT)
				{
					//Con_Printf("Could not find a working OpenGL 2.0 shader for permutation %s %s\n", shadermodeinfo[mode].vertexfilename, shadermodeinfo[mode].pretext);
					r_glsl_permutation = R_GLSL_FindPermutation(mode, permutation);
					qglUseProgramObjectARB(0);CHECKGLERROR
					return; // no bit left to clear, entire mode is broken
				}
			}
		}
		CHECKGLERROR
		qglUseProgramObjectARB(r_glsl_permutation->program);CHECKGLERROR
	}
	if (r_glsl_permutation->loc_ModelViewProjectionMatrix >= 0) qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelViewProjectionMatrix, 1, false, gl_modelviewprojection16f);
	if (r_glsl_permutation->loc_ModelViewMatrix >= 0) qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelViewMatrix, 1, false, gl_modelview16f);
	if (r_glsl_permutation->loc_ClientTime >= 0) qglUniform1fARB(r_glsl_permutation->loc_ClientTime, cl.time);
}

#ifdef SUPPORTCG
#include <Cg/cgGL.h>
struct r_cg_permutation_s;
typedef struct r_cg_permutation_s
{
	/// hash lookup data
	struct r_cg_permutation_s *hashnext;
	unsigned int mode;
	unsigned int permutation;

	/// indicates if we have tried compiling this permutation already
	qboolean compiled;
	/// 0 if compilation failed
	CGprogram vprogram;
	CGprogram fprogram;
	/// locations of detected parameters in programs, or NULL if not found
	CGparameter vp_EyePosition;
	CGparameter vp_FogPlane;
	CGparameter vp_LightDir;
	CGparameter vp_LightPosition;
	CGparameter vp_ModelToLight;
	CGparameter vp_TexMatrix;
	CGparameter vp_BackgroundTexMatrix;
	CGparameter vp_ModelViewProjectionMatrix;
	CGparameter vp_ModelViewMatrix;
	CGparameter vp_ShadowMapMatrix;

	CGparameter fp_Texture_First;
	CGparameter fp_Texture_Second;
	CGparameter fp_Texture_GammaRamps;
	CGparameter fp_Texture_Normal;
	CGparameter fp_Texture_Color;
	CGparameter fp_Texture_Gloss;
	CGparameter fp_Texture_Glow;
	CGparameter fp_Texture_SecondaryNormal;
	CGparameter fp_Texture_SecondaryColor;
	CGparameter fp_Texture_SecondaryGloss;
	CGparameter fp_Texture_SecondaryGlow;
	CGparameter fp_Texture_Pants;
	CGparameter fp_Texture_Shirt;
	CGparameter fp_Texture_FogMask;
	CGparameter fp_Texture_Lightmap;
	CGparameter fp_Texture_Deluxemap;
	CGparameter fp_Texture_Attenuation;
	CGparameter fp_Texture_Cube;
	CGparameter fp_Texture_Refraction;
	CGparameter fp_Texture_Reflection;
	CGparameter fp_Texture_ShadowMapRect;
	CGparameter fp_Texture_ShadowMapCube;
	CGparameter fp_Texture_ShadowMap2D;
	CGparameter fp_Texture_CubeProjection;
	CGparameter fp_Texture_ScreenDepth;
	CGparameter fp_Texture_ScreenNormalMap;
	CGparameter fp_Texture_ScreenDiffuse;
	CGparameter fp_Texture_ScreenSpecular;
	CGparameter fp_Texture_ReflectMask;
	CGparameter fp_Texture_ReflectCube;
	CGparameter fp_Alpha;
	CGparameter fp_BloomBlur_Parameters;
	CGparameter fp_ClientTime;
	CGparameter fp_Color_Ambient;
	CGparameter fp_Color_Diffuse;
	CGparameter fp_Color_Specular;
	CGparameter fp_Color_Glow;
	CGparameter fp_Color_Pants;
	CGparameter fp_Color_Shirt;
	CGparameter fp_DeferredColor_Ambient;
	CGparameter fp_DeferredColor_Diffuse;
	CGparameter fp_DeferredColor_Specular;
	CGparameter fp_DeferredMod_Diffuse;
	CGparameter fp_DeferredMod_Specular;
	CGparameter fp_DistortScaleRefractReflect;
	CGparameter fp_EyePosition;
	CGparameter fp_FogColor;
	CGparameter fp_FogHeightFade;
	CGparameter fp_FogPlane;
	CGparameter fp_FogPlaneViewDist;
	CGparameter fp_FogRangeRecip;
	CGparameter fp_LightColor;
	CGparameter fp_LightDir;
	CGparameter fp_LightPosition;
	CGparameter fp_OffsetMapping_Scale;
	CGparameter fp_PixelSize;
	CGparameter fp_ReflectColor;
	CGparameter fp_ReflectFactor;
	CGparameter fp_ReflectOffset;
	CGparameter fp_RefractColor;
	CGparameter fp_Saturation;
	CGparameter fp_ScreenCenterRefractReflect;
	CGparameter fp_ScreenScaleRefractReflect;
	CGparameter fp_ScreenToDepth;
	CGparameter fp_ShadowMap_Parameters;
	CGparameter fp_ShadowMap_TextureScale;
	CGparameter fp_SpecularPower;
	CGparameter fp_UserVec1;
	CGparameter fp_UserVec2;
	CGparameter fp_UserVec3;
	CGparameter fp_UserVec4;
	CGparameter fp_ViewTintColor;
	CGparameter fp_ViewToLight;
	CGparameter fp_PixelToScreenTexCoord;
	CGparameter fp_ModelToReflectCube;
}
r_cg_permutation_t;

/// information about each possible shader permutation
r_cg_permutation_t *r_cg_permutationhash[SHADERMODE_COUNT][SHADERPERMUTATION_HASHSIZE];
/// currently selected permutation
r_cg_permutation_t *r_cg_permutation;
/// storage for permutations linked in the hash table
memexpandablearray_t r_cg_permutationarray;

#define CHECKCGERROR {CGerror err = cgGetError(), err2 = err;if (err){Con_Printf("%s:%i CG error %i: %s : %s\n", __FILE__, __LINE__, err, cgGetErrorString(err), cgGetLastErrorString(&err2));if (err == 1) Con_Printf("last listing:\n%s\n", cgGetLastListing(vid.cgcontext));}}

static r_cg_permutation_t *R_CG_FindPermutation(unsigned int mode, unsigned int permutation)
{
	//unsigned int hashdepth = 0;
	unsigned int hashindex = (permutation * 0x1021) & (SHADERPERMUTATION_HASHSIZE - 1);
	r_cg_permutation_t *p;
	for (p = r_cg_permutationhash[mode][hashindex];p;p = p->hashnext)
	{
		if (p->mode == mode && p->permutation == permutation)
		{
			//if (hashdepth > 10)
			//	Con_Printf("R_CG_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
			return p;
		}
		//hashdepth++;
	}
	p = (r_cg_permutation_t*)Mem_ExpandableArray_AllocRecord(&r_cg_permutationarray);
	p->mode = mode;
	p->permutation = permutation;
	p->hashnext = r_cg_permutationhash[mode][hashindex];
	r_cg_permutationhash[mode][hashindex] = p;
	//if (hashdepth > 10)
	//	Con_Printf("R_CG_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
	return p;
}

static char *R_CG_GetText(const char *filename, qboolean printfromdisknotice)
{
	char *shaderstring;
	if (!filename || !filename[0])
		return NULL;
	if (!strcmp(filename, "cg/default.cg"))
	{
		if (!cgshaderstring)
		{
			cgshaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
			if (cgshaderstring)
				Con_DPrintf("Loading shaders from file %s...\n", filename);
			else
				cgshaderstring = (char *)builtincgshaderstring;
		}
		shaderstring = (char *) Mem_Alloc(r_main_mempool, strlen(cgshaderstring) + 1);
		memcpy(shaderstring, cgshaderstring, strlen(cgshaderstring) + 1);
		return shaderstring;
	}
	shaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
	if (shaderstring)
	{
		if (printfromdisknotice)
			Con_DPrintf("from disk %s... ", filename);
		return shaderstring;
	}
	return shaderstring;
}

static void R_CG_CacheShader(r_cg_permutation_t *p, const char *cachename, const char *vertstring, const char *fragstring)
{
	// TODO: load or create .fp and .vp shader files
}

static void R_CG_CompilePermutation(r_cg_permutation_t *p, unsigned int mode, unsigned int permutation)
{
	int i;
	shadermodeinfo_t *modeinfo = cgshadermodeinfo + mode;
	int vertstrings_count = 0, vertstring_length = 0;
	int geomstrings_count = 0, geomstring_length = 0;
	int fragstrings_count = 0, fragstring_length = 0;
	char *t;
	char *vertexstring, *geometrystring, *fragmentstring;
	char *vertstring, *geomstring, *fragstring;
	const char *vertstrings_list[32+3];
	const char *geomstrings_list[32+3];
	const char *fragstrings_list[32+3];
	char permutationname[256];
	char cachename[256];
	CGprofile vertexProfile;
	CGprofile fragmentProfile;

	if (p->compiled)
		return;
	p->compiled = true;
	p->vprogram = NULL;
	p->fprogram = NULL;

	permutationname[0] = 0;
	cachename[0] = 0;
	vertexstring   = R_CG_GetText(modeinfo->vertexfilename, true);
	geometrystring = R_CG_GetText(modeinfo->geometryfilename, false);
	fragmentstring = R_CG_GetText(modeinfo->fragmentfilename, false);

	strlcat(permutationname, modeinfo->vertexfilename, sizeof(permutationname));
	strlcat(cachename, "cg/", sizeof(cachename));

	// the first pretext is which type of shader to compile as
	// (later these will all be bound together as a program object)
	vertstrings_list[vertstrings_count++] = "#define VERTEX_SHADER\n";
	geomstrings_list[geomstrings_count++] = "#define GEOMETRY_SHADER\n";
	fragstrings_list[fragstrings_count++] = "#define FRAGMENT_SHADER\n";

	// the second pretext is the mode (for example a light source)
	vertstrings_list[vertstrings_count++] = modeinfo->pretext;
	geomstrings_list[geomstrings_count++] = modeinfo->pretext;
	fragstrings_list[fragstrings_count++] = modeinfo->pretext;
	strlcat(permutationname, modeinfo->name, sizeof(permutationname));
	strlcat(cachename, modeinfo->name, sizeof(cachename));

	// now add all the permutation pretexts
	for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
	{
		if (permutation & (1<<i))
		{
			vertstrings_list[vertstrings_count++] = shaderpermutationinfo[i].pretext;
			geomstrings_list[geomstrings_count++] = shaderpermutationinfo[i].pretext;
			fragstrings_list[fragstrings_count++] = shaderpermutationinfo[i].pretext;
			strlcat(permutationname, shaderpermutationinfo[i].name, sizeof(permutationname));
			strlcat(cachename, shaderpermutationinfo[i].name, sizeof(cachename));
		}
		else
		{
			// keep line numbers correct
			vertstrings_list[vertstrings_count++] = "\n";
			geomstrings_list[geomstrings_count++] = "\n";
			fragstrings_list[fragstrings_count++] = "\n";
		}
	}

	// replace spaces in the cachename with _ characters
	for (i = 0;cachename[i];i++)
		if (cachename[i] == ' ')
			cachename[i] = '_';

	// now append the shader text itself
	vertstrings_list[vertstrings_count++] = vertexstring;
	geomstrings_list[geomstrings_count++] = geometrystring;
	fragstrings_list[fragstrings_count++] = fragmentstring;

	// if any sources were NULL, clear the respective list
	if (!vertexstring)
		vertstrings_count = 0;
	if (!geometrystring)
		geomstrings_count = 0;
	if (!fragmentstring)
		fragstrings_count = 0;

	vertstring_length = 0;
	for (i = 0;i < vertstrings_count;i++)
		vertstring_length += strlen(vertstrings_list[i]);
	vertstring = t = Mem_Alloc(tempmempool, vertstring_length + 1);
	for (i = 0;i < vertstrings_count;t += strlen(vertstrings_list[i]), i++)
		memcpy(t, vertstrings_list[i], strlen(vertstrings_list[i]));

	geomstring_length = 0;
	for (i = 0;i < geomstrings_count;i++)
		geomstring_length += strlen(geomstrings_list[i]);
	geomstring = t = Mem_Alloc(tempmempool, geomstring_length + 1);
	for (i = 0;i < geomstrings_count;t += strlen(geomstrings_list[i]), i++)
		memcpy(t, geomstrings_list[i], strlen(geomstrings_list[i]));

	fragstring_length = 0;
	for (i = 0;i < fragstrings_count;i++)
		fragstring_length += strlen(fragstrings_list[i]);
	fragstring = t = Mem_Alloc(tempmempool, fragstring_length + 1);
	for (i = 0;i < fragstrings_count;t += strlen(fragstrings_list[i]), i++)
		memcpy(t, fragstrings_list[i], strlen(fragstrings_list[i]));

	CHECKGLERROR
	CHECKCGERROR
	//vertexProfile = CG_PROFILE_ARBVP1;
	//fragmentProfile = CG_PROFILE_ARBFP1;
	vertexProfile = cgGLGetLatestProfile(CG_GL_VERTEX);CHECKCGERROR
	fragmentProfile = cgGLGetLatestProfile(CG_GL_FRAGMENT);CHECKCGERROR
	//cgGLSetOptimalOptions(vertexProfile);CHECKCGERROR
	//cgGLSetOptimalOptions(fragmentProfile);CHECKCGERROR
	//cgSetAutoCompile(vid.cgcontext, CG_COMPILE_MANUAL);CHECKCGERROR
	CHECKGLERROR

	// try to load the cached shader, or generate one
	R_CG_CacheShader(p, cachename, vertstring, fragstring);

	// if caching failed, do a dynamic compile for now
	CHECKCGERROR
	if (vertstring[0] && !p->vprogram)
		p->vprogram = cgCreateProgram(vid.cgcontext, CG_SOURCE, vertstring, vertexProfile, NULL, NULL);
	CHECKCGERROR
	if (fragstring[0] && !p->fprogram)
		p->fprogram = cgCreateProgram(vid.cgcontext, CG_SOURCE, fragstring, fragmentProfile, NULL, NULL);
	CHECKCGERROR

	// look up all the uniform variable names we care about, so we don't
	// have to look them up every time we set them
	if (p->vprogram)
	{
		CHECKCGERROR
		cgGLLoadProgram(p->vprogram);CHECKCGERROR CHECKGLERROR
		cgGLEnableProfile(vertexProfile);CHECKCGERROR CHECKGLERROR
		p->vp_EyePosition                = cgGetNamedParameter(p->vprogram, "EyePosition");
		p->vp_FogPlane                   = cgGetNamedParameter(p->vprogram, "FogPlane");
		p->vp_LightDir                   = cgGetNamedParameter(p->vprogram, "LightDir");
		p->vp_LightPosition              = cgGetNamedParameter(p->vprogram, "LightPosition");
		p->vp_ModelToLight               = cgGetNamedParameter(p->vprogram, "ModelToLight");
		p->vp_TexMatrix                  = cgGetNamedParameter(p->vprogram, "TexMatrix");
		p->vp_BackgroundTexMatrix        = cgGetNamedParameter(p->vprogram, "BackgroundTexMatrix");
		p->vp_ModelViewProjectionMatrix  = cgGetNamedParameter(p->vprogram, "ModelViewProjectionMatrix");
		p->vp_ModelViewMatrix            = cgGetNamedParameter(p->vprogram, "ModelViewMatrix");
		p->vp_ShadowMapMatrix            = cgGetNamedParameter(p->vprogram, "ShadowMapMatrix");
		CHECKCGERROR
	}
	if (p->fprogram)
	{
		CHECKCGERROR
		cgGLLoadProgram(p->fprogram);CHECKCGERROR CHECKGLERROR
		cgGLEnableProfile(fragmentProfile);CHECKCGERROR CHECKGLERROR
		p->fp_Texture_First              = cgGetNamedParameter(p->fprogram, "Texture_First");
		p->fp_Texture_Second             = cgGetNamedParameter(p->fprogram, "Texture_Second");
		p->fp_Texture_GammaRamps         = cgGetNamedParameter(p->fprogram, "Texture_GammaRamps");
		p->fp_Texture_Normal             = cgGetNamedParameter(p->fprogram, "Texture_Normal");
		p->fp_Texture_Color              = cgGetNamedParameter(p->fprogram, "Texture_Color");
		p->fp_Texture_Gloss              = cgGetNamedParameter(p->fprogram, "Texture_Gloss");
		p->fp_Texture_Glow               = cgGetNamedParameter(p->fprogram, "Texture_Glow");
		p->fp_Texture_SecondaryNormal    = cgGetNamedParameter(p->fprogram, "Texture_SecondaryNormal");
		p->fp_Texture_SecondaryColor     = cgGetNamedParameter(p->fprogram, "Texture_SecondaryColor");
		p->fp_Texture_SecondaryGloss     = cgGetNamedParameter(p->fprogram, "Texture_SecondaryGloss");
		p->fp_Texture_SecondaryGlow      = cgGetNamedParameter(p->fprogram, "Texture_SecondaryGlow");
		p->fp_Texture_Pants              = cgGetNamedParameter(p->fprogram, "Texture_Pants");
		p->fp_Texture_Shirt              = cgGetNamedParameter(p->fprogram, "Texture_Shirt");
		p->fp_Texture_FogMask            = cgGetNamedParameter(p->fprogram, "Texture_FogMask");
		p->fp_Texture_Lightmap           = cgGetNamedParameter(p->fprogram, "Texture_Lightmap");
		p->fp_Texture_Deluxemap          = cgGetNamedParameter(p->fprogram, "Texture_Deluxemap");
		p->fp_Texture_Attenuation        = cgGetNamedParameter(p->fprogram, "Texture_Attenuation");
		p->fp_Texture_Cube               = cgGetNamedParameter(p->fprogram, "Texture_Cube");
		p->fp_Texture_Refraction         = cgGetNamedParameter(p->fprogram, "Texture_Refraction");
		p->fp_Texture_Reflection         = cgGetNamedParameter(p->fprogram, "Texture_Reflection");
		p->fp_Texture_ShadowMapRect      = cgGetNamedParameter(p->fprogram, "Texture_ShadowMapRect");
		p->fp_Texture_ShadowMapCube      = cgGetNamedParameter(p->fprogram, "Texture_ShadowMapCube");
		p->fp_Texture_ShadowMap2D        = cgGetNamedParameter(p->fprogram, "Texture_ShadowMap2D");
		p->fp_Texture_CubeProjection     = cgGetNamedParameter(p->fprogram, "Texture_CubeProjection");
		p->fp_Texture_ScreenDepth        = cgGetNamedParameter(p->fprogram, "Texture_ScreenDepth");
		p->fp_Texture_ScreenNormalMap    = cgGetNamedParameter(p->fprogram, "Texture_ScreenNormalMap");
		p->fp_Texture_ScreenDiffuse      = cgGetNamedParameter(p->fprogram, "Texture_ScreenDiffuse");
		p->fp_Texture_ScreenSpecular     = cgGetNamedParameter(p->fprogram, "Texture_ScreenSpecular");
		p->fp_Texture_ReflectMask        = cgGetNamedParameter(p->fprogram, "Texture_ReflectMask");
		p->fp_Texture_ReflectCube        = cgGetNamedParameter(p->fprogram, "Texture_ReflectCube");
		p->fp_Alpha                      = cgGetNamedParameter(p->fprogram, "Alpha");
		p->fp_BloomBlur_Parameters       = cgGetNamedParameter(p->fprogram, "BloomBlur_Parameters");
		p->fp_ClientTime                 = cgGetNamedParameter(p->fprogram, "ClientTime");
		p->fp_Color_Ambient              = cgGetNamedParameter(p->fprogram, "Color_Ambient");
		p->fp_Color_Diffuse              = cgGetNamedParameter(p->fprogram, "Color_Diffuse");
		p->fp_Color_Specular             = cgGetNamedParameter(p->fprogram, "Color_Specular");
		p->fp_Color_Glow                 = cgGetNamedParameter(p->fprogram, "Color_Glow");
		p->fp_Color_Pants                = cgGetNamedParameter(p->fprogram, "Color_Pants");
		p->fp_Color_Shirt                = cgGetNamedParameter(p->fprogram, "Color_Shirt");
		p->fp_DeferredColor_Ambient      = cgGetNamedParameter(p->fprogram, "DeferredColor_Ambient");
		p->fp_DeferredColor_Diffuse      = cgGetNamedParameter(p->fprogram, "DeferredColor_Diffuse");
		p->fp_DeferredColor_Specular     = cgGetNamedParameter(p->fprogram, "DeferredColor_Specular");
		p->fp_DeferredMod_Diffuse        = cgGetNamedParameter(p->fprogram, "DeferredMod_Diffuse");
		p->fp_DeferredMod_Specular       = cgGetNamedParameter(p->fprogram, "DeferredMod_Specular");
		p->fp_DistortScaleRefractReflect = cgGetNamedParameter(p->fprogram, "DistortScaleRefractReflect");
		p->fp_EyePosition                = cgGetNamedParameter(p->fprogram, "EyePosition");
		p->fp_FogColor                   = cgGetNamedParameter(p->fprogram, "FogColor");
		p->fp_FogHeightFade              = cgGetNamedParameter(p->fprogram, "FogHeightFade");
		p->fp_FogPlane                   = cgGetNamedParameter(p->fprogram, "FogPlane");
		p->fp_FogPlaneViewDist           = cgGetNamedParameter(p->fprogram, "FogPlaneViewDist");
		p->fp_FogRangeRecip              = cgGetNamedParameter(p->fprogram, "FogRangeRecip");
		p->fp_LightColor                 = cgGetNamedParameter(p->fprogram, "LightColor");
		p->fp_LightDir                   = cgGetNamedParameter(p->fprogram, "LightDir");
		p->fp_LightPosition              = cgGetNamedParameter(p->fprogram, "LightPosition");
		p->fp_OffsetMapping_Scale        = cgGetNamedParameter(p->fprogram, "OffsetMapping_Scale");
		p->fp_PixelSize                  = cgGetNamedParameter(p->fprogram, "PixelSize");
		p->fp_ReflectColor               = cgGetNamedParameter(p->fprogram, "ReflectColor");
		p->fp_ReflectFactor              = cgGetNamedParameter(p->fprogram, "ReflectFactor");
		p->fp_ReflectOffset              = cgGetNamedParameter(p->fprogram, "ReflectOffset");
		p->fp_RefractColor               = cgGetNamedParameter(p->fprogram, "RefractColor");
		p->fp_Saturation                 = cgGetNamedParameter(p->fprogram, "Saturation");
		p->fp_ScreenCenterRefractReflect = cgGetNamedParameter(p->fprogram, "ScreenCenterRefractReflect");
		p->fp_ScreenScaleRefractReflect  = cgGetNamedParameter(p->fprogram, "ScreenScaleRefractReflect");
		p->fp_ScreenToDepth              = cgGetNamedParameter(p->fprogram, "ScreenToDepth");
		p->fp_ShadowMap_Parameters       = cgGetNamedParameter(p->fprogram, "ShadowMap_Parameters");
		p->fp_ShadowMap_TextureScale     = cgGetNamedParameter(p->fprogram, "ShadowMap_TextureScale");
		p->fp_SpecularPower              = cgGetNamedParameter(p->fprogram, "SpecularPower");
		p->fp_UserVec1                   = cgGetNamedParameter(p->fprogram, "UserVec1");
		p->fp_UserVec2                   = cgGetNamedParameter(p->fprogram, "UserVec2");
		p->fp_UserVec3                   = cgGetNamedParameter(p->fprogram, "UserVec3");
		p->fp_UserVec4                   = cgGetNamedParameter(p->fprogram, "UserVec4");
		p->fp_ViewTintColor              = cgGetNamedParameter(p->fprogram, "ViewTintColor");
		p->fp_ViewToLight                = cgGetNamedParameter(p->fprogram, "ViewToLight");
		p->fp_PixelToScreenTexCoord      = cgGetNamedParameter(p->fprogram, "PixelToScreenTexCoord");
		p->fp_ModelToReflectCube         = cgGetNamedParameter(p->fprogram, "ModelToReflectCube");
		CHECKCGERROR
	}

	if ((p->vprogram || !vertstring[0]) && (p->fprogram || !fragstring[0]))
		Con_DPrintf("^5CG shader %s compiled.\n", permutationname);
	else
		Con_Printf("^1CG shader %s failed!  some features may not work properly.\n", permutationname);

	// free the strings
	if (vertstring)
		Mem_Free(vertstring);
	if (geomstring)
		Mem_Free(geomstring);
	if (fragstring)
		Mem_Free(fragstring);
	if (vertexstring)
		Mem_Free(vertexstring);
	if (geometrystring)
		Mem_Free(geometrystring);
	if (fragmentstring)
		Mem_Free(fragmentstring);
}

void R_SetupShader_SetPermutationCG(unsigned int mode, unsigned int permutation)
{
	r_cg_permutation_t *perm = R_CG_FindPermutation(mode, permutation);
	CHECKGLERROR
	CHECKCGERROR
	if (r_cg_permutation != perm)
	{
		r_cg_permutation = perm;
		if (!r_cg_permutation->vprogram && !r_cg_permutation->fprogram)
		{
			if (!r_cg_permutation->compiled)
				R_CG_CompilePermutation(perm, mode, permutation);
			if (!r_cg_permutation->vprogram && !r_cg_permutation->fprogram)
			{
				// remove features until we find a valid permutation
				int i;
				for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
				{
					// reduce i more quickly whenever it would not remove any bits
					int j = 1<<(SHADERPERMUTATION_COUNT-1-i);
					if (!(permutation & j))
						continue;
					permutation -= j;
					r_cg_permutation = R_CG_FindPermutation(mode, permutation);
					if (!r_cg_permutation->compiled)
						R_CG_CompilePermutation(perm, mode, permutation);
					if (r_cg_permutation->vprogram || r_cg_permutation->fprogram)
						break;
				}
				if (i >= SHADERPERMUTATION_COUNT)
				{
					//Con_Printf("Could not find a working Cg shader for permutation %s %s\n", shadermodeinfo[mode].vertexfilename, shadermodeinfo[mode].pretext);
					r_cg_permutation = R_CG_FindPermutation(mode, permutation);
					return; // no bit left to clear, entire mode is broken
				}
			}
		}
		CHECKGLERROR
		CHECKCGERROR
		if (r_cg_permutation->vprogram)
		{
			cgGLLoadProgram(r_cg_permutation->vprogram);CHECKCGERROR CHECKGLERROR
			cgGLBindProgram(r_cg_permutation->vprogram);CHECKCGERROR CHECKGLERROR
			cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));CHECKCGERROR CHECKGLERROR
		}
		else
		{
			cgGLDisableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));CHECKCGERROR CHECKGLERROR
			cgGLUnbindProgram(cgGLGetLatestProfile(CG_GL_VERTEX));CHECKCGERROR CHECKGLERROR
		}
		if (r_cg_permutation->fprogram)
		{
			cgGLLoadProgram(r_cg_permutation->fprogram);CHECKCGERROR CHECKGLERROR
			cgGLBindProgram(r_cg_permutation->fprogram);CHECKCGERROR CHECKGLERROR
			cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));CHECKCGERROR CHECKGLERROR
		}
		else
		{
			cgGLDisableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));CHECKCGERROR CHECKGLERROR
			cgGLUnbindProgram(cgGLGetLatestProfile(CG_GL_FRAGMENT));CHECKCGERROR CHECKGLERROR
		}
	}
	CHECKCGERROR
	if (r_cg_permutation->vp_ModelViewProjectionMatrix) cgGLSetMatrixParameterfc(r_cg_permutation->vp_ModelViewProjectionMatrix, gl_modelviewprojection16f);CHECKCGERROR
	if (r_cg_permutation->vp_ModelViewMatrix) cgGLSetMatrixParameterfc(r_cg_permutation->vp_ModelViewMatrix, gl_modelview16f);CHECKCGERROR
	if (r_cg_permutation->fp_ClientTime) cgGLSetParameter1f(r_cg_permutation->fp_ClientTime, cl.time);CHECKCGERROR
}

void CG_BindTexture(CGparameter param, rtexture_t *tex)
{
	cgGLSetTextureParameter(param, R_GetTexture(tex));
	cgGLEnableTextureParameter(param);
}
#endif

void R_GLSL_Restart_f(void)
{
	unsigned int i, limit;
	if (glslshaderstring && glslshaderstring != builtinshaderstring)
		Mem_Free(glslshaderstring);
	glslshaderstring = NULL;
	if (cgshaderstring && cgshaderstring != builtincgshaderstring)
		Mem_Free(cgshaderstring);
	cgshaderstring = NULL;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
		{
			r_glsl_permutation_t *p;
			r_glsl_permutation = NULL;
			limit = Mem_ExpandableArray_IndexRange(&r_glsl_permutationarray);
			for (i = 0;i < limit;i++)
			{
				if ((p = (r_glsl_permutation_t*)Mem_ExpandableArray_RecordAtIndex(&r_glsl_permutationarray, i)))
				{
					GL_Backend_FreeProgram(p->program);
					Mem_ExpandableArray_FreeRecord(&r_glsl_permutationarray, (void*)p);
				}
			}
			memset(r_glsl_permutationhash, 0, sizeof(r_glsl_permutationhash));
		}
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		{
			r_cg_permutation_t *p;
			r_cg_permutation = NULL;
			cgGLDisableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));CHECKCGERROR CHECKGLERROR
			cgGLUnbindProgram(cgGLGetLatestProfile(CG_GL_VERTEX));CHECKCGERROR CHECKGLERROR
			cgGLDisableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));CHECKCGERROR CHECKGLERROR
			cgGLUnbindProgram(cgGLGetLatestProfile(CG_GL_FRAGMENT));CHECKCGERROR CHECKGLERROR
			limit = Mem_ExpandableArray_IndexRange(&r_cg_permutationarray);
			for (i = 0;i < limit;i++)
			{
				if ((p = (r_cg_permutation_t*)Mem_ExpandableArray_RecordAtIndex(&r_cg_permutationarray, i)))
				{
					if (p->vprogram)
						cgDestroyProgram(p->vprogram);
					if (p->fprogram)
						cgDestroyProgram(p->fprogram);
					Mem_ExpandableArray_FreeRecord(&r_cg_permutationarray, (void*)p);
				}
			}
			memset(r_cg_permutationhash, 0, sizeof(r_cg_permutationhash));
		}
		break;
#endif
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		break;
	}
}

void R_GLSL_DumpShader_f(void)
{
	int i;
	qfile_t *file;

	file = FS_OpenRealFile("glsl/default.glsl", "w", false);
	if (file)
	{
		FS_Print(file, "/* The engine may define the following macros:\n");
		FS_Print(file, "#define VERTEX_SHADER\n#define GEOMETRY_SHADER\n#define FRAGMENT_SHADER\n");
		for (i = 0;i < SHADERMODE_COUNT;i++)
			FS_Print(file, glslshadermodeinfo[i].pretext);
		for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
			FS_Print(file, shaderpermutationinfo[i].pretext);
		FS_Print(file, "*/\n");
		FS_Print(file, builtinshaderstring);
		FS_Close(file);
		Con_Printf("glsl/default.glsl written\n");
	}
	else
		Con_Printf("failed to write to glsl/default.glsl\n");

#ifdef SUPPORTCG
	file = FS_OpenRealFile("cg/default.cg", "w", false);
	if (file)
	{
		FS_Print(file, "/* The engine may define the following macros:\n");
		FS_Print(file, "#define VERTEX_SHADER\n#define GEOMETRY_SHADER\n#define FRAGMENT_SHADER\n");
		for (i = 0;i < SHADERMODE_COUNT;i++)
			FS_Print(file, cgshadermodeinfo[i].pretext);
		for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
			FS_Print(file, shaderpermutationinfo[i].pretext);
		FS_Print(file, "*/\n");
		FS_Print(file, builtincgshaderstring);
		FS_Close(file);
		Con_Printf("cg/default.cg written\n");
	}
	else
		Con_Printf("failed to write to cg/default.cg\n");
#endif
}

void R_SetupShader_Generic(rtexture_t *first, rtexture_t *second, int texturemode, int rgbscale)
{
	if (!second)
		texturemode = GL_MODULATE;
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_GENERIC, (first ? SHADERPERMUTATION_DIFFUSE : 0) | (second ? SHADERPERMUTATION_SPECULAR : 0) | (r_shadow_glossexact.integer ? SHADERPERMUTATION_EXACTSPECULARMATH : 0) | (texturemode == GL_MODULATE ? SHADERPERMUTATION_COLORMAPPING : (texturemode == GL_ADD ? SHADERPERMUTATION_GLOW : (texturemode == GL_DECAL ? SHADERPERMUTATION_VERTEXTEXTUREBLEND : 0))));
		if (r_glsl_permutation->loc_Texture_First ) R_Mesh_TexBind(GL20TU_FIRST , first );
		if (r_glsl_permutation->loc_Texture_Second) R_Mesh_TexBind(GL20TU_SECOND, second);
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		CHECKCGERROR
		R_SetupShader_SetPermutationCG(SHADERMODE_GENERIC, (first ? SHADERPERMUTATION_DIFFUSE : 0) | (second ? SHADERPERMUTATION_SPECULAR : 0) | (r_shadow_glossexact.integer ? SHADERPERMUTATION_EXACTSPECULARMATH : 0) | (texturemode == GL_MODULATE ? SHADERPERMUTATION_COLORMAPPING : (texturemode == GL_ADD ? SHADERPERMUTATION_GLOW : (texturemode == GL_DECAL ? SHADERPERMUTATION_VERTEXTEXTUREBLEND : 0))));
		if (r_cg_permutation->fp_Texture_First ) CG_BindTexture(r_cg_permutation->fp_Texture_First , first );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Second) CG_BindTexture(r_cg_permutation->fp_Texture_Second, second);CHECKCGERROR
#endif
		break;
	case RENDERPATH_GL13:
		R_Mesh_TexBind(0, first );
		R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
		R_Mesh_TexBind(1, second);
		if (second)
			R_Mesh_TexCombine(1, texturemode, texturemode, rgbscale, 1);
		break;
	case RENDERPATH_GL11:
		R_Mesh_TexBind(0, first );
		break;
	}
}

void R_SetupShader_DepthOrShadow(void)
{
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_DEPTH_OR_SHADOW, 0);
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		R_SetupShader_SetPermutationCG(SHADERMODE_DEPTH_OR_SHADOW, 0);
#endif
		break;
	case RENDERPATH_GL13:
		R_Mesh_TexBind(0, 0);
		R_Mesh_TexBind(1, 0);
		break;
	case RENDERPATH_GL11:
		R_Mesh_TexBind(0, 0);
		break;
	}
}

void R_SetupShader_ShowDepth(void)
{
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_SHOWDEPTH, 0);
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		R_SetupShader_SetPermutationCG(SHADERMODE_SHOWDEPTH, 0);
#endif
		break;
	case RENDERPATH_GL13:
		break;
	case RENDERPATH_GL11:
		break;
	}
}

extern qboolean r_shadow_usingdeferredprepass;
extern cvar_t r_shadow_deferred_8bitrange;
extern rtexture_t *r_shadow_attenuationgradienttexture;
extern rtexture_t *r_shadow_attenuation2dtexture;
extern rtexture_t *r_shadow_attenuation3dtexture;
extern qboolean r_shadow_usingshadowmaprect;
extern qboolean r_shadow_usingshadowmapcube;
extern qboolean r_shadow_usingshadowmap2d;
extern qboolean r_shadow_usingshadowmaportho;
extern float r_shadow_shadowmap_texturescale[2];
extern float r_shadow_shadowmap_parameters[4];
extern qboolean r_shadow_shadowmapvsdct;
extern qboolean r_shadow_shadowmapsampler;
extern int r_shadow_shadowmappcf;
extern rtexture_t *r_shadow_shadowmaprectangletexture;
extern rtexture_t *r_shadow_shadowmap2dtexture;
extern rtexture_t *r_shadow_shadowmapcubetexture[R_SHADOW_SHADOWMAP_NUMCUBEMAPS];
extern rtexture_t *r_shadow_shadowmapvsdcttexture;
extern matrix4x4_t r_shadow_shadowmapmatrix;
extern int r_shadow_shadowmaplod; // changes for each light based on distance
extern int r_shadow_prepass_width;
extern int r_shadow_prepass_height;
extern rtexture_t *r_shadow_prepassgeometrydepthtexture;
extern rtexture_t *r_shadow_prepassgeometrynormalmaptexture;
extern rtexture_t *r_shadow_prepasslightingdiffusetexture;
extern rtexture_t *r_shadow_prepasslightingspeculartexture;
void R_SetupShader_Surface(const vec3_t lightcolorbase, qboolean modellighting, float ambientscale, float diffusescale, float specularscale, rsurfacepass_t rsurfacepass)
{
	// select a permutation of the lighting shader appropriate to this
	// combination of texture, entity, light source, and fogging, only use the
	// minimum features necessary to avoid wasting rendering time in the
	// fragment shader on features that are not being used
	unsigned int permutation = 0;
	unsigned int mode = 0;
	float m16f[16];
	if (rsurfacepass == RSURFPASS_BACKGROUND)
	{
		// distorted background
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_WATERSHADER)
			mode = SHADERMODE_WATER;
		else
			mode = SHADERMODE_REFRACTION;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
		R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
		R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest(false);
		GL_BlendFunc(GL_ONE, GL_ZERO);
	}
	else if (rsurfacepass == RSURFPASS_DEFERREDGEOMETRY)
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
			permutation |= SHADERPERMUTATION_ALPHAKILL;
		// normalmap (deferred prepass), may use alpha test on diffuse
		mode = SHADERMODE_DEFERREDGEOMETRY;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
		R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
		R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
			R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
		else
			R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest(false);
		GL_BlendFunc(GL_ONE, GL_ZERO);
	}
	else if (rsurfacepass == RSURFPASS_RTLIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// light source
		mode = SHADERMODE_LIGHTSOURCE;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		if (rsurface.rtlight->currentcubemap != r_texture_whitecube)
			permutation |= SHADERPERMUTATION_CUBEFILTER;
		if (diffusescale > 0)
			permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
		{
			permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
			if (r_shadow_glossexact.integer)
				permutation |= SHADERPERMUTATION_EXACTSPECULARMATH;
		}
		if (r_refdef.fogenabled)
			permutation |= r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaprect || r_shadow_usingshadowmap2d || r_shadow_usingshadowmapcube)
		{
			if (r_shadow_usingshadowmaprect)
				permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
			if (r_shadow_usingshadowmap2d)
				permutation |= SHADERPERMUTATION_SHADOWMAP2D;
			if (r_shadow_usingshadowmapcube)
				permutation |= SHADERPERMUTATION_SHADOWMAPCUBE;
			else if(r_shadow_shadowmapvsdct)
				permutation |= SHADERPERMUTATION_SHADOWMAPVSDCT;

			if (r_shadow_shadowmapsampler)
				permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
			if (r_shadow_shadowmappcf > 1)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
			else if (r_shadow_shadowmappcf)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
		}
		if (rsurface.texture->reflectmasktexture)
			permutation |= SHADERPERMUTATION_REFLECTCUBE;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		if (true || permutation & (SHADERPERMUTATION_DIFFUSE | SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_OFFSETMAPPING))
		{
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		}
		else
		{
			R_Mesh_TexCoordPointer(1, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(2, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(3, 0, NULL, 0, 0);
		}
		//R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
			R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
		else
			R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// unshaded geometry (fullbright or ambient model lighting)
		mode = SHADERMODE_FLATCOLOR;
		ambientscale = diffusescale = specularscale = 0;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
			if (r_shadow_usingshadowmaprect)
				permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
			if (r_shadow_usingshadowmap2d)
				permutation |= SHADERPERMUTATION_SHADOWMAP2D;

			if (r_shadow_shadowmapsampler)
				permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
			if (r_shadow_shadowmappcf > 1)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
			else if (r_shadow_shadowmappcf)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
		if (rsurface.texture->reflectmasktexture)
			permutation |= SHADERPERMUTATION_REFLECTCUBE;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		if (true || permutation & (SHADERPERMUTATION_DIFFUSE | SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_OFFSETMAPPING))
		{
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		}
		else
		{
			R_Mesh_TexCoordPointer(1, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(2, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(3, 0, NULL, 0, 0);
		}
		R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
			R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
		else
			R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT_DIRECTIONAL)
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// directional model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
		{
			permutation |= SHADERPERMUTATION_SPECULAR;
			if (r_shadow_glossexact.integer)
				permutation |= SHADERPERMUTATION_EXACTSPECULARMATH;
		}
		if (r_refdef.fogenabled)
			permutation |= r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
			if (r_shadow_usingshadowmaprect)
				permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
			if (r_shadow_usingshadowmap2d)
				permutation |= SHADERPERMUTATION_SHADOWMAP2D;

			if (r_shadow_shadowmapsampler)
				permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
			if (r_shadow_shadowmappcf > 1)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
			else if (r_shadow_shadowmappcf)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
		if (r_shadow_usingdeferredprepass && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED))
			permutation |= SHADERPERMUTATION_DEFERREDLIGHTMAP;
		if (rsurface.texture->reflectmasktexture)
			permutation |= SHADERPERMUTATION_REFLECTCUBE;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		if (true || permutation & (SHADERPERMUTATION_DIFFUSE | SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_OFFSETMAPPING))
		{
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		}
		else
		{
			R_Mesh_TexCoordPointer(1, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(2, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(3, 0, NULL, 0, 0);
		}
		R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// ambient model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
			if (r_shadow_usingshadowmaprect)
				permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
			if (r_shadow_usingshadowmap2d)
				permutation |= SHADERPERMUTATION_SHADOWMAP2D;

			if (r_shadow_shadowmapsampler)
				permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
			if (r_shadow_shadowmappcf > 1)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
			else if (r_shadow_shadowmappcf)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
		if (r_shadow_usingdeferredprepass && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED))
			permutation |= SHADERPERMUTATION_DEFERREDLIGHTMAP;
		if (rsurface.texture->reflectmasktexture)
			permutation |= SHADERPERMUTATION_REFLECTCUBE;
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		if (true || permutation & (SHADERPERMUTATION_DIFFUSE | SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_OFFSETMAPPING))
		{
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		}
		else
		{
			R_Mesh_TexCoordPointer(1, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(2, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(3, 0, NULL, 0, 0);
		}
		R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
	}
	else
	{
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// lightmapped wall
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
			if (r_shadow_usingshadowmaprect)
				permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
			if (r_shadow_usingshadowmap2d)
				permutation |= SHADERPERMUTATION_SHADOWMAP2D;

			if (r_shadow_shadowmapsampler)
				permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
			if (r_shadow_shadowmappcf > 1)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
			else if (r_shadow_shadowmappcf)
				permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
		if (r_shadow_usingdeferredprepass && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED))
			permutation |= SHADERPERMUTATION_DEFERREDLIGHTMAP;
		if (rsurface.texture->reflectmasktexture)
			permutation |= SHADERPERMUTATION_REFLECTCUBE;
		if (r_glsl_deluxemapping.integer >= 1 && rsurface.uselightmaptexture && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brushq3.deluxemapping)
		{
			// deluxemapping (light direction texture)
			if (rsurface.uselightmaptexture && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brushq3.deluxemapping && r_refdef.scene.worldmodel->brushq3.deluxemapping_modelspace)
				mode = SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE;
			else
				mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			if (specularscale > 0)
			{
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
				if (r_shadow_glossexact.integer)
					permutation |= SHADERPERMUTATION_EXACTSPECULARMATH;
			}
			R_Mesh_TexCoordPointer(4, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
			if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
				R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
			else
				R_Mesh_ColorPointer(NULL, 0, 0);
		}
		else if (r_glsl_deluxemapping.integer >= 2)
		{
			// fake deluxemapping (uniform light direction in tangentspace)
			mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			if (specularscale > 0)
			{
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
				if (r_shadow_glossexact.integer)
					permutation |= SHADERPERMUTATION_EXACTSPECULARMATH;
			}
			R_Mesh_TexCoordPointer(4, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
			if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
				R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
			else
				R_Mesh_ColorPointer(NULL, 0, 0);
		}
		else if (rsurface.uselightmaptexture)
		{
			// ordinary lightmapping (q1bsp, q3bsp)
			mode = SHADERMODE_LIGHTMAP;
			R_Mesh_TexCoordPointer(4, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
			if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND)
				R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
			else
				R_Mesh_ColorPointer(NULL, 0, 0);
		}
		else
		{
			// ordinary vertex coloring (q3bsp)
			mode = SHADERMODE_VERTEXCOLOR;
			R_Mesh_TexCoordPointer(4, 0, NULL, 0, 0);
			R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
		}
		R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
		if (true || permutation & (SHADERPERMUTATION_DIFFUSE | SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_OFFSETMAPPING))
		{
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
		}
		else
		{
			R_Mesh_TexCoordPointer(1, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(2, 0, NULL, 0, 0);
			R_Mesh_TexCoordPointer(3, 0, NULL, 0, 0);
		}
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
		R_SetupShader_SetPermutationGLSL(mode, permutation);
		if (r_glsl_permutation->loc_ModelToReflectCube >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.matrix, m16f);qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelToReflectCube, 1, false, m16f);}
		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			if (r_glsl_permutation->loc_ModelToLight >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.entitytolight, m16f);qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelToLight, 1, false, m16f);}
			if (r_glsl_permutation->loc_LightPosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
			if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
			if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Ambient, rsurface.colormod[0] * ambientscale, rsurface.colormod[1] * ambientscale, rsurface.colormod[2] * ambientscale);
			if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Diffuse, rsurface.colormod[0] * diffusescale, rsurface.colormod[1] * diffusescale, rsurface.colormod[2] * diffusescale);
			if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Specular, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale);
	
			// additive passes are only darkened by fog, not tinted
			if (r_glsl_permutation->loc_FogColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_FogColor, 0, 0, 0);
			if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularPower, rsurface.texture->specularpower * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));
		}
		else
		{
			if (mode == SHADERMODE_FLATCOLOR)
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Ambient, rsurface.colormod[0], rsurface.colormod[1], rsurface.colormod[2]);
			}
			else if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Ambient, (r_refdef.scene.ambient + rsurface.modellight_ambient[0] * r_refdef.lightmapintensity) * rsurface.colormod[0], (r_refdef.scene.ambient + rsurface.modellight_ambient[1] * r_refdef.lightmapintensity) * rsurface.colormod[1], (r_refdef.scene.ambient + rsurface.modellight_ambient[2] * r_refdef.lightmapintensity) * rsurface.colormod[2]);
				if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Diffuse, r_refdef.lightmapintensity * rsurface.colormod[0], r_refdef.lightmapintensity * rsurface.colormod[1], r_refdef.lightmapintensity * rsurface.colormod[2]);
				if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				if (r_glsl_permutation->loc_DeferredMod_Diffuse >= 0) qglUniform3fARB(r_glsl_permutation->loc_DeferredMod_Diffuse, rsurface.colormod[0] * r_shadow_deferred_8bitrange.value, rsurface.colormod[1] * r_shadow_deferred_8bitrange.value, rsurface.colormod[2] * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_DeferredMod_Specular >= 0) qglUniform3fARB(r_glsl_permutation->loc_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightColor, rsurface.modellight_diffuse[0], rsurface.modellight_diffuse[1], rsurface.modellight_diffuse[2]);
				if (r_glsl_permutation->loc_LightDir >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
			}
			else
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Ambient, r_refdef.scene.ambient * rsurface.colormod[0], r_refdef.scene.ambient * rsurface.colormod[1], r_refdef.scene.ambient * rsurface.colormod[2]);
				if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Diffuse, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2]);
				if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				if (r_glsl_permutation->loc_DeferredMod_Diffuse >= 0) qglUniform3fARB(r_glsl_permutation->loc_DeferredMod_Diffuse, rsurface.colormod[0] * diffusescale * r_shadow_deferred_8bitrange.value, rsurface.colormod[1] * diffusescale * r_shadow_deferred_8bitrange.value, rsurface.colormod[2] * diffusescale * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_DeferredMod_Specular >= 0) qglUniform3fARB(r_glsl_permutation->loc_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
			}
			// additive passes are only darkened by fog, not tinted
			if (r_glsl_permutation->loc_FogColor >= 0)
			{
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ADD)
					qglUniform3fARB(r_glsl_permutation->loc_FogColor, 0, 0, 0);
				else
					qglUniform3fARB(r_glsl_permutation->loc_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
			}
			if (r_glsl_permutation->loc_DistortScaleRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);
			if (r_glsl_permutation->loc_ScreenScaleRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);
			if (r_glsl_permutation->loc_ScreenCenterRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);
			if (r_glsl_permutation->loc_RefractColor >= 0) qglUniform4fvARB(r_glsl_permutation->loc_RefractColor, 1, rsurface.texture->refractcolor4f);
			if (r_glsl_permutation->loc_ReflectColor >= 0) qglUniform4fvARB(r_glsl_permutation->loc_ReflectColor, 1, rsurface.texture->reflectcolor4f);
			if (r_glsl_permutation->loc_ReflectFactor >= 0) qglUniform1fARB(r_glsl_permutation->loc_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);
			if (r_glsl_permutation->loc_ReflectOffset >= 0) qglUniform1fARB(r_glsl_permutation->loc_ReflectOffset, rsurface.texture->reflectmin);
			if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularPower, rsurface.texture->specularpower * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));
		}
		if (r_glsl_permutation->loc_TexMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currenttexmatrix, m16f);qglUniformMatrix4fvARB(r_glsl_permutation->loc_TexMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_BackgroundTexMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currentbackgroundtexmatrix, m16f);qglUniformMatrix4fvARB(r_glsl_permutation->loc_BackgroundTexMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_ShadowMapMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&r_shadow_shadowmapmatrix, m16f);qglUniformMatrix4fvARB(r_glsl_permutation->loc_ShadowMapMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_ShadowMap_TextureScale >= 0) qglUniform2fARB(r_glsl_permutation->loc_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		if (r_glsl_permutation->loc_ShadowMap_Parameters >= 0) qglUniform4fARB(r_glsl_permutation->loc_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);

		if (r_glsl_permutation->loc_Color_Glow >= 0) qglUniform3fARB(r_glsl_permutation->loc_Color_Glow, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2]);
		if (r_glsl_permutation->loc_Alpha >= 0) qglUniform1fARB(r_glsl_permutation->loc_Alpha, rsurface.texture->lightmapcolor[3]);
		if (r_glsl_permutation->loc_EyePosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);
		if (r_glsl_permutation->loc_Color_Pants >= 0)
		{
			if (rsurface.texture->pantstexture)
				qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
			else
				qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, 0, 0, 0);
		}
		if (r_glsl_permutation->loc_Color_Shirt >= 0)
		{
			if (rsurface.texture->shirttexture)
				qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
			else
				qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, 0, 0, 0);
		}
		if (r_glsl_permutation->loc_FogPlane >= 0) qglUniform4fARB(r_glsl_permutation->loc_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);
		if (r_glsl_permutation->loc_FogPlaneViewDist >= 0) qglUniform1fARB(r_glsl_permutation->loc_FogPlaneViewDist, rsurface.fogplaneviewdist);
		if (r_glsl_permutation->loc_FogRangeRecip >= 0) qglUniform1fARB(r_glsl_permutation->loc_FogRangeRecip, rsurface.fograngerecip);
		if (r_glsl_permutation->loc_FogHeightFade >= 0) qglUniform1fARB(r_glsl_permutation->loc_FogHeightFade, rsurface.fogheightfade);
		if (r_glsl_permutation->loc_OffsetMapping_Scale >= 0) qglUniform1fARB(r_glsl_permutation->loc_OffsetMapping_Scale, r_glsl_offsetmapping_scale.value);
		if (r_glsl_permutation->loc_ScreenToDepth >= 0) qglUniform2fARB(r_glsl_permutation->loc_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		if (r_glsl_permutation->loc_PixelToScreenTexCoord >= 0) qglUniform2fARB(r_glsl_permutation->loc_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);

	//	if (r_glsl_permutation->loc_Texture_First           >= 0) R_Mesh_TexBind(GL20TU_FIRST             , r_texture_white                                     );
	//	if (r_glsl_permutation->loc_Texture_Second          >= 0) R_Mesh_TexBind(GL20TU_SECOND            , r_texture_white                                     );
	//	if (r_glsl_permutation->loc_Texture_GammaRamps      >= 0) R_Mesh_TexBind(GL20TU_GAMMARAMPS        , r_texture_gammaramps                                );
		if (r_glsl_permutation->loc_Texture_Normal          >= 0) R_Mesh_TexBind(GL20TU_NORMAL            , rsurface.texture->nmaptexture                       );
		if (r_glsl_permutation->loc_Texture_Color           >= 0) R_Mesh_TexBind(GL20TU_COLOR             , rsurface.texture->basetexture                       );
		if (r_glsl_permutation->loc_Texture_Gloss           >= 0) R_Mesh_TexBind(GL20TU_GLOSS             , rsurface.texture->glosstexture                      );
		if (r_glsl_permutation->loc_Texture_Glow            >= 0) R_Mesh_TexBind(GL20TU_GLOW              , rsurface.texture->glowtexture                       );
		if (r_glsl_permutation->loc_Texture_SecondaryNormal >= 0) R_Mesh_TexBind(GL20TU_SECONDARY_NORMAL  , rsurface.texture->backgroundnmaptexture             );
		if (r_glsl_permutation->loc_Texture_SecondaryColor  >= 0) R_Mesh_TexBind(GL20TU_SECONDARY_COLOR   , rsurface.texture->backgroundbasetexture             );
		if (r_glsl_permutation->loc_Texture_SecondaryGloss  >= 0) R_Mesh_TexBind(GL20TU_SECONDARY_GLOSS   , rsurface.texture->backgroundglosstexture            );
		if (r_glsl_permutation->loc_Texture_SecondaryGlow   >= 0) R_Mesh_TexBind(GL20TU_SECONDARY_GLOW    , rsurface.texture->backgroundglowtexture             );
		if (r_glsl_permutation->loc_Texture_Pants           >= 0) R_Mesh_TexBind(GL20TU_PANTS             , rsurface.texture->pantstexture                      );
		if (r_glsl_permutation->loc_Texture_Shirt           >= 0) R_Mesh_TexBind(GL20TU_SHIRT             , rsurface.texture->shirttexture                      );
		if (r_glsl_permutation->loc_Texture_ReflectMask     >= 0) R_Mesh_TexBind(GL20TU_REFLECTMASK       , rsurface.texture->reflectmasktexture                );
		if (r_glsl_permutation->loc_Texture_ReflectCube     >= 0) R_Mesh_TexBind(GL20TU_REFLECTCUBE       , rsurface.texture->reflectcubetexture ? rsurface.texture->reflectcubetexture : r_texture_whitecube);
		if (r_glsl_permutation->loc_Texture_FogMask         >= 0) R_Mesh_TexBind(GL20TU_FOGMASK           , r_texture_fogattenuation                            );
		if (r_glsl_permutation->loc_Texture_Lightmap        >= 0) R_Mesh_TexBind(GL20TU_LIGHTMAP          , r_texture_white                                     );
		if (r_glsl_permutation->loc_Texture_Deluxemap       >= 0) R_Mesh_TexBind(GL20TU_LIGHTMAP          , r_texture_blanknormalmap                            );
		if (r_glsl_permutation->loc_Texture_Attenuation     >= 0) R_Mesh_TexBind(GL20TU_ATTENUATION       , r_shadow_attenuationgradienttexture                 );
		if (r_glsl_permutation->loc_Texture_Refraction      >= 0) R_Mesh_TexBind(GL20TU_REFRACTION        , r_texture_white                                     );
		if (r_glsl_permutation->loc_Texture_Reflection      >= 0) R_Mesh_TexBind(GL20TU_REFLECTION        , r_texture_white                                     );
		if (r_glsl_permutation->loc_Texture_ScreenDepth     >= 0) R_Mesh_TexBind(GL20TU_SCREENDEPTH       , r_shadow_prepassgeometrydepthtexture                );
		if (r_glsl_permutation->loc_Texture_ScreenNormalMap >= 0) R_Mesh_TexBind(GL20TU_SCREENNORMALMAP   , r_shadow_prepassgeometrynormalmaptexture            );
		if (r_glsl_permutation->loc_Texture_ScreenDiffuse   >= 0) R_Mesh_TexBind(GL20TU_SCREENDIFFUSE     , r_shadow_prepasslightingdiffusetexture              );
		if (r_glsl_permutation->loc_Texture_ScreenSpecular  >= 0) R_Mesh_TexBind(GL20TU_SCREENSPECULAR    , r_shadow_prepasslightingspeculartexture             );
		if (rsurface.rtlight || (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW)))
		{
			if (r_glsl_permutation->loc_Texture_ShadowMap2D     >= 0) R_Mesh_TexBind(r_shadow_usingshadowmaportho ? GL20TU_SHADOWMAPORTHO2D : GL20TU_SHADOWMAP2D, r_shadow_shadowmap2dtexture                         );
			if (r_glsl_permutation->loc_Texture_ShadowMapRect   >= 0) R_Mesh_TexBind(r_shadow_usingshadowmaportho ? GL20TU_SHADOWMAPORTHORECT : GL20TU_SHADOWMAPRECT, r_shadow_shadowmaprectangletexture                  );
			if (rsurface.rtlight)
			{
				if (r_glsl_permutation->loc_Texture_Cube            >= 0) R_Mesh_TexBind(GL20TU_CUBE              , rsurface.rtlight->currentcubemap                    );
				if (r_shadow_usingshadowmapcube)
					if (r_glsl_permutation->loc_Texture_ShadowMapCube   >= 0) R_Mesh_TexBind(GL20TU_SHADOWMAPCUBE     , r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]);
				if (r_glsl_permutation->loc_Texture_CubeProjection  >= 0) R_Mesh_TexBind(GL20TU_CUBEPROJECTION    , r_shadow_shadowmapvsdcttexture                      );
			}
		}
		CHECKGLERROR
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		R_SetupShader_SetPermutationCG(mode, permutation);
		if (r_cg_permutation->fp_ModelToReflectCube) {Matrix4x4_ToArrayFloatGL(&rsurface.matrix, m16f);cgGLSetMatrixParameterfc(r_cg_permutation->fp_ModelToReflectCube, m16f);}CHECKCGERROR
		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			if (r_cg_permutation->vp_ModelToLight) {Matrix4x4_ToArrayFloatGL(&rsurface.entitytolight, m16f);cgGLSetMatrixParameterfc(r_cg_permutation->vp_ModelToLight, m16f);}CHECKCGERROR
			if (r_cg_permutation->vp_LightPosition) cgGLSetParameter3f(r_cg_permutation->vp_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);CHECKCGERROR
		}
		else
		{
			if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				if (r_cg_permutation->vp_LightDir) cgGLSetParameter3f(r_cg_permutation->vp_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);CHECKCGERROR
			}
		}
		if (r_cg_permutation->vp_TexMatrix) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currenttexmatrix, m16f);cgGLSetMatrixParameterfc(r_cg_permutation->vp_TexMatrix, m16f);}CHECKCGERROR
		if (r_cg_permutation->vp_BackgroundTexMatrix) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currentbackgroundtexmatrix, m16f);cgGLSetMatrixParameterfc(r_cg_permutation->vp_BackgroundTexMatrix, m16f);}CHECKCGERROR
		if (r_cg_permutation->vp_ShadowMapMatrix) {Matrix4x4_ToArrayFloatGL(&r_shadow_shadowmapmatrix, m16f);cgGLSetMatrixParameterfc(r_cg_permutation->vp_ShadowMapMatrix, m16f);}CHECKGLERROR
		if (r_cg_permutation->vp_EyePosition) cgGLSetParameter3f(r_cg_permutation->vp_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);CHECKCGERROR
		if (r_cg_permutation->vp_FogPlane) cgGLSetParameter4f(r_cg_permutation->vp_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);CHECKCGERROR
		CHECKGLERROR

		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			if (r_cg_permutation->fp_LightPosition) cgGLSetParameter3f(r_cg_permutation->fp_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);CHECKCGERROR
			if (r_cg_permutation->fp_LightColor) cgGLSetParameter3f(r_cg_permutation->fp_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);CHECKCGERROR
			if (r_cg_permutation->fp_Color_Ambient) cgGLSetParameter3f(r_cg_permutation->fp_Color_Ambient, rsurface.colormod[0] * ambientscale, rsurface.colormod[1] * ambientscale, rsurface.colormod[2] * ambientscale);CHECKCGERROR
			if (r_cg_permutation->fp_Color_Diffuse) cgGLSetParameter3f(r_cg_permutation->fp_Color_Diffuse, rsurface.colormod[0] * diffusescale, rsurface.colormod[1] * diffusescale, rsurface.colormod[2] * diffusescale);CHECKCGERROR
			if (r_cg_permutation->fp_Color_Specular) cgGLSetParameter3f(r_cg_permutation->fp_Color_Specular, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale);CHECKCGERROR

			// additive passes are only darkened by fog, not tinted
			if (r_cg_permutation->fp_FogColor) cgGLSetParameter3f(r_cg_permutation->fp_FogColor, 0, 0, 0);CHECKCGERROR
			if (r_cg_permutation->fp_SpecularPower) cgGLSetParameter1f(r_cg_permutation->fp_SpecularPower, rsurface.texture->specularpower * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));CHECKCGERROR
		}
		else
		{
			if (mode == SHADERMODE_FLATCOLOR)
			{
				if (r_cg_permutation->fp_Color_Ambient) cgGLSetParameter3f(r_cg_permutation->fp_Color_Ambient, rsurface.colormod[0], rsurface.colormod[1], rsurface.colormod[2]);CHECKCGERROR
			}
			else if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				if (r_cg_permutation->fp_Color_Ambient) cgGLSetParameter3f(r_cg_permutation->fp_Color_Ambient, (r_refdef.scene.ambient + rsurface.modellight_ambient[0] * r_refdef.lightmapintensity) * rsurface.colormod[0], (r_refdef.scene.ambient + rsurface.modellight_ambient[1] * r_refdef.lightmapintensity) * rsurface.colormod[1], (r_refdef.scene.ambient + rsurface.modellight_ambient[2] * r_refdef.lightmapintensity) * rsurface.colormod[2]);CHECKCGERROR
				if (r_cg_permutation->fp_Color_Diffuse) cgGLSetParameter3f(r_cg_permutation->fp_Color_Diffuse, r_refdef.lightmapintensity * rsurface.colormod[0], r_refdef.lightmapintensity * rsurface.colormod[1], r_refdef.lightmapintensity * rsurface.colormod[2]);CHECKCGERROR
				if (r_cg_permutation->fp_Color_Specular) cgGLSetParameter3f(r_cg_permutation->fp_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);CHECKCGERROR
				if (r_cg_permutation->fp_DeferredMod_Diffuse) cgGLSetParameter3f(r_cg_permutation->fp_DeferredMod_Diffuse, rsurface.colormod[0] * r_shadow_deferred_8bitrange.value, rsurface.colormod[1] * r_shadow_deferred_8bitrange.value, rsurface.colormod[2] * r_shadow_deferred_8bitrange.value);CHECKCGERROR
				if (r_cg_permutation->fp_DeferredMod_Specular) cgGLSetParameter3f(r_cg_permutation->fp_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);CHECKCGERROR
				if (r_cg_permutation->fp_LightColor) cgGLSetParameter3f(r_cg_permutation->fp_LightColor, rsurface.modellight_diffuse[0], rsurface.modellight_diffuse[1], rsurface.modellight_diffuse[2]);CHECKCGERROR
				if (r_cg_permutation->fp_LightDir) cgGLSetParameter3f(r_cg_permutation->fp_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);CHECKCGERROR
			}
			else
			{
				if (r_cg_permutation->fp_Color_Ambient) cgGLSetParameter3f(r_cg_permutation->fp_Color_Ambient, r_refdef.scene.ambient * rsurface.colormod[0], r_refdef.scene.ambient * rsurface.colormod[1], r_refdef.scene.ambient * rsurface.colormod[2]);CHECKCGERROR
				if (r_cg_permutation->fp_Color_Diffuse) cgGLSetParameter3f(r_cg_permutation->fp_Color_Diffuse, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2]);CHECKCGERROR
				if (r_cg_permutation->fp_Color_Specular) cgGLSetParameter3f(r_cg_permutation->fp_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);CHECKCGERROR
				if (r_cg_permutation->fp_DeferredMod_Diffuse) cgGLSetParameter3f(r_cg_permutation->fp_DeferredMod_Diffuse, rsurface.colormod[0] * diffusescale * r_shadow_deferred_8bitrange.value, rsurface.colormod[1] * diffusescale * r_shadow_deferred_8bitrange.value, rsurface.colormod[2] * diffusescale * r_shadow_deferred_8bitrange.value);CHECKCGERROR
				if (r_cg_permutation->fp_DeferredMod_Specular) cgGLSetParameter3f(r_cg_permutation->fp_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);CHECKCGERROR
			}
			// additive passes are only darkened by fog, not tinted
			if (r_cg_permutation->fp_FogColor)
			{
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ADD)
					cgGLSetParameter3f(r_cg_permutation->fp_FogColor, 0, 0, 0);
				else
					cgGLSetParameter3f(r_cg_permutation->fp_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
				CHECKCGERROR
			}
			if (r_cg_permutation->fp_DistortScaleRefractReflect) cgGLSetParameter4f(r_cg_permutation->fp_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);CHECKCGERROR
			if (r_cg_permutation->fp_ScreenScaleRefractReflect) cgGLSetParameter4f(r_cg_permutation->fp_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);CHECKCGERROR
			if (r_cg_permutation->fp_ScreenCenterRefractReflect) cgGLSetParameter4f(r_cg_permutation->fp_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);CHECKCGERROR
			if (r_cg_permutation->fp_RefractColor) cgGLSetParameter4fv(r_cg_permutation->fp_RefractColor, rsurface.texture->refractcolor4f);CHECKCGERROR
			if (r_cg_permutation->fp_ReflectColor) cgGLSetParameter4fv(r_cg_permutation->fp_ReflectColor, rsurface.texture->reflectcolor4f);CHECKCGERROR
			if (r_cg_permutation->fp_ReflectFactor) cgGLSetParameter1f(r_cg_permutation->fp_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);CHECKCGERROR
			if (r_cg_permutation->fp_ReflectOffset) cgGLSetParameter1f(r_cg_permutation->fp_ReflectOffset, rsurface.texture->reflectmin);CHECKCGERROR
			if (r_cg_permutation->fp_SpecularPower) cgGLSetParameter1f(r_cg_permutation->fp_SpecularPower, rsurface.texture->specularpower * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));CHECKCGERROR
		}
		if (r_cg_permutation->fp_ShadowMap_TextureScale) cgGLSetParameter2f(r_cg_permutation->fp_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);CHECKCGERROR
		if (r_cg_permutation->fp_ShadowMap_Parameters) cgGLSetParameter4f(r_cg_permutation->fp_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);CHECKCGERROR
		if (r_cg_permutation->fp_Color_Glow) cgGLSetParameter3f(r_cg_permutation->fp_Color_Glow, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2]);CHECKCGERROR
		if (r_cg_permutation->fp_Alpha) cgGLSetParameter1f(r_cg_permutation->fp_Alpha, rsurface.texture->lightmapcolor[3]);CHECKCGERROR
		if (r_cg_permutation->fp_EyePosition) cgGLSetParameter3f(r_cg_permutation->fp_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);CHECKCGERROR
		if (r_cg_permutation->fp_Color_Pants)
		{
			if (rsurface.texture->pantstexture)
				cgGLSetParameter3f(r_cg_permutation->fp_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
			else
				cgGLSetParameter3f(r_cg_permutation->fp_Color_Pants, 0, 0, 0);
			CHECKCGERROR
		}
		if (r_cg_permutation->fp_Color_Shirt)
		{
			if (rsurface.texture->shirttexture)
				cgGLSetParameter3f(r_cg_permutation->fp_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
			else
				cgGLSetParameter3f(r_cg_permutation->fp_Color_Shirt, 0, 0, 0);
			CHECKCGERROR
		}
		if (r_cg_permutation->fp_FogPlane) cgGLSetParameter4f(r_cg_permutation->fp_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);CHECKCGERROR
		if (r_cg_permutation->fp_FogPlaneViewDist) cgGLSetParameter1f(r_cg_permutation->fp_FogPlaneViewDist, rsurface.fogplaneviewdist);CHECKCGERROR
		if (r_cg_permutation->fp_FogRangeRecip) cgGLSetParameter1f(r_cg_permutation->fp_FogRangeRecip, rsurface.fograngerecip);CHECKCGERROR
		if (r_cg_permutation->fp_FogHeightFade) cgGLSetParameter1f(r_cg_permutation->fp_FogHeightFade, rsurface.fogheightfade);CHECKCGERROR
		if (r_cg_permutation->fp_OffsetMapping_Scale) cgGLSetParameter1f(r_cg_permutation->fp_OffsetMapping_Scale, r_glsl_offsetmapping_scale.value);CHECKCGERROR
		if (r_cg_permutation->fp_ScreenToDepth) cgGLSetParameter2f(r_cg_permutation->fp_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);CHECKCGERROR
		if (r_cg_permutation->fp_PixelToScreenTexCoord) cgGLSetParameter2f(r_cg_permutation->fp_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);CHECKCGERROR

	//	if (r_cg_permutation->fp_Texture_First          ) CG_BindTexture(r_cg_permutation->fp_Texture_First          , r_texture_white                                     );CHECKCGERROR
	//	if (r_cg_permutation->fp_Texture_Second         ) CG_BindTexture(r_cg_permutation->fp_Texture_Second         , r_texture_white                                     );CHECKCGERROR
	//	if (r_cg_permutation->fp_Texture_GammaRamps     ) CG_BindTexture(r_cg_permutation->fp_Texture_GammaRamps     , r_texture_gammaramps                                );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Normal         ) CG_BindTexture(r_cg_permutation->fp_Texture_Normal         , rsurface.texture->nmaptexture                       );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Color          ) CG_BindTexture(r_cg_permutation->fp_Texture_Color          , rsurface.texture->basetexture                       );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Gloss          ) CG_BindTexture(r_cg_permutation->fp_Texture_Gloss          , rsurface.texture->glosstexture                      );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Glow           ) CG_BindTexture(r_cg_permutation->fp_Texture_Glow           , rsurface.texture->glowtexture                       );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_SecondaryNormal) CG_BindTexture(r_cg_permutation->fp_Texture_SecondaryNormal, rsurface.texture->backgroundnmaptexture             );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_SecondaryColor ) CG_BindTexture(r_cg_permutation->fp_Texture_SecondaryColor , rsurface.texture->backgroundbasetexture             );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_SecondaryGloss ) CG_BindTexture(r_cg_permutation->fp_Texture_SecondaryGloss , rsurface.texture->backgroundglosstexture            );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_SecondaryGlow  ) CG_BindTexture(r_cg_permutation->fp_Texture_SecondaryGlow  , rsurface.texture->backgroundglowtexture             );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Pants          ) CG_BindTexture(r_cg_permutation->fp_Texture_Pants          , rsurface.texture->pantstexture                      );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Shirt          ) CG_BindTexture(r_cg_permutation->fp_Texture_Shirt          , rsurface.texture->shirttexture                      );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ReflectMask    ) CG_BindTexture(r_cg_permutation->fp_Texture_ReflectMask    , rsurface.texture->reflectmasktexture                );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ReflectCube    ) CG_BindTexture(r_cg_permutation->fp_Texture_ReflectCube    , rsurface.texture->reflectcubetexture ? rsurface.texture->reflectcubetexture : r_texture_whitecube);CHECKCGERROR
		if (r_cg_permutation->fp_Texture_FogMask        ) CG_BindTexture(r_cg_permutation->fp_Texture_FogMask        , r_texture_fogattenuation                            );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Lightmap       ) CG_BindTexture(r_cg_permutation->fp_Texture_Lightmap       , r_texture_white                                     );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Deluxemap      ) CG_BindTexture(r_cg_permutation->fp_Texture_Deluxemap      , r_texture_blanknormalmap                            );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Attenuation    ) CG_BindTexture(r_cg_permutation->fp_Texture_Attenuation    , r_shadow_attenuationgradienttexture                 );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Refraction     ) CG_BindTexture(r_cg_permutation->fp_Texture_Refraction     , r_texture_white                                     );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Reflection     ) CG_BindTexture(r_cg_permutation->fp_Texture_Reflection     , r_texture_white                                     );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenDepth    ) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenDepth    , r_shadow_prepassgeometrydepthtexture                );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenNormalMap) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenNormalMap, r_shadow_prepassgeometrynormalmaptexture            );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenDiffuse  ) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenDiffuse  , r_shadow_prepasslightingdiffusetexture              );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenSpecular ) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenSpecular , r_shadow_prepasslightingspeculartexture             );CHECKCGERROR
		if (rsurface.rtlight || (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW)))
		{
			if (r_cg_permutation->fp_Texture_ShadowMap2D    ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMap2D    , r_shadow_shadowmap2dtexture                         );CHECKCGERROR
			if (r_cg_permutation->fp_Texture_ShadowMapRect  ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMapRect  , r_shadow_shadowmaprectangletexture                  );CHECKCGERROR
			if (rsurface.rtlight)
			{
				if (r_cg_permutation->fp_Texture_Cube           ) CG_BindTexture(r_cg_permutation->fp_Texture_Cube           , rsurface.rtlight->currentcubemap                    );CHECKCGERROR
				if (r_shadow_usingshadowmapcube)
					if (r_cg_permutation->fp_Texture_ShadowMapCube  ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMapCube  , r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]);CHECKCGERROR
				if (r_cg_permutation->fp_Texture_CubeProjection ) CG_BindTexture(r_cg_permutation->fp_Texture_CubeProjection , r_shadow_shadowmapvsdcttexture                      );CHECKCGERROR
			}
		}

		CHECKGLERROR
#endif
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		break;
	}
}

void R_SetupShader_DeferredLight(const rtlight_t *rtlight)
{
	// select a permutation of the lighting shader appropriate to this
	// combination of texture, entity, light source, and fogging, only use the
	// minimum features necessary to avoid wasting rendering time in the
	// fragment shader on features that are not being used
	unsigned int permutation = 0;
	unsigned int mode = 0;
	const float *lightcolorbase = rtlight->currentcolor;
	float ambientscale = rtlight->ambientscale;
	float diffusescale = rtlight->diffusescale;
	float specularscale = rtlight->specularscale;
	// this is the location of the light in view space
	vec3_t viewlightorigin;
	// this transforms from view space (camera) to light space (cubemap)
	matrix4x4_t viewtolight;
	matrix4x4_t lighttoview;
	float viewtolight16f[16];
	float range = 1.0f / r_shadow_deferred_8bitrange.value;
	// light source
	mode = SHADERMODE_DEFERREDLIGHTSOURCE;
	if (rtlight->currentcubemap != r_texture_whitecube)
		permutation |= SHADERPERMUTATION_CUBEFILTER;
	if (diffusescale > 0)
		permutation |= SHADERPERMUTATION_DIFFUSE;
	if (specularscale > 0)
	{
		permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		if (r_shadow_glossexact.integer)
			permutation |= SHADERPERMUTATION_EXACTSPECULARMATH;
	}
	if (r_shadow_usingshadowmaprect || r_shadow_usingshadowmap2d || r_shadow_usingshadowmapcube)
	{
		if (r_shadow_usingshadowmaprect)
			permutation |= SHADERPERMUTATION_SHADOWMAPRECT;
		if (r_shadow_usingshadowmap2d)
			permutation |= SHADERPERMUTATION_SHADOWMAP2D;
		if (r_shadow_usingshadowmapcube)
			permutation |= SHADERPERMUTATION_SHADOWMAPCUBE;
		else if(r_shadow_shadowmapvsdct)
			permutation |= SHADERPERMUTATION_SHADOWMAPVSDCT;

		if (r_shadow_shadowmapsampler)
			permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
		if (r_shadow_shadowmappcf > 1)
			permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
		else if (r_shadow_shadowmappcf)
			permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
	}
	Matrix4x4_Transform(&r_refdef.view.viewport.viewmatrix, rtlight->shadoworigin, viewlightorigin);
	Matrix4x4_Concat(&lighttoview, &r_refdef.view.viewport.viewmatrix, &rtlight->matrix_lighttoworld);
	Matrix4x4_Invert_Simple(&viewtolight, &lighttoview);
	Matrix4x4_ToArrayFloatGL(&viewtolight, viewtolight16f);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
		R_SetupShader_SetPermutationGLSL(mode, permutation);
		if (r_glsl_permutation->loc_LightPosition             >= 0) qglUniform3fARB(       r_glsl_permutation->loc_LightPosition            , viewlightorigin[0], viewlightorigin[1], viewlightorigin[2]);
		if (r_glsl_permutation->loc_ViewToLight               >= 0) qglUniformMatrix4fvARB(r_glsl_permutation->loc_ViewToLight              , 1, false, viewtolight16f);
		if (r_glsl_permutation->loc_DeferredColor_Ambient     >= 0) qglUniform3fARB(       r_glsl_permutation->loc_DeferredColor_Ambient    , lightcolorbase[0] * ambientscale  * range, lightcolorbase[1] * ambientscale  * range, lightcolorbase[2] * ambientscale  * range);
		if (r_glsl_permutation->loc_DeferredColor_Diffuse     >= 0) qglUniform3fARB(       r_glsl_permutation->loc_DeferredColor_Diffuse    , lightcolorbase[0] * diffusescale  * range, lightcolorbase[1] * diffusescale  * range, lightcolorbase[2] * diffusescale  * range);
		if (r_glsl_permutation->loc_DeferredColor_Specular    >= 0) qglUniform3fARB(       r_glsl_permutation->loc_DeferredColor_Specular   , lightcolorbase[0] * specularscale * range, lightcolorbase[1] * specularscale * range, lightcolorbase[2] * specularscale * range);
		if (r_glsl_permutation->loc_ShadowMap_TextureScale    >= 0) qglUniform2fARB(       r_glsl_permutation->loc_ShadowMap_TextureScale   , r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		if (r_glsl_permutation->loc_ShadowMap_Parameters      >= 0) qglUniform4fARB(       r_glsl_permutation->loc_ShadowMap_Parameters     , r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);
		if (r_glsl_permutation->loc_SpecularPower             >= 0) qglUniform1fARB(       r_glsl_permutation->loc_SpecularPower            , (r_shadow_gloss.integer == 2 ? r_shadow_gloss2exponent.value : r_shadow_glossexponent.value) * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));
		if (r_glsl_permutation->loc_ScreenToDepth             >= 0) qglUniform2fARB(       r_glsl_permutation->loc_ScreenToDepth            , r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		if (r_glsl_permutation->loc_PixelToScreenTexCoord >= 0) qglUniform2fARB(r_glsl_permutation->loc_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);

		if (r_glsl_permutation->loc_Texture_Attenuation       >= 0) R_Mesh_TexBind(GL20TU_ATTENUATION        , r_shadow_attenuationgradienttexture                 );
		if (r_glsl_permutation->loc_Texture_ScreenDepth       >= 0) R_Mesh_TexBind(GL20TU_SCREENDEPTH        , r_shadow_prepassgeometrydepthtexture                );
		if (r_glsl_permutation->loc_Texture_ScreenNormalMap   >= 0) R_Mesh_TexBind(GL20TU_SCREENNORMALMAP    , r_shadow_prepassgeometrynormalmaptexture            );
		if (r_glsl_permutation->loc_Texture_Cube              >= 0) R_Mesh_TexBind(GL20TU_CUBE               , rsurface.rtlight->currentcubemap                    );
		if (r_glsl_permutation->loc_Texture_ShadowMapRect     >= 0) R_Mesh_TexBind(GL20TU_SHADOWMAPRECT      , r_shadow_shadowmaprectangletexture                  );
		if (r_shadow_usingshadowmapcube)
			if (r_glsl_permutation->loc_Texture_ShadowMapCube     >= 0) R_Mesh_TexBind(GL20TU_SHADOWMAPCUBE      , r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]);
		if (r_glsl_permutation->loc_Texture_ShadowMap2D       >= 0) R_Mesh_TexBind(GL20TU_SHADOWMAP2D        , r_shadow_shadowmap2dtexture                         );
		if (r_glsl_permutation->loc_Texture_CubeProjection    >= 0) R_Mesh_TexBind(GL20TU_CUBEPROJECTION     , r_shadow_shadowmapvsdcttexture                      );
		break;
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		R_SetupShader_SetPermutationCG(mode, permutation);
		if (r_cg_permutation->fp_LightPosition            ) cgGLSetParameter3f(r_cg_permutation->fp_LightPosition, viewlightorigin[0], viewlightorigin[1], viewlightorigin[2]);CHECKCGERROR
		if (r_cg_permutation->fp_ViewToLight              ) cgGLSetMatrixParameterfc(r_cg_permutation->fp_ViewToLight, viewtolight16f);CHECKCGERROR
		if (r_cg_permutation->fp_DeferredColor_Ambient    ) cgGLSetParameter3f(r_cg_permutation->fp_DeferredColor_Ambient , lightcolorbase[0] * ambientscale  * range, lightcolorbase[1] * ambientscale  * range, lightcolorbase[2] * ambientscale  * range);CHECKCGERROR
		if (r_cg_permutation->fp_DeferredColor_Diffuse    ) cgGLSetParameter3f(r_cg_permutation->fp_DeferredColor_Diffuse , lightcolorbase[0] * diffusescale  * range, lightcolorbase[1] * diffusescale  * range, lightcolorbase[2] * diffusescale  * range);CHECKCGERROR
		if (r_cg_permutation->fp_DeferredColor_Specular   ) cgGLSetParameter3f(r_cg_permutation->fp_DeferredColor_Specular, lightcolorbase[0] * specularscale * range, lightcolorbase[1] * specularscale * range, lightcolorbase[2] * specularscale * range);CHECKCGERROR
		if (r_cg_permutation->fp_ShadowMap_TextureScale   ) cgGLSetParameter2f(r_cg_permutation->fp_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);CHECKCGERROR
		if (r_cg_permutation->fp_ShadowMap_Parameters     ) cgGLSetParameter4f(r_cg_permutation->fp_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);CHECKCGERROR
		if (r_cg_permutation->fp_SpecularPower            ) cgGLSetParameter1f(r_cg_permutation->fp_SpecularPower, (r_shadow_gloss.integer == 2 ? r_shadow_gloss2exponent.value : r_shadow_glossexponent.value) * ((permutation & SHADERPERMUTATION_EXACTSPECULARMATH) ? 0.25f : 1.0f));CHECKCGERROR
		if (r_cg_permutation->fp_ScreenToDepth            ) cgGLSetParameter2f(r_cg_permutation->fp_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);CHECKCGERROR
		if (r_cg_permutation->fp_PixelToScreenTexCoord    ) cgGLSetParameter2f(r_cg_permutation->fp_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);CHECKCGERROR

		if (r_cg_permutation->fp_Texture_Attenuation      ) CG_BindTexture(r_cg_permutation->fp_Texture_Attenuation    , r_shadow_attenuationgradienttexture                 );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenDepth      ) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenDepth    , r_shadow_prepassgeometrydepthtexture                );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ScreenNormalMap  ) CG_BindTexture(r_cg_permutation->fp_Texture_ScreenNormalMap, r_shadow_prepassgeometrynormalmaptexture            );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Cube             ) CG_BindTexture(r_cg_permutation->fp_Texture_Cube           , rsurface.rtlight->currentcubemap                    );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ShadowMapRect    ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMapRect  , r_shadow_shadowmaprectangletexture                  );CHECKCGERROR
		if (r_shadow_usingshadowmapcube)
			if (r_cg_permutation->fp_Texture_ShadowMapCube    ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMapCube  , r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]);CHECKCGERROR
		if (r_cg_permutation->fp_Texture_ShadowMap2D      ) CG_BindTexture(r_cg_permutation->fp_Texture_ShadowMap2D    , r_shadow_shadowmap2dtexture                         );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_CubeProjection   ) CG_BindTexture(r_cg_permutation->fp_Texture_CubeProjection , r_shadow_shadowmapvsdcttexture                      );CHECKCGERROR
#endif
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		break;
	}
}

#define SKINFRAME_HASH 1024

typedef struct
{
	int loadsequence; // incremented each level change
	memexpandablearray_t array;
	skinframe_t *hash[SKINFRAME_HASH];
}
r_skinframe_t;
r_skinframe_t r_skinframe;

void R_SkinFrame_PrepareForPurge(void)
{
	r_skinframe.loadsequence++;
	// wrap it without hitting zero
	if (r_skinframe.loadsequence >= 200)
		r_skinframe.loadsequence = 1;
}

void R_SkinFrame_MarkUsed(skinframe_t *skinframe)
{
	if (!skinframe)
		return;
	// mark the skinframe as used for the purging code
	skinframe->loadsequence = r_skinframe.loadsequence;
}

void R_SkinFrame_Purge(void)
{
	int i;
	skinframe_t *s;
	for (i = 0;i < SKINFRAME_HASH;i++)
	{
		for (s = r_skinframe.hash[i];s;s = s->next)
		{
			if (s->loadsequence && s->loadsequence != r_skinframe.loadsequence)
			{
				if (s->merged == s->base)
					s->merged = NULL;
				// FIXME: maybe pass a pointer to the pointer to R_PurgeTexture and reset it to NULL inside? [11/29/2007 Black]
				R_PurgeTexture(s->stain );s->stain  = NULL;
				R_PurgeTexture(s->merged);s->merged = NULL;
				R_PurgeTexture(s->base  );s->base   = NULL;
				R_PurgeTexture(s->pants );s->pants  = NULL;
				R_PurgeTexture(s->shirt );s->shirt  = NULL;
				R_PurgeTexture(s->nmap  );s->nmap   = NULL;
				R_PurgeTexture(s->gloss );s->gloss  = NULL;
				R_PurgeTexture(s->glow  );s->glow   = NULL;
				R_PurgeTexture(s->fog   );s->fog    = NULL;
				R_PurgeTexture(s->reflect);s->reflect = NULL;
				s->loadsequence = 0;
			}
		}
	}
}

skinframe_t *R_SkinFrame_FindNextByName( skinframe_t *last, const char *name ) {
	skinframe_t *item;
	char basename[MAX_QPATH];

	Image_StripImageExtension(name, basename, sizeof(basename));

	if( last == NULL ) {
		int hashindex;
		hashindex = CRC_Block((unsigned char *)basename, strlen(basename)) & (SKINFRAME_HASH - 1);
		item = r_skinframe.hash[hashindex];
	} else {
		item = last->next;
	}

	// linearly search through the hash bucket
	for( ; item ; item = item->next ) {
		if( !strcmp( item->basename, basename ) ) {
			return item;
		}
	}
	return NULL;
}

skinframe_t *R_SkinFrame_Find(const char *name, int textureflags, int comparewidth, int compareheight, int comparecrc, qboolean add)
{
	skinframe_t *item;
	int hashindex;
	char basename[MAX_QPATH];

	Image_StripImageExtension(name, basename, sizeof(basename));

	hashindex = CRC_Block((unsigned char *)basename, strlen(basename)) & (SKINFRAME_HASH - 1);
	for (item = r_skinframe.hash[hashindex];item;item = item->next)
		if (!strcmp(item->basename, basename) && item->textureflags == textureflags && item->comparewidth == comparewidth && item->compareheight == compareheight && item->comparecrc == comparecrc)
			break;

	if (!item) {
		rtexture_t *dyntexture;
		// check whether its a dynamic texture
		dyntexture = CL_GetDynTexture( basename );
		if (!add && !dyntexture)
			return NULL;
		item = (skinframe_t *)Mem_ExpandableArray_AllocRecord(&r_skinframe.array);
		memset(item, 0, sizeof(*item));
		strlcpy(item->basename, basename, sizeof(item->basename));
		item->base = dyntexture; // either NULL or dyntexture handle
		item->textureflags = textureflags;
		item->comparewidth = comparewidth;
		item->compareheight = compareheight;
		item->comparecrc = comparecrc;
		item->next = r_skinframe.hash[hashindex];
		r_skinframe.hash[hashindex] = item;
	}
	else if( item->base == NULL )
	{
		rtexture_t *dyntexture;
		// check whether its a dynamic texture
		// this only needs to be done because Purge doesnt delete skinframes - only sets the texture pointers to NULL and we need to restore it before returing.. [11/29/2007 Black]
		dyntexture = CL_GetDynTexture( basename );
		item->base = dyntexture; // either NULL or dyntexture handle
	}

	R_SkinFrame_MarkUsed(item);
	return item;
}

#define R_SKINFRAME_LOAD_AVERAGE_COLORS(cnt, getpixel) \
	{ \
		unsigned long long avgcolor[5], wsum; \
		int pix, comp, w; \
		avgcolor[0] = 0; \
		avgcolor[1] = 0; \
		avgcolor[2] = 0; \
		avgcolor[3] = 0; \
		avgcolor[4] = 0; \
		wsum = 0; \
		for(pix = 0; pix < cnt; ++pix) \
		{ \
			w = 0; \
			for(comp = 0; comp < 3; ++comp) \
				w += getpixel; \
			if(w) /* ignore perfectly black pixels because that is better for model skins */ \
			{ \
				++wsum; \
				/* comp = 3; -- not needed, comp is always 3 when we get here */ \
				w = getpixel; \
				for(comp = 0; comp < 3; ++comp) \
					avgcolor[comp] += getpixel * w; \
				avgcolor[3] += w; \
			} \
			/* comp = 3; -- not needed, comp is always 3 when we get here */ \
			avgcolor[4] += getpixel; \
		} \
		if(avgcolor[3] == 0) /* no pixels seen? even worse */ \
			avgcolor[3] = 1; \
		skinframe->avgcolor[0] = avgcolor[2] / (255.0 * avgcolor[3]); \
		skinframe->avgcolor[1] = avgcolor[1] / (255.0 * avgcolor[3]); \
		skinframe->avgcolor[2] = avgcolor[0] / (255.0 * avgcolor[3]); \
		skinframe->avgcolor[3] = avgcolor[4] / (255.0 * cnt); \
	}

skinframe_t *R_SkinFrame_LoadExternal(const char *name, int textureflags, qboolean complain)
{
	int j;
	unsigned char *pixels;
	unsigned char *bumppixels;
	unsigned char *basepixels = NULL;
	int basepixels_width = 0;
	int basepixels_height = 0;
	skinframe_t *skinframe;
	rtexture_t *ddsbase = NULL;
	qboolean ddshasalpha = false;
	float ddsavgcolor[4];
	char basename[MAX_QPATH];

	if (cls.state == ca_dedicated)
		return NULL;

	// return an existing skinframe if already loaded
	// if loading of the first image fails, don't make a new skinframe as it
	// would cause all future lookups of this to be missing
	skinframe = R_SkinFrame_Find(name, textureflags, 0, 0, 0, false);
	if (skinframe && skinframe->base)
		return skinframe;

	Image_StripImageExtension(name, basename, sizeof(basename));

	// check for DDS texture file first
	if (!r_loaddds || !(ddsbase = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s.dds", basename), textureflags, &ddshasalpha, ddsavgcolor)))
	{
		basepixels = loadimagepixelsbgra(name, complain, true, r_texture_convertsRGB_skin.integer);
		if (basepixels == NULL)
			return NULL;
	}

	if (developer_loading.integer)
		Con_Printf("loading skin \"%s\"\n", name);

	// we've got some pixels to store, so really allocate this new texture now
	if (!skinframe)
		skinframe = R_SkinFrame_Find(name, textureflags, 0, 0, 0, true);
	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = NULL;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = NULL;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;
	skinframe->reflect = NULL;
	skinframe->hasalpha = false;

	if (ddsbase)
	{
		skinframe->base = ddsbase;
		skinframe->hasalpha = ddshasalpha;
		VectorCopy(ddsavgcolor, skinframe->avgcolor);
		if (r_loadfog && skinframe->hasalpha)
			skinframe->fog = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_mask.dds", skinframe->basename), textureflags | TEXF_ALPHA, NULL, NULL);
		//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);
	}
	else
	{
		basepixels_width = image_width;
		basepixels_height = image_height;
		skinframe->base = R_LoadTexture2D (r_main_texturepool, skinframe->basename, basepixels_width, basepixels_height, basepixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (textureflags & TEXF_ALPHA)
		{
			for (j = 3;j < basepixels_width * basepixels_height * 4;j += 4)
			{
				if (basepixels[j] < 255)
				{
					skinframe->hasalpha = true;
					break;
				}
			}
			if (r_loadfog && skinframe->hasalpha)
			{
				// has transparent pixels
				pixels = (unsigned char *)Mem_Alloc(tempmempool, image_width * image_height * 4);
				for (j = 0;j < image_width * image_height * 4;j += 4)
				{
					pixels[j+0] = 255;
					pixels[j+1] = 255;
					pixels[j+2] = 255;
					pixels[j+3] = basepixels[j+3];
				}
				skinframe->fog = R_LoadTexture2D (r_main_texturepool, va("%s_mask", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);
				Mem_Free(pixels);
			}
		}
		R_SKINFRAME_LOAD_AVERAGE_COLORS(basepixels_width * basepixels_height, basepixels[4 * pix + comp]);
		//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->base)
			R_SaveTextureDDSFile(skinframe->base, va("dds/%s.dds", skinframe->basename), true);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->fog)
			R_SaveTextureDDSFile(skinframe->fog, va("dds/%s_mask.dds", skinframe->basename), true);
	}

	if (r_loaddds)
	{
		if (r_loadnormalmap)
			skinframe->nmap = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_norm.dds", skinframe->basename), textureflags | TEXF_ALPHA, NULL, NULL);
		skinframe->glow = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_glow.dds", skinframe->basename), textureflags, NULL, NULL);
		if (r_loadgloss)
			skinframe->gloss = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_gloss.dds", skinframe->basename), textureflags, NULL, NULL);
		skinframe->pants = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_pants.dds", skinframe->basename), textureflags, NULL, NULL);
		skinframe->shirt = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_shirt.dds", skinframe->basename), textureflags, NULL, NULL);
		skinframe->reflect = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_reflect.dds", skinframe->basename), textureflags, NULL, NULL);
	}

	// _norm is the name used by tenebrae and has been adopted as standard
	if (r_loadnormalmap && skinframe->nmap == NULL)
	{
		if ((pixels = loadimagepixelsbgra(va("%s_norm", skinframe->basename), false, false, false)) != NULL)
		{
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | skinframe->textureflags) & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
			pixels = NULL;
		}
		else if (r_shadow_bumpscale_bumpmap.value > 0 && (bumppixels = loadimagepixelsbgra(va("%s_bump", skinframe->basename), false, false, false)) != NULL)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, image_width * image_height * 4);
			Image_HeightmapToNormalmap_BGRA(bumppixels, pixels, image_width, image_height, false, r_shadow_bumpscale_bumpmap.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | skinframe->textureflags) & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
			Mem_Free(bumppixels);
		}
		else if (r_shadow_bumpscale_basetexture.value > 0)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, basepixels_width * basepixels_height * 4);
			Image_HeightmapToNormalmap_BGRA(basepixels, pixels, basepixels_width, basepixels_height, false, r_shadow_bumpscale_basetexture.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), basepixels_width, basepixels_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | skinframe->textureflags) & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
		}
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->nmap)
			R_SaveTextureDDSFile(skinframe->nmap, va("dds/%s_norm.dds", skinframe->basename), true);
	}

	// _luma is supported only for tenebrae compatibility
	// _glow is the preferred name
	if (skinframe->glow == NULL && ((pixels = loadimagepixelsbgra(va("%s_glow",  skinframe->basename), false, false, r_texture_convertsRGB_skin.integer)) || (pixels = loadimagepixelsbgra(va("%s_luma", skinframe->basename), false, false, r_texture_convertsRGB_skin.integer))))
	{
		skinframe->glow = R_LoadTexture2D (r_main_texturepool, va("%s_glow", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_glow.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->glow)
			R_SaveTextureDDSFile(skinframe->glow, va("dds/%s_glow.dds", skinframe->basename), true);
		Mem_Free(pixels);pixels = NULL;
	}

	if (skinframe->gloss == NULL && r_loadgloss && (pixels = loadimagepixelsbgra(va("%s_gloss", skinframe->basename), false, false, r_texture_convertsRGB_skin.integer)))
	{
		skinframe->gloss = R_LoadTexture2D (r_main_texturepool, va("%s_gloss", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_gloss.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->gloss)
			R_SaveTextureDDSFile(skinframe->gloss, va("dds/%s_gloss.dds", skinframe->basename), true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	if (skinframe->pants == NULL && (pixels = loadimagepixelsbgra(va("%s_pants", skinframe->basename), false, false, r_texture_convertsRGB_skin.integer)))
	{
		skinframe->pants = R_LoadTexture2D (r_main_texturepool, va("%s_pants", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->pants)
			R_SaveTextureDDSFile(skinframe->pants, va("dds/%s_pants.dds", skinframe->basename), true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	if (skinframe->shirt == NULL && (pixels = loadimagepixelsbgra(va("%s_shirt", skinframe->basename), false, false, r_texture_convertsRGB_skin.integer)))
	{
		skinframe->shirt = R_LoadTexture2D (r_main_texturepool, va("%s_shirt", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->shirt)
			R_SaveTextureDDSFile(skinframe->shirt, va("dds/%s_shirt.dds", skinframe->basename), true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	if (skinframe->reflect == NULL && (pixels = loadimagepixelsbgra(va("%s_reflect", skinframe->basename), false, false, r_texture_convertsRGB_skin.integer)))
	{
		skinframe->reflect = R_LoadTexture2D (r_main_texturepool, va("%s_reflect", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_reflectmask.integer ? ~0 : ~TEXF_COMPRESS), NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->reflect)
			R_SaveTextureDDSFile(skinframe->reflect, va("dds/%s_reflect.dds", skinframe->basename), true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	if (basepixels)
		Mem_Free(basepixels);

	return skinframe;
}

// this is only used by .spr32 sprites, HL .spr files, HL .bsp files
skinframe_t *R_SkinFrame_LoadInternalBGRA(const char *name, int textureflags, const unsigned char *skindata, int width, int height)
{
	int i;
	unsigned char *temp1, *temp2;
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	// if already loaded just return it, otherwise make a new skinframe
	skinframe = R_SkinFrame_Find(name, textureflags, width, height, skindata ? CRC_Block(skindata, width*height*4) : 0, true);
	if (skinframe && skinframe->base)
		return skinframe;

	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = NULL;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = NULL;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;
	skinframe->reflect = NULL;
	skinframe->hasalpha = false;

	// if no data was provided, then clearly the caller wanted to get a blank skinframe
	if (!skindata)
		return NULL;

	if (developer_loading.integer)
		Con_Printf("loading 32bit skin \"%s\"\n", name);

	if (r_loadnormalmap && r_shadow_bumpscale_basetexture.value > 0)
	{
		temp1 = (unsigned char *)Mem_Alloc(tempmempool, width * height * 8);
		temp2 = temp1 + width * height * 4;
		Image_HeightmapToNormalmap_BGRA(skindata, temp2, width, height, false, r_shadow_bumpscale_basetexture.value);
		skinframe->nmap = R_LoadTexture2D(r_main_texturepool, va("%s_nmap", skinframe->basename), width, height, temp2, TEXTYPE_BGRA, skinframe->textureflags | TEXF_ALPHA, NULL);
		Mem_Free(temp1);
	}
	skinframe->base = skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, TEXTYPE_BGRA, skinframe->textureflags, NULL);
	if (textureflags & TEXF_ALPHA)
	{
		for (i = 3;i < width * height * 4;i += 4)
		{
			if (skindata[i] < 255)
			{
				skinframe->hasalpha = true;
				break;
			}
		}
		if (r_loadfog && skinframe->hasalpha)
		{
			unsigned char *fogpixels = (unsigned char *)Mem_Alloc(tempmempool, width * height * 4);
			memcpy(fogpixels, skindata, width * height * 4);
			for (i = 0;i < width * height * 4;i += 4)
				fogpixels[i] = fogpixels[i+1] = fogpixels[i+2] = 255;
			skinframe->fog = R_LoadTexture2D(r_main_texturepool, va("%s_fog", skinframe->basename), width, height, fogpixels, TEXTYPE_BGRA, skinframe->textureflags, NULL);
			Mem_Free(fogpixels);
		}
	}

	R_SKINFRAME_LOAD_AVERAGE_COLORS(width * height, skindata[4 * pix + comp]);
	//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);

	return skinframe;
}

skinframe_t *R_SkinFrame_LoadInternalQuake(const char *name, int textureflags, int loadpantsandshirt, int loadglowtexture, const unsigned char *skindata, int width, int height)
{
	int i;
	int featuresmask;
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	// if already loaded just return it, otherwise make a new skinframe
	skinframe = R_SkinFrame_Find(name, textureflags, width, height, skindata ? CRC_Block(skindata, width*height) : 0, true);
	if (skinframe && skinframe->base)
		return skinframe;

	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = NULL;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = NULL;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;
	skinframe->reflect = NULL;
	skinframe->hasalpha = false;

	// if no data was provided, then clearly the caller wanted to get a blank skinframe
	if (!skindata)
		return NULL;

	if (developer_loading.integer)
		Con_Printf("loading quake skin \"%s\"\n", name);

	// we actually don't upload anything until the first use, because mdl skins frequently go unused, and are almost never used in both modes (colormapped and non-colormapped)
	skinframe->qpixels = Mem_Alloc(r_main_mempool, width*height);
	memcpy(skinframe->qpixels, skindata, width*height);
	skinframe->qwidth = width;
	skinframe->qheight = height;

	featuresmask = 0;
	for (i = 0;i < width * height;i++)
		featuresmask |= palette_featureflags[skindata[i]];

	skinframe->hasalpha = false;
	skinframe->qhascolormapping = loadpantsandshirt && (featuresmask & (PALETTEFEATURE_PANTS | PALETTEFEATURE_SHIRT));
	skinframe->qgeneratenmap = r_shadow_bumpscale_basetexture.value > 0;
	skinframe->qgeneratemerged = true;
	skinframe->qgeneratebase = skinframe->qhascolormapping;
	skinframe->qgenerateglow = loadglowtexture && (featuresmask & PALETTEFEATURE_GLOW);

	R_SKINFRAME_LOAD_AVERAGE_COLORS(width * height, ((unsigned char *)palette_bgra_complete)[skindata[pix]*4 + comp]);
	//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);

	return skinframe;
}

static void R_SkinFrame_GenerateTexturesFromQPixels(skinframe_t *skinframe, qboolean colormapped)
{
	int width;
	int height;
	unsigned char *skindata;

	if (!skinframe->qpixels)
		return;

	if (!skinframe->qhascolormapping)
		colormapped = false;

	if (colormapped)
	{
		if (!skinframe->qgeneratebase)
			return;
	}
	else
	{
		if (!skinframe->qgeneratemerged)
			return;
	}

	width = skinframe->qwidth;
	height = skinframe->qheight;
	skindata = skinframe->qpixels;

	if (skinframe->qgeneratenmap)
	{
		unsigned char *temp1, *temp2;
		skinframe->qgeneratenmap = false;
		temp1 = (unsigned char *)Mem_Alloc(tempmempool, width * height * 8);
		temp2 = temp1 + width * height * 4;
		// use either a custom palette or the quake palette
		Image_Copy8bitBGRA(skindata, temp1, width * height, palette_bgra_complete);
		Image_HeightmapToNormalmap_BGRA(temp1, temp2, width, height, false, r_shadow_bumpscale_basetexture.value);
		skinframe->nmap = R_LoadTexture2D(r_main_texturepool, va("%s_nmap", skinframe->basename), width, height, temp2, TEXTYPE_BGRA, skinframe->textureflags | TEXF_ALPHA, NULL);
		Mem_Free(temp1);
	}

	if (skinframe->qgenerateglow)
	{
		skinframe->qgenerateglow = false;
		skinframe->glow = R_LoadTexture2D(r_main_texturepool, va("%s_glow", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, palette_bgra_onlyfullbrights); // glow
	}

	if (colormapped)
	{
		skinframe->qgeneratebase = false;
		skinframe->base  = R_LoadTexture2D(r_main_texturepool, va("%s_nospecial", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, skinframe->glow ? palette_bgra_nocolormapnofullbrights : palette_bgra_nocolormap);
		skinframe->pants = R_LoadTexture2D(r_main_texturepool, va("%s_pants", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, palette_bgra_pantsaswhite);
		skinframe->shirt = R_LoadTexture2D(r_main_texturepool, va("%s_shirt", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, palette_bgra_shirtaswhite);
	}
	else
	{
		skinframe->qgeneratemerged = false;
		skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, skinframe->glow ? palette_bgra_nofullbrights : palette_bgra_complete);
	}

	if (!skinframe->qgeneratemerged && !skinframe->qgeneratebase)
	{
		Mem_Free(skinframe->qpixels);
		skinframe->qpixels = NULL;
	}
}

skinframe_t *R_SkinFrame_LoadInternal8bit(const char *name, int textureflags, const unsigned char *skindata, int width, int height, const unsigned int *palette, const unsigned int *alphapalette)
{
	int i;
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	// if already loaded just return it, otherwise make a new skinframe
	skinframe = R_SkinFrame_Find(name, textureflags, width, height, skindata ? CRC_Block(skindata, width*height) : 0, true);
	if (skinframe && skinframe->base)
		return skinframe;

	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = NULL;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = NULL;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;
	skinframe->reflect = NULL;
	skinframe->hasalpha = false;

	// if no data was provided, then clearly the caller wanted to get a blank skinframe
	if (!skindata)
		return NULL;

	if (developer_loading.integer)
		Con_Printf("loading embedded 8bit image \"%s\"\n", name);

	skinframe->base = skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, palette);
	if (textureflags & TEXF_ALPHA)
	{
		for (i = 0;i < width * height;i++)
		{
			if (((unsigned char *)palette)[skindata[i]*4+3] < 255)
			{
				skinframe->hasalpha = true;
				break;
			}
		}
		if (r_loadfog && skinframe->hasalpha)
			skinframe->fog = R_LoadTexture2D(r_main_texturepool, va("%s_fog", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, skinframe->textureflags, alphapalette);
	}

	R_SKINFRAME_LOAD_AVERAGE_COLORS(width * height, ((unsigned char *)palette)[skindata[pix]*4 + comp]);
	//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);

	return skinframe;
}

skinframe_t *R_SkinFrame_LoadMissing(void)
{
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	skinframe = R_SkinFrame_Find("missing", TEXF_FORCENEAREST, 0, 0, 0, true);
	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = NULL;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = NULL;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;
	skinframe->reflect = NULL;
	skinframe->hasalpha = false;

	skinframe->avgcolor[0] = rand() / RAND_MAX;
	skinframe->avgcolor[1] = rand() / RAND_MAX;
	skinframe->avgcolor[2] = rand() / RAND_MAX;
	skinframe->avgcolor[3] = 1;

	return skinframe;
}

//static char *suffix[6] = {"ft", "bk", "rt", "lf", "up", "dn"};
typedef struct suffixinfo_s
{
	char *suffix;
	qboolean flipx, flipy, flipdiagonal;
}
suffixinfo_t;
static suffixinfo_t suffix[3][6] =
{
	{
		{"px",   false, false, false},
		{"nx",   false, false, false},
		{"py",   false, false, false},
		{"ny",   false, false, false},
		{"pz",   false, false, false},
		{"nz",   false, false, false}
	},
	{
		{"posx", false, false, false},
		{"negx", false, false, false},
		{"posy", false, false, false},
		{"negy", false, false, false},
		{"posz", false, false, false},
		{"negz", false, false, false}
	},
	{
		{"rt",    true, false,  true},
		{"lf",   false,  true,  true},
		{"ft",    true,  true, false},
		{"bk",   false, false, false},
		{"up",    true, false,  true},
		{"dn",    true, false,  true}
	}
};

static int componentorder[4] = {0, 1, 2, 3};

rtexture_t *R_LoadCubemap(const char *basename)
{
	int i, j, cubemapsize;
	unsigned char *cubemappixels, *image_buffer;
	rtexture_t *cubemaptexture;
	char name[256];
	// must start 0 so the first loadimagepixels has no requested width/height
	cubemapsize = 0;
	cubemappixels = NULL;
	cubemaptexture = NULL;
	// keep trying different suffix groups (posx, px, rt) until one loads
	for (j = 0;j < 3 && !cubemappixels;j++)
	{
		// load the 6 images in the suffix group
		for (i = 0;i < 6;i++)
		{
			// generate an image name based on the base and and suffix
			dpsnprintf(name, sizeof(name), "%s%s", basename, suffix[j][i].suffix);
			// load it
			if ((image_buffer = loadimagepixelsbgra(name, false, false, r_texture_convertsRGB_cubemap.integer)))
			{
				// an image loaded, make sure width and height are equal
				if (image_width == image_height && (!cubemappixels || image_width == cubemapsize))
				{
					// if this is the first image to load successfully, allocate the cubemap memory
					if (!cubemappixels && image_width >= 1)
					{
						cubemapsize = image_width;
						// note this clears to black, so unavailable sides are black
						cubemappixels = (unsigned char *)Mem_Alloc(tempmempool, 6*cubemapsize*cubemapsize*4);
					}
					// copy the image with any flipping needed by the suffix (px and posx types don't need flipping)
					if (cubemappixels)
						Image_CopyMux(cubemappixels+i*cubemapsize*cubemapsize*4, image_buffer, cubemapsize, cubemapsize, suffix[j][i].flipx, suffix[j][i].flipy, suffix[j][i].flipdiagonal, 4, 4, componentorder);
				}
				else
					Con_Printf("Cubemap image \"%s\" (%ix%i) is not square, OpenGL requires square cubemaps.\n", name, image_width, image_height);
				// free the image
				Mem_Free(image_buffer);
			}
		}
	}
	// if a cubemap loaded, upload it
	if (cubemappixels)
	{
		if (developer_loading.integer)
			Con_Printf("loading cubemap \"%s\"\n", basename);

		cubemaptexture = R_LoadTextureCubeMap(r_main_texturepool, basename, cubemapsize, cubemappixels, TEXTYPE_BGRA, (gl_texturecompression_lightcubemaps.integer ? TEXF_COMPRESS : 0) | TEXF_FORCELINEAR, NULL);
		Mem_Free(cubemappixels);
	}
	else
	{
		Con_DPrintf("failed to load cubemap \"%s\"\n", basename);
		if (developer_loading.integer)
		{
			Con_Printf("(tried tried images ");
			for (j = 0;j < 3;j++)
				for (i = 0;i < 6;i++)
					Con_Printf("%s\"%s%s.tga\"", j + i > 0 ? ", " : "", basename, suffix[j][i].suffix);
			Con_Print(" and was unable to find any of them).\n");
		}
	}
	return cubemaptexture;
}

rtexture_t *R_GetCubemap(const char *basename)
{
	int i;
	for (i = 0;i < r_texture_numcubemaps;i++)
		if (!strcasecmp(r_texture_cubemaps[i].basename, basename))
			return r_texture_cubemaps[i].texture ? r_texture_cubemaps[i].texture : r_texture_whitecube;
	if (i >= MAX_CUBEMAPS)
		return r_texture_whitecube;
	r_texture_numcubemaps++;
	strlcpy(r_texture_cubemaps[i].basename, basename, sizeof(r_texture_cubemaps[i].basename));
	r_texture_cubemaps[i].texture = R_LoadCubemap(r_texture_cubemaps[i].basename);
	return r_texture_cubemaps[i].texture;
}

void R_FreeCubemaps(void)
{
	int i;
	for (i = 0;i < r_texture_numcubemaps;i++)
	{
		if (developer_loading.integer)
			Con_DPrintf("unloading cubemap \"%s\"\n", r_texture_cubemaps[i].basename);
		if (r_texture_cubemaps[i].texture)
			R_FreeTexture(r_texture_cubemaps[i].texture);
	}
	r_texture_numcubemaps = 0;
}

void R_Main_FreeViewCache(void)
{
	if (r_refdef.viewcache.entityvisible)
		Mem_Free(r_refdef.viewcache.entityvisible);
	if (r_refdef.viewcache.world_pvsbits)
		Mem_Free(r_refdef.viewcache.world_pvsbits);
	if (r_refdef.viewcache.world_leafvisible)
		Mem_Free(r_refdef.viewcache.world_leafvisible);
	if (r_refdef.viewcache.world_surfacevisible)
		Mem_Free(r_refdef.viewcache.world_surfacevisible);
	memset(&r_refdef.viewcache, 0, sizeof(r_refdef.viewcache));
}

void R_Main_ResizeViewCache(void)
{
	int numentities = r_refdef.scene.numentities;
	int numclusters = r_refdef.scene.worldmodel ? r_refdef.scene.worldmodel->brush.num_pvsclusters : 1;
	int numclusterbytes = r_refdef.scene.worldmodel ? r_refdef.scene.worldmodel->brush.num_pvsclusterbytes : 1;
	int numleafs = r_refdef.scene.worldmodel ? r_refdef.scene.worldmodel->brush.num_leafs : 1;
	int numsurfaces = r_refdef.scene.worldmodel ? r_refdef.scene.worldmodel->num_surfaces : 1;
	if (r_refdef.viewcache.maxentities < numentities)
	{
		r_refdef.viewcache.maxentities = numentities;
		if (r_refdef.viewcache.entityvisible)
			Mem_Free(r_refdef.viewcache.entityvisible);
		r_refdef.viewcache.entityvisible = Mem_Alloc(r_main_mempool, r_refdef.viewcache.maxentities);
	}
	if (r_refdef.viewcache.world_numclusters != numclusters)
	{
		r_refdef.viewcache.world_numclusters = numclusters;
		r_refdef.viewcache.world_numclusterbytes = numclusterbytes;
		if (r_refdef.viewcache.world_pvsbits)
			Mem_Free(r_refdef.viewcache.world_pvsbits);
		r_refdef.viewcache.world_pvsbits = Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numclusterbytes);
	}
	if (r_refdef.viewcache.world_numleafs != numleafs)
	{
		r_refdef.viewcache.world_numleafs = numleafs;
		if (r_refdef.viewcache.world_leafvisible)
			Mem_Free(r_refdef.viewcache.world_leafvisible);
		r_refdef.viewcache.world_leafvisible = Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numleafs);
	}
	if (r_refdef.viewcache.world_numsurfaces != numsurfaces)
	{
		r_refdef.viewcache.world_numsurfaces = numsurfaces;
		if (r_refdef.viewcache.world_surfacevisible)
			Mem_Free(r_refdef.viewcache.world_surfacevisible);
		r_refdef.viewcache.world_surfacevisible = Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numsurfaces);
	}
}

extern rtexture_t *loadingscreentexture;
void gl_main_start(void)
{
	loadingscreentexture = NULL;
	r_texture_blanknormalmap = NULL;
	r_texture_white = NULL;
	r_texture_grey128 = NULL;
	r_texture_black = NULL;
	r_texture_whitecube = NULL;
	r_texture_normalizationcube = NULL;
	r_texture_fogattenuation = NULL;
	r_texture_gammaramps = NULL;
	r_texture_numcubemaps = 0;

	r_loaddds = vid.support.arb_texture_compression && vid.support.ext_texture_compression_s3tc && r_texture_dds_load.integer;
	r_savedds = vid.support.arb_texture_compression && vid.support.ext_texture_compression_s3tc && r_texture_dds_save.integer;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		Cvar_SetValueQuick(&r_textureunits, vid.texunits);
		Cvar_SetValueQuick(&gl_combine, 1);
		Cvar_SetValueQuick(&r_glsl, 1);
		r_loadnormalmap = true;
		r_loadgloss = true;
		r_loadfog = false;
		break;
	case RENDERPATH_GL13:
		Cvar_SetValueQuick(&r_textureunits, vid.texunits);
		Cvar_SetValueQuick(&gl_combine, 1);
		Cvar_SetValueQuick(&r_glsl, 0);
		r_loadnormalmap = false;
		r_loadgloss = false;
		r_loadfog = true;
		break;
	case RENDERPATH_GL11:
		Cvar_SetValueQuick(&r_textureunits, vid.texunits);
		Cvar_SetValueQuick(&gl_combine, 0);
		Cvar_SetValueQuick(&r_glsl, 0);
		r_loadnormalmap = false;
		r_loadgloss = false;
		r_loadfog = true;
		break;
	}

	R_AnimCache_Free();
	R_FrameData_Reset();

	r_numqueries = 0;
	r_maxqueries = 0;
	memset(r_queries, 0, sizeof(r_queries));

	r_qwskincache = NULL;
	r_qwskincache_size = 0;

	// set up r_skinframe loading system for textures
	memset(&r_skinframe, 0, sizeof(r_skinframe));
	r_skinframe.loadsequence = 1;
	Mem_ExpandableArray_NewArray(&r_skinframe.array, r_main_mempool, sizeof(skinframe_t), 256);

	r_main_texturepool = R_AllocTexturePool();
	R_BuildBlankTextures();
	R_BuildNoTexture();
	if (vid.support.arb_texture_cube_map)
	{
		R_BuildWhiteCube();
		R_BuildNormalizationCube();
	}
	r_texture_fogattenuation = NULL;
	r_texture_gammaramps = NULL;
	//r_texture_fogintensity = NULL;
	memset(&r_bloomstate, 0, sizeof(r_bloomstate));
	memset(&r_waterstate, 0, sizeof(r_waterstate));
	memset(r_glsl_permutationhash, 0, sizeof(r_glsl_permutationhash));
	Mem_ExpandableArray_NewArray(&r_glsl_permutationarray, r_main_mempool, sizeof(r_glsl_permutation_t), 256);
	glslshaderstring = NULL;
#ifdef SUPPORTCG
	memset(r_cg_permutationhash, 0, sizeof(r_cg_permutationhash));
	Mem_ExpandableArray_NewArray(&r_cg_permutationarray, r_main_mempool, sizeof(r_cg_permutation_t), 256);
	cgshaderstring = NULL;
#endif
	memset(&r_svbsp, 0, sizeof (r_svbsp));

	r_refdef.fogmasktable_density = 0;
}

void gl_main_shutdown(void)
{
	R_AnimCache_Free();
	R_FrameData_Reset();

	R_Main_FreeViewCache();

	if (r_maxqueries)
		qglDeleteQueriesARB(r_maxqueries, r_queries);

	r_numqueries = 0;
	r_maxqueries = 0;
	memset(r_queries, 0, sizeof(r_queries));

	r_qwskincache = NULL;
	r_qwskincache_size = 0;

	// clear out the r_skinframe state
	Mem_ExpandableArray_FreeArray(&r_skinframe.array);
	memset(&r_skinframe, 0, sizeof(r_skinframe));

	if (r_svbsp.nodes)
		Mem_Free(r_svbsp.nodes);
	memset(&r_svbsp, 0, sizeof (r_svbsp));
	R_FreeTexturePool(&r_main_texturepool);
	loadingscreentexture = NULL;
	r_texture_blanknormalmap = NULL;
	r_texture_white = NULL;
	r_texture_grey128 = NULL;
	r_texture_black = NULL;
	r_texture_whitecube = NULL;
	r_texture_normalizationcube = NULL;
	r_texture_fogattenuation = NULL;
	r_texture_gammaramps = NULL;
	r_texture_numcubemaps = 0;
	//r_texture_fogintensity = NULL;
	memset(&r_bloomstate, 0, sizeof(r_bloomstate));
	memset(&r_waterstate, 0, sizeof(r_waterstate));
	R_GLSL_Restart_f();
}

extern void CL_ParseEntityLump(char *entitystring);
void gl_main_newmap(void)
{
	// FIXME: move this code to client
	int l;
	char *entities, entname[MAX_QPATH];
	if (r_qwskincache)
		Mem_Free(r_qwskincache);
	r_qwskincache = NULL;
	r_qwskincache_size = 0;
	if (cl.worldmodel)
	{
		strlcpy(entname, cl.worldmodel->name, sizeof(entname));
		l = (int)strlen(entname) - 4;
		if (l >= 0 && !strcmp(entname + l, ".bsp"))
		{
			memcpy(entname + l, ".ent", 5);
			if ((entities = (char *)FS_LoadFile(entname, tempmempool, true, NULL)))
			{
				CL_ParseEntityLump(entities);
				Mem_Free(entities);
				return;
			}
		}
		if (cl.worldmodel->brush.entities)
			CL_ParseEntityLump(cl.worldmodel->brush.entities);
	}
	R_Main_FreeViewCache();

	R_FrameData_Reset();
}

void GL_Main_Init(void)
{
	r_main_mempool = Mem_AllocPool("Renderer", 0, NULL);

	Cmd_AddCommand("r_glsl_restart", R_GLSL_Restart_f, "unloads GLSL shaders, they will then be reloaded as needed");
	Cmd_AddCommand("r_glsl_dumpshader", R_GLSL_DumpShader_f, "dumps the engine internal default.glsl shader into glsl/default.glsl");
	// FIXME: the client should set up r_refdef.fog stuff including the fogmasktable
	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_RegisterVariable (&gl_fogenable);
		Cvar_RegisterVariable (&gl_fogdensity);
		Cvar_RegisterVariable (&gl_fogred);
		Cvar_RegisterVariable (&gl_foggreen);
		Cvar_RegisterVariable (&gl_fogblue);
		Cvar_RegisterVariable (&gl_fogstart);
		Cvar_RegisterVariable (&gl_fogend);
		Cvar_RegisterVariable (&gl_skyclip);
	}
	Cvar_RegisterVariable(&r_motionblur);
	Cvar_RegisterVariable(&r_motionblur_maxblur);
	Cvar_RegisterVariable(&r_motionblur_bmin);
	Cvar_RegisterVariable(&r_motionblur_vmin);
	Cvar_RegisterVariable(&r_motionblur_vmax);
	Cvar_RegisterVariable(&r_motionblur_vcoeff);
	Cvar_RegisterVariable(&r_motionblur_randomize);
	Cvar_RegisterVariable(&r_damageblur);
	Cvar_RegisterVariable(&r_equalize_entities_fullbright);
	Cvar_RegisterVariable(&r_equalize_entities_minambient);
	Cvar_RegisterVariable(&r_equalize_entities_by);
	Cvar_RegisterVariable(&r_equalize_entities_to);
	Cvar_RegisterVariable(&r_depthfirst);
	Cvar_RegisterVariable(&r_useinfinitefarclip);
	Cvar_RegisterVariable(&r_farclip_base);
	Cvar_RegisterVariable(&r_farclip_world);
	Cvar_RegisterVariable(&r_nearclip);
	Cvar_RegisterVariable(&r_showbboxes);
	Cvar_RegisterVariable(&r_showsurfaces);
	Cvar_RegisterVariable(&r_showtris);
	Cvar_RegisterVariable(&r_shownormals);
	Cvar_RegisterVariable(&r_showlighting);
	Cvar_RegisterVariable(&r_showshadowvolumes);
	Cvar_RegisterVariable(&r_showcollisionbrushes);
	Cvar_RegisterVariable(&r_showcollisionbrushes_polygonfactor);
	Cvar_RegisterVariable(&r_showcollisionbrushes_polygonoffset);
	Cvar_RegisterVariable(&r_showdisabledepthtest);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_cullentities_trace);
	Cvar_RegisterVariable(&r_cullentities_trace_samples);
	Cvar_RegisterVariable(&r_cullentities_trace_tempentitysamples);
	Cvar_RegisterVariable(&r_cullentities_trace_enlarge);
	Cvar_RegisterVariable(&r_cullentities_trace_delay);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_shadows_darken);
	Cvar_RegisterVariable(&r_shadows_drawafterrtlighting);
	Cvar_RegisterVariable(&r_shadows_castfrombmodels);
	Cvar_RegisterVariable(&r_shadows_throwdistance);
	Cvar_RegisterVariable(&r_shadows_throwdirection);
	Cvar_RegisterVariable(&r_shadows_focus);
	Cvar_RegisterVariable(&r_shadows_shadowmapscale);
	Cvar_RegisterVariable(&r_q1bsp_skymasking);
	Cvar_RegisterVariable(&r_polygonoffset_submodel_factor);
	Cvar_RegisterVariable(&r_polygonoffset_submodel_offset);
	Cvar_RegisterVariable(&r_polygonoffset_decals_factor);
	Cvar_RegisterVariable(&r_polygonoffset_decals_offset);
	Cvar_RegisterVariable(&r_fog_exp2);
	Cvar_RegisterVariable(&r_drawfog);
	Cvar_RegisterVariable(&r_transparentdepthmasking);
	Cvar_RegisterVariable(&r_texture_dds_load);
	Cvar_RegisterVariable(&r_texture_dds_save);
	Cvar_RegisterVariable(&r_texture_convertsRGB_2d);
	Cvar_RegisterVariable(&r_texture_convertsRGB_skin);
	Cvar_RegisterVariable(&r_texture_convertsRGB_cubemap);
	Cvar_RegisterVariable(&r_texture_convertsRGB_skybox);
	Cvar_RegisterVariable(&r_texture_convertsRGB_particles);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&gl_combine);
	Cvar_RegisterVariable(&r_glsl);
	Cvar_RegisterVariable(&r_glsl_deluxemapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_scale);
	Cvar_RegisterVariable(&r_glsl_postprocess);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec1);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec2);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec3);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec4);
	Cvar_RegisterVariable(&r_water);
	Cvar_RegisterVariable(&r_water_resolutionmultiplier);
	Cvar_RegisterVariable(&r_water_clippingplanebias);
	Cvar_RegisterVariable(&r_water_refractdistort);
	Cvar_RegisterVariable(&r_water_reflectdistort);
	Cvar_RegisterVariable(&r_lerpsprites);
	Cvar_RegisterVariable(&r_lerpmodels);
	Cvar_RegisterVariable(&r_lerplightstyles);
	Cvar_RegisterVariable(&r_waterscroll);
	Cvar_RegisterVariable(&r_bloom);
	Cvar_RegisterVariable(&r_bloom_colorscale);
	Cvar_RegisterVariable(&r_bloom_brighten);
	Cvar_RegisterVariable(&r_bloom_blur);
	Cvar_RegisterVariable(&r_bloom_resolution);
	Cvar_RegisterVariable(&r_bloom_colorexponent);
	Cvar_RegisterVariable(&r_bloom_colorsubtract);
	Cvar_RegisterVariable(&r_hdr);
	Cvar_RegisterVariable(&r_hdr_scenebrightness);
	Cvar_RegisterVariable(&r_hdr_glowintensity);
	Cvar_RegisterVariable(&r_hdr_range);
	Cvar_RegisterVariable(&r_smoothnormals_areaweighting);
	Cvar_RegisterVariable(&developer_texturelogging);
	Cvar_RegisterVariable(&gl_lightmaps);
	Cvar_RegisterVariable(&r_test);
	Cvar_RegisterVariable(&r_batchmode);
	Cvar_RegisterVariable(&r_glsl_saturation);
	Cvar_RegisterVariable(&r_framedatasize);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_TENEBRAE)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);

	Cvar_RegisterVariable(&r_track_sprites);
	Cvar_RegisterVariable(&r_track_sprites_flags);
	Cvar_RegisterVariable(&r_track_sprites_scalew);
	Cvar_RegisterVariable(&r_track_sprites_scaleh);
	Cvar_RegisterVariable(&r_overheadsprites_perspective);
	Cvar_RegisterVariable(&r_overheadsprites_pushback);
}

extern void R_Textures_Init(void);
extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void R_Shadow_Init(void);
extern void R_Sky_Init(void);
extern void GL_Surf_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void gl_backend_init(void);
extern void Sbar_Init(void);
extern void R_LightningBeams_Init(void);
extern void Mod_RenderInit(void);
extern void Font_Init(void);

void Render_Init(void)
{
	gl_backend_init();
	R_Textures_Init();
	GL_Main_Init();
	Font_Init();
	GL_Draw_Init();
	R_Shadow_Init();
	R_Sky_Init();
	GL_Surf_Init();
	Sbar_Init();
	R_Particles_Init();
	R_Explosion_Init();
	R_LightningBeams_Init();
	Mod_RenderInit();
}

/*
===============
GL_Init
===============
*/
extern char *ENGINE_EXTENSIONS;
void GL_Init (void)
{
	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);

	if (!gl_extensions)
		gl_extensions = "";
	if (!gl_platformextensions)
		gl_platformextensions = "";

	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	Con_DPrintf("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_DPrintf("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	VID_CheckExtensions();

	// LordHavoc: report supported extensions
	Con_DPrintf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );

	// clear to black (loading plaque will be seen over this)
	CHECKGLERROR
	qglClearColor(0,0,0,1);CHECKGLERROR
	qglClear(GL_COLOR_BUFFER_BIT);CHECKGLERROR
}

int R_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int i;
	mplane_t *p;
	for (i = 0;i < r_refdef.view.numfrustumplanes;i++)
	{
		// skip nearclip plane, it often culls portals when you are very close, and is almost never useful
		if (i == 4)
			continue;
		p = r_refdef.view.frustum + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

int R_CullBoxCustomPlanes(const vec3_t mins, const vec3_t maxs, int numplanes, const mplane_t *planes)
{
	int i;
	const mplane_t *p;
	for (i = 0;i < numplanes;i++)
	{
		p = planes + i;
		switch(p->signbits)
		{
		default:
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

//==================================================================================

// LordHavoc: this stores temporary data used within the same frame

qboolean r_framedata_failed;
static size_t r_framedata_size;
static size_t r_framedata_current;
static void *r_framedata_base;

void R_FrameData_Reset(void)
{
	if (r_framedata_base)
		Mem_Free(r_framedata_base);
	r_framedata_base = NULL;
	r_framedata_size = 0;
	r_framedata_current = 0;
	r_framedata_failed = false;
}

void R_FrameData_NewFrame(void)
{
	size_t wantedsize;
	if (r_framedata_failed)
		Cvar_SetValueQuick(&r_framedatasize, r_framedatasize.value + 1.0f);
	wantedsize = (size_t)(r_framedatasize.value * 1024*1024);
	wantedsize = bound(65536, wantedsize, 128*1024*1024);
	if (r_framedata_size != wantedsize)
	{
		r_framedata_size = wantedsize;
		if (r_framedata_base)
			Mem_Free(r_framedata_base);
		r_framedata_base = Mem_Alloc(r_main_mempool, r_framedata_size);
	}
	r_framedata_current = 0;
	r_framedata_failed = false;
}

void *R_FrameData_Alloc(size_t size)
{
	void *data;

	// align to 16 byte boundary
	size = (size + 15) & ~15;
	data = (void *)((unsigned char*)r_framedata_base + r_framedata_current);
	r_framedata_current += size;

	// check overflow
	if (r_framedata_current > r_framedata_size)
		r_framedata_failed = true;

	// return NULL on everything after a failure
	if (r_framedata_failed)
		return NULL;

	return data;
}

void *R_FrameData_Store(size_t size, void *data)
{
	void *d = R_FrameData_Alloc(size);
	if (d)
		memcpy(d, data, size);
	return d;
}

//==================================================================================

// LordHavoc: animcache originally written by Echon, rewritten since then

/**
 * Animation cache prevents re-generating mesh data for an animated model
 * multiple times in one frame for lighting, shadowing, reflections, etc.
 */

void R_AnimCache_Free(void)
{
}

void R_AnimCache_ClearCache(void)
{
	int i;
	entity_render_t *ent;

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		ent = r_refdef.scene.entities[i];
		ent->animcache_vertex3f = NULL;
		ent->animcache_normal3f = NULL;
		ent->animcache_svector3f = NULL;
		ent->animcache_tvector3f = NULL;
	}
}

qboolean R_AnimCache_GetEntity(entity_render_t *ent, qboolean wantnormals, qboolean wanttangents)
{
	dp_model_t *model = ent->model;
	int numvertices;
	// see if it's already cached this frame
	if (ent->animcache_vertex3f)
	{
		// add normals/tangents if needed
		if (wantnormals || wanttangents)
		{
			if (ent->animcache_normal3f)
				wantnormals = false;
			if (ent->animcache_svector3f)
				wanttangents = false;
			if (wantnormals || wanttangents)
			{
				numvertices = model->surfmesh.num_vertices;
				if (wantnormals)
					ent->animcache_normal3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
				if (wanttangents)
				{
					ent->animcache_svector3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
					ent->animcache_tvector3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
				}
				if (!r_framedata_failed)
					model->AnimateVertices(model, ent->frameblend, ent->skeleton, NULL, wantnormals ? ent->animcache_normal3f : NULL, wanttangents ? ent->animcache_svector3f : NULL, wanttangents ? ent->animcache_tvector3f : NULL);
			}
		}
	}
	else
	{
		// see if this ent is worth caching
		if (!model || !model->Draw || !model->surfmesh.isanimated || !model->AnimateVertices || (ent->frameblend[0].lerp == 1 && ent->frameblend[0].subframe == 0 && !ent->skeleton))
			return false;
		// get some memory for this entity and generate mesh data
		numvertices = model->surfmesh.num_vertices;
		ent->animcache_vertex3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
		if (wantnormals)
			ent->animcache_normal3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
		if (wanttangents)
		{
			ent->animcache_svector3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
			ent->animcache_tvector3f = R_FrameData_Alloc(sizeof(float[3])*numvertices);
		}
		if (!r_framedata_failed)
			model->AnimateVertices(model, ent->frameblend, ent->skeleton, ent->animcache_vertex3f, ent->animcache_normal3f, ent->animcache_svector3f, ent->animcache_tvector3f);
	}
	return !r_framedata_failed;
}

void R_AnimCache_CacheVisibleEntities(void)
{
	int i;
	qboolean wantnormals = !r_showsurfaces.integer;
	qboolean wanttangents = !r_showsurfaces.integer;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		wanttangents = false;
		break;
	}

	// TODO: thread this
	// NOTE: R_PrepareRTLights() also caches entities

	for (i = 0;i < r_refdef.scene.numentities;i++)
		if (r_refdef.viewcache.entityvisible[i])
			R_AnimCache_GetEntity(r_refdef.scene.entities[i], wantnormals, wanttangents);
}

//==================================================================================

static void R_View_UpdateEntityLighting (void)
{
	int i;
	entity_render_t *ent;
	vec3_t tempdiffusenormal, avg;
	vec_t f, fa, fd, fdd;
	qboolean skipunseen = r_shadows.integer != 1 || R_Shadow_ShadowMappingEnabled();

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		ent = r_refdef.scene.entities[i];

		// skip unseen models
		if (!r_refdef.viewcache.entityvisible[i] && skipunseen)
			continue;

		// skip bsp models
		if (ent->model && ent->model->brush.num_leafs)
		{
			// TODO: use modellight for r_ambient settings on world?
			VectorSet(ent->modellight_ambient, 0, 0, 0);
			VectorSet(ent->modellight_diffuse, 0, 0, 0);
			VectorSet(ent->modellight_lightdir, 0, 0, 1);
			continue;
		}

		// fetch the lighting from the worldmodel data
		VectorClear(ent->modellight_ambient);
		VectorClear(ent->modellight_diffuse);
		VectorClear(tempdiffusenormal);
		if ((ent->flags & RENDER_LIGHT) && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.LightPoint)
		{
			vec3_t org;
			Matrix4x4_OriginFromMatrix(&ent->matrix, org);
			r_refdef.scene.worldmodel->brush.LightPoint(r_refdef.scene.worldmodel, org, ent->modellight_ambient, ent->modellight_diffuse, tempdiffusenormal);
			if(ent->flags & RENDER_EQUALIZE)
			{
				// first fix up ambient lighting...
				if(r_equalize_entities_minambient.value > 0)
				{
					fd = 0.299f * ent->modellight_diffuse[0] + 0.587f * ent->modellight_diffuse[1] + 0.114f * ent->modellight_diffuse[2];
					if(fd > 0)
					{
						fa = (0.299f * ent->modellight_ambient[0] + 0.587f * ent->modellight_ambient[1] + 0.114f * ent->modellight_ambient[2]);
						if(fa < r_equalize_entities_minambient.value * fd)
						{
							// solve:
							//   fa'/fd' = minambient
							//   fa'+0.25*fd' = fa+0.25*fd
							//   ...
							//   fa' = fd' * minambient
							//   fd'*(0.25+minambient) = fa+0.25*fd
							//   ...
							//   fd' = (fa+0.25*fd) * 1 / (0.25+minambient)
							//   fa' = (fa+0.25*fd) * minambient / (0.25+minambient)
							//   ...
							fdd = (fa + 0.25f * fd) / (0.25f + r_equalize_entities_minambient.value);
							f = fdd / fd; // f>0 because all this is additive; f<1 because fdd<fd because this follows from fa < r_equalize_entities_minambient.value * fd
							VectorMA(ent->modellight_ambient, (1-f)*0.25f, ent->modellight_diffuse, ent->modellight_ambient);
							VectorScale(ent->modellight_diffuse, f, ent->modellight_diffuse);
						}
					}
				}

				if(r_equalize_entities_to.value > 0 && r_equalize_entities_by.value != 0)
				{
					VectorMA(ent->modellight_ambient, 0.25f, ent->modellight_diffuse, avg);
					f = 0.299f * avg[0] + 0.587f * avg[1] + 0.114f * avg[2];
					if(f > 0)
					{
						f = pow(f / r_equalize_entities_to.value, -r_equalize_entities_by.value);
						VectorScale(ent->modellight_ambient, f, ent->modellight_ambient);
						VectorScale(ent->modellight_diffuse, f, ent->modellight_diffuse);
					}
				}
			}
		}
		else // highly rare
			VectorSet(ent->modellight_ambient, 1, 1, 1);

		// move the light direction into modelspace coordinates for lighting code
		Matrix4x4_Transform3x3(&ent->inversematrix, tempdiffusenormal, ent->modellight_lightdir);
		if(VectorLength2(ent->modellight_lightdir) == 0)
			VectorSet(ent->modellight_lightdir, 0, 0, 1); // have to set SOME valid vector here
		VectorNormalize(ent->modellight_lightdir);
	}
}

#define MAX_LINEOFSIGHTTRACES 64

static qboolean R_CanSeeBox(int numsamples, vec_t enlarge, vec3_t eye, vec3_t entboxmins, vec3_t entboxmaxs)
{
	int i;
	vec3_t boxmins, boxmaxs;
	vec3_t start;
	vec3_t end;
	dp_model_t *model = r_refdef.scene.worldmodel;

	if (!model || !model->brush.TraceLineOfSight)
		return true;

	// expand the box a little
	boxmins[0] = (enlarge+1) * entboxmins[0] - enlarge * entboxmaxs[0];
	boxmaxs[0] = (enlarge+1) * entboxmaxs[0] - enlarge * entboxmins[0];
	boxmins[1] = (enlarge+1) * entboxmins[1] - enlarge * entboxmaxs[1];
	boxmaxs[1] = (enlarge+1) * entboxmaxs[1] - enlarge * entboxmins[1];
	boxmins[2] = (enlarge+1) * entboxmins[2] - enlarge * entboxmaxs[2];
	boxmaxs[2] = (enlarge+1) * entboxmaxs[2] - enlarge * entboxmins[2];

	// return true if eye is inside enlarged box
	if (BoxesOverlap(boxmins, boxmaxs, eye, eye))
		return true;

	// try center
	VectorCopy(eye, start);
	VectorMAM(0.5f, boxmins, 0.5f, boxmaxs, end);
	if (model->brush.TraceLineOfSight(model, start, end))
		return true;

	// try various random positions
	for (i = 0;i < numsamples;i++)
	{
		VectorSet(end, lhrandom(boxmins[0], boxmaxs[0]), lhrandom(boxmins[1], boxmaxs[1]), lhrandom(boxmins[2], boxmaxs[2]));
		if (model->brush.TraceLineOfSight(model, start, end))
			return true;
	}

	return false;
}


static void R_View_UpdateEntityVisible (void)
{
	int i;
	int renderimask;
	int samples;
	entity_render_t *ent;

	renderimask = r_refdef.envmap                                    ? (RENDER_EXTERIORMODEL | RENDER_VIEWMODEL)
		: r_waterstate.renderingrefraction                       ? (RENDER_EXTERIORMODEL | RENDER_VIEWMODEL)
		: (chase_active.integer || r_waterstate.renderingscene)  ? RENDER_VIEWMODEL
		:                                                          RENDER_EXTERIORMODEL;
	if (!r_drawviewmodel.integer)
		renderimask |= RENDER_VIEWMODEL;
	if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.BoxTouchingVisibleLeafs)
	{
		// worldmodel can check visibility
		memset(r_refdef.viewcache.entityvisible, 0, r_refdef.scene.numentities);
		for (i = 0;i < r_refdef.scene.numentities;i++)
		{
			ent = r_refdef.scene.entities[i];
			if (!(ent->flags & renderimask))
			if (!R_CullBox(ent->mins, ent->maxs) || (ent->model->type == mod_sprite && (ent->model->sprite.sprnum_type == SPR_LABEL || ent->model->sprite.sprnum_type == SPR_LABEL_SCALE)))
			if ((ent->flags & (RENDER_NODEPTHTEST | RENDER_VIEWMODEL)) || r_refdef.scene.worldmodel->brush.BoxTouchingVisibleLeafs(r_refdef.scene.worldmodel, r_refdef.viewcache.world_leafvisible, ent->mins, ent->maxs))
				r_refdef.viewcache.entityvisible[i] = true;
		}
		if(r_cullentities_trace.integer && r_refdef.scene.worldmodel->brush.TraceLineOfSight)
		{
			for (i = 0;i < r_refdef.scene.numentities;i++)
			{
				ent = r_refdef.scene.entities[i];
				if(r_refdef.viewcache.entityvisible[i] && !(ent->flags & (RENDER_VIEWMODEL | RENDER_NOCULL | RENDER_NODEPTHTEST)) && !(ent->model && (ent->model->name[0] == '*')))
				{
					samples = ent->entitynumber ? r_cullentities_trace_samples.integer : r_cullentities_trace_tempentitysamples.integer;
					if (samples < 0)
						continue; // temp entities do pvs only
					if(R_CanSeeBox(samples, r_cullentities_trace_enlarge.value, r_refdef.view.origin, ent->mins, ent->maxs))
						ent->last_trace_visibility = realtime;
					if(ent->last_trace_visibility < realtime - r_cullentities_trace_delay.value)
						r_refdef.viewcache.entityvisible[i] = 0;
				}
			}
		}
	}
	else
	{
		// no worldmodel or it can't check visibility
		for (i = 0;i < r_refdef.scene.numentities;i++)
		{
			ent = r_refdef.scene.entities[i];
			r_refdef.viewcache.entityvisible[i] = !(ent->flags & renderimask) && ((ent->model && ent->model->type == mod_sprite && (ent->model->sprite.sprnum_type == SPR_LABEL || ent->model->sprite.sprnum_type == SPR_LABEL_SCALE)) || !R_CullBox(ent->mins, ent->maxs));
		}
	}
}

/// only used if skyrendermasked, and normally returns false
int R_DrawBrushModelsSky (void)
{
	int i, sky;
	entity_render_t *ent;

	sky = false;
	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		if (!ent->model || !ent->model->DrawSky)
			continue;
		ent->model->DrawSky(ent);
		sky = true;
	}
	return sky;
}

static void R_DrawNoModel(entity_render_t *ent);
static void R_DrawModels(void)
{
	int i;
	entity_render_t *ent;

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		r_refdef.stats.entities++;
		if (ent->model && ent->model->Draw != NULL)
			ent->model->Draw(ent);
		else
			R_DrawNoModel(ent);
	}
}

static void R_DrawModelsDepth(void)
{
	int i;
	entity_render_t *ent;

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		if (ent->model && ent->model->DrawDepth != NULL)
			ent->model->DrawDepth(ent);
	}
}

static void R_DrawModelsDebug(void)
{
	int i;
	entity_render_t *ent;

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		if (ent->model && ent->model->DrawDebug != NULL)
			ent->model->DrawDebug(ent);
	}
}

static void R_DrawModelsAddWaterPlanes(void)
{
	int i;
	entity_render_t *ent;

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		if (ent->model && ent->model->DrawAddWaterPlanes != NULL)
			ent->model->DrawAddWaterPlanes(ent);
	}
}

static void R_View_SetFrustum(void)
{
	int i;
	double slopex, slopey;
	vec3_t forward, left, up, origin;

	// we can't trust r_refdef.view.forward and friends in reflected scenes
	Matrix4x4_ToVectors(&r_refdef.view.matrix, forward, left, up, origin);

#if 0
	r_refdef.view.frustum[0].normal[0] = 0 - 1.0 / r_refdef.view.frustum_x;
	r_refdef.view.frustum[0].normal[1] = 0 - 0;
	r_refdef.view.frustum[0].normal[2] = -1 - 0;
	r_refdef.view.frustum[1].normal[0] = 0 + 1.0 / r_refdef.view.frustum_x;
	r_refdef.view.frustum[1].normal[1] = 0 + 0;
	r_refdef.view.frustum[1].normal[2] = -1 + 0;
	r_refdef.view.frustum[2].normal[0] = 0 - 0;
	r_refdef.view.frustum[2].normal[1] = 0 - 1.0 / r_refdef.view.frustum_y;
	r_refdef.view.frustum[2].normal[2] = -1 - 0;
	r_refdef.view.frustum[3].normal[0] = 0 + 0;
	r_refdef.view.frustum[3].normal[1] = 0 + 1.0 / r_refdef.view.frustum_y;
	r_refdef.view.frustum[3].normal[2] = -1 + 0;
#endif

#if 0
	zNear = r_refdef.nearclip;
	nudge = 1.0 - 1.0 / (1<<23);
	r_refdef.view.frustum[4].normal[0] = 0 - 0;
	r_refdef.view.frustum[4].normal[1] = 0 - 0;
	r_refdef.view.frustum[4].normal[2] = -1 - -nudge;
	r_refdef.view.frustum[4].dist = 0 - -2 * zNear * nudge;
	r_refdef.view.frustum[5].normal[0] = 0 + 0;
	r_refdef.view.frustum[5].normal[1] = 0 + 0;
	r_refdef.view.frustum[5].normal[2] = -1 + -nudge;
	r_refdef.view.frustum[5].dist = 0 + -2 * zNear * nudge;
#endif



#if 0
	r_refdef.view.frustum[0].normal[0] = m[3] - m[0];
	r_refdef.view.frustum[0].normal[1] = m[7] - m[4];
	r_refdef.view.frustum[0].normal[2] = m[11] - m[8];
	r_refdef.view.frustum[0].dist = m[15] - m[12];

	r_refdef.view.frustum[1].normal[0] = m[3] + m[0];
	r_refdef.view.frustum[1].normal[1] = m[7] + m[4];
	r_refdef.view.frustum[1].normal[2] = m[11] + m[8];
	r_refdef.view.frustum[1].dist = m[15] + m[12];

	r_refdef.view.frustum[2].normal[0] = m[3] - m[1];
	r_refdef.view.frustum[2].normal[1] = m[7] - m[5];
	r_refdef.view.frustum[2].normal[2] = m[11] - m[9];
	r_refdef.view.frustum[2].dist = m[15] - m[13];

	r_refdef.view.frustum[3].normal[0] = m[3] + m[1];
	r_refdef.view.frustum[3].normal[1] = m[7] + m[5];
	r_refdef.view.frustum[3].normal[2] = m[11] + m[9];
	r_refdef.view.frustum[3].dist = m[15] + m[13];

	r_refdef.view.frustum[4].normal[0] = m[3] - m[2];
	r_refdef.view.frustum[4].normal[1] = m[7] - m[6];
	r_refdef.view.frustum[4].normal[2] = m[11] - m[10];
	r_refdef.view.frustum[4].dist = m[15] - m[14];

	r_refdef.view.frustum[5].normal[0] = m[3] + m[2];
	r_refdef.view.frustum[5].normal[1] = m[7] + m[6];
	r_refdef.view.frustum[5].normal[2] = m[11] + m[10];
	r_refdef.view.frustum[5].dist = m[15] + m[14];
#endif

	if (r_refdef.view.useperspective)
	{
		slopex = 1.0 / r_refdef.view.frustum_x;
		slopey = 1.0 / r_refdef.view.frustum_y;
		VectorMA(forward, -slopex, left, r_refdef.view.frustum[0].normal);
		VectorMA(forward,  slopex, left, r_refdef.view.frustum[1].normal);
		VectorMA(forward, -slopey, up  , r_refdef.view.frustum[2].normal);
		VectorMA(forward,  slopey, up  , r_refdef.view.frustum[3].normal);
		VectorCopy(forward, r_refdef.view.frustum[4].normal);

		// Leaving those out was a mistake, those were in the old code, and they
		// fix a reproducable bug in this one: frustum culling got fucked up when viewmatrix was an identity matrix
		// I couldn't reproduce it after adding those normalizations. --blub
		VectorNormalize(r_refdef.view.frustum[0].normal);
		VectorNormalize(r_refdef.view.frustum[1].normal);
		VectorNormalize(r_refdef.view.frustum[2].normal);
		VectorNormalize(r_refdef.view.frustum[3].normal);

		// calculate frustum corners, which are used to calculate deformed frustum planes for shadow caster culling
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, forward, -1024 * r_refdef.view.frustum_x, left, -1024 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[0]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, forward,  1024 * r_refdef.view.frustum_x, left, -1024 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[1]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, forward, -1024 * r_refdef.view.frustum_x, left,  1024 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[2]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, forward,  1024 * r_refdef.view.frustum_x, left,  1024 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[3]);

		r_refdef.view.frustum[0].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[0].normal);
		r_refdef.view.frustum[1].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[1].normal);
		r_refdef.view.frustum[2].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[2].normal);
		r_refdef.view.frustum[3].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[3].normal);
		r_refdef.view.frustum[4].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[4].normal) + r_refdef.nearclip;
	}
	else
	{
		VectorScale(left, -r_refdef.view.ortho_x, r_refdef.view.frustum[0].normal);
		VectorScale(left,  r_refdef.view.ortho_x, r_refdef.view.frustum[1].normal);
		VectorScale(up, -r_refdef.view.ortho_y, r_refdef.view.frustum[2].normal);
		VectorScale(up,  r_refdef.view.ortho_y, r_refdef.view.frustum[3].normal);
		VectorCopy(forward, r_refdef.view.frustum[4].normal);
		r_refdef.view.frustum[0].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[0].normal) + r_refdef.view.ortho_x;
		r_refdef.view.frustum[1].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[1].normal) + r_refdef.view.ortho_x;
		r_refdef.view.frustum[2].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[2].normal) + r_refdef.view.ortho_y;
		r_refdef.view.frustum[3].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[3].normal) + r_refdef.view.ortho_y;
		r_refdef.view.frustum[4].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[4].normal) + r_refdef.nearclip;
	}
	r_refdef.view.numfrustumplanes = 5;

	if (r_refdef.view.useclipplane)
	{
		r_refdef.view.numfrustumplanes = 6;
		r_refdef.view.frustum[5] = r_refdef.view.clipplane;
	}

	for (i = 0;i < r_refdef.view.numfrustumplanes;i++)
		PlaneClassify(r_refdef.view.frustum + i);

	// LordHavoc: note to all quake engine coders, Quake had a special case
	// for 90 degrees which assumed a square view (wrong), so I removed it,
	// Quake2 has it disabled as well.

	// rotate R_VIEWFORWARD right by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[0].normal, up, forward, -(90 - r_refdef.fov_x / 2));
	//r_refdef.view.frustum[0].dist = DotProduct (r_refdef.view.origin, frustum[0].normal);
	//PlaneClassify(&frustum[0]);

	// rotate R_VIEWFORWARD left by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[1].normal, up, forward, (90 - r_refdef.fov_x / 2));
	//r_refdef.view.frustum[1].dist = DotProduct (r_refdef.view.origin, frustum[1].normal);
	//PlaneClassify(&frustum[1]);

	// rotate R_VIEWFORWARD up by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[2].normal, left, forward, -(90 - r_refdef.fov_y / 2));
	//r_refdef.view.frustum[2].dist = DotProduct (r_refdef.view.origin, frustum[2].normal);
	//PlaneClassify(&frustum[2]);

	// rotate R_VIEWFORWARD down by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[3].normal, left, forward, (90 - r_refdef.fov_y / 2));
	//r_refdef.view.frustum[3].dist = DotProduct (r_refdef.view.origin, frustum[3].normal);
	//PlaneClassify(&frustum[3]);

	// nearclip plane
	//VectorCopy(forward, r_refdef.view.frustum[4].normal);
	//r_refdef.view.frustum[4].dist = DotProduct (r_refdef.view.origin, frustum[4].normal) + r_nearclip.value;
	//PlaneClassify(&frustum[4]);
}

void R_View_Update(void)
{
	R_Main_ResizeViewCache();
	R_View_SetFrustum();
	R_View_WorldVisibility(r_refdef.view.useclipplane);
	R_View_UpdateEntityVisible();
	R_View_UpdateEntityLighting();
}

void R_SetupView(qboolean allowwaterclippingplane)
{
	const float *customclipplane = NULL;
	float plane[4];
	if (r_refdef.view.useclipplane && allowwaterclippingplane)
	{
		// LordHavoc: couldn't figure out how to make this approach the
		vec_t dist = r_refdef.view.clipplane.dist - r_water_clippingplanebias.value;
		vec_t viewdist = DotProduct(r_refdef.view.origin, r_refdef.view.clipplane.normal);
		if (viewdist < r_refdef.view.clipplane.dist + r_water_clippingplanebias.value)
			dist = r_refdef.view.clipplane.dist;
		plane[0] = r_refdef.view.clipplane.normal[0];
		plane[1] = r_refdef.view.clipplane.normal[1];
		plane[2] = r_refdef.view.clipplane.normal[2];
		plane[3] = dist;
		customclipplane = plane;
	}

	if (!r_refdef.view.useperspective)
		R_Viewport_InitOrtho(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height, -r_refdef.view.ortho_x, -r_refdef.view.ortho_y, r_refdef.view.ortho_x, r_refdef.view.ortho_y, -r_refdef.farclip, r_refdef.farclip, customclipplane);
	else if (vid.stencil && r_useinfinitefarclip.integer)
		R_Viewport_InitPerspectiveInfinite(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height, r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip, customclipplane);
	else
		R_Viewport_InitPerspective(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height, r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip, r_refdef.farclip, customclipplane);
	R_SetViewport(&r_refdef.view.viewport);
}

void R_EntityMatrix(const matrix4x4_t *matrix)
{
	if (gl_modelmatrixchanged || memcmp(matrix, &gl_modelmatrix, sizeof(matrix4x4_t)))
	{
		gl_modelmatrixchanged = false;
		gl_modelmatrix = *matrix;
		Matrix4x4_Concat(&gl_modelviewmatrix, &gl_viewmatrix, &gl_modelmatrix);
		Matrix4x4_Concat(&gl_modelviewprojectionmatrix, &gl_projectionmatrix, &gl_modelviewmatrix);
		Matrix4x4_ToArrayFloatGL(&gl_modelviewmatrix, gl_modelview16f);
		Matrix4x4_ToArrayFloatGL(&gl_modelviewprojectionmatrix, gl_modelviewprojection16f);
		CHECKGLERROR
		switch(vid.renderpath)
		{
		case RENDERPATH_GL20:
			if (r_glsl_permutation && r_glsl_permutation->loc_ModelViewProjectionMatrix >= 0) qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelViewProjectionMatrix, 1, false, gl_modelviewprojection16f);
			if (r_glsl_permutation && r_glsl_permutation->loc_ModelViewMatrix >= 0) qglUniformMatrix4fvARB(r_glsl_permutation->loc_ModelViewMatrix, 1, false, gl_modelview16f);
			qglLoadMatrixf(gl_modelview16f);CHECKGLERROR
			break;
		case RENDERPATH_CGGL:
#ifdef SUPPORTCG
			CHECKCGERROR
			if (r_cg_permutation && r_cg_permutation->vp_ModelViewProjectionMatrix) cgGLSetMatrixParameterfc(r_cg_permutation->vp_ModelViewProjectionMatrix, gl_modelviewprojection16f);CHECKCGERROR
			if (r_cg_permutation && r_cg_permutation->vp_ModelViewMatrix) cgGLSetMatrixParameterfc(r_cg_permutation->vp_ModelViewMatrix, gl_modelview16f);CHECKCGERROR
			qglLoadMatrixf(gl_modelview16f);CHECKGLERROR
#endif
			break;
		case RENDERPATH_GL13:
		case RENDERPATH_GL11:
			qglLoadMatrixf(gl_modelview16f);CHECKGLERROR
			break;
		}
	}
}

void R_ResetViewRendering2D(void)
{
	r_viewport_t viewport;
	DrawQ_Finish();

	// GL is weird because it's bottom to top, r_refdef.view.y is top to bottom
	R_Viewport_InitOrtho(&viewport, &identitymatrix, r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height, 0, 0, 1, 1, -10, 100, NULL);
	R_SetViewport(&viewport);
	GL_Scissor(viewport.x, viewport.y, viewport.width, viewport.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_AlphaTest(false);
	GL_ScissorTest(false);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_DepthTest(false);
	R_EntityMatrix(&identitymatrix);
	R_Mesh_ResetTextureState();
	GL_PolygonOffset(0, 0);
	qglEnable(GL_POLYGON_OFFSET_FILL);CHECKGLERROR
	qglDepthFunc(GL_LEQUAL);CHECKGLERROR
	qglDisable(GL_STENCIL_TEST);CHECKGLERROR
	qglStencilMask(~0);CHECKGLERROR
	qglStencilFunc(GL_ALWAYS, 128, ~0);CHECKGLERROR
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);CHECKGLERROR
	GL_CullFace(GL_FRONT); // quake is backwards, this culls back faces
}

void R_ResetViewRendering3D(void)
{
	DrawQ_Finish();

	R_SetupView(true);
	GL_Scissor(r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_AlphaTest(false);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	GL_DepthRange(0, 1);
	GL_DepthTest(true);
	R_EntityMatrix(&identitymatrix);
	R_Mesh_ResetTextureState();
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	qglEnable(GL_POLYGON_OFFSET_FILL);CHECKGLERROR
	qglDepthFunc(GL_LEQUAL);CHECKGLERROR
	qglDisable(GL_STENCIL_TEST);CHECKGLERROR
	qglStencilMask(~0);CHECKGLERROR
	qglStencilFunc(GL_ALWAYS, 128, ~0);CHECKGLERROR
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);CHECKGLERROR
	GL_CullFace(r_refdef.view.cullface_back);
}

void R_RenderScene(void);
void R_RenderWaterPlanes(void);

static void R_Water_StartFrame(void)
{
	int i;
	int waterwidth, waterheight, texturewidth, textureheight;
	r_waterstate_waterplane_t *p;

	if (vid.width > (int)vid.maxtexturesize_2d || vid.height > (int)vid.maxtexturesize_2d)
		return;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		return;
	}

	// set waterwidth and waterheight to the water resolution that will be
	// used (often less than the screen resolution for faster rendering)
	waterwidth = (int)bound(1, vid.width * r_water_resolutionmultiplier.value, vid.width);
	waterheight = (int)bound(1, vid.height * r_water_resolutionmultiplier.value, vid.height);

	// calculate desired texture sizes
	// can't use water if the card does not support the texture size
	if (!r_water.integer || r_showsurfaces.integer)
		texturewidth = textureheight = waterwidth = waterheight = 0;
	else if (vid.support.arb_texture_non_power_of_two)
	{
		texturewidth = waterwidth;
		textureheight = waterheight;
	}
	else
	{
		for (texturewidth   = 1;texturewidth   < waterwidth ;texturewidth   *= 2);
		for (textureheight  = 1;textureheight  < waterheight;textureheight  *= 2);
	}

	// allocate textures as needed
	if (r_waterstate.texturewidth != texturewidth || r_waterstate.textureheight != textureheight)
	{
		r_waterstate.maxwaterplanes = MAX_WATERPLANES;
		for (i = 0, p = r_waterstate.waterplanes;i < r_waterstate.maxwaterplanes;i++, p++)
		{
			if (p->texture_refraction)
				R_FreeTexture(p->texture_refraction);
			p->texture_refraction = NULL;
			if (p->texture_reflection)
				R_FreeTexture(p->texture_reflection);
			p->texture_reflection = NULL;
		}
		memset(&r_waterstate, 0, sizeof(r_waterstate));
		r_waterstate.texturewidth = texturewidth;
		r_waterstate.textureheight = textureheight;
	}

	if (r_waterstate.texturewidth)
	{
		r_waterstate.enabled = true;

		// when doing a reduced render (HDR) we want to use a smaller area
		r_waterstate.waterwidth = (int)bound(1, r_refdef.view.width * r_water_resolutionmultiplier.value, r_refdef.view.width);
		r_waterstate.waterheight = (int)bound(1, r_refdef.view.height * r_water_resolutionmultiplier.value, r_refdef.view.height);

		// set up variables that will be used in shader setup
		r_waterstate.screenscale[0] = 0.5f * (float)r_waterstate.waterwidth / (float)r_waterstate.texturewidth;
		r_waterstate.screenscale[1] = 0.5f * (float)r_waterstate.waterheight / (float)r_waterstate.textureheight;
		r_waterstate.screencenter[0] = 0.5f * (float)r_waterstate.waterwidth / (float)r_waterstate.texturewidth;
		r_waterstate.screencenter[1] = 0.5f * (float)r_waterstate.waterheight / (float)r_waterstate.textureheight;
	}

	r_waterstate.maxwaterplanes = MAX_WATERPLANES;
	r_waterstate.numwaterplanes = 0;
}

void R_Water_AddWaterPlane(msurface_t *surface)
{
	int triangleindex, planeindex;
	const int *e;
	vec3_t vert[3];
	vec3_t normal;
	vec3_t center;
	mplane_t plane;
	r_waterstate_waterplane_t *p;
	texture_t *t = R_GetCurrentTexture(surface->texture);
	// just use the first triangle with a valid normal for any decisions
	VectorClear(normal);
	for (triangleindex = 0, e = rsurface.modelelement3i + surface->num_firsttriangle * 3;triangleindex < surface->num_triangles;triangleindex++, e += 3)
	{
		Matrix4x4_Transform(&rsurface.matrix, rsurface.modelvertex3f + e[0]*3, vert[0]);
		Matrix4x4_Transform(&rsurface.matrix, rsurface.modelvertex3f + e[1]*3, vert[1]);
		Matrix4x4_Transform(&rsurface.matrix, rsurface.modelvertex3f + e[2]*3, vert[2]);
		TriangleNormal(vert[0], vert[1], vert[2], normal);
		if (VectorLength2(normal) >= 0.001)
			break;
	}

	VectorCopy(normal, plane.normal);
	VectorNormalize(plane.normal);
	plane.dist = DotProduct(vert[0], plane.normal);
	PlaneClassify(&plane);
	if (PlaneDiff(r_refdef.view.origin, &plane) < 0)
	{
		// skip backfaces (except if nocullface is set)
		if (!(t->currentmaterialflags & MATERIALFLAG_NOCULLFACE))
			return;
		VectorNegate(plane.normal, plane.normal);
		plane.dist *= -1;
		PlaneClassify(&plane);
	}


	// find a matching plane if there is one
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
		if (fabs(PlaneDiff(vert[0], &p->plane)) < 1 && fabs(PlaneDiff(vert[1], &p->plane)) < 1 && fabs(PlaneDiff(vert[2], &p->plane)) < 1)
			break;
	if (planeindex >= r_waterstate.maxwaterplanes)
		return; // nothing we can do, out of planes

	// if this triangle does not fit any known plane rendered this frame, add one
	if (planeindex >= r_waterstate.numwaterplanes)
	{
		// store the new plane
		r_waterstate.numwaterplanes++;
		p->plane = plane;
		// clear materialflags and pvs
		p->materialflags = 0;
		p->pvsvalid = false;
	}
	// merge this surface's materialflags into the waterplane
	p->materialflags |= t->currentmaterialflags;
	// merge this surface's PVS into the waterplane
	VectorMAM(0.5f, surface->mins, 0.5f, surface->maxs, center);
	if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION) && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.FatPVS
	 && r_refdef.scene.worldmodel->brush.PointInLeaf && r_refdef.scene.worldmodel->brush.PointInLeaf(r_refdef.scene.worldmodel, center)->clusterindex >= 0)
	{
		r_refdef.scene.worldmodel->brush.FatPVS(r_refdef.scene.worldmodel, center, 2, p->pvsbits, sizeof(p->pvsbits), p->pvsvalid);
		p->pvsvalid = true;
	}
}

static void R_Water_ProcessPlanes(void)
{
	r_refdef_view_t originalview;
	r_refdef_view_t myview;
	int planeindex;
	r_waterstate_waterplane_t *p;

	originalview = r_refdef.view;

	// make sure enough textures are allocated
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			if (!p->texture_refraction)
				p->texture_refraction = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_refraction", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
			if (!p->texture_refraction)
				goto error;
		}

		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION))
		{
			if (!p->texture_reflection)
				p->texture_reflection = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_reflection", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
			if (!p->texture_reflection)
				goto error;
		}
	}

	// render views
	r_refdef.view = originalview;
	r_refdef.view.showdebug = false;
	r_refdef.view.width = r_waterstate.waterwidth;
	r_refdef.view.height = r_waterstate.waterheight;
	r_refdef.view.useclipplane = true;
	myview = r_refdef.view;
	r_waterstate.renderingscene = true;
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION))
		{
			r_refdef.view = myview;
			// render reflected scene and copy into texture
			Matrix4x4_Reflect(&r_refdef.view.matrix, p->plane.normal[0], p->plane.normal[1], p->plane.normal[2], p->plane.dist, -2);
			// update the r_refdef.view.origin because otherwise the sky renders at the wrong location (amongst other problems)
			Matrix4x4_OriginFromMatrix(&r_refdef.view.matrix, r_refdef.view.origin);
			r_refdef.view.clipplane = p->plane;
			// reverse the cullface settings for this render
			r_refdef.view.cullface_front = GL_FRONT;
			r_refdef.view.cullface_back = GL_BACK;
			if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.num_pvsclusterbytes)
			{
				r_refdef.view.usecustompvs = true;
				if (p->pvsvalid)
					memcpy(r_refdef.viewcache.world_pvsbits, p->pvsbits, r_refdef.scene.worldmodel->brush.num_pvsclusterbytes);
				else
					memset(r_refdef.viewcache.world_pvsbits, 0xFF, r_refdef.scene.worldmodel->brush.num_pvsclusterbytes);
			}

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			R_View_Update();
			R_RenderScene();

			R_Mesh_CopyToTexture(p->texture_reflection, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
		}

		// render the normal view scene and copy into texture
		// (except that a clipping plane should be used to hide everything on one side of the water, and the viewer's weapon model should be omitted)
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			r_waterstate.renderingrefraction = true;
			r_refdef.view = myview;
			r_refdef.view.clipplane = p->plane;
			VectorNegate(r_refdef.view.clipplane.normal, r_refdef.view.clipplane.normal);
			r_refdef.view.clipplane.dist = -r_refdef.view.clipplane.dist;
			PlaneClassify(&r_refdef.view.clipplane);

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			R_View_Update();
			R_RenderScene();

			R_Mesh_CopyToTexture(p->texture_refraction, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
			r_waterstate.renderingrefraction = false;
		}

	}
	r_waterstate.renderingscene = false;
	r_refdef.view = originalview;
	R_ResetViewRendering3D();
	R_ClearScreen(r_refdef.fogenabled);
	R_View_Update();
	return;
error:
	r_refdef.view = originalview;
	r_waterstate.renderingscene = false;
	Cvar_SetValueQuick(&r_water, 0);
	Con_Printf("R_Water_ProcessPlanes: Error: texture creation failed!  Turned off r_water.\n");
	return;
}

void R_Bloom_StartFrame(void)
{
	int bloomtexturewidth, bloomtextureheight, screentexturewidth, screentextureheight;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		return;
	}

	// set bloomwidth and bloomheight to the bloom resolution that will be
	// used (often less than the screen resolution for faster rendering)
	r_bloomstate.bloomwidth = bound(1, r_bloom_resolution.integer, vid.height);
	r_bloomstate.bloomheight = r_bloomstate.bloomwidth * vid.height / vid.width;
	r_bloomstate.bloomheight = bound(1, r_bloomstate.bloomheight, vid.height);
	r_bloomstate.bloomwidth = bound(1, r_bloomstate.bloomwidth, (int)vid.maxtexturesize_2d);
	r_bloomstate.bloomheight = bound(1, r_bloomstate.bloomheight, (int)vid.maxtexturesize_2d);

	// calculate desired texture sizes
	if (vid.support.arb_texture_non_power_of_two)
	{
		screentexturewidth = r_refdef.view.width;
		screentextureheight = r_refdef.view.height;
		bloomtexturewidth = r_bloomstate.bloomwidth;
		bloomtextureheight = r_bloomstate.bloomheight;
	}
	else
	{
		for (screentexturewidth  = 1;screentexturewidth  < vid.width               ;screentexturewidth  *= 2);
		for (screentextureheight = 1;screentextureheight < vid.height              ;screentextureheight *= 2);
		for (bloomtexturewidth   = 1;bloomtexturewidth   < r_bloomstate.bloomwidth ;bloomtexturewidth   *= 2);
		for (bloomtextureheight  = 1;bloomtextureheight  < r_bloomstate.bloomheight;bloomtextureheight  *= 2);
	}

	if ((r_hdr.integer || r_bloom.integer || (!R_Stereo_Active() && (r_motionblur.value > 0 || r_damageblur.value > 0))) && ((r_bloom_resolution.integer < 4 || r_bloom_blur.value < 1 || r_bloom_blur.value >= 512) || r_refdef.view.width > (int)vid.maxtexturesize_2d || r_refdef.view.height > (int)vid.maxtexturesize_2d))
	{
		Cvar_SetValueQuick(&r_hdr, 0);
		Cvar_SetValueQuick(&r_bloom, 0);
		Cvar_SetValueQuick(&r_motionblur, 0);
		Cvar_SetValueQuick(&r_damageblur, 0);
	}

	if (!(r_glsl_postprocess.integer || (!R_Stereo_ColorMasking() && r_glsl_saturation.value != 1) || (v_glslgamma.integer && !vid_gammatables_trivial)) && !r_bloom.integer && !r_hdr.integer && (R_Stereo_Active() || (r_motionblur.value <= 0 && r_damageblur.value <= 0)))
		screentexturewidth = screentextureheight = 0;
	if (!r_hdr.integer && !r_bloom.integer)
		bloomtexturewidth = bloomtextureheight = 0;

	// allocate textures as needed
	if (r_bloomstate.screentexturewidth != screentexturewidth || r_bloomstate.screentextureheight != screentextureheight)
	{
		if (r_bloomstate.texture_screen)
			R_FreeTexture(r_bloomstate.texture_screen);
		r_bloomstate.texture_screen = NULL;
		r_bloomstate.screentexturewidth = screentexturewidth;
		r_bloomstate.screentextureheight = screentextureheight;
		if (r_bloomstate.screentexturewidth && r_bloomstate.screentextureheight)
			r_bloomstate.texture_screen = R_LoadTexture2D(r_main_texturepool, "screen", r_bloomstate.screentexturewidth, r_bloomstate.screentextureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_FORCENEAREST | TEXF_CLAMP, NULL);
	}
	if (r_bloomstate.bloomtexturewidth != bloomtexturewidth || r_bloomstate.bloomtextureheight != bloomtextureheight)
	{
		if (r_bloomstate.texture_bloom)
			R_FreeTexture(r_bloomstate.texture_bloom);
		r_bloomstate.texture_bloom = NULL;
		r_bloomstate.bloomtexturewidth = bloomtexturewidth;
		r_bloomstate.bloomtextureheight = bloomtextureheight;
		if (r_bloomstate.bloomtexturewidth && r_bloomstate.bloomtextureheight)
			r_bloomstate.texture_bloom = R_LoadTexture2D(r_main_texturepool, "bloom", r_bloomstate.bloomtexturewidth, r_bloomstate.bloomtextureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
	}

	// when doing a reduced render (HDR) we want to use a smaller area
	r_bloomstate.bloomwidth = bound(1, r_bloom_resolution.integer, r_refdef.view.height);
	r_bloomstate.bloomheight = r_bloomstate.bloomwidth * r_refdef.view.height / r_refdef.view.width;
	r_bloomstate.bloomheight = bound(1, r_bloomstate.bloomheight, r_refdef.view.height);
	r_bloomstate.bloomwidth = bound(1, r_bloomstate.bloomwidth, r_bloomstate.bloomtexturewidth);
	r_bloomstate.bloomheight = bound(1, r_bloomstate.bloomheight, r_bloomstate.bloomtextureheight);

	// set up a texcoord array for the full resolution screen image
	// (we have to keep this around to copy back during final render)
	r_bloomstate.screentexcoord2f[0] = 0;
	r_bloomstate.screentexcoord2f[1] = (float)r_refdef.view.height    / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[2] = (float)r_refdef.view.width     / (float)r_bloomstate.screentexturewidth;
	r_bloomstate.screentexcoord2f[3] = (float)r_refdef.view.height    / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[4] = (float)r_refdef.view.width     / (float)r_bloomstate.screentexturewidth;
	r_bloomstate.screentexcoord2f[5] = 0;
	r_bloomstate.screentexcoord2f[6] = 0;
	r_bloomstate.screentexcoord2f[7] = 0;

	// set up a texcoord array for the reduced resolution bloom image
	// (which will be additive blended over the screen image)
	r_bloomstate.bloomtexcoord2f[0] = 0;
	r_bloomstate.bloomtexcoord2f[1] = (float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
	r_bloomstate.bloomtexcoord2f[2] = (float)r_bloomstate.bloomwidth  / (float)r_bloomstate.bloomtexturewidth;
	r_bloomstate.bloomtexcoord2f[3] = (float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
	r_bloomstate.bloomtexcoord2f[4] = (float)r_bloomstate.bloomwidth  / (float)r_bloomstate.bloomtexturewidth;
	r_bloomstate.bloomtexcoord2f[5] = 0;
	r_bloomstate.bloomtexcoord2f[6] = 0;
	r_bloomstate.bloomtexcoord2f[7] = 0;

	if (r_hdr.integer || r_bloom.integer)
	{
		r_bloomstate.enabled = true;
		r_bloomstate.hdr = r_hdr.integer != 0;
	}

	R_Viewport_InitOrtho(&r_bloomstate.viewport, &identitymatrix, r_refdef.view.x, vid.height - r_bloomstate.bloomheight - r_refdef.view.y, r_bloomstate.bloomwidth, r_bloomstate.bloomheight, 0, 0, 1, 1, -10, 100, NULL);
}

void R_Bloom_CopyBloomTexture(float colorscale)
{
	r_refdef.stats.bloom++;

	// scale down screen texture to the bloom texture size
	CHECKGLERROR
	R_SetViewport(&r_bloomstate.viewport);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_Color(colorscale, colorscale, colorscale, 1);
	// TODO: optimize with multitexture or GLSL
	R_Mesh_TexCoordPointer(0, 2, r_bloomstate.screentexcoord2f, 0, 0);
	R_SetupShader_Generic(r_bloomstate.texture_screen, NULL, GL_MODULATE, 1);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
	r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

	// we now have a bloom image in the framebuffer
	// copy it into the bloom image texture for later processing
	R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
	r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
}

void R_Bloom_CopyHDRTexture(void)
{
	R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
	r_refdef.stats.bloom_copypixels += r_refdef.view.viewport.width * r_refdef.view.viewport.height;
}

void R_Bloom_MakeTexture(void)
{
	int x, range, dir;
	float xoffset, yoffset, r, brighten;

	r_refdef.stats.bloom++;

	R_ResetViewRendering2D();
	R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);

	// we have a bloom image in the framebuffer
	CHECKGLERROR
	R_SetViewport(&r_bloomstate.viewport);

	for (x = 1;x < min(r_bloom_colorexponent.value, 32);)
	{
		x *= 2;
		r = bound(0, r_bloom_colorexponent.value / x, 1);
		GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		GL_Color(r, r, r, 1);
		R_SetupShader_Generic(r_bloomstate.texture_bloom, NULL, GL_MODULATE, 1);
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

		// copy the vertically blurred bloom view to a texture
		R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
		r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
	}

	range = r_bloom_blur.integer * r_bloomstate.bloomwidth / 320;
	brighten = r_bloom_brighten.value;
	if (r_hdr.integer)
		brighten *= r_hdr_range.value;
	brighten = sqrt(brighten);
	if(range >= 1)
		brighten *= (3 * range) / (2 * range - 1); // compensate for the "dot particle"
	R_SetupShader_Generic(r_bloomstate.texture_bloom, NULL, GL_MODULATE, 1);
	R_Mesh_TexCoordPointer(0, 2, r_bloomstate.offsettexcoord2f, 0, 0);

	for (dir = 0;dir < 2;dir++)
	{
		// blend on at multiple vertical offsets to achieve a vertical blur
		// TODO: do offset blends using GLSL
		// TODO instead of changing the texcoords, change the target positions to prevent artifacts at edges
		GL_BlendFunc(GL_ONE, GL_ZERO);
		for (x = -range;x <= range;x++)
		{
			if (!dir){xoffset = 0;yoffset = x;}
			else {xoffset = x;yoffset = 0;}
			xoffset /= (float)r_bloomstate.bloomtexturewidth;
			yoffset /= (float)r_bloomstate.bloomtextureheight;
			// compute a texcoord array with the specified x and y offset
			r_bloomstate.offsettexcoord2f[0] = xoffset+0;
			r_bloomstate.offsettexcoord2f[1] = yoffset+(float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
			r_bloomstate.offsettexcoord2f[2] = xoffset+(float)r_bloomstate.bloomwidth / (float)r_bloomstate.bloomtexturewidth;
			r_bloomstate.offsettexcoord2f[3] = yoffset+(float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
			r_bloomstate.offsettexcoord2f[4] = xoffset+(float)r_bloomstate.bloomwidth / (float)r_bloomstate.bloomtexturewidth;
			r_bloomstate.offsettexcoord2f[5] = yoffset+0;
			r_bloomstate.offsettexcoord2f[6] = xoffset+0;
			r_bloomstate.offsettexcoord2f[7] = yoffset+0;
			// this r value looks like a 'dot' particle, fading sharply to
			// black at the edges
			// (probably not realistic but looks good enough)
			//r = ((range*range+1)/((float)(x*x+1)))/(range*2+1);
			//r = brighten/(range*2+1);
			r = brighten / (range * 2 + 1);
			if(range >= 1)
				r *= (1 - x*x/(float)(range*range));
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
			r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}

		// copy the vertically blurred bloom view to a texture
		R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
		r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
	}

	// apply subtract last
	// (just like it would be in a GLSL shader)
	if (r_bloom_colorsubtract.value > 0 && vid.support.ext_blend_subtract)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		R_SetupShader_Generic(r_bloomstate.texture_bloom, NULL, GL_MODULATE, 1);
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		GL_Color(1, 1, 1, 1);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

		GL_BlendFunc(GL_ONE, GL_ONE);
		qglBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT);
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		GL_Color(r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, 1);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
		qglBlendEquationEXT(GL_FUNC_ADD_EXT);

		// copy the darkened bloom view to a texture
		R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
		r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
	}
}

void R_HDR_RenderBloomTexture(void)
{
	int oldwidth, oldheight;
	float oldcolorscale;

	oldcolorscale = r_refdef.view.colorscale;
	oldwidth = r_refdef.view.width;
	oldheight = r_refdef.view.height;
	r_refdef.view.width = r_bloomstate.bloomwidth;
	r_refdef.view.height = r_bloomstate.bloomheight;

	// TODO: support GL_EXT_framebuffer_object rather than reusing the framebuffer?  it might improve SLI performance.
	// TODO: add exposure compensation features
	// TODO: add fp16 framebuffer support (using GL_EXT_framebuffer_object)

	r_refdef.view.showdebug = false;
	r_refdef.view.colorscale *= r_bloom_colorscale.value / bound(1, r_hdr_range.value, 16);

	R_ResetViewRendering3D();

	R_ClearScreen(r_refdef.fogenabled);
	if (r_timereport_active)
		R_TimeReport("HDRclear");

	R_View_Update();
	if (r_timereport_active)
		R_TimeReport("visibility");

	// only do secondary renders with HDR if r_hdr is 2 or higher
	r_waterstate.numwaterplanes = 0;
	if (r_waterstate.enabled && r_hdr.integer >= 2)
		R_RenderWaterPlanes();

	r_refdef.view.showdebug = true;
	R_RenderScene();
	r_waterstate.numwaterplanes = 0;

	R_ResetViewRendering2D();

	R_Bloom_CopyHDRTexture();
	R_Bloom_MakeTexture();

	// restore the view settings
	r_refdef.view.width = oldwidth;
	r_refdef.view.height = oldheight;
	r_refdef.view.colorscale = oldcolorscale;

	R_ResetViewRendering3D();

	R_ClearScreen(r_refdef.fogenabled);
	if (r_timereport_active)
		R_TimeReport("viewclear");
}

static void R_BlendView(void)
{
	unsigned int permutation;
	float uservecs[4][4];

	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		permutation =
			  (r_bloomstate.texture_bloom ? SHADERPERMUTATION_BLOOM : 0)
			| (r_refdef.viewblend[3] > 0 ? SHADERPERMUTATION_VIEWTINT : 0)
			| ((v_glslgamma.value && !vid_gammatables_trivial) ? SHADERPERMUTATION_GAMMARAMPS : 0)
			| (r_glsl_postprocess.integer ? SHADERPERMUTATION_POSTPROCESSING : 0)
			| ((!R_Stereo_ColorMasking() && r_glsl_saturation.value != 1) ? SHADERPERMUTATION_SATURATION : 0);

		if (r_bloomstate.texture_screen)
		{
			// make sure the buffer is available
			if (r_bloom_blur.value < 1) { Cvar_SetValueQuick(&r_bloom_blur, 1); }

			R_ResetViewRendering2D();
			R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
			R_Mesh_ColorPointer(NULL, 0, 0);

			if(!R_Stereo_Active() && (r_motionblur.value > 0 || r_damageblur.value > 0))
			{
				// declare variables
				float speed;
				static float avgspeed;

				speed = VectorLength(cl.movement_velocity);

				cl.motionbluralpha = bound(0, (cl.time - cl.oldtime) / max(0.001, r_motionblur_vcoeff.value), 1);
				avgspeed = avgspeed * (1 - cl.motionbluralpha) + speed * cl.motionbluralpha;

				speed = (avgspeed - r_motionblur_vmin.value) / max(1, r_motionblur_vmax.value - r_motionblur_vmin.value);
				speed = bound(0, speed, 1);
				speed = speed * (1 - r_motionblur_bmin.value) + r_motionblur_bmin.value;

				// calculate values into a standard alpha
				cl.motionbluralpha = 1 - exp(-
						(
						 (r_motionblur.value * speed / 80)
						 +
						 (r_damageblur.value * (cl.cshifts[CSHIFT_DAMAGE].percent / 1600))
						)
						/
						max(0.0001, cl.time - cl.oldtime) // fps independent
					   );

				cl.motionbluralpha *= lhrandom(1 - r_motionblur_randomize.value, 1 + r_motionblur_randomize.value);
				cl.motionbluralpha = bound(0, cl.motionbluralpha, r_motionblur_maxblur.value);
				// apply the blur
				if (cl.motionbluralpha > 0)
				{
					GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					GL_Color(1, 1, 1, cl.motionbluralpha);
					R_SetupShader_Generic(r_bloomstate.texture_screen, NULL, GL_MODULATE, 1);
					R_Mesh_TexCoordPointer(0, 2, r_bloomstate.screentexcoord2f, 0, 0);
					R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
					r_refdef.stats.bloom_drawpixels += r_refdef.view.viewport.width * r_refdef.view.viewport.height;
				}
			}

			// copy view into the screen texture
			R_Mesh_CopyToTexture(r_bloomstate.texture_screen, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
			r_refdef.stats.bloom_copypixels += r_refdef.view.viewport.width * r_refdef.view.viewport.height;
		}
		else if (!r_bloomstate.texture_bloom)
		{
			// we may still have to do view tint...
			if (r_refdef.viewblend[3] >= (1.0f / 256.0f))
			{
				// apply a color tint to the whole view
				R_ResetViewRendering2D();
				R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
				R_Mesh_ColorPointer(NULL, 0, 0);
				R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
				R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
			}
			break; // no screen processing, no bloom, skip it
		}

		if (r_bloomstate.texture_bloom && !r_bloomstate.hdr)
		{
			// render simple bloom effect
			// copy the screen and shrink it and darken it for the bloom process
			R_Bloom_CopyBloomTexture(r_bloom_colorscale.value);
			// make the bloom texture
			R_Bloom_MakeTexture();
		}

#if _MSC_VER >= 1400
#define sscanf sscanf_s
#endif
		memset(uservecs, 0, sizeof(uservecs));
		sscanf(r_glsl_postprocess_uservec1.string, "%f %f %f %f", &uservecs[0][0], &uservecs[0][1], &uservecs[0][2], &uservecs[0][3]);
		sscanf(r_glsl_postprocess_uservec2.string, "%f %f %f %f", &uservecs[1][0], &uservecs[1][1], &uservecs[1][2], &uservecs[1][3]);
		sscanf(r_glsl_postprocess_uservec3.string, "%f %f %f %f", &uservecs[2][0], &uservecs[2][1], &uservecs[2][2], &uservecs[2][3]);
		sscanf(r_glsl_postprocess_uservec4.string, "%f %f %f %f", &uservecs[3][0], &uservecs[3][1], &uservecs[3][2], &uservecs[3][3]);

		R_ResetViewRendering2D();
		R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_Color(1, 1, 1, 1);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.screentexcoord2f, 0, 0);
		R_Mesh_TexCoordPointer(1, 2, r_bloomstate.bloomtexcoord2f, 0, 0);

		switch(vid.renderpath)
		{
		case RENDERPATH_GL20:
			R_SetupShader_SetPermutationGLSL(SHADERMODE_POSTPROCESS, permutation);
			if (r_glsl_permutation->loc_Texture_First      >= 0) R_Mesh_TexBind(GL20TU_FIRST     , r_bloomstate.texture_screen);
			if (r_glsl_permutation->loc_Texture_Second     >= 0) R_Mesh_TexBind(GL20TU_SECOND    , r_bloomstate.texture_bloom );
			if (r_glsl_permutation->loc_Texture_GammaRamps >= 0) R_Mesh_TexBind(GL20TU_GAMMARAMPS, r_texture_gammaramps       );
			if (r_glsl_permutation->loc_ViewTintColor      >= 0) qglUniform4fARB(r_glsl_permutation->loc_ViewTintColor     , r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			if (r_glsl_permutation->loc_PixelSize          >= 0) qglUniform2fARB(r_glsl_permutation->loc_PixelSize         , 1.0/r_bloomstate.screentexturewidth, 1.0/r_bloomstate.screentextureheight);
			if (r_glsl_permutation->loc_UserVec1           >= 0) qglUniform4fARB(r_glsl_permutation->loc_UserVec1          , uservecs[0][0], uservecs[0][1], uservecs[0][2], uservecs[0][3]);
			if (r_glsl_permutation->loc_UserVec2           >= 0) qglUniform4fARB(r_glsl_permutation->loc_UserVec2          , uservecs[1][0], uservecs[1][1], uservecs[1][2], uservecs[1][3]);
			if (r_glsl_permutation->loc_UserVec3           >= 0) qglUniform4fARB(r_glsl_permutation->loc_UserVec3          , uservecs[2][0], uservecs[2][1], uservecs[2][2], uservecs[2][3]);
			if (r_glsl_permutation->loc_UserVec4           >= 0) qglUniform4fARB(r_glsl_permutation->loc_UserVec4          , uservecs[3][0], uservecs[3][1], uservecs[3][2], uservecs[3][3]);
			if (r_glsl_permutation->loc_Saturation         >= 0) qglUniform1fARB(r_glsl_permutation->loc_Saturation        , r_glsl_saturation.value);
			if (r_glsl_permutation->loc_PixelToScreenTexCoord >= 0) qglUniform2fARB(r_glsl_permutation->loc_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);
			break;
		case RENDERPATH_CGGL:
#ifdef SUPPORTCG
			R_SetupShader_SetPermutationCG(SHADERMODE_POSTPROCESS, permutation);
			if (r_cg_permutation->fp_Texture_First     ) CG_BindTexture(r_cg_permutation->fp_Texture_First     , r_bloomstate.texture_screen);CHECKCGERROR
			if (r_cg_permutation->fp_Texture_Second    ) CG_BindTexture(r_cg_permutation->fp_Texture_Second    , r_bloomstate.texture_bloom );CHECKCGERROR
			if (r_cg_permutation->fp_Texture_GammaRamps) CG_BindTexture(r_cg_permutation->fp_Texture_GammaRamps, r_texture_gammaramps       );CHECKCGERROR
			if (r_cg_permutation->fp_ViewTintColor     ) cgGLSetParameter4f(     r_cg_permutation->fp_ViewTintColor     , r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);CHECKCGERROR
			if (r_cg_permutation->fp_PixelSize         ) cgGLSetParameter2f(     r_cg_permutation->fp_PixelSize         , 1.0/r_bloomstate.screentexturewidth, 1.0/r_bloomstate.screentextureheight);CHECKCGERROR
			if (r_cg_permutation->fp_UserVec1          ) cgGLSetParameter4f(     r_cg_permutation->fp_UserVec1          , uservecs[0][0], uservecs[0][1], uservecs[0][2], uservecs[0][3]);CHECKCGERROR
			if (r_cg_permutation->fp_UserVec2          ) cgGLSetParameter4f(     r_cg_permutation->fp_UserVec2          , uservecs[1][0], uservecs[1][1], uservecs[1][2], uservecs[1][3]);CHECKCGERROR
			if (r_cg_permutation->fp_UserVec3          ) cgGLSetParameter4f(     r_cg_permutation->fp_UserVec3          , uservecs[2][0], uservecs[2][1], uservecs[2][2], uservecs[2][3]);CHECKCGERROR
			if (r_cg_permutation->fp_UserVec4          ) cgGLSetParameter4f(     r_cg_permutation->fp_UserVec4          , uservecs[3][0], uservecs[3][1], uservecs[3][2], uservecs[3][3]);CHECKCGERROR
			if (r_cg_permutation->fp_Saturation        ) cgGLSetParameter1f(     r_cg_permutation->fp_Saturation        , r_glsl_saturation.value);CHECKCGERROR
			if (r_cg_permutation->fp_PixelToScreenTexCoord) cgGLSetParameter2f(r_cg_permutation->fp_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);CHECKCGERROR
#endif
			break;
		default:
			break;
		}
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_refdef.view.viewport.width * r_refdef.view.viewport.height;
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		if (r_refdef.viewblend[3] >= (1.0f / 256.0f))
		{
			// apply a color tint to the whole view
			R_ResetViewRendering2D();
			R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
			R_Mesh_ColorPointer(NULL, 0, 0);
			R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		}
		break;
	}
}

matrix4x4_t r_waterscrollmatrix;

void R_UpdateFogColor(void) // needs to be called before HDR subrender too, as that changes colorscale!
{
	if (r_refdef.fog_density)
	{
		r_refdef.fogcolor[0] = r_refdef.fog_red;
		r_refdef.fogcolor[1] = r_refdef.fog_green;
		r_refdef.fogcolor[2] = r_refdef.fog_blue;

		Vector4Set(r_refdef.fogplane, 0, 0, 1, -r_refdef.fog_height);
		r_refdef.fogplaneviewdist = DotProduct(r_refdef.fogplane, r_refdef.view.origin) + r_refdef.fogplane[3];
		r_refdef.fogplaneviewabove = r_refdef.fogplaneviewdist >= 0;
		r_refdef.fogheightfade = -0.5f/max(0.125f, r_refdef.fog_fadedepth);

		{
			vec3_t fogvec;
			VectorCopy(r_refdef.fogcolor, fogvec);
			//   color.rgb *= ContrastBoost * SceneBrightness;
			VectorScale(fogvec, r_refdef.view.colorscale, fogvec);
			r_refdef.fogcolor[0] = bound(0.0f, fogvec[0], 1.0f);
			r_refdef.fogcolor[1] = bound(0.0f, fogvec[1], 1.0f);
			r_refdef.fogcolor[2] = bound(0.0f, fogvec[2], 1.0f);
		}
	}
}

void R_UpdateVariables(void)
{
	R_Textures_Frame();

	r_refdef.scene.ambient = r_ambient.value * (1.0f / 64.0f);

	r_refdef.farclip = r_farclip_base.value;
	if (r_refdef.scene.worldmodel)
		r_refdef.farclip += r_refdef.scene.worldmodel->radius * r_farclip_world.value * 2;
	r_refdef.nearclip = bound (0.001f, r_nearclip.value, r_refdef.farclip - 1.0f);

	if (r_shadow_frontsidecasting.integer < 0 || r_shadow_frontsidecasting.integer > 1)
		Cvar_SetValueQuick(&r_shadow_frontsidecasting, 1);
	r_refdef.polygonfactor = 0;
	r_refdef.polygonoffset = 0;
	r_refdef.shadowpolygonfactor = r_refdef.polygonfactor + r_shadow_polygonfactor.value * (r_shadow_frontsidecasting.integer ? 1 : -1);
	r_refdef.shadowpolygonoffset = r_refdef.polygonoffset + r_shadow_polygonoffset.value * (r_shadow_frontsidecasting.integer ? 1 : -1);

	r_refdef.scene.rtworld = r_shadow_realtime_world.integer != 0;
	r_refdef.scene.rtworldshadows = r_shadow_realtime_world_shadows.integer && vid.stencil;
	r_refdef.scene.rtdlight = (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer) && !gl_flashblend.integer && r_dynamic.integer;
	r_refdef.scene.rtdlightshadows = r_refdef.scene.rtdlight && r_shadow_realtime_dlight_shadows.integer && vid.stencil;
	r_refdef.lightmapintensity = r_refdef.scene.rtworld ? r_shadow_realtime_world_lightmaps.value : 1;
	if (r_showsurfaces.integer)
	{
		r_refdef.scene.rtworld = false;
		r_refdef.scene.rtworldshadows = false;
		r_refdef.scene.rtdlight = false;
		r_refdef.scene.rtdlightshadows = false;
		r_refdef.lightmapintensity = 0;
	}

	if (gamemode == GAME_NEHAHRA)
	{
		if (gl_fogenable.integer)
		{
			r_refdef.oldgl_fogenable = true;
			r_refdef.fog_density = gl_fogdensity.value;
			r_refdef.fog_red = gl_fogred.value;
			r_refdef.fog_green = gl_foggreen.value;
			r_refdef.fog_blue = gl_fogblue.value;
			r_refdef.fog_alpha = 1;
			r_refdef.fog_start = 0;
			r_refdef.fog_end = gl_skyclip.value;
			r_refdef.fog_height = 1<<30;
			r_refdef.fog_fadedepth = 128;
		}
		else if (r_refdef.oldgl_fogenable)
		{
			r_refdef.oldgl_fogenable = false;
			r_refdef.fog_density = 0;
			r_refdef.fog_red = 0;
			r_refdef.fog_green = 0;
			r_refdef.fog_blue = 0;
			r_refdef.fog_alpha = 0;
			r_refdef.fog_start = 0;
			r_refdef.fog_end = 0;
			r_refdef.fog_height = 1<<30;
			r_refdef.fog_fadedepth = 128;
		}
	}

	r_refdef.fog_alpha = bound(0, r_refdef.fog_alpha, 1);
	r_refdef.fog_start = max(0, r_refdef.fog_start);
	r_refdef.fog_end = max(r_refdef.fog_start + 0.01, r_refdef.fog_end);

	// R_UpdateFogColor(); // why? R_RenderScene does it anyway

	if (r_refdef.fog_density && r_drawfog.integer)
	{
		r_refdef.fogenabled = true;
		// this is the point where the fog reaches 0.9986 alpha, which we
		// consider a good enough cutoff point for the texture
		// (0.9986 * 256 == 255.6)
		if (r_fog_exp2.integer)
			r_refdef.fogrange = 32 / (r_refdef.fog_density * r_refdef.fog_density) + r_refdef.fog_start;
		else
			r_refdef.fogrange = 2048 / r_refdef.fog_density + r_refdef.fog_start;
		r_refdef.fogrange = bound(r_refdef.fog_start, r_refdef.fogrange, r_refdef.fog_end);
		r_refdef.fograngerecip = 1.0f / r_refdef.fogrange;
		r_refdef.fogmasktabledistmultiplier = FOGMASKTABLEWIDTH * r_refdef.fograngerecip;
		// fog color was already set
		// update the fog texture
		if (r_refdef.fogmasktable_start != r_refdef.fog_start || r_refdef.fogmasktable_alpha != r_refdef.fog_alpha || r_refdef.fogmasktable_density != r_refdef.fog_density || r_refdef.fogmasktable_range != r_refdef.fogrange)
			R_BuildFogTexture();
	}
	else
		r_refdef.fogenabled = false;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		if(v_glslgamma.integer && !vid_gammatables_trivial)
		{
			if(!r_texture_gammaramps || vid_gammatables_serial != r_texture_gammaramps_serial)
			{
				// build GLSL gamma texture
#define RAMPWIDTH 256
				unsigned short ramp[RAMPWIDTH * 3];
				unsigned char rampbgr[RAMPWIDTH][4];
				int i;

				r_texture_gammaramps_serial = vid_gammatables_serial;

				VID_BuildGammaTables(&ramp[0], RAMPWIDTH);
				for(i = 0; i < RAMPWIDTH; ++i)
				{
					rampbgr[i][0] = (unsigned char) (ramp[i + 2 * RAMPWIDTH] * 255.0 / 65535.0 + 0.5);
					rampbgr[i][1] = (unsigned char) (ramp[i + RAMPWIDTH] * 255.0 / 65535.0 + 0.5);
					rampbgr[i][2] = (unsigned char) (ramp[i] * 255.0 / 65535.0 + 0.5);
					rampbgr[i][3] = 0;
				}
				if (r_texture_gammaramps)
				{
					R_UpdateTexture(r_texture_gammaramps, &rampbgr[0][0], 0, 0, RAMPWIDTH, 1);
				}
				else
				{
					r_texture_gammaramps = R_LoadTexture2D(r_main_texturepool, "gammaramps", RAMPWIDTH, 1, &rampbgr[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_PERSISTENT | TEXF_ALLOWUPDATES, NULL);
				}
			}
		}
		else
		{
			// remove GLSL gamma texture
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		break;
	}
}

static r_refdef_scene_type_t r_currentscenetype = RST_CLIENT;
static r_refdef_scene_t r_scenes_store[ RST_COUNT ];
/*
================
R_SelectScene
================
*/
void R_SelectScene( r_refdef_scene_type_t scenetype ) {
	if( scenetype != r_currentscenetype ) {
		// store the old scenetype
		r_scenes_store[ r_currentscenetype ] = r_refdef.scene;
		r_currentscenetype = scenetype;
		// move in the new scene
		r_refdef.scene = r_scenes_store[ r_currentscenetype ];
	}
}

/*
================
R_GetScenePointer
================
*/
r_refdef_scene_t * R_GetScenePointer( r_refdef_scene_type_t scenetype )
{
	// of course, we could also add a qboolean that provides a lock state and a ReleaseScenePointer function..
	if( scenetype == r_currentscenetype ) {
		return &r_refdef.scene;
	} else {
		return &r_scenes_store[ scenetype ];
	}
}

/*
================
R_RenderView
================
*/
void R_RenderView(void)
{
	if (r_timereport_active)
		R_TimeReport("start");
	r_textureframe++; // used only by R_GetCurrentTexture
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity

	if (!r_drawentities.integer)
		r_refdef.scene.numentities = 0;

	R_AnimCache_ClearCache();
	R_FrameData_NewFrame();

	if (r_refdef.view.isoverlay)
	{
		// TODO: FIXME: move this into its own backend function maybe? [2/5/2008 Andreas]
		GL_Clear( GL_DEPTH_BUFFER_BIT );
		R_TimeReport("depthclear");

		r_refdef.view.showdebug = false;

		r_waterstate.enabled = false;
		r_waterstate.numwaterplanes = 0;

		R_RenderScene();

		CHECKGLERROR
		return;
	}

	if (!r_refdef.scene.entities || r_refdef.view.width * r_refdef.view.height == 0 || !r_renderview.integer/* || !r_refdef.scene.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	r_refdef.view.colorscale = r_hdr_scenebrightness.value;

	// break apart the view matrix into vectors for various purposes
	// it is important that this occurs outside the RenderScene function because that can be called from reflection renders, where the vectors come out wrong
	// however the r_refdef.view.origin IS updated in RenderScene intentionally - otherwise the sky renders at the wrong origin, etc
	Matrix4x4_ToVectors(&r_refdef.view.matrix, r_refdef.view.forward, r_refdef.view.left, r_refdef.view.up, r_refdef.view.origin);
	VectorNegate(r_refdef.view.left, r_refdef.view.right);
	// make an inverted copy of the view matrix for tracking sprites
	Matrix4x4_Invert_Simple(&r_refdef.view.inverse_matrix, &r_refdef.view.matrix);

	R_Shadow_UpdateWorldLightSelection();

	R_Bloom_StartFrame();
	R_Water_StartFrame();

	CHECKGLERROR
	if (r_timereport_active)
		R_TimeReport("viewsetup");

	R_ResetViewRendering3D();

	if (r_refdef.view.clear || r_refdef.fogenabled)
	{
		R_ClearScreen(r_refdef.fogenabled);
		if (r_timereport_active)
			R_TimeReport("viewclear");
	}
	r_refdef.view.clear = true;

	// this produces a bloom texture to be used in R_BlendView() later
	if (r_hdr.integer && r_bloomstate.bloomwidth)
	{
		R_HDR_RenderBloomTexture();
		// we have to bump the texture frame again because r_refdef.view.colorscale is cached in the textures
		r_textureframe++; // used only by R_GetCurrentTexture
	}

	r_refdef.view.showdebug = true;

	R_View_Update();
	if (r_timereport_active)
		R_TimeReport("visibility");

	r_waterstate.numwaterplanes = 0;
	if (r_waterstate.enabled)
		R_RenderWaterPlanes();

	R_RenderScene();
	r_waterstate.numwaterplanes = 0;

	R_BlendView();
	if (r_timereport_active)
		R_TimeReport("blendview");

	GL_Scissor(0, 0, vid.width, vid.height);
	GL_ScissorTest(false);
	CHECKGLERROR
}

void R_RenderWaterPlanes(void)
{
	if (cl.csqc_vidvars.drawworld && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawAddWaterPlanes)
	{
		r_refdef.scene.worldmodel->DrawAddWaterPlanes(r_refdef.scene.worldentity);
		if (r_timereport_active)
			R_TimeReport("waterworld");
	}

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	R_DrawModelsAddWaterPlanes();
	if (r_timereport_active)
		R_TimeReport("watermodels");

	if (r_waterstate.numwaterplanes)
	{
		R_Water_ProcessPlanes();
		if (r_timereport_active)
			R_TimeReport("waterscenes");
	}
}

extern void R_DrawLightningBeams (void);
extern void VM_CL_AddPolygonsToMeshQueue (void);
extern void R_DrawPortals (void);
extern cvar_t cl_locs_show;
static void R_DrawLocs(void);
static void R_DrawEntityBBoxes(void);
static void R_DrawModelDecals(void);
extern void R_DrawModelShadows(void);
extern void R_DrawModelShadowMaps(void);
extern cvar_t cl_decals_newsystem;
extern qboolean r_shadow_usingdeferredprepass;
void R_RenderScene(void)
{
	qboolean shadowmapping = false;

	r_refdef.stats.renders++;

	R_UpdateFogColor();

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	R_MeshQueue_BeginScene();

	R_SkyStartFrame();

	Matrix4x4_CreateTranslate(&r_waterscrollmatrix, sin(r_refdef.scene.time) * 0.025 * r_waterscroll.value, sin(r_refdef.scene.time * 0.8f) * 0.025 * r_waterscroll.value, 0);

	if (cl.csqc_vidvars.drawworld)
	{
		// don't let sound skip if going slow
		if (r_refdef.scene.extraupdate)
			S_ExtraUpdate ();

		if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawSky)
		{
			r_refdef.scene.worldmodel->DrawSky(r_refdef.scene.worldentity);
			if (r_timereport_active)
				R_TimeReport("worldsky");
		}

		if (R_DrawBrushModelsSky() && r_timereport_active)
			R_TimeReport("bmodelsky");

		if (skyrendermasked && skyrenderlater)
		{
			// we have to force off the water clipping plane while rendering sky
			R_SetupView(false);
			R_Sky();
			R_SetupView(true);
			if (r_timereport_active)
				R_TimeReport("sky");
		}
	}

	R_AnimCache_CacheVisibleEntities();
	if (r_timereport_active)
		R_TimeReport("animation");

	R_Shadow_PrepareLights();
	if (r_shadows.integer > 0 && r_refdef.lightmapintensity > 0)
		R_Shadow_PrepareModelShadows();
	if (r_timereport_active)
		R_TimeReport("preparelights");

	if (R_Shadow_ShadowMappingEnabled())
		shadowmapping = true;

	if (r_shadow_usingdeferredprepass)
		R_Shadow_DrawPrepass();

	if (r_depthfirst.integer >= 1 && cl.csqc_vidvars.drawworld && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawDepth)
	{
		r_refdef.scene.worldmodel->DrawDepth(r_refdef.scene.worldentity);
		if (r_timereport_active)
			R_TimeReport("worlddepth");
	}
	if (r_depthfirst.integer >= 2)
	{
		R_DrawModelsDepth();
		if (r_timereport_active)
			R_TimeReport("modeldepth");
	}

	if (r_shadows.integer > 0 && shadowmapping && r_refdef.lightmapintensity > 0)
	{
		R_DrawModelShadowMaps();
		R_ResetViewRendering3D();
		// don't let sound skip if going slow
		if (r_refdef.scene.extraupdate)
			S_ExtraUpdate ();
	}

	if (cl.csqc_vidvars.drawworld && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->Draw)
	{
		r_refdef.scene.worldmodel->Draw(r_refdef.scene.worldentity);
		if (r_timereport_active)
			R_TimeReport("world");
	}

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	R_DrawModels();
	if (r_timereport_active)
		R_TimeReport("models");

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	if (r_shadows.integer > 0 && !shadowmapping && !r_shadows_drawafterrtlighting.integer && r_refdef.lightmapintensity > 0)
	{
		R_DrawModelShadows();
		R_ResetViewRendering3D();
		// don't let sound skip if going slow
		if (r_refdef.scene.extraupdate)
			S_ExtraUpdate ();
	}

	if (!r_shadow_usingdeferredprepass)
	{
		R_Shadow_DrawLights();
		if (r_timereport_active)
			R_TimeReport("rtlights");
	}

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	if (r_shadows.integer > 0 && !shadowmapping && r_shadows_drawafterrtlighting.integer && r_refdef.lightmapintensity > 0)
	{
		R_DrawModelShadows();
		R_ResetViewRendering3D();
		// don't let sound skip if going slow
		if (r_refdef.scene.extraupdate)
			S_ExtraUpdate ();
	}

	if (cl.csqc_vidvars.drawworld)
	{
		if (cl_decals_newsystem.integer)
		{
			R_DrawModelDecals();
			if (r_timereport_active)
				R_TimeReport("modeldecals");
		}
		else
		{
			R_DrawDecals();
			if (r_timereport_active)
				R_TimeReport("decals");
		}

		R_DrawParticles();
		if (r_timereport_active)
			R_TimeReport("particles");

		R_DrawExplosions();
		if (r_timereport_active)
			R_TimeReport("explosions");

		R_DrawLightningBeams();
		if (r_timereport_active)
			R_TimeReport("lightning");
	}

	VM_CL_AddPolygonsToMeshQueue();

	if (r_refdef.view.showdebug)
	{
		if (cl_locs_show.integer)
		{
			R_DrawLocs();
			if (r_timereport_active)
				R_TimeReport("showlocs");
		}

		if (r_drawportals.integer)
		{
			R_DrawPortals();
			if (r_timereport_active)
				R_TimeReport("portals");
		}

		if (r_showbboxes.value > 0)
		{
			R_DrawEntityBBoxes();
			if (r_timereport_active)
				R_TimeReport("bboxes");
		}
	}

	R_MeshQueue_RenderTransparent();
	if (r_timereport_active)
		R_TimeReport("drawtrans");

	if (r_refdef.view.showdebug && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawDebug && (r_showtris.value > 0 || r_shownormals.value != 0 || r_showcollisionbrushes.value > 0))
	{
		r_refdef.scene.worldmodel->DrawDebug(r_refdef.scene.worldentity);
		if (r_timereport_active)
			R_TimeReport("worlddebug");
		R_DrawModelsDebug();
		if (r_timereport_active)
			R_TimeReport("modeldebug");
	}

	if (cl.csqc_vidvars.drawworld)
	{
		R_Shadow_DrawCoronas();
		if (r_timereport_active)
			R_TimeReport("coronas");
	}

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	R_ResetViewRendering2D();
}

static const unsigned short bboxelements[36] =
{
	5, 1, 3, 5, 3, 7,
	6, 2, 0, 6, 0, 4,
	7, 3, 2, 7, 2, 6,
	4, 0, 1, 4, 1, 5,
	4, 5, 7, 4, 7, 6,
	1, 0, 2, 1, 2, 3,
};

void R_DrawBBoxMesh(vec3_t mins, vec3_t maxs, float cr, float cg, float cb, float ca)
{
	int i;
	float *v, *c, f1, f2, vertex3f[8*3], color4f[8*4];

	RSurf_ActiveWorldEntity();

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	R_Mesh_ResetTextureState();

	vertex3f[ 0] = mins[0];vertex3f[ 1] = mins[1];vertex3f[ 2] = mins[2]; //
	vertex3f[ 3] = maxs[0];vertex3f[ 4] = mins[1];vertex3f[ 5] = mins[2];
	vertex3f[ 6] = mins[0];vertex3f[ 7] = maxs[1];vertex3f[ 8] = mins[2];
	vertex3f[ 9] = maxs[0];vertex3f[10] = maxs[1];vertex3f[11] = mins[2];
	vertex3f[12] = mins[0];vertex3f[13] = mins[1];vertex3f[14] = maxs[2];
	vertex3f[15] = maxs[0];vertex3f[16] = mins[1];vertex3f[17] = maxs[2];
	vertex3f[18] = mins[0];vertex3f[19] = maxs[1];vertex3f[20] = maxs[2];
	vertex3f[21] = maxs[0];vertex3f[22] = maxs[1];vertex3f[23] = maxs[2];
	R_FillColors(color4f, 8, cr, cg, cb, ca);
	if (r_refdef.fogenabled)
	{
		for (i = 0, v = vertex3f, c = color4f;i < 8;i++, v += 3, c += 4)
		{
			f1 = RSurf_FogVertex(v);
			f2 = 1 - f1;
			c[0] = c[0] * f1 + r_refdef.fogcolor[0] * f2;
			c[1] = c[1] * f1 + r_refdef.fogcolor[1] * f2;
			c[2] = c[2] * f1 + r_refdef.fogcolor[2] * f2;
		}
	}
	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(color4f, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	R_Mesh_Draw(0, 8, 0, 12, NULL, bboxelements, 0, 0);
}

static void R_DrawEntityBBoxes_Callback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i;
	float color[4];
	prvm_edict_t *edict;
	prvm_prog_t *prog_save = prog;

	// this function draws bounding boxes of server entities
	if (!sv.active)
		return;

	GL_CullFace(GL_NONE);
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);

	prog = 0;
	SV_VM_Begin();
	for (i = 0;i < numsurfaces;i++)
	{
		edict = PRVM_EDICT_NUM(surfacelist[i]);
		switch ((int)edict->fields.server->solid)
		{
			case SOLID_NOT:      Vector4Set(color, 1, 1, 1, 0.05);break;
			case SOLID_TRIGGER:  Vector4Set(color, 1, 0, 1, 0.10);break;
			case SOLID_BBOX:     Vector4Set(color, 0, 1, 0, 0.10);break;
			case SOLID_SLIDEBOX: Vector4Set(color, 1, 0, 0, 0.10);break;
			case SOLID_BSP:      Vector4Set(color, 0, 0, 1, 0.05);break;
			default:             Vector4Set(color, 0, 0, 0, 0.50);break;
		}
		color[3] *= r_showbboxes.value;
		color[3] = bound(0, color[3], 1);
		GL_DepthTest(!r_showdisabledepthtest.integer);
		GL_CullFace(r_refdef.view.cullface_front);
		R_DrawBBoxMesh(edict->priv.server->areamins, edict->priv.server->areamaxs, color[0], color[1], color[2], color[3]);
	}
	SV_VM_End();
	prog = prog_save;
}

static void R_DrawEntityBBoxes(void)
{
	int i;
	prvm_edict_t *edict;
	vec3_t center;
	prvm_prog_t *prog_save = prog;

	// this function draws bounding boxes of server entities
	if (!sv.active)
		return;

	prog = 0;
	SV_VM_Begin();
	for (i = 0;i < prog->num_edicts;i++)
	{
		edict = PRVM_EDICT_NUM(i);
		if (edict->priv.server->free)
			continue;
		// exclude the following for now, as they don't live in world coordinate space and can't be solid:
		if(PRVM_EDICTFIELDVALUE(edict, prog->fieldoffsets.tag_entity)->edict != 0)
			continue;
		if(PRVM_EDICTFIELDVALUE(edict, prog->fieldoffsets.viewmodelforclient)->edict != 0)
			continue;
		VectorLerp(edict->priv.server->areamins, 0.5f, edict->priv.server->areamaxs, center);
		R_MeshQueue_AddTransparent(center, R_DrawEntityBBoxes_Callback, (entity_render_t *)NULL, i, (rtlight_t *)NULL);
	}
	SV_VM_End();
	prog = prog_save;
}

static const int nomodelelement3i[24] =
{
	5, 2, 0,
	5, 1, 2,
	5, 0, 3,
	5, 3, 1,
	0, 2, 4,
	2, 1, 4,
	3, 0, 4,
	1, 3, 4
};

static const unsigned short nomodelelement3s[24] =
{
	5, 2, 0,
	5, 1, 2,
	5, 0, 3,
	5, 3, 1,
	0, 2, 4,
	2, 1, 4,
	3, 0, 4,
	1, 3, 4
};

static const float nomodelvertex3f[6*3] =
{
	-16,   0,   0,
	 16,   0,   0,
	  0, -16,   0,
	  0,  16,   0,
	  0,   0, -16,
	  0,   0,  16
};

static const float nomodelcolor4f[6*4] =
{
	0.0f, 0.0f, 0.5f, 1.0f,
	0.0f, 0.0f, 0.5f, 1.0f,
	0.0f, 0.5f, 0.0f, 1.0f,
	0.0f, 0.5f, 0.0f, 1.0f,
	0.5f, 0.0f, 0.0f, 1.0f,
	0.5f, 0.0f, 0.0f, 1.0f
};

void R_DrawNoModel_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i;
	float f1, f2, *c;
	float color4f[6*4];

	RSurf_ActiveCustomEntity(&ent->matrix, &ent->inversematrix, ent->flags, ent->shadertime, ent->colormod[0], ent->colormod[1], ent->colormod[2], ent->alpha, 6, nomodelvertex3f, NULL, NULL, NULL, NULL, nomodelcolor4f, 8, nomodelelement3i, nomodelelement3s, false, false);

	// this is only called once per entity so numsurfaces is always 1, and
	// surfacelist is always {0}, so this code does not handle batches

	if (rsurface.ent_flags & RENDER_ADDITIVE)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (rsurface.colormod[3] < 1)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
	}
	GL_DepthRange(0, (rsurface.ent_flags & RENDER_VIEWMODEL) ? 0.0625 : 1);
	GL_PolygonOffset(rsurface.basepolygonfactor, rsurface.basepolygonoffset);
	GL_DepthTest(!(rsurface.ent_flags & RENDER_NODEPTHTEST));
	GL_CullFace((rsurface.ent_flags & RENDER_DOUBLESIDED) ? GL_NONE : r_refdef.view.cullface_back);
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	R_Mesh_VertexPointer(rsurface.vertex3f, rsurface.vertex3f_bufferobject, rsurface.vertex3f_bufferoffset);
	memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
	R_Mesh_ColorPointer(color4f, 0, 0);
	for (i = 0, c = color4f;i < 6;i++, c += 4)
	{
		c[0] *= rsurface.colormod[0];
		c[1] *= rsurface.colormod[1];
		c[2] *= rsurface.colormod[2];
		c[3] *= rsurface.colormod[3];
	}
	if (r_refdef.fogenabled)
	{
		for (i = 0, c = color4f;i < 6;i++, c += 4)
		{
			f1 = RSurf_FogVertex(rsurface.vertex3f + 3*i);
			f2 = 1 - f1;
			c[0] = (c[0] * f1 + r_refdef.fogcolor[0] * f2);
			c[1] = (c[1] * f1 + r_refdef.fogcolor[1] * f2);
			c[2] = (c[2] * f1 + r_refdef.fogcolor[2] * f2);
		}
	}
	R_Mesh_ResetTextureState();
	R_Mesh_Draw(0, 6, 0, 8, nomodelelement3i, nomodelelement3s, 0, 0);
}

void R_DrawNoModel(entity_render_t *ent)
{
	vec3_t org;
	Matrix4x4_OriginFromMatrix(&ent->matrix, org);
	if ((ent->flags & RENDER_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->flags & RENDER_NODEPTHTEST ? r_refdef.view.origin : org, R_DrawNoModel_TransparentCallback, ent, 0, rsurface.rtlight);
	else
		R_DrawNoModel_TransparentCallback(ent, rsurface.rtlight, 0, NULL);
}

void R_CalcBeam_Vertex3f (float *vert, const vec3_t org1, const vec3_t org2, float width)
{
	vec3_t right1, right2, diff, normal;

	VectorSubtract (org2, org1, normal);

	// calculate 'right' vector for start
	VectorSubtract (r_refdef.view.origin, org1, diff);
	CrossProduct (normal, diff, right1);
	VectorNormalize (right1);

	// calculate 'right' vector for end
	VectorSubtract (r_refdef.view.origin, org2, diff);
	CrossProduct (normal, diff, right2);
	VectorNormalize (right2);

	vert[ 0] = org1[0] + width * right1[0];
	vert[ 1] = org1[1] + width * right1[1];
	vert[ 2] = org1[2] + width * right1[2];
	vert[ 3] = org1[0] - width * right1[0];
	vert[ 4] = org1[1] - width * right1[1];
	vert[ 5] = org1[2] - width * right1[2];
	vert[ 6] = org2[0] - width * right2[0];
	vert[ 7] = org2[1] - width * right2[1];
	vert[ 8] = org2[2] - width * right2[2];
	vert[ 9] = org2[0] + width * right2[0];
	vert[10] = org2[1] + width * right2[1];
	vert[11] = org2[2] + width * right2[2];
}

void R_CalcSprite_Vertex3f(float *vertex3f, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2)
{
	vertex3f[ 0] = origin[0] + left[0] * scalex2 + up[0] * scaley1;
	vertex3f[ 1] = origin[1] + left[1] * scalex2 + up[1] * scaley1;
	vertex3f[ 2] = origin[2] + left[2] * scalex2 + up[2] * scaley1;
	vertex3f[ 3] = origin[0] + left[0] * scalex2 + up[0] * scaley2;
	vertex3f[ 4] = origin[1] + left[1] * scalex2 + up[1] * scaley2;
	vertex3f[ 5] = origin[2] + left[2] * scalex2 + up[2] * scaley2;
	vertex3f[ 6] = origin[0] + left[0] * scalex1 + up[0] * scaley2;
	vertex3f[ 7] = origin[1] + left[1] * scalex1 + up[1] * scaley2;
	vertex3f[ 8] = origin[2] + left[2] * scalex1 + up[2] * scaley2;
	vertex3f[ 9] = origin[0] + left[0] * scalex1 + up[0] * scaley1;
	vertex3f[10] = origin[1] + left[1] * scalex1 + up[1] * scaley1;
	vertex3f[11] = origin[2] + left[2] * scalex1 + up[2] * scaley1;
}

int R_Mesh_AddVertex(rmesh_t *mesh, float x, float y, float z)
{
	int i;
	float *vertex3f;
	float v[3];
	VectorSet(v, x, y, z);
	for (i = 0, vertex3f = mesh->vertex3f;i < mesh->numvertices;i++, vertex3f += 3)
		if (VectorDistance2(v, vertex3f) < mesh->epsilon2)
			break;
	if (i == mesh->numvertices)
	{
		if (mesh->numvertices < mesh->maxvertices)
		{
			VectorCopy(v, vertex3f);
			mesh->numvertices++;
		}
		return mesh->numvertices;
	}
	else
		return i;
}

void R_Mesh_AddPolygon3f(rmesh_t *mesh, int numvertices, float *vertex3f)
{
	int i;
	int *e, element[3];
	element[0] = R_Mesh_AddVertex(mesh, vertex3f[0], vertex3f[1], vertex3f[2]);vertex3f += 3;
	element[1] = R_Mesh_AddVertex(mesh, vertex3f[0], vertex3f[1], vertex3f[2]);vertex3f += 3;
	e = mesh->element3i + mesh->numtriangles * 3;
	for (i = 0;i < numvertices - 2;i++, vertex3f += 3)
	{
		element[2] = R_Mesh_AddVertex(mesh, vertex3f[0], vertex3f[1], vertex3f[2]);
		if (mesh->numtriangles < mesh->maxtriangles)
		{
			*e++ = element[0];
			*e++ = element[1];
			*e++ = element[2];
			mesh->numtriangles++;
		}
		element[1] = element[2];
	}
}

void R_Mesh_AddPolygon3d(rmesh_t *mesh, int numvertices, double *vertex3d)
{
	int i;
	int *e, element[3];
	element[0] = R_Mesh_AddVertex(mesh, vertex3d[0], vertex3d[1], vertex3d[2]);vertex3d += 3;
	element[1] = R_Mesh_AddVertex(mesh, vertex3d[0], vertex3d[1], vertex3d[2]);vertex3d += 3;
	e = mesh->element3i + mesh->numtriangles * 3;
	for (i = 0;i < numvertices - 2;i++, vertex3d += 3)
	{
		element[2] = R_Mesh_AddVertex(mesh, vertex3d[0], vertex3d[1], vertex3d[2]);
		if (mesh->numtriangles < mesh->maxtriangles)
		{
			*e++ = element[0];
			*e++ = element[1];
			*e++ = element[2];
			mesh->numtriangles++;
		}
		element[1] = element[2];
	}
}

#define R_MESH_PLANE_DIST_EPSILON (1.0 / 32.0)
void R_Mesh_AddBrushMeshFromPlanes(rmesh_t *mesh, int numplanes, mplane_t *planes)
{
	int planenum, planenum2;
	int w;
	int tempnumpoints;
	mplane_t *plane, *plane2;
	double maxdist;
	double temppoints[2][256*3];
	// figure out how large a bounding box we need to properly compute this brush
	maxdist = 0;
	for (w = 0;w < numplanes;w++)
		maxdist = max(maxdist, fabs(planes[w].dist));
	// now make it large enough to enclose the entire brush, and round it off to a reasonable multiple of 1024
	maxdist = floor(maxdist * (4.0 / 1024.0) + 1) * 1024.0;
	for (planenum = 0, plane = planes;planenum < numplanes;planenum++, plane++)
	{
		w = 0;
		tempnumpoints = 4;
		PolygonD_QuadForPlane(temppoints[w], plane->normal[0], plane->normal[1], plane->normal[2], plane->dist, maxdist);
		for (planenum2 = 0, plane2 = planes;planenum2 < numplanes && tempnumpoints >= 3;planenum2++, plane2++)
		{
			if (planenum2 == planenum)
				continue;
			PolygonD_Divide(tempnumpoints, temppoints[w], plane2->normal[0], plane2->normal[1], plane2->normal[2], plane2->dist, R_MESH_PLANE_DIST_EPSILON, 0, NULL, NULL, 256, temppoints[!w], &tempnumpoints, NULL);
			w = !w;
		}
		if (tempnumpoints < 3)
			continue;
		// generate elements forming a triangle fan for this polygon
		R_Mesh_AddPolygon3d(mesh, tempnumpoints, temppoints[w]);
	}
}

static void R_Texture_AddLayer(texture_t *t, qboolean depthmask, int blendfunc1, int blendfunc2, texturelayertype_t type, rtexture_t *texture, const matrix4x4_t *matrix, float r, float g, float b, float a)
{
	texturelayer_t *layer;
	layer = t->currentlayers + t->currentnumlayers++;
	layer->type = type;
	layer->depthmask = depthmask;
	layer->blendfunc1 = blendfunc1;
	layer->blendfunc2 = blendfunc2;
	layer->texture = texture;
	layer->texmatrix = *matrix;
	layer->color[0] = r;
	layer->color[1] = g;
	layer->color[2] = b;
	layer->color[3] = a;
}

static float R_EvaluateQ3WaveFunc(q3wavefunc_t func, const float *parms)
{
	double index, f;
	index = parms[2] + r_refdef.scene.time * parms[3];
	index -= floor(index);
	switch (func)
	{
	default:
	case Q3WAVEFUNC_NONE:
	case Q3WAVEFUNC_NOISE:
	case Q3WAVEFUNC_COUNT:
		f = 0;
		break;
	case Q3WAVEFUNC_SIN: f = sin(index * M_PI * 2);break;
	case Q3WAVEFUNC_SQUARE: f = index < 0.5 ? 1 : -1;break;
	case Q3WAVEFUNC_SAWTOOTH: f = index;break;
	case Q3WAVEFUNC_INVERSESAWTOOTH: f = 1 - index;break;
	case Q3WAVEFUNC_TRIANGLE:
		index *= 4;
		f = index - floor(index);
		if (index < 1)
			f = f;
		else if (index < 2)
			f = 1 - f;
		else if (index < 3)
			f = -f;
		else
			f = -(1 - f);
		break;
	}
	return (float)(parms[0] + parms[1] * f);
}

void R_tcMod_ApplyToMatrix(matrix4x4_t *texmatrix, q3shaderinfo_layer_tcmod_t *tcmod, int currentmaterialflags)
{
	int w, h, idx;
	float f;
	float tcmat[12];
	matrix4x4_t matrix, temp;
	switch(tcmod->tcmod)
	{
		case Q3TCMOD_COUNT:
		case Q3TCMOD_NONE:
			if (currentmaterialflags & MATERIALFLAG_WATERSCROLL)
				matrix = r_waterscrollmatrix;
			else
				matrix = identitymatrix;
			break;
		case Q3TCMOD_ENTITYTRANSLATE:
			// this is used in Q3 to allow the gamecode to control texcoord
			// scrolling on the entity, which is not supported in darkplaces yet.
			Matrix4x4_CreateTranslate(&matrix, 0, 0, 0);
			break;
		case Q3TCMOD_ROTATE:
			Matrix4x4_CreateTranslate(&matrix, 0.5, 0.5, 0);
			Matrix4x4_ConcatRotate(&matrix, tcmod->parms[0] * r_refdef.scene.time, 0, 0, 1);
			Matrix4x4_ConcatTranslate(&matrix, -0.5, -0.5, 0);
			break;
		case Q3TCMOD_SCALE:
			Matrix4x4_CreateScale3(&matrix, tcmod->parms[0], tcmod->parms[1], 1);
			break;
		case Q3TCMOD_SCROLL:
			Matrix4x4_CreateTranslate(&matrix, tcmod->parms[0] * r_refdef.scene.time, tcmod->parms[1] * r_refdef.scene.time, 0);
			break;
		case Q3TCMOD_PAGE: // poor man's animmap (to store animations into a single file, useful for HTTP downloaded textures)
			w = (int) tcmod->parms[0];
			h = (int) tcmod->parms[1];
			f = r_refdef.scene.time / (tcmod->parms[2] * w * h);
			f = f - floor(f);
			idx = (int) floor(f * w * h);
			Matrix4x4_CreateTranslate(&matrix, (idx % w) / tcmod->parms[0], (idx / w) / tcmod->parms[1], 0);
			break;
		case Q3TCMOD_STRETCH:
			f = 1.0f / R_EvaluateQ3WaveFunc(tcmod->wavefunc, tcmod->waveparms);
			Matrix4x4_CreateFromQuakeEntity(&matrix, 0.5f * (1 - f), 0.5 * (1 - f), 0, 0, 0, 0, f);
			break;
		case Q3TCMOD_TRANSFORM:
			VectorSet(tcmat +  0, tcmod->parms[0], tcmod->parms[1], 0);
			VectorSet(tcmat +  3, tcmod->parms[2], tcmod->parms[3], 0);
			VectorSet(tcmat +  6, 0                   , 0                , 1);
			VectorSet(tcmat +  9, tcmod->parms[4], tcmod->parms[5], 0);
			Matrix4x4_FromArray12FloatGL(&matrix, tcmat);
			break;
		case Q3TCMOD_TURBULENT:
			// this is handled in the RSurf_PrepareVertices function
			matrix = identitymatrix;
			break;
	}
	temp = *texmatrix;
	Matrix4x4_Concat(texmatrix, &matrix, &temp);
}

void R_LoadQWSkin(r_qwskincache_t *cache, const char *skinname)
{
	int textureflags = (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_PICMIP | TEXF_COMPRESS;
	char name[MAX_QPATH];
	skinframe_t *skinframe;
	unsigned char pixels[296*194];
	strlcpy(cache->name, skinname, sizeof(cache->name));
	dpsnprintf(name, sizeof(name), "skins/%s.pcx", cache->name);
	if (developer_loading.integer)
		Con_Printf("loading %s\n", name);
	skinframe = R_SkinFrame_Find(name, textureflags, 0, 0, 0, false);
	if (!skinframe || !skinframe->base)
	{
		unsigned char *f;
		fs_offset_t filesize;
		skinframe = NULL;
		f = FS_LoadFile(name, tempmempool, true, &filesize);
		if (f)
		{
			if (LoadPCX_QWSkin(f, (int)filesize, pixels, 296, 194))
				skinframe = R_SkinFrame_LoadInternalQuake(name, textureflags, true, r_fullbrights.integer, pixels, image_width, image_height);
			Mem_Free(f);
		}
	}
	cache->skinframe = skinframe;
}

texture_t *R_GetCurrentTexture(texture_t *t)
{
	int i;
	const entity_render_t *ent = rsurface.entity;
	dp_model_t *model = ent->model;
	q3shaderinfo_layer_tcmod_t *tcmod;

	if (t->update_lastrenderframe == r_textureframe && t->update_lastrenderentity == (void *)ent)
		return t->currentframe;
	t->update_lastrenderframe = r_textureframe;
	t->update_lastrenderentity = (void *)ent;

	// switch to an alternate material if this is a q1bsp animated material
	{
		texture_t *texture = t;
		int s = rsurface.ent_skinnum;
		if ((unsigned int)s >= (unsigned int)model->numskins)
			s = 0;
		if (model->skinscenes)
		{
			if (model->skinscenes[s].framecount > 1)
				s = model->skinscenes[s].firstframe + (unsigned int) (r_refdef.scene.time * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
			else
				s = model->skinscenes[s].firstframe;
		}
		if (s > 0)
			t = t + s * model->num_surfaces;
		if (t->animated)
		{
			// use an alternate animation if the entity's frame is not 0,
			// and only if the texture has an alternate animation
			if (rsurface.ent_alttextures && t->anim_total[1])
				t = t->anim_frames[1][(t->anim_total[1] >= 2) ? ((int)(r_refdef.scene.time * 5.0f) % t->anim_total[1]) : 0];
			else
				t = t->anim_frames[0][(t->anim_total[0] >= 2) ? ((int)(r_refdef.scene.time * 5.0f) % t->anim_total[0]) : 0];
		}
		texture->currentframe = t;
	}

	// update currentskinframe to be a qw skin or animation frame
	if (rsurface.ent_qwskin >= 0)
	{
		i = rsurface.ent_qwskin;
		if (!r_qwskincache || r_qwskincache_size != cl.maxclients)
		{
			r_qwskincache_size = cl.maxclients;
			if (r_qwskincache)
				Mem_Free(r_qwskincache);
			r_qwskincache = Mem_Alloc(r_main_mempool, sizeof(*r_qwskincache) * r_qwskincache_size);
		}
		if (strcmp(r_qwskincache[i].name, cl.scores[i].qw_skin))
			R_LoadQWSkin(&r_qwskincache[i], cl.scores[i].qw_skin);
		t->currentskinframe = r_qwskincache[i].skinframe;
		if (t->currentskinframe == NULL)
			t->currentskinframe = t->skinframes[(int)(t->skinframerate * (cl.time - rsurface.ent_shadertime)) % t->numskinframes];
	}
	else if (t->numskinframes >= 2)
		t->currentskinframe = t->skinframes[(int)(t->skinframerate * (cl.time - rsurface.ent_shadertime)) % t->numskinframes];
	if (t->backgroundnumskinframes >= 2)
		t->backgroundcurrentskinframe = t->backgroundskinframes[(int)(t->backgroundskinframerate * (cl.time - rsurface.ent_shadertime)) % t->backgroundnumskinframes];

	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = rsurface.colormod[3];
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA && (model->brush.supportwateralpha || r_novis.integer))
		t->currentalpha *= r_wateralpha.value;
	if(t->basematerialflags & MATERIALFLAG_WATERSHADER && r_waterstate.enabled && !r_refdef.view.isoverlay)
		t->currentalpha *= t->r_water_wateralpha;
	if(!r_waterstate.enabled || r_refdef.view.isoverlay)
		t->currentmaterialflags &= ~(MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION);
	if (!(rsurface.ent_flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	else if (rsurface.modeltexcoordlightmap2f == NULL && !(t->currentmaterialflags & MATERIALFLAG_FULLBRIGHT))
	{
		// pick a model lighting mode
		if (VectorLength2(rsurface.modellight_diffuse) >= (1.0f / 256.0f))
			t->currentmaterialflags |= MATERIALFLAG_MODELLIGHT | MATERIALFLAG_MODELLIGHT_DIRECTIONAL;
		else
			t->currentmaterialflags |= MATERIALFLAG_MODELLIGHT;
	}
	if (rsurface.ent_flags & RENDER_ADDITIVE)
		t->currentmaterialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	else if (t->currentalpha < 1)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	if (rsurface.ent_flags & RENDER_DOUBLESIDED)
		t->currentmaterialflags |= MATERIALFLAG_NOSHADOW | MATERIALFLAG_NOCULLFACE;
	if (rsurface.ent_flags & (RENDER_NODEPTHTEST | RENDER_VIEWMODEL))
		t->currentmaterialflags |= MATERIALFLAG_SHORTDEPTHRANGE;
	if (t->backgroundnumskinframes)
		t->currentmaterialflags |= MATERIALFLAG_VERTEXTEXTUREBLEND;
	if (t->currentmaterialflags & MATERIALFLAG_BLENDED)
	{
		if (t->currentmaterialflags & (MATERIALFLAG_REFRACTION | MATERIALFLAG_WATERSHADER))
			t->currentmaterialflags &= ~MATERIALFLAG_BLENDED;
	}
	else
		t->currentmaterialflags &= ~(MATERIALFLAG_REFRACTION | MATERIALFLAG_WATERSHADER);
	if ((t->currentmaterialflags & (MATERIALFLAG_BLENDED | MATERIALFLAG_NODEPTHTEST)) == MATERIALFLAG_BLENDED && r_transparentdepthmasking.integer && !(t->basematerialflags & MATERIALFLAG_BLENDED))
		t->currentmaterialflags |= MATERIALFLAG_TRANSDEPTH;

	// there is no tcmod
	if (t->currentmaterialflags & MATERIALFLAG_WATERSCROLL)
	{
		t->currenttexmatrix = r_waterscrollmatrix;
		t->currentbackgroundtexmatrix = r_waterscrollmatrix;
	}
	else if (!(t->currentmaterialflags & MATERIALFLAG_CUSTOMSURFACE))
	{
		Matrix4x4_CreateIdentity(&t->currenttexmatrix);
		Matrix4x4_CreateIdentity(&t->currentbackgroundtexmatrix);
	}

	for (i = 0, tcmod = t->tcmods;i < Q3MAXTCMODS && tcmod->tcmod;i++, tcmod++)
		R_tcMod_ApplyToMatrix(&t->currenttexmatrix, tcmod, t->currentmaterialflags);
	for (i = 0, tcmod = t->backgroundtcmods;i < Q3MAXTCMODS && tcmod->tcmod;i++, tcmod++)
		R_tcMod_ApplyToMatrix(&t->currentbackgroundtexmatrix, tcmod, t->currentmaterialflags);

	t->colormapping = VectorLength2(rsurface.colormap_pantscolor) + VectorLength2(rsurface.colormap_shirtcolor) >= (1.0f / 1048576.0f);
	if (t->currentskinframe->qpixels)
		R_SkinFrame_GenerateTexturesFromQPixels(t->currentskinframe, t->colormapping);
	t->basetexture = (!t->colormapping && t->currentskinframe->merged) ? t->currentskinframe->merged : t->currentskinframe->base;
	if (!t->basetexture)
		t->basetexture = r_texture_notexture;
	t->pantstexture = t->colormapping ? t->currentskinframe->pants : NULL;
	t->shirttexture = t->colormapping ? t->currentskinframe->shirt : NULL;
	t->nmaptexture = t->currentskinframe->nmap;
	if (!t->nmaptexture)
		t->nmaptexture = r_texture_blanknormalmap;
	t->glosstexture = r_texture_black;
	t->glowtexture = t->currentskinframe->glow;
	t->fogtexture = t->currentskinframe->fog;
	t->reflectmasktexture = t->currentskinframe->reflect;
	if (t->backgroundnumskinframes)
	{
		t->backgroundbasetexture = (!t->colormapping && t->backgroundcurrentskinframe->merged) ? t->backgroundcurrentskinframe->merged : t->backgroundcurrentskinframe->base;
		t->backgroundnmaptexture = t->backgroundcurrentskinframe->nmap;
		t->backgroundglosstexture = r_texture_black;
		t->backgroundglowtexture = t->backgroundcurrentskinframe->glow;
		if (!t->backgroundnmaptexture)
			t->backgroundnmaptexture = r_texture_blanknormalmap;
	}
	else
	{
		t->backgroundbasetexture = t->backgroundnumskinframes ? ((!t->colormapping && t->backgroundcurrentskinframe->merged) ? t->backgroundcurrentskinframe->merged : t->backgroundcurrentskinframe->base) : r_texture_white;
		t->backgroundnmaptexture = r_texture_blanknormalmap;
		t->backgroundglosstexture = r_texture_black;
		t->backgroundglowtexture = NULL;
	}
	t->specularpower = r_shadow_glossexponent.value;
	// TODO: store reference values for these in the texture?
	t->specularscale = 0;
	if (r_shadow_gloss.integer > 0)
	{
		if (t->currentskinframe->gloss || (t->backgroundcurrentskinframe && t->backgroundcurrentskinframe->gloss))
		{
			if (r_shadow_glossintensity.value > 0)
			{
				t->glosstexture = t->currentskinframe->gloss ? t->currentskinframe->gloss : r_texture_white;
				t->backgroundglosstexture = (t->backgroundcurrentskinframe && t->backgroundcurrentskinframe->gloss) ? t->backgroundcurrentskinframe->gloss : r_texture_white;
				t->specularscale = r_shadow_glossintensity.value;
			}
		}
		else if (r_shadow_gloss.integer >= 2 && r_shadow_gloss2intensity.value > 0)
		{
			t->glosstexture = r_texture_white;
			t->backgroundglosstexture = r_texture_white;
			t->specularscale = r_shadow_gloss2intensity.value;
			t->specularpower = r_shadow_gloss2exponent.value;
		}
	}
	t->specularscale *= t->specularscalemod;
	t->specularpower *= t->specularpowermod;

	// lightmaps mode looks bad with dlights using actual texturing, so turn
	// off the colormap and glossmap, but leave the normalmap on as it still
	// accurately represents the shading involved
	if (gl_lightmaps.integer)
	{
		t->basetexture = r_texture_grey128;
		t->pantstexture = r_texture_black;
		t->shirttexture = r_texture_black;
		t->nmaptexture = r_texture_blanknormalmap;
		t->glosstexture = r_texture_black;
		t->glowtexture = NULL;
		t->fogtexture = NULL;
		t->reflectmasktexture = NULL;
		t->backgroundbasetexture = NULL;
		t->backgroundnmaptexture = r_texture_blanknormalmap;
		t->backgroundglosstexture = r_texture_black;
		t->backgroundglowtexture = NULL;
		t->specularscale = 0;
		t->currentmaterialflags = MATERIALFLAG_WALL | (t->currentmaterialflags & (MATERIALFLAG_NOCULLFACE | MATERIALFLAG_MODELLIGHT | MATERIALFLAG_MODELLIGHT_DIRECTIONAL | MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_SHORTDEPTHRANGE));
	}

	Vector4Set(t->lightmapcolor, rsurface.colormod[0], rsurface.colormod[1], rsurface.colormod[2], t->currentalpha);
	VectorClear(t->dlightcolor);
	t->currentnumlayers = 0;
	if (t->currentmaterialflags & MATERIALFLAG_WALL)
	{
		int blendfunc1, blendfunc2;
		qboolean depthmask;
		if (t->currentmaterialflags & MATERIALFLAG_ADD)
		{
			blendfunc1 = GL_SRC_ALPHA;
			blendfunc2 = GL_ONE;
		}
		else if (t->currentmaterialflags & MATERIALFLAG_ALPHA)
		{
			blendfunc1 = GL_SRC_ALPHA;
			blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else if (t->currentmaterialflags & MATERIALFLAG_CUSTOMBLEND)
		{
			blendfunc1 = t->customblendfunc[0];
			blendfunc2 = t->customblendfunc[1];
		}
		else
		{
			blendfunc1 = GL_ONE;
			blendfunc2 = GL_ZERO;
		}
		depthmask = !(t->currentmaterialflags & MATERIALFLAG_BLENDED);
		if (t->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
		{
			// fullbright is not affected by r_refdef.lightmapintensity
			R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_TEXTURE, t->basetexture, &t->currenttexmatrix, t->lightmapcolor[0], t->lightmapcolor[1], t->lightmapcolor[2], t->lightmapcolor[3]);
			if (VectorLength2(rsurface.colormap_pantscolor) >= (1.0f / 1048576.0f) && t->pantstexture)
				R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->pantstexture, &t->currenttexmatrix, rsurface.colormap_pantscolor[0] * t->lightmapcolor[0], rsurface.colormap_pantscolor[1] * t->lightmapcolor[1], rsurface.colormap_pantscolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
			if (VectorLength2(rsurface.colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->shirttexture)
				R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->shirttexture, &t->currenttexmatrix, rsurface.colormap_shirtcolor[0] * t->lightmapcolor[0], rsurface.colormap_shirtcolor[1] * t->lightmapcolor[1], rsurface.colormap_shirtcolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
		}
		else
		{
			vec3_t ambientcolor;
			float colorscale;
			// set the color tint used for lights affecting this surface
			VectorSet(t->dlightcolor, t->lightmapcolor[0] * t->lightmapcolor[3], t->lightmapcolor[1] * t->lightmapcolor[3], t->lightmapcolor[2] * t->lightmapcolor[3]);
			colorscale = 2;
			// q3bsp has no lightmap updates, so the lightstylevalue that
			// would normally be baked into the lightmap must be
			// applied to the color
			// FIXME: r_glsl 1 rendering doesn't support overbright lightstyles with this (the default light style is not overbright)
			if (model->type == mod_brushq3)
				colorscale *= r_refdef.scene.rtlightstylevalue[0];
			colorscale *= r_refdef.lightmapintensity;
			VectorScale(t->lightmapcolor, r_refdef.scene.ambient, ambientcolor);
			VectorScale(t->lightmapcolor, colorscale, t->lightmapcolor);
			// basic lit geometry
			R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_LITTEXTURE, t->basetexture, &t->currenttexmatrix, t->lightmapcolor[0], t->lightmapcolor[1], t->lightmapcolor[2], t->lightmapcolor[3]);
			// add pants/shirt if needed
			if (VectorLength2(rsurface.colormap_pantscolor) >= (1.0f / 1048576.0f) && t->pantstexture)
				R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE, t->pantstexture, &t->currenttexmatrix, rsurface.colormap_pantscolor[0] * t->lightmapcolor[0], rsurface.colormap_pantscolor[1] * t->lightmapcolor[1], rsurface.colormap_pantscolor[2]  * t->lightmapcolor[2], t->lightmapcolor[3]);
			if (VectorLength2(rsurface.colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->shirttexture)
				R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE, t->shirttexture, &t->currenttexmatrix, rsurface.colormap_shirtcolor[0] * t->lightmapcolor[0], rsurface.colormap_shirtcolor[1] * t->lightmapcolor[1], rsurface.colormap_shirtcolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
			// now add ambient passes if needed
			if (VectorLength2(ambientcolor) >= (1.0f/1048576.0f))
			{
				R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->basetexture, &t->currenttexmatrix, ambientcolor[0], ambientcolor[1], ambientcolor[2], t->lightmapcolor[3]);
				if (VectorLength2(rsurface.colormap_pantscolor) >= (1.0f / 1048576.0f) && t->pantstexture)
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->pantstexture, &t->currenttexmatrix, rsurface.colormap_pantscolor[0] * ambientcolor[0], rsurface.colormap_pantscolor[1] * ambientcolor[1], rsurface.colormap_pantscolor[2] * ambientcolor[2], t->lightmapcolor[3]);
				if (VectorLength2(rsurface.colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->shirttexture)
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->shirttexture, &t->currenttexmatrix, rsurface.colormap_shirtcolor[0] * ambientcolor[0], rsurface.colormap_shirtcolor[1] * ambientcolor[1], rsurface.colormap_shirtcolor[2] * ambientcolor[2], t->lightmapcolor[3]);
			}
		}
		if (t->glowtexture != NULL && !gl_lightmaps.integer)
			R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->glowtexture, &t->currenttexmatrix, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2], t->lightmapcolor[3]);
		if (r_refdef.fogenabled && !(t->currentmaterialflags & MATERIALFLAG_ADD))
		{
			// if this is opaque use alpha blend which will darken the earlier
			// passes cheaply.
			//
			// if this is an alpha blended material, all the earlier passes
			// were darkened by fog already, so we only need to add the fog
			// color ontop through the fog mask texture
			//
			// if this is an additive blended material, all the earlier passes
			// were darkened by fog already, and we should not add fog color
			// (because the background was not darkened, there is no fog color
			// that was lost behind it).
			R_Texture_AddLayer(t, false, GL_SRC_ALPHA, (t->currentmaterialflags & MATERIALFLAG_BLENDED) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, TEXTURELAYERTYPE_FOG, t->fogtexture, &t->currenttexmatrix, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2], t->lightmapcolor[3]);
		}
	}

	return t->currentframe;
}

rsurfacestate_t rsurface;

void R_Mesh_ResizeArrays(int newvertices)
{
	float *base;
	if (rsurface.array_size >= newvertices)
		return;
	if (rsurface.array_modelvertex3f)
		Mem_Free(rsurface.array_modelvertex3f);
	rsurface.array_size = (newvertices + 1023) & ~1023;
	base = (float *)Mem_Alloc(r_main_mempool, rsurface.array_size * sizeof(float[33]));
	rsurface.array_modelvertex3f     = base + rsurface.array_size * 0;
	rsurface.array_modelsvector3f    = base + rsurface.array_size * 3;
	rsurface.array_modeltvector3f    = base + rsurface.array_size * 6;
	rsurface.array_modelnormal3f     = base + rsurface.array_size * 9;
	rsurface.array_deformedvertex3f  = base + rsurface.array_size * 12;
	rsurface.array_deformedsvector3f = base + rsurface.array_size * 15;
	rsurface.array_deformedtvector3f = base + rsurface.array_size * 18;
	rsurface.array_deformednormal3f  = base + rsurface.array_size * 21;
	rsurface.array_texcoord3f        = base + rsurface.array_size * 24;
	rsurface.array_color4f           = base + rsurface.array_size * 27;
	rsurface.array_generatedtexcoordtexture2f = base + rsurface.array_size * 31;
}

void RSurf_ActiveWorldEntity(void)
{
	dp_model_t *model = r_refdef.scene.worldmodel;
	//if (rsurface.entity == r_refdef.scene.worldentity)
	//	return;
	rsurface.entity = r_refdef.scene.worldentity;
	rsurface.skeleton = NULL;
	rsurface.ent_skinnum = 0;
	rsurface.ent_qwskin = -1;
	rsurface.ent_shadertime = 0;
	rsurface.ent_flags = r_refdef.scene.worldentity->flags;
	if (rsurface.array_size < model->surfmesh.num_vertices)
		R_Mesh_ResizeArrays(model->surfmesh.num_vertices);
	rsurface.matrix = identitymatrix;
	rsurface.inversematrix = identitymatrix;
	rsurface.matrixscale = 1;
	rsurface.inversematrixscale = 1;
	R_EntityMatrix(&identitymatrix);
	VectorCopy(r_refdef.view.origin, rsurface.localvieworigin);
	Vector4Copy(r_refdef.fogplane, rsurface.fogplane);
	rsurface.fograngerecip = r_refdef.fograngerecip;
	rsurface.fogheightfade = r_refdef.fogheightfade;
	rsurface.fogplaneviewdist = r_refdef.fogplaneviewdist;
	rsurface.fogmasktabledistmultiplier = FOGMASKTABLEWIDTH * rsurface.fograngerecip;
	VectorSet(rsurface.modellight_ambient, 0, 0, 0);
	VectorSet(rsurface.modellight_diffuse, 0, 0, 0);
	VectorSet(rsurface.modellight_lightdir, 0, 0, 1);
	VectorSet(rsurface.colormap_pantscolor, 0, 0, 0);
	VectorSet(rsurface.colormap_shirtcolor, 0, 0, 0);
	VectorSet(rsurface.colormod, r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale);
	rsurface.colormod[3] = 1;
	VectorSet(rsurface.glowmod, r_refdef.view.colorscale * r_hdr_glowintensity.value, r_refdef.view.colorscale * r_hdr_glowintensity.value, r_refdef.view.colorscale * r_hdr_glowintensity.value);
	memset(rsurface.frameblend, 0, sizeof(rsurface.frameblend));
	rsurface.frameblend[0].lerp = 1;
	rsurface.ent_alttextures = false;
	rsurface.basepolygonfactor = r_refdef.polygonfactor;
	rsurface.basepolygonoffset = r_refdef.polygonoffset;
	rsurface.modelvertex3f  = model->surfmesh.data_vertex3f;
	rsurface.modelvertex3f_bufferobject = model->surfmesh.vbo;
	rsurface.modelvertex3f_bufferoffset = model->surfmesh.vbooffset_vertex3f;
	rsurface.modelsvector3f = model->surfmesh.data_svector3f;
	rsurface.modelsvector3f_bufferobject = model->surfmesh.vbo;
	rsurface.modelsvector3f_bufferoffset = model->surfmesh.vbooffset_svector3f;
	rsurface.modeltvector3f = model->surfmesh.data_tvector3f;
	rsurface.modeltvector3f_bufferobject = model->surfmesh.vbo;
	rsurface.modeltvector3f_bufferoffset = model->surfmesh.vbooffset_tvector3f;
	rsurface.modelnormal3f  = model->surfmesh.data_normal3f;
	rsurface.modelnormal3f_bufferobject = model->surfmesh.vbo;
	rsurface.modelnormal3f_bufferoffset = model->surfmesh.vbooffset_normal3f;
	rsurface.modellightmapcolor4f  = model->surfmesh.data_lightmapcolor4f;
	rsurface.modellightmapcolor4f_bufferobject = model->surfmesh.vbo;
	rsurface.modellightmapcolor4f_bufferoffset = model->surfmesh.vbooffset_lightmapcolor4f;
	rsurface.modeltexcoordtexture2f  = model->surfmesh.data_texcoordtexture2f;
	rsurface.modeltexcoordtexture2f_bufferobject = model->surfmesh.vbo;
	rsurface.modeltexcoordtexture2f_bufferoffset = model->surfmesh.vbooffset_texcoordtexture2f;
	rsurface.modeltexcoordlightmap2f  = model->surfmesh.data_texcoordlightmap2f;
	rsurface.modeltexcoordlightmap2f_bufferobject = model->surfmesh.vbo;
	rsurface.modeltexcoordlightmap2f_bufferoffset = model->surfmesh.vbooffset_texcoordlightmap2f;
	rsurface.modelelement3i = model->surfmesh.data_element3i;
	rsurface.modelelement3s = model->surfmesh.data_element3s;
	rsurface.modelelement3i_bufferobject = model->surfmesh.ebo3i;
	rsurface.modelelement3s_bufferobject = model->surfmesh.ebo3s;
	rsurface.modellightmapoffsets = model->surfmesh.data_lightmapoffsets;
	rsurface.modelnum_vertices = model->surfmesh.num_vertices;
	rsurface.modelnum_triangles = model->surfmesh.num_triangles;
	rsurface.modelsurfaces = model->data_surfaces;
	rsurface.generatedvertex = false;
	rsurface.vertex3f  = rsurface.modelvertex3f;
	rsurface.vertex3f_bufferobject = rsurface.modelvertex3f_bufferobject;
	rsurface.vertex3f_bufferoffset = rsurface.modelvertex3f_bufferoffset;
	rsurface.svector3f = rsurface.modelsvector3f;
	rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject;
	rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset;
	rsurface.tvector3f = rsurface.modeltvector3f;
	rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject;
	rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset;
	rsurface.normal3f  = rsurface.modelnormal3f;
	rsurface.normal3f_bufferobject = rsurface.modelnormal3f_bufferobject;
	rsurface.normal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset;
	rsurface.texcoordtexture2f = rsurface.modeltexcoordtexture2f;
}

void RSurf_ActiveModelEntity(const entity_render_t *ent, qboolean wantnormals, qboolean wanttangents, qboolean prepass)
{
	dp_model_t *model = ent->model;
	//if (rsurface.entity == ent && (!model->surfmesh.isanimated || (!wantnormals && !wanttangents)))
	//	return;
	rsurface.entity = (entity_render_t *)ent;
	rsurface.skeleton = ent->skeleton;
	rsurface.ent_skinnum = ent->skinnum;
	rsurface.ent_qwskin = (ent->entitynumber <= cl.maxclients && ent->entitynumber >= 1 && cls.protocol == PROTOCOL_QUAKEWORLD && cl.scores[ent->entitynumber - 1].qw_skin[0] && !strcmp(ent->model->name, "progs/player.mdl")) ? (ent->entitynumber - 1) : -1;
	rsurface.ent_shadertime = ent->shadertime;
	rsurface.ent_flags = ent->flags;
	if (rsurface.array_size < model->surfmesh.num_vertices)
		R_Mesh_ResizeArrays(model->surfmesh.num_vertices);
	rsurface.matrix = ent->matrix;
	rsurface.inversematrix = ent->inversematrix;
	rsurface.matrixscale = Matrix4x4_ScaleFromMatrix(&rsurface.matrix);
	rsurface.inversematrixscale = 1.0f / rsurface.matrixscale;
	R_EntityMatrix(&rsurface.matrix);
	Matrix4x4_Transform(&rsurface.inversematrix, r_refdef.view.origin, rsurface.localvieworigin);
	Matrix4x4_TransformStandardPlane(&rsurface.inversematrix, r_refdef.fogplane[0], r_refdef.fogplane[1], r_refdef.fogplane[2], r_refdef.fogplane[3], rsurface.fogplane);
	rsurface.fogplaneviewdist *= rsurface.inversematrixscale;
	rsurface.fograngerecip = r_refdef.fograngerecip * rsurface.matrixscale;
	rsurface.fogheightfade = r_refdef.fogheightfade * rsurface.matrixscale;
	rsurface.fogmasktabledistmultiplier = FOGMASKTABLEWIDTH * rsurface.fograngerecip;
	VectorCopy(ent->modellight_ambient, rsurface.modellight_ambient);
	VectorCopy(ent->modellight_diffuse, rsurface.modellight_diffuse);
	VectorCopy(ent->modellight_lightdir, rsurface.modellight_lightdir);
	VectorCopy(ent->colormap_pantscolor, rsurface.colormap_pantscolor);
	VectorCopy(ent->colormap_shirtcolor, rsurface.colormap_shirtcolor);
	VectorScale(ent->colormod, r_refdef.view.colorscale, rsurface.colormod);
	rsurface.colormod[3] = ent->alpha;
	VectorScale(ent->glowmod, r_refdef.view.colorscale * r_hdr_glowintensity.value, rsurface.glowmod);
	memcpy(rsurface.frameblend, ent->frameblend, sizeof(ent->frameblend));
	rsurface.ent_alttextures = ent->framegroupblend[0].frame != 0;
	rsurface.basepolygonfactor = r_refdef.polygonfactor;
	rsurface.basepolygonoffset = r_refdef.polygonoffset;
	if (ent->model->brush.submodel && !prepass)
	{
		rsurface.basepolygonfactor += r_polygonoffset_submodel_factor.value;
		rsurface.basepolygonoffset += r_polygonoffset_submodel_offset.value;
	}
	if (model->surfmesh.isanimated && model->AnimateVertices && (rsurface.frameblend[0].lerp != 1 || rsurface.frameblend[0].subframe != 0))
	{
		if (ent->animcache_vertex3f && !r_framedata_failed)
		{
			rsurface.modelvertex3f = ent->animcache_vertex3f;
			rsurface.modelsvector3f = wanttangents ? ent->animcache_svector3f : NULL;
			rsurface.modeltvector3f = wanttangents ? ent->animcache_tvector3f : NULL;
			rsurface.modelnormal3f = wantnormals ? ent->animcache_normal3f : NULL;
		}
		else if (wanttangents)
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = rsurface.array_modelsvector3f;
			rsurface.modeltvector3f = rsurface.array_modeltvector3f;
			rsurface.modelnormal3f = rsurface.array_modelnormal3f;
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.array_modelvertex3f, rsurface.array_modelnormal3f, rsurface.array_modelsvector3f, rsurface.array_modeltvector3f);
		}
		else if (wantnormals)
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = rsurface.array_modelnormal3f;
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.array_modelvertex3f, rsurface.array_modelnormal3f, NULL, NULL);
		}
		else
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = NULL;
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.array_modelvertex3f, NULL, NULL, NULL);
		}
		rsurface.modelvertex3f_bufferobject = 0;
		rsurface.modelvertex3f_bufferoffset = 0;
		rsurface.modelsvector3f_bufferobject = 0;
		rsurface.modelsvector3f_bufferoffset = 0;
		rsurface.modeltvector3f_bufferobject = 0;
		rsurface.modeltvector3f_bufferoffset = 0;
		rsurface.modelnormal3f_bufferobject = 0;
		rsurface.modelnormal3f_bufferoffset = 0;
		rsurface.generatedvertex = true;
	}
	else
	{
		rsurface.modelvertex3f  = model->surfmesh.data_vertex3f;
		rsurface.modelvertex3f_bufferobject = model->surfmesh.vbo;
		rsurface.modelvertex3f_bufferoffset = model->surfmesh.vbooffset_vertex3f;
		rsurface.modelsvector3f = model->surfmesh.data_svector3f;
		rsurface.modelsvector3f_bufferobject = model->surfmesh.vbo;
		rsurface.modelsvector3f_bufferoffset = model->surfmesh.vbooffset_svector3f;
		rsurface.modeltvector3f = model->surfmesh.data_tvector3f;
		rsurface.modeltvector3f_bufferobject = model->surfmesh.vbo;
		rsurface.modeltvector3f_bufferoffset = model->surfmesh.vbooffset_tvector3f;
		rsurface.modelnormal3f  = model->surfmesh.data_normal3f;
		rsurface.modelnormal3f_bufferobject = model->surfmesh.vbo;
		rsurface.modelnormal3f_bufferoffset = model->surfmesh.vbooffset_normal3f;
		rsurface.generatedvertex = false;
	}
	rsurface.modellightmapcolor4f  = model->surfmesh.data_lightmapcolor4f;
	rsurface.modellightmapcolor4f_bufferobject = model->surfmesh.vbo;
	rsurface.modellightmapcolor4f_bufferoffset = model->surfmesh.vbooffset_lightmapcolor4f;
	rsurface.modeltexcoordtexture2f  = model->surfmesh.data_texcoordtexture2f;
	rsurface.modeltexcoordtexture2f_bufferobject = model->surfmesh.vbo;
	rsurface.modeltexcoordtexture2f_bufferoffset = model->surfmesh.vbooffset_texcoordtexture2f;
	rsurface.modeltexcoordlightmap2f  = model->surfmesh.data_texcoordlightmap2f;
	rsurface.modeltexcoordlightmap2f_bufferobject = model->surfmesh.vbo;
	rsurface.modeltexcoordlightmap2f_bufferoffset = model->surfmesh.vbooffset_texcoordlightmap2f;
	rsurface.modelelement3i = model->surfmesh.data_element3i;
	rsurface.modelelement3s = model->surfmesh.data_element3s;
	rsurface.modelelement3i_bufferobject = model->surfmesh.ebo3i;
	rsurface.modelelement3s_bufferobject = model->surfmesh.ebo3s;
	rsurface.modellightmapoffsets = model->surfmesh.data_lightmapoffsets;
	rsurface.modelnum_vertices = model->surfmesh.num_vertices;
	rsurface.modelnum_triangles = model->surfmesh.num_triangles;
	rsurface.modelsurfaces = model->data_surfaces;
	rsurface.vertex3f  = rsurface.modelvertex3f;
	rsurface.vertex3f_bufferobject = rsurface.modelvertex3f_bufferobject;
	rsurface.vertex3f_bufferoffset = rsurface.modelvertex3f_bufferoffset;
	rsurface.svector3f = rsurface.modelsvector3f;
	rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject;
	rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset;
	rsurface.tvector3f = rsurface.modeltvector3f;
	rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject;
	rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset;
	rsurface.normal3f  = rsurface.modelnormal3f;
	rsurface.normal3f_bufferobject = rsurface.modelnormal3f_bufferobject;
	rsurface.normal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset;
	rsurface.texcoordtexture2f = rsurface.modeltexcoordtexture2f;
}

void RSurf_ActiveCustomEntity(const matrix4x4_t *matrix, const matrix4x4_t *inversematrix, int entflags, double shadertime, float r, float g, float b, float a, int numvertices, const float *vertex3f, const float *texcoord2f, const float *normal3f, const float *svector3f, const float *tvector3f, const float *color4f, int numtriangles, const int *element3i, const unsigned short *element3s, qboolean wantnormals, qboolean wanttangents)
{
	rsurface.entity = r_refdef.scene.worldentity;
	rsurface.skeleton = NULL;
	rsurface.ent_skinnum = 0;
	rsurface.ent_qwskin = -1;
	rsurface.ent_shadertime = shadertime;
	rsurface.ent_flags = entflags;
	rsurface.modelnum_vertices = numvertices;
	rsurface.modelnum_triangles = numtriangles;
	if (rsurface.array_size < rsurface.modelnum_vertices)
		R_Mesh_ResizeArrays(rsurface.modelnum_vertices);
	rsurface.matrix = *matrix;
	rsurface.inversematrix = *inversematrix;
	rsurface.matrixscale = Matrix4x4_ScaleFromMatrix(&rsurface.matrix);
	rsurface.inversematrixscale = 1.0f / rsurface.matrixscale;
	R_EntityMatrix(&rsurface.matrix);
	Matrix4x4_Transform(&rsurface.inversematrix, r_refdef.view.origin, rsurface.localvieworigin);
	Matrix4x4_TransformStandardPlane(&rsurface.inversematrix, r_refdef.fogplane[0], r_refdef.fogplane[1], r_refdef.fogplane[2], r_refdef.fogplane[3], rsurface.fogplane);
	rsurface.fogplaneviewdist *= rsurface.inversematrixscale;
	rsurface.fograngerecip = r_refdef.fograngerecip * rsurface.matrixscale;
	rsurface.fogheightfade = r_refdef.fogheightfade * rsurface.matrixscale;
	rsurface.fogmasktabledistmultiplier = FOGMASKTABLEWIDTH * rsurface.fograngerecip;
	VectorSet(rsurface.modellight_ambient, 0, 0, 0);
	VectorSet(rsurface.modellight_diffuse, 0, 0, 0);
	VectorSet(rsurface.modellight_lightdir, 0, 0, 1);
	VectorSet(rsurface.colormap_pantscolor, 0, 0, 0);
	VectorSet(rsurface.colormap_shirtcolor, 0, 0, 0);
	Vector4Set(rsurface.colormod, r * r_refdef.view.colorscale, g * r_refdef.view.colorscale, b * r_refdef.view.colorscale, a);
	VectorSet(rsurface.glowmod, r_refdef.view.colorscale * r_hdr_glowintensity.value, r_refdef.view.colorscale * r_hdr_glowintensity.value, r_refdef.view.colorscale * r_hdr_glowintensity.value);
	memset(rsurface.frameblend, 0, sizeof(rsurface.frameblend));
	rsurface.frameblend[0].lerp = 1;
	rsurface.ent_alttextures = false;
	rsurface.basepolygonfactor = r_refdef.polygonfactor;
	rsurface.basepolygonoffset = r_refdef.polygonoffset;
	if (wanttangents)
	{
		rsurface.modelvertex3f = vertex3f;
		rsurface.modelsvector3f = svector3f ? svector3f : rsurface.array_modelsvector3f;
		rsurface.modeltvector3f = tvector3f ? tvector3f : rsurface.array_modeltvector3f;
		rsurface.modelnormal3f = normal3f ? normal3f : rsurface.array_modelnormal3f;
	}
	else if (wantnormals)
	{
		rsurface.modelvertex3f = vertex3f;
		rsurface.modelsvector3f = NULL;
		rsurface.modeltvector3f = NULL;
		rsurface.modelnormal3f = normal3f ? normal3f : rsurface.array_modelnormal3f;
	}
	else
	{
		rsurface.modelvertex3f = vertex3f;
		rsurface.modelsvector3f = NULL;
		rsurface.modeltvector3f = NULL;
		rsurface.modelnormal3f = NULL;
	}
	rsurface.modelvertex3f_bufferobject = 0;
	rsurface.modelvertex3f_bufferoffset = 0;
	rsurface.modelsvector3f_bufferobject = 0;
	rsurface.modelsvector3f_bufferoffset = 0;
	rsurface.modeltvector3f_bufferobject = 0;
	rsurface.modeltvector3f_bufferoffset = 0;
	rsurface.modelnormal3f_bufferobject = 0;
	rsurface.modelnormal3f_bufferoffset = 0;
	rsurface.generatedvertex = true;
	rsurface.modellightmapcolor4f  = color4f;
	rsurface.modellightmapcolor4f_bufferobject = 0;
	rsurface.modellightmapcolor4f_bufferoffset = 0;
	rsurface.modeltexcoordtexture2f  = texcoord2f;
	rsurface.modeltexcoordtexture2f_bufferobject = 0;
	rsurface.modeltexcoordtexture2f_bufferoffset = 0;
	rsurface.modeltexcoordlightmap2f  = NULL;
	rsurface.modeltexcoordlightmap2f_bufferobject = 0;
	rsurface.modeltexcoordlightmap2f_bufferoffset = 0;
	rsurface.modelelement3i = element3i;
	rsurface.modelelement3s = element3s;
	rsurface.modelelement3i_bufferobject = 0;
	rsurface.modelelement3s_bufferobject = 0;
	rsurface.modellightmapoffsets = NULL;
	rsurface.modelsurfaces = NULL;
	rsurface.vertex3f  = rsurface.modelvertex3f;
	rsurface.vertex3f_bufferobject = rsurface.modelvertex3f_bufferobject;
	rsurface.vertex3f_bufferoffset = rsurface.modelvertex3f_bufferoffset;
	rsurface.svector3f = rsurface.modelsvector3f;
	rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject;
	rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset;
	rsurface.tvector3f = rsurface.modeltvector3f;
	rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject;
	rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset;
	rsurface.normal3f  = rsurface.modelnormal3f;
	rsurface.normal3f_bufferobject = rsurface.modelnormal3f_bufferobject;
	rsurface.normal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset;
	rsurface.texcoordtexture2f = rsurface.modeltexcoordtexture2f;

	if (rsurface.modelnum_vertices && rsurface.modelelement3i)
	{
		if ((wantnormals || wanttangents) && !normal3f)
			Mod_BuildNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modelelement3i, rsurface.array_modelnormal3f, r_smoothnormals_areaweighting.integer != 0);
		if (wanttangents && !svector3f)
			Mod_BuildTextureVectorsFromNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modeltexcoordtexture2f, rsurface.modelnormal3f, rsurface.modelelement3i, rsurface.array_modelsvector3f, rsurface.array_modeltvector3f, r_smoothnormals_areaweighting.integer != 0);
	}
}

float RSurf_FogPoint(const float *v)
{
	// this code is identical to the USEFOGINSIDE/USEFOGOUTSIDE code in the shader
	float FogPlaneViewDist = r_refdef.fogplaneviewdist;
	float FogPlaneVertexDist = DotProduct(r_refdef.fogplane, v) + r_refdef.fogplane[3];
	float FogHeightFade = r_refdef.fogheightfade;
	float fogfrac;
	unsigned int fogmasktableindex;
	if (r_refdef.fogplaneviewabove)
		fogfrac = min(0.0f, FogPlaneVertexDist) / (FogPlaneVertexDist - FogPlaneViewDist) * min(1.0f, min(0.0f, FogPlaneVertexDist) * FogHeightFade);
	else
		fogfrac = FogPlaneViewDist / (FogPlaneViewDist - max(0.0f, FogPlaneVertexDist)) * min(1.0f, (min(0.0f, FogPlaneVertexDist) + FogPlaneViewDist) * FogHeightFade);
	fogmasktableindex = (unsigned int)(VectorDistance(r_refdef.view.origin, v) * fogfrac * r_refdef.fogmasktabledistmultiplier);
	return r_refdef.fogmasktable[min(fogmasktableindex, FOGMASKTABLEWIDTH - 1)];
}

float RSurf_FogVertex(const float *v)
{
	// this code is identical to the USEFOGINSIDE/USEFOGOUTSIDE code in the shader
	float FogPlaneViewDist = rsurface.fogplaneviewdist;
	float FogPlaneVertexDist = DotProduct(rsurface.fogplane, v) + rsurface.fogplane[3];
	float FogHeightFade = rsurface.fogheightfade;
	float fogfrac;
	unsigned int fogmasktableindex;
	if (r_refdef.fogplaneviewabove)
		fogfrac = min(0.0f, FogPlaneVertexDist) / (FogPlaneVertexDist - FogPlaneViewDist) * min(1.0f, min(0.0f, FogPlaneVertexDist) * FogHeightFade);
	else
		fogfrac = FogPlaneViewDist / (FogPlaneViewDist - max(0.0f, FogPlaneVertexDist)) * min(1.0f, (min(0.0f, FogPlaneVertexDist) + FogPlaneViewDist) * FogHeightFade);
	fogmasktableindex = (unsigned int)(VectorDistance(rsurface.localvieworigin, v) * fogfrac * rsurface.fogmasktabledistmultiplier);
	return r_refdef.fogmasktable[min(fogmasktableindex, FOGMASKTABLEWIDTH - 1)];
}

static const int quadedges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
void RSurf_PrepareVerticesForBatch(qboolean generatenormals, qboolean generatetangents, int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int deformindex;
	int texturesurfaceindex;
	int i, j;
	float amplitude;
	float animpos;
	float scale;
	const float *v1, *in_tc;
	float *out_tc;
	float center[3], forward[3], right[3], up[3], v[3], newforward[3], newright[3], newup[3];
	float waveparms[4];
	q3shaderinfo_deform_t *deform;
	// if vertices are dynamic (animated models), generate them into the temporary rsurface.array_model* arrays and point rsurface.model* at them instead of the static data from the model itself
	if (rsurface.generatedvertex)
	{
		if (rsurface.texture->tcgen.tcgen == Q3TCGEN_ENVIRONMENT)
			generatenormals = true;
		for (i = 0;i < Q3MAXDEFORMS;i++)
		{
			if (rsurface.texture->deforms[i].deform == Q3DEFORM_AUTOSPRITE)
			{
				generatetangents = true;
				generatenormals = true;
			}
			if (rsurface.texture->deforms[i].deform != Q3DEFORM_NONE)
				generatenormals = true;
		}
		if (generatenormals && !rsurface.modelnormal3f)
		{
			rsurface.normal3f = rsurface.modelnormal3f = rsurface.array_modelnormal3f;
			rsurface.normal3f_bufferobject = rsurface.modelnormal3f_bufferobject = 0;
			rsurface.normal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset = 0;
			Mod_BuildNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modelelement3i, rsurface.array_modelnormal3f, r_smoothnormals_areaweighting.integer != 0);
		}
		if (generatetangents && !rsurface.modelsvector3f)
		{
			rsurface.svector3f = rsurface.modelsvector3f = rsurface.array_modelsvector3f;
			rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject = 0;
			rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset = 0;
			rsurface.tvector3f = rsurface.modeltvector3f = rsurface.array_modeltvector3f;
			rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject = 0;
			rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset = 0;
			Mod_BuildTextureVectorsFromNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modeltexcoordtexture2f, rsurface.modelnormal3f, rsurface.modelelement3i, rsurface.array_modelsvector3f, rsurface.array_modeltvector3f, r_smoothnormals_areaweighting.integer != 0);
		}
	}
	rsurface.vertex3f  = rsurface.modelvertex3f;
	rsurface.vertex3f_bufferobject = rsurface.modelvertex3f_bufferobject;
	rsurface.vertex3f_bufferoffset = rsurface.modelvertex3f_bufferoffset;
	rsurface.svector3f = rsurface.modelsvector3f;
	rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject;
	rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset;
	rsurface.tvector3f = rsurface.modeltvector3f;
	rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject;
	rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset;
	rsurface.normal3f  = rsurface.modelnormal3f;
	rsurface.normal3f_bufferobject = rsurface.modelnormal3f_bufferobject;
	rsurface.normal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset;
	// if vertices are deformed (sprite flares and things in maps, possibly
	// water waves, bulges and other deformations), generate them into
	// rsurface.deform* arrays from whatever the rsurface.* arrays point to
	// (may be static model data or generated data for an animated model, or
	//  the previous deform pass)
	for (deformindex = 0, deform = rsurface.texture->deforms;deformindex < Q3MAXDEFORMS && deform->deform;deformindex++, deform++)
	{
		switch (deform->deform)
		{
		default:
		case Q3DEFORM_PROJECTIONSHADOW:
		case Q3DEFORM_TEXT0:
		case Q3DEFORM_TEXT1:
		case Q3DEFORM_TEXT2:
		case Q3DEFORM_TEXT3:
		case Q3DEFORM_TEXT4:
		case Q3DEFORM_TEXT5:
		case Q3DEFORM_TEXT6:
		case Q3DEFORM_TEXT7:
		case Q3DEFORM_NONE:
			break;
		case Q3DEFORM_AUTOSPRITE:
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.forward, newforward);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.right, newright);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.up, newup);
			VectorNormalize(newforward);
			VectorNormalize(newright);
			VectorNormalize(newup);
			// make deformed versions of only the model vertices used by the specified surfaces
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				// a single autosprite surface can contain multiple sprites...
				for (j = 0;j < surface->num_vertices - 3;j += 4)
				{
					VectorClear(center);
					for (i = 0;i < 4;i++)
						VectorAdd(center, (rsurface.vertex3f + 3 * surface->num_firstvertex) + (j+i) * 3, center);
					VectorScale(center, 0.25f, center);
					VectorCopy((rsurface.normal3f  + 3 * surface->num_firstvertex) + j*3, forward);
					VectorCopy((rsurface.svector3f + 3 * surface->num_firstvertex) + j*3, right);
					VectorCopy((rsurface.tvector3f + 3 * surface->num_firstvertex) + j*3, up);
					for (i = 0;i < 4;i++)
					{
						VectorSubtract((rsurface.vertex3f + 3 * surface->num_firstvertex) + (j+i)*3, center, v);
						VectorMAMAMAM(1, center, DotProduct(forward, v), newforward, DotProduct(right, v), newright, DotProduct(up, v), newup, rsurface.array_deformedvertex3f + (surface->num_firstvertex+i+j) * 3);
					}
				}
				Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformednormal3f, r_smoothnormals_areaweighting.integer != 0);
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			rsurface.vertex3f = rsurface.array_deformedvertex3f;
			rsurface.vertex3f_bufferobject = 0;
			rsurface.vertex3f_bufferoffset = 0;
			rsurface.svector3f = rsurface.array_deformedsvector3f;
			rsurface.svector3f_bufferobject = 0;
			rsurface.svector3f_bufferoffset = 0;
			rsurface.tvector3f = rsurface.array_deformedtvector3f;
			rsurface.tvector3f_bufferobject = 0;
			rsurface.tvector3f_bufferoffset = 0;
			rsurface.normal3f = rsurface.array_deformednormal3f;
			rsurface.normal3f_bufferobject = 0;
			rsurface.normal3f_bufferoffset = 0;
			break;
		case Q3DEFORM_AUTOSPRITE2:
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.forward, newforward);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.right, newright);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.up, newup);
			VectorNormalize(newforward);
			VectorNormalize(newright);
			VectorNormalize(newup);
			// make deformed versions of only the model vertices used by the specified surfaces
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				const float *v1, *v2;
				vec3_t start, end;
				float f, l;
				struct
				{
					float length2;
					const float *v1;
					const float *v2;
				}
				shortest[2];
				memset(shortest, 0, sizeof(shortest));
				// a single autosprite surface can contain multiple sprites...
				for (j = 0;j < surface->num_vertices - 3;j += 4)
				{
					VectorClear(center);
					for (i = 0;i < 4;i++)
						VectorAdd(center, (rsurface.vertex3f + 3 * surface->num_firstvertex) + (j+i) * 3, center);
					VectorScale(center, 0.25f, center);
					// find the two shortest edges, then use them to define the
					// axis vectors for rotating around the central axis
					for (i = 0;i < 6;i++)
					{
						v1 = rsurface.vertex3f + 3 * (surface->num_firstvertex + quadedges[i][0]);
						v2 = rsurface.vertex3f + 3 * (surface->num_firstvertex + quadedges[i][1]);
#if 0
						Debug_PolygonBegin(NULL, 0);
						Debug_PolygonVertex(v1[0], v1[1], v1[2], 0, 0, 1, 0, 0, 1);
						Debug_PolygonVertex((v1[0] + v2[0]) * 0.5f + rsurface.normal3f[3 * (surface->num_firstvertex + j)+0] * 4, (v1[1] + v2[1]) * 0.5f + rsurface.normal3f[3 * (surface->num_firstvertex + j)+1], (v1[2] + v2[2]) * 0.5f + rsurface.normal3f[3 * (surface->num_firstvertex + j)+2], 0, 0, 1, 1, 0, 1);
						Debug_PolygonVertex(v2[0], v2[1], v2[2], 0, 0, 1, 0, 0, 1);
						Debug_PolygonEnd();
#endif
						l = VectorDistance2(v1, v2);
						// this length bias tries to make sense of square polygons, assuming they are meant to be upright
						if (v1[2] != v2[2])
							l += (1.0f / 1024.0f);
						if (shortest[0].length2 > l || i == 0)
						{
							shortest[1] = shortest[0];
							shortest[0].length2 = l;
							shortest[0].v1 = v1;
							shortest[0].v2 = v2;
						}
						else if (shortest[1].length2 > l || i == 1)
						{
							shortest[1].length2 = l;
							shortest[1].v1 = v1;
							shortest[1].v2 = v2;
						}
					}
					VectorLerp(shortest[0].v1, 0.5f, shortest[0].v2, start);
					VectorLerp(shortest[1].v1, 0.5f, shortest[1].v2, end);
#if 0
					Debug_PolygonBegin(NULL, 0);
					Debug_PolygonVertex(start[0], start[1], start[2], 0, 0, 1, 1, 0, 1);
					Debug_PolygonVertex(center[0] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+0] * 4, center[1] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+1] * 4, center[2] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+2] * 4, 0, 0, 0, 1, 0, 1);
					Debug_PolygonVertex(end[0], end[1], end[2], 0, 0, 0, 1, 1, 1);
					Debug_PolygonEnd();
#endif
					// this calculates the right vector from the shortest edge
					// and the up vector from the edge midpoints
					VectorSubtract(shortest[0].v1, shortest[0].v2, right);
					VectorNormalize(right);
					VectorSubtract(end, start, up);
					VectorNormalize(up);
					// calculate a forward vector to use instead of the original plane normal (this is how we get a new right vector)
					VectorSubtract(rsurface.localvieworigin, center, forward);
					//Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.forward, forward);
					VectorNegate(forward, forward);
					VectorReflect(forward, 0, up, forward);
					VectorNormalize(forward);
					CrossProduct(up, forward, newright);
					VectorNormalize(newright);
#if 0
					Debug_PolygonBegin(NULL, 0);
					Debug_PolygonVertex(center[0] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+0] * 8, center[1] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+1] * 8, center[2] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+2] * 8, 0, 0, 1, 0, 0, 1);
					Debug_PolygonVertex(center[0] + right[0] * 8, center[1] + right[1] * 8, center[2] + right[2] * 8, 0, 0, 0, 1, 0, 1);
					Debug_PolygonVertex(center[0] + up   [0] * 8, center[1] + up   [1] * 8, center[2] + up   [2] * 8, 0, 0, 0, 0, 1, 1);
					Debug_PolygonEnd();
#endif
#if 0
					Debug_PolygonBegin(NULL, 0);
					Debug_PolygonVertex(center[0] + forward [0] * 8, center[1] + forward [1] * 8, center[2] + forward [2] * 8, 0, 0, 1, 0, 0, 1);
					Debug_PolygonVertex(center[0] + newright[0] * 8, center[1] + newright[1] * 8, center[2] + newright[2] * 8, 0, 0, 0, 1, 0, 1);
					Debug_PolygonVertex(center[0] + up      [0] * 8, center[1] + up      [1] * 8, center[2] + up      [2] * 8, 0, 0, 0, 0, 1, 1);
					Debug_PolygonEnd();
#endif
					// rotate the quad around the up axis vector, this is made
					// especially easy by the fact we know the quad is flat,
					// so we only have to subtract the center position and
					// measure distance along the right vector, and then
					// multiply that by the newright vector and add back the
					// center position
					// we also need to subtract the old position to undo the
					// displacement from the center, which we do with a
					// DotProduct, the subtraction/addition of center is also
					// optimized into DotProducts here
					l = DotProduct(right, center);
					for (i = 0;i < 4;i++)
					{
						v1 = rsurface.vertex3f + 3 * (surface->num_firstvertex + j + i);
						f = DotProduct(right, v1) - l;
						VectorMAMAM(1, v1, -f, right, f, newright, rsurface.array_deformedvertex3f + (surface->num_firstvertex+i+j) * 3);
					}
				}
				Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformednormal3f, r_smoothnormals_areaweighting.integer != 0);
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			rsurface.vertex3f = rsurface.array_deformedvertex3f;
			rsurface.vertex3f_bufferobject = 0;
			rsurface.vertex3f_bufferoffset = 0;
			rsurface.svector3f = rsurface.array_deformedsvector3f;
			rsurface.svector3f_bufferobject = 0;
			rsurface.svector3f_bufferoffset = 0;
			rsurface.tvector3f = rsurface.array_deformedtvector3f;
			rsurface.tvector3f_bufferobject = 0;
			rsurface.tvector3f_bufferoffset = 0;
			rsurface.normal3f = rsurface.array_deformednormal3f;
			rsurface.normal3f_bufferobject = 0;
			rsurface.normal3f_bufferoffset = 0;
			break;
		case Q3DEFORM_NORMAL:
			// deform the normals to make reflections wavey
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (j = 0;j < surface->num_vertices;j++)
				{
					float vertex[3];
					float *normal = (rsurface.array_deformednormal3f  + 3 * surface->num_firstvertex) + j*3;
					VectorScale((rsurface.vertex3f  + 3 * surface->num_firstvertex) + j*3, 0.98f, vertex);
					VectorCopy((rsurface.normal3f  + 3 * surface->num_firstvertex) + j*3, normal);
					normal[0] += deform->parms[0] * noise4f(      vertex[0], vertex[1], vertex[2], r_refdef.scene.time * deform->parms[1]);
					normal[1] += deform->parms[0] * noise4f( 98 + vertex[0], vertex[1], vertex[2], r_refdef.scene.time * deform->parms[1]);
					normal[2] += deform->parms[0] * noise4f(196 + vertex[0], vertex[1], vertex[2], r_refdef.scene.time * deform->parms[1]);
					VectorNormalize(normal);
				}
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			rsurface.svector3f = rsurface.array_deformedsvector3f;
			rsurface.svector3f_bufferobject = 0;
			rsurface.svector3f_bufferoffset = 0;
			rsurface.tvector3f = rsurface.array_deformedtvector3f;
			rsurface.tvector3f_bufferobject = 0;
			rsurface.tvector3f_bufferoffset = 0;
			rsurface.normal3f = rsurface.array_deformednormal3f;
			rsurface.normal3f_bufferobject = 0;
			rsurface.normal3f_bufferoffset = 0;
			break;
		case Q3DEFORM_WAVE:
			// deform vertex array to make wavey water and flags and such
			waveparms[0] = deform->waveparms[0];
			waveparms[1] = deform->waveparms[1];
			waveparms[2] = deform->waveparms[2];
			waveparms[3] = deform->waveparms[3];
			// this is how a divisor of vertex influence on deformation
			animpos = deform->parms[0] ? 1.0f / deform->parms[0] : 100.0f;
			scale = R_EvaluateQ3WaveFunc(deform->wavefunc, waveparms);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (j = 0;j < surface->num_vertices;j++)
				{
					float *vertex = (rsurface.array_deformedvertex3f  + 3 * surface->num_firstvertex) + j*3;
					VectorCopy((rsurface.vertex3f  + 3 * surface->num_firstvertex) + j*3, vertex);
					// if the wavefunc depends on time, evaluate it per-vertex
					if (waveparms[3])
					{
						waveparms[2] = deform->waveparms[2] + (vertex[0] + vertex[1] + vertex[2]) * animpos;
						scale = R_EvaluateQ3WaveFunc(deform->wavefunc, waveparms);
					}
					VectorMA(vertex, scale, (rsurface.normal3f  + 3 * surface->num_firstvertex) + j*3, vertex);
				}
			}
			rsurface.vertex3f = rsurface.array_deformedvertex3f;
			rsurface.vertex3f_bufferobject = 0;
			rsurface.vertex3f_bufferoffset = 0;
			break;
		case Q3DEFORM_BULGE:
			// deform vertex array to make the surface have moving bulges
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (j = 0;j < surface->num_vertices;j++)
				{
					scale = sin((rsurface.modeltexcoordtexture2f[2 * (surface->num_firstvertex + j)] * deform->parms[0] + r_refdef.scene.time * deform->parms[2])) * deform->parms[1];
					VectorMA(rsurface.vertex3f + 3 * (surface->num_firstvertex + j), scale, rsurface.normal3f + 3 * (surface->num_firstvertex + j), rsurface.array_deformedvertex3f + 3 * (surface->num_firstvertex + j));
				}
			}
			rsurface.vertex3f = rsurface.array_deformedvertex3f;
			rsurface.vertex3f_bufferobject = 0;
			rsurface.vertex3f_bufferoffset = 0;
			break;
		case Q3DEFORM_MOVE:
			// deform vertex array
			scale = R_EvaluateQ3WaveFunc(deform->wavefunc, deform->waveparms);
			VectorScale(deform->parms, scale, waveparms);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (j = 0;j < surface->num_vertices;j++)
					VectorAdd(rsurface.vertex3f + 3 * (surface->num_firstvertex + j), waveparms, rsurface.array_deformedvertex3f + 3 * (surface->num_firstvertex + j));
			}
			rsurface.vertex3f = rsurface.array_deformedvertex3f;
			rsurface.vertex3f_bufferobject = 0;
			rsurface.vertex3f_bufferoffset = 0;
			break;
		}
	}
	// generate texcoords based on the chosen texcoord source
	switch(rsurface.texture->tcgen.tcgen)
	{
	default:
	case Q3TCGEN_TEXTURE:
		rsurface.texcoordtexture2f               = rsurface.modeltexcoordtexture2f;
		rsurface.texcoordtexture2f_bufferobject  = rsurface.modeltexcoordtexture2f_bufferobject;
		rsurface.texcoordtexture2f_bufferoffset  = rsurface.modeltexcoordtexture2f_bufferoffset;
		break;
	case Q3TCGEN_LIGHTMAP:
		rsurface.texcoordtexture2f               = rsurface.modeltexcoordlightmap2f;
		rsurface.texcoordtexture2f_bufferobject  = rsurface.modeltexcoordlightmap2f_bufferobject;
		rsurface.texcoordtexture2f_bufferoffset  = rsurface.modeltexcoordlightmap2f_bufferoffset;
		break;
	case Q3TCGEN_VECTOR:
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (j = 0, v1 = rsurface.modelvertex3f + 3 * surface->num_firstvertex, out_tc = rsurface.array_generatedtexcoordtexture2f + 2 * surface->num_firstvertex;j < surface->num_vertices;j++, v1 += 3, out_tc += 2)
			{
				out_tc[0] = DotProduct(v1, rsurface.texture->tcgen.parms);
				out_tc[1] = DotProduct(v1, rsurface.texture->tcgen.parms + 3);
			}
		}
		rsurface.texcoordtexture2f               = rsurface.array_generatedtexcoordtexture2f;
		rsurface.texcoordtexture2f_bufferobject  = 0;
		rsurface.texcoordtexture2f_bufferoffset  = 0;
		break;
	case Q3TCGEN_ENVIRONMENT:
		// make environment reflections using a spheremap
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			const float *vertex = rsurface.modelvertex3f + 3 * surface->num_firstvertex;
			const float *normal = rsurface.modelnormal3f + 3 * surface->num_firstvertex;
			float *out_tc = rsurface.array_generatedtexcoordtexture2f + 2 * surface->num_firstvertex;
			for (j = 0;j < surface->num_vertices;j++, vertex += 3, normal += 3, out_tc += 2)
			{
				// identical to Q3A's method, but executed in worldspace so
				// carried models can be shiny too

				float viewer[3], d, reflected[3], worldreflected[3];

				VectorSubtract(rsurface.localvieworigin, vertex, viewer);
				// VectorNormalize(viewer);

				d = DotProduct(normal, viewer);

				reflected[0] = normal[0]*2*d - viewer[0];
				reflected[1] = normal[1]*2*d - viewer[1];
				reflected[2] = normal[2]*2*d - viewer[2];
				// note: this is proportinal to viewer, so we can normalize later

				Matrix4x4_Transform3x3(&rsurface.matrix, reflected, worldreflected);
				VectorNormalize(worldreflected);

				// note: this sphere map only uses world x and z!
				// so positive and negative y will LOOK THE SAME.
				out_tc[0] = 0.5 + 0.5 * worldreflected[1];
				out_tc[1] = 0.5 - 0.5 * worldreflected[2];
			}
		}
		rsurface.texcoordtexture2f               = rsurface.array_generatedtexcoordtexture2f;
		rsurface.texcoordtexture2f_bufferobject  = 0;
		rsurface.texcoordtexture2f_bufferoffset  = 0;
		break;
	}
	// the only tcmod that needs software vertex processing is turbulent, so
	// check for it here and apply the changes if needed
	// and we only support that as the first one
	// (handling a mixture of turbulent and other tcmods would be problematic
	//  without punting it entirely to a software path)
	if (rsurface.texture->tcmods[0].tcmod == Q3TCMOD_TURBULENT)
	{
		amplitude = rsurface.texture->tcmods[0].parms[1];
		animpos = rsurface.texture->tcmods[0].parms[2] + r_refdef.scene.time * rsurface.texture->tcmods[0].parms[3];
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (j = 0, v1 = rsurface.modelvertex3f + 3 * surface->num_firstvertex, in_tc = rsurface.texcoordtexture2f + 2 * surface->num_firstvertex, out_tc = rsurface.array_generatedtexcoordtexture2f + 2 * surface->num_firstvertex;j < surface->num_vertices;j++, v1 += 3, in_tc += 2, out_tc += 2)
			{
				out_tc[0] = in_tc[0] + amplitude * sin(((v1[0] + v1[2]) * 1.0 / 1024.0f + animpos) * M_PI * 2);
				out_tc[1] = in_tc[1] + amplitude * sin(((v1[1]        ) * 1.0 / 1024.0f + animpos) * M_PI * 2);
			}
		}
		rsurface.texcoordtexture2f               = rsurface.array_generatedtexcoordtexture2f;
		rsurface.texcoordtexture2f_bufferobject  = 0;
		rsurface.texcoordtexture2f_bufferoffset  = 0;
	}
	rsurface.texcoordlightmap2f              = rsurface.modeltexcoordlightmap2f;
	rsurface.texcoordlightmap2f_bufferobject = rsurface.modeltexcoordlightmap2f_bufferobject;
	rsurface.texcoordlightmap2f_bufferoffset = rsurface.modeltexcoordlightmap2f_bufferoffset;
	R_Mesh_VertexPointer(rsurface.vertex3f, rsurface.vertex3f_bufferobject, rsurface.vertex3f_bufferoffset);
}

void RSurf_DrawBatch_Simple(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int i, j;
	const msurface_t *surface = texturesurfacelist[0];
	const msurface_t *surface2;
	int firstvertex;
	int endvertex;
	int numvertices;
	int numtriangles;
	// TODO: lock all array ranges before render, rather than on each surface
	if (texturenumsurfaces == 1)
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
	else if (r_batchmode.integer == 2)
	{
		#define MAXBATCHTRIANGLES 4096
		int batchtriangles = 0;
		static int batchelements[MAXBATCHTRIANGLES*3];
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			j = i + 1;
			if (surface->num_triangles > MAXBATCHTRIANGLES)
			{
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
				continue;
			}
			memcpy(batchelements, rsurface.modelelement3i + 3 * surface->num_firsttriangle, surface->num_triangles * sizeof(int[3]));
			batchtriangles = surface->num_triangles;
			firstvertex = surface->num_firstvertex;
			endvertex = surface->num_firstvertex + surface->num_vertices;
			for (;j < texturenumsurfaces;j++)
			{
				surface2 = texturesurfacelist[j];
				if (batchtriangles + surface2->num_triangles > MAXBATCHTRIANGLES)
					break;
				memcpy(batchelements + batchtriangles * 3, rsurface.modelelement3i + 3 * surface2->num_firsttriangle, surface2->num_triangles * sizeof(int[3]));
				batchtriangles += surface2->num_triangles;
				firstvertex = min(firstvertex, surface2->num_firstvertex);
				endvertex = max(endvertex, surface2->num_firstvertex + surface2->num_vertices);
			}
			surface2 = texturesurfacelist[j-1];
			numvertices = endvertex - firstvertex;
			R_Mesh_Draw(firstvertex, numvertices, 0, batchtriangles, batchelements, NULL, 0, 0);
		}
	}
	else if (r_batchmode.integer == 1)
	{
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			for (j = i + 1, surface2 = surface + 1;j < texturenumsurfaces;j++, surface2++)
				if (texturesurfacelist[j] != surface2)
					break;
			surface2 = texturesurfacelist[j-1];
			numvertices = surface2->num_firstvertex + surface2->num_vertices - surface->num_firstvertex;
			numtriangles = surface2->num_firsttriangle + surface2->num_triangles - surface->num_firsttriangle;
			R_Mesh_Draw(surface->num_firstvertex, numvertices, surface->num_firsttriangle, numtriangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
		}
	}
	else
	{
		for (i = 0;i < texturenumsurfaces;i++)
		{
			surface = texturesurfacelist[i];
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
		}
	}
}

static void RSurf_BindLightmapForSurface(const msurface_t *surface)
{
	switch(vid.renderpath)
	{
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		if (r_cg_permutation->fp_Texture_Lightmap ) CG_BindTexture(r_cg_permutation->fp_Texture_Lightmap , surface->lightmaptexture );CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Deluxemap) CG_BindTexture(r_cg_permutation->fp_Texture_Deluxemap, surface->deluxemaptexture);CHECKCGERROR
#endif
		break;
	case RENDERPATH_GL20:
		if (r_glsl_permutation->loc_Texture_Lightmap  >= 0) R_Mesh_TexBind(GL20TU_LIGHTMAP , surface->lightmaptexture );
		if (r_glsl_permutation->loc_Texture_Deluxemap >= 0) R_Mesh_TexBind(GL20TU_DELUXEMAP, surface->deluxemaptexture);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		R_Mesh_TexBind(0, surface->lightmaptexture);
		break;
	}
}

static void RSurf_BindReflectionForSurface(const msurface_t *surface)
{
	// pick the closest matching water plane and bind textures
	int planeindex, vertexindex;
	float d, bestd;
	vec3_t vert;
	const float *v;
	r_waterstate_waterplane_t *p, *bestp;
	bestd = 0;
	bestp = NULL;
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		d = 0;
		for (vertexindex = 0, v = rsurface.modelvertex3f + surface->num_firstvertex * 3;vertexindex < surface->num_vertices;vertexindex++, v += 3)
		{
			Matrix4x4_Transform(&rsurface.matrix, v, vert);
			d += fabs(PlaneDiff(vert, &p->plane));
		}
		if (bestd > d || !bestp)
		{
			bestd = d;
			bestp = p;
		}
	}
	switch(vid.renderpath)
	{
	case RENDERPATH_CGGL:
#ifdef SUPPORTCG
		if (r_cg_permutation->fp_Texture_Refraction) CG_BindTexture(r_cg_permutation->fp_Texture_Refraction, bestp ? bestp->texture_refraction : r_texture_black);CHECKCGERROR
		if (r_cg_permutation->fp_Texture_Reflection) CG_BindTexture(r_cg_permutation->fp_Texture_Reflection, bestp ? bestp->texture_reflection : r_texture_black);CHECKCGERROR
#endif
		break;
	case RENDERPATH_GL20:
		if (r_glsl_permutation->loc_Texture_Refraction >= 0) R_Mesh_TexBind(GL20TU_REFRACTION, bestp ? bestp->texture_refraction : r_texture_black);
		if (r_glsl_permutation->loc_Texture_Reflection >= 0) R_Mesh_TexBind(GL20TU_REFLECTION, bestp ? bestp->texture_reflection : r_texture_black);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		break;
	}
}

static void RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int i;
	const msurface_t *surface;
	if (r_waterstate.renderingscene)
		return;
	for (i = 0;i < texturenumsurfaces;i++)
	{
		surface = texturesurfacelist[i];
		RSurf_BindLightmapForSurface(surface);
		RSurf_BindReflectionForSurface(surface);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
	}
}

static void RSurf_DrawBatch_WithLightmapSwitching(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int i;
	int j;
	const msurface_t *surface = texturesurfacelist[0];
	const msurface_t *surface2;
	int firstvertex;
	int endvertex;
	int numvertices;
	int numtriangles;
	if (texturenumsurfaces == 1)
	{
		RSurf_BindLightmapForSurface(surface);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
	}
	else if (r_batchmode.integer == 2)
	{
#define MAXBATCHTRIANGLES 4096
		int batchtriangles = 0;
		static int batchelements[MAXBATCHTRIANGLES*3];
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			RSurf_BindLightmapForSurface(surface);
			j = i + 1;
			if (surface->num_triangles > MAXBATCHTRIANGLES)
			{
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
				continue;
			}
			memcpy(batchelements, rsurface.modelelement3i + 3 * surface->num_firsttriangle, surface->num_triangles * sizeof(int[3]));
			batchtriangles = surface->num_triangles;
			firstvertex = surface->num_firstvertex;
			endvertex = surface->num_firstvertex + surface->num_vertices;
			for (;j < texturenumsurfaces;j++)
			{
				surface2 = texturesurfacelist[j];
				if (surface2->lightmaptexture != surface->lightmaptexture || batchtriangles + surface2->num_triangles > MAXBATCHTRIANGLES)
					break;
				memcpy(batchelements + batchtriangles * 3, rsurface.modelelement3i + 3 * surface2->num_firsttriangle, surface2->num_triangles * sizeof(int[3]));
				batchtriangles += surface2->num_triangles;
				firstvertex = min(firstvertex, surface2->num_firstvertex);
				endvertex = max(endvertex, surface2->num_firstvertex + surface2->num_vertices);
			}
			surface2 = texturesurfacelist[j-1];
			numvertices = endvertex - firstvertex;
			R_Mesh_Draw(firstvertex, numvertices, 0, batchtriangles, batchelements, NULL, 0, 0);
		}
	}
	else if (r_batchmode.integer == 1)
	{
#if 0
		Con_Printf("%s batch sizes ignoring lightmap:", rsurface.texture->name);
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			for (j = i + 1, surface2 = surface + 1;j < texturenumsurfaces;j++, surface2++)
				if (texturesurfacelist[j] != surface2)
					break;
			Con_Printf(" %i", j - i);
		}
		Con_Printf("\n");
		Con_Printf("%s batch sizes honoring lightmap:", rsurface.texture->name);
#endif
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			RSurf_BindLightmapForSurface(surface);
			for (j = i + 1, surface2 = surface + 1;j < texturenumsurfaces;j++, surface2++)
				if (texturesurfacelist[j] != surface2 || texturesurfacelist[j]->lightmaptexture != surface->lightmaptexture)
					break;
#if 0
			Con_Printf(" %i", j - i);
#endif
			surface2 = texturesurfacelist[j-1];
			numvertices = surface2->num_firstvertex + surface2->num_vertices - surface->num_firstvertex;
			numtriangles = surface2->num_firsttriangle + surface2->num_triangles - surface->num_firsttriangle;
			R_Mesh_Draw(surface->num_firstvertex, numvertices, surface->num_firsttriangle, numtriangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
		}
#if 0
		Con_Printf("\n");
#endif
	}
	else
	{
		for (i = 0;i < texturenumsurfaces;i++)
		{
			surface = texturesurfacelist[i];
			RSurf_BindLightmapForSurface(surface);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
		}
	}
}

static void RSurf_DrawBatch_ShowSurfaces(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int j;
	int texturesurfaceindex;
	if (r_showsurfaces.integer == 2)
	{
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (j = 0;j < surface->num_triangles;j++)
			{
				float f = ((j + surface->num_firsttriangle) & 31) * (1.0f / 31.0f) * r_refdef.view.colorscale;
				GL_Color(f, f, f, 1);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle + j, 1, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
			}
		}
	}
	else
	{
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			int k = (int)(((size_t)surface) / sizeof(msurface_t));
			GL_Color((k & 15) * (1.0f / 16.0f) * r_refdef.view.colorscale, ((k >> 4) & 15) * (1.0f / 16.0f) * r_refdef.view.colorscale, ((k >> 8) & 15) * (1.0f / 16.0f) * r_refdef.view.colorscale, 1);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
		}
	}
}

static void RSurf_DrawBatch_GL11_MakeFullbrightLightmapColorArray(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int i;
	const float *v;
	float *c2;
	for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
	{
		const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
		for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c2 += 4)
		{
			c2[0] = 0.5;
			c2[1] = 0.5;
			c2[2] = 0.5;
			c2[3] = 1;
		}
	}
	rsurface.lightmapcolor4f = rsurface.array_color4f;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
}

static void RSurf_DrawBatch_GL11_ApplyFog(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int i;
	float f;
	const float *v;
	const float *c;
	float *c2;
	if (rsurface.lightmapcolor4f)
	{
		// generate color arrays for the surfaces in this list
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4, c2 += 4)
			{
				f = RSurf_FogVertex(v);
				c2[0] = c[0] * f;
				c2[1] = c[1] * f;
				c2[2] = c[2] * f;
				c2[3] = c[3];
			}
		}
	}
	else
	{
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c2 += 4)
			{
				f = RSurf_FogVertex(v);
				c2[0] = f;
				c2[1] = f;
				c2[2] = f;
				c2[3] = 1;
			}
		}
	}
	rsurface.lightmapcolor4f = rsurface.array_color4f;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
}

static void RSurf_DrawBatch_GL11_ApplyFogToFinishedVertexColors(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int i;
	float f;
	const float *v;
	const float *c;
	float *c2;
	if (!rsurface.lightmapcolor4f)
		return;
	// generate color arrays for the surfaces in this list
	for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
	{
		const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
		for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4, c2 += 4)
		{
			f = RSurf_FogVertex(v);
			c2[0] = c[0] * f + r_refdef.fogcolor[0] * (1 - f);
			c2[1] = c[1] * f + r_refdef.fogcolor[1] * (1 - f);
			c2[2] = c[2] * f + r_refdef.fogcolor[2] * (1 - f);
			c2[3] = c[3];
		}
	}
	rsurface.lightmapcolor4f = rsurface.array_color4f;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
}

static void RSurf_DrawBatch_GL11_ApplyColor(int texturenumsurfaces, const msurface_t **texturesurfacelist, float r, float g, float b, float a)
{
	int texturesurfaceindex;
	int i;
	const float *c;
	float *c2;
	if (!rsurface.lightmapcolor4f)
		return;
	for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
	{
		const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
		for (i = 0, c = (rsurface.lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, c += 4, c2 += 4)
		{
			c2[0] = c[0] * r;
			c2[1] = c[1] * g;
			c2[2] = c[2] * b;
			c2[3] = c[3] * a;
		}
	}
	rsurface.lightmapcolor4f = rsurface.array_color4f;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
}

static void RSurf_DrawBatch_GL11_ApplyAmbient(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int i;
	const float *c;
	float *c2;
	if (!rsurface.lightmapcolor4f)
		return;
	for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
	{
		const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
		for (i = 0, c = (rsurface.lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, c += 4, c2 += 4)
		{
			c2[0] = c[0] + r_refdef.scene.ambient;
			c2[1] = c[1] + r_refdef.scene.ambient;
			c2[2] = c[2] + r_refdef.scene.ambient;
			c2[3] = c[3];
		}
	}
	rsurface.lightmapcolor4f = rsurface.array_color4f;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
}

static void RSurf_DrawBatch_GL11_Lightmap(int texturenumsurfaces, const msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize
	rsurface.lightmapcolor4f = NULL;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog(texturenumsurfaces, texturesurfacelist);
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, r, g, b, a);
	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch_WithLightmapSwitching(texturenumsurfaces, texturesurfacelist);
}

static void RSurf_DrawBatch_GL11_Unlit(int texturenumsurfaces, const msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize applyfog && applycolor case
	// just apply fog if necessary, and tint the fog color array if necessary
	rsurface.lightmapcolor4f = NULL;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog(texturenumsurfaces, texturesurfacelist);
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, r, g, b, a);
	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
}

static void RSurf_DrawBatch_GL11_VertexColor(int texturenumsurfaces, const msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	int texturesurfaceindex;
	int i;
	float *c;
	// TODO: optimize
	if (texturesurfacelist[0]->lightmapinfo)
	{
		// generate color arrays for the surfaces in this list
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (i = 0, c = rsurface.array_color4f + 4 * surface->num_firstvertex;i < surface->num_vertices;i++, c += 4)
			{
				if (surface->lightmapinfo->samples)
				{
					const unsigned char *lm = surface->lightmapinfo->samples + (rsurface.modellightmapoffsets + surface->num_firstvertex)[i];
					float scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[0]] * (1.0f / 32768.0f);
					VectorScale(lm, scale, c);
					if (surface->lightmapinfo->styles[1] != 255)
					{
						int size3 = ((surface->lightmapinfo->extents[0]>>4)+1)*((surface->lightmapinfo->extents[1]>>4)+1)*3;
						lm += size3;
						scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[1]] * (1.0f / 32768.0f);
						VectorMA(c, scale, lm, c);
						if (surface->lightmapinfo->styles[2] != 255)
						{
							lm += size3;
							scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[2]] * (1.0f / 32768.0f);
							VectorMA(c, scale, lm, c);
							if (surface->lightmapinfo->styles[3] != 255)
							{
								lm += size3;
								scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[3]] * (1.0f / 32768.0f);
								VectorMA(c, scale, lm, c);
							}
						}
					}
				}
				else
					VectorClear(c);
				c[3] = 1;
			}
		}
		rsurface.lightmapcolor4f = rsurface.array_color4f;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
	}
	else
	{
		rsurface.lightmapcolor4f = rsurface.modellightmapcolor4f;
		rsurface.lightmapcolor4f_bufferobject = rsurface.modellightmapcolor4f_bufferobject;
		rsurface.lightmapcolor4f_bufferoffset = rsurface.modellightmapcolor4f_bufferoffset;
	}
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog(texturenumsurfaces, texturesurfacelist);
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, r, g, b, a);
	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
}

static void RSurf_DrawBatch_GL11_ApplyVertexShade(int texturenumsurfaces, const msurface_t **texturesurfacelist, float *r, float *g, float *b, float *a, qboolean *applycolor)
{
	int texturesurfaceindex;
	int i;
	float f;
	float alpha;
	const float *v;
	const float *n;
	float *c;
	vec3_t ambientcolor;
	vec3_t diffusecolor;
	vec3_t lightdir;
	// TODO: optimize
	// model lighting
	VectorCopy(rsurface.modellight_lightdir, lightdir);
	f = 0.5f * r_refdef.lightmapintensity;
	ambientcolor[0] = rsurface.modellight_ambient[0] * *r * f;
	ambientcolor[1] = rsurface.modellight_ambient[1] * *g * f;
	ambientcolor[2] = rsurface.modellight_ambient[2] * *b * f;
	diffusecolor[0] = rsurface.modellight_diffuse[0] * *r * f;
	diffusecolor[1] = rsurface.modellight_diffuse[1] * *g * f;
	diffusecolor[2] = rsurface.modellight_diffuse[2] * *b * f;
	alpha = *a;
	if (VectorLength2(diffusecolor) > 0 && rsurface.normal3f)
	{
		// generate color arrays for the surfaces in this list
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			int numverts = surface->num_vertices;
			v = rsurface.vertex3f + 3 * surface->num_firstvertex;
			n = rsurface.normal3f + 3 * surface->num_firstvertex;
			c = rsurface.array_color4f + 4 * surface->num_firstvertex;
			// q3-style directional shading
			for (i = 0;i < numverts;i++, v += 3, n += 3, c += 4)
			{
				if ((f = DotProduct(n, lightdir)) > 0)
					VectorMA(ambientcolor, f, diffusecolor, c);
				else
					VectorCopy(ambientcolor, c);
				c[3] = alpha;
			}
		}
		*r = 1;
		*g = 1;
		*b = 1;
		*a = 1;
		rsurface.lightmapcolor4f = rsurface.array_color4f;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
		*applycolor = false;
	}
	else
	{
		*r = ambientcolor[0];
		*g = ambientcolor[1];
		*b = ambientcolor[2];
		rsurface.lightmapcolor4f = NULL;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
	}
}

static void RSurf_DrawBatch_GL11_VertexShade(int texturenumsurfaces, const msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	RSurf_DrawBatch_GL11_ApplyVertexShade(texturenumsurfaces, texturesurfacelist, &r, &g, &b, &a, &applycolor);
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog(texturenumsurfaces, texturesurfacelist);
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, r, g, b, a);
	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
}

void RSurf_SetupDepthAndCulling(void)
{
	// submodels are biased to avoid z-fighting with world surfaces that they
	// may be exactly overlapping (avoids z-fighting artifacts on certain
	// doors and things in Quake maps)
	GL_DepthRange(0, (rsurface.texture->currentmaterialflags & MATERIALFLAG_SHORTDEPTHRANGE) ? 0.0625 : 1);
	GL_PolygonOffset(rsurface.basepolygonfactor + rsurface.texture->biaspolygonfactor, rsurface.basepolygonoffset + rsurface.texture->biaspolygonoffset);
	GL_DepthTest(!(rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST));
	GL_CullFace((rsurface.texture->currentmaterialflags & MATERIALFLAG_NOCULLFACE) ? GL_NONE : r_refdef.view.cullface_back);
}

static void R_DrawTextureSurfaceList_Sky(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	// transparent sky would be ridiculous
	if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
		return;
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	skyrenderlater = true;
	RSurf_SetupDepthAndCulling();
	GL_DepthMask(true);
	// LordHavoc: HalfLife maps have freaky skypolys so don't use
	// skymasking on them, and Quake3 never did sky masking (unlike
	// software Quake and software Quake2), so disable the sky masking
	// in Quake3 maps as it causes problems with q3map2 sky tricks,
	// and skymasking also looks very bad when noclipping outside the
	// level, so don't use it then either.
	if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->type == mod_brushq1 && r_q1bsp_skymasking.integer && !r_refdef.viewcache.world_novis)
	{
		GL_Color(r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2], 1);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		if (skyrendermasked)
		{
			R_SetupShader_DepthOrShadow();
			// depth-only (masking)
			GL_ColorMask(0,0,0,0);
			// just to make sure that braindead drivers don't draw
			// anything despite that colormask...
			GL_BlendFunc(GL_ZERO, GL_ONE);
		}
		else
		{
			R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
			// fog sky
			GL_BlendFunc(GL_ONE, GL_ZERO);
		}
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
		if (skyrendermasked)
			GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	}
	R_Mesh_ResetTextureState();
	GL_Color(1, 1, 1, 1);
}

extern rtexture_t *r_shadow_prepasslightingdiffusetexture;
extern rtexture_t *r_shadow_prepasslightingspeculartexture;
static void R_DrawTextureSurfaceList_GL20(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean prepass)
{
	if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION)))
		return;
	RSurf_PrepareVerticesForBatch(true, true, texturenumsurfaces, texturesurfacelist);
	if (prepass)
	{
		// render screenspace normalmap to texture
		GL_DepthMask(true);
		R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_DEFERREDGEOMETRY);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION)) && !r_waterstate.renderingscene)
	{
		// render water or distortion background, then blend surface on top
		GL_DepthMask(true);
		R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BACKGROUND);
		RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(texturenumsurfaces, texturesurfacelist);
		GL_DepthMask(false);
		R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE);
		if (rsurface.uselightmaptexture && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT))
			RSurf_DrawBatch_WithLightmapSwitching(texturenumsurfaces, texturesurfacelist);
		else
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else
	{
		// render surface normally
		GL_DepthMask(writedepth && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED));
		R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE);
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(texturenumsurfaces, texturesurfacelist);
		else if (rsurface.uselightmaptexture && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT))
			RSurf_DrawBatch_WithLightmapSwitching(texturenumsurfaces, texturesurfacelist);
		else
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
}

static void R_DrawTextureSurfaceList_GL13(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	// OpenGL 1.3 path - anything not completely ancient
	int texturesurfaceindex;
	qboolean applycolor;
	qboolean applyfog;
	int layerindex;
	const texturelayer_t *layer;
	RSurf_PrepareVerticesForBatch(true, false, texturenumsurfaces, texturesurfacelist);

	for (layerindex = 0, layer = rsurface.texture->currentlayers;layerindex < rsurface.texture->currentnumlayers;layerindex++, layer++)
	{
		vec4_t layercolor;
		int layertexrgbscale;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
		{
			if (layerindex == 0)
				GL_AlphaTest(true);
			else
			{
				GL_AlphaTest(false);
				qglDepthFunc(GL_EQUAL);CHECKGLERROR
			}
		}
		GL_DepthMask(layer->depthmask && writedepth);
		GL_BlendFunc(layer->blendfunc1, layer->blendfunc2);
		if (layer->color[0] > 2 || layer->color[1] > 2 || layer->color[2] > 2)
		{
			layertexrgbscale = 4;
			VectorScale(layer->color, 0.25f, layercolor);
		}
		else if (layer->color[0] > 1 || layer->color[1] > 1 || layer->color[2] > 1)
		{
			layertexrgbscale = 2;
			VectorScale(layer->color, 0.5f, layercolor);
		}
		else
		{
			layertexrgbscale = 1;
			VectorScale(layer->color, 1.0f, layercolor);
		}
		layercolor[3] = layer->color[3];
		applycolor = layercolor[0] != 1 || layercolor[1] != 1 || layercolor[2] != 1 || layercolor[3] != 1;
		R_Mesh_ColorPointer(NULL, 0, 0);
		applyfog = r_refdef.fogenabled && (rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED);
		switch (layer->type)
		{
		case TEXTURELAYERTYPE_LITTEXTURE:
			// single-pass lightmapped texture with 2x rgbscale
			R_Mesh_TexBind(0, r_texture_white);
			R_Mesh_TexMatrix(0, NULL);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
			R_Mesh_TexCoordPointer(0, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
			R_Mesh_TexBind(1, layer->texture);
			R_Mesh_TexMatrix(1, &layer->texmatrix);
			R_Mesh_TexCombine(1, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
			R_Mesh_TexCoordPointer(1, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
				RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else if (rsurface.uselightmaptexture)
				RSurf_DrawBatch_GL11_Lightmap(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else
				RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			// singletexture unlit texture with transparency support
			R_Mesh_TexBind(0, layer->texture);
			R_Mesh_TexMatrix(0, &layer->texmatrix);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
			R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			R_Mesh_TexBind(1, 0);
			R_Mesh_TexCoordPointer(1, 2, NULL, 0, 0);
			RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			// singletexture fogging
			if (layer->texture)
			{
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
				R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			}
			else
			{
				R_Mesh_TexBind(0, 0);
				R_Mesh_TexCoordPointer(0, 2, NULL, 0, 0);
			}
			R_Mesh_TexBind(1, 0);
			R_Mesh_TexCoordPointer(1, 2, NULL, 0, 0);
			// generate a color array for the fog pass
			R_Mesh_ColorPointer(rsurface.array_color4f, 0, 0);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				int i;
				float f;
				const float *v;
				float *c;
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
				{
					f = 1 - RSurf_FogVertex(v);
					c[0] = layercolor[0];
					c[1] = layercolor[1];
					c[2] = layercolor[2];
					c[3] = f * layercolor[3];
				}
			}
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
			break;
		default:
			Con_Printf("R_DrawTextureSurfaceList: unknown layer type %i\n", layer->type);
		}
	}
	CHECKGLERROR
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_LEQUAL);CHECKGLERROR
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList_GL11(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	// OpenGL 1.1 - crusty old voodoo path
	int texturesurfaceindex;
	qboolean applyfog;
	int layerindex;
	const texturelayer_t *layer;
	RSurf_PrepareVerticesForBatch(true, false, texturenumsurfaces, texturesurfacelist);

	for (layerindex = 0, layer = rsurface.texture->currentlayers;layerindex < rsurface.texture->currentnumlayers;layerindex++, layer++)
	{
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
		{
			if (layerindex == 0)
				GL_AlphaTest(true);
			else
			{
				GL_AlphaTest(false);
				qglDepthFunc(GL_EQUAL);CHECKGLERROR
			}
		}
		GL_DepthMask(layer->depthmask && writedepth);
		GL_BlendFunc(layer->blendfunc1, layer->blendfunc2);
		R_Mesh_ColorPointer(NULL, 0, 0);
		applyfog = r_refdef.fogenabled && (rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED);
		switch (layer->type)
		{
		case TEXTURELAYERTYPE_LITTEXTURE:
			if (layer->blendfunc1 == GL_ONE && layer->blendfunc2 == GL_ZERO)
			{
				// two-pass lit texture with 2x rgbscale
				// first the lightmap pass
				R_Mesh_TexBind(0, r_texture_white);
				R_Mesh_TexMatrix(0, NULL);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				else if (rsurface.uselightmaptexture)
					RSurf_DrawBatch_GL11_Lightmap(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				else
					RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				// then apply the texture to it
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
				RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layer->color[0] * 0.5f, layer->color[1] * 0.5f, layer->color[2] * 0.5f, layer->color[3], layer->color[0] != 2 || layer->color[1] != 2 || layer->color[2] != 2 || layer->color[3] != 1, false);
			}
			else
			{
				// single pass vertex-lighting-only texture with 1x rgbscale and transparency support
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
				else
					RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			}
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			// singletexture unlit texture with transparency support
			R_Mesh_TexBind(0, layer->texture);
			R_Mesh_TexMatrix(0, &layer->texmatrix);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
			R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			// singletexture fogging
			if (layer->texture)
			{
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			}
			else
			{
				R_Mesh_TexBind(0, 0);
				R_Mesh_TexCoordPointer(0, 2, NULL, 0, 0);
			}
			// generate a color array for the fog pass
			R_Mesh_ColorPointer(rsurface.array_color4f, 0, 0);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				int i;
				float f;
				const float *v;
				float *c;
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
				{
					f = 1 - RSurf_FogVertex(v);
					c[0] = layer->color[0];
					c[1] = layer->color[1];
					c[2] = layer->color[2];
					c[3] = f * layer->color[3];
				}
			}
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
			break;
		default:
			Con_Printf("R_DrawTextureSurfaceList: unknown layer type %i\n", layer->type);
		}
	}
	CHECKGLERROR
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_LEQUAL);CHECKGLERROR
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList_ShowSurfaces3(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	float c[4];

	GL_AlphaTest(false);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);

	if(rsurface.texture && rsurface.texture->currentskinframe)
	{
		memcpy(c, rsurface.texture->currentskinframe->avgcolor, sizeof(c));
		c[3] *= rsurface.texture->currentalpha;
	}
	else
	{
		c[0] = 1;
		c[1] = 0;
		c[2] = 1;
		c[3] = 1;
	}

	if (rsurface.texture->pantstexture || rsurface.texture->shirttexture)
	{
		c[0] = 0.5 * (rsurface.colormap_pantscolor[0] * 0.3 + rsurface.colormap_shirtcolor[0] * 0.7);
		c[1] = 0.5 * (rsurface.colormap_pantscolor[1] * 0.3 + rsurface.colormap_shirtcolor[1] * 0.7);
		c[2] = 0.5 * (rsurface.colormap_pantscolor[2] * 0.3 + rsurface.colormap_shirtcolor[2] * 0.7);
	}

	// brighten it up (as texture value 127 means "unlit")
	c[0] *= 2 * r_refdef.view.colorscale;
	c[1] *= 2 * r_refdef.view.colorscale;
	c[2] *= 2 * r_refdef.view.colorscale;

	if(rsurface.texture->currentmaterialflags & MATERIALFLAG_WATERALPHA)
		c[3] *= r_wateralpha.value;

	if(rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHA && c[3] != 1)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else if(rsurface.texture->currentmaterialflags & MATERIALFLAG_ADD)
	{
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_DepthMask(false);
	}
	else if(rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // can't do alpha test without texture, so let's blend instead
		GL_DepthMask(false);
	}
	else if(rsurface.texture->currentmaterialflags & MATERIALFLAG_CUSTOMBLEND)
	{
		GL_BlendFunc(rsurface.texture->customblendfunc[0], rsurface.texture->customblendfunc[1]);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(writedepth);
	}

	rsurface.lightmapcolor4f = NULL;

	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
	{
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);

		rsurface.lightmapcolor4f = NULL;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
	{
		qboolean applycolor = true;
		float one = 1.0;

		RSurf_PrepareVerticesForBatch(true, false, texturenumsurfaces, texturesurfacelist);

		r_refdef.lightmapintensity = 1;
		RSurf_DrawBatch_GL11_ApplyVertexShade(texturenumsurfaces, texturesurfacelist, &one, &one, &one, &one, &applycolor);
		r_refdef.lightmapintensity = 0; // we're in showsurfaces, after all
	}
	else
	{
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);

		rsurface.lightmapcolor4f = rsurface.modellightmapcolor4f;
		rsurface.lightmapcolor4f_bufferobject = rsurface.modellightmapcolor4f_bufferobject;
		rsurface.lightmapcolor4f_bufferoffset = rsurface.modellightmapcolor4f_bufferoffset;
	}

	if(!rsurface.lightmapcolor4f)
		RSurf_DrawBatch_GL11_MakeFullbrightLightmapColorArray(texturenumsurfaces, texturesurfacelist);

	RSurf_DrawBatch_GL11_ApplyAmbient(texturenumsurfaces, texturesurfacelist);
	RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, c[0], c[1], c[2], c[3]);
	if(r_refdef.fogenabled)
		RSurf_DrawBatch_GL11_ApplyFogToFinishedVertexColors(texturenumsurfaces, texturesurfacelist);

	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
}

static void R_DrawWorldTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean prepass)
{
	CHECKGLERROR
	RSurf_SetupDepthAndCulling();
	if (r_showsurfaces.integer == 3 && !prepass && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY))
	{
		R_DrawTextureSurfaceList_ShowSurfaces3(texturenumsurfaces, texturesurfacelist, writedepth);
		return;
	}
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		R_DrawTextureSurfaceList_GL20(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
		break;
	case RENDERPATH_GL13:
		R_DrawTextureSurfaceList_GL13(texturenumsurfaces, texturesurfacelist, writedepth);
		break;
	case RENDERPATH_GL11:
		R_DrawTextureSurfaceList_GL11(texturenumsurfaces, texturesurfacelist, writedepth);
		break;
	}
	CHECKGLERROR
}

static void R_DrawModelTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean prepass)
{
	CHECKGLERROR
	RSurf_SetupDepthAndCulling();
	if (r_showsurfaces.integer == 3 && !prepass && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY))
	{
		R_DrawTextureSurfaceList_ShowSurfaces3(texturenumsurfaces, texturesurfacelist, writedepth);
		return;
	}
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		R_DrawTextureSurfaceList_GL20(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
		break;
	case RENDERPATH_GL13:
		R_DrawTextureSurfaceList_GL13(texturenumsurfaces, texturesurfacelist, writedepth);
		break;
	case RENDERPATH_GL11:
		R_DrawTextureSurfaceList_GL11(texturenumsurfaces, texturesurfacelist, writedepth);
		break;
	}
	CHECKGLERROR
}

static void R_DrawSurface_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i, j;
	int texturenumsurfaces, endsurface;
	texture_t *texture;
	const msurface_t *surface;
	const msurface_t *texturesurfacelist[256];

	// if the model is static it doesn't matter what value we give for
	// wantnormals and wanttangents, so this logic uses only rules applicable
	// to a model, knowing that they are meaningless otherwise
	if (ent == r_refdef.scene.worldentity)
		RSurf_ActiveWorldEntity();
	else if (r_showsurfaces.integer && r_showsurfaces.integer != 3)
		RSurf_ActiveModelEntity(ent, false, false, false);
	else
	{
		switch (vid.renderpath)
		{
		case RENDERPATH_GL20:
		case RENDERPATH_CGGL:
			RSurf_ActiveModelEntity(ent, true, true, false);
			break;
		case RENDERPATH_GL13:
		case RENDERPATH_GL11:
			RSurf_ActiveModelEntity(ent, true, false, false);
			break;
		}
	}

	if (r_transparentdepthmasking.integer)
	{
		qboolean setup = false;
		for (i = 0;i < numsurfaces;i = j)
		{
			j = i + 1;
			surface = rsurface.modelsurfaces + surfacelist[i];
			texture = surface->texture;
			rsurface.texture = R_GetCurrentTexture(texture);
			rsurface.uselightmaptexture = surface->lightmaptexture != NULL;
			// scan ahead until we find a different texture
			endsurface = min(i + 1024, numsurfaces);
			texturenumsurfaces = 0;
			texturesurfacelist[texturenumsurfaces++] = surface;
			for (;j < endsurface;j++)
			{
				surface = rsurface.modelsurfaces + surfacelist[j];
				if (texture != surface->texture || rsurface.uselightmaptexture != (surface->lightmaptexture != NULL))
					break;
				texturesurfacelist[texturenumsurfaces++] = surface;
			}
			if (!(rsurface.texture->currentmaterialflags & MATERIALFLAG_TRANSDEPTH))
				continue;
			// render the range of surfaces as depth
			if (!setup)
			{
				setup = true;
				GL_ColorMask(0,0,0,0);
				GL_Color(1,1,1,1);
				GL_DepthTest(true);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_DepthMask(true);
				GL_AlphaTest(false);
				R_Mesh_ColorPointer(NULL, 0, 0);
				R_Mesh_ResetTextureState();
				R_SetupShader_DepthOrShadow();
			}
			RSurf_SetupDepthAndCulling();
			RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
		}
		if (setup)
			GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	}

	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		surface = rsurface.modelsurfaces + surfacelist[i];
		texture = surface->texture;
		rsurface.texture = R_GetCurrentTexture(texture);
		rsurface.uselightmaptexture = surface->lightmaptexture != NULL;
		// scan ahead until we find a different texture
		endsurface = min(i + 1024, numsurfaces);
		texturenumsurfaces = 0;
		texturesurfacelist[texturenumsurfaces++] = surface;
		for (;j < endsurface;j++)
		{
			surface = rsurface.modelsurfaces + surfacelist[j];
			if (texture != surface->texture || rsurface.uselightmaptexture != (surface->lightmaptexture != NULL))
				break;
			texturesurfacelist[texturenumsurfaces++] = surface;
		}
		// render the range of surfaces
		if (ent == r_refdef.scene.worldentity)
			R_DrawWorldTextureSurfaceList(texturenumsurfaces, texturesurfacelist, false, false);
		else
			R_DrawModelTextureSurfaceList(texturenumsurfaces, texturesurfacelist, false, false);
	}
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
	GL_AlphaTest(false);
}

static void R_ProcessTransparentTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, const entity_render_t *queueentity)
{
	// transparent surfaces get pushed off into the transparent queue
	int surfacelistindex;
	const msurface_t *surface;
	vec3_t tempcenter, center;
	for (surfacelistindex = 0;surfacelistindex < texturenumsurfaces;surfacelistindex++)
	{
		surface = texturesurfacelist[surfacelistindex];
		tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
		tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
		tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
		Matrix4x4_Transform(&rsurface.matrix, tempcenter, center);
		if (queueentity->transparent_offset) // transparent offset
		{
			center[0] += r_refdef.view.forward[0]*queueentity->transparent_offset;
			center[1] += r_refdef.view.forward[1]*queueentity->transparent_offset;
			center[2] += r_refdef.view.forward[2]*queueentity->transparent_offset;
		}
		R_MeshQueue_AddTransparent(rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST ? r_refdef.view.origin : center, R_DrawSurface_TransparentCallback, queueentity, surface - rsurface.modelsurfaces, rsurface.rtlight);
	}
}

static void R_ProcessWorldTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean depthonly, qboolean prepass)
{
	const entity_render_t *queueentity = r_refdef.scene.worldentity;
	CHECKGLERROR
	if (depthonly)
	{
		if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHATEST)))
			return;
		if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION)))
			return;
		RSurf_SetupDepthAndCulling();
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if (prepass)
	{
		if (!rsurface.texture->currentnumlayers)
			return;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
		else
			R_DrawWorldTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
	}
	else if (r_showsurfaces.integer && !r_refdef.view.showdebug && !prepass)
	{
		RSurf_SetupDepthAndCulling();
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(0, 0, 0, 1);
		GL_DepthTest(writedepth);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if (r_showsurfaces.integer && r_showsurfaces.integer != 3 && !prepass)
	{
		RSurf_SetupDepthAndCulling();
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthTest(true);
		RSurf_DrawBatch_ShowSurfaces(texturenumsurfaces, texturesurfacelist);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY)
		R_DrawTextureSurfaceList_Sky(texturenumsurfaces, texturesurfacelist);
	else if (!rsurface.texture->currentnumlayers)
		return;
	else if (((rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED) || (r_showsurfaces.integer == 3 && (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST))) && queueentity)
	{
		// in the deferred case, transparent surfaces were queued during prepass
		if (!r_shadow_usingdeferredprepass)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
	}
	else
	{
		// the alphatest check is to make sure we write depth for anything we skipped on the depth-only pass earlier
		R_DrawWorldTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth || (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST), prepass);
	}
	CHECKGLERROR
}

void R_QueueWorldSurfaceList(int numsurfaces, const msurface_t **surfacelist, int flagsmask, qboolean writedepth, qboolean depthonly, qboolean prepass)
{
	int i, j;
	texture_t *texture;
	// break the surface list down into batches by texture and use of lightmapping
	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		// texture is the base texture pointer, rsurface.texture is the
		// current frame/skin the texture is directing us to use (for example
		// if a model has 2 skins and it is on skin 1, then skin 0 tells us to
		// use skin 1 instead)
		texture = surfacelist[i]->texture;
		rsurface.texture = R_GetCurrentTexture(texture);
		rsurface.uselightmaptexture = surfacelist[i]->lightmaptexture != NULL && !depthonly && !prepass;
		if (!(rsurface.texture->currentmaterialflags & flagsmask) || (rsurface.texture->currentmaterialflags & MATERIALFLAG_NODRAW))
		{
			// if this texture is not the kind we want, skip ahead to the next one
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
			continue;
		}
		// simply scan ahead until we find a different texture or lightmap state
		for (;j < numsurfaces && texture == surfacelist[j]->texture && rsurface.uselightmaptexture == (surfacelist[j]->lightmaptexture != NULL);j++)
			;
		// render the range of surfaces
		R_ProcessWorldTextureSurfaceList(j - i, surfacelist + i, writedepth, depthonly, prepass);
	}
}

static void R_ProcessModelTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean depthonly, const entity_render_t *queueentity, qboolean prepass)
{
	CHECKGLERROR
	if (depthonly)
	{
		if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHATEST)))
			return;
		if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION)))
			return;
		RSurf_SetupDepthAndCulling();
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if (prepass)
	{
		if (!rsurface.texture->currentnumlayers)
			return;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
		else
			R_DrawModelTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
	}
	else if (r_showsurfaces.integer && !r_refdef.view.showdebug)
	{
		RSurf_SetupDepthAndCulling();
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(0, 0, 0, 1);
		GL_DepthTest(writedepth);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if (r_showsurfaces.integer && r_showsurfaces.integer != 3)
	{
		RSurf_SetupDepthAndCulling();
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthTest(true);
		RSurf_DrawBatch_ShowSurfaces(texturenumsurfaces, texturesurfacelist);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY)
		R_DrawTextureSurfaceList_Sky(texturenumsurfaces, texturesurfacelist);
	else if (!rsurface.texture->currentnumlayers)
		return;
	else if (((rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED) || (r_showsurfaces.integer == 3 && (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST))) && queueentity)
	{
		// in the deferred case, transparent surfaces were queued during prepass
		if (!r_shadow_usingdeferredprepass)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
	}
	else
	{
		// the alphatest check is to make sure we write depth for anything we skipped on the depth-only pass earlier
		R_DrawModelTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth || (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST), prepass);
	}
	CHECKGLERROR
}

void R_QueueModelSurfaceList(entity_render_t *ent, int numsurfaces, const msurface_t **surfacelist, int flagsmask, qboolean writedepth, qboolean depthonly, qboolean prepass)
{
	int i, j;
	texture_t *texture;
	// break the surface list down into batches by texture and use of lightmapping
	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		// texture is the base texture pointer, rsurface.texture is the
		// current frame/skin the texture is directing us to use (for example
		// if a model has 2 skins and it is on skin 1, then skin 0 tells us to
		// use skin 1 instead)
		texture = surfacelist[i]->texture;
		rsurface.texture = R_GetCurrentTexture(texture);
		rsurface.uselightmaptexture = surfacelist[i]->lightmaptexture != NULL && !depthonly && !prepass;
		if (!(rsurface.texture->currentmaterialflags & flagsmask) || (rsurface.texture->currentmaterialflags & MATERIALFLAG_NODRAW))
		{
			// if this texture is not the kind we want, skip ahead to the next one
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
			continue;
		}
		// simply scan ahead until we find a different texture or lightmap state
		for (;j < numsurfaces && texture == surfacelist[j]->texture && rsurface.uselightmaptexture == (surfacelist[j]->lightmaptexture != NULL);j++)
			;
		// render the range of surfaces
		R_ProcessModelTextureSurfaceList(j - i, surfacelist + i, writedepth, depthonly, ent, prepass);
	}
}

float locboxvertex3f[6*4*3] =
{
	1,0,1, 1,0,0, 1,1,0, 1,1,1,
	0,1,1, 0,1,0, 0,0,0, 0,0,1,
	1,1,1, 1,1,0, 0,1,0, 0,1,1,
	0,0,1, 0,0,0, 1,0,0, 1,0,1,
	0,0,1, 1,0,1, 1,1,1, 0,1,1,
	1,0,0, 0,0,0, 0,1,0, 1,1,0
};

unsigned short locboxelements[6*2*3] =
{
	 0, 1, 2, 0, 2, 3,
	 4, 5, 6, 4, 6, 7,
	 8, 9,10, 8,10,11,
	12,13,14, 12,14,15,
	16,17,18, 16,18,19,
	20,21,22, 20,22,23
};

void R_DrawLoc_Callback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i, j;
	cl_locnode_t *loc = (cl_locnode_t *)ent;
	vec3_t mins, size;
	float vertex3f[6*4*3];
	CHECKGLERROR
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	GL_DepthTest(true);
	GL_CullFace(GL_NONE);
	R_EntityMatrix(&identitymatrix);

	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);

	i = surfacelist[0];
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f) * r_refdef.view.colorscale,
			surfacelist[0] < 0 ? 0.5f : 0.125f);

	if (VectorCompare(loc->mins, loc->maxs))
	{
		VectorSet(size, 2, 2, 2);
		VectorMA(loc->mins, -0.5f, size, mins);
	}
	else
	{
		VectorCopy(loc->mins, mins);
		VectorSubtract(loc->maxs, loc->mins, size);
	}

	for (i = 0;i < 6*4*3;)
		for (j = 0;j < 3;j++, i++)
			vertex3f[i] = mins[j] + size[j] * locboxvertex3f[i];

	R_Mesh_Draw(0, 6*4, 0, 6*2, NULL, locboxelements, 0, 0);
}

void R_DrawLocs(void)
{
	int index;
	cl_locnode_t *loc, *nearestloc;
	vec3_t center;
	nearestloc = CL_Locs_FindNearest(cl.movement_origin);
	for (loc = cl.locnodes, index = 0;loc;loc = loc->next, index++)
	{
		VectorLerp(loc->mins, 0.5f, loc->maxs, center);
		R_MeshQueue_AddTransparent(center, R_DrawLoc_Callback, (entity_render_t *)loc, loc == nearestloc ? -1 : index, NULL);
	}
}

void R_DecalSystem_Reset(decalsystem_t *decalsystem)
{
	if (decalsystem->decals)
		Mem_Free(decalsystem->decals);
	memset(decalsystem, 0, sizeof(*decalsystem));
}

static void R_DecalSystem_SpawnTriangle(decalsystem_t *decalsystem, const float *v0, const float *v1, const float *v2, const float *t0, const float *t1, const float *t2, const float *c0, const float *c1, const float *c2, int triangleindex, int surfaceindex, int decalsequence)
{
	tridecal_t *decal;
	tridecal_t *decals;
	int i;

	// expand or initialize the system
	if (decalsystem->maxdecals <= decalsystem->numdecals)
	{
		decalsystem_t old = *decalsystem;
		qboolean useshortelements;
		decalsystem->maxdecals = max(16, decalsystem->maxdecals * 2);
		useshortelements = decalsystem->maxdecals * 3 <= 65536;
		decalsystem->decals = Mem_Alloc(cls.levelmempool, decalsystem->maxdecals * (sizeof(tridecal_t) + sizeof(float[3][3]) + sizeof(float[3][2]) + sizeof(float[3][4]) + sizeof(int[3]) + (useshortelements ? sizeof(unsigned short[3]) : 0)));
		decalsystem->color4f = (float *)(decalsystem->decals + decalsystem->maxdecals);
		decalsystem->texcoord2f = (float *)(decalsystem->color4f + decalsystem->maxdecals*12);
		decalsystem->vertex3f = (float *)(decalsystem->texcoord2f + decalsystem->maxdecals*6);
		decalsystem->element3i = (int *)(decalsystem->vertex3f + decalsystem->maxdecals*9);
		decalsystem->element3s = (useshortelements ? ((unsigned short *)(decalsystem->element3i + decalsystem->maxdecals*3)) : NULL);
		if (decalsystem->numdecals)
			memcpy(decalsystem->decals, old.decals, decalsystem->numdecals * sizeof(tridecal_t));
		if (old.decals)
			Mem_Free(old.decals);
		for (i = 0;i < decalsystem->maxdecals*3;i++)
			decalsystem->element3i[i] = i;
		if (useshortelements)
			for (i = 0;i < decalsystem->maxdecals*3;i++)
				decalsystem->element3s[i] = i;
	}

	// grab a decal and search for another free slot for the next one
	decals = decalsystem->decals;
	decal = decalsystem->decals + (i = decalsystem->freedecal++);
	for (i = decalsystem->freedecal;i < decalsystem->numdecals && decals[i].color4ub[0][3];i++)
		;
	decalsystem->freedecal = i;
	if (decalsystem->numdecals <= i)
		decalsystem->numdecals = i + 1;

	// initialize the decal
	decal->lived = 0;
	decal->triangleindex = triangleindex;
	decal->surfaceindex = surfaceindex;
	decal->decalsequence = decalsequence;
	decal->color4ub[0][0] = (unsigned char)(c0[0]*255.0f);
	decal->color4ub[0][1] = (unsigned char)(c0[1]*255.0f);
	decal->color4ub[0][2] = (unsigned char)(c0[2]*255.0f);
	decal->color4ub[0][3] = 255;
	decal->color4ub[1][0] = (unsigned char)(c1[0]*255.0f);
	decal->color4ub[1][1] = (unsigned char)(c1[1]*255.0f);
	decal->color4ub[1][2] = (unsigned char)(c1[2]*255.0f);
	decal->color4ub[1][3] = 255;
	decal->color4ub[2][0] = (unsigned char)(c2[0]*255.0f);
	decal->color4ub[2][1] = (unsigned char)(c2[1]*255.0f);
	decal->color4ub[2][2] = (unsigned char)(c2[2]*255.0f);
	decal->color4ub[2][3] = 255;
	decal->vertex3f[0][0] = v0[0];
	decal->vertex3f[0][1] = v0[1];
	decal->vertex3f[0][2] = v0[2];
	decal->vertex3f[1][0] = v1[0];
	decal->vertex3f[1][1] = v1[1];
	decal->vertex3f[1][2] = v1[2];
	decal->vertex3f[2][0] = v2[0];
	decal->vertex3f[2][1] = v2[1];
	decal->vertex3f[2][2] = v2[2];
	decal->texcoord2f[0][0] = t0[0];
	decal->texcoord2f[0][1] = t0[1];
	decal->texcoord2f[1][0] = t1[0];
	decal->texcoord2f[1][1] = t1[1];
	decal->texcoord2f[2][0] = t2[0];
	decal->texcoord2f[2][1] = t2[1];
}

extern cvar_t cl_decals_bias;
extern cvar_t cl_decals_models;
extern cvar_t cl_decals_newsystem_intensitymultiplier;
static void R_DecalSystem_SplatEntity(entity_render_t *ent, const vec3_t worldorigin, const vec3_t worldnormal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float worldsize, int decalsequence)
{
	matrix4x4_t projection;
	decalsystem_t *decalsystem;
	qboolean dynamic;
	dp_model_t *model;
	const float *vertex3f;
	const msurface_t *surface;
	const msurface_t *surfaces;
	const int *surfacelist;
	const texture_t *texture;
	int numtriangles;
	int numsurfacelist;
	int surfacelistindex;
	int surfaceindex;
	int triangleindex;
	int cornerindex;
	int index;
	int numpoints;
	const int *e;
	float localorigin[3];
	float localnormal[3];
	float localmins[3];
	float localmaxs[3];
	float localsize;
	float v[9][3];
	float tc[9][2];
	float c[9][4];
	//float normal[3];
	float planes[6][4];
	float f;
	float points[2][9][3];
	float angles[3];
	float temp[3];

	decalsystem = &ent->decalsystem;
	model = ent->model;
	if (!model || !ent->allowdecals || ent->alpha < 1 || (ent->flags & (RENDER_ADDITIVE | RENDER_NODEPTHTEST)))
	{
		R_DecalSystem_Reset(&ent->decalsystem);
		return;
	}

	if (!model->brush.data_nodes && !cl_decals_models.integer)
	{
		if (decalsystem->model)
			R_DecalSystem_Reset(decalsystem);
		return;
	}

	if (decalsystem->model != model)
		R_DecalSystem_Reset(decalsystem);
	decalsystem->model = model;

	RSurf_ActiveModelEntity(ent, false, false, false);

	Matrix4x4_Transform(&rsurface.inversematrix, worldorigin, localorigin);
	Matrix4x4_Transform3x3(&rsurface.inversematrix, worldnormal, localnormal);
	VectorNormalize(localnormal);
	localsize = worldsize*rsurface.inversematrixscale;
	localmins[0] = localorigin[0] - localsize;
	localmins[1] = localorigin[1] - localsize;
	localmins[2] = localorigin[2] - localsize;
	localmaxs[0] = localorigin[0] + localsize;
	localmaxs[1] = localorigin[1] + localsize;
	localmaxs[2] = localorigin[2] + localsize;

	//VectorCopy(localnormal, planes[4]);
	//VectorVectors(planes[4], planes[2], planes[0]);
	AnglesFromVectors(angles, localnormal, NULL, false);
	AngleVectors(angles, planes[0], planes[2], planes[4]);
	VectorNegate(planes[0], planes[1]);
	VectorNegate(planes[2], planes[3]);
	VectorNegate(planes[4], planes[5]);
	planes[0][3] = DotProduct(planes[0], localorigin) - localsize;
	planes[1][3] = DotProduct(planes[1], localorigin) - localsize;
	planes[2][3] = DotProduct(planes[2], localorigin) - localsize;
	planes[3][3] = DotProduct(planes[3], localorigin) - localsize;
	planes[4][3] = DotProduct(planes[4], localorigin) - localsize;
	planes[5][3] = DotProduct(planes[5], localorigin) - localsize;

#if 1
// works
{
	matrix4x4_t forwardprojection;
	Matrix4x4_CreateFromQuakeEntity(&forwardprojection, localorigin[0], localorigin[1], localorigin[2], angles[0], angles[1], angles[2], localsize);
	Matrix4x4_Invert_Simple(&projection, &forwardprojection);
}
#else
// broken
{
	float projectionvector[4][3];
	VectorScale(planes[0], ilocalsize, projectionvector[0]);
	VectorScale(planes[2], ilocalsize, projectionvector[1]);
	VectorScale(planes[4], ilocalsize, projectionvector[2]);
	projectionvector[0][0] = planes[0][0] * ilocalsize;
	projectionvector[0][1] = planes[1][0] * ilocalsize;
	projectionvector[0][2] = planes[2][0] * ilocalsize;
	projectionvector[1][0] = planes[0][1] * ilocalsize;
	projectionvector[1][1] = planes[1][1] * ilocalsize;
	projectionvector[1][2] = planes[2][1] * ilocalsize;
	projectionvector[2][0] = planes[0][2] * ilocalsize;
	projectionvector[2][1] = planes[1][2] * ilocalsize;
	projectionvector[2][2] = planes[2][2] * ilocalsize;
	projectionvector[3][0] = -(localorigin[0]*projectionvector[0][0]+localorigin[1]*projectionvector[1][0]+localorigin[2]*projectionvector[2][0]);
	projectionvector[3][1] = -(localorigin[0]*projectionvector[0][1]+localorigin[1]*projectionvector[1][1]+localorigin[2]*projectionvector[2][1]);
	projectionvector[3][2] = -(localorigin[0]*projectionvector[0][2]+localorigin[1]*projectionvector[1][2]+localorigin[2]*projectionvector[2][2]);
	Matrix4x4_FromVectors(&projection, projectionvector[0], projectionvector[1], projectionvector[2], projectionvector[3]);
}
#endif

	dynamic = model->surfmesh.isanimated;
	vertex3f = rsurface.modelvertex3f;
	numsurfacelist = model->nummodelsurfaces;
	surfacelist = model->sortedmodelsurfaces;
	surfaces = model->data_surfaces;
	for (surfacelistindex = 0;surfacelistindex < numsurfacelist;surfacelistindex++)
	{
		surfaceindex = surfacelist[surfacelistindex];
		surface = surfaces + surfaceindex;
		// check cull box first because it rejects more than any other check
		if (!dynamic && !BoxesOverlap(surface->mins, surface->maxs, localmins, localmaxs))
			continue;
		// skip transparent surfaces
		texture = surface->texture;
		if (texture->currentmaterialflags & (MATERIALFLAG_BLENDED | MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_SKY | MATERIALFLAG_SHORTDEPTHRANGE | MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
			continue;
		if (texture->surfaceflags & Q3SURFACEFLAG_NOMARKS)
			continue;
		numtriangles = surface->num_triangles;
		for (triangleindex = 0, e = model->surfmesh.data_element3i + 3*surface->num_firsttriangle;triangleindex < numtriangles;triangleindex++, e += 3)
		{
			for (cornerindex = 0;cornerindex < 3;cornerindex++)
			{
				index = 3*e[cornerindex];
				VectorCopy(vertex3f + index, v[cornerindex]);
			}
			// cull backfaces
			//TriangleNormal(v[0], v[1], v[2], normal);
			//if (DotProduct(normal, localnormal) < 0.0f)
			//	continue;
			// clip by each of the box planes formed from the projection matrix
			// if anything survives, we emit the decal
			numpoints = PolygonF_Clip(3        , v[0]        , planes[0][0], planes[0][1], planes[0][2], planes[0][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
			if (numpoints < 3)
				continue;
			numpoints = PolygonF_Clip(numpoints, points[1][0], planes[1][0], planes[1][1], planes[1][2], planes[1][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[0][0]);
			if (numpoints < 3)
				continue;
			numpoints = PolygonF_Clip(numpoints, points[0][0], planes[2][0], planes[2][1], planes[2][2], planes[2][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
			if (numpoints < 3)
				continue;
			numpoints = PolygonF_Clip(numpoints, points[1][0], planes[3][0], planes[3][1], planes[3][2], planes[3][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[0][0]);
			if (numpoints < 3)
				continue;
			numpoints = PolygonF_Clip(numpoints, points[0][0], planes[4][0], planes[4][1], planes[4][2], planes[4][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
			if (numpoints < 3)
				continue;
			numpoints = PolygonF_Clip(numpoints, points[1][0], planes[5][0], planes[5][1], planes[5][2], planes[5][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), v[0]);
			if (numpoints < 3)
				continue;
			// some part of the triangle survived, so we have to accept it...
			if (dynamic)
			{
				// dynamic always uses the original triangle
				numpoints = 3;
				for (cornerindex = 0;cornerindex < 3;cornerindex++)
				{
					index = 3*e[cornerindex];
					VectorCopy(vertex3f + index, v[cornerindex]);
				}
			}
			for (cornerindex = 0;cornerindex < numpoints;cornerindex++)
			{
				// convert vertex positions to texcoords
				Matrix4x4_Transform(&projection, v[cornerindex], temp);
				tc[cornerindex][0] = (temp[1]+1.0f)*0.5f * (s2-s1) + s1;
				tc[cornerindex][1] = (temp[2]+1.0f)*0.5f * (t2-t1) + t1;
				// calculate distance fade from the projection origin
				f = a * (1.0f-fabs(temp[0])) * cl_decals_newsystem_intensitymultiplier.value;
				f = bound(0.0f, f, 1.0f);
				c[cornerindex][0] = r * f;
				c[cornerindex][1] = g * f;
				c[cornerindex][2] = b * f;
				c[cornerindex][3] = 1.0f;
				//VectorMA(v[cornerindex], cl_decals_bias.value, localnormal, v[cornerindex]);
			}
			if (dynamic)
				R_DecalSystem_SpawnTriangle(decalsystem, v[0], v[1], v[2], tc[0], tc[1], tc[2], c[0], c[1], c[2], triangleindex+surface->num_firsttriangle, surfaceindex, decalsequence);
			else
				for (cornerindex = 0;cornerindex < numpoints-2;cornerindex++)
					R_DecalSystem_SpawnTriangle(decalsystem, v[0], v[cornerindex+1], v[cornerindex+2], tc[0], tc[cornerindex+1], tc[cornerindex+2], c[0], c[cornerindex+1], c[cornerindex+2], -1, surfaceindex, decalsequence);
		}
	}
}

// do not call this outside of rendering code - use R_DecalSystem_SplatEntities instead
static void R_DecalSystem_ApplySplatEntities(const vec3_t worldorigin, const vec3_t worldnormal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float worldsize, int decalsequence)
{
	int renderentityindex;
	float worldmins[3];
	float worldmaxs[3];
	entity_render_t *ent;

	if (!cl_decals_newsystem.integer)
		return;

	worldmins[0] = worldorigin[0] - worldsize;
	worldmins[1] = worldorigin[1] - worldsize;
	worldmins[2] = worldorigin[2] - worldsize;
	worldmaxs[0] = worldorigin[0] + worldsize;
	worldmaxs[1] = worldorigin[1] + worldsize;
	worldmaxs[2] = worldorigin[2] + worldsize;

	R_DecalSystem_SplatEntity(r_refdef.scene.worldentity, worldorigin, worldnormal, r, g, b, a, s1, t1, s2, t2, worldsize, decalsequence);

	for (renderentityindex = 0;renderentityindex < r_refdef.scene.numentities;renderentityindex++)
	{
		ent = r_refdef.scene.entities[renderentityindex];
		if (!BoxesOverlap(ent->mins, ent->maxs, worldmins, worldmaxs))
			continue;

		R_DecalSystem_SplatEntity(ent, worldorigin, worldnormal, r, g, b, a, s1, t1, s2, t2, worldsize, decalsequence);
	}
}

typedef struct r_decalsystem_splatqueue_s
{
	vec3_t worldorigin;
	vec3_t worldnormal;
	float color[4];
	float tcrange[4];
	float worldsize;
	int decalsequence;
}
r_decalsystem_splatqueue_t;

int r_decalsystem_numqueued = 0;
r_decalsystem_splatqueue_t r_decalsystem_queue[MAX_DECALSYSTEM_QUEUE];

void R_DecalSystem_SplatEntities(const vec3_t worldorigin, const vec3_t worldnormal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float worldsize)
{
	r_decalsystem_splatqueue_t *queue;

	if (!cl_decals_newsystem.integer || r_decalsystem_numqueued == MAX_DECALSYSTEM_QUEUE)
		return;

	queue = &r_decalsystem_queue[r_decalsystem_numqueued++];
	VectorCopy(worldorigin, queue->worldorigin);
	VectorCopy(worldnormal, queue->worldnormal);
	Vector4Set(queue->color, r, g, b, a);
	Vector4Set(queue->tcrange, s1, t1, s2, t2);
	queue->worldsize = worldsize;
	queue->decalsequence = cl.decalsequence++;
}

static void R_DecalSystem_ApplySplatEntitiesQueue(void)
{
	int i;
	r_decalsystem_splatqueue_t *queue;

	for (i = 0, queue = r_decalsystem_queue;i < r_decalsystem_numqueued;i++, queue++)
		R_DecalSystem_ApplySplatEntities(queue->worldorigin, queue->worldnormal, queue->color[0], queue->color[1], queue->color[2], queue->color[3], queue->tcrange[0], queue->tcrange[1], queue->tcrange[2], queue->tcrange[3], queue->worldsize, queue->decalsequence);
	r_decalsystem_numqueued = 0;
}

extern cvar_t cl_decals_max;
static void R_DrawModelDecals_FadeEntity(entity_render_t *ent)
{
	int i;
	decalsystem_t *decalsystem = &ent->decalsystem;
	int numdecals;
	int killsequence;
	tridecal_t *decal;
	float frametime;
	float lifetime;

	if (!decalsystem->numdecals)
		return;

	if (r_showsurfaces.integer)
		return;

	if (ent->model != decalsystem->model || ent->alpha < 1 || (ent->flags & RENDER_ADDITIVE))
	{
		R_DecalSystem_Reset(decalsystem);
		return;
	}

	killsequence = cl.decalsequence - max(1, cl_decals_max.integer);
	lifetime = cl_decals_time.value + cl_decals_fadetime.value;

	if (decalsystem->lastupdatetime)
		frametime = (cl.time - decalsystem->lastupdatetime);
	else
		frametime = 0;
	decalsystem->lastupdatetime = cl.time;
	decal = decalsystem->decals;
	numdecals = decalsystem->numdecals;

	for (i = 0, decal = decalsystem->decals;i < numdecals;i++, decal++)
	{
		if (decal->color4ub[0][3])
		{
			decal->lived += frametime;
			if (killsequence - decal->decalsequence > 0 || decal->lived >= lifetime)
			{
				memset(decal, 0, sizeof(*decal));
				if (decalsystem->freedecal > i)
					decalsystem->freedecal = i;
			}
		}
	}
	decal = decalsystem->decals;
	while (numdecals > 0 && !decal[numdecals-1].color4ub[0][3])
		numdecals--;

	// collapse the array by shuffling the tail decals into the gaps
	for (;;)
	{
		while (decalsystem->freedecal < numdecals && decal[decalsystem->freedecal].color4ub[0][3])
			decalsystem->freedecal++;
		if (decalsystem->freedecal == numdecals)
			break;
		decal[decalsystem->freedecal] = decal[--numdecals];
	}

	decalsystem->numdecals = numdecals;

	if (numdecals <= 0)
	{
		// if there are no decals left, reset decalsystem
		R_DecalSystem_Reset(decalsystem);
	}
}

extern skinframe_t *decalskinframe;
static void R_DrawModelDecals_Entity(entity_render_t *ent)
{
	int i;
	decalsystem_t *decalsystem = &ent->decalsystem;
	int numdecals;
	tridecal_t *decal;
	float faderate;
	float alpha;
	float *v3f;
	float *c4f;
	float *t2f;
	const int *e;
	const unsigned char *surfacevisible = ent == r_refdef.scene.worldentity ? r_refdef.viewcache.world_surfacevisible : NULL;
	int numtris = 0;

	numdecals = decalsystem->numdecals;
	if (!numdecals)
		return;

	if (r_showsurfaces.integer)
		return;

	if (ent->model != decalsystem->model || ent->alpha < 1 || (ent->flags & RENDER_ADDITIVE))
	{
		R_DecalSystem_Reset(decalsystem);
		return;
	}

	// if the model is static it doesn't matter what value we give for
	// wantnormals and wanttangents, so this logic uses only rules applicable
	// to a model, knowing that they are meaningless otherwise
	if (ent == r_refdef.scene.worldentity)
		RSurf_ActiveWorldEntity();
	else
		RSurf_ActiveModelEntity(ent, false, false, false);

	decalsystem->lastupdatetime = cl.time;
	decal = decalsystem->decals;

	faderate = 1.0f / max(0.001f, cl_decals_fadetime.value);

	// update vertex positions for animated models
	v3f = decalsystem->vertex3f;
	c4f = decalsystem->color4f;
	t2f = decalsystem->texcoord2f;
	for (i = 0, decal = decalsystem->decals;i < numdecals;i++, decal++)
	{
		if (!decal->color4ub[0][3])
			continue;

		if (surfacevisible && !surfacevisible[decal->surfaceindex])
			continue;

		// update color values for fading decals
		if (decal->lived >= cl_decals_time.value)
		{
			alpha = 1 - faderate * (decal->lived - cl_decals_time.value);
			alpha *= (1.0f/255.0f);
		}
		else
			alpha = 1.0f/255.0f;

		c4f[ 0] = decal->color4ub[0][0] * alpha;
		c4f[ 1] = decal->color4ub[0][1] * alpha;
		c4f[ 2] = decal->color4ub[0][2] * alpha;
		c4f[ 3] = 1;
		c4f[ 4] = decal->color4ub[1][0] * alpha;
		c4f[ 5] = decal->color4ub[1][1] * alpha;
		c4f[ 6] = decal->color4ub[1][2] * alpha;
		c4f[ 7] = 1;
		c4f[ 8] = decal->color4ub[2][0] * alpha;
		c4f[ 9] = decal->color4ub[2][1] * alpha;
		c4f[10] = decal->color4ub[2][2] * alpha;
		c4f[11] = 1;

		t2f[0] = decal->texcoord2f[0][0];
		t2f[1] = decal->texcoord2f[0][1];
		t2f[2] = decal->texcoord2f[1][0];
		t2f[3] = decal->texcoord2f[1][1];
		t2f[4] = decal->texcoord2f[2][0];
		t2f[5] = decal->texcoord2f[2][1];

		// update vertex positions for animated models
		if (decal->triangleindex >= 0 && decal->triangleindex < rsurface.modelnum_triangles)
		{
			e = rsurface.modelelement3i + 3*decal->triangleindex;
			VectorCopy(rsurface.vertex3f + 3*e[0], v3f);
			VectorCopy(rsurface.vertex3f + 3*e[1], v3f + 3);
			VectorCopy(rsurface.vertex3f + 3*e[2], v3f + 6);
		}
		else
		{
			VectorCopy(decal->vertex3f[0], v3f);
			VectorCopy(decal->vertex3f[1], v3f + 3);
			VectorCopy(decal->vertex3f[2], v3f + 6);
		}

		if (r_refdef.fogenabled)
		{
			alpha = RSurf_FogVertex(v3f);
			VectorScale(c4f, alpha, c4f);
			alpha = RSurf_FogVertex(v3f + 3);
			VectorScale(c4f + 4, alpha, c4f + 4);
			alpha = RSurf_FogVertex(v3f + 6);
			VectorScale(c4f + 8, alpha, c4f + 8);
		}

		v3f += 9;
		c4f += 12;
		t2f += 6;
		numtris++;
	}

	if (numtris > 0)
	{
		r_refdef.stats.drawndecals += numtris;

		// now render the decals all at once
		// (this assumes they all use one particle font texture!)
		RSurf_ActiveCustomEntity(&rsurface.matrix, &rsurface.inversematrix, rsurface.ent_flags, rsurface.ent_shadertime, 1, 1, 1, 1, numdecals*3, decalsystem->vertex3f, decalsystem->texcoord2f, NULL, NULL, NULL, decalsystem->color4f, numtris, decalsystem->element3i, decalsystem->element3s, false, false);
		R_Mesh_ResetTextureState();
		R_Mesh_VertexPointer(decalsystem->vertex3f, 0, 0);
		R_Mesh_TexCoordPointer(0, 2, decalsystem->texcoord2f, 0, 0);
		R_Mesh_ColorPointer(decalsystem->color4f, 0, 0);
		GL_DepthMask(false);
		GL_DepthRange(0, 1);
		GL_PolygonOffset(rsurface.basepolygonfactor + r_polygonoffset_decals_factor.value, rsurface.basepolygonoffset + r_polygonoffset_decals_offset.value);
		GL_DepthTest(true);
		GL_CullFace(GL_NONE);
		GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		R_SetupShader_Generic(decalskinframe->base, NULL, GL_MODULATE, 1);
		R_Mesh_Draw(0, numtris * 3, 0, numtris, decalsystem->element3i, decalsystem->element3s, 0, 0);
	}
}

static void R_DrawModelDecals(void)
{
	int i, numdecals;

	// fade faster when there are too many decals
	numdecals = r_refdef.scene.worldentity->decalsystem.numdecals;
	for (i = 0;i < r_refdef.scene.numentities;i++)
		numdecals += r_refdef.scene.entities[i]->decalsystem.numdecals;

	R_DrawModelDecals_FadeEntity(r_refdef.scene.worldentity);
	for (i = 0;i < r_refdef.scene.numentities;i++)
		if (r_refdef.scene.entities[i]->decalsystem.numdecals)
			R_DrawModelDecals_FadeEntity(r_refdef.scene.entities[i]);

	R_DecalSystem_ApplySplatEntitiesQueue();

	numdecals = r_refdef.scene.worldentity->decalsystem.numdecals;
	for (i = 0;i < r_refdef.scene.numentities;i++)
		numdecals += r_refdef.scene.entities[i]->decalsystem.numdecals;

	r_refdef.stats.totaldecals += numdecals;

	if (r_showsurfaces.integer)
		return;

	R_DrawModelDecals_Entity(r_refdef.scene.worldentity);

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		if (r_refdef.scene.entities[i]->decalsystem.numdecals)
			R_DrawModelDecals_Entity(r_refdef.scene.entities[i]);
	}
}

extern cvar_t mod_collision_bih;
void R_DrawDebugModel(void)
{
	entity_render_t *ent = rsurface.entity;
	int i, j, k, l, flagsmask;
	const msurface_t *surface;
	dp_model_t *model = ent->model;
	vec3_t v;

	flagsmask = MATERIALFLAG_SKY | MATERIALFLAG_WALL;

	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	GL_DepthRange(0, 1);
	GL_DepthTest(!r_showdisabledepthtest.integer);
	GL_DepthMask(false);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (r_showcollisionbrushes.value > 0 && model->collision_bih.numleafs)
	{
		int triangleindex;
		int bihleafindex;
		qboolean cullbox = ent == r_refdef.scene.worldentity;
		const q3mbrush_t *brush;
		const bih_t *bih = &model->collision_bih;
		const bih_leaf_t *bihleaf;
		float vertex3f[3][3];
		GL_PolygonOffset(r_refdef.polygonfactor + r_showcollisionbrushes_polygonfactor.value, r_refdef.polygonoffset + r_showcollisionbrushes_polygonoffset.value);
		cullbox = false;
		for (bihleafindex = 0, bihleaf = bih->leafs;bihleafindex < bih->numleafs;bihleafindex++, bihleaf++)
		{
			if (cullbox && R_CullBox(bihleaf->mins, bihleaf->maxs))
				continue;
			switch (bihleaf->type)
			{
			case BIH_BRUSH:
				brush = model->brush.data_brushes + bihleaf->itemindex;
				if (brush->colbrushf && brush->colbrushf->numtriangles)
				{
					R_Mesh_VertexPointer(brush->colbrushf->points->v, 0, 0);
					GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
					R_Mesh_Draw(0, brush->colbrushf->numpoints, 0, brush->colbrushf->numtriangles, brush->colbrushf->elements, NULL, 0, 0);
				}
				break;
			case BIH_COLLISIONTRIANGLE:
				triangleindex = bihleaf->itemindex;
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+0], vertex3f[0]);
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+1], vertex3f[1]);
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+2], vertex3f[2]);
				R_Mesh_VertexPointer(vertex3f[0], 0, 0);
				GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, polygonelement3s, 0, 0);
				break;
			case BIH_RENDERTRIANGLE:
				triangleindex = bihleaf->itemindex;
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+0], vertex3f[0]);
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+1], vertex3f[1]);
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+2], vertex3f[2]);
				R_Mesh_VertexPointer(vertex3f[0], 0, 0);
				GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, polygonelement3s, 0, 0);
				break;
			}
		}
	}

	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);

	if (r_showtris.integer || r_shownormals.integer)
	{
		if (r_showdisabledepthtest.integer)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
		}
		else
		{
			GL_BlendFunc(GL_ONE, GL_ZERO);
			GL_DepthMask(true);
		}
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (ent == r_refdef.scene.worldentity && !r_refdef.viewcache.world_surfacevisible[j])
				continue;
			rsurface.texture = R_GetCurrentTexture(surface->texture);
			if ((rsurface.texture->currentmaterialflags & flagsmask) && surface->num_triangles)
			{
				RSurf_PrepareVerticesForBatch(true, true, 1, &surface);
				if (r_showtris.value > 0)
				{
					if (!rsurface.texture->currentlayers->depthmask)
						GL_Color(r_refdef.view.colorscale, 0, 0, r_showtris.value);
					else if (ent == r_refdef.scene.worldentity)
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, r_showtris.value);
					else
						GL_Color(0, r_refdef.view.colorscale, 0, r_showtris.value);
					R_Mesh_VertexPointer(rsurface.vertex3f, 0, 0);
					R_Mesh_ColorPointer(NULL, 0, 0);
					R_Mesh_TexCoordPointer(0, 0, NULL, 0, 0);
					qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					//R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, model->surfmesh.data_element3i, NULL, 0, 0);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_firsttriangle, surface->num_triangles, rsurface.modelelement3i, rsurface.modelelement3s, rsurface.modelelement3i_bufferobject, rsurface.modelelement3s_bufferobject);
					qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					CHECKGLERROR
				}
				if (r_shownormals.value < 0)
				{
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface.vertex3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 0, 0, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, -r_shownormals.value, rsurface.svector3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 1, 1, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					CHECKGLERROR
				}
				if (r_shownormals.value > 0)
				{
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface.vertex3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 0, 0, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.svector3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 1, 1, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					CHECKGLERROR
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface.vertex3f + l * 3, v);
						GL_Color(0, r_refdef.view.colorscale, 0, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.tvector3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 1, 1, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					CHECKGLERROR
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface.vertex3f + l * 3, v);
						GL_Color(0, 0, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.normal3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 1, 1, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					CHECKGLERROR
				}
			}
		}
		rsurface.texture = NULL;
	}
}

extern void R_BuildLightMap(const entity_render_t *ent, msurface_t *surface);
int r_maxsurfacelist = 0;
const msurface_t **r_surfacelist = NULL;
void R_DrawWorldSurfaces(qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean debug, qboolean prepass)
{
	int i, j, endj, flagsmask;
	dp_model_t *model = r_refdef.scene.worldmodel;
	msurface_t *surfaces;
	unsigned char *update;
	int numsurfacelist = 0;
	if (model == NULL)
		return;

	if (r_maxsurfacelist < model->num_surfaces)
	{
		r_maxsurfacelist = model->num_surfaces;
		if (r_surfacelist)
			Mem_Free((msurface_t**)r_surfacelist);
		r_surfacelist = (const msurface_t **) Mem_Alloc(r_main_mempool, r_maxsurfacelist * sizeof(*r_surfacelist));
	}

	RSurf_ActiveWorldEntity();

	surfaces = model->data_surfaces;
	update = model->brushq1.lightmapupdateflags;

	// update light styles on this submodel
	if (!skysurfaces && !depthonly && !prepass && model->brushq1.num_lightstyles && r_refdef.lightmapintensity > 0)
	{
		model_brush_lightstyleinfo_t *style;
		for (i = 0, style = model->brushq1.data_lightstyleinfo;i < model->brushq1.num_lightstyles;i++, style++)
		{
			if (style->value != r_refdef.scene.lightstylevalue[style->style])
			{
				int *list = style->surfacelist;
				style->value = r_refdef.scene.lightstylevalue[style->style];
				for (j = 0;j < style->numsurfaces;j++)
					update[list[j]] = true;
			}
		}
	}

	flagsmask = skysurfaces ? MATERIALFLAG_SKY : MATERIALFLAG_WALL;

	if (debug)
	{
		R_DrawDebugModel();
		rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
		return;
	}

	rsurface.uselightmaptexture = false;
	rsurface.texture = NULL;
	rsurface.rtlight = NULL;
	numsurfacelist = 0;
	// add visible surfaces to draw list
	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		j = model->sortedmodelsurfaces[i];
		if (r_refdef.viewcache.world_surfacevisible[j])
			r_surfacelist[numsurfacelist++] = surfaces + j;
	}
	// update lightmaps if needed
	if (model->brushq1.firstrender)
	{
		model->brushq1.firstrender = false;
		for (j = model->firstmodelsurface, endj = model->firstmodelsurface + model->nummodelsurfaces;j < endj;j++)
			if (update[j])
				R_BuildLightMap(r_refdef.scene.worldentity, surfaces + j);
	}
	else if (update)
	{
		for (j = model->firstmodelsurface, endj = model->firstmodelsurface + model->nummodelsurfaces;j < endj;j++)
			if (r_refdef.viewcache.world_surfacevisible[j])
				if (update[j])
					R_BuildLightMap(r_refdef.scene.worldentity, surfaces + j);
	}
	// don't do anything if there were no surfaces
	if (!numsurfacelist)
	{
		rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
		return;
	}
	R_QueueWorldSurfaceList(numsurfacelist, r_surfacelist, flagsmask, writedepth, depthonly, prepass);
	GL_AlphaTest(false);

	// add to stats if desired
	if (r_speeds.integer && !skysurfaces && !depthonly)
	{
		r_refdef.stats.world_surfaces += numsurfacelist;
		for (j = 0;j < numsurfacelist;j++)
			r_refdef.stats.world_triangles += r_surfacelist[j]->num_triangles;
	}

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_DrawModelSurfaces(entity_render_t *ent, qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean debug, qboolean prepass)
{
	int i, j, endj, flagsmask;
	dp_model_t *model = ent->model;
	msurface_t *surfaces;
	unsigned char *update;
	int numsurfacelist = 0;
	if (model == NULL)
		return;

	if (r_maxsurfacelist < model->num_surfaces)
	{
		r_maxsurfacelist = model->num_surfaces;
		if (r_surfacelist)
			Mem_Free((msurface_t **)r_surfacelist);
		r_surfacelist = (const msurface_t **) Mem_Alloc(r_main_mempool, r_maxsurfacelist * sizeof(*r_surfacelist));
	}

	// if the model is static it doesn't matter what value we give for
	// wantnormals and wanttangents, so this logic uses only rules applicable
	// to a model, knowing that they are meaningless otherwise
	if (ent == r_refdef.scene.worldentity)
		RSurf_ActiveWorldEntity();
	else if (r_showsurfaces.integer && r_showsurfaces.integer != 3)
		RSurf_ActiveModelEntity(ent, false, false, false);
	else if (prepass)
		RSurf_ActiveModelEntity(ent, true, true, true);
	else if (depthonly)
	{
		switch (vid.renderpath)
		{
		case RENDERPATH_GL20:
		case RENDERPATH_CGGL:
			RSurf_ActiveModelEntity(ent, model->wantnormals, model->wanttangents, false);
			break;
		case RENDERPATH_GL13:
		case RENDERPATH_GL11:
			RSurf_ActiveModelEntity(ent, model->wantnormals, false, false);
			break;
		}
	}
	else
	{
		switch (vid.renderpath)
		{
		case RENDERPATH_GL20:
		case RENDERPATH_CGGL:
			RSurf_ActiveModelEntity(ent, true, true, false);
			break;
		case RENDERPATH_GL13:
		case RENDERPATH_GL11:
			RSurf_ActiveModelEntity(ent, true, false, false);
			break;
		}
	}

	surfaces = model->data_surfaces;
	update = model->brushq1.lightmapupdateflags;

	// update light styles
	if (!skysurfaces && !depthonly && !prepass && model->brushq1.num_lightstyles && r_refdef.lightmapintensity > 0)
	{
		model_brush_lightstyleinfo_t *style;
		for (i = 0, style = model->brushq1.data_lightstyleinfo;i < model->brushq1.num_lightstyles;i++, style++)
		{
			if (style->value != r_refdef.scene.lightstylevalue[style->style])
			{
				int *list = style->surfacelist;
				style->value = r_refdef.scene.lightstylevalue[style->style];
				for (j = 0;j < style->numsurfaces;j++)
					update[list[j]] = true;
			}
		}
	}

	flagsmask = skysurfaces ? MATERIALFLAG_SKY : MATERIALFLAG_WALL;

	if (debug)
	{
		R_DrawDebugModel();
		rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
		return;
	}

	rsurface.uselightmaptexture = false;
	rsurface.texture = NULL;
	rsurface.rtlight = NULL;
	numsurfacelist = 0;
	// add visible surfaces to draw list
	for (i = 0;i < model->nummodelsurfaces;i++)
		r_surfacelist[numsurfacelist++] = surfaces + model->sortedmodelsurfaces[i];
	// don't do anything if there were no surfaces
	if (!numsurfacelist)
	{
		rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
		return;
	}
	// update lightmaps if needed
	if (update)
	{
		int updated = 0;
		for (j = model->firstmodelsurface, endj = model->firstmodelsurface + model->nummodelsurfaces;j < endj;j++)
		{
			if (update[j])
			{
				updated++;
				R_BuildLightMap(ent, surfaces + j);
			}
		}
	}
	if (update)
		for (j = model->firstmodelsurface, endj = model->firstmodelsurface + model->nummodelsurfaces;j < endj;j++)
			if (update[j])
				R_BuildLightMap(ent, surfaces + j);
	R_QueueModelSurfaceList(ent, numsurfacelist, r_surfacelist, flagsmask, writedepth, depthonly, prepass);
	GL_AlphaTest(false);

	// add to stats if desired
	if (r_speeds.integer && !skysurfaces && !depthonly)
	{
		r_refdef.stats.entities_surfaces += numsurfacelist;
		for (j = 0;j < numsurfacelist;j++)
			r_refdef.stats.entities_triangles += r_surfacelist[j]->num_triangles;
	}

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_DrawCustomSurface(skinframe_t *skinframe, const matrix4x4_t *texmatrix, int materialflags, int firstvertex, int numvertices, int firsttriangle, int numtriangles, qboolean writedepth, qboolean prepass)
{
	static texture_t texture;
	static msurface_t surface;
	const msurface_t *surfacelist = &surface;

	// fake enough texture and surface state to render this geometry

	texture.update_lastrenderframe = -1; // regenerate this texture
	texture.basematerialflags = materialflags | MATERIALFLAG_CUSTOMSURFACE | MATERIALFLAG_WALL;
	texture.currentskinframe = skinframe;
	texture.currenttexmatrix = *texmatrix; // requires MATERIALFLAG_CUSTOMSURFACE
	texture.specularscalemod = 1;
	texture.specularpowermod = 1;

	surface.texture = &texture;
	surface.num_triangles = numtriangles;
	surface.num_firsttriangle = firsttriangle;
	surface.num_vertices = numvertices;
	surface.num_firstvertex = firstvertex;

	// now render it
	rsurface.texture = R_GetCurrentTexture(surface.texture);
	rsurface.uselightmaptexture = false;
	R_DrawModelTextureSurfaceList(1, &surfacelist, writedepth, prepass);
}

void R_DrawCustomSurface_Texture(texture_t *texture, const matrix4x4_t *texmatrix, int materialflags, int firstvertex, int numvertices, int firsttriangle, int numtriangles, qboolean writedepth, qboolean prepass)
{
	static msurface_t surface;
	const msurface_t *surfacelist = &surface;

	// fake enough texture and surface state to render this geometry

	surface.texture = texture;
	surface.num_triangles = numtriangles;
	surface.num_firsttriangle = firsttriangle;
	surface.num_vertices = numvertices;
	surface.num_firstvertex = firstvertex;

	// now render it
	rsurface.texture = R_GetCurrentTexture(surface.texture);
	rsurface.uselightmaptexture = false;
	R_DrawModelTextureSurfaceList(1, &surfacelist, writedepth, prepass);
}
