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

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

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

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

float ixtable[4096];

void R_MarkLeaves (void);

//cvar_t	r_norefresh = {0, "r_norefresh","0"};
cvar_t	r_drawentities = {0, "r_drawentities","1"};
cvar_t	r_drawviewmodel = {0, "r_drawviewmodel","1"};
cvar_t	r_speeds = {0, "r_speeds","0"};
cvar_t	r_speeds2 = {0, "r_speeds2","0"};
cvar_t	r_fullbright = {0, "r_fullbright","0"};
//cvar_t	r_lightmap = {0, "r_lightmap","0"};
cvar_t	r_wateralpha = {CVAR_SAVE, "r_wateralpha","1"};
cvar_t	r_dynamic = {CVAR_SAVE, "r_dynamic","1"};
cvar_t	r_waterripple = {CVAR_SAVE, "r_waterripple","0"};
cvar_t	r_fullbrights = {CVAR_SAVE, "r_fullbrights", "1"};

cvar_t	gl_lightmode = {CVAR_SAVE, "gl_lightmode", "1"}; // LordHavoc: overbright lighting
//cvar_t	r_dynamicbothsides = {CVAR_SAVE, "r_dynamicbothsides", "1"}; // LordHavoc: can disable dynamic lighting of backfaces, but quake maps are weird so it doesn't always work right...
cvar_t	r_farclip = {0, "r_farclip", "6144"}; // FIXME: make this go away (calculate based on farthest visible object/polygon)

cvar_t	gl_fogenable = {0, "gl_fogenable", "0"};
cvar_t	gl_fogdensity = {0, "gl_fogdensity", "0.25"};
cvar_t	gl_fogred = {0, "gl_fogred","0.3"};
cvar_t	gl_foggreen = {0, "gl_foggreen","0.3"};
cvar_t	gl_fogblue = {0, "gl_fogblue","0.3"};
cvar_t	gl_fogstart = {0, "gl_fogstart", "0"};
cvar_t	gl_fogend = {0, "gl_fogend","0"};
cvar_t	glfog = {0, "glfog", "0"};

/*
int R_VisibleCullBox (vec3_t mins, vec3_t maxs)
{
	int sides;
	mnode_t *nodestack[8192], *node;
	int stack = 0;

	node = cl.worldmodel->nodes;
loc0:
	if (node->contents < 0)
	{
		if (((mleaf_t *)node)->visframe == r_framecount)
		{
			if (R_CullBox(mins, maxs))
				return true;
			return false;
		}
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
*/

qboolean lighthalf;

vec3_t fogcolor;
vec_t fogdensity;
float fog_density, fog_red, fog_green, fog_blue;
qboolean fogenabled;
qboolean oldgl_fogenable;
void FOG_framebegin(void)
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

void FOG_frameend(void)
{
	if (glfog.value)
		glDisable(GL_FOG);
}

void FOG_clear(void)
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

void FOG_registercvars(void)
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

void gl_main_start(void)
{
}

void gl_main_shutdown(void)
{
}

void gl_main_newmap(void)
{
}

void GL_Main_Init(void)
{
	int a;
	FOG_registercvars();
	// LordHavoc: setup 1.0f / N table for quick recipricols of integers
	ixtable[0] = 0;
	for (a = 1;a < 4096;a++)
		ixtable[a] = 1.0f / a;
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_speeds2);
	Cvar_RegisterVariable (&gl_lightmode);
//	Cvar_RegisterVariable (&r_dynamicwater);
//	Cvar_RegisterVariable (&r_dynamicbothsides);
	Cvar_RegisterVariable (&r_fullbrights);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_waterripple);
	Cvar_RegisterVariable (&r_farclip);
	if (nehahra)
		Cvar_SetValue("r_fullbrights", 0);
//	if (gl_vendor && strstr(gl_vendor, "3Dfx"))
//		gl_lightmode.value = 0;
	Cvar_RegisterVariable (&r_fullbright);
	R_RegisterModule("GL_Main", gl_main_start, gl_main_shutdown, gl_main_newmap);
}

extern void GL_Draw_Init(void);
extern void GL_Main_Init(void);
extern void GL_Models_Init(void);
extern void GL_Poly_Init(void);
extern void GL_Surf_Init(void);
extern void GL_Screen_Init(void);
extern void GL_Misc_Init(void);
extern void R_Crosshairs_Init(void);
extern void R_Light_Init(void);
extern void R_Particles_Init(void);
extern void R_Explosion_Init(void);
extern void CL_Effects_Init(void);
extern void R_Clip_Init(void);
extern void ui_init(void);

void Render_Init(void)
{
	R_Modules_Shutdown();
	R_Clip_Init();
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
	R_Explosion_Init();
	CL_Effects_Init();
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

	VID_CheckMultitexture();
	VID_CheckCVA();

	// LordHavoc: report supported extensions
	Con_Printf ("\nengine extensions: %s\n", ENGINE_EXTENSIONS);

	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
//	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5);

//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
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

// LordHavoc: shading stuff
vec3_t	shadevector;
vec3_t	shadecolor;

float	modelalpha;

//==================================================================================

void R_LerpUpdate(entity_t *ent)
{
	int frame;
	frame = ent->render.frame;
	if (ent->render.model && ent->render.frame >= ent->render.model->numframes)
	{
		Con_Printf("R_LerpUpdate: no such frame%6i in \"%s\"\n", ent->render.frame, ent->render.model->name);
		frame = 0;
	}

	if (ent->render.lerp_model != ent->render.model)
	{
		// reset all interpolation information
		ent->render.lerp_model = ent->render.model;
		ent->render.frame1 = ent->render.frame2 = frame;
		ent->render.frame1start = ent->render.frame2start = cl.time;
		ent->render.framelerp = 1;
		ent->render.lerp_starttime = 0;
	}
	else if (ent->render.frame2 != frame)
	{
		// transition to new frame
		ent->render.frame1 = ent->render.frame2;
		ent->render.frame1start = ent->render.frame2start;
		ent->render.frame2 = frame;
		ent->render.frame2start = cl.time;
		ent->render.framelerp = 0;
		ent->render.lerp_starttime = cl.time;
	}
	else
	{
		// lerp_starttime < 0 is used to prevent changing of framelerp
		if (ent->render.lerp_starttime >= 0)
		{
			// update transition
			ent->render.framelerp = (cl.time - ent->render.lerp_starttime) * 10;
			ent->render.framelerp = bound(0, ent->render.framelerp, 1);
		}
	}
}


void R_PrepareEntities (void)
{
	int i;
	entity_t *ent;
	vec3_t v;
	// this updates entities that are supposed to be view relative
	for (i = 0;i < cl_numvisedicts;i++)
	{
		ent = cl_visedicts[i];

		if (ent->render.flags & RENDER_VIEWMODEL)
		{
			// remove flag so it will not be repeated incase RelinkEntities is not called again for a while
			ent->render.flags -= RENDER_VIEWMODEL;
			// transform origin
			VectorCopy(ent->render.origin, v);
			ent->render.origin[0] = v[0] * vpn[0] + v[1] * vright[0] + v[2] * vup[0] + r_origin[0];
			ent->render.origin[1] = v[0] * vpn[1] + v[1] * vright[1] + v[2] * vup[1] + r_origin[1];
			ent->render.origin[2] = v[0] * vpn[2] + v[1] * vright[2] + v[2] * vup[2] + r_origin[2];
			// adjust angles
			VectorAdd(ent->render.angles, r_refdef.viewangles, ent->render.angles);
		}
	}
}

void R_Entity_Callback(void *data, void *junk)
{
	((entity_t *)data)->render.visframe = r_framecount;
}

void R_AddModelEntities (void)
{
	int		i;
	vec3_t	mins, maxs;
	frameblend_t blend[4];

	if (!r_drawentities.value)
		return;

	for (i = 0;i < cl_numvisedicts;i++)
	{
		currententity = cl_visedicts[i];
		if (currententity->render.model->type == mod_brush)
		{
			modelalpha = currententity->render.alpha;
			R_DrawBrushModel (currententity);
		}
		else if (currententity->render.model->type == mod_alias)
		{
			VectorAdd(currententity->render.origin, currententity->render.model->mins, mins);
			VectorAdd(currententity->render.origin, currententity->render.model->maxs, maxs);
			R_Clip_AddBox(mins, maxs, R_Entity_Callback, currententity, NULL);
		}
		else if (currententity->render.model->type == mod_sprite)
		{
			R_LerpUpdate(currententity);
			R_LerpAnimation(currententity->render.model, currententity->render.frame1, currententity->render.frame2, currententity->render.frame1start, currententity->render.frame2start, currententity->render.framelerp, blend);
			R_ClipSprite(currententity, blend);
		}
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
/*
void R_DrawEntitiesOnList1 (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	for (i = 0;i < cl_numvisedicts;i++)
	{
		if (cl_visedicts[i]->render.visframe != r_framecount)
			continue;
		if (cl_visedicts[i]->render.model->type != mod_brush)
			continue;
		currententity = cl_visedicts[i];
		modelalpha = currententity->render.alpha;

		R_DrawBrushModel (currententity);
	}
}
*/

void R_DrawModels (void)
{
	int		i;
	frameblend_t blend[4];
//	vec3_t	mins, maxs;

	if (!r_drawentities.value)
		return;

	for (i = 0;i < cl_numvisedicts;i++)
	{
		if (cl_visedicts[i]->render.visframe != r_framecount)
			continue;
		currententity = cl_visedicts[i];
		if (currententity->render.model->type != mod_alias && currententity->render.model->type != mod_sprite)
			continue;

		modelalpha = currententity->render.alpha;

		if (currententity->render.model->type == mod_alias)
		{
			// only lerp models here because sprites were already lerped for their clip polygon
			R_LerpUpdate(currententity);
			R_LerpAnimation(currententity->render.model, currententity->render.frame1, currententity->render.frame2, currententity->render.frame1start, currententity->render.frame2start, currententity->render.framelerp, blend);
			R_DrawAliasModel (currententity, true, modelalpha, currententity->render.model, blend, currententity->render.skinnum, currententity->render.origin, currententity->render.angles, currententity->render.scale, currententity->render.effects, currententity->render.model->flags, currententity->render.colormap);
		}
		else //if (currententity->render.model->type == mod_sprite)
		{
			// build blend array
			R_LerpAnimation(currententity->render.model, currententity->render.frame1, currententity->render.frame2, currententity->render.frame1start, currententity->render.frame2start, currententity->render.framelerp, blend);
			R_DrawSpriteModel (currententity, blend);
		}

		/*
		VectorAdd(cl_visedicts[i]->render.origin, cl_visedicts[i]->render.model->mins, mins);
		VectorAdd(cl_visedicts[i]->render.origin, cl_visedicts[i]->render.model->maxs, maxs);

		switch (cl_visedicts[i]->render.model->type)
		{
		case mod_alias:
			R_Clip_AddBox(mins, maxs, R_DrawModelCallback, cl_visedicts[i], NULL);
			break;
		case mod_sprite:
			R_Clip_AddBox(mins, maxs, R_DrawSpriteCallback, cl_visedicts[i], NULL);
			break;
		}
		*/
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	frameblend_t blend[4];

	if (!r_drawviewmodel.value || chase_active.value || envmap || !r_drawentities.value || cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0 || !cl.viewent.render.model)
		return;

	currententity = &cl.viewent;
	currententity->render.alpha = modelalpha = cl_entities[cl.viewentity].render.alpha; // LordHavoc: if the player is transparent, so is the gun
	currententity->render.effects = cl_entities[cl.viewentity].render.effects;
	currententity->render.scale = 1;
	VectorCopy(cl_entities[cl.viewentity].render.colormod, currententity->render.colormod);

	R_LerpUpdate(currententity);
	R_LerpAnimation(currententity->render.model, currententity->render.frame1, currententity->render.frame2, currententity->render.frame1start, currententity->render.frame2start, currententity->render.framelerp, blend);

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity, false, modelalpha, currententity->render.model, blend, currententity->render.skinnum, currententity->render.origin, currententity->render.angles, currententity->render.scale, currententity->render.effects, currententity->render.model->flags, currententity->render.colormap);
	glDepthRange (gldepthmin, gldepthmax);
}

void R_DrawBrushModel (entity_t *e);

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );

void R_SetFrustum (void)
{
	int		i;

	// LordHavoc: note to all quake engine coders, this code was making the
	// view frustum taller than it should have been (it assumed the view is
	// square; it is not square), so I disabled it
	/*
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
	*/
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	//}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
//		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
		PlaneClassify(&frustum[i]);
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
	c_faces = 0;
	c_nodes = 0;
	c_leafs = 0;
	c_models = 0;
	c_bmodels = 0;
	c_sprites = 0;
	c_particles = 0;
//	c_dlights = 0;

	R_AnimateLight ();
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


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
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
void GL_Brighten(void)
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

void GL_BlendView(void)
{
	if (!r_render.value)
		return;

	if (v_blend[3] < 0.01f)
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
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_TRIANGLES);
	if (lighthalf)
		glColor4f (v_blend[0] * 0.5f, v_blend[1] * 0.5f, v_blend[2] * 0.5f, v_blend[3]);
	else
		glColor4fv (v_blend);
	glVertex2f (-5000, -5000);
	glVertex2f (10000, -5000);
	glVertex2f (-5000, 10000);
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
extern void R_Sky(void);
extern void UploadLightmaps(void);
extern void R_DrawSurfaces(void);
extern void R_DrawPortals(void);
char r_speeds2_string[1024];
int speedstringcount;

void timestring(int t, char *desc)
{
	char tempbuf[256];
	int length;
	if (t < 1000000)
		sprintf(tempbuf, " %6ius %s", t, desc);
	else
		sprintf(tempbuf, " %6ims %s", t / 1000, desc);
	length = strlen(tempbuf);
//	while (length < 20)
//		tempbuf[length++] = ' ';
//	tempbuf[length] = 0;
	if (speedstringcount + length > 80)
	{
		strcat(r_speeds2_string, "\n");
		speedstringcount = 0;
	}
	// skip the space at the beginning if it's the first on the line
	if (speedstringcount == 0)
	{
		strcat(r_speeds2_string, tempbuf + 1);
		speedstringcount = length - 1;
	}
	else
	{
		strcat(r_speeds2_string, tempbuf);
		speedstringcount += length;
	}
}

#define TIMEREPORT(NAME) \
	if (r_speeds2.value)\
	{\
		temptime = currtime;\
		currtime = Sys_DoubleTime();\
		timestring((int) ((currtime - temptime) * 1000000.0), NAME);\
	}

void R_RenderView (void)
{
	double starttime, currtime, temptime;
//	if (r_norefresh.value)
//		return;

	if (!cl.worldmodel)
		Host_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds2.value)
	{
		starttime = currtime = Sys_DoubleTime();

		speedstringcount = 0;
		sprintf(r_speeds2_string, "org:'%c%6.2f %c%6.2f %c%6.2f' ang:'%c%3.0f %c%3.0f %c%3.0f' dir:'%c%2.3f %c%2.3f %c%2.3f'\n%6i walls %6i dlitwalls %7i modeltris %7i transpoly\nBSP: %6i faces %6i nodes %6i leafs\n%4i models %4i bmodels %4i sprites %5i particles %3i dlights\n",
			r_origin[0] < 0 ? '-' : ' ', fabs(r_origin[0]), r_origin[1] < 0 ? '-' : ' ', fabs(r_origin[1]), r_origin[2] < 0 ? '-' : ' ', fabs(r_origin[2]), r_refdef.viewangles[0] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[0]), r_refdef.viewangles[1] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[1]), r_refdef.viewangles[2] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[2]), vpn[0] < 0 ? '-' : ' ', fabs(vpn[0]), vpn[1] < 0 ? '-' : ' ', fabs(vpn[1]), vpn[2] < 0 ? '-' : ' ', fabs(vpn[2]),
			c_brush_polys, c_light_polys, c_alias_polys, currenttranspoly,
			c_faces, c_nodes, c_leafs,
			c_models, c_bmodels, c_sprites, c_particles, c_dlights);
	}
	else
		starttime = currtime = 0;

	R_MoveParticles ();
	R_MoveExplosions();

	lighthalf = gl_lightmode.value;

	FOG_framebegin();

	R_Clear();
	TIMEREPORT("clear     ")

	// render normal view

	R_SetupFrame ();
	R_SetFrustum ();
	R_SetupGL ();
	R_Clip_StartFrame();

	R_PrepareEntities();

	skypolyclear();
	wallpolyclear();
	transpolyclear();

	TIMEREPORT("setup     ")

	R_DrawWorld ();
	TIMEREPORT("world     ")

	R_AddModelEntities();
	TIMEREPORT("addmodels")

	R_Clip_EndFrame();
	TIMEREPORT("scanedge  ")

	// now mark the lit surfaces
	R_PushDlights ();
	// yes this does add the world surfaces after the brush models
	R_DrawSurfaces ();
	R_DrawPortals ();
	TIMEREPORT("surfaces  ");

	UploadLightmaps();
	TIMEREPORT("uploadlmap")

	// fogged sky polys, affects depth
	skypolyrender();

	// does not affect depth, draws over the sky polys
	if (currentskypoly)
		R_Sky();
	TIMEREPORT("skypoly   ")

	wallpolyrender();
	TIMEREPORT("wallpoly  ")

	GL_DrawDecals();
	TIMEREPORT("ddecal    ")

	// don't let sound skip if going slow
	if (!intimerefresh && !r_speeds2.value)
		S_ExtraUpdate ();

	R_DrawViewModel ();
	R_DrawModels ();
	TIMEREPORT("models    ")

	R_DrawParticles ();
	TIMEREPORT("dparticles")
	R_DrawExplosions();
	TIMEREPORT("dexplosion")

	transpolyrender();
	TIMEREPORT("transpoly ")

	FOG_frameend();

	GL_BlendView();
	TIMEREPORT("blend     ")

	if (r_speeds2.value)
		timestring((int) ((Sys_DoubleTime() - starttime) * 1000000.0), "total    ");
}
