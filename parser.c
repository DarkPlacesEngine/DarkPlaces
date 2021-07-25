/*
Copyright (C) 2021 David Knapp (Cloudwalk)

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

#include "darkplaces.h"
#include "token.h"
#include "parser.h"

jmp_buf parse_error;

// Tell the user that their stuff is broken, why it's broken, and where it's broken, so hopefully they fix it.
DP_FUNC_NORETURN void Parse_Error(struct qparser_state_s *state, qparser_err_t error, const char *expected)
{
	switch (error)
	{
	case PARSE_ERR_INVAL:
		if(!expected)
			Sys_Error("Parse_Error: Expected to expect something (expected == NULL)! Your parser is broken.");
		Con_Printf(CON_ERROR "Parse Error: %s: Unexpected token '%c', expected %s. Line %i, column %i\n", state->name, *state->pos, expected, state->line, state->col);
		break;
	case PARSE_ERR_EOF:
		if(expected)
			Sys_Error("Parse_Error: expected != NULL when it should be NULL. Your parser is broken.");
		Con_Printf(CON_ERROR "Parse Error: %s: Unexpected end-of-file\n", state->name);
		break;
	case PARSE_ERR_DEPTH:
		Con_Printf(CON_ERROR "Parse Error: %s: This file is nested too deep. Max depth of %i reached.\n", state->name, PARSER_MAX_DEPTH);
		break;
	default:
		Sys_Error("Parse_Error: Invalid error number %i. Your parser is broken.", error);
	}

	longjmp(parse_error, 1);
}

// Advance forward in the stream as many times as 'count', cleanly.
void Parse_Next(struct qparser_state_s *state, int count)
{
	const char *next = Token_Next(state->pos, count);

	if(next)
	{
		state->pos = next;
		state->col += count;
	}
	else
		Parse_Error(state, PARSE_ERR_EOF, NULL);
}

// Skips newlines, and handles different line endings.
static qbool Parse_Newline(struct qparser_state_s *state)
{
	if(Token_Newline(&state->pos))
	{
		state->col = 1;
		state->line++;
		return true;
	}
	return false;
}

// Skip all whitespace, as we normally know it.
static inline qbool Parse_Skip_Whitespace(struct qparser_state_s *state)
{
	qbool ret = false;
	// TODO: Some languages enforce indentation style. Add a callback to override this.
	while(*state->pos == ' ' || *state->pos == '\t')
	{
		Parse_Next(state, 1);
		ret = true;
	}
	return ret;
}

// Skips the current line. Only useful for comments.
static inline void Parse_Skip_Line(struct qparser_state_s *state)
{
	while(!Parse_Newline(state))
		Parse_Next(state, 1);
}

static inline qbool Parse_Skip_Comments(struct qparser_state_s *state)
{
	// Make sure these are both defined (or both not defined)
	if((state->callback.CheckComment_Multiline_Start != NULL) ^ (state->callback.CheckComment_Multiline_End != NULL))
		Sys_Error("Parse_Skip_Comments: CheckComment_Multiline_Start (or _End) == NULL. Your parser is broken.");

	// Assume language doesn't support the respective comment types if one of these are NULL.
	if(state->callback.CheckComment_SingleLine && state->callback.CheckComment_SingleLine(state))
		Parse_Skip_Line(state);
	else if(state->callback.CheckComment_Multiline_Start && state->callback.CheckComment_Multiline_Start(state))
	{
		do
		{
			Parse_Next(state, 1);
			Parse_Newline(state);
		} while (state->callback.CheckComment_Multiline_End(state));
	}
	else
		return false;

	return true;
}

// Skip all whitespace.
static inline void Parse_SkipToToken(struct qparser_state_s *state)
{
	/*
	 * Repeat this until we run out of whitespace, newlines, and comments.
	 * state->pos should be left on non-whitespace when this returns.
	 */
	while(Parse_Skip_Comments(state) || Parse_Skip_Whitespace(state) || Parse_Newline(state));
}

// Skip to the next token. Advance the pointer at least 1 if we're not sitting on whitespace.
char Parse_NextToken(struct qparser_state_s *state, int skip)
{
	if(!state->pos)
		state->pos = state->buf;
	else
		Parse_Next(state, 1 + skip);

	Parse_SkipToToken(state);

	return *state->pos;
}

qparser_state_t *Parse_New(const unsigned char *in)
{
	qparser_state_t *out;

	if(!in)
	{
		Con_Printf("Parse_New: FS_LoadFile() failed");
		return NULL;
	}

	out = (qparser_state_t *)Z_Malloc(sizeof(qparser_state_t));

	out->buf = (const char *)in;
	out->pos = NULL;
	out->line = 1;
	out->col = 1;
	out->depth = 0;

	return out;
}

qparser_state_t *Parse_LoadFile(const char *file)
{
	return Parse_New(FS_LoadFile(file, tempmempool, false, NULL));
}
