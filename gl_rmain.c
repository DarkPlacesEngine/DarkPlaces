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

//static qboolean	r_cache_thrash;		// compatability

entity_render_t	*currentrenderentity;

int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;

qboolean	envmap;				// true during envmap command capture

// LordHavoc: moved all code related to particles into r_part.c
//int			particletexture;	// little dot for particles
//int			playertextures;		// up to 16 color translated skins

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//float	r_world_matrix[16];
//float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

//cvar_t	r_norefresh = {0, "r_norefresh","0"};
cvar_t	r_drawentities = {0, "r_drawentities","1"};
cvar_t	r_drawviewmodel = {0, "r_drawviewmodel","1"};
cvar_t	r_speeds = {0, "r_speeds","0"};
cvar_t	r_fullbright = {0, "r_fullbright","0"};
//cvar_t	r_lightmap = {0, "r_lightmap","0"};
cvar_t	r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t	r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t	r_waterripple = {CVAR_SAVE, "r_waterripple","0"};
cvar_t	r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};

cvar_t	gl_lightmode = {CVAR_SAVE, "gl_lightmode", "1"}; // LordHavoc: overbright lighting
//cvar_t	r_dynamicbothsides = {CVAR_SAVE, "r_dynamicbothsides", "1"}; // LordHavoc: can disable dynamic lighting of backfaces, but quake maps are weird so it doesn't always work right...

cvar_t	gl_fogenable = {0, "gl_fogenable", "0"};
cvar_t	gl_fogdensity = {0, "gl_fogdensity", "0.25"};
cvar_t	gl_fogred = {0, "gl_fogred","0.3"};
cvar_t	gl_foggreen = {0, "gl_foggreen","0.3"};
cvar_t	gl_fogblue = {0, "gl_fogblue","0.3"};
cvar_t	gl_fogstart = {0, "gl_fogstart", "0"};
cvar_t	gl_fogend = {0, "gl_fogend","0"};

cvar_t	r_ser = {CVAR_SAVE, "r_ser", "1"};
cvar_t	gl_viewmodeldepthhack = {0, "gl_viewmodeldepthhack", "1"};

cvar_t r_multitexture = {0, "r_multitexture", "1"};

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
qboolean intimerefresh = 0;
static void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	intimerefresh = 1;
	start = Sys_DoubleTime ();
	glDrawBuffer (GL_FRONT);
	for (i = 0;i < 128;i++)
	{
		r_refdef.viewangles[0] = 0;
		r_refdef.viewangles[1] = i/128.0*360.0;
		r_refdef.viewangles[2] = 0;
		R_RenderView();
	}
	glDrawBuffer  (GL_BACK);

	stop = Sys_DoubleTime ();
	intimerefresh = 0;
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

extern cvar_t r_drawportals;

int R_VisibleCullBox (vec3_t mins, vec3_t maxs)
{
	int sides;
	mnode_t *nodestack[8192], *node;
	int stack = 0;

	if (R_CullBox(mins, maxs))
		return true;

	node = cl.worldmodel->nodes;
loc0:
	if (node->contents < 0)
	{
		if (((mleaf_t *)node)->visframe == r_framecount)
			return false;
		if (!stack)
			return true;
		node = nodestack[--stack];
		goto loc0;
	}

	sides = BOX_ON_PLANE_SIDE(mins, maxs, node->plane);

// recurse down the contacted sides
	if (sides & 1)
	{
		if (sides & 2) // 3
		{
			// put second child on the stack for later examination
			nodestack[stack++] = node->children[1];
			node = node->children[0];
			goto loc0;
		}
		else // 1
		{
			node = node->children[0];
			goto loc0;
		}
	}
	// 2
	node = node->children[1];
	goto loc0;
}

qboolean lighthalf;

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

void gl_main_newmap(void)
{
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
	Cvar_RegisterVariable (&gl_lightmode);
//	Cvar_RegisterVariable (&r_dynamicwater);
//	Cvar_RegisterVariable (&r_dynamicbothsides);
	Cvar_RegisterVariable (&r_fullbrights);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_waterripple);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_ser);
	Cvar_RegisterVariable (&gl_viewmodeldepthhack);
	Cvar_RegisterVariable (&r_multitexture);
	if (gamemode == GAME_NEHAHRA)
		Cvar_SetValue("r_fullbrights", 0);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);
}

/*
===============
R_NewMap
===============
*/
void CL_ParseEntityLump(char *entitystring);
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_viewleaf = NULL;
	if (cl.worldmodel->entities)
		CL_ParseEntityLump(cl.worldmodel->entities);
	R_Modules_NewMap();
}

extern void R_Textures_Init(void);
extern void Mod_RenderInit(void);
extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void GL_Models_Init(void);
extern void R_Sky_Init(void);
extern void GL_Surf_Init(void);
extern void GL_Screen_Init(void);
extern void R_Crosshairs_Init(void);
extern void R_Light_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void R_Clip_Init(void);
extern void ui_init(void);
extern void gl_backend_init(void);

void Render_Init(void)
{
	R_Modules_Shutdown();
	R_Textures_Init();
	Mod_RenderInit();
	gl_backend_init();
	R_Clip_Init();
	GL_Draw_Init();
	GL_Main_Init();
	GL_Models_Init();
	R_Sky_Init();
	GL_Surf_Init();
	GL_Screen_Init();
	R_Crosshairs_Init();
	R_Light_Init();
	R_Particles_Init();
	R_Explosion_Init();
	R_Decals_Init();
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
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

//	Con_Printf ("%s %s\n", gl_renderer, gl_version);

	VID_CheckExtensions();

	// LordHavoc: report supported extensions
	Con_Printf ("\nengine extensions: %s\n", ENGINE_EXTENSIONS);

	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
}


//==================================================================================

void R_Entity_Callback(void *data, void *junk)
{
	((entity_render_t *)data)->visframe = r_framecount;
}

static void R_MarkEntities (void)
{
	int		i;
	vec3_t	v;

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		currentrenderentity = r_refdef.entities[i];
		Mod_CheckLoaded(currentrenderentity->model);

		// move view-relative models to where they should be
		if (currentrenderentity->flags & RENDER_VIEWMODEL)
		{
			// remove flag so it will not be repeated incase RelinkEntities is not called again for a while
			currentrenderentity->flags -= RENDER_VIEWMODEL;
			// transform origin
			VectorCopy(currentrenderentity->origin, v);
			currentrenderentity->origin[0] = v[0] * vpn[0] + v[1] * vright[0] + v[2] * vup[0] + r_origin[0];
			currentrenderentity->origin[1] = v[0] * vpn[1] + v[1] * vright[1] + v[2] * vup[1] + r_origin[1];
			currentrenderentity->origin[2] = v[0] * vpn[2] + v[1] * vright[2] + v[2] * vup[2] + r_origin[2];
			// adjust angles
			VectorAdd(currentrenderentity->angles, r_refdef.viewangles, currentrenderentity->angles);
		}

		if (currentrenderentity->angles[0] || currentrenderentity->angles[2])
		{
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->rotatedmins, currentrenderentity->mins);
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->rotatedmaxs, currentrenderentity->maxs);
		}
		else if (currentrenderentity->angles[1])
		{
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->yawmins, currentrenderentity->mins);
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->yawmaxs, currentrenderentity->maxs);
		}
		else
		{
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->normalmins, currentrenderentity->mins);
			VectorMA(currentrenderentity->origin, currentrenderentity->scale, currentrenderentity->model->normalmaxs, currentrenderentity->maxs);
		}
		if (R_VisibleCullBox(currentrenderentity->mins, currentrenderentity->maxs))
			continue;

		R_LerpAnimation(currentrenderentity);
		if (r_ser.integer)
			currentrenderentity->model->SERAddEntity();
		else
			currentrenderentity->visframe = r_framecount;
	}
}

// only used if skyrendermasked, and normally returns false
int R_DrawBModelSky (void)
{
	int		i, sky = false;

	if (!r_drawentities.integer)
		return false;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		currentrenderentity = r_refdef.entities[i];
		if (currentrenderentity->visframe == r_framecount && currentrenderentity->model->DrawSky)
		{
			currentrenderentity->model->DrawSky();
			sky = true;
		}
	}
	return sky;
}

void R_DrawModels (void)
{
	int		i;

	if (!r_drawentities.integer)
		return;

	for (i = 0;i < r_refdef.numentities;i++)
	{
		currentrenderentity = r_refdef.entities[i];
		if (currentrenderentity->visframe == r_framecount && currentrenderentity->model->Draw)
			currentrenderentity->model->Draw();
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	// FIXME: move these checks to client
	if (!r_drawviewmodel.integer || chase_active.integer || envmap || !r_drawentities.integer || cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0 || !cl.viewent.render.model)
		return;

	currentrenderentity = &cl.viewent.render;
	Mod_CheckLoaded(currentrenderentity->model);

	R_LerpAnimation(currentrenderentity);

	// hack the depth range to prevent view model from poking into walls
	if (gl_viewmodeldepthhack.integer)
	{
		R_Mesh_Render();
		glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	}
	currentrenderentity->model->Draw();
	if (gl_viewmodeldepthhack.integer)
	{
		R_Mesh_Render();
		glDepthRange (gldepthmin, gldepthmax);
	}
}

static void R_SetFrustum (void)
{
	int		i;

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


	for (i=0 ; i<4 ; i++)
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
	if (r_multitexture.integer && gl_textureunits < 2)
		Cvar_SetValue("r_multitexture", 0);

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

//	r_cache_thrash = false;

	R_AnimateLight ();
}


static void MYgluPerspective(GLdouble fovx, GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmax, ymax;

	xmax = zNear * tan( fovx * M_PI / 360.0 ) * aspect;
	ymax = zNear * tan( fovy * M_PI / 360.0 );

	if (r_viewleaf->contents != CONTENTS_EMPTY && r_viewleaf->contents != CONTENTS_SOLID)
	{
		xmax *= (sin(cl.time * 4.7) * 0.03 + 0.97);
		ymax *= (sin(cl.time * 3.0) * 0.03 + 0.97);
	}

	glFrustum(-xmax, xmax, -ymax, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
static void R_SetupGL (void)
{
	if (!r_render.integer)
		return;

//	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: moved to SCR_UpdateScreen
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);

	// update farclip based on previous frame
	r_farclip = r_newfarclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();

	// y is weird beause OpenGL is bottom to top, we use top to bottom
	glViewport(r_refdef.x, vid.realheight - (r_refdef.y + r_refdef.height), r_refdef.width, r_refdef.height);
//	yfov = 2*atan((float)r_refdef.height/r_refdef.width)*180/M_PI;
	MYgluPerspective (r_refdef.fov_x, r_refdef.fov_y, r_refdef.width/r_refdef.height, 4, r_farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
	glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
	glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

//	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
//	if (gl_cull.integer)
		glEnable(GL_CULL_FACE);
//	else
//		glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND); // was Disable
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
}

static void R_BlendView(void)
{
	if (!r_render.integer)
		return;

	if (r_refdef.viewblend[3] < 0.01f)
		return;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	glOrtho  (0, 1, 1, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity ();
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_TRIANGLES);
	if (lighthalf)
		glColor4f (r_refdef.viewblend[0] * 0.5f, r_refdef.viewblend[1] * 0.5f, r_refdef.viewblend[2] * 0.5f, r_refdef.viewblend[3]);
	else
		glColor4fv (r_refdef.viewblend);
	glVertex2f (-5, -5);
	glVertex2f (10, -5);
	glVertex2f (-5, 10);
	glEnd ();

	glEnable (GL_CULL_FACE);
	glEnable (GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	if (!cl.worldmodel)
		Host_Error ("R_RenderView: NULL worldmodel");

	// FIXME: move to client
	R_MoveExplosions();
	R_TimeReport("mexplosion");

	R_SetupFrame();
	R_SetFrustum();
	R_SetupGL();
	R_SetupFog();
	R_SkyStartFrame();
	R_Mesh_Clear();
	if (r_ser.integer)
		R_Clip_StartFrame();
	R_BuildLightList();

	R_TimeReport("setup");

	R_DrawWorld();
	R_TimeReport("worldnode");

	R_MarkEntities();
	R_TimeReport("markentity");

	if (r_ser.integer)
	{
		R_Clip_EndFrame();
		R_TimeReport("hiddensurf");
	}

	R_MarkWorldLights();
	R_TimeReport("marklights");

	if (skyrendermasked && R_DrawBModelSky())
	{
		R_TimeReport("bmodelsky");
	}

	R_SetupForWorldRendering();
	R_PrepareSurfaces();
	R_TimeReport("surfprep");

	R_DrawSurfacesAll();
	R_TimeReport("surfdraw");

	if (r_drawportals.integer)
	{
		R_DrawPortals();
		R_TimeReport("portals");
	}

	// don't let sound skip if going slow
	if (!intimerefresh && !r_speeds.integer)
		S_ExtraUpdate ();

	R_DrawViewModel();
	R_DrawModels();
	R_TimeReport("models");

	R_DrawDecals();
	R_TimeReport("decals");

	R_DrawParticles();
	R_TimeReport("particles");

	R_DrawExplosions();
	R_TimeReport("explosions");

	// draw transparent meshs
	R_Mesh_AddTransparent();
	R_TimeReport("addtrans");

	R_DrawCoronas();
	R_TimeReport("coronas");

	// render any queued meshs
	R_Mesh_Render();
	R_TimeReport("meshrender");

	R_BlendView();
	R_TimeReport("blendview");

	//Mem_CheckSentinelsGlobal();
	//R_TimeReport("memtest");
}
