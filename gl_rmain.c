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
#include "csprogs.h"
#include "cl_video.h"
#include "dpsoftrast.h"

#ifdef SUPPORTD3D
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 vid_d3d9dev;
#endif

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
cvar_t r_deformvertexes = {0, "r_deformvertexes", "1", "allows use of deformvertexes in shader files (can be turned off to check performance impact)"};
cvar_t r_transparent = {0, "r_transparent", "1", "allows use of transparent surfaces (can be turned off to check performance impact)"};
cvar_t r_transparent_alphatocoverage = {0, "r_transparent_alphatocoverage", "1", "enables GL_ALPHA_TO_COVERAGE antialiasing technique on alphablend and alphatest surfaces when using vid_samples 2 or higher"};
cvar_t r_showoverdraw = {0, "r_showoverdraw", "0", "shows overlapping geometry"};
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
cvar_t r_draw2d = {0, "r_draw2d","1", "draw 2D stuff (dangerous to turn off)"};
cvar_t r_drawworld = {0, "r_drawworld","1", "draw world (most static stuff)"};
cvar_t r_drawviewmodel = {0, "r_drawviewmodel","1", "draw your weapon model"};
cvar_t r_drawexteriormodel = {0, "r_drawexteriormodel","1", "draw your player model (e.g. in chase cam, reflections)"};
cvar_t r_cullentities_trace = {0, "r_cullentities_trace", "1", "probabistically cull invisible entities"};
cvar_t r_cullentities_trace_samples = {0, "r_cullentities_trace_samples", "2", "number of samples to test for entity culling (in addition to center sample)"};
cvar_t r_cullentities_trace_tempentitysamples = {0, "r_cullentities_trace_tempentitysamples", "-1", "number of samples to test for entity culling of temp entities (including all CSQC entities), -1 disables trace culling on these entities to prevent flicker (pvs still applies)"};
cvar_t r_cullentities_trace_enlarge = {0, "r_cullentities_trace_enlarge", "0", "box enlargement for entity culling"};
cvar_t r_cullentities_trace_delay = {0, "r_cullentities_trace_delay", "1", "number of seconds until the entity gets actually culled"};
cvar_t r_speeds = {0, "r_speeds","0", "displays rendering statistics and per-subsystem timings"};
cvar_t r_fullbright = {0, "r_fullbright","0", "makes map very bright and renders faster"};

cvar_t r_fakelight = {0, "r_fakelight","0", "render 'fake' lighting instead of real lightmaps"};
cvar_t r_fakelight_intensity = {0, "r_fakelight_intensity","0.75", "fakelight intensity modifier"};
#define FAKELIGHT_ENABLED (r_fakelight.integer >= 2 || (r_fakelight.integer && r_refdef.scene.worldmodel && !r_refdef.scene.worldmodel->lit))

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
cvar_t r_fog_clear = {0, "r_fog_clear", "1", "clears renderbuffer with fog color before render starts"};
cvar_t r_drawfog = {CVAR_SAVE, "r_drawfog", "1", "allows one to disable fog rendering"};
cvar_t r_transparentdepthmasking = {CVAR_SAVE, "r_transparentdepthmasking", "0", "enables depth writes on transparent meshes whose materially is normally opaque, this prevents seeing the inside of a transparent mesh"};
cvar_t r_transparent_sortmaxdist = {CVAR_SAVE, "r_transparent_sortmaxdist", "32768", "upper distance limit for transparent sorting"};
cvar_t r_transparent_sortarraysize = {CVAR_SAVE, "r_transparent_sortarraysize", "4096", "number of distance-sorting layers"};

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

cvar_t r_textureunits = {0, "r_textureunits", "32", "number of texture units to use in GL 1.1 and GL 1.3 rendering paths"};
static cvar_t gl_combine = {CVAR_READONLY, "gl_combine", "1", "indicates whether the OpenGL 1.3 rendering path is active"};
static cvar_t r_glsl = {CVAR_READONLY, "r_glsl", "1", "indicates whether the OpenGL 2.0 rendering path is active"};

cvar_t r_viewfbo = {CVAR_SAVE, "r_viewfbo", "0", "enables use of an 8bit (1) or 16bit (2) or 32bit (3) per component float framebuffer render, which may be at a different resolution than the video mode"};
cvar_t r_viewscale = {CVAR_SAVE, "r_viewscale", "1", "scaling factor for resolution of the fbo rendering method, must be > 0, can be above 1 for a costly antialiasing behavior, typical values are 0.5 for 1/4th as many pixels rendered, or 1 for normal rendering"};
cvar_t r_viewscale_fpsscaling = {CVAR_SAVE, "r_viewscale_fpsscaling", "0", "change resolution based on framerate"};
cvar_t r_viewscale_fpsscaling_min = {CVAR_SAVE, "r_viewscale_fpsscaling_min", "0.0625", "worst acceptable quality"};
cvar_t r_viewscale_fpsscaling_multiply = {CVAR_SAVE, "r_viewscale_fpsscaling_multiply", "5", "adjust quality up or down by the frametime difference from 1.0/target, multiplied by this factor"};
cvar_t r_viewscale_fpsscaling_stepsize = {CVAR_SAVE, "r_viewscale_fpsscaling_stepsize", "0.01", "smallest adjustment to hit the target framerate (this value prevents minute oscillations)"};
cvar_t r_viewscale_fpsscaling_stepmax = {CVAR_SAVE, "r_viewscale_fpsscaling_stepmax", "1.00", "largest adjustment to hit the target framerate (this value prevents wild overshooting of the estimate)"};
cvar_t r_viewscale_fpsscaling_target = {CVAR_SAVE, "r_viewscale_fpsscaling_target", "70", "desired framerate"};

cvar_t r_glsl_deluxemapping = {CVAR_SAVE, "r_glsl_deluxemapping", "1", "use per pixel lighting on deluxemap-compiled q3bsp maps (or a value of 2 forces deluxemap shading even without deluxemaps)"};
cvar_t r_glsl_offsetmapping = {CVAR_SAVE, "r_glsl_offsetmapping", "0", "offset mapping effect (also known as parallax mapping or virtual displacement mapping)"};
cvar_t r_glsl_offsetmapping_steps = {CVAR_SAVE, "r_glsl_offsetmapping_steps", "2", "offset mapping steps (note: too high values may be not supported by your GPU)"};
cvar_t r_glsl_offsetmapping_reliefmapping = {CVAR_SAVE, "r_glsl_offsetmapping_reliefmapping", "0", "relief mapping effect (higher quality)"};
cvar_t r_glsl_offsetmapping_reliefmapping_steps = {CVAR_SAVE, "r_glsl_offsetmapping_reliefmapping_steps", "10", "relief mapping steps (note: too high values may be not supported by your GPU)"};
cvar_t r_glsl_offsetmapping_reliefmapping_refinesteps = {CVAR_SAVE, "r_glsl_offsetmapping_reliefmapping_refinesteps", "5", "relief mapping refine steps (these are a binary search executed as the last step as given by r_glsl_offsetmapping_reliefmapping_steps)"};
cvar_t r_glsl_offsetmapping_scale = {CVAR_SAVE, "r_glsl_offsetmapping_scale", "0.04", "how deep the offset mapping effect is"};
cvar_t r_glsl_postprocess = {CVAR_SAVE, "r_glsl_postprocess", "0", "use a GLSL postprocessing shader"};
cvar_t r_glsl_postprocess_uservec1 = {CVAR_SAVE, "r_glsl_postprocess_uservec1", "0 0 0 0", "a 4-component vector to pass as uservec1 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec2 = {CVAR_SAVE, "r_glsl_postprocess_uservec2", "0 0 0 0", "a 4-component vector to pass as uservec2 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec3 = {CVAR_SAVE, "r_glsl_postprocess_uservec3", "0 0 0 0", "a 4-component vector to pass as uservec3 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec4 = {CVAR_SAVE, "r_glsl_postprocess_uservec4", "0 0 0 0", "a 4-component vector to pass as uservec4 to the postprocessing shader (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec1_enable = {CVAR_SAVE, "r_glsl_postprocess_uservec1_enable", "1", "enables postprocessing uservec1 usage, creates USERVEC1 define (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec2_enable = {CVAR_SAVE, "r_glsl_postprocess_uservec2_enable", "1", "enables postprocessing uservec2 usage, creates USERVEC1 define (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec3_enable = {CVAR_SAVE, "r_glsl_postprocess_uservec3_enable", "1", "enables postprocessing uservec3 usage, creates USERVEC1 define (only useful if default.glsl has been customized)"};
cvar_t r_glsl_postprocess_uservec4_enable = {CVAR_SAVE, "r_glsl_postprocess_uservec4_enable", "1", "enables postprocessing uservec4 usage, creates USERVEC1 define (only useful if default.glsl has been customized)"};

cvar_t r_water = {CVAR_SAVE, "r_water", "0", "whether to use reflections and refraction on water surfaces (note: r_wateralpha must be set below 1)"};
cvar_t r_water_clippingplanebias = {CVAR_SAVE, "r_water_clippingplanebias", "1", "a rather technical setting which avoids black pixels around water edges"};
cvar_t r_water_resolutionmultiplier = {CVAR_SAVE, "r_water_resolutionmultiplier", "0.5", "multiplier for screen resolution when rendering refracted/reflected scenes, 1 is full quality, lower values are faster"};
cvar_t r_water_refractdistort = {CVAR_SAVE, "r_water_refractdistort", "0.01", "how much water refractions shimmer"};
cvar_t r_water_reflectdistort = {CVAR_SAVE, "r_water_reflectdistort", "0.01", "how much water reflections shimmer"};
cvar_t r_water_scissormode = {0, "r_water_scissormode", "3", "scissor (1) or cull (2) or both (3) water renders"};
cvar_t r_water_lowquality = {0, "r_water_lowquality", "0", "special option to accelerate water rendering, 1 disables shadows and particles, 2 disables all dynamic lights"};

cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "0", "enables animation smoothing on sprites"};
cvar_t r_lerpmodels = {CVAR_SAVE, "r_lerpmodels", "1", "enables animation smoothing on models"};
cvar_t r_lerplightstyles = {CVAR_SAVE, "r_lerplightstyles", "0", "enable animation smoothing on flickering lights"};
cvar_t r_waterscroll = {CVAR_SAVE, "r_waterscroll", "1", "makes water scroll around, value controls how much"};

cvar_t r_bloom = {CVAR_SAVE, "r_bloom", "0", "enables bloom effect (makes bright pixels affect neighboring pixels)"};
cvar_t r_bloom_colorscale = {CVAR_SAVE, "r_bloom_colorscale", "1", "how bright the glow is"};
cvar_t r_bloom_brighten = {CVAR_SAVE, "r_bloom_brighten", "2", "how bright the glow is, after subtract/power"};
cvar_t r_bloom_blur = {CVAR_SAVE, "r_bloom_blur", "4", "how large the glow is"};
cvar_t r_bloom_resolution = {CVAR_SAVE, "r_bloom_resolution", "320", "what resolution to perform the bloom effect at (independent of screen resolution)"};
cvar_t r_bloom_colorexponent = {CVAR_SAVE, "r_bloom_colorexponent", "1", "how exaggerated the glow is"};
cvar_t r_bloom_colorsubtract = {CVAR_SAVE, "r_bloom_colorsubtract", "0.125", "reduces bloom colors by a certain amount"};

cvar_t r_hdr = {CVAR_SAVE, "r_hdr", "0", "enables High Dynamic Range bloom effect (higher quality version of r_bloom)"};
cvar_t r_hdr_scenebrightness = {CVAR_SAVE, "r_hdr_scenebrightness", "1", "global rendering brightness"};
cvar_t r_hdr_glowintensity = {CVAR_SAVE, "r_hdr_glowintensity", "1", "how bright light emitting textures should appear"};
cvar_t r_hdr_range = {CVAR_SAVE, "r_hdr_range", "4", "how much dynamic range to render bloom with (equivalent to multiplying r_bloom_brighten by this value and dividing r_bloom_colorscale by this value)"};
cvar_t r_hdr_irisadaptation = {CVAR_SAVE, "r_hdr_irisadaptation", "0", "adjust scene brightness according to light intensity at player location"};
cvar_t r_hdr_irisadaptation_multiplier = {CVAR_SAVE, "r_hdr_irisadaptation_multiplier", "2", "brightness at which value will be 1.0"};
cvar_t r_hdr_irisadaptation_minvalue = {CVAR_SAVE, "r_hdr_irisadaptation_minvalue", "0.5", "minimum value that can result from multiplier / brightness"};
cvar_t r_hdr_irisadaptation_maxvalue = {CVAR_SAVE, "r_hdr_irisadaptation_maxvalue", "4", "maximum value that can result from multiplier / brightness"};
cvar_t r_hdr_irisadaptation_value = {0, "r_hdr_irisadaptation_value", "1", "current value as scenebrightness multiplier, changes continuously when irisadaptation is active"};
cvar_t r_hdr_irisadaptation_fade = {CVAR_SAVE, "r_hdr_irisadaptation_fade", "1", "fade rate at which value adjusts"};

cvar_t r_smoothnormals_areaweighting = {0, "r_smoothnormals_areaweighting", "1", "uses significantly faster (and supposedly higher quality) area-weighted vertex normals and tangent vectors rather than summing normalized triangle normals and tangents"};

cvar_t developer_texturelogging = {0, "developer_texturelogging", "0", "produces a textures.log file containing names of skins and map textures the engine tried to load"};

cvar_t gl_lightmaps = {0, "gl_lightmaps", "0", "draws only lightmaps, no texture (for level designers)"};

cvar_t r_test = {0, "r_test", "0", "internal development use only, leave it alone (usually does nothing anyway)"};

cvar_t r_glsl_saturation = {CVAR_SAVE, "r_glsl_saturation", "1", "saturation multiplier (only working in glsl!)"};
cvar_t r_glsl_saturation_redcompensate = {CVAR_SAVE, "r_glsl_saturation_redcompensate", "0", "a 'vampire sight' addition to desaturation effect, does compensation for red color, r_glsl_restart is required"};

cvar_t r_glsl_vertextextureblend_usebothalphas = {CVAR_SAVE, "r_glsl_vertextextureblend_usebothalphas", "0", "use both alpha layers on vertex blended surfaces, each alpha layer sets amount of 'blend leak' on another layer."};

cvar_t r_framedatasize = {CVAR_SAVE, "r_framedatasize", "0.5", "size of renderer data cache used during one frame (for skeletal animation caching, light processing, etc)"};

extern cvar_t v_glslgamma;

extern qboolean v_flipped_state;

static struct r_bloomstate_s
{
	qboolean enabled;
	qboolean hdr;

	int bloomwidth, bloomheight;

	textype_t texturetype;
	int viewfbo; // used to check if r_viewfbo cvar has changed

	int fbo_framebuffer; // non-zero if r_viewfbo is enabled and working
	rtexture_t *texture_framebuffercolor; // non-NULL if fbo_screen is non-zero
	rtexture_t *texture_framebufferdepth; // non-NULL if fbo_screen is non-zero

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
rtexture_t *r_texture_fogheighttexture;
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
cubemapinfo_t *r_texture_cubemaps[MAX_CUBEMAPS];

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
extern const float r_screenvertex3f[12];
extern const float r_d3dscreenvertex3f[12];
const float r_screenvertex3f[12] =
{
	0, 0, 0,
	1, 0, 0,
	1, 1, 0,
	0, 1, 0
};
const float r_d3dscreenvertex3f[12] =
{
	0, 1, 0,
	1, 1, 0,
	1, 0, 0,
	0, 0, 0
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
	memset(r_refdef.fog_height_texturename, 0, sizeof(r_refdef.fog_height_texturename));
}

static void R_BuildBlankTextures(void)
{
	unsigned char data[4];
	data[2] = 128; // normal X
	data[1] = 128; // normal Y
	data[0] = 255; // normal Z
	data[3] = 128; // height
	r_texture_blanknormalmap = R_LoadTexture2D(r_main_texturepool, "blankbump", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, -1, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_texture_white = R_LoadTexture2D(r_main_texturepool, "blankwhite", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, -1, NULL);
	data[0] = 128;
	data[1] = 128;
	data[2] = 128;
	data[3] = 255;
	r_texture_grey128 = R_LoadTexture2D(r_main_texturepool, "blankgrey128", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, -1, NULL);
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 255;
	r_texture_black = R_LoadTexture2D(r_main_texturepool, "blankblack", 1, 1, data, TEXTYPE_BGRA, TEXF_PERSISTENT, -1, NULL);
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
	r_texture_notexture = R_LoadTexture2D(r_main_texturepool, "notexture", 16, 16, &pix[0][0][0], TEXTYPE_BGRA, TEXF_MIPMAP | TEXF_PERSISTENT, -1, NULL);
}

static void R_BuildWhiteCube(void)
{
	unsigned char data[6*1*1*4];
	memset(data, 255, sizeof(data));
	r_texture_whitecube = R_LoadTextureCubeMap(r_main_texturepool, "whitecube", 1, data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_PERSISTENT, -1, NULL);
}

static void R_BuildNormalizationCube(void)
{
	int x, y, side;
	vec3_t v;
	vec_t s, t, intensity;
#define NORMSIZE 64
	unsigned char *data;
	data = (unsigned char *)Mem_Alloc(tempmempool, 6*NORMSIZE*NORMSIZE*4);
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
	r_texture_normalizationcube = R_LoadTextureCubeMap(r_main_texturepool, "normalcube", NORMSIZE, data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_PERSISTENT, -1, NULL);
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
		R_UpdateTexture(r_texture_fogattenuation, &data1[0][0], 0, 0, 0, FOGWIDTH, 1, 1);
		//R_UpdateTexture(r_texture_fogattenuation, &data2[0][0], 0, 0, 0, FOGWIDTH, 1, 1);
	}
	else
	{
		r_texture_fogattenuation = R_LoadTexture2D(r_main_texturepool, "fogattenuation", FOGWIDTH, 1, &data1[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_PERSISTENT, -1, NULL);
		//r_texture_fogintensity = R_LoadTexture2D(r_main_texturepool, "fogintensity", FOGWIDTH, 1, &data2[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
	}
}

static void R_BuildFogHeightTexture(void)
{
	unsigned char *inpixels;
	int size;
	int x;
	int y;
	int j;
	float c[4];
	float f;
	inpixels = NULL;
	strlcpy(r_refdef.fogheighttexturename, r_refdef.fog_height_texturename, sizeof(r_refdef.fogheighttexturename));
	if (r_refdef.fogheighttexturename[0])
		inpixels = loadimagepixelsbgra(r_refdef.fogheighttexturename, true, false, false, NULL);
	if (!inpixels)
	{
		r_refdef.fog_height_tablesize = 0;
		if (r_texture_fogheighttexture)
			R_FreeTexture(r_texture_fogheighttexture);
		r_texture_fogheighttexture = NULL;
		if (r_refdef.fog_height_table2d)
			Mem_Free(r_refdef.fog_height_table2d);
		r_refdef.fog_height_table2d = NULL;
		if (r_refdef.fog_height_table1d)
			Mem_Free(r_refdef.fog_height_table1d);
		r_refdef.fog_height_table1d = NULL;
		return;
	}
	size = image_width;
	r_refdef.fog_height_tablesize = size;
	r_refdef.fog_height_table1d = (unsigned char *)Mem_Alloc(r_main_mempool, size * 4);
	r_refdef.fog_height_table2d = (unsigned char *)Mem_Alloc(r_main_mempool, size * size * 4);
	memcpy(r_refdef.fog_height_table1d, inpixels, size * 4);
	Mem_Free(inpixels);
	// LordHavoc: now the magic - what is that table2d for?  it is a cooked
	// average fog color table accounting for every fog layer between a point
	// and the camera.  (Note: attenuation is handled separately!)
	for (y = 0;y < size;y++)
	{
		for (x = 0;x < size;x++)
		{
			Vector4Clear(c);
			f = 0;
			if (x < y)
			{
				for (j = x;j <= y;j++)
				{
					Vector4Add(c, r_refdef.fog_height_table1d + j*4, c);
					f++;
				}
			}
			else
			{
				for (j = x;j >= y;j--)
				{
					Vector4Add(c, r_refdef.fog_height_table1d + j*4, c);
					f++;
				}
			}
			f = 1.0f / f;
			r_refdef.fog_height_table2d[(y*size+x)*4+0] = (unsigned char)(c[0] * f);
			r_refdef.fog_height_table2d[(y*size+x)*4+1] = (unsigned char)(c[1] * f);
			r_refdef.fog_height_table2d[(y*size+x)*4+2] = (unsigned char)(c[2] * f);
			r_refdef.fog_height_table2d[(y*size+x)*4+3] = (unsigned char)(c[3] * f);
		}
	}
	r_texture_fogheighttexture = R_LoadTexture2D(r_main_texturepool, "fogheighttable", size, size, r_refdef.fog_height_table2d, TEXTYPE_BGRA, TEXF_ALPHA | TEXF_CLAMP, -1, NULL);
}

//=======================================================================================================================================================

static const char *builtinshaderstring =
#include "shader_glsl.h"
;

const char *builtinhlslshaderstring =
#include "shader_hlsl.h"
;

char *glslshaderstring = NULL;
char *hlslshaderstring = NULL;

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
	{"#define USEFOGHEIGHTTEXTURE\n", " fogheighttexture"},
	{"#define USEFOGALPHAHACK\n", " fogalphahack"},
	{"#define USEGAMMARAMPS\n", " gammaramps"},
	{"#define USECUBEFILTER\n", " cubefilter"},
	{"#define USEGLOW\n", " glow"},
	{"#define USEBLOOM\n", " bloom"},
	{"#define USESPECULAR\n", " specular"},
	{"#define USEPOSTPROCESSING\n", " postprocessing"},
	{"#define USEREFLECTION\n", " reflection"},
	{"#define USEOFFSETMAPPING\n", " offsetmapping"},
	{"#define USEOFFSETMAPPING_RELIEFMAPPING\n", " reliefmapping"},
	{"#define USESHADOWMAP2D\n", " shadowmap2d"},
	{"#define USESHADOWMAPPCF 1\n", " shadowmappcf"},
	{"#define USESHADOWMAPPCF 2\n", " shadowmappcf2"},
	{"#define USESHADOWSAMPLER\n", " shadowsampler"},
	{"#define USESHADOWMAPVSDCT\n", " shadowmapvsdct"},
	{"#define USESHADOWMAPORTHO\n", " shadowmaportho"},
	{"#define USEDEFERREDLIGHTMAP\n", " deferredlightmap"},
	{"#define USEALPHAKILL\n", " alphakill"},
	{"#define USEREFLECTCUBE\n", " reflectcube"},
	{"#define USENORMALMAPSCROLLBLEND\n", " normalmapscrollblend"},
	{"#define USEBOUNCEGRID\n", " bouncegrid"},
	{"#define USEBOUNCEGRIDDIRECTIONAL\n", " bouncegriddirectional"},
	{"#define USETRIPPY\n", " trippy"},
};

// NOTE: MUST MATCH ORDER OF SHADERMODE_* ENUMS!
shadermodeinfo_t glslshadermodeinfo[SHADERMODE_COUNT] =
{
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_GENERIC\n", " generic"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_POSTPROCESS\n", " postprocess"},
	{"glsl/default.glsl", NULL, NULL               , "#define MODE_DEPTH_OR_SHADOW\n", " depth/shadow"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_FLATCOLOR\n", " flatcolor"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_VERTEXCOLOR\n", " vertexcolor"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_LIGHTMAP\n", " lightmap"},
	{"glsl/default.glsl", NULL, "glsl/default.glsl", "#define MODE_FAKELIGHT\n", " fakelight"},
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

shadermodeinfo_t hlslshadermodeinfo[SHADERMODE_COUNT] =
{
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_GENERIC\n", " generic"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_POSTPROCESS\n", " postprocess"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_DEPTH_OR_SHADOW\n", " depth/shadow"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_FLATCOLOR\n", " flatcolor"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_VERTEXCOLOR\n", " vertexcolor"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_LIGHTMAP\n", " lightmap"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_FAKELIGHT\n", " fakelight"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n", " lightdirectionmap_modelspace"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n", " lightdirectionmap_tangentspace"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_LIGHTDIRECTION\n", " lightdirection"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_LIGHTSOURCE\n", " lightsource"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_REFRACTION\n", " refraction"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_WATER\n", " water"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_SHOWDEPTH\n", " showdepth"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_DEFERREDGEOMETRY\n", " deferredgeometry"},
	{"hlsl/default.hlsl", NULL, "hlsl/default.hlsl", "#define MODE_DEFERREDLIGHTSOURCE\n", " deferredlightsource"},
};

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
	// texture units assigned to each detected uniform
	int tex_Texture_First;
	int tex_Texture_Second;
	int tex_Texture_GammaRamps;
	int tex_Texture_Normal;
	int tex_Texture_Color;
	int tex_Texture_Gloss;
	int tex_Texture_Glow;
	int tex_Texture_SecondaryNormal;
	int tex_Texture_SecondaryColor;
	int tex_Texture_SecondaryGloss;
	int tex_Texture_SecondaryGlow;
	int tex_Texture_Pants;
	int tex_Texture_Shirt;
	int tex_Texture_FogHeightTexture;
	int tex_Texture_FogMask;
	int tex_Texture_Lightmap;
	int tex_Texture_Deluxemap;
	int tex_Texture_Attenuation;
	int tex_Texture_Cube;
	int tex_Texture_Refraction;
	int tex_Texture_Reflection;
	int tex_Texture_ShadowMap2D;
	int tex_Texture_CubeProjection;
	int tex_Texture_ScreenDepth;
	int tex_Texture_ScreenNormalMap;
	int tex_Texture_ScreenDiffuse;
	int tex_Texture_ScreenSpecular;
	int tex_Texture_ReflectMask;
	int tex_Texture_ReflectCube;
	int tex_Texture_BounceGrid;
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
	int loc_Texture_FogHeightTexture;
	int loc_Texture_FogMask;
	int loc_Texture_Lightmap;
	int loc_Texture_Deluxemap;
	int loc_Texture_Attenuation;
	int loc_Texture_Cube;
	int loc_Texture_Refraction;
	int loc_Texture_Reflection;
	int loc_Texture_ShadowMap2D;
	int loc_Texture_CubeProjection;
	int loc_Texture_ScreenDepth;
	int loc_Texture_ScreenNormalMap;
	int loc_Texture_ScreenDiffuse;
	int loc_Texture_ScreenSpecular;
	int loc_Texture_ReflectMask;
	int loc_Texture_ReflectCube;
	int loc_Texture_BounceGrid;
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
	int loc_OffsetMapping_ScaleSteps;
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
	int loc_BloomColorSubtract;
	int loc_NormalmapScrollBlend;
	int loc_BounceGridMatrix;
	int loc_BounceGridIntensity;
}
r_glsl_permutation_t;

#define SHADERPERMUTATION_HASHSIZE 256


// non-degradable "lightweight" shader parameters to keep the permutations simpler
// these can NOT degrade! only use for simple stuff
enum
{
	SHADERSTATICPARM_SATURATION_REDCOMPENSATE = 0, ///< red compensation filter for saturation
	SHADERSTATICPARM_EXACTSPECULARMATH = 1, ///< (lightsource or deluxemapping) use exact reflection map for specular effects, as opposed to the usual OpenGL approximation
	SHADERSTATICPARM_POSTPROCESS_USERVEC1 = 2, ///< postprocess uservec1 is enabled
	SHADERSTATICPARM_POSTPROCESS_USERVEC2 = 3, ///< postprocess uservec2 is enabled
	SHADERSTATICPARM_POSTPROCESS_USERVEC3 = 4, ///< postprocess uservec3 is enabled
	SHADERSTATICPARM_POSTPROCESS_USERVEC4 = 5,  ///< postprocess uservec4 is enabled
	SHADERSTATICPARM_VERTEXTEXTUREBLEND_USEBOTHALPHAS = 6 // use both alpha layers while blending materials, allows more advanced microblending
};
#define SHADERSTATICPARMS_COUNT 7

static const char *shaderstaticparmstrings_list[SHADERSTATICPARMS_COUNT];
static int shaderstaticparms_count = 0;

static unsigned int r_compileshader_staticparms[(SHADERSTATICPARMS_COUNT + 0x1F) >> 5] = {0};
#define R_COMPILESHADER_STATICPARM_ENABLE(p) r_compileshader_staticparms[(p) >> 5] |= (1 << ((p) & 0x1F))
qboolean R_CompileShader_CheckStaticParms(void)
{
	static int r_compileshader_staticparms_save[1];
	memcpy(r_compileshader_staticparms_save, r_compileshader_staticparms, sizeof(r_compileshader_staticparms));
	memset(r_compileshader_staticparms, 0, sizeof(r_compileshader_staticparms));

	// detect all
	if (r_glsl_saturation_redcompensate.integer)
		R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_SATURATION_REDCOMPENSATE);
	if (r_glsl_vertextextureblend_usebothalphas.integer)
		R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_VERTEXTEXTUREBLEND_USEBOTHALPHAS);
	if (r_shadow_glossexact.integer)
		R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_EXACTSPECULARMATH);
	if (r_glsl_postprocess.integer)
	{
		if (r_glsl_postprocess_uservec1_enable.integer)
			R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_POSTPROCESS_USERVEC1);
		if (r_glsl_postprocess_uservec2_enable.integer)
			R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_POSTPROCESS_USERVEC2);
		if (r_glsl_postprocess_uservec3_enable.integer)
			R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_POSTPROCESS_USERVEC3);
		if (r_glsl_postprocess_uservec4_enable.integer)
			R_COMPILESHADER_STATICPARM_ENABLE(SHADERSTATICPARM_POSTPROCESS_USERVEC4);
	}
	return memcmp(r_compileshader_staticparms, r_compileshader_staticparms_save, sizeof(r_compileshader_staticparms)) != 0;
}

#define R_COMPILESHADER_STATICPARM_EMIT(p, n) \
	if(r_compileshader_staticparms[(p) >> 5] & (1 << ((p) & 0x1F))) \
		shaderstaticparmstrings_list[shaderstaticparms_count++] = "#define " n "\n"; \
	else \
		shaderstaticparmstrings_list[shaderstaticparms_count++] = "\n"
void R_CompileShader_AddStaticParms(unsigned int mode, unsigned int permutation)
{
	shaderstaticparms_count = 0;

	// emit all
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_SATURATION_REDCOMPENSATE, "SATURATION_REDCOMPENSATE");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_EXACTSPECULARMATH, "USEEXACTSPECULARMATH");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_POSTPROCESS_USERVEC1, "USERVEC1");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_POSTPROCESS_USERVEC2, "USERVEC2");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_POSTPROCESS_USERVEC3, "USERVEC3");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_POSTPROCESS_USERVEC4, "USERVEC4");
	R_COMPILESHADER_STATICPARM_EMIT(SHADERSTATICPARM_VERTEXTEXTUREBLEND_USEBOTHALPHAS, "USEBOTHALPHAS");
}

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
	int sampler;
	shadermodeinfo_t *modeinfo = glslshadermodeinfo + mode;
	char *vertexstring, *geometrystring, *fragmentstring;
	char permutationname[256];
	int vertstrings_count = 0;
	int geomstrings_count = 0;
	int fragstrings_count = 0;
	const char *vertstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];
	const char *geomstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];
	const char *fragstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];

	if (p->compiled)
		return;
	p->compiled = true;
	p->program = 0;

	permutationname[0] = 0;
	vertexstring   = R_GLSL_GetText(modeinfo->vertexfilename, true);
	geometrystring = R_GLSL_GetText(modeinfo->geometryfilename, false);
	fragmentstring = R_GLSL_GetText(modeinfo->fragmentfilename, false);

	strlcat(permutationname, modeinfo->vertexfilename, sizeof(permutationname));

	// if we can do #version 130, we should (this improves quality of offset/reliefmapping thanks to textureGrad)
	if(vid.support.gl20shaders130)
	{
		vertstrings_list[vertstrings_count++] = "#version 130\n";
		geomstrings_list[geomstrings_count++] = "#version 130\n";
		fragstrings_list[fragstrings_count++] = "#version 130\n";
		vertstrings_list[vertstrings_count++] = "#define GLSL130\n";
		geomstrings_list[geomstrings_count++] = "#define GLSL130\n";
		fragstrings_list[fragstrings_count++] = "#define GLSL130\n";
	}

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

	// add static parms
	R_CompileShader_AddStaticParms(mode, permutation);
	memcpy((char *)(vertstrings_list + vertstrings_count), shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	vertstrings_count += shaderstaticparms_count;
	memcpy((char *)(geomstrings_list + geomstrings_count), shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	geomstrings_count += shaderstaticparms_count;
	memcpy((char *)(fragstrings_list + fragstrings_count), shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	fragstrings_count += shaderstaticparms_count;

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
		qglUseProgram(p->program);CHECKGLERROR
		// look up all the uniform variable names we care about, so we don't
		// have to look them up every time we set them

		p->loc_Texture_First              = qglGetUniformLocation(p->program, "Texture_First");
		p->loc_Texture_Second             = qglGetUniformLocation(p->program, "Texture_Second");
		p->loc_Texture_GammaRamps         = qglGetUniformLocation(p->program, "Texture_GammaRamps");
		p->loc_Texture_Normal             = qglGetUniformLocation(p->program, "Texture_Normal");
		p->loc_Texture_Color              = qglGetUniformLocation(p->program, "Texture_Color");
		p->loc_Texture_Gloss              = qglGetUniformLocation(p->program, "Texture_Gloss");
		p->loc_Texture_Glow               = qglGetUniformLocation(p->program, "Texture_Glow");
		p->loc_Texture_SecondaryNormal    = qglGetUniformLocation(p->program, "Texture_SecondaryNormal");
		p->loc_Texture_SecondaryColor     = qglGetUniformLocation(p->program, "Texture_SecondaryColor");
		p->loc_Texture_SecondaryGloss     = qglGetUniformLocation(p->program, "Texture_SecondaryGloss");
		p->loc_Texture_SecondaryGlow      = qglGetUniformLocation(p->program, "Texture_SecondaryGlow");
		p->loc_Texture_Pants              = qglGetUniformLocation(p->program, "Texture_Pants");
		p->loc_Texture_Shirt              = qglGetUniformLocation(p->program, "Texture_Shirt");
		p->loc_Texture_FogHeightTexture   = qglGetUniformLocation(p->program, "Texture_FogHeightTexture");
		p->loc_Texture_FogMask            = qglGetUniformLocation(p->program, "Texture_FogMask");
		p->loc_Texture_Lightmap           = qglGetUniformLocation(p->program, "Texture_Lightmap");
		p->loc_Texture_Deluxemap          = qglGetUniformLocation(p->program, "Texture_Deluxemap");
		p->loc_Texture_Attenuation        = qglGetUniformLocation(p->program, "Texture_Attenuation");
		p->loc_Texture_Cube               = qglGetUniformLocation(p->program, "Texture_Cube");
		p->loc_Texture_Refraction         = qglGetUniformLocation(p->program, "Texture_Refraction");
		p->loc_Texture_Reflection         = qglGetUniformLocation(p->program, "Texture_Reflection");
		p->loc_Texture_ShadowMap2D        = qglGetUniformLocation(p->program, "Texture_ShadowMap2D");
		p->loc_Texture_CubeProjection     = qglGetUniformLocation(p->program, "Texture_CubeProjection");
		p->loc_Texture_ScreenDepth        = qglGetUniformLocation(p->program, "Texture_ScreenDepth");
		p->loc_Texture_ScreenNormalMap    = qglGetUniformLocation(p->program, "Texture_ScreenNormalMap");
		p->loc_Texture_ScreenDiffuse      = qglGetUniformLocation(p->program, "Texture_ScreenDiffuse");
		p->loc_Texture_ScreenSpecular     = qglGetUniformLocation(p->program, "Texture_ScreenSpecular");
		p->loc_Texture_ReflectMask        = qglGetUniformLocation(p->program, "Texture_ReflectMask");
		p->loc_Texture_ReflectCube        = qglGetUniformLocation(p->program, "Texture_ReflectCube");
		p->loc_Texture_BounceGrid         = qglGetUniformLocation(p->program, "Texture_BounceGrid");
		p->loc_Alpha                      = qglGetUniformLocation(p->program, "Alpha");
		p->loc_BloomBlur_Parameters       = qglGetUniformLocation(p->program, "BloomBlur_Parameters");
		p->loc_ClientTime                 = qglGetUniformLocation(p->program, "ClientTime");
		p->loc_Color_Ambient              = qglGetUniformLocation(p->program, "Color_Ambient");
		p->loc_Color_Diffuse              = qglGetUniformLocation(p->program, "Color_Diffuse");
		p->loc_Color_Specular             = qglGetUniformLocation(p->program, "Color_Specular");
		p->loc_Color_Glow                 = qglGetUniformLocation(p->program, "Color_Glow");
		p->loc_Color_Pants                = qglGetUniformLocation(p->program, "Color_Pants");
		p->loc_Color_Shirt                = qglGetUniformLocation(p->program, "Color_Shirt");
		p->loc_DeferredColor_Ambient      = qglGetUniformLocation(p->program, "DeferredColor_Ambient");
		p->loc_DeferredColor_Diffuse      = qglGetUniformLocation(p->program, "DeferredColor_Diffuse");
		p->loc_DeferredColor_Specular     = qglGetUniformLocation(p->program, "DeferredColor_Specular");
		p->loc_DeferredMod_Diffuse        = qglGetUniformLocation(p->program, "DeferredMod_Diffuse");
		p->loc_DeferredMod_Specular       = qglGetUniformLocation(p->program, "DeferredMod_Specular");
		p->loc_DistortScaleRefractReflect = qglGetUniformLocation(p->program, "DistortScaleRefractReflect");
		p->loc_EyePosition                = qglGetUniformLocation(p->program, "EyePosition");
		p->loc_FogColor                   = qglGetUniformLocation(p->program, "FogColor");
		p->loc_FogHeightFade              = qglGetUniformLocation(p->program, "FogHeightFade");
		p->loc_FogPlane                   = qglGetUniformLocation(p->program, "FogPlane");
		p->loc_FogPlaneViewDist           = qglGetUniformLocation(p->program, "FogPlaneViewDist");
		p->loc_FogRangeRecip              = qglGetUniformLocation(p->program, "FogRangeRecip");
		p->loc_LightColor                 = qglGetUniformLocation(p->program, "LightColor");
		p->loc_LightDir                   = qglGetUniformLocation(p->program, "LightDir");
		p->loc_LightPosition              = qglGetUniformLocation(p->program, "LightPosition");
		p->loc_OffsetMapping_ScaleSteps   = qglGetUniformLocation(p->program, "OffsetMapping_ScaleSteps");
		p->loc_PixelSize                  = qglGetUniformLocation(p->program, "PixelSize");
		p->loc_ReflectColor               = qglGetUniformLocation(p->program, "ReflectColor");
		p->loc_ReflectFactor              = qglGetUniformLocation(p->program, "ReflectFactor");
		p->loc_ReflectOffset              = qglGetUniformLocation(p->program, "ReflectOffset");
		p->loc_RefractColor               = qglGetUniformLocation(p->program, "RefractColor");
		p->loc_Saturation                 = qglGetUniformLocation(p->program, "Saturation");
		p->loc_ScreenCenterRefractReflect = qglGetUniformLocation(p->program, "ScreenCenterRefractReflect");
		p->loc_ScreenScaleRefractReflect  = qglGetUniformLocation(p->program, "ScreenScaleRefractReflect");
		p->loc_ScreenToDepth              = qglGetUniformLocation(p->program, "ScreenToDepth");
		p->loc_ShadowMap_Parameters       = qglGetUniformLocation(p->program, "ShadowMap_Parameters");
		p->loc_ShadowMap_TextureScale     = qglGetUniformLocation(p->program, "ShadowMap_TextureScale");
		p->loc_SpecularPower              = qglGetUniformLocation(p->program, "SpecularPower");
		p->loc_UserVec1                   = qglGetUniformLocation(p->program, "UserVec1");
		p->loc_UserVec2                   = qglGetUniformLocation(p->program, "UserVec2");
		p->loc_UserVec3                   = qglGetUniformLocation(p->program, "UserVec3");
		p->loc_UserVec4                   = qglGetUniformLocation(p->program, "UserVec4");
		p->loc_ViewTintColor              = qglGetUniformLocation(p->program, "ViewTintColor");
		p->loc_ViewToLight                = qglGetUniformLocation(p->program, "ViewToLight");
		p->loc_ModelToLight               = qglGetUniformLocation(p->program, "ModelToLight");
		p->loc_TexMatrix                  = qglGetUniformLocation(p->program, "TexMatrix");
		p->loc_BackgroundTexMatrix        = qglGetUniformLocation(p->program, "BackgroundTexMatrix");
		p->loc_ModelViewMatrix            = qglGetUniformLocation(p->program, "ModelViewMatrix");
		p->loc_ModelViewProjectionMatrix  = qglGetUniformLocation(p->program, "ModelViewProjectionMatrix");
		p->loc_PixelToScreenTexCoord      = qglGetUniformLocation(p->program, "PixelToScreenTexCoord");
		p->loc_ModelToReflectCube         = qglGetUniformLocation(p->program, "ModelToReflectCube");
		p->loc_ShadowMapMatrix            = qglGetUniformLocation(p->program, "ShadowMapMatrix");
		p->loc_BloomColorSubtract         = qglGetUniformLocation(p->program, "BloomColorSubtract");
		p->loc_NormalmapScrollBlend       = qglGetUniformLocation(p->program, "NormalmapScrollBlend");
		p->loc_BounceGridMatrix           = qglGetUniformLocation(p->program, "BounceGridMatrix");
		p->loc_BounceGridIntensity        = qglGetUniformLocation(p->program, "BounceGridIntensity");
		// initialize the samplers to refer to the texture units we use
		p->tex_Texture_First = -1;
		p->tex_Texture_Second = -1;
		p->tex_Texture_GammaRamps = -1;
		p->tex_Texture_Normal = -1;
		p->tex_Texture_Color = -1;
		p->tex_Texture_Gloss = -1;
		p->tex_Texture_Glow = -1;
		p->tex_Texture_SecondaryNormal = -1;
		p->tex_Texture_SecondaryColor = -1;
		p->tex_Texture_SecondaryGloss = -1;
		p->tex_Texture_SecondaryGlow = -1;
		p->tex_Texture_Pants = -1;
		p->tex_Texture_Shirt = -1;
		p->tex_Texture_FogHeightTexture = -1;
		p->tex_Texture_FogMask = -1;
		p->tex_Texture_Lightmap = -1;
		p->tex_Texture_Deluxemap = -1;
		p->tex_Texture_Attenuation = -1;
		p->tex_Texture_Cube = -1;
		p->tex_Texture_Refraction = -1;
		p->tex_Texture_Reflection = -1;
		p->tex_Texture_ShadowMap2D = -1;
		p->tex_Texture_CubeProjection = -1;
		p->tex_Texture_ScreenDepth = -1;
		p->tex_Texture_ScreenNormalMap = -1;
		p->tex_Texture_ScreenDiffuse = -1;
		p->tex_Texture_ScreenSpecular = -1;
		p->tex_Texture_ReflectMask = -1;
		p->tex_Texture_ReflectCube = -1;
		p->tex_Texture_BounceGrid = -1;
		sampler = 0;
		if (p->loc_Texture_First           >= 0) {p->tex_Texture_First            = sampler;qglUniform1i(p->loc_Texture_First           , sampler);sampler++;}
		if (p->loc_Texture_Second          >= 0) {p->tex_Texture_Second           = sampler;qglUniform1i(p->loc_Texture_Second          , sampler);sampler++;}
		if (p->loc_Texture_GammaRamps      >= 0) {p->tex_Texture_GammaRamps       = sampler;qglUniform1i(p->loc_Texture_GammaRamps      , sampler);sampler++;}
		if (p->loc_Texture_Normal          >= 0) {p->tex_Texture_Normal           = sampler;qglUniform1i(p->loc_Texture_Normal          , sampler);sampler++;}
		if (p->loc_Texture_Color           >= 0) {p->tex_Texture_Color            = sampler;qglUniform1i(p->loc_Texture_Color           , sampler);sampler++;}
		if (p->loc_Texture_Gloss           >= 0) {p->tex_Texture_Gloss            = sampler;qglUniform1i(p->loc_Texture_Gloss           , sampler);sampler++;}
		if (p->loc_Texture_Glow            >= 0) {p->tex_Texture_Glow             = sampler;qglUniform1i(p->loc_Texture_Glow            , sampler);sampler++;}
		if (p->loc_Texture_SecondaryNormal >= 0) {p->tex_Texture_SecondaryNormal  = sampler;qglUniform1i(p->loc_Texture_SecondaryNormal , sampler);sampler++;}
		if (p->loc_Texture_SecondaryColor  >= 0) {p->tex_Texture_SecondaryColor   = sampler;qglUniform1i(p->loc_Texture_SecondaryColor  , sampler);sampler++;}
		if (p->loc_Texture_SecondaryGloss  >= 0) {p->tex_Texture_SecondaryGloss   = sampler;qglUniform1i(p->loc_Texture_SecondaryGloss  , sampler);sampler++;}
		if (p->loc_Texture_SecondaryGlow   >= 0) {p->tex_Texture_SecondaryGlow    = sampler;qglUniform1i(p->loc_Texture_SecondaryGlow   , sampler);sampler++;}
		if (p->loc_Texture_Pants           >= 0) {p->tex_Texture_Pants            = sampler;qglUniform1i(p->loc_Texture_Pants           , sampler);sampler++;}
		if (p->loc_Texture_Shirt           >= 0) {p->tex_Texture_Shirt            = sampler;qglUniform1i(p->loc_Texture_Shirt           , sampler);sampler++;}
		if (p->loc_Texture_FogHeightTexture>= 0) {p->tex_Texture_FogHeightTexture = sampler;qglUniform1i(p->loc_Texture_FogHeightTexture, sampler);sampler++;}
		if (p->loc_Texture_FogMask         >= 0) {p->tex_Texture_FogMask          = sampler;qglUniform1i(p->loc_Texture_FogMask         , sampler);sampler++;}
		if (p->loc_Texture_Lightmap        >= 0) {p->tex_Texture_Lightmap         = sampler;qglUniform1i(p->loc_Texture_Lightmap        , sampler);sampler++;}
		if (p->loc_Texture_Deluxemap       >= 0) {p->tex_Texture_Deluxemap        = sampler;qglUniform1i(p->loc_Texture_Deluxemap       , sampler);sampler++;}
		if (p->loc_Texture_Attenuation     >= 0) {p->tex_Texture_Attenuation      = sampler;qglUniform1i(p->loc_Texture_Attenuation     , sampler);sampler++;}
		if (p->loc_Texture_Cube            >= 0) {p->tex_Texture_Cube             = sampler;qglUniform1i(p->loc_Texture_Cube            , sampler);sampler++;}
		if (p->loc_Texture_Refraction      >= 0) {p->tex_Texture_Refraction       = sampler;qglUniform1i(p->loc_Texture_Refraction      , sampler);sampler++;}
		if (p->loc_Texture_Reflection      >= 0) {p->tex_Texture_Reflection       = sampler;qglUniform1i(p->loc_Texture_Reflection      , sampler);sampler++;}
		if (p->loc_Texture_ShadowMap2D     >= 0) {p->tex_Texture_ShadowMap2D      = sampler;qglUniform1i(p->loc_Texture_ShadowMap2D     , sampler);sampler++;}
		if (p->loc_Texture_CubeProjection  >= 0) {p->tex_Texture_CubeProjection   = sampler;qglUniform1i(p->loc_Texture_CubeProjection  , sampler);sampler++;}
		if (p->loc_Texture_ScreenDepth     >= 0) {p->tex_Texture_ScreenDepth      = sampler;qglUniform1i(p->loc_Texture_ScreenDepth     , sampler);sampler++;}
		if (p->loc_Texture_ScreenNormalMap >= 0) {p->tex_Texture_ScreenNormalMap  = sampler;qglUniform1i(p->loc_Texture_ScreenNormalMap , sampler);sampler++;}
		if (p->loc_Texture_ScreenDiffuse   >= 0) {p->tex_Texture_ScreenDiffuse    = sampler;qglUniform1i(p->loc_Texture_ScreenDiffuse   , sampler);sampler++;}
		if (p->loc_Texture_ScreenSpecular  >= 0) {p->tex_Texture_ScreenSpecular   = sampler;qglUniform1i(p->loc_Texture_ScreenSpecular  , sampler);sampler++;}
		if (p->loc_Texture_ReflectMask     >= 0) {p->tex_Texture_ReflectMask      = sampler;qglUniform1i(p->loc_Texture_ReflectMask     , sampler);sampler++;}
		if (p->loc_Texture_ReflectCube     >= 0) {p->tex_Texture_ReflectCube      = sampler;qglUniform1i(p->loc_Texture_ReflectCube     , sampler);sampler++;}
		if (p->loc_Texture_BounceGrid      >= 0) {p->tex_Texture_BounceGrid       = sampler;qglUniform1i(p->loc_Texture_BounceGrid      , sampler);sampler++;}
		CHECKGLERROR
		Con_DPrintf("^5GLSL shader %s compiled (%i textures).\n", permutationname, sampler);
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
					qglUseProgram(0);CHECKGLERROR
					return; // no bit left to clear, entire mode is broken
				}
			}
		}
		CHECKGLERROR
		qglUseProgram(r_glsl_permutation->program);CHECKGLERROR
	}
	if (r_glsl_permutation->loc_ModelViewProjectionMatrix >= 0) qglUniformMatrix4fv(r_glsl_permutation->loc_ModelViewProjectionMatrix, 1, false, gl_modelviewprojection16f);
	if (r_glsl_permutation->loc_ModelViewMatrix >= 0) qglUniformMatrix4fv(r_glsl_permutation->loc_ModelViewMatrix, 1, false, gl_modelview16f);
	if (r_glsl_permutation->loc_ClientTime >= 0) qglUniform1f(r_glsl_permutation->loc_ClientTime, cl.time);
}

#ifdef SUPPORTD3D

#ifdef SUPPORTD3D
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 vid_d3d9dev;
extern D3DCAPS9 vid_d3d9caps;
#endif

struct r_hlsl_permutation_s;
typedef struct r_hlsl_permutation_s
{
	/// hash lookup data
	struct r_hlsl_permutation_s *hashnext;
	unsigned int mode;
	unsigned int permutation;

	/// indicates if we have tried compiling this permutation already
	qboolean compiled;
	/// NULL if compilation failed
	IDirect3DVertexShader9 *vertexshader;
	IDirect3DPixelShader9 *pixelshader;
}
r_hlsl_permutation_t;

typedef enum D3DVSREGISTER_e
{
	D3DVSREGISTER_TexMatrix = 0, // float4x4
	D3DVSREGISTER_BackgroundTexMatrix = 4, // float4x4
	D3DVSREGISTER_ModelViewProjectionMatrix = 8, // float4x4
	D3DVSREGISTER_ModelViewMatrix = 12, // float4x4
	D3DVSREGISTER_ShadowMapMatrix = 16, // float4x4
	D3DVSREGISTER_ModelToLight = 20, // float4x4
	D3DVSREGISTER_EyePosition = 24,
	D3DVSREGISTER_FogPlane = 25,
	D3DVSREGISTER_LightDir = 26,
	D3DVSREGISTER_LightPosition = 27,
}
D3DVSREGISTER_t;

typedef enum D3DPSREGISTER_e
{
	D3DPSREGISTER_Alpha = 0,
	D3DPSREGISTER_BloomBlur_Parameters = 1,
	D3DPSREGISTER_ClientTime = 2,
	D3DPSREGISTER_Color_Ambient = 3,
	D3DPSREGISTER_Color_Diffuse = 4,
	D3DPSREGISTER_Color_Specular = 5,
	D3DPSREGISTER_Color_Glow = 6,
	D3DPSREGISTER_Color_Pants = 7,
	D3DPSREGISTER_Color_Shirt = 8,
	D3DPSREGISTER_DeferredColor_Ambient = 9,
	D3DPSREGISTER_DeferredColor_Diffuse = 10,
	D3DPSREGISTER_DeferredColor_Specular = 11,
	D3DPSREGISTER_DeferredMod_Diffuse = 12,
	D3DPSREGISTER_DeferredMod_Specular = 13,
	D3DPSREGISTER_DistortScaleRefractReflect = 14,
	D3DPSREGISTER_EyePosition = 15, // unused
	D3DPSREGISTER_FogColor = 16,
	D3DPSREGISTER_FogHeightFade = 17,
	D3DPSREGISTER_FogPlane = 18,
	D3DPSREGISTER_FogPlaneViewDist = 19,
	D3DPSREGISTER_FogRangeRecip = 20,
	D3DPSREGISTER_LightColor = 21,
	D3DPSREGISTER_LightDir = 22, // unused
	D3DPSREGISTER_LightPosition = 23,
	D3DPSREGISTER_OffsetMapping_ScaleSteps = 24,
	D3DPSREGISTER_PixelSize = 25,
	D3DPSREGISTER_ReflectColor = 26,
	D3DPSREGISTER_ReflectFactor = 27,
	D3DPSREGISTER_ReflectOffset = 28,
	D3DPSREGISTER_RefractColor = 29,
	D3DPSREGISTER_Saturation = 30,
	D3DPSREGISTER_ScreenCenterRefractReflect = 31,
	D3DPSREGISTER_ScreenScaleRefractReflect = 32,
	D3DPSREGISTER_ScreenToDepth = 33,
	D3DPSREGISTER_ShadowMap_Parameters = 34,
	D3DPSREGISTER_ShadowMap_TextureScale = 35,
	D3DPSREGISTER_SpecularPower = 36,
	D3DPSREGISTER_UserVec1 = 37,
	D3DPSREGISTER_UserVec2 = 38,
	D3DPSREGISTER_UserVec3 = 39,
	D3DPSREGISTER_UserVec4 = 40,
	D3DPSREGISTER_ViewTintColor = 41,
	D3DPSREGISTER_PixelToScreenTexCoord = 42,
	D3DPSREGISTER_BloomColorSubtract = 43,
	D3DPSREGISTER_ViewToLight = 44, // float4x4
	D3DPSREGISTER_ModelToReflectCube = 48, // float4x4
	D3DPSREGISTER_NormalmapScrollBlend = 52,
	// next at 53
}
D3DPSREGISTER_t;

/// information about each possible shader permutation
r_hlsl_permutation_t *r_hlsl_permutationhash[SHADERMODE_COUNT][SHADERPERMUTATION_HASHSIZE];
/// currently selected permutation
r_hlsl_permutation_t *r_hlsl_permutation;
/// storage for permutations linked in the hash table
memexpandablearray_t r_hlsl_permutationarray;

static r_hlsl_permutation_t *R_HLSL_FindPermutation(unsigned int mode, unsigned int permutation)
{
	//unsigned int hashdepth = 0;
	unsigned int hashindex = (permutation * 0x1021) & (SHADERPERMUTATION_HASHSIZE - 1);
	r_hlsl_permutation_t *p;
	for (p = r_hlsl_permutationhash[mode][hashindex];p;p = p->hashnext)
	{
		if (p->mode == mode && p->permutation == permutation)
		{
			//if (hashdepth > 10)
			//	Con_Printf("R_HLSL_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
			return p;
		}
		//hashdepth++;
	}
	p = (r_hlsl_permutation_t*)Mem_ExpandableArray_AllocRecord(&r_hlsl_permutationarray);
	p->mode = mode;
	p->permutation = permutation;
	p->hashnext = r_hlsl_permutationhash[mode][hashindex];
	r_hlsl_permutationhash[mode][hashindex] = p;
	//if (hashdepth > 10)
	//	Con_Printf("R_HLSL_FindPermutation: Warning: %i:%i has hashdepth %i\n", mode, permutation, hashdepth);
	return p;
}

static char *R_HLSL_GetText(const char *filename, qboolean printfromdisknotice)
{
	char *shaderstring;
	if (!filename || !filename[0])
		return NULL;
	if (!strcmp(filename, "hlsl/default.hlsl"))
	{
		if (!hlslshaderstring)
		{
			hlslshaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
			if (hlslshaderstring)
				Con_DPrintf("Loading shaders from file %s...\n", filename);
			else
				hlslshaderstring = (char *)builtinhlslshaderstring;
		}
		shaderstring = (char *) Mem_Alloc(r_main_mempool, strlen(hlslshaderstring) + 1);
		memcpy(shaderstring, hlslshaderstring, strlen(hlslshaderstring) + 1);
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

#include <d3dx9.h>
//#include <d3dx9shader.h>
//#include <d3dx9mesh.h>

static void R_HLSL_CacheShader(r_hlsl_permutation_t *p, const char *cachename, const char *vertstring, const char *fragstring)
{
	DWORD *vsbin = NULL;
	DWORD *psbin = NULL;
	fs_offset_t vsbinsize;
	fs_offset_t psbinsize;
//	IDirect3DVertexShader9 *vs = NULL;
//	IDirect3DPixelShader9 *ps = NULL;
	ID3DXBuffer *vslog = NULL;
	ID3DXBuffer *vsbuffer = NULL;
	ID3DXConstantTable *vsconstanttable = NULL;
	ID3DXBuffer *pslog = NULL;
	ID3DXBuffer *psbuffer = NULL;
	ID3DXConstantTable *psconstanttable = NULL;
	int vsresult = 0;
	int psresult = 0;
	char temp[MAX_INPUTLINE];
	const char *vsversion = "vs_3_0", *psversion = "ps_3_0";
	qboolean debugshader = gl_paranoid.integer != 0;
	if (p->permutation & SHADERPERMUTATION_OFFSETMAPPING) {vsversion = "vs_3_0";psversion = "ps_3_0";}
	if (p->permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) {vsversion = "vs_3_0";psversion = "ps_3_0";}
	if (!debugshader)
	{
		vsbin = (DWORD *)FS_LoadFile(va("%s.vsbin", cachename), r_main_mempool, true, &vsbinsize);
		psbin = (DWORD *)FS_LoadFile(va("%s.psbin", cachename), r_main_mempool, true, &psbinsize);
	}
	if ((!vsbin && vertstring) || (!psbin && fragstring))
	{
		const char* dllnames_d3dx9 [] =
		{
			"d3dx9_43.dll",
			"d3dx9_42.dll",
			"d3dx9_41.dll",
			"d3dx9_40.dll",
			"d3dx9_39.dll",
			"d3dx9_38.dll",
			"d3dx9_37.dll",
			"d3dx9_36.dll",
			"d3dx9_35.dll",
			"d3dx9_34.dll",
			"d3dx9_33.dll",
			"d3dx9_32.dll",
			"d3dx9_31.dll",
			"d3dx9_30.dll",
			"d3dx9_29.dll",
			"d3dx9_28.dll",
			"d3dx9_27.dll",
			"d3dx9_26.dll",
			"d3dx9_25.dll",
			"d3dx9_24.dll",
			NULL
		};
		dllhandle_t d3dx9_dll = NULL;
		HRESULT (WINAPI *qD3DXCompileShaderFromFileA)(LPCSTR pSrcFile, CONST D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags, LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs, LPD3DXCONSTANTTABLE* ppConstantTable);
		HRESULT (WINAPI *qD3DXPreprocessShader)(LPCSTR pSrcData, UINT SrcDataSize, CONST D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, LPD3DXBUFFER* ppShaderText, LPD3DXBUFFER* ppErrorMsgs);
		HRESULT (WINAPI *qD3DXCompileShader)(LPCSTR pSrcData, UINT SrcDataLen, CONST D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags, LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs, LPD3DXCONSTANTTABLE* ppConstantTable);
		dllfunction_t d3dx9_dllfuncs[] =
		{
			{"D3DXCompileShaderFromFileA",	(void **) &qD3DXCompileShaderFromFileA},
			{"D3DXPreprocessShader",		(void **) &qD3DXPreprocessShader},
			{"D3DXCompileShader",			(void **) &qD3DXCompileShader},
			{NULL, NULL}
		};
		if (Sys_LoadLibrary(dllnames_d3dx9, &d3dx9_dll, d3dx9_dllfuncs))
		{
			DWORD shaderflags = 0;
			if (debugshader)
				shaderflags = D3DXSHADER_DEBUG | D3DXSHADER_SKIPOPTIMIZATION;
			vsbin = (DWORD *)Mem_Realloc(tempmempool, vsbin, 0);
			psbin = (DWORD *)Mem_Realloc(tempmempool, psbin, 0);
			if (vertstring && vertstring[0])
			{
				if (debugshader)
				{
//					vsresult = qD3DXPreprocessShader(vertstring, strlen(vertstring), NULL, NULL, &vsbuffer, &vslog);
//					FS_WriteFile(va("%s_vs.fx", cachename), vsbuffer->GetBufferPointer(), vsbuffer->GetBufferSize());
					FS_WriteFile(va("%s_vs.fx", cachename), vertstring, strlen(vertstring));
					vsresult = qD3DXCompileShaderFromFileA(va("%s/%s_vs.fx", fs_gamedir, cachename), NULL, NULL, "main", vsversion, shaderflags, &vsbuffer, &vslog, &vsconstanttable);
				}
				else
					vsresult = qD3DXCompileShader(vertstring, strlen(vertstring), NULL, NULL, "main", vsversion, shaderflags, &vsbuffer, &vslog, &vsconstanttable);
				if (vsbuffer)
				{
					vsbinsize = vsbuffer->GetBufferSize();
					vsbin = (DWORD *)Mem_Alloc(tempmempool, vsbinsize);
					memcpy(vsbin, vsbuffer->GetBufferPointer(), vsbinsize);
					vsbuffer->Release();
				}
				if (vslog)
				{
					strlcpy(temp, (const char *)vslog->GetBufferPointer(), min(sizeof(temp), vslog->GetBufferSize()));
					Con_Printf("HLSL vertex shader compile output for %s follows:\n%s\n", cachename, temp);
					vslog->Release();
				}
			}
			if (fragstring && fragstring[0])
			{
				if (debugshader)
				{
//					psresult = qD3DXPreprocessShader(fragstring, strlen(fragstring), NULL, NULL, &psbuffer, &pslog);
//					FS_WriteFile(va("%s_ps.fx", cachename), psbuffer->GetBufferPointer(), psbuffer->GetBufferSize());
					FS_WriteFile(va("%s_ps.fx", cachename), fragstring, strlen(fragstring));
					psresult = qD3DXCompileShaderFromFileA(va("%s/%s_ps.fx", fs_gamedir, cachename), NULL, NULL, "main", psversion, shaderflags, &psbuffer, &pslog, &psconstanttable);
				}
				else
					psresult = qD3DXCompileShader(fragstring, strlen(fragstring), NULL, NULL, "main", psversion, shaderflags, &psbuffer, &pslog, &psconstanttable);
				if (psbuffer)
				{
					psbinsize = psbuffer->GetBufferSize();
					psbin = (DWORD *)Mem_Alloc(tempmempool, psbinsize);
					memcpy(psbin, psbuffer->GetBufferPointer(), psbinsize);
					psbuffer->Release();
				}
				if (pslog)
				{
					strlcpy(temp, (const char *)pslog->GetBufferPointer(), min(sizeof(temp), pslog->GetBufferSize()));
					Con_Printf("HLSL pixel shader compile output for %s follows:\n%s\n", cachename, temp);
					pslog->Release();
				}
			}
			Sys_UnloadLibrary(&d3dx9_dll);
		}
		else
			Con_Printf("Unable to compile shader - D3DXCompileShader function not found\n");
	}
	if (vsbin && psbin)
	{
		vsresult = IDirect3DDevice9_CreateVertexShader(vid_d3d9dev, vsbin, &p->vertexshader);
		if (FAILED(vsresult))
			Con_Printf("HLSL CreateVertexShader failed for %s (hresult = %8x)\n", cachename, vsresult);
		psresult = IDirect3DDevice9_CreatePixelShader(vid_d3d9dev, psbin, &p->pixelshader);
		if (FAILED(psresult))
			Con_Printf("HLSL CreatePixelShader failed for %s (hresult = %8x)\n", cachename, psresult);
	}
	// free the shader data
	vsbin = (DWORD *)Mem_Realloc(tempmempool, vsbin, 0);
	psbin = (DWORD *)Mem_Realloc(tempmempool, psbin, 0);
}

static void R_HLSL_CompilePermutation(r_hlsl_permutation_t *p, unsigned int mode, unsigned int permutation)
{
	int i;
	shadermodeinfo_t *modeinfo = hlslshadermodeinfo + mode;
	int vertstring_length = 0;
	int geomstring_length = 0;
	int fragstring_length = 0;
	char *t;
	char *vertexstring, *geometrystring, *fragmentstring;
	char *vertstring, *geomstring, *fragstring;
	char permutationname[256];
	char cachename[256];
	int vertstrings_count = 0;
	int geomstrings_count = 0;
	int fragstrings_count = 0;
	const char *vertstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];
	const char *geomstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];
	const char *fragstrings_list[32+3+SHADERSTATICPARMS_COUNT+1];

	if (p->compiled)
		return;
	p->compiled = true;
	p->vertexshader = NULL;
	p->pixelshader = NULL;

	permutationname[0] = 0;
	cachename[0] = 0;
	vertexstring   = R_HLSL_GetText(modeinfo->vertexfilename, true);
	geometrystring = R_HLSL_GetText(modeinfo->geometryfilename, false);
	fragmentstring = R_HLSL_GetText(modeinfo->fragmentfilename, false);

	strlcat(permutationname, modeinfo->vertexfilename, sizeof(permutationname));
	strlcat(cachename, "hlsl/", sizeof(cachename));

	// define HLSL so that the shader can tell apart the HLSL compiler and the Cg compiler
	vertstrings_count = 0;
	geomstrings_count = 0;
	fragstrings_count = 0;
	vertstrings_list[vertstrings_count++] = "#define HLSL\n";
	geomstrings_list[geomstrings_count++] = "#define HLSL\n";
	fragstrings_list[fragstrings_count++] = "#define HLSL\n";

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

	// add static parms
	R_CompileShader_AddStaticParms(mode, permutation);
	memcpy(vertstrings_list + vertstrings_count, shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	vertstrings_count += shaderstaticparms_count;
	memcpy(geomstrings_list + geomstrings_count, shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	geomstrings_count += shaderstaticparms_count;
	memcpy(fragstrings_list + fragstrings_count, shaderstaticparmstrings_list, sizeof(*vertstrings_list) * shaderstaticparms_count);
	fragstrings_count += shaderstaticparms_count;

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
	vertstring = t = (char *)Mem_Alloc(tempmempool, vertstring_length + 1);
	for (i = 0;i < vertstrings_count;t += strlen(vertstrings_list[i]), i++)
		memcpy(t, vertstrings_list[i], strlen(vertstrings_list[i]));

	geomstring_length = 0;
	for (i = 0;i < geomstrings_count;i++)
		geomstring_length += strlen(geomstrings_list[i]);
	geomstring = t = (char *)Mem_Alloc(tempmempool, geomstring_length + 1);
	for (i = 0;i < geomstrings_count;t += strlen(geomstrings_list[i]), i++)
		memcpy(t, geomstrings_list[i], strlen(geomstrings_list[i]));

	fragstring_length = 0;
	for (i = 0;i < fragstrings_count;i++)
		fragstring_length += strlen(fragstrings_list[i]);
	fragstring = t = (char *)Mem_Alloc(tempmempool, fragstring_length + 1);
	for (i = 0;i < fragstrings_count;t += strlen(fragstrings_list[i]), i++)
		memcpy(t, fragstrings_list[i], strlen(fragstrings_list[i]));

	// try to load the cached shader, or generate one
	R_HLSL_CacheShader(p, cachename, vertstring, fragstring);

	if ((p->vertexshader || !vertstring[0]) && (p->pixelshader || !fragstring[0]))
		Con_DPrintf("^5HLSL shader %s compiled.\n", permutationname);
	else
		Con_Printf("^1HLSL shader %s failed!  some features may not work properly.\n", permutationname);

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

static inline void hlslVSSetParameter16f(D3DVSREGISTER_t r, const float *a) {IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, a, 4);}
static inline void hlslVSSetParameter4fv(D3DVSREGISTER_t r, const float *a) {IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, a, 1);}
static inline void hlslVSSetParameter4f(D3DVSREGISTER_t r, float x, float y, float z, float w) {float temp[4];Vector4Set(temp, x, y, z, w);IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslVSSetParameter3f(D3DVSREGISTER_t r, float x, float y, float z) {float temp[4];Vector4Set(temp, x, y, z, 0);IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslVSSetParameter2f(D3DVSREGISTER_t r, float x, float y) {float temp[4];Vector4Set(temp, x, y, 0, 0);IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslVSSetParameter1f(D3DVSREGISTER_t r, float x) {float temp[4];Vector4Set(temp, x, 0, 0, 0);IDirect3DDevice9_SetVertexShaderConstantF(vid_d3d9dev, r, temp, 1);}

static inline void hlslPSSetParameter16f(D3DPSREGISTER_t r, const float *a) {IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, a, 4);}
static inline void hlslPSSetParameter4fv(D3DPSREGISTER_t r, const float *a) {IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, a, 1);}
static inline void hlslPSSetParameter4f(D3DPSREGISTER_t r, float x, float y, float z, float w) {float temp[4];Vector4Set(temp, x, y, z, w);IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslPSSetParameter3f(D3DPSREGISTER_t r, float x, float y, float z) {float temp[4];Vector4Set(temp, x, y, z, 0);IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslPSSetParameter2f(D3DPSREGISTER_t r, float x, float y) {float temp[4];Vector4Set(temp, x, y, 0, 0);IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, temp, 1);}
static inline void hlslPSSetParameter1f(D3DPSREGISTER_t r, float x) {float temp[4];Vector4Set(temp, x, 0, 0, 0);IDirect3DDevice9_SetPixelShaderConstantF(vid_d3d9dev, r, temp, 1);}

void R_SetupShader_SetPermutationHLSL(unsigned int mode, unsigned int permutation)
{
	r_hlsl_permutation_t *perm = R_HLSL_FindPermutation(mode, permutation);
	if (r_hlsl_permutation != perm)
	{
		r_hlsl_permutation = perm;
		if (!r_hlsl_permutation->vertexshader && !r_hlsl_permutation->pixelshader)
		{
			if (!r_hlsl_permutation->compiled)
				R_HLSL_CompilePermutation(perm, mode, permutation);
			if (!r_hlsl_permutation->vertexshader && !r_hlsl_permutation->pixelshader)
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
					r_hlsl_permutation = R_HLSL_FindPermutation(mode, permutation);
					if (!r_hlsl_permutation->compiled)
						R_HLSL_CompilePermutation(perm, mode, permutation);
					if (r_hlsl_permutation->vertexshader || r_hlsl_permutation->pixelshader)
						break;
				}
				if (i >= SHADERPERMUTATION_COUNT)
				{
					//Con_Printf("Could not find a working HLSL shader for permutation %s %s\n", shadermodeinfo[mode].vertexfilename, shadermodeinfo[mode].pretext);
					r_hlsl_permutation = R_HLSL_FindPermutation(mode, permutation);
					return; // no bit left to clear, entire mode is broken
				}
			}
		}
		IDirect3DDevice9_SetVertexShader(vid_d3d9dev, r_hlsl_permutation->vertexshader);
		IDirect3DDevice9_SetPixelShader(vid_d3d9dev, r_hlsl_permutation->pixelshader);
	}
	hlslVSSetParameter16f(D3DVSREGISTER_ModelViewProjectionMatrix, gl_modelviewprojection16f);
	hlslVSSetParameter16f(D3DVSREGISTER_ModelViewMatrix, gl_modelview16f);
	hlslPSSetParameter1f(D3DPSREGISTER_ClientTime, cl.time);
}
#endif

void R_SetupShader_SetPermutationSoft(unsigned int mode, unsigned int permutation)
{
	DPSOFTRAST_SetShader(mode, permutation, r_shadow_glossexact.integer);
	DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1, 1, false, gl_modelviewprojection16f);
	DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelViewMatrixM1, 1, false, gl_modelview16f);
	DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_ClientTime, cl.time);
}

void R_GLSL_Restart_f(void)
{
	unsigned int i, limit;
	if (glslshaderstring && glslshaderstring != builtinshaderstring)
		Mem_Free(glslshaderstring);
	glslshaderstring = NULL;
	if (hlslshaderstring && hlslshaderstring != builtinhlslshaderstring)
		Mem_Free(hlslshaderstring);
	hlslshaderstring = NULL;
	switch(vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		{
			r_hlsl_permutation_t *p;
			r_hlsl_permutation = NULL;
			limit = Mem_ExpandableArray_IndexRange(&r_hlsl_permutationarray);
			for (i = 0;i < limit;i++)
			{
				if ((p = (r_hlsl_permutation_t*)Mem_ExpandableArray_RecordAtIndex(&r_hlsl_permutationarray, i)))
				{
					if (p->vertexshader)
						IDirect3DVertexShader9_Release(p->vertexshader);
					if (p->pixelshader)
						IDirect3DPixelShader9_Release(p->pixelshader);
					Mem_ExpandableArray_FreeRecord(&r_hlsl_permutationarray, (void*)p);
				}
			}
			memset(r_hlsl_permutationhash, 0, sizeof(r_hlsl_permutationhash));
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
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
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		break;
	case RENDERPATH_SOFT:
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

	file = FS_OpenRealFile("hlsl/default.hlsl", "w", false);
	if (file)
	{
		FS_Print(file, "/* The engine may define the following macros:\n");
		FS_Print(file, "#define VERTEX_SHADER\n#define GEOMETRY_SHADER\n#define FRAGMENT_SHADER\n");
		for (i = 0;i < SHADERMODE_COUNT;i++)
			FS_Print(file, hlslshadermodeinfo[i].pretext);
		for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
			FS_Print(file, shaderpermutationinfo[i].pretext);
		FS_Print(file, "*/\n");
		FS_Print(file, builtinhlslshaderstring);
		FS_Close(file);
		Con_Printf("hlsl/default.hlsl written\n");
	}
	else
		Con_Printf("failed to write to hlsl/default.hlsl\n");
}

void R_SetupShader_Generic(rtexture_t *first, rtexture_t *second, int texturemode, int rgbscale, qboolean notrippy)
{
	unsigned int permutation = 0;
	if (r_trippy.integer && !notrippy)
		permutation |= SHADERPERMUTATION_TRIPPY;
	permutation |= SHADERPERMUTATION_VIEWTINT;
	if (first)
		permutation |= SHADERPERMUTATION_DIFFUSE;
	if (second)
		permutation |= SHADERPERMUTATION_SPECULAR;
	if (texturemode == GL_MODULATE)
		permutation |= SHADERPERMUTATION_COLORMAPPING;
	else if (texturemode == GL_ADD)
		permutation |= SHADERPERMUTATION_GLOW;
	else if (texturemode == GL_DECAL)
		permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
	if (!second)
		texturemode = GL_MODULATE;
	if (vid.allowalphatocoverage)
		GL_AlphaToCoverage(false);
	switch (vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		R_SetupShader_SetPermutationHLSL(SHADERMODE_GENERIC, permutation);
		R_Mesh_TexBind(GL20TU_FIRST , first );
		R_Mesh_TexBind(GL20TU_SECOND, second);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_GENERIC, permutation);
		R_Mesh_TexBind(r_glsl_permutation->tex_Texture_First , first );
		R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Second, second);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		R_Mesh_TexBind(0, first );
		R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
		R_Mesh_TexBind(1, second);
		if (second)
			R_Mesh_TexCombine(1, texturemode, texturemode, rgbscale, 1);
		break;
	case RENDERPATH_GL11:
		R_Mesh_TexBind(0, first );
		break;
	case RENDERPATH_SOFT:
		R_SetupShader_SetPermutationSoft(SHADERMODE_GENERIC, permutation);
		R_Mesh_TexBind(GL20TU_FIRST , first );
		R_Mesh_TexBind(GL20TU_SECOND, second);
		break;
	}
}

void R_SetupShader_DepthOrShadow(qboolean notrippy)
{
	unsigned int permutation = 0;
	if (r_trippy.integer && !notrippy)
		permutation |= SHADERPERMUTATION_TRIPPY;
	if (vid.allowalphatocoverage)
		GL_AlphaToCoverage(false);
	switch (vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		R_SetupShader_SetPermutationHLSL(SHADERMODE_DEPTH_OR_SHADOW, permutation);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_DEPTH_OR_SHADOW, permutation);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		R_Mesh_TexBind(0, 0);
		R_Mesh_TexBind(1, 0);
		break;
	case RENDERPATH_GL11:
		R_Mesh_TexBind(0, 0);
		break;
	case RENDERPATH_SOFT:
		R_SetupShader_SetPermutationSoft(SHADERMODE_DEPTH_OR_SHADOW, permutation);
		break;
	}
}

void R_SetupShader_ShowDepth(qboolean notrippy)
{
	int permutation = 0;
	if (r_trippy.integer && !notrippy)
		permutation |= SHADERPERMUTATION_TRIPPY;
	if (r_trippy.integer)
		permutation |= SHADERPERMUTATION_TRIPPY;
	if (vid.allowalphatocoverage)
		GL_AlphaToCoverage(false);
	switch (vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTHLSL
		R_SetupShader_SetPermutationHLSL(SHADERMODE_SHOWDEPTH, permutation);
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		R_SetupShader_SetPermutationGLSL(SHADERMODE_SHOWDEPTH, permutation);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		break;
	case RENDERPATH_GL11:
		break;
	case RENDERPATH_SOFT:
		R_SetupShader_SetPermutationSoft(SHADERMODE_SHOWDEPTH, permutation);
		break;
	}
}

extern qboolean r_shadow_usingdeferredprepass;
extern cvar_t r_shadow_deferred_8bitrange;
extern rtexture_t *r_shadow_attenuationgradienttexture;
extern rtexture_t *r_shadow_attenuation2dtexture;
extern rtexture_t *r_shadow_attenuation3dtexture;
extern qboolean r_shadow_usingshadowmap2d;
extern qboolean r_shadow_usingshadowmaportho;
extern float r_shadow_shadowmap_texturescale[2];
extern float r_shadow_shadowmap_parameters[4];
extern qboolean r_shadow_shadowmapvsdct;
extern qboolean r_shadow_shadowmapsampler;
extern int r_shadow_shadowmappcf;
extern rtexture_t *r_shadow_shadowmap2dtexture;
extern rtexture_t *r_shadow_shadowmap2dcolortexture;
extern rtexture_t *r_shadow_shadowmapvsdcttexture;
extern matrix4x4_t r_shadow_shadowmapmatrix;
extern int r_shadow_shadowmaplod; // changes for each light based on distance
extern int r_shadow_prepass_width;
extern int r_shadow_prepass_height;
extern rtexture_t *r_shadow_prepassgeometrydepthtexture;
extern rtexture_t *r_shadow_prepassgeometrynormalmaptexture;
extern rtexture_t *r_shadow_prepassgeometrydepthcolortexture;
extern rtexture_t *r_shadow_prepasslightingdiffusetexture;
extern rtexture_t *r_shadow_prepasslightingspeculartexture;

#define BLENDFUNC_ALLOWS_COLORMOD      1
#define BLENDFUNC_ALLOWS_FOG           2
#define BLENDFUNC_ALLOWS_FOG_HACK0     4
#define BLENDFUNC_ALLOWS_FOG_HACKALPHA 8
#define BLENDFUNC_ALLOWS_ANYFOG        (BLENDFUNC_ALLOWS_FOG | BLENDFUNC_ALLOWS_FOG_HACK0 | BLENDFUNC_ALLOWS_FOG_HACKALPHA)
static int R_BlendFuncFlags(int src, int dst)
{
	int r = 0;

	// a blendfunc allows colormod if:
	// a) it can never keep the destination pixel invariant, or
	// b) it can keep the destination pixel invariant, and still can do so if colormodded
	// this is to prevent unintended side effects from colormod

	// a blendfunc allows fog if:
	// blend(fog(src), fog(dst)) == fog(blend(src, dst))
	// this is to prevent unintended side effects from fog

	// these checks are the output of fogeval.pl

	r |= BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_DST_ALPHA && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG_HACK0;
	if(src == GL_DST_ALPHA && dst == GL_ONE_MINUS_DST_ALPHA) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_DST_COLOR && dst == GL_ONE_MINUS_SRC_ALPHA) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_DST_COLOR && dst == GL_ONE_MINUS_SRC_COLOR) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_DST_COLOR && dst == GL_SRC_ALPHA) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_DST_COLOR && dst == GL_SRC_COLOR) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_DST_COLOR && dst == GL_ZERO) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_ONE && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG_HACK0;
	if(src == GL_ONE && dst == GL_ONE_MINUS_SRC_ALPHA) r |= BLENDFUNC_ALLOWS_FOG_HACKALPHA;
	if(src == GL_ONE && dst == GL_ZERO) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ONE_MINUS_DST_ALPHA && dst == GL_DST_ALPHA) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ONE_MINUS_DST_ALPHA && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG_HACK0;
	if(src == GL_ONE_MINUS_DST_COLOR && dst == GL_SRC_COLOR) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ONE_MINUS_SRC_ALPHA && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG_HACK0;
	if(src == GL_ONE_MINUS_SRC_ALPHA && dst == GL_SRC_ALPHA) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ONE_MINUS_SRC_ALPHA && dst == GL_SRC_COLOR) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_ONE_MINUS_SRC_COLOR && dst == GL_SRC_COLOR) r &= ~BLENDFUNC_ALLOWS_COLORMOD;
	if(src == GL_SRC_ALPHA && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG_HACK0;
	if(src == GL_SRC_ALPHA && dst == GL_ONE_MINUS_SRC_ALPHA) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ZERO && dst == GL_ONE) r |= BLENDFUNC_ALLOWS_FOG;
	if(src == GL_ZERO && dst == GL_SRC_COLOR) r &= ~BLENDFUNC_ALLOWS_COLORMOD;

	return r;
}

void R_SetupShader_Surface(const vec3_t lightcolorbase, qboolean modellighting, float ambientscale, float diffusescale, float specularscale, rsurfacepass_t rsurfacepass, int texturenumsurfaces, const msurface_t **texturesurfacelist, void *surfacewaterplane, qboolean notrippy)
{
	// select a permutation of the lighting shader appropriate to this
	// combination of texture, entity, light source, and fogging, only use the
	// minimum features necessary to avoid wasting rendering time in the
	// fragment shader on features that are not being used
	unsigned int permutation = 0;
	unsigned int mode = 0;
	int blendfuncflags;
	static float dummy_colormod[3] = {1, 1, 1};
	float *colormod = rsurface.colormod;
	float m16f[16];
	matrix4x4_t tempmatrix;
	r_waterstate_waterplane_t *waterplane = (r_waterstate_waterplane_t *)surfacewaterplane;
	if (r_trippy.integer && !notrippy)
		permutation |= SHADERPERMUTATION_TRIPPY;
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
		permutation |= SHADERPERMUTATION_ALPHAKILL;
	if (rsurface.texture->r_water_waterscroll[0] && rsurface.texture->r_water_waterscroll[1])
		permutation |= SHADERPERMUTATION_NORMALMAPSCROLLBLEND; // todo: make generic
	if (rsurfacepass == RSURFPASS_BACKGROUND)
	{
		// distorted background
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_WATERSHADER)
		{
			mode = SHADERMODE_WATER;
			if((r_wateralpha.value < 1) && (rsurface.texture->currentmaterialflags & MATERIALFLAG_WATERALPHA))
			{
				// this is the right thing to do for wateralpha
				GL_BlendFunc(GL_ONE, GL_ZERO);
				blendfuncflags = R_BlendFuncFlags(GL_ONE, GL_ZERO);
			}
			else
			{
				// this is the right thing to do for entity alpha
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				blendfuncflags = R_BlendFuncFlags(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
		else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFRACTION)
		{
			mode = SHADERMODE_REFRACTION;
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			blendfuncflags = R_BlendFuncFlags(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			mode = SHADERMODE_GENERIC;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			GL_BlendFunc(GL_ONE, GL_ZERO);
			blendfuncflags = R_BlendFuncFlags(GL_ONE, GL_ZERO);
		}
		if (vid.allowalphatocoverage)
			GL_AlphaToCoverage(false);
	}
	else if (rsurfacepass == RSURFPASS_DEFERREDGEOMETRY)
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// normalmap (deferred prepass), may use alpha test on diffuse
		mode = SHADERMODE_DEFERREDGEOMETRY;
		GL_BlendFunc(GL_ONE, GL_ZERO);
		blendfuncflags = R_BlendFuncFlags(GL_ONE, GL_ZERO);
		if (vid.allowalphatocoverage)
			GL_AlphaToCoverage(false);
	}
	else if (rsurfacepass == RSURFPASS_RTLIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// light source
		mode = SHADERMODE_LIGHTSOURCE;
		if (rsurface.rtlight->currentcubemap != r_texture_whitecube)
			permutation |= SHADERPERMUTATION_CUBEFILTER;
		if (diffusescale > 0)
			permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
			permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		if (r_refdef.fogenabled)
			permutation |= r_texture_fogheighttexture ? SHADERPERMUTATION_FOGHEIGHTTEXTURE : (r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE);
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmap2d)
		{
			permutation |= SHADERPERMUTATION_SHADOWMAP2D;
			if(r_shadow_shadowmapvsdct)
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
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		blendfuncflags = R_BlendFuncFlags(GL_SRC_ALPHA, GL_ONE);
		if (vid.allowalphatocoverage)
			GL_AlphaToCoverage(false);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// unshaded geometry (fullbright or ambient model lighting)
		mode = SHADERMODE_FLATCOLOR;
		ambientscale = diffusescale = specularscale = 0;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_texture_fogheighttexture ? SHADERPERMUTATION_FOGHEIGHTTEXTURE : (r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE);
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
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
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		blendfuncflags = R_BlendFuncFlags(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		// when using alphatocoverage, we don't need alphakill
		if (vid.allowalphatocoverage)
		{
			if (r_transparent_alphatocoverage.integer)
			{
				GL_AlphaToCoverage((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
				permutation &= ~SHADERPERMUTATION_ALPHAKILL;
			}
			else
				GL_AlphaToCoverage(false);
		}
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT_DIRECTIONAL)
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// directional model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
			permutation |= SHADERPERMUTATION_SPECULAR;
		if (r_refdef.fogenabled)
			permutation |= r_texture_fogheighttexture ? SHADERPERMUTATION_FOGHEIGHTTEXTURE : (r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE);
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
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
		if (r_shadow_bouncegridtexture)
		{
			permutation |= SHADERPERMUTATION_BOUNCEGRID;
			if (r_shadow_bouncegriddirectional)
				permutation |= SHADERPERMUTATION_BOUNCEGRIDDIRECTIONAL;
		}
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		blendfuncflags = R_BlendFuncFlags(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		// when using alphatocoverage, we don't need alphakill
		if (vid.allowalphatocoverage)
		{
			if (r_transparent_alphatocoverage.integer)
			{
				GL_AlphaToCoverage((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
				permutation &= ~SHADERPERMUTATION_ALPHAKILL;
			}
			else
				GL_AlphaToCoverage(false);
		}
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// ambient model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_texture_fogheighttexture ? SHADERPERMUTATION_FOGHEIGHTTEXTURE : (r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE);
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
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
		if (r_shadow_bouncegridtexture)
		{
			permutation |= SHADERPERMUTATION_BOUNCEGRID;
			if (r_shadow_bouncegriddirectional)
				permutation |= SHADERPERMUTATION_BOUNCEGRIDDIRECTIONAL;
		}
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		blendfuncflags = R_BlendFuncFlags(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		// when using alphatocoverage, we don't need alphakill
		if (vid.allowalphatocoverage)
		{
			if (r_transparent_alphatocoverage.integer)
			{
				GL_AlphaToCoverage((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
				permutation &= ~SHADERPERMUTATION_ALPHAKILL;
			}
			else
				GL_AlphaToCoverage(false);
		}
	}
	else
	{
		if (r_glsl_offsetmapping.integer)
		{
			switch(rsurface.texture->offsetmapping)
			{
			case OFFSETMAPPING_LINEAR: permutation |= SHADERPERMUTATION_OFFSETMAPPING;break;
			case OFFSETMAPPING_RELIEF: permutation |= SHADERPERMUTATION_OFFSETMAPPING | SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_DEFAULT: permutation |= SHADERPERMUTATION_OFFSETMAPPING;if (r_glsl_offsetmapping_reliefmapping.integer) permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;break;
			case OFFSETMAPPING_OFF: break;
			}
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_VERTEXTEXTUREBLEND)
			permutation |= SHADERPERMUTATION_VERTEXTEXTUREBLEND;
		// lightmapped wall
		if (rsurface.texture->glowtexture && r_hdr_glowintensity.value > 0 && !gl_lightmaps.integer)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= r_texture_fogheighttexture ? SHADERPERMUTATION_FOGHEIGHTTEXTURE : (r_refdef.fogplaneviewabove ? SHADERPERMUTATION_FOGOUTSIDE : SHADERPERMUTATION_FOGINSIDE);
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW))
		{
			permutation |= SHADERPERMUTATION_SHADOWMAPORTHO;
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
		if (FAKELIGHT_ENABLED)
		{
			// fake lightmapping (q1bsp, q3bsp, fullbright map)
			mode = SHADERMODE_FAKELIGHT;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			if (specularscale > 0)
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		}
		else if (r_glsl_deluxemapping.integer >= 1 && rsurface.uselightmaptexture && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brushq3.deluxemapping)
		{
			// deluxemapping (light direction texture)
			if (rsurface.uselightmaptexture && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brushq3.deluxemapping && r_refdef.scene.worldmodel->brushq3.deluxemapping_modelspace)
				mode = SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE;
			else
				mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			if (specularscale > 0)
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		}
		else if (r_glsl_deluxemapping.integer >= 2 && rsurface.uselightmaptexture)
		{
			// fake deluxemapping (uniform light direction in tangentspace)
			mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			permutation |= SHADERPERMUTATION_DIFFUSE;
			if (specularscale > 0)
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		}
		else if (rsurface.uselightmaptexture)
		{
			// ordinary lightmapping (q1bsp, q3bsp)
			mode = SHADERMODE_LIGHTMAP;
		}
		else
		{
			// ordinary vertex coloring (q3bsp)
			mode = SHADERMODE_VERTEXCOLOR;
		}
		if (r_shadow_bouncegridtexture)
		{
			permutation |= SHADERPERMUTATION_BOUNCEGRID;
			if (r_shadow_bouncegriddirectional)
				permutation |= SHADERPERMUTATION_BOUNCEGRIDDIRECTIONAL;
		}
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		blendfuncflags = R_BlendFuncFlags(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		// when using alphatocoverage, we don't need alphakill
		if (vid.allowalphatocoverage)
		{
			if (r_transparent_alphatocoverage.integer)
			{
				GL_AlphaToCoverage((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
				permutation &= ~SHADERPERMUTATION_ALPHAKILL;
			}
			else
				GL_AlphaToCoverage(false);
		}
	}
	if(!(blendfuncflags & BLENDFUNC_ALLOWS_COLORMOD))
		colormod = dummy_colormod;
	if(!(blendfuncflags & BLENDFUNC_ALLOWS_ANYFOG))
		permutation &= ~(SHADERPERMUTATION_FOGHEIGHTTEXTURE | SHADERPERMUTATION_FOGOUTSIDE | SHADERPERMUTATION_FOGINSIDE);
	if(blendfuncflags & BLENDFUNC_ALLOWS_FOG_HACKALPHA)
		permutation |= SHADERPERMUTATION_FOGALPHAHACK;
	switch(vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		RSurf_PrepareVerticesForBatch(BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | (rsurface.modellightmapcolor4f ? BATCHNEED_VERTEXMESH_VERTEXCOLOR : 0) | BATCHNEED_VERTEXMESH_TEXCOORD | (rsurface.uselightmaptexture ? BATCHNEED_VERTEXMESH_LIGHTMAP : 0), texturenumsurfaces, texturesurfacelist);
		R_Mesh_PrepareVertices_Mesh(rsurface.batchnumvertices, rsurface.batchvertexmesh, rsurface.batchvertexmeshbuffer);
		R_SetupShader_SetPermutationHLSL(mode, permutation);
		Matrix4x4_ToArrayFloatGL(&rsurface.matrix, m16f);hlslPSSetParameter16f(D3DPSREGISTER_ModelToReflectCube, m16f);
		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			Matrix4x4_ToArrayFloatGL(&rsurface.entitytolight, m16f);hlslVSSetParameter16f(D3DVSREGISTER_ModelToLight, m16f);
			hlslVSSetParameter3f(D3DVSREGISTER_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
		}
		else
		{
			if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				hlslVSSetParameter3f(D3DVSREGISTER_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
			}
		}
		Matrix4x4_ToArrayFloatGL(&rsurface.texture->currenttexmatrix, m16f);hlslVSSetParameter16f(D3DVSREGISTER_TexMatrix, m16f);
		Matrix4x4_ToArrayFloatGL(&rsurface.texture->currentbackgroundtexmatrix, m16f);hlslVSSetParameter16f(D3DVSREGISTER_BackgroundTexMatrix, m16f);
		Matrix4x4_ToArrayFloatGL(&r_shadow_shadowmapmatrix, m16f);hlslVSSetParameter16f(D3DVSREGISTER_ShadowMapMatrix, m16f);
		hlslVSSetParameter3f(D3DVSREGISTER_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);
		hlslVSSetParameter4f(D3DVSREGISTER_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);

		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			hlslPSSetParameter3f(D3DPSREGISTER_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
			hlslPSSetParameter3f(D3DPSREGISTER_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Ambient, colormod[0] * ambientscale, colormod[1] * ambientscale, colormod[2] * ambientscale);
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Diffuse, colormod[0] * diffusescale, colormod[1] * diffusescale, colormod[2] * diffusescale);
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Specular, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale);

			// additive passes are only darkened by fog, not tinted
			hlslPSSetParameter3f(D3DPSREGISTER_FogColor, 0, 0, 0);
			hlslPSSetParameter1f(D3DPSREGISTER_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		}
		else
		{
			if (mode == SHADERMODE_FLATCOLOR)
			{
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Ambient, colormod[0], colormod[1], colormod[2]);
			}
			else if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Ambient, (r_refdef.scene.ambient + rsurface.modellight_ambient[0] * r_refdef.lightmapintensity) * colormod[0], (r_refdef.scene.ambient + rsurface.modellight_ambient[1] * r_refdef.lightmapintensity) * colormod[1], (r_refdef.scene.ambient + rsurface.modellight_ambient[2] * r_refdef.lightmapintensity) * colormod[2]);
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Diffuse, r_refdef.lightmapintensity * colormod[0], r_refdef.lightmapintensity * colormod[1], r_refdef.lightmapintensity * colormod[2]);
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				hlslPSSetParameter3f(D3DPSREGISTER_DeferredMod_Diffuse, colormod[0] * r_shadow_deferred_8bitrange.value, colormod[1] * r_shadow_deferred_8bitrange.value, colormod[2] * r_shadow_deferred_8bitrange.value);
				hlslPSSetParameter3f(D3DPSREGISTER_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
				hlslPSSetParameter3f(D3DPSREGISTER_LightColor, rsurface.modellight_diffuse[0], rsurface.modellight_diffuse[1], rsurface.modellight_diffuse[2]);
				hlslPSSetParameter3f(D3DPSREGISTER_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
			}
			else
			{
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Ambient, r_refdef.scene.ambient * colormod[0], r_refdef.scene.ambient * colormod[1], r_refdef.scene.ambient * colormod[2]);
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Diffuse, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2]);
				hlslPSSetParameter3f(D3DPSREGISTER_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				hlslPSSetParameter3f(D3DPSREGISTER_DeferredMod_Diffuse, colormod[0] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[1] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[2] * diffusescale * r_shadow_deferred_8bitrange.value);
				hlslPSSetParameter3f(D3DPSREGISTER_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
			}
			// additive passes are only darkened by fog, not tinted
			if(blendfuncflags & BLENDFUNC_ALLOWS_FOG_HACK0)
				hlslPSSetParameter3f(D3DPSREGISTER_FogColor, 0, 0, 0);
			else
				hlslPSSetParameter3f(D3DPSREGISTER_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
			hlslPSSetParameter4f(D3DPSREGISTER_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);
			hlslPSSetParameter4f(D3DPSREGISTER_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);
			hlslPSSetParameter4f(D3DPSREGISTER_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);
			hlslPSSetParameter4f(D3DPSREGISTER_RefractColor, rsurface.texture->refractcolor4f[0], rsurface.texture->refractcolor4f[1], rsurface.texture->refractcolor4f[2], rsurface.texture->refractcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			hlslPSSetParameter4f(D3DPSREGISTER_ReflectColor, rsurface.texture->reflectcolor4f[0], rsurface.texture->reflectcolor4f[1], rsurface.texture->reflectcolor4f[2], rsurface.texture->reflectcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			hlslPSSetParameter1f(D3DPSREGISTER_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);
			hlslPSSetParameter1f(D3DPSREGISTER_ReflectOffset, rsurface.texture->reflectmin);
			hlslPSSetParameter1f(D3DPSREGISTER_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
			if (mode == SHADERMODE_WATER)
				hlslPSSetParameter2f(D3DPSREGISTER_NormalmapScrollBlend, rsurface.texture->r_water_waterscroll[0], rsurface.texture->r_water_waterscroll[1]);
		}
		hlslPSSetParameter2f(D3DPSREGISTER_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		hlslPSSetParameter4f(D3DPSREGISTER_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);
		hlslPSSetParameter3f(D3DPSREGISTER_Color_Glow, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2]);
		hlslPSSetParameter1f(D3DPSREGISTER_Alpha, rsurface.texture->lightmapcolor[3] * ((rsurface.texture->basematerialflags & MATERIALFLAG_WATERSHADER && r_waterstate.enabled && !r_refdef.view.isoverlay) ? rsurface.texture->r_water_wateralpha : 1));
		hlslPSSetParameter3f(D3DPSREGISTER_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);
		if (rsurface.texture->pantstexture)
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
		else
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Pants, 0, 0, 0);
		if (rsurface.texture->shirttexture)
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
		else
			hlslPSSetParameter3f(D3DPSREGISTER_Color_Shirt, 0, 0, 0);
		hlslPSSetParameter4f(D3DPSREGISTER_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);
		hlslPSSetParameter1f(D3DPSREGISTER_FogPlaneViewDist, rsurface.fogplaneviewdist);
		hlslPSSetParameter1f(D3DPSREGISTER_FogRangeRecip, rsurface.fograngerecip);
		hlslPSSetParameter1f(D3DPSREGISTER_FogHeightFade, rsurface.fogheightfade);
		hlslPSSetParameter4f(D3DPSREGISTER_OffsetMapping_ScaleSteps,
				r_glsl_offsetmapping_scale.value*rsurface.texture->offsetscale,
				max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				1.0 / max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				max(1, r_glsl_offsetmapping_reliefmapping_refinesteps.integer)
			);
		hlslPSSetParameter2f(D3DPSREGISTER_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		hlslPSSetParameter2f(D3DPSREGISTER_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);

		R_Mesh_TexBind(GL20TU_NORMAL            , rsurface.texture->nmaptexture                       );
		R_Mesh_TexBind(GL20TU_COLOR             , rsurface.texture->basetexture                       );
		R_Mesh_TexBind(GL20TU_GLOSS             , rsurface.texture->glosstexture                      );
		R_Mesh_TexBind(GL20TU_GLOW              , rsurface.texture->glowtexture                       );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_NORMAL  , rsurface.texture->backgroundnmaptexture             );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_COLOR   , rsurface.texture->backgroundbasetexture             );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_GLOSS   , rsurface.texture->backgroundglosstexture            );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_GLOW    , rsurface.texture->backgroundglowtexture             );
		if (permutation & SHADERPERMUTATION_COLORMAPPING) R_Mesh_TexBind(GL20TU_PANTS             , rsurface.texture->pantstexture                      );
		if (permutation & SHADERPERMUTATION_COLORMAPPING) R_Mesh_TexBind(GL20TU_SHIRT             , rsurface.texture->shirttexture                      );
		if (permutation & SHADERPERMUTATION_REFLECTCUBE) R_Mesh_TexBind(GL20TU_REFLECTMASK       , rsurface.texture->reflectmasktexture                );
		if (permutation & SHADERPERMUTATION_REFLECTCUBE) R_Mesh_TexBind(GL20TU_REFLECTCUBE       , rsurface.texture->reflectcubetexture ? rsurface.texture->reflectcubetexture : r_texture_whitecube);
		if (permutation & SHADERPERMUTATION_FOGHEIGHTTEXTURE) R_Mesh_TexBind(GL20TU_FOGHEIGHTTEXTURE  , r_texture_fogheighttexture                          );
		if (permutation & (SHADERPERMUTATION_FOGINSIDE | SHADERPERMUTATION_FOGOUTSIDE)) R_Mesh_TexBind(GL20TU_FOGMASK           , r_texture_fogattenuation                            );
		R_Mesh_TexBind(GL20TU_LIGHTMAP          , rsurface.lightmaptexture ? rsurface.lightmaptexture : r_texture_white);
		R_Mesh_TexBind(GL20TU_DELUXEMAP         , rsurface.deluxemaptexture ? rsurface.deluxemaptexture : r_texture_blanknormalmap);
		if (rsurface.rtlight                                  ) R_Mesh_TexBind(GL20TU_ATTENUATION       , r_shadow_attenuationgradienttexture                 );
		if (rsurfacepass == RSURFPASS_BACKGROUND)
		{
			R_Mesh_TexBind(GL20TU_REFRACTION        , waterplane->texture_refraction ? waterplane->texture_refraction : r_texture_black);
			if(mode == SHADERMODE_GENERIC) R_Mesh_TexBind(GL20TU_FIRST             , waterplane->texture_camera ? waterplane->texture_camera : r_texture_black);
			R_Mesh_TexBind(GL20TU_REFLECTION        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
		else
		{
			if (permutation & SHADERPERMUTATION_REFLECTION        ) R_Mesh_TexBind(GL20TU_REFLECTION        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
//		if (rsurfacepass == RSURFPASS_DEFERREDLIGHT           ) R_Mesh_TexBind(GL20TU_SCREENDEPTH       , r_shadow_prepassgeometrydepthtexture                );
//		if (rsurfacepass == RSURFPASS_DEFERREDLIGHT           ) R_Mesh_TexBind(GL20TU_SCREENNORMALMAP   , r_shadow_prepassgeometrynormalmaptexture            );
		if (permutation & SHADERPERMUTATION_DEFERREDLIGHTMAP  ) R_Mesh_TexBind(GL20TU_SCREENDIFFUSE     , r_shadow_prepasslightingdiffusetexture              );
		if (permutation & SHADERPERMUTATION_DEFERREDLIGHTMAP  ) R_Mesh_TexBind(GL20TU_SCREENSPECULAR    , r_shadow_prepasslightingspeculartexture             );
		if (rsurface.rtlight || (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW)))
		{
			R_Mesh_TexBind(GL20TU_SHADOWMAP2D, r_shadow_shadowmap2dcolortexture);
			if (rsurface.rtlight)
			{
				if (permutation & SHADERPERMUTATION_CUBEFILTER        ) R_Mesh_TexBind(GL20TU_CUBE              , rsurface.rtlight->currentcubemap                    );
				if (permutation & SHADERPERMUTATION_SHADOWMAPVSDCT    ) R_Mesh_TexBind(GL20TU_CUBEPROJECTION    , r_shadow_shadowmapvsdcttexture                      );
			}
		}
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (!vid.useinterleavedarrays)
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | (rsurface.modellightmapcolor4f ? BATCHNEED_ARRAY_VERTEXCOLOR : 0) | BATCHNEED_ARRAY_TEXCOORD | (rsurface.uselightmaptexture ? BATCHNEED_ARRAY_LIGHTMAP : 0), texturenumsurfaces, texturesurfacelist);
			R_Mesh_VertexPointer(     3, GL_FLOAT, sizeof(float[3]), rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer, rsurface.batchvertex3f_bufferoffset);
			R_Mesh_ColorPointer(      4, GL_FLOAT, sizeof(float[4]), rsurface.batchlightmapcolor4f, rsurface.batchlightmapcolor4f_vertexbuffer, rsurface.batchlightmapcolor4f_bufferoffset);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			R_Mesh_TexCoordPointer(1, 3, GL_FLOAT, sizeof(float[3]), rsurface.batchsvector3f, rsurface.batchsvector3f_vertexbuffer, rsurface.batchsvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, GL_FLOAT, sizeof(float[3]), rsurface.batchtvector3f, rsurface.batchtvector3f_vertexbuffer, rsurface.batchtvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, GL_FLOAT, sizeof(float[3]), rsurface.batchnormal3f, rsurface.batchnormal3f_vertexbuffer, rsurface.batchnormal3f_bufferoffset);
			R_Mesh_TexCoordPointer(4, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordlightmap2f, rsurface.batchtexcoordlightmap2f_vertexbuffer, rsurface.batchtexcoordlightmap2f_bufferoffset);
		}
		else
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | (rsurface.modellightmapcolor4f ? BATCHNEED_VERTEXMESH_VERTEXCOLOR : 0) | BATCHNEED_VERTEXMESH_TEXCOORD | (rsurface.uselightmaptexture ? BATCHNEED_VERTEXMESH_LIGHTMAP : 0), texturenumsurfaces, texturesurfacelist);
			R_Mesh_PrepareVertices_Mesh(rsurface.batchnumvertices, rsurface.batchvertexmesh, rsurface.batchvertexmeshbuffer);
		}
		R_SetupShader_SetPermutationGLSL(mode, permutation);
		if (r_glsl_permutation->loc_ModelToReflectCube >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.matrix, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_ModelToReflectCube, 1, false, m16f);}
		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			if (r_glsl_permutation->loc_ModelToLight >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.entitytolight, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_ModelToLight, 1, false, m16f);}
			if (r_glsl_permutation->loc_LightPosition >= 0) qglUniform3f(r_glsl_permutation->loc_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
			if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3f(r_glsl_permutation->loc_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
			if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Ambient, colormod[0] * ambientscale, colormod[1] * ambientscale, colormod[2] * ambientscale);
			if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Diffuse, colormod[0] * diffusescale, colormod[1] * diffusescale, colormod[2] * diffusescale);
			if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Specular, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale);
	
			// additive passes are only darkened by fog, not tinted
			if (r_glsl_permutation->loc_FogColor >= 0)
				qglUniform3f(r_glsl_permutation->loc_FogColor, 0, 0, 0);
			if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1f(r_glsl_permutation->loc_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		}
		else
		{
			if (mode == SHADERMODE_FLATCOLOR)
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Ambient, colormod[0], colormod[1], colormod[2]);
			}
			else if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Ambient, (r_refdef.scene.ambient + rsurface.modellight_ambient[0] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[0], (r_refdef.scene.ambient + rsurface.modellight_ambient[1] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[1], (r_refdef.scene.ambient + rsurface.modellight_ambient[2] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[2]);
				if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Diffuse, r_refdef.lightmapintensity * colormod[0], r_refdef.lightmapintensity * colormod[1], r_refdef.lightmapintensity * colormod[2]);
				if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				if (r_glsl_permutation->loc_DeferredMod_Diffuse >= 0) qglUniform3f(r_glsl_permutation->loc_DeferredMod_Diffuse, colormod[0] * r_shadow_deferred_8bitrange.value, colormod[1] * r_shadow_deferred_8bitrange.value, colormod[2] * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_DeferredMod_Specular >= 0) qglUniform3f(r_glsl_permutation->loc_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3f(r_glsl_permutation->loc_LightColor, rsurface.modellight_diffuse[0] * r_refdef.scene.rtlightstylevalue[0], rsurface.modellight_diffuse[1] * r_refdef.scene.rtlightstylevalue[0], rsurface.modellight_diffuse[2] * r_refdef.scene.rtlightstylevalue[0]);
				if (r_glsl_permutation->loc_LightDir >= 0) qglUniform3f(r_glsl_permutation->loc_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
			}
			else
			{
				if (r_glsl_permutation->loc_Color_Ambient >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Ambient, r_refdef.scene.ambient * colormod[0], r_refdef.scene.ambient * colormod[1], r_refdef.scene.ambient * colormod[2]);
				if (r_glsl_permutation->loc_Color_Diffuse >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Diffuse, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2]);
				if (r_glsl_permutation->loc_Color_Specular >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				if (r_glsl_permutation->loc_DeferredMod_Diffuse >= 0) qglUniform3f(r_glsl_permutation->loc_DeferredMod_Diffuse, colormod[0] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[1] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[2] * diffusescale * r_shadow_deferred_8bitrange.value);
				if (r_glsl_permutation->loc_DeferredMod_Specular >= 0) qglUniform3f(r_glsl_permutation->loc_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
			}
			// additive passes are only darkened by fog, not tinted
			if (r_glsl_permutation->loc_FogColor >= 0)
			{
				if(blendfuncflags & BLENDFUNC_ALLOWS_FOG_HACK0)
					qglUniform3f(r_glsl_permutation->loc_FogColor, 0, 0, 0);
				else
					qglUniform3f(r_glsl_permutation->loc_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
			}
			if (r_glsl_permutation->loc_DistortScaleRefractReflect >= 0) qglUniform4f(r_glsl_permutation->loc_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);
			if (r_glsl_permutation->loc_ScreenScaleRefractReflect >= 0) qglUniform4f(r_glsl_permutation->loc_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);
			if (r_glsl_permutation->loc_ScreenCenterRefractReflect >= 0) qglUniform4f(r_glsl_permutation->loc_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);
			if (r_glsl_permutation->loc_RefractColor >= 0) qglUniform4f(r_glsl_permutation->loc_RefractColor, rsurface.texture->refractcolor4f[0], rsurface.texture->refractcolor4f[1], rsurface.texture->refractcolor4f[2], rsurface.texture->refractcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			if (r_glsl_permutation->loc_ReflectColor >= 0) qglUniform4f(r_glsl_permutation->loc_ReflectColor, rsurface.texture->reflectcolor4f[0], rsurface.texture->reflectcolor4f[1], rsurface.texture->reflectcolor4f[2], rsurface.texture->reflectcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			if (r_glsl_permutation->loc_ReflectFactor >= 0) qglUniform1f(r_glsl_permutation->loc_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);
			if (r_glsl_permutation->loc_ReflectOffset >= 0) qglUniform1f(r_glsl_permutation->loc_ReflectOffset, rsurface.texture->reflectmin);
			if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1f(r_glsl_permutation->loc_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
			if (r_glsl_permutation->loc_NormalmapScrollBlend >= 0) qglUniform2f(r_glsl_permutation->loc_NormalmapScrollBlend, rsurface.texture->r_water_waterscroll[0], rsurface.texture->r_water_waterscroll[1]);
		}
		if (r_glsl_permutation->loc_TexMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currenttexmatrix, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_TexMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_BackgroundTexMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&rsurface.texture->currentbackgroundtexmatrix, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_BackgroundTexMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_ShadowMapMatrix >= 0) {Matrix4x4_ToArrayFloatGL(&r_shadow_shadowmapmatrix, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_ShadowMapMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_ShadowMap_TextureScale >= 0) qglUniform2f(r_glsl_permutation->loc_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		if (r_glsl_permutation->loc_ShadowMap_Parameters >= 0) qglUniform4f(r_glsl_permutation->loc_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);

		if (r_glsl_permutation->loc_Color_Glow >= 0) qglUniform3f(r_glsl_permutation->loc_Color_Glow, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2]);
		if (r_glsl_permutation->loc_Alpha >= 0) qglUniform1f(r_glsl_permutation->loc_Alpha, rsurface.texture->lightmapcolor[3] * ((rsurface.texture->basematerialflags & MATERIALFLAG_WATERSHADER && r_waterstate.enabled && !r_refdef.view.isoverlay) ? rsurface.texture->r_water_wateralpha : 1));
		if (r_glsl_permutation->loc_EyePosition >= 0) qglUniform3f(r_glsl_permutation->loc_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);
		if (r_glsl_permutation->loc_Color_Pants >= 0)
		{
			if (rsurface.texture->pantstexture)
				qglUniform3f(r_glsl_permutation->loc_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
			else
				qglUniform3f(r_glsl_permutation->loc_Color_Pants, 0, 0, 0);
		}
		if (r_glsl_permutation->loc_Color_Shirt >= 0)
		{
			if (rsurface.texture->shirttexture)
				qglUniform3f(r_glsl_permutation->loc_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
			else
				qglUniform3f(r_glsl_permutation->loc_Color_Shirt, 0, 0, 0);
		}
		if (r_glsl_permutation->loc_FogPlane >= 0) qglUniform4f(r_glsl_permutation->loc_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);
		if (r_glsl_permutation->loc_FogPlaneViewDist >= 0) qglUniform1f(r_glsl_permutation->loc_FogPlaneViewDist, rsurface.fogplaneviewdist);
		if (r_glsl_permutation->loc_FogRangeRecip >= 0) qglUniform1f(r_glsl_permutation->loc_FogRangeRecip, rsurface.fograngerecip);
		if (r_glsl_permutation->loc_FogHeightFade >= 0) qglUniform1f(r_glsl_permutation->loc_FogHeightFade, rsurface.fogheightfade);
		if (r_glsl_permutation->loc_OffsetMapping_ScaleSteps >= 0) qglUniform4f(r_glsl_permutation->loc_OffsetMapping_ScaleSteps,
				r_glsl_offsetmapping_scale.value*rsurface.texture->offsetscale,
				max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				1.0 / max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				max(1, r_glsl_offsetmapping_reliefmapping_refinesteps.integer)
			);
		if (r_glsl_permutation->loc_ScreenToDepth >= 0) qglUniform2f(r_glsl_permutation->loc_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		if (r_glsl_permutation->loc_PixelToScreenTexCoord >= 0) qglUniform2f(r_glsl_permutation->loc_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);
		if (r_glsl_permutation->loc_BounceGridMatrix >= 0) {Matrix4x4_Concat(&tempmatrix, &r_shadow_bouncegridmatrix, &rsurface.matrix);Matrix4x4_ToArrayFloatGL(&tempmatrix, m16f);qglUniformMatrix4fv(r_glsl_permutation->loc_BounceGridMatrix, 1, false, m16f);}
		if (r_glsl_permutation->loc_BounceGridIntensity >= 0) qglUniform1f(r_glsl_permutation->loc_BounceGridIntensity, r_shadow_bouncegridintensity*r_refdef.view.colorscale);

		if (r_glsl_permutation->tex_Texture_First           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_First            , r_texture_white                                     );
		if (r_glsl_permutation->tex_Texture_Second          >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Second           , r_texture_white                                     );
		if (r_glsl_permutation->tex_Texture_GammaRamps      >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_GammaRamps       , r_texture_gammaramps                                );
		if (r_glsl_permutation->tex_Texture_Normal          >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Normal           , rsurface.texture->nmaptexture                       );
		if (r_glsl_permutation->tex_Texture_Color           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Color            , rsurface.texture->basetexture                       );
		if (r_glsl_permutation->tex_Texture_Gloss           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Gloss            , rsurface.texture->glosstexture                      );
		if (r_glsl_permutation->tex_Texture_Glow            >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Glow             , rsurface.texture->glowtexture                       );
		if (r_glsl_permutation->tex_Texture_SecondaryNormal >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_SecondaryNormal  , rsurface.texture->backgroundnmaptexture             );
		if (r_glsl_permutation->tex_Texture_SecondaryColor  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_SecondaryColor   , rsurface.texture->backgroundbasetexture             );
		if (r_glsl_permutation->tex_Texture_SecondaryGloss  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_SecondaryGloss   , rsurface.texture->backgroundglosstexture            );
		if (r_glsl_permutation->tex_Texture_SecondaryGlow   >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_SecondaryGlow    , rsurface.texture->backgroundglowtexture             );
		if (r_glsl_permutation->tex_Texture_Pants           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Pants            , rsurface.texture->pantstexture                      );
		if (r_glsl_permutation->tex_Texture_Shirt           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Shirt            , rsurface.texture->shirttexture                      );
		if (r_glsl_permutation->tex_Texture_ReflectMask     >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ReflectMask      , rsurface.texture->reflectmasktexture                );
		if (r_glsl_permutation->tex_Texture_ReflectCube     >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ReflectCube      , rsurface.texture->reflectcubetexture ? rsurface.texture->reflectcubetexture : r_texture_whitecube);
		if (r_glsl_permutation->tex_Texture_FogHeightTexture>= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_FogHeightTexture , r_texture_fogheighttexture                          );
		if (r_glsl_permutation->tex_Texture_FogMask         >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_FogMask          , r_texture_fogattenuation                            );
		if (r_glsl_permutation->tex_Texture_Lightmap        >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Lightmap         , rsurface.lightmaptexture ? rsurface.lightmaptexture : r_texture_white);
		if (r_glsl_permutation->tex_Texture_Deluxemap       >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Deluxemap        , rsurface.deluxemaptexture ? rsurface.deluxemaptexture : r_texture_blanknormalmap);
		if (r_glsl_permutation->tex_Texture_Attenuation     >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Attenuation      , r_shadow_attenuationgradienttexture                 );
		if (rsurfacepass == RSURFPASS_BACKGROUND)
		{
			if (r_glsl_permutation->tex_Texture_Refraction  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Refraction        , waterplane->texture_refraction ? waterplane->texture_refraction : r_texture_black);
			if (r_glsl_permutation->tex_Texture_First       >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_First             , waterplane->texture_camera ? waterplane->texture_camera : r_texture_black);
			if (r_glsl_permutation->tex_Texture_Reflection  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Reflection        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
		else
		{
			if (r_glsl_permutation->tex_Texture_Reflection >= 0 && waterplane) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Reflection        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
		if (r_glsl_permutation->tex_Texture_ScreenDepth     >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenDepth       , r_shadow_prepassgeometrydepthtexture                );
		if (r_glsl_permutation->tex_Texture_ScreenNormalMap >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenNormalMap   , r_shadow_prepassgeometrynormalmaptexture            );
		if (r_glsl_permutation->tex_Texture_ScreenDiffuse   >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenDiffuse     , r_shadow_prepasslightingdiffusetexture              );
		if (r_glsl_permutation->tex_Texture_ScreenSpecular  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenSpecular    , r_shadow_prepasslightingspeculartexture             );
		if (rsurface.rtlight || (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW)))
		{
			if (r_glsl_permutation->tex_Texture_ShadowMap2D     >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ShadowMap2D, r_shadow_shadowmap2dtexture                         );
			if (rsurface.rtlight)
			{
				if (r_glsl_permutation->tex_Texture_Cube            >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Cube              , rsurface.rtlight->currentcubemap                    );
				if (r_glsl_permutation->tex_Texture_CubeProjection  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_CubeProjection    , r_shadow_shadowmapvsdcttexture                      );
			}
		}
		if (r_glsl_permutation->tex_Texture_BounceGrid  >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_BounceGrid, r_shadow_bouncegridtexture);
		CHECKGLERROR
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		break;
	case RENDERPATH_SOFT:
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | (rsurface.modellightmapcolor4f ? BATCHNEED_ARRAY_VERTEXCOLOR : 0) | BATCHNEED_ARRAY_TEXCOORD | (rsurface.uselightmaptexture ? BATCHNEED_ARRAY_LIGHTMAP : 0), texturenumsurfaces, texturesurfacelist);
		R_Mesh_PrepareVertices_Mesh_Arrays(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchsvector3f, rsurface.batchtvector3f, rsurface.batchnormal3f, rsurface.batchlightmapcolor4f, rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordlightmap2f);
		R_SetupShader_SetPermutationSoft(mode, permutation);
		{Matrix4x4_ToArrayFloatGL(&rsurface.matrix, m16f);DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelToReflectCubeM1, 1, false, m16f);}
		if (mode == SHADERMODE_LIGHTSOURCE)
		{
			{Matrix4x4_ToArrayFloatGL(&rsurface.entitytolight, m16f);DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelToLightM1, 1, false, m16f);}
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Ambient, colormod[0] * ambientscale, colormod[1] * ambientscale, colormod[2] * ambientscale);
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Diffuse, colormod[0] * diffusescale, colormod[1] * diffusescale, colormod[2] * diffusescale);
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Specular, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale, r_refdef.view.colorscale * specularscale);
	
			// additive passes are only darkened by fog, not tinted
			DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_FogColor, 0, 0, 0);
			DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		}
		else
		{
			if (mode == SHADERMODE_FLATCOLOR)
			{
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Ambient, colormod[0], colormod[1], colormod[2]);
			}
			else if (mode == SHADERMODE_LIGHTDIRECTION)
			{
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Ambient, (r_refdef.scene.ambient + rsurface.modellight_ambient[0] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[0], (r_refdef.scene.ambient + rsurface.modellight_ambient[1] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[1], (r_refdef.scene.ambient + rsurface.modellight_ambient[2] * r_refdef.lightmapintensity * r_refdef.scene.rtlightstylevalue[0]) * colormod[2]);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Diffuse, r_refdef.lightmapintensity * colormod[0], r_refdef.lightmapintensity * colormod[1], r_refdef.lightmapintensity * colormod[2]);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_DeferredMod_Diffuse, colormod[0] * r_shadow_deferred_8bitrange.value, colormod[1] * r_shadow_deferred_8bitrange.value, colormod[2] * r_shadow_deferred_8bitrange.value);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_LightColor, rsurface.modellight_diffuse[0] * r_refdef.scene.rtlightstylevalue[0], rsurface.modellight_diffuse[1] * r_refdef.scene.rtlightstylevalue[0], rsurface.modellight_diffuse[2] * r_refdef.scene.rtlightstylevalue[0]);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
			}
			else
			{
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Ambient, r_refdef.scene.ambient * colormod[0], r_refdef.scene.ambient * colormod[1], r_refdef.scene.ambient * colormod[2]);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Diffuse, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2]);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Specular, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale, r_refdef.lightmapintensity * r_refdef.view.colorscale * specularscale);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_DeferredMod_Diffuse, colormod[0] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[1] * diffusescale * r_shadow_deferred_8bitrange.value, colormod[2] * diffusescale * r_shadow_deferred_8bitrange.value);
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_DeferredMod_Specular, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value, specularscale * r_shadow_deferred_8bitrange.value);
			}
			// additive passes are only darkened by fog, not tinted
			if(blendfuncflags & BLENDFUNC_ALLOWS_FOG_HACK0)
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_FogColor, 0, 0, 0);
			else
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_RefractColor, rsurface.texture->refractcolor4f[0], rsurface.texture->refractcolor4f[1], rsurface.texture->refractcolor4f[2], rsurface.texture->refractcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_ReflectColor, rsurface.texture->reflectcolor4f[0], rsurface.texture->reflectcolor4f[1], rsurface.texture->reflectcolor4f[2], rsurface.texture->reflectcolor4f[3] * rsurface.texture->lightmapcolor[3]);
			DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);
			DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_ReflectOffset, rsurface.texture->reflectmin);
			DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_SpecularPower, rsurface.texture->specularpower * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
			DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_NormalmapScrollBlend, rsurface.texture->r_water_waterscroll[0], rsurface.texture->r_water_waterscroll[1]);
		}
		{Matrix4x4_ToArrayFloatGL(&rsurface.texture->currenttexmatrix, m16f);DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_TexMatrixM1, 1, false, m16f);}
		{Matrix4x4_ToArrayFloatGL(&rsurface.texture->currentbackgroundtexmatrix, m16f);DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_BackgroundTexMatrixM1, 1, false, m16f);}
		{Matrix4x4_ToArrayFloatGL(&r_shadow_shadowmapmatrix, m16f);DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ShadowMapMatrixM1, 1, false, m16f);}
		DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);

		DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Glow, rsurface.glowmod[0], rsurface.glowmod[1], rsurface.glowmod[2]);
		DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_Alpha, rsurface.texture->lightmapcolor[3] * ((rsurface.texture->basematerialflags & MATERIALFLAG_WATERSHADER && r_waterstate.enabled && !r_refdef.view.isoverlay) ? rsurface.texture->r_water_wateralpha : 1));
		DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_EyePosition, rsurface.localvieworigin[0], rsurface.localvieworigin[1], rsurface.localvieworigin[2]);
		if (DPSOFTRAST_UNIFORM_Color_Pants >= 0)
		{
			if (rsurface.texture->pantstexture)
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
			else
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Pants, 0, 0, 0);
		}
		if (DPSOFTRAST_UNIFORM_Color_Shirt >= 0)
		{
			if (rsurface.texture->shirttexture)
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
			else
				DPSOFTRAST_Uniform3f(DPSOFTRAST_UNIFORM_Color_Shirt, 0, 0, 0);
		}
		DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_FogPlane, rsurface.fogplane[0], rsurface.fogplane[1], rsurface.fogplane[2], rsurface.fogplane[3]);
		DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_FogPlaneViewDist, rsurface.fogplaneviewdist);
		DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_FogRangeRecip, rsurface.fograngerecip);
		DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_FogHeightFade, rsurface.fogheightfade);
		DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_OffsetMapping_ScaleSteps,
				r_glsl_offsetmapping_scale.value*rsurface.texture->offsetscale,
				max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				1.0 / max(1, (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING) ? r_glsl_offsetmapping_reliefmapping_steps.integer : r_glsl_offsetmapping_steps.integer),
				max(1, r_glsl_offsetmapping_reliefmapping_refinesteps.integer)
			);
		DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);

		R_Mesh_TexBind(GL20TU_NORMAL            , rsurface.texture->nmaptexture                       );
		R_Mesh_TexBind(GL20TU_COLOR             , rsurface.texture->basetexture                       );
		R_Mesh_TexBind(GL20TU_GLOSS             , rsurface.texture->glosstexture                      );
		R_Mesh_TexBind(GL20TU_GLOW              , rsurface.texture->glowtexture                       );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_NORMAL  , rsurface.texture->backgroundnmaptexture             );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_COLOR   , rsurface.texture->backgroundbasetexture             );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_GLOSS   , rsurface.texture->backgroundglosstexture            );
		if (permutation & SHADERPERMUTATION_VERTEXTEXTUREBLEND) R_Mesh_TexBind(GL20TU_SECONDARY_GLOW    , rsurface.texture->backgroundglowtexture             );
		if (permutation & SHADERPERMUTATION_COLORMAPPING) R_Mesh_TexBind(GL20TU_PANTS             , rsurface.texture->pantstexture                      );
		if (permutation & SHADERPERMUTATION_COLORMAPPING) R_Mesh_TexBind(GL20TU_SHIRT             , rsurface.texture->shirttexture                      );
		if (permutation & SHADERPERMUTATION_REFLECTCUBE) R_Mesh_TexBind(GL20TU_REFLECTMASK       , rsurface.texture->reflectmasktexture                );
		if (permutation & SHADERPERMUTATION_REFLECTCUBE) R_Mesh_TexBind(GL20TU_REFLECTCUBE       , rsurface.texture->reflectcubetexture ? rsurface.texture->reflectcubetexture : r_texture_whitecube);
		if (permutation & SHADERPERMUTATION_FOGHEIGHTTEXTURE) R_Mesh_TexBind(GL20TU_FOGHEIGHTTEXTURE  , r_texture_fogheighttexture                          );
		if (permutation & (SHADERPERMUTATION_FOGINSIDE | SHADERPERMUTATION_FOGOUTSIDE)) R_Mesh_TexBind(GL20TU_FOGMASK           , r_texture_fogattenuation                            );
		R_Mesh_TexBind(GL20TU_LIGHTMAP          , rsurface.lightmaptexture ? rsurface.lightmaptexture : r_texture_white);
		R_Mesh_TexBind(GL20TU_DELUXEMAP         , rsurface.deluxemaptexture ? rsurface.deluxemaptexture : r_texture_blanknormalmap);
		if (rsurface.rtlight                                  ) R_Mesh_TexBind(GL20TU_ATTENUATION       , r_shadow_attenuationgradienttexture                 );
		if (rsurfacepass == RSURFPASS_BACKGROUND)
		{
			R_Mesh_TexBind(GL20TU_REFRACTION        , waterplane->texture_refraction ? waterplane->texture_refraction : r_texture_black);
			if(mode == SHADERMODE_GENERIC) R_Mesh_TexBind(GL20TU_FIRST             , waterplane->texture_camera ? waterplane->texture_camera : r_texture_black);
			R_Mesh_TexBind(GL20TU_REFLECTION        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
		else
		{
			if (permutation & SHADERPERMUTATION_REFLECTION        ) R_Mesh_TexBind(GL20TU_REFLECTION        , waterplane->texture_reflection ? waterplane->texture_reflection : r_texture_black);
		}
//		if (rsurfacepass == RSURFPASS_DEFERREDLIGHT           ) R_Mesh_TexBind(GL20TU_SCREENDEPTH       , r_shadow_prepassgeometrydepthtexture                );
//		if (rsurfacepass == RSURFPASS_DEFERREDLIGHT           ) R_Mesh_TexBind(GL20TU_SCREENNORMALMAP   , r_shadow_prepassgeometrynormalmaptexture            );
		if (permutation & SHADERPERMUTATION_DEFERREDLIGHTMAP  ) R_Mesh_TexBind(GL20TU_SCREENDIFFUSE     , r_shadow_prepasslightingdiffusetexture              );
		if (permutation & SHADERPERMUTATION_DEFERREDLIGHTMAP  ) R_Mesh_TexBind(GL20TU_SCREENSPECULAR    , r_shadow_prepasslightingspeculartexture             );
		if (rsurface.rtlight || (r_shadow_usingshadowmaportho && !(rsurface.ent_flags & RENDER_NOSELFSHADOW)))
		{
			R_Mesh_TexBind(GL20TU_SHADOWMAP2D, r_shadow_shadowmap2dcolortexture);
			if (rsurface.rtlight)
			{
				if (permutation & SHADERPERMUTATION_CUBEFILTER        ) R_Mesh_TexBind(GL20TU_CUBE              , rsurface.rtlight->currentcubemap                    );
				if (permutation & SHADERPERMUTATION_SHADOWMAPVSDCT    ) R_Mesh_TexBind(GL20TU_CUBEPROJECTION    , r_shadow_shadowmapvsdcttexture                      );
			}
		}
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
	if (specularscale > 0 && r_shadow_gloss.integer > 0)
		permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
	if (r_shadow_usingshadowmap2d)
	{
		permutation |= SHADERPERMUTATION_SHADOWMAP2D;
		if (r_shadow_shadowmapvsdct)
			permutation |= SHADERPERMUTATION_SHADOWMAPVSDCT;

		if (r_shadow_shadowmapsampler)
			permutation |= SHADERPERMUTATION_SHADOWSAMPLER;
		if (r_shadow_shadowmappcf > 1)
			permutation |= SHADERPERMUTATION_SHADOWMAPPCF2;
		else if (r_shadow_shadowmappcf)
			permutation |= SHADERPERMUTATION_SHADOWMAPPCF;
	}
	if (vid.allowalphatocoverage)
		GL_AlphaToCoverage(false);
	Matrix4x4_Transform(&r_refdef.view.viewport.viewmatrix, rtlight->shadoworigin, viewlightorigin);
	Matrix4x4_Concat(&lighttoview, &r_refdef.view.viewport.viewmatrix, &rtlight->matrix_lighttoworld);
	Matrix4x4_Invert_Simple(&viewtolight, &lighttoview);
	Matrix4x4_ToArrayFloatGL(&viewtolight, viewtolight16f);
	switch(vid.renderpath)
	{
	case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
		R_SetupShader_SetPermutationHLSL(mode, permutation);
		hlslPSSetParameter3f(D3DPSREGISTER_LightPosition, viewlightorigin[0], viewlightorigin[1], viewlightorigin[2]);
		hlslPSSetParameter16f(D3DPSREGISTER_ViewToLight, viewtolight16f);
		hlslPSSetParameter3f(D3DPSREGISTER_DeferredColor_Ambient , lightcolorbase[0] * ambientscale  * range, lightcolorbase[1] * ambientscale  * range, lightcolorbase[2] * ambientscale  * range);
		hlslPSSetParameter3f(D3DPSREGISTER_DeferredColor_Diffuse , lightcolorbase[0] * diffusescale  * range, lightcolorbase[1] * diffusescale  * range, lightcolorbase[2] * diffusescale  * range);
		hlslPSSetParameter3f(D3DPSREGISTER_DeferredColor_Specular, lightcolorbase[0] * specularscale * range, lightcolorbase[1] * specularscale * range, lightcolorbase[2] * specularscale * range);
		hlslPSSetParameter2f(D3DPSREGISTER_ShadowMap_TextureScale, r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		hlslPSSetParameter4f(D3DPSREGISTER_ShadowMap_Parameters, r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);
		hlslPSSetParameter1f(D3DPSREGISTER_SpecularPower, (r_shadow_gloss.integer == 2 ? r_shadow_gloss2exponent.value : r_shadow_glossexponent.value) * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		hlslPSSetParameter2f(D3DPSREGISTER_ScreenToDepth, r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		hlslPSSetParameter2f(D3DPSREGISTER_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);

		R_Mesh_TexBind(GL20TU_ATTENUATION        , r_shadow_attenuationgradienttexture                 );
		R_Mesh_TexBind(GL20TU_SCREENDEPTH        , r_shadow_prepassgeometrydepthcolortexture           );
		R_Mesh_TexBind(GL20TU_SCREENNORMALMAP    , r_shadow_prepassgeometrynormalmaptexture            );
		R_Mesh_TexBind(GL20TU_CUBE               , rsurface.rtlight->currentcubemap                    );
		R_Mesh_TexBind(GL20TU_SHADOWMAP2D        , r_shadow_shadowmap2dcolortexture                    );
		R_Mesh_TexBind(GL20TU_CUBEPROJECTION     , r_shadow_shadowmapvsdcttexture                      );
#endif
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		R_SetupShader_SetPermutationGLSL(mode, permutation);
		if (r_glsl_permutation->loc_LightPosition             >= 0) qglUniform3f(       r_glsl_permutation->loc_LightPosition            , viewlightorigin[0], viewlightorigin[1], viewlightorigin[2]);
		if (r_glsl_permutation->loc_ViewToLight               >= 0) qglUniformMatrix4fv(r_glsl_permutation->loc_ViewToLight              , 1, false, viewtolight16f);
		if (r_glsl_permutation->loc_DeferredColor_Ambient     >= 0) qglUniform3f(       r_glsl_permutation->loc_DeferredColor_Ambient    , lightcolorbase[0] * ambientscale  * range, lightcolorbase[1] * ambientscale  * range, lightcolorbase[2] * ambientscale  * range);
		if (r_glsl_permutation->loc_DeferredColor_Diffuse     >= 0) qglUniform3f(       r_glsl_permutation->loc_DeferredColor_Diffuse    , lightcolorbase[0] * diffusescale  * range, lightcolorbase[1] * diffusescale  * range, lightcolorbase[2] * diffusescale  * range);
		if (r_glsl_permutation->loc_DeferredColor_Specular    >= 0) qglUniform3f(       r_glsl_permutation->loc_DeferredColor_Specular   , lightcolorbase[0] * specularscale * range, lightcolorbase[1] * specularscale * range, lightcolorbase[2] * specularscale * range);
		if (r_glsl_permutation->loc_ShadowMap_TextureScale    >= 0) qglUniform2f(       r_glsl_permutation->loc_ShadowMap_TextureScale   , r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		if (r_glsl_permutation->loc_ShadowMap_Parameters      >= 0) qglUniform4f(       r_glsl_permutation->loc_ShadowMap_Parameters     , r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);
		if (r_glsl_permutation->loc_SpecularPower             >= 0) qglUniform1f(       r_glsl_permutation->loc_SpecularPower            , (r_shadow_gloss.integer == 2 ? r_shadow_gloss2exponent.value : r_shadow_glossexponent.value) * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		if (r_glsl_permutation->loc_ScreenToDepth             >= 0) qglUniform2f(       r_glsl_permutation->loc_ScreenToDepth            , r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		if (r_glsl_permutation->loc_PixelToScreenTexCoord     >= 0) qglUniform2f(       r_glsl_permutation->loc_PixelToScreenTexCoord    , 1.0f/vid.width, 1.0f/vid.height);

		if (r_glsl_permutation->tex_Texture_Attenuation       >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Attenuation        , r_shadow_attenuationgradienttexture                 );
		if (r_glsl_permutation->tex_Texture_ScreenDepth       >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenDepth        , r_shadow_prepassgeometrydepthtexture                );
		if (r_glsl_permutation->tex_Texture_ScreenNormalMap   >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ScreenNormalMap    , r_shadow_prepassgeometrynormalmaptexture            );
		if (r_glsl_permutation->tex_Texture_Cube              >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Cube               , rsurface.rtlight->currentcubemap                    );
		if (r_glsl_permutation->tex_Texture_ShadowMap2D       >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_ShadowMap2D        , r_shadow_shadowmap2dtexture                         );
		if (r_glsl_permutation->tex_Texture_CubeProjection    >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_CubeProjection     , r_shadow_shadowmapvsdcttexture                      );
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		break;
	case RENDERPATH_SOFT:
		R_SetupShader_SetPermutationGLSL(mode, permutation);
		DPSOFTRAST_Uniform3f(       DPSOFTRAST_UNIFORM_LightPosition            , viewlightorigin[0], viewlightorigin[1], viewlightorigin[2]);
		DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ViewToLightM1            , 1, false, viewtolight16f);
		DPSOFTRAST_Uniform3f(       DPSOFTRAST_UNIFORM_DeferredColor_Ambient    , lightcolorbase[0] * ambientscale  * range, lightcolorbase[1] * ambientscale  * range, lightcolorbase[2] * ambientscale  * range);
		DPSOFTRAST_Uniform3f(       DPSOFTRAST_UNIFORM_DeferredColor_Diffuse    , lightcolorbase[0] * diffusescale  * range, lightcolorbase[1] * diffusescale  * range, lightcolorbase[2] * diffusescale  * range);
		DPSOFTRAST_Uniform3f(       DPSOFTRAST_UNIFORM_DeferredColor_Specular   , lightcolorbase[0] * specularscale * range, lightcolorbase[1] * specularscale * range, lightcolorbase[2] * specularscale * range);
		DPSOFTRAST_Uniform2f(       DPSOFTRAST_UNIFORM_ShadowMap_TextureScale   , r_shadow_shadowmap_texturescale[0], r_shadow_shadowmap_texturescale[1]);
		DPSOFTRAST_Uniform4f(       DPSOFTRAST_UNIFORM_ShadowMap_Parameters     , r_shadow_shadowmap_parameters[0], r_shadow_shadowmap_parameters[1], r_shadow_shadowmap_parameters[2], r_shadow_shadowmap_parameters[3]);
		DPSOFTRAST_Uniform1f(       DPSOFTRAST_UNIFORM_SpecularPower            , (r_shadow_gloss.integer == 2 ? r_shadow_gloss2exponent.value : r_shadow_glossexponent.value) * (r_shadow_glossexact.integer ? 0.25f : 1.0f));
		DPSOFTRAST_Uniform2f(       DPSOFTRAST_UNIFORM_ScreenToDepth            , r_refdef.view.viewport.screentodepth[0], r_refdef.view.viewport.screentodepth[1]);
		DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);

		R_Mesh_TexBind(GL20TU_ATTENUATION        , r_shadow_attenuationgradienttexture                 );
		R_Mesh_TexBind(GL20TU_SCREENDEPTH        , r_shadow_prepassgeometrydepthtexture                );
		R_Mesh_TexBind(GL20TU_SCREENNORMALMAP    , r_shadow_prepassgeometrynormalmaptexture            );
		R_Mesh_TexBind(GL20TU_CUBE               , rsurface.rtlight->currentcubemap                    );
		R_Mesh_TexBind(GL20TU_SHADOWMAP2D        , r_shadow_shadowmap2dtexture                         );
		R_Mesh_TexBind(GL20TU_CUBEPROJECTION     , r_shadow_shadowmapvsdcttexture                      );
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

extern cvar_t gl_picmip;
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
	int miplevel = R_PicmipForFlags(textureflags);
	int savemiplevel = miplevel;
	int mymiplevel;

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
	if (!r_loaddds || !(ddsbase = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s.dds", basename), textureflags, &ddshasalpha, ddsavgcolor, miplevel)))
	{
		basepixels = loadimagepixelsbgra(name, complain, true, false, &miplevel);
		if (basepixels == NULL)
			return NULL;
	}

	// FIXME handle miplevel

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
			skinframe->fog = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_mask.dds", skinframe->basename), textureflags | TEXF_ALPHA, NULL, NULL, miplevel);
		//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);
	}
	else
	{
		basepixels_width = image_width;
		basepixels_height = image_height;
		skinframe->base = R_LoadTexture2D (r_main_texturepool, skinframe->basename, basepixels_width, basepixels_height, basepixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_color.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), miplevel, NULL);
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
				skinframe->fog = R_LoadTexture2D (r_main_texturepool, va("%s_mask", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, textureflags & (gl_texturecompression_color.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), miplevel, NULL);
				Mem_Free(pixels);
			}
		}
		R_SKINFRAME_LOAD_AVERAGE_COLORS(basepixels_width * basepixels_height, basepixels[4 * pix + comp]);
		//Con_Printf("Texture %s has average colors %f %f %f alpha %f\n", name, skinframe->avgcolor[0], skinframe->avgcolor[1], skinframe->avgcolor[2], skinframe->avgcolor[3]);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->base)
			R_SaveTextureDDSFile(skinframe->base, va("dds/%s.dds", skinframe->basename), r_texture_dds_save.integer < 2, skinframe->hasalpha);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->fog)
			R_SaveTextureDDSFile(skinframe->fog, va("dds/%s_mask.dds", skinframe->basename), r_texture_dds_save.integer < 2, true);
	}

	if (r_loaddds)
	{
		mymiplevel = savemiplevel;
		if (r_loadnormalmap)
			skinframe->nmap = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_norm.dds", skinframe->basename), (TEXF_ALPHA | textureflags) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP), NULL, NULL, mymiplevel);
		skinframe->glow = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_glow.dds", skinframe->basename), textureflags, NULL, NULL, mymiplevel);
		if (r_loadgloss)
			skinframe->gloss = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_gloss.dds", skinframe->basename), textureflags, NULL, NULL, mymiplevel);
		skinframe->pants = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_pants.dds", skinframe->basename), textureflags, NULL, NULL, mymiplevel);
		skinframe->shirt = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_shirt.dds", skinframe->basename), textureflags, NULL, NULL, mymiplevel);
		skinframe->reflect = R_LoadTextureDDSFile(r_main_texturepool, va("dds/%s_reflect.dds", skinframe->basename), textureflags, NULL, NULL, mymiplevel);
	}

	// _norm is the name used by tenebrae and has been adopted as standard
	if (r_loadnormalmap && skinframe->nmap == NULL)
	{
		mymiplevel = savemiplevel;
		if ((pixels = loadimagepixelsbgra(va("%s_norm", skinframe->basename), false, false, false, &mymiplevel)) != NULL)
		{
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | textureflags) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP) & (gl_texturecompression_normal.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
			Mem_Free(pixels);
			pixels = NULL;
		}
		else if (r_shadow_bumpscale_bumpmap.value > 0 && (bumppixels = loadimagepixelsbgra(va("%s_bump", skinframe->basename), false, false, false, &mymiplevel)) != NULL)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, image_width * image_height * 4);
			Image_HeightmapToNormalmap_BGRA(bumppixels, pixels, image_width, image_height, false, r_shadow_bumpscale_bumpmap.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | textureflags) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP) & (gl_texturecompression_normal.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
			Mem_Free(pixels);
			Mem_Free(bumppixels);
		}
		else if (r_shadow_bumpscale_basetexture.value > 0)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, basepixels_width * basepixels_height * 4);
			Image_HeightmapToNormalmap_BGRA(basepixels, pixels, basepixels_width, basepixels_height, false, r_shadow_bumpscale_basetexture.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), basepixels_width, basepixels_height, pixels, TEXTYPE_BGRA, (TEXF_ALPHA | textureflags) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP) & (gl_texturecompression_normal.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
			Mem_Free(pixels);
		}
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->nmap)
			R_SaveTextureDDSFile(skinframe->nmap, va("dds/%s_norm.dds", skinframe->basename), r_texture_dds_save.integer < 2, true);
	}

	// _luma is supported only for tenebrae compatibility
	// _glow is the preferred name
	mymiplevel = savemiplevel;
	if (skinframe->glow == NULL && ((pixels = loadimagepixelsbgra(va("%s_glow",  skinframe->basename), false, false, false, &mymiplevel)) || (pixels = loadimagepixelsbgra(va("%s_luma", skinframe->basename), false, false, false, &mymiplevel))))
	{
		skinframe->glow = R_LoadTexture2D (r_main_texturepool, va("%s_glow", skinframe->basename), image_width, image_height, pixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_glow.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->glow)
			R_SaveTextureDDSFile(skinframe->glow, va("dds/%s_glow.dds", skinframe->basename), r_texture_dds_save.integer < 2, true);
		Mem_Free(pixels);pixels = NULL;
	}

	mymiplevel = savemiplevel;
	if (skinframe->gloss == NULL && r_loadgloss && (pixels = loadimagepixelsbgra(va("%s_gloss", skinframe->basename), false, false, false, &mymiplevel)))
	{
		skinframe->gloss = R_LoadTexture2D (r_main_texturepool, va("%s_gloss", skinframe->basename), image_width, image_height, pixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_gloss.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->gloss)
			R_SaveTextureDDSFile(skinframe->gloss, va("dds/%s_gloss.dds", skinframe->basename), r_texture_dds_save.integer < 2, true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	mymiplevel = savemiplevel;
	if (skinframe->pants == NULL && (pixels = loadimagepixelsbgra(va("%s_pants", skinframe->basename), false, false, false, &mymiplevel)))
	{
		skinframe->pants = R_LoadTexture2D (r_main_texturepool, va("%s_pants", skinframe->basename), image_width, image_height, pixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_color.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->pants)
			R_SaveTextureDDSFile(skinframe->pants, va("dds/%s_pants.dds", skinframe->basename), r_texture_dds_save.integer < 2, false);
		Mem_Free(pixels);
		pixels = NULL;
	}

	mymiplevel = savemiplevel;
	if (skinframe->shirt == NULL && (pixels = loadimagepixelsbgra(va("%s_shirt", skinframe->basename), false, false, false, &mymiplevel)))
	{
		skinframe->shirt = R_LoadTexture2D (r_main_texturepool, va("%s_shirt", skinframe->basename), image_width, image_height, pixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_color.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->shirt)
			R_SaveTextureDDSFile(skinframe->shirt, va("dds/%s_shirt.dds", skinframe->basename), r_texture_dds_save.integer < 2, false);
		Mem_Free(pixels);
		pixels = NULL;
	}

	mymiplevel = savemiplevel;
	if (skinframe->reflect == NULL && (pixels = loadimagepixelsbgra(va("%s_reflect", skinframe->basename), false, false, false, &mymiplevel)))
	{
		skinframe->reflect = R_LoadTexture2D (r_main_texturepool, va("%s_reflect", skinframe->basename), image_width, image_height, pixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags & (gl_texturecompression_reflectmask.integer && gl_texturecompression.integer ? ~0 : ~TEXF_COMPRESS), mymiplevel, NULL);
		if (r_savedds && qglGetCompressedTexImageARB && skinframe->reflect)
			R_SaveTextureDDSFile(skinframe->reflect, va("dds/%s_reflect.dds", skinframe->basename), r_texture_dds_save.integer < 2, true);
		Mem_Free(pixels);
		pixels = NULL;
	}

	if (basepixels)
		Mem_Free(basepixels);

	return skinframe;
}

// this is only used by .spr32 sprites, HL .spr files, HL .bsp files
skinframe_t *R_SkinFrame_LoadInternalBGRA(const char *name, int textureflags, const unsigned char *skindata, int width, int height, qboolean sRGB)
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
		skinframe->nmap = R_LoadTexture2D(r_main_texturepool, va("%s_nmap", skinframe->basename), width, height, temp2, TEXTYPE_BGRA, (textureflags | TEXF_ALPHA) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP), -1, NULL);
		Mem_Free(temp1);
	}
	skinframe->base = skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, sRGB ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, textureflags, -1, NULL);
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
			skinframe->fog = R_LoadTexture2D(r_main_texturepool, va("%s_fog", skinframe->basename), width, height, fogpixels, TEXTYPE_BGRA, textureflags, -1, NULL);
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
	skinframe->qpixels = (unsigned char *)Mem_Alloc(r_main_mempool, width*height); // FIXME LEAK
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
		skinframe->nmap = R_LoadTexture2D(r_main_texturepool, va("%s_nmap", skinframe->basename), width, height, temp2, TEXTYPE_BGRA, (skinframe->textureflags | TEXF_ALPHA) & (r_mipnormalmaps.integer ? ~0 : ~TEXF_MIPMAP), -1, NULL);
		Mem_Free(temp1);
	}

	if (skinframe->qgenerateglow)
	{
		skinframe->qgenerateglow = false;
		skinframe->glow = R_LoadTexture2D(r_main_texturepool, va("%s_glow", skinframe->basename), width, height, skindata, vid.sRGB3D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, skinframe->textureflags, -1, palette_bgra_onlyfullbrights); // glow
	}

	if (colormapped)
	{
		skinframe->qgeneratebase = false;
		skinframe->base  = R_LoadTexture2D(r_main_texturepool, va("%s_nospecial", skinframe->basename), width, height, skindata, vid.sRGB3D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, skinframe->textureflags, -1, skinframe->glow ? palette_bgra_nocolormapnofullbrights : palette_bgra_nocolormap);
		skinframe->pants = R_LoadTexture2D(r_main_texturepool, va("%s_pants", skinframe->basename), width, height, skindata, vid.sRGB3D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, skinframe->textureflags, -1, palette_bgra_pantsaswhite);
		skinframe->shirt = R_LoadTexture2D(r_main_texturepool, va("%s_shirt", skinframe->basename), width, height, skindata, vid.sRGB3D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, skinframe->textureflags, -1, palette_bgra_shirtaswhite);
	}
	else
	{
		skinframe->qgeneratemerged = false;
		skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, vid.sRGB3D ? TEXTYPE_SRGB_PALETTE : TEXTYPE_PALETTE, skinframe->textureflags, -1, skinframe->glow ? palette_bgra_nofullbrights : palette_bgra_complete);
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

	skinframe->base = skinframe->merged = R_LoadTexture2D(r_main_texturepool, skinframe->basename, width, height, skindata, TEXTYPE_PALETTE, textureflags, -1, palette);
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
			skinframe->fog = R_LoadTexture2D(r_main_texturepool, va("%s_fog", skinframe->basename), width, height, skindata, TEXTYPE_PALETTE, textureflags, -1, alphapalette);
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
	const char *suffix;
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
			if ((image_buffer = loadimagepixelsbgra(name, false, false, false, NULL)))
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

		cubemaptexture = R_LoadTextureCubeMap(r_main_texturepool, basename, cubemapsize, cubemappixels, vid.sRGB3D ? TEXTYPE_SRGB_BGRA : TEXTYPE_BGRA, (gl_texturecompression_lightcubemaps.integer && gl_texturecompression.integer ? TEXF_COMPRESS : 0) | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
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
		if (r_texture_cubemaps[i] != NULL)
			if (!strcasecmp(r_texture_cubemaps[i]->basename, basename))
				return r_texture_cubemaps[i]->texture ? r_texture_cubemaps[i]->texture : r_texture_whitecube;
	if (i >= MAX_CUBEMAPS || !r_main_mempool)
		return r_texture_whitecube;
	r_texture_numcubemaps++;
	r_texture_cubemaps[i] = (cubemapinfo_t *)Mem_Alloc(r_main_mempool, sizeof(cubemapinfo_t));
	strlcpy(r_texture_cubemaps[i]->basename, basename, sizeof(r_texture_cubemaps[i]->basename));
	r_texture_cubemaps[i]->texture = R_LoadCubemap(r_texture_cubemaps[i]->basename);
	return r_texture_cubemaps[i]->texture;
}

void R_FreeCubemap(const char *basename)
{
	int i;

	for (i = 0;i < r_texture_numcubemaps;i++)
	{
		if (r_texture_cubemaps[i] != NULL)
		{
			if (r_texture_cubemaps[i]->texture)
			{
				if (developer_loading.integer)
					Con_DPrintf("unloading cubemap \"%s\"\n", r_texture_cubemaps[i]->basename);
				R_FreeTexture(r_texture_cubemaps[i]->texture);
				Mem_Free(r_texture_cubemaps[i]);
				r_texture_cubemaps[i] = NULL;
			}
		}
	}
}

void R_FreeCubemaps(void)
{
	int i;
	for (i = 0;i < r_texture_numcubemaps;i++)
	{
		if (developer_loading.integer)
			Con_DPrintf("unloading cubemap \"%s\"\n", r_texture_cubemaps[i]->basename);
		if (r_texture_cubemaps[i] != NULL)
		{
			if (r_texture_cubemaps[i]->texture)
				R_FreeTexture(r_texture_cubemaps[i]->texture);
			Mem_Free(r_texture_cubemaps[i]);
		}
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
		r_refdef.viewcache.entityvisible = (unsigned char *)Mem_Alloc(r_main_mempool, r_refdef.viewcache.maxentities);
	}
	if (r_refdef.viewcache.world_numclusters != numclusters)
	{
		r_refdef.viewcache.world_numclusters = numclusters;
		r_refdef.viewcache.world_numclusterbytes = numclusterbytes;
		if (r_refdef.viewcache.world_pvsbits)
			Mem_Free(r_refdef.viewcache.world_pvsbits);
		r_refdef.viewcache.world_pvsbits = (unsigned char *)Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numclusterbytes);
	}
	if (r_refdef.viewcache.world_numleafs != numleafs)
	{
		r_refdef.viewcache.world_numleafs = numleafs;
		if (r_refdef.viewcache.world_leafvisible)
			Mem_Free(r_refdef.viewcache.world_leafvisible);
		r_refdef.viewcache.world_leafvisible = (unsigned char *)Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numleafs);
	}
	if (r_refdef.viewcache.world_numsurfaces != numsurfaces)
	{
		r_refdef.viewcache.world_numsurfaces = numsurfaces;
		if (r_refdef.viewcache.world_surfacevisible)
			Mem_Free(r_refdef.viewcache.world_surfacevisible);
		r_refdef.viewcache.world_surfacevisible = (unsigned char *)Mem_Alloc(r_main_mempool, r_refdef.viewcache.world_numsurfaces);
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
	r_texture_fogheighttexture = NULL;
	r_texture_gammaramps = NULL;
	r_texture_numcubemaps = 0;

	r_loaddds = r_texture_dds_load.integer != 0;
	r_savedds = vid.support.arb_texture_compression && vid.support.ext_texture_compression_s3tc && r_texture_dds_save.integer;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		Cvar_SetValueQuick(&r_textureunits, vid.texunits);
		Cvar_SetValueQuick(&gl_combine, 1);
		Cvar_SetValueQuick(&r_glsl, 1);
		r_loadnormalmap = true;
		r_loadgloss = true;
		r_loadfog = false;
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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

	// due to caching of texture_t references, the collision cache must be reset
	Collision_Cache_Reset(true);

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
	r_texture_fogheighttexture = NULL;
	r_texture_gammaramps = NULL;
	//r_texture_fogintensity = NULL;
	memset(&r_bloomstate, 0, sizeof(r_bloomstate));
	memset(&r_waterstate, 0, sizeof(r_waterstate));
	r_glsl_permutation = NULL;
	memset(r_glsl_permutationhash, 0, sizeof(r_glsl_permutationhash));
	Mem_ExpandableArray_NewArray(&r_glsl_permutationarray, r_main_mempool, sizeof(r_glsl_permutation_t), 256);
	glslshaderstring = NULL;
#ifdef SUPPORTD3D
	r_hlsl_permutation = NULL;
	memset(r_hlsl_permutationhash, 0, sizeof(r_hlsl_permutationhash));
	Mem_ExpandableArray_NewArray(&r_hlsl_permutationarray, r_main_mempool, sizeof(r_hlsl_permutation_t), 256);
#endif
	hlslshaderstring = NULL;
	memset(&r_svbsp, 0, sizeof (r_svbsp));

	memset(r_texture_cubemaps, 0, sizeof(r_texture_cubemaps));
	r_texture_numcubemaps = 0;

	r_refdef.fogmasktable_density = 0;
}

void gl_main_shutdown(void)
{
	R_AnimCache_Free();
	R_FrameData_Reset();

	R_Main_FreeViewCache();

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		if (r_maxqueries)
			qglDeleteQueriesARB(r_maxqueries, r_queries);
		break;
	case RENDERPATH_D3D9:
		//Con_DPrintf("FIXME D3D9 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D10:
		Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_D3D11:
		Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
		break;
	case RENDERPATH_SOFT:
		break;
	}

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
	r_texture_fogheighttexture = NULL;
	r_texture_gammaramps = NULL;
	r_texture_numcubemaps = 0;
	//r_texture_fogintensity = NULL;
	memset(&r_bloomstate, 0, sizeof(r_bloomstate));
	memset(&r_waterstate, 0, sizeof(r_waterstate));
	R_GLSL_Restart_f();

	r_glsl_permutation = NULL;
	memset(r_glsl_permutationhash, 0, sizeof(r_glsl_permutationhash));
	Mem_ExpandableArray_FreeArray(&r_glsl_permutationarray);
	glslshaderstring = NULL;
#ifdef SUPPORTD3D
	r_hlsl_permutation = NULL;
	memset(r_hlsl_permutationhash, 0, sizeof(r_hlsl_permutationhash));
	Mem_ExpandableArray_FreeArray(&r_hlsl_permutationarray);
#endif
	hlslshaderstring = NULL;
}

extern void CL_ParseEntityLump(char *entitystring);
void gl_main_newmap(void)
{
	// FIXME: move this code to client
	char *entities, entname[MAX_QPATH];
	if (r_qwskincache)
		Mem_Free(r_qwskincache);
	r_qwskincache = NULL;
	r_qwskincache_size = 0;
	if (cl.worldmodel)
	{
		dpsnprintf(entname, sizeof(entname), "%s.ent", cl.worldnamenoextension);
		if ((entities = (char *)FS_LoadFile(entname, tempmempool, true, NULL)))
		{
			CL_ParseEntityLump(entities);
			Mem_Free(entities);
			return;
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
	Cvar_RegisterVariable(&r_deformvertexes);
	Cvar_RegisterVariable(&r_transparent);
	Cvar_RegisterVariable(&r_transparent_alphatocoverage);
	Cvar_RegisterVariable(&r_showoverdraw);
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
	Cvar_RegisterVariable(&r_draw2d);
	Cvar_RegisterVariable(&r_drawworld);
	Cvar_RegisterVariable(&r_cullentities_trace);
	Cvar_RegisterVariable(&r_cullentities_trace_samples);
	Cvar_RegisterVariable(&r_cullentities_trace_tempentitysamples);
	Cvar_RegisterVariable(&r_cullentities_trace_enlarge);
	Cvar_RegisterVariable(&r_cullentities_trace_delay);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_drawexteriormodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fakelight);
	Cvar_RegisterVariable(&r_fakelight_intensity);
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
	Cvar_RegisterVariable(&r_fog_clear);
	Cvar_RegisterVariable(&r_drawfog);
	Cvar_RegisterVariable(&r_transparentdepthmasking);
	Cvar_RegisterVariable(&r_transparent_sortmaxdist);
	Cvar_RegisterVariable(&r_transparent_sortarraysize);
	Cvar_RegisterVariable(&r_texture_dds_load);
	Cvar_RegisterVariable(&r_texture_dds_save);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&gl_combine);
	Cvar_RegisterVariable(&r_viewfbo);
	Cvar_RegisterVariable(&r_viewscale);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling_min);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling_multiply);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling_stepsize);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling_stepmax);
	Cvar_RegisterVariable(&r_viewscale_fpsscaling_target);
	Cvar_RegisterVariable(&r_glsl);
	Cvar_RegisterVariable(&r_glsl_deluxemapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_steps);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping_steps);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping_refinesteps);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_scale);
	Cvar_RegisterVariable(&r_glsl_postprocess);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec1);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec2);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec3);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec4);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec1_enable);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec2_enable);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec3_enable);
	Cvar_RegisterVariable(&r_glsl_postprocess_uservec4_enable);

	Cvar_RegisterVariable(&r_water);
	Cvar_RegisterVariable(&r_water_resolutionmultiplier);
	Cvar_RegisterVariable(&r_water_clippingplanebias);
	Cvar_RegisterVariable(&r_water_refractdistort);
	Cvar_RegisterVariable(&r_water_reflectdistort);
	Cvar_RegisterVariable(&r_water_scissormode);
	Cvar_RegisterVariable(&r_water_lowquality);

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
	Cvar_RegisterVariable(&r_hdr_irisadaptation);
	Cvar_RegisterVariable(&r_hdr_irisadaptation_multiplier);
	Cvar_RegisterVariable(&r_hdr_irisadaptation_minvalue);
	Cvar_RegisterVariable(&r_hdr_irisadaptation_maxvalue);
	Cvar_RegisterVariable(&r_hdr_irisadaptation_value);
	Cvar_RegisterVariable(&r_hdr_irisadaptation_fade);
	Cvar_RegisterVariable(&r_smoothnormals_areaweighting);
	Cvar_RegisterVariable(&developer_texturelogging);
	Cvar_RegisterVariable(&gl_lightmaps);
	Cvar_RegisterVariable(&r_test);
	Cvar_RegisterVariable(&r_glsl_saturation);
	Cvar_RegisterVariable(&r_glsl_saturation_redcompensate);
	Cvar_RegisterVariable(&r_glsl_vertextextureblend_usebothalphas);
	Cvar_RegisterVariable(&r_framedatasize);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_TENEBRAE)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap, NULL, NULL);
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
	GL_Clear(GL_COLOR_BUFFER_BIT, NULL, 1.0f, 128);
}

int R_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int i;
	mplane_t *p;
	if (r_trippy.integer)
		return false;
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
	if (r_trippy.integer)
		return false;
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

typedef struct r_framedata_mem_s
{
	struct r_framedata_mem_s *purge; // older mem block to free on next frame
	size_t size; // how much usable space
	size_t current; // how much space in use
	size_t mark; // last "mark" location, temporary memory can be freed by returning to this
	size_t wantedsize; // how much space was allocated
	unsigned char *data; // start of real data (16byte aligned)
}
r_framedata_mem_t;

static r_framedata_mem_t *r_framedata_mem;

void R_FrameData_Reset(void)
{
	while (r_framedata_mem)
	{
		r_framedata_mem_t *next = r_framedata_mem->purge;
		Mem_Free(r_framedata_mem);
		r_framedata_mem = next;
	}
}

void R_FrameData_Resize(void)
{
	size_t wantedsize;
	wantedsize = (size_t)(r_framedatasize.value * 1024*1024);
	wantedsize = bound(65536, wantedsize, 1000*1024*1024);
	if (!r_framedata_mem || r_framedata_mem->wantedsize != wantedsize)
	{
		r_framedata_mem_t *newmem = (r_framedata_mem_t *)Mem_Alloc(r_main_mempool, wantedsize);
		newmem->wantedsize = wantedsize;
		newmem->data = (unsigned char *)(((size_t)(newmem+1) + 15) & ~15);
		newmem->size = (unsigned char *)newmem + wantedsize - newmem->data;
		newmem->current = 0;
		newmem->mark = 0;
		newmem->purge = r_framedata_mem;
		r_framedata_mem = newmem;
	}
}

void R_FrameData_NewFrame(void)
{
	R_FrameData_Resize();
	if (!r_framedata_mem)
		return;
	// if we ran out of space on the last frame, free the old memory now
	while (r_framedata_mem->purge)
	{
		// repeatedly remove the second item in the list, leaving only head
		r_framedata_mem_t *next = r_framedata_mem->purge->purge;
		Mem_Free(r_framedata_mem->purge);
		r_framedata_mem->purge = next;
	}
	// reset the current mem pointer
	r_framedata_mem->current = 0;
	r_framedata_mem->mark = 0;
}

void *R_FrameData_Alloc(size_t size)
{
	void *data;

	// align to 16 byte boundary - the data pointer is already aligned, so we
	// only need to ensure the size of every allocation is also aligned
	size = (size + 15) & ~15;

	while (!r_framedata_mem || r_framedata_mem->current + size > r_framedata_mem->size)
	{
		// emergency - we ran out of space, allocate more memory
		Cvar_SetValueQuick(&r_framedatasize, bound(0.25f, r_framedatasize.value * 2.0f, 128.0f));
		R_FrameData_Resize();
	}

	data = r_framedata_mem->data + r_framedata_mem->current;
	r_framedata_mem->current += size;

	// count the usage for stats
	r_refdef.stats.framedatacurrent = max(r_refdef.stats.framedatacurrent, (int)r_framedata_mem->current);
	r_refdef.stats.framedatasize = max(r_refdef.stats.framedatasize, (int)r_framedata_mem->size);

	return (void *)data;
}

void *R_FrameData_Store(size_t size, void *data)
{
	void *d = R_FrameData_Alloc(size);
	if (d && data)
		memcpy(d, data, size);
	return d;
}

void R_FrameData_SetMark(void)
{
	if (!r_framedata_mem)
		return;
	r_framedata_mem->mark = r_framedata_mem->current;
}

void R_FrameData_ReturnToMark(void)
{
	if (!r_framedata_mem)
		return;
	r_framedata_mem->current = r_framedata_mem->mark;
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
		ent->animcache_vertexmesh = NULL;
		ent->animcache_vertex3fbuffer = NULL;
		ent->animcache_vertexmeshbuffer = NULL;
	}
}

void R_AnimCache_UpdateEntityMeshBuffers(entity_render_t *ent, int numvertices)
{
	int i;

	// check if we need the meshbuffers
	if (!vid.useinterleavedarrays)
		return;

	if (!ent->animcache_vertexmesh && ent->animcache_normal3f)
		ent->animcache_vertexmesh = (r_vertexmesh_t *)R_FrameData_Alloc(sizeof(r_vertexmesh_t)*numvertices);
	// TODO: upload vertex3f buffer?
	if (ent->animcache_vertexmesh)
	{
		memcpy(ent->animcache_vertexmesh, ent->model->surfmesh.vertexmesh, sizeof(r_vertexmesh_t)*numvertices);
		for (i = 0;i < numvertices;i++)
			memcpy(ent->animcache_vertexmesh[i].vertex3f, ent->animcache_vertex3f + 3*i, sizeof(float[3]));
		if (ent->animcache_svector3f)
			for (i = 0;i < numvertices;i++)
				memcpy(ent->animcache_vertexmesh[i].svector3f, ent->animcache_svector3f + 3*i, sizeof(float[3]));
		if (ent->animcache_tvector3f)
			for (i = 0;i < numvertices;i++)
				memcpy(ent->animcache_vertexmesh[i].tvector3f, ent->animcache_tvector3f + 3*i, sizeof(float[3]));
		if (ent->animcache_normal3f)
			for (i = 0;i < numvertices;i++)
				memcpy(ent->animcache_vertexmesh[i].normal3f, ent->animcache_normal3f + 3*i, sizeof(float[3]));
		// TODO: upload vertexmeshbuffer?
	}
}

qboolean R_AnimCache_GetEntity(entity_render_t *ent, qboolean wantnormals, qboolean wanttangents)
{
	dp_model_t *model = ent->model;
	int numvertices;
	// see if it's already cached this frame
	if (ent->animcache_vertex3f)
	{
		// add normals/tangents if needed (this only happens with multiple views, reflections, cameras, etc)
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
					ent->animcache_normal3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
				if (wanttangents)
				{
					ent->animcache_svector3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
					ent->animcache_tvector3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
				}
				model->AnimateVertices(model, ent->frameblend, ent->skeleton, NULL, wantnormals ? ent->animcache_normal3f : NULL, wanttangents ? ent->animcache_svector3f : NULL, wanttangents ? ent->animcache_tvector3f : NULL);
				R_AnimCache_UpdateEntityMeshBuffers(ent, model->surfmesh.num_vertices);
			}
		}
	}
	else
	{
		// see if this ent is worth caching
		if (!model || !model->Draw || !model->surfmesh.isanimated || !model->AnimateVertices)
			return false;
		// get some memory for this entity and generate mesh data
		numvertices = model->surfmesh.num_vertices;
		ent->animcache_vertex3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
		if (wantnormals)
			ent->animcache_normal3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
		if (wanttangents)
		{
			ent->animcache_svector3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
			ent->animcache_tvector3f = (float *)R_FrameData_Alloc(sizeof(float[3])*numvertices);
		}
		model->AnimateVertices(model, ent->frameblend, ent->skeleton, ent->animcache_vertex3f, ent->animcache_normal3f, ent->animcache_svector3f, ent->animcache_tvector3f);
		R_AnimCache_UpdateEntityMeshBuffers(ent, model->surfmesh.num_vertices);
	}
	return true;
}

void R_AnimCache_CacheVisibleEntities(void)
{
	int i;
	qboolean wantnormals = true;
	qboolean wanttangents = !r_showsurfaces.integer;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		wanttangents = false;
		break;
	case RENDERPATH_SOFT:
		break;
	}

	if (r_shownormals.integer)
		wanttangents = wantnormals = true;

	// TODO: thread this
	// NOTE: R_PrepareRTLights() also caches entities

	for (i = 0;i < r_refdef.scene.numentities;i++)
		if (r_refdef.viewcache.entityvisible[i])
			R_AnimCache_GetEntity(r_refdef.scene.entities[i], wantnormals, wanttangents);
}

//==================================================================================

extern cvar_t r_overheadsprites_pushback;

static void R_View_UpdateEntityLighting (void)
{
	int i;
	entity_render_t *ent;
	vec3_t tempdiffusenormal, avg;
	vec_t f, fa, fd, fdd;
	qboolean skipunseen = r_shadows.integer != 1; //|| R_Shadow_ShadowMappingEnabled();

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
		if (ent->flags & RENDER_LIGHT)
		{
			vec3_t org;
			Matrix4x4_OriginFromMatrix(&ent->matrix, org);

			// complete lightning for lit sprites
			// todo: make a EF_ field so small ents could be lit purely by modellight and skipping real rtlight pass (like EF_NORTLIGHT)?
			if (ent->model->type == mod_sprite && !(ent->model->data_textures[0].basematerialflags & MATERIALFLAG_FULLBRIGHT))
			{
				if (ent->model->sprite.sprnum_type == SPR_OVERHEAD) // apply offset for overhead sprites
					org[2] = org[2] + r_overheadsprites_pushback.value;
				R_LightPoint(ent->modellight_ambient, org, LP_LIGHTMAP | LP_RTWORLD | LP_DYNLIGHT);
			}
			else
				R_CompleteLightPoint(ent->modellight_ambient, ent->modellight_diffuse, tempdiffusenormal, org, LP_LIGHTMAP);

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
					fa = 0.299f * ent->modellight_ambient[0] + 0.587f * ent->modellight_ambient[1] + 0.114f * ent->modellight_ambient[2];
					fd = 0.299f * ent->modellight_diffuse[0] + 0.587f * ent->modellight_diffuse[1] + 0.114f * ent->modellight_diffuse[2];
					f = fa + 0.25 * fd;
					if(f > 0)
					{
						// adjust brightness and saturation to target
						avg[0] = avg[1] = avg[2] = fa / f;
						VectorLerp(ent->modellight_ambient, r_equalize_entities_by.value, avg, ent->modellight_ambient);
						avg[0] = avg[1] = avg[2] = fd / f;
						VectorLerp(ent->modellight_diffuse, r_equalize_entities_by.value, avg, ent->modellight_diffuse);
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
	if (!r_drawexteriormodel.integer)
		renderimask |= RENDER_EXTERIORMODEL;
	if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.BoxTouchingVisibleLeafs)
	{
		// worldmodel can check visibility
		memset(r_refdef.viewcache.entityvisible, 0, r_refdef.scene.numentities);
		for (i = 0;i < r_refdef.scene.numentities;i++)
		{
			ent = r_refdef.scene.entities[i];
			if (!(ent->flags & renderimask))
			if (!R_CullBox(ent->mins, ent->maxs) || (ent->model && ent->model->type == mod_sprite && (ent->model->sprite.sprnum_type == SPR_LABEL || ent->model->sprite.sprnum_type == SPR_LABEL_SCALE)))
			if ((ent->flags & (RENDER_NODEPTHTEST | RENDER_VIEWMODEL)) || r_refdef.scene.worldmodel->brush.BoxTouchingVisibleLeafs(r_refdef.scene.worldmodel, r_refdef.viewcache.world_leafvisible, ent->mins, ent->maxs))
				r_refdef.viewcache.entityvisible[i] = true;
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
	if(r_cullentities_trace.integer && r_refdef.scene.worldmodel->brush.TraceLineOfSight && !r_refdef.view.useclipplane && !r_trippy.integer)
		// sorry, this check doesn't work for portal/reflection/refraction renders as the view origin is not useful for culling
	{
		for (i = 0;i < r_refdef.scene.numentities;i++)
		{
			if (!r_refdef.viewcache.entityvisible[i])
				continue;
			ent = r_refdef.scene.entities[i];
			if(!(ent->flags & (RENDER_VIEWMODEL | RENDER_NOCULL | RENDER_NODEPTHTEST)) && !(ent->model && (ent->model->name[0] == '*')))
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
		/*
		if (ent->model && !strncmp(ent->model->name, "models/proto_", 13))
		{
			vec3_t f, l, u, o;
			Matrix4x4_ToVectors(&ent->matrix, f, l, u, o);
			Con_Printf("R_DrawModels\n");
			Con_Printf("model %s O %f %f %f F %f %f %f L %f %f %f U %f %f %f\n", ent->model->name, o[0], o[1], o[2], f[0], f[1], f[2], l[0], l[1], l[2], u[0], u[1], u[2]);
			Con_Printf("group: %i %f %i %f %i %f %i %f\n", ent->framegroupblend[0].frame, ent->framegroupblend[0].lerp, ent->framegroupblend[1].frame, ent->framegroupblend[1].lerp, ent->framegroupblend[2].frame, ent->framegroupblend[2].lerp, ent->framegroupblend[3].frame, ent->framegroupblend[3].lerp);
			Con_Printf("blend: %i %f %i %f %i %f %i %f %i %f %i %f %i %f %i %f\n", ent->frameblend[0].subframe, ent->frameblend[0].lerp, ent->frameblend[1].subframe, ent->frameblend[1].lerp, ent->frameblend[2].subframe, ent->frameblend[2].lerp, ent->frameblend[3].subframe, ent->frameblend[3].lerp, ent->frameblend[4].subframe, ent->frameblend[4].lerp, ent->frameblend[5].subframe, ent->frameblend[5].lerp, ent->frameblend[6].subframe, ent->frameblend[6].lerp, ent->frameblend[7].subframe, ent->frameblend[7].lerp);
		}
		*/
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

void R_HDR_UpdateIrisAdaptation(const vec3_t point)
{
	if (r_hdr_irisadaptation.integer)
	{
		vec3_t ambient;
		vec3_t diffuse;
		vec3_t diffusenormal;
		vec_t brightness;
		vec_t goal;
		vec_t adjust;
		vec_t current;
		R_CompleteLightPoint(ambient, diffuse, diffusenormal, point, LP_LIGHTMAP | LP_RTWORLD | LP_DYNLIGHT);
		brightness = (ambient[0] + ambient[1] + ambient[2] + diffuse[0] + diffuse[1] + diffuse[2]) * (1.0f / 3.0f);
		brightness = max(0.0000001f, brightness);
		goal = r_hdr_irisadaptation_multiplier.value / brightness;
		goal = bound(r_hdr_irisadaptation_minvalue.value, goal, r_hdr_irisadaptation_maxvalue.value);
		adjust = r_hdr_irisadaptation_fade.value * cl.realframetime;
		current = r_hdr_irisadaptation_value.value;
		if (current < goal)
			current = min(current + adjust, goal);
		else if (current > goal)
			current = max(current - adjust, goal);
		if (fabs(r_hdr_irisadaptation_value.value - current) > 0.0001f)
			Cvar_SetValueQuick(&r_hdr_irisadaptation_value, current);
	}
	else if (r_hdr_irisadaptation_value.value != 1.0f)
		Cvar_SetValueQuick(&r_hdr_irisadaptation_value, 1.0f);
}

static void R_View_SetFrustum(const int *scissor)
{
	int i;
	double fpx = +1, fnx = -1, fpy = +1, fny = -1;
	vec3_t forward, left, up, origin, v;

	if(scissor)
	{
		// flipped x coordinates (because x points left here)
		fpx =  1.0 - 2.0 * (scissor[0]              - r_refdef.view.viewport.x) / (double) (r_refdef.view.viewport.width);
		fnx =  1.0 - 2.0 * (scissor[0] + scissor[2] - r_refdef.view.viewport.x) / (double) (r_refdef.view.viewport.width);

		// D3D Y coordinate is top to bottom, OpenGL is bottom to top, fix the D3D one
		switch(vid.renderpath)
		{
			case RENDERPATH_D3D9:
			case RENDERPATH_D3D10:
			case RENDERPATH_D3D11:
				// non-flipped y coordinates
				fny = -1.0 + 2.0 * (vid.height - scissor[1] - scissor[3] - r_refdef.view.viewport.y) / (double) (r_refdef.view.viewport.height);
				fpy = -1.0 + 2.0 * (vid.height - scissor[1]              - r_refdef.view.viewport.y) / (double) (r_refdef.view.viewport.height);
				break;
			case RENDERPATH_SOFT:
			case RENDERPATH_GL11:
			case RENDERPATH_GL13:
			case RENDERPATH_GL20:
			case RENDERPATH_GLES1:
			case RENDERPATH_GLES2:
				// non-flipped y coordinates
				fny = -1.0 + 2.0 * (scissor[1]              - r_refdef.view.viewport.y) / (double) (r_refdef.view.viewport.height);
				fpy = -1.0 + 2.0 * (scissor[1] + scissor[3] - r_refdef.view.viewport.y) / (double) (r_refdef.view.viewport.height);
				break;
		}
	}

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
		// calculate frustum corners, which are used to calculate deformed frustum planes for shadow caster culling
		VectorMAMAM(1024, forward, fnx * 1024.0 * r_refdef.view.frustum_x, left, fny * 1024.0 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[0]);
		VectorMAMAM(1024, forward, fpx * 1024.0 * r_refdef.view.frustum_x, left, fny * 1024.0 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[1]);
		VectorMAMAM(1024, forward, fnx * 1024.0 * r_refdef.view.frustum_x, left, fpy * 1024.0 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[2]);
		VectorMAMAM(1024, forward, fpx * 1024.0 * r_refdef.view.frustum_x, left, fpy * 1024.0 * r_refdef.view.frustum_y, up, r_refdef.view.frustumcorner[3]);

		// then the normals from the corners relative to origin
		CrossProduct(r_refdef.view.frustumcorner[2], r_refdef.view.frustumcorner[0], r_refdef.view.frustum[0].normal);
		CrossProduct(r_refdef.view.frustumcorner[1], r_refdef.view.frustumcorner[3], r_refdef.view.frustum[1].normal);
		CrossProduct(r_refdef.view.frustumcorner[0], r_refdef.view.frustumcorner[1], r_refdef.view.frustum[2].normal);
		CrossProduct(r_refdef.view.frustumcorner[3], r_refdef.view.frustumcorner[2], r_refdef.view.frustum[3].normal);

		// in a NORMAL view, forward cross left == up
		// in a REFLECTED view, forward cross left == down
		// so our cross products above need to be adjusted for a left handed coordinate system
		CrossProduct(forward, left, v);
		if(DotProduct(v, up) < 0)
		{
			VectorNegate(r_refdef.view.frustum[0].normal, r_refdef.view.frustum[0].normal);
			VectorNegate(r_refdef.view.frustum[1].normal, r_refdef.view.frustum[1].normal);
			VectorNegate(r_refdef.view.frustum[2].normal, r_refdef.view.frustum[2].normal);
			VectorNegate(r_refdef.view.frustum[3].normal, r_refdef.view.frustum[3].normal);
		}

		// Leaving those out was a mistake, those were in the old code, and they
		// fix a reproducable bug in this one: frustum culling got fucked up when viewmatrix was an identity matrix
		// I couldn't reproduce it after adding those normalizations. --blub
		VectorNormalize(r_refdef.view.frustum[0].normal);
		VectorNormalize(r_refdef.view.frustum[1].normal);
		VectorNormalize(r_refdef.view.frustum[2].normal);
		VectorNormalize(r_refdef.view.frustum[3].normal);

		// make the corners absolute
		VectorAdd(r_refdef.view.frustumcorner[0], r_refdef.view.origin, r_refdef.view.frustumcorner[0]);
		VectorAdd(r_refdef.view.frustumcorner[1], r_refdef.view.origin, r_refdef.view.frustumcorner[1]);
		VectorAdd(r_refdef.view.frustumcorner[2], r_refdef.view.origin, r_refdef.view.frustumcorner[2]);
		VectorAdd(r_refdef.view.frustumcorner[3], r_refdef.view.origin, r_refdef.view.frustumcorner[3]);

		// one more normal
		VectorCopy(forward, r_refdef.view.frustum[4].normal);

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

void R_View_UpdateWithScissor(const int *myscissor)
{
	R_Main_ResizeViewCache();
	R_View_SetFrustum(myscissor);
	R_View_WorldVisibility(r_refdef.view.useclipplane);
	R_View_UpdateEntityVisible();
	R_View_UpdateEntityLighting();
}

void R_View_Update(void)
{
	R_Main_ResizeViewCache();
	R_View_SetFrustum(NULL);
	R_View_WorldVisibility(r_refdef.view.useclipplane);
	R_View_UpdateEntityVisible();
	R_View_UpdateEntityLighting();
}

float viewscalefpsadjusted = 1.0f;

void R_GetScaledViewSize(int width, int height, int *outwidth, int *outheight)
{
	float scale = r_viewscale.value * sqrt(viewscalefpsadjusted);
	scale = bound(0.03125f, scale, 1.0f);
	*outwidth = (int)ceil(width * scale);
	*outheight = (int)ceil(height * scale);
}

void R_Mesh_SetMainRenderTargets(void)
{
	if (r_bloomstate.fbo_framebuffer)
		R_Mesh_SetRenderTargets(r_bloomstate.fbo_framebuffer, r_bloomstate.texture_framebufferdepth, r_bloomstate.texture_framebuffercolor, NULL, NULL, NULL);
	else
		R_Mesh_ResetRenderTargets();
}

void R_SetupView(qboolean allowwaterclippingplane)
{
	const float *customclipplane = NULL;
	float plane[4];
	int scaledwidth, scaledheight;
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
		plane[3] = -dist;
		if(vid.renderpath != RENDERPATH_SOFT) customclipplane = plane;
	}

	R_GetScaledViewSize(r_refdef.view.width, r_refdef.view.height, &scaledwidth, &scaledheight);
	if (!r_refdef.view.useperspective)
		R_Viewport_InitOrtho(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - scaledheight - r_refdef.view.y, scaledwidth, scaledheight, -r_refdef.view.ortho_x, -r_refdef.view.ortho_y, r_refdef.view.ortho_x, r_refdef.view.ortho_y, -r_refdef.farclip, r_refdef.farclip, customclipplane);
	else if (vid.stencil && r_useinfinitefarclip.integer)
		R_Viewport_InitPerspectiveInfinite(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - scaledheight - r_refdef.view.y, scaledwidth, scaledheight, r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip, customclipplane);
	else
		R_Viewport_InitPerspective(&r_refdef.view.viewport, &r_refdef.view.matrix, r_refdef.view.x, vid.height - scaledheight - r_refdef.view.y, scaledwidth, scaledheight, r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip, r_refdef.farclip, customclipplane);
	R_Mesh_SetMainRenderTargets();
	R_SetViewport(&r_refdef.view.viewport);
	if (r_refdef.view.useclipplane && allowwaterclippingplane && vid.renderpath == RENDERPATH_SOFT)
	{
		matrix4x4_t mvpmatrix, invmvpmatrix, invtransmvpmatrix;
		float screenplane[4];
		Matrix4x4_Concat(&mvpmatrix, &r_refdef.view.viewport.projectmatrix, &r_refdef.view.viewport.viewmatrix);
		Matrix4x4_Invert_Full(&invmvpmatrix, &mvpmatrix);
		Matrix4x4_Transpose(&invtransmvpmatrix, &invmvpmatrix);
		Matrix4x4_Transform4(&invtransmvpmatrix, plane, screenplane);
		DPSOFTRAST_ClipPlane(screenplane[0], screenplane[1], screenplane[2], screenplane[3]);
	}
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
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			hlslVSSetParameter16f(D3DVSREGISTER_ModelViewProjectionMatrix, gl_modelviewprojection16f);
			hlslVSSetParameter16f(D3DVSREGISTER_ModelViewMatrix, gl_modelview16f);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 shader %s:%i\n", __FILE__, __LINE__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 shader %s:%i\n", __FILE__, __LINE__);
			break;
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
			qglLoadMatrixf(gl_modelview16f);CHECKGLERROR
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelViewProjectionMatrixM1, 1, false, gl_modelviewprojection16f);
			DPSOFTRAST_UniformMatrix4fv(DPSOFTRAST_UNIFORM_ModelViewMatrixM1, 1, false, gl_modelview16f);
			break;
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			if (r_glsl_permutation && r_glsl_permutation->loc_ModelViewProjectionMatrix >= 0) qglUniformMatrix4fv(r_glsl_permutation->loc_ModelViewProjectionMatrix, 1, false, gl_modelviewprojection16f);
			if (r_glsl_permutation && r_glsl_permutation->loc_ModelViewMatrix >= 0) qglUniformMatrix4fv(r_glsl_permutation->loc_ModelViewMatrix, 1, false, gl_modelview16f);
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
	R_Mesh_ResetRenderTargets();
	R_SetViewport(&viewport);
	GL_Scissor(viewport.x, viewport.y, viewport.width, viewport.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_ScissorTest(false);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_DepthTest(false);
	GL_DepthFunc(GL_LEQUAL);
	R_EntityMatrix(&identitymatrix);
	R_Mesh_ResetTextureState();
	GL_PolygonOffset(0, 0);
	R_SetStencil(false, 255, GL_KEEP, GL_KEEP, GL_KEEP, GL_ALWAYS, 128, 255);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		qglEnable(GL_POLYGON_OFFSET_FILL);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	}
	GL_CullFace(GL_NONE);
}

void R_ResetViewRendering3D(void)
{
	DrawQ_Finish();

	R_SetupView(true);
	GL_Scissor(r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	GL_DepthRange(0, 1);
	GL_DepthTest(true);
	GL_DepthFunc(GL_LEQUAL);
	R_EntityMatrix(&identitymatrix);
	R_Mesh_ResetTextureState();
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	R_SetStencil(false, 255, GL_KEEP, GL_KEEP, GL_KEEP, GL_ALWAYS, 128, 255);
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		qglEnable(GL_POLYGON_OFFSET_FILL);CHECKGLERROR
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	}
	GL_CullFace(r_refdef.view.cullface_back);
}

/*
================
R_RenderView_UpdateViewVectors
================
*/
static void R_RenderView_UpdateViewVectors(void)
{
	// break apart the view matrix into vectors for various purposes
	// it is important that this occurs outside the RenderScene function because that can be called from reflection renders, where the vectors come out wrong
	// however the r_refdef.view.origin IS updated in RenderScene intentionally - otherwise the sky renders at the wrong origin, etc
	Matrix4x4_ToVectors(&r_refdef.view.matrix, r_refdef.view.forward, r_refdef.view.left, r_refdef.view.up, r_refdef.view.origin);
	VectorNegate(r_refdef.view.left, r_refdef.view.right);
	// make an inverted copy of the view matrix for tracking sprites
	Matrix4x4_Invert_Simple(&r_refdef.view.inverse_matrix, &r_refdef.view.matrix);
}

void R_RenderScene(void);
void R_RenderWaterPlanes(void);

static void R_Water_StartFrame(void)
{
	int i;
	int waterwidth, waterheight, texturewidth, textureheight, camerawidth, cameraheight;
	r_waterstate_waterplane_t *p;

	if (vid.width > (int)vid.maxtexturesize_2d || vid.height > (int)vid.maxtexturesize_2d)
		return;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		return;
	}

	// set waterwidth and waterheight to the water resolution that will be
	// used (often less than the screen resolution for faster rendering)
	R_GetScaledViewSize(bound(1, vid.width * r_water_resolutionmultiplier.value, vid.width), bound(1, vid.height * r_water_resolutionmultiplier.value, vid.height), &waterwidth, &waterheight);

	// calculate desired texture sizes
	// can't use water if the card does not support the texture size
	if (!r_water.integer || r_showsurfaces.integer)
		texturewidth = textureheight = waterwidth = waterheight = camerawidth = cameraheight = 0;
	else if (vid.support.arb_texture_non_power_of_two)
	{
		texturewidth = waterwidth;
		textureheight = waterheight;
		camerawidth = waterwidth;
		cameraheight = waterheight;
	}
	else
	{
		for (texturewidth   = 1;texturewidth   < waterwidth ;texturewidth   *= 2);
		for (textureheight  = 1;textureheight  < waterheight;textureheight  *= 2);
		for (camerawidth    = 1;camerawidth   <= waterwidth; camerawidth    *= 2); camerawidth  /= 2;
		for (cameraheight   = 1;cameraheight  <= waterheight;cameraheight   *= 2); cameraheight /= 2;
	}

	// allocate textures as needed
	if (r_waterstate.texturewidth != texturewidth || r_waterstate.textureheight != textureheight || r_waterstate.camerawidth != camerawidth || r_waterstate.cameraheight != cameraheight)
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
			if (p->texture_camera)
				R_FreeTexture(p->texture_camera);
			p->texture_camera = NULL;
		}
		memset(&r_waterstate, 0, sizeof(r_waterstate));
		r_waterstate.texturewidth = texturewidth;
		r_waterstate.textureheight = textureheight;
		r_waterstate.camerawidth = camerawidth;
		r_waterstate.cameraheight = cameraheight;
	}

	if (r_waterstate.texturewidth)
	{
		int scaledwidth, scaledheight;

		r_waterstate.enabled = true;

		// when doing a reduced render (HDR) we want to use a smaller area
		r_waterstate.waterwidth = (int)bound(1, r_refdef.view.width * r_water_resolutionmultiplier.value, r_refdef.view.width);
		r_waterstate.waterheight = (int)bound(1, r_refdef.view.height * r_water_resolutionmultiplier.value, r_refdef.view.height);
		R_GetScaledViewSize(r_waterstate.waterwidth, r_waterstate.waterheight, &scaledwidth, &scaledheight);

		// set up variables that will be used in shader setup
		r_waterstate.screenscale[0] = 0.5f * (float)scaledwidth / (float)r_waterstate.texturewidth;
		r_waterstate.screenscale[1] = 0.5f * (float)scaledheight / (float)r_waterstate.textureheight;
		r_waterstate.screencenter[0] = 0.5f * (float)scaledwidth / (float)r_waterstate.texturewidth;
		r_waterstate.screencenter[1] = 0.5f * (float)scaledheight / (float)r_waterstate.textureheight;
	}

	r_waterstate.maxwaterplanes = MAX_WATERPLANES;
	r_waterstate.numwaterplanes = 0;
}

void R_Water_AddWaterPlane(msurface_t *surface, int entno)
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
		if(p->camera_entity == t->camera_entity)
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
		p->camera_entity = t->camera_entity;
		VectorCopy(surface->mins, p->mins);
		VectorCopy(surface->maxs, p->maxs);
	}
	else
	{
		// merge mins/maxs
		p->mins[0] = min(p->mins[0], surface->mins[0]);
		p->mins[1] = min(p->mins[1], surface->mins[1]);
		p->mins[2] = min(p->mins[2], surface->mins[2]);
		p->maxs[0] = max(p->maxs[0], surface->maxs[0]);
		p->maxs[1] = max(p->maxs[1], surface->maxs[1]);
		p->maxs[2] = max(p->maxs[2], surface->maxs[2]);
	}
	// merge this surface's materialflags into the waterplane
	p->materialflags |= t->currentmaterialflags;
	if(!(p->materialflags & MATERIALFLAG_CAMERA))
	{
		// merge this surface's PVS into the waterplane
		VectorMAM(0.5f, surface->mins, 0.5f, surface->maxs, center);
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA) && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.FatPVS
		 && r_refdef.scene.worldmodel->brush.PointInLeaf && r_refdef.scene.worldmodel->brush.PointInLeaf(r_refdef.scene.worldmodel, center)->clusterindex >= 0)
		{
			r_refdef.scene.worldmodel->brush.FatPVS(r_refdef.scene.worldmodel, center, 2, p->pvsbits, sizeof(p->pvsbits), p->pvsvalid);
			p->pvsvalid = true;
		}
	}
}

extern cvar_t r_drawparticles;
extern cvar_t r_drawdecals;

static void R_Water_ProcessPlanes(void)
{
	int myscissor[4];
	r_refdef_view_t originalview;
	r_refdef_view_t myview;
	int planeindex, qualityreduction = 0, old_r_dynamic = 0, old_r_shadows = 0, old_r_worldrtlight = 0, old_r_dlight = 0, old_r_particles = 0, old_r_decals = 0;
	r_waterstate_waterplane_t *p;
	vec3_t visorigin;

	originalview = r_refdef.view;

	// lowquality hack, temporarily shut down some cvars and restore afterwards
	qualityreduction = r_water_lowquality.integer;
	if (qualityreduction > 0)
	{
		if (qualityreduction >= 1)
		{
			old_r_shadows = r_shadows.integer;
			old_r_worldrtlight = r_shadow_realtime_world.integer;
			old_r_dlight = r_shadow_realtime_dlight.integer;
			Cvar_SetValueQuick(&r_shadows, 0);
			Cvar_SetValueQuick(&r_shadow_realtime_world, 0);
			Cvar_SetValueQuick(&r_shadow_realtime_dlight, 0);
		}
		if (qualityreduction >= 2)
		{
			old_r_dynamic = r_dynamic.integer;
			old_r_particles = r_drawparticles.integer;
			old_r_decals = r_drawdecals.integer;
			Cvar_SetValueQuick(&r_dynamic, 0);
			Cvar_SetValueQuick(&r_drawparticles, 0);
			Cvar_SetValueQuick(&r_drawdecals, 0);
		}
	}

	// make sure enough textures are allocated
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			if (!p->texture_refraction)
				p->texture_refraction = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_refraction", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_RENDERTARGET | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
			if (!p->texture_refraction)
				goto error;
		}
		else if (p->materialflags & MATERIALFLAG_CAMERA)
		{
			if (!p->texture_camera)
				p->texture_camera = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_camera", planeindex), r_waterstate.camerawidth, r_waterstate.cameraheight, NULL, TEXTYPE_COLORBUFFER, TEXF_RENDERTARGET | TEXF_FORCELINEAR, -1, NULL);
			if (!p->texture_camera)
				goto error;
		}

		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION))
		{
			if (!p->texture_reflection)
				p->texture_reflection = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_reflection", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_COLORBUFFER, TEXF_RENDERTARGET | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
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
			if(r_water_scissormode.integer)
			{
				R_SetupView(true);
				if(R_ScissorForBBox(p->mins, p->maxs, myscissor))
					continue; // FIXME the plane then still may get rendered but with broken texture, but it sure won't be visible
			}

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
			if(r_water_scissormode.integer & 2)
				R_View_UpdateWithScissor(myscissor);
			else
				R_View_Update();
			if(r_water_scissormode.integer & 1)
				GL_Scissor(myscissor[0], myscissor[1], myscissor[2], myscissor[3]);
			R_RenderScene();

			R_Mesh_CopyToTexture(p->texture_reflection, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
		}

		// render the normal view scene and copy into texture
		// (except that a clipping plane should be used to hide everything on one side of the water, and the viewer's weapon model should be omitted)
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			r_refdef.view = myview;
			if(r_water_scissormode.integer)
			{
				R_SetupView(true);
				if(R_ScissorForBBox(p->mins, p->maxs, myscissor))
					continue; // FIXME the plane then still may get rendered but with broken texture, but it sure won't be visible
			}

			r_waterstate.renderingrefraction = true;

			r_refdef.view.clipplane = p->plane;
			VectorNegate(r_refdef.view.clipplane.normal, r_refdef.view.clipplane.normal);
			r_refdef.view.clipplane.dist = -r_refdef.view.clipplane.dist;

			if((p->materialflags & MATERIALFLAG_CAMERA) && p->camera_entity)
			{
				// we need to perform a matrix transform to render the view... so let's get the transformation matrix
				r_waterstate.renderingrefraction = false; // we don't want to hide the player model from these ones
				CL_VM_TransformView(p->camera_entity - MAX_EDICTS, &r_refdef.view.matrix, &r_refdef.view.clipplane, visorigin);
				R_RenderView_UpdateViewVectors();
				if(r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.FatPVS)
				{
					r_refdef.view.usecustompvs = true;
					r_refdef.scene.worldmodel->brush.FatPVS(r_refdef.scene.worldmodel, visorigin, 2, r_refdef.viewcache.world_pvsbits, (r_refdef.viewcache.world_numclusters+7)>>3, false);
				}
			}

			PlaneClassify(&r_refdef.view.clipplane);

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			if(r_water_scissormode.integer & 2)
				R_View_UpdateWithScissor(myscissor);
			else
				R_View_Update();
			if(r_water_scissormode.integer & 1)
				GL_Scissor(myscissor[0], myscissor[1], myscissor[2], myscissor[3]);
			R_RenderScene();

			R_Mesh_CopyToTexture(p->texture_refraction, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
			r_waterstate.renderingrefraction = false;
		}
		else if (p->materialflags & MATERIALFLAG_CAMERA)
		{
			r_refdef.view = myview;

			r_refdef.view.clipplane = p->plane;
			VectorNegate(r_refdef.view.clipplane.normal, r_refdef.view.clipplane.normal);
			r_refdef.view.clipplane.dist = -r_refdef.view.clipplane.dist;

			r_refdef.view.width = r_waterstate.camerawidth;
			r_refdef.view.height = r_waterstate.cameraheight;
			r_refdef.view.frustum_x = 1; // tan(45 * M_PI / 180.0);
			r_refdef.view.frustum_y = 1; // tan(45 * M_PI / 180.0);

			if(p->camera_entity)
			{
				// we need to perform a matrix transform to render the view... so let's get the transformation matrix
				CL_VM_TransformView(p->camera_entity - MAX_EDICTS, &r_refdef.view.matrix, &r_refdef.view.clipplane, visorigin);
			}

			// note: all of the view is used for displaying... so
			// there is no use in scissoring

			// reverse the cullface settings for this render
			r_refdef.view.cullface_front = GL_FRONT;
			r_refdef.view.cullface_back = GL_BACK;
			// also reverse the view matrix
			Matrix4x4_ConcatScale3(&r_refdef.view.matrix, 1, 1, -1); // this serves to invert texcoords in the result, as the copied texture is mapped the wrong way round
			R_RenderView_UpdateViewVectors();
			if(p->camera_entity && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.FatPVS)
			{
				r_refdef.view.usecustompvs = true;
				r_refdef.scene.worldmodel->brush.FatPVS(r_refdef.scene.worldmodel, visorigin, 2, r_refdef.viewcache.world_pvsbits, (r_refdef.viewcache.world_numclusters+7)>>3, false);
			}
			
			// camera needs no clipplane
			r_refdef.view.useclipplane = false;

			PlaneClassify(&r_refdef.view.clipplane);

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			R_View_Update();
			R_RenderScene();

			R_Mesh_CopyToTexture(p->texture_camera, 0, 0, r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
			r_waterstate.renderingrefraction = false;
		}

	}
	if(vid.renderpath==RENDERPATH_SOFT) DPSOFTRAST_ClipPlane(0, 0, 0, 1);
	r_waterstate.renderingscene = false;
	r_refdef.view = originalview;
	R_ResetViewRendering3D();
	R_ClearScreen(r_refdef.fogenabled);
	R_View_Update();
	goto finish;
error:
	r_refdef.view = originalview;
	r_waterstate.renderingscene = false;
	Cvar_SetValueQuick(&r_water, 0);
	Con_Printf("R_Water_ProcessPlanes: Error: texture creation failed!  Turned off r_water.\n");
finish:
	// lowquality hack, restore cvars
	if (qualityreduction > 0)
	{
		if (qualityreduction >= 1)
		{
			Cvar_SetValueQuick(&r_shadows, old_r_shadows);
			Cvar_SetValueQuick(&r_shadow_realtime_world, old_r_worldrtlight);
			Cvar_SetValueQuick(&r_shadow_realtime_dlight, old_r_dlight);
		}
		if (qualityreduction >= 2)
		{
			Cvar_SetValueQuick(&r_dynamic, old_r_dynamic);
			Cvar_SetValueQuick(&r_drawparticles, old_r_particles);
			Cvar_SetValueQuick(&r_drawdecals, old_r_decals);
		}
	}
}

void R_Bloom_StartFrame(void)
{
	int bloomtexturewidth, bloomtextureheight, screentexturewidth, screentextureheight;
	int viewwidth, viewheight;
	textype_t textype;

	if (r_viewscale_fpsscaling.integer)
	{
		double actualframetime;
		double targetframetime;
		double adjust;
		actualframetime = r_refdef.lastdrawscreentime;
		targetframetime = (1.0 / r_viewscale_fpsscaling_target.value);
		adjust = (targetframetime - actualframetime) * r_viewscale_fpsscaling_multiply.value;
		adjust = bound(-r_viewscale_fpsscaling_stepmax.value, adjust, r_viewscale_fpsscaling_stepmax.value);
		if (r_viewscale_fpsscaling_stepsize.value > 0)
			adjust = (int)(adjust / r_viewscale_fpsscaling_stepsize.value) * r_viewscale_fpsscaling_stepsize.value;
		viewscalefpsadjusted += adjust;
		viewscalefpsadjusted = bound(r_viewscale_fpsscaling_min.value, viewscalefpsadjusted, 1.0f);
	}
	else
		viewscalefpsadjusted = 1.0f;

	R_GetScaledViewSize(r_refdef.view.width, r_refdef.view.height, &viewwidth, &viewheight);

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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
		screentexturewidth = vid.width;
		screentextureheight = vid.height;
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

	if (!(r_glsl_postprocess.integer || (!R_Stereo_ColorMasking() && r_glsl_saturation.value != 1) || (v_glslgamma.integer && !vid_gammatables_trivial)) && !r_bloom.integer && !r_hdr.integer && (R_Stereo_Active() || (r_motionblur.value <= 0 && r_damageblur.value <= 0)) && r_viewfbo.integer < 1 && r_viewscale.value == 1.0f && !r_viewscale_fpsscaling.integer)
		screentexturewidth = screentextureheight = 0;
	if (!r_hdr.integer && !r_bloom.integer)
		bloomtexturewidth = bloomtextureheight = 0;

	textype = TEXTYPE_COLORBUFFER;
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_GLES2:
		if (vid.support.ext_framebuffer_object)
		{
			if (r_viewfbo.integer == 2) textype = TEXTYPE_COLORBUFFER16F;
			if (r_viewfbo.integer == 3) textype = TEXTYPE_COLORBUFFER32F;
		}
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
		break;
	}

	// allocate textures as needed
	if (r_bloomstate.screentexturewidth != screentexturewidth
	 || r_bloomstate.screentextureheight != screentextureheight
	 || r_bloomstate.bloomtexturewidth != bloomtexturewidth
	 || r_bloomstate.bloomtextureheight != bloomtextureheight
	 || r_bloomstate.texturetype != textype
	 || r_bloomstate.viewfbo != r_viewfbo.integer)
	{
		if (r_bloomstate.texture_bloom)
			R_FreeTexture(r_bloomstate.texture_bloom);
		r_bloomstate.texture_bloom = NULL;
		if (r_bloomstate.texture_screen)
			R_FreeTexture(r_bloomstate.texture_screen);
		r_bloomstate.texture_screen = NULL;
		if (r_bloomstate.fbo_framebuffer)
			R_Mesh_DestroyFramebufferObject(r_bloomstate.fbo_framebuffer);
		r_bloomstate.fbo_framebuffer = 0;
		if (r_bloomstate.texture_framebuffercolor)
			R_FreeTexture(r_bloomstate.texture_framebuffercolor);
		r_bloomstate.texture_framebuffercolor = NULL;
		if (r_bloomstate.texture_framebufferdepth)
			R_FreeTexture(r_bloomstate.texture_framebufferdepth);
		r_bloomstate.texture_framebufferdepth = NULL;
		r_bloomstate.screentexturewidth = screentexturewidth;
		r_bloomstate.screentextureheight = screentextureheight;
		if (r_bloomstate.screentexturewidth && r_bloomstate.screentextureheight)
			r_bloomstate.texture_screen = R_LoadTexture2D(r_main_texturepool, "screen", r_bloomstate.screentexturewidth, r_bloomstate.screentextureheight, NULL, textype, TEXF_RENDERTARGET | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
		if (r_viewfbo.integer >= 1 && vid.support.ext_framebuffer_object)
		{
			// FIXME: choose depth bits based on a cvar
			r_bloomstate.texture_framebufferdepth = R_LoadTextureShadowMap2D(r_main_texturepool, "framebufferdepth", r_bloomstate.screentexturewidth, r_bloomstate.screentextureheight, 24, false);
			r_bloomstate.texture_framebuffercolor = R_LoadTexture2D(r_main_texturepool, "framebuffercolor", r_bloomstate.screentexturewidth, r_bloomstate.screentextureheight, NULL, textype, TEXF_RENDERTARGET | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
			r_bloomstate.fbo_framebuffer = R_Mesh_CreateFramebufferObject(r_bloomstate.texture_framebufferdepth, r_bloomstate.texture_framebuffercolor, NULL, NULL, NULL);
			R_Mesh_SetRenderTargets(r_bloomstate.fbo_framebuffer, r_bloomstate.texture_framebufferdepth, r_bloomstate.texture_framebuffercolor, NULL, NULL, NULL);
			// render depth into one texture and normalmap into the other
			if (qglDrawBuffer)
			{
				int status;
				qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);CHECKGLERROR
				qglReadBuffer(GL_COLOR_ATTACHMENT0_EXT);CHECKGLERROR
				status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
				if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
					Con_Printf("R_Bloom_StartFrame: glCheckFramebufferStatusEXT returned %i\n", status);
			}
		}
		r_bloomstate.bloomtexturewidth = bloomtexturewidth;
		r_bloomstate.bloomtextureheight = bloomtextureheight;
		if (r_bloomstate.bloomtexturewidth && r_bloomstate.bloomtextureheight)
			r_bloomstate.texture_bloom = R_LoadTexture2D(r_main_texturepool, "bloom", r_bloomstate.bloomtexturewidth, r_bloomstate.bloomtextureheight, NULL, textype, TEXF_RENDERTARGET | TEXF_FORCELINEAR | TEXF_CLAMP, -1, NULL);
		r_bloomstate.viewfbo = r_viewfbo.integer;
		r_bloomstate.texturetype = textype;
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
	r_bloomstate.screentexcoord2f[1] = (float)viewheight    / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[2] = (float)viewwidth     / (float)r_bloomstate.screentexturewidth;
	r_bloomstate.screentexcoord2f[3] = (float)viewheight    / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[4] = (float)viewwidth     / (float)r_bloomstate.screentexturewidth;
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

	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		{
			int i;
			for (i = 0;i < 4;i++)
			{
				r_bloomstate.screentexcoord2f[i*2+0] += 0.5f / (float)r_bloomstate.screentexturewidth;
				r_bloomstate.screentexcoord2f[i*2+1] += 0.5f / (float)r_bloomstate.screentextureheight;
				r_bloomstate.bloomtexcoord2f[i*2+0] += 0.5f / (float)r_bloomstate.bloomtexturewidth;
				r_bloomstate.bloomtexcoord2f[i*2+1] += 0.5f / (float)r_bloomstate.bloomtextureheight;
			}
		}
		break;
	}

	if ((r_hdr.integer || r_bloom.integer) && r_bloomstate.bloomwidth)
	{
		r_bloomstate.enabled = true;
		r_bloomstate.hdr = r_hdr.integer != 0 && !r_bloomstate.fbo_framebuffer;
	}

	R_Viewport_InitOrtho(&r_bloomstate.viewport, &identitymatrix, r_refdef.view.x, vid.height - r_bloomstate.bloomheight - r_refdef.view.y, r_bloomstate.bloomwidth, r_bloomstate.bloomheight, 0, 0, 1, 1, -10, 100, NULL);

	if (r_bloomstate.fbo_framebuffer)
		r_refdef.view.clear = true;
}

void R_Bloom_CopyBloomTexture(float colorscale)
{
	r_refdef.stats.bloom++;

	// scale down screen texture to the bloom texture size
	CHECKGLERROR
	R_Mesh_SetMainRenderTargets();
	R_SetViewport(&r_bloomstate.viewport);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_Color(colorscale, colorscale, colorscale, 1);
	// D3D has upside down Y coords, the easiest way to flip this is to flip the screen vertices rather than the texcoords, so we just use a different array for that...
	switch(vid.renderpath)
	{
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GL20:
	case RENDERPATH_GLES1:
	case RENDERPATH_GLES2:
	case RENDERPATH_SOFT:
		R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, r_bloomstate.screentexcoord2f);
		break;
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
		R_Mesh_PrepareVertices_Generic_Arrays(4, r_d3dscreenvertex3f, NULL, r_bloomstate.screentexcoord2f);
		break;
	}
	// TODO: do boxfilter scale-down in shader?
	R_SetupShader_Generic(r_bloomstate.texture_screen, NULL, GL_MODULATE, 1, true);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
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

	// we have a bloom image in the framebuffer
	CHECKGLERROR
	R_SetViewport(&r_bloomstate.viewport);

	for (x = 1;x < min(r_bloom_colorexponent.value, 32);)
	{
		x *= 2;
		r = bound(0, r_bloom_colorexponent.value / x, 1);
		GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		GL_Color(r,r,r,1);
		R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, r_bloomstate.bloomtexcoord2f);
		R_SetupShader_Generic(r_bloomstate.texture_bloom, NULL, GL_MODULATE, 1, true);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

		// copy the vertically blurred bloom view to a texture
		R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
		r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
	}

	range = r_bloom_blur.integer * r_bloomstate.bloomwidth / 320;
	brighten = r_bloom_brighten.value;
	if (r_bloomstate.hdr)
		brighten *= r_hdr_range.value;
	brighten = sqrt(brighten);
	if(range >= 1)
		brighten *= (3 * range) / (2 * range - 1); // compensate for the "dot particle"
	R_SetupShader_Generic(r_bloomstate.texture_bloom, NULL, GL_MODULATE, 1, true);

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
			R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, r_bloomstate.offsettexcoord2f);
			R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
			r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}

		// copy the vertically blurred bloom view to a texture
		R_Mesh_CopyToTexture(r_bloomstate.texture_bloom, 0, 0, r_bloomstate.viewport.x, r_bloomstate.viewport.y, r_bloomstate.viewport.width, r_bloomstate.viewport.height);
		r_refdef.stats.bloom_copypixels += r_bloomstate.viewport.width * r_bloomstate.viewport.height;
	}
}

void R_HDR_RenderBloomTexture(void)
{
	int oldwidth, oldheight;
	float oldcolorscale;
	qboolean oldwaterstate;

	oldwaterstate = r_waterstate.enabled;
	oldcolorscale = r_refdef.view.colorscale;
	oldwidth = r_refdef.view.width;
	oldheight = r_refdef.view.height;
	r_refdef.view.width = r_bloomstate.bloomwidth;
	r_refdef.view.height = r_bloomstate.bloomheight;

	if(r_hdr.integer < 2)
		r_waterstate.enabled = false;

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
	if (r_waterstate.enabled)
		R_RenderWaterPlanes();

	r_refdef.view.showdebug = true;
	R_RenderScene();
	r_waterstate.numwaterplanes = 0;

	R_ResetViewRendering2D();

	R_Bloom_CopyHDRTexture();
	R_Bloom_MakeTexture();

	// restore the view settings
	r_waterstate.enabled = oldwaterstate;
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
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
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
			R_Mesh_SetMainRenderTargets();

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
				if (cl.motionbluralpha > 0 && !r_refdef.envmap)
				{
					GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					GL_Color(1, 1, 1, cl.motionbluralpha);
					switch(vid.renderpath)
					{
					case RENDERPATH_GL11:
					case RENDERPATH_GL13:
					case RENDERPATH_GL20:
					case RENDERPATH_GLES1:
					case RENDERPATH_GLES2:
					case RENDERPATH_SOFT:
						R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, r_bloomstate.screentexcoord2f);
						break;
					case RENDERPATH_D3D9:
					case RENDERPATH_D3D10:
					case RENDERPATH_D3D11:
						R_Mesh_PrepareVertices_Generic_Arrays(4, r_d3dscreenvertex3f, NULL, r_bloomstate.screentexcoord2f);
						break;
					}
					R_SetupShader_Generic(r_bloomstate.texture_screen, NULL, GL_MODULATE, 1, true);
					R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
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
				GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
				R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, NULL);
				R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
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
		if (r_glsl_postprocess_uservec1_enable.integer)
			sscanf(r_glsl_postprocess_uservec1.string, "%f %f %f %f", &uservecs[0][0], &uservecs[0][1], &uservecs[0][2], &uservecs[0][3]);
		if (r_glsl_postprocess_uservec2_enable.integer)
			sscanf(r_glsl_postprocess_uservec2.string, "%f %f %f %f", &uservecs[1][0], &uservecs[1][1], &uservecs[1][2], &uservecs[1][3]);
		if (r_glsl_postprocess_uservec3_enable.integer)
			sscanf(r_glsl_postprocess_uservec3.string, "%f %f %f %f", &uservecs[2][0], &uservecs[2][1], &uservecs[2][2], &uservecs[2][3]);
		if (r_glsl_postprocess_uservec4_enable.integer)
			sscanf(r_glsl_postprocess_uservec4.string, "%f %f %f %f", &uservecs[3][0], &uservecs[3][1], &uservecs[3][2], &uservecs[3][3]);

		R_ResetViewRendering2D();
		GL_Color(1, 1, 1, 1);
		GL_BlendFunc(GL_ONE, GL_ZERO);

		switch(vid.renderpath)
		{
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			R_Mesh_PrepareVertices_Mesh_Arrays(4, r_screenvertex3f, NULL, NULL, NULL, NULL, r_bloomstate.screentexcoord2f, r_bloomstate.bloomtexcoord2f);
			R_SetupShader_SetPermutationGLSL(SHADERMODE_POSTPROCESS, permutation);
			if (r_glsl_permutation->tex_Texture_First           >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_First     , r_bloomstate.texture_screen);
			if (r_glsl_permutation->tex_Texture_Second          >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_Second    , r_bloomstate.texture_bloom );
			if (r_glsl_permutation->tex_Texture_GammaRamps      >= 0) R_Mesh_TexBind(r_glsl_permutation->tex_Texture_GammaRamps, r_texture_gammaramps       );
			if (r_glsl_permutation->loc_ViewTintColor           >= 0) qglUniform4f(r_glsl_permutation->loc_ViewTintColor     , r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			if (r_glsl_permutation->loc_PixelSize               >= 0) qglUniform2f(r_glsl_permutation->loc_PixelSize         , 1.0/r_bloomstate.screentexturewidth, 1.0/r_bloomstate.screentextureheight);
			if (r_glsl_permutation->loc_UserVec1                >= 0) qglUniform4f(r_glsl_permutation->loc_UserVec1          , uservecs[0][0], uservecs[0][1], uservecs[0][2], uservecs[0][3]);
			if (r_glsl_permutation->loc_UserVec2                >= 0) qglUniform4f(r_glsl_permutation->loc_UserVec2          , uservecs[1][0], uservecs[1][1], uservecs[1][2], uservecs[1][3]);
			if (r_glsl_permutation->loc_UserVec3                >= 0) qglUniform4f(r_glsl_permutation->loc_UserVec3          , uservecs[2][0], uservecs[2][1], uservecs[2][2], uservecs[2][3]);
			if (r_glsl_permutation->loc_UserVec4                >= 0) qglUniform4f(r_glsl_permutation->loc_UserVec4          , uservecs[3][0], uservecs[3][1], uservecs[3][2], uservecs[3][3]);
			if (r_glsl_permutation->loc_Saturation              >= 0) qglUniform1f(r_glsl_permutation->loc_Saturation        , r_glsl_saturation.value);
			if (r_glsl_permutation->loc_PixelToScreenTexCoord   >= 0) qglUniform2f(r_glsl_permutation->loc_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);
			if (r_glsl_permutation->loc_BloomColorSubtract      >= 0) qglUniform4f(r_glsl_permutation->loc_BloomColorSubtract   , r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, 0.0f);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			// D3D has upside down Y coords, the easiest way to flip this is to flip the screen vertices rather than the texcoords, so we just use a different array for that...
			R_Mesh_PrepareVertices_Mesh_Arrays(4, r_d3dscreenvertex3f, NULL, NULL, NULL, NULL, r_bloomstate.screentexcoord2f, r_bloomstate.bloomtexcoord2f);
			R_SetupShader_SetPermutationHLSL(SHADERMODE_POSTPROCESS, permutation);
			R_Mesh_TexBind(GL20TU_FIRST     , r_bloomstate.texture_screen);
			R_Mesh_TexBind(GL20TU_SECOND    , r_bloomstate.texture_bloom );
			R_Mesh_TexBind(GL20TU_GAMMARAMPS, r_texture_gammaramps       );
			hlslPSSetParameter4f(D3DPSREGISTER_ViewTintColor        , r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			hlslPSSetParameter2f(D3DPSREGISTER_PixelSize            , 1.0/r_bloomstate.screentexturewidth, 1.0/r_bloomstate.screentextureheight);
			hlslPSSetParameter4f(D3DPSREGISTER_UserVec1             , uservecs[0][0], uservecs[0][1], uservecs[0][2], uservecs[0][3]);
			hlslPSSetParameter4f(D3DPSREGISTER_UserVec2             , uservecs[1][0], uservecs[1][1], uservecs[1][2], uservecs[1][3]);
			hlslPSSetParameter4f(D3DPSREGISTER_UserVec3             , uservecs[2][0], uservecs[2][1], uservecs[2][2], uservecs[2][3]);
			hlslPSSetParameter4f(D3DPSREGISTER_UserVec4             , uservecs[3][0], uservecs[3][1], uservecs[3][2], uservecs[3][3]);
			hlslPSSetParameter1f(D3DPSREGISTER_Saturation           , r_glsl_saturation.value);
			hlslPSSetParameter2f(D3DPSREGISTER_PixelToScreenTexCoord, 1.0f/vid.width, 1.0/vid.height);
			hlslPSSetParameter4f(D3DPSREGISTER_BloomColorSubtract   , r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, 0.0f);
#endif
			break;
		case RENDERPATH_D3D10:
			Con_DPrintf("FIXME D3D10 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_D3D11:
			Con_DPrintf("FIXME D3D11 %s:%i %s\n", __FILE__, __LINE__, __FUNCTION__);
			break;
		case RENDERPATH_SOFT:
			R_Mesh_PrepareVertices_Mesh_Arrays(4, r_screenvertex3f, NULL, NULL, NULL, NULL, r_bloomstate.screentexcoord2f, r_bloomstate.bloomtexcoord2f);
			R_SetupShader_SetPermutationSoft(SHADERMODE_POSTPROCESS, permutation);
			R_Mesh_TexBind(GL20TU_FIRST     , r_bloomstate.texture_screen);
			R_Mesh_TexBind(GL20TU_SECOND    , r_bloomstate.texture_bloom );
			R_Mesh_TexBind(GL20TU_GAMMARAMPS, r_texture_gammaramps       );
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_ViewTintColor     , r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_PixelSize         , 1.0/r_bloomstate.screentexturewidth, 1.0/r_bloomstate.screentextureheight);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_UserVec1          , uservecs[0][0], uservecs[0][1], uservecs[0][2], uservecs[0][3]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_UserVec2          , uservecs[1][0], uservecs[1][1], uservecs[1][2], uservecs[1][3]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_UserVec3          , uservecs[2][0], uservecs[2][1], uservecs[2][2], uservecs[2][3]);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_UserVec4          , uservecs[3][0], uservecs[3][1], uservecs[3][2], uservecs[3][3]);
			DPSOFTRAST_Uniform1f(DPSOFTRAST_UNIFORM_Saturation        , r_glsl_saturation.value);
			DPSOFTRAST_Uniform2f(DPSOFTRAST_UNIFORM_PixelToScreenTexCoord, 1.0f/vid.width, 1.0f/vid.height);
			DPSOFTRAST_Uniform4f(DPSOFTRAST_UNIFORM_BloomColorSubtract   , r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, 0.0f);
			break;
		default:
			break;
		}
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
		r_refdef.stats.bloom_drawpixels += r_refdef.view.width * r_refdef.view.height;
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
		if (r_refdef.viewblend[3] >= (1.0f / 256.0f))
		{
			// apply a color tint to the whole view
			R_ResetViewRendering2D();
			GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
			R_Mesh_PrepareVertices_Generic_Arrays(4, r_screenvertex3f, NULL, NULL);
			R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
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
	r_refdef.scene.rtdlight = r_shadow_realtime_dlight.integer != 0 && !gl_flashblend.integer && r_dynamic.integer;
	r_refdef.scene.rtdlightshadows = r_refdef.scene.rtdlight && r_shadow_realtime_dlight_shadows.integer && vid.stencil;
	r_refdef.lightmapintensity = r_refdef.scene.rtworld ? r_shadow_realtime_world_lightmaps.value : 1;
	if (FAKELIGHT_ENABLED)
	{
		r_refdef.lightmapintensity *= r_fakelight_intensity.value;
	}
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
		if (strcmp(r_refdef.fogheighttexturename, r_refdef.fog_height_texturename))
			R_BuildFogHeightTexture();
		// fog color was already set
		// update the fog texture
		if (r_refdef.fogmasktable_start != r_refdef.fog_start || r_refdef.fogmasktable_alpha != r_refdef.fog_alpha || r_refdef.fogmasktable_density != r_refdef.fog_density || r_refdef.fogmasktable_range != r_refdef.fogrange)
			R_BuildFogTexture();
		r_refdef.fog_height_texcoordscale = 1.0f / max(0.125f, r_refdef.fog_fadedepth);
		r_refdef.fog_height_tablescale = r_refdef.fog_height_tablesize * r_refdef.fog_height_texcoordscale;
	}
	else
		r_refdef.fogenabled = false;

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
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
					R_UpdateTexture(r_texture_gammaramps, &rampbgr[0][0], 0, 0, 0, RAMPWIDTH, 1, 1);
				}
				else
				{
					r_texture_gammaramps = R_LoadTexture2D(r_main_texturepool, "gammaramps", RAMPWIDTH, 1, &rampbgr[0][0], TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_PERSISTENT, -1, NULL);
				}
			}
		}
		else
		{
			// remove GLSL gamma texture
		}
		break;
	case RENDERPATH_GL11:
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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
int dpsoftrast_test;
extern void R_Shadow_UpdateBounceGridTexture(void);
extern cvar_t r_shadow_bouncegrid;
void R_RenderView(void)
{
	matrix4x4_t originalmatrix = r_refdef.view.matrix, offsetmatrix;

	dpsoftrast_test = r_test.integer;

	if (r_timereport_active)
		R_TimeReport("start");
	r_textureframe++; // used only by R_GetCurrentTexture
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity

	if(R_CompileShader_CheckStaticParms())
		R_GLSL_Restart_f();

	if (!r_drawentities.integer)
		r_refdef.scene.numentities = 0;

	R_AnimCache_ClearCache();
	R_FrameData_NewFrame();

	/* adjust for stereo display */
	if(R_Stereo_Active())
	{
		Matrix4x4_CreateFromQuakeEntity(&offsetmatrix, 0, r_stereo_separation.value * (0.5f - r_stereo_side), 0, 0, r_stereo_angle.value * (0.5f - r_stereo_side), 0, 1);
		Matrix4x4_Concat(&r_refdef.view.matrix, &originalmatrix, &offsetmatrix);
	}

	if (r_refdef.view.isoverlay)
	{
		// TODO: FIXME: move this into its own backend function maybe? [2/5/2008 Andreas]
		GL_Clear(GL_DEPTH_BUFFER_BIT, NULL, 1.0f, 0);
		R_TimeReport("depthclear");

		r_refdef.view.showdebug = false;

		r_waterstate.enabled = false;
		r_waterstate.numwaterplanes = 0;

		R_RenderScene();

		r_refdef.view.matrix = originalmatrix;

		CHECKGLERROR
		return;
	}

	if (!r_refdef.scene.entities || r_refdef.view.width * r_refdef.view.height == 0 || !r_renderview.integer || cl_videoplaying/* || !r_refdef.scene.worldmodel*/)
	{
		r_refdef.view.matrix = originalmatrix;
		return; //Host_Error ("R_RenderView: NULL worldmodel");
	}

	r_refdef.view.colorscale = r_hdr_scenebrightness.value * r_hdr_irisadaptation_value.value;

	R_RenderView_UpdateViewVectors();

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
	if (r_bloomstate.hdr)
	{
		R_HDR_RenderBloomTexture();
		// we have to bump the texture frame again because r_refdef.view.colorscale is cached in the textures
		r_textureframe++; // used only by R_GetCurrentTexture
	}

	r_refdef.view.showdebug = true;

	R_View_Update();
	if (r_timereport_active)
		R_TimeReport("visibility");

	R_Shadow_UpdateBounceGridTexture();
	if (r_timereport_active && r_shadow_bouncegrid.integer)
		R_TimeReport("bouncegrid");

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

	r_refdef.view.matrix = originalmatrix;

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

	if (r_timereport_active)
		R_TimeReport("beginscene");

	r_refdef.stats.renders++;

	R_UpdateFogColor();

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	R_MeshQueue_BeginScene();

	R_SkyStartFrame();

	Matrix4x4_CreateTranslate(&r_waterscrollmatrix, sin(r_refdef.scene.time) * 0.025 * r_waterscroll.value, sin(r_refdef.scene.time * 0.8f) * 0.025 * r_waterscroll.value, 0);

	if (r_timereport_active)
		R_TimeReport("skystartframe");

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

	if (r_shadows.integer >= 2 && shadowmapping && r_refdef.lightmapintensity > 0)
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

	if ((r_shadows.integer == 1 || (r_shadows.integer > 0 && !shadowmapping)) && !r_shadows_drawafterrtlighting.integer && r_refdef.lightmapintensity > 0)
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

	if ((r_shadows.integer == 1 || (r_shadows.integer > 0 && !shadowmapping)) && r_shadows_drawafterrtlighting.integer && r_refdef.lightmapintensity > 0)
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

	if (r_transparent.integer)
	{
		R_MeshQueue_RenderTransparent();
		if (r_timereport_active)
			R_TimeReport("drawtrans");
	}

	if (r_refdef.view.showdebug && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawDebug && (r_showtris.value > 0 || r_shownormals.value != 0 || r_showcollisionbrushes.value > 0 || r_showoverdraw.value > 0))
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

#if 0
	{
		GL_DepthTest(false);
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		GL_Color(1, 1, 1, 1);
		qglBegin(GL_POLYGON);
		qglVertex3f(r_refdef.view.frustumcorner[0][0], r_refdef.view.frustumcorner[0][1], r_refdef.view.frustumcorner[0][2]);
		qglVertex3f(r_refdef.view.frustumcorner[1][0], r_refdef.view.frustumcorner[1][1], r_refdef.view.frustumcorner[1][2]);
		qglVertex3f(r_refdef.view.frustumcorner[3][0], r_refdef.view.frustumcorner[3][1], r_refdef.view.frustumcorner[3][2]);
		qglVertex3f(r_refdef.view.frustumcorner[2][0], r_refdef.view.frustumcorner[2][1], r_refdef.view.frustumcorner[2][2]);
		qglEnd();
		qglBegin(GL_POLYGON);
		qglVertex3f(r_refdef.view.frustumcorner[0][0] + 1000 * r_refdef.view.forward[0], r_refdef.view.frustumcorner[0][1] + 1000 * r_refdef.view.forward[1], r_refdef.view.frustumcorner[0][2] + 1000 * r_refdef.view.forward[2]);
		qglVertex3f(r_refdef.view.frustumcorner[1][0] + 1000 * r_refdef.view.forward[0], r_refdef.view.frustumcorner[1][1] + 1000 * r_refdef.view.forward[1], r_refdef.view.frustumcorner[1][2] + 1000 * r_refdef.view.forward[2]);
		qglVertex3f(r_refdef.view.frustumcorner[3][0] + 1000 * r_refdef.view.forward[0], r_refdef.view.frustumcorner[3][1] + 1000 * r_refdef.view.forward[1], r_refdef.view.frustumcorner[3][2] + 1000 * r_refdef.view.forward[2]);
		qglVertex3f(r_refdef.view.frustumcorner[2][0] + 1000 * r_refdef.view.forward[0], r_refdef.view.frustumcorner[2][1] + 1000 * r_refdef.view.forward[1], r_refdef.view.frustumcorner[2][2] + 1000 * r_refdef.view.forward[2]);
		qglEnd();
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
#endif

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
//	R_Mesh_ResetTextureState();

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
	R_Mesh_PrepareVertices_Generic_Arrays(8, vertex3f, color4f, NULL);
	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
	R_Mesh_Draw(0, 8, 0, 12, NULL, NULL, 0, bboxelements, NULL, 0);
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
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);

	prog = 0;
	SV_VM_Begin();
	for (i = 0;i < numsurfaces;i++)
	{
		edict = PRVM_EDICT_NUM(surfacelist[i]);
		switch ((int)PRVM_serveredictfloat(edict, solid))
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
		if(PRVM_serveredictedict(edict, tag_entity) != 0)
			continue;
		if(PRVM_serveredictedict(edict, viewmodelforclient) != 0)
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
	memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
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
			f1 = RSurf_FogVertex(nomodelvertex3f + 3*i);
			f2 = 1 - f1;
			c[0] = (c[0] * f1 + r_refdef.fogcolor[0] * f2);
			c[1] = (c[1] * f1 + r_refdef.fogcolor[1] * f2);
			c[2] = (c[2] * f1 + r_refdef.fogcolor[2] * f2);
		}
	}
//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
	R_Mesh_PrepareVertices_Generic_Arrays(6, nomodelvertex3f, color4f, NULL);
	R_Mesh_Draw(0, 6, 0, 8, nomodelelement3i, NULL, 0, nomodelelement3s, NULL, 0);
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

static qboolean R_TestQ3WaveFunc(q3wavefunc_t func, const float *parms)
{
	if(parms[0] == 0 && parms[1] == 0)
		return false;
	if(func >> Q3WAVEFUNC_USER_SHIFT) // assumes rsurface to be set!
		if(rsurface.userwavefunc_param[bound(0, (func >> Q3WAVEFUNC_USER_SHIFT) - 1, Q3WAVEFUNC_USER_COUNT)] == 0)
			return false;
	return true;
}

static float R_EvaluateQ3WaveFunc(q3wavefunc_t func, const float *parms)
{
	double index, f;
	index = parms[2] + rsurface.shadertime * parms[3];
	index -= floor(index);
	switch (func & ((1 << Q3WAVEFUNC_USER_SHIFT) - 1))
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
		{
			// f = f;
		}
		else if (index < 2)
			f = 1 - f;
		else if (index < 3)
			f = -f;
		else
			f = -(1 - f);
		break;
	}
	f = parms[0] + parms[1] * f;
	if(func >> Q3WAVEFUNC_USER_SHIFT) // assumes rsurface to be set!
		f *= rsurface.userwavefunc_param[bound(0, (func >> Q3WAVEFUNC_USER_SHIFT) - 1, Q3WAVEFUNC_USER_COUNT)];
	return (float) f;
}

void R_tcMod_ApplyToMatrix(matrix4x4_t *texmatrix, q3shaderinfo_layer_tcmod_t *tcmod, int currentmaterialflags)
{
	int w, h, idx;
	double f;
	double offsetd[2];
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
			Matrix4x4_ConcatRotate(&matrix, tcmod->parms[0] * rsurface.shadertime, 0, 0, 1);
			Matrix4x4_ConcatTranslate(&matrix, -0.5, -0.5, 0);
			break;
		case Q3TCMOD_SCALE:
			Matrix4x4_CreateScale3(&matrix, tcmod->parms[0], tcmod->parms[1], 1);
			break;
		case Q3TCMOD_SCROLL:
			// extra care is needed because of precision breakdown with large values of time
			offsetd[0] = tcmod->parms[0] * rsurface.shadertime;
			offsetd[1] = tcmod->parms[1] * rsurface.shadertime;
			Matrix4x4_CreateTranslate(&matrix, offsetd[0] - floor(offsetd[0]), offsetd[1] - floor(offsetd[1]), 0);
			break;
		case Q3TCMOD_PAGE: // poor man's animmap (to store animations into a single file, useful for HTTP downloaded textures)
			w = (int) tcmod->parms[0];
			h = (int) tcmod->parms[1];
			f = rsurface.shadertime / (tcmod->parms[2] * w * h);
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
	int textureflags = (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_PICMIP;
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

	if(ent && ent->entitynumber >= MAX_EDICTS && ent->entitynumber < 2 * MAX_EDICTS)
		t->camera_entity = ent->entitynumber;
	else
		t->camera_entity = 0;

	// switch to an alternate material if this is a q1bsp animated material
	{
		texture_t *texture = t;
		int s = rsurface.ent_skinnum;
		if ((unsigned int)s >= (unsigned int)model->numskins)
			s = 0;
		if (model->skinscenes)
		{
			if (model->skinscenes[s].framecount > 1)
				s = model->skinscenes[s].firstframe + (unsigned int) (rsurface.shadertime * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
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
				t = t->anim_frames[1][(t->anim_total[1] >= 2) ? ((int)(rsurface.shadertime * 5.0f) % t->anim_total[1]) : 0];
			else
				t = t->anim_frames[0][(t->anim_total[0] >= 2) ? ((int)(rsurface.shadertime * 5.0f) % t->anim_total[0]) : 0];
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
			r_qwskincache = (r_qwskincache_t *)Mem_Alloc(r_main_mempool, sizeof(*r_qwskincache) * r_qwskincache_size);
		}
		if (strcmp(r_qwskincache[i].name, cl.scores[i].qw_skin))
			R_LoadQWSkin(&r_qwskincache[i], cl.scores[i].qw_skin);
		t->currentskinframe = r_qwskincache[i].skinframe;
		if (t->currentskinframe == NULL)
			t->currentskinframe = t->skinframes[LoopingFrameNumberFromDouble(rsurface.shadertime * t->skinframerate, t->numskinframes)];
	}
	else if (t->numskinframes >= 2)
		t->currentskinframe = t->skinframes[LoopingFrameNumberFromDouble(rsurface.shadertime * t->skinframerate, t->numskinframes)];
	if (t->backgroundnumskinframes >= 2)
		t->backgroundcurrentskinframe = t->backgroundskinframes[LoopingFrameNumberFromDouble(rsurface.shadertime * t->backgroundskinframerate, t->backgroundnumskinframes)];

	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = rsurface.colormod[3];
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA && (model->brush.supportwateralpha || r_novis.integer || r_trippy.integer))
		t->currentalpha *= r_wateralpha.value;
	if(t->basematerialflags & MATERIALFLAG_WATERSHADER && r_waterstate.enabled && !r_refdef.view.isoverlay)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW; // we apply wateralpha later
	if(!r_waterstate.enabled || r_refdef.view.isoverlay)
		t->currentmaterialflags &= ~(MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA);
	if (!(rsurface.ent_flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	else if (FAKELIGHT_ENABLED)
	{
		// no modellight if using fakelight for the map
	}
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
		if (t->currentmaterialflags & (MATERIALFLAG_REFRACTION | MATERIALFLAG_WATERSHADER | MATERIALFLAG_CAMERA))
			t->currentmaterialflags &= ~MATERIALFLAG_BLENDED;
	}
	else
		t->currentmaterialflags &= ~(MATERIALFLAG_REFRACTION | MATERIALFLAG_WATERSHADER | MATERIALFLAG_CAMERA);
	if (vid.allowalphatocoverage && r_transparent_alphatocoverage.integer >= 2 && ((t->currentmaterialflags & (MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHA | MATERIALFLAG_ADD | MATERIALFLAG_CUSTOMBLEND)) == (MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHA)))
	{
		// promote alphablend to alphatocoverage (a type of alphatest) if antialiasing is on
		t->currentmaterialflags = (t->currentmaterialflags & ~(MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHA)) | MATERIALFLAG_ALPHATEST;
	}
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
		t->backgroundbasetexture = r_texture_white;
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
		// don't colormod evilblend textures
		if(!R_BlendFuncFlags(blendfunc1, blendfunc2) & BLENDFUNC_ALLOWS_COLORMOD)
			VectorSet(t->lightmapcolor, 1, 1, 1);
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

void RSurf_ActiveWorldEntity(void)
{
	dp_model_t *model = r_refdef.scene.worldmodel;
	//if (rsurface.entity == r_refdef.scene.worldentity)
	//	return;
	rsurface.entity = r_refdef.scene.worldentity;
	rsurface.skeleton = NULL;
	memset(rsurface.userwavefunc_param, 0, sizeof(rsurface.userwavefunc_param));
	rsurface.ent_skinnum = 0;
	rsurface.ent_qwskin = -1;
	rsurface.ent_flags = r_refdef.scene.worldentity->flags;
	rsurface.shadertime = r_refdef.scene.time;
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
	rsurface.modelvertex3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modelvertex3f_bufferoffset = model->surfmesh.vbooffset_vertex3f;
	rsurface.modelsvector3f = model->surfmesh.data_svector3f;
	rsurface.modelsvector3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modelsvector3f_bufferoffset = model->surfmesh.vbooffset_svector3f;
	rsurface.modeltvector3f = model->surfmesh.data_tvector3f;
	rsurface.modeltvector3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modeltvector3f_bufferoffset = model->surfmesh.vbooffset_tvector3f;
	rsurface.modelnormal3f  = model->surfmesh.data_normal3f;
	rsurface.modelnormal3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modelnormal3f_bufferoffset = model->surfmesh.vbooffset_normal3f;
	rsurface.modellightmapcolor4f  = model->surfmesh.data_lightmapcolor4f;
	rsurface.modellightmapcolor4f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modellightmapcolor4f_bufferoffset = model->surfmesh.vbooffset_lightmapcolor4f;
	rsurface.modeltexcoordtexture2f  = model->surfmesh.data_texcoordtexture2f;
	rsurface.modeltexcoordtexture2f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modeltexcoordtexture2f_bufferoffset = model->surfmesh.vbooffset_texcoordtexture2f;
	rsurface.modeltexcoordlightmap2f  = model->surfmesh.data_texcoordlightmap2f;
	rsurface.modeltexcoordlightmap2f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modeltexcoordlightmap2f_bufferoffset = model->surfmesh.vbooffset_texcoordlightmap2f;
	rsurface.modelelement3i = model->surfmesh.data_element3i;
	rsurface.modelelement3i_indexbuffer = model->surfmesh.data_element3i_indexbuffer;
	rsurface.modelelement3i_bufferoffset = model->surfmesh.data_element3i_bufferoffset;
	rsurface.modelelement3s = model->surfmesh.data_element3s;
	rsurface.modelelement3s_indexbuffer = model->surfmesh.data_element3s_indexbuffer;
	rsurface.modelelement3s_bufferoffset = model->surfmesh.data_element3s_bufferoffset;
	rsurface.modellightmapoffsets = model->surfmesh.data_lightmapoffsets;
	rsurface.modelnumvertices = model->surfmesh.num_vertices;
	rsurface.modelnumtriangles = model->surfmesh.num_triangles;
	rsurface.modelsurfaces = model->data_surfaces;
	rsurface.modelvertexmesh = model->surfmesh.vertexmesh;
	rsurface.modelvertexmeshbuffer = model->surfmesh.vertexmeshbuffer;
	rsurface.modelvertex3fbuffer = model->surfmesh.vertex3fbuffer;
	rsurface.modelgeneratedvertex = false;
	rsurface.batchgeneratedvertex = false;
	rsurface.batchfirstvertex = 0;
	rsurface.batchnumvertices = 0;
	rsurface.batchfirsttriangle = 0;
	rsurface.batchnumtriangles = 0;
	rsurface.batchvertex3f  = NULL;
	rsurface.batchvertex3f_vertexbuffer = NULL;
	rsurface.batchvertex3f_bufferoffset = 0;
	rsurface.batchsvector3f = NULL;
	rsurface.batchsvector3f_vertexbuffer = NULL;
	rsurface.batchsvector3f_bufferoffset = 0;
	rsurface.batchtvector3f = NULL;
	rsurface.batchtvector3f_vertexbuffer = NULL;
	rsurface.batchtvector3f_bufferoffset = 0;
	rsurface.batchnormal3f  = NULL;
	rsurface.batchnormal3f_vertexbuffer = NULL;
	rsurface.batchnormal3f_bufferoffset = 0;
	rsurface.batchlightmapcolor4f = NULL;
	rsurface.batchlightmapcolor4f_vertexbuffer = NULL;
	rsurface.batchlightmapcolor4f_bufferoffset = 0;
	rsurface.batchtexcoordtexture2f = NULL;
	rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
	rsurface.batchtexcoordtexture2f_bufferoffset = 0;
	rsurface.batchtexcoordlightmap2f = NULL;
	rsurface.batchtexcoordlightmap2f_vertexbuffer = NULL;
	rsurface.batchtexcoordlightmap2f_bufferoffset = 0;
	rsurface.batchvertexmesh = NULL;
	rsurface.batchvertexmeshbuffer = NULL;
	rsurface.batchvertex3fbuffer = NULL;
	rsurface.batchelement3i = NULL;
	rsurface.batchelement3i_indexbuffer = NULL;
	rsurface.batchelement3i_bufferoffset = 0;
	rsurface.batchelement3s = NULL;
	rsurface.batchelement3s_indexbuffer = NULL;
	rsurface.batchelement3s_bufferoffset = 0;
	rsurface.passcolor4f = NULL;
	rsurface.passcolor4f_vertexbuffer = NULL;
	rsurface.passcolor4f_bufferoffset = 0;
}

void RSurf_ActiveModelEntity(const entity_render_t *ent, qboolean wantnormals, qboolean wanttangents, qboolean prepass)
{
	dp_model_t *model = ent->model;
	//if (rsurface.entity == ent && (!model->surfmesh.isanimated || (!wantnormals && !wanttangents)))
	//	return;
	rsurface.entity = (entity_render_t *)ent;
	rsurface.skeleton = ent->skeleton;
	memcpy(rsurface.userwavefunc_param, ent->userwavefunc_param, sizeof(rsurface.userwavefunc_param));
	rsurface.ent_skinnum = ent->skinnum;
	rsurface.ent_qwskin = (ent->entitynumber <= cl.maxclients && ent->entitynumber >= 1 && cls.protocol == PROTOCOL_QUAKEWORLD && cl.scores[ent->entitynumber - 1].qw_skin[0] && !strcmp(ent->model->name, "progs/player.mdl")) ? (ent->entitynumber - 1) : -1;
	rsurface.ent_flags = ent->flags;
	rsurface.shadertime = r_refdef.scene.time - ent->shadertime;
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
	if (model->surfmesh.isanimated && model->AnimateVertices)
	{
		if (ent->animcache_vertex3f)
		{
			rsurface.modelvertex3f = ent->animcache_vertex3f;
			rsurface.modelsvector3f = wanttangents ? ent->animcache_svector3f : NULL;
			rsurface.modeltvector3f = wanttangents ? ent->animcache_tvector3f : NULL;
			rsurface.modelnormal3f = wantnormals ? ent->animcache_normal3f : NULL;
			rsurface.modelvertexmesh = ent->animcache_vertexmesh;
			rsurface.modelvertexmeshbuffer = ent->animcache_vertexmeshbuffer;
			rsurface.modelvertex3fbuffer = ent->animcache_vertex3fbuffer;
		}
		else if (wanttangents)
		{
			rsurface.modelvertex3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			rsurface.modelsvector3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			rsurface.modeltvector3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			rsurface.modelnormal3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.modelvertex3f, rsurface.modelnormal3f, rsurface.modelsvector3f, rsurface.modeltvector3f);
			rsurface.modelvertexmesh = NULL;
			rsurface.modelvertexmeshbuffer = NULL;
			rsurface.modelvertex3fbuffer = NULL;
		}
		else if (wantnormals)
		{
			rsurface.modelvertex3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.modelvertex3f, rsurface.modelnormal3f, NULL, NULL);
			rsurface.modelvertexmesh = NULL;
			rsurface.modelvertexmeshbuffer = NULL;
			rsurface.modelvertex3fbuffer = NULL;
		}
		else
		{
			rsurface.modelvertex3f = (float *)R_FrameData_Alloc(model->surfmesh.num_vertices * sizeof(float[3]));
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = NULL;
			model->AnimateVertices(model, rsurface.frameblend, rsurface.skeleton, rsurface.modelvertex3f, NULL, NULL, NULL);
			rsurface.modelvertexmesh = NULL;
			rsurface.modelvertexmeshbuffer = NULL;
			rsurface.modelvertex3fbuffer = NULL;
		}
		rsurface.modelvertex3f_vertexbuffer = 0;
		rsurface.modelvertex3f_bufferoffset = 0;
		rsurface.modelsvector3f_vertexbuffer = 0;
		rsurface.modelsvector3f_bufferoffset = 0;
		rsurface.modeltvector3f_vertexbuffer = 0;
		rsurface.modeltvector3f_bufferoffset = 0;
		rsurface.modelnormal3f_vertexbuffer = 0;
		rsurface.modelnormal3f_bufferoffset = 0;
		rsurface.modelgeneratedvertex = true;
	}
	else
	{
		rsurface.modelvertex3f  = model->surfmesh.data_vertex3f;
		rsurface.modelvertex3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
		rsurface.modelvertex3f_bufferoffset = model->surfmesh.vbooffset_vertex3f;
		rsurface.modelsvector3f = model->surfmesh.data_svector3f;
		rsurface.modelsvector3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
		rsurface.modelsvector3f_bufferoffset = model->surfmesh.vbooffset_svector3f;
		rsurface.modeltvector3f = model->surfmesh.data_tvector3f;
		rsurface.modeltvector3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
		rsurface.modeltvector3f_bufferoffset = model->surfmesh.vbooffset_tvector3f;
		rsurface.modelnormal3f  = model->surfmesh.data_normal3f;
		rsurface.modelnormal3f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
		rsurface.modelnormal3f_bufferoffset = model->surfmesh.vbooffset_normal3f;
		rsurface.modelvertexmesh = model->surfmesh.vertexmesh;
		rsurface.modelvertexmeshbuffer = model->surfmesh.vertexmeshbuffer;
		rsurface.modelvertex3fbuffer = model->surfmesh.vertex3fbuffer;
		rsurface.modelgeneratedvertex = false;
	}
	rsurface.modellightmapcolor4f  = model->surfmesh.data_lightmapcolor4f;
	rsurface.modellightmapcolor4f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modellightmapcolor4f_bufferoffset = model->surfmesh.vbooffset_lightmapcolor4f;
	rsurface.modeltexcoordtexture2f  = model->surfmesh.data_texcoordtexture2f;
	rsurface.modeltexcoordtexture2f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modeltexcoordtexture2f_bufferoffset = model->surfmesh.vbooffset_texcoordtexture2f;
	rsurface.modeltexcoordlightmap2f  = model->surfmesh.data_texcoordlightmap2f;
	rsurface.modeltexcoordlightmap2f_vertexbuffer = model->surfmesh.vbo_vertexbuffer;
	rsurface.modeltexcoordlightmap2f_bufferoffset = model->surfmesh.vbooffset_texcoordlightmap2f;
	rsurface.modelelement3i = model->surfmesh.data_element3i;
	rsurface.modelelement3i_indexbuffer = model->surfmesh.data_element3i_indexbuffer;
	rsurface.modelelement3i_bufferoffset = model->surfmesh.data_element3i_bufferoffset;
	rsurface.modelelement3s = model->surfmesh.data_element3s;
	rsurface.modelelement3s_indexbuffer = model->surfmesh.data_element3s_indexbuffer;
	rsurface.modelelement3s_bufferoffset = model->surfmesh.data_element3s_bufferoffset;
	rsurface.modellightmapoffsets = model->surfmesh.data_lightmapoffsets;
	rsurface.modelnumvertices = model->surfmesh.num_vertices;
	rsurface.modelnumtriangles = model->surfmesh.num_triangles;
	rsurface.modelsurfaces = model->data_surfaces;
	rsurface.batchgeneratedvertex = false;
	rsurface.batchfirstvertex = 0;
	rsurface.batchnumvertices = 0;
	rsurface.batchfirsttriangle = 0;
	rsurface.batchnumtriangles = 0;
	rsurface.batchvertex3f  = NULL;
	rsurface.batchvertex3f_vertexbuffer = NULL;
	rsurface.batchvertex3f_bufferoffset = 0;
	rsurface.batchsvector3f = NULL;
	rsurface.batchsvector3f_vertexbuffer = NULL;
	rsurface.batchsvector3f_bufferoffset = 0;
	rsurface.batchtvector3f = NULL;
	rsurface.batchtvector3f_vertexbuffer = NULL;
	rsurface.batchtvector3f_bufferoffset = 0;
	rsurface.batchnormal3f  = NULL;
	rsurface.batchnormal3f_vertexbuffer = NULL;
	rsurface.batchnormal3f_bufferoffset = 0;
	rsurface.batchlightmapcolor4f = NULL;
	rsurface.batchlightmapcolor4f_vertexbuffer = NULL;
	rsurface.batchlightmapcolor4f_bufferoffset = 0;
	rsurface.batchtexcoordtexture2f = NULL;
	rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
	rsurface.batchtexcoordtexture2f_bufferoffset = 0;
	rsurface.batchtexcoordlightmap2f = NULL;
	rsurface.batchtexcoordlightmap2f_vertexbuffer = NULL;
	rsurface.batchtexcoordlightmap2f_bufferoffset = 0;
	rsurface.batchvertexmesh = NULL;
	rsurface.batchvertexmeshbuffer = NULL;
	rsurface.batchvertex3fbuffer = NULL;
	rsurface.batchelement3i = NULL;
	rsurface.batchelement3i_indexbuffer = NULL;
	rsurface.batchelement3i_bufferoffset = 0;
	rsurface.batchelement3s = NULL;
	rsurface.batchelement3s_indexbuffer = NULL;
	rsurface.batchelement3s_bufferoffset = 0;
	rsurface.passcolor4f = NULL;
	rsurface.passcolor4f_vertexbuffer = NULL;
	rsurface.passcolor4f_bufferoffset = 0;
}

void RSurf_ActiveCustomEntity(const matrix4x4_t *matrix, const matrix4x4_t *inversematrix, int entflags, double shadertime, float r, float g, float b, float a, int numvertices, const float *vertex3f, const float *texcoord2f, const float *normal3f, const float *svector3f, const float *tvector3f, const float *color4f, int numtriangles, const int *element3i, const unsigned short *element3s, qboolean wantnormals, qboolean wanttangents)
{
	rsurface.entity = r_refdef.scene.worldentity;
	rsurface.skeleton = NULL;
	rsurface.ent_skinnum = 0;
	rsurface.ent_qwskin = -1;
	rsurface.ent_flags = entflags;
	rsurface.shadertime = r_refdef.scene.time - shadertime;
	rsurface.modelnumvertices = numvertices;
	rsurface.modelnumtriangles = numtriangles;
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
		rsurface.modelvertex3f = (float *)vertex3f;
		rsurface.modelsvector3f = svector3f ? (float *)svector3f : (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
		rsurface.modeltvector3f = tvector3f ? (float *)tvector3f : (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
		rsurface.modelnormal3f = normal3f ? (float *)normal3f : (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
	}
	else if (wantnormals)
	{
		rsurface.modelvertex3f = (float *)vertex3f;
		rsurface.modelsvector3f = NULL;
		rsurface.modeltvector3f = NULL;
		rsurface.modelnormal3f = normal3f ? (float *)normal3f : (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
	}
	else
	{
		rsurface.modelvertex3f = (float *)vertex3f;
		rsurface.modelsvector3f = NULL;
		rsurface.modeltvector3f = NULL;
		rsurface.modelnormal3f = NULL;
	}
	rsurface.modelvertexmesh = NULL;
	rsurface.modelvertexmeshbuffer = NULL;
	rsurface.modelvertex3fbuffer = NULL;
	rsurface.modelvertex3f_vertexbuffer = 0;
	rsurface.modelvertex3f_bufferoffset = 0;
	rsurface.modelsvector3f_vertexbuffer = 0;
	rsurface.modelsvector3f_bufferoffset = 0;
	rsurface.modeltvector3f_vertexbuffer = 0;
	rsurface.modeltvector3f_bufferoffset = 0;
	rsurface.modelnormal3f_vertexbuffer = 0;
	rsurface.modelnormal3f_bufferoffset = 0;
	rsurface.modelgeneratedvertex = true;
	rsurface.modellightmapcolor4f  = (float *)color4f;
	rsurface.modellightmapcolor4f_vertexbuffer = 0;
	rsurface.modellightmapcolor4f_bufferoffset = 0;
	rsurface.modeltexcoordtexture2f  = (float *)texcoord2f;
	rsurface.modeltexcoordtexture2f_vertexbuffer = 0;
	rsurface.modeltexcoordtexture2f_bufferoffset = 0;
	rsurface.modeltexcoordlightmap2f  = NULL;
	rsurface.modeltexcoordlightmap2f_vertexbuffer = 0;
	rsurface.modeltexcoordlightmap2f_bufferoffset = 0;
	rsurface.modelelement3i = (int *)element3i;
	rsurface.modelelement3i_indexbuffer = NULL;
	rsurface.modelelement3i_bufferoffset = 0;
	rsurface.modelelement3s = (unsigned short *)element3s;
	rsurface.modelelement3s_indexbuffer = NULL;
	rsurface.modelelement3s_bufferoffset = 0;
	rsurface.modellightmapoffsets = NULL;
	rsurface.modelsurfaces = NULL;
	rsurface.batchgeneratedvertex = false;
	rsurface.batchfirstvertex = 0;
	rsurface.batchnumvertices = 0;
	rsurface.batchfirsttriangle = 0;
	rsurface.batchnumtriangles = 0;
	rsurface.batchvertex3f  = NULL;
	rsurface.batchvertex3f_vertexbuffer = NULL;
	rsurface.batchvertex3f_bufferoffset = 0;
	rsurface.batchsvector3f = NULL;
	rsurface.batchsvector3f_vertexbuffer = NULL;
	rsurface.batchsvector3f_bufferoffset = 0;
	rsurface.batchtvector3f = NULL;
	rsurface.batchtvector3f_vertexbuffer = NULL;
	rsurface.batchtvector3f_bufferoffset = 0;
	rsurface.batchnormal3f  = NULL;
	rsurface.batchnormal3f_vertexbuffer = NULL;
	rsurface.batchnormal3f_bufferoffset = 0;
	rsurface.batchlightmapcolor4f = NULL;
	rsurface.batchlightmapcolor4f_vertexbuffer = NULL;
	rsurface.batchlightmapcolor4f_bufferoffset = 0;
	rsurface.batchtexcoordtexture2f = NULL;
	rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
	rsurface.batchtexcoordtexture2f_bufferoffset = 0;
	rsurface.batchtexcoordlightmap2f = NULL;
	rsurface.batchtexcoordlightmap2f_vertexbuffer = NULL;
	rsurface.batchtexcoordlightmap2f_bufferoffset = 0;
	rsurface.batchvertexmesh = NULL;
	rsurface.batchvertexmeshbuffer = NULL;
	rsurface.batchvertex3fbuffer = NULL;
	rsurface.batchelement3i = NULL;
	rsurface.batchelement3i_indexbuffer = NULL;
	rsurface.batchelement3i_bufferoffset = 0;
	rsurface.batchelement3s = NULL;
	rsurface.batchelement3s_indexbuffer = NULL;
	rsurface.batchelement3s_bufferoffset = 0;
	rsurface.passcolor4f = NULL;
	rsurface.passcolor4f_vertexbuffer = NULL;
	rsurface.passcolor4f_bufferoffset = 0;

	if (rsurface.modelnumvertices && rsurface.modelelement3i)
	{
		if ((wantnormals || wanttangents) && !normal3f)
		{
			rsurface.modelnormal3f = (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
			Mod_BuildNormals(0, rsurface.modelnumvertices, rsurface.modelnumtriangles, rsurface.modelvertex3f, rsurface.modelelement3i, rsurface.modelnormal3f, r_smoothnormals_areaweighting.integer != 0);
		}
		if (wanttangents && !svector3f)
		{
			rsurface.modelsvector3f = (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
			rsurface.modeltvector3f = (float *)R_FrameData_Alloc(rsurface.modelnumvertices * sizeof(float[3]));
			Mod_BuildTextureVectorsFromNormals(0, rsurface.modelnumvertices, rsurface.modelnumtriangles, rsurface.modelvertex3f, rsurface.modeltexcoordtexture2f, rsurface.modelnormal3f, rsurface.modelelement3i, rsurface.modelsvector3f, rsurface.modeltvector3f, r_smoothnormals_areaweighting.integer != 0);
		}
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

void RSurf_RenumberElements(const int *inelement3i, int *outelement3i, int numelements, int adjust)
{
	int i;
	for (i = 0;i < numelements;i++)
		outelement3i[i] = inelement3i[i] + adjust;
}

static const int quadedges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
extern cvar_t gl_vbo;
void RSurf_PrepareVerticesForBatch(int batchneed, int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	int deformindex;
	int firsttriangle;
	int numtriangles;
	int firstvertex;
	int endvertex;
	int numvertices;
	int surfacefirsttriangle;
	int surfacenumtriangles;
	int surfacefirstvertex;
	int surfaceendvertex;
	int surfacenumvertices;
	int batchnumvertices;
	int batchnumtriangles;
	int needsupdate;
	int i, j;
	qboolean gaps;
	qboolean dynamicvertex;
	float amplitude;
	float animpos;
	float scale;
	float center[3], forward[3], right[3], up[3], v[3], newforward[3], newright[3], newup[3];
	float waveparms[4];
	q3shaderinfo_deform_t *deform;
	const msurface_t *surface, *firstsurface;
	r_vertexmesh_t *vertexmesh;
	if (!texturenumsurfaces)
		return;
	// find vertex range of this surface batch
	gaps = false;
	firstsurface = texturesurfacelist[0];
	firsttriangle = firstsurface->num_firsttriangle;
	batchnumvertices = 0;
	batchnumtriangles = 0;
	firstvertex = endvertex = firstsurface->num_firstvertex;
	for (i = 0;i < texturenumsurfaces;i++)
	{
		surface = texturesurfacelist[i];
		if (surface != firstsurface + i)
			gaps = true;
		surfacefirstvertex = surface->num_firstvertex;
		surfaceendvertex = surfacefirstvertex + surface->num_vertices;
		surfacenumvertices = surface->num_vertices;
		surfacenumtriangles = surface->num_triangles;
		if (firstvertex > surfacefirstvertex)
			firstvertex = surfacefirstvertex;
		if (endvertex < surfaceendvertex)
			endvertex = surfaceendvertex;
		batchnumvertices += surfacenumvertices;
		batchnumtriangles += surfacenumtriangles;
	}

	// we now know the vertex range used, and if there are any gaps in it
	rsurface.batchfirstvertex = firstvertex;
	rsurface.batchnumvertices = endvertex - firstvertex;
	rsurface.batchfirsttriangle = firsttriangle;
	rsurface.batchnumtriangles = batchnumtriangles;

	// this variable holds flags for which properties have been updated that
	// may require regenerating vertexmesh array...
	needsupdate = 0;

	// check if any dynamic vertex processing must occur
	dynamicvertex = false;

	// if there is a chance of animated vertex colors, it's a dynamic batch
	if ((batchneed & (BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_ARRAY_VERTEXCOLOR)) && texturesurfacelist[0]->lightmapinfo)
	{
		dynamicvertex = true;
		batchneed |= BATCHNEED_NOGAPS;
		needsupdate |= BATCHNEED_VERTEXMESH_VERTEXCOLOR;
	}

	for (deformindex = 0, deform = rsurface.texture->deforms;deformindex < Q3MAXDEFORMS && deform->deform && r_deformvertexes.integer;deformindex++, deform++)
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
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR;
			break;
		case Q3DEFORM_AUTOSPRITE2:
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR;
			break;
		case Q3DEFORM_NORMAL:
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR;
			break;
		case Q3DEFORM_WAVE:
			if(!R_TestQ3WaveFunc(deform->wavefunc, deform->waveparms))
				break; // if wavefunc is a nop, ignore this transform
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR;
			break;
		case Q3DEFORM_BULGE:
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR;
			break;
		case Q3DEFORM_MOVE:
			if(!R_TestQ3WaveFunc(deform->wavefunc, deform->waveparms))
				break; // if wavefunc is a nop, ignore this transform
			dynamicvertex = true;
			batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS;
			needsupdate |= BATCHNEED_VERTEXMESH_VERTEX;
			break;
		}
	}
	switch(rsurface.texture->tcgen.tcgen)
	{
	default:
	case Q3TCGEN_TEXTURE:
		break;
	case Q3TCGEN_LIGHTMAP:
		dynamicvertex = true;
		batchneed |= BATCHNEED_ARRAY_LIGHTMAP | BATCHNEED_NOGAPS;
		needsupdate |= BATCHNEED_VERTEXMESH_LIGHTMAP;
		break;
	case Q3TCGEN_VECTOR:
		dynamicvertex = true;
		batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS;
		needsupdate |= BATCHNEED_VERTEXMESH_TEXCOORD;
		break;
	case Q3TCGEN_ENVIRONMENT:
		dynamicvertex = true;
		batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_NOGAPS;
		needsupdate |= BATCHNEED_VERTEXMESH_TEXCOORD;
		break;
	}
	if (rsurface.texture->tcmods[0].tcmod == Q3TCMOD_TURBULENT)
	{
		dynamicvertex = true;
		batchneed |= BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_NOGAPS;
		needsupdate |= BATCHNEED_VERTEXMESH_TEXCOORD;
	}

	if (!rsurface.modelvertexmesh && (batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP)))
	{
		dynamicvertex = true;
		batchneed |= BATCHNEED_NOGAPS;
		needsupdate |= (batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP));
	}

	if (dynamicvertex || gaps || rsurface.batchfirstvertex)
	{
		// when copying, we need to consider the regeneration of vertexmesh, any dependencies it may have must be set...
		if (batchneed & BATCHNEED_VERTEXMESH_VERTEX)      batchneed |= BATCHNEED_ARRAY_VERTEX;
		if (batchneed & BATCHNEED_VERTEXMESH_NORMAL)      batchneed |= BATCHNEED_ARRAY_NORMAL;
		if (batchneed & BATCHNEED_VERTEXMESH_VECTOR)      batchneed |= BATCHNEED_ARRAY_VECTOR;
		if (batchneed & BATCHNEED_VERTEXMESH_VERTEXCOLOR) batchneed |= BATCHNEED_ARRAY_VERTEXCOLOR;
		if (batchneed & BATCHNEED_VERTEXMESH_TEXCOORD)    batchneed |= BATCHNEED_ARRAY_TEXCOORD;
		if (batchneed & BATCHNEED_VERTEXMESH_LIGHTMAP)    batchneed |= BATCHNEED_ARRAY_LIGHTMAP;
	}

	// when the model data has no vertex buffer (dynamic mesh), we need to
	// eliminate gaps
	if (vid.useinterleavedarrays ? !rsurface.modelvertexmeshbuffer : !rsurface.modelvertex3f_vertexbuffer)
		batchneed |= BATCHNEED_NOGAPS;

	// if needsupdate, we have to do a dynamic vertex batch for sure
	if (needsupdate & batchneed)
		dynamicvertex = true;

	// see if we need to build vertexmesh from arrays
	if (!rsurface.modelvertexmesh && (batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP)))
		dynamicvertex = true;

	// if gaps are unacceptable, and there are gaps, it's a dynamic batch...
	// also some drivers strongly dislike firstvertex
	if ((batchneed & BATCHNEED_NOGAPS) && (gaps || firstvertex))
		dynamicvertex = true;

	rsurface.batchvertex3f = rsurface.modelvertex3f;
	rsurface.batchvertex3f_vertexbuffer = rsurface.modelvertex3f_vertexbuffer;
	rsurface.batchvertex3f_bufferoffset = rsurface.modelvertex3f_bufferoffset;
	rsurface.batchsvector3f = rsurface.modelsvector3f;
	rsurface.batchsvector3f_vertexbuffer = rsurface.modelsvector3f_vertexbuffer;
	rsurface.batchsvector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset;
	rsurface.batchtvector3f = rsurface.modeltvector3f;
	rsurface.batchtvector3f_vertexbuffer = rsurface.modeltvector3f_vertexbuffer;
	rsurface.batchtvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset;
	rsurface.batchnormal3f = rsurface.modelnormal3f;
	rsurface.batchnormal3f_vertexbuffer = rsurface.modelnormal3f_vertexbuffer;
	rsurface.batchnormal3f_bufferoffset = rsurface.modelnormal3f_bufferoffset;
	rsurface.batchlightmapcolor4f = rsurface.modellightmapcolor4f;
	rsurface.batchlightmapcolor4f_vertexbuffer  = rsurface.modellightmapcolor4f_vertexbuffer;
	rsurface.batchlightmapcolor4f_bufferoffset  = rsurface.modellightmapcolor4f_bufferoffset;
	rsurface.batchtexcoordtexture2f = rsurface.modeltexcoordtexture2f;
	rsurface.batchtexcoordtexture2f_vertexbuffer  = rsurface.modeltexcoordtexture2f_vertexbuffer;
	rsurface.batchtexcoordtexture2f_bufferoffset  = rsurface.modeltexcoordtexture2f_bufferoffset;
	rsurface.batchtexcoordlightmap2f = rsurface.modeltexcoordlightmap2f;
	rsurface.batchtexcoordlightmap2f_vertexbuffer = rsurface.modeltexcoordlightmap2f_vertexbuffer;
	rsurface.batchtexcoordlightmap2f_bufferoffset = rsurface.modeltexcoordlightmap2f_bufferoffset;
	rsurface.batchvertex3fbuffer = rsurface.modelvertex3fbuffer;
	rsurface.batchvertexmesh = rsurface.modelvertexmesh;
	rsurface.batchvertexmeshbuffer = rsurface.modelvertexmeshbuffer;
	rsurface.batchelement3i = rsurface.modelelement3i;
	rsurface.batchelement3i_indexbuffer = rsurface.modelelement3i_indexbuffer;
	rsurface.batchelement3i_bufferoffset = rsurface.modelelement3i_bufferoffset;
	rsurface.batchelement3s = rsurface.modelelement3s;
	rsurface.batchelement3s_indexbuffer = rsurface.modelelement3s_indexbuffer;
	rsurface.batchelement3s_bufferoffset = rsurface.modelelement3s_bufferoffset;

	// if any dynamic vertex processing has to occur in software, we copy the
	// entire surface list together before processing to rebase the vertices
	// to start at 0 (otherwise we waste a lot of room in a vertex buffer).
	//
	// if any gaps exist and we do not have a static vertex buffer, we have to
	// copy the surface list together to avoid wasting upload bandwidth on the
	// vertices in the gaps.
	//
	// if gaps exist and we have a static vertex buffer, we still have to
	// combine the index buffer ranges into one dynamic index buffer.
	//
	// in all cases we end up with data that can be drawn in one call.

	if (!dynamicvertex)
	{
		// static vertex data, just set pointers...
		rsurface.batchgeneratedvertex = false;
		// if there are gaps, we want to build a combined index buffer,
		// otherwise use the original static buffer with an appropriate offset
		if (gaps)
		{
			// build a new triangle elements array for this batch
			rsurface.batchelement3i = (int *)R_FrameData_Alloc(batchnumtriangles * sizeof(int[3]));
			rsurface.batchfirsttriangle = 0;
			numtriangles = 0;
			for (i = 0;i < texturenumsurfaces;i++)
			{
				surfacefirsttriangle = texturesurfacelist[i]->num_firsttriangle;
				surfacenumtriangles = texturesurfacelist[i]->num_triangles;
				memcpy(rsurface.batchelement3i + 3*numtriangles, rsurface.modelelement3i + 3*surfacefirsttriangle, surfacenumtriangles*sizeof(int[3]));
				numtriangles += surfacenumtriangles;
			}
			rsurface.batchelement3i_indexbuffer = NULL;
			rsurface.batchelement3i_bufferoffset = 0;
			rsurface.batchelement3s = NULL;
			rsurface.batchelement3s_indexbuffer = NULL;
			rsurface.batchelement3s_bufferoffset = 0;
			if (endvertex <= 65536)
			{
				// make a 16bit (unsigned short) index array if possible
				rsurface.batchelement3s = (unsigned short *)R_FrameData_Alloc(batchnumtriangles * sizeof(unsigned short[3]));
				for (i = 0;i < numtriangles*3;i++)
					rsurface.batchelement3s[i] = rsurface.batchelement3i[i];
			}
		}
		return;
	}

	// something needs software processing, do it for real...
	// we only directly handle separate array data in this case and then
	// generate interleaved data if needed...
	rsurface.batchgeneratedvertex = true;

	// now copy the vertex data into a combined array and make an index array
	// (this is what Quake3 does all the time)
	//if (gaps || rsurface.batchfirstvertex)
	{
		rsurface.batchvertex3fbuffer = NULL;
		rsurface.batchvertexmesh = NULL;
		rsurface.batchvertexmeshbuffer = NULL;
		rsurface.batchvertex3f = NULL;
		rsurface.batchvertex3f_vertexbuffer = NULL;
		rsurface.batchvertex3f_bufferoffset = 0;
		rsurface.batchsvector3f = NULL;
		rsurface.batchsvector3f_vertexbuffer = NULL;
		rsurface.batchsvector3f_bufferoffset = 0;
		rsurface.batchtvector3f = NULL;
		rsurface.batchtvector3f_vertexbuffer = NULL;
		rsurface.batchtvector3f_bufferoffset = 0;
		rsurface.batchnormal3f = NULL;
		rsurface.batchnormal3f_vertexbuffer = NULL;
		rsurface.batchnormal3f_bufferoffset = 0;
		rsurface.batchlightmapcolor4f = NULL;
		rsurface.batchlightmapcolor4f_vertexbuffer = NULL;
		rsurface.batchlightmapcolor4f_bufferoffset = 0;
		rsurface.batchtexcoordtexture2f = NULL;
		rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
		rsurface.batchtexcoordtexture2f_bufferoffset = 0;
		rsurface.batchtexcoordlightmap2f = NULL;
		rsurface.batchtexcoordlightmap2f_vertexbuffer = NULL;
		rsurface.batchtexcoordlightmap2f_bufferoffset = 0;
		rsurface.batchelement3i = (int *)R_FrameData_Alloc(batchnumtriangles * sizeof(int[3]));
		rsurface.batchelement3i_indexbuffer = NULL;
		rsurface.batchelement3i_bufferoffset = 0;
		rsurface.batchelement3s = NULL;
		rsurface.batchelement3s_indexbuffer = NULL;
		rsurface.batchelement3s_bufferoffset = 0;
		// we'll only be setting up certain arrays as needed
		if (batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP))
			rsurface.batchvertexmesh = (r_vertexmesh_t *)R_FrameData_Alloc(batchnumvertices * sizeof(r_vertexmesh_t));
		if (batchneed & BATCHNEED_ARRAY_VERTEX)
			rsurface.batchvertex3f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
		if (batchneed & BATCHNEED_ARRAY_NORMAL)
			rsurface.batchnormal3f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
		if (batchneed & BATCHNEED_ARRAY_VECTOR)
		{
			rsurface.batchsvector3f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
			rsurface.batchtvector3f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
		}
		if (batchneed & BATCHNEED_ARRAY_VERTEXCOLOR)
			rsurface.batchlightmapcolor4f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[4]));
		if (batchneed & BATCHNEED_ARRAY_TEXCOORD)
			rsurface.batchtexcoordtexture2f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
		if (batchneed & BATCHNEED_ARRAY_LIGHTMAP)
			rsurface.batchtexcoordlightmap2f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
		numvertices = 0;
		numtriangles = 0;
		for (i = 0;i < texturenumsurfaces;i++)
		{
			surfacefirstvertex = texturesurfacelist[i]->num_firstvertex;
			surfacenumvertices = texturesurfacelist[i]->num_vertices;
			surfacefirsttriangle = texturesurfacelist[i]->num_firsttriangle;
			surfacenumtriangles = texturesurfacelist[i]->num_triangles;
			// copy only the data requested
			if ((batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP)) && rsurface.modelvertexmesh)
				memcpy(rsurface.batchvertexmesh + numvertices, rsurface.modelvertexmesh + surfacefirstvertex, surfacenumvertices * sizeof(rsurface.batchvertexmesh[0]));
			if (batchneed & (BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | BATCHNEED_ARRAY_VERTEXCOLOR | BATCHNEED_ARRAY_TEXCOORD | BATCHNEED_ARRAY_LIGHTMAP))
			{
				if (batchneed & BATCHNEED_ARRAY_VERTEX)
				{
					if (rsurface.batchvertex3f)
						memcpy(rsurface.batchvertex3f + 3*numvertices, rsurface.modelvertex3f + 3*surfacefirstvertex, surfacenumvertices * sizeof(float[3]));
					else
						memset(rsurface.batchvertex3f + 3*numvertices, 0, surfacenumvertices * sizeof(float[3]));
				}
				if (batchneed & BATCHNEED_ARRAY_NORMAL)
				{
					if (rsurface.modelnormal3f)
						memcpy(rsurface.batchnormal3f + 3*numvertices, rsurface.modelnormal3f + 3*surfacefirstvertex, surfacenumvertices * sizeof(float[3]));
					else
						memset(rsurface.batchnormal3f + 3*numvertices, 0, surfacenumvertices * sizeof(float[3]));
				}
				if (batchneed & BATCHNEED_ARRAY_VECTOR)
				{
					if (rsurface.modelsvector3f)
					{
						memcpy(rsurface.batchsvector3f + 3*numvertices, rsurface.modelsvector3f + 3*surfacefirstvertex, surfacenumvertices * sizeof(float[3]));
						memcpy(rsurface.batchtvector3f + 3*numvertices, rsurface.modeltvector3f + 3*surfacefirstvertex, surfacenumvertices * sizeof(float[3]));
					}
					else
					{
						memset(rsurface.batchsvector3f + 3*numvertices, 0, surfacenumvertices * sizeof(float[3]));
						memset(rsurface.batchtvector3f + 3*numvertices, 0, surfacenumvertices * sizeof(float[3]));
					}
				}
				if (batchneed & BATCHNEED_ARRAY_VERTEXCOLOR)
				{
					if (rsurface.modellightmapcolor4f)
						memcpy(rsurface.batchlightmapcolor4f + 4*numvertices, rsurface.modellightmapcolor4f + 4*surfacefirstvertex, surfacenumvertices * sizeof(float[4]));
					else
						memset(rsurface.batchlightmapcolor4f + 4*numvertices, 0, surfacenumvertices * sizeof(float[4]));
				}
				if (batchneed & BATCHNEED_ARRAY_TEXCOORD)
				{
					if (rsurface.modeltexcoordtexture2f)
						memcpy(rsurface.batchtexcoordtexture2f + 2*numvertices, rsurface.modeltexcoordtexture2f + 2*surfacefirstvertex, surfacenumvertices * sizeof(float[2]));
					else
						memset(rsurface.batchtexcoordtexture2f + 2*numvertices, 0, surfacenumvertices * sizeof(float[2]));
				}
				if (batchneed & BATCHNEED_ARRAY_LIGHTMAP)
				{
					if (rsurface.modeltexcoordlightmap2f)
						memcpy(rsurface.batchtexcoordlightmap2f + 2*numvertices, rsurface.modeltexcoordlightmap2f + 2*surfacefirstvertex, surfacenumvertices * sizeof(float[2]));
					else
						memset(rsurface.batchtexcoordlightmap2f + 2*numvertices, 0, surfacenumvertices * sizeof(float[2]));
				}
			}
			RSurf_RenumberElements(rsurface.modelelement3i + 3*surfacefirsttriangle, rsurface.batchelement3i + 3*numtriangles, 3*surfacenumtriangles, numvertices - surfacefirstvertex);
			numvertices += surfacenumvertices;
			numtriangles += surfacenumtriangles;
		}

		// generate a 16bit index array as well if possible
		// (in general, dynamic batches fit)
		if (numvertices <= 65536)
		{
			rsurface.batchelement3s = (unsigned short *)R_FrameData_Alloc(batchnumtriangles * sizeof(unsigned short[3]));
			for (i = 0;i < numtriangles*3;i++)
				rsurface.batchelement3s[i] = rsurface.batchelement3i[i];
		}

		// since we've copied everything, the batch now starts at 0
		rsurface.batchfirstvertex = 0;
		rsurface.batchnumvertices = batchnumvertices;
		rsurface.batchfirsttriangle = 0;
		rsurface.batchnumtriangles = batchnumtriangles;
	}

	// q1bsp surfaces rendered in vertex color mode have to have colors
	// calculated based on lightstyles
	if ((batchneed & (BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_ARRAY_VERTEXCOLOR)) && texturesurfacelist[0]->lightmapinfo)
	{
		// generate color arrays for the surfaces in this list
		int c[4];
		int scale;
		int size3;
		const int *offsets;
		const unsigned char *lm;
		rsurface.batchlightmapcolor4f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[4]));
		rsurface.batchlightmapcolor4f_vertexbuffer = NULL;
		rsurface.batchlightmapcolor4f_bufferoffset = 0;
		numvertices = 0;
		for (i = 0;i < texturenumsurfaces;i++)
		{
			surface = texturesurfacelist[i];
			offsets = rsurface.modellightmapoffsets + surface->num_firstvertex;
			surfacenumvertices = surface->num_vertices;
			if (surface->lightmapinfo->samples)
			{
				for (j = 0;j < surfacenumvertices;j++)
				{
					lm = surface->lightmapinfo->samples + offsets[j];
					scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[0]];
					VectorScale(lm, scale, c);
					if (surface->lightmapinfo->styles[1] != 255)
					{
						size3 = ((surface->lightmapinfo->extents[0]>>4)+1)*((surface->lightmapinfo->extents[1]>>4)+1)*3;
						lm += size3;
						scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[1]];
						VectorMA(c, scale, lm, c);
						if (surface->lightmapinfo->styles[2] != 255)
						{
							lm += size3;
							scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[2]];
							VectorMA(c, scale, lm, c);
							if (surface->lightmapinfo->styles[3] != 255)
							{
								lm += size3;
								scale = r_refdef.scene.lightstylevalue[surface->lightmapinfo->styles[3]];
								VectorMA(c, scale, lm, c);
							}
						}
					}
					c[0] >>= 7;
					c[1] >>= 7;
					c[2] >>= 7;
					Vector4Set(rsurface.batchlightmapcolor4f + 4*numvertices, min(c[0], 255) * (1.0f / 255.0f), min(c[1], 255) * (1.0f / 255.0f), min(c[2], 255) * (1.0f / 255.0f), 1);
					numvertices++;
				}
			}
			else
			{
				for (j = 0;j < surfacenumvertices;j++)
				{
					Vector4Set(rsurface.batchlightmapcolor4f + 4*numvertices, 0, 0, 0, 1);
					numvertices++;
				}
			}
		}
	}

	// if vertices are deformed (sprite flares and things in maps, possibly
	// water waves, bulges and other deformations), modify the copied vertices
	// in place
	for (deformindex = 0, deform = rsurface.texture->deforms;deformindex < Q3MAXDEFORMS && deform->deform && r_deformvertexes.integer;deformindex++, deform++)
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
//			rsurface.batchvertex3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchvertex3f);
//			rsurface.batchvertex3f_vertexbuffer = NULL;
//			rsurface.batchvertex3f_bufferoffset = 0;
//			rsurface.batchsvector3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchsvector3f);
//			rsurface.batchsvector3f_vertexbuffer = NULL;
//			rsurface.batchsvector3f_bufferoffset = 0;
//			rsurface.batchtvector3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchtvector3f);
//			rsurface.batchtvector3f_vertexbuffer = NULL;
//			rsurface.batchtvector3f_bufferoffset = 0;
//			rsurface.batchnormal3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchnormal3f);
//			rsurface.batchnormal3f_vertexbuffer = NULL;
//			rsurface.batchnormal3f_bufferoffset = 0;
			// sometimes we're on a renderpath that does not use vectors (GL11/GL13/GLES1)
			if (!VectorLength2(rsurface.batchnormal3f + 3*rsurface.batchfirstvertex))
				Mod_BuildNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchnormal3f, r_smoothnormals_areaweighting.integer != 0);
			if (!VectorLength2(rsurface.batchsvector3f + 3*rsurface.batchfirstvertex))
				Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			// a single autosprite surface can contain multiple sprites...
			for (j = 0;j < batchnumvertices - 3;j += 4)
			{
				VectorClear(center);
				for (i = 0;i < 4;i++)
					VectorAdd(center, rsurface.batchvertex3f + 3*(j+i), center);
				VectorScale(center, 0.25f, center);
				VectorCopy(rsurface.batchnormal3f + 3*j, forward);
				VectorCopy(rsurface.batchsvector3f + 3*j, right);
				VectorCopy(rsurface.batchtvector3f + 3*j, up);
				for (i = 0;i < 4;i++)
				{
					VectorSubtract(rsurface.batchvertex3f + 3*(j+i), center, v);
					VectorMAMAMAM(1, center, DotProduct(forward, v), newforward, DotProduct(right, v), newright, DotProduct(up, v), newup, rsurface.batchvertex3f + 3*(j+i));
				}
			}
			// if we get here, BATCHNEED_ARRAY_NORMAL and BATCHNEED_ARRAY_VECTOR are in batchneed, so no need to check
			Mod_BuildNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchnormal3f, r_smoothnormals_areaweighting.integer != 0);
			Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			break;
		case Q3DEFORM_AUTOSPRITE2:
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.forward, newforward);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.right, newright);
			Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.up, newup);
			VectorNormalize(newforward);
			VectorNormalize(newright);
			VectorNormalize(newup);
//			rsurface.batchvertex3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchvertex3f);
//			rsurface.batchvertex3f_vertexbuffer = NULL;
//			rsurface.batchvertex3f_bufferoffset = 0;
			{
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
				for (j = 0;j < batchnumvertices - 3;j += 4)
				{
					VectorClear(center);
					for (i = 0;i < 4;i++)
						VectorAdd(center, rsurface.batchvertex3f + 3*(j+i), center);
					VectorScale(center, 0.25f, center);
					// find the two shortest edges, then use them to define the
					// axis vectors for rotating around the central axis
					for (i = 0;i < 6;i++)
					{
						v1 = rsurface.batchvertex3f + 3*(j+quadedges[i][0]);
						v2 = rsurface.batchvertex3f + 3*(j+quadedges[i][1]);
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
						v1 = rsurface.batchvertex3f + 3*(j+i);
						f = DotProduct(right, v1) - l;
						VectorMAMAM(1, v1, -f, right, f, newright, rsurface.batchvertex3f + 3*(j+i));
					}
				}
			}
			if(batchneed & (BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR)) // otherwise these can stay NULL
			{
//				rsurface.batchnormal3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchnormal3f_vertexbuffer = NULL;
//				rsurface.batchnormal3f_bufferoffset = 0;
				Mod_BuildNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchnormal3f, r_smoothnormals_areaweighting.integer != 0);
			}
			if(batchneed & BATCHNEED_ARRAY_VECTOR) // otherwise these can stay NULL
			{
//				rsurface.batchsvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchsvector3f_vertexbuffer = NULL;
//				rsurface.batchsvector3f_bufferoffset = 0;
//				rsurface.batchtvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchtvector3f_vertexbuffer = NULL;
//				rsurface.batchtvector3f_bufferoffset = 0;
				Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			break;
		case Q3DEFORM_NORMAL:
			// deform the normals to make reflections wavey
			rsurface.batchnormal3f = (float *)R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchnormal3f);
			rsurface.batchnormal3f_vertexbuffer = NULL;
			rsurface.batchnormal3f_bufferoffset = 0;
			for (j = 0;j < batchnumvertices;j++)
			{
				float vertex[3];
				float *normal = rsurface.batchnormal3f + 3*j;
				VectorScale(rsurface.batchvertex3f + 3*j, 0.98f, vertex);
				normal[0] = rsurface.batchnormal3f[j*3+0] + deform->parms[0] * noise4f(      vertex[0], vertex[1], vertex[2], rsurface.shadertime * deform->parms[1]);
				normal[1] = rsurface.batchnormal3f[j*3+1] + deform->parms[0] * noise4f( 98 + vertex[0], vertex[1], vertex[2], rsurface.shadertime * deform->parms[1]);
				normal[2] = rsurface.batchnormal3f[j*3+2] + deform->parms[0] * noise4f(196 + vertex[0], vertex[1], vertex[2], rsurface.shadertime * deform->parms[1]);
				VectorNormalize(normal);
			}
			if(batchneed & BATCHNEED_ARRAY_VECTOR) // otherwise these can stay NULL
			{
//				rsurface.batchsvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchsvector3f_vertexbuffer = NULL;
//				rsurface.batchsvector3f_bufferoffset = 0;
//				rsurface.batchtvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchtvector3f_vertexbuffer = NULL;
//				rsurface.batchtvector3f_bufferoffset = 0;
				Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			break;
		case Q3DEFORM_WAVE:
			// deform vertex array to make wavey water and flags and such
			waveparms[0] = deform->waveparms[0];
			waveparms[1] = deform->waveparms[1];
			waveparms[2] = deform->waveparms[2];
			waveparms[3] = deform->waveparms[3];
			if(!R_TestQ3WaveFunc(deform->wavefunc, waveparms))
				break; // if wavefunc is a nop, don't make a dynamic vertex array
			// this is how a divisor of vertex influence on deformation
			animpos = deform->parms[0] ? 1.0f / deform->parms[0] : 100.0f;
			scale = R_EvaluateQ3WaveFunc(deform->wavefunc, waveparms);
//			rsurface.batchvertex3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchvertex3f);
//			rsurface.batchvertex3f_vertexbuffer = NULL;
//			rsurface.batchvertex3f_bufferoffset = 0;
//			rsurface.batchnormal3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchnormal3f);
//			rsurface.batchnormal3f_vertexbuffer = NULL;
//			rsurface.batchnormal3f_bufferoffset = 0;
			for (j = 0;j < batchnumvertices;j++)
			{
				// if the wavefunc depends on time, evaluate it per-vertex
				if (waveparms[3])
				{
					waveparms[2] = deform->waveparms[2] + (rsurface.batchvertex3f[j*3+0] + rsurface.batchvertex3f[j*3+1] + rsurface.batchvertex3f[j*3+2]) * animpos;
					scale = R_EvaluateQ3WaveFunc(deform->wavefunc, waveparms);
				}
				VectorMA(rsurface.batchvertex3f + 3*j, scale, rsurface.batchnormal3f + 3*j, rsurface.batchvertex3f + 3*j);
			}
			// if we get here, BATCHNEED_ARRAY_NORMAL is in batchneed, so no need to check
			Mod_BuildNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchnormal3f, r_smoothnormals_areaweighting.integer != 0);
			if(batchneed & BATCHNEED_ARRAY_VECTOR) // otherwise these can stay NULL
			{
//				rsurface.batchsvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchsvector3f_vertexbuffer = NULL;
//				rsurface.batchsvector3f_bufferoffset = 0;
//				rsurface.batchtvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchtvector3f_vertexbuffer = NULL;
//				rsurface.batchtvector3f_bufferoffset = 0;
				Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			break;
		case Q3DEFORM_BULGE:
			// deform vertex array to make the surface have moving bulges
//			rsurface.batchvertex3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchvertex3f);
//			rsurface.batchvertex3f_vertexbuffer = NULL;
//			rsurface.batchvertex3f_bufferoffset = 0;
//			rsurface.batchnormal3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchnormal3f);
//			rsurface.batchnormal3f_vertexbuffer = NULL;
//			rsurface.batchnormal3f_bufferoffset = 0;
			for (j = 0;j < batchnumvertices;j++)
			{
				scale = sin(rsurface.batchtexcoordtexture2f[j*2+0] * deform->parms[0] + rsurface.shadertime * deform->parms[2]) * deform->parms[1];
				VectorMA(rsurface.batchvertex3f + 3*j, scale, rsurface.batchnormal3f + 3*j, rsurface.batchvertex3f + 3*j);
			}
			// if we get here, BATCHNEED_ARRAY_NORMAL is in batchneed, so no need to check
			Mod_BuildNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchnormal3f, r_smoothnormals_areaweighting.integer != 0);
			if(batchneed & BATCHNEED_ARRAY_VECTOR) // otherwise these can stay NULL
			{
//				rsurface.batchsvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchsvector3f_vertexbuffer = NULL;
//				rsurface.batchsvector3f_bufferoffset = 0;
//				rsurface.batchtvector3f = R_FrameData_Alloc(batchnumvertices * sizeof(float[3]));
//				rsurface.batchtvector3f_vertexbuffer = NULL;
//				rsurface.batchtvector3f_bufferoffset = 0;
				Mod_BuildTextureVectorsFromNormals(rsurface.batchfirstvertex, batchnumvertices, batchnumtriangles, rsurface.batchvertex3f, rsurface.batchtexcoordtexture2f, rsurface.batchnormal3f, rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle, rsurface.batchsvector3f, rsurface.batchtvector3f, r_smoothnormals_areaweighting.integer != 0);
			}
			break;
		case Q3DEFORM_MOVE:
			// deform vertex array
			if(!R_TestQ3WaveFunc(deform->wavefunc, deform->waveparms))
				break; // if wavefunc is a nop, don't make a dynamic vertex array
			scale = R_EvaluateQ3WaveFunc(deform->wavefunc, deform->waveparms);
			VectorScale(deform->parms, scale, waveparms);
//			rsurface.batchvertex3f = R_FrameData_Store(batchnumvertices * sizeof(float[3]), rsurface.batchvertex3f);
//			rsurface.batchvertex3f_vertexbuffer = NULL;
//			rsurface.batchvertex3f_bufferoffset = 0;
			for (j = 0;j < batchnumvertices;j++)
				VectorAdd(rsurface.batchvertex3f + 3*j, waveparms, rsurface.batchvertex3f + 3*j);
			break;
		}
	}

	// generate texcoords based on the chosen texcoord source
	switch(rsurface.texture->tcgen.tcgen)
	{
	default:
	case Q3TCGEN_TEXTURE:
		break;
	case Q3TCGEN_LIGHTMAP:
//		rsurface.batchtexcoordtexture2f = R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
//		rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
//		rsurface.batchtexcoordtexture2f_bufferoffset = 0;
		if (rsurface.batchtexcoordlightmap2f)
			memcpy(rsurface.batchtexcoordlightmap2f, rsurface.batchtexcoordtexture2f, batchnumvertices * sizeof(float[2]));
		break;
	case Q3TCGEN_VECTOR:
//		rsurface.batchtexcoordtexture2f = R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
//		rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
//		rsurface.batchtexcoordtexture2f_bufferoffset = 0;
		for (j = 0;j < batchnumvertices;j++)
		{
			rsurface.batchtexcoordtexture2f[j*2+0] = DotProduct(rsurface.batchvertex3f + 3*j, rsurface.texture->tcgen.parms);
			rsurface.batchtexcoordtexture2f[j*2+1] = DotProduct(rsurface.batchvertex3f + 3*j, rsurface.texture->tcgen.parms + 3);
		}
		break;
	case Q3TCGEN_ENVIRONMENT:
		// make environment reflections using a spheremap
		rsurface.batchtexcoordtexture2f = (float *)R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
		rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
		rsurface.batchtexcoordtexture2f_bufferoffset = 0;
		for (j = 0;j < batchnumvertices;j++)
		{
			// identical to Q3A's method, but executed in worldspace so
			// carried models can be shiny too

			float viewer[3], d, reflected[3], worldreflected[3];

			VectorSubtract(rsurface.localvieworigin, rsurface.batchvertex3f + 3*j, viewer);
			// VectorNormalize(viewer);

			d = DotProduct(rsurface.batchnormal3f + 3*j, viewer);

			reflected[0] = rsurface.batchnormal3f[j*3+0]*2*d - viewer[0];
			reflected[1] = rsurface.batchnormal3f[j*3+1]*2*d - viewer[1];
			reflected[2] = rsurface.batchnormal3f[j*3+2]*2*d - viewer[2];
			// note: this is proportinal to viewer, so we can normalize later

			Matrix4x4_Transform3x3(&rsurface.matrix, reflected, worldreflected);
			VectorNormalize(worldreflected);

			// note: this sphere map only uses world x and z!
			// so positive and negative y will LOOK THE SAME.
			rsurface.batchtexcoordtexture2f[j*2+0] = 0.5 + 0.5 * worldreflected[1];
			rsurface.batchtexcoordtexture2f[j*2+1] = 0.5 - 0.5 * worldreflected[2];
		}
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
		animpos = rsurface.texture->tcmods[0].parms[2] + rsurface.shadertime * rsurface.texture->tcmods[0].parms[3];
//		rsurface.batchtexcoordtexture2f = R_FrameData_Alloc(batchnumvertices * sizeof(float[2]));
//		rsurface.batchtexcoordtexture2f_vertexbuffer = NULL;
//		rsurface.batchtexcoordtexture2f_bufferoffset = 0;
		for (j = 0;j < batchnumvertices;j++)
		{
			rsurface.batchtexcoordtexture2f[j*2+0] += amplitude * sin(((rsurface.batchvertex3f[j*3+0] + rsurface.batchvertex3f[j*3+2]) * 1.0 / 1024.0f + animpos) * M_PI * 2);
			rsurface.batchtexcoordtexture2f[j*2+1] += amplitude * sin(((rsurface.batchvertex3f[j*3+1]                                ) * 1.0 / 1024.0f + animpos) * M_PI * 2);
		}
	}

	if (needsupdate & batchneed & (BATCHNEED_VERTEXMESH_VERTEX | BATCHNEED_VERTEXMESH_NORMAL | BATCHNEED_VERTEXMESH_VECTOR | BATCHNEED_VERTEXMESH_VERTEXCOLOR | BATCHNEED_VERTEXMESH_TEXCOORD | BATCHNEED_VERTEXMESH_LIGHTMAP))
	{
		// convert the modified arrays to vertex structs
//		rsurface.batchvertexmesh = R_FrameData_Alloc(batchnumvertices * sizeof(r_vertexmesh_t));
//		rsurface.batchvertexmeshbuffer = NULL;
		if (batchneed & BATCHNEED_VERTEXMESH_VERTEX)
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
				VectorCopy(rsurface.batchvertex3f + 3*j, vertexmesh->vertex3f);
		if (batchneed & BATCHNEED_VERTEXMESH_NORMAL)
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
				VectorCopy(rsurface.batchnormal3f + 3*j, vertexmesh->normal3f);
		if (batchneed & BATCHNEED_VERTEXMESH_VECTOR)
		{
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
			{
				VectorCopy(rsurface.batchsvector3f + 3*j, vertexmesh->svector3f);
				VectorCopy(rsurface.batchtvector3f + 3*j, vertexmesh->tvector3f);
			}
		}
		if ((batchneed & BATCHNEED_VERTEXMESH_VERTEXCOLOR) && rsurface.batchlightmapcolor4f)
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
				Vector4Copy(rsurface.batchlightmapcolor4f + 4*j, vertexmesh->color4f);
		if (batchneed & BATCHNEED_VERTEXMESH_TEXCOORD)
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
				Vector2Copy(rsurface.batchtexcoordtexture2f + 2*j, vertexmesh->texcoordtexture2f);
		if ((batchneed & BATCHNEED_VERTEXMESH_LIGHTMAP) && rsurface.batchtexcoordlightmap2f)
			for (j = 0, vertexmesh = rsurface.batchvertexmesh;j < batchnumvertices;j++, vertexmesh++)
				Vector2Copy(rsurface.batchtexcoordlightmap2f + 2*j, vertexmesh->texcoordlightmap2f);
	}
}

void RSurf_DrawBatch(void)
{
	// sometimes a zero triangle surface (usually a degenerate patch) makes it
	// through the pipeline, killing it earlier in the pipeline would have
	// per-surface overhead rather than per-batch overhead, so it's best to
	// reject it here, before it hits glDraw.
	if (rsurface.batchnumtriangles == 0)
		return;
#if 0
	// batch debugging code
	if (r_test.integer && rsurface.entity == r_refdef.scene.worldentity && rsurface.batchvertex3f == r_refdef.scene.worldentity->model->surfmesh.data_vertex3f)
	{
		int i;
		int j;
		int c;
		const int *e;
		e = rsurface.batchelement3i + rsurface.batchfirsttriangle*3;
		for (i = 0;i < rsurface.batchnumtriangles*3;i++)
		{
			c = e[i];
			for (j = 0;j < rsurface.entity->model->num_surfaces;j++)
			{
				if (c >= rsurface.modelsurfaces[j].num_firstvertex && c < (rsurface.modelsurfaces[j].num_firstvertex + rsurface.modelsurfaces[j].num_vertices))
				{
					if (rsurface.modelsurfaces[j].texture != rsurface.texture)
						Sys_Error("RSurf_DrawBatch: index %i uses different texture (%s) than surface %i which it belongs to (which uses %s)\n", c, rsurface.texture->name, j, rsurface.modelsurfaces[j].texture->name);
					break;
				}
			}
		}
	}
#endif
	R_Mesh_Draw(rsurface.batchfirstvertex, rsurface.batchnumvertices, rsurface.batchfirsttriangle, rsurface.batchnumtriangles, rsurface.batchelement3i, rsurface.batchelement3i_indexbuffer, rsurface.batchelement3i_bufferoffset, rsurface.batchelement3s, rsurface.batchelement3s_indexbuffer, rsurface.batchelement3s_bufferoffset);
}

static int RSurf_FindWaterPlaneForSurface(const msurface_t *surface)
{
	// pick the closest matching water plane
	int planeindex, vertexindex, bestplaneindex = -1;
	float d, bestd;
	vec3_t vert;
	const float *v;
	r_waterstate_waterplane_t *p;
	qboolean prepared = false;
	bestd = 0;
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		if(p->camera_entity != rsurface.texture->camera_entity)
			continue;
		d = 0;
		if(!prepared)
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, 1, &surface);
			prepared = true;
			if(rsurface.batchnumvertices == 0)
				break;
		}
		for (vertexindex = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3;vertexindex < rsurface.batchnumvertices;vertexindex++, v += 3)
		{
			Matrix4x4_Transform(&rsurface.matrix, v, vert);
			d += fabs(PlaneDiff(vert, &p->plane));
		}
		if (bestd > d || bestplaneindex < 0)
		{
			bestd = d;
			bestplaneindex = planeindex;
		}
	}
	return bestplaneindex;
	// NOTE: this MAY return a totally unrelated water plane; we can ignore
	// this situation though, as it might be better to render single larger
	// batches with useless stuff (backface culled for example) than to
	// render multiple smaller batches
}

static void RSurf_DrawBatch_GL11_MakeFullbrightLightmapColorArray(void)
{
	int i;
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	for (i = 0;i < rsurface.batchnumvertices;i++)
		Vector4Set(rsurface.passcolor4f + 4*i, 0.5f, 0.5f, 0.5f, 1.0f);
}

static void RSurf_DrawBatch_GL11_ApplyFog(void)
{
	int i;
	float f;
	const float *v;
	const float *c;
	float *c2;
	if (rsurface.passcolor4f)
	{
		// generate color arrays
		c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;
		rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
		rsurface.passcolor4f_vertexbuffer = 0;
		rsurface.passcolor4f_bufferoffset = 0;
		for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4, c2 = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, c += 4, c2 += 4)
		{
			f = RSurf_FogVertex(v);
			c2[0] = c[0] * f;
			c2[1] = c[1] * f;
			c2[2] = c[2] * f;
			c2[3] = c[3];
		}
	}
	else
	{
		rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
		rsurface.passcolor4f_vertexbuffer = 0;
		rsurface.passcolor4f_bufferoffset = 0;
		for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, c2 = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, c2 += 4)
		{
			f = RSurf_FogVertex(v);
			c2[0] = f;
			c2[1] = f;
			c2[2] = f;
			c2[3] = 1;
		}
	}
}

static void RSurf_DrawBatch_GL11_ApplyFogToFinishedVertexColors(void)
{
	int i;
	float f;
	const float *v;
	const float *c;
	float *c2;
	if (!rsurface.passcolor4f)
		return;
	c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, c2 = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, c += 4, c2 += 4)
	{
		f = RSurf_FogVertex(v);
		c2[0] = c[0] * f + r_refdef.fogcolor[0] * (1 - f);
		c2[1] = c[1] * f + r_refdef.fogcolor[1] * (1 - f);
		c2[2] = c[2] * f + r_refdef.fogcolor[2] * (1 - f);
		c2[3] = c[3];
	}
}

static void RSurf_DrawBatch_GL11_ApplyColor(float r, float g, float b, float a)
{
	int i;
	const float *c;
	float *c2;
	if (!rsurface.passcolor4f)
		return;
	c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	for (i = 0, c2 = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, c += 4, c2 += 4)
	{
		c2[0] = c[0] * r;
		c2[1] = c[1] * g;
		c2[2] = c[2] * b;
		c2[3] = c[3] * a;
	}
}

static void RSurf_DrawBatch_GL11_ApplyAmbient(void)
{
	int i;
	const float *c;
	float *c2;
	if (!rsurface.passcolor4f)
		return;
	c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	for (i = 0, c2 = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, c += 4, c2 += 4)
	{
		c2[0] = c[0] + r_refdef.scene.ambient;
		c2[1] = c[1] + r_refdef.scene.ambient;
		c2[2] = c[2] + r_refdef.scene.ambient;
		c2[3] = c[3];
	}
}

static void RSurf_DrawBatch_GL11_Lightmap(float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize
	rsurface.passcolor4f = NULL;
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog();
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(r, g, b, a);
	R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, rsurface.passcolor4f_vertexbuffer, rsurface.passcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	R_Mesh_TexBind(0, rsurface.lightmaptexture);
	RSurf_DrawBatch();
}

static void RSurf_DrawBatch_GL11_Unlit(float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize applyfog && applycolor case
	// just apply fog if necessary, and tint the fog color array if necessary
	rsurface.passcolor4f = NULL;
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog();
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(r, g, b, a);
	R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, rsurface.passcolor4f_vertexbuffer, rsurface.passcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch();
}

static void RSurf_DrawBatch_GL11_VertexColor(float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize
	rsurface.passcolor4f = rsurface.batchlightmapcolor4f;
	rsurface.passcolor4f_vertexbuffer = rsurface.batchlightmapcolor4f_vertexbuffer;
	rsurface.passcolor4f_bufferoffset = rsurface.batchlightmapcolor4f_bufferoffset;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog();
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(r, g, b, a);
	R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, rsurface.passcolor4f_vertexbuffer, rsurface.passcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch();
}

static void RSurf_DrawBatch_GL11_ClampColor(void)
{
	int i;
	const float *c1;
	float *c2;
	if (!rsurface.passcolor4f)
		return;
	for (i = 0, c1 = rsurface.passcolor4f + 4*rsurface.batchfirstvertex, c2 = rsurface.passcolor4f + 4*rsurface.batchfirstvertex;i < rsurface.batchnumvertices;i++, c1 += 4, c2 += 4)
	{
		c2[0] = bound(0.0f, c1[0], 1.0f);
		c2[1] = bound(0.0f, c1[1], 1.0f);
		c2[2] = bound(0.0f, c1[2], 1.0f);
		c2[3] = bound(0.0f, c1[3], 1.0f);
	}
}

static void RSurf_DrawBatch_GL11_ApplyFakeLight(void)
{
	int i;
	float f;
	const float *v;
	const float *n;
	float *c;
	//vec3_t eyedir;

	// fake shading
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;
	for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, n = rsurface.batchnormal3f + rsurface.batchfirstvertex * 3, c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, n += 3, c += 4)
	{
		f = -DotProduct(r_refdef.view.forward, n);
		f = max(0, f);
		f = f * 0.85 + 0.15; // work around so stuff won't get black
		f *= r_refdef.lightmapintensity;
		Vector4Set(c, f, f, f, 1);
	}
}

static void RSurf_DrawBatch_GL11_FakeLight(float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	RSurf_DrawBatch_GL11_ApplyFakeLight();
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog();
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(r, g, b, a);
	R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, rsurface.passcolor4f_vertexbuffer, rsurface.passcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch();
}

static void RSurf_DrawBatch_GL11_ApplyVertexShade(float *r, float *g, float *b, float *a, qboolean *applycolor)
{
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
	if (VectorLength2(diffusecolor) > 0)
	{
		// q3-style directional shading
		rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
		rsurface.passcolor4f_vertexbuffer = 0;
		rsurface.passcolor4f_bufferoffset = 0;
		for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, n = rsurface.batchnormal3f + rsurface.batchfirstvertex * 3, c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, n += 3, c += 4)
		{
			if ((f = DotProduct(n, lightdir)) > 0)
				VectorMA(ambientcolor, f, diffusecolor, c);
			else
				VectorCopy(ambientcolor, c);
			c[3] = alpha;
		}
		*r = 1;
		*g = 1;
		*b = 1;
		*a = 1;
		*applycolor = false;
	}
	else
	{
		*r = ambientcolor[0];
		*g = ambientcolor[1];
		*b = ambientcolor[2];
		rsurface.passcolor4f = NULL;
		rsurface.passcolor4f_vertexbuffer = 0;
		rsurface.passcolor4f_bufferoffset = 0;
	}
}

static void RSurf_DrawBatch_GL11_VertexShade(float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	RSurf_DrawBatch_GL11_ApplyVertexShade(&r, &g, &b, &a, &applycolor);
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog();
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(r, g, b, a);
	R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, rsurface.passcolor4f_vertexbuffer, rsurface.passcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch();
}

static void RSurf_DrawBatch_GL11_MakeFogColor(float r, float g, float b, float a)
{
	int i;
	float f;
	const float *v;
	float *c;

	// fake shading
	rsurface.passcolor4f = (float *)R_FrameData_Alloc(rsurface.batchnumvertices * sizeof(float[4]));
	rsurface.passcolor4f_vertexbuffer = 0;
	rsurface.passcolor4f_bufferoffset = 0;

	for (i = 0, v = rsurface.batchvertex3f + rsurface.batchfirstvertex * 3, c = rsurface.passcolor4f + rsurface.batchfirstvertex * 4;i < rsurface.batchnumvertices;i++, v += 3, c += 4)
	{
		f = 1 - RSurf_FogVertex(v);
		c[0] = r;
		c[1] = g;
		c[2] = b;
		c[3] = f * a;
	}
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
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
	skyrenderlater = true;
	RSurf_SetupDepthAndCulling();
	GL_DepthMask(true);
	// LordHavoc: HalfLife maps have freaky skypolys so don't use
	// skymasking on them, and Quake3 never did sky masking (unlike
	// software Quake and software Quake2), so disable the sky masking
	// in Quake3 maps as it causes problems with q3map2 sky tricks,
	// and skymasking also looks very bad when noclipping outside the
	// level, so don't use it then either.
	if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->type == mod_brushq1 && r_q1bsp_skymasking.integer && !r_refdef.viewcache.world_novis && !r_trippy.integer)
	{
		R_Mesh_ResetTextureState();
		if (skyrendermasked)
		{
			R_SetupShader_DepthOrShadow(false);
			// depth-only (masking)
			GL_ColorMask(0,0,0,0);
			// just to make sure that braindead drivers don't draw
			// anything despite that colormask...
			GL_BlendFunc(GL_ZERO, GL_ONE);
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
			if (rsurface.batchvertex3fbuffer)
				R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3fbuffer);
			else
				R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer);
		}
		else
		{
			R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
			// fog sky
			GL_BlendFunc(GL_ONE, GL_ZERO);
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
			GL_Color(r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2], 1);
			R_Mesh_PrepareVertices_Generic_Arrays(rsurface.batchnumvertices, rsurface.batchvertex3f, NULL, NULL);
		}
		RSurf_DrawBatch();
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
	if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION | MATERIALFLAG_CAMERA)))
		return;
	if (prepass)
	{
		// render screenspace normalmap to texture
		GL_DepthMask(true);
		R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_DEFERREDGEOMETRY, texturenumsurfaces, texturesurfacelist, NULL, false);
		RSurf_DrawBatch();
	}

	// bind lightmap texture

	// water/refraction/reflection/camera surfaces have to be handled specially
	if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_CAMERA | MATERIALFLAG_REFLECTION)))
	{
		int start, end, startplaneindex;
		for (start = 0;start < texturenumsurfaces;start = end)
		{
			startplaneindex = RSurf_FindWaterPlaneForSurface(texturesurfacelist[start]);
			if(startplaneindex < 0)
			{
				// this happens if the plane e.g. got backface culled and thus didn't get a water plane. We can just ignore this.
				// Con_Printf("No matching water plane for surface with material flags 0x%08x - PLEASE DEBUG THIS\n", rsurface.texture->currentmaterialflags);
				end = start + 1;
				continue;
			}
			for (end = start + 1;end < texturenumsurfaces && startplaneindex == RSurf_FindWaterPlaneForSurface(texturesurfacelist[end]);end++)
				;
			// now that we have a batch using the same planeindex, render it
			if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_CAMERA)))
			{
				// render water or distortion background
				GL_DepthMask(true);
				R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BACKGROUND, end-start, texturesurfacelist + start, (void *)(r_waterstate.waterplanes + startplaneindex), false);
				RSurf_DrawBatch();
				// blend surface on top
				GL_DepthMask(false);
				R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE, end-start, texturesurfacelist + start, NULL, false);
				RSurf_DrawBatch();
			}
			else if ((rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION))
			{
				// render surface with reflection texture as input
				GL_DepthMask(writedepth && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED));
				R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE, end-start, texturesurfacelist + start, (void *)(r_waterstate.waterplanes + startplaneindex), false);
				RSurf_DrawBatch();
			}
		}
		return;
	}

	// render surface batch normally
	GL_DepthMask(writedepth && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED));
	R_SetupShader_Surface(vec3_origin, (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT) != 0, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE, texturenumsurfaces, texturesurfacelist, NULL, (rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY) != 0);
	RSurf_DrawBatch();
}

static void R_DrawTextureSurfaceList_GL13(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	// OpenGL 1.3 path - anything not completely ancient
	qboolean applycolor;
	qboolean applyfog;
	int layerindex;
	const texturelayer_t *layer;
	RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | ((!rsurface.uselightmaptexture && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)) ? BATCHNEED_ARRAY_VERTEXCOLOR : 0) | BATCHNEED_ARRAY_TEXCOORD | (rsurface.modeltexcoordlightmap2f ? BATCHNEED_ARRAY_LIGHTMAP : 0) | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
	R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer, rsurface.batchvertex3f_bufferoffset);

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
				GL_DepthFunc(GL_EQUAL);
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
		R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, 0, 0);
		applyfog = r_refdef.fogenabled && (rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED);
		switch (layer->type)
		{
		case TEXTURELAYERTYPE_LITTEXTURE:
			// single-pass lightmapped texture with 2x rgbscale
			R_Mesh_TexBind(0, r_texture_white);
			R_Mesh_TexMatrix(0, NULL);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordlightmap2f, rsurface.batchtexcoordlightmap2f_vertexbuffer, rsurface.batchtexcoordlightmap2f_bufferoffset);
			R_Mesh_TexBind(1, layer->texture);
			R_Mesh_TexMatrix(1, &layer->texmatrix);
			R_Mesh_TexCombine(1, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
				RSurf_DrawBatch_GL11_VertexShade(layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else if (FAKELIGHT_ENABLED)
				RSurf_DrawBatch_GL11_FakeLight(layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else if (rsurface.uselightmaptexture)
				RSurf_DrawBatch_GL11_Lightmap(layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else
				RSurf_DrawBatch_GL11_VertexColor(layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			// singletexture unlit texture with transparency support
			R_Mesh_TexBind(0, layer->texture);
			R_Mesh_TexMatrix(0, &layer->texmatrix);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			R_Mesh_TexBind(1, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, 0, 0);
			RSurf_DrawBatch_GL11_Unlit(layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			// singletexture fogging
			if (layer->texture)
			{
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, layertexrgbscale, 1);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			}
			else
			{
				R_Mesh_TexBind(0, 0);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, 0, 0);
			}
			R_Mesh_TexBind(1, 0);
			R_Mesh_TexCoordPointer(1, 2, GL_FLOAT, sizeof(float[2]), NULL, 0, 0);
			// generate a color array for the fog pass
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, 0, 0);
			RSurf_DrawBatch_GL11_MakeFogColor(layercolor[0], layercolor[1], layercolor[2], layercolor[3]);
			RSurf_DrawBatch();
			break;
		default:
			Con_Printf("R_DrawTextureSurfaceList: unknown layer type %i\n", layer->type);
		}
	}
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		GL_DepthFunc(GL_LEQUAL);
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList_GL11(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	// OpenGL 1.1 - crusty old voodoo path
	qboolean applyfog;
	int layerindex;
	const texturelayer_t *layer;
	RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | ((!rsurface.uselightmaptexture && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)) ? BATCHNEED_ARRAY_VERTEXCOLOR : 0) | BATCHNEED_ARRAY_TEXCOORD | (rsurface.modeltexcoordlightmap2f ? BATCHNEED_ARRAY_LIGHTMAP : 0) | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
	R_Mesh_VertexPointer(3, GL_FLOAT, sizeof(float[3]), rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer, rsurface.batchvertex3f_bufferoffset);

	for (layerindex = 0, layer = rsurface.texture->currentlayers;layerindex < rsurface.texture->currentnumlayers;layerindex++, layer++)
	{
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
		{
			if (layerindex == 0)
				GL_AlphaTest(true);
			else
			{
				GL_AlphaTest(false);
				GL_DepthFunc(GL_EQUAL);
			}
		}
		GL_DepthMask(layer->depthmask && writedepth);
		GL_BlendFunc(layer->blendfunc1, layer->blendfunc2);
		R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), NULL, 0, 0);
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
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordlightmap2f, rsurface.batchtexcoordlightmap2f_vertexbuffer, rsurface.batchtexcoordlightmap2f_bufferoffset);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(1, 1, 1, 1, false, false);
				else if (FAKELIGHT_ENABLED)
					RSurf_DrawBatch_GL11_FakeLight(1, 1, 1, 1, false, false);
				else if (rsurface.uselightmaptexture)
					RSurf_DrawBatch_GL11_Lightmap(1, 1, 1, 1, false, false);
				else
					RSurf_DrawBatch_GL11_VertexColor(1, 1, 1, 1, false, false);
				// then apply the texture to it
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
				RSurf_DrawBatch_GL11_Unlit(layer->color[0] * 0.5f, layer->color[1] * 0.5f, layer->color[2] * 0.5f, layer->color[3], layer->color[0] != 2 || layer->color[1] != 2 || layer->color[2] != 2 || layer->color[3] != 1, false);
			}
			else
			{
				// single pass vertex-lighting-only texture with 1x rgbscale and transparency support
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
				else
					RSurf_DrawBatch_GL11_VertexColor(layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			}
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			// singletexture unlit texture with transparency support
			R_Mesh_TexBind(0, layer->texture);
			R_Mesh_TexMatrix(0, &layer->texmatrix);
			R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
			R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			RSurf_DrawBatch_GL11_Unlit(layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			// singletexture fogging
			if (layer->texture)
			{
				R_Mesh_TexBind(0, layer->texture);
				R_Mesh_TexMatrix(0, &layer->texmatrix);
				R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), rsurface.batchtexcoordtexture2f, rsurface.batchtexcoordtexture2f_vertexbuffer, rsurface.batchtexcoordtexture2f_bufferoffset);
			}
			else
			{
				R_Mesh_TexBind(0, 0);
				R_Mesh_TexCoordPointer(0, 2, GL_FLOAT, sizeof(float[2]), NULL, 0, 0);
			}
			// generate a color array for the fog pass
			R_Mesh_ColorPointer(4, GL_FLOAT, sizeof(float[4]), rsurface.passcolor4f, 0, 0);
			RSurf_DrawBatch_GL11_MakeFogColor(layer->color[0], layer->color[1], layer->color[2], layer->color[3]);
			RSurf_DrawBatch();
			break;
		default:
			Con_Printf("R_DrawTextureSurfaceList: unknown layer type %i\n", layer->type);
		}
	}
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		GL_DepthFunc(GL_LEQUAL);
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList_ShowSurfaces(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth)
{
	int vi;
	int j;
	r_vertexgeneric_t *batchvertex;
	float c[4];

//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);

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

	if (r_showsurfaces.integer == 3)
	{
		rsurface.passcolor4f = NULL;

		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);

			rsurface.passcolor4f = NULL;
			rsurface.passcolor4f_vertexbuffer = 0;
			rsurface.passcolor4f_bufferoffset = 0;
		}
		else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
		{
			qboolean applycolor = true;
			float one = 1.0;

			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);

			r_refdef.lightmapintensity = 1;
			RSurf_DrawBatch_GL11_ApplyVertexShade(&one, &one, &one, &one, &applycolor);
			r_refdef.lightmapintensity = 0; // we're in showsurfaces, after all
		}
		else if (FAKELIGHT_ENABLED)
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);

			r_refdef.lightmapintensity = r_fakelight_intensity.value;
			RSurf_DrawBatch_GL11_ApplyFakeLight();
			r_refdef.lightmapintensity = 0; // we're in showsurfaces, after all
		}
		else
		{
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_VERTEXCOLOR | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);

			rsurface.passcolor4f = rsurface.batchlightmapcolor4f;
			rsurface.passcolor4f_vertexbuffer = rsurface.batchlightmapcolor4f_vertexbuffer;
			rsurface.passcolor4f_bufferoffset = rsurface.batchlightmapcolor4f_bufferoffset;
		}

		if(!rsurface.passcolor4f)
			RSurf_DrawBatch_GL11_MakeFullbrightLightmapColorArray();

		RSurf_DrawBatch_GL11_ApplyAmbient();
		RSurf_DrawBatch_GL11_ApplyColor(c[0], c[1], c[2], c[3]);
		if(r_refdef.fogenabled)
			RSurf_DrawBatch_GL11_ApplyFogToFinishedVertexColors();
		RSurf_DrawBatch_GL11_ClampColor();

		R_Mesh_PrepareVertices_Generic_Arrays(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.passcolor4f, NULL);
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
		RSurf_DrawBatch();
	}
	else if (!r_refdef.view.showdebug)
	{
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
		batchvertex = R_Mesh_PrepareVertices_Generic_Lock(rsurface.batchnumvertices);
		for (j = 0, vi = rsurface.batchfirstvertex;j < rsurface.batchnumvertices;j++, vi++)
		{
			VectorCopy(rsurface.batchvertex3f + 3*vi, batchvertex[vi].vertex3f);
			Vector4Set(batchvertex[vi].color4f, 0, 0, 0, 1);
		}
		R_Mesh_PrepareVertices_Generic_Unlock();
		RSurf_DrawBatch();
	}
	else if (r_showsurfaces.integer == 4)
	{
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
		batchvertex = R_Mesh_PrepareVertices_Generic_Lock(rsurface.batchnumvertices);
		for (j = 0, vi = rsurface.batchfirstvertex;j < rsurface.batchnumvertices;j++, vi++)
		{
			unsigned char c = (vi << 3) * (1.0f / 256.0f);
			VectorCopy(rsurface.batchvertex3f + 3*vi, batchvertex[vi].vertex3f);
			Vector4Set(batchvertex[vi].color4f, c, c, c, 1);
		}
		R_Mesh_PrepareVertices_Generic_Unlock();
		RSurf_DrawBatch();
	}
	else if (r_showsurfaces.integer == 2)
	{
		const int *e;
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
		batchvertex = R_Mesh_PrepareVertices_Generic_Lock(3*rsurface.batchnumtriangles);
		for (j = 0, e = rsurface.batchelement3i + 3 * rsurface.batchfirsttriangle;j < rsurface.batchnumtriangles;j++, e += 3)
		{
			unsigned char c = ((j + rsurface.batchfirsttriangle) << 3) * (1.0f / 256.0f);
			VectorCopy(rsurface.batchvertex3f + 3*e[0], batchvertex[j*3+0].vertex3f);
			VectorCopy(rsurface.batchvertex3f + 3*e[1], batchvertex[j*3+1].vertex3f);
			VectorCopy(rsurface.batchvertex3f + 3*e[2], batchvertex[j*3+2].vertex3f);
			Vector4Set(batchvertex[j*3+0].color4f, c, c, c, 1);
			Vector4Set(batchvertex[j*3+1].color4f, c, c, c, 1);
			Vector4Set(batchvertex[j*3+2].color4f, c, c, c, 1);
		}
		R_Mesh_PrepareVertices_Generic_Unlock();
		R_Mesh_Draw(0, rsurface.batchnumtriangles*3, 0, rsurface.batchnumtriangles, NULL, NULL, 0, NULL, NULL, 0);
	}
	else
	{
		int texturesurfaceindex;
		int k;
		const msurface_t *surface;
		float surfacecolor4f[4];
		RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, texturenumsurfaces, texturesurfacelist);
		batchvertex = R_Mesh_PrepareVertices_Generic_Lock(rsurface.batchfirstvertex + rsurface.batchnumvertices);
		vi = 0;
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			surface = texturesurfacelist[texturesurfaceindex];
			k = (int)(((size_t)surface) / sizeof(msurface_t));
			Vector4Set(surfacecolor4f, (k & 0xF) * (1.0f / 16.0f), (k & 0xF0) * (1.0f / 256.0f), (k & 0xF00) * (1.0f / 4096.0f), 1);
			for (j = 0;j < surface->num_vertices;j++)
			{
				VectorCopy(rsurface.batchvertex3f + 3*vi, batchvertex[vi].vertex3f);
				Vector4Copy(surfacecolor4f, batchvertex[vi].color4f);
				vi++;
			}
		}
		R_Mesh_PrepareVertices_Generic_Unlock();
		RSurf_DrawBatch();
	}
}

static void R_DrawWorldTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean prepass)
{
	CHECKGLERROR
	RSurf_SetupDepthAndCulling();
	if (r_showsurfaces.integer)
	{
		R_DrawTextureSurfaceList_ShowSurfaces(texturenumsurfaces, texturesurfacelist, writedepth);
		return;
	}
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		R_DrawTextureSurfaceList_GL20(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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
	if (r_showsurfaces.integer)
	{
		R_DrawTextureSurfaceList_ShowSurfaces(texturenumsurfaces, texturesurfacelist, writedepth);
		return;
	}
	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_D3D9:
	case RENDERPATH_D3D10:
	case RENDERPATH_D3D11:
	case RENDERPATH_SOFT:
	case RENDERPATH_GLES2:
		R_DrawTextureSurfaceList_GL20(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GLES1:
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
	const msurface_t *texturesurfacelist[MESHQUEUE_TRANSPARENT_BATCHSIZE];

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
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
		case RENDERPATH_GLES2:
			RSurf_ActiveModelEntity(ent, true, true, false);
			break;
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
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
			rsurface.lightmaptexture = NULL;
			rsurface.deluxemaptexture = NULL;
			rsurface.uselightmaptexture = false;
			// scan ahead until we find a different texture
			endsurface = min(i + 1024, numsurfaces);
			texturenumsurfaces = 0;
			texturesurfacelist[texturenumsurfaces++] = surface;
			for (;j < endsurface;j++)
			{
				surface = rsurface.modelsurfaces + surfacelist[j];
				if (texture != surface->texture)
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
//				R_Mesh_ResetTextureState();
				R_SetupShader_DepthOrShadow(false);
			}
			RSurf_SetupDepthAndCulling();
			RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX, texturenumsurfaces, texturesurfacelist);
			if (rsurface.batchvertex3fbuffer)
				R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3fbuffer);
			else
				R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer);
			RSurf_DrawBatch();
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
		// scan ahead until we find a different texture
		endsurface = min(i + MESHQUEUE_TRANSPARENT_BATCHSIZE, numsurfaces);
		texturenumsurfaces = 0;
		texturesurfacelist[texturenumsurfaces++] = surface;
		if(FAKELIGHT_ENABLED)
		{
			rsurface.lightmaptexture = NULL;
			rsurface.deluxemaptexture = NULL;
			rsurface.uselightmaptexture = false;
			for (;j < endsurface;j++)
			{
				surface = rsurface.modelsurfaces + surfacelist[j];
				if (texture != surface->texture)
					break;
				texturesurfacelist[texturenumsurfaces++] = surface;
			}
		}
		else
		{
			rsurface.lightmaptexture = surface->lightmaptexture;
			rsurface.deluxemaptexture = surface->deluxemaptexture;
			rsurface.uselightmaptexture = surface->lightmaptexture != NULL;
			for (;j < endsurface;j++)
			{
				surface = rsurface.modelsurfaces + surfacelist[j];
				if (texture != surface->texture || rsurface.lightmaptexture != surface->lightmaptexture)
					break;
				texturesurfacelist[texturenumsurfaces++] = surface;
			}
		}
		// render the range of surfaces
		if (ent == r_refdef.scene.worldentity)
			R_DrawWorldTextureSurfaceList(texturenumsurfaces, texturesurfacelist, false, false);
		else
			R_DrawModelTextureSurfaceList(texturenumsurfaces, texturesurfacelist, false, false);
	}
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
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

static void R_DrawTextureSurfaceList_DepthOnly(int texturenumsurfaces, const msurface_t **texturesurfacelist)
{
	if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHATEST)))
		return;
	if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION)))
		return;
	RSurf_SetupDepthAndCulling();
	RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX, texturenumsurfaces, texturesurfacelist);
	if (rsurface.batchvertex3fbuffer)
		R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3fbuffer);
	else
		R_Mesh_PrepareVertices_Vertex3f(rsurface.batchnumvertices, rsurface.batchvertex3f, rsurface.batchvertex3f_vertexbuffer);
	RSurf_DrawBatch();
}

static void R_ProcessWorldTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean depthonly, qboolean prepass)
{
	const entity_render_t *queueentity = r_refdef.scene.worldentity;
	CHECKGLERROR
	if (depthonly)
		R_DrawTextureSurfaceList_DepthOnly(texturenumsurfaces, texturesurfacelist);
	else if (prepass)
	{
		if (!rsurface.texture->currentnumlayers)
			return;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
		else
			R_DrawWorldTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
	}
	else if ((rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY) && (!r_showsurfaces.integer || r_showsurfaces.integer == 3))
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
	R_FrameData_SetMark();
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
		if (!(rsurface.texture->currentmaterialflags & flagsmask) || (rsurface.texture->currentmaterialflags & MATERIALFLAG_NODRAW))
		{
			// if this texture is not the kind we want, skip ahead to the next one
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
			continue;
		}
		if(FAKELIGHT_ENABLED || depthonly || prepass)
		{
			rsurface.lightmaptexture = NULL;
			rsurface.deluxemaptexture = NULL;
			rsurface.uselightmaptexture = false;
			// simply scan ahead until we find a different texture or lightmap state
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
		}
		else
		{
			rsurface.lightmaptexture = surfacelist[i]->lightmaptexture;
			rsurface.deluxemaptexture = surfacelist[i]->deluxemaptexture;
			rsurface.uselightmaptexture = surfacelist[i]->lightmaptexture != NULL;
			// simply scan ahead until we find a different texture or lightmap state
			for (;j < numsurfaces && texture == surfacelist[j]->texture && rsurface.lightmaptexture == surfacelist[j]->lightmaptexture;j++)
				;
		}
		// render the range of surfaces
		R_ProcessWorldTextureSurfaceList(j - i, surfacelist + i, writedepth, depthonly, prepass);
	}
	R_FrameData_ReturnToMark();
}

static void R_ProcessModelTextureSurfaceList(int texturenumsurfaces, const msurface_t **texturesurfacelist, qboolean writedepth, qboolean depthonly, const entity_render_t *queueentity, qboolean prepass)
{
	CHECKGLERROR
	if (depthonly)
		R_DrawTextureSurfaceList_DepthOnly(texturenumsurfaces, texturesurfacelist);
	else if (prepass)
	{
		if (!rsurface.texture->currentnumlayers)
			return;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
			R_ProcessTransparentTextureSurfaceList(texturenumsurfaces, texturesurfacelist, queueentity);
		else
			R_DrawModelTextureSurfaceList(texturenumsurfaces, texturesurfacelist, writedepth, prepass);
	}
	else if ((rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY) && (!r_showsurfaces.integer || r_showsurfaces.integer == 3))
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
	R_FrameData_SetMark();
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
		if (!(rsurface.texture->currentmaterialflags & flagsmask) || (rsurface.texture->currentmaterialflags & MATERIALFLAG_NODRAW))
		{
			// if this texture is not the kind we want, skip ahead to the next one
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
			continue;
		}
		if(FAKELIGHT_ENABLED || depthonly || prepass)
		{
			rsurface.lightmaptexture = NULL;
			rsurface.deluxemaptexture = NULL;
			rsurface.uselightmaptexture = false;
			// simply scan ahead until we find a different texture or lightmap state
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
		}
		else
		{
			rsurface.lightmaptexture = surfacelist[i]->lightmaptexture;
			rsurface.deluxemaptexture = surfacelist[i]->deluxemaptexture;
			rsurface.uselightmaptexture = surfacelist[i]->lightmaptexture != NULL;
			// simply scan ahead until we find a different texture or lightmap state
			for (;j < numsurfaces && texture == surfacelist[j]->texture && rsurface.lightmaptexture == surfacelist[j]->lightmaptexture;j++)
				;
		}
		// render the range of surfaces
		R_ProcessModelTextureSurfaceList(j - i, surfacelist + i, writedepth, depthonly, ent, prepass);
	}
	R_FrameData_ReturnToMark();
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

//	R_Mesh_ResetTextureState();

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

	R_Mesh_PrepareVertices_Generic_Arrays(6*4, vertex3f, NULL, NULL);
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
	R_Mesh_Draw(0, 6*4, 0, 6*2, NULL, NULL, 0, locboxelements, NULL, 0);
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
		decalsystem->decals = (tridecal_t *)Mem_Alloc(cls.levelmempool, decalsystem->maxdecals * (sizeof(tridecal_t) + sizeof(float[3][3]) + sizeof(float[3][2]) + sizeof(float[3][4]) + sizeof(int[3]) + (useshortelements ? sizeof(unsigned short[3]) : 0)));
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
	for (i = decalsystem->freedecal;i < decalsystem->numdecals && decals[i].color4f[0][3];i++)
		;
	decalsystem->freedecal = i;
	if (decalsystem->numdecals <= i)
		decalsystem->numdecals = i + 1;

	// initialize the decal
	decal->lived = 0;
	decal->triangleindex = triangleindex;
	decal->surfaceindex = surfaceindex;
	decal->decalsequence = decalsequence;
	decal->color4f[0][0] = c0[0];
	decal->color4f[0][1] = c0[1];
	decal->color4f[0][2] = c0[2];
	decal->color4f[0][3] = 1;
	decal->color4f[1][0] = c1[0];
	decal->color4f[1][1] = c1[1];
	decal->color4f[1][2] = c1[2];
	decal->color4f[1][3] = 1;
	decal->color4f[2][0] = c2[0];
	decal->color4f[2][1] = c2[1];
	decal->color4f[2][2] = c2[2];
	decal->color4f[2][3] = 1;
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
	TriangleNormal(v0, v1, v2, decal->plane);
	VectorNormalize(decal->plane);
	decal->plane[3] = DotProduct(v0, decal->plane);
}

extern cvar_t cl_decals_bias;
extern cvar_t cl_decals_models;
extern cvar_t cl_decals_newsystem_intensitymultiplier;
// baseparms, parms, temps
static void R_DecalSystem_SplatTriangle(decalsystem_t *decalsystem, float r, float g, float b, float a, float s1, float t1, float s2, float t2, int decalsequence, qboolean dynamic, float (*planes)[4], matrix4x4_t *projection, int triangleindex, int surfaceindex)
{
	int cornerindex;
	int index;
	float v[9][3];
	const float *vertex3f;
	const float *normal3f;
	int numpoints;
	float points[2][9][3];
	float temp[3];
	float tc[9][2];
	float f;
	float c[9][4];
	const int *e;

	e = rsurface.modelelement3i + 3*triangleindex;

	vertex3f = rsurface.modelvertex3f;
	normal3f = rsurface.modelnormal3f;

	if (normal3f)
	{
		for (cornerindex = 0;cornerindex < 3;cornerindex++)
		{
			index = 3*e[cornerindex];
			VectorMA(vertex3f + index, cl_decals_bias.value, normal3f + index, v[cornerindex]);
		}
	}
	else
	{
		for (cornerindex = 0;cornerindex < 3;cornerindex++)
		{
			index = 3*e[cornerindex];
			VectorCopy(vertex3f + index, v[cornerindex]);
		}
	}

	// cull backfaces
	//TriangleNormal(v[0], v[1], v[2], normal);
	//if (DotProduct(normal, localnormal) < 0.0f)
	//	continue;
	// clip by each of the box planes formed from the projection matrix
	// if anything survives, we emit the decal
	numpoints = PolygonF_Clip(3        , v[0]        , planes[0][0], planes[0][1], planes[0][2], planes[0][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
	if (numpoints < 3)
		return;
	numpoints = PolygonF_Clip(numpoints, points[1][0], planes[1][0], planes[1][1], planes[1][2], planes[1][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[0][0]);
	if (numpoints < 3)
		return;
	numpoints = PolygonF_Clip(numpoints, points[0][0], planes[2][0], planes[2][1], planes[2][2], planes[2][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
	if (numpoints < 3)
		return;
	numpoints = PolygonF_Clip(numpoints, points[1][0], planes[3][0], planes[3][1], planes[3][2], planes[3][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[0][0]);
	if (numpoints < 3)
		return;
	numpoints = PolygonF_Clip(numpoints, points[0][0], planes[4][0], planes[4][1], planes[4][2], planes[4][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), points[1][0]);
	if (numpoints < 3)
		return;
	numpoints = PolygonF_Clip(numpoints, points[1][0], planes[5][0], planes[5][1], planes[5][2], planes[5][3], 1.0f/64.0f, sizeof(points[0])/sizeof(points[0][0]), v[0]);
	if (numpoints < 3)
		return;
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
		Matrix4x4_Transform(projection, v[cornerindex], temp);
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
		R_DecalSystem_SpawnTriangle(decalsystem, v[0], v[1], v[2], tc[0], tc[1], tc[2], c[0], c[1], c[2], triangleindex, surfaceindex, decalsequence);
	else
		for (cornerindex = 0;cornerindex < numpoints-2;cornerindex++)
			R_DecalSystem_SpawnTriangle(decalsystem, v[0], v[cornerindex+1], v[cornerindex+2], tc[0], tc[cornerindex+1], tc[cornerindex+2], c[0], c[cornerindex+1], c[cornerindex+2], -1, surfaceindex, decalsequence);
}
static void R_DecalSystem_SplatEntity(entity_render_t *ent, const vec3_t worldorigin, const vec3_t worldnormal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float worldsize, int decalsequence)
{
	matrix4x4_t projection;
	decalsystem_t *decalsystem;
	qboolean dynamic;
	dp_model_t *model;
	const msurface_t *surface;
	const msurface_t *surfaces;
	const int *surfacelist;
	const texture_t *texture;
	int numtriangles;
	int numsurfacelist;
	int surfacelistindex;
	int surfaceindex;
	int triangleindex;
	float localorigin[3];
	float localnormal[3];
	float localmins[3];
	float localmaxs[3];
	float localsize;
	//float normal[3];
	float planes[6][4];
	float angles[3];
	bih_t *bih;
	int bih_triangles_count;
	int bih_triangles[256];
	int bih_surfaces[256];

	decalsystem = &ent->decalsystem;
	model = ent->model;
	if (!model || !ent->allowdecals || ent->alpha < 1 || (ent->flags & (RENDER_ADDITIVE | RENDER_NODEPTHTEST)))
	{
		R_DecalSystem_Reset(&ent->decalsystem);
		return;
	}

	if (!model->brush.data_leafs && !cl_decals_models.integer)
	{
		if (decalsystem->model)
			R_DecalSystem_Reset(decalsystem);
		return;
	}

	if (decalsystem->model != model)
		R_DecalSystem_Reset(decalsystem);
	decalsystem->model = model;

	RSurf_ActiveModelEntity(ent, true, false, false);

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
	numsurfacelist = model->nummodelsurfaces;
	surfacelist = model->sortedmodelsurfaces;
	surfaces = model->data_surfaces;

	bih = NULL;
	bih_triangles_count = -1;
	if(!dynamic)
	{
		if(model->render_bih.numleafs)
			bih = &model->render_bih;
		else if(model->collision_bih.numleafs)
			bih = &model->collision_bih;
	}
	if(bih)
		bih_triangles_count = BIH_GetTriangleListForBox(bih, sizeof(bih_triangles) / sizeof(*bih_triangles), bih_triangles, bih_surfaces, localmins, localmaxs);
	if(bih_triangles_count == 0)
		return;
	if(bih_triangles_count > (int) (sizeof(bih_triangles) / sizeof(*bih_triangles))) // hit too many, likely bad anyway
		return;
	if(bih_triangles_count > 0)
	{
		for (triangleindex = 0; triangleindex < bih_triangles_count; ++triangleindex)
		{
			surfaceindex = bih_surfaces[triangleindex];
			surface = surfaces + surfaceindex;
			texture = surface->texture;
			if (texture->currentmaterialflags & (MATERIALFLAG_BLENDED | MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_SKY | MATERIALFLAG_SHORTDEPTHRANGE | MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
				continue;
			if (texture->surfaceflags & Q3SURFACEFLAG_NOMARKS)
				continue;
			R_DecalSystem_SplatTriangle(decalsystem, r, g, b, a, s1, t1, s2, t2, decalsequence, dynamic, planes, &projection, bih_triangles[triangleindex], surfaceindex);
		}
	}
	else
	{
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
			for (triangleindex = 0; triangleindex < numtriangles; triangleindex++)
				R_DecalSystem_SplatTriangle(decalsystem, r, g, b, a, s1, t1, s2, t2, decalsequence, dynamic, planes, &projection, triangleindex + surface->num_firsttriangle, surfaceindex);
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
		frametime = (r_refdef.scene.time - decalsystem->lastupdatetime);
	else
		frametime = 0;
	decalsystem->lastupdatetime = r_refdef.scene.time;
	decal = decalsystem->decals;
	numdecals = decalsystem->numdecals;

	for (i = 0, decal = decalsystem->decals;i < numdecals;i++, decal++)
	{
		if (decal->color4f[0][3])
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
	while (numdecals > 0 && !decal[numdecals-1].color4f[0][3])
		numdecals--;

	// collapse the array by shuffling the tail decals into the gaps
	for (;;)
	{
		while (decalsystem->freedecal < numdecals && decal[decalsystem->freedecal].color4f[0][3])
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

	decalsystem->lastupdatetime = r_refdef.scene.time;
	decal = decalsystem->decals;

	faderate = 1.0f / max(0.001f, cl_decals_fadetime.value);

	// update vertex positions for animated models
	v3f = decalsystem->vertex3f;
	c4f = decalsystem->color4f;
	t2f = decalsystem->texcoord2f;
	for (i = 0, decal = decalsystem->decals;i < numdecals;i++, decal++)
	{
		if (!decal->color4f[0][3])
			continue;

		if (surfacevisible && !surfacevisible[decal->surfaceindex])
			continue;

		// skip backfaces
		if (decal->triangleindex < 0 && DotProduct(r_refdef.view.origin, decal->plane) < decal->plane[3])
			continue;

		// update color values for fading decals
		if (decal->lived >= cl_decals_time.value)
			alpha = 1 - faderate * (decal->lived - cl_decals_time.value);
		else
			alpha = 1.0f;

		c4f[ 0] = decal->color4f[0][0] * alpha;
		c4f[ 1] = decal->color4f[0][1] * alpha;
		c4f[ 2] = decal->color4f[0][2] * alpha;
		c4f[ 3] = 1;
		c4f[ 4] = decal->color4f[1][0] * alpha;
		c4f[ 5] = decal->color4f[1][1] * alpha;
		c4f[ 6] = decal->color4f[1][2] * alpha;
		c4f[ 7] = 1;
		c4f[ 8] = decal->color4f[2][0] * alpha;
		c4f[ 9] = decal->color4f[2][1] * alpha;
		c4f[10] = decal->color4f[2][2] * alpha;
		c4f[11] = 1;

		t2f[0] = decal->texcoord2f[0][0];
		t2f[1] = decal->texcoord2f[0][1];
		t2f[2] = decal->texcoord2f[1][0];
		t2f[3] = decal->texcoord2f[1][1];
		t2f[4] = decal->texcoord2f[2][0];
		t2f[5] = decal->texcoord2f[2][1];

		// update vertex positions for animated models
		if (decal->triangleindex >= 0 && decal->triangleindex < rsurface.modelnumtriangles)
		{
			e = rsurface.modelelement3i + 3*decal->triangleindex;
			VectorCopy(rsurface.modelvertex3f + 3*e[0], v3f);
			VectorCopy(rsurface.modelvertex3f + 3*e[1], v3f + 3);
			VectorCopy(rsurface.modelvertex3f + 3*e[2], v3f + 6);
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
		RSurf_ActiveCustomEntity(&rsurface.matrix, &rsurface.inversematrix, rsurface.ent_flags, ent->shadertime, 1, 1, 1, 1, numdecals*3, decalsystem->vertex3f, decalsystem->texcoord2f, NULL, NULL, NULL, decalsystem->color4f, numtris, decalsystem->element3i, decalsystem->element3s, false, false);
//		R_Mesh_ResetTextureState();
		R_Mesh_PrepareVertices_Generic_Arrays(numtris * 3, decalsystem->vertex3f, decalsystem->color4f, decalsystem->texcoord2f);
		GL_DepthMask(false);
		GL_DepthRange(0, 1);
		GL_PolygonOffset(rsurface.basepolygonfactor + r_polygonoffset_decals_factor.value, rsurface.basepolygonoffset + r_polygonoffset_decals_offset.value);
		GL_DepthTest(true);
		GL_CullFace(GL_NONE);
		GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		R_SetupShader_Generic(decalskinframe->base, NULL, GL_MODULATE, 1, false);
		R_Mesh_Draw(0, numtris * 3, 0, numtris, decalsystem->element3i, NULL, 0, decalsystem->element3s, NULL, 0);
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

	if (!sv.active  && !cls.demoplayback && ent != r_refdef.scene.worldentity)
		return;

	if (r_showoverdraw.value > 0)
	{
		float c = r_refdef.view.colorscale * r_showoverdraw.value * 0.125f;
		flagsmask = MATERIALFLAG_SKY | MATERIALFLAG_WALL;
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
		GL_DepthTest(false);
		GL_DepthMask(false);
		GL_DepthRange(0, 1);
		GL_BlendFunc(GL_ONE, GL_ONE);
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (ent == r_refdef.scene.worldentity && !r_refdef.viewcache.world_surfacevisible[j])
				continue;
			rsurface.texture = R_GetCurrentTexture(surface->texture);
			if ((rsurface.texture->currentmaterialflags & flagsmask) && surface->num_triangles)
			{
				RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_NOGAPS, 1, &surface);
				GL_CullFace((rsurface.texture->currentmaterialflags & MATERIALFLAG_NOCULLFACE) ? GL_NONE : r_refdef.view.cullface_back);
				if (!rsurface.texture->currentlayers->depthmask)
					GL_Color(c, 0, 0, 1.0f);
				else if (ent == r_refdef.scene.worldentity)
					GL_Color(c, c, c, 1.0f);
				else
					GL_Color(0, c, 0, 1.0f);
				R_Mesh_PrepareVertices_Generic_Arrays(rsurface.batchnumvertices, rsurface.batchvertex3f, NULL, NULL);
				RSurf_DrawBatch();
			}
		}
		rsurface.texture = NULL;
	}

	flagsmask = MATERIALFLAG_SKY | MATERIALFLAG_WALL;

//	R_Mesh_ResetTextureState();
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, false);
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
					GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
					R_Mesh_PrepareVertices_Generic_Arrays(brush->colbrushf->numpoints, brush->colbrushf->points->v, NULL, NULL);
					R_Mesh_Draw(0, brush->colbrushf->numpoints, 0, brush->colbrushf->numtriangles, brush->colbrushf->elements, NULL, 0, NULL, NULL, 0);
				}
				break;
			case BIH_COLLISIONTRIANGLE:
				triangleindex = bihleaf->itemindex;
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+0], vertex3f[0]);
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+1], vertex3f[1]);
				VectorCopy(model->brush.data_collisionvertex3f + 3*model->brush.data_collisionelement3i[triangleindex*3+2], vertex3f[2]);
				GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_PrepareVertices_Generic_Arrays(3, vertex3f[0], NULL, NULL);
				R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
				break;
			case BIH_RENDERTRIANGLE:
				triangleindex = bihleaf->itemindex;
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+0], vertex3f[0]);
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+1], vertex3f[1]);
				VectorCopy(model->surfmesh.data_vertex3f + 3*model->surfmesh.data_element3i[triangleindex*3+2], vertex3f[2]);
				GL_Color((bihleafindex & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((bihleafindex >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_PrepareVertices_Generic_Arrays(3, vertex3f[0], NULL, NULL);
				R_Mesh_Draw(0, 3, 0, 1, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
				break;
			}
		}
	}

	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);

	if (r_showtris.integer && qglPolygonMode)
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
		qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);CHECKGLERROR
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (ent == r_refdef.scene.worldentity && !r_refdef.viewcache.world_surfacevisible[j])
				continue;
			rsurface.texture = R_GetCurrentTexture(surface->texture);
			if ((rsurface.texture->currentmaterialflags & flagsmask) && surface->num_triangles)
			{
				RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | BATCHNEED_NOGAPS, 1, &surface);
				if (!rsurface.texture->currentlayers->depthmask)
					GL_Color(r_refdef.view.colorscale, 0, 0, r_showtris.value);
				else if (ent == r_refdef.scene.worldentity)
					GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, r_showtris.value);
				else
					GL_Color(0, r_refdef.view.colorscale, 0, r_showtris.value);
				R_Mesh_PrepareVertices_Generic_Arrays(rsurface.batchnumvertices, rsurface.batchvertex3f, NULL, NULL);
				RSurf_DrawBatch();
			}
		}
		qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);CHECKGLERROR
		rsurface.texture = NULL;
	}

	if (r_shownormals.value != 0 && qglBegin)
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
				RSurf_PrepareVerticesForBatch(BATCHNEED_ARRAY_VERTEX | BATCHNEED_ARRAY_NORMAL | BATCHNEED_ARRAY_VECTOR | BATCHNEED_NOGAPS, 1, &surface);
				qglBegin(GL_LINES);
				if (r_shownormals.value < 0 && rsurface.batchnormal3f)
				{
					for (k = 0, l = rsurface.batchfirstvertex;k < rsurface.batchnumvertices;k++, l++)
					{
						VectorCopy(rsurface.batchvertex3f + l * 3, v);
						GL_Color(0, 0, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, -r_shownormals.value, rsurface.batchnormal3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
				}
				if (r_shownormals.value > 0 && rsurface.batchsvector3f)
				{
					for (k = 0, l = rsurface.batchfirstvertex;k < rsurface.batchnumvertices;k++, l++)
					{
						VectorCopy(rsurface.batchvertex3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, 0, 0, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.batchsvector3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
				}
				if (r_shownormals.value > 0 && rsurface.batchtvector3f)
				{
					for (k = 0, l = rsurface.batchfirstvertex;k < rsurface.batchnumvertices;k++, l++)
					{
						VectorCopy(rsurface.batchvertex3f + l * 3, v);
						GL_Color(0, r_refdef.view.colorscale, 0, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.batchtvector3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
				}
				if (r_shownormals.value > 0 && rsurface.batchnormal3f)
				{
					for (k = 0, l = rsurface.batchfirstvertex;k < rsurface.batchnumvertices;k++, l++)
					{
						VectorCopy(rsurface.batchvertex3f + l * 3, v);
						GL_Color(0, 0, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, r_shownormals.value, rsurface.batchnormal3f + l * 3, v);
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, 1);
						qglVertex3f(v[0], v[1], v[2]);
					}
				}
				qglEnd();
				CHECKGLERROR
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

	rsurface.lightmaptexture = NULL;
	rsurface.deluxemaptexture = NULL;
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
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
		case RENDERPATH_GLES2:
			RSurf_ActiveModelEntity(ent, model->wantnormals, model->wanttangents, false);
			break;
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
			RSurf_ActiveModelEntity(ent, model->wantnormals, false, false);
			break;
		}
	}
	else
	{
		switch (vid.renderpath)
		{
		case RENDERPATH_GL20:
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
		case RENDERPATH_SOFT:
		case RENDERPATH_GLES2:
			RSurf_ActiveModelEntity(ent, true, true, false);
			break;
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GLES1:
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

	rsurface.lightmaptexture = NULL;
	rsurface.deluxemaptexture = NULL;
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
	texture.offsetmapping = OFFSETMAPPING_OFF;
	texture.offsetscale = 1;
	texture.specularscalemod = 1;
	texture.specularpowermod = 1;

	surface.texture = &texture;
	surface.num_triangles = numtriangles;
	surface.num_firsttriangle = firsttriangle;
	surface.num_vertices = numvertices;
	surface.num_firstvertex = firstvertex;

	// now render it
	rsurface.texture = R_GetCurrentTexture(surface.texture);
	rsurface.lightmaptexture = NULL;
	rsurface.deluxemaptexture = NULL;
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
	rsurface.lightmaptexture = NULL;
	rsurface.deluxemaptexture = NULL;
	rsurface.uselightmaptexture = false;
	R_DrawModelTextureSurfaceList(1, &surfacelist, writedepth, prepass);
}
