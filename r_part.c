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

#define MAX_PARTICLES			4096	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

// LordHavoc: added dust, smoke, snow, bloodcloud, and many others
typedef enum {
	pt_static, pt_grav, pt_blob, pt_blob2, pt_smoke, pt_snow, pt_rain, pt_bloodcloud, pt_fallfadespark, pt_bubble, pt_fade, pt_smokecloud, pt_splash
} ptype_t;

typedef struct particle_s
{
	vec3_t		org;
	float		color;
	vec3_t		vel;
	float		die;
	ptype_t		type;
	float		scale;
	short		texnum;
	float		alpha; // 0-255
	float		time2; // used for various things (snow fluttering, for example)
	vec3_t		oldorg;
	vec3_t		vel2; // used for snow fluttering (base velocity, wind for instance)
} particle_t;

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

int		particletexture;
int		smokeparticletexture[8];
int		rainparticletexture;
int		bubbleparticletexture;

particle_t	*particles;
int			r_numparticles;

vec3_t			r_pright, r_pup, r_ppn;

int			numparticles;
particle_t	**freeparticles; // list used only in compacting particles array

// LordHavoc: reduced duplicate code, and allow particle allocation system independence
#define ALLOCPARTICLE \
	if (numparticles >= r_numparticles)\
		return;\
	p = &particles[numparticles++];

cvar_t r_particles = {"r_particles", "1"};
cvar_t r_dynamicparticles = {"r_dynamicparticles", "0", TRUE};

byte shadebubble(float dx, float dy, vec3_t light)
{
	float	dz, f, dot;
	vec3_t	normal;
	if ((dx*dx+dy*dy) < 1) // it does hit the sphere
	{
		dz = 1 - (dx*dx+dy*dy);
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

void R_InitParticleTexture (void)
{
	int		x,y,d,i;
	float	dx, dy;
	byte	data[32][32][4], noise1[32][32], noise2[32][32];
	vec3_t	light;

	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			dx = x - 16;
			dy = y - 16;
			d = (255 - (dx*dx+dy*dy));
			if (d < 0) d = 0;
			data[y][x][3] = (byte) d;
		}
	}
	particletexture = GL_LoadTexture ("particletexture", 32, 32, &data[0][0][0], true, true, 4);

	for (i = 0;i < 8;i++)
	{
		fractalnoise(&noise1[0][0], 32, 1);
		fractalnoise(&noise2[0][0], 32, 8);
		for (y = 0;y < 32;y++)
			for (x = 0;x < 32;x++)
			{
				data[y][x][0] = data[y][x][1] = data[y][x][2] = (noise1[y][x] >> 1) + 128;
				dx = x - 16;
				dy = y - 16;
				d = ((noise2[y][x] * 384) >> 8) - 128;
				if (d > 0)
				{
					if (d > 255)
						d = 255;
					d = (d * (255 - (int) (dx*dx+dy*dy))) >> 8;
					if (d < 0) d = 0;
					if (d > 255) d = 255;
					data[y][x][3] = (byte) d;
				}
				else
					data[y][x][3] = 0;
			}

		smokeparticletexture[i] = GL_LoadTexture (va("smokeparticletexture%d", i), 32, 32, &data[0][0][0], true, true, 4);
	}

	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 8.0), y < 24 ? (y - 24) * (1.0 / 24.0) : (y - 24) * (1.0 / 8.0), light);
		}
	}
	rainparticletexture = GL_LoadTexture ("rainparticletexture", 32, 32, &data[0][0][0], true, true, 4);

	light[0] = 1;light[1] = 1;light[2] = 1;
	VectorNormalize(light);
	for (x=0 ; x<32 ; x++)
	{
		for (y=0 ; y<32 ; y++)
		{
			data[y][x][0] = data[y][x][1] = data[y][x][2] = 255;
			data[y][x][3] = shadebubble((x - 16) * (1.0 / 16.0), (y - 16) * (1.0 / 16.0), light);
		}
	}
	bubbleparticletexture = GL_LoadTexture ("bubbleparticletexture", 32, 32, &data[0][0][0], true, true, 4);
}

void r_part_start()
{
	particles = (particle_t *) malloc (r_numparticles * sizeof(particle_t));
	freeparticles = (void *) malloc (r_numparticles * sizeof(particle_t *));
	R_InitParticleTexture ();
}

void r_part_shutdown()
{
	free(particles);
	free(freeparticles);
}

/*
===============
R_InitParticles
===============
*/
void R_Particles_Init (void)
{
	int		i;

	i = COM_CheckParm ("-particles");

	if (i)
	{
		r_numparticles = (int)(atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	Cvar_RegisterVariable (&r_particles);
	Cvar_RegisterVariable (&r_dynamicparticles);

	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown);
}

#define particle(ptype, pcolor, ptex, pscale, palpha, ptime, px, py, pz, pvx, pvy, pvz)\
{\
	particle_t	*p;\
	ALLOCPARTICLE\
	p->type = (ptype);\
	p->color = (pcolor);\
	p->texnum = (ptex);\
	p->scale = (pscale);\
	p->alpha = (palpha);\
	p->die = cl.time + (ptime);\
	p->org[0] = (px);\
	p->org[1] = (py);\
	p->org[2] = (pz);\
	p->vel[0] = (pvx);\
	p->vel[1] = (pvy);\
	p->vel[2] = (pvz);\
}
#define particle2(ptype, pcolor, ptex, pscale, palpha, ptime, pbase, poscale, pvscale)\
{\
	particle_t	*p;\
	ALLOCPARTICLE\
	p->type = (ptype);\
	p->color = (pcolor);\
	p->texnum = (ptex);\
	p->scale = (pscale);\
	p->alpha = (palpha);\
	p->die = cl.time + (ptime);\
	p->org[0] = lhrandom(-(poscale), (poscale)) + (pbase)[0];\
	p->org[1] = lhrandom(-(poscale), (poscale)) + (pbase)[1];\
	p->org[2] = lhrandom(-(poscale), (poscale)) + (pbase)[2];\
	p->vel[0] = lhrandom(-(pvscale), (pvscale));\
	p->vel[1] = lhrandom(-(pvscale), (pvscale));\
	p->vel[2] = lhrandom(-(pvscale), (pvscale));\
}
#define particle3(ptype, pcolor, ptex, pscale, palpha, ptime, pbase, pscalex, pscaley, pscalez, pvscalex, pvscaley, pvscalez)\
{\
	particle_t	*p;\
	ALLOCPARTICLE\
	p->type = (ptype);\
	p->color = (pcolor);\
	p->texnum = (ptex);\
	p->scale = (pscale);\
	p->alpha = (palpha);\
	p->die = cl.time + (ptime);\
	p->org[0] = lhrandom(-(pscalex), (pscalex)) + (pbase)[0];\
	p->org[1] = lhrandom(-(pscaley), (pscaley)) + (pbase)[1];\
	p->org[2] = lhrandom(-(pscalez), (pscalez)) + (pbase)[2];\
	p->vel[0] = lhrandom(-(pvscalex), (pvscalex));\
	p->vel[1] = lhrandom(-(pvscaley), (pvscaley));\
	p->vel[2] = lhrandom(-(pvscalez), (pvscalez));\
}
/*
void particle(int type, int color, int tex, float scale, int alpha, float time, float x, float y, float z, float vx, float vy, float vz)
{
	particle_t	*p;
	ALLOCPARTICLE

	p->type = type;
	p->color = color;
	p->texnum = tex;
	p->scale = scale;
	p->alpha = alpha;
	p->die = cl.time + time;
	p->org[0] = x;
	p->org[1] = y;
	p->org[2] = z;
	p->vel[0] = vx;
	p->vel[1] = vy;
	p->vel[2] = vz;
}
void particle2(int type, int color, int tex, float scale, int alpha, float time, vec3_t base, float oscale, float vscale)
{
	particle_t	*p;
	ALLOCPARTICLE

	p->type = type;
	p->color = color;
	p->texnum = tex;
	p->scale = scale;
	p->alpha = alpha;
	p->die = cl.time + time;
	p->org[0] = lhrandom(-oscale, oscale) + base[0];
	p->org[1] = lhrandom(-oscale, oscale) + base[1];
	p->org[2] = lhrandom(-oscale, oscale) + base[2];
	p->vel[0] = lhrandom(-vscale, vscale);
	p->vel[1] = lhrandom(-vscale, vscale);
	p->vel[2] = lhrandom(-vscale, vscale);
}
void particle3(int type, int color, int tex, float scale, int alpha, float time, vec3_t base, float scalex, float scaley, float scalez, float vscalex, float vscaley, float vscalez)
{
	particle_t	*p;
	ALLOCPARTICLE

	p->type = type;
	p->color = color;
	p->texnum = tex;
	p->scale = scale;
	p->alpha = alpha;
	p->die = cl.time + time;
	p->org[0] = lhrandom(-scalex, scalex) + base[0];
	p->org[1] = lhrandom(-scaley, scaley) + base[1];
	p->org[2] = lhrandom(-scalez, scalez) + base[2];
	p->vel[0] = lhrandom(-vscalex, vscalex);
	p->vel[1] = lhrandom(-vscaley, vscaley);
	p->vel[2] = lhrandom(-vscalez, vscalez);
}
*/

/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
	int			count;
	int			i;
	float		angle;
	float		sp, sy, cp, cy;
	vec3_t		forward;
	float		dist;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	
	dist = 64;
	count = 50;

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

		particle(pt_static, 0x6f, particletexture, 2, 255, 0, ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength, ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength, ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0);
	}
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
//	int		i;
//	free_particles = &particles[0];
//	active_particles = NULL;

//	for (i=0 ;i<r_numparticles ; i++)
//		particles[i].next = &particles[i+1];
//	particles[r_numparticles-1].next = NULL;

	numparticles = 0;
}


void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	char	name[MAX_OSPATH];
	
	sprintf (name,"maps/%s.pts", sv.name);

	COM_FOpenFile (name, &f, false);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;)
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		if (numparticles >= r_numparticles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		particle(pt_static, (-c)&15, particletexture, 2, 255, 99999, org[0], org[1], org[2], 0, 0, 0);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
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
	
	R_RunParticleEffect (org, dir, color, count);
}
	
/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion (vec3_t org, int smoke)
{
	int			i;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	particle(pt_smokecloud, (rand()&7) + 8, smokeparticletexture[rand()&7], 30, 160, 2, org[0], org[1], org[2], 0, 0, 0);

	i = Mod_PointInLeaf(org, cl.worldmodel)->contents;
	if (i == CONTENTS_SLIME || i == CONTENTS_WATER)
		for (i=0 ; i<128 ; i++)
			particle2(pt_bubble, (rand()&3) + 12, bubbleparticletexture, lhrandom(1, 2), 255, 2, org, 16, 96);
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	for (i = 0;i < 512;i++)
		particle2(pt_fade, colorStart + (i % colorLength), particletexture, 1.5, 255, 0.3, org, 8, 192);
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
	int			i;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	
	for (i=0 ; i<512 ; i++)
		particle3(pt_blob, 66+(rand()%6), particletexture, 2, 255, lhrandom(1, 1.4), org, 16, 16, 16, 4, 4, 128);
	for (i=0 ; i<512 ; i++)
		particle3(pt_blob2, 150+(rand()%6), particletexture, 2, 255, lhrandom(1, 1.4), org, 16, 16, 16, 4, 4, 128);
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	if (!r_particles.value) return; // LordHavoc: particles are optional
	
	if (count == 1024)
	{
		R_ParticleExplosion(org, false);
		return;
	}
	color &= ~7;
	if (count & 7)
	{
		particle2(pt_fade, color + (rand()&7), particletexture, 6, (count & 7) * 16 + (rand()&15), 1, org, 8, 15);
		count &= ~7;
	}
	count >>= 3;
	while (count--)
		particle2(pt_fade, color + (rand()&7), particletexture, 6, 128, 1, org, 8, 15);
}

// LordHavoc: added this for spawning sparks/dust (which have strong gravity)
/*
===============
R_SparkShower
===============
*/
void R_SparkShower (vec3_t org, vec3_t dir, int count)
{
	if (!r_particles.value) return; // LordHavoc: particles are optional

	// smoke puff
	particle(pt_smokecloud, 12+(rand()&3), smokeparticletexture[rand()&7], 8, 64, 99, org[0], org[1], org[2], 0, 0, 0);
	// sparks
	while(count--)
		particle2(pt_fallfadespark, ramp3[rand()%6], particletexture, 1, lhrandom(0, 255), 5, org, 4, 96);
}

void R_BloodPuff (vec3_t org)
{
	if (!r_particles.value) return; // LordHavoc: particles are optional

	particle(pt_bloodcloud, 68+(rand()&3), smokeparticletexture[rand()&7], 12, 128, 99, org[0], org[1], org[2], 0, 0, 0);
}

void R_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	int			j;
	particle_t	*p;
	vec3_t		diff;
	vec3_t		center;
	vec3_t		velscale;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	VectorSubtract(maxs, mins, diff);
	center[0] = (mins[0] + maxs[0]) * 0.5;
	center[1] = (mins[1] + maxs[1]) * 0.5;
	center[2] = (mins[2] + maxs[2]) * 0.5;
	// FIXME: change velspeed back to 2.0x after fixing mod
	velscale[0] = velspeed * 0.5 / diff[0];
	velscale[1] = velspeed * 0.5 / diff[1];
	velscale[2] = velspeed * 0.5 / diff[2];
	
	while (count--)
	{
		ALLOCPARTICLE

		p->texnum = smokeparticletexture[rand()&7];
		p->scale = lhrandom(6, 8);
		p->alpha = 96 + (rand()&63);
		p->die = cl.time + 2;
		p->type = pt_bloodcloud;
		p->color = (rand()&3)+68;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = diff[j] * (float) (rand()%1024) * (1.0 / 1024.0) + mins[j];
			p->vel[j] = (p->org[j] - center[j]) * velscale[j];
		}
	}
}

void R_ParticleCube (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int gravity, int randomvel)
{
	int			j;
	particle_t	*p;
	vec3_t		diff;
	float		t;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	VectorSubtract(maxs, mins, diff);
	
	while (count--)
	{
		ALLOCPARTICLE

		p->texnum = particletexture;
		p->scale = 6;
		p->alpha = 255;
		p->die = cl.time + 1 + (rand()&15)*0.0625;
		if (gravity)
			p->type = pt_grav;
		else
			p->type = pt_static;
		p->color = colorbase + (rand()&3);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = diff[j] * (float) (rand()&1023) * (1.0 / 1024.0) + mins[j];
			if (randomvel)
				p->vel[j] = dir[j] + (rand()%randomvel)-(randomvel*0.5);
			else
				p->vel[j] = 0;
		}
	}
}

void R_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type)
{
	int			i;
	particle_t	*p;
	vec3_t		diff;
	vec3_t		org;
	vec3_t		vel;
	float		t, z;
	if (!r_particles.value) return; // LordHavoc: particles are optional
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
	t += cl.time;

	VectorSubtract(maxs, mins, diff);
	
	for (i=0 ; i<count ; i++)
	{
		ALLOCPARTICLE

		vel[0] = dir[0] + (rand()&31) - 16;
		vel[1] = dir[1] + (rand()&31) - 16;
		vel[2] = dir[2] + (rand()&63) - 32;
		org[0] = diff[0] * (float) (rand()&1023) * (1.0 / 1024.0) + mins[0];
		org[1] = diff[1] * (float) (rand()&1023) * (1.0 / 1024.0) + mins[1];
		org[2] = z;

		p->alpha = 255;
		p->die = t;
		if (type == 1)
		{
			p->scale = 2;
			p->texnum = particletexture;
			p->type = pt_snow;
		}
		else // 0
		{
			p->scale = 3;
			p->texnum = rainparticletexture;
			p->type = pt_rain;
		}
		p->color = colorbase + (rand()&3);
		VectorCopy(org, p->org);
		VectorCopy(vel, p->vel);
		VectorCopy(vel, p->vel2);
	}
}


/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	for (i=-128 ; i<128 ; i+=16)
		for (j=-128 ; j<128 ; j+=16)
		{
			ALLOCPARTICLE
		
			p->texnum = particletexture;
			p->scale = 10;
			p->alpha = 128;
			p->die = cl.time + 2 + (rand()&31) * 0.02;
			p->color = 224 + (rand()&7);
			p->type = pt_grav;
			
			dir[0] = j + (rand()&7);
			dir[1] = i + (rand()&7);
			dir[2] = 256;

			p->org[0] = org[0] + dir[0];
			p->org[1] = org[1] + dir[1];
			p->org[2] = org[2] + (rand()&63);

			VectorNormalize (dir);						
			vel = 50 + (rand()&63);
			VectorScale (dir, vel, p->vel);
		}
}

/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=-24 ; k<32 ; k+=8)
			{
				ALLOCPARTICLE
		
				p->texnum = particletexture;
				p->scale = 1;
				p->alpha = lhrandom(32,128);
				p->die = cl.time + 5;
				p->color = 254;
				p->type = pt_fade;
				
				p->org[0] = org[0] + i + (rand()&7);
				p->org[1] = org[1] + j + (rand()&7);
				p->org[2] = org[2] + k + (rand()&7);
	
				p->vel[0] = i*2 + (rand()%25) - 12;
				p->vel[1] = j*2 + (rand()%25) - 12;
				p->vel[2] = k*2 + (rand()%25) - 12 + 40;
			}
}

void R_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent)
{
	vec3_t		vec;
	float		len, dec = 0, t, nt, speed;
	int			j, contents, bubbles;
	particle_t	*p;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	t = cl.oldtime;
	nt = cl.time;
	if (ent->trail_leftover < 0)
		ent->trail_leftover = 0;
	t += ent->trail_leftover;
	ent->trail_leftover -= (cl.time - cl.oldtime);
	if (t >= cl.time)
		return;

	contents = Mod_PointInLeaf(start, cl.worldmodel)->contents;
	if (contents == CONTENTS_SKY || contents == CONTENTS_LAVA)
		return;

	VectorSubtract (end, start, vec);
	len = VectorNormalizeLength (vec);
	if (len <= 0.01f)
		return;
	speed = len / (nt - t);

	bubbles = (contents == CONTENTS_WATER || contents == CONTENTS_SLIME);

	while (t < nt)
	{
		ALLOCPARTICLE
		
		p->vel[0] = p->vel[1] = p->vel[2] = 0;
		p->die = cl.time + 2;

		switch (type)
		{
			case 0:	// rocket trail
			case 1: // grenade trail
				if (bubbles)
				{
					dec = 0.005f;
					p->texnum = bubbleparticletexture;
					p->scale = lhrandom(1,2);
					p->alpha = 255;
					p->color = (rand()&3)+12;
					p->type = pt_bubble;
					p->die = cl.time + 2;
					for (j=0 ; j<3 ; j++)
					{
						p->vel[j] = (rand()&31)-16;
						p->org[j] = start[j] + ((rand()&3)-2);
					}
				}
				else
				{
					dec = 0.02f;
					p->texnum = smokeparticletexture[rand()&7];
					p->scale = lhrandom(8, 12);
					p->alpha = 64 + (rand()&31);
					p->color = (rand()&3)+12;
					p->type = pt_smoke;
					p->die = cl.time + 10000;
					VectorCopy(start, p->org);
				}
				break;

				/*
			case 1:	// smoke smoke
				dec = 0.016f;
				p->texnum = smokeparticletexture;
				p->scale = lhrandom(6,9);
				p->alpha = 64;
				if (r_smokecolor.value)
					p->color = r_smokecolor.value;
				else
					p->color = (rand()&3)+12;
				p->type = pt_smoke;
				p->die = cl.time + 1;
				VectorCopy(start, p->org);
				break;
				*/

			case 2:	// blood
			case 4:	// slight blood
				dec = 0.025f;
				p->texnum = smokeparticletexture[rand()&7];
				p->scale = lhrandom(6, 8);
				p->alpha = type == 4 ? 192 : 255;
				p->color = (rand()&3)+68;
				p->type = pt_bloodcloud;
				p->die = cl.time + 9999;
				for (j=0 ; j<3 ; j++)
				{
					p->vel[j] = (rand()&15)-8;
					p->org[j] = start[j] + ((rand()&3)-2);
				}
				break;

			case 3:
			case 5:	// tracer
				dec = 0.02f;
				p->texnum = smokeparticletexture[rand()&7];
				p->scale = 4;
				p->alpha = 64 + (rand()&31);
				p->color = type == 3 ? 56 : 234;
				p->type = pt_fade;
				p->die = cl.time + 10000;
				VectorCopy(start, p->org);
				break;

			case 6:	// voor trail
				dec = 0.05f; // sparse trail
				p->texnum = smokeparticletexture[rand()&7];
				p->scale = lhrandom(3, 5);
				p->alpha = 255;
				p->color = 9*16 + 8 + (rand()&3);
				p->type = pt_fade;
				p->die = cl.time + 2;
				for (j=0 ; j<3 ; j++)
				{
					p->vel[j] = (rand()&15)-8;
					p->org[j] = start[j] + ((rand()&3)-2);
				}
				break;

			case 7:	// Nehahra smoke tracer
				dec = 0.14f;
				p->texnum = smokeparticletexture[rand()&7];
				p->scale = lhrandom(8, 12);
				p->alpha = 64;
				p->color = (rand()&3)+12;
				p->type = pt_smoke;
				p->die = cl.time + 10000;
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()&3)-2);
				break;
		}
		
		t += dec;
		dec *= speed;
		VectorMA (start, dec, vec, start);
	}
	ent->trail_leftover = t - cl.time;
}

void R_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent)
{
	vec3_t		vec;
	int			len;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	VectorSubtract (end, start, vec);
	len = (int) (VectorNormalizeLength (vec) * (1.0f / 3.0f));
	VectorScale(vec, 3, vec);
	while (len--)
	{
		particle(pt_smoke, color, particletexture, 8, 192, 99, start[0], start[1], start[2], 0, 0, 0);
		VectorAdd (start, vec, start);
	}
}


extern qboolean lighthalf;

/*
===============
R_DrawParticles
===============
*/
extern	cvar_t	sv_gravity;
void R_CompleteLightPoint (vec3_t color, vec3_t p);

void TraceLine (vec3_t start, vec3_t end, vec3_t impact);

void R_DrawParticles (void)
{
	particle_t		*p;
	int				i, r,g,b,a;
	float			gravity, dvel, frametime, scale, scale2, minparticledist;
	byte			*color24;
	vec3_t			up, right, uprightangles, forward2, up2, right2, tempcolor, v;
	int				activeparticles, maxparticle, j, k;

	// LordHavoc: early out condition
	if (!numparticles)
		return;

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	uprightangles[0] = 0;
	uprightangles[1] = r_refdef.viewangles[1];
	uprightangles[2] = 0;
	AngleVectors (uprightangles, forward2, right2, up2);

	frametime = cl.time - cl.oldtime;
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;

	minparticledist = DotProduct(r_refdef.vieworg, vpn) + 16.0f;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	for (k = 0, p = particles;k < numparticles;k++, p++)
	{
		if (p->die < cl.time)
		{
			freeparticles[j++] = p;
			continue;
		}
		maxparticle = k;
		activeparticles++;

		// LordHavoc: only render if not too close
		if (DotProduct(p->org, vpn) >= minparticledist)
		{
			color24 = (byte *) &d_8to24table[(int)p->color];
			r = color24[0];
			g = color24[1];
			b = color24[2];
			a = p->alpha;
			if (r_dynamicparticles.value)
			{
				R_CompleteLightPoint(tempcolor, p->org);
				r = (r * (int) tempcolor[0]) >> 7;
				g = (g * (int) tempcolor[1]) >> 7;
				b = (b * (int) tempcolor[2]) >> 7;
			}
			transpolybegin(p->texnum, 0, p->texnum, TPOLYTYPE_ALPHA);
			scale = p->scale * -0.5;scale2 = p->scale * 0.5;
			if (p->texnum == rainparticletexture) // rain streak
			{
				transpolyvert(p->org[0] + up2[0]*scale  + right2[0]*scale , p->org[1] + up2[1]*scale  + right2[1]*scale , p->org[2] + up2[2]*scale  + right2[2]*scale , 0,1,r,g,b,a);
				transpolyvert(p->org[0] + up2[0]*scale2 + right2[0]*scale , p->org[1] + up2[1]*scale2 + right2[1]*scale , p->org[2] + up2[2]*scale2 + right2[2]*scale , 0,0,r,g,b,a);
				transpolyvert(p->org[0] + up2[0]*scale2 + right2[0]*scale2, p->org[1] + up2[1]*scale2 + right2[1]*scale2, p->org[2] + up2[2]*scale2 + right2[2]*scale2, 1,0,r,g,b,a);
				transpolyvert(p->org[0] + up2[0]*scale  + right2[0]*scale2, p->org[1] + up2[1]*scale  + right2[1]*scale2, p->org[2] + up2[2]*scale  + right2[2]*scale2, 1,1,r,g,b,a);
			}
			else
			{
				transpolyvert(p->org[0] + up[0]*scale  + right[0]*scale , p->org[1] + up[1]*scale  + right[1]*scale , p->org[2] + up[2]*scale  + right[2]*scale , 0,1,r,g,b,a);
				transpolyvert(p->org[0] + up[0]*scale2 + right[0]*scale , p->org[1] + up[1]*scale2 + right[1]*scale , p->org[2] + up[2]*scale2 + right[2]*scale , 0,0,r,g,b,a);
				transpolyvert(p->org[0] + up[0]*scale2 + right[0]*scale2, p->org[1] + up[1]*scale2 + right[1]*scale2, p->org[2] + up[2]*scale2 + right[2]*scale2, 1,0,r,g,b,a);
				transpolyvert(p->org[0] + up[0]*scale  + right[0]*scale2, p->org[1] + up[1]*scale  + right[1]*scale2, p->org[2] + up[2]*scale  + right[2]*scale2, 1,1,r,g,b,a);
			}
			transpolyend();
		}

		VectorCopy(p->org, p->oldorg);
		p->org[0] += p->vel[0]*frametime;
		p->org[1] += p->vel[1]*frametime;
		p->org[2] += p->vel[2]*frametime;
		
		switch (p->type)
		{
		case pt_static:
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] *= dvel;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] *= dvel;
			break;

		case pt_grav:
			p->vel[2] -= gravity;
			break;
// LordHavoc: for smoke trails
		case pt_smoke:
			p->scale += frametime * 6;
			p->alpha -= frametime * 128;
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
			break;
		case pt_bloodcloud:
//			if (Mod_PointInLeaf(p->org, cl.worldmodel)->contents != CONTENTS_EMPTY)
//			{
//				p->die = -1;
//				break;
//			}
			p->scale += frametime * 4;
			p->alpha -= frametime * 64;
			if (p->alpha < 1 || p->scale < 1)
				p->die = -1;
			break;
		case pt_fallfadespark:
			p->alpha -= frametime * 256;
			p->vel[2] -= gravity;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_fade:
			p->alpha -= frametime * 512;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_bubble:
			a = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (a != CONTENTS_WATER && a != CONTENTS_SLIME)
			{
				p->texnum = smokeparticletexture[rand()&7];
				p->type = pt_splash;
				p->alpha = 96;
				p->scale = 5;
				p->vel[0] = p->vel[1] = p->vel[2] = 0;
				p->die = cl.time + 1000;
//				p->die = -1;
			}
			p->vel[2] += gravity * 0.25;
			p->vel[0] *= (1 - (frametime * 0.0625));
			p->vel[1] *= (1 - (frametime * 0.0625));
			p->vel[2] *= (1 - (frametime * 0.0625));
			if (cl.time > p->time2)
			{
				p->time2 = cl.time + lhrandom(0, 0.5);
				p->vel[0] += lhrandom(-32,32);
				p->vel[1] += lhrandom(-32,32);
				p->vel[2] += lhrandom(-32,32);
			}
			p->alpha -= frametime * 64;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_smokecloud:
			p->scale += frametime * 60;
			p->alpha -= frametime * 96;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_splash:
			p->scale += frametime * 24;
			p->alpha -= frametime * 256;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_rain:
			a = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
			{
				if (a == CONTENTS_SOLID && Mod_PointInLeaf(p->oldorg, cl.worldmodel)->contents == CONTENTS_SOLID)
					break; // still in solid
				p->die = cl.time + 1000;
				switch (a)
				{
				case CONTENTS_LAVA:
				case CONTENTS_SLIME:
					p->texnum = smokeparticletexture[rand()&7];
					p->type = pt_smokecloud;
					p->alpha = 64;
					p->vel[2] = 96;
					break;
				case CONTENTS_WATER:
					p->texnum = smokeparticletexture[rand()&7];
					p->type = pt_splash;
					p->alpha = 96;
					p->scale = 5;
					p->vel[0] = p->vel[1] = p->vel[2] = 0;
//					p->texnum = bubbleparticletexture;
//					p->type = pt_bubble;
//					p->vel[2] *= 0.1;
					break;
				default: // CONTENTS_SOLID and any others
					TraceLine(p->oldorg, p->org, v);
					VectorCopy(v, p->org);
					p->texnum = smokeparticletexture[rand()&7];
					p->type = pt_splash;
					p->alpha = 96;
					p->scale = 5;
					p->vel[0] = p->vel[1] = p->vel[2] = 0;
					p->die = cl.time + 1000;
					break;
				}
			}
			break;
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
	numparticles = activeparticles;
}

