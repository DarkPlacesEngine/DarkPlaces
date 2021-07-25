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
	JSON_TYPE_USTRING = 4,
	JSON_TYPE_NUMBER = 5,
	JSON_TYPE_BOOL = 6,
	JSON_TYPE_NULL = 7
} qjson_type_t;

typedef struct qjson_token_s
{
	qjson_type_t type;

	struct qjson_token_s *parent;

	llist_t list; // Array elements or key-value pairs
	llist_t clist; // Head of list for children key-value pairs

	char *key;
	char *string;

	union
	{
		double decimal;
		int number;
	};
} qjson_token_t;

static inline qjson_token_t *Json_Parse_Object(struct qparser_state_s *state, qjson_token_t *, qjson_token_t *);
static inline qjson_token_t *Json_Parse_Array(struct qparser_state_s *state, qjson_token_t *, qjson_token_t *);
static inline qjson_token_t *Json_Parse_Terminator(struct qparser_state_s *state, qjson_token_t *, qjson_token_t *);

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

static inline qjson_token_t *Json_Parse_NewToken(qparser_state_t *json, qjson_token_t *parent)
{
	qjson_token_t *token;
	token = (qjson_token_t *)Z_Malloc(sizeof(qjson_token_t));
	if(parent)
		List_Add_Tail(&token->list, &parent->clist);
	token->parent = parent;
	return token;
}

static inline char Json_Parse_String_Escape(qparser_state_t *json, char escape)
{
	switch(escape)
	{
	case '"':
	case '\\':
	case '/':
		// These can be returned literally
		return escape;
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	default:
		Parse_Error(json, PARSE_ERR_INVAL, "a valid escape sequence");
		return 0;
	}
}

static inline qjson_token_t *Json_Parse_String(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	int i;
	const char *start, *end;
	size_t subtract = 0;

	Parse_Next(json, 1);

	start = json->pos;

	// Get the length
	while(*json->pos != '"')
	{
		if(*json->pos == '\\')
		{
			subtract++;
			if(json->pos[1] == 'u')
			{
				Parse_Error(json, PARSE_ERR_INVAL, "Json Unicode escapes (\\u) are not supported");
				return NULL;
			}
			Parse_Next(json, 1);
		}
		Parse_Next(json, 1);
	}

	end = json->pos;

	if(start != end)
	{
		token->string = (char *)Z_Malloc(((end - start) - subtract));

		// Actually copy stuff over. 'i' should never exceed end - start.
		for(i = 0; start != end; i++, start++)
		{
			if(*start == '\\')
			{
				start++;
				token->string[i] = Json_Parse_String_Escape(json, *start);
				continue;
			}
			token->string[i] = *start;
		}
	}

	token->type = JSON_TYPE_STRING;

	return Json_Parse_Terminator(json, parent, NULL);
}

// Handles numbers. Json numbers can be either an integer or a double.
static inline qjson_token_t *Json_Parse_Number(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	const char *lookahead = json->pos;

	// First, figure out where the cursor should end up after atof.
	// We don't care if the number is valid right now. atof takes care of that.
	while(isdigit(*lookahead) || *lookahead == '-' || *lookahead == '+' || *lookahead == 'E' || *lookahead == 'e' || *lookahead == '.')
		lookahead++;

	token->type = JSON_TYPE_NUMBER;
	token->decimal = atof(json->pos);

	if(!token->decimal)
	{
		Parse_Error(json, PARSE_ERR_INVAL, "a valid number");
		return NULL;
	}

	Parse_Next(json, (lookahead - json->pos) - 1);

	return Json_Parse_Terminator(json, parent, NULL);
}

static const char *keyword_list[] =
{
	"false",
	"true",
	"null",
	NULL
};

// Parse a keyword.
static inline qjson_token_t *Json_Parse_Keyword(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	size_t keyword_size;

	for (int i = 0; keyword_list[i]; i++)
	{
		keyword_size = strlen(keyword_list[i]);

		if(!strncmp(keyword_list[i], json->pos, keyword_size))
		{
			// Don't advance the entire length of the keyword or we might run into a valid token that'd go missed.
			Parse_Next(json, keyword_size - 1);
			if(i == 2)
				token->type = JSON_TYPE_NULL;
			else
			{
				token->type = JSON_TYPE_BOOL;
				token->number = i;
			}

			return Json_Parse_Terminator(json, parent, NULL);
		}
	}
	Parse_Error(json, PARSE_ERR_INVAL, "true, false, or null");
	return NULL;
}

static inline qjson_token_t *Json_Parse_Value(qparser_state_t *json, qjson_token_t *parent, qjson_token_t *token)
{
	switch(Parse_NextToken(json, 0))
	{
	case '"': // string
		return Json_Parse_String(json, parent, token);
	case '{': // object
		return Json_Parse_Object(json, parent, token);
	case '[': // array
		return Json_Parse_Array(json, parent, token);
	case '-':
		return Json_Parse_Number(json, parent, token);
	case 't': // true
	case 'f': // false
	case 'n': // null
		return Json_Parse_Keyword(json, parent, token);
	default:
		if(isdigit(*json->pos))
			return Json_Parse_Number(json, parent, token);
	}
	Parse_Error(json, PARSE_ERR_INVAL, "a value");
	return NULL;
}

static inline qjson_token_t *Json_Parse_Single(qparser_state_t *json, qjson_token_t *parent, qjson_token_t *token)
{
	// TODO: Handle blank arrays

	token = Json_Parse_NewToken(json, parent);
	return Json_Parse_Value(json, parent, token);
}

static inline qjson_token_t *Json_Parse_Pair(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	const char *start;
	size_t length = 0;

	Parse_NextToken(json, 0);

	// TODO: Handle blank objects

	start = &json->pos[1];

	while(json->pos[1] != '"')
	{
		Parse_Next(json, 1);
		if(ISWHITESPACE(*json->pos))
		{
			Parse_Error(json, PARSE_ERR_INVAL, "a key without whitespace");
			return NULL;
		}
		length++;
	}

	if(!length)
	{
		Parse_Error(json, PARSE_ERR_INVAL, "a key");
		return NULL;
	}

	if(Parse_NextToken(json, 1) != ':')
	{
		Parse_Error(json, PARSE_ERR_INVAL, "':'");
		return NULL;
	}

	token = Json_Parse_NewToken(json, parent);
	token->key = (char *)Z_Malloc(length + 1);
	memcpy(token->key, start, length);

	return Json_Parse_Value(json, parent, token);
}

static inline qjson_token_t *Json_Parse_Terminator(qparser_state_t *json, qjson_token_t *parent, qjson_token_t *token)
{
	switch(Parse_NextToken(json, 0))
	{
	case ']':
	case '}':
		if(!parent->parent)
			return parent;
		return Json_Parse_Terminator(json, parent->parent, NULL);
	case ',':
		if(parent->type == JSON_TYPE_ARRAY)
			return Json_Parse_Single(json, parent, NULL);
		else
			return Json_Parse_Pair(json, parent, NULL);
	default:
		Parse_Error(json, PARSE_ERR_INVAL, "']', '}', or ','");
		return NULL;
	}
}

// Parse an object.
static inline qjson_token_t *Json_Parse_Object(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	//Parse_Indent(json);

	/*
	 * Json objects are basically a data map; key-value pairs.
	 * They end in a comma or a closing curly brace.
	 */
	token->type = JSON_TYPE_OBJECT;
	List_Create(&token->clist);

	return Json_Parse_Pair(json, token, NULL);
}

// Parse an array.
static inline qjson_token_t *Json_Parse_Array(struct qparser_state_s *json, qjson_token_t *parent, qjson_token_t *token)
{
	//Parse_Indent(json);

	/*
	 * Json arrays are basically lists. They can contain
	 * any value, comma-separated, and end with a closing square bracket.
	 */
	token->type = JSON_TYPE_ARRAY;
	List_Create(&token->clist);

	return Json_Parse_Single(json, token, NULL);
}

static void Json_Parse_Cleanup(qparser_state_t *json, qjson_token_t *parent, qjson_token_t *token)
{
	qjson_token_t *cur, *next;

	token->type = JSON_TYPE_UNDEFINED;

	List_For_Each_Entry_Safe(cur, next, &token->clist, list)
	{
		if(cur->type == JSON_TYPE_ARRAY || cur->type == JSON_TYPE_BOOL)
		{
			Json_Parse_Cleanup(json, token, cur);
			return;
		}
		List_Delete(&cur->list);
		if(cur->key)
			Z_Free(cur->key);
		if(cur->string)
			Z_Free(cur->string);
	}

	if(parent)
		Json_Parse_Cleanup(json, parent->parent, parent);
	else
	{
		if(token->key)
			Z_Free(token->key);
		Z_Free(token);
	}
}

// Main function for the parser.
static qjson_token_t *Json_Parse_Start(qparser_state_t *json)
{
	qjson_token_t *tree = NULL;
	qjson_token_t *head = NULL;

	json->callback.CheckComment_SingleLine = Json_Parse_Comment_SingleLine;
	json->callback.CheckComment_Multiline_Start = Json_Parse_CheckComment_Multiline_Start;
	json->callback.CheckComment_Multiline_End = Json_Parse_CheckComment_Multiline_End;

	if(json->buf == NULL)
	{
		Con_Printf(CON_ERROR "Json_Parse: Empty json file\n");
		return NULL;
	}

	if(setjmp(parse_error))
	{
		// actually not sure about this
		Json_Parse_Cleanup(json, NULL, head);
		Z_Free(json);
		return NULL;
	}

	head = Json_Parse_NewToken(json, NULL);

	switch(Parse_NextToken(json, 0))
	{
	case '{':
		tree = Json_Parse_Object(json, NULL, head);
		break;
	case '[':
		tree = Json_Parse_Array(json, NULL, head);
		break;
	default:
		Con_Printf(CON_ERROR "Json_Parse: Not a json file\n");
		break;
	}

	Z_Free(json);
	return tree;
}

qjson_token_t *Json_Parse_File(const char *file)
{
	return Json_Parse_Start(Parse_LoadFile(file));
}

qjson_token_t *Json_Parse(const unsigned char *data)
{
	return Json_Parse_Start(Parse_New(data));
}

void Json_Test_f(cmd_state_t *cmd)
{
	qjson_token_t *testing = Json_Parse_File("test.json");
	if(testing)
		Con_Printf("hmm yes this json here is made out of json\n");
	else
		Con_Printf("failure\n");
}
