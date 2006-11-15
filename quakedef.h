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


#include <sys/types.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include "qtypes.h"

extern char *buildstring;
extern char engineversion[128];

#define GAMENAME "id1"

#define MAX_NUM_ARGVS	50


#define	MAX_QPATH		128			// max length of a quake game pathname
#define	MAX_OSPATH		1024		// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define MAX_PACKETFRAGMENT 1024		// max length of packet fragment
#define NET_MAXMESSAGE	65536
#define NET_MINRATE		1000 // limits "rate" and "sv_maxrate" cvars
#define NET_MAXRATE		150000 // limits "rate" and "sv_maxrate" cvars

//
// per-level limits
//
// LordHavoc: increased entity limit to 2048 from 600
#define	MAX_EDICTS		32768		// FIXME: ouch! ouch! ouch!
#define	MAX_LIGHTSTYLES	256			// LordHavoc: increased from 64, NOTE special consideration is needed in savegames!
// LordHavoc: increased model and sound limits from 256 and 256 to 4096 and 4096 (and added protocol extensions accordingly to break the 256 barrier)
#define	MAX_MODELS		4096
#define	MAX_SOUNDS		4096

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		256
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
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
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster
#define STAT_ITEMS			15 // FTE, DP
#define STAT_VIEWHEIGHT		16 // FTE, DP
//#define STAT_TIME			17 // FTE
//#define STAT_VIEW2		20 // FTE
#define STAT_VIEWZOOM		21 // DP

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

// LordHavoc: increased player limit from 16 to 255
#define	MAX_SCOREBOARD		255
// LordHavoc: increased name limit from 32 to 64 characters
#define	MAX_SCOREBOARDNAME	64
// infostring sizes used by QuakeWorld support
// was 196
#define MAX_USERINFO_STRING 1280
// was 512
#define MAX_SERVERINFO_STRING 1280
#define MAX_LOCALINFO_STRING 32768

#include "zone.h"
#include "fs.h"
#include "common.h"
#include "cvar.h"
#include "bspfile.h"
#include "sys.h"
#include "vid.h"
#include "mathlib.h"

#include "r_textures.h"

#include "draw.h"
#include "screen.h"
#include "netconn.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "model_shared.h"
#include "client.h"
#include "render.h"
#include "progs.h"
#include "progsvm.h"
#include "server.h"

#include "input.h"
#include "world.h"
#include "keys.h"
#include "console.h"
#include "menu.h"

#include "glquake.h"

#include "palette.h"

extern qboolean noclip_anglehack;

extern char engineversion[128];
extern cvar_t developer;

// incremented every frame, never reset
extern int host_framecount;
// not bounded in any way, changed at start of every frame, never reset
extern double realtime;

void Host_InitCommands(void);
void Host_Main(void);
void Host_Shutdown(void);
void Host_StartVideo(void);
void Host_Error(const char *error, ...);
void Host_Quit_f(void);
void Host_ClientCommands(const char *fmt, ...);
void Host_ShutdownServer(void);
void Host_Reconnect_f(void);

void Host_AbortCurrentFrame(void);

// skill level for currently loaded level (in case the user changes the cvar while the level is running, this reflects the level actually in use)
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

void Sys_Shared_Init(void);

#endif

