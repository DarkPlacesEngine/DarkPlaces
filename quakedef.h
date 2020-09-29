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

#ifdef __APPLE__
# include <TargetConditionals.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "qtypes.h"

extern const char *buildstring;
extern char engineversion[128];

#define GAMENAME "id1"

#include "qdefs.h"
#include "qstats.h"

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
#include "thread.h"
#include "fs.h"
#include "common.h"
#include "com_list.h"
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
#include "host.h"
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
#ifdef CONFIG_MENU
#include "menu.h"
#endif
#include "csprogs.h"

extern qbool noclip_anglehack;

extern cvar_t developer;
extern cvar_t developer_entityparsing;
extern cvar_t developer_extra;
extern cvar_t developer_insane;
extern cvar_t developer_loadfile;
extern cvar_t developer_loading;

extern cvar_t sessionid;

#define STARTCONFIGFILENAME "quake.rc"
#define CONFIGFILENAME "config.cfg"

/* Preprocessor macros to identify platform
    DP_OS_NAME 	- "friendly" name of the OS, for humans to read
    DP_OS_STR	- "identifier" of the OS, more suited for code to use
    DP_ARCH_STR	- "identifier" of the processor architecture
 */
#if defined(__ANDROID__) /* must come first because it also defines linux */
# define DP_OS_NAME		"Android"
# define DP_OS_STR		"android"
# define USE_GLES2		1
# define USE_RWOPS		1
# define LINK_TO_ZLIB	1
# define LINK_TO_LIBVORBIS 1
#ifdef USEXMP
# define LINK_TO_LIBXMP 1 // nyov: if someone can test with the android NDK compiled libxmp?
#endif
# define DP_MOBILETOUCH	1
# define DP_FREETYPE_STATIC 1
#elif TARGET_OS_IPHONE /* must come first because it also defines MACOSX */
# define DP_OS_NAME		"iPhoneOS"
# define DP_OS_STR		"iphoneos"
# define USE_GLES2		1
# define LINK_TO_ZLIB	1
# define LINK_TO_LIBVORBIS 1
# define DP_MOBILETOUCH	1
# define DP_FREETYPE_STATIC 1
#elif defined(__linux__)
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
#elif defined(__DragonFly__)
# define DP_OS_NAME		"DragonFlyBSD"
# define DP_OS_STR		"dragonflybsd"
#elif defined(MACOSX)
# define DP_OS_NAME		"Mac OS X"
# define DP_OS_STR		"osx"
#elif defined(__MORPHOS__)
# define DP_OS_NAME		"MorphOS"
# define DP_OS_STR		"morphos"
#elif defined (sun) || defined (__sun)
# if defined (__SVR4) || defined (__svr4__)
#  define DP_OS_NAME	"Solaris"
#  define DP_OS_STR		"solaris"
# else
#  define DP_OS_NAME	"SunOS"
#  define DP_OS_STR		"sunos"
# endif
#else
# define DP_OS_NAME		"Unknown"
# define DP_OS_STR		"unknown"
#endif

#if defined(__GNUC__) || (__clang__)
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

#ifdef SSE_POSSIBLE
// runtime detection of SSE/SSE2 capabilities for x86
qbool Sys_HaveSSE(void);
qbool Sys_HaveSSE2(void);
#else
#define Sys_HaveSSE() false
#define Sys_HaveSSE2() false
#endif

#include "glquake.h"

#include "palette.h"
extern cvar_t host_isclient;

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
#define ISCOMMENT(ch, pos) ch[pos] == '/' && ch[pos + 1] == '/' && (pos == 0 || ISWHITESPACE(ch[pos - 1]))
// This also includes extended characters, and ALL control chars
#define ISWHITESPACEORCONTROL(ch) ((signed char) (ch) <= (signed char) ' ')


#ifdef PRVM_64
#define FLOAT_IS_TRUE_FOR_INT(x) ((x) & 0x7FFFFFFFFFFFFFFF) // also match "negative zero" doubles of value 0x8000000000000000
#define FLOAT_LOSSLESS_FORMAT "%.17g"
#define VECTOR_LOSSLESS_FORMAT "%.17g %.17g %.17g"
#else
#define FLOAT_IS_TRUE_FOR_INT(x) ((x) & 0x7FFFFFFF) // also match "negative zero" floats of value 0x80000000
#define FLOAT_LOSSLESS_FORMAT "%.9g"
#define VECTOR_LOSSLESS_FORMAT "%.9g %.9g %.9g"
#endif

// originally this was _MSC_VER
// but here we want to test the system libc, which on win32 is borked, and NOT the compiler
#ifdef WIN32
#define INT_LOSSLESS_FORMAT_SIZE "I64"
#define INT_LOSSLESS_FORMAT_CONVERT_S(x) ((__int64)(x))
#define INT_LOSSLESS_FORMAT_CONVERT_U(x) ((unsigned __int64)(x))
#else
#define INT_LOSSLESS_FORMAT_SIZE "j"
#define INT_LOSSLESS_FORMAT_CONVERT_S(x) ((intmax_t)(x))
#define INT_LOSSLESS_FORMAT_CONVERT_U(x) ((uintmax_t)(x))
#endif

#endif

