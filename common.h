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

#ifndef COMMON_H
#define COMMON_H


/// MSVC has a different name for several standard functions
#ifdef WIN32
# define strcasecmp _stricmp
# define strncasecmp _strnicmp
#else
#include "strings.h"
#endif

// Create our own define for Mac OS X
#if defined(__APPLE__) && defined(__MACH__)
# define MACOSX
#endif


//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	///< if false, do a Sys_Error
	qboolean	overflowed;		///< set to true if the buffer size failed
	unsigned char		*data;
	int			maxsize;
	int			cursize;
	int			readcount;
	qboolean	badread;		// set if a read goes beyond end of message
} sizebuf_t;

void SZ_Clear (sizebuf_t *buf);
unsigned char *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const unsigned char *data, int length);
void SZ_HexDumpToConsole(const sizebuf_t *buf);

void Com_HexDumpToConsole(const unsigned char *data, int size);

unsigned short CRC_Block(const unsigned char *data, size_t size);
unsigned short CRC_Block_CaseInsensitive(const unsigned char *data, size_t size); // for hash lookup functions that use strcasecmp for comparison

unsigned char COM_BlockSequenceCRCByteQW(unsigned char *base, int length, int sequence);

// these are actually md4sum (mdfour.c)
unsigned Com_BlockChecksum (void *buffer, int length);
void Com_BlockFullChecksum (void *buffer, int len, unsigned char *outbuf);

void COM_Init_Commands(void);


//============================================================================
//							Endianess handling
//============================================================================

// check mem_bigendian if you need to know the system byte order

/*! \name Byte order functions.
 * @{
 */

// unaligned memory access crashes on some platform, so always read bytes...
#define BigShort(l) BuffBigShort((unsigned char *)&(l))
#define LittleShort(l) BuffLittleShort((unsigned char *)&(l))
#define BigLong(l) BuffBigLong((unsigned char *)&(l))
#define LittleLong(l) BuffLittleLong((unsigned char *)&(l))
#define BigFloat(l) BuffBigFloat((unsigned char *)&(l))
#define LittleFloat(l) BuffLittleFloat((unsigned char *)&(l))

/// Extract a big endian 32bit float from the given \p buffer.
float BuffBigFloat (const unsigned char *buffer);

/// Extract a big endian 32bit int from the given \p buffer.
int BuffBigLong (const unsigned char *buffer);

/// Extract a big endian 16bit short from the given \p buffer.
short BuffBigShort (const unsigned char *buffer);

/// Extract a little endian 32bit float from the given \p buffer.
float BuffLittleFloat (const unsigned char *buffer);

/// Extract a little endian 32bit int from the given \p buffer.
int BuffLittleLong (const unsigned char *buffer);

/// Extract a little endian 16bit short from the given \p buffer.
short BuffLittleShort (const unsigned char *buffer);

/// Encode a big endian 32bit int to the given \p buffer
void StoreBigLong (unsigned char *buffer, unsigned int i);

/// Encode a big endian 16bit int to the given \p buffer
void StoreBigShort (unsigned char *buffer, unsigned short i);

/// Encode a little endian 32bit int to the given \p buffer
void StoreLittleLong (unsigned char *buffer, unsigned int i);

/// Encode a little endian 16bit int to the given \p buffer
void StoreLittleShort (unsigned char *buffer, unsigned short i);
//@}

//============================================================================

// these versions are purely for internal use, never sent in network protocol
// (use Protocol_EnumForNumber and Protocol_NumberToEnum to convert)
typedef enum protocolversion_e
{
	PROTOCOL_UNKNOWN,
	PROTOCOL_DARKPLACES7, ///< added QuakeWorld-style movement protocol to allow more consistent prediction
	PROTOCOL_DARKPLACES6, ///< various changes
	PROTOCOL_DARKPLACES5, ///< uses EntityFrame5 entity snapshot encoder/decoder which is based on a Tribes networking article at http://www.garagegames.com/articles/networking1/
	PROTOCOL_DARKPLACES4, ///< various changes
	PROTOCOL_DARKPLACES3, ///< uses EntityFrame4 entity snapshot encoder/decoder which is broken, this attempted to do partial snapshot updates on a QuakeWorld-like protocol, but it is broken and impossible to fix
	PROTOCOL_DARKPLACES2, ///< various changes
	PROTOCOL_DARKPLACES1, ///< uses EntityFrame entity snapshot encoder/decoder which is a QuakeWorld-like entity snapshot delta compression method
	PROTOCOL_QUAKEDP, ///< darkplaces extended quake protocol (used by TomazQuake and others), backwards compatible as long as no extended features are used
	PROTOCOL_NEHAHRAMOVIE, ///< Nehahra movie protocol, a big nasty hack dating back to early days of the Quake Standards Group (but only ever used by neh_gl.exe), this is potentially backwards compatible with quake protocol as long as no extended features are used (but in actuality the neh_gl.exe which wrote this protocol ALWAYS wrote the extended information)
	PROTOCOL_QUAKE, ///< quake (aka netquake/normalquake/nq) protocol
	PROTOCOL_QUAKEWORLD, ///< quakeworld protocol
	PROTOCOL_NEHAHRABJP, ///< same as QUAKEDP but with 16bit modelindex
	PROTOCOL_NEHAHRABJP2, ///< same as NEHAHRABJP but with 16bit soundindex
	PROTOCOL_NEHAHRABJP3 ///< same as NEHAHRABJP2 but with some changes
}
protocolversion_t;

/*! \name Message IO functions.
 * Handles byte ordering and avoids alignment errors
 * @{
 */

void MSG_InitReadBuffer (sizebuf_t *buf, unsigned char *data, int size);
void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, vec_t f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s);
void MSG_WriteAngle8i (sizebuf_t *sb, vec_t f);
void MSG_WriteAngle16i (sizebuf_t *sb, vec_t f);
void MSG_WriteAngle32f (sizebuf_t *sb, vec_t f);
void MSG_WriteCoord13i (sizebuf_t *sb, vec_t f);
void MSG_WriteCoord16i (sizebuf_t *sb, vec_t f);
void MSG_WriteCoord32f (sizebuf_t *sb, vec_t f);
void MSG_WriteCoord (sizebuf_t *sb, vec_t f, protocolversion_t protocol);
void MSG_WriteVector (sizebuf_t *sb, const vec3_t v, protocolversion_t protocol);
void MSG_WriteAngle (sizebuf_t *sb, vec_t f, protocolversion_t protocol);

void MSG_BeginReading (sizebuf_t *sb);
int MSG_ReadLittleShort (sizebuf_t *sb);
int MSG_ReadBigShort (sizebuf_t *sb);
int MSG_ReadLittleLong (sizebuf_t *sb);
int MSG_ReadBigLong (sizebuf_t *sb);
float MSG_ReadLittleFloat (sizebuf_t *sb);
float MSG_ReadBigFloat (sizebuf_t *sb);
char *MSG_ReadString (sizebuf_t *sb, char *string, size_t maxstring);
int MSG_ReadBytes (sizebuf_t *sb, int numbytes, unsigned char *out);

#define MSG_ReadChar(sb) ((sb)->readcount >= (sb)->cursize ? ((sb)->badread = true, -1) : (signed char)(sb)->data[(sb)->readcount++])
#define MSG_ReadByte(sb) ((sb)->readcount >= (sb)->cursize ? ((sb)->badread = true, -1) : (unsigned char)(sb)->data[(sb)->readcount++])
#define MSG_ReadShort MSG_ReadLittleShort
#define MSG_ReadLong MSG_ReadLittleLong
#define MSG_ReadFloat MSG_ReadLittleFloat

float MSG_ReadAngle8i (sizebuf_t *sb);
float MSG_ReadAngle16i (sizebuf_t *sb);
float MSG_ReadAngle32f (sizebuf_t *sb);
float MSG_ReadCoord13i (sizebuf_t *sb);
float MSG_ReadCoord16i (sizebuf_t *sb);
float MSG_ReadCoord32f (sizebuf_t *sb);
float MSG_ReadCoord (sizebuf_t *sb, protocolversion_t protocol);
void MSG_ReadVector (sizebuf_t *sb, vec3_t v, protocolversion_t protocol);
float MSG_ReadAngle (sizebuf_t *sb, protocolversion_t protocol);
//@}
//============================================================================

typedef float (*COM_WordWidthFunc_t) (void *passthrough, const char *w, size_t *length, float maxWidth); // length is updated to the longest fitting string into maxWidth; if maxWidth < 0, all characters are used and length is used as is
typedef int (*COM_LineProcessorFunc) (void *passthrough, const char *line, size_t length, float width, qboolean isContination);
int COM_Wordwrap(const char *string, size_t length, float continuationSize, float maxWidth, COM_WordWidthFunc_t wordWidth, void *passthroughCW, COM_LineProcessorFunc processLine, void *passthroughPL);

extern char com_token[MAX_INPUTLINE];

int COM_ParseToken_Simple(const char **datapointer, qboolean returnnewline, qboolean parsebackslash, qboolean parsecomments);
int COM_ParseToken_QuakeC(const char **datapointer, qboolean returnnewline);
int COM_ParseToken_VM_Tokenize(const char **datapointer, qboolean returnnewline);
int COM_ParseToken_Console(const char **datapointer);

int COM_CheckParm (const char *parm);
void COM_Init (void);
void COM_Shutdown (void);
void COM_InitGameType (void);

char *va(char *buf, size_t buflen, const char *format, ...) DP_FUNC_PRINTF(3);
// does a varargs printf into provided buffer, returns buffer (so it can be called in-line unlike dpsnprintf)


// snprintf and vsnprintf are NOT portable. Use their DP counterparts instead
#ifdef snprintf
# undef snprintf
#endif
#define snprintf DO_NOT_USE_SNPRINTF__USE_DPSNPRINTF
#ifdef vsnprintf
# undef vsnprintf
#endif
#define vsnprintf DO_NOT_USE_VSNPRINTF__USE_DPVSNPRINTF

// dpsnprintf and dpvsnprintf
// return the number of printed characters, excluding the final '\0'
// or return -1 if the buffer isn't big enough to contain the entire string.
// buffer is ALWAYS null-terminated
extern int dpsnprintf (char *buffer, size_t buffersize, const char *format, ...) DP_FUNC_PRINTF(3);
extern int dpvsnprintf (char *buffer, size_t buffersize, const char *format, va_list args);

// A bunch of functions are forbidden for security reasons (and also to please MSVS 2005, for some of them)
// LadyHavoc: added #undef lines here to avoid warnings in Linux
#undef strcat
#define strcat DO_NOT_USE_STRCAT__USE_STRLCAT_OR_MEMCPY
#undef strncat
#define strncat DO_NOT_USE_STRNCAT__USE_STRLCAT_OR_MEMCPY
#undef strcpy
#define strcpy DO_NOT_USE_STRCPY__USE_STRLCPY_OR_MEMCPY
#undef strncpy
#define strncpy DO_NOT_USE_STRNCPY__USE_STRLCPY_OR_MEMCPY
//#undef sprintf
//#define sprintf DO_NOT_USE_SPRINTF__USE_DPSNPRINTF


//============================================================================

extern	struct cvar_s	registered;
extern	struct cvar_s	cmdline;

typedef enum userdirmode_e
{
	USERDIRMODE_NOHOME, // basedir only
	USERDIRMODE_HOME, // Windows basedir, general POSIX (~/.)
	USERDIRMODE_MYGAMES, // pre-Vista (My Documents/My Games/), general POSIX (~/.)
	USERDIRMODE_SAVEDGAMES, // Vista (%USERPROFILE%/Saved Games/), OSX (~/Library/Application Support/), Linux (~/.config)
	USERDIRMODE_COUNT
}
userdirmode_t;

typedef enum gamemode_e
{
	GAME_NORMAL,
	GAME_HIPNOTIC,
	GAME_ROGUE,
	GAME_QUOTH,
	GAME_NEHAHRA,
	GAME_NEXUIZ,
	GAME_XONOTIC,
	GAME_TRANSFUSION,
	GAME_GOODVSBAD2,
	GAME_TEU,
	GAME_BATTLEMECH,
	GAME_ZYMOTIC,
	GAME_SETHERAL,
	GAME_TENEBRAE, // full of evil hackery
	GAME_NEOTERIC,
	GAME_OPENQUARTZ, //this game sucks
	GAME_PRYDON,
	GAME_DELUXEQUAKE,
	GAME_THEHUNTED,
	GAME_DEFEATINDETAIL2,
	GAME_DARSANA,
	GAME_CONTAGIONTHEORY,
	GAME_EDU2P,
	GAME_PROPHECY,
	GAME_BLOODOMNICIDE,
	GAME_STEELSTORM, // added by motorsep
	GAME_STEELSTORM2, // added by motorsep
	GAME_SSAMMO, // added by motorsep
	GAME_STEELSTORMREVENANTS, // added by motorsep 07/19/2015
	GAME_TOMESOFMEPHISTOPHELES, // added by motorsep
	GAME_STRAPBOMB, // added by motorsep for Urre
	GAME_MOONHELM,
	GAME_VORETOURNAMENT,
	GAME_DOOMBRINGER, // added by Cloudwalk for kristus
	GAME_COUNT
}
gamemode_t;

// Master switch for some hacks/changes that eventually should become cvars.
#define IS_NEXUIZ_DERIVED(g) ((g) == GAME_NEXUIZ || (g) == GAME_XONOTIC || (g) == GAME_VORETOURNAMENT)
// Pre-csqcmodels era.
#define IS_OLDNEXUIZ_DERIVED(g) ((g) == GAME_NEXUIZ || (g) == GAME_VORETOURNAMENT)

extern gamemode_t gamemode;
extern const char *gamename;
extern const char *gamenetworkfiltername;
extern const char *gamedirname1;
extern const char *gamedirname2;
extern const char *gamescreenshotname;
extern const char *gameuserdirname;
extern char com_modname[MAX_OSPATH];

void COM_ChangeGameTypeForGameDirs(void);

void COM_ToLowerString (const char *in, char *out, size_t size_out);
void COM_ToUpperString (const char *in, char *out, size_t size_out);
int COM_StringBeginsWith(const char *s, const char *match);

int COM_ReadAndTokenizeLine(const char **text, char **argv, int maxargc, char *tokenbuf, int tokenbufsize, const char *commentprefix);

size_t COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid);
qboolean COM_StringDecolorize(const char *in, size_t size_in, char *out, size_t size_out, qboolean escape_carets);
void COM_ToLowerString (const char *in, char *out, size_t size_out);
void COM_ToUpperString (const char *in, char *out, size_t size_out);

typedef struct stringlist_s
{
	/// maxstrings changes as needed, causing reallocation of strings[] array
	int maxstrings;
	int numstrings;
	char **strings;
} stringlist_t;

int matchpattern(const char *in, const char *pattern, int caseinsensitive);
int matchpattern_with_separator(const char *in, const char *pattern, int caseinsensitive, const char *separators, qboolean wildcard_least_one);
void stringlistinit(stringlist_t *list);
void stringlistfreecontents(stringlist_t *list);
void stringlistappend(stringlist_t *list, const char *text);
void stringlistsort(stringlist_t *list, qboolean uniq);
void listdirectory(stringlist_t *list, const char *basepath, const char *path);

char *InfoString_GetValue(const char *buffer, const char *key, char *value, size_t valuelength);
void InfoString_SetValue(char *buffer, size_t bufferlength, const char *key, const char *value);
void InfoString_Print(char *buffer);

// strlcat and strlcpy, from OpenBSD
// Most (all?) BSDs already have them
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(MACOSX)
# define HAVE_STRLCAT 1
# define HAVE_STRLCPY 1
#endif

#ifndef HAVE_STRLCAT
/*!
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t strlcat(char *dst, const char *src, size_t siz);
#endif  // #ifndef HAVE_STRLCAT

#ifndef HAVE_STRLCPY
/*!
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz);

#endif  // #ifndef HAVE_STRLCPY

void FindFraction(double val, int *num, int *denom, int denomMax);

// decodes XPM file to XPM array (as if #include'd)
char **XPM_DecodeString(const char *in);

size_t base64_encode(unsigned char *buf, size_t buflen, size_t outbuflen);

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#endif

