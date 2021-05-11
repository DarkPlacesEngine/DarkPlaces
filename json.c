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

typedef enum qjson_type_e
{
	JSON_TYPE_UNDEFINED = 0,
	JSON_TYPE_OBJECT = 1,
	JSON_TYPE_ARRAY = 2,
	JSON_TYPE_STRING = 3,
	JSON_TYPE_NUMBER = 4,
	JSON_TYPE_BOOL = 5
} qjson_type_t;

typedef struct qjson_token_s
{
	qjson_type_t type;

	struct qjson_token_s *prev, *next; // Linked list for arrays
	struct qjson_token_s *parent, *child;
	
	char *key;
	union
	{
		char *string;
		double decimal;
		int number;
	} value;
} qjson_token_t;

typedef struct qjson_state_s
{
	qjson_token_t *head, *cur;
	qparser_state_t *state;
} qjson_state_t;

static inline void Json_Parse_Object(struct qjson_state_s *state);
static inline void Json_Parse_Array(struct qjson_state_s *state);

// Checks for C/C++-style comments and ignores them. This is not standard json.
static qbool Json_Parse_Comment_SingleLine(struct qparser_state_s *state)
{
	if(*state->pos == '/')
	{
		// FIXME: Let the parser interface increment this?
		if(*state->pos++ == '/')
			return true;
		else
			Parse_Error(state, PARSE_ERR_INVAL, "// or /*");
	}
	return false;
}

static qbool Json_Parse_CheckComment_Multiline_Start(struct qparser_state_s *state)
{
	if(*state->pos == '/')
	{
		// FIXME: Let the parser interface increment this?
		if(*state->pos++ == '*')
			return true;
		else
			Parse_Error(state, PARSE_ERR_INVAL, "// or /*");
	}
	return false;
}

static qbool Json_Parse_CheckComment_Multiline_End(struct qparser_state_s *state)
{
	if(*state->pos == '*')
	{
		// FIXME: Let the parser interface increment this?
		if(*state->pos++ == '/')
			return true;
	}
	return false;
}

static inline qbool Json_Handle_String_Escape(struct qjson_state_s *json)
{
	switch(*json->state->pos)
	{
	case '\\':
	case '/':
	case 'b':
	case 'f':
	case 'n':
	case 'r':
	case 't':
	case 'u':
		return true; // TODO
	default:
		return false;
	}
}

// TODO: handle escape sequences
static inline void Json_Parse_String(struct qjson_state_s *json)
{
	do {
		Parse_Next(json->state, 1);
		if(*json->state->pos == '\\')
		{
			Parse_Next(json->state, 1);
			if(Json_Handle_String_Escape(json))
				continue;
			Parse_Error(json->state, PARSE_ERR_INVAL, "a valid escape sequence");
		}
	} while(*json->state->pos != '"');
}

// Handles numbers. Json numbers can be either an integer or a double.
static inline qbool Json_Parse_Number(struct qjson_state_s *json)
{
	int i, numsize;
	const unsigned char *in = json->state->pos;
	//char out[128];
	qbool is_float = false;
	qbool is_exp = false;

	for(i = 0, numsize = 0; isdigit(in[i]); i++, numsize++)
	{
		//out[numsize] = in[numsize];

		if(in[i] == '.')
		{
			if(is_float || is_exp)
				Parse_Error(json->state, PARSE_ERR_INVAL, "a number");
			is_float = true;
			i++;
			continue;
		}

		if(in[i] == 'e' || in[i] == 'E')
		{
			if(is_exp)
				Parse_Error(json->state, PARSE_ERR_INVAL, "a number");
			if(in[i+1] == '+' || in[i+1] == '-')
				i++;
			is_exp = true;
			i++;
			continue;
		}
	}
	// TODO: use strtod()
	Parse_Next(json->state, i - 1);
	return true;
}

static const char *keyword_list[] =
{
	"true",
	"false",
	"null",
	NULL
};

// Parse a keyword.
static inline qbool Json_Parse_Keyword(struct qjson_state_s *json)
{
	size_t keyword_size;

	for (int i = 0; keyword_list[i]; i++)
	{
		keyword_size = strlen(keyword_list[i]);

		if(!strncmp(keyword_list[i], (const char *)json->state->pos, keyword_size))
		{
			// Don't advance the entire length of the keyword or we might run into a valid token that'd go missed.
			Parse_Next(json->state, keyword_size - 1);
			return true;
		}
	}
	return false;
}

static inline void Json_Parse_Key(struct qjson_state_s *json)
{
	do {
		Parse_Next(json->state, 1);
		if(ISWHITESPACE(*json->state->pos))
			Parse_Error(json->state, PARSE_ERR_INVAL, "a key");
	} while(*json->state->pos != '"');
}

// Parse a value.
static inline qbool Json_Parse_Value(struct qjson_state_s *json)
{
	switch(Parse_NextToken(json->state))
	{
	case '"': // string
		Json_Parse_String(json);
		break;
	case '{': // object
		Json_Parse_Object(json);
		break;
	case '[': // array
		Json_Parse_Array(json);
		break;
	case '-':
		Json_Parse_Number(json);
		break;
	case 't': // true
	case 'f': // false
	case 'n': // null
		Json_Parse_Keyword(json);
		break;
	default:
		if(isdigit(*json->state->pos))
		{
			Json_Parse_Number(json);
			break;
		}
		//Parse_Error(json->state, PARSE_ERR_INVAL, "a value");
		return false;
	}
	return true;
}

static inline qbool Json_Parse_Pairs(struct qjson_state_s *json)
{
	do
	{
		if(Parse_NextToken(json->state) == '"')
		{
			// Parse the key
			Json_Parse_Key(json);

			// And its value
			if(Parse_NextToken(json->state) == ':')
			{
				if(!Json_Parse_Value(json))
					Parse_Error(json->state, PARSE_ERR_INVAL, "a value");
			}
			else
				Parse_Error(json->state, PARSE_ERR_INVAL, ":");
		}
		else
			return false;
	} while (Parse_NextToken(json->state) == ',');

	return true;
}

// Parse an object.
static inline void Json_Parse_Object(struct qjson_state_s *json)
{
	Parse_Indent(json->state);

	/*
	 * Json objects are basically a data map; key-value pairs.
	 * They end in a comma or a closing curly brace.
	 */
	Json_Parse_Pairs(json);

	if(*json->state->pos != '}')
		Parse_Error(json->state, PARSE_ERR_INVAL, ", or }");

	Parse_Dedent(json->state);
}

// Parse an array.
static inline void Json_Parse_Array(struct qjson_state_s *json)
{
	Parse_Indent(json->state);

	/*
	 * Json arrays are basically lists. They can contain
	 * any value, comma-separated, and end with a closing square bracket.
	 */
	do {
		if(!Json_Parse_Value(json))
			break;
	} while (Parse_NextToken(json->state) == ',');

	if(*json->state->pos != ']')
		Parse_Error(json->state, PARSE_ERR_INVAL, ", or ]");

	Parse_Dedent(json->state);
}

// Main function for the parser.
static qjson_token_t *Json_Parse_Main(qjson_state_t *json)
{
	json->state->callback.CheckComment_SingleLine = Json_Parse_Comment_SingleLine;
	json->state->callback.CheckComment_Multiline_Start = Json_Parse_CheckComment_Multiline_Start;
	json->state->callback.CheckComment_Multiline_End = Json_Parse_CheckComment_Multiline_End;

	if(setjmp(parse_error))
	{
		// actually not sure about this
		return NULL;
	}
	if(json->state->buf == NULL)
	{
		Con_Printf(CON_ERROR "Json_Parse: Empty json file\n");
		return NULL;
	}

	switch(Parse_NextToken(json->state))
	{
	case '{':
		Json_Parse_Object(json);
		break;
	case '[':
		Json_Parse_Array(json);
		break;
	default:
		Con_Printf(CON_ERROR "Json_Parse: Not a json file\n");
		return NULL;
	}

	// Success!
	// TODO: Actually parse.
	Con_Printf("Hmm, yes. This json is made of json\n");

	return NULL;
}

qjson_token_t *Json_Parse_File(const char *file)
{
	struct qjson_state_s json =
	{
		.head = NULL,
		.cur = NULL,
		.state = Parse_LoadFile(file)
	};

	return Json_Parse_Main(&json);
}

qjson_token_t *Json_Parse(const unsigned char *data)
{
	struct qjson_state_s json =
	{
		.head = NULL,
		.cur = NULL,
		.state = Parse_New(data)
	};

	return Json_Parse_Main(&json);
}

void Json_Test_f(cmd_state_t *cmd)
{
	Json_Parse_File("test.json");
}
