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
cvar_t r_shadows = {CVAR_SAVE, "r_shadows", "0"};
cvar_t r_shadow_staticworldlights = {0, "r_shadow_staticworldlights", "1"};
cvar_t r_speeds = {0, "r_speeds","0"};
cvar_t r_fullbright = {0, "r_fullbright","0"};
cvar_t r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};
cvar_t r_shadow_cull = {0, "r_shadow_cull", "1"};

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
	Cvar_RegisterVariable (&r_shadow_staticworldlights);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_fullbrights);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_textureunits);
	Cvar_RegisterVariable (&r_shadow_cull);
	if (gamemode == GAME_NEHAHRA || gamemode == GAME_NEXIUZ)
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

int PVS_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int stackpos, sides;
	mnode_t *node, *stack[4096];
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->pvsframe == cl.worldmodel->pvsframecount)
				return false;
		}
		else
		{
			sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides & 2 && stackpos < 4096)
				stack[stackpos++] = node->children[1];
			if (sides & 1 && stackpos < 4096)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}

int VIS_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int stackpos, sides;
	mnode_t *node, *stack[4096];
	if (R_CullBox(mins, maxs))
		return true;
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->visframe == r_framecount)
				return false;
		}
		else
		{
			sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides & 2 && stackpos < 4096)
				stack[stackpos++] = node->children[1];
			if (sides & 1 && stackpos < 4096)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}

int R_CullSphere(const vec3_t origin, vec_t radius)
{
	return (DotProduct(frustum[0].normal, origin) + radius < frustum[0].dist
	     || DotProduct(frustum[1].normal, origin) + radius < frustum[1].dist
	     || DotProduct(frustum[2].normal, origin) + radius < frustum[2].dist
	     || DotProduct(frustum[3].normal, origin) + radius < frustum[3].dist);
}

int PVS_CullSphere(const vec3_t origin, vec_t radius)
{
	int stackpos;
	mnode_t *node, *stack[4096];
	float dist;
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->pvsframe == cl.worldmodel->pvsframecount)
				return false;
		}
		else
		{
			dist = PlaneDiff(origin, node->plane);
			if (dist <= radius)
				stack[stackpos++] = node->children[1];
			if (dist >= -radius)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}

int VIS_CullSphere(const vec3_t origin, vec_t radius)
{
	int stackpos;
	mnode_t *node, *stack[4096];
	float dist;
	if (R_CullSphere(origin, radius))
		return true;
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->visframe == r_framecount)
				return false;
		}
		else
		{
			dist = PlaneDiff(origin, node->plane);
			if (dist <= radius)
				stack[stackpos++] = node->children[1];
			if (dist >= -radius)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}


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
		 && !VIS_CullSphere(ent->origin, (ent->model != NULL ? ent->model->radius : 16) * ent->scale)
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
void R_DrawModels ()
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
		if ((ent->flags & RENDER_SHADOW) && ent->model && ent->model->DrawFakeShadow)
			ent->model->DrawFakeShadow(ent);
	}
}

#include "r_shadow.h"

int shadowframecount = 0;

int Light_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int stackpos, sides;
	mnode_t *node, *stack[4096];
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->worldnodeframe == shadowframecount)
				return false;
		}
		else
		{
			sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides & 2 && stackpos < 4096)
				stack[stackpos++] = node->children[1];
			if (sides & 1 && stackpos < 4096)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}

int LightAndVis_CullBox(const vec3_t mins, const vec3_t maxs)
{
	int stackpos, sides;
	mnode_t *node, *stack[4096];
	if (R_CullBox(mins, maxs))
		return true;
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->visframe == r_framecount && ((mleaf_t *)node)->worldnodeframe == shadowframecount)
				return false;
		}
		else
		{
			sides = BoxOnPlaneSide(mins, maxs, node->plane);
			if (sides & 2 && stackpos < 4096)
				stack[stackpos++] = node->children[1];
			if (sides & 1 && stackpos < 4096)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}

int LightAndVis_CullPointCloud(int numpoints, const float *points)
{
	int i;
	const float *p;
	int stackpos, sides;
	mnode_t *node, *stack[4096];
	//if (R_CullBox(mins, maxs))
	//	return true;
	if (cl.worldmodel == NULL)
		return false;
	stackpos = 0;
	stack[stackpos++] = cl.worldmodel->nodes;
	while (stackpos)
	{
		node = stack[--stackpos];
		if (node->contents < 0)
		{
			if (((mleaf_t *)node)->visframe == r_framecount && ((mleaf_t *)node)->worldnodeframe == shadowframecount)
				return false;
		}
		else
		{
			sides = 0;
			for (i = 0, p = points;i < numpoints && sides != 3;i++, p += 3)
			{
				if (DotProduct(p, node->plane->normal) < node->plane->dist)
					sides |= 1;
				else
					sides |= 2;
			}
			if (sides & 2 && stackpos < 4096)
				stack[stackpos++] = node->children[1];
			if (sides & 1 && stackpos < 4096)
				stack[stackpos++] = node->children[0];
		}
	}
	return true;
}


void R_TestAndDrawShadowVolume(entity_render_t *ent, vec3_t lightorigin, float cullradius, float lightradius, vec3_t lightmins, vec3_t lightmaxs, vec3_t clipmins, vec3_t clipmaxs)
{
	vec3_t relativelightorigin;
	#if 0
	int i;
	vec3_t temp;
	float dist, projectdistance;
	float points[16][3];
	#endif
	// rough checks
	if (!(ent->flags & RENDER_SHADOW) || ent->model == NULL || ent->model->DrawShadowVolume == NULL)
		return;
	if (r_shadow_cull.integer)
	{
		if (ent->maxs[0] < lightmins[0] || ent->mins[0] > lightmaxs[0]
		 || ent->maxs[1] < lightmins[1] || ent->mins[1] > lightmaxs[1]
		 || ent->maxs[2] < lightmins[2] || ent->mins[2] > lightmaxs[2]
		 || Light_CullBox(ent->mins, ent->maxs))
			return;
	}
	#if 0
	if (r_shadow_cull.integer)
	{
		projectdistance = cullradius;
		// calculate projected bounding box and decide if it is on-screen
		for (i = 0;i < 8;i++)
		{
			temp[0] = i & 1 ? ent->model->normalmaxs[0] : ent->model->normalmins[0];
			temp[1] = i & 2 ? ent->model->normalmaxs[1] : ent->model->normalmins[1];
			temp[2] = i & 4 ? ent->model->normalmaxs[2] : ent->model->normalmins[2];
			Matrix4x4_Transform(&ent->matrix, temp, points[i]);
			VectorSubtract(points[i], lightorigin, temp);
			dist = projectdistance / sqrt(DotProduct(temp, temp));
			VectorMA(points[i], dist, temp, points[i+8]);
		}
		if (LightAndVis_CullPointCloud(16, points[0]))
			return;
		/*
		for (i = 0;i < 8;i++)
		{
			p2[0] = i & 1 ? ent->model->normalmaxs[0] : ent->model->normalmins[0];
			p2[1] = i & 2 ? ent->model->normalmaxs[1] : ent->model->normalmins[1];
			p2[2] = i & 4 ? ent->model->normalmaxs[2] : ent->model->normalmins[2];
			Matrix4x4_Transform(&ent->matrix, p2, p);
			VectorSubtract(p, lightorigin, temp);
			dist = projectdistance / sqrt(DotProduct(temp, temp));
			VectorMA(p, dist, temp, p2);
			if (i)
			{
				if (mins[0] > p[0]) mins[0] = p[0];if (maxs[0] < p[0]) maxs[0] = p[0];
				if (mins[1] > p[1]) mins[1] = p[1];if (maxs[1] < p[1]) maxs[1] = p[1];
				if (mins[2] > p[2]) mins[2] = p[2];if (maxs[2] < p[2]) maxs[2] = p[2];
			}
			else
			{
				VectorCopy(p, mins);
				VectorCopy(p, maxs);
			}
			if (mins[0] > p2[0]) mins[0] = p2[0];if (maxs[0] < p2[0]) maxs[0] = p2[0];
			if (mins[1] > p2[1]) mins[1] = p2[1];if (maxs[1] < p2[1]) maxs[1] = p2[1];
			if (mins[2] > p2[2]) mins[2] = p2[2];if (maxs[2] < p2[2]) maxs[2] = p2[2];
		}
		if (mins[0] >= clipmaxs[0] || maxs[0] <= clipmins[0]
		 || mins[1] >= clipmaxs[1] || maxs[1] <= clipmins[1]
		 || mins[2] >= clipmaxs[2] || maxs[2] <= clipmins[2]
		 || LightAndVis_CullBox(mins, maxs))
			return;
		*/
	}
	#endif
	Matrix4x4_Transform(&ent->inversematrix, lightorigin, relativelightorigin);
	ent->model->DrawShadowVolume (ent, relativelightorigin, lightradius);
}

void R_Shadow_DrawWorldLightShadowVolume(matrix4x4_t *matrix, worldlight_t *light);

extern void R_Model_Brush_DrawLightForSurfaceList(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, msurface_t **surflist, int numsurfaces, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void R_ShadowVolumeLighting (int visiblevolumes)
{
	int i;
	entity_render_t *ent;
	int lnum;
	float f, lightradius, cullradius;
	vec3_t relativelightorigin, relativeeyeorigin, lightcolor, clipmins, clipmaxs;
	worldlight_t *wl;
	rdlight_t *rd;
	rmeshstate_t m;
	mleaf_t *leaf;
	matrix4x4_t matrix;
	matrix4x4_t matrix_worldtofilter, matrix_worldtoattenuationxyz, matrix_worldtoattenuationz;
	matrix4x4_t matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz;

	if (visiblevolumes)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ONE;
		if (r_shadow_realtime.integer >= 3)
			m.depthdisable = true;
		R_Mesh_State(&m);
		qglDisable(GL_CULL_FACE);
		GL_Color(0.0 * r_colorscale, 0.0125 * r_colorscale, 0.1 * r_colorscale, 1);
	}
	else
		R_Shadow_Stage_Begin();
	shadowframecount++;
	for (lnum = 0, wl = r_shadow_worldlightchain;wl;wl = wl->next, lnum++)
	{
		if (d_lightstylevalue[wl->style] <= 0)
			continue;
		if (R_CullBox(wl->mins, wl->maxs))
		//if (R_CullSphere(wl->origin, cullradius))
			continue;
		//if (R_CullBox(wl->mins, wl->maxs) || R_CullSphere(wl->origin, lightradius))
		//	continue;
		//if (VIS_CullBox(wl->mins, wl->maxs) || VIS_CullSphere(wl->origin, lightradius))
		//	continue;
		if (r_shadow_debuglight.integer >= 0 && lnum != r_shadow_debuglight.integer)
			continue;

		cullradius = wl->cullradius;
		lightradius = wl->lightradius;

		if (cl.worldmodel != NULL)
		{
			for (i = 0;i < wl->numleafs;i++)
				if (wl->leafs[i]->visframe == r_framecount)
					break;
			if (i == wl->numleafs)
				continue;
			leaf = wl->leafs[i++];
			VectorCopy(leaf->mins, clipmins);
			VectorCopy(leaf->maxs, clipmaxs);
			for (;i < wl->numleafs;i++)
			{
				leaf = wl->leafs[i];
				if (leaf->visframe == r_framecount)
				{
					if (clipmins[0] > leaf->mins[0]) clipmins[0] = leaf->mins[0];
					if (clipmaxs[0] < leaf->maxs[0]) clipmaxs[0] = leaf->maxs[0];
					if (clipmins[1] > leaf->mins[1]) clipmins[1] = leaf->mins[1];
					if (clipmaxs[1] < leaf->maxs[1]) clipmaxs[1] = leaf->maxs[1];
					if (clipmins[2] > leaf->mins[2]) clipmins[2] = leaf->mins[2];
					if (clipmaxs[2] < leaf->maxs[2]) clipmaxs[2] = leaf->maxs[2];
				}
			}
			if (clipmins[0] < wl->mins[0]) clipmins[0] = wl->mins[0];
			if (clipmaxs[0] > wl->maxs[0]) clipmaxs[0] = wl->maxs[0];
			if (clipmins[1] < wl->mins[1]) clipmins[1] = wl->mins[1];
			if (clipmaxs[1] > wl->maxs[1]) clipmaxs[1] = wl->maxs[1];
			if (clipmins[2] < wl->mins[2]) clipmins[2] = wl->mins[2];
			if (clipmaxs[2] > wl->maxs[2]) clipmaxs[2] = wl->maxs[2];
		}
		else
		{
			VectorCopy(wl->mins, clipmins);
			VectorCopy(wl->maxs, clipmaxs);
		}

		//if (R_Shadow_ScissorForBBoxAndSphere(clipmins, clipmaxs, wl->origin, wl->cullradius))
		if (R_CullBox(clipmins, clipmaxs) || R_Shadow_ScissorForBBox(clipmins, clipmaxs))
			continue;

		// mark the leafs we care about so only things in those leafs will matter
		if (cl.worldmodel != NULL)
			for (i = 0;i < wl->numleafs;i++)
				wl->leafs[i]->worldnodeframe = shadowframecount;

		f = d_lightstylevalue[wl->style] * (1.0f / 256.0f);
		VectorScale(wl->light, f, lightcolor);
		if (wl->selected)
		{
			f = 2 + sin(realtime * M_PI * 4.0);
			VectorScale(lightcolor, f, lightcolor);
		}

		if (wl->castshadows)
		{
			if (!visiblevolumes)
				R_Shadow_Stage_ShadowVolumes();
			ent = &cl_entities[0].render;
			if (wl->shadowvolume && r_shadow_staticworldlights.integer)
				R_Shadow_DrawWorldLightShadowVolume(&ent->matrix, wl);
			else
				R_TestAndDrawShadowVolume(ent, wl->origin, cullradius, lightradius, wl->mins, wl->maxs, clipmins, clipmaxs);
			if (r_drawentities.integer)
				for (i = 0;i < r_refdef.numentities;i++)
					R_TestAndDrawShadowVolume(r_refdef.entities[i], wl->origin, cullradius, lightradius, wl->mins, wl->maxs, clipmins, clipmaxs);
		}

		if (!visiblevolumes)
		{
			if (wl->castshadows)
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
					 && ent->maxs[0] >= clipmins[0] && ent->mins[0] <= clipmaxs[0]
					 && ent->maxs[1] >= clipmins[1] && ent->mins[1] <= clipmaxs[1]
					 && ent->maxs[2] >= clipmins[2] && ent->mins[2] <= clipmaxs[2]
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
	for (lnum = 0, rd = r_dlight;lnum < r_numdlights;lnum++, rd++)
	{
		lightradius = rd->cullradius;
		clipmins[0] = rd->origin[0] - lightradius;
		clipmins[1] = rd->origin[1] - lightradius;
		clipmins[2] = rd->origin[2] - lightradius;
		clipmaxs[0] = rd->origin[0] + lightradius;
		clipmaxs[1] = rd->origin[1] + lightradius;
		clipmaxs[2] = rd->origin[2] + lightradius;
		if (VIS_CullBox(clipmins, clipmaxs))
			continue;

		//if (R_Shadow_ScissorForBBoxAndSphere(clipmins, clipmaxs, rd->origin, cullradius))
		if (R_Shadow_ScissorForBBox(clipmins, clipmaxs))
			continue;

		if (!visiblevolumes)
			R_Shadow_Stage_ShadowVolumes();

		cullradius = RadiusFromBoundsAndOrigin(clipmins, clipmaxs, rd->origin);
		VectorScale(rd->light, (1.0f / 4096.0f), lightcolor);

		ent = &cl_entities[0].render;
		R_TestAndDrawShadowVolume(ent, rd->origin, cullradius, lightradius, clipmins, clipmaxs, clipmins, clipmaxs);
		if (r_drawentities.integer)
		{
			for (i = 0;i < r_refdef.numentities;i++)
			{
				ent = r_refdef.entities[i];
				if (ent != rd->ent)
					R_TestAndDrawShadowVolume(ent, rd->origin, cullradius, lightradius, clipmins, clipmaxs, clipmins, clipmaxs);
			}
		}

		if (!visiblevolumes)
		{
			R_Shadow_Stage_LightWithShadows();

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
					 && ent->maxs[0] >= clipmins[0] && ent->mins[0] <= clipmaxs[0]
					 && ent->maxs[1] >= clipmins[1] && ent->mins[1] <= clipmaxs[1]
					 && ent->maxs[2] >= clipmins[2] && ent->mins[2] <= clipmaxs[2]
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

	if (!visiblevolumes)
		R_Shadow_Stage_End();
	qglEnable(GL_CULL_FACE);
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

	R_Mesh_GetSpace(3);
	r = 64000;
	varray_vertex3f[0] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r - vup[0] * r;
	varray_vertex3f[1] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r - vup[1] * r;
	varray_vertex3f[2] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r - vup[2] * r;
	varray_vertex3f[3] = r_origin[0] + vpn[0] * 1.5 - vright[0] * r + vup[0] * r * 3;
	varray_vertex3f[4] = r_origin[1] + vpn[1] * 1.5 - vright[1] * r + vup[1] * r * 3;
	varray_vertex3f[5] = r_origin[2] + vpn[2] * 1.5 - vright[2] * r + vup[2] * r * 3;
	varray_vertex3f[6] = r_origin[0] + vpn[0] * 1.5 + vright[0] * r * 3 - vup[0] * r;
	varray_vertex3f[7] = r_origin[1] + vpn[1] * 1.5 + vright[1] * r * 3 - vup[1] * r;
	varray_vertex3f[8] = r_origin[2] + vpn[2] * 1.5 + vright[2] * r * 3 - vup[2] * r;
	GL_Color(r_refdef.viewblend[0], r_refdef.viewblend[1], r_refdef.viewblend[2], r_refdef.viewblend[3]);
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

	if (r_shadow_realtime.integer == 1)
	{
#if 0
		if (!gl_texturecubemap)
		{
			Con_Printf("Cubemap texture support not detected, turning off r_shadow_realtime\n");
			Cvar_SetValueQuick(&r_shadow_realtime, 0);
		}
		else if (!gl_dot3arb)
		{
			Con_Printf("Bumpmapping support not detected, turning off r_shadow_realtime\n");
			Cvar_SetValueQuick(&r_shadow_realtime, 0);
		}
		else if (!gl_combine.integer)
		{
			Con_Printf("Combine disabled, please turn on gl_combine, turning off r_shadow_realtime\n");
			Cvar_SetValueQuick(&r_shadow_realtime, 0);
		}
		else
#endif
		if (!gl_stencil)
		{
			Con_Printf("Stencil not enabled, turning off r_shadow_realtime, please type vid_stencil 1;vid_bitsperpixel 32;vid_restart and try again\n");
			Cvar_SetValueQuick(&r_shadow_realtime, 0);
		}
	}

	R_Shadow_UpdateLightingMode();

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

	R_WorldVisibility(world);
	R_TimeReport("worldvis");

	R_FarClip_Start(r_origin, vpn, 768.0f);
	R_MarkEntities();
	r_farclip = R_FarClip_Finish() + 256.0f;
	R_TimeReport("markentity");

	GL_SetupView_ViewPort(r_refdef.x, r_refdef.y, r_refdef.width, r_refdef.height);
	if (r_shadow_lightingmode > 0)
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

	R_DrawModels(r_shadow_lightingmode > 0);
	R_TimeReport("models");

	if (r_shadows.integer == 1 && r_shadow_lightingmode <= 0)
	{
		R_DrawFakeShadows();
		R_TimeReport("fakeshadow");
	}

	if (r_shadow_lightingmode > 0)
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
	if (r_shadow_realtime.integer >= 2)
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
	float *v, *c, f1, f2, diff[3];
	rmeshstate_t m;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	R_Mesh_GetSpace(8);
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

void R_DrawNoModelCallback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	int i;
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

	GL_UseColorArray();
	R_Mesh_GetSpace(6);
	varray_vertex3f[ 0] = -16;varray_vertex3f[ 1] =   0;varray_vertex3f[ 2] =   0;
	varray_vertex3f[ 3] =  16;varray_vertex3f[ 4] =   0;varray_vertex3f[ 5] =   0;
	varray_vertex3f[ 6] =   0;varray_vertex3f[ 7] = -16;varray_vertex3f[ 8] =   0;
	varray_vertex3f[ 9] =   0;varray_vertex3f[10] =  16;varray_vertex3f[11] =   0;
	varray_vertex3f[12] =   0;varray_vertex3f[13] =   0;varray_vertex3f[14] = -16;
	varray_vertex3f[15] =   0;varray_vertex3f[16] =   0;varray_vertex3f[17] =  16;
	varray_color4f[ 0] = 0.00f * r_colorscale;varray_color4f[ 1] = 0.00f * r_colorscale;varray_color4f[ 2] = 0.50f * r_colorscale;varray_color4f[ 3] = ent->alpha;
	varray_color4f[ 4] = 0.00f * r_colorscale;varray_color4f[ 5] = 0.00f * r_colorscale;varray_color4f[ 6] = 0.50f * r_colorscale;varray_color4f[ 7] = ent->alpha;
	varray_color4f[ 8] = 0.00f * r_colorscale;varray_color4f[ 9] = 0.50f * r_colorscale;varray_color4f[10] = 0.00f * r_colorscale;varray_color4f[11] = ent->alpha;
	varray_color4f[12] = 0.00f * r_colorscale;varray_color4f[13] = 0.50f * r_colorscale;varray_color4f[14] = 0.00f * r_colorscale;varray_color4f[15] = ent->alpha;
	varray_color4f[16] = 0.50f * r_colorscale;varray_color4f[17] = 0.00f * r_colorscale;varray_color4f[18] = 0.00f * r_colorscale;varray_color4f[19] = ent->alpha;
	varray_color4f[20] = 0.50f * r_colorscale;varray_color4f[21] = 0.00f * r_colorscale;varray_color4f[22] = 0.00f * r_colorscale;varray_color4f[23] = ent->alpha;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_origin, diff);
		f2 = exp(fogdensity/DotProduct(diff, diff));
		f1 = 1 - f2;
		for (i = 0, c = varray_color4f;i < 6;i++, c += 4)
		{
			c[0] = (c[0] * f1 + fogcolor[0] * f2) * r_colorscale;
			c[1] = (c[1] * f1 + fogcolor[1] * f2) * r_colorscale;
			c[2] = (c[2] * f1 + fogcolor[2] * f2) * r_colorscale;
		}
	}
	else
	{
		for (i = 0, c = varray_color4f;i < 6;i++, c += 4)
		{
			c[0] *= r_colorscale;
			c[1] *= r_colorscale;
			c[2] *= r_colorscale;
		}
	}
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

void R_DrawSpriteMesh(const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2)
{
	R_Mesh_GetSpace(4);
	varray_texcoord2f[0][0] = 1;varray_texcoord2f[0][1] = 1;
	varray_texcoord2f[0][2] = 1;varray_texcoord2f[0][3] = 0;
	varray_texcoord2f[0][4] = 0;varray_texcoord2f[0][5] = 0;
	varray_texcoord2f[0][6] = 0;varray_texcoord2f[0][7] = 1;
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

