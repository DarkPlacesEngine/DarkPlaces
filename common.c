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
#include "utf8lib.h"

cvar_t registered = {0, "registered","0", "indicates if this is running registered quake (whether gfx/pop.lmp was found)"};
cvar_t cmdline = {0, "cmdline","0", "contains commandline the engine was launched with"};

char com_token[MAX_INPUTLINE];
int com_argc;
const char **com_argv;
int com_selffd = -1;

gamemode_t gamemode;
const char *gamename;
const char *gamenetworkfiltername; // same as gamename currently but with _ in place of spaces so that "getservers" packets parse correctly (this also means the 
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


float BuffBigFloat (const unsigned char *buffer)
{
	union
	{
		float f;
		unsigned int i;
	}
	u;
	u.i = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	return u.f;
}

int BuffBigLong (const unsigned char *buffer)
{
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

short BuffBigShort (const unsigned char *buffer)
{
	return (buffer[0] << 8) | buffer[1];
}

float BuffLittleFloat (const unsigned char *buffer)
{
	union
	{
		float f;
		unsigned int i;
	}
	u;
	u.i = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
	return u.f;
}

int BuffLittleLong (const unsigned char *buffer)
{
	return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

short BuffLittleShort (const unsigned char *buffer)
{
	return (buffer[1] << 8) | buffer[0];
}

void StoreBigLong (unsigned char *buffer, unsigned int i)
{
	buffer[0] = (i >> 24) & 0xFF;
	buffer[1] = (i >> 16) & 0xFF;
	buffer[2] = (i >>  8) & 0xFF;
	buffer[3] = i         & 0xFF;
}

void StoreBigShort (unsigned char *buffer, unsigned short i)
{
	buffer[0] = (i >>  8) & 0xFF;
	buffer[1] = i         & 0xFF;
}

void StoreLittleLong (unsigned char *buffer, unsigned int i)
{
	buffer[0] = i         & 0xFF;
	buffer[1] = (i >>  8) & 0xFF;
	buffer[2] = (i >> 16) & 0xFF;
	buffer[3] = (i >> 24) & 0xFF;
}

void StoreLittleShort (unsigned char *buffer, unsigned short i)
{
	buffer[0] = i         & 0xFF;
	buffer[1] = (i >>  8) & 0xFF;
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

unsigned short CRC_Block_CaseInsensitive(const unsigned char *data, size_t size)
{
	unsigned short crc = CRC_INIT_VALUE;
	while (size--)
		crc = (crc << 8) ^ crctable[(crc >> 8) ^ (tolower(*data++))];
	return crc ^ CRC_XOR_VALUE;
}

// QuakeWorld
static unsigned char chktbl[1024 + 4] =
{
	0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
	0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
	0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
	0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
	0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
	0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
	0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
	0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
	0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
	0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
	0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
	0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
	0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
	0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
	0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
	0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
	0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
	0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
	0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
	0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
	0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
	0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
	0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
	0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
	0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
	0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
	0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
	0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
	0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
	0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
	0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
	0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

	// map checksum goes here
	0x00,0x00,0x00,0x00
};

// QuakeWorld
unsigned char COM_BlockSequenceCRCByteQW(unsigned char *base, int length, int sequence)
{
	unsigned char *p;
	unsigned char chkb[60 + 4];

	p = chktbl + (sequence % (sizeof(chktbl) - 8));

	if (length > 60)
		length = 60;
	memcpy(chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	return CRC_Block(chkb, length + 4) & 0xff;
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
	if (!s || !*s)
		MSG_WriteChar (sb, 0);
	else
		SZ_Write (sb, (unsigned char *)s, (int)strlen(s)+1);
}

void MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s)
{
	if (s && *s)
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
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_NEHAHRABJP || protocol == PROTOCOL_NEHAHRABJP2 || protocol == PROTOCOL_NEHAHRABJP3 || protocol == PROTOCOL_QUAKEWORLD)
		MSG_WriteCoord13i (sb, f);
	else if (protocol == PROTOCOL_DARKPLACES1)
		MSG_WriteCoord32f (sb, f);
	else if (protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		MSG_WriteCoord16i (sb, f);
	else
		MSG_WriteCoord32f (sb, f);
}

void MSG_WriteVector (sizebuf_t *sb, const vec3_t v, protocolversion_t protocol)
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
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_NEHAHRABJP || protocol == PROTOCOL_NEHAHRABJP2 || protocol == PROTOCOL_NEHAHRABJP3 || protocol == PROTOCOL_DARKPLACES1 || protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4 || protocol == PROTOCOL_QUAKEWORLD)
		MSG_WriteAngle8i (sb, f);
	else
		MSG_WriteAngle16i (sb, f);
}

//
// reading functions
//

void MSG_InitReadBuffer (sizebuf_t *buf, unsigned char *data, int size)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = buf->cursize = size;
	MSG_BeginReading(buf);
}

void MSG_BeginReading(sizebuf_t *sb)
{
	sb->readcount = 0;
	sb->badread = false;
}

int MSG_ReadLittleShort(sizebuf_t *sb)
{
	if (sb->readcount+2 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 2;
	return (short)(sb->data[sb->readcount-2] | (sb->data[sb->readcount-1]<<8));
}

int MSG_ReadBigShort (sizebuf_t *sb)
{
	if (sb->readcount+2 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 2;
	return (short)((sb->data[sb->readcount-2]<<8) + sb->data[sb->readcount-1]);
}

int MSG_ReadLittleLong (sizebuf_t *sb)
{
	if (sb->readcount+4 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 4;
	return sb->data[sb->readcount-4] | (sb->data[sb->readcount-3]<<8) | (sb->data[sb->readcount-2]<<16) | (sb->data[sb->readcount-1]<<24);
}

int MSG_ReadBigLong (sizebuf_t *sb)
{
	if (sb->readcount+4 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 4;
	return (sb->data[sb->readcount-4]<<24) + (sb->data[sb->readcount-3]<<16) + (sb->data[sb->readcount-2]<<8) + sb->data[sb->readcount-1];
}

float MSG_ReadLittleFloat (sizebuf_t *sb)
{
	union
	{
		float f;
		int l;
	} dat;
	if (sb->readcount+4 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 4;
	dat.l = sb->data[sb->readcount-4] | (sb->data[sb->readcount-3]<<8) | (sb->data[sb->readcount-2]<<16) | (sb->data[sb->readcount-1]<<24);
	return dat.f;
}

float MSG_ReadBigFloat (sizebuf_t *sb)
{
	union
	{
		float f;
		int l;
	} dat;
	if (sb->readcount+4 > sb->cursize)
	{
		sb->badread = true;
		return -1;
	}
	sb->readcount += 4;
	dat.l = (sb->data[sb->readcount-4]<<24) | (sb->data[sb->readcount-3]<<16) | (sb->data[sb->readcount-2]<<8) | sb->data[sb->readcount-1];
	return dat.f;
}

char *MSG_ReadString (sizebuf_t *sb, char *string, size_t maxstring)
{
	int c;
	size_t l = 0;
	// read string into sbfer, but only store as many characters as will fit
	while ((c = MSG_ReadByte(sb)) > 0)
		if (l < maxstring - 1)
			string[l++] = c;
	string[l] = 0;
	return string;
}

int MSG_ReadBytes (sizebuf_t *sb, int numbytes, unsigned char *out)
{
	int l, c;
	for (l = 0;l < numbytes && (c = MSG_ReadByte(sb)) != -1;l++)
		out[l] = c;
	return l;
}

float MSG_ReadCoord13i (sizebuf_t *sb)
{
	return MSG_ReadLittleShort(sb) * (1.0/8.0);
}

float MSG_ReadCoord16i (sizebuf_t *sb)
{
	return (signed short) MSG_ReadLittleShort(sb);
}

float MSG_ReadCoord32f (sizebuf_t *sb)
{
	return MSG_ReadLittleFloat(sb);
}

float MSG_ReadCoord (sizebuf_t *sb, protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_NEHAHRABJP || protocol == PROTOCOL_NEHAHRABJP2 || protocol == PROTOCOL_NEHAHRABJP3 || protocol == PROTOCOL_QUAKEWORLD)
		return MSG_ReadCoord13i(sb);
	else if (protocol == PROTOCOL_DARKPLACES1)
		return MSG_ReadCoord32f(sb);
	else if (protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4)
		return MSG_ReadCoord16i(sb);
	else
		return MSG_ReadCoord32f(sb);
}

void MSG_ReadVector (sizebuf_t *sb, vec3_t v, protocolversion_t protocol)
{
	v[0] = MSG_ReadCoord(sb, protocol);
	v[1] = MSG_ReadCoord(sb, protocol);
	v[2] = MSG_ReadCoord(sb, protocol);
}

// LordHavoc: round to nearest value, rather than rounding toward zero, fixes crosshair problem
float MSG_ReadAngle8i (sizebuf_t *sb)
{
	return (signed char) MSG_ReadByte (sb) * (360.0/256.0);
}

float MSG_ReadAngle16i (sizebuf_t *sb)
{
	return (signed short)MSG_ReadShort (sb) * (360.0/65536.0);
}

float MSG_ReadAngle32f (sizebuf_t *sb)
{
	return MSG_ReadFloat (sb);
}

float MSG_ReadAngle (sizebuf_t *sb, protocolversion_t protocol)
{
	if (protocol == PROTOCOL_QUAKE || protocol == PROTOCOL_QUAKEDP || protocol == PROTOCOL_NEHAHRAMOVIE || protocol == PROTOCOL_NEHAHRABJP || protocol == PROTOCOL_NEHAHRABJP2 || protocol == PROTOCOL_NEHAHRABJP3 || protocol == PROTOCOL_DARKPLACES1 || protocol == PROTOCOL_DARKPLACES2 || protocol == PROTOCOL_DARKPLACES3 || protocol == PROTOCOL_DARKPLACES4 || protocol == PROTOCOL_QUAKEWORLD)
		return MSG_ReadAngle8i (sb);
	else
		return MSG_ReadAngle16i (sb);
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

static const char *hexchar = "0123456789ABCDEF";
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
				// color change prefix character has to be treated specially
				if (d[j] == STRING_COLOR_TAG)
				{
					*cur++ = STRING_COLOR_TAG;
					*cur++ = STRING_COLOR_TAG;
				}
				else if (d[j] >= (unsigned char) ' ')
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
COM_Wordwrap

Word wraps a string. The wordWidth function is guaranteed to be called exactly
once for each word in the string, so it may be stateful, no idea what that
would be good for any more. At the beginning of the string, it will be called
for the char 0 to initialize a clean state, and then once with the string " "
(a space) so the routine knows how long a space is.

In case no single character fits into the given width, the wordWidth function
must return the width of exactly one character.

Wrapped lines get the isContinuation flag set and are continuationWidth less wide.

The sum of the return values of the processLine function will be returned.
==============
*/
int COM_Wordwrap(const char *string, size_t length, float continuationWidth, float maxWidth, COM_WordWidthFunc_t wordWidth, void *passthroughCW, COM_LineProcessorFunc processLine, void *passthroughPL)
{
	// Logic is as follows:
	//
	// For each word or whitespace:
	//   Newline found? Output current line, advance to next line. This is not a continuation. Continue.
	//   Space found? Always add it to the current line, no matter if it fits.
	//   Word found? Check if current line + current word fits.
	//     If it fits, append it. Continue.
	//     If it doesn't fit, output current line, advance to next line. Append the word. This is a continuation. Continue.

	qboolean isContinuation = false;
	float spaceWidth;
	const char *startOfLine = string;
	const char *cursor = string;
	const char *end = string + length;
	float spaceUsedInLine = 0;
	float spaceUsedForWord;
	int result = 0;
	size_t wordLen;
	size_t dummy;

	dummy = 0;
	wordWidth(passthroughCW, NULL, &dummy, -1);
	dummy = 1;
	spaceWidth = wordWidth(passthroughCW, " ", &dummy, -1);

	for(;;)
	{
		char ch = (cursor < end) ? *cursor : 0;
		switch(ch)
		{
			case 0: // end of string
				result += processLine(passthroughPL, startOfLine, cursor - startOfLine, spaceUsedInLine, isContinuation);
				goto out;
			case '\n': // end of line
				result += processLine(passthroughPL, startOfLine, cursor - startOfLine, spaceUsedInLine, isContinuation);
				isContinuation = false;
				++cursor;
				startOfLine = cursor;
				break;
			case ' ': // space
				++cursor;
				spaceUsedInLine += spaceWidth;
				break;
			default: // word
				wordLen = 1;
				while(cursor + wordLen < end)
				{
					switch(cursor[wordLen])
					{
						case 0:
						case '\n':
						case ' ':
							goto out_inner;
						default:
							++wordLen;
							break;
					}
				}
				out_inner:
				spaceUsedForWord = wordWidth(passthroughCW, cursor, &wordLen, maxWidth - continuationWidth); // this may have reduced wordLen when it won't fit - but this is GOOD. TODO fix words that do fit in a non-continuation line
				if(wordLen < 1) // cannot happen according to current spec of wordWidth
				{
					wordLen = 1;
					spaceUsedForWord = maxWidth + 1; // too high, forces it in a line of itself
				}
				if(spaceUsedInLine + spaceUsedForWord <= maxWidth || cursor == startOfLine)
				{
					// we can simply append it
					cursor += wordLen;
					spaceUsedInLine += spaceUsedForWord;
				}
				else
				{
					// output current line
					result += processLine(passthroughPL, startOfLine, cursor - startOfLine, spaceUsedInLine, isContinuation);
					isContinuation = true;
					startOfLine = cursor;
					cursor += wordLen;
					spaceUsedInLine = continuationWidth + spaceUsedForWord;
				}
		}
	}
	out:

	return result;

/*
	qboolean isContinuation = false;
	float currentWordSpace = 0;
	const char *currentWord = 0;
	float minReserve = 0;

	float spaceUsedInLine = 0;
	const char *currentLine = 0;
	const char *currentLineEnd = 0;
	float currentLineFinalWhitespace = 0;
	const char *p;

	int result = 0;
	minReserve = charWidth(passthroughCW, 0);
	minReserve += charWidth(passthroughCW, ' ');

	if(maxWidth < continuationWidth + minReserve)
		maxWidth = continuationWidth + minReserve;

	charWidth(passthroughCW, 0);

	for(p = string; p < string + length; ++p)
	{
		char c = *p;
		float w = charWidth(passthroughCW, c);

		if(!currentWord)
		{
			currentWord = p;
			currentWordSpace = 0;
		}

		if(!currentLine)
		{
			currentLine = p;
			spaceUsedInLine = isContinuation ? continuationWidth : 0;
			currentLineEnd = 0;
		}

		if(c == ' ')
		{
			// 1. I can add the word AND a space - then just append it.
			if(spaceUsedInLine + currentWordSpace + w <= maxWidth)
			{
				currentLineEnd = p; // note: space not included here
				currentLineFinalWhitespace = w;
				spaceUsedInLine += currentWordSpace + w;
			}
			// 2. I can just add the word - then append it, output current line and go to next one.
			else if(spaceUsedInLine + currentWordSpace <= maxWidth)
			{
				result += processLine(passthroughPL, currentLine, p - currentLine, spaceUsedInLine + currentWordSpace, isContinuation);
				currentLine = 0;
				isContinuation = true;
			}
			// 3. Otherwise, output current line and go to next one, where I can add the word.
			else if(continuationWidth + currentWordSpace + w <= maxWidth)
			{
				if(currentLineEnd)
					result += processLine(passthroughPL, currentLine, currentLineEnd - currentLine, spaceUsedInLine - currentLineFinalWhitespace, isContinuation);
				currentLine = currentWord;
				spaceUsedInLine = continuationWidth + currentWordSpace + w;
				currentLineEnd = p;
				currentLineFinalWhitespace = w;
				isContinuation = true;
			}
			// 4. We can't even do that? Then output both current and next word as new lines.
			else
			{
				if(currentLineEnd)
				{
					result += processLine(passthroughPL, currentLine, currentLineEnd - currentLine, spaceUsedInLine - currentLineFinalWhitespace, isContinuation);
					isContinuation = true;
				}
				result += processLine(passthroughPL, currentWord, p - currentWord, currentWordSpace, isContinuation);
				currentLine = 0;
				isContinuation = true;
			}
			currentWord = 0;
		}
		else if(c == '\n')
		{
			// 1. I can add the word - then do it.
			if(spaceUsedInLine + currentWordSpace <= maxWidth)
			{
				result += processLine(passthroughPL, currentLine, p - currentLine, spaceUsedInLine + currentWordSpace, isContinuation);
			}
			// 2. Otherwise, output current line, next one and make tabula rasa.
			else
			{
				if(currentLineEnd)
				{
					processLine(passthroughPL, currentLine, currentLineEnd - currentLine, spaceUsedInLine - currentLineFinalWhitespace, isContinuation);
					isContinuation = true;
				}
				result += processLine(passthroughPL, currentWord, p - currentWord, currentWordSpace, isContinuation);
			}
			currentWord = 0;
			currentLine = 0;
			isContinuation = false;
		}
		else
		{
			currentWordSpace += w;
			if(
				spaceUsedInLine + currentWordSpace > maxWidth // can't join this line...
				&&
				continuationWidth + currentWordSpace > maxWidth // can't join any other line...
			)
			{
				// this word cannot join ANY line...
				// so output the current line...
				if(currentLineEnd)
				{
					result += processLine(passthroughPL, currentLine, currentLineEnd - currentLine, spaceUsedInLine - currentLineFinalWhitespace, isContinuation);
					isContinuation = true;
				}

				// then this word's beginning...
				if(isContinuation)
				{
					// it may not fit, but we know we have to split it into maxWidth - continuationWidth pieces
					float pieceWidth = maxWidth - continuationWidth;
					const char *pos = currentWord;
					currentWordSpace = 0;

					// reset the char width function to a state where no kerning occurs (start of word)
					charWidth(passthroughCW, ' ');
					while(pos <= p)
					{
						float w = charWidth(passthroughCW, *pos);
						if(currentWordSpace + w > pieceWidth) // this piece won't fit any more
						{
							// print everything until it
							result += processLine(passthroughPL, currentWord, pos - currentWord, currentWordSpace, true);
							// go to here
							currentWord = pos;
							currentWordSpace = 0;
						}
						currentWordSpace += w;
						++pos;
					}
					// now we have a currentWord that fits... set up its next line
					// currentWordSpace has been set
					// currentWord has been set
					spaceUsedInLine = continuationWidth;
					currentLine = currentWord;
					currentLineEnd = 0;
					isContinuation = true;
				}
				else
				{
					// we have a guarantee that it will fix (see if clause)
					result += processLine(passthroughPL, currentWord, p - currentWord, currentWordSpace - w, isContinuation);

					// and use the rest of this word as new start of a line
					currentWordSpace = w;
					currentWord = p;
					spaceUsedInLine = continuationWidth;
					currentLine = p;
					currentLineEnd = 0;
					isContinuation = true;
				}
			}
		}
	}

	if(!currentWord)
	{
		currentWord = p;
		currentWordSpace = 0;
	}

	if(currentLine) // Same procedure as \n
	{
		// Can I append the current word?
		if(spaceUsedInLine + currentWordSpace <= maxWidth)
			result += processLine(passthroughPL, currentLine, p - currentLine, spaceUsedInLine + currentWordSpace, isContinuation);
		else
		{
			if(currentLineEnd)
			{
				result += processLine(passthroughPL, currentLine, currentLineEnd - currentLine, spaceUsedInLine - currentLineFinalWhitespace, isContinuation);
				isContinuation = true;
			}
			result += processLine(passthroughPL, currentWord, p - currentWord, currentWordSpace, isContinuation);
		}
	}

	return result;
*/
}

/*
==============
COM_ParseToken_Simple

Parse a token out of a string
==============
*/
int COM_ParseToken_Simple(const char **datapointer, qboolean returnnewline, qboolean parsebackslash, qboolean parsecomments)
{
	int len;
	int c;
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
	for (;ISWHITESPACE(*data) && ((*data != '\n' && *data != '\r') || !returnnewline);data++)
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

	if (parsecomments && data[0] == '/' && data[1] == '/')
	{
		// comment
		while (*data && *data != '\n' && *data != '\r')
			data++;
		goto skipwhite;
	}
	else if (parsecomments && data[0] == '/' && data[1] == '*')
	{
		// comment
		data++;
		while (*data && (data[0] != '*' || data[1] != '/'))
			data++;
		if (*data)
			data++;
		if (*data)
			data++;
		goto skipwhite;
	}
	else if (*data == '\"')
	{
		// quoted string
		for (data++;*data && *data != '\"';data++)
		{
			c = *data;
			if (*data == '\\' && parsebackslash)
			{
				data++;
				c = *data;
				if (c == 'n')
					c = '\n';
				else if (c == 't')
					c = '\t';
			}
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = c;
		}
		com_token[len] = 0;
		if (*data == '\"')
			data++;
		*datapointer = data;
		return true;
	}
	else if (*data == '\r')
	{
		// translate Mac line ending to UNIX
		com_token[len++] = '\n';data++;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else if (*data == '\n')
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
		for (;!ISWHITESPACE(*data);data++)
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = *data;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}

/*
==============
COM_ParseToken_QuakeC

Parse a token out of a string
==============
*/
int COM_ParseToken_QuakeC(const char **datapointer, qboolean returnnewline)
{
	int len;
	int c;
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
	for (;ISWHITESPACE(*data) && ((*data != '\n' && *data != '\r') || !returnnewline);data++)
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
		if (*data)
			data++;
		if (*data)
			data++;
		goto skipwhite;
	}
	else if (*data == '\"' || *data == '\'')
	{
		// quoted string
		char quote = *data;
		for (data++;*data && *data != quote;data++)
		{
			c = *data;
			if (*data == '\\')
			{
				data++;
				c = *data;
				if (c == 'n')
					c = '\n';
				else if (c == 't')
					c = '\t';
			}
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = c;
		}
		com_token[len] = 0;
		if (*data == quote)
			data++;
		*datapointer = data;
		return true;
	}
	else if (*data == '\r')
	{
		// translate Mac line ending to UNIX
		com_token[len++] = '\n';data++;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else if (*data == '\n' || *data == '{' || *data == '}' || *data == ')' || *data == '(' || *data == ']' || *data == '[' || *data == ':' || *data == ',' || *data == ';')
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
		for (;!ISWHITESPACE(*data) && *data != '{' && *data != '}' && *data != ')' && *data != '(' && *data != ']' && *data != '[' && *data != ':' && *data != ',' && *data != ';';data++)
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = *data;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}

/*
==============
COM_ParseToken_VM_Tokenize

Parse a token out of a string
==============
*/
int COM_ParseToken_VM_Tokenize(const char **datapointer, qboolean returnnewline)
{
	int len;
	int c;
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
	for (;ISWHITESPACE(*data) && ((*data != '\n' && *data != '\r') || !returnnewline);data++)
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
		if (*data)
			data++;
		if (*data)
			data++;
		goto skipwhite;
	}
	else if (*data == '\"' || *data == '\'')
	{
		char quote = *data;
		// quoted string
		for (data++;*data && *data != quote;data++)
		{
			c = *data;
			if (*data == '\\')
			{
				data++;
				c = *data;
				if (c == 'n')
					c = '\n';
				else if (c == 't')
					c = '\t';
			}
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = c;
		}
		com_token[len] = 0;
		if (*data == quote)
			data++;
		*datapointer = data;
		return true;
	}
	else if (*data == '\r')
	{
		// translate Mac line ending to UNIX
		com_token[len++] = '\n';data++;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
	else if (*data == '\n' || *data == '{' || *data == '}' || *data == ')' || *data == '(' || *data == ']' || *data == '[' || *data == ':' || *data == ',' || *data == ';')
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
		for (;!ISWHITESPACE(*data) && *data != '{' && *data != '}' && *data != ')' && *data != '(' && *data != ']' && *data != '[' && *data != ':' && *data != ',' && *data != ';';data++)
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = *data;
		com_token[len] = 0;
		*datapointer = data;
		return true;
	}
}

/*
==============
COM_ParseToken_Console

Parse a token out of a string, behaving like the qwcl console
==============
*/
int COM_ParseToken_Console(const char **datapointer)
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
	for (;ISWHITESPACE(*data);data++)
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
		for (data++;*data && *data != '\"';data++)
		{
			// allow escaped " and \ case
			if (*data == '\\' && (data[1] == '\"' || data[1] == '\\'))
				data++;
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = *data;
		}
		com_token[len] = 0;
		if (*data == '\"')
			data++;
		*datapointer = data;
	}
	else
	{
		// regular word
		for (;!ISWHITESPACE(*data);data++)
			if (len < (int)sizeof(com_token) - 1)
				com_token[len++] = *data;
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

//===========================================================================

// Game mods

gamemode_t com_startupgamemode;
gamemode_t com_startupgamegroup;

typedef struct gamemode_info_s
{
	gamemode_t mode; // this gamemode
	gamemode_t group; // different games with same group can switch automatically when gamedirs change
	const char* prog_name; // not null
	const char* cmdline; // not null
	const char* gamename; // not null
	const char*	gamenetworkfiltername; // not null
	const char* gamedirname1; // not null
	const char* gamedirname2; // null
	const char* gamescreenshotname; // not nul
	const char* gameuserdirname; // not null
} gamemode_info_t;

static const gamemode_info_t gamemode_info [GAME_COUNT] =
{// game						basegame					prog_name				cmdline						gamename					gamenetworkfilername		basegame	modgame			screenshot			userdir					   // commandline option
{ GAME_NORMAL,					GAME_NORMAL,				"",						"-quake",					"DarkPlaces-Quake",			"DarkPlaces-Quake",			"id1",		NULL,			"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -quake runs the game Quake (default)
{ GAME_HIPNOTIC,				GAME_NORMAL,				"hipnotic",				"-hipnotic",				"Darkplaces-Hipnotic",		"Darkplaces-Hipnotic",		"id1",		"hipnotic",		"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -hipnotic runs Quake mission pack 1: The Scourge of Armagon
{ GAME_ROGUE,					GAME_NORMAL,				"rogue",				"-rogue",					"Darkplaces-Rogue",			"Darkplaces-Rogue",			"id1",		"rogue",		"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -rogue runs Quake mission pack 2: The Dissolution of Eternity
{ GAME_NEHAHRA,					GAME_NORMAL,				"nehahra",				"-nehahra",					"DarkPlaces-Nehahra",		"DarkPlaces-Nehahra",		"id1",		"nehahra",		"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -nehahra runs The Seal of Nehahra movie and game
{ GAME_QUOTH,					GAME_NORMAL,				"quoth",				"-quoth",					"Darkplaces-Quoth",			"Darkplaces-Quoth",			"id1",		"quoth",		"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -quoth runs the Quoth mod for playing community maps made for it
{ GAME_NEXUIZ,					GAME_NEXUIZ,				"nexuiz",				"-nexuiz",					"Nexuiz",					"Nexuiz",					"data",		NULL,			"nexuiz",			"nexuiz"				}, // COMMANDLINEOPTION: Game: -nexuiz runs the multiplayer game Nexuiz
{ GAME_XONOTIC,					GAME_XONOTIC,				"xonotic",				"-xonotic",					"Xonotic",					"Xonotic",					"data",		NULL,			"xonotic",			"xonotic"				}, // COMMANDLINEOPTION: Game: -xonotic runs the multiplayer game Xonotic
{ GAME_TRANSFUSION,				GAME_TRANSFUSION,			"transfusion",			"-transfusion",				"Transfusion",				"Transfusion",				"basetf",	NULL,			"transfusion",		"transfusion"			}, // COMMANDLINEOPTION: Game: -transfusion runs Transfusion (the recreation of Blood in Quake)
{ GAME_GOODVSBAD2,				GAME_GOODVSBAD2,			"gvb2",					"-goodvsbad2",				"GoodVs.Bad2",				"GoodVs.Bad2",				"rts",		NULL,			"gvb2",				"gvb2"					}, // COMMANDLINEOPTION: Game: -goodvsbad2 runs the psychadelic RTS FPS game Good Vs Bad 2
{ GAME_TEU,						GAME_TEU,					"teu",					"-teu",						"TheEvilUnleashed",			"TheEvilUnleashed",			"baseteu",	NULL,			"teu",				"teu"					}, // COMMANDLINEOPTION: Game: -teu runs The Evil Unleashed (this option is obsolete as they are not using darkplaces)
{ GAME_BATTLEMECH,				GAME_BATTLEMECH,			"battlemech",			"-battlemech",				"Battlemech",				"Battlemech",				"base",		NULL,			"battlemech",		"battlemech"			}, // COMMANDLINEOPTION: Game: -battlemech runs the multiplayer topdown deathmatch game BattleMech
{ GAME_ZYMOTIC,					GAME_ZYMOTIC,				"zymotic",				"-zymotic",					"Zymotic",					"Zymotic",					"basezym",	NULL,			"zymotic",			"zymotic"				}, // COMMANDLINEOPTION: Game: -zymotic runs the singleplayer game Zymotic
{ GAME_SETHERAL,				GAME_SETHERAL,				"setheral",				"-setheral",				"Setheral",					"Setheral",					"data",		NULL,			"setheral",			"setheral"				}, // COMMANDLINEOPTION: Game: -setheral runs the multiplayer game Setheral
{ GAME_TENEBRAE,				GAME_NORMAL,				"tenebrae",				"-tenebrae",				"DarkPlaces-Tenebrae",		"DarkPlaces-Tenebrae",		"id1",		"tenebrae",		"dp",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -tenebrae runs the graphics test mod known as Tenebrae (some features not implemented)
{ GAME_NEOTERIC,				GAME_NORMAL,				"neoteric",				"-neoteric",				"Neoteric",					"Neoteric",					"id1",		"neobase",		"neo",				"darkplaces"			}, // COMMANDLINEOPTION: Game: -neoteric runs the game Neoteric
{ GAME_OPENQUARTZ,				GAME_NORMAL,				"openquartz",			"-openquartz",				"OpenQuartz",				"OpenQuartz",				"id1",		NULL,			"openquartz",		"darkplaces"			}, // COMMANDLINEOPTION: Game: -openquartz runs the game OpenQuartz, a standalone GPL replacement of the quake content
{ GAME_PRYDON,					GAME_NORMAL,				"prydon",				"-prydon",					"PrydonGate",				"PrydonGate",				"id1",		"prydon",		"prydon",			"darkplaces"			}, // COMMANDLINEOPTION: Game: -prydon runs the topdown point and click action-RPG Prydon Gate
{ GAME_DELUXEQUAKE,				GAME_DELUXEQUAKE,			"dq",					"-dq",						"Deluxe Quake",				"Deluxe_Quake",				"basedq",	"extradq",		"basedq",			"dq"					}, // COMMANDLINEOPTION: Game: -dq runs the game Deluxe Quake
{ GAME_THEHUNTED,				GAME_THEHUNTED,				"thehunted",			"-thehunted",				"The Hunted",				"The_Hunted",				"thdata",	NULL, 			"th",				"thehunted"				}, // COMMANDLINEOPTION: Game: -thehunted runs the game The Hunted
{ GAME_DEFEATINDETAIL2,			GAME_DEFEATINDETAIL2,		"did2",					"-did2",					"Defeat In Detail 2",		"Defeat_In_Detail_2",		"data",		NULL, 			"did2_",			"did2"					}, // COMMANDLINEOPTION: Game: -did2 runs the game Defeat In Detail 2
{ GAME_DARSANA,					GAME_DARSANA,				"darsana",				"-darsana",					"Darsana",					"Darsana",					"ddata",	NULL, 			"darsana",			"darsana"				}, // COMMANDLINEOPTION: Game: -darsana runs the game Darsana
{ GAME_CONTAGIONTHEORY,			GAME_CONTAGIONTHEORY,		"contagiontheory",		"-contagiontheory",			"Contagion Theory",			"Contagion_Theory",			"ctdata",	NULL, 			"ct",				"contagiontheory"		}, // COMMANDLINEOPTION: Game: -contagiontheory runs the game Contagion Theory
{ GAME_EDU2P,					GAME_EDU2P,					"edu2p",				"-edu2p",					"EDU2 Prototype",			"EDU2_Prototype",			"id1",		"edu2",			"edu2_p",			"edu2prototype"			}, // COMMANDLINEOPTION: Game: -edu2p runs the game Edu2 prototype
{ GAME_PROPHECY,				GAME_PROPHECY,				"prophecy",				"-prophecy",				"Prophecy",					"Prophecy",					"gamedata",	NULL,			"phcy",				"prophecy"				}, // COMMANDLINEOPTION: Game: -prophecy runs the game Prophecy
{ GAME_BLOODOMNICIDE,			GAME_BLOODOMNICIDE,			"omnicide",				"-omnicide",				"Blood Omnicide",			"Blood_Omnicide",			"kain",		NULL,			"omnicide",			"omnicide"				}, // COMMANDLINEOPTION: Game: -omnicide runs the game Blood Omnicide
{ GAME_STEELSTORM,				GAME_STEELSTORM,			"steelstorm",			"-steelstorm",				"Steel-Storm",				"Steel-Storm",				"gamedata",	NULL,			"ss",				"steelstorm"			}, // COMMANDLINEOPTION: Game: -steelstorm runs the game Steel Storm
{ GAME_STEELSTORM2,				GAME_STEELSTORM2,			"steelstorm2",			"-steelstorm2",				"Steel Storm 2",			"Steel_Storm_2",			"gamedata",	NULL,			"ss2",				"steelstorm2"			}, // COMMANDLINEOPTION: Game: -steelstorm2 runs the game Steel Storm 2
{ GAME_SSAMMO,					GAME_SSAMMO,				"steelstorm-ammo",		"-steelstormammo",			"Steel Storm A.M.M.O.",		"Steel_Storm_A.M.M.O.",		"gamedata", NULL,			"ssammo",			"steelstorm-ammo"		}, // COMMANDLINEOPTION: Game: -steelstormammo runs the game Steel Storm A.M.M.O.
{ GAME_TOMESOFMEPHISTOPHELES,	GAME_TOMESOFMEPHISTOPHELES,	"tomesofmephistopheles","-tomesofmephistopheles",	"Tomes of Mephistopheles",	"Tomes_of_Mephistopheles",	"gamedata",	NULL,			"tom",				"tomesofmephistopheles"	}, // COMMANDLINEOPTION: Game: -tomesofmephistopheles runs the game Tomes of Mephistopheles
{ GAME_STRAPBOMB,				GAME_STRAPBOMB,				"strapbomb",			"-strapbomb",				"Strap-on-bomb Car",		"Strap-on-bomb_Car",		"id1",		NULL,			"strap",			"strapbomb"				}, // COMMANDLINEOPTION: Game: -strapbomb runs the game Strap-on-bomb Car
{ GAME_MOONHELM,				GAME_MOONHELM,				"moonhelm",				"-moonhelm",				"MoonHelm",					"MoonHelm",					"data",		NULL,			"mh",				"moonhelm"				}, // COMMANDLINEOPTION: Game: -moonhelm runs the game MoonHelm
{ GAME_VORETOURNAMENT,			GAME_VORETOURNAMENT,		"voretournament",		"-voretournament",			"Vore Tournament",			"Vore_Tournament",			"data",		NULL,			"voretournament",	"voretournament"		}, // COMMANDLINEOPTION: Game: -voretournament runs the multiplayer game Vore Tournament
};

static void COM_SetGameType(int index);
void COM_InitGameType (void)
{
	char name [MAX_OSPATH];
	int i;
	int index = 0;

#ifdef FORCEGAME
	COM_ToLowerString(FORCEGAME, name, sizeof (name));
#else
	// check executable filename for keywords, but do it SMARTLY - only check the last path element
	FS_StripExtension(FS_FileWithoutPath(com_argv[0]), name, sizeof (name));
	COM_ToLowerString(name, name, sizeof (name));
#endif
	for (i = 1;i < (int)(sizeof (gamemode_info) / sizeof (gamemode_info[0]));i++)
		if (gamemode_info[i].prog_name && gamemode_info[i].prog_name[0] && strstr (name, gamemode_info[i].prog_name))
			index = i;

	// check commandline options for keywords
	for (i = 0;i < (int)(sizeof (gamemode_info) / sizeof (gamemode_info[0]));i++)
		if (COM_CheckParm (gamemode_info[i].cmdline))
			index = i;

	com_startupgamemode = gamemode_info[index].mode;
	com_startupgamegroup = gamemode_info[index].group;
	COM_SetGameType(index);
}

void COM_ChangeGameTypeForGameDirs(void)
{
	int i;
	int index = -1;
	// this will not not change the gamegroup
	// first check if a base game (single gamedir) matches
	for (i = 0;i < (int)(sizeof (gamemode_info) / sizeof (gamemode_info[0]));i++)
	{
		if (gamemode_info[i].group == com_startupgamegroup && !(gamemode_info[i].gamedirname2 && gamemode_info[i].gamedirname2[0]))
		{
			index = i;
			break;
		}
	}
	// now that we have a base game, see if there is a matching derivative game (two gamedirs)
	if (fs_numgamedirs)
	{
		for (i = 0;i < (int)(sizeof (gamemode_info) / sizeof (gamemode_info[0]));i++)
		{
			if (gamemode_info[i].group == com_startupgamegroup && (gamemode_info[i].gamedirname2 && gamemode_info[i].gamedirname2[0]) && !strcasecmp(fs_gamedirs[0], gamemode_info[i].gamedirname2))
			{
				index = i;
				break;
			}
		}
	}
	// we now have a good guess at which game this is meant to be...
	if (index >= 0 && gamemode != gamemode_info[index].mode)
		COM_SetGameType(index);
}

static void COM_SetGameType(int index)
{
	static char gamenetworkfilternamebuffer[64];
	int i, t;
	if (index < 0 || index >= (int)(sizeof (gamemode_info) / sizeof (gamemode_info[0])))
		index = 0;
	gamemode = gamemode_info[index].mode;
	gamename = gamemode_info[index].gamename;
	gamenetworkfiltername = gamemode_info[index].gamenetworkfiltername;
	gamedirname1 = gamemode_info[index].gamedirname1;
	gamedirname2 = gamemode_info[index].gamedirname2;
	gamescreenshotname = gamemode_info[index].gamescreenshotname;
	gameuserdirname = gamemode_info[index].gameuserdirname;

	if (gamemode == com_startupgamemode)
	{
		if((t = COM_CheckParm("-customgamename")) && t + 1 < com_argc)
			gamename = gamenetworkfiltername = com_argv[t+1];
		if((t = COM_CheckParm("-customgamenetworkfiltername")) && t + 1 < com_argc)
			gamenetworkfiltername = com_argv[t+1];
		if((t = COM_CheckParm("-customgamedirname1")) && t + 1 < com_argc)
			gamedirname1 = com_argv[t+1];
		if((t = COM_CheckParm("-customgamedirname2")) && t + 1 < com_argc)
			gamedirname2 = *com_argv[t+1] ? com_argv[t+1] : NULL;
		if((t = COM_CheckParm("-customgamescreenshotname")) && t + 1 < com_argc)
			gamescreenshotname = com_argv[t+1];
		if((t = COM_CheckParm("-customgameuserdirname")) && t + 1 < com_argc)
			gameuserdirname = com_argv[t+1];
	}

	if (gamedirname2 && gamedirname2[0])
		Con_Printf("Game is %s using base gamedirs %s %s", gamename, gamedirname1, gamedirname2);
	else
		Con_Printf("Game is %s using base gamedir %s", gamename, gamedirname1);
	for (i = 0;i < fs_numgamedirs;i++)
	{
		if (i == 0)
			Con_Printf(", with mod gamedirs");
		Con_Printf(" %s", fs_gamedirs[i]);
	}
	Con_Printf("\n");

	if (strchr(gamenetworkfiltername, ' '))
	{
		char *s;
		// if there are spaces in the game's network filter name it would
		// cause parse errors in getservers in dpmaster, so we need to replace
		// them with _ characters
		strlcpy(gamenetworkfilternamebuffer, gamenetworkfiltername, sizeof(gamenetworkfilternamebuffer));
		while ((s = strchr(gamenetworkfilternamebuffer, ' ')) != NULL)
			*s = '_';
		gamenetworkfiltername = gamenetworkfilternamebuffer;
	}

	Con_Printf("gamename for server filtering: %s\n", gamenetworkfiltername);
}


/*
================
COM_Init
================
*/
void COM_Init_Commands (void)
{
	int i, j, n;
	char com_cmdline[MAX_INPUTLINE];

	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);

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
	Cvar_Set ("cmdline", com_cmdline);
}

/*
============
va

varargs print into provided buffer, returns buffer (so that it can be called in-line, unlike dpsnprintf)
============
*/
char *va(char *buf, size_t buflen, const char *format, ...)
{
	va_list argptr;

	va_start (argptr, format);
	dpvsnprintf (buf, buflen, format,argptr);
	va_end (argptr);

	return buf;
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

#if _MSC_VER >= 1400
	result = _vsnprintf_s (buffer, buffersize, _TRUNCATE, format, args);
#else
	result = vsnprintf (buffer, buffersize, format, args);
#endif
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

	if(utf8_enable.integer)
	{
		*out = 0;
		while(*in && size_out > 1)
		{
			int n;
			Uchar ch = u8_getchar_utf8_enabled(in, &in);
			ch = u8_tolower(ch);
			n = u8_fromchar(ch, out, size_out);
			if(n <= 0)
				break;
			out += n;
			size_out -= n;
		}
		return;
	}

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

	if(utf8_enable.integer)
	{
		*out = 0;
		while(*in && size_out > 1)
		{
			int n;
			Uchar ch = u8_getchar_utf8_enabled(in, &in);
			ch = u8_toupper(ch);
			n = u8_fromchar(ch, out, size_out);
			if(n <= 0)
				break;
			out += n;
			size_out -= n;
		}
		return;
	}

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
		if (!ISWHITESPACE(*l))
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
				while (!ISWHITESPACE(*l))
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

/*
============
COM_StringLengthNoColors

calculates the visible width of a color coded string.

*valid is filled with TRUE if the string is a valid colored string (that is, if
it does not end with an unfinished color code). If it gets filled with FALSE, a
fix would be adding a STRING_COLOR_TAG at the end of the string.

valid can be set to NULL if the caller doesn't care.

For size_s, specify the maximum number of characters from s to use, or 0 to use
all characters until the zero terminator.
============
*/
size_t
COM_StringLengthNoColors(const char *s, size_t size_s, qboolean *valid)
{
	const char *end = size_s ? (s + size_s) : NULL;
	size_t len = 0;
	for(;;)
	{
		switch((s == end) ? 0 : *s)
		{
			case 0:
				if(valid)
					*valid = TRUE;
				return len;
			case STRING_COLOR_TAG:
				++s;
				switch((s == end) ? 0 : *s)
				{
					case STRING_COLOR_RGB_TAG_CHAR:
						if (s+1 != end && isxdigit(s[1]) &&
							s+2 != end && isxdigit(s[2]) &&
							s+3 != end && isxdigit(s[3]) )
						{
							s+=3;
							break;
						}
						++len; // STRING_COLOR_TAG
						++len; // STRING_COLOR_RGB_TAG_CHAR
						break;
					case 0: // ends with unfinished color code!
						++len;
						if(valid)
							*valid = FALSE;
						return len;
					case STRING_COLOR_TAG: // escaped ^
						++len;
						break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9': // color code
						break;
					default: // not a color code
						++len; // STRING_COLOR_TAG
						++len; // the character
						break;
				}
				break;
			default:
				++len;
				break;
		}
		++s;
	}
	// never get here
}

/*
============
COM_StringDecolorize

removes color codes from a string.

If escape_carets is true, the resulting string will be safe for printing. If
escape_carets is false, the function will just strip color codes (for logging
for example).

If the output buffer size did not suffice for converting, the function returns
FALSE. Generally, if escape_carets is false, the output buffer needs
strlen(str)+1 bytes, and if escape_carets is true, it can need strlen(str)*1.5+2
bytes. In any case, the function makes sure that the resulting string is
zero terminated.

For size_in, specify the maximum number of characters from in to use, or 0 to use
all characters until the zero terminator.
============
*/
qboolean
COM_StringDecolorize(const char *in, size_t size_in, char *out, size_t size_out, qboolean escape_carets)
{
#define APPEND(ch) do { if(--size_out) { *out++ = (ch); } else { *out++ = 0; return FALSE; } } while(0)
	const char *end = size_in ? (in + size_in) : NULL;
	if(size_out < 1)
		return FALSE;
	for(;;)
	{
		switch((in == end) ? 0 : *in)
		{
			case 0:
				*out++ = 0;
				return TRUE;
			case STRING_COLOR_TAG:
				++in;
				switch((in == end) ? 0 : *in)
				{
					case STRING_COLOR_RGB_TAG_CHAR:
						if (in+1 != end && isxdigit(in[1]) &&
							in+2 != end && isxdigit(in[2]) &&
							in+3 != end && isxdigit(in[3]) )
						{
							in+=3;
							break;
						}
						APPEND(STRING_COLOR_TAG);
						if(escape_carets)
							APPEND(STRING_COLOR_TAG);
						APPEND(STRING_COLOR_RGB_TAG_CHAR);
						break;
					case 0: // ends with unfinished color code!
						APPEND(STRING_COLOR_TAG);
						// finish the code by appending another caret when escaping
						if(escape_carets)
							APPEND(STRING_COLOR_TAG);
						*out++ = 0;
						return TRUE;
					case STRING_COLOR_TAG: // escaped ^
						APPEND(STRING_COLOR_TAG);
						// append a ^ twice when escaping
						if(escape_carets)
							APPEND(STRING_COLOR_TAG);
						break;
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9': // color code
						break;
					default: // not a color code
						APPEND(STRING_COLOR_TAG);
						APPEND(*in);
						break;
				}
				break;
			default:
				APPEND(*in);
				break;
		}
		++in;
	}
	// never get here
#undef APPEND
}

char *InfoString_GetValue(const char *buffer, const char *key, char *value, size_t valuelength)
{
	int pos = 0, j;
	size_t keylength;
	if (!key)
		key = "";
	keylength = strlen(key);
	if (valuelength < 1 || !value)
	{
		Con_Printf("InfoString_GetValue: no room in value\n");
		return NULL;
	}
	value[0] = 0;
	if (strchr(key, '\\'))
	{
		Con_Printf("InfoString_GetValue: key name \"%s\" contains \\ which is not possible in an infostring\n", key);
		return NULL;
	}
	if (strchr(key, '\"'))
	{
		Con_Printf("InfoString_SetValue: key name \"%s\" contains \" which is not allowed in an infostring\n", key);
		return NULL;
	}
	if (!key[0])
	{
		Con_Printf("InfoString_GetValue: can not look up a key with no name\n");
		return NULL;
	}
	while (buffer[pos] == '\\')
	{
		if (!memcmp(buffer + pos+1, key, keylength))
		{
			for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
			pos++;
			for (j = 0;buffer[pos+j] && buffer[pos+j] != '\\' && j < (int)valuelength - 1;j++)
				value[j] = buffer[pos+j];
			value[j] = 0;
			return value;
		}
		for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
		for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
	}
	// if we reach this point the key was not found
	return NULL;
}

void InfoString_SetValue(char *buffer, size_t bufferlength, const char *key, const char *value)
{
	int pos = 0, pos2;
	size_t keylength;
	if (!key)
		key = "";
	if (!value)
		value = "";
	keylength = strlen(key);
	if (strchr(key, '\\') || strchr(value, '\\'))
	{
		Con_Printf("InfoString_SetValue: \"%s\" \"%s\" contains \\ which is not possible to store in an infostring\n", key, value);
		return;
	}
	if (strchr(key, '\"') || strchr(value, '\"'))
	{
		Con_Printf("InfoString_SetValue: \"%s\" \"%s\" contains \" which is not allowed in an infostring\n", key, value);
		return;
	}
	if (!key[0])
	{
		Con_Printf("InfoString_SetValue: can not set a key with no name\n");
		return;
	}
	while (buffer[pos] == '\\')
	{
		if (!memcmp(buffer + pos+1, key, keylength))
			break;
		for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
		for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
	}
	// if we found the key, find the end of it because we will be replacing it
	pos2 = pos;
	if (buffer[pos] == '\\')
	{
		for (pos2++;buffer[pos2] && buffer[pos2] != '\\';pos2++);
		for (pos2++;buffer[pos2] && buffer[pos2] != '\\';pos2++);
	}
	if (bufferlength <= pos + 1 + strlen(key) + 1 + strlen(value) + strlen(buffer + pos2))
	{
		Con_Printf("InfoString_SetValue: no room for \"%s\" \"%s\" in infostring\n", key, value);
		return;
	}
	if (value && value[0])
	{
		// set the key/value and append the remaining text
		char tempbuffer[MAX_INPUTLINE];
		strlcpy(tempbuffer, buffer + pos2, sizeof(tempbuffer));
		dpsnprintf(buffer + pos, bufferlength - pos, "\\%s\\%s%s", key, value, tempbuffer);
	}
	else
	{
		// just remove the key from the text
		strlcpy(buffer + pos, buffer + pos2, bufferlength - pos);
	}
}

void InfoString_Print(char *buffer)
{
	int i;
	char key[MAX_INPUTLINE];
	char value[MAX_INPUTLINE];
	while (*buffer)
	{
		if (*buffer != '\\')
		{
			Con_Printf("InfoString_Print: corrupt string\n");
			return;
		}
		for (buffer++, i = 0;*buffer && *buffer != '\\';buffer++)
			if (i < (int)sizeof(key)-1)
				key[i++] = *buffer;
		key[i] = 0;
		if (*buffer != '\\')
		{
			Con_Printf("InfoString_Print: corrupt string\n");
			return;
		}
		for (buffer++, i = 0;*buffer && *buffer != '\\';buffer++)
			if (i < (int)sizeof(value)-1)
				value[i++] = *buffer;
		value[i] = 0;
		// empty value is an error case
		Con_Printf("%20s %s\n", key, value[0] ? value : "NO VALUE");
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

void FindFraction(double val, int *num, int *denom, int denomMax)
{
	int i;
	double bestdiff;
	// initialize
	bestdiff = fabs(val);
	*num = 0;
	*denom = 1;

	for(i = 1; i <= denomMax; ++i)
	{
		int inum = (int) floor(0.5 + val * i);
		double diff = fabs(val - inum / (double)i);
		if(diff < bestdiff)
		{
			bestdiff = diff;
			*num = inum;
			*denom = i;
		}
	}
}

// decodes an XPM from C syntax
char **XPM_DecodeString(const char *in)
{
	static char *tokens[257];
	static char lines[257][512];
	size_t line = 0;

	// skip until "{" token
	while(COM_ParseToken_QuakeC(&in, false) && strcmp(com_token, "{"));

	// now, read in succession: string, comma-or-}
	while(COM_ParseToken_QuakeC(&in, false))
	{
		tokens[line] = lines[line];
		strlcpy(lines[line++], com_token, sizeof(lines[0]));
		if(!COM_ParseToken_QuakeC(&in, false))
			return NULL;
		if(!strcmp(com_token, "}"))
			break;
		if(strcmp(com_token, ","))
			return NULL;
		if(line >= sizeof(tokens) / sizeof(tokens[0]))
			return NULL;
	}

	return tokens;
}

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_3to4(const unsigned char *in, unsigned char *out, int bytes)
{
	unsigned char i0 = (bytes > 0) ? in[0] : 0;
	unsigned char i1 = (bytes > 1) ? in[1] : 0;
	unsigned char i2 = (bytes > 2) ? in[2] : 0;
	unsigned char o0 = base64[i0 >> 2];
	unsigned char o1 = base64[((i0 << 4) | (i1 >> 4)) & 077];
	unsigned char o2 = base64[((i1 << 2) | (i2 >> 6)) & 077];
	unsigned char o3 = base64[i2 & 077];
	out[0] = (bytes > 0) ? o0 : '?';
	out[1] = (bytes > 0) ? o1 : '?';
	out[2] = (bytes > 1) ? o2 : '=';
	out[3] = (bytes > 2) ? o3 : '=';
}

size_t base64_encode(unsigned char *buf, size_t buflen, size_t outbuflen)
{
	size_t blocks, i;
	// expand the out-buffer
	blocks = (buflen + 2) / 3;
	if(blocks*4 > outbuflen)
		return 0;
	for(i = blocks; i > 0; )
	{
		--i;
		base64_3to4(buf + 3*i, buf + 4*i, buflen - 3*i);
	}
	return blocks * 4;
}
