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
// protocol.h -- communications protocols

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define	PROTOCOL_VERSION	15
#define	DPPROTOCOL_VERSION1	96
#define	DPPROTOCOL_VERSION2	97
// LordHavoc: I think the 96-99 range was going to run out too soon...  so here I jump to 3500
#define	DPPROTOCOL_VERSION3	3500

// model effects
#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail
// entity effects
#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8
#define	EF_NODRAW				16
#define EF_ADDITIVE				32
#define EF_BLUE					64
#define EF_RED					128
#define EF_DELTA				8388608	// LordHavoc: (obsolete) entity is delta compressed to save network bandwidth  (no longer used)
#define EF_LOWPRECISION			4194304 // LordHavoc: entity is low precision (integer coordinates) to save network bandwidth
// effects/model (can be used as model flags or entity effects)
#define	EF_REFLECTIVE			256		// LordHavoc: shiny metal objects :)  (not currently supported)
#define EF_FULLBRIGHT			512		// LordHavoc: fullbright
#define EF_FLAME				1024	// LordHavoc: on fire
#define EF_STARDUST				2048	// LordHavoc: showering sparks

#define EF_STEP					0x80000000 // internal client use only - present on MOVETYPE_STEP entities, not QC accessible (too many bits)

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define U_MOREBITS		(1<<0)
#define U_ORIGIN1		(1<<1)
#define U_ORIGIN2		(1<<2)
#define U_ORIGIN3		(1<<3)
#define U_ANGLE2		(1<<4)
// LordHavoc: U_NOLERP was only ever used for monsters, so I renamed it U_STEP
#define U_STEP			(1<<5)
#define U_FRAME			(1<<6)
// just differentiates from other updates
#define U_SIGNAL		(1<<7)

#define U_ANGLE1		(1<<8)
#define U_ANGLE3		(1<<9)
#define U_MODEL			(1<<10)
#define U_COLORMAP		(1<<11)
#define U_SKIN			(1<<12)
#define U_EFFECTS		(1<<13)
#define U_LONGENTITY	(1<<14)

// LordHavoc: protocol extension
#define U_EXTEND1		(1<<15)
// LordHavoc: first extend byte
#define U_DELTA			(1<<16) // no data, while this is set the entity is delta compressed (uses previous frame as a baseline, meaning only things that have changed from the previous frame are sent, except for the forced full update every half second)
#define U_ALPHA			(1<<17) // 1 byte, 0.0-1.0 maps to 0-255, not sent if exactly 1, and the entity is not sent if <=0 unless it has effects (model effects are checked as well)
#define U_SCALE			(1<<18) // 1 byte, scale / 16 positive, not sent if 1.0
#define U_EFFECTS2		(1<<19) // 1 byte, this is .effects & 0xFF00 (second byte)
#define U_GLOWSIZE		(1<<20) // 1 byte, encoding is float/8.0, signed (negative is darklight), not sent if 0
#define U_GLOWCOLOR		(1<<21) // 1 byte, palette index, default is 254 (white), this IS used for darklight (allowing colored darklight), however the particles from a darklight are always black, not sent if default value (even if glowsize or glowtrail is set)
// LordHavoc: colormod feature has been removed, because no one used it
#define U_COLORMOD		(1<<22) // 1 byte, 3 bit red, 3 bit green, 2 bit blue, this lets you tint an object artifically, so you could make a red rocket, or a blue fiend...
#define U_EXTEND2		(1<<23) // another byte to follow
// LordHavoc: second extend byte
#define U_GLOWTRAIL		(1<<24) // leaves a trail of particles (of color .glowcolor, or black if it is a negative glowsize)
#define U_VIEWMODEL		(1<<25) // attachs the model to the view (origin and angles become relative to it), only shown to owner, a more powerful alternative to .weaponmodel and such
#define U_FRAME2		(1<<26) // 1 byte, this is .frame & 0xFF00 (second byte)
#define U_MODEL2		(1<<27) // 1 byte, this is .modelindex & 0xFF00 (second byte)
#define U_EXTERIORMODEL	(1<<28) // causes this model to not be drawn when using a first person view (third person will draw it, first person will not)
#define U_UNUSED29		(1<<29) // future expansion
#define U_UNUSED30		(1<<30) // future expansion
#define U_EXTEND3		(1<<31) // another byte to follow, future expansion

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
//define	SU_AIMENT		(1<<8)  AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)
#define SU_EXTEND1		(1<<15)
// first extend byte
#define SU_PUNCHVEC1	(1<<16)
#define SU_PUNCHVEC2	(1<<17)
#define SU_PUNCHVEC3	(1<<18)
#define SU_VIEWZOOM		(1<<19) // byte factor (0 = 0.0 (not valid), 255 = 1.0)
#define SU_UNUSED20		(1<<20)
#define SU_UNUSED21		(1<<21)
#define SU_UNUSED22		(1<<22)
#define SU_EXTEND2		(1<<23) // another byte to follow, future expansion
// second extend byte
#define SU_UNUSED24		(1<<24)
#define SU_UNUSED25		(1<<25)
#define SU_UNUSED26		(1<<26)
#define SU_UNUSED27		(1<<27)
#define SU_UNUSED28		(1<<28)
#define SU_UNUSED29		(1<<29)
#define SU_UNUSED30		(1<<30)
#define SU_EXTEND3		(1<<31) // another byte to follow, future expansion

// a sound with no channel is a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_LOOPING		(1<<2)		// a long


// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	22


// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_updatestat		3	// [byte] [long]
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
								// the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value

#define	svc_serverinfo		11	// [long] version
						// [string] signon string
						// [string]..[0]model cache
						// [string]...[0]sounds cache
#define	svc_lightstyle		12	// [byte] [string]
#define	svc_updatename		13	// [byte] [string]
#define	svc_updatefrags		14	// [byte] [short]
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_updatecolors	17	// [byte] [byte]
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19

#define	svc_spawnstatic		20
//	svc_spawnbinary		21
#define	svc_spawnbaseline	22

#define	svc_temp_entity		23

#define	svc_setpause		24	// [byte] on / off
#define	svc_signonnum		25	// [byte]  used for the signon sequence

#define	svc_centerprint		26	// [string] to put in center of the screen

#define	svc_killedmonster	27
#define	svc_foundsecret		28

#define	svc_spawnstaticsound	29	// [coord3] [byte] samp [byte] vol [byte] aten

#define	svc_intermission	30		// [string] music
#define	svc_finale			31		// [string] music [string] text

#define	svc_cdtrack			32		// [byte] track [byte] looptrack
#define svc_sellscreen		33

#define svc_cutscene		34

#define	svc_showlmp			35		// [string] slotname [string] lmpfilename [short] x [short] y
#define	svc_hidelmp			36		// [string] slotname
#define	svc_skybox			37		// [string] skyname

// LordHavoc: my svc_ range, 50-59
#define svc_cgame			50		// [short] length [bytes] data
#define svc_unusedlh1		51
#define svc_effect			52		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
#define svc_effect2			53		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
#define	svc_sound2			54		// short soundindex instead of byte
#define	svc_spawnbaseline2	55		// short modelindex instead of byte
#define svc_spawnstatic2	56		// short modelindex instead of byte
#define svc_entities		57		// [int] deltaframe [int] thisframe [float vector] eye [variable length] entitydata
#define svc_unusedlh3			58
#define	svc_spawnstaticsound2	59	// [coord3] [short] samp [byte] vol [byte] aten

//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_disconnect	2
#define	clc_move		3			// [usercmd_t]
#define	clc_stringcmd	4		// [string] message

// LordHavoc: my clc_ range, 50-59
#define clc_ackentities	50		// [int] framenumber
#define clc_unusedlh1 	51
#define clc_unusedlh2 	52
#define clc_unusedlh3 	53
#define clc_unusedlh4 	54
#define clc_unusedlh5 	55
#define clc_unusedlh6 	56
#define clc_unusedlh7 	57
#define clc_unusedlh8 	58
#define clc_unusedlh9 	59

//
// temp entity events
//
#define	TE_SPIKE			0 // [vector] origin
#define	TE_SUPERSPIKE		1 // [vector] origin
#define	TE_GUNSHOT			2 // [vector] origin
#define	TE_EXPLOSION		3 // [vector] origin
#define	TE_TAREXPLOSION		4 // [vector] origin
#define	TE_LIGHTNING1		5 // [entity] entity [vector] start [vector] end
#define	TE_LIGHTNING2		6 // [entity] entity [vector] start [vector] end
#define	TE_WIZSPIKE			7 // [vector] origin
#define	TE_KNIGHTSPIKE		8 // [vector] origin
#define	TE_LIGHTNING3		9 // [entity] entity [vector] start [vector] end
#define	TE_LAVASPLASH		10 // [vector] origin
#define	TE_TELEPORT			11 // [vector] origin
#define TE_EXPLOSION2		12 // [vector] origin [byte] startcolor [byte] colorcount

// PGM 01/21/97
#define TE_BEAM				13 // [entity] entity [vector] start [vector] end
// PGM 01/21/97

// Nehahra effects used in the movie (TE_EXPLOSION3 also got written up in a QSG tutorial, hence it's not marked NEH)
#define	TE_EXPLOSION3		16 // [vector] origin [coord] red [coord] green [coord] blue
#define TE_LIGHTNING4NEH	17 // [string] model [entity] entity [vector] start [vector] end

// LordHavoc: added some TE_ codes (block1 - 50-60)
#define	TE_BLOOD			50 // [vector] origin [byte] xvel [byte] yvel [byte] zvel [byte] count
#define	TE_SPARK			51 // [vector] origin [byte] xvel [byte] yvel [byte] zvel [byte] count
#define	TE_BLOODSHOWER		52 // [vector] min [vector] max [coord] explosionspeed [short] count
#define	TE_EXPLOSIONRGB		53 // [vector] origin [byte] red [byte] green [byte] blue
#define TE_PARTICLECUBE		54 // [vector] min [vector] max [vector] dir [short] count [byte] color [byte] gravity [coord] randomvel
#define TE_PARTICLERAIN		55 // [vector] min [vector] max [vector] dir [short] count [byte] color
#define TE_PARTICLESNOW		56 // [vector] min [vector] max [vector] dir [short] count [byte] color
#define TE_GUNSHOTQUAD		57 // [vector] origin
#define TE_SPIKEQUAD		58 // [vector] origin
#define TE_SUPERSPIKEQUAD	59 // [vector] origin
// LordHavoc: block2 - 70-80
#define TE_EXPLOSIONQUAD	70 // [vector] origin
#define	TE_BLOOD2			71 // [vector] origin
#define TE_SMALLFLASH		72 // [vector] origin
#define TE_CUSTOMFLASH		73 // [vector] origin [byte] radius / 8 - 1 [byte] lifetime / 256 - 1 [byte] red [byte] green [byte] blue
#define TE_FLAMEJET			74 // [vector] origin [vector] velocity [byte] count
#define TE_PLASMABURN		75 // [vector] origin

// these are bits for the 'flags' field of the entity_state_t
#define RENDER_STEP 1
#define RENDER_GLOWTRAIL 2
#define RENDER_VIEWMODEL 4
#define RENDER_EXTERIORMODEL 8
#define RENDER_LOWPRECISION 16 // send as low precision coordinates to save bandwidth

typedef struct
{
	double time; // time this state was built
	vec3_t origin;
	vec3_t angles;
	int number; // entity number this state is for
	unsigned short active; // true if a valid state
	unsigned short modelindex;
	unsigned short frame;
	unsigned short effects;
	qbyte colormap;
	qbyte skin;
	qbyte alpha;
	qbyte scale;
	qbyte glowsize;
	qbyte glowcolor;
	qbyte flags;
}
entity_state_t;

typedef struct
{
	double time;
	int framenum;
	int firstentity; // index into entitydata, modulo MAX_ENTITY_DATABASE
	int endentity; // index into entitydata, firstentity + numentities
}
entity_frameinfo_t;

#define MAX_ENTITY_HISTORY 64
#define MAX_ENTITY_DATABASE 4096

typedef struct
{
	// note: these can be far out of range, modulo with MAX_ENTITY_DATABASE to get a valid range (which may wrap)
	// start and end of used area, when adding a new update to database, store at endpos, and increment endpos
	// when removing updates from database, nudge down frames array to only contain useful frames
	// this logic should explain better:
	// if (numframes >= MAX_ENTITY_HISTORY || (frames[numframes - 1].endentity - frames[0].firstentity) + entitiestoadd > MAX_ENTITY_DATABASE)
	//     flushdatabase();
	// note: if numframes == 0, insert at start (0 in entitydata)
	// the only reason this system is used is to avoid copying memory when frames are removed
	int numframes;
	int ackframe; // server only: last acknowledged frame
	vec3_t eye;
	entity_frameinfo_t frames[MAX_ENTITY_HISTORY];
	entity_state_t entitydata[MAX_ENTITY_DATABASE];
}
entity_database_t;

// build entity data in this, to pass to entity read/write functions
typedef struct
{
	double time;
	int framenum;
	int numentities;
	vec3_t eye;
	entity_state_t entitydata[MAX_ENTITY_DATABASE];
}
entity_frame_t;

// LordHavoc: these are in approximately sorted order, according to cost and
// likelyhood of being used for numerous objects in a frame

// note that the bytes are not written/read in this order, this is only the
// order of the bits to minimize overhead from extend bytes

// enough to describe a nail, gib, shell casing, bullet hole, or rocket
#define E_ORIGIN1		(1<<0)
#define E_ORIGIN2		(1<<1)
#define E_ORIGIN3		(1<<2)
#define E_ANGLE1		(1<<3)
#define E_ANGLE2		(1<<4)
#define E_ANGLE3		(1<<5)
#define E_MODEL1		(1<<6)
#define E_EXTEND1		(1<<7)

// enough to describe almost anything
#define E_FRAME1		(1<<8)
#define E_EFFECTS1		(1<<9)
#define E_ALPHA			(1<<10)
#define E_SCALE			(1<<11)
#define E_COLORMAP		(1<<12)
#define E_SKIN			(1<<13)
#define E_FLAGS			(1<<14)
#define E_EXTEND2		(1<<15)

// players, custom color glows, high model numbers
#define E_FRAME2		(1<<16)
#define E_MODEL2		(1<<17)
#define E_EFFECTS2		(1<<18)
#define E_GLOWSIZE		(1<<19)
#define E_GLOWCOLOR		(1<<20)
#define E_UNUSED1		(1<<21)
#define E_UNUSED2		(1<<22)
#define E_EXTEND3		(1<<23)

#define E_SOUND1		(1<<24)
#define E_SOUNDVOL		(1<<25)
#define E_SOUNDATTEN	(1<<26)
#define E_UNUSED4		(1<<27)
#define E_UNUSED5		(1<<28)
#define E_UNUSED6		(1<<29)
#define E_UNUSED7		(1<<30)
#define E_EXTEND4		(1<<31)

void ClearStateToDefault(entity_state_t *s);
// (server) clears the database to contain no frames (thus delta compression
// compresses against nothing)
void EntityFrame_ClearDatabase(entity_database_t *d);
// (server and client) removes frames older than 'frame' from database
void EntityFrame_AckFrame(entity_database_t *d, int frame);
// (server) clears frame, to prepare for adding entities
void EntityFrame_Clear(entity_frame_t *f, vec3_t eye);
// (server) allocates an entity slot in frame, returns NULL if full
entity_state_t *EntityFrame_NewEntity(entity_frame_t *f, int number);
// (server and client) reads a frame from the database
void EntityFrame_FetchFrame(entity_database_t *d, int framenum, entity_frame_t *f);
// (server and client) adds a entity_frame to the database, for future
// reference
void EntityFrame_AddFrame(entity_database_t *d, entity_frame_t *f);
// (server) writes a frame to network stream
void EntityFrame_Write(entity_database_t *d, entity_frame_t *f, sizebuf_t *msg);
// (client) reads a frame from network stream
void EntityFrame_Read(entity_database_t *d);
// (client) returns the frame number of the most recent frame recieved
int EntityFrame_MostRecentlyRecievedFrameNum(entity_database_t *d);

#endif

