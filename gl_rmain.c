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

// used for dlight push checking and other things
int r_framecount;

mplane_t frustum[4];

int c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;

// true during envmap command capture
qboolean envmap;

float r_farclip;

// view origin
vec3_t r_origin;
vec3_t vpn;
vec3_t vright;
vec3_t vup;

//
// screen size info
//
refdef_t r_refdef;

// 8.8 fraction of base light value
unsigned short d_lightstylevalue[256];

cvar_t r_drawentities = {0, "r_drawentities","1"};
cvar_t r_drawviewmodel = {0, "r_drawviewmodel","1"};
cvar_t r_speeds = {0, "r_speeds","0"};
cvar_t r_fullbright = {0, "r_fullbright","0"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};

cvar_t gl_fogenable = {0, "gl_fogenable", "0"};
cvar_t gl_fogdensity = {0, "gl_fogdensity", "0.25"};
cvar_t gl_fogred = {0, "gl_fogred","0.3"};
cvar_t gl_foggreen = {0, "gl_foggreen","0.3"};
cvar_t gl_fogblue = {0, "gl_fogblue","0.3"};
cvar_t gl_fogstart = {0, "gl_fogstart", "0"};
cvar_t gl_fogend = {0, "gl_fogend","0"};

cvar_t r_textureunits = {0, "r_textureunits", "32"};

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

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
qboolean intimerefresh = 0;
static void R_TimeRefresh_f (void)
{
	int i;
	float start, stop, time;

	intimerefresh = 1;
	start = Sys_DoubleTime ();
	for (i = 0;i < 128;i++)
	{
		r_refdef.viewangles[0] = 0;
		r_refdef.viewangles[1] = i/128.0*360.0;
		r_refdef.viewangles[2] = 0;
		CL_UpdateScreen();
	}

	stop = Sys_DoubleTime ();
	intimerefresh = 0;
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

extern cvar_t r_drawportals;

vec3_t fogcolor;
vec_t fogdensity;
float fog_density, fog_red, fog_green, fog_blue;
qboolean fogenabled;
qboolean oldgl_fogenable;
void R_SetupFog(void)
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
}

void gl_main_shutdown(void)
{
}

extern void CL_ParseEntityLump(char *entitystring);
void gl_main_newmap(void)
{
	if (cl.worldmodel && cl.worldmodel->entities)
		CL_ParseEntityLump(cl.worldmodel->entities);
	r_framecount = 1;
}

void GL_Main_Init(void)
{
// FIXME: move this to client?
	FOG_registercvars();
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_fullbrights);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_textureunits);
	if (gamemode == GAME_NEHAHRA)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);
}

vec3_t r_farclip_origin;
vec3_t r_farclip_direction;
vec_t r_farclip_directiondist;
vec_t r_farclip_meshfarclip;
int r_farclip_directionbit0;
int r_farclip_directionbit1;
int r_farclip_directionbit2;

// start a farclip measuring session
void R_FarClip_Start(vec3_t origin, vec3_t direction, vec_t startfarclip)
{
	VectorCopy(origin, r_farclip_origin);
	VectorCopy(direction, r_farclip_direction);
	r_farclip_directiondist = DotProduct(r_farclip_origin, r_farclip_direction);
	r_farclip_directionbit0 = r_farclip_direction[0] < 0;
	r_farclip_directionbit1 = r_farclip_direction[1] < 0;
	r_farclip_directionbit2 = r_farclip_direction[2] < 0;
	r_farclip_meshfarclip = r_farclip_directiondist + startfarclip;
}

// enlarge farclip to accomodate box
void R_FarClip_Box(vec3_t mins, vec3_t maxs)
{
	float d;
	d = (r_farclip_directionbit0 ? mins[0] : maxs[0]) * r_farclip_direction[0]
	  + (r_farclip_directionbit1 ? mins[1] : maxs[1]) * r_farclip_direction[1]
	  + (r_farclip_directionbit2 ? mins[2] : maxs[2]) * r_farclip_direction[2];
	if (r_farclip_meshfarclip < d)
		r_farclip_meshfarclip = d;
}

// return farclip value
float R_FarClip_Finish(void)
{
	return r_farclip_meshfarclip - r_farclip_directiondist;
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	R_Modules_NewMap();
}

extern void R_Textures_Init(void);
extern void Mod_RenderInit(void);
extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void GL_Models_Init(void);
extern void R_Sky_Init(void);
extern void GL_Surf_Init(void);
extern void R_Crosshairs_Init(void);
extern void R_Light_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void ui_init(void);
extern void gl_backend_init(void);

void Render_Init(void)
{
	R_Modules_Shutdown();
	R_Textures_Init();
	Mod_RenderInit();
	gl_backend_init();
	R_MeshQueue_Init();
	GL_Draw_Init();
	GL_Main_Init();
	GL_Models_Init();
	R_Sky_Init();
	GL_Surf_Init();
	R_Crosshairs_Init();
	R_Light_Init();
	R_Particles_Init();
	R_Explosion_Init();
	ui_init();
	R_Modules_Start();
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
	Con_Printf ("\nengine extensions: %s\n", ENGINE_EXTENSIONS);

	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);
}


//==================================================================================

static void R_MarkEntities (void)
{
	int i;
	vec3_t v;
	entity_render_t *ent;

	ent = &cl_entities[0].render;
	Matrix4x4_CreateIdentity(&ent->matrix);
	Matrix4x4_CreateIdentity(&ent->inversematrix);

	if (cl.worldmodel)
		R_FarClip_Box(cl.worldmodel->normalmins, cl.worldmodel->normalmaxs);

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		ent = r_refdef.entities[i];
		Mod_CheckLoaded(ent->model);

		// move view-relative models to where they should be
		if (ent->flags & RENDER_VIEWMODEL)
		{
			// remove flag so it will not be repeated incase RelinkEntities is not called again for a while
			ent->flags -= RENDER_VIEWMODEL;
			// transform origin
			VectorCopy(ent->origin, v);
			ent->origin[0] = v[0] * vpn[0] + v[1] * vright[0] + v[2] * vup[0] + r_origin[0];
			ent->origin[1] = v[0] * vpn[1] + v[1] * vright[1] + v[2] * vup[1] + r_origin[1];
			ent->origin[2] = v[0] * vpn[2] + v[1] * vright[2] + v[2] * vup[2] + r_origin[2];
			// adjust angles
			VectorAdd(ent->angles, r_refdef.viewangles, ent->angles);
		}

		if (R_CullBox(ent->mins, ent->maxs))
			continue;

		ent->visframe = r_framecount;
		VectorCopy(ent->angles, v);
		if (!ent->model || ent->model->type != mod_brush)
			v[0] = -v[0];
		Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin[0], ent->origin[1], ent->origin[2], v[0], v[1], v[2], ent->scale);
		Matrix4x4_Invert_Simple(&ent->inversematrix, &ent->matrix);
		R_LerpAnimation(ent);
		R_UpdateEntLights(ent);
		R_FarClip_Box(ent->mins, ent->maxs);
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
void R_DrawModels (void)
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
			if (ent->model)
			{
				if (ent->model->Draw)
					ent->model->Draw(ent);
			}
			else
				R_DrawNoModel(ent);
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	entity_render_t *ent;

	// FIXME: move these checks to client
	if (!r_drawviewmodel.integer || chase_active.integer || envmap || !r_drawentities.integer || cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0 || !cl.viewent.render.model)
		return;

	ent = &cl.viewent.render;
	Mod_CheckLoaded(ent->model);
	R_LerpAnimation(ent);
	Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin[0], ent->origin[1], ent->origin[2], -ent->angles[0], ent->angles[1], ent->angles[2], ent->scale);
	Matrix4x4_Invert_Simple(&ent->inversematrix, &ent->matrix);
	R_UpdateEntLights(ent);
	ent->model->Draw(ent);
}

static void R_SetFrustum (void)
{
	int i;

	// LordHavoc: note to all quake engine coders, the special case for 90
	// degrees assumed a square view (wrong), so I removed it, Quake2 has it
	// disabled as well.
	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for (i = 0;i < 4;i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		PlaneClassify(&frustum[i]);
	}
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		if (r_fullbright.integer != 0)
			Cvar_Set ("r_fullbright", "0");
		if (r_ambient.value != 0)
			Cvar_Set ("r_ambient", "0");
	}

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	R_AnimateLight ();
}


static void R_BlendView(void)
{
	rmeshbufferinfo_t m;
	float r;

	if (r_refdef.viewblend[3] < 0.01f)
		return;

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.depthdisable = true; // magic
	m.numtriangles = 1;
	m.numverts = 3;
	Matrix4x4_CreateIdentity(&m.matrix);
	if (R_Mesh_Draw_GetBuffer(&m, false))
	{
		m.index[0] = 0;
		m.index[1] = 1;
		m.index[2] = 2;
		m.color[0] = m.color[4] = m.color[8] = r_refdef.viewblend[0];
		m.color[1] = m.color[5] = m.color[9] = r_refdef.viewblend[1];
		m.color[2] = m.color[6] = m.color[10] = r_refdef.viewblend[2];
		m.color[3] = m.color[7] = m.color[11] = r_refdef.viewblend[3];
		r = 64000;
		m.vertex[0] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r - vup[0] * r;
		m.vertex[1] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r - vup[1] * r;
		m.vertex[2] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r - vup[2] * r;
		r *= 3;
		m.vertex[4] = m.vertex[0] + vup[0] * r;
		m.vertex[5] = m.vertex[1] + vup[1] * r;
		m.vertex[6] = m.vertex[2] + vup[2] * r;
		m.vertex[8] = m.vertex[0] + vright[0] * r;
		m.vertex[9] = m.vertex[1] + vright[1] * r;
		m.vertex[10] = m.vertex[2] + vright[2] * r;
		R_Mesh_Render();
	}
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	entity_render_t *world;
	if (!r_refdef.entities/* || !cl.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	world = &cl_entities[0].render;

	// FIXME: move to client
	R_MoveExplosions();
	R_TimeReport("mexplosion");

	R_Textures_Frame();
	R_SetupFrame();
	R_SetFrustum();
	R_SetupFog();
	R_SkyStartFrame();
	R_BuildLightList();
	R_TimeReport("setup");

	R_FarClip_Start(r_origin, vpn, 768.0f);
	R_MarkEntities();
	r_farclip = R_FarClip_Finish() + 256.0f;
	R_TimeReport("markentity");

	R_Mesh_Start(r_farclip);
	R_MeshQueue_BeginScene();

	if (R_DrawBrushModelsSky())
		R_TimeReport("bmodelsky");

	if (world->model)
	{
		R_DrawWorld(world);
		R_TimeReport("worldnode");

		R_SurfMarkLights(world);
		R_TimeReport("marklights");

		R_PrepareSurfaces(world);
		R_TimeReport("surfprep");

		R_DrawSurfaces(world, SHADERSTAGE_SKY);
		R_DrawSurfaces(world, SHADERSTAGE_NORMAL);
		R_TimeReport("surfdraw");

		if (r_drawportals.integer)
		{
			R_DrawPortals(world);
			R_TimeReport("portals");
		}
	}

	// don't let sound skip if going slow
	if (!intimerefresh && !r_speeds.integer)
		S_ExtraUpdate ();

	R_DrawViewModel();
	R_TimeReport("viewmodel");

	R_DrawModels();
	R_TimeReport("models");

	R_DrawParticles();
	R_TimeReport("particles");

	R_DrawExplosions();
	R_TimeReport("explosions");

	R_MeshQueue_RenderTransparent();
	R_TimeReport("addtrans");

	R_DrawCoronas();
	R_TimeReport("coronas");

	R_DrawCrosshair();
	R_TimeReport("crosshair");

	R_BlendView();
	R_TimeReport("blendview");

	R_MeshQueue_Render();
	R_MeshQueue_EndScene();
	R_Mesh_Finish();
	R_TimeReport("meshfinish");
}

void R_DrawBBoxMesh(vec3_t mins, vec3_t maxs, float cr, float cg, float cb, float ca)
{
	int i;
	float *v, *c, f1, f2, diff[3];
	rmeshbufferinfo_t m;
	m.numtriangles = 12;
	m.numverts = 8;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	Matrix4x4_CreateIdentity(&m.matrix);
	if (R_Mesh_Draw_GetBuffer(&m, false))
	{
		m.vertex[ 0] = mins[0];m.vertex[ 1] = mins[1];m.vertex[ 2] = mins[2];
		m.vertex[ 4] = maxs[0];m.vertex[ 5] = mins[1];m.vertex[ 6] = mins[2];
		m.vertex[ 8] = mins[0];m.vertex[ 9] = maxs[1];m.vertex[10] = mins[2];
		m.vertex[12] = maxs[0];m.vertex[13] = maxs[1];m.vertex[14] = mins[2];
		m.vertex[16] = mins[0];m.vertex[17] = mins[1];m.vertex[18] = maxs[2];
		m.vertex[20] = maxs[0];m.vertex[21] = mins[1];m.vertex[22] = maxs[2];
		m.vertex[24] = mins[0];m.vertex[25] = maxs[1];m.vertex[26] = maxs[2];
		m.vertex[28] = maxs[0];m.vertex[29] = maxs[1];m.vertex[30] = maxs[2];
		m.color[ 0] = m.color[ 4] = m.color[ 8] = m.color[12] = m.color[16] = m.color[20] = m.color[24] = m.color[28] = cr * m.colorscale;
		m.color[ 1] = m.color[ 5] = m.color[ 9] = m.color[13] = m.color[17] = m.color[21] = m.color[25] = m.color[29] = cg * m.colorscale;
		m.color[ 2] = m.color[ 6] = m.color[10] = m.color[14] = m.color[18] = m.color[22] = m.color[26] = m.color[30] = cb * m.colorscale;
		m.color[ 3] = m.color[ 7] = m.color[11] = m.color[15] = m.color[19] = m.color[23] = m.color[27] = m.color[31] = ca;
		if (fogenabled)
		{
			for (i = 0, v = m.vertex, c = m.color;i < m.numverts;i++, v += 4, c += 4)
			{
				VectorSubtract(v, r_origin, diff);
				f2 = exp(fogdensity/DotProduct(diff, diff));
				f1 = 1 - f2;
				f2 *= m.colorscale;
				c[0] = c[0] * f1 + fogcolor[0] * f2;
				c[1] = c[1] * f1 + fogcolor[1] * f2;
				c[2] = c[2] * f1 + fogcolor[2] * f2;
			}
		}
		R_Mesh_Render();
	}
}

void R_DrawNoModelCallback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	int i;
	float f1, f2, *c, diff[3];
	rmeshbufferinfo_t m;
	memset(&m, 0, sizeof(m));
	if (ent->flags & EF_ADDITIVE)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (ent->alpha < 1)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.numtriangles = 8;
	m.numverts = 6;
	m.matrix = ent->matrix;
	if (R_Mesh_Draw_GetBuffer(&m, false))
	{
		m.index[ 0] = 5;m.index[ 1] = 2;m.index[ 2] = 0;
		m.index[ 3] = 5;m.index[ 4] = 1;m.index[ 5] = 2;
		m.index[ 6] = 5;m.index[ 7] = 0;m.index[ 8] = 3;
		m.index[ 9] = 5;m.index[10] = 3;m.index[11] = 1;
		m.index[12] = 0;m.index[13] = 2;m.index[14] = 4;
		m.index[15] = 2;m.index[16] = 1;m.index[17] = 4;
		m.index[18] = 3;m.index[19] = 0;m.index[20] = 4;
		m.index[21] = 1;m.index[22] = 3;m.index[23] = 4;
		m.vertex[ 0] = -16;m.vertex[ 1] =   0;m.vertex[ 2] =   0;
		m.vertex[ 4] =  16;m.vertex[ 5] =   0;m.vertex[ 6] =   0;
		m.vertex[ 8] =   0;m.vertex[ 9] = -16;m.vertex[10] =   0;
		m.vertex[12] =   0;m.vertex[13] =  16;m.vertex[14] =   0;
		m.vertex[16] =   0;m.vertex[17] =   0;m.vertex[18] = -16;
		m.vertex[20] =   0;m.vertex[21] =   0;m.vertex[22] =  16;
		m.color[ 0] = 0.00f;m.color[ 1] = 0.00f;m.color[ 2] = 0.50f;m.color[ 3] = ent->alpha;
		m.color[ 4] = 0.00f;m.color[ 5] = 0.00f;m.color[ 6] = 0.50f;m.color[ 7] = ent->alpha;
		m.color[ 8] = 0.00f;m.color[ 9] = 0.50f;m.color[10] = 0.00f;m.color[11] = ent->alpha;
		m.color[12] = 0.00f;m.color[13] = 0.50f;m.color[14] = 0.00f;m.color[15] = ent->alpha;
		m.color[16] = 0.50f;m.color[17] = 0.00f;m.color[18] = 0.00f;m.color[19] = ent->alpha;
		m.color[20] = 0.50f;m.color[21] = 0.00f;m.color[22] = 0.00f;m.color[23] = ent->alpha;
		if (fogenabled)
		{
			VectorSubtract(ent->origin, r_origin, diff);
			f2 = exp(fogdensity/DotProduct(diff, diff));
			f1 = 1 - f2;
			for (i = 0, c = m.color;i < m.numverts;i++, c += 4)
			{
				c[0] = (c[0] * f1 + fogcolor[0] * f2) * m.colorscale;
				c[1] = (c[1] * f1 + fogcolor[1] * f2) * m.colorscale;
				c[2] = (c[2] * f1 + fogcolor[2] * f2) * m.colorscale;
			}
		}
		else
		{
			for (i = 0, c = m.color;i < m.numverts;i++, c += 4)
			{
				c[0] *= m.colorscale;
				c[1] *= m.colorscale;
				c[2] *= m.colorscale;
			}
		}
		R_Mesh_Render();
	}
}

void R_DrawNoModel(entity_render_t *ent)
{
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->origin, R_DrawNoModelCallback, ent, 0);
	//else
	//	R_DrawNoModelCallback(ent, 0);
}

