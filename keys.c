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


/*
key up events are sent even if in console mode
*/

int			edit_line = MAX_INPUTLINES-1;
int			history_line = MAX_INPUTLINES-1;
char		key_lines[MAX_INPUTLINES][MAX_INPUTLINE];
int			key_linepos;
qboolean	key_insert = true;	// insert key toggle (for editing)
keydest_t	key_dest;
int			key_consoleactive;
char		*keybindings[MAX_BINDMAPS][MAX_KEYS];

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

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
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

	{"NUMLOCK", K_NUMLOCK},
	{"CAPSLOCK", K_CAPSLOCK},
	{"SCROLLOCK", K_SCROLLOCK},

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },

	{"KP_0", K_KP_0},
	{"KP_1", K_KP_1},
	{"KP_2", K_KP_2},
	{"KP_3", K_KP_3},
	{"KP_4", K_KP_4},
	{"KP_5", K_KP_5},
	{"KP_6", K_KP_6},
	{"KP_7", K_KP_7},
	{"KP_8", K_KP_8},
	{"KP_9", K_KP_9},
	{"KP_PERIOD", K_KP_PERIOD},
	{"KP_DIVIDE", K_KP_DIVIDE},
	{"KP_MULTIPLY", K_KP_MULTIPLY},
	{"KP_MINUS", K_KP_MINUS},
	{"KP_PLUS", K_KP_PLUS},
	{"KP_ENTER", K_KP_ENTER},
	{"KP_EQUALS", K_KP_EQUALS},

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

	{"SEMICOLON", ';'},			// because a raw semicolon seperates commands
	{"TILDE", '~'},
	{"BACKQUOTE", '`'},
	{"QUOTE", '"'},
	{"APOSTROPHE", '\''},

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
	memset (key_lines[edit_line], '\0', sizeof(key_lines[edit_line]));
	key_lines[edit_line][0] = ']';
	key_linepos = 1;
}

/*
====================
Interactive line editing and console scrollback
====================
*/
static void
Key_Console (int key, char ascii)
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

	if ((toupper(key) == 'V' && keydown[K_CTRL]) || ((key == K_INS || key == K_KP_INS) && keydown[K_SHIFT]))
	{
		char *cbd;
		if ((cbd = Sys_GetClipboardData()) != 0)
		{
			int i;
			strtok(cbd, "\n\r\b");
			i = (int)strlen(cbd);
			if (i + key_linepos >= MAX_INPUTLINE)
				i= MAX_INPUTLINE - key_linepos;
			if (i > 0)
			{
				cbd[i]=0;
				strcat(key_lines[edit_line], cbd);
				key_linepos += i;
			}
			free(cbd);
		}
		return;
	}

	if (key == 'l')
	{
		if (keydown[K_CTRL])
		{
			Cbuf_AddText ("clear\n");
			return;
		}
	}

	if (key == K_ENTER || key == K_KP_ENTER)
	{
		Cbuf_AddText (key_lines[edit_line]+1);	// skip the ]
		Cbuf_AddText ("\n");
		Con_Printf("%s\n",key_lines[edit_line]);
		// LordHavoc: redesigned edit_line/history_line
		edit_line = 31;
		history_line = edit_line;
		memmove(key_lines[0], key_lines[1], sizeof(key_lines[0]) * edit_line);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;	// EvilTypeGuy: null terminate
		key_linepos = 1;
		// force an update, because the command may take some time
		if (cls.state == ca_disconnected)
			CL_UpdateScreen ();
		return;
	}

	if (key == K_TAB)
	{
		// Enhanced command completion
		// by EvilTypeGuy eviltypeguy@qeradiant.com
		// Thanks to Fett, Taniwha
		Con_CompleteCommandLine();
		return;
	}

	// Advanced Console Editing by Radix radix@planetquake.com
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com

	// left arrow will just move left one without erasing, backspace will
	// actually erase charcter
	if (key == K_LEFTARROW || key == K_KP_LEFTARROW)
	{
		if (key_linepos > 1)
			key_linepos--;
		return;
	}

	// delete char before cursor
	if (key == K_BACKSPACE || (key == 'h' && keydown[K_CTRL]))
	{
		if (key_linepos > 1)
		{
			strcpy(key_lines[edit_line] + key_linepos - 1, key_lines[edit_line] + key_linepos);
			key_linepos--;
		}
		return;
	}

	// delete char on cursor
	if (key == K_DEL || key == K_KP_DEL)
	{
		if (key_linepos < (int)strlen(key_lines[edit_line]))
			strcpy(key_lines[edit_line] + key_linepos, key_lines[edit_line] + key_linepos + 1);
		return;
	}


	// if we're at the end, get one character from previous line,
	// otherwise just go right one
	if (key == K_RIGHTARROW || key == K_KP_RIGHTARROW)
	{
		if (key_linepos < (int)strlen(key_lines[edit_line]))
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
		if (history_line > 0 && key_lines[history_line-1][1])
		{
			history_line--;
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = (int)strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_DOWNARROW || key == K_KP_DOWNARROW || (key == 'n' && keydown[K_CTRL]))
	{
		history_line++;
		if (history_line >= edit_line)
		{
			history_line = edit_line;
			key_lines[edit_line][0] = ']';
			key_lines[edit_line][1] = 0;
			key_linepos = 1;
		}
		else
		{
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = (int)strlen(key_lines[edit_line]);
		}
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP)
	{
		con_backscroll += ((int) vid_conheight.integer >> 5);
		if (con_backscroll > con_totallines - (vid_conheight.integer>>3) - 1)
			con_backscroll = con_totallines - (vid_conheight.integer>>3) - 1;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN)
	{
		con_backscroll -= ((int) vid_conheight.integer >> 5);
		if (con_backscroll < 0)
			con_backscroll = 0;
		return;
	}

	if (key == K_HOME || key == K_KP_HOME)
	{
		con_backscroll = con_totallines - (vid_conheight.integer>>3) - 1;
		return;
	}

	if (key == K_END || key == K_KP_END)
	{
		con_backscroll = 0;
		return;
	}

	// non printable
	if (ascii < 32)
		return;

	if (key_linepos < MAX_INPUTLINE-1)
	{
		int len;
		len = (int)strlen(&key_lines[edit_line][key_linepos]);
		// check insert mode, or always insert if at end of line
		if (key_insert || len == 0)
		{
			// can't use strcpy to move string to right
			len++;
			memmove(&key_lines[edit_line][key_linepos + 1], &key_lines[edit_line][key_linepos], len);
		}
		key_lines[edit_line][key_linepos] = ascii;
		key_linepos++;
	}
}

//============================================================================

qboolean	chat_team;
char		chat_buffer[MAX_INPUTLINE];
unsigned int	chat_bufferlen = 0;

static void
Key_Message (int key, char ascii)
{

	if (key == K_ENTER)
	{
		Cmd_ForwardStringToServer(va("%s %s", chat_team ? "say_team" : "say ", chat_buffer));

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

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

	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127) {	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn = keynames; kn->name; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


void
Key_SetBinding (int keynum, int bindmap, const char *binding)
{
	char *newbinding;
	size_t l;

	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[bindmap][keynum]) {
		Z_Free (keybindings[bindmap][keynum]);
		keybindings[bindmap][keynum] = NULL;
	}
// allocate memory for new binding
	l = strlen (binding);
	newbinding = (char *)Z_Malloc (l + 1);
	strcpy (newbinding, binding);
	newbinding[l] = 0;
	keybindings[bindmap][keynum] = newbinding;
}

static void
Key_In_Unbind_f (void)
{
	int         b, m;

	if (Cmd_Argc () != 3) {
		Con_Print("in_unbind <bindmap> <key> : remove commands from a key\n");
		return;
	}

	m = strtol(Cmd_Argv (1), NULL, 0);
	if ((m < 0) || (m >= 8)) {
		Con_Printf("%d isn't a valid bindmap\n", m);
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
	char        cmd[1024];

	c = Cmd_Argc ();

	if (c != 3 && c != 4) {
		Con_Print("in_bind <bindmap> <key> [command] : attach a command to a key\n");
		return;
	}

	m = strtol(Cmd_Argv (1), NULL, 0);
	if ((m < 0) || (m >= 8)) {
		Con_Printf("%d isn't a valid bindmap\n", m);
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv (2));
	if (b == -1) {
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

	c = Cmd_Argc ();

	if (c != 3) {
		Con_Print("in_bindmap <bindmap> <fallback>: set current bindmap and fallback\n");
		return;
	}

	m1 = strtol(Cmd_Argv (1), NULL, 0);
	if ((m1 < 0) || (m1 >= 8)) {
		Con_Printf("%d isn't a valid bindmap\n", m1);
		return;
	}

	m2 = strtol(Cmd_Argv (2), NULL, 0);
	if ((m2 < 0) || (m2 >= 8)) {
		Con_Printf("%d isn't a valid bindmap\n", m2);
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
Key_Bind_f (void)
{
	int         i, c, b;
	char        cmd[1024];

	c = Cmd_Argc ();

	if (c != 2 && c != 3) {
		Con_Print("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv (1));
	if (b == -1) {
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

	for (i = 0; i < (int)(sizeof(keybindings[0])/sizeof(keybindings[0][0])); i++)
		if (keybindings[0][i])
			FS_Printf(f, "bind \"%s\" \"%s\"\n",
					Key_KeynumToString (i), keybindings[0][i]);
	for (j = 1; j < 8; j++)
		for (i = 0; i < (int)(sizeof(keybindings[0])/sizeof(keybindings[0][0])); i++)
			if (keybindings[j][i])
				FS_Printf(f, "in_bind %d \"%s\" \"%s\"\n",
						j, Key_KeynumToString (i), keybindings[j][i]);
}


void
Key_Init (void)
{
	int         i;

	for (i = 0; i < 32; i++) {
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

//
// register our functions
//
	Cmd_AddCommand ("in_bind", Key_In_Bind_f);
	Cmd_AddCommand ("in_unbind", Key_In_Unbind_f);
	Cmd_AddCommand ("in_bindmap", Key_In_Bindmap_f);

	Cmd_AddCommand ("bind", Key_Bind_f);
	Cmd_AddCommand ("unbind", Key_Unbind_f);
	Cmd_AddCommand ("unbindall", Key_Unbindall_f);
}


/*
===================
Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void
Key_Event (int key, char ascii, qboolean down)
{
	const char *bind;

	// get key binding
	bind = keybindings[key_bmap][key];
	if (!bind)
		bind = keybindings[key_bmap2][key];

	if (!down)
	{
		// clear repeat count now that the key is released
		keydown[key] = 0;
		// key up events only generate commands if the game key binding is a button
		// command (leading + sign).  These will occur even in console mode, to
		// keep the character from continuing an action started before a console
		// switch.  Button commands include the kenum as a parameter, so multiple
		// downs can be matched with ups
		if (bind && bind[0] == '+')
			Cbuf_AddText(va("-%s %i\n", bind + 1, key));
		return;
	}

	// from here on we know this is a down event

	// increment key repeat count each time a down is received so that things
	// which want to ignore key repeat can ignore it
	keydown[key] = min(keydown[key] + 1, 2);

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
		if ((key_consoleactive & KEY_CONSOLEACTIVE_USER) || keydown[K_SHIFT])
		{
			Con_ToggleConsole_f ();
			return;
		}
		switch (key_dest)
		{
			case key_message:
				Key_Message (key, ascii);
				break;
			case key_menu:
				MR_Keydown (key, ascii);
				break;
			case key_game:
				MR_ToggleMenu_f ();
				break;
			default:
				if(UI_Callback_IsSlotUsed(key_dest - 3))
					UI_Callback_KeyDown (key, ascii);
				else
					Con_Printf ("Key_Event: Bad key_dest\n");
		}
		return;
	}

	// send function keydowns to interpreter no matter what mode is
	if (key >= K_F1 && key <= K_F12)
	{
		// ignore key repeats on F1-F12 binds
		if (keydown[key] > 1)
			return;
		if (bind)
		{
			// button commands add keynum as a parm
			if (bind[0] == '+')
				Cbuf_AddText (va("%s %i\n", bind, key));
			else
			{
				Cbuf_AddText (bind);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

#if 1
	// ignore binds (other than the above escape/F1-F12 keys) while in console
	if (key_consoleactive)
#else
	// respond to toggleconsole binds while in console unless the pressed key
	// happens to be the color prefix character (such as on German keyboards)
	if (key_consoleactive && (strncmp(bind, "toggleconsole", strlen("toggleconsole")) || ascii == STRING_COLOR_TAG))
#endif
	{
		Key_Console (key, ascii);
		return;
	}

	// anything else is a key press into the game, chat line, or menu
	switch (key_dest)
	{
		case key_message:
			Key_Message (key, ascii);
			break;
		case key_menu:
			MR_Keydown (key, ascii);
			break;
		case key_game:
			// ignore key repeats on binds
			if (bind && keydown[key] == 1)
			{
				// button commands add keynum as a parm
				if (bind[0] == '+')
					Cbuf_AddText (va("%s %i\n", bind, key));
				else
				{
					Cbuf_AddText (bind);
					Cbuf_AddText ("\n");
				}
			}
			break;
		default:
			if(UI_Callback_IsSlotUsed(key_dest - 3))
				UI_Callback_KeyDown (key, ascii);
			else
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
