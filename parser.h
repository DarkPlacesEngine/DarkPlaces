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

#include "qtypes.h"
#include <setjmp.h>

#define PARSER_MAX_DEPTH 256

typedef enum qparser_err_e
{
	PARSE_ERR_SUCCESS = 0,
	PARSE_ERR_INVAL = 1,
	PARSE_ERR_EOF = 2,
	PARSE_ERR_DEPTH = 3,
	PARSE_ERR_EMPTY = 4
} qparser_err_t;

typedef struct qparser_state_s
{
	const char *name;
	const char *buf;
	const char *pos;
	int line, col, depth;

	struct
	{
		qbool (*CheckComment_SingleLine)(struct qparser_state_s *);
		qbool (*CheckComment_Multiline_Start)(struct qparser_state_s *);
		qbool (*CheckComment_Multiline_End)(struct qparser_state_s *);
	} callback;
} qparser_state_t;

extern jmp_buf parse_error;

void Parse_Error(struct qparser_state_s *state, qparser_err_t error, const char *expected);
void Parse_Next(struct qparser_state_s *state, int count);
char Parse_NextToken(struct qparser_state_s *state, int skip);
qparser_state_t *Parse_New(const unsigned char *in);
qparser_state_t *Parse_LoadFile(const char *file);

static inline void Parse_Indent(struct qparser_state_s *state)
{
	if(state->depth >= PARSER_MAX_DEPTH)
		Parse_Error(state, PARSE_ERR_DEPTH, NULL);
	state->depth++;
}

static inline void Parse_Dedent(struct qparser_state_s *state)
{
	state->depth--;
}