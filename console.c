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
// console.c

#ifdef NeXT
#include <libc.h>
#endif
#ifndef _MSC_VER
#ifndef __BORLANDC__
#include <unistd.h>
#endif
#endif
#ifdef WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "quakedef.h"

int con_linewidth;

float con_cursorspeed = 4;

#define		CON_TEXTSIZE	131072

// total lines in console scrollback
int con_totallines;
// lines up from bottom to display
int con_backscroll;
// where next message will be printed
int con_current;
// offset in current line for next print
int con_x;
char *con_text = 0;

//seconds
cvar_t con_notifytime = {CVAR_SAVE, "con_notifytime","3"};
cvar_t logfile = {0, "logfile","0"};

#define	NUM_CON_TIMES 4
// realtime time the line was generated for transparent notify lines
float con_times[NUM_CON_TIMES];

int con_vislines;

qboolean con_debuglog;

#define MAXCMDLINE	256
extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;
extern int key_insert;


qboolean con_initialized;

mempool_t *console_mempool;

// scan lines to clear for notify lines
int con_notifylines;

extern void M_Menu_Main_f (void);

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	// toggle the 'user wants console' bit
	key_consoleactive ^= KEY_CONSOLEACTIVE_USER;
	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		memset (con_text, ' ', CON_TEXTSIZE);
}


/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int i;

	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	key_dest = key_message;
	team_message = false;
}


/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	team_message = true;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char tbuf[CON_TEXTSIZE];

	width = (vid.conwidth >> 3);

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 80;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con_text, CON_TEXTSIZE);
		memset (con_text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
#define MAXGAMEDIRLEN 1000
	char temp[MAXGAMEDIRLEN+1];
	char *t2 = "/qconsole.log";

	Cvar_RegisterVariable(&logfile);
	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog)
	{
		if (strlen (com_gamedir) < (MAXGAMEDIRLEN - strlen (t2)))
		{
			sprintf (temp, "%s%s", com_gamedir, t2);
			unlink (temp);
		}
		logfile.integer = 1;
	}

	console_mempool = Mem_AllocPool("console");
	con_text = Mem_Alloc(console_mempool, CON_TEXTSIZE);
	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_RegisterVariable (&con_notifytime);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con_x = 0;
	con_current++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (const char *txt)
{
	int y, c, l, mask;
	static int cr;

	con_backscroll = 0;

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound ("misc/talk.wav");
	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}


		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}

	}
}


/*
================
Con_DebugLog
================
*/
void Con_DebugLog(const char *file, const char *fmt, ...)
{
    va_list argptr;
    FILE* fd;

    fd = fopen(file, "at");
    va_start(argptr, fmt);
    vfprintf (fd, fmt, argptr);
    va_end(argptr);
    fclose(fd);
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
// LordHavoc: increased from 4096 to 16384
#define	MAXPRINTMSG	16384
// FIXME: make a buffer size safe vsprintf?
void Con_Printf (const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

// also echo to debugging console
	Sys_Printf ("%s", msg);

// log all messages to file
	if (con_debuglog)
	{
		// can't use va() here because it might overwrite other important things
		char logname[MAX_OSPATH];
		sprintf(logname, "%s/qconsole.log", com_gamedir);
		Con_DebugLog(logname, "%s", msg);
	}

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		return;		// no graphics mode

// write it to the scrollable buffer
	Con_Print (msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.integer)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge

Modified by EvilTypeGuy eviltypeguy@qeradiant.com
================
*/
void Con_DrawInput (void)
{
	char editlinecopy[256], *text;

	if (!key_consoleactive)
		return;		// don't draw anything

	text = strcpy(editlinecopy, key_lines[edit_line]);

	// Advanced Console Editing by Radix radix@planetquake.com
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	// use strlen of edit_line instead of key_linepos to allow editing
	// of early characters w/o erasing

	// add the cursor frame
	if ((int)(realtime*con_cursorspeed) & 1)		// cursor is visible
		text[key_linepos] = 11 + 130 * key_insert;	// either solid or triangle facing right

	text[key_linepos + 1] = 0;

	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	DrawQ_String(0, con_vislines - 16, text, con_linewidth, 8, 8, 1, 1, 1, 1, 0);

	// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	float	time;
	extern char chat_buffer[];
	char	temptext[256];

	v = 0;
	for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines)*con_linewidth;

		clearnotify = 0;

		DrawQ_String(0, v, text, con_linewidth, 8, 8, 1, 1, 1, 1, 0);

		v += 8;
	}


	if (key_dest == key_message)
	{
		clearnotify = 0;

		x = 0;

		// LordHavoc: speedup, and other improvements
		if (team_message)
			sprintf(temptext, "say_team:%s%c", chat_buffer, (int) 10+((int)(realtime*con_cursorspeed)&1));
		else
			sprintf(temptext, "say:%s%c", chat_buffer, (int) 10+((int)(realtime*con_cursorspeed)&1));
		while (strlen(temptext) >= con_linewidth)
		{
			DrawQ_String (0, v, temptext, con_linewidth, 8, 8, 1, 1, 1, 1, 0);
			strcpy(temptext, &temptext[con_linewidth]);
			v += 8;
		}
		if (strlen(temptext) > 0)
		{
			DrawQ_String (0, v, temptext, 0, 8, 8, 1, 1, 1, 1, 0);
			v += 8;
		}
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
extern cvar_t scr_conalpha;
extern char engineversion[40];
void Con_DrawConsole (int lines)
{
	int i, y, rows, j;
	char *text;

	if (lines <= 0)
		return;

// draw the background
	DrawQ_Pic(0, lines - vid.conheight, "gfx/conback", vid.conwidth, vid.conheight, 1, 1, 1, scr_conalpha.value * lines / vid.conheight, 0);
	DrawQ_String(vid.conwidth - strlen(engineversion) * 8 - 8, lines - 8, engineversion, 0, 8, 8, 1, 0, 0, 1, 0);

// draw the text
	con_vislines = lines;

	rows = (lines-16)>>3;		// rows of text to draw
	y = lines - 16 - (rows<<3);	// may start slightly negative

	for (i = con_current - rows + 1;i <= con_current;i++, y += 8)
	{
		j = max(i - con_backscroll, 0);
		text = con_text + (j % con_totallines)*con_linewidth;

		DrawQ_String(0, y, text, con_linewidth, 8, 8, 1, 1, 1, 1, 0);
	}

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

*/
void Con_DisplayList(const char **list)
{
	int i = 0, pos = 0, len = 0, maxlen = 0, width = (con_linewidth - 4);
	const char **walk = list;

	while (*walk) {
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen(*list);
		if (pos + maxlen >= width) {
			Con_Printf("\n");
			pos = 0;
		}

		Con_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Con_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Con_Printf("\n\n");
}

/*
	Con_CompleteCommandLine

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
void Con_CompleteCommandLine (void)
{
	const char *cmd = "", *s;
	const char **list[3] = {0, 0, 0};
	int c, v, a, i, cmd_len;

	s = key_lines[edit_line] + 1;
	// Count number of possible matches
	c = Cmd_CompleteCountPossible(s);
	v = Cvar_CompleteCountPossible(s);
	a = Cmd_CompleteAliasCountPossible(s);

	if (!(c + v + a))	// No possible matches
		return;

	if (c + v + a == 1) {
		if (c)
			list[0] = Cmd_CompleteBuildList(s);
		else if (v)
			list[0] = Cvar_CompleteBuildList(s);
		else
			list[0] = Cmd_CompleteAliasBuildList(s);
		cmd = *list[0];
		cmd_len = strlen (cmd);
	} else {
		if (c)
			cmd = *(list[0] = Cmd_CompleteBuildList(s));
		if (v)
			cmd = *(list[1] = Cvar_CompleteBuildList(s));
		if (a)
			cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));

		cmd_len = strlen (s);
		do {
			for (i = 0; i < 3; i++) {
				char ch = cmd[cmd_len];
				const char **l = list[i];
				if (l) {
					while (*l && (*l)[cmd_len] == ch)
						l++;
					if (*l)
						break;
				}
			}
			if (i == 3)
				cmd_len++;
		} while (i == 3);
		// 'quakebar'
		Con_Printf("\n\35");
		for (i = 0; i < con_linewidth - 4; i++)
			Con_Printf("\36");
		Con_Printf("\37\n");

		// Print Possible Commands
		if (c) {
			Con_Printf("%i possible command%s\n", c, (c > 1) ? "s: " : ":");
			Con_DisplayList(list[0]);
		}

		if (v) {
			Con_Printf("%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
			Con_DisplayList(list[1]);
		}

		if (a) {
			Con_Printf("%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
			Con_DisplayList(list[2]);
		}
	}

	if (cmd) {
		strncpy(key_lines[edit_line] + 1, cmd, cmd_len);
		key_linepos = cmd_len + 1;
		if (c + v + a == 1) {
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
		}
		key_lines[edit_line][key_linepos] = 0;
	}
	for (i = 0; i < 3; i++)
		if (list[i])
			Mem_Free((void *)list[i]);
}

