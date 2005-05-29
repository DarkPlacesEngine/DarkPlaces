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

// used for dlight push checking and other things
int r_framecount;

mplane_t frustum[5];

matrix4x4_t r_identitymatrix;

int c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights, c_meshs, c_meshelements, c_rt_lights, c_rt_clears, c_rt_scissored, c_rt_shadowmeshes, c_rt_shadowtris, c_rt_lightmeshes, c_rt_lighttris, c_rtcached_shadowmeshes, c_rtcached_shadowtris, c_bloom, c_bloomcopies, c_bloomcopypixels, c_bloomdraws, c_bloomdrawpixels;

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


// forces all rendering to draw triangle outlines
int r_showtrispass;

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
float r_view_fov_x;
float r_view_fov_y;
matrix4x4_t r_view_matrix;

//
// screen size info
//
refdef_t r_refdef;

// 8.8 fraction of base light value
unsigned short d_lightstylevalue[256];

cvar_t r_showtris = {0, "r_showtris", "0"};
cvar_t r_drawentities = {0, "r_drawentities","1"};
cvar_t r_drawviewmodel = {0, "r_drawviewmodel","1"};
cvar_t r_speeds = {0, "r_speeds","0"};
cvar_t r_fullbright = {0, "r_fullbright","0"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};
cvar_t r_drawcollisionbrushes = {0, "r_drawcollisionbrushes", "0"};

cvar_t gl_fogenable = {0, "gl_fogenable", "0"};
cvar_t gl_fogdensity = {0, "gl_fogdensity", "0.25"};
cvar_t gl_fogred = {0, "gl_fogred","0.3"};
cvar_t gl_foggreen = {0, "gl_foggreen","0.3"};
cvar_t gl_fogblue = {0, "gl_fogblue","0.3"};
cvar_t gl_fogstart = {0, "gl_fogstart", "0"};
cvar_t gl_fogend = {0, "gl_fogend","0"};

cvar_t r_textureunits = {0, "r_textureunits", "32"};

cvar_t r_lerpsprites = {CVAR_SAVE, "r_lerpsprites", "1"};
cvar_t r_lerpmodels = {CVAR_SAVE, "r_lerpmodels", "1"};
cvar_t r_waterscroll = {CVAR_SAVE, "r_waterscroll", "1"};
cvar_t r_watershader = {CVAR_SAVE, "r_watershader", "1"};

cvar_t r_bloom = {CVAR_SAVE, "r_bloom", "0"};
cvar_t r_bloom_intensity = {CVAR_SAVE, "r_bloom_intensity", "2"};
cvar_t r_bloom_blur = {CVAR_SAVE, "r_bloom_blur", "8"};
cvar_t r_bloom_resolution = {CVAR_SAVE, "r_bloom_resolution", "320"};
cvar_t r_bloom_power = {CVAR_SAVE, "r_bloom_power", "4"};

cvar_t developer_texturelogging = {0, "developer_texturelogging", "1"};

cvar_t gl_lightmaps = {0, "gl_lightmaps", "0"};

rtexturepool_t *r_main_texturepool;
rtexture_t *r_bloom_texture_screen;
rtexture_t *r_bloom_texture_bloom;
rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_black;
rtexture_t *r_texture_notexture;
rtexture_t *r_texture_whitecube;
rtexture_t *r_texture_normalizationcube;
rtexture_t *r_texture_detailtextures[NUM_DETAILTEXTURES];
rtexture_t *r_texture_distorttexture[64];

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
}

static void R_BuildDetailTextures (void)
{
	int i, x, y, light;
	float vc[3], vx[3], vy[3], vn[3], lightdir[3];
#define DETAILRESOLUTION 256
	qbyte (*data)[DETAILRESOLUTION][4];
	qbyte (*noise)[DETAILRESOLUTION];

	// Allocate the buffers dynamically to avoid having such big guys on the stack
	data = Mem_Alloc(tempmempool, DETAILRESOLUTION * sizeof(*data));
	noise = Mem_Alloc(tempmempool, DETAILRESOLUTION * sizeof(*noise));

	lightdir[0] = 0.5;
	lightdir[1] = 1;
	lightdir[2] = -0.25;
	VectorNormalize(lightdir);
	for (i = 0;i < NUM_DETAILTEXTURES;i++)
	{
		fractalnoise(&noise[0][0], DETAILRESOLUTION, DETAILRESOLUTION >> 4);
		for (y = 0;y < DETAILRESOLUTION;y++)
		{
			for (x = 0;x < DETAILRESOLUTION;x++)
			{
				vc[0] = x;
				vc[1] = y;
				vc[2] = noise[y][x] * (1.0f / 32.0f);
				vx[0] = x + 1;
				vx[1] = y;
				vx[2] = noise[y][(x + 1) % DETAILRESOLUTION] * (1.0f / 32.0f);
				vy[0] = x;
				vy[1] = y + 1;
				vy[2] = noise[(y + 1) % DETAILRESOLUTION][x] * (1.0f / 32.0f);
				VectorSubtract(vx, vc, vx);
				VectorSubtract(vy, vc, vy);
				CrossProduct(vx, vy, vn);
				VectorNormalize(vn);
				light = 128 - DotProduct(vn, lightdir) * 128;
				light = bound(0, light, 255);
				data[y][x][0] = data[y][x][1] = data[y][x][2] = light;
				data[y][x][3] = 255;
			}
		}
		r_texture_detailtextures[i] = R_LoadTexture2D(r_main_texturepool, va("detailtexture%i", i), DETAILRESOLUTION, DETAILRESOLUTION, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_PRECACHE, NULL);
	}

	Mem_Free(noise);
	Mem_Free(data);
}

static qbyte R_MorphDistortTexture (double y0, double y1, double y2, double y3, double morph)
{
	int m =	(int)(((y1 + y3 - (y0 + y2)) * morph * morph * morph) +
			((2 * (y0 - y1) + y2 - y3) * morph * morph) +
			((y2 - y0) * morph) +
			(y1));
	return (qbyte)bound(0, m, 255);
}

static void R_BuildDistortTexture (void)
{
	int x, y, i, j;
#define DISTORTRESOLUTION 32
	qbyte data[5][DISTORTRESOLUTION][DISTORTRESOLUTION][2];

	for (i=0; i<4; i++)
	{
		for (y=0; y<DISTORTRESOLUTION; y++)
		{
			for (x=0; x<DISTORTRESOLUTION; x++)
			{
				data[i][y][x][0] = rand () & 255;
				data[i][y][x][1] = rand () & 255;
			}
		}
	}

	for (i=0; i<4; i++)
	{
		for (j=0; j<16; j++)
		{
			r_texture_distorttexture[i*16+j] = NULL;
			if (gl_textureshader)
			{
				for (y=0; y<DISTORTRESOLUTION; y++)
				{
					for (x=0; x<DISTORTRESOLUTION; x++)
					{
						data[4][y][x][0] = R_MorphDistortTexture (data[(i-1)&3][y][x][0], data[i][y][x][0], data[(i+1)&3][y][x][0], data[(i+2)&3][y][x][0], 0.0625*j);
						data[4][y][x][1] = R_MorphDistortTexture (data[(i-1)&3][y][x][1], data[i][y][x][1], data[(i+1)&3][y][x][1], data[(i+2)&3][y][x][1], 0.0625*j);
					}
				}
				r_texture_distorttexture[i*16+j] = R_LoadTexture2D(r_main_texturepool, va("distorttexture%i", i*16+j), DISTORTRESOLUTION, DISTORTRESOLUTION, &data[4][0][0][0], TEXTYPE_DSDT, TEXF_PRECACHE, NULL);
			}
		}
	}
}

static void R_BuildBlankTextures(void)
{
	qbyte data[4];
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
	qbyte pix[16][16][4];
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
	qbyte data[6*1*1*4];
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
	qbyte data[6][NORMSIZE][NORMSIZE][4];
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
				data[side][y][x][0] = 128.0f + intensity * v[0];
				data[side][y][x][1] = 128.0f + intensity * v[1];
				data[side][y][x][2] = 128.0f + intensity * v[2];
				data[side][y][x][3] = 255;
			}
		}
	}
	r_texture_normalizationcube = R_LoadTextureCubeMap(r_main_texturepool, "normalcube", NORMSIZE, &data[0][0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE | TEXF_CLAMP, NULL);
}

void gl_main_start(void)
{
	r_main_texturepool = R_AllocTexturePool();
	r_bloom_texture_screen = NULL;
	r_bloom_texture_bloom = NULL;
	R_BuildBlankTextures();
	R_BuildNoTexture();
	R_BuildDetailTextures();
	R_BuildDistortTexture();
	if (gl_texturecubemap)
	{
		R_BuildWhiteCube();
		R_BuildNormalizationCube();
	}
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
		l = strlen(entname) - 4;
		if (l >= 0 && !strcmp(entname + l, ".bsp"))
		{
			strcpy(entname + l, ".ent");
			if ((entities = FS_LoadFile(entname, tempmempool, true)))
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
	Matrix4x4_CreateIdentity(&r_identitymatrix);
// FIXME: move this to client?
	FOG_registercvars();
	Cvar_RegisterVariable(&r_showtris);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&r_lerpsprites);
	Cvar_RegisterVariable(&r_lerpmodels);
	Cvar_RegisterVariable(&r_waterscroll);
	Cvar_RegisterVariable(&r_watershader);
	Cvar_RegisterVariable(&r_drawcollisionbrushes);
	Cvar_RegisterVariable(&r_bloom);
	Cvar_RegisterVariable(&r_bloom_intensity);
	Cvar_RegisterVariable(&r_bloom_blur);
	Cvar_RegisterVariable(&r_bloom_resolution);
	Cvar_RegisterVariable(&r_bloom_power);
	Cvar_RegisterVariable(&developer_texturelogging);
	Cvar_RegisterVariable(&gl_lightmaps);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_NEXUIZ || gamemode == GAME_TENEBRAE)
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
	Mod_RenderInit();
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
	UI_Init();
	Sbar_Init();
	R_LightningBeams_Init();
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
			Mod_CheckLoaded(ent->model);
			// some of the renderer still relies on origin...
			Matrix4x4_OriginFromMatrix(&ent->matrix, ent->origin);
			// some of the renderer still relies on scale...
			ent->scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
			if (!(ent->flags & renderimask) && !R_CullBox(ent->mins, ent->maxs) && ((ent->effects & EF_NODEPTHTEST) || r_refdef.worldmodel->brush.BoxTouchingVisibleLeafs(r_refdef.worldmodel, r_worldleafvisible, ent->mins, ent->maxs)))
			{
				R_UpdateEntLights(ent);
				ent->visframe = r_framecount;
			}
		}
	}
	else
	{
		// no worldmodel or it can't check visibility
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			Mod_CheckLoaded(ent->model);
			// some of the renderer still relies on origin...
			Matrix4x4_OriginFromMatrix(&ent->matrix, ent->origin);
			// some of the renderer still relies on scale...
			ent->scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
			if (!(ent->flags & renderimask) && !R_CullBox(ent->mins, ent->maxs) && (ent->effects & EF_NODEPTHTEST))
			{
				R_UpdateEntLights(ent);
				ent->visframe = r_framecount;
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

	// LordHavoc: note to all quake engine coders, the special case for 90
	// degrees assumed a square view (wrong), so I removed it, Quake2 has it
	// disabled as well.

	// rotate R_VIEWFORWARD right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, r_viewup, r_viewforward, -(90 - r_view_fov_x / 2));
	frustum[0].dist = DotProduct (r_vieworigin, frustum[0].normal);
	PlaneClassify(&frustum[0]);

	// rotate R_VIEWFORWARD left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, r_viewup, r_viewforward, (90 - r_view_fov_x / 2));
	frustum[1].dist = DotProduct (r_vieworigin, frustum[1].normal);
	PlaneClassify(&frustum[1]);

	// rotate R_VIEWFORWARD up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, r_viewleft, r_viewforward, -(90 - r_view_fov_y / 2));
	frustum[2].dist = DotProduct (r_vieworigin, frustum[2].normal);
	PlaneClassify(&frustum[2]);

	// rotate R_VIEWFORWARD down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, r_viewleft, r_viewforward, (90 - r_view_fov_y / 2));
	frustum[3].dist = DotProduct (r_vieworigin, frustum[3].normal);
	PlaneClassify(&frustum[3]);

	// nearclip plane
	VectorCopy(r_viewforward, frustum[4].normal);
	frustum[4].dist = DotProduct (r_vieworigin, frustum[4].normal) + 1.0f;
	PlaneClassify(&frustum[4]);
}

static void R_BlendView(void)
{
	rmeshstate_t m;

	if (r_refdef.viewblend[3] < 0.01f && !r_bloom.integer)
		return;

	GL_SetupView_Mode_Ortho(0, 0, 1, 1, -10, 100);
	GL_DepthMask(true);
	GL_DepthTest(false);
	R_Mesh_Matrix(&r_identitymatrix);
	// vertex coordinates for a quad that covers the screen exactly
	varray_vertex3f[0] = 0;varray_vertex3f[1] = 0;varray_vertex3f[2] = 0;
	varray_vertex3f[3] = 1;varray_vertex3f[4] = 0;varray_vertex3f[5] = 0;
	varray_vertex3f[6] = 1;varray_vertex3f[7] = 1;varray_vertex3f[8] = 0;
	varray_vertex3f[9] = 0;varray_vertex3f[10] = 1;varray_vertex3f[11] = 0;
	if (r_bloom.integer && r_bloom_resolution.value >= 32 && r_bloom_power.integer >= 1 && r_bloom_power.integer < 100 && r_bloom_blur.value >= 0 && r_bloom_blur.value < 512)
	{
		int screenwidth, screenheight, bloomwidth, bloomheight, x, dobloomblend, range;
		float xoffset, yoffset, r;
		c_bloom++;
		// set the (poorly named) screenwidth and screenheight variables to
		// a power of 2 at least as large as the screen, these will define the
		// size of the texture to allocate
		for (screenwidth = 1;screenwidth < vid.width;screenwidth *= 2);
		for (screenheight = 1;screenheight < vid.height;screenheight *= 2);
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
		varray_texcoord2f[0][0] = 0;
		varray_texcoord2f[0][1] = (float)r_view_height / (float)screenheight;
		varray_texcoord2f[0][2] = (float)r_view_width / (float)screenwidth;
		varray_texcoord2f[0][3] = (float)r_view_height / (float)screenheight;
		varray_texcoord2f[0][4] = (float)r_view_width / (float)screenwidth;
		varray_texcoord2f[0][5] = 0;
		varray_texcoord2f[0][6] = 0;
		varray_texcoord2f[0][7] = 0;
		// set up a texcoord array for the reduced resolution bloom image
		// (which will be additive blended over the screen image)
		varray_texcoord2f[1][0] = 0;
		varray_texcoord2f[1][1] = (float)bloomheight / (float)screenheight;
		varray_texcoord2f[1][2] = (float)bloomwidth / (float)screenwidth;
		varray_texcoord2f[1][3] = (float)bloomheight / (float)screenheight;
		varray_texcoord2f[1][4] = (float)bloomwidth / (float)screenwidth;
		varray_texcoord2f[1][5] = 0;
		varray_texcoord2f[1][6] = 0;
		varray_texcoord2f[1][7] = 0;
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = varray_vertex3f;
		m.pointer_texcoord[0] = varray_texcoord2f[0];
		m.tex[0] = R_GetTexture(r_bloom_texture_screen);
		R_Mesh_State(&m);
		// copy view into the full resolution screen image texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
		c_bloomcopies++;
		c_bloomcopypixels += r_view_width * r_view_height;
		// now scale it down to the bloom size and raise to a power of itself
		// to darken it (this leaves the really bright stuff bright, and
		// everything else becomes very dark)
		// TODO: optimize with multitexture or GLSL
		qglViewport(r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(1, 1, 1, 1);
		R_Mesh_Draw(0, 4, 2, polygonelements);
		c_bloomdraws++;
		c_bloomdrawpixels += bloomwidth * bloomheight;
		// render multiple times with a multiply blendfunc to raise to a power
		GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		for (x = 1;x < r_bloom_power.integer;x++)
		{
			R_Mesh_Draw(0, 4, 2, polygonelements);
			c_bloomdraws++;
			c_bloomdrawpixels += bloomwidth * bloomheight;
		}
		// we now have a darkened bloom image in the framebuffer, copy it into
		// the bloom image texture for more processing
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = varray_vertex3f;
		m.tex[0] = R_GetTexture(r_bloom_texture_bloom);
		m.pointer_texcoord[0] = varray_texcoord2f[2];
		R_Mesh_State(&m);
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		c_bloomcopies++;
		c_bloomcopypixels += bloomwidth * bloomheight;
		// blend on at multiple vertical offsets to achieve a vertical blur
		// TODO: do offset blends using GLSL
		range = r_bloom_blur.integer * bloomwidth / 320;
		GL_BlendFunc(GL_ONE, GL_ZERO);
		for (x = -range;x <= range;x++)
		{
			xoffset = 0 / (float)bloomwidth * (float)bloomwidth / (float)screenwidth;
			yoffset = x / (float)bloomheight * (float)bloomheight / (float)screenheight;
			// compute a texcoord array with the specified x and y offset
			varray_texcoord2f[2][0] = xoffset+0;
			varray_texcoord2f[2][1] = yoffset+(float)bloomheight / (float)screenheight;
			varray_texcoord2f[2][2] = xoffset+(float)bloomwidth / (float)screenwidth;
			varray_texcoord2f[2][3] = yoffset+(float)bloomheight / (float)screenheight;
			varray_texcoord2f[2][4] = xoffset+(float)bloomwidth / (float)screenwidth;
			varray_texcoord2f[2][5] = yoffset+0;
			varray_texcoord2f[2][6] = xoffset+0;
			varray_texcoord2f[2][7] = yoffset+0;
			// this r value looks like a 'dot' particle, fading sharply to
			// black at the edges
			// (probably not realistic but looks good enough)
			r = r_bloom_intensity.value/(range*2+1)*(1 - x*x/(float)(range*range));
			if (r < 0.01f)
				continue;
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 2, polygonelements);
			c_bloomdraws++;
			c_bloomdrawpixels += bloomwidth * bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
		// copy the vertically blurred bloom view to a texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		c_bloomcopies++;
		c_bloomcopypixels += bloomwidth * bloomheight;
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
			varray_texcoord2f[2][0] = xoffset+0;
			varray_texcoord2f[2][1] = yoffset+(float)bloomheight / (float)screenheight;
			varray_texcoord2f[2][2] = xoffset+(float)bloomwidth / (float)screenwidth;
			varray_texcoord2f[2][3] = yoffset+(float)bloomheight / (float)screenheight;
			varray_texcoord2f[2][4] = xoffset+(float)bloomwidth / (float)screenwidth;
			varray_texcoord2f[2][5] = yoffset+0;
			varray_texcoord2f[2][6] = xoffset+0;
			varray_texcoord2f[2][7] = yoffset+0;
			// this r value looks like a 'dot' particle, fading sharply to
			// black at the edges
			// (probably not realistic but looks good enough)
			r = r_bloom_intensity.value/(range*2+1)*(1 - x*x/(float)(range*range));
			if (r < 0.01f)
				continue;
			GL_Color(r, r, r, 1);
			R_Mesh_Draw(0, 4, 2, polygonelements);
			c_bloomdraws++;
			c_bloomdrawpixels += bloomwidth * bloomheight;
			GL_BlendFunc(GL_ONE, GL_ONE);
		}
		// copy the blurred bloom view to a texture
		GL_ActiveTexture(0);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.height - (r_view_y + bloomheight), bloomwidth, bloomheight);
		c_bloomcopies++;
		c_bloomcopypixels += bloomwidth * bloomheight;
		// go back to full view area
		qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
		// put the original screen image back in place and blend the bloom
		// texture on it
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = varray_vertex3f;
		m.tex[0] = R_GetTexture(r_bloom_texture_screen);
		m.pointer_texcoord[0] = varray_texcoord2f[0];
#if 0
		dobloomblend = false;
#else
		// do both in one pass if possible
		if (r_textureunits.integer >= 2 && gl_combine.integer)
		{
			dobloomblend = false;
			m.texcombinergb[1] = GL_ADD;
			m.tex[1] = R_GetTexture(r_bloom_texture_bloom);
			m.pointer_texcoord[1] = varray_texcoord2f[1];
		}
		else
			dobloomblend = true;
#endif
		R_Mesh_State(&m);
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_Color(1,1,1,1);
		R_Mesh_Draw(0, 4, 2, polygonelements);
		c_bloomdraws++;
		c_bloomdrawpixels += r_view_width * r_view_height;
		// now blend on the bloom texture if multipass
		if (dobloomblend)
		{
			memset(&m, 0, sizeof(m));
			m.pointer_vertex = varray_vertex3f;
			m.tex[0] = R_GetTexture(r_bloom_texture_bloom);
			m.pointer_texcoord[0] = varray_texcoord2f[1];
			R_Mesh_State(&m);
			GL_BlendFunc(GL_ONE, GL_ONE);
			GL_Color(1,1,1,1);
			R_Mesh_Draw(0, 4, 2, polygonelements);
			c_bloomdraws++;
			c_bloomdrawpixels += r_view_width * r_view_height;
		}
	}
	if (r_refdef.viewblend[3] >= 0.01f)
	{
		// apply a color tint to the whole view
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = varray_vertex3f;
		R_Mesh_State(&m);
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
	r_view_fov_x = bound(1, r_refdef.fov_x, 170);
	r_view_fov_y = bound(1, r_refdef.fov_y, 170);
	r_view_matrix = r_refdef.viewentitymatrix;
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
	r_rtworld = r_shadow_realtime_world.integer;
	r_rtworldshadows = r_shadow_realtime_world_shadows.integer && gl_stencil;
	r_rtdlight = (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer) && !gl_flashblend.integer;
	r_rtdlightshadows = r_rtdlight && (r_rtworld ? r_shadow_realtime_world_dlightshadows.integer : r_shadow_realtime_dlight_shadows.integer) && gl_stencil;
	r_lightmapintensity = r_rtworld ? r_shadow_realtime_world_lightmaps.value : 1;

	// GL is weird because it's bottom to top, r_view_y is top to bottom
	qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
	GL_Scissor(r_view_x, r_view_y, r_view_width, r_view_height);
	GL_ScissorTest(true);
	GL_DepthMask(true);
	R_ClearScreen();
	R_Textures_Frame();
	R_UpdateFog();
	R_UpdateLights();
	R_TimeReport("setup");

	qglDepthFunc(GL_LEQUAL);
	qglPolygonOffset(0, 0);
	qglEnable(GL_POLYGON_OFFSET_FILL);

	R_RenderScene();

	qglPolygonOffset(0, 0);
	qglDisable(GL_POLYGON_OFFSET_FILL);

	R_BlendView();
	R_TimeReport("blendview");

	GL_Scissor(0, 0, vid.width, vid.height);
	GL_ScissorTest(false);
}

extern void R_DrawLightningBeams (void);
void R_RenderScene(void)
{
	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	r_framecount++;

	R_MeshQueue_BeginScene();

	GL_ShowTrisColor(0.05, 0.05, 0.05, 1);

	R_SetFrustum();

	r_farclip = R_FarClip(r_vieworigin, r_viewforward, 768.0f) + 256.0f;
	if (r_rtworldshadows || r_rtdlightshadows)
		GL_SetupView_Mode_PerspectiveInfiniteFarClip(r_view_fov_x, r_view_fov_y, 1.0f);
	else
		GL_SetupView_Mode_Perspective(r_view_fov_x, r_view_fov_y, 1.0f, r_farclip);

	GL_SetupView_Orientation_FromEntity(&r_view_matrix);

	Matrix4x4_CreateTranslate(&r_waterscrollmatrix, sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);

	R_SkyStartFrame();

	R_WorldVisibility();
	R_TimeReport("worldvis");

	R_MarkEntities();
	R_TimeReport("markentity");

	R_Shadow_UpdateWorldLightSelection();

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	GL_ShowTrisColor(0.025, 0.025, 0, 1);
	if (r_refdef.worldmodel && r_refdef.worldmodel->DrawSky)
	{
		r_refdef.worldmodel->DrawSky(r_refdef.worldentity);
		R_TimeReport("worldsky");
	}

	if (R_DrawBrushModelsSky())
		R_TimeReport("bmodelsky");

	GL_ShowTrisColor(0.05, 0.05, 0.05, 1);
	if (r_refdef.worldmodel && r_refdef.worldmodel->Draw)
	{
		r_refdef.worldmodel->Draw(r_refdef.worldentity);
		R_TimeReport("world");
	}

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	GL_ShowTrisColor(0, 0.015, 0, 1);

	R_DrawModels();
	R_TimeReport("models");

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	GL_ShowTrisColor(0, 0, 0.033, 1);
	R_ShadowVolumeLighting(false);
	R_TimeReport("rtlights");

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();

	GL_ShowTrisColor(0.1, 0, 0, 1);

	R_DrawLightningBeams();
	R_TimeReport("lightning");

	R_DrawParticles();
	R_TimeReport("particles");

	R_DrawExplosions();
	R_TimeReport("explosions");

	R_MeshQueue_RenderTransparent();
	R_TimeReport("drawtrans");

	R_DrawCoronas();
	R_TimeReport("coronas");

	R_DrawWorldCrosshair();
	R_TimeReport("crosshair");

	R_MeshQueue_Render();
	R_MeshQueue_EndScene();

	if ((r_shadow_visiblelighting.integer || r_shadow_visiblevolumes.integer) && !r_showtrispass)
	{
		R_ShadowVolumeLighting(true);
		R_TimeReport("visiblevolume");
	}

	GL_ShowTrisColor(0.05, 0.05, 0.05, 1);

	// don't let sound skip if going slow
	if (r_refdef.extraupdate)
		S_ExtraUpdate ();
}

/*
void R_DrawBBoxMesh(vec3_t mins, vec3_t maxs, float cr, float cg, float cb, float ca)
{
	int i;
	float *v, *c, f1, f2, diff[3], vertex3f[8*3], color4f[8*4];
	rmeshstate_t m;
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&r_identitymatrix);

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
			VectorSubtract(v, r_vieworigin, diff);
			f2 = exp(fogdensity/DotProduct(diff, diff));
			f1 = 1 - f2;
			c[0] = c[0] * f1 + fogcolor[0] * f2;
			c[1] = c[1] * f1 + fogcolor[1] * f2;
			c[2] = c[2] * f1 + fogcolor[2] * f2;
		}
	}
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = vertex3f;
	m.pointer_color = color;
	R_Mesh_State(&m);
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

void R_DrawNoModelCallback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	int i;
	float f1, f2, *c, diff[3];
	float color4f[6*4];
	rmeshstate_t m;
	R_Mesh_Matrix(&ent->matrix);

	memset(&m, 0, sizeof(m));
	m.pointer_vertex = nomodelvertex3f;

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
	if (fogenabled)
	{
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		m.pointer_color = color4f;
		VectorSubtract(ent->origin, r_vieworigin, diff);
		f2 = exp(fogdensity/DotProduct(diff, diff));
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
		m.pointer_color = color4f;
		for (i = 0, c = color4f;i < 6;i++, c += 4)
			c[3] *= ent->alpha;
	}
	else
		m.pointer_color = nomodelcolor4f;
	R_Mesh_State(&m);
	R_Mesh_Draw(0, 6, 8, nomodelelements);
}

void R_DrawNoModel(entity_render_t *ent)
{
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : ent->origin, R_DrawNoModelCallback, ent, 0);
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

void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, int depthdisable, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca)
{
	float diff[3];
	rmeshstate_t m;

	if (fogenabled)
	{
		VectorSubtract(origin, r_vieworigin, diff);
		ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
	}

	R_Mesh_Matrix(&r_identitymatrix);
	GL_BlendFunc(blendfunc1, blendfunc2);
	GL_DepthMask(false);
	GL_DepthTest(!depthdisable);

	varray_vertex3f[ 0] = origin[0] + left[0] * scalex2 + up[0] * scaley1;
	varray_vertex3f[ 1] = origin[1] + left[1] * scalex2 + up[1] * scaley1;
	varray_vertex3f[ 2] = origin[2] + left[2] * scalex2 + up[2] * scaley1;
	varray_vertex3f[ 3] = origin[0] + left[0] * scalex2 + up[0] * scaley2;
	varray_vertex3f[ 4] = origin[1] + left[1] * scalex2 + up[1] * scaley2;
	varray_vertex3f[ 5] = origin[2] + left[2] * scalex2 + up[2] * scaley2;
	varray_vertex3f[ 6] = origin[0] + left[0] * scalex1 + up[0] * scaley2;
	varray_vertex3f[ 7] = origin[1] + left[1] * scalex1 + up[1] * scaley2;
	varray_vertex3f[ 8] = origin[2] + left[2] * scalex1 + up[2] * scaley2;
	varray_vertex3f[ 9] = origin[0] + left[0] * scalex1 + up[0] * scaley1;
	varray_vertex3f[10] = origin[1] + left[1] * scalex1 + up[1] * scaley1;
	varray_vertex3f[11] = origin[2] + left[2] * scalex1 + up[2] * scaley1;

	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(texture);
	m.pointer_texcoord[0] = spritetexcoord2f;
	m.pointer_vertex = varray_vertex3f;
	R_Mesh_State(&m);
	GL_Color(cr, cg, cb, ca);
	R_Mesh_Draw(0, 4, 2, polygonelements);
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
			PolygonF_Divide(tempnumpoints, temppoints[w], plane2->normal[0], plane2->normal[1], plane2->normal[2], plane2->dist, 1.0/32.0, 0, NULL, NULL, 256, temppoints[!w], &tempnumpoints);
			w = !w;
		}
		if (tempnumpoints < 3)
			continue;
		// generate elements forming a triangle fan for this polygon
		R_Mesh_AddPolygon3f(mesh, tempnumpoints, temppoints[w]);
	}
}

void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t)
{
	texture_t *texture = t;
	model_t *model = ent->model;
	int s = ent->skinnum;
	if ((unsigned int)s >= (unsigned int)model->numskins)
		s = 0;
	if (s >= 1)
		c_models++;
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
	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = ent->alpha;
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA)
		t->currentalpha *= r_wateralpha.value;
	if (!(ent->flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	if (ent->effects & EF_ADDITIVE)
		t->currentmaterialflags |= MATERIALFLAG_ADD | MATERIALFLAG_TRANSPARENT;
	else if (t->currentalpha < 1)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_TRANSPARENT;
	if (ent->effects & EF_NODEPTHTEST)
		t->currentmaterialflags |= MATERIALFLAG_NODEPTHTEST;
}

void R_UpdateAllTextureInfo(entity_render_t *ent)
{
	int i;
	if (ent->model)
		for (i = 0;i < ent->model->num_textures;i++)
			R_UpdateTextureInfo(ent, ent->model->data_textures + i);
}

float *rsurface_vertex3f;
float *rsurface_svector3f;
float *rsurface_tvector3f;
float *rsurface_normal3f;
float *rsurface_lightmapcolor4f;

void RSurf_SetVertexPointer(const entity_render_t *ent, const texture_t *texture, const msurface_t *surface, const vec3_t modelorg)
{
	int i, j;
	float center[3], forward[3], right[3], up[3], v[4][3];
	matrix4x4_t matrix1, imatrix1;
	if ((ent->frameblend[0].lerp != 1 || ent->frameblend[0].frame != 0) && (surface->groupmesh->data_morphvertex3f || surface->groupmesh->data_vertexboneweights))
	{
		rsurface_vertex3f = varray_vertex3f;
		rsurface_svector3f = NULL;
		rsurface_tvector3f = NULL;
		rsurface_normal3f = NULL;
		Mod_Alias_GetMesh_Vertex3f(ent->model, ent->frameblend, surface->groupmesh, rsurface_vertex3f);
	}
	else
	{
		rsurface_vertex3f = surface->groupmesh->data_vertex3f;
		rsurface_svector3f = surface->groupmesh->data_svector3f;
		rsurface_tvector3f = surface->groupmesh->data_tvector3f;
		rsurface_normal3f = surface->groupmesh->data_normal3f;
	}
	if (texture->textureflags & Q3TEXTUREFLAG_AUTOSPRITE2)
	{
		if (!rsurface_svector3f)
		{
			rsurface_svector3f = varray_svector3f;
			rsurface_tvector3f = varray_tvector3f;
			rsurface_normal3f = varray_normal3f;
			Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f);
		}
		// a single autosprite surface can contain multiple sprites...
		VectorClear(forward);
		VectorClear(right);
		VectorSet(up, 0, 0, 1);
		for (j = 0;j < surface->num_vertices - 3;j += 4)
		{
			VectorClear(center);
			for (i = 0;i < 4;i++)
				VectorAdd(center, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i) * 3, center);
			VectorScale(center, 0.25f, center);
			// FIXME: calculate vectors from triangle edges instead of using texture vectors as an easy way out?
			Matrix4x4_FromVectors(&matrix1, (rsurface_normal3f + 3 * surface->num_firstvertex) + j*3, (rsurface_svector3f + 3 * surface->num_firstvertex) + j*3, (rsurface_tvector3f + 3 * surface->num_firstvertex) + j*3, center);
			Matrix4x4_Invert_Simple(&imatrix1, &matrix1);
			for (i = 0;i < 4;i++)
				Matrix4x4_Transform(&imatrix1, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i)*3, v[i]);
			forward[0] = modelorg[0] - center[0];
			forward[1] = modelorg[1] - center[1];
			VectorNormalize(forward);
			right[0] = forward[1];
			right[1] = -forward[0];
			for (i = 0;i < 4;i++)
				VectorMAMAMAM(1, center, v[i][0], forward, v[i][1], right, v[i][2], up, varray_vertex3f + (surface->num_firstvertex+i+j) * 3);
		}
		rsurface_vertex3f = varray_vertex3f;
		rsurface_svector3f = NULL;
		rsurface_tvector3f = NULL;
		rsurface_normal3f = NULL;
	}
	else if (texture->textureflags & Q3TEXTUREFLAG_AUTOSPRITE)
	{
		if (!rsurface_svector3f)
		{
			rsurface_svector3f = varray_svector3f;
			rsurface_tvector3f = varray_tvector3f;
			rsurface_normal3f = varray_normal3f;
			Mod_BuildTextureVectorsAndNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_texcoordtexture2f, surface->groupmesh->data_element3i + surface->num_firsttriangle * 3, rsurface_svector3f, rsurface_tvector3f, rsurface_normal3f);
		}
		Matrix4x4_Transform(&ent->inversematrix, r_viewforward, forward);
		Matrix4x4_Transform(&ent->inversematrix, r_viewright, right);
		Matrix4x4_Transform(&ent->inversematrix, r_viewup, up);
		// a single autosprite surface can contain multiple sprites...
		for (j = 0;j < surface->num_vertices - 3;j += 4)
		{
			VectorClear(center);
			for (i = 0;i < 4;i++)
				VectorAdd(center, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i) * 3, center);
			VectorScale(center, 0.25f, center);
			// FIXME: calculate vectors from triangle edges instead of using texture vectors as an easy way out?
			Matrix4x4_FromVectors(&matrix1, (rsurface_normal3f + 3 * surface->num_firstvertex) + j*3, (rsurface_svector3f + 3 * surface->num_firstvertex) + j*3, (rsurface_tvector3f + 3 * surface->num_firstvertex) + j*3, center);
			Matrix4x4_Invert_Simple(&imatrix1, &matrix1);
			for (i = 0;i < 4;i++)
				Matrix4x4_Transform(&imatrix1, (rsurface_vertex3f + 3 * surface->num_firstvertex) + (j+i)*3, v[i]);
			for (i = 0;i < 4;i++)
				VectorMAMAMAM(1, center, v[i][0], forward, v[i][1], right, v[i][2], up, varray_vertex3f + (surface->num_firstvertex+i+j) * 3);
		}
		rsurface_vertex3f = varray_vertex3f;
		rsurface_svector3f = NULL;
		rsurface_tvector3f = NULL;
		rsurface_normal3f = NULL;
	}
	R_Mesh_VertexPointer(rsurface_vertex3f);
}

void RSurf_SetColorPointer(const entity_render_t *ent, const msurface_t *surface, const vec3_t modelorg, float r, float g, float b, float a, qboolean lightmodel, qboolean vertexlight, qboolean applycolor, qboolean applyfog)
{
	int i;
	float f;
	float *v, *c, *c2;
	vec3_t diff;
	if (lightmodel)
	{
		vec4_t ambientcolor4f;
		vec3_t diffusecolor;
		vec3_t diffusenormal;
		if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, r, g, b, a, false))
		{
			rsurface_lightmapcolor4f = varray_color4f;
			if (rsurface_normal3f == NULL)
			{
				rsurface_normal3f = varray_normal3f;
				Mod_BuildNormals(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, rsurface_vertex3f, surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle, rsurface_normal3f);
			}
			R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, surface->groupmesh->num_vertices, rsurface_vertex3f + 3 * surface->num_firstvertex, rsurface_normal3f + 3 * surface->num_firstvertex, rsurface_lightmapcolor4f + 4 * surface->num_firstvertex);
			r = 1;
			g = 1;
			b = 1;
			a = 1;
			applycolor = false;
		}
		else
		{
			r = ambientcolor4f[0];
			g = ambientcolor4f[1];
			b = ambientcolor4f[2];
			a = ambientcolor4f[3];
			rsurface_lightmapcolor4f = NULL;
		}
	}
	else if (vertexlight)
	{
		if (surface->lightmapinfo)
		{
			rsurface_lightmapcolor4f = varray_color4f;
			for (i = 0, c = rsurface_lightmapcolor4f + 4 * surface->num_firstvertex;i < surface->num_vertices;i++, c += 4)
			{
				const qbyte *lm = surface->lightmapinfo->samples + (surface->groupmesh->data_lightmapoffsets + surface->num_firstvertex)[i];
				float scale = d_lightstylevalue[surface->lightmapinfo->styles[0]] * (1.0f / 32768.0f);
				VectorScale(lm, scale, c);
				if (surface->lightmapinfo->styles[1] != 255)
				{
					int size3 = ((surface->lightmapinfo->extents[0]>>4)+1)*((surface->lightmapinfo->extents[1]>>4)+1)*3;
					lm += size3;
					scale = d_lightstylevalue[surface->lightmapinfo->styles[1]] * (1.0f / 32768.0f);
					VectorMA(c, scale, lm, c);
					if (surface->lightmapinfo->styles[2] != 255)
					{
						lm += size3;
						scale = d_lightstylevalue[surface->lightmapinfo->styles[2]] * (1.0f / 32768.0f);
						VectorMA(c, scale, lm, c);
						if (surface->lightmapinfo->styles[3] != 255)
						{
							lm += size3;
							scale = d_lightstylevalue[surface->lightmapinfo->styles[3]] * (1.0f / 32768.0f);
							VectorMA(c, scale, lm, c);
						}
					}
				}
			}
		}
		else
			rsurface_lightmapcolor4f = surface->groupmesh->data_lightmapcolor4f;
	}
	else
		rsurface_lightmapcolor4f = NULL;
	if (applyfog)
	{
		if (rsurface_lightmapcolor4f)
		{
			for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c = (rsurface_lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (varray_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4, c2 += 4)
			{
				VectorSubtract(v, modelorg, diff);
				f = 1 - exp(fogdensity/DotProduct(diff, diff));
				c2[0] = c[0] * f;
				c2[1] = c[1] * f;
				c2[2] = c[2] * f;
				c2[3] = c[3];
			}
		}
		else
		{
			for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c2 = (varray_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c2 += 4)
			{
				VectorSubtract(v, modelorg, diff);
				f = 1 - exp(fogdensity/DotProduct(diff, diff));
				c2[0] = f;
				c2[1] = f;
				c2[2] = f;
				c2[3] = 1;
			}
		}
		rsurface_lightmapcolor4f = varray_color4f;
	}
	if (applycolor && rsurface_lightmapcolor4f)
	{
		for (i = 0, c = (rsurface_lightmapcolor4f + 4 * surface->num_firstvertex), c2 = (varray_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, c += 4, c2 += 4)
		{
			c2[0] = c[0] * r;
			c2[1] = c[1] * g;
			c2[2] = c[2] * b;
			c2[3] = c[3] * a;
		}
	}
	R_Mesh_ColorPointer(rsurface_lightmapcolor4f);
	GL_Color(r, g, b, a);
}


static void R_DrawTextureSurfaceList(const entity_render_t *ent, texture_t *texture, int texturenumsurfaces, const msurface_t **texturesurfacelist, const vec3_t modelorg)
{
	int i;
	int texturesurfaceindex;
	const float *v;
	float *c;
	float diff[3];
	float colorpants[3], colorshirt[3];
	float f, r, g, b, a, colorscale;
	const msurface_t *surface;
	qboolean dolightmap;
	qboolean doambient;
	qboolean dodetail;
	qboolean doglow;
	qboolean dofogpass;
	qboolean fogallpasses;
	qboolean waterscrolling;
	qboolean dopants;
	qboolean doshirt;
	qboolean dofullbrightpants;
	qboolean dofullbrightshirt;
	qboolean applycolor;
	qboolean lightmodel = false;
	rtexture_t *basetexture;
	rmeshstate_t m;
	if (texture->currentmaterialflags & MATERIALFLAG_NODRAW)
		return;
	c_faces += texturenumsurfaces;
	// FIXME: identify models using a better check than ent->model->shadowmesh
	if (!(ent->effects & EF_FULLBRIGHT) && !ent->model->brush.shadowmesh)
		lightmodel = true;
	// gl_lightmaps debugging mode skips normal texturing
	if (gl_lightmaps.integer)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_DepthTest(true);
		qglDisable(GL_CULL_FACE);
		GL_Color(1, 1, 1, 1);
		memset(&m, 0, sizeof(m));
		R_Mesh_State(&m);
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			surface = texturesurfacelist[texturesurfaceindex];
			R_Mesh_TexBind(0, R_GetTexture(surface->lightmaptexture));
			R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordlightmap2f);
			RSurf_SetVertexPointer(ent, texture, surface, modelorg);
			RSurf_SetColorPointer(ent, surface, modelorg, 1, 1, 1, 1, lightmodel, !surface->lightmaptexture, false, false);
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
			GL_LockArrays(0, 0);
		}
		qglEnable(GL_CULL_FACE);
		return;
	}
	GL_DepthTest(!(texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST));
	GL_DepthMask(!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT));
	if (texture->currentmaterialflags & MATERIALFLAG_ADD)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	else if (texture->currentmaterialflags & MATERIALFLAG_ALPHA)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else
		GL_BlendFunc(GL_ONE, GL_ZERO);
	// water waterscrolling in texture matrix
	waterscrolling = (texture->currentmaterialflags & MATERIALFLAG_WATER) && r_waterscroll.value != 0;
	if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglDisable(GL_CULL_FACE);
	if (texture->currentmaterialflags & MATERIALFLAG_SKY)
	{
		if (skyrendernow)
		{
			skyrendernow = false;
			if (skyrendermasked)
				R_Sky();
		}
		// LordHavoc: HalfLife maps have freaky skypolys...
		//if (!ent->model->brush.ishlbsp)
		{
			R_Mesh_Matrix(&ent->matrix);
			GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], 1);
			if (skyrendermasked)
			{
				// depth-only (masking)
				GL_ColorMask(0,0,0,0);
				// just to make sure that braindead drivers don't draw anything
				// despite that colormask...
				GL_BlendFunc(GL_ZERO, GL_ONE);
			}
			else
			{
				// fog sky
				GL_BlendFunc(GL_ONE, GL_ZERO);
			}
			GL_DepthMask(true);
			GL_DepthTest(true);
			memset(&m, 0, sizeof(m));
			R_Mesh_State(&m);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
		}
	}
	else if ((texture->currentmaterialflags & MATERIALFLAG_WATER) && r_watershader.value && gl_textureshader && !texture->skin.glow && !fogenabled && ent->colormod[0] == 1 && ent->colormod[1] == 1 && ent->colormod[2] == 1)
	{
		// NVIDIA Geforce3 distortion texture shader on water
		float args[4] = {0.05f,0,0,0.04f};
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(r_texture_distorttexture[(int)(r_refdef.time * 16)&63]);
		m.tex[1] = R_GetTexture(texture->skin.base);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_REPLACE;
		Matrix4x4_CreateFromQuakeEntity(&m.texmatrix[0], 0, 0, 0, 0, 0, 0, r_watershader.value);
		m.texmatrix[1] = r_waterscrollmatrix;
		R_Mesh_State(&m);

		GL_Color(1, 1, 1, texture->currentalpha);
		GL_ActiveTexture(0);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(1);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);
		qglEnable(GL_TEXTURE_SHADER_NV);

		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			surface = texturesurfacelist[texturesurfaceindex];
			RSurf_SetVertexPointer(ent, texture, surface, modelorg);
			R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
			R_Mesh_TexCoordPointer(1, 2, surface->groupmesh->data_texcoordtexture2f);
			GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
			R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
			GL_LockArrays(0, 0);
		}

		qglDisable(GL_TEXTURE_SHADER_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(0);
	}
	else if (texture->currentmaterialflags & (MATERIALFLAG_WATER | MATERIALFLAG_WALL))
	{
		// normal surface (wall or water)
		dolightmap = !(texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT);
		doambient = r_ambient.value >= (1/64.0f);
		dodetail = r_detailtextures.integer && texture->skin.detail != NULL && !(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT);
		doglow = texture->skin.glow != NULL;
		dofogpass = fogenabled && !(texture->currentmaterialflags & MATERIALFLAG_ADD);
		fogallpasses = fogenabled && !(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT);
		if (ent->colormap >= 0)
		{
			int b;
			qbyte *bcolor;
			basetexture = texture->skin.base;
			dopants = texture->skin.pants != NULL;
			doshirt = texture->skin.shirt != NULL;
			// 128-224 are backwards ranges
			b = (ent->colormap & 0xF) << 4;b += (b >= 128 && b < 224) ? 4 : 12;
			dofullbrightpants = b >= 224;
			bcolor = (qbyte *) (&palette_complete[b]);
			VectorScale(bcolor, (1.0f / 255.0f), colorpants);
			// 128-224 are backwards ranges
			b = (ent->colormap & 0xF0);b += (b >= 128 && b < 224) ? 4 : 12;
			dofullbrightshirt = b >= 224;
			bcolor = (qbyte *) (&palette_complete[b]);
			VectorScale(bcolor, (1.0f / 255.0f), colorshirt);
		}
		else
		{
			basetexture = texture->skin.merged ? texture->skin.merged : texture->skin.base;
			dopants = false;
			doshirt = false;
			dofullbrightshirt = false;
			dofullbrightpants = false;
		}
		if (dolightmap && r_textureunits.integer >= 2 && gl_combine.integer)
		{
			memset(&m, 0, sizeof(m));
			m.tex[1] = R_GetTexture(basetexture);
			if (waterscrolling)
				m.texmatrix[1] = r_waterscrollmatrix;
			m.texrgbscale[1] = 2;
			m.pointer_color = varray_color4f;
			R_Mesh_State(&m);
			// transparent is not affected by r_lightmapintensity
			if (!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				colorscale = r_lightmapintensity;
			else
				colorscale = 1;
			r = ent->colormod[0] * colorscale;
			g = ent->colormod[1] * colorscale;
			b = ent->colormod[2] * colorscale;
			a = texture->currentalpha;
			// q3bsp has no lightmap updates, so the lightstylevalue that
			// would normally be baked into the lightmaptexture must be
			// applied to the color
			if (ent->model->brushq3.data_lightmaps)
			{
				float scale = d_lightstylevalue[0] * (1.0f / 128.0f);
				r *= scale;
				g *= scale;
				b *= scale;
			}
			applycolor = r != 1 || g != 1 || b != 1 || a != 1;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordlightmap2f);
				R_Mesh_TexCoordPointer(1, 2, surface->groupmesh->data_texcoordtexture2f);
				if (surface->lightmaptexture)
					R_Mesh_TexBind(0, R_GetTexture(surface->lightmaptexture));
				else
					R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
				RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, lightmodel, !surface->lightmaptexture, applycolor, fogallpasses);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
		else if (dolightmap && !(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT) && !lightmodel)
		{
			// single texture
			GL_BlendFunc(GL_ONE, GL_ZERO);
			GL_DepthMask(true);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			R_Mesh_State(&m);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordlightmap2f);
				if (surface->lightmaptexture)
				{
					R_Mesh_TexBind(0, R_GetTexture(surface->lightmaptexture));
					R_Mesh_ColorPointer(NULL);
				}
				else
				{
					R_Mesh_TexBind(0, R_GetTexture(r_texture_white));
					R_Mesh_ColorPointer(surface->groupmesh->data_lightmapcolor4f);
				}
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
			GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			GL_DepthMask(false);
			GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			R_Mesh_State(&m);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
		else
		{
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(basetexture);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 2;
			if (gl_combine.integer)
			{
				m.texrgbscale[0] = 2;
				colorscale = 1;
			}
			// transparent is not affected by r_lightmapintensity
			if (!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				colorscale *= r_lightmapintensity;
			R_Mesh_State(&m);
			r = ent->colormod[0] * colorscale;
			g = ent->colormod[1] * colorscale;
			b = ent->colormod[2] * colorscale;
			a = texture->currentalpha;
			if (dolightmap)
			{
				// q3bsp has no lightmap updates, so the lightstylevalue that
				// would normally be baked into the lightmaptexture must be
				// applied to the color
				if (ent->model->brushq3.data_lightmaps)
				{
					float scale = d_lightstylevalue[0] * (1.0f / 128.0f);
					r *= scale;
					g *= scale;
					b *= scale;
				}
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, lightmodel, true, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
			else
			{
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, false, false, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
		}
		if (dopants)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.pants);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 1;
			if (gl_combine.integer)
			{
				m.texrgbscale[0] = 2;
				colorscale *= 0.5f;
			}
			// transparent is not affected by r_lightmapintensity
			if (!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				colorscale *= r_lightmapintensity;
			R_Mesh_State(&m);
			r = ent->colormod[0] * colorpants[0] * colorscale;
			g = ent->colormod[1] * colorpants[1] * colorscale;
			b = ent->colormod[2] * colorpants[2] * colorscale;
			a = texture->currentalpha;
			if (dolightmap && !dofullbrightpants)
			{
				// q3bsp has no lightmap updates, so the lightstylevalue that
				// would normally be baked into the lightmaptexture must be
				// applied to the color
				if (ent->model->brushq3.data_lightmaps)
				{
					float scale = d_lightstylevalue[0] * (1.0f / 128.0f);
					r *= scale;
					g *= scale;
					b *= scale;
				}
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, lightmodel, true, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
			else
			{
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, false, false, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
		}
		if (doshirt)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.shirt);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 1;
			if (gl_combine.integer)
			{
				m.texrgbscale[0] = 2;
				colorscale *= 0.5f;
			}
			// transparent is not affected by r_lightmapintensity
			if (!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				colorscale *= r_lightmapintensity;
			R_Mesh_State(&m);
			r = ent->colormod[0] * colorshirt[0] * colorscale;
			g = ent->colormod[1] * colorshirt[1] * colorscale;
			b = ent->colormod[2] * colorshirt[2] * colorscale;
			a = texture->currentalpha;
			if (dolightmap && !dofullbrightshirt)
			{
				// q3bsp has no lightmap updates, so the lightstylevalue that
				// would normally be baked into the lightmaptexture must be
				// applied to the color
				if (ent->model->brushq3.data_lightmaps)
				{
					float scale = d_lightstylevalue[0] * (1.0f / 128.0f);
					r *= scale;
					g *= scale;
					b *= scale;
				}
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, lightmodel, true, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
			else
			{
				applycolor = r != 1 || g != 1 || b != 1 || a != 1;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					RSurf_SetVertexPointer(ent, texture, surface, modelorg);
					R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
					RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, false, false, applycolor, fogallpasses);
					GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
					R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
					GL_LockArrays(0, 0);
				}
			}
		}
		if (doambient)
		{
			doambient = false;
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.base);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 1;
			if (gl_combine.integer)
			{
				m.texrgbscale[0] = 2;
				colorscale *= 0.5f;
			}
			R_Mesh_State(&m);
			colorscale *= r_ambient.value * (1.0f / 64.0f);
			r = ent->colormod[0] * colorscale;
			g = ent->colormod[1] * colorscale;
			b = ent->colormod[2] * colorscale;
			a = texture->currentalpha;
			applycolor = r != 1 || g != 1 || b != 1 || a != 1;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
				RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, false, false, applycolor, fogallpasses);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
		if (dodetail)
		{
			GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			GL_DepthMask(false);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.detail);
			R_Mesh_State(&m);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoorddetail2f);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
		if (doglow)
		{
			// if glow was not already done using multitexture, do it now.
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.glow);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			R_Mesh_State(&m);
			colorscale = 1;
			r = ent->colormod[0] * colorscale;
			g = ent->colormod[1] * colorscale;
			b = ent->colormod[2] * colorscale;
			a = texture->currentalpha;
			applycolor = r != 1 || g != 1 || b != 1 || a != 1;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
				RSurf_SetColorPointer(ent, surface, modelorg, r, g, b, a, false, false, applycolor, fogallpasses);
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
		if (dofogpass)
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
			if (fogallpasses)
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.fog);
			if (waterscrolling)
				m.texmatrix[0] = r_waterscrollmatrix;
			R_Mesh_State(&m);
			r = fogcolor[0];
			g = fogcolor[1];
			b = fogcolor[2];
			a = texture->currentalpha;
			applycolor = r != 1 || g != 1 || b != 1 || a != 1;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				RSurf_SetVertexPointer(ent, texture, surface, modelorg);
				R_Mesh_TexCoordPointer(0, 2, surface->groupmesh->data_texcoordtexture2f);
				R_Mesh_ColorPointer(varray_color4f);
				//RSurf_FogPassColors_Vertex3f_Color4f((surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex), varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], texture->currentalpha, 1, surface->num_vertices, modelorg);
				if (!surface->lightmaptexture && surface->groupmesh->data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				{
					for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c = (varray_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
					{
						VectorSubtract(v, modelorg, diff);
						f = exp(fogdensity/DotProduct(diff, diff));
						c[0] = r;
						c[1] = g;
						c[2] = b;
						c[3] = (surface->groupmesh->data_lightmapcolor4f + 4 * surface->num_firstvertex)[i*4+3] * f * a;
					}
				}
				else
				{
					for (i = 0, v = (rsurface_vertex3f + 3 * surface->num_firstvertex), c = (varray_color4f + 4 * surface->num_firstvertex);i < surface->num_vertices;i++, v += 3, c += 4)
					{
						VectorSubtract(v, modelorg, diff);
						f = exp(fogdensity/DotProduct(diff, diff));
						c[0] = r;
						c[1] = g;
						c[2] = b;
						c[3] = f * a;
					}
				}
				GL_LockArrays(surface->num_firstvertex, surface->num_vertices);
				R_Mesh_Draw(surface->num_firstvertex, surface->num_vertices, surface->num_triangles, (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle));
				GL_LockArrays(0, 0);
			}
		}
	}
	if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglEnable(GL_CULL_FACE);
}

static void RSurfShader_Transparent_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surface = ent->model->data_surfaces + calldata2;
	vec3_t modelorg;
	texture_t *texture;

	texture = surface->texture;
	if (texture->basematerialflags & MATERIALFLAG_SKY)
		return; // transparent sky is too difficult
	R_UpdateTextureInfo(ent, texture);

	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	R_DrawTextureSurfaceList(ent, texture->currentframe, 1, &surface, modelorg);
}

void R_QueueTextureSurfaceList(entity_render_t *ent, texture_t *texture, int texturenumsurfaces, const msurface_t **texturesurfacelist, const vec3_t modelorg)
{
	int texturesurfaceindex;
	const msurface_t *surface;
	vec3_t tempcenter, center;
	if (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT)
	{
		// drawing sky transparently would be too difficult
		if (!(texture->currentmaterialflags & MATERIALFLAG_SKY))
		{
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
				tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
				tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
				Matrix4x4_Transform(&ent->matrix, tempcenter, center);
				R_MeshQueue_AddTransparent(texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST ? r_vieworigin : center, RSurfShader_Transparent_Callback, ent, surface - ent->model->data_surfaces);
			}
		}
	}
	else
		R_DrawTextureSurfaceList(ent, texture, texturenumsurfaces, texturesurfacelist, modelorg);
}

extern void R_BuildLightMap(const entity_render_t *ent, msurface_t *surface);
void R_DrawSurfaces(entity_render_t *ent, qboolean skysurfaces)
{
	int i, j, f, flagsmask;
	msurface_t *surface, **surfacechain;
	texture_t *t, *texture;
	model_t *model = ent->model;
	vec3_t modelorg;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	const msurface_t *surfacelist[1024];
	if (model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	// update light styles
	if (!skysurfaces && model->brushq1.light_styleupdatechains)
	{
		for (i = 0;i < model->brushq1.light_styles;i++)
		{
			if (model->brushq1.light_stylevalue[i] != d_lightstylevalue[model->brushq1.light_style[i]])
			{
				model->brushq1.light_stylevalue[i] = d_lightstylevalue[model->brushq1.light_style[i]];
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
	texture = NULL;
	numsurfacelist = 0;
	if (ent == r_refdef.worldentity)
	{
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (!r_worldsurfacevisible[j])
				continue;
			if (t != surface->texture)
			{
				if (numsurfacelist)
				{
					R_QueueTextureSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
				t = surface->texture;
				texture = t->currentframe;
				f = texture->currentmaterialflags & flagsmask;
			}
			if (f && surface->num_triangles)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight && surface->lightmapinfo->samples)
					R_BuildLightMap(ent, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_QueueTextureSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
			}
		}
	}
	else
	{
		for (i = 0, j = model->firstmodelsurface, surface = model->data_surfaces + j;i < model->nummodelsurfaces;i++, j++, surface++)
		{
			if (t != surface->texture)
			{
				if (numsurfacelist)
				{
					R_QueueTextureSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
				t = surface->texture;
				texture = t->currentframe;
				f = texture->currentmaterialflags & flagsmask;
			}
			if (f && surface->num_triangles)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight && surface->lightmapinfo->samples)
					R_BuildLightMap(ent, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_QueueTextureSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
			}
		}
	}
	if (numsurfacelist)
		R_QueueTextureSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
}

