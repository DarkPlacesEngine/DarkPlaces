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
// common.c -- misc functions used in client and server

#include <stdlib.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "quakedef.h"

cvar_t registered = {0, "registered","0"};
cvar_t cmdline = {0, "cmdline","0"};

mempool_t *pak_mempool;

qboolean com_modified;   // set true if using non-id files

qboolean msg_suppress_1 = 0;

void COM_InitFilesystem (void);

char com_token[1024];
char com_basedir[MAX_OSPATH];
int com_argc;
char **com_argv;

// LordHavoc: made commandline 1024 characters instead of 256
#define CMDLINE_LENGTH	1024
char com_cmdline[CMDLINE_LENGTH];

int gamemode;
char *gamename;
char *gamedirname;

/*


All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.



FIXME:
The file "parms.txt" will be read out of the game directory and appended to the current command line arguments to allow different games to initialize startup parms differently.  This could be used to add a "-sspeed 22050" for the high quality sound edition.  Because they are added at the end, they will not override an explicit setting on the original command line.

*/

//============================================================================


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int Q_strncasecmp (char *s1, char *s2, int n)
{
	int             c1, c2;

	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;               // strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;              // strings not equal
		}
		if (!c1)
			return 0;               // strings are equal
	}

	return -1;
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int     (*BigLong) (int l);
int     (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);
#endif

short   ShortSwap (short l)
{
	qbyte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
short   ShortNoSwap (short l)
{
	return l;
}
#endif

int    LongSwap (int l)
{
	qbyte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
int     LongNoSwap (int l)
{
	return l;
}
#endif

float FloatSwap (float f)
{
	union
	{
		float   f;
		qbyte    b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
float FloatNoSwap (float f)
{
	return f;
}
#endif

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	qbyte    *buf;
	
	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	qbyte    *buf;
	
	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	qbyte    *buf;

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	qbyte    *buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float   f;
		int     l;
	} dat;


	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

// used by server (always latest dpprotocol)
void MSG_WriteDPCoord (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteShort (sb, (int)(f + 0.5f));
	else
		MSG_WriteShort (sb, (int)(f - 0.5f));
}

void MSG_WritePreciseAngle (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteShort (sb, (int)(f*(65536.0f/360.0f) + 0.5f) & 65535);
	else
		MSG_WriteShort (sb, (int)(f*(65536.0f/360.0f) - 0.5f) & 65535);
}

// LordHavoc: round to nearest value, rather than rounding toward zero, fixes crosshair problem
void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteByte (sb, (int)(f*(256.0f/360.0f) + 0.5f) & 255);
	else
		MSG_WriteByte (sb, (int)(f*(256.0f/360.0f) - 0.5f) & 255);
}

//
// reading functions
//
int                     msg_readcount;
qboolean        msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_ReadShort (void)
{
	int     c;

	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void)
{
	int     c;

	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8)
	+ (net_message.data[msg_readcount+2]<<16)
	+ (net_message.data[msg_readcount+3]<<24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		qbyte    b[4];
		float   f;
		int     l;
	} dat;

	dat.b[0] =      net_message.data[msg_readcount];
	dat.b[1] =      net_message.data[msg_readcount+1];
	dat.b[2] =      net_message.data[msg_readcount+2];
	dat.b[3] =      net_message.data[msg_readcount+3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (void)
{
	static char     string[2048];
	int             l,c;

	l = 0;
	do
	{
		c = MSG_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

// used by server (always latest dpprotocol)
float MSG_ReadDPCoord (void)
{
	return (signed short) MSG_ReadShort();
}

// used by client
float MSG_ReadCoord (void)
{
	if (dpprotocol == DPPROTOCOL_VERSION2 || dpprotocol == DPPROTOCOL_VERSION3)
		return (signed short) MSG_ReadShort();
	else if (dpprotocol == DPPROTOCOL_VERSION1)
		return MSG_ReadFloat();
	else
		return MSG_ReadShort() * (1.0f/8.0f);
}


//===========================================================================

void SZ_Alloc (sizebuf_t *buf, int startsize, char *name)
{
	if (startsize < 256)
		startsize = 256;
	buf->mempool = Mem_AllocPool(name);
	buf->data = Mem_Alloc(buf->mempool, startsize);
	buf->maxsize = startsize;
	buf->cursize = 0;
}


void SZ_Free (sizebuf_t *buf)
{
	Mem_FreePool(&buf->mempool);
	buf->data = NULL;
	buf->maxsize = 0;
	buf->cursize = 0;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void    *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Host_Error ("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Host_Error ("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf ("SZ_GetSpace: overflow");
		SZ_Clear (buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);         
}

void SZ_Print (sizebuf_t *buf, char *data)
{
	int             len;
	
	len = strlen(data)+1;

// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize-1])
		memcpy ((qbyte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((qbyte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}


//============================================================================


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char    *last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
// LordHavoc: replacement for severely broken COM_StripExtension that was used in original quake.
void COM_StripExtension (char *in, char *out)
{
	char *last = NULL;
	while (*in)
	{
		if (*in == '.')
			last = out;
		else if (*in == '/' || *in == '\\' || *in == ':')
			last = NULL;
		*out++ = *in++;
	}
	if (last)
		*last = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int             i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *slash, *dot;
	char *s;

	slash = in;
	dot = NULL;
	s = in;
	while(*s)
	{
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;
	if (dot - slash < 2)
		strcpy (out,"?model?");
	else
	{
		while (slash < dot)
			*out++ = *slash++;
		*out++ = 0;
	}
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (char *parm)
{
	int             i;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,com_argv[i]))
			return i;
	}
		
	return 0;
}

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
void COM_CheckRegistered (void)
{
	Cvar_Set ("cmdline", com_cmdline);

	if (!Sys_FileTime("gfx/pop.lmp"))
	{
		if (com_modified)
			Con_Printf ("Playing shareware version, with modification.\nwarning: most mods require full quake data.\n");
		else
			Con_Printf ("Playing shareware version.\n");
		return;
	}

	Cvar_Set ("registered", "1");
	Con_Printf ("Playing registered version.\n");
}


void COM_Path_f (void);


/*
================
COM_InitArgv
================
*/
void COM_InitArgv (void)
{
	int i, j, n;
	// reconstitute the command line for the cmdline externally visible cvar
	n = 0;
	for (j = 0;(j < MAX_NUM_ARGVS) && (j < com_argc);j++)
	{
		i = 0;
		while ((n < (CMDLINE_LENGTH - 1)) && com_argv[j][i])
			com_cmdline[n++] = com_argv[j][i++];
		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}
	com_cmdline[n] = 0;
}

void COM_InitGameType (void)
{
	char name[128];
	COM_StripExtension(com_argv[0], name);
	COM_ToLowerString(name, name);

	if (strstr(name, "transfusion"))
		gamemode = GAME_TRANSFUSION;
	else if (strstr(name, "zymotic"))
		gamemode = GAME_ZYMOTIC;
	else if (strstr(name, "fiendarena"))
		gamemode = GAME_FIENDARENA;
	else if (strstr(name, "nehahra"))
		gamemode = GAME_NEHAHRA;
	else if (strstr(name, "hipnotic"))
		gamemode = GAME_HIPNOTIC;
	else if (strstr(name, "rogue"))
		gamemode = GAME_ROGUE;
	else
		gamemode = GAME_NORMAL;

	if (COM_CheckParm ("-transfusion"))
		gamemode = GAME_TRANSFUSION;
	else if (COM_CheckParm ("-zymotic"))
		gamemode = GAME_ZYMOTIC;
	else if (COM_CheckParm ("-fiendarena"))
		gamemode = GAME_FIENDARENA;
	else if (COM_CheckParm ("-nehahra"))
		gamemode = GAME_NEHAHRA;
	else if (COM_CheckParm ("-hipnotic"))
		gamemode = GAME_HIPNOTIC;
	else if (COM_CheckParm ("-rogue"))
		gamemode = GAME_ROGUE;
	else if (COM_CheckParm ("-quake"))
		gamemode = GAME_NORMAL;

	switch(gamemode)
	{
	case GAME_NORMAL:
		if (registered.integer)
			gamename = "DarkPlaces-Quake";
		else
			gamename = "DarkPlaces-SharewareQuake";
		gamedirname = "";
		break;
	case GAME_HIPNOTIC:
		gamename = "Darkplaces-Hipnotic";
		gamedirname = "hipnotic";
		break;
	case GAME_ROGUE:
		gamename = "Darkplaces-Rogue";
		gamedirname = "rogue";
		break;
	case GAME_NEHAHRA:
		gamename = "DarkPlaces-Nehahra";
		gamedirname = "nehahra";
		break;
	case GAME_FIENDARENA:
		gamename = "FiendArena";
		gamedirname = "fiendarena";
		break;
	case GAME_ZYMOTIC:
		gamename = "Zymotic";
		gamedirname = "zymotic";
		break;
	case GAME_TRANSFUSION:
		gamename = "Transfusion";
		gamedirname = "transfusion";
		break;
	default:
		Sys_Error("COM_InitGameType: unknown gamemode %i\n", gamemode);
		break;
	}
}


extern void Mathlib_Init(void);

/*
================
COM_Init
================
*/
void COM_Init (void)
{
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	qbyte    swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner
	if ( *(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
#endif

	pak_mempool = Mem_AllocPool("paks");

	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);
	Cmd_AddCommand ("path", COM_Path_f);

	Mathlib_Init();

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char    *va(char *format, ...)
{
	va_list argptr;
	// LordHavoc: now cycles through 8 buffers to avoid problems in most cases
	static char string[8][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 7;
	va_start (argptr, format);
	vsprintf (s, format,argptr);
	va_end (argptr);

	return s;
}


/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int     com_filesize;


//
// in memory
//

typedef struct
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char filename[MAX_OSPATH];
	int handle;
	int numfiles;
	packfile_t *files;
	mempool_t *mempool;
	struct pack_s *next;
} pack_t;

//
// on disk
//
typedef struct
{
	char name[56];
	int filepos, filelen;
} dpackfile_t;

typedef struct
{
	char id[4];
	int dirofs;
	int dirlen;
} dpackheader_t;

// LordHavoc: was 2048, increased to 65536 and changed info[MAX_PACK_FILES] to a temporary alloc
#define MAX_FILES_IN_PACK       65536

pack_t	*packlist = NULL;

#if CACHEENABLE
char	com_cachedir[MAX_OSPATH];
#endif
char	com_gamedir[MAX_OSPATH];

typedef struct searchpath_s
{
	char filename[MAX_OSPATH];
	pack_t *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t    *com_searchpaths;

/*
============
COM_Path_f

============
*/
void COM_Path_f (void)
{
	searchpath_t    *s;

	Con_Printf ("Current search path:\n");
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
		{
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf ("%s\n", s->filename);
	}
}

/*
============
COM_CreatePath

LordHavoc: Previously only used for CopyFile, now also used for COM_WriteFile.
============
*/
void    COM_CreatePath (char *path)
{
	char    *ofs, save;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = save;
		}
	}
}


/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
qboolean COM_WriteFile (char *filename, void *data, int len)
{
	int             handle;
	char    name[MAX_OSPATH];

	sprintf (name, "%s/%s", com_gamedir, filename);

	// LordHavoc: added this
	COM_CreatePath (name); // create directories up to the file

	handle = Sys_FileOpenWrite (name);
	if (handle == -1)
	{
		Con_Printf ("COM_WriteFile: failed on %s\n", name);
		return false;
	}

	Con_DPrintf ("COM_WriteFile: %s\n", name);
	Sys_FileWrite (handle, data, len);
	Sys_FileClose (handle);
	return true;
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	int             in, out;
	int             remaining, count;
	char    buf[4096];

	remaining = Sys_FileOpenRead (netpath, &in);            
	COM_CreatePath (cachepath);     // create directories up to the cache file
	out = Sys_FileOpenWrite (cachepath);
	
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		Sys_FileRead (in, buf, count);
		Sys_FileWrite (out, buf, count);
		remaining -= count;
	}

	Sys_FileClose (in);
	Sys_FileClose (out);    
}

/*
===========
COM_OpenRead
===========
*/
QFile * COM_OpenRead (const char *path, int offs, int len, qboolean zip)
{
	int				fd = open (path, O_RDONLY);
	unsigned char	id[2];
	unsigned char	len_bytes[4];

	if (fd == -1)
	{
		Sys_Error ("Couldn't open %s", path);
		return 0;
	}
	if (offs < 0 || len < 0)
	{
		// normal file
		offs = 0;
		len = lseek (fd, 0, SEEK_END);
		lseek (fd, 0, SEEK_SET);
	}
	lseek (fd, offs, SEEK_SET);
	if (zip)
	{
		read (fd, id, 2);
		if (id[0] == 0x1f && id[1] == 0x8b)
		{
			lseek (fd, offs + len - 4, SEEK_SET);
			read (fd, len_bytes, 4);
			len = ((len_bytes[3] << 24)
				   | (len_bytes[2] << 16)
				   | (len_bytes[1] << 8)
				   | (len_bytes[0]));
		}
	}
	lseek (fd, offs, SEEK_SET);
	com_filesize = len;

#ifdef WIN32
	setmode (fd, O_BINARY);
#endif
	if (zip)
		return Qdopen (fd, "rbz");
	else
		return Qdopen (fd, "rb");
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
int COM_FindFile (char *filename, QFile **file, qboolean quiet, qboolean zip)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
#if CACHEENABLE
	char			cachepath[MAX_OSPATH];
	int				cachetime;
#endif
	pack_t			*pak;
	int				i;
	int				findtime;
	char			gzfilename[MAX_OSPATH];
	int				filenamelen;

	filenamelen = strlen (filename);
	sprintf (gzfilename, "%s.gz", filename);

	if (!file)
		Sys_Error ("COM_FindFile: file not set");
		
//
// search through the path, one element at a time
//
	search = com_searchpaths;

	for ( ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
				if (!strcmp (pak->files[i].name, filename)
				    || !strcmp (pak->files[i].name, gzfilename))
				{       // found it!
					if (!quiet)
						Sys_Printf ("PackFile: %s : %s\n",pak->filename, pak->files[i].name);
					// open a new file on the pakfile
					*file = COM_OpenRead (pak->filename, pak->files[i].filepos, pak->files[i].filelen, zip);
					return com_filesize;
				}
		}
		else
		{               
			sprintf (netpath, "%s/%s",search->filename, filename);
			
			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;
				
#if CACHEENABLE
			// see if the file needs to be updated in the cache
			if (com_cachedir[0])
			{	
#if defined(_WIN32)
				if ((strlen(netpath) < 2) || (netpath[1] != ':'))
					sprintf (cachepath,"%s%s", com_cachedir, netpath);
				else
					sprintf (cachepath,"%s%s", com_cachedir, netpath+2);
#else
				sprintf (cachepath,"%s%s", com_cachedir, netpath);
#endif

				cachetime = Sys_FileTime (cachepath);

				if (cachetime < findtime)
					COM_CopyFile (netpath, cachepath);
				strcpy (netpath, cachepath);
			}	
#endif

			if (!quiet)
				Sys_Printf ("FindFile: %s\n",netpath);
			*file = COM_OpenRead (netpath, -1, -1, zip);
			return com_filesize;
		}
		
	}
	
	if (!quiet)
		Sys_Printf ("FindFile: can't find %s\n", filename);
	
	*file = NULL;
	com_filesize = -1;
	return -1;
}


/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new QFile * will be opened
into the file.
===========
*/
int COM_FOpenFile (char *filename, QFile **file, qboolean quiet, qboolean zip)
{
	return COM_FindFile (filename, file, quiet, zip);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 byte.
============
*/
qbyte *loadbuf;
int loadsize;
qbyte *COM_LoadFile (char *path, qboolean quiet)
{
	QFile *h;
	qbyte *buf;
	char base[1024];
	int len;

	buf = NULL;     // quiet compiler warning
	loadsize = 0;

// look for it in the filesystem or pack files
	len = COM_FOpenFile (path, &h, quiet, true);
	if (!h)
		return NULL;

	loadsize = len;

// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	buf = Mem_Alloc(tempmempool, len+1);
	if (!buf)
		Sys_Error ("COM_LoadFile: not enough available memory for %s (size %i)", path, len);

	((qbyte *)buf)[len] = 0;

	Qread (h, buf, len);
	Qclose (h);

	return buf;
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *COM_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	int				numpackfiles;
	pack_t			*pack;
	int				packhandle;
	// LordHavoc: changed from stack array to temporary alloc, allowing huge pack directories
	dpackfile_t		*info;

	if (Sys_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	Sys_FileRead (packhandle, (void *)&header, sizeof(header));
	if (memcmp(header.id, "PACK", 4))
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen % sizeof(dpackfile_t))
		Sys_Error ("%s has an invalid directory size", packfile);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	pack = Mem_Alloc(pak_mempool, sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->mempool = Mem_AllocPool(packfile);
	pack->files = Mem_Alloc(pack->mempool, numpackfiles * sizeof(packfile_t));
	pack->next = packlist;
	packlist = pack;

	info = Mem_Alloc(tempmempool, sizeof(*info) * numpackfiles);
	Sys_FileSeek (packhandle, header.dirofs);
	Sys_FileRead (packhandle, (void *)info, header.dirlen);

// parse the directory
	for (i = 0;i < numpackfiles;i++)
	{
		strcpy (pack->files[i].name, info[i].name);
		pack->files[i].filepos = LittleLong(info[i].filepos);
		pack->files[i].filelen = LittleLong(info[i].filelen);
	}

	Mem_Free(info);

	Con_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void COM_AddGameDirectory (char *dir)
{
	stringlist_t *list, *current;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	strcpy (com_gamedir, dir);

//
// add the directory to the search path
//
	search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any paks in the directory
	list = listdirectory(dir);
	for (current = list;current;current = current->next)
	{
		if (matchpattern(current->text, "*.pak"))
		{
			sprintf (pakfile, "%s/%s", dir, current->text);
			pak = COM_LoadPackFile (pakfile);
			if (pak)
			{
				search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
				search->pack = pak;
				search->next = com_searchpaths;
				com_searchpaths = search;
			}
			else
				Con_Printf("unable to load pak \"%s\"\n", pakfile);
		}
	}
	freedirectory(list);
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void)
{
	int i;
	searchpath_t *search;

	strcpy(com_basedir, ".");

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_basedir, com_argv[i+1]);

	i = strlen (com_basedir);
	if (i > 0 && (com_basedir[i-1] == '\\' || com_basedir[i-1] == '/'))
		com_basedir[i-1] = 0;

// start up with GAMENAME by default (id1)
	COM_AddGameDirectory (va("%s/"GAMENAME, com_basedir));
	if (gamedirname[0])
	{
		com_modified = true;
		COM_AddGameDirectory (va("%s/%s", com_basedir, gamedirname));
	}

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		com_modified = true;
		COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]));
	}

	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact search path, overriding the generated one
	i = COM_CheckParm ("-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;

			search = Mem_Alloc(pak_mempool, sizeof(searchpath_t));
			if ( !strcmp(COM_FileExtension(com_argv[i]), "pak") )
			{
				search->pack = COM_LoadPackFile (com_argv[i]);
				if (!search->pack)
					Sys_Error ("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strcpy (search->filename, com_argv[i]);
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}
}

int COM_FileExists(char *filename)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	int				i;
	int				findtime;

	for (search = com_searchpaths;search;search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			for (i = 0;i < pak->numfiles;i++)
				if (!strcmp (pak->files[i].name, filename))
					return true;
		}
		else
		{
			sprintf (netpath, "%s/%s",search->filename, filename);
			findtime = Sys_FileTime (netpath);
			if (findtime != -1)
				return true;
		}		
	}

	return false;
}


//======================================
// LordHavoc: added these because they are useful

void COM_ToLowerString(char *in, char *out)
{
	while (*in)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else
			*out++ = *in++;
	}
}

void COM_ToUpperString(char *in, char *out)
{
	while (*in)
	{
		if (*in >= 'a' && *in <= 'z')
			*out++ = *in++ + 'A' - 'a';
		else
			*out++ = *in++;
	}
}

int COM_StringBeginsWith(const char *s, const char *match)
{
	for (;*s && *match;s++, match++)
		if (*s != *match)
			return false;
	return true;
}

