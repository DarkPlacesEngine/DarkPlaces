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

static rtexture_t *particlefonttexture;
// [0] is normal, [1] is fog, they may be the same
static particletexture_t particletexture[MAX_PARTICLETEXTURES][2];

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
	// smoke/blood
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

	// rain splash
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
		setuptex(i + 8, 0, i + 16, &data[0][0][0], particletexturedata);
		setuptex(i + 8, 1, i + 16, &data[0][0][0], particletexturedata);
	}

	// normal particle
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
	setuptex(24, 0, 32, &data[0][0][0], particletexturedata);
	setuptex(24, 1, 32, &data[0][0][0], particletexturedata);

	// rain
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
	setuptex(25, 0, 33, &data[0][0][0], particletexturedata);
	setuptex(25, 1, 33, &data[0][0][0], particletexturedata);

	// bubble
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
	setuptex(26, 0, 34, &data[0][0][0], particletexturedata);
	setuptex(26, 1, 34, &data[0][0][0], particletexturedata);

	// rocket flare
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
	setuptex(27, 0, 35, &data[0][0][0], particletexturedata);
	for (y = 0;y < 32;y++)
		for (x = 0;x < 32;x++)
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
	setuptex(28, 1, 36, &data[0][0][0], particletexturedata);

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

//int partindexarray[6] = {0, 1, 2, 0, 2, 3};

void R_DrawParticles (void)
{
	renderparticle_t *r;
	int i, lighting;
	float minparticledist, org[3], uprightangles[3], up2[3], right2[3], v[3], right[3], up[3], tvxyz[4][4], tvst[4][2], fog, ifog, fogvec[3];
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

	// LordHavoc: this meshinfo must match up with R_Mesh_DrawDecal
	// LordHavoc: the commented out lines are hardwired behavior in R_Mesh_DrawDecal
	memset(&m, 0, sizeof(m));
	m.transparent = true;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	//m.numtriangles = 2;
	//m.index = partindexarray;
	//m.numverts = 4;
	m.vertex = &tvxyz[0][0];
	//m.vertexstep = sizeof(float[4]);
	m.tex[0] = R_GetTexture(particlefonttexture);
	m.texcoords[0] = &tvst[0][0];
	//m.texcoordstep[0] = sizeof(float[2]);

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
			VectorScale(vright, r->scalex, right);
			VectorScale(vup, r->scaley, up);
		}
		else if (r->orientation == PARTICLE_UPRIGHT_FACING)
		{
			VectorScale(right2, r->scalex, right);
			VectorScale(up2, r->scaley, up);
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
			VectorScale(right, r->scalex, right);
			VectorScale(up, r->scaley, up);
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

		tvxyz[0][0] = org[0] - right[0] - up[0];
		tvxyz[0][1] = org[1] - right[1] - up[1];
		tvxyz[0][2] = org[2] - right[2] - up[2];
		tvxyz[1][0] = org[0] - right[0] + up[0];
		tvxyz[1][1] = org[1] - right[1] + up[1];
		tvxyz[1][2] = org[2] - right[2] + up[2];
		tvxyz[2][0] = org[0] + right[0] + up[0];
		tvxyz[2][1] = org[1] + right[1] + up[1];
		tvxyz[2][2] = org[2] + right[2] + up[2];
		tvxyz[3][0] = org[0] + right[0] - up[0];
		tvxyz[3][1] = org[1] + right[1] - up[1];
		tvxyz[3][2] = org[2] + right[2] - up[2];
		tvst[0][0] = tex->s1;
		tvst[0][1] = tex->t1;
		tvst[1][0] = tex->s1;
		tvst[1][1] = tex->t2;
		tvst[2][0] = tex->s2;
		tvst[2][1] = tex->t2;
		tvst[3][0] = tex->s2;
		tvst[3][1] = tex->t1;

		if (r->additive)
		{
			m.blendfunc2 = GL_ONE;
			fog = 0;
			if (fogenabled)
			{
				texfog = &particletexture[r->tex][1];
				VectorSubtract(org, r_origin, fogvec);
				ifog = 1 - exp(fogdensity/DotProduct(fogvec,fogvec));
				if (ifog < (1.0f - (1.0f / 64.0f)))
				{
					if (ifog >= (1.0f / 64.0f))
					{
						// partially fogged, darken it
						m.cr *= ifog;
						m.cg *= ifog;
						m.cb *= ifog;
						R_Mesh_DrawDecal(&m);
					}
				}
				else
					R_Mesh_DrawDecal(&m);
			}
			else
				R_Mesh_DrawDecal(&m);
		}
		else
		{
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
			fog = 0;
			if (fogenabled)
			{
				texfog = &particletexture[r->tex][1];
				VectorSubtract(org, r_origin, fogvec);
				fog = exp(fogdensity/DotProduct(fogvec,fogvec));
				if (fog >= (1.0f / 64.0f))
				{
					if (fog >= (1.0f - (1.0f / 64.0f)))
					{
						// fully fogged, just use the fog texture and render as alpha
						m.cr = fogcolor[0];
						m.cg = fogcolor[1];
						m.cb = fogcolor[2];
						m.ca = r->color[3];
						tvst[0][0] = texfog->s1;
						tvst[0][1] = texfog->t1;
						tvst[1][0] = texfog->s1;
						tvst[1][1] = texfog->t2;
						tvst[2][0] = texfog->s2;
						tvst[2][1] = texfog->t2;
						tvst[3][0] = texfog->s2;
						tvst[3][1] = texfog->t1;
						R_Mesh_DrawDecal(&m);
					}
					else
					{
						// partially fogged, darken the first pass
						ifog = 1 - fog;
						m.cr *= ifog;
						m.cg *= ifog;
						m.cb *= ifog;
						if (tex->s1 == texfog->s1 && tex->t1 == texfog->t1)
						{
							// fog texture is the same as the base, just change the color
							m.cr += fogcolor[0] * fog;
							m.cg += fogcolor[1] * fog;
							m.cb += fogcolor[2] * fog;
							R_Mesh_DrawDecal(&m);
						}
						else
						{
							// render the first pass (alpha), then do additive fog
							R_Mesh_DrawDecal(&m);

							m.blendfunc2 = GL_ONE;
							m.cr = fogcolor[0];
							m.cg = fogcolor[1];
							m.cb = fogcolor[2];
							m.ca = r->color[3] * fog;
							tvst[0][0] = texfog->s1;
							tvst[0][1] = texfog->t1;
							tvst[1][0] = texfog->s1;
							tvst[1][1] = texfog->t2;
							tvst[2][0] = texfog->s2;
							tvst[2][1] = texfog->t2;
							tvst[3][0] = texfog->s2;
							tvst[3][1] = texfog->t1;
							R_Mesh_DrawDecal(&m);
						}
					}
				}
				else
					R_Mesh_DrawDecal(&m);
			}
			else
				R_Mesh_DrawDecal(&m);
		}
	}
}
