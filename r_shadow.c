
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
rtexture_t *r_shadow_normalsattenuationtexture;
rtexture_t *r_shadow_normalscubetexture;
rtexture_t *r_shadow_attenuation2dtexture;
rtexture_t *r_shadow_blankbumptexture;
rtexture_t *r_shadow_blankglosstexture;
rtexture_t *r_shadow_blankwhitetexture;

cvar_t r_shadow_lightattenuationscale = {0, "r_shadow_lightattenuationscale", "2"};
cvar_t r_shadow_lightintensityscale = {0, "r_shadow_lightintensityscale", "1"};
cvar_t r_shadow_realtime = {0, "r_shadow_realtime", "0"};
cvar_t r_shadow_erasebydrawing = {0, "r_shadow_erasebydrawing", "0"};
cvar_t r_shadow_texture3d = {0, "r_shadow_texture3d", "0"};
cvar_t r_shadow_gloss = {0, "r_shadow_gloss", "1"};
cvar_t r_shadow_debuglight = {0, "r_shadow_debuglight", "-1"};

void R_Shadow_ClearWorldLights(void);
void r_shadow_start(void)
{
	// allocate vertex processing arrays
	r_shadow_mempool = Mem_AllocPool("R_Shadow");
	maxshadowelements = 0;
	shadowelements = NULL;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	r_shadow_normalsattenuationtexture = NULL;
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
	r_shadow_normalsattenuationtexture = NULL;
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

void R_Shadow_LoadWorldLights(const char *mapname);
void r_shadow_newmap(void)
{
	R_Shadow_ClearWorldLights();
	r_shadow_reloadlights = true;
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow_lightattenuationscale);
	Cvar_RegisterVariable(&r_shadow_lightintensityscale);
	Cvar_RegisterVariable(&r_shadow_realtime);
	Cvar_RegisterVariable(&r_shadow_texture3d);
	Cvar_RegisterVariable(&r_shadow_gloss);
	Cvar_RegisterVariable(&r_shadow_debuglight);
	Cvar_RegisterVariable(&r_shadow_erasebydrawing);
	R_Shadow_EditLights_Init();
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

void R_Shadow_ProjectVertices(const float *in, float *out, int numverts, const float *relativelightorigin, float projectdistance)
{
	int i;
	for (i = 0;i < numverts;i++, in += 4, out += 4)
	{
#if 1
		out[0] = in[0] + 1000000.0f * (in[0] - relativelightorigin[0]);
		out[1] = in[1] + 1000000.0f * (in[1] - relativelightorigin[1]);
		out[2] = in[2] + 1000000.0f * (in[2] - relativelightorigin[2]);
#elif 0
		VectorSubtract(in, relativelightorigin, temp);
		f = lightradius / sqrt(DotProduct(temp,temp));
		if (f < 1)
			f = 1;
		VectorMA(relativelightorigin, f, temp, out);
#else
		VectorSubtract(in, relativelightorigin, temp);
		f = projectdistance / sqrt(DotProduct(temp,temp));
		VectorMA(in, f, temp, out);
#endif
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
#if 0
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
		float dir0[3], dir1[3], temp[3], f;

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
		//trianglefacinglight[i] = DotProduct(relativelightorigin, temp) >= DotProduct(v0, temp);
		f = DotProduct(relativelightorigin, temp) - DotProduct(v0, temp);
		trianglefacinglight[i] = f > 0 && f < lightradius * sqrt(DotProduct(temp, temp));
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

	// generate projected vertices
	// by clever use of elements we'll construct the whole shadow from
	// the unprojected vertices and these projected vertices
	R_Shadow_ProjectVertices(varray_vertex, varray_vertex + numverts * 4, numverts, relativelightorigin, projectdistance);

	// check which triangles are facing the light
	R_Shadow_MakeTriangleShadowFlags(elements, varray_vertex, numtris, trianglefacinglight, relativelightorigin, lightradius);

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

float r_shadow_atten1;
#define ATTEN3DSIZE 64
static void R_Shadow_Make3DTextures(void)
{
	int x, y, z;
	float v[3], intensity, ilen, bordercolor[4];
	qbyte *data;
	if (r_shadow_texture3d.integer != 1 || !gl_texture3d)
		return;
	data = Mem_Alloc(tempmempool, ATTEN3DSIZE * ATTEN3DSIZE * ATTEN3DSIZE * 4);
	for (z = 0;z < ATTEN3DSIZE;z++)
	{
		for (y = 0;y < ATTEN3DSIZE;y++)
		{
			for (x = 0;x < ATTEN3DSIZE;x++)
			{
				v[0] = (x + 0.5f) * (2.0f / (float) ATTEN3DSIZE) - 1.0f;
				v[1] = (y + 0.5f) * (2.0f / (float) ATTEN3DSIZE) - 1.0f;
				v[2] = (z + 0.5f) * (2.0f / (float) ATTEN3DSIZE) - 1.0f;
				intensity = 1.0f - sqrt(DotProduct(v, v));
				if (intensity > 0)
					intensity *= intensity;
				ilen = 127.0f * bound(0, intensity * r_shadow_atten1, 1) / sqrt(DotProduct(v, v));
				data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+0] = 128.0f + ilen * v[0];
				data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+1] = 128.0f + ilen * v[1];
				data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+2] = 128.0f + ilen * v[2];
				data[((z*ATTEN3DSIZE+y)*ATTEN3DSIZE+x)*4+3] = 255;
			}
		}
	}
	r_shadow_normalsattenuationtexture = R_LoadTexture3D(r_shadow_texturepool, "normalsattenuation", ATTEN3DSIZE, ATTEN3DSIZE, ATTEN3DSIZE, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
	bordercolor[0] = 0.5f;
	bordercolor[1] = 0.5f;
	bordercolor[2] = 0.5f;
	bordercolor[3] = 1.0f;
	qglTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, bordercolor);
	Mem_Free(data);
}

static void R_Shadow_MakeTextures(void)
{
	int x, y, d, side;
	float v[3], s, t, intensity;
	qbyte *data;
	data = Mem_Alloc(tempmempool, 6*128*128*4);
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_atten1 = r_shadow_lightattenuationscale.value;
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
			v[0] = (x + 0.5f) * (2.0f / 128.0f) - 1.0f;
			v[1] = (y + 0.5f) * (2.0f / 128.0f) - 1.0f;
			v[2] = 0;
			intensity = 1.0f - sqrt(DotProduct(v, v));
			if (intensity > 0)
				intensity *= intensity;
			intensity = bound(0, intensity * r_shadow_atten1 * 256.0f, 255.0f);
			d = bound(0, intensity, 255);
			data[((0*128+y)*128+x)*4+0] = d;
			data[((0*128+y)*128+x)*4+1] = d;
			data[((0*128+y)*128+x)*4+2] = d;
			data[((0*128+y)*128+x)*4+3] = d;
		}
	}
	r_shadow_attenuation2dtexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation2d", 128, 128, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALPHA | TEXF_MIPMAP, NULL);
	Mem_Free(data);
	R_Shadow_Make3DTextures();
}

void R_Shadow_Stage_Begin(void)
{
	rmeshstate_t m;

	if (r_shadow_texture3d.integer == 1 && !gl_texture3d)
	{
		Con_Printf("3D texture support not detected, falling back on slower 2D + 1D + normalization lighting\n");
		Cvar_SetValueQuick(&r_shadow_texture3d, 0);
	}
	//cl.worldmodel->numlights = min(cl.worldmodel->numlights, 1);
	if (!r_shadow_attenuation2dtexture
	 || (r_shadow_texture3d.integer == 1 && !r_shadow_normalsattenuationtexture)
	 || r_shadow_lightattenuationscale.value != r_shadow_atten1)
		R_Shadow_MakeTextures();
	if (r_shadow_reloadlights && cl.worldmodel)
	{
		r_shadow_reloadlights = false;
		R_Shadow_LoadWorldLights(cl.worldmodel->name);
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
	qglStencilFunc(GL_ALWAYS, 0, 0xFF);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_DEPTH_TEST);
	r_shadowstage = SHADOWSTAGE_STENCIL;
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
	// stencil is 0 (non-zero means shadow)
	qglStencilFunc(GL_EQUAL, 0, 0xFF);
	qglEnable(GL_CULL_FACE);
	qglEnable(GL_DEPTH_TEST);
	r_shadowstage = SHADOWSTAGE_LIGHT;
}

int R_Shadow_Stage_EraseShadowVolumes(void)
{
	if (r_shadow_erasebydrawing.integer)
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
		qglStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
		qglStencilFunc(GL_ALWAYS, 0, 0xFF);
		qglDisable(GL_CULL_FACE);
		qglDisable(GL_DEPTH_TEST);
		r_shadowstage = SHADOWSTAGE_ERASESTENCIL;
		return true;
	}
	else
	{
		qglClear(GL_STENCIL_BUFFER_BIT);
		return false;
	}
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
	qglDepthFunc(GL_LEQUAL);
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 0, 0xFF);
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
	int renders, mult;
	float scale, colorscale;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	if (!bumptexture)
		bumptexture = r_shadow_blankbumptexture;
	// colorscale accounts for how much we multiply the brightness during combine
	// mult is how many times the final pass of the lighting will be
	// performed to get more brightness than otherwise possible
	// limit mult to 64 for sanity sake
	if (r_shadow_texture3d.integer)
	{
		if (r_textureunits.integer >= 4 && !lightcubemap)
		{
			// 4 texture 3D combine path, one pass, no light cubemap support
			m.tex[0] = R_GetTexture(bumptexture);
			m.tex3d[1] = R_GetTexture(r_shadow_normalsattenuationtexture);
			m.tex[2] = R_GetTexture(basetexture);
			m.tex[3] = R_GetTexture(r_shadow_blankwhitetexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGB_ARB;
			m.texcombinergb[2] = GL_MODULATE;
			m.texcombinergb[3] = GL_MODULATE;
			m.texrgbscale[1] = 1;
			m.texrgbscale[3] = 4;
			R_Mesh_TextureState(&m);
			memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
			memcpy(varray_texcoord[2], texcoords, numverts * sizeof(float[4]));
			R_Shadow_GenTexCoords_Diffuse_Attenuation3D(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
			qglActiveTexture(GL_TEXTURE3_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
			colorscale = r_colorscale * 0.25f * r_shadow_lightintensityscale.value;
			for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
			colorscale *= scale;
			GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
			for (renders = 0;renders < mult;renders++)
				R_Mesh_Draw(numverts, numtriangles, elements);
			qglActiveTexture(GL_TEXTURE3_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		}
		else
		{
			// 2 texture no3D combine path, two pass
			m.tex[0] = R_GetTexture(bumptexture);
			m.tex3d[1] = R_GetTexture(r_shadow_normalsattenuationtexture);
			m.texcombinergb[0] = GL_REPLACE;
			m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
			m.texalphascale[1] = 1;
			R_Mesh_TextureState(&m);
			qglColorMask(0,0,0,1);
			qglDisable(GL_BLEND);
			GL_Color(1,1,1,1);
			memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
			R_Shadow_GenTexCoords_Diffuse_Attenuation3D(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
			R_Mesh_Draw(numverts, numtriangles, elements);

			m.tex[0] = R_GetTexture(basetexture);
			m.tex3d[1] = 0;
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.texcombinergb[0] = GL_MODULATE;
			m.texcombinergb[1] = GL_MODULATE;
			m.texrgbscale[1] = 1;
			m.texalphascale[1] = 1;
			R_Mesh_TextureState(&m);
			qglColorMask(1,1,1,1);
			qglBlendFunc(GL_DST_ALPHA, GL_ONE);
			qglEnable(GL_BLEND);
			if (lightcubemap)
				R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

			colorscale = r_colorscale * 1.0f * r_shadow_lightintensityscale.value;
			for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
			colorscale *= scale;
			GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
			for (renders = 0;renders < mult;renders++)
				R_Mesh_Draw(numverts, numtriangles, elements);
		}
	}
	else if (r_textureunits.integer >= 4)
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
		qglColorMask(1,1,1,1);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		qglEnable(GL_BLEND);
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		colorscale = r_colorscale * 1.0f * r_shadow_lightintensityscale.value;
		for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
		colorscale *= scale;
		GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
		for (renders = 0;renders < mult;renders++)
			R_Mesh_Draw(numverts, numtriangles, elements);
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
		m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
		R_Mesh_TextureState(&m);
		qglBlendFunc(GL_DST_ALPHA, GL_ZERO);
		qglEnable(GL_BLEND);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin);
		R_Mesh_Draw(numverts, numtriangles, elements);

		m.tex[0] = R_GetTexture(basetexture);
		m.texcubemap[1] = R_GetTexture(lightcubemap);
		m.texcombinergb[1] = GL_MODULATE;
		R_Mesh_TextureState(&m);
		qglColorMask(1,1,1,1);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		colorscale = r_colorscale * 1.0f * r_shadow_lightintensityscale.value;
		for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
		colorscale *= scale;
		GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
		for (renders = 0;renders < mult;renders++)
			R_Mesh_Draw(numverts, numtriangles, elements);
	}
}

void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int renders, mult;
	float scale, colorscale;
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
		qglColorMask(1,1,1,1);
		qglBlendFunc(GL_DST_ALPHA, GL_ONE);
		memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
		if (lightcubemap)
			R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);

		// the 0.25f makes specular lighting much dimmer than diffuse (intentionally)
		colorscale = r_colorscale * 0.25f * r_shadow_lightintensityscale.value;
		for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
		colorscale *= scale;
		GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
		for (renders = 0;renders < mult;renders++)
			R_Mesh_Draw(numverts, numtriangles, elements);
	}
}

#define PRECOMPUTEDSHADOWVOLUMES 1
void R_Shadow_DrawWorldLightShadowVolume(matrix4x4_t *matrix, worldlight_t *light)
{
#if PRECOMPUTEDSHADOWVOLUMES
	R_Mesh_Matrix(matrix);
	R_Shadow_RenderShadowMeshVolume(light->shadowvolume);
#else
	shadowmesh_t *mesh;
	R_Mesh_Matrix(matrix);
	for (mesh = light->shadowvolume;mesh;mesh = mesh->next)
	{
		R_Mesh_ResizeCheck(mesh->numverts * 2);
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		R_Shadow_Volume(mesh->numverts, mesh->numtriangles, varray_vertex, mesh->elements, mesh->neighbors, light->origin, light->lightradius, light->lightradius);
	}
#endif
}

cvar_t r_editlights = {0, "r_editlights", "0"};
cvar_t r_editlights_cursordistance = {0, "r_editlights_distance", "1024"};
cvar_t r_editlights_cursorpushback = {0, "r_editlights_pushback", "0"};
cvar_t r_editlights_cursorpushoff = {0, "r_editlights_pushoff", "4"};
cvar_t r_editlights_cursorgrid = {0, "r_editlights_grid", "4"};
worldlight_t *r_shadow_worldlightchain;
worldlight_t *r_shadow_selectedlight;
vec3_t r_editlights_cursorlocation;

static int castshadowcount = 1;
void R_Shadow_NewWorldLight(vec3_t origin, float radius, vec3_t color, int style, const char *cubemapname)
{
	int i, j, k, l, maxverts, *mark;
	float *verts, *v, *v0, *v1, f, projectdistance, temp[3], temp2[3], temp3[3], radius2;
	worldlight_t *e;
	shadowmesh_t *mesh;
	mleaf_t *leaf;
	msurface_t *surf;
	qbyte *pvs;

	e = Mem_Alloc(r_shadow_mempool, sizeof(worldlight_t));
	VectorCopy(origin, e->origin);
	VectorCopy(color, e->light);
	e->lightradius = radius;
	VectorCopy(origin, e->mins);
	VectorCopy(origin, e->maxs);
	e->cullradius = 0;
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
								VectorSubtract(e->origin, surf->poly_center, temp);
								if (DotProduct(temp, temp) - surf->poly_radius2 < e->lightradius * e->lightradius)
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
		Con_Printf("%f %f %f, %f %f %f, %f, %f, %d, %d\n", e->mins[0], e->mins[1], e->mins[2], e->maxs[0], e->maxs[1], e->maxs[2], e->cullradius, e->lightradius, e->numleafs, e->numsurfaces);
		// clip shadow volumes against eachother to remove unnecessary
		// polygons (and sections of polygons)
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
		e->shadowvolume = Mod_ShadowMesh_Begin(loadmodel->mempool, 32768);
#if !PRECOMPUTEDSHADOWVOLUMES
		// make a mesh to cast a shadow volume from
		for (j = 0;j < e->numsurfaces;j++)
			if (e->surfaces[j]->castshadow == castshadowcount)
				Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, e->surfaces[j]->poly_numverts, e->surfaces[j]->poly_verts);
#else
#if 1
		{
		int tris;
		shadowmesh_t *castmesh, *mesh;
		surfmesh_t *surfmesh;
		// make a mesh to cast a shadow volume from
		castmesh = Mod_ShadowMesh_Begin(loadmodel->mempool, 32768);
		for (j = 0;j < e->numsurfaces;j++)
			if (e->surfaces[j]->castshadow == castshadowcount)
				for (surfmesh = e->surfaces[j]->mesh;surfmesh;surfmesh = surfmesh->chain)
					Mod_ShadowMesh_AddMesh(loadmodel->mempool, castmesh, surfmesh->numverts, surfmesh->verts, surfmesh->numtriangles, surfmesh->index);
		castmesh = Mod_ShadowMesh_Finish(loadmodel->mempool, castmesh);

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
				verts = Mem_Alloc(loadmodel->mempool, maxverts * sizeof(float[4]));

			// now that we have the buffers big enough, construct shadow volume mesh
			memcpy(verts, castmesh->verts, castmesh->numverts * sizeof(float[4]));
			R_Shadow_ProjectVertices(verts, verts + castmesh->numverts * 4, castmesh->numverts, e->origin, e->lightradius);
			R_Shadow_MakeTriangleShadowFlags(castmesh->elements, verts, castmesh->numtriangles, trianglefacinglight, e->origin, e->lightradius);
			tris = R_Shadow_BuildShadowVolumeTriangles(castmesh->elements, castmesh->neighbors, castmesh->numtriangles, castmesh->numverts, trianglefacinglight, shadowelements);
			// add the constructed shadow volume mesh
			Mod_ShadowMesh_AddMesh(loadmodel->mempool, e->shadowvolume, castmesh->numverts, verts, tris, shadowelements);
		}
		// we're done with castmesh now
		Mod_ShadowMesh_Free(castmesh);
		}
#else
		// make a shadow volume mesh
		if (verts == NULL && maxverts > 0)
			verts = Mem_Alloc(loadmodel->mempool, maxverts * sizeof(float[4]));
		for (j = 0;j < e->numsurfaces;j++)
		{
			surf = e->surfaces[j];
			if (surf->castshadow != castshadowcount)
				continue;
			projectdistance = 1000000.0f;//e->lightradius;
			// copy the original polygon, for the front cap of the volume
			for (k = 0, v0 = surf->poly_verts, v1 = verts;k < surf->poly_numverts;k++, v0 += 3, v1 += 3)
				VectorCopy(v0, v1);
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, surf->poly_numverts, verts);
			// project the original polygon, reversed, for the back cap of the volume
			for (k = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = verts;k < surf->poly_numverts;k++, v0 -= 3, v1 += 3)
			{
				VectorSubtract(v0, e->origin, temp);
				//VectorNormalize(temp);
				VectorMA(v0, projectdistance, temp, v1);
			}
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, surf->poly_numverts, verts);
			// project the shadow volume sides
			for (l = surf->poly_numverts - 1, k = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = surf->poly_verts;k < surf->poly_numverts;l = k, k++, v0 = v1, v1 += 3)
			{
				if (surf->neighborsurfaces == NULL || surf->neighborsurfaces[l] == NULL || surf->neighborsurfaces[l]->castshadow != castshadowcount)
				{
					VectorCopy(v1, &verts[0]);
					VectorCopy(v0, &verts[3]);
					VectorCopy(v0, &verts[6]);
					VectorCopy(v1, &verts[9]);
					VectorSubtract(&verts[6], e->origin, temp);
					//VectorNormalize(temp);
					VectorMA(&verts[6], projectdistance, temp, &verts[6]);
					VectorSubtract(&verts[9], e->origin, temp);
					//VectorNormalize(temp);
					VectorMA(&verts[9], projectdistance, temp, &verts[9]);

#if 0
					VectorSubtract(&verts[0], &verts[3], temp);
					VectorSubtract(&verts[6], &verts[3], temp2);
					CrossProduct(temp, temp2, temp3);
					VectorNormalize(temp3);
					if (DotProduct(surf->poly_center, temp3) > DotProduct(&verts[0], temp3))
					{
						VectorCopy(v0, &verts[0]);
						VectorCopy(v1, &verts[3]);
						VectorCopy(v1, &verts[6]);
						VectorCopy(v0, &verts[9]);
						VectorSubtract(&verts[6], e->origin, temp);
						//VectorNormalize(temp);
						VectorMA(&verts[6], projectdistance, temp, &verts[6]);
						VectorSubtract(&verts[9], e->origin, temp);
						//VectorNormalize(temp);
						VectorMA(&verts[9], projectdistance, temp, &verts[9]);
						Con_Printf("flipped shadow volume edge %8p %i\n", surf, l);
					}
#endif

					Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
				}
			}
		}
#endif
#endif
		e->shadowvolume = Mod_ShadowMesh_Finish(loadmodel->mempool, e->shadowvolume);
		for (l = 0, mesh = e->shadowvolume;mesh;mesh = mesh->next)
			l += mesh->numtriangles;
		Con_Printf("static shadow volume built containing %i triangles\n", l);
	}
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

void R_Shadow_SelectLightInView(void)
{
	float bestrating, rating, temp[3], dist;
	worldlight_t *best, *light;
	best = NULL;
	bestrating = 1e30;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		VectorSubtract(light->origin, r_refdef.vieworg, temp);
		dist = sqrt(DotProduct(temp, temp));
		if (DotProduct(temp, vpn) >= 0.97 * dist && bestrating > dist && CL_TraceLine(light->origin, r_refdef.vieworg, NULL, NULL, 0, true, NULL) == 1.0f)
		{
			bestrating = dist;
			best = light;
		}
	}
	R_Shadow_SelectLight(best);
}

void R_Shadow_LoadWorldLights(const char *mapname)
{
	int n, a, style;
	char name[MAX_QPATH], cubemapname[MAX_QPATH], *lightsstring, *s, *t;
	float origin[3], radius, color[3];
	COM_StripExtension(mapname, name);
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

void R_Shadow_SaveWorldLights(const char *mapname)
{
	worldlight_t *light;
	int bufchars, bufmaxchars;
	char *buf, *oldbuf;
	char name[MAX_QPATH];
	char line[1024];
	if (!r_shadow_worldlightchain)
		return;
	COM_StripExtension(mapname, name);
	strcat(name, ".rtlights");
	bufchars = bufmaxchars = 0;
	buf = NULL;
	for (light = r_shadow_worldlightchain;light;light = light->next)
	{
		sprintf(line, "%g %g %g %g %g %g %g %d %s\n", light->origin[0], light->origin[1], light->origin[2], light->lightradius, light->light[0], light->light[1], light->light[2], light->style, light->cubemapname ? light->cubemapname : "");
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

extern void R_DrawCrosshairSprite(rtexture_t *texture, vec3_t origin, vec_t scale, float cr, float cg, float cb, float ca);
void R_Shadow_DrawCursorCallback(const void *calldata1, int calldata2)
{
	cachepic_t *pic;
	pic = Draw_CachePic("gfx/crosshair1.tga");
	if (pic)
		R_DrawCrosshairSprite(pic->tex, r_editlights_cursorlocation, r_editlights_cursorgrid.value * 0.5f, 1, 1, 1, 1);
}

void R_Shadow_DrawCursor(void)
{
	R_MeshQueue_AddTransparent(r_editlights_cursorlocation, R_Shadow_DrawCursorCallback, NULL, 0);
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
	if (r_editlights.integer)
	{
		R_Shadow_SelectLightInView();
		R_Shadow_SetCursorLocationForView();
		R_Shadow_DrawCursor();
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
	if (cl.worldmodel)
	{
		R_Shadow_ClearWorldLights();
		R_Shadow_LoadWorldLights(cl.worldmodel->name);
	}
}

void R_Shadow_EditLights_Save_f(void)
{
	if (cl.worldmodel)
		R_Shadow_SaveWorldLights(cl.worldmodel->name);
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
	Cmd_AddCommand("r_editlights_clear", R_Shadow_EditLights_Clear_f);
	Cmd_AddCommand("r_editlights_reload", R_Shadow_EditLights_Reload_f);
	Cmd_AddCommand("r_editlights_save", R_Shadow_EditLights_Save_f);
	Cmd_AddCommand("r_editlights_spawn", R_Shadow_EditLights_Spawn_f);
	Cmd_AddCommand("r_editlights_edit", R_Shadow_EditLights_Edit_f);
	Cmd_AddCommand("r_editlights_remove", R_Shadow_EditLights_Remove_f);
}
