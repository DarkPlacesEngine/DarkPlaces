
/*
Terminology: Stencil Shadow Volume (sometimes called Stencil Shadows)
An extrusion of the lit faces, beginning at the original geometry and ending
further from the light source than the original geometry (presumably at least
as far as the light's radius, if the light has a radius at all), capped at
both front and back to avoid any problems (extrusion from dark faces also
works but has a different set of problems)

This is rendered using Carmack's Reverse technique, in which backfaces behind
zbuffer (zfail) increment the stencil, and frontfaces behind zbuffer (zfail)
decrement the stencil, the result is a stencil value of zero where shadows
did not intersect the visible geometry, suitable as a stencil mask for
rendering lighting everywhere but shadow.

In our case we use a biased stencil clear of 128 to avoid requiring the
stencil wrap extension (but probably should support it), and to address
Creative's patent on this sort of technology we also draw the frontfaces
first, and backfaces second (decrement, increment).

Patent warning:
This algorithm may be covered by Creative's patent (US Patent #6384822)
on Carmack's Reverse paper (which I have not read), however that patent
seems to be about drawing a stencil shadow from a model in an otherwise
unshadowed scene, where as realtime lighting technology draws light where
shadows do not lie.



Terminology: Stencil Light Volume (sometimes called Light Volumes)
Similar to a Stencil Shadow Volume, but inverted; rather than containing the
areas in shadow it contanis the areas in light, this can only be built
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
is unavoidable on hardware without GL_ARB_fragment_program.



Terminology: Normalization CubeMap
A cubemap containing normalized dot3-encoded (vectors of length 1 or less
encoded as RGB colors) for any possible direction, this technique allows per
pixel calculation of incidence vector for per pixel lighting purposes, which
would not otherwise be possible per pixel without GL_ARB_fragment_program.



Terminology: 2D Attenuation Texturing
A very crude approximation of light attenuation with distance which results
in cylindrical light shapes which fade vertically as a streak (some games
such as Doom3 allow this to be rotated to be less noticable in specific
cases), the technique is simply modulating lighting by two 2D textures (which
can be the same) on different axes of projection (XY and Z, typically), this
is the best technique available without 3D Attenuation Texturing or
GL_ARB_fragment_program technology.



Terminology: 3D Attenuation Texturing
A slightly crude approximation of light attenuation with distance, its flaws
are limited radius and resolution (performance tradeoffs).



Terminology: 3D Attenuation-Normalization Texturing
A 3D Attenuation Texture merged with a Normalization CubeMap, by making the
vectors shorter the lighting becomes darker, a very effective optimization of
diffuse lighting if 3D Attenuation Textures are already used.



Terminology: Light Cubemap Filtering
A technique for modeling non-uniform light distribution according to
direction, for example projecting a stained glass window image onto a wall,
this is done by texturing the lighting with a cubemap.



Terminology: Light Projection Filtering
A technique for modeling shadowing of light passing through translucent
surfaces, allowing stained glass windows and other effects to be done more
elegantly than possible with Light Cubemap Filtering by applying an occluder
texture to the lighting combined with a stencil light volume to limit the lit
area (this allows evaluating multiple translucent occluders in a scene).



Terminology: Doom3 Lighting
A combination of Stencil Shadow Volume, Per Pixel Lighting, Normalization
CubeMap, 2D Attenuation Texturing, and Light Filtering, as demonstrated by
the (currently upcoming) game Doom3.
*/

#include "quakedef.h"
#include "r_shadow.h"
#include "cl_collision.h"
#include "portals.h"
#include "image.h"

extern void R_Shadow_EditLights_Init(void);

#define SHADOWSTAGE_NONE 0
#define SHADOWSTAGE_STENCIL 1
#define SHADOWSTAGE_LIGHT 2
#define SHADOWSTAGE_STENCILTWOSIDE 3

int r_shadowstage = SHADOWSTAGE_NONE;

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

int r_shadow_buffer_numclusterpvsbytes;
qbyte *r_shadow_buffer_clusterpvs;
int *r_shadow_buffer_clusterlist;

int r_shadow_buffer_numsurfacepvsbytes;
qbyte *r_shadow_buffer_surfacepvs;
int *r_shadow_buffer_surfacelist;

rtexturepool_t *r_shadow_texturepool;
rtexture_t *r_shadow_normalcubetexture;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_attenuation3dtexture;
rtexture_t *r_shadow_blankbumptexture;
rtexture_t *r_shadow_blankglosstexture;
rtexture_t *r_shadow_blankwhitetexture;

// lights are reloaded when this changes
char r_shadow_mapname[MAX_QPATH];

// used only for light filters (cubemaps)
rtexturepool_t *r_shadow_filters_texturepool;

cvar_t r_shadow_bumpscale_basetexture = {0, "r_shadow_bumpscale_basetexture", "0"};
cvar_t r_shadow_bumpscale_bumpmap = {0, "r_shadow_bumpscale_bumpmap", "4"};
cvar_t r_shadow_cull = {0, "r_shadow_cull", "1"};
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
cvar_t r_shadow_realtime_dlight_shadows = {CVAR_SAVE, "r_shadow_realtime_dlight_shadows", "0"};
cvar_t r_shadow_realtime_world = {CVAR_SAVE, "r_shadow_realtime_world", "0"};
cvar_t r_shadow_realtime_world_dlightshadows = {CVAR_SAVE, "r_shadow_realtime_world_dlightshadows", "1"};
cvar_t r_shadow_realtime_world_lightmaps = {CVAR_SAVE, "r_shadow_realtime_world_lightmaps", "0"};
cvar_t r_shadow_realtime_world_shadows = {CVAR_SAVE, "r_shadow_realtime_world_shadows", "1"};
cvar_t r_shadow_scissor = {0, "r_shadow_scissor", "1"};
cvar_t r_shadow_shadow_polygonfactor = {0, "r_shadow_shadow_polygonfactor", "0"};
cvar_t r_shadow_shadow_polygonoffset = {0, "r_shadow_shadow_polygonoffset", "1"};
cvar_t r_shadow_singlepassvolumegeneration = {0, "r_shadow_singlepassvolumegeneration", "1"};
cvar_t r_shadow_staticworldlights = {0, "r_shadow_staticworldlights", "1"};
cvar_t r_shadow_texture3d = {0, "r_shadow_texture3d", "1"};
cvar_t r_shadow_visiblevolumes = {0, "r_shadow_visiblevolumes", "0"};
cvar_t gl_ext_stenciltwoside = {0, "gl_ext_stenciltwoside", "1"};
cvar_t r_editlights = {0, "r_editlights", "0"};
cvar_t r_editlights_cursordistance = {0, "r_editlights_distance", "1024"};
cvar_t r_editlights_cursorpushback = {0, "r_editlights_pushback", "0"};
cvar_t r_editlights_cursorpushoff = {0, "r_editlights_pushoff", "4"};
cvar_t r_editlights_cursorgrid = {0, "r_editlights_grid", "4"};
cvar_t r_editlights_quakelightsizescale = {CVAR_SAVE, "r_editlights_quakelightsizescale", "0.8"};
cvar_t r_editlights_rtlightssizescale = {CVAR_SAVE, "r_editlights_rtlightssizescale", "0.7"};
cvar_t r_editlights_rtlightscolorscale = {CVAR_SAVE, "r_editlights_rtlightscolorscale", "2"};

int c_rt_lights, c_rt_clears, c_rt_scissored;
int c_rt_shadowmeshes, c_rt_shadowtris, c_rt_lightmeshes, c_rt_lighttris;
int c_rtcached_shadowmeshes, c_rtcached_shadowtris;

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

void r_shadow_start(void)
{
	// allocate vertex processing arrays
	numcubemaps = 0;
	r_shadow_normalcubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	r_shadow_blankglosstexture = NULL;
	r_shadow_blankwhitetexture = NULL;
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
	r_shadow_buffer_numclusterpvsbytes = 0;
	r_shadow_buffer_clusterpvs = NULL;
	r_shadow_buffer_clusterlist = NULL;
	r_shadow_buffer_numsurfacepvsbytes = 0;
	r_shadow_buffer_surfacepvs = NULL;
	r_shadow_buffer_surfacelist = NULL;
}

void r_shadow_shutdown(void)
{
	R_Shadow_UncompileWorldLights();
	numcubemaps = 0;
	r_shadow_normalcubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	r_shadow_blankglosstexture = NULL;
	r_shadow_blankwhitetexture = NULL;
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
	r_shadow_buffer_numclusterpvsbytes = 0;
	if (r_shadow_buffer_clusterpvs)
		Mem_Free(r_shadow_buffer_clusterpvs);
	r_shadow_buffer_clusterpvs = NULL;
	if (r_shadow_buffer_clusterlist)
		Mem_Free(r_shadow_buffer_clusterlist);
	r_shadow_buffer_clusterlist = NULL;
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
"r_shadow_realtime_world : use high quality world lighting mode\n"
"r_shadow_realtime_world_dlightshadows : cast shadows from dlights\n"
"r_shadow_realtime_world_lightmaps : use lightmaps in addition to lights\n"
"r_shadow_realtime_world_shadows : cast shadows from world lights\n"
"r_shadow_scissor : use scissor optimization\n"
"r_shadow_shadow_polygonfactor : nudge shadow volumes closer/further\n"
"r_shadow_shadow_polygonoffset : nudge shadow volumes closer/further\n"
"r_shadow_singlepassvolumegeneration : selects shadow volume algorithm\n"
"r_shadow_texture3d : use 3d attenuation texture (if hardware supports)\n"
"r_shadow_visiblevolumes : useful for performance testing; bright = slow!\n"
"Commands:\n"
"r_shadow_help : this help\n"
	);
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_bumpscale_basetexture);
	Cvar_RegisterVariable(&r_shadow_bumpscale_bumpmap);
	Cvar_RegisterVariable(&r_shadow_cull);
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
	Cvar_RegisterVariable(&r_shadow_realtime_world);
	Cvar_RegisterVariable(&r_shadow_realtime_world_dlightshadows);
	Cvar_RegisterVariable(&r_shadow_realtime_world_lightmaps);
	Cvar_RegisterVariable(&r_shadow_realtime_world_shadows);
	Cvar_RegisterVariable(&r_shadow_scissor);
	Cvar_RegisterVariable(&r_shadow_shadow_polygonfactor);
	Cvar_RegisterVariable(&r_shadow_shadow_polygonoffset);
	Cvar_RegisterVariable(&r_shadow_singlepassvolumegeneration);
	Cvar_RegisterVariable(&r_shadow_staticworldlights);
	Cvar_RegisterVariable(&r_shadow_texture3d);
	Cvar_RegisterVariable(&r_shadow_visiblevolumes);
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
	r_shadow_buffer_numclusterpvsbytes = 0;
	r_shadow_buffer_clusterpvs = NULL;
	r_shadow_buffer_clusterlist = NULL;
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
		shadowelements = Mem_Alloc(r_shadow_mempool, maxshadowelements * sizeof(int));
	}
	return shadowelements;
}

void R_Shadow_EnlargeClusterBuffer(int numclusters)
{
	int numclusterpvsbytes = (((numclusters + 7) >> 3) + 255) & ~255;
	if (r_shadow_buffer_numclusterpvsbytes < numclusterpvsbytes)
	{
		if (r_shadow_buffer_clusterpvs)
			Mem_Free(r_shadow_buffer_clusterpvs);
		if (r_shadow_buffer_clusterlist)
			Mem_Free(r_shadow_buffer_clusterlist);
		r_shadow_buffer_numclusterpvsbytes = numclusterpvsbytes;
		r_shadow_buffer_clusterpvs = Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numclusterpvsbytes);
		r_shadow_buffer_clusterlist = Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numclusterpvsbytes * 8 * sizeof(*r_shadow_buffer_clusterlist));
	}
}

void R_Shadow_EnlargeSurfaceBuffer(int numsurfaces)
{
	int numsurfacepvsbytes = (((numsurfaces + 7) >> 3) + 255) & ~255;
	if (r_shadow_buffer_numsurfacepvsbytes < numsurfacepvsbytes)
	{
		if (r_shadow_buffer_surfacepvs)
			Mem_Free(r_shadow_buffer_surfacepvs);
		if (r_shadow_buffer_surfacelist)
			Mem_Free(r_shadow_buffer_surfacelist);
		r_shadow_buffer_numsurfacepvsbytes = numsurfacepvsbytes;
		r_shadow_buffer_surfacepvs = Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numsurfacepvsbytes);
		r_shadow_buffer_surfacelist = Mem_Alloc(r_shadow_mempool, r_shadow_buffer_numsurfacepvsbytes * 8 * sizeof(*r_shadow_buffer_surfacelist));
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
		shadowmark = Mem_Alloc(r_shadow_mempool, maxshadowmark * sizeof(*shadowmark));
		shadowmarklist = Mem_Alloc(r_shadow_mempool, maxshadowmark * sizeof(*shadowmarklist));
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
	int i, j, tris = 0, vr[3], t, outvertices = 0;
	float f, temp[3];
	const int *e, *n;
	const float *v;

	if (maxvertexupdate < innumvertices)
	{
		maxvertexupdate = innumvertices;
		if (vertexupdate)
			Mem_Free(vertexupdate);
		if (vertexremap)
			Mem_Free(vertexremap);
		vertexupdate = Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
		vertexremap = Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
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
		t = shadowmarktris[i];
		e = inelement3i + t * 3;
		// make sure the vertices are created
		for (j = 0;j < 3;j++)
		{
			if (vertexupdate[e[j]] != vertexupdatenum)
			{
				vertexupdate[e[j]] = vertexupdatenum;
				vertexremap[e[j]] = outvertices;
				v = invertex3f + e[j] * 3;
				// project one copy of the vertex to the sphere radius of the light
				// (FIXME: would projecting it to the light box be better?)
				VectorSubtract(v, projectorigin, temp);
				f = projectdistance / VectorLength(temp);
				VectorCopy(v, outvertex3f);
				VectorMA(projectorigin, f, temp, (outvertex3f + 3));
				outvertex3f += 6;
				outvertices += 2;
			}
		}
	}

	for (i = 0;i < numshadowmarktris;i++)
	{
		t = shadowmarktris[i];
		e = inelement3i + t * 3;
		n = inneighbor3i + t * 3;
		// output the front and back triangles
		outelement3i[0] = vertexremap[e[0]];
		outelement3i[1] = vertexremap[e[1]];
		outelement3i[2] = vertexremap[e[2]];
		outelement3i[3] = vertexremap[e[2]] + 1;
		outelement3i[4] = vertexremap[e[1]] + 1;
		outelement3i[5] = vertexremap[e[0]] + 1;
		outelement3i += 6;
		tris += 2;
		// output the sides (facing outward from this triangle)
		if (shadowmark[n[0]] != shadowmarkcount)
		{
			vr[0] = vertexremap[e[0]];
			vr[1] = vertexremap[e[1]];
			outelement3i[0] = vr[1];
			outelement3i[1] = vr[0];
			outelement3i[2] = vr[0] + 1;
			outelement3i[3] = vr[1];
			outelement3i[4] = vr[0] + 1;
			outelement3i[5] = vr[1] + 1;
			outelement3i += 6;
			tris += 2;
		}
		if (shadowmark[n[1]] != shadowmarkcount)
		{
			vr[1] = vertexremap[e[1]];
			vr[2] = vertexremap[e[2]];
			outelement3i[0] = vr[2];
			outelement3i[1] = vr[1];
			outelement3i[2] = vr[1] + 1;
			outelement3i[3] = vr[2];
			outelement3i[4] = vr[1] + 1;
			outelement3i[5] = vr[2] + 1;
			outelement3i += 6;
			tris += 2;
		}
		if (shadowmark[n[2]] != shadowmarkcount)
		{
			vr[0] = vertexremap[e[0]];
			vr[2] = vertexremap[e[2]];
			outelement3i[0] = vr[0];
			outelement3i[1] = vr[2];
			outelement3i[2] = vr[2] + 1;
			outelement3i[3] = vr[0];
			outelement3i[4] = vr[2] + 1;
			outelement3i[5] = vr[0] + 1;
			outelement3i += 6;
			tris += 2;
		}
	}
	if (outnumvertices)
		*outnumvertices = outvertices;
	return tris;
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
	R_Shadow_RenderVolume(outverts, tris, varray_vertex3f2, shadowelements);
}

void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, vec3_t lightmins, vec3_t lightmaxs, vec3_t surfacemins, vec3_t surfacemaxs)
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
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = vertex3f;
	R_Mesh_State(&m);
	GL_LockArrays(0, numvertices);
	if (r_shadowstage == SHADOWSTAGE_STENCIL)
	{
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		R_Mesh_Draw(numvertices, numtriangles, element3i);
		c_rt_shadowmeshes++;
		c_rt_shadowtris += numtriangles;
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	}
	R_Mesh_Draw(numvertices, numtriangles, element3i);
	c_rt_shadowmeshes++;
	c_rt_shadowtris += numtriangles;
	GL_LockArrays(0, 0);
}

static void R_Shadow_MakeTextures(void)
{
	int x, y, z, d, side;
	float v[3], s, t, intensity;
	qbyte *data;
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_attenpower = r_shadow_lightattenuationpower.value;
	r_shadow_attenscale = r_shadow_lightattenuationscale.value;
#define NORMSIZE 64
#define ATTEN2DSIZE 64
#define ATTEN3DSIZE 32
	data = Mem_Alloc(tempmempool, max(6*NORMSIZE*NORMSIZE*4, max(ATTEN3DSIZE*ATTEN3DSIZE*ATTEN3DSIZE*4, ATTEN2DSIZE*ATTEN2DSIZE*4)));
	data[0] = 128;
	data[1] = 128;
	data[2] = 255;
	data[3] = 255;
	r_shadow_blankbumptexture = R_LoadTexture2D(r_shadow_texturepool, "blankbump", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_shadow_blankglosstexture = R_LoadTexture2D(r_shadow_texturepool, "blankgloss", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_shadow_blankwhitetexture = R_LoadTexture2D(r_shadow_texturepool, "blankwhite", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	if (gl_texturecubemap)
	{
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
					data[((side*NORMSIZE+y)*NORMSIZE+x)*4+0] = 128.0f + intensity * v[0];
					data[((side*NORMSIZE+y)*NORMSIZE+x)*4+1] = 128.0f + intensity * v[1];
					data[((side*NORMSIZE+y)*NORMSIZE+x)*4+2] = 128.0f + intensity * v[2];
					data[((side*NORMSIZE+y)*NORMSIZE+x)*4+3] = 255;
				}
			}
		}
		r_shadow_normalcubetexture = R_LoadTextureCubeMap(r_shadow_texturepool, "normalcube", NORMSIZE, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
	}
	else
		r_shadow_normalcubetexture = NULL;
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
	r_shadowstage = SHADOWSTAGE_NONE;

	c_rt_lights = c_rt_clears = c_rt_scissored = 0;
	c_rt_shadowmeshes = c_rt_shadowtris = c_rt_lightmeshes = c_rt_lighttris = 0;
	c_rtcached_shadowmeshes = c_rtcached_shadowtris = 0;
}

void R_Shadow_Stage_ShadowVolumes(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State(&m);
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
		r_shadowstage = SHADOWSTAGE_STENCILTWOSIDE;
		qglDisable(GL_CULL_FACE);
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
		qglActiveStencilFaceEXT(GL_BACK); // quake is backwards, this is front faces
		qglStencilMask(~0);
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		qglActiveStencilFaceEXT(GL_FRONT); // quake is backwards, this is back faces
		qglStencilMask(~0);
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	}
	else
	{
		r_shadowstage = SHADOWSTAGE_STENCIL;
		qglEnable(GL_CULL_FACE);
		qglStencilMask(~0);
		// this is changed by every shadow render so its value here is unimportant
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	}
	GL_Clear(GL_STENCIL_BUFFER_BIT);
	c_rt_clears++;
	// LordHavoc note: many shadow volumes reside entirely inside the world
	// (that is to say they are entirely bounded by their lit surfaces),
	// which can be optimized by handling things as an inverted light volume,
	// with the shadow boundaries of the world being simulated by an altered
	// (129) bias to stencil clearing on such lights
	// FIXME: generate inverted light volumes for use as shadow volumes and
	// optimize for them as noted above
}

void R_Shadow_Stage_Light(int shadowtest)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State(&m);
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
	if (shadowtest)
		qglEnable(GL_STENCIL_TEST);
	else
		qglDisable(GL_STENCIL_TEST);
	if (gl_support_stenciltwoside)
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	qglStencilMask(~0);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw light where this geometry was already rendered AND the
	// stencil is 128 (values other than this mean shadow)
	qglStencilFunc(GL_EQUAL, 128, ~0);
	r_shadowstage = SHADOWSTAGE_LIGHT;
	c_rt_lights++;
}

void R_Shadow_Stage_End(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State(&m);
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
	r_shadowstage = SHADOWSTAGE_NONE;
}

int R_Shadow_ScissorForBBox(const float *mins, const float *maxs)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2, x, y, f;
	vec3_t smins, smaxs;
	vec4_t v, v2;
	if (!r_shadow_scissor.integer)
		return false;
	// if view is inside the box, just say yes it's visible
	if (BoxesOverlap(r_vieworigin, r_vieworigin, mins, maxs))
	{
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
		return false;
	}
	for (i = 0;i < 3;i++)
	{
		if (r_viewforward[i] >= 0)
		{
			v[i] = mins[i];
			v2[i] = maxs[i];
		}
		else
		{
			v[i] = maxs[i];
			v2[i] = mins[i];
		}
	}
	f = DotProduct(r_viewforward, r_vieworigin) + 1;
	if (DotProduct(r_viewforward, v2) <= f)
	{
		// entirely behind nearclip plane
		return true;
	}
	if (DotProduct(r_viewforward, v) >= f)
	{
		// entirely infront of nearclip plane
		x1 = y1 = x2 = y2 = 0;
		for (i = 0;i < 8;i++)
		{
			v[0] = (i & 1) ? mins[0] : maxs[0];
			v[1] = (i & 2) ? mins[1] : maxs[1];
			v[2] = (i & 4) ? mins[2] : maxs[2];
			v[3] = 1.0f;
			GL_TransformToScreen(v, v2);
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			x = v2[0];
			y = v2[1];
			if (i)
			{
				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
			else
			{
				x1 = x2 = x;
				y1 = y2 = y;
			}
		}
	}
	else
	{
		// clipped by nearclip plane
		// this is nasty and crude...
		// create viewspace bbox
		for (i = 0;i < 8;i++)
		{
			v[0] = ((i & 1) ? mins[0] : maxs[0]) - r_vieworigin[0];
			v[1] = ((i & 2) ? mins[1] : maxs[1]) - r_vieworigin[1];
			v[2] = ((i & 4) ? mins[2] : maxs[2]) - r_vieworigin[2];
			v2[0] = -DotProduct(v, r_viewleft);
			v2[1] = DotProduct(v, r_viewup);
			v2[2] = DotProduct(v, r_viewforward);
			if (i)
			{
				if (smins[0] > v2[0]) smins[0] = v2[0];
				if (smaxs[0] < v2[0]) smaxs[0] = v2[0];
				if (smins[1] > v2[1]) smins[1] = v2[1];
				if (smaxs[1] < v2[1]) smaxs[1] = v2[1];
				if (smins[2] > v2[2]) smins[2] = v2[2];
				if (smaxs[2] < v2[2]) smaxs[2] = v2[2];
			}
			else
			{
				smins[0] = smaxs[0] = v2[0];
				smins[1] = smaxs[1] = v2[1];
				smins[2] = smaxs[2] = v2[2];
			}
		}
		// now we have a bbox in viewspace
		// clip it to the view plane
		if (smins[2] < 1)
			smins[2] = 1;
		// return true if that culled the box
		if (smins[2] >= smaxs[2])
			return true;
		// ok some of it is infront of the view, transform each corner back to
		// worldspace and then to screenspace and make screen rect
		// initialize these variables just to avoid compiler warnings
		x1 = y1 = x2 = y2 = 0;
		for (i = 0;i < 8;i++)
		{
			v2[0] = (i & 1) ? smins[0] : smaxs[0];
			v2[1] = (i & 2) ? smins[1] : smaxs[1];
			v2[2] = (i & 4) ? smins[2] : smaxs[2];
			v[0] = v2[0] * -r_viewleft[0] + v2[1] * r_viewup[0] + v2[2] * r_viewforward[0] + r_vieworigin[0];
			v[1] = v2[0] * -r_viewleft[1] + v2[1] * r_viewup[1] + v2[2] * r_viewforward[1] + r_vieworigin[1];
			v[2] = v2[0] * -r_viewleft[2] + v2[1] * r_viewup[2] + v2[2] * r_viewforward[2] + r_vieworigin[2];
			v[3] = 1.0f;
			GL_TransformToScreen(v, v2);
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			x = v2[0];
			y = v2[1];
			if (i)
			{
				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
			else
			{
				x1 = x2 = x;
				y1 = y2 = y;
			}
		}
		/*
		// this code doesn't handle boxes with any points behind view properly
		x1 = 1000;x2 = -1000;
		y1 = 1000;y2 = -1000;
		for (i = 0;i < 8;i++)
		{
			v[0] = (i & 1) ? mins[0] : maxs[0];
			v[1] = (i & 2) ? mins[1] : maxs[1];
			v[2] = (i & 4) ? mins[2] : maxs[2];
			v[3] = 1.0f;
			GL_TransformToScreen(v, v2);
			//Con_Printf("%.3f %.3f %.3f %.3f transformed to %.3f %.3f %.3f %.3f\n", v[0], v[1], v[2], v[3], v2[0], v2[1], v2[2], v2[3]);
			if (v2[2] > 0)
			{
				x = v2[0];
				y = v2[1];

				if (x1 > x) x1 = x;
				if (x2 < x) x2 = x;
				if (y1 > y) y1 = y;
				if (y2 < y) y2 = y;
			}
		}
		*/
	}
	ix1 = x1 - 1.0f;
	iy1 = y1 - 1.0f;
	ix2 = x2 + 1.0f;
	iy2 = y2 + 1.0f;
	//Con_Printf("%f %f %f %f\n", x1, y1, x2, y2);
	if (ix1 < r_view_x) ix1 = r_view_x;
	if (iy1 < r_view_y) iy1 = r_view_y;
	if (ix2 > r_view_x + r_view_width) ix2 = r_view_x + r_view_width;
	if (iy2 > r_view_y + r_view_height) iy2 = r_view_y + r_view_height;
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;
	// set up the scissor rectangle
	GL_Scissor(ix1, vid.realheight - iy2, ix2 - ix1, iy2 - iy1);
	//qglScissor(ix1, iy1, ix2 - ix1, iy2 - iy1);
	//qglEnable(GL_SCISSOR_TEST);
	c_rt_scissored++;
	return false;
}

static void R_Shadow_VertexShadingWithXYZAttenuation(int numverts, const float *vertex3f, const float *normal3f, const float *lightcolor, const matrix4x4_t *m)
{
	float *color4f = varray_color4f;
	float dist, dot, intensity, v[3], n[3];
	for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
	{
		Matrix4x4_Transform(m, vertex3f, v);
		if ((dist = DotProduct(v, v)) < 1)
		{
			Matrix4x4_Transform3x3(m, normal3f, n);
			if ((dot = DotProduct(n, v)) > 0)
			{
				dist = sqrt(dist);
				intensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
				intensity *= pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale;
				VectorScale(lightcolor, intensity, color4f);
				color4f[3] = 1;
			}
			else
			{
				VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		else
		{
			VectorClear(color4f);
			color4f[3] = 1;
		}
	}
}

static void R_Shadow_VertexShadingWithZAttenuation(int numverts, const float *vertex3f, const float *normal3f, const float *lightcolor, const matrix4x4_t *m)
{
	float *color4f = varray_color4f;
	float dist, dot, intensity, v[3], n[3];
	for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
	{
		Matrix4x4_Transform(m, vertex3f, v);
		if ((dist = fabs(v[2])) < 1)
		{
			Matrix4x4_Transform3x3(m, normal3f, n);
			if ((dot = DotProduct(n, v)) > 0)
			{
				intensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
				intensity *= pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale;
				VectorScale(lightcolor, intensity, color4f);
				color4f[3] = 1;
			}
			else
			{
				VectorClear(color4f);
				color4f[3] = 1;
			}
		}
		else
		{
			VectorClear(color4f);
			color4f[3] = 1;
		}
	}
}

static void R_Shadow_VertexShading(int numverts, const float *vertex3f, const float *normal3f, const float *lightcolor, const matrix4x4_t *m)
{
	float *color4f = varray_color4f;
	float dot, intensity, v[3], n[3];
	for (;numverts > 0;numverts--, vertex3f += 3, normal3f += 3, color4f += 4)
	{
		Matrix4x4_Transform(m, vertex3f, v);
		Matrix4x4_Transform3x3(m, normal3f, n);
		if ((dot = DotProduct(n, v)) > 0)
		{
			intensity = dot / sqrt(VectorLength2(v) * VectorLength2(n));
			VectorScale(lightcolor, intensity, color4f);
			color4f[3] = 1;
		}
		else
		{
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
		VectorNormalizeFast(lightdir);
		VectorSubtract(vertex3f, relativeeyeorigin, eyedir);
		VectorNormalizeFast(eyedir);
		VectorAdd(lightdir, eyedir, halfdir);
		// the cubemap normalizes this for us
		out3f[0] = DotProduct(svector3f, halfdir);
		out3f[1] = DotProduct(tvector3f, halfdir);
		out3f[2] = DotProduct(normal3f, halfdir);
	}
}

void R_Shadow_RenderLighting(int numverts, int numtriangles, const int *elements, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, const float *relativelightorigin, const float *relativeeyeorigin, const float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *glosstexture, rtexture_t *lightcubemap, int lighting)
{
	int renders;
	float color[3], color2[3], colorscale;
	rmeshstate_t m;
	if (!bumptexture)
		bumptexture = r_shadow_blankbumptexture;
	if (!glosstexture)
		glosstexture = r_shadow_blankglosstexture;
	// FIXME: support EF_NODEPTHTEST
	GL_DepthMask(false);
	GL_DepthTest(true);
	if (gl_dot3arb && gl_texturecubemap && gl_combine.integer && gl_stencil)
	{
		if (lighting & LIGHTING_DIFFUSE)
		{
			GL_Color(1,1,1,1);
			colorscale = r_shadow_lightintensityscale.value;
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
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(bumptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = texcoord2f;
				m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
				m.tex3d[2] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[2] = vertex3f;
				m.texmatrix[2] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord3f[2] = varray_texcoord3f[2];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[2], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = texcoord2f;
				if (lightcubemap)
				{
					m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
				}
			}
			else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && lightcubemap)
			{
				// 1/2/2 3D combine path (original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = vertex3f;
				m.texmatrix[0] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord3f[0] = varray_texcoord3f[0];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(bumptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = texcoord2f;
				m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
				R_Mesh_State(&m);
				GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = texcoord2f;
				if (lightcubemap)
				{
					m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
				}
			}
			else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && !lightcubemap)
			{
				// 2/2 3D combine path (original Radeon)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(bumptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = texcoord2f;
				m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = texcoord2f;
				m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = vertex3f;
				m.texmatrix[1] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
			}
			else if (r_textureunits.integer >= 4)
			{
				// 4/2 2D combine path (Geforce3, Radeon 8500)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(bumptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = texcoord2f;
				m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
				m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[2] = vertex3f;
				m.texmatrix[2] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord[2] = varray_texcoord2f[2];
				R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[2], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				m.tex[3] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[3] = vertex3f;
				m.texmatrix[3] = *matrix_modeltoattenuationz;
#else
				m.pointer_texcoord[3] = varray_texcoord2f[3];
				R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[3], numverts, vertex3f, matrix_modeltoattenuationz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = texcoord2f;
				if (lightcubemap)
				{
					m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
				}
			}
			else
			{
				// 2/2/2 2D combine path (any dot3 card)
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[0] = vertex3f;
				m.texmatrix[0] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord[0] = varray_texcoord2f[0];
				R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = vertex3f;
				m.texmatrix[1] = *matrix_modeltoattenuationz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationz);
#endif
				R_Mesh_State(&m);
				GL_ColorMask(0,0,0,1);
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(bumptexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.pointer_texcoord[0] = texcoord2f;
				m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.pointer_texcoord3f[1] = varray_texcoord3f[1];
				R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
				R_Mesh_State(&m);
				GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
	
				memset(&m, 0, sizeof(m));
				m.pointer_vertex = vertex3f;
				m.tex[0] = R_GetTexture(basetexture);
				m.pointer_texcoord[0] = texcoord2f;
				if (lightcubemap)
				{
					m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltolight;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
				}
			}
			// this final code is shared
			R_Mesh_State(&m);
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			VectorScale(lightcolor, colorscale, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				GL_Color(bound(0, color2[0], 1), bound(0, color2[1], 1), bound(0, color2[2], 1), 1);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		if ((lighting & LIGHTING_SPECULAR) && (r_shadow_gloss.integer >= 2 || (r_shadow_gloss.integer >= 1 && glosstexture != r_shadow_blankglosstexture)))
		{
			// FIXME: detect blendsquare!
			//if (gl_support_blendsquare)
			{
				colorscale = r_shadow_lightintensityscale.value * r_shadow_glossintensity.value;
				if (glosstexture == r_shadow_blankglosstexture)
					colorscale *= r_shadow_gloss2intensity.value;
				GL_Color(1,1,1,1);
				if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && lightcubemap /*&& gl_support_blendsquare*/) // FIXME: detect blendsquare!
				{
					// 2/0/0/1/2 3D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(bumptexture);
					m.pointer_texcoord[0] = texcoord2f;
					m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(0, numverts);
					R_Mesh_Draw(numverts, numtriangles, elements);
					GL_LockArrays(0, 0);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(0, numverts);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					GL_LockArrays(0, 0);
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[0] = vertex3f;
					m.texmatrix[0] = *matrix_modeltoattenuationxyz;
#else
					m.pointer_texcoord3f[0] = varray_texcoord3f[0];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
					R_Mesh_State(&m);
					GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
					GL_LockArrays(0, numverts);
					R_Mesh_Draw(numverts, numtriangles, elements);
					GL_LockArrays(0, 0);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = texcoord2f;
					if (lightcubemap)
					{
						m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
						m.pointer_texcoord3f[1] = vertex3f;
						m.texmatrix[1] = *matrix_modeltolight;
#else
						m.pointer_texcoord3f[1] = varray_texcoord3f[1];
						R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
					}
				}
				else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && !lightcubemap /*&& gl_support_blendsquare*/) // FIXME: detect blendsquare!
				{
					// 2/0/0/2 3D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(bumptexture);
					m.pointer_texcoord[0] = texcoord2f;
					m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(0, numverts);
					R_Mesh_Draw(numverts, numtriangles, elements);
					GL_LockArrays(0, 0);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(0, numverts);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					GL_LockArrays(0, 0);
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = texcoord2f;
					m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltoattenuationxyz;
#else
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				}
				else
				{
					// 2/0/0/2/2 2D combine blendsquare path
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(bumptexture);
					m.pointer_texcoord[0] = texcoord2f;
					m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
					m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
					m.pointer_texcoord3f[1] = varray_texcoord3f[1];
					R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
					R_Mesh_State(&m);
					GL_ColorMask(0,0,0,1);
					// this squares the result
					GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
					GL_LockArrays(0, numverts);
					R_Mesh_Draw(numverts, numtriangles, elements);
					GL_LockArrays(0, 0);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					R_Mesh_State(&m);
					GL_LockArrays(0, numverts);
					// square alpha in framebuffer a few times to make it shiny
					GL_BlendFunc(GL_ZERO, GL_DST_ALPHA);
					// these comments are a test run through this math for intensity 0.5
					// 0.5 * 0.5 = 0.25 (done by the BlendFunc earlier)
					// 0.25 * 0.25 = 0.0625 (this is another pass)
					// 0.0625 * 0.0625 = 0.00390625 (this is another pass)
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					R_Mesh_Draw(numverts, numtriangles, elements);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
					GL_LockArrays(0, 0);
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[0] = vertex3f;
					m.texmatrix[0] = *matrix_modeltoattenuationxyz;
#else
					m.pointer_texcoord[0] = varray_texcoord2f[0];
					R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
					m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[1] = vertex3f;
					m.texmatrix[1] = *matrix_modeltoattenuationz;
#else
					m.pointer_texcoord[1] = varray_texcoord2f[1];
					R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationz);
#endif
					R_Mesh_State(&m);
					GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
					GL_LockArrays(0, numverts);
					R_Mesh_Draw(numverts, numtriangles, elements);
					GL_LockArrays(0, 0);
					c_rt_lightmeshes++;
					c_rt_lighttris += numtriangles;
		
					memset(&m, 0, sizeof(m));
					m.pointer_vertex = vertex3f;
					m.tex[0] = R_GetTexture(glosstexture);
					m.pointer_texcoord[0] = texcoord2f;
					if (lightcubemap)
					{
						m.texcubemap[1] = R_GetTexture(lightcubemap);
#ifdef USETEXMATRIX
						m.pointer_texcoord3f[1] = vertex3f;
						m.texmatrix[1] = *matrix_modeltolight;
#else
						m.pointer_texcoord3f[1] = varray_texcoord3f[1];
						R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltolight);
#endif
					}
				}
			}
			R_Mesh_State(&m);
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			VectorScale(lightcolor, colorscale, color2);
			GL_LockArrays(0, numverts);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				GL_Color(bound(0, color2[0], 1), bound(0, color2[1], 1), bound(0, color2[2], 1), 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
			GL_LockArrays(0, 0);
		}
	}
	else
	{
		if (lighting & LIGHTING_DIFFUSE)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			memset(&m, 0, sizeof(m));
			m.pointer_vertex = vertex3f;
			m.pointer_color = varray_color4f;
			m.tex[0] = R_GetTexture(basetexture);
			m.pointer_texcoord[0] = texcoord2f;
			if (r_textureunits.integer >= 2)
			{
				// voodoo2
				m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
				m.pointer_texcoord3f[1] = vertex3f;
				m.texmatrix[1] = *matrix_modeltoattenuationxyz;
#else
				m.pointer_texcoord[1] = varray_texcoord2f[1];
				R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
#endif
				if (r_textureunits.integer >= 3)
				{
					// Geforce3/Radeon class but not using dot3
					m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
#ifdef USETEXMATRIX
					m.pointer_texcoord3f[2] = vertex3f;
					m.texmatrix[2] = *matrix_modeltoattenuationz;
#else
					m.pointer_texcoord[2] = varray_texcoord2f[2];
					R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[2], numverts, vertex3f, matrix_modeltoattenuationz);
#endif
				}
			}
			R_Mesh_State(&m);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				if (r_textureunits.integer >= 3)
					R_Shadow_VertexShading(numverts, vertex3f, normal3f, color, matrix_modeltolight);
				else if (r_textureunits.integer >= 2)
					R_Shadow_VertexShadingWithZAttenuation(numverts, vertex3f, normal3f, color, matrix_modeltolight);
				else
					R_Shadow_VertexShadingWithXYZAttenuation(numverts, vertex3f, normal3f, color, matrix_modeltolight);
				GL_LockArrays(0, numverts);
				R_Mesh_Draw(numverts, numtriangles, elements);
				GL_LockArrays(0, 0);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
	}
}

void R_RTLight_UpdateFromDLight(rtlight_t *rtlight, const dlight_t *light, int isstatic)
{
	int j, k;
	float scale;
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
	Matrix4x4_Invert_Simple(&rtlight->matrix_worldtolight, &light->matrix);
	// ConcatScale won't work here because this needs to scale rotate and
	// translate, not just rotate
	scale = 1.0f / rtlight->radius;
	for (k = 0;k < 3;k++)
		for (j = 0;j < 4;j++)
			rtlight->matrix_worldtolight.m[k][j] *= scale;
	Matrix4x4_Concat(&rtlight->matrix_worldtoattenuationxyz, &matrix_attenuationxyz, &rtlight->matrix_worldtolight);
	Matrix4x4_Concat(&rtlight->matrix_worldtoattenuationz, &matrix_attenuationz, &rtlight->matrix_worldtolight);

	rtlight->lightmap_cullradius = bound(0, rtlight->radius, 2048.0f);
	rtlight->lightmap_cullradius2 = rtlight->lightmap_cullradius * rtlight->lightmap_cullradius;
	VectorScale(rtlight->color, rtlight->radius * d_lightstylevalue[rtlight->style] * 0.125f, rtlight->lightmap_light);
	rtlight->lightmap_subtract = 1.0f / rtlight->lightmap_cullradius2;
}

// compiles rtlight geometry
// (undone by R_FreeCompiledRTLight, which R_UpdateLight calls)
void R_RTLight_Compile(rtlight_t *rtlight)
{
	int shadowmeshes, shadowtris, lightmeshes, lighttris, numclusters, numsurfaces;
	entity_render_t *ent = &cl_entities[0].render;
	model_t *model = ent->model;

	// compile the light
	rtlight->compiled = true;
	rtlight->static_numclusters = 0;
	rtlight->static_numclusterpvsbytes = 0;
	rtlight->static_clusterlist = NULL;
	rtlight->static_clusterpvs = NULL;
	rtlight->cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	rtlight->cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	rtlight->cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	rtlight->cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	rtlight->cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	rtlight->cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;

	if (model && model->GetLightInfo)
	{
		// this variable directs the DrawShadowVolume and DrawLight code to capture into the mesh chain instead of rendering
		r_shadow_compilingrtlight = rtlight;
		R_Shadow_EnlargeClusterBuffer(model->brush.num_pvsclusters);
		R_Shadow_EnlargeSurfaceBuffer(model->nummodelsurfaces); 
		model->GetLightInfo(ent, rtlight->shadoworigin, rtlight->radius, rtlight->cullmins, rtlight->cullmaxs, r_shadow_buffer_clusterlist, r_shadow_buffer_clusterpvs, &numclusters, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces);
		if (numclusters)
		{
			rtlight->static_numclusters = numclusters;
			rtlight->static_numclusterpvsbytes = (model->brush.num_pvsclusters + 7) >> 3;
			rtlight->static_clusterlist = Mem_Alloc(r_shadow_mempool, rtlight->static_numclusters * sizeof(*rtlight->static_clusterlist));
			rtlight->static_clusterpvs = Mem_Alloc(r_shadow_mempool, rtlight->static_numclusterpvsbytes);
			memcpy(rtlight->static_clusterlist, r_shadow_buffer_clusterlist, rtlight->static_numclusters * sizeof(*rtlight->static_clusterlist));
			memcpy(rtlight->static_clusterpvs, r_shadow_buffer_clusterpvs, rtlight->static_numclusterpvsbytes);
		}
		if (model->DrawShadowVolume && rtlight->shadow)
		{
			rtlight->static_meshchain_shadow = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768, 32768, NULL, NULL, NULL, false, false, true);
			model->DrawShadowVolume(ent, rtlight->shadoworigin, rtlight->radius, numsurfaces, r_shadow_buffer_surfacelist);
			rtlight->static_meshchain_shadow = Mod_ShadowMesh_Finish(r_shadow_mempool, rtlight->static_meshchain_shadow, false, false);
		}
		if (model->DrawLight)
		{
			rtlight->static_meshchain_light = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768, 32768, NULL, NULL, NULL, true, false, true);
			model->DrawLight(ent, rtlight->shadoworigin, vec3_origin, rtlight->radius, vec3_origin, &r_identitymatrix, &r_identitymatrix, &r_identitymatrix, NULL, numsurfaces, r_shadow_buffer_surfacelist);
			rtlight->static_meshchain_light = Mod_ShadowMesh_Finish(r_shadow_mempool, rtlight->static_meshchain_light, true, false);
		}
		// switch back to rendering when DrawShadowVolume or DrawLight is called
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

	lightmeshes = 0;
	lighttris = 0;
	if (rtlight->static_meshchain_light)
	{
		shadowmesh_t *mesh;
		for (mesh = rtlight->static_meshchain_light;mesh;mesh = mesh->next)
		{
			lightmeshes++;
			lighttris += mesh->numtriangles;
		}
	}

	Con_DPrintf("static light built: %f %f %f : %f %f %f box, %i shadow volume triangles (in %i meshes), %i light triangles (in %i meshes)\n", rtlight->cullmins[0], rtlight->cullmins[1], rtlight->cullmins[2], rtlight->cullmaxs[0], rtlight->cullmaxs[1], rtlight->cullmaxs[2], shadowtris, shadowmeshes, lighttris, lightmeshes);
}

void R_RTLight_Uncompile(rtlight_t *rtlight)
{
	if (rtlight->compiled)
	{
		if (rtlight->static_meshchain_shadow)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_shadow);
		rtlight->static_meshchain_shadow = NULL;
		if (rtlight->static_meshchain_light)
			Mod_ShadowMesh_Free(rtlight->static_meshchain_light);
		rtlight->static_meshchain_light = NULL;
		if (rtlight->static_clusterlist)
			Mem_Free(rtlight->static_clusterlist);
		rtlight->static_clusterlist = NULL;
		if (rtlight->static_clusterpvs)
			Mem_Free(rtlight->static_clusterpvs);
		rtlight->static_clusterpvs = NULL;
		rtlight->static_numclusters = 0;
		rtlight->static_numclusterpvsbytes = 0;
		rtlight->compiled = false;
	}
}

void R_Shadow_UncompileWorldLights(void)
{
	dlight_t *light;
	for (light = r_shadow_worldlightchain;light;light = light->next)
		R_RTLight_Uncompile(&light->rtlight);
}

void R_DrawRTLight(rtlight_t *rtlight, int visiblevolumes)
{
	int i, shadow;
	entity_render_t *ent;
	float f;
	vec3_t relativelightorigin, relativeeyeorigin, lightcolor, lightcolor2;
	rtexture_t *cubemaptexture;
	matrix4x4_t matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz;
	int numclusters, numsurfaces;
	int *clusterlist, *surfacelist;
	qbyte *clusterpvs;
	vec3_t cullmins, cullmaxs;
	shadowmesh_t *mesh;
	rmeshstate_t m;

	// loading is done before visibility checks because loading should happen
	// all at once at the start of a level, not when it stalls gameplay.
	// (especially important to benchmarks)
	if (rtlight->isstatic && !rtlight->compiled && r_shadow_staticworldlights.integer)
		R_RTLight_Compile(rtlight);
	if (rtlight->cubemapname[0])
		cubemaptexture = R_Shadow_Cubemap(rtlight->cubemapname);
	else
		cubemaptexture = NULL;

	cullmins[0] = rtlight->shadoworigin[0] - rtlight->radius;
	cullmins[1] = rtlight->shadoworigin[1] - rtlight->radius;
	cullmins[2] = rtlight->shadoworigin[2] - rtlight->radius;
	cullmaxs[0] = rtlight->shadoworigin[0] + rtlight->radius;
	cullmaxs[1] = rtlight->shadoworigin[1] + rtlight->radius;
	cullmaxs[2] = rtlight->shadoworigin[2] + rtlight->radius;
	if (d_lightstylevalue[rtlight->style] <= 0)
		return;
	numclusters = 0;
	clusterlist = NULL;
	clusterpvs = NULL;
	numsurfaces = 0;
	surfacelist = NULL;
	if (rtlight->compiled && r_shadow_staticworldlights.integer)
	{
		// compiled light, world available and can receive realtime lighting
		// retrieve cluster information
		numclusters = rtlight->static_numclusters;
		clusterlist = rtlight->static_clusterlist;
		clusterpvs = rtlight->static_clusterpvs;
		VectorCopy(rtlight->cullmins, cullmins);
		VectorCopy(rtlight->cullmaxs, cullmaxs);
	}
	else if (cl.worldmodel && cl.worldmodel->GetLightInfo)
	{
		// dynamic light, world available and can receive realtime lighting
		// if the light box is offscreen, skip it right away
		if (R_CullBox(cullmins, cullmaxs))
			return;
		// calculate lit surfaces and clusters
		R_Shadow_EnlargeClusterBuffer(cl.worldmodel->brush.num_pvsclusters);
		R_Shadow_EnlargeSurfaceBuffer(cl.worldmodel->nummodelsurfaces); 
		cl.worldmodel->GetLightInfo(&cl_entities[0].render, rtlight->shadoworigin, rtlight->radius, cullmins, cullmaxs, r_shadow_buffer_clusterlist, r_shadow_buffer_clusterpvs, &numclusters, r_shadow_buffer_surfacelist, r_shadow_buffer_surfacepvs, &numsurfaces);
		clusterlist = r_shadow_buffer_clusterlist;
		clusterpvs = r_shadow_buffer_clusterpvs;
		surfacelist = r_shadow_buffer_surfacelist;
	}
	// if the reduced cluster bounds are offscreen, skip it
	if (R_CullBox(cullmins, cullmaxs))
		return;
	// check if light is illuminating any visible clusters
	if (numclusters)
	{
		for (i = 0;i < numclusters;i++)
			if (CHECKPVSBIT(r_pvsbits, clusterlist[i]))
				break;
		if (i == numclusters)
			return;
	}
	// set up a scissor rectangle for this light
	if (R_Shadow_ScissorForBBox(cullmins, cullmaxs))
		return;

	f = d_lightstylevalue[rtlight->style] * (1.0f / 256.0f);
	VectorScale(rtlight->color, f, lightcolor);
	/*
	if (rtlight->selected)
	{
		f = 2 + sin(realtime * M_PI * 4.0);
		VectorScale(lightcolor, f, lightcolor);
	}
	*/

	shadow = rtlight->shadow && (rtlight->isstatic ? r_rtworldshadows : r_rtdlightshadows);

	if (shadow && (gl_stencil || visiblevolumes))
	{
		if (!visiblevolumes)
			R_Shadow_Stage_ShadowVolumes();
		ent = &cl_entities[0].render;
		if (r_shadow_staticworldlights.integer && rtlight->compiled)
		{
			memset(&m, 0, sizeof(m));
			R_Mesh_Matrix(&ent->matrix);
			for (mesh = rtlight->static_meshchain_shadow;mesh;mesh = mesh->next)
			{
				m.pointer_vertex = mesh->vertex3f;
				R_Mesh_State(&m);
				GL_LockArrays(0, mesh->numverts);
				if (r_shadowstage == SHADOWSTAGE_STENCIL)
				{
					// decrement stencil if frontface is behind depthbuffer
					qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
					qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
					R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
					c_rtcached_shadowmeshes++;
					c_rtcached_shadowtris += mesh->numtriangles;
					// increment stencil if backface is behind depthbuffer
					qglCullFace(GL_BACK); // quake is backwards, this culls front faces
					qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
				}
				R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
				c_rtcached_shadowmeshes++;
				c_rtcached_shadowtris += mesh->numtriangles;
				GL_LockArrays(0, 0);
			}
		}
		else if (numsurfaces)
		{
			Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, relativelightorigin);
			ent->model->DrawShadowVolume(ent, relativelightorigin, rtlight->radius, numsurfaces, surfacelist);
		}
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				// rough checks
				if (r_shadow_cull.integer)
				{
					if (!BoxesOverlap(ent->mins, ent->maxs, cullmins, cullmaxs))
						continue;
					if (cl.worldmodel != NULL && cl.worldmodel->brush.BoxTouchingPVS != NULL && !cl.worldmodel->brush.BoxTouchingPVS(cl.worldmodel, clusterpvs, ent->mins, ent->maxs))
						continue;
				}
				if (!(ent->flags & RENDER_SHADOW) || !ent->model || !ent->model->DrawShadowVolume)
					continue;
				Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, relativelightorigin);
				// light emitting entities should not cast their own shadow
				if (VectorLength2(relativelightorigin) < 0.1)
					continue;
				ent->model->DrawShadowVolume(ent, relativelightorigin, rtlight->radius, ent->model->nummodelsurfaces, ent->model->surfacelist);
			}
		}
	}

	if (!visiblevolumes)
	{
		R_Shadow_Stage_Light(shadow && gl_stencil);

		ent = &cl_entities[0].render;
		if (ent->model && ent->model->DrawLight && (ent->flags & RENDER_LIGHT))
		{
			Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, relativelightorigin);
			Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, relativeeyeorigin);
			Matrix4x4_Concat(&matrix_modeltolight, &rtlight->matrix_worldtolight, &ent->matrix);
			Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &rtlight->matrix_worldtoattenuationxyz, &ent->matrix);
			Matrix4x4_Concat(&matrix_modeltoattenuationz, &rtlight->matrix_worldtoattenuationz, &ent->matrix);
			if (r_shadow_staticworldlights.integer && rtlight->compiled)
			{
				R_Mesh_Matrix(&ent->matrix);
				for (mesh = rtlight->static_meshchain_light;mesh;mesh = mesh->next)
					R_Shadow_RenderLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoord2f, relativelightorigin, relativeeyeorigin, lightcolor, &matrix_modeltolight, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz, mesh->map_diffuse, mesh->map_normal, mesh->map_specular, cubemaptexture, LIGHTING_DIFFUSE | LIGHTING_SPECULAR);
			}
			else
				ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, rtlight->radius, lightcolor, &matrix_modeltolight, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz, cubemaptexture, numsurfaces, surfacelist);
		}
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				// can't draw transparent entity lighting here because
				// transparent meshes are deferred for later
				if (ent->visframe == r_framecount && BoxesOverlap(ent->mins, ent->maxs, cullmins, cullmaxs) && ent->model && ent->model->DrawLight && (ent->flags & (RENDER_LIGHT | RENDER_TRANSPARENT)) == RENDER_LIGHT)
				{
					VectorScale(lightcolor, ent->alpha, lightcolor2);
					Matrix4x4_Transform(&ent->inversematrix, rtlight->shadoworigin, relativelightorigin);
					Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, relativeeyeorigin);
					Matrix4x4_Concat(&matrix_modeltolight, &rtlight->matrix_worldtolight, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &rtlight->matrix_worldtoattenuationxyz, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationz, &rtlight->matrix_worldtoattenuationz, &ent->matrix);
					ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, rtlight->radius, lightcolor2, &matrix_modeltolight, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz, cubemaptexture, ent->model->nummodelsurfaces, ent->model->surfacelist);
				}
			}
		}
	}
}

void R_ShadowVolumeLighting(int visiblevolumes)
{
	int lnum;
	dlight_t *light;
	rmeshstate_t m;

	if (cl.worldmodel && strncmp(cl.worldmodel->name, r_shadow_mapname, sizeof(r_shadow_mapname)))
		R_Shadow_EditLights_Reload_f();

	if (visiblevolumes)
	{
		memset(&m, 0, sizeof(m));
		R_Mesh_State(&m);

		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(r_shadow_visiblevolumes.integer < 2);
		qglDisable(GL_CULL_FACE);
		GL_Color(0.0, 0.0125, 0.1, 1);
	}
	else
		R_Shadow_Stage_Begin();
	if (r_rtworld)
	{
		if (r_shadow_debuglight.integer >= 0)
		{
			for (lnum = 0, light = r_shadow_worldlightchain;light;lnum++, light = light->next)
				if (lnum == r_shadow_debuglight.integer)
					R_DrawRTLight(&light->rtlight, visiblevolumes);
		}
		else
			for (lnum = 0, light = r_shadow_worldlightchain;light;lnum++, light = light->next)
				R_DrawRTLight(&light->rtlight, visiblevolumes);
	}
	if (r_rtdlight)
		for (lnum = 0, light = r_dlight;lnum < r_numdlights;lnum++, light++)
			R_DrawRTLight(&light->rtlight, visiblevolumes);

	if (visiblevolumes)
	{
		qglEnable(GL_CULL_FACE);
		GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	}
	else
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
			snprintf(name, sizeof(name), "%s%s", basename, suffix[j][i].suffix);
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
						cubemappixels = Mem_Alloc(tempmempool, 6*cubemapsize*cubemapsize*4);
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
		return NULL;
	numcubemaps++;
	strcpy(cubemaps[i].basename, basename);
	cubemaps[i].texture = R_Shadow_LoadCubemap(cubemaps[i].basename);
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
	light = Mem_Alloc(r_shadow_mempool, sizeof(dlight_t));
	light->next = r_shadow_worldlightchain;
	r_shadow_worldlightchain = light;
	return light;
}

void R_Shadow_UpdateWorldLight(dlight_t *light, vec3_t origin, vec3_t angles, vec3_t color, vec_t radius, vec_t corona, int style, int shadowenable, const char *cubemapname)
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
	strlcpy(light->cubemapname, cubemapname, strlen(light->cubemapname));
	Matrix4x4_CreateFromQuakeEntity(&light->matrix, light->origin[0], light->origin[1], light->origin[2], light->angles[0], light->angles[1], light->angles[2], 1);

	R_RTLight_UpdateFromDLight(&light->rtlight, light, true);
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
	R_DrawSprite(GL_SRC_ALPHA, GL_ONE, lighttextures[0], false, r_editlights_cursorlocation, r_viewright, r_viewup, scale, -scale, -scale, scale, 1, 1, 1, 0.5f);
}

void R_Shadow_DrawLightSpriteCallback(const void *calldata1, int calldata2)
{
	float intensity;
	const dlight_t *light;
	light = calldata1;
	intensity = 0.5;
	if (light->selected)
		intensity = 0.75 + 0.25 * sin(realtime * M_PI * 4.0);
	if (!light->shadow)
		intensity *= 0.5f;
	R_DrawSprite(GL_SRC_ALPHA, GL_ONE, lighttextures[calldata2], false, light->origin, r_viewright, r_viewup, 8, -8, -8, 8, intensity, intensity, intensity, 0.5);
}

void R_Shadow_DrawLightSprites(void)
{
	int i;
	cachepic_t *pic;
	dlight_t *light;

	for (i = 0;i < 5;i++)
	{
		lighttextures[i] = NULL;
		if ((pic = Draw_CachePic(va("gfx/crosshair%i.tga", i + 1))))
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
			if (bestrating < rating && CL_TraceLine(light->origin, r_vieworigin, NULL, NULL, true, NULL, SUPERCONTENTS_SOLID) == 1.0f)
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
	int n, a, style, shadow;
	char name[MAX_QPATH], cubemapname[MAX_QPATH], *lightsstring, *s, *t;
	float origin[3], radius, color[3], angles[3], corona;
	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	lightsstring = FS_LoadFile(name, tempmempool, false);
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
			while (*s && *s != '\n')
				s++;
			if (!*s)
				break;
			*s = 0;
			shadow = true;
			// check for modifier flags
			if (*t == '!')
			{
				shadow = false;
				t++;
			}
			a = sscanf(t, "%f %f %f %f %f %f %f %d %s %f %f %f %f", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname, &corona, &angles[0], &angles[1], &angles[2]);
			if (a < 13)
				VectorClear(angles);
			if (a < 10)
				corona = 0;
			if (a < 9 || !strcmp(cubemapname, "\"\""))
				cubemapname[0] = 0;
			*s = '\n';
			if (a < 8)
			{
				Con_Printf("found %d parameters on line %i, should be 8 or more parameters (origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style \"cubemapname\" corona angles[0] angles[1] angles[2])\n", a, n + 1);
				break;
			}
			VectorScale(color, r_editlights_rtlightscolorscale.value, color);
			radius *= r_editlights_rtlightssizescale.value;
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, corona, style, shadow, cubemapname);
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
	int bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[1024];
	if (!r_shadow_worldlightchain)
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
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		if (light->cubemapname[0] || light->corona || light->angles[0] || light->angles[1] || light->angles[2])
			sprintf(line, "%s%f %f %f %f %f %f %f %d \"%s\" %f %f %f %f\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius / r_editlights_rtlightssizescale.value, light->color[0] / r_editlights_rtlightscolorscale.value, light->color[1] / r_editlights_rtlightscolorscale.value, light->color[2] / r_editlights_rtlightscolorscale.value, light->style, light->cubemapname, light->corona, light->angles[0], light->angles[1], light->angles[2]);
		else
			sprintf(line, "%s%f %f %f %f %f %f %f %d\n", light->shadow ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->radius / r_editlights_rtlightssizescale.value, light->color[0] / r_editlights_rtlightscolorscale.value, light->color[1] / r_editlights_rtlightscolorscale.value, light->color[2] / r_editlights_rtlightscolorscale.value, light->style);
		if (bufchars + (int) strlen(line) > bufmaxchars)
		{
			bufmaxchars = bufchars + strlen(line) + 2048;
			oldbuf = buf;
			buf = Mem_Alloc(r_shadow_mempool, bufmaxchars);
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
		FS_WriteFile(name, buf, bufchars);
	if (buf)
		Mem_Free(buf);
}

void R_Shadow_LoadLightsFile(void)
{
	int n, a, style;
	char name[MAX_QPATH], *lightsstring, *s, *t;
	float origin[3], radius, color[3], subtract, spotdir[3], spotcone, falloff, distbias;
	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".lights", sizeof (name));
	lightsstring = FS_LoadFile(name, tempmempool, false);
	if (lightsstring)
	{
		s = lightsstring;
		n = 0;
		while (*s)
		{
			t = s;
			while (*s && *s != '\n')
				s++;
			if (!*s)
				break;
			*s = 0;
			a = sscanf(t, "%f %f %f %f %f %f %f %f %f %f %f %f %f %d", &origin[0], &origin[1], &origin[2], &falloff, &color[0], &color[1], &color[2], &subtract, &spotdir[0], &spotdir[1], &spotdir[2], &spotcone, &distbias, &style);
			*s = '\n';
			if (a < 14)
			{
				Con_Printf("invalid lights file, found %d parameters on line %i, should be 14 parameters (origin[0] origin[1] origin[2] falloff light[0] light[1] light[2] subtract spotdir[0] spotdir[1] spotdir[2] spotcone distancebias style)\n", a, n + 1);
				break;
			}
			radius = sqrt(DotProduct(color, color) / (falloff * falloff * 8192.0f * 8192.0f));
			radius = bound(15, radius, 4096);
			VectorScale(color, (2.0f / (8388608.0f)), color);
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, vec3_origin, color, radius, 0, style, true, NULL);
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

	if (cl.worldmodel == NULL)
	{
		Con_Print("No map loaded.\n");
		return;
	}
	// try to load a .ent file first
	FS_StripExtension (cl.worldmodel->name, key, sizeof (key));
	strlcat (key, ".ent", sizeof (key));
	data = entfiledata = FS_LoadFile(key, tempmempool, true);
	// and if that is not found, fall back to the bsp file entity string
	if (!data)
		data = cl.worldmodel->brush.entities;
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
						originhack[2] = 48;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
			else if (cl.worldmodel->type == mod_brushq3)
			{
				if (!strcmp("scale", key))
					lightscale = atof(value);
				if (!strcmp("fade", key))
					fadescale = atof(value);
			}
			else if (!strcmp("skin", key))
				skin = (int)atof(value);
			else if (!strcmp("pflags", key))
				pflags = (int)atof(value);
			else if (!strcmp("effects", key))
				effects = (int)atof(value);
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
			R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), origin, angles, color, radius, (pflags & PFLAGS_CORONA) != 0, style, (pflags & PFLAGS_NOSHADOW) == 0, skin >= 16 ? va("cubemaps/%i", skin) : NULL);
	}
	if (entfiledata)
		Mem_Free(entfiledata);
}


void R_Shadow_SetCursorLocationForView(void)
{
	vec_t dist, push, frac;
	vec3_t dest, endpos, normal;
	VectorMA(r_vieworigin, r_editlights_cursordistance.value, r_viewforward, dest);
	frac = CL_TraceLine(r_vieworigin, dest, endpos, normal, true, NULL, SUPERCONTENTS_SOLID);
	if (frac < 1)
	{
		dist = frac * r_editlights_cursordistance.value;
		push = r_editlights_cursorpushback.value;
		if (push > dist)
			push = dist;
		push = -push;
		VectorMA(endpos, push, r_viewforward, endpos);
		VectorMA(endpos, r_editlights_cursorpushoff.value, normal, endpos);
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
	if (!cl.worldmodel)
		return;
	strlcpy(r_shadow_mapname, cl.worldmodel->name, sizeof(r_shadow_mapname));
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
	R_Shadow_UpdateWorldLight(R_Shadow_NewWorldLight(), r_editlights_cursorlocation, vec3_origin, color, 200, 0, 0, true, NULL);
}

void R_Shadow_EditLights_Edit_f(void)
{
	vec3_t origin, angles, color;
	vec_t radius, corona;
	int style, shadows;
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
	else
	{
		Con_Print("usage: r_editlights_edit [property] [value]\n");
		Con_Print("Selected light's properties:\n");
		Con_Printf("Origin : %f %f %f\n", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);
		Con_Printf("Angles : %f %f %f\n", r_shadow_selectedlight->angles[0], r_shadow_selectedlight->angles[1], r_shadow_selectedlight->angles[2]);
		Con_Printf("Color  : %f %f %f\n", r_shadow_selectedlight->color[0], r_shadow_selectedlight->color[1], r_shadow_selectedlight->color[2]);
		Con_Printf("Radius : %f\n", r_shadow_selectedlight->radius);
		Con_Printf("Corona : %f\n", r_shadow_selectedlight->corona);
		Con_Printf("Style  : %i\n", r_shadow_selectedlight->style);
		Con_Printf("Shadows: %s\n", r_shadow_selectedlight->shadow ? "yes" : "no");
		Con_Printf("Cubemap: %s\n", r_shadow_selectedlight->cubemapname);
		return;
	}
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, origin, angles, color, radius, corona, style, shadows, cubemapname);
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
	float x, y;
	char temp[256];
	if (!r_editlights.integer)
		return;
	x = 0;
	y = con_vislines;
	sprintf(temp, "Cursor  %f %f %f", r_editlights_cursorlocation[0], r_editlights_cursorlocation[1], r_editlights_cursorlocation[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	if (r_shadow_selectedlight == NULL)
		return;
	sprintf(temp, "Light properties");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Origin  %f %f %f", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Angles  %f %f %f", r_shadow_selectedlight->angles[0], r_shadow_selectedlight->angles[1], r_shadow_selectedlight->angles[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Color   %f %f %f", r_shadow_selectedlight->color[0], r_shadow_selectedlight->color[1], r_shadow_selectedlight->color[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Radius  %f", r_shadow_selectedlight->radius);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Corona  %f", r_shadow_selectedlight->corona);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Style   %i", r_shadow_selectedlight->style);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Shadows %s", r_shadow_selectedlight->shadow ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Cubemap %s", r_shadow_selectedlight->cubemapname);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
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
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_selectedlight->angles, r_shadow_selectedlight->color, r_shadow_selectedlight->radius, r_shadow_selectedlight->corona, r_shadow_selectedlight->style, !r_shadow_selectedlight->shadow, r_shadow_selectedlight->cubemapname);
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
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_selectedlight->angles, r_shadow_selectedlight->color, r_shadow_selectedlight->radius, !r_shadow_selectedlight->corona, r_shadow_selectedlight->style, r_shadow_selectedlight->shadow, r_shadow_selectedlight->cubemapname);
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
"r_editlights_rtlightssizescale : imported rtlight size scaling\n"
"r_editlights_rtlightscolorscale : imported rtlight color scaling\n"
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
"style style : set lightstyle of light (flickering patterns, switches, etc)\n"
"cubemap basename : set filter cubemap of light (not yet supported)\n"
"shadows 1/0 : turn on/off shadows\n"
"corona n : set corona intensity\n"
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
	R_Shadow_UpdateWorldLight(r_shadow_selectedlight, r_shadow_selectedlight->origin, r_shadow_bufferlight.angles, r_shadow_bufferlight.color, r_shadow_bufferlight.radius, r_shadow_bufferlight.corona, r_shadow_bufferlight.style, r_shadow_bufferlight.shadow, r_shadow_bufferlight.cubemapname);
}

void R_Shadow_EditLights_Init(void)
{
	Cvar_RegisterVariable(&r_editlights);
	Cvar_RegisterVariable(&r_editlights_cursordistance);
	Cvar_RegisterVariable(&r_editlights_cursorpushback);
	Cvar_RegisterVariable(&r_editlights_cursorpushoff);
	Cvar_RegisterVariable(&r_editlights_cursorgrid);
	Cvar_RegisterVariable(&r_editlights_quakelightsizescale);
	Cvar_RegisterVariable(&r_editlights_rtlightssizescale);
	Cvar_RegisterVariable(&r_editlights_rtlightscolorscale);
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

