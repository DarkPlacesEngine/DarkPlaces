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

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys, c_light_polys, c_nodes, c_leafs;

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

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

//cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_speeds2 = {"r_speeds2","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
//cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_waterripple = {"r_waterripple","0"};
cvar_t	r_fullbrights = {"r_fullbrights", "1"};

cvar_t	contrast = {"contrast", "1.0", TRUE}; // LordHavoc: a method of operating system independent color correction
cvar_t	brightness = {"brightness", "1.0", TRUE}; // LordHavoc: a method of operating system independent color correction
cvar_t	gl_lightmode = {"gl_lightmode", "1", TRUE}; // LordHavoc: overbright lighting
//cvar_t	r_dynamicbothsides = {"r_dynamicbothsides", "1"}; // LordHavoc: can disable dynamic lighting of backfaces, but quake maps are weird so it doesn't always work right...
cvar_t	r_farclip = {"r_farclip", "6144"};

cvar_t	gl_fogenable = {"gl_fogenable", "0"};
cvar_t	gl_fogdensity = {"gl_fogdensity", "0.25"};
cvar_t	gl_fogred = {"gl_fogred","0.3"};
cvar_t	gl_foggreen = {"gl_foggreen","0.3"};
cvar_t	gl_fogblue = {"gl_fogblue","0.3"};
cvar_t	gl_fogstart = {"gl_fogstart", "0"};
cvar_t	gl_fogend = {"gl_fogend","0"};
cvar_t	glfog = {"glfog", "0"};

qboolean lighthalf;

vec3_t fogcolor;
vec_t fogdensity;
float fog_density, fog_red, fog_green, fog_blue;
qboolean fogenabled;
qboolean oldgl_fogenable;
void FOG_framebegin()
{
	if (nehahra)
	{
//		if (!Nehahrademcompatibility)
//			gl_fogenable.value = 0;
		if (gl_fogenable.value)
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
			if (lighthalf)
			{
				fogcolor[0] *= 0.5f;
				fogcolor[1] *= 0.5f;
				fogcolor[2] *= 0.5f;
			}
	}
	if (glfog.value)
	{
		if (!r_render.value)
			return;
		if(fog_density)
		{
			// LordHavoc: Borland C++ 5.0 was choking on this line, stupid compiler...
			//GLfloat colors[4] = {(GLfloat) gl_fogred.value, (GLfloat) gl_foggreen.value, (GLfloat) gl_fogblue.value, (GLfloat) 1};
			GLfloat colors[4];
			colors[0] = fog_red;
			colors[1] = fog_green;
			colors[2] = fog_blue;
			colors[3] = 1;
			if (lighthalf)
			{
				colors[0] *= 0.5f;
				colors[1] *= 0.5f;
				colors[2] *= 0.5f;
			}

			glFogi (GL_FOG_MODE, GL_EXP2);
			glFogf (GL_FOG_DENSITY, (GLfloat) fog_density / 100); 
			glFogfv (GL_FOG_COLOR, colors);
			glEnable (GL_FOG);
		}
		else
			glDisable(GL_FOG);
	}
	else
	{
		if (fog_density)
		{
			fogenabled = true;
			fogdensity = -4000.0f / (fog_density * fog_density);
			// fog color was already set
		}
		else
			fogenabled = false;
	}
}

void FOG_frameend()
{
	if (glfog.value)
		glDisable(GL_FOG);
}

void FOG_clear()
{
	if (nehahra)
	{
		Cvar_Set("gl_fogenable", "0");
		Cvar_Set("gl_fogdensity", "0.2");
		Cvar_Set("gl_fogred", "0.3");
		Cvar_Set("gl_foggreen", "0.3");
		Cvar_Set("gl_fogblue", "0.3");
	}
	fog_density = fog_red = fog_green = fog_blue = 0.0f;
}

void FOG_registercvars()
{
	Cvar_RegisterVariable (&glfog);
	if (nehahra)
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

void glmain_start()
{
}

void glmain_shutdown()
{
}

void GL_Main_Init()
{
	FOG_registercvars();
	Cvar_RegisterVariable (&r_speeds2);
	Cvar_RegisterVariable (&contrast);
	Cvar_RegisterVariable (&brightness);
	Cvar_RegisterVariable (&gl_lightmode);
//	Cvar_RegisterVariable (&r_dynamicwater);
//	Cvar_RegisterVariable (&r_dynamicbothsides);
	Cvar_RegisterVariable (&r_fullbrights);
	if (nehahra)
		Cvar_SetValue("r_fullbrights", 0);
//	if (gl_vendor && strstr(gl_vendor, "3Dfx"))
//		gl_lightmode.value = 0;
	Cvar_RegisterVariable (&r_fullbright);
	R_RegisterModule("GL_Main", glmain_start, glmain_shutdown);
}

extern void GL_Draw_Init();
extern void GL_Main_Init();
extern void GL_Models_Init();
extern void GL_Poly_Init();
extern void GL_Surf_Init();
extern void GL_Screen_Init();
extern void GL_Misc_Init();
extern void R_Crosshairs_Init();
extern void R_Light_Init();
extern void R_Particles_Init();

void Render_Init()
{
	R_ShutdownModules();
	GL_Draw_Init();
	GL_Main_Init();
	GL_Models_Init();
	GL_Poly_Init();
	GL_Surf_Init();
	GL_Screen_Init();
	GL_Misc_Init();
	R_Crosshairs_Init();
	R_Light_Init();
	R_Particles_Init();
	R_StartModules();
}

/*
===============
GL_Init
===============
*/
extern char *QSG_EXTENSIONS;
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

	VID_CheckMultiTexture();
	VID_CheckVertexArrays();

	// LordHavoc: report supported extensions
	Con_Printf ("\nQSG extensions: %s\n", QSG_EXTENSIONS);

	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
//	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);

//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	Palette_Init();
}


/*
void R_RotateForEntity (entity_t *e)
{
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	glRotatef (e->angles[1],  0, 0, 1);
	glRotatef (-e->angles[0],  0, 1, 0);
	glRotatef (e->angles[2],  1, 0, 0);

	glScalef (e->scale, e->scale, e->scale); // LordHavoc: model scale
}
*/

// LordHavoc: if not for the fact BRIGHTFIELD particles require this, it would be removed...
#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// LordHavoc: shading stuff
vec3_t	shadevector;
vec3_t	shadecolor;

float	modelalpha;

//==================================================================================

void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);

/*
=============
R_DrawEntitiesOnList
=============
*/
// LordHavoc: split so bmodels are rendered before any other objects
void R_DrawEntitiesOnList1 (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if (cl_visedicts[i]->model->type != mod_brush)
			continue;
		currententity = cl_visedicts[i];
		modelalpha = currententity->alpha;

		R_DrawBrushModel (currententity);
	}
}

void R_DrawEntitiesOnList2 (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];
		modelalpha = currententity->alpha;

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity, true, modelalpha, currententity->model, currententity->frame, currententity->skinnum, currententity->origin, currententity->effects, currententity->model->flags, currententity->colormap);
			break;

		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;

		default:
			break;
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
	if (!r_drawviewmodel.value || chase_active.value || envmap || !r_drawentities.value || cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0 || !cl.viewent.model)
		return;

	currententity = &cl.viewent;
	currententity->alpha = modelalpha = cl_entities[cl.viewentity].alpha; // LordHavoc: if the player is transparent, so is his gun
	currententity->effects = cl_entities[cl.viewentity].effects;
	currententity->scale = 1;
	VectorCopy(cl_entities[cl.viewentity].colormod, currententity->colormod);

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity, FALSE, modelalpha, currententity->model, currententity->frame, currententity->skinnum, currententity->origin, currententity->effects, currententity->model->flags, currententity->colormap);
	glDepthRange (gldepthmin, gldepthmax);
}

void R_DrawBrushModel (entity_t *e);

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );

void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
//		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
		BoxOnPlaneSideClassify(&frustum[i]);
	}
}

void R_AnimateLight (void);
void V_CalcBlend (void);

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("r_fullbright", "0");
		Cvar_Set ("r_ambient", "0");
	}

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
	c_light_polys = 0;
	c_nodes = 0;
	c_leafs = 0;

}


void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


extern char skyname[];

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	if (!r_render.value)
		return;
	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glViewport (glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
//	if (skyname[0]) // skybox enabled?
//		MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  r_skyboxsize.value*1.732050807569 + 256); // this is size*sqrt(3) + 256
//	else
		MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  r_farclip.value);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
//	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
//	else
//		glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND); // was Disable
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	glShadeModel(GL_SMOOTH);
}

void R_DrawWorld (void);
//void R_RenderDlights (void);
void R_DrawParticles (void);

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (!r_render.value)
		return;
//	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: moved to SCR_UpdateScreen
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

// LordHavoc: my trick to *FIX* GLQuake lighting once and for all :)
void GL_Brighten()
{
	if (!r_render.value)
		return;
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glBegin (GL_TRIANGLES);
	glColor3f (1, 1, 1);
	glVertex2f (-5000, -5000);
	glVertex2f (10000, -5000);
	glVertex2f (-5000, 10000);
	glEnd ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable (GL_DEPTH_TEST);
	glEnable (GL_CULL_FACE);
}

extern cvar_t contrast;
extern cvar_t brightness;
extern cvar_t gl_lightmode;

void GL_BlendView()
{
	if (!r_render.value)
		return;
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	if (lighthalf)
	{
		glBlendFunc (GL_DST_COLOR, GL_ONE);
		glBegin (GL_TRIANGLES);
		glColor3f (1, 1, 1);
		glVertex2f (-5000, -5000);
		glVertex2f (10000, -5000);
		glVertex2f (-5000, 10000);
		glEnd ();
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	contrast.value = bound(0.2, contrast.value, 1.0);
	if (/*gl_polyblend.value && */v_blend[3])
	{
		glBegin (GL_TRIANGLES);
		glColor4fv (v_blend);
		glVertex2f (-5000, -5000);
		glVertex2f (10000, -5000);
		glVertex2f (-5000, 10000);
		glEnd ();
	}

	glEnable (GL_CULL_FACE);
	glEnable (GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

#define TIMEREPORT(DESC) \
	if (r_speeds2.value)\
	{\
		temptime = -currtime;\
		currtime = Sys_FloatTime();\
		temptime += currtime;\
		Con_Printf(DESC " %.4fms ", temptime * 1000.0);\
	}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
extern qboolean intimerefresh;
extern qboolean skyisvisible;
extern void R_Sky();
extern void UploadLightmaps();
void R_RenderView (void)
{
//	double currtime, temptime;
//	if (r_norefresh.value)
//		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	lighthalf = gl_lightmode.value;

	FOG_framebegin();

//	if (r_speeds2.value)
//	{
//		currtime = Sys_FloatTime();
//		Con_Printf("render time: ");
//	}
	R_Clear();
//	TIMEREPORT("R_Clear")

	// render normal view

	R_SetupFrame ();
	R_SetFrustum ();
	R_SetupGL ();

	skypolyclear();
	wallpolyclear();
	transpolyclear();
	skyisvisible = false;

	R_MarkLeaves ();	// done here so we know if we're in water
	R_DrawWorld ();		// adds static entities to the list
	if (!intimerefresh)
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
	R_DrawEntitiesOnList1 (); // BSP models

	skypolyrender(); // fogged sky polys, affects depth
	if (skyname[0] && skyisvisible && !fogenabled)
		R_Sky(); // does not affect depth, draws over the sky polys

	UploadLightmaps();
	wallpolyrender();

	R_DrawEntitiesOnList2 (); // other models
//	R_RenderDlights ();
	R_DrawViewModel ();
	R_DrawParticles ();

	transpolyrender();

	FOG_frameend();
	GL_BlendView();
//	if (r_speeds2.value)
//		Con_Printf("\n");
}
