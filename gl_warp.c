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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



extern qboolean lighthalf;

int skyboxside[6];

char skyname[256];

// LordHavoc: moved LoadTGA and LoadPCX to gl_draw.c

extern int image_width, image_height;

byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight);
/*
==================
R_LoadSkyBox
==================
*/
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void R_LoadSkyBox (void)
{
	int		i;
	char	name[64];
	byte*	image_rgba;

	for (i=0 ; i<6 ; i++)
	{
		sprintf (name, "env/%s%s", skyname, suf[i]);
		if (!(image_rgba = loadimagepixels(name, FALSE, 0, 0)))
		{
			sprintf (name, "gfx/env/%s%s", skyname, suf[i]);
			if (!(image_rgba = loadimagepixels(name, FALSE, 0, 0)))
			{
				Con_Printf ("Couldn't load %s\n", name);
				continue;
			}
		}
		skyboxside[i] = GL_LoadTexture(va("skyboxside%d", i), image_width, image_height, image_rgba, false, false, 4);
		free (image_rgba);
	}
}

void R_SetSkyBox (char *sky)
{
	strcpy(skyname, sky);
	R_LoadSkyBox ();
}

// LordHavoc: added LoadSky console command
void LoadSky_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		if (skyname[0])
			Con_Printf("current sky: %s\n", skyname);
		else
			Con_Printf("no skybox has been set\n", skyname);
		break;
	case 2:
		R_SetSkyBox(Cmd_Argv(1));
		Con_Printf("skybox set to %s\n", skyname);
		break;
	default:
		Con_Printf("usage: loadsky skyname\n");
		break;
	}
}

extern cvar_t r_farclip;

#define R_SkyBoxPolyVec(s,t,x,y,z) \
	glTexCoord2f((s) * (254.0f/256.0f) + (1.0f/256.0f), (t) * (254.0f/256.0f) + (1.0f/256.0f));\
	glVertex3f((x) * 1024.0 + r_refdef.vieworg[0], (y) * 1024.0 + r_refdef.vieworg[1], (z) * 1024.0 + r_refdef.vieworg[2]);

void R_SkyBox()
{
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	if (lighthalf)
		glColor3f(0.5,0.5,0.5);
	else
		glColor3f(1,1,1);
	glBindTexture(GL_TEXTURE_2D, skyboxside[3]); // front
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 1,  1,  1, -1);
	R_SkyBoxPolyVec(0, 0,  1,  1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, skyboxside[1]); // back
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0, -1,  1,  1);
	R_SkyBoxPolyVec(1, 1, -1,  1, -1);
	R_SkyBoxPolyVec(0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 0, -1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, skyboxside[0]); // right
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1,  1,  1);
	R_SkyBoxPolyVec(1, 1,  1,  1, -1);
	R_SkyBoxPolyVec(0, 1, -1,  1, -1);
	R_SkyBoxPolyVec(0, 0, -1,  1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, skyboxside[2]); // left
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0, -1, -1,  1);
	R_SkyBoxPolyVec(1, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 0,  1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, skyboxside[4]); // up
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1,  1,  1,  1);
	R_SkyBoxPolyVec(0, 1, -1,  1,  1);
	R_SkyBoxPolyVec(0, 0, -1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, skyboxside[5]); // down
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1,  1, -1);
	R_SkyBoxPolyVec(1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 0, -1,  1, -1);
	glEnd();
}

/*
float skydomeouter[33*33*3];
float skydomeinner[33*33*3];
unsigned short skydomeindices[32*66];
qboolean skydomeinitialized = 0;
void skydomecalc(float *dome, float dx, float dy, float dz)
{
	float a, b, x, ax, ay;
	int i;
	unsigned short *index;
	for (a = 0;a <= 1;a += (1.0 / 32.0))
	{
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (b = 0;b <= 1;b += (1.0 / 32.0))
		{
			x = cos(b * M_PI * 2);
			*dome++ = ax*x * dx;
			*dome++ = ay*x * dy;
			*dome++ = -sin(b * M_PI * 2) * dz;
		}
	}
	index = skydomeindices;
	for (i = 0;i < (32*33);i++)
	{
		*index++ = i;
		*index++ = i + 33;
	}
}

extern cvar_t gl_vertexarrays;
void skydome(float *source, float s, float texscale)
{
	vec_t vert[33*33][3], tex[33*33][2], *v, *t;
	int i, j;
	unsigned short *index;
	v = &vert[0][0];t = &tex[0][0];
	for (i = 0;i < (33*33);i++)
	{
		*t++ = source[0] * texscale + s;
		*t++ = source[1] * texscale + s;
		*v++ = *source++ + r_refdef.vieworg[0];
		*v++ = *source++ + r_refdef.vieworg[1];
		*v++ = *source++ + r_refdef.vieworg[2];
	}
	if (gl_vertexarrays.value)
	{
		qglTexCoordPointer(2, GL_FLOAT, 0, tex);
		qglVertexPointer(3, GL_FLOAT, 0, vert);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
//		qglInterleavedArrays(GL_T2F_V3F, 0, vert);
		for (i = 0;i < (32*66);i+=66)
			qglDrawElements(GL_TRIANGLE_STRIP, 66, GL_UNSIGNED_SHORT, &skydomeindices[i]);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		index = skydomeindices;
		for (i = 0;i < (32*66);i+=66)
		{
			glBegin(GL_TRIANGLE_STRIP);
			for (j = 0;j < 66;j++)
			{
				// Matrox G200 (and possibly G400) drivers don't support TexCoord2fv...
				glTexCoord2f(tex[*index][0], tex[*index][1]);
				glVertex3fv(&vert[*index++][0]);
			}
			glEnd();
		}
	}
}

void R_SkyDome()
{
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (lighthalf)
		glColor3f(0.5,0.5,0.5);
	else
		glColor3f(1,1,1);
	glBindTexture(GL_TEXTURE_2D, solidskytexture); // upper clouds
	if (!skydomeinitialized)
	{
		skydomeinitialized = true;
		skydomecalc(skydomeouter, 1024, 1024, 256);
		skydomecalc(skydomeinner, 512, 512, 128);
	}
	speedscale = realtime*8.0/256.0;
	speedscale -= (int)speedscale;
	skydome(skydomeouter, speedscale, 1.0 / 256.0);
	glEnable (GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, alphaskytexture); // lower clouds
	speedscale = realtime*8.0/128.0;
	speedscale -= (int)speedscale;
	skydome(skydomeinner, speedscale, 1.0 / 128.0);
	glDisable (GL_BLEND);
}
*/

void R_Sky()
{
	if (!skyname[0])
		return;
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
//	if (skyname[0])
		R_SkyBox();
//	else // classic quake sky
//		R_SkyDome();
	glDepthMask(1);
	glEnable (GL_DEPTH_TEST);
	glColor3f (1,1,1);
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (byte *src, int bytesperpixel)
{
	int			i, j, p;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j+128];
	}
	else
	{
		// make an average value for the back to avoid
		// a fringe on the top level
		r = g = b = 0;
		for (i=0 ; i<128 ; i++)
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j + 128];
				rgba = &d_8to24table[p];
				trans[(i*128) + j] = *rgba;
				r += ((byte *)rgba)[0];
				g += ((byte *)rgba)[1];
				b += ((byte *)rgba)[2];
			}

		((byte *)&transpix)[0] = r/(128*128);
		((byte *)&transpix)[1] = g/(128*128);
		((byte *)&transpix)[2] = b/(128*128);
		((byte *)&transpix)[3] = 0;
	}

	solidskytexture = GL_LoadTexture ("sky_solidtexture", 128, 128, (byte *) trans, false, false, 4);

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j];
	}
	else
	{
		for (i=0 ; i<128 ; i++)
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j];
				if (p == 0)
					trans[(i*128) + j] = transpix;
				else
					trans[(i*128) + j] = d_8to24table[p];
			}
	}

	alphaskytexture = GL_LoadTexture ("sky_alphatexture", 128, 128, (byte *) trans, false, true, 4);
}

