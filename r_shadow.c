
/*
Terminology: Stencil Shadow Volume (sometimes called Stencil Shadows)
An extrusion of the lit faces, beginning at the original geometry and ending
further from the light source than the original geometry (presumably at least
as far as the light's radius, if the light has a radius at all), capped at
both front and back to avoid any problems (extrusion from dark faces also
works but has a different set of problems)

This is normally rendered using Carmack's Reverse technique, in which
backfaces behind zbuffer (zfail) increment the stencil, and frontfaces behind
zbuffer (zfail) decrement the stencil, the result is a stencil value of zero
where shadows did not intersect the visible geometry, suitable as a stencil
mask for rendering lighting everywhere but shadow.

In our case to hopefully avoid the Creative Labs patent, we draw the backfaces
as decrement and the frontfaces as increment, and we redefine the DepthFunc to
GL_LESS (the patent uses GL_GEQUAL) which causes zfail when behind surfaces
and zpass when infront (the patent draws where zpass with a GL_GEQUAL test),
additionally we clear stencil to 128 to avoid the need for the unclamped
incr/decr extension (not related to patent).

Patent warning:
This algorithm may be covered by Creative's patent (US Patent #6384822),
however that patent is quite specific about increment on backfaces and
decrement on frontfaces where zpass with GL_GEQUAL depth test, which is
opposite this implementation and partially opposite Carmack's Reverse paper
(which uses GL_LESS, but increments on backfaces and decrements on frontfaces).



Terminology: Stencil Light Volume (sometimes called Light Volumes)
Similar to a Stencil Shadow Volume, but inverted; rather than containing the
areas in shadow it contains the areas in light, this can only be built
quickly for certain limited cases (such as portal visibility from a point),
but is quite useful for some effects (sunlight coming from sky polygons is
one possible example, translucent occluders is another example).



Terminology: Optimized Stencil Shadow Volume
A Stencil Shadow Volume that has been processed sufficiently to ensure it has
no duplicate coverage of areas (no need to shadow an area twice), often this
greatly improves performance but is an operation too costly to use on moving
lights (however completely optimal Stencil Light Volumes can be constructed
in some ideal cases).



Terminology: Per Pixel Lighting (sometimes abbreviated PPL)
Per pixel evaluation of lighting equations, at a bare minimum this involves
DOT3 shading of diffuse lighting (per pixel dotproduct of negated incidence
vector and surface normal, using a texture of the surface bumps, called a
NormalMap) if supported by hardware; in our case there is support for cards
which are incapable of DOT3, the quality is quite poor however.  Additionally
it is desirable to have specular evaluation per pixel, per vertex
normalization of specular halfangle vectors causes noticable distortion but
is unavoidable on hardware without GL_ARB_fragment_program or
GL_ARB_fragment_shader.



Terminology: Normalization CubeMap
A cubemap containing normalized dot3-encoded (vectors of length 1 or less
encoded as RGB colors) for any possible direction, this technique allows per
pixel calculation of incidence vector for per pixel lighting purposes, which
would not otherwise be possible per pixel without GL_ARB_fragment_program or
GL_ARB_fragment_shader.



Terminology: 2D+1D Attenuation Texturing
A very crude approximation of light attenuation with distance which results
in cylindrical light shapes which fade vertically as a streak (some games
such as Doom3 allow this to be rotated to be less noticable in specific
cases), the technique is simply modulating lighting by two 2D textures (which
can be the same) on different axes of projection (XY and Z, typically), this
is the second best technique available without 3D Attenuation Texturing,
GL_ARB_fragment_program or GL_ARB_fragment_shader technology.



Terminology: 2D+1D Inverse Attenuation Texturing
A clever method described in papers on the Abducted engine, this has a squared
distance texture (bright on the outside, black in the middle), which is used
twice using GL_ADD blending, the result of this is used in an inverse modulate
(GL_ONE_MINUS_DST_ALPHA, GL_ZERO) to implement the equation
lighting*=(1-((X*X+Y*Y)+(Z*Z))) which is spherical (unlike 2D+1D attenuation
texturing).



Terminology: 3D Attenuation Texturing
A slightly crude approximation of light attenuation with distance, its flaws
are limited radius and resolution (performance tradeoffs).



Terminology: 3D Attenuation-Normalization Texturing
A 3D Attenuation Texture merged with a Normalization CubeMap, by making the
vectors shorter the lighting becomes darker, a very effective optimization of
diffuse lighting if 3D Attenuation Textures are already used.



Terminology: Light Cubemap Filtering
A technique for modeling non-uniform light distribution according to
direction, for example a lantern may use a cubemap to describe the light
emission pattern of the cage around the lantern (as well as soot buildup
discoloring the light in certain areas), often also used for softened grate
shadows and light shining through a stained glass window (done crudely by
texturing the lighting with a cubemap), another good example would be a disco
light.  This technique is used heavily in many games (Doom3 does not support
this however).



Terminology: Light Projection Filtering
A technique for modeling shadowing of light passing through translucent
surfaces, allowing stained glass windows and other effects to be done more
elegantly than possible with Light Cubemap Filtering by applying an occluder
texture to the lighting combined with a stencil light volume to limit the lit
area, this technique is used by Doom3 for spotlights and flashlights, among
other things, this can also be used more generally to render light passing
through multiple translucent occluders in a scene (using a light volume to
describe the area beyond the occluder, and thus mask off rendering of all
other areas).



Terminology: Doom3 Lighting
A combination of Stencil Shadow Volume, Per Pixel Lighting, Normalization
CubeMap, 2D+1D Attenuation Texturing, and Light Projection Filtering, as
demonstrated by the game Doom3.
*/

#include "quakedef.h"
#include "r_shadow.h"
#include "cl_collision.h"
#include "portals.h"
#include "image.h"

extern void R_Shadow_EditLights_Init(void);

typedef enum r_shadow_rendermode_e
{
	R_SHADOW_RENDERMODE_NONE,
	R_SHADOW_RENDERMODE_ZPASS_STENCIL,
	R_SHADOW_RENDERMODE_ZPASS_SEPARATESTENCIL,
	R_SHADOW_RENDERMODE_ZPASS_STENCILTWOSIDE,
	R_SHADOW_RENDERMODE_ZFAIL_STENCIL,
	R_SHADOW_RENDERMODE_ZFAIL_SEPARATESTENCIL,
	R_SHADOW_RENDERMODE_ZFAIL_STENCILTWOSIDE,
	R_SHADOW_RENDERMODE_LIGHT_VERTEX,
	R_SHADOW_RENDERMODE_LIGHT_VERTEX2DATTEN,
	R_SHADOW_RENDERMODE_LIGHT_VERTEX2D1DATTEN,
	R_SHADOW_RENDERMODE_LIGHT_VERTEX3DATTEN,
	R_SHADOW_RENDERMODE_LIGHT_GLSL,
	R_SHADOW_RENDERMODE_VISIBLEVOLUMES,
	R_SHADOW_RENDERMODE_VISIBLELIGHTING,
	R_SHADOW_RENDERMODE_SHADOWMAP2D,
	R_SHADOW_RENDERMODE_SHADOWMAPRECTANGLE,
	R_SHADOW_RENDERMODE_SHADOWMAPCUBESIDE,
}
r_shadow_rendermode_t;

typedef enum r_shadow_shadowmode_e
{
    R_SHADOW_SHADOWMODE_STENCIL,
    R_SHADOW_SHADOWMODE_SHADOWMAP2D,
    R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE,
    R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE
}
r_shadow_shadowmode_t;

r_shadow_rendermode_t r_shadow_rendermode = R_SHADOW_RENDERMODE_NONE;
r_shadow_rendermode_t r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_NONE;
r_shadow_rendermode_t r_shadow_shadowingrendermode_zpass = R_SHADOW_RENDERMODE_NONE;
r_shadow_rendermode_t r_shadow_shadowingrendermode_zfail = R_SHADOW_RENDERMODE_NONE;
qboolean r_shadow_usingshadowmaprect;
qboolean r_shadow_usingshadowmap2d;
qboolean r_shadow_usingshadowmapcube;
int r_shadow_shadowmapside;
float r_shadow_shadowmap_texturescale[2];
float r_shadow_shadowmap_parameters[4];
#if 0
int r_shadow_drawbuffer;
int r_shadow_readbuffer;
#endif
int r_shadow_cullface_front, r_shadow_cullface_back;
GLuint r_shadow_fborectangle;
GLuint r_shadow_fbocubeside[R_SHADOW_SHADOWMAP_NUMCUBEMAPS];
GLuint r_shadow_fbo2d;
r_shadow_shadowmode_t r_shadow_shadowmode;
int r_shadow_shadowmapfilterquality;
int r_shadow_shadowmaptexturetype;
int r_shadow_shadowmapdepthbits;
int r_shadow_shadowmapmaxsize;
qboolean r_shadow_shadowmapvsdct;
qboolean r_shadow_shadowmapsampler;
int r_shadow_shadowmappcf;
int r_shadow_shadowmapborder;
int r_shadow_lightscissor[4];
qboolean r_shadow_usingdeferredprepass;

int maxshadowtriangles;
int *shadowelements;

int maxshadowvertices;
float *shadowvertex3f;

int maxshadowmark;
int numshadowmark;
int *shadowmark;
int *shadowmarklist;
int shadowmarkcount;

int maxshadowsides;
int numshadowsides;
unsigned char *shadowsides;
int *shadowsideslist;

int maxvertexupdate;
int *vertexupdate;
int *vertexremap;
int vertexupdatenum;

int r_shadow_buffer_numleafpvsbytes;
unsigned char *r_shadow_buffer_visitingleafpvs;
unsigned char *r_shadow_buffer_leafpvs;
int *r_shadow_buffer_leaflist;

int r_shadow_buffer_numsurfacepvsbytes;
unsigned char *r_shadow_buffer_surfacepvs;
int *r_shadow_buffer_surfacelist;
unsigned char *r_shadow_buffer_surfacesides;

int r_shadow_buffer_numshadowtrispvsbytes;
unsigned char *r_shadow_buffer_shadowtrispvs;
int r_shadow_buffer_numlighttrispvsbytes;
unsigned char *r_shadow_buffer_lighttrispvs;

rtexturepool_t *r_shadow_texturepool;
rtexture_t *r_shadow_attenuationgradienttexture;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_attenuation3dtexture;
skinframe_t *r_shadow_lightcorona;
rtexture_t *r_shadow_shadowmaprectangletexture;
rtexture_t *r_shadow_shadowmap2dtexture;
rtexture_t *r_shadow_shadowmapcubetexture[R_SHADOW_SHADOWMAP_NUMCUBEMAPS];
rtexture_t *r_shadow_shadowmapvsdcttexture;
int r_shadow_shadowmapsize; // changes for each light based on distance
int r_shadow_shadowmaplod; // changes for each light based on distance

GLuint r_shadow_prepassgeometryfbo;
GLuint r_shadow_prepasslightingfbo;
int r_shadow_prepass_width;
int r_shadow_prepass_height;
rtexture_t *r_shadow_prepassgeometrydepthtexture;
rtexture_t *r_shadow_prepassgeometrynormalmaptexture;
rtexture_t *r_shadow_prepasslightingdiffusetexture;
rtexture_t *r_shadow_prepasslightingspeculartexture;

// lights are reloaded when this changes
char r_shadow_mapname[MAX_QPATH];

// used only for light filters (cubemaps)
rtexturepool_t *r_shadow_filters_texturepool;

static const GLenum r_shadow_prepasslightingdrawbuffers[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};

cvar_t r_shadow_bumpscale_basetexture = {0, "r_shadow_bumpscale_basetexture", "0", "generate fake bumpmaps from diffuse textures at this bumpyness, try 4 to match tenebrae, higher values increase depth, requires r_restart to take effect"};
cvar_t r_shadow_bumpscale_bumpmap = {0, "r_shadow_bumpscale_bumpmap", "4", "what magnitude to interpret _bump.tga textures as, higher values increase depth, requires r_restart to take effect"};
cvar_t r_shadow_debuglight = {0, "r_shadow_debuglight", "-1", "renders only one light, for level design purposes or debugging"};
cvar_t r_shadow_deferred = {CVAR_SAVE, "r_shadow_deferred", "0", "uses image-based lighting instead of geometry-based lighting, the method used renders a depth image and a normalmap image, renders lights into separate diffuse and specular images, and then combines this into the normal rendering, requires r_shadow_shadowmapping"};
cvar_t r_shadow_deferred_8bitrange = {CVAR_SAVE, "r_shadow_deferred_8bitrange", "2", "dynamic range of image-based lighting when using 32bit color (does not apply to fp)"};
//cvar_t r_shadow_deferred_fp = {CVAR_SAVE, "r_shadow_deferred_fp", "0", "use 16bit (1) or 32bit (2) floating point for accumulation of image-based lighting"};
cvar_t r_shadow_usenormalmap = {CVAR_SAVE, "r_shadow_usenormalmap", "1", "enables use of directional shading on lights"};
cvar_t r_shadow_gloss = {CVAR_SAVE, "r_shadow_gloss", "1", "0 disables gloss (specularity) rendering, 1 uses gloss if textures are found, 2 forces a flat metallic specular effect on everything without textures (similar to tenebrae)"};
cvar_t r_shadow_gloss2intensity = {0, "r_shadow_gloss2intensity", "0.125", "how bright the forced flat gloss should look if r_shadow_gloss is 2"};
cvar_t r_shadow_glossintensity = {0, "r_shadow_glossintensity", "1", "how bright textured glossmaps should look if r_shadow_gloss is 1 or 2"};
cvar_t r_shadow_glossexponent = {0, "r_shadow_glossexponent", "32", "how 'sharp' the gloss should appear (specular power)"};
cvar_t r_shadow_gloss2exponent = {0, "r_shadow_gloss2exponent", "32", "same as r_shadow_glossexponent but for forced gloss (gloss 2) surfaces"};
cvar_t r_shadow_glossexact = {0, "r_shadow_glossexact", "0", "use exact reflection math for gloss (slightly slower, but should look a tad better)"};
cvar_t r_shadow_lightattenuationdividebias = {0, "r_shadow_lightattenuationdividebias", "1", "changes attenuation texture generation"};
cvar_t r_shadow_lightattenuationlinearscale = {0, "r_shadow_lightattenuationlinearscale", "2", "changes attenuation texture generation"};
cvar_t r_shadow_lightintensityscale = {0, "r_shadow_lightintensityscale", "1", "renders all world lights brighter or darker"};
cvar_t r_shadow_lightradiusscale = {0, "r_shadow_lightradiusscale", "1", "renders all world lights larger or smaller"};
cvar_t r_shadow_projectdistance = {0, "r_shadow_projectdistance", "0", "how far to cast shadows"};
cvar_t r_shadow_frontsidecasting = {0, "r_shadow_frontsidecasting", "1", "whether to cast shadows from illuminated triangles (front side of model) or unlit triangles (back side of model)"};
cvar_t r_shadow_realtime_dlight = {CVAR_SAVE, "r_shadow_realtime_dlight", "1", "enables rendering of dynamic lights such as explosions and rocket light"};
cvar_t r_shadow_realtime_dlight_shadows = {CVAR_SAVE, "r_shadow_realtime_dlight_shadows", "1", "enables rendering of shadows from dynamic lights"};
cvar_t r_shadow_realtime_dlight_svbspculling = {0, "r_shadow_realtime_dlight_svbspculling", "0", "enables svbsp optimization on dynamic lights (very slow!)"};
cvar_t r_shadow_realtime_dlight_portalculling = {0, "r_shadow_realtime_dlight_portalculling", "0", "enables portal optimization on dynamic lights (slow!)"};
cvar_t r_shadow_realtime_world = {CVAR_SAVE, "r_shadow_realtime_world", "0", "enables rendering of full world lighting (whether loaded from the map, or a .rtlights file, or a .ent file, or a .lights file produced by hlight)"};
cvar_t r_shadow_realtime_world_lightmaps = {CVAR_SAVE, "r_shadow_realtime_world_lightmaps", "0", "brightness to render lightmaps when using full world lighting, try 0.5 for a tenebrae-like appearance"};
cvar_t r_shadow_realtime_world_shadows = {CVAR_SAVE, "r_shadow_realtime_world_shadows", "1", "enables rendering of shadows from world lights"};
cvar_t r_shadow_realtime_world_compile = {0, "r_shadow_realtime_world_compile", "1", "enables compilation of world lights for higher performance rendering"};
cvar_t r_shadow_realtime_world_compileshadow = {0, "r_shadow_realtime_world_compileshadow", "1", "enables compilation of shadows from world lights for higher performance rendering"};
cvar_t r_shadow_realtime_world_compilesvbsp = {0, "r_shadow_realtime_world_compilesvbsp", "1", "enables svbsp optimization during compilation (slower than compileportalculling but more exact)"};
cvar_t r_shadow_realtime_world_compileportalculling = {0, "r_shadow_realtime_world_compileportalculling", "0", "enables portal-based culling optimization during compilation (overrides compilesvbsp)"};
cvar_t r_shadow_scissor = {0, "r_shadow_scissor", "1", "use scissor optimization of light rendering (restricts rendering to the portion of the screen affected by the light)"};
cvar_t r_shadow_shadowmapping = {CVAR_SAVE, "r_shadow_shadowmapping", "0", "enables use of shadowmapping (depth texture sampling) instead of stencil shadow volumes, requires gl_fbo 1"};
cvar_t r_shadow_shadowmapping_texturetype = {CVAR_SAVE, "r_shadow_shadowmapping_texturetype", "-1", "shadowmap texture types: -1 = auto-select, 0 = 2D, 1 = rectangle, 2 = cubemap"};
cvar_t r_shadow_shadowmapping_filterquality = {CVAR_SAVE, "r_shadow_shadowmapping_filterquality", "-1", "shadowmap filter modes: -1 = auto-select, 0 = no filtering, 1 = bilinear, 2 = bilinear 2x2 blur (fast), 3 = 3x3 blur (moderate), 4 = 4x4 blur (slow)"};
cvar_t r_shadow_shadowmapping_depthbits = {CVAR_SAVE, "r_shadow_shadowmapping_depthbits", "24", "requested minimum shadowmap texture depth bits"};
cvar_t r_shadow_shadowmapping_vsdct = {CVAR_SAVE, "r_shadow_shadowmapping_vsdct", "1", "enables use of virtual shadow depth cube texture"};
cvar_t r_shadow_shadowmapping_minsize = {CVAR_SAVE, "r_shadow_shadowmapping_minsize", "32", "shadowmap size limit"};
cvar_t r_shadow_shadowmapping_maxsize = {CVAR_SAVE, "r_shadow_shadowmapping_maxsize", "512", "shadowmap size limit"};
cvar_t r_shadow_shadowmapping_precision = {CVAR_SAVE, "r_shadow_shadowmapping_precision", "1", "makes shadowmaps have a maximum resolution of this number of pixels per light source radius unit such that, for example, at precision 0.5 a light with radius 200 will have a maximum resolution of 100 pixels"};
//cvar_t r_shadow_shadowmapping_lod_bias = {CVAR_SAVE, "r_shadow_shadowmapping_lod_bias", "16", "shadowmap size bias"};
//cvar_t r_shadow_shadowmapping_lod_scale = {CVAR_SAVE, "r_shadow_shadowmapping_lod_scale", "128", "shadowmap size scaling parameter"};
cvar_t r_shadow_shadowmapping_bordersize = {CVAR_SAVE, "r_shadow_shadowmapping_bordersize", "4", "shadowmap size bias for filtering"};
cvar_t r_shadow_shadowmapping_nearclip = {CVAR_SAVE, "r_shadow_shadowmapping_nearclip", "1", "shadowmap nearclip in world units"};
cvar_t r_shadow_shadowmapping_bias = {CVAR_SAVE, "r_shadow_shadowmapping_bias", "0.03", "shadowmap bias parameter (this is multiplied by nearclip * 1024 / lodsize)"};
cvar_t r_shadow_shadowmapping_polygonfactor = {CVAR_SAVE, "r_shadow_shadowmapping_polygonfactor", "2", "slope-dependent shadowmapping bias"};
cvar_t r_shadow_shadowmapping_polygonoffset = {CVAR_SAVE, "r_shadow_shadowmapping_polygonoffset", "0", "constant shadowmapping bias"};
cvar_t r_shadow_polygonfactor = {0, "r_shadow_polygonfactor", "0", "how much to enlarge shadow volume polygons when rendering (should be 0!)"};
cvar_t r_shadow_polygonoffset = {0, "r_shadow_polygonoffset", "1", "how much to push shadow volumes into the distance when rendering, to reduce chances of zfighting artifacts (should not be less than 0)"};
cvar_t r_shadow_texture3d = {0, "r_shadow_texture3d", "1", "use 3D voxel textures for spherical attenuation rather than cylindrical (does not affect OpenGL 2.0 render path)"};
cvar_t r_coronas = {CVAR_SAVE, "r_coronas", "1", "brightness of corona flare effects around certain lights, 0 disables corona effects"};
cvar_t r_coronas_occlusionsizescale = {CVAR_SAVE, "r_coronas_occlusionsizescale", "0.1", "size of light source for corona occlusion checksm the proportion of hidden pixels controls corona intensity"};
cvar_t r_coronas_occlusionquery = {CVAR_SAVE, "r_coronas_occlusionquery", "1", "use GL_ARB_occlusion_query extension if supported (fades coronas according to visibility)"};
cvar_t gl_flashblend = {CVAR_SAVE, "gl_flashblend", "0", "render bright coronas for dynamic lights instead of actual lighting, fast but ugly"};
cvar_t gl_ext_separatestencil = {0, "gl_ext_separatestencil", "1", "make use of OpenGL 2.0 glStencilOpSeparate or GL_ATI_separate_stencil extension"};
cvar_t gl_ext_stenciltwoside = {0, "gl_ext_stenciltwoside", "1", "make use of GL_EXT_stenciltwoside extension (NVIDIA only)"};
cvar_t r_editlights = {0, "r_editlights", "0", "enables .rtlights file editing mode"};
cvar_t r_editlights_cursordistance = {0, "r_editlights_cursordistance", "1024", "maximum distance of cursor from eye"};
cvar_t r_editlights_cursorpushback = {0, "r_editlights_cursorpushback", "0", "how far to pull the cursor back toward the eye"};
cvar_t r_editlights_cursorpushoff = {0, "r_editlights_cursorpushoff", "4", "how far to push the cursor off the impacted surface"};
cvar_t r_editlights_cursorgrid = {0, "r_editlights_cursorgrid", "4", "snaps cursor to this grid size"};
cvar_t r_editlights_quakelightsizescale = {CVAR_SAVE, "r_editlights_quakelightsizescale", "1", "changes size of light entities loaded from a map"};

// note the table actually includes one more value, just to avoid the need to clamp the distance index due to minor math error
#define ATTENTABLESIZE 256
// 1D gradient, 2D circle and 3D sphere attenuation textures
#define ATTEN1DSIZE 32
#define ATTEN2DSIZE 64
#define ATTEN3DSIZE 32

static float r_shadow_attendividebias; // r_shadow_lightattenuationdividebias
static float r_shadow_attenlinearscale; // r_shadow_lightattenuationlinearscale
static float r_shadow_attentable[ATTENTABLESIZE+1];

rtlight_t *r_shadow_compilingrtlight;
static memexpandablearray_t r_shadow_worldlightsarray;
dlight_t *r_shadow_selectedlight;
dlight_t r_shadow_bufferlight;
vec3_t r_editlights_cursorlocation;

extern int con_vislines;

typedef struct cubemapinfo_s
{
	char basename[64];
	rtexture_t *texture;
}
cubemapinfo_t;

static int numcubemaps;
static cubemapinfo_t cubemaps[MAX_CUBEMAPS];

void R_Shadow_UncompileWorldLights(void);
void R_Shadow_ClearWorldLights(void);
void R_Shadow_SaveWorldLights(void);
void R_Shadow_LoadWorldLights(void);
void R_Shadow_LoadLightsFile(void);
void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void);
void R_Shadow_EditLights_Reload_f(void);
void R_Shadow_ValidateCvars(void);
static void R_Shadow_MakeTextures(void);

#define EDLIGHTSPRSIZE			8
skinframe_t *r_editlights_sprcursor;
skinframe_t *r_editlights_sprlight;
skinframe_t *r_editlights_sprnoshadowlight;
skinframe_t *r_editlights_sprcubemaplight;
skinframe_t *r_editlights_sprcubemapnoshadowlight;
skinframe_t *r_editlights_sprselection;

void R_Shadow_SetShadowMode(void)
{
	r_shadow_shadowmapmaxsize = bound(1, r_shadow_shadowmapping_maxsize.integer, (int)vid.maxtexturesize_2d / 4);
	r_shadow_shadowmapvsdct = r_shadow_shadowmapping_vsdct.integer != 0;
	r_shadow_shadowmapfilterquality = r_shadow_shadowmapping_filterquality.integer;
	r_shadow_shadowmaptexturetype = r_shadow_shadowmapping_texturetype.integer;
	r_shadow_shadowmapdepthbits = r_shadow_shadowmapping_depthbits.integer;
	r_shadow_shadowmapborder = bound(0, r_shadow_shadowmapping_bordersize.integer, 16);
	r_shadow_shadowmaplod = -1;
	r_shadow_shadowmapsize = 0;
	r_shadow_shadowmapsampler = false;
	r_shadow_shadowmappcf = 0;
	r_shadow_shadowmode = R_SHADOW_SHADOWMODE_STENCIL;
	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		if ((r_shadow_shadowmapping.integer || r_shadow_deferred.integer) && vid.support.ext_framebuffer_object)
		{
			if(r_shadow_shadowmapfilterquality < 0)
			{
				if(vid.support.amd_texture_texture4 || vid.support.arb_texture_gather)
					r_shadow_shadowmappcf = 1;
				else if(strstr(gl_vendor, "NVIDIA") || strstr(gl_renderer, "Radeon HD")) 
				{
					r_shadow_shadowmapsampler = vid.support.arb_shadow;
					r_shadow_shadowmappcf = 1;
				}
				else if(strstr(gl_vendor, "ATI")) 
					r_shadow_shadowmappcf = 1;
				else 
					r_shadow_shadowmapsampler = vid.support.arb_shadow;
			}
			else 
			{
				switch (r_shadow_shadowmapfilterquality)
				{
				case 1:
					r_shadow_shadowmapsampler = vid.support.arb_shadow;
					break;
				case 2:
					r_shadow_shadowmapsampler = vid.support.arb_shadow;
					r_shadow_shadowmappcf = 1;
					break;
				case 3:
					r_shadow_shadowmappcf = 1;
					break;
				case 4:
					r_shadow_shadowmappcf = 2;
					break;
				}
			}
			switch (r_shadow_shadowmaptexturetype)
			{
			case 0:
				r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAP2D;
				break;
			case 1:
				r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE;
				break;
			case 2:
				r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE;
				break;
			default:
				if((vid.support.amd_texture_texture4 || vid.support.arb_texture_gather) && r_shadow_shadowmappcf && !r_shadow_shadowmapsampler)
					r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAP2D;
				else if(vid.support.arb_texture_rectangle) 
					r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE;
				else
					r_shadow_shadowmode = R_SHADOW_SHADOWMODE_SHADOWMAP2D;
				break;
			}
		}
		break;
	case RENDERPATH_GL13:
		break;
	case RENDERPATH_GL11:
		break;
	}
}

void R_Shadow_FreeShadowMaps(void)
{
	int i;

	R_Shadow_SetShadowMode();

	if (!vid.support.ext_framebuffer_object || !vid.support.arb_fragment_shader)
		return;

	CHECKGLERROR

	if (r_shadow_fborectangle)
		qglDeleteFramebuffersEXT(1, &r_shadow_fborectangle);CHECKGLERROR
	r_shadow_fborectangle = 0;

	if (r_shadow_fbo2d)
		qglDeleteFramebuffersEXT(1, &r_shadow_fbo2d);CHECKGLERROR
	r_shadow_fbo2d = 0;
	for (i = 0;i < R_SHADOW_SHADOWMAP_NUMCUBEMAPS;i++)
		if (r_shadow_fbocubeside[i])
			qglDeleteFramebuffersEXT(1, &r_shadow_fbocubeside[i]);CHECKGLERROR
	memset(r_shadow_fbocubeside, 0, sizeof(r_shadow_fbocubeside));

	if (r_shadow_shadowmaprectangletexture)
		R_FreeTexture(r_shadow_shadowmaprectangletexture);
	r_shadow_shadowmaprectangletexture = NULL;

	if (r_shadow_shadowmap2dtexture)
		R_FreeTexture(r_shadow_shadowmap2dtexture);
	r_shadow_shadowmap2dtexture = NULL;

	for (i = 0;i < R_SHADOW_SHADOWMAP_NUMCUBEMAPS;i++)
		if (r_shadow_shadowmapcubetexture[i])
			R_FreeTexture(r_shadow_shadowmapcubetexture[i]);
	memset(r_shadow_shadowmapcubetexture, 0, sizeof(r_shadow_shadowmapcubetexture));

	if (r_shadow_shadowmapvsdcttexture)
		R_FreeTexture(r_shadow_shadowmapvsdcttexture);
	r_shadow_shadowmapvsdcttexture = NULL;

	CHECKGLERROR
}

void r_shadow_start(void)
{
	// allocate vertex processing arrays
	numcubemaps = 0;
	r_shadow_attenuationgradienttexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_shadowmode = R_SHADOW_SHADOWMODE_STENCIL;
	r_shadow_shadowmaprectangletexture = NULL;
	r_shadow_shadowmap2dtexture = NULL;
	memset(r_shadow_shadowmapcubetexture, 0, sizeof(r_shadow_shadowmapcubetexture));
	r_shadow_shadowmapvsdcttexture = NULL;
	r_shadow_shadowmapmaxsize = 0;
	r_shadow_shadowmapsize = 0;
	r_shadow_shadowmaplod = 0;
	r_shadow_shadowmapfilterquality = -1;
	r_shadow_shadowmaptexturetype = -1;
	r_shadow_shadowmapdepthbits = 0;
	r_shadow_shadowmapvsdct = false;
	r_shadow_shadowmapsampler = false;
	r_shadow_shadowmappcf = 0;
	r_shadow_fborectangle = 0;
	r_shadow_fbo2d = 0;
	memset(r_shadow_fbocubeside, 0, sizeof(r_shadow_fbocubeside));

	R_Shadow_FreeShadowMaps();

	r_shadow_texturepool = NULL;
	r_shadow_filters_texturepool = NULL;
	R_Shadow_ValidateCvars();
	R_Shadow_MakeTextures();
	maxshadowtriangles = 0;
	shadowelements = NULL;
	maxshadowvertices = 0;
	shadowvertex3f = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxshadowmark = 0;
	numshadowmark = 0;
	shadowmark = NULL;
	shadowmarklist = NULL;
	shadowmarkcount = 0;
	maxshadowsides = 0;
	numshadowsides = 0;
	shadowsides = NULL;
	shadowsideslist = NULL;
	r_shadow_buffer_numleafpvsbytes = 0;
	r_shadow_buffer_visitingleafpvs = NULL;
	r_shadow_buffer_leafpvs = NULL;
	r_shadow_buffer_leaflist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	r_shadow_buffer_surfacepvs = NULL;
	r_shadow_buffer_surfacelist = NULL;
	r_shadow_buffer_surfacesides = NULL;
	r_shadow_buffer_numshadowtrispvsbytes = 0;
	r_shadow_buffer_shadowtrispvs = NULL;
	r_shadow_buffer_numlighttrispvsbytes = 0;
	r_shadow_buffer_lighttrispvs = NULL;

	r_shadow_usingdeferredprepass = false;
	r_shadow_prepass_width = r_shadow_prepass_height = 0;
}

static void R_Shadow_FreeDeferred(void);
void r_shadow_shutdown(void)
{
	CHECKGLERROR
	R_Shadow_UncompileWorldLights();

	R_Shadow_FreeShadowMaps();

	r_shadow_usingdeferredprepass = false;
	if (r_shadow_prepass_width)
		R_Shadow_FreeDeferred();
	r_shadow_prepass_width = r_shadow_prepass_height = 0;

	CHECKGLERROR
	numcubemaps = 0;
	r_shadow_attenuationgradienttexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	R_FreeTexturePool(&r_shadow_texturepool);
	R_FreeTexturePool(&r_shadow_filters_texturepool);
	maxshadowtriangles = 0;
	if (shadowelements)
		Mem_Free(shadowelements);
	shadowelements = NULL;
	if (shadowvertex3f)
		Mem_Free(shadowvertex3f);
	shadowvertex3f = NULL;
	maxvertexupdate = 0;
	if (vertexupdate)
		Mem_Free(vertexupdate);
	vertexupdate = NULL;
	if (vertexremap)
		Mem_Free(vertexremap);
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxshadowmark = 0;
	numshadowmark = 0;
	if (shadowmark)
		Mem_Free(shadowmark);
	shadowmark = NULL;
	if (shadowmarklist)
		Mem_Free(shadowmarklist);
	shadowmarklist = NULL;
	shadowmarkcount = 0;
	maxshadowsides = 0;
	numshadowsides = 0;
	if (shadowsides)
		Mem_Free(shadowsides);
	shadowsides = NULL;
	if (shadowsideslist)
		Mem_Free(shadowsideslist);
	shadowsideslist = NULL;
	r_shadow_buffer_numleafpvsbytes = 0;
	if (r_shadow_buffer_visitingleafpvs)
		Mem_Free(r_shadow_buffer_visitingleafpvs);
	r_shadow_buffer_visitingleafpvs = NULL;
	if (r_shadow_buffer_leafpvs)
		Mem_Free(r_shadow_buffer_leafpvs);
	r_shadow_buffer_leafpvs = NULL;
	if (r_shadow_buffer_leaflist)
		Mem_Free(r_shadow_buffer_leaflist);
	r_shadow_buffer_leaflist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	if (r_shadow_buffer_surfacepvs)
		Mem_Free(r_shadow_buffer_surfacepvs);
	r_shadow_buffer_surfacepvs = NULL;
	if (r_shadow_buffer_surfacelist)
		Mem_Free(r_shadow_buffer_surfacelist);
	r_shadow_buffer_surfacelist = NULL;
	if (r_shadow_buffer_surfacesides)
		Mem_Free(r_shadow_buffer_surfacesides);
	r_shadow_buffer_surfacesides = NULL;
	r_shadow_buffer_numshadowtrispvsbytes = 0;
	if (r_shadow_buffer_shadowtrispvs)
		Mem_Free(r_shadow_buffer_shadowtrispvs);
	r_shadow_buffer_numlighttrispvsbytes = 0;
	if (r_shadow_buffer_lighttrispvs)
		Mem_Free(r_shadow_buffer_lighttrispvs);
}

void r_shadow_newmap(void)
{
	if (r_shadow_lightcorona)                 R_SkinFrame_MarkUsed(r_shadow_lightcorona);
	if (r_editlights_sprcursor)               R_SkinFrame_MarkUsed(r_editlights_sprcursor);
	if (r_editlights_sprlight)                R_SkinFrame_MarkUsed(r_editlights_sprlight);
	if (r_editlights_sprnoshadowlight)        R_SkinFrame_MarkUsed(r_editlights_sprnoshadowlight);
	if (r_editlights_sprcubemaplight)         R_SkinFrame_MarkUsed(r_editlights_sprcubemaplight);
	if (r_editlights_sprcubemapnoshadowlight) R_SkinFrame_MarkUsed(r_editlights_sprcubemapnoshadowlight);
	if (r_editlights_sprselection)            R_SkinFrame_MarkUsed(r_editlights_sprselection);
	if (cl.worldmodel && strncmp(cl.worldmodel->name, r_shadow_mapname, sizeof(r_shadow_mapname)))
		R_Shadow_EditLights_Reload_f();
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_bumpscale_basetexture);
	Cvar_RegisterVariable(&r_shadow_bumpscale_bumpmap);
	Cvar_RegisterVariable(&r_shadow_usenormalmap);
	Cvar_RegisterVariable(&r_shadow_debuglight);
	Cvar_RegisterVariable(&r_shadow_deferred);
	Cvar_RegisterVariable(&r_shadow_deferred_8bitrange);
//	Cvar_RegisterVariable(&r_shadow_deferred_fp);
	Cvar_RegisterVariable(&r_shadow_gloss);
	Cvar_RegisterVariable(&r_shadow_gloss2intensity);
	Cvar_RegisterVariable(&r_shadow_glossintensity);
	Cvar_RegisterVariable(&r_shadow_glossexponent);
	Cvar_RegisterVariable(&r_shadow_gloss2exponent);
	Cvar_RegisterVariable(&r_shadow_glossexact);
	Cvar_RegisterVariable(&r_shadow_lightattenuationdividebias);
	Cvar_RegisterVariable(&r_shadow_lightattenuationlinearscale);
	Cvar_RegisterVariable(&r_shadow_lightintensityscale);
	Cvar_RegisterVariable(&r_shadow_lightradiusscale);
	Cvar_RegisterVariable(&r_shadow_projectdistance);
	Cvar_RegisterVariable(&r_shadow_frontsidecasting);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight_shadows);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight_svbspculling);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight_portalculling);
	Cvar_RegisterVariable(&r_shadow_realtime_world);
	Cvar_RegisterVariable(&r_shadow_realtime_world_lightmaps);
	Cvar_RegisterVariable(&r_shadow_realtime_world_shadows);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compile);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compileshadow);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compilesvbsp);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compileportalculling);
	Cvar_RegisterVariable(&r_shadow_scissor);
	Cvar_RegisterVariable(&r_shadow_shadowmapping);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_vsdct);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_texturetype);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_filterquality);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_depthbits);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_precision);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_maxsize);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_minsize);
//	Cvar_RegisterVariable(&r_shadow_shadowmapping_lod_bias);
//	Cvar_RegisterVariable(&r_shadow_shadowmapping_lod_scale);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_bordersize);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_nearclip);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_bias);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_polygonfactor);
	Cvar_RegisterVariable(&r_shadow_shadowmapping_polygonoffset);
	Cvar_RegisterVariable(&r_shadow_polygonfactor);
	Cvar_RegisterVariable(&r_shadow_polygonoffset);
	Cvar_RegisterVariable(&r_shadow_texture3d);
	Cvar_RegisterVariable(&r_coronas);
	Cvar_RegisterVariable(&r_coronas_occlusionsizescale);
	Cvar_RegisterVariable(&r_coronas_occlusionquery);
	Cvar_RegisterVariable(&gl_flashblend);
	Cvar_RegisterVariable(&gl_ext_separatestencil);
	Cvar_RegisterVariable(&gl_ext_stenciltwoside);
	if (gamemode == GAME_TENEBRAE)
	{
		Cvar_SetValue("r_shadow_gloss", 2);
		Cvar_SetValue("r_shadow_bumpscale_basetexture", 4);
	}
	R_Shadow_EditLights_Init();
	Mem_ExpandableArray_NewArray(&r_shadow_worldlightsarray, r_main_mempool, sizeof(dlight_t), 128);
	maxshadowtriangles = 0;
	shadowelements = NULL;
	maxshadowvertices = 0;
	shadowvertex3f = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxshadowmark = 0;
	numshadowmark = 0;
	shadowmark = NULL;
	shadowmarklist = NULL;
	shadowmarkcount = 0;
	maxshadowsides = 0;
	numshadowsides = 0;
	shadowsides = NULL;
	shadowsideslist = NULL;
	r_shadow_buffer_numleafpvsbytes = 0;
	r_shadow_buffer_visitingleafpvs = NULL;
	r_shadow_buffer_leafpvs = NULL;
	r_shadow_buffer_leaflist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	r_shadow_buffer_surfacepvs = NULL;
	r_shadow_buffer_surfacelist = NULL;
	r_shadow_buffer_surfacesides = NULL;
	r_shadow_buffer_shadowtrispvs = NULL;
	r_shadow_buffer_lighttrispvs = NULL;
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

matrix4x4_t matrix_attenuationxyz =
{
	{
		{0.5, 0.0, 0.0, 0.5},
		{0.0, 0.5, 0.0, 0.5},
		{0.0, 0.0, 0.5, 0.5},
		{0.0, 0.0, 0.0, 1.0}
	}
};

matrix4x4_t matrix_attenuationz =
{
	{
		{0.0, 0.0, 0.5, 0.5},
		{0.0, 0.0, 0.0, 0.5},
		{0.0, 0.0, 0.0, 0.5},
		{0.0, 0.0, 0.0, 1.0}
	}
};

void R_Shadow_ResizeShadowArrays(int numvertices, int numtriangles, int vertscale, int triscale)
{
	numvertices = ((numvertices + 255) & ~255) * vertscale;
	numtriangles = ((numtriangles + 255) & ~255) * triscale;
	// make sure shadowelements is big enough for this volume
	if (maxshadowtriangles < numtriangles)
	{
		maxshadowtriangles = numtriangles;
		if (shadowelements)
			Mem_Free(shadowelements);
		shadowelements = (int *)Mem_Alloc(r_main_mempool, maxshadowtriangles * sizeof(int[3]));
	}
	// make sure shadowvertex3f is big enough for this volume
	if (maxshadowvertices < numvertices)
	{
		maxshadowvertices = numvertices;
		if (shadowvertex3f)
			Mem_Free(shadowvertex3f);
		shadowvertex3f = (float *)Mem_Alloc(r_main_mempool, maxshadowvertices * sizeof(float[3]));
	}
}

static void R_Shadow_EnlargeLeafSurfaceTrisBuffer(int numleafs, int numsurfaces, int numshadowtriangles, int numlighttriangles)
{
	int numleafpvsbytes = (((numleafs + 7) >> 3) + 255) & ~255;
	int numsurfacepvsbytes = (((numsurfaces + 7) >> 3) + 255) & ~255;
	int numshadowtrispvsbytes = (((numshadowtriangles + 7) >> 3) + 255) & ~255;
	int numlighttrispvsbytes = (((numlighttriangles + 7) >> 3) + 255) & ~255;
	if (r_shadow_buffer_numleafpvsbytes < numleafpvsbytes)
	{
		if (r_shadow_buffer_visitingleafpvs)
			Mem_Free(r_shadow_buffer_visitingleafpvs);
		if (r_shadow_buffer_leafpvs)
			Mem_Free(r_shadow_buffer_leafpvs);
		if (r_shadow_buffer_leaflist)
			Mem_Free(r_shadow_buffer_leaflist);
		r_shadow_buffer_numleafpvsbytes = numleafpvsbytes;
		r_shadow_buffer_visitingleafpvs = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numleafpvsbytes);
		r_shadow_buffer_leafpvs = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numleafpvsbytes);
		r_shadow_buffer_leaflist = (int *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numleafpvsbytes * 8 * sizeof(*r_shadow_buffer_leaflist));
	}
	if (r_shadow_buffer_numsurfacepvsbytes < numsurfacepvsbytes)
	{
		if (r_shadow_buffer_surfacepvs)
			Mem_Free(r_shadow_buffer_surfacepvs);
		if (r_shadow_buffer_surfacelist)
			Mem_Free(r_shadow_buffer_surfacelist);
		if (r_shadow_buffer_surfacesides)
			Mem_Free(r_shadow_buffer_surfacesides);
		r_shadow_buffer_numsurfacepvsbytes = numsurfacepvsbytes;
		r_shadow_buffer_surfacepvs = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numsurfacepvsbytes);
		r_shadow_buffer_surfacelist = (int *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numsurfacepvsbytes * 8 * sizeof(*r_shadow_buffer_surfacelist));
		r_shadow_buffer_surfacesides = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numsurfacepvsbytes * 8 * sizeof(*r_shadow_buffer_surfacelist));
	}
	if (r_shadow_buffer_numshadowtrispvsbytes < numshadowtrispvsbytes)
	{
		if (r_shadow_buffer_shadowtrispvs)
			Mem_Free(r_shadow_buffer_shadowtrispvs);
		r_shadow_buffer_numshadowtrispvsbytes = numshadowtrispvsbytes;
		r_shadow_buffer_shadowtrispvs = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numshadowtrispvsbytes);
	}
	if (r_shadow_buffer_numlighttrispvsbytes < numlighttrispvsbytes)
	{
		if (r_shadow_buffer_lighttrispvs)
			Mem_Free(r_shadow_buffer_lighttrispvs);
		r_shadow_buffer_numlighttrispvsbytes = numlighttrispvsbytes;
		r_shadow_buffer_lighttrispvs = (unsigned char *)Mem_Alloc(r_main_mempool, r_shadow_buffer_numlighttrispvsbytes);
	}
}

void R_Shadow_PrepareShadowMark(int numtris)
{
	// make sure shadowmark is big enough for this volume
	if (maxshadowmark < numtris)
	{
		maxshadowmark = numtris;
		if (shadowmark)
			Mem_Free(shadowmark);
		if (shadowmarklist)
			Mem_Free(shadowmarklist);
		shadowmark = (int *)Mem_Alloc(r_main_mempool, maxshadowmark * sizeof(*shadowmark));
		shadowmarklist = (int *)Mem_Alloc(r_main_mempool, maxshadowmark * sizeof(*shadowmarklist));
		shadowmarkcount = 0;
	}
	shadowmarkcount++;
	// if shadowmarkcount wrapped we clear the array and adjust accordingly
	if (shadowmarkcount == 0)
	{
		shadowmarkcount = 1;
		memset(shadowmark, 0, maxshadowmark * sizeof(*shadowmark));
	}
	numshadowmark = 0;
}

void R_Shadow_PrepareShadowSides(int numtris)
{
    if (maxshadowsides < numtris)
    {
        maxshadowsides = numtris;
        if (shadowsides)
			Mem_Free(shadowsides);
		if (shadowsideslist)
			Mem_Free(shadowsideslist);
		shadowsides = (unsigned char *)Mem_Alloc(r_main_mempool, maxshadowsides * sizeof(*shadowsides));
		shadowsideslist = (int *)Mem_Alloc(r_main_mempool, maxshadowsides * sizeof(*shadowsideslist));
	}
	numshadowsides = 0;
}

static int R_Shadow_ConstructShadowVolume_ZFail(int innumvertices, int innumtris, const int *inelement3i, const int *inneighbor3i, const float *invertex3f, int *outnumvertices, int *outelement3i, float *outvertex3f, const float *projectorigin, const float *projectdirection, float projectdistance, int numshadowmarktris, const int *shadowmarktris)
{
	int i, j;
	int outtriangles = 0, outvertices = 0;
	const int *element;
	const float *vertex;
	float ratio, direction[3], projectvector[3];

	if (projectdirection)
		VectorScale(projectdirection, projectdistance, projectvector);
	else
		VectorClear(projectvector);

	// create the vertices
	if (projectdirection)
	{
		for (i = 0;i < numshadowmarktris;i++)
		{
			element = inelement3i + shadowmarktris[i] * 3;
			for (j = 0;j < 3;j++)
			{
				if (vertexupdate[element[j]] != vertexupdatenum)
				{
					vertexupdate[element[j]] = vertexupdatenum;
					vertexremap[element[j]] = outvertices;
					vertex = invertex3f + element[j] * 3;
					// project one copy of the vertex according to projectvector
					VectorCopy(vertex, outvertex3f);
					VectorAdd(vertex, projectvector, (outvertex3f + 3));
					outvertex3f += 6;
					outvertices += 2;
				}
			}
		}
	}
	else
	{
		for (i = 0;i < numshadowmarktris;i++)
		{
			element = inelement3i + shadowmarktris[i] * 3;
			for (j = 0;j < 3;j++)
			{
				if (vertexupdate[element[j]] != vertexupdatenum)
				{
					vertexupdate[element[j]] = vertexupdatenum;
					vertexremap[element[j]] = outvertices;
					vertex = invertex3f + element[j] * 3;
					// project one copy of the vertex to the sphere radius of the light
					// (FIXME: would projecting it to the light box be better?)
					VectorSubtract(vertex, projectorigin, direction);
					ratio = projectdistance / VectorLength(direction);
					VectorCopy(vertex, outvertex3f);
					VectorMA(projectorigin, ratio, direction, (outvertex3f + 3));
					outvertex3f += 6;
					outvertices += 2;
				}
			}
		}
	}

	if (r_shadow_frontsidecasting.integer)
	{
		for (i = 0;i < numshadowmarktris;i++)
		{
			int remappedelement[3];
			int markindex;
			const int *neighbortriangle;

			markindex = shadowmarktris[i] * 3;
			element = inelement3i + markindex;
			neighbortriangle = inneighbor3i + markindex;
			// output the front and back triangles
			outelement3i[0] = vertexremap[element[0]];
			outelement3i[1] = vertexremap[element[1]];
			outelement3i[2] = vertexremap[element[2]];
			outelement3i[3] = vertexremap[element[2]] + 1;
			outelement3i[4] = vertexremap[element[1]] + 1;
			outelement3i[5] = vertexremap[element[0]] + 1;

			outelement3i += 6;
			outtriangles += 2;
			// output the sides (facing outward from this triangle)
			if (shadowmark[neighbortriangle[0]] != shadowmarkcount)
			{
				remappedelement[0] = vertexremap[element[0]];
				remappedelement[1] = vertexremap[element[1]];
				outelement3i[0] = remappedelement[1];
				outelement3i[1] = remappedelement[0];
				outelement3i[2] = remappedelement[0] + 1;
				outelement3i[3] = remappedelement[1];
				outelement3i[4] = remappedelement[0] + 1;
				outelement3i[5] = remappedelement[1] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
			if (shadowmark[neighbortriangle[1]] != shadowmarkcount)
			{
				remappedelement[1] = vertexremap[element[1]];
				remappedelement[2] = vertexremap[element[2]];
				outelement3i[0] = remappedelement[2];
				outelement3i[1] = remappedelement[1];
				outelement3i[2] = remappedelement[1] + 1;
				outelement3i[3] = remappedelement[2];
				outelement3i[4] = remappedelement[1] + 1;
				outelement3i[5] = remappedelement[2] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
			if (shadowmark[neighbortriangle[2]] != shadowmarkcount)
			{
				remappedelement[0] = vertexremap[element[0]];
				remappedelement[2] = vertexremap[element[2]];
				outelement3i[0] = remappedelement[0];
				outelement3i[1] = remappedelement[2];
				outelement3i[2] = remappedelement[2] + 1;
				outelement3i[3] = remappedelement[0];
				outelement3i[4] = remappedelement[2] + 1;
				outelement3i[5] = remappedelement[0] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
		}
	}
	else
	{
		for (i = 0;i < numshadowmarktris;i++)
		{
			int remappedelement[3];
			int markindex;
			const int *neighbortriangle;

			markindex = shadowmarktris[i] * 3;
			element = inelement3i + markindex;
			neighbortriangle = inneighbor3i + markindex;
			// output the front and back triangles
			outelement3i[0] = vertexremap[element[2]];
			outelement3i[1] = vertexremap[element[1]];
			outelement3i[2] = vertexremap[element[0]];
			outelement3i[3] = vertexremap[element[0]] + 1;
			outelement3i[4] = vertexremap[element[1]] + 1;
			outelement3i[5] = vertexremap[element[2]] + 1;

			outelement3i += 6;
			outtriangles += 2;
			// output the sides (facing outward from this triangle)
			if (shadowmark[neighbortriangle[0]] != shadowmarkcount)
			{
				remappedelement[0] = vertexremap[element[0]];
				remappedelement[1] = vertexremap[element[1]];
				outelement3i[0] = remappedelement[0];
				outelement3i[1] = remappedelement[1];
				outelement3i[2] = remappedelement[1] + 1;
				outelement3i[3] = remappedelement[0];
				outelement3i[4] = remappedelement[1] + 1;
				outelement3i[5] = remappedelement[0] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
			if (shadowmark[neighbortriangle[1]] != shadowmarkcount)
			{
				remappedelement[1] = vertexremap[element[1]];
				remappedelement[2] = vertexremap[element[2]];
				outelement3i[0] = remappedelement[1];
				outelement3i[1] = remappedelement[2];
				outelement3i[2] = remappedelement[2] + 1;
				outelement3i[3] = remappedelement[1];
				outelement3i[4] = remappedelement[2] + 1;
				outelement3i[5] = remappedelement[1] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
			if (shadowmark[neighbortriangle[2]] != shadowmarkcount)
			{
				remappedelement[0] = vertexremap[element[0]];
				remappedelement[2] = vertexremap[element[2]];
				outelement3i[0] = remappedelement[2];
				outelement3i[1] = remappedelement[0];
				outelement3i[2] = remappedelement[0] + 1;
				outelement3i[3] = remappedelement[2];
				outelement3i[4] = remappedelement[0] + 1;
				outelement3i[5] = remappedelement[2] + 1;

				outelement3i += 6;
				outtriangles += 2;
			}
		}
	}
	if (outnumvertices)
		*outnumvertices = outvertices;
	return outtriangles;
}

static int R_Shadow_ConstructShadowVolume_ZPass(int innumvertices, int innumtris, const int *inelement3i, const int *inneighbor3i, const float *invertex3f, int *outnumvertices, int *outelement3i, float *outvertex3f, const float *projectorigin, const float *projectdirection, float projectdistance, int numshadowmarktris, const int *shadowmarktris)
{
	int i, j, k;
	int outtriangles = 0, outvertices = 0;
	const int *element;
	const float *vertex;
	float ratio, direction[3], projectvector[3];
	qboolean side[4];

	if (projectdirection)
		VectorScale(projectdirection, projectdistance, projectvector);
	else
		VectorClear(projectvector);

	for (i = 0;i < numshadowmarktris;i++)
	{
		int remappedelement[3];
		int markindex;
		const int *neighbortriangle;

		markindex = shadowmarktris[i] * 3;
		neighbortriangle = inneighbor3i + markindex;
		side[0] = shadowmark[neighbortriangle[0]] == shadowmarkcount;
		side[1] = shadowmark[neighbortriangle[1]] == shadowmarkcount;
		side[2] = shadowmark[neighbortriangle[2]] == shadowmarkcount;
		if (side[0] + side[1] + side[2] == 0)
			continue;

		side[3] = side[0];
		element = inelement3i + markindex;

		// create the vertices
		for (j = 0;j < 3;j++)
		{
			if (side[j] + side[j+1] == 0)
				continue;
			k = element[j];
			if (vertexupdate[k] != vertexupdatenum)
			{
				vertexupdate[k] = vertexupdatenum;
				vertexremap[k] = outvertices;
				vertex = invertex3f + k * 3;
				VectorCopy(vertex, outvertex3f);
				if (projectdirection)
				{
					// project one copy of the vertex according to projectvector
					VectorAdd(vertex, projectvector, (outvertex3f + 3));
				}
				else
				{
					// project one copy of the vertex to the sphere radius of the light
					// (FIXME: would projecting it to the light box be better?)
					VectorSubtract(vertex, projectorigin, direction);
					ratio = projectdistance / VectorLength(direction);
					VectorMA(projectorigin, ratio, direction, (outvertex3f + 3));
				}
				outvertex3f += 6;
				outvertices += 2;
			}
		}

		// output the sides (facing outward from this triangle)
		if (!side[0])
		{
			remappedelement[0] = vertexremap[element[0]];
			remappedelement[1] = vertexremap[element[1]];
			outelement3i[0] = remappedelement[1];
			outelement3i[1] = remappedelement[0];
			outelement3i[2] = remappedelement[0] + 1;
			outelement3i[3] = remappedelement[1];
			outelement3i[4] = remappedelement[0] + 1;
			outelement3i[5] = remappedelement[1] + 1;

			outelement3i += 6;
			outtriangles += 2;
		}
		if (!side[1])
		{
			remappedelement[1] = vertexremap[element[1]];
			remappedelement[2] = vertexremap[element[2]];
			outelement3i[0] = remappedelement[2];
			outelement3i[1] = remappedelement[1];
			outelement3i[2] = remappedelement[1] + 1;
			outelement3i[3] = remappedelement[2];
			outelement3i[4] = remappedelement[1] + 1;
			outelement3i[5] = remappedelement[2] + 1;

			outelement3i += 6;
			outtriangles += 2;
		}
		if (!side[2])
		{
			remappedelement[0] = vertexremap[element[0]];
			remappedelement[2] = vertexremap[element[2]];
			outelement3i[0] = remappedelement[0];
			outelement3i[1] = remappedelement[2];
			outelement3i[2] = remappedelement[2] + 1;
			outelement3i[3] = remappedelement[0];
			outelement3i[4] = remappedelement[2] + 1;
			outelement3i[5] = remappedelement[0] + 1;

			outelement3i += 6;
			outtriangles += 2;
		}
	}
	if (outnumvertices)
		*outnumvertices = outvertices;
	return outtriangles;
}

void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, const vec3_t projectdirection, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs)
{
	int t, tend;
	const int *e;
	const float *v[3];
	float normal[3];
	if (!BoxesOverlap(lightmins, lightmaxs, surfacemins, surfacemaxs))
		return;
	tend = firsttriangle + numtris;
	if (BoxInsideBox(surfacemins, surfacemaxs, lightmins, lightmaxs))
	{
		// surface box entirely inside light box, no box cull
		if (projectdirection)
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				TriangleNormal(invertex3f + e[0] * 3, invertex3f + e[1] * 3, invertex3f + e[2] * 3, normal);
				if (r_shadow_frontsidecasting.integer == (DotProduct(normal, projectdirection) < 0))
					shadowmarklist[numshadowmark++] = t;
			}
		}
		else
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
				if (r_shadow_frontsidecasting.integer == PointInfrontOfTriangle(projectorigin, invertex3f + e[0] * 3, invertex3f + e[1] * 3, invertex3f + e[2] * 3))
					shadowmarklist[numshadowmark++] = t;
		}
	}
	else
	{
		// surface box not entirely inside light box, cull each triangle
		if (projectdirection)
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3;
				v[1] = invertex3f + e[1] * 3;
				v[2] = invertex3f + e[2] * 3;
				TriangleNormal(v[0], v[1], v[2], normal);
				if (r_shadow_frontsidecasting.integer == (DotProduct(normal, projectdirection) < 0)
				 && TriangleOverlapsBox(v[0], v[1], v[2], lightmins, lightmaxs))
					shadowmarklist[numshadowmark++] = t;
			}
		}
		else
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3;
				v[1] = invertex3f + e[1] * 3;
				v[2] = invertex3f + e[2] * 3;
				if (r_shadow_frontsidecasting.integer == PointInfrontOfTriangle(projectorigin, v[0], v[1], v[2])
				 && TriangleOverlapsBox(v[0], v[1], v[2], lightmins, lightmaxs))
					shadowmarklist[numshadowmark++] = t;
			}
		}
	}
}

qboolean R_Shadow_UseZPass(vec3_t mins, vec3_t maxs)
{
#if 1
	return false;
#else
	if (r_shadow_compilingrtlight || !r_shadow_frontsidecasting.integer || !r_shadow_usezpassifpossible.integer)
		return false;
	// check if the shadow volume intersects the near plane
	//
	// a ray between the eye and light origin may intersect the caster,
	// indicating that the shadow may touch the eye location, however we must
	// test the near plane (a polygon), not merely the eye location, so it is
	// easiest to enlarge the caster bounding shape slightly for this.
	// TODO
	return true;
#endif
}

void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, const vec3_t projectdirection, float projectdistance, int nummarktris, const int *marktris, vec3_t trismins, vec3_t trismaxs)
{
	int i, tris, outverts;
	if (projectdistance < 0.1)
	{
		Con_Printf("R_Shadow_Volume: projectdistance %f\n", projectdistance);
		return;
	}
	if (!numverts || !nummarktris)
		return;
	// make sure shadowelements is big enough for this volume
	if (maxshadowtriangles < nummarktris*8 || maxshadowvertices < numverts*2)
		R_Shadow_ResizeShadowArrays(numverts, nummarktris, 2, 8);

	if (maxvertexupdate < numverts)
	{
		maxvertexupdate = numverts;
		if (vertexupdate)
			Mem_Free(vertexupdate);
		if (vertexremap)
			Mem_Free(vertexremap);
		vertexupdate = (int *)Mem_Alloc(r_main_mempool, maxvertexupdate * sizeof(int));
		vertexremap = (int *)Mem_Alloc(r_main_mempool, maxvertexupdate * sizeof(int));
		vertexupdatenum = 0;
	}
	vertexupdatenum++;
	if (vertexupdatenum == 0)
	{
		vertexupdatenum = 1;
		memset(vertexupdate, 0, maxvertexupdate * sizeof(int));
		memset(vertexremap, 0, maxvertexupdate * sizeof(int));
	}

	for (i = 0;i < nummarktris;i++)
		shadowmark[marktris[i]] = shadowmarkcount;

	if (r_shadow_compilingrtlight)
	{
		// if we're compiling an rtlight, capture the mesh
		//tris = R_Shadow_ConstructShadowVolume_ZPass(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, shadowvertex3f, projectorigin, projectdirection, projectdistance, nummarktris, marktris);
		//Mod_ShadowMesh_AddMesh(r_main_mempool, r_shadow_compilingrtlight->static_meshchain_shadow_zpass, NULL, NULL, NULL, shadowvertex3f, NULL, NULL, NULL, NULL, tris, shadowelements);
		tris = R_Shadow_ConstructShadowVolume_ZFail(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, shadowvertex3f, projectorigin, projectdirection, projectdistance, nummarktris, marktris);
		Mod_ShadowMesh_AddMesh(r_main_mempool, r_shadow_compilingrtlight->static_meshchain_shadow_zfail, NULL, NULL, NULL, shadowvertex3f, NULL, NULL, NULL, NULL, tris, shadowelements);
	}
	else if (r_shadow_rendermode == R_SHADOW_RENDERMODE_VISIBLEVOLUMES)
	{
		tris = R_Shadow_ConstructShadowVolume_ZFail(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, shadowvertex3f, projectorigin, projectdirection, projectdistance, nummarktris, marktris);
		R_Mesh_VertexPointer(shadowvertex3f, 0, 0);
		R_Mesh_Draw(0, outverts, 0, tris, shadowelements, NULL, 0, 0);
	}
	else
	{
		// decide which type of shadow to generate and set stencil mode
		R_Shadow_RenderMode_StencilShadowVolumes(R_Shadow_UseZPass(trismins, trismaxs));
		// generate the sides or a solid volume, depending on type
		if (r_shadow_rendermode >= R_SHADOW_RENDERMODE_ZPASS_STENCIL && r_shadow_rendermode <= R_SHADOW_RENDERMODE_ZPASS_STENCILTWOSIDE)
			tris = R_Shadow_ConstructShadowVolume_ZPass(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, shadowvertex3f, projectorigin, projectdirection, projectdistance, nummarktris, marktris);
		else
			tris = R_Shadow_ConstructShadowVolume_ZFail(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, shadowvertex3f, projectorigin, projectdirection, projectdistance, nummarktris, marktris);
		r_refdef.stats.lights_dynamicshadowtriangles += tris;
		r_refdef.stats.lights_shadowtriangles += tris;
		CHECKGLERROR
		R_Mesh_VertexPointer(shadowvertex3f, 0, 0);
		if (r_shadow_rendermode == R_SHADOW_RENDERMODE_ZPASS_STENCIL)
		{
			// increment stencil if frontface is infront of depthbuffer
			GL_CullFace(r_refdef.view.cullface_front);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);CHECKGLERROR
			R_Mesh_Draw(0, outverts, 0, tris, shadowelements, NULL, 0, 0);
			// decrement stencil if backface is infront of depthbuffer
			GL_CullFace(r_refdef.view.cullface_back);
			qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);CHECKGLERROR
		}
		else if (r_shadow_rendermode == R_SHADOW_RENDERMODE_ZFAIL_STENCIL)
		{
			// decrement stencil if backface is behind depthbuffer
			GL_CullFace(r_refdef.view.cullface_front);
			qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);CHECKGLERROR
			R_Mesh_Draw(0, outverts, 0, tris, shadowelements, NULL, 0, 0);
			// increment stencil if frontface is behind depthbuffer
			GL_CullFace(r_refdef.view.cullface_back);
			qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);CHECKGLERROR
		}
		R_Mesh_Draw(0, outverts, 0, tris, shadowelements, NULL, 0, 0);
		CHECKGLERROR
	}
}

int R_Shadow_CalcTriangleSideMask(const vec3_t p1, const vec3_t p2, const vec3_t p3, float bias)
{
    // p1, p2, p3 are in the cubemap's local coordinate system
    // bias = border/(size - border)
	int mask = 0x3F;

    float dp1 = p1[0] + p1[1], dn1 = p1[0] - p1[1], ap1 = fabs(dp1), an1 = fabs(dn1),
    	  dp2 = p2[0] + p2[1], dn2 = p2[0] - p2[1], ap2 = fabs(dp2), an2 = fabs(dn2),
    	  dp3 = p3[0] + p3[1], dn3 = p3[0] - p3[1], ap3 = fabs(dp3), an3 = fabs(dn3);
	if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
    	mask &= (3<<4)
			| (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
			| (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
			| (dp3 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<4)
            | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))            
            | (dn3 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = p1[1] + p1[2], dn1 = p1[1] - p1[2], ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2[1] + p2[2], dn2 = p2[1] - p2[2], ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3[1] + p3[2], dn3 = p3[1] - p3[2], ap3 = fabs(dp3), an3 = fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<0)
            | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))            
            | (dp3 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<0)
            | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn3 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = p1[2] + p1[0], dn1 = p1[2] - p1[0], ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2[2] + p2[0], dn2 = p2[2] - p2[0], ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3[2] + p3[0], dn3 = p3[2] - p3[0], ap3 = fabs(dp3), an3 = fabs(dn3);
    if(ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<2)
            | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp3 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if(an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<2)
            | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn3 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

	return mask;
}

int R_Shadow_CalcBBoxSideMask(const vec3_t mins, const vec3_t maxs, const matrix4x4_t *worldtolight, const matrix4x4_t *radiustolight, float bias)
{
	vec3_t center, radius, lightcenter, lightradius, pmin, pmax;
	float dp1, dn1, ap1, an1, dp2, dn2, ap2, an2;
	int mask = 0x3F;

	VectorSubtract(maxs, mins, radius);
    VectorScale(radius, 0.5f, radius);
    VectorAdd(mins, radius, center);
    Matrix4x4_Transform(worldtolight, center, lightcenter);
	Matrix4x4_Transform3x3(radiustolight, radius, lightradius);
	VectorSubtract(lightcenter, lightradius, pmin);
	VectorAdd(lightcenter, lightradius, pmax);

    dp1 = pmax[0] + pmax[1], dn1 = pmax[0] - pmin[1], ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = pmin[0] + pmin[1], dn2 = pmin[0] - pmax[1], ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<4)
            | (dp1 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp2 >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<4)
            | (dn1 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = pmax[1] + pmax[2], dn1 = pmax[1] - pmin[2], ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = pmin[1] + pmin[2], dn2 = pmin[1] - pmax[2], ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<0)
            | (dp1 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<0)
            | (dn1 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = pmax[2] + pmax[0], dn1 = pmax[2] - pmin[0], ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = pmin[2] + pmin[0], dn2 = pmin[2] - pmax[0], ap2 = fabs(dp2), an2 = fabs(dn2);
    if(ap1 > bias*an1 && ap2 > bias*an2)
        mask &= (3<<2)
            | (dp1 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if(an1 > bias*ap1 && an2 > bias*ap2)
        mask &= (3<<2)
            | (dn1 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

    return mask;
}

#define R_Shadow_CalcEntitySideMask(ent, worldtolight, radiustolight, bias) R_Shadow_CalcBBoxSideMask((ent)->mins, (ent)->maxs, worldtolight, radiustolight, bias)

int R_Shadow_CalcSphereSideMask(const vec3_t p, float radius, float bias)
{
    // p is in the cubemap's local coordinate system
    // bias = border/(size - border)
    float dxyp = p[0] + p[1], dxyn = p[0] - p[1], axyp = fabs(dxyp), axyn = fabs(dxyn);
    float dyzp = p[1] + p[2], dyzn = p[1] - p[2], ayzp = fabs(dyzp), ayzn = fabs(dyzn);
    float dzxp = p[2] + p[0], dzxn = p[2] - p[0], azxp = fabs(dzxp), azxn = fabs(dzxn);
    int mask = 0x3F;
    if(axyp > bias*axyn + radius) mask &= dxyp < 0 ? ~((1<<0)|(1<<2)) : ~((2<<0)|(2<<2));
    if(axyn > bias*axyp + radius) mask &= dxyn < 0 ? ~((1<<0)|(2<<2)) : ~((2<<0)|(1<<2));
    if(ayzp > bias*ayzn + radius) mask &= dyzp < 0 ? ~((1<<2)|(1<<4)) : ~((2<<2)|(2<<4));
    if(ayzn > bias*ayzp + radius) mask &= dyzn < 0 ? ~((1<<2)|(2<<4)) : ~((2<<2)|(1<<4));
    if(azxp > bias*azxn + radius) mask &= dzxp < 0 ? ~((1<<4)|(1<<0)) : ~((2<<4)|(2<<0));
    if(azxn > bias*azxp + radius) mask &= dzxn < 0 ? ~((1<<4)|(2<<0)) : ~((2<<4)|(1<<0));
    return mask;
}

int R_Shadow_CullFrustumSides(rtlight_t *rtlight, float size, float border)
{
	int i;
	vec3_t p, n;
	int sides = 0x3F, masks[6] = { 3<<4, 3<<4, 3<<0, 3<<0, 3<<2, 3<<2 };
	float scale = (size - 2*border)/size, len;
	float bias = border / (float)(size - border), dp, dn, ap, an;
	// check if cone enclosing side would cross frustum plane 
	scale = 2 / (scale*scale + 2);
	for (i = 0;i < 5;i++)
	{
		if (PlaneDiff(rtlight->shadoworigin, &r_refdef.view.frustum[i]) > -0.03125)
			continue;
		Matrix4x4_Transform3x3(&rtlight->matrix_worldtolight, r_refdef.view.frustum[i].normal, n);
		len = scale*VectorLength2(n);
		if(n[0]*n[0] > len) sides &= n[0] < 0 ? ~(1<<0) : ~(2 << 0);
		if(n[1]*n[1] > len) sides &= n[1] < 0 ? ~(1<<2) : ~(2 << 2);
		if(n[2]*n[2] > len) sides &= n[2] < 0 ? ~(1<<4) : ~(2 << 4);
	}
	if (PlaneDiff(rtlight->shadoworigin, &r_refdef.view.frustum[4]) >= r_refdef.farclip - r_refdef.nearclip + 0.03125)
	{
        Matrix4x4_Transform3x3(&rtlight->matrix_worldtolight, r_refdef.view.frustum[4].normal, n);
        len = scale*VectorLength(n);
		if(n[0]*n[0] > len) sides &= n[0] >= 0 ? ~(1<<0) : ~(2 << 0);
		if(n[1]*n[1] > len) sides &= n[1] >= 0 ? ~(1<<2) : ~(2 << 2);
		if(n[2]*n[2] > len) sides &= n[2] >= 0 ? ~(1<<4) : ~(2 << 4);
	}
	// this next test usually clips off more sides than the former, but occasionally clips fewer/different ones, so do both and combine results
	// check if frustum corners/origin cross plane sides
	for (i = 0;i < 5;i++)
	{
		Matrix4x4_Transform(&rtlight->matrix_worldtolight, !i ? r_refdef.view.origin : r_refdef.view.frustumcorner[i-1], p);
		dp = p[0] + p[1], dn = p[0] - p[1], ap = fabs(dp), an = fabs(dn),
		masks[0] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
		masks[1] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));
		dp = p[1] + p[2], dn = p[1] - p[2], ap = fabs(dp), an = fabs(dn),
		masks[2] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
		masks[3] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));
		dp = p[2] + p[0], dn = p[2] - p[0], ap = fabs(dp), an = fabs(dn),
		masks[4] |= ap <= bias*an ? 0x3F : (dp >= 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
		masks[5] |= an <= bias*ap ? 0x3F : (dn >= 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));
	}
	return sides & masks[0] & masks[1] & masks[2] & masks[3] & masks[4] & masks[5];
}

int R_Shadow_ChooseSidesFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const matrix4x4_t *worldtolight, const vec3_t projectorigin, const vec3_t projectdirection, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs, int *totals)
{
	int t, tend;
	const int *e;
	const float *v[3];
	float normal[3];
	vec3_t p[3];
	float bias;
	int mask, surfacemask = 0;
	if (!BoxesOverlap(lightmins, lightmaxs, surfacemins, surfacemaxs))
		return 0;
	bias = r_shadow_shadowmapborder / (float)(r_shadow_shadowmapmaxsize - r_shadow_shadowmapborder);
	tend = firsttriangle + numtris;
	if (BoxInsideBox(surfacemins, surfacemaxs, lightmins, lightmaxs))
	{
		// surface box entirely inside light box, no box cull
		if (projectdirection)
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3, v[1] = invertex3f + e[1] * 3, v[2] = invertex3f + e[2] * 3;
				TriangleNormal(v[0], v[1], v[2], normal);
				if (r_shadow_frontsidecasting.integer == (DotProduct(normal, projectdirection) < 0))
				{
					Matrix4x4_Transform(worldtolight, v[0], p[0]), Matrix4x4_Transform(worldtolight, v[1], p[1]), Matrix4x4_Transform(worldtolight, v[2], p[2]);
					mask = R_Shadow_CalcTriangleSideMask(p[0], p[1], p[2], bias);
					surfacemask |= mask;
					if(totals)
					{
						totals[0] += mask&1, totals[1] += (mask>>1)&1, totals[2] += (mask>>2)&1, totals[3] += (mask>>3)&1, totals[4] += (mask>>4)&1, totals[5] += mask>>5;
						shadowsides[numshadowsides] = mask;
						shadowsideslist[numshadowsides++] = t;
					}
				}
			}
		}
		else
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3, v[1] = invertex3f + e[1] * 3,	v[2] = invertex3f + e[2] * 3;
				if (r_shadow_frontsidecasting.integer == PointInfrontOfTriangle(projectorigin, v[0], v[1], v[2]))
				{
					Matrix4x4_Transform(worldtolight, v[0], p[0]), Matrix4x4_Transform(worldtolight, v[1], p[1]), Matrix4x4_Transform(worldtolight, v[2], p[2]);
					mask = R_Shadow_CalcTriangleSideMask(p[0], p[1], p[2], bias);
					surfacemask |= mask;
					if(totals)
					{
						totals[0] += mask&1, totals[1] += (mask>>1)&1, totals[2] += (mask>>2)&1, totals[3] += (mask>>3)&1, totals[4] += (mask>>4)&1, totals[5] += mask>>5;
						shadowsides[numshadowsides] = mask;
						shadowsideslist[numshadowsides++] = t;
					}
				}
			}
		}
	}
	else
	{
		// surface box not entirely inside light box, cull each triangle
		if (projectdirection)
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3, v[1] = invertex3f + e[1] * 3,	v[2] = invertex3f + e[2] * 3;
				TriangleNormal(v[0], v[1], v[2], normal);
				if (r_shadow_frontsidecasting.integer == (DotProduct(normal, projectdirection) < 0)
				 && TriangleOverlapsBox(v[0], v[1], v[2], lightmins, lightmaxs))
				{
					Matrix4x4_Transform(worldtolight, v[0], p[0]), Matrix4x4_Transform(worldtolight, v[1], p[1]), Matrix4x4_Transform(worldtolight, v[2], p[2]);
					mask = R_Shadow_CalcTriangleSideMask(p[0], p[1], p[2], bias);
					surfacemask |= mask;
					if(totals)
					{
						totals[0] += mask&1, totals[1] += (mask>>1)&1, totals[2] += (mask>>2)&1, totals[3] += (mask>>3)&1, totals[4] += (mask>>4)&1, totals[5] += mask>>5;
						shadowsides[numshadowsides] = mask;
						shadowsideslist[numshadowsides++] = t;
					}
				}
			}
		}
		else
		{
			for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			{
				v[0] = invertex3f + e[0] * 3, v[1] = invertex3f + e[1] * 3, v[2] = invertex3f + e[2] * 3;
				if (r_shadow_frontsidecasting.integer == PointInfrontOfTriangle(projectorigin, v[0], v[1], v[2])
				 && TriangleOverlapsBox(v[0], v[1], v[2], lightmins, lightmaxs))
				{
					Matrix4x4_Transform(worldtolight, v[0], p[0]), Matrix4x4_Transform(worldtolight, v[1], p[1]), Matrix4x4_Transform(worldtolight, v[2], p[2]);
					mask = R_Shadow_CalcTriangleSideMask(p[0], p[1], p[2], bias);
					surfacemask |= mask;
					if(totals)
					{
						totals[0] += mask&1, totals[1] += (mask>>1)&1, totals[2] += (mask>>2)&1, totals[3] += (mask>>3)&1, totals[4] += (mask>>4)&1, totals[5] += mask>>5;
						shadowsides[numshadowsides] = mask;
						shadowsideslist[numshadowsides++] = t;
					}
				}
			}
		}
	}
	return surfacemask;
}

void R_Shadow_ShadowMapFromList(int numverts, int numtris, const float *vertex3f, const int *elements, int numsidetris, const int *sidetotals, const unsigned char *sides, const int *sidetris)
{
	int i, j, outtriangles = 0;
	int *outelement3i[6];
	if (!numverts || !numsidetris || !r_shadow_compilingrtlight)
		return;
	outtriangles = sidetotals[0] + sidetotals[1] + sidetotals[2] + sidetotals[3] + sidetotals[4] + sidetotals[5];
	// make sure shadowelements is big enough for this mesh
	if (maxshadowtriangles < outtriangles)
		R_Shadow_ResizeShadowArrays(0, outtriangles, 0, 1);

	// compute the offset and size of the separate index lists for each cubemap side
	outtriangles = 0;
	for (i = 0;i < 6;i++)
	{
		outelement3i[i] = shadowelements + outtriangles * 3;
		r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap->sideoffsets[i] = outtriangles;
		r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap->sidetotals[i] = sidetotals[i];
		outtriangles += sidetotals[i];
	}

	// gather up the (sparse) triangles into separate index lists for each cubemap side
	for (i = 0;i < numsidetris;i++)
	{
		const int *element = elements + sidetris[i] * 3;
		for (j = 0;j < 6;j++)
		{
			if (sides[i] & (1 << j))
			{
				outelement3i[j][0] = element[0];
				outelement3i[j][1] = element[1];
				outelement3i[j][2] = element[2];
				outelement3i[j] += 3;
			}
		}
	}
			
	Mod_ShadowMesh_AddMesh(r_main_mempool, r_shadow_compilingrtlight->static_meshchain_shadow_shadowmap, NULL, NULL, NULL, vertex3f, NULL, NULL, NULL, NULL, outtriangles, shadowelements);
}

static void R_Shadow_MakeTextures_MakeCorona(void)
{
	float dx, dy;
	int x, y, a;
	unsigned char pixels[32][32][4];
	for (y = 0;y < 32;y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0;x < 32;x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = (int)(((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / (1.0f + 0.2))) * 32.0f / (1.0f / (1.0f + 0.2)));
			a = bound(0, a, 255);
			pixels[y][x][0] = a;
			pixels[y][x][1] = a;
			pixels[y][x][2] = a;
			pixels[y][x][3] = 255;
		}
	}
	r_shadow_lightcorona = R_SkinFrame_LoadInternalBGRA("lightcorona", TEXF_FORCELINEAR, &pixels[0][0][0], 32, 32);
}

static unsigned int R_Shadow_MakeTextures_SamplePoint(float x, float y, float z)
{
	float dist = sqrt(x*x+y*y+z*z);
	float intensity = dist < 1 ? ((1.0f - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist)) : 0;
	// note this code could suffer byte order issues except that it is multiplying by an integer that reads the same both ways
	return (unsigned char)bound(0, intensity * 256.0f, 255) * 0x01010101;
}

static void R_Shadow_MakeTextures(void)
{
	int x, y, z;
	float intensity, dist;
	unsigned int *data;
	R_Shadow_FreeShadowMaps();
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_attenlinearscale = r_shadow_lightattenuationlinearscale.value;
	r_shadow_attendividebias = r_shadow_lightattenuationdividebias.value;
	data = (unsigned int *)Mem_Alloc(tempmempool, max(max(ATTEN3DSIZE*ATTEN3DSIZE*ATTEN3DSIZE, ATTEN2DSIZE*ATTEN2DSIZE), ATTEN1DSIZE) * 4);
	// the table includes one additional value to avoid the need to clamp indexing due to minor math errors
	for (x = 0;x <= ATTENTABLESIZE;x++)
	{
		dist = (x + 0.5f) * (1.0f / ATTENTABLESIZE) * (1.0f / 0.9375);
		intensity = dist < 1 ? ((1.0f - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist)) : 0;
		r_shadow_attentable[x] = bound(0, intensity, 1);
	}
	// 1D gradient texture
	for (x = 0;x < ATTEN1DSIZE;x++)
		data[x] = R_Shadow_MakeTextures_SamplePoint((x + 0.5f) * (1.0f / ATTEN1DSIZE) * (1.0f / 0.9375), 0, 0);
	r_shadow_attenuationgradienttexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation1d", ATTEN1DSIZE, 1, (unsigned char *)data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCELINEAR, NULL);
	// 2D circle texture
	for (y = 0;y < ATTEN2DSIZE;y++)
		for (x = 0;x < ATTEN2DSIZE;x++)
			data[y*ATTEN2DSIZE+x] = R_Shadow_MakeTextures_SamplePoint(((x + 0.5f) * (2.0f / ATTEN2DSIZE) - 1.0f) * (1.0f / 0.9375), ((y + 0.5f) * (2.0f / ATTEN2DSIZE) - 1.0f) * (1.0f / 0.9375), 0);
	r_shadow_attenuation2dtexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation2d", ATTEN2DSIZE, ATTEN2DSIZE, (unsigned char *)data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCELINEAR, NULL);
	// 3D sphere texture
	if (r_shadow_texture3d.integer && vid.support.ext_texture_3d)
	{
		for (z = 0;z < ATTEN3DSIZE;z++)
			for (y = 0;y < ATTEN3DSIZE;y++)
				for (x = 0;x < ATTEN3DSIZE;x++)
					data[(z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x] = R_Shadow_MakeTextures_SamplePoint(((x + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375), ((y + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375), ((z + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375));
		r_shadow_attenuation3dtexture = R_LoadTexture3D(r_shadow_texturepool, "attenuation3d", ATTEN3DSIZE, ATTEN3DSIZE, ATTEN3DSIZE, (unsigned char *)data, TEXTYPE_BGRA, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCELINEAR, NULL);
	}
	else
		r_shadow_attenuation3dtexture = NULL;
	Mem_Free(data);

	R_Shadow_MakeTextures_MakeCorona();

	// Editor light sprites
	r_editlights_sprcursor = R_SkinFrame_LoadInternal8bit("gfx/editlights/cursor", TEXF_ALPHA | TEXF_CLAMP, (const unsigned char *)
	"................"
	".3............3."
	"..5...2332...5.."
	"...7.3....3.7..."
	"....7......7...."
	"...3.7....7.3..."
	"..2...7..7...2.."
	"..3..........3.."
	"..3..........3.."
	"..2...7..7...2.."
	"...3.7....7.3..."
	"....7......7...."
	"...7.3....3.7..."
	"..5...2332...5.."
	".3............3."
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
	r_editlights_sprlight = R_SkinFrame_LoadInternal8bit("gfx/editlights/light", TEXF_ALPHA | TEXF_CLAMP, (const unsigned char *)
	"................"
	"................"
	"......1111......"
	"....11233211...."
	"...1234554321..."
	"...1356776531..."
	"..124677776421.."
	"..135777777531.."
	"..135777777531.."
	"..124677776421.."
	"...1356776531..."
	"...1234554321..."
	"....11233211...."
	"......1111......"
	"................"
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
	r_editlights_sprnoshadowlight = R_SkinFrame_LoadInternal8bit("gfx/editlights/noshadow", TEXF_ALPHA | TEXF_CLAMP, (const unsigned char *)
	"................"
	"................"
	"......1111......"
	"....11233211...."
	"...1234554321..."
	"...1356226531..."
	"..12462..26421.."
	"..1352....2531.."
	"..1352....2531.."
	"..12462..26421.."
	"...1356226531..."
	"...1234554321..."
	"....11233211...."
	"......1111......"
	"................"
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
	r_editlights_sprcubemaplight = R_SkinFrame_LoadInternal8bit("gfx/editlights/cubemaplight", TEXF_ALPHA | TEXF_CLAMP, (const unsigned char *)
	"................"
	"................"
	"......2772......"
	"....27755772...."
	"..277533335772.."
	"..753333333357.."
	"..777533335777.."
	"..735775577537.."
	"..733357753337.."
	"..733337733337.."
	"..753337733357.."
	"..277537735772.."
	"....27777772...."
	"......2772......"
	"................"
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
	r_editlights_sprcubemapnoshadowlight = R_SkinFrame_LoadInternal8bit("gfx/editlights/cubemapnoshadowlight", TEXF_ALPHA | TEXF_CLAMP, (const unsigned char *)
	"................"
	"................"
	"......2772......"
	"....27722772...."
	"..2772....2772.."
	"..72........27.."
	"..7772....2777.."
	"..7.27722772.7.."
	"..7...2772...7.."
	"..7....77....7.."
	"..72...77...27.."
	"..2772.77.2772.."
	"....27777772...."
	"......2772......"
	"................"
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
	r_editlights_sprselection = R_SkinFrame_LoadInternal8bit("gfx/editlights/selection", TEXF_ALPHA | TEXF_CLAMP, (unsigned char *)
	"................"
	".777752..257777."
	".742........247."
	".72..........27."
	".7............7."
	".5............5."
	".2............2."
	"................"
	"................"
	".2............2."
	".5............5."
	".7............7."
	".72..........27."
	".742........247."
	".777752..257777."
	"................"
	, 16, 16, palette_bgra_embeddedpic, palette_bgra_embeddedpic);
}

void R_Shadow_ValidateCvars(void)
{
	if (r_shadow_texture3d.integer && !vid.support.ext_texture_3d)
		Cvar_SetValueQuick(&r_shadow_texture3d, 0);
	if (gl_ext_separatestencil.integer && !vid.support.ati_separate_stencil)
		Cvar_SetValueQuick(&gl_ext_separatestencil, 0);
	if (gl_ext_stenciltwoside.integer && !vid.support.ext_stencil_two_side)
		Cvar_SetValueQuick(&gl_ext_stenciltwoside, 0);
}

void R_Shadow_RenderMode_Begin(void)
{
#if 0
	GLint drawbuffer;
	GLint readbuffer;
#endif
	R_Shadow_ValidateCvars();

	if (!r_shadow_attenuation2dtexture
	 || (!r_shadow_attenuation3dtexture && r_shadow_texture3d.integer)
	 || r_shadow_lightattenuationdividebias.value != r_shadow_attendividebias
	 || r_shadow_lightattenuationlinearscale.value != r_shadow_attenlinearscale)
		R_Shadow_MakeTextures();

	CHECKGLERROR
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	GL_DepthTest(true);
	GL_DepthMask(false);
	GL_Color(0, 0, 0, 1);
	GL_Scissor(r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);

	r_shadow_rendermode = R_SHADOW_RENDERMODE_NONE;

	if (gl_ext_separatestencil.integer && vid.support.ati_separate_stencil)
	{
		r_shadow_shadowingrendermode_zpass = R_SHADOW_RENDERMODE_ZPASS_SEPARATESTENCIL;
		r_shadow_shadowingrendermode_zfail = R_SHADOW_RENDERMODE_ZFAIL_SEPARATESTENCIL;
	}
	else if (gl_ext_stenciltwoside.integer && vid.support.ext_stencil_two_side)
	{
		r_shadow_shadowingrendermode_zpass = R_SHADOW_RENDERMODE_ZPASS_STENCILTWOSIDE;
		r_shadow_shadowingrendermode_zfail = R_SHADOW_RENDERMODE_ZFAIL_STENCILTWOSIDE;
	}
	else
	{
		r_shadow_shadowingrendermode_zpass = R_SHADOW_RENDERMODE_ZPASS_STENCIL;
		r_shadow_shadowingrendermode_zfail = R_SHADOW_RENDERMODE_ZFAIL_STENCIL;
	}

	switch(vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_LIGHT_GLSL;
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		if (r_textureunits.integer >= 2 && vid.texunits >= 2 && r_shadow_texture3d.integer && r_shadow_attenuation3dtexture)
			r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_LIGHT_VERTEX3DATTEN;
		else if (r_textureunits.integer >= 3 && vid.texunits >= 3)
			r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_LIGHT_VERTEX2D1DATTEN;
		else if (r_textureunits.integer >= 2 && vid.texunits >= 2)
			r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_LIGHT_VERTEX2DATTEN;
		else
			r_shadow_lightingrendermode = R_SHADOW_RENDERMODE_LIGHT_VERTEX;
		break;
	}

	CHECKGLERROR
#if 0
	qglGetIntegerv(GL_DRAW_BUFFER, &drawbuffer);CHECKGLERROR
	qglGetIntegerv(GL_READ_BUFFER, &readbuffer);CHECKGLERROR
	r_shadow_drawbuffer = drawbuffer;
	r_shadow_readbuffer = readbuffer;
#endif
	r_shadow_cullface_front = r_refdef.view.cullface_front;
	r_shadow_cullface_back = r_refdef.view.cullface_back;
}

void R_Shadow_RenderMode_ActiveLight(const rtlight_t *rtlight)
{
	rsurface.rtlight = rtlight;
}

void R_Shadow_RenderMode_Reset(void)
{
	CHECKGLERROR
	if (r_shadow_rendermode == R_SHADOW_RENDERMODE_ZPASS_STENCILTWOSIDE || r_shadow_rendermode == R_SHADOW_RENDERMODE_ZFAIL_STENCILTWOSIDE)
	{
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);CHECKGLERROR
	}
	if (vid.support.ext_framebuffer_object)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);CHECKGLERROR
	}
#if 0
	qglDrawBuffer(r_shadow_drawbuffer);CHECKGLERROR
	qglReadBuffer(r_shadow_readbuffer);CHECKGLERROR
#endif
	R_SetViewport(&r_refdef.view.viewport);
	GL_Scissor(r_shadow_lightscissor[0], r_shadow_lightscissor[1], r_shadow_lightscissor[2], r_shadow_lightscissor[3]);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	GL_DepthRange(0, 1);
	GL_DepthTest(true);
	GL_DepthMask(false);
	qglDepthFunc(GL_LEQUAL);CHECKGLERROR
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);CHECKGLERROR
	qglDisable(GL_STENCIL_TEST);CHECKGLERROR
	qglStencilMask(255);CHECKGLERROR
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);CHECKGLERROR
	qglStencilFunc(GL_ALWAYS, 128, 255);CHECKGLERROR
	r_refdef.view.cullface_front = r_shadow_cullface_front;
	r_refdef.view.cullface_back = r_shadow_cullface_back;
	GL_CullFace(r_refdef.view.cullface_back);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	r_shadow_usingshadowmaprect = false;
	r_shadow_usingshadowmapcube = false;
	r_shadow_usingshadowmap2d = false;
	CHECKGLERROR
}

void R_Shadow_ClearStencil(void)
{
	CHECKGLERROR
	GL_Clear(GL_STENCIL_BUFFER_BIT);
	r_refdef.stats.lights_clears++;
}

void R_Shadow_RenderMode_StencilShadowVolumes(qboolean zpass)
{
	r_shadow_rendermode_t mode = zpass ? r_shadow_shadowingrendermode_zpass : r_shadow_shadowingrendermode_zfail;
	if (r_shadow_rendermode == mode)
		return;
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	GL_ColorMask(0, 0, 0, 0);
	GL_PolygonOffset(r_refdef.shadowpolygonfactor, r_refdef.shadowpolygonoffset);CHECKGLERROR
	R_SetupShader_DepthOrShadow();
	qglDepthFunc(GL_LESS);CHECKGLERROR
	qglEnable(GL_STENCIL_TEST);CHECKGLERROR
	r_shadow_rendermode = mode;
	switch(mode)
	{
	default:
		break;
	case R_SHADOW_RENDERMODE_ZPASS_SEPARATESTENCIL:
		GL_CullFace(GL_NONE);
		qglStencilOpSeparate(r_refdef.view.cullface_front, GL_KEEP, GL_KEEP, GL_INCR);CHECKGLERROR
		qglStencilOpSeparate(r_refdef.view.cullface_back, GL_KEEP, GL_KEEP, GL_DECR);CHECKGLERROR
		break;
	case R_SHADOW_RENDERMODE_ZFAIL_SEPARATESTENCIL:
		GL_CullFace(GL_NONE);
		qglStencilOpSeparate(r_refdef.view.cullface_front, GL_KEEP, GL_INCR, GL_KEEP);CHECKGLERROR
		qglStencilOpSeparate(r_refdef.view.cullface_back, GL_KEEP, GL_DECR, GL_KEEP);CHECKGLERROR
		break;
	case R_SHADOW_RENDERMODE_ZPASS_STENCILTWOSIDE:
		GL_CullFace(GL_NONE);
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);CHECKGLERROR
		qglActiveStencilFaceEXT(r_refdef.view.cullface_front);CHECKGLERROR
		qglStencilMask(255);CHECKGLERROR
		qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);CHECKGLERROR
		qglActiveStencilFaceEXT(r_refdef.view.cullface_back);CHECKGLERROR
		qglStencilMask(255);CHECKGLERROR
		qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);CHECKGLERROR
		break;
	case R_SHADOW_RENDERMODE_ZFAIL_STENCILTWOSIDE:
		GL_CullFace(GL_NONE);
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);CHECKGLERROR
		qglActiveStencilFaceEXT(r_refdef.view.cullface_front);CHECKGLERROR
		qglStencilMask(255);CHECKGLERROR
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);CHECKGLERROR
		qglActiveStencilFaceEXT(r_refdef.view.cullface_back);CHECKGLERROR
		qglStencilMask(255);CHECKGLERROR
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);CHECKGLERROR
		break;
	}
}

static void R_Shadow_MakeVSDCT(void)
{
	// maps to a 2x3 texture rectangle with normalized coordinates
	// +-
	// XX
	// YY
	// ZZ
	// stores abs(dir.xy), offset.xy/2.5
	unsigned char data[4*6] =
	{
		255, 0, 0x33, 0x33, // +X: <1, 0>, <0.5, 0.5>
		255, 0, 0x99, 0x33, // -X: <1, 0>, <1.5, 0.5>
		0, 255, 0x33, 0x99, // +Y: <0, 1>, <0.5, 1.5>
		0, 255, 0x99, 0x99, // -Y: <0, 1>, <1.5, 1.5>
		0,   0, 0x33, 0xFF, // +Z: <0, 0>, <0.5, 2.5>
		0,   0, 0x99, 0xFF, // -Z: <0, 0>, <1.5, 2.5>
	};
	r_shadow_shadowmapvsdcttexture = R_LoadTextureCubeMap(r_shadow_texturepool, "shadowmapvsdct", 1, data, TEXTYPE_RGBA, TEXF_FORCENEAREST | TEXF_CLAMP | TEXF_ALPHA, NULL);
}

void R_Shadow_RenderMode_ShadowMap(int side, int clear, int size)
{
	int status;
	int maxsize;
	float nearclip, farclip, bias;
	r_viewport_t viewport;
	GLuint fbo = 0;
	CHECKGLERROR
	maxsize = r_shadow_shadowmapmaxsize;
	nearclip = r_shadow_shadowmapping_nearclip.value / rsurface.rtlight->radius;
	farclip = 1.0f;
	bias = r_shadow_shadowmapping_bias.value * nearclip * (1024.0f / size);// * rsurface.rtlight->radius;
	r_shadow_shadowmap_parameters[2] = 0.5f + 0.5f * (farclip + nearclip) / (farclip - nearclip);
	r_shadow_shadowmap_parameters[3] = -nearclip * farclip / (farclip - nearclip) - 0.5f * bias;
	r_shadow_shadowmapside = side;
	r_shadow_shadowmapsize = size;
	if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAP2D)
	{
		r_shadow_shadowmap_parameters[0] = 0.5f * (size - r_shadow_shadowmapborder);
		r_shadow_shadowmap_parameters[1] = r_shadow_shadowmapvsdct ? 2.5f*size : size;
		R_Viewport_InitRectSideView(&viewport, &rsurface.rtlight->matrix_lighttoworld, side, size, r_shadow_shadowmapborder, nearclip, farclip, NULL);
		if (r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAP2D) goto init_done;

		// complex unrolled cube approach (more flexible)
		if (r_shadow_shadowmapvsdct && !r_shadow_shadowmapvsdcttexture)
			R_Shadow_MakeVSDCT();
		if (!r_shadow_shadowmap2dtexture)
		{
#if 1
			int w = maxsize*2, h = vid.support.arb_texture_non_power_of_two ? maxsize*3 : maxsize*4;
			r_shadow_shadowmap2dtexture = R_LoadTextureShadowMap2D(r_shadow_texturepool, "shadowmap", w, h, r_shadow_shadowmapdepthbits, r_shadow_shadowmapsampler);
			qglGenFramebuffersEXT(1, &r_shadow_fbo2d);CHECKGLERROR
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_fbo2d);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_shadowmap2dtexture), 0);CHECKGLERROR
            // render depth into the fbo, do not render color at all
			qglDrawBuffer(GL_NONE);CHECKGLERROR
			qglReadBuffer(GL_NONE);CHECKGLERROR
			status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT && (r_shadow_shadowmapping.integer || r_shadow_deferred.integer))
			{
				Con_Printf("R_Shadow_RenderMode_ShadowMap: glCheckFramebufferStatusEXT returned %i\n", status);
				Cvar_SetValueQuick(&r_shadow_shadowmapping, 0);
				Cvar_SetValueQuick(&r_shadow_deferred, 0);
			}
#endif
		}
		CHECKGLERROR
		if (r_shadow_shadowmap2dtexture) fbo = r_shadow_fbo2d;
		r_shadow_shadowmap_texturescale[0] = 1.0f / R_TextureWidth(r_shadow_shadowmap2dtexture);
		r_shadow_shadowmap_texturescale[1] = 1.0f / R_TextureHeight(r_shadow_shadowmap2dtexture);
		r_shadow_rendermode = R_SHADOW_RENDERMODE_SHADOWMAP2D;
	}
	else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE)
	{
		r_shadow_shadowmap_parameters[0] = 0.5f * (size - r_shadow_shadowmapborder);
		r_shadow_shadowmap_parameters[1] = r_shadow_shadowmapvsdct ? 2.5f*size : size;
		R_Viewport_InitRectSideView(&viewport, &rsurface.rtlight->matrix_lighttoworld, side, size, r_shadow_shadowmapborder, nearclip, farclip, NULL);
		if (r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPRECTANGLE) goto init_done;

		// complex unrolled cube approach (more flexible)
		if (r_shadow_shadowmapvsdct && !r_shadow_shadowmapvsdcttexture)
			R_Shadow_MakeVSDCT();
		if (!r_shadow_shadowmaprectangletexture)
		{
#if 1
			r_shadow_shadowmaprectangletexture = R_LoadTextureShadowMapRectangle(r_shadow_texturepool, "shadowmap", maxsize*2, maxsize*3, r_shadow_shadowmapdepthbits, r_shadow_shadowmapsampler);
			qglGenFramebuffersEXT(1, &r_shadow_fborectangle);CHECKGLERROR
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_fborectangle);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_RECTANGLE_ARB, R_GetTexture(r_shadow_shadowmaprectangletexture), 0);CHECKGLERROR
			// render depth into the fbo, do not render color at all
			qglDrawBuffer(GL_NONE);CHECKGLERROR
			qglReadBuffer(GL_NONE);CHECKGLERROR
			status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT && (r_shadow_shadowmapping.integer || r_shadow_deferred.integer))
			{
				Con_Printf("R_Shadow_RenderMode_ShadowMap: glCheckFramebufferStatusEXT returned %i\n", status);
				Cvar_SetValueQuick(&r_shadow_shadowmapping, 0);
				Cvar_SetValueQuick(&r_shadow_deferred, 0);
			}
#endif
		}
		CHECKGLERROR
		if(r_shadow_shadowmaprectangletexture) fbo = r_shadow_fborectangle;
		r_shadow_shadowmap_texturescale[0] = 1.0f;
		r_shadow_shadowmap_texturescale[1] = 1.0f;
		r_shadow_rendermode = R_SHADOW_RENDERMODE_SHADOWMAPRECTANGLE;
	}
	else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE)
	{
		r_shadow_shadowmap_parameters[0] = 1.0f;
		r_shadow_shadowmap_parameters[1] = 1.0f;
		R_Viewport_InitCubeSideView(&viewport, &rsurface.rtlight->matrix_lighttoworld, side, size, nearclip, farclip, NULL);
		if (r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPCUBESIDE) goto init_done;

		// simple cube approach
		if (!r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod])
		{
 #if 1
			r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod] = R_LoadTextureShadowMapCube(r_shadow_texturepool, "shadowmapcube", size, r_shadow_shadowmapdepthbits, r_shadow_shadowmapsampler);
			qglGenFramebuffersEXT(1, &r_shadow_fbocubeside[r_shadow_shadowmaplod]);CHECKGLERROR
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_fbocubeside[r_shadow_shadowmaplod]);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, R_GetTexture(r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]), 0);CHECKGLERROR
			// render depth into the fbo, do not render color at all
			qglDrawBuffer(GL_NONE);CHECKGLERROR
			qglReadBuffer(GL_NONE);CHECKGLERROR
			status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT && (r_shadow_shadowmapping.integer || r_shadow_deferred.integer))
			{
				Con_Printf("R_Shadow_RenderMode_ShadowMap: glCheckFramebufferStatusEXT returned %i\n", status);
				Cvar_SetValueQuick(&r_shadow_shadowmapping, 0);
				Cvar_SetValueQuick(&r_shadow_deferred, 0);
			}
 #endif
		}
		CHECKGLERROR
		if (r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]) fbo = r_shadow_fbocubeside[r_shadow_shadowmaplod];
		r_shadow_shadowmap_texturescale[0] = 0.0f;
		r_shadow_shadowmap_texturescale[1] = 0.0f;
		r_shadow_rendermode = R_SHADOW_RENDERMODE_SHADOWMAPCUBESIDE;
	}

	R_Shadow_RenderMode_Reset();
	if (fbo)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);CHECKGLERROR
		R_SetupShader_DepthOrShadow();
	}
	else
	{
		R_SetupShader_ShowDepth();
		qglClearColor(1,1,1,1);CHECKGLERROR
	}
	CHECKGLERROR
	GL_PolygonOffset(r_shadow_shadowmapping_polygonfactor.value, r_shadow_shadowmapping_polygonoffset.value);
	GL_DepthMask(true);
	GL_DepthTest(true);
	qglClearDepth(1);
	CHECKGLERROR

init_done:
	R_SetViewport(&viewport);
	if(r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAP2D || r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPRECTANGLE)
	{
		int flipped = (side & 1) ^ (side >> 2);
		r_refdef.view.cullface_front = flipped ? r_shadow_cullface_back : r_shadow_cullface_front;
		r_refdef.view.cullface_back = flipped ? r_shadow_cullface_front : r_shadow_cullface_back;
		GL_CullFace(r_refdef.view.cullface_back);
		if ((clear & ((2 << side) - 1)) == (1 << side)) // only clear if the side is the first in the mask
		{
			// get tightest scissor rectangle that encloses all viewports in the clear mask
			int x1 = clear & 0x15 ? 0 : size;
			int x2 = clear & 0x2A ? 2 * size : size;
			int y1 = clear & 0x03 ? 0 : (clear & 0xC ? size : 2 * size);
			int y2 = clear & 0x30 ? 3 * size : (clear & 0xC ? 2 * size : size);
			GL_Scissor(x1, y1, x2 - x1, y2 - y1);
			GL_Clear(GL_DEPTH_BUFFER_BIT);
		}
		GL_Scissor(viewport.x, viewport.y, viewport.width, viewport.height);
	}
	else if(r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPCUBESIDE)
	{
		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, R_GetTexture(r_shadow_shadowmapcubetexture[r_shadow_shadowmaplod]), 0);CHECKGLERROR
		GL_Scissor(viewport.x, viewport.y, viewport.width, viewport.height);
		if (clear)
			GL_Clear(GL_DEPTH_BUFFER_BIT);
	}
	CHECKGLERROR
}

void R_Shadow_RenderMode_Lighting(qboolean stenciltest, qboolean transparent, qboolean shadowmapping)
{
	if (transparent)
	{
		r_shadow_lightscissor[0] = r_refdef.view.viewport.x;
		r_shadow_lightscissor[1] = r_refdef.view.viewport.y;
		r_shadow_lightscissor[2] = r_refdef.view.viewport.width;
		r_shadow_lightscissor[3] = r_refdef.view.viewport.height;
	}
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	if (!transparent)
	{
		qglDepthFunc(GL_EQUAL);CHECKGLERROR
	}
	if (stenciltest)
	{
		qglEnable(GL_STENCIL_TEST);CHECKGLERROR
		// only draw light where this geometry was already rendered AND the
		// stencil is 128 (values other than this mean shadow)
		qglStencilFunc(GL_EQUAL, 128, 255);CHECKGLERROR
	}
	r_shadow_rendermode = r_shadow_lightingrendermode;
	// do global setup needed for the chosen lighting mode
	if (r_shadow_rendermode == R_SHADOW_RENDERMODE_LIGHT_GLSL)
	{
		GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 0);
		CHECKGLERROR
	}
	if (shadowmapping)
	{
		if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAP2D)
			r_shadow_usingshadowmap2d = true;
		else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE)
			r_shadow_usingshadowmaprect = true;
		else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE)
			r_shadow_usingshadowmapcube = true;
	}
	R_Mesh_ColorPointer(rsurface.array_color4f, 0, 0);
	CHECKGLERROR
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

static const float bboxpoints[8][3] =
{
	{-1,-1,-1},
	{ 1,-1,-1},
	{-1, 1,-1},
	{ 1, 1,-1},
	{-1,-1, 1},
	{ 1,-1, 1},
	{-1, 1, 1},
	{ 1, 1, 1},
};

void R_Shadow_RenderMode_DrawDeferredLight(qboolean stenciltest, qboolean shadowmapping)
{
	int i;
	float vertex3f[8*3];
	const matrix4x4_t *matrix = &rsurface.rtlight->matrix_lighttoworld;
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	r_shadow_rendermode = r_shadow_lightingrendermode;
	// do global setup needed for the chosen lighting mode
	{
		R_EntityMatrix(&identitymatrix);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		if (stenciltest)
		{
			qglEnable(GL_STENCIL_TEST);CHECKGLERROR
			// only draw light where this geometry was already rendered AND the
			// stencil is 128 (values other than this mean shadow)
			qglStencilFunc(GL_EQUAL, 128, 255);CHECKGLERROR
		}
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_prepasslightingfbo);CHECKGLERROR
		if (shadowmapping)
		{
			if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAP2D)
				r_shadow_usingshadowmap2d = true;
			else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE)
				r_shadow_usingshadowmaprect = true;
			else if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE)
				r_shadow_usingshadowmapcube = true;
		}

		// render the lighting
		R_SetupShader_DeferredLight(rsurface.rtlight);
		for (i = 0;i < 8;i++)
			Matrix4x4_Transform(matrix, bboxpoints[i], vertex3f + i*3);
		CHECKGLERROR
		R_Mesh_VertexPointer(vertex3f, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_ColorMask(1,1,1,1);
		GL_DepthMask(false);
		GL_DepthRange(0, 1);
		GL_PolygonOffset(0, 0);
		GL_DepthTest(true);
		qglDepthFunc(GL_GREATER);CHECKGLERROR
		GL_CullFace(r_refdef.view.cullface_back);
		R_Mesh_Draw(0, 8, 0, 12, NULL, bboxelements, 0, 0);
	}
}

void R_Shadow_RenderMode_VisibleShadowVolumes(void)
{
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthRange(0, 1);
	GL_DepthTest(r_showshadowvolumes.integer < 2);
	GL_Color(0.0, 0.0125 * r_refdef.view.colorscale, 0.1 * r_refdef.view.colorscale, 1);
	GL_PolygonOffset(r_refdef.shadowpolygonfactor, r_refdef.shadowpolygonoffset);CHECKGLERROR
	GL_CullFace(GL_NONE);
	r_shadow_rendermode = R_SHADOW_RENDERMODE_VISIBLEVOLUMES;
}

void R_Shadow_RenderMode_VisibleLighting(qboolean stenciltest, qboolean transparent)
{
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthRange(0, 1);
	GL_DepthTest(r_showlighting.integer < 2);
	GL_Color(0.1 * r_refdef.view.colorscale, 0.0125 * r_refdef.view.colorscale, 0, 1);
	if (!transparent)
	{
		qglDepthFunc(GL_EQUAL);CHECKGLERROR
	}
	if (stenciltest)
	{
		qglEnable(GL_STENCIL_TEST);CHECKGLERROR
		qglStencilFunc(GL_EQUAL, 128, 255);CHECKGLERROR
	}
	r_shadow_rendermode = R_SHADOW_RENDERMODE_VISIBLELIGHTING;
}

void R_Shadow_RenderMode_End(void)
{
	CHECKGLERROR
	R_Shadow_RenderMode_Reset();
	R_Shadow_RenderMode_ActiveLight(NULL);
	GL_DepthMask(true);
	GL_Scissor(r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
	r_shadow_rendermode = R_SHADOW_RENDERMODE_NONE;
}

int bboxedges[12][2] =
{
	// top
	{0, 1}, // +X
	{0, 2}, // +Y
	{1, 3}, // Y, +X
	{2, 3}, // X, +Y
	// bottom
	{4, 5}, // +X
	{4, 6}, // +Y
	{5, 7}, // Y, +X
	{6, 7}, // X, +Y
	// verticals
	{0, 4}, // +Z
	{1, 5}, // X, +Z
	{2, 6}, // Y, +Z
	{3, 7}, // XY, +Z
};

qboolean R_Shadow_ScissorForBBox(const float *mins, const float *maxs)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;
	vec4_t v, v2;
	float vertex[20][3];
	int j, k;
	vec4_t plane4f;
	int numvertices;
	float corner[8][4];
	float dist[8];
	int sign[8];
	float f;

	r_shadow_lightscissor[0] = r_refdef.view.viewport.x;
	r_shadow_lightscissor[1] = r_refdef.view.viewport.y;
	r_shadow_lightscissor[2] = r_refdef.view.viewport.width;
	r_shadow_lightscissor[3] = r_refdef.view.viewport.height;

	if (!r_shadow_scissor.integer)
		return false;

	// if view is inside the light box, just say yes it's visible
	if (BoxesOverlap(r_refdef.view.origin, r_refdef.view.origin, mins, maxs))
		return false;

	x1 = y1 = x2 = y2 = 0;

	// transform all corners that are infront of the nearclip plane
	VectorNegate(r_refdef.view.frustum[4].normal, plane4f);
	plane4f[3] = r_refdef.view.frustum[4].dist;
	numvertices = 0;
	for (i = 0;i < 8;i++)
	{
		Vector4Set(corner[i], (i & 1) ? maxs[0] : mins[0], (i & 2) ? maxs[1] : mins[1], (i & 4) ? maxs[2] : mins[2], 1);
		dist[i] = DotProduct4(corner[i], plane4f);
		sign[i] = dist[i] > 0;
		if (!sign[i])
		{
			VectorCopy(corner[i], vertex[numvertices]);
			numvertices++;
		}
	}
	// if some points are behind the nearclip, add clipped edge points to make
	// sure that the scissor boundary is complete
	if (numvertices > 0 && numvertices < 8)
	{
		// add clipped edge points
		for (i = 0;i < 12;i++)
		{
			j = bboxedges[i][0];
			k = bboxedges[i][1];
			if (sign[j] != sign[k])
			{
				f = dist[j] / (dist[j] - dist[k]);
				VectorLerp(corner[j], f, corner[k], vertex[numvertices]);
				numvertices++;
			}
		}
	}

	// if we have no points to check, the light is behind the view plane
	if (!numvertices)
		return true;

	// if we have some points to transform, check what screen area is covered
	x1 = y1 = x2 = y2 = 0;
	v[3] = 1.0f;
	//Con_Printf("%i vertices to transform...\n", numvertices);
	for (i = 0;i < numvertices;i++)
	{
		VectorCopy(vertex[i], v);
		R_Viewport_TransformToScreen(&r_refdef.view.viewport, v, v2);
		//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
		if (i)
		{
			if (x1 > v2[0]) x1 = v2[0];
			if (x2 < v2[0]) x2 = v2[0];
			if (y1 > v2[1]) y1 = v2[1];
			if (y2 < v2[1]) y2 = v2[1];
		}
		else
		{
			x1 = x2 = v2[0];
			y1 = y2 = v2[1];
		}
	}

	// now convert the scissor rectangle to integer screen coordinates
	ix1 = (int)(x1 - 1.0f);
	iy1 = vid.height - (int)(y2 - 1.0f);
	ix2 = (int)(x2 + 1.0f);
	iy2 = vid.height - (int)(y1 + 1.0f);
	//Con_Printf("%f %f %f %f\n", x1, y1, x2, y2);

	// clamp it to the screen
	if (ix1 < r_refdef.view.viewport.x) ix1 = r_refdef.view.viewport.x;
	if (iy1 < r_refdef.view.viewport.y) iy1 = r_refdef.view.viewport.y;
	if (ix2 > r_refdef.view.viewport.x + r_refdef.view.viewport.width) ix2 = r_refdef.view.viewport.x + r_refdef.view.viewport.width;
	if (iy2 > r_refdef.view.viewport.y + r_refdef.view.viewport.height) iy2 = r_refdef.view.viewport.y + r_refdef.view.viewport.height;

	// if it is inside out, it's not visible
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;

	// the light area is visible, set up the scissor rectangle
	r_shadow_lightscissor[0] = ix1;
	r_shadow_lightscissor[1] = iy1;
	r_shadow_lightscissor[2] = ix2 - ix1;
	r_shadow_lightscissor[3] = iy2 - iy1;

	r_refdef.stats.lights_scissored++;
	return false;
}

static void R_Shadow_RenderLighting_Light_Vertex_Shading(int firstvertex, int numverts, int numtriangles, const int *element3i, const float *diffusecolor, const float *ambientcolor)
{
	const float *vertex3f = rsurface.vertex3f + 3 * firstvertex;
	const float *normal3f = rsurface.normal3f + 3 * firstvertex;
	float *color4f = rsurface.array_color4f + 4 * firstvertex;
	float dist, dot, distintensity, shadeintensity, v[3], n[3];
	switch (r_shadow_rendermode)
	{
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX3DATTEN:
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2D1DATTEN:
		if (VectorLength2(diffusecolor) > 0)
		{
			for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
			{
				Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
				Matrix4x4_Transform3x3(&rsurface.entitytolight, normal3f, n);
				if ((dot = DotProduct(n, v)) < 0)
				{
					shadeintensity = -dot / sqrt(VectorLength2(v) * VectorLength2(n));
					VectorMA(ambientcolor, shadeintensity, diffusecolor, color4f);
				}
				else
					VectorCopy(ambientcolor, color4f);
				if (r_refdef.fogenabled)
				{
					float f;
					f = RSurf_FogVertex(vertex3f);
					VectorScale(color4f, f, color4f);
				}
				color4f[3] = 1;
			}
		}
		else
		{
			for (;numverts > 0;numverts--, vertex3f += 3, color4f += 4)
			{
				VectorCopy(ambientcolor, color4f);
				if (r_refdef.fogenabled)
				{
					float f;
					Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
					f = RSurf_FogVertex(vertex3f);
					VectorScale(color4f, f, color4f);
				}
				color4f[3] = 1;
			}
		}
		break;
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2DATTEN:
		if (VectorLength2(diffusecolor) > 0)
		{
			for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
			{
				Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
				if ((dist = fabs(v[2])) < 1 && (distintensity = r_shadow_attentable[(int)(dist * ATTENTABLESIZE)]))
				{
					Matrix4x4_Transform3x3(&rsurface.entitytolight, normal3f, n);
					if ((dot = DotProduct(n, v)) < 0)
					{
						shadeintensity = -dot / sqrt(VectorLength2(v) * VectorLength2(n));
						color4f[0] = (ambientcolor[0] + shadeintensity * diffusecolor[0]) * distintensity;
						color4f[1] = (ambientcolor[1] + shadeintensity * diffusecolor[1]) * distintensity;
						color4f[2] = (ambientcolor[2] + shadeintensity * diffusecolor[2]) * distintensity;
					}
					else
					{
						color4f[0] = ambientcolor[0] * distintensity;
						color4f[1] = ambientcolor[1] * distintensity;
						color4f[2] = ambientcolor[2] * distintensity;
					}
					if (r_refdef.fogenabled)
					{
						float f;
						f = RSurf_FogVertex(vertex3f);
						VectorScale(color4f, f, color4f);
					}
				}
				else
					VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		else
		{
			for (;numverts > 0;numverts--, vertex3f += 3, color4f += 4)
			{
				Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
				if ((dist = fabs(v[2])) < 1 && (distintensity = r_shadow_attentable[(int)(dist * ATTENTABLESIZE)]))
				{
					color4f[0] = ambientcolor[0] * distintensity;
					color4f[1] = ambientcolor[1] * distintensity;
					color4f[2] = ambientcolor[2] * distintensity;
					if (r_refdef.fogenabled)
					{
						float f;
						f = RSurf_FogVertex(vertex3f);
						VectorScale(color4f, f, color4f);
					}
				}
				else
					VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		break;
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX:
		if (VectorLength2(diffusecolor) > 0)
		{
			for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
			{
				Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
				if ((dist = VectorLength(v)) < 1 && (distintensity = r_shadow_attentable[(int)(dist * ATTENTABLESIZE)]))
				{
					distintensity = (1 - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist);
					Matrix4x4_Transform3x3(&rsurface.entitytolight, normal3f, n);
					if ((dot = DotProduct(n, v)) < 0)
					{
						shadeintensity = -dot / sqrt(VectorLength2(v) * VectorLength2(n));
						color4f[0] = (ambientcolor[0] + shadeintensity * diffusecolor[0]) * distintensity;
						color4f[1] = (ambientcolor[1] + shadeintensity * diffusecolor[1]) * distintensity;
						color4f[2] = (ambientcolor[2] + shadeintensity * diffusecolor[2]) * distintensity;
					}
					else
					{
						color4f[0] = ambientcolor[0] * distintensity;
						color4f[1] = ambientcolor[1] * distintensity;
						color4f[2] = ambientcolor[2] * distintensity;
					}
					if (r_refdef.fogenabled)
					{
						float f;
						f = RSurf_FogVertex(vertex3f);
						VectorScale(color4f, f, color4f);
					}
				}
				else
					VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		else
		{
			for (;numverts > 0;numverts--, vertex3f += 3, color4f += 4)
			{
				Matrix4x4_Transform(&rsurface.entitytolight, vertex3f, v);
				if ((dist = VectorLength(v)) < 1 && (distintensity = r_shadow_attentable[(int)(dist * ATTENTABLESIZE)]))
				{
					distintensity = (1 - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist);
					color4f[0] = ambientcolor[0] * distintensity;
					color4f[1] = ambientcolor[1] * distintensity;
					color4f[2] = ambientcolor[2] * distintensity;
					if (r_refdef.fogenabled)
					{
						float f;
						f = RSurf_FogVertex(vertex3f);
						VectorScale(color4f, f, color4f);
					}
				}
				else
					VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		break;
	default:
		break;
	}
}

static void R_Shadow_RenderLighting_VisibleLighting(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const unsigned short *element3s, int element3i_bufferobject, int element3s_bufferobject)
{
	// used to display how many times a surface is lit for level design purposes
	R_Mesh_Draw(firstvertex, numvertices, firsttriangle, numtriangles, element3i, element3s, element3i_bufferobject, element3s_bufferobject);
}

static void R_Shadow_RenderLighting_Light_GLSL(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const unsigned short *element3s, int element3i_bufferobject, int element3s_bufferobject, const vec3_t lightcolor, float ambientscale, float diffusescale, float specularscale)
{
	// ARB2 GLSL shader path (GFFX5200, Radeon 9500)
	R_SetupShader_Surface(lightcolor, false, ambientscale, diffusescale, specularscale, RSURFPASS_RTLIGHT);
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_EQUAL);CHECKGLERROR
	}
	R_Mesh_Draw(firstvertex, numvertices, firsttriangle, numtriangles, element3i, element3s, element3i_bufferobject, element3s_bufferobject);
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_LEQUAL);CHECKGLERROR
	}
}

static void R_Shadow_RenderLighting_Light_Vertex_Pass(int firstvertex, int numvertices, int numtriangles, const int *element3i, vec3_t diffusecolor2, vec3_t ambientcolor2)
{
	int renders;
	int i;
	int stop;
	int newfirstvertex;
	int newlastvertex;
	int newnumtriangles;
	int *newe;
	const int *e;
	float *c;
	int maxtriangles = 4096;
	static int newelements[4096*3];
	R_Shadow_RenderLighting_Light_Vertex_Shading(firstvertex, numvertices, numtriangles, element3i, diffusecolor2, ambientcolor2);
	for (renders = 0;renders < 4;renders++)
	{
		stop = true;
		newfirstvertex = 0;
		newlastvertex = 0;
		newnumtriangles = 0;
		newe = newelements;
		// due to low fillrate on the cards this vertex lighting path is
		// designed for, we manually cull all triangles that do not
		// contain a lit vertex
		// this builds batches of triangles from multiple surfaces and
		// renders them at once
		for (i = 0, e = element3i;i < numtriangles;i++, e += 3)
		{
			if (VectorLength2(rsurface.array_color4f + e[0] * 4) + VectorLength2(rsurface.array_color4f + e[1] * 4) + VectorLength2(rsurface.array_color4f + e[2] * 4) >= 0.01)
			{
				if (newnumtriangles)
				{
					newfirstvertex = min(newfirstvertex, e[0]);
					newlastvertex  = max(newlastvertex, e[0]);
				}
				else
				{
					newfirstvertex = e[0];
					newlastvertex = e[0];
				}
				newfirstvertex = min(newfirstvertex, e[1]);
				newlastvertex  = max(newlastvertex, e[1]);
				newfirstvertex = min(newfirstvertex, e[2]);
				newlastvertex  = max(newlastvertex, e[2]);
				newe[0] = e[0];
				newe[1] = e[1];
				newe[2] = e[2];
				newnumtriangles++;
				newe += 3;
				if (newnumtriangles >= maxtriangles)
				{
					R_Mesh_Draw(newfirstvertex, newlastvertex - newfirstvertex + 1, 0, newnumtriangles, newelements, NULL, 0, 0);
					newnumtriangles = 0;
					newe = newelements;
					stop = false;
				}
			}
		}
		if (newnumtriangles >= 1)
		{
			R_Mesh_Draw(newfirstvertex, newlastvertex - newfirstvertex + 1, 0, newnumtriangles, newelements, NULL, 0, 0);
			stop = false;
		}
		// if we couldn't find any lit triangles, exit early
		if (stop)
			break;
		// now reduce the intensity for the next overbright pass
		// we have to clamp to 0 here incase the drivers have improper
		// handling of negative colors
		// (some old drivers even have improper handling of >1 color)
		stop = true;
		for (i = 0, c = rsurface.array_color4f + 4 * firstvertex;i < numvertices;i++, c += 4)
		{
			if (c[0] > 1 || c[1] > 1 || c[2] > 1)
			{
				c[0] = max(0, c[0] - 1);
				c[1] = max(0, c[1] - 1);
				c[2] = max(0, c[2] - 1);
				stop = false;
			}
			else
				VectorClear(c);
		}
		// another check...
		if (stop)
			break;
	}
}

static void R_Shadow_RenderLighting_Light_Vertex(int firstvertex, int numvertices, int numtriangles, const int *element3i, const vec3_t lightcolor, float ambientscale, float diffusescale)
{
	// OpenGL 1.1 path (anything)
	float ambientcolorbase[3], diffusecolorbase[3];
	float ambientcolorpants[3], diffusecolorpants[3];
	float ambientcolorshirt[3], diffusecolorshirt[3];
	const float *surfacecolor = rsurface.texture->dlightcolor;
	const float *surfacepants = rsurface.colormap_pantscolor;
	const float *surfaceshirt = rsurface.colormap_shirtcolor;
	rtexture_t *basetexture = rsurface.texture->basetexture;
	rtexture_t *pantstexture = rsurface.texture->pantstexture;
	rtexture_t *shirttexture = rsurface.texture->shirttexture;
	qboolean dopants = pantstexture && VectorLength2(surfacepants) >= (1.0f / 1048576.0f);
	qboolean doshirt = shirttexture && VectorLength2(surfaceshirt) >= (1.0f / 1048576.0f);
	ambientscale *= 2 * r_refdef.view.colorscale;
	diffusescale *= 2 * r_refdef.view.colorscale;
	ambientcolorbase[0] = lightcolor[0] * ambientscale * surfacecolor[0];ambientcolorbase[1] = lightcolor[1] * ambientscale * surfacecolor[1];ambientcolorbase[2] = lightcolor[2] * ambientscale * surfacecolor[2];
	diffusecolorbase[0] = lightcolor[0] * diffusescale * surfacecolor[0];diffusecolorbase[1] = lightcolor[1] * diffusescale * surfacecolor[1];diffusecolorbase[2] = lightcolor[2] * diffusescale * surfacecolor[2];
	ambientcolorpants[0] = ambientcolorbase[0] * surfacepants[0];ambientcolorpants[1] = ambientcolorbase[1] * surfacepants[1];ambientcolorpants[2] = ambientcolorbase[2] * surfacepants[2];
	diffusecolorpants[0] = diffusecolorbase[0] * surfacepants[0];diffusecolorpants[1] = diffusecolorbase[1] * surfacepants[1];diffusecolorpants[2] = diffusecolorbase[2] * surfacepants[2];
	ambientcolorshirt[0] = ambientcolorbase[0] * surfaceshirt[0];ambientcolorshirt[1] = ambientcolorbase[1] * surfaceshirt[1];ambientcolorshirt[2] = ambientcolorbase[2] * surfaceshirt[2];
	diffusecolorshirt[0] = diffusecolorbase[0] * surfaceshirt[0];diffusecolorshirt[1] = diffusecolorbase[1] * surfaceshirt[1];diffusecolorshirt[2] = diffusecolorbase[2] * surfaceshirt[2];
	R_Mesh_TexBind(0, basetexture);
	R_Mesh_TexMatrix(0, &rsurface.texture->currenttexmatrix);
	R_Mesh_TexCombine(0, GL_MODULATE, GL_MODULATE, 1, 1);
	R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
	switch(r_shadow_rendermode)
	{
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX3DATTEN:
		R_Mesh_TexBind(1, r_shadow_attenuation3dtexture);
		R_Mesh_TexMatrix(1, &rsurface.entitytoattenuationxyz);
		R_Mesh_TexCombine(1, GL_MODULATE, GL_MODULATE, 1, 1);
		R_Mesh_TexCoordPointer(1, 3, rsurface.vertex3f, rsurface.vertex3f_bufferobject, rsurface.vertex3f_bufferoffset);
		break;
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2D1DATTEN:
		R_Mesh_TexBind(2, r_shadow_attenuation2dtexture);
		R_Mesh_TexMatrix(2, &rsurface.entitytoattenuationz);
		R_Mesh_TexCombine(2, GL_MODULATE, GL_MODULATE, 1, 1);
		R_Mesh_TexCoordPointer(2, 3, rsurface.vertex3f, rsurface.vertex3f_bufferobject, rsurface.vertex3f_bufferoffset);
		// fall through
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2DATTEN:
		R_Mesh_TexBind(1, r_shadow_attenuation2dtexture);
		R_Mesh_TexMatrix(1, &rsurface.entitytoattenuationxyz);
		R_Mesh_TexCombine(1, GL_MODULATE, GL_MODULATE, 1, 1);
		R_Mesh_TexCoordPointer(1, 3, rsurface.vertex3f, rsurface.vertex3f_bufferobject, rsurface.vertex3f_bufferoffset);
		break;
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX:
		break;
	default:
		break;
	}
	//R_Mesh_TexBind(0, basetexture);
	R_Shadow_RenderLighting_Light_Vertex_Pass(firstvertex, numvertices, numtriangles, element3i, diffusecolorbase, ambientcolorbase);
	if (dopants)
	{
		R_Mesh_TexBind(0, pantstexture);
		R_Shadow_RenderLighting_Light_Vertex_Pass(firstvertex, numvertices, numtriangles, element3i, diffusecolorpants, ambientcolorpants);
	}
	if (doshirt)
	{
		R_Mesh_TexBind(0, shirttexture);
		R_Shadow_RenderLighting_Light_Vertex_Pass(firstvertex, numvertices, numtriangles, element3i, diffusecolorshirt, ambientcolorshirt);
	}
}

extern cvar_t gl_lightmaps;
void R_Shadow_RenderLighting(int firstvertex, int numvertices, int firsttriangle, int numtriangles, const int *element3i, const unsigned short *element3s, int element3i_bufferobject, int element3s_bufferobject)
{
	float ambientscale, diffusescale, specularscale;
	qboolean negated;
	float lightcolor[3];
	VectorCopy(rsurface.rtlight->currentcolor, lightcolor);
	ambientscale = rsurface.rtlight->ambientscale;
	diffusescale = rsurface.rtlight->diffusescale;
	specularscale = rsurface.rtlight->specularscale * rsurface.texture->specularscale;
	if (!r_shadow_usenormalmap.integer)
	{
		ambientscale += 1.0f * diffusescale;
		diffusescale = 0;
		specularscale = 0;
	}
	if ((ambientscale + diffusescale) * VectorLength2(lightcolor) + specularscale * VectorLength2(lightcolor) < (1.0f / 1048576.0f))
		return;
	negated = (lightcolor[0] + lightcolor[1] + lightcolor[2] < 0) && vid.support.ext_blend_subtract;
	if(negated)
	{
		VectorNegate(lightcolor, lightcolor);
		qglBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT);
	}
	RSurf_SetupDepthAndCulling();
	switch (r_shadow_rendermode)
	{
	case R_SHADOW_RENDERMODE_VISIBLELIGHTING:
		GL_DepthTest(!(rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST) && !r_showdisabledepthtest.integer);
		R_Shadow_RenderLighting_VisibleLighting(firstvertex, numvertices, firsttriangle, numtriangles, element3i, element3s, element3i_bufferobject, element3s_bufferobject);
		break;
	case R_SHADOW_RENDERMODE_LIGHT_GLSL:
		R_Shadow_RenderLighting_Light_GLSL(firstvertex, numvertices, firsttriangle, numtriangles, element3i, element3s, element3i_bufferobject, element3s_bufferobject, lightcolor, ambientscale, diffusescale, specularscale);
		break;
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX3DATTEN:
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2D1DATTEN:
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX2DATTEN:
	case R_SHADOW_RENDERMODE_LIGHT_VERTEX:
		R_Shadow_RenderLighting_Light_Vertex(firstvertex, numvertices, numtriangles, element3i + firsttriangle * 3, lightcolor, ambientscale, diffusescale);
		break;
	default:
		Con_Printf("R_Shadow_RenderLighting: unknown r_shadow_rendermode %i\n", r_shadow_rendermode);
		break;
	}
	if(negated)
		qglBlendEquationEXT(GL_FUNC_ADD_EXT);
}

void R_RTLight_Update(rtlight_t *rtlight, int isstatic, matrix4x4_t *matrix, vec3_t color, int style, const char *cubemapname, int shadow, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
	matrix4x4_t tempmatrix = *matrix;
	Matrix4x4_Scale(&tempmatrix, r_shadow_lightradiusscale.value, 1);

	// if this light has been compiled before, free the associated data
	R_RTLight_Uncompile(rtlight);

	// clear it completely to avoid any lingering data
	memset(rtlight, 0, sizeof(*rtlight));

	// copy the properties
	rtlight->matrix_lighttoworld = tempmatrix;
	Matrix4x4_Invert_Simple(&rtlight->matrix_worldtolight, &tempmatrix);
	Matrix4x4_OriginFromMatrix(&tempmatrix, rtlight->shadoworigin);
	rtlight->radius = Matrix4x4_ScaleFromMatrix(&tempmatrix);
	VectorCopy(color, rtlight->color);
	rtlight->cubemapname[0] = 0;
	if (cubemapname && cubemapname[0])
		strlcpy(rtlight->cubemapname, cubemapname, sizeof(rtlight->cubemapname));
	rtlight->shadow = shadow;
	rtlight->corona = corona;
	rtlight->style = style;
	rtlight->isstatic = isstatic;
	rtlight->coronasizescale = coronasizescale;
	rtlight->ambientscale = ambientscale;
	rtlight->diffusescale = diffusescale;
	rtlight->specularscale = specularscale;
	rtlight->flags = flags;

	// compute derived data
	//rtlight->cullradius = rtlight->radius;
	//rtlight->cullradius2 = rtlight->radius * rtlight->radius;
	rtlight->cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	rtlight->cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	rtlight->cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	rtlight->cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	rtlight->cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	rtlight->cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;
}

// compiles rtlight geometry
// (undone by R_FreeCompiledRTLight, which R_UpdateLight calls)
void R_RTLight_Compile(rtlight_t *rtlight)
{
	int i;
	int numsurfaces, numleafs, numleafpvsbytes, numshadowtrispvsbytes, numlighttrispvsbytes;
	int lighttris, shadowtris, shadowzpasstris, shadowzfailtris;
	entity_render_t *ent = r_refdef.scene.worldentity;
	dp_model_t *model = r_refdef.scene.worldmodel;
	unsigned char *data;
	shadowmesh_t *mesh;

	// compile the light
	rtlight->compiled = true;
	rtlight->shadowmode = rtlight->shadow ? (int)r_shadow_shadowmode : -1;
	rtlight->static_numleafs = 0;
	rtlight->static_numleafpvsbytes = 0;
	rtlight->static_leaflist = NULL;
	rtlight->static_leafpvs = NULL;
	rtlight->static_numsurfaces = 0;
	rtlight->static_surfacelist = NULL;
	rtlight->static_shadowmap_receivers = 0x3F;
	rtlight->static_shadowmap_casters = 0x3F;
	rtlight->cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	rtlight->cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	rtlight->cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	rtlight->cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	rtlight->cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	rtlight->cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;

	if (model && model->GetLightInfo)
	{
		// this variable must be set for the CompileShadowVolume/CompileShadowMap code
		r_shadow_compilingrtlight = rtlight;
		model->GetLightInfo(ent, rtlight->shadoworigin, rtlight->radius, rtlight->cullmins, rtlight->cullmaxs, r_shadow_buffer_leaflist, r_shadow_buffer_leafpvs, &numleafs, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces, r_shadow_buffer_shadowtrispvs, r_shadow_buffer_lighttrispvs, r_shadow_buffer_visitingleafpvs, 0, NULL);
		numleafpvsbytes = (model->brush.num_leafs + 7) >> 3;
		numshadowtrispvsbytes = ((model->brush.shadowmesh ? model->brush.shadowmesh->numtriangles : model->surfmesh.num_triangles) + 7) >> 3;
		numlighttrispvsbytes = (model->surfmesh.num_triangles + 7) >> 3;
		data = (unsigned char *)Mem_Alloc(r_main_mempool, sizeof(int) * numsurfaces + sizeof(int) * numleafs + numleafpvsbytes + numshadowtrispvsbytes + numlighttrispvsbytes);
		rtlight->static_numsurfaces = numsurfaces;
		rtlight->static_surfacelist = (int *)data;data += sizeof(int) * numsurfaces;
		rtlight->static_numleafs = numleafs;
		rtlight->static_leaflist = (int *)data;data += sizeof(int) * numleafs;
		rtlight->static_numleafpvsbytes = numleafpvsbytes;
		rtlight->static_leafpvs = (unsigned char *)data;data += numleafpvsbytes;
		rtlight->static_numshadowtrispvsbytes = numshadowtrispvsbytes;
		rtlight->static_shadowtrispvs = (unsigned char *)data;data += numshadowtrispvsbytes;
		rtlight->static_numlighttrispvsbytes = numlighttrispvsbytes;
		rtlight->static_lighttrispvs = (unsigned char *)data;data += numlighttrispvsbytes;
		if (rtlight->static_numsurfaces)
			memcpy(rtlight->static_surfacelist, r_shadow_buffer_surfacelist, rtlight->static_numsurfaces * sizeof(*rtlight->static_surfacelist));
		if (rtlight->static_numleafs)
			memcpy(rtlight->static_leaflist, r_shadow_buffer_leaflist, rtlight->static_numleafs * sizeof(*rtlight->static_leaflist));
		if (rtlight->static_numleafpvsbytes)
			memcpy(rtlight->static_leafpvs, r_shadow_buffer_leafpvs, rtlight->static_numleafpvsbytes);
		if (rtlight->static_numshadowtrispvsbytes)
			memcpy(rtlight->static_shadowtrispvs, r_shadow_buffer_shadowtrispvs, rtlight->static_numshadowtrispvsbytes);
		if (rtlight->static_numlighttrispvsbytes)
			memcpy(rtlight->static_lighttrispvs, r_shadow_buffer_lighttrispvs, rtlight->static_numlighttrispvsbytes);
		switch (rtlight->shadowmode)
		{
		case R_SHADOW_SHADOWMODE_SHADOWMAP2D:
		case R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE:
		case R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE:
			if (model->CompileShadowMap && rtlight->shadow)
				model->CompileShadowMap(ent, rtlight->shadoworigin, NULL, rtlight->radius, numsurfaces, r_shadow_buffer_surfacelist);
			break;
		default:
			if (model->CompileShadowVolume && rtlight->shadow)
				model->CompileShadowVolume(ent, rtlight->shadoworigin, NULL, rtlight->radius, numsurfaces, r_shadow_buffer_surfacelist);
			break;
		}
		// now we're done compiling the rtlight
		r_shadow_compilingrtlight = NULL;
	}


	// use smallest available cullradius - box radius or light radius
	//rtlight->cullradius = RadiusFromBoundsAndOrigin(rtlight->cullmins, rtlight->cullmaxs, rtlight->shadoworigin);
	//rtlight->cullradius = min(rtlight->cullradius, rtlight->radius);

	shadowzpasstris = 0;
	if (rtlight->static_meshchain_shadow_zpass)
		for (mesh = rtlight->static_meshchain_shadow_zpass;mesh;mesh = mesh->next)
			shadowzpasstris += mesh->numtriangles;

	shadowzfailtris = 0;
	if (rtlight->static_meshchain_shadow_zfail)
		for (mesh = rtlight->static_meshchain_shadow_zfail;mesh;mesh = mesh->next)
			shadowzfailtris += mesh->numtriangles;

	lighttris = 0;
	if (rtlight->static_numlighttrispvsbytes)
		for (i = 0;i < rtlight->static_numlighttrispvsbytes*8;i++)
			if (CHECKPVSBIT(rtlight->static_lighttrispvs, i))
				lighttris++;

	shadowtris = 0;
	if (rtlight->static_numlighttrispvsbytes)
		for (i = 0;i < rtlight->static_numshadowtrispvsbytes*8;i++)
			if (CHECKPVSBIT(rtlight->static_shadowtrispvs, i))
				shadowtris++;

	if (developer_extra.integer)
		Con_DPrintf("static light built: %f %f %f : %f %f %f box, %i light triangles, %i shadow triangles, %i zpass/%i zfail compiled shadow volume triangles\n", rtlight->cullmins[0], rtlight->cullmins[1], rtlight->cullmins[2], rtlight->cullmaxs[0], rtlight->cullmaxs[1], rtlight->cullmaxs[2], lighttris, shadowtris, shadowzpasstris, shadowzfailtris);
}

void R_RTLight_Uncompile(rtlight_t *rtlight)
{
	if (rtlight->compiled)
	{
		if (rtlight->static_meshchain_shadow_zpass)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_shadow_zpass);
		rtlight->static_meshchain_shadow_zpass = NULL;
		if (rtlight->static_meshchain_shadow_zfail)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_shadow_zfail);
		rtlight->static_meshchain_shadow_zfail = NULL;
		if (rtlight->static_meshchain_shadow_shadowmap)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_shadow_shadowmap);
		rtlight->static_meshchain_shadow_shadowmap = NULL;
		// these allocations are grouped
		if (rtlight->static_surfacelist)
			Mem_Free(rtlight->static_surfacelist);
		rtlight->static_numleafs = 0;
		rtlight->static_numleafpvsbytes = 0;
		rtlight->static_leaflist = NULL;
		rtlight->static_leafpvs = NULL;
		rtlight->static_numsurfaces = 0;
		rtlight->static_surfacelist = NULL;
		rtlight->static_numshadowtrispvsbytes = 0;
		rtlight->static_shadowtrispvs = NULL;
		rtlight->static_numlighttrispvsbytes = 0;
		rtlight->static_lighttrispvs = NULL;
		rtlight->compiled = false;
	}
}

void R_Shadow_UncompileWorldLights(void)
{
	size_t lightindex;
	dlight_t *light;
	size_t range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		R_RTLight_Uncompile(&light->rtlight);
	}
}

void R_Shadow_ComputeShadowCasterCullingPlanes(rtlight_t *rtlight)
{
	int i, j;
	mplane_t plane;
	// reset the count of frustum planes
	// see rtlight->cached_frustumplanes definition for how much this array
	// can hold
	rtlight->cached_numfrustumplanes = 0;

	// haven't implemented a culling path for ortho rendering
	if (!r_refdef.view.useperspective)
	{
		// check if the light is on screen and copy the 4 planes if it is
		for (i = 0;i < 4;i++)
			if (PlaneDiff(rtlight->shadoworigin, &r_refdef.view.frustum[i]) < -0.03125)
				break;
		if (i == 4)
			for (i = 0;i < 4;i++)
				rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = r_refdef.view.frustum[i];
		return;
	}

#if 1
	// generate a deformed frustum that includes the light origin, this is
	// used to cull shadow casting surfaces that can not possibly cast a
	// shadow onto the visible light-receiving surfaces, which can be a
	// performance gain
	//
	// if the light origin is onscreen the result will be 4 planes exactly
	// if the light origin is offscreen on only one axis the result will
	// be exactly 5 planes (split-side case)
	// if the light origin is offscreen on two axes the result will be
	// exactly 4 planes (stretched corner case)
	for (i = 0;i < 4;i++)
	{
		// quickly reject standard frustum planes that put the light
		// origin outside the frustum
		if (PlaneDiff(rtlight->shadoworigin, &r_refdef.view.frustum[i]) < -0.03125)
			continue;
		// copy the plane
		rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = r_refdef.view.frustum[i];
	}
	// if all the standard frustum planes were accepted, the light is onscreen
	// otherwise we need to generate some more planes below...
	if (rtlight->cached_numfrustumplanes < 4)
	{
		// at least one of the stock frustum planes failed, so we need to
		// create one or two custom planes to enclose the light origin
		for (i = 0;i < 4;i++)
		{
			// create a plane using the view origin and light origin, and a
			// single point from the frustum corner set
			TriangleNormal(r_refdef.view.origin, r_refdef.view.frustumcorner[i], rtlight->shadoworigin, plane.normal);
			VectorNormalize(plane.normal);
			plane.dist = DotProduct(r_refdef.view.origin, plane.normal);
			// see if this plane is backwards and flip it if so
			for (j = 0;j < 4;j++)
				if (j != i && DotProduct(r_refdef.view.frustumcorner[j], plane.normal) - plane.dist < -0.03125)
					break;
			if (j < 4)
			{
				VectorNegate(plane.normal, plane.normal);
				plane.dist *= -1;
				// flipped plane, test again to see if it is now valid
				for (j = 0;j < 4;j++)
					if (j != i && DotProduct(r_refdef.view.frustumcorner[j], plane.normal) - plane.dist < -0.03125)
						break;
				// if the plane is still not valid, then it is dividing the
				// frustum and has to be rejected
				if (j < 4)
					continue;
			}
			// we have created a valid plane, compute extra info
			PlaneClassify(&plane);
			// copy the plane
			rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = plane;
#if 1
			// if we've found 5 frustum planes then we have constructed a
			// proper split-side case and do not need to keep searching for
			// planes to enclose the light origin
			if (rtlight->cached_numfrustumplanes == 5)
				break;
#endif
		}
	}
#endif

#if 0
	for (i = 0;i < rtlight->cached_numfrustumplanes;i++)
	{
		plane = rtlight->cached_frustumplanes[i];
		Con_Printf("light %p plane #%i %f %f %f : %f (%f %f %f %f %f)\n", rtlight, i, plane.normal[0], plane.normal[1], plane.normal[2], plane.dist, PlaneDiff(r_refdef.view.frustumcorner[0], &plane), PlaneDiff(r_refdef.view.frustumcorner[1], &plane), PlaneDiff(r_refdef.view.frustumcorner[2], &plane), PlaneDiff(r_refdef.view.frustumcorner[3], &plane), PlaneDiff(rtlight->shadoworigin, &plane));
	}
#endif

#if 0
	// now add the light-space box planes if the light box is rotated, as any
	// caster outside the oriented light box is irrelevant (even if it passed
	// the worldspace light box, which is axial)
	if (rtlight->matrix_lighttoworld.m[0][0] != 1 || rtlight->matrix_lighttoworld.m[1][1] != 1 || rtlight->matrix_lighttoworld.m[2][2] != 1)
	{
		for (i = 0;i < 6;i++)
		{
			vec3_t v;
			VectorClear(v);
			v[i >> 1] = (i & 1) ? -1 : 1;
			Matrix4x4_Transform(&rtlight->matrix_lighttoworld, v, plane.normal);
			VectorSubtract(plane.normal, rtlight->shadoworigin, plane.normal);
			plane.dist = VectorNormalizeLength(plane.normal);
			plane.dist += DotProduct(plane.normal, rtlight->shadoworigin);
			rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = plane;
		}
	}
#endif

#if 0
	// add the world-space reduced box planes
	for (i = 0;i < 6;i++)
	{
		VectorClear(plane.normal);
		plane.normal[i >> 1] = (i & 1) ? -1 : 1;
		plane.dist = (i & 1) ? -rtlight->cached_cullmaxs[i >> 1] : rtlight->cached_cullmins[i >> 1];
		rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = plane;
	}
#endif

#if 0
	{
	int j, oldnum;
	vec3_t points[8];
	vec_t bestdist;
	// reduce all plane distances to tightly fit the rtlight cull box, which
	// is in worldspace
	VectorSet(points[0], rtlight->cached_cullmins[0], rtlight->cached_cullmins[1], rtlight->cached_cullmins[2]);
	VectorSet(points[1], rtlight->cached_cullmaxs[0], rtlight->cached_cullmins[1], rtlight->cached_cullmins[2]);
	VectorSet(points[2], rtlight->cached_cullmins[0], rtlight->cached_cullmaxs[1], rtlight->cached_cullmins[2]);
	VectorSet(points[3], rtlight->cached_cullmaxs[0], rtlight->cached_cullmaxs[1], rtlight->cached_cullmins[2]);
	VectorSet(points[4], rtlight->cached_cullmins[0], rtlight->cached_cullmins[1], rtlight->cached_cullmaxs[2]);
	VectorSet(points[5], rtlight->cached_cullmaxs[0], rtlight->cached_cullmins[1], rtlight->cached_cullmaxs[2]);
	VectorSet(points[6], rtlight->cached_cullmins[0], rtlight->cached_cullmaxs[1], rtlight->cached_cullmaxs[2]);
	VectorSet(points[7], rtlight->cached_cullmaxs[0], rtlight->cached_cullmaxs[1], rtlight->cached_cullmaxs[2]);
	oldnum = rtlight->cached_numfrustumplanes;
	rtlight->cached_numfrustumplanes = 0;
	for (j = 0;j < oldnum;j++)
	{
		// find the nearest point on the box to this plane
		bestdist = DotProduct(rtlight->cached_frustumplanes[j].normal, points[0]);
		for (i = 1;i < 8;i++)
		{
			dist = DotProduct(rtlight->cached_frustumplanes[j].normal, points[i]);
			if (bestdist > dist)
				bestdist = dist;
		}
		Con_Printf("light %p %splane #%i %f %f %f : %f < %f\n", rtlight, rtlight->cached_frustumplanes[j].dist < bestdist + 0.03125 ? "^2" : "^1", j, rtlight->cached_frustumplanes[j].normal[0], rtlight->cached_frustumplanes[j].normal[1], rtlight->cached_frustumplanes[j].normal[2], rtlight->cached_frustumplanes[j].dist, bestdist);
		// if the nearest point is near or behind the plane, we want this
		// plane, otherwise the plane is useless as it won't cull anything
		if (rtlight->cached_frustumplanes[j].dist < bestdist + 0.03125)
		{
			PlaneClassify(&rtlight->cached_frustumplanes[j]);
			rtlight->cached_frustumplanes[rtlight->cached_numfrustumplanes++] = rtlight->cached_frustumplanes[j];
		}
	}
	}
#endif
}

void R_Shadow_DrawWorldShadow_ShadowMap(int numsurfaces, int *surfacelist, const unsigned char *trispvs, const unsigned char *surfacesides)
{
	shadowmesh_t *mesh;

	RSurf_ActiveWorldEntity();

	if (rsurface.rtlight->compiled && r_shadow_realtime_world_compile.integer && r_shadow_realtime_world_compileshadow.integer)
	{
		CHECKGLERROR
		GL_CullFace(GL_NONE);
        mesh = rsurface.rtlight->static_meshchain_shadow_shadowmap;
        for (;mesh;mesh = mesh->next)
        {
			if (!mesh->sidetotals[r_shadow_shadowmapside])
				continue;
            r_refdef.stats.lights_shadowtriangles += mesh->sidetotals[r_shadow_shadowmapside];
            R_Mesh_VertexPointer(mesh->vertex3f, mesh->vbo, mesh->vbooffset_vertex3f);
            R_Mesh_Draw(0, mesh->numverts, mesh->sideoffsets[r_shadow_shadowmapside], mesh->sidetotals[r_shadow_shadowmapside], mesh->element3i, mesh->element3s, mesh->ebo3i, mesh->ebo3s);
        }
        CHECKGLERROR
    }
	else if (r_refdef.scene.worldentity->model)
		r_refdef.scene.worldmodel->DrawShadowMap(r_shadow_shadowmapside, r_refdef.scene.worldentity, rsurface.rtlight->shadoworigin, NULL, rsurface.rtlight->radius, numsurfaces, surfacelist, surfacesides, rsurface.rtlight->cached_cullmins, rsurface.rtlight->cached_cullmaxs);

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Shadow_DrawWorldShadow_ShadowVolume(int numsurfaces, int *surfacelist, const unsigned char *trispvs)
{
	qboolean zpass = false;
	shadowmesh_t *mesh;
	int t, tend;
	int surfacelistindex;
	msurface_t *surface;

	RSurf_ActiveWorldEntity();

	if (rsurface.rtlight->compiled && r_shadow_realtime_world_compile.integer && r_shadow_realtime_world_compileshadow.integer)
	{
		CHECKGLERROR
		if (r_shadow_rendermode != R_SHADOW_RENDERMODE_VISIBLEVOLUMES)
		{
			zpass = R_Shadow_UseZPass(r_refdef.scene.worldmodel->normalmins, r_refdef.scene.worldmodel->normalmaxs);
			R_Shadow_RenderMode_StencilShadowVolumes(zpass);
		}
		mesh = zpass ? rsurface.rtlight->static_meshchain_shadow_zpass : rsurface.rtlight->static_meshchain_shadow_zfail;
		for (;mesh;mesh = mesh->next)
		{
			r_refdef.stats.lights_shadowtriangles += mesh->numtriangles;
			R_Mesh_VertexPointer(mesh->vertex3f, mesh->vbo, mesh->vbooffset_vertex3f);
			if (r_shadow_rendermode == R_SHADOW_RENDERMODE_ZPASS_STENCIL)
			{
				// increment stencil if frontface is infront of depthbuffer
				GL_CullFace(r_refdef.view.cullface_back);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);CHECKGLERROR
				R_Mesh_Draw(0, mesh->numverts, 0, mesh->numtriangles, mesh->element3i, mesh->element3s, mesh->ebo3i, mesh->ebo3s);
				// decrement stencil if backface is infront of depthbuffer
				GL_CullFace(r_refdef.view.cullface_front);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);CHECKGLERROR
			}
			else if (r_shadow_rendermode == R_SHADOW_RENDERMODE_ZFAIL_STENCIL)
			{
				// decrement stencil if backface is behind depthbuffer
				GL_CullFace(r_refdef.view.cullface_front);
				qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);CHECKGLERROR
				R_Mesh_Draw(0, mesh->numverts, 0, mesh->numtriangles, mesh->element3i, mesh->element3s, mesh->ebo3i, mesh->ebo3s);
				// increment stencil if frontface is behind depthbuffer
				GL_CullFace(r_refdef.view.cullface_back);
				qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);CHECKGLERROR
			}
			R_Mesh_Draw(0, mesh->numverts, 0, mesh->numtriangles, mesh->element3i, mesh->element3s, mesh->ebo3i, mesh->ebo3s);
		}
		CHECKGLERROR
	}
	else if (numsurfaces && r_refdef.scene.worldmodel->brush.shadowmesh)
	{
		// use the shadow trispvs calculated earlier by GetLightInfo to cull world triangles on this dynamic light
		R_Shadow_PrepareShadowMark(r_refdef.scene.worldmodel->brush.shadowmesh->numtriangles);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = r_refdef.scene.worldmodel->data_surfaces + surfacelist[surfacelistindex];
			for (t = surface->num_firstshadowmeshtriangle, tend = t + surface->num_triangles;t < tend;t++)
				if (CHECKPVSBIT(trispvs, t))
					shadowmarklist[numshadowmark++] = t;
		}
		R_Shadow_VolumeFromList(r_refdef.scene.worldmodel->brush.shadowmesh->numverts, r_refdef.scene.worldmodel->brush.shadowmesh->numtriangles, r_refdef.scene.worldmodel->brush.shadowmesh->vertex3f, r_refdef.scene.worldmodel->brush.shadowmesh->element3i, r_refdef.scene.worldmodel->brush.shadowmesh->neighbor3i, rsurface.rtlight->shadoworigin, NULL, rsurface.rtlight->radius + r_refdef.scene.worldmodel->radius*2 + r_shadow_projectdistance.value, numshadowmark, shadowmarklist, r_refdef.scene.worldmodel->normalmins, r_refdef.scene.worldmodel->normalmaxs);
	}
	else if (numsurfaces)
		r_refdef.scene.worldmodel->DrawShadowVolume(r_refdef.scene.worldentity, rsurface.rtlight->shadoworigin, NULL, rsurface.rtlight->radius, numsurfaces, surfacelist, rsurface.rtlight->cached_cullmins, rsurface.rtlight->cached_cullmaxs);

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Shadow_DrawEntityShadow(entity_render_t *ent)
{
	vec3_t relativeshadoworigin, relativeshadowmins, relativeshadowmaxs;
	vec_t relativeshadowradius;
	RSurf_ActiveModelEntity(ent, false, false, false);
	Matrix4x4_Transform(&ent->inversematrix, rsurface.rtlight->shadoworigin, relativeshadoworigin);
	// we need to re-init the shader for each entity because the matrix changed
	relativeshadowradius = rsurface.rtlight->radius / ent->scale;
	relativeshadowmins[0] = relativeshadoworigin[0] - relativeshadowradius;
	relativeshadowmins[1] = relativeshadoworigin[1] - relativeshadowradius;
	relativeshadowmins[2] = relativeshadoworigin[2] - relativeshadowradius;
	relativeshadowmaxs[0] = relativeshadoworigin[0] + relativeshadowradius;
	relativeshadowmaxs[1] = relativeshadoworigin[1] + relativeshadowradius;
	relativeshadowmaxs[2] = relativeshadoworigin[2] + relativeshadowradius;
	if (r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPRECTANGLE || r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAPCUBESIDE || r_shadow_rendermode == R_SHADOW_RENDERMODE_SHADOWMAP2D)
	{
		ent->model->DrawShadowMap(r_shadow_shadowmapside, ent, relativeshadoworigin, NULL, relativeshadowradius, ent->model->nummodelsurfaces, ent->model->sortedmodelsurfaces, NULL, relativeshadowmins, relativeshadowmaxs);
	}
	else
		ent->model->DrawShadowVolume(ent, relativeshadoworigin, NULL, relativeshadowradius, ent->model->nummodelsurfaces, ent->model->sortedmodelsurfaces, relativeshadowmins, relativeshadowmaxs);
	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Shadow_SetupEntityLight(const entity_render_t *ent)
{
	// set up properties for rendering light onto this entity
	RSurf_ActiveModelEntity(ent, true, true, false);
	GL_AlphaTest(false);
	Matrix4x4_Concat(&rsurface.entitytolight, &rsurface.rtlight->matrix_worldtolight, &ent->matrix);
	Matrix4x4_Concat(&rsurface.entitytoattenuationxyz, &matrix_attenuationxyz, &rsurface.entitytolight);
	Matrix4x4_Concat(&rsurface.entitytoattenuationz, &matrix_attenuationz, &rsurface.entitytolight);
	Matrix4x4_Transform(&ent->inversematrix, rsurface.rtlight->shadoworigin, rsurface.entitylightorigin);
}

void R_Shadow_DrawWorldLight(int numsurfaces, int *surfacelist, const unsigned char *lighttrispvs)
{
	if (!r_refdef.scene.worldmodel->DrawLight)
		return;

	// set up properties for rendering light onto this entity
	RSurf_ActiveWorldEntity();
	GL_AlphaTest(false);
	rsurface.entitytolight = rsurface.rtlight->matrix_worldtolight;
	Matrix4x4_Concat(&rsurface.entitytoattenuationxyz, &matrix_attenuationxyz, &rsurface.entitytolight);
	Matrix4x4_Concat(&rsurface.entitytoattenuationz, &matrix_attenuationz, &rsurface.entitytolight);
	VectorCopy(rsurface.rtlight->shadoworigin, rsurface.entitylightorigin);

	r_refdef.scene.worldmodel->DrawLight(r_refdef.scene.worldentity, numsurfaces, surfacelist, lighttrispvs);

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Shadow_DrawEntityLight(entity_render_t *ent)
{
	dp_model_t *model = ent->model;
	if (!model->DrawLight)
		return;

	R_Shadow_SetupEntityLight(ent);

	model->DrawLight(ent, model->nummodelsurfaces, model->sortedmodelsurfaces, NULL);

	rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
}

void R_Shadow_PrepareLight(rtlight_t *rtlight)
{
	int i;
	float f;
	int numleafs, numsurfaces;
	int *leaflist, *surfacelist;
	unsigned char *leafpvs, *shadowtrispvs, *lighttrispvs, *surfacesides;
	int numlightentities;
	int numlightentities_noselfshadow;
	int numshadowentities;
	int numshadowentities_noselfshadow;
	static entity_render_t *lightentities[MAX_EDICTS];
	static entity_render_t *lightentities_noselfshadow[MAX_EDICTS];
	static entity_render_t *shadowentities[MAX_EDICTS];
	static entity_render_t *shadowentities_noselfshadow[MAX_EDICTS];

	rtlight->draw = false;

	// skip lights that don't light because of ambientscale+diffusescale+specularscale being 0 (corona only lights)
	// skip lights that are basically invisible (color 0 0 0)
	if (VectorLength2(rtlight->color) * (rtlight->ambientscale + rtlight->diffusescale + rtlight->specularscale) < (1.0f / 1048576.0f))
		return;

	// loading is done before visibility checks because loading should happen
	// all at once at the start of a level, not when it stalls gameplay.
	// (especially important to benchmarks)
	// compile light
	if (rtlight->isstatic && (!rtlight->compiled || (rtlight->shadow && rtlight->shadowmode != (int)r_shadow_shadowmode)) && r_shadow_realtime_world_compile.integer)
	{
		if (rtlight->compiled)
			R_RTLight_Uncompile(rtlight);
		R_RTLight_Compile(rtlight);
	}

	// load cubemap
	rtlight->currentcubemap = rtlight->cubemapname[0] ? R_Shadow_Cubemap(rtlight->cubemapname) : r_texture_whitecube;

	// look up the light style value at this time
	f = (rtlight->style >= 0 ? r_refdef.scene.rtlightstylevalue[rtlight->style] : 1) * r_shadow_lightintensityscale.value;
	VectorScale(rtlight->color, f, rtlight->currentcolor);
	/*
	if (rtlight->selected)
	{
		f = 2 + sin(realtime * M_PI * 4.0);
		VectorScale(rtlight->currentcolor, f, rtlight->currentcolor);
	}
	*/

	// if lightstyle is currently off, don't draw the light
	if (VectorLength2(rtlight->currentcolor) < (1.0f / 1048576.0f))
		return;

	// if the light box is offscreen, skip it
	if (R_CullBox(rtlight->cullmins, rtlight->cullmaxs))
		return;

	VectorCopy(rtlight->cullmins, rtlight->cached_cullmins);
	VectorCopy(rtlight->cullmaxs, rtlight->cached_cullmaxs);

	R_Shadow_ComputeShadowCasterCullingPlanes(rtlight);

	if (rtlight->compiled && r_shadow_realtime_world_compile.integer)
	{
		// compiled light, world available and can receive realtime lighting
		// retrieve leaf information
		numleafs = rtlight->static_numleafs;
		leaflist = rtlight->static_leaflist;
		leafpvs = rtlight->static_leafpvs;
		numsurfaces = rtlight->static_numsurfaces;
		surfacelist = rtlight->static_surfacelist;
		surfacesides = NULL;
		shadowtrispvs = rtlight->static_shadowtrispvs;
		lighttrispvs = rtlight->static_lighttrispvs;
	}
	else if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->GetLightInfo)
	{
		// dynamic light, world available and can receive realtime lighting
		// calculate lit surfaces and leafs
		r_refdef.scene.worldmodel->GetLightInfo(r_refdef.scene.worldentity, rtlight->shadoworigin, rtlight->radius, rtlight->cached_cullmins, rtlight->cached_cullmaxs, r_shadow_buffer_leaflist, r_shadow_buffer_leafpvs, &numleafs, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces, r_shadow_buffer_shadowtrispvs, r_shadow_buffer_lighttrispvs, r_shadow_buffer_visitingleafpvs, rtlight->cached_numfrustumplanes, rtlight->cached_frustumplanes);
		R_Shadow_ComputeShadowCasterCullingPlanes(rtlight);
		leaflist = r_shadow_buffer_leaflist;
		leafpvs = r_shadow_buffer_leafpvs;
		surfacelist = r_shadow_buffer_surfacelist;
		surfacesides = r_shadow_buffer_surfacesides;
		shadowtrispvs = r_shadow_buffer_shadowtrispvs;
		lighttrispvs = r_shadow_buffer_lighttrispvs;
		// if the reduced leaf bounds are offscreen, skip it
		if (R_CullBox(rtlight->cached_cullmins, rtlight->cached_cullmaxs))
			return;
	}
	else
	{
		// no world
		numleafs = 0;
		leaflist = NULL;
		leafpvs = NULL;
		numsurfaces = 0;
		surfacelist = NULL;
		surfacesides = NULL;
		shadowtrispvs = NULL;
		lighttrispvs = NULL;
	}
	// check if light is illuminating any visible leafs
	if (numleafs)
	{
		for (i = 0;i < numleafs;i++)
			if (r_refdef.viewcache.world_leafvisible[leaflist[i]])
				break;
		if (i == numleafs)
			return;
	}

	// make a list of lit entities and shadow casting entities
	numlightentities = 0;
	numlightentities_noselfshadow = 0;
	numshadowentities = 0;
	numshadowentities_noselfshadow = 0;

	// add dynamic entities that are lit by the light
	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		dp_model_t *model;
		entity_render_t *ent = r_refdef.scene.entities[i];
		vec3_t org;
		if (!BoxesOverlap(ent->mins, ent->maxs, rtlight->cached_cullmins, rtlight->cached_cullmaxs))
			continue;
		// skip the object entirely if it is not within the valid
		// shadow-casting region (which includes the lit region)
		if (R_CullBoxCustomPlanes(ent->mins, ent->maxs, rtlight->cached_numfrustumplanes, rtlight->cached_frustumplanes))
			continue;
		if (!(model = ent->model))
			continue;
		if (r_refdef.viewcache.entityvisible[i] && model->DrawLight && (ent->flags & RENDER_LIGHT))
		{
			// this entity wants to receive light, is visible, and is
			// inside the light box
			// TODO: check if the surfaces in the model can receive light
			// so now check if it's in a leaf seen by the light
			if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.BoxTouchingLeafPVS && !r_refdef.scene.worldmodel->brush.BoxTouchingLeafPVS(r_refdef.scene.worldmodel, leafpvs, ent->mins, ent->maxs))
				continue;
			if (ent->flags & RENDER_NOSELFSHADOW)
				lightentities_noselfshadow[numlightentities_noselfshadow++] = ent;
			else
				lightentities[numlightentities++] = ent;
			// since it is lit, it probably also casts a shadow...
			// about the VectorDistance2 - light emitting entities should not cast their own shadow
			Matrix4x4_OriginFromMatrix(&ent->matrix, org);
			if ((ent->flags & RENDER_SHADOW) && model->DrawShadowVolume && VectorDistance2(org, rtlight->shadoworigin) > 0.1)
			{
				// note: exterior models without the RENDER_NOSELFSHADOW
				// flag still create a RENDER_NOSELFSHADOW shadow but
				// are lit normally, this means that they are
				// self-shadowing but do not shadow other
				// RENDER_NOSELFSHADOW entities such as the gun
				// (very weird, but keeps the player shadow off the gun)
				if (ent->flags & (RENDER_NOSELFSHADOW | RENDER_EXTERIORMODEL))
					shadowentities_noselfshadow[numshadowentities_noselfshadow++] = ent;
				else
					shadowentities[numshadowentities++] = ent;
			}
		}
		else if (ent->flags & RENDER_SHADOW)
		{
			// this entity is not receiving light, but may still need to
			// cast a shadow...
			// TODO: check if the surfaces in the model can cast shadow
			// now check if it is in a leaf seen by the light
			if (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.BoxTouchingLeafPVS && !r_refdef.scene.worldmodel->brush.BoxTouchingLeafPVS(r_refdef.scene.worldmodel, leafpvs, ent->mins, ent->maxs))
				continue;
			// about the VectorDistance2 - light emitting entities should not cast their own shadow
			Matrix4x4_OriginFromMatrix(&ent->matrix, org);
			if ((ent->flags & RENDER_SHADOW) && model->DrawShadowVolume && VectorDistance2(org, rtlight->shadoworigin) > 0.1)
			{
				if (ent->flags & (RENDER_NOSELFSHADOW | RENDER_EXTERIORMODEL))
					shadowentities_noselfshadow[numshadowentities_noselfshadow++] = ent;
				else
					shadowentities[numshadowentities++] = ent;
			}
		}
	}

	// return if there's nothing at all to light
	if (numsurfaces + numlightentities + numlightentities_noselfshadow == 0)
		return;

	// count this light in the r_speeds
	r_refdef.stats.lights++;

	// flag it as worth drawing later
	rtlight->draw = true;

	// cache all the animated entities that cast a shadow but are not visible
	for (i = 0;i < numshadowentities;i++)
		if (!shadowentities[i]->animcache_vertex3f)
			R_AnimCache_GetEntity(shadowentities[i], false, false);
	for (i = 0;i < numshadowentities_noselfshadow;i++)
		if (!shadowentities_noselfshadow[i]->animcache_vertex3f)
			R_AnimCache_GetEntity(shadowentities_noselfshadow[i], false, false);

	// allocate some temporary memory for rendering this light later in the frame
	// reusable buffers need to be copied, static data can be used as-is
	rtlight->cached_numlightentities               = numlightentities;
	rtlight->cached_numlightentities_noselfshadow  = numlightentities_noselfshadow;
	rtlight->cached_numshadowentities              = numshadowentities;
	rtlight->cached_numshadowentities_noselfshadow = numshadowentities_noselfshadow;
	rtlight->cached_numsurfaces                    = numsurfaces;
	rtlight->cached_lightentities                  = (entity_render_t**)R_FrameData_Store(numlightentities*sizeof(entity_render_t*), (void*)lightentities);
	rtlight->cached_lightentities_noselfshadow     = (entity_render_t**)R_FrameData_Store(numlightentities_noselfshadow*sizeof(entity_render_t*), (void*)lightentities_noselfshadow);
	rtlight->cached_shadowentities                 = (entity_render_t**)R_FrameData_Store(numshadowentities*sizeof(entity_render_t*), (void*)shadowentities);
	rtlight->cached_shadowentities_noselfshadow    = (entity_render_t**)R_FrameData_Store(numshadowentities_noselfshadow*sizeof(entity_render_t *), (void*)shadowentities_noselfshadow);
	if (shadowtrispvs == r_shadow_buffer_shadowtrispvs)
	{
		int numshadowtrispvsbytes = (((r_refdef.scene.worldmodel->brush.shadowmesh ? r_refdef.scene.worldmodel->brush.shadowmesh->numtriangles : r_refdef.scene.worldmodel->surfmesh.num_triangles) + 7) >> 3);
		int numlighttrispvsbytes = ((r_refdef.scene.worldmodel->surfmesh.num_triangles + 7) >> 3);
		rtlight->cached_shadowtrispvs                  =   (unsigned char *)R_FrameData_Store(numshadowtrispvsbytes, shadowtrispvs);
		rtlight->cached_lighttrispvs                   =   (unsigned char *)R_FrameData_Store(numlighttrispvsbytes, lighttrispvs);
		rtlight->cached_surfacelist                    =              (int*)R_FrameData_Store(numsurfaces*sizeof(int), (void*)surfacelist);
	}
	else
	{
		// compiled light data
		rtlight->cached_shadowtrispvs = shadowtrispvs;
		rtlight->cached_lighttrispvs = lighttrispvs;
		rtlight->cached_surfacelist = surfacelist;
	}
}

void R_Shadow_DrawLight(rtlight_t *rtlight)
{
	int i;
	int numsurfaces;
	unsigned char *shadowtrispvs, *lighttrispvs, *surfacesides;
	int numlightentities;
	int numlightentities_noselfshadow;
	int numshadowentities;
	int numshadowentities_noselfshadow;
	entity_render_t **lightentities;
	entity_render_t **lightentities_noselfshadow;
	entity_render_t **shadowentities;
	entity_render_t **shadowentities_noselfshadow;
	int *surfacelist;
	static unsigned char entitysides[MAX_EDICTS];
	static unsigned char entitysides_noselfshadow[MAX_EDICTS];
	vec3_t nearestpoint;
	vec_t distance;
	qboolean castshadows;
	int lodlinear;

	// check if we cached this light this frame (meaning it is worth drawing)
	if (!rtlight->draw)
		return;

	// if R_FrameData_Store ran out of space we skip anything dependent on it
	if (r_framedata_failed)
		return;

	numlightentities = rtlight->cached_numlightentities;
	numlightentities_noselfshadow = rtlight->cached_numlightentities_noselfshadow;
	numshadowentities = rtlight->cached_numshadowentities;
	numshadowentities_noselfshadow = rtlight->cached_numshadowentities_noselfshadow;
	numsurfaces = rtlight->cached_numsurfaces;
	lightentities = rtlight->cached_lightentities;
	lightentities_noselfshadow = rtlight->cached_lightentities_noselfshadow;
	shadowentities = rtlight->cached_shadowentities;
	shadowentities_noselfshadow = rtlight->cached_shadowentities_noselfshadow;
	shadowtrispvs = rtlight->cached_shadowtrispvs;
	lighttrispvs = rtlight->cached_lighttrispvs;
	surfacelist = rtlight->cached_surfacelist;

	// set up a scissor rectangle for this light
	if (R_Shadow_ScissorForBBox(rtlight->cached_cullmins, rtlight->cached_cullmaxs))
		return;

	// don't let sound skip if going slow
	if (r_refdef.scene.extraupdate)
		S_ExtraUpdate ();

	// make this the active rtlight for rendering purposes
	R_Shadow_RenderMode_ActiveLight(rtlight);

	if (r_showshadowvolumes.integer && r_refdef.view.showdebug && numsurfaces + numshadowentities + numshadowentities_noselfshadow && rtlight->shadow && (rtlight->isstatic ? r_refdef.scene.rtworldshadows : r_refdef.scene.rtdlightshadows))
	{
		// optionally draw visible shape of the shadow volumes
		// for performance analysis by level designers
		R_Shadow_RenderMode_VisibleShadowVolumes();
		if (numsurfaces)
			R_Shadow_DrawWorldShadow_ShadowVolume(numsurfaces, surfacelist, shadowtrispvs);
		for (i = 0;i < numshadowentities;i++)
			R_Shadow_DrawEntityShadow(shadowentities[i]);
		for (i = 0;i < numshadowentities_noselfshadow;i++)
			R_Shadow_DrawEntityShadow(shadowentities_noselfshadow[i]);
		R_Shadow_RenderMode_VisibleLighting(false, false);
	}

	if (r_showlighting.integer && r_refdef.view.showdebug && numsurfaces + numlightentities + numlightentities_noselfshadow)
	{
		// optionally draw the illuminated areas
		// for performance analysis by level designers
		R_Shadow_RenderMode_VisibleLighting(false, false);
		if (numsurfaces)
			R_Shadow_DrawWorldLight(numsurfaces, surfacelist, lighttrispvs);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i]);
		for (i = 0;i < numlightentities_noselfshadow;i++)
			R_Shadow_DrawEntityLight(lightentities_noselfshadow[i]);
	}

	castshadows = numsurfaces + numshadowentities + numshadowentities_noselfshadow > 0 && rtlight->shadow && (rtlight->isstatic ? r_refdef.scene.rtworldshadows : r_refdef.scene.rtdlightshadows);

	nearestpoint[0] = bound(rtlight->cullmins[0], r_refdef.view.origin[0], rtlight->cullmaxs[0]);
	nearestpoint[1] = bound(rtlight->cullmins[1], r_refdef.view.origin[1], rtlight->cullmaxs[1]);
	nearestpoint[2] = bound(rtlight->cullmins[2], r_refdef.view.origin[2], rtlight->cullmaxs[2]);
	distance = VectorDistance(nearestpoint, r_refdef.view.origin);

	lodlinear = (rtlight->radius * r_shadow_shadowmapping_precision.value) / sqrt(max(1.0f, distance/rtlight->radius));
	//lodlinear = (int)(r_shadow_shadowmapping_lod_bias.value + r_shadow_shadowmapping_lod_scale.value * rtlight->radius / max(1.0f, distance));
	lodlinear = bound(r_shadow_shadowmapping_minsize.integer, lodlinear, r_shadow_shadowmapmaxsize);

	if (castshadows && (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAP2D || r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE || r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE))
	{
		float borderbias;
		int side;
		int size;
		int castermask = 0;
		int receivermask = 0;
		matrix4x4_t radiustolight = rtlight->matrix_worldtolight;
		Matrix4x4_Abs(&radiustolight);

		r_shadow_shadowmaplod = 0;
		for (i = 1;i < R_SHADOW_SHADOWMAP_NUMCUBEMAPS;i++)
			if ((r_shadow_shadowmapmaxsize >> i) > lodlinear)
				r_shadow_shadowmaplod = i;

		if (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE)
			size = max(1, r_shadow_shadowmapmaxsize >> r_shadow_shadowmaplod);
		else
			size = bound(r_shadow_shadowmapborder, lodlinear, r_shadow_shadowmapmaxsize);
			
		borderbias = r_shadow_shadowmapborder / (float)(size - r_shadow_shadowmapborder);

		surfacesides = NULL;
		if (numsurfaces)
		{
			if (rtlight->compiled && r_shadow_realtime_world_compile.integer && r_shadow_realtime_world_compileshadow.integer)
			{
				castermask = rtlight->static_shadowmap_casters;
				receivermask = rtlight->static_shadowmap_receivers;
			}
			else
			{
				surfacesides = r_shadow_buffer_surfacesides;
				for(i = 0;i < numsurfaces;i++)
				{
					msurface_t *surface = r_refdef.scene.worldmodel->data_surfaces + surfacelist[i];
					surfacesides[i] = R_Shadow_CalcBBoxSideMask(surface->mins, surface->maxs, &rtlight->matrix_worldtolight, &radiustolight, borderbias);		
					castermask |= surfacesides[i];
					receivermask |= surfacesides[i];
				}
			}
		}
		if (receivermask < 0x3F) 
		{
			for (i = 0;i < numlightentities;i++)
				receivermask |= R_Shadow_CalcEntitySideMask(lightentities[i], &rtlight->matrix_worldtolight, &radiustolight, borderbias);
			if (receivermask < 0x3F)
				for(i = 0; i < numlightentities_noselfshadow;i++)
					receivermask |= R_Shadow_CalcEntitySideMask(lightentities_noselfshadow[i], &rtlight->matrix_worldtolight, &radiustolight, borderbias);
		}

		receivermask &= R_Shadow_CullFrustumSides(rtlight, size, r_shadow_shadowmapborder);

		if (receivermask)
		{
			for (i = 0;i < numshadowentities;i++)
				castermask |= (entitysides[i] = R_Shadow_CalcEntitySideMask(shadowentities[i], &rtlight->matrix_worldtolight, &radiustolight, borderbias));
			for (i = 0;i < numshadowentities_noselfshadow;i++)
				castermask |= (entitysides_noselfshadow[i] = R_Shadow_CalcEntitySideMask(shadowentities_noselfshadow[i], &rtlight->matrix_worldtolight, &radiustolight, borderbias)); 
		}

		//Con_Printf("distance %f lodlinear %i (lod %i) size %i\n", distance, lodlinear, r_shadow_shadowmaplod, size);

		// render shadow casters into 6 sided depth texture
		for (side = 0;side < 6;side++) if (receivermask & (1 << side))
		{
			R_Shadow_RenderMode_ShadowMap(side, receivermask, size);
			if (! (castermask & (1 << side))) continue;
			if (numsurfaces)
				R_Shadow_DrawWorldShadow_ShadowMap(numsurfaces, surfacelist, shadowtrispvs, surfacesides);
			for (i = 0;i < numshadowentities;i++) if (entitysides[i] & (1 << side))
				R_Shadow_DrawEntityShadow(shadowentities[i]);
		}

		if (numlightentities_noselfshadow)
		{
			// render lighting using the depth texture as shadowmap
			// draw lighting in the unmasked areas
			R_Shadow_RenderMode_Lighting(false, false, true);
			for (i = 0;i < numlightentities_noselfshadow;i++)
				R_Shadow_DrawEntityLight(lightentities_noselfshadow[i]);
		}

		// render shadow casters into 6 sided depth texture
		if (numshadowentities_noselfshadow)
		{
			for (side = 0;side < 6;side++) if ((receivermask & castermask) & (1 << side))
			{
				R_Shadow_RenderMode_ShadowMap(side, 0, size);
				for (i = 0;i < numshadowentities_noselfshadow;i++) if (entitysides_noselfshadow[i] & (1 << side))
					R_Shadow_DrawEntityShadow(shadowentities_noselfshadow[i]);
			}
		}

		// render lighting using the depth texture as shadowmap
		// draw lighting in the unmasked areas
		R_Shadow_RenderMode_Lighting(false, false, true);
		// draw lighting in the unmasked areas
		if (numsurfaces)
			R_Shadow_DrawWorldLight(numsurfaces, surfacelist, lighttrispvs);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i]);
	}
	else if (castshadows && vid.stencil)
	{
		// draw stencil shadow volumes to mask off pixels that are in shadow
		// so that they won't receive lighting
		GL_Scissor(r_shadow_lightscissor[0], r_shadow_lightscissor[1], r_shadow_lightscissor[2], r_shadow_lightscissor[3]);
		R_Shadow_ClearStencil();

		if (numsurfaces)
			R_Shadow_DrawWorldShadow_ShadowVolume(numsurfaces, surfacelist, shadowtrispvs);
		for (i = 0;i < numshadowentities;i++)
			R_Shadow_DrawEntityShadow(shadowentities[i]);

		// draw lighting in the unmasked areas
		R_Shadow_RenderMode_Lighting(true, false, false);
		for (i = 0;i < numlightentities_noselfshadow;i++)
			R_Shadow_DrawEntityLight(lightentities_noselfshadow[i]);

		for (i = 0;i < numshadowentities_noselfshadow;i++)
			R_Shadow_DrawEntityShadow(shadowentities_noselfshadow[i]);

		// draw lighting in the unmasked areas
		R_Shadow_RenderMode_Lighting(true, false, false);
		if (numsurfaces)
			R_Shadow_DrawWorldLight(numsurfaces, surfacelist, lighttrispvs);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i]);
	}
	else
	{
		// draw lighting in the unmasked areas
		R_Shadow_RenderMode_Lighting(false, false, false);
		if (numsurfaces)
			R_Shadow_DrawWorldLight(numsurfaces, surfacelist, lighttrispvs);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i]);
		for (i = 0;i < numlightentities_noselfshadow;i++)
			R_Shadow_DrawEntityLight(lightentities_noselfshadow[i]);
	}

	if (r_shadow_usingdeferredprepass)
	{
		// when rendering deferred lighting, we simply rasterize the box
		if (castshadows && (r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAP2D || r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPRECTANGLE || r_shadow_shadowmode == R_SHADOW_SHADOWMODE_SHADOWMAPCUBESIDE))
			R_Shadow_RenderMode_DrawDeferredLight(false, true);
		else if (castshadows && vid.stencil)
			R_Shadow_RenderMode_DrawDeferredLight(true, false);
		else
			R_Shadow_RenderMode_DrawDeferredLight(false, false);
	}
}

static void R_Shadow_FreeDeferred(void)
{
	if (r_shadow_prepassgeometryfbo)
		qglDeleteFramebuffersEXT(1, &r_shadow_prepassgeometryfbo);CHECKGLERROR
	r_shadow_prepassgeometryfbo = 0;

	if (r_shadow_prepasslightingfbo)
		qglDeleteFramebuffersEXT(1, &r_shadow_prepasslightingfbo);CHECKGLERROR
	r_shadow_prepasslightingfbo = 0;

	if (r_shadow_prepassgeometrydepthtexture)
		R_FreeTexture(r_shadow_prepassgeometrydepthtexture);
	r_shadow_prepassgeometrydepthtexture = NULL;

	if (r_shadow_prepassgeometrynormalmaptexture)
		R_FreeTexture(r_shadow_prepassgeometrynormalmaptexture);
	r_shadow_prepassgeometrynormalmaptexture = NULL;

	if (r_shadow_prepasslightingdiffusetexture)
		R_FreeTexture(r_shadow_prepasslightingdiffusetexture);
	r_shadow_prepasslightingdiffusetexture = NULL;

	if (r_shadow_prepasslightingspeculartexture)
		R_FreeTexture(r_shadow_prepasslightingspeculartexture);
	r_shadow_prepasslightingspeculartexture = NULL;
}

void R_Shadow_DrawPrepass(void)
{
	int i;
	int flag;
	int lnum;
	size_t lightindex;
	dlight_t *light;
	size_t range;
	entity_render_t *ent;

	GL_AlphaTest(false);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	GL_DepthMask(true);
	GL_ColorMask(1,1,1,1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_Color(1,1,1,1);
	GL_DepthTest(true);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_prepassgeometryfbo);CHECKGLERROR
	qglClearColor(0.5f,0.5f,0.5f,1.0f);CHECKGLERROR
	GL_Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);CHECKGLERROR

	if (cl.csqc_vidvars.drawworld && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->DrawPrepass)
		r_refdef.scene.worldmodel->DrawPrepass(r_refdef.scene.worldentity);
	if (r_timereport_active)
		R_TimeReport("prepassworld");

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.scene.entities[i];
		if (ent->model && ent->model->DrawPrepass != NULL)
			ent->model->DrawPrepass(ent);
	}

	if (r_timereport_active)
		R_TimeReport("prepassmodels");

	GL_DepthMask(false);
	GL_ColorMask(1,1,1,1);
	GL_Color(1,1,1,1);
	GL_DepthTest(true);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_prepasslightingfbo);CHECKGLERROR
	qglClearColor(0.0f,0.0f,0.0f,0.0f);CHECKGLERROR
	GL_Clear(GL_COLOR_BUFFER_BIT);CHECKGLERROR
	if (r_refdef.fogenabled)
		qglClearColor(r_refdef.fogcolor[0],r_refdef.fogcolor[1],r_refdef.fogcolor[2],0);CHECKGLERROR

	R_Shadow_RenderMode_Begin();

	flag = r_refdef.scene.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	if (r_shadow_debuglight.integer >= 0)
	{
		lightindex = r_shadow_debuglight.integer;
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (light && (light->flags & flag))
			R_Shadow_DrawLight(&light->rtlight);
	}
	else
	{
		range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
		for (lightindex = 0;lightindex < range;lightindex++)
		{
			light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
			if (light && (light->flags & flag))
				R_Shadow_DrawLight(&light->rtlight);
		}
	}
	if (r_refdef.scene.rtdlight)
		for (lnum = 0;lnum < r_refdef.scene.numlights;lnum++)
			R_Shadow_DrawLight(r_refdef.scene.lights[lnum]);

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);CHECKGLERROR
	if (r_refdef.fogenabled)
		qglClearColor(r_refdef.fogcolor[0],r_refdef.fogcolor[1],r_refdef.fogcolor[2],0);CHECKGLERROR

	R_Shadow_RenderMode_End();

	if (r_timereport_active)
		R_TimeReport("prepasslights");
}

void R_Shadow_DrawLightSprites(void);
void R_Shadow_PrepareLights(void)
{
	int flag;
	int lnum;
	size_t lightindex;
	dlight_t *light;
	size_t range;
	float f;
	GLenum status;

	if (r_shadow_shadowmapmaxsize != bound(1, r_shadow_shadowmapping_maxsize.integer, (int)vid.maxtexturesize_2d / 4) ||
		(r_shadow_shadowmode != R_SHADOW_SHADOWMODE_STENCIL) != (r_shadow_shadowmapping.integer || r_shadow_deferred.integer) ||
		r_shadow_shadowmapvsdct != (r_shadow_shadowmapping_vsdct.integer != 0) || 
		r_shadow_shadowmaptexturetype != r_shadow_shadowmapping_texturetype.integer ||
		r_shadow_shadowmapfilterquality != r_shadow_shadowmapping_filterquality.integer || 
		r_shadow_shadowmapdepthbits != r_shadow_shadowmapping_depthbits.integer || 
		r_shadow_shadowmapborder != bound(0, r_shadow_shadowmapping_bordersize.integer, 16))
		R_Shadow_FreeShadowMaps();

	switch (vid.renderpath)
	{
	case RENDERPATH_GL20:
	case RENDERPATH_CGGL:
		if (!r_shadow_deferred.integer || r_shadow_shadowmode == R_SHADOW_SHADOWMODE_STENCIL || !vid.support.ext_framebuffer_object || vid.maxdrawbuffers < 2)
		{
			r_shadow_usingdeferredprepass = false;
			if (r_shadow_prepass_width)
				R_Shadow_FreeDeferred();
			r_shadow_prepass_width = r_shadow_prepass_height = 0;
			break;
		}

		if (r_shadow_prepass_width != vid.width || r_shadow_prepass_height != vid.height)
		{
			R_Shadow_FreeDeferred();

			r_shadow_usingdeferredprepass = true;
			r_shadow_prepass_width = vid.width;
			r_shadow_prepass_height = vid.height;
			r_shadow_prepassgeometrydepthtexture = R_LoadTextureShadowMap2D(r_shadow_texturepool, "prepassgeometrydepthmap", vid.width, vid.height, 24, false);
			r_shadow_prepassgeometrynormalmaptexture = R_LoadTexture2D(r_shadow_texturepool, "prepassgeometrynormalmap", vid.width, vid.height, NULL, TEXTYPE_COLORBUFFER, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCENEAREST, NULL);
			r_shadow_prepasslightingdiffusetexture = R_LoadTexture2D(r_shadow_texturepool, "prepasslightingdiffuse", vid.width, vid.height, NULL, TEXTYPE_COLORBUFFER, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCENEAREST, NULL);
			r_shadow_prepasslightingspeculartexture = R_LoadTexture2D(r_shadow_texturepool, "prepasslightingspecular", vid.width, vid.height, NULL, TEXTYPE_COLORBUFFER, TEXF_CLAMP | TEXF_ALPHA | TEXF_FORCENEAREST, NULL);

			// set up the geometry pass fbo (depth + normalmap)
			qglGenFramebuffersEXT(1, &r_shadow_prepassgeometryfbo);CHECKGLERROR
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_prepassgeometryfbo);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_prepassgeometrydepthtexture), 0);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_prepassgeometrynormalmaptexture), 0);CHECKGLERROR
			// render depth into one texture and normalmap into the other
			if (qglDrawBuffersARB)
			{
				qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);CHECKGLERROR
				qglReadBuffer(GL_NONE);CHECKGLERROR
			}
			status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
			{
				Con_Printf("R_PrepareRTLights: glCheckFramebufferStatusEXT returned %i\n", status);
				Cvar_SetValueQuick(&r_shadow_deferred, 0);
				r_shadow_usingdeferredprepass = false;
			}

			// set up the lighting pass fbo (diffuse + specular)
			qglGenFramebuffersEXT(1, &r_shadow_prepasslightingfbo);CHECKGLERROR
			qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r_shadow_prepasslightingfbo);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_prepassgeometrydepthtexture), 0);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_prepasslightingdiffusetexture), 0);CHECKGLERROR
			qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, R_GetTexture(r_shadow_prepasslightingspeculartexture), 0);CHECKGLERROR
			// render diffuse into one texture and specular into another,
			// with depth and normalmap bound as textures,
			// with depth bound as attachment as well
			if (qglDrawBuffersARB)
			{
				qglDrawBuffersARB(2, r_shadow_prepasslightingdrawbuffers);CHECKGLERROR
				qglReadBuffer(GL_NONE);CHECKGLERROR
			}
			status = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);CHECKGLERROR
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
			{
				Con_Printf("R_PrepareRTLights: glCheckFramebufferStatusEXT returned %i\n", status);
				Cvar_SetValueQuick(&r_shadow_deferred, 0);
				r_shadow_usingdeferredprepass = false;
			}
		}
		break;
	case RENDERPATH_GL13:
	case RENDERPATH_GL11:
		r_shadow_usingdeferredprepass = false;
		break;
	}

	R_Shadow_EnlargeLeafSurfaceTrisBuffer(r_refdef.scene.worldmodel->brush.num_leafs, r_refdef.scene.worldmodel->num_surfaces, r_refdef.scene.worldmodel->brush.shadowmesh ? r_refdef.scene.worldmodel->brush.shadowmesh->numtriangles : r_refdef.scene.worldmodel->surfmesh.num_triangles, r_refdef.scene.worldmodel->surfmesh.num_triangles);

	flag = r_refdef.scene.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	if (r_shadow_debuglight.integer >= 0)
	{
		lightindex = r_shadow_debuglight.integer;
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (light && (light->flags & flag))
			R_Shadow_PrepareLight(&light->rtlight);
	}
	else
	{
		range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
		for (lightindex = 0;lightindex < range;lightindex++)
		{
			light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
			if (light && (light->flags & flag))
				R_Shadow_PrepareLight(&light->rtlight);
		}
	}
	if (r_refdef.scene.rtdlight)
	{
		for (lnum = 0;lnum < r_refdef.scene.numlights;lnum++)
			R_Shadow_PrepareLight(r_refdef.scene.lights[lnum]);
	}
	else if(gl_flashblend.integer)
	{
		for (lnum = 0;lnum < r_refdef.scene.numlights;lnum++)
		{
			rtlight_t *rtlight = r_refdef.scene.lights[lnum];
			f = (rtlight->style >= 0 ? r_refdef.scene.lightstylevalue[rtlight->style] : 1) * r_shadow_lightintensityscale.value;
			VectorScale(rtlight->color, f, rtlight->currentcolor);
		}
	}

	if (r_editlights.integer)
		R_Shadow_DrawLightSprites();
}

void R_Shadow_DrawLights(void)
{
	int flag;
	int lnum;
	size_t lightindex;
	dlight_t *light;
	size_t range;

	R_Shadow_RenderMode_Begin();

	flag = r_refdef.scene.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	if (r_shadow_debuglight.integer >= 0)
	{
		lightindex = r_shadow_debuglight.integer;
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (light && (light->flags & flag))
			R_Shadow_DrawLight(&light->rtlight);
	}
	else
	{
		range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
		for (lightindex = 0;lightindex < range;lightindex++)
		{
			light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
			if (light && (light->flags & flag))
				R_Shadow_DrawLight(&light->rtlight);
		}
	}
	if (r_refdef.scene.rtdlight)
		for (lnum = 0;lnum < r_refdef.scene.numlights;lnum++)
			R_Shadow_DrawLight(r_refdef.scene.lights[lnum]);

	R_Shadow_RenderMode_End();
}

extern const float r_screenvertex3f[12];
extern void R_SetupView(qboolean allowwaterclippingplane);
extern void R_ResetViewRendering3D(void);
extern void R_ResetViewRendering2D(void);
extern cvar_t r_shadows;
extern cvar_t r_shadows_darken;
extern cvar_t r_shadows_drawafterrtlighting;
extern cvar_t r_shadows_castfrombmodels;
extern cvar_t r_shadows_throwdistance;
extern cvar_t r_shadows_throwdirection;
void R_DrawModelShadows(void)
{
	int i;
	float relativethrowdistance;
	entity_render_t *ent;
	vec3_t relativelightorigin;
	vec3_t relativelightdirection;
	vec3_t relativeshadowmins, relativeshadowmaxs;
	vec3_t tmp, shadowdir;

	if (!r_refdef.scene.numentities || !vid.stencil)
		return;

	CHECKGLERROR
	R_ResetViewRendering3D();
	//GL_Scissor(r_refdef.view.viewport.x, r_refdef.view.viewport.y, r_refdef.view.viewport.width, r_refdef.view.viewport.height);
	//GL_Scissor(r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height);
	R_Shadow_RenderMode_Begin();
	R_Shadow_RenderMode_ActiveLight(NULL);
	r_shadow_lightscissor[0] = r_refdef.view.x;
	r_shadow_lightscissor[1] = vid.height - r_refdef.view.y - r_refdef.view.height;
	r_shadow_lightscissor[2] = r_refdef.view.width;
	r_shadow_lightscissor[3] = r_refdef.view.height;
	R_Shadow_RenderMode_StencilShadowVolumes(false);

	// get shadow dir
	if (r_shadows.integer == 2)
	{
		Math_atov(r_shadows_throwdirection.string, shadowdir);
		VectorNormalize(shadowdir);
	}

	R_Shadow_ClearStencil();

	for (i = 0;i < r_refdef.scene.numentities;i++)
	{
		ent = r_refdef.scene.entities[i];

		// cast shadows from anything of the map (submodels are optional)
		if (ent->model && ent->model->DrawShadowVolume != NULL && (!ent->model->brush.submodel || r_shadows_castfrombmodels.integer) && (ent->flags & RENDER_SHADOW))
		{
			relativethrowdistance = r_shadows_throwdistance.value * Matrix4x4_ScaleFromMatrix(&ent->inversematrix);
			VectorSet(relativeshadowmins, -relativethrowdistance, -relativethrowdistance, -relativethrowdistance);
			VectorSet(relativeshadowmaxs, relativethrowdistance, relativethrowdistance, relativethrowdistance);
			if (r_shadows.integer == 2) // 2: simpler mode, throw shadows always in same direction
				Matrix4x4_Transform3x3(&ent->inversematrix, shadowdir, relativelightdirection);
			else
			{
				if(ent->entitynumber != 0)
				{
					// networked entity - might be attached in some way (then we should use the parent's light direction, to not tear apart attached entities)
					int entnum, entnum2, recursion;
					entnum = entnum2 = ent->entitynumber;
					for(recursion = 32; recursion > 0; --recursion)
					{
						entnum2 = cl.entities[entnum].state_current.tagentity;
						if(entnum2 >= 1 && entnum2 < cl.num_entities && cl.entities_active[entnum2])
							entnum = entnum2;
						else
							break;
					}
					if(recursion && recursion != 32) // if we followed a valid non-empty attachment chain
					{
						VectorNegate(cl.entities[entnum].render.modellight_lightdir, relativelightdirection);
						// transform into modelspace of OUR entity
						Matrix4x4_Transform3x3(&cl.entities[entnum].render.matrix, relativelightdirection, tmp);
						Matrix4x4_Transform3x3(&ent->inversematrix, tmp, relativelightdirection);
					}
					else
						VectorNegate(ent->modellight_lightdir, relativelightdirection);
				}
				else
					VectorNegate(ent->modellight_lightdir, relativelightdirection);
			}

			VectorScale(relativelightdirection, -relativethrowdistance, relativelightorigin);
			RSurf_ActiveModelEntity(ent, false, false, false);
			ent->model->DrawShadowVolume(ent, relativelightorigin, relativelightdirection, relativethrowdistance, ent->model->nummodelsurfaces, ent->model->sortedmodelsurfaces, relativeshadowmins, relativeshadowmaxs);
			rsurface.entity = NULL; // used only by R_GetCurrentTexture and RSurf_ActiveWorldEntity/RSurf_ActiveModelEntity
		}
	}

	// not really the right mode, but this will disable any silly stencil features
	R_Shadow_RenderMode_End();

	// set up ortho view for rendering this pass
	//GL_Scissor(r_refdef.view.x, vid.height - r_refdef.view.height - r_refdef.view.y, r_refdef.view.width, r_refdef.view.height);
	//GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	//GL_ScissorTest(true);
	//R_EntityMatrix(&identitymatrix);
	//R_Mesh_ResetTextureState();
	R_ResetViewRendering2D();
	R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);

	// set up a darkening blend on shadowed areas
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//GL_DepthRange(0, 1);
	//GL_DepthTest(false);
	//GL_DepthMask(false);
	//GL_PolygonOffset(0, 0);CHECKGLERROR
	GL_Color(0, 0, 0, r_shadows_darken.value);
	//GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	//qglDepthFunc(GL_ALWAYS);CHECKGLERROR
	qglEnable(GL_STENCIL_TEST);CHECKGLERROR
	qglStencilMask(255);CHECKGLERROR
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);CHECKGLERROR
	qglStencilFunc(GL_NOTEQUAL, 128, 255);CHECKGLERROR

	// apply the blend to the shadowed areas
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);

	// restore the viewport
	R_SetViewport(&r_refdef.view.viewport);

	// restore other state to normal
	//R_Shadow_RenderMode_End();
}

void R_BeginCoronaQuery(rtlight_t *rtlight, float scale, qboolean usequery)
{
	float zdist;
	vec3_t centerorigin;
	float vertex3f[12];
	// if it's too close, skip it
	if (VectorLength(rtlight->currentcolor) < (1.0f / 256.0f))
		return;
	zdist = (DotProduct(rtlight->shadoworigin, r_refdef.view.forward) - DotProduct(r_refdef.view.origin, r_refdef.view.forward));
	if (zdist < 32)
 		return;
	if (usequery && r_numqueries + 2 <= r_maxqueries)
	{
		rtlight->corona_queryindex_allpixels = r_queries[r_numqueries++];
		rtlight->corona_queryindex_visiblepixels = r_queries[r_numqueries++];
		// we count potential samples in the middle of the screen, we count actual samples at the light location, this allows counting potential samples of off-screen lights
		VectorMA(r_refdef.view.origin, zdist, r_refdef.view.forward, centerorigin);

		CHECKGLERROR
		// NOTE: GL_DEPTH_TEST must be enabled or ATI won't count samples, so use qglDepthFunc instead
		qglBeginQueryARB(GL_SAMPLES_PASSED_ARB, rtlight->corona_queryindex_allpixels);
		qglDepthFunc(GL_ALWAYS);
		R_CalcSprite_Vertex3f(vertex3f, centerorigin, r_refdef.view.right, r_refdef.view.up, scale, -scale, -scale, scale);
		R_Mesh_VertexPointer(vertex3f, 0, 0);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		qglEndQueryARB(GL_SAMPLES_PASSED_ARB);
		qglDepthFunc(GL_LEQUAL);
		qglBeginQueryARB(GL_SAMPLES_PASSED_ARB, rtlight->corona_queryindex_visiblepixels);
		R_CalcSprite_Vertex3f(vertex3f, rtlight->shadoworigin, r_refdef.view.right, r_refdef.view.up, scale, -scale, -scale, scale);
		R_Mesh_VertexPointer(vertex3f, 0, 0);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, polygonelement3s, 0, 0);
		qglEndQueryARB(GL_SAMPLES_PASSED_ARB);
		CHECKGLERROR
	}
	rtlight->corona_visibility = bound(0, (zdist - 32) / 32, 1);
}

static float spritetexcoord2f[4*2] = {0, 1, 0, 0, 1, 0, 1, 1};

void R_DrawCorona(rtlight_t *rtlight, float cscale, float scale)
{
	vec3_t color;
	GLint allpixels = 0, visiblepixels = 0;
	// now we have to check the query result
	if (rtlight->corona_queryindex_visiblepixels)
	{
		CHECKGLERROR
		qglGetQueryObjectivARB(rtlight->corona_queryindex_visiblepixels, GL_QUERY_RESULT_ARB, &visiblepixels);
		qglGetQueryObjectivARB(rtlight->corona_queryindex_allpixels, GL_QUERY_RESULT_ARB, &allpixels);
		CHECKGLERROR
		//Con_Printf("%i of %i pixels\n", (int)visiblepixels, (int)allpixels);
		if (visiblepixels < 1 || allpixels < 1)
			return;
		rtlight->corona_visibility *= bound(0, (float)visiblepixels / (float)allpixels, 1);
		cscale *= rtlight->corona_visibility;
	}
	else
	{
		// FIXME: these traces should scan all render entities instead of cl.world
		if (CL_TraceLine(r_refdef.view.origin, rtlight->shadoworigin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction < 1)
			return;
	}
	VectorScale(rtlight->currentcolor, cscale, color);
	if (VectorLength(color) > (1.0f / 256.0f))
	{
		float vertex3f[12];
		qboolean negated = (color[0] + color[1] + color[2] < 0) && vid.support.ext_blend_subtract;
		if(negated)
		{
			VectorNegate(color, color);
			qglBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT);
		}
		R_CalcSprite_Vertex3f(vertex3f, rtlight->shadoworigin, r_refdef.view.right, r_refdef.view.up, scale, -scale, -scale, scale);
		RSurf_ActiveCustomEntity(&identitymatrix, &identitymatrix, RENDER_NODEPTHTEST, 0, color[0], color[1], color[2], 1, 4, vertex3f, spritetexcoord2f, NULL, NULL, NULL, NULL, 2, polygonelement3i, polygonelement3s, false, false);
		R_DrawCustomSurface(r_shadow_lightcorona, &identitymatrix, MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOCULLFACE, 0, 4, 0, 2, false, false);
		if(negated)
			qglBlendEquationEXT(GL_FUNC_ADD_EXT);
	}
}

void R_Shadow_DrawCoronas(void)
{
	int i, flag;
	qboolean usequery;
	size_t lightindex;
	dlight_t *light;
	rtlight_t *rtlight;
	size_t range;
	if (r_coronas.value < (1.0f / 256.0f) && !gl_flashblend.integer)
		return;
	if (r_waterstate.renderingscene)
		return;
	flag = r_refdef.scene.rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	R_EntityMatrix(&identitymatrix);

	range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked

	// check occlusion of coronas
	// use GL_ARB_occlusion_query if available
	// otherwise use raytraces
	r_numqueries = 0;
	usequery = vid.support.arb_occlusion_query && r_coronas_occlusionquery.integer;
	if (usequery)
	{
		GL_ColorMask(0,0,0,0);
		if (r_maxqueries < (range + r_refdef.scene.numlights) * 2)
		if (r_maxqueries < MAX_OCCLUSION_QUERIES)
		{
			i = r_maxqueries;
			r_maxqueries = (range + r_refdef.scene.numlights) * 4;
			r_maxqueries = min(r_maxqueries, MAX_OCCLUSION_QUERIES);
			CHECKGLERROR
			qglGenQueriesARB(r_maxqueries - i, r_queries + i);
			CHECKGLERROR
		}
		RSurf_ActiveWorldEntity();
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_CullFace(GL_NONE);
		GL_DepthMask(false);
		GL_DepthRange(0, 1);
		GL_PolygonOffset(0, 0);
		GL_DepthTest(true);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1);
	}
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		rtlight = &light->rtlight;
		rtlight->corona_visibility = 0;
		rtlight->corona_queryindex_visiblepixels = 0;
		rtlight->corona_queryindex_allpixels = 0;
		if (!(rtlight->flags & flag))
			continue;
		if (rtlight->corona <= 0)
			continue;
		if (r_shadow_debuglight.integer >= 0 && r_shadow_debuglight.integer != (int)lightindex)
			continue;
		R_BeginCoronaQuery(rtlight, rtlight->radius * rtlight->coronasizescale * r_coronas_occlusionsizescale.value, usequery);
	}
	for (i = 0;i < r_refdef.scene.numlights;i++)
	{
		rtlight = r_refdef.scene.lights[i];
		rtlight->corona_visibility = 0;
		rtlight->corona_queryindex_visiblepixels = 0;
		rtlight->corona_queryindex_allpixels = 0;
		if (!(rtlight->flags & flag))
			continue;
		if (rtlight->corona <= 0)
			continue;
		R_BeginCoronaQuery(rtlight, rtlight->radius * rtlight->coronasizescale * r_coronas_occlusionsizescale.value, usequery);
	}
	if (usequery)
		GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);

	// now draw the coronas using the query data for intensity info
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		rtlight = &light->rtlight;
		if (rtlight->corona_visibility <= 0)
			continue;
		R_DrawCorona(rtlight, rtlight->corona * r_coronas.value * 0.25f, rtlight->radius * rtlight->coronasizescale);
	}
	for (i = 0;i < r_refdef.scene.numlights;i++)
	{
		rtlight = r_refdef.scene.lights[i];
		if (rtlight->corona_visibility <= 0)
			continue;
		if (gl_flashblend.integer)
			R_DrawCorona(rtlight, rtlight->corona, rtlight->radius * rtlight->coronasizescale * 2.0f);
		else
			R_DrawCorona(rtlight, rtlight->corona * r_coronas.value * 0.25f, rtlight->radius * rtlight->coronasizescale);
	}
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

rtexture_t *R_Shadow_LoadCubemap(const char *basename)
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
			if ((image_buffer = loadimagepixelsbgra(name, false, false)))
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

		if (!r_shadow_filters_texturepool)
			r_shadow_filters_texturepool = R_AllocTexturePool();
		cubemaptexture = R_LoadTextureCubeMap(r_shadow_filters_texturepool, basename, cubemapsize, cubemappixels, TEXTYPE_BGRA, (gl_texturecompression_lightcubemaps.integer ? TEXF_COMPRESS : 0) | TEXF_FORCELINEAR, NULL);
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

rtexture_t *R_Shadow_Cubemap(const char *basename)
{
	int i;
	for (i = 0;i < numcubemaps;i++)
		if (!strcasecmp(cubemaps[i].basename, basename))
			return cubemaps[i].texture ? cubemaps[i].texture : r_texture_whitecube;
	if (i >= MAX_CUBEMAPS)
		return r_texture_whitecube;
	numcubemaps++;
	strlcpy(cubemaps[i].basename, basename, sizeof(cubemaps[i].basename));
	cubemaps[i].texture = R_Shadow_LoadCubemap(cubemaps[i].basename);
	return cubemaps[i].texture;
}

void R_Shadow_FreeCubemaps(void)
{
	int i;
	for (i = 0;i < numcubemaps;i++)
	{
		if (developer_loading.integer)
			Con_Printf("unloading cubemap \"%s\"\n", cubemaps[i].basename);
		if (cubemaps[i].texture)
			R_FreeTexture(cubemaps[i].texture);
	}

	numcubemaps = 0;
	R_FreeTexturePool(&r_shadow_filters_texturepool);
}

dlight_t *R_Shadow_NewWorldLight(void)
{
	return (dlight_t *)Mem_ExpandableArray_AllocRecord(&r_shadow_worldlightsarray);
}

void R_Shadow_UpdateWorldLight(dlight_t *light, vec3_t origin, vec3_t angles, vec3_t color, vec_t radius, vec_t corona, int style, int shadowenable, const char *cubemapname, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
	matrix4x4_t matrix;
	// validate parameters
	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("R_Shadow_NewWorldLight: invalid light style number %i, must be >= 0 and < %i\n", light->style, MAX_LIGHTSTYLES);
		style = 0;
	}
	if (!cubemapname)
		cubemapname = "";

	// copy to light properties
	VectorCopy(origin, light->origin);
	light->angles[0] = angles[0] - 360 * floor(angles[0] / 360);
	light->angles[1] = angles[1] - 360 * floor(angles[1] / 360);
	light->angles[2] = angles[2] - 360 * floor(angles[2] / 360);
	/*
	light->color[0] = max(color[0], 0);
	light->color[1] = max(color[1], 0);
	light->color[2] = max(color[2], 0);
	*/
	light->color[0] = color[0];
	light->color[1] = color[1];
	light->color[2] = color[2];
	light->radius = max(radius, 0);
	light->style = style;
	light->shadow = shadowenable;
	light->corona = corona;
	strlcpy(light->cubemapname, cubemapname, sizeof(light->cubemapname));
	light->coronasizescale = coronasizescale;
	light->ambientscale = ambientscale;
	light->diffusescale = diffusescale;
	light->specularscale = specularscale;
	light->flags = flags;

	// update renderable light data
	Matrix4x4_CreateFromQuakeEntity(&matrix, light->origin[0], light->origin[1], light->origin[2], light->angles[0], light->angles[1], light->angles[2], light->radius);
	R_RTLight_Update(&light->rtlight, true, &matrix, light->color, light->style, light->cubemapname[0] ? light->cubemapname : NULL, light->shadow, light->corona, light->coronasizescale, light->ambientscale, light->diffusescale, light->specularscale, light->flags);
}

void R_Shadow_FreeWorldLight(dlight_t *light)
{
	if (r_shadow_selectedlight == light)
		r_shadow_selectedlight = NULL;
	R_RTLight_Uncompile(&light->rtlight);
	Mem_ExpandableArray_FreeRecord(&r_shadow_worldlightsarray, light);
}

void R_Shadow_ClearWorldLights(void)
{
	size_t lightindex;
	dlight_t *light;
	size_t range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (light)
			R_Shadow_FreeWorldLight(light);
	}
	r_shadow_selectedlight = NULL;
	R_Shadow_FreeCubemaps();
}

void R_Shadow_SelectLight(dlight_t *light)
{
	if (r_shadow_selectedlight)
		r_shadow_selectedlight->selected = false;
	r_shadow_selectedlight = light;
	if (r_shadow_selectedlight)
		r_shadow_selectedlight->selected = true;
}

void R_Shadow_DrawCursor_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	// this is never batched (there can be only one)
	float vertex3f[12];
	R_CalcSprite_Vertex3f(vertex3f, r_editlights_cursorlocation, r_refdef.view.right, r_refdef.view.up, EDLIGHTSPRSIZE, -EDLIGHTSPRSIZE, -EDLIGHTSPRSIZE, EDLIGHTSPRSIZE);
	RSurf_ActiveCustomEntity(&identitymatrix, &identitymatrix, 0, 0, 1, 1, 1, 1, 4, vertex3f, spritetexcoord2f, NULL, NULL, NULL, NULL, 2, polygonelement3i, polygonelement3s, false, false);
	R_DrawCustomSurface(r_editlights_sprcursor, &identitymatrix, MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOCULLFACE, 0, 4, 0, 2, false, false);
}

void R_Shadow_DrawLightSprite_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	float intensity;
	float s;
	vec3_t spritecolor;
	skinframe_t *skinframe;
	float vertex3f[12];

	// this is never batched (due to the ent parameter changing every time)
	// so numsurfaces == 1 and surfacelist[0] == lightnumber
	const dlight_t *light = (dlight_t *)ent;
	s = EDLIGHTSPRSIZE;

	R_CalcSprite_Vertex3f(vertex3f, light->origin, r_refdef.view.right, r_refdef.view.up, s, -s, -s, s);

	intensity = 0.5f;
	VectorScale(light->color, intensity, spritecolor);
	if (VectorLength(spritecolor) < 0.1732f)
		VectorSet(spritecolor, 0.1f, 0.1f, 0.1f);
	if (VectorLength(spritecolor) > 1.0f)
		VectorNormalize(spritecolor);

	// draw light sprite
	if (light->cubemapname[0] && !light->shadow)
		skinframe = r_editlights_sprcubemapnoshadowlight;
	else if (light->cubemapname[0])
		skinframe = r_editlights_sprcubemaplight;
	else if (!light->shadow)
		skinframe = r_editlights_sprnoshadowlight;
	else
		skinframe = r_editlights_sprlight;

	RSurf_ActiveCustomEntity(&identitymatrix, &identitymatrix, 0, 0, spritecolor[0], spritecolor[1], spritecolor[2], 1, 4, vertex3f, spritetexcoord2f, NULL, NULL, NULL, NULL, 2, polygonelement3i, polygonelement3s, false, false);
	R_DrawCustomSurface(skinframe, &identitymatrix, MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOCULLFACE, 0, 4, 0, 2, false, false);

	// draw selection sprite if light is selected
	if (light->selected)
	{
		RSurf_ActiveCustomEntity(&identitymatrix, &identitymatrix, 0, 0, 1, 1, 1, 1, 4, vertex3f, spritetexcoord2f, NULL, NULL, NULL, NULL, 2, polygonelement3i, polygonelement3s, false, false);
		R_DrawCustomSurface(r_editlights_sprselection, &identitymatrix, MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOCULLFACE, 0, 4, 0, 2, false, false);
		// VorteX todo: add normalmode/realtime mode light overlay sprites?
	}
}

void R_Shadow_DrawLightSprites(void)
{
	size_t lightindex;
	dlight_t *light;
	size_t range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (light)
			R_MeshQueue_AddTransparent(light->origin, R_Shadow_DrawLightSprite_TransparentCallback, (entity_render_t *)light, 5, &light->rtlight);
	}
	R_MeshQueue_AddTransparent(r_editlights_cursorlocation, R_Shadow_DrawCursor_TransparentCallback, NULL, 0, NULL);
}

int R_Shadow_GetRTLightInfo(unsigned int lightindex, float *origin, float *radius, float *color)
{
	unsigned int range;
	dlight_t *light;
	rtlight_t *rtlight;
	range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray);
	if (lightindex >= range)
		return -1;
	light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
	if (!light)
		return 0;
	rtlight = &light->rtlight;
	//if (!(rtlight->flags & flag))
	//	return 0;
	VectorCopy(rtlight->shadoworigin, origin);
	*radius = rtlight->radius;
	VectorCopy(rtlight->color, color);
	return 1;
}

void R_Shadow_SelectLightInView(void)
{
	float bestrating, rating, temp[3];
	dlight_t *best;
	size_t lightindex;
	dlight_t *light;
	size_t range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	best = NULL;
	bestrating = 0;
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		VectorSubtract(light->origin, r_refdef.view.origin, temp);
		rating = (DotProduct(temp, r_refdef.view.forward) / sqrt(DotProduct(temp, temp)));
		if (rating >= 0.95)
		{
			rating /= (1 + 0.0625f * sqrt(DotProduct(temp, temp)));
			if (bestrating < rating && CL_TraceLine(light->origin, r_refdef.view.origin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction == 1.0f)
			{
				bestrating = rating;
				best = light;
			}
		}
	}
	R_Shadow_SelectLight(best);
}

void R_Shadow_LoadWorldLights(void)
{
	int n, a, style, shadow, flags;
	char tempchar, *lightsstring, *s, *t, name[MAX_QPATH], cubemapname[MAX_QPATH];
	float origin[3], radius, color[3], angles[3], corona, coronasizescale, ambientscale, diffusescale, specularscale;
	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	lightsstring = (char *)FS_LoadFile(name, tempmempool, false, NULL);
	if (lightsstring)
	{
		s = lightsstring;
		n = 0;
		while (*s)
		{
			t = s;
			/*
			shadow = true;
			for (;COM_Parse(t, true) && strcmp(
			if (COM_Parse(t, true))
			{
				if (com_token[0] == '!')
				{
					shadow = false;
					origin[0] = atof(com_token+1);
				}
				else
					origin[0] = atof(com_token);
				if (Com_Parse(t
			}
			*/
			t = s;
			while (*s && *s != '\n' && *s != '\r')
				s++;
			if (!*s)
				break;
			tempchar = *s;
			shadow = true;
			// check for modifier flags
			if (*t == '!')
			{
				shadow = false;
				t++;
			}
			*s = 0;
#if _MSC_VER >= 1400
#define sscanf sscanf_s
#endif
			cubemapname[sizeof(cubemapname)-1] = 0;
#if MAX_QPATH != 128
#error update this code if MAX_QPATH changes
#endif
			a = sscanf(t, "%f %f %f %f %f %f %f %d %127s %f %f %f %f %f %f %f %f %i", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname
#if _MSC_VER >= 1400
, sizeof(cubemapname)
#endif
, &corona, &angles[0], &angles[1], &angles[2], &coronasizescale, &ambientscale, &diffusescale, &specularscale, &flags);
			*s = tempchar;
			if (a < 18)
				flags = LIGHTFLAG_REALTIMEMODE;
			if (a < 17)
				specularscale = 1;
			if (a < 16)
				diffusescale = 1;
			if (a < 15)
				ambientscale = 0;
			if (a < 14)
				coronasizescale = 0.25f;
			if (a < 13)
				VectorClear(angles);
			if (a < 10)
				corona = 0;
			if (a < 9 || !strcmp(cubemapname, "\"\""))
				cubemapname[0] = 0;
			// remove quotes on cubemapname
			if (cubemapname[0] == '"' && cubemapname[strlen(cubemapname) - 1] == '"')
			{
				size_t namelen;
				namelen = strlen(cubemapname) - 2;
				memmove(cubemapname, cubemapname + 1, namelen);
				cubemapname[namelen] = '\0';
			}
			if (a < 8)
			{
				Con_Printf("found %d parameters on line %i, should be 8 or more parameters (origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style \"cubemapname\" corona angles[0] angles[1] angles[2] coronasizescale ambientscale diffusescale specularscale flags)\n", a, n + 1);
				break;
			}
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, corona, style, shadow, cubemapname, coronasizescale, ambientscale, diffusescale, specularscale, flags);
			if (*s == '\r')
				s++;
			if (*s == '\n')
				s++;
			n++;
		}
		if (*s)
			Con_Printf("invalid rtlights file \"%s\"\n", name);
		Mem_Free(lightsstring);
	}
}

void R_Shadow_SaveWorldLights(void)
{
	size_t lightindex;
	dlight_t *light;
	size_t bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[MAX_INPUTLINE];
	size_t range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked, assuming the dpsnprintf mess doesn't screw it up...
	// I hate lines which are 3 times my screen size :( --blub
	if (!range)
		return;
	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		if (light->coronasizescale != 0.25f || light->ambientscale != 0 || light->diffusescale != 1 || light->specularscale != 1 || light->flags != LIGHTFLAG_REALTIMEMODE)
			dpsnprintf(line, sizeof(line), "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f %f %f %f %f %i\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style, light->cubemapname, light->corona, light->angles[0], light->angles[1], light->angles[2], light->coronasizescale, light->ambientscale, light->diffusescale, light->specularscale, light->flags);
		else if (light->cubemapname[0] || light->corona || light->angles[0] || light->angles[1] || light->angles[2])
			dpsnprintf(line, sizeof(line), "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style, light->cubemapname, light->corona, light->angles[0], light->angles[1], light->angles[2]);
		else
			dpsnprintf(line, sizeof(line), "%s%f %f %f %f %f %f %f %d\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style);
		if (bufchars + strlen(line) > bufmaxchars)
		{
			bufmaxchars = bufchars + strlen(line) + 2048;
			oldbuf = buf;
			buf = (char *)Mem_Alloc(tempmempool, bufmaxchars);
			if (oldbuf)
			{
				if (bufchars)
					memcpy(buf, oldbuf, bufchars);
				Mem_Free(oldbuf);
			}
		}
		if (strlen(line))
		{
			memcpy(buf + bufchars, line, strlen(line));
			bufchars += strlen(line);
		}
	}
	if (bufchars)
		FS_WriteFile(name, buf, (fs_offset_t)bufchars);
	if (buf)
		Mem_Free(buf);
}

void R_Shadow_LoadLightsFile(void)
{
	int n, a, style;
	char tempchar, *lightsstring, *s, *t, name[MAX_QPATH];
	float origin[3], radius, color[3], subtract, spotdir[3], spotcone, falloff, distbias;
	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".lights", sizeof (name));
	lightsstring = (char *)FS_LoadFile(name, tempmempool, false, NULL);
	if (lightsstring)
	{
		s = lightsstring;
		n = 0;
		while (*s)
		{
			t = s;
			while (*s && *s != '\n' && *s != '\r')
				s++;
			if (!*s)
				break;
			tempchar = *s;
			*s = 0;
			a = sscanf(t, "%f %f %f %f %f %f %f %f %f %f %f %f %f %d", &origin[0], &origin[1], &origin[2], &falloff, &color[0], &color[1], &color[2], &subtract, &spotdir[0], &spotdir[1], &spotdir[2], &spotcone, &distbias, &style);
			*s = tempchar;
			if (a < 14)
			{
				Con_Printf("invalid lights file, found %d parameters on line %i, should be 14 parameters (origin[0] origin[1] origin[2] falloff light[0] light[1] light[2] subtract spotdir[0] spotdir[1] spotdir[2] spotcone distancebias style)\n", a, n + 1);
				break;
			}
			radius = sqrt(DotProduct(color, color) / (falloff * falloff * 8192.0f * 8192.0f));
			radius = bound(15, radius, 4096);
			VectorScale(color, (2.0f / (8388608.0f)), color);
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, vec3_origin, color, radius, 0, style, true, NULL, 0.25, 0, 1, 1, LIGHTFLAG_REALTIMEMODE);
			if (*s == '\r')
				s++;
			if (*s == '\n')
				s++;
			n++;
		}
		if (*s)
			Con_Printf("invalid lights file \"%s\"\n", name);
		Mem_Free(lightsstring);
	}
}

// tyrlite/hmap2 light types in the delay field
typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void)
{
	int entnum, style, islight, skin, pflags, effects, type, n;
	char *entfiledata;
	const char *data;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], vec[4];
	char key[256], value[MAX_INPUTLINE];

	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	// try to load a .ent file first
	FS_StripExtension (cl.worldmodel->name, key, sizeof (key));
	strlcat (key, ".ent", sizeof (key));
	data = entfiledata = (char *)FS_LoadFile(key, tempmempool, true, NULL);
	// and if that is not found, fall back to the bsp file entity string
	if (!data)
		data = cl.worldmodel->brush.entities;
	if (!data)
		return;
	for (entnum = 0;COM_ParseToken_Simple(&data, false, false) && com_token[0] == '{';entnum++)
	{
		type = LIGHTTYPE_MINUSX;
		origin[0] = origin[1] = origin[2] = 0;
		originhack[0] = originhack[1] = originhack[2] = 0;
		angles[0] = angles[1] = angles[2] = 0;
		color[0] = color[1] = color[2] = 1;
		light[0] = light[1] = light[2] = 1;light[3] = 300;
		overridecolor[0] = overridecolor[1] = overridecolor[2] = 1;
		fadescale = 1;
		lightscale = 1;
		style = 0;
		skin = 0;
		pflags = 0;
		effects = 0;
		islight = false;
		while (1)
		{
			if (!COM_ParseToken_Simple(&data, false, false))
				break; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '_')
				strlcpy(key, com_token + 1, sizeof(key));
			else
				strlcpy(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken_Simple(&data, false, false))
				break; // error
			strlcpy(value, com_token, sizeof(value));

			// now that we have the key pair worked out...
			if (!strcmp("light", key))
			{
				n = sscanf(value, "%f %f %f %f", &vec[0], &vec[1], &vec[2], &vec[3]);
				if (n == 1)
				{
					// quake
					light[0] = vec[0] * (1.0f / 256.0f);
					light[1] = vec[0] * (1.0f / 256.0f);
					light[2] = vec[0] * (1.0f / 256.0f);
					light[3] = vec[0];
				}
				else if (n == 4)
				{
					// halflife
					light[0] = vec[0] * (1.0f / 255.0f);
					light[1] = vec[1] * (1.0f / 255.0f);
					light[2] = vec[2] * (1.0f / 255.0f);
					light[3] = vec[3];
				}
			}
			else if (!strcmp("delay", key))
				type = atoi(value);
			else if (!strcmp("origin", key))
				sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp("angle", key))
				angles[0] = 0, angles[1] = atof(value), angles[2] = 0;
			else if (!strcmp("angles", key))
				sscanf(value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
			else if (!strcmp("color", key))
				sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
			else if (!strcmp("wait", key))
				fadescale = atof(value);
			else if (!strcmp("classname", key))
			{
				if (!strncmp(value, "light", 5))
				{
					islight = true;
					if (!strcmp(value, "light_fluoro"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_fluorospark"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_globe"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.8;
						overridecolor[2] = 0.4;
					}
					if (!strcmp(value, "light_flame_large_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
			else if (!strcmp("skin", key))
				skin = (int)atof(value);
			else if (!strcmp("pflags", key))
				pflags = (int)atof(value);
			else if (!strcmp("effects", key))
				effects = (int)atof(value);
			else if (cl.worldmodel->type == mod_brushq3)
			{
				if (!strcmp("scale", key))
					lightscale = atof(value);
				if (!strcmp("fade", key))
					fadescale = atof(value);
			}
		}
		if (!islight)
			continue;
		if (lightscale <= 0)
			lightscale = 1;
		if (fadescale <= 0)
			fadescale = 1;
		if (color[0] == color[1] && color[0] == color[2])
		{
			color[0] *= overridecolor[0];
			color[1] *= overridecolor[1];
			color[2] *= overridecolor[2];
		}
		radius = light[3] * r_editlights_quakelightsizescale.value * lightscale / fadescale;
		color[0] = color[0] * light[0];
		color[1] = color[1] * light[1];
		color[2] = color[2] * light[2];
		switch (type)
		{
		case LIGHTTYPE_MINUSX:
			break;
		case LIGHTTYPE_RECIPX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		case LIGHTTYPE_RECIPXX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		default:
		case LIGHTTYPE_NONE:
			break;
		case LIGHTTYPE_SUN:
			break;
		case LIGHTTYPE_MINUSXX:
			break;
		}
		VectorAdd(origin, originhack, origin);
		if (radius >= 1)
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, (pflags & PFLAGS_CORONA) != 0, style, (pflags & PFLAGS_NOSHADOW) == 0, skin >= 16 ? va("cubemaps/%i", skin) : NULL, 0.25, 0, 1, 1, LIGHTFLAG_REALTIMEMODE);
	}
	if (entfiledata)
		Mem_Free(entfiledata);
}


void R_Shadow_SetCursorLocationForView(void)
{
	vec_t dist, push;
	vec3_t dest, endpos;
	trace_t trace;
	VectorMA(r_refdef.view.origin, r_editlights_cursordistance.value, r_refdef.view.forward, dest);
	trace = CL_TraceLine(r_refdef.view.origin, dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false);
	if (trace.fraction < 1)
	{
		dist = trace.fraction * r_editlights_cursordistance.value;
		push = r_editlights_cursorpushback.value;
		if (push > dist)
			push = dist;
		push = -push;
		VectorMA(trace.endpos, push, r_refdef.view.forward, endpos);
		VectorMA(endpos, r_editlights_cursorpushoff.value, trace.plane.normal, endpos);
	}
	else
	{
		VectorClear( endpos );
	}
	r_editlights_cursorlocation[0] = floor(endpos[0] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
	r_editlights_cursorlocation[1] = floor(endpos[1] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
	r_editlights_cursorlocation[2] = floor(endpos[2] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
}

void R_Shadow_UpdateWorldLightSelection(void)
{
	if (r_editlights.integer)
	{
		R_Shadow_SetCursorLocationForView();
		R_Shadow_SelectLightInView();
	}
	else
		R_Shadow_SelectLight(NULL);
}

void R_Shadow_EditLights_Clear_f(void)
{
	R_Shadow_ClearWorldLights();
}

void R_Shadow_EditLights_Reload_f(void)
{
	if (!cl.worldmodel)
		return;
	strlcpy(r_shadow_mapname, cl.worldmodel->name, sizeof(r_shadow_mapname));
	R_Shadow_ClearWorldLights();
	R_Shadow_LoadWorldLights();
	if (!Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray))
	{
		R_Shadow_LoadLightsFile();
		if (!Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray))
			R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
	}
}

void R_Shadow_EditLights_Save_f(void)
{
	if (!cl.worldmodel)
		return;
	R_Shadow_SaveWorldLights();
}

void R_Shadow_EditLights_ImportLightEntitiesFromMap_f(void)
{
	R_Shadow_ClearWorldLights();
	R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
}

void R_Shadow_EditLights_ImportLightsFile_f(void)
{
	R_Shadow_ClearWorldLights();
	R_Shadow_LoadLightsFile();
}

void R_Shadow_EditLights_Spawn_f(void)
{
	vec3_t color;
	if (!r_editlights.integer)
	{
		Con_Print("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (Cmd_Argc() != 1)
	{
		Con_Print("r_editlights_spawn does not take parameters\n");
		return;
	}
	color[0] = color[1] = color[2] = 1;
	R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), r_editlights_cursorlocation, vec3_origin, color, 200, 0, 0, true, NULL, 0.25, 0, 1, 1, LIGHTFLAG_REALTIMEMODE);
}

void R_Shadow_EditLights_Edit_f(void)
{
	vec3_t origin, angles, color;
	vec_t radius, corona, coronasizescale, ambientscale, diffusescale, specularscale;
	int style, shadows, flags, normalmode, realtimemode;
	char cubemapname[MAX_INPUTLINE];
	if (!r_editlights.integer)
	{
		Con_Print("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	VectorCopy(r_shadow_selectedlight->origin, origin);
	VectorCopy(r_shadow_selectedlight->angles, angles);
	VectorCopy(r_shadow_selectedlight->color, color);
	radius = r_shadow_selectedlight->radius;
	style = r_shadow_selectedlight->style;
	if (r_shadow_selectedlight->cubemapname)
		strlcpy(cubemapname, r_shadow_selectedlight->cubemapname, sizeof(cubemapname));
	else
		cubemapname[0] = 0;
	shadows = r_shadow_selectedlight->shadow;
	corona = r_shadow_selectedlight->corona;
	coronasizescale = r_shadow_selectedlight->coronasizescale;
	ambientscale = r_shadow_selectedlight->ambientscale;
	diffusescale = r_shadow_selectedlight->diffusescale;
	specularscale = r_shadow_selectedlight->specularscale;
	flags = r_shadow_selectedlight->flags;
	normalmode = (flags & LIGHTFLAG_NORMALMODE) != 0;
	realtimemode = (flags & LIGHTFLAG_REALTIMEMODE) != 0;
	if (!strcmp(Cmd_Argv(1), "origin"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s x y z\n", Cmd_Argv(1));
			return;
		}
		origin[0] = atof(Cmd_Argv(2));
		origin[1] = atof(Cmd_Argv(3));
		origin[2] = atof(Cmd_Argv(4));
	}
	else if (!strcmp(Cmd_Argv(1), "originx"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[0] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "originy"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[1] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "originz"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[2] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "move"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s x y z\n", Cmd_Argv(1));
			return;
		}
		origin[0] += atof(Cmd_Argv(2));
		origin[1] += atof(Cmd_Argv(3));
		origin[2] += atof(Cmd_Argv(4));
	}
	else if (!strcmp(Cmd_Argv(1), "movex"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[0] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "movey"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[1] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "movez"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		origin[2] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "angles"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s x y z\n", Cmd_Argv(1));
			return;
		}
		angles[0] = atof(Cmd_Argv(2));
		angles[1] = atof(Cmd_Argv(3));
		angles[2] = atof(Cmd_Argv(4));
	}
	else if (!strcmp(Cmd_Argv(1), "anglesx"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		angles[0] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "anglesy"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		angles[1] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "anglesz"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		angles[2] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "color"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s red green blue\n", Cmd_Argv(1));
			return;
		}
		color[0] = atof(Cmd_Argv(2));
		color[1] = atof(Cmd_Argv(3));
		color[2] = atof(Cmd_Argv(4));
	}
	else if (!strcmp(Cmd_Argv(1), "radius"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		radius = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "colorscale"))
	{
		if (Cmd_Argc() == 3)
		{
			double scale = atof(Cmd_Argv(2));
			color[0] *= scale;
			color[1] *= scale;
			color[2] *= scale;
		}
		else
		{
			if (Cmd_Argc() != 5)
			{
				Con_Printf("usage: r_editlights_edit %s red green blue  (OR grey instead of red green blue)\n", Cmd_Argv(1));
				return;
			}
			color[0] *= atof(Cmd_Argv(2));
			color[1] *= atof(Cmd_Argv(3));
			color[2] *= atof(Cmd_Argv(4));
		}
	}
	else if (!strcmp(Cmd_Argv(1), "radiusscale") || !strcmp(Cmd_Argv(1), "sizescale"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		radius *= atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "style"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		style = atoi(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "cubemap"))
	{
		if (Cmd_Argc() > 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		if (Cmd_Argc() == 3)
			strlcpy(cubemapname, Cmd_Argv(2), sizeof(cubemapname));
		else
			cubemapname[0] = 0;
	}
	else if (!strcmp(Cmd_Argv(1), "shadows"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		shadows = Cmd_Argv(2)[0] == 'y' || Cmd_Argv(2)[0] == 'Y' || Cmd_Argv(2)[0] == 't' || atoi(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "corona"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		corona = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "coronasize"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		coronasizescale = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "ambient"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		ambientscale = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "diffuse"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		diffusescale = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "specular"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		specularscale = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "normalmode"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		normalmode = Cmd_Argv(2)[0] == 'y' || Cmd_Argv(2)[0] == 'Y' || Cmd_Argv(2)[0] == 't' || atoi(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "realtimemode"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(1));
			return;
		}
		realtimemode = Cmd_Argv(2)[0] == 'y' || Cmd_Argv(2)[0] == 'Y' || Cmd_Argv(2)[0] == 't' || atoi(Cmd_Argv(2));
	}
	else
	{
		Con_Print("usage: r_editlights_edit [property] [value]\n");
		Con_Print("Selected light's properties:\n");
		Con_Printf("Origin       : %f %f %f\n", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);
		Con_Printf("Angles       : %f %f %f\n", r_shadow_selectedlight->angles[0], r_shadow_selectedlight->angles[1], r_shadow_selectedlight->angles[2]);
		Con_Printf("Color        : %f %f %f\n", r_shadow_selectedlight->color[0], r_shadow_selectedlight->color[1], r_shadow_selectedlight->color[2]);
		Con_Printf("Radius       : %f\n", r_shadow_selectedlight->radius);
		Con_Printf("Corona       : %f\n", r_shadow_selectedlight->corona);
		Con_Printf("Style        : %i\n", r_shadow_selectedlight->style);
		Con_Printf("Shadows      : %s\n", r_shadow_selectedlight->shadow ? "yes" : "no");
		Con_Printf("Cubemap      : %s\n", r_shadow_selectedlight->cubemapname);
		Con_Printf("CoronaSize   : %f\n", r_shadow_selectedlight->coronasizescale);
		Con_Printf("Ambient      : %f\n", r_shadow_selectedlight->ambientscale);
		Con_Printf("Diffuse      : %f\n", r_shadow_selectedlight->diffusescale);
		Con_Printf("Specular     : %f\n", r_shadow_selectedlight->specularscale);
		Con_Printf("NormalMode   : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_NORMALMODE) ? "yes" : "no");
		Con_Printf("RealTimeMode : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_REALTIMEMODE) ? "yes" : "no");
		return;
	}
	flags = (normalmode ? LIGHTFLAG_NORMALMODE : 0) | (realtimemode ? LIGHTFLAG_REALTIMEMODE : 0);
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, origin, angles, color, radius, corona, style, shadows, cubemapname, coronasizescale, ambientscale, diffusescale, specularscale, flags);
}

void R_Shadow_EditLights_EditAll_f(void)
{
	size_t lightindex;
	dlight_t *light;
	size_t range;

	if (!r_editlights.integer)
	{
		Con_Print("Cannot edit lights when not in editing mode. Set r_editlights to 1.\n");
		return;
	}

	// EditLights doesn't seem to have a "remove" command or something so:
	range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		R_Shadow_SelectLight(light);
		R_Shadow_EditLights_Edit_f();
	}
}

void R_Shadow_EditLights_DrawSelectedLightProperties(void)
{
	int lightnumber, lightcount;
	size_t lightindex, range;
	dlight_t *light;
	float x, y;
	char temp[256];
	if (!r_editlights.integer)
		return;
	x = vid_conwidth.value - 240;
	y = 5;
	DrawQ_Pic(x-5, y-5, NULL, 250, 155, 0, 0, 0, 0.75, 0);
	lightnumber = -1;
	lightcount = 0;
	range = Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray); // checked
	for (lightindex = 0;lightindex < range;lightindex++)
	{
		light = (dlight_t *) Mem_ExpandableArray_RecordAtIndex(&r_shadow_worldlightsarray, lightindex);
		if (!light)
			continue;
		if (light == r_shadow_selectedlight)
			lightnumber = lightindex;
		lightcount++;
	}
	dpsnprintf(temp, sizeof(temp), "Cursor origin: %.0f %.0f %.0f", r_editlights_cursorlocation[0], r_editlights_cursorlocation[1], r_editlights_cursorlocation[2]); DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, false, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Total lights : %i active (%i total)", lightcount, (int)Mem_ExpandableArray_IndexRange(&r_shadow_worldlightsarray)); DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, false, FONT_DEFAULT);y += 8;
	y += 8;
	if (r_shadow_selectedlight == NULL)
		return;
	dpsnprintf(temp, sizeof(temp), "Light #%i properties:", lightnumber);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Origin       : %.0f %.0f %.0f\n", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Angles       : %.0f %.0f %.0f\n", r_shadow_selectedlight->angles[0], r_shadow_selectedlight->angles[1], r_shadow_selectedlight->angles[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Color        : %.2f %.2f %.2f\n", r_shadow_selectedlight->color[0], r_shadow_selectedlight->color[1], r_shadow_selectedlight->color[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Radius       : %.0f\n", r_shadow_selectedlight->radius);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Corona       : %.0f\n", r_shadow_selectedlight->corona);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Style        : %i\n", r_shadow_selectedlight->style);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Shadows      : %s\n", r_shadow_selectedlight->shadow ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Cubemap      : %s\n", r_shadow_selectedlight->cubemapname);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "CoronaSize   : %.2f\n", r_shadow_selectedlight->coronasizescale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Ambient      : %.2f\n", r_shadow_selectedlight->ambientscale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Diffuse      : %.2f\n", r_shadow_selectedlight->diffusescale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "Specular     : %.2f\n", r_shadow_selectedlight->specularscale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "NormalMode   : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_NORMALMODE) ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
	dpsnprintf(temp, sizeof(temp), "RealTimeMode : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_REALTIMEMODE) ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);y += 8;
}

void R_Shadow_EditLights_ToggleShadow_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Print("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_selectedlight->angles, r_shadow_selectedlight->color, r_shadow_selectedlight->radius, r_shadow_selectedlight->corona, r_shadow_selectedlight->style, !r_shadow_selectedlight->shadow, r_shadow_selectedlight->cubemapname, r_shadow_selectedlight->coronasizescale, r_shadow_selectedlight->ambientscale, r_shadow_selectedlight->diffusescale, r_shadow_selectedlight->specularscale, r_shadow_selectedlight->flags);
}

void R_Shadow_EditLights_ToggleCorona_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Print("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_selectedlight->angles, r_shadow_selectedlight->color, r_shadow_selectedlight->radius, !r_shadow_selectedlight->corona, r_shadow_selectedlight->style, r_shadow_selectedlight->shadow, r_shadow_selectedlight->cubemapname, r_shadow_selectedlight->coronasizescale, r_shadow_selectedlight->ambientscale, r_shadow_selectedlight->diffusescale, r_shadow_selectedlight->specularscale, r_shadow_selectedlight->flags);
}

void R_Shadow_EditLights_Remove_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Print("Cannot remove light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	R_Shadow_FreeWorldLight(r_shadow_selectedlight);
	r_shadow_selectedlight = NULL;
}

void R_Shadow_EditLights_Help_f(void)
{
	Con_Print(
"Documentation on r_editlights system:\n"
"Settings:\n"
"r_editlights : enable/disable editing mode\n"
"r_editlights_cursordistance : maximum distance of cursor from eye\n"
"r_editlights_cursorpushback : push back cursor this far from surface\n"
"r_editlights_cursorpushoff : push cursor off surface this far\n"
"r_editlights_cursorgrid : snap cursor to grid of this size\n"
"r_editlights_quakelightsizescale : imported quake light entity size scaling\n"
"Commands:\n"
"r_editlights_help : this help\n"
"r_editlights_clear : remove all lights\n"
"r_editlights_reload : reload .rtlights, .lights file, or entities\n"
"r_editlights_save : save to .rtlights file\n"
"r_editlights_spawn : create a light with default settings\n"
"r_editlights_edit command : edit selected light - more documentation below\n"
"r_editlights_remove : remove selected light\n"
"r_editlights_toggleshadow : toggles on/off selected light's shadow property\n"
"r_editlights_importlightentitiesfrommap : reload light entities\n"
"r_editlights_importlightsfile : reload .light file (produced by hlight)\n"
"Edit commands:\n"
"origin x y z : set light location\n"
"originx x: set x component of light location\n"
"originy y: set y component of light location\n"
"originz z: set z component of light location\n"
"move x y z : adjust light location\n"
"movex x: adjust x component of light location\n"
"movey y: adjust y component of light location\n"
"movez z: adjust z component of light location\n"
"angles x y z : set light angles\n"
"anglesx x: set x component of light angles\n"
"anglesy y: set y component of light angles\n"
"anglesz z: set z component of light angles\n"
"color r g b : set color of light (can be brighter than 1 1 1)\n"
"radius radius : set radius (size) of light\n"
"colorscale grey : multiply color of light (1 does nothing)\n"
"colorscale r g b : multiply color of light (1 1 1 does nothing)\n"
"radiusscale scale : multiply radius (size) of light (1 does nothing)\n"
"sizescale scale : multiply radius (size) of light (1 does nothing)\n"
"style style : set lightstyle of light (flickering patterns, switches, etc)\n"
"cubemap basename : set filter cubemap of light (not yet supported)\n"
"shadows 1/0 : turn on/off shadows\n"
"corona n : set corona intensity\n"
"coronasize n : set corona size (0-1)\n"
"ambient n : set ambient intensity (0-1)\n"
"diffuse n : set diffuse intensity (0-1)\n"
"specular n : set specular intensity (0-1)\n"
"normalmode 1/0 : turn on/off rendering of this light in rtworld 0 mode\n"
"realtimemode 1/0 : turn on/off rendering of this light in rtworld 1 mode\n"
"<nothing> : print light properties to console\n"
	);
}

void R_Shadow_EditLights_CopyInfo_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Print("Cannot copy light info when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	VectorCopy(r_shadow_selectedlight->angles, r_shadow_bufferlight.angles);
	VectorCopy(r_shadow_selectedlight->color, r_shadow_bufferlight.color);
	r_shadow_bufferlight.radius = r_shadow_selectedlight->radius;
	r_shadow_bufferlight.style = r_shadow_selectedlight->style;
	if (r_shadow_selectedlight->cubemapname)
		strlcpy(r_shadow_bufferlight.cubemapname, r_shadow_selectedlight->cubemapname, sizeof(r_shadow_bufferlight.cubemapname));
	else
		r_shadow_bufferlight.cubemapname[0] = 0;
	r_shadow_bufferlight.shadow = r_shadow_selectedlight->shadow;
	r_shadow_bufferlight.corona = r_shadow_selectedlight->corona;
	r_shadow_bufferlight.coronasizescale = r_shadow_selectedlight->coronasizescale;
	r_shadow_bufferlight.ambientscale = r_shadow_selectedlight->ambientscale;
	r_shadow_bufferlight.diffusescale = r_shadow_selectedlight->diffusescale;
	r_shadow_bufferlight.specularscale = r_shadow_selectedlight->specularscale;
	r_shadow_bufferlight.flags = r_shadow_selectedlight->flags;
}

void R_Shadow_EditLights_PasteInfo_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Print("Cannot paste light info when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Print("No selected light.\n");
		return;
	}
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_bufferlight.angles, r_shadow_bufferlight.color, r_shadow_bufferlight.radius, r_shadow_bufferlight.corona, r_shadow_bufferlight.style, r_shadow_bufferlight.shadow, r_shadow_bufferlight.cubemapname, r_shadow_bufferlight.coronasizescale, r_shadow_bufferlight.ambientscale, r_shadow_bufferlight.diffusescale, r_shadow_bufferlight.specularscale, r_shadow_bufferlight.flags);
}

void R_Shadow_EditLights_Init(void)
{
	Cvar_RegisterVariable(&r_editlights);
	Cvar_RegisterVariable(&r_editlights_cursordistance);
	Cvar_RegisterVariable(&r_editlights_cursorpushback);
	Cvar_RegisterVariable(&r_editlights_cursorpushoff);
	Cvar_RegisterVariable(&r_editlights_cursorgrid);
	Cvar_RegisterVariable(&r_editlights_quakelightsizescale);
	Cmd_AddCommand("r_editlights_help", R_Shadow_EditLights_Help_f, "prints documentation on console commands and variables in rtlight editing system");
	Cmd_AddCommand("r_editlights_clear", R_Shadow_EditLights_Clear_f, "removes all world lights (let there be darkness!)");
	Cmd_AddCommand("r_editlights_reload", R_Shadow_EditLights_Reload_f, "reloads rtlights file (or imports from .lights file or .ent file or the map itself)");
	Cmd_AddCommand("r_editlights_save", R_Shadow_EditLights_Save_f, "save .rtlights file for current level");
	Cmd_AddCommand("r_editlights_spawn", R_Shadow_EditLights_Spawn_f, "creates a light with default properties (let there be light!)");
	Cmd_AddCommand("r_editlights_edit", R_Shadow_EditLights_Edit_f, "changes a property on the selected light");
	Cmd_AddCommand("r_editlights_editall", R_Shadow_EditLights_EditAll_f, "changes a property on ALL lights at once (tip: use radiusscale and colorscale to alter these properties)");
	Cmd_AddCommand("r_editlights_remove", R_Shadow_EditLights_Remove_f, "remove selected light");
	Cmd_AddCommand("r_editlights_toggleshadow", R_Shadow_EditLights_ToggleShadow_f, "toggle on/off the shadow option on the selected light");
	Cmd_AddCommand("r_editlights_togglecorona", R_Shadow_EditLights_ToggleCorona_f, "toggle on/off the corona option on the selected light");
	Cmd_AddCommand("r_editlights_importlightentitiesfrommap", R_Shadow_EditLights_ImportLightEntitiesFromMap_f, "load lights from .ent file or map entities (ignoring .rtlights or .lights file)");
	Cmd_AddCommand("r_editlights_importlightsfile", R_Shadow_EditLights_ImportLightsFile_f, "load lights from .lights file (ignoring .rtlights or .ent files and map entities)");
	Cmd_AddCommand("r_editlights_copyinfo", R_Shadow_EditLights_CopyInfo_f, "store a copy of all properties (except origin) of the selected light");
	Cmd_AddCommand("r_editlights_pasteinfo", R_Shadow_EditLights_PasteInfo_f, "apply the stored properties onto the selected light (making it exactly identical except for origin)");
}



/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic)
{
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);

	if (!r_fullbright.integer && r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.LightPoint)
	{
		ambientcolor[0] = ambientcolor[1] = ambientcolor[2] = r_refdef.scene.ambient;
		r_refdef.scene.worldmodel->brush.LightPoint(r_refdef.scene.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	}
	else
		VectorSet(ambientcolor, 1, 1, 1);

	if (dynamic)
	{
		int i;
		float f, v[3];
		rtlight_t *light;
		for (i = 0;i < r_refdef.scene.numlights;i++)
		{
			light = r_refdef.scene.lights[i];
			Matrix4x4_Transform(&light->matrix_worldtolight, p, v);
			f = 1 - VectorLength2(v);
			if (f > 0 && CL_TraceLine(p, light->shadoworigin, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false).fraction == 1)
				VectorMA(ambientcolor, f, light->currentcolor, ambientcolor);
		}
	}
}
