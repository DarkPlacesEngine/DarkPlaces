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

// used for dlight push checking and other things
int r_framecount;

mplane_t frustum[4];

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
rtexturepool_t *r_main_texturepool;
rtexture_t *r_bloom_texture_screen;
rtexture_t *r_bloom_texture_bloom;
rtexture_t *r_texture_blanknormalmap;
rtexture_t *r_texture_white;
rtexture_t *r_texture_black;
rtexture_t *r_texture_notexture;

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

void gl_main_start(void)
{
	int x, y;
	qbyte pix[16][16][4];
	qbyte data[4];
	r_main_texturepool = R_AllocTexturePool();
	r_bloom_texture_screen = NULL;
	r_bloom_texture_bloom = NULL;
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
	r_texture_notexture = R_LoadTexture2D(mod_shared_texturepool, "notexture", 16, 16, &pix[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP, NULL);
}

void gl_main_shutdown(void)
{
	R_FreeTexturePool(&r_main_texturepool);
	r_bloom_texture_screen = NULL;
	r_bloom_texture_bloom = NULL;
	r_texture_blanknormalmap = NULL;
	r_texture_white = NULL;
	r_texture_black = NULL;
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
extern void Mod_RenderInit(void);
extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void R_Shadow_Init(void);
extern void GL_Models_Init(void);
extern void R_Sky_Init(void);
extern void GL_Surf_Init(void);
extern void R_Crosshairs_Init(void);
extern void R_Light_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void ui_init(void);
extern void gl_backend_init(void);
extern void Sbar_Init(void);
extern void R_LightningBeams_Init(void);

void Render_Init(void)
{
	R_Textures_Init();
	Mod_RenderInit();
	gl_backend_init();
	R_MeshQueue_Init();
	GL_Draw_Init();
	GL_Main_Init();
	R_Shadow_Init();
	GL_Models_Init();
	R_Sky_Init();
	GL_Surf_Init();
	R_Crosshairs_Init();
	R_Light_Init();
	R_Particles_Init();
	R_Explosion_Init();
	//ui_init();
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
	Con_DPrintf("\nengine extensions: %s\n", ENGINE_EXTENSIONS);

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
		for (screenwidth = 1;screenwidth < vid.realwidth;screenwidth *= 2);
		for (screenheight = 1;screenheight < vid.realheight;screenheight *= 2);
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
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.realheight - (r_view_y + r_view_height), r_view_width, r_view_height);
		c_bloomcopies++;
		c_bloomcopypixels += r_view_width * r_view_height;
		// now scale it down to the bloom size and raise to a power of itself
		// to darken it (this leaves the really bright stuff bright, and
		// everything else becomes very dark)
		// TODO: optimize with multitexture or GLSL
		qglViewport(r_view_x, vid.realheight - (r_view_y + bloomheight), bloomwidth, bloomheight);
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
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.realheight - (r_view_y + bloomheight), bloomwidth, bloomheight);
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
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.realheight - (r_view_y + bloomheight), bloomwidth, bloomheight);
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
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r_view_x, vid.realheight - (r_view_y + bloomheight), bloomwidth, bloomheight);
		c_bloomcopies++;
		c_bloomcopypixels += bloomwidth * bloomheight;
		// go back to full view area
		qglViewport(r_view_x, vid.realheight - (r_view_y + r_view_height), r_view_width, r_view_height);
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

/*
================
R_RenderView
================
*/
void R_RenderView(void)
{
	if (!r_refdef.entities/* || !r_refdef.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	r_view_width = bound(0, r_refdef.width, vid.realwidth);
	r_view_height = bound(0, r_refdef.height, vid.realheight);
	r_view_depth = 1;
	r_view_x = bound(0, r_refdef.x, vid.realwidth - r_refdef.width);
	r_view_y = bound(0, r_refdef.y, vid.realheight - r_refdef.height);
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
	qglViewport(r_view_x, vid.realheight - (r_view_y + r_view_height), r_view_width, r_view_height);
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

	GL_Scissor(0, 0, vid.realwidth, vid.realheight);
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

	if (r_shadow_visiblevolumes.integer && !r_showtrispass)
	{
		R_ShadowVolumeLighting(true);
		R_TimeReport("shadowvolume");
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

