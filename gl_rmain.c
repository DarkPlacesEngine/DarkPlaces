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
cvar_t r_shadows = {CVAR_SAVE, "r_shadows", "1"};
cvar_t r_staticworldlights = {0, "r_staticworldlights", "0"};
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
	Matrix4x4_CreateIdentity(&r_identitymatrix);
// FIXME: move this to client?
	FOG_registercvars();
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_staticworldlights);
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
}

int R_CullBox(const vec3_t emins, const vec3_t emaxs)
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
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

int R_NotCulledBox(const vec3_t emins, const vec3_t emaxs)
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
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return false;
			break;
		case 1:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
				return false;
			break;
		case 2:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return false;
			break;
		case 3:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
				return false;
			break;
		case 4:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return false;
			break;
		case 5:
			if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
				return false;
			break;
		case 6:
			if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return false;
			break;
		case 7:
			if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
				return false;
			break;
		}
	}
	return true;
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

		VectorCopy(ent->angles, v);
		if (!ent->model || ent->model->type != mod_brush)
			v[0] = -v[0];
		Matrix4x4_CreateFromQuakeEntity(&ent->matrix, ent->origin[0], ent->origin[1], ent->origin[2], v[0], v[1], v[2], ent->scale);
		Matrix4x4_Invert_Simple(&ent->inversematrix, &ent->matrix);
		R_LerpAnimation(ent);
		R_UpdateEntLights(ent);
		if (R_NotCulledBox(ent->mins, ent->maxs))
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

void R_DrawNoModel(entity_render_t *ent);
void R_DrawModels (void)
{
	int i;
	entity_render_t *ent;

	if (!r_drawentities.integer)
		return;

	R_DrawViewModel();
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

void R_DrawFakeShadows (void)
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
		if (ent->model && ent->model->DrawFakeShadow)
			ent->model->DrawFakeShadow(ent);
	}
}

#include "r_shadow.h"

void R_TestAndDrawShadowVolume(entity_render_t *ent, vec3_t lightorigin, float lightradius, int visiblevolume)
{
	int i;
	vec3_t p, p2, temp, relativelightorigin;
	float dist, projectdistance;
	// rough checks
	if (ent->model && ent->model->DrawShadowVolume)
	{
		temp[0] = bound(ent->mins[0], lightorigin[0], ent->maxs[0]) - lightorigin[0];
		temp[1] = bound(ent->mins[1], lightorigin[1], ent->maxs[1]) - lightorigin[1];
		temp[2] = bound(ent->mins[2], lightorigin[2], ent->maxs[2]) - lightorigin[2];
		dist = DotProduct(temp, temp);
		if (dist < lightradius * lightradius)
		{
			projectdistance = lightradius - sqrt(dist);
			{
#if 0
			int d0, d1, d2, d3;
			// calculate projected bounding box and decide if it is on-screen
			d0 = false;
			d1 = false;
			d2 = false;
			d3 = false;
			for (i = 0;i < 8;i++)
			{
				p[0] = i & 1 ? ent->maxs[0] : ent->mins[0];
				p[1] = i & 2 ? ent->maxs[1] : ent->mins[1];
				p[2] = i & 4 ? ent->maxs[2] : ent->mins[2];
				VectorSubtract(p, lightorigin, temp);
				dist = projectdistance / sqrt(DotProduct(temp, temp));
				VectorMA(p, dist, temp, p2);
				if (!d0 && (DotProduct(p , frustum[0].normal) < frustum[0].dist || DotProduct(p2, frustum[0].normal) < frustum[0].dist))
					d0 = true;
				if (!d1 && (DotProduct(p , frustum[1].normal) < frustum[1].dist || DotProduct(p2, frustum[1].normal) < frustum[1].dist))
					d1 = true;
				if (!d2 && (DotProduct(p , frustum[2].normal) < frustum[2].dist || DotProduct(p2, frustum[2].normal) < frustum[2].dist))
					d2 = true;
				if (!d3 && (DotProduct(p , frustum[3].normal) < frustum[3].dist || DotProduct(p2, frustum[3].normal) < frustum[3].dist))
					d3 = true;
			}
			if (d0 && d1 && d2 && d3)
#else
			vec3_t mins, maxs;
			// calculate projected bounding box and decide if it is on-screen
			VectorCopy(ent->mins, mins);
			VectorCopy(ent->maxs, maxs);
			for (i = 0;i < 8;i++)
			{
				p[0] = i & 1 ? ent->maxs[0] : ent->mins[0];
				p[1] = i & 2 ? ent->maxs[1] : ent->mins[1];
				p[2] = i & 4 ? ent->maxs[2] : ent->mins[2];
				VectorSubtract(p, lightorigin, temp);
				dist = projectdistance / sqrt(DotProduct(temp, temp));
				VectorMA(p, dist, temp, p2);
				if (mins[0] > p2[0]) mins[0] = p2[0];if (maxs[0] < p2[0]) maxs[0] = p2[0];
				if (mins[1] > p2[1]) mins[1] = p2[1];if (maxs[1] < p2[1]) maxs[1] = p2[1];
				if (mins[2] > p2[2]) mins[2] = p2[2];if (maxs[2] < p2[2]) maxs[2] = p2[2];
			}
			if (R_NotCulledBox(mins, maxs))
#endif
			{
				Matrix4x4_Transform(&ent->inversematrix, lightorigin, relativelightorigin);
				R_Mesh_Matrix(&ent->matrix);
				ent->model->DrawShadowVolume (ent, relativelightorigin, lightradius, visiblevolume);
			}
			}
		}
	}
}

void R_DrawWorldLightShadowVolume(mlight_t *sl, int visiblevolume)
{
	shadowmesh_t *mesh;
	R_Mesh_Matrix(&cl_entities[0].render.matrix);
	for (mesh = sl->shadowvolume;mesh;mesh = mesh->next)
	{
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		R_Shadow_RenderVolume(mesh->numverts, mesh->numtriangles, mesh->elements, visiblevolume);
	}
}

void R_DrawShadowVolumes (void)
{
	int i, lnum;
	entity_render_t *ent;
	vec3_t mins, maxs;//, relativelightorigin;
	mlight_t *sl;
	rdlight_t *rd;
	rmeshstate_t m;

	for (lnum = 0, sl = cl.worldmodel->lights;lnum < cl.worldmodel->numlights;lnum++, sl++)
	{
		if (d_lightstylevalue[sl->style] <= 0)
			continue;
		/*
		mins[0] = sl->origin[0] - sl->cullradius;
		maxs[0] = sl->origin[0] + sl->cullradius;
		mins[1] = sl->origin[1] - sl->cullradius;
		maxs[1] = sl->origin[1] + sl->cullradius;
		mins[2] = sl->origin[2] - sl->cullradius;
		maxs[2] = sl->origin[2] + sl->cullradius;
		if (R_CullBox(mins, maxs))
			continue;
		*/
		if (R_CullBox(sl->mins, sl->maxs))
			continue;
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ONE;
		R_Mesh_State(&m);
		GL_Color(0.0 * r_colorscale, 0.0125 * r_colorscale, 0.1 * r_colorscale, 1);
		if (sl->shadowvolume && r_staticworldlights.integer)
			R_DrawWorldLightShadowVolume(sl, true);
		else
		{
			ent = &cl_entities[0].render;
			R_TestAndDrawShadowVolume(ent, sl->origin, sl->cullradius, true);
		}
		/*
		ent = &cl_entities[0].render;
		if (ent->model && ent->model->DrawShadowVolume && ent->maxs[0] >= mins[0] && ent->mins[0] <= maxs[0] && ent->maxs[1] >= mins[1] && ent->mins[1] <= maxs[1] && ent->maxs[2] >= mins[2] && ent->mins[2] <= maxs[2])
		{
			Matrix4x4_Transform(&ent->inversematrix, sl->origin, relativelightorigin);
			R_Mesh_Matrix(&ent->matrix);
			ent->model->DrawShadowVolume (ent, relativelightorigin, sl->cullradius, true);
		}
		*/
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				/*
				if (ent->mins[0] <= sl->maxs[0]
				 && ent->maxs[0] >= sl->mins[0]
				 && ent->mins[1] <= sl->maxs[1]
				 && ent->maxs[1] >= sl->mins[1]
				 && ent->mins[2] <= sl->maxs[2]
				 && ent->maxs[2] >= sl->mins[2])
				*/
					R_TestAndDrawShadowVolume(ent, sl->origin, sl->cullradius, true);
				/*
				ent = r_refdef.entities[i];
				if (ent->model && ent->model->DrawShadowVolume && ent->maxs[0] >= mins[0] && ent->mins[0] <= maxs[0] && ent->maxs[1] >= mins[1] && ent->mins[1] <= maxs[1] && ent->maxs[2] >= mins[2] && ent->mins[2] <= maxs[2])
				{
					Matrix4x4_Transform(&ent->inversematrix, sl->origin, relativelightorigin);
					R_Mesh_Matrix(&ent->matrix);
					ent->model->DrawShadowVolume (ent, relativelightorigin, sl->cullradius, true);
				}
				*/
			}
		}
	}

	for (lnum = 0, rd = r_dlight;lnum < r_numdlights;lnum++, rd++)
	{
		mins[0] = rd->origin[0] - rd->cullradius;
		maxs[0] = rd->origin[0] + rd->cullradius;
		mins[1] = rd->origin[1] - rd->cullradius;
		maxs[1] = rd->origin[1] + rd->cullradius;
		mins[2] = rd->origin[2] - rd->cullradius;
		maxs[2] = rd->origin[2] + rd->cullradius;
		if (R_CullBox(mins, maxs))
			continue;
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ONE;
		R_Mesh_State(&m);
		GL_Color(0.1 * r_colorscale, 0.0125 * r_colorscale, 0.0 * r_colorscale, 1);
		ent = &cl_entities[0].render;
		if (ent != rd->ent)
			R_TestAndDrawShadowVolume(ent, rd->origin, rd->cullradius, true);
		/*
		ent = &cl_entities[0].render;
		if (ent != rd->ent && ent->model && ent->model->DrawShadowVolume && ent->maxs[0] >= mins[0] && ent->mins[0] <= maxs[0] && ent->maxs[1] >= mins[1] && ent->mins[1] <= maxs[1] && ent->maxs[2] >= mins[2] && ent->mins[2] <= maxs[2])
		{
			Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
			R_Mesh_Matrix(&ent->matrix);
			ent->model->DrawShadowVolume (ent, relativelightorigin, rd->cullradius, true);
		}
		*/
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent != rd->ent)
					R_TestAndDrawShadowVolume(ent, rd->origin, rd->cullradius, true);
				/*
				ent = r_refdef.entities[i];
				if (ent != rd->ent && ent->model && ent->model->DrawShadowVolume && ent->maxs[0] >= mins[0] && ent->mins[0] <= maxs[0] && ent->maxs[1] >= mins[1] && ent->mins[1] <= maxs[1] && ent->maxs[2] >= mins[2] && ent->mins[2] <= maxs[2])
				{
					Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
					R_Mesh_Matrix(&ent->matrix);
					ent->model->DrawShadowVolume (ent, relativelightorigin, rd->cullradius, true);
				}
				*/
			}
		}
	}
}

#define SHADOWSPHERE_SEGMENTS 16

shadowmesh_t *shadowsphere;
void R_CreateShadowSphere(void)
{
	int i, j;
	vec3_t angles, angles2, angles3, angles4;
	float verts[12];
	shadowsphere = Mod_ShadowMesh_Begin(zonemempool);
	for (i = 0;i < SHADOWSPHERE_SEGMENTS;i++)
	{
		for (j = 0;j < SHADOWSPHERE_SEGMENTS;j++)
		{
			angles[0] = i * 360.0f / SHADOWSPHERE_SEGMENTS;
			angles[1] = j * 360.0f / SHADOWSPHERE_SEGMENTS;
			angles[2] = 0;
			VectorCopy(angles, angles2);
			VectorCopy(angles, angles3);
			VectorCopy(angles, angles4);
			angles2[1] += 360.0f / SHADOWSPHERE_SEGMENTS;
			angles3[0] += 360.0f / SHADOWSPHERE_SEGMENTS;
			angles3[1] += 360.0f / SHADOWSPHERE_SEGMENTS;
			angles4[0] += 360.0f / SHADOWSPHERE_SEGMENTS;
			AngleVectorsFLU(angles, verts, NULL, NULL);
			AngleVectorsFLU(angles2, verts + 9, NULL, NULL);
			AngleVectorsFLU(angles3, verts + 6, NULL, NULL);
			AngleVectorsFLU(angles4, verts + 3, NULL, NULL);
			VectorScale(&verts[0], 64.0f, &verts[0]);
			VectorScale(&verts[3], 64.0f, &verts[3]);
			VectorScale(&verts[6], 64.0f, &verts[6]);
			VectorScale(&verts[9], 64.0f, &verts[9]);
			Mod_ShadowMesh_AddPolygon(zonemempool, shadowsphere, 4, verts);
			AngleVectorsFLU(angles, verts, NULL, NULL);
			AngleVectorsFLU(angles2, verts + 3, NULL, NULL);
			AngleVectorsFLU(angles3, verts + 6, NULL, NULL);
			AngleVectorsFLU(angles4, verts + 9, NULL, NULL);
			VectorScale(&verts[0], 128.0f, &verts[0]);
			VectorScale(&verts[3], 128.0f, &verts[3]);
			VectorScale(&verts[6], 128.0f, &verts[6]);
			VectorScale(&verts[9], 128.0f, &verts[9]);
			Mod_ShadowMesh_AddPolygon(zonemempool, shadowsphere, 4, verts);
		}
	}
	shadowsphere = Mod_ShadowMesh_Finish(zonemempool, shadowsphere);
}


void R_DrawShadowSphere(vec3_t origin, float radius, int visiblevolume)
{
	int i;
	float *v;
	shadowmesh_t *mesh;
	//matrix4x4_t matrix;
	if (!shadowsphere)
		R_CreateShadowSphere();
	//Matrix4x4_CreateTranslate(&matrix, origin[0], origin[1], origin[2]);
	//Matrix4x4_ConcatScale(&matrix, radius);
	//R_Mesh_Matrix(&matrix);
	R_Mesh_Matrix(&r_identitymatrix);
	for (mesh = shadowsphere;mesh;mesh = mesh->next)
	{
		memcpy(varray_vertex, mesh->verts, mesh->numverts * sizeof(float[4]));
		for (i = 0, v = varray_vertex;i < mesh->numverts;i++, v += 4)
			VectorMA(origin, radius, v, v);
		R_Shadow_RenderVolume(mesh->numverts, mesh->numtriangles, mesh->elements, visiblevolume);
	}
}

void R_ShadowVolumeLighting (void)
{
	int i;
	entity_render_t *ent;
	int lnum;
	float f;
	vec3_t mins, maxs, relativelightorigin, lightcolor;
	mlight_t *sl;
	rdlight_t *rd;

	R_Shadow_Stage_Depth();
	ent = &cl_entities[0].render;
	if (ent->model && ent->model->DrawDepth)
	{
		R_Mesh_Matrix(&ent->matrix);
		ent->model->DrawDepth(ent);
	}
	if (r_drawentities.integer)
	{
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			if (ent->visframe == r_framecount && ent->model && ent->model->DrawDepth)
			{
				R_Mesh_Matrix(&ent->matrix);
				ent->model->DrawDepth(ent);
			}
		}
	}

	for (lnum = 0, sl = cl.worldmodel->lights;lnum < cl.worldmodel->numlights;lnum++, sl++)
	{
		if (d_lightstylevalue[sl->style] <= 0)
			continue;
		VectorCopy(sl->mins, mins);
		VectorCopy(sl->maxs, maxs);
		/*
		mins[0] = sl->origin[0] - sl->cullradius;
		maxs[0] = sl->origin[0] + sl->cullradius;
		mins[1] = sl->origin[1] - sl->cullradius;
		maxs[1] = sl->origin[1] + sl->cullradius;
		mins[2] = sl->origin[2] - sl->cullradius;
		maxs[2] = sl->origin[2] + sl->cullradius;
		if (R_CullBox(mins, maxs))
			continue;
		*/
		if (R_CullBox(mins, maxs))
			continue;

		f = d_lightstylevalue[sl->style] * (1.0f / 32768.0f);
		VectorScale(sl->light, f, lightcolor);

		R_Shadow_Stage_ShadowVolumes();
		R_DrawShadowSphere(sl->origin, 1.0f/*sl->cullradius - 16*/, false);
		if (sl->shadowvolume && r_staticworldlights.integer)
			R_DrawWorldLightShadowVolume(sl, false);
		else
			R_TestAndDrawShadowVolume(&cl_entities[0].render, sl->origin, sl->cullradius, false);
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent->maxs[0] >= mins[0]
				 && ent->mins[0] <= maxs[0]
				 && ent->maxs[1] >= mins[1]
				 && ent->mins[1] <= maxs[1]
				 && ent->maxs[2] >= mins[2]
				 && ent->mins[2] <= maxs[2])
					R_TestAndDrawShadowVolume(r_refdef.entities[i], sl->origin, sl->cullradius, false);
			}
		}

		R_Shadow_Stage_Light();
		ent = &cl_entities[0].render;
		if (ent->model && ent->model->DrawLight)
		{
			Matrix4x4_Transform(&ent->inversematrix, sl->origin, relativelightorigin);
			R_Mesh_Matrix(&ent->matrix);
			ent->model->DrawLight(ent, relativelightorigin, sl->cullradius, sl->distbias, sl->subtract, lightcolor);
		}
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent->visframe == r_framecount && ent->model && ent->model->DrawLight
				 && ent->maxs[0] >= mins[0]
				 && ent->mins[0] <= maxs[0]
				 && ent->maxs[1] >= mins[1]
				 && ent->mins[1] <= maxs[1]
				 && ent->maxs[2] >= mins[2]
				 && ent->mins[2] <= maxs[2])
				{
					Matrix4x4_Transform(&ent->inversematrix, sl->origin, relativelightorigin);
					R_Mesh_Matrix(&ent->matrix);
					ent->model->DrawLight(ent, relativelightorigin, sl->cullradius, sl->distbias, sl->subtract, lightcolor);
				}
			}
		}
	}
	for (lnum = 0, rd = r_dlight;lnum < r_numdlights;lnum++, rd++)
	{
		mins[0] = rd->origin[0] - rd->cullradius;
		maxs[0] = rd->origin[0] + rd->cullradius;
		mins[1] = rd->origin[1] - rd->cullradius;
		maxs[1] = rd->origin[1] + rd->cullradius;
		mins[2] = rd->origin[2] - rd->cullradius;
		maxs[2] = rd->origin[2] + rd->cullradius;
		if (R_CullBox(mins, maxs))
			continue;

		R_Shadow_Stage_ShadowVolumes();
		R_TestAndDrawShadowVolume(&cl_entities[0].render, rd->origin, rd->cullradius, false);
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent->maxs[0] >= mins[0]
				 && ent->mins[0] <= maxs[0]
				 && ent->maxs[1] >= mins[1]
				 && ent->mins[1] <= maxs[1]
				 && ent->maxs[2] >= mins[2]
				 && ent->mins[2] <= maxs[2])
				R_TestAndDrawShadowVolume(ent, rd->origin, rd->cullradius, false);
			}
		}

		R_Shadow_Stage_Light();
		ent = &cl_entities[0].render;
		if (ent->model && ent->model->DrawLight)
		{
			Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
			R_Mesh_Matrix(&ent->matrix);
			ent->model->DrawLight(ent, relativelightorigin, rd->cullradius, LIGHTOFFSET, rd->subtract, rd->light);
		}
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent->visframe == r_framecount && ent->model && ent->model->DrawLight
				 && ent->maxs[0] >= mins[0]
				 && ent->mins[0] <= maxs[0]
				 && ent->maxs[1] >= mins[1]
				 && ent->mins[1] <= maxs[1]
				 && ent->maxs[2] >= mins[2]
				 && ent->mins[2] <= maxs[2])
				{
					Matrix4x4_Transform(&ent->inversematrix, rd->origin, relativelightorigin);
					R_Mesh_Matrix(&ent->matrix);
					ent->model->DrawLight(ent, relativelightorigin, rd->cullradius, LIGHTOFFSET, rd->subtract, rd->light);
				}
			}
		}
	}

	R_Shadow_Stage_Textures();
	ent = &cl_entities[0].render;
	if (ent->model && ent->model->DrawOntoLight)
	{
		R_Mesh_Matrix(&ent->matrix);
		ent->model->DrawOntoLight(ent);
	}
	if (r_drawentities.integer)
	{
		for (i = 0;i < r_refdef.numentities;i++)
		{
			ent = r_refdef.entities[i];
			if (ent->visframe == r_framecount && ent->model && ent->model->DrawOntoLight)
			{
				R_Mesh_Matrix(&ent->matrix);
				ent->model->DrawOntoLight(ent);
			}
		}
	}

	R_Shadow_Stage_End();
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
	rmeshstate_t m;
	float r;

	if (r_refdef.viewblend[3] < 0.01f)
		return;

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.depthdisable = true; // magic
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	r = 64000;
	varray_vertex[0] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r - vup[0] * r;
	varray_vertex[1] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r - vup[1] * r;
	varray_vertex[2] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r - vup[2] * r;
	r *= 3;
	varray_vertex[4] = varray_vertex[0] + vup[0] * r;
	varray_vertex[5] = varray_vertex[1] + vup[1] * r;
	varray_vertex[6] = varray_vertex[2] + vup[2] * r;
	varray_vertex[8] = varray_vertex[0] + vright[0] * r;
	varray_vertex[9] = varray_vertex[1] + vright[1] * r;
	varray_vertex[10] = varray_vertex[2] + vright[2] * r;
	GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
	R_Mesh_Draw(3, 1, polygonelements);
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
	r_farclip = R_FarClip_Finish() + 16384.0f;//256.0f;
	R_TimeReport("markentity");

	GL_SetupView_ViewPort(r_refdef.x, r_refdef.y, r_refdef.width, r_refdef.height);
	GL_SetupView_Mode_Perspective((double) r_refdef.height / r_refdef.width, r_refdef.fov_x, r_refdef.fov_y, 1.0f, r_farclip);
	GL_SetupView_Orientation_FromEntity (r_refdef.vieworg, r_refdef.viewangles);
	qglDepthFunc(GL_LEQUAL);

	R_Mesh_Start();
	R_MeshQueue_BeginScene();

	if (r_shadows.integer == 3)
	{
		if (!gl_stencil)
		{
			Con_Printf("Stencil not enabled, turning off r_shadows 3\n");
			Cvar_SetValueQuick(&r_shadows, 0);
		}
		else if (!gl_texture3d)
		{
			Con_Printf("3D texture support not detected, turning off r_shadows 3\n");
			Cvar_SetValueQuick(&r_shadows, 0);
		}
		else if (!gl_texturecubemap)
		{
			Con_Printf("Cubemap texture support not detected, turning off r_shadows 3\n");
			Cvar_SetValueQuick(&r_shadows, 0);
		}
		else if (!gl_dot3arb)
		{
			Con_Printf("Bumpmapping support not detected, turning off r_shadows 3\n");
			Cvar_SetValueQuick(&r_shadows, 0);
		}
	}

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

	if (r_shadows.integer == 3)
		R_ShadowVolumeLighting();

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
	if (r_shadows.integer == 1)
	{
		R_DrawFakeShadows();
		R_TimeReport("fakeshadows");
	}
	if (r_shadows.integer == 2)
	{
		R_DrawShadowVolumes();
		R_TimeReport("shadowvolumes");
	}
	R_Mesh_Finish();
	R_TimeReport("meshfinish");
}

/*
void R_DrawBBoxMesh(vec3_t mins, vec3_t maxs, float cr, float cg, float cb, float ca)
{
	int i;
	float *v, *c, f1, f2, diff[3];
	rmeshstate_t m;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	varray_vertex[ 0] = mins[0];varray_vertex[ 1] = mins[1];varray_vertex[ 2] = mins[2];
	varray_vertex[ 4] = maxs[0];varray_vertex[ 5] = mins[1];varray_vertex[ 6] = mins[2];
	varray_vertex[ 8] = mins[0];varray_vertex[ 9] = maxs[1];varray_vertex[10] = mins[2];
	varray_vertex[12] = maxs[0];varray_vertex[13] = maxs[1];varray_vertex[14] = mins[2];
	varray_vertex[16] = mins[0];varray_vertex[17] = mins[1];varray_vertex[18] = maxs[2];
	varray_vertex[20] = maxs[0];varray_vertex[21] = mins[1];varray_vertex[22] = maxs[2];
	varray_vertex[24] = mins[0];varray_vertex[25] = maxs[1];varray_vertex[26] = maxs[2];
	varray_vertex[28] = maxs[0];varray_vertex[29] = maxs[1];varray_vertex[30] = maxs[2];
	R_FillColors(varray_color, 8, cr * r_colorscale, cg * r_colorscale, cb * r_colorscale, ca);
	if (fogenabled)
	{
		for (i = 0, v = varray_vertex, c = varray_color;i < 8;i++, v += 4, c += 4)
		{
			VectorSubtract(v, r_origin, diff);
			f2 = exp(fogdensity/DotProduct(diff, diff));
			f1 = 1 - f2;
			f2 *= r_colorscale;
			c[0] = c[0] * f1 + fogcolor[0] * f2;
			c[1] = c[1] * f1 + fogcolor[1] * f2;
			c[2] = c[2] * f1 + fogcolor[2] * f2;
		}
	}
	GL_UseColorArray();
	R_Mesh_Draw(8, 12);
}
*/

void R_DrawNoModelCallback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	int i, element[24];
	float f1, f2, *c, diff[3];
	rmeshstate_t m;
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
	R_Mesh_Matrix(&ent->matrix);
	R_Mesh_State(&m);

	element[ 0] = 5;element[ 1] = 2;element[ 2] = 0;
	element[ 3] = 5;element[ 4] = 1;element[ 5] = 2;
	element[ 6] = 5;element[ 7] = 0;element[ 8] = 3;
	element[ 9] = 5;element[10] = 3;element[11] = 1;
	element[12] = 0;element[13] = 2;element[14] = 4;
	element[15] = 2;element[16] = 1;element[17] = 4;
	element[18] = 3;element[19] = 0;element[20] = 4;
	element[21] = 1;element[22] = 3;element[23] = 4;
	varray_vertex[ 0] = -16;varray_vertex[ 1] =   0;varray_vertex[ 2] =   0;
	varray_vertex[ 4] =  16;varray_vertex[ 5] =   0;varray_vertex[ 6] =   0;
	varray_vertex[ 8] =   0;varray_vertex[ 9] = -16;varray_vertex[10] =   0;
	varray_vertex[12] =   0;varray_vertex[13] =  16;varray_vertex[14] =   0;
	varray_vertex[16] =   0;varray_vertex[17] =   0;varray_vertex[18] = -16;
	varray_vertex[20] =   0;varray_vertex[21] =   0;varray_vertex[22] =  16;
	varray_color[ 0] = 0.00f * r_colorscale;varray_color[ 1] = 0.00f * r_colorscale;varray_color[ 2] = 0.50f * r_colorscale;varray_color[ 3] = ent->alpha;
	varray_color[ 4] = 0.00f * r_colorscale;varray_color[ 5] = 0.00f * r_colorscale;varray_color[ 6] = 0.50f * r_colorscale;varray_color[ 7] = ent->alpha;
	varray_color[ 8] = 0.00f * r_colorscale;varray_color[ 9] = 0.50f * r_colorscale;varray_color[10] = 0.00f * r_colorscale;varray_color[11] = ent->alpha;
	varray_color[12] = 0.00f * r_colorscale;varray_color[13] = 0.50f * r_colorscale;varray_color[14] = 0.00f * r_colorscale;varray_color[15] = ent->alpha;
	varray_color[16] = 0.50f * r_colorscale;varray_color[17] = 0.00f * r_colorscale;varray_color[18] = 0.00f * r_colorscale;varray_color[19] = ent->alpha;
	varray_color[20] = 0.50f * r_colorscale;varray_color[21] = 0.00f * r_colorscale;varray_color[22] = 0.00f * r_colorscale;varray_color[23] = ent->alpha;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_origin, diff);
		f2 = exp(fogdensity/DotProduct(diff, diff));
		f1 = 1 - f2;
		for (i = 0, c = varray_color;i < 6;i++, c += 4)
		{
			c[0] = (c[0] * f1 + fogcolor[0] * f2) * r_colorscale;
			c[1] = (c[1] * f1 + fogcolor[1] * f2) * r_colorscale;
			c[2] = (c[2] * f1 + fogcolor[2] * f2) * r_colorscale;
		}
	}
	else
	{
		for (i = 0, c = varray_color;i < 6;i++, c += 4)
		{
			c[0] *= r_colorscale;
			c[1] *= r_colorscale;
			c[2] *= r_colorscale;
		}
	}
	GL_UseColorArray();
	R_Mesh_Draw(6, 8, element);
}

void R_DrawNoModel(entity_render_t *ent)
{
	//if ((ent->effects & EF_ADDITIVE) || (ent->alpha < 1))
		R_MeshQueue_AddTransparent(ent->origin, R_DrawNoModelCallback, ent, 0);
	//else
	//	R_DrawNoModelCallback(ent, 0);
}

void R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width)
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
	vert[ 4] = org1[0] - width * right1[0];
	vert[ 5] = org1[1] - width * right1[1];
	vert[ 6] = org1[2] - width * right1[2];
	vert[ 8] = org2[0] - width * right2[0];
	vert[ 9] = org2[1] - width * right2[1];
	vert[10] = org2[2] - width * right2[2];
	vert[12] = org2[0] + width * right2[0];
	vert[13] = org2[1] + width * right2[1];
	vert[14] = org2[2] + width * right2[2];
}
