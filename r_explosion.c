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

#include "quakedef.h"
#include "cl_collision.h"

#define MAX_EXPLOSIONS 64
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONGRID*EXPLOSIONGRID*2)
#define EXPLOSIONSTARTVELOCITY (256.0f)
#define EXPLOSIONFADESTART (1.5f)
#define EXPLOSIONFADERATE (3.0f)

float explosiontexcoords[EXPLOSIONVERTS][2];
int explosiontris[EXPLOSIONTRIS][3];
int explosionnoiseindex[EXPLOSIONVERTS];
vec3_t explosionpoint[EXPLOSIONVERTS];
vec3_t explosionspherevertvel[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float starttime;
	float time;
	float alpha;
	vec3_t origin;
	vec3_t vert[EXPLOSIONVERTS];
	vec3_t vertvel[EXPLOSIONVERTS];
}
explosion_t;

explosion_t explosion[MAX_EXPLOSIONS];

rtexture_t	*explosiontexture;
rtexture_t	*explosiontexturefog;

rtexturepool_t	*explosiontexturepool;

cvar_t r_explosionclip = {CVAR_SAVE, "r_explosionclip", "1"};
cvar_t r_drawexplosions = {0, "r_drawexplosions", "1"};

void r_explosion_start(void)
{
	int x, y;
	qbyte noise1[128][128], noise2[128][128], noise3[128][128], data[128][128][4];
	explosiontexturepool = R_AllocTexturePool();
	fractalnoise(&noise1[0][0], 128, 32);
	fractalnoise(&noise2[0][0], 128, 4);
	fractalnoise(&noise3[0][0], 128, 4);
	for (y = 0;y < 128;y++)
	{
		for (x = 0;x < 128;x++)
		{
			int j, r, g, b, a;
			j = (noise1[y][x] * noise2[y][x]) * 3 / 256 - 128;
			r = (j * 512) / 256;
			g = (j * 256) / 256;
			b = (j * 128) / 256;
			a = noise3[y][x] * 3 - 128;
			data[y][x][0] = bound(0, r, 255);
			data[y][x][1] = bound(0, g, 255);
			data[y][x][2] = bound(0, b, 255);
			data[y][x][3] = bound(0, a, 255);
		}
	}
	explosiontexture = R_LoadTexture (explosiontexturepool, "explosiontexture", 128, 128, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
	for (y = 0;y < 128;y++)
		for (x = 0;x < 128;x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
	explosiontexturefog = R_LoadTexture (explosiontexturepool, "explosiontexturefog", 128, 128, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
	// note that explosions survive the restart
}

void r_explosion_shutdown(void)
{
	R_FreeTexturePool(&explosiontexturepool);
}

void r_explosion_newmap(void)
{
	memset(explosion, 0, sizeof(explosion));
}

int R_ExplosionVert(int column, int row)
{
	int i;
	float a, b, c;
	i = row * (EXPLOSIONGRID + 1) + column;
	a = row * M_PI * 2 / EXPLOSIONGRID;
	b = column * M_PI * 2 / EXPLOSIONGRID;
	c = cos(b);
	explosionpoint[i][0] = cos(a) * c;
	explosionpoint[i][1] = sin(a) * c;
	explosionpoint[i][2] = -sin(b);
	explosionspherevertvel[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTVELOCITY;
	explosionspherevertvel[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTVELOCITY;
	explosionspherevertvel[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTVELOCITY;
	explosiontexcoords[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoords[i][1] = (float) row / (float) EXPLOSIONGRID;
	// top and bottom rows are all one position...
	if (row == 0 || row == EXPLOSIONGRID)
		column = 0;
	explosionnoiseindex[i] = (row % EXPLOSIONGRID) * EXPLOSIONGRID + (column % EXPLOSIONGRID);
	return i;
}

void R_Explosion_Init(void)
{
	int i, x, y;
	i = 0;
	for (y = 0;y < EXPLOSIONGRID;y++)
	{
		for (x = 0;x < EXPLOSIONGRID;x++)
		{
			explosiontris[i][0] = R_ExplosionVert(x    , y    );
			explosiontris[i][1] = R_ExplosionVert(x + 1, y    );
			explosiontris[i][2] = R_ExplosionVert(x    , y + 1);
			i++;
			explosiontris[i][0] = R_ExplosionVert(x + 1, y    );
			explosiontris[i][1] = R_ExplosionVert(x + 1, y + 1);
			explosiontris[i][2] = R_ExplosionVert(x    , y + 1);
			i++;
		}
	}

	Cvar_RegisterVariable(&r_explosionclip);
	Cvar_RegisterVariable(&r_drawexplosions);

	R_RegisterModule("R_Explosions", r_explosion_start, r_explosion_shutdown, r_explosion_newmap);
}

void R_NewExplosion(vec3_t org)
{
	int i, j;
	float dist;
	qbyte noise[EXPLOSIONGRID*EXPLOSIONGRID];
	fractalnoisequick(noise, EXPLOSIONGRID, 4); // adjust noise grid size according to explosion
	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha <= 0.01f)
		{
			explosion[i].starttime = cl.time;
			explosion[i].time = explosion[i].starttime - 0.1;
			explosion[i].alpha = EXPLOSIONFADESTART;
			VectorCopy(org, explosion[i].origin);
			for (j = 0;j < EXPLOSIONVERTS;j++)
			{
				// calculate start
				VectorCopy(explosion[i].origin, explosion[i].vert[j]);
				// calculate velocity
				dist = noise[explosionnoiseindex[j]] * (1.0f / 255.0f) + 0.5;
				VectorScale(explosionspherevertvel[j], dist, explosion[i].vertvel[j]);
			}
			break;
		}
	}
}

void R_DrawExplosionCallback(const void *calldata1, int calldata2)
{
	int i, numtriangles, numverts;
	float *c, *v, diff[3], centerdir[3], ifog, alpha, dist;
	rmeshstate_t m;
	const explosion_t *e;
	e = calldata1;

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.wantoverbright = false;
	m.tex[0] = R_GetTexture(explosiontexture);
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	numtriangles = EXPLOSIONTRIS;
	numverts = EXPLOSIONVERTS;
	R_Mesh_ResizeCheck(numverts, numtriangles);

	memcpy(varray_element, explosiontris, numtriangles * sizeof(int[3]));
	for (i = 0, v = varray_vertex;i < numverts;i++, v += 4)
	{
		v[0] = e->vert[i][0];
		v[1] = e->vert[i][1];
		v[2] = e->vert[i][2];
	}
	memcpy(varray_texcoord[0], explosiontexcoords, numverts * sizeof(float[2]));
	alpha = e->alpha;
	VectorSubtract(r_origin, e->origin, centerdir);
	VectorNormalizeFast(centerdir);
	if (fogenabled)
	{
		for (i = 0, c = varray_color;i < EXPLOSIONVERTS;i++, c += 4)
		{
			VectorSubtract(e->vert[i], e->origin, diff);
			VectorNormalizeFast(diff);
			dist = (DotProduct(diff, centerdir) * 6.0f - 4.0f) * alpha;
			if (dist > 0)
			{
				// use inverse fog alpha
				VectorSubtract(e->vert[i], r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff,diff));
				dist = dist * ifog;
				if (dist < 0)
					dist = 0;
				else
					dist *= mesh_colorscale;
			}
			else
				dist = 0;
			c[0] = c[1] = c[2] = dist;
			c[3] = 1;
		}
	}
	else
	{
		for (i = 0, c = varray_color;i < EXPLOSIONVERTS;i++, c += 4)
		{
			VectorSubtract(e->vert[i], e->origin, diff);
			VectorNormalizeFast(diff);
			dist = (DotProduct(diff, centerdir) * 6.0f - 4.0f) * alpha;
			if (dist < 0)
				dist = 0;
			else
				dist *= mesh_colorscale;
			c[0] = c[1] = c[2] = dist;
			c[3] = 1;
		}
	}
	R_Mesh_Draw(numverts, numtriangles);
}

void R_MoveExplosion(explosion_t *e)
{
	int i;
	float dot, frictionscale, end[3], impact[3], normal[3], frametime;

	frametime = cl.time - e->time;
	e->time = cl.time;
	e->alpha = EXPLOSIONFADESTART - (cl.time - e->starttime) * EXPLOSIONFADERATE;
	if (e->alpha <= 0.01f)
	{
		e->alpha = -1;
		return;
	}
	frictionscale = 1 - frametime;
	frictionscale = bound(0, frictionscale, 1);
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			VectorScale(e->vertvel[i], frictionscale, e->vertvel[i]);
			VectorMA(e->vert[i], frametime, e->vertvel[i], end);
			if (r_explosionclip.integer)
			{
				if (CL_TraceLine(e->vert[i], end, impact, normal, 0, true) < 1)
				{
					// clip velocity against the wall
					dot = DotProduct(e->vertvel[i], normal) * -1.125f;
					VectorMA(e->vertvel[i], dot, normal, e->vertvel[i]);
				}
				VectorCopy(impact, e->vert[i]);
			}
			else
				VectorCopy(end, e->vert[i]);
		}
	}
	for (i = 0;i < EXPLOSIONGRID;i++)
		VectorCopy(e->vert[i * (EXPLOSIONGRID + 1)], e->vert[i * (EXPLOSIONGRID + 1) + EXPLOSIONGRID]);
	memcpy(e->vert[EXPLOSIONGRID * (EXPLOSIONGRID + 1)], e->vert[0], sizeof(float[3]) * (EXPLOSIONGRID + 1));
}


void R_MoveExplosions(void)
{
	int i;
	float frametime;

	frametime = cl.time - cl.oldtime;

	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (explosion[i].alpha > 0.01f)
			R_MoveExplosion(&explosion[i]);
}

void R_DrawExplosions(void)
{
	int i;

	if (!r_drawexplosions.integer)
		return;
	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (explosion[i].alpha > 0.01f)
			R_MeshQueue_AddTransparent(explosion[i].origin, R_DrawExplosionCallback, &explosion[i], 0);
}

