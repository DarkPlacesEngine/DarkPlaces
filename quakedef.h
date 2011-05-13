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
// quakedef.h -- primary header for client

#ifndef QUAKEDEF_H
#define QUAKEDEF_H

#if defined(__GNUC__) && (__GNUC__ > 2)
#define DP_FUNC_PRINTF(n) __attribute__ ((format (printf, n, n+1)))
#define DP_FUNC_PURE      __attribute__ ((pure))
#else
#define DP_FUNC_PRINTF(n)
#define DP_FUNC_PURE
#endif

#include <sys/types.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <setjmp.h>

#include "qtypes.h"

extern const char *buildstring;
extern char engineversion[128];

#define GAMENAME "id1"

#define MAX_NUM_ARGVS	50

#ifdef DP_SMALLMEMORY
#define	MAX_INPUTLINE			1024
#define	CON_TEXTSIZE			16384
#define	CON_MAXLINES			256
#define	HIST_TEXTSIZE			2048
#define	HIST_MAXLINES			16
#define	MAX_ALIAS_NAME			32
#define	CMDBUFSIZE				131072
#define	MAX_ARGS				80

#define	NET_MAXMESSAGE			65536
#define	MAX_PACKETFRAGMENT		1024
#define	MAX_EDICTS				4096
#define	MAX_MODELS				1024
#define	MAX_SOUNDS				1024
#define	MAX_LIGHTSTYLES			64
#define	MAX_STYLESTRING			16
#define	MAX_SCOREBOARD			32
#define	MAX_SCOREBOARDNAME		128
#define	MAX_USERINFO_STRING		196
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	1 // not actually used by DP servers
#define	CL_MAX_USERCMDS			32
#define	CVAR_HASHSIZE			1024
#define	M_MAX_EDICTS			4096
#define	MAX_DEMOS				8
#define	MAX_DEMONAME			16
#define	MAX_SAVEGAMES			12
#define	SAVEGAME_COMMENT_LENGTH	39
#define	MAX_CLIENTNETWORKEYES	2
#define	MAX_LEVELNETWORKEYES	0 // no portal support
#define	MAX_OCCLUSION_QUERIES	256

#define CRYPTO_HOSTKEY_HASHSIZE 256
#define MAX_NETWM_ICON 1026 // one 32x32

#define	MAX_WATERPLANES			2
#define	MAX_CUBEMAPS			1024
#define	MAX_EXPLOSIONS			8
#define	MAX_DLIGHTS				16
#define	MAX_CACHED_PICS			1024 // this is 144 bytes each (or 152 on 64bit)
#define	CACHEPICHASHSIZE		256
#define	MAX_PARTICLEEFFECTNAME	256
#define	MAX_PARTICLEEFFECTINFO	1024
#define	MAX_PARTICLETEXTURES	256
#define	MAXCLVIDEOS				1
#define	MAX_GECKO_INSTANCES		1
#define	MAX_DYNAMIC_TEXTURE_COUNT	2
#define	MAX_MAP_LEAFS			8192

#define	MAXTRACKS				256
#define	MAX_DYNAMIC_CHANNELS	64
#define	MAX_CHANNELS			260
#define	MODLIST_TOTALSIZE		32
#define	MAX_FAVORITESERVERS		32
#define	MAX_DECALSYSTEM_QUEUE	64
#define	PAINTBUFFER_SIZE		512
#define	MAX_BINDMAPS			8
#define	MAX_PARTICLES_INITIAL	8192
#define	MAX_PARTICLES			8192
#define	MAX_DECALS_INITIAL		1024
#define	MAX_DECALS				1024
#define	MAX_ENITIES_INITIAL		256
#define	MAX_STATICENTITIES		256
#define	MAX_EFFECTS				16
#define	MAX_BEAMS				16
#define	MAX_TEMPENTITIES		256
#else
#define	MAX_INPUTLINE			16384 ///< maximum length of console commandline, QuakeC strings, and many other text processing buffers
#define	CON_TEXTSIZE			1048576 ///< max scrollback buffer characters in console
#define	CON_MAXLINES			16384 ///< max scrollback buffer lines in console
#define	HIST_TEXTSIZE			262144 ///< max command history buffer characters in console
#define	HIST_MAXLINES			4096 ///< max command history buffer lines in console
#define	MAX_ALIAS_NAME			32
#define	CMDBUFSIZE				655360 ///< maximum script size that can be loaded by the exec command (8192 in Quake)
#define	MAX_ARGS				80 ///< maximum number of parameters to a console command or alias

#define	NET_MAXMESSAGE			65536 ///< max reliable packet size (sent as multiple fragments of MAX_PACKETFRAGMENT)
#define	MAX_PACKETFRAGMENT		1024 ///< max length of packet fragment
#define	MAX_EDICTS				32768 ///< max number of objects in game world at once (32768 protocol limit)
#define	MAX_MODELS				8192 ///< max number of models loaded at once (including during level transitions)
#define	MAX_SOUNDS				4096 ///< max number of sounds loaded at once
#define	MAX_LIGHTSTYLES			256 ///< max flickering light styles in level (note: affects savegame format)
#define	MAX_STYLESTRING			64 ///< max length of flicker pattern for light style
#define	MAX_SCOREBOARD			255 ///< max number of players in game at once (255 protocol limit)
#define	MAX_SCOREBOARDNAME		128 ///< max length of player name in game
#define	MAX_USERINFO_STRING		1280 ///< max length of infostring for PROTOCOL_QUAKEWORLD (196 in QuakeWorld)
#define	MAX_SERVERINFO_STRING	1280 ///< max length of server infostring for PROTOCOL_QUAKEWORLD (512 in QuakeWorld)
#define	MAX_LOCALINFO_STRING	32768 ///< max length of server-local infostring for PROTOCOL_QUAKEWORLD (32768 in QuakeWorld)
#define	CL_MAX_USERCMDS			128 ///< max number of predicted input packets in queue
#define	CVAR_HASHSIZE			65536 ///< number of hash buckets for accelerating cvar name lookups
#define	M_MAX_EDICTS			32768 ///< max objects in menu vm
#define	MAX_DEMOS				8 ///< max demos provided to demos command
#define	MAX_DEMONAME			16 ///< max demo name length for demos command
#define	MAX_SAVEGAMES			12 ///< max savegames listed in savegame menu
#define	SAVEGAME_COMMENT_LENGTH	39 ///< max comment length of savegame in menu
#define	MAX_CLIENTNETWORKEYES	16 ///< max number of locations that can be added to pvs when culling network entities (must be at least 2 for prediction)
#define	MAX_LEVELNETWORKEYES	512 ///< max number of locations that can be added to pvs when culling network entities (must be at least 2 for prediction)
#define	MAX_OCCLUSION_QUERIES	4096 ///< max number of GL_ARB_occlusion_query objects that can be used in one frame

#define CRYPTO_HOSTKEY_HASHSIZE 8192 ///< number of hash buckets for accelerating host key lookups
#define MAX_NETWM_ICON 352822 // 16x16, 22x22, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256, 512x512

#define	MAX_WATERPLANES			16 ///< max number of water planes visible (each one causes additional view renders)
#define	MAX_CUBEMAPS			1024 ///< max number of cubemap textures loaded for light filters
#define	MAX_EXPLOSIONS			64 ///< max number of explosion shell effects active at once (not particle related)
#define	MAX_DLIGHTS				256 ///< max number of dynamic lights (rocket flashes, etc) in scene at once
#define	MAX_CACHED_PICS			1024 ///< max number of 2D pics loaded at once
#define	CACHEPICHASHSIZE		256 ///< number of hash buckets for accelerating 2D pic name lookups
#define	MAX_PARTICLEEFFECTNAME	256 ///< maximum number of unique names of particle effects (for particleeffectnum)
#define	MAX_PARTICLEEFFECTINFO	4096 ///< maximum number of unique particle effects (each name may associate with several of these)
#define	MAX_PARTICLETEXTURES	256 ///< maximum number of unique particle textures in the particle font
#define	MAXCLVIDEOS				65 ///< maximum number of video streams being played back at once (1 is reserved for the playvideo command)
#define	MAX_GECKO_INSTANCES		16 ///< maximum number of web browser textures active at once
#define	MAX_DYNAMIC_TEXTURE_COUNT	64 ///< maximum number of dynamic textures (web browsers, playvideo, etc)
#define	MAX_MAP_LEAFS			65536 ///< maximum number of BSP leafs in world (8192 in Quake)

#define	MAXTRACKS				256 ///< max CD track index
// 0 to NUM_AMBIENTS - 1 = water, etc
// NUM_AMBIENTS to NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS - 1 = normal entity sounds
// NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS to total_channels = static sounds
#define	MAX_DYNAMIC_CHANNELS	512
#define	MAX_CHANNELS			1028
#define	MODLIST_TOTALSIZE		256
#define	MAX_FAVORITESERVERS		256
#define	MAX_DECALSYSTEM_QUEUE	1024
#define	PAINTBUFFER_SIZE		2048
#define	MAX_BINDMAPS			8
#define	MAX_PARTICLES_INITIAL	8192 ///< initial allocation for cl.particles
#define	MAX_PARTICLES			1048576 ///< upper limit on cl.particles size
#define	MAX_DECALS_INITIAL		8192 ///< initial allocation for cl.decals
#define	MAX_DECALS				1048576 ///< upper limit on cl.decals size
#define	MAX_ENITIES_INITIAL		256 ///< initial size of cl.entities
#define	MAX_STATICENTITIES		1024 ///< limit on size of cl.static_entities
#define	MAX_EFFECTS				256 ///< limit on size of cl.effects
#define	MAX_BEAMS				256 ///< limit on size of cl.beams
#define	MAX_TEMPENTITIES		4096 ///< max number of temporary models visible per frame (certain sprite effects, certain types of CSQC entities also use this)
#endif


#define CMD_TOKENIZELENGTH (MAX_INPUTLINE + MAX_ARGS) ///< maximum tokenizable commandline length (counting trailing 0)


#define	MAX_QPATH		128			///< max length of a quake game pathname
#ifdef PATH_MAX
#define	MAX_OSPATH		PATH_MAX
#elif MAX_PATH
#define	MAX_OSPATH		MAX_PATH
#else
#define	MAX_OSPATH		1024		///< max length of a filesystem pathname
#endif

#define	ON_EPSILON		0.1			///< point on plane side epsilon

#define	NET_MINRATE		1000 ///< limits "rate" and "sv_maxrate" cvars

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		256
#define	STAT_HEALTH			0
//#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		///< bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		///< bumped by svc_killedmonster
#define STAT_ITEMS			15 ///< FTE, DP
#define STAT_VIEWHEIGHT		16 ///< FTE, DP
//#define STAT_TIME			17 ///< FTE
//#define STAT_VIEW2		20 ///< FTE
#define STAT_VIEWZOOM		21 ///< DP
#define STAT_MOVEVARS_AIRACCEL_QW_STRETCHFACTOR 220 ///< DP
#define STAT_MOVEVARS_AIRCONTROL_PENALTY					221 ///< DP
#define STAT_MOVEVARS_AIRSPEEDLIMIT_NONQW 222 ///< DP
#define STAT_MOVEVARS_AIRSTRAFEACCEL_QW 223 ///< DP
#define STAT_MOVEVARS_AIRCONTROL_POWER					224 ///< DP
#define STAT_MOVEFLAGS                              225 ///< DP
#define STAT_MOVEVARS_WARSOWBUNNY_AIRFORWARDACCEL	226 ///< DP
#define STAT_MOVEVARS_WARSOWBUNNY_ACCEL				227 ///< DP
#define STAT_MOVEVARS_WARSOWBUNNY_TOPSPEED			228 ///< DP
#define STAT_MOVEVARS_WARSOWBUNNY_TURNACCEL			229 ///< DP
#define STAT_MOVEVARS_WARSOWBUNNY_BACKTOSIDERATIO	230 ///< DP
#define STAT_MOVEVARS_AIRSTOPACCELERATE				231 ///< DP
#define STAT_MOVEVARS_AIRSTRAFEACCELERATE			232 ///< DP
#define STAT_MOVEVARS_MAXAIRSTRAFESPEED				233 ///< DP
#define STAT_MOVEVARS_AIRCONTROL					234 ///< DP
#define STAT_FRAGLIMIT								235 ///< DP
#define STAT_TIMELIMIT								236 ///< DP
#define STAT_MOVEVARS_WALLFRICTION					237 ///< DP
#define STAT_MOVEVARS_FRICTION						238 ///< DP
#define STAT_MOVEVARS_WATERFRICTION					239 ///< DP
#define STAT_MOVEVARS_TICRATE						240 ///< DP
#define STAT_MOVEVARS_TIMESCALE						241 ///< DP
#define STAT_MOVEVARS_GRAVITY						242 ///< DP
#define STAT_MOVEVARS_STOPSPEED						243 ///< DP
#define STAT_MOVEVARS_MAXSPEED						244 ///< DP
#define STAT_MOVEVARS_SPECTATORMAXSPEED				245 ///< DP
#define STAT_MOVEVARS_ACCELERATE					246 ///< DP
#define STAT_MOVEVARS_AIRACCELERATE					247 ///< DP
#define STAT_MOVEVARS_WATERACCELERATE				248 ///< DP
#define STAT_MOVEVARS_ENTGRAVITY					249 ///< DP
#define STAT_MOVEVARS_JUMPVELOCITY					250 ///< DP
#define STAT_MOVEVARS_EDGEFRICTION					251 ///< DP
#define STAT_MOVEVARS_MAXAIRSPEED					252 ///< DP
#define STAT_MOVEVARS_STEPHEIGHT					253 ///< DP
#define STAT_MOVEVARS_AIRACCEL_QW					254 ///< DP
#define STAT_MOVEVARS_AIRACCEL_SIDEWAYS_FRICTION	255 ///< DP

// moveflags values
#define MOVEFLAG_VALID 0x80000000
#define MOVEFLAG_Q2AIRACCELERATE 0x00000001
#define MOVEFLAG_NOGRAVITYONGROUND 0x00000002
#define MOVEFLAG_GRAVITYUNAFFECTEDBYTICRATE 0x00000004

// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define IT_SUPER_LIGHTNING      128
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
// AK nexuiz changed and added defines

#define NEX_IT_UZI              1
#define NEX_IT_SHOTGUN          2
#define NEX_IT_GRENADE_LAUNCHER 4
#define NEX_IT_ELECTRO          8
#define NEX_IT_CRYLINK          16
#define NEX_IT_NEX              32
#define NEX_IT_HAGAR            64
#define NEX_IT_ROCKET_LAUNCHER  128
#define NEX_IT_SHELLS           256
#define NEX_IT_BULLETS          512
#define NEX_IT_ROCKETS          1024
#define NEX_IT_CELLS            2048
#define NEX_IT_LASER            4094
#define NEX_IT_STRENGTH         8192
#define NEX_IT_INVINCIBLE       16384
#define NEX_IT_SPEED            32768
#define NEX_IT_SLOWMO           65536

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

#include "zone.h"
#include "fs.h"
#include "common.h"
#include "cvar.h"
#include "bspfile.h"
#include "sys.h"
#include "vid.h"
#include "mathlib.h"

#include "r_textures.h"

#include "crypto.h"
#include "draw.h"
#include "screen.h"
#include "netconn.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "model_shared.h"
#include "world.h"
#include "client.h"
#include "render.h"
#include "progs.h"
#include "progsvm.h"
#include "server.h"

#include "input.h"
#include "keys.h"
#include "console.h"
#include "menu.h"

#include "glquake.h"

#include "palette.h"

extern qboolean noclip_anglehack;

extern cvar_t developer;
extern cvar_t developer_extra;
extern cvar_t developer_insane;
extern cvar_t developer_loadfile;
extern cvar_t developer_loading;

#define STARTCONFIGFILENAME "quake.rc"
#define CONFIGFILENAME "config.cfg"

/* Preprocessor macros to identify platform
    DP_OS_NAME 	- "friendly" name of the OS, for humans to read
    DP_OS_STR	- "identifier" of the OS, more suited for code to use
    DP_ARCH_STR	- "identifier" of the processor architecture
 */
#if defined(__linux__)
# define DP_OS_NAME		"Linux"
# define DP_OS_STR		"linux"
#elif defined(_WIN64)
# define DP_OS_NAME		"Windows64"
# define DP_OS_STR		"win64"
#elif defined(WIN32)
# define DP_OS_NAME		"Windows"
# define DP_OS_STR		"win32"
#elif defined(__FreeBSD__)
# define DP_OS_NAME		"FreeBSD"
# define DP_OS_STR		"freebsd"
#elif defined(__NetBSD__)
# define DP_OS_NAME		"NetBSD"
# define DP_OS_STR		"netbsd"
#elif defined(__OpenBSD__)
# define DP_OS_NAME		"OpenBSD"
# define DP_OS_STR		"openbsd"
#elif defined(TARGET_OS_IPHONE)
# define DP_OS_NAME		"iPhoneOS"
# define DP_OS_STR		"iphoneos"
# define USE_GLES2		1
#elif defined(MACOSX)
# define DP_OS_NAME		"Mac OS X"
# define DP_OS_STR		"osx"
#elif defined(__MORPHOS__)
# define DP_OS_NAME		"MorphOS"
# define DP_OS_STR		"morphos"
#else
# define DP_OS_NAME		"Unknown"
# define DP_OS_STR		"unknown"
#endif

#if defined(__GNUC__)
# if defined(__i386__)
#  define DP_ARCH_STR		"686"
#  define SSE_POSSIBLE
#  ifdef __SSE__
#   define SSE_PRESENT
#  endif
#  ifdef __SSE2__
#   define SSE2_PRESENT
#  endif
# elif defined(__x86_64__)
#  define DP_ARCH_STR		"x86_64"
#  define SSE_PRESENT
#  define SSE2_PRESENT
# elif defined(__powerpc__)
#  define DP_ARCH_STR		"ppc"
# endif
#elif defined(_WIN64)
# define DP_ARCH_STR		"x86_64"
# define SSE_PRESENT
# define SSE2_PRESENT
#elif defined(WIN32)
# define DP_ARCH_STR		"x86"
# define SSE_POSSIBLE
#endif

#ifdef SSE_PRESENT
# define SSE_POSSIBLE
#endif

#ifdef NO_SSE
# undef SSE_PRESENT
# undef SSE_POSSIBLE
# undef SSE2_PRESENT
#endif

#ifdef SSE2_PRESENT
#define Sys_HaveSSE() true
#define Sys_HaveSSE2() true
#elif defined(SSE_POSSIBLE)
// runtime detection of SSE/SSE2 capabilities for x86
qboolean Sys_HaveSSE(void);
qboolean Sys_HaveSSE2(void);
#else
#define Sys_HaveSSE() false
#define Sys_HaveSSE2() false
#endif

/// incremented every frame, never reset
extern int host_framecount;
/// not bounded in any way, changed at start of every frame, never reset
extern double realtime;

void Host_InitCommands(void);
void Host_Main(void);
void Host_Shutdown(void);
void Host_StartVideo(void);
void Host_Error(const char *error, ...) DP_FUNC_PRINTF(1);
void Host_Quit_f(void);
void Host_ClientCommands(const char *fmt, ...) DP_FUNC_PRINTF(1);
void Host_ShutdownServer(void);
void Host_Reconnect_f(void);
void Host_NoOperation_f(void);

void Host_AbortCurrentFrame(void);

/// skill level for currently loaded level (in case the user changes the cvar while the level is running, this reflects the level actually in use)
extern int current_skill;

//
// chase
//
extern cvar_t chase_active;
extern cvar_t cl_viewmodel_scale;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);

void fractalnoise(unsigned char *noise, int size, int startgrid);
void fractalnoisequick(unsigned char *noise, int size, int startgrid);
float noise4f(float x, float y, float z, float w);

void Sys_Shared_Init(void);

// Flag in size field of demos to indicate a client->server packet. Demo
// playback will ignore this, but it may be useful to make DP sniff packets to
// debug protocol exploits.
#define DEMOMSG_CLIENT_TO_SERVER 0x80000000

// In Quake, any char in 0..32 counts as whitespace
//#define ISWHITESPACE(ch) ((unsigned char) ch <= (unsigned char) ' ')
#define ISWHITESPACE(ch) (!(ch) || (ch) == ' ' || (ch) == '\t' || (ch) == '\r' || (ch) == '\n')

// This also includes extended characters, and ALL control chars
#define ISWHITESPACEORCONTROL(ch) ((signed char) (ch) <= (signed char) ' ')


#define FLOAT_IS_TRUE_FOR_INT(x) ((x) & 0x7FFFFFFF) // also match "negative zero" floats of value 0x80000000

#endif

