
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

typedef enum r_shadowstage_e
{
	R_SHADOWSTAGE_NONE,
	R_SHADOWSTAGE_STENCIL,
	R_SHADOWSTAGE_STENCILTWOSIDE,
	R_SHADOWSTAGE_LIGHT_VERTEX,
	R_SHADOWSTAGE_LIGHT_DOT3,
	R_SHADOWSTAGE_LIGHT_GLSL,
	R_SHADOWSTAGE_VISIBLEVOLUMES,
	R_SHADOWSTAGE_VISIBLELIGHTING,
}
r_shadowstage_t;

r_shadowstage_t r_shadowstage = R_SHADOWSTAGE_NONE;

mempool_t *r_shadow_mempool;

int maxshadowelements;
int *shadowelements;

int maxshadowmark;
int numshadowmark;
int *shadowmark;
int *shadowmarklist;
int shadowmarkcount;

int maxvertexupdate;
int *vertexupdate;
int *vertexremap;
int vertexupdatenum;

int r_shadow_buffer_numleafpvsbytes;
qbyte *r_shadow_buffer_leafpvs;
int *r_shadow_buffer_leaflist;

int r_shadow_buffer_numsurfacepvsbytes;
qbyte *r_shadow_buffer_surfacepvs;
int *r_shadow_buffer_surfacelist;

rtexturepool_t *r_shadow_texturepool;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_attenuation3dtexture;

// lights are reloaded when this changes
char r_shadow_mapname[MAX_QPATH];

// used only for light filters (cubemaps)
rtexturepool_t *r_shadow_filters_texturepool;

cvar_t r_shadow_bumpscale_basetexture = {0, "r_shadow_bumpscale_basetexture", "0"};
cvar_t r_shadow_bumpscale_bumpmap = {0, "r_shadow_bumpscale_bumpmap", "4"};
cvar_t r_shadow_debuglight = {0, "r_shadow_debuglight", "-1"};
cvar_t r_shadow_gloss = {CVAR_SAVE, "r_shadow_gloss", "1"};
cvar_t r_shadow_gloss2intensity = {0, "r_shadow_gloss2intensity", "0.25"};
cvar_t r_shadow_glossintensity = {0, "r_shadow_glossintensity", "1"};
cvar_t r_shadow_lightattenuationpower = {0, "r_shadow_lightattenuationpower", "0.5"};
cvar_t r_shadow_lightattenuationscale = {0, "r_shadow_lightattenuationscale", "1"};
cvar_t r_shadow_lightintensityscale = {0, "r_shadow_lightintensityscale", "1"};
cvar_t r_shadow_portallight = {0, "r_shadow_portallight", "1"};
cvar_t r_shadow_projectdistance = {0, "r_shadow_projectdistance", "1000000"};
cvar_t r_shadow_realtime_dlight = {CVAR_SAVE, "r_shadow_realtime_dlight", "1"};
cvar_t r_shadow_realtime_dlight_shadows = {CVAR_SAVE, "r_shadow_realtime_dlight_shadows", "1"};
cvar_t r_shadow_realtime_dlight_portalculling = {0, "r_shadow_realtime_dlight_portalculling", "0"};
cvar_t r_shadow_realtime_world = {CVAR_SAVE, "r_shadow_realtime_world", "0"};
cvar_t r_shadow_realtime_world_dlightshadows = {CVAR_SAVE, "r_shadow_realtime_world_dlightshadows", "1"};
cvar_t r_shadow_realtime_world_lightmaps = {CVAR_SAVE, "r_shadow_realtime_world_lightmaps", "0"};
cvar_t r_shadow_realtime_world_shadows = {CVAR_SAVE, "r_shadow_realtime_world_shadows", "1"};
cvar_t r_shadow_realtime_world_compile = {0, "r_shadow_realtime_world_compile", "1"};
cvar_t r_shadow_realtime_world_compileshadow = {0, "r_shadow_realtime_world_compileshadow", "1"};
cvar_t r_shadow_scissor = {0, "r_shadow_scissor", "1"};
cvar_t r_shadow_shadow_polygonfactor = {0, "r_shadow_shadow_polygonfactor", "0"};
cvar_t r_shadow_shadow_polygonoffset = {0, "r_shadow_shadow_polygonoffset", "1"};
cvar_t r_shadow_singlepassvolumegeneration = {0, "r_shadow_singlepassvolumegeneration", "1"};
cvar_t r_shadow_texture3d = {0, "r_shadow_texture3d", "1"};
cvar_t r_shadow_visiblelighting = {0, "r_shadow_visiblelighting", "0"};
cvar_t r_shadow_visiblevolumes = {0, "r_shadow_visiblevolumes", "0"};
cvar_t r_shadow_glsl = {0, "r_shadow_glsl", "1"};
cvar_t r_shadow_glsl_offsetmapping = {0, "r_shadow_glsl_offsetmapping", "0"};
cvar_t r_shadow_glsl_offsetmapping_scale = {0, "r_shadow_glsl_offsetmapping_scale", "-0.04"};
cvar_t r_shadow_glsl_offsetmapping_bias = {0, "r_shadow_glsl_offsetmapping_bias", "0.04"};
cvar_t r_shadow_glsl_usehalffloat = {0, "r_shadow_glsl_usehalffloat", "0"};
cvar_t r_shadow_glsl_surfacenormalize = {0, "r_shadow_glsl_surfacenormalize", "1"};
cvar_t gl_ext_stenciltwoside = {0, "gl_ext_stenciltwoside", "1"};
cvar_t r_editlights = {0, "r_editlights", "0"};
cvar_t r_editlights_cursordistance = {0, "r_editlights_cursordistance", "1024"};
cvar_t r_editlights_cursorpushback = {0, "r_editlights_cursorpushback", "0"};
cvar_t r_editlights_cursorpushoff = {0, "r_editlights_cursorpushoff", "4"};
cvar_t r_editlights_cursorgrid = {0, "r_editlights_cursorgrid", "4"};
cvar_t r_editlights_quakelightsizescale = {CVAR_SAVE, "r_editlights_quakelightsizescale", "0.8"};

float r_shadow_attenpower, r_shadow_attenscale;

rtlight_t *r_shadow_compilingrtlight;
dlight_t *r_shadow_worldlightchain;
dlight_t *r_shadow_selectedlight;
dlight_t r_shadow_bufferlight;
vec3_t r_editlights_cursorlocation;

rtexture_t *lighttextures[5];

extern int con_vislines;

typedef struct cubemapinfo_s
{
	char basename[64];
	rtexture_t *texture;
}
cubemapinfo_t;

#define MAX_CUBEMAPS 256
static int numcubemaps;
static cubemapinfo_t cubemaps[MAX_CUBEMAPS];

#define SHADERPERMUTATION_SPECULAR (1<<0)
#define SHADERPERMUTATION_FOG (1<<1)
#define SHADERPERMUTATION_CUBEFILTER (1<<2)
#define SHADERPERMUTATION_OFFSETMAPPING (1<<3)
#define SHADERPERMUTATION_SURFACENORMALIZE (1<<4)
#define SHADERPERMUTATION_GEFORCEFX (1<<5)
#define SHADERPERMUTATION_COUNT (1<<6)

GLhandleARB r_shadow_program_light[SHADERPERMUTATION_COUNT];

void R_Shadow_UncompileWorldLights(void);
void R_Shadow_ClearWorldLights(void);
void R_Shadow_SaveWorldLights(void);
void R_Shadow_LoadWorldLights(void);
void R_Shadow_LoadLightsFile(void);
void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void);
void R_Shadow_EditLights_Reload_f(void);
void R_Shadow_ValidateCvars(void);
static void R_Shadow_MakeTextures(void);
void R_Shadow_DrawWorldLightShadowVolume(matrix4x4_t *matrix, dlight_t *light);

const char *builtinshader_light_vert =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"\n"
"// use half floats if available for math performance\n"
"#ifdef GEFORCEFX\n"
"#define myhalf half\n"
"#define myhvec2 hvec2\n"
"#define myhvec3 hvec3\n"
"#define myhvec4 hvec4\n"
"#else\n"
"#define myhalf float\n"
"#define myhvec2 vec2\n"
"#define myhvec3 vec3\n"
"#define myhvec4 vec4\n"
"#endif\n"
"\n"
"uniform vec3 LightPosition;\n"
"\n"
"varying vec2 TexCoord;\n"
"varying myhvec3 CubeVector;\n"
"varying vec3 LightVector;\n"
"\n"
"#if defined(USESPECULAR) || defined(USEFOG) || defined(USEOFFSETMAPPING)\n"
"uniform vec3 EyePosition;\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"\n"
"// TODO: get rid of tangentt (texcoord2) and use a crossproduct to regenerate it from tangents (texcoord1) and normal (texcoord3)\n"
"\n"
"void main(void)\n"
"{\n"
"	// copy the surface texcoord\n"
"	TexCoord = vec2(gl_TextureMatrix[0] * gl_MultiTexCoord0);\n"
"\n"
"	// transform vertex position into light attenuation/cubemap space\n"
"	// (-1 to +1 across the light box)\n"
"	CubeVector = vec3(gl_TextureMatrix[3] * gl_Vertex);\n"
"\n"
"	// transform unnormalized light direction into tangent space\n"
"	// (we use unnormalized to ensure that it interpolates correctly and then\n"
"	//  normalize it per pixel)\n"
"	vec3 lightminusvertex = LightPosition - gl_Vertex.xyz;\n"
"	LightVector.x = -dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = -dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = -dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n"
"\n"
"#if defined(USESPECULAR) || defined(USEFOG) || defined(USEOFFSETMAPPING)\n"
"	// transform unnormalized eye direction into tangent space\n"
"	vec3 eyeminusvertex = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = -dot(eyeminusvertex, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = -dot(eyeminusvertex, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = -dot(eyeminusvertex, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	// transform vertex to camera space, using ftransform to match non-VS\n"
"	// rendering\n"
"	gl_Position = ftransform();\n"
"}\n"
;

const char *builtinshader_light_frag =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"\n"
"// use half floats if available for math performance\n"
"#ifdef GEFORCEFX\n"
"#define myhalf half\n"
"#define myhvec2 hvec2\n"
"#define myhvec3 hvec3\n"
"#define myhvec4 hvec4\n"
"#else\n"
"#define myhalf float\n"
"#define myhvec2 vec2\n"
"#define myhvec3 vec3\n"
"#define myhvec4 vec4\n"
"#endif\n"
"\n"
"uniform myhvec3 LightColor;\n"
"#ifdef USEOFFSETMAPPING\n"
"uniform myhalf OffsetMapping_Scale;\n"
"uniform myhalf OffsetMapping_Bias;\n"
"#endif\n"
"#ifdef USESPECULAR\n"
"uniform myhalf SpecularPower;\n"
"#endif\n"
"#ifdef USEFOG\n"
"uniform myhalf FogRangeRecip;\n"
"#endif\n"
"uniform myhalf AmbientScale;\n"
"uniform myhalf DiffuseScale;\n"
"#ifdef USESPECULAR\n"
"uniform myhalf SpecularScale;\n"
"#endif\n"
"\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Color;\n"
"#ifdef USESPECULAR\n"
"uniform sampler2D Texture_Gloss;\n"
"#endif\n"
"#ifdef USECUBEFILTER\n"
"uniform samplerCube Texture_Cube;\n"
"#endif\n"
"#ifdef USEFOG\n"
"uniform sampler2D Texture_FogMask;\n"
"#endif\n"
"\n"
"varying vec2 TexCoord;\n"
"varying myhvec3 CubeVector;\n"
"varying vec3 LightVector;\n"
"#if defined(USESPECULAR) || defined(USEFOG) || defined(USEOFFSETMAPPING)\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"\n"
"void main(void)\n"
"{\n"
"	// attenuation\n"
"	//\n"
"	// the attenuation is (1-(x*x+y*y+z*z)) which gives a large bright\n"
"	// center and sharp falloff at the edge, this is about the most efficient\n"
"	// we can get away with as far as providing illumination.\n"
"	//\n"
"	// pow(1-(x*x+y*y+z*z), 4) is far more realistic but needs large lights to\n"
"	// provide significant illumination, large = slow = pain.\n"
"	myhalf colorscale = max(1.0 - dot(CubeVector, CubeVector), 0.0);\n"
"\n"
"#ifdef USEFOG\n"
"	// apply fog\n"
"	colorscale *= texture2D(Texture_FogMask, myhvec2(length(EyeVector)*FogRangeRecip, 0)).x;\n"
"#endif\n"
"\n"
"#ifdef USEOFFSETMAPPING\n"
"	// this is 3 sample because of ATI Radeon 9500-9800/X300 limits\n"
"	myhvec2 OffsetVector = normalize(EyeVector).xy * vec2(-0.333, 0.333);\n"
"	myhvec2 TexCoordOffset = TexCoord + OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(Texture_Normal, TexCoord).w);\n"
"	TexCoordOffset += OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(Texture_Normal, TexCoordOffset).w);\n"
"	TexCoordOffset += OffsetVector * (OffsetMapping_Bias + OffsetMapping_Scale * texture2D(Texture_Normal, TexCoordOffset).w);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	// get the surface normal\n"
"#ifdef SURFACENORMALIZE\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - 0.5);\n"
"#else\n"
"	myhvec3 surfacenormal = -1.0 + 2.0 * myhvec3(texture2D(Texture_Normal, TexCoord));\n"
"#endif\n"
"\n"
"	// calculate shading\n"
"	myhvec3 diffusenormal = myhvec3(normalize(LightVector));\n"
"	myhvec3 color = myhvec3(texture2D(Texture_Color, TexCoord)) * (AmbientScale + DiffuseScale * max(dot(surfacenormal, diffusenormal), 0.0));\n"
"#ifdef USESPECULAR\n"
"	myhvec3 specularnormal = myhvec3(normalize(diffusenormal + myhvec3(normalize(EyeVector))));\n"
"	color += myhvec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(max(dot(surfacenormal, specularnormal), 0.0), SpecularPower);\n"
"#endif\n"
"\n"
"#ifdef USECUBEFILTER\n"
"	// apply light cubemap filter\n"
"	color *= myhvec3(textureCube(Texture_Cube, CubeVector));\n"
"#endif\n"
"\n"
"	// calculate fragment color (apply light color and attenuation/fog scaling)\n"
"	gl_FragColor = myhvec4(color * LightColor * colorscale, 1);\n"
"}\n"
;

void r_shadow_start(void)
{
	int i;
	// use half float math where available (speed gain on NVIDIA GFFX and GF6)
	if (gl_support_half_float)
		Cvar_SetValue("r_shadow_glsl_usehalffloat", 1);
	// allocate vertex processing arrays
	numcubemaps = 0;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_texturepool = NULL;
	r_shadow_filters_texturepool = NULL;
	R_Shadow_ValidateCvars();
	R_Shadow_MakeTextures();
	maxshadowelements = 0;
	shadowelements = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxshadowmark = 0;
	numshadowmark = 0;
	shadowmark = NULL;
	shadowmarklist = NULL;
	shadowmarkcount = 0;
	r_shadow_buffer_numleafpvsbytes = 0;
	r_shadow_buffer_leafpvs = NULL;
	r_shadow_buffer_leaflist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	r_shadow_buffer_surfacepvs = NULL;
	r_shadow_buffer_surfacelist = NULL;
	for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
		r_shadow_program_light[i] = 0;
	if (gl_support_fragment_shader)
	{
		char *vertstring, *fragstring;
		int vertstrings_count;
		int fragstrings_count;
		const char *vertstrings_list[SHADERPERMUTATION_COUNT+1];
		const char *fragstrings_list[SHADERPERMUTATION_COUNT+1];
		vertstring = (char *)FS_LoadFile("glsl/light.vert", tempmempool, false);
		fragstring = (char *)FS_LoadFile("glsl/light.frag", tempmempool, false);
		for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
		{
			vertstrings_count = 0;
			fragstrings_count = 0;
			if (i & SHADERPERMUTATION_SPECULAR)
			{
				vertstrings_list[vertstrings_count++] = "#define USESPECULAR\n";
				fragstrings_list[fragstrings_count++] = "#define USESPECULAR\n";
			}
			if (i & SHADERPERMUTATION_FOG)
			{
				vertstrings_list[vertstrings_count++] = "#define USEFOG\n";
				fragstrings_list[fragstrings_count++] = "#define USEFOG\n";
			}
			if (i & SHADERPERMUTATION_CUBEFILTER)
			{
				vertstrings_list[vertstrings_count++] = "#define USECUBEFILTER\n";
				fragstrings_list[fragstrings_count++] = "#define USECUBEFILTER\n";
			}
			if (i & SHADERPERMUTATION_OFFSETMAPPING)
			{
				vertstrings_list[vertstrings_count++] = "#define USEOFFSETMAPPING\n";
				fragstrings_list[fragstrings_count++] = "#define USEOFFSETMAPPING\n";
			}
			if (i & SHADERPERMUTATION_SURFACENORMALIZE)
			{
				vertstrings_list[vertstrings_count++] = "#define SURFACENORMALIZE\n";
				fragstrings_list[fragstrings_count++] = "#define SURFACENORMALIZE\n";
			}
			if (i & SHADERPERMUTATION_GEFORCEFX)
			{
				// if the extension does not exist, don't try to compile it
				if (!gl_support_half_float)
					continue;
				vertstrings_list[vertstrings_count++] = "#define GEFORCEFX\n";
				fragstrings_list[fragstrings_count++] = "#define GEFORCEFX\n";
			}
			vertstrings_list[vertstrings_count++] = vertstring ? vertstring : builtinshader_light_vert;
			fragstrings_list[fragstrings_count++] = fragstring ? fragstring : builtinshader_light_frag;
			r_shadow_program_light[i] = GL_Backend_CompileProgram(vertstrings_count, vertstrings_list, fragstrings_count, fragstrings_list);
			if (!r_shadow_program_light[i])
			{
				Con_Printf("permutation %s %s %s %s %s %s failed for shader %s, some features may not work properly!\n", i & 1 ? "specular" : "", i & 2 ? "fog" : "", i & 4 ? "cubefilter" : "", i & 8 ? "offsetmapping" : "", i & 16 ? "surfacenormalize" : "", i & 32 ? "geforcefx" : "", "glsl/light");
				continue;
			}
			qglUseProgramObjectARB(r_shadow_program_light[i]);
			qglUniform1iARB(qglGetUniformLocationARB(r_shadow_program_light[i], "Texture_Normal"), 0);CHECKGLERROR
			qglUniform1iARB(qglGetUniformLocationARB(r_shadow_program_light[i], "Texture_Color"), 1);CHECKGLERROR
			if (i & SHADERPERMUTATION_SPECULAR)
			{
				qglUniform1iARB(qglGetUniformLocationARB(r_shadow_program_light[i], "Texture_Gloss"), 2);CHECKGLERROR
			}
			if (i & SHADERPERMUTATION_CUBEFILTER)
			{
				qglUniform1iARB(qglGetUniformLocationARB(r_shadow_program_light[i], "Texture_Cube"), 3);CHECKGLERROR
			}
			if (i & SHADERPERMUTATION_FOG)
			{
				qglUniform1iARB(qglGetUniformLocationARB(r_shadow_program_light[i], "Texture_FogMask"), 4);CHECKGLERROR
			}
		}
		qglUseProgramObjectARB(0);
		if (fragstring)
			Mem_Free(fragstring);
		if (vertstring)
			Mem_Free(vertstring);
	}
}

void r_shadow_shutdown(void)
{
	int i;
	R_Shadow_UncompileWorldLights();
	for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
	{
		if (r_shadow_program_light[i])
		{
			GL_Backend_FreeProgram(r_shadow_program_light[i]);
			r_shadow_program_light[i] = 0;
		}
	}
	numcubemaps = 0;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	R_FreeTexturePool(&r_shadow_texturepool);
	R_FreeTexturePool(&r_shadow_filters_texturepool);
	maxshadowelements = 0;
	if (shadowelements)
		Mem_Free(shadowelements);
	shadowelements = NULL;
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
	r_shadow_buffer_numleafpvsbytes = 0;
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
}

void r_shadow_newmap(void)
{
}

void R_Shadow_Help_f(void)
{
	Con_Printf(
"Documentation on r_shadow system:\n"
"Settings:\n"
"r_shadow_bumpscale_basetexture : base texture as bumpmap with this scale\n"
"r_shadow_bumpscale_bumpmap : depth scale for bumpmap conversion\n"
"r_shadow_debuglight : render only this light number (-1 = all)\n"
"r_shadow_gloss 0/1/2 : no gloss, gloss textures only, force gloss\n"
"r_shadow_gloss2intensity : brightness of forced gloss\n"
"r_shadow_glossintensity : brightness of textured gloss\n"
"r_shadow_lightattenuationpower : used to generate attenuation texture\n"
"r_shadow_lightattenuationscale : used to generate attenuation texture\n"
"r_shadow_lightintensityscale : scale rendering brightness of all lights\n"
"r_shadow_portallight : use portal visibility for static light precomputation\n"
"r_shadow_projectdistance : shadow volume projection distance\n"
"r_shadow_realtime_dlight : use high quality dynamic lights in normal mode\n"
"r_shadow_realtime_dlight_shadows : cast shadows from dlights\n"
"r_shadow_realtime_dlight_portalculling : work hard to reduce graphics work\n"
"r_shadow_realtime_world : use high quality world lighting mode\n"
"r_shadow_realtime_world_dlightshadows : cast shadows from dlights\n"
"r_shadow_realtime_world_lightmaps : use lightmaps in addition to lights\n"
"r_shadow_realtime_world_shadows : cast shadows from world lights\n"
"r_shadow_realtime_world_compile : compile surface/visibility information\n"
"r_shadow_realtime_world_compileshadow : compile shadow geometry\n"
"r_shadow_glsl : use OpenGL Shading Language for lighting\n"
"r_shadow_glsl_offsetmapping : enables Offset Mapping bumpmap enhancement\n"
"r_shadow_glsl_offsetmapping_scale : controls depth of Offset Mapping\n"
"r_shadow_glsl_offsetmapping_bias : should be negative half of scale\n"
"r_shadow_glsl_usehalffloat : use lower quality lighting\n"
"r_shadow_glsl_surfacenormalize : makes bumpmapping slightly higher quality\n"
"r_shadow_scissor : use scissor optimization\n"
"r_shadow_shadow_polygonfactor : nudge shadow volumes closer/further\n"
"r_shadow_shadow_polygonoffset : nudge shadow volumes closer/further\n"
"r_shadow_singlepassvolumegeneration : selects shadow volume algorithm\n"
"r_shadow_texture3d : use 3d attenuation texture (if hardware supports)\n"
"r_shadow_visiblelighting : useful for performance testing; bright = slow!\n"
"r_shadow_visiblevolumes : useful for performance testing; bright = slow!\n"
"Commands:\n"
"r_shadow_help : this help\n"
	);
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_bumpscale_basetexture);
	Cvar_RegisterVariable(&r_shadow_bumpscale_bumpmap);
	Cvar_RegisterVariable(&r_shadow_debuglight);
	Cvar_RegisterVariable(&r_shadow_gloss);
	Cvar_RegisterVariable(&r_shadow_gloss2intensity);
	Cvar_RegisterVariable(&r_shadow_glossintensity);
	Cvar_RegisterVariable(&r_shadow_lightattenuationpower);
	Cvar_RegisterVariable(&r_shadow_lightattenuationscale);
	Cvar_RegisterVariable(&r_shadow_lightintensityscale);
	Cvar_RegisterVariable(&r_shadow_portallight);
	Cvar_RegisterVariable(&r_shadow_projectdistance);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight_shadows);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight_portalculling);
	Cvar_RegisterVariable(&r_shadow_realtime_world);
	Cvar_RegisterVariable(&r_shadow_realtime_world_dlightshadows);
	Cvar_RegisterVariable(&r_shadow_realtime_world_lightmaps);
	Cvar_RegisterVariable(&r_shadow_realtime_world_shadows);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compile);
	Cvar_RegisterVariable(&r_shadow_realtime_world_compileshadow);
	Cvar_RegisterVariable(&r_shadow_scissor);
	Cvar_RegisterVariable(&r_shadow_shadow_polygonfactor);
	Cvar_RegisterVariable(&r_shadow_shadow_polygonoffset);
	Cvar_RegisterVariable(&r_shadow_singlepassvolumegeneration);
	Cvar_RegisterVariable(&r_shadow_texture3d);
	Cvar_RegisterVariable(&r_shadow_visiblelighting);
	Cvar_RegisterVariable(&r_shadow_visiblevolumes);
	Cvar_RegisterVariable(&r_shadow_glsl);
	Cvar_RegisterVariable(&r_shadow_glsl_offsetmapping);
	Cvar_RegisterVariable(&r_shadow_glsl_offsetmapping_scale);
	Cvar_RegisterVariable(&r_shadow_glsl_offsetmapping_bias);
	Cvar_RegisterVariable(&r_shadow_glsl_usehalffloat);
	Cvar_RegisterVariable(&r_shadow_glsl_surfacenormalize);
	Cvar_RegisterVariable(&gl_ext_stenciltwoside);
	if (gamemode == GAME_TENEBRAE)
	{
		Cvar_SetValue("r_shadow_gloss", 2);
		Cvar_SetValue("r_shadow_bumpscale_basetexture", 4);
	}
	Cmd_AddCommand("r_shadow_help", R_Shadow_Help_f);
	R_Shadow_EditLights_Init();
	r_shadow_mempool = Mem_AllocPool("R_Shadow", 0, NULL);
	r_shadow_worldlightchain = NULL;
	maxshadowelements = 0;
	shadowelements = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxshadowmark = 0;
	numshadowmark = 0;
	shadowmark = NULL;
	shadowmarklist = NULL;
	shadowmarkcount = 0;
	r_shadow_buffer_numleafpvsbytes = 0;
	r_shadow_buffer_leafpvs = NULL;
	r_shadow_buffer_leaflist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	r_shadow_buffer_surfacepvs = NULL;
	r_shadow_buffer_surfacelist = NULL;
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

int *R_Shadow_ResizeShadowElements(int numtris)
{
	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
	{
		maxshadowelements = numtris * 24;
		if (shadowelements)
			Mem_Free(shadowelements);
		shadowelements = (int *)Mem_Alloc(r_shadow_mempool, maxshadowelements * sizeof(int));
	}
	return shadowelements;
}

static void R_Shadow_EnlargeLeafSurfaceBuffer(int numleafs, int numsurfaces)
{
	int numleafpvsbytes = (((numleafs + 7) >> 3) + 255) & ~255;
	int numsurfacepvsbytes = (((numsurfaces + 7) >> 3) + 255) & ~255;
	if (r_shadow_buffer_numleafpvsbytes < numleafpvsbytes)
	{
		if (r_shadow_buffer_leafpvs)
			Mem_Free(r_shadow_buffer_leafpvs);
		if (r_shadow_buffer_leaflist)
			Mem_Free(r_shadow_buffer_leaflist);
		r_shadow_buffer_numleafpvsbytes = numleafpvsbytes;
		r_shadow_buffer_leafpvs = (qbyte *)Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numleafpvsbytes);
		r_shadow_buffer_leaflist = (int *)Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numleafpvsbytes * 8 * sizeof(*r_shadow_buffer_leaflist));
	}
	if (r_shadow_buffer_numsurfacepvsbytes < numsurfacepvsbytes)
	{
		if (r_shadow_buffer_surfacepvs)
			Mem_Free(r_shadow_buffer_surfacepvs);
		if (r_shadow_buffer_surfacelist)
			Mem_Free(r_shadow_buffer_surfacelist);
		r_shadow_buffer_numsurfacepvsbytes = numsurfacepvsbytes;
		r_shadow_buffer_surfacepvs = (qbyte *)Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numsurfacepvsbytes);
		r_shadow_buffer_surfacelist = (int *)Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numsurfacepvsbytes * 8 * sizeof(*r_shadow_buffer_surfacelist));
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
		shadowmark = (int *)Mem_Alloc(r_shadow_mempool, maxshadowmark * sizeof(*shadowmark));
		shadowmarklist = (int *)Mem_Alloc(r_shadow_mempool, maxshadowmark * sizeof(*shadowmarklist));
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

int R_Shadow_ConstructShadowVolume(int innumvertices, int innumtris, const int *inelement3i, const int *inneighbor3i, const float *invertex3f, int *outnumvertices, int *outelement3i, float *outvertex3f, const float *projectorigin, float projectdistance, int numshadowmarktris, const int *shadowmarktris)
{
	int i, j;
	int outtriangles = 0, outvertices = 0;
	const int *element;
	const float *vertex;

	if (maxvertexupdate < innumvertices)
	{
		maxvertexupdate = innumvertices;
		if (vertexupdate)
			Mem_Free(vertexupdate);
		if (vertexremap)
			Mem_Free(vertexremap);
		vertexupdate = (int *)Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
		vertexremap = (int *)Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
		vertexupdatenum = 0;
	}
	vertexupdatenum++;
	if (vertexupdatenum == 0)
	{
		vertexupdatenum = 1;
		memset(vertexupdate, 0, maxvertexupdate * sizeof(int));
		memset(vertexremap, 0, maxvertexupdate * sizeof(int));
	}

	for (i = 0;i < numshadowmarktris;i++)
		shadowmark[shadowmarktris[i]] = shadowmarkcount;

	for (i = 0;i < numshadowmarktris;i++)
	{
		element = inelement3i + shadowmarktris[i] * 3;
		// make sure the vertices are created
		for (j = 0;j < 3;j++)
		{
			if (vertexupdate[element[j]] != vertexupdatenum)
			{
				float ratio, direction[3];
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
	if (outnumvertices)
		*outnumvertices = outvertices;
	return outtriangles;
}

void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, float projectdistance, int nummarktris, const int *marktris)
{
	int tris, outverts;
	if (projectdistance < 0.1)
	{
		Con_Printf("R_Shadow_Volume: projectdistance %f\n");
		return;
	}
	if (!numverts || !nummarktris)
		return;
	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < nummarktris * 24)
		R_Shadow_ResizeShadowElements((nummarktris + 256) * 24);
	tris = R_Shadow_ConstructShadowVolume(numverts, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, varray_vertex3f2, projectorigin, projectdistance, nummarktris, marktris);
	renderstats.lights_dynamicshadowtriangles += tris;
	R_Shadow_RenderVolume(outverts, tris, varray_vertex3f2, shadowelements);
}

void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs)
{
	int t, tend;
	const int *e;
	const float *v[3];
	if (!BoxesOverlap(lightmins, lightmaxs, surfacemins, surfacemaxs))
		return;
	tend = firsttriangle + numtris;
	if (surfacemins[0] >= lightmins[0] && surfacemaxs[0] <= lightmaxs[0]
	 && surfacemins[1] >= lightmins[1] && surfacemaxs[1] <= lightmaxs[1]
	 && surfacemins[2] >= lightmins[2] && surfacemaxs[2] <= lightmaxs[2])
	{
		// surface box entirely inside light box, no box cull
		for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
			if (PointInfrontOfTriangle(projectorigin, invertex3f + e[0] * 3, invertex3f + e[1] * 3, invertex3f + e[2] * 3))
				shadowmarklist[numshadowmark++] = t;
	}
	else
	{
		// surface box not entirely inside light box, cull each triangle
		for (t = firsttriangle, e = elements + t * 3;t < tend;t++, e += 3)
		{
			v[0] = invertex3f + e[0] * 3;
			v[1] = invertex3f + e[1] * 3;
			v[2] = invertex3f + e[2] * 3;
			if (PointInfrontOfTriangle(projectorigin, v[0], v[1], v[2])
			 && lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0]))
			 && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0]))
			 && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1]))
			 && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1]))
			 && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2]))
			 && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
				shadowmarklist[numshadowmark++] = t;
		}
	}
}

void R_Shadow_RenderVolume(int numvertices, int numtriangles, const float *vertex3f, const int *element3i)
{
	rmeshstate_t m;
	if (r_shadow_compilingrtlight)
	{
		// if we're compiling an rtlight, capture the mesh
		Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_shadow, NULL, NULL, NULL, vertex3f, NULL, NULL, NULL, NULL, numtriangles, element3i);
		return;
	}
	renderstats.lights_shadowtriangles += numtriangles;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, numvertices);
	if (r_shadowstage == R_SHADOWSTAGE_STENCIL)
	{
		// decrement stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		R_Mesh_Draw(0, numvertices, numtriangles, element3i);
		// increment stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	}
	R_Mesh_Draw(0, numvertices, numtriangles, element3i);
	GL_LockArrays(0, 0);
}

static void R_Shadow_MakeTextures(void)
{
	int x, y, z, d;
	float v[3], intensity;
	qbyte *data;
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_attenpower = r_shadow_lightattenuationpower.value;
	r_shadow_attenscale = r_shadow_lightattenuationscale.value;
#define ATTEN2DSIZE 64
#define ATTEN3DSIZE 32
	data = (qbyte *)Mem_Alloc(tempmempool, max(ATTEN3DSIZE*ATTEN3DSIZE*ATTEN3DSIZE*4, ATTEN2DSIZE*ATTEN2DSIZE*4));
	for (y = 0;y < ATTEN2DSIZE;y++)
	{
		for (x = 0;x < ATTEN2DSIZE;x++)
		{
			v[0] = ((x + 0.5f) * (2.0f / ATTEN2DSIZE) - 1.0f) * (1.0f / 0.9375);
			v[1] = ((y + 0.5f) * (2.0f / ATTEN2DSIZE) - 1.0f) * (1.0f / 0.9375);
			v[2] = 0;
			intensity = 1.0f - sqrt(DotProduct(v, v));
			if (intensity > 0)
				intensity = pow(intensity, r_shadow_attenpower) * r_shadow_attenscale * 256.0f;
			d = bound(0, intensity, 255);
			data[(y*ATTEN2DSIZE+x)*4+0] = d;
			data[(y*ATTEN2DSIZE+x)*4+1] = d;
			data[(y*ATTEN2DSIZE+x)*4+2] = d;
			data[(y*ATTEN2DSIZE+x)*4+3] = d;
		}
	}
	r_shadow_attenuation2dtexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation2d", ATTEN2DSIZE, ATTEN2DSIZE, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALPHA, NULL);
	if (r_shadow_texture3d.integer)
	{
		for (z = 0;z < ATTEN3DSIZE;z++)
		{
			for (y = 0;y < ATTEN3DSIZE;y++)
			{
				for (x = 0;x < ATTEN3DSIZE;x++)
				{
					v[0] = ((x + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375);
					v[1] = ((y + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375);
					v[2] = ((z + 0.5f) * (2.0f / ATTEN3DSIZE) - 1.0f) * (1.0f / 0.9375);
					intensity = 1.0f - sqrt(DotProduct(v, v));
					if (intensity > 0)
						intensity = pow(intensity, r_shadow_attenpower) * r_shadow_attenscale * 256.0f;
					d = bound(0, intensity, 255);
					data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+0] = d;
					data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+1] = d;
					data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+2] = d;
					data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+3] = d;
				}
			}
		}
		r_shadow_attenuation3dtexture = R_LoadTexture3D(r_shadow_texturepool, "attenuation3d", ATTEN3DSIZE, ATTEN3DSIZE, ATTEN3DSIZE, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALPHA, NULL);
	}
	Mem_Free(data);
}

void R_Shadow_ValidateCvars(void)
{
	if (r_shadow_texture3d.integer && !gl_texture3d)
		Cvar_SetValueQuick(&r_shadow_texture3d, 0);
	if (gl_ext_stenciltwoside.integer && !gl_support_stenciltwoside)
		Cvar_SetValueQuick(&gl_ext_stenciltwoside, 0);
}

// light currently being rendered
rtlight_t *r_shadow_rtlight;
// light filter cubemap being used by the light
static rtexture_t *r_shadow_lightcubemap;

// this is the location of the eye in entity space
static vec3_t r_shadow_entityeyeorigin;
// this is the location of the light in entity space
static vec3_t r_shadow_entitylightorigin;
// this transforms entity coordinates to light filter cubemap coordinates
// (also often used for other purposes)
static matrix4x4_t r_shadow_entitytolight;
// based on entitytolight this transforms -1 to +1 to 0 to 1 for purposes
// of attenuation texturing in full 3D (Z result often ignored)
static matrix4x4_t r_shadow_entitytoattenuationxyz;
// this transforms only the Z to S, and T is always 0.5
static matrix4x4_t r_shadow_entitytoattenuationz;
// rtlight->color * r_refdef.lightstylevalue[rtlight->style] / 256 * r_shadow_lightintensityscale.value * ent->colormod * ent->alpha
static vec3_t r_shadow_entitylightcolorbase;
// rtlight->color * r_refdef.lightstylevalue[rtlight->style] / 256 * r_shadow_lightintensityscale.value * ent->colormap_pantscolor * ent->alpha
static vec3_t r_shadow_entitylightcolorpants;
// rtlight->color * r_refdef.lightstylevalue[rtlight->style] / 256 * r_shadow_lightintensityscale.value * ent->colormap_shirtcolor * ent->alpha
static vec3_t r_shadow_entitylightcolorshirt;

static int r_shadow_lightpermutation;
static int r_shadow_lightprog;

void R_Shadow_Stage_Begin(void)
{
	rmeshstate_t m;

	R_Shadow_ValidateCvars();

	if (!r_shadow_attenuation2dtexture
	 || (!r_shadow_attenuation3dtexture && r_shadow_texture3d.integer)
	 || r_shadow_lightattenuationpower.value != r_shadow_attenpower
	 || r_shadow_lightattenuationscale.value != r_shadow_attenscale)
		R_Shadow_MakeTextures();

	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_State(&m);
	GL_Color(0, 0, 0, 1);
	qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
	qglEnable(GL_CULL_FACE);
	GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	r_shadowstage = R_SHADOWSTAGE_NONE;
}

void R_Shadow_Stage_ActiveLight(rtlight_t *rtlight)
{
	r_shadow_rtlight = rtlight;
}

void R_Shadow_Stage_Reset(void)
{
	rmeshstate_t m;
	if (gl_support_stenciltwoside)
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	if (r_shadowstage == R_SHADOWSTAGE_LIGHT_GLSL)
	{
		qglUseProgramObjectARB(0);
		// HACK HACK HACK: work around for stupid NVIDIA bug that causes GL_OUT_OF_MEMORY and/or software rendering in 6xxx drivers
		qglBegin(GL_TRIANGLES);
		qglEnd();
		CHECKGLERROR
	}
	memset(&m, 0, sizeof(m));
	R_Mesh_State(&m);
}

void R_Shadow_Stage_StencilShadowVolumes(void)
{
	R_Shadow_Stage_Reset();
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(0, 0, 0, 0);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(false);
	GL_DepthTest(true);
	qglPolygonOffset(r_shadow_shadow_polygonfactor.value, r_shadow_shadow_polygonoffset.value);
	//if (r_shadow_shadow_polygonoffset.value != 0)
	//{
	//	qglPolygonOffset(r_shadow_shadow_polygonfactor.value, r_shadow_shadow_polygonoffset.value);
	//	qglEnable(GL_POLYGON_OFFSET_FILL);
	//}
	//else
	//	qglDisable(GL_POLYGON_OFFSET_FILL);
	qglDepthFunc(GL_LESS);
	qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
	qglEnable(GL_STENCIL_TEST);
	qglStencilFunc(GL_ALWAYS, 128, ~0);
	if (gl_ext_stenciltwoside.integer)
	{
		r_shadowstage = R_SHADOWSTAGE_STENCILTWOSIDE;
		qglDisable(GL_CULL_FACE);
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
		qglActiveStencilFaceEXT(GL_BACK); // quake is backwards, this is front faces
		qglStencilMask(~0);
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		qglActiveStencilFaceEXT(GL_FRONT); // quake is backwards, this is back faces
		qglStencilMask(~0);
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	}
	else
	{
		r_shadowstage = R_SHADOWSTAGE_STENCIL;
		qglEnable(GL_CULL_FACE);
		qglStencilMask(~0);
		// this is changed by every shadow render so its value here is unimportant
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	}
	GL_Clear(GL_STENCIL_BUFFER_BIT);
	renderstats.lights_clears++;
}

void R_Shadow_Stage_Lighting(int stenciltest)
{
	rmeshstate_t m;
	R_Shadow_Stage_Reset();
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	qglPolygonOffset(0, 0);
	//qglDisable(GL_POLYGON_OFFSET_FILL);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	qglDepthFunc(GL_EQUAL);
	qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
	qglEnable(GL_CULL_FACE);
	if (r_shadowstage == R_SHADOWSTAGE_STENCIL || r_shadowstage == R_SHADOWSTAGE_STENCILTWOSIDE)
		qglEnable(GL_STENCIL_TEST);
	else
		qglDisable(GL_STENCIL_TEST);
	qglStencilMask(~0);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw light where this geometry was already rendered AND the
	// stencil is 128 (values other than this mean shadow)
	qglStencilFunc(GL_EQUAL, 128, ~0);
	if (r_shadow_glsl.integer && r_shadow_program_light[0])
	{
		r_shadowstage = R_SHADOWSTAGE_LIGHT_GLSL;
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = varray_vertex3f;
		m.pointer_texcoord[0] = varray_texcoord2f[0];
		m.pointer_texcoord3f[1] = varray_svector3f;
		m.pointer_texcoord3f[2] = varray_tvector3f;
		m.pointer_texcoord3f[3] = varray_normal3f;
		m.tex[0] = R_GetTexture(r_texture_blanknormalmap); // normal
		m.tex[1] = R_GetTexture(r_texture_white); // diffuse
		m.tex[2] = R_GetTexture(r_texture_white); // gloss
		m.texcubemap[3] = R_GetTexture(r_shadow_lightcubemap); // light filter
		m.tex[4] = R_GetTexture(r_texture_fogattenuation); // fog
		//m.texmatrix[3] = r_shadow_entitytolight; // light filter matrix
		R_Mesh_State(&m);
		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
		CHECKGLERROR
		r_shadow_lightpermutation = 0;
		// only add a feature to the permutation if that permutation exists
		// (otherwise it might end up not using a shader at all, which looks
		// worse than using less features)
		if (fogenabled && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_FOG])
			r_shadow_lightpermutation |= SHADERPERMUTATION_FOG;
		if (r_shadow_rtlight->specularscale && r_shadow_gloss.integer >= 1 && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_SPECULAR])
			r_shadow_lightpermutation |= SHADERPERMUTATION_SPECULAR;
		if (r_shadow_lightcubemap != r_texture_whitecube && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_CUBEFILTER])
			r_shadow_lightpermutation |= SHADERPERMUTATION_CUBEFILTER;
		if (r_shadow_glsl_offsetmapping.integer && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_OFFSETMAPPING])
			r_shadow_lightpermutation |= SHADERPERMUTATION_OFFSETMAPPING;
		if (r_shadow_glsl_surfacenormalize.integer && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_SURFACENORMALIZE])
			r_shadow_lightpermutation |= SHADERPERMUTATION_SURFACENORMALIZE;
		if (r_shadow_glsl_usehalffloat.integer && r_shadow_program_light[r_shadow_lightpermutation | SHADERPERMUTATION_GEFORCEFX])
			r_shadow_lightpermutation |= SHADERPERMUTATION_GEFORCEFX;
		r_shadow_lightprog = r_shadow_program_light[r_shadow_lightpermutation];
		qglUseProgramObjectARB(r_shadow_lightprog);CHECKGLERROR
		// TODO: support fog (after renderer is converted to texture fog)
		if (r_shadow_lightpermutation & SHADERPERMUTATION_FOG)
		{
			qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "FogRangeRecip"), fograngerecip);CHECKGLERROR
		}
		qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "AmbientScale"), r_shadow_rtlight->ambientscale);CHECKGLERROR
		qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "DiffuseScale"), r_shadow_rtlight->diffusescale);CHECKGLERROR
		if (r_shadow_lightpermutation & SHADERPERMUTATION_SPECULAR)
		{
			qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "SpecularPower"), 8);CHECKGLERROR
			qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "SpecularScale"), r_shadow_rtlight->specularscale);CHECKGLERROR
		}
		//qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "LightColor"), lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);CHECKGLERROR
		//qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "LightPosition"), relativelightorigin[0], relativelightorigin[1], relativelightorigin[2]);CHECKGLERROR
		//if (r_shadow_lightpermutation & (SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_FOG | SHADERPERMUTATION_OFFSETMAPPING))
		//{
		//	qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "EyePosition"), relativeeyeorigin[0], relativeeyeorigin[1], relativeeyeorigin[2]);CHECKGLERROR
		//}
		if (r_shadow_lightpermutation & SHADERPERMUTATION_OFFSETMAPPING)
		{
			qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "OffsetMapping_Scale"), r_shadow_glsl_offsetmapping_scale.value);CHECKGLERROR
			qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "OffsetMapping_Bias"), r_shadow_glsl_offsetmapping_bias.value);CHECKGLERROR
		}
	}
	else if (gl_dot3arb && gl_texturecubemap && r_textureunits.integer >= 2 && gl_combine.integer && gl_stencil)
		r_shadowstage = R_SHADOWSTAGE_LIGHT_DOT3;
	else
		r_shadowstage = R_SHADOWSTAGE_LIGHT_VERTEX;
}

void R_Shadow_Stage_VisibleShadowVolumes(void)
{
	R_Shadow_Stage_Reset();
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(r_shadow_visiblevolumes.integer < 2);
	qglPolygonOffset(0, 0);
	GL_Color(0.0, 0.0125, 0.1, 1);
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	qglDepthFunc(GL_GEQUAL);
	qglCullFace(GL_FRONT); // this culls back
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_STENCIL_TEST);
	r_shadowstage = R_SHADOWSTAGE_VISIBLEVOLUMES;
}

void R_Shadow_Stage_VisibleLighting(int stenciltest)
{
	R_Shadow_Stage_Reset();
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(r_shadow_visiblelighting.integer < 2);
	qglPolygonOffset(0, 0);
	GL_Color(0.1, 0.0125, 0, 1);
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	qglDepthFunc(GL_EQUAL);
	qglCullFace(GL_FRONT); // this culls back
	qglEnable(GL_CULL_FACE);
	if (stenciltest)
		qglEnable(GL_STENCIL_TEST);
	else
		qglDisable(GL_STENCIL_TEST);
	r_shadowstage = R_SHADOWSTAGE_VISIBLELIGHTING;
}

void R_Shadow_Stage_End(void)
{
	R_Shadow_Stage_Reset();
	R_Shadow_Stage_ActiveLight(NULL);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	qglPolygonOffset(0, 0);
	//qglDisable(GL_POLYGON_OFFSET_FILL);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	qglDepthFunc(GL_LEQUAL);
	qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	if (gl_support_stenciltwoside)
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	qglStencilMask(~0);
	qglStencilFunc(GL_ALWAYS, 128, ~0);
	r_shadowstage = R_SHADOWSTAGE_NONE;
}

qboolean R_Shadow_ScissorForBBox(const float *mins, const float *maxs)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2;
	vec4_t v, v2;
	rmesh_t mesh;
	mplane_t planes[11];
	float vertex3f[256*3];

	// if view is inside the light box, just say yes it's visible
	if (BoxesOverlap(r_vieworigin, r_vieworigin, mins, maxs))
	{
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return false;
	}

	// create a temporary brush describing the area the light can affect in worldspace
	VectorNegate(frustum[0].normal, planes[ 0].normal);planes[ 0].dist = -frustum[0].dist;
	VectorNegate(frustum[1].normal, planes[ 1].normal);planes[ 1].dist = -frustum[1].dist;
	VectorNegate(frustum[2].normal, planes[ 2].normal);planes[ 2].dist = -frustum[2].dist;
	VectorNegate(frustum[3].normal, planes[ 3].normal);planes[ 3].dist = -frustum[3].dist;
	VectorNegate(frustum[4].normal, planes[ 4].normal);planes[ 4].dist = -frustum[4].dist;
	VectorSet   (planes[ 5].normal,  1, 0, 0);         planes[ 5].dist =  maxs[0];
	VectorSet   (planes[ 6].normal, -1, 0, 0);         planes[ 6].dist = -mins[0];
	VectorSet   (planes[ 7].normal, 0,  1, 0);         planes[ 7].dist =  maxs[1];
	VectorSet   (planes[ 8].normal, 0, -1, 0);         planes[ 8].dist = -mins[1];
	VectorSet   (planes[ 9].normal, 0, 0,  1);         planes[ 9].dist =  maxs[2];
	VectorSet   (planes[10].normal, 0, 0, -1);         planes[10].dist = -mins[2];

	// turn the brush into a mesh
	memset(&mesh, 0, sizeof(rmesh_t));
	mesh.maxvertices = 256;
	mesh.vertex3f = vertex3f;
	mesh.epsilon2 = (1.0f / (32.0f * 32.0f));
	R_Mesh_AddBrushMeshFromPlanes(&mesh, 11, planes);

	// if that mesh is empty, the light is not visible at all
	if (!mesh.numvertices)
		return true;

	if (!r_shadow_scissor.integer)
		return false;

	// if that mesh is not empty, check what area of the screen it covers
	x1 = y1 = x2 = y2 = 0;
	v[3] = 1.0f;
	for (i = 0;i < mesh.numvertices;i++)
	{
		VectorCopy(mesh.vertex3f + i * 3, v);
		GL_TransformToScreen(v, v2);
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
	ix1 = x1 - 1.0f;
	iy1 = y1 - 1.0f;
	ix2 = x2 + 1.0f;
	iy2 = y2 + 1.0f;
	//Con_Printf("%f %f %f %f\n", x1, y1, x2, y2);

	// clamp it to the screen
	if (ix1 < r_view_x) ix1 = r_view_x;
	if (iy1 < r_view_y) iy1 = r_view_y;
	if (ix2 > r_view_x + r_view_width) ix2 = r_view_x + r_view_width;
	if (iy2 > r_view_y + r_view_height) iy2 = r_view_y + r_view_height;

	// if it is inside out, it's not visible
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;

	// the light area is visible, set up the scissor rectangle
	GL_Scissor(ix1, vid.height - iy2, ix2 - ix1, iy2 - iy1);
	//qglScissor(ix1, iy1, ix2 - ix1, iy2 - iy1);
	//qglEnable(GL_SCISSOR_TEST);
	renderstats.lights_scissored++;
	return false;
}

extern float *rsurface_vertex3f;
extern float *rsurface_svector3f;
extern float *rsurface_tvector3f;
extern float *rsurface_normal3f;
extern void RSurf_SetVertexPointer(const entity_render_t *ent, const texture_t *texture, const msurface_t *surface, const vec3_t modelorg);

static void R_Shadow_RenderSurfacesLighting_Light_Vertex_Shading(const msurface_t *surface, const float *diffusecolor, const float *ambientcolor, float reduce, const vec3_t modelorg)
{
	int numverts = surface->num_vertices;
	float *vertex3f = rsurface_vertex3f + 3 * surface->num_firstvertex;
	float *normal3f = rsurface_normal3f + 3 * surface->num_firstvertex;
	float *color4f = varray_color4f + 4 * surface->num_firstvertex;
	float dist, dot, distintensity, shadeintensity, v[3], n[3];
	if (r_textureunits.integer >= 3)
	{
		for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
		{
			Matrix4x4_Transform(&r_shadow_entitytolight, vertex3f, v);
			Matrix4x4_Transform3x3(&r_shadow_entitytolight, normal3f, n);
			if ((dot = DotProduct(n, v)) > 0)
			{
				shadeintensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
				color4f[0] = (ambientcolor[0] + shadeintensity * diffusecolor[0]) - reduce;
				color4f[1] = (ambientcolor[1] + shadeintensity * diffusecolor[1]) - reduce;
				color4f[2] = (ambientcolor[2] + shadeintensity * diffusecolor[2]) - reduce;
				if (fogenabled)
				{
					float f = VERTEXFOGTABLE(VectorDistance(v, modelorg));
					VectorScale(color4f, f, color4f);
				}
			}
			else
				VectorClear(color4f);
			color4f[3] = 1;
		}
	}
	else if (r_textureunits.integer >= 2)
	{
		for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
		{
			Matrix4x4_Transform(&r_shadow_entitytolight, vertex3f, v);
			if ((dist = fabs(v[2])) < 1)
			{
				distintensity = pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale;
				Matrix4x4_Transform3x3(&r_shadow_entitytolight, normal3f, n);
				if ((dot = DotProduct(n, v)) > 0)
				{
					shadeintensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
					color4f[0] = (ambientcolor[0] + shadeintensity * diffusecolor[0]) * distintensity - reduce;
					color4f[1] = (ambientcolor[1] + shadeintensity * diffusecolor[1]) * distintensity - reduce;
					color4f[2] = (ambientcolor[2] + shadeintensity * diffusecolor[2]) * distintensity - reduce;
				}
				else
				{
					color4f[0] = ambientcolor[0] * distintensity - reduce;
					color4f[1] = ambientcolor[1] * distintensity - reduce;
					color4f[2] = ambientcolor[2] * distintensity - reduce;
				}
				if (fogenabled)
				{
					float f = VERTEXFOGTABLE(VectorDistance(v, modelorg));
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
		for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
		{
			Matrix4x4_Transform(&r_shadow_entitytolight, vertex3f, v);
			if ((dist = DotProduct(v, v)) < 1)
			{
				dist = sqrt(dist);
				distintensity = pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale;
				Matrix4x4_Transform3x3(&r_shadow_entitytolight, normal3f, n);
				if ((dot = DotProduct(n, v)) > 0)
				{
					shadeintensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
					color4f[0] = (ambientcolor[0] + shadeintensity * diffusecolor[0]) * distintensity - reduce;
					color4f[1] = (ambientcolor[1] + shadeintensity * diffusecolor[1]) * distintensity - reduce;
					color4f[2] = (ambientcolor[2] + shadeintensity * diffusecolor[2]) * distintensity - reduce;
				}
				else
				{
					color4f[0] = ambientcolor[0] * distintensity - reduce;
					color4f[1] = ambientcolor[1] * distintensity - reduce;
					color4f[2] = ambientcolor[2] * distintensity - reduce;
				}
				if (fogenabled)
				{
					float f = VERTEXFOGTABLE(VectorDistance(v, modelorg));
					VectorScale(color4f, f, color4f);
				}
			}
			else
				VectorClear(color4f);
			color4f[3] = 1;
		}
	}
}

// TODO: use glTexGen instead of feeding vertices to texcoordpointer?
#define USETEXMATRIX

#ifndef USETEXMATRIX
// this should be done in a texture matrix or vertex program when possible, but here's code to do it manually
// if hardware texcoord manipulation is not available (or not suitable, this would really benefit from 3DNow! or SSE
static void R_Shadow_Transform_Vertex3f_TexCoord3f(float *tc3f, int numverts, const float *vertex3f, const matrix4x4_t *matrix)
{
	do
	{
		tc3f[0] = vertex3f[0] * matrix->m[0][0] + vertex3f[1] * matrix->m[0][1] + vertex3f[2] * matrix->m[0][2] + matrix->m[0][3];
		tc3f[1] = vertex3f[0] * matrix->m[1][0] + vertex3f[1] * matrix->m[1][1] + vertex3f[2] * matrix->m[1][2] + matrix->m[1][3];
		tc3f[2] = vertex3f[0] * matrix->m[2][0] + vertex3f[1] * matrix->m[2][1] + vertex3f[2] * matrix->m[2][2] + matrix->m[2][3];
		vertex3f += 3;
		tc3f += 3;
	}
	while (--numverts);
}

static void R_Shadow_Transform_Vertex3f_TexCoord2f(float *tc2f, int numverts, const float *vertex3f, const matrix4x4_t *matrix)
{
	do
	{
		tc2f[0] = vertex3f[0] * matrix->m[0][0] + vertex3f[1] * matrix->m[0][1] + vertex3f[2] * matrix->m[0][2] + matrix->m[0][3];
		tc2f[1] = vertex3f[0] * matrix->m[1][0] + vertex3f[1] * matrix->m[1][1] + vertex3f[2] * matrix->m[1][2] + matrix->m[1][3];
		vertex3f += 3;
		tc2f += 2;
	}
	while (--numverts);
}
#endif

static void R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(float *out3f, int numverts, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const vec3_t relativelightorigin)
{
	int i;
	float lightdir[3];
	for (i = 0;i < numverts;i++, vertex3f += 3, svector3f += 3, tvector3f += 3, normal3f += 3, out3f += 3)
	{
		VectorSubtract(vertex3f, relativelightorigin, lightdir);
		// the cubemap normalizes this for us
		out3f[0] = DotProduct(svector3f, lightdir);
		out3f[1] = DotProduct(tvector3f, lightdir);
		out3f[2] = DotProduct(normal3f, lightdir);
	}
}

static void R_Shadow_GenTexCoords_Specular_NormalCubeMap(float *out3f, int numverts, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const vec3_t relativelightorigin, const vec3_t relativeeyeorigin)
{
	int i;
	float lightdir[3], eyedir[3], halfdir[3];
	for (i = 0;i < numverts;i++, vertex3f += 3, svector3f += 3, tvector3f += 3, normal3f += 3, out3f += 3)
	{
		VectorSubtract(vertex3f, relativelightorigin, lightdir);
		VectorNormalize(lightdir);
		VectorSubtract(vertex3f, relativeeyeorigin, eyedir);
		VectorNormalize(eyedir);
		VectorAdd(lightdir, eyedir, halfdir);
		// the cubemap normalizes this for us
		out3f[0] = DotProduct(svector3f, halfdir);
		out3f[1] = DotProduct(tvector3f, halfdir);
		out3f[2] = DotProduct(normal3f, halfdir);
	}
}

static void R_Shadow_RenderSurfacesLighting_VisibleLighting(const entity_render_t *ent, const texture_t *texture, int numsurfaces, msurface_t **surfacelist, const vec3_t lightcolorbase, const vec3_t lightcolorpants, const vec3_t lightcolorshirt, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *normalmaptexture, rtexture_t *glosstexture, float specularscale, const vec3_t modelorg)
{
	// used to display how many times a surface is lit for level design purposes
	int surfacelistindex;
	rmeshstate_t m;
	qboolean doambientbase = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean dodiffusebase = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean doambientpants = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean dodiffusepants = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean doambientshirt = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dodiffuseshirt = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dospecular = specularscale * VectorLength2(lightcolorbase) > 0.00001 && glosstexture != r_texture_black;
	if (!doambientbase && !dodiffusebase && !doambientpants && !dodiffusepants && !doambientshirt && !dodiffuseshirt && !dospecular)
		return;
	GL_Color(0.1, 0.025, 0, 1);
	memset(&m, 0, sizeof(m));
	R_Mesh_State(&m);
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const msurface_t *surface = surfacelist[surfacelistindex];
		RSurf_SetVertexPointer(ent, texture, surface, modelorg);
		GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle);
		GL_LockArrays(0, 0);
	}
}

static void R_Shadow_RenderSurfacesLighting_Light_GLSL(const entity_render_t *ent, const texture_t *texture, int numsurfaces, msurface_t **surfacelist, const vec3_t lightcolorbase, const vec3_t lightcolorpants, const vec3_t lightcolorshirt, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *normalmaptexture, rtexture_t *glosstexture, float specularscale, const vec3_t modelorg)
{
	// ARB2 GLSL shader path (GFFX5200, Radeon 9500)
	int surfacelistindex;
	qboolean doambientbase = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean dodiffusebase = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean doambientpants = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean dodiffusepants = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean doambientshirt = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dodiffuseshirt = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dospecular = specularscale * VectorLength2(lightcolorbase) > 0.00001 && glosstexture != r_texture_black;
	// TODO: add direct pants/shirt rendering
	if (doambientpants || dodiffusepants)
		R_Shadow_RenderSurfacesLighting_Light_GLSL(ent, texture, numsurfaces, surfacelist, lightcolorpants, vec3_origin, vec3_origin, pantstexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (doambientshirt || dodiffuseshirt)
		R_Shadow_RenderSurfacesLighting_Light_GLSL(ent, texture, numsurfaces, surfacelist, lightcolorshirt, vec3_origin, vec3_origin, shirttexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (!doambientbase && !dodiffusebase && !dospecular)
		return;
	R_Mesh_TexMatrix(0, &texture->currenttexmatrix);
	R_Mesh_TexBind(0, R_GetTexture(normalmaptexture));
	R_Mesh_TexBind(1, R_GetTexture(basetexture));
	R_Mesh_TexBind(2, R_GetTexture(glosstexture));
	if (r_shadow_lightpermutation & SHADERPERMUTATION_SPECULAR)
	{
		qglUniform1fARB(qglGetUniformLocationARB(r_shadow_lightprog, "SpecularScale"), specularscale);CHECKGLERROR
	}
	qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "LightColor"), lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);CHECKGLERROR
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const msurface_t *surface = surfacelist[surfacelistindex];
		const int *elements = surface->groupmesh->data_element3i + surface->num_firsttriangle * 3;
		RSurf_SetVertexPointer(ent, texture, surface, modelorg);
		if (!rsurface_svector3f)
		{
			rsurface_svector3f = varray_svector3f;
			rsurface_tvector3f = varray_tvector3f;
			rsurface_normal3f = varray_normal3f;
			Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f, r_smoothnormals_areaweighting.integer);
		}
		R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
		R_Mesh_TexCoordPointer(1, 3, rsurface_svector3f);
		R_Mesh_TexCoordPointer(2, 3, rsurface_tvector3f);
		R_Mesh_TexCoordPointer(3, 3, rsurface_normal3f);
		GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
		GL_LockArrays(0, 0);
	}
}

static void R_Shadow_RenderSurfacesLighting_Light_Dot3(const entity_render_t *ent, const texture_t *texture, int numsurfaces, msurface_t **surfacelist, const vec3_t lightcolorbase, const vec3_t lightcolorpants, const vec3_t lightcolorshirt, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *normalmaptexture, rtexture_t *glosstexture, float specularscale, const vec3_t modelorg)
{
	// ARB path (any Geforce, any Radeon)
	int surfacelistindex;
	int renders;
	float color2[3], colorscale;
	rmeshstate_t m;
	qboolean doambientbase = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean dodiffusebase = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean doambientpants = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean dodiffusepants = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean doambientshirt = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dodiffuseshirt = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dospecular = specularscale * VectorLength2(lightcolorbase) > 0.00001 && glosstexture != r_texture_black;
	// TODO: add direct pants/shirt rendering
	if (doambientpants || dodiffusepants)
		R_Shadow_RenderSurfacesLighting_Light_Dot3(ent, texture, numsurfaces, surfacelist, lightcolorpants, vec3_origin, vec3_origin, pantstexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (doambientshirt || dodiffuseshirt)
		R_Shadow_RenderSurfacesLighting_Light_Dot3(ent, texture, numsurfaces, surfacelist, lightcolorshirt, vec3_origin, vec3_origin, shirttexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (!doambientbase && !dodiffusebase && !dospecular)
		return;
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const msurface_t *surface = surfacelist[surfacelistindex];
		const int *elements = surface->groupmesh->data_element3i + surface->num_firsttriangle * 3;
		RSurf_SetVertexPointer(ent, texture, surface, modelorg);
		if (!rsurface_svector3f)
		{
			rsurface_svector3f = varray_svector3f;
			rsurface_tvector3f = varray_tvector3f;
			rsurface_normal3f = varray_normal3f;
			Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f, r_smoothnormals_areaweighting.integer);
		}
		if (doambientbase)
		{
			GL_Color(1,1,1,1);
			colorscale = r_shadow_rtlight->ambientscale;
			// colorscale accounts for how much we multiply the brightness
			// during combine.
			//
			// mult is how many times the final pass of the lighting will be
			// performed to get more brightness than otherwise possible.
			//
			// Limit mult to 64 for sanity sake.
			if (r_shadow_texture3d.integer && r_shadow_lightcubemap != r_texture_whitecube && r_textureunits.integer >= 4)
			{
				// 3 3D combine path (Geforce3, Radeon 8500)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord3f[0] = varray_texcoord3f[0];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(basetexture);
				m.pointer_texcoord[1] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[1] = texture->currenttexmatrix;
				m.texcubemap[2] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[2] = rsurface_vertex3f;
				m.texmatrix[2] = r_shadow_entitytolight;
#else
				m.pointer_texcoord3f[2] = varray_texcoord3f[2];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[2] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				GL_BlendFunc(GL_ONE, GL_ONE);
			}
			else if (r_shadow_texture3d.integer && r_shadow_lightcubemap == r_texture_whitecube && r_textureunits.integer >= 2)
			{
				// 2 3D combine path (Geforce3, original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord3f[0] = varray_texcoord3f[0];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(basetexture);
				m.pointer_texcoord[1] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[1] = texture->currenttexmatrix;
				GL_BlendFunc(GL_ONE, GL_ONE);
			}
			else if (r_textureunits.integer >= 4 && r_shadow_lightcubemap != r_texture_whitecube)
			{
				// 4 2D combine path (Geforce3, Radeon 8500)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord[0] = varray_texcoord2f[0];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = rsurface_vertex3f;
				m.texmatrix[1] = r_shadow_entitytoattenuationz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
				m.tex[2] = R_GetTexture(basetexture);
				m.pointer_texcoord[2] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[2] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[3] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[3] = rsurface_vertex3f;
					m.texmatrix[3] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[3] = varray_texcoord3f[3];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[3] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_ONE, GL_ONE);
			}
			else if (r_textureunits.integer >= 3 && r_shadow_lightcubemap == r_texture_whitecube)
			{
				// 3 2D combine path (Geforce3, original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord[0] = varray_texcoord2f[0];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = rsurface_vertex3f;
				m.texmatrix[1] = r_shadow_entitytoattenuationz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
				m.tex[2] = R_GetTexture(basetexture);
				m.pointer_texcoord[2] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[2] = texture->currenttexmatrix;
				GL_BlendFunc(GL_ONE, GL_ONE);
			}
			else
			{
				// 2/2/2 2D combine path (any dot3 card)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord[0] = varray_texcoord2f[0];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = rsurface_vertex3f;
				m.texmatrix[1] = r_shadow_entitytoattenuationz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			// this final code is shared
			R_Mesh_State(&m);
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
			VectorScale(lightcolorbase, colorscale, color2);
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				GL_Color(bound(0, color2[0], 1), bound(0, color2[1], 1), bound(0, color2[2], 1), 1);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
			}
			GL_LockArrays(0, 0);
		}
		if (dodiffusebase)
		{
			GL_Color(1,1,1,1);
			colorscale = r_shadow_rtlight->diffusescale;
			// colorscale accounts for how much we multiply the brightness
			// during combine.
			//
			// mult is how many times the final pass of the lighting will be
			// performed to get more brightness than otherwise possible.
			//
			// Limit mult to 64 for sanity sake.
			if (r_shadow_texture3d.integer && r_textureunits.integer >= 4)
			{
				// 3/2 3D combine path (Geforce3, Radeon 8500)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(normalmaptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin);
				m.tex3d[2] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[2] = rsurface_vertex3f;
				m.texmatrix[2] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord3f[2] = varray_texcoord3f[2];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[2] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && r_shadow_lightcubemap != r_texture_whitecube)
			{
				// 1/2/2 3D combine path (original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord3f[0] = varray_texcoord3f[0];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(normalmaptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin);
				R_Mesh_State(&m);
				GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && r_shadow_lightcubemap == r_texture_whitecube)
			{
				// 2/2 3D combine path (original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(normalmaptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin);
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = rsurface_vertex3f;
				m.texmatrix[1] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			else if (r_textureunits.integer >= 4)
			{
				// 4/2 2D combine path (Geforce3, Radeon 8500)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(normalmaptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin);
				m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[2] = rsurface_vertex3f;
				m.texmatrix[2] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord[2] = varray_texcoord2f[2];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[2] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[3] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[3] = rsurface_vertex3f;
				m.texmatrix[3] = r_shadow_entitytoattenuationz;
#else
				m.pointer_texcoord[3] = varray_texcoord2f[3];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[3] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			else
			{
				// 2/2/2 2D combine path (any dot3 card)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = rsurface_vertex3f;
				m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
				m.pointer_texcoord[0] = varray_texcoord2f[0];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = rsurface_vertex3f;
				m.texmatrix[1] = r_shadow_entitytoattenuationz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(normalmaptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin);
				R_Mesh_State(&m);
				GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				GL_LockArrays(0, 0);

				memset(&m, 0, sizeof(m));
				m.pointer_vertex = rsurface_vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
				m.texmatrix[0] = texture->currenttexmatrix;
				if (r_shadow_lightcubemap != r_texture_whitecube)
				{
					m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
				}
				GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			}
			// this final code is shared
			R_Mesh_State(&m);
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
			VectorScale(lightcolorbase, colorscale, color2);
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				GL_Color(bound(0, color2[0], 1), bound(0, color2[1], 1), bound(0, color2[2], 1), 1);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
			}
			GL_LockArrays(0, 0);
		}
		if (dospecular)
		{
			// FIXME: detect blendsquare!
			//if (gl_support_blendsquare)
			{
				colorscale = specularscale;
				GL_Color(1,1,1,1);
				if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && r_shadow_lightcubemap != r_texture_whitecube /* && gl_support_blendsquare*/) // FIXME: detect blendsquare!
				{
					// 2/0/0/1/2 3D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(normalmaptexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin, r_shadow_entityeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[0] = rsurface_vertex3f;
					m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
					m.pointer_texcoord3f[0] = varray_texcoord3f[0];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
					R_Mesh_State(&m);
					GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					if (r_shadow_lightcubemap != r_texture_whitecube)
					{
						m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
						m.pointer_texcoord3f[1] = rsurface_vertex3f;
						m.texmatrix[1] = r_shadow_entitytolight;
#else
						m.pointer_texcoord3f[1] = varray_texcoord3f[1];
						R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
					}
					GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
				}
				else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && r_shadow_lightcubemap == r_texture_whitecube /* && gl_support_blendsquare*/) // FIXME: detect blendsquare!
				{
					// 2/0/0/2 3D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(normalmaptexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin, r_shadow_entityeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytoattenuationxyz;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
					GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
				}
				else
				{
					// 2/0/0/2/2 2D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(normalmaptexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					m.texcubemap[1] = R_GetTexture(r_texture_normalizationcube);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_svector3f + 3 * surface->num_firstvertex, rsurface_tvector3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, r_shadow_entitylightorigin, r_shadow_entityeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[0] = rsurface_vertex3f;
					m.texmatrix[0] = r_shadow_entitytoattenuationxyz;
#else
					m.pointer_texcoord[0] = varray_texcoord2f[0];
					R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[0] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
					m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = rsurface_vertex3f;
					m.texmatrix[1] = r_shadow_entitytoattenuationz;
#else
					m.pointer_texcoord[1] = varray_texcoord2f[1];
					R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
					R_Mesh_State(&m);
					GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
					GL_LockArrays(0, 0);

					memset(&m, 0, sizeof(m));
					m.pointer_vertex = rsurface_vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = surface->groupmesh->data_texcoordtexture2f;
					m.texmatrix[0] = texture->currenttexmatrix;
					if (r_shadow_lightcubemap != r_texture_whitecube)
					{
						m.texcubemap[1] = R_GetTexture(r_shadow_lightcubemap);
#ifdef USETEXMATRIX
						m.pointer_texcoord3f[1] = rsurface_vertex3f;
						m.texmatrix[1] = r_shadow_entitytolight;
#else
						m.pointer_texcoord3f[1] = varray_texcoord3f[1];
						R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytolight);
#endif
					}
					GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
				}
				R_Mesh_State(&m);
				GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
				VectorScale(lightcolorbase, colorscale, color2);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
				{
					GL_Color(bound(0, color2[0], 1), bound(0, color2[1], 1), bound(0, color2[2], 1), 1);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
				}
				GL_LockArrays(0, 0);
			}
		}
	}
}

static void R_Shadow_RenderSurfacesLighting_Light_Vertex(const entity_render_t *ent, const texture_t *texture, int numsurfaces, msurface_t **surfacelist, const vec3_t lightcolorbase, const vec3_t lightcolorpants, const vec3_t lightcolorshirt, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *normalmaptexture, rtexture_t *glosstexture, float specularscale, const vec3_t modelorg)
{
	int surfacelistindex;
	int renders;
	float ambientcolor2[3], diffusecolor2[3];
	rmeshstate_t m;
	qboolean doambientbase = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean dodiffusebase = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorbase) > 0.00001 && basetexture != r_texture_black;
	qboolean doambientpants = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean dodiffusepants = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorpants) > 0.00001 && pantstexture != r_texture_black;
	qboolean doambientshirt = r_shadow_rtlight->ambientscale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	qboolean dodiffuseshirt = r_shadow_rtlight->diffusescale * VectorLength2(lightcolorshirt) > 0.00001 && shirttexture != r_texture_black;
	//qboolean dospecular = specularscale * VectorLength2(lightcolorbase) > 0.00001 && glosstexture != r_texture_black;
	// TODO: add direct pants/shirt rendering
	if (doambientpants || dodiffusepants)
		R_Shadow_RenderSurfacesLighting_Light_Vertex(ent, texture, numsurfaces, surfacelist, lightcolorpants, vec3_origin, vec3_origin, pantstexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (doambientshirt || dodiffuseshirt)
		R_Shadow_RenderSurfacesLighting_Light_Vertex(ent, texture, numsurfaces, surfacelist, lightcolorshirt, vec3_origin, vec3_origin, shirttexture, r_texture_black, r_texture_black, normalmaptexture, r_texture_black, 0, modelorg);
	if (!doambientbase && !dodiffusebase)
		return;
	VectorScale(lightcolorbase, r_shadow_rtlight->ambientscale, ambientcolor2);
	VectorScale(lightcolorbase, r_shadow_rtlight->diffusescale, diffusecolor2);
	GL_BlendFunc(GL_ONE, GL_ONE);
	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(basetexture);
	if (r_textureunits.integer >= 2)
	{
		// voodoo2
		m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
		m.texmatrix[1] = r_shadow_entitytoattenuationxyz;
#else
		m.pointer_texcoord[1] = varray_texcoord2f[1];
		R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
		if (r_textureunits.integer >= 3)
		{
			// Geforce3/Radeon class but not using dot3
			m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
			m.texmatrix[2] = r_shadow_entitytoattenuationz;
#else
			m.pointer_texcoord[2] = varray_texcoord2f[2];
			R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[2] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
		}
	}
	m.pointer_color = varray_color4f;
	R_Mesh_State(&m);
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const msurface_t *surface = surfacelist[surfacelistindex];
		const int *elements = surface->groupmesh->data_element3i + surface->num_firsttriangle * 3;
		RSurf_SetVertexPointer(ent, texture, surface, modelorg);
		if (!rsurface_svector3f)
		{
			rsurface_svector3f = varray_svector3f;
			rsurface_tvector3f = varray_tvector3f;
			rsurface_normal3f = varray_normal3f;
			Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f, r_smoothnormals_areaweighting.integer);
		}
		// OpenGL 1.1 path (anything)
		R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
		R_Mesh_TexMatrix(0, &texture->currenttexmatrix);
		if (r_textureunits.integer >= 2)
		{
			// voodoo2 or TNT
#ifdef USETEXMATRIX
			R_Mesh_TexCoordPointer(1, 3, rsurface_vertex3f);
#else
			R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[1] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationxyz);
#endif
			if (r_textureunits.integer >= 3)
			{
				// Voodoo4 or Kyro (or Geforce3/Radeon with gl_combine off)
#ifdef USETEXMATRIX
				R_Mesh_TexCoordPointer(2, 3, rsurface_vertex3f);
#else
				R_Shadow_Transform_Vertex3f_Texcoord2f(varray_texcoord2f[2] + 3 * surface->num_firstvertex, surface->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, &r_shadow_entitytoattenuationz);
#endif
			}
		}
		R_Shadow_RenderSurfacesLighting_Light_Vertex_Shading(surface, diffusecolor2, ambientcolor2, 0, modelorg);
		for (renders = 0;renders < 64 && (ambientcolor2[0] > renders || ambientcolor2[1] > renders || ambientcolor2[2] > renders || diffusecolor2[0] > renders || diffusecolor2[1] > renders || diffusecolor2[2] > renders);renders++)
		{
			int i;
			float *c;
#if 1
			// due to low fillrate on the cards this vertex lighting path is
			// designed for, we manually cull all triangles that do not
			// contain a lit vertex
			int draw;
			const int *e;
			int newnumtriangles;
			int *newe;
			int newelements[3072];
			draw = false;
			newnumtriangles = 0;
			newe = newelements;
			for (i = 0, e = elements;i < surface->num_triangles;i++, e += 3)
			{
				if (newnumtriangles >= 1024)
				{
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, newnumtriangles, newelements);
					GL_LockArrays(0, 0);
					newnumtriangles = 0;
					newe = newelements;
				}
				if (VectorLength2(varray_color4f + e[0] * 4) + VectorLength2(varray_color4f + e[1] * 4) + VectorLength2(varray_color4f + e[2] * 4) >= 0.01)
				{
					newe[0] = e[0];
					newe[1] = e[1];
					newe[2] = e[2];
					newnumtriangles++;
					newe += 3;
					draw = true;
				}
			}
			if (newnumtriangles >= 1)
			{
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, newnumtriangles, newelements);
				GL_LockArrays(0, 0);
				draw = true;
			}
			if (!draw)
				break;
#else
			for (i = 0, c = varray_color4f + 4 * surface->num_firstvertex;i < surface->num_vertices;i++, c += 4)
				if (VectorLength2(c))
					goto goodpass;
			break;
goodpass:
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, elements);
			GL_LockArrays(0, 0);
#endif
			// now reduce the intensity for the next overbright pass
			for (i = 0, c = varray_color4f + 4 * surface->num_firstvertex;i < surface->num_vertices;i++, c += 4)
			{
				c[0] = max(0, c[0] - 1);
				c[1] = max(0, c[1] - 1);
				c[2] = max(0, c[2] - 1);
			}
		}
	}
}

void R_Shadow_RenderSurfacesLighting(const entity_render_t *ent, const texture_t *texture, int numsurfaces, msurface_t **surfacelist, const vec3_t lightcolorbase, const vec3_t lightcolorpants, const vec3_t lightcolorshirt, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *normalmaptexture, rtexture_t *glosstexture, float specularscale, const vec3_t modelorg)
{
	// FIXME: support MATERIALFLAG_NODEPTHTEST
	switch (r_shadowstage)
	{
	case R_SHADOWSTAGE_VISIBLELIGHTING:
		R_Shadow_RenderSurfacesLighting_VisibleLighting(ent, texture, numsurfaces, surfacelist, lightcolorbase, lightcolorpants, lightcolorshirt, basetexture, texture->skin.pants, texture->skin.shirt, texture->skin.nmap, glosstexture, specularscale, modelorg);
		break;
	case R_SHADOWSTAGE_LIGHT_GLSL:
		R_Shadow_RenderSurfacesLighting_Light_GLSL(ent, texture, numsurfaces, surfacelist, lightcolorbase, lightcolorpants, lightcolorshirt, basetexture, texture->skin.pants, texture->skin.shirt, texture->skin.nmap, glosstexture, specularscale, modelorg);
		break;
	case R_SHADOWSTAGE_LIGHT_DOT3:
		R_Shadow_RenderSurfacesLighting_Light_Dot3(ent, texture, numsurfaces, surfacelist, lightcolorbase, lightcolorpants, lightcolorshirt, basetexture, texture->skin.pants, texture->skin.shirt, texture->skin.nmap, glosstexture, specularscale, modelorg);
		break;
	case R_SHADOWSTAGE_LIGHT_VERTEX:
		R_Shadow_RenderSurfacesLighting_Light_Vertex(ent, texture, numsurfaces, surfacelist, lightcolorbase, lightcolorpants, lightcolorshirt, basetexture, texture->skin.pants, texture->skin.shirt, texture->skin.nmap, glosstexture, specularscale, modelorg);
		break;
	default:
		Con_Printf("R_Shadow_RenderLighting: unknown r_shadowstage %i\n", r_shadowstage);
		break;
	}
}

void R_RTLight_Update(dlight_t *light, int isstatic)
{
	int j, k;
	float scale;
	rtlight_t *rtlight = &light->rtlight;
	R_RTLight_Uncompile(rtlight);
	memset(rtlight, 0, sizeof(*rtlight));

	VectorCopy(light->origin, rtlight->shadoworigin);
	VectorCopy(light->color, rtlight->color);
	rtlight->radius = light->radius;
	//rtlight->cullradius = rtlight->radius;
	//rtlight->cullradius2 = rtlight->radius * rtlight->radius;
	rtlight->cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	rtlight->cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	rtlight->cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	rtlight->cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	rtlight->cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	rtlight->cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;
	rtlight->cubemapname[0] = 0;
	if (light->cubemapname[0])
		strcpy(rtlight->cubemapname, light->cubemapname);
	else if (light->cubemapnum > 0)
		sprintf(rtlight->cubemapname, "cubemaps/%i", light->cubemapnum);
	rtlight->shadow = light->shadow;
	rtlight->corona = light->corona;
	rtlight->style = light->style;
	rtlight->isstatic = isstatic;
	rtlight->coronasizescale = light->coronasizescale;
	rtlight->ambientscale = light->ambientscale;
	rtlight->diffusescale = light->diffusescale;
	rtlight->specularscale = light->specularscale;
	rtlight->flags = light->flags;
	Matrix4x4_Invert_Simple(&rtlight->matrix_worldtolight, &light->matrix);
	// ConcatScale won't work here because this needs to scale rotate and
	// translate, not just rotate
	scale = 1.0f / rtlight->radius;
	for (k = 0;k < 3;k++)
		for (j = 0;j < 4;j++)
			rtlight->matrix_worldtolight.m[k][j] *= scale;

	rtlight->lightmap_cullradius = bound(0, rtlight->radius, 2048.0f);
	rtlight->lightmap_cullradius2 = rtlight->lightmap_cullradius * rtlight->lightmap_cullradius;
	VectorScale(rtlight->color, rtlight->radius * (rtlight->style >= 0 ? r_refdef.lightstylevalue[rtlight->style] : 128) * 0.125f, rtlight->lightmap_light);
	rtlight->lightmap_subtract = 1.0f / rtlight->lightmap_cullradius2;
}

// compiles rtlight geometry
// (undone by R_FreeCompiledRTLight, which R_UpdateLight calls)
void R_RTLight_Compile(rtlight_t *rtlight)
{
	int shadowmeshes, shadowtris, numleafs, numleafpvsbytes, numsurfaces;
	entity_render_t *ent = r_refdef.worldentity;
	model_t *model = r_refdef.worldmodel;
	qbyte *data;

	// compile the light
	rtlight->compiled = true;
	rtlight->static_numleafs = 0;
	rtlight->static_numleafpvsbytes = 0;
	rtlight->static_leaflist = NULL;
	rtlight->static_leafpvs = NULL;
	rtlight->static_numsurfaces = 0;
	rtlight->static_surfacelist = NULL;
	rtlight->cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	rtlight->cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	rtlight->cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	rtlight->cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	rtlight->cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	rtlight->cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;

	if (model && model->GetLightInfo)
	{
		// this variable must be set for the CompileShadowVolume code
		r_shadow_compilingrtlight = rtlight;
		R_Shadow_EnlargeLeafSurfaceBuffer(model->brush.num_leafs, model->num_surfaces);
		model->GetLightInfo(ent, rtlight->shadoworigin, rtlight->radius, rtlight->cullmins, rtlight->cullmaxs, r_shadow_buffer_leaflist, r_shadow_buffer_leafpvs, &numleafs, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces);
		numleafpvsbytes = (model->brush.num_leafs + 7) >> 3;
		data = (qbyte *)Mem_Alloc(r_shadow_mempool, sizeof(int) * numleafs + numleafpvsbytes + sizeof(int) * numsurfaces);
		rtlight->static_numleafs = numleafs;
		rtlight->static_numleafpvsbytes = numleafpvsbytes;
		rtlight->static_leaflist = (int *)data;data += sizeof(int) * numleafs;
		rtlight->static_leafpvs = (qbyte *)data;data += numleafpvsbytes;
		rtlight->static_numsurfaces = numsurfaces;
		rtlight->static_surfacelist = (int *)data;data += sizeof(int) * numsurfaces;
		if (numleafs)
			memcpy(rtlight->static_leaflist, r_shadow_buffer_leaflist, rtlight->static_numleafs * sizeof(*rtlight->static_leaflist));
		if (numleafpvsbytes)
			memcpy(rtlight->static_leafpvs, r_shadow_buffer_leafpvs, rtlight->static_numleafpvsbytes);
		if (numsurfaces)
			memcpy(rtlight->static_surfacelist, r_shadow_buffer_surfacelist, rtlight->static_numsurfaces * sizeof(*rtlight->static_surfacelist));
		if (model->CompileShadowVolume && rtlight->shadow)
			model->CompileShadowVolume(ent, rtlight->shadoworigin, rtlight->radius, numsurfaces, r_shadow_buffer_surfacelist);
		// now we're done compiling the rtlight
		r_shadow_compilingrtlight = NULL;
	}


	// use smallest available cullradius - box radius or light radius
	//rtlight->cullradius = RadiusFromBoundsAndOrigin(rtlight->cullmins, rtlight->cullmaxs, rtlight->shadoworigin);
	//rtlight->cullradius = min(rtlight->cullradius, rtlight->radius);

	shadowmeshes = 0;
	shadowtris = 0;
	if (rtlight->static_meshchain_shadow)
	{
		shadowmesh_t *mesh;
		for (mesh = rtlight->static_meshchain_shadow;mesh;mesh = mesh->next)
		{
			shadowmeshes++;
			shadowtris += mesh->numtriangles;
		}
	}

	Con_DPrintf("static light built: %f %f %f : %f %f %f box, %i shadow volume triangles (in %i meshes)\n", rtlight->cullmins[0], rtlight->cullmins[1], rtlight->cullmins[2], rtlight->cullmaxs[0], rtlight->cullmaxs[1], rtlight->cullmaxs[2], shadowtris, shadowmeshes);
}

void R_RTLight_Uncompile(rtlight_t *rtlight)
{
	if (rtlight->compiled)
	{
		if (rtlight->static_meshchain_shadow)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_shadow);
		rtlight->static_meshchain_shadow = NULL;
		// these allocations are grouped
		if (rtlight->static_leaflist)
			Mem_Free(rtlight->static_leaflist);
		rtlight->static_numleafs = 0;
		rtlight->static_numleafpvsbytes = 0;
		rtlight->static_leaflist = NULL;
		rtlight->static_leafpvs = NULL;
		rtlight->static_numsurfaces = 0;
		rtlight->static_surfacelist = NULL;
		rtlight->compiled = false;
	}
}

void R_Shadow_UncompileWorldLights(void)
{
	dlight_t *light;
	for (light = r_shadow_worldlightchain;light;light = light->next)
		R_RTLight_Uncompile(&light->rtlight);
}

void R_Shadow_DrawEntityShadow(entity_render_t *ent, rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	vec3_t relativeshadoworigin, relativeshadowmins, relativeshadowmaxs;
	vec_t relativeshadowradius;
	if (ent == r_refdef.worldentity)
	{
		if (rtlight->compiled && r_shadow_realtime_world_compile.integer && r_shadow_realtime_world_compileshadow.integer)
		{
			shadowmesh_t *mesh;
			R_Mesh_Matrix(&ent->matrix);
			for (mesh = rtlight->static_meshchain_shadow;mesh;mesh = mesh->next)
			{
				renderstats.lights_shadowtriangles += mesh->numtriangles;
				R_Mesh_VertexPointer(mesh->vertex3f);
				GL_LockArrays(0, mesh->numverts);
				if (r_shadowstage == R_SHADOWSTAGE_STENCIL)
				{
					// decrement stencil if backface is behind depthbuffer
					qglCullFace(GL_BACK); // quake is backwards, this culls front faces
					qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
					R_Mesh_Draw(0, mesh->numverts, mesh->numtriangles, mesh->element3i);
					// increment stencil if frontface is behind depthbuffer
					qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
					qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
				}
				R_Mesh_Draw(0, mesh->numverts, mesh->numtriangles, mesh->element3i);
				GL_LockArrays(0, 0);
			}
		}
		else if (numsurfaces)
		{
			R_Mesh_Matrix(&ent->matrix);
			ent->model->DrawShadowVolume(ent, rtlight->shadoworigin, rtlight->radius, numsurfaces, surfacelist, rtlight->cullmins, rtlight->cullmaxs);
		}
	}
	else
	{
		Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, relativeshadoworigin);
		relativeshadowradius = rtlight->radius / ent->scale;
		relativeshadowmins[0] = relativeshadoworigin[0] - relativeshadowradius;
		relativeshadowmins[1] = relativeshadoworigin[1] - relativeshadowradius;
		relativeshadowmins[2] = relativeshadoworigin[2] - relativeshadowradius;
		relativeshadowmaxs[0] = relativeshadoworigin[0] + relativeshadowradius;
		relativeshadowmaxs[1] = relativeshadoworigin[1] + relativeshadowradius;
		relativeshadowmaxs[2] = relativeshadoworigin[2] + relativeshadowradius;
		R_Mesh_Matrix(&ent->matrix);
		ent->model->DrawShadowVolume(ent, relativeshadoworigin, relativeshadowradius, ent->model->nummodelsurfaces, ent->model->surfacelist, relativeshadowmins, relativeshadowmaxs);
	}
}

void R_Shadow_DrawEntityLight(entity_render_t *ent, rtlight_t *rtlight, vec3_t lightcolor, int numsurfaces, int *surfacelist)
{
	// set up properties for rendering light onto this entity
	r_shadow_entitylightcolorbase[0] = lightcolor[0] * ent->colormod[0] * ent->alpha;
	r_shadow_entitylightcolorbase[1] = lightcolor[1] * ent->colormod[1] * ent->alpha;
	r_shadow_entitylightcolorbase[2] = lightcolor[2] * ent->colormod[2] * ent->alpha;
	r_shadow_entitylightcolorpants[0] = lightcolor[0] * ent->colormap_pantscolor[0] * ent->alpha;
	r_shadow_entitylightcolorpants[1] = lightcolor[1] * ent->colormap_pantscolor[1] * ent->alpha;
	r_shadow_entitylightcolorpants[2] = lightcolor[2] * ent->colormap_pantscolor[2] * ent->alpha;
	r_shadow_entitylightcolorshirt[0] = lightcolor[0] * ent->colormap_shirtcolor[0] * ent->alpha;
	r_shadow_entitylightcolorshirt[1] = lightcolor[1] * ent->colormap_shirtcolor[1] * ent->alpha;
	r_shadow_entitylightcolorshirt[2] = lightcolor[2] * ent->colormap_shirtcolor[2] * ent->alpha;
	Matrix4x4_Concat(&r_shadow_entitytolight, &rtlight->matrix_worldtolight, &ent->matrix);
	Matrix4x4_Concat(&r_shadow_entitytoattenuationxyz, &matrix_attenuationxyz, &r_shadow_entitytolight);
	Matrix4x4_Concat(&r_shadow_entitytoattenuationz, &matrix_attenuationz, &r_shadow_entitytolight);
	Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, r_shadow_entitylightorigin);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, r_shadow_entityeyeorigin);
	R_Mesh_Matrix(&ent->matrix);
	if (r_shadowstage == R_SHADOWSTAGE_LIGHT_GLSL)
	{
		R_Mesh_TexBindCubeMap(3, R_GetTexture(r_shadow_lightcubemap));
		R_Mesh_TexMatrix(3, &r_shadow_entitytolight);
		qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "LightPosition"), r_shadow_entitylightorigin[0], r_shadow_entitylightorigin[1], r_shadow_entitylightorigin[2]);CHECKGLERROR
		if (r_shadow_lightpermutation & (SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_FOG | SHADERPERMUTATION_OFFSETMAPPING))
		{
			qglUniform3fARB(qglGetUniformLocationARB(r_shadow_lightprog, "EyePosition"), r_shadow_entityeyeorigin[0], r_shadow_entityeyeorigin[1], r_shadow_entityeyeorigin[2]);CHECKGLERROR
		}
	}
	if (ent == r_refdef.worldentity)
		ent->model->DrawLight(ent, r_shadow_entitylightcolorbase, r_shadow_entitylightcolorpants, r_shadow_entitylightcolorshirt, numsurfaces, surfacelist);
	else
		ent->model->DrawLight(ent, r_shadow_entitylightcolorbase, r_shadow_entitylightcolorpants, r_shadow_entitylightcolorshirt, ent->model->nummodelsurfaces, ent->model->surfacelist);
}

void R_DrawRTLight(rtlight_t *rtlight, qboolean visible)
{
	int i, usestencil;
	float f;
	vec3_t lightcolor;
	int numleafs, numsurfaces;
	int *leaflist, *surfacelist;
	qbyte *leafpvs;
	int numlightentities;
	int numshadowentities;
	entity_render_t *lightentities[MAX_EDICTS];
	entity_render_t *shadowentities[MAX_EDICTS];

	// skip lights that don't light (corona only lights)
	if (rtlight->ambientscale + rtlight->diffusescale + rtlight->specularscale < (1.0f / 32768.0f))
		return;

	f = (rtlight->style >= 0 ? r_refdef.lightstylevalue[rtlight->style] : 128) * (1.0f / 256.0f) * r_shadow_lightintensityscale.value;
	VectorScale(rtlight->color, f, lightcolor);
	if (VectorLength2(lightcolor) < (1.0f / 32768.0f))
		return;
	/*
	if (rtlight->selected)
	{
		f = 2 + sin(realtime * M_PI * 4.0);
		VectorScale(lightcolor, f, lightcolor);
	}
	*/

	// loading is done before visibility checks because loading should happen
	// all at once at the start of a level, not when it stalls gameplay.
	// (especially important to benchmarks)
	// compile light
	if (rtlight->isstatic && !rtlight->compiled && r_shadow_realtime_world_compile.integer)
		R_RTLight_Compile(rtlight);
	// load cubemap
	r_shadow_lightcubemap = rtlight->cubemapname[0] ? R_Shadow_Cubemap(rtlight->cubemapname) : r_texture_whitecube;

	// if the light box is offscreen, skip it
	if (R_CullBox(rtlight->cullmins, rtlight->cullmaxs))
		return;

	if (rtlight->compiled && r_shadow_realtime_world_compile.integer)
	{
		// compiled light, world available and can receive realtime lighting
		// retrieve leaf information
		numleafs = rtlight->static_numleafs;
		leaflist = rtlight->static_leaflist;
		leafpvs = rtlight->static_leafpvs;
		numsurfaces = rtlight->static_numsurfaces;
		surfacelist = rtlight->static_surfacelist;
	}
	else if (r_refdef.worldmodel && r_refdef.worldmodel->GetLightInfo)
	{
		// dynamic light, world available and can receive realtime lighting
		// calculate lit surfaces and leafs
		R_Shadow_EnlargeLeafSurfaceBuffer(r_refdef.worldmodel->brush.num_leafs, r_refdef.worldmodel->num_surfaces);
		r_refdef.worldmodel->GetLightInfo(r_refdef.worldentity, rtlight->shadoworigin, rtlight->radius, rtlight->cullmins, rtlight->cullmaxs, r_shadow_buffer_leaflist, r_shadow_buffer_leafpvs, &numleafs, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces);
		leaflist = r_shadow_buffer_leaflist;
		leafpvs = r_shadow_buffer_leafpvs;
		surfacelist = r_shadow_buffer_surfacelist;
		// if the reduced leaf bounds are offscreen, skip it
		if (R_CullBox(rtlight->cullmins, rtlight->cullmaxs))
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
	}
	// check if light is illuminating any visible leafs
	if (numleafs)
	{
		for (i = 0;i < numleafs;i++)
			if (r_worldleafvisible[leaflist[i]])
				break;
		if (i == numleafs)
			return;
	}
	// set up a scissor rectangle for this light
	if (R_Shadow_ScissorForBBox(rtlight->cullmins, rtlight->cullmaxs))
		return;

	numlightentities = 0;
	if (numsurfaces)
		lightentities[numlightentities++] = r_refdef.worldentity;
	numshadowentities = 0;
	if (numsurfaces)
		shadowentities[numshadowentities++] = r_refdef.worldentity;
	if (r_drawentities.integer)
	{
		for (i = 0;i < r_refdef.numentities;i++)
		{
			entity_render_t *ent = r_refdef.entities[i];
			if (BoxesOverlap(ent->mins, ent->maxs, rtlight->cullmins, rtlight->cullmaxs)
			 && ent->model
			 && !(ent->flags & RENDER_TRANSPARENT)
			 && (r_refdef.worldmodel == NULL || r_refdef.worldmodel->brush.BoxTouchingLeafPVS == NULL || r_refdef.worldmodel->brush.BoxTouchingLeafPVS(r_refdef.worldmodel, leafpvs, ent->mins, ent->maxs)))
			{
				// about the VectorDistance2 - light emitting entities should not cast their own shadow
				if ((ent->flags & RENDER_SHADOW) && ent->model->DrawShadowVolume && VectorDistance2(ent->origin, rtlight->shadoworigin) > 0.1)
					shadowentities[numshadowentities++] = ent;
				if (ent->visframe == r_framecount && (ent->flags & RENDER_LIGHT) && ent->model->DrawLight)
					lightentities[numlightentities++] = ent;
			}
		}
	}

	// return if there's nothing at all to light
	if (!numlightentities)
		return;

	R_Shadow_Stage_ActiveLight(rtlight);
	renderstats.lights++;

	usestencil = false;
	if (numshadowentities && (!visible || r_shadow_visiblelighting.integer == 1) && gl_stencil && rtlight->shadow && (rtlight->isstatic ? r_rtworldshadows : r_rtdlightshadows))
	{
		usestencil = true;
		R_Shadow_Stage_StencilShadowVolumes();
		for (i = 0;i < numshadowentities;i++)
			R_Shadow_DrawEntityShadow(shadowentities[i], rtlight, numsurfaces, surfacelist);
	}

	if (numlightentities && !visible)
	{
		R_Shadow_Stage_Lighting(usestencil);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i], rtlight, lightcolor, numsurfaces, surfacelist);
	}

	if (numshadowentities && visible && r_shadow_visiblevolumes.integer > 0 && rtlight->shadow && (rtlight->isstatic ? r_rtworldshadows : r_rtdlightshadows))
	{
		R_Shadow_Stage_VisibleShadowVolumes();
		for (i = 0;i < numshadowentities;i++)
			R_Shadow_DrawEntityShadow(shadowentities[i], rtlight, numsurfaces, surfacelist);
	}

	if (numlightentities && visible && r_shadow_visiblelighting.integer > 0)
	{
		R_Shadow_Stage_VisibleLighting(usestencil);
		for (i = 0;i < numlightentities;i++)
			R_Shadow_DrawEntityLight(lightentities[i], rtlight, lightcolor, numsurfaces, surfacelist);
	}
}

void R_ShadowVolumeLighting(qboolean visible)
{
	int lnum, flag;
	dlight_t *light;

	if (r_refdef.worldmodel && strncmp(r_refdef.worldmodel->name, r_shadow_mapname, sizeof(r_shadow_mapname)))
		R_Shadow_EditLights_Reload_f();

	R_Shadow_Stage_Begin();

	flag = r_rtworld ? LIGHTFLAG_REALTIMEMODE : LIGHTFLAG_NORMALMODE;
	if (r_shadow_debuglight.integer >= 0)
	{
		for (lnum = 0, light = r_shadow_worldlightchain;light;lnum++, light = light->next)
			if (lnum == r_shadow_debuglight.integer && (light->flags & flag))
				R_DrawRTLight(&light->rtlight, visible);
	}
	else
		for (lnum = 0, light = r_shadow_worldlightchain;light;lnum++, light = light->next)
			if (light->flags & flag)
				R_DrawRTLight(&light->rtlight, visible);
	if (r_rtdlight)
		for (lnum = 0;lnum < r_refdef.numlights;lnum++)
			R_DrawRTLight(&r_refdef.lights[lnum]->rtlight, visible);

	R_Shadow_Stage_End();
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
	qbyte *cubemappixels, *image_rgba;
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
			if ((image_rgba = loadimagepixels(name, false, cubemapsize, cubemapsize)))
			{
				// an image loaded, make sure width and height are equal
				if (image_width == image_height)
				{
					// if this is the first image to load successfully, allocate the cubemap memory
					if (!cubemappixels && image_width >= 1)
					{
						cubemapsize = image_width;
						// note this clears to black, so unavailable sides are black
						cubemappixels = (qbyte *)Mem_Alloc(tempmempool, 6*cubemapsize*cubemapsize*4);
					}
					// copy the image with any flipping needed by the suffix (px and posx types don't need flipping)
					if (cubemappixels)
						Image_CopyMux(cubemappixels+i*cubemapsize*cubemapsize*4, image_rgba, cubemapsize, cubemapsize, suffix[j][i].flipx, suffix[j][i].flipy, suffix[j][i].flipdiagonal, 4, 4, componentorder);
				}
				else
					Con_Printf("Cubemap image \"%s\" (%ix%i) is not square, OpenGL requires square cubemaps.\n", name, image_width, image_height);
				// free the image
				Mem_Free(image_rgba);
			}
		}
	}
	// if a cubemap loaded, upload it
	if (cubemappixels)
	{
		if (!r_shadow_filters_texturepool)
			r_shadow_filters_texturepool = R_AllocTexturePool();
		cubemaptexture = R_LoadTextureCubeMap(r_shadow_filters_texturepool, basename, cubemapsize, cubemappixels, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
		Mem_Free(cubemappixels);
	}
	else
	{
		Con_Printf("Failed to load Cubemap \"%s\", tried ", basename);
		for (j = 0;j < 3;j++)
			for (i = 0;i < 6;i++)
				Con_Printf("%s\"%s%s.tga\"", j + i > 0 ? ", " : "", basename, suffix[j][i].suffix);
		Con_Print(" and was unable to find any of them.\n");
	}
	return cubemaptexture;
}

rtexture_t *R_Shadow_Cubemap(const char *basename)
{
	int i;
	for (i = 0;i < numcubemaps;i++)
		if (!strcasecmp(cubemaps[i].basename, basename))
			return cubemaps[i].texture;
	if (i >= MAX_CUBEMAPS)
		return r_texture_whitecube;
	numcubemaps++;
	strcpy(cubemaps[i].basename, basename);
	cubemaps[i].texture = R_Shadow_LoadCubemap(cubemaps[i].basename);
	if (!cubemaps[i].texture)
		cubemaps[i].texture = r_texture_whitecube;
	return cubemaps[i].texture;
}

void R_Shadow_FreeCubemaps(void)
{
	numcubemaps = 0;
	R_FreeTexturePool(&r_shadow_filters_texturepool);
}

dlight_t *R_Shadow_NewWorldLight(void)
{
	dlight_t *light;
	light = (dlight_t *)Mem_Alloc(r_shadow_mempool, sizeof(dlight_t));
	light->next = r_shadow_worldlightchain;
	r_shadow_worldlightchain = light;
	return light;
}

void R_Shadow_UpdateWorldLight(dlight_t *light, vec3_t origin, vec3_t angles, vec3_t color, vec_t radius, vec_t corona, int style, int shadowenable, const char *cubemapname, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
	VectorCopy(origin, light->origin);
	light->angles[0] = angles[0] - 360 * floor(angles[0] / 360);
	light->angles[1] = angles[1] - 360 * floor(angles[1] / 360);
	light->angles[2] = angles[2] - 360 * floor(angles[2] / 360);
	light->color[0] = max(color[0], 0);
	light->color[1] = max(color[1], 0);
	light->color[2] = max(color[2], 0);
	light->radius = max(radius, 0);
	light->style = style;
	if (light->style < 0 || light->style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("R_Shadow_NewWorldLight: invalid light style number %i, must be >= 0 and < %i\n", light->style, MAX_LIGHTSTYLES);
		light->style = 0;
	}
	light->shadow = shadowenable;
	light->corona = corona;
	if (!cubemapname)
		cubemapname = "";
	strlcpy(light->cubemapname, cubemapname, sizeof(light->cubemapname));
	light->coronasizescale = coronasizescale;
	light->ambientscale = ambientscale;
	light->diffusescale = diffusescale;
	light->specularscale = specularscale;
	light->flags = flags;
	Matrix4x4_CreateFromQuakeEntity(&light->matrix, light->origin[0], light->origin[1], light->origin[2], light->angles[0], light->angles[1], light->angles[2], 1);

	R_RTLight_Update(light, true);
}

void R_Shadow_FreeWorldLight(dlight_t *light)
{
	dlight_t **lightpointer;
	R_RTLight_Uncompile(&light->rtlight);
	for (lightpointer = &r_shadow_worldlightchain;*lightpointer && *lightpointer != light;lightpointer = &(*lightpointer)->next);
	if (*lightpointer != light)
		Sys_Error("R_Shadow_FreeWorldLight: light not linked into chain\n");
	*lightpointer = light->next;
	Mem_Free(light);
}

void R_Shadow_ClearWorldLights(void)
{
	while (r_shadow_worldlightchain)
		R_Shadow_FreeWorldLight(r_shadow_worldlightchain);
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

void R_Shadow_DrawCursorCallback(const void *calldata1, int calldata2)
{
	float scale = r_editlights_cursorgrid.value * 0.5f;
	R_DrawSprite(GL_ONE, GL_ONE, lighttextures[0], NULL, false, r_editlights_cursorlocation, r_viewright, r_viewup, scale, -scale, -scale, scale, 1, 1, 1, 0.5f);
}

void R_Shadow_DrawLightSpriteCallback(const void *calldata1, int calldata2)
{
	float intensity;
	const dlight_t *light;
	light = (dlight_t *)calldata1;
	intensity = 0.5;
	if (light->selected)
		intensity = 0.75 + 0.25 * sin(realtime * M_PI * 4.0);
	if (!light->shadow)
		intensity *= 0.5f;
	R_DrawSprite(GL_ONE, GL_ONE, lighttextures[calldata2], NULL, false, light->origin, r_viewright, r_viewup, 8, -8, -8, 8, intensity, intensity, intensity, 0.5);
}

void R_Shadow_DrawLightSprites(void)
{
	int i;
	cachepic_t *pic;
	dlight_t *light;

	for (i = 0;i < 5;i++)
	{
		lighttextures[i] = NULL;
		if ((pic = Draw_CachePic(va("gfx/crosshair%i.tga", i + 1), true)))
			lighttextures[i] = pic->tex;
	}

	for (i = 0, light = r_shadow_worldlightchain;light;i++, light = light->next)
		R_MeshQueue_AddTransparent(light->origin, R_Shadow_DrawLightSpriteCallback, light, i % 5);
	R_MeshQueue_AddTransparent(r_editlights_cursorlocation, R_Shadow_DrawCursorCallback, NULL, 0);
}

void R_Shadow_SelectLightInView(void)
{
	float bestrating, rating, temp[3];
	dlight_t *best, *light;
	best = NULL;
	bestrating = 0;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		VectorSubtract(light->origin, r_vieworigin, temp);
		rating = (DotProduct(temp, r_viewforward) / sqrt(DotProduct(temp, temp)));
		if (rating >= 0.95)
		{
			rating /= (1 + 0.0625f * sqrt(DotProduct(temp, temp)));
			if (bestrating < rating && CL_TraceBox(light->origin, vec3_origin, vec3_origin, r_vieworigin, true, NULL, SUPERCONTENTS_SOLID, false).fraction == 1.0f)
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
	if (r_refdef.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (r_refdef.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	lightsstring = (char *)FS_LoadFile(name, tempmempool, false);
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
			a = sscanf(t, "%f %f %f %f %f %f %f %d %s %f %f %f %f %f %f %f %f %i", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname, &corona, &angles[0], &angles[1], &angles[2], &coronasizescale, &ambientscale, &diffusescale, &specularscale, &flags);
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
				cubemapname[strlen(cubemapname)-1] = 0;
				strcpy(cubemapname, cubemapname + 1);
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
	dlight_t *light;
	size_t bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[1024];
	if (!r_shadow_worldlightchain)
		return;
	if (r_refdef.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (r_refdef.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		if (light->coronasizescale != 0.25f || light->ambientscale != 0 || light->diffusescale != 1 || light->specularscale != 1 || light->flags != LIGHTFLAG_REALTIMEMODE)
			sprintf(line, "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f %f %f %f %f %i\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style, light->cubemapname, light->corona, light->angles[0], light->angles[1], light->angles[2], light->coronasizescale, light->ambientscale, light->diffusescale, light->specularscale, light->flags);
		else if (light->cubemapname[0] || light->corona || light->angles[0] || light->angles[1] || light->angles[2])
			sprintf(line, "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style, light->cubemapname, light->corona, light->angles[0], light->angles[1], light->angles[2]);
		else
			sprintf(line, "%s%f %f %f %f %f %f %f %d\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius, light->color[0], light->color[1], light->color[2], light->style);
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
	if (r_refdef.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (r_refdef.worldmodel->name, name, sizeof (name));
	strlcat (name, ".lights", sizeof (name));
	lightsstring = (char *)FS_LoadFile(name, tempmempool, false);
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
	char key[256], value[1024];

	if (r_refdef.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	// try to load a .ent file first
	FS_StripExtension (r_refdef.worldmodel->name, key, sizeof (key));
	strlcat (key, ".ent", sizeof (key));
	data = entfiledata = (char *)FS_LoadFile(key, tempmempool, true);
	// and if that is not found, fall back to the bsp file entity string
	if (!data)
		data = r_refdef.worldmodel->brush.entities;
	if (!data)
		return;
	for (entnum = 0;COM_ParseToken(&data, false) && com_token[0] == '{';entnum++)
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
			if (!COM_ParseToken(&data, false))
				break; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '_')
				strcpy(key, com_token + 1);
			else
				strcpy(key, com_token);
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken(&data, false))
				break; // error
			strcpy(value, com_token);

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
			else if (r_refdef.worldmodel->type == mod_brushq3)
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
	VectorMA(r_vieworigin, r_editlights_cursordistance.value, r_viewforward, dest);
	trace = CL_TraceBox(r_vieworigin, vec3_origin, vec3_origin, dest, true, NULL, SUPERCONTENTS_SOLID, false);
	if (trace.fraction < 1)
	{
		dist = trace.fraction * r_editlights_cursordistance.value;
		push = r_editlights_cursorpushback.value;
		if (push > dist)
			push = dist;
		push = -push;
		VectorMA(trace.endpos, push, r_viewforward, endpos);
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
		R_Shadow_DrawLightSprites();
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
	if (!r_refdef.worldmodel)
		return;
	strlcpy(r_shadow_mapname, r_refdef.worldmodel->name, sizeof(r_shadow_mapname));
	R_Shadow_ClearWorldLights();
	R_Shadow_LoadWorldLights();
	if (r_shadow_worldlightchain == NULL)
	{
		R_Shadow_LoadLightsFile();
		if (r_shadow_worldlightchain == NULL)
			R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
	}
}

void R_Shadow_EditLights_Save_f(void)
{
	if (!r_refdef.worldmodel)
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
	char cubemapname[1024];
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
		strcpy(cubemapname, r_shadow_selectedlight->cubemapname);
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
			strcpy(cubemapname, Cmd_Argv(2));
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
	dlight_t *light;

	if (!r_editlights.integer)
	{
		Con_Print("Cannot edit lights when not in editing mode. Set r_editlights to 1.\n");
		return;
	}

	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		R_Shadow_SelectLight(light);
		R_Shadow_EditLights_Edit_f();
	}
}

void R_Shadow_EditLights_DrawSelectedLightProperties(void)
{
	int lightnumber, lightcount;
	dlight_t *light;
	float x, y;
	char temp[256];
	if (!r_editlights.integer)
		return;
	x = 0;
	y = con_vislines;
	lightnumber = -1;
	lightcount = 0;
	for (lightcount = 0, light = r_shadow_worldlightchain;light;lightcount++, light = light->next)
		if (light == r_shadow_selectedlight)
			lightnumber = lightcount;
	sprintf(temp, "Cursor  %f %f %f  Total Lights %i", r_editlights_cursorlocation[0], r_editlights_cursorlocation[1], r_editlights_cursorlocation[2], lightcount);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	if (r_shadow_selectedlight == NULL)
		return;
	sprintf(temp, "Light #%i properties", lightnumber);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Origin       : %f %f %f\n", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Angles       : %f %f %f\n", r_shadow_selectedlight->angles[0], r_shadow_selectedlight->angles[1], r_shadow_selectedlight->angles[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Color        : %f %f %f\n", r_shadow_selectedlight->color[0], r_shadow_selectedlight->color[1], r_shadow_selectedlight->color[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Radius       : %f\n", r_shadow_selectedlight->radius);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Corona       : %f\n", r_shadow_selectedlight->corona);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Style        : %i\n", r_shadow_selectedlight->style);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Shadows      : %s\n", r_shadow_selectedlight->shadow ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Cubemap      : %s\n", r_shadow_selectedlight->cubemapname);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "CoronaSize   : %f\n", r_shadow_selectedlight->coronasizescale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Ambient      : %f\n", r_shadow_selectedlight->ambientscale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Diffuse      : %f\n", r_shadow_selectedlight->diffusescale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Specular     : %f\n", r_shadow_selectedlight->specularscale);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "NormalMode   : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_NORMALMODE) ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "RealTimeMode : %s\n", (r_shadow_selectedlight->flags & LIGHTFLAG_REALTIMEMODE) ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
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
		strcpy(r_shadow_bufferlight.cubemapname, r_shadow_selectedlight->cubemapname);
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
	Cmd_AddCommand("r_editlights_help", R_Shadow_EditLights_Help_f);
	Cmd_AddCommand("r_editlights_clear", R_Shadow_EditLights_Clear_f);
	Cmd_AddCommand("r_editlights_reload", R_Shadow_EditLights_Reload_f);
	Cmd_AddCommand("r_editlights_save", R_Shadow_EditLights_Save_f);
	Cmd_AddCommand("r_editlights_spawn", R_Shadow_EditLights_Spawn_f);
	Cmd_AddCommand("r_editlights_edit", R_Shadow_EditLights_Edit_f);
	Cmd_AddCommand("r_editlights_editall", R_Shadow_EditLights_EditAll_f);
	Cmd_AddCommand("r_editlights_remove", R_Shadow_EditLights_Remove_f);
	Cmd_AddCommand("r_editlights_toggleshadow", R_Shadow_EditLights_ToggleShadow_f);
	Cmd_AddCommand("r_editlights_togglecorona", R_Shadow_EditLights_ToggleCorona_f);
	Cmd_AddCommand("r_editlights_importlightentitiesfrommap", R_Shadow_EditLights_ImportLightEntitiesFromMap_f);
	Cmd_AddCommand("r_editlights_importlightsfile", R_Shadow_EditLights_ImportLightsFile_f);
	Cmd_AddCommand("r_editlights_copyinfo", R_Shadow_EditLights_CopyInfo_f);
	Cmd_AddCommand("r_editlights_pasteinfo", R_Shadow_EditLights_PasteInfo_f);
}

