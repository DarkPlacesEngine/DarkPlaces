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
	pt_static, pt_grav, pt_blob, pt_blob2, pt_bulletsmoke, pt_smoke, pt_snow, pt_rain, pt_spark, pt_bubble, pt_fade, pt_steam, pt_splash, pt_splashpuff, pt_flame, pt_blood, pt_oneframe, pt_lavasplash, pt_raindropsplash, pt_underwaterspark, pt_explosionsplash, pt_stardust
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
	float		time2; // used for various things (snow fluttering, for example)
	float		bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
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

static int explosparkramp[8] = {0x4b0700, 0x6f0f00, 0x931f07, 0xb7330f, 0xcf632b, 0xe3974f, 0xffe7b5, 0xffffff};
//static int explounderwatersparkramp[8] = {0x00074b, 0x000f6f, 0x071f93, 0x0f33b7, 0x2b63cf, 0x4f97e3, 0xb5e7ff, 0xffffff};

// these must match r_part.c's textures
static const int tex_smoke[8] = {0, 1, 2, 3, 4, 5, 6, 7};
static const int tex_rainsplash[16] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
static const int tex_particle = 24;
static const int tex_rain = 25;
static const int tex_bubble = 26;
//static const int tex_rocketglow = 27;

static int			cl_maxparticles;
static int			cl_numparticles;
static particle_t	*particles;
static particle_t	**freeparticles; // list used only in compacting particles array
//static renderparticle_t	*cl_renderparticles;

static cvar_t cl_particles = {CVAR_SAVE, "cl_particles", "1"};
static cvar_t cl_particles_size = {CVAR_SAVE, "cl_particles_size", "1"};
static cvar_t cl_particles_bloodshowers = {CVAR_SAVE, "cl_particles_bloodshowers", "1"};
static cvar_t cl_particles_blood = {CVAR_SAVE, "cl_particles_blood", "1"};
static cvar_t cl_particles_blood_size_min = {CVAR_SAVE, "cl_particles_blood_size_min", "5"};
static cvar_t cl_particles_blood_size_max = {CVAR_SAVE, "cl_particles_blood_size_max", "10"};
static cvar_t cl_particles_blood_alpha = {CVAR_SAVE, "cl_particles_blood_alpha", "0.5"};
static cvar_t cl_particles_smoke = {CVAR_SAVE, "cl_particles_smoke", "1"};
static cvar_t cl_particles_sparks = {CVAR_SAVE, "cl_particles_sparks", "1"};
static cvar_t cl_particles_bubbles = {CVAR_SAVE, "cl_particles_bubbles", "1"};
static cvar_t cl_particles_explosions = {CVAR_SAVE, "cl_particles_explosions", "0"};

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

	if (i)
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
	Cvar_RegisterVariable (&cl_particles_blood_size_min);
	Cvar_RegisterVariable (&cl_particles_blood_size_max);
	Cvar_RegisterVariable (&cl_particles_blood_alpha);
	Cvar_RegisterVariable (&cl_particles_smoke);
	Cvar_RegisterVariable (&cl_particles_sparks);
	Cvar_RegisterVariable (&cl_particles_bubbles);
	Cvar_RegisterVariable (&cl_particles_explosions);

	cl_part_mempool = Mem_AllocPool("CL_Part");
	particles = (particle_t *) Mem_Alloc(cl_part_mempool, cl_maxparticles * sizeof(particle_t));
	freeparticles = (void *) Mem_Alloc(cl_part_mempool, cl_maxparticles * sizeof(particle_t *));
	cl_numparticles = 0;

	// FIXME: r_refdef stuff should be allocated somewhere else?
	//r_refdef.particles = cl_renderparticles = Mem_Alloc(cl_refdef_mempool, cl_maxparticles * sizeof(renderparticle_t));
}

#define particle(ptype, porientation, pcolor1, pcolor2, ptex, plight, padditive, pscalex, pscaley, palpha, ptime, pbounce, px, py, pz, pvx, pvy, pvz, ptime2, pvx2, pvy2, pvz2, pfriction, ppressure)\
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
		/*part->tex = (ptex);*/\
		/*part->orientation = (porientation);*/\
		/*part->dynlight = (plight);*/\
		/*part->additive = (padditive);*/\
		part->scalex = (pscalex);\
		part->scaley = (pscaley);\
		part->alpha = (palpha);\
		part->die = cl.time + (ptime);\
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

		particle(pt_oneframe, PARTICLE_BILLBOARD, particlepalette[0x6f], particlepalette[0x6f], tex_particle, false, false, 2, 2, 255, 9999, 0, ent->render.origin[0] + m_bytenormals[i][0]*dist + forward[0]*beamlength, ent->render.origin[1] + m_bytenormals[i][1]*dist + forward[1]*beamlength, ent->render.origin[2] + m_bytenormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
		particle(pt_static, PARTICLE_BILLBOARD, particlepalette[(-c)&15], particlepalette[(-c)&15], tex_particle, false, false, 2, 2, 255, 99999, 0, org[0], org[1], org[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
	R_Stain(org, 96, 80, 80, 80, 128, 176, 176, 176, 128);

	i = Mod_PointInLeaf(org, cl.worldmodel)->contents;
	if (i == CONTENTS_SLIME || i == CONTENTS_WATER)
		for (i = 0;i < 128;i++)
			particle(pt_bubble, PARTICLE_BILLBOARD, 0x808080, 0xFFFFFF, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-96, 96), lhrandom(-96, 96), lhrandom(-96, 96), 0, 0, 0, 0, 0, 0);

	if (cl_particles.integer && cl_particles_explosions.integer)
	{
		int j, k;
		float f;
		vec3_t v, end, ang;
		qbyte noise1[32*32], noise2[32*32];

		VectorClear(end); // hush MSVC
		i = Mod_PointInLeaf(org, cl.worldmodel)->contents;
		if (i == CONTENTS_SLIME || i == CONTENTS_WATER)
		{
			//for (i = 0;i < 128;i++)
			//	particle(pt_bubble, PARTICLE_BILLBOARD, 0x808080, 0xFFFFFF, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-96, 96), lhrandom(-96, 96), lhrandom(-96, 96), 0, 0, 0, 0, 0, 0);

			ang[2] = lhrandom(0, 360);
			fractalnoisequick(noise1, 32, 4);
			fractalnoisequick(noise2, 32, 8);
			for (i = 0;i < 32;i++)
			{
				for (j = 0;j < 32;j++)
				{
					VectorRandom(v);
					VectorMA(org, 16, v, v);
					CL_TraceLine(org, v, end, NULL, 0, true);
					ang[0] = (j + 0.5f) * (360.0f / 32.0f);
					ang[1] = (i + 0.5f) * (360.0f / 32.0f);
					AngleVectors(ang, v, NULL, NULL);
					f = noise1[j*32+i] * 1.5f;
					VectorScale(v, f, v);
					k = noise2[j*32+i] * 0x010101;
					particle(pt_underwaterspark, PARTICLE_BILLBOARD, k, k, tex_smoke[rand()&7], false, true, 10, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2], 512.0f, 0, 0, 0, 2, 0);
					VectorScale(v, 0.75, v);
					k = explosparkramp[(noise2[j*32+i] >> 5)];
					particle(pt_underwaterspark, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 10, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2], 512.0f, 0, 0, 0, 2, 0);
				}
			}
		}
		else
		{
			ang[2] = lhrandom(0, 360);
			fractalnoisequick(noise1, 32, 4);
			fractalnoisequick(noise2, 32, 8);
			for (i = 0;i < 32;i++)
			{
				for (j = 0;j < 32;j++)
				{
					VectorRandom(v);
					VectorMA(org, 16, v, v);
					CL_TraceLine(org, v, end, NULL, 0, true);
					ang[0] = (j + 0.5f) * (360.0f / 32.0f);
					ang[1] = (i + 0.5f) * (360.0f / 32.0f);
					AngleVectors(ang, v, NULL, NULL);
					f = noise1[j*32+i] * 1.5f;
					VectorScale(v, f, v);
					k = noise2[j*32+i] * 0x010101;
					particle(pt_spark, PARTICLE_BILLBOARD, k, k, tex_smoke[rand()&7], false, true, 10, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
					VectorScale(v, 0.75, v);
					k = explosparkramp[(noise2[j*32+i] >> 5)];
					particle(pt_spark, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 10, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
				//	VectorRandom(v);
				//	VectorScale(v, 384, v);
				//	particle(pt_spark, PARTICLE_BILLBOARD, explosparkramp[rand()&7], tex_particle, false, true, 2, 2, lhrandom(16, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
				}
			}
		}
	}
	else
	{
		/*
		int i;
		vec3_t v;
		for (i = 0;i < 256;i++)
		{
			do
			{
				VectorRandom(v);
			}
			while(DotProduct(v,v) < 0.75);
			VectorScale(v, 512, v);
			k = explosparkramp[rand()&7];
			particle(pt_spark, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 4, 4, 255, 9999, 1.5, org[0], org[1], org[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
		}
		*/
		R_NewExplosion(org);
	}
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
		particle(pt_fade, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 1.5, 1.5, 255, 0.3, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-192, 192), lhrandom(-192, 192), lhrandom(-192, 192), 384, 0, 0, 0, 1, 0);
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion (vec3_t org)
{
	//int i;
	if (!cl_particles.integer) return;

	R_Stain(org, 96, 80, 80, 80, 128, 176, 176, 176, 128);
	//R_Stain(org, 96, 96, 64, 96, 128, 160, 128, 160, 128);

	R_NewExplosion(org);

	//for (i = 0;i < 256;i++)
	//	particle(pt_blob , PARTICLE_BILLBOARD, particlepalette[ 66+(rand()%6)], tex_particle, false, true, 4, 4, 255, 9999, 0, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(-128, 128), 0, 0, 0, 0, 0, 0);
	//for (i = 0;i < 256;i++)
	//	particle(pt_blob2, PARTICLE_BILLBOARD, particlepalette[150+(rand()%6)], tex_particle, false, true, 4, 4, 255, 9999, 0, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(-128, 128), 0, 0, 0, 0, 0, 0);
}

/*
===============
CL_RunParticleEffect

===============
*/
void CL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int k;
	if (!cl_particles.integer) return;

	if (count == 1024)
	{
		CL_ParticleExplosion(org, false);
		return;
	}
	while (count--)
	{
		k = particlepalette[color + (rand()&7)];
		particle(pt_fade, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 1, 1, 128, 9999, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-15, 15), lhrandom(-15, 15), lhrandom(-15, 15), 384, 0, 0, 0, 0, 0);
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

	R_Stain(org, 32, 96, 96, 96, 32, 128, 128, 128, 32);

	// smoke puff
	if (cl_particles_smoke.integer)
		particle(pt_bulletsmoke, PARTICLE_BILLBOARD, 0xA0A0A0, 0xFFFFFF, tex_smoke[rand()&7], true, true, 2, 2, 255, 9999, 0, org[0], org[1], org[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 16), 0, 0, 0, 0, 0, 0);

	if (cl_particles_sparks.integer)
	{
		// sparks
		while(count--)
		{
			k = particlepalette[0x68 + (rand() & 7)];
			particle(pt_spark, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 1, 1, lhrandom(64, 128), 9999, 0, org[0], org[1], org[2], lhrandom(-64, 64) + dir[0], lhrandom(-64, 64) + dir[1], lhrandom(0, 128) + dir[2], 480, 0, 0, 0, 1, 0);
		}
	}
}

void CL_PlasmaBurn (vec3_t org)
{
	if (!cl_particles.integer) return;

	R_Stain(org, 48, 96, 96, 96, 48, 128, 128, 128, 48);
}

void CL_BloodPuff (vec3_t org, vec3_t vel, int count)
{
	float r, s;
	// bloodcount is used to accumulate counts too small to cause a blood particle
	static int bloodcount = 0;
	if (!cl_particles.integer) return;
	if (!cl_particles_blood.integer) return;

	s = count + 32.0f;
	count *= 5.0f;
	if (count > 1000)
		count = 1000;
	bloodcount += count;
	while(bloodcount > 0)
	{
		r = lhrandom(cl_particles_blood_size_min.value, cl_particles_blood_size_max.value);
		particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, r, r, cl_particles_blood_alpha.value * 255, 9999, -1, org[0], org[1], org[2], vel[0] + lhrandom(-s, s), vel[1] + lhrandom(-s, s), vel[2] + lhrandom(-s, s), 0, 0, 0, 0, 1, 0);
		bloodcount -= r;
	}
}

void CL_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	float c;
	float r;
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

	c = count * 5;
	while (c > 0)
	{
		vec3_t org, vel;
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		r = lhrandom(cl_particles_blood_size_min.value, cl_particles_blood_size_max.value);
		c -= r;
		particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, r, r, cl_particles_blood_alpha.value * 255, 9999, -1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 0, 0, 0, 0, 1, 0);
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
		particle(gravity ? pt_grav : pt_static, PARTICLE_BILLBOARD, k, k, tex_particle, false, false, 2, 2, 255, lhrandom(1, 2), 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0] + lhrandom(-randomvel, randomvel), dir[1] + lhrandom(-randomvel, randomvel), dir[2] + lhrandom(-randomvel, randomvel), 0, 0, 0, 0, 0, 0);
	}
}

void CL_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type)
{
	int k;
	vec3_t vel;
	float t, z;
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

	switch(type)
	{
	case 0:
		count *= 4; // ick, this should be in the mod or maps?

		while(count--)
		{
			vel[0] = dir[0] + lhrandom(-16, 16);
			vel[1] = dir[1] + lhrandom(-16, 16);
			vel[2] = dir[2] + lhrandom(-32, 32);
			k = particlepalette[colorbase + (rand()&3)];
			particle(pt_rain, PARTICLE_UPRIGHT_FACING, k, k, tex_particle, true, true, 1, 64, 64, t, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), z, vel[0], vel[1], vel[2], 0, vel[0], vel[1], vel[2], 0, 0);
		}
		break;
	case 1:
		while(count--)
		{
			vel[0] = dir[0] + lhrandom(-16, 16);
			vel[1] = dir[1] + lhrandom(-16, 16);
			vel[2] = dir[2] + lhrandom(-32, 32);
			k = particlepalette[colorbase + (rand()&3)];
			particle(pt_snow, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 2, 2, 255, t, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), z, vel[0], vel[1], vel[2], 0, vel[0], vel[1], vel[2], 0, 0);
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
	//Con_Printf("CL_Stardust ('%f %f %f', '%f %f %f', %d);\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], count);
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
		particle(pt_stardust, PARTICLE_BILLBOARD, 0x903010, 0xFFD030, tex_particle, false, true, 1.5, 1.5, lhrandom(64, 128), 9999, 0, o[0], o[1], o[2], v[0], v[1], v[2], 0, 0, 0, 0, 0, 0);
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
		particle(pt_flame, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 4, 4, lhrandom(64, 128), 9999, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-32, 32), lhrandom(-32, 32), lhrandom(0, 64), 0, 0, 0, 0, 1, 0);
		if (count & 1)
			particle(pt_fade, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], false, true, 6, 6, lhrandom(48, 96), 9999, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 32), 64.0f, 0, 0, 0, 0, 0);
	}
}

void CL_Flames (vec3_t org, vec3_t vel, int count)
{
	int k;
	if (!cl_particles.integer) return;

	while (count--)
	{
		k = particlepalette[224 + (rand()&15)];
		particle(pt_flame, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 4, 4, lhrandom(64, 128), 9999, 1.1, org[0], org[1], org[2], vel[0] + lhrandom(-128, 128), vel[1] + lhrandom(-128, 128), vel[2] + lhrandom(-128, 128), 0, 0, 0, 0, 1, 0);
		//if (count & 1)
		//	particle(pt_fade, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], false, true, 6, 6, lhrandom(32, 64), 9999, 0, org[0], org[1], org[2], vel[0] + lhrandom(-64, 64), vel[1] + lhrandom(-64, 64), vel[2] + lhrandom(-64, 64), 64.0f, 0, 0, 0, 0, 0);
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
			particle(pt_lavasplash, PARTICLE_BILLBOARD, k, k, tex_particle, false, true, 7, 7, 255, 9999, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0, 0, 0, 0);
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
	int			i, j, k;
	if (!cl_particles.integer) return;

	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=-24 ; k<32 ; k+=8)
				//particle(pt_fade, PARTICLE_BILLBOARD, 0xA0A0A0, 0xFFFFFF, tex_particle, false, true, 1.5, 1.5, lhrandom(64, 128), 9999, 0, org[0] + i + lhrandom(0, 8), org[1] + j + lhrandom(0, 8), org[2] + k + lhrandom(0, 8), i*2 + lhrandom(-12.5, 12.5), j*2 + lhrandom(-12.5, 12.5), k*2 + lhrandom(27.5, 52.5), 384.0f, 0, 0, 0, 1, 0);
				particle(pt_fade, PARTICLE_BILLBOARD, 0xA0A0A0, 0xFFFFFF, tex_particle, false, true, 10, 10, lhrandom(64, 128), 9999, 0, org[0] + i + lhrandom(0, 8), org[1] + j + lhrandom(0, 8), org[2] + k + lhrandom(0, 8), lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-256, 256), 256.0f, 0, 0, 0, 1, 0);
}

void CL_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent)
{
	vec3_t		vec, dir, vel, pos;
	float		len, dec, speed;
	int			contents, bubbles;
	if (!cl_particles.integer) return;

	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	//if (type == 0 && host_frametime != 0) // rocket glow
	//	particle(pt_oneframe, PARTICLE_BILLBOARD, 0xFFFFFF, 0xFFFFFF, tex_rocketglow, false, true, 24, 24, 255, 9999, 0, end[0] - 12 * dir[0], end[1] - 12 * dir[1], end[2] - 12 * dir[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);

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

	bubbles = (contents == CONTENTS_WATER || contents == CONTENTS_SLIME);

	while (len >= 0)
	{
		switch (type)
		{
			case 0:	// rocket trail
				if (!cl_particles_smoke.integer)
					return;
				//dec = 5;
				//particle(pt_fade, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_particle, true, false, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 256.0f, 0, 0, 0, 0, 0);
				dec = 6;
				particle(pt_fade, PARTICLE_BILLBOARD, 0x303030, 0x606060, tex_smoke[rand()&7], true, false, dec, dec, 64, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 64.0f, 0, 0, 0, 0, 0);
				//particle(pt_fade, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], false, true, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-10, 10), lhrandom(-10, 10), lhrandom(-10, 10), 128.0f, 0, 0, 0, 0, 0);
				//dec = 10;
				//particle(pt_smoke, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], false, true, 2, 2, 160, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				if (bubbles && cl_particles_bubbles.integer)
				{
					particle(pt_bubble, PARTICLE_BILLBOARD, 0x404040, 0x808080, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					//particle(pt_bubble, PARTICLE_BILLBOARD, 0x404040, 0x808080, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
				}
				else
				{
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 2, 2, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 512.0f, 0, 0, 0, 1, 0);
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 2, 2, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 512.0f, 0, 0, 0, 1, 0);
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 1, 1, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 1, 0);
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 1, 1, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 1, 0);
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 1, 1, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 1, 0);
					//particle(pt_spark, PARTICLE_BILLBOARD, particlepalette[0x68 + (rand() & 7)], tex_particle, false, true, 1, 1, lhrandom(128, 255), 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 1, 0);
				}
				break;

			case 1: // grenade trail
				// FIXME: make it gradually stop smoking
				if (!cl_particles_smoke.integer)
					return;
				//dec = 5;
				//particle(pt_fade, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_particle, true, false, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 256.0f, 0, 0, 0, 0, 0);
				dec = 6;
				particle(pt_fade, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], true, false, dec, dec, 64, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 128.0f, 0, 0, 0, 0, 0);
				//particle(pt_smoke, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], false, true, 2, 2, 160, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				if (bubbles && cl_particles_bubbles.integer)
				{
					particle(pt_bubble, PARTICLE_BILLBOARD, 0x404040, 0x808080, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					//particle(pt_bubble, PARTICLE_BILLBOARD, c * 2, tex_bubble, false, true, 2, 2, 255, 9999, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
				}
				break;


			case 2:	// blood
				if (!cl_particles_blood.integer)
					return;
				dec = lhrandom(cl_particles_blood_size_min.value, cl_particles_blood_size_max.value);
				particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, dec, dec, cl_particles_blood_alpha.value * 255.0f, 9999, -1, pos[0], pos[1], pos[2], vel[0] * 0.5f + lhrandom(-64, 64), vel[1] * 0.5f + lhrandom(-64, 64), vel[2] * 0.5f + lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				//particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, dec, dec, cl_particles_blood_alpha.value * 255.0f, 9999, -1, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				//particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_particle, true, false, dec, dec, cl_particles_blood_alpha.value * 255.0f, 9999, -1, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				break;

			case 4:	// slight blood
				if (!cl_particles_blood.integer)
					return;
				dec = lhrandom(cl_particles_blood_size_min.value, cl_particles_blood_size_max.value);
				particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, dec, dec, cl_particles_blood_alpha.value * 255.0f, 9999, -1, pos[0], pos[1], pos[2], vel[0] * 0.5f + lhrandom(-64, 64), vel[1] * 0.5f + lhrandom(-64, 64), vel[2] * 0.5f + lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				//particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_smoke[rand()&7], true, false, dec, dec, cl_particles_blood_alpha.value * 128.0f, 9999, -1, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				//particle(pt_blood, PARTICLE_BILLBOARD, 0x000000, 0x200000, tex_particle, true, false, dec, dec, cl_particles_blood_alpha.value * 128.0f, 9999, -1, pos[0], pos[1], pos[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-64, 64), 0, 0, 0, 0, 1, 0);
				break;

			case 3:	// green tracer
				dec = 6;
				//particle(pt_fade, PARTICLE_BILLBOARD, 0x373707, 0x373707, tex_particle, false, true, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 384.0f, 0, 0, 0, 0, 0);
				particle(pt_fade, PARTICLE_BILLBOARD, 0x373707, 0x373707, tex_particle, false, false, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 384.0f, 0, 0, 0, 0, 0);
				break;

			case 5:	// flame tracer
				dec = 6;
				//particle(pt_fade, PARTICLE_BILLBOARD, 0xCF632B, 0xCF632B, tex_particle, false, true, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 384.0f, 0, 0, 0, 0, 0);
				particle(pt_fade, PARTICLE_BILLBOARD, 0xCF632B, 0xCF632B, tex_particle, false, false, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 384.0f, 0, 0, 0, 0, 0);
				break;

			case 6:	// voor trail
				dec = 6;
				//particle(pt_fade, PARTICLE_BILLBOARD, 0x47232B, 0x47232B, tex_particle, false, true, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 384.0f, 0, 0, 0, 0, 0);
				particle(pt_fade, PARTICLE_BILLBOARD, 0x47232B, 0x47232B, tex_particle, false, false, dec, dec, 128, 9999, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 384.0f, 0, 0, 0, 0, 0);
				break;

			case 7:	// Nehahra smoke tracer
				if (!cl_particles_smoke.integer)
					return;
				dec = 10;
				particle(pt_smoke, PARTICLE_BILLBOARD, 0x202020, 0x404040, tex_smoke[rand()&7], true, false, dec, dec, 64, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
	vec3_t		vec, pos;
	int			len;
	if (!cl_particles.integer) return;
	if (!cl_particles_smoke.integer) return;

	VectorCopy(start, pos);
	VectorSubtract (end, start, vec);
	len = (int) (VectorNormalizeLength (vec) * (1.0f / 3.0f));
	VectorScale(vec, 3, vec);
	color = particlepalette[color];
	while (len--)
	{
		particle(pt_smoke, PARTICLE_BILLBOARD, color, color, tex_particle, false, false, 5, 5, 128, 9999, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
	//renderparticle_t *r, *rend;
	int i, activeparticles, maxparticle, j, a, pressureused = false, content;
	float gravity, dvel, frametime, f, dist, normal[3], v[3], org[3];

	// LordHavoc: early out condition
	if (!cl_numparticles)
	{
		//r_refdef.numparticles = 0;
		return;
	}

	frametime = cl.time - cl.oldtime;
	if (!frametime)
		return; // if absolutely still, don't update particles
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	for (i = 0, p = particles/*, r = r_refdef.particles, rend = r + cl_maxparticles*/;i < cl_numparticles;i++, p++)
	{
		if (p->die < cl.time)
		{
			freeparticles[j++] = p;
			continue;
		}

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

			// LordHavoc: drop-through because of shared code
		case pt_blob:
			p->vel[2] *= dvel;
		case pt_blob2:
			p->vel[0] *= dvel;
			p->vel[1] *= dvel;
			p->alpha -= frametime * 256;
			if (p->alpha < 1)
				p->die = -1;
			break;

		case pt_grav:
			p->vel[2] -= gravity;
			break;
		case pt_lavasplash:
			p->vel[2] -= gravity * 0.05;
			p->alpha -= frametime * 192;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_snow:
			if (cl.time > p->time2)
			{
				p->time2 = cl.time + (rand() & 3) * 0.1;
				p->vel[0] = (rand()&63)-32 + p->vel2[0];
				p->vel[1] = (rand()&63)-32 + p->vel2[1];
				p->vel[2] = (rand()&63)-32 + p->vel2[2];
			}
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			a = content;
			if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
			{
				p->die = -1;
				/*
				if (a == CONTENTS_SOLID && Mod_PointInLeaf(p->oldorg, cl.worldmodel)->contents == CONTENTS_SOLID)
					break; // still in solid
				p->die = cl.time + 1000;
				p->vel[0] = p->vel[1] = p->vel[2] = 0;
				switch (a)
				{
				case CONTENTS_LAVA:
				case CONTENTS_SLIME:
					p->tex = tex_smoke[rand()&7];
					p->orientation = PARTICLE_BILLBOARD;
					p->type = pt_steam;
					p->alpha = 96;
					p->scalex = 5;
					p->scaley = 5;
					p->vel[2] = 96;
					break;
				case CONTENTS_WATER:
					p->tex = tex_smoke[rand()&7];
					p->orientation = PARTICLE_BILLBOARD;
					p->type = pt_splash;
					p->alpha = 96;
					p->scalex = 5;
					p->scaley = 5;
					p->vel[2] = 96;
					break;
				default: // CONTENTS_SOLID and any others
					TraceLine(p->oldorg, p->org, v, normal, 0, true);
					VectorCopy(v, p->org);
					p->tex = tex_smoke[rand()&7];
					p->orientation = PARTICLE_BILLBOARD;
					p->type = pt_fade;
					p->time2 = 384.0f;
					p->scalex = 5;
					p->scaley = 5;
					VectorClear(p->vel);
					break;
				}
				*/
			}
			break;
		case pt_blood:
			p->friction = 1;
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			a = content;
			if (a != CONTENTS_EMPTY)
			{
				if (a == CONTENTS_WATER || a == CONTENTS_SLIME)
				{
					//p->friction = 5;
					p->scalex += frametime * (cl_particles_blood_size_min.value + cl_particles_blood_size_max.value);
					p->scaley += frametime * (cl_particles_blood_size_min.value + cl_particles_blood_size_max.value);
					p->alpha -= frametime * max(cl_particles_blood_alpha.value, 0.01f) * 128.0f;
					//p->vel[2] += gravity * 0.25f;
					if (p->alpha < 1)
						p->die = -1;
				}
				else
					p->die = -1;
			}
			else
				p->vel[2] -= gravity;
			break;
		case pt_spark:
			p->alpha -= frametime * p->time2;
			p->vel[2] -= gravity;
			if (p->alpha < 1)
				p->die = -1;
			else
			{
				if (!content)
					content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
				if (content != CONTENTS_EMPTY)
					p->die = -1;
			}
			break;
		case pt_explosionsplash:
			if (Mod_PointInLeaf(p->org, cl.worldmodel)->contents == CONTENTS_EMPTY)
				p->vel[2] -= gravity;
			else
				p->alpha = 0;
			p->scalex += frametime * 64.0f;
			p->scaley += frametime * 64.0f;
			p->alpha -= frametime * 1024.0f;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_fade:
			p->alpha -= frametime * p->time2;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_bubble:
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (content != CONTENTS_WATER && content != CONTENTS_SLIME)
			{
				p->die = -1;
				break;
				/*
				p->tex = tex_smoke[rand()&7];
				p->orientation = PARTICLE_BILLBOARD;
				p->type = pt_splashpuff;
				p->scalex = 4;
				p->scaley = 4;
				p->vel[0] = p->vel[1] = p->vel[2] = 0;
				break;
				*/
			}
			p->vel[0] *= (1 - (frametime * 0.0625));
			p->vel[1] *= (1 - (frametime * 0.0625));
			p->vel[2] = (p->vel[2] + gravity * 0.25) * (1 - (frametime * 0.0625));
			if (cl.time > p->time2)
			{
				p->time2 = cl.time + lhrandom(0, 0.5);
				p->vel[0] += lhrandom(-32,32);
				p->vel[1] += lhrandom(-32,32);
				p->vel[2] += lhrandom(-32,32);
			}
			p->alpha -= frametime * 256;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_bulletsmoke:
			p->scalex += frametime * 16;
			p->scaley += frametime * 16;
			p->alpha -= frametime * 1024;
			p->vel[2] += gravity * 0.2;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_smoke:
			p->scalex += frametime * 16;
			p->scaley += frametime * 16;
			p->alpha -= frametime * 320;
			//p->vel[2] += gravity * 0.2;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_steam:
			p->scalex += frametime * 48;
			p->scaley += frametime * 48;
			p->alpha -= frametime * 512;
			p->vel[2] += gravity * 0.05;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_splashpuff:
			p->alpha -= frametime * 1024;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_rain:
			if (!content)
				content = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			a = content;
			if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
				p->die = -1;
			/*
			f = 0;
			b = Mod_PointInLeaf(p->oldorg, cl.worldmodel)->contents;
			VectorCopy(p->oldorg, o);
			while (f < 1)
			{
				a = b;
				f = TraceLine(o, p->org, v, normal, a, true);
				b = cl_traceline_endcontents;
				if (f < 1 && b != CONTENTS_EMPTY && b != CONTENTS_SKY)
				{
					#if 1
					p->die = -1;
					#else
					p->die = cl.time + 1000;
					p->vel[0] = p->vel[1] = p->vel[2] = 0;
					VectorCopy(v, p->org);
					switch (b)
					{
					case CONTENTS_LAVA:
					case CONTENTS_SLIME:
						p->tex = tex_smoke[rand()&7];
						p->orientation = PARTICLE_BILLBOARD;
						p->type = pt_steam;
						p->scalex = 3;
						p->scaley = 3;
						p->vel[2] = 96;
						break;
					default: // water, solid, and anything else
						p->tex = tex_rainsplash[0];
						p->orientation = PARTICLE_ORIENTED_DOUBLESIDED;
						p->time2 = 0;
						VectorCopy(normal, p->vel2);
					//	VectorAdd(p->org, normal, p->org);
						p->type = pt_raindropsplash;
						p->scalex = 8;
						p->scaley = 8;
						break;
					}
					#endif
					break;
				}
			}
			*/
			break;
			/*
		case pt_raindropsplash:
			p->time2 += frametime * 64.0f;
			if (p->time2 >= 16.0f)
			{
				p->die = -1;
				break;
			}
			p->tex = tex_rainsplash[(int) p->time2];
			p->orientation = PARTICLE_ORIENTED_DOUBLESIDED;
			break;
			*/
		case pt_flame:
			p->alpha -= frametime * 384;
			p->vel[2] += gravity;
			if (p->alpha < 16)
				p->die = -1;
			break;
		case pt_oneframe:
			if (p->time2)
				p->die = -1;
			p->time2 = 1;
			break;
		case pt_stardust:
			p->alpha -= frametime * 128;
			p->vel[2] -= gravity;
			if (p->alpha < 16)
				p->die = -1;
			break;
		default:
			printf("unknown particle type %i\n", p->type);
			p->die = -1;
			break;
		}

		// LordHavoc: immediate removal of unnecessary particles (must be done to ensure compactor below operates properly in all cases)
		if (p->die < cl.time)
			freeparticles[j++] = p;
		else
		{
			maxparticle = i;
			activeparticles++;
			if (p->pressure)
				pressureused = true;

			/*
			// build renderparticle for renderer to use
			r->orientation = p->orientation;
			r->additive = p->additive;
			r->dir[0] = p->vel2[0];
			r->dir[1] = p->vel2[1];
			r->dir[2] = p->vel2[2];
			r->org[0] = p->org[0];
			r->org[1] = p->org[1];
			r->org[2] = p->org[2];
			r->tex = p->tex;
			r->scalex = p->scalex * cl_particles_size.value;
			r->scaley = p->scaley * cl_particles_size.value;
			r->dynlight = p->dynlight;
			r->color[0] = p->color[0] * (1.0f / 255.0f);
			r->color[1] = p->color[1] * (1.0f / 255.0f);
			r->color[2] = p->color[2] * (1.0f / 255.0f);
			r->color[3] = p->alpha * (1.0f / 255.0f);
			r++;
			*/
		}
	}
	//r_refdef.numparticles = r - r_refdef.particles;
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
							//dist = freeparticles[j]->scalex * 4.0f * frametime / dist;
							//VectorMA(p->vel, dist, freeparticles[j]->vel, p->vel);
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
static cvar_t r_particles_lighting = {0, "r_particles_lighting", "1"};

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
	/*
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
					d = (noise1[y][x] - 128) * 2 + 128;
					d = bound(0, d, 255);
					data[y][x][0] = data[y][x][1] = data[y][x][2] = d;
					dx = x - 16;
					d = (noise2[y][x] - 128) * 3 + 192;
					if (d > 0)
				 		d = (d * (256 - (int) (dx*dx+dy*dy))) >> 8;
					d = bound(0, d, 255);
					data[y][x][3] = (qbyte) d;
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
	*/

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

	// rocket flare
	/*
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
	*/

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
	//renderparticle_t *r;
	int i, lighting, dynlight, additive, texnum, orientation;
	float minparticledist, org[3], uprightangles[3], up2[3], right2[3], v[3], right[3], up[3], tvxyz[4][4], tvst[4][2], fog, ifog, fogvec[3];
	mleaf_t *leaf;
	particletexture_t *tex, *texfog;
	rmeshinfo_t m;
	particle_t *p;

	// LordHavoc: early out conditions
	//if ((!r_refdef.numparticles) || (!r_drawparticles.integer))
	if ((!cl_numparticles) || (!r_drawparticles.integer))
		return;

	lighting = r_particles_lighting.integer;
	if (!r_dynamic.integer)
		lighting = 0;

	c_particles += cl_numparticles; //r_refdef.numparticles;

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

	for (i = 0/*, r = r_refdef.particles*/, p = particles;i < /*r_refdef.numparticles*/cl_numparticles;i++, p++)
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
