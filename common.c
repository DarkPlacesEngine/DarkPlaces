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
#ifndef WIN32
#include <unistd.h>
#endif

#include "quakedef.h"

cvar_t registered = {0, "registered","0"};
cvar_t cmdline = {0, "cmdline","0"};

extern qboolean fs_modified;   // set true if using non-id files

char com_token[1024];
int com_argc;
const char **com_argv;

// LordHavoc: made commandline 1024 characters instead of 256
#define CMDLINE_LENGTH	1024
char com_cmdline[CMDLINE_LENGTH];

int gamemode;
char *gamename;
char *gamedirname;
char com_modname[MAX_OSPATH];


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


// Extract integers from buffers

unsigned int BuffBigLong (const qbyte *buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

unsigned short BuffBigShort (const qbyte *buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

unsigned int BuffLittleLong (const qbyte *buffer)
{
	return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

unsigned short BuffLittleShort (const qbyte *buffer)
{
	return (buffer[1] << 8) | buffer[0];
}


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

void MSG_WriteString (sizebuf_t *sb, const char *s)
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
int msg_readcount;
qboolean msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_ReadLittleShort (void)
{
	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 2;
	return (short)(net_message.data[msg_readcount-2] | (net_message.data[msg_readcount-1]<<8));
}

int MSG_ReadBigShort (void)
{
	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 2;
	return (short)((net_message.data[msg_readcount-2]<<8) + net_message.data[msg_readcount-1]);
}

int MSG_ReadLittleLong (void)
{
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 4;
	return net_message.data[msg_readcount-4] | (net_message.data[msg_readcount-3]<<8) | (net_message.data[msg_readcount-2]<<16) | (net_message.data[msg_readcount-1]<<24);
}

int MSG_ReadBigLong (void)
{
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 4;
	return (net_message.data[msg_readcount-4]<<24) + (net_message.data[msg_readcount-3]<<16) + (net_message.data[msg_readcount-2]<<8) + net_message.data[msg_readcount-1];
}

float MSG_ReadLittleFloat (void)
{
	union
	{
		float f;
		int l;
	} dat;
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 4;
	dat.l = net_message.data[msg_readcount-4] | (net_message.data[msg_readcount-3]<<8) | (net_message.data[msg_readcount-2]<<16) | (net_message.data[msg_readcount-1]<<24);
	return dat.f;
}

float MSG_ReadBigFloat (void)
{
	union
	{
		float f;
		int l;
	} dat;
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	msg_readcount += 4;
	dat.l = (net_message.data[msg_readcount-4]<<24) | (net_message.data[msg_readcount-3]<<16) | (net_message.data[msg_readcount-2]<<8) | net_message.data[msg_readcount-1];
	return dat.f;
}

char *MSG_ReadString (void)
{
	static char string[2048];
	int l,c;
	for (l = 0;l < (int) sizeof(string) - 1 && (c = MSG_ReadChar()) != -1 && c != 0;l++)
		string[l] = c;
	string[l] = 0;
	return string;
}

int MSG_ReadBytes (int numbytes, unsigned char *out)
{
	int l, c;
	for (l = 0;l < numbytes && (c = MSG_ReadChar()) != -1;l++)
		out[l] = c;
	return l;
}

// used by server (always latest dpprotocol)
float MSG_ReadDPCoord (void)
{
	return (signed short) MSG_ReadLittleShort();
}

// used by client
float MSG_ReadCoord (void)
{
	if (dpprotocol == DPPROTOCOL_VERSION2 || dpprotocol == DPPROTOCOL_VERSION3)
		return (signed short) MSG_ReadLittleShort();
	else if (dpprotocol == DPPROTOCOL_VERSION1)
		return MSG_ReadLittleFloat();
	else
		return MSG_ReadLittleShort() * (1.0f/8.0f);
}


//===========================================================================

void SZ_Alloc (sizebuf_t *buf, int startsize, const char *name)
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
	void *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Host_Error ("SZ_GetSpace: overflow without allowoverflow set\n");

		if (length > buf->maxsize)
			Host_Error ("SZ_GetSpace: %i is > full buffer size\n", length);

		buf->overflowed = true;
		Con_Printf ("SZ_GetSpace: overflow\n");
		SZ_Clear (buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t *buf, const void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);
}

void SZ_Print (sizebuf_t *buf, const char *data)
{
	int len;
	len = strlen(data)+1;

// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize-1])
		memcpy ((qbyte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((qbyte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}

static char *hexchar = "0123456789ABCDEF";
void Com_HexDumpToConsole(const qbyte *data, int size)
{
	int i, j, n;
	char text[1024];
	char *cur, *flushpointer;
	const qbyte *d;
	cur = text;
	flushpointer = text + 512;
	for (i = 0;i < size;)
	{
		n = 16;
		if (n > size - i)
			n = size - i;
		d = data + i;
		*cur++ = hexchar[(i >> 12) & 15];
		*cur++ = hexchar[(i >>  8) & 15];
		*cur++ = hexchar[(i >>  4) & 15];
		*cur++ = hexchar[(i >>  0) & 15];
		*cur++ = ':';
		for (j = 0;j < n;j++)
		{
			if (j & 1)
			{
				*cur++ = hexchar[(d[j] >> 4) & 15] | 0x80;
				*cur++ = hexchar[(d[j] >> 0) & 15] | 0x80;
			}
			else
			{
				*cur++ = hexchar[(d[j] >> 4) & 15];
				*cur++ = hexchar[(d[j] >> 0) & 15];
			}
		}
		for (;j < 16;j++)
		{
			*cur++ = ' ';
			*cur++ = ' ';
		}
		for (j = 0;j < n;j++)
			*cur++ = (d[j] >= ' ' && d[j] <= 0x7E) ? d[j] : '.';
		for (;j < 16;j++)
			*cur++ = ' ';
		*cur++ = '\n';
		i += n;
		if (cur >= flushpointer || i >= size)
		{
			*cur++ = 0;
			Con_Printf("%s", text);
			cur = text;
		}
	}
}

void SZ_HexDumpToConsole(const sizebuf_t *buf)
{
	Com_HexDumpToConsole(buf->data, buf->cursize);
}


//============================================================================


/*
==============
COM_ParseToken

Parse a token out of a string
==============
*/
int COM_ParseToken (const char **datapointer)
{
	int c;
	int len;
	const char *data = *datapointer;

	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*datapointer = NULL;
		return false;
	}

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			// end of file
			*datapointer = NULL;
			return false;
		}
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
				*datapointer = data;
				return true;
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
		*datapointer = data+1;
		return true;
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
	*datapointer = data;
	return true;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (const char *parm)
{
	int i;

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

	if (!FS_FileExists("gfx/pop.lmp"))
	{
		if (fs_modified)
			Con_Printf ("Playing shareware version, with modification.\nwarning: most mods require full quake data.\n");
		else
			Con_Printf ("Playing shareware version.\n");
		return;
	}

	Cvar_Set ("registered", "1");
	Con_Printf ("Playing registered version.\n");
}


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
	char name[MAX_OSPATH];
	FS_StripExtension(com_argv[0], name);
	COM_ToLowerString(name, name);

	if (strstr(name, "transfusion"))
		gamemode = GAME_TRANSFUSION;
	else if (strstr(name, "nexuiz"))
		gamemode = GAME_NEXUIZ;
	else if (strstr(name, "nehahra"))
		gamemode = GAME_NEHAHRA;
	else if (strstr(name, "hipnotic"))
		gamemode = GAME_HIPNOTIC;
	else if (strstr(name, "rogue"))
		gamemode = GAME_ROGUE;
	else if (strstr(name, "gvb2"))
		gamemode = GAME_GOODVSBAD2;
	else if (strstr(name, "teu"))
		gamemode = GAME_TEU;
	else
		gamemode = GAME_NORMAL;

	if (COM_CheckParm ("-transfusion"))
		gamemode = GAME_TRANSFUSION;
	else if (COM_CheckParm ("-nexuiz"))
		gamemode = GAME_NEXUIZ;
	else if (COM_CheckParm ("-nehahra"))
		gamemode = GAME_NEHAHRA;
	else if (COM_CheckParm ("-hipnotic"))
		gamemode = GAME_HIPNOTIC;
	else if (COM_CheckParm ("-rogue"))
		gamemode = GAME_ROGUE;
	else if (COM_CheckParm ("-quake"))
		gamemode = GAME_NORMAL;
	else if (COM_CheckParm ("-goodvsbad2"))
		gamemode = GAME_GOODVSBAD2;
	else if (COM_CheckParm ("-teu"))
		gamemode = GAME_TEU;

	switch(gamemode)
	{
	case GAME_NORMAL:
		gamename = "DarkPlaces-Quake";
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
	case GAME_NEXUIZ:
		gamename = "Nexuiz";
		gamedirname = "data";
		break;
	case GAME_TRANSFUSION:
		gamename = "Transfusion";
		gamedirname = "transfusion";
		break;
	case GAME_GOODVSBAD2:
		gamename = "GoodVs.Bad2";
		gamedirname = "rts";
		break;
	case GAME_TEU:
		gamename = "TheEvilUnleashed";
		gamedirname = "teu";
		break;
	default:
		Sys_Error("COM_InitGameType: unknown gamemode %i\n", gamemode);
		break;
	}
}


extern void Mathlib_Init(void);
extern void FS_Init (void);

/*
================
COM_Init
================
*/
void COM_Init (void)
{
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
	qbyte swaptest[2] = {1,0};

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

	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);

	Mathlib_Init();

	FS_Init ();
	Con_InitLogging();
	COM_CheckRegistered ();

	COM_InitGameType();
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va(const char *format, ...)
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


//======================================
// LordHavoc: added these because they are useful

void COM_ToLowerString(const char *in, char *out)
{
	while (*in)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else
			*out++ = *in++;
	}
}

void COM_ToUpperString(const char *in, char *out)
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

// written by Elric, thanks Elric!
char *SearchInfostring(const char *infostring, const char *key)
{
	static char value [256];
	char crt_key [256];
	size_t value_ind, key_ind;
	char c;

	if (*infostring++ != '\\')
		return NULL;

	value_ind = 0;
	for (;;)
	{
		key_ind = 0;

		// Get the key name
		for (;;)
		{
			c = *infostring++;

			if (c == '\0')
				return NULL;
			if (c == '\\')
			{
				crt_key[key_ind] = '\0';
				break;
			}

			crt_key[key_ind++] = c;
		}

		// If it's the key we are looking for, save it in "value"
		if (!strcmp(crt_key, key))
		{
			for (;;)
			{
				c = *infostring++;

				if (c == '\0' || c == '\\')
				{
					value[value_ind] = '\0';
					return value;
				}

				value[value_ind++] = c;
			}
		}

		// Else, skip the value
		for (;;)
		{
			c = *infostring++;

			if (c == '\0')
				return NULL;
			if (c == '\\')
				break;
		}
	}
}

