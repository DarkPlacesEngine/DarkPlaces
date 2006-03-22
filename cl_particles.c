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
#include "image.h"

// must match ptype_t values
particletype_t particletype[pt_total] =
{
	{PBLEND_ALPHA, PARTICLE_BILLBOARD, false}, //pt_alphastatic
	{PBLEND_ADD, PARTICLE_BILLBOARD, false}, //pt_static
	{PBLEND_ADD, PARTICLE_SPARK, false}, //pt_spark
	{PBLEND_ADD, PARTICLE_BEAM, false}, //pt_beam
	{PBLEND_ADD, PARTICLE_SPARK, false}, //pt_rain
	{PBLEND_ADD, PARTICLE_ORIENTED_DOUBLESIDED, false}, //pt_raindecal
	{PBLEND_ADD, PARTICLE_BILLBOARD, false}, //pt_snow
	{PBLEND_ADD, PARTICLE_BILLBOARD, false}, //pt_bubble
	{PBLEND_MOD, PARTICLE_BILLBOARD, false}, //pt_blood
	{PBLEND_ADD, PARTICLE_BILLBOARD, false}, //pt_smoke
	{PBLEND_MOD, PARTICLE_ORIENTED_DOUBLESIDED, false}, //pt_decal
	{PBLEND_ALPHA, PARTICLE_BILLBOARD, false}, //pt_entityparticle
};

static int particlepalette[256] =
{
	0x000000,0x0f0f0f,0x1f1f1f,0x2f2f2f,0x3f3f3f,0x4b4b4b,0x5b5b5b,0x6b6b6b, // 0-7
	0x7b7b7b,0x8b8b8b,0x9b9b9b,0xababab,0xbbbbbb,0xcbcbcb,0xdbdbdb,0xebebeb, // 8-15
	0x0f0b07,0x170f0b,0x1f170b,0x271b0f,0x2f2313,0x372b17,0x3f2f17,0x4b371b, // 16-23
	0x533b1b,0x5b431f,0x634b1f,0x6b531f,0x73571f,0x7b5f23,0x836723,0x8f6f23, // 24-31
	0x0b0b0f,0x13131b,0x1b1b27,0x272733,0x2f2f3f,0x37374b,0x3f3f57,0x474767, // 32-39
	0x4f4f73,0x5b5b7f,0x63638b,0x6b6b97,0x7373a3,0x7b7baf,0x8383bb,0x8b8bcb, // 40-47
	0x000000,0x070700,0x0b0b00,0x131300,0x1b1b00,0x232300,0x2b2b07,0x2f2f07, // 48-55
	0x373707,0x3f3f07,0x474707,0x4b4b0b,0x53530b,0x5b5b0b,0x63630b,0x6b6b0f, // 56-63
	0x070000,0x0f0000,0x170000,0x1f0000,0x270000,0x2f0000,0x370000,0x3f0000, // 64-71
	0x470000,0x4f0000,0x570000,0x5f0000,0x670000,0x6f0000,0x770000,0x7f0000, // 72-79
	0x131300,0x1b1b00,0x232300,0x2f2b00,0x372f00,0x433700,0x4b3b07,0x574307, // 80-87
	0x5f4707,0x6b4b0b,0x77530f,0x835713,0x8b5b13,0x975f1b,0xa3631f,0xaf6723, // 88-95
	0x231307,0x2f170b,0x3b1f0f,0x4b2313,0x572b17,0x632f1f,0x733723,0x7f3b2b, // 96-103
	0x8f4333,0x9f4f33,0xaf632f,0xbf772f,0xcf8f2b,0xdfab27,0xefcb1f,0xfff31b, // 104-111
	0x0b0700,0x1b1300,0x2b230f,0x372b13,0x47331b,0x533723,0x633f2b,0x6f4733, // 112-119
	0x7f533f,0x8b5f47,0x9b6b53,0xa77b5f,0xb7876b,0xc3937b,0xd3a38b,0xe3b397, // 120-127
	0xab8ba3,0x9f7f97,0x937387,0x8b677b,0x7f5b6f,0x775363,0x6b4b57,0x5f3f4b, // 128-135
	0x573743,0x4b2f37,0x43272f,0x371f23,0x2b171b,0x231313,0x170b0b,0x0f0707, // 136-143
	0xbb739f,0xaf6b8f,0xa35f83,0x975777,0x8b4f6b,0x7f4b5f,0x734353,0x6b3b4b, // 144-151
	0x5f333f,0x532b37,0x47232b,0x3b1f23,0x2f171b,0x231313,0x170b0b,0x0f0707, // 152-159
	0xdbc3bb,0xcbb3a7,0xbfa39b,0xaf978b,0xa3877b,0x977b6f,0x876f5f,0x7b6353, // 160-167
	0x6b5747,0x5f4b3b,0x533f33,0x433327,0x372b1f,0x271f17,0x1b130f,0x0f0b07, // 168-175
	0x6f837b,0x677b6f,0x5f7367,0x576b5f,0x4f6357,0x475b4f,0x3f5347,0x374b3f, // 176-183
	0x2f4337,0x2b3b2f,0x233327,0x1f2b1f,0x172317,0x0f1b13,0x0b130b,0x070b07, // 184-191
	0xfff31b,0xefdf17,0xdbcb13,0xcbb70f,0xbba70f,0xab970b,0x9b8307,0x8b7307, // 192-199
	0x7b6307,0x6b5300,0x5b4700,0x4b3700,0x3b2b00,0x2b1f00,0x1b0f00,0x0b0700, // 200-207
	0x0000ff,0x0b0bef,0x1313df,0x1b1bcf,0x2323bf,0x2b2baf,0x2f2f9f,0x2f2f8f, // 208-215
	0x2f2f7f,0x2f2f6f,0x2f2f5f,0x2b2b4f,0x23233f,0x1b1b2f,0x13131f,0x0b0b0f, // 216-223
	0x2b0000,0x3b0000,0x4b0700,0x5f0700,0x6f0f00,0x7f1707,0x931f07,0xa3270b, // 224-231
	0xb7330f,0xc34b1b,0xcf632b,0xdb7f3b,0xe3974f,0xe7ab5f,0xefbf77,0xf7d38b, // 232-239
	0xa77b3b,0xb79b37,0xc7c337,0xe7e357,0x7fbfff,0xabe7ff,0xd7ffff,0x670000, // 240-247
	0x8b0000,0xb30000,0xd70000,0xff0000,0xfff393,0xfff7c7,0xffffff,0x9f5b53  // 248-255
};

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

//static int explosparkramp[8] = {0x4b0700, 0x6f0f00, 0x931f07, 0xb7330f, 0xcf632b, 0xe3974f, 0xffe7b5, 0xffffff};

// texture numbers in particle font
static const int tex_smoke[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const int tex_bulletdecal[8] = {8, 9, 10, 11, 12, 13, 14, 15};
static const int tex_blooddecal[8] = {16, 17, 18, 19, 20, 21, 22, 23};
static const int tex_bloodparticle[8] = {24, 25, 26, 27, 28, 29, 30, 31};
static const int tex_rainsplash[16] = {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
static const int tex_particle = 63;
static const int tex_bubble = 62;
static const int tex_raindrop = 61;
static const int tex_beam = 60;

cvar_t cl_particles = {CVAR_SAVE, "cl_particles", "1", "enables particle effects"};
cvar_t cl_particles_quality = {CVAR_SAVE, "cl_particles_quality", "1", "multiplies number of particles and reduces their alpha"};
cvar_t cl_particles_size = {CVAR_SAVE, "cl_particles_size", "1", "multiplies particle size"};
cvar_t cl_particles_quake = {CVAR_SAVE, "cl_particles_quake", "0", "makes particle effects look mostly like the ones in Quake"};
cvar_t cl_particles_bloodshowers = {CVAR_SAVE, "cl_particles_bloodshowers", "1", "enables blood shower effects"};
cvar_t cl_particles_blood = {CVAR_SAVE, "cl_particles_blood", "1", "enables blood effects"};
cvar_t cl_particles_blood_alpha = {CVAR_SAVE, "cl_particles_blood_alpha", "0.5", "opacity of blood"};
cvar_t cl_particles_blood_bloodhack = {CVAR_SAVE, "cl_particles_blood_bloodhack", "1", "make certain quake particle() calls create blood effects instead"};
cvar_t cl_particles_bulletimpacts = {CVAR_SAVE, "cl_particles_bulletimpacts", "1", "enables bulletimpact effects"};
cvar_t cl_particles_explosions_bubbles = {CVAR_SAVE, "cl_particles_explosions_bubbles", "1", "enables bubbles from underwater explosions"};
cvar_t cl_particles_explosions_smoke = {CVAR_SAVE, "cl_particles_explosions_smokes", "0", "enables smoke from explosions"};
cvar_t cl_particles_explosions_sparks = {CVAR_SAVE, "cl_particles_explosions_sparks", "1", "enables sparks from explosions"};
cvar_t cl_particles_explosions_shell = {CVAR_SAVE, "cl_particles_explosions_shell", "0", "enables polygonal shell from explosions"};
cvar_t cl_particles_smoke = {CVAR_SAVE, "cl_particles_smoke", "1", "enables smoke (used by multiple effects)"};
cvar_t cl_particles_smoke_alpha = {CVAR_SAVE, "cl_particles_smoke_alpha", "0.5", "smoke brightness"};
cvar_t cl_particles_smoke_alphafade = {CVAR_SAVE, "cl_particles_smoke_alphafade", "0.55", "brightness fade per second"};
cvar_t cl_particles_sparks = {CVAR_SAVE, "cl_particles_sparks", "1", "enables sparks (used by multiple effects)"};
cvar_t cl_particles_bubbles = {CVAR_SAVE, "cl_particles_bubbles", "1", "enables bubbles (used by multiple effects)"};
cvar_t cl_decals = {CVAR_SAVE, "cl_decals", "0", "enables decals (bullet holes, blood, etc)"};
cvar_t cl_decals_time = {CVAR_SAVE, "cl_decals_time", "0", "how long before decals start to fade away"};
cvar_t cl_decals_fadetime = {CVAR_SAVE, "cl_decals_fadetime", "20", "how long decals take to fade away"};

/*
===============
CL_InitParticles
===============
*/
void CL_ReadPointFile_f (void);
void CL_Particles_Init (void)
{
	Cmd_AddCommand ("pointfile", CL_ReadPointFile_f, "display point file produced by qbsp when a leak was detected in the map (a line leading through the leak hole, to an entity inside the level)");

	Cvar_RegisterVariable (&cl_particles);
	Cvar_RegisterVariable (&cl_particles_quality);
	Cvar_RegisterVariable (&cl_particles_size);
	Cvar_RegisterVariable (&cl_particles_quake);
	Cvar_RegisterVariable (&cl_particles_bloodshowers);
	Cvar_RegisterVariable (&cl_particles_blood);
	Cvar_RegisterVariable (&cl_particles_blood_alpha);
	Cvar_RegisterVariable (&cl_particles_blood_bloodhack);
	Cvar_RegisterVariable (&cl_particles_explosions_bubbles);
	Cvar_RegisterVariable (&cl_particles_explosions_smoke);
	Cvar_RegisterVariable (&cl_particles_explosions_sparks);
	Cvar_RegisterVariable (&cl_particles_explosions_shell);
	Cvar_RegisterVariable (&cl_particles_bulletimpacts);
	Cvar_RegisterVariable (&cl_particles_smoke);
	Cvar_RegisterVariable (&cl_particles_smoke_alpha);
	Cvar_RegisterVariable (&cl_particles_smoke_alphafade);
	Cvar_RegisterVariable (&cl_particles_sparks);
	Cvar_RegisterVariable (&cl_particles_bubbles);
	Cvar_RegisterVariable (&cl_decals);
	Cvar_RegisterVariable (&cl_decals_time);
	Cvar_RegisterVariable (&cl_decals_fadetime);
}

void CL_Particles_Shutdown (void)
{
}

// list of all 26 parameters:
// ptype - any of the pt_ enum values (pt_static, pt_blood, etc), see ptype_t near the top of this file
// pcolor1,pcolor2 - minimum and maximum ranges of color, randomly interpolated to decide particle color
// ptex - any of the tex_ values such as tex_smoke[rand()&7] or tex_particle
// psize - size of particle (or thickness for PARTICLE_SPARK and PARTICLE_BEAM)
// palpha - opacity of particle as 0-255 (can be more than 255)
// palphafade - rate of fade per second (so 256 would mean a 256 alpha particle would fade to nothing in 1 second)
// ptime - how long the particle can live (note it is also removed if alpha drops to nothing)
// pgravity - how much effect gravity has on the particle (0-1)
// pbounce - how much bounce the particle has when it hits a surface (0-1), -1 makes a blood splat when it hits a surface, 0 does not even check for collisions
// px,py,pz - starting origin of particle
// pvx,pvy,pvz - starting velocity of particle
// pfriction - how much the particle slows down per second (0-1 typically, can slowdown faster than 1)
particle_t *particle(particletype_t *ptype, int pcolor1, int pcolor2, int ptex, float psize, float palpha, float palphafade, float pgravity, float pbounce, float px, float py, float pz, float pvx, float pvy, float pvz, float pfriction, float originjitter, float velocityjitter)
{
	int l1, l2;
	particle_t *part;
	vec3_t v;
	for (;cl.free_particle < cl.max_particles && cl.particles[cl.free_particle].type;cl.free_particle++);
	if (cl.free_particle >= cl.max_particles)
		return NULL;
	part = &cl.particles[cl.free_particle++];
	if (cl.num_particles < cl.free_particle)
		cl.num_particles = cl.free_particle;
	memset(part, 0, sizeof(*part));
	part->type = ptype;
	l2 = (int)lhrandom(0.5, 256.5);
	l1 = 256 - l2;
	part->color[0] = ((((pcolor1 >> 16) & 0xFF) * l1 + ((pcolor2 >> 16) & 0xFF) * l2) >> 8) & 0xFF;
	part->color[1] = ((((pcolor1 >>  8) & 0xFF) * l1 + ((pcolor2 >>  8) & 0xFF) * l2) >> 8) & 0xFF;
	part->color[2] = ((((pcolor1 >>  0) & 0xFF) * l1 + ((pcolor2 >>  0) & 0xFF) * l2) >> 8) & 0xFF;
	part->color[3] = 0xFF;
	part->texnum = ptex;
	part->size = psize;
	part->alpha = palpha;
	part->alphafade = palphafade;
	part->gravity = pgravity;
	part->bounce = pbounce;
	VectorRandom(v);
	part->org[0] = px + originjitter * v[0];
	part->org[1] = py + originjitter * v[1];
	part->org[2] = pz + originjitter * v[2];
	part->vel[0] = pvx + velocityjitter * v[0];
	part->vel[1] = pvy + velocityjitter * v[1];
	part->vel[2] = pvz + velocityjitter * v[2];
	part->time2 = 0;
	part->friction = pfriction;
	return part;
}

void CL_SpawnDecalParticleForSurface(int hitent, const vec3_t org, const vec3_t normal, int color1, int color2, int texnum, float size, float alpha)
{
	particle_t *p;
	if (!cl_decals.integer)
		return;
	p = particle(particletype + pt_decal, color1, color2, texnum, size, alpha, 0, 0, 0, org[0] + normal[0], org[1] + normal[1], org[2] + normal[2], normal[0], normal[1], normal[2], 0, 0, 0);
	if (p)
	{
		p->time2 = cl.time;
		p->owner = hitent;
		p->ownermodel = cl.entities[p->owner].render.model;
		Matrix4x4_Transform(&cl.entities[p->owner].render.inversematrix, org, p->relativeorigin);
		Matrix4x4_Transform3x3(&cl.entities[p->owner].render.inversematrix, normal, p->relativedirection);
		VectorAdd(p->relativeorigin, p->relativedirection, p->relativeorigin);
	}
}

void CL_SpawnDecalParticleForPoint(const vec3_t org, float maxdist, float size, float alpha, int texnum, int color1, int color2)
{
	int i;
	float bestfrac, bestorg[3], bestnormal[3];
	float org2[3];
	int besthitent = 0, hitent;
	trace_t trace;
	bestfrac = 10;
	for (i = 0;i < 32;i++)
	{
		VectorRandom(org2);
		VectorMA(org, maxdist, org2, org2);
		trace = CL_TraceBox(org, vec3_origin, vec3_origin, org2, true, &hitent, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY, false);
		// take the closest trace result that doesn't end up hitting a NOMARKS
		// surface (sky for example)
		if (bestfrac > trace.fraction && !(trace.hitq3surfaceflags & Q3SURFACEFLAG_NOMARKS))
		{
			bestfrac = trace.fraction;
			besthitent = hitent;
			VectorCopy(trace.endpos, bestorg);
			VectorCopy(trace.plane.normal, bestnormal);
		}
	}
	if (bestfrac < 1)
		CL_SpawnDecalParticleForSurface(besthitent, bestorg, bestnormal, color1, color2, texnum, size, alpha);
}

/*
===============
CL_EntityParticles
===============
*/
void CL_EntityParticles (entity_t *ent)
{
	int i;
	float pitch, yaw, dist = 64, beamlength = 16, org[3], v[3];
	static vec3_t avelocities[NUMVERTEXNORMALS];
	if (!cl_particles.integer) return;

	Matrix4x4_OriginFromMatrix(&ent->render.matrix, org);

	if (!avelocities[0][0])
		for (i = 0;i < NUMVERTEXNORMALS * 3;i++)
			avelocities[0][i] = lhrandom(0, 2.55);

	for (i = 0;i < NUMVERTEXNORMALS;i++)
	{
		yaw = cl.time * avelocities[i][0];
		pitch = cl.time * avelocities[i][1];
		v[0] = org[0] + m_bytenormals[i][0] * dist + (cos(pitch)*cos(yaw)) * beamlength;
		v[1] = org[1] + m_bytenormals[i][1] * dist + (cos(pitch)*sin(yaw)) * beamlength;
		v[2] = org[2] + m_bytenormals[i][2] * dist + (-sin(pitch)) * beamlength;
		particle(particletype + pt_entityparticle, particlepalette[0x6f], particlepalette[0x6f], tex_particle, 1, 255, 0, 0, 0, v[0], v[1], v[2], 0, 0, 0, 0, 0, 0);
	}
}


void CL_ReadPointFile_f (void)
{
	vec3_t org, leakorg;
	int r, c, s;
	char *pointfile = NULL, *pointfilepos, *t, tchar;
	char name[MAX_OSPATH];

	if (!cl.worldmodel)
		return;

	FS_StripExtension (cl.worldmodel->name, name, sizeof (name));
	strlcat (name, ".pts", sizeof (name));
	pointfile = (char *)FS_LoadFile(name, tempmempool, true, NULL);
	if (!pointfile)
	{
		Con_Printf("Could not open %s\n", name);
		return;
	}

	Con_Printf("Reading %s...\n", name);
	VectorClear(leakorg);
	c = 0;
	s = 0;
	pointfilepos = pointfile;
	while (*pointfilepos)
	{
		while (*pointfilepos == '\n' || *pointfilepos == '\r')
			pointfilepos++;
		if (!*pointfilepos)
			break;
		t = pointfilepos;
		while (*t && *t != '\n' && *t != '\r')
			t++;
		tchar = *t;
		*t = 0;
		r = sscanf (pointfilepos,"%f %f %f", &org[0], &org[1], &org[2]);
		*t = tchar;
		pointfilepos = t;
		if (r != 3)
			break;
		if (c == 0)
			VectorCopy(org, leakorg);
		c++;

		if (cl.num_particles < cl.max_particles - 3)
		{
			s++;
			particle(particletype + pt_static, particlepalette[(-c)&15], particlepalette[(-c)&15], tex_particle, 2, 255, 0, 0, 0, org[0], org[1], org[2], 0, 0, 0, 0, 0, 0);
		}
	}
	Mem_Free(pointfile);
	VectorCopy(leakorg, org);
	Con_Printf("%i points read (%i particles spawned)\nLeak at %f %f %f\n", c, s, org[0], org[1], org[2]);

	particle(particletype + pt_beam, 0xFF0000, 0xFF0000, tex_beam, 64, 255, 0, 0, 0, org[0] - 4096, org[1], org[2], org[0] + 4096, org[1], org[2], 0, 0, 0);
	particle(particletype + pt_beam, 0x00FF00, 0x00FF00, tex_beam, 64, 255, 0, 0, 0, org[0], org[1] - 4096, org[2], org[0], org[1] + 4096, org[2], 0, 0, 0);
	particle(particletype + pt_beam, 0x0000FF, 0x0000FF, tex_beam, 64, 255, 0, 0, 0, org[0], org[1], org[2] - 4096, org[0], org[1], org[2] + 4096, 0, 0, 0);
}

/*
===============
CL_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void CL_ParseParticleEffect (void)
{
	vec3_t org, dir;
	int i, count, msgcount, color;

	MSG_ReadVector(org, cls.protocol);
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar ();
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	if (cl_particles_blood_bloodhack.integer && !cl_particles_quake.integer)
	{
		if (color == 73)
		{
			// regular blood
			CL_BloodPuff(org, dir, count / 2);
			return;
		}
		if (color == 225)
		{
			// lightning blood
			CL_BloodPuff(org, dir, count / 2);
			return;
		}
	}
	CL_RunParticleEffect (org, dir, color, count);
}

/*
===============
CL_ParticleExplosion

===============
*/
void CL_ParticleExplosion (vec3_t org)
{
	int i;
	trace_t trace;
	//vec3_t v;
	//vec3_t v2;
	if (cl_stainmaps.integer)
		R_Stain(org, 96, 80, 80, 80, 64, 176, 176, 176, 64);
	CL_SpawnDecalParticleForPoint(org, 40, 48, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);

	if (cl_particles_quake.integer)
	{
		for (i = 0;i < 1024;i++)
		{
			int r, color;
			r = rand()&3;
			if (i & 1)
			{
				color = particlepalette[ramp1[r]];
				particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 32 * (8 - r), 318, 0, 0, org[0], org[1], org[2], 0, 0, 0, -4, 16, 256);
			}
			else
			{
				color = particlepalette[ramp2[r]];
				particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 32 * (8 - r), 478, 0, 0, org[0], org[1], org[2], 0, 0, 0, 1, 16, 256);
			}
		}
	}
	else
	{
		i = CL_PointSuperContents(org);
		if (i & (SUPERCONTENTS_SLIME | SUPERCONTENTS_WATER))
		{
			if (cl_particles.integer && cl_particles_bubbles.integer && cl_particles_explosions_bubbles.integer)
				for (i = 0;i < 128 * cl_particles_quality.value;i++)
					particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, lhrandom(128, 255), 128, -0.125, 1.5, org[0], org[1], org[2], 0, 0, 0, (1.0 / 16.0), 16, 96);
		}
		else
		{
			// LordHavoc: smoke effect similar to UT2003, chews fillrate too badly up close
			// smoke puff
			if (cl_particles.integer && cl_particles_smoke.integer && cl_particles_explosions_smoke.integer)
			{
				for (i = 0;i < 32;i++)
				{
					int k;
					vec3_t v, v2;
					for (k = 0;k < 16;k++)
					{
						v[0] = org[0] + lhrandom(-48, 48);
						v[1] = org[1] + lhrandom(-48, 48);
						v[2] = org[2] + lhrandom(-48, 48);
						trace = CL_TraceBox(org, vec3_origin, vec3_origin, v, true, NULL, SUPERCONTENTS_SOLID, false);
						if (trace.fraction >= 0.1)
							break;
					}
					VectorSubtract(trace.endpos, org, v2);
					VectorScale(v2, 2.0f, v2);
					particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 12, 32, 64, 0, 0, org[0], org[1], org[2], v2[0], v2[1], v2[2], 0, 0, 0);
				}
			}

			if (cl_particles.integer && cl_particles_sparks.integer && cl_particles_explosions_sparks.integer)
				for (i = 0;i < 128 * cl_particles_quality.value;i++)
					particle(particletype + pt_spark, 0x903010, 0xFFD030, tex_particle, 1.0f, lhrandom(0, 255), 512, 1, 0, org[0], org[1], org[2], 0, 0, 80, 0.2, 0, 256);
		}
	}

	if (cl_particles_explosions_shell.integer)
		R_NewExplosion(org);
}

/*
===============
CL_ParticleExplosion2

===============
*/
void CL_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int i, k;
	if (!cl_particles.integer) return;

	for (i = 0;i < 512 * cl_particles_quality.value;i++)
	{
		k = particlepalette[colorStart + (i % colorLength)];
		if (cl_particles_quake.integer)
			particle(particletype + pt_static, k, k, tex_particle, 1, 255, 850, 0, 0, org[0], org[1], org[2], 0, 0, 0, -4, 8, 256);
		else
			particle(particletype + pt_static, k, k, tex_particle, lhrandom(0.5, 1.5), 255, 512, 0, 0, org[0], org[1], org[2], 0, 0, 0, lhrandom(1.5, 3), 8, 192);
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion (vec3_t org)
{
	int i, k;
	if (!cl_particles.integer) return;

	if (!cl_particles_quake.integer)
	{
		CL_ParticleExplosion(org);
		return;
	}

	for (i = 0;i < 1024 * cl_particles_quality.value;i++)
	{
		if (i & 1)
		{
			k = particlepalette[66 + rand()%6];
			particle(particletype + pt_static, k, k, tex_particle, 1, lhrandom(182, 255), 182, 0, 0, org[0], org[1], org[2], 0, 0, 0, -4, 16, 256);
		}
		else
		{
			k = particlepalette[150 + rand()%6];
			particle(particletype + pt_static, k, k, tex_particle, 1, lhrandom(182, 255), 182, 0, 0, org[0], org[1], org[2], 0, 0, lhrandom(-256, 256), 0, 16, 0);
		}
	}
}

/*
===============
CL_RunParticleEffect

===============
*/
void CL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int k;

	if (count == 1024)
	{
		CL_ParticleExplosion(org);
		return;
	}
	if (!cl_particles.integer) return;
	if (cl_particles_quake.integer)
	{
		count *= cl_particles_quality.value;
		while (count--)
		{
			k = particlepalette[color + (rand()&7)];
			particle(particletype + pt_alphastatic, k, k, tex_particle, 1, lhrandom(51, 255), 512, 0, 0.05, org[0], org[1], org[2], dir[0], dir[1], dir[2], 0, 8, 0);
		}
	}
	else
	{
		count *= cl_particles_quality.value;
		while (count--)
		{
			k = particlepalette[color + (rand()&7)];
			if (gamemode == GAME_GOODVSBAD2)
				particle(particletype + pt_alphastatic, k, k, tex_particle, 5, 255, 300, 0, 0, org[0], org[1], org[2], 0, 0, 0, 0, 8, 10);
			else
				particle(particletype + pt_alphastatic, k, k, tex_particle, 1, 255, 512, 0, 0, org[0], org[1], org[2], dir[0], dir[1], dir[2], 0, 8, 15);
		}
	}
}

// LordHavoc: added this for spawning sparks/dust (which have strong gravity)
/*
===============
CL_SparkShower
===============
*/
void CL_SparkShower (vec3_t org, vec3_t dir, int count, vec_t gravityscale, vec_t radius)
{
	int k;

	if (!cl_particles.integer) return;

	if (cl_particles_sparks.integer)
	{
		// sparks
		count *= cl_particles_quality.value;
		while(count--)
		{
			k = particlepalette[0x68 + (rand() & 7)];
			particle(particletype + pt_spark, k, k, tex_particle, 0.4f, lhrandom(64, 255), 512, gravityscale, 0, org[0], org[1], org[2], dir[0], dir[1], dir[2] + sv_gravity.value * 0.1, 0, radius, 64);
		}
	}
}

void CL_Smoke (vec3_t org, vec3_t dir, int count, vec_t radius)
{
	vec3_t org2;
	int k;
	trace_t trace;

	if (!cl_particles.integer) return;

	// smoke puff
	if (cl_particles_smoke.integer)
	{
		k = count * 0.25 * cl_particles_quality.value;
		while(k--)
		{
			org2[0] = org[0] + 0.125f * lhrandom(-count, count);
			org2[1] = org[1] + 0.125f * lhrandom(-count, count);
			org2[2] = org[2] + 0.125f * lhrandom(-count, count);
			trace = CL_TraceBox(org, vec3_origin, vec3_origin, org2, true, NULL, SUPERCONTENTS_SOLID, false);
			particle(particletype + pt_smoke, 0x101010, 0x202020, tex_smoke[rand()&7], 3, 255, 1024, 0, 0, trace.endpos[0], trace.endpos[1], trace.endpos[2], 0, 0, 0, 0, radius, 8);
		}
	}
}

void CL_BulletMark (vec3_t org)
{
	if (cl_stainmaps.integer)
		R_Stain(org, 32, 96, 96, 96, 24, 128, 128, 128, 24);
	CL_SpawnDecalParticleForPoint(org, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
}

void CL_PlasmaBurn (vec3_t org)
{
	if (cl_stainmaps.integer)
		R_Stain(org, 48, 96, 96, 96, 32, 128, 128, 128, 32);
	CL_SpawnDecalParticleForPoint(org, 6, 6, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
}

static float bloodcount = 0;
void CL_BloodPuff (vec3_t org, vec3_t vel, int count)
{
	float s;
	vec3_t org2;
	trace_t trace;
	// bloodcount is used to accumulate counts too small to cause a blood particle
	if (!cl_particles.integer) return;
	if (cl_particles_quake.integer)
	{
		CL_RunParticleEffect(org, vel, 73, count * 2);
		return;
	}
	if (!cl_particles_blood.integer) return;

	s = count + 64.0f;
	count *= 5.0f;
	if (count > 1000)
		count = 1000;
	bloodcount += count * cl_particles_quality.value;
	while(bloodcount > 0)
	{
		org2[0] = org[0] + 0.125f * lhrandom(-bloodcount, bloodcount);
		org2[1] = org[1] + 0.125f * lhrandom(-bloodcount, bloodcount);
		org2[2] = org[2] + 0.125f * lhrandom(-bloodcount, bloodcount);
		trace = CL_TraceBox(org, vec3_origin, vec3_origin, org2, true, NULL, SUPERCONTENTS_SOLID, false);
		particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, cl_particles_blood_alpha.value * 768, cl_particles_blood_alpha.value * 384, 0, -1, trace.endpos[0], trace.endpos[1], trace.endpos[2], vel[0], vel[1], vel[2], 1, 0, s);
		bloodcount -= 16;
	}
}

void CL_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	vec3_t org, vel, diff, center, velscale;
	if (!cl_particles.integer) return;
	if (!cl_particles_bloodshowers.integer) return;
	if (!cl_particles_blood.integer) return;

	VectorSubtract(maxs, mins, diff);
	center[0] = (mins[0] + maxs[0]) * 0.5;
	center[1] = (mins[1] + maxs[1]) * 0.5;
	center[2] = (mins[2] + maxs[2]) * 0.5;
	velscale[0] = velspeed * 2.0 / diff[0];
	velscale[1] = velspeed * 2.0 / diff[1];
	velscale[2] = velspeed * 2.0 / diff[2];

	bloodcount += count * 5.0f * cl_particles_quality.value;
	while (bloodcount > 0)
	{
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		bloodcount -= 16;
		particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, cl_particles_blood_alpha.value * 768, cl_particles_blood_alpha.value * 384, 0, -1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 1, 0, 0);
	}
}

void CL_ParticleCube (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int gravity, int randomvel)
{
	int k;
	float t;
	if (!cl_particles.integer) return;
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	count *= cl_particles_quality.value;
	while (count--)
	{
		k = particlepalette[colorbase + (rand()&3)];
		particle(particletype + pt_alphastatic, k, k, tex_particle, 2, 255, 128, gravity ? 1 : 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0], dir[1], dir[2], 0, 0, randomvel);
	}
}

void CL_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type)
{
	int k;
	float t, z, minz, maxz;
	particle_t *p;
	if (!cl_particles.integer) return;
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}
	if (dir[2] < 0) // falling
		z = maxs[2];
	else // rising??
		z = mins[2];

	minz = z - fabs(dir[2]) * 0.1;
	maxz = z + fabs(dir[2]) * 0.1;
	minz = bound(mins[2], minz, maxs[2]);
	maxz = bound(mins[2], maxz, maxs[2]);

	count *= cl_particles_quality.value;

	switch(type)
	{
	case 0:
		count *= 4; // ick, this should be in the mod or maps?

		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			if (gamemode == GAME_GOODVSBAD2)
				particle(particletype + pt_rain, k, k, tex_particle, 20, lhrandom(8, 16), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0);
			else
				particle(particletype + pt_rain, k, k, tex_particle, 0.5, lhrandom(8, 16), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0);
		}
		break;
	case 1:
		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			if (gamemode == GAME_GOODVSBAD2)
				p = particle(particletype + pt_snow, k, k, tex_particle, 20, lhrandom(64, 128), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0);
			else
				p = particle(particletype + pt_snow, k, k, tex_particle, 1, lhrandom(64, 128), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0);
			if (p)
				VectorCopy(p->vel, p->relativedirection);
		}
		break;
	default:
		Con_Printf ("CL_ParticleRain: unknown type %i (0 = rain, 1 = snow)\n", type);
	}
}

void CL_Stardust (vec3_t mins, vec3_t maxs, int count)
{
	int k;
	float t;
	vec3_t o, v, center;
	if (!cl_particles.integer) return;

	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	center[0] = (mins[0] + maxs[0]) * 0.5f;
	center[1] = (mins[1] + maxs[1]) * 0.5f;
	center[2] = (mins[2] + maxs[2]) * 0.5f;

	count *= cl_particles_quality.value;
	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		o[0] = lhrandom(mins[0], maxs[0]);
		o[1] = lhrandom(mins[1], maxs[1]);
		o[2] = lhrandom(mins[2], maxs[2]);
		VectorSubtract(o, center, v);
		VectorNormalize(v);
		VectorScale(v, 100, v);
		v[2] += sv_gravity.value * 0.15f;
		particle(particletype + pt_static, 0x903010, 0xFFD030, tex_particle, 1.5, lhrandom(64, 128), 128, 1, 0, o[0], o[1], o[2], v[0], v[1], v[2], 0.2, 0, 0);
	}
}

void CL_FlameCube (vec3_t mins, vec3_t maxs, int count)
{
	int k;
	float t;
	if (!cl_particles.integer) return;
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	count *= cl_particles_quality.value;
	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		particle(particletype + pt_static, k, k, tex_particle, 4, lhrandom(64, 128), 384, -1, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), 0, 0, 32, 1, 0, 32);
		if (count & 1)
			particle(particletype + pt_static, 0x303030, 0x606060, tex_smoke[rand()&7], 6, lhrandom(48, 96), 64, 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), 0, 0, 24, 0, 0, 8);
	}
}

void CL_Flames (vec3_t org, vec3_t vel, int count)
{
	int k;
	if (!cl_particles.integer) return;

	count *= cl_particles_quality.value;
	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		particle(particletype + pt_static, k, k, tex_particle, 4, lhrandom(64, 128), 384, -1, 1.1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 1, 0, 128);
	}
}



/*
===============
CL_LavaSplash

===============
*/
void CL_LavaSplash (vec3_t origin)
{
	float i, j, inc, vel;
	int k, l;
	vec3_t		dir, org;
	if (!cl_particles.integer) return;

	if (cl_particles_quake.integer)
	{
		inc = 8 / cl_particles_quality.value;
		for (i = -128;i < 128;i += inc)
		{
			for (j = -128;j < 128;j += inc)
			{
				dir[0] = j + lhrandom(0, inc);
				dir[1] = i + lhrandom(0, inc);
				dir[2] = 256;
				org[0] = origin[0] + dir[0];
				org[1] = origin[1] + dir[1];
				org[2] = origin[2] + lhrandom(0, 64);
				vel = lhrandom(50, 120) / VectorLength(dir); // normalize and scale
				k = l = particlepalette[224 + (rand()&7)];
				particle(particletype + pt_alphastatic, k, l, tex_particle, 1, inc * lhrandom(24, 32), inc * 12, 0.05, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0);
			}
		}
	}
	else
	{
		inc = 32 / cl_particles_quality.value;
		for (i = -128;i < 128;i += inc)
		{
			for (j = -128;j < 128;j += inc)
			{
				dir[0] = j + lhrandom(0, inc);
				dir[1] = i + lhrandom(0, inc);
				dir[2] = 256;
				org[0] = origin[0] + dir[0];
				org[1] = origin[1] + dir[1];
				org[2] = origin[2] + lhrandom(0, 64);
				vel = lhrandom(50, 120) / VectorLength(dir); // normalize and scale
				if (gamemode == GAME_GOODVSBAD2)
				{
					k = particlepalette[0 + (rand()&255)];
					l = particlepalette[0 + (rand()&255)];
					particle(particletype + pt_static, k, l, tex_particle, 12, inc * 8, inc * 8, 0.05, 1, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0);
				}
				else
				{
					k = l = particlepalette[224 + (rand()&7)];
					particle(particletype + pt_static, k, l, tex_particle, 12, inc * 8, inc * 8, 0.05, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0);
				}
			}
		}
	}
}

/*
===============
CL_TeleportSplash

===============
*/
void CL_TeleportSplash (vec3_t org)
{
	float i, j, k, inc;
	if (!cl_particles.integer) return;

	if (cl_particles_quake.integer)
	{
		inc = 4 / cl_particles_quality.value;
		for (i = -16;i < 16;i += inc)
		{
			for (j = -16;j < 16;j += inc)
			{
				for (k = -24;k < 32;k += inc)
				{
					vec3_t dir;
					float vel;
					VectorSet(dir, i*8, j*8, k*8);
					VectorNormalize(dir);
					vel = lhrandom(50, 113);
					particle(particletype + pt_alphastatic, particlepalette[7], particlepalette[14], tex_particle, 1, inc * lhrandom(37, 63), inc * 187, 0, 0, org[0] + i + lhrandom(0, inc), org[1] + j + lhrandom(0, inc), org[2] + k + lhrandom(0, inc), dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0);
				}
			}
		}
	}
	else
	{
		inc = 8 / cl_particles_quality.value;
		for (i = -16;i < 16;i += inc)
			for (j = -16;j < 16;j += inc)
				for (k = -24;k < 32;k += inc)
					particle(particletype + pt_static, 0xA0A0A0, 0xFFFFFF, tex_particle, 10, inc * lhrandom(8, 16), inc * 32, 0, 0, org[0] + i + lhrandom(0, inc), org[1] + j + lhrandom(0, inc), org[2] + k + lhrandom(0, inc), 0, 0, lhrandom(-256, 256), 1, 0, 0);
	}
}

void CL_RocketTrail (vec3_t start, vec3_t end, int type, int color, entity_t *ent)
{
	vec3_t vec, dir, vel, pos;
	float len, dec, speed, qd;
	int smoke, blood, bubbles, r;

	if (end[0] == start[0] && end[1] == start[1] && end[2] == start[2])
		return;

	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	VectorSubtract (end, start, vec);
	len = VectorNormalizeLength (vec);
	dec = -ent->persistent.trail_time;
	ent->persistent.trail_time += len;
	if (ent->persistent.trail_time < 0.01f)
		return;

	// if we skip out, leave it reset
	ent->persistent.trail_time = 0.0f;

	speed = ent->state_current.time - ent->state_previous.time;
	if (speed)
		speed = 1.0f / speed;
	VectorSubtract(ent->state_current.origin, ent->state_previous.origin, vel);
	color = particlepalette[color];
	VectorScale(vel, speed, vel);

	// advance into this frame to reach the first puff location
	VectorMA(start, dec, vec, pos);
	len -= dec;

	smoke = cl_particles.integer && cl_particles_smoke.integer;
	blood = cl_particles.integer && cl_particles_blood.integer;
	bubbles = cl_particles.integer && cl_particles_bubbles.integer && (CL_PointSuperContents(pos) & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME));
	qd = 1.0f / cl_particles_quality.value;

	while (len >= 0)
	{
		switch (type)
		{
			case 0:	// rocket trail
				if (cl_particles_quake.integer)
				{
					dec = 3;
					r = rand()&3;
					color = particlepalette[ramp3[r]];
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 42*(6-r), 306, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 3, 0);
				}
				else
				{
					dec = 3;
					if (smoke)
					{
						particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, cl_particles_smoke_alpha.value*62, cl_particles_smoke_alphafade.value*62, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						particle(particletype + pt_static, 0x801010, 0xFFA020, tex_smoke[rand()&7], 3, cl_particles_smoke_alpha.value*288, cl_particles_smoke_alphafade.value*1400, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 20);
					}
					if (bubbles)
						particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, lhrandom(64, 255), 256, -0.25, 1.5, pos[0], pos[1], pos[2], 0, 0, 0, (1.0 / 16.0), 0, 16);
				}
				break;

			case 1: // grenade trail
				if (cl_particles_quake.integer)
				{
					dec = 3;
					r = 2 + (rand()%5);
					color = particlepalette[ramp3[r]];
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 42*(6-r), 306, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 3, 0);
				}
				else
				{
					dec = 3;
					if (smoke)
						particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, cl_particles_smoke_alpha.value*50, cl_particles_smoke_alphafade.value*50, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
				}
				break;


			case 2:	// blood
			case 4:	// slight blood
				if (cl_particles_quake.integer)
				{
					if (type == 2)
					{
						dec = 3;
						color = particlepalette[67 + (rand()&3)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 128, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 3, 0);
					}
					else
					{
						dec = 6;
						color = particlepalette[67 + (rand()&3)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 128, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 3, 0);
					}
				}
				else
				{
					dec = 16;
					if (blood)
						particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, qd * cl_particles_blood_alpha.value * 768.0f, qd * cl_particles_blood_alpha.value * 384.0f, 0, -1, pos[0], pos[1], pos[2], vel[0] * 0.5f, vel[1] * 0.5f, vel[2] * 0.5f, 1, 0, 64);
				}
				break;

			case 3:	// green tracer
				if (cl_particles_quake.integer)
				{
					dec = 6;
					color = particlepalette[52 + (rand()&7)];
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*vec[1], 30*-vec[0], 0, 0, 0, 0);
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*-vec[1], 30*vec[0], 0, 0, 0, 0);
				}
				else
				{
					dec = 16;
					if (smoke)
					{
						if (gamemode == GAME_GOODVSBAD2)
						{
							dec = 6;
							particle(particletype + pt_static, 0x00002E, 0x000030, tex_particle, 6, 128, 384, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						}
						else
						{
							dec = 3;
							color = particlepalette[20 + (rand()&7)];
							particle(particletype + pt_static, color, color, tex_particle, 2, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						}
					}
				}
				break;

			case 5:	// flame tracer
				if (cl_particles_quake.integer)
				{
					dec = 6;
					color = particlepalette[230 + (rand()&7)];
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*vec[1], 30*-vec[0], 0, 0, 0, 0);
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*-vec[1], 30*vec[0], 0, 0, 0, 0);
				}
				else
				{
					dec = 3;
					if (smoke)
					{
						color = particlepalette[226 + (rand()&7)];
						particle(particletype + pt_static, color, color, tex_particle, 2, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
					}
				}
				break;

			case 6:	// voor trail
				if (cl_particles_quake.integer)
				{
					dec = 3;
					color = particlepalette[152 + (rand()&3)];
					particle(particletype + pt_alphastatic, color, color, tex_particle, 1, 255, 850, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 8, 0);
				}
				else
				{
					dec = 16;
					if (smoke)
					{
						if (gamemode == GAME_GOODVSBAD2)
						{
							dec = 6;
							particle(particletype + pt_alphastatic, particlepalette[0 + (rand()&255)], particlepalette[0 + (rand()&255)], tex_particle, 6, 255, 384, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						}
						else if (gamemode == GAME_PRYDON)
						{
							dec = 6;
							particle(particletype + pt_static, 0x103040, 0x204050, tex_particle, 6, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						}
						else
						{
							dec = 3;
							particle(particletype + pt_static, 0x502030, 0x502030, tex_particle, 3, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
						}
					}
				}
				break;
			case 7:	// Nehahra smoke tracer
				dec = 7;
				if (smoke)
					particle(particletype + pt_alphastatic, 0x303030, 0x606060, tex_smoke[rand()&7], 7, 64, 320, 0, 0, pos[0], pos[1], pos[2], 0, 0, lhrandom(4, 12), 0, 0, 4);
				break;
			case 8: // Nexuiz plasma trail
				dec = 4;
				if (smoke)
					particle(particletype + pt_static, 0x283880, 0x283880, tex_particle, 4, 255, 1024, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 16);
				break;
			case 9: // glow trail
				dec = 3;
				if (smoke)
					particle(particletype + pt_alphastatic, color, color, tex_particle, 5, 128, 320, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0);
				break;
			default:
				Sys_Error("CL_RocketTrail: unknown trail type %i", type);
		}

		// advance to next time and position
		dec *= qd;
		len -= dec;
		VectorMA (pos, dec, vec, pos);
	}
	ent->persistent.trail_time = len;
}

void CL_BeamParticle (const vec3_t start, const vec3_t end, vec_t radius, float red, float green, float blue, float alpha, float lifetime)
{
	int tempcolor2, cr, cg, cb;
	cr = red * 255;
	cg = green * 255;
	cb = blue * 255;
	tempcolor2 = (bound(0, cr, 255) << 16) | (bound(0, cg, 255) << 8) | bound(0, cb, 255);
	particle(particletype + pt_beam, tempcolor2, tempcolor2, tex_beam, radius, alpha * 255, alpha * 255 / lifetime, 0, 0, start[0], start[1], start[2], end[0], end[1], end[2], 0, 0, 0);
}

void CL_Tei_Smoke(const vec3_t org, const vec3_t dir, int count)
{
	float f;
	if (!cl_particles.integer) return;

	// smoke puff
	if (cl_particles_smoke.integer)
		for (f = 0;f < count;f += 4.0f / cl_particles_quality.value)
			particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 255, 512, 0, 0, org[0], org[1], org[2], dir[0], dir[1], dir[2], 0, count * 0.125f, count * 0.5f);
}

void CL_Tei_PlasmaHit(const vec3_t org, const vec3_t dir, int count)
{
	float f;
	if (!cl_particles.integer) return;

	if (cl_stainmaps.integer)
		R_Stain(org, 40, 96, 96, 96, 40, 128, 128, 128, 40);
	CL_SpawnDecalParticleForPoint(org, 6, 8, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);

	// smoke puff
	if (cl_particles_smoke.integer)
		for (f = 0;f < count;f += 4.0f / cl_particles_quality.value)
			particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 255, 512, 0, 0, org[0], org[1], org[2], dir[0], dir[1], dir[2], 0, count * 0.125f, count);

	// sparks
	if (cl_particles_sparks.integer)
		for (f = 0;f < count;f += 1.0f / cl_particles_quality.value)
			particle(particletype + pt_spark, 0x2030FF, 0x80C0FF, tex_particle, 2.0f, lhrandom(64, 255), 512, 0, 0, org[0], org[1], org[2], dir[0], dir[1], dir[2], 0, 0, count * 3.0f);
}

/*
===============
CL_MoveParticles
===============
*/
void CL_MoveParticles (void)
{
	particle_t *p;
	int i, maxparticle, j, a, content;
	float gravity, dvel, bloodwaterfade, frametime, f, dist, org[3], oldorg[3];
	int hitent;
	trace_t trace;

	// LordHavoc: early out condition
	if (!cl.num_particles)
	{
		cl.free_particle = 0;
		return;
	}

	frametime = cl.time - cl.oldtime;
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;
	bloodwaterfade = max(cl_particles_blood_alpha.value, 0.01f) * frametime * 128.0f;

	maxparticle = -1;
	j = 0;
	for (i = 0, p = cl.particles;i < cl.num_particles;i++, p++)
	{
		if (!p->type)
			continue;
		maxparticle = i;
		content = 0;

		p->alpha -= p->alphafade * frametime;

		if (p->alpha <= 0)
		{
			p->type = NULL;
			continue;
		}

		if (p->type->orientation != PARTICLE_BEAM)
		{
			VectorCopy(p->org, oldorg);
			VectorMA(p->org, frametime, p->vel, p->org);
			VectorCopy(p->org, org);
			if (p->bounce)
			{
				trace = CL_TraceBox(oldorg, vec3_origin, vec3_origin, p->org, true, &hitent, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | (p->type == particletype + pt_rain ? SUPERCONTENTS_LIQUIDSMASK : 0), false);
				// if the trace started in or hit something of SUPERCONTENTS_NODROP
				// or if the trace hit something flagged as NOIMPACT
				// then remove the particle
				if (trace.hitq3surfaceflags & Q3SURFACEFLAG_NOIMPACT || ((trace.startsupercontents | trace.hitsupercontents) & SUPERCONTENTS_NODROP))
				{
					p->type = NULL;
					continue;
				}
				// react if the particle hit something
				if (trace.fraction < 1)
				{
					VectorCopy(trace.endpos, p->org);
					if (p->type == particletype + pt_rain)
					{
						// raindrop - splash on solid/water/slime/lava
						int count;
						// convert from a raindrop particle to a rainsplash decal
						VectorCopy(trace.plane.normal, p->vel);
						VectorAdd(p->org, p->vel, p->org);
						p->type = particletype + pt_raindecal;
						p->texnum = tex_rainsplash[0];
						p->time2 = cl.time;
						p->alphafade = p->alpha / 0.4;
						p->bounce = 0;
						p->friction = 0;
						p->gravity = 0;
						p->size = 8.0;
						count = rand() & 3;
						while(count--)
							particle(particletype + pt_spark, 0x000000, 0x707070, tex_particle, 0.25f, lhrandom(64, 255), 512, 1, 0, p->org[0], p->org[1], p->org[2], p->vel[0]*16, p->vel[1]*16, 32 + p->vel[2]*16, 0, 0, 32);
					}
					else if (p->type == particletype + pt_blood)
					{
						// blood - splash on solid
						if (trace.hitq3surfaceflags & Q3SURFACEFLAG_NOMARKS)
						{
							p->type = NULL;
							continue;
						}
						if (!cl_decals.integer)
						{
							p->type = NULL;
							continue;
						}
						// convert from a blood particle to a blood decal
						VectorCopy(trace.plane.normal, p->vel);
						VectorAdd(p->org, p->vel, p->org);
						if (cl_stainmaps.integer)
							R_Stain(p->org, 32, 32, 16, 16, p->alpha * p->size * (1.0f / 40.0f), 192, 48, 48, p->alpha * p->size * (1.0f / 40.0f));

						p->type = particletype + pt_decal;
						p->texnum = tex_blooddecal[rand()&7];
						p->owner = hitent;
						p->ownermodel = cl.entities[hitent].render.model;
						Matrix4x4_Transform(&cl.entities[hitent].render.inversematrix, p->org, p->relativeorigin);
						Matrix4x4_Transform3x3(&cl.entities[hitent].render.inversematrix, p->vel, p->relativedirection);
						p->time2 = cl.time;
						p->alphafade = 0;
						p->bounce = 0;
						p->friction = 0;
						p->gravity = 0;
						p->size *= 2.0f;
					}
					else if (p->bounce < 0)
					{
						// bounce -1 means remove on impact
						p->type = NULL;
						continue;
					}
					else
					{
						// anything else - bounce off solid
						dist = DotProduct(p->vel, trace.plane.normal) * -p->bounce;
						VectorMA(p->vel, dist, trace.plane.normal, p->vel);
						if (DotProduct(p->vel, p->vel) < 0.03)
							VectorClear(p->vel);
					}
				}
			}
			p->vel[2] -= p->gravity * gravity;

			if (p->friction)
			{
				f = p->friction * frametime;
				if (CL_PointSuperContents(p->org) & SUPERCONTENTS_LIQUIDSMASK)
					f *= 4;
				f = 1.0f - f;
				VectorScale(p->vel, f, p->vel);
			}
		}

		if (p->type != particletype + pt_static)
		{
			switch (p->type - particletype)
			{
			case pt_entityparticle:
				// particle that removes itself after one rendered frame
				if (p->time2)
					p->type = NULL;
				else
					p->time2 = 1;
				break;
			case pt_blood:
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME))
				{
					p->size += frametime * 8;
					//p->alpha -= bloodwaterfade;
				}
				else
					p->vel[2] -= gravity;
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_LAVA | SUPERCONTENTS_NODROP))
					p->type = NULL;
				break;
			case pt_bubble:
				a = CL_PointSuperContents(p->org);
				if (!(a & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME)))
				{
					p->type = NULL;
					break;
				}
				break;
			case pt_rain:
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_LIQUIDSMASK))
					p->type = NULL;
				break;
			case pt_snow:
				if (cl.time > p->time2)
				{
					// snow flutter
					p->time2 = cl.time + (rand() & 3) * 0.1;
					p->vel[0] = p->relativedirection[0] + lhrandom(-32, 32);
					p->vel[1] = p->relativedirection[1] + lhrandom(-32, 32);
					//p->vel[2] = p->relativedirection[2] + lhrandom(-32, 32);
				}
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_LIQUIDSMASK))
					p->type = NULL;
				break;
			case pt_smoke:
				//p->size += frametime * 15;
				break;
			case pt_decal:
				// FIXME: this has fairly wacky handling of alpha
				p->alphafade = cl.time > (p->time2 + cl_decals_time.value) ? (255 / cl_decals_fadetime.value) : 0;
				if (cl.entities[p->owner].render.model == p->ownermodel)
				{
					Matrix4x4_Transform(&cl.entities[p->owner].render.matrix, p->relativeorigin, p->org);
					Matrix4x4_Transform3x3(&cl.entities[p->owner].render.matrix, p->relativedirection, p->vel);
				}
				else
					p->type = NULL;
				break;
			case pt_raindecal:
				a = max(0, (cl.time - p->time2) * 40);
				if (a < 16)
					p->texnum = tex_rainsplash[a];
				else
					p->type = NULL;
				break;
			default:
				break;
			}
		}
	}
	cl.num_particles = maxparticle + 1;
	cl.free_particle = 0;
}

#define MAX_PARTICLETEXTURES 64
// particletexture_t is a rectangle in the particlefonttexture
typedef struct particletexture_s
{
	rtexture_t *texture;
	float s1, t1, s2, t2;
}
particletexture_t;

static rtexturepool_t *particletexturepool;
static rtexture_t *particlefonttexture;
static particletexture_t particletexture[MAX_PARTICLETEXTURES];

static cvar_t r_drawparticles = {0, "r_drawparticles", "1", "enables drawing of particles"};

#define PARTICLETEXTURESIZE 64
#define PARTICLEFONTSIZE (PARTICLETEXTURESIZE*8)

static unsigned char shadebubble(float dx, float dy, vec3_t light)
{
	float dz, f, dot;
	vec3_t normal;
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
		return (unsigned char) f;
	}
	else
		return 0;
}

static void setuptex(int texnum, unsigned char *data, unsigned char *particletexturedata)
{
	int basex, basey, y;
	basex = ((texnum >> 0) & 7) * PARTICLETEXTURESIZE;
	basey = ((texnum >> 3) & 7) * PARTICLETEXTURESIZE;
	particletexture[texnum].s1 = (basex + 1) / (float)PARTICLEFONTSIZE;
	particletexture[texnum].t1 = (basey + 1) / (float)PARTICLEFONTSIZE;
	particletexture[texnum].s2 = (basex + PARTICLETEXTURESIZE - 1) / (float)PARTICLEFONTSIZE;
	particletexture[texnum].t2 = (basey + PARTICLETEXTURESIZE - 1) / (float)PARTICLEFONTSIZE;
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
		memcpy(particletexturedata + ((basey + y) * PARTICLEFONTSIZE + basex) * 4, data + y * PARTICLETEXTURESIZE * 4, PARTICLETEXTURESIZE * 4);
}

void particletextureblotch(unsigned char *data, float radius, float red, float green, float blue, float alpha)
{
	int x, y;
	float cx, cy, dx, dy, f, iradius;
	unsigned char *d;
	cx = (lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius) + lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius)) * 0.5f;
	cy = (lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius) + lhrandom(radius + 1, PARTICLETEXTURESIZE - 2 - radius)) * 0.5f;
	iradius = 1.0f / radius;
	alpha *= (1.0f / 255.0f);
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - cx);
			dy = (y - cy);
			f = (1.0f - sqrt(dx * dx + dy * dy) * iradius) * alpha;
			if (f > 0)
			{
				d = data + (y * PARTICLETEXTURESIZE + x) * 4;
				d[0] += f * (red   - d[0]);
				d[1] += f * (green - d[1]);
				d[2] += f * (blue  - d[2]);
			}
		}
	}
}

void particletextureclamp(unsigned char *data, int minr, int ming, int minb, int maxr, int maxg, int maxb)
{
	int i;
	for (i = 0;i < PARTICLETEXTURESIZE*PARTICLETEXTURESIZE;i++, data += 4)
	{
		data[0] = bound(minr, data[0], maxr);
		data[1] = bound(ming, data[1], maxg);
		data[2] = bound(minb, data[2], maxb);
	}
}

void particletextureinvert(unsigned char *data)
{
	int i;
	for (i = 0;i < PARTICLETEXTURESIZE*PARTICLETEXTURESIZE;i++, data += 4)
	{
		data[0] = 255 - data[0];
		data[1] = 255 - data[1];
		data[2] = 255 - data[2];
	}
}

// Those loops are in a separate function to work around an optimization bug in Mac OS X's GCC
static void R_InitBloodTextures (unsigned char *particletexturedata)
{
	int i, j, k, m;
	unsigned char data[PARTICLETEXTURESIZE][PARTICLETEXTURESIZE][4];

	// blood particles
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		for (k = 0;k < 24;k++)
			particletextureblotch(&data[0][0][0], PARTICLETEXTURESIZE/16, 96, 0, 0, 160);
		//particletextureclamp(&data[0][0][0], 32, 32, 32, 255, 255, 255);
		particletextureinvert(&data[0][0][0]);
		setuptex(tex_bloodparticle[i], &data[0][0][0], particletexturedata);
	}

	// blood decals
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		m = 8;
		for (j = 1;j < 10;j++)
			for (k = min(j, m - 1);k < m;k++)
				particletextureblotch(&data[0][0][0], (float)j*PARTICLETEXTURESIZE/64.0f, 96, 0, 0, 192 - j * 8);
		//particletextureclamp(&data[0][0][0], 32, 32, 32, 255, 255, 255);
		particletextureinvert(&data[0][0][0]);
		setuptex(tex_blooddecal[i], &data[0][0][0], particletexturedata);
	}

}

static void R_InitParticleTexture (void)
{
	int x, y, d, i, k, m;
	float dx, dy, radius, f, f2;
	unsigned char data[PARTICLETEXTURESIZE][PARTICLETEXTURESIZE][4], noise3[64][64], data2[64][16][4];
	vec3_t light;
	unsigned char *particletexturedata;

	// a note: decals need to modulate (multiply) the background color to
	// properly darken it (stain), and they need to be able to alpha fade,
	// this is a very difficult challenge because it means fading to white
	// (no change to background) rather than black (darkening everything
	// behind the whole decal polygon), and to accomplish this the texture is
	// inverted (dark red blood on white background becomes brilliant cyan
	// and white on black background) so we can alpha fade it to black, then
	// we invert it again during the blendfunc to make it work...

	particletexturedata = (unsigned char *)Mem_Alloc(tempmempool, PARTICLEFONTSIZE*PARTICLEFONTSIZE*4);
	memset(particletexturedata, 255, PARTICLEFONTSIZE*PARTICLEFONTSIZE*4);

	// smoke
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		do
		{
			unsigned char noise1[PARTICLETEXTURESIZE*2][PARTICLETEXTURESIZE*2], noise2[PARTICLETEXTURESIZE*2][PARTICLETEXTURESIZE*2];

			fractalnoise(&noise1[0][0], PARTICLETEXTURESIZE*2, PARTICLETEXTURESIZE/8);
			fractalnoise(&noise2[0][0], PARTICLETEXTURESIZE*2, PARTICLETEXTURESIZE/4);
			m = 0;
			for (y = 0;y < PARTICLETEXTURESIZE;y++)
			{
				dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
				for (x = 0;x < PARTICLETEXTURESIZE;x++)
				{
					dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
					d = (noise2[y][x] - 128) * 3 + 192;
					if (d > 0)
						d = d * (1-(dx*dx+dy*dy));
					d = (d * noise1[y][x]) >> 7;
					d = bound(0, d, 255);
					data[y][x][3] = (unsigned char) d;
					if (m < d)
						m = d;
				}
			}
		}
		while (m < 224);
		setuptex(tex_smoke[i], &data[0][0][0], particletexturedata);
	}

	// rain splash
	for (i = 0;i < 16;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		radius = i * 3.0f / 4.0f / 16.0f;
		f2 = 255.0f * ((15.0f - i) / 15.0f);
		for (y = 0;y < PARTICLETEXTURESIZE;y++)
		{
			dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			for (x = 0;x < PARTICLETEXTURESIZE;x++)
			{
				dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
				f = f2 * (1.0 - 4.0f * fabs(radius - sqrt(dx*dx+dy*dy)));
				data[y][x][3] = (int) (bound(0.0f, f, 255.0f));
			}
		}
		setuptex(tex_rainsplash[i], &data[0][0][0], particletexturedata);
	}

	// normal particle
	memset(&data[0][0][0], 255, sizeof(data));
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			d = 256 * (1 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y][x][3] = (unsigned char) d;
		}
	}
	setuptex(tex_particle, &data[0][0][0], particletexturedata);

	// rain
	memset(&data[0][0][0], 255, sizeof(data));
	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		// stretch upper half of bubble by +50% and shrink lower half by -50%
		// (this gives an elongated teardrop shape)
		if (dy > 0.5f)
			dy = (dy - 0.5f) * 2.0f;
		else
			dy = (dy - 0.5f) / 1.5f;
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			// shrink bubble width to half
			dx *= 2.0f;
			data[y][x][3] = shadebubble(dx, dy, light);
		}
	}
	setuptex(tex_raindrop, &data[0][0][0], particletexturedata);

	// bubble
	memset(&data[0][0][0], 255, sizeof(data));
	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			data[y][x][3] = shadebubble(dx, dy, light);
		}
	}
	setuptex(tex_bubble, &data[0][0][0], particletexturedata);

	// Blood particles and blood decals
	R_InitBloodTextures (particletexturedata);

	// bullet decals
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		for (k = 0;k < 12;k++)
			particletextureblotch(&data[0][0][0], PARTICLETEXTURESIZE/16, 0, 0, 0, 128);
		for (k = 0;k < 3;k++)
			particletextureblotch(&data[0][0][0], PARTICLETEXTURESIZE/2, 0, 0, 0, 160);
		//particletextureclamp(&data[0][0][0], 64, 64, 64, 255, 255, 255);
		particletextureinvert(&data[0][0][0]);
		setuptex(tex_bulletdecal[i], &data[0][0][0], particletexturedata);
	}

#if 0
	Image_WriteTGARGBA ("particles/particlefont.tga", PARTICLEFONTSIZE, PARTICLEFONTSIZE, particletexturedata);
#endif

	particlefonttexture = loadtextureimage(particletexturepool, "particles/particlefont.tga", 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (!particlefonttexture)
		particlefonttexture = R_LoadTexture2D(particletexturepool, "particlefont", PARTICLEFONTSIZE, PARTICLEFONTSIZE, particletexturedata, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
	for (i = 0;i < MAX_PARTICLETEXTURES;i++)
		particletexture[i].texture = particlefonttexture;

	// nexbeam
	fractalnoise(&noise3[0][0], 64, 4);
	m = 0;
	for (y = 0;y < 64;y++)
	{
		dy = (y - 0.5f*64) / (64*0.5f-1);
		for (x = 0;x < 16;x++)
		{
			dx = (x - 0.5f*16) / (16*0.5f-2);
			d = (1 - sqrt(fabs(dx))) * noise3[y][x];
			data2[y][x][0] = data2[y][x][1] = data2[y][x][2] = (unsigned char) bound(0, d, 255);
			data2[y][x][3] = 255;
		}
	}

#if 0
	Image_WriteTGARGBA ("particles/nexbeam.tga", 64, 64, &data2[0][0][0]);
#endif

	particletexture[tex_beam].texture = loadtextureimage(particletexturepool, "particles/nexbeam.tga", 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (!particletexture[tex_beam].texture)
		particletexture[tex_beam].texture = R_LoadTexture2D(particletexturepool, "nexbeam", 16, 64, &data2[0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	particletexture[tex_beam].s1 = 0;
	particletexture[tex_beam].t1 = 0;
	particletexture[tex_beam].s2 = 1;
	particletexture[tex_beam].t2 = 1;
	Mem_Free(particletexturedata);
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
	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown, r_part_newmap);
}

float particle_vertex3f[12], particle_texcoord2f[8];

void R_DrawParticle_TransparentCallback(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	const particle_t *p = cl.particles + surfacenumber;
	rmeshstate_t m;
	pblend_t blendmode;
	float org[3], up2[3], v[3], right[3], up[3], fog, ifog, cr, cg, cb, ca, size;
	particletexture_t *tex;

	VectorCopy(p->org, org);

	blendmode = p->type->blendmode;
	tex = &particletexture[p->texnum];
	cr = p->color[0] * (1.0f / 255.0f);
	cg = p->color[1] * (1.0f / 255.0f);
	cb = p->color[2] * (1.0f / 255.0f);
	ca = p->alpha * (1.0f / 255.0f);
	if (blendmode == PBLEND_MOD)
	{
		cr *= ca;
		cg *= ca;
		cb *= ca;
		cr = min(cr, 1);
		cg = min(cg, 1);
		cb = min(cb, 1);
		ca = 1;
	}
	ca /= cl_particles_quality.value;
	if (p->type->lighting)
	{
		float ambient[3], diffuse[3], diffusenormal[3];
		R_CompleteLightPoint(ambient, diffuse, diffusenormal, org, true);
		cr *= (ambient[0] + 0.5 * diffuse[0]);
		cg *= (ambient[1] + 0.5 * diffuse[1]);
		cb *= (ambient[2] + 0.5 * diffuse[2]);
	}
	if (fogenabled)
	{
		fog = VERTEXFOGTABLE(VectorDistance(org, r_vieworigin));
		ifog = 1 - fog;
		cr = cr * ifog;
		cg = cg * ifog;
		cb = cb * ifog;
		if (blendmode == PBLEND_ALPHA)
		{
			cr += fogcolor[0] * fog;
			cg += fogcolor[1] * fog;
			cb += fogcolor[2] * fog;
		}
	}

	R_Mesh_Matrix(&identitymatrix);

	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(tex->texture);
	m.pointer_texcoord[0] = particle_texcoord2f;
	m.pointer_vertex = particle_vertex3f;
	R_Mesh_State(&m);

	GL_Color(cr, cg, cb, ca);

	if (blendmode == PBLEND_ALPHA)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else if (blendmode == PBLEND_ADD)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	else //if (blendmode == PBLEND_MOD)
		GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	GL_DepthMask(false);
	GL_DepthTest(true);
	size = p->size * cl_particles_size.value;
	if (p->type->orientation == PARTICLE_BILLBOARD || p->type->orientation == PARTICLE_ORIENTED_DOUBLESIDED)
	{
		if (p->type->orientation == PARTICLE_ORIENTED_DOUBLESIDED)
		{
			// double-sided
			if (DotProduct(p->vel, r_vieworigin) > DotProduct(p->vel, org))
			{
				VectorNegate(p->vel, v);
				VectorVectors(v, right, up);
			}
			else
				VectorVectors(p->vel, right, up);
			VectorScale(right, size, right);
			VectorScale(up, size, up);
		}
		else
		{
			VectorScale(r_viewleft, -size, right);
			VectorScale(r_viewup, size, up);
		}
		particle_vertex3f[ 0] = org[0] - right[0] - up[0];
		particle_vertex3f[ 1] = org[1] - right[1] - up[1];
		particle_vertex3f[ 2] = org[2] - right[2] - up[2];
		particle_vertex3f[ 3] = org[0] - right[0] + up[0];
		particle_vertex3f[ 4] = org[1] - right[1] + up[1];
		particle_vertex3f[ 5] = org[2] - right[2] + up[2];
		particle_vertex3f[ 6] = org[0] + right[0] + up[0];
		particle_vertex3f[ 7] = org[1] + right[1] + up[1];
		particle_vertex3f[ 8] = org[2] + right[2] + up[2];
		particle_vertex3f[ 9] = org[0] + right[0] - up[0];
		particle_vertex3f[10] = org[1] + right[1] - up[1];
		particle_vertex3f[11] = org[2] + right[2] - up[2];
		particle_texcoord2f[0] = tex->s1;particle_texcoord2f[1] = tex->t2;
		particle_texcoord2f[2] = tex->s1;particle_texcoord2f[3] = tex->t1;
		particle_texcoord2f[4] = tex->s2;particle_texcoord2f[5] = tex->t1;
		particle_texcoord2f[6] = tex->s2;particle_texcoord2f[7] = tex->t2;
	}
	else if (p->type->orientation == PARTICLE_SPARK)
	{
		VectorMA(p->org, -0.02, p->vel, v);
		VectorMA(p->org, 0.02, p->vel, up2);
		R_CalcBeam_Vertex3f(particle_vertex3f, v, up2, size);
		particle_texcoord2f[0] = tex->s1;particle_texcoord2f[1] = tex->t2;
		particle_texcoord2f[2] = tex->s1;particle_texcoord2f[3] = tex->t1;
		particle_texcoord2f[4] = tex->s2;particle_texcoord2f[5] = tex->t1;
		particle_texcoord2f[6] = tex->s2;particle_texcoord2f[7] = tex->t2;
	}
	else if (p->type->orientation == PARTICLE_BEAM)
	{
		R_CalcBeam_Vertex3f(particle_vertex3f, p->org, p->vel, size);
		VectorSubtract(p->vel, p->org, up);
		VectorNormalize(up);
		v[0] = DotProduct(p->org, up) * (1.0f / 64.0f);
		v[1] = DotProduct(p->vel, up) * (1.0f / 64.0f);
		particle_texcoord2f[0] = 1;particle_texcoord2f[1] = v[0];
		particle_texcoord2f[2] = 0;particle_texcoord2f[3] = v[0];
		particle_texcoord2f[4] = 0;particle_texcoord2f[5] = v[1];
		particle_texcoord2f[6] = 1;particle_texcoord2f[7] = v[1];
	}
	else
	{
		Con_Printf("R_DrawParticles: unknown particle orientation %i\n", p->type->orientation);
		return;
	}

	R_Mesh_Draw(0, 4, 2, polygonelements);
}

void R_DrawParticles (void)
{
	int i;
	float minparticledist;
	particle_t *p;

	// LordHavoc: early out conditions
	if ((!cl.num_particles) || (!r_drawparticles.integer))
		return;

	minparticledist = DotProduct(r_vieworigin, r_viewforward) + 4.0f;

	// LordHavoc: only render if not too close
	for (i = 0, p = cl.particles;i < cl.num_particles;i++, p++)
	{
		if (p->type)
		{
			renderstats.particles++;
			if (DotProduct(p->org, r_viewforward) >= minparticledist || p->type->orientation == PARTICLE_BEAM)
			{
				if (p->type == particletype + pt_decal)
					R_DrawParticle_TransparentCallback(0, i, 0);
				else
					R_MeshQueue_AddTransparent(p->org, R_DrawParticle_TransparentCallback, NULL, i, NULL);
			}
		}
	}
}

