
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
stencil wrap extension (but probably should support it).



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

extern void R_Shadow_EditLights_Init(void);

#define SHADOWSTAGE_NONE 0
#define SHADOWSTAGE_STENCIL 1
#define SHADOWSTAGE_LIGHT 2
#define SHADOWSTAGE_ERASESTENCIL 3

int r_shadowstage = SHADOWSTAGE_NONE;
int r_shadow_reloadlights = false;

mempool_t *r_shadow_mempool;

int maxshadowelements;
int *shadowelements;
int maxtrianglefacinglight;
qbyte *trianglefacinglight;
int *trianglefacinglightlist;

int maxvertexupdate;
int *vertexupdate;
int *vertexremap;
int vertexupdatenum;

rtexturepool_t *r_shadow_texturepool;
rtexture_t *r_shadow_normalcubetexture;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_attenuation3dtexture;
rtexture_t *r_shadow_blankbumptexture;
rtexture_t *r_shadow_blankglosstexture;
rtexture_t *r_shadow_blankwhitetexture;

cvar_t r_shadow_lightattenuationpower = {0, "r_shadow_lightattenuationpower", "0.5"};
cvar_t r_shadow_lightattenuationscale = {0, "r_shadow_lightattenuationscale", "1"};
cvar_t r_shadow_lightintensityscale = {0, "r_shadow_lightintensityscale", "1"};
cvar_t r_shadow_realtime_world = {0, "r_shadow_realtime_world", "0"};
cvar_t r_shadow_realtime_dlight = {0, "r_shadow_realtime_dlight", "0"};
cvar_t r_shadow_visiblevolumes = {0, "r_shadow_visiblevolumes", "0"};
cvar_t r_shadow_gloss = {0, "r_shadow_gloss", "1"};
cvar_t r_shadow_glossintensity = {0, "r_shadow_glossintensity", "1"};
cvar_t r_shadow_gloss2intensity = {0, "r_shadow_gloss2intensity", "0.25"};
cvar_t r_shadow_debuglight = {0, "r_shadow_debuglight", "-1"};
cvar_t r_shadow_scissor = {0, "r_shadow_scissor", "1"};
cvar_t r_shadow_bumpscale_bumpmap = {0, "r_shadow_bumpscale_bumpmap", "4"};
cvar_t r_shadow_bumpscale_basetexture = {0, "r_shadow_bumpscale_basetexture", "0"};
cvar_t r_shadow_polygonoffset = {0, "r_shadow_polygonoffset", "0"};
cvar_t r_shadow_portallight = {0, "r_shadow_portallight", "1"};
cvar_t r_shadow_projectdistance = {0, "r_shadow_projectdistance", "10000"};
cvar_t r_shadow_texture3d = {0, "r_shadow_texture3d", "1"};
cvar_t r_shadow_singlepassvolumegeneration = {0, "r_shadow_singlepassvolumegeneration", "1"};
cvar_t r_shadow_shadows = {CVAR_SAVE, "r_shadow_shadows", "1"};
cvar_t r_shadow_showtris = {0, "r_shadow_showtris", "0"};

int c_rt_lights, c_rt_clears, c_rt_scissored;
int c_rt_shadowmeshes, c_rt_shadowtris, c_rt_lightmeshes, c_rt_lighttris;
int c_rtcached_shadowmeshes, c_rtcached_shadowtris;

void R_Shadow_ClearWorldLights(void);
void R_Shadow_SaveWorldLights(void);
void R_Shadow_LoadWorldLights(void);
void R_Shadow_LoadLightsFile(void);
void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void);

void r_shadow_start(void)
{
	// allocate vertex processing arrays
	r_shadow_mempool = Mem_AllocPool("R_Shadow");
	maxshadowelements = 0;
	shadowelements = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	trianglefacinglightlist = NULL;
	r_shadow_normalcubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	r_shadow_blankglosstexture = NULL;
	r_shadow_blankwhitetexture = NULL;
	r_shadow_texturepool = NULL;
	R_Shadow_ClearWorldLights();
	r_shadow_reloadlights = true;
}

void r_shadow_shutdown(void)
{
	R_Shadow_ClearWorldLights();
	r_shadow_reloadlights = true;
	r_shadow_normalcubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_attenuation3dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	r_shadow_blankglosstexture = NULL;
	r_shadow_blankwhitetexture = NULL;
	R_FreeTexturePool(&r_shadow_texturepool);
	maxshadowelements = 0;
	shadowelements = NULL;
	maxvertexupdate = 0;
	vertexupdate = NULL;
	vertexremap = NULL;
	vertexupdatenum = 0;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	trianglefacinglightlist = NULL;
	Mem_FreePool(&r_shadow_mempool);
}

void r_shadow_newmap(void)
{
	R_Shadow_ClearWorldLights();
	r_shadow_reloadlights = true;
}

void R_Shadow_Help_f(void)
{
	Con_Printf(
"Documentation on r_shadow system:\n"
"Settings:\n"
"r_shadow_lightattenuationpower : used to generate attenuation texture\n"
"r_shadow_lightattenuationscale : used to generate attenuation texture\n"
"r_shadow_lightintensityscale : scale rendering brightness of all lights\n"
"r_shadow_realtime_world : use realtime world light rendering\n"
"r_shadow_realtime_dlight : use high quality dlight rendering\n"
"r_shadow_visiblevolumes : useful for performance testing; bright = slow!\n"
"r_shadow_gloss 0/1/2 : no gloss, gloss textures only, force gloss\n"
"r_shadow_glossintensity : brightness of textured gloss\n"
"r_shadow_gloss2intensity : brightness of forced gloss\n"
"r_shadow_debuglight : render only this light number (-1 = all)\n"
"r_shadow_scissor : use scissor optimization\n"
"r_shadow_bumpscale_bumpmap : depth scale for bumpmap conversion\n"
"r_shadow_bumpscale_basetexture : base texture as bumpmap with this scale\n"
"r_shadow_polygonoffset : nudge shadow volumes closer/further\n"
"r_shadow_portallight : use portal visibility for static light precomputation\n"
"r_shadow_projectdistance : shadow volume projection distance\n"
"r_shadow_texture3d : use 3d attenuation texture (if hardware supports)\n"
"r_shadow_singlepassvolumegeneration : selects shadow volume algorithm\n"
"r_shadow_shadows : dlight shadows (world always has shadows)\n"
"Commands:\n"
"r_shadow_help : this help\n"
	);
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_lightattenuationpower);
	Cvar_RegisterVariable(&r_shadow_lightattenuationscale);
	Cvar_RegisterVariable(&r_shadow_lightintensityscale);
	Cvar_RegisterVariable(&r_shadow_realtime_world);
	Cvar_RegisterVariable(&r_shadow_realtime_dlight);
	Cvar_RegisterVariable(&r_shadow_visiblevolumes);
	Cvar_RegisterVariable(&r_shadow_gloss);
	Cvar_RegisterVariable(&r_shadow_glossintensity);
	Cvar_RegisterVariable(&r_shadow_gloss2intensity);
	Cvar_RegisterVariable(&r_shadow_debuglight);
	Cvar_RegisterVariable(&r_shadow_scissor);
	Cvar_RegisterVariable(&r_shadow_bumpscale_bumpmap);
	Cvar_RegisterVariable(&r_shadow_bumpscale_basetexture);
	Cvar_RegisterVariable(&r_shadow_polygonoffset);
	Cvar_RegisterVariable(&r_shadow_portallight);
	Cvar_RegisterVariable(&r_shadow_projectdistance);
	Cvar_RegisterVariable(&r_shadow_texture3d);
	Cvar_RegisterVariable(&r_shadow_singlepassvolumegeneration);
	Cvar_RegisterVariable(&r_shadow_shadows);
	Cvar_RegisterVariable(&r_shadow_showtris);
	Cmd_AddCommand("r_shadow_help", R_Shadow_Help_f);
	R_Shadow_EditLights_Init();
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

void R_Shadow_ResizeTriangleFacingLight(int numtris)
{
	// make sure trianglefacinglight is big enough for this volume
	// ameks ru ertaignelaficgnilhg tsib gie ongu hof rhtsiv lomu e
	// m4k3 5ur3 7r14ng13f4c1n5115h7 15 b15 3n0u5h f0r 7h15 v01um3
	if (maxtrianglefacinglight < numtris)
	{
		maxtrianglefacinglight = numtris;
		if (trianglefacinglight)
			Mem_Free(trianglefacinglight);
		if (trianglefacinglightlist)
			Mem_Free(trianglefacinglightlist);
		trianglefacinglight = Mem_Alloc(r_shadow_mempool, maxtrianglefacinglight);
		trianglefacinglightlist = Mem_Alloc(r_shadow_mempool, sizeof(int) * maxtrianglefacinglight);
	}
}

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

/*
// readable version of some code found below
//if ((t >= trianglerange_start && t < trianglerange_end) ? (t < i && !trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
int PointInfrontOfTriangle(const float *p, const float *a, const float *b, const float *c)
{
	float dir0[3], dir1[3], normal[3];

	// calculate two mostly perpendicular edge directions
	VectorSubtract(a, b, dir0);
	VectorSubtract(c, b, dir1);

	// we have two edge directions, we can calculate a third vector from
	// them, which is the direction of the surface normal (it's magnitude
	// is not 1 however)
	CrossProduct(dir0, dir1, normal);

	// compare distance of light along normal, with distance of any point
	// of the triangle along the same normal (the triangle is planar,
	// I.E. flat, so all points give the same answer)
	return DotProduct(p, normal) > DotProduct(a, normal);
}
int checkcastshadowfromedge(int t, int i)
{
	int *te;
	float *v[3];
	if (t >= trianglerange_start && t < trianglerange_end)
	{
		if (t < i && !trianglefacinglight[t])
			return true;
		else
			return false;
	}
	else
	{
		if (t < 0)
			return true;
		else
		{
			te = inelement3i + t * 3;
			v[0] = invertex3f + te[0] * 3;
			v[1] = invertex3f + te[1] * 3;
			v[2] = invertex3f + te[2] * 3;
			if (!PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
				return true;
			else
				return false;
		}
	}
}
*/

int R_Shadow_ConstructShadowVolume(int innumvertices, int trianglerange_start, int trianglerange_end, const int *inelement3i, const int *inneighbor3i, const float *invertex3f, int *outnumvertices, int *outelement3i, float *outvertex3f, const float *relativelightorigin, float projectdistance)
{
	int i, j, tris = 0, numfacing = 0, vr[3], t, outvertices = 0;
	const float *v[3];
	const int *e, *n, *te;
	float f, temp[3];

	// make sure trianglefacinglight is big enough for this volume
	if (maxtrianglefacinglight < trianglerange_end)
		R_Shadow_ResizeTriangleFacingLight(trianglerange_end);

	if (maxvertexupdate < innumvertices)
	{
		maxvertexupdate = innumvertices;
		if (vertexupdate)
			Mem_Free(vertexupdate);
		if (vertexremap)
			Mem_Free(vertexremap);
		vertexupdate = Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
		vertexremap = Mem_Alloc(r_shadow_mempool, maxvertexupdate * sizeof(int));
	}
	vertexupdatenum++;

	if (r_shadow_singlepassvolumegeneration.integer)
	{
		// one pass approach (identify lit/dark faces and generate sides while doing so)
		for (i = trianglerange_start, e = inelement3i + i * 3, n = inneighbor3i + i * 3;i < trianglerange_end;i++, e += 3, n += 3)
		{
			// calculate triangle facing flag
			v[0] = invertex3f + e[0] * 3;
			v[1] = invertex3f + e[1] * 3;
			v[2] = invertex3f + e[2] * 3;
			if((trianglefacinglight[i] = PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2])))
			{
				// make sure the vertices are created
				for (j = 0;j < 3;j++)
				{
					if (vertexupdate[e[j]] != vertexupdatenum)
					{
						vertexupdate[e[j]] = vertexupdatenum;
						vertexremap[e[j]] = outvertices;
						VectorCopy(v[j], outvertex3f);
						VectorSubtract(v[j], relativelightorigin, temp);
						f = projectdistance / VectorLength(temp);
						VectorMA(relativelightorigin, f, temp, (outvertex3f + 3));
						outvertex3f += 6;
						outvertices += 2;
					}
				}
				// output the front and back triangles
				vr[0] = vertexremap[e[0]];
				vr[1] = vertexremap[e[1]];
				vr[2] = vertexremap[e[2]];
				outelement3i[0] = vr[0];
				outelement3i[1] = vr[1];
				outelement3i[2] = vr[2];
				outelement3i[3] = vr[2] + 1;
				outelement3i[4] = vr[1] + 1;
				outelement3i[5] = vr[0] + 1;
				outelement3i += 6;
				tris += 2;
				// output the sides (facing outward from this triangle)
				t = n[0];
				if ((t >= trianglerange_start && t < trianglerange_end) ? (t < i && !trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
				{
					outelement3i[0] = vr[1];
					outelement3i[1] = vr[0];
					outelement3i[2] = vr[0] + 1;
					outelement3i[3] = vr[1];
					outelement3i[4] = vr[0] + 1;
					outelement3i[5] = vr[1] + 1;
					outelement3i += 6;
					tris += 2;
				}
				t = n[1];
				if ((t >= trianglerange_start && t < trianglerange_end) ? (t < i && !trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
				{
					outelement3i[0] = vr[2];
					outelement3i[1] = vr[1];
					outelement3i[2] = vr[1] + 1;
					outelement3i[3] = vr[2];
					outelement3i[4] = vr[1] + 1;
					outelement3i[5] = vr[2] + 1;
					outelement3i += 6;
					tris += 2;
				}
				t = n[2];
				if ((t >= trianglerange_start && t < trianglerange_end) ? (t < i && !trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
				{
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
			else
			{
				// this triangle is not facing the light
				// output the sides (facing inward to this triangle)
				t = n[0];
				if (t < i && t >= trianglerange_start && t < trianglerange_end && trianglefacinglight[t])
				{
					vr[0] = vertexremap[e[0]];
					vr[1] = vertexremap[e[1]];
					outelement3i[0] = vr[1];
					outelement3i[1] = vr[0] + 1;
					outelement3i[2] = vr[0];
					outelement3i[3] = vr[1];
					outelement3i[4] = vr[1] + 1;
					outelement3i[5] = vr[0] + 1;
					outelement3i += 6;
					tris += 2;
				}
				t = n[1];
				if (t < i && t >= trianglerange_start && t < trianglerange_end && trianglefacinglight[t])
				{
					vr[1] = vertexremap[e[1]];
					vr[2] = vertexremap[e[2]];
					outelement3i[0] = vr[2];
					outelement3i[1] = vr[1] + 1;
					outelement3i[2] = vr[1];
					outelement3i[3] = vr[2];
					outelement3i[4] = vr[2] + 1;
					outelement3i[5] = vr[1] + 1;
					outelement3i += 6;
					tris += 2;
				}
				t = n[2];
				if (t < i && t >= trianglerange_start && t < trianglerange_end && trianglefacinglight[t])
				{
					vr[0] = vertexremap[e[0]];
					vr[2] = vertexremap[e[2]];
					outelement3i[0] = vr[0];
					outelement3i[1] = vr[2] + 1;
					outelement3i[2] = vr[2];
					outelement3i[3] = vr[0];
					outelement3i[4] = vr[0] + 1;
					outelement3i[5] = vr[2] + 1;
					outelement3i += 6;
					tris += 2;
				}
			}
		}
	}
	else
	{
		// two pass approach (identify lit/dark faces and then generate sides)
		for (i = trianglerange_start, e = inelement3i + i * 3, numfacing = 0;i < trianglerange_end;i++, e += 3)
		{
			// calculate triangle facing flag
			v[0] = invertex3f + e[0] * 3;
			v[1] = invertex3f + e[1] * 3;
			v[2] = invertex3f + e[2] * 3;
			if((trianglefacinglight[i] = PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2])))
			{
				trianglefacinglightlist[numfacing++] = i;
				// make sure the vertices are created
				for (j = 0;j < 3;j++)
				{
					if (vertexupdate[e[j]] != vertexupdatenum)
					{
						vertexupdate[e[j]] = vertexupdatenum;
						vertexremap[e[j]] = outvertices;
						VectorSubtract(v[j], relativelightorigin, temp);
						f = projectdistance / VectorLength(temp);
						VectorCopy(v[j], outvertex3f);
						VectorMA(relativelightorigin, f, temp, (outvertex3f + 3));
						outvertex3f += 6;
						outvertices += 2;
					}
				}
				// output the front and back triangles
				outelement3i[0] = vertexremap[e[0]];
				outelement3i[1] = vertexremap[e[1]];
				outelement3i[2] = vertexremap[e[2]];
				outelement3i[3] = vertexremap[e[2]] + 1;
				outelement3i[4] = vertexremap[e[1]] + 1;
				outelement3i[5] = vertexremap[e[0]] + 1;
				outelement3i += 6;
				tris += 2;
			}
		}
		for (i = 0;i < numfacing;i++)
		{
			t = trianglefacinglightlist[i];
			e = inelement3i + t * 3;
			n = inneighbor3i + t * 3;
			// output the sides (facing outward from this triangle)
			t = n[0];
			if ((t >= trianglerange_start && t < trianglerange_end) ? (!trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
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
			t = n[1];
			if ((t >= trianglerange_start && t < trianglerange_end) ? (!trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
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
			t = n[2];
			if ((t >= trianglerange_start && t < trianglerange_end) ? (!trianglefacinglight[t]) : (t < 0 || (te = inelement3i + t * 3, v[0] = invertex3f + te[0] * 3, v[1] = invertex3f + te[1] * 3, v[2] = invertex3f + te[2] * 3, !PointInfrontOfTriangle(relativelightorigin, v[0], v[1], v[2]))))
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
	}
	if (outnumvertices)
		*outnumvertices = outvertices;
	return tris;
}

float varray_vertex3f2[65536*3];

void R_Shadow_Volume(int numverts, int numtris, const float *invertex3f, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance)
{
	int tris, outverts;
	if (projectdistance < 0.1)
	{
		Con_Printf("R_Shadow_Volume: projectdistance %f\n");
		return;
	}
	if (!numverts)
		return;

	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
		R_Shadow_ResizeShadowElements(numtris);

	// check which triangles are facing the light, and then output
	// triangle elements and vertices...  by clever use of elements we
	// can construct the whole shadow from the unprojected vertices and
	// the projected vertices
	if ((tris = R_Shadow_ConstructShadowVolume(numverts, 0, numtris, elements, neighbors, invertex3f, &outverts, shadowelements, varray_vertex3f2, relativelightorigin, r_shadow_projectdistance.value/*projectdistance*/)))
	{
		GL_VertexPointer(varray_vertex3f2);
		if (r_shadowstage == SHADOWSTAGE_STENCIL)
		{
			// increment stencil if backface is behind depthbuffer
			qglCullFace(GL_BACK); // quake is backwards, this culls front faces
			qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
			R_Mesh_Draw(outverts, tris, shadowelements);
			c_rt_shadowmeshes++;
			c_rt_shadowtris += numtris;
			// decrement stencil if frontface is behind depthbuffer
			qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
			qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		}
		R_Mesh_Draw(outverts, tris, shadowelements);
		c_rt_shadowmeshes++;
		c_rt_shadowtris += numtris;
	}
}

void R_Shadow_RenderShadowMeshVolume(shadowmesh_t *firstmesh)
{
	shadowmesh_t *mesh;
	if (r_shadowstage == SHADOWSTAGE_STENCIL)
	{
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		for (mesh = firstmesh;mesh;mesh = mesh->next)
		{
			GL_VertexPointer(mesh->vertex3f);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
			c_rtcached_shadowmeshes++;
			c_rtcached_shadowtris += mesh->numtriangles;
		}
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	}
	for (mesh = firstmesh;mesh;mesh = mesh->next)
	{
		GL_VertexPointer(mesh->vertex3f);
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->element3i);
		c_rtcached_shadowmeshes++;
		c_rtcached_shadowtris += mesh->numtriangles;
	}
}

float r_shadow_attenpower, r_shadow_attenscale;
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

void R_Shadow_Stage_Begin(void)
{
	rmeshstate_t m;

	if (r_shadow_texture3d.integer && !gl_texture3d)
		Cvar_SetValueQuick(&r_shadow_texture3d, 0);

	if (!r_shadow_attenuation2dtexture
	 || (!r_shadow_attenuation3dtexture && r_shadow_texture3d.integer)
	 || r_shadow_lightattenuationpower.value != r_shadow_attenpower
	 || r_shadow_lightattenuationscale.value != r_shadow_attenscale)
		R_Shadow_MakeTextures();

	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_State_Texture(&m);
	GL_Color(0, 0, 0, 1);
	qglDisable(GL_SCISSOR_TEST);
	r_shadowstage = SHADOWSTAGE_NONE;

	c_rt_lights = c_rt_clears = c_rt_scissored = 0;
	c_rt_shadowmeshes = c_rt_shadowtris = c_rt_lightmeshes = c_rt_lighttris = 0;
	c_rtcached_shadowmeshes = c_rtcached_shadowtris = 0;
}

void R_Shadow_LoadWorldLightsIfNeeded(void)
{
	if (r_shadow_reloadlights && cl.worldmodel)
	{
		R_Shadow_ClearWorldLights();
		r_shadow_reloadlights = false;
		R_Shadow_LoadWorldLights();
		if (r_shadow_worldlightchain == NULL)
		{
			R_Shadow_LoadLightsFile();
			if (r_shadow_worldlightchain == NULL)
				R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite();
		}
	}
}

void R_Shadow_Stage_ShadowVolumes(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);
	GL_Color(1, 1, 1, 1);
	qglColorMask(0, 0, 0, 0);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(false);
	GL_DepthTest(true);
	if (r_shadow_polygonoffset.value != 0)
	{
		qglPolygonOffset(1.0f, r_shadow_polygonoffset.value);
		qglEnable(GL_POLYGON_OFFSET_FILL);
	}
	else
		qglDisable(GL_POLYGON_OFFSET_FILL);
	qglDepthFunc(GL_LESS);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 128, 0xFF);
	r_shadowstage = SHADOWSTAGE_STENCIL;
	qglClear(GL_STENCIL_BUFFER_BIT);
	c_rt_clears++;
	// LordHavoc note: many shadow volumes reside entirely inside the world
	// (that is to say they are entirely bounded by their lit surfaces),
	// which can be optimized by handling things as an inverted light volume,
	// with the shadow boundaries of the world being simulated by an altered
	// (129) bias to stencil clearing on such lights
	// FIXME: generate inverted light volumes for use as shadow volumes and
	// optimize for them as noted above
}

void R_Shadow_Stage_LightWithoutShadows(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthFunc(GL_EQUAL);
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_EQUAL, 128, 0xFF);
	r_shadowstage = SHADOWSTAGE_LIGHT;
	c_rt_lights++;
}

void R_Shadow_Stage_LightWithShadows(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthFunc(GL_EQUAL);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw light where this geometry was already rendered AND the
	// stencil is 128 (values other than this mean shadow)
	qglStencilFunc(GL_EQUAL, 128, 0xFF);
	r_shadowstage = SHADOWSTAGE_LIGHT;
	c_rt_lights++;
}

void R_Shadow_Stage_End(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(true);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDisable(GL_SCISSOR_TEST);
	qglDepthFunc(GL_LEQUAL);
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 128, 0xFF);
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
	// LordHavoc: for some odd reason scissor seems broken without stencil
	// (?!?  seems like a driver bug) so abort if gl_stencil is false
	if (!gl_stencil || BoxesOverlap(r_vieworigin, r_vieworigin, mins, maxs))
	{
		qglDisable(GL_SCISSOR_TEST);
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
	if (ix1 < r_refdef.x) ix1 = r_refdef.x;
	if (iy1 < r_refdef.y) iy1 = r_refdef.y;
	if (ix2 > r_refdef.x + r_refdef.width) ix2 = r_refdef.x + r_refdef.width;
	if (iy2 > r_refdef.y + r_refdef.height) iy2 = r_refdef.y + r_refdef.height;
	if (ix2 <= ix1 || iy2 <= iy1)
		return true;
	// set up the scissor rectangle
	qglScissor(ix1, iy1, ix2 - ix1, iy2 - iy1);
	qglEnable(GL_SCISSOR_TEST);
	c_rt_scissored++;
	return false;
}

void R_Shadow_VertexLighting(int numverts, const float *vertex3f, const float *normal3f, const float *lightcolor, const matrix4x4_t *m)
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
				intensity = pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale * dot / sqrt(DotProduct(n,n));
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

void R_Shadow_VertexLightingWithXYAttenuationTexture(int numverts, const float *vertex3f, const float *normal3f, const float *lightcolor, const matrix4x4_t *m)
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
				intensity = pow(1 - dist, r_shadow_attenpower) * r_shadow_attenscale * dot / sqrt(DotProduct(n,n));
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

// FIXME: this should be done in a vertex program when possible
// FIXME: if vertex program not available, this would really benefit from 3DNow! or SSE
void R_Shadow_Transform_Vertex3f_TexCoord3f(float *tc3f, int numverts, const float *vertex3f, const matrix4x4_t *matrix)
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

void R_Shadow_Transform_Vertex3f_TexCoord2f(float *tc2f, int numverts, const float *vertex3f, const matrix4x4_t *matrix)
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

void R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(float *out3f, int numverts, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const vec3_t relativelightorigin)
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

void R_Shadow_GenTexCoords_Specular_NormalCubeMap(float *out3f, int numverts, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const vec3_t relativelightorigin, const vec3_t relativeeyeorigin)
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

void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, const float *relativelightorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int renders;
	float color[3], color2[3];
	rmeshstate_t m;
	GL_VertexPointer(vertex3f);
	if (gl_dot3arb && gl_texturecubemap && gl_combine.integer && gl_stencil)
	{
		if (!bumptexture)
			bumptexture = r_shadow_blankbumptexture;
		GL_Color(1,1,1,1);
		// colorscale accounts for how much we multiply the brightness during combine
		// mult is how many times the final pass of the lighting will be
		// performed to get more brightness than otherwise possible
		// limit mult to 64 for sanity sake
		if (r_shadow_texture3d.integer && r_textureunits.integer >= 4)
		{
			// 3/2 3D combine path (Geforce3, Radeon 8500)
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.tex3d[2] = R_GetTexture(r_shadow_attenuation3dtexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			m.pointer_texcoord[2] = varray_texcoord3f[2];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
			R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[2], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && lightcubemap)
		{
			// 1/2/2 3D combine path (original Radeon)
			memset(&m, 0, sizeof(m));
			m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
			m.pointer_texcoord[0] = varray_texcoord3f[0];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && !lightcubemap)
		{
			// 2/2 3D combine path (original Radeon)
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else if (r_textureunits.integer >= 4)
		{
			// 4/2 2D combine path (Geforce3, Radeon 8500)
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.tex[3] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			m.pointer_texcoord[2] = varray_texcoord2f[2];
			m.pointer_texcoord[3] = varray_texcoord2f[3];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[2], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[3], numverts, vertex3f, matrix_modeltoattenuationz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else
		{
			// 2/2/2 2D combine path (any dot3 card)
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.pointer_texcoord[0] = varray_texcoord2f[0];
			m.pointer_texcoord[1] = varray_texcoord2f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
	}
	else
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		GL_ColorPointer(varray_color4f);
		VectorScale(lightcolor, r_shadow_lightintensityscale.value, color2);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(basetexture);
		m.pointer_texcoord[0] = texcoord2f;
		if (r_textureunits.integer >= 2)
		{
			// voodoo2
			m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.pointer_texcoord[1] = varray_texcoord2f[1];
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
		}
		R_Mesh_State_Texture(&m);
		for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
		{
			color[0] = bound(0, color2[0], 1);
			color[1] = bound(0, color2[1], 1);
			color[2] = bound(0, color2[2], 1);
			if (r_textureunits.integer >= 2)
				R_Shadow_VertexLightingWithXYAttenuationTexture(numverts, vertex3f, normal3f, color, matrix_modeltofilter);
			else
				R_Shadow_VertexLighting(numverts, vertex3f, normal3f, color, matrix_modeltofilter);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;
		}
	}
}

void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int renders;
	float color[3], color2[3], colorscale;
	rmeshstate_t m;
	if (!gl_dot3arb || !gl_texturecubemap || !gl_combine.integer || !gl_stencil)
		return;
	if (!glosstexture)
		glosstexture = r_shadow_blankglosstexture;
	if (r_shadow_gloss.integer >= 2 || (r_shadow_gloss.integer >= 1 && glosstexture != r_shadow_blankglosstexture))
	{
		colorscale = r_shadow_glossintensity.value;
		if (!bumptexture)
			bumptexture = r_shadow_blankbumptexture;
		if (glosstexture == r_shadow_blankglosstexture)
			colorscale *= r_shadow_gloss2intensity.value;
		GL_VertexPointer(vertex3f);
		GL_Color(1,1,1,1);
		if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && lightcubemap /*&& gl_support_blendsquare*/) // FIXME: detect blendsquare!
		{
			// 2/0/0/1/2 3D combine blendsquare path
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			// this squares the result
			GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
			R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			R_Mesh_State_Texture(&m);
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

			memset(&m, 0, sizeof(m));
			m.tex3d[0] = R_GetTexture(r_shadow_attenuation3dtexture);
			m.pointer_texcoord[0] = varray_texcoord3f[0];
			R_Mesh_State_Texture(&m);
			GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
			R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(glosstexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, colorscale, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else if (r_shadow_texture3d.integer && r_textureunits.integer >= 2 && !lightcubemap /*&& gl_support_blendsquare*/) // FIXME: detect blendsquare!
		{
			// 2/0/0/2 3D combine blendsquare path
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			// this squares the result
			GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
			R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			R_Mesh_State_Texture(&m);
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

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(glosstexture);
			m.tex3d[1] = R_GetTexture(r_shadow_attenuation3dtexture);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltoattenuationxyz);
			VectorScale(lightcolor, colorscale, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
		else if (r_textureunits.integer >= 2 /*&& gl_support_blendsquare*/) // FIXME: detect blendsquare!
		{
			// 2/0/0/2/2 2D combine blendsquare path
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(bumptexture);
			m.texcubemap[1] = R_GetTexture(r_shadow_normalcubetexture);
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = varray_texcoord3f[1];
			R_Mesh_State_Texture(&m);
			qglColorMask(0,0,0,1);
			// this squares the result
			GL_BlendFunc(GL_SRC_ALPHA, GL_ZERO);
			R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord3f[1], numverts, vertex3f, svector3f, tvector3f, normal3f, relativelightorigin, relativeeyeorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			R_Mesh_State_Texture(&m);
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

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
			m.pointer_texcoord[0] = varray_texcoord2f[0];
			m.pointer_texcoord[1] = varray_texcoord2f[1];
			R_Mesh_State_Texture(&m);
			GL_BlendFunc(GL_DST_ALPHA, GL_ZERO);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[0], numverts, vertex3f, matrix_modeltoattenuationxyz);
			R_Shadow_Transform_Vertex3f_TexCoord2f(varray_texcoord2f[1], numverts, vertex3f, matrix_modeltoattenuationz);
			R_Mesh_Draw(numverts, numtriangles, elements);
			c_rt_lightmeshes++;
			c_rt_lighttris += numtriangles;

			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(glosstexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.pointer_texcoord[0] = texcoord2f;
			m.pointer_texcoord[1] = lightcubemap ? varray_texcoord3f[1] : NULL;
			R_Mesh_State_Texture(&m);
			qglColorMask(1,1,1,0);
			GL_BlendFunc(GL_DST_ALPHA, GL_ONE);
			if (lightcubemap)
				R_Shadow_Transform_Vertex3f_TexCoord3f(varray_texcoord3f[1], numverts, vertex3f, matrix_modeltofilter);
			VectorScale(lightcolor, colorscale, color2);
			for (renders = 0;renders < 64 && (color2[0] > 0 || color2[1] > 0 || color2[2] > 0);renders++, color2[0]--, color2[1]--, color2[2]--)
			{
				color[0] = bound(0, color2[0], 1);
				color[1] = bound(0, color2[1], 1);
				color[2] = bound(0, color2[2], 1);
				GL_Color(color[0], color[1], color[2], 1);
				R_Mesh_Draw(numverts, numtriangles, elements);
				c_rt_lightmeshes++;
				c_rt_lighttris += numtriangles;
			}
		}
	}
}

void R_Shadow_DrawStaticWorldLight_Shadow(worldlight_t *light, matrix4x4_t *matrix)
{
	R_Mesh_Matrix(matrix);
	if (r_shadow_showtris.integer)
	{
		shadowmesh_t *mesh;
		rmeshstate_t m;
		int depthenabled = qglIsEnabled(GL_DEPTH_TEST);
		int stencilenabled = qglIsEnabled(GL_STENCIL_TEST);
		qglDisable(GL_DEPTH_TEST);
		qglDisable(GL_STENCIL_TEST);
		//qglDisable(GL_CULL_FACE);
		qglColorMask(1,1,1,1);
		memset(&m, 0, sizeof(m));
		R_Mesh_State_Texture(&m);
		GL_Color(0,0.1,0,1);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		for (mesh = light->meshchain_shadow;mesh;mesh = mesh->next)
		{
			GL_VertexPointer(mesh->vertex3f);
			R_Mesh_Draw_ShowTris(mesh->numverts, mesh->numtriangles, mesh->element3i);
		}
		//qglEnable(GL_CULL_FACE);
		if (depthenabled)
			qglEnable(GL_DEPTH_TEST);
		if (stencilenabled)
		{
			qglEnable(GL_STENCIL_TEST);
			qglColorMask(0,0,0,0);
		}
	}
	R_Shadow_RenderShadowMeshVolume(light->meshchain_shadow);
}

void R_Shadow_DrawStaticWorldLight_Light(worldlight_t *light, matrix4x4_t *matrix, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz)
{
	shadowmesh_t *mesh;
	R_Mesh_Matrix(matrix);
	if (r_shadow_showtris.integer)
	{
		rmeshstate_t m;
		int depthenabled = qglIsEnabled(GL_DEPTH_TEST);
		int stencilenabled = qglIsEnabled(GL_STENCIL_TEST);
		qglDisable(GL_DEPTH_TEST);
		qglDisable(GL_STENCIL_TEST);
		//qglDisable(GL_CULL_FACE);
		memset(&m, 0, sizeof(m));
		R_Mesh_State_Texture(&m);
		GL_Color(0.2,0,0,1);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		for (mesh = light->meshchain_light;mesh;mesh = mesh->next)
		{
			GL_VertexPointer(mesh->vertex3f);
			R_Mesh_Draw_ShowTris(mesh->numverts, mesh->numtriangles, mesh->element3i);
		}
		//qglEnable(GL_CULL_FACE);
		if (depthenabled)
			qglEnable(GL_DEPTH_TEST);
		if (stencilenabled)
			qglEnable(GL_STENCIL_TEST);
	}
	for (mesh = light->meshchain_light;mesh;mesh = mesh->next)
	{
		R_Shadow_DiffuseLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoord2f, relativelightorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, mesh->map_diffuse, mesh->map_normal, NULL);
		R_Shadow_SpecularLighting(mesh->numverts, mesh->numtriangles, mesh->element3i, mesh->vertex3f, mesh->svector3f, mesh->tvector3f, mesh->normal3f, mesh->texcoord2f, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, mesh->map_specular, mesh->map_normal, NULL);
	}
}

cvar_t r_editlights = {0, "r_editlights", "0"};
cvar_t r_editlights_cursordistance = {0, "r_editlights_distance", "1024"};
cvar_t r_editlights_cursorpushback = {0, "r_editlights_pushback", "0"};
cvar_t r_editlights_cursorpushoff = {0, "r_editlights_pushoff", "4"};
cvar_t r_editlights_cursorgrid = {0, "r_editlights_grid", "4"};
cvar_t r_editlights_quakelightsizescale = {CVAR_SAVE, "r_editlights_quakelightsizescale", "0.8"};
cvar_t r_editlights_rtlightssizescale = {CVAR_SAVE, "r_editlights_rtlightssizescale", "0.7"};
cvar_t r_editlights_rtlightscolorscale = {CVAR_SAVE, "r_editlights_rtlightscolorscale", "2"};
worldlight_t *r_shadow_worldlightchain;
worldlight_t *r_shadow_selectedlight;
vec3_t r_editlights_cursorlocation;

static int lightpvsbytes;
static qbyte lightpvs[(MAX_MAP_LEAFS + 7)/ 8];

void R_Shadow_NewWorldLight(vec3_t origin, float radius, vec3_t color, int style, const char *cubemapname, int castshadow)
{
	int i, j, k, l, maxverts = 256, tris;
	float *vertex3f = NULL, mins[3], maxs[3];
	worldlight_t *e;
	shadowmesh_t *mesh, *castmesh = NULL;

	if (radius < 15 || DotProduct(color, color) < 0.03)
	{
		Con_Printf("R_Shadow_NewWorldLight: refusing to create a light too small/dim\n");
		return;
	}

	e = Mem_Alloc(r_shadow_mempool, sizeof(worldlight_t));
	VectorCopy(origin, e->origin);
	VectorCopy(color, e->light);
	e->lightradius = radius;
	e->style = style;
	if (e->style < 0 || e->style >= MAX_LIGHTSTYLES)
	{
		Con_Printf("R_Shadow_NewWorldLight: invalid light style number %i, must be >= 0 and < %i\n", e->style, MAX_LIGHTSTYLES);
		e->style = 0;
	}
	e->castshadows = castshadow;

	e->cullradius = e->lightradius;
	for (k = 0;k < 3;k++)
	{
		mins[k] = e->origin[k] - e->lightradius;
		maxs[k] = e->origin[k] + e->lightradius;
	}

	e->next = r_shadow_worldlightchain;
	r_shadow_worldlightchain = e;
	if (cubemapname && cubemapname[0])
	{
		e->cubemapname = Mem_Alloc(r_shadow_mempool, strlen(cubemapname) + 1);
		strcpy(e->cubemapname, cubemapname);
		// FIXME: add cubemap loading (and don't load a cubemap twice)
	}
	// FIXME: rewrite this to store ALL geometry into a cache in the light
	if (e->castshadows)
		castmesh = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768, 32768, NULL, NULL, NULL, false, false, true);
	e->meshchain_light = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768, 32768, NULL, NULL, NULL, true, false, true);
	if (cl.worldmodel)
	{
		if (cl.worldmodel->brushq3.num_leafs)
		{
			q3mleaf_t *leaf;
			q3mface_t *face;
			lightpvsbytes = cl.worldmodel->brush.FatPVS(cl.worldmodel, origin, 0, lightpvs, sizeof(lightpvs));
			VectorCopy(e->origin, e->mins);
			VectorCopy(e->origin, e->maxs);
			for (i = 0, face = cl.worldmodel->brushq3.data_thismodel->firstface;i < cl.worldmodel->brushq3.data_thismodel->numfaces;i++, face++)
				face->lighttemp_castshadow = false;
			for (i = 0, leaf = cl.worldmodel->brushq3.data_leafs;i < cl.worldmodel->brushq3.num_leafs;i++, leaf++)
			{
				if ((leaf->clusterindex < 0 || lightpvs[leaf->clusterindex >> 3] & (1 << (leaf->clusterindex & 7))) && BoxesOverlap(leaf->mins, leaf->maxs, mins, maxs))
				{
					for (k = 0;k < 3;k++)
					{
						if (e->mins[k] > leaf->mins[k]) e->mins[k] = leaf->mins[k];
						if (e->maxs[k] < leaf->maxs[k]) e->maxs[k] = leaf->maxs[k];
					}
					for (j = 0;j < leaf->numleaffaces;j++)
					{
						face = leaf->firstleafface[j];
						if (BoxesOverlap(face->mins, face->maxs, mins, maxs))
							face->lighttemp_castshadow = true;
					}
				}
			}

			// add surfaces to shadow casting mesh and light mesh
			for (i = 0, face = cl.worldmodel->brushq3.data_thismodel->firstface;i < cl.worldmodel->brushq3.data_thismodel->numfaces;i++, face++)
			{
				if (face->lighttemp_castshadow)
				{
					face->lighttemp_castshadow = false;
					if (!(face->texture->surfaceparms & (SURFACEPARM_NODRAW | SURFACEPARM_SKY)))
					{
						if (e->castshadows)
							if (!(face->texture->nativecontents & CONTENTSQ3_TRANSLUCENT))
								Mod_ShadowMesh_AddMesh(r_shadow_mempool, castmesh, NULL, NULL, NULL, face->data_vertex3f, NULL, NULL, NULL, NULL, face->num_triangles, face->data_element3i);
						if (!(face->texture->surfaceparms & SURFACEPARM_SKY))
							Mod_ShadowMesh_AddMesh(r_shadow_mempool, e->meshchain_light, face->texture->skin.base, face->texture->skin.gloss, face->texture->skin.nmap, face->data_vertex3f, face->data_svector3f, face->data_tvector3f, face->data_normal3f, face->data_texcoordtexture2f, face->num_triangles, face->data_element3i);
					}
				}
			}
		}
		else if (cl.worldmodel->brushq1.numleafs)
		{
			mleaf_t *leaf;
			msurface_t *surf;
			VectorCopy(e->origin, e->mins);
			VectorCopy(e->origin, e->maxs);
			i = CL_PointQ1Contents(e->origin);

			for (i = 0, surf = cl.worldmodel->brushq1.surfaces + cl.worldmodel->brushq1.firstmodelsurface;i < cl.worldmodel->brushq1.nummodelsurfaces;i++, surf++)
				surf->lighttemp_castshadow = false;

			if (r_shadow_portallight.integer && i != CONTENTS_SOLID && i != CONTENTS_SKY)
			{
				qbyte *byteleafpvs;
				qbyte *bytesurfacepvs;

				byteleafpvs = Mem_Alloc(tempmempool, cl.worldmodel->brushq1.numleafs);
				bytesurfacepvs = Mem_Alloc(tempmempool, cl.worldmodel->brushq1.numsurfaces);

				Portal_Visibility(cl.worldmodel, e->origin, byteleafpvs, bytesurfacepvs, NULL, 0, true, mins, maxs, e->mins, e->maxs);

				for (i = 0, leaf = cl.worldmodel->brushq1.leafs;i < cl.worldmodel->brushq1.numleafs;i++, leaf++)
				{
					if (byteleafpvs[i] && BoxesOverlap(leaf->mins, leaf->maxs, mins, maxs))
					{
						for (k = 0;k < 3;k++)
						{
							if (e->mins[k] > leaf->mins[k]) e->mins[k] = leaf->mins[k];
							if (e->maxs[k] < leaf->maxs[k]) e->maxs[k] = leaf->maxs[k];
						}
					}
				}

				for (i = 0, surf = cl.worldmodel->brushq1.surfaces;i < cl.worldmodel->brushq1.numsurfaces;i++, surf++)
					if (bytesurfacepvs[i] && BoxesOverlap(surf->poly_mins, surf->poly_maxs, mins, maxs))
						surf->lighttemp_castshadow = true;

				Mem_Free(byteleafpvs);
				Mem_Free(bytesurfacepvs);
			}
			else
			{
				lightpvsbytes = cl.worldmodel->brush.FatPVS(cl.worldmodel, origin, 0, lightpvs, sizeof(lightpvs));
				for (i = 0, leaf = cl.worldmodel->brushq1.leafs + 1;i < cl.worldmodel->brushq1.visleafs;i++, leaf++)
				{
					if (lightpvs[i >> 3] & (1 << (i & 7)) && BoxesOverlap(leaf->mins, leaf->maxs, mins, maxs))
					{
						for (k = 0;k < 3;k++)
						{
							if (e->mins[k] > leaf->mins[k]) e->mins[k] = leaf->mins[k];
							if (e->maxs[k] < leaf->maxs[k]) e->maxs[k] = leaf->maxs[k];
						}
						for (j = 0;j < leaf->nummarksurfaces;j++)
						{
							surf = cl.worldmodel->brushq1.surfaces + leaf->firstmarksurface[j];
							if (!surf->lighttemp_castshadow && BoxesOverlap(surf->poly_mins, surf->poly_maxs, mins, maxs))
								surf->lighttemp_castshadow = true;
						}
					}
				}
			}

			// add surfaces to shadow casting mesh and light mesh
			for (i = 0, surf = cl.worldmodel->brushq1.surfaces + cl.worldmodel->brushq1.firstmodelsurface;i < cl.worldmodel->brushq1.nummodelsurfaces;i++, surf++)
			{
				if (surf->lighttemp_castshadow)
				{
					surf->lighttemp_castshadow = false;
					if (e->castshadows && (surf->flags & SURF_SHADOWCAST))
						Mod_ShadowMesh_AddMesh(r_shadow_mempool, castmesh, NULL, NULL, NULL, surf->mesh.data_vertex3f, NULL, NULL, NULL, NULL, surf->mesh.num_triangles, surf->mesh.data_element3i);
					if (!(surf->flags & SURF_DRAWSKY))
						Mod_ShadowMesh_AddMesh(r_shadow_mempool, e->meshchain_light, surf->texinfo->texture->skin.base, surf->texinfo->texture->skin.gloss, surf->texinfo->texture->skin.nmap, surf->mesh.data_vertex3f, surf->mesh.data_svector3f, surf->mesh.data_tvector3f, surf->mesh.data_normal3f, surf->mesh.data_texcoordtexture2f, surf->mesh.num_triangles, surf->mesh.data_element3i);
				}
			}
		}
	}

	// limit box to light bounds (in case it grew larger)
	for (k = 0;k < 3;k++)
	{
		if (e->mins[k] < e->origin[k] - e->lightradius) e->mins[k] = e->origin[k] - e->lightradius;
		if (e->maxs[k] > e->origin[k] + e->lightradius) e->maxs[k] = e->origin[k] + e->lightradius;
	}
	e->cullradius = RadiusFromBoundsAndOrigin(e->mins, e->maxs, e->origin);

	// cast shadow volume from castmesh
	castmesh = Mod_ShadowMesh_Finish(r_shadow_mempool, castmesh, false, true);
	if (castmesh)
	{
		maxverts = 0;
		for (mesh = castmesh;mesh;mesh = mesh->next)
		{
			R_Shadow_ResizeShadowElements(mesh->numtriangles);
			maxverts = max(maxverts, mesh->numverts * 2);
		}

		if (maxverts > 0)
		{
			vertex3f = Mem_Alloc(r_shadow_mempool, maxverts * sizeof(float[3]));
			// now that we have the buffers big enough, construct and add
			// the shadow volume mesh
			if (e->castshadows)
				e->meshchain_shadow = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768, 32768, NULL, NULL, NULL, false, false, true);
			for (mesh = castmesh;mesh;mesh = mesh->next)
			{
				Mod_BuildTriangleNeighbors(mesh->neighbor3i, mesh->element3i, mesh->numtriangles);
				if ((tris = R_Shadow_ConstructShadowVolume(castmesh->numverts, 0, castmesh->numtriangles, castmesh->element3i, castmesh->neighbor3i, castmesh->vertex3f, NULL, shadowelements, vertex3f, e->origin, r_shadow_projectdistance.value)))
					Mod_ShadowMesh_AddMesh(r_shadow_mempool, e->meshchain_shadow, NULL, NULL, NULL, vertex3f, NULL, NULL, NULL, NULL, tris, shadowelements);
			}
			Mem_Free(vertex3f);
			vertex3f = NULL;
		}
		// we're done with castmesh now
		Mod_ShadowMesh_Free(castmesh);
	}

	e->meshchain_shadow = Mod_ShadowMesh_Finish(r_shadow_mempool, e->meshchain_shadow, false, false);
	e->meshchain_light = Mod_ShadowMesh_Finish(r_shadow_mempool, e->meshchain_light, true, false);

	k = 0;
	if (e->meshchain_shadow)
		for (mesh = e->meshchain_shadow;mesh;mesh = mesh->next)
			k += mesh->numtriangles;
	l = 0;
	if (e->meshchain_light)
		for (mesh = e->meshchain_light;mesh;mesh = mesh->next)
			l += mesh->numtriangles;
	Con_Printf("static light built: %f %f %f : %f %f %f box, %i shadow volume triangles, %i light triangles\n", e->mins[0], e->mins[1], e->mins[2], e->maxs[0], e->maxs[1], e->maxs[2], k, l);
}

void R_Shadow_FreeWorldLight(worldlight_t *light)
{
	worldlight_t **lightpointer;
	for (lightpointer = &r_shadow_worldlightchain;*lightpointer && *lightpointer != light;lightpointer = &(*lightpointer)->next);
	if (*lightpointer != light)
		Sys_Error("R_Shadow_FreeWorldLight: light not linked into chain\n");
	*lightpointer = light->next;
	if (light->cubemapname)
		Mem_Free(light->cubemapname);
	if (light->meshchain_shadow)
		Mod_ShadowMesh_Free(light->meshchain_shadow);
	if (light->meshchain_light)
		Mod_ShadowMesh_Free(light->meshchain_light);
	Mem_Free(light);
}

void R_Shadow_ClearWorldLights(void)
{
	while (r_shadow_worldlightchain)
		R_Shadow_FreeWorldLight(r_shadow_worldlightchain);
	r_shadow_selectedlight = NULL;
}

void R_Shadow_SelectLight(worldlight_t *light)
{
	if (r_shadow_selectedlight)
		r_shadow_selectedlight->selected = false;
	r_shadow_selectedlight = light;
	if (r_shadow_selectedlight)
		r_shadow_selectedlight->selected = true;
}

rtexture_t *lighttextures[5];

void R_Shadow_DrawCursorCallback(const void *calldata1, int calldata2)
{
	float scale = r_editlights_cursorgrid.value * 0.5f;
	R_DrawSprite(GL_SRC_ALPHA, GL_ONE, lighttextures[0], false, r_editlights_cursorlocation, r_viewright, r_viewup, scale, -scale, -scale, scale, 1, 1, 1, 0.5f);
}

void R_Shadow_DrawLightSpriteCallback(const void *calldata1, int calldata2)
{
	float intensity;
	const worldlight_t *light;
	light = calldata1;
	intensity = 0.5;
	if (light->selected)
		intensity = 0.75 + 0.25 * sin(realtime * M_PI * 4.0);
	if (!light->meshchain_shadow)
		intensity *= 0.5f;
	R_DrawSprite(GL_SRC_ALPHA, GL_ONE, lighttextures[calldata2], false, light->origin, r_viewright, r_viewup, 8, -8, -8, 8, intensity, intensity, intensity, 0.5);
}

void R_Shadow_DrawLightSprites(void)
{
	int i;
	cachepic_t *pic;
	worldlight_t *light;

	for (i = 0;i < 5;i++)
	{
		lighttextures[i] = NULL;
		if ((pic = Draw_CachePic(va("gfx/crosshair%i.tga", i + 1))))
			lighttextures[i] = pic->tex;
	}

	for (light = r_shadow_worldlightchain;light;light = light->next)
		R_MeshQueue_AddTransparent(light->origin, R_Shadow_DrawLightSpriteCallback, light, ((int) light) % 5);
	R_MeshQueue_AddTransparent(r_editlights_cursorlocation, R_Shadow_DrawCursorCallback, NULL, 0);
}

void R_Shadow_SelectLightInView(void)
{
	float bestrating, rating, temp[3];
	worldlight_t *best, *light;
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
	float origin[3], radius, color[3];
	if (cl.worldmodel == NULL)
	{
		Con_Printf("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	lightsstring = FS_LoadFile(name, false);
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
			shadow = true;
			// check for modifier flags
			if (*t == '!')
			{
				shadow = false;
				t++;
			}
			a = sscanf(t, "%f %f %f %f %f %f %f %d %s", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname);
			if (a < 9)
				cubemapname[0] = 0;
			*s = '\n';
			if (a < 8)
			{
				Con_Printf("found %d parameters on line %i, should be 8 or 9 parameters (origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style cubemapname)\n", a, n + 1);
				break;
			}
			VectorScale(color, r_editlights_rtlightscolorscale.value, color);
			radius *= r_editlights_rtlightssizescale.value;
			R_Shadow_NewWorldLight(origin, radius, color, style, cubemapname, shadow);
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
	worldlight_t *light;
	int bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[1024];
	if (!r_shadow_worldlightchain)
		return;
	if (cl.worldmodel == NULL)
	{
		Con_Printf("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".rtlights", sizeof (name));
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		sprintf(line, "%s%f %f %f %f %f %f %f %d %s\n", light->castshadows ? "" : "!", light->origin[0], light->origin[1], light->origin[2], light->lightradius / r_editlights_rtlightssizescale.value, light->light[0] / r_editlights_rtlightscolorscale.value, light->light[1] / r_editlights_rtlightscolorscale.value, light->light[2] / r_editlights_rtlightscolorscale.value, light->style, light->cubemapname ? light->cubemapname : "");
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
		Con_Printf("No map loaded.\n");
		return;
	}
	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".lights", sizeof (name));
	lightsstring = FS_LoadFile(name, false);
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
			R_Shadow_NewWorldLight(origin, radius, color, style, NULL, true);
			s++;
			n++;
		}
		if (*s)
			Con_Printf("invalid lights file \"%s\"\n", name);
		Mem_Free(lightsstring);
	}
}

void R_Shadow_LoadWorldLightsFromMap_LightArghliteTyrlite(void)
{
	int entnum, style, islight;
	char key[256], value[1024];
	float origin[3], radius, color[3], light, scale, originhack[3], overridecolor[3];
	const char *data;

	if (cl.worldmodel == NULL)
	{
		Con_Printf("No map loaded.\n");
		return;
	}
	data = cl.worldmodel->brush.entities;
	if (!data)
		return;
	for (entnum = 0;COM_ParseToken(&data, false) && com_token[0] == '{';entnum++)
	{
		light = 0;
		origin[0] = origin[1] = origin[2] = 0;
		originhack[0] = originhack[1] = originhack[2] = 0;
		color[0] = color[1] = color[2] = 1;
		overridecolor[0] = overridecolor[1] = overridecolor[2] = 1;
		scale = 1;
		style = 0;
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
				light = atof(value);
			else if (!strcmp("origin", key))
				sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp("color", key))
				sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
			else if (!strcmp("wait", key))
				scale = atof(value);
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
		}
		if (light <= 0 && islight)
			light = 300;
		radius = min(light * r_editlights_quakelightsizescale.value / scale, 1048576);
		light = sqrt(bound(0, light, 1048576)) * (1.0f / 16.0f);
		if (color[0] == 1 && color[1] == 1 && color[2] == 1)
			VectorCopy(overridecolor, color);
		VectorScale(color, light, color);
		VectorAdd(origin, originhack, origin);
		if (radius >= 15)
			R_Shadow_NewWorldLight(origin, radius, color, style, NULL, true);
	}
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
	r_shadow_reloadlights = true;
}

void R_Shadow_EditLights_Save_f(void)
{
	if (cl.worldmodel)
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
		Con_Printf("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (Cmd_Argc() != 1)
	{
		Con_Printf("r_editlights_spawn does not take parameters\n");
		return;
	}
	color[0] = color[1] = color[2] = 1;
	R_Shadow_NewWorldLight(r_editlights_cursorlocation, 200, color, 0, NULL, true);
}

void R_Shadow_EditLights_Edit_f(void)
{
	vec3_t origin, color;
	vec_t radius;
	int style, shadows;
	char cubemapname[1024];
	if (!r_editlights.integer)
	{
		Con_Printf("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Printf("No selected light.\n");
		return;
	}
	VectorCopy(r_shadow_selectedlight->origin, origin);
	radius = r_shadow_selectedlight->lightradius;
	VectorCopy(r_shadow_selectedlight->light, color);
	style = r_shadow_selectedlight->style;
	if (r_shadow_selectedlight->cubemapname)
		strcpy(cubemapname, r_shadow_selectedlight->cubemapname);
	else
		cubemapname[0] = 0;
	shadows = r_shadow_selectedlight->castshadows;
	if (!strcmp(Cmd_Argv(1), "origin"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s x y z\n", Cmd_Argv(0));
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
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[0] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "originy"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[1] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "originz"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[2] = atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "move"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s x y z\n", Cmd_Argv(0));
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
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[0] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "movey"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[1] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "movez"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		origin[2] += atof(Cmd_Argv(2));
	}
	else if (!strcmp(Cmd_Argv(1), "color"))
	{
		if (Cmd_Argc() != 5)
		{
			Con_Printf("usage: r_editlights_edit %s red green blue\n", Cmd_Argv(0));
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
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		radius = atof(Cmd_Argv(2));
	}
	else if (Cmd_Argc() == 3 && !strcmp(Cmd_Argv(1), "style"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		style = atoi(Cmd_Argv(2));
	}
	else if (Cmd_Argc() == 3 && !strcmp(Cmd_Argv(1), "cubemap"))
	{
		if (Cmd_Argc() > 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		if (Cmd_Argc() == 3)
			strcpy(cubemapname, Cmd_Argv(2));
		else
			cubemapname[0] = 0;
	}
	else if (Cmd_Argc() == 3 && !strcmp(Cmd_Argv(1), "shadows"))
	{
		if (Cmd_Argc() != 3)
		{
			Con_Printf("usage: r_editlights_edit %s value\n", Cmd_Argv(0));
			return;
		}
		shadows = Cmd_Argv(2)[0] == 'y' || Cmd_Argv(2)[0] == 'Y' || Cmd_Argv(2)[0] == 't' || atoi(Cmd_Argv(2));
	}
	else
	{
		Con_Printf("usage: r_editlights_edit [property] [value]\n");
		Con_Printf("Selected light's properties:\n");
		Con_Printf("Origin: %f %f %f\n", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);
		Con_Printf("Radius: %f\n", r_shadow_selectedlight->lightradius);
		Con_Printf("Color: %f %f %f\n", r_shadow_selectedlight->light[0], r_shadow_selectedlight->light[1], r_shadow_selectedlight->light[2]);
		Con_Printf("Style: %i\n", r_shadow_selectedlight->style);
		Con_Printf("Cubemap: %s\n", r_shadow_selectedlight->cubemapname);
		Con_Printf("Shadows: %s\n", r_shadow_selectedlight->castshadows ? "yes" : "no");
		return;
	}
	R_Shadow_FreeWorldLight(r_shadow_selectedlight);
	r_shadow_selectedlight = NULL;
	R_Shadow_NewWorldLight(origin, radius, color, style, cubemapname, shadows);
}

extern int con_vislines;
void R_Shadow_EditLights_DrawSelectedLightProperties(void)
{
	float x, y;
	char temp[256];
	if (r_shadow_selectedlight == NULL)
		return;
	x = 0;
	y = con_vislines;
	sprintf(temp, "Light properties");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Origin %f %f %f", r_shadow_selectedlight->origin[0], r_shadow_selectedlight->origin[1], r_shadow_selectedlight->origin[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Radius %f", r_shadow_selectedlight->lightradius);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Color %f %f %f", r_shadow_selectedlight->light[0], r_shadow_selectedlight->light[1], r_shadow_selectedlight->light[2]);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Style %i", r_shadow_selectedlight->style);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Cubemap %s", r_shadow_selectedlight->cubemapname);DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
	sprintf(temp, "Shadows %s", r_shadow_selectedlight->castshadows ? "yes" : "no");DrawQ_String(x, y, temp, 0, 8, 8, 1, 1, 1, 1, 0);y += 8;
}

void R_Shadow_EditLights_ToggleShadow_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Printf("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Printf("No selected light.\n");
		return;
	}
	R_Shadow_NewWorldLight(r_shadow_selectedlight->origin, r_shadow_selectedlight->lightradius, r_shadow_selectedlight->light, r_shadow_selectedlight->style, r_shadow_selectedlight->cubemapname, !r_shadow_selectedlight->castshadows);
	R_Shadow_FreeWorldLight(r_shadow_selectedlight);
	r_shadow_selectedlight = NULL;
}

void R_Shadow_EditLights_Remove_f(void)
{
	if (!r_editlights.integer)
	{
		Con_Printf("Cannot remove light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (!r_shadow_selectedlight)
	{
		Con_Printf("No selected light.\n");
		return;
	}
	R_Shadow_FreeWorldLight(r_shadow_selectedlight);
	r_shadow_selectedlight = NULL;
}

void R_Shadow_EditLights_Help_f(void)
{
	Con_Printf(
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
"color r g b : set color of light (can be brighter than 1 1 1)\n"
"radius radius : set radius (size) of light\n"
"style style : set lightstyle of light (flickering patterns, switches, etc)\n"
"cubemap basename : set filter cubemap of light (not yet supported)\n"
"shadows 1/0 : turn on/off shadows\n"
"<nothing> : print light properties to console\n"
	);
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
	Cmd_AddCommand("r_editlights_remove", R_Shadow_EditLights_Remove_f);
	Cmd_AddCommand("r_editlights_toggleshadow", R_Shadow_EditLights_ToggleShadow_f);
	Cmd_AddCommand("r_editlights_importlightentitiesfrommap", R_Shadow_EditLights_ImportLightEntitiesFromMap_f);
	Cmd_AddCommand("r_editlights_importlightsfile", R_Shadow_EditLights_ImportLightsFile_f);
}
