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
#include "r_shadow.h"
#include "polygon.h"

mempool_t *r_main_mempool;
rtexturepool_t *r_main_texturepool;

// used for dlight push checking and other things
int r_framecount;

mplane_t frustum[5];

renderstats_t renderstats;

// true during envmap command capture
qboolean envmap;

// maximum visible distance (recalculated from world box each frame)
float r_farclip;
// brightness of world lightmaps and related lighting
// (often reduced when world rtlights are enabled)
float r_lightmapintensity;
// whether to draw world lights realtime, dlights realtime, and their shadows
qboolean r_rtworld;
qboolean r_rtworldshadows;
qboolean r_rtdlight;
qboolean r_rtdlightshadows;


// view origin
vec3_t r_vieworigin;
vec3_t r_viewforward;
vec3_t r_viewleft;
vec3_t r_viewright;
vec3_t r_viewup;
int r_view_x;
int r_view_y;
int r_view_z;
int r_view_width;
int r_view_height;
int r_view_depth;
matrix4x4_t r_view_matrix;
float r_polygonfactor;
float r_polygonoffset;
float r_shadowpolygonfactor;
float r_shadowpolygonoffset;
//
// screen size info
//
refdef_t r_refdef;

cvar_t r_nearclip = {0, "r_nearclip", "1", "distance from camera of nearclip plane" };
cvar_t r_showsurfaces = {0, "r_showsurfaces", "0", "shows surfaces as different colors"};
cvar_t r_showtris = {0, "r_showtris", "0", "shows triangle outlines, value controls brightness (can be above 1)"};
cvar_t r_shownormals = {0, "r_shownormals", "0", "shows per-vertex surface normals and tangent vectors for bumpmapped lighting"};
cvar_t r_showlighting = {0, "r_showlighting", "0", "shows areas lit by lights, useful for finding out why some areas of a map render slowly (bright orange = lots of passes = slow), a value of 2 disables depth testing which can be interesting but not very useful"};
cvar_t r_showshadowvolumes = {0, "r_showshadowvolumes", "0", "shows areas shadowed by lights, useful for finding out why some areas of a map render slowly (bright blue = lots of passes = slow), a value of 2 disables depth testing which can be interesting but not very useful"};
cvar_t r_showcollisionbrushes = {0, "r_showcollisionbrushes", "0", "draws collision brushes in quake3 maps (mode 1), mode 2 disables rendering of world (trippy!)"};
cvar_t r_showcollisionbrushes_polygonfactor = {0, "r_showcollisionbrushes_polygonfactor", "-1", "expands outward the brush polygons a little bit, used to make collision brushes appear infront of walls"};
cvar_t r_showcollisionbrushes_polygonoffset = {0, "r_showcollisionbrushes_polygonoffset", "0", "nudges brush polygon depth in hardware depth units, used to make collision brushes appear infront of walls"};
cvar_t r_showdisabledepthtest = {0, "r_showdisabledepthtest", "0", "disables depth testing on r_show* cvars, allowing you to see what hidden geometry the graphics card is processing\n"};
cvar_t r_drawentities = {0, "r_drawentities","1", "draw entities (doors, players, projectiles, etc)"};
cvar_t r_drawviewmodel = {0, "r_drawviewmodel","1", "draw your weapon model"};
cvar_t r_speeds = {0, "r_speeds","0", "displays rendering statistics and per-subsystem timings"};
cvar_t r_fullbright = {0, "r_fullbright","0", "make everything bright cheat (not allowed in multiplayer)"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1", "opacity of water polygons"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1", "enables dynamic lights (rocket glow and such)"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1", "enables glowing pixels in quake textures (changes need r_restart to take effect)"};
cvar_t r_q1bsp_skymasking = {0, "r_qb1sp_skymasking", "1", "allows sky polygons in quake1 maps to obscure other geometry"};

cvar_t gl_fogenable = {0, "gl_fogenable", "0", "nehahra fog enable (for Nehahra compatibility only)"};
cvar_t gl_fogdensity = {0, "gl_fogdensity", "0.25", "nehahra fog density (recommend values below 0.1) (for Nehahra compatibility only)"};
cvar_t gl_fogred = {0, "gl_fogred","0.3", "nehahra fog color red value (for Nehahra compatibility only)"};
cvar_t gl_foggreen = {0, "gl_foggreen","0.3", "nehahra fog color green value (for Nehahra compatibility only)"};
cvar_t gl_fogblue = {0, "gl_fogblue","0.3", "nehahra fog color blue value (for Nehahra compatibility only)"};
cvar_t gl_fogstart = {0, "gl_fogstart", "0", "nehahra fog start distance (for Nehahra compatibility only)"};
cvar_t gl_fogend = {0, "gl_fogend","0", "nehahra fog end distance (for Nehahra compatibility only)"};

cvar_t r_textureunits = {0, "r_textureunits", "32", "number of hardware texture units reported by driver (note: setting this to 1 turns off gl_combine)"};

cvar_t r_glsl = {0, "r_glsl", "1", "enables use of OpenGL 2.0 pixel shaders for lighting"};
cvar_t r_glsl_offsetmapping = {0, "r_glsl_offsetmapping", "0", "offset mapping effect (also known as parallax mapping or virtual displacement mapping)"};
cvar_t r_glsl_offsetmapping_reliefmapping = {0, "r_glsl_offsetmapping_reliefmapping", "0", "relief mapping effect (higher quality)"};
cvar_t r_glsl_offsetmapping_scale = {0, "r_glsl_offsetmapping_scale", "0.04", "how deep the offset mapping effect is"};
cvar_t r_glsl_deluxemapping = {0, "r_glsl_deluxemapping", "1", "use per pixel lighting on deluxemap-compiled q3bsp maps (or a value of 2 forces deluxemap shading even without deluxemaps)"};

cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "1", "enables animation smoothing on sprites (requires r_lerpmodels 1)"};
cvar_t r_lerpmodels = {CVAR_SAVE, "r_lerpmodels", "1", "enables animation smoothing on models"};
cvar_t r_waterscroll = {CVAR_SAVE, "r_waterscroll", "1", "makes water scroll around, value controls how much"};

cvar_t r_bloom = {CVAR_SAVE, "r_bloom", "0", "enables bloom effect (makes bright pixels affect neighboring pixels)"};
cvar_t r_bloom_intensity = {CVAR_SAVE, "r_bloom_intensity", "1.5", "how bright the glow is"};
cvar_t r_bloom_blur = {CVAR_SAVE, "r_bloom_blur", "4", "how large the glow is"};
cvar_t r_bloom_resolution = {CVAR_SAVE, "r_bloom_resolution", "320", "what resolution to perform the bloom effect at (independent of screen resolution)"};
cvar_t r_bloom_power = {CVAR_SAVE, "r_bloom_power", "2", "how much to darken the image before blurring to make the bloom effect"};

cvar_t r_smoothnormals_areaweighting = {0, "r_smoothnormals_areaweighting", "1", "uses significantly faster (and supposedly higher quality) area-weighted vertex normals and tangent vectors rather than summing normalized triangle normals and tangents"};

cvar_t developer_texturelogging = {0, "developer_texturelogging", "0", "produces a textures.log file containing names of skins and map textures the engine tried to load"};

cvar_t gl_lightmaps = {0, "gl_lightmaps", "0", "draws only lightmaps, no texture (for level designers)"};

cvar_t r_test = {0, "r_test", "0", "internal development use only, leave it alone (usually does nothing anyway)"}; // used for testing renderer code changes, otherwise does nothing

rtexture_t *r_bloom_texture_screen;
rtexture_t *r_bloom_texture_bloom;
rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_black;
rtexture_t *r_texture_notexture;
rtexture_t *r_texture_whitecube;
rtexture_t *r_texture_normalizationcube;
rtexture_t *r_texture_fogattenuation;
//rtexture_t *r_texture_fogintensity;

// information about each possible shader permutation
r_glsl_permutation_t r_glsl_permutations[SHADERPERMUTATION_COUNT];
// currently selected permutation
r_glsl_permutation_t *r_glsl_permutation;

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

vec3_t fogcolor;
vec_t fogdensity;
vec_t fogrange;
vec_t fograngerecip;
int fogtableindex;
vec_t fogtabledistmultiplier;
float fogtable[FOGTABLEWIDTH];
float fog_density, fog_red, fog_green, fog_blue;
qboolean fogenabled;
qboolean oldgl_fogenable;
void R_UpdateFog(void)
{
	if (gamemode == GAME_NEHAHRA)
	{
		if (gl_fogenable.integer)
		{
			oldgl_fogenable = true;
			fog_density = gl_fogdensity.value;
			fog_red = gl_fogred.value;
			fog_green = gl_foggreen.value;
			fog_blue = gl_fogblue.value;
		}
		else if (oldgl_fogenable)
		{
			oldgl_fogenable = false;
			fog_density = 0;
			fog_red = 0;
			fog_green = 0;
			fog_blue = 0;
		}
	}
	if (fog_density)
	{
		fogcolor[0] = fog_red   = bound(0.0f, fog_red  , 1.0f);
		fogcolor[1] = fog_green = bound(0.0f, fog_green, 1.0f);
		fogcolor[2] = fog_blue  = bound(0.0f, fog_blue , 1.0f);
	}
	if (fog_density)
	{
		fogenabled = true;
		fogdensity = -4000.0f / (fog_density * fog_density);
		// this is the point where the fog reaches 0.9986 alpha, which we
		// consider a good enough cutoff point for the texture
		// (0.9986 * 256 == 255.6)
		fogrange = 400 / fog_density;
		fograngerecip = 1.0f / fogrange;
		fogtabledistmultiplier = FOGTABLEWIDTH * fograngerecip;
		// fog color was already set
	}
	else
		fogenabled = false;
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
	fog_density = fog_red = fog_green = fog_blue = 0.0f;
}

// FIXME: move this to client?
void FOG_registercvars(void)
{
	int x;
	double r, alpha;

	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_RegisterVariable (&gl_fogenable);
		Cvar_RegisterVariable (&gl_fogdensity);
		Cvar_RegisterVariable (&gl_fogred);
		Cvar_RegisterVariable (&gl_foggreen);
		Cvar_RegisterVariable (&gl_fogblue);
		Cvar_RegisterVariable (&gl_fogstart);
		Cvar_RegisterVariable (&gl_fogend);
	}

	r = (-1.0/256.0) * (FOGTABLEWIDTH * FOGTABLEWIDTH);
	for (x = 0;x < FOGTABLEWIDTH;x++)
	{
		alpha = exp(r / ((double)x*(double)x));
		if (x == FOGTABLEWIDTH - 1)
			alpha = 1;
		fogtable[x] = bound(0, alpha, 1);
	}
}

static void R_BuildBlankTextures(void)
{
	unsigned char data[4];
	data[0] = 128; // normal X
	data[1] = 128; // normal Y
	data[2] = 255; // normal Z
	data[3] = 128; // height
	r_texture_blanknormalmap = R_LoadTexture2D(r_main_texturepool, "blankbump", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_texture_white = R_LoadTexture2D(r_main_texturepool, "blankwhite", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 255;
	r_texture_black = R_LoadTexture2D(r_main_texturepool, "blankblack", 1, 1, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
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
	r_texture_notexture = R_LoadTexture2D(r_main_texturepool, "notexture", 16, 16, &pix[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP, NULL);
}

static void R_BuildWhiteCube(void)
{
	unsigned char data[6*1*1*4];
	data[ 0] = 255;data[ 1] = 255;data[ 2] = 255;data[ 3] = 255;
	data[ 4] = 255;data[ 5] = 255;data[ 6] = 255;data[ 7] = 255;
	data[ 8] = 255;data[ 9] = 255;data[10] = 255;data[11] = 255;
	data[12] = 255;data[13] = 255;data[14] = 255;data[15] = 255;
	data[16] = 255;data[17] = 255;data[18] = 255;data[19] = 255;
	data[20] = 255;data[21] = 255;data[22] = 255;data[23] = 255;
	r_texture_whitecube = R_LoadTextureCubeMap(r_main_texturepool, "whitecube", 1, data, TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
}

static void R_BuildNormalizationCube(void)
{
	int x, y, side;
	vec3_t v;
	vec_t s, t, intensity;
#define NORMSIZE 64
	unsigned char data[6][NORMSIZE][NORMSIZE][4];
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
				data[side][y][x][0] = (unsigned char)(128.0f + intensity * v[0]);
				data[side][y][x][1] = (unsigned char)(128.0f + intensity * v[1]);
				data[side][y][x][2] = (unsigned char)(128.0f + intensity * v[2]);
				data[side][y][x][3] = 255;
			}
		}
	}
	r_texture_normalizationcube = R_LoadTextureCubeMap(r_main_texturepool, "normalcube", NORMSIZE, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
}

static void R_BuildFogTexture(void)
{
	int x, b;
	double r, alpha;
#define FOGWIDTH 64
	unsigned char data1[FOGWIDTH][4];
	//unsigned char data2[FOGWIDTH][4];
	r = (-1.0/256.0) * (FOGWIDTH * FOGWIDTH);
	for (x = 0;x < FOGWIDTH;x++)
	{
		alpha = exp(r / ((double)x*(double)x));
		if (x == FOGWIDTH - 1)
			alpha = 1;
		b = (int)(256.0 * alpha);
		b = bound(0, b, 255);
		data1[x][0] = 255 - b;
		data1[x][1] = 255 - b;
		data1[x][2] = 255 - b;
		data1[x][3] = 255;
		//data2[x][0] = b;
		//data2[x][1] = b;
		//data2[x][2] = b;
		//data2[x][3] = 255;
	}
	r_texture_fogattenuation = R_LoadTexture2D(r_main_texturepool, "fogattenuation", FOGWIDTH, 1, &data1[0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
	//r_texture_fogintensity = R_LoadTexture2D(r_main_texturepool, "fogintensity", FOGWIDTH, 1, &data2[0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
}

static const char *builtinshaderstring =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"\n"
"// common definitions between vertex shader and fragment shader:\n"
"\n"
"varying vec2 TexCoord;\n"
"varying vec2 TexCoordLightmap;\n"
"\n"
"varying vec3 CubeVector;\n"
"varying vec3 LightVector;\n"
"varying vec3 EyeVector;\n"
"#ifdef USEFOG\n"
"varying vec3 EyeVectorModelSpace;\n"
"#endif\n"
"\n"
"varying vec3 VectorS; // direction of S texcoord (sometimes crudely called tangent)\n"
"varying vec3 VectorT; // direction of T texcoord (sometimes crudely called binormal)\n"
"varying vec3 VectorR; // direction of R texcoord (surface normal)\n"
"\n"
"\n"
"\n"
"\n"
"// vertex shader specific:\n"
"#ifdef VERTEX_SHADER\n"
"\n"
"uniform vec3 LightPosition;\n"
"uniform vec3 EyePosition;\n"
"uniform vec3 LightDir;\n"
"\n"
"// TODO: get rid of tangentt (texcoord2) and use a crossproduct to regenerate it from tangents (texcoord1) and normal (texcoord3)\n"
"\n"
"void main(void)\n"
"{\n"
"	gl_FrontColor = gl_Color;\n"
"	// copy the surface texcoord\n"
"	TexCoord = vec2(gl_TextureMatrix[0] * gl_MultiTexCoord0);\n"
"#if !defined(MODE_LIGHTSOURCE) && !defined(MODE_LIGHTDIRECTION)\n"
"	TexCoordLightmap = vec2(gl_MultiTexCoord4);\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// transform vertex position into light attenuation/cubemap space\n"
"	// (-1 to +1 across the light box)\n"
"	CubeVector = vec3(gl_TextureMatrix[3] * gl_Vertex);\n"
"\n"
"	// transform unnormalized light direction into tangent space\n"
"	// (we use unnormalized to ensure that it interpolates correctly and then\n"
"	//  normalize it per pixel)\n"
"	vec3 lightminusvertex = LightPosition - gl_Vertex.xyz;\n"
"	LightVector.x = dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"	LightVector.x = dot(LightDir, gl_MultiTexCoord1.xyz);\n"
"	LightVector.y = dot(LightDir, gl_MultiTexCoord2.xyz);\n"
"	LightVector.z = dot(LightDir, gl_MultiTexCoord3.xyz);\n"
"#endif\n"
"\n"
"	// transform unnormalized eye direction into tangent space\n"
"#ifndef USEFOG\n"
"	vec3 EyeVectorModelSpace;\n"
"#endif\n"
"	EyeVectorModelSpace = EyePosition - gl_Vertex.xyz;\n"
"	EyeVector.x = dot(EyeVectorModelSpace, gl_MultiTexCoord1.xyz);\n"
"	EyeVector.y = dot(EyeVectorModelSpace, gl_MultiTexCoord2.xyz);\n"
"	EyeVector.z = dot(EyeVectorModelSpace, gl_MultiTexCoord3.xyz);\n"
"\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"	VectorS = gl_MultiTexCoord1.xyz;\n"
"	VectorT = gl_MultiTexCoord2.xyz;\n"
"	VectorR = gl_MultiTexCoord3.xyz;\n"
"#endif\n"
"\n"
"	// transform vertex to camera space, using ftransform to match non-VS\n"
"	// rendering\n"
"	gl_Position = ftransform();\n"
"}\n"
"\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"\n"
"\n"
"// fragment shader specific:\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Color;\n"
"uniform sampler2D Texture_Gloss;\n"
"uniform samplerCube Texture_Cube;\n"
"uniform sampler2D Texture_FogMask;\n"
"uniform sampler2D Texture_Pants;\n"
"uniform sampler2D Texture_Shirt;\n"
"uniform sampler2D Texture_Lightmap;\n"
"uniform sampler2D Texture_Deluxemap;\n"
"uniform sampler2D Texture_Glow;\n"
"\n"
"uniform vec3 LightColor;\n"
"uniform vec3 AmbientColor;\n"
"uniform vec3 DiffuseColor;\n"
"uniform vec3 SpecularColor;\n"
"uniform vec3 Color_Pants;\n"
"uniform vec3 Color_Shirt;\n"
"uniform vec3 FogColor;\n"
"\n"
"uniform float OffsetMapping_Scale;\n"
"uniform float OffsetMapping_Bias;\n"
"uniform float FogRangeRecip;\n"
"\n"
"uniform float AmbientScale;\n"
"uniform float DiffuseScale;\n"
"uniform float SpecularScale;\n"
"uniform float SpecularPower;\n"
"\n"
"void main(void)\n"
"{\n"
"	// apply offsetmapping\n"
"#ifdef USEOFFSETMAPPING\n"
"	vec2 TexCoordOffset = TexCoord;\n"
"#define TexCoord TexCoordOffset\n"
"\n"
"	vec3 eyedir = vec3(normalize(EyeVector));\n"
"	float depthbias = 1.0 - eyedir.z; // should this be a -?\n"
"	depthbias = 1.0 - depthbias * depthbias;\n"
"\n"
"#ifdef USEOFFSETMAPPING_RELIEFMAPPING\n"
"	// 14 sample relief mapping: linear search and then binary search\n"
"	vec3 OffsetVector = vec3(EyeVector.xy * (1.0 / EyeVector.z) * depthbias * OffsetMapping_Scale * vec2(-0.1, 0.1), -0.1);\n"
"	vec3 RT = vec3(TexCoord - OffsetVector.xy * 10.0, 1.0) + OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	if (RT.z > texture2D(Texture_Normal, RT.xy).a) RT += OffsetVector;OffsetVector *= 0.5;RT -= OffsetVector;\n"
"	TexCoord = RT.xy;\n"
"#else\n"
"	// 3 sample offset mapping (only 3 samples because of ATI Radeon 9500-9800/X300 limits)\n"
"	vec2 OffsetVector = vec2((EyeVector.xy * (1.0 / EyeVector.z) * depthbias) * OffsetMapping_Scale * vec2(-0.333, 0.333));\n"
"	//TexCoord += OffsetVector * 3.0;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"	TexCoord -= OffsetVector * texture2D(Texture_Normal, TexCoord).a;\n"
"#endif\n"
"#endif\n"
"\n"
"	// combine the diffuse textures (base, pants, shirt)\n"
"	vec4 color = vec4(texture2D(Texture_Color, TexCoord));\n"
"#ifdef USECOLORMAPPING\n"
"	color.rgb += vec3(texture2D(Texture_Pants, TexCoord)) * Color_Pants + vec3(texture2D(Texture_Shirt, TexCoord)) * Color_Shirt;\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// light source\n"
"\n"
"	// get the surface normal and light normal\n"
"	vec3 surfacenormal = normalize(vec3(texture2D(Texture_Normal, TexCoord)) - 0.5);\n"
"	vec3 diffusenormal = vec3(normalize(LightVector));\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb *= (AmbientScale + DiffuseScale * max(dot(surfacenormal, diffusenormal), 0.0));\n"
"#ifdef USESPECULAR\n"
"	vec3 specularnormal = vec3(normalize(diffusenormal + vec3(normalize(EyeVector))));\n"
"	color.rgb += vec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(max(dot(surfacenormal, specularnormal), 0.0), SpecularPower);\n"
"#endif\n"
"\n"
"#ifdef USECUBEFILTER\n"
"	// apply light cubemap filter\n"
"	//color.rgb *= normalize(CubeVector) * 0.5 + 0.5;//vec3(textureCube(Texture_Cube, CubeVector));\n"
"	color.rgb *= vec3(textureCube(Texture_Cube, CubeVector));\n"
"#endif\n"
"\n"
"	// apply light color\n"
"	color.rgb *= LightColor;\n"
"\n"
"	// apply attenuation\n"
"	//\n"
"	// the attenuation is (1-(x*x+y*y+z*z)) which gives a large bright\n"
"	// center and sharp falloff at the edge, this is about the most efficient\n"
"	// we can get away with as far as providing illumination.\n"
"	//\n"
"	// pow(1-(x*x+y*y+z*z), 4) is far more realistic but needs large lights to\n"
"	// provide significant illumination, large = slow = pain.\n"
"	color.rgb *= max(1.0 - dot(CubeVector, CubeVector), 0.0);\n"
"\n"
"\n"
"\n"
"\n"
"#elif defined(MODE_LIGHTDIRECTION)\n"
"	// directional model lighting\n"
"\n"
"	// get the surface normal and light normal\n"
"	vec3 surfacenormal = normalize(vec3(texture2D(Texture_Normal, TexCoord)) - 0.5);\n"
"	vec3 diffusenormal = vec3(normalize(LightVector));\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb *= AmbientColor + DiffuseColor * max(dot(surfacenormal, diffusenormal), 0.0);\n"
"#ifdef USESPECULAR\n"
"	vec3 specularnormal = vec3(normalize(diffusenormal + vec3(normalize(EyeVector))));\n"
"	color.rgb += vec3(texture2D(Texture_Gloss, TexCoord)) * SpecularColor * pow(max(dot(surfacenormal, specularnormal), 0.0), SpecularPower);\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#elif defined(MODE_LIGHTDIRECTIONMAP_MODELSPACE)\n"
"	// deluxemap lightmapping using light vectors in modelspace (evil q3map2)\n"
"\n"
"	// get the surface normal and light normal\n"
"	vec3 surfacenormal = normalize(vec3(texture2D(Texture_Normal, TexCoord)) - 0.5);\n"
"	vec3 diffusenormal_modelspace = vec3(texture2D(Texture_Deluxemap, TexCoordLightmap)) - 0.5;\n"
"	vec3 diffusenormal = normalize(vec3(dot(diffusenormal_modelspace, VectorS), dot(diffusenormal_modelspace, VectorT), dot(diffusenormal_modelspace, VectorR)));\n"
"\n"
"	// calculate directional shading\n"
"	vec3 tempcolor = color.rgb * (DiffuseScale * max(dot(surfacenormal, diffusenormal), 0.0));\n"
"#ifdef USESPECULAR\n"
"	vec3 specularnormal = vec3(normalize(diffusenormal + vec3(normalize(EyeVector))));\n"
"	tempcolor += vec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(max(dot(surfacenormal, specularnormal), 0.0), SpecularPower);\n"
"#endif\n"
"\n"
"	// apply lightmap color\n"
"	color.rgb = tempcolor * vec3(texture2D(Texture_Lightmap, TexCoordLightmap)) + color.rgb * vec3(AmbientScale);\n"
"\n"
"\n"
"\n"
"\n"
"#elif defined(MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)\n"
"	// deluxemap lightmapping using light vectors in tangentspace\n"
"\n"
"	// get the surface normal and light normal\n"
"	vec3 surfacenormal = normalize(vec3(texture2D(Texture_Normal, TexCoord)) - 0.5);\n"
"	vec3 diffusenormal = normalize(vec3(texture2D(Texture_Deluxemap, TexCoordLightmap)) - 0.5);\n"
"\n"
"	// calculate directional shading\n"
"	vec3 tempcolor = color.rgb * (DiffuseScale * max(dot(surfacenormal, diffusenormal), 0.0));\n"
"#ifdef USESPECULAR\n"
"	vec3 specularnormal = vec3(normalize(diffusenormal + vec3(normalize(EyeVector))));\n"
"	tempcolor += vec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(max(dot(surfacenormal, specularnormal), 0.0), SpecularPower);\n"
"#endif\n"
"\n"
"	// apply lightmap color\n"
"	color.rgb = tempcolor * vec3(texture2D(Texture_Lightmap, TexCoordLightmap)) + color.rgb * vec3(AmbientScale);\n"
"\n"
"\n"
"\n"
"\n"
"#else // MODE none (lightmap)\n"
"	// apply lightmap color\n"
"	color.rgb *= vec3(texture2D(Texture_Lightmap, TexCoordLightmap)) * DiffuseScale + vec3(AmbientScale);\n"
"#endif // MODE\n"
"\n"
"	color *= gl_Color;\n"
"\n"
"#ifdef USEGLOW\n"
"	color.rgb += vec3(texture2D(Texture_Glow, TexCoord));\n"
"#endif\n"
"\n"
"#ifdef USEFOG\n"
"	// apply fog\n"
"	float fog = texture2D(Texture_FogMask, vec2(length(EyeVectorModelSpace)*FogRangeRecip, 0.0)).x;\n"
"	color.rgb = color.rgb * fog + FogColor * (1.0 - fog);\n"
"#endif\n"
"\n"
"	gl_FragColor = color;\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
;

void R_GLSL_CompilePermutation(int permutation)
{
	r_glsl_permutation_t *p = r_glsl_permutations + permutation;
	int vertstrings_count;
	int fragstrings_count;
	char *shaderstring;
	const char *vertstrings_list[SHADERPERMUTATION_COUNT+1];
	const char *fragstrings_list[SHADERPERMUTATION_COUNT+1];
	char permutationname[256];
	if (p->compiled)
		return;
	p->compiled = true;
	vertstrings_list[0] = "#define VERTEX_SHADER\n";
	fragstrings_list[0] = "#define FRAGMENT_SHADER\n";
	vertstrings_count = 1;
	fragstrings_count = 1;
	permutationname[0] = 0;
	if (permutation & SHADERPERMUTATION_MODE_LIGHTSOURCE)
	{
		vertstrings_list[vertstrings_count++] = "#define MODE_LIGHTSOURCE\n";
		fragstrings_list[fragstrings_count++] = "#define MODE_LIGHTSOURCE\n";
		strlcat(permutationname, " lightsource", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_MODELSPACE)
	{
		vertstrings_list[vertstrings_count++] = "#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n";
		fragstrings_list[fragstrings_count++] = "#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n";
		strlcat(permutationname, " lightdirectionmap_modelspace", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_TANGENTSPACE)
	{
		vertstrings_list[vertstrings_count++] = "#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n";
		fragstrings_list[fragstrings_count++] = "#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n";
		strlcat(permutationname, " lightdirectionmap_tangentspace", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_MODE_LIGHTDIRECTION)
	{
		vertstrings_list[vertstrings_count++] = "#define MODE_LIGHTDIRECTION\n";
		fragstrings_list[fragstrings_count++] = "#define MODE_LIGHTDIRECTION\n";
		strlcat(permutationname, " lightdirection", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_GLOW)
	{
		vertstrings_list[vertstrings_count++] = "#define USEGLOW\n";
		fragstrings_list[fragstrings_count++] = "#define USEGLOW\n";
		strlcat(permutationname, " glow", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_COLORMAPPING)
	{
		vertstrings_list[vertstrings_count++] = "#define USECOLORMAPPING\n";
		fragstrings_list[fragstrings_count++] = "#define USECOLORMAPPING\n";
		strlcat(permutationname, " colormapping", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_SPECULAR)
	{
		vertstrings_list[vertstrings_count++] = "#define USESPECULAR\n";
		fragstrings_list[fragstrings_count++] = "#define USESPECULAR\n";
		strlcat(permutationname, " specular", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_FOG)
	{
		vertstrings_list[vertstrings_count++] = "#define USEFOG\n";
		fragstrings_list[fragstrings_count++] = "#define USEFOG\n";
		strlcat(permutationname, " fog", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_CUBEFILTER)
	{
		vertstrings_list[vertstrings_count++] = "#define USECUBEFILTER\n";
		fragstrings_list[fragstrings_count++] = "#define USECUBEFILTER\n";
		strlcat(permutationname, " cubefilter", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_OFFSETMAPPING)
	{
		vertstrings_list[vertstrings_count++] = "#define USEOFFSETMAPPING\n";
		fragstrings_list[fragstrings_count++] = "#define USEOFFSETMAPPING\n";
		strlcat(permutationname, " offsetmapping", sizeof(permutationname));
	}
	if (permutation & SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING)
	{
		vertstrings_list[vertstrings_count++] = "#define USEOFFSETMAPPING_RELIEFMAPPING\n";
		fragstrings_list[fragstrings_count++] = "#define USEOFFSETMAPPING_RELIEFMAPPING\n";
		strlcat(permutationname, " OFFSETMAPPING_RELIEFMAPPING", sizeof(permutationname));
	}
	shaderstring = (char *)FS_LoadFile("glsl/default.glsl", r_main_mempool, false, NULL);
	if (shaderstring)
	{
		Con_DPrintf("GLSL shader text loaded from disk\n");
		vertstrings_list[vertstrings_count++] = shaderstring;
		fragstrings_list[fragstrings_count++] = shaderstring;
	}
	else
	{
		vertstrings_list[vertstrings_count++] = builtinshaderstring;
		fragstrings_list[fragstrings_count++] = builtinshaderstring;
	}
	p->program = GL_Backend_CompileProgram(vertstrings_count, vertstrings_list, fragstrings_count, fragstrings_list);
	if (p->program)
	{
		CHECKGLERROR
		qglUseProgramObjectARB(p->program);
		p->loc_Texture_Normal      = qglGetUniformLocationARB(p->program, "Texture_Normal");
		p->loc_Texture_Color       = qglGetUniformLocationARB(p->program, "Texture_Color");
		p->loc_Texture_Gloss       = qglGetUniformLocationARB(p->program, "Texture_Gloss");
		p->loc_Texture_Cube        = qglGetUniformLocationARB(p->program, "Texture_Cube");
		p->loc_Texture_FogMask     = qglGetUniformLocationARB(p->program, "Texture_FogMask");
		p->loc_Texture_Pants       = qglGetUniformLocationARB(p->program, "Texture_Pants");
		p->loc_Texture_Shirt       = qglGetUniformLocationARB(p->program, "Texture_Shirt");
		p->loc_Texture_Lightmap    = qglGetUniformLocationARB(p->program, "Texture_Lightmap");
		p->loc_Texture_Deluxemap   = qglGetUniformLocationARB(p->program, "Texture_Deluxemap");
		p->loc_Texture_Glow        = qglGetUniformLocationARB(p->program, "Texture_Glow");
		p->loc_FogColor            = qglGetUniformLocationARB(p->program, "FogColor");
		p->loc_LightPosition       = qglGetUniformLocationARB(p->program, "LightPosition");
		p->loc_EyePosition         = qglGetUniformLocationARB(p->program, "EyePosition");
		p->loc_LightColor          = qglGetUniformLocationARB(p->program, "LightColor");
		p->loc_Color_Pants         = qglGetUniformLocationARB(p->program, "Color_Pants");
		p->loc_Color_Shirt         = qglGetUniformLocationARB(p->program, "Color_Shirt");
		p->loc_FogRangeRecip       = qglGetUniformLocationARB(p->program, "FogRangeRecip");
		p->loc_AmbientScale        = qglGetUniformLocationARB(p->program, "AmbientScale");
		p->loc_DiffuseScale        = qglGetUniformLocationARB(p->program, "DiffuseScale");
		p->loc_SpecularPower       = qglGetUniformLocationARB(p->program, "SpecularPower");
		p->loc_SpecularScale       = qglGetUniformLocationARB(p->program, "SpecularScale");
		p->loc_OffsetMapping_Scale = qglGetUniformLocationARB(p->program, "OffsetMapping_Scale");
		p->loc_AmbientColor        = qglGetUniformLocationARB(p->program, "AmbientColor");
		p->loc_DiffuseColor        = qglGetUniformLocationARB(p->program, "DiffuseColor");
		p->loc_SpecularColor       = qglGetUniformLocationARB(p->program, "SpecularColor");
		p->loc_LightDir            = qglGetUniformLocationARB(p->program, "LightDir");
		if (p->loc_Texture_Normal >= 0)    qglUniform1iARB(p->loc_Texture_Normal, 0);
		if (p->loc_Texture_Color >= 0)     qglUniform1iARB(p->loc_Texture_Color, 1);
		if (p->loc_Texture_Gloss >= 0)     qglUniform1iARB(p->loc_Texture_Gloss, 2);
		if (p->loc_Texture_Cube >= 0)      qglUniform1iARB(p->loc_Texture_Cube, 3);
		if (p->loc_Texture_FogMask >= 0)   qglUniform1iARB(p->loc_Texture_FogMask, 4);
		if (p->loc_Texture_Pants >= 0)     qglUniform1iARB(p->loc_Texture_Pants, 5);
		if (p->loc_Texture_Shirt >= 0)     qglUniform1iARB(p->loc_Texture_Shirt, 6);
		if (p->loc_Texture_Lightmap >= 0)  qglUniform1iARB(p->loc_Texture_Lightmap, 7);
		if (p->loc_Texture_Deluxemap >= 0) qglUniform1iARB(p->loc_Texture_Deluxemap, 8);
		if (p->loc_Texture_Glow >= 0)      qglUniform1iARB(p->loc_Texture_Glow, 9);
		qglUseProgramObjectARB(0);
		CHECKGLERROR
	}
	else
		Con_Printf("permutation%s failed for shader %s, some features may not work properly!\n", permutationname, "glsl/default.glsl");
	if (shaderstring)
		Mem_Free(shaderstring);
}

void R_GLSL_Restart_f(void)
{
	int i;
	for (i = 0;i < SHADERPERMUTATION_COUNT;i++)
		if (r_glsl_permutations[i].program)
			GL_Backend_FreeProgram(r_glsl_permutations[i].program);
	memset(r_glsl_permutations, 0, sizeof(r_glsl_permutations));
}

int R_SetupSurfaceShader(const vec3_t lightcolorbase, qboolean modellighting)
{
	// select a permutation of the lighting shader appropriate to this
	// combination of texture, entity, light source, and fogging, only use the
	// minimum features necessary to avoid wasting rendering time in the
	// fragment shader on features that are not being used
	int permutation = 0;
	float specularscale = rsurface_texture->specularscale;
	r_glsl_permutation = NULL;
	if (r_shadow_rtlight)
	{
		permutation |= SHADERPERMUTATION_MODE_LIGHTSOURCE;
		specularscale *= r_shadow_rtlight->specularscale;
		if (r_shadow_rtlight->currentcubemap != r_texture_whitecube)
			permutation |= SHADERPERMUTATION_CUBEFILTER;
	}
	else
	{
		if (!(rsurface_texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT))
		{
			if (modellighting)
				permutation |= SHADERPERMUTATION_MODE_LIGHTDIRECTION;
			else
			{
				if (r_glsl_deluxemapping.integer >= 1 && r_refdef.worldmodel && r_refdef.worldmodel->brushq3.deluxemapping && rsurface_lightmaptexture)
				{
					if (r_refdef.worldmodel->brushq3.deluxemapping_modelspace)
						permutation |= SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_MODELSPACE;
					else
						permutation |= SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
				}
				else if (r_glsl_deluxemapping.integer >= 2) // fake mode
					permutation |= SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			}
		}
		if (rsurface_texture->skin.glow)
			permutation |= SHADERPERMUTATION_GLOW;
	}
	if (specularscale > 0)
		permutation |= SHADERPERMUTATION_SPECULAR;
	if (fogenabled)
		permutation |= SHADERPERMUTATION_FOG;
	if (rsurface_texture->colormapping)
		permutation |= SHADERPERMUTATION_COLORMAPPING;
	if (r_glsl_offsetmapping.integer)
	{
		permutation |= SHADERPERMUTATION_OFFSETMAPPING;
		if (r_glsl_offsetmapping_reliefmapping.integer)
			permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
	}
	if (!r_glsl_permutations[permutation].program)
	{
		if (!r_glsl_permutations[permutation].compiled)
			R_GLSL_CompilePermutation(permutation);
		if (!r_glsl_permutations[permutation].program)
		{
			// remove features until we find a valid permutation
			int i;
			for (i = SHADERPERMUTATION_COUNT-1;;i>>=1)
			{
				// reduce i more quickly whenever it would not remove any bits
				if (permutation < i)
					continue;
				permutation &= i;
				if (!r_glsl_permutations[permutation].compiled)
					R_GLSL_CompilePermutation(permutation);
				if (r_glsl_permutations[permutation].program)
					break;
				if (!i)
					return 0; // utterly failed
			}
		}
	}
	r_glsl_permutation = r_glsl_permutations + permutation;
	CHECKGLERROR
	qglUseProgramObjectARB(r_glsl_permutation->program);CHECKGLERROR
	R_Mesh_TexMatrix(0, &rsurface_texture->currenttexmatrix);
	if (permutation & SHADERPERMUTATION_MODE_LIGHTSOURCE)
	{
		if (r_glsl_permutation->loc_Texture_Cube >= 0 && r_shadow_rtlight) R_Mesh_TexBindCubeMap(3, R_GetTexture(r_shadow_rtlight->currentcubemap));
		if (r_glsl_permutation->loc_LightPosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightPosition, r_shadow_entitylightorigin[0], r_shadow_entitylightorigin[1], r_shadow_entitylightorigin[2]);
		if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
		if (r_glsl_permutation->loc_AmbientScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_AmbientScale, r_shadow_rtlight->ambientscale);
		if (r_glsl_permutation->loc_DiffuseScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_DiffuseScale, r_shadow_rtlight->diffusescale);
		if (r_glsl_permutation->loc_SpecularScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularScale, specularscale);
	}
	else if (permutation & SHADERPERMUTATION_MODE_LIGHTDIRECTION)
	{
		if (rsurface_texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
		{
			if (r_glsl_permutation->loc_AmbientColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_AmbientColor, 1, 1, 1);
			if (r_glsl_permutation->loc_DiffuseColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_DiffuseColor, 0, 0, 0);
			if (r_glsl_permutation->loc_SpecularColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_SpecularColor, 0, 0, 0);
			if (r_glsl_permutation->loc_LightDir >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_LightDir, 0, 0, -1);
		}
		else
		{
			if (r_glsl_permutation->loc_AmbientColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_AmbientColor, rsurface_entity->modellight_ambient[0], rsurface_entity->modellight_ambient[1], rsurface_entity->modellight_ambient[2]);
			if (r_glsl_permutation->loc_DiffuseColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_DiffuseColor, rsurface_entity->modellight_diffuse[0], rsurface_entity->modellight_diffuse[1], rsurface_entity->modellight_diffuse[2]);
			if (r_glsl_permutation->loc_SpecularColor >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_SpecularColor, rsurface_entity->modellight_diffuse[0] * rsurface_texture->specularscale, rsurface_entity->modellight_diffuse[1] * rsurface_texture->specularscale, rsurface_entity->modellight_diffuse[2] * rsurface_texture->specularscale);
			if (r_glsl_permutation->loc_LightDir >= 0)
				qglUniform3fARB(r_glsl_permutation->loc_LightDir, rsurface_entity->modellight_lightdir[0], rsurface_entity->modellight_lightdir[1], rsurface_entity->modellight_lightdir[2]);
		}
	}
	else
	{
		if (r_glsl_permutation->loc_AmbientScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_AmbientScale, r_ambient.value * 2.0f / 128.0f);
		if (r_glsl_permutation->loc_DiffuseScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_DiffuseScale, r_lightmapintensity * 2.0f);
		if (r_glsl_permutation->loc_SpecularScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularScale, r_lightmapintensity * specularscale * 2.0f);
	}
	if (r_glsl_permutation->loc_Texture_Normal >= 0) R_Mesh_TexBind(0, R_GetTexture(rsurface_texture->skin.nmap));
	if (r_glsl_permutation->loc_Texture_Color >= 0) R_Mesh_TexBind(1, R_GetTexture(rsurface_texture->basetexture));
	if (r_glsl_permutation->loc_Texture_Gloss >= 0) R_Mesh_TexBind(2, R_GetTexture(rsurface_texture->glosstexture));
	//if (r_glsl_permutation->loc_Texture_Cube >= 0 && permutation & SHADERPERMUTATION_MODE_LIGHTSOURCE) R_Mesh_TexBindCubeMap(3, R_GetTexture(r_shadow_rtlight->currentcubemap));
	if (r_glsl_permutation->loc_Texture_FogMask >= 0) R_Mesh_TexBind(4, R_GetTexture(r_texture_fogattenuation));
	if (r_glsl_permutation->loc_Texture_Pants >= 0) R_Mesh_TexBind(5, R_GetTexture(rsurface_texture->skin.pants));
	if (r_glsl_permutation->loc_Texture_Shirt >= 0) R_Mesh_TexBind(6, R_GetTexture(rsurface_texture->skin.shirt));
	//if (r_glsl_permutation->loc_Texture_Lightmap >= 0) R_Mesh_TexBind(7, R_GetTexture(r_texture_white));
	//if (r_glsl_permutation->loc_Texture_Deluxemap >= 0) R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
	if (r_glsl_permutation->loc_Texture_Glow >= 0) R_Mesh_TexBind(9, R_GetTexture(rsurface_texture->skin.glow));
	if (r_glsl_permutation->loc_FogColor >= 0)
	{
		// additive passes are only darkened by fog, not tinted
		if (r_shadow_rtlight || (rsurface_texture->currentmaterialflags & MATERIALFLAG_ADD))
			qglUniform3fARB(r_glsl_permutation->loc_FogColor, 0, 0, 0);
		else
			qglUniform3fARB(r_glsl_permutation->loc_FogColor, fogcolor[0], fogcolor[1], fogcolor[2]);
	}
	if (r_glsl_permutation->loc_EyePosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_EyePosition, rsurface_modelorg[0], rsurface_modelorg[1], rsurface_modelorg[2]);
	if (r_glsl_permutation->loc_Color_Pants >= 0)
	{
		if (rsurface_texture->skin.pants)
			qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, rsurface_entity->colormap_pantscolor[0], rsurface_entity->colormap_pantscolor[1], rsurface_entity->colormap_pantscolor[2]);
		else
			qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, 0, 0, 0);
	}
	if (r_glsl_permutation->loc_Color_Shirt >= 0)
	{
		if (rsurface_texture->skin.shirt)
			qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, rsurface_entity->colormap_shirtcolor[0], rsurface_entity->colormap_shirtcolor[1], rsurface_entity->colormap_shirtcolor[2]);
		else
			qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, 0, 0, 0);
	}
	if (r_glsl_permutation->loc_FogRangeRecip >= 0) qglUniform1fARB(r_glsl_permutation->loc_FogRangeRecip, fograngerecip);
	if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularPower, rsurface_texture->specularpower);
	if (r_glsl_permutation->loc_OffsetMapping_Scale >= 0) qglUniform1fARB(r_glsl_permutation->loc_OffsetMapping_Scale, r_glsl_offsetmapping_scale.value);
	CHECKGLERROR
	return permutation;
}

void R_SwitchSurfaceShader(int permutation)
{
	if (r_glsl_permutation != r_glsl_permutations + permutation)
	{
		r_glsl_permutation = r_glsl_permutations + permutation;
		CHECKGLERROR
		qglUseProgramObjectARB(r_glsl_permutation->program);
		CHECKGLERROR
	}
}

void gl_main_start(void)
{
	r_main_texturepool = R_AllocTexturePool();
	r_bloom_texture_screen = NULL;
	r_bloom_texture_bloom = NULL;
	R_BuildBlankTextures();
	R_BuildNoTexture();
	if (gl_texturecubemap)
	{
		R_BuildWhiteCube();
		R_BuildNormalizationCube();
	}
	R_BuildFogTexture();
	memset(r_glsl_permutations, 0, sizeof(r_glsl_permutations));
}

void gl_main_shutdown(void)
{
	R_FreeTexturePool(&r_main_texturepool);
	r_bloom_texture_screen = NULL;
	r_bloom_texture_bloom = NULL;
	r_texture_blanknormalmap = NULL;
	r_texture_white = NULL;
	r_texture_black = NULL;
	r_texture_whitecube = NULL;
	r_texture_normalizationcube = NULL;
	R_GLSL_Restart_f();
}

extern void CL_ParseEntityLump(char *entitystring);
void gl_main_newmap(void)
{
	// FIXME: move this code to client
	int l;
	char *entities, entname[MAX_QPATH];
	r_framecount = 1;
	if (cl.worldmodel)
	{
		strlcpy(entname, cl.worldmodel->name, sizeof(entname));
		l = (int)strlen(entname) - 4;
		if (l >= 0 && !strcmp(entname + l, ".bsp"))
		{
			strcpy(entname + l, ".ent");
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
}

void GL_Main_Init(void)
{
	r_main_mempool = Mem_AllocPool("Renderer", 0, NULL);

	Cmd_AddCommand("r_glsl_restart", R_GLSL_Restart_f, "unloads GLSL shaders, they will then be reloaded as needed\n");
	FOG_registercvars(); // FIXME: move this fog stuff to client?
	Cvar_RegisterVariable(&r_nearclip);
	Cvar_RegisterVariable(&r_showsurfaces);
	Cvar_RegisterVariable(&r_showtris);
	Cvar_RegisterVariable(&r_shownormals);
	Cvar_RegisterVariable(&r_showlighting);
	Cvar_RegisterVariable(&r_showshadowvolumes);
	Cvar_RegisterVariable(&r_showcollisionbrushes);
	Cvar_RegisterVariable(&r_showcollisionbrushes_polygonfactor);
	Cvar_RegisterVariable(&r_showcollisionbrushes_polygonoffset);
	Cvar_RegisterVariable(&r_showdisabledepthtest);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_q1bsp_skymasking);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&r_glsl);
	Cvar_RegisterVariable(&r_glsl_offsetmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_scale);
	Cvar_RegisterVariable(&r_glsl_deluxemapping);
	Cvar_RegisterVariable(&r_lerpsprites);
	Cvar_RegisterVariable(&r_lerpmodels);
	Cvar_RegisterVariable(&r_waterscroll);
	Cvar_RegisterVariable(&r_bloom);
	Cvar_RegisterVariable(&r_bloom_intensity);
	Cvar_RegisterVariable(&r_bloom_blur);
	Cvar_RegisterVariable(&r_bloom_resolution);
	Cvar_RegisterVariable(&r_bloom_power);
	Cvar_RegisterVariable(&r_smoothnormals_areaweighting);
	Cvar_RegisterVariable(&developer_texturelogging);
	Cvar_RegisterVariable(&gl_lightmaps);
	Cvar_RegisterVariable(&r_test);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_TENEBRAE)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);
}

static vec3_t r_farclip_origin;
static vec3_t r_farclip_direction;
static vec_t r_farclip_directiondist;
static vec_t r_farclip_meshfarclip;
static int r_farclip_directionbit0;
static int r_farclip_directionbit1;
static int r_farclip_directionbit2;

// enlarge farclip to accomodate box
static void R_FarClip_Box(vec3_t mins, vec3_t maxs)
{
	float d;
	d = (r_farclip_directionbit0 ? mins[0] : maxs[0]) * r_farclip_direction[0]
	  + (r_farclip_directionbit1 ? mins[1] : maxs[1]) * r_farclip_direction[1]
	  + (r_farclip_directionbit2 ? mins[2] : maxs[2]) * r_farclip_direction[2];
	if (r_farclip_meshfarclip < d)
		r_farclip_meshfarclip = d;
}

// return farclip value
static float R_FarClip(vec3_t origin, vec3_t direction, vec_t startfarclip)
{
	int i;

	VectorCopy(origin, r_farclip_origin);
	VectorCopy(direction, r_farclip_direction);
	r_farclip_directiondist = DotProduct(r_farclip_origin, r_farclip_direction);
	r_farclip_directionbit0 = r_farclip_direction[0] < 0;
	r_farclip_directionbit1 = r_farclip_direction[1] < 0;
	r_farclip_directionbit2 = r_farclip_direction[2] < 0;
	r_farclip_meshfarclip = r_farclip_directiondist + startfarclip;

	if (r_refdef.worldmodel)
		R_FarClip_Box(r_refdef.worldmodel->normalmins, r_refdef.worldmodel->normalmaxs);
	for (i = 0;i < r_refdef.numentities;i++)
		R_FarClip_Box(r_refdef.entities[i]->mins, r_refdef.entities[i]->maxs);

	return r_farclip_meshfarclip - r_farclip_directiondist;
}

extern void R_Textures_Init(void);
extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void R_Shadow_Init(void);
extern void R_Sky_Init(void);
extern void GL_Surf_Init(void);
extern void R_Crosshairs_Init(void);
extern void R_Light_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void gl_backend_init(void);
extern void Sbar_Init(void);
extern void R_LightningBeams_Init(void);
extern void Mod_RenderInit(void);

void Render_Init(void)
{
	gl_backend_init();
	R_Textures_Init();
	R_MeshQueue_Init();
	GL_Main_Init();
	GL_Draw_Init();
	R_Shadow_Init();
	R_Sky_Init();
	GL_Surf_Init();
	R_Crosshairs_Init();
	R_Light_Init();
	R_Particles_Init();
	R_Explosion_Init();
	Sbar_Init();
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
	VID_CheckExtensions();

	// LordHavoc: report supported extensions
	Con_DPrintf("\nengine extensions: %s\n", vm_sv_extensions );

	// clear to black (loading plaque will be seen over this)
	qglClearColor(0,0,0,1);
	qglClear(GL_COLOR_BUFFER_BIT);
}

int R_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int i;
	mplane_t *p;
	for (i = 0;i < 4;i++)
	{
		p = frustum + i;
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

static void R_UpdateEntityLighting(entity_render_t *ent)
{
	vec3_t tempdiffusenormal;
	VectorSet(ent->modellight_ambient, r_ambient.value * (2.0f / 128.0f), r_ambient.value * (2.0f / 128.0f), r_ambient.value * (2.0f / 128.0f));
	VectorClear(ent->modellight_diffuse);
	VectorClear(ent->modellight_lightdir);
	if ((ent->flags & RENDER_LIGHT) && r_refdef.worldmodel && r_refdef.worldmodel->brush.LightPoint)
		r_refdef.worldmodel->brush.LightPoint(r_refdef.worldmodel, ent->origin, ent->modellight_ambient, ent->modellight_diffuse, tempdiffusenormal);
	else // highly rare
		VectorSet(ent->modellight_ambient, 1, 1, 1);
	Matrix4x4_Transform3x3(&ent->inversematrix, tempdiffusenormal, ent->modellight_lightdir);
	VectorNormalize(ent->modellight_lightdir);
	ent->modellight_ambient[0] *= ent->colormod[0] * r_lightmapintensity;
	ent->modellight_ambient[1] *= ent->colormod[1] * r_lightmapintensity;
	ent->modellight_ambient[2] *= ent->colormod[2] * r_lightmapintensity;
	ent->modellight_diffuse[0] *= ent->colormod[0] * r_lightmapintensity;
	ent->modellight_diffuse[1] *= ent->colormod[1] * r_lightmapintensity;
	ent->modellight_diffuse[2] *= ent->colormod[2] * r_lightmapintensity;
}

static void R_MarkEntities (void)
{
	int i, renderimask;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	r_refdef.worldentity->visframe = r_framecount;
	renderimask = envmap ? (RENDER_EXTERIORMODEL | RENDER_VIEWMODEL) : (chase_active.integer ? 0 : RENDER_EXTERIORMODEL);
	if (r_refdef.worldmodel && r_refdef.worldmodel->brush.BoxTouchingVisibleLeafs)
	{
		// worldmodel can check visibility
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			// some of the renderer still relies on origin...
			Matrix4x4_OriginFromMatrix(&ent->matrix, ent->origin);
			// some of the renderer still relies on scale...
			ent->scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
			if (!(ent->flags & renderimask) && !R_CullBox(ent->mins, ent->maxs) && ((ent->effects & EF_NODEPTHTEST) || r_refdef.worldmodel->brush.BoxTouchingVisibleLeafs(r_refdef.worldmodel, r_worldleafvisible, ent->mins, ent->maxs)))
			{
				ent->visframe = r_framecount;
				R_UpdateEntityLighting(ent);
			}
		}
	}
	else
	{
		// no worldmodel or it can't check visibility
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			// some of the renderer still relies on origin...
			Matrix4x4_OriginFromMatrix(&ent->matrix, ent->origin);
			// some of the renderer still relies on scale...
			ent->scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
			if (!(ent->flags & renderimask) && !R_CullBox(ent->mins, ent->maxs) && (ent->effects & EF_NODEPTHTEST))
			{
				ent->visframe = r_framecount;
				R_UpdateEntityLighting(ent);
			}
		}
	}
}

// only used if skyrendermasked, and normally returns false
int R_DrawBrushModelsSky (void)
{
	int i, sky;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return false;

	sky = false;
	for (i = 0;i < r_refdef.numentities;i++)
	{
		ent = r_refdef.entities[i];
		if (ent->visframe == r_framecount && ent->model && ent->model->DrawSky)
		{
			ent->model->DrawSky(ent);
			sky = true;
		}
	}
	return sky;
}

void R_DrawNoModel(entity_render_t *ent);
void R_DrawModels(void)
{
	int i;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		ent = r_refdef.entities[i];
		if (ent->visframe == r_framecount)
		{
			renderstats.entities++;
			if (ent->model && ent->model->Draw != NULL)
				ent->model->Draw(ent);
			else
				R_DrawNoModel(ent);
		}
	}
}

static void R_SetFrustum(void)
{
	// break apart the view matrix into vectors for various purposes
	Matrix4x4_ToVectors(&r_view_matrix, r_viewforward, r_viewleft, r_viewup, r_vieworigin);
	VectorNegate(r_viewleft, r_viewright);

#if 0
	frustum[0].normal[0] = 0 - 1.0 / r_refdef.frustum_x;
	frustum[0].normal[1] = 0 - 0;
	frustum[0].normal[2] = -1 - 0;
	frustum[1].normal[0] = 0 + 1.0 / r_refdef.frustum_x;
	frustum[1].normal[1] = 0 + 0;
	frustum[1].normal[2] = -1 + 0;
	frustum[2].normal[0] = 0 - 0;
	frustum[2].normal[1] = 0 - 1.0 / r_refdef.frustum_y;
	frustum[2].normal[2] = -1 - 0;
	frustum[3].normal[0] = 0 + 0;
	frustum[3].normal[1] = 0 + 1.0 / r_refdef.frustum_y;
	frustum[3].normal[2] = -1 + 0;
#endif

#if 0
	zNear = r_nearclip.value;
	nudge = 1.0 - 1.0 / (1<<23);
	frustum[4].normal[0] = 0 - 0;
	frustum[4].normal[1] = 0 - 0;
	frustum[4].normal[2] = -1 - -nudge;
	frustum[4].dist = 0 - -2 * zNear * nudge;
	frustum[5].normal[0] = 0 + 0;
	frustum[5].normal[1] = 0 + 0;
	frustum[5].normal[2] = -1 + -nudge;
	frustum[5].dist = 0 + -2 * zNear * nudge;
#endif



#if 0
	frustum[0].normal[0] = m[3] - m[0];
	frustum[0].normal[1] = m[7] - m[4];
	frustum[0].normal[2] = m[11] - m[8];
	frustum[0].dist = m[15] - m[12];

	frustum[1].normal[0] = m[3] + m[0];
	frustum[1].normal[1] = m[7] + m[4];
	frustum[1].normal[2] = m[11] + m[8];
	frustum[1].dist = m[15] + m[12];

	frustum[2].normal[0] = m[3] - m[1];
	frustum[2].normal[1] = m[7] - m[5];
	frustum[2].normal[2] = m[11] - m[9];
	frustum[2].dist = m[15] - m[13];

	frustum[3].normal[0] = m[3] + m[1];
	frustum[3].normal[1] = m[7] + m[5];
	frustum[3].normal[2] = m[11] + m[9];
	frustum[3].dist = m[15] + m[13];

	frustum[4].normal[0] = m[3] - m[2];
	frustum[4].normal[1] = m[7] - m[6];
	frustum[4].normal[2] = m[11] - m[10];
	frustum[4].dist = m[15] - m[14];

	frustum[5].normal[0] = m[3] + m[2];
	frustum[5].normal[1] = m[7] + m[6];
	frustum[5].normal[2] = m[11] + m[10];
	frustum[5].dist = m[15] + m[14];
#endif



	VectorMAM(1, r_viewforward, 1.0 / -r_refdef.frustum_x, r_viewleft, frustum[0].normal);
	VectorMAM(1, r_viewforward, 1.0 /  r_refdef.frustum_x, r_viewleft, frustum[1].normal);
	VectorMAM(1, r_viewforward, 1.0 / -r_refdef.frustum_y, r_viewup, frustum[2].normal);
	VectorMAM(1, r_viewforward, 1.0 /  r_refdef.frustum_y, r_viewup, frustum[3].normal);
	VectorCopy(r_viewforward, frustum[4].normal);
	VectorNormalize(frustum[0].normal);
	VectorNormalize(frustum[1].normal);
	VectorNormalize(frustum[2].normal);
	VectorNormalize(frustum[3].normal);
	frustum[0].dist = DotProduct (r_vieworigin, frustum[0].normal);
	frustum[1].dist = DotProduct (r_vieworigin, frustum[1].normal);
	frustum[2].dist = DotProduct (r_vieworigin, frustum[2].normal);
	frustum[3].dist = DotProduct (r_vieworigin, frustum[3].normal);
	frustum[4].dist = DotProduct (r_vieworigin, frustum[4].normal) + r_nearclip.value;
	PlaneClassify(&frustum[0]);
	PlaneClassify(&frustum[1]);
	PlaneClassify(&frustum[2]);
	PlaneClassify(&frustum[3]);
	PlaneClassify(&frustum[4]);

	// LordHavoc: note to all quake engine coders, Quake had a special case
	// for 90 degrees which assumed a square view (wrong), so I removed it,
	// Quake2 has it disabled as well.

	// rotate R_VIEWFORWARD right by FOV_X/2 degrees
	//RotatePointAroundVector( frustum[0].normal, r_viewup, r_viewforward, -(90 - r_refdef.fov_x / 2));
	//frustum[0].dist = DotProduct (r_vieworigin, frustum[0].normal);
	//PlaneClassify(&frustum[0]);

	// rotate R_VIEWFORWARD left by FOV_X/2 degrees
	//RotatePointAroundVector( frustum[1].normal, r_viewup, r_viewforward, (90 - r_refdef.fov_x / 2));
	//frustum[1].dist = DotProduct (r_vieworigin, frustum[1].normal);
	//PlaneClassify(&frustum[1]);

	// rotate R_VIEWFORWARD up by FOV_X/2 degrees
	//RotatePointAroundVector( frustum[2].normal, r_viewleft, r_viewforward, -(90 - r_refdef.fov_y / 2));
	//frustum[2].dist = DotProduct (r_vieworigin, frustum[2].normal);
	//PlaneClassify(&frustum[2]);

	// rotate R_VIEWFORWARD down by FOV_X/2 degrees
	//RotatePointAroundVector( frustum[3].normal, r_viewleft, r_viewforward, (90 - r_refdef.fov_y / 2));
	//frustum[3].dist = DotProduct (r_vieworigin, frustum[3].normal);
	//PlaneClassify(&frustum[3]);

	// nearclip plane
	//VectorCopy(r_viewforward, frustum[4].normal);
	//frustum[4].dist = DotProduct (r_vieworigin, frustum[4].normal) + r_nearclip.value;
	//PlaneClassify(&frustum[4]);
}

static void R_BlendView(void)
{
	int screenwidth, screenheight;
	qboolean dobloom;
	qboolean doblend;
	float vertex3f[12];
	float texcoord2f[3][8];

	// set the (poorly named) screenwidth and screenheight variables to
	// a power of 2 at least as large as the screen, these will define the
	// size of the texture to allocate
	for (screenwidth = 1;screenwidth < vid.width;screenwidth *= 2);
	for (screenheight = 1;screenheight < vid.height;screenheight *= 2);

	doblend = r_refdef.viewblend[3] >= 0.01f;
	dobloom = r_bloom.integer && screenwidth <= gl_max_texture_size && screenheight <= gl_max_texture_size && r_bloom_resolution.value >= 32 && r_bloom_power.integer >= 1 && r_bloom_power.integer < 100 && r_bloom_blur.value >= 0 && r_bloom_blur.value < 512;

	if (!dobloom && !doblend)
		return;

	GL_SetupView_Mode_Ortho(0, 0, 1, 1, -10, 100);
	GL_DepthMask(true);
	GL_DepthTest(false);
	R_Mesh_Matrix(&identitymatrix);
	// vertex coordinates for a quad that covers the screen exactly
	vertex3f[0] = 0;vertex3f[1] = 0;vertex3f[2] = 0;
	vertex3f[3] = 1;vertex3f[4] = 0;vertex3f[5] = 0;
	vertex3f[6] = 1;vertex3f[7] = 1;vertex3f[8] = 0;
	vertex3f[9] = 0;vertex3f[10] = 1;vertex3f[11] = 0;
	R_Mesh_VertexPointer(vertex3f);
	R_Mesh_ColorPointer(NULL);
	R_Mesh_ResetTextureState();
	if (dobloom)
	{
		int bloomwidth, bloomheight, x, range;
		float xoffset, yoffset, r;
		renderstats.bloom++;
		// allocate textures as needed
		if (!r_bloom_texture_screen)
			r_bloom_texture_screen = R_LoadTexture2D(r_main_texturepool, "screen", screenwidth, screenheight, NULL, TEXTYPE_RGBA, TEXF_FORCENEAREST | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
		if (!r_bloom_texture_bloom)
			r_bloom_texture_bloom = R_LoadTexture2D(r_main_texturepool, "bloom", screenwidth, screenheight, NULL, TEXTYPE_RGBA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
		// set bloomwidth and bloomheight to the bloom resolution that will be
		// used (often less than the screen resolution for faster rendering)
		bloomwidth = min(r_view_width, r_bloom_resolution.integer);
		bloomheight = min(r_view_height, bloomwidth * r_view_height / r_view_width);
		// set up a texcoord array for the full resolution screen image
		// (we have to keep this around to copy back during final render)
		texcoord2f[0][0] = 0;
		texcoord2f[0][1] = (float)r_view_height / (float)screenheight;
		texcoord2f[0][2] = (float)r_view_width / (float)screenwidth;
		texcoord2f[0][3] = (float)r_view_height / (float)screenheight;
		texcoord2f[0][4] = (float)r_view_width / (float)screenwidth;
		texcoord2f[0][5] = 0;
		texcoord2f[0][6] = 0;
		texcoord2f[0][7] = 0;
		// set up a texcoord array for the reduced resolution bloom image
		// (which will be additive blended over the screen image)
		texcoord2f[1][0] = 0;
		texcoord2f[1][1] = (float)bloomheight / (float)screenheight;
		texcoord2f[1][2] = (float)bloomwidth / (float)screenwidth;
		texcoord2f[1][3] = (float)bloomheight / (float)screenheight;
		texcoord2f[1][4] = (float)bloomwidth / (float)screenwidth;
		texcoord2f[1][5] = 0;
		texcoord2f[1][6] = 0;
		texcoord2f[1][7] = 0;
		R_Mesh_TexCoordPointer(0, 2, texcoord2f[0]);
		R_Mesh_TexBind(0, R_GetTexture(r_bloom_texture_screen));
		// copy view into the full resolution screen image texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
		renderstats.bloom_copypixels += r_view_width * r_view_height;
		// now scale it down to the bloom size and raise to a power of itself
		// to darken it (this leaves the really bright stuff bright, and
		// everything else becomes very dark)
		// TODO: optimize with multitexture or GLSL
		qglViewport(r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(1, 1, 1, 1);
		R_Mesh_Draw(0, 4, 2, polygonelements);
		renderstats.bloom_drawpixels += bloomwidth * bloomheight;
		// render multiple times with a multiply blendfunc to raise to a power
		GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		for (x = 1;x < r_bloom_power.integer;x++)
		{
			R_Mesh_Draw(0, 4, 2, polygonelements);
			renderstats.bloom_drawpixels += bloomwidth * bloomheight;
		}
		// we now have a darkened bloom image in the framebuffer, copy it into
		// the bloom image texture for more processing
		R_Mesh_TexBind(0, R_GetTexture(r_bloom_texture_bloom));
		R_Mesh_TexCoordPointer(0, 2, texcoord2f[2]);
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		renderstats.bloom_copypixels += bloomwidth * bloomheight;
		// blend on at multiple vertical offsets to achieve a vertical blur
		// TODO: do offset blends using GLSL
		range = r_bloom_blur.integer * bloomwidth / 320;
		GL_BlendFunc(GL_ONE, GL_ZERO);
		for (x = -range;x <= range;x++)
		{
			xoffset = 0 / (float)bloomwidth * (float)bloomwidth / (float)screenwidth;
			yoffset = x / (float)bloomheight * (float)bloomheight / (float)screenheight;
			// compute a texcoord array with the specified x and y offset
			texcoord2f[2][0] = xoffset+0;
			texcoord2f[2][1] = yoffset+(float)bloomheight / (float)screenheight;
			texcoord2f[2][2] = xoffset+(float)bloomwidth / (float)screenwidth;
			texcoord2f[2][3] = yoffset+(float)bloomheight / (float)screenheight;
			texcoord2f[2][4] = xoffset+(float)bloomwidth / (float)screenwidth;
			texcoord2f[2][5] = yoffset+0;
			texcoord2f[2][6] = xoffset+0;
			texcoord2f[2][7] = yoffset+0;
			// this r value looks like a 'dot' particle, fading sharply to
			// black at the edges
			// (probably not realistic but looks good enough)
			r = r_bloom_intensity.value/(range*2+1)*(1 - x*x/(float)(range*range));
			if (r < 0.01f)
				continue;
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 2, polygonelements);
			renderstats.bloom_drawpixels += bloomwidth * bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
		// copy the vertically blurred bloom view to a texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		renderstats.bloom_copypixels += bloomwidth * bloomheight;
		// blend the vertically blurred image at multiple offsets horizontally
		// to finish the blur effect
		// TODO: do offset blends using GLSL
		range = r_bloom_blur.integer * bloomwidth / 320;
		GL_BlendFunc(GL_ONE, GL_ZERO);
		for (x = -range;x <= range;x++)
		{
			xoffset = x / (float)bloomwidth * (float)bloomwidth / (float)screenwidth;
			yoffset = 0 / (float)bloomheight * (float)bloomheight / (float)screenheight;
			// compute a texcoord array with the specified x and y offset
			texcoord2f[2][0] = xoffset+0;
			texcoord2f[2][1] = yoffset+(float)bloomheight / (float)screenheight;
			texcoord2f[2][2] = xoffset+(float)bloomwidth / (float)screenwidth;
			texcoord2f[2][3] = yoffset+(float)bloomheight / (float)screenheight;
			texcoord2f[2][4] = xoffset+(float)bloomwidth / (float)screenwidth;
			texcoord2f[2][5] = yoffset+0;
			texcoord2f[2][6] = xoffset+0;
			texcoord2f[2][7] = yoffset+0;
			// this r value looks like a 'dot' particle, fading sharply to
			// black at the edges
			// (probably not realistic but looks good enough)
			r = r_bloom_intensity.value/(range*2+1)*(1 - x*x/(float)(range*range));
			if (r < 0.01f)
				continue;
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 2, polygonelements);
			renderstats.bloom_drawpixels += bloomwidth * bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
		// copy the blurred bloom view to a texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		renderstats.bloom_copypixels += bloomwidth * bloomheight;
		// go back to full view area
		qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
		// put the original screen image back in place and blend the bloom
		// texture on it
		GL_Color(1,1,1,1);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		// do both in one pass if possible
		R_Mesh_TexBind(0, R_GetTexture(r_bloom_texture_screen));
		R_Mesh_TexCoordPointer(0, 2, texcoord2f[0]);
		if (r_textureunits.integer >= 2 && gl_combine.integer)
		{
			R_Mesh_TexCombine(1, GL_ADD, GL_ADD, 1, 1);
			R_Mesh_TexBind(1, R_GetTexture(r_bloom_texture_bloom));
			R_Mesh_TexCoordPointer(1, 2, texcoord2f[1]);
		}
		else
		{
			R_Mesh_Draw(0, 4, 2, polygonelements);
			renderstats.bloom_drawpixels += r_view_width * r_view_height;
			// now blend on the bloom texture
			GL_BlendFunc(GL_ONE, GL_ONE);
			R_Mesh_TexBind(0, R_GetTexture(r_bloom_texture_bloom));
			R_Mesh_TexCoordPointer(0, 2, texcoord2f[1]);
		}
		R_Mesh_Draw(0, 4, 2, polygonelements);
		renderstats.bloom_drawpixels += r_view_width * r_view_height;
	}
	if (doblend)
	{
		// apply a color tint to the whole view
		R_Mesh_ResetTextureState();
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
		R_Mesh_Draw(0, 4, 2, polygonelements);
	}
}

void R_RenderScene(void);

matrix4x4_t r_waterscrollmatrix;

/*
================
R_RenderView
================
*/
void R_RenderView(void)
{
	if (!r_refdef.entities/* || !r_refdef.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	r_view_width = bound(0, r_refdef.width, vid.width);
	r_view_height = bound(0, r_refdef.height, vid.height);
	r_view_depth = 1;
	r_view_x = bound(0, r_refdef.x, vid.width - r_refdef.width);
	r_view_y = bound(0, r_refdef.y, vid.height - r_refdef.height);
	r_view_z = 0;
	r_view_matrix = r_refdef.viewentitymatrix;
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	r_rtworld = r_shadow_realtime_world.integer;
	r_rtworldshadows = r_shadow_realtime_world_shadows.integer && gl_stencil;
	r_rtdlight = (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer) && !gl_flashblend.integer;
	r_rtdlightshadows = r_rtdlight && (r_rtworld ? r_shadow_realtime_world_dlightshadows.integer : r_shadow_realtime_dlight_shadows.integer) && gl_stencil;
	r_lightmapintensity = r_rtworld ? r_shadow_realtime_world_lightmaps.value : 1;
	r_polygonfactor = 0;
	r_polygonoffset = 0;
	r_shadowpolygonfactor = r_polygonfactor + r_shadow_shadow_polygonfactor.value;
	r_shadowpolygonoffset = r_polygonoffset + r_shadow_shadow_polygonoffset.value;
	if (r_showsurfaces.integer)
	{
		r_rtworld = false;
		r_rtworldshadows = false;
		r_rtdlight = false;
		r_rtdlightshadows = false;
		r_lightmapintensity = 0;
	}

	// GL is weird because it's bottom to top, r_view_y is top to bottom
	qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
	GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	R_ClearScreen();
	R_Textures_Frame();
	R_UpdateFog();
	if (r_timereport_active)
		R_TimeReport("setup");

	qglDepthFunc(GL_LEQUAL);
	qglPolygonOffset(r_polygonfactor, r_polygonoffset);
	qglEnable(GL_POLYGON_OFFSET_FILL);

	R_RenderScene();

	qglPolygonOffset(r_polygonfactor, r_polygonoffset);
	qglDisable(GL_POLYGON_OFFSET_FILL);

	R_BlendView();
	if (r_timereport_active)
		R_TimeReport("blendview");

	GL_Scissor(0, 0, vid.width, vid.height);
	GL_ScissorTest(false);
}

//[515]: csqc
void CSQC_R_ClearScreen (void)
{
	if (!r_refdef.entities/* || !r_refdef.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	r_view_width = bound(0, r_refdef.width, vid.width);
	r_view_height = bound(0, r_refdef.height, vid.height);
	r_view_depth = 1;
	r_view_x = bound(0, r_refdef.x, vid.width - r_refdef.width);
	r_view_y = bound(0, r_refdef.y, vid.height - r_refdef.height);
	r_view_z = 0;
	r_view_matrix = r_refdef.viewentitymatrix;
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	r_rtworld = r_shadow_realtime_world.integer;
	r_rtworldshadows = r_shadow_realtime_world_shadows.integer && gl_stencil;
	r_rtdlight = (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer) && !gl_flashblend.integer;
	r_rtdlightshadows = r_rtdlight && (r_rtworld ? r_shadow_realtime_world_dlightshadows.integer : r_shadow_realtime_dlight_shadows.integer) && gl_stencil;
	r_lightmapintensity = r_rtworld ? r_shadow_realtime_world_lightmaps.value : 1;
	r_polygonfactor = 0;
	r_polygonoffset = 0;
	r_shadowpolygonfactor = r_polygonfactor + r_shadow_shadow_polygonfactor.value;
	r_shadowpolygonoffset = r_polygonoffset + r_shadow_shadow_polygonoffset.value;
	if (r_showsurfaces.integer)
	{
		r_rtworld = false;
		r_rtworldshadows = false;
		r_rtdlight = false;
		r_rtdlightshadows = false;
		r_lightmapintensity = 0;
	}

	// GL is weird because it's bottom to top, r_view_y is top to bottom
	qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
	GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	R_ClearScreen();
	R_Textures_Frame();
	R_UpdateFog();
	if (r_timereport_active)
		R_TimeReport("setup");
}

//[515]: csqc
void CSQC_R_RenderScene (void)
{
	qglDepthFunc(GL_LEQUAL);
	qglPolygonOffset(r_polygonfactor, r_polygonoffset);
	qglEnable(GL_POLYGON_OFFSET_FILL);

	R_RenderScene();

	qglPolygonOffset(r_polygonfactor, r_polygonoffset);
	qglDisable(GL_POLYGON_OFFSET_FILL);

	R_BlendView();
	if (r_timereport_active)
		R_TimeReport("blendview");

	GL_Scissor(0, 0, vid.width, vid.height);
	GL_ScissorTest(false);
}

extern void R_DrawLightningBeams (void);
extern void VM_AddPolygonsToMeshQueue (void);
void R_RenderScene(void)
{
	float nearclip;

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	r_framecount++;

	if (gl_support_fragment_shader)
		qglUseProgramObjectARB(0);

	R_MeshQueue_BeginScene();

	R_SetFrustum();

	r_farclip = R_FarClip(r_vieworigin, r_viewforward, 768.0f) + 256.0f;
	nearclip = bound (0.001f, r_nearclip.value, r_farclip - 1.0f);

	if (r_rtworldshadows || r_rtdlightshadows)
		GL_SetupView_Mode_PerspectiveInfiniteFarClip(r_refdef.frustum_x, r_refdef.frustum_y, nearclip);
	else
		GL_SetupView_Mode_Perspective(r_refdef.frustum_x, r_refdef.frustum_y, nearclip, r_farclip);

	GL_SetupView_Orientation_FromEntity(&r_view_matrix);

	Matrix4x4_CreateTranslate(&r_waterscrollmatrix, sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);

	R_SkyStartFrame();

	R_WorldVisibility();
	if (r_timereport_active)
		R_TimeReport("worldvis");

	R_MarkEntities();
	if (r_timereport_active)
		R_TimeReport("markentity");

	R_Shadow_UpdateWorldLightSelection();

	if (cl.csqc_vidvars.drawworld)
	{
		// don't let sound skip if going slow
		if (r_refdef.extraupdate)
			S_ExtraUpdate ();

		if (r_refdef.worldmodel && r_refdef.worldmodel->DrawSky)
		{
			r_refdef.worldmodel->DrawSky(r_refdef.worldentity);
			if (r_timereport_active)
				R_TimeReport("worldsky");
		}

		if (R_DrawBrushModelsSky() && r_timereport_active)
			R_TimeReport("bmodelsky");

		if (r_refdef.worldmodel && r_refdef.worldmodel->Draw)
		{
			r_refdef.worldmodel->Draw(r_refdef.worldentity);
			if (r_timereport_active)
				R_TimeReport("world");
		}
	}

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	R_DrawModels();
	if (r_timereport_active)
		R_TimeReport("models");

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	R_ShadowVolumeLighting(false);
	if (r_timereport_active)
		R_TimeReport("rtlights");

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	if (cl.csqc_vidvars.drawworld)
	{
		R_DrawLightningBeams();
		if (r_timereport_active)
			R_TimeReport("lightning");

		R_DrawParticles();
		if (r_timereport_active)
			R_TimeReport("particles");

		R_DrawExplosions();
		if (r_timereport_active)
			R_TimeReport("explosions");
	}

	R_MeshQueue_RenderTransparent();
	if (r_timereport_active)
		R_TimeReport("drawtrans");

	if (cl.csqc_vidvars.drawworld)
	{
		R_DrawCoronas();
		if (r_timereport_active)
			R_TimeReport("coronas");
	}
	if(cl.csqc_vidvars.drawcrosshair)
	{
		R_DrawWorldCrosshair();
		if (r_timereport_active)
			R_TimeReport("crosshair");
	}

	VM_AddPolygonsToMeshQueue();

	R_MeshQueue_Render();

	R_MeshQueue_EndScene();

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	if (gl_support_fragment_shader)
		qglUseProgramObjectARB(0);
}

/*
void R_DrawBBoxMesh(vec3_t mins, vec3_t maxs, float cr, float cg, float cb, float ca)
{
	int i;
	float *v, *c, f1, f2, diff[3], vertex3f[8*3], color4f[8*4];
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&identitymatrix);

	vertex3f[ 0] = mins[0];vertex3f[ 1] = mins[1];vertex3f[ 2] = mins[2];
	vertex3f[ 3] = maxs[0];vertex3f[ 4] = mins[1];vertex3f[ 5] = mins[2];
	vertex3f[ 6] = mins[0];vertex3f[ 7] = maxs[1];vertex3f[ 8] = mins[2];
	vertex3f[ 9] = maxs[0];vertex3f[10] = maxs[1];vertex3f[11] = mins[2];
	vertex3f[12] = mins[0];vertex3f[13] = mins[1];vertex3f[14] = maxs[2];
	vertex3f[15] = maxs[0];vertex3f[16] = mins[1];vertex3f[17] = maxs[2];
	vertex3f[18] = mins[0];vertex3f[19] = maxs[1];vertex3f[20] = maxs[2];
	vertex3f[21] = maxs[0];vertex3f[22] = maxs[1];vertex3f[23] = maxs[2];
	R_FillColors(color, 8, cr, cg, cb, ca);
	if (fogenabled)
	{
		for (i = 0, v = vertex, c = color;i < 8;i++, v += 4, c += 4)
		{
			f2 = VERTEXFOGTABLE(VectorDistance(v, r_vieworigin));
			f1 = 1 - f2;
			c[0] = c[0] * f1 + fogcolor[0] * f2;
			c[1] = c[1] * f1 + fogcolor[1] * f2;
			c[2] = c[2] * f1 + fogcolor[2] * f2;
		}
	}
	R_Mesh_VertexPointer(vertex3f);
	R_Mesh_ColorPointer(color);
	R_Mesh_ResetTextureState();
	R_Mesh_Draw(8, 12);
}
*/

int nomodelelements[24] =
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

float nomodelvertex3f[6*3] =
{
	-16,   0,   0,
	 16,   0,   0,
	  0, -16,   0,
	  0,  16,   0,
	  0,   0, -16,
	  0,   0,  16
};

float nomodelcolor4f[6*4] =
{
	0.0f, 0.0f, 0.5f, 1.0f,
	0.0f, 0.0f, 0.5f, 1.0f,
	0.0f, 0.5f, 0.0f, 1.0f,
	0.0f, 0.5f, 0.0f, 1.0f,
	0.5f, 0.0f, 0.0f, 1.0f,
	0.5f, 0.0f, 0.0f, 1.0f
};

void R_DrawNoModel_TransparentCallback(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	int i;
	float f1, f2, *c;
	float color4f[6*4];
	R_Mesh_Matrix(&ent->matrix);

	if (ent->flags & EF_ADDITIVE)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (ent->alpha < 1)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
	}
	GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
	R_Mesh_VertexPointer(nomodelvertex3f);
	if (fogenabled)
	{
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		R_Mesh_ColorPointer(color4f);
		f2 = VERTEXFOGTABLE(VectorDistance(ent->origin, r_vieworigin));
		f1 = 1 - f2;
		for (i = 0, c = color4f;i < 6;i++, c += 4)
		{
			c[0] = (c[0] * f1 + fogcolor[0] * f2);
			c[1] = (c[1] * f1 + fogcolor[1] * f2);
			c[2] = (c[2] * f1 + fogcolor[2] * f2);
			c[3] *= ent->alpha;
		}
	}
	else if (ent->alpha != 1)
	{
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		R_Mesh_ColorPointer(color4f);
		for (i = 0, c = color4f;i < 6;i++, c += 4)
			c[3] *= ent->alpha;
	}
	else
		R_Mesh_ColorPointer(nomodelcolor4f);
	R_Mesh_ResetTextureState();
	R_Mesh_Draw(0, 6, 8, nomodelelements);
}

void R_DrawNoModel(entity_render_t *ent)
{
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : ent->origin, R_DrawNoModel_TransparentCallback, ent, 0, r_shadow_rtlight);
	//else
	//	R_DrawNoModelCallback(ent, 0);
}

void R_CalcBeam_Vertex3f (float *vert, const vec3_t org1, const vec3_t org2, float width)
{
	vec3_t right1, right2, diff, normal;

	VectorSubtract (org2, org1, normal);

	// calculate 'right' vector for start
	VectorSubtract (r_vieworigin, org1, diff);
	CrossProduct (normal, diff, right1);
	VectorNormalize (right1);

	// calculate 'right' vector for end
	VectorSubtract (r_vieworigin, org2, diff);
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

float spritetexcoord2f[4*2] = {0, 1, 0, 0, 1, 0, 1, 1};

void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, rtexture_t *fogtexture, int depthdisable, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca)
{
	float fog = 0.0f, ifog;
	float vertex3f[12];

	if (fogenabled)
		fog = VERTEXFOGTABLE(VectorDistance(origin, r_vieworigin));
	ifog = 1 - fog;

	R_Mesh_Matrix(&identitymatrix);
	GL_BlendFunc(blendfunc1, blendfunc2);
	GL_DepthMask(false);
	GL_DepthTest(!depthdisable);

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

	R_Mesh_VertexPointer(vertex3f);
	R_Mesh_ColorPointer(NULL);
	R_Mesh_ResetTextureState();
	R_Mesh_TexBind(0, R_GetTexture(texture));
	R_Mesh_TexCoordPointer(0, 2, spritetexcoord2f);
	GL_Color(cr * ifog, cg * ifog, cb * ifog, ca);
	R_Mesh_Draw(0, 4, 2, polygonelements);

	if (blendfunc2 == GL_ONE_MINUS_SRC_ALPHA)
	{
		R_Mesh_TexBind(0, R_GetTexture(fogtexture));
		GL_BlendFunc(blendfunc1, GL_ONE);
		GL_Color(fogcolor[0] * fog, fogcolor[1] * fog, fogcolor[2] * fog, ca);
		R_Mesh_Draw(0, 4, 2, polygonelements);
	}
}

int R_Mesh_AddVertex3f(rmesh_t *mesh, const float *v)
{
	int i;
	float *vertex3f;
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
	element[0] = R_Mesh_AddVertex3f(mesh, vertex3f);vertex3f += 3;
	element[1] = R_Mesh_AddVertex3f(mesh, vertex3f);vertex3f += 3;
	e = mesh->element3i + mesh->numtriangles * 3;
	for (i = 0;i < numvertices - 2;i++, vertex3f += 3)
	{
		element[2] = R_Mesh_AddVertex3f(mesh, vertex3f);
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

void R_Mesh_AddBrushMeshFromPlanes(rmesh_t *mesh, int numplanes, mplane_t *planes)
{
	int planenum, planenum2;
	int w;
	int tempnumpoints;
	mplane_t *plane, *plane2;
	float temppoints[2][256*3];
	for (planenum = 0, plane = planes;planenum < numplanes;planenum++, plane++)
	{
		w = 0;
		tempnumpoints = 4;
		PolygonF_QuadForPlane(temppoints[w], plane->normal[0], plane->normal[1], plane->normal[2], plane->normal[3], 1024.0*1024.0*1024.0);
		for (planenum2 = 0, plane2 = planes;planenum2 < numplanes && tempnumpoints >= 3;planenum2++, plane2++)
		{
			if (planenum2 == planenum)
				continue;
			PolygonF_Divide(tempnumpoints, temppoints[w], plane2->normal[0], plane2->normal[1], plane2->normal[2], plane2->dist, 1.0/32.0, 0, NULL, NULL, 256, temppoints[!w], &tempnumpoints, NULL);
			w = !w;
		}
		if (tempnumpoints < 3)
			continue;
		// generate elements forming a triangle fan for this polygon
		R_Mesh_AddPolygon3f(mesh, tempnumpoints, temppoints[w]);
	}
}

static void R_DrawCollisionBrush(const colbrushf_t *brush)
{
	int i;
	R_Mesh_VertexPointer(brush->points->v);
	i = (int)(((size_t)brush) / sizeof(colbrushf_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, brush->numpoints);
	R_Mesh_Draw(0, brush->numpoints, brush->numtriangles, brush->elements);
	GL_LockArrays(0, 0);
}

static void R_DrawCollisionSurface(const entity_render_t *ent, const msurface_t *surface)
{
	int i;
	if (!surface->num_collisiontriangles)
		return;
	R_Mesh_VertexPointer(surface->data_collisionvertex3f);
	i = (int)(((size_t)surface) / sizeof(msurface_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, surface->num_collisionvertices);
	R_Mesh_Draw(0, surface->num_collisionvertices, surface->num_collisiontriangles, surface->data_collisionelement3i);
	GL_LockArrays(0, 0);
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

void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t)
{
	// FIXME: identify models using a better check than ent->model->brush.shadowmesh
	//int lightmode = ((ent->effects & EF_FULLBRIGHT) || ent->model->brush.shadowmesh) ? 0 : 2;

	{
		texture_t *texture = t;
		model_t *model = ent->model;
		int s = ent->skinnum;
		if ((unsigned int)s >= (unsigned int)model->numskins)
			s = 0;
		if (model->skinscenes)
		{
			if (model->skinscenes[s].framecount > 1)
				s = model->skinscenes[s].firstframe + (unsigned int) (r_refdef.time * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
			else
				s = model->skinscenes[s].firstframe;
		}
		if (s > 0)
			t = t + s * model->num_surfaces;
		if (t->animated)
			t = t->anim_frames[ent->frame != 0][(t->anim_total[ent->frame != 0] >= 2) ? ((int)(r_refdef.time * 5.0f) % t->anim_total[ent->frame != 0]) : 0];
		texture->currentframe = t;
	}

	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = ent->alpha;
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA)
		t->currentalpha *= r_wateralpha.value;
	if (!(ent->flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	if (ent->effects & EF_ADDITIVE)
		t->currentmaterialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT;
	else if (t->currentalpha < 1)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_TRANSPARENT;
	if (ent->effects & EF_NODEPTHTEST)
		t->currentmaterialflags |= MATERIALFLAG_NODEPTHTEST;
	if (t->currentmaterialflags & MATERIALFLAG_WATER && r_waterscroll.value != 0)
		t->currenttexmatrix = r_waterscrollmatrix;
	else
		t->currenttexmatrix = identitymatrix;

	t->colormapping = VectorLength2(ent->colormap_pantscolor) + VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f);
	t->basetexture = (!t->colormapping && t->skin.merged) ? t->skin.merged : t->skin.base;
	t->glosstexture = r_texture_white;
	t->specularpower = 8;
	t->specularscale = 0;
	if (r_shadow_gloss.integer > 0)
	{
		if (t->skin.gloss)
		{
			if (r_shadow_glossintensity.value > 0)
			{
				t->glosstexture = t->skin.gloss;
				t->specularscale = r_shadow_glossintensity.value;
			}
		}
		else if (r_shadow_gloss.integer >= 2 && r_shadow_gloss2intensity.value > 0)
			t->specularscale = r_shadow_gloss2intensity.value;
	}

	t->currentnumlayers = 0;
	if (!(t->currentmaterialflags & MATERIALFLAG_NODRAW))
	{
		if (gl_lightmaps.integer)
			R_Texture_AddLayer(t, true, GL_ONE, GL_ZERO, TEXTURELAYERTYPE_LITTEXTURE_MULTIPASS, r_texture_white, &identitymatrix, 1, 1, 1, 1);
		else if (!(t->currentmaterialflags & MATERIALFLAG_SKY))
		{
			int blendfunc1, blendfunc2, depthmask;
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
			if (t->currentmaterialflags & (MATERIALFLAG_WATER | MATERIALFLAG_WALL))
			{
				rtexture_t *currentbasetexture;
				int layerflags = 0;
				if (fogenabled && (t->currentmaterialflags & MATERIALFLAG_BLENDED))
					layerflags |= TEXTURELAYERFLAG_FOGDARKEN;
				currentbasetexture = (VectorLength2(ent->colormap_pantscolor) + VectorLength2(ent->colormap_shirtcolor) < (1.0f / 1048576.0f) && t->skin.merged) ? t->skin.merged : t->skin.base;
				if (t->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
				{
					// fullbright is not affected by r_lightmapintensity
					R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_TEXTURE, currentbasetexture, &t->currenttexmatrix, ent->colormod[0], ent->colormod[1], ent->colormod[2], t->currentalpha);
					if (VectorLength2(ent->colormap_pantscolor) >= (1.0f / 1048576.0f) && t->skin.pants)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->skin.pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * ent->colormod[0], ent->colormap_pantscolor[1] * ent->colormod[1], ent->colormap_pantscolor[2] * ent->colormod[2], t->currentalpha);
					if (VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->skin.shirt)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->skin.shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * ent->colormod[0], ent->colormap_shirtcolor[1] * ent->colormod[1], ent->colormap_shirtcolor[2] * ent->colormod[2], t->currentalpha);
				}
				else
				{
					float colorscale;
					colorscale = 2;
					// q3bsp has no lightmap updates, so the lightstylevalue that
					// would normally be baked into the lightmap must be
					// applied to the color
					if (ent->model->type == mod_brushq3)
						colorscale *= r_refdef.lightstylevalue[0] * (1.0f / 256.0f);
					colorscale *= r_lightmapintensity;
					if (r_textureunits.integer >= 2 && gl_combine.integer)
						R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_LITTEXTURE_COMBINE, currentbasetexture, &t->currenttexmatrix, ent->colormod[0] * colorscale, ent->colormod[1] * colorscale, ent->colormod[2] * colorscale, t->currentalpha);
					else if ((t->currentmaterialflags & MATERIALFLAG_BLENDED) == 0)
						R_Texture_AddLayer(t, true, GL_ONE, GL_ZERO, TEXTURELAYERTYPE_LITTEXTURE_MULTIPASS, currentbasetexture, &t->currenttexmatrix, ent->colormod[0] * colorscale * 0.5f, ent->colormod[1] * colorscale * 0.5f, ent->colormod[2] * colorscale * 0.5f, t->currentalpha);
					else
						R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_LITTEXTURE_VERTEX, currentbasetexture, &t->currenttexmatrix, ent->colormod[0] * colorscale, ent->colormod[1] * colorscale, ent->colormod[2] * colorscale, t->currentalpha);
					if (r_ambient.value >= (1.0f/64.0f))
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, currentbasetexture, &t->currenttexmatrix, ent->colormod[0] * r_ambient.value * (1.0f / 64.0f), ent->colormod[1] * r_ambient.value * (1.0f / 64.0f), ent->colormod[2] * r_ambient.value * (1.0f / 64.0f), t->currentalpha);
					if (VectorLength2(ent->colormap_pantscolor) >= (1.0f / 1048576.0f) && t->skin.pants)
					{
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE_VERTEX, t->skin.pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * ent->colormod[0] * colorscale, ent->colormap_pantscolor[1] * ent->colormod[1] * colorscale, ent->colormap_pantscolor[2]  * ent->colormod[2] * colorscale, t->currentalpha);
						if (r_ambient.value >= (1.0f/64.0f))
							R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->skin.pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * ent->colormod[0] * r_ambient.value * (1.0f / 64.0f), ent->colormap_pantscolor[1] * ent->colormod[1] * r_ambient.value * (1.0f / 64.0f), ent->colormap_pantscolor[2] * ent->colormod[2] * r_ambient.value * (1.0f / 64.0f), t->currentalpha);
					}
					if (VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->skin.shirt)
					{
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE_VERTEX, t->skin.shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * ent->colormod[0] * colorscale, ent->colormap_shirtcolor[1] * ent->colormod[1] * colorscale, ent->colormap_shirtcolor[2] * ent->colormod[2] * colorscale, t->currentalpha);
						if (r_ambient.value >= (1.0f/64.0f))
							R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->skin.shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * ent->colormod[0] * r_ambient.value * (1.0f / 64.0f), ent->colormap_shirtcolor[1] * ent->colormod[1] * r_ambient.value * (1.0f / 64.0f), ent->colormap_shirtcolor[2] * ent->colormod[2] * r_ambient.value * (1.0f / 64.0f), t->currentalpha);
					}
				}
				if (t->skin.glow != NULL)
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->skin.glow, &t->currenttexmatrix, 1, 1, 1, t->currentalpha);
				if (fogenabled && !(t->currentmaterialflags & MATERIALFLAG_ADD))
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
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, (t->currentmaterialflags & MATERIALFLAG_BLENDED) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, TEXTURELAYERTYPE_FOG, t->skin.fog, &identitymatrix, fogcolor[0], fogcolor[1], fogcolor[2], t->currentalpha);
				}
			}
		}
	}
}

void R_UpdateAllTextureInfo(entity_render_t *ent)
{
	int i;
	if (ent->model)
		for (i = 0;i < ent->model->num_textures;i++)
			R_UpdateTextureInfo(ent, ent->model->data_textures + i);
}

int rsurface_array_size = 0;
float *rsurface_array_modelvertex3f = NULL;
float *rsurface_array_modelsvector3f = NULL;
float *rsurface_array_modeltvector3f = NULL;
float *rsurface_array_modelnormal3f = NULL;
float *rsurface_array_deformedvertex3f = NULL;
float *rsurface_array_deformedsvector3f = NULL;
float *rsurface_array_deformedtvector3f = NULL;
float *rsurface_array_deformednormal3f = NULL;
float *rsurface_array_color4f = NULL;
float *rsurface_array_texcoord3f = NULL;

void R_Mesh_ResizeArrays(int newvertices)
{
	float *base;
	if (rsurface_array_size >= newvertices)
		return;
	if (rsurface_array_modelvertex3f)
		Mem_Free(rsurface_array_modelvertex3f);
	rsurface_array_size = (newvertices + 1023) & ~1023;
	base = (float *)Mem_Alloc(r_main_mempool, rsurface_array_size * sizeof(float[31]));
	rsurface_array_modelvertex3f     = base + rsurface_array_size * 0;
	rsurface_array_modelsvector3f    = base + rsurface_array_size * 3;
	rsurface_array_modeltvector3f    = base + rsurface_array_size * 6;
	rsurface_array_modelnormal3f     = base + rsurface_array_size * 9;
	rsurface_array_deformedvertex3f  = base + rsurface_array_size * 12;
	rsurface_array_deformedsvector3f = base + rsurface_array_size * 15;
	rsurface_array_deformedtvector3f = base + rsurface_array_size * 18;
	rsurface_array_deformednormal3f  = base + rsurface_array_size * 21;
	rsurface_array_texcoord3f        = base + rsurface_array_size * 24;
	rsurface_array_color4f           = base + rsurface_array_size * 27;
}

float *rsurface_modelvertex3f;
float *rsurface_modelsvector3f;
float *rsurface_modeltvector3f;
float *rsurface_modelnormal3f;
float *rsurface_vertex3f;
float *rsurface_svector3f;
float *rsurface_tvector3f;
float *rsurface_normal3f;
float *rsurface_lightmapcolor4f;
vec3_t rsurface_modelorg;
qboolean rsurface_generatedvertex;
const entity_render_t *rsurface_entity;
const model_t *rsurface_model;
texture_t *rsurface_texture;
rtexture_t *rsurface_lightmaptexture;
rsurfmode_t rsurface_mode;
texture_t *rsurface_glsl_texture;

void RSurf_ActiveEntity(const entity_render_t *ent)
{
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, rsurface_modelorg);
	rsurface_entity = ent;
	rsurface_model = ent->model;
	if (rsurface_array_size < rsurface_model->surfmesh.num_vertices)
		R_Mesh_ResizeArrays(rsurface_model->surfmesh.num_vertices);
	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, rsurface_modelorg);
	if ((rsurface_entity->frameblend[0].lerp != 1 || rsurface_entity->frameblend[0].frame != 0) && (rsurface_model->surfmesh.data_morphvertex3f || rsurface_model->surfmesh.data_vertexboneweights))
	{
		rsurface_modelvertex3f = rsurface_array_modelvertex3f;
		rsurface_modelsvector3f = NULL;
		rsurface_modeltvector3f = NULL;
		rsurface_modelnormal3f = NULL;
		Mod_Alias_GetMesh_Vertex3f(rsurface_model, rsurface_entity->frameblend, rsurface_array_modelvertex3f);
		rsurface_generatedvertex = true;
	}
	else
	{
		rsurface_modelvertex3f  = rsurface_model->surfmesh.data_vertex3f;
		rsurface_modelsvector3f = rsurface_model->surfmesh.data_svector3f;
		rsurface_modeltvector3f = rsurface_model->surfmesh.data_tvector3f;
		rsurface_modelnormal3f  = rsurface_model->surfmesh.data_normal3f;
		rsurface_generatedvertex = false;
	}
	rsurface_vertex3f  = rsurface_modelvertex3f;
	rsurface_svector3f = rsurface_modelsvector3f;
	rsurface_tvector3f = rsurface_modeltvector3f;
	rsurface_normal3f  = rsurface_modelnormal3f;
	rsurface_mode = RSURFMODE_NONE;
}

void RSurf_CleanUp(void)
{
	if (rsurface_mode == RSURFMODE_GLSL)
		qglUseProgramObjectARB(0);
	GL_AlphaTest(false);
	rsurface_mode = RSURFMODE_NONE;
	rsurface_lightmaptexture = NULL;
	rsurface_texture = NULL;
	rsurface_glsl_texture = NULL;
}

void RSurf_PrepareVerticesForBatch(qboolean generatenormals, qboolean generatetangents, int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	if (rsurface_generatedvertex)
	{
		if (rsurface_texture->textureflags & (Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
			generatetangents = true;
		if (generatetangents)
			generatenormals = true;
		if (generatenormals && !rsurface_modelnormal3f)
		{
			rsurface_normal3f = rsurface_modelnormal3f = rsurface_array_modelnormal3f;
			Mod_BuildNormals(0, rsurface_model->surfmesh.num_vertices, rsurface_model->surfmesh.num_triangles, rsurface_modelvertex3f, rsurface_model->surfmesh.data_element3i, rsurface_array_modelnormal3f, r_smoothnormals_areaweighting.integer);
		}
		if (generatetangents && !rsurface_modelsvector3f)
		{
			rsurface_svector3f = rsurface_modelsvector3f = rsurface_array_modelsvector3f;
			rsurface_tvector3f = rsurface_modeltvector3f = rsurface_array_modeltvector3f;
			Mod_BuildTextureVectorsFromNormals(0, rsurface_model->surfmesh.num_vertices, rsurface_model->surfmesh.num_triangles, rsurface_modelvertex3f, rsurface_model->surfmesh.data_texcoordtexture2f, rsurface_modelnormal3f, rsurface_model->surfmesh.data_element3i, rsurface_array_modelsvector3f, rsurface_array_modeltvector3f, r_smoothnormals_areaweighting.integer);
		}
	}
	if (rsurface_texture->textureflags & (Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
	{
		int texturesurfaceindex;
		float center[3], forward[3], right[3], up[3], v[4][3];
		matrix4x4_t matrix1, imatrix1;
		Matrix4x4_Transform(&rsurface_entity->inversematrix, r_viewforward, forward);
		Matrix4x4_Transform(&rsurface_entity->inversematrix, r_viewright, right);
		Matrix4x4_Transform(&rsurface_entity->inversematrix, r_viewup, up);
		// make deformed versions of only the vertices used by the specified surfaces
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			int i, j;
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			// a single autosprite surface can contain multiple sprites...
			for (j = 0;j < surface->num_vertices - 3;j += 4)
			{
				VectorClear(center);
				for (i = 0;i < 4;i++)
					VectorAdd(center, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i) * 3, center);
				VectorScale(center, 0.25f, center);
				if (rsurface_texture->textureflags & Q3TEXTUREFLAG_AUTOSPRITE2)
				{
					forward[0] = rsurface_modelorg[0] - center[0];
					forward[1] = rsurface_modelorg[1] - center[1];
					forward[2] = 0;
					VectorNormalize(forward);
					right[0] = forward[1];
					right[1] = -forward[0];
					right[2] = 0;
					VectorSet(up, 0, 0, 1);
				}
				// FIXME: calculate vectors from triangle edges instead of using texture vectors as an easy way out?
				Matrix4x4_FromVectors(&matrix1, (rsurface_normal3f + 3 * surface->num_firstvertex) + j*3, (rsurface_svector3f + 3 * surface->num_firstvertex) + j*3, (rsurface_tvector3f + 3 * surface->num_firstvertex) + j*3, center);
				Matrix4x4_Invert_Simple(&imatrix1, &matrix1);
				for (i = 0;i < 4;i++)
					Matrix4x4_Transform(&imatrix1, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i)*3, v[i]);
				for (i = 0;i < 4;i++)
					VectorMAMAMAM(1, center, v[i][0], forward, v[i][1], right, v[i][2], up, rsurface_array_modelvertex3f + (surface->num_firstvertex+i+j) * 3);
			}
			Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_modelvertex3f, rsurface_model->surfmesh.data_element3i + surface->num_firsttriangle * 3, rsurface_array_deformednormal3f, r_smoothnormals_areaweighting.integer);
			Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_modelvertex3f, rsurface_model->surfmesh.data_texcoordtexture2f, rsurface_array_deformednormal3f, rsurface_model->surfmesh.data_element3i + surface->num_firsttriangle * 3, rsurface_array_deformedsvector3f, rsurface_array_deformedtvector3f, r_smoothnormals_areaweighting.integer);
		}
		rsurface_vertex3f = rsurface_array_deformedvertex3f;
		rsurface_svector3f = rsurface_array_deformedsvector3f;
		rsurface_tvector3f = rsurface_array_deformedtvector3f;
		rsurface_normal3f = rsurface_array_deformednormal3f;
	}
	R_Mesh_VertexPointer(rsurface_vertex3f);
}

static void RSurf_Draw(const msurface_t *surface)
{
	GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
	R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface_model->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
}

static void RSurf_DrawLightmap(const msurface_t *surface, float r, float g, float b, float a, int lightmode, qboolean applycolor, qboolean applyfog)
{
	int i;
	float f;
	float *v, *c, *c2;
	if (lightmode >= 2)
	{
		// model lighting
		vec3_t ambientcolor;
		vec3_t diffusecolor;
		vec3_t lightdir;
		VectorCopy(rsurface_entity->modellight_lightdir, lightdir);
		ambientcolor[0] = rsurface_entity->modellight_ambient[0] * r * 0.5f;
		ambientcolor[1] = rsurface_entity->modellight_ambient[1] * g * 0.5f;
		ambientcolor[2] = rsurface_entity->modellight_ambient[2] * b * 0.5f;
		diffusecolor[0] = rsurface_entity->modellight_diffuse[0] * r * 0.5f;
		diffusecolor[1] = rsurface_entity->modellight_diffuse[1] * g * 0.5f;
		diffusecolor[2] = rsurface_entity->modellight_diffuse[2] * b * 0.5f;
		if (VectorLength2(diffusecolor) > 0)
		{
			int numverts = surface->num_vertices;
			v = rsurface_vertex3f + 3 * surface->num_firstvertex;
			c2 = rsurface_normal3f + 3 * surface->num_firstvertex;
			c = rsurface_array_color4f + 4 * surface->num_firstvertex;
			// q3-style directional shading
			for (i = 0;i < numverts;i++, v += 3, c2 += 3, c += 4)
			{
				if ((f = DotProduct(c2, lightdir)) > 0)
					VectorMA(ambientcolor, f, diffusecolor, c);
				else
					VectorCopy(ambientcolor, c);
				c[3] = a;
			}
			r = 1;
			g = 1;
			b = 1;
			a = 1;
			applycolor = false;
			rsurface_lightmapcolor4f = rsurface_array_color4f;
		}
		else
		{
			r = ambientcolor[0];
			g = ambientcolor[1];
			b = ambientcolor[2];
			rsurface_lightmapcolor4f = NULL;
		}
	}
	else if (lightmode >= 1)
	{
		if (surface->lightmapinfo && surface->lightmapinfo->stainsamples)
		{
			for (i = 0, c = rsurface_array_color4f + 4 * surface->num_firstvertex;i < surface->num_vertices;i++, c += 4)
			{
				if (surface->lightmapinfo->samples)
				{
					const unsigned char *lm = surface->lightmapinfo->samples + (rsurface_model->surfmesh.data_lightmapoffsets + surface->num_firstvertex)[i];
					float scale = r_refdef.lightstylevalue[surface->lightmapinfo->styles[0]] * (1.0f / 32768.0f);
					VectorScale(lm, scale, c);
					if (surface->lightmapinfo->styles[1] != 255)
					{
						int size3 = ((surface->lightmapinfo->extents[0]>>4)+1)*((surface->lightmapinfo->extents[1]>>4)+1)*3;
						lm += size3;
						scale = r_refdef.lightstylevalue[surface->lightmapinfo->styles[1]] * (1.0f / 32768.0f);
						VectorMA(c, scale, lm, c);
						if (surface->lightmapinfo->styles[2] != 255)
						{
							lm += size3;
							scale = r_refdef.lightstylevalue[surface->lightmapinfo->styles[2]] * (1.0f / 32768.0f);
							VectorMA(c, scale, lm, c);
							if (surface->lightmapinfo->styles[3] != 255)
							{
								lm += size3;
								scale = r_refdef.lightstylevalue[surface->lightmapinfo->styles[3]] * (1.0f / 32768.0f);
								VectorMA(c, scale, lm, c);
							}
						}
					}
				}
				else
					VectorClear(c);
			}
			rsurface_lightmapcolor4f = rsurface_array_color4f;
		}
		else
			rsurface_lightmapcolor4f = rsurface_model->surfmesh.data_lightmapcolor4f;
	}
	else
		rsurface_lightmapcolor4f = NULL;
	if (applyfog)
	{
		if (rsurface_lightmapcolor4f)
		{
			for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c = (rsurface_lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface_array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4, c2 += 4)
			{
				f = 1 - VERTEXFOGTABLE(VectorDistance(v, rsurface_modelorg));
				c2[0] = c[0] * f;
				c2[1] = c[1] * f;
				c2[2] = c[2] * f;
				c2[3] = c[3];
			}
		}
		else
		{
			for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c2 = (rsurface_array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c2 += 4)
			{
				f = 1 - VERTEXFOGTABLE(VectorDistance(v, rsurface_modelorg));
				c2[0] = f;
				c2[1] = f;
				c2[2] = f;
				c2[3] = 1;
			}
		}
		rsurface_lightmapcolor4f = rsurface_array_color4f;
	}
	if (applycolor && rsurface_lightmapcolor4f)
	{
		for (i = 0, c = (rsurface_lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface_array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, c += 4, c2 += 4)
		{
			c2[0] = c[0] * r;
			c2[1] = c[1] * g;
			c2[2] = c[2] * b;
			c2[3] = c[3] * a;
		}
		rsurface_lightmapcolor4f = rsurface_array_color4f;
	}
	R_Mesh_ColorPointer(rsurface_lightmapcolor4f);
	GL_Color(r, g, b, a);
	RSurf_Draw(surface);
}

static void R_DrawTextureSurfaceList(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int lightmode;
	qboolean applycolor;
	qboolean applyfog;
	rmeshstate_t m;
	if (rsurface_texture->currentmaterialflags & MATERIALFLAG_NODRAW)
		return;
	r_shadow_rtlight = NULL;
	renderstats.entities_surfaces += texturenumsurfaces;
	// FIXME: identify models using a better check than rsurface_model->brush.shadowmesh
	lightmode = ((rsurface_entity->effects & EF_FULLBRIGHT) || rsurface_model->brush.shadowmesh) ? 0 : 2;
	GL_DepthTest(!(rsurface_texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST));
	if ((rsurface_texture->textureflags & Q3TEXTUREFLAG_TWOSIDED) || (rsurface_entity->flags & RENDER_NOCULLFACE))
		qglDisable(GL_CULL_FACE);
	if (r_showsurfaces.integer)
	{
		if (rsurface_mode != RSURFMODE_SHOWSURFACES)
		{
			rsurface_mode = RSURFMODE_SHOWSURFACES;
			GL_DepthMask(true);
			GL_BlendFunc(GL_ONE, GL_ZERO);
			R_Mesh_ColorPointer(NULL);
			R_Mesh_ResetTextureState();
		}
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			int k = (int)(((size_t)surface) / sizeof(msurface_t));
			GL_Color((k & 15) * (1.0f / 16.0f), ((k >> 4) & 15) * (1.0f / 16.0f), ((k >> 8) & 15) * (1.0f / 16.0f), 0.2f);
			RSurf_Draw(surface);
		}
	}
	else if (rsurface_texture->currentmaterialflags & MATERIALFLAG_SKY)
	{
		// transparent sky would be ridiculous
		if (!(rsurface_texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
		{
			if (rsurface_mode != RSURFMODE_SKY)
			{
				if (rsurface_mode == RSURFMODE_GLSL)
					qglUseProgramObjectARB(0);
				rsurface_mode = RSURFMODE_SKY;
				if (skyrendernow)
				{
					skyrendernow = false;
					R_Sky();
					// restore entity matrix
					R_Mesh_Matrix(&rsurface_entity->matrix);
				}
				GL_DepthMask(true);
				// LordHavoc: HalfLife maps have freaky skypolys so don't use
				// skymasking on them, and Quake3 never did sky masking (unlike
				// software Quake and software Quake2), so disable the sky masking
				// in Quake3 maps as it causes problems with q3map2 sky tricks,
				// and skymasking also looks very bad when noclipping outside the
				// level, so don't use it then either.
				if (rsurface_model->type == mod_brushq1 && r_q1bsp_skymasking.integer && !r_worldnovis)
				{
					GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], 1);
					R_Mesh_ColorPointer(NULL);
					R_Mesh_ResetTextureState();
					if (skyrendermasked)
					{
						// depth-only (masking)
						GL_ColorMask(0,0,0,0);
						// just to make sure that braindead drivers don't draw
						// anything despite that colormask...
						GL_BlendFunc(GL_ZERO, GL_ONE);
					}
					else
					{
						// fog sky
						GL_BlendFunc(GL_ONE, GL_ZERO);
					}
				}
			}
			// LordHavoc: HalfLife maps have freaky skypolys so don't use
			// skymasking on them, and Quake3 never did sky masking (unlike
			// software Quake and software Quake2), so disable the sky masking
			// in Quake3 maps as it causes problems with q3map2 sky tricks,
			// and skymasking also looks very bad when noclipping outside the
			// level, so don't use it then either.
			if (rsurface_model->type == mod_brushq1 && r_q1bsp_skymasking.integer && !r_worldnovis)
			{
				RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					RSurf_Draw(surface);
				}
				if (skyrendermasked)
					GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
			}
		}
	}
	else if (rsurface_texture->currentnumlayers && r_glsl.integer && gl_support_fragment_shader)
	{
		if (rsurface_mode != RSURFMODE_GLSL)
		{
			rsurface_mode = RSURFMODE_GLSL;
			rsurface_glsl_texture = NULL;
			R_Mesh_ResetTextureState();
		}
		if (rsurface_glsl_texture != rsurface_texture)
		{
			rsurface_glsl_texture = rsurface_texture;
			GL_BlendFunc(rsurface_texture->currentlayers[0].blendfunc1, rsurface_texture->currentlayers[0].blendfunc2);
			GL_DepthMask(!(rsurface_texture->currentmaterialflags & MATERIALFLAG_BLENDED));
			GL_Color(rsurface_entity->colormod[0], rsurface_entity->colormod[1], rsurface_entity->colormod[2], rsurface_texture->currentalpha);
			R_SetupSurfaceShader(vec3_origin, lightmode == 2);
			//permutation_deluxemapping = permutation_lightmapping = R_SetupSurfaceShader(vec3_origin, lightmode == 2, false);
			//if (r_glsl_deluxemapping.integer)
			//	permutation_deluxemapping = R_SetupSurfaceShader(vec3_origin, lightmode == 2, true);
			R_Mesh_TexCoordPointer(0, 2, rsurface_model->surfmesh.data_texcoordtexture2f);
			if (lightmode != 2)
				R_Mesh_TexCoordPointer(4, 2, rsurface_model->surfmesh.data_texcoordlightmap2f);
			GL_AlphaTest((rsurface_texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		}
		if (!r_glsl_permutation)
			return;
		RSurf_PrepareVerticesForBatch(true, true, texturenumsurfaces, texturesurfacelist);
		R_Mesh_TexCoordPointer(1, 3, rsurface_svector3f);
		R_Mesh_TexCoordPointer(2, 3, rsurface_tvector3f);
		R_Mesh_TexCoordPointer(3, 3, rsurface_normal3f);
		if (lightmode != 2)
		{
			if (rsurface_lightmaptexture)
			{
				R_Mesh_TexBind(7, R_GetTexture(rsurface_lightmaptexture));
				if (r_glsl_permutation->loc_Texture_Deluxemap >= 0)
					R_Mesh_TexBind(8, R_GetTexture(texturesurfacelist[0]->deluxemaptexture));
				R_Mesh_ColorPointer(NULL);
			}
			else
			{
				R_Mesh_TexBind(7, R_GetTexture(r_texture_white));
				if (r_glsl_permutation->loc_Texture_Deluxemap >= 0)
					R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
				R_Mesh_ColorPointer(rsurface_model->surfmesh.data_lightmapcolor4f);
			}
		}
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			RSurf_Draw(surface);
		}
	}
	else if (rsurface_texture->currentnumlayers)
	{
		int layerindex;
		const texturelayer_t *layer;
		if (rsurface_mode != RSURFMODE_MULTIPASS)
		{
			if (rsurface_mode == RSURFMODE_GLSL)
				qglUseProgramObjectARB(0);
			rsurface_mode = RSURFMODE_MULTIPASS;
		}
		RSurf_PrepareVerticesForBatch(true, false, texturenumsurfaces, texturesurfacelist);
		for (layerindex = 0, layer = rsurface_texture->currentlayers;layerindex < rsurface_texture->currentnumlayers;layerindex++, layer++)
		{
			vec4_t layercolor;
			int layertexrgbscale;
			if (rsurface_texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
			{
				if (layerindex == 0)
					GL_AlphaTest(true);
				else
				{
					GL_AlphaTest(false);
					qglDepthFunc(GL_EQUAL);
				}
			}
			GL_DepthMask(layer->depthmask);
			GL_BlendFunc(layer->blendfunc1, layer->blendfunc2);
			if ((layer->color[0] > 2 || layer->color[1] > 2 || layer->color[2] > 2) && (gl_combine.integer || layer->depthmask))
			{
				layertexrgbscale = 4;
				VectorScale(layer->color, 0.25f, layercolor);
			}
			else if ((layer->color[0] > 1 || layer->color[1] > 1 || layer->color[2] > 1) && (gl_combine.integer || layer->depthmask))
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
			R_Mesh_ColorPointer(NULL);
			GL_Color(layercolor[0], layercolor[1], layercolor[2], layercolor[3]);
			applycolor = layercolor[0] != 1 || layercolor[1] != 1 || layercolor[2] != 1 || layercolor[3] != 1;
			applyfog = (layer->flags & TEXTURELAYERFLAG_FOGDARKEN) != 0;
			switch (layer->type)
			{
			case TEXTURELAYERTYPE_LITTEXTURE_COMBINE:
				memset(&m, 0, sizeof(m));
				m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordlightmap2f;
				m.tex[1] = R_GetTexture(layer->texture);
				m.texmatrix[1] = layer->texmatrix;
				m.texrgbscale[1] = layertexrgbscale;
				m.pointer_texcoord[1] = rsurface_model->surfmesh.data_texcoordtexture2f;
				R_Mesh_TextureState(&m);
				if (lightmode == 2)
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
						RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 2, applycolor, applyfog);
					}
				}
				else if (rsurface_lightmaptexture)
				{
					R_Mesh_TexBind(0, R_GetTexture(rsurface_lightmaptexture));
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 0, applycolor, applyfog);
					}
				}
				else
				{
					R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 1, applycolor, applyfog);
					}
				}
				break;
			case TEXTURELAYERTYPE_LITTEXTURE_MULTIPASS:
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.texrgbscale[0] = layertexrgbscale;
				m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordlightmap2f;
				R_Mesh_TextureState(&m);
				if (lightmode == 2)
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
						RSurf_DrawLightmap(surface, 1, 1, 1, 1, 2, false, false);
					}
				}
				else if (rsurface_lightmaptexture)
				{
					R_Mesh_TexBind(0, R_GetTexture(rsurface_lightmaptexture));
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, 1, 1, 1, 1, 0, false, false);
					}
				}
				else
				{
					R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, 1, 1, 1, 1, 1, false, false);
					}
				}
				GL_LockArrays(0, 0);
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.texrgbscale[0] = layertexrgbscale;
				m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordtexture2f;
				R_Mesh_TextureState(&m);
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 0, applycolor, false);
				}
				break;
			case TEXTURELAYERTYPE_LITTEXTURE_VERTEX:
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.texrgbscale[0] = layertexrgbscale;
				m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordtexture2f;
				R_Mesh_TextureState(&m);
				if (lightmode == 2)
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 2, applycolor, applyfog);
					}
				}
				else
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
						RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 1, applycolor, applyfog);
					}
				}
				break;
			case TEXTURELAYERTYPE_TEXTURE:
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.texrgbscale[0] = layertexrgbscale;
				m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordtexture2f;
				R_Mesh_TextureState(&m);
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					RSurf_DrawLightmap(surface, layercolor[0], layercolor[1], layercolor[2], layercolor[3], 0, applycolor, applyfog);
				}
				break;
			case TEXTURELAYERTYPE_FOG:
				R_Mesh_ColorPointer(rsurface_array_color4f);
				if (layer->texture)
				{
					memset(&m, 0, sizeof(m));
					m.tex[0] = R_GetTexture(layer->texture);
					m.texmatrix[0] = layer->texmatrix;
					m.pointer_texcoord[0] = rsurface_model->surfmesh.data_texcoordtexture2f;
					R_Mesh_TextureState(&m);
				}
				else
					R_Mesh_ResetTextureState();
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					int i;
					float f, *v, *c;
					const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c = (rsurface_array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
					{
						f = VERTEXFOGTABLE(VectorDistance(v, rsurface_modelorg));
						c[0] = layercolor[0];
						c[1] = layercolor[1];
						c[2] = layercolor[2];
						c[3] = f * layercolor[3];
					}
					RSurf_Draw(surface);
				}
				break;
			default:
				Con_Printf("R_DrawTextureSurfaceList: unknown layer type %i\n", layer->type);
			}
			GL_LockArrays(0, 0);
			// if trying to do overbright on first pass of an opaque surface
			// when combine is not supported, brighten as a post process
			if (layertexrgbscale > 1 && !gl_combine.integer && layer->depthmask)
			{
				int scale;
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				R_Mesh_ColorPointer(NULL);
				GL_Color(1, 1, 1, 1);
				R_Mesh_ResetTextureState();
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
					for (scale = 1;scale < layertexrgbscale;scale <<= 1)
						RSurf_Draw(surface);
				}
				GL_LockArrays(0, 0);
			}
		}
		if (rsurface_texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
		{
			qglDepthFunc(GL_LEQUAL);
			GL_AlphaTest(false);
		}
	}
	GL_LockArrays(0, 0);
	if ((rsurface_texture->textureflags & Q3TEXTUREFLAG_TWOSIDED) || (rsurface_entity->flags & RENDER_NOCULLFACE))
		qglEnable(GL_CULL_FACE);
}

static void R_DrawSurface_TransparentCallback(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	msurface_t *surface = ent->model->data_surfaces + surfacenumber;

	rsurface_texture = surface->texture;
	if (rsurface_texture->basematerialflags & MATERIALFLAG_SKY)
		return; // transparent sky is too difficult

	RSurf_ActiveEntity(ent);
	R_UpdateTextureInfo(ent, rsurface_texture);
	rsurface_texture = rsurface_texture->currentframe;
	R_DrawTextureSurfaceList(1, &surface);
	RSurf_CleanUp();
}

void R_QueueTextureSurfaceList(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	vec3_t tempcenter, center;
	if (rsurface_texture->currentmaterialflags & MATERIALFLAG_BLENDED)
	{
		// drawing sky transparently would be too difficult
		if (!(rsurface_texture->currentmaterialflags & MATERIALFLAG_SKY))
		{
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
				tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
				tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
				Matrix4x4_Transform(&rsurface_entity->matrix, tempcenter, center);
				R_MeshQueue_AddTransparent(rsurface_texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST ? r_vieworigin : center, R_DrawSurface_TransparentCallback, rsurface_entity, surface - rsurface_model->data_surfaces, r_shadow_rtlight);
			}
		}
	}
	else
		R_DrawTextureSurfaceList(texturenumsurfaces, texturesurfacelist);
}

extern void R_BuildLightMap(const entity_render_t *ent, msurface_t *surface);
void R_DrawSurfaces(entity_render_t *ent, qboolean skysurfaces)
{
	int i, j, f, flagsmask;
	int counttriangles = 0;
	texture_t *t;
	model_t *model = ent->model;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	msurface_t *surfacelist[1024];
	if (model == NULL)
		return;

	RSurf_ActiveEntity(ent);

	// update light styles
	if (!skysurfaces && model->brushq1.light_styleupdatechains)
	{
		msurface_t *surface, **surfacechain;
		for (i = 0;i < model->brushq1.light_styles;i++)
		{
			if (model->brushq1.light_stylevalue[i] != r_refdef.lightstylevalue[model->brushq1.light_style[i]])
			{
				model->brushq1.light_stylevalue[i] = r_refdef.lightstylevalue[model->brushq1.light_style[i]];
				if ((surfacechain = model->brushq1.light_styleupdatechains[i]))
					for (;(surface = *surfacechain);surfacechain++)
						surface->cached_dlight = true;
			}
		}
	}

	R_UpdateAllTextureInfo(ent);
	flagsmask = skysurfaces ? MATERIALFLAG_SKY : (MATERIALFLAG_WATER | MATERIALFLAG_WALL);
	f = 0;
	t = NULL;
	rsurface_lightmaptexture = NULL;
	rsurface_texture = NULL;
	numsurfacelist = 0;
	if (ent == r_refdef.worldentity)
	{
		msurface_t *surface;
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (!r_worldsurfacevisible[j])
				continue;
			if (t != surface->texture || rsurface_lightmaptexture != surface->lightmaptexture)
			{
				if (numsurfacelist)
				{
					R_QueueTextureSurfaceList(numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
				t = surface->texture;
				rsurface_lightmaptexture = surface->lightmaptexture;
				rsurface_texture = t->currentframe;
				f = rsurface_texture->currentmaterialflags & flagsmask;
			}
			if (f && surface->num_triangles)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight)
					R_BuildLightMap(ent, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				counttriangles += surface->num_triangles;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_QueueTextureSurfaceList(numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
			}
		}
	}
	else
	{
		msurface_t *surface;
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (t != surface->texture || rsurface_lightmaptexture != surface->lightmaptexture)
			{
				if (numsurfacelist)
				{
					R_QueueTextureSurfaceList(numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
				t = surface->texture;
				rsurface_lightmaptexture = surface->lightmaptexture;
				rsurface_texture = t->currentframe;
				f = rsurface_texture->currentmaterialflags & flagsmask;
			}
			if (f && surface->num_triangles)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight)
					R_BuildLightMap(ent, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				counttriangles += surface->num_triangles;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_QueueTextureSurfaceList(numsurfacelist, surfacelist);
					numsurfacelist = 0;
				}
			}
		}
	}
	if (numsurfacelist)
		R_QueueTextureSurfaceList(numsurfacelist, surfacelist);
	renderstats.entities_triangles += counttriangles;
	RSurf_CleanUp();

	if (r_showcollisionbrushes.integer && model->brush.num_brushes && !skysurfaces)
	{
		int i;
		const msurface_t *surface;
		q3mbrush_t *brush;
		R_Mesh_Matrix(&ent->matrix);
		R_Mesh_ColorPointer(NULL);
		R_Mesh_ResetTextureState();
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(!r_showdisabledepthtest.integer);
		qglPolygonOffset(r_polygonfactor + r_showcollisionbrushes_polygonfactor.value, r_polygonoffset + r_showcollisionbrushes_polygonoffset.value);
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && brush->colbrushf->numtriangles)
				R_DrawCollisionBrush(brush->colbrushf);
		for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
			if (surface->num_collisiontriangles)
				R_DrawCollisionSurface(ent, surface);
		qglPolygonOffset(r_polygonfactor, r_polygonoffset);
	}

	if (r_showtris.integer || r_shownormals.integer)
	{
		int k, l;
		msurface_t *surface;
		const int *elements;
		vec3_t v;
		GL_DepthTest(true);
		GL_DepthMask(true);
		if (r_showdisabledepthtest.integer)
			qglDepthFunc(GL_ALWAYS);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		R_Mesh_ColorPointer(NULL);
		R_Mesh_ResetTextureState();
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (ent == r_refdef.worldentity && !r_worldsurfacevisible[j])
				continue;
			rsurface_texture = surface->texture->currentframe;
			if ((rsurface_texture->currentmaterialflags & flagsmask) && surface->num_triangles)
			{
				RSurf_PrepareVerticesForBatch(true, true, 1, &surface);
				if (r_showtris.integer)
				{
					if (!rsurface_texture->currentlayers->depthmask)
						GL_Color(r_showtris.value, 0, 0, 1);
					else if (ent == r_refdef.worldentity)
						GL_Color(r_showtris.value, r_showtris.value, r_showtris.value, 1);
					else
						GL_Color(0, r_showtris.value, 0, 1);
					elements = (ent->model->surfmesh.data_element3i + 3 * surface->num_firsttriangle);
					qglBegin(GL_LINES);
					for (k = 0;k < surface->num_triangles;k++, elements += 3)
					{
						qglArrayElement(elements[0]);qglArrayElement(elements[1]);
						qglArrayElement(elements[1]);qglArrayElement(elements[2]);
						qglArrayElement(elements[2]);qglArrayElement(elements[0]);
					}
					qglEnd();
				}
				if (r_shownormals.integer)
				{
					GL_Color(r_shownormals.value, 0, 0, 1);
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface_vertex3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, 8, rsurface_svector3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					GL_Color(0, 0, r_shownormals.value, 1);
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface_vertex3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, 8, rsurface_tvector3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
					GL_Color(0, r_shownormals.value, 0, 1);
					qglBegin(GL_LINES);
					for (k = 0, l = surface->num_firstvertex;k < surface->num_vertices;k++, l++)
					{
						VectorCopy(rsurface_vertex3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
						VectorMA(v, 8, rsurface_normal3f + l * 3, v);
						qglVertex3f(v[0], v[1], v[2]);
					}
					qglEnd();
				}
			}
		}
		rsurface_texture = NULL;
		if (r_showdisabledepthtest.integer)
			qglDepthFunc(GL_LEQUAL);
	}
}

