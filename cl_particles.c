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

#ifdef WORKINGLQUAKE
#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define NUMVERTEXNORMALS	162
siextern float r_avertexnormals[NUMVERTEXNORMALS][3];
#define m_bytenormals r_avertexnormals
#define VectorNormalizeFast VectorNormalize
#define CL_PointQ1Contents(v) (Mod_PointInLeaf(v,cl.worldmodel)->contents)
typedef unsigned char qbyte;
#define cl_stainmaps.integer 0
void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
}
#define CL_EntityParticles R_EntityParticles
#define CL_ReadPointFile_f R_ReadPointFile_f
#define CL_ParseParticleEffect R_ParseParticleEffect
#define CL_ParticleExplosion R_ParticleExplosion
#define CL_ParticleExplosion2 R_ParticleExplosion2
#define CL_TeleportSplash R_TeleportSplash
#define CL_BlobExplosion R_BlobExplosion
#define CL_RunParticleEffect R_RunParticleEffect
#define CL_LavaSplash R_LavaSplash
void R_CalcBeam_Vertex3f (float *vert, vec3_t org1, vec3_t org2, float width)
{
	vec3_t right1, right2, diff, normal;

	VectorSubtract (org2, org1, normal);
	VectorNormalizeFast (normal);

	// calculate 'right' vector for start
	VectorSubtract (r_vieworigin, org1, diff);
	VectorNormalizeFast (diff);
	CrossProduct (normal, diff, right1);

	// calculate 'right' vector for end
	VectorSubtract (r_vieworigin, org2, diff);
	VectorNormalizeFast (diff);
	CrossProduct (normal, diff, right2);

	vert[ 0] = org1[0] + width * right1[0];
	vert[ 1] = org1[1] + width * right1[1];
	vert[ 2] = org1[2] + width * right1[2];
	vert[ 3] = org1[0] - width * right1[0];
	vert[ 4] = org1[1] - width * right1[1];
	vert[ 5] = org1[2] - width * right1[2];
	vert[ 6] = org2[0] - width * right2[0];
	vert[ 7] = org2[1] - width * right2[1];
	vert[ 8] = org2[2] - width * right2[2];
	vert[ 9] = org2[0] + width * right2[0];
	vert[10] = org2[1] + width * right2[1];
	vert[11] = org2[2] + width * right2[2];
}
void fractalnoise(qbyte *noise, int size, int startgrid)
{
	int x, y, g, g2, amplitude, min, max, size1 = size - 1, sizepower, gridpower;
	int *noisebuf;
#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]

	for (sizepower = 0;(1 << sizepower) < size;sizepower++);
	if (size != (1 << sizepower))
		Sys_Error("fractalnoise: size must be power of 2\n");

	for (gridpower = 0;(1 << gridpower) < startgrid;gridpower++);
	if (startgrid != (1 << gridpower))
		Sys_Error("fractalnoise: grid must be power of 2\n");

	startgrid = bound(0, startgrid, size);

	amplitude = 0xFFFF; // this gets halved before use
	noisebuf = malloc(size*size*sizeof(int));
	memset(noisebuf, 0, size*size*sizeof(int));

	for (g2 = startgrid;g2;g2 >>= 1)
	{
		// brownian motion (at every smaller level there is random behavior)
		amplitude >>= 1;
		for (y = 0;y < size;y += g2)
			for (x = 0;x < size;x += g2)
				n(x,y) += (rand()&amplitude);

		g = g2 >> 1;
		if (g)
		{
			// subdivide, diamond-square algorithm (really this has little to do with squares)
			// diamond
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
					n(x+g,y+g) = (n(x,y) + n(x+g2,y) + n(x,y+g2) + n(x+g2,y+g2)) >> 2;
			// square
			for (y = 0;y < size;y += g2)
				for (x = 0;x < size;x += g2)
				{
					n(x+g,y) = (n(x,y) + n(x+g2,y) + n(x+g,y-g) + n(x+g,y+g)) >> 2;
					n(x,y+g) = (n(x,y) + n(x,y+g2) + n(x-g,y+g) + n(x+g,y+g)) >> 2;
				}
		}
	}
	// find range of noise values
	min = max = 0;
	for (y = 0;y < size;y++)
		for (x = 0;x < size;x++)
		{
			if (n(x,y) < min) min = n(x,y);
			if (n(x,y) > max) max = n(x,y);
		}
	max -= min;
	max++;
	// normalize noise and copy to output
	for (y = 0;y < size;y++)
		for (x = 0;x < size;x++)
			*noise++ = (qbyte) (((n(x,y) - min) * 256) / max);
	free(noisebuf);
#undef n
}
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	right[0] -= d * forward[0];
	right[1] -= d * forward[1];
	right[2] -= d * forward[2];
	VectorNormalizeFast(right);
	CrossProduct(right, forward, up);
}
#if QW
#include "pmove.h"
extern qboolean PM_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, pmtrace_t *trace);
#endif
trace_t CL_TraceBox (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int hitbmodels, int *hitent, int hitsupercontentsmask, qboolean hitplayers)
{
#if QW
	pmtrace_t trace;
#else
	trace_t trace;
#endif
	memset (&trace, 0, sizeof(trace));
	trace.fraction = 1;
	VectorCopy (end, trace.endpos);
#if QW
	PM_RecursiveHullCheck (cl.model_precache[1]->hulls, 0, 0, 1, start, end, &trace);
#else
	RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);
#endif
	return trace;
}
#else
#include "cl_collision.h"
#include "image.h"
#endif

#define MAX_PARTICLES			32768	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

typedef enum
{
	PARTICLE_BILLBOARD = 0,
	PARTICLE_SPARK = 1,
	PARTICLE_ORIENTED_DOUBLESIDED = 2,
	PARTICLE_BEAM = 3
}
porientation_t;

typedef enum
{
	PBLEND_ALPHA = 0,
	PBLEND_ADD = 1,
	PBLEND_MOD = 2
}
pblend_t;

typedef struct particletype_s
{
	pblend_t blendmode;
	porientation_t orientation;
	qboolean lighting;
}
particletype_t;

typedef enum
{
	pt_alphastatic, pt_static, pt_spark, pt_beam, pt_rain, pt_raindecal, pt_snow, pt_bubble, pt_blood, pt_smoke, pt_decal, pt_entityparticle, pt_total
}
ptype_t;

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

typedef struct particle_s
{
	particletype_t *type;
	int			texnum;
	vec3_t		org;
	vec3_t		vel; // velocity of particle, or orientation of decal, or end point of beam
	float		size;
	float		alpha; // 0-255
	float		alphafade; // how much alpha reduces per second
	float		time2; // used for snow fluttering and decal fade
	float		bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	float		gravity; // how much gravity affects this particle (1.0 = normal gravity, 0.0 = none)
	float		friction; // how much air friction affects this object (objects with a low mass/size ratio tend to get more air friction)
	qbyte		color[4];
#ifndef WORKINGLQUAKE
	unsigned short owner; // decal stuck to this entity
	model_t		*ownermodel; // model the decal is stuck to (used to make sure the entity is still alive)
	vec3_t		relativeorigin; // decal at this location in entity's coordinate space
	vec3_t		relativedirection; // decal oriented this way relative to entity's coordinate space
#endif
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

static int			cl_maxparticles;
static int			cl_numparticles;
static int			cl_freeparticle;
static particle_t	*particles;

cvar_t cl_particles = {CVAR_SAVE, "cl_particles", "1"};
cvar_t cl_particles_quality = {CVAR_SAVE, "cl_particles_quality", "1"};
cvar_t cl_particles_size = {CVAR_SAVE, "cl_particles_size", "1"};
cvar_t cl_particles_bloodshowers = {CVAR_SAVE, "cl_particles_bloodshowers", "1"};
cvar_t cl_particles_blood = {CVAR_SAVE, "cl_particles_blood", "1"};
cvar_t cl_particles_blood_alpha = {CVAR_SAVE, "cl_particles_blood_alpha", "0.5"};
cvar_t cl_particles_blood_bloodhack = {CVAR_SAVE, "cl_particles_blood_bloodhack", "1"};
cvar_t cl_particles_bulletimpacts = {CVAR_SAVE, "cl_particles_bulletimpacts", "1"};
cvar_t cl_particles_explosions_bubbles = {CVAR_SAVE, "cl_particles_explosions_bubbles", "1"};
cvar_t cl_particles_explosions_smoke = {CVAR_SAVE, "cl_particles_explosions_smokes", "0"};
cvar_t cl_particles_explosions_sparks = {CVAR_SAVE, "cl_particles_explosions_sparks", "1"};
cvar_t cl_particles_explosions_shell = {CVAR_SAVE, "cl_particles_explosions_shell", "0"};
cvar_t cl_particles_smoke = {CVAR_SAVE, "cl_particles_smoke", "1"};
cvar_t cl_particles_smoke_alpha = {CVAR_SAVE, "cl_particles_smoke_alpha", "0.5"};
cvar_t cl_particles_smoke_alphafade = {CVAR_SAVE, "cl_particles_smoke_alphafade", "0.55"};
cvar_t cl_particles_sparks = {CVAR_SAVE, "cl_particles_sparks", "1"};
cvar_t cl_particles_bubbles = {CVAR_SAVE, "cl_particles_bubbles", "1"};
cvar_t cl_decals = {CVAR_SAVE, "cl_decals", "0"};
cvar_t cl_decals_time = {CVAR_SAVE, "cl_decals_time", "0"};
cvar_t cl_decals_fadetime = {CVAR_SAVE, "cl_decals_fadetime", "20"};

void CL_Particles_Clear(void)
{
	cl_numparticles = 0;
	cl_freeparticle = 0;
	memset(particles, 0, sizeof(particle_t) * cl_maxparticles);
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

// COMMANDLINEOPTION: Client: -particles <number> changes maximum number of particles at once, default 32768
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
	Cvar_RegisterVariable (&cl_particles_quality);
	Cvar_RegisterVariable (&cl_particles_size);
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

#ifdef WORKINGLQUAKE
	particles = (particle_t *) Hunk_AllocName(cl_maxparticles * sizeof(particle_t), "particles");
#else
	particles = (particle_t *) Mem_Alloc(cl_mempool, cl_maxparticles * sizeof(particle_t));
#endif
	CL_Particles_Clear();
}

void CL_Particles_Shutdown (void)
{
#ifdef WORKINGLQUAKE
	// No clue what to do here...
#endif
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
particle_t *particle(particletype_t *ptype, int pcolor1, int pcolor2, int ptex, float psize, float palpha, float palphafade, float pgravity, float pbounce, float px, float py, float pz, float pvx, float pvy, float pvz, float pfriction)
{
	particle_t *part;
	int ptempcolor, ptempcolor2, pcr1, pcg1, pcb1, pcr2, pcg2, pcb2;
	ptempcolor = (pcolor1);
	ptempcolor2 = (pcolor2);
	pcr2 = ((ptempcolor2) >> 16) & 0xFF;
	pcg2 = ((ptempcolor2) >> 8) & 0xFF;
	pcb2 = (ptempcolor2) & 0xFF;
	if (ptempcolor != ptempcolor2)
	{
		pcr1 = ((ptempcolor) >> 16) & 0xFF;
		pcg1 = ((ptempcolor) >> 8) & 0xFF;
		pcb1 = (ptempcolor) & 0xFF;
		ptempcolor = rand() & 0xFF;
		pcr2 = (((pcr2 - pcr1) * ptempcolor) >> 8) + pcr1;
		pcg2 = (((pcg2 - pcg1) * ptempcolor) >> 8) + pcg1;
		pcb2 = (((pcb2 - pcb1) * ptempcolor) >> 8) + pcb1;
	}
	for (;cl_freeparticle < cl_maxparticles && particles[cl_freeparticle].type;cl_freeparticle++);
	if (cl_freeparticle >= cl_maxparticles)
		return NULL;
	part = &particles[cl_freeparticle++];
	if (cl_numparticles < cl_freeparticle)
		cl_numparticles = cl_freeparticle;
	memset(part, 0, sizeof(*part));
	part->type = (ptype);
	part->color[0] = pcr2;
	part->color[1] = pcg2;
	part->color[2] = pcb2;
	part->color[3] = 0xFF;
	part->texnum = ptex;
	part->size = (psize);
	part->alpha = (palpha);
	part->alphafade = (palphafade);
	part->gravity = (pgravity);
	part->bounce = (pbounce);
	part->org[0] = (px);
	part->org[1] = (py);
	part->org[2] = (pz);
	part->vel[0] = (pvx);
	part->vel[1] = (pvy);
	part->vel[2] = (pvz);
	part->time2 = 0;
	part->friction = (pfriction);
	return part;
}

void CL_SpawnDecalParticleForSurface(int hitent, const vec3_t org, const vec3_t normal, int color1, int color2, int texnum, float size, float alpha)
{
	particle_t *p;
	if (!cl_decals.integer)
		return;
	p = particle(particletype + pt_decal, color1, color2, texnum, size, alpha, 0, 0, 0, org[0] + normal[0], org[1] + normal[1], org[2] + normal[2], normal[0], normal[1], normal[2], 0);
	if (p)
	{
		p->time2 = cl.time;
#ifndef WORKINGLQUAKE
		p->owner = hitent;
		p->ownermodel = cl_entities[p->owner].render.model;
		Matrix4x4_Transform(&cl_entities[p->owner].render.inversematrix, org, p->relativeorigin);
		Matrix4x4_Transform3x3(&cl_entities[p->owner].render.inversematrix, normal, p->relativedirection);
		VectorAdd(p->relativeorigin, p->relativedirection, p->relativeorigin);
#endif
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
		trace = CL_TraceBox(org, vec3_origin, vec3_origin, org2, true, &hitent, SUPERCONTENTS_SOLID, false);
		if (bestfrac > trace.fraction)
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

#ifdef WORKINGLQUAKE
		particle(particletype + pt_entityparticle, particlepalette[0x6f], particlepalette[0x6f], tex_particle, 2, 255, 0, 0, 0, ent->origin[0] + m_bytenormals[i][0]*dist + forward[0]*beamlength, ent->origin[1] + m_bytenormals[i][1]*dist + forward[1]*beamlength, ent->origin[2] + m_bytenormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0, 0);
#else
		particle(particletype + pt_entityparticle, particlepalette[0x6f], particlepalette[0x6f], tex_particle, 2, 255, 0, 0, 0, ent->render.origin[0] + m_bytenormals[i][0]*dist + forward[0]*beamlength, ent->render.origin[1] + m_bytenormals[i][1]*dist + forward[1]*beamlength, ent->render.origin[2] + m_bytenormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0, 0);
#endif
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
#if WORKINGLQUAKE
	pointfile = COM_LoadTempFile (name);
#else
	pointfile = FS_LoadFile(name, tempmempool, true);
#endif
	if (!pointfile)
	{
		Con_Printf("Could not open %s\n", name);
		return;
	}

	Con_Printf("Reading %s...\n", name);
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

		if (cl_numparticles < cl_maxparticles - 3)
		{
			s++;
			particle(particletype + pt_static, particlepalette[(-c)&15], particlepalette[(-c)&15], tex_particle, 2, 255, 0, 0, 0, org[0], org[1], org[2], 0, 0, 0, 0);
		}
	}
#ifndef WORKINGLQUAKE
	Mem_Free(pointfile);
#endif
	VectorCopy(leakorg, org);
	Con_Printf("%i points read (%i particles spawned)\nLeak at %f %f %f\n", c, s, org[0], org[1], org[2]);

	particle(particletype + pt_beam, 0xFF0000, 0xFF0000, tex_beam, 64, 255, 0, 0, 0, org[0] - 4096, org[1], org[2], org[0] + 4096, org[1], org[2], 0);
	particle(particletype + pt_beam, 0x00FF00, 0x00FF00, tex_beam, 64, 255, 0, 0, 0, org[0], org[1] - 4096, org[2], org[0], org[1] + 4096, org[2], 0);
	particle(particletype + pt_beam, 0x0000FF, 0x0000FF, tex_beam, 64, 255, 0, 0, 0, org[0], org[1], org[2] - 4096, org[0], org[1], org[2] + 4096, 0);
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

	MSG_ReadVector(org, cl.protocol);
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	if (cl_particles_blood_bloodhack.integer)
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

	i = CL_PointSuperContents(org);
	if (i & (SUPERCONTENTS_SLIME | SUPERCONTENTS_WATER))
	{
		if (cl_particles.integer && cl_particles_bubbles.integer && cl_particles_explosions_bubbles.integer)
			for (i = 0;i < 128 * cl_particles_quality.value;i++)
				particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, (1.0f / cl_particles_quality.value) * lhrandom(128, 255), (1.0f / cl_particles_quality.value) * 128, -0.125, 1.5, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-96, 96), lhrandom(-96, 96), lhrandom(-96, 96), (1.0 / 16.0));
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
#ifdef WORKINGLQUAKE
				v2[0] = lhrandom(-48, 48);
				v2[1] = lhrandom(-48, 48);
				v2[2] = lhrandom(-48, 48);
#else
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
#endif
				VectorScale(v2, 2.0f, v2);
				particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 12, 32, 64, 0, 0, org[0], org[1], org[2], v2[0], v2[1], v2[2], 0);
			}
		}

		if (cl_particles.integer && cl_particles_sparks.integer && cl_particles_explosions_sparks.integer)
			for (i = 0;i < 128 * cl_particles_quality.value;i++)
				particle(particletype + pt_spark, 0x903010, 0xFFD030, tex_particle, 1.0f, (1.0f / cl_particles_quality.value) * lhrandom(0, 255), (1.0f / cl_particles_quality.value) * 512, 1, 0, org[0], org[1], org[2], lhrandom(-256, 256), lhrandom(-256, 256), lhrandom(-256, 256) + 80, 0.2);
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
	vec3_t vel;
	vec3_t offset;
	int i, k;
	float pscale;
	if (!cl_particles.integer) return;

	for (i = 0;i < 512 * cl_particles_quality.value;i++)
	{
		VectorRandom (offset);
		VectorScale (offset, 192, vel);
		VectorScale (offset, 8, offset);
		k = particlepalette[colorStart + (i % colorLength)];
		pscale = lhrandom(0.5, 1.5);
		particle(particletype + pt_static, k, k, tex_particle, pscale, (1.0f / cl_particles_quality.value) * 255, (1.0f/cl_particles_quality.value)*512, 0, 0, org[0] + offset[0], org[1] + offset[1], org[2] + offset[2], vel[0], vel[1], vel[2], lhrandom(1.5, 3));
	}
}

/*
===============
CL_BlobExplosion

===============
*/
void CL_BlobExplosion (vec3_t org)
{
	CL_ParticleExplosion(org);
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
	count *= cl_particles_quality.value;
	while (count--)
	{
		k = particlepalette[color + (rand()&7)];
		if (gamemode == GAME_GOODVSBAD2)
			particle(particletype + pt_alphastatic, k, k, tex_particle, 5, (1.0f / cl_particles_quality.value) * 255, (1.0f / cl_particles_quality.value) * 300, 0, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-10, 10), lhrandom(-10, 10), lhrandom(-10, 10), 0);
		else
			particle(particletype + pt_alphastatic, k, k, tex_particle, 1, (1.0f / cl_particles_quality.value) * 255, (1.0f / cl_particles_quality.value) * 512, 0, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), dir[0] + lhrandom(-15, 15), dir[1] + lhrandom(-15, 15), dir[2] + lhrandom(-15, 15), 0);
	}
}

// LordHavoc: added this for spawning sparks/dust (which have strong gravity)
/*
===============
CL_SparkShower
===============
*/
void CL_SparkShower (vec3_t org, vec3_t dir, int count, vec_t gravityscale)
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
			particle(particletype + pt_spark, k, k, tex_particle, 0.4f, (1.0f / cl_particles_quality.value) * lhrandom(64, 255), (1.0f / cl_particles_quality.value) * 512, gravityscale, 0, org[0], org[1], org[2], lhrandom(-64, 64) + dir[0], lhrandom(-64, 64) + dir[1], lhrandom(0, 128) + dir[2], 0);
		}
	}
}

void CL_Smoke (vec3_t org, vec3_t dir, int count)
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
			particle(particletype + pt_smoke, 0x101010, 0x202020, tex_smoke[rand()&7], 3, (1.0f / cl_particles_quality.value) * 255, (1.0f / cl_particles_quality.value) * 1024, 0, 0, trace.endpos[0], trace.endpos[1], trace.endpos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
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
	if (!cl_particles_blood.integer) return;

	s = count + 64.0f;
	count *= 5.0f;
	if (count > 1000)
		count = 1000;
	bloodcount += count;
	while(bloodcount > 0)
	{
		org2[0] = org[0] + 0.125f * lhrandom(-bloodcount, bloodcount);
		org2[1] = org[1] + 0.125f * lhrandom(-bloodcount, bloodcount);
		org2[2] = org[2] + 0.125f * lhrandom(-bloodcount, bloodcount);
		trace = CL_TraceBox(org, vec3_origin, vec3_origin, org2, true, NULL, SUPERCONTENTS_SOLID, false);
		particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, cl_particles_blood_alpha.value * 768 / cl_particles_quality.value, cl_particles_blood_alpha.value * 384 / cl_particles_quality.value, 0, -1, trace.endpos[0], trace.endpos[1], trace.endpos[2], vel[0] + lhrandom(-s, s), vel[1] + lhrandom(-s, s), vel[2] + lhrandom(-s, s), 1);
		bloodcount -= 16 / cl_particles_quality.value;
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

	bloodcount += count * 5.0f;
	while (bloodcount > 0)
	{
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		bloodcount -= 16 / cl_particles_quality.value;
		particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, cl_particles_blood_alpha.value * 768 / cl_particles_quality.value, cl_particles_blood_alpha.value * 384 / cl_particles_quality.value, 0, -1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 1);
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
		particle(particletype + pt_alphastatic, k, k, tex_particle, 2, 255 / cl_particles_quality.value, (255 / cl_particles_quality.value) / 2, gravity ? 1 : 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0] + lhrandom(-randomvel, randomvel), dir[1] + lhrandom(-randomvel, randomvel), dir[2] + lhrandom(-randomvel, randomvel), 0);
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
				particle(particletype + pt_rain, k, k, tex_particle, 20, lhrandom(8, 16) / cl_particles_quality.value, 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0);
			else
				particle(particletype + pt_rain, k, k, tex_particle, 0.5, lhrandom(8, 16) / cl_particles_quality.value, 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0);
		}
		break;
	case 1:
		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			if (gamemode == GAME_GOODVSBAD2)
				particle(particletype + pt_snow, k, k, tex_particle, 20, lhrandom(64, 128) / cl_particles_quality.value, 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0);
			else
				particle(particletype + pt_snow, k, k, tex_particle, 1, lhrandom(64, 128) / cl_particles_quality.value, 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0);
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

	count *= cl_particles_quality.value;
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
		particle(particletype + pt_static, 0x903010, 0xFFD030, tex_particle, 1.5, lhrandom(64, 128) / cl_particles_quality.value, 128 / cl_particles_quality.value, 1, 0, o[0], o[1], o[2], v[0], v[1], v[2], 0.2);
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
		particle(particletype + pt_static, k, k, tex_particle, 4, lhrandom(64, 128) / cl_particles_quality.value, 384 / cl_particles_quality.value, -1, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-32, 32), lhrandom(-32, 32), lhrandom(0, 64), 1);
		if (count & 1)
			particle(particletype + pt_static, 0x303030, 0x606060, tex_smoke[rand()&7], 6, lhrandom(48, 96) / cl_particles_quality.value, 64 / cl_particles_quality.value, 0, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 32), 0);
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
		particle(particletype + pt_static, k, k, tex_particle, 4, lhrandom(64, 128) / cl_particles_quality.value, 384 / cl_particles_quality.value, -1, 1.1, org[0], org[1], org[2], vel[0] + lhrandom(-128, 128), vel[1] + lhrandom(-128, 128), vel[2] + lhrandom(-128, 128), 1);
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

	inc = 32 / cl_particles_quality.value;
	for (i = -128;i < 128;i += inc)
	{
		for (j = -128;j < 128;j += inc)
		{
			dir[0] = j + lhrandom(0, 8);
			dir[1] = i + lhrandom(0, 8);
			dir[2] = 256;
			org[0] = origin[0] + dir[0];
			org[1] = origin[1] + dir[1];
			org[2] = origin[2] + lhrandom(0, 64);
			vel = lhrandom(50, 120) / VectorLength(dir); // normalize and scale
			if (gamemode == GAME_GOODVSBAD2)
			{
				k = particlepalette[0 + (rand()&255)];
				l = particlepalette[0 + (rand()&255)];
				particle(particletype + pt_static, k, l, tex_particle, 12, inc * 8, inc * 8, 0.05, 1, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0);
			}
			else
			{
				k = l = particlepalette[224 + (rand()&7)];
				particle(particletype + pt_static, k, l, tex_particle, 12, inc * 8, inc * 8, 0.05, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0);
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

	inc = 8 / cl_particles_quality.value;
	for (i = -16;i < 16;i += inc)
		for (j = -16;j < 16;j += inc)
			for (k = -24;k < 32;k += inc)
				particle(particletype + pt_static, 0xA0A0A0, 0xFFFFFF, tex_particle, 10, inc * lhrandom(8, 16), inc * 32, 0, 0, org[0] + i + lhrandom(0, 8), org[1] + j + lhrandom(0, 8), org[2] + k + lhrandom(0, 8), lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(-256, 256), 1);
}

#ifdef WORKINGLQUAKE
void R_RocketTrail (vec3_t start, vec3_t end, int type)
#else
void CL_RocketTrail (vec3_t start, vec3_t end, int type, int color, entity_t *ent)
#endif
{
	vec3_t vec, dir, vel, pos;
	float len, dec, speed, qd;
	int smoke, blood, bubbles;
#ifdef WORKINGLQUAKE
	int contents;
#endif

	if (end[0] == start[0] && end[1] == start[1] && end[2] == start[2])
		return;

	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	VectorSubtract (end, start, vec);
#ifdef WORKINGLQUAKE
	len = VectorNormalize (vec);
	dec = 0;
	speed = 1.0f / cl.frametime;
	VectorSubtract(end, start, vel);
#else
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
#endif
	VectorScale(vel, speed, vel);

	// advance into this frame to reach the first puff location
	VectorMA(start, dec, vec, pos);
	len -= dec;

	smoke = cl_particles.integer && cl_particles_smoke.integer;
	blood = cl_particles.integer && cl_particles_blood.integer;
#ifdef WORKINGLQUAKE
	contents = CL_PointQ1Contents(pos);
	bubbles = cl_particles.integer && cl_particles_bubbles.integer && (contents == CONTENTS_WATER || contents == CONTENTS_SLIME);
#else
	bubbles = cl_particles.integer && cl_particles_bubbles.integer && (CL_PointSuperContents(pos) & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME));
#endif
	qd = 1.0f / cl_particles_quality.value;

	while (len >= 0)
	{
		switch (type)
		{
			case 0:	// rocket trail
				dec = qd*3;
				if (smoke)
				{
					particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, qd*cl_particles_smoke_alpha.value*125, qd*cl_particles_smoke_alphafade.value*125, 0, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 0);
					particle(particletype + pt_static, 0x801010, 0xFFA020, tex_smoke[rand()&7], 3, qd*cl_particles_smoke_alpha.value*288, qd*cl_particles_smoke_alphafade.value*1400, 0, 0, pos[0], pos[1], pos[2], lhrandom(-20, 20), lhrandom(-20, 20), lhrandom(-20, 20), 0);
				}
				if (bubbles)
					particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, qd*lhrandom(64, 255), qd*256, -0.25, 1.5, pos[0], pos[1], pos[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), (1.0 / 16.0));
				break;

			case 1: // grenade trail
				// FIXME: make it gradually stop smoking
				dec = qd*3;
				if (smoke)
					particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, qd*cl_particles_smoke_alpha.value*100, qd*cl_particles_smoke_alphafade.value*100, 0, 0, pos[0], pos[1], pos[2], lhrandom(-5, 5), lhrandom(-5, 5), lhrandom(-5, 5), 0);
				break;


			case 2:	// blood
			case 4:	// slight blood
				dec = qd*16;
				if (blood)
					particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, qd * cl_particles_blood_alpha.value * 768.0f, qd * cl_particles_blood_alpha.value * 384.0f, 0, -1, pos[0], pos[1], pos[2], vel[0] * 0.5f + lhrandom(-64, 64), vel[1] * 0.5f + lhrandom(-64, 64), vel[2] * 0.5f + lhrandom(-64, 64), 1);
				break;

			case 3:	// green tracer
				dec = qd*6;
				if (smoke)
				{
					if (gamemode == GAME_GOODVSBAD2)
						particle(particletype + pt_static, 0x00002E, 0x000030, tex_particle, 6, qd*128, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
					else
						particle(particletype + pt_static, 0x002000, 0x003000, tex_particle, 6, qd*128, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
				}
				break;

			case 5:	// flame tracer
				dec = qd*6;
				if (smoke)
					particle(particletype + pt_static, 0x301000, 0x502000, tex_particle, 6, qd*128, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
				break;

			case 6:	// voor trail
				dec = qd*6;
				if (smoke)
				{
					if (gamemode == GAME_GOODVSBAD2)
						particle(particletype + pt_alphastatic, particlepalette[0 + (rand()&255)], particlepalette[0 + (rand()&255)], tex_particle, 6, qd*255, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
					else if (gamemode == GAME_PRYDON)
						particle(particletype + pt_static, 0x103040, 0x204050, tex_particle, 6, qd*128, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
					else
						particle(particletype + pt_static, 0x502030, 0x502030, tex_particle, 6, qd*128, qd*384, 0, 0, pos[0], pos[1], pos[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(-8, 8), 0);
				}
				break;
#ifndef WORKINGLQUAKE
			case 7:	// Nehahra smoke tracer
				dec = qd*7;
				if (smoke)
					particle(particletype + pt_alphastatic, 0x303030, 0x606060, tex_smoke[rand()&7], 7, qd*64, qd*320, 0, 0, pos[0], pos[1], pos[2], lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(0, 16), 0);
				break;
			case 8: // Nexuiz plasma trail
				dec = qd*4;
				if (smoke)
					particle(particletype + pt_static, 0x283880, 0x283880, tex_particle, 4, qd*255, qd*1024, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0);
				break;
			case 9: // glow trail
				dec = qd*3;
				if (smoke)
					particle(particletype + pt_alphastatic, color, color, tex_particle, 5, qd*128, qd*320, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0);
				break;
#endif
		}

		// advance to next time and position
		len -= dec;
		VectorMA (pos, dec, vec, pos);
	}
#ifndef WORKINGLQUAKE
	ent->persistent.trail_time = len;
#endif
}

void CL_BeamParticle (const vec3_t start, const vec3_t end, vec_t radius, float red, float green, float blue, float alpha, float lifetime)
{
	int tempcolor2, cr, cg, cb;
	cr = red * 255;
	cg = green * 255;
	cb = blue * 255;
	tempcolor2 = (bound(0, cr, 255) << 16) | (bound(0, cg, 255) << 8) | bound(0, cb, 255);
	particle(particletype + pt_beam, tempcolor2, tempcolor2, tex_beam, radius, alpha * 255, alpha * 255 / lifetime, 0, 0, start[0], start[1], start[2], end[0], end[1], end[2], 0);
}

void CL_Tei_Smoke(const vec3_t org, const vec3_t dir, int count)
{
	float f;
	if (!cl_particles.integer) return;

	// smoke puff
	if (cl_particles_smoke.integer)
		for (f = 0;f < count;f += 4.0f / cl_particles_quality.value)
			particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 255 / cl_particles_quality.value, 512 / cl_particles_quality.value, 0, 0, org[0] + 0.125f * lhrandom(-count, count), org[1] + 0.125f * lhrandom (-count, count), org[2] + 0.125f * lhrandom(-count, count), dir[0] + lhrandom(-count, count) * 0.5f, dir[1] + lhrandom(-count, count) * 0.5f, dir[2] + lhrandom(-count, count) * 0.5f, 0);
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
			particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 255 / cl_particles_quality.value, 512 / cl_particles_quality.value, 0, 0, org[0] + 0.125f * lhrandom(-count, count), org[1] + 0.125f * lhrandom (-count, count), org[2] + 0.125f * lhrandom(-count, count), dir[0] + lhrandom(-count, count), dir[1] + lhrandom(-count, count), dir[2] + lhrandom(-count, count), 0);

	// sparks
	if (cl_particles_sparks.integer)
		for (f = 0;f < count;f += 1.0f / cl_particles_quality.value)
			particle(particletype + pt_spark, 0x2030FF, 0x80C0FF, tex_particle, 2.0f, lhrandom(64, 255) / cl_particles_quality.value, 512 / cl_particles_quality.value, 0, 0, org[0], org[1], org[2], lhrandom(-count, count) * 3.0f + dir[0], lhrandom(-count, count) * 3.0f + dir[1], lhrandom(-count, count) * 3.0f + dir[2], 0);
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
	if (!cl_numparticles)
	{
		cl_freeparticle = 0;
		return;
	}

#ifdef WORKINGLQUAKE
	frametime = cl.frametime;
#else
	frametime = cl.time - cl.oldtime;
#endif
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;
	bloodwaterfade = max(cl_particles_blood_alpha.value, 0.01f) * frametime * 128.0f;

	maxparticle = -1;
	j = 0;
	for (i = 0, p = particles;i < cl_numparticles;i++, p++)
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
				if (p->type == particletype + pt_rain)
				{
					// raindrop - splash on solid/water/slime/lava
					trace = CL_TraceBox(oldorg, vec3_origin, vec3_origin, p->org, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_LIQUIDSMASK, false);
					if (trace.fraction < 1)
					{
						// convert from a raindrop particle to a rainsplash decal
						VectorCopy(trace.endpos, p->org);
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
					}
				}
				else if (p->type == particletype + pt_blood)
				{
					// blood - splash on solid
					trace = CL_TraceBox(oldorg, vec3_origin, vec3_origin, p->org, true, &hitent, SUPERCONTENTS_SOLID, false);
					if (trace.fraction < 1)
					{
						// convert from a blood particle to a blood decal
						VectorCopy(trace.endpos, p->org);
						VectorCopy(trace.plane.normal, p->vel);
						VectorAdd(p->org, p->vel, p->org);
#ifndef WORKINGLQUAKE
						if (cl_stainmaps.integer)
							R_Stain(p->org, 32, 32, 16, 16, p->alpha * p->size * (1.0f / 40.0f), 192, 48, 48, p->alpha * p->size * (1.0f / 40.0f));
#endif
						if (!cl_decals.integer)
						{
							p->type = NULL;
							continue;
						}

						p->type = particletype + pt_decal;
						p->texnum = tex_blooddecal[rand()&7];
#ifndef WORKINGLQUAKE
						p->owner = hitent;
						p->ownermodel = cl_entities[hitent].render.model;
						Matrix4x4_Transform(&cl_entities[hitent].render.inversematrix, p->org, p->relativeorigin);
						Matrix4x4_Transform3x3(&cl_entities[hitent].render.inversematrix, p->vel, p->relativedirection);
#endif
						p->time2 = cl.time;
						p->alphafade = 0;
						p->bounce = 0;
						p->friction = 0;
						p->gravity = 0;
						p->size *= 2.0f;
					}
				}
				else
				{
					trace = CL_TraceBox(oldorg, vec3_origin, vec3_origin, p->org, true, NULL, SUPERCONTENTS_SOLID, false);
					if (trace.fraction < 1)
					{
						VectorCopy(trace.endpos, p->org);
						if (p->bounce < 0)
						{
							p->type = NULL;
							continue;
						}
						else
						{
							dist = DotProduct(p->vel, trace.plane.normal) * -p->bounce;
							VectorMA(p->vel, dist, trace.plane.normal, p->vel);
							if (DotProduct(p->vel, p->vel) < 0.03)
								VectorClear(p->vel);
						}
					}
				}
			}
			p->vel[2] -= p->gravity * gravity;

			if (p->friction)
			{
				f = p->friction * frametime;
#ifdef WORKINGLQUAKE
				if (CL_PointQ1Contents(p->org) != CONTENTS_EMPTY)
#else
				if (CL_PointSuperContents(p->org) & SUPERCONTENTS_LIQUIDSMASK)
#endif
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
#ifdef WORKINGLQUAKE
				a = CL_PointQ1Contents(p->org);
				if (a <= CONTENTS_WATER)
#else
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME))
#endif
				{
					p->size += frametime * 8;
					//p->alpha -= bloodwaterfade;
				}
				else
					p->vel[2] -= gravity;
#ifdef WORKINGLQUAKE
				if (a == CONTENTS_SOLID || a == CONTENTS_LAVA)
#else
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_LAVA | SUPERCONTENTS_NODROP))
#endif
					p->type = NULL;
				break;
			case pt_bubble:
#ifdef WORKINGLQUAKE
				a = CL_PointQ1Contents(p->org);
				if (a != CONTENTS_WATER && a != CONTENTS_SLIME)
#else
				a = CL_PointSuperContents(p->org);
				if (!(a & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME)))
#endif
				{
					p->type = NULL;
					break;
				}
				break;
			case pt_rain:
#ifdef WORKINGLQUAKE
				a = CL_PointQ1Contents(p->org);
				if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
#else
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_LIQUIDSMASK))
#endif
					p->type = NULL;
				break;
			case pt_snow:
				if (cl.time > p->time2)
				{
					// snow flutter
					p->time2 = cl.time + (rand() & 3) * 0.1;
					p->vel[0] += lhrandom(-32, 32);
					p->vel[1] += lhrandom(-32, 32);
					p->vel[2] += lhrandom(-32, 32);
				}
#ifdef WORKINGLQUAKE
				a = CL_PointQ1Contents(p->org);
				if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
#else
				a = CL_PointSuperContents(p->org);
				if (a & (SUPERCONTENTS_SOLID | SUPERCONTENTS_LIQUIDSMASK))
#endif
					p->type = NULL;
				break;
			case pt_smoke:
				//p->size += frametime * 15;
				break;
			case pt_decal:
				// FIXME: this has fairly wacky handling of alpha
				p->alphafade = cl.time > (p->time2 + cl_decals_time.value) ? (255 / cl_decals_fadetime.value) : 0;
#ifndef WORKINGLQUAKE
				if (cl_entities[p->owner].render.model == p->ownermodel)
				{
					Matrix4x4_Transform(&cl_entities[p->owner].render.matrix, p->relativeorigin, p->org);
					Matrix4x4_Transform3x3(&cl_entities[p->owner].render.matrix, p->relativedirection, p->vel);
				}
				else
					p->type = NULL;
#endif
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
	cl_numparticles = maxparticle + 1;
	cl_freeparticle = 0;
}

#define MAX_PARTICLETEXTURES 64
// particletexture_t is a rectangle in the particlefonttexture
typedef struct
{
	rtexture_t *texture;
	float s1, t1, s2, t2;
}
particletexture_t;

#if WORKINGLQUAKE
static int particlefonttexture;
#else
static rtexturepool_t *particletexturepool;
static rtexture_t *particlefonttexture;
#endif
static particletexture_t particletexture[MAX_PARTICLETEXTURES];

static cvar_t r_drawparticles = {0, "r_drawparticles", "1"};

#define PARTICLETEXTURESIZE 64
#define PARTICLEFONTSIZE (PARTICLETEXTURESIZE*8)

static qbyte shadebubble(float dx, float dy, vec3_t light)
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
		return (qbyte) f;
	}
	else
		return 0;
}

static void setuptex(int texnum, qbyte *data, qbyte *particletexturedata)
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

void particletextureblotch(qbyte *data, float radius, float red, float green, float blue, float alpha)
{
	int x, y;
	float cx, cy, dx, dy, f, iradius;
	qbyte *d;
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

void particletextureclamp(qbyte *data, int minr, int ming, int minb, int maxr, int maxg, int maxb)
{
	int i;
	for (i = 0;i < PARTICLETEXTURESIZE*PARTICLETEXTURESIZE;i++, data += 4)
	{
		data[0] = bound(minr, data[0], maxr);
		data[1] = bound(ming, data[1], maxg);
		data[2] = bound(minb, data[2], maxb);
	}
}

void particletextureinvert(qbyte *data)
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
static void R_InitBloodTextures (qbyte *particletexturedata)
{
	int i, j, k, m;
	qbyte data[PARTICLETEXTURESIZE][PARTICLETEXTURESIZE][4];

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
	qbyte data[PARTICLETEXTURESIZE][PARTICLETEXTURESIZE][4], noise3[64][64], data2[64][16][4];
	vec3_t light;
	qbyte *particletexturedata;

	// a note: decals need to modulate (multiply) the background color to
	// properly darken it (stain), and they need to be able to alpha fade,
	// this is a very difficult challenge because it means fading to white
	// (no change to background) rather than black (darkening everything
	// behind the whole decal polygon), and to accomplish this the texture is
	// inverted (dark red blood on white background becomes brilliant cyan
	// and white on black background) so we can alpha fade it to black, then
	// we invert it again during the blendfunc to make it work...

	particletexturedata = Mem_Alloc(tempmempool, PARTICLEFONTSIZE*PARTICLEFONTSIZE*4);
	memset(particletexturedata, 255, PARTICLEFONTSIZE*PARTICLEFONTSIZE*4);

	// smoke
	for (i = 0;i < 8;i++)
	{
		memset(&data[0][0][0], 255, sizeof(data));
		do
		{
			qbyte noise1[PARTICLETEXTURESIZE*2][PARTICLETEXTURESIZE*2], noise2[PARTICLETEXTURESIZE*2][PARTICLETEXTURESIZE*2];

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
					data[y][x][3] = (qbyte) d;
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
			data[y][x][3] = (qbyte) d;
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

#if WORKINGLQUAKE
	glBindTexture(GL_TEXTURE_2D, (particlefonttexture = gl_extension_number++));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#else

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
			data2[y][x][0] = data2[y][x][1] = data2[y][x][2] = (qbyte) bound(0, d, 255);
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
#endif
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
	cl_numparticles = 0;
	cl_freeparticle = 0;
}

void R_Particles_Init (void)
{
	Cvar_RegisterVariable(&r_drawparticles);
#ifdef WORKINGLQUAKE
	r_part_start();
#else
	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown, r_part_newmap);
#endif
}

#ifdef WORKINGLQUAKE
void R_InitParticles(void)
{
	CL_Particles_Init();
	R_Particles_Init();
}
#endif

float particle_vertex3f[12], particle_texcoord2f[8];

#ifdef WORKINGLQUAKE
void R_DrawParticle(particle_t *p)
{
#else
void R_DrawParticleCallback(const void *calldata1, int calldata2)
{
	const particle_t *p = calldata1;
	rmeshstate_t m;
#endif
	pblend_t blendmode;
	float org[3], up2[3], v[3], right[3], up[3], fog, ifog, fogvec[3], cr, cg, cb, ca, size;
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
#ifndef WORKINGLQUAKE
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
		VectorSubtract(org, r_vieworigin, fogvec);
		fog = exp(fogdensity/DotProduct(fogvec,fogvec));
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

	R_Mesh_Matrix(&r_identitymatrix);

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
#endif
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
		VectorNormalizeFast(up);
		v[0] = DotProduct(p->org, up) * (1.0f / 64.0f);
		v[1] = DotProduct(p->vel, up) * (1.0f / 64.0f);
		particle_texcoord2f[0] = 1;particle_texcoord2f[1] = v[0];
		particle_texcoord2f[2] = 0;particle_texcoord2f[3] = v[0];
		particle_texcoord2f[4] = 0;particle_texcoord2f[5] = v[1];
		particle_texcoord2f[6] = 1;particle_texcoord2f[7] = v[1];
	}
	else
		Host_Error("R_DrawParticles: unknown particle orientation %i\n", p->type->orientation);

#if WORKINGLQUAKE
	if (blendmode == PBLEND_ALPHA)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else if (blendmode == PBLEND_ADD)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else //if (blendmode == PBLEND_MOD)
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	glColor4f(cr, cg, cb, ca);
	glBegin(GL_QUADS);
	glTexCoord2f(particle_texcoord2f[0], particle_texcoord2f[1]);glVertex3f(particle_vertex3f[ 0], particle_vertex3f[ 1], particle_vertex3f[ 2]);
	glTexCoord2f(particle_texcoord2f[2], particle_texcoord2f[3]);glVertex3f(particle_vertex3f[ 3], particle_vertex3f[ 4], particle_vertex3f[ 5]);
	glTexCoord2f(particle_texcoord2f[4], particle_texcoord2f[5]);glVertex3f(particle_vertex3f[ 6], particle_vertex3f[ 7], particle_vertex3f[ 8]);
	glTexCoord2f(particle_texcoord2f[6], particle_texcoord2f[7]);glVertex3f(particle_vertex3f[ 9], particle_vertex3f[10], particle_vertex3f[11]);
	glEnd();
#else
	R_Mesh_Draw(0, 4, 2, polygonelements);
#endif
}

void R_DrawParticles (void)
{
	int i;
	float minparticledist;
	particle_t *p;

#ifdef WORKINGLQUAKE
	CL_MoveParticles();
#endif

	// LordHavoc: early out conditions
	if ((!cl_numparticles) || (!r_drawparticles.integer))
		return;

	minparticledist = DotProduct(r_vieworigin, r_viewforward) + 4.0f;

#ifdef WORKINGLQUAKE
	glBindTexture(GL_TEXTURE_2D, particlefonttexture);
	glEnable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(0);
	// LordHavoc: only render if not too close
	for (i = 0, p = particles;i < cl_numparticles;i++, p++)
		if (p->type && DotProduct(p->org, r_viewforward) >= minparticledist)
			R_DrawParticle(p);
	glDepthMask(1);
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
	// LordHavoc: only render if not too close
	for (i = 0, p = particles;i < cl_numparticles;i++, p++)
	{
		if (p->type)
		{
			c_particles++;
			if (DotProduct(p->org, r_viewforward) >= minparticledist || p->type->orientation == PARTICLE_BEAM)
			{
				if (p->type == particletype + pt_decal)
					R_DrawParticleCallback(p, 0);
				else
					R_MeshQueue_AddTransparent(p->org, R_DrawParticleCallback, p, 0);
			}
		}
	}
#endif
}

