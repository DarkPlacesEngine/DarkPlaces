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

// used for visibility checking
qbyte r_pvsbits[(MAX_MAP_LEAFS+7)>>3];

mplane_t frustum[4];

matrix4x4_t r_identitymatrix;

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
cvar_t r_shadows = {CVAR_SAVE, "r_shadows", "0"};
cvar_t r_shadow_staticworldlights = {0, "r_shadow_staticworldlights", "1"};
cvar_t r_speeds = {0, "r_speeds","0"};
cvar_t r_fullbright = {0, "r_fullbright","0"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};
cvar_t r_shadow_cull = {0, "r_shadow_cull", "1"};
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
	int l;
	char *entities, entname[MAX_QPATH];
	r_framecount = 1;
	if (cl.worldmodel)
	{
		strcpy(entname, cl.worldmodel->name);
		l = strlen(entname) - 4;
		if (l >= 0 && !strcmp(entname + l, ".bsp"))
		{
			strcpy(entname + l, ".ent");
			if ((entities = FS_LoadFile(entname, true)))
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
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_shadow_staticworldlights);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_fullbrights);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_textureunits);
	Cvar_RegisterVariable(&r_shadow_cull);
	Cvar_RegisterVariable(&r_lerpsprites);
	Cvar_RegisterVariable(&r_lerpmodels);
	Cvar_RegisterVariable(&r_waterscroll);
	Cvar_RegisterVariable(&r_watershader);
	Cvar_RegisterVariable(&r_drawcollisionbrushes);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_NEXUIZ)
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
	ui_init();
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

#define VIS_CullBox(mins,maxs) (R_CullBox((mins), (maxs)) || (cl.worldmodel && cl.worldmodel->brush.BoxTouchingPVS && !cl.worldmodel->brush.BoxTouchingPVS(cl.worldmodel, r_pvsbits, (mins), (maxs))))

//==================================================================================

static void R_MarkEntities (void)
{
	int i;
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
		// some of the renderer still relies on origin...
		Matrix4x4_OriginFromMatrix(&ent->matrix, ent->origin);
		// some of the renderer still relies on scale...
		ent->scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
		R_LerpAnimation(ent);
		R_UpdateEntLights(ent);
		if ((chase_active.integer || !(ent->flags & RENDER_EXTERIORMODEL))
		 && !VIS_CullBox(ent->mins, ent->maxs))
		{
			ent->visframe = r_framecount;
			R_FarClip_Box(ent->mins, ent->maxs);
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

/*
=============
R_DrawViewModel
=============
*/
/*
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
*/

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

void R_DrawFakeShadows(void)
{
	int i;
	entity_render_t *ent;

	ent = &cl_entities[0].render;
	if (ent->model && ent->model->DrawFakeShadow)
		ent->model->DrawFakeShadow(ent);

	if (!r_drawentities.integer)
		return;
	for (i = 0;i < r_refdef.numentities;i++)
	{
		ent = r_refdef.entities[i];
		if ((ent->flags & RENDER_SHADOW) && ent->model && ent->model->DrawFakeShadow)
			ent->model->DrawFakeShadow(ent);
	}
}

#include "r_shadow.h"

int shadowframecount = 0;

void R_TestAndDrawShadowVolume(entity_render_t *ent, vec3_t lightorigin, float cullradius, float lightradius, vec3_t lightmins, vec3_t lightmaxs, vec3_t clipmins, vec3_t clipmaxs, int lightmarked)
{
	vec3_t relativelightorigin;
	// rough checks
	if ((ent->flags & RENDER_SHADOW) && ent->model && ent->model->DrawShadowVolume && !(r_shadow_cull.integer && (ent->maxs[0] < lightmins[0] || ent->mins[0] > lightmaxs[0] || ent->maxs[1] < lightmins[1] || ent->mins[1] > lightmaxs[1] || ent->maxs[2] < lightmins[2] || ent->mins[2] > lightmaxs[2])))
	{
		Matrix4x4_Transform(&ent->inversematrix, lightorigin, relativelightorigin);
		ent->model->DrawShadowVolume (ent, relativelightorigin, lightradius);
	}
}

void R_Shadow_DrawWorldLightShadowVolume(matrix4x4_t *matrix, worldlight_t *light);

extern void R_Model_Brush_DrawLightForSurfaceList(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, msurface_t **surflist, int numsurfaces, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void R_ShadowVolumeLighting(int visiblevolumes)
{
	int i;
	entity_render_t *ent;
	int lnum;
	float f, lightradius, cullradius;
	vec3_t relativelightorigin, relativeeyeorigin, lightcolor, clipmins, clipmaxs;
	worldlight_t *wl;
	rdlight_t *rd;
	rmeshstate_t m;
	matrix4x4_t matrix;
	matrix4x4_t matrix_worldtofilter, matrix_worldtoattenuationxyz, matrix_worldtoattenuationz;
	matrix4x4_t matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz;

	if (visiblevolumes)
	{
		memset(&m, 0, sizeof(m));
		R_Mesh_State_Texture(&m);

		GL_BlendFunc(GL_ONE, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(r_shadow_visiblevolumes.integer < 2);
		qglDisable(GL_CULL_FACE);
		GL_Color(0.0, 0.0125, 0.1, 1);
	}
	else
		R_Shadow_Stage_Begin();
	shadowframecount++;
	if (r_shadow_realtime_world.integer)
	{
		R_Shadow_LoadWorldLightsIfNeeded();
		for (lnum = 0, wl = r_shadow_worldlightchain;wl;wl = wl->next, lnum++)
		{
			if (d_lightstylevalue[wl->style] <= 0)
				continue;
			if (VIS_CullBox(wl->mins, wl->maxs))
				continue;
			if (r_shadow_debuglight.integer >= 0 && lnum != r_shadow_debuglight.integer)
				continue;
			if (R_Shadow_ScissorForBBox(wl->mins, wl->maxs))
				continue;

			cullradius = wl->cullradius;
			lightradius = wl->lightradius;
			VectorCopy(wl->mins, clipmins);
			VectorCopy(wl->maxs, clipmaxs);

			f = d_lightstylevalue[wl->style] * (1.0f / 256.0f);
			VectorScale(wl->light, f, lightcolor);
			if (wl->selected)
			{
				f = 2 + sin(realtime * M_PI * 4.0);
				VectorScale(lightcolor, f, lightcolor);
			}

			if (wl->castshadows && (gl_stencil || visiblevolumes))
			{
				if (!visiblevolumes)
					R_Shadow_Stage_ShadowVolumes();
				ent = &cl_entities[0].render;
				if (wl->shadowvolume && r_shadow_staticworldlights.integer)
					R_Shadow_DrawWorldLightShadowVolume(&ent->matrix, wl);
				else
					R_TestAndDrawShadowVolume(ent, wl->origin, cullradius, lightradius, wl->mins, wl->maxs, clipmins, clipmaxs, true);
				if (r_drawentities.integer)
					for (i = 0;i < r_refdef.numentities;i++)
						R_TestAndDrawShadowVolume(r_refdef.entities[i], wl->origin, cullradius, lightradius, wl->mins, wl->maxs, clipmins, clipmaxs, true);
			}

			if (!visiblevolumes)
			{
				if (wl->castshadows && gl_stencil)
					R_Shadow_Stage_LightWithShadows();
				else
					R_Shadow_Stage_LightWithoutShadows();

				// calculate world to filter matrix
				Matrix4x4_CreateFromQuakeEntity(&matrix, wl->origin[0], wl->origin[1], wl->origin[2], wl->angles[0], wl->angles[1], wl->angles[2], lightradius);
				Matrix4x4_Invert_Simple(&matrix_worldtofilter, &matrix);
				// calculate world to attenuationxyz/xy matrix
				Matrix4x4_CreateFromQuakeEntity(&matrix, 0.5, 0.5, 0.5, 0, 0, 0, 0.5);
				Matrix4x4_Concat(&matrix_worldtoattenuationxyz, &matrix, &matrix_worldtofilter);
				// calculate world to attenuationz matrix
				matrix.m[0][0] = 0;matrix.m[0][1] = 0;matrix.m[0][2] = 0.5;matrix.m[0][3] = 0.5;
				matrix.m[1][0] = 0;matrix.m[1][1] = 0;matrix.m[1][2] = 0  ;matrix.m[1][3] = 0.5;
				matrix.m[2][0] = 0;matrix.m[2][1] = 0;matrix.m[2][2] = 0  ;matrix.m[2][3] = 0.5;
				matrix.m[3][0] = 0;matrix.m[3][1] = 0;matrix.m[3][2] = 0  ;matrix.m[3][3] = 1;
				Matrix4x4_Concat(&matrix_worldtoattenuationz, &matrix, &matrix_worldtofilter);

				ent = &cl_entities[0].render;
				if (ent->model && ent->model->DrawLight)
				{
					Matrix4x4_Transform(&ent->inversematrix, wl->origin, relativelightorigin);
					Matrix4x4_Transform(&ent->inversematrix, r_origin, relativeeyeorigin);
					Matrix4x4_Concat(&matrix_modeltofilter, &matrix_worldtofilter, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &matrix_worldtoattenuationxyz, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationz, &matrix_worldtoattenuationz, &ent->matrix);
					if (wl->numsurfaces)
						R_Model_Brush_DrawLightForSurfaceList(ent, relativelightorigin, relativeeyeorigin, lightradius, lightcolor, wl->surfaces, wl->numsurfaces, &matrix_modeltofilter, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz);
					else
						ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, lightradius / ent->scale, lightcolor, &matrix_modeltofilter, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz);
				}
				if (r_drawentities.integer)
				{
					for (i = 0;i < r_refdef.numentities;i++)
					{
						ent = r_refdef.entities[i];
						if (ent->visframe == r_framecount && ent->model && ent->model->DrawLight
						 && BoxesOverlap(ent->mins, ent->maxs, clipmins, clipmaxs)
						 && !(ent->effects & EF_ADDITIVE) && ent->alpha == 1)
						{
							Matrix4x4_Transform(&ent->inversematrix, wl->origin, relativelightorigin);
							Matrix4x4_Transform(&ent->inversematrix, r_origin, relativeeyeorigin);
							Matrix4x4_Concat(&matrix_modeltofilter, &matrix_worldtofilter, &ent->matrix);
							Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &matrix_worldtoattenuationxyz, &ent->matrix);
							Matrix4x4_Concat(&matrix_modeltoattenuationz, &matrix_worldtoattenuationz, &ent->matrix);
							ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, lightradius / ent->scale, lightcolor, &matrix_modeltofilter, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz);
						}
					}
				}
			}
		}
	}
	if (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer)
	{
		for (lnum = 0, rd = r_dlight;lnum < r_numdlights;lnum++, rd++)
		{
			lightradius = rd->cullradius;
			clipmins[0] = rd->origin[0] - lightradius;
			clipmins[1] = rd->origin[1] - lightradius;
			clipmins[2] = rd->origin[2] - lightradius;
			clipmaxs[0] = rd->origin[0] + lightradius;
			clipmaxs[1] = rd->origin[1] + lightradius;
			clipmaxs[2] = rd->origin[2] + lightradius;
			if (VIS_CullBox(clipmins, clipmaxs) || R_Shadow_ScissorForBBox(clipmins, clipmaxs))
				continue;

			cullradius = RadiusFromBoundsAndOrigin(clipmins, clipmaxs, rd->origin);
			VectorScale(rd->light, (1.0f / 4096.0f), lightcolor);

			if (gl_stencil || visiblevolumes)
			{
				if (!visiblevolumes)
					R_Shadow_Stage_ShadowVolumes();
				ent = &cl_entities[0].render;
				R_TestAndDrawShadowVolume(ent, rd->origin, cullradius, lightradius, clipmins, clipmaxs, clipmins, clipmaxs, false);
				if (r_drawentities.integer)
				{
					for (i = 0;i < r_refdef.numentities;i++)
					{
						ent = r_refdef.entities[i];
						if (ent != rd->ent)
							R_TestAndDrawShadowVolume(ent, rd->origin, cullradius, lightradius, clipmins, clipmaxs, clipmins, clipmaxs, false);
					}
				}
			}

			if (!visiblevolumes)
			{
				if (gl_stencil)
					R_Shadow_Stage_LightWithShadows();
				else
					R_Shadow_Stage_LightWithoutShadows();

				// calculate world to filter matrix
				Matrix4x4_CreateFromQuakeEntity(&matrix, rd->origin[0], rd->origin[1], rd->origin[2], 0, 0, 0, lightradius);
				Matrix4x4_Invert_Simple(&matrix_worldtofilter, &matrix);
				// calculate world to attenuationxyz/xy matrix
				Matrix4x4_CreateFromQuakeEntity(&matrix, 0.5, 0.5, 0.5, 0, 0, 0, 0.5);
				Matrix4x4_Concat(&matrix_worldtoattenuationxyz, &matrix, &matrix_worldtofilter);
				// calculate world to attenuationz matrix
				matrix.m[0][0] = 0;matrix.m[0][1] = 0;matrix.m[0][2] = 0.5;matrix.m[0][3] = 0.5;
				matrix.m[1][0] = 0;matrix.m[1][1] = 0;matrix.m[1][2] = 0  ;matrix.m[1][3] = 0.5;
				matrix.m[2][0] = 0;matrix.m[2][1] = 0;matrix.m[2][2] = 0  ;matrix.m[2][3] = 0.5;
				matrix.m[3][0] = 0;matrix.m[3][1] = 0;matrix.m[3][2] = 0  ;matrix.m[3][3] = 1;
				Matrix4x4_Concat(&matrix_worldtoattenuationz, &matrix, &matrix_worldtofilter);

				ent = &cl_entities[0].render;
				if (ent->model && ent->model->DrawLight)
				{
					Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
					Matrix4x4_Transform(&ent->inversematrix, r_origin, relativeeyeorigin);
					Matrix4x4_Concat(&matrix_modeltofilter, &matrix_worldtofilter, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &matrix_worldtoattenuationxyz, &ent->matrix);
					Matrix4x4_Concat(&matrix_modeltoattenuationz, &matrix_worldtoattenuationz, &ent->matrix);
					ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, lightradius / ent->scale, lightcolor, &matrix_modeltofilter, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz);
				}
				if (r_drawentities.integer)
				{
					for (i = 0;i < r_refdef.numentities;i++)
					{
						ent = r_refdef.entities[i];
						if (ent->visframe == r_framecount && ent->model && ent->model->DrawLight
						 && BoxesOverlap(ent->mins, ent->maxs, clipmins, clipmaxs)
						 && !(ent->effects & EF_ADDITIVE) && ent->alpha == 1)
						{
							Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
							Matrix4x4_Transform(&ent->inversematrix, r_origin, relativeeyeorigin);
							Matrix4x4_Concat(&matrix_modeltofilter, &matrix_worldtofilter, &ent->matrix);
							Matrix4x4_Concat(&matrix_modeltoattenuationxyz, &matrix_worldtoattenuationxyz, &ent->matrix);
							Matrix4x4_Concat(&matrix_modeltoattenuationz, &matrix_worldtoattenuationz, &ent->matrix);
							ent->model->DrawLight(ent, relativelightorigin, relativeeyeorigin, lightradius / ent->scale, lightcolor, &matrix_modeltofilter, &matrix_modeltoattenuationxyz, &matrix_modeltoattenuationz);
						}
					}
				}
			}
		}
	}

	if (visiblevolumes)
		qglEnable(GL_CULL_FACE);
	else
		R_Shadow_Stage_End();
	qglDisable(GL_SCISSOR_TEST);
}

static void R_SetFrustum (void)
{
	// LordHavoc: note to all quake engine coders, the special case for 90
	// degrees assumed a square view (wrong), so I removed it, Quake2 has it
	// disabled as well.

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	frustum[0].dist = DotProduct (r_origin, frustum[0].normal);
	PlaneClassify(&frustum[0]);

	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	frustum[1].dist = DotProduct (r_origin, frustum[1].normal);
	PlaneClassify(&frustum[1]);

	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	frustum[2].dist = DotProduct (r_origin, frustum[2].normal);
	PlaneClassify(&frustum[2]);

	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	frustum[3].dist = DotProduct (r_origin, frustum[3].normal);
	PlaneClassify(&frustum[3]);
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (!cl.islocalgame)
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
	rmeshstate_t m;
	float r;
	float vertex3f[3*3];

	if (r_refdef.viewblend[3] < 0.01f)
		return;

	R_Mesh_Matrix(&r_identitymatrix);

	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(true);
	GL_DepthTest(false); // magic
	GL_VertexPointer(vertex3f);
	GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
	r = 64000;
	vertex3f[0] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r - vup[0] * r;
	vertex3f[1] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r - vup[1] * r;
	vertex3f[2] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r - vup[2] * r;
	vertex3f[3] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r + vup[0] * r * 3;
	vertex3f[4] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r + vup[1] * r * 3;
	vertex3f[5] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r + vup[2] * r * 3;
	vertex3f[6] = r_origin[0] + vpn[0] * 1.5 + vright[0] * r * 3 - vup[0] * r;
	vertex3f[7] = r_origin[1] + vpn[1] * 1.5 + vright[1] * r * 3 - vup[1] * r;
	vertex3f[8] = r_origin[2] + vpn[2] * 1.5 + vright[2] * r * 3 - vup[2] * r;
	R_Mesh_Draw(3, 1, polygonelements);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
extern void R_DrawLightningBeams (void);
void R_RenderView (void)
{
	entity_render_t *world;
	if (!r_refdef.entities/* || !cl.worldmodel*/)
		return; //Host_Error ("R_RenderView: NULL worldmodel");

	if (r_shadow_realtime_world.integer)
	{
		if (!gl_stencil)
		{
			Con_Printf("Stencil not enabled, turning off r_shadow_realtime_world, please type vid_stencil 1;vid_bitsperpixel 32;vid_restart and try again\n");
			Cvar_SetValueQuick(&r_shadow_realtime_world, 0);
		}
	}

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

	if (cl.worldmodel && cl.worldmodel->brush.FatPVS)
		cl.worldmodel->brush.FatPVS(cl.worldmodel, r_origin, 2, r_pvsbits, sizeof(r_pvsbits));

	R_WorldVisibility(world);
	R_TimeReport("worldvis");

	R_FarClip_Start(r_origin, vpn, 768.0f);
	R_MarkEntities();
	r_farclip = R_FarClip_Finish() + 256.0f;
	R_TimeReport("markentity");

	GL_SetupView_ViewPort(r_refdef.x, r_refdef.y, r_refdef.width, r_refdef.height);
	if (r_shadow_realtime_world.integer || gl_stencil)
		GL_SetupView_Mode_PerspectiveInfiniteFarClip(r_refdef.fov_x, r_refdef.fov_y, 1.0f);
	else
		GL_SetupView_Mode_Perspective(r_refdef.fov_x, r_refdef.fov_y, 1.0f, r_farclip);
	GL_SetupView_Orientation_FromEntity (r_refdef.vieworg, r_refdef.viewangles);
	qglDepthFunc(GL_LEQUAL);

	R_Mesh_Start();
	R_MeshQueue_BeginScene();

	R_Shadow_UpdateWorldLightSelection();

	if (R_DrawBrushModelsSky())
		R_TimeReport("bmodelsky");

	// must occur early because it can draw sky
	R_DrawWorld(world);
	R_TimeReport("world");

	// don't let sound skip if going slow
	if (!intimerefresh && !r_speeds.integer)
		S_ExtraUpdate ();

	R_DrawModels();
	R_TimeReport("models");

	if (r_shadows.integer == 1 && !r_shadow_realtime_world.integer)
	{
		R_DrawFakeShadows();
		R_TimeReport("fakeshadow");
	}

	if (r_shadow_realtime_world.integer || r_shadow_realtime_dlight.integer)
	{
		R_ShadowVolumeLighting(false);
		R_TimeReport("dynlight");
	}

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

	R_BlendView();
	R_TimeReport("blendview");

	R_MeshQueue_Render();
	R_MeshQueue_EndScene();

	if (r_shadow_visiblevolumes.integer)
	{
		R_ShadowVolumeLighting(true);
		R_TimeReport("shadowvolume");
	}

	R_Mesh_Finish();
	R_TimeReport("meshfinish");
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

	memset(&m, 0, sizeof(m));
	R_Mesh_State_Texture(&m);

	R_Mesh_GetSpace(8);
	vertex3f[ 0] = mins[0];vertex3f[ 1] = mins[1];vertex3f[ 2] = mins[2];
	vertex3f[ 3] = maxs[0];vertex3f[ 4] = mins[1];vertex3f[ 5] = mins[2];
	vertex3f[ 6] = mins[0];vertex3f[ 7] = maxs[1];vertex3f[ 8] = mins[2];
	vertex3f[ 9] = maxs[0];vertex3f[10] = maxs[1];vertex3f[11] = mins[2];
	vertex3f[12] = mins[0];vertex3f[13] = mins[1];vertex3f[14] = maxs[2];
	vertex3f[15] = maxs[0];vertex3f[16] = mins[1];vertex3f[17] = maxs[2];
	vertex3f[18] = mins[0];vertex3f[19] = maxs[1];vertex3f[20] = maxs[2];
	vertex3f[21] = maxs[0];vertex3f[22] = maxs[1];vertex3f[23] = maxs[2];
	GL_ColorPointer(color);
	R_FillColors(color, 8, cr, cg, cb, ca);
	if (fogenabled)
	{
		for (i = 0, v = vertex, c = color;i < 8;i++, v += 4, c += 4)
		{
			VectorSubtract(v, r_origin, diff);
			f2 = exp(fogdensity/DotProduct(diff, diff));
			f1 = 1 - f2;
			c[0] = c[0] * f1 + fogcolor[0] * f2;
			c[1] = c[1] * f1 + fogcolor[1] * f2;
			c[2] = c[2] * f1 + fogcolor[2] * f2;
		}
	}
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
	R_Mesh_State_Texture(&m);

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
	GL_DepthTest(true);
	GL_VertexPointer(nomodelvertex3f);
	if (fogenabled)
	{
		memcpy(color4f, nomodelcolor4f, sizeof(float[6*4]));
		GL_ColorPointer(color4f);
		VectorSubtract(ent->origin, r_origin, diff);
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
		GL_ColorPointer(color4f);
		for (i = 0, c = color4f;i < 6;i++, c += 4)
			c[3] *= ent->alpha;
	}
	else
		GL_ColorPointer(nomodelcolor4f);
	R_Mesh_Draw(6, 8, nomodelelements);
}

void R_DrawNoModel(entity_render_t *ent)
{
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->origin, R_DrawNoModelCallback, ent, 0);
	//else
	//	R_DrawNoModelCallback(ent, 0);
}

void R_CalcBeam_Vertex3f (float *vert, const vec3_t org1, const vec3_t org2, float width)
{
	vec3_t right1, right2, diff, normal;

	VectorSubtract (org2, org1, normal);
	VectorNormalizeFast (normal);

	// calculate 'right' vector for start
	VectorSubtract (r_origin, org1, diff);
	VectorNormalizeFast (diff);
	CrossProduct (normal, diff, right1);

	// calculate 'right' vector for end
	VectorSubtract (r_origin, org2, diff);
	VectorNormalizeFast (diff);
	CrossProduct (normal, diff, right2);

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
		VectorSubtract(origin, r_origin, diff);
		ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
	}

	R_Mesh_Matrix(&r_identitymatrix);
	GL_Color(cr, cg, cb, ca);
	GL_VertexPointer(varray_vertex3f);
	GL_BlendFunc(blendfunc1, blendfunc2);
	GL_DepthMask(false);
	GL_DepthTest(!depthdisable);

	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(texture);
	m.pointer_texcoord[0] = spritetexcoord2f;
	R_Mesh_State_Texture(&m);

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
	R_Mesh_Draw(4, 2, polygonelements);
}

