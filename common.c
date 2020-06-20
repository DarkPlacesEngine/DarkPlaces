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

cvar_t registered = {CVAR_CLIENT | CVAR_SERVER, "registered","0", "indicates if this is running registered quake (whether gfx/pop.lmp was found)"};
cvar_t cmdline = {CVAR_CLIENT | CVAR_SERVER, "cmdline","0", "contains commandline the engine was launched with"};

char com_token[MAX_INPUTLINE];

gamemode_t gamemode;
const char *gamename;
const char *gamenetworkfiltername; // same as gamename currently but with _ in place of spaces so that "getservers" packets parse correctly (this also means the 
const char *gamedirname1;
const char *gamedirname2;
const char *gamescreenshotname;
const char *gameuserdirname;
char com_modname[MAX_OSPATH] = "";

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

// LadyHavoc: thanks to Fuh for bringing the pure evil of SZ_Print to my
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

	for (i=1 ; i<sys.argc ; i++)
	{
		if (!sys.argv[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,sys.argv[i]))
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
{ GAME_STEELSTORMREVENANTS,		GAME_STEELSTORMREVENANTS,	"steelstorm-revenants", "-steelstormrev",			"Steel Storm: Revenants",	"Steel_Storm_Revenants",	"base", NULL,				"ssrev",			"steelstorm-revenants"	}, // COMMANDLINEOPTION: Game: -steelstormrev runs the game Steel Storm: Revenants
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
	FS_StripExtension(FS_FileWithoutPath(sys.argv[0]), name, sizeof (name));
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
		if((t = COM_CheckParm("-customgamename")) && t + 1 < sys.argc)
			gamename = gamenetworkfiltername = sys.argv[t+1];
		if((t = COM_CheckParm("-customgamenetworkfiltername")) && t + 1 < sys.argc)
			gamenetworkfiltername = sys.argv[t+1];
		if((t = COM_CheckParm("-customgamedirname1")) && t + 1 < sys.argc)
			gamedirname1 = sys.argv[t+1];
		if((t = COM_CheckParm("-customgamedirname2")) && t + 1 < sys.argc)
			gamedirname2 = *sys.argv[t+1] ? sys.argv[t+1] : NULL;
		if((t = COM_CheckParm("-customgamescreenshotname")) && t + 1 < sys.argc)
			gamescreenshotname = sys.argv[t+1];
		if((t = COM_CheckParm("-customgameuserdirname")) && t + 1 < sys.argc)
			gameuserdirname = sys.argv[t+1];
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
	for (j = 0;(j < MAX_NUM_ARGVS) && (j < sys.argc);j++)
	{
		i = 0;
		if (strstr(sys.argv[j], " "))
		{
			// arg contains whitespace, store quotes around it
			// This condition checks whether we can allow to put
			// in two quote characters.
			if (n >= ((int)sizeof(com_cmdline) - 2))
				break;
			com_cmdline[n++] = '\"';
			// This condition checks whether we can allow one
			// more character and a quote character.
			while ((n < ((int)sizeof(com_cmdline) - 2)) && sys.argv[j][i])
				// FIXME: Doesn't quote special characters.
				com_cmdline[n++] = sys.argv[j][i++];
			com_cmdline[n++] = '\"';
		}
		else
		{
			// This condition checks whether we can allow one
			// more character.
			while ((n < ((int)sizeof(com_cmdline) - 1)) && sys.argv[j][i])
				com_cmdline[n++] = sys.argv[j][i++];
		}
		if (n < ((int)sizeof(com_cmdline) - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}
	com_cmdline[n] = 0;
	Cvar_SetQuick(&cmdline, com_cmdline);
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
					*valid = true;
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
							*valid = false;
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
#define APPEND(ch) do { if(--size_out) { *out++ = (ch); } else { *out++ = 0; return false; } } while(0)
	const char *end = size_in ? (in + size_in) : NULL;
	if(size_out < 1)
		return false;
	for(;;)
	{
		switch((in == end) ? 0 : *in)
		{
			case 0:
				*out++ = 0;
				return true;
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
						return true;
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
		if (!memcmp(buffer + pos+1, key, keylength) &&
				(buffer[pos+1 + keylength] == 0 ||
				 buffer[pos+1 + keylength] == '\\'))
		{
			pos += 1 + (int)keylength;           // Skip \key
			if (buffer[pos] == '\\') pos++; // Skip \ before value.
			for (j = 0;buffer[pos+j] && buffer[pos+j] != '\\' && j < (int)valuelength - 1;j++)
				value[j] = buffer[pos+j];
			value[j] = 0;
			return value;
		}
		if (buffer[pos] == '\\') pos++; // Skip \ before value.
		for (pos++;buffer[pos] && buffer[pos] != '\\';pos++);
		if (buffer[pos] == '\\') pos++; // Skip \ before value.
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
		if (!memcmp(buffer + pos+1, key, keylength) &&
				(buffer[pos+1 + keylength] == 0 ||
				 buffer[pos+1 + keylength] == '\\'))
			break;
		if (buffer[pos] == '\\') pos++; // Skip \ before value.
		for (;buffer[pos] && buffer[pos] != '\\';pos++);
		if (buffer[pos] == '\\') pos++; // Skip \ before value.
		for (;buffer[pos] && buffer[pos] != '\\';pos++);
	}
	// if we found the key, find the end of it because we will be replacing it
	pos2 = pos;
	if (buffer[pos] == '\\')
	{
		pos2 += 1 + (int)keylength;  // Skip \key
		if (buffer[pos2] == '\\') pos2++; // Skip \ before value.
		for (;buffer[pos2] && buffer[pos2] != '\\';pos2++);
	}
	if (bufferlength <= pos + 1 + strlen(key) + 1 + strlen(value) + strlen(buffer + pos2))
	{
		Con_Printf("InfoString_SetValue: no room for \"%s\" \"%s\" in infostring\n", key, value);
		return;
	}
	if (value[0])
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
		base64_3to4(buf + 3*i, buf + 4*i, (int)(buflen - 3*i));
	}
	return blocks * 4;
}
