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

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

// LordHavoc: moved all code related to particles into r_part.c
//int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins

extern qboolean isG200, isRagePro; // LordHavoc: special card hacks

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

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


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

//cvar_t	gl_cull = {"gl_cull","1"};
//cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
//cvar_t	gl_polyblend = {"gl_polyblend","1"};
//cvar_t	gl_flashblend = {"gl_flashblend","0"};
cvar_t	gl_playermip = {"gl_playermip","0"};
//cvar_t	gl_nocolors = {"gl_nocolors","0"};
//cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","1"};
//cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	contrast = {"contrast", "1.0", TRUE}; // LordHavoc: a method of operating system independent color correction
cvar_t	brightness = {"brightness", "1.0", TRUE}; // LordHavoc: a method of operating system independent color correction
cvar_t	gl_lightmode = {"gl_lightmode", "1", TRUE}; // LordHavoc: overbright lighting
//cvar_t	r_dynamicwater = {"r_dynamicwater", "1"};
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

int chrometexture;

void makechrometextures()
{
	int x, y, g, g2, amplitude, noise[64][64], min, max;
	byte data[64][64][4];
	//
	// particle texture
	//
	chrometexture = texture_extension_number++;
    glBindTexture(GL_TEXTURE_2D, chrometexture);

#define n(x,y) noise[(y)&63][(x)&63]

	amplitude = 16777215;
	g2 = 64;
	noise[0][0] = 0;
	for (;(g = g2 >> 1) >= 1;g2 >>= 1)
	{
		// subdivide, diamond-square algorythm (really this has little to do with squares)
		// diamond
		for (y = 0;y < 64;y += g2)
			for (x = 0;x < 64;x += g2)
				n(x+g,y+g) = (n(x,y) + n(x+g2,y) + n(x,y+g2) + n(x+g2,y+g2)) >> 2;
		// square
		for (y = 0;y < 64;y += g2)
			for (x = 0;x < 64;x += g2)
			{
				n(x+g,y) = (n(x,y) + n(x+g2,y) + n(x+g,y-g) + n(x+g,y+g)) >> 2;
				n(x,y+g) = (n(x,y) + n(x,y+g2) + n(x-g,y+g) + n(x+g,y+g)) >> 2;
			}
		// brownian motion theory
		amplitude >>= 1;
		for (y = 0;y < 64;y += g)
			for (x = 0;x < 64;x += g)
				noise[y][x] += rand()&amplitude;
	}
	// normalize the noise range
	min = max = 0;
	for (y = 0;y < 64;y++)
		for (x = 0;x < 64;x++)
		{
			if (n(x,y) < min) min = n(x,y);
			if (n(x,y) > max) max = n(x,y);
		}
	max -= min;
	for (y = 0;y < 64;y++)
		for (x = 0;x < 64;x++)
			n(x,y) = (n(x,y) - min) * 255 / max;

#undef n

	// convert to RGBA data
	for (y = 0;y < 64;y++)
		for (x = 0;x < 64;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = (byte) noise[y][x];
			data[y][x][3] = 255;
		}

	glTexImage2D (GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

extern qboolean isRagePro;

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
	if (glfog.value)
	{
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

void glpoly_init();
void glrsurf_init();
void rlight_init();

// LordHavoc: vertex array
float *aliasvert;
float *aliasvertnorm;
byte *aliasvertcolor;

void rmain_registercvars()
{
	// allocate vertex processing arrays
	aliasvert = malloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertnorm = malloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertcolor = malloc(sizeof(byte[MD2MAX_VERTS][4]));

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
	makechrometextures();
	glpoly_init();
	glrsurf_init();
	rlight_init();
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

// LordHavoc: moved this shading stuff up because the sprites need shading stuff
vec3_t	shadevector;
vec3_t	shadecolor;

float	modelalpha;

void R_LightPoint (vec3_t color, vec3_t p);
void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
void R_DynamicLightPointNoMask(vec3_t color, vec3_t org);

float R_CalcAnimLerp(int pose, float lerpscale)
{
	if (currententity->draw_lastmodel == currententity->model && currententity->draw_lerpstart <= cl.time)
	{
		if (pose != currententity->draw_pose)
		{
			currententity->draw_lastpose = currententity->draw_pose;
			currententity->draw_pose = pose;
			currententity->draw_lerpstart = cl.time;
			return 0;
		}
		else
			return ((cl.time - currententity->draw_lerpstart) * lerpscale);
	}
	else // uninitialized
	{
		currententity->draw_lastmodel = currententity->model;
		currententity->draw_lastpose = currententity->draw_pose = pose;
		currententity->draw_lerpstart = cl.time;
		return 0;
	}
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
void R_GetSpriteFrame (entity_t *currententity, mspriteframe_t **oldframe, mspriteframe_t **newframe, float *framelerp)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	int				i, j, numframes, frame;
	float			*pintervals, fullinterval, targettime, time, jtime, jinterval;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		if (currententity->draw_lastmodel == currententity->model && currententity->draw_lerpstart < cl.time)
		{
			if (frame != currententity->draw_pose)
			{
				currententity->draw_lastpose = currententity->draw_pose;
				currententity->draw_pose = frame;
				currententity->draw_lerpstart = cl.time;
				*framelerp = 0;
			}
			else
				*framelerp = (cl.time - currententity->draw_lerpstart) * 10.0;
		}
		else // uninitialized
		{
			currententity->draw_lastmodel = currententity->model;
			currententity->draw_lastpose = currententity->draw_pose = frame;
			currententity->draw_lerpstart = cl.time;
			*framelerp = 0;
		}
		*oldframe = psprite->frames[currententity->draw_lastpose].frameptr;
		*newframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		// LordHavoc: since I can't measure the time properly when it loops from numframes-1 to 0,
		//            I instead measure the time of the first frame, hoping it is consistent
		j = numframes-1;jtime = 0;jinterval = pintervals[1] - pintervals[0];
		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
			j = i;jinterval = pintervals[i] - jtime;jtime = pintervals[i];
		}
		*framelerp = (targettime - jtime) / jinterval;

		*oldframe = pspritegroup->frames[j];
		*newframe = pspritegroup->frames[i];
	}
}

void GL_DrawSpriteImage (mspriteframe_t *frame, vec3_t origin, vec3_t up, vec3_t right, int red, int green, int blue, int alpha)
{
	// LordHavoc: rewrote this to use the transparent poly system
	transpolybegin(frame->gl_texturenum, 0, frame->gl_fogtexturenum, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
	transpolyvert(origin[0] + frame->down * up[0] + frame->left * right[0], origin[1] + frame->down * up[1] + frame->left * right[1], origin[2] + frame->down * up[2] + frame->left * right[2], 0, 1, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->up * up[0] + frame->left * right[0], origin[1] + frame->up * up[1] + frame->left * right[1], origin[2] + frame->up * up[2] + frame->left * right[2], 0, 0, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->up * up[0] + frame->right * right[0], origin[1] + frame->up * up[1] + frame->right * right[1], origin[2] + frame->up * up[2] + frame->right * right[2], 1, 0, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->down * up[0] + frame->right * right[0], origin[1] + frame->down * up[1] + frame->right * right[1], origin[2] + frame->down * up[2] + frame->right * right[2], 1, 1, red, green, blue, alpha);
	transpolyend();
}

extern qboolean isG200, isRagePro, lighthalf;

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	mspriteframe_t	*oldframe, *newframe;
	float		*up, *right, lerp, ilerp;
	vec3_t		v_forward, v_right, v_up, org;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	R_GetSpriteFrame (e, &oldframe, &newframe, &lerp);
	if (lerp < 0) lerp = 0;
	if (lerp > 1) lerp = 1;
	if (isRagePro) // LordHavoc: no alpha scaling supported on per pixel alpha images on ATI Rage Pro... ACK!
		lerp = 1;
	ilerp = 1.0 - lerp;
	psprite = e->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (e->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
		VectorSubtract(e->origin, vpn, org);
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
		VectorCopy(e->origin, org);
	}
	if (e->scale != 1)
	{
		VectorScale(up, e->scale, up);
		VectorScale(right, e->scale, right);
	}

	if (e->model->flags & EF_FULLBRIGHT || e->effects & EF_FULLBRIGHT)
	{
		if (lighthalf)
		{
			shadecolor[0] = e->colormod[0] * 128;
			shadecolor[1] = e->colormod[1] * 128;
			shadecolor[2] = e->colormod[2] * 128;
		}
		else
		{
			shadecolor[0] = e->colormod[0] * 255;
			shadecolor[1] = e->colormod[1] * 255;
			shadecolor[2] = e->colormod[2] * 255;
		}
	}
	else
	{
		R_LightPoint (shadecolor, e->origin);
		R_DynamicLightPointNoMask(shadecolor, e->origin);
		if (lighthalf)
		{
			shadecolor[0] *= e->colormod[0] * 0.5;
			shadecolor[1] *= e->colormod[1] * 0.5;
			shadecolor[2] *= e->colormod[2] * 0.5;
		}
	}

	// LordHavoc: interpolated sprite rendering
	if (ilerp != 0)
		GL_DrawSpriteImage(oldframe, org, up, right, shadecolor[0],shadecolor[1],shadecolor[2],e->alpha*255*ilerp);
	if (lerp != 0)
		GL_DrawSpriteImage(newframe, org, up, right, shadecolor[0],shadecolor[1],shadecolor[2],e->alpha*255*lerp);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

extern vec3_t softwaretransform_x;
extern vec3_t softwaretransform_y;
extern vec3_t softwaretransform_z;
extern vec_t softwaretransform_scale;
extern vec3_t softwaretransform_offset;
void R_AliasLerpVerts(int vertcount, float lerp, trivert2 *verts1, vec3_t scale1, vec3_t translate1, trivert2 *verts2, vec3_t scale2, vec3_t translate2)
{
	int i;
	vec3_t point, matrix_x, matrix_y, matrix_z;
	float *av, *avn;
	av = aliasvert;
	avn = aliasvertnorm;
	if (lerp < 0) lerp = 0;
	if (lerp > 1) lerp = 1;
	if (lerp != 0)
	{
		float ilerp, ilerp127, lerp127, scalex1, scalex2, translatex, scaley1, scaley2, translatey, scalez1, scalez2, translatez;
		if (lerp < 0) lerp = 0;
		if (lerp > 1) lerp = 1;
		ilerp = 1 - lerp;
		ilerp127 = ilerp * (1.0 / 127.0);
		lerp127 = lerp * (1.0 / 127.0);
		VectorScale(softwaretransform_x, softwaretransform_scale, matrix_x);
		VectorScale(softwaretransform_y, softwaretransform_scale, matrix_y);
		VectorScale(softwaretransform_z, softwaretransform_scale, matrix_z);
		// calculate combined interpolation variables
		scalex1 = scale1[0] * ilerp;scalex2 = scale2[0] *  lerp;translatex = translate1[0] * ilerp + translate2[0] *  lerp;
		scaley1 = scale1[1] * ilerp;scaley2 = scale2[1] *  lerp;translatey = translate1[1] * ilerp + translate2[1] *  lerp;
		scalez1 = scale1[2] * ilerp;scalez2 = scale2[2] *  lerp;translatez = translate1[2] * ilerp + translate2[2] *  lerp;
		// generate vertices
		for (i = 0;i < vertcount;i++)
		{
			// rotate, scale, and translate the vertex locations
			point[0] = verts1->v[0] * scalex1 + verts2->v[0] * scalex2 + translatex;
			point[1] = verts1->v[1] * scaley1 + verts2->v[1] * scaley2 + translatey;
			point[2] = verts1->v[2] * scalez1 + verts2->v[2] * scalez2 + translatez;
			*av++ = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
			*av++ = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
			*av++ = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
			// rotate the normals
			point[0] = verts1->n[0] * ilerp127 + verts2->n[0] * lerp127;
			point[1] = verts1->n[1] * ilerp127 + verts2->n[1] * lerp127;
			point[2] = verts1->n[2] * ilerp127 + verts2->n[2] * lerp127;
			*avn++ = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
			*avn++ = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
			*avn++ = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
			verts1++;verts2++;
		}
	}
	else
	{
		float i127;
		i127 = 1.0f / 127.0f;
		VectorScale(softwaretransform_x, softwaretransform_scale, matrix_x);
		VectorScale(softwaretransform_y, softwaretransform_scale, matrix_y);
		VectorScale(softwaretransform_z, softwaretransform_scale, matrix_z);
		// generate vertices
		for (i = 0;i < vertcount;i++)
		{
			// rotate, scale, and translate the vertex locations
			point[0] = verts1->v[0] * scale1[0] + translate1[0];
			point[1] = verts1->v[1] * scale1[1] + translate1[1];
			point[2] = verts1->v[2] * scale1[2] + translate1[2];
			*av++ = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
			*av++ = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
			*av++ = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
			// rotate the normals
			point[0] = verts1->n[0] * i127;
			point[1] = verts1->n[1] * i127;
			point[2] = verts1->n[2] * i127;
			*avn++ = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
			*avn++ = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
			*avn++ = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
			verts1++;
		}
	}
}

/*
=================
R_DrawAliasFrame

=================
*/
extern vec3_t lightspot;
void R_LightModel(int numverts, vec3_t center);
extern cvar_t gl_vertexarrays;
void R_DrawAliasFrame (aliashdr_t *paliashdr)
{
	int				i, pose, frame = currententity->frame;
	float			lerpscale, lerp;

	softwaretransformforentity(currententity);

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;

	if (paliashdr->frames[frame].numposes > 1)
	{
		lerpscale = 1.0 / paliashdr->frames[frame].interval;
		pose += (int)(cl.time * lerpscale) % paliashdr->frames[frame].numposes;
	}
	else
		lerpscale = 10.0;

	lerp = R_CalcAnimLerp(pose, lerpscale);

	R_AliasLerpVerts(paliashdr->numverts, lerp, (trivert2 *)((byte *)paliashdr + paliashdr->posedata) + currententity->draw_lastpose * paliashdr->numverts, paliashdr->scale, paliashdr->scale_origin, (trivert2 *)((byte *)paliashdr + paliashdr->posedata) + currententity->draw_pose * paliashdr->numverts, paliashdr->scale, paliashdr->scale_origin);

	R_LightModel(paliashdr->numverts, currententity->origin);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel(GL_SMOOTH);
	if (currententity->effects & EF_ADDITIVE)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive rendering
		glEnable(GL_BLEND);
		glDepthMask(0);
	}
	else if (modelalpha != 1.0)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glDepthMask(0);
	}
	else
	{
		glDisable(GL_BLEND);
		glDepthMask(1);
	}

	if (gl_vertexarrays.value)
	{
		// LordHavoc: I would use InterleavedArrays here,
		// but the texture coordinates are a seperate array,
		// and it would be wasteful to copy them into the main array...
	//	glColor4f(shadecolor[0], shadecolor[1], shadecolor[2], modelalpha);
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, aliasvertcolor);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		// draw the front faces
		qglTexCoordPointer(2, GL_FLOAT, 0, (void *)((int) paliashdr->texcoords + (int) paliashdr));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDrawElements(GL_TRIANGLES, paliashdr->frontfaces * 3, GL_UNSIGNED_SHORT, (void *)((int) paliashdr->vertindices + (int) paliashdr));
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		// draw the back faces
		qglTexCoordPointer(2, GL_FLOAT, 0, (void *)((int) paliashdr->texcoords + sizeof(float[2]) * paliashdr->numverts + (int) paliashdr));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDrawElements(GL_TRIANGLES, paliashdr->backfaces * 3, GL_UNSIGNED_SHORT, (void *)((int) paliashdr->vertindices + sizeof(unsigned short[3]) * paliashdr->frontfaces + (int) paliashdr));
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		unsigned short *in, index;
		float *tex;
		in = (void *)((int) paliashdr->vertindices + (int) paliashdr);
		glBegin(GL_TRIANGLES);
		// draw the front faces
		tex = (void *)((int) paliashdr->texcoords + (int) paliashdr);
		//if (isG200)
		//{
			for (i = 0;i < paliashdr->frontfaces * 3;i++)
			{
				index = *in++;
				glTexCoord2f(tex[index*2], tex[index*2+1]);
				glColor4f(aliasvertcolor[index*4] * (1.0f / 255.0f), aliasvertcolor[index*4+1] * (1.0f / 255.0f), aliasvertcolor[index*4+2] * (1.0f / 255.0f), aliasvertcolor[index*4+3] * (1.0f / 255.0f));
				glVertex3fv(&aliasvert[index*3]);
			}
		/*
		}
		else
		{
			for (i = 0;i < paliashdr->frontfaces * 3;i++)
			{
				index = *in++;
				glTexCoord2f(tex[index*2], tex[index*2+1]);
				glColor4ub(aliasvertcolor[index*4], aliasvertcolor[index*4+1], aliasvertcolor[index*4+2], aliasvertcolor[index*4+3]);
				glVertex3fv(&aliasvert[index*3]);
			}
		}
		*/
		// draw the back faces
		tex += 2 * paliashdr->numverts;
		//if (isG200)
		//{
			for (i = 0;i < paliashdr->backfaces * 3;i++)
			{
				index = *in++;
				glTexCoord2f(tex[index*2], tex[index*2+1]);
				glColor4f(aliasvertcolor[index*4] * (1.0f / 255.0f), aliasvertcolor[index*4+1] * (1.0f / 255.0f), aliasvertcolor[index*4+2] * (1.0f / 255.0f), aliasvertcolor[index*4+3] * (1.0f / 255.0f));
				glVertex3fv(&aliasvert[index*3]);
			}
		/*
		}
		else
		{
			for (i = 0;i < paliashdr->backfaces * 3;i++)
			{
				index = *in++;
				glTexCoord2f(tex[index*2], tex[index*2+1]);
				glColor4ub(aliasvertcolor[index*4], aliasvertcolor[index*4+1], aliasvertcolor[index*4+2], aliasvertcolor[index*4+3]);
				glVertex3fv(&aliasvert[index*3]);
			}
		}
		*/
		glEnd();
	}

	if (fogenabled)
	{
		vec3_t diff;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates

		VectorSubtract(currententity->origin, r_refdef.vieworg, diff);
		glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));

		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
			qglDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) paliashdr->vertindices + (int) paliashdr));
			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			unsigned short *in;
			in = (void *)((int) paliashdr->vertindices + (int) paliashdr);
			glBegin(GL_TRIANGLES);
			for (i = 0;i < paliashdr->numtris * 3;i++)
				glVertex3fv(&aliasvert[*in++ * 3]);
			glEnd();
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	if (!fogenabled && r_shadows.value && !(currententity->effects & EF_ADDITIVE) && currententity != &cl.viewent)
	{
		// flatten it to make a shadow
		float *av = aliasvert + 2, l = lightspot[2] + 0.125;
		av = aliasvert + 2;
		for (i = 0;i < paliashdr->numverts;i++, av+=3)
			if (*av > l)
				*av = l;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		glColor4f (0,0,0,0.5 * modelalpha);

		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
			qglDrawElements(GL_TRIANGLES, paliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) paliashdr->vertindices + (int) paliashdr));
			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			unsigned short *in;
			in = (void *)((int) paliashdr->vertindices + (int) paliashdr);
			glBegin(GL_TRIANGLES);
			for (i = 0;i < paliashdr->numtris * 3;i++)
				glVertex3fv(&aliasvert[*in++ * 3]);
			glEnd();
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

/*
=================
R_DrawQ2AliasFrame

=================
*/
void R_DrawQ2AliasFrame (md2mem_t *pheader)
{
	int *order, count, frame = currententity->frame;
	float lerp;
	md2memframe_t *frame1, *frame2;

	softwaretransformforentity(currententity);

	if ((frame >= pheader->num_frames) || (frame < 0))
	{
		Con_DPrintf ("R_SetupQ2AliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	lerp = R_CalcAnimLerp(frame, 10);

	frame1 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * currententity->draw_lastpose));
	frame2 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * currententity->draw_pose));
	R_AliasLerpVerts(pheader->num_xyz, lerp, frame1->verts, frame1->scale, frame1->translate, frame2->verts, frame2->scale, frame2->translate);

	R_LightModel(pheader->num_xyz, currententity->origin);

	if (gl_vertexarrays.value)
	{
		// LordHavoc: big mess...
		// using arrays only slightly, although it is enough to prevent duplicates
		// (saving half the transforms)
		//glColor4f(shadecolor[0], shadecolor[1], shadecolor[2], modelalpha);
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, aliasvertcolor);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		order = (int *)((int)pheader + pheader->ofs_glcmds);
		while(1)
		{
			if (!(count = *order++))
				break;
			if (count > 0)
				glBegin(GL_TRIANGLE_STRIP);
			else
			{
				glBegin(GL_TRIANGLE_FAN);
				count = -count;
			}
			do
			{
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
				qglArrayElement(order[2]);
				order += 3;
			}
			while (count--);
		}

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		order = (int *)((int)pheader + pheader->ofs_glcmds);
		while(1)
		{
			if (!(count = *order++))
				break;
			if (count > 0)
				glBegin(GL_TRIANGLE_STRIP);
			else
			{
				glBegin(GL_TRIANGLE_FAN);
				count = -count;
			}
			//if (isG200)
			//{
				do
				{
					glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
					glColor4f(aliasvertcolor[order[2] * 4] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 1] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 2] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 3] * (1.0f / 255.0f));
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			/*
			}
			else
			{
				do
				{
					glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
					glColor4ub(aliasvertcolor[order[2] * 4], aliasvertcolor[order[2] * 4 + 1], aliasvertcolor[order[2] * 4 + 2], aliasvertcolor[order[2] * 4 + 3]);
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			}
			*/
		}
	}

	if (fogenabled)
	{
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		{
			vec3_t diff;
			VectorSubtract(currententity->origin, r_refdef.vieworg, diff);
			glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));
		}

		if (gl_vertexarrays.value)
		{
			// LordHavoc: big mess...
			// using arrays only slightly, although it is enough to prevent duplicates
			// (saving half the transforms)
			//glColor4f(shadecolor[0], shadecolor[1], shadecolor[2], modelalpha);
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);

			order = (int *)((int)pheader + pheader->ofs_glcmds);
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					qglArrayElement(order[2]);
					order += 3;
				}
				while (count--);
			}

			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			order = (int *)((int)pheader + pheader->ofs_glcmds);
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			}
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	if (!fogenabled && r_shadows.value && !(currententity->effects & EF_ADDITIVE) && currententity != &cl.viewent)
	{
		int i;
		float *av = aliasvert + 2, l = lightspot[2] + 0.125;
		av = aliasvert + 2;
		for (i = 0;i < pheader->num_xyz;i++, av+=3)
			if (*av > l)
				*av = l;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		glColor4f (0,0,0,0.5 * modelalpha);

		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
						
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					qglArrayElement(order[2]);
					order += 3;
				}
				while (count--);
			}

			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			}
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e, int cull)
{
	int			i;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr = NULL;
	md2mem_t		*pheader = NULL;
	int			anim;

	if (modelalpha < (1.0 / 64.0))
		return; // basically completely transparent

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (cull && R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information

	if (currententity->model->flags & EF_FULLBRIGHT || currententity->effects & EF_FULLBRIGHT)
	{
		shadecolor[0] = currententity->colormod[0] * 256;
		shadecolor[1] = currententity->colormod[1] * 256;
		shadecolor[2] = currententity->colormod[2] * 256;
	}
	else
	{
		R_LightPoint (shadecolor, currententity->origin);

		// HACK HACK HACK -- no fullbright colors, so make torches full light
		if (!strcmp (currententity->model->name, "progs/flame2.mdl") || !strcmp (currententity->model->name, "progs/flame.mdl") )
			shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;

		shadecolor[0] *= currententity->colormod[0];
		shadecolor[1] *= currententity->colormod[1];
		shadecolor[2] *= currententity->colormod[2];
	}

	// locate the proper data
	if (clmodel->aliastype == ALIASTYPE_MD2)
	{
		pheader = (void *)Mod_Extradata (currententity->model);
		c_alias_polys += pheader->num_tris;
	}
	else
	{
		paliashdr = (void *)Mod_Extradata (currententity->model);
		c_alias_polys += paliashdr->numtris;
	}

	// draw all the triangles

	if (clmodel->aliastype == ALIASTYPE_MD2)
	{
		if (currententity->skinnum < 0 || currententity->skinnum >= pheader->num_skins)
		{
			currententity->skinnum = 0;
			Con_DPrintf("invalid skin number %d for model %s\n", currententity->skinnum, clmodel->name);
		}
		glBindTexture(GL_TEXTURE_2D, pheader->gl_texturenum[currententity->skinnum]);
	}
	else
	{
		if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins)
		{
			currententity->skinnum = 0;
			Con_DPrintf("invalid skin number %d for model %s\n", currententity->skinnum, clmodel->name);
		}
		anim = (int)(cl.time*10) & 3;
	    glBindTexture(GL_TEXTURE_2D, paliashdr->gl_texturenum[currententity->skinnum][anim]);
	}
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_TEXTURE_2D);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != 0 /*vid.colormap*/ /* && !gl_nocolors.value*/)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
			glBindTexture(GL_TEXTURE_2D, playertextures - 1 + i);
	}

//	if (gl_affinemodels.value)
//		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	if (clmodel->aliastype == ALIASTYPE_MD2)
		R_DrawQ2AliasFrame (pheader);
	else
		R_DrawAliasFrame (paliashdr);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

//==================================================================================

void R_DrawBrushModel (entity_t *e);

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
			R_DrawAliasModel (currententity, true);
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
	R_DrawAliasModel (currententity, FALSE);
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
		Cvar_Set ("r_fullbright", "0");

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
//	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: moved to SCR_UpdateScreen
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

// LordHavoc: my trick to *FIX* GLQuake lighting once and for all :)
void GL_Brighten()
{
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
void R_RenderView (void)
{
//	double currtime, temptime;
//	if (r_norefresh.value)
//		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	lighthalf = gl_lightmode.value;

	FOG_framebegin();
	transpolyclear();

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
	R_MarkLeaves ();	// done here so we know if we're in water
	R_DrawWorld ();		// adds static entities to the list
	S_ExtraUpdate ();	// don't let sound get messed up if going slow
	wallpolyclear();
	R_DrawEntitiesOnList1 (); // BSP models
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
