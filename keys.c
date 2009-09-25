/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/

#include "quakedef.h"
#include "cl_video.h"

cvar_t con_closeontoggleconsole = {CVAR_SAVE, "con_closeontoggleconsole","1", "allows toggleconsole binds to close the console as well"};

/*
key up events are sent even if in console mode
*/

char		key_line[MAX_INPUTLINE];
int			key_linepos;
qboolean	key_insert = true;	// insert key toggle (for editing)
keydest_t	key_dest;
int			key_consoleactive;
char		*keybindings[MAX_BINDMAPS][MAX_KEYS];
int         history_line;
char		history_savedline[MAX_INPUTLINE];
conbuffer_t history;
#define HIST_TEXTSIZE 262144
#define HIST_MAXLINES 4096

extern cvar_t	con_textsize;


static void Key_History_Init(void)
{
	qfile_t *historyfile;
	ConBuffer_Init(&history, HIST_TEXTSIZE, HIST_MAXLINES, zonemempool);

	historyfile = FS_OpenRealFile("darkplaces_history.txt", "rb", false); // rb to handle unix line endings on windows too
	if(historyfile)
	{
		char buf[MAX_INPUTLINE];
		int bufpos;
		int c;

		bufpos = 0;
		for(;;)
		{
			c = FS_Getc(historyfile);
			if(c < 0 || c == 0 || c == '\r' || c == '\n')
			{
				if(bufpos > 0)
				{
					buf[bufpos] = 0;
					ConBuffer_AddLine(&history, buf, bufpos, 0);
					bufpos = 0;
				}
				if(c < 0)
					break;
			}
			else
			{
				if(bufpos < MAX_INPUTLINE - 1)
					buf[bufpos++] = c;
			}
		}

		FS_Close(historyfile);
	}

	history_line = -1;
}

static void Key_History_Shutdown(void)
{
	// TODO write history to a file

	qfile_t *historyfile = FS_OpenRealFile("darkplaces_history.txt", "w", false);
	if(historyfile)
	{
		int i;
		for(i = 0; i < CONBUFFER_LINES_COUNT(&history); ++i)
			FS_Printf(historyfile, "%s\n", ConBuffer_GetLine(&history, i));
		FS_Close(historyfile);
	}

	ConBuffer_Shutdown(&history);
}

static void Key_History_Push(void)
{
	if(key_line[1]) // empty?
	if(strcmp(key_line, "]quit")) // putting these into the history just sucks
	if(strncmp(key_line, "]quit ", 6)) // putting these into the history just sucks
		ConBuffer_AddLine(&history, key_line + 1, strlen(key_line) - 1, 0);
	Con_Printf("%s\n", key_line); // don't mark empty lines as history
	history_line = -1;
}

static void Key_History_Up(void)
{
	if(history_line == -1) // editing the "new" line
		strlcpy(history_savedline, key_line + 1, sizeof(history_savedline));

	if(history_line == -1)
	{
		history_line = CONBUFFER_LINES_COUNT(&history) - 1;
		if(history_line != -1)
		{
			strlcpy(key_line + 1, ConBuffer_GetLine(&history, history_line), sizeof(key_line) - 1);
			key_linepos = strlen(key_line);
		}
	}
	else if(history_line > 0)
	{
		--history_line; // this also does -1 -> 0, so it is good
		strlcpy(key_line + 1, ConBuffer_GetLine(&history, history_line), sizeof(key_line) - 1);
		key_linepos = strlen(key_line);
	}
}

static void Key_History_Down(void)
{
	if(history_line == -1) // editing the "new" line
		return;

	if(history_line < CONBUFFER_LINES_COUNT(&history) - 1)
	{
		++history_line;
		strlcpy(key_line + 1, ConBuffer_GetLine(&history, history_line), sizeof(key_line) - 1);
	}
	else
	{
		history_line = -1;
		strlcpy(key_line + 1, history_savedline, sizeof(key_line) - 1);
	}

	key_linepos = strlen(key_line);
}

static int	key_bmap, key_bmap2;
static unsigned char keydown[MAX_KEYS];	// 0 = up, 1 = down, 2 = repeating

typedef struct keyname_s
{
	const char	*name;
	int			keynum;
}
keyname_t;

static const keyname_t   keynames[] = {
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},

	// spacer so it lines up with keys.h

	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"PAUSE", K_PAUSE},

	{"NUMLOCK", K_NUMLOCK},
	{"CAPSLOCK", K_CAPSLOCK},
	{"SCROLLOCK", K_SCROLLOCK},

	{"KP_INS",			K_KP_INS },
	{"KP_0", K_KP_0},
	{"KP_END",			K_KP_END },
	{"KP_1", K_KP_1},
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_2", K_KP_2},
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_3", K_KP_3},
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_4", K_KP_4},
	{"KP_5", K_KP_5},
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_6", K_KP_6},
	{"KP_HOME",			K_KP_HOME },
	{"KP_7", K_KP_7},
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_8", K_KP_8},
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_9", K_KP_9},
	{"KP_DEL",			K_KP_DEL },
	{"KP_PERIOD", K_KP_PERIOD},
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_DIVIDE", K_KP_DIVIDE},
	{"KP_MULTIPLY", K_KP_MULTIPLY},
	{"KP_MINUS", K_KP_MINUS},
	{"KP_PLUS", K_KP_PLUS},
	{"KP_ENTER", K_KP_ENTER},
	{"KP_EQUALS", K_KP_EQUALS},



	{"MOUSE1", K_MOUSE1},

	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},
	{"MOUSE9", K_MOUSE9},
	{"MOUSE10", K_MOUSE10},
	{"MOUSE11", K_MOUSE11},
	{"MOUSE12", K_MOUSE12},
	{"MOUSE13", K_MOUSE13},
	{"MOUSE14", K_MOUSE14},
	{"MOUSE15", K_MOUSE15},
	{"MOUSE16", K_MOUSE16},




	{"JOY1",  K_JOY1},
	{"JOY2",  K_JOY2},
	{"JOY3",  K_JOY3},
	{"JOY4",  K_JOY4},
	{"JOY5",  K_JOY5},
	{"JOY6",  K_JOY6},
	{"JOY7",  K_JOY7},
	{"JOY8",  K_JOY8},
	{"JOY9",  K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},






	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},

	{"SEMICOLON", ';'},			// because a raw semicolon separates commands
	{"TILDE", '~'},
	{"BACKQUOTE", '`'},
	{"QUOTE", '"'},
	{"APOSTROPHE", '\''},
	{"BACKSLASH", '\\'},		// because a raw backslash is used for special characters

	{"MIDINOTE0", K_MIDINOTE0},
	{"MIDINOTE1", K_MIDINOTE1},
	{"MIDINOTE2", K_MIDINOTE2},
	{"MIDINOTE3", K_MIDINOTE3},
	{"MIDINOTE4", K_MIDINOTE4},
	{"MIDINOTE5", K_MIDINOTE5},
	{"MIDINOTE6", K_MIDINOTE6},
	{"MIDINOTE7", K_MIDINOTE7},
	{"MIDINOTE8", K_MIDINOTE8},
	{"MIDINOTE9", K_MIDINOTE9},
	{"MIDINOTE10", K_MIDINOTE10},
	{"MIDINOTE11", K_MIDINOTE11},
	{"MIDINOTE12", K_MIDINOTE12},
	{"MIDINOTE13", K_MIDINOTE13},
	{"MIDINOTE14", K_MIDINOTE14},
	{"MIDINOTE15", K_MIDINOTE15},
	{"MIDINOTE16", K_MIDINOTE16},
	{"MIDINOTE17", K_MIDINOTE17},
	{"MIDINOTE18", K_MIDINOTE18},
	{"MIDINOTE19", K_MIDINOTE19},
	{"MIDINOTE20", K_MIDINOTE20},
	{"MIDINOTE21", K_MIDINOTE21},
	{"MIDINOTE22", K_MIDINOTE22},
	{"MIDINOTE23", K_MIDINOTE23},
	{"MIDINOTE24", K_MIDINOTE24},
	{"MIDINOTE25", K_MIDINOTE25},
	{"MIDINOTE26", K_MIDINOTE26},
	{"MIDINOTE27", K_MIDINOTE27},
	{"MIDINOTE28", K_MIDINOTE28},
	{"MIDINOTE29", K_MIDINOTE29},
	{"MIDINOTE30", K_MIDINOTE30},
	{"MIDINOTE31", K_MIDINOTE31},
	{"MIDINOTE32", K_MIDINOTE32},
	{"MIDINOTE33", K_MIDINOTE33},
	{"MIDINOTE34", K_MIDINOTE34},
	{"MIDINOTE35", K_MIDINOTE35},
	{"MIDINOTE36", K_MIDINOTE36},
	{"MIDINOTE37", K_MIDINOTE37},
	{"MIDINOTE38", K_MIDINOTE38},
	{"MIDINOTE39", K_MIDINOTE39},
	{"MIDINOTE40", K_MIDINOTE40},
	{"MIDINOTE41", K_MIDINOTE41},
	{"MIDINOTE42", K_MIDINOTE42},
	{"MIDINOTE43", K_MIDINOTE43},
	{"MIDINOTE44", K_MIDINOTE44},
	{"MIDINOTE45", K_MIDINOTE45},
	{"MIDINOTE46", K_MIDINOTE46},
	{"MIDINOTE47", K_MIDINOTE47},
	{"MIDINOTE48", K_MIDINOTE48},
	{"MIDINOTE49", K_MIDINOTE49},
	{"MIDINOTE50", K_MIDINOTE50},
	{"MIDINOTE51", K_MIDINOTE51},
	{"MIDINOTE52", K_MIDINOTE52},
	{"MIDINOTE53", K_MIDINOTE53},
	{"MIDINOTE54", K_MIDINOTE54},
	{"MIDINOTE55", K_MIDINOTE55},
	{"MIDINOTE56", K_MIDINOTE56},
	{"MIDINOTE57", K_MIDINOTE57},
	{"MIDINOTE58", K_MIDINOTE58},
	{"MIDINOTE59", K_MIDINOTE59},
	{"MIDINOTE60", K_MIDINOTE60},
	{"MIDINOTE61", K_MIDINOTE61},
	{"MIDINOTE62", K_MIDINOTE62},
	{"MIDINOTE63", K_MIDINOTE63},
	{"MIDINOTE64", K_MIDINOTE64},
	{"MIDINOTE65", K_MIDINOTE65},
	{"MIDINOTE66", K_MIDINOTE66},
	{"MIDINOTE67", K_MIDINOTE67},
	{"MIDINOTE68", K_MIDINOTE68},
	{"MIDINOTE69", K_MIDINOTE69},
	{"MIDINOTE70", K_MIDINOTE70},
	{"MIDINOTE71", K_MIDINOTE71},
	{"MIDINOTE72", K_MIDINOTE72},
	{"MIDINOTE73", K_MIDINOTE73},
	{"MIDINOTE74", K_MIDINOTE74},
	{"MIDINOTE75", K_MIDINOTE75},
	{"MIDINOTE76", K_MIDINOTE76},
	{"MIDINOTE77", K_MIDINOTE77},
	{"MIDINOTE78", K_MIDINOTE78},
	{"MIDINOTE79", K_MIDINOTE79},
	{"MIDINOTE80", K_MIDINOTE80},
	{"MIDINOTE81", K_MIDINOTE81},
	{"MIDINOTE82", K_MIDINOTE82},
	{"MIDINOTE83", K_MIDINOTE83},
	{"MIDINOTE84", K_MIDINOTE84},
	{"MIDINOTE85", K_MIDINOTE85},
	{"MIDINOTE86", K_MIDINOTE86},
	{"MIDINOTE87", K_MIDINOTE87},
	{"MIDINOTE88", K_MIDINOTE88},
	{"MIDINOTE89", K_MIDINOTE89},
	{"MIDINOTE90", K_MIDINOTE90},
	{"MIDINOTE91", K_MIDINOTE91},
	{"MIDINOTE92", K_MIDINOTE92},
	{"MIDINOTE93", K_MIDINOTE93},
	{"MIDINOTE94", K_MIDINOTE94},
	{"MIDINOTE95", K_MIDINOTE95},
	{"MIDINOTE96", K_MIDINOTE96},
	{"MIDINOTE97", K_MIDINOTE97},
	{"MIDINOTE98", K_MIDINOTE98},
	{"MIDINOTE99", K_MIDINOTE99},
	{"MIDINOTE100", K_MIDINOTE100},
	{"MIDINOTE101", K_MIDINOTE101},
	{"MIDINOTE102", K_MIDINOTE102},
	{"MIDINOTE103", K_MIDINOTE103},
	{"MIDINOTE104", K_MIDINOTE104},
	{"MIDINOTE105", K_MIDINOTE105},
	{"MIDINOTE106", K_MIDINOTE106},
	{"MIDINOTE107", K_MIDINOTE107},
	{"MIDINOTE108", K_MIDINOTE108},
	{"MIDINOTE109", K_MIDINOTE109},
	{"MIDINOTE110", K_MIDINOTE110},
	{"MIDINOTE111", K_MIDINOTE111},
	{"MIDINOTE112", K_MIDINOTE112},
	{"MIDINOTE113", K_MIDINOTE113},
	{"MIDINOTE114", K_MIDINOTE114},
	{"MIDINOTE115", K_MIDINOTE115},
	{"MIDINOTE116", K_MIDINOTE116},
	{"MIDINOTE117", K_MIDINOTE117},
	{"MIDINOTE118", K_MIDINOTE118},
	{"MIDINOTE119", K_MIDINOTE119},
	{"MIDINOTE120", K_MIDINOTE120},
	{"MIDINOTE121", K_MIDINOTE121},
	{"MIDINOTE122", K_MIDINOTE122},
	{"MIDINOTE123", K_MIDINOTE123},
	{"MIDINOTE124", K_MIDINOTE124},
	{"MIDINOTE125", K_MIDINOTE125},
	{"MIDINOTE126", K_MIDINOTE126},
	{"MIDINOTE127", K_MIDINOTE127},

	{NULL, 0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void
Key_ClearEditLine (int edit_line)
{
	memset (key_line, '\0', sizeof(key_line));
	key_line[0] = ']';
	key_linepos = 1;
}

/*
====================
Interactive line editing and console scrollback
====================
*/
static void
Key_Console (int key, int ascii)
{
	// LordHavoc: copied most of this from Q2 to improve keyboard handling
	switch (key)
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ((key == 'v' && keydown[K_CTRL]) || ((key == K_INS || key == K_KP_INS) && keydown[K_SHIFT]))
	{
		char *cbd, *p;
		if ((cbd = Sys_GetClipboardData()) != 0)
		{
			int i;
#if 1
			p = cbd;
			while (*p)
			{
				if (*p == '\r' && *(p+1) == '\n')
				{
					*p++ = ';';
					*p++ = ' ';
				}
				else if (*p == '\n' || *p == '\r' || *p == '\b')
					*p++ = ';';
				p++;
			}
#else
			strtok(cbd, "\n\r\b");
#endif
			i = (int)strlen(cbd);
			if (i + key_linepos >= MAX_INPUTLINE)
				i= MAX_INPUTLINE - key_linepos - 1;
			if (i > 0)
			{
				// terencehill: insert the clipboard text between the characters of the line
				char *temp = (char *) Z_Malloc(MAX_INPUTLINE);
				cbd[i]=0;
				temp[0]=0;
				if ( key_linepos < (int)strlen(key_line) )
					strlcpy(temp, key_line + key_linepos, (int)strlen(key_line) - key_linepos +1);
				key_line[key_linepos] = 0;
				strlcat(key_line, cbd, sizeof(key_line));
				if (temp[0])
					strlcat(key_line, temp, sizeof(key_line));
				Z_Free(temp);
				key_linepos += i;
			}
			Z_Free(cbd);
		}
		return;
	}

	if (key == 'l' && keydown[K_CTRL])
	{
		Cbuf_AddText ("clear\n");
		return;
	}

	if (key == 'u' && keydown[K_CTRL]) // like vi/readline ^u: delete currently edited line
	{
		// clear line
		key_line[0] = ']';
		key_line[1] = 0;
		key_linepos = 1;
		return;
	}

	if (key == 'q' && keydown[K_CTRL]) // like zsh ^q: push line to history, don't execute, and clear
	{
		// clear line
		Key_History_Push();
		key_line[0] = ']';
		key_line[1] = 0;
		key_linepos = 1;
		return;
	}

	if (key == K_ENTER || key == K_KP_ENTER)
	{
		Cbuf_AddText (key_line+1);	// skip the ]
		Cbuf_AddText ("\n");
		Key_History_Push();
		key_line[0] = ']';
		key_line[1] = 0;	// EvilTypeGuy: null terminate
		key_linepos = 1;
		// force an update, because the command may take some time
		if (cls.state == ca_disconnected)
			CL_UpdateScreen ();
		return;
	}

	if (key == K_TAB)
	{
		if(keydown[K_CTRL]) // append to the cvar its value
		{
			int		cvar_len, cvar_str_len, chars_to_move;
			char	k;
			char	cvar[MAX_INPUTLINE];
			const char *cvar_str;
			
			// go to the start of the variable
			while(--key_linepos)
			{
				k = key_line[key_linepos];
				if(k == '\"' || k == ';' || k == ' ' || k == '\'')
					break;
			}
			key_linepos++;
			
			// save the variable name in cvar
			for(cvar_len=0; (k = key_line[key_linepos + cvar_len]) != 0; cvar_len++)
			{
				if(k == '\"' || k == ';' || k == ' ' || k == '\'')
					break;
				cvar[cvar_len] = k;
			}
			if (cvar_len==0)
				return;
			cvar[cvar_len] = 0;
			
			// go to the end of the cvar
			key_linepos += cvar_len;
			
			// save the content of the variable in cvar_str
			cvar_str = Cvar_VariableString(cvar);
			cvar_str_len = strlen(cvar_str);
			if (cvar_str_len==0)
				return;
			
			// insert space and cvar_str in key_line
			chars_to_move = strlen(&key_line[key_linepos]);
			if (key_linepos + 1 + cvar_str_len + chars_to_move < MAX_INPUTLINE)
			{
				if (chars_to_move)
					memmove(&key_line[key_linepos + 1 + cvar_str_len], &key_line[key_linepos], chars_to_move);
				key_line[key_linepos++] = ' ';
				memcpy(&key_line[key_linepos], cvar_str, cvar_str_len);
				key_linepos += cvar_str_len;
				key_line[key_linepos + chars_to_move] = 0;
			}
			else
				Con_Printf("Couldn't append cvar value, edit line too long.\n");
			return;
		}
		// Enhanced command completion
		// by EvilTypeGuy eviltypeguy@qeradiant.com
		// Thanks to Fett, Taniwha
		Con_CompleteCommandLine();
		return;
	}

	// Advanced Console Editing by Radix radix@planetquake.com
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	// Enhanced by [515]
	// Enhanced by terencehill

	// move cursor to the previous character
	if (key == K_LEFTARROW || key == K_KP_LEFTARROW)
	{
		if (key_linepos < 2)
			return;
		if(keydown[K_CTRL]) // move cursor to the previous word
		{
			int		pos;
			char	k;
			pos = key_linepos-1;

			if(pos) // skip all "; ' after the word
				while(--pos)
				{
					k = key_line[pos];
					if (!(k == '\"' || k == ';' || k == ' ' || k == '\''))
						break;
				}

			if(pos)
				while(--pos)
				{
					k = key_line[pos];
					if(k == '\"' || k == ';' || k == ' ' || k == '\'')
						break;
				}
			key_linepos = pos + 1;
		}
		else if(keydown[K_SHIFT]) // move cursor to the previous character ignoring colors
		{
			int		pos;
			pos = key_linepos-1;
			while (pos)
				if(pos-1 > 0 && key_line[pos-1] == STRING_COLOR_TAG && isdigit(key_line[pos]))
					pos-=2;
				else if(pos-4 > 0 && key_line[pos-4] == STRING_COLOR_TAG && key_line[pos-3] == STRING_COLOR_RGB_TAG_CHAR
						&& isxdigit(key_line[pos-2]) && isxdigit(key_line[pos-1]) && isxdigit(key_line[pos]))
					pos-=5;
				else
				{
					if(pos-1 > 0 && key_line[pos-1] == STRING_COLOR_TAG && key_line[pos] == STRING_COLOR_TAG) // consider ^^ as a character
						pos--;
					pos--;
					break;
				}
			key_linepos = pos + 1;
		}
		else
			key_linepos--;
		return;
	}

	// delete char before cursor
	if (key == K_BACKSPACE || (key == 'h' && keydown[K_CTRL]))
	{
		if (key_linepos > 1)
		{
			strlcpy(key_line + key_linepos - 1, key_line + key_linepos, sizeof(key_line) + 1 - key_linepos);
			key_linepos--;
		}
		return;
	}

	// delete char on cursor
	if (key == K_DEL || key == K_KP_DEL)
	{
		size_t linelen;
		linelen = strlen(key_line);
		if (key_linepos < (int)linelen)
			memmove(key_line + key_linepos, key_line + key_linepos + 1, linelen - key_linepos);
		return;
	}


	// move cursor to the next character
	if (key == K_RIGHTARROW || key == K_KP_RIGHTARROW)
	{
		if (key_linepos >= (int)strlen(key_line))
			return;
		if(keydown[K_CTRL]) // move cursor to the next word
		{
			int		pos, len;
			char	k;
			len = (int)strlen(key_line);
			pos = key_linepos;

			while(++pos < len)
			{
				k = key_line[pos];
				if(k == '\"' || k == ';' || k == ' ' || k == '\'')
					break;
			}
			
			if (pos < len) // skip all "; ' after the word
				while(++pos < len)
				{
					k = key_line[pos];
					if (!(k == '\"' || k == ';' || k == ' ' || k == '\''))
						break;
				}
			key_linepos = pos;
		}
		else if(keydown[K_SHIFT]) // move cursor to the next character ignoring colors
		{
			int		pos, len;
			len = (int)strlen(key_line);
			pos = key_linepos;
			
			// go beyond all initial consecutive color tags, if any
			if(pos < len)
				while (key_line[pos] == STRING_COLOR_TAG)
				{
					if(isdigit(key_line[pos+1]))
						pos+=2;
					else if(key_line[pos+1] == STRING_COLOR_RGB_TAG_CHAR && isxdigit(key_line[pos+2]) && isxdigit(key_line[pos+3]) && isxdigit(key_line[pos+4]))
						pos+=5;
					else
						break;
				}
			
			// skip the char
			if (key_line[pos] == STRING_COLOR_TAG && key_line[pos+1] == STRING_COLOR_TAG) // consider ^^ as a character
				pos++;
			pos++;
			
			// now go beyond all next consecutive color tags, if any
			if(pos < len)
				while (key_line[pos] == STRING_COLOR_TAG)
				{
					if(isdigit(key_line[pos+1]))
						pos+=2;
					else if(key_line[pos+1] == STRING_COLOR_RGB_TAG_CHAR && isxdigit(key_line[pos+2]) && isxdigit(key_line[pos+3]) && isxdigit(key_line[pos+4]))
						pos+=5;
					else
						break;
				}
			key_linepos = pos;
		}
		else
			key_linepos++;
		return;
	}

	if (key == K_INS || key == K_KP_INS) // toggle insert mode
	{
		key_insert ^= 1;
		return;
	}

	// End Advanced Console Editing

	if (key == K_UPARROW || key == K_KP_UPARROW || (key == 'p' && keydown[K_CTRL]))
	{
		Key_History_Up();
		return;
	}

	if (key == K_DOWNARROW || key == K_KP_DOWNARROW || (key == 'n' && keydown[K_CTRL]))
	{
		Key_History_Down();
		return;
	}
	// ~1.0795 = 82/76  using con_textsize 64 76 is height of the char, 6 is the distance between 2 lines
	if (key == K_PGUP || key == K_KP_PGUP)
	{
		if(keydown[K_CTRL])
		{
			con_backscroll += ((vid_conheight.integer >> 2) / con_textsize.integer)-1;
		}
		else
			con_backscroll += ((vid_conheight.integer >> 1) / con_textsize.integer)-3;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN)
	{
		if(keydown[K_CTRL])
		{
			con_backscroll -= ((vid_conheight.integer >> 2) / con_textsize.integer)-1;
		}
		else
			con_backscroll -= ((vid_conheight.integer >> 1) / con_textsize.integer)-3;
		return;
	}
 
	if (key == K_MWHEELUP)
	{
		if(keydown[K_CTRL])
			con_backscroll += 1;
		else if(keydown[K_SHIFT])
			con_backscroll += ((vid_conheight.integer >> 2) / con_textsize.integer)-1;
		else
			con_backscroll += 5;
		return;
	}

	if (key == K_MWHEELDOWN)
	{
		if(keydown[K_CTRL])
			con_backscroll -= 1;
		else if(keydown[K_SHIFT])
			con_backscroll -= ((vid_conheight.integer >> 2) / con_textsize.integer)-1;
		else
			con_backscroll -= 5;
		return;
	}

	if (keydown[K_CTRL])
	{
		// text zoom in
		if (key == '+' || key == K_KP_PLUS)
		{
			if (con_textsize.integer < 128)
				Cvar_SetValueQuick(&con_textsize, con_textsize.integer + 1);
			return;
		}
		// text zoom out
		if (key == '-' || key == K_KP_MINUS)
		{
			if (con_textsize.integer > 1)
				Cvar_SetValueQuick(&con_textsize, con_textsize.integer - 1);
			return;
		}
		// text zoom reset
		if (key == '0' || key == K_KP_INS)
		{
			Cvar_SetValueQuick(&con_textsize, atoi(Cvar_VariableDefString("con_textsize")));
			return;
		}
	}

	if (key == K_HOME || key == K_KP_HOME)
	{
		if (keydown[K_CTRL])
			con_backscroll = INT_MAX;
		else
			key_linepos = 1;
		return;
	}

	if (key == K_END || key == K_KP_END)
	{
		if (keydown[K_CTRL])
			con_backscroll = 0;
		else
			key_linepos = (int)strlen(key_line);
		return;
	}

	// non printable
	if (ascii < 32)
		return;

	if (key_linepos < MAX_INPUTLINE-1)
	{
		int len;
		len = (int)strlen(&key_line[key_linepos]);
		// check insert mode, or always insert if at end of line
		if (key_insert || len == 0)
		{
			// can't use strcpy to move string to right
			len++;
			memmove(&key_line[key_linepos + 1], &key_line[key_linepos], len);
		}
		key_line[key_linepos] = ascii;
		key_linepos++;
	}
}

//============================================================================

int chat_mode;
char		chat_buffer[MAX_INPUTLINE];
unsigned int	chat_bufferlen = 0;

extern int Nicks_CompleteChatLine(char *buffer, size_t size, unsigned int pos);

static void
Key_Message (int key, int ascii)
{

	if (key == K_ENTER || ascii == 10 || ascii == 13)
	{
		if(chat_mode < 0)
			Cmd_ExecuteString(chat_buffer, src_command); // not Cbuf_AddText to allow semiclons in args; however, this allows no variables then. Use aliases!
		else
			Cmd_ForwardStringToServer(va("%s %s", chat_mode ? "say_team" : "say ", chat_buffer));

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	// TODO add support for arrow keys and simple editing

	if (key == K_ESCAPE) {
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_BACKSPACE) {
		if (chat_bufferlen) {
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if(key == K_TAB) {
		chat_bufferlen = Nicks_CompleteChatLine(chat_buffer, sizeof(chat_buffer), chat_bufferlen);
		return;
	}

	if (chat_bufferlen == sizeof (chat_buffer) - 1)
		return;							// all full

	if (!ascii)
		return;							// non printable

	chat_buffer[chat_bufferlen++] = ascii;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int
Key_StringToKeynum (const char *str)
{
	const keyname_t  *kn;

	if (!str || !str[0])
		return -1;
	if (!str[1])
		return tolower(str[0]);

	for (kn = keynames; kn->name; kn++) {
		if (!strcasecmp (str, kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
const char *
Key_KeynumToString (int keynum)
{
	const keyname_t  *kn;
	static char tinystr[2];

	// -1 is an invalid code
	if (keynum < 0)
		return "<KEY NOT FOUND>";

	// search overrides first, because some characters are special
	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	// if it is printable, output it as a single character
	if (keynum > 32 && keynum < 256)
	{
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	// if it is not overridden and not printable, we don't know what to do with it
	return "<UNKNOWN KEYNUM>";
}


void
Key_SetBinding (int keynum, int bindmap, const char *binding)
{
	char *newbinding;
	size_t l;

	if (keynum == -1 || keynum >= MAX_KEYS)
		return;

// free old bindings
	if (keybindings[bindmap][keynum]) {
		Z_Free (keybindings[bindmap][keynum]);
		keybindings[bindmap][keynum] = NULL;
	}
	if(!binding[0]) // make "" binds be removed --blub
		return;
// allocate memory for new binding
	l = strlen (binding);
	newbinding = (char *)Z_Malloc (l + 1);
	memcpy (newbinding, binding, l + 1);
	newbinding[l] = 0;
	keybindings[bindmap][keynum] = newbinding;
}

static void
Key_In_Unbind_f (void)
{
	int         b, m;
	char *errchar = NULL;

	if (Cmd_Argc () != 3) {
		Con_Print("in_unbind <bindmap> <key> : remove commands from a key\n");
		return;
	}

	m = strtol(Cmd_Argv (1), &errchar, 0);
	if ((m < 0) || (m >= 8) || (errchar && *errchar)) {
		Con_Printf("%s isn't a valid bindmap\n", Cmd_Argv(1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	Key_SetBinding (b, m, "");
}

static void
Key_In_Bind_f (void)
{
	int         i, c, b, m;
	char        cmd[MAX_INPUTLINE];
	char *errchar = NULL;

	c = Cmd_Argc ();

	if (c != 3 && c != 4) {
		Con_Print("in_bind <bindmap> <key> [command] : attach a command to a key\n");
		return;
	}

	m = strtol(Cmd_Argv (1), &errchar, 0);
	if ((m < 0) || (m >= 8) || (errchar && *errchar)) {
		Con_Printf("%s isn't a valid bindmap\n", Cmd_Argv(1));
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1 || b >= MAX_KEYS) {
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv (2));
		return;
	}

	if (c == 3) {
		if (keybindings[m][b])
			Con_Printf("\"%s\" = \"%s\"\n", Cmd_Argv (2), keybindings[m][b]);
		else
			Con_Printf("\"%s\" is not bound\n", Cmd_Argv (2));
		return;
	}
// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	for (i = 3; i < c; i++) {
		strlcat (cmd, Cmd_Argv (i), sizeof (cmd));
		if (i != (c - 1))
			strlcat (cmd, " ", sizeof (cmd));
	}

	Key_SetBinding (b, m, cmd);
}

static void
Key_In_Bindmap_f (void)
{
	int         m1, m2, c;
	char *errchar = NULL;

	c = Cmd_Argc ();

	if (c != 3) {
		Con_Print("in_bindmap <bindmap> <fallback>: set current bindmap and fallback\n");
		return;
	}

	m1 = strtol(Cmd_Argv (1), &errchar, 0);
	if ((m1 < 0) || (m1 >= 8) || (errchar && *errchar)) {
		Con_Printf("%s isn't a valid bindmap\n", Cmd_Argv(1));
		return;
	}

	m2 = strtol(Cmd_Argv (2), &errchar, 0);
	if ((m2 < 0) || (m2 >= 8) || (errchar && *errchar)) {
		Con_Printf("%s isn't a valid bindmap\n", Cmd_Argv(2));
		return;
	}

	key_bmap = m1;
	key_bmap2 = m2;
}

static void
Key_Unbind_f (void)
{
	int         b;

	if (Cmd_Argc () != 2) {
		Con_Print("unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (1));
	if (b == -1) {
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	Key_SetBinding (b, 0, "");
}

static void
Key_Unbindall_f (void)
{
	int         i, j;

	for (j = 0; j < 8; j++)
		for (i = 0; i < (int)(sizeof(keybindings[0])/sizeof(keybindings[0][0])); i++)
			if (keybindings[j][i])
				Key_SetBinding (i, j, "");
}

static void
Key_PrintBindList(int j)
{
	char bindbuf[MAX_INPUTLINE];
	const char *p;
	int i;

	for (i = 0; i < (int)(sizeof(keybindings[0])/sizeof(keybindings[0][0])); i++)
	{
		p = keybindings[j][i];
		if (p)
		{
			Cmd_QuoteString(bindbuf, sizeof(bindbuf), p, "\"\\");
			if (j == 0)
				Con_Printf("^2%s ^7= \"%s\"\n", Key_KeynumToString (i), bindbuf);
			else
				Con_Printf("^3bindmap %d: ^2%s ^7= \"%s\"\n", j, Key_KeynumToString (i), bindbuf);
		}
	}
}

static void
Key_In_BindList_f (void)
{
	int m;
	char *errchar = NULL;

	if(Cmd_Argc() >= 2)
	{
		m = strtol(Cmd_Argv(1), &errchar, 0);
		if ((m < 0) || (m >= 8) || (errchar && *errchar)) {
			Con_Printf("%s isn't a valid bindmap\n", Cmd_Argv(1));
			return;
		}
		Key_PrintBindList(m);
	}
	else
	{
		for (m = 0; m < MAX_BINDMAPS; m++)
			Key_PrintBindList(m);
	}
}

static void
Key_BindList_f (void)
{
	Key_PrintBindList(0);
}

static void
Key_Bind_f (void)
{
	int         i, c, b;
	char        cmd[MAX_INPUTLINE];

	c = Cmd_Argc ();

	if (c != 2 && c != 3) {
		Con_Print("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv (1));
	if (b == -1 || b >= MAX_KEYS) {
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv (1));
		return;
	}

	if (c == 2) {
		if (keybindings[0][b])
			Con_Printf("\"%s\" = \"%s\"\n", Cmd_Argv (1), keybindings[0][b]);
		else
			Con_Printf("\"%s\" is not bound\n", Cmd_Argv (1));
		return;
	}
// copy the rest of the command line
	cmd[0] = 0;							// start out with a null string
	for (i = 2; i < c; i++) {
		strlcat (cmd, Cmd_Argv (i), sizeof (cmd));
		if (i != (c - 1))
			strlcat (cmd, " ", sizeof (cmd));
	}

	Key_SetBinding (b, 0, cmd);
}

/*
============
Writes lines containing "bind key value"
============
*/
void
Key_WriteBindings (qfile_t *f)
{
	int         i, j;
	char bindbuf[MAX_INPUTLINE];
	const char *p;

	for (j = 0; j < MAX_BINDMAPS; j++)
	{
		for (i = 0; i < (int)(sizeof(keybindings[0])/sizeof(keybindings[0][0])); i++)
		{
			p = keybindings[j][i];
			if (p)
			{
				Cmd_QuoteString(bindbuf, sizeof(bindbuf), p, "\"\\");
				if (j == 0)
					FS_Printf(f, "bind %s \"%s\"\n", Key_KeynumToString (i), bindbuf);
				else
					FS_Printf(f, "in_bind %d %s \"%s\"\n", j, Key_KeynumToString (i), bindbuf);
			}
		}
	}
}


void
Key_Init (void)
{
	Key_History_Init();
	key_line[0] = ']';
	key_line[1] = 0;
	key_linepos = 1;

//
// register our functions
//
	Cmd_AddCommand ("in_bind", Key_In_Bind_f, "binds a command to the specified key in the selected bindmap");
	Cmd_AddCommand ("in_unbind", Key_In_Unbind_f, "removes command on the specified key in the selected bindmap");
	Cmd_AddCommand ("in_bindlist", Key_In_BindList_f, "bindlist: displays bound keys for all bindmaps, or the given bindmap");
	Cmd_AddCommand ("in_bindmap", Key_In_Bindmap_f, "selects active foreground and background (used only if a key is not bound in the foreground) bindmaps for typing");

	Cmd_AddCommand ("bind", Key_Bind_f, "binds a command to the specified key in bindmap 0");
	Cmd_AddCommand ("unbind", Key_Unbind_f, "removes a command on the specified key in bindmap 0");
	Cmd_AddCommand ("bindlist", Key_BindList_f, "bindlist: displays bound keys for bindmap 0 bindmaps");
	Cmd_AddCommand ("unbindall", Key_Unbindall_f, "removes all commands from all keys in all bindmaps (leaving only shift-escape and escape)");

	Cvar_RegisterVariable (&con_closeontoggleconsole);
}

void
Key_Shutdown (void)
{
	Key_History_Shutdown();
}

const char *Key_GetBind (int key)
{
	const char *bind;
	if (key < 0 || key >= MAX_KEYS)
		return NULL;
	bind = keybindings[key_bmap][key];
	if (!bind)
		bind = keybindings[key_bmap2][key];
	return bind;
}

qboolean CL_VM_InputEvent (qboolean down, int key, int ascii);

/*
===================
Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
static char tbl_keyascii[MAX_KEYS];
static keydest_t tbl_keydest[MAX_KEYS];

void
Key_Event (int key, int ascii, qboolean down)
{
	const char *bind;
	qboolean q;
	keydest_t keydest = key_dest;

	if (key < 0 || key >= MAX_KEYS)
		return;

	// get key binding
	bind = keybindings[key_bmap][key];
	if (!bind)
		bind = keybindings[key_bmap2][key];

	if (developer.integer >= 1000)
		Con_Printf("Key_Event(%i, '%c', %s) keydown %i bind \"%s\"\n", key, ascii, down ? "down" : "up", keydown[key], bind ? bind : "");

	if(key_consoleactive)
		keydest = key_console;
	
	if (down)
	{
		// increment key repeat count each time a down is received so that things
		// which want to ignore key repeat can ignore it
		keydown[key] = min(keydown[key] + 1, 2);
		if(keydown[key] == 1) {
			tbl_keyascii[key] = ascii;
			tbl_keydest[key] = keydest;
		} else {
			ascii = tbl_keyascii[key];
			keydest = tbl_keydest[key];
		}
	}
	else
	{
		// clear repeat count now that the key is released
		keydown[key] = 0;
		keydest = tbl_keydest[key];
		ascii = tbl_keyascii[key];
	}

	if(keydest == key_void)
		return;
	
	// key_consoleactive is a flag not a key_dest because the console is a
	// high priority overlay ontop of the normal screen (designed as a safety
	// feature so that developers and users can rescue themselves from a bad
	// situation).
	//
	// this also means that toggling the console on/off does not lose the old
	// key_dest state

	// specially handle escape (togglemenu) and shift-escape (toggleconsole)
	// engine bindings, these are not handled as normal binds so that the user
	// can recover from a completely empty bindmap
	if (key == K_ESCAPE)
	{
		// ignore key repeats on escape
		if (keydown[key] > 1)
			return;

		// escape does these things:
		// key_consoleactive - close console
		// key_message - abort messagemode
		// key_menu - go to parent menu (or key_game)
		// key_game - open menu

		// in all modes shift-escape toggles console
		if (keydown[K_SHIFT])
		{
			if(down)
			{
				Con_ToggleConsole_f ();
				tbl_keydest[key] = key_void; // esc release should go nowhere (especially not to key_menu or key_game)
			}
			return;
		}

		switch (keydest)
		{
			case key_console:
				if(down)
				{
					if(key_consoleactive & KEY_CONSOLEACTIVE_FORCED)
					{
						key_consoleactive &= ~KEY_CONSOLEACTIVE_USER;
						MR_ToggleMenu_f ();
					}
					else
						Con_ToggleConsole_f();
				}
				break;

			case key_message:
				if (down)
					Key_Message (key, ascii); // that'll close the message input
				break;

			case key_menu:
			case key_menu_grabbed:
				MR_KeyEvent (key, ascii, down);
				break;

			case key_game:
				// csqc has priority over toggle menu if it wants to (e.g. handling escape for UI stuff in-game.. :sick:)
				q = CL_VM_InputEvent(down, key, ascii);
				if (!q && down)
					MR_ToggleMenu_f ();
				break;

			default:
				Con_Printf ("Key_Event: Bad key_dest\n");
		}
		return;
	}

	// send function keydowns to interpreter no matter what mode is (unless the menu has specifically grabbed the keyboard, for rebinding keys)
	if (keydest != key_menu_grabbed)
	if (key >= K_F1 && key <= K_F12)
	{
		if (bind)
		{
			if(keydown[key] == 1 && down)
			{
				// button commands add keynum as a parm
				if (bind[0] == '+')
					Cbuf_AddText (va("%s %i\n", bind, key));
				else
				{
					Cbuf_AddText (bind);
					Cbuf_AddText ("\n");
				}
			} else if(bind[0] == '+' && !down && keydown[key] == 0)
				Cbuf_AddText(va("-%s %i\n", bind + 1, key));
		}
		return;
	}

	// send input to console if it wants it
	if (keydest == key_console)
	{
		if (!down)
			return;
		// con_closeontoggleconsole enables toggleconsole keys to close the
		// console, as long as they are not the color prefix character
		// (special exemption for german keyboard layouts)
		if (con_closeontoggleconsole.integer && bind && !strncmp(bind, "toggleconsole", strlen("toggleconsole")) && (key_consoleactive & KEY_CONSOLEACTIVE_USER) && ascii != STRING_COLOR_TAG)
		{
			Con_ToggleConsole_f ();
			return;
		}
		Key_Console (key, ascii);
		return;
	}

	// handle toggleconsole in menu too
	if (keydest == key_menu)
	{
		if (down && con_closeontoggleconsole.integer && bind && !strncmp(bind, "toggleconsole", strlen("toggleconsole")) && ascii != STRING_COLOR_TAG)
		{
			Con_ToggleConsole_f ();
			tbl_keydest[key] = key_void; // key release should go nowhere (especially not to key_menu or key_game)
			return;
		}
	}

	// ignore binds while a video is played, let the video system handle the key event
	if (cl_videoplaying)
	{
		CL_Video_KeyEvent (key, ascii, keydown[key] != 0);
		return;
	}

	// anything else is a key press into the game, chat line, or menu
	switch (keydest)
	{
		case key_message:
			if (down)
				Key_Message (key, ascii);
			break;
		case key_menu:
		case key_menu_grabbed:
			MR_KeyEvent (key, ascii, down);
			break;
		case key_game:
			q = CL_VM_InputEvent(down, key, ascii);
			// ignore key repeats on binds and only send the bind if the event hasnt been already processed by csqc
			if (!q && bind)
			{
				if(keydown[key] == 1 && down)
				{
					// button commands add keynum as a parm
					if (bind[0] == '+')
						Cbuf_AddText (va("%s %i\n", bind, key));
					else
					{
						Cbuf_AddText (bind);
						Cbuf_AddText ("\n");
					}
				} else if(bind[0] == '+' && !down && keydown[key] == 0)
					Cbuf_AddText(va("-%s %i\n", bind + 1, key));
			}
			break;
		default:
			Con_Printf ("Key_Event: Bad key_dest\n");
	}
}

/*
===================
Key_ClearStates
===================
*/
void
Key_ClearStates (void)
{
	memset(keydown, 0, sizeof(keydown));
}
