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

float explosiontexcoord2f[EXPLOSIONVERTS][2];
int explosiontris[EXPLOSIONTRIS][3];
int explosionnoiseindex[EXPLOSIONVERTS];
vec3_t explosionpoint[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float starttime;
	float endtime;
	float time;
	float alpha;
	float fade;
	vec3_t origin;
	vec3_t vert[EXPLOSIONVERTS];
	vec3_t vertvel[EXPLOSIONVERTS];
	qboolean clipping;
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
	explosiontexture = R_LoadTexture2D(explosiontexturepool, "explosiontexture", 128, 128, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
	for (y = 0;y < 128;y++)
		for (x = 0;x < 128;x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
	explosiontexturefog = R_LoadTexture2D(explosiontexturepool, "explosiontexturefog", 128, 128, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
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
	float yaw, pitch;
	// top and bottom rows are all one position...
	if (row == 0 || row == EXPLOSIONGRID)
		column = 0;
	i = row * (EXPLOSIONGRID + 1) + column;
	yaw = ((double) column / EXPLOSIONGRID) * M_PI * 2;
	pitch = (((double) row / EXPLOSIONGRID) - 0.5) * M_PI;
	explosionpoint[i][0] = cos(yaw) *  cos(pitch);
	explosionpoint[i][1] = sin(yaw) *  cos(pitch);
	explosionpoint[i][2] =        1 * -sin(pitch);
	explosiontexcoord2f[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoord2f[i][1] = (float) row / (float) EXPLOSIONGRID;
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
	float dist, n;
	vec3_t impact;
	explosion_t *e;
	qbyte noise[EXPLOSIONGRID*EXPLOSIONGRID];
	fractalnoisequick(noise, EXPLOSIONGRID, 4); // adjust noise grid size according to explosion
	for (i = 0, e = explosion;i < MAX_EXPLOSIONS;i++, e++)
	{
		if (e->alpha <= cl_explosions_alpha_end.value)
		{
			e->starttime = cl.time;
			e->endtime = cl.time + cl_explosions_lifetime.value;
			e->time = e->starttime;
			e->alpha = cl_explosions_alpha_start.value;
			e->fade = (cl_explosions_alpha_start.value - cl_explosions_alpha_end.value) / cl_explosions_lifetime.value;
			e->clipping = r_explosionclip.integer != 0;
			VectorCopy(org, e->origin);
			for (j = 0;j < EXPLOSIONVERTS;j++)
			{
				// calculate start origin and velocity
				n = noise[explosionnoiseindex[j]] * (1.0f / 255.0f) + 0.5;
				dist = n * cl_explosions_size_start.value;
				VectorMA(e->origin, dist, explosionpoint[j], e->vert[j]);
				dist = n * (cl_explosions_size_end.value - cl_explosions_size_start.value) / cl_explosions_lifetime.value;
				VectorScale(explosionpoint[j], dist, e->vertvel[j]);
				// clip start origin
				if (e->clipping)
				{
					CL_TraceLine(e->origin, e->vert[j], impact, NULL, true, NULL, SUPERCONTENTS_SOLID);
					VectorCopy(impact, e->vert[i]);
				}
			}
			break;
		}
	}
}

void R_DrawExplosionCallback(const void *calldata1, int calldata2)
{
	int numtriangles, numverts;
	float alpha;
	rmeshstate_t m;
	const explosion_t *e;
	e = calldata1;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&r_identitymatrix);

	numtriangles = EXPLOSIONTRIS;
	numverts = EXPLOSIONVERTS;
	alpha = e->alpha;

	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(explosiontexture);
	m.pointer_texcoord[0] = explosiontexcoord2f[0];
	m.pointer_vertex = e->vert[0];
	R_Mesh_State(&m);

	GL_Color(alpha, alpha, alpha, 1);

	GL_LockArrays(0, numverts);
	R_Mesh_Draw(0, numverts, numtriangles, explosiontris[0]);
	GL_LockArrays(0, 0);
}

void R_MoveExplosion(explosion_t *e)
{
	int i;
	float dot, end[3], impact[3], normal[3], frametime;

	frametime = cl.time - e->time;
	e->time = cl.time;
	e->alpha = e->alpha - (e->fade * frametime);
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			VectorMA(e->vert[i], frametime, e->vertvel[i], end);
			if (e->clipping)
			{
				if (CL_TraceLine(e->vert[i], end, impact, normal, true, NULL, SUPERCONTENTS_SOLID) < 1)
				{
					// clip velocity against the wall
					dot = -DotProduct(e->vertvel[i], normal);
					VectorMA(e->vertvel[i], dot, normal, e->vertvel[i]);
				}
				VectorCopy(impact, e->vert[i]);
			}
			else
				VectorCopy(end, e->vert[i]);
		}
	}
}


void R_MoveExplosions(void)
{
	int i;
	float frametime;

	frametime = cl.time - cl.oldtime;

	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (cl.time < explosion[i].endtime)
			R_MoveExplosion(&explosion[i]);
}

void R_DrawExplosions(void)
{
	int i;

	if (!r_drawexplosions.integer)
		return;
	for (i = 0;i < MAX_EXPLOSIONS;i++)
		if (r_refdef.time < explosion[i].endtime)
			R_MeshQueue_AddTransparent(explosion[i].origin, R_DrawExplosionCallback, &explosion[i], 0);
}

