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
// comndef.h  -- general definitions

#if !defined BYTE_DEFINED
typedef unsigned char 		byte;
#define BYTE_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true}	qboolean;

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

//============================================================================
#ifdef WIN32
short   ShortSwap (short l);
int    LongSwap (int l);
float FloatSwap (float f);
#define BigShort(l) ShortSwap(l)
#define LittleShort(l) (l)
#define BigLong(l) LongSwap(l)
#define LittleLong(l) (l)
#define BigFloat(l) FloatSwap(l)
#define LittleFloat(l) (l)
#else
extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);
#endif

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading (void);
//int MSG_ReadChar (void);
//int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

#define MSG_ReadChar() (msg_readcount >= net_message.cursize ? (msg_badread = true, -1) : (signed char)net_message.data[msg_readcount++])
#define MSG_ReadByte() (msg_readcount >= net_message.cursize ? (msg_badread = true, -1) : (unsigned char)net_message.data[msg_readcount++])
//#define MSG_ReadShort() ((msg_readcount + 2) > net_message.cursize ? (msg_badread = true, -1) : (short)net_message.data[msg_readcount+=2, msg_readcount-2] | (net_message.data[msg_readcount-1] << 8))
//#define MSG_ReadLong() ((msg_readcount + 4) > net_message.cursize ? (msg_badread = true, -1) : (int)net_message.data[msg_readcount+=4, msg_readcount-4] | (net_message.data[msg_readcount-3] << 8) | (net_message.data[msg_readcount-2] << 16) | (net_message.data[msg_readcount-1] << 24))

//float MSG_ReadCoord (void);
//float MSG_ReadAngle (void);

#define MSG_ReadAngle() (MSG_ReadByte() * (360.0f / 256.0f))
#define MSG_ReadCoord() (MSG_ReadShort() * 0.125f)

//============================================================================

/*
void Q_memset (void *dest, int fill, int count);
void Q_memcpy (void *dest, void *src, int count);
int Q_memcmp (void *m1, void *m2, int count);
void Q_strcpy (char *dest, char *src);
void Q_strncpy (char *dest, char *src, int count);
int Q_strlen (char *str);
char *Q_strrchr (char *s, char c);
void Q_strcat (char *dest, char *src);
int Q_strcmp (char *s1, char *s2);
int Q_strncmp (char *s1, char *s2, int count);
*/
int Q_strcasecmp (char *s1, char *s2);
int Q_strncasecmp (char *s1, char *s2, int n);
/*
int	Q_atoi (char *str);
float Q_atof (char *str);
*/

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);


extern	int		com_argc;
extern	char	**com_argv;

int COM_CheckParm (char *parm);
void COM_Init (char *path);
void COM_InitArgv (int argc, char **argv);

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer


//============================================================================

extern int com_filesize;
struct cache_user_s;

extern	char	com_gamedir[MAX_OSPATH];

void COM_WriteFile (char *filename, void *data, int len);
int COM_OpenFile (char *filename, int *hndl, qboolean quiet);
int COM_FOpenFile (char *filename, FILE **file, qboolean quiet);
void COM_CloseFile (int h);

byte *COM_LoadStackFile (char *path, void *buffer, int bufsize, qboolean quiet);
byte *COM_LoadTempFile (char *path, qboolean quiet);
byte *COM_LoadHunkFile (char *path, qboolean quiet);
byte *COM_LoadMallocFile (char *path, qboolean quiet);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu, qboolean quiet);

byte *COM_LoadFile (char *path, int usehunk, qboolean quiet);

int COM_FileExists(char *filename);

extern	struct cvar_s	registered;

extern qboolean		standard_quake, rogue, hipnotic, nehahra;
