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

#define MAX_PARTICLES			16384	// default max # of particles at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

typedef enum
{
	pt_static, pt_grav, pt_blob, pt_blob2, pt_bulletsmoke, pt_smoke, pt_snow, pt_rain, pt_spark, pt_bubble, pt_fade, pt_steam, pt_splash, pt_splashpuff, pt_flame, pt_blood, pt_oneframe, pt_lavasplash, pt_raindropsplash, pt_underwaterspark, pt_explosionsplash
}
ptype_t;

typedef struct
{
	float s1, t1, s2, t2;
}
particletexture_t;

typedef struct particle_s
{
	ptype_t		type;
	vec3_t		org;
	vec3_t		vel;
	particletexture_t	*tex;
	float		die;
	float		scale;
	float		alpha; // 0-255
	float		time2; // used for various things (snow fluttering, for example)
	float		bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	vec3_t		oldorg;
	vec3_t		vel2; // used for snow fluttering (base velocity, wind for instance)
	float		friction; // how much air friction affects this object (objects with a low mass/size ratio tend to get more air friction)
	float		pressure; // if non-zero, apply pressure to other particles
	int			dynlight; // if set the particle will be dynamically lit (if r_dynamicparticles is on), used for smoke and blood
	int			rendermode; // a TPOLYTYPE_ value
	byte		color[4];
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

static rtexture_t *particlefonttexture;

static particletexture_t particletexture;
static particletexture_t smokeparticletexture[8];
static particletexture_t rainparticletexture;
static particletexture_t bubbleparticletexture;
static particletexture_t bulletholetexture[8];
static particletexture_t rocketglowparticletexture;
static particletexture_t raindropsplashparticletexture[16];

static particle_t	*particles;
static int			r_numparticles;

static int			numparticles;
static particle_t	**freeparticles; // list used only in compacting particles array

static cvar_t r_particles = {CVAR_SAVE, "r_particles", "1"};
static cvar_t r_drawparticles = {0, "r_drawparticles", "1"};
static cvar_t r_particles_lighting = {CVAR_SAVE, "r_particles_lighting", "1"};
static cvar_t r_particles_bloodshowers = {CVAR_SAVE, "r_particles_bloodshowers", "1"};
static cvar_t r_particles_blood = {CVAR_SAVE, "r_particles_blood", "1"};
static cvar_t r_particles_smoke = {CVAR_SAVE, "r_particles_smoke", "1"};
static cvar_t r_particles_sparks = {CVAR_SAVE, "r_particles_sparks", "1"};
static cvar_t r_particles_bubbles = {CVAR_SAVE, "r_particles_bubbles", "1"};
static cvar_t r_particles_explosions = {CVAR_SAVE, "r_particles_explosions", "0"};

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

static void R_InitParticleTexture (void)
{
	int		x,y,d,i,m, texnum;
	float	dx, dy, radius, f, f2;
	byte	data[32][32][4], noise1[64][64], noise2[64][64];
	vec3_t	light;
	byte	particletexturedata[256][256][4];

	memset(&particletexturedata[0][0][0], 255, sizeof(particletexturedata));
	texnum = 0;
	#define SETUPTEX(var)\
	{\
		int basex, basey, y;\
		if (texnum >= 64)\
		{\
			Sys_Error("R_InitParticleTexture: ran out of textures (64)\n");\
			return; /* only to hush compiler */ \
		}\
		basex = (texnum & 7) * 32;\
		basey = ((texnum >> 3) & 7) * 32;\
		var.s1 = (basex + 1) / 256.0f;\
		var.t1 = (basey + 1) / 256.0f;\
		var.s2 = (basex + 31) / 256.0f;\
		var.t2 = (basey + 31) / 256.0f;\
		for (y = 0;y < 32;y++)\
			memcpy(&particletexturedata[basey + y][basex][0], &data[y][0][0], 32*4);\
		texnum++;\
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
	SETUPTEX(particletexture)
//	particletexture = R_LoadTexture ("particletexture", 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);

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

		SETUPTEX(smokeparticletexture[i])
//		smokeparticletexture[i] = R_LoadTexture (va("smokeparticletexture%02d", i), 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
	}

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
	SETUPTEX(rainparticletexture)
//	rainparticletexture = R_LoadTexture ("rainparticletexture", 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);

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
	SETUPTEX(bubbleparticletexture)
//	bubbleparticletexture = R_LoadTexture ("bubbleparticletexture", 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);

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

		SETUPTEX(bulletholetexture[i])
//		bulletholetexture[i] = R_LoadTexture (va("bulletholetexture%02d", i), 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
	}

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
	SETUPTEX(rocketglowparticletexture)
//	rocketglowparticletexture = R_LoadTexture ("glowparticletexture", 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);

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
		SETUPTEX(raindropsplashparticletexture[i])
//		raindropsplashparticletexture[i] = R_LoadTexture (va("raindropslashparticletexture%02d", i), 32, 32, &data[0][0][0], TEXF_MIPMAP | TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
	}

	particlefonttexture = R_LoadTexture ("particlefont", 256, 256, &particletexturedata[0][0][0], TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
}

static void r_part_start(void)
{
	particles = (particle_t *) qmalloc(r_numparticles * sizeof(particle_t));
	freeparticles = (void *) qmalloc(r_numparticles * sizeof(particle_t *));
	numparticles = 0;
	R_InitParticleTexture ();
}

static void r_part_shutdown(void)
{
	numparticles = 0;
	qfree(particles);
	qfree(freeparticles);
}

static void r_part_newmap(void)
{
	numparticles = 0;
}

/*
===============
R_InitParticles
===============
*/
void R_ReadPointFile_f (void);
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

	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable (&r_particles);
	Cvar_RegisterVariable (&r_drawparticles);
	Cvar_RegisterVariable (&r_particles_lighting);
	Cvar_RegisterVariable (&r_particles_bloodshowers);
	Cvar_RegisterVariable (&r_particles_blood);
	Cvar_RegisterVariable (&r_particles_smoke);
	Cvar_RegisterVariable (&r_particles_sparks);
	Cvar_RegisterVariable (&r_particles_bubbles);
	Cvar_RegisterVariable (&r_particles_explosions);

	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown, r_part_newmap);
}

//void particle(int ptype, int pcolor, int ptex, int prendermode, int plight, float pscale, float palpha, float ptime, float pbounce, float px, float py, float pz, float pvx, float pvy, float pvz)
#define particle(ptype, pcolor, ptex, prendermode, plight, pscale, palpha, ptime, pbounce, px, py, pz, pvx, pvy, pvz, ptime2, pvx2, pvy2, pvz2, pfriction, ppressure)\
{\
	particle_t	*part;\
	int tempcolor;\
	if (numparticles >= r_numparticles)\
		return;\
	part = &particles[numparticles++];\
	part->type = (ptype);\
	tempcolor = (pcolor);\
	part->color[0] = ((tempcolor) >> 16) & 0xFF;\
	part->color[1] = ((tempcolor) >> 8) & 0xFF;\
	part->color[2] = (tempcolor) & 0xFF;\
	part->color[3] = 0xFF;\
	part->tex = (&ptex);\
	part->dynlight = (plight);\
	part->rendermode = (prendermode);\
	part->scale = (pscale);\
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
}

/*
===============
R_EntityParticles
===============
*/
void R_EntityParticles (entity_t *ent)
{
	int			i;
	float		angle;
	float		sp, sy, cp, cy;
	vec3_t		forward;
	float		dist;
	float		beamlength;
	static vec3_t avelocities[NUMVERTEXNORMALS];
	if (!r_particles.value) return; // LordHavoc: particles are optional

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

		particle(pt_oneframe, particlepalette[0x6f], particletexture, TPOLYTYPE_ALPHA, false, 2, 255, 9999, 0, ent->render.origin[0] + m_bytenormals[i][0]*dist + forward[0]*beamlength, ent->render.origin[1] + m_bytenormals[i][1]*dist + forward[1]*beamlength, ent->render.origin[2] + m_bytenormals[i][2]*dist + forward[2]*beamlength, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
}


void R_ReadPointFile_f (void)
{
	QFile	*f;
	vec3_t	org;
	int		r;
	int		c;
	char	name[MAX_OSPATH];

	sprintf (name,"maps/%s.pts", sv.name);

	COM_FOpenFile (name, &f, false, true);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;)
	{
	    	char *str = Qgetline (f);
		r = sscanf (str,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (numparticles >= r_numparticles)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}
		particle(pt_static, particlepalette[(-c)&15], particletexture, TPOLYTYPE_ALPHA, false, 2, 255, 99999, 0, org[0], org[1], org[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	Qclose (f);
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
	int i, j;
	float f;
	vec3_t v, end, ang;
	byte noise1[32*32], noise2[32*32];

	if (r_particles.value && r_particles_explosions.value)
	{
		i = Mod_PointInLeaf(org, cl.worldmodel)->contents;
		if (i == CONTENTS_SLIME || i == CONTENTS_WATER)
		{
			for (i = 0;i < 128;i++)
				particle(pt_bubble, 0xFFFFFF, bubbleparticletexture, TPOLYTYPE_ALPHA, false, lhrandom(1, 2), 255, 9999, 1.5, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-96, 96), lhrandom(-96, 96), lhrandom(-96, 96), 0, 0, 0, 0, 0, 0);

			ang[2] = lhrandom(0, 360);
			fractalnoise(noise1, 32, 4);
			fractalnoise(noise2, 32, 8);
			for (i = 0;i < 32;i++)
			{
				for (j = 0;j < 32;j++)
				{
					VectorRandom(v);
					VectorMA(org, 16, v, v);
					TraceLine(org, v, end, NULL, 0);
					ang[0] = (j + 0.5f) * (360.0f / 32.0f);
					ang[1] = (i + 0.5f) * (360.0f / 32.0f);
					AngleVectors(ang, v, NULL, NULL);
					f = noise1[j*32+i] * 1.5f;
					VectorScale(v, f, v);
					particle(pt_underwaterspark, noise2[j*32+i] * 0x010101, smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, false, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2], 512.0f, 0, 0, 0, 2, 0);
					VectorScale(v, 0.75, v);
					particle(pt_underwaterspark, explosparkramp[(noise2[j*32+i] >> 5)], particletexture, TPOLYTYPE_ALPHA, false, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2], 512.0f, 0, 0, 0, 2, 0);
				}
			}
		}
		else
		{
			ang[2] = lhrandom(0, 360);
			fractalnoise(noise1, 32, 4);
			fractalnoise(noise2, 32, 8);
			for (i = 0;i < 32;i++)
			{
				for (j = 0;j < 32;j++)
				{
					VectorRandom(v);
					VectorMA(org, 16, v, v);
					TraceLine(org, v, end, NULL, 0);
					ang[0] = (j + 0.5f) * (360.0f / 32.0f);
					ang[1] = (i + 0.5f) * (360.0f / 32.0f);
					AngleVectors(ang, v, NULL, NULL);
					f = noise1[j*32+i] * 1.5f;
					VectorScale(v, f, v);
					particle(pt_spark, noise2[j*32+i] * 0x010101, smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, false, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
					VectorScale(v, 0.75, v);
					particle(pt_spark, explosparkramp[(noise2[j*32+i] >> 5)], particletexture, TPOLYTYPE_ALPHA, false, 10, lhrandom(128, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
				//	VectorRandom(v);
				//	VectorScale(v, 384, v);
				//	particle(pt_spark, explosparkramp[rand()&7], particletexture, TPOLYTYPE_ALPHA, false, 2, lhrandom(16, 255), 9999, 1.5, end[0], end[1], end[2], v[0], v[1], v[2] + 160.0f, 512.0f, 0, 0, 0, 2, 0);
				}
			}
		}
	}
	else
		R_NewExplosion(org);
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
		particle(pt_fade, particlepalette[colorStart + (i % colorLength)], particletexture, TPOLYTYPE_ALPHA, false, 1.5, 255, 0.3, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-192, 192), lhrandom(-192, 192), lhrandom(-192, 192), 0, 0, 0, 0, 0.1f, 0);
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

	for (i = 0;i < 256;i++)
		particle(pt_blob , particlepalette[ 66+(rand()%6)], particletexture, TPOLYTYPE_ALPHA, false, 4, 255, 9999, 0, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(-128, 128), 0, 0, 0, 0, 0, 0);
	for (i = 0;i < 256;i++)
		particle(pt_blob2, particlepalette[150+(rand()%6)], particletexture, TPOLYTYPE_ALPHA, false, 4, 255, 9999, 0, org[0] + lhrandom(-16, 16), org[1] + lhrandom(-16, 16), org[2] + lhrandom(-16, 16), lhrandom(-4, 4), lhrandom(-4, 4), lhrandom(-128, 128), 0, 0, 0, 0, 0, 0);
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
	while (count--)
		particle(pt_fade, particlepalette[color + (rand()&7)], particletexture, TPOLYTYPE_ALPHA, false, 1, 128, 9999, 0, org[0] + lhrandom(-8, 8), org[1] + lhrandom(-8, 8), org[2] + lhrandom(-8, 8), lhrandom(-15, 15), lhrandom(-15, 15), lhrandom(-15, 15), 0, 0, 0, 0, 0, 0);
}

// LordHavoc: added this for spawning sparks/dust (which have strong gravity)
/*
===============
R_SparkShower
===============
*/
void R_SparkShower (vec3_t org, vec3_t dir, int count)
{
	particletexture_t *tex;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	tex = &bulletholetexture[rand()&7];
	R_Decal(org, particlefonttexture, tex->s1, tex->t1, tex->s2, tex->t2, 16, 0, 0, 0, 255);

	// smoke puff
	if (r_particles_smoke.value)
		particle(pt_bulletsmoke, 0xA0A0A0, smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, true, 5, 255, 9999, 0, org[0], org[1], org[2], lhrandom(-8, 8), lhrandom(-8, 8), lhrandom(0, 16), 0, 0, 0, 0, 0, 0);

	if (r_particles_sparks.value)
	{
		// sparks
		while(count--)
			particle(pt_spark, particlepalette[0x68 + (rand() & 7)], particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(0, 255), 9999, 1.5, org[0], org[1], org[2], lhrandom(-64, 64), lhrandom(-64, 64), lhrandom(0, 128), 512.0f, 0, 0, 0, 0.2f, 0);
	}
}

void R_BloodPuff (vec3_t org, vec3_t vel, int count)
{
	// bloodcount is used to accumulate counts too small to cause a blood particle
	static int bloodcount = 0;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (!r_particles_blood.value) return;

	if (count > 100)
		count = 100;
	bloodcount += count;
	while(bloodcount >= 10)
	{
		particle(pt_blood, 0x300000, smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, true, 24, 255, 9999, -1, org[0], org[1], org[2], vel[0] + lhrandom(-64, 64), vel[1] + lhrandom(-64, 64), vel[2] + lhrandom(-64, 64), 0, 0, 0, 0, 1.0f, 0);
		bloodcount -= 10;
	}
}

void R_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count)
{
	vec3_t		diff;
	vec3_t		center;
	vec3_t		velscale;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (!r_particles_bloodshowers.value) return;
	if (!r_particles_blood.value) return;

	VectorSubtract(maxs, mins, diff);
	center[0] = (mins[0] + maxs[0]) * 0.5;
	center[1] = (mins[1] + maxs[1]) * 0.5;
	center[2] = (mins[2] + maxs[2]) * 0.5;
	// FIXME: change velspeed back to 2.0x after fixing mod
	velscale[0] = velspeed * 2.0 / diff[0];
	velscale[1] = velspeed * 2.0 / diff[1];
	velscale[2] = velspeed * 2.0 / diff[2];

	while (count--)
	{
		vec3_t org, vel;
		org[0] = lhrandom(mins[0], maxs[0]);
		org[1] = lhrandom(mins[1], maxs[1]);
		org[2] = lhrandom(mins[2], maxs[2]);
		vel[0] = (org[0] - center[0]) * velscale[0];
		vel[1] = (org[1] - center[1]) * velscale[1];
		vel[2] = (org[2] - center[2]) * velscale[2];
		particle(pt_blood, 0x300000, smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, true, 24, 255, 9999, -1, org[0], org[1], org[2], vel[0], vel[1], vel[2], 0, 0, 0, 0, 1.0f, 0);
	}
}

void R_ParticleCube (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int gravity, int randomvel)
{
	float		t;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	while (count--)
		particle(gravity ? pt_grav : pt_static, particlepalette[colorbase + (rand()&3)], particletexture, TPOLYTYPE_ALPHA, false, 2, 255, lhrandom(1, 2), 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0] + lhrandom(-randomvel, randomvel), dir[1] + lhrandom(-randomvel, randomvel), dir[2] + lhrandom(-randomvel, randomvel), 0, 0, 0, 0, 0, 0);
}

void R_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type)
{
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

	switch(type)
	{
	case 0:
		while(count--)
		{
			vel[0] = dir[0] + lhrandom(-16, 16);
			vel[1] = dir[1] + lhrandom(-16, 16);
			vel[2] = dir[2] + lhrandom(-32, 32);
			particle(pt_rain, particlepalette[colorbase + (rand()&3)], rainparticletexture, TPOLYTYPE_ALPHA, true, 3, 255, t, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), z, vel[0], vel[1], vel[2], 0, vel[0], vel[1], vel[2], 0, 0);
		}
		break;
	case 1:
		while(count--)
		{
			vel[0] = dir[0] + lhrandom(-16, 16);
			vel[1] = dir[1] + lhrandom(-16, 16);
			vel[2] = dir[2] + lhrandom(-32, 32);
			particle(pt_snow, particlepalette[colorbase + (rand()&3)], particletexture, TPOLYTYPE_ALPHA, false, 2, 255, t, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), z, vel[0], vel[1], vel[2], 0, vel[0], vel[1], vel[2], 0, 0);
		}
		break;
	default:
		Host_Error("R_ParticleRain: unknown type %i (0 = rain, 1 = snow)\n", type);
	}
}

void R_FlameCube (vec3_t mins, vec3_t maxs, int count)
{
	float		t;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (maxs[0] <= mins[0]) {t = mins[0];mins[0] = maxs[0];maxs[0] = t;}
	if (maxs[1] <= mins[1]) {t = mins[1];mins[1] = maxs[1];maxs[1] = t;}
	if (maxs[2] <= mins[2]) {t = mins[2];mins[2] = maxs[2];maxs[2] = t;}

	while (count--)
		particle(pt_flame, particlepalette[224 + (rand()&15)], smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, false, 8, 255, 9999, 1.1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), lhrandom(-32, 32), lhrandom(-32, 32), lhrandom(-32, 64), 0, 0, 0, 0, 0.1f, 0);
}

void R_Flames (vec3_t org, vec3_t vel, int count)
{
	if (!r_particles.value) return; // LordHavoc: particles are optional

	while (count--)
		particle(pt_flame, particlepalette[224 + (rand()&15)], smokeparticletexture[rand()&7], TPOLYTYPE_ALPHA, false, 8, 255, 9999, 1.1, org[0], org[1], org[2], vel[0] + lhrandom(-128, 128), vel[1] + lhrandom(-128, 128), vel[2] + lhrandom(-128, 128), 0, 0, 0, 0, 0.1f, 0);
}



/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t origin)
{
	int			i, j;
	float		vel;
	vec3_t		dir, org;
	if (!r_particles.value) return; // LordHavoc: particles are optional

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
			particle(pt_lavasplash, particlepalette[224 + (rand()&7)], particletexture, TPOLYTYPE_ALPHA, false, 7, 255, 9999, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0, 0, 0, 0);
		}
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
	if (!r_particles.value) return; // LordHavoc: particles are optional

	for (i=-16 ; i<16 ; i+=8)
		for (j=-16 ; j<16 ; j+=8)
			for (k=-24 ; k<32 ; k+=8)
				particle(pt_fade, 0xFFFFFF, particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(64, 128), 9999, 0, org[0] + i + lhrandom(0, 8), org[1] + j + lhrandom(0, 8), org[2] + k + lhrandom(0, 8), i*2 + lhrandom(-12.5, 12.5), j*2 + lhrandom(-12.5, 12.5), k*2 + lhrandom(27.5, 52.5), 0, 0, 0, 0, 0.1f, -512.0f);
}

void R_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent)
{
	vec3_t		vec, dir, vel;
	float		len, dec = 0, speed;
	int			contents, bubbles, polytype;
	double		t;
	if (!r_particles.value) return; // LordHavoc: particles are optional

	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	if (type == 0 && host_frametime != 0) // rocket glow
		particle(pt_oneframe, 0xFFFFFF, rocketglowparticletexture, TPOLYTYPE_ALPHA, false, 24, 255, 9999, 0, end[0] - 12 * dir[0], end[1] - 12 * dir[1], end[2] - 12 * dir[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);

	t = ent->persistent.trail_time;
	if (t >= cl.time)
		return; // no particles to spawn this frame (sparse trail)

	if (t < cl.oldtime)
		t = cl.oldtime;

	VectorSubtract (end, start, vec);
	len = VectorNormalizeLength (vec);
	if (len <= 0.01f)
	{
		// advance the trail time
		ent->persistent.trail_time = cl.time;
		return;
	}
	speed = len / (cl.time - cl.oldtime);
	VectorScale(vec, speed, vel);

	// advance into this frame to reach the first puff location
	dec = t - cl.oldtime;
	dec *= speed;
	VectorMA(start, dec, vec, start);

	contents = Mod_PointInLeaf(start, cl.worldmodel)->contents;
	if (contents == CONTENTS_SKY || contents == CONTENTS_LAVA)
	{
		// advance the trail time
		ent->persistent.trail_time = cl.time;
		return;
	}

	bubbles = (contents == CONTENTS_WATER || contents == CONTENTS_SLIME);

	polytype = TPOLYTYPE_ALPHA;
	if (ent->render.effects & EF_ADDITIVE)
		polytype = TPOLYTYPE_ADD;

	while (t < cl.time)
	{
		switch (type)
		{
			case 0:	// rocket trail
				if (!r_particles_smoke.value)
					dec = cl.time - t;
				else if (bubbles && r_particles_bubbles.value)
				{
					dec = 0.005f;
					particle(pt_bubble, 0xFFFFFF, bubbleparticletexture, polytype, false, lhrandom(1, 2), 255, 9999, 1.5, start[0], start[1], start[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					particle(pt_bubble, 0xFFFFFF, bubbleparticletexture, polytype, false, lhrandom(1, 2), 255, 9999, 1.5, start[0], start[1], start[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					particle(pt_smoke, 0xFFFFFF, smokeparticletexture[rand()&7], polytype, false, 2, 160, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				else
				{
					dec = 0.005f;
					particle(pt_smoke, 0xC0C0C0, smokeparticletexture[rand()&7], polytype, true, 2, 160, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
					//particle(pt_spark, particlepalette[0x68 + (rand() & 7)], particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(128, 255), 9999, 1.5, start[0], start[1], start[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 0.1f, 0);
					//particle(pt_spark, particlepalette[0x68 + (rand() & 7)], particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(128, 255), 9999, 1.5, start[0], start[1], start[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 0.1f, 0);
					//particle(pt_spark, particlepalette[0x68 + (rand() & 7)], particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(128, 255), 9999, 1.5, start[0], start[1], start[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 0.1f, 0);
					//particle(pt_spark, particlepalette[0x68 + (rand() & 7)], particletexture, TPOLYTYPE_ALPHA, false, 1, lhrandom(128, 255), 9999, 1.5, start[0], start[1], start[2], lhrandom(-64, 64) - vel[0] * 0.0625, lhrandom(-64, 64) - vel[1] * 0.0625, lhrandom(-64, 64) - vel[2] * 0.0625, 512.0f, 0, 0, 0, 0.1f, 0);
				}
				break;

			case 1: // grenade trail
				// FIXME: make it gradually stop smoking
				if (!r_particles_smoke.value)
					dec = cl.time - t;
				else if (bubbles && r_particles_bubbles.value)
				{
					dec = 0.02f;
					particle(pt_bubble, 0xFFFFFF, bubbleparticletexture, polytype, false, lhrandom(1, 2), 255, 9999, 1.5, start[0], start[1], start[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					particle(pt_bubble, 0xFFFFFF, bubbleparticletexture, polytype, false, lhrandom(1, 2), 255, 9999, 1.5, start[0], start[1], start[2], lhrandom(-16, 16), lhrandom(-16, 16), lhrandom(-16, 16), 0, 0, 0, 0, 0, 0);
					particle(pt_smoke, 0xFFFFFF, smokeparticletexture[rand()&7], polytype, false, 2, 160, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				else
				{
					dec = 0.02f;
					particle(pt_smoke, 0x808080, smokeparticletexture[rand()&7], polytype, true, 2, 160, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				break;


			case 2:	// blood
				if (!r_particles_blood.value)
					dec = cl.time - t;
				else
				{
					dec = 0.1f;
					particle(pt_blood, 0x300000, smokeparticletexture[rand()&7], polytype, true, 24, 255, 9999, -1, start[0], start[1], start[2], vel[0] + lhrandom(-64, 64), vel[1] + lhrandom(-64, 64), vel[2] + lhrandom(-64, 64), 0, 0, 0, 0, 1.0f, 0);
				}
				break;

			case 4:	// slight blood
				if (!r_particles_blood.value)
					dec = cl.time - t;
				else
				{
					dec = 0.15f;
					particle(pt_blood, 0x300000, smokeparticletexture[rand()&7], polytype, true, 24, 255, 9999, -1, start[0], start[1], start[2], vel[0] + lhrandom(-64, 64), vel[1] + lhrandom(-64, 64), vel[2] + lhrandom(-64, 64), 0, 0, 0, 0, 1.0f, 0);
				}
				break;

			case 3:	// green tracer
				dec = 0.02f;
				particle(pt_fade, 0x373707, smokeparticletexture[rand()&7], polytype, false, 4, 255, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				break;

			case 5:	// flame tracer
				dec = 0.02f;
				particle(pt_fade, 0xCF632B, smokeparticletexture[rand()&7], polytype, false, 4, 255, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				break;

			case 6:	// voor trail
				dec = 0.05f; // sparse trail
				particle(pt_fade, 0x47232B, smokeparticletexture[rand()&7], polytype, false, 4, 255, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				break;

			case 7:	// Nehahra smoke tracer
				if (!r_particles_smoke.value)
					dec = cl.time - t;
				else
				{
					dec = 0.14f;
					particle(pt_smoke, 0xC0C0C0, smokeparticletexture[rand()&7], polytype, true, 10, 64, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				break;
		}

		// advance to next time and position
		t += dec;
		dec *= speed;
		VectorMA (start, dec, vec, start);
	}
	ent->persistent.trail_time = t;
}

void R_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent)
{
	vec3_t		vec;
	int			len;
	if (!r_particles.value) return; // LordHavoc: particles are optional
	if (!r_particles_smoke.value) return;

	VectorSubtract (end, start, vec);
	len = (int) (VectorNormalizeLength (vec) * (1.0f / 3.0f));
	VectorScale(vec, 3, vec);
	color = particlepalette[color];
	while (len--)
	{
		particle(pt_smoke, color, particletexture, TPOLYTYPE_ALPHA, false, 8, 192, 9999, 0, start[0], start[1], start[2], 0, 0, 0, 0, 0, 0, 0, 0, 0);
		VectorAdd (start, vec, start);
	}
}


/*
===============
R_DrawParticles
===============
*/
void R_MoveParticles (void)
{
	particle_t		*p;
	int				i, activeparticles, maxparticle, j, a, b, pressureused = false;
	vec3_t			v, org, o, normal;
	float			gravity, dvel, frametime, f;

	// LordHavoc: early out condition
	if (!numparticles)
		return;

	frametime = cl.time - cl.oldtime;
	if (!frametime)
		return; // if absolutely still, don't update particles
	gravity = frametime * sv_gravity.value;
	dvel = 1+4*frametime;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;
	for (i = 0, p = particles;i < numparticles;i++, p++)
	{
		if (p->die < cl.time)
		{
			freeparticles[j++] = p;
			continue;
		}

		VectorCopy(p->org, p->oldorg);
		VectorMA(p->org, frametime, p->vel, p->org);
		if (p->friction)
		{
			f = 1.0f - (p->friction * frametime);
			VectorScale(p->vel, f, p->vel);
		}
		VectorCopy(p->org, org);
		if (p->bounce)
		{
			vec3_t normal;
			float dist;
			if (TraceLine(p->oldorg, p->org, v, normal, 0) < 1)
			{
				VectorCopy(v, p->org);
				if (p->bounce < 0)
				{
					R_Decal(v, particlefonttexture, p->tex->s1, p->tex->t1, p->tex->s2, p->tex->t2, p->scale, p->color[0], p->color[1], p->color[2], p->alpha);
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
			a = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (a != CONTENTS_EMPTY && a != CONTENTS_SKY)
			{
				vec3_t normal;
				if (a == CONTENTS_SOLID && Mod_PointInLeaf(p->oldorg, cl.worldmodel)->contents == CONTENTS_SOLID)
					break; // still in solid
				p->die = cl.time + 1000;
				p->vel[0] = p->vel[1] = p->vel[2] = 0;
				switch (a)
				{
				case CONTENTS_LAVA:
				case CONTENTS_SLIME:
					p->tex = &smokeparticletexture[rand()&7];
					p->type = pt_steam;
					p->alpha = 96;
					p->scale = 5;
					p->vel[2] = 96;
					break;
				case CONTENTS_WATER:
					p->tex = &smokeparticletexture[rand()&7];
					p->type = pt_splash;
					p->alpha = 96;
					p->scale = 5;
					p->vel[2] = 96;
					break;
				default: // CONTENTS_SOLID and any others
					TraceLine(p->oldorg, p->org, v, normal, 0);
					VectorCopy(v, p->org);
					p->tex = &smokeparticletexture[rand()&7];
					p->type = pt_fade;
					VectorClear(p->vel);
					break;
				}
			}
			break;
		case pt_blood:
			p->friction = 1;
			a = Mod_PointInLeaf(p->org, cl.worldmodel)->contents;
			if (a != CONTENTS_EMPTY)
			{
				if (a == CONTENTS_WATER || a == CONTENTS_SLIME)
				{
					p->friction = 5;
					p->scale += frametime * 32.0f;
					p->alpha -= frametime * 128.0f;
					p->vel[2] += gravity * 0.125f;
					if (p->alpha < 1)
						p->die = -1;
					break;
				}
				else
				{
					p->die = -1;
					break;
				}
			}
			p->vel[2] -= gravity * 0.5;
			break;
		case pt_spark:
			p->alpha -= frametime * p->time2;
			p->vel[2] -= gravity;
			if (p->alpha < 1)
				p->die = -1;
			else if (Mod_PointInLeaf(p->org, cl.worldmodel)->contents != CONTENTS_EMPTY)
				p->type = pt_underwaterspark;
			break;
		case pt_underwaterspark:
			if (Mod_PointInLeaf(p->org, cl.worldmodel)->contents == CONTENTS_EMPTY)
			{
				p->tex = &smokeparticletexture[rand()&7];
				p->color[0] = p->color[1] = p->color[2] = 255;
				p->scale = 16;
				p->type = pt_explosionsplash;
			}
			else
				p->vel[2] += gravity * 0.5f;
			p->alpha -= frametime * p->time2;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_explosionsplash:
			if (Mod_PointInLeaf(p->org, cl.worldmodel)->contents == CONTENTS_EMPTY)
				p->vel[2] -= gravity;
			else
				p->alpha = 0;
			p->scale += frametime * 64.0f;
			p->alpha -= frametime * 1024.0f;
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
				p->tex = &smokeparticletexture[rand()&7];
				p->type = pt_splashpuff;
				p->scale = 4;
				p->vel[0] = p->vel[1] = p->vel[2] = 0;
				break;
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
			p->alpha -= frametime * 256;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_bulletsmoke:
			p->scale += frametime * 16;
			p->alpha -= frametime * 1024;
			p->vel[2] += gravity * 0.1;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_smoke:
			p->scale += frametime * 24;
			p->alpha -= frametime * 256;
			p->vel[2] += gravity * 0.1;
			if (p->alpha < 1)
				p->die = -1;
			break;
		case pt_steam:
			p->scale += frametime * 48;
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
			f = 0;
			b = Mod_PointInLeaf(p->oldorg, cl.worldmodel)->contents;
			VectorCopy(p->oldorg, o);
			while (f < 1)
			{
				a = b;
				f = TraceLine(o, p->org, v, normal, a);
				b = traceline_endcontents;
				if (f < 1 && b != CONTENTS_EMPTY && b != CONTENTS_SKY)
				{
					p->die = cl.time + 1000;
					p->vel[0] = p->vel[1] = p->vel[2] = 0;
					VectorCopy(v, p->org);
					switch (b)
					{
					case CONTENTS_LAVA:
					case CONTENTS_SLIME:
						p->tex = &smokeparticletexture[rand()&7];
						p->type = pt_steam;
						p->scale = 3;
						p->vel[2] = 96;
						break;
					default: // water, solid, and anything else
						p->tex = &raindropsplashparticletexture[0];
						p->time2 = 0;
						VectorCopy(normal, p->vel2);
					//	VectorAdd(p->org, normal, p->org);
						p->type = pt_raindropsplash;
						p->scale = 8;
						break;
					}
				}
			}
			break;
		case pt_raindropsplash:
			p->time2 += frametime * 64.0f;
			if (p->time2 >= 16.0f)
			{
				p->die = -1;
				break;
			}
			p->tex = &raindropsplashparticletexture[(int) p->time2];
			break;
		case pt_flame:
			p->alpha -= frametime * 512;
			p->vel[2] += gravity;
			if (p->alpha < 16)
				p->die = -1;
			break;
		case pt_oneframe:
			if (p->time2)
				p->die = -1;
			p->time2 = 1;
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

	if (pressureused)
	{
		activeparticles = 0;
		for (i = 0, p = particles;i < numparticles;i++, p++)
			if (p->pressure)
				freeparticles[activeparticles++] = p;

		if (activeparticles)
		{
			for (i = 0, p = particles;i < numparticles;i++, p++)
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
							dist = freeparticles[j]->scale * 4.0f * frametime / sqrt(dist);
							VectorMA(p->vel, dist, diff, p->vel);
							//dist = freeparticles[j]->scale * 4.0f * frametime / dist;
							//VectorMA(p->vel, dist, freeparticles[j]->vel, p->vel);
						}
					}
				}
			}
		}
	}
}

void R_DrawParticles (void)
{
	particle_t		*p;
	int				i, dynamiclight, staticlight, r, g, b, texnum;
	float			minparticledist;
	vec3_t			uprightangles, up2, right2, v, right, up;
	mleaf_t			*leaf;

	// LordHavoc: early out condition
	if ((!numparticles) || (!r_drawparticles.value))
		return;

	staticlight = dynamiclight = r_particles_lighting.value;
	if (!r_dynamic.value)
		dynamiclight = 0;
	c_particles += numparticles;

	uprightangles[0] = 0;
	uprightangles[1] = r_refdef.viewangles[1];
	uprightangles[2] = 0;
	AngleVectors (uprightangles, NULL, right2, up2);

	minparticledist = DotProduct(r_origin, vpn) + 16.0f;

	texnum = R_GetTexture(particlefonttexture);
	for (i = 0, p = particles;i < numparticles;i++, p++)
	{
		if (p->tex == NULL || p->alpha < 1 || p->scale < 0.1f)
			continue;

		// LordHavoc: only render if not too close
		if (DotProduct(p->org, vpn) < minparticledist)
			continue;

		// LordHavoc: check if it's in a visible leaf
		leaf = Mod_PointInLeaf(p->org, cl.worldmodel);
		if (leaf->visframe != r_framecount)
			continue;

		r = p->color[0];
		g = p->color[1];
		b = p->color[2];
		if (staticlight && (p->dynlight || staticlight >= 2)) // LordHavoc: only light blood and smoke
		{
			R_CompleteLightPoint(v, p->org, dynamiclight, leaf);
#if SLOWMATH
			r = (r * (int) v[0]) >> 7;
			g = (g * (int) v[1]) >> 7;
			b = (b * (int) v[2]) >> 7;
#else
			v[0] += 8388608.0f;
			v[1] += 8388608.0f;
			v[2] += 8388608.0f;
			r = (r * (*((long *) &v[0]) & 0x7FFFFF)) >> 7;
			g = (g * (*((long *) &v[1]) & 0x7FFFFF)) >> 7;
			b = (b * (*((long *) &v[2]) & 0x7FFFFF)) >> 7;
#endif
		}
		if (p->type == pt_raindropsplash)
		{
			// treat as double-sided
			if (DotProduct(p->vel2, r_origin) > DotProduct(p->vel2, p->org))
			{
				VectorNegate(p->vel2, v);
				VectorVectors(v, right, up);
			}
			else
				VectorVectors(p->vel2, right, up);
			transpolyparticle(p->org, right, up, p->scale, texnum, p->rendermode, r, g, b, p->alpha, p->tex->s1, p->tex->t1, p->tex->s2, p->tex->t2);
		}
		else if (p->tex == &rainparticletexture) // rain streak
			transpolyparticle(p->org, right2, up2, p->scale, texnum, p->rendermode, r, g, b, p->alpha, p->tex->s1, p->tex->t1, p->tex->s2, p->tex->t2);
		else
			transpolyparticle(p->org, vright, vup, p->scale, texnum, p->rendermode, r, g, b, p->alpha, p->tex->s1, p->tex->t1, p->tex->s2, p->tex->t2);
	}
}
