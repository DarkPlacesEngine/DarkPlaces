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

#define MAX_EXPLOSIONS 64
#define EXPLOSIONGRID 8
#define EXPLOSIONVERTS ((EXPLOSIONGRID+1)*(EXPLOSIONGRID+1))
#define EXPLOSIONTRIS (EXPLOSIONGRID*EXPLOSIONGRID*2)
#define EXPLOSIONSTARTRADIUS (20.0f)
#define EXPLOSIONSTARTVELOCITY (256.0f)
#define EXPLOSIONRANDOMVELOCITY (64.0f)
#define EXPLOSIONFADESTART (1.5f)
#define EXPLOSIONFADERATE (4.5f)
/*
#define MAX_EXPLOSIONGAS (MAX_EXPLOSIONS * EXPLOSIONGAS)
#define EXPLOSIONGAS 8
#define EXPLOSIONGASSTARTRADIUS (15.0f)
#define EXPLOSIONGASSTARTVELOCITY (50.0f)
#define GASDENSITY_SCALER (32768.0f / EXPLOSIONGAS)
#define GASFADERATE (GASDENSITY_SCALER * EXPLOSIONGAS * 2)

typedef struct explosiongas_s
{
	float pressure;
	vec3_t origin;
	vec3_t velocity;
}
explosiongas_t;

explosiongas_t explosiongas[MAX_EXPLOSIONGAS];
*/

vec3_t explosionspherevert[EXPLOSIONVERTS];
vec3_t explosionspherevertvel[EXPLOSIONVERTS];
float explosiontexcoords[EXPLOSIONVERTS][2];
int explosiontris[EXPLOSIONTRIS][3];
int explosionnoiseindex[EXPLOSIONVERTS];
vec3_t explosionpoint[EXPLOSIONVERTS];

typedef struct explosion_s
{
	float starttime;
	float alpha;
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
	byte noise1[128][128], noise2[128][128], noise3[128][128], data[128][128][4];
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
//	memset(explosiongas, 0, sizeof(explosiongas));
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
	explosionnoiseindex[i] = (row % EXPLOSIONGRID) * EXPLOSIONGRID + (column % EXPLOSIONGRID);
	explosiontexcoords[i][0] = (float) column / (float) EXPLOSIONGRID;
	explosiontexcoords[i][1] = (float) row / (float) EXPLOSIONGRID;
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
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		explosionspherevert[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTRADIUS;
		explosionspherevert[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTRADIUS;
		explosionspherevert[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTRADIUS;
		explosionspherevertvel[i][0] = explosionpoint[i][0] * EXPLOSIONSTARTVELOCITY;
		explosionspherevertvel[i][1] = explosionpoint[i][1] * EXPLOSIONSTARTVELOCITY;
		explosionspherevertvel[i][2] = explosionpoint[i][2] * EXPLOSIONSTARTVELOCITY;
	}

	Cvar_RegisterVariable(&r_explosionclip);
	Cvar_RegisterVariable(&r_drawexplosions);

	R_RegisterModule("R_Explosions", r_explosion_start, r_explosion_shutdown, r_explosion_newmap);
}

void R_NewExplosion(vec3_t org)
{
	int i, j;
	float dist, v[3], normal[3];
	byte noise[4][EXPLOSIONGRID*EXPLOSIONGRID];
	fractalnoise(noise[0], EXPLOSIONGRID, 4);
	fractalnoise(noise[1], EXPLOSIONGRID, 2);
	fractalnoise(noise[2], EXPLOSIONGRID, 2);
	fractalnoise(noise[3], EXPLOSIONGRID, 2);
	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha <= 0.0f)
		{
			explosion[i].alpha = EXPLOSIONFADESTART;
			for (j = 0;j < EXPLOSIONVERTS;j++)
			{
				dist = noise[3][explosionnoiseindex[j]] * (1.0f / 256.0f) + 0.5;
				VectorMA(org, dist, explosionspherevert[j], v);
				TraceLine(org, v, explosion[i].vert[j], normal, 0);
				VectorAdd(explosion[i].vert[j], normal, explosion[i].vert[j]);
				explosion[i].vertvel[j][0] = explosionspherevertvel[j][0] * dist + (((float) noise[0][explosionnoiseindex[j]] - 128.0f) * (EXPLOSIONRANDOMVELOCITY / 128.0f));
				explosion[i].vertvel[j][1] = explosionspherevertvel[j][1] * dist + (((float) noise[1][explosionnoiseindex[j]] - 128.0f) * (EXPLOSIONRANDOMVELOCITY / 128.0f));
				explosion[i].vertvel[j][2] = explosionspherevertvel[j][2] * dist + (((float) noise[2][explosionnoiseindex[j]] - 128.0f) * (EXPLOSIONRANDOMVELOCITY / 128.0f));
			}
			break;
		}
	}

	/*
	i = 0;
	j = EXPLOSIONGAS;
	while (i < MAX_EXPLOSIONGAS && j > 0)
	{
		while (explosiongas[i].pressure > 0)
		{
			i++;
			if (i >= MAX_EXPLOSIONGAS)
				return;
		}
		VectorRandom(v);
		VectorMA(org, EXPLOSIONGASSTARTRADIUS, v, v);
		TraceLine(org, v, explosiongas[i].origin, NULL, 0);
		VectorRandom(v);
		VectorScale(v, EXPLOSIONGASSTARTVELOCITY, explosiongas[i].velocity);
		explosiongas[i].pressure = j * GASDENSITY_SCALER;
		j--;
	}
	*/
}

void R_DrawExplosion(explosion_t *e)
{
	int i;
	float c[EXPLOSIONVERTS][4], diff[3], fog, ifog, alpha;
	rmeshinfo_t m;
	memset(&m, 0, sizeof(m));
	m.transparent = true;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.numtriangles = EXPLOSIONTRIS;
	m.index = &explosiontris[0][0];
	m.numverts = EXPLOSIONVERTS;
	m.vertex = &e->vert[0][0];
	m.vertexstep = sizeof(float[3]);
	alpha = e->alpha;
	if (alpha > 1)
		alpha = 1;
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = alpha;
	if (fogenabled)
	{
		m.color = &c[0][0];
		m.colorstep = sizeof(float[4]);
		for (i = 0;i < EXPLOSIONVERTS;i++)
		{
			// use inverse fog alpha as color
			VectorSubtract(e->vert[i], r_origin, diff);
			ifog = 1 - exp(fogdensity/DotProduct(diff,diff));
			if (ifog < 0)
				ifog = 0;
			c[i][0] = ifog;
			c[i][1] = ifog;
			c[i][2] = ifog;
			c[i][3] = alpha;
		}
	}
	m.tex[0] = R_GetTexture(explosiontexture);
	m.texcoords[0] = &explosiontexcoords[0][0];
	m.texcoordstep[0] = sizeof(float[2]);

	R_Mesh_Draw(&m);

	if (fogenabled)
	{
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
		for (i = 0;i < EXPLOSIONVERTS;i++)
		{
			VectorSubtract(e->vert[i], r_origin, diff);
			fog = exp(fogdensity/DotProduct(diff,diff));
			c[i][0] = fogcolor[0];
			c[i][1] = fogcolor[1];
			c[i][2] = fogcolor[2];
			c[i][3] = alpha * fog;
		}
		//m.color = &c[0][0];
		//m.colorstep = sizeof(float[4]);
		m.tex[0] = R_GetTexture(explosiontexturefog);
		R_Mesh_Draw(&m);
	}
}

void R_MoveExplosion(explosion_t *e, /*explosiongas_t **list, explosiongas_t **listend, */float frametime)
{
	int i;
	float f, dot, frictionscale, end[3], impact[3], normal[3];
	/*
	vec3_t diff;
	vec_t dist;
	explosiongas_t **l;
	*/
	e->alpha -= frametime * EXPLOSIONFADERATE;
	frictionscale = 1 - frametime;
	frictionscale = bound(0, frictionscale, 1);
	for (i = 0;i < EXPLOSIONVERTS;i++)
	{
		if (e->vertvel[i][0] || e->vertvel[i][1] || e->vertvel[i][2])
		{
			end[0] = e->vert[i][0] + frametime * e->vertvel[i][0];
			end[1] = e->vert[i][1] + frametime * e->vertvel[i][1];
			end[2] = e->vert[i][2] + frametime * e->vertvel[i][2];
			if (r_explosionclip.integer)
			{
				f = TraceLine(e->vert[i], end, impact, normal, 0);
				VectorCopy(impact, e->vert[i]);
				if (f < 1)
				{
					// clip velocity against the wall
					dot = DotProduct(e->vertvel[i], normal) * 1.125f;
					e->vertvel[i][0] -= normal[0] * dot;
					e->vertvel[i][1] -= normal[1] * dot;
					e->vertvel[i][2] -= normal[2] * dot;
				}
			}
			else
			{
				VectorCopy(end, e->vert[i]);
			}
			e->vertvel[i][2] += sv_gravity.value * frametime * -0.25f;
			VectorScale(e->vertvel[i], frictionscale, e->vertvel[i]);
		}
		/*
		for (l = list;l < listend;l++)
		{
			VectorSubtract(e->vert[i], (*l)->origin, diff);
			dist = DotProduct(diff, diff);
			if (dist < 4096 && dist >= 1)
			{
				dist = (*l)->pressure * frametime / dist;
				VectorMA(e->vertvel[i], dist, diff, e->vertvel[i]);
			}
		}
		*/
	}
}

/*
void R_MoveExplosionGas(explosiongas_t *e, explosiongas_t **list, explosiongas_t **listend, float frametime)
{
	vec3_t end, diff;
	vec_t dist, frictionscale;
	explosiongas_t **l;
	frictionscale = 1 - frametime;
	frictionscale = bound(0, frictionscale, 1);
	if (e->velocity[0] || e->velocity[1] || e->velocity[2])
	{
		end[0] = e->origin[0] + frametime * e->velocity[0];
		end[1] = e->origin[1] + frametime * e->velocity[1];
		end[2] = e->origin[2] + frametime * e->velocity[2];
		if (r_explosionclip.integer)
		{
			float f, dot;
			vec3_t impact, normal;
			f = TraceLine(e->origin, end, impact, normal, 0);
			VectorCopy(impact, e->origin);
			if (f < 1)
			{
				// clip velocity against the wall
				dot = DotProduct(e->velocity, normal) * -1.3f;
				e->velocity[0] += normal[0] * dot;
				e->velocity[1] += normal[1] * dot;
				e->velocity[2] += normal[2] * dot;
			}
		}
		else
		{
			VectorCopy(end, e->origin);
		}
		e->velocity[2] += sv_gravity.value * frametime;
		VectorScale(e->velocity, frictionscale, e->velocity);
	}
	for (l = list;l < listend;l++)
	{
		if (*l != e)
		{
			VectorSubtract(e->origin, (*l)->origin, diff);
			dist = DotProduct(diff, diff);
			if (dist < 4096 && dist >= 1)
			{
				dist = (*l)->pressure * frametime / dist;
				VectorMA(e->velocity, dist, diff, e->velocity);
			}
		}
	}
}
*/

void R_MoveExplosions(void)
{
	int i;
	float frametime;
//	explosiongas_t *gaslist[MAX_EXPLOSIONGAS], **l, **end;
	frametime = cl.time - cl.oldtime;
	/*
	l = &gaslist[0];
	for (i = 0;i < MAX_EXPLOSIONGAS;i++)
	{
		if (explosiongas[i].pressure > 0)
		{
			explosiongas[i].pressure -= frametime * GASFADERATE;
			if (explosiongas[i].pressure > 0)
				*l++ = &explosiongas[i];
		}
	}
	end = l;
	for (l = gaslist;l < end;l++)
		R_MoveExplosionGas(*l, gaslist, end, frametime);
	*/

	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha > 0.0f)
		{
			if (explosion[i].starttime > cl.time)
			{
				explosion[i].alpha = 0;
				continue;
			}
			R_MoveExplosion(&explosion[i], /*gaslist, end, */frametime);
		}
	}
}

void R_DrawExplosions(void)
{
	int i;
	if (!r_drawexplosions.integer)
		return;
	for (i = 0;i < MAX_EXPLOSIONS;i++)
	{
		if (explosion[i].alpha > 0.0f)
		{
			R_DrawExplosion(&explosion[i]);
		}
	}
}
