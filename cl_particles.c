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

#define MAX_PARTICLES			16384	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

typedef enum
{
	pt_static, pt_rain, pt_bubble, pt_blood
}
ptype_t;

#define P_TEXNUM_FIRSTBIT 0
#define P_TEXNUM_BITS 6
#define P_ORIENTATION_FIRSTBIT (P_TEXNUM_FIRSTBIT + P_TEXNUM_BITS)
#define P_ORIENTATION_BITS 2
#define P_FLAGS_FIRSTBIT (P_ORIENTATION_FIRSTBIT + P_ORIENTATION_BITS)
#define P_DYNLIGHT (1 << (P_FLAGS_FIRSTBIT + 0))
#define P_ADDITIVE (1 << (P_FLAGS_FIRSTBIT + 1))

typedef struct particle_s
{
	ptype_t		type;
	unsigned int	flags; // dynamically lit, orientation, additive blending, texnum
	vec3_t		org;
	vec3_t		vel;
	float		die;
	float		scalex;
	float		scaley;
	float		alpha; // 0-255
	float		alphafade; // how much alpha reduces per second
	float		time2; // used for various things (snow fluttering, for example)
	float		bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	float		gravity; // how much gravity affects this particle (1.0 = normal gravity, 0.0 = none)
	vec3_t		oldorg;
	vec3_t		vel2; // used for snow fluttering (base velocity, wind for instance)
	float		friction; // how much air friction affects this object (objects with a low mass/size ratio tend to get more air friction)
	float		pressure; // if non-zero, apply pressure to other particles
	qbyte		color[4];
}
particle_t;

static int particlepalette[256] =
{
	0x000000,0x0f0f0f,0x1f1f1f,0x2f2f2f,0x3f3f3f,0x4b4b4b,0x5b5b5b,0x6b6b6b,
	0x7b7b7b,0x8b8b8b,0x9b9b9b,0xababab,0xbbbbbb,0xcbcbcb,0xdbdbdb,0xebebeb,
	0x0f0b07,0x170f0b,0x1f170b,0x271b0f,0x2f2313,0x372b17,0x3f2f17,0x4b371b,
	0x533b1b,0x5b431f,0x634b1f,0x6b531f,0x73571f,0x7b5f23,0x836723,0x8f6f23,
	0x0b0b0f,0x13131b,0x1b1b27,0x272733,0x2f2f3f,0x37374b,0x3f3f57,0x474767,
	0x4f4f73,0x5b5b7f,0x63638b,0x6b6b97,0x7373a3,0x7b7baf,0x8383bb,0x8b8bcb,
	0x000000,0x070700,0x0b0b00,0x131300,0x1b1b00,0x232300,0x2b2b07,0x2f2f07,
	0x373707,0x3f3f07,0x474707,0x4b4b0b,0x53530b,0x5b5b0b,0x63630b,0x6b6b0f,
	0x070000,0x0f0000,0x170000,0x1f0000,0x270000,0x2f0000,0x370000,0x3f0000,
	0x470000,0x4f0000,0x570000,0x5f0000,0x670000,0x6f0000,0x770000,0x7f0000,
	0x131300,0x1b1b00,0x232300,0x2f2b00,0x372f00,0x433700,0x4b3b07,0x574307,
	0x5f4707,0x6b4b0b,0x77530f,0x835713,0x8b5b13,0x975f1b,0xa3631f,0xaf6723,
	0x231307,0x2f170b,0x3b1f0f,0x4b2313,0x572b17,0x632f1f,0x733723,0x7f3b2b,
	0x8f4333,0x9f4f33,0xaf632f,0xbf772f,0xcf8f2b,0xdfab27,0xefcb1f,0xfff31b,
	0x0b0700,0x1b1300,0x2b230f,0x372b13,0x47331b,0x533723,0x633f2b,0x6f4733,
	0x7f533f,0x8b5f47,0x9b6b53,0xa77b5f,0xb7876b,0xc3937b,0xd3a38b,0xe3b397,
	0xab8ba3,0x9f7f97,0x937387,0x8b677b,0x7f5b6f,0x775363,0x6b4b57,0x5f3f4b,
	0x573743,0x4b2f37,0x43272f,0x371f23,0x2b171b,0x231313,0x170b0b,0x0f0707,
	0xbb739f,0xaf6b8f,0xa35f83,0x975777,0x8b4f6b,0x7f4b5f,0x734353,0x6b3b4b,
	0x5f333f,0x532b37,0x47232b,0x3b1f23,0x2f171b,0x231313,0x170b0b,0x0f0707,
	0xdbc3bb,0xcbb3a7,0xbfa39b,0xaf978b,0xa3877b,0x977b6f,0x876f5f,0x7b6353,
	0x6b5747,0x5f4b3b,0x533f33,0x433327,0x372b1f,0x271f17,0x1b130f,0x0f0b07,
	0x6f837b,0x677b6f,0x5f7367,0x576b5f,0x4f6357,0x475b4f,0x3f5347,0x374b3f,
	0x2f4337,0x2b3b2f,0x233327,0x1f2b1f,0x172317,0x0f1b13,0x0b130b,0x070b07,
	0xfff31b,0xefdf17,0xdbcb13,0xcbb70f,0xbba70f,0xab970b,0x9b8307,0x8b7307,
	0x7b6307,0x6b5300,0x5b4700,0x4b3700,0x3b2b00,0x2b1f00,0x1b0f00,0x0b0700,
	0x0000ff,0x0b0bef,0x1313df,0x1b1bcf,0x2323bf,0x2b2baf,0x2f2f9f,0x2f2f8f,
	0x2f2f7f,0x2f2f6f,0x2f2f5f,0x2b2b4f,0x23233f,0x1b1b2f,0x13131f,0x0b0b0f,
	0x2b0000,0x3b0000,0x4b0700,0x5f0700,0x6f0f00,0x7f1707,0x931f07,0xa3270b,
	0xb7330f,0xc34b1b,0xcf632b,0xdb7f3b,0xe3974f,0xe7ab5f,0xefbf77,0xf7d38b,
	0xa77b3b,0xb79b37,0xc7c337,0xe7e357,0x7fbfff,0xabe7ff,0xd7ffff,0x670000,
	0x8b0000,0xb30000,0xd70000,0xff0000,0xfff393,0xfff7c7,0xffffff,0x9f5b53
};

//static int explosparkramp[8] = {0x4b0700, 0x6f0f00, 0x931f07, 0xb7330f, 0xcf632b, 0xe3974f, 0xffe7b5, 0xffffff};

// these must match r_part.c's textures
static const int tex_smoke[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const int tex_rainsplash[16] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
static const int tex_particle = 24;
static const int tex_rain = 25;
static const int tex_bubble = 26;

static int			cl_maxparticles;
static int			cl_numparticles;
static particle_t	*particles;
static particle_t	**freeparticles; // list used only in compacting particles array

cvar_t cl_particles = {CVAR_SAVE, "cl_particles", "1"};
cvar_t cl_particles_size = {CVAR_SAVE, "cl_particles_size", "1"};
cvar_t cl_particles_bloodshowers = {CVAR_SAVE, "cl_particles_bloodshowers", "1"};
cvar_t cl_particles_blood = {CVAR_SAVE, "cl_particles_blood", "1"};
cvar_t cl_particles_blood_size = {CVAR_SAVE, "cl_particles_blood_size", "8"};
cvar_t cl_particles_blood_alpha = {CVAR_SAVE, "cl_particles_blood_alpha", "0.5"};
cvar_t cl_particles_bulletimpacts = {CVAR_SAVE, "cl_particles_bulletimpacts", "1"};
cvar_t cl_particles_smoke = {CVAR_SAVE, "cl_particles_smoke", "1"};
cvar_t cl_particles_sparks = {CVAR_SAVE, "cl_particles_sparks", "1"};
cvar_t cl_particles_bubbles = {CVAR_SAVE, "cl_particles_bubbles", "1"};

static mempool_t *cl_part_mempool;

void CL_Particles_Clear(void)
{
	cl_numparticles = 0;
}

/*
===============
CL_InitParticles
===============
*/
void CL_ReadPointFile_f (void);
void CL_Particles_Init (void)
{
	int		i;

	i = COM_CheckParm ("-particles");

	if (i && i < com_argc - 1)
	{
		cl_maxparticles = (int)(atoi(com_argv[i+1]));
		if (cl_maxparticles < ABSOLUTE_MIN_PARTICLES)
			cl_maxparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
		cl_maxparticles = MAX_PARTICLES;

	Cmd_AddCommand ("pointfile", CL_ReadPointFile_f);

	Cvar_RegisterVariable (&cl_particles);
	Cvar_RegisterVariable (&cl_particles_size);
	Cvar_RegisterVariable (&cl_particles_bloodshowers);
	Cvar_RegisterVariable (&cl_particles_blood);
	Cvar_RegisterVariable (&cl_particles_blood_size);
	Cvar_RegisterVariable (&cl_particles_blood_alpha);
	Cvar_RegisterVariable (&cl_particles_bulletimpacts);
	Cvar_RegisterVariable (&cl_particles_smoke);
	Cvar_RegisterVariable (&cl_particles_sparks);
	Cvar_RegisterVariable (&cl_particles_bubbles);

	cl_part_mempool = Mem_AllocPool("CL_Part");
	particles = (particle_t *) Mem_Alloc(cl_part_mempool, cl_maxparticles * sizeof(particle_t));
	freeparticles = (void *) Mem_Alloc(cl_part_mempool, cl_maxparticles * sizeof(particle_t *));
	cl_numparticles = 0;
}

#define particle(ptype, porientation, pcolor1, pcolor2, ptex, plight, padditive, pscalex, pscaley, palpha, palphafade, ptime, pgravity, pbounce, px, py, pz, pvx, pvy, pvz, ptime2, pvx2, pvy2, pvz2, pfriction, ppressure)\
{\
	if (cl_numparticles >= cl_maxparticles)\
		return;\
	{\
		particle_t	*part;\
		int tempcolor, tempcolor2, cr1, cg1, cb1, cr2, cg2, cb2;\
		unsigned int partflags;\
		partflags = ((porientation) << P_ORIENTATION_FIRSTBIT) | ((ptex) << P_TEXNUM_FIRSTBIT);\
		if (padditive)\
			partflags |= P_ADDITIVE;\
		if (plight)\
			partflags |= P_DYNLIGHT;\
		tempcolor = (pcolor1);\
		tempcolor2 = (pcolor2);\
		cr2 = ((tempcolor2) >> 16) & 0xFF;\
		cg2 = ((tempcolor2) >> 8) & 0xFF;\
		cb2 = (tempcolor2) & 0xFF;\
		if (tempcolor != tempcolor2)\
		{\
			cr1 = ((tempcolor) >> 16) & 0xFF;\
			cg1 = ((tempcolor) >> 8) & 0xFF;\
			cb1 = (tempcolor) & 0xFF;\
			tempcolor = rand() & 0xFF;\
			cr2 = (((cr2 - cr1) * tempcolor) >> 8) + cr1;\
			cg2 = (((cg2 - cg1) * tempcolor) >> 8) + cg1;\
			cb2 = (((cb2 - cb1) * tempcolor) >> 8) + cb1;\
		}\
		part = &particles[cl_numparticles++];\
		part->type = (ptype);\
		part->color[0] = cr2;\
		part->color[1] = cg2;\
		part->color[2] = cb2;\
		part->color[3] = 0xFF;\
		part->flags = partflags;\
		part->scalex = (pscalex);\
		part->scaley = (pscaley);\
		part->alpha = (palpha);\
		part->alphafade = (palphafade);\
		part->die = cl.time + (ptime);\
		part->gravity = (pgravity);\
		part->bounce = (pbounce);\
		part->org[0] = (px);\
		part->org[1] = (py);\
		part->org[2] = (pz);\
		part->vel[0] = (pvx);\
		part->vel[1] = (pvy);\
		part->vel[2] = (pvz);\
		part->time2 = (ptime2);\
		part->vel2[0] = (pvx2);\
		part->vel2[1] = (pvy2);\
		part->vel2[2] = (pvz2);\
		part->friction = (pfriction);\
		part->pressure = (ppressure);\
	}\
}

/*
===============
CL_EntityParticles
===============
*/
void CL_EntityParticles (entity_t *ent)
{
	int			i;
	float		angle;
	float		sp, sy, cp, cy;
	vec3_t		forward;
	float		dist;
	float		beamlength;
	static vec3_t avelocities[NUMVERTEXNORMALS];
	if (!cl_particles.integer) return;

	dist = 64;
	beamlength = 16;

	if (!avelocities[0][0])
		for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
			avelocities[0][i] = (rand()&255) * 0.01;

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);

		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		particle(pt_static, PARTICLE_BILLBOARD, particlepalette[0x6f], particlepalette[0x6f], tex_particle, false, false, 2, 2, 255, 0, -19999, 0, 0, ent->render.origin[0] + m_bytenormals[i][0]*dist + forward[0]*beamlength, ent->render.origin[1] + m_bytenormals[i][1]*dist + forward[1]*beamlength, ent->render.origin[2] + m_bytenormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
}


void CL_ReadPointFile_f (void)
{
	vec3_t	org;
	int		r, c;
	char	*pointfile, *pointfilepos, *t, tchar;

	pointfile = COM_LoadFile(va("maps/%s.pts", sv.name), true);
	if (!pointfile)
	{
		Con_Printf ("couldn't open %s.pts\n", sv.name);
		return;
	}

	Con_Printf ("Reading %s.pts...\n", sv.name);
	c = 0;
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
		c++;

		if (cl_numparticles >= cl_maxparticles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		particle(pt_static, PARTICLE_BILLBOARD, particlepalette[(-c)&15], particlepalette[(-c)&15], tex_particle, false, false, 2, 2, 255, 0, 99999, 0, 0, org[0], org[1], org[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	Mem_Free(pointfile);
	Con_Printf ("%i points read\n", c);
}

/*
===============
CL_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void CL_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, color;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	CL_RunParticleEffect (org, dir, color, count);
}

/*
===============
CL_ParticleExplosion

===============
*/
void CL_ParticleExplosion (vec3_t org, int smoke)
{
	int i;
	if (cl_stainmaps.integer)
		R_Stain(org, 96, 80, 80, 80, 128, 176, 176, 176, 128);

	i = Mod_PointInLeaf(org, cl.worldmodel)->contents;
	if ((i == CONTENTS_SLIME || i == CONTENTS_WATER) && cl_particles.integer && cl_particles_bubbles.integer)
	{
		for (i = 0;i < 128;i++)
		{
			particle(pt_bubble, PARTICLE_BILLBOARD, 0x404040, 0x808080, tex_bubble, false, true, 2, 2, lhrandom(128, 255), 256, 9999, -0.25, 1.5, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-96, 96), lhrandom(-96, 96), lhrandom(-96, 96), 0, 0, 0, 0, (1.0 / 16.0), 0);
		}
	}

	if (cl_explosions.integer)
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

	for (i = 0;i < 512;i++)
	{
		k = particlepalette[colorStart + (i % colorLength)];
		particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 1.5, 1.5, 255, 384, 0.3, 0, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-192, 192), lhrandom(-192, 192), lhrandom(-192, 192), 0, 0, 0, 0, 1, 0);
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion (vec3_t org)
{
	if (cl_stainmaps.integer)
		R_Stain(org, 96, 80, 80, 80, 128, 176, 176, 176, 128);

	if (cl_explosions.integer)
		R_NewExplosion(org);
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
		CL_ParticleExplosion(org, false);
		return;
	}
	if (!cl_particles.integer) return;
	while (count--)
	{
		k = particlepalette[color + (rand()&7)];
		particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 1, 1, 255, 512, 9999, 0, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-15, 15), lhrandom(-15, 15), lhrandom(-15, 15), 0, 0, 0, 0, 0, 0);
	}
}

// LordHavoc: added this for spawning sparks/dust (which have strong gravity)
/*
===============
CL_SparkShower
===============
*/
void CL_SparkShower (vec3_t org, vec3_t dir, int count)
{
	int k;
	if (!cl_particles.integer) return;

	if (cl_stainmaps.integer)
		R_Stain(org, 32, 96, 96, 96, 32, 128, 128, 128, 32);

	if (cl_particles_bulletimpacts.integer)
	{
		// smoke puff
		if (cl_particles_smoke.integer)
			particle(pt_static, PARTICLE_BILLBOARD, 0x606060, 0xA0A0A0, tex_smoke[rand()&7], true, true, 4, 4, 255, 1024, 9999, -0.2, 0, org[0], org[1], org[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 16), 0, 0, 0, 0, 0, 0);

		if (cl_particles_sparks.integer)
		{
			// sparks
			while(count--)
			{
				k = particlepalette[0x68 + (rand() & 7)];
				particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 1, 1, lhrandom(64, 255), 512, 9999, 1, 0, org[0], org[1], org[2], lhrandom(-64, 64) + dir[0], lhrandom(-64, 64) + dir[1], lhrandom(0, 128) + dir[2], 0, 0, 0, 0, 1, 0);
			}
		}
	}
}

void CL_PlasmaBurn (vec3_t org)
{
	if (cl_stainmaps.integer)
		R_Stain(org, 48, 96, 96, 96, 48, 128, 128, 128, 48);
}

static float bloodcount = 0;
void CL_BloodPuff (vec3_t org, vec3_t vel, int count)
{
	float s, r, a;
	// bloodcount is used to accumulate counts too small to cause a blood particle
	if (!cl_particles.integer) return;
	if (!cl_particles_blood.integer) return;

	s = count + 32.0f;
	count *= 5.0f;
	if (count > 1000)
		count = 1000;
	bloodcount += count;
	r = cl_particles_blood_size.value;
	a = cl_particles_blood_alpha.value * 255;
	while(bloodcount > 0)
	{
		particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, r, r, a, a * 0.5, 9999, 0, -1, org[0], org[1], org[2], vel[0] + lhrandom(-s, s), vel[1] + lhrandom(-s, s), vel[2] + lhrandom(-s, s), 0, 0, 0, 0, 1, 0);
		bloodcount -= r;
	}
}

void CL_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	float r;
	float a;
	vec3_t diff, center, velscale;
	if (!cl_particles.integer) return;
	if (!cl_particles_bloodshowers.integer) return;
	if (!cl_particles_blood.integer) return;

	VectorSubtract(maxs, mins, diff);
	center[0] = (mins[0] + maxs[0]) * 0.5;
	center[1] = (mins[1] + maxs[1]) * 0.5;
	center[2] = (mins[2] + maxs[2]) * 0.5;
	// FIXME: change velspeed back to 2.0x after fixing mod
	velscale[0] = velspeed * 2.0 / diff[0];
	velscale[1] = velspeed * 2.0 / diff[1];
	velscale[2] = velspeed * 2.0 / diff[2];

	bloodcount += count * 5.0f;
	r = cl_particles_blood_size.value;
	a = cl_particles_blood_alpha.value * 255;
	while (bloodcount > 0)
	{
		vec3_t org, vel;
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		bloodcount -= r;
		particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, r, r, a, a * 0.5, 9999, 0, -1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 0, 0, 0, 0, 1, 0);
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

	while (count--)
	{
		k = particlepalette[colorbase + (rand()&3)];
		particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 2, 2, 255, 0, lhrandom(1, 2), gravity ? 1 : 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0] + lhrandom(-randomvel, randomvel), dir[1] + lhrandom(-randomvel, randomvel), dir[2] + lhrandom(-randomvel, randomvel), 0, 0, 0, 0, 0, 0);
	}
}

void CL_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type)
{
	int k;
	float t, z, minz, maxz;
	if (!cl_particles.integer) return;
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}
	if (dir[2] < 0) // falling
	{
		t = (maxs[2] - mins[2]) / -dir[2];
		z = maxs[2];
	}
	else // rising??
	{
		t = (maxs[2] - mins[2]) / dir[2];
		z = mins[2];
	}
	if (t < 0 || t > 2) // sanity check
		t = 2;

	minz = z - fabs(dir[2]) * 0.1;
	maxz = z + fabs(dir[2]) * 0.1;
	minz = bound(mins[2], minz, maxs[2]);
	maxz = bound(mins[2], maxz, maxs[2]);

	switch(type)
	{
	case 0:
		count *= 4; // ick, this should be in the mod or maps?

		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			particle(pt_rain, PARTICLE_UPRIGHT_FACING, k, k, tex_particle, true, true, 0.5, 8, lhrandom(8, 16), 0, t, 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], cl.time + 9999, dir[0], dir[1], dir[2], 0, 0);
		}
		break;
	case 1:
		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			particle(pt_rain, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 1, 1, lhrandom(64, 128), 0, t, 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, dir[0], dir[1], dir[2], 0, 0);
		}
		break;
	default:
		Host_Error("CL_ParticleRain: unknown type %i (0 = rain, 1 = snow)\n", type);
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

	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		o[0] = lhrandom(mins[0], maxs[0]);
		o[1] = lhrandom(mins[1], maxs[1]);
		o[2] = lhrandom(mins[2], maxs[2]);
		VectorSubtract(o, center, v);
		VectorNormalizeFast(v);
		VectorScale(v, 100, v);
		v[2] += sv_gravity.value * 0.15f;
		particle(pt_static, PARTICLE_BILLBOARD, 0x903010, 0xFFD030, tex_particle, false, true, 1.5, 1.5, lhrandom(64, 128), 128, 9999, 1, 0, o[0], o[1], o[2], v[0], v[1], v[2], 0, 0, 0, 0, 0, 0);
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

	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 4, 4, lhrandom(64, 128), 384, 9999, -1, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-32, 32), lhrandom(-32, 32), lhrandom(0, 64), 0, 0, 0, 0, 1, 0);
		if (count & 1)
			particle(pt_static, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], false, true, 6, 6, lhrandom(48, 96), 64, 9999, 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 32), 0, 0, 0, 0, 0, 0);
	}
}

void CL_Flames (vec3_t org, vec3_t vel, int count)
{
	int k;
	if (!cl_particles.integer) return;

	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 4, 4, lhrandom(64, 128), 384, 9999, -1, 1.1, org[0], org[1], org[2], vel[0] + lhrandom(-128, 128), vel[1] + lhrandom(-128, 128), vel[2] + lhrandom(-128, 128), 0, 0, 0, 0, 1, 0);
	}
}



/*
===============
CL_LavaSplash

===============
*/
void CL_LavaSplash (vec3_t origin)
{
	int			i, j, k;
	float		vel;
	vec3_t		dir, org;
	if (!cl_particles.integer) return;

	for (i=-128 ; i<128 ; i+=16)
	{
		for (j=-128 ; j<128 ; j+=16)
		{
			dir[0] = j + lhrandom(0, 8);
			dir[1] = i + lhrandom(0, 8);
			dir[2] = 256;
			org[0] = origin[0] + dir[0];
			org[1] = origin[1] + dir[1];
			org[2] = origin[2] + lhrandom(0, 64);
			vel = lhrandom(50, 120) / VectorLength(dir); // normalize and scale
			k = particlepalette[224 + (rand()&7)];
			particle(pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 7, 7, 255, 192, 9999, 0.05, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0, 0, 0, 0);
		}
	}
}

/*
===============
CL_TeleportSplash

===============
*/
/*
void CL_TeleportSplash (vec3_t org)
{
	int i, j, k;
	if (!cl_particles.integer) return;

	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=-24 ; k<32 ; k+=8)
				particle(pt_static, PARTICLE_BILLBOARD, 0xA0A0A0, 0xFFFFFF, tex_particle, false, true, 10, 10, lhrandom(64, 128), 256, 9999, 0, 0, org[0] + i + lhrandom(0, 8), org[1] + j + lhrandom(0, 8), org[2] + k + lhrandom(0, 8), lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-256, 256), 0, 0, 0, 0, 1, 0);
}
*/

void CL_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent)
{
	vec3_t vec, dir, vel, pos;
	float len, dec, speed, r;
	int contents, smoke, blood, bubbles;

	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	VectorSubtract (end, start, vec);
	len = VectorNormalizeLength (vec);
	dec = -ent->persistent.trail_time;
	ent->persistent.trail_time += len;
	if (ent->persistent.trail_time < 0.01f)
		return;

	speed = 1.0f / (ent->state_current.time - ent->state_previous.time);
	VectorSubtract(ent->state_current.origin, ent->state_previous.origin, vel);
	VectorScale(vel, speed, vel);

	// advance into this frame to reach the first puff location
	VectorMA(start, dec, vec, pos);
	len -= dec;

	// if we skip out, leave it reset
	ent->persistent.trail_time = 0.0f;

	contents = Mod_PointInLeaf(pos, cl.worldmodel)->contents;
	if (contents == CONTENTS_SKY || contents == CONTENTS_LAVA)
		return;

	smoke = cl_particles.integer && cl_particles_smoke.integer;
	blood = cl_particles.integer && cl_particles_blood.integer;
	bubbles = cl_particles.integer && cl_particles_bubbles.integer && (contents == CONTENTS_WATER || contents == CONTENTS_SLIME);

	while (len >= 0)
	{
		switch (type)
		{
			case 0:	// rocket trail
				dec = 3;
				if (smoke)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], false, true, dec, dec, 32, 64, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 0, 0, 0, 0, 0, 0);
					particle(pt_static, PARTICLE_BILLBOARD, 0x801010, 0xFFA020, tex_smoke[rand()&7], false, true, dec, dec, 128, 768, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-20, 20), lhrandom(-20, 20), lhrandom(-20, 20), 0, 0, 0, 0, 0, 0);
				}
				if (bubbles)
				{
					r = lhrandom(1, 2);
					particle(pt_bubble, PARTICLE_BILLBOARD, 0x404040, 0x808080, tex_bubble, false, true, r, r, lhrandom(64, 255), 256, 9999, -0.25, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, (1.0 / 16.0), 0);
				}
				break;

			case 1: // grenade trail
				// FIXME: make it gradually stop smoking
				dec = 3;
				if (cl_particles.integer && cl_particles_smoke.integer)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], false, true, dec, dec, 32, 96, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 0, 0, 0, 0, 0, 0);
				}
				break;


			case 2:	// blood
			case 4:	// slight blood
				dec = cl_particles_blood_size.value;
				if (blood)
				{
					particle(pt_blood, PARTICLE_BILLBOARD, 0x100000, 0x280000, tex_smoke[rand()&7], true, false, dec, dec, cl_particles_blood_alpha.value * 255.0f, cl_particles_blood_alpha.value * 255.0f * 0.5, 9999, 0, -1, pos[0], pos[1], pos[2], vel[0] * 0.5f + lhrandom(-64, 64), vel[1] * 0.5f + lhrandom(-64, 64), vel[2] * 0.5f + lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				}
				break;

			case 3:	// green tracer
				dec = 6;
				if (smoke)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x002000, 0x003000, tex_particle, false, true, dec, dec, 128, 384, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0, 0, 0, 0, 0, 0);
				}
				break;

			case 5:	// flame tracer
				dec = 6;
				if (smoke)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x301000, 0x502000, tex_particle, false, true, dec, dec, 128, 384, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0, 0, 0, 0, 0, 0);
				}
				break;

			case 6:	// voor trail
				dec = 6;
				if (smoke)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x502030, 0x502030, tex_particle, false, true, dec, dec, 128, 384, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0, 0, 0, 0, 0, 0);
				}
				break;

			case 7:	// Nehahra smoke tracer
				dec = 7;
				if (smoke)
				{
					particle(pt_static, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], true, false, dec, dec, 64, 320, 9999, 0, 0, pos[0], pos[1], pos[2], lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(0, 16), 0, 0, 0, 0, 0, 0);
				}
				break;
		}

		// advance to next time and position
		len -= dec;
		VectorMA (pos, dec, vec, pos);
	}
	ent->persistent.trail_time = len;
}

void CL_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent)
{
	vec3_t vec, pos;
	int len;
	if (!cl_particles.integer) return;
	if (!cl_particles_smoke.integer) return;

	VectorCopy(start, pos);
	VectorSubtract (end, start, vec);
	len = (int) (VectorNormalizeLength (vec) * (1.0f / 3.0f));
	VectorScale(vec, 3, vec);
	color = particlepalette[color];
	while (len--)
	{
		particle(pt_static, PARTICLE_BILLBOARD, color, color, tex_particle, false, false, 5, 5, 128, 320, 9999, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
		VectorAdd (pos, vec, pos);
	}
}


/*
===============
CL_MoveParticles
===============
*/
void CL_MoveParticles (void)
{
	particle_t *p;
	int i, activeparticles, maxparticle, j, a, pressureused = false, content;
	float gravity, dvel, bloodwaterfade, frametime, f, dist, normal[3], v[3], org[3];

	// LordHavoc: early out condition
	if (!cl_numparticles)
		return;

	frametime = cl.time - cl.oldtime;
	if (!frametime)
		return; // if absolutely still, don't update particles
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;
	bloodwaterfade = max(cl_particles_blood_alpha.value, 0.01f) * frametime * 128.0f;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	for (i = 0, p = particles;i < cl_numparticles;i++, p++)
	{
		content = 0;
		VectorCopy(p->org, p->oldorg);
		VectorMA(p->org, frametime, p->vel, p->org);
		VectorCopy(p->org, org);
		if (p->bounce)
		{
			if (CL_TraceLine(p->oldorg, p->org, v, normal, 0, true) < 1)
			{
				VectorCopy(v, p->org);
				if (p->bounce < 0)
				{
					// assume it's blood (lame, but...)
					if (cl_stainmaps.integer)
						R_Stain(v, 64, 32, 16, 16, p->alpha * p->scalex * (1.0f / 100.0f), 192, 48, 48, p->alpha * p->scalex * (1.0f / 100.0f));
					p->die = -1;
					freeparticles[j++] = p;
					continue;
				}
				else
				{
					dist = DotProduct(p->vel, normal) * -p->bounce;
					VectorMA(p->vel, dist, normal, p->vel);
					if (DotProduct(p->vel, p->vel) < 0.03)
						VectorClear(p->vel);
				}
			}
		}
		p->vel[2] -= p->gravity * gravity;
		if (p->friction)
		{
			f = p->friction * frametime;
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (content != CONTENTS_EMPTY)
				f *= 4;
			f = 1.0f - f;
			VectorScale(p->vel, f, p->vel);
		}

		switch (p->type)
		{
		case pt_static:
			break;

		case pt_blood:
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			a = content;
			if (a != CONTENTS_EMPTY)
			{
				if (a == CONTENTS_WATER || a == CONTENTS_SLIME)
				{
					p->scalex += frametime * cl_particles_blood_size.value;
					p->scaley += frametime * cl_particles_blood_size.value;
					//p->alpha -= bloodwaterfade;
				}
				else
					p->die = -1;
			}
			else
				p->vel[2] -= gravity;
			break;
		case pt_bubble:
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (content != CONTENTS_WATER && content != CONTENTS_SLIME)
			{
				p->die = -1;
				break;
			}
			break;
		case pt_rain:
			if (cl.time > p->time2)
			{
				// snow flutter
				p->time2 = cl.time + (rand() & 3) * 0.1;
				p->vel[0] = lhrandom(-32, 32) + p->vel2[0];
				p->vel[1] = lhrandom(-32, 32) + p->vel2[1];
				p->vel[2] = /*lhrandom(-32, 32) +*/ p->vel2[2];
			}
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			a = content;
			if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
				p->die = -1;
			break;
		default:
			printf("unknown particle type %i\n", p->type);
			p->die = -1;
			break;
		}
		p->alpha -= p->alphafade * frametime;

		// remove dead particles
		if (p->alpha < 1 || p->die < cl.time)
			freeparticles[j++] = p;
		else
		{
			maxparticle = i;
			activeparticles++;
			if (p->pressure)
				pressureused = true;
		}
	}
	// fill in gaps to compact the array
	i = 0;
	while (maxparticle >= activeparticles)
	{
		*freeparticles[i++] = particles[maxparticle--];
		while (maxparticle >= activeparticles && particles[maxparticle].die < cl.time)
			maxparticle--;
	}
	cl_numparticles = activeparticles;

	if (pressureused)
	{
		activeparticles = 0;
		for (i = 0, p = particles;i < cl_numparticles;i++, p++)
			if (p->pressure)
				freeparticles[activeparticles++] = p;

		if (activeparticles)
		{
			for (i = 0, p = particles;i < cl_numparticles;i++, p++)
			{
				for (j = 0;j < activeparticles;j++)
				{
					if (freeparticles[j] != p)
					{
						float dist, diff[3];
						VectorSubtract(p->org, freeparticles[j]->org, diff);
						dist = DotProduct(diff, diff);
						if (dist < 4096 && dist >= 1)
						{
							dist = freeparticles[j]->scalex * 4.0f * frametime / sqrt(dist);
							VectorMA(p->vel, dist, diff, p->vel);
						}
					}
				}
			}
		}
	}
}

static rtexturepool_t *particletexturepool;

static rtexture_t *particlefonttexture;
// [0] is normal, [1] is fog, they may be the same
static particletexture_t particletexture[MAX_PARTICLETEXTURES][2];

static cvar_t r_drawparticles = {0, "r_drawparticles", "1"};
static cvar_t r_particles_lighting = {0, "r_particles_lighting", "0"};

static qbyte shadebubble(float dx, float dy, vec3_t light)
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
		return (qbyte) f;
	}
	else
		return 0;
}

static void setuptex(int cltexnum, int fog, int rtexnum, qbyte *data, qbyte *particletexturedata)
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
	qbyte	data[32][32][4], noise1[64][64], noise2[64][64];
	vec3_t	light;
	qbyte	particletexturedata[256*256*4];

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
					data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
					dx = x - 16;
					d = (noise2[y][x] - 128) * 3 + 192;
					if (d > 0)
				 		d = (d * (256 - (int) (dx*dx+dy*dy))) >> 8;
					d = (d * noise1[y][x]) >> 7;
					d = bound(0, d, 255);
					data[y][x][3] = (qbyte) d;
					if (m < d)
						m = d;
				}
			}
		}
		while (m < 224);

		setuptex(i + 0, 0, i + 0, &data[0][0][0], particletexturedata);
		setuptex(i + 0, 1, i + 0, &data[0][0][0], particletexturedata);
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
			data[y][x][3] = (qbyte) d;
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
	int i, lighting, dynlight, additive, texnum, orientation;
	float minparticledist, org[3], uprightangles[3], up2[3], right2[3], v[3], right[3], up[3], tvxyz[4][4], tvst[4][2], fog, ifog, fogvec[3];
	mleaf_t *leaf;
	particletexture_t *tex, *texfog;
	rmeshinfo_t m;
	particle_t *p;

	// LordHavoc: early out conditions
	if ((!cl_numparticles) || (!r_drawparticles.integer))
		return;

	lighting = r_particles_lighting.integer;
	if (!r_dynamic.integer)
		lighting = 0;

	c_particles += cl_numparticles;

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
	m.numtriangles = 2;
	m.index = partindexarray;
	m.numverts = 4;
	m.vertex = &tvxyz[0][0];
	m.vertexstep = sizeof(float[4]);
	m.tex[0] = R_GetTexture(particlefonttexture);
	m.texcoords[0] = &tvst[0][0];
	m.texcoordstep[0] = sizeof(float[2]);

	for (i = 0, p = particles;i < cl_numparticles;i++, p++)
	{
		// LordHavoc: only render if not too close
		if (DotProduct(p->org, vpn) < minparticledist)
			continue;

		// LordHavoc: check if it's in a visible leaf
		leaf = Mod_PointInLeaf(p->org, cl.worldmodel);
		if (leaf->visframe != r_framecount)
			continue;

		VectorCopy(p->org, org);
		orientation = (p->flags >> P_ORIENTATION_FIRSTBIT) & ((1 << P_ORIENTATION_BITS) - 1);
		texnum = (p->flags >> P_TEXNUM_FIRSTBIT) & ((1 << P_TEXNUM_BITS) - 1);
		dynlight = p->flags & P_DYNLIGHT;
		additive = p->flags & P_ADDITIVE;
		if (orientation == PARTICLE_BILLBOARD)
		{
			VectorScale(vright, p->scalex, right);
			VectorScale(vup, p->scaley, up);
		}
		else if (orientation == PARTICLE_UPRIGHT_FACING)
		{
			VectorScale(right2, p->scalex, right);
			VectorScale(up2, p->scaley, up);
		}
		else if (orientation == PARTICLE_ORIENTED_DOUBLESIDED)
		{
			// double-sided
			if (DotProduct(p->vel2, r_origin) > DotProduct(p->vel2, org))
			{
				VectorNegate(p->vel2, v);
				VectorVectors(v, right, up);
			}
			else
				VectorVectors(p->vel2, right, up);
			VectorScale(right, p->scalex, right);
			VectorScale(up, p->scaley, up);
		}
		else
			Host_Error("R_DrawParticles: unknown particle orientation %i\n", orientation);

		m.cr = p->color[0] * (1.0f / 255.0f);
		m.cg = p->color[1] * (1.0f / 255.0f);
		m.cb = p->color[2] * (1.0f / 255.0f);
		m.ca = p->alpha * (1.0f / 255.0f);
		if (lighting >= 1 && (dynlight || lighting >= 2))
		{
			R_CompleteLightPoint(v, org, true, leaf);
			m.cr *= v[0];
			m.cg *= v[1];
			m.cb *= v[2];
		}

		tex = &particletexture[texnum][0];

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

		if (additive)
		{
			m.blendfunc2 = GL_ONE;
			fog = 0;
			if (fogenabled)
			{
				texfog = &particletexture[texnum][1];
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
						R_Mesh_Draw(&m);
					}
				}
				else
					R_Mesh_Draw(&m);
			}
			else
				R_Mesh_Draw(&m);
		}
		else
		{
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
			fog = 0;
			if (fogenabled)
			{
				texfog = &particletexture[texnum][1];
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
						tvst[0][0] = texfog->s1;
						tvst[0][1] = texfog->t1;
						tvst[1][0] = texfog->s1;
						tvst[1][1] = texfog->t2;
						tvst[2][0] = texfog->s2;
						tvst[2][1] = texfog->t2;
						tvst[3][0] = texfog->s2;
						tvst[3][1] = texfog->t1;
						R_Mesh_Draw(&m);
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
							R_Mesh_Draw(&m);
						}
						else
						{
							// render the first pass (alpha), then do additive fog
							R_Mesh_Draw(&m);

							m.blendfunc2 = GL_ONE;
							m.cr = fogcolor[0] * fog;
							m.cg = fogcolor[1] * fog;
							m.cb = fogcolor[2] * fog;
							tvst[0][0] = texfog->s1;
							tvst[0][1] = texfog->t1;
							tvst[1][0] = texfog->s1;
							tvst[1][1] = texfog->t2;
							tvst[2][0] = texfog->s2;
							tvst[2][1] = texfog->t2;
							tvst[3][0] = texfog->s2;
							tvst[3][1] = texfog->t1;
							R_Mesh_Draw(&m);
						}
					}
				}
				else
					R_Mesh_Draw(&m);
			}
			else
				R_Mesh_Draw(&m);
		}
	}
}

