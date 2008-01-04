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

mempool_t *r_main_mempool;
rtexturepool_t *r_main_texturepool;

//
// screen size info
//
r_refdef_t r_refdef;

cvar_t r_depthfirst = {CVAR_SAVE, "r_depthfirst", "1", "renders a depth-only version of the scene before normal rendering begins to eliminate overdraw, values: 0 = off, 1 = world depth, 2 = world and model depth"};
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
cvar_t r_cullentities_trace_samples = {0, "r_cullentities_trace_samples", "2", "number of samples to test for entity culling"};
cvar_t r_cullentities_trace_enlarge = {0, "r_cullentities_trace_enlarge", "0", "box enlargement for entity culling"};
cvar_t r_cullentities_trace_delay = {0, "r_cullentities_trace_delay", "1", "number of seconds until the entity gets actually culled"};
cvar_t r_speeds = {0, "r_speeds","0", "displays rendering statistics and per-subsystem timings"};
cvar_t r_fullbright = {0, "r_fullbright","0", "makes map very bright and renders faster"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1", "opacity of water polygons"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1", "enables dynamic lights (rocket glow and such)"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1", "enables glowing pixels in quake textures (changes need r_restart to take effect)"};
cvar_t r_shadows = {CVAR_SAVE, "r_shadows", "0", "casts fake stencil shadows from models onto the world (rtlights are unaffected by this)"};
cvar_t r_shadows_throwdistance = {CVAR_SAVE, "r_shadows_throwdistance", "500", "how far to cast shadows from models"};
cvar_t r_q1bsp_skymasking = {0, "r_q1bsp_skymasking", "1", "allows sky polygons in quake1 maps to obscure other geometry"};
cvar_t r_polygonoffset_submodel_factor = {0, "r_polygonoffset_submodel_factor", "0", "biases depth values of world submodels such as doors, to prevent z-fighting artifacts in Quake maps"};
cvar_t r_polygonoffset_submodel_offset = {0, "r_polygonoffset_submodel_offset", "2", "biases depth values of world submodels such as doors, to prevent z-fighting artifacts in Quake maps"};
cvar_t r_fog_exp2 = {0, "r_fog_exp2", "0", "uses GL_EXP2 fog (as in Nehahra) rather than realistic GL_EXP fog"};

cvar_t gl_fogenable = {0, "gl_fogenable", "0", "nehahra fog enable (for Nehahra compatibility only)"};
cvar_t gl_fogdensity = {0, "gl_fogdensity", "0.25", "nehahra fog density (recommend values below 0.1) (for Nehahra compatibility only)"};
cvar_t gl_fogred = {0, "gl_fogred","0.3", "nehahra fog color red value (for Nehahra compatibility only)"};
cvar_t gl_foggreen = {0, "gl_foggreen","0.3", "nehahra fog color green value (for Nehahra compatibility only)"};
cvar_t gl_fogblue = {0, "gl_fogblue","0.3", "nehahra fog color blue value (for Nehahra compatibility only)"};
cvar_t gl_fogstart = {0, "gl_fogstart", "0", "nehahra fog start distance (for Nehahra compatibility only)"};
cvar_t gl_fogend = {0, "gl_fogend","0", "nehahra fog end distance (for Nehahra compatibility only)"};
cvar_t gl_skyclip = {0, "gl_skyclip", "4608", "nehahra farclip distance - the real fog end (for Nehahra compatibility only)"};

cvar_t r_textureunits = {0, "r_textureunits", "32", "number of hardware texture units reported by driver (note: setting this to 1 turns off gl_combine)"};

cvar_t r_glsl = {CVAR_SAVE, "r_glsl", "1", "enables use of OpenGL 2.0 pixel shaders for lighting"};
cvar_t r_glsl_offsetmapping = {CVAR_SAVE, "r_glsl_offsetmapping", "0", "offset mapping effect (also known as parallax mapping or virtual displacement mapping)"};
cvar_t r_glsl_offsetmapping_reliefmapping = {CVAR_SAVE, "r_glsl_offsetmapping_reliefmapping", "0", "relief mapping effect (higher quality)"};
cvar_t r_glsl_offsetmapping_scale = {CVAR_SAVE, "r_glsl_offsetmapping_scale", "0.04", "how deep the offset mapping effect is"};
cvar_t r_glsl_deluxemapping = {CVAR_SAVE, "r_glsl_deluxemapping", "1", "use per pixel lighting on deluxemap-compiled q3bsp maps (or a value of 2 forces deluxemap shading even without deluxemaps)"};
cvar_t r_glsl_contrastboost = {CVAR_SAVE, "r_glsl_contrastboost", "1", "by how much to multiply the contrast in dark areas (1 is no change)"};

cvar_t r_water = {CVAR_SAVE, "r_water", "0", "whether to use reflections and refraction on water surfaces (note: r_wateralpha must be set below 1)"};
cvar_t r_water_clippingplanebias = {CVAR_SAVE, "r_water_clippingplanebias", "1", "a rather technical setting which avoids black pixels around water edges"};
cvar_t r_water_resolutionmultiplier = {CVAR_SAVE, "r_water_resolutionmultiplier", "0.5", "multiplier for screen resolution when rendering refracted/reflected scenes, 1 is full quality, lower values are faster"};
cvar_t r_water_refractdistort = {CVAR_SAVE, "r_water_refractdistort", "0.01", "how much water refractions shimmer"};
cvar_t r_water_reflectdistort = {CVAR_SAVE, "r_water_reflectdistort", "0.01", "how much water reflections shimmer"};

cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "1", "enables animation smoothing on sprites (requires r_lerpmodels 1)"};
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

extern qboolean v_flipped_state;

typedef struct r_glsl_bloomshader_s
{
	int program;
	int loc_Texture_Bloom;
}
r_glsl_bloomshader_t;

static struct r_bloomstate_s
{
	qboolean enabled;
	qboolean hdr;

	int bloomwidth, bloomheight;

	int screentexturewidth, screentextureheight;
	rtexture_t *texture_screen;

	int bloomtexturewidth, bloomtextureheight;
	rtexture_t *texture_bloom;

	r_glsl_bloomshader_t *shader;

	// arrays for rendering the screen passes
	float screentexcoord2f[8];
	float bloomtexcoord2f[8];
	float offsettexcoord2f[8];
}
r_bloomstate;

typedef struct r_waterstate_waterplane_s
{
	rtexture_t *texture_refraction;
	rtexture_t *texture_reflection;
	mplane_t plane;
	int materialflags; // combined flags of all water surfaces on this plane
	unsigned char pvsbits[(32768+7)>>3]; // FIXME: buffer overflow on huge maps
	qboolean pvsvalid;
}
r_waterstate_waterplane_t;

#define MAX_WATERPLANES 16

static struct r_waterstate_s
{
	qboolean enabled;

	qboolean renderingscene; // true while rendering a refraction or reflection texture, disables water surfaces

	int waterwidth, waterheight;
	int texturewidth, textureheight;

	int maxwaterplanes; // same as MAX_WATERPLANES
	int numwaterplanes;
	r_waterstate_waterplane_t waterplanes[MAX_WATERPLANES];

	float screenscale[2];
	float screencenter[2];
}
r_waterstate;

// shadow volume bsp struct with automatically growing nodes buffer
svbsp_t r_svbsp;

rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_grey128;
rtexture_t *r_texture_black;
rtexture_t *r_texture_notexture;
rtexture_t *r_texture_whitecube;
rtexture_t *r_texture_normalizationcube;
rtexture_t *r_texture_fogattenuation;
//rtexture_t *r_texture_fogintensity;

char r_qwskincache[MAX_SCOREBOARD][MAX_QPATH];
skinframe_t *r_qwskincache_skinframe[MAX_SCOREBOARD];

// vertex coordinates for a quad that covers the screen exactly
const static float r_screenvertex3f[12] =
{
	0, 0, 0,
	1, 0, 0,
	1, 1, 0,
	0, 1, 0
};

extern void R_DrawModelShadows(void);

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
	r_refdef.fog_end = 0;
}

float FogForDistance(vec_t dist)
{
	unsigned int fogmasktableindex = (unsigned int)(dist * r_refdef.fogmasktabledistmultiplier);
	return r_refdef.fogmasktable[min(fogmasktableindex, FOGMASKTABLEWIDTH - 1)];
}

float FogPoint_World(const vec3_t p)
{
	return FogForDistance(VectorDistance((p), r_refdef.view.origin));
}

float FogPoint_Model(const vec3_t p)
{
	return FogForDistance(VectorDistance((p), rsurface.modelorg));
}

static void R_BuildBlankTextures(void)
{
	unsigned char data[4];
	data[2] = 128; // normal X
	data[1] = 128; // normal Y
	data[0] = 255; // normal Z
	data[3] = 128; // height
	r_texture_blanknormalmap = R_LoadTexture2D(r_main_texturepool, "blankbump", 1, 1, data, TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_PERSISTENT, NULL);
	data[0] = 255;
	data[1] = 255;
	data[2] = 255;
	data[3] = 255;
	r_texture_white = R_LoadTexture2D(r_main_texturepool, "blankwhite", 1, 1, data, TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_PERSISTENT, NULL);
	data[0] = 128;
	data[1] = 128;
	data[2] = 128;
	data[3] = 255;
	r_texture_grey128 = R_LoadTexture2D(r_main_texturepool, "blankgrey128", 1, 1, data, TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_PERSISTENT, NULL);
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 255;
	r_texture_black = R_LoadTexture2D(r_main_texturepool, "blankblack", 1, 1, data, TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_PERSISTENT, NULL);
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
	r_texture_whitecube = R_LoadTextureCubeMap(r_main_texturepool, "whitecube", 1, data, TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_PERSISTENT, NULL);
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
				data[side][y][x][2] = (unsigned char)(128.0f + intensity * v[0]);
				data[side][y][x][1] = (unsigned char)(128.0f + intensity * v[1]);
				data[side][y][x][0] = (unsigned char)(128.0f + intensity * v[2]);
				data[side][y][x][3] = 255;
			}
		}
	}
	r_texture_normalizationcube = R_LoadTextureCubeMap(r_main_texturepool, "normalcube", NORMSIZE, &data[0][0][0][0], TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_CLAMP | TEXF_PERSISTENT, NULL);
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
		if(developer.integer >= 100)
			Con_Printf("%f ", d);
		d = max(0, d);
		if (r_fog_exp2.integer)
			alpha = exp(-r_refdef.fogmasktable_density * r_refdef.fogmasktable_density * 0.0001 * d * d);
		else
			alpha = exp(-r_refdef.fogmasktable_density * 0.004 * d);
		if(developer.integer >= 100)
			Con_Printf(" : %f ", alpha);
		alpha = 1 - (1 - alpha) * r_refdef.fogmasktable_alpha;
		if(developer.integer >= 100)
			Con_Printf(" = %f\n", alpha);
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
		r_texture_fogattenuation = R_LoadTexture2D(r_main_texturepool, "fogattenuation", FOGWIDTH, 1, &data1[0][0], TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_PERSISTENT, NULL);
		//r_texture_fogintensity = R_LoadTexture2D(r_main_texturepool, "fogintensity", FOGWIDTH, 1, &data2[0][0], TEXTYPE_BGRA, TEXF_PRECACHE | TEXF_FORCELINEAR | TEXF_CLAMP, NULL);
	}
}

static const char *builtinshaderstring =
"// ambient+diffuse+specular+normalmap+attenuation+cubemap+fog shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"\n"
"// common definitions between vertex shader and fragment shader:\n"
"\n"
"#ifdef __GLSL_CG_DATA_TYPES\n"
"# define myhalf half\n"
"# define myhvec2 hvec2\n"
"# define myhvec3 hvec3\n"
"# define myhvec4 hvec4\n"
"#else\n"
"# define myhalf float\n"
"# define myhvec2 vec2\n"
"# define myhvec3 vec3\n"
"# define myhvec4 vec4\n"
"#endif\n"
"\n"
"varying vec2 TexCoord;\n"
"varying vec2 TexCoordLightmap;\n"
"\n"
"//#ifdef MODE_LIGHTSOURCE\n"
"varying vec3 CubeVector;\n"
"//#endif\n"
"\n"
"//#ifdef MODE_LIGHTSOURCE\n"
"varying vec3 LightVector;\n"
"//#else\n"
"//# ifdef MODE_LIGHTDIRECTION\n"
"//varying vec3 LightVector;\n"
"//# endif\n"
"//#endif\n"
"\n"
"varying vec3 EyeVector;\n"
"//#ifdef USEFOG\n"
"varying vec3 EyeVectorModelSpace;\n"
"//#endif\n"
"\n"
"varying vec3 VectorS; // direction of S texcoord (sometimes crudely called tangent)\n"
"varying vec3 VectorT; // direction of T texcoord (sometimes crudely called binormal)\n"
"varying vec3 VectorR; // direction of R texcoord (surface normal)\n"
"\n"
"//#ifdef MODE_WATER\n"
"varying vec4 ModelViewProjectionPosition;\n"
"//#else\n"
"//# ifdef MODE_REFRACTION\n"
"//varying vec4 ModelViewProjectionPosition;\n"
"//# else\n"
"//#  ifdef USEREFLECTION\n"
"//varying vec4 ModelViewProjectionPosition;\n"
"//#  endif\n"
"//# endif\n"
"//#endif\n"
"\n"
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
"#ifndef MODE_LIGHTSOURCE\n"
"# ifndef MODE_LIGHTDIRECTION\n"
"	TexCoordLightmap = vec2(gl_MultiTexCoord4);\n"
"# endif\n"
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
"//#if defined(MODE_WATER) || defined(MODE_REFRACTION) || defined(USEREFLECTION)\n"
"//	ModelViewProjectionPosition = gl_Vertex * gl_ModelViewProjectionMatrix;\n"
"//	//ModelViewProjectionPosition_svector = (gl_Vertex + vec4(gl_MultiTexCoord1.xyz, 0)) * gl_ModelViewProjectionMatrix - ModelViewProjectionPosition;\n"
"//	//ModelViewProjectionPosition_tvector = (gl_Vertex + vec4(gl_MultiTexCoord2.xyz, 0)) * gl_ModelViewProjectionMatrix - ModelViewProjectionPosition;\n"
"//#endif\n"
"\n"
"// transform vertex to camera space, using ftransform to match non-VS\n"
"	// rendering\n"
"	gl_Position = ftransform();\n"
"\n"
"#ifdef MODE_WATER\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"#endif\n"
"#ifdef MODE_REFRACTION\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"#endif\n"
"#ifdef USEREFLECTION\n"
"	ModelViewProjectionPosition = gl_Position;\n"
"#endif\n"
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
"// 13 textures, we can only use up to 16 on DX9-class hardware\n"
"uniform sampler2D Texture_Normal;\n"
"uniform sampler2D Texture_Color;\n"
"uniform sampler2D Texture_Gloss;\n"
"uniform samplerCube Texture_Cube;\n"
"uniform sampler2D Texture_Attenuation;\n"
"uniform sampler2D Texture_FogMask;\n"
"uniform sampler2D Texture_Pants;\n"
"uniform sampler2D Texture_Shirt;\n"
"uniform sampler2D Texture_Lightmap;\n"
"uniform sampler2D Texture_Deluxemap;\n"
"uniform sampler2D Texture_Glow;\n"
"uniform sampler2D Texture_Reflection;\n"
"uniform sampler2D Texture_Refraction;\n"
"\n"
"uniform myhvec3 LightColor;\n"
"uniform myhvec3 AmbientColor;\n"
"uniform myhvec3 DiffuseColor;\n"
"uniform myhvec3 SpecularColor;\n"
"uniform myhvec3 Color_Pants;\n"
"uniform myhvec3 Color_Shirt;\n"
"uniform myhvec3 FogColor;\n"
"\n"
"uniform myhvec4 TintColor;\n"
"\n"
"\n"
"//#ifdef MODE_WATER\n"
"uniform vec4 DistortScaleRefractReflect;\n"
"uniform vec4 ScreenScaleRefractReflect;\n"
"uniform vec4 ScreenCenterRefractReflect;\n"
"uniform myhvec4 RefractColor;\n"
"uniform myhvec4 ReflectColor;\n"
"uniform myhalf ReflectFactor;\n"
"uniform myhalf ReflectOffset;\n"
"//#else\n"
"//# ifdef MODE_REFRACTION\n"
"//uniform vec4 DistortScaleRefractReflect;\n"
"//uniform vec4 ScreenScaleRefractReflect;\n"
"//uniform vec4 ScreenCenterRefractReflect;\n"
"//uniform myhvec4 RefractColor;\n"
"//#  ifdef USEREFLECTION\n"
"//uniform myhvec4 ReflectColor;\n"
"//#  endif\n"
"//# else\n"
"//#  ifdef USEREFLECTION\n"
"//uniform vec4 DistortScaleRefractReflect;\n"
"//uniform vec4 ScreenScaleRefractReflect;\n"
"//uniform vec4 ScreenCenterRefractReflect;\n"
"//uniform myhvec4 ReflectColor;\n"
"//#  endif\n"
"//# endif\n"
"//#endif\n"
"\n"
"uniform myhalf GlowScale;\n"
"uniform myhalf SceneBrightness;\n"
"#ifdef USECONTRASTBOOST\n"
"uniform myhalf ContrastBoostCoeff;\n"
"#endif\n"
"\n"
"uniform float OffsetMapping_Scale;\n"
"uniform float OffsetMapping_Bias;\n"
"uniform float FogRangeRecip;\n"
"\n"
"uniform myhalf AmbientScale;\n"
"uniform myhalf DiffuseScale;\n"
"uniform myhalf SpecularScale;\n"
"uniform myhalf SpecularPower;\n"
"\n"
"#ifdef USEOFFSETMAPPING\n"
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
"#ifdef MODE_WATER\n"
"\n"
"// water pass\n"
"void main(void)\n"
"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	vec4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	vec4 ScreenTexCoord = ModelViewProjectionPosition.xyxy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect + vec2(normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5))).xyxy * DistortScaleRefractReflect;\n"
"	float Fresnel = pow(min(1.0, 1.0 - float(normalize(EyeVector).z)), 5.0) * ReflectFactor + ReflectOffset;\n"
"	gl_FragColor = mix(texture2D(Texture_Refraction, ScreenTexCoord.xy) * RefractColor, texture2D(Texture_Reflection, ScreenTexCoord.zw) * ReflectColor, Fresnel);\n"
"}\n"
"\n"
"#else // MODE_WATER\n"
"#ifdef MODE_REFRACTION\n"
"\n"
"// refraction pass\n"
"void main(void)\n"
"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	vec2 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect.xy * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec2 ScreenTexCoord = (ModelViewProjectionPosition.xy + normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5)).xy * DistortScaleRefractReflect.xy * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy;\n"
"	vec2 ScreenTexCoord = ModelViewProjectionPosition.xy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect.xy + vec2(normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5))).xy * DistortScaleRefractReflect.xy;\n"
"	gl_FragColor = texture2D(Texture_Refraction, ScreenTexCoord) * RefractColor;\n"
"}\n"
"\n"
"#else // MODE_REFRACTION\n"
"void main(void)\n"
"{\n"
"#ifdef USEOFFSETMAPPING\n"
"	// apply offsetmapping\n"
"	vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"\n"
"	// combine the diffuse textures (base, pants, shirt)\n"
"	myhvec4 color = myhvec4(texture2D(Texture_Color, TexCoord));\n"
"#ifdef USECOLORMAPPING\n"
"	color.rgb += myhvec3(texture2D(Texture_Pants, TexCoord)) * Color_Pants + myhvec3(texture2D(Texture_Shirt, TexCoord)) * Color_Shirt;\n"
"#endif\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTSOURCE\n"
"	// light source\n"
"\n"
"	// calculate surface normal, light normal, and specular normal\n"
"	// compute color intensity for the two textures (colormap and glossmap)\n"
"	// scale by light color and attenuation as efficiently as possible\n"
"	// (do as much scalar math as possible rather than vector math)\n"
"# ifdef USESPECULAR\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"	myhvec3 diffusenormal = myhvec3(normalize(LightVector));\n"
"	myhvec3 specularnormal = normalize(diffusenormal + myhvec3(normalize(EyeVector)));\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb = LightColor * myhalf(texture2D(Texture_Attenuation, vec2(length(CubeVector), 0.0))) * (color.rgb * (AmbientScale + DiffuseScale * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0))) + (SpecularScale * pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower)) * myhvec3(texture2D(Texture_Gloss, TexCoord)));\n"
"# else\n"
"#  ifdef USEDIFFUSE\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"	myhvec3 diffusenormal = myhvec3(normalize(LightVector));\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb = color.rgb * LightColor * (myhalf(texture2D(Texture_Attenuation, vec2(length(CubeVector), 0.0))) * (AmbientScale + DiffuseScale * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0))));\n"
"#  else\n"
"	// calculate directionless shading\n"
"	color.rgb = color.rgb * LightColor * myhalf(texture2D(Texture_Attenuation, vec2(length(CubeVector), 0.0)));\n"
"#  endif\n"
"# endif\n"
"\n"
"# ifdef USECUBEFILTER\n"
"	// apply light cubemap filter\n"
"	//color.rgb *= normalize(CubeVector) * 0.5 + 0.5;//vec3(textureCube(Texture_Cube, CubeVector));\n"
"	color.rgb *= myhvec3(textureCube(Texture_Cube, CubeVector));\n"
"# endif\n"
"#endif // MODE_LIGHTSOURCE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTDIRECTION\n"
"	// directional model lighting\n"
"# ifdef USESPECULAR\n"
"	// get the surface normal and light normal\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"	myhvec3 diffusenormal = myhvec3(LightVector);\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb *= AmbientColor + DiffuseColor * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0));\n"
"	myhvec3 specularnormal = normalize(diffusenormal + myhvec3(normalize(EyeVector)));\n"
"	color.rgb += myhvec3(texture2D(Texture_Gloss, TexCoord)) * SpecularColor * pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower);\n"
"# else\n"
"#  ifdef USEDIFFUSE\n"
"	// get the surface normal and light normal\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"	myhvec3 diffusenormal = myhvec3(LightVector);\n"
"\n"
"	// calculate directional shading\n"
"	color.rgb *= AmbientColor + DiffuseColor * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0));\n"
"#  else\n"
"	color.rgb *= AmbientColor;\n"
"#  endif\n"
"# endif\n"
"\n"
"	color.a *= TintColor.a;\n"
"#endif // MODE_LIGHTDIRECTION\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"	// deluxemap lightmapping using light vectors in modelspace (evil q3map2)\n"
"\n"
"	// get the surface normal and light normal\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"\n"
"	myhvec3 diffusenormal_modelspace = myhvec3(texture2D(Texture_Deluxemap, TexCoordLightmap)) - myhvec3(0.5);\n"
"	myhvec3 diffusenormal = normalize(myhvec3(dot(diffusenormal_modelspace, myhvec3(VectorS)), dot(diffusenormal_modelspace, myhvec3(VectorT)), dot(diffusenormal_modelspace, myhvec3(VectorR))));\n"
"	// calculate directional shading\n"
"	myhvec3 tempcolor = color.rgb * (DiffuseScale * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0)));\n"
"# ifdef USESPECULAR\n"
"	myhvec3 specularnormal = myhvec3(normalize(diffusenormal + myhvec3(normalize(EyeVector))));\n"
"	tempcolor += myhvec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower);\n"
"# endif\n"
"\n"
"	// apply lightmap color\n"
"	color.rgb = color.rgb * AmbientScale + tempcolor * myhvec3(texture2D(Texture_Lightmap, TexCoordLightmap));\n"
"\n"
"	color *= TintColor;\n"
"#endif // MODE_LIGHTDIRECTIONMAP_MODELSPACE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n"
"	// deluxemap lightmapping using light vectors in tangentspace (hmap2 -light)\n"
"\n"
"	// get the surface normal and light normal\n"
"	myhvec3 surfacenormal = normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5));\n"
"\n"
"	myhvec3 diffusenormal = normalize(myhvec3(texture2D(Texture_Deluxemap, TexCoordLightmap)) - myhvec3(0.5));\n"
"	// calculate directional shading\n"
"	myhvec3 tempcolor = color.rgb * (DiffuseScale * myhalf(max(float(dot(surfacenormal, diffusenormal)), 0.0)));\n"
"# ifdef USESPECULAR\n"
"	myhvec3 specularnormal = myhvec3(normalize(diffusenormal + myhvec3(normalize(EyeVector))));\n"
"	tempcolor += myhvec3(texture2D(Texture_Gloss, TexCoord)) * SpecularScale * pow(myhalf(max(float(dot(surfacenormal, specularnormal)), 0.0)), SpecularPower);\n"
"# endif\n"
"\n"
"	// apply lightmap color\n"
"	color.rgb = color.rgb * AmbientScale + tempcolor * myhvec3(texture2D(Texture_Lightmap, TexCoordLightmap));\n"
"\n"
"	color *= TintColor;\n"
"#endif // MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_LIGHTMAP\n"
"	// apply lightmap color\n"
"	color.rgb = color.rgb * myhvec3(texture2D(Texture_Lightmap, TexCoordLightmap)) * DiffuseScale + color.rgb * AmbientScale;\n"
"\n"
"	color *= TintColor;\n"
"#endif // MODE_LIGHTMAP\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_VERTEXCOLOR\n"
"	// apply lightmap color\n"
"	color.rgb = color.rgb * myhvec3(gl_Color.rgb) * DiffuseScale + color.rgb * AmbientScale;\n"
"\n"
"	color *= TintColor;\n"
"#endif // MODE_VERTEXCOLOR\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef MODE_FLATCOLOR\n"
"	color *= TintColor;\n"
"#endif // MODE_FLATCOLOR\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"#ifdef USEGLOW\n"
"	color.rgb += myhvec3(texture2D(Texture_Glow, TexCoord)) * GlowScale;\n"
"#endif\n"
"\n"
"#ifdef USECONTRASTBOOST\n"
"	color.rgb = color.rgb / (ContrastBoostCoeff * color.rgb + myhvec3(1, 1, 1));\n"
"#endif\n"
"\n"
"	color.rgb *= SceneBrightness;\n"
"\n"
"	// apply fog after Contrastboost/SceneBrightness because its color is already modified appropriately\n"
"#ifdef USEFOG\n"
"	color.rgb = mix(FogColor, color.rgb, myhalf(texture2D(Texture_FogMask, myhvec2(length(EyeVectorModelSpace)*FogRangeRecip, 0.0))));\n"
"#endif\n"
"\n"
"	// reflection must come last because it already contains exactly the correct fog (the reflection render preserves camera distance from the plane, it only flips the side) and ContrastBoost/SceneBrightness\n"
"#ifdef USEREFLECTION\n"
"	vec4 ScreenScaleRefractReflectIW = ScreenScaleRefractReflect * (1.0 / ModelViewProjectionPosition.w);\n"
"	//vec4 ScreenTexCoord = (ModelViewProjectionPosition.xyxy + normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5)).xyxy * DistortScaleRefractReflect * 100) * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect;\n"
"	vec4 ScreenTexCoord = ModelViewProjectionPosition.xyxy * ScreenScaleRefractReflectIW + ScreenCenterRefractReflect + vec3(normalize(myhvec3(texture2D(Texture_Normal, TexCoord)) - myhvec3(0.5))).xyxy * DistortScaleRefractReflect;\n"
"	color.rgb = mix(color.rgb, myhvec3(texture2D(Texture_Reflection, ScreenTexCoord.zw)) * ReflectColor.rgb, ReflectColor.a);\n"
"#endif\n"
"\n"
"	gl_FragColor = vec4(color);\n"
"}\n"
"#endif // MODE_REFRACTION\n"
"#endif // MODE_WATER\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
;

#define SHADERPERMUTATION_COLORMAPPING (1<<0) // indicates this is a colormapped skin
#define SHADERPERMUTATION_CONTRASTBOOST (1<<1) // r_glsl_contrastboost boosts the contrast at low color levels (similar to gamma)
#define SHADERPERMUTATION_FOG (1<<2) // tint the color by fog color or black if using additive blend mode
#define SHADERPERMUTATION_CUBEFILTER (1<<3) // (lightsource) use cubemap light filter
#define SHADERPERMUTATION_GLOW (1<<4) // (lightmap) blend in an additive glow texture
#define SHADERPERMUTATION_DIFFUSE (1<<5) // (lightsource) whether to use directional shading
#define SHADERPERMUTATION_SPECULAR (1<<6) // (lightsource or deluxemapping) render specular effects
#define SHADERPERMUTATION_REFLECTION (1<<7) // normalmap-perturbed reflection of the scene infront of the surface, preformed as an overlay on the surface
#define SHADERPERMUTATION_OFFSETMAPPING (1<<8) // adjust texcoords to roughly simulate a displacement mapped surface
#define SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING (1<<9) // adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)
#define SHADERPERMUTATION_MODEBASE (1<<10) // multiplier for the SHADERMODE_ values to get a valid index

// NOTE: MUST MATCH ORDER OF SHADERPERMUTATION_* DEFINES!
const char *shaderpermutationinfo[][2] =
{
	{"#define USECOLORMAPPING\n", " colormapping"},
	{"#define USECONTRASTBOOST\n", " contrastboost"},
	{"#define USEFOG\n", " fog"},
	{"#define USECUBEFILTER\n", " cubefilter"},
	{"#define USEGLOW\n", " glow"},
	{"#define USEDIFFUSE\n", " diffuse"},
	{"#define USESPECULAR\n", " specular"},
	{"#define USEREFLECTION\n", " reflection"},
	{"#define USEOFFSETMAPPING\n", " offsetmapping"},
	{"#define USEOFFSETMAPPING_RELIEFMAPPING\n", " reliefmapping"},
	{NULL, NULL}
};

// this enum is multiplied by SHADERPERMUTATION_MODEBASE
typedef enum shadermode_e
{
	SHADERMODE_FLATCOLOR, // (lightmap) modulate texture by uniform color (q1bsp, q3bsp)
	SHADERMODE_VERTEXCOLOR, // (lightmap) modulate texture by vertex colors (q3bsp)
	SHADERMODE_LIGHTMAP, // (lightmap) modulate texture by lightmap texture (q1bsp, q3bsp)
	SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE, // (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
	SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE, // (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
	SHADERMODE_LIGHTDIRECTION, // (lightmap) use directional pixel shading from fixed light direction (q3bsp)
	SHADERMODE_LIGHTSOURCE, // (lightsource) use directional pixel shading from light source (rtlight)
	SHADERMODE_REFRACTION, // refract background (the material is rendered normally after this pass)
	SHADERMODE_WATER, // refract background and reflection (the material is rendered normally after this pass)
	SHADERMODE_COUNT
}
shadermode_t;

// NOTE: MUST MATCH ORDER OF SHADERMODE_* ENUMS!
const char *shadermodeinfo[][2] =
{
	{"#define MODE_FLATCOLOR\n", " flatcolor"},
	{"#define MODE_VERTEXCOLOR\n", " vertexcolor"},
	{"#define MODE_LIGHTMAP\n", " lightmap"},
	{"#define MODE_LIGHTDIRECTIONMAP_MODELSPACE\n", " lightdirectionmap_modelspace"},
	{"#define MODE_LIGHTDIRECTIONMAP_TANGENTSPACE\n", " lightdirectionmap_tangentspace"},
	{"#define MODE_LIGHTDIRECTION\n", " lightdirection"},
	{"#define MODE_LIGHTSOURCE\n", " lightsource"},
	{"#define MODE_REFRACTION\n", " refraction"},
	{"#define MODE_WATER\n", " water"},
	{NULL, NULL}
};

#define SHADERPERMUTATION_INDICES (SHADERPERMUTATION_MODEBASE * SHADERMODE_COUNT)

typedef struct r_glsl_permutation_s
{
	// indicates if we have tried compiling this permutation already
	qboolean compiled;
	// 0 if compilation failed
	int program;
	// locations of detected uniforms in program object, or -1 if not found
	int loc_Texture_Normal;
	int loc_Texture_Color;
	int loc_Texture_Gloss;
	int loc_Texture_Cube;
	int loc_Texture_Attenuation;
	int loc_Texture_FogMask;
	int loc_Texture_Pants;
	int loc_Texture_Shirt;
	int loc_Texture_Lightmap;
	int loc_Texture_Deluxemap;
	int loc_Texture_Glow;
	int loc_Texture_Refraction;
	int loc_Texture_Reflection;
	int loc_FogColor;
	int loc_LightPosition;
	int loc_EyePosition;
	int loc_LightColor;
	int loc_Color_Pants;
	int loc_Color_Shirt;
	int loc_FogRangeRecip;
	int loc_AmbientScale;
	int loc_DiffuseScale;
	int loc_SpecularScale;
	int loc_SpecularPower;
	int loc_GlowScale;
	int loc_SceneBrightness; // or: Scenebrightness * ContrastBoost
	int loc_OffsetMapping_Scale;
	int loc_TintColor;
	int loc_AmbientColor;
	int loc_DiffuseColor;
	int loc_SpecularColor;
	int loc_LightDir;
	int loc_ContrastBoostCoeff; // 1 - 1/ContrastBoost
	int loc_DistortScaleRefractReflect;
	int loc_ScreenScaleRefractReflect;
	int loc_ScreenCenterRefractReflect;
	int loc_RefractColor;
	int loc_ReflectColor;
	int loc_ReflectFactor;
	int loc_ReflectOffset;
}
r_glsl_permutation_t;

// information about each possible shader permutation
r_glsl_permutation_t r_glsl_permutations[SHADERPERMUTATION_INDICES];
// currently selected permutation
r_glsl_permutation_t *r_glsl_permutation;

// these are additional flags used only by R_GLSL_CompilePermutation
#define SHADERTYPE_USES_VERTEXSHADER (1<<0)
#define SHADERTYPE_USES_GEOMETRYSHADER (1<<1)
#define SHADERTYPE_USES_FRAGMENTSHADER (1<<2)

static void R_GLSL_CompilePermutation(const char *filename, int permutation, int shadertype)
{
	int i;
	qboolean shaderfound;
	r_glsl_permutation_t *p = r_glsl_permutations + permutation;
	int vertstrings_count;
	int geomstrings_count;
	int fragstrings_count;
	char *shaderstring;
	const char *vertstrings_list[32+1];
	const char *geomstrings_list[32+1];
	const char *fragstrings_list[32+1];
	char permutationname[256];
	if (p->compiled)
		return;
	p->compiled = true;
	p->program = 0;
	vertstrings_list[0] = "#define VERTEX_SHADER\n";
	geomstrings_list[0] = "#define GEOMETRY_SHADER\n";
	fragstrings_list[0] = "#define FRAGMENT_SHADER\n";
	vertstrings_count = 1;
	geomstrings_count = 1;
	fragstrings_count = 1;
	permutationname[0] = 0;
	i = permutation / SHADERPERMUTATION_MODEBASE;
	vertstrings_list[vertstrings_count++] = shadermodeinfo[i][0];
	geomstrings_list[geomstrings_count++] = shadermodeinfo[i][0];
	fragstrings_list[fragstrings_count++] = shadermodeinfo[i][0];
	strlcat(permutationname, shadermodeinfo[i][1], sizeof(permutationname));
	for (i = 0;shaderpermutationinfo[i][0];i++)
	{
		if (permutation & (1<<i))
		{
			vertstrings_list[vertstrings_count++] = shaderpermutationinfo[i][0];
			geomstrings_list[geomstrings_count++] = shaderpermutationinfo[i][0];
			fragstrings_list[fragstrings_count++] = shaderpermutationinfo[i][0];
			strlcat(permutationname, shaderpermutationinfo[i][1], sizeof(permutationname));
		}
		else
		{
			// keep line numbers correct
			vertstrings_list[vertstrings_count++] = "\n";
			geomstrings_list[geomstrings_count++] = "\n";
			fragstrings_list[fragstrings_count++] = "\n";
		}
	}
	shaderstring = (char *)FS_LoadFile(filename, r_main_mempool, false, NULL);
	shaderfound = false;
	if (shaderstring)
	{
		Con_DPrint("from disk... ");
		vertstrings_list[vertstrings_count++] = shaderstring;
		geomstrings_list[geomstrings_count++] = shaderstring;
		fragstrings_list[fragstrings_count++] = shaderstring;
		shaderfound = true;
	}
	else if (!strcmp(filename, "glsl/default.glsl"))
	{
		vertstrings_list[vertstrings_count++] = builtinshaderstring;
		geomstrings_list[geomstrings_count++] = builtinshaderstring;
		fragstrings_list[fragstrings_count++] = builtinshaderstring;
		shaderfound = true;
	}
	// clear any lists that are not needed by this shader
	if (!(shadertype & SHADERTYPE_USES_VERTEXSHADER))
		vertstrings_count = 0;
	if (!(shadertype & SHADERTYPE_USES_GEOMETRYSHADER))
		geomstrings_count = 0;
	if (!(shadertype & SHADERTYPE_USES_FRAGMENTSHADER))
		fragstrings_count = 0;
	// compile the shader program
	if (shaderfound && vertstrings_count + geomstrings_count + fragstrings_count)
		p->program = GL_Backend_CompileProgram(vertstrings_count, vertstrings_list, geomstrings_count, geomstrings_list, fragstrings_count, fragstrings_list);
	if (p->program)
	{
		CHECKGLERROR
		qglUseProgramObjectARB(p->program);CHECKGLERROR
		// look up all the uniform variable names we care about, so we don't
		// have to look them up every time we set them
		p->loc_Texture_Normal      = qglGetUniformLocationARB(p->program, "Texture_Normal");
		p->loc_Texture_Color       = qglGetUniformLocationARB(p->program, "Texture_Color");
		p->loc_Texture_Gloss       = qglGetUniformLocationARB(p->program, "Texture_Gloss");
		p->loc_Texture_Cube        = qglGetUniformLocationARB(p->program, "Texture_Cube");
		p->loc_Texture_Attenuation = qglGetUniformLocationARB(p->program, "Texture_Attenuation");
		p->loc_Texture_FogMask     = qglGetUniformLocationARB(p->program, "Texture_FogMask");
		p->loc_Texture_Pants       = qglGetUniformLocationARB(p->program, "Texture_Pants");
		p->loc_Texture_Shirt       = qglGetUniformLocationARB(p->program, "Texture_Shirt");
		p->loc_Texture_Lightmap    = qglGetUniformLocationARB(p->program, "Texture_Lightmap");
		p->loc_Texture_Deluxemap   = qglGetUniformLocationARB(p->program, "Texture_Deluxemap");
		p->loc_Texture_Glow        = qglGetUniformLocationARB(p->program, "Texture_Glow");
		p->loc_Texture_Refraction  = qglGetUniformLocationARB(p->program, "Texture_Refraction");
		p->loc_Texture_Reflection  = qglGetUniformLocationARB(p->program, "Texture_Reflection");
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
		p->loc_GlowScale           = qglGetUniformLocationARB(p->program, "GlowScale");
		p->loc_SceneBrightness     = qglGetUniformLocationARB(p->program, "SceneBrightness");
		p->loc_OffsetMapping_Scale = qglGetUniformLocationARB(p->program, "OffsetMapping_Scale");
		p->loc_TintColor       = qglGetUniformLocationARB(p->program, "TintColor");
		p->loc_AmbientColor        = qglGetUniformLocationARB(p->program, "AmbientColor");
		p->loc_DiffuseColor        = qglGetUniformLocationARB(p->program, "DiffuseColor");
		p->loc_SpecularColor       = qglGetUniformLocationARB(p->program, "SpecularColor");
		p->loc_LightDir            = qglGetUniformLocationARB(p->program, "LightDir");
		p->loc_ContrastBoostCoeff  = qglGetUniformLocationARB(p->program, "ContrastBoostCoeff");
		p->loc_DistortScaleRefractReflect = qglGetUniformLocationARB(p->program, "DistortScaleRefractReflect");
		p->loc_ScreenScaleRefractReflect = qglGetUniformLocationARB(p->program, "ScreenScaleRefractReflect");
		p->loc_ScreenCenterRefractReflect = qglGetUniformLocationARB(p->program, "ScreenCenterRefractReflect");
		p->loc_RefractColor        = qglGetUniformLocationARB(p->program, "RefractColor");
		p->loc_ReflectColor        = qglGetUniformLocationARB(p->program, "ReflectColor");
		p->loc_ReflectFactor       = qglGetUniformLocationARB(p->program, "ReflectFactor");
		p->loc_ReflectOffset       = qglGetUniformLocationARB(p->program, "ReflectOffset");
		// initialize the samplers to refer to the texture units we use
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
		if (p->loc_Texture_Attenuation >= 0) qglUniform1iARB(p->loc_Texture_Attenuation, 10);
		if (p->loc_Texture_Refraction >= 0) qglUniform1iARB(p->loc_Texture_Refraction, 11);
		if (p->loc_Texture_Reflection >= 0) qglUniform1iARB(p->loc_Texture_Reflection, 12);
		CHECKGLERROR
		qglUseProgramObjectARB(0);CHECKGLERROR
		if (developer.integer)
			Con_Printf("GLSL shader %s :%s compiled.\n", filename, permutationname);
	}
	else
	{
		if (developer.integer)
			Con_Printf("GLSL shader %s :%s failed!  source code line offset for above errors is %i.\n", permutationname, filename, -(vertstrings_count - 1));
		else
			Con_Printf("GLSL shader %s :%s failed!  some features may not work properly.\n", permutationname, filename);
	}
	if (shaderstring)
		Mem_Free(shaderstring);
}

void R_GLSL_Restart_f(void)
{
	int i;
	for (i = 0;i < SHADERPERMUTATION_INDICES;i++)
		if (r_glsl_permutations[i].program)
			GL_Backend_FreeProgram(r_glsl_permutations[i].program);
	memset(r_glsl_permutations, 0, sizeof(r_glsl_permutations));
}

void R_GLSL_DumpShader_f(void)
{
	int i;

	qfile_t *file = FS_Open("glsl/default.glsl", "w", false, false);
	if(!file)
	{
		Con_Printf("failed to write to glsl/default.glsl\n");
		return;
	}

	FS_Print(file, "// The engine may define the following macros:\n");
	FS_Print(file, "// #define VERTEX_SHADER\n// #define GEOMETRY_SHADER\n// #define FRAGMENT_SHADER\n");
	for (i = 0;shadermodeinfo[i][0];i++)
		FS_Printf(file, "// %s", shadermodeinfo[i][0]);
	for (i = 0;shaderpermutationinfo[i][0];i++)
		FS_Printf(file, "// %s", shaderpermutationinfo[i][0]);
	FS_Print(file, "\n");
	FS_Print(file, builtinshaderstring);
	FS_Close(file);

	Con_Printf("glsl/default.glsl written\n");
}

extern rtexture_t *r_shadow_attenuationgradienttexture;
extern rtexture_t *r_shadow_attenuation2dtexture;
extern rtexture_t *r_shadow_attenuation3dtexture;
int R_SetupSurfaceShader(const vec3_t lightcolorbase, qboolean modellighting, float ambientscale, float diffusescale, float specularscale, rsurfacepass_t rsurfacepass)
{
	// select a permutation of the lighting shader appropriate to this
	// combination of texture, entity, light source, and fogging, only use the
	// minimum features necessary to avoid wasting rendering time in the
	// fragment shader on features that are not being used
	const char *shaderfilename = NULL;
	unsigned int permutation = 0;
	unsigned int shadertype = 0;
	shadermode_t mode = 0;
	r_glsl_permutation = NULL;
	shaderfilename = "glsl/default.glsl";
	shadertype = SHADERTYPE_USES_VERTEXSHADER | SHADERTYPE_USES_FRAGMENTSHADER;
	// TODO: implement geometry-shader based shadow volumes someday
	if (r_glsl_offsetmapping.integer)
	{
		permutation |= SHADERPERMUTATION_OFFSETMAPPING;
		if (r_glsl_offsetmapping_reliefmapping.integer)
			permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
	}
	if (rsurfacepass == RSURFPASS_BACKGROUND)
	{
		// distorted background
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_WATERSHADER)
			mode = SHADERMODE_WATER;
		else
			mode = SHADERMODE_REFRACTION;
	}
	else if (rsurfacepass == RSURFPASS_RTLIGHT)
	{
		// light source
		mode = SHADERMODE_LIGHTSOURCE;
		if (rsurface.rtlight->currentcubemap != r_texture_whitecube)
			permutation |= SHADERPERMUTATION_CUBEFILTER;
		if (diffusescale > 0)
			permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
			permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		if (r_refdef.fogenabled)
			permutation |= SHADERPERMUTATION_FOG;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if(r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)
			permutation |= SHADERPERMUTATION_CONTRASTBOOST;
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
	{
		// unshaded geometry (fullbright or ambient model lighting)
		mode = SHADERMODE_FLATCOLOR;
		if (rsurface.texture->currentskinframe->glow)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= SHADERPERMUTATION_FOG;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if (r_glsl_offsetmapping.integer)
		{
			permutation |= SHADERPERMUTATION_OFFSETMAPPING;
			if (r_glsl_offsetmapping_reliefmapping.integer)
				permutation |= SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING;
		}
		if(r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)
			permutation |= SHADERPERMUTATION_CONTRASTBOOST;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT_DIRECTIONAL)
	{
		// directional model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->currentskinframe->glow)
			permutation |= SHADERPERMUTATION_GLOW;
		permutation |= SHADERPERMUTATION_DIFFUSE;
		if (specularscale > 0)
			permutation |= SHADERPERMUTATION_SPECULAR;
		if (r_refdef.fogenabled)
			permutation |= SHADERPERMUTATION_FOG;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if(r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)
			permutation |= SHADERPERMUTATION_CONTRASTBOOST;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
	{
		// ambient model lighting
		mode = SHADERMODE_LIGHTDIRECTION;
		if (rsurface.texture->currentskinframe->glow)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= SHADERPERMUTATION_FOG;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if(r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)
			permutation |= SHADERPERMUTATION_CONTRASTBOOST;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
	}
	else
	{
		// lightmapped wall
		if (r_glsl_deluxemapping.integer >= 1 && rsurface.uselightmaptexture && r_refdef.worldmodel && r_refdef.worldmodel->brushq3.deluxemapping)
		{
			// deluxemapping (light direction texture)
			if (rsurface.uselightmaptexture && r_refdef.worldmodel && r_refdef.worldmodel->brushq3.deluxemapping && r_refdef.worldmodel->brushq3.deluxemapping_modelspace)
				mode = SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE;
			else
				mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
			if (specularscale > 0)
				permutation |= SHADERPERMUTATION_SPECULAR | SHADERPERMUTATION_DIFFUSE;
		}
		else if (r_glsl_deluxemapping.integer >= 2)
		{
			// fake deluxemapping (uniform light direction in tangentspace)
			mode = SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE;
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
		if (rsurface.texture->currentskinframe->glow)
			permutation |= SHADERPERMUTATION_GLOW;
		if (r_refdef.fogenabled)
			permutation |= SHADERPERMUTATION_FOG;
		if (rsurface.texture->colormapping)
			permutation |= SHADERPERMUTATION_COLORMAPPING;
		if(r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)
			permutation |= SHADERPERMUTATION_CONTRASTBOOST;
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_REFLECTION)
			permutation |= SHADERPERMUTATION_REFLECTION;
	}
	permutation |= mode * SHADERPERMUTATION_MODEBASE;
	if (!r_glsl_permutations[permutation].program)
	{
		if (!r_glsl_permutations[permutation].compiled)
			R_GLSL_CompilePermutation(shaderfilename, permutation, shadertype);
		if (!r_glsl_permutations[permutation].program)
		{
			// remove features until we find a valid permutation
			unsigned int i;
			for (i = (SHADERPERMUTATION_MODEBASE >> 1);;i>>=1)
			{
				if (!i)
				{
					Con_Printf("OpenGL 2.0 shaders disabled - unable to find a working shader permutation fallback on this driver (set r_glsl 1 if you want to try again)\n");
					Cvar_SetValueQuick(&r_glsl, 0);
					return 0; // no bit left to clear
				}
				// reduce i more quickly whenever it would not remove any bits
				if (!(permutation & i))
					continue;
				permutation &= ~i;
				if (!r_glsl_permutations[permutation].compiled)
					R_GLSL_CompilePermutation(shaderfilename, permutation, shadertype);
				if (r_glsl_permutations[permutation].program)
					break;
			}
		}
	}
	r_glsl_permutation = r_glsl_permutations + permutation;
	CHECKGLERROR
	qglUseProgramObjectARB(r_glsl_permutation->program);CHECKGLERROR
	if (mode == SHADERMODE_LIGHTSOURCE)
	{
		if (r_glsl_permutation->loc_LightPosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightPosition, rsurface.entitylightorigin[0], rsurface.entitylightorigin[1], rsurface.entitylightorigin[2]);
		if (permutation & SHADERPERMUTATION_DIFFUSE)
		{
			if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightColor, lightcolorbase[0], lightcolorbase[1], lightcolorbase[2]);
			if (r_glsl_permutation->loc_AmbientScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_AmbientScale, ambientscale);
			if (r_glsl_permutation->loc_DiffuseScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_DiffuseScale, diffusescale);
			if (r_glsl_permutation->loc_SpecularScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularScale, specularscale);
		}
		else
		{
			// ambient only is simpler
			if (r_glsl_permutation->loc_LightColor >= 0) qglUniform3fARB(r_glsl_permutation->loc_LightColor, lightcolorbase[0] * ambientscale, lightcolorbase[1] * ambientscale, lightcolorbase[2] * ambientscale);
			if (r_glsl_permutation->loc_AmbientScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_AmbientScale, 1);
			if (r_glsl_permutation->loc_DiffuseScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_DiffuseScale, 0);
			if (r_glsl_permutation->loc_SpecularScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularScale, 0);
		}
	}
	else if (mode == SHADERMODE_LIGHTDIRECTION)
	{
		if (r_glsl_permutation->loc_AmbientColor >= 0)
			qglUniform3fARB(r_glsl_permutation->loc_AmbientColor , rsurface.modellight_ambient[0] * ambientscale  * rsurface.texture->lightmapcolor[0] * 0.5f, rsurface.modellight_ambient[1] * ambientscale  * rsurface.texture->lightmapcolor[1] * 0.5f, rsurface.modellight_ambient[2] * ambientscale  * rsurface.texture->lightmapcolor[2] * 0.5f);
		if (r_glsl_permutation->loc_DiffuseColor >= 0)
			qglUniform3fARB(r_glsl_permutation->loc_DiffuseColor , rsurface.modellight_diffuse[0] * diffusescale  * rsurface.texture->lightmapcolor[0] * 0.5f, rsurface.modellight_diffuse[1] * diffusescale  * rsurface.texture->lightmapcolor[1] * 0.5f, rsurface.modellight_diffuse[2] * diffusescale  * rsurface.texture->lightmapcolor[2] * 0.5f);
		if (r_glsl_permutation->loc_SpecularColor >= 0)
			qglUniform3fARB(r_glsl_permutation->loc_SpecularColor, rsurface.modellight_diffuse[0] * specularscale * rsurface.texture->lightmapcolor[0] * 0.5f, rsurface.modellight_diffuse[1] * specularscale * rsurface.texture->lightmapcolor[1] * 0.5f, rsurface.modellight_diffuse[2] * specularscale * rsurface.texture->lightmapcolor[2] * 0.5f);
		if (r_glsl_permutation->loc_LightDir >= 0)
			qglUniform3fARB(r_glsl_permutation->loc_LightDir, rsurface.modellight_lightdir[0], rsurface.modellight_lightdir[1], rsurface.modellight_lightdir[2]);
	}
	else
	{
		if (r_glsl_permutation->loc_AmbientScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_AmbientScale, r_ambient.value * 1.0f / 128.0f);
		if (r_glsl_permutation->loc_DiffuseScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_DiffuseScale, r_refdef.lightmapintensity);
		if (r_glsl_permutation->loc_SpecularScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularScale, r_refdef.lightmapintensity * specularscale);
	}
	if (r_glsl_permutation->loc_TintColor >= 0) qglUniform4fARB(r_glsl_permutation->loc_TintColor, rsurface.texture->lightmapcolor[0], rsurface.texture->lightmapcolor[1], rsurface.texture->lightmapcolor[2], rsurface.texture->lightmapcolor[3]);
	if (r_glsl_permutation->loc_GlowScale >= 0) qglUniform1fARB(r_glsl_permutation->loc_GlowScale, r_hdr_glowintensity.value);
	if (r_glsl_permutation->loc_ContrastBoostCoeff >= 0)
	{
		// The formula used is actually:
		//   color.rgb *= ContrastBoost / ((ContrastBoost - 1) * color.rgb + 1);
		//   color.rgb *= SceneBrightness;
		// simplified:
		//   color.rgb = [[SceneBrightness * ContrastBoost]] * color.rgb / ([[ContrastBoost - 1]] * color.rgb + 1);
		// and do [[calculations]] here in the engine
		qglUniform1fARB(r_glsl_permutation->loc_ContrastBoostCoeff, r_glsl_contrastboost.value - 1);
		if (r_glsl_permutation->loc_SceneBrightness >= 0) qglUniform1fARB(r_glsl_permutation->loc_SceneBrightness, r_refdef.view.colorscale * r_glsl_contrastboost.value);
	}
	else
		if (r_glsl_permutation->loc_SceneBrightness >= 0) qglUniform1fARB(r_glsl_permutation->loc_SceneBrightness, r_refdef.view.colorscale);
	if (r_glsl_permutation->loc_FogColor >= 0)
	{
		// additive passes are only darkened by fog, not tinted
		if (rsurface.rtlight || (rsurface.texture->currentmaterialflags & MATERIALFLAG_ADD))
			qglUniform3fARB(r_glsl_permutation->loc_FogColor, 0, 0, 0);
		else
			qglUniform3fARB(r_glsl_permutation->loc_FogColor, r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2]);
	}
	if (r_glsl_permutation->loc_EyePosition >= 0) qglUniform3fARB(r_glsl_permutation->loc_EyePosition, rsurface.modelorg[0], rsurface.modelorg[1], rsurface.modelorg[2]);
	if (r_glsl_permutation->loc_Color_Pants >= 0)
	{
		if (rsurface.texture->currentskinframe->pants)
			qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, rsurface.colormap_pantscolor[0], rsurface.colormap_pantscolor[1], rsurface.colormap_pantscolor[2]);
		else
			qglUniform3fARB(r_glsl_permutation->loc_Color_Pants, 0, 0, 0);
	}
	if (r_glsl_permutation->loc_Color_Shirt >= 0)
	{
		if (rsurface.texture->currentskinframe->shirt)
			qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, rsurface.colormap_shirtcolor[0], rsurface.colormap_shirtcolor[1], rsurface.colormap_shirtcolor[2]);
		else
			qglUniform3fARB(r_glsl_permutation->loc_Color_Shirt, 0, 0, 0);
	}
	if (r_glsl_permutation->loc_FogRangeRecip >= 0) qglUniform1fARB(r_glsl_permutation->loc_FogRangeRecip, r_refdef.fograngerecip);
	if (r_glsl_permutation->loc_SpecularPower >= 0) qglUniform1fARB(r_glsl_permutation->loc_SpecularPower, rsurface.texture->specularpower);
	if (r_glsl_permutation->loc_OffsetMapping_Scale >= 0) qglUniform1fARB(r_glsl_permutation->loc_OffsetMapping_Scale, r_glsl_offsetmapping_scale.value);
	if (r_glsl_permutation->loc_DistortScaleRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_DistortScaleRefractReflect, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_refractdistort.value * rsurface.texture->refractfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor, r_water_reflectdistort.value * rsurface.texture->reflectfactor);
	if (r_glsl_permutation->loc_ScreenScaleRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_ScreenScaleRefractReflect, r_waterstate.screenscale[0], r_waterstate.screenscale[1], r_waterstate.screenscale[0], r_waterstate.screenscale[1]);
	if (r_glsl_permutation->loc_ScreenCenterRefractReflect >= 0) qglUniform4fARB(r_glsl_permutation->loc_ScreenCenterRefractReflect, r_waterstate.screencenter[0], r_waterstate.screencenter[1], r_waterstate.screencenter[0], r_waterstate.screencenter[1]);
	if (r_glsl_permutation->loc_RefractColor >= 0) qglUniform4fvARB(r_glsl_permutation->loc_RefractColor, 1, rsurface.texture->refractcolor4f);
	if (r_glsl_permutation->loc_ReflectColor >= 0) qglUniform4fvARB(r_glsl_permutation->loc_ReflectColor, 1, rsurface.texture->reflectcolor4f);
	if (r_glsl_permutation->loc_ReflectFactor >= 0) qglUniform1fARB(r_glsl_permutation->loc_ReflectFactor, rsurface.texture->reflectmax - rsurface.texture->reflectmin);
	if (r_glsl_permutation->loc_ReflectOffset >= 0) qglUniform1fARB(r_glsl_permutation->loc_ReflectOffset, rsurface.texture->reflectmin);
	CHECKGLERROR
	return permutation;
}

#define SKINFRAME_HASH 1024

struct
{
	int loadsequence; // incremented each level change
	memexpandablearray_t array;
	skinframe_t *hash[SKINFRAME_HASH];
}
r_skinframe;

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

skinframe_t *R_SkinFrame_LoadExternal(const char *name, int textureflags, qboolean complain)
{
	// FIXME: it should be possible to disable loading various layers using
	// cvars, to prevent wasted loading time and memory usage if the user does
	// not want them
	qboolean loadnormalmap = true;
	qboolean loadgloss = true;
	qboolean loadpantsandshirt = true;
	qboolean loadglow = true;
	int j;
	unsigned char *pixels;
	unsigned char *bumppixels;
	unsigned char *basepixels = NULL;
	int basepixels_width;
	int basepixels_height;
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	// return an existing skinframe if already loaded
	// if loading of the first image fails, don't make a new skinframe as it
	// would cause all future lookups of this to be missing
	skinframe = R_SkinFrame_Find(name, textureflags, 0, 0, 0, false);
	if (skinframe && skinframe->base)
		return skinframe;

	basepixels = loadimagepixelsbgra(name, complain, true);
	if (basepixels == NULL)
		return NULL;

	// we've got some pixels to store, so really allocate this new texture now
	if (!skinframe)
		skinframe = R_SkinFrame_Find(name, textureflags, 0, 0, 0, true);
	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = r_texture_notexture;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = r_texture_blanknormalmap;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;

	basepixels_width = image_width;
	basepixels_height = image_height;
	skinframe->base = R_LoadTexture2D (r_main_texturepool, skinframe->basename, basepixels_width, basepixels_height, basepixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);

	if (textureflags & TEXF_ALPHA)
	{
		for (j = 3;j < basepixels_width * basepixels_height * 4;j += 4)
			if (basepixels[j] < 255)
				break;
		if (j < basepixels_width * basepixels_height * 4)
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

	// _norm is the name used by tenebrae and has been adopted as standard
	if (loadnormalmap)
	{
		if ((pixels = loadimagepixelsbgra(va("%s_norm", skinframe->basename), false, false)) != NULL)
		{
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
			pixels = NULL;
		}
		else if (r_shadow_bumpscale_bumpmap.value > 0 && (bumppixels = loadimagepixelsbgra(va("%s_bump", skinframe->basename), false, false)) != NULL)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, image_width * image_height * 4);
			Image_HeightmapToNormalmap_BGRA(bumppixels, pixels, image_width, image_height, false, r_shadow_bumpscale_bumpmap.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
			Mem_Free(bumppixels);
		}
		else if (r_shadow_bumpscale_basetexture.value > 0)
		{
			pixels = (unsigned char *)Mem_Alloc(tempmempool, basepixels_width * basepixels_height * 4);
			Image_HeightmapToNormalmap_BGRA(basepixels, pixels, basepixels_width, basepixels_height, false, r_shadow_bumpscale_basetexture.value);
			skinframe->nmap = R_LoadTexture2D (r_main_texturepool, va("%s_nmap", skinframe->basename), basepixels_width, basepixels_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_normal.integer ? ~0 : ~TEXF_COMPRESS), NULL);
			Mem_Free(pixels);
		}
	}
	// _luma is supported for tenebrae compatibility
	// (I think it's a very stupid name, but oh well)
	// _glow is the preferred name
	if (loadglow          && ((pixels = loadimagepixelsbgra(va("%s_glow", skinframe->basename), false, false)) != NULL || (pixels = loadimagepixelsbgra(va("%s_luma", skinframe->basename), false, false)) != NULL)) {skinframe->glow = R_LoadTexture2D (r_main_texturepool, va("%s_glow", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_glow.integer ? ~0 : ~TEXF_COMPRESS), NULL);Mem_Free(pixels);pixels = NULL;}
	if (loadgloss         && (pixels = loadimagepixelsbgra(va("%s_gloss", skinframe->basename), false, false)) != NULL) {skinframe->gloss = R_LoadTexture2D (r_main_texturepool, va("%s_gloss", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_gloss.integer ? ~0 : ~TEXF_COMPRESS), NULL);Mem_Free(pixels);pixels = NULL;}
	if (loadpantsandshirt && (pixels = loadimagepixelsbgra(va("%s_pants", skinframe->basename), false, false)) != NULL) {skinframe->pants = R_LoadTexture2D (r_main_texturepool, va("%s_pants", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);Mem_Free(pixels);pixels = NULL;}
	if (loadpantsandshirt && (pixels = loadimagepixelsbgra(va("%s_shirt", skinframe->basename), false, false)) != NULL) {skinframe->shirt = R_LoadTexture2D (r_main_texturepool, va("%s_shirt", skinframe->basename), image_width, image_height, pixels, TEXTYPE_BGRA, skinframe->textureflags & (gl_texturecompression_color.integer ? ~0 : ~TEXF_COMPRESS), NULL);Mem_Free(pixels);pixels = NULL;}

	if (basepixels)
		Mem_Free(basepixels);

	return skinframe;
}

static rtexture_t *R_SkinFrame_TextureForSkinLayer(const unsigned char *in, int width, int height, const char *name, const unsigned int *palette, int textureflags, qboolean force)
{
	int i;
	if (!force)
	{
		for (i = 0;i < width*height;i++)
			if (((unsigned char *)&palette[in[i]])[3] > 0)
				break;
		if (i == width*height)
			return NULL;
	}
	return R_LoadTexture2D (r_main_texturepool, name, width, height, in, TEXTYPE_PALETTE, textureflags, palette);
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
	skinframe->base = r_texture_notexture;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = r_texture_blanknormalmap;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;

	// if no data was provided, then clearly the caller wanted to get a blank skinframe
	if (!skindata)
		return NULL;

	if (r_shadow_bumpscale_basetexture.value > 0)
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
			if (skindata[i] < 255)
				break;
		if (i < width * height * 4)
		{
			unsigned char *fogpixels = (unsigned char *)Mem_Alloc(tempmempool, width * height * 4);
			memcpy(fogpixels, skindata, width * height * 4);
			for (i = 0;i < width * height * 4;i += 4)
				fogpixels[i] = fogpixels[i+1] = fogpixels[i+2] = 255;
			skinframe->fog = R_LoadTexture2D(r_main_texturepool, va("%s_fog", skinframe->basename), width, height, fogpixels, TEXTYPE_BGRA, skinframe->textureflags, NULL);
			Mem_Free(fogpixels);
		}
	}

	return skinframe;
}

skinframe_t *R_SkinFrame_LoadInternalQuake(const char *name, int textureflags, int loadpantsandshirt, int loadglowtexture, const unsigned char *skindata, int width, int height)
{
	int i;
	unsigned char *temp1, *temp2;
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	// if already loaded just return it, otherwise make a new skinframe
	skinframe = R_SkinFrame_Find(name, textureflags, width, height, skindata ? CRC_Block(skindata, width*height) : 0, true);
	if (skinframe && skinframe->base)
		return skinframe;

	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = r_texture_notexture;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = r_texture_blanknormalmap;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;

	// if no data was provided, then clearly the caller wanted to get a blank skinframe
	if (!skindata)
		return NULL;

	if (r_shadow_bumpscale_basetexture.value > 0)
	{
		temp1 = (unsigned char *)Mem_Alloc(tempmempool, width * height * 8);
		temp2 = temp1 + width * height * 4;
		// use either a custom palette or the quake palette
		Image_Copy8bitBGRA(skindata, temp1, width * height, palette_bgra_complete);
		Image_HeightmapToNormalmap_BGRA(temp1, temp2, width, height, false, r_shadow_bumpscale_basetexture.value);
		skinframe->nmap = R_LoadTexture2D(r_main_texturepool, va("%s_nmap", skinframe->basename), width, height, temp2, TEXTYPE_BGRA, skinframe->textureflags | TEXF_ALPHA, NULL);
		Mem_Free(temp1);
	}
	// use either a custom palette, or the quake palette
	skinframe->base = skinframe->merged = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_merged", skinframe->basename), (loadglowtexture ? palette_bgra_nofullbrights : ((skinframe->textureflags & TEXF_ALPHA) ? palette_bgra_transparent : palette_bgra_complete)), skinframe->textureflags, true); // all
	if (loadglowtexture)
		skinframe->glow = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_glow", skinframe->basename), palette_bgra_onlyfullbrights, skinframe->textureflags, false); // glow
	if (loadpantsandshirt)
	{
		skinframe->pants = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_pants", skinframe->basename), palette_bgra_pantsaswhite, skinframe->textureflags, false); // pants
		skinframe->shirt = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_shirt", skinframe->basename), palette_bgra_shirtaswhite, skinframe->textureflags, false); // shirt
	}
	if (skinframe->pants || skinframe->shirt)
		skinframe->base = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_nospecial", skinframe->basename), loadglowtexture ? palette_bgra_nocolormapnofullbrights : palette_bgra_nocolormap, skinframe->textureflags, false); // no special colors
	if (textureflags & TEXF_ALPHA)
	{
		for (i = 0;i < width * height;i++)
			if (((unsigned char *)palette_bgra_alpha)[skindata[i]*4+3] < 255)
				break;
		if (i < width * height)
			skinframe->fog = R_SkinFrame_TextureForSkinLayer(skindata, width, height, va("%s_fog", skinframe->basename), palette_bgra_alpha, skinframe->textureflags, true); // fog mask
	}

	return skinframe;
}

skinframe_t *R_SkinFrame_LoadMissing(void)
{
	skinframe_t *skinframe;

	if (cls.state == ca_dedicated)
		return NULL;

	skinframe = R_SkinFrame_Find("missing", TEXF_PRECACHE, 0, 0, 0, true);
	skinframe->stain = NULL;
	skinframe->merged = NULL;
	skinframe->base = r_texture_notexture;
	skinframe->pants = NULL;
	skinframe->shirt = NULL;
	skinframe->nmap = r_texture_blanknormalmap;
	skinframe->gloss = NULL;
	skinframe->glow = NULL;
	skinframe->fog = NULL;

	return skinframe;
}

void gl_main_start(void)
{
	memset(r_qwskincache, 0, sizeof(r_qwskincache));
	memset(r_qwskincache_skinframe, 0, sizeof(r_qwskincache_skinframe));

	// set up r_skinframe loading system for textures
	memset(&r_skinframe, 0, sizeof(r_skinframe));
	r_skinframe.loadsequence = 1;
	Mem_ExpandableArray_NewArray(&r_skinframe.array, r_main_mempool, sizeof(skinframe_t), 256);

	r_main_texturepool = R_AllocTexturePool();
	R_BuildBlankTextures();
	R_BuildNoTexture();
	if (gl_texturecubemap)
	{
		R_BuildWhiteCube();
		R_BuildNormalizationCube();
	}
	r_texture_fogattenuation = NULL;
	//r_texture_fogintensity = NULL;
	memset(&r_bloomstate, 0, sizeof(r_bloomstate));
	memset(&r_waterstate, 0, sizeof(r_waterstate));
	memset(r_glsl_permutations, 0, sizeof(r_glsl_permutations));
	memset(&r_svbsp, 0, sizeof (r_svbsp));

	r_refdef.fogmasktable_density = 0;
}

void gl_main_shutdown(void)
{
	memset(r_qwskincache, 0, sizeof(r_qwskincache));
	memset(r_qwskincache_skinframe, 0, sizeof(r_qwskincache_skinframe));

	// clear out the r_skinframe state
	Mem_ExpandableArray_FreeArray(&r_skinframe.array);
	memset(&r_skinframe, 0, sizeof(r_skinframe));

	if (r_svbsp.nodes)
		Mem_Free(r_svbsp.nodes);
	memset(&r_svbsp, 0, sizeof (r_svbsp));
	R_FreeTexturePool(&r_main_texturepool);
	r_texture_blanknormalmap = NULL;
	r_texture_white = NULL;
	r_texture_grey128 = NULL;
	r_texture_black = NULL;
	r_texture_whitecube = NULL;
	r_texture_normalizationcube = NULL;
	r_texture_fogattenuation = NULL;
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
	Cvar_RegisterVariable(&r_depthfirst);
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
	Cvar_RegisterVariable(&r_cullentities_trace_enlarge);
	Cvar_RegisterVariable(&r_cullentities_trace_delay);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_shadows_throwdistance);
	Cvar_RegisterVariable(&r_q1bsp_skymasking);
	Cvar_RegisterVariable(&r_polygonoffset_submodel_factor);
	Cvar_RegisterVariable(&r_polygonoffset_submodel_offset);
	Cvar_RegisterVariable(&r_fog_exp2);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&r_glsl);
	Cvar_RegisterVariable(&r_glsl_offsetmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_reliefmapping);
	Cvar_RegisterVariable(&r_glsl_offsetmapping_scale);
	Cvar_RegisterVariable(&r_glsl_deluxemapping);
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
	Cvar_RegisterVariable(&r_glsl_contrastboost);
	Cvar_RegisterVariable(&r_hdr_glowintensity);
	Cvar_RegisterVariable(&r_hdr_range);
	Cvar_RegisterVariable(&r_smoothnormals_areaweighting);
	Cvar_RegisterVariable(&developer_texturelogging);
	Cvar_RegisterVariable(&gl_lightmaps);
	Cvar_RegisterVariable(&r_test);
	Cvar_RegisterVariable(&r_batchmode);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_TENEBRAE)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);

	Cvar_RegisterVariable(&r_track_sprites);
	Cvar_RegisterVariable(&r_track_sprites_flags);
	Cvar_RegisterVariable(&r_track_sprites_scalew);
	Cvar_RegisterVariable(&r_track_sprites_scaleh);
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

void Render_Init(void)
{
	gl_backend_init();
	R_Textures_Init();
	GL_Main_Init();
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

static void R_View_UpdateEntityVisible (void)
{
	int i, renderimask;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	renderimask = r_refdef.envmap ? (RENDER_EXTERIORMODEL | RENDER_VIEWMODEL) : ((chase_active.integer || r_waterstate.renderingscene) ? RENDER_VIEWMODEL : RENDER_EXTERIORMODEL);
	if (r_refdef.worldmodel && r_refdef.worldmodel->brush.BoxTouchingVisibleLeafs)
	{
		// worldmodel can check visibility
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			r_refdef.viewcache.entityvisible[i] = !(ent->flags & renderimask) && ((ent->model && ent->model->type == mod_sprite && (ent->model->sprite.sprnum_type == SPR_LABEL || ent->model->sprite.sprnum_type == SPR_LABEL_SCALE)) || !R_CullBox(ent->mins, ent->maxs)) && ((ent->effects & EF_NODEPTHTEST) || (ent->flags & RENDER_VIEWMODEL) || r_refdef.worldmodel->brush.BoxTouchingVisibleLeafs(r_refdef.worldmodel, r_refdef.viewcache.world_leafvisible, ent->mins, ent->maxs));

		}
		if(r_cullentities_trace.integer && r_refdef.worldmodel->brush.TraceLineOfSight)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if(r_refdef.viewcache.entityvisible[i] && !(ent->effects & EF_NODEPTHTEST) && !(ent->flags & RENDER_VIEWMODEL) && !(ent->model && (ent->model->name[0] == '*')))
				{
					if(Mod_CanSeeBox_Trace(r_cullentities_trace_samples.integer, r_cullentities_trace_enlarge.value, r_refdef.worldmodel, r_refdef.view.origin, ent->mins, ent->maxs))
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
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			r_refdef.viewcache.entityvisible[i] = !(ent->flags & renderimask) && ((ent->model && ent->model->type == mod_sprite && (ent->model->sprite.sprnum_type == SPR_LABEL || ent->model->sprite.sprnum_type == SPR_LABEL_SCALE)) || !R_CullBox(ent->mins, ent->maxs));
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
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.entities[i];
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

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.entities[i];
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

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.entities[i];
		if (ent->model && ent->model->DrawDepth != NULL)
			ent->model->DrawDepth(ent);
	}
}

static void R_DrawModelsDebug(void)
{
	int i;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.entities[i];
		if (ent->model && ent->model->DrawDebug != NULL)
			ent->model->DrawDebug(ent);
	}
}

static void R_DrawModelsAddWaterPlanes(void)
{
	int i;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		if (!r_refdef.viewcache.entityvisible[i])
			continue;
		ent = r_refdef.entities[i];
		if (ent->model && ent->model->DrawAddWaterPlanes != NULL)
			ent->model->DrawAddWaterPlanes(ent);
	}
}

static void R_View_SetFrustum(void)
{
	int i;
	double slopex, slopey;

	// break apart the view matrix into vectors for various purposes
	Matrix4x4_ToVectors(&r_refdef.view.matrix, r_refdef.view.forward, r_refdef.view.left, r_refdef.view.up, r_refdef.view.origin);
	VectorNegate(r_refdef.view.left, r_refdef.view.right);

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
		VectorMA(r_refdef.view.forward, -slopex, r_refdef.view.left, r_refdef.view.frustum[0].normal);
		VectorMA(r_refdef.view.forward,  slopex, r_refdef.view.left, r_refdef.view.frustum[1].normal);
		VectorMA(r_refdef.view.forward, -slopey, r_refdef.view.up  , r_refdef.view.frustum[2].normal);
		VectorMA(r_refdef.view.forward,  slopey, r_refdef.view.up  , r_refdef.view.frustum[3].normal);
		VectorCopy(r_refdef.view.forward, r_refdef.view.frustum[4].normal);

		// Leaving those out was a mistake, those were in the old code, and they
		// fix a reproducable bug in this one: frustum culling got fucked up when viewmatrix was an identity matrix
		// I couldn't reproduce it after adding those normalizations. --blub
		VectorNormalize(r_refdef.view.frustum[0].normal);
		VectorNormalize(r_refdef.view.frustum[1].normal);
		VectorNormalize(r_refdef.view.frustum[2].normal);
		VectorNormalize(r_refdef.view.frustum[3].normal);

		// calculate frustum corners, which are used to calculate deformed frustum planes for shadow caster culling
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, r_refdef.view.forward, -1024 * slopex, r_refdef.view.left, -1024 * slopey, r_refdef.view.up, r_refdef.view.frustumcorner[0]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, r_refdef.view.forward,  1024 * slopex, r_refdef.view.left, -1024 * slopey, r_refdef.view.up, r_refdef.view.frustumcorner[1]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, r_refdef.view.forward, -1024 * slopex, r_refdef.view.left,  1024 * slopey, r_refdef.view.up, r_refdef.view.frustumcorner[2]);
		VectorMAMAMAM(1, r_refdef.view.origin, 1024, r_refdef.view.forward,  1024 * slopex, r_refdef.view.left,  1024 * slopey, r_refdef.view.up, r_refdef.view.frustumcorner[3]);

		r_refdef.view.frustum[0].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[0].normal);
		r_refdef.view.frustum[1].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[1].normal);
		r_refdef.view.frustum[2].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[2].normal);
		r_refdef.view.frustum[3].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[3].normal);
		r_refdef.view.frustum[4].dist = DotProduct (r_refdef.view.origin, r_refdef.view.frustum[4].normal) + r_refdef.nearclip;
	}
	else
	{
		VectorScale(r_refdef.view.left, -r_refdef.view.ortho_x, r_refdef.view.frustum[0].normal);
		VectorScale(r_refdef.view.left,  r_refdef.view.ortho_x, r_refdef.view.frustum[1].normal);
		VectorScale(r_refdef.view.up, -r_refdef.view.ortho_y, r_refdef.view.frustum[2].normal);
		VectorScale(r_refdef.view.up,  r_refdef.view.ortho_y, r_refdef.view.frustum[3].normal);
		VectorCopy(r_refdef.view.forward, r_refdef.view.frustum[4].normal);
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
	//RotatePointAroundVector( r_refdef.view.frustum[0].normal, r_refdef.view.up, r_refdef.view.forward, -(90 - r_refdef.fov_x / 2));
	//r_refdef.view.frustum[0].dist = DotProduct (r_refdef.view.origin, frustum[0].normal);
	//PlaneClassify(&frustum[0]);

	// rotate R_VIEWFORWARD left by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[1].normal, r_refdef.view.up, r_refdef.view.forward, (90 - r_refdef.fov_x / 2));
	//r_refdef.view.frustum[1].dist = DotProduct (r_refdef.view.origin, frustum[1].normal);
	//PlaneClassify(&frustum[1]);

	// rotate R_VIEWFORWARD up by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[2].normal, r_refdef.view.left, r_refdef.view.forward, -(90 - r_refdef.fov_y / 2));
	//r_refdef.view.frustum[2].dist = DotProduct (r_refdef.view.origin, frustum[2].normal);
	//PlaneClassify(&frustum[2]);

	// rotate R_VIEWFORWARD down by FOV_X/2 degrees
	//RotatePointAroundVector( r_refdef.view.frustum[3].normal, r_refdef.view.left, r_refdef.view.forward, (90 - r_refdef.fov_y / 2));
	//r_refdef.view.frustum[3].dist = DotProduct (r_refdef.view.origin, frustum[3].normal);
	//PlaneClassify(&frustum[3]);

	// nearclip plane
	//VectorCopy(r_refdef.view.forward, r_refdef.view.frustum[4].normal);
	//r_refdef.view.frustum[4].dist = DotProduct (r_refdef.view.origin, frustum[4].normal) + r_nearclip.value;
	//PlaneClassify(&frustum[4]);
}

void R_View_Update(void)
{
	R_View_SetFrustum();
	R_View_WorldVisibility(r_refdef.view.useclipplane);
	R_View_UpdateEntityVisible();
}

void R_SetupView(void)
{
	if (!r_refdef.view.useperspective)
		GL_SetupView_Mode_Ortho(-r_refdef.view.ortho_x, -r_refdef.view.ortho_y, r_refdef.view.ortho_x, r_refdef.view.ortho_y, -r_refdef.farclip, r_refdef.farclip);
	else if (r_refdef.rtworldshadows || r_refdef.rtdlightshadows)
		GL_SetupView_Mode_PerspectiveInfiniteFarClip(r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip);
	else
		GL_SetupView_Mode_Perspective(r_refdef.view.frustum_x, r_refdef.view.frustum_y, r_refdef.nearclip, r_refdef.farclip);

	GL_SetupView_Orientation_FromEntity(&r_refdef.view.matrix);

	if (r_refdef.view.useclipplane)
	{
		// LordHavoc: couldn't figure out how to make this approach the
		vec_t dist = r_refdef.view.clipplane.dist - r_water_clippingplanebias.value;
		vec_t viewdist = DotProduct(r_refdef.view.origin, r_refdef.view.clipplane.normal);
		if (viewdist < r_refdef.view.clipplane.dist + r_water_clippingplanebias.value)
			dist = r_refdef.view.clipplane.dist;
		GL_SetupView_ApplyCustomNearClipPlane(r_refdef.view.clipplane.normal[0], r_refdef.view.clipplane.normal[1], r_refdef.view.clipplane.normal[2], dist);
	}
}

void R_ResetViewRendering2D(void)
{
	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}

	DrawQ_Finish();

	// GL is weird because it's bottom to top, r_refdef.view.y is top to bottom
	qglViewport(r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR
	GL_SetupView_Mode_Ortho(0, 0, 1, 1, -10, 100);
	GL_Scissor(r_refdef.view.x, r_refdef.view.y, r_refdef.view.width, r_refdef.view.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_AlphaTest(false);
	GL_ScissorTest(false);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_DepthTest(false);
	R_Mesh_Matrix(&identitymatrix);
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
	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}

	DrawQ_Finish();

	// GL is weird because it's bottom to top, r_refdef.view.y is top to bottom
	qglViewport(r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR
	R_SetupView();
	GL_Scissor(r_refdef.view.x, r_refdef.view.y, r_refdef.view.width, r_refdef.view.height);
	GL_Color(1, 1, 1, 1);
	GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_AlphaTest(false);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	GL_DepthRange(0, 1);
	GL_DepthTest(true);
	R_Mesh_Matrix(&identitymatrix);
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

/*
	R_Bloom_SetupShader(
"// bloom shader\n"
"// written by Forest 'LordHavoc' Hale\n"
"\n"
"// common definitions between vertex shader and fragment shader:\n"
"\n"
"#ifdef __GLSL_CG_DATA_TYPES\n"
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
"varying vec2 ScreenTexCoord;\n"
"varying vec2 BloomTexCoord;\n"
"\n"
"\n"
"\n"
"\n"
"// vertex shader specific:\n"
"#ifdef VERTEX_SHADER\n"
"\n"
"void main(void)\n"
"{\n"
"	ScreenTexCoord = vec2(gl_MultiTexCoord0);\n"
"	BloomTexCoord = vec2(gl_MultiTexCoord1);\n"
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
"void main(void)\n"
"{\n"
"	int x, y;
"	myhvec3 color = myhvec3(texture2D(Texture_Screen, ScreenTexCoord));\n"
"	for (x = -BLUR_X;x <= BLUR_X;x++)
"	color.rgb += myhvec3(texture2D(Texture_Bloom, BloomTexCoord));\n"
"	color.rgb += myhvec3(texture2D(Texture_Bloom, BloomTexCoord));\n"
"	color.rgb += myhvec3(texture2D(Texture_Bloom, BloomTexCoord));\n"
"	color.rgb += myhvec3(texture2D(Texture_Bloom, BloomTexCoord));\n"

"	gl_FragColor = vec4(color);\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
*/

void R_RenderScene(qboolean addwaterplanes);

static void R_Water_StartFrame(void)
{
	int i;
	int waterwidth, waterheight, texturewidth, textureheight;
	r_waterstate_waterplane_t *p;

	// set waterwidth and waterheight to the water resolution that will be
	// used (often less than the screen resolution for faster rendering)
	waterwidth = (int)bound(1, r_refdef.view.width * r_water_resolutionmultiplier.value, r_refdef.view.width);
	waterheight = (int)bound(1, r_refdef.view.height * r_water_resolutionmultiplier.value, r_refdef.view.height);

	// calculate desired texture sizes
	// can't use water if the card does not support the texture size
	if (!r_water.integer || !r_glsl.integer || !gl_support_fragment_shader || waterwidth > gl_max_texture_size || waterheight > gl_max_texture_size)
		texturewidth = textureheight = waterwidth = waterheight = 0;
	else if (gl_support_arb_texture_non_power_of_two)
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
	if (r_waterstate.waterwidth != waterwidth || r_waterstate.waterheight != waterheight || r_waterstate.texturewidth != texturewidth || r_waterstate.textureheight != textureheight)
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
		r_waterstate.waterwidth = waterwidth;
		r_waterstate.waterheight = waterheight;
		r_waterstate.texturewidth = texturewidth;
		r_waterstate.textureheight = textureheight;
	}

	if (r_waterstate.waterwidth)
	{
		r_waterstate.enabled = true;

		// set up variables that will be used in shader setup
		r_waterstate.screenscale[0] = 0.5f * (float)waterwidth / (float)texturewidth;
		r_waterstate.screenscale[1] = 0.5f * (float)waterheight / (float)textureheight;
		r_waterstate.screencenter[0] = 0.5f * (float)waterwidth / (float)texturewidth;
		r_waterstate.screencenter[1] = 0.5f * (float)waterheight / (float)textureheight;
	}

	r_waterstate.maxwaterplanes = MAX_WATERPLANES;
	r_waterstate.numwaterplanes = 0;
}

static void R_Water_AddWaterPlane(msurface_t *surface)
{
	int triangleindex, planeindex;
	const int *e;
	vec3_t vert[3];
	vec3_t normal;
	vec3_t center;
	r_waterstate_waterplane_t *p;
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
		VectorCopy(normal, p->plane.normal);
		VectorNormalize(p->plane.normal);
		p->plane.dist = DotProduct(vert[0], p->plane.normal);
		PlaneClassify(&p->plane);
		// flip the plane if it does not face the viewer
		if (PlaneDiff(r_refdef.view.origin, &p->plane) < 0)
		{
			VectorNegate(p->plane.normal, p->plane.normal);
			p->plane.dist *= -1;
			PlaneClassify(&p->plane);
		}
		// clear materialflags and pvs
		p->materialflags = 0;
		p->pvsvalid = false;
	}
	// merge this surface's materialflags into the waterplane
	p->materialflags |= surface->texture->currentframe->currentmaterialflags;
	// merge this surface's PVS into the waterplane
	VectorMAM(0.5f, surface->mins, 0.5f, surface->maxs, center);
	if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION) && r_refdef.worldmodel && r_refdef.worldmodel->brush.FatPVS
	 && r_refdef.worldmodel->brush.PointInLeaf && r_refdef.worldmodel->brush.PointInLeaf(r_refdef.worldmodel, center)->clusterindex >= 0)
	{
		r_refdef.worldmodel->brush.FatPVS(r_refdef.worldmodel, center, 2, p->pvsbits, sizeof(p->pvsbits), p->pvsvalid);
		p->pvsvalid = true;
	}
}

static void R_Water_ProcessPlanes(void)
{
	r_refdef_view_t originalview;
	int planeindex;
	r_waterstate_waterplane_t *p;

	originalview = r_refdef.view;

	// make sure enough textures are allocated
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			if (!p->texture_refraction)
				p->texture_refraction = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_refraction", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
			if (!p->texture_refraction)
				goto error;
		}

		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION))
		{
			if (!p->texture_reflection)
				p->texture_reflection = R_LoadTexture2D(r_main_texturepool, va("waterplane%i_reflection", planeindex), r_waterstate.texturewidth, r_waterstate.textureheight, NULL, TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
			if (!p->texture_reflection)
				goto error;
		}
	}

	// render views
	for (planeindex = 0, p = r_waterstate.waterplanes;planeindex < r_waterstate.numwaterplanes;planeindex++, p++)
	{
		r_refdef.view.showdebug = false;
		r_refdef.view.width = r_waterstate.waterwidth;
		r_refdef.view.height = r_waterstate.waterheight;
		r_refdef.view.useclipplane = true;
		r_waterstate.renderingscene = true;

		// render the normal view scene and copy into texture
		// (except that a clipping plane should be used to hide everything on one side of the water, and the viewer's weapon model should be omitted)
		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
		{
			r_refdef.view.clipplane = p->plane;
			VectorNegate(r_refdef.view.clipplane.normal, r_refdef.view.clipplane.normal);
			r_refdef.view.clipplane.dist = -r_refdef.view.clipplane.dist;
			PlaneClassify(&r_refdef.view.clipplane);

			R_RenderScene(false);

			// copy view into the screen texture
			R_Mesh_TexBind(0, R_GetTexture(p->texture_refraction));
			GL_ActiveTexture(0);
			CHECKGLERROR
			qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR
		}

		if (p->materialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION))
		{
			// render reflected scene and copy into texture
			Matrix4x4_Reflect(&r_refdef.view.matrix, p->plane.normal[0], p->plane.normal[1], p->plane.normal[2], p->plane.dist, -2);
			r_refdef.view.clipplane = p->plane;
			// reverse the cullface settings for this render
			r_refdef.view.cullface_front = GL_FRONT;
			r_refdef.view.cullface_back = GL_BACK;
			if (r_refdef.worldmodel && r_refdef.worldmodel->brush.num_pvsclusterbytes)
			{
				r_refdef.view.usecustompvs = true;
				if (p->pvsvalid)
					memcpy(r_refdef.viewcache.world_pvsbits, p->pvsbits, r_refdef.worldmodel->brush.num_pvsclusterbytes);
				else
					memset(r_refdef.viewcache.world_pvsbits, 0xFF, r_refdef.worldmodel->brush.num_pvsclusterbytes);
			}

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			if (r_timereport_active)
				R_TimeReport("viewclear");

			R_RenderScene(false);

			R_Mesh_TexBind(0, R_GetTexture(p->texture_reflection));
			GL_ActiveTexture(0);
			CHECKGLERROR
			qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR

			R_ResetViewRendering3D();
			R_ClearScreen(r_refdef.fogenabled);
			if (r_timereport_active)
				R_TimeReport("viewclear");
		}

		r_refdef.view = originalview;
		r_refdef.view.clear = true;
		r_waterstate.renderingscene = false;
	}
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

	// set bloomwidth and bloomheight to the bloom resolution that will be
	// used (often less than the screen resolution for faster rendering)
	r_bloomstate.bloomwidth = bound(1, r_bloom_resolution.integer, r_refdef.view.width);
	r_bloomstate.bloomheight = r_bloomstate.bloomwidth * r_refdef.view.height / r_refdef.view.width;
	r_bloomstate.bloomheight = bound(1, r_bloomstate.bloomheight, r_refdef.view.height);

	// calculate desired texture sizes
	if (gl_support_arb_texture_non_power_of_two)
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

	if (r_hdr.integer)
	{
		screentexturewidth = screentextureheight = 0;
	}
	else if (r_bloom.integer)
	{
	}
	else
	{
		screentexturewidth = screentextureheight = 0;
		bloomtexturewidth = bloomtextureheight = 0;
	}

	if ((!bloomtexturewidth && !bloomtextureheight) || r_bloom_resolution.integer < 4 || r_bloom_blur.value < 1 || r_bloom_blur.value >= 512 || screentexturewidth > gl_max_texture_size || screentextureheight > gl_max_texture_size || bloomtexturewidth > gl_max_texture_size || bloomtextureheight > gl_max_texture_size)
	{
		// can't use bloom if the parameters are too weird
		// can't use bloom if the card does not support the texture size
		if (r_bloomstate.texture_screen)
			R_FreeTexture(r_bloomstate.texture_screen);
		if (r_bloomstate.texture_bloom)
			R_FreeTexture(r_bloomstate.texture_bloom);
		memset(&r_bloomstate, 0, sizeof(r_bloomstate));
		return;
	}

	r_bloomstate.enabled = true;
	r_bloomstate.hdr = r_hdr.integer != 0;

	// allocate textures as needed
	if (r_bloomstate.screentexturewidth != screentexturewidth || r_bloomstate.screentextureheight != screentextureheight)
	{
		if (r_bloomstate.texture_screen)
			R_FreeTexture(r_bloomstate.texture_screen);
		r_bloomstate.texture_screen = NULL;
		r_bloomstate.screentexturewidth = screentexturewidth;
		r_bloomstate.screentextureheight = screentextureheight;
		if (r_bloomstate.screentexturewidth && r_bloomstate.screentextureheight)
			r_bloomstate.texture_screen = R_LoadTexture2D(r_main_texturepool, "screen", r_bloomstate.screentexturewidth, r_bloomstate.screentextureheight, NULL, TEXTYPE_BGRA, TEXF_FORCENEAREST | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
	}
	if (r_bloomstate.bloomtexturewidth != bloomtexturewidth || r_bloomstate.bloomtextureheight != bloomtextureheight)
	{
		if (r_bloomstate.texture_bloom)
			R_FreeTexture(r_bloomstate.texture_bloom);
		r_bloomstate.texture_bloom = NULL;
		r_bloomstate.bloomtexturewidth = bloomtexturewidth;
		r_bloomstate.bloomtextureheight = bloomtextureheight;
		if (r_bloomstate.bloomtexturewidth && r_bloomstate.bloomtextureheight)
			r_bloomstate.texture_bloom = R_LoadTexture2D(r_main_texturepool, "bloom", r_bloomstate.bloomtexturewidth, r_bloomstate.bloomtextureheight, NULL, TEXTYPE_BGRA, TEXF_FORCELINEAR | TEXF_CLAMP | TEXF_ALWAYSPRECACHE, NULL);
	}

	// set up a texcoord array for the full resolution screen image
	// (we have to keep this around to copy back during final render)
	r_bloomstate.screentexcoord2f[0] = 0;
	r_bloomstate.screentexcoord2f[1] = (float)r_refdef.view.height / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[2] = (float)r_refdef.view.width / (float)r_bloomstate.screentexturewidth;
	r_bloomstate.screentexcoord2f[3] = (float)r_refdef.view.height / (float)r_bloomstate.screentextureheight;
	r_bloomstate.screentexcoord2f[4] = (float)r_refdef.view.width / (float)r_bloomstate.screentexturewidth;
	r_bloomstate.screentexcoord2f[5] = 0;
	r_bloomstate.screentexcoord2f[6] = 0;
	r_bloomstate.screentexcoord2f[7] = 0;

	// set up a texcoord array for the reduced resolution bloom image
	// (which will be additive blended over the screen image)
	r_bloomstate.bloomtexcoord2f[0] = 0;
	r_bloomstate.bloomtexcoord2f[1] = (float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
	r_bloomstate.bloomtexcoord2f[2] = (float)r_bloomstate.bloomwidth / (float)r_bloomstate.bloomtexturewidth;
	r_bloomstate.bloomtexcoord2f[3] = (float)r_bloomstate.bloomheight / (float)r_bloomstate.bloomtextureheight;
	r_bloomstate.bloomtexcoord2f[4] = (float)r_bloomstate.bloomwidth / (float)r_bloomstate.bloomtexturewidth;
	r_bloomstate.bloomtexcoord2f[5] = 0;
	r_bloomstate.bloomtexcoord2f[6] = 0;
	r_bloomstate.bloomtexcoord2f[7] = 0;
}

void R_Bloom_CopyScreenTexture(float colorscale)
{
	r_refdef.stats.bloom++;

	R_ResetViewRendering2D();
	R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_TexCoordPointer(0, 2, r_bloomstate.screentexcoord2f, 0, 0);
	R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_screen));

	// copy view into the screen texture
	GL_ActiveTexture(0);
	CHECKGLERROR
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR
	r_refdef.stats.bloom_copypixels += r_refdef.view.width * r_refdef.view.height;

	// now scale it down to the bloom texture size
	CHECKGLERROR
	qglViewport(r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_Color(colorscale, colorscale, colorscale, 1);
	// TODO: optimize with multitexture or GLSL
	R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
	r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

	// we now have a bloom image in the framebuffer
	// copy it into the bloom image texture for later processing
	R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
	GL_ActiveTexture(0);
	CHECKGLERROR
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR
	r_refdef.stats.bloom_copypixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
}

void R_Bloom_CopyHDRTexture(void)
{
	R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
	GL_ActiveTexture(0);
	CHECKGLERROR
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_refdef.view.height), r_refdef.view.width, r_refdef.view.height);CHECKGLERROR
	r_refdef.stats.bloom_copypixels += r_refdef.view.width * r_refdef.view.height;
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
	qglViewport(r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR

	for (x = 1;x < min(r_bloom_colorexponent.value, 32);)
	{
		x *= 2;
		r = bound(0, r_bloom_colorexponent.value / x, 1);
		GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		GL_Color(r, r, r, 1);
		R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

		// copy the vertically blurred bloom view to a texture
		GL_ActiveTexture(0);
		CHECKGLERROR
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR
		r_refdef.stats.bloom_copypixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
	}

	range = r_bloom_blur.integer * r_bloomstate.bloomwidth / 320;
	brighten = r_bloom_brighten.value;
	if (r_hdr.integer)
		brighten *= r_hdr_range.value;
	R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
	R_Mesh_TexCoordPointer(0, 2, r_bloomstate.offsettexcoord2f, 0, 0);

	for (dir = 0;dir < 2;dir++)
	{
		// blend on at multiple vertical offsets to achieve a vertical blur
		// TODO: do offset blends using GLSL
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
			//r = (dir ? 1.0f : brighten)/(range*2+1);
			r = (dir ? 1.0f : brighten)/(range*2+1)*(1 - x*x/(float)(range*range));
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
			r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}

		// copy the vertically blurred bloom view to a texture
		GL_ActiveTexture(0);
		CHECKGLERROR
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR
		r_refdef.stats.bloom_copypixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
	}

	// apply subtract last
	// (just like it would be in a GLSL shader)
	if (r_bloom_colorsubtract.value > 0 && gl_support_ext_blend_subtract)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		GL_Color(1, 1, 1, 1);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;

		GL_BlendFunc(GL_ONE, GL_ONE);
		qglBlendEquationEXT(GL_FUNC_REVERSE_SUBTRACT_EXT);
		R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		GL_Color(r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, r_bloom_colorsubtract.value, 1);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
		qglBlendEquationEXT(GL_FUNC_ADD_EXT);

		// copy the darkened bloom view to a texture
		R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
		GL_ActiveTexture(0);
		CHECKGLERROR
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_refdef.view.x, vid.height - (r_refdef.view.y + r_bloomstate.bloomheight), r_bloomstate.bloomwidth, r_bloomstate.bloomheight);CHECKGLERROR
		r_refdef.stats.bloom_copypixels += r_bloomstate.bloomwidth * r_bloomstate.bloomheight;
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
	// TODO: add fp16 framebuffer support

	r_refdef.view.showdebug = false;
	r_refdef.view.colorscale *= r_bloom_colorscale.value / bound(1, r_hdr_range.value, 16);

	R_ClearScreen(r_refdef.fogenabled);
	if (r_timereport_active)
		R_TimeReport("HDRclear");

	r_waterstate.numwaterplanes = 0;
	R_RenderScene(r_waterstate.enabled);
	r_refdef.view.showdebug = true;

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
	if (r_bloomstate.enabled && r_bloomstate.hdr)
	{
		// render high dynamic range bloom effect
		// the bloom texture was made earlier this render, so we just need to
		// blend it onto the screen...
		R_ResetViewRendering2D();
		R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_Color(1, 1, 1, 1);
		GL_BlendFunc(GL_ONE, GL_ONE);
		R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_refdef.view.width * r_refdef.view.height;
	}
	else if (r_bloomstate.enabled)
	{
		// render simple bloom effect
		// copy the screen and shrink it and darken it for the bloom process
		R_Bloom_CopyScreenTexture(r_bloom_colorscale.value);
		// make the bloom texture
		R_Bloom_MakeTexture();
		// put the original screen image back in place and blend the bloom
		// texture on it
		R_ResetViewRendering2D();
		R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_Color(1, 1, 1, 1);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		// do both in one pass if possible
		R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_bloom));
		R_Mesh_TexCoordPointer(0, 2, r_bloomstate.bloomtexcoord2f, 0, 0);
		if (r_textureunits.integer >= 2 && gl_combine.integer)
		{
			R_Mesh_TexCombine(1, GL_ADD, GL_ADD, 1, 1);
			R_Mesh_TexBind(1, R_GetTexture(r_bloomstate.texture_screen));
			R_Mesh_TexCoordPointer(1, 2, r_bloomstate.screentexcoord2f, 0, 0);
		}
		else
		{
			R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
			r_refdef.stats.bloom_drawpixels += r_refdef.view.width * r_refdef.view.height;
			// now blend on the bloom texture
			GL_BlendFunc(GL_ONE, GL_ONE);
			R_Mesh_TexBind(0, R_GetTexture(r_bloomstate.texture_screen));
			R_Mesh_TexCoordPointer(0, 2, r_bloomstate.screentexcoord2f, 0, 0);
		}
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
		r_refdef.stats.bloom_drawpixels += r_refdef.view.width * r_refdef.view.height;
	}
	if (r_refdef.viewblend[3] >= (1.0f / 256.0f))
	{
		// apply a color tint to the whole view
		R_ResetViewRendering2D();
		R_Mesh_VertexPointer(r_screenvertex3f, 0, 0);
		R_Mesh_ColorPointer(NULL, 0, 0);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
	}
}

void R_RenderScene(qboolean addwaterplanes);

matrix4x4_t r_waterscrollmatrix;

void R_UpdateFogColor(void) // needs to be called before HDR subrender too, as that changes colorscale!
{
	if (r_refdef.fog_density)
	{
		r_refdef.fogcolor[0] = r_refdef.fog_red;
		r_refdef.fogcolor[1] = r_refdef.fog_green;
		r_refdef.fogcolor[2] = r_refdef.fog_blue;

		{
			vec3_t fogvec;
			VectorCopy(r_refdef.fogcolor, fogvec);
			if(r_glsl.integer && (r_glsl_contrastboost.value > 1 || r_glsl_contrastboost.value < 0)) // need to support contrast boost
			{
				//   color.rgb /= ((ContrastBoost - 1) * color.rgb + 1);
				fogvec[0] *= r_glsl_contrastboost.value / ((r_glsl_contrastboost.value - 1) * fogvec[0] + 1);
				fogvec[1] *= r_glsl_contrastboost.value / ((r_glsl_contrastboost.value - 1) * fogvec[1] + 1);
				fogvec[2] *= r_glsl_contrastboost.value / ((r_glsl_contrastboost.value - 1) * fogvec[2] + 1);
			}
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

	r_refdef.farclip = 4096;
	if (r_refdef.worldmodel)
		r_refdef.farclip += VectorDistance(r_refdef.worldmodel->normalmins, r_refdef.worldmodel->normalmaxs);
	r_refdef.nearclip = bound (0.001f, r_nearclip.value, r_refdef.farclip - 1.0f);

	if (r_shadow_frontsidecasting.integer < 0 || r_shadow_frontsidecasting.integer > 1)
		Cvar_SetValueQuick(&r_shadow_frontsidecasting, 1);
	r_refdef.polygonfactor = 0;
	r_refdef.polygonoffset = 0;
	r_refdef.shadowpolygonfactor = r_refdef.polygonfactor + r_shadow_polygonfactor.value * (r_shadow_frontsidecasting.integer ? 1 : -1);
	r_refdef.shadowpolygonoffset = r_refdef.polygonoffset + r_shadow_polygonoffset.value * (r_shadow_frontsidecasting.integer ? 1 : -1);

	r_refdef.rtworld = r_shadow_realtime_world.integer;
	r_refdef.rtworldshadows = r_shadow_realtime_world_shadows.integer && gl_stencil;
	r_refdef.rtdlight = (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer) && !gl_flashblend.integer && r_dynamic.integer;
	r_refdef.rtdlightshadows = r_refdef.rtdlight && r_shadow_realtime_dlight_shadows.integer && gl_stencil;
	r_refdef.lightmapintensity = r_refdef.rtworld ? r_shadow_realtime_world_lightmaps.value : 1;
	if (r_showsurfaces.integer)
	{
		r_refdef.rtworld = false;
		r_refdef.rtworldshadows = false;
		r_refdef.rtdlight = false;
		r_refdef.rtdlightshadows = false;
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
		}
	}

	r_refdef.fog_alpha = bound(0, r_refdef.fog_alpha, 1);
	r_refdef.fog_start = max(0, r_refdef.fog_start);
	r_refdef.fog_end = max(r_refdef.fog_start + 0.01, r_refdef.fog_end);

	// R_UpdateFogColor(); // why? R_RenderScene does it anyway

	if (r_refdef.fog_density)
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
}

/*
================
R_RenderView
================
*/
void R_RenderView(void)
{
	if (!r_refdef.entities/* || !r_refdef.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	r_refdef.view.colorscale = r_hdr_scenebrightness.value;

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

	r_refdef.view.showdebug = true;

	// this produces a bloom texture to be used in R_BlendView() later
	if (r_hdr.integer)
		R_HDR_RenderBloomTexture();

	r_waterstate.numwaterplanes = 0;
	R_RenderScene(r_waterstate.enabled);

	R_BlendView();
	if (r_timereport_active)
		R_TimeReport("blendview");

	GL_Scissor(0, 0, vid.width, vid.height);
	GL_ScissorTest(false);
	CHECKGLERROR
}

extern void R_DrawLightningBeams (void);
extern void VM_CL_AddPolygonsToMeshQueue (void);
extern void R_DrawPortals (void);
extern cvar_t cl_locs_show;
static void R_DrawLocs(void);
static void R_DrawEntityBBoxes(void);
void R_RenderScene(qboolean addwaterplanes)
{
	Matrix4x4_Invert_Simple(&r_refdef.view.inverse_matrix, &r_refdef.view.matrix);
	R_UpdateFogColor();

	if (addwaterplanes)
	{
		R_ResetViewRendering3D();

		R_View_Update();
		if (r_timereport_active)
			R_TimeReport("watervis");

		if (cl.csqc_vidvars.drawworld && r_refdef.worldmodel && r_refdef.worldmodel->DrawAddWaterPlanes)
		{
			r_refdef.worldmodel->DrawAddWaterPlanes(r_refdef.worldentity);
			if (r_timereport_active)
				R_TimeReport("waterworld");
		}

		// don't let sound skip if going slow
		if (r_refdef.extraupdate)
			S_ExtraUpdate ();

		R_DrawModelsAddWaterPlanes();
		if (r_timereport_active)
			R_TimeReport("watermodels");

		R_Water_ProcessPlanes();
		if (r_timereport_active)
			R_TimeReport("waterscenes");
	}

	R_ResetViewRendering3D();

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	R_MeshQueue_BeginScene();

	R_SkyStartFrame();

	R_View_Update();
	if (r_timereport_active)
		R_TimeReport("visibility");

	Matrix4x4_CreateTranslate(&r_waterscrollmatrix, sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);

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
	}

	if (r_depthfirst.integer >= 1 && cl.csqc_vidvars.drawworld && r_refdef.worldmodel && r_refdef.worldmodel->DrawDepth)
	{
		r_refdef.worldmodel->DrawDepth(r_refdef.worldentity);
		if (r_timereport_active)
			R_TimeReport("worlddepth");
	}
	if (r_depthfirst.integer >= 2)
	{
		R_DrawModelsDepth();
		if (r_timereport_active)
			R_TimeReport("modeldepth");
	}

	if (cl.csqc_vidvars.drawworld && r_refdef.worldmodel && r_refdef.worldmodel->Draw)
	{
		r_refdef.worldmodel->Draw(r_refdef.worldentity);
		if (r_timereport_active)
			R_TimeReport("world");
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

	if (r_shadows.integer > 0 && r_refdef.lightmapintensity > 0)
	{
		R_DrawModelShadows();

		R_ResetViewRendering3D();

		// don't let sound skip if going slow
		if (r_refdef.extraupdate)
			S_ExtraUpdate ();
	}

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

		R_DrawDecals();
		if (r_timereport_active)
			R_TimeReport("decals");

		R_DrawParticles();
		if (r_timereport_active)
			R_TimeReport("particles");

		R_DrawExplosions();
		if (r_timereport_active)
			R_TimeReport("explosions");
	}

	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
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

	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}
	R_MeshQueue_RenderTransparent();
	if (r_timereport_active)
		R_TimeReport("drawtrans");

	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}

	if (r_refdef.view.showdebug && r_refdef.worldmodel && r_refdef.worldmodel->DrawDebug && (r_showtris.value > 0 || r_shownormals.value > 0 || r_showcollisionbrushes.value > 0))
	{
		r_refdef.worldmodel->DrawDebug(r_refdef.worldentity);
		if (r_timereport_active)
			R_TimeReport("worlddebug");
		R_DrawModelsDebug();
		if (r_timereport_active)
			R_TimeReport("modeldebug");
	}

	if (gl_support_fragment_shader)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}

	if (cl.csqc_vidvars.drawworld)
	{
		R_DrawCoronas();
		if (r_timereport_active)
			R_TimeReport("coronas");
	}

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	R_ResetViewRendering2D();
}

static const int bboxelements[36] =
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
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	R_Mesh_Matrix(&identitymatrix);
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
			f1 = FogPoint_World(v);
			f2 = 1 - f1;
			c[0] = c[0] * f1 + r_refdef.fogcolor[0] * f2;
			c[1] = c[1] * f1 + r_refdef.fogcolor[1] * f2;
			c[2] = c[2] * f1 + r_refdef.fogcolor[2] * f2;
		}
	}
	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(color4f, 0, 0);
	R_Mesh_ResetTextureState();
	R_Mesh_Draw(0, 8, 12, bboxelements, 0, 0);
}

static void R_DrawEntityBBoxes_Callback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i;
	float color[4];
	prvm_edict_t *edict;
	// this function draws bounding boxes of server entities
	if (!sv.active)
		return;
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
}

static void R_DrawEntityBBoxes(void)
{
	int i;
	prvm_edict_t *edict;
	vec3_t center;
	// this function draws bounding boxes of server entities
	if (!sv.active)
		return;
	SV_VM_Begin();
	for (i = 0;i < prog->num_edicts;i++)
	{
		edict = PRVM_EDICT_NUM(i);
		if (edict->priv.server->free)
			continue;
		VectorLerp(edict->priv.server->areamins, 0.5f, edict->priv.server->areamaxs, center);
		R_MeshQueue_AddTransparent(center, R_DrawEntityBBoxes_Callback, (entity_render_t *)NULL, i, (rtlight_t *)NULL);
	}
	SV_VM_End();
}

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

void R_DrawNoModel_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i;
	float f1, f2, *c;
	float color4f[6*4];
	// this is only called once per entity so numsurfaces is always 1, and
	// surfacelist is always {0}, so this code does not handle batches
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
	GL_DepthRange(0, (ent->flags & RENDER_VIEWMODEL) ? 0.0625 : 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
	GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
	GL_CullFace((ent->effects & EF_DOUBLESIDED) ? GL_NONE : r_refdef.view.cullface_back);
	R_Mesh_VertexPointer(nomodelvertex3f, 0, 0);
	if (r_refdef.fogenabled)
	{
		vec3_t org;
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		R_Mesh_ColorPointer(color4f, 0, 0);
		Matrix4x4_OriginFromMatrix(&ent->matrix, org);
		f1 = FogPoint_World(org);
		f2 = 1 - f1;
		for (i = 0, c = color4f;i < 6;i++, c += 4)
		{
			c[0] = (c[0] * f1 + r_refdef.fogcolor[0] * f2);
			c[1] = (c[1] * f1 + r_refdef.fogcolor[1] * f2);
			c[2] = (c[2] * f1 + r_refdef.fogcolor[2] * f2);
			c[3] *= ent->alpha;
		}
	}
	else if (ent->alpha != 1)
	{
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		R_Mesh_ColorPointer(color4f, 0, 0);
		for (i = 0, c = color4f;i < 6;i++, c += 4)
			c[3] *= ent->alpha;
	}
	else
		R_Mesh_ColorPointer(nomodelcolor4f, 0, 0);
	R_Mesh_ResetTextureState();
	R_Mesh_Draw(0, 6, 8, nomodelelements, 0, 0);
}

void R_DrawNoModel(entity_render_t *ent)
{
	vec3_t org;
	Matrix4x4_OriginFromMatrix(&ent->matrix, org);
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_refdef.view.origin : org, R_DrawNoModel_TransparentCallback, ent, 0, rsurface.rtlight);
	//else
	//	R_DrawNoModelCallback(ent, 0);
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

float spritetexcoord2f[4*2] = {0, 1, 0, 0, 1, 0, 1, 1};

void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, rtexture_t *fogtexture, qboolean depthdisable, qboolean depthshort, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca)
{
	float fog = 1.0f;
	float vertex3f[12];

	if (r_refdef.fogenabled && !depthdisable) // TODO maybe make the unfog effect a separate flag?
		fog = FogPoint_World(origin);

	R_Mesh_Matrix(&identitymatrix);
	GL_BlendFunc(blendfunc1, blendfunc2);

	if(v_flipped_state)
	{
		scalex1 = -scalex1;
		scalex2 = -scalex2;
		GL_CullFace(r_refdef.view.cullface_front);
	}
	else
		GL_CullFace(r_refdef.view.cullface_back);

	GL_DepthMask(false);
	GL_DepthRange(0, depthshort ? 0.0625 : 1);
	GL_PolygonOffset(r_refdef.polygonfactor, r_refdef.polygonoffset);
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

	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_Mesh_TexBind(0, R_GetTexture(texture));
	R_Mesh_TexCoordPointer(0, 2, spritetexcoord2f, 0, 0);
	// FIXME: fixed function path can't properly handle r_refdef.view.colorscale > 1
	GL_Color(cr * fog * r_refdef.view.colorscale, cg * fog * r_refdef.view.colorscale, cb * fog * r_refdef.view.colorscale, ca);
	R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);

	if (blendfunc2 == GL_ONE_MINUS_SRC_ALPHA)
	{
		R_Mesh_TexBind(0, R_GetTexture(fogtexture));
		GL_BlendFunc(blendfunc1, GL_ONE);
		fog = 1 - fog;
		GL_Color(r_refdef.fogcolor[0] * fog, r_refdef.fogcolor[1] * fog, r_refdef.fogcolor[2] * fog, ca);
		R_Mesh_Draw(0, 4, 2, polygonelements, 0, 0);
	}
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
		maxdist = max(maxdist, planes[w].dist);
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
	layer->color[0] = r * r_refdef.view.colorscale;
	layer->color[1] = g * r_refdef.view.colorscale;
	layer->color[2] = b * r_refdef.view.colorscale;
	layer->color[3] = a;
}

static float R_EvaluateQ3WaveFunc(q3wavefunc_t func, const float *parms)
{
	double index, f;
	index = parms[2] + r_refdef.time * parms[3];
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

void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t)
{
	int i;
	model_t *model = ent->model;
	float f;
	float tcmat[12];
	q3shaderinfo_layer_tcmod_t *tcmod;

	// switch to an alternate material if this is a q1bsp animated material
	{
		texture_t *texture = t;
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
		{
			// use an alternate animation if the entity's frame is not 0,
			// and only if the texture has an alternate animation
			if (ent->frame2 != 0 && t->anim_total[1])
				t = t->anim_frames[1][(t->anim_total[1] >= 2) ? ((int)(r_refdef.time * 5.0f) % t->anim_total[1]) : 0];
			else
				t = t->anim_frames[0][(t->anim_total[0] >= 2) ? ((int)(r_refdef.time * 5.0f) % t->anim_total[0]) : 0];
		}
		texture->currentframe = t;
	}

	// update currentskinframe to be a qw skin or animation frame
	if ((i = ent->entitynumber - 1) >= 0 && i < cl.maxclients)
	{
		if (strcmp(r_qwskincache[i], cl.scores[i].qw_skin))
		{
			strlcpy(r_qwskincache[i], cl.scores[i].qw_skin, sizeof(r_qwskincache[i]));
			Con_DPrintf("loading skins/%s\n", r_qwskincache[i]);
			r_qwskincache_skinframe[i] = R_SkinFrame_LoadExternal(va("skins/%s", r_qwskincache[i]), TEXF_PRECACHE | (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_PICMIP | TEXF_COMPRESS, developer.integer > 0);
		}
		t->currentskinframe = r_qwskincache_skinframe[i];
		if (t->currentskinframe == NULL)
			t->currentskinframe = t->skinframes[(int)(t->skinframerate * (cl.time - ent->frame2time)) % t->numskinframes];
	}
	else if (t->numskinframes >= 2)
		t->currentskinframe = t->skinframes[(int)(t->skinframerate * (cl.time - ent->frame2time)) % t->numskinframes];
	if (t->backgroundnumskinframes >= 2)
		t->backgroundcurrentskinframe = t->backgroundskinframes[(int)(t->backgroundskinframerate * (cl.time - ent->frame2time)) % t->backgroundnumskinframes];

	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = ent->alpha;
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA && (model->brush.supportwateralpha || r_novis.integer))
	{
		t->currentalpha *= r_wateralpha.value;
		/*
		 * FIXME what is this supposed to do?
		// if rendering refraction/reflection, disable transparency
		if (r_waterstate.enabled && (t->currentalpha < 1 || (t->currentmaterialflags & MATERIALFLAG_ALPHA)))
			t->currentmaterialflags |= MATERIALFLAG_WATERSHADER;
		*/
	}
	if(!r_waterstate.enabled)
		t->currentmaterialflags &= ~(MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION);
	if (!(ent->flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	else if (rsurface.modeltexcoordlightmap2f == NULL)
	{
		// pick a model lighting mode
		if (VectorLength2(ent->modellight_diffuse) >= (1.0f / 256.0f))
			t->currentmaterialflags |= MATERIALFLAG_MODELLIGHT | MATERIALFLAG_MODELLIGHT_DIRECTIONAL;
		else
			t->currentmaterialflags |= MATERIALFLAG_MODELLIGHT;
	}
	if (ent->effects & EF_ADDITIVE)
		t->currentmaterialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	else if (t->currentalpha < 1)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	if (ent->effects & EF_DOUBLESIDED)
		t->currentmaterialflags |= MATERIALFLAG_NOSHADOW | MATERIALFLAG_NOCULLFACE;
	if (ent->effects & EF_NODEPTHTEST)
		t->currentmaterialflags |= MATERIALFLAG_SHORTDEPTHRANGE;
	if (ent->flags & RENDER_VIEWMODEL)
		t->currentmaterialflags |= MATERIALFLAG_SHORTDEPTHRANGE;
	if (t->backgroundnumskinframes && !(t->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED))
		t->currentmaterialflags |= MATERIALFLAG_VERTEXTEXTUREBLEND;

	// make sure that the waterscroll matrix is used on water surfaces when
	// there is no tcmod
	if (t->currentmaterialflags & MATERIALFLAG_WATER && r_waterscroll.value != 0)
		t->currenttexmatrix = r_waterscrollmatrix;

	for (i = 0, tcmod = t->tcmods;i < Q3MAXTCMODS && tcmod->tcmod;i++, tcmod++)
	{
		matrix4x4_t matrix;
		switch(tcmod->tcmod)
		{
		case Q3TCMOD_COUNT:
		case Q3TCMOD_NONE:
			if (t->currentmaterialflags & MATERIALFLAG_WATER && r_waterscroll.value != 0)
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
			Matrix4x4_ConcatRotate(&matrix, tcmod->parms[0] * r_refdef.time, 0, 0, 1);
			Matrix4x4_ConcatTranslate(&matrix, -0.5, -0.5, 0);
			break;
		case Q3TCMOD_SCALE:
			Matrix4x4_CreateScale3(&matrix, tcmod->parms[0], tcmod->parms[1], 1);
			break;
		case Q3TCMOD_SCROLL:
			Matrix4x4_CreateTranslate(&matrix, tcmod->parms[0] * r_refdef.time, tcmod->parms[1] * r_refdef.time, 0);
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
		// either replace or concatenate the transformation
		if (i < 1)
			t->currenttexmatrix = matrix;
		else
		{
			matrix4x4_t temp = t->currenttexmatrix;
			Matrix4x4_Concat(&t->currenttexmatrix, &matrix, &temp);
		}
	}

	t->colormapping = VectorLength2(ent->colormap_pantscolor) + VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f);
	t->basetexture = (!t->colormapping && t->currentskinframe->merged) ? t->currentskinframe->merged : t->currentskinframe->base;
	t->glosstexture = r_texture_black;
	t->backgroundbasetexture = t->backgroundnumskinframes ? ((!t->colormapping && t->backgroundcurrentskinframe->merged) ? t->backgroundcurrentskinframe->merged : t->backgroundcurrentskinframe->base) : r_texture_white;
	t->backgroundglosstexture = r_texture_black;
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
		}
	}

	// lightmaps mode looks bad with dlights using actual texturing, so turn
	// off the colormap and glossmap, but leave the normalmap on as it still
	// accurately represents the shading involved
	if (gl_lightmaps.integer && !(t->currentmaterialflags & MATERIALFLAG_BLENDED))
	{
		t->basetexture = r_texture_white;
		t->specularscale = 0;
	}

	Vector4Set(t->lightmapcolor, ent->colormod[0], ent->colormod[1], ent->colormod[2], t->currentalpha);
	VectorClear(t->dlightcolor);
	t->currentnumlayers = 0;
	if (!(t->currentmaterialflags & MATERIALFLAG_NODRAW))
	{
		if (!(t->currentmaterialflags & MATERIALFLAG_SKY))
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
				if (r_refdef.fogenabled && (t->currentmaterialflags & MATERIALFLAG_BLENDED))
					layerflags |= TEXTURELAYERFLAG_FOGDARKEN;
				currentbasetexture = (VectorLength2(ent->colormap_pantscolor) + VectorLength2(ent->colormap_shirtcolor) < (1.0f / 1048576.0f) && t->currentskinframe->merged) ? t->currentskinframe->merged : t->currentskinframe->base;
				if (t->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
				{
					// fullbright is not affected by r_refdef.lightmapintensity
					R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_TEXTURE, currentbasetexture, &t->currenttexmatrix, t->lightmapcolor[0], t->lightmapcolor[1], t->lightmapcolor[2], t->lightmapcolor[3]);
					if (VectorLength2(ent->colormap_pantscolor) >= (1.0f / 1048576.0f) && t->currentskinframe->pants)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->currentskinframe->pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * t->lightmapcolor[0], ent->colormap_pantscolor[1] * t->lightmapcolor[1], ent->colormap_pantscolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
					if (VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->currentskinframe->shirt)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->currentskinframe->shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * t->lightmapcolor[0], ent->colormap_shirtcolor[1] * t->lightmapcolor[1], ent->colormap_shirtcolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
				}
				else
				{
					vec3_t ambientcolor;
					float colorscale;
					// set the color tint used for lights affecting this surface
					VectorSet(t->dlightcolor, ent->colormod[0] * t->lightmapcolor[3], ent->colormod[1] * t->lightmapcolor[3], ent->colormod[2] * t->lightmapcolor[3]);
					colorscale = 2;
					// q3bsp has no lightmap updates, so the lightstylevalue that
					// would normally be baked into the lightmap must be
					// applied to the color
					// FIXME: r_glsl 1 rendering doesn't support overbright lightstyles with this (the default light style is not overbright)
					if (ent->model->type == mod_brushq3)
						colorscale *= r_refdef.rtlightstylevalue[0];
					colorscale *= r_refdef.lightmapintensity;
					VectorScale(t->lightmapcolor, r_ambient.value * (1.0f / 64.0f), ambientcolor);
					VectorScale(t->lightmapcolor, colorscale, t->lightmapcolor);
					// basic lit geometry
					R_Texture_AddLayer(t, depthmask, blendfunc1, blendfunc2, TEXTURELAYERTYPE_LITTEXTURE, currentbasetexture, &t->currenttexmatrix, t->lightmapcolor[0], t->lightmapcolor[1], t->lightmapcolor[2], t->lightmapcolor[3]);
					// add pants/shirt if needed
					if (VectorLength2(ent->colormap_pantscolor) >= (1.0f / 1048576.0f) && t->currentskinframe->pants)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE, t->currentskinframe->pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * t->lightmapcolor[0], ent->colormap_pantscolor[1] * t->lightmapcolor[1], ent->colormap_pantscolor[2]  * t->lightmapcolor[2], t->lightmapcolor[3]);
					if (VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->currentskinframe->shirt)
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_LITTEXTURE, t->currentskinframe->shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * t->lightmapcolor[0], ent->colormap_shirtcolor[1] * t->lightmapcolor[1], ent->colormap_shirtcolor[2] * t->lightmapcolor[2], t->lightmapcolor[3]);
					// now add ambient passes if needed
					if (VectorLength2(ambientcolor) >= (1.0f/1048576.0f))
					{
						R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, currentbasetexture, &t->currenttexmatrix, ambientcolor[0], ambientcolor[1], ambientcolor[2], t->lightmapcolor[3]);
						if (VectorLength2(ent->colormap_pantscolor) >= (1.0f / 1048576.0f) && t->currentskinframe->pants)
							R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->currentskinframe->pants, &t->currenttexmatrix, ent->colormap_pantscolor[0] * ambientcolor[0], ent->colormap_pantscolor[1] * ambientcolor[1], ent->colormap_pantscolor[2] * ambientcolor[2], t->lightmapcolor[3]);
						if (VectorLength2(ent->colormap_shirtcolor) >= (1.0f / 1048576.0f) && t->currentskinframe->shirt)
							R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->currentskinframe->shirt, &t->currenttexmatrix, ent->colormap_shirtcolor[0] * ambientcolor[0], ent->colormap_shirtcolor[1] * ambientcolor[1], ent->colormap_shirtcolor[2] * ambientcolor[2], t->lightmapcolor[3]);
					}
				}
				if (t->currentskinframe->glow != NULL)
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, GL_ONE, TEXTURELAYERTYPE_TEXTURE, t->currentskinframe->glow, &t->currenttexmatrix, r_hdr_glowintensity.value, r_hdr_glowintensity.value, r_hdr_glowintensity.value, t->lightmapcolor[3]);
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
					R_Texture_AddLayer(t, false, GL_SRC_ALPHA, (t->currentmaterialflags & MATERIALFLAG_BLENDED) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, TEXTURELAYERTYPE_FOG, t->currentskinframe->fog, &identitymatrix, r_refdef.fogcolor[0] / r_refdef.view.colorscale, r_refdef.fogcolor[1] / r_refdef.view.colorscale, r_refdef.fogcolor[2] / r_refdef.view.colorscale, t->lightmapcolor[3]);
				}
			}
		}
	}
}

void R_UpdateAllTextureInfo(entity_render_t *ent)
{
	int i;
	if (ent->model)
		for (i = 0;i < ent->model->num_texturesperskin;i++)
			R_UpdateTextureInfo(ent, ent->model->data_textures + i);
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

void RSurf_CleanUp(void)
{
	CHECKGLERROR
	if (rsurface.mode == RSURFMODE_GLSL)
	{
		qglUseProgramObjectARB(0);CHECKGLERROR
	}
	GL_AlphaTest(false);
	rsurface.mode = RSURFMODE_NONE;
	rsurface.uselightmaptexture = false;
	rsurface.texture = NULL;
}

void RSurf_ActiveWorldEntity(void)
{
	model_t *model = r_refdef.worldmodel;
	RSurf_CleanUp();
	if (rsurface.array_size < model->surfmesh.num_vertices)
		R_Mesh_ResizeArrays(model->surfmesh.num_vertices);
	rsurface.matrix = identitymatrix;
	rsurface.inversematrix = identitymatrix;
	R_Mesh_Matrix(&identitymatrix);
	VectorCopy(r_refdef.view.origin, rsurface.modelorg);
	VectorSet(rsurface.modellight_ambient, 0, 0, 0);
	VectorSet(rsurface.modellight_diffuse, 0, 0, 0);
	VectorSet(rsurface.modellight_lightdir, 0, 0, 1);
	VectorSet(rsurface.colormap_pantscolor, 0, 0, 0);
	VectorSet(rsurface.colormap_shirtcolor, 0, 0, 0);
	rsurface.frameblend[0].frame = 0;
	rsurface.frameblend[0].lerp = 1;
	rsurface.frameblend[1].frame = 0;
	rsurface.frameblend[1].lerp = 0;
	rsurface.frameblend[2].frame = 0;
	rsurface.frameblend[2].lerp = 0;
	rsurface.frameblend[3].frame = 0;
	rsurface.frameblend[3].lerp = 0;
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
	rsurface.modelelement3i_bufferobject = model->surfmesh.ebo;
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

void RSurf_ActiveModelEntity(const entity_render_t *ent, qboolean wantnormals, qboolean wanttangents)
{
	model_t *model = ent->model;
	RSurf_CleanUp();
	if (rsurface.array_size < model->surfmesh.num_vertices)
		R_Mesh_ResizeArrays(model->surfmesh.num_vertices);
	rsurface.matrix = ent->matrix;
	rsurface.inversematrix = ent->inversematrix;
	R_Mesh_Matrix(&rsurface.matrix);
	Matrix4x4_Transform(&rsurface.inversematrix, r_refdef.view.origin, rsurface.modelorg);
	rsurface.modellight_ambient[0] = ent->modellight_ambient[0] * ent->colormod[0];
	rsurface.modellight_ambient[1] = ent->modellight_ambient[1] * ent->colormod[1];
	rsurface.modellight_ambient[2] = ent->modellight_ambient[2] * ent->colormod[2];
	rsurface.modellight_diffuse[0] = ent->modellight_diffuse[0] * ent->colormod[0];
	rsurface.modellight_diffuse[1] = ent->modellight_diffuse[1] * ent->colormod[1];
	rsurface.modellight_diffuse[2] = ent->modellight_diffuse[2] * ent->colormod[2];
	VectorCopy(ent->modellight_diffuse, rsurface.modellight_diffuse);
	VectorCopy(ent->modellight_lightdir, rsurface.modellight_lightdir);
	VectorCopy(ent->colormap_pantscolor, rsurface.colormap_pantscolor);
	VectorCopy(ent->colormap_shirtcolor, rsurface.colormap_shirtcolor);
	rsurface.frameblend[0] = ent->frameblend[0];
	rsurface.frameblend[1] = ent->frameblend[1];
	rsurface.frameblend[2] = ent->frameblend[2];
	rsurface.frameblend[3] = ent->frameblend[3];
	rsurface.basepolygonfactor = r_refdef.polygonfactor;
	rsurface.basepolygonoffset = r_refdef.polygonoffset;
	if (ent->model->brush.submodel)
	{
		rsurface.basepolygonfactor += r_polygonoffset_submodel_factor.value;
		rsurface.basepolygonoffset += r_polygonoffset_submodel_offset.value;
	}
	if (model->surfmesh.isanimated && (rsurface.frameblend[0].lerp != 1 || rsurface.frameblend[0].frame != 0))
	{
		if (wanttangents)
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = rsurface.array_modelsvector3f;
			rsurface.modeltvector3f = rsurface.array_modeltvector3f;
			rsurface.modelnormal3f = rsurface.array_modelnormal3f;
			Mod_Alias_GetMesh_Vertices(model, rsurface.frameblend, rsurface.array_modelvertex3f, rsurface.array_modelnormal3f, rsurface.array_modelsvector3f, rsurface.array_modeltvector3f);
		}
		else if (wantnormals)
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = rsurface.array_modelnormal3f;
			Mod_Alias_GetMesh_Vertices(model, rsurface.frameblend, rsurface.array_modelvertex3f, rsurface.array_modelnormal3f, NULL, NULL);
		}
		else
		{
			rsurface.modelvertex3f = rsurface.array_modelvertex3f;
			rsurface.modelsvector3f = NULL;
			rsurface.modeltvector3f = NULL;
			rsurface.modelnormal3f = NULL;
			Mod_Alias_GetMesh_Vertices(model, rsurface.frameblend, rsurface.array_modelvertex3f, NULL, NULL, NULL);
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
	rsurface.modelelement3i_bufferobject = model->surfmesh.ebo;
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

static const int quadedges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
void RSurf_PrepareVerticesForBatch(qboolean generatenormals, qboolean generatetangents, int texturenumsurfaces, msurface_t **texturesurfacelist)
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
			Mod_BuildNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modelelement3i, rsurface.array_modelnormal3f, r_smoothnormals_areaweighting.integer);
		}
		if (generatetangents && !rsurface.modelsvector3f)
		{
			rsurface.svector3f = rsurface.modelsvector3f = rsurface.array_modelsvector3f;
			rsurface.svector3f_bufferobject = rsurface.modelsvector3f_bufferobject = 0;
			rsurface.svector3f_bufferoffset = rsurface.modelsvector3f_bufferoffset = 0;
			rsurface.tvector3f = rsurface.modeltvector3f = rsurface.array_modeltvector3f;
			rsurface.tvector3f_bufferobject = rsurface.modeltvector3f_bufferobject = 0;
			rsurface.tvector3f_bufferoffset = rsurface.modeltvector3f_bufferoffset = 0;
			Mod_BuildTextureVectorsFromNormals(0, rsurface.modelnum_vertices, rsurface.modelnum_triangles, rsurface.modelvertex3f, rsurface.modeltexcoordtexture2f, rsurface.modelnormal3f, rsurface.modelelement3i, rsurface.array_modelsvector3f, rsurface.array_modeltvector3f, r_smoothnormals_areaweighting.integer);
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
				Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformednormal3f, r_smoothnormals_areaweighting.integer);
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer);
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
						Debug_PolygonBegin(NULL, 0, false, 0);
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
					Debug_PolygonBegin(NULL, 0, false, 0);
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
					//VectorSubtract(rsurface.modelorg, center, forward);
					Matrix4x4_Transform3x3(&rsurface.inversematrix, r_refdef.view.forward, forward);
					VectorNegate(forward, forward);
					VectorReflect(forward, 0, up, forward);
					VectorNormalize(forward);
					CrossProduct(up, forward, newright);
					VectorNormalize(newright);
#if 0
					Debug_PolygonBegin(NULL, 0, false, 0);
					Debug_PolygonVertex(center[0] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+0] * 8, center[1] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+1] * 8, center[2] + rsurface.normal3f[3 * (surface->num_firstvertex + j)+2] * 8, 0, 0, 1, 0, 0, 1);
					Debug_PolygonVertex(center[0] + right[0] * 8, center[1] + right[1] * 8, center[2] + right[2] * 8, 0, 0, 0, 1, 0, 1);
					Debug_PolygonVertex(center[0] + up   [0] * 8, center[1] + up   [1] * 8, center[2] + up   [2] * 8, 0, 0, 0, 0, 1, 1);
					Debug_PolygonEnd();
#endif
#if 0
					Debug_PolygonBegin(NULL, 0, false, 0);
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
				Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformednormal3f, r_smoothnormals_areaweighting.integer);
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer);
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
					normal[0] += deform->parms[0] * noise4f(      vertex[0], vertex[1], vertex[2], r_refdef.time * deform->parms[1]);
					normal[1] += deform->parms[0] * noise4f( 98 + vertex[0], vertex[1], vertex[2], r_refdef.time * deform->parms[1]);
					normal[2] += deform->parms[0] * noise4f(196 + vertex[0], vertex[1], vertex[2], r_refdef.time * deform->parms[1]);
					VectorNormalize(normal);
				}
				Mod_BuildTextureVectorsFromNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface.vertex3f, rsurface.modeltexcoordtexture2f, rsurface.array_deformednormal3f, rsurface.modelelement3i + surface->num_firsttriangle * 3, rsurface.array_deformedsvector3f, rsurface.array_deformedtvector3f, r_smoothnormals_areaweighting.integer);
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
					scale = sin((rsurface.modeltexcoordtexture2f[2 * (surface->num_firstvertex + j)] * deform->parms[0] + r_refdef.time * deform->parms[2])) * deform->parms[1];
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
				float l, d, eyedir[3];
				VectorSubtract(rsurface.modelorg, vertex, eyedir);
				l = 0.5f / VectorLength(eyedir);
				d = DotProduct(normal, eyedir)*2;
				out_tc[0] = 0.5f + (normal[1]*d - eyedir[1])*l;
				out_tc[1] = 0.5f - (normal[2]*d - eyedir[2])*l;
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
		animpos = rsurface.texture->tcmods[0].parms[2] + r_refdef.time * rsurface.texture->tcmods[0].parms[3];
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

void RSurf_DrawBatch_Simple(int texturenumsurfaces, msurface_t **texturesurfacelist)
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
	{
		GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
	}
	else if (r_batchmode.integer == 2)
	{
		#define MAXBATCHTRIANGLES 4096
		int batchtriangles = 0;
		int batchelements[MAXBATCHTRIANGLES*3];
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			j = i + 1;
			if (surface->num_triangles > MAXBATCHTRIANGLES)
			{
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
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
			R_Mesh_Draw(firstvertex, numvertices, batchtriangles, batchelements, 0, 0);
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
			GL_LockArrays(surface->num_firstvertex, numvertices);
			R_Mesh_Draw(surface->num_firstvertex, numvertices, numtriangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
		}
	}
	else
	{
		for (i = 0;i < texturenumsurfaces;i++)
		{
			surface = texturesurfacelist[i];
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
		}
	}
}

static void RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(int texturenumsurfaces, msurface_t **texturesurfacelist, int lightmaptexunit, int deluxemaptexunit, int refractiontexunit, int reflectiontexunit)
{
	int i, planeindex, vertexindex;
	float d, bestd;
	vec3_t vert;
	const float *v;
	r_waterstate_waterplane_t *p, *bestp;
	msurface_t *surface;
	if (r_waterstate.renderingscene)
		return;
	for (i = 0;i < texturenumsurfaces;i++)
	{
		surface = texturesurfacelist[i];
		if (lightmaptexunit >= 0)
			R_Mesh_TexBind(lightmaptexunit, R_GetTexture(surface->lightmaptexture));
		if (deluxemaptexunit >= 0)
			R_Mesh_TexBind(deluxemaptexunit, R_GetTexture(surface->deluxemaptexture));
		// pick the closest matching water plane
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
		if (bestp)
		{
			if (refractiontexunit >= 0)
				R_Mesh_TexBind(refractiontexunit, R_GetTexture(bestp->texture_refraction));
			if (reflectiontexunit >= 0)
				R_Mesh_TexBind(reflectiontexunit, R_GetTexture(bestp->texture_reflection));
		}
		else
		{
			if (refractiontexunit >= 0)
				R_Mesh_TexBind(refractiontexunit, R_GetTexture(r_texture_black));
			if (reflectiontexunit >= 0)
				R_Mesh_TexBind(reflectiontexunit, R_GetTexture(r_texture_black));
		}
		GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
	}
}

static void RSurf_DrawBatch_WithLightmapSwitching(int texturenumsurfaces, msurface_t **texturesurfacelist, int lightmaptexunit, int deluxemaptexunit)
{
	int i;
	int j;
	const msurface_t *surface = texturesurfacelist[0];
	const msurface_t *surface2;
	int firstvertex;
	int endvertex;
	int numvertices;
	int numtriangles;
	// TODO: lock all array ranges before render, rather than on each surface
	if (texturenumsurfaces == 1)
	{
		R_Mesh_TexBind(lightmaptexunit, R_GetTexture(surface->lightmaptexture));
		if (deluxemaptexunit >= 0)
			R_Mesh_TexBind(deluxemaptexunit, R_GetTexture(surface->deluxemaptexture));
		GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
		R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
	}
	else if (r_batchmode.integer == 2)
	{
		#define MAXBATCHTRIANGLES 4096
		int batchtriangles = 0;
		int batchelements[MAXBATCHTRIANGLES*3];
		for (i = 0;i < texturenumsurfaces;i = j)
		{
			surface = texturesurfacelist[i];
			R_Mesh_TexBind(lightmaptexunit, R_GetTexture(surface->lightmaptexture));
			if (deluxemaptexunit >= 0)
				R_Mesh_TexBind(deluxemaptexunit, R_GetTexture(surface->deluxemaptexture));
			j = i + 1;
			if (surface->num_triangles > MAXBATCHTRIANGLES)
			{
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
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
			R_Mesh_Draw(firstvertex, numvertices, batchtriangles, batchelements, 0, 0);
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
			R_Mesh_TexBind(lightmaptexunit, R_GetTexture(surface->lightmaptexture));
			if (deluxemaptexunit >= 0)
				R_Mesh_TexBind(deluxemaptexunit, R_GetTexture(surface->deluxemaptexture));
			for (j = i + 1, surface2 = surface + 1;j < texturenumsurfaces;j++, surface2++)
				if (texturesurfacelist[j] != surface2 || texturesurfacelist[j]->lightmaptexture != surface->lightmaptexture)
					break;
#if 0
			Con_Printf(" %i", j - i);
#endif
			surface2 = texturesurfacelist[j-1];
			numvertices = surface2->num_firstvertex + surface2->num_vertices - surface->num_firstvertex;
			numtriangles = surface2->num_firsttriangle + surface2->num_triangles - surface->num_firsttriangle;
			GL_LockArrays(surface->num_firstvertex, numvertices);
			R_Mesh_Draw(surface->num_firstvertex, numvertices, numtriangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
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
			R_Mesh_TexBind(lightmaptexunit, R_GetTexture(surface->lightmaptexture));
			if (deluxemaptexunit >= 0)
				R_Mesh_TexBind(deluxemaptexunit, R_GetTexture(surface->deluxemaptexture));
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
		}
	}
}

static void RSurf_DrawBatch_ShowSurfaces(int texturenumsurfaces, msurface_t **texturesurfacelist)
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
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, 1, (rsurface.modelelement3i + 3 * (j + surface->num_firsttriangle)), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * (j + surface->num_firsttriangle)));
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
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (rsurface.modelelement3i + 3 * surface->num_firsttriangle), rsurface.modelelement3i_bufferobject, (sizeof(int[3]) * surface->num_firsttriangle));
		}
	}
}

static void RSurf_DrawBatch_GL11_ApplyFog(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	int texturesurfaceindex;
	int i;
	float f;
	float *v, *c, *c2;
	if (rsurface.lightmapcolor4f)
	{
		// generate color arrays for the surfaces in this list
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4, c2 += 4)
			{
				f = FogPoint_Model(v);
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
				f = FogPoint_Model(v);
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

static void RSurf_DrawBatch_GL11_ApplyColor(int texturenumsurfaces, msurface_t **texturesurfacelist, float r, float g, float b, float a)
{
	int texturesurfaceindex;
	int i;
	float *c, *c2;
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

static void RSurf_DrawBatch_GL11_Lightmap(int texturenumsurfaces, msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	// TODO: optimize
	rsurface.lightmapcolor4f = NULL;
	rsurface.lightmapcolor4f_bufferobject = 0;
	rsurface.lightmapcolor4f_bufferoffset = 0;
	if (applyfog)   RSurf_DrawBatch_GL11_ApplyFog(texturenumsurfaces, texturesurfacelist);
	if (applycolor) RSurf_DrawBatch_GL11_ApplyColor(texturenumsurfaces, texturesurfacelist, r, g, b, a);
	R_Mesh_ColorPointer(rsurface.lightmapcolor4f, rsurface.lightmapcolor4f_bufferobject, rsurface.lightmapcolor4f_bufferoffset);
	GL_Color(r, g, b, a);
	RSurf_DrawBatch_WithLightmapSwitching(texturenumsurfaces, texturesurfacelist, 0, -1);
}

static void RSurf_DrawBatch_GL11_Unlit(int texturenumsurfaces, msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
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

static void RSurf_DrawBatch_GL11_VertexColor(int texturenumsurfaces, msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	int texturesurfaceindex;
	int i;
	float *c;
	// TODO: optimize
	if (texturesurfacelist[0]->lightmapinfo && texturesurfacelist[0]->lightmapinfo->stainsamples)
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

static void RSurf_DrawBatch_GL11_VertexShade(int texturenumsurfaces, msurface_t **texturesurfacelist, float r, float g, float b, float a, qboolean applycolor, qboolean applyfog)
{
	int texturesurfaceindex;
	int i;
	float f;
	float *v, *c, *c2;
	vec3_t ambientcolor;
	vec3_t diffusecolor;
	vec3_t lightdir;
	// TODO: optimize
	// model lighting
	VectorCopy(rsurface.modellight_lightdir, lightdir);
	f = 0.5f * r_refdef.lightmapintensity;
	ambientcolor[0] = rsurface.modellight_ambient[0] * r * f;
	ambientcolor[1] = rsurface.modellight_ambient[1] * g * f;
	ambientcolor[2] = rsurface.modellight_ambient[2] * b * f;
	diffusecolor[0] = rsurface.modellight_diffuse[0] * r * f;
	diffusecolor[1] = rsurface.modellight_diffuse[1] * g * f;
	diffusecolor[2] = rsurface.modellight_diffuse[2] * b * f;
	if (VectorLength2(diffusecolor) > 0)
	{
		// generate color arrays for the surfaces in this list
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
			int numverts = surface->num_vertices;
			v = rsurface.vertex3f + 3 * surface->num_firstvertex;
			c2 = rsurface.normal3f + 3 * surface->num_firstvertex;
			c = rsurface.array_color4f + 4 * surface->num_firstvertex;
			// q3-style directional shading
			for (i = 0;i < numverts;i++, v += 3, c2 += 3, c += 4)
			{
				if ((f = DotProduct(c2, lightdir)) > 0)
					VectorMA(ambientcolor, f, diffusecolor, c);
				else
					VectorCopy(ambientcolor, c);
				c[3] = a;
			}
		}
		r = 1;
		g = 1;
		b = 1;
		a = 1;
		applycolor = false;
		rsurface.lightmapcolor4f = rsurface.array_color4f;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
	}
	else
	{
		r = ambientcolor[0];
		g = ambientcolor[1];
		b = ambientcolor[2];
		rsurface.lightmapcolor4f = NULL;
		rsurface.lightmapcolor4f_bufferobject = 0;
		rsurface.lightmapcolor4f_bufferoffset = 0;
	}
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

static void R_DrawTextureSurfaceList_ShowSurfaces(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	RSurf_SetupDepthAndCulling();
	if (rsurface.mode != RSURFMODE_SHOWSURFACES)
	{
		rsurface.mode = RSURFMODE_SHOWSURFACES;
		GL_DepthMask(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
	}
	RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
	RSurf_DrawBatch_ShowSurfaces(texturenumsurfaces, texturesurfacelist);
}

static void R_DrawTextureSurfaceList_Sky(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	// transparent sky would be ridiculous
	if ((rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED))
		return;
	if (rsurface.mode != RSURFMODE_SKY)
	{
		if (rsurface.mode == RSURFMODE_GLSL)
		{
			qglUseProgramObjectARB(0);CHECKGLERROR
		}
		rsurface.mode = RSURFMODE_SKY;
	}
	if (skyrendernow)
	{
		skyrendernow = false;
		R_Sky();
		// restore entity matrix
		R_Mesh_Matrix(&rsurface.matrix);
	}
	RSurf_SetupDepthAndCulling();
	GL_DepthMask(true);
	// LordHavoc: HalfLife maps have freaky skypolys so don't use
	// skymasking on them, and Quake3 never did sky masking (unlike
	// software Quake and software Quake2), so disable the sky masking
	// in Quake3 maps as it causes problems with q3map2 sky tricks,
	// and skymasking also looks very bad when noclipping outside the
	// level, so don't use it then either.
	if (r_refdef.worldmodel && r_refdef.worldmodel->type == mod_brushq1 && r_q1bsp_skymasking.integer && !r_refdef.viewcache.world_novis)
	{
		GL_Color(r_refdef.fogcolor[0], r_refdef.fogcolor[1], r_refdef.fogcolor[2], 1);
		R_Mesh_ColorPointer(NULL, 0, 0);
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
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
		if (skyrendermasked)
			GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	}
}

static void R_DrawTextureSurfaceList_GL20(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION)))
		return;

	if (rsurface.mode != RSURFMODE_GLSL)
	{
		rsurface.mode = RSURFMODE_GLSL;
		R_Mesh_ResetTextureState();
		GL_Color(1, 1, 1, 1);
	}

	R_Mesh_TexMatrix(0, &rsurface.texture->currenttexmatrix);
	R_Mesh_TexBind(0, R_GetTexture(rsurface.texture->currentskinframe->nmap));
	R_Mesh_TexBind(1, R_GetTexture(rsurface.texture->basetexture));
	R_Mesh_TexBind(2, R_GetTexture(rsurface.texture->glosstexture));
	R_Mesh_TexBind(4, R_GetTexture(r_texture_fogattenuation));
	R_Mesh_TexBind(5, R_GetTexture(rsurface.texture->currentskinframe->pants));
	R_Mesh_TexBind(6, R_GetTexture(rsurface.texture->currentskinframe->shirt));
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
	{
		R_Mesh_TexBind(7, R_GetTexture(r_texture_grey128));
		R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
		R_Mesh_ColorPointer(NULL, 0, 0);
	}
	else if (rsurface.uselightmaptexture)
	{
		R_Mesh_TexBind(7, R_GetTexture(texturesurfacelist[0]->lightmaptexture));
		R_Mesh_TexBind(8, R_GetTexture(texturesurfacelist[0]->deluxemaptexture));
		R_Mesh_ColorPointer(NULL, 0, 0);
	}
	else
	{
		R_Mesh_TexBind(7, R_GetTexture(r_texture_white));
		R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
		R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
	}
	R_Mesh_TexBind(9, R_GetTexture(rsurface.texture->currentskinframe->glow));
	R_Mesh_TexBind(11, R_GetTexture(r_texture_white)); // changed per surface
	R_Mesh_TexBind(12, R_GetTexture(r_texture_white)); // changed per surface

	if (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION))
	{
		// render background
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_AlphaTest(false);

		GL_Color(1, 1, 1, 1);
		R_Mesh_ColorPointer(NULL, 0, 0);

		R_SetupSurfaceShader(vec3_origin, rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT, 1, 1, rsurface.texture->specularscale, RSURFPASS_BACKGROUND);
		if (r_glsl_permutation)
		{
			RSurf_PrepareVerticesForBatch(true, true, texturenumsurfaces, texturesurfacelist);
			R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
			R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
			R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
			R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
			R_Mesh_TexCoordPointer(4, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);
			RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(texturenumsurfaces, texturesurfacelist, -1, -1, r_glsl_permutation->loc_Texture_Refraction ? 11 : -1, r_glsl_permutation->loc_Texture_Reflection ? 12 : -1);
		}

		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		GL_DepthMask(false);
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT)
		{
			R_Mesh_TexBind(7, R_GetTexture(r_texture_grey128));
			R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
			R_Mesh_ColorPointer(NULL, 0, 0);
		}
		else if (rsurface.uselightmaptexture)
		{
			R_Mesh_TexBind(7, R_GetTexture(texturesurfacelist[0]->lightmaptexture));
			R_Mesh_TexBind(8, R_GetTexture(texturesurfacelist[0]->deluxemaptexture));
			R_Mesh_ColorPointer(NULL, 0, 0);
		}
		else
		{
			R_Mesh_TexBind(7, R_GetTexture(r_texture_white));
			R_Mesh_TexBind(8, R_GetTexture(r_texture_blanknormalmap));
			R_Mesh_ColorPointer(rsurface.modellightmapcolor4f, rsurface.modellightmapcolor4f_bufferobject, rsurface.modellightmapcolor4f_bufferoffset);
		}
		R_Mesh_TexBind(11, R_GetTexture(r_texture_white)); // changed per surface
		R_Mesh_TexBind(12, R_GetTexture(r_texture_white)); // changed per surface
	}

	R_SetupSurfaceShader(vec3_origin, rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT, 1, 1, rsurface.texture->specularscale, RSURFPASS_BASE);
	if (!r_glsl_permutation)
		return;

	RSurf_PrepareVerticesForBatch(r_glsl_permutation->loc_Texture_Normal >= 0 || r_glsl_permutation->loc_LightDir >= 0, r_glsl_permutation->loc_Texture_Normal >= 0, texturenumsurfaces, texturesurfacelist);
	R_Mesh_TexCoordPointer(0, 2, rsurface.texcoordtexture2f, rsurface.texcoordtexture2f_bufferobject, rsurface.texcoordtexture2f_bufferoffset);
	R_Mesh_TexCoordPointer(1, 3, rsurface.svector3f, rsurface.svector3f_bufferobject, rsurface.svector3f_bufferoffset);
	R_Mesh_TexCoordPointer(2, 3, rsurface.tvector3f, rsurface.tvector3f_bufferobject, rsurface.tvector3f_bufferoffset);
	R_Mesh_TexCoordPointer(3, 3, rsurface.normal3f, rsurface.normal3f_bufferobject, rsurface.normal3f_bufferoffset);
	R_Mesh_TexCoordPointer(4, 2, rsurface.modeltexcoordlightmap2f, rsurface.modeltexcoordlightmap2f_bufferobject, rsurface.modeltexcoordlightmap2f_bufferoffset);

	if (r_glsl_permutation->loc_Texture_Refraction >= 0)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_AlphaTest(false);
	}

	if (rsurface.uselightmaptexture && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT))
	{
		if (r_glsl_permutation->loc_Texture_Refraction >= 0 || r_glsl_permutation->loc_Texture_Reflection >= 0)
			RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(texturenumsurfaces, texturesurfacelist, 7, r_glsl_permutation->loc_Texture_Deluxemap >= 0 ? 8 : -1, r_glsl_permutation->loc_Texture_Refraction >= 0 ? 11 : -1, r_glsl_permutation->loc_Texture_Reflection >= 0 ? 12 : -1);
		else
			RSurf_DrawBatch_WithLightmapSwitching(texturenumsurfaces, texturesurfacelist, 7, r_glsl_permutation->loc_Texture_Deluxemap >= 0 ? 8 : -1);
	}
	else
	{
		if (r_glsl_permutation->loc_Texture_Refraction >= 0 || r_glsl_permutation->loc_Texture_Reflection >= 0)
			RSurf_DrawBatch_WithLightmapSwitching_WithWaterTextureSwitching(texturenumsurfaces, texturesurfacelist, -1, -1, r_glsl_permutation->loc_Texture_Refraction >= 0 ? 11 : -1, r_glsl_permutation->loc_Texture_Reflection >= 0 ? 12 : -1);
		else
			RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	if (rsurface.texture->backgroundnumskinframes && !(rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED))
	{
	}
}

static void R_DrawTextureSurfaceList_GL13(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	// OpenGL 1.3 path - anything not completely ancient
	int texturesurfaceindex;
	qboolean applycolor;
	qboolean applyfog;
	rmeshstate_t m;
	int layerindex;
	const texturelayer_t *layer;
	if (rsurface.mode != RSURFMODE_MULTIPASS)
		rsurface.mode = RSURFMODE_MULTIPASS;
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
		GL_DepthMask(layer->depthmask);
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
		applyfog = (layer->flags & TEXTURELAYERFLAG_FOGDARKEN) != 0;
		switch (layer->type)
		{
		case TEXTURELAYERTYPE_LITTEXTURE:
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(r_texture_white);
			m.pointer_texcoord[0] = rsurface.modeltexcoordlightmap2f;
			m.pointer_texcoord_bufferobject[0] = rsurface.modeltexcoordlightmap2f_bufferobject;
			m.pointer_texcoord_bufferoffset[0] = rsurface.modeltexcoordlightmap2f_bufferoffset;
			m.tex[1] = R_GetTexture(layer->texture);
			m.texmatrix[1] = layer->texmatrix;
			m.texrgbscale[1] = layertexrgbscale;
			m.pointer_texcoord[1] = rsurface.texcoordtexture2f;
			m.pointer_texcoord_bufferobject[1] = rsurface.texcoordtexture2f_bufferobject;
			m.pointer_texcoord_bufferoffset[1] = rsurface.texcoordtexture2f_bufferoffset;
			R_Mesh_TextureState(&m);
			if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
				RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else if (rsurface.uselightmaptexture)
				RSurf_DrawBatch_GL11_Lightmap(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			else
				RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(layer->texture);
			m.texmatrix[0] = layer->texmatrix;
			m.texrgbscale[0] = layertexrgbscale;
			m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
			m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
			m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
			R_Mesh_TextureState(&m);
			RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layercolor[0], layercolor[1], layercolor[2], layercolor[3], applycolor, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			memset(&m, 0, sizeof(m));
			m.texrgbscale[0] = layertexrgbscale;
			if (layer->texture)
			{
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
				m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
				m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
			}
			R_Mesh_TextureState(&m);
			// generate a color array for the fog pass
			R_Mesh_ColorPointer(rsurface.array_color4f, 0, 0);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				int i;
				float f, *v, *c;
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
				{
					f = 1 - FogPoint_Model(v);
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
		GL_LockArrays(0, 0);
	}
	CHECKGLERROR
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_LEQUAL);CHECKGLERROR
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList_GL11(int texturenumsurfaces, msurface_t **texturesurfacelist)
{
	// OpenGL 1.1 - crusty old voodoo path
	int texturesurfaceindex;
	qboolean applyfog;
	rmeshstate_t m;
	int layerindex;
	const texturelayer_t *layer;
	if (rsurface.mode != RSURFMODE_MULTIPASS)
		rsurface.mode = RSURFMODE_MULTIPASS;
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
		GL_DepthMask(layer->depthmask);
		GL_BlendFunc(layer->blendfunc1, layer->blendfunc2);
		R_Mesh_ColorPointer(NULL, 0, 0);
		applyfog = (layer->flags & TEXTURELAYERFLAG_FOGDARKEN) != 0;
		switch (layer->type)
		{
		case TEXTURELAYERTYPE_LITTEXTURE:
			if (layer->blendfunc1 == GL_ONE && layer->blendfunc2 == GL_ZERO)
			{
				// two-pass lit texture with 2x rgbscale
				// first the lightmap pass
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(r_texture_white);
				m.pointer_texcoord[0] = rsurface.modeltexcoordlightmap2f;
				m.pointer_texcoord_bufferobject[0] = rsurface.modeltexcoordlightmap2f_bufferobject;
				m.pointer_texcoord_bufferoffset[0] = rsurface.modeltexcoordlightmap2f_bufferoffset;
				R_Mesh_TextureState(&m);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				else if (rsurface.uselightmaptexture)
					RSurf_DrawBatch_GL11_Lightmap(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				else
					RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
				GL_LockArrays(0, 0);
				// then apply the texture to it
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
				m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
				m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
				R_Mesh_TextureState(&m);
				RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layer->color[0] * 0.5f, layer->color[1] * 0.5f, layer->color[2] * 0.5f, layer->color[3], layer->color[0] != 2 || layer->color[1] != 2 || layer->color[2] != 2 || layer->color[3] != 1, false);
			}
			else
			{
				// single pass vertex-lighting-only texture with 1x rgbscale and transparency support
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
				m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
				m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
				R_Mesh_TextureState(&m);
				if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
					RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
				else
					RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			}
			break;
		case TEXTURELAYERTYPE_TEXTURE:
			// singletexture unlit texture with transparency support
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(layer->texture);
			m.texmatrix[0] = layer->texmatrix;
			m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
			m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
			m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
			R_Mesh_TextureState(&m);
			RSurf_DrawBatch_GL11_Unlit(texturenumsurfaces, texturesurfacelist, layer->color[0], layer->color[1], layer->color[2], layer->color[3], layer->color[0] != 1 || layer->color[1] != 1 || layer->color[2] != 1 || layer->color[3] != 1, applyfog);
			break;
		case TEXTURELAYERTYPE_FOG:
			// singletexture fogging
			R_Mesh_ColorPointer(rsurface.array_color4f, 0, 0);
			if (layer->texture)
			{
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(layer->texture);
				m.texmatrix[0] = layer->texmatrix;
				m.pointer_texcoord[0] = rsurface.texcoordtexture2f;
				m.pointer_texcoord_bufferobject[0] = rsurface.texcoordtexture2f_bufferobject;
				m.pointer_texcoord_bufferoffset[0] = rsurface.texcoordtexture2f_bufferoffset;
				R_Mesh_TextureState(&m);
			}
			else
				R_Mesh_ResetTextureState();
			// generate a color array for the fog pass
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				int i;
				float f, *v, *c;
				const msurface_t *surface = texturesurfacelist[texturesurfaceindex];
				for (i = 0, v = (rsurface.vertex3f + 3 * surface->num_firstvertex), c = (rsurface.array_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
				{
					f = 1 - FogPoint_Model(v);
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
		GL_LockArrays(0, 0);
	}
	CHECKGLERROR
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
	{
		qglDepthFunc(GL_LEQUAL);CHECKGLERROR
		GL_AlphaTest(false);
	}
}

static void R_DrawTextureSurfaceList(int texturenumsurfaces, msurface_t **texturesurfacelist, qboolean writedepth, qboolean depthonly)
{
	if (rsurface.texture->currentmaterialflags & MATERIALFLAG_NODRAW)
		return;
	rsurface.rtlight = NULL;
	CHECKGLERROR
	if (depthonly)
	{
		if ((rsurface.texture->currentmaterialflags & (MATERIALFLAG_NODEPTHTEST | MATERIALFLAG_BLENDED | MATERIALFLAG_ALPHATEST)))
			return;
		if (r_waterstate.renderingscene && (rsurface.texture->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFLECTION)))
			return;
		if (rsurface.mode != RSURFMODE_MULTIPASS)
			rsurface.mode = RSURFMODE_MULTIPASS;
		if (r_depthfirst.integer == 3)
		{
			int i = (int)(texturesurfacelist[0] - rsurface.modelsurfaces);
			if (!r_refdef.view.showdebug)
				GL_Color(0, 0, 0, 1);
			else
				GL_Color(((i >> 6) & 7) / 7.0f, ((i >> 3) & 7) / 7.0f, (i & 7) / 7.0f,1);
		}
		else
		{
			GL_ColorMask(0,0,0,0);
			GL_Color(1,1,1,1);
		}
		RSurf_SetupDepthAndCulling();
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
		GL_ColorMask(r_refdef.view.colormask[0], r_refdef.view.colormask[1], r_refdef.view.colormask[2], 1);
	}
	else if (r_depthfirst.integer == 3)
		return;
	else if (!r_refdef.view.showdebug && (r_showsurfaces.integer || gl_lightmaps.integer))
	{
		GL_Color(0, 0, 0, 1);
		RSurf_DrawBatch_Simple(texturenumsurfaces, texturesurfacelist);
	}
	else if (r_showsurfaces.integer)
	{
		if (rsurface.mode != RSURFMODE_MULTIPASS)
			rsurface.mode = RSURFMODE_MULTIPASS;
		RSurf_SetupDepthAndCulling();
		GL_DepthTest(true);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(writedepth);
		GL_Color(1,1,1,1);
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		R_Mesh_ResetTextureState();
		RSurf_PrepareVerticesForBatch(false, false, texturenumsurfaces, texturesurfacelist);
		R_DrawTextureSurfaceList_ShowSurfaces(texturenumsurfaces, texturesurfacelist);
	}
	else if (gl_lightmaps.integer)
	{
		rmeshstate_t m;
		if (rsurface.mode != RSURFMODE_MULTIPASS)
			rsurface.mode = RSURFMODE_MULTIPASS;
		GL_DepthRange(0, (rsurface.texture->currentmaterialflags & MATERIALFLAG_SHORTDEPTHRANGE) ? 0.0625 : 1);
		GL_DepthTest(true);
		GL_CullFace((rsurface.texture->currentmaterialflags & MATERIALFLAG_NOCULLFACE) ? GL_NONE : r_refdef.view.cullface_back);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(writedepth);
		GL_Color(1,1,1,1);
		GL_AlphaTest(false);
		R_Mesh_ColorPointer(NULL, 0, 0);
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(r_texture_white);
		m.pointer_texcoord[0] = rsurface.modeltexcoordlightmap2f;
		m.pointer_texcoord_bufferobject[0] = rsurface.modeltexcoordlightmap2f_bufferobject;
		m.pointer_texcoord_bufferoffset[0] = rsurface.modeltexcoordlightmap2f_bufferoffset;
		R_Mesh_TextureState(&m);
		RSurf_PrepareVerticesForBatch(rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT, false, texturenumsurfaces, texturesurfacelist);
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_MODELLIGHT)
			RSurf_DrawBatch_GL11_VertexShade(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
		else if (rsurface.uselightmaptexture)
			RSurf_DrawBatch_GL11_Lightmap(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
		else
			RSurf_DrawBatch_GL11_VertexColor(texturenumsurfaces, texturesurfacelist, 1, 1, 1, 1, false, false);
	}
	else if (rsurface.texture->currentmaterialflags & MATERIALFLAG_SKY)
		R_DrawTextureSurfaceList_Sky(texturenumsurfaces, texturesurfacelist);
	else if (rsurface.texture->currentnumlayers)
	{
		// write depth for anything we skipped on the depth-only pass earlier
		if (rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST)
			writedepth = true;
		RSurf_SetupDepthAndCulling();
		GL_BlendFunc(rsurface.texture->currentlayers[0].blendfunc1, rsurface.texture->currentlayers[0].blendfunc2);
		GL_DepthMask(writedepth && !(rsurface.texture->currentmaterialflags & MATERIALFLAG_BLENDED));
		GL_AlphaTest((rsurface.texture->currentmaterialflags & MATERIALFLAG_ALPHATEST) != 0);
		if (r_glsl.integer && gl_support_fragment_shader)
			R_DrawTextureSurfaceList_GL20(texturenumsurfaces, texturesurfacelist);
		else if (gl_combine.integer && r_textureunits.integer >= 2)
			R_DrawTextureSurfaceList_GL13(texturenumsurfaces, texturesurfacelist);
		else
			R_DrawTextureSurfaceList_GL11(texturenumsurfaces, texturesurfacelist);
	}
	CHECKGLERROR
	GL_LockArrays(0, 0);
}

static void R_DrawSurface_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int i, j;
	int texturenumsurfaces, endsurface;
	texture_t *texture;
	msurface_t *surface;
	msurface_t *texturesurfacelist[1024];

	// if the model is static it doesn't matter what value we give for
	// wantnormals and wanttangents, so this logic uses only rules applicable
	// to a model, knowing that they are meaningless otherwise
	if (ent == r_refdef.worldentity)
		RSurf_ActiveWorldEntity();
	else if ((ent->effects & EF_FULLBRIGHT) || r_showsurfaces.integer || VectorLength2(ent->modellight_diffuse) < (1.0f / 256.0f))
		RSurf_ActiveModelEntity(ent, false, false);
	else
		RSurf_ActiveModelEntity(ent, true, r_glsl.integer && gl_support_fragment_shader);

	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		surface = rsurface.modelsurfaces + surfacelist[i];
		texture = surface->texture;
		R_UpdateTextureInfo(ent, texture);
		rsurface.texture = texture->currentframe;
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
		R_DrawTextureSurfaceList(texturenumsurfaces, texturesurfacelist, true, false);
	}

	RSurf_CleanUp();
}

void R_QueueSurfaceList(entity_render_t *ent, int numsurfaces, msurface_t **surfacelist, int flagsmask, qboolean writedepth, qboolean depthonly, qboolean addwaterplanes)
{
	int i, j;
	vec3_t tempcenter, center;
	texture_t *texture;
	// if we're rendering water textures (extra scene renders), use a separate loop to avoid burdening the main one
	if (addwaterplanes)
	{
		for (i = 0;i < numsurfaces;i++)
			if (surfacelist[i]->texture->currentframe->currentmaterialflags & (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION))
				R_Water_AddWaterPlane(surfacelist[i]);
		return;
	}
	// break the surface list down into batches by texture and use of lightmapping
	for (i = 0;i < numsurfaces;i = j)
	{
		j = i + 1;
		// texture is the base texture pointer, rsurface.texture is the
		// current frame/skin the texture is directing us to use (for example
		// if a model has 2 skins and it is on skin 1, then skin 0 tells us to
		// use skin 1 instead)
		texture = surfacelist[i]->texture;
		rsurface.texture = texture->currentframe;
		rsurface.uselightmaptexture = surfacelist[i]->lightmaptexture != NULL;
		if (!(rsurface.texture->currentmaterialflags & flagsmask))
		{
			// if this texture is not the kind we want, skip ahead to the next one
			for (;j < numsurfaces && texture == surfacelist[j]->texture;j++)
				;
			continue;
		}
		if (rsurface.texture->currentmaterialflags & MATERIALFLAGMASK_DEPTHSORTED)
		{
			// transparent surfaces get pushed off into the transparent queue
			const msurface_t *surface = surfacelist[i];
			if (depthonly)
				continue;
			tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
			tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
			tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
			Matrix4x4_Transform(&rsurface.matrix, tempcenter, center);
			R_MeshQueue_AddTransparent(rsurface.texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST ? r_refdef.view.origin : center, R_DrawSurface_TransparentCallback, ent, surface - rsurface.modelsurfaces, rsurface.rtlight);
		}
		else
		{
			// simply scan ahead until we find a different texture or lightmap state
			for (;j < numsurfaces && texture == surfacelist[j]->texture && rsurface.uselightmaptexture == (surfacelist[j]->lightmaptexture != NULL);j++)
				;
			// render the range of surfaces
			R_DrawTextureSurfaceList(j - i, surfacelist + i, writedepth, depthonly);
		}
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

int locboxelement3i[6*2*3] =
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
	R_Mesh_Matrix(&identitymatrix);

	R_Mesh_VertexPointer(vertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();

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

	R_Mesh_Draw(0, 6*4, 6*2, locboxelement3i, 0, 0);
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

void R_DrawDebugModel(entity_render_t *ent)
{
	int i, j, k, l, flagsmask;
	const int *elements;
	q3mbrush_t *brush;
	msurface_t *surface;
	model_t *model = ent->model;
	vec3_t v;

	flagsmask = MATERIALFLAG_SKY | MATERIALFLAG_WATER | MATERIALFLAG_WALL;

	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	GL_DepthRange(0, 1);
	GL_DepthTest(!r_showdisabledepthtest.integer);
	GL_DepthMask(false);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (r_showcollisionbrushes.value > 0 && model->brush.num_brushes)
	{
		GL_PolygonOffset(r_refdef.polygonfactor + r_showcollisionbrushes_polygonfactor.value, r_refdef.polygonoffset + r_showcollisionbrushes_polygonoffset.value);
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
		{
			if (brush->colbrushf && brush->colbrushf->numtriangles)
			{
				R_Mesh_VertexPointer(brush->colbrushf->points->v, 0, 0);
				GL_Color((i & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((i >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((i >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_Draw(0, brush->colbrushf->numpoints, brush->colbrushf->numtriangles, brush->colbrushf->elements, 0, 0);
			}
		}
		for (i = 0, surface = model->data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
		{
			if (surface->num_collisiontriangles)
			{
				R_Mesh_VertexPointer(surface->data_collisionvertex3f, 0, 0);
				GL_Color((i & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((i >> 5) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, ((i >> 10) & 31) * (1.0f / 32.0f) * r_refdef.view.colorscale, r_showcollisionbrushes.value);
				R_Mesh_Draw(0, surface->num_collisionvertices, surface->num_collisiontriangles, surface->data_collisionelement3i, 0, 0);
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
			if (ent == r_refdef.worldentity && !r_refdef.viewcache.world_surfacevisible[j])
				continue;
			rsurface.texture = surface->texture->currentframe;
			if ((rsurface.texture->currentmaterialflags & flagsmask) && surface->num_triangles)
			{
				RSurf_PrepareVerticesForBatch(true, true, 1, &surface);
				if (r_showtris.value > 0)
				{
					if (!rsurface.texture->currentlayers->depthmask)
						GL_Color(r_refdef.view.colorscale, 0, 0, r_showtris.value);
					else if (ent == r_refdef.worldentity)
						GL_Color(r_refdef.view.colorscale, r_refdef.view.colorscale, r_refdef.view.colorscale, r_showtris.value);
					else
						GL_Color(0, r_refdef.view.colorscale, 0, r_showtris.value);
					elements = (ent->model->surfmesh.data_element3i + 3 * surface->num_firsttriangle);
					CHECKGLERROR
					qglBegin(GL_LINES);
					for (k = 0;k < surface->num_triangles;k++, elements += 3)
					{
#define GLVERTEXELEMENT(n) qglVertex3f(rsurface.vertex3f[elements[n]*3+0], rsurface.vertex3f[elements[n]*3+1], rsurface.vertex3f[elements[n]*3+2])
						GLVERTEXELEMENT(0);GLVERTEXELEMENT(1);
						GLVERTEXELEMENT(1);GLVERTEXELEMENT(2);
						GLVERTEXELEMENT(2);GLVERTEXELEMENT(0);
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
void R_DrawWorldSurfaces(qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean addwaterplanes, qboolean debug)
{
	int i, j, endj, f, flagsmask;
	msurface_t *surface;
	texture_t *t;
	model_t *model = r_refdef.worldmodel;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	msurface_t *surfacelist[1024];
	if (model == NULL)
		return;

	RSurf_ActiveWorldEntity();

	// update light styles on this submodel
	if (!skysurfaces && !depthonly && !addwaterplanes && model->brushq1.num_lightstyles && r_refdef.lightmapintensity > 0)
	{
		model_brush_lightstyleinfo_t *style;
		for (i = 0, style = model->brushq1.data_lightstyleinfo;i < model->brushq1.num_lightstyles;i++, style++)
		{
			if (style->value != r_refdef.lightstylevalue[style->style])
			{
				msurface_t *surfaces = model->data_surfaces;
				int *list = style->surfacelist;
				style->value = r_refdef.lightstylevalue[style->style];
				for (j = 0;j < style->numsurfaces;j++)
					surfaces[list[j]].cached_dlight = true;
			}
		}
	}

	R_UpdateAllTextureInfo(r_refdef.worldentity);
	flagsmask = addwaterplanes ? (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION) : (skysurfaces ? MATERIALFLAG_SKY : (MATERIALFLAG_WATER | MATERIALFLAG_WALL));

	if (debug)
	{
		R_DrawDebugModel(r_refdef.worldentity);
		return;
	}

	f = 0;
	t = NULL;
	rsurface.uselightmaptexture = false;
	rsurface.texture = NULL;
	numsurfacelist = 0;
	j = model->firstmodelsurface;
	endj = j + model->nummodelsurfaces;
	while (j < endj)
	{
		// quickly skip over non-visible surfaces
		for (;j < endj && !r_refdef.viewcache.world_surfacevisible[j];j++)
			;
		// quickly iterate over visible surfaces
		for (;j < endj && r_refdef.viewcache.world_surfacevisible[j];j++)
		{
			// process this surface
			surface = model->data_surfaces + j;
			// if this surface fits the criteria, add it to the list
			if (surface->num_triangles)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight)
					R_BuildLightMap(r_refdef.worldentity, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				r_refdef.stats.world_triangles += surface->num_triangles;
				if (numsurfacelist >= maxsurfacelist)
				{
					r_refdef.stats.world_surfaces += numsurfacelist;
					R_QueueSurfaceList(r_refdef.worldentity, numsurfacelist, surfacelist, flagsmask, writedepth, depthonly, addwaterplanes);
					numsurfacelist = 0;
				}
			}
		}
	}
	r_refdef.stats.world_surfaces += numsurfacelist;
	if (numsurfacelist)
		R_QueueSurfaceList(r_refdef.worldentity, numsurfacelist, surfacelist, flagsmask, writedepth, depthonly, addwaterplanes);
	RSurf_CleanUp();
}

void R_DrawModelSurfaces(entity_render_t *ent, qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean addwaterplanes, qboolean debug)
{
	int i, j, f, flagsmask;
	msurface_t *surface, *endsurface;
	texture_t *t;
	model_t *model = ent->model;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	msurface_t *surfacelist[1024];
	if (model == NULL)
		return;

	// if the model is static it doesn't matter what value we give for
	// wantnormals and wanttangents, so this logic uses only rules applicable
	// to a model, knowing that they are meaningless otherwise
	if (ent == r_refdef.worldentity)
		RSurf_ActiveWorldEntity();
	else if ((ent->effects & EF_FULLBRIGHT) || r_showsurfaces.integer || VectorLength2(ent->modellight_diffuse) < (1.0f / 256.0f))
		RSurf_ActiveModelEntity(ent, false, false);
	else
		RSurf_ActiveModelEntity(ent, true, r_glsl.integer && gl_support_fragment_shader && !depthonly);

	// update light styles
	if (!skysurfaces && !depthonly && !addwaterplanes && model->brushq1.num_lightstyles && r_refdef.lightmapintensity > 0)
	{
		model_brush_lightstyleinfo_t *style;
		for (i = 0, style = model->brushq1.data_lightstyleinfo;i < model->brushq1.num_lightstyles;i++, style++)
		{
			if (style->value != r_refdef.lightstylevalue[style->style])
			{
				msurface_t *surfaces = model->data_surfaces;
				int *list = style->surfacelist;
				style->value = r_refdef.lightstylevalue[style->style];
				for (j = 0;j < style->numsurfaces;j++)
					surfaces[list[j]].cached_dlight = true;
			}
		}
	}

	R_UpdateAllTextureInfo(ent);
	flagsmask = addwaterplanes ? (MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION | MATERIALFLAG_REFLECTION) : (skysurfaces ? MATERIALFLAG_SKY : (MATERIALFLAG_WATER | MATERIALFLAG_WALL));

	if (debug)
	{
		R_DrawDebugModel(ent);
		return;
	}

	f = 0;
	t = NULL;
	rsurface.uselightmaptexture = false;
	rsurface.texture = NULL;
	numsurfacelist = 0;
	surface = model->data_surfaces + model->firstmodelsurface;
	endsurface = surface + model->nummodelsurfaces;
	for (;surface < endsurface;surface++)
	{
		// if this surface fits the criteria, add it to the list
		if (surface->num_triangles)
		{
			// if lightmap parameters changed, rebuild lightmap texture
			if (surface->cached_dlight)
				R_BuildLightMap(ent, surface);
			// add face to draw list
			surfacelist[numsurfacelist++] = surface;
			r_refdef.stats.entities_triangles += surface->num_triangles;
			if (numsurfacelist >= maxsurfacelist)
			{
				r_refdef.stats.entities_surfaces += numsurfacelist;
				R_QueueSurfaceList(ent, numsurfacelist, surfacelist, flagsmask, writedepth, depthonly, addwaterplanes);
				numsurfacelist = 0;
			}
		}
	}
	r_refdef.stats.entities_surfaces += numsurfacelist;
	if (numsurfacelist)
		R_QueueSurfaceList(ent, numsurfacelist, surfacelist, flagsmask, writedepth, depthonly, addwaterplanes);
	RSurf_CleanUp();
}
