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

static rtexturepool_t *particletexturepool;

// these are used by the decal system so they can't be static
rtexture_t *particlefonttexture;
// [0] is normal, [1] is fog, they may be the same
particletexture_t particletexture[MAX_PARTICLETEXTURES][2];

static cvar_t r_drawparticles = {0, "r_drawparticles", "1"};
static cvar_t r_particles_lighting = {0, "r_particles_lighting", "1"};

static byte shadebubble(float dx, float dy, vec3_t light)
{
	float	dz, f, dot;
	vec3_t	normal;
	dz = 1 - (dx*dx+dy*dy);
	if (dz > 0) // it does hit the sphere
	{
		f = 0;
		// back side
		normal[0] = dx;normal[1] = dy;normal[2] = dz;
		VectorNormalize(normal);
		dot = DotProduct(normal, light);
		if (dot > 0.5) // interior reflection
			f += ((dot *  2) - 1);
		else if (dot < -0.5) // exterior reflection
			f += ((dot * -2) - 1);
		// front side
		normal[0] = dx;normal[1] = dy;normal[2] = -dz;
		VectorNormalize(normal);
		dot = DotProduct(normal, light);
		if (dot > 0.5) // interior reflection
			f += ((dot *  2) - 1);
		else if (dot < -0.5) // exterior reflection
			f += ((dot * -2) - 1);
		f *= 128;
		f += 16; // just to give it a haze so you can see the outline
		f = bound(0, f, 255);
		return (byte) f;
	}
	else
		return 0;
}

static void setuptex(int cltexnum, int fog, int rtexnum, byte *data, byte *particletexturedata)
{
	int basex, basey, y;
	basex = ((rtexnum >> 0) & 7) * 32;
	basey = ((rtexnum >> 3) & 7) * 32;
	particletexture[cltexnum][fog].s1 = (basex + 1) / 256.0f;
	particletexture[cltexnum][fog].t1 = (basey + 1) / 256.0f;
	particletexture[cltexnum][fog].s2 = (basex + 31) / 256.0f;
	particletexture[cltexnum][fog].t2 = (basey + 31) / 256.0f;
	for (y = 0;y < 32;y++)
		memcpy(particletexturedata + ((basey + y) * 256 + basex) * 4, data + y * 32 * 4, 32 * 4);
}

static void R_InitParticleTexture (void)
{
	int		x,y,d,i,m;
	float	dx, dy, radius, f, f2;
	byte	data[32][32][4], noise1[64][64], noise2[64][64];
	vec3_t	light;
	byte	particletexturedata[256*256*4];

	memset(particletexturedata, 255, sizeof(particletexturedata));

	// the particletexture[][] array numbers must match the cl_part.c textures
	for (i = 0;i < 8;i++)
	{
		do
		{
			fractalnoise(&noise1[0][0], 64, 4);
			fractalnoise(&noise2[0][0], 64, 8);
			m = 0;
			for (y = 0;y < 32;y++)
			{
				dy = y - 16;
				for (x = 0;x < 32;x++)
				{
					d = (noise1[y][x] - 128) * 2 + 64; // was + 128
					d = bound(0, d, 255);
					data[y][x][0] = data[y][x][1] = data[y][x][2] = d;
					dx = x - 16;
					d = (noise2[y][x] - 128) * 3 + 192;
					if (d > 0)
				 		d = (d * (256 - (int) (dx*dx+dy*dy))) >> 8;
					d = bound(0, d, 255);
					data[y][x][3] = (byte) d;
					if (m < d)
						m = d;
				}
			}
		}
		while (m < 224);

		setuptex(i + 0, 0, i + 0, &data[0][0][0], particletexturedata);
		for (y = 0;y < 32;y++)
			for (x = 0;x < 32;x++)
				data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
		setuptex(i + 0, 1, i + 8, &data[0][0][0], particletexturedata);
	}

	for (i = 0;i < 8;i++)
	{
		float p[32][32];
		fractalnoise(&noise1[0][0], 64, 8);
		for (y = 0;y < 32;y++)
			for (x = 0;x < 32;x++)
				p[y][x] = (noise1[y][x] / 8.0f) - 64.0f;
		for (m = 0;m < 32;m++)
		{
			int j;
			float fx, fy, f;
			fx = lhrandom(14, 18);
			fy = lhrandom(14, 18);
			do
			{
				dx = lhrandom(-1, 1);
				dy = lhrandom(-1, 1);
				f = (dx * dx + dy * dy);
			}
			while(f < 0.125f || f > 1.0f);
			f = (m + 1) / 40.0f; //lhrandom(0.0f, 1.0);
			dx *= 1.0f / 32.0f;
			dy *= 1.0f / 32.0f;
			for (j = 0;f > 0 && j < (32 * 14);j++)
			{
				y = fy;
				x = fx;
				fx += dx;
				fy += dy;
				if (x < 1 || y < 1 || x >= 31 || y >= 31)
					break;
				p[y - 1][x - 1] += f * 0.125f;
				p[y - 1][x    ] += f * 0.25f;
				p[y - 1][x + 1] += f * 0.125f;
				p[y    ][x - 1] += f * 0.25f;
				p[y    ][x    ] += f;
				p[y    ][x + 1] += f * 0.25f;
				p[y + 1][x - 1] += f * 0.125f;
				p[y + 1][x    ] += f * 0.25f;
				p[y + 1][x + 1] += f * 0.125f;
//				f -= (0.5f / (32 * 16));
			}
		}
		for (y = 0;y < 32;y++)
		{
			for (x = 0;x < 32;x++)
			{
				m = p[y][x];
				data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
				data[y][x][3] = (byte) bound(0, m, 255);
			}
		}

		setuptex(i + 8, 0, i + 16, &data[0][0][0], particletexturedata);
		setuptex(i + 8, 1, i + 16, &data[0][0][0], particletexturedata);
	}

	for (i = 0;i < 16;i++)
	{
		radius = i * 3.0f / 16.0f;
		f2 = 255.0f * ((15.0f - i) / 15.0f);
		for (y = 0;y < 32;y++)
		{
			dy = (y - 16) * 0.25f;
			for (x = 0;x < 32;x++)
			{
				dx = (x - 16) * 0.25f;
				data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
				f = (1.0 - fabs(radius - sqrt(dx*dx+dy*dy))) * f2;
				f = bound(0.0f, f, 255.0f);
				data[y][x][3] = (int) f;
			}
		}
		setuptex(i + 16, 0, i + 24, &data[0][0][0], particletexturedata);
		setuptex(i + 16, 1, i + 24, &data[0][0][0], particletexturedata);
	}

	for (y = 0;y < 32;y++)
	{
		dy = y - 16;
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			dx = x - 16;
			d = (256 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y][x][3] = (byte) d;
		}
	}
	setuptex(32, 0, 40, &data[0][0][0], particletexturedata);
	setuptex(32, 1, 40, &data[0][0][0], particletexturedata);

	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 8.0), y < 24 ? (y - 24) * (1.0 / 24.0) : (y - 24) * (1.0 / 8.0), light);
		}
	}
	setuptex(33, 0, 41, &data[0][0][0], particletexturedata);
	setuptex(33, 1, 41, &data[0][0][0], particletexturedata);

	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 16.0), (y - 16) * (1.0 / 16.0), light);
		}
	}
	setuptex(34, 0, 42, &data[0][0][0], particletexturedata);
	setuptex(34, 1, 42, &data[0][0][0], particletexturedata);

	for (y = 0;y < 32;y++)
	{
		dy = y - 16;
		for (x = 0;x < 32;x++)
		{
			dx = x - 16;
			d = (2048.0f / (dx*dx+dy*dy+1)) - 8.0f;
			data[y][x][0] = bound(0, d * 1.0f, 255);
			data[y][x][1] = bound(0, d * 0.8f, 255);
			data[y][x][2] = bound(0, d * 0.5f, 255);
			data[y][x][3] = bound(0, d * 1.0f, 255);
		}
	}
	setuptex(35, 0, 43, &data[0][0][0], particletexturedata);
	for (y = 0;y < 32;y++)
		for (x = 0;x < 32;x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
	setuptex(35, 1, 44, &data[0][0][0], particletexturedata);

	particlefonttexture = R_LoadTexture (particletexturepool, "particlefont", 256, 256, particletexturedata, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
}

static void r_part_start(void)
{
	particletexturepool = R_AllocTexturePool();
	R_InitParticleTexture ();
}

static void r_part_shutdown(void)
{
	R_FreeTexturePool(&particletexturepool);
}

static void r_part_newmap(void)
{
}

void R_Particles_Init (void)
{
	Cvar_RegisterVariable(&r_drawparticles);
	Cvar_RegisterVariable(&r_particles_lighting);
	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown, r_part_newmap);
}

int partindexarray[6] = {0, 1, 2, 0, 2, 3};

void R_DrawParticles (void)
{
	renderparticle_t *r;
	int i, lighting;
	float minparticledist, org[3], uprightangles[3], up2[3], right2[3], v[3], right[3], up[3], tv[4][5], fog, diff[3];
	mleaf_t *leaf;
	particletexture_t *tex, *texfog;
	rmeshinfo_t m;

	// LordHavoc: early out conditions
	if ((!r_refdef.numparticles) || (!r_drawparticles.integer))
		return;

	lighting = r_particles_lighting.integer;
	if (!r_dynamic.integer)
		lighting = 0;

	c_particles += r_refdef.numparticles;

	uprightangles[0] = 0;
	uprightangles[1] = r_refdef.viewangles[1];
	uprightangles[2] = 0;
	AngleVectors (uprightangles, NULL, right2, up2);

	minparticledist = DotProduct(r_origin, vpn) + 16.0f;

	memset(&m, 0, sizeof(m));
	m.transparent = true;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.numtriangles = 2;
	m.index = partindexarray;
	m.numverts = 4;
	m.vertex = &tv[0][0];
	m.vertexstep = sizeof(float[5]);
	m.tex[0] = R_GetTexture(particlefonttexture);
	m.texcoords[0] = &tv[0][3];
	m.texcoordstep[0] = sizeof(float[5]);

	for (i = 0, r = r_refdef.particles;i < r_refdef.numparticles;i++, r++)
	{
		// LordHavoc: only render if not too close
		if (DotProduct(r->org, vpn) < minparticledist)
			continue;

		// LordHavoc: check if it's in a visible leaf
		leaf = Mod_PointInLeaf(r->org, cl.worldmodel);
		if (leaf->visframe != r_framecount)
			continue;

		VectorCopy(r->org, org);
		if (r->orientation == PARTICLE_BILLBOARD)
		{
			VectorScale(vright, r->scale, right);
			VectorScale(vup, r->scale, up);
		}
		else if (r->orientation == PARTICLE_UPRIGHT_FACING)
		{
			VectorScale(right2, r->scale, right);
			VectorScale(up2, r->scale, up);
		}
		else if (r->orientation == PARTICLE_ORIENTED_DOUBLESIDED)
		{
			// double-sided
			if (DotProduct(r->dir, r_origin) > DotProduct(r->dir, org))
			{
				VectorNegate(r->dir, v);
				VectorVectors(v, right, up);
			}
			else
				VectorVectors(r->dir, right, up);
			VectorScale(right, r->scale, right);
			VectorScale(up, r->scale, up);
		}
		else
			Host_Error("R_DrawParticles: unknown particle orientation %i\n", r->orientation);

		m.cr = r->color[0];
		m.cg = r->color[1];
		m.cb = r->color[2];
		m.ca = r->color[3];
		if (lighting >= 1 && (r->dynlight || lighting >= 2))
		{
			R_CompleteLightPoint(v, org, true, leaf);
			m.cr *= v[0];
			m.cg *= v[1];
			m.cb *= v[2];
		}

		tex = &particletexture[r->tex][0];
		texfog = &particletexture[r->tex][1];

		fog = 0;
		if (fogenabled)
		{
			VectorSubtract(org, r_origin, diff);
			fog = exp(fogdensity/DotProduct(diff,diff));
			if (fog >= 0.01f)
			{
				if (fog > 1)
					fog = 1;
				m.cr *= 1 - fog;
				m.cg *= 1 - fog;
				m.cb *= 1 - fog;
				if (tex->s1 == texfog->s1 && tex->t1 == texfog->t1)
				{
					m.cr += fogcolor[0] * fog;
					m.cg += fogcolor[1] * fog;
					m.cb += fogcolor[2] * fog;
				}
			}
			else
				fog = 0;
		}

		tv[0][0] = org[0] - right[0] - up[0];
		tv[0][1] = org[1] - right[1] - up[1];
		tv[0][2] = org[2] - right[2] - up[2];
		tv[0][3] = tex->s1;
		tv[0][4] = tex->t1;
		tv[1][0] = org[0] - right[0] + up[0];
		tv[1][1] = org[1] - right[1] + up[1];
		tv[1][2] = org[2] - right[2] + up[2];
		tv[1][3] = tex->s1;
		tv[1][4] = tex->t2;
		tv[2][0] = org[0] + right[0] + up[0];
		tv[2][1] = org[1] + right[1] + up[1];
		tv[2][2] = org[2] + right[2] + up[2];
		tv[2][3] = tex->s2;
		tv[2][4] = tex->t2;
		tv[3][0] = org[0] + right[0] - up[0];
		tv[3][1] = org[1] + right[1] - up[1];
		tv[3][2] = org[2] + right[2] - up[2];
		tv[3][3] = tex->s2;
		tv[3][4] = tex->t1;

		R_Mesh_Draw(&m);

		if (fog && (tex->s1 != texfog->s1 || tex->t1 != texfog->t1))
		{
			m.blendfunc2 = GL_ONE;
			m.cr = fogcolor[0];
			m.cg = fogcolor[1];
			m.cb = fogcolor[2];
			m.ca = r->color[3] * fog;

			tv[0][3] = texfog->s1;
			tv[0][4] = texfog->t1;
			tv[1][3] = texfog->s1;
			tv[1][4] = texfog->t2;
			tv[2][3] = texfog->s2;
			tv[2][4] = texfog->t2;
			tv[3][3] = texfog->s2;
			tv[3][4] = texfog->t1;

			R_Mesh_Draw(&m);
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
	}
}
