
#include "quakedef.h"
#include "r_shadow.h"
#include "cl_collision.h"

extern void R_Shadow_EditLights_Init(void);

#define SHADOWSTAGE_NONE 0
#define SHADOWSTAGE_STENCIL 1
#define SHADOWSTAGE_LIGHT 2
#define SHADOWSTAGE_ERASESTENCIL 3

int r_shadowstage = SHADOWSTAGE_NONE;
int r_shadow_reloadlights = false;

int r_shadow_lightingmode = 0;

mempool_t *r_shadow_mempool;

int maxshadowelements;
int *shadowelements;
int maxtrianglefacinglight;
qbyte *trianglefacinglight;

rtexturepool_t *r_shadow_texturepool;
rtexture_t *r_shadow_normalscubetexture;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_blankbumptexture;
rtexture_t *r_shadow_blankglosstexture;
rtexture_t *r_shadow_blankwhitetexture;

cvar_t r_shadow_lightattenuationpower = {0, "r_shadow_lightattenuationpower", "0.5"};
cvar_t r_shadow_lightattenuationscale = {0, "r_shadow_lightattenuationscale", "1"};
cvar_t r_shadow_lightintensityscale = {0, "r_shadow_lightintensityscale", "1"};
cvar_t r_shadow_realtime = {0, "r_shadow_realtime", "0"};
cvar_t r_shadow_gloss = {0, "r_shadow_gloss", "1"};
cvar_t r_shadow_debuglight = {0, "r_shadow_debuglight", "-1"};
cvar_t r_shadow_scissor = {0, "r_shadow_scissor", "1"};
cvar_t r_shadow_bumpscale_bumpmap = {0, "r_shadow_bumpscale_bumpmap", "4"};
cvar_t r_shadow_bumpscale_basetexture = {0, "r_shadow_bumpscale_basetexture", "0"};
cvar_t r_shadow_shadownudge = {0, "r_shadow_shadownudge", "1"};

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
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	r_shadow_normalscubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
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
	r_shadow_normalscubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	r_shadow_blankglosstexture = NULL;
	r_shadow_blankwhitetexture = NULL;
	R_FreeTexturePool(&r_shadow_texturepool);
	maxshadowelements = 0;
	shadowelements = NULL;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	Mem_FreePool(&r_shadow_mempool);
}

void r_shadow_newmap(void)
{
	R_Shadow_ClearWorldLights();
	r_shadow_reloadlights = true;
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_lightattenuationpower);
	Cvar_RegisterVariable(&r_shadow_lightattenuationscale);
	Cvar_RegisterVariable(&r_shadow_lightintensityscale);
	Cvar_RegisterVariable(&r_shadow_realtime);
	Cvar_RegisterVariable(&r_shadow_gloss);
	Cvar_RegisterVariable(&r_shadow_debuglight);
	Cvar_RegisterVariable(&r_shadow_scissor);
	Cvar_RegisterVariable(&r_shadow_bumpscale_bumpmap);
	Cvar_RegisterVariable(&r_shadow_bumpscale_basetexture);
	Cvar_RegisterVariable(&r_shadow_shadownudge);
	R_Shadow_EditLights_Init();
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

void R_Shadow_ProjectVertices(float *verts, int numverts, const float *relativelightorigin, float projectdistance)
{
	int i;
	float *in, *out, diff[4];
	in = verts;
	out = verts + numverts * 4;
	for (i = 0;i < numverts;i++, in += 4, out += 4)
	{
		VectorSubtract(in, relativelightorigin, diff);
		VectorNormalizeFast(diff);
		VectorMA(in, projectdistance, diff, out);
		VectorMA(in, r_shadow_shadownudge.value, diff, in);
	}
}

void R_Shadow_MakeTriangleShadowFlags(const int *elements, const float *vertex, int numtris, qbyte *trianglefacinglight, const float *relativelightorigin, float lightradius)
{
	int i;
	const float *v0, *v1, *v2;
	for (i = 0;i < numtris;i++, elements += 3)
	{
		// calculate triangle facing flag
		v0 = vertex + elements[0] * 4;
		v1 = vertex + elements[1] * 4;
		v2 = vertex + elements[2] * 4;
		// we do not need to normalize the surface normal because both sides
		// of the comparison use it, therefore they are both multiplied the
		// same amount...  furthermore the subtract can be done on the
		// vectors, saving a little bit of math in the dotproducts
#if 1
		// fast version
		// subtracts v1 from v0 and v2, combined into a crossproduct,
		// combined with a dotproduct of the light location relative to the
		// first point of the triangle (any point works, since the triangle
		// is obviously flat), and finally a comparison to determine if the
		// light is infront of the triangle (the goal of this statement)
		trianglefacinglight[i] =
		   (relativelightorigin[0] - v0[0]) * ((v0[1] - v1[1]) * (v2[2] - v1[2]) - (v0[2] - v1[2]) * (v2[1] - v1[1]))
		 + (relativelightorigin[1] - v0[1]) * ((v0[2] - v1[2]) * (v2[0] - v1[0]) - (v0[0] - v1[0]) * (v2[2] - v1[2]))
		 + (relativelightorigin[2] - v0[2]) * ((v0[0] - v1[0]) * (v2[1] - v1[1]) - (v0[1] - v1[1]) * (v2[0] - v1[0])) > 0;
#else
		// readable version
		{
		float dir0[3], dir1[3], temp[3];

		// calculate two mostly perpendicular edge directions
		VectorSubtract(v0, v1, dir0);
		VectorSubtract(v2, v1, dir1);

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal (it's magnitude
		// is not 1 however)
		CrossProduct(dir0, dir1, temp);

		// this is entirely unnecessary, but kept for clarity
		//VectorNormalize(temp);

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		// the normal is not normalized because it is used on both sides of
		// the comparison, so it's magnitude does not matter
		trianglefacinglight[i] = DotProduct(relativelightorigin, temp) >= DotProduct(v0, temp);
		}
#endif
	}
}

int R_Shadow_BuildShadowVolumeTriangles(const int *elements, const int *neighbors, int numtris, int numverts, const qbyte *trianglefacinglight, int *out)
{
	int i, tris;
	// check each frontface for bordering backfaces,
	// and cast shadow polygons from those edges,
	// also create front and back caps for shadow volume
	tris = 0;
	for (i = 0;i < numtris;i++, elements += 3, neighbors += 3)
	{
		if (trianglefacinglight[i])
		{
			// triangle is frontface and therefore casts shadow,
			// output front and back caps for shadow volume
			// front cap
			out[0] = elements[0];
			out[1] = elements[1];
			out[2] = elements[2];
			// rear cap (with flipped winding order)
			out[3] = elements[0] + numverts;
			out[4] = elements[2] + numverts;
			out[5] = elements[1] + numverts;
			out += 6;
			tris += 2;
			// check the edges
			if (neighbors[0] < 0 || !trianglefacinglight[neighbors[0]])
			{
				out[0] = elements[1];
				out[1] = elements[0];
				out[2] = elements[0] + numverts;
				out[3] = elements[1];
				out[4] = elements[0] + numverts;
				out[5] = elements[1] + numverts;
				out += 6;
				tris += 2;
			}
			if (neighbors[1] < 0 || !trianglefacinglight[neighbors[1]])
			{
				out[0] = elements[2];
				out[1] = elements[1];
				out[2] = elements[1] + numverts;
				out[3] = elements[2];
				out[4] = elements[1] + numverts;
				out[5] = elements[2] + numverts;
				out += 6;
				tris += 2;
			}
			if (neighbors[2] < 0 || !trianglefacinglight[neighbors[2]])
			{
				out[0] = elements[0];
				out[1] = elements[2];
				out[2] = elements[2] + numverts;
				out[3] = elements[0];
				out[4] = elements[2] + numverts;
				out[5] = elements[0] + numverts;
				out += 6;
				tris += 2;
			}
		}
	}
	return tris;
}

void R_Shadow_ResizeTriangleFacingLight(int numtris)
{
	// make sure trianglefacinglight is big enough for this volume
	if (maxtrianglefacinglight < numtris)
	{
		maxtrianglefacinglight = numtris;
		if (trianglefacinglight)
			Mem_Free(trianglefacinglight);
		trianglefacinglight = Mem_Alloc(r_shadow_mempool, maxtrianglefacinglight);
	}
}

void R_Shadow_ResizeShadowElements(int numtris)
{
	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
	{
		maxshadowelements = numtris * 24;
		if (shadowelements)
			Mem_Free(shadowelements);
		shadowelements = Mem_Alloc(r_shadow_mempool, maxshadowelements * sizeof(int));
	}
}

void R_Shadow_Volume(int numverts, int numtris, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance)
{
	int tris;
	if (projectdistance < 0.1)
	{
		Con_Printf("R_Shadow_Volume: projectdistance %f\n");
		return;
	}
// terminology:
//
// frontface:
// a triangle facing the light source
//
// backface:
// a triangle not facing the light source
//
// shadow volume:
// an extrusion of the frontfaces, beginning at the original geometry and
// ending further from the light source than the original geometry
// (presumably at least as far as the light's radius, if the light has a
// radius at all), capped at both front and back to avoid any problems
//
// description:
// draws the shadow volumes of the model.
// requirements:
// vertex locations must already be in varray_vertex before use.
// varray_vertex must have capacity for numverts * 2.

	// make sure trianglefacinglight is big enough for this volume
	if (maxtrianglefacinglight < numtris)
		R_Shadow_ResizeTriangleFacingLight(numtris);

	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
		R_Shadow_ResizeShadowElements(numtris);

	// check which triangles are facing the light
	R_Shadow_MakeTriangleShadowFlags(elements, varray_vertex, numtris, trianglefacinglight, relativelightorigin, lightradius);

	// generate projected vertices
	// by clever use of elements we'll construct the whole shadow from
	// the unprojected vertices and these projected vertices
	R_Shadow_ProjectVertices(varray_vertex, numverts, relativelightorigin, projectdistance);

	// output triangle elements
	tris = R_Shadow_BuildShadowVolumeTriangles(elements, neighbors, numtris, numverts, trianglefacinglight, shadowelements);
	R_Shadow_RenderVolume(numverts * 2, tris, shadowelements);
}

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements)
{
	if (!numverts || !numtris)
		return;
	if (r_shadowstage == SHADOWSTAGE_STENCIL)
	{
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		R_Mesh_Draw(numverts, numtris, elements);
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	}
	R_Mesh_Draw(numverts, numtris, elements);
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
			R_Mesh_ResizeCheck(mesh->numverts);
			memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->elements);
		}
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	}
	for (mesh = firstmesh;mesh;mesh = mesh->next)
	{
		R_Mesh_ResizeCheck(mesh->numverts);
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->elements);
	}
}

float r_shadow_attenpower, r_shadow_attenscale;
static void R_Shadow_MakeTextures(void)
{
	int x, y, d, side;
	float v[3], s, t, intensity;
	qbyte *data;
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_attenpower = r_shadow_lightattenuationpower.value;
	r_shadow_attenscale = r_shadow_lightattenuationscale.value;
	data = Mem_Alloc(tempmempool, 6*128*128*4);
	data[0] = 128;
	data[1] = 128;
	data[2] = 255;
	data[3] = 255;
	r_shadow_blankbumptexture = R_LoadTexture2D(r_shadow_texturepool, "blankbump", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 64;
	data[1] = 64;
	data[2] = 64;
	data[3] = 255;
	r_shadow_blankglosstexture = R_LoadTexture2D(r_shadow_texturepool, "blankgloss", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_shadow_blankwhitetexture = R_LoadTexture2D(r_shadow_texturepool, "blankwhite", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	for (side = 0;side < 6;side++)
	{
		for (y = 0;y < 128;y++)
		{
			for (x = 0;x < 128;x++)
			{
				s = (x + 0.5f) * (2.0f / 128.0f) - 1.0f;
				t = (y + 0.5f) * (2.0f / 128.0f) - 1.0f;
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
				data[((side*128+y)*128+x)*4+0] = 128.0f + intensity * v[0];
				data[((side*128+y)*128+x)*4+1] = 128.0f + intensity * v[1];
				data[((side*128+y)*128+x)*4+2] = 128.0f + intensity * v[2];
				data[((side*128+y)*128+x)*4+3] = 255;
			}
		}
	}
	r_shadow_normalscubetexture = R_LoadTextureCubeMap(r_shadow_texturepool, "normalscube", 128, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
	for (y = 0;y < 128;y++)
	{
		for (x = 0;x < 128;x++)
		{
			v[0] = (x + 0.5f) * (2.0f / (128.0f - 8.0f)) - 1.0f;
			v[1] = (y + 0.5f) * (2.0f / (128.0f - 8.0f)) - 1.0f;
			v[2] = 0;
			intensity = 1.0f - sqrt(DotProduct(v, v));
			if (intensity > 0)
				intensity = pow(intensity, r_shadow_attenpower);
			intensity = bound(0, intensity * r_shadow_attenscale * 256.0f, 255.0f);
			d = bound(0, intensity, 255);
			data[((0*128+y)*128+x)*4+0] = d;
			data[((0*128+y)*128+x)*4+1] = d;
			data[((0*128+y)*128+x)*4+2] = d;
			data[((0*128+y)*128+x)*4+3] = d;
		}
	}
	r_shadow_attenuation2dtexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation2d", 128, 128, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALPHA, NULL);
	Mem_Free(data);
}

void R_Shadow_Stage_Begin(void)
{
	rmeshstate_t m;

	//cl.worldmodel->numlights = min(cl.worldmodel->numlights, 1);
	if (!r_shadow_attenuation2dtexture
	 || r_shadow_lightattenuationpower.value != r_shadow_attenpower
	 || r_shadow_lightattenuationscale.value != r_shadow_attenscale)
		R_Shadow_MakeTextures();
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

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	R_Mesh_State(&m);
	GL_Color(0, 0, 0, 1);
	r_shadowstage = SHADOWSTAGE_NONE;
}

void R_Shadow_Stage_ShadowVolumes(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_TextureState(&m);
	GL_Color(1, 1, 1, 1);
	qglColorMask(0, 0, 0, 0);
	qglDisable(GL_BLEND);
	qglDepthMask(0);
	qglDepthFunc(GL_LESS);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 128, 0xFF);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_DEPTH_TEST);
	r_shadowstage = SHADOWSTAGE_STENCIL;
	qglClear(GL_STENCIL_BUFFER_BIT);
}

void R_Shadow_Stage_Light(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_TextureState(&m);
	qglActiveTexture(GL_TEXTURE0_ARB);

	qglEnable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthMask(0);
	qglDepthFunc(GL_EQUAL);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw light where this geometry was already rendered AND the
	// stencil is 128 (values other than this mean shadow)
	qglStencilFunc(GL_EQUAL, 128, 0xFF);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_DEPTH_TEST);
	r_shadowstage = SHADOWSTAGE_LIGHT;
}

void R_Shadow_Stage_End(void)
{
	rmeshstate_t m;
	// attempt to restore state to what Mesh_State thinks it is
	qglDisable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ZERO);
	qglDepthMask(1);
	// now restore the rest of the state to normal
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDisable(GL_SCISSOR_TEST);
	qglDepthFunc(GL_LEQUAL);
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 128, 0xFF);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_DEPTH_TEST);
	// force mesh state to reset by using various combinations of features
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	R_Mesh_State(&m);
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	R_Mesh_State(&m);
	r_shadowstage = SHADOWSTAGE_NONE;
}

int R_Shadow_ScissorForBBoxAndSphere(const float *mins, const float *maxs, const float *origin, float radius)
{
	int i, ix1, iy1, ix2, iy2;
	float x1, y1, x2, y2, x, y;
	vec3_t smins, smaxs;
	vec4_t v, v2;
	if (!r_shadow_scissor.integer)
		return false;
	// if view is inside the box, just say yes it's visible
	if (r_origin[0] >= mins[0] && r_origin[0] <= maxs[0]
	 && r_origin[1] >= mins[1] && r_origin[1] <= maxs[1]
	 && r_origin[2] >= mins[2] && r_origin[2] <= maxs[2])
	{
		qglDisable(GL_SCISSOR_TEST);
		return false;
	}
	VectorSubtract(r_origin, origin, v);
	if (DotProduct(v, v) < radius * radius)
	{
		qglDisable(GL_SCISSOR_TEST);
		return false;
	}
	// create viewspace bbox
	for (i = 0;i < 8;i++)
	{
		v[0] = ((i & 1) ? mins[0] : maxs[0]) - r_origin[0];
		v[1] = ((i & 2) ? mins[1] : maxs[1]) - r_origin[1];
		v[2] = ((i & 4) ? mins[2] : maxs[2]) - r_origin[2];
		v2[0] = DotProduct(v, vright);
		v2[1] = DotProduct(v, vup);
		v2[2] = DotProduct(v, vpn);
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
	// clip it to the viewspace version of the sphere
	v[0] = origin[0] - r_origin[0];
	v[1] = origin[1] - r_origin[1];
	v[2] = origin[2] - r_origin[2];
	v2[0] = DotProduct(v, vright);
	v2[1] = DotProduct(v, vup);
	v2[2] = DotProduct(v, vpn);
	if (smins[0] < v2[0] - radius) smins[0] = v2[0] - radius;
	if (smaxs[0] < v2[0] - radius) smaxs[0] = v2[0] + radius;
	if (smins[1] < v2[1] - radius) smins[1] = v2[1] - radius;
	if (smaxs[1] < v2[1] - radius) smaxs[1] = v2[1] + radius;
	if (smins[2] < v2[2] - radius) smins[2] = v2[2] - radius;
	if (smaxs[2] < v2[2] - radius) smaxs[2] = v2[2] + radius;
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
		v[0] = v2[0] * vright[0] + v2[1] * vup[0] + v2[2] * vpn[0] + r_origin[0];
		v[1] = v2[0] * vright[1] + v2[1] * vup[1] + v2[2] * vpn[1] + r_origin[1];
		v[2] = v2[0] * vright[2] + v2[1] * vup[2] + v2[2] * vpn[2] + r_origin[2];
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
	return false;
}

void R_Shadow_GenTexCoords_Attenuation2D1D(float *out2d, float *out1d, int numverts, const float *vertex, const float *svectors, const float *tvectors, const float *normals, const vec3_t relativelightorigin, float lightradius)
{
	int i;
	float lightvec[3], iradius;
	iradius = 0.5f / lightradius;
	for (i = 0;i < numverts;i++, vertex += 4, svectors += 4, tvectors += 4, normals += 4, out2d += 4, out1d += 4)
	{
		VectorSubtract(vertex, relativelightorigin, lightvec);
		out2d[0] = 0.5f + DotProduct(svectors, lightvec) * iradius;
		out2d[1] = 0.5f + DotProduct(tvectors, lightvec) * iradius;
		out2d[2] = 0;
		out1d[0] = 0.5f + DotProduct(normals, lightvec) * iradius;
		out1d[1] = 0.5f;
		out1d[2] = 0;
	}
}

void R_Shadow_GenTexCoords_Diffuse_Attenuation3D(float *out, int numverts, const float *vertex, const float *svectors, const float *tvectors, const float *normals, const vec3_t relativelightorigin, float lightradius)
{
	int i;
	float lightvec[3], iradius;
	iradius = 0.5f / lightradius;
	for (i = 0;i < numverts;i++, vertex += 4, svectors += 4, tvectors += 4, normals += 4, out += 4)
	{
		VectorSubtract(vertex, relativelightorigin, lightvec);
		out[0] = 0.5f + DotProduct(svectors, lightvec) * iradius;
		out[1] = 0.5f + DotProduct(tvectors, lightvec) * iradius;
		out[2] = 0.5f + DotProduct(normals, lightvec) * iradius;
	}
}

void R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(float *out, int numverts, const float *vertex, const float *svectors, const float *tvectors, const float *normals, const vec3_t relativelightorigin)
{
	int i;
	float lightdir[3];
	for (i = 0;i < numverts;i++, vertex += 4, svectors += 4, tvectors += 4, normals += 4, out += 4)
	{
		VectorSubtract(vertex, relativelightorigin, lightdir);
		// the cubemap normalizes this for us
		out[0] = DotProduct(svectors, lightdir);
		out[1] = DotProduct(tvectors, lightdir);
		out[2] = DotProduct(normals, lightdir);
	}
}

void R_Shadow_GenTexCoords_Specular_Attenuation3D(float *out, int numverts, const float *vertex, const float *svectors, const float *tvectors, const float *normals, const vec3_t relativelightorigin, const vec3_t relativeeyeorigin, float lightradius)
{
	int i;
	float lightdir[3], eyedir[3], halfdir[3], lightdirlen, iradius;
	iradius = 0.5f / lightradius;
	for (i = 0;i < numverts;i++, vertex += 4, svectors += 4, tvectors += 4, normals += 4, out += 4)
	{
		VectorSubtract(vertex, relativelightorigin, lightdir);
		// this is used later to make the attenuation correct
		lightdirlen = sqrt(DotProduct(lightdir, lightdir)) * iradius;
		VectorNormalizeFast(lightdir);
		VectorSubtract(vertex, relativeeyeorigin, eyedir);
		VectorNormalizeFast(eyedir);
		VectorAdd(lightdir, eyedir, halfdir);
		VectorNormalizeFast(halfdir);
		out[0] = 0.5f + DotProduct(svectors, halfdir) * lightdirlen;
		out[1] = 0.5f + DotProduct(tvectors, halfdir) * lightdirlen;
		out[2] = 0.5f + DotProduct(normals, halfdir) * lightdirlen;
	}
}

void R_Shadow_GenTexCoords_Specular_NormalCubeMap(float *out, int numverts, const float *vertex, const float *svectors, const float *tvectors, const float *normals, const vec3_t relativelightorigin, const vec3_t relativeeyeorigin)
{
	int i;
	float lightdir[3], eyedir[3], halfdir[3];
	for (i = 0;i < numverts;i++, vertex += 4, svectors += 4, tvectors += 4, normals += 4, out += 4)
	{
		VectorSubtract(vertex, relativelightorigin, lightdir);
		VectorNormalizeFast(lightdir);
		VectorSubtract(vertex, relativeeyeorigin, eyedir);
		VectorNormalizeFast(eyedir);
		VectorAdd(lightdir, eyedir, halfdir);
		// the cubemap normalizes this for us
		out[0] = DotProduct(svectors, halfdir);
		out[1] = DotProduct(tvectors, halfdir);
		out[2] = DotProduct(normals, halfdir);
	}
}

void R_Shadow_GenTexCoords_LightCubeMap(float *out, int numverts, const float *vertex, const vec3_t relativelightorigin)
{
	int i;
	// FIXME: this needs to be written
	// this code assumes the vertices are in worldspace (a false assumption)
	for (i = 0;i < numverts;i++, vertex += 4, out += 4)
		VectorSubtract(vertex, relativelightorigin, out);
}

void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, float lightradius, const float *lightcolor, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int renders;
	float color[3];
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	if (!bumptexture)
		bumptexture = r_shadow_blankbumptexture;
	// colorscale accounts for how much we multiply the brightness during combine
	// mult is how many times the final pass of the lighting will be
	// performed to get more brightness than otherwise possible
	// limit mult to 64 for sanity sake
	if (r_textureunits.integer >= 4)
	{
		// 4 texture no3D combine path, two pass
		m.tex[0] = R_GetTexture(bumptexture);
		m.texcubemap[1] = R_GetTexture(r_shadow_normalscubetexture);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
		m.tex[2] = R_GetTexture(r_shadow_attenuation2dtexture);
		m.tex[3] = R_GetTexture(r_shadow_attenuation2dtexture);
		R_Mesh_TextureState(&m);
		qglColorMask(0,0,0,1);
		qglDisable(GL_BLEND);
		GL_Color(1,1,1,1);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin);
		R_Shadow_GenTexCoords_Attenuation2D1D(varray_texcoord[2], varray_texcoord[3], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(basetexture);
		m.texcubemap[1] = R_GetTexture(lightcubemap);
		m.texcombinergb[0] = GL_MODULATE;
		m.texcombinergb[1] = GL_MODULATE;
		m.tex[2] = 0;
		m.tex[3] = 0;
		R_Mesh_TextureState(&m);
		qglColorMask(1,1,1,0);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		qglEnable(GL_BLEND);
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		VectorScale(lightcolor, r_colorscale * r_shadow_lightintensityscale.value, color);
		for (renders = 0;renders < 64 && (color[0] > 0 || color[1] > 0 || color[2] > 0);renders++, color[0] = max(0, color[0] - 1.0f), color[1] = max(0, color[1] - 1.0f), color[2] = max(0, color[2] - 1.0f))
		{
			GL_Color(color[0], color[1], color[2], 1);
			R_Mesh_Draw(numverts, numtriangles, elements);
		}
	}
	else
	{
		// 2 texture no3D combine path, three pass
		m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
		m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
		R_Mesh_TextureState(&m);
		qglColorMask(0,0,0,1);
		qglDisable(GL_BLEND);
		GL_Color(1,1,1,1);
		R_Shadow_GenTexCoords_Attenuation2D1D(varray_texcoord[0], varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(bumptexture);
		m.tex[1] = 0;
		m.texcubemap[1] = R_GetTexture(r_shadow_normalscubetexture);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
		R_Mesh_TextureState(&m);
		qglBlendFunc(GL_DST_ALPHA, GL_ZERO);
		qglEnable(GL_BLEND);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(basetexture);
		m.texcubemap[1] = R_GetTexture(lightcubemap);
		m.texcombinergb[0] = GL_MODULATE;
		m.texcombinergb[1] = GL_MODULATE;
		R_Mesh_TextureState(&m);
		qglColorMask(1,1,1,0);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		VectorScale(lightcolor, r_colorscale * r_shadow_lightintensityscale.value, color);
		for (renders = 0;renders < 64 && (color[0] > 0 || color[1] > 0 || color[2] > 0);renders++, color[0] = max(0, color[0] - 1.0f), color[1] = max(0, color[1] - 1.0f), color[2] = max(0, color[2] - 1.0f))
		{
			GL_Color(color[0], color[1], color[2], 1);
			R_Mesh_Draw(numverts, numtriangles, elements);
		}
	}
}

void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int renders;
	float color[3];
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	if (!bumptexture)
		bumptexture = r_shadow_blankbumptexture;
	if (!glosstexture)
		glosstexture = r_shadow_blankglosstexture;
	if (r_shadow_gloss.integer >= 2 || (r_shadow_gloss.integer >= 1 && glosstexture != r_shadow_blankglosstexture))
	{
		// 2 texture no3D combine path, five pass
		memset(&m, 0, sizeof(m));

		m.tex[0] = R_GetTexture(bumptexture);
		m.texcubemap[1] = R_GetTexture(r_shadow_normalscubetexture);
		m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
		R_Mesh_TextureState(&m);
		qglColorMask(0,0,0,1);
		qglDisable(GL_BLEND);
		GL_Color(1,1,1,1);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		R_Shadow_GenTexCoords_Specular_NormalCubeMap(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, relativeeyeorigin);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = 0;
		m.texcubemap[1] = 0;
		m.texcombinergb[1] = GL_MODULATE;
		R_Mesh_TextureState(&m);
		// square alpha in framebuffer a few times to make it shiny
		qglBlendFunc(GL_ZERO, GL_DST_ALPHA);
		qglEnable(GL_BLEND);
		// these comments are a test run through this math for intensity 0.5
		// 0.5 * 0.5 = 0.25
		R_Mesh_Draw(numverts, numtriangles, elements);
		// 0.25 * 0.25 = 0.0625
		R_Mesh_Draw(numverts, numtriangles, elements);
		// 0.0625 * 0.0625 = 0.00390625
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(r_shadow_attenuation2dtexture);
		m.tex[1] = R_GetTexture(r_shadow_attenuation2dtexture);
		R_Mesh_TextureState(&m);
		qglBlendFunc(GL_DST_ALPHA, GL_ZERO);
		R_Shadow_GenTexCoords_Attenuation2D1D(varray_texcoord[0], varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(glosstexture);
		m.texcubemap[1] = R_GetTexture(lightcubemap);
		R_Mesh_TextureState(&m);
		qglColorMask(1,1,1,0);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		VectorScale(lightcolor, r_colorscale * r_shadow_lightintensityscale.value, color);
		for (renders = 0;renders < 64 && (color[0] > 0 || color[1] > 0 || color[2] > 0);renders++, color[0] = max(0, color[0] - 1.0f), color[1] = max(0, color[1] - 1.0f), color[2] = max(0, color[2] - 1.0f))
		{
			GL_Color(color[0], color[1], color[2], 1);
			R_Mesh_Draw(numverts, numtriangles, elements);
		}
	}
}

void R_Shadow_DrawWorldLightShadowVolume(matrix4x4_t *matrix, worldlight_t *light)
{
	R_Mesh_Matrix(matrix);
	R_Shadow_RenderShadowMeshVolume(light->shadowvolume);
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

static int castshadowcount = 1;
void R_Shadow_NewWorldLight(vec3_t origin, float radius, vec3_t color, int style, const char *cubemapname)
{
	int i, j, k, l, maxverts, *mark, tris;
	float *verts, *v, f, temp[3], radius2;
	//float projectdistance, *v0, *v1, temp2[3], temp3[3];
	worldlight_t *e;
	shadowmesh_t *mesh, *castmesh;
	mleaf_t *leaf;
	msurface_t *surf;
	qbyte *pvs;
	surfmesh_t *surfmesh;

	if (radius < 15 || DotProduct(color, color) < 0.03)
	{
		Con_Printf("R_Shadow_NewWorldLight: refusing to create a light too small/dim\n");
		return;
	}

	e = Mem_Alloc(r_shadow_mempool, sizeof(worldlight_t));
	VectorCopy(origin, e->origin);
	VectorScale(color, r_editlights_rtlightscolorscale.value, e->light);
	e->lightradius = radius * r_editlights_rtlightssizescale.value;
	e->cullradius = e->lightradius;
	e->mins[0] = e->origin[0] - e->lightradius;
	e->maxs[0] = e->origin[0] + e->lightradius;
	e->mins[1] = e->origin[1] - e->lightradius;
	e->maxs[1] = e->origin[1] + e->lightradius;
	e->mins[2] = e->origin[2] - e->lightradius;
	e->maxs[2] = e->origin[2] + e->lightradius;

	e->style = style;
	e->next = r_shadow_worldlightchain;
	r_shadow_worldlightchain = e;
	if (cubemapname)
	{
		e->cubemapname = Mem_Alloc(r_shadow_mempool, strlen(cubemapname) + 1);
		strcpy(e->cubemapname, cubemapname);
		// FIXME: add cubemap loading (and don't load a cubemap twice)
	}
	if (cl.worldmodel)
	{
		castshadowcount++;
		leaf = Mod_PointInLeaf(origin, cl.worldmodel);
		pvs = Mod_LeafPVS(leaf, cl.worldmodel);
		for (i = 0, leaf = cl.worldmodel->leafs + 1;i < cl.worldmodel->numleafs;i++, leaf++)
		{
			if (pvs[i >> 3] & (1 << (i & 7)))
			{
				VectorCopy(origin, temp);
				if (temp[0] < leaf->mins[0]) temp[0] = leaf->mins[0];
				if (temp[0] > leaf->maxs[0]) temp[0] = leaf->maxs[0];
				if (temp[1] < leaf->mins[1]) temp[1] = leaf->mins[1];
				if (temp[1] > leaf->maxs[1]) temp[1] = leaf->maxs[1];
				if (temp[2] < leaf->mins[2]) temp[2] = leaf->mins[2];
				if (temp[2] > leaf->maxs[2]) temp[2] = leaf->maxs[2];
				VectorSubtract(temp, origin, temp);
				if (DotProduct(temp, temp) < e->lightradius * e->lightradius)
				{
					leaf->worldnodeframe = castshadowcount;
					for (j = 0, mark = leaf->firstmarksurface;j < leaf->nummarksurfaces;j++, mark++)
					{
						surf = cl.worldmodel->surfaces + *mark;
						if (surf->castshadow != castshadowcount)
						{
							f = DotProduct(e->origin, surf->plane->normal) - surf->plane->dist;
							if (surf->flags & SURF_PLANEBACK)
								f = -f;
							if (f > 0 && f < e->lightradius)
							{
								temp[0] = bound(surf->poly_mins[0], e->origin[0], surf->poly_maxs[0]) - e->origin[0];
								temp[1] = bound(surf->poly_mins[1], e->origin[1], surf->poly_maxs[1]) - e->origin[1];
								temp[2] = bound(surf->poly_mins[2], e->origin[2], surf->poly_maxs[2]) - e->origin[2];
								if (DotProduct(temp, temp) < e->lightradius * e->lightradius)
									surf->castshadow = castshadowcount;
							}
						}
					}
				}
			}
		}

		e->numleafs = 0;
		for (i = 0, leaf = cl.worldmodel->leafs + 1;i < cl.worldmodel->numleafs;i++, leaf++)
			if (leaf->worldnodeframe == castshadowcount)
				e->numleafs++;
		e->numsurfaces = 0;
		for (i = 0, surf = cl.worldmodel->surfaces + cl.worldmodel->firstmodelsurface;i < cl.worldmodel->nummodelsurfaces;i++, surf++)
			if (surf->castshadow == castshadowcount)
				e->numsurfaces++;

		if (e->numleafs)
			e->leafs = Mem_Alloc(r_shadow_mempool, e->numleafs * sizeof(mleaf_t *));
		if (e->numsurfaces)
			e->surfaces = Mem_Alloc(r_shadow_mempool, e->numsurfaces * sizeof(msurface_t *));
		e->numleafs = 0;
		for (i = 0, leaf = cl.worldmodel->leafs + 1;i < cl.worldmodel->numleafs;i++, leaf++)
			if (leaf->worldnodeframe == castshadowcount)
				e->leafs[e->numleafs++] = leaf;
		e->numsurfaces = 0;
		for (i = 0, surf = cl.worldmodel->surfaces + cl.worldmodel->firstmodelsurface;i < cl.worldmodel->nummodelsurfaces;i++, surf++)
			if (surf->castshadow == castshadowcount)
				e->surfaces[e->numsurfaces++] = surf;
		// find bounding box and sphere of lit surfaces
		// (these will be used for creating a shape to clip the light)
		radius2 = 0;
		VectorCopy(e->origin, e->mins);
		VectorCopy(e->origin, e->maxs);
		for (j = 0;j < e->numsurfaces;j++)
		{
			surf = e->surfaces[j];
			for (k = 0, v = surf->poly_verts;k < surf->poly_numverts;k++, v += 3)
			{
				if (e->mins[0] > v[0]) e->mins[0] = v[0];if (e->maxs[0] < v[0]) e->maxs[0] = v[0];
				if (e->mins[1] > v[1]) e->mins[1] = v[1];if (e->maxs[1] < v[1]) e->maxs[1] = v[1];
				if (e->mins[2] > v[2]) e->mins[2] = v[2];if (e->maxs[2] < v[2]) e->maxs[2] = v[2];
				VectorSubtract(v, e->origin, temp);
				f = DotProduct(temp, temp);
				if (radius2 < f)
					radius2 = f;
			}
		}
		e->cullradius = sqrt(radius2);
		if (e->cullradius > e->lightradius)
			e->cullradius = e->lightradius;
		if (e->mins[0] < e->origin[0] - e->lightradius) e->mins[0] = e->origin[0] - e->lightradius;
		if (e->maxs[0] > e->origin[0] + e->lightradius) e->maxs[0] = e->origin[0] + e->lightradius;
		if (e->mins[1] < e->origin[1] - e->lightradius) e->mins[1] = e->origin[1] - e->lightradius;
		if (e->maxs[1] > e->origin[1] + e->lightradius) e->maxs[1] = e->origin[1] + e->lightradius;
		if (e->mins[2] < e->origin[2] - e->lightradius) e->mins[2] = e->origin[2] - e->lightradius;
		if (e->maxs[2] > e->origin[2] + e->lightradius) e->maxs[2] = e->origin[2] + e->lightradius;

		maxverts = 256;
		verts = NULL;
		castshadowcount++;
		for (j = 0;j < e->numsurfaces;j++)
		{
			surf = e->surfaces[j];
			if (surf->flags & SURF_SHADOWCAST)
			{
				surf->castshadow = castshadowcount;
				if (maxverts < surf->poly_numverts)
					maxverts = surf->poly_numverts;
			}
		}
		e->shadowvolume = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768);
		// make a mesh to cast a shadow volume from
		castmesh = Mod_ShadowMesh_Begin(r_shadow_mempool, 32768);
		for (j = 0;j < e->numsurfaces;j++)
			if (e->surfaces[j]->castshadow == castshadowcount)
				for (surfmesh = e->surfaces[j]->mesh;surfmesh;surfmesh = surfmesh->chain)
					Mod_ShadowMesh_AddMesh(r_shadow_mempool, castmesh, surfmesh->numverts, surfmesh->verts, surfmesh->numtriangles, surfmesh->index);
		castmesh = Mod_ShadowMesh_Finish(r_shadow_mempool, castmesh);

		// cast shadow volume from castmesh
		for (mesh = castmesh;mesh;mesh = mesh->next)
		{
			R_Shadow_ResizeTriangleFacingLight(castmesh->numtriangles);
			R_Shadow_ResizeShadowElements(castmesh->numtriangles);

			if (maxverts < castmesh->numverts * 2)
			{
				maxverts = castmesh->numverts * 2;
				if (verts)
					Mem_Free(verts);
				verts = NULL;
			}
			if (verts == NULL && maxverts > 0)
				verts = Mem_Alloc(r_shadow_mempool, maxverts * sizeof(float[4]));

			// now that we have the buffers big enough, construct shadow volume mesh
			memcpy(verts, castmesh->verts, castmesh->numverts * sizeof(float[4]));
			R_Shadow_ProjectVertices(verts, castmesh->numverts, e->origin, 10000000.0f);//, e->lightradius);
			R_Shadow_MakeTriangleShadowFlags(castmesh->elements, verts, castmesh->numtriangles, trianglefacinglight, e->origin, e->lightradius);
			tris = R_Shadow_BuildShadowVolumeTriangles(castmesh->elements, castmesh->neighbors, castmesh->numtriangles, castmesh->numverts, trianglefacinglight, shadowelements);
			// add the constructed shadow volume mesh
			Mod_ShadowMesh_AddMesh(r_shadow_mempool, e->shadowvolume, castmesh->numverts, verts, tris, shadowelements);
		}
		// we're done with castmesh now
		Mod_ShadowMesh_Free(castmesh);
		e->shadowvolume = Mod_ShadowMesh_Finish(r_shadow_mempool, e->shadowvolume);
		for (l = 0, mesh = e->shadowvolume;mesh;mesh = mesh->next)
			l += mesh->numtriangles;
		Con_Printf("static shadow volume built containing %i triangles\n", l);
	}
	Con_Printf("%f %f %f, %f %f %f, %f, %f, %d, %d\n", e->mins[0], e->mins[1], e->mins[2], e->maxs[0], e->maxs[1], e->maxs[2], e->cullradius, e->lightradius, e->numleafs, e->numsurfaces);
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
	if (light->shadowvolume)
		Mod_ShadowMesh_Free(light->shadowvolume);
	if (light->surfaces)
		Mem_Free(light->surfaces);
	if (light->leafs)
		Mem_Free(light->leafs);
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

void R_Shadow_FreeSelectedWorldLight(void)
{
	if (r_shadow_selectedlight)
	{
		R_Shadow_FreeWorldLight(r_shadow_selectedlight);
		r_shadow_selectedlight = NULL;
	}
}

void R_DrawLightSprite(int texnum, const vec3_t origin, vec_t scale, float cr, float cg, float cb, float ca)
{
	rmeshstate_t m;
	float diff[3];

	if (fogenabled)
	{
		VectorSubtract(origin, r_origin, diff);
		ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
	}

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.tex[0] = texnum;
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	GL_Color(cr * r_colorscale, cg * r_colorscale, cb * r_colorscale, ca);
	varray_texcoord[0][ 0] = 0;varray_texcoord[0][ 1] = 0;
	varray_texcoord[0][ 4] = 0;varray_texcoord[0][ 5] = 1;
	varray_texcoord[0][ 8] = 1;varray_texcoord[0][ 9] = 1;
	varray_texcoord[0][12] = 1;varray_texcoord[0][13] = 0;
	varray_vertex[0] = origin[0] - vright[0] * scale - vup[0] * scale;
	varray_vertex[1] = origin[1] - vright[1] * scale - vup[1] * scale;
	varray_vertex[2] = origin[2] - vright[2] * scale - vup[2] * scale;
	varray_vertex[4] = origin[0] - vright[0] * scale + vup[0] * scale;
	varray_vertex[5] = origin[1] - vright[1] * scale + vup[1] * scale;
	varray_vertex[6] = origin[2] - vright[2] * scale + vup[2] * scale;
	varray_vertex[8] = origin[0] + vright[0] * scale + vup[0] * scale;
	varray_vertex[9] = origin[1] + vright[1] * scale + vup[1] * scale;
	varray_vertex[10] = origin[2] + vright[2] * scale + vup[2] * scale;
	varray_vertex[12] = origin[0] + vright[0] * scale - vup[0] * scale;
	varray_vertex[13] = origin[1] + vright[1] * scale - vup[1] * scale;
	varray_vertex[14] = origin[2] + vright[2] * scale - vup[2] * scale;
	R_Mesh_Draw(4, 2, polygonelements);
}

void R_Shadow_DrawCursorCallback(const void *calldata1, int calldata2)
{
	cachepic_t *pic;
	pic = Draw_CachePic("gfx/crosshair1.tga");
	if (pic)
		R_DrawLightSprite(R_GetTexture(pic->tex), r_editlights_cursorlocation, r_editlights_cursorgrid.value * 0.5f, 1, 1, 1, 0.5);
}

void R_Shadow_DrawLightSpriteCallback(const void *calldata1, int calldata2)
{
	float intensity;
	const worldlight_t *light;
	light = calldata1;
	intensity = 0.5;
	if (light->selected)
		intensity = 0.75 + 0.25 * sin(realtime * M_PI * 4.0);
	if (light->shadowvolume)
		R_DrawLightSprite(calldata2, light->origin, 8, intensity, intensity, intensity, 0.5);
	else
		R_DrawLightSprite(calldata2, light->origin, 8, intensity * 0.5, intensity * 0.5, intensity * 0.5, 0.5);
}

void R_Shadow_DrawLightSprites(void)
{
	int i, texnums[5];
	cachepic_t *pic;
	worldlight_t *light;

	for (i = 0;i < 5;i++)
	{
		pic = Draw_CachePic(va("gfx/crosshair%i.tga", i + 1));
		if (pic)
			texnums[i] = R_GetTexture(pic->tex);
		else
			texnums[i] = 0;
	}

	for (light = r_shadow_worldlightchain;light;light = light->next)
		R_MeshQueue_AddTransparent(light->origin, R_Shadow_DrawLightSpriteCallback, light, texnums[((int) light) % 5]);
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
		VectorSubtract(light->origin, r_refdef.vieworg, temp);
		rating = (DotProduct(temp, vpn) / sqrt(DotProduct(temp, temp)));
		if (rating >= 0.95)
		{
			rating /= (1 + 0.0625f * sqrt(DotProduct(temp, temp)));
			if (bestrating < rating && CL_TraceLine(light->origin, r_refdef.vieworg, NULL, NULL, 0, true, NULL) == 1.0f)
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
	int n, a, style;
	char name[MAX_QPATH], cubemapname[MAX_QPATH], *lightsstring, *s, *t;
	float origin[3], radius, color[3];
	if (cl.worldmodel == NULL)
	{
		Con_Printf("No map loaded.\n");
		return;
	}
	COM_StripExtension(cl.worldmodel->name, name);
	strcat(name, ".rtlights");
	lightsstring = COM_LoadFile(name, false);
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
			a = sscanf(t, "%f %f %f %f %f %f %f %d %s", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, &cubemapname);
			if (a < 9)
				cubemapname[0] = 0;
			*s = '\n';
			if (a < 8)
			{
				Con_Printf("found %d parameters on line %i, should be 8 or 9 parameters (origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style cubemapname)\n", a, n + 1);
				break;
			}
			R_Shadow_NewWorldLight(origin, radius, color, style, cubemapname);
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
	COM_StripExtension(cl.worldmodel->name, name);
	strcat(name, ".rtlights");
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		sprintf(line, "%g %g %g %g %g %g %g %d %s\n", light->origin[0], light->origin[1], light->origin[2], light->lightradius / r_editlights_rtlightssizescale.value, light->light[0] / r_editlights_rtlightscolorscale.value, light->light[1] / r_editlights_rtlightscolorscale.value, light->light[2] / r_editlights_rtlightscolorscale.value, light->style, light->cubemapname ? light->cubemapname : "");
		if (bufchars + strlen(line) > bufmaxchars)
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
		COM_WriteFile(name, buf, bufchars);
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
	COM_StripExtension(cl.worldmodel->name, name);
	strcat(name, ".lights");
	lightsstring = COM_LoadFile(name, false);
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
			VectorScale(color, (1.0f / (8388608.0f)), color);
			R_Shadow_NewWorldLight(origin, radius, color, style, NULL);
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
	data = cl.worldmodel->entities;
	if (!data)
		return;
	for (entnum = 0;COM_ParseToken(&data) && com_token[0] == '{';entnum++)
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
			if (!COM_ParseToken(&data))
				break; // error
			if (com_token[0] == '}')
				break; // end of entity
			if (com_token[0] == '_')
				strcpy(key, com_token + 1);
			else
				strcpy(key, com_token);
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken(&data))
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
						overridecolor[1] = 0.7;
						overridecolor[2] = 0.2;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.7;
						overridecolor[2] = 0.2;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.9;
						overridecolor[2] = 0.7;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 40;
						overridecolor[0] = 1;
						overridecolor[1] = 0.7;
						overridecolor[2] = 0.2;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
		}
		if (light <= 0 && islight)
			light = 300;
		radius = bound(15, light * r_editlights_quakelightsizescale.value / scale, 1048576);
		light = sqrt(bound(0, light, 1048576)) * (1.0f / 16.0f);
		if (color[0] == 1 && color[1] == 1 && color[2] == 1)
			VectorCopy(overridecolor, color);
		VectorScale(color, light, color);
		VectorAdd(origin, originhack, origin);
		if (radius >= 15)
			R_Shadow_NewWorldLight(origin, radius, color, style, NULL);
	}
}


void R_Shadow_SetCursorLocationForView(void)
{
	vec_t dist, push, frac;
	vec3_t dest, endpos, normal;
	VectorMA(r_refdef.vieworg, r_editlights_cursordistance.value, vpn, dest);
	frac = CL_TraceLine(r_refdef.vieworg, dest, endpos, normal, 0, true, NULL);
	if (frac < 1)
	{
		dist = frac * r_editlights_cursordistance.value;
		push = r_editlights_cursorpushback.value;
		if (push > dist)
			push = dist;
		push = -push;
		VectorMA(endpos, push, vpn, endpos);
		VectorMA(endpos, r_editlights_cursorpushoff.value, normal, endpos);
	}
	r_editlights_cursorlocation[0] = floor(endpos[0] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
	r_editlights_cursorlocation[1] = floor(endpos[1] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
	r_editlights_cursorlocation[2] = floor(endpos[2] / r_editlights_cursorgrid.value + 0.5f) * r_editlights_cursorgrid.value;
}

void R_Shadow_UpdateLightingMode(void)
{
	r_shadow_lightingmode = 0;
	if (r_shadow_realtime.integer)
	{
		if (r_shadow_worldlightchain)
			r_shadow_lightingmode = 2;
		else
			r_shadow_lightingmode = 1;
	}
}

void R_Shadow_UpdateWorldLightSelection(void)
{
	R_Shadow_SetCursorLocationForView();
	if (r_editlights.integer)
	{
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
	vec3_t origin, color;
	vec_t radius;
	int style;
	const char *cubemapname;
	if (!r_editlights.integer)
	{
		Con_Printf("Cannot spawn light when not in editing mode.  Set r_editlights to 1.\n");
		return;
	}
	if (Cmd_Argc() <= 7)
	{
		radius = 200;
		color[0] = color[1] = color[2] = 1;
		style = 0;
		cubemapname = NULL;
		if (Cmd_Argc() >= 2)
		{
			radius = atof(Cmd_Argv(1));
			if (Cmd_Argc() >= 3)
			{
				color[0] = atof(Cmd_Argv(2));
				color[1] = color[0];
				color[2] = color[0];
				if (Cmd_Argc() >= 5)
				{
					color[1] = atof(Cmd_Argv(3));
					color[2] = atof(Cmd_Argv(4));
					if (Cmd_Argc() >= 6)
					{
						style = atoi(Cmd_Argv(5));
						if (Cmd_Argc() >= 7)
							cubemapname = Cmd_Argv(6);
					}
				}
			}
		}
		if (cubemapname && !cubemapname[0])
			cubemapname = NULL;
		if (radius >= 16 && color[0] >= 0 && color[1] >= 0 && color[2] >= 0 && style >= 0 && style < 256 && (color[0] >= 0.1 || color[1] >= 0.1 || color[2] >= 0.1))
		{
			VectorCopy(r_editlights_cursorlocation, origin);
			R_Shadow_NewWorldLight(origin, radius, color, style, cubemapname);
			return;
		}
	}
	Con_Printf("usage: r_editlights_spawn radius red green blue [style [cubemap]]\n");
}

void R_Shadow_EditLights_Edit_f(void)
{
	vec3_t origin, color;
	vec_t radius;
	int style;
	const char *cubemapname;
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
	if (Cmd_Argc() <= 7)
	{
		radius = 200;
		color[0] = color[1] = color[2] = 1;
		style = 0;
		cubemapname = NULL;
		if (Cmd_Argc() >= 2)
		{
			radius = atof(Cmd_Argv(1));
			if (Cmd_Argc() >= 3)
			{
				color[0] = atof(Cmd_Argv(2));
				color[1] = color[0];
				color[2] = color[0];
				if (Cmd_Argc() >= 5)
				{
					color[1] = atof(Cmd_Argv(3));
					color[2] = atof(Cmd_Argv(4));
					if (Cmd_Argc() >= 6)
					{
						style = atoi(Cmd_Argv(5));
						if (Cmd_Argc() >= 7)
							cubemapname = Cmd_Argv(6);
					}
				}
			}
		}
		if (cubemapname && !cubemapname[0])
			cubemapname = NULL;
		if (radius >= 16 && color[0] >= 0 && color[1] >= 0 && color[2] >= 0 && style >= 0 && style < 256 && (color[0] >= 0.1 || color[1] >= 0.1 || color[2] >= 0.1))
		{
			VectorCopy(r_shadow_selectedlight->origin, origin);
			R_Shadow_FreeWorldLight(r_shadow_selectedlight);
			r_shadow_selectedlight = NULL;
			R_Shadow_NewWorldLight(origin, radius, color, style, cubemapname);
			return;
		}
	}
	Con_Printf("usage: r_editlights_edit radius red green blue [style [cubemap]]\n");
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
	R_Shadow_FreeSelectedWorldLight();
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
	Cmd_AddCommand("r_editlights_clear", R_Shadow_EditLights_Clear_f);
	Cmd_AddCommand("r_editlights_reload", R_Shadow_EditLights_Reload_f);
	Cmd_AddCommand("r_editlights_save", R_Shadow_EditLights_Save_f);
	Cmd_AddCommand("r_editlights_spawn", R_Shadow_EditLights_Spawn_f);
	Cmd_AddCommand("r_editlights_edit", R_Shadow_EditLights_Edit_f);
	Cmd_AddCommand("r_editlights_remove", R_Shadow_EditLights_Remove_f);
	Cmd_AddCommand("r_editlights_importlightentitiesfrommap", R_Shadow_EditLights_ImportLightEntitiesFromMap_f);
	Cmd_AddCommand("r_editlights_importlightsfile", R_Shadow_EditLights_ImportLightsFile_f);
}
