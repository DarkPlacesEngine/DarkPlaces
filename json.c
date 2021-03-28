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

#include "quakedef.h"
#include <setjmp.h>

// taken from json's wikipedia article
const char json_test_string[] =
{
	"{\n"
	"\t\"firstName\": \"John\",\n"
	"\t\"lastName\": \"Smith\",\n"
	"\t\"isAlive\": true,\n"
	"\t\"age\": 27,\n"
	"\t\"address\": {\n"
	"\t\t\"streetAddress\": \"21 2nd Street\",\n"
	"\t\t\"city\": \"New York\",\n"
	"\t\t\"state\": \"NY\",\n"
	"\t\t\"postalCode\": \"10021-3100\"\n"
	"\t},\n"
	"\t\"phoneNumbers\": [\n"
	"\t\t{\n"
	"\t\t\t\"type\": \"home\",\n"
	"\t\t\t\"number\": \"212 555-1234\"\n"
	"\t\t},\n"
	"\t\t{\n"
	"\t\t\t\"type\": \"office\",\n"
	"\t\t\t\"number\": \"646 555-4567\"\n"
	"\t\t}\n"
	"\t],\n"
	"\t\"children\": [],\n"
	"\t\"spouse\": null\n"
	"}\n\000"
};

static jmp_buf json_error;

typedef enum qjson_err_e
{
	JSON_ERR_SUCCESS = 0,
	JSON_ERR_INVAL = 1,
	JSON_ERR_EOF = 2,
	JSON_ERR_EMPTY = 3
} qjson_err_t;

typedef enum qjson_type_e
{
	JSON_TYPE_UNDEFINED = 0,
	JSON_TYPE_OBJECT = 1,
	JSON_TYPE_ARRAY = 2,
	JSON_TYPE_STRING = 3,
	JSON_TYPE_PRIMITIVE = 4
} qjson_type_t;

typedef struct qjson_token_s
{
	qjson_type_t type;
	struct qjson_token_s *next; // if an array, next will be a NULL terminated array
	char *string; // ASCII only for now
} qjson_token_t;

struct qjson_state_s
{
	qjson_token_t *head, *cur;
	const char *buf;
	const char *pos;
	int line, col;
};

static void Json_Parse_Object(struct qjson_state_s *state);
static void Json_Parse_Array(struct qjson_state_s *state);

// Tell the user that their json is broken, why it's broken, and where it's broken, so hopefully they fix it.
static void Json_Parse_Error(struct qjson_state_s *state, qjson_err_t error)
{
	if(!error)
		return;
	else
	{ 
		switch (error)
		{
		case JSON_ERR_INVAL:
			Con_Printf(CON_ERROR "Json Error: Unexpected token '%c', line %i, column %i\n", *state->pos, state->line, state->col);
			break;
		case JSON_ERR_EOF:
			Con_Printf(CON_ERROR "Json Error: Unexpected end-of-file\n");
			break;
		default:
			return;
		}
	}
	longjmp(json_error, 1);
}

// Skips newlines, and handles different line endings.
static qbool Json_Parse_Newline(struct qjson_state_s *state)
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

// Skips the current line. Only useful for comments.
static void Json_Parse_SkipLine(struct qjson_state_s *state)
{
	while(!Json_Parse_Newline(state))
		state->pos++;
}

// Checks for C/C++-style comments and ignores them. This is not standard json.
static qbool Json_Parse_Comment(struct qjson_state_s *state)
{
	if(*state->pos == '/')
	{
		if(*state->pos++ == '/')
			Json_Parse_SkipLine(state);
		else if(*state->pos == '*')
		{
			while(*state->pos++ != '*' && *state->pos + 1 != '/')
				continue;
		}
		else
			Json_Parse_Error(state, JSON_ERR_INVAL);
		return true;
	}
	return false;
}

// Advance forward in the stream as many times as 'count', cleanly.
static void Json_Parse_Next(struct qjson_state_s *state, size_t count)
{
	state->col = state->col + count;
	state->pos = state->pos + count;

	if(!*state->pos)
		Json_Parse_Error(state, JSON_ERR_EOF);
}

// Skip all whitespace, as we normally know it.
static void Json_Parse_Whitespace(struct qjson_state_s *state)
{
	while(*state->pos == ' ' || *state->pos == '\t')
		Json_Parse_Next(state, 1);
}

// Skip all whitespace, as json defines it.
static void Json_Parse_Skip(struct qjson_state_s *state)
{
	/*
	 * Repeat this until we run out of whitespace, newlines, and comments.
	 * state->pos should be left on non-whitespace when this returns.
	 */
	do {
		Json_Parse_Whitespace(state);
	} while (Json_Parse_Comment(state) || Json_Parse_Newline(state));
}

// Skip to the next token that isn't whitespace. Hopefully a valid one.
static char Json_Parse_NextToken(struct qjson_state_s *state)
{
	/*
	 * This assumes state->pos is already on whitespace. Most of the time this
	 * doesn't happen automatically, but advancing the pointer here would break
	 * comment and newline handling when it does happen automatically.
	 */
	Json_Parse_Skip(state);
	return *state->pos;
}

// TODO: handle escape sequences
static void Json_Parse_String(struct qjson_state_s *state)
{
	do {
		Json_Parse_Next(state, 1);
		if(*state->pos == '\\')
			Json_Parse_Next(state, 1);
	} while(*state->pos != '"');

	Json_Parse_Next(state, 1);
}

// Handles numbers. Json numbers can be either an integer or a double.
static qbool Json_Parse_Number(struct qjson_state_s *state)
{
	int i, numsize;
	const char *in = state->pos;
	//char out[128];
	qbool is_float = false;
	qbool is_exp = false;

	for(i = 0, numsize = 0; isdigit(in[i]); i++, numsize++)
	{
		//out[numsize] = in[numsize];

		if(in[i] == '.')
		{
			if(is_float || is_exp)
				Json_Parse_Error(state, JSON_ERR_INVAL);
			is_float = true;
			i++;
			continue;
		}

		if(in[i] == 'e' || in[i] == 'E')
		{
			if(is_exp)
				Json_Parse_Error(state, JSON_ERR_INVAL);
			if(in[i+1] == '+' || in[i+1] == '-')
				i++;
			is_exp = true;
			i++;
			continue;
		}
	}
	// TODO: use strtod()
	Json_Parse_Next(state, i);
	return true;
}

// Parse a keyword.
static qbool Json_Parse_Keyword(struct qjson_state_s *state, const char *keyword)
{
	size_t keyword_size = strlen(keyword);
	if(!strncmp(keyword, state->pos, keyword_size))
	{
		Json_Parse_Next(state, keyword_size);
		return true;
	}
	return false;
}

// Parse a value.
static void Json_Parse_Value(struct qjson_state_s *state)
{
	Json_Parse_Next(state, 1);

	switch(Json_Parse_NextToken(state))
	{
	case '"': // string
		Json_Parse_String(state);
		break;
	case '{': // object
		Json_Parse_Object(state);
		break;
	case '[': // array
		Json_Parse_Array(state);
		break;
	case '-':
		Json_Parse_Number(state);
		break;
	default:
		if(Json_Parse_Keyword(state, "true"))
			break;
		if(Json_Parse_Keyword(state, "false"))
			break;
		if(Json_Parse_Keyword(state, "null"))
			break;
		if(isdigit(*state->pos))
			Json_Parse_Number(state);
	}
}

// Parse an object.
static void Json_Parse_Object(struct qjson_state_s *state)
{
	/*
	 * Json objects are basically a data map; key-value pairs.
	 * They end in a comma or a closing curly brace.
	 */
	do {
		Json_Parse_Next(state, 1);

		// Parse the key
		if(Json_Parse_NextToken(state) == '"')
			Json_Parse_String(state);
		else
			goto fail;
		
		// And its value
		if(Json_Parse_NextToken(state) == ':')
			Json_Parse_Value(state);
		else
			goto fail;
	} while (Json_Parse_NextToken(state) == ',');

	if(Json_Parse_NextToken(state) == '}')
		return;
fail:
	Json_Parse_Error(state, JSON_ERR_INVAL);
}

// Parse an array.
static void Json_Parse_Array(struct qjson_state_s *state)
{
	/*
	 * Json arrays are basically lists. They can contain
	 * any value, comma-separated, and end with a closing square bracket.
	 */
	do {
		Json_Parse_Value(state);
	} while (Json_Parse_NextToken(state) == ',');

	if(Json_Parse_NextToken(state) == ']')
		return;
	else
		Json_Parse_Error(state, JSON_ERR_INVAL);
}

// Main function for the parser.
qjson_token_t *Json_Parse(const char *data)
{
	struct qjson_state_s state =
	{
		.head = NULL,
		.buf = data,
		.pos = &data[0],
		.line = 1,
		.col = 1
	};

	if(data == NULL)
	{
		Con_Printf(CON_ERROR "Json_Parse: Empty json file\n");
		return NULL;
	}

	if(setjmp(json_error))
	{
		// actually not sure about this
		return NULL;
	}

	if(Json_Parse_NextToken(&state) == '{')
		Json_Parse_Object(&state);
	else
	{
		Con_Printf(CON_ERROR "Json_Parse: Not a json file\n");
		return NULL;
	}

	// Success!
	// TODO: Actually parse.
	Con_Printf("Hmm, yes. This json is made of json\n");

	return state.head;
}

void Json_Test_f(cmd_state_t *cmd)
{
	Json_Parse(json_test_string);
}
