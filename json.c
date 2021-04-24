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

typedef struct qjson_state_s
{
	qjson_token_t *head, *cur;
	qparser_state_t state;
} qjson_state_t;

static void Json_Parse_Object(struct qjson_state_s *state);
static void Json_Parse_Array(struct qjson_state_s *state);

// Checks for C/C++-style comments and ignores them. This is not standard json.
static qbool Json_Parse_Comment_SingleLine(struct qparser_state_s *state)
{
	if(*state->pos == '/')
	{
		// FIXME: Let the parser interface increment this?
		if(*state->pos++ == '/')
			return true;
		else
			Parse_Error(state, PARSE_ERR_INVAL);
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
			Parse_Error(state, PARSE_ERR_INVAL);
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


// TODO: handle escape sequences
static void Json_Parse_String(struct qjson_state_s *json)
{
	do {
		Parse_Next(&json->state, 1);
		if(*json->state.pos == '\\')
		{
			Parse_Next(&json->state, 1);
			continue;
		}
	} while(*json->state.pos != '"');

	Parse_Next(&json->state, 1);
}

// Handles numbers. Json numbers can be either an integer or a double.
static qbool Json_Parse_Number(struct qjson_state_s *json)
{
	int i, numsize;
	const char *in = json->state.pos;
	//char out[128];
	qbool is_float = false;
	qbool is_exp = false;

	for(i = 0, numsize = 0; isdigit(in[i]); i++, numsize++)
	{
		//out[numsize] = in[numsize];

		if(in[i] == '.')
		{
			if(is_float || is_exp)
				Parse_Error(&json->state, PARSE_ERR_INVAL);
			is_float = true;
			i++;
			continue;
		}

		if(in[i] == 'e' || in[i] == 'E')
		{
			if(is_exp)
				Parse_Error(&json->state, PARSE_ERR_INVAL);
			if(in[i+1] == '+' || in[i+1] == '-')
				i++;
			is_exp = true;
			i++;
			continue;
		}
	}
	// TODO: use strtod()
	Parse_Next(&json->state, i);
	return true;
}

// Parse a keyword.
static qbool Json_Parse_Keyword(struct qjson_state_s *json, const char *keyword)
{
	size_t keyword_size = strlen(keyword);
	if(!strncmp(keyword, json->state.pos, keyword_size))
	{
		Parse_Next(&json->state, keyword_size);
		return true;
	}
	return false;
}

// Parse a value.
static void Json_Parse_Value(struct qjson_state_s *json)
{
	Parse_Next(&json->state, 1);

	switch(Parse_NextToken(&json->state))
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
	default:
		if(Json_Parse_Keyword(json, "true"))
			break;
		if(Json_Parse_Keyword(json, "false"))
			break;
		if(Json_Parse_Keyword(json, "null"))
			break;
		if(isdigit(*json->state.pos))
			Json_Parse_Number(json);
	}
}

// Parse an object.
static void Json_Parse_Object(struct qjson_state_s *json)
{
	/*
	 * Json objects are basically a data map; key-value pairs.
	 * They end in a comma or a closing curly brace.
	 */
	do {
		Parse_Next(&json->state, 1);

		// Parse the key
		if(Parse_NextToken(&json->state) == '"')
			Json_Parse_String(json);
		else
			goto fail;
		
		// And its value
		if(Parse_NextToken(&json->state) == ':')
			Json_Parse_Value(json);
		else
			goto fail;
	} while (Parse_NextToken(&json->state) == ',');

	if(Parse_NextToken(&json->state) == '}')
		return;
fail:
	Parse_Error(&json->state, PARSE_ERR_INVAL);
}

// Parse an array.
static void Json_Parse_Array(struct qjson_state_s *json)
{
	/*
	 * Json arrays are basically lists. They can contain
	 * any value, comma-separated, and end with a closing square bracket.
	 */
	do {
		Json_Parse_Value(json);
	} while (Parse_NextToken(&json->state) == ',');

	if(Parse_NextToken(&json->state) == ']')
		return;
	else
		Parse_Error(&json->state, PARSE_ERR_INVAL);
}

// Main function for the parser.
qjson_token_t *Json_Parse(const char *data)
{
	struct qjson_state_s json =
	{
		.head = NULL,
		.cur = NULL,
		.state =
		{
			.name = "json",
			.buf = data,
			.pos = &data[0],
			.line = 1,
			.col = 1,
			.callback =
			{
				.CheckComment_SingleLine = Json_Parse_Comment_SingleLine,
				.CheckComment_Multiline_Start = Json_Parse_CheckComment_Multiline_Start,
				.CheckComment_Multiline_End = Json_Parse_CheckComment_Multiline_End
			}
		}
	};

	if(data == NULL)
	{
		Con_Printf(CON_ERROR "Json_Parse: Empty json file\n");
		return NULL;
	}

	if(setjmp(parse_error))
	{
		// actually not sure about this
		return NULL;
	}

	if(Parse_NextToken(&(json.state)) == '{')
		Json_Parse_Object(&json);
	else
	{
		Con_Printf(CON_ERROR "Json_Parse: Not a json file\n");
		return NULL;
	}

	// Success!
	// TODO: Actually parse.
	Con_Printf("Hmm, yes. This json is made of json\n");

	return NULL;
}

void Json_Test_f(cmd_state_t *cmd)
{
	Json_Parse(json_test_string);
}
