
#include "quakedef.h"
#include "r_shadow.h"

#define SHADOWSTAGE_NONE 0
#define SHADOWSTAGE_STENCIL 1
#define SHADOWSTAGE_LIGHT 2
#define SHADOWSTAGE_ERASESTENCIL 3

int r_shadowstage = SHADOWSTAGE_NONE;

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

cvar_t r_shadow1 = {0, "r_shadow1", "2"};
cvar_t r_shadow2 = {0, "r_shadow2", "0"};
cvar_t r_shadow3 = {0, "r_shadow3", "32768"};
cvar_t r_shadow4 = {0, "r_shadow4", "0"};
cvar_t r_shadow5 = {0, "r_shadow5", "0"};
cvar_t r_shadow6 = {0, "r_shadow6", "0"};
cvar_t r_light_realtime = {0, "r_light_realtime", "0"};
cvar_t r_light_quality = {0, "r_light_quality", "1"};
cvar_t r_light_gloss = {0, "r_light_gloss", "0"};
cvar_t r_light_debuglight = {0, "r_light_debuglight", "-1"};

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
	r_shadow_texturepool = NULL;
}

void r_shadow_shutdown(void)
{
	r_shadow_normalsattenuationtexture = NULL;
	r_shadow_normalscubetexture = NULL;
	r_shadow_attenuation2dtexture = NULL;
	r_shadow_blankbumptexture = NULL;
	R_FreeTexturePool(&r_shadow_texturepool);
	maxshadowelements = 0;
	shadowelements = NULL;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	Mem_FreePool(&r_shadow_mempool);
}

void r_shadow_newmap(void)
{
}

void R_Shadow_Init(void)
{
	Cvar_RegisterVariable(&r_shadow1);
	Cvar_RegisterVariable(&r_shadow2);
	Cvar_RegisterVariable(&r_shadow3);
	Cvar_RegisterVariable(&r_shadow4);
	Cvar_RegisterVariable(&r_shadow5);
	Cvar_RegisterVariable(&r_shadow6);
	Cvar_RegisterVariable(&r_light_realtime);
	Cvar_RegisterVariable(&r_light_quality);
	Cvar_RegisterVariable(&r_light_gloss);
	Cvar_RegisterVariable(&r_light_debuglight);
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

void R_Shadow_Volume(int numverts, int numtris, float *vertex, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance)
{
	int i, *e, *n, *out, tris;
	float *v0, *v1, *v2, temp[3], f;
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
// an extrusion of the backfaces, beginning at the original geometry and
// ending further from the light source than the original geometry
// (presumably at least as far as the light's radius, if the light has a
// radius at all), capped at both front and back to avoid any problems
//
// description:
// draws the shadow volumes of the model.
// requirements:
// vertex loations must already be in vertex before use.
// vertex must have capacity for numverts * 2.

	// make sure trianglefacinglight is big enough for this volume
	if (maxtrianglefacinglight < numtris)
	{
		maxtrianglefacinglight = numtris;
		if (trianglefacinglight)
			Mem_Free(trianglefacinglight);
		trianglefacinglight = Mem_Alloc(r_shadow_mempool, maxtrianglefacinglight);
	}

	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
	{
		maxshadowelements = numtris * 24;
		if (shadowelements)
			Mem_Free(shadowelements);
		shadowelements = Mem_Alloc(r_shadow_mempool, maxshadowelements * sizeof(int));
	}

	// make projected vertices
	// by clever use of elements we'll construct the whole shadow from
	// the unprojected vertices and these projected vertices
	for (i = 0, v0 = vertex, v1 = vertex + numverts * 4;i < numverts;i++, v0 += 4, v1 += 4)
	{
#if 1
		v1[0] = v0[0] + 50.0f * (v0[0] - relativelightorigin[0]);
		v1[1] = v0[1] + 50.0f * (v0[1] - relativelightorigin[1]);
		v1[2] = v0[2] + 50.0f * (v0[2] - relativelightorigin[2]);
#elif 0
		VectorSubtract(v0, relativelightorigin, temp);
		f = lightradius / sqrt(DotProduct(temp,temp));
		if (f < 1)
			f = 1;
		VectorMA(relativelightorigin, f, temp, v1);
#else
		VectorSubtract(v0, relativelightorigin, temp);
		f = projectdistance / sqrt(DotProduct(temp,temp));
		VectorMA(v0, f, temp, v1);
#endif
	}

	// check which triangles are facing the light
	for (i = 0, e = elements;i < numtris;i++, e += 3)
	{
		// calculate triangle facing flag
		v0 = vertex + e[0] * 4;
		v1 = vertex + e[1] * 4;
		v2 = vertex + e[2] * 4;
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
		float dir0[3], dir1[3];

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

	// output triangle elements
	out = shadowelements;
	tris = 0;

	// check each backface for bordering frontfaces,
	// and cast shadow polygons from those edges,
	// also create front and back caps for shadow volume
	for (i = 0, e = elements, n = neighbors;i < numtris;i++, e += 3, n += 3)
	{
		if (!trianglefacinglight[i])
		{
			// triangle is backface and therefore casts shadow,
			// output front and back caps for shadow volume
#if 1
			// front cap (with flipped winding order)
			out[0] = e[0];
			out[1] = e[2];
			out[2] = e[1];
			// rear cap
			out[3] = e[0] + numverts;
			out[4] = e[1] + numverts;
			out[5] = e[2] + numverts;
			out += 6;
			tris += 2;
#else if 1
			// rear cap
			out[0] = e[0] + numverts;
			out[1] = e[1] + numverts;
			out[2] = e[2] + numverts;
			out += 3;
			tris += 1;
#endif
			// check the edges
			if (n[0] < 0 || trianglefacinglight[n[0]])
			{
				out[0] = e[0];
				out[1] = e[1];
				out[2] = e[1] + numverts;
				out[3] = e[0];
				out[4] = e[1] + numverts;
				out[5] = e[0] + numverts;
				out += 6;
				tris += 2;
			}
			if (n[1] < 0 || trianglefacinglight[n[1]])
			{
				out[0] = e[1];
				out[1] = e[2];
				out[2] = e[2] + numverts;
				out[3] = e[1];
				out[4] = e[2] + numverts;
				out[5] = e[1] + numverts;
				out += 6;
				tris += 2;
			}
			if (n[2] < 0 || trianglefacinglight[n[2]])
			{
				out[0] = e[2];
				out[1] = e[0];
				out[2] = e[0] + numverts;
				out[3] = e[2];
				out[4] = e[0] + numverts;
				out[5] = e[2] + numverts;
				out += 6;
				tris += 2;
			}
		}
	}
	R_Shadow_RenderVolume(numverts * 2, tris, shadowelements);
}

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements)
{
	if (!numverts || !numtris)
		return;
	// draw the volume
	if (r_shadowstage == SHADOWSTAGE_STENCIL)
	{
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		R_Mesh_Draw(numverts, numtris, elements);
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		R_Mesh_Draw(numverts, numtris, elements);
	}
	else
		R_Mesh_Draw(numverts, numtris, elements);
}

float r_shadow_atten1, r_shadow_atten2, r_shadow_atten5;
#define ATTEN3DSIZE 64
static void R_Shadow_Make3DTextures(void)
{
	int x, y, z;
	float v[3], intensity, ilen, bordercolor[4];
	qbyte data[ATTEN3DSIZE][ATTEN3DSIZE][ATTEN3DSIZE][4];
	if (r_light_quality.integer != 1 || !gl_texture3d)
		return;
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
				data[z][y][x][0] = 128.0f + ilen * v[0];
				data[z][y][x][1] = 128.0f + ilen * v[1];
				data[z][y][x][2] = 128.0f + ilen * v[2];
				data[z][y][x][3] = 255;
			}
		}
	}
	r_shadow_normalsattenuationtexture = R_LoadTexture3D(r_shadow_texturepool, "normalsattenuation", ATTEN3DSIZE, ATTEN3DSIZE, ATTEN3DSIZE, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
	bordercolor[0] = 0.5f;
	bordercolor[1] = 0.5f;
	bordercolor[2] = 0.5f;
	bordercolor[3] = 1.0f;
	qglTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, bordercolor);
}

static void R_Shadow_MakeTextures(void)
{
	int x, y, d, side;
	float v[3], s, t, intensity;
	qbyte data[6][128][128][4];
	R_FreeTexturePool(&r_shadow_texturepool);
	r_shadow_texturepool = R_AllocTexturePool();
	r_shadow_atten1 = r_shadow1.value;
	r_shadow_atten2 = r_shadow2.value;
	r_shadow_atten5 = r_shadow5.value;
	for (y = 0;y < 128;y++)
	{
		for (x = 0;x < 128;x++)
		{
			data[0][y][x][0] = 128;
			data[0][y][x][1] = 128;
			data[0][y][x][2] = 255;
			data[0][y][x][3] = 255;
		}
	}
	r_shadow_blankbumptexture = R_LoadTexture2D(r_shadow_texturepool, "blankbump", 128, 128, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
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
				data[side][y][x][0] = 128.0f + intensity * v[0];
				data[side][y][x][1] = 128.0f + intensity * v[1];
				data[side][y][x][2] = 128.0f + intensity * v[2];
				data[side][y][x][3] = 255;
			}
		}
	}
	r_shadow_normalscubetexture = R_LoadTextureCubeMap(r_shadow_texturepool, "normalscube", 128, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
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
			data[0][y][x][0] = d;
			data[0][y][x][1] = d;
			data[0][y][x][2] = d;
			data[0][y][x][3] = d;
		}
	}
	r_shadow_attenuation2dtexture = R_LoadTexture2D(r_shadow_texturepool, "attenuation2d", 128, 128, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_ALPHA | TEXF_MIPMAP, NULL);
	R_Shadow_Make3DTextures();
}

void R_Shadow_Stage_Begin(void)
{
	rmeshstate_t m;

	if (r_light_quality.integer == 1 && !gl_texture3d)
	{
		Con_Printf("3D texture support not detected, falling back on slower 2D + 1D + normalization lighting\n");
		Cvar_SetValueQuick(&r_light_quality, 0);
	}
	//cl.worldmodel->numlights = min(cl.worldmodel->numlights, 1);
	if (!r_shadow_attenuation2dtexture
	 || (r_light_quality.integer == 1 && !r_shadow_normalsattenuationtexture)
	 || r_shadow1.value != r_shadow_atten1
	 || r_shadow2.value != r_shadow_atten2
	 || r_shadow5.value != r_shadow_atten5)
		R_Shadow_MakeTextures();

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
	qglClearStencil(0);
	qglClear(GL_STENCIL_BUFFER_BIT);
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

void R_Shadow_Stage_EraseShadowVolumes(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	R_Mesh_TextureState(&m);
	GL_Color(1, 1, 1, 1);
	qglColorMask(0, 0, 0, 0);
	qglDisable(GL_BLEND);
	qglDepthMask(0);
	qglDepthFunc(GL_LESS);
	qglClearStencil(0);
	qglClear(GL_STENCIL_BUFFER_BIT);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_ZERO, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_NOTEQUAL, 0, 0xFF);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_DEPTH_TEST);
	r_shadowstage = SHADOWSTAGE_ERASESTENCIL;
}

void R_Shadow_Stage_End(void)
{
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
	float lightdir[3], eyedir[3], halfdir[3], lightdirlen, ilen;
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

void R_Shadow_RenderLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, rtexture_t *basetexture, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap)
{
	int mult;
	float scale, colorscale;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	if (!bumptexture)
		bumptexture = r_shadow_blankbumptexture;
	// colorscale accounts for how much we multiply the brightness during combine
	if (r_light_quality.integer == 1)
	{
		if (r_textureunits.integer >= 4)
			colorscale = r_colorscale * 0.125f / r_shadow3.value;
		else
			colorscale = r_colorscale * 0.5f / r_shadow3.value;
	}
	else
		colorscale = r_colorscale * 0.5f / r_shadow3.value;
	// limit mult to 64 for sanity sake
	for (mult = 1, scale = ixtable[mult];mult < 64 && (lightcolor[0] * scale * colorscale > 1 || lightcolor[1] * scale * colorscale > 1 || lightcolor[2] * scale * colorscale > 1);mult++, scale = ixtable[mult]);
	colorscale *= scale;
	for (;mult > 0;mult--)
	{
		if (r_light_quality.integer == 1)
		{
			if (r_textureunits.integer >= 4)
			{
				// 4 texture 3D path, two pass
				m.tex[0] = R_GetTexture(bumptexture);
				m.tex3d[1] = R_GetTexture(r_shadow_normalsattenuationtexture);
				m.tex[2] = R_GetTexture(basetexture);
				m.texcubemap[3] = R_GetTexture(lightcubemap);
				m.tex[3] = R_GetTexture(r_notexture);
				m.texcombinergb[0] = GL_REPLACE;
				m.texcombinergb[1] = GL_DOT3_RGB_ARB;
				m.texcombinergb[2] = GL_MODULATE;
				m.texcombinergb[3] = GL_MODULATE;
				m.texrgbscale[1] = 2;
				m.texrgbscale[3] = 4;
				R_Mesh_TextureState(&m);
				GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
				memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
				memcpy(varray_texcoord[2], texcoords, numverts * sizeof(float[4]));
				if (lightcubemap)
					R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[3], numverts, varray_vertex, relativelightorigin);
				else
				{
					qglActiveTexture(GL_TEXTURE3_ARB);
					qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
				}
				R_Shadow_GenTexCoords_Diffuse_Attenuation3D(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, lightradius);
				R_Mesh_Draw(numverts, numtriangles, elements);
				if (!lightcubemap)
				{
					qglActiveTexture(GL_TEXTURE3_ARB);
					qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
				}
				if (r_light_gloss.integer && glosstexture)
				{
					m.tex[2] = R_GetTexture(glosstexture);
					R_Mesh_TextureState(&m);
					R_Shadow_GenTexCoords_Specular_Attenuation3D(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin, relativeeyeorigin, lightradius);
					if (!lightcubemap)
					{
						qglActiveTexture(GL_TEXTURE3_ARB);
						qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
					}
					R_Mesh_Draw(numverts, numtriangles, elements);
					if (!lightcubemap)
					{
						qglActiveTexture(GL_TEXTURE3_ARB);
						qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
					}
				}
			}
			else
			{
				// 2 texture 3D path, four pass
				m.tex[0] = R_GetTexture(bumptexture);
				m.tex3d[1] = R_GetTexture(r_shadow_normalsattenuationtexture);
				m.texcombinergb[1] = GL_DOT3_RGBA_ARB;
				m.texalphascale[1] = 2;
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
				m.texcombinergb[1] = GL_MODULATE;
				m.texrgbscale[1] = 1;
				m.texalphascale[1] = 1;
				R_Mesh_TextureState(&m);
				qglColorMask(1,1,1,1);
				qglBlendFunc(GL_DST_ALPHA, GL_ONE);
				qglEnable(GL_BLEND);
				GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
				if (lightcubemap)
					R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);
				R_Mesh_Draw(numverts, numtriangles, elements);
			}
		}
		else
		{
			// 2 texture no3D path, six pass
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
			m.texalphascale[1] = 2;
			R_Mesh_TextureState(&m);
			qglBlendFunc(GL_DST_ALPHA, GL_ZERO);
			qglEnable(GL_BLEND);
			memcpy(varray_texcoord[0], texcoords, numverts * sizeof(float[4]));
			R_Shadow_GenTexCoords_Diffuse_NormalCubeMap(varray_texcoord[1], numverts, varray_vertex, svectors, tvectors, normals, relativelightorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);

			m.tex[0] = R_GetTexture(basetexture);
			m.texcubemap[1] = R_GetTexture(lightcubemap);
			m.texcombinergb[1] = GL_MODULATE;
			m.texrgbscale[1] = 1;
			m.texalphascale[1] = 1;
			R_Mesh_TextureState(&m);
			qglColorMask(1,1,1,1);
			qglBlendFunc(GL_DST_ALPHA, GL_ONE);
			GL_Color(lightcolor[0] * colorscale, lightcolor[1] * colorscale, lightcolor[2] * colorscale, 1);
			if (lightcubemap)
				R_Shadow_GenTexCoords_LightCubeMap(varray_texcoord[1], numverts, varray_vertex, relativelightorigin);
			R_Mesh_Draw(numverts, numtriangles, elements);
		}
	}
}

