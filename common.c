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

cvar_t registered = {0, "registered","0", "indicates if this is running registered quake (whether gfx/qpop.lmp was found)"};
cvar_t cmdline = {0, "cmdline","0", "contains commandline the engine was launched with"};

extern qboolean fs_modified;   // set true if using non-id files

char com_token[MAX_INPUTLINE];
int com_argc;
const char **com_argv;

char com_cmdline[MAX_INPUTLINE];

gamemode_t gamemode;
const char *gamename;
const char *gamedirname1;
const char *gamedirname2;
const char *gamescreenshotname;
const char *gameuserdirname;
char com_modname[MAX_OSPATH] = "";


/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

short   ShortSwap (short l)
{
	unsigned char    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

int    LongSwap (int l)
{
	unsigned char    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

float FloatSwap (float f)
{
	union
	{
		float   f;
		unsigned char    b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}


// Extract integers from buffers

unsigned int BuffBigLong (const unsigned char *buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

unsigned short BuffBigShort (const unsigned char *buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

unsigned int BuffLittleLong (const unsigned char *buffer)
{
	return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

unsigned short BuffLittleShort (const unsigned char *buffer)
{
	return (buffer[1] << 8) | buffer[0];
}


/*
============================================================================

					CRC FUNCTIONS

============================================================================
*/

// this is a 16 bit, non-reflected CRC using the polynomial 0x1021
// and the initial and final xor values shown below...  in other words, the
// CCITT standard CRC used by XMODEM

#define CRC_INIT_VALUE	0xffff
#define CRC_XOR_VALUE	0x0000

static unsigned short crctable[256] =
{
	0x0000,	0x1021,	0x2042,	0x3063,	0x4084,	0x50a5,	0x60c6,	0x70e7,
	0x8108,	0x9129,	0xa14a,	0xb16b,	0xc18c,	0xd1ad,	0xe1ce,	0xf1ef,
	0x1231,	0x0210,	0x3273,	0x2252,	0x52b5,	0x4294,	0x72f7,	0x62d6,
	0x9339,	0x8318,	0xb37b,	0xa35a,	0xd3bd,	0xc39c,	0xf3ff,	0xe3de,
	0x2462,	0x3443,	0x0420,	0x1401,	0x64e6,	0x74c7,	0x44a4,	0x5485,
	0xa56a,	0xb54b,	0x8528,	0x9509,	0xe5ee,	0xf5cf,	0xc5ac,	0xd58d,
	0x3653,	0x2672,	0x1611,	0x0630,	0x76d7,	0x66f6,	0x5695,	0x46b4,
	0xb75b,	0xa77a,	0x9719,	0x8738,	0xf7df,	0xe7fe,	0xd79d,	0xc7bc,
	0x48c4,	0x58e5,	0x6886,	0x78a7,	0x0840,	0x1861,	0x2802,	0x3823,
	0xc9cc,	0xd9ed,	0xe98e,	0xf9af,	0x8948,	0x9969,	0xa90a,	0xb92b,
	0x5af5,	0x4ad4,	0x7ab7,	0x6a96,	0x1a71,	0x0a50,	0x3a33,	0x2a12,
	0xdbfd,	0xcbdc,	0xfbbf,	0xeb9e,	0x9b79,	0x8b58,	0xbb3b,	0xab1a,
	0x6ca6,	0x7c87,	0x4ce4,	0x5cc5,	0x2c22,	0x3c03,	0x0c60,	0x1c41,
	0xedae,	0xfd8f,	0xcdec,	0xddcd,	0xad2a,	0xbd0b,	0x8d68,	0x9d49,
	0x7e97,	0x6eb6,	0x5ed5,	0x4ef4,	0x3e13,	0x2e32,	0x1e51,	0x0e70,
	0xff9f,	0xefbe,	0xdfdd,	0xcffc,	0xbf1b,	0xaf3a,	0x9f59,	0x8f78,
	0x9188,	0x81a9,	0xb1ca,	0xa1eb,	0xd10c,	0xc12d,	0xf14e,	0xe16f,
	0x1080,	0x00a1,	0x30c2,	0x20e3,	0x5004,	0x4025,	0x7046,	0x6067,
	0x83b9,	0x9398,	0xa3fb,	0xb3da,	0xc33d,	0xd31c,	0xe37f,	0xf35e,
	0x02b1,	0x1290,	0x22f3,	0x32d2,	0x4235,	0x5214,	0x6277,	0x7256,
	0xb5ea,	0xa5cb,	0x95a8,	0x8589,	0xf56e,	0xe54f,	0xd52c,	0xc50d,
	0x34e2,	0x24c3,	0x14a0,	0x0481,	0x7466,	0x6447,	0x5424,	0x4405,
	0xa7db,	0xb7fa,	0x8799,	0x97b8,	0xe75f,	0xf77e,	0xc71d,	0xd73c,
	0x26d3,	0x36f2,	0x0691,	0x16b0,	0x6657,	0x7676,	0x4615,	0x5634,
	0xd94c,	0xc96d,	0xf90e,	0xe92f,	0x99c8,	0x89e9,	0xb98a,	0xa9ab,
	0x5844,	0x4865,	0x7806,	0x6827,	0x18c0,	0x08e1,	0x3882,	0x28a3,
	0xcb7d,	0xdb5c,	0xeb3f,	0xfb1e,	0x8bf9,	0x9bd8,	0xabbb,	0xbb9a,
	0x4a75,	0x5a54,	0x6a37,	0x7a16,	0x0af1,	0x1ad0,	0x2ab3,	0x3a92,
	0xfd2e,	0xed0f,	0xdd6c,	0xcd4d,	0xbdaa,	0xad8b,	0x9de8,	0x8dc9,
	0x7c26,	0x6c07,	0x5c64,	0x4c45,	0x3ca2,	0x2c83,	0x1ce0,	0x0cc1,
	0xef1f,	0xff3e,	0xcf5d,	0xdf7c,	0xaf9b,	0xbfba,	0x8fd9,	0x9ff8,
	0x6e17,	0x7e36,	0x4e55,	0x5e74,	0x2e93,	0x3eb2,	0x0ed1,	0x1ef0
};

unsigned short CRC_Block(const unsigned char *data, size_t size)
{
	unsigned short crc = CRC_INIT_VALUE;
	while (size--)
		crc = (crc << 8) ^ crctable[(crc >> 8) ^ (*data++)];
	return crc ^ CRC_XOR_VALUE;
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
	unsigned char    *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	unsigned char    *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	unsigned char    *buf;

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	unsigned char    *buf;

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

	SZ_Write (sb, (unsigned char *)&dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, (unsigned char *)"", 1);
	else
		SZ_Write (sb, (unsigned char *)s, (int)strlen(s)+1);
}

void MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s)
{
	if (s)
		SZ_Write (sb, (unsigned char *)s, (int)strlen(s));
}

void MSG_WriteCoord13i (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteShort (sb, (int)(f * 8.0 + 0.5));
	else
		MSG_WriteShort (sb, (int)(f * 8.0 - 0.5));
}

void MSG_WriteCoord16i (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteShort (sb, (int)(f + 0.5));
	else
		MSG_WriteShort (sb, (int)(f - 0.5));
}

void MSG_WriteCoord32f (sizebuf_t *sb, float f)
{
	MSG_WriteFloat (sb, f);
}

void MSG_WriteCoord (sizebuf_t *sb, float f, protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE)
		MSG_WriteCoord13i (sb, f);
	else if (protocol == PROTOCOL_DARKPLACES1)
		MSG_WriteCoord32f (sb, f);
	else if (protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		MSG_WriteCoord16i (sb, f);
	else
		MSG_WriteCoord32f (sb, f);
}

void MSG_WriteVector (sizebuf_t *sb, float *v, protocolversion_t protocol)
{
	MSG_WriteCoord (sb, v[0], protocol);
	MSG_WriteCoord (sb, v[1], protocol);
	MSG_WriteCoord (sb, v[2], protocol);
}

// LordHavoc: round to nearest value, rather than rounding toward zero, fixes crosshair problem
void MSG_WriteAngle8i (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteByte (sb, (int)(f*(256.0/360.0) + 0.5) & 255);
	else
		MSG_WriteByte (sb, (int)(f*(256.0/360.0) - 0.5) & 255);
}

void MSG_WriteAngle16i (sizebuf_t *sb, float f)
{
	if (f >= 0)
		MSG_WriteShort (sb, (int)(f*(65536.0/360.0) + 0.5) & 65535);
	else
		MSG_WriteShort (sb, (int)(f*(65536.0/360.0) - 0.5) & 65535);
}

void MSG_WriteAngle32f (sizebuf_t *sb, float f)
{
	MSG_WriteFloat (sb, f);
}

void MSG_WriteAngle (sizebuf_t *sb, float f, protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_DARKPLACES1 || protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		MSG_WriteAngle8i (sb, f);
	else
		MSG_WriteAngle16i (sb, f);
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
	static char string[MAX_INPUTLINE];
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

float MSG_ReadCoord13i (void)
{
	return MSG_ReadLittleShort() * (1.0/8.0);
}

float MSG_ReadCoord16i (void)
{
	return (signed short) MSG_ReadLittleShort();
}

float MSG_ReadCoord32f (void)
{
	return MSG_ReadLittleFloat();
}

float MSG_ReadCoord (protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE)
		return MSG_ReadCoord13i();
	else if (protocol == PROTOCOL_DARKPLACES1)
		return MSG_ReadCoord32f();
	else if (protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		return MSG_ReadCoord16i();
	else
		return MSG_ReadCoord32f();
}

void MSG_ReadVector (float *v, protocolversion_t protocol)
{
	v[0] = MSG_ReadCoord(protocol);
	v[1] = MSG_ReadCoord(protocol);
	v[2] = MSG_ReadCoord(protocol);
}

// LordHavoc: round to nearest value, rather than rounding toward zero, fixes crosshair problem
float MSG_ReadAngle8i (void)
{
	return (signed char) MSG_ReadByte () * (360.0/256.0);
}

float MSG_ReadAngle16i (void)
{
	return (signed short)MSG_ReadShort () * (360.0/65536.0);
}

float MSG_ReadAngle32f (void)
{
	return MSG_ReadFloat ();
}

float MSG_ReadAngle (protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_DARKPLACES1 || protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		return MSG_ReadAngle8i ();
	else
		return MSG_ReadAngle16i ();
}


//===========================================================================

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
}

unsigned char *SZ_GetSpace (sizebuf_t *buf, int length)
{
	unsigned char *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Host_Error ("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Host_Error ("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Print("SZ_GetSpace: overflow\n");
		SZ_Clear (buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write (sizebuf_t *buf, const unsigned char *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);
}

// LordHavoc: thanks to Fuh for bringing the pure evil of SZ_Print to my
// attention, it has been eradicated from here, its only (former) use in
// all of darkplaces.

static char *hexchar = "0123456789ABCDEF";
void Com_HexDumpToConsole(const unsigned char *data, int size)
{
	int i, j, n;
	char text[1024];
	char *cur, *flushpointer;
	const unsigned char *d;
	cur = text;
	flushpointer = text + 512;
	for (i = 0;i < size;)
	{
		n = 16;
		if (n > size - i)
			n = size - i;
		d = data + i;
		// print offset
		*cur++ = hexchar[(i >> 12) & 15];
		*cur++ = hexchar[(i >>  8) & 15];
		*cur++ = hexchar[(i >>  4) & 15];
		*cur++ = hexchar[(i >>  0) & 15];
		*cur++ = ':';
		// print hex
		for (j = 0;j < 16;j++)
		{
			if (j < n)
			{
				*cur++ = hexchar[(d[j] >> 4) & 15];
				*cur++ = hexchar[(d[j] >> 0) & 15];
			}
			else
			{
				*cur++ = ' ';
				*cur++ = ' ';
			}
			if ((j & 3) == 3)
				*cur++ = ' ';
		}
		// print text
		for (j = 0;j < 16;j++)
		{
			if (j < n)
			{
				if (d[j] >= ' ' && d[j] <= 127)
					*cur++ = d[j];
				else
					*cur++ = '.';
			}
			else
				*cur++ = ' ';
		}
		*cur++ = '\n';
		i += n;
		if (cur >= flushpointer || i >= size)
		{
			*cur++ = 0;
			Con_Print(text);
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
int COM_ParseToken(const char **datapointer, int returnnewline)
{
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
	// line endings:
	// UNIX: \n
	// Mac: \r
	// Windows: \r\n
	for (;*data <= ' ' && ((*data != '\n' && *data != '\r') || !returnnewline);data++)
	{
		if (*data == 0)
		{
			// end of file
			*datapointer = NULL;
			return false;
		}
	}

	// handle Windows line ending
	if (data[0] == '\r' && data[1] == '\n')
		data++;

	if (data[0] == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (data[0] == '/' && data[1] == '*')
	{
		// comment
		data++;
		while (*data && (data[0] != '*' || data[1] != '/'))
			data++;
		data += 2;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data != '\"';data++)
		{
			if (*data == '\\' && data[1] == '"' )
				data++;
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
		return true;
	}
	else if (*data == '\'')
	{
		// quoted string
		for (data++;*data != '\'';data++)
		{
			if (*data == '\\' && data[1] == '\'' )
				data++;
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
		return true;
	}
	else if (*data == '\r')
	{
		// translate Mac line ending to UNIX
		com_token[len++] = '\n';
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else if (*data == '\n' || *data == '{' || *data == '}' || *data == ')' || *data == '(' || *data == ']' || *data == '[' || *data == '\'' || *data == ':' || *data == ',' || *data == ';')
	{
		// single character
		com_token[len++] = *data++;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else
	{
		// regular word
		for (;*data > ' ' && *data != '{' && *data != '}' && *data != ')' && *data != '(' && *data != ']' && *data != '[' && *data != '\'' && *data != ':' && *data != ',' && *data != ';' && *data != '\'' && *data != '"';data++)
		{
			if (len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}

/*
==============
COM_ParseTokenConsole

Parse a token out of a string, behaving like the qwcl console
==============
*/
int COM_ParseTokenConsole(const char **datapointer)
{
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
	for (;*data <= ' ';data++)
	{
		if (*data == 0)
		{
			// end of file
			*datapointer = NULL;
			return false;
		}
	}

	if (*data == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data != '\"';data++)
		{
			if (!*data || len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data+1;
	}
	else
	{
		// regular word
		for (;*data > ' ';data++)
		{
			if (len >= (int)sizeof(com_token) - 1)
			{
				com_token[0] = 0;
				*datapointer = NULL;
				return false;
			}
			com_token[len++] = *data;
		}
		com_token[len] = 0;
		*datapointer = data;
	}

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

	if (gamemode == GAME_NORMAL && !FS_FileExists("gfx/pop.lmp"))
	{
		if (fs_modified)
			Con_Print("Playing shareware version, with modification.\nwarning: most mods require full quake data.\n");
		else
			Con_Print("Playing shareware version.\n");
		return;
	}

	Cvar_Set ("registered", "1");
	Con_Print("Playing registered version.\n");
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
		if (strstr(com_argv[j], " "))
		{
			// arg contains whitespace, store quotes around it
			com_cmdline[n++] = '\"';
			while ((n < ((int)sizeof(com_cmdline) - 1)) && com_argv[j][i])
				com_cmdline[n++] = com_argv[j][i++];
			com_cmdline[n++] = '\"';
		}
		else
		{
			while ((n < ((int)sizeof(com_cmdline) - 1)) && com_argv[j][i])
				com_cmdline[n++] = com_argv[j][i++];
		}
		if (n < ((int)sizeof(com_cmdline) - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}
	com_cmdline[n] = 0;
}


//===========================================================================

// Game mods

typedef struct gamemode_info_s
{
	const char* prog_name;
	const char* cmdline;
	const char* gamename;
	const char* gamedirname1;
	const char* gamedirname2;
	const char* gamescreenshotname;
	const char* gameuserdirname;
} gamemode_info_t;

static const gamemode_info_t gamemode_info [] =
{// prog_name		cmdline			gamename				gamedirname	gamescreenshotname

// GAME_NORMAL
// COMMANDLINEOPTION: Game: -quake runs the game Quake (default)
{ "",				"-quake",		"DarkPlaces-Quake",		"id1",		NULL,			"dp",			"darkplaces" },
// GAME_HIPNOTIC
// COMMANDLINEOPTION: Game: -hipnotic runs Quake mission pack 1: The Scourge of Armagon
{ "hipnotic",		"-hipnotic",	"Darkplaces-Hipnotic",	"id1",		"hipnotic",		"dp",			"darkplaces" },
// GAME_ROGUE
// COMMANDLINEOPTION: Game: -rogue runs Quake mission pack 2: The Dissolution of Eternity
{ "rogue",			"-rogue",		"Darkplaces-Rogue",		"id1",		"rogue",		"dp",			"darkplaces" },
// GAME_NEHAHRA
// COMMANDLINEOPTION: Game: -nehahra runs The Seal of Nehahra movie and game
{ "nehahra",		"-nehahra",		"DarkPlaces-Nehahra",	"id1",		"nehahra",		"dp",			"darkplaces" },
// GAME_NEXUIZ
// COMMANDLINEOPTION: Game: -nexuiz runs the multiplayer game Nexuiz
{ "nexuiz",			"-nexuiz",		"Nexuiz",				"data",		NULL,			"nexuiz",		"nexuiz" },
// GAME_TRANSFUSION
// COMMANDLINEOPTION: Game: -transfusion runs Transfusion (the recreation of Blood in Quake)
{ "transfusion",	"-transfusion",	"Transfusion",			"basetf",	NULL,			"transfusion",	"transfusion" },
// GAME_GOODVSBAD2
// COMMANDLINEOPTION: Game: -goodvsbad2 runs the psychadelic RTS FPS game Good Vs Bad 2
{ "gvb2",			"-goodvsbad2",	"GoodVs.Bad2",			"rts",		NULL,			"gvb2",			"gvb2" },
// GAME_TEU
// COMMANDLINEOPTION: Game: -teu runs The Evil Unleashed (this option is obsolete as they are not using darkplaces)
{ "teu",			"-teu",			"TheEvilUnleashed",		"baseteu",	NULL,			"teu",			"teu" },
// GAME_BATTLEMECH
// COMMANDLINEOPTION: Game: -battlemech runs the multiplayer topdown deathmatch game BattleMech
{ "battlemech",		"-battlemech",	"Battlemech",			"base",		NULL,			"battlemech",	"battlemech" },
// GAME_ZYMOTIC
// COMMANDLINEOPTION: Game: -zymotic runs the singleplayer game Zymotic
{ "zymotic",		"-zymotic",		"Zymotic",				"basezym",		NULL,			"zymotic",		"zymotic" },
// GAME_FNIGGIUM
// COMMANDLINEOPTION: Game: -fniggium runs the post apocalyptic melee RPG Fniggium
{ "fniggium",		"-fniggium",	"Fniggium",				"data",		NULL,			"fniggium",		"fniggium" },
// GAME_SETHERAL
// COMMANDLINEOPTION: Game: -setheral runs the multiplayer game Setheral
{ "setheral",		"-setheral",	"Setheral",				"data",		NULL,			"setheral",		"setheral" },
// GAME_SOM
// COMMANDLINEOPTION: Game: -som runs the multiplayer game Son Of Man
{ "som",			"-som",			"Son of Man",			"id1",		"sonofman",		"som",			"darkplaces" },
// GAME_TENEBRAE
// COMMANDLINEOPTION: Game: -tenebrae runs the graphics test mod known as Tenebrae (some features not implemented)
{ "tenebrae",		"-tenebrae",	"DarkPlaces-Tenebrae",	"id1",		"tenebrae",		"dp",			"darkplaces" },
// GAME_NEOTERIC
// COMMANDLINEOPTION: Game: -neoteric runs the game Neoteric
{ "neoteric",		"-neoteric",	"Neoteric",				"id1",		"neobase",		"neo",			"darkplaces" },
// GAME_OPENQUARTZ
// COMMANDLINEOPTION: Game: -openquartz runs the game OpenQuartz, a standalone GPL replacement of the quake content
{ "openquartz",		"-openquartz",	"OpenQuartz",			"id1",		NULL,			"openquartz",	"darkplaces" },
// GAME_PRYDON
// COMMANDLINEOPTION: Game: -prydon runs the topdown point and click action-RPG Prydon Gate
{ "prydon",			"-prydon",		"PrydonGate",			"id1",		"prydon",		"prydon",		"darkplaces" },
// GAME_NETHERWORLD
// COMMANDLINEOPTION: Game: -netherworld runs the game Netherworld: Dark Master
{ "netherworld",	"-netherworld",	"Netherworld: Dark Master",	"id1",		"netherworld", 	"nw",			"darkplaces" },
// GAME_THEHUNTED
// COMMANDLINEOPTION: Game: -thehunted runs the game The Hunted
{ "thehunted",		"-thehunted",	"The Hunted",			"thdata",	NULL, 			"th",			"thehunted" },
// GAME_DEFEATINDETAIL2
// COMMANDLINEOPTION: Game: -did2 runs the game Defeat In Detail 2
{ "did2",			"-did2",		"Defeat In Detail 2",	"data",		NULL, 			"did2_",		"did2" },
};

void COM_InitGameType (void)
{
	char name [MAX_OSPATH];
	unsigned int i;

	FS_StripExtension (com_argv[0], name, sizeof (name));
	COM_ToLowerString (name, name, sizeof (name));

	// Check the binary name; default to GAME_NORMAL (0)
	gamemode = GAME_NORMAL;
	for (i = 1; i < sizeof (gamemode_info) / sizeof (gamemode_info[0]); i++)
		if (strstr (name, gamemode_info[i].prog_name))
		{
			gamemode = (gamemode_t)i;
			break;
		}

	// Look for a command-line option
	for (i = 0; i < sizeof (gamemode_info) / sizeof (gamemode_info[0]); i++)
		if (COM_CheckParm (gamemode_info[i].cmdline))
		{
			gamemode = (gamemode_t)i;
			break;
		}

	gamename = gamemode_info[gamemode].gamename;
	gamedirname1 = gamemode_info[gamemode].gamedirname1;
	gamedirname2 = gamemode_info[gamemode].gamedirname2;
	gamescreenshotname = gamemode_info[gamemode].gamescreenshotname;
	gameuserdirname = gamemode_info[gamemode].gameuserdirname;
}


/*
================
COM_Init
================
*/
void COM_Init_Commands (void)
{
	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);
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
	dpvsnprintf (s, sizeof (string[0]), format,argptr);
	va_end (argptr);

	return s;
}


//======================================

// snprintf and vsnprintf are NOT portable. Use their DP counterparts instead

#undef snprintf
#undef vsnprintf

#ifdef WIN32
# define snprintf _snprintf
# define vsnprintf _vsnprintf
#endif


int dpsnprintf (char *buffer, size_t buffersize, const char *format, ...)
{
	va_list args;
	int result;

	va_start (args, format);
	result = dpvsnprintf (buffer, buffersize, format, args);
	va_end (args);

	return result;
}


int dpvsnprintf (char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = vsnprintf (buffer, buffersize, format, args);
	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}


//======================================

void COM_ToLowerString (const char *in, char *out, size_t size_out)
{
	if (size_out == 0)
		return;

	while (*in && size_out > 1)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else
			*out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void COM_ToUpperString (const char *in, char *out, size_t size_out)
{
	if (size_out == 0)
		return;

	while (*in && size_out > 1)
	{
		if (*in >= 'a' && *in <= 'z')
			*out++ = *in++ + 'A' - 'a';
		else
			*out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

int COM_StringBeginsWith(const char *s, const char *match)
{
	for (;*s && *match;s++, match++)
		if (*s != *match)
			return false;
	return true;
}

int COM_ReadAndTokenizeLine(const char **text, char **argv, int maxargc, char *tokenbuf, int tokenbufsize, const char *commentprefix)
{
	int argc, commentprefixlength;
	char *tokenbufend;
	const char *l;
	argc = 0;
	tokenbufend = tokenbuf + tokenbufsize;
	l = *text;
	commentprefixlength = 0;
	if (commentprefix)
		commentprefixlength = (int)strlen(commentprefix);
	while (*l && *l != '\n' && *l != '\r')
	{
		if (*l > ' ')
		{
			if (commentprefixlength && !strncmp(l, commentprefix, commentprefixlength))
			{
				while (*l && *l != '\n' && *l != '\r')
					l++;
				break;
			}
			if (argc >= maxargc)
				return -1;
			argv[argc++] = tokenbuf;
			if (*l == '"')
			{
				l++;
				while (*l && *l != '"')
				{
					if (tokenbuf >= tokenbufend)
						return -1;
					*tokenbuf++ = *l++;
				}
				if (*l == '"')
					l++;
			}
			else
			{
				while (*l > ' ')
				{
					if (tokenbuf >= tokenbufend)
						return -1;
					*tokenbuf++ = *l++;
				}
			}
			if (tokenbuf >= tokenbufend)
				return -1;
			*tokenbuf++ = 0;
		}
		else
			l++;
	}
	// line endings:
	// UNIX: \n
	// Mac: \r
	// Windows: \r\n
	if (*l == '\r')
		l++;
	if (*l == '\n')
		l++;
	*text = l;
	return argc;
}

// written by Elric, thanks Elric!
char *SearchInfostring(const char *infostring, const char *key)
{
	static char value [MAX_INPUTLINE];
	char crt_key [MAX_INPUTLINE];
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
			if (c == '\\' || key_ind == sizeof (crt_key) - 1)
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

				if (c == '\0' || c == '\\' || value_ind == sizeof (value) - 1)
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


//========================================================
// strlcat and strlcpy, from OpenBSD

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*	$OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $	*/
/*	$OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $	*/


#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif  // #ifndef HAVE_STRLCAT


#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

#endif  // #ifndef HAVE_STRLCPY
