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
#include "r_shadow.h"

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

#define PARTICLEEFFECT_UNDERWATER 1
#define PARTICLEEFFECT_NOTUNDERWATER 2

typedef struct particleeffectinfo_s
{
	int effectnameindex; // which effect this belongs to
	// PARTICLEEFFECT_* bits
	int flags;
	// blood effects may spawn very few particles, so proper fraction-overflow
	// handling is very important, this variable keeps track of the fraction
	double particleaccumulator;
	// the math is: countabsolute + requestedcount * countmultiplier * quality
	// absolute number of particles to spawn, often used for decals
	// (unaffected by quality and requestedcount)
	float countabsolute;
	// multiplier for the number of particles CL_ParticleEffect was told to
	// spawn, most effects do not really have a count and hence use 1, so
	// this is often the actual count to spawn, not merely a multiplier
	float countmultiplier;
	// if > 0 this causes the particle to spawn in an evenly spaced line from
	// originmins to originmaxs (causing them to describe a trail, not a box)
	float trailspacing;
	// type of particle to spawn (defines some aspects of behavior)
	ptype_t particletype;
	// range of colors to choose from in hex RRGGBB (like HTML color tags),
	// randomly interpolated at spawn
	unsigned int color[2];
	// a random texture is chosen in this range (note the second value is one
	// past the last choosable, so for example 8,16 chooses any from 8 up and
	// including 15)
	// if start and end of the range are the same, no randomization is done
	int tex[2];
	// range of size values randomly chosen when spawning, plus size increase over time
	float size[3];
	// range of alpha values randomly chosen when spawning, plus alpha fade
	float alpha[3];
	// how long the particle should live (note it is also removed if alpha drops to 0)
	float time[2];
	// how much gravity affects this particle (negative makes it fly up!)
	float gravity;
	// how much bounce the particle has when it hits a surface
	// if negative the particle is removed on impact
	float bounce;
	// if in air this friction is applied
	// if negative the particle accelerates
	float airfriction;
	// if in liquid (water/slime/lava) this friction is applied
	// if negative the particle accelerates
	float liquidfriction;
	// these offsets are added to the values given to particleeffect(), and
	// then an ellipsoid-shaped jitter is added as defined by these
	// (they are the 3 radii)
	float originoffset[3];
	float velocityoffset[3];
	float originjitter[3];
	float velocityjitter[3];
	float velocitymultiplier;
	// an effect can also spawn a dlight
	float lightradiusstart;
	float lightradiusfade;
	float lighttime;
	float lightcolor[3];
	qboolean lightshadow;
	int lightcubemapnum;
}
particleeffectinfo_t;

#define MAX_PARTICLEEFFECTNAME 256
char particleeffectname[MAX_PARTICLEEFFECTNAME][64];

#define MAX_PARTICLEEFFECTINFO 4096

particleeffectinfo_t particleeffectinfo[MAX_PARTICLEEFFECTINFO];

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
static const int tex_rainsplash = 32;
static const int tex_particle = 63;
static const int tex_bubble = 62;
static const int tex_raindrop = 61;
static const int tex_beam = 60;

cvar_t cl_particles = {CVAR_SAVE, "cl_particles", "1", "enables particle effects"};
cvar_t cl_particles_quality = {CVAR_SAVE, "cl_particles_quality", "1", "multiplies number of particles"};
cvar_t cl_particles_alpha = {CVAR_SAVE, "cl_particles_alpha", "1", "multiplies opacity of particles"};
cvar_t cl_particles_size = {CVAR_SAVE, "cl_particles_size", "1", "multiplies particle size"};
cvar_t cl_particles_quake = {CVAR_SAVE, "cl_particles_quake", "0", "makes particle effects look mostly like the ones in Quake"};
cvar_t cl_particles_blood = {CVAR_SAVE, "cl_particles_blood", "1", "enables blood effects"};
cvar_t cl_particles_blood_alpha = {CVAR_SAVE, "cl_particles_blood_alpha", "0.5", "opacity of blood"};
cvar_t cl_particles_blood_bloodhack = {CVAR_SAVE, "cl_particles_blood_bloodhack", "1", "make certain quake particle() calls create blood effects instead"};
cvar_t cl_particles_bulletimpacts = {CVAR_SAVE, "cl_particles_bulletimpacts", "1", "enables bulletimpact effects"};
cvar_t cl_particles_explosions_smoke = {CVAR_SAVE, "cl_particles_explosions_smokes", "0", "enables smoke from explosions"};
cvar_t cl_particles_explosions_sparks = {CVAR_SAVE, "cl_particles_explosions_sparks", "1", "enables sparks from explosions"};
cvar_t cl_particles_explosions_shell = {CVAR_SAVE, "cl_particles_explosions_shell", "0", "enables polygonal shell from explosions"};
cvar_t cl_particles_rain = {CVAR_SAVE, "cl_particles_rain", "1", "enables rain effects"};
cvar_t cl_particles_snow = {CVAR_SAVE, "cl_particles_snow", "1", "enables snow effects"};
cvar_t cl_particles_smoke = {CVAR_SAVE, "cl_particles_smoke", "1", "enables smoke (used by multiple effects)"};
cvar_t cl_particles_smoke_alpha = {CVAR_SAVE, "cl_particles_smoke_alpha", "0.5", "smoke brightness"};
cvar_t cl_particles_smoke_alphafade = {CVAR_SAVE, "cl_particles_smoke_alphafade", "0.55", "brightness fade per second"};
cvar_t cl_particles_sparks = {CVAR_SAVE, "cl_particles_sparks", "1", "enables sparks (used by multiple effects)"};
cvar_t cl_particles_bubbles = {CVAR_SAVE, "cl_particles_bubbles", "1", "enables bubbles (used by multiple effects)"};
cvar_t cl_decals = {CVAR_SAVE, "cl_decals", "0", "enables decals (bullet holes, blood, etc)"};
cvar_t cl_decals_time = {CVAR_SAVE, "cl_decals_time", "0", "how long before decals start to fade away"};
cvar_t cl_decals_fadetime = {CVAR_SAVE, "cl_decals_fadetime", "20", "how long decals take to fade away"};


void CL_Particles_ParseEffectInfo(const char *textstart, const char *textend)
{
	int arrayindex;
	int argc;
	int effectinfoindex;
	int linenumber;
	particleeffectinfo_t *info = NULL;
	const char *text = textstart;
	char argv[16][1024];
	effectinfoindex = -1;
	for (linenumber = 1;;linenumber++)
	{
		argc = 0;
		for (arrayindex = 0;arrayindex < 16;arrayindex++)
			argv[arrayindex][0] = 0;
		for (;;)
		{
			if (!COM_ParseToken_Simple(&text, true, false))
				return;
			if (!strcmp(com_token, "\n"))
				break;
			if (argc < 16)
			{
				strlcpy(argv[argc], com_token, sizeof(argv[argc]));
				argc++;
			}
		}
		if (argc < 1)
			continue;
#define checkparms(n) if (argc != (n)) {Con_Printf("effectinfo.txt:%i: error while parsing: %s given %i parameters, should be %i parameters\n", linenumber, argv[0], argc, (n));break;}
#define readints(array, n) checkparms(n+1);for (arrayindex = 0;arrayindex < argc - 1;arrayindex++) array[arrayindex] = strtol(argv[1+arrayindex], NULL, 0)
#define readfloats(array, n) checkparms(n+1);for (arrayindex = 0;arrayindex < argc - 1;arrayindex++) array[arrayindex] = atof(argv[1+arrayindex])
#define readint(var) checkparms(2);var = strtol(argv[1], NULL, 0)
#define readfloat(var) checkparms(2);var = atof(argv[1])
		if (!strcmp(argv[0], "effect"))
		{
			int effectnameindex;
			checkparms(2);
			effectinfoindex++;
			if (effectinfoindex >= MAX_PARTICLEEFFECTINFO)
			{
				Con_Printf("effectinfo.txt:%i: too many effects!\n", linenumber);
				break;
			}
			for (effectnameindex = 1;effectnameindex < MAX_PARTICLEEFFECTNAME;effectnameindex++)
			{
				if (particleeffectname[effectnameindex][0])
				{
					if (!strcmp(particleeffectname[effectnameindex], argv[1]))
						break;
				}
				else
				{
					strlcpy(particleeffectname[effectnameindex], argv[1], sizeof(particleeffectname[effectnameindex]));
					break;
				}
			}
			// if we run out of names, abort
			if (effectnameindex == MAX_PARTICLEEFFECTNAME)
			{
				Con_Printf("effectinfo.txt:%i: too many effects!\n", linenumber);
				break;
			}
			info = particleeffectinfo + effectinfoindex;
			info->effectnameindex = effectnameindex;
			info->particletype = pt_alphastatic;
			info->tex[0] = tex_particle;
			info->tex[1] = tex_particle;
			info->color[0] = 0xFFFFFF;
			info->color[1] = 0xFFFFFF;
			info->size[0] = 1;
			info->size[1] = 1;
			info->alpha[0] = 0;
			info->alpha[1] = 256;
			info->alpha[2] = 256;
			info->time[0] = 9999;
			info->time[1] = 9999;
			VectorSet(info->lightcolor, 1, 1, 1);
			info->lightshadow = true;
			info->lighttime = 9999;
		}
		else if (info == NULL)
		{
			Con_Printf("effectinfo.txt:%i: command %s encountered before effect\n", linenumber, argv[0]);
			break;
		}
		else if (!strcmp(argv[0], "countabsolute")) {readfloat(info->countabsolute);}
		else if (!strcmp(argv[0], "count")) {readfloat(info->countmultiplier);}
		else if (!strcmp(argv[0], "type"))
		{
			checkparms(2);
			if (!strcmp(argv[1], "alphastatic")) info->particletype = pt_alphastatic;
			else if (!strcmp(argv[1], "static")) info->particletype = pt_static;
			else if (!strcmp(argv[1], "spark")) info->particletype = pt_spark;
			else if (!strcmp(argv[1], "beam")) info->particletype = pt_beam;
			else if (!strcmp(argv[1], "rain")) info->particletype = pt_rain;
			else if (!strcmp(argv[1], "raindecal")) info->particletype = pt_raindecal;
			else if (!strcmp(argv[1], "snow")) info->particletype = pt_snow;
			else if (!strcmp(argv[1], "bubble")) info->particletype = pt_bubble;
			else if (!strcmp(argv[1], "blood")) info->particletype = pt_blood;
			else if (!strcmp(argv[1], "smoke")) info->particletype = pt_smoke;
			else if (!strcmp(argv[1], "decal")) info->particletype = pt_decal;
			else if (!strcmp(argv[1], "entityparticle")) info->particletype = pt_entityparticle;
			else Con_Printf("effectinfo.txt:%i: unrecognized particle type %s\n", linenumber, argv[1]);
		}
		else if (!strcmp(argv[0], "color")) {readints(info->color, 2);}
		else if (!strcmp(argv[0], "tex")) {readints(info->tex, 2);}
		else if (!strcmp(argv[0], "size")) {readfloats(info->size, 2);}
		else if (!strcmp(argv[0], "sizeincrease")) {readfloat(info->size[2]);}
		else if (!strcmp(argv[0], "alpha")) {readfloats(info->alpha, 3);}
		else if (!strcmp(argv[0], "time")) {readints(info->time, 2);}
		else if (!strcmp(argv[0], "gravity")) {readfloat(info->gravity);}
		else if (!strcmp(argv[0], "bounce")) {readfloat(info->bounce);}
		else if (!strcmp(argv[0], "airfriction")) {readfloat(info->airfriction);}
		else if (!strcmp(argv[0], "liquidfriction")) {readfloat(info->liquidfriction);}
		else if (!strcmp(argv[0], "originoffset")) {readfloats(info->originoffset, 3);}
		else if (!strcmp(argv[0], "velocityoffset")) {readfloats(info->velocityoffset, 3);}
		else if (!strcmp(argv[0], "originjitter")) {readfloats(info->originjitter, 3);}
		else if (!strcmp(argv[0], "velocityjitter")) {readfloats(info->velocityjitter, 3);}
		else if (!strcmp(argv[0], "velocitymultiplier")) {readfloat(info->velocitymultiplier);}
		else if (!strcmp(argv[0], "lightradius")) {readfloat(info->lightradiusstart);}
		else if (!strcmp(argv[0], "lightradiusfade")) {readfloat(info->lightradiusfade);}
		else if (!strcmp(argv[0], "lighttime")) {readfloat(info->lighttime);}
		else if (!strcmp(argv[0], "lightcolor")) {readfloats(info->lightcolor, 3);}
		else if (!strcmp(argv[0], "lightshadow")) {readint(info->lightshadow);}
		else if (!strcmp(argv[0], "lightcubemapnum")) {readint(info->lightcubemapnum);}
		else if (!strcmp(argv[0], "underwater")) {checkparms(1);info->flags |= PARTICLEEFFECT_UNDERWATER;}
		else if (!strcmp(argv[0], "notunderwater")) {checkparms(1);info->flags |= PARTICLEEFFECT_NOTUNDERWATER;}
		else if (!strcmp(argv[0], "trailspacing")) {readfloat(info->trailspacing);if (info->trailspacing > 0) info->countmultiplier = 1.0f / info->trailspacing;}
		else
			Con_Printf("effectinfo.txt:%i: skipping unknown command %s\n", linenumber, argv[0]);
#undef checkparms
#undef readints
#undef readfloats
#undef readint
#undef readfloat
	}
}

int CL_ParticleEffectIndexForName(const char *name)
{
	int i;
	for (i = 1;i < MAX_PARTICLEEFFECTNAME && particleeffectname[i][0];i++)
		if (!strcmp(particleeffectname[i], name))
			return i;
	return 0;
}

const char *CL_ParticleEffectNameForIndex(int i)
{
	if (i < 1 || i >= MAX_PARTICLEEFFECTNAME)
		return NULL;
	return particleeffectname[i];
}

// MUST match effectnameindex_t in client.h
static const char *standardeffectnames[EFFECT_TOTAL] =
{
	"",
	"TE_GUNSHOT",
	"TE_GUNSHOTQUAD",
	"TE_SPIKE",
	"TE_SPIKEQUAD",
	"TE_SUPERSPIKE",
	"TE_SUPERSPIKEQUAD",
	"TE_WIZSPIKE",
	"TE_KNIGHTSPIKE",
	"TE_EXPLOSION",
	"TE_EXPLOSIONQUAD",
	"TE_TAREXPLOSION",
	"TE_TELEPORT",
	"TE_LAVASPLASH",
	"TE_SMALLFLASH",
	"TE_FLAMEJET",
	"EF_FLAME",
	"TE_BLOOD",
	"TE_SPARK",
	"TE_PLASMABURN",
	"TE_TEI_G3",
	"TE_TEI_SMOKE",
	"TE_TEI_BIGEXPLOSION",
	"TE_TEI_PLASMAHIT",
	"EF_STARDUST",
	"TR_ROCKET",
	"TR_GRENADE",
	"TR_BLOOD",
	"TR_WIZSPIKE",
	"TR_SLIGHTBLOOD",
	"TR_KNIGHTSPIKE",
	"TR_VORESPIKE",
	"TR_NEHAHRASMOKE",
	"TR_NEXUIZPLASMA",
	"TR_GLOWTRAIL",
	"SVC_PARTICLE"
};

void CL_Particles_LoadEffectInfo(void)
{
	int i;
	unsigned char *filedata;
	fs_offset_t filesize;
	memset(particleeffectinfo, 0, sizeof(particleeffectinfo));
	memset(particleeffectname, 0, sizeof(particleeffectname));
	for (i = 0;i < EFFECT_TOTAL;i++)
		strlcpy(particleeffectname[i], standardeffectnames[i], sizeof(particleeffectname[i]));
	filedata = FS_LoadFile("effectinfo.txt", tempmempool, true, &filesize);
	if (filedata)
	{
		CL_Particles_ParseEffectInfo((const char *)filedata, (const char *)filedata + filesize);
		Mem_Free(filedata);
	}
};

/*
===============
CL_InitParticles
===============
*/
void CL_ReadPointFile_f (void);
void CL_Particles_Init (void)
{
	Cmd_AddCommand ("pointfile", CL_ReadPointFile_f, "display point file produced by qbsp when a leak was detected in the map (a line leading through the leak hole, to an entity inside the level)");
	Cmd_AddCommand ("cl_particles_reloadeffects", CL_Particles_LoadEffectInfo, "reloads effectinfo.txt");

	Cvar_RegisterVariable (&cl_particles);
	Cvar_RegisterVariable (&cl_particles_quality);
	Cvar_RegisterVariable (&cl_particles_alpha);
	Cvar_RegisterVariable (&cl_particles_size);
	Cvar_RegisterVariable (&cl_particles_quake);
	Cvar_RegisterVariable (&cl_particles_blood);
	Cvar_RegisterVariable (&cl_particles_blood_alpha);
	Cvar_RegisterVariable (&cl_particles_blood_bloodhack);
	Cvar_RegisterVariable (&cl_particles_explosions_smoke);
	Cvar_RegisterVariable (&cl_particles_explosions_sparks);
	Cvar_RegisterVariable (&cl_particles_explosions_shell);
	Cvar_RegisterVariable (&cl_particles_bulletimpacts);
	Cvar_RegisterVariable (&cl_particles_rain);
	Cvar_RegisterVariable (&cl_particles_snow);
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
static particle_t *particle(particletype_t *ptype, int pcolor1, int pcolor2, int ptex, float psize, float psizeincrease, float palpha, float palphafade, float pgravity, float pbounce, float px, float py, float pz, float pvx, float pvy, float pvz, float pairfriction, float pliquidfriction, float originjitter, float velocityjitter)
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
	part->sizeincrease = psizeincrease;
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
	part->airfriction = pairfriction;
	part->liquidfriction = pliquidfriction;
	part->die = cl.time + part->alpha / (part->alphafade ? part->alphafade : 1);
	part->delayedcollisions = 0;
	// if it is rain or snow, trace ahead and shut off collisions until an actual collision event needs to occur to improve performance
	if (part->type == particletype + pt_rain)
	{
		int i;
		particle_t *part2;
		float lifetime = part->die - cl.time;
		vec3_t endvec;
		trace_t trace;
		// turn raindrop into simple spark and create delayedspawn splash effect
		part->type = particletype + pt_spark;
		part->bounce = 0;
		VectorMA(part->org, lifetime, part->vel, endvec);
		trace = CL_Move(part->org, vec3_origin, vec3_origin, endvec, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | ((part->type == particletype + pt_rain || part->type == particletype + pt_snow) ? SUPERCONTENTS_LIQUIDSMASK : 0), true, false, NULL, false);
		part->die = cl.time + lifetime * trace.fraction;
		part2 = particle(particletype + pt_raindecal, pcolor1, pcolor2, tex_rainsplash, part->size, part->size * 20, part->alpha, part->alpha / 0.4, 0, 0, trace.endpos[0] + trace.plane.normal[0], trace.endpos[1] + trace.plane.normal[1], trace.endpos[2] + trace.plane.normal[2], trace.plane.normal[0], trace.plane.normal[1], trace.plane.normal[2], 0, 0, 0, 0);
		if (part2)
		{
			part2->delayedspawn = part->die;
			part2->die += part->die - cl.time;
			for (i = rand() & 7;i < 10;i++)
			{
				part2 = particle(particletype + pt_spark, pcolor1, pcolor2, tex_particle, 0.25f, 0, part->alpha * 2, part->alpha * 4, 1, 0, trace.endpos[0] + trace.plane.normal[0], trace.endpos[1] + trace.plane.normal[1], trace.endpos[2] + trace.plane.normal[2], trace.plane.normal[0] * 16, trace.plane.normal[1] * 16, trace.plane.normal[2] * 16 + cl.movevars_gravity * 0.04, 0, 0, 0, 32);
				if (part2)
				{
					part2->delayedspawn = part->die;
					part2->die += part->die - cl.time;
				}
			}
		}
	}
	else if (part->bounce != 0 && part->gravity == 0)
	{
		float lifetime = part->alpha / (part->alphafade ? part->alphafade : 1);
		vec3_t endvec;
		trace_t trace;
		VectorMA(part->org, lifetime, part->vel, endvec);
		trace = CL_Move(part->org, vec3_origin, vec3_origin, endvec, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | ((part->type == particletype + pt_rain || part->type == particletype + pt_snow) ? SUPERCONTENTS_LIQUIDSMASK : 0), true, false, NULL, false);
		part->delayedcollisions = cl.time + lifetime * trace.fraction;
	}
	return part;
}

void CL_SpawnDecalParticleForSurface(int hitent, const vec3_t org, const vec3_t normal, int color1, int color2, int texnum, float size, float alpha)
{
	particle_t *p;
	if (!cl_decals.integer)
		return;
	p = particle(particletype + pt_decal, color1, color2, texnum, size, 0, alpha, 0, 0, 0, org[0] + normal[0], org[1] + normal[1], org[2] + normal[2], normal[0], normal[1], normal[2], 0, 0, 0, 0);
	if (p)
	{
		p->time2 = cl.time;
		p->owner = hitent;
		p->ownermodel = cl.entities[p->owner].render.model;
		VectorAdd(org, normal, p->org);
		VectorCopy(normal, p->vel);
		// these relative things are only used to regenerate p->org and p->vel if p->owner is not world (0)
		Matrix4x4_Transform(&cl.entities[p->owner].render.inversematrix, org, p->relativeorigin);
		Matrix4x4_Transform3x3(&cl.entities[p->owner].render.inversematrix, normal, p->relativedirection);
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
		trace = CL_Move(org, vec3_origin, vec3_origin, org2, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY, true, false, &hitent, false);
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

static void CL_Sparks(const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, float sparkcount);
static void CL_Smoke(const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, float smokecount);
void CL_ParticleEffect_Fallback(int effectnameindex, float count, const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, entity_t *ent, int palettecolor, qboolean spawndlight, qboolean spawnparticles)
{
	vec3_t center;
	matrix4x4_t tempmatrix;
	VectorLerp(originmins, 0.5, originmaxs, center);
	Matrix4x4_CreateTranslate(&tempmatrix, center[0], center[1], center[2]);
	if (effectnameindex == EFFECT_SVC_PARTICLE)
	{
		if (cl_particles.integer)
		{
			// bloodhack checks if this effect's color matches regular or lightning blood and if so spawns a blood effect instead
			if (count == 1024)
				CL_ParticleExplosion(center);
			else if (cl_particles_blood_bloodhack.integer && !cl_particles_quake.integer && (palettecolor == 73 || palettecolor == 225))
				CL_ParticleEffect(EFFECT_TE_BLOOD, count / 6.0f, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			else
			{
				count *= cl_particles_quality.value;
				for (;count > 0;count--)
				{
					int k = particlepalette[palettecolor + (rand()&7)];
					if (cl_particles_quake.integer)
						particle(particletype + pt_alphastatic, k, k, tex_particle, 1.5, 0, lhrandom(51, 255), 512, 0.05, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 8, 0);
					else if (gamemode == GAME_GOODVSBAD2)
						particle(particletype + pt_alphastatic, k, k, tex_particle, 5, 0, 255, 300, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 8, 10);
					else
						particle(particletype + pt_alphastatic, k, k, tex_particle, 1.5, 0, 255, 512, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 8, 15);
				}
			}
		}
	}
	else if (effectnameindex == EFFECT_TE_WIZSPIKE)
		CL_ParticleEffect(EFFECT_SVC_PARTICLE, 30*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 20);
	else if (effectnameindex == EFFECT_TE_KNIGHTSPIKE)
		CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 226);
	else if (effectnameindex == EFFECT_TE_SPIKE)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
			{
				if (cl_particles_smoke.integer)
					CL_ParticleEffect(EFFECT_SVC_PARTICLE, 10*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			}
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 4*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 15*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
	}
	else if (effectnameindex == EFFECT_TE_SPIKEQUAD)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
			{
				if (cl_particles_smoke.integer)
					CL_ParticleEffect(EFFECT_SVC_PARTICLE, 10*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			}
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 4*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 15*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
		CL_AllocLightFlash(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_SUPERSPIKE)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
			{
				if (cl_particles_smoke.integer)
					CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			}
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 8*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 30*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
	}
	else if (effectnameindex == EFFECT_TE_SUPERSPIKEQUAD)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
			{
				if (cl_particles_smoke.integer)
					CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			}
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 8*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 30*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
		CL_AllocLightFlash(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_BLOOD)
	{
		if (!cl_particles_blood.integer)
			return;
		if (cl_particles_quake.integer)
			CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 73);
		else
		{
			static double bloodaccumulator = 0;
			bloodaccumulator += count * 0.333 * cl_particles_quality.value;
			for (;bloodaccumulator > 0;bloodaccumulator--)
				particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, 0, cl_particles_blood_alpha.value * 768, cl_particles_blood_alpha.value * 384, 0, -1, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 1, 4, 0, 64);
		}
	}
	else if (effectnameindex == EFFECT_TE_SPARK)
		CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, count);
	else if (effectnameindex == EFFECT_TE_PLASMABURN)
	{
		// plasma scorch mark
		if (cl_stainmaps.integer) R_Stain(center, 48, 96, 96, 96, 32, 128, 128, 128, 32);
		CL_SpawnDecalParticleForPoint(center, 6, 6, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
		CL_AllocLightFlash(NULL, &tempmatrix, 200, 1, 1, 1, 1000, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_GUNSHOT)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
				CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 4*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 20*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
	}
	else if (effectnameindex == EFFECT_TE_GUNSHOTQUAD)
	{
		if (cl_particles_bulletimpacts.integer)
		{
			if (cl_particles_quake.integer)
				CL_ParticleEffect(EFFECT_SVC_PARTICLE, 20*count, originmins, originmaxs, velocitymins, velocitymaxs, NULL, 0);
			else
			{
				CL_Smoke(originmins, originmaxs, velocitymins, velocitymaxs, 4*count);
				CL_Sparks(originmins, originmaxs, velocitymins, velocitymaxs, 20*count);
			}
		}
		// bullet hole
		if (cl_stainmaps.integer) R_Stain(center, 32, 96, 96, 96, 24, 128, 128, 128, 24);
		CL_SpawnDecalParticleForPoint(center, 6, 3, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
		CL_AllocLightFlash(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_EXPLOSION)
	{
		CL_ParticleExplosion(center);
		CL_AllocLightFlash(NULL, &tempmatrix, 350, 4.0f, 2.0f, 0.50f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_EXPLOSIONQUAD)
	{
		CL_ParticleExplosion(center);
		CL_AllocLightFlash(NULL, &tempmatrix, 350, 2.5f, 2.0f, 4.0f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_TAREXPLOSION)
	{
		if (cl_particles_quake.integer)
		{
			int i;
			for (i = 0;i < 1024 * cl_particles_quality.value;i++)
			{
				if (i & 1)
					particle(particletype + pt_static, particlepalette[66], particlepalette[71], tex_particle, 1.5f, 0, lhrandom(182, 255), 182, 0, 0, center[0], center[1], center[2], 0, 0, 0, -4, -4, 16, 256);
				else
					particle(particletype + pt_static, particlepalette[150], particlepalette[155], tex_particle, 1.5f, 0, lhrandom(182, 255), 182, 0, 0, center[0], center[1], center[2], 0, 0, lhrandom(-256, 256), 0, 0, 16, 0);
			}
		}
		else
			CL_ParticleExplosion(center);
		CL_AllocLightFlash(NULL, &tempmatrix, 600, 1.6f, 0.8f, 2.0f, 1200, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_SMALLFLASH)
		CL_AllocLightFlash(NULL, &tempmatrix, 200, 2, 2, 2, 1000, 0.2, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	else if (effectnameindex == EFFECT_TE_FLAMEJET)
	{
		count *= cl_particles_quality.value;
		while (count-- > 0)
			particle(particletype + pt_smoke, 0x6f0f00, 0xe3974f, tex_particle, 4, 0, lhrandom(64, 128), 384, -1, 1.1, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 1, 4, 0, 128);
	}
	else if (effectnameindex == EFFECT_TE_LAVASPLASH)
	{
		float i, j, inc, vel;
		vec3_t dir, org;

		inc = 8 / cl_particles_quality.value;
		for (i = -128;i < 128;i += inc)
		{
			for (j = -128;j < 128;j += inc)
			{
				dir[0] = j + lhrandom(0, inc);
				dir[1] = i + lhrandom(0, inc);
				dir[2] = 256;
				org[0] = center[0] + dir[0];
				org[1] = center[1] + dir[1];
				org[2] = center[2] + lhrandom(0, 64);
				vel = lhrandom(50, 120) / VectorLength(dir); // normalize and scale
				particle(particletype + pt_alphastatic, particlepalette[224], particlepalette[231], tex_particle, 1.5f, 0, inc * lhrandom(24, 32), inc * 12, 0.05, 0, org[0], org[1], org[2], dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0, 0);
			}
		}
	}
	else if (effectnameindex == EFFECT_TE_TELEPORT)
	{
		float i, j, k, inc, vel;
		vec3_t dir;

		inc = 8 / cl_particles_quality.value;
		for (i = -16;i < 16;i += inc)
		{
			for (j = -16;j < 16;j += inc)
			{
				for (k = -24;k < 32;k += inc)
				{
					VectorSet(dir, i*8, j*8, k*8);
					VectorNormalize(dir);
					vel = lhrandom(50, 113);
					particle(particletype + pt_alphastatic, particlepalette[7], particlepalette[14], tex_particle, 1.5f, 0, inc * lhrandom(37, 63), inc * 187, 0, 0, center[0] + i + lhrandom(0, inc), center[1] + j + lhrandom(0, inc), center[2] + k + lhrandom(0, inc), dir[0] * vel, dir[1] * vel, dir[2] * vel, 0, 0, 0, 0);
				}
			}
		}
		particle(particletype + pt_static, particlepalette[14], particlepalette[14], tex_particle, 30, 0, 256, 512, 0, 0, center[0], center[1], center[2], 0, 0, 0, 0, 0, 0, 0);
		CL_AllocLightFlash(NULL, &tempmatrix, 200, 2.0f, 2.0f, 2.0f, 400, 99.0f, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_TEI_G3)
		particle(particletype + pt_beam, 0xFFFFFF, 0xFFFFFF, tex_beam, 8, 0, 256, 256, 0, 0, originmins[0], originmins[1], originmins[2], originmaxs[0], originmaxs[1], originmaxs[2], 0, 0, 0, 0);
	else if (effectnameindex == EFFECT_TE_TEI_SMOKE)
	{
		if (cl_particles_smoke.integer)
		{
			count *= 0.25f * cl_particles_quality.value;
			while (count-- > 0)
				particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 0, 255, 512, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 1.5f, 6.0f);
		}
	}
	else if (effectnameindex == EFFECT_TE_TEI_BIGEXPLOSION)
	{
		CL_ParticleExplosion(center);
		CL_AllocLightFlash(NULL, &tempmatrix, 500, 2.5f, 2.0f, 1.0f, 500, 9999, 0, -1, true, 1, 0.25, 0.5, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_TE_TEI_PLASMAHIT)
	{
		float f;
		if (cl_stainmaps.integer)
			R_Stain(center, 40, 96, 96, 96, 40, 128, 128, 128, 40);
		CL_SpawnDecalParticleForPoint(center, 6, 8, 255, tex_bulletdecal[rand()&7], 0xFFFFFF, 0xFFFFFF);
		if (cl_particles_smoke.integer)
			for (f = 0;f < count;f += 4.0f / cl_particles_quality.value)
				particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 5, 0, 255, 512, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 20, 155);
		if (cl_particles_sparks.integer)
			for (f = 0;f < count;f += 1.0f / cl_particles_quality.value)
				particle(particletype + pt_spark, 0x2030FF, 0x80C0FF, tex_particle, 2.0f, 0, lhrandom(64, 255), 512, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 0, 465);
		CL_AllocLightFlash(NULL, &tempmatrix, 500, 0.6f, 1.2f, 2.0f, 2000, 9999, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_EF_FLAME)
	{
		count *= 300 * cl_particles_quality.value;
		while (count-- > 0)
			particle(particletype + pt_smoke, 0x6f0f00, 0xe3974f, tex_particle, 4, 0, lhrandom(64, 128), 384, -1, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 1, 4, 16, 128);
		CL_AllocLightFlash(NULL, &tempmatrix, 200, 2.0f, 1.5f, 0.5f, 0, 0, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (effectnameindex == EFFECT_EF_STARDUST)
	{
		count *= 200 * cl_particles_quality.value;
		while (count-- > 0)
			particle(particletype + pt_static, 0x903010, 0xFFD030, tex_particle, 4, 0, lhrandom(64, 128), 128, 1, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0.2, 0.8, 16, 128);
		CL_AllocLightFlash(NULL, &tempmatrix, 200, 1.0f, 0.7f, 0.3f, 0, 0, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	else if (!strncmp(particleeffectname[effectnameindex], "TR_", 3))
	{
		vec3_t dir, pos;
		float len, dec, qd;
		int smoke, blood, bubbles, r, color;

		if (spawndlight && r_refdef.numlights < MAX_DLIGHTS)
		{
			vec4_t light;
			Vector4Set(light, 0, 0, 0, 0);

			if (effectnameindex == EFFECT_TR_ROCKET)
				Vector4Set(light, 3.0f, 1.5f, 0.5f, 200);
			else if (effectnameindex == EFFECT_TR_VORESPIKE)
			{
				if (gamemode == GAME_PRYDON && !cl_particles_quake.integer)
					Vector4Set(light, 0.3f, 0.6f, 1.2f, 100);
				else
					Vector4Set(light, 1.2f, 0.5f, 1.0f, 200);
			}
			else if (effectnameindex == EFFECT_TR_NEXUIZPLASMA)
				Vector4Set(light, 0.75f, 1.5f, 3.0f, 200);

			if (light[3])
			{
				matrix4x4_t tempmatrix;
				Matrix4x4_CreateFromQuakeEntity(&tempmatrix, originmaxs[0], originmaxs[1], originmaxs[2], 0, 0, 0, light[3]);
				R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &tempmatrix, light, -1, NULL, true, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
			}
		}

		if (!spawnparticles)
			return;

		if (originmaxs[0] == originmins[0] && originmaxs[1] == originmins[1] && originmaxs[2] == originmins[2])
			return;

		VectorSubtract(originmaxs, originmins, dir);
		len = VectorNormalizeLength(dir);
		if (ent)
		{
			dec = -ent->persistent.trail_time;
			ent->persistent.trail_time += len;
			if (ent->persistent.trail_time < 0.01f)
				return;

			// if we skip out, leave it reset
			ent->persistent.trail_time = 0.0f;
		}
		else
			dec = 0;

		// advance into this frame to reach the first puff location
		VectorMA(originmins, dec, dir, pos);
		len -= dec;

		smoke = cl_particles.integer && cl_particles_smoke.integer;
		blood = cl_particles.integer && cl_particles_blood.integer;
		bubbles = cl_particles.integer && cl_particles_bubbles.integer && !cl_particles_quake.integer && (CL_PointSuperContents(pos) & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME));
		qd = 1.0f / cl_particles_quality.value;

		while (len >= 0)
		{
			dec = 3;
			if (blood)
			{
				if (effectnameindex == EFFECT_TR_BLOOD)
				{
					if (cl_particles_quake.integer)
					{
						color = particlepalette[67 + (rand()&3)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 128, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 3, 0);
					}
					else
					{
						dec = 16;
						particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, 0, qd * cl_particles_blood_alpha.value * 768.0f, qd * cl_particles_blood_alpha.value * 384.0f, 0, -1, pos[0], pos[1], pos[2], lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 1, 4, 0, 64);
					}
				}
				else if (effectnameindex == EFFECT_TR_SLIGHTBLOOD)
				{
					if (cl_particles_quake.integer)
					{
						dec = 6;
						color = particlepalette[67 + (rand()&3)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 128, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 3, 0);
					}
					else
					{
						dec = 32;
						particle(particletype + pt_blood, 0xFFFFFF, 0xFFFFFF, tex_bloodparticle[rand()&7], 8, 0, qd * cl_particles_blood_alpha.value * 768.0f, qd * cl_particles_blood_alpha.value * 384.0f, 0, -1, pos[0], pos[1], pos[2], lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 1, 4, 0, 64);
					}
				}
			}
			if (smoke)
			{
				if (effectnameindex == EFFECT_TR_ROCKET)
				{
					if (cl_particles_quake.integer)
					{
						r = rand()&3;
						color = particlepalette[ramp3[r]];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 42*(6-r), 306, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 3, 0);
					}
					else
					{
						particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, 0, cl_particles_smoke_alpha.value*62, cl_particles_smoke_alphafade.value*62, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
						particle(particletype + pt_static, 0x801010, 0xFFA020, tex_smoke[rand()&7], 3, 0, cl_particles_smoke_alpha.value*288, cl_particles_smoke_alphafade.value*1400, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 20);
					}
				}
				else if (effectnameindex == EFFECT_TR_GRENADE)
				{
					if (cl_particles_quake.integer)
					{
						r = 2 + (rand()%5);
						color = particlepalette[ramp3[r]];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 42*(6-r), 306, 0, -0.05, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 3, 0);
					}
					else
					{
						particle(particletype + pt_smoke, 0x303030, 0x606060, tex_smoke[rand()&7], 3, 0, cl_particles_smoke_alpha.value*50, cl_particles_smoke_alphafade.value*75, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
				}
				else if (effectnameindex == EFFECT_TR_WIZSPIKE)
				{
					if (cl_particles_quake.integer)
					{
						dec = 6;
						color = particlepalette[52 + (rand()&7)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*dir[1], 30*-dir[0], 0, 0, 0, 0, 0);
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*-dir[1], 30*dir[0], 0, 0, 0, 0, 0);
					}
					else if (gamemode == GAME_GOODVSBAD2)
					{
						dec = 6;
						particle(particletype + pt_static, 0x00002E, 0x000030, tex_particle, 6, 0, 128, 384, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
					else
					{
						color = particlepalette[20 + (rand()&7)];
						particle(particletype + pt_static, color, color, tex_particle, 2, 0, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
				}
				else if (effectnameindex == EFFECT_TR_KNIGHTSPIKE)
				{
					if (cl_particles_quake.integer)
					{
						dec = 6;
						color = particlepalette[230 + (rand()&7)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*dir[1], 30*-dir[0], 0, 0, 0, 0, 0);
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 512, 0, 0, pos[0], pos[1], pos[2], 30*-dir[1], 30*dir[0], 0, 0, 0, 0, 0);
					}
					else
					{
						color = particlepalette[226 + (rand()&7)];
						particle(particletype + pt_static, color, color, tex_particle, 2, 0, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
				}
				else if (effectnameindex == EFFECT_TR_VORESPIKE)
				{
					if (cl_particles_quake.integer)
					{
						color = particlepalette[152 + (rand()&3)];
						particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 255, 850, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 8, 0);
					}
					else if (gamemode == GAME_GOODVSBAD2)
					{
						dec = 6;
						particle(particletype + pt_alphastatic, particlepalette[0 + (rand()&255)], particlepalette[0 + (rand()&255)], tex_particle, 6, 0, 255, 384, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
					else if (gamemode == GAME_PRYDON)
					{
						dec = 6;
						particle(particletype + pt_static, 0x103040, 0x204050, tex_particle, 6, 0, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
					}
					else
						particle(particletype + pt_static, 0x502030, 0x502030, tex_particle, 3, 0, 64, 192, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
				}
				else if (effectnameindex == EFFECT_TR_NEHAHRASMOKE)
				{
					dec = 7;
					particle(particletype + pt_alphastatic, 0x303030, 0x606060, tex_smoke[rand()&7], 7, 0, 64, 320, 0, 0, pos[0], pos[1], pos[2], 0, 0, lhrandom(4, 12), 0, 0, 0, 4);
				}
				else if (effectnameindex == EFFECT_TR_NEXUIZPLASMA)
				{
					dec = 4;
					particle(particletype + pt_static, 0x283880, 0x283880, tex_particle, 4, 0, 255, 1024, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 16);
				}
				else if (effectnameindex == EFFECT_TR_GLOWTRAIL)
					particle(particletype + pt_alphastatic, particlepalette[palettecolor], particlepalette[palettecolor], tex_particle, 5, 0, 128, 320, 0, 0, pos[0], pos[1], pos[2], 0, 0, 0, 0, 0, 0, 0);
			}
			if (bubbles)
			{
				if (effectnameindex == EFFECT_TR_ROCKET)
					particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, 0, lhrandom(64, 255), 256, -0.25, 1.5, pos[0], pos[1], pos[2], 0, 0, 0, 0.0625, 0.25, 0, 16);
				else if (effectnameindex == EFFECT_TR_GRENADE)
					particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, 0, lhrandom(64, 255), 256, -0.25, 1.5, pos[0], pos[1], pos[2], 0, 0, 0, 0.0625, 0.25, 0, 16);
			}
			// advance to next time and position
			dec *= qd;
			len -= dec;
			VectorMA (pos, dec, dir, pos);
		}
		if (ent)
			ent->persistent.trail_time = len;
	}
	else if (developer.integer >= 1)
		Con_Printf("CL_ParticleEffect_Fallback: no fallback found for effect %s\n", particleeffectname[effectnameindex]);
}

// this is also called on point effects with spawndlight = true and
// spawnparticles = true
// it is called CL_ParticleTrail because most code does not want to supply
// these parameters, only trail handling does
void CL_ParticleTrail(int effectnameindex, float pcount, const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, entity_t *ent, int palettecolor, qboolean spawndlight, qboolean spawnparticles)
{
	vec3_t center;
	qboolean found = false;
	if (effectnameindex < 1 || effectnameindex >= MAX_PARTICLEEFFECTNAME)
		return; // invalid effect index
	if (!particleeffectname[effectnameindex][0])
		return; // no such effect
	VectorLerp(originmins, 0.5, originmaxs, center);
	if (!cl_particles_quake.integer && particleeffectinfo[0].effectnameindex)
	{
		int effectinfoindex;
		int supercontents;
		int tex;
		particleeffectinfo_t *info;
		vec3_t center;
		vec3_t centervelocity;
		vec3_t traildir;
		vec3_t trailpos;
		vec3_t rvec;
		vec_t traillen;
		vec_t trailstep;
		qboolean underwater;
		// note this runs multiple effects with the same name, each one spawns only one kind of particle, so some effects need more than one
		VectorLerp(originmins, 0.5, originmaxs, center);
		VectorLerp(velocitymins, 0.5, velocitymaxs, centervelocity);
		supercontents = CL_PointSuperContents(center);
		underwater = (supercontents & (SUPERCONTENTS_WATER | SUPERCONTENTS_SLIME)) != 0;
		VectorSubtract(originmaxs, originmins, traildir);
		traillen = VectorLength(traildir);
		VectorNormalize(traildir);
		for (effectinfoindex = 0, info = particleeffectinfo;effectinfoindex < MAX_PARTICLEEFFECTINFO && info->effectnameindex;effectinfoindex++, info++)
		{
			if (info->effectnameindex == effectnameindex)
			{
				found = true;
				if ((info->flags & PARTICLEEFFECT_UNDERWATER) && !underwater)
					continue;
				if ((info->flags & PARTICLEEFFECT_NOTUNDERWATER) && underwater)
					continue;

				// spawn a dlight if requested
				if (info->lightradiusstart > 0 && spawndlight)
				{
					matrix4x4_t tempmatrix;
					if (info->trailspacing > 0)
						Matrix4x4_CreateTranslate(&tempmatrix, originmaxs[0], originmaxs[1], originmaxs[2]);
					else
						Matrix4x4_CreateTranslate(&tempmatrix, center[0], center[1], center[2]);
					if (info->lighttime > 0 && info->lightradiusfade > 0)
					{
						// light flash (explosion, etc)
						// called when effect starts
						CL_AllocLightFlash(NULL, &tempmatrix, info->lightradiusstart, info->lightcolor[0], info->lightcolor[1], info->lightcolor[2], info->lightradiusfade, info->lighttime, info->lightcubemapnum, -1, info->lightshadow, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
					}
					else
					{
						// glowing entity
						// called by CL_LinkNetworkEntity
						Matrix4x4_Scale(&tempmatrix, info->lightradiusstart, 1);
						R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &tempmatrix, info->lightcolor, -1, info->lightcubemapnum > 0 ? va("cubemaps/%i", info->lightcubemapnum) : NULL, info->lightshadow, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
					}
				}

				if (!spawnparticles)
					continue;

				// spawn particles
				tex = info->tex[0];
				if (info->tex[1] > info->tex[0])
				{
					tex = (int)lhrandom(info->tex[0], info->tex[1]);
					tex = min(tex, info->tex[1] - 1);
				}
				if (info->particletype == pt_decal)
					CL_SpawnDecalParticleForPoint(center, info->originjitter[0], lhrandom(info->size[0], info->size[1]), lhrandom(info->alpha[0], info->alpha[1]), tex, info->color[0], info->color[1]);
				else if (info->particletype == pt_beam)
					particle(particletype + info->particletype, info->color[0], info->color[1], tex, lhrandom(info->size[0], info->size[1]), info->size[2], lhrandom(info->alpha[0], info->alpha[1]), info->alpha[2], 0, 0, originmins[0], originmins[1], originmins[2], originmaxs[0], originmaxs[1], originmaxs[2], 0, 0, 0, 0);
				else
				{
					if (!cl_particles.integer)
						continue;
					switch (info->particletype)
					{
					case pt_smoke: if (!cl_particles_smoke.integer) continue;break;
					case pt_spark: if (!cl_particles_sparks.integer) continue;break;
					case pt_bubble: if (!cl_particles_bubbles.integer) continue;break;
					case pt_blood: if (!cl_particles_blood.integer) continue;break;
					case pt_rain: if (!cl_particles_rain.integer) continue;break;
					case pt_snow: if (!cl_particles_snow.integer) continue;break;
					default: break;
					}
					VectorCopy(originmins, trailpos);
					if (info->trailspacing > 0)
					{
						info->particleaccumulator += traillen / info->trailspacing * cl_particles_quality.value;
						trailstep = info->trailspacing / cl_particles_quality.value;
					}
					else
					{
						info->particleaccumulator += info->countabsolute + pcount * info->countmultiplier * cl_particles_quality.value;
						trailstep = 0;
					}
					info->particleaccumulator = bound(0, info->particleaccumulator, 16384);
					for (;info->particleaccumulator >= 1;info->particleaccumulator--)
					{
						if (info->tex[1] > info->tex[0])
						{
							tex = (int)lhrandom(info->tex[0], info->tex[1]);
							tex = min(tex, info->tex[1] - 1);
						}
						if (!trailstep)
						{
							trailpos[0] = lhrandom(originmins[0], originmaxs[0]);
							trailpos[1] = lhrandom(originmins[1], originmaxs[1]);
							trailpos[2] = lhrandom(originmins[2], originmaxs[2]);
						}
						VectorRandom(rvec);
						particle(particletype + info->particletype, info->color[0], info->color[1], tex, lhrandom(info->size[0], info->size[1]), info->size[2], lhrandom(info->alpha[0], info->alpha[1]), info->alpha[2], info->gravity, info->bounce, trailpos[0] + info->originoffset[0] + info->originjitter[0] * rvec[0], trailpos[1] + info->originoffset[1] + info->originjitter[1] * rvec[1], trailpos[2] + info->originoffset[2] + info->originjitter[2] * rvec[2], lhrandom(velocitymins[0], velocitymaxs[0]) * info->velocitymultiplier + info->velocityoffset[0] + info->velocityjitter[0] * rvec[0], lhrandom(velocitymins[1], velocitymaxs[1]) * info->velocitymultiplier + info->velocityoffset[1] + info->velocityjitter[1] * rvec[1], lhrandom(velocitymins[2], velocitymaxs[2]) * info->velocitymultiplier + info->velocityoffset[2] + info->velocityjitter[2] * rvec[2], info->airfriction, info->liquidfriction, 0, 0);
						if (trailstep)
							VectorMA(trailpos, trailstep, traildir, trailpos);
					}
				}
			}
		}
	}
	if (!found)
		CL_ParticleEffect_Fallback(effectnameindex, pcount, originmins, originmaxs, velocitymins, velocitymaxs, ent, palettecolor, spawndlight, spawnparticles);
}

void CL_ParticleEffect(int effectnameindex, float pcount, const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, entity_t *ent, int palettecolor)
{
	CL_ParticleTrail(effectnameindex, pcount, originmins, originmaxs, velocitymins, velocitymaxs, ent, palettecolor, true, true);
}

/*
===============
CL_EntityParticles
===============
*/
void CL_EntityParticles (const entity_t *ent)
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
		particle(particletype + pt_entityparticle, particlepalette[0x6f], particlepalette[0x6f], tex_particle, 1, 0, 255, 0, 0, 0, v[0], v[1], v[2], 0, 0, 0, 0, 0, 0, 0);
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
			particle(particletype + pt_static, particlepalette[(-c)&15], particlepalette[(-c)&15], tex_particle, 2, 0, 255, 0, 0, 0, org[0], org[1], org[2], 0, 0, 0, 0, 0, 0, 0);
		}
	}
	Mem_Free(pointfile);
	VectorCopy(leakorg, org);
	Con_Printf("%i points read (%i particles spawned)\nLeak at %f %f %f\n", c, s, org[0], org[1], org[2]);

	particle(particletype + pt_beam, 0xFF0000, 0xFF0000, tex_beam, 64, 0, 255, 0, 0, 0, org[0] - 4096, org[1], org[2], org[0] + 4096, org[1], org[2], 0, 0, 0, 0);
	particle(particletype + pt_beam, 0x00FF00, 0x00FF00, tex_beam, 64, 0, 255, 0, 0, 0, org[0], org[1] - 4096, org[2], org[0], org[1] + 4096, org[2], 0, 0, 0, 0);
	particle(particletype + pt_beam, 0x0000FF, 0x0000FF, tex_beam, 64, 0, 255, 0, 0, 0, org[0], org[1], org[2] - 4096, org[0], org[1], org[2] + 4096, 0, 0, 0, 0);
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

	CL_ParticleEffect(EFFECT_SVC_PARTICLE, count, org, org, dir, dir, NULL, color);
}

/*
===============
CL_ParticleExplosion

===============
*/
void CL_ParticleExplosion (const vec3_t org)
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
				particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 32 * (8 - r), 318, 0, 0, org[0], org[1], org[2], 0, 0, 0, -4, -4, 16, 256);
			}
			else
			{
				color = particlepalette[ramp2[r]];
				particle(particletype + pt_alphastatic, color, color, tex_particle, 1.5f, 0, 32 * (8 - r), 478, 0, 0, org[0], org[1], org[2], 0, 0, 0, 1, 1, 16, 256);
			}
		}
	}
	else
	{
		i = CL_PointSuperContents(org);
		if (i & (SUPERCONTENTS_SLIME | SUPERCONTENTS_WATER))
		{
			if (cl_particles.integer && cl_particles_bubbles.integer)
				for (i = 0;i < 128 * cl_particles_quality.value;i++)
					particle(particletype + pt_bubble, 0x404040, 0x808080, tex_bubble, 2, 0, lhrandom(128, 255), 128, -0.125, 1.5, org[0], org[1], org[2], 0, 0, 0, 0.0625, 0.25, 16, 96);
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
						trace = CL_Move(org, vec3_origin, vec3_origin, v, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false);
						if (trace.fraction >= 0.1)
							break;
					}
					VectorSubtract(trace.endpos, org, v2);
					VectorScale(v2, 2.0f, v2);
					particle(particletype + pt_smoke, 0x202020, 0x404040, tex_smoke[rand()&7], 12, 0, 32, 64, 0, 0, org[0], org[1], org[2], v2[0], v2[1], v2[2], 0, 0, 0, 0);
				}
			}

			if (cl_particles.integer && cl_particles_sparks.integer && cl_particles_explosions_sparks.integer)
			{
				for (i = 0;i < 512 * cl_particles_quality.value;i++)
				{
					int k;
					vec3_t v, v2;
					for (k = 0;k < 16;k++)
					{
						VectorRandom(v2);
						VectorMA(org, 128, v2, v);
						trace = CL_Move(org, vec3_origin, vec3_origin, v, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID, true, false, NULL, false);
						if (trace.fraction >= 0.1)
							break;
					}
					VectorSubtract(trace.endpos, org, v2);
					VectorScale(v2, 2.0f, v2);
					particle(particletype + pt_spark, 0x903010, 0xFFD030, tex_particle, 1.0f, 0, lhrandom(0, 255), 512, 0, 0, org[0], org[1], org[2], v2[0], v2[1], v2[2], 0, 0, 0, 0);
				}
			}
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
void CL_ParticleExplosion2 (const vec3_t org, int colorStart, int colorLength)
{
	int i, k;
	if (!cl_particles.integer) return;

	for (i = 0;i < 512 * cl_particles_quality.value;i++)
	{
		k = particlepalette[colorStart + (i % colorLength)];
		if (cl_particles_quake.integer)
			particle(particletype + pt_static, k, k, tex_particle, 1, 0, 255, 850, 0, 0, org[0], org[1], org[2], 0, 0, 0, -4, -4, 8, 256);
		else
			particle(particletype + pt_static, k, k, tex_particle, lhrandom(0.5, 1.5), 0, 255, 512, 0, 0, org[0], org[1], org[2], 0, 0, 0, lhrandom(1.5, 3), lhrandom(1.5, 3), 8, 192);
	}
}

static void CL_Sparks(const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, float sparkcount)
{
	if (cl_particles_sparks.integer)
	{
		sparkcount *= cl_particles_quality.value;
		while(sparkcount-- > 0)
			particle(particletype + pt_spark, particlepalette[0x68], particlepalette[0x6f], tex_particle, 0.5f, 0, lhrandom(64, 255), 512, 1, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]) + cl.movevars_gravity * 0.1f, 0, 0, 0, 64);
	}
}

static void CL_Smoke(const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, float smokecount)
{
	if (cl_particles_smoke.integer)
	{
		smokecount *= cl_particles_quality.value;
		while(smokecount-- > 0)
			particle(particletype + pt_smoke, 0x101010, 0x101010, tex_smoke[rand()&7], 2, 2, 255, 256, 0, 0, lhrandom(originmins[0], originmaxs[0]), lhrandom(originmins[1], originmaxs[1]), lhrandom(originmins[2], originmaxs[2]), lhrandom(velocitymins[0], velocitymaxs[0]), lhrandom(velocitymins[1], velocitymaxs[1]), lhrandom(velocitymins[2], velocitymaxs[2]), 0, 0, 0, smokecount > 0 ? 16 : 0);
	}
}

void CL_ParticleCube (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, vec_t gravity, vec_t randomvel)
{
	int k;
	if (!cl_particles.integer) return;

	count = (int)(count * cl_particles_quality.value);
	while (count--)
	{
		k = particlepalette[colorbase + (rand()&3)];
		particle(particletype + pt_alphastatic, k, k, tex_particle, 2, 0, 255, 128, gravity, 0, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(mins[2], maxs[2]), dir[0], dir[1], dir[2], 0, 0, 0, randomvel);
	}
}

void CL_ParticleRain (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, int type)
{
	int k;
	float z, minz, maxz;
	particle_t *p;
	if (!cl_particles.integer) return;
	if (dir[2] < 0) // falling
		z = maxs[2];
	else // rising??
		z = mins[2];

	minz = z - fabs(dir[2]) * 0.1;
	maxz = z + fabs(dir[2]) * 0.1;
	minz = bound(mins[2], minz, maxs[2]);
	maxz = bound(mins[2], maxz, maxs[2]);

	count = (int)(count * cl_particles_quality.value);

	switch(type)
	{
	case 0:
		if (!cl_particles_rain.integer) break;
		count *= 4; // ick, this should be in the mod or maps?

		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			if (gamemode == GAME_GOODVSBAD2)
				particle(particletype + pt_rain, k, k, tex_particle, 20, 0, lhrandom(8, 16), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0, 0);
			else
				particle(particletype + pt_rain, k, k, tex_particle, 0.5, 0, lhrandom(8, 16), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0, 0);
		}
		break;
	case 1:
		if (!cl_particles_snow.integer) break;
		while(count--)
		{
			k = particlepalette[colorbase + (rand()&3)];
			if (gamemode == GAME_GOODVSBAD2)
				p = particle(particletype + pt_snow, k, k, tex_particle, 20, 0, lhrandom(64, 128), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0, 0);
			else
				p = particle(particletype + pt_snow, k, k, tex_particle, 1, 0, lhrandom(64, 128), 0, 0, -1, lhrandom(mins[0], maxs[0]), lhrandom(mins[1], maxs[1]), lhrandom(minz, maxz), dir[0], dir[1], dir[2], 0, 0, 0, 0);
			if (p)
				VectorCopy(p->vel, p->relativedirection);
		}
		break;
	default:
		Con_Printf ("CL_ParticleRain: unknown type %i (0 = rain, 1 = snow)\n", type);
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
	int i, maxparticle, j, a, content;
	float gravity, dvel, decalfade, frametime, f, dist, org[3], oldorg[3];
	particletype_t *decaltype, *bloodtype;
	int hitent;
	trace_t trace;

	// LordHavoc: early out condition
	if (!cl.num_particles)
	{
		cl.free_particle = 0;
		return;
	}

	frametime = bound(0, cl.time - cl.oldtime, 0.1);
	gravity = frametime * cl.movevars_gravity;
	dvel = 1+4*frametime;
	decalfade = frametime * 255 / cl_decals_fadetime.value;
	decaltype = particletype + pt_decal;
	bloodtype = particletype + pt_blood;

	maxparticle = -1;
	j = 0;
	for (i = 0, p = cl.particles;i < cl.num_particles;i++, p++)
	{
		if (!p->type)
		{
			if (cl.free_particle > i)
				cl.free_particle = i;
			continue;
		}
		maxparticle = i;

		// heavily optimized decal case
		if (p->type == decaltype)
		{
			// FIXME: this has fairly wacky handling of alpha
			if (cl.time > p->time2 + cl_decals_time.value)
			{
				p->alpha -= decalfade;
				if (p->alpha <= 0)
				{
					p->type = NULL;
					if (cl.free_particle > i)
						cl.free_particle = i;
					continue;
				}
			}
			if (p->owner)
			{
				if (cl.entities[p->owner].render.model == p->ownermodel)
				{
					Matrix4x4_Transform(&cl.entities[p->owner].render.matrix, p->relativeorigin, p->org);
					Matrix4x4_Transform3x3(&cl.entities[p->owner].render.matrix, p->relativedirection, p->vel);
				}
				else
				{
					p->type = NULL;
					if (cl.free_particle > i)
						cl.free_particle = i;
				}
			}
			continue;
		}

		if (p->delayedspawn)
		{
			if (p->delayedspawn > cl.time)
				continue;
			p->delayedspawn = 0;
		}

		content = 0;

		p->size += p->sizeincrease * frametime;
		p->alpha -= p->alphafade * frametime;

		if (p->alpha <= 0 || p->die <= cl.time)
		{
			p->type = NULL;
			if (cl.free_particle > i)
				cl.free_particle = i;
			continue;
		}

		if (p->type->orientation != PARTICLE_BEAM)
		{
			VectorCopy(p->org, oldorg);
			VectorMA(p->org, frametime, p->vel, p->org);
			VectorCopy(p->org, org);
			if (p->bounce && cl.time >= p->delayedcollisions)
			{
				trace = CL_Move(oldorg, vec3_origin, vec3_origin, p->org, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | (p->type == particletype + pt_rain ? SUPERCONTENTS_LIQUIDSMASK : 0), true, false, &hitent, false);
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
						p->texnum = tex_rainsplash;
						p->time2 = cl.time;
						p->alphafade = p->alpha / 0.4;
						p->bounce = 0;
						p->airfriction = 0;
						p->liquidfriction = 0;
						p->gravity = 0;
						p->size *= 1.0f;
						p->sizeincrease = p->size * 20;
						count = (int)lhrandom(1, 10);
						while(count--)
							particle(particletype + pt_spark, 0x000000, 0x707070, tex_particle, 0.25f, 0, lhrandom(64, 255), 512, 1, 0, p->org[0], p->org[1], p->org[2], p->vel[0]*16, p->vel[1]*16, cl.movevars_gravity * 0.04 + p->vel[2]*16, 0, 0, 0, 32);
					}
					else if (p->type == bloodtype)
					{
						// blood - splash on solid
						if (trace.hitq3surfaceflags & Q3SURFACEFLAG_NOMARKS)
						{
							p->type = NULL;
							continue;
						}
						if (cl_stainmaps.integer)
							R_Stain(p->org, 32, 32, 16, 16, (int)(p->alpha * p->size * (1.0f / 40.0f)), 192, 48, 48, (int)(p->alpha * p->size * (1.0f / 40.0f)));
						if (!cl_decals.integer)
						{
							p->type = NULL;
							continue;
						}
						// convert from a blood particle to a blood decal
						VectorCopy(trace.plane.normal, p->vel);
						VectorAdd(p->org, p->vel, p->org);

						p->type = particletype + pt_decal;
						p->texnum = tex_blooddecal[rand()&7];
						p->owner = hitent;
						p->ownermodel = cl.entities[hitent].render.model;
						// these relative things are only used to regenerate p->org and p->vel if p->owner is not world (0)
						Matrix4x4_Transform(&cl.entities[hitent].render.inversematrix, p->org, p->relativeorigin);
						Matrix4x4_Transform3x3(&cl.entities[hitent].render.inversematrix, p->vel, p->relativedirection);
						p->time2 = cl.time;
						p->alphafade = 0;
						p->bounce = 0;
						p->airfriction = 0;
						p->liquidfriction = 0;
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

			if (p->liquidfriction && CL_PointSuperContents(p->org) & SUPERCONTENTS_LIQUIDSMASK)
			{
				f = 1.0f - min(p->liquidfriction * frametime, 1);
				VectorScale(p->vel, f, p->vel);
			}
			else if (p->airfriction)
			{
				f = 1.0f - min(p->airfriction * frametime, 1);
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
			default:
				break;
			}
		}
	}
	cl.num_particles = maxparticle + 1;
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
				d[0] += (int)(f * (red   - d[0]));
				d[1] += (int)(f * (green - d[1]));
				d[2] += (int)(f * (blue  - d[2]));
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

//uncomment this to make engine save out particle font to a tga file when run
//#define DUMPPARTICLEFONT

static void R_InitParticleTexture (void)
{
	int x, y, d, i, k, m;
	float dx, dy, f;
	vec3_t light;

	// a note: decals need to modulate (multiply) the background color to
	// properly darken it (stain), and they need to be able to alpha fade,
	// this is a very difficult challenge because it means fading to white
	// (no change to background) rather than black (darkening everything
	// behind the whole decal polygon), and to accomplish this the texture is
	// inverted (dark red blood on white background becomes brilliant cyan
	// and white on black background) so we can alpha fade it to black, then
	// we invert it again during the blendfunc to make it work...

#ifndef DUMPPARTICLEFONT
	particlefonttexture = loadtextureimage(particletexturepool, "particles/particlefont.tga", 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (!particlefonttexture)
#endif
	{
		unsigned char *particletexturedata = (unsigned char *)Mem_Alloc(tempmempool, PARTICLEFONTSIZE*PARTICLEFONTSIZE*4);
		unsigned char data[PARTICLETEXTURESIZE][PARTICLETEXTURESIZE][4];
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
							d = (int)(d * (1-(dx*dx+dy*dy)));
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
		memset(&data[0][0][0], 255, sizeof(data));
		for (y = 0;y < PARTICLETEXTURESIZE;y++)
		{
			dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			for (x = 0;x < PARTICLETEXTURESIZE;x++)
			{
				dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
				f = 255.0f * (1.0 - 4.0f * fabs(10.0f - sqrt(dx*dx+dy*dy)));
				data[y][x][3] = (int) (bound(0.0f, f, 255.0f));
			}
		}
		setuptex(tex_rainsplash, &data[0][0][0], particletexturedata);

		// normal particle
		memset(&data[0][0][0], 255, sizeof(data));
		for (y = 0;y < PARTICLETEXTURESIZE;y++)
		{
			dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			for (x = 0;x < PARTICLETEXTURESIZE;x++)
			{
				dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
				d = (int)(256 * (1 - (dx*dx+dy*dy)));
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

#ifdef DUMPPARTICLEFONT
		Image_WriteTGARGBA ("particles/particlefont.tga", PARTICLEFONTSIZE, PARTICLEFONTSIZE, particletexturedata);
#endif

		particlefonttexture = R_LoadTexture2D(particletexturepool, "particlefont", PARTICLEFONTSIZE, PARTICLEFONTSIZE, particletexturedata, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);

		Mem_Free(particletexturedata);
	}
	for (i = 0;i < MAX_PARTICLETEXTURES;i++)
	{
		int basex = ((i >> 0) & 7) * PARTICLETEXTURESIZE;
		int basey = ((i >> 3) & 7) * PARTICLETEXTURESIZE;
		particletexture[i].texture = particlefonttexture;
		particletexture[i].s1 = (basex + 1) / (float)PARTICLEFONTSIZE;
		particletexture[i].t1 = (basey + 1) / (float)PARTICLEFONTSIZE;
		particletexture[i].s2 = (basex + PARTICLETEXTURESIZE - 1) / (float)PARTICLEFONTSIZE;
		particletexture[i].t2 = (basey + PARTICLETEXTURESIZE - 1) / (float)PARTICLEFONTSIZE;
	}

#ifndef DUMPPARTICLEFONT
	particletexture[tex_beam].texture = loadtextureimage(particletexturepool, "particles/nexbeam.tga", 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (!particletexture[tex_beam].texture)
#endif
	{
		unsigned char noise3[64][64], data2[64][16][4];
		// nexbeam
		fractalnoise(&noise3[0][0], 64, 4);
		m = 0;
		for (y = 0;y < 64;y++)
		{
			dy = (y - 0.5f*64) / (64*0.5f-1);
			for (x = 0;x < 16;x++)
			{
				dx = (x - 0.5f*16) / (16*0.5f-2);
				d = (int)((1 - sqrt(fabs(dx))) * noise3[y][x]);
				data2[y][x][0] = data2[y][x][1] = data2[y][x][2] = (unsigned char) bound(0, d, 255);
				data2[y][x][3] = 255;
			}
		}

#ifdef DUMPPARTICLEFONT
		Image_WriteTGARGBA ("particles/nexbeam.tga", 64, 64, &data2[0][0][0]);
#endif
		particletexture[tex_beam].texture = R_LoadTexture2D(particletexturepool, "nexbeam", 16, 64, &data2[0][0][0], TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	}
	particletexture[tex_beam].s1 = 0;
	particletexture[tex_beam].t1 = 0;
	particletexture[tex_beam].s2 = 1;
	particletexture[tex_beam].t2 = 1;
}

static void r_part_start(void)
{
	particletexturepool = R_AllocTexturePool();
	R_InitParticleTexture ();
	CL_Particles_LoadEffectInfo();
}

static void r_part_shutdown(void)
{
	R_FreeTexturePool(&particletexturepool);
}

static void r_part_newmap(void)
{
}

#define BATCHSIZE 256
int particle_element3i[BATCHSIZE*6];
float particle_vertex3f[BATCHSIZE*12], particle_texcoord2f[BATCHSIZE*8], particle_color4f[BATCHSIZE*16];

void R_Particles_Init (void)
{
	int i;
	for (i = 0;i < BATCHSIZE;i++)
	{
		particle_element3i[i*6+0] = i*4+0;
		particle_element3i[i*6+1] = i*4+1;
		particle_element3i[i*6+2] = i*4+2;
		particle_element3i[i*6+3] = i*4+0;
		particle_element3i[i*6+4] = i*4+2;
		particle_element3i[i*6+5] = i*4+3;
	}

	Cvar_RegisterVariable(&r_drawparticles);
	R_RegisterModule("R_Particles", r_part_start, r_part_shutdown, r_part_newmap);
}

void R_DrawParticle_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int surfacelistindex;
	int batchstart, batchcount;
	const particle_t *p;
	pblend_t blendmode;
	rtexture_t *texture;
	float *v3f, *t2f, *c4f;

	R_Mesh_Matrix(&identitymatrix);
	R_Mesh_ResetTextureState();
	R_Mesh_VertexPointer(particle_vertex3f, 0, 0);
	R_Mesh_TexCoordPointer(0, 2, particle_texcoord2f, 0, 0);
	R_Mesh_ColorPointer(particle_color4f, 0, 0);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(true);
	GL_CullFace(GL_NONE);

	// first generate all the vertices at once
	for (surfacelistindex = 0, v3f = particle_vertex3f, t2f = particle_texcoord2f, c4f = particle_color4f;surfacelistindex < numsurfaces;surfacelistindex++, v3f += 3*4, t2f += 2*4, c4f += 4*4)
	{
		particletexture_t *tex;
		const float *org;
		float up2[3], v[3], right[3], up[3], fog, cr, cg, cb, ca, size;

		p = cl.particles + surfacelist[surfacelistindex];

		blendmode = p->type->blendmode;

		cr = p->color[0] * (1.0f / 255.0f) * r_view.colorscale;
		cg = p->color[1] * (1.0f / 255.0f) * r_view.colorscale;
		cb = p->color[2] * (1.0f / 255.0f) * r_view.colorscale;
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
		ca *= cl_particles_alpha.value;
		if (p->type->lighting)
		{
			float ambient[3], diffuse[3], diffusenormal[3];
			R_CompleteLightPoint(ambient, diffuse, diffusenormal, p->org, true);
			cr *= (ambient[0] + 0.5 * diffuse[0]);
			cg *= (ambient[1] + 0.5 * diffuse[1]);
			cb *= (ambient[2] + 0.5 * diffuse[2]);
		}
		if (r_refdef.fogenabled)
		{
			fog = FogPoint_World(p->org);
			cr = cr * fog;
			cg = cg * fog;
			cb = cb * fog;
			if (blendmode == PBLEND_ALPHA)
			{
				fog = 1 - fog;
				cr += r_refdef.fogcolor[0] * fog * r_view.colorscale;
				cg += r_refdef.fogcolor[1] * fog * r_view.colorscale;
				cb += r_refdef.fogcolor[2] * fog * r_view.colorscale;
			}
		}
		c4f[0] = c4f[4] = c4f[8] = c4f[12] = cr;
		c4f[1] = c4f[5] = c4f[9] = c4f[13] = cg;
		c4f[2] = c4f[6] = c4f[10] = c4f[14] = cb;
		c4f[3] = c4f[7] = c4f[11] = c4f[15] = ca;

		size = p->size * cl_particles_size.value;
		org = p->org;
		tex = &particletexture[p->texnum];
		if (p->type->orientation == PARTICLE_BILLBOARD)
		{
			VectorScale(r_view.left, -size, right);
			VectorScale(r_view.up, size, up);
			v3f[ 0] = org[0] - right[0] - up[0];
			v3f[ 1] = org[1] - right[1] - up[1];
			v3f[ 2] = org[2] - right[2] - up[2];
			v3f[ 3] = org[0] - right[0] + up[0];
			v3f[ 4] = org[1] - right[1] + up[1];
			v3f[ 5] = org[2] - right[2] + up[2];
			v3f[ 6] = org[0] + right[0] + up[0];
			v3f[ 7] = org[1] + right[1] + up[1];
			v3f[ 8] = org[2] + right[2] + up[2];
			v3f[ 9] = org[0] + right[0] - up[0];
			v3f[10] = org[1] + right[1] - up[1];
			v3f[11] = org[2] + right[2] - up[2];
			t2f[0] = tex->s1;t2f[1] = tex->t2;
			t2f[2] = tex->s1;t2f[3] = tex->t1;
			t2f[4] = tex->s2;t2f[5] = tex->t1;
			t2f[6] = tex->s2;t2f[7] = tex->t2;
		}
		else if (p->type->orientation == PARTICLE_ORIENTED_DOUBLESIDED)
		{
			VectorVectors(p->vel, right, up);
			VectorScale(right, size, right);
			VectorScale(up, size, up);
			v3f[ 0] = org[0] - right[0] - up[0];
			v3f[ 1] = org[1] - right[1] - up[1];
			v3f[ 2] = org[2] - right[2] - up[2];
			v3f[ 3] = org[0] - right[0] + up[0];
			v3f[ 4] = org[1] - right[1] + up[1];
			v3f[ 5] = org[2] - right[2] + up[2];
			v3f[ 6] = org[0] + right[0] + up[0];
			v3f[ 7] = org[1] + right[1] + up[1];
			v3f[ 8] = org[2] + right[2] + up[2];
			v3f[ 9] = org[0] + right[0] - up[0];
			v3f[10] = org[1] + right[1] - up[1];
			v3f[11] = org[2] + right[2] - up[2];
			t2f[0] = tex->s1;t2f[1] = tex->t2;
			t2f[2] = tex->s1;t2f[3] = tex->t1;
			t2f[4] = tex->s2;t2f[5] = tex->t1;
			t2f[6] = tex->s2;t2f[7] = tex->t2;
		}
		else if (p->type->orientation == PARTICLE_SPARK)
		{
			VectorMA(org, -0.02, p->vel, v);
			VectorMA(org, 0.02, p->vel, up2);
			R_CalcBeam_Vertex3f(v3f, v, up2, size);
			t2f[0] = tex->s1;t2f[1] = tex->t2;
			t2f[2] = tex->s1;t2f[3] = tex->t1;
			t2f[4] = tex->s2;t2f[5] = tex->t1;
			t2f[6] = tex->s2;t2f[7] = tex->t2;
		}
		else if (p->type->orientation == PARTICLE_BEAM)
		{
			R_CalcBeam_Vertex3f(v3f, org, p->vel, size);
			VectorSubtract(p->vel, org, up);
			VectorNormalize(up);
			v[0] = DotProduct(org, up) * (1.0f / 64.0f);
			v[1] = DotProduct(p->vel, up) * (1.0f / 64.0f);
			t2f[0] = 1;t2f[1] = v[0];
			t2f[2] = 0;t2f[3] = v[0];
			t2f[4] = 0;t2f[5] = v[1];
			t2f[6] = 1;t2f[7] = v[1];
		}
		else
		{
			Con_Printf("R_DrawParticles: unknown particle orientation %i\n", p->type->orientation);
			return;
		}
	}

	// now render batches of particles based on blendmode and texture
	blendmode = PBLEND_ADD;
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	texture = particletexture[63].texture;
	R_Mesh_TexBind(0, R_GetTexture(texture));
	GL_LockArrays(0, numsurfaces*4);
	batchstart = 0;
	batchcount = 0;
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		p = cl.particles + surfacelist[surfacelistindex];

		if (blendmode != p->type->blendmode)
		{
			if (batchcount > 0)
				R_Mesh_Draw(batchstart * 4, batchcount * 4, batchcount * 2, particle_element3i + batchstart * 6, 0, 0);
			batchcount = 0;
			batchstart = surfacelistindex;
			blendmode = p->type->blendmode;
			if (blendmode == PBLEND_ALPHA)
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else if (blendmode == PBLEND_ADD)
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			else //if (blendmode == PBLEND_MOD)
				GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		}
		if (texture != particletexture[p->texnum].texture)
		{
			if (batchcount > 0)
				R_Mesh_Draw(batchstart * 4, batchcount * 4, batchcount * 2, particle_element3i + batchstart * 6, 0, 0);
			batchcount = 0;
			batchstart = surfacelistindex;
			texture = particletexture[p->texnum].texture;
			R_Mesh_TexBind(0, R_GetTexture(texture));
		}

		batchcount++;
	}
	if (batchcount > 0)
		R_Mesh_Draw(batchstart * 4, batchcount * 4, batchcount * 2, particle_element3i + batchstart * 6, 0, 0);
	GL_LockArrays(0, 0);
}

void R_DrawParticles (void)
{
	int i;
	float minparticledist;
	particle_t *p;

	// LordHavoc: early out conditions
	if ((!cl.num_particles) || (!r_drawparticles.integer))
		return;

	minparticledist = DotProduct(r_view.origin, r_view.forward) + 4.0f;

	// LordHavoc: only render if not too close
	for (i = 0, p = cl.particles;i < cl.num_particles;i++, p++)
	{
		if (p->type && !p->delayedspawn)
		{
			r_refdef.stats.particles++;
			if (DotProduct(p->org, r_view.forward) >= minparticledist || p->type->orientation == PARTICLE_BEAM)
				R_MeshQueue_AddTransparent(p->org, R_DrawParticle_TransparentCallback, NULL, i, NULL);
		}
	}
}

