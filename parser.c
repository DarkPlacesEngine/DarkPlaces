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
#include "parser.h"

jmp_buf parse_error;

// Tell the user that their stuff is broken, why it's broken, and where it's broken, so hopefully they fix it.
void Parse_Error(struct qparser_state_s *state, qparser_err_t error)
{
	if(!error)
		return;
	else
	{ 
		switch (error)
		{
		case PARSE_ERR_INVAL:
			Con_Printf(CON_ERROR "Parse Error: %s: Unexpected token '%c', line %i, column %i\n", state->name, *state->pos, state->line, state->col);
			break;
		case PARSE_ERR_EOF:
			Con_Printf(CON_ERROR "Parse Error: %s: Unexpected end-of-file\n", state->name);
			break;
		default:
			return;
		}
	}

	longjmp(parse_error, 1);
}

// Advance forward in the stream as many times as 'count', cleanly.
void Parse_Next(struct qparser_state_s *state, size_t count)
{
	state->col += count;
	state->pos += count;

	if(!*state->pos)
		Parse_Error(state, PARSE_ERR_EOF);
}

// Skips newlines, and handles different line endings.
static qbool Parse_Newline(struct qparser_state_s *state)
{
	if(*state->pos == '\n')
		goto newline;
	if(*state->pos == '\r')
	{
		if(*state->pos + 1 == '\n')
			state->pos++;
		goto newline;
	}
	return false;
newline:
	state->col = 1;
	state->line++;
	state->pos++;
	return true;
}

// Skip all whitespace, as we normally know it.
static inline void Parse_Whitespace(struct qparser_state_s *state)
{
	// TODO: Some languages enforce indentation style. Add a callback to override this.
	while(*state->pos == ' ' || *state->pos == '\t')
		Parse_Next(state, 1);
}

// Skips the current line. Only useful for comments.
static inline void Parse_SkipLine(struct qparser_state_s *state)
{
	while(!Parse_Newline(state))
		Parse_Next(state, 1);
}

static inline qbool Parse_Skip_Comments(struct qparser_state_s *state)
{
	// Make sure these are both defined (or both not defined)
	if((state->callback.CheckComment_Multiline_Start != NULL) ^ (state->callback.CheckComment_Multiline_End != NULL))
		Sys_Error("Parse_Skip_Comments: CheckComment_Multiline_Start (or _End) == NULL");

	// Assume language doesn't support the respective comment types if one of these are NULL.
	if(state->callback.CheckComment_SingleLine && state->callback.CheckComment_SingleLine(state))
		Parse_SkipLine(state);
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
static inline void Parse_Skip(struct qparser_state_s *state)
{
	/*
	 * Repeat this until we run out of whitespace, newlines, and comments.
	 * state->pos should be left on non-whitespace when this returns.
	 */
	do {
		Parse_Whitespace(state);
	} while (Parse_Skip_Comments(state) || Parse_Newline(state));
}

// Skip to the next token that isn't whitespace. Hopefully a valid one.
char Parse_NextToken(struct qparser_state_s *state)
{
	/*
	 * This assumes state->pos is already on whitespace. Most of the time this
	 * doesn't happen automatically, but advancing the pointer here would break
	 * comment and newline handling when it does happen automatically.
	 */
	Parse_Skip(state);
	return *state->pos;
}
