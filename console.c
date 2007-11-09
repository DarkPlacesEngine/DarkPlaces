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

#if !defined(WIN32) || defined(__MINGW32__)
# include <unistd.h>
#endif
#include <time.h>
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
char con_text[CON_TEXTSIZE];

cvar_t con_notifytime = {CVAR_SAVE, "con_notifytime","3", "how long notify lines last, in seconds"};
cvar_t con_notify = {CVAR_SAVE, "con_notify","4", "how many notify lines to show (0-32)"};
cvar_t con_textsize = {CVAR_SAVE, "con_textsize","8", "console text size in virtual 2D pixels"};


cvar_t sys_specialcharactertranslation = {0, "sys_specialcharactertranslation", "1", "terminal console conchars to ASCII translation (set to 0 if your conchars.tga is for an 8bit character set or if you want raw output)"};
#ifdef WIN32
cvar_t sys_colortranslation = {0, "sys_colortranslation", "0", "terminal console color translation (supported values: 0 = strip color codes, 1 = translate to ANSI codes, 2 = no translation)"};
#else
cvar_t sys_colortranslation = {0, "sys_colortranslation", "1", "terminal console color translation (supported values: 0 = strip color codes, 1 = translate to ANSI codes, 2 = no translation)"};
#endif


cvar_t con_nickcompletion = {CVAR_SAVE, "con_nickcompletion", "1", "tab-complete nicks in console and message input"};
cvar_t con_nickcompletion_flags = {CVAR_SAVE, "con_nickcompletion_flags", "11", "Bitfield: "
				   "0: add nothing after completion. "
				   "1: add the last color after completion. "
				   "2: add a quote when starting a quote instead of the color. "
				   "4: will replace 1, will force color, even after a quote. "
				   "8: ignore non-alphanumerics. "
				   "16: ignore spaces. "};
#define NICKS_ADD_COLOR 1
#define NICKS_ADD_QUOTE 2
#define NICKS_FORCE_COLOR 4
#define NICKS_ALPHANUMERICS_ONLY 8
#define NICKS_NO_SPACES 16

#define MAX_NOTIFYLINES 32
// cl.time time the line was generated for transparent notify lines
float con_times[MAX_NOTIFYLINES];

int con_vislines;

qboolean con_initialized;

// used for server replies to rcon command
qboolean rcon_redirect = false;
int rcon_redirect_bufferpos = 0;
char rcon_redirect_buffer[1400];


/*
==============================================================================

LOGGING

==============================================================================
*/

cvar_t log_file = {0, "log_file","", "filename to log messages to"};
cvar_t log_dest_udp = {0, "log_dest_udp","", "UDP address to log messages to (in QW rcon compatible format); multiple destinations can be separated by spaces; DO NOT SPECIFY DNS NAMES HERE"};
char log_dest_buffer[1400]; // UDP packet
size_t log_dest_buffer_pos;
qboolean log_dest_buffer_appending;
char crt_log_file [MAX_OSPATH] = "";
qfile_t* logfile = NULL;

unsigned char* logqueue = NULL;
size_t logq_ind = 0;
size_t logq_size = 0;

void Log_ConPrint (const char *msg);

/*
====================
Log_DestBuffer_Init
====================
*/
static void Log_DestBuffer_Init()
{
	memcpy(log_dest_buffer, "\377\377\377\377n", 5); // QW rcon print
	log_dest_buffer_pos = 5;
}

/*
====================
Log_DestBuffer_Flush
====================
*/
void Log_DestBuffer_Flush()
{
	lhnetaddress_t log_dest_addr;
	lhnetsocket_t *log_dest_socket;
	const char *s = log_dest_udp.string;
	qboolean have_opened_temp_sockets = false;
	if(s) if(log_dest_buffer_pos > 5)
	{
		++log_dest_buffer_appending;
		log_dest_buffer[log_dest_buffer_pos++] = 0;

		if(!NetConn_HaveServerPorts() && !NetConn_HaveClientPorts()) // then temporarily open one
 		{
			have_opened_temp_sockets = true;
			NetConn_OpenServerPorts(true);
		}

		while(COM_ParseToken_Console(&s))
			if(LHNETADDRESS_FromString(&log_dest_addr, com_token, 26000))
			{
				log_dest_socket = NetConn_ChooseClientSocketForAddress(&log_dest_addr);
				if(!log_dest_socket)
					log_dest_socket = NetConn_ChooseServerSocketForAddress(&log_dest_addr);
				if(log_dest_socket)
					NetConn_WriteString(log_dest_socket, log_dest_buffer, &log_dest_addr);
			}

		if(have_opened_temp_sockets)
			NetConn_CloseServerPorts();
		--log_dest_buffer_appending;
	}
	log_dest_buffer_pos = 0;
}

/*
====================
Log_Timestamp
====================
*/
const char* Log_Timestamp (const char *desc)
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Wed Jun 30 21:49:08 1993");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%a %b %d %H:%M:%S %Y", crt_tm);

	if (desc != NULL)
		dpsnprintf (timestamp, sizeof (timestamp), "====== %s (%s) ======\n", desc, timestring);
	else
		dpsnprintf (timestamp, sizeof (timestamp), "====== %s ======\n", timestring);

	return timestamp;
}


/*
====================
Log_Open
====================
*/
void Log_Open (void)
{
	if (logfile != NULL || log_file.string[0] == '\0')
		return;

	logfile = FS_Open (log_file.string, "ab", false, false);
	if (logfile != NULL)
	{
		strlcpy (crt_log_file, log_file.string, sizeof (crt_log_file));
		FS_Print (logfile, Log_Timestamp ("Log started"));
	}
}


/*
====================
Log_Close
====================
*/
void Log_Close (void)
{
	if (logfile == NULL)
		return;

	FS_Print (logfile, Log_Timestamp ("Log stopped"));
	FS_Print (logfile, "\n");
	FS_Close (logfile);

	logfile = NULL;
	crt_log_file[0] = '\0';
}


/*
====================
Log_Start
====================
*/
void Log_Start (void)
{
	size_t pos;
	size_t n;
	Log_Open ();

	// Dump the contents of the log queue into the log file and free it
	if (logqueue != NULL)
	{
		unsigned char *temp = logqueue;
		logqueue = NULL;
		if(logq_ind != 0)
		{
			if (logfile != NULL)
				FS_Write (logfile, temp, logq_ind);
			if(*log_dest_udp.string)
			{
				for(pos = 0; pos < logq_ind; )
				{
					if(log_dest_buffer_pos == 0)
						Log_DestBuffer_Init();
					n = min(sizeof(log_dest_buffer) - log_dest_buffer_pos - 1, logq_ind - pos);
					memcpy(log_dest_buffer + log_dest_buffer_pos, temp + pos, n);
					log_dest_buffer_pos += n;
					Log_DestBuffer_Flush();
					pos += n;
				}
			}
		}
		Mem_Free (temp);
		logq_ind = 0;
		logq_size = 0;
	}
}


/*
================
Log_ConPrint
================
*/
void Log_ConPrint (const char *msg)
{
	static qboolean inprogress = false;

	// don't allow feedback loops with memory error reports
	if (inprogress)
		return;
	inprogress = true;

	// Until the host is completely initialized, we maintain a log queue
	// to store the messages, since the log can't be started before
	if (logqueue != NULL)
	{
		size_t remain = logq_size - logq_ind;
		size_t len = strlen (msg);

		// If we need to enlarge the log queue
		if (len > remain)
		{
			size_t factor = ((logq_ind + len) / logq_size) + 1;
			unsigned char* newqueue;

			logq_size *= factor;
			newqueue = (unsigned char *)Mem_Alloc (tempmempool, logq_size);
			memcpy (newqueue, logqueue, logq_ind);
			Mem_Free (logqueue);
			logqueue = newqueue;
			remain = logq_size - logq_ind;
		}
		memcpy (&logqueue[logq_ind], msg, len);
		logq_ind += len;

		inprogress = false;
		return;
	}

	// Check if log_file has changed
	if (strcmp (crt_log_file, log_file.string) != 0)
	{
		Log_Close ();
		Log_Open ();
	}

	// If a log file is available
	if (logfile != NULL)
		FS_Print (logfile, msg);

	inprogress = false;
}


/*
================
Log_Printf
================
*/
void Log_Printf (const char *logfilename, const char *fmt, ...)
{
	qfile_t *file;

	file = FS_Open (logfilename, "ab", true, false);
	if (file != NULL)
	{
		va_list argptr;

		va_start (argptr, fmt);
		FS_VPrintf (file, fmt, argptr);
		va_end (argptr);

		FS_Close (file);
	}
}


/*
==============================================================================

CONSOLE

==============================================================================
*/

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
	//if (con_text)
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

	for (i=0 ; i<MAX_NOTIFYLINES ; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	key_dest = key_message;
	chat_team = false;
}


/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	chat_team = true;
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
	float f;
	char tbuf[CON_TEXTSIZE];

	f = bound(1, con_textsize.value, 128);
	if(f != con_textsize.value)
		Cvar_SetValueQuick(&con_textsize, f);
	width = (int)floor(vid_conwidth.value / con_textsize.value);
	width = bound(1, width, CON_TEXTSIZE/4);

	if (width == con_linewidth)
		return;

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

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

//[515]: the simplest command ever
//LordHavoc: not so simple after I made it print usage...
static void Con_Maps_f (void)
{
	if (Cmd_Argc() > 2)
	{
		Con_Printf("usage: maps [mapnameprefix]\n");
		return;
	}
	else if (Cmd_Argc() == 2)
		GetMapList(Cmd_Argv(1), NULL, 0);
	else
		GetMapList("", NULL, 0);
}

void Con_ConDump_f (void)
{
	int i, l;
	qboolean allblankssofar;
	const char *text;
	qfile_t *file;
	char temp[MAX_INPUTLINE+2];
	if (Cmd_Argc() != 2)
	{
		Con_Printf("usage: condump <filename>\n");
		return;
	}
	file = FS_Open(Cmd_Argv(1), "wb", false, false);
	if (!file)
	{
		Con_Printf("condump: unable to write file \"%s\"\n", Cmd_Argv(1));
		return;
	}
	// iterate over the entire console history buffer line by line
	allblankssofar = true;
	for (i = 0;i < con_totallines;i++)
	{
		text = con_text + ((con_current + 1 + i) % con_totallines)*con_linewidth;
		// count the used characters on this line
		for (l = min(con_linewidth, (int)sizeof(temp));l > 0 && text[l-1] == ' ';l--);
		// if not a blank line, begin output
		if (l)
			allblankssofar = false;
		// output the current line to the file
		if (!allblankssofar)
		{
			if (l)
				memcpy(temp, text, l);
			temp[l] = '\n';
			temp[l+1] = 0;
			FS_Print(file, temp);
		}
	}
	FS_Close(file);
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = 80;
	con_totallines = CON_TEXTSIZE / con_linewidth;

	// Allocate a log queue, this will be freed after configs are parsed
	logq_size = MAX_INPUTLINE;
	logqueue = (unsigned char *)Mem_Alloc (tempmempool, logq_size);
	logq_ind = 0;

	Cvar_RegisterVariable (&sys_colortranslation);
	Cvar_RegisterVariable (&sys_specialcharactertranslation);

	Cvar_RegisterVariable (&log_file);
	Cvar_RegisterVariable (&log_dest_udp);

	// support for the classic Quake option
// COMMANDLINEOPTION: Console: -condebug logs console messages to qconsole.log, see also log_file
	if (COM_CheckParm ("-condebug") != 0)
		Cvar_SetQuick (&log_file, "qconsole.log");

	// register our cvars
	Cvar_RegisterVariable (&con_notifytime);
	Cvar_RegisterVariable (&con_notify);
	Cvar_RegisterVariable (&con_textsize);

	// --blub
	Cvar_RegisterVariable (&con_nickcompletion);
	Cvar_RegisterVariable (&con_nickcompletion_flags);

	// register our commands
	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f, "opens or closes the console");
	Cmd_AddCommand ("messagemode", Con_MessageMode_f, "input a chat message to say to everyone");
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f, "input a chat message to say to only your team");
	Cmd_AddCommand ("clear", Con_Clear_f, "clear console history");
	Cmd_AddCommand ("maps", Con_Maps_f, "list information about available maps");
	Cmd_AddCommand ("condump", Con_ConDump_f, "output console history to a file (see also log_file)");

	con_initialized = true;
	Con_Print("Console initialized.\n");
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	if (con_backscroll)
		con_backscroll++;

	con_x = 0;
	con_current++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/*
================
Con_PrintToHistory

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be displayed
If no console is visible, the notify window will pop up.
================
*/
void Con_PrintToHistory(const char *txt, int mask)
{
	int y, c, l;
	static int cr;

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
			{
				if (con_notify.integer < 0)
					Cvar_SetValueQuick(&con_notify, 0);
				if (con_notify.integer > MAX_NOTIFYLINES)
					Cvar_SetValueQuick(&con_notify, MAX_NOTIFYLINES);
				if (con_notify.integer > 0)
					con_times[con_current % con_notify.integer] = cl.time;
			}
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

/* The translation table between the graphical font and plain ASCII  --KB */
static char qfont_table[256] = {
	'\0', '#',  '#',  '#',  '#',  '.',  '#',  '#',
	'#',  9,    10,   '#',  ' ',  13,   '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<',

	'<',  '=',  '>',  '#',  '#',  '.',  '#',  '#',
	'#',  '#',  ' ',  '#',  ' ',  '>',  '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<'
};

/*
================
Con_Rcon_AddChar

Adds a character to the rcon buffer
================
*/
void Con_Rcon_AddChar(char c)
{
	if(log_dest_buffer_appending)
		return;
	++log_dest_buffer_appending;

	// if this print is in response to an rcon command, add the character
	// to the rcon redirect buffer

	if (rcon_redirect && rcon_redirect_bufferpos < (int)sizeof(rcon_redirect_buffer) - 1)
		rcon_redirect_buffer[rcon_redirect_bufferpos++] = c;
	else if(*log_dest_udp.string) // don't duplicate rcon command responses here, these are sent another way
	{
		if(log_dest_buffer_pos == 0)
			Log_DestBuffer_Init();
		log_dest_buffer[log_dest_buffer_pos++] = c;
		if(log_dest_buffer_pos >= sizeof(log_dest_buffer) - 1) // minus one, to allow for terminating zero
			Log_DestBuffer_Flush();
	}
	else
		log_dest_buffer_pos = 0;

	--log_dest_buffer_appending;
}

/*
================
Con_Print

Prints to all appropriate console targets, and adds timestamps
================
*/
extern cvar_t timestamps;
extern cvar_t timeformat;
extern qboolean sys_nostdout;
void Con_Print(const char *msg)
{
	static int mask = 0;
	static int index = 0;
	static char line[MAX_INPUTLINE];

	for (;*msg;msg++)
	{
		Con_Rcon_AddChar(*msg);
		// if this is the beginning of a new line, print timestamp
		if (index == 0)
		{
			const char *timestamp = timestamps.integer ? Sys_TimeString(timeformat.string) : "";
			// reset the color
			// FIXME: 1. perhaps we should use a terminal system 2. use a constant instead of 7!
			line[index++] = STRING_COLOR_TAG;
			// assert( STRING_COLOR_DEFAULT < 10 )
			line[index++] = STRING_COLOR_DEFAULT + '0';
			// special color codes for chat messages must always come first
			// for Con_PrintToHistory to work properly
			if (*msg == 1 || *msg == 2)
			{
				// play talk wav
				if (*msg == 1)
				{
					if (msg[1] == '(' && cl.foundtalk2wav)
						S_LocalSound ("sound/misc/talk2.wav");
					else
						S_LocalSound ("sound/misc/talk.wav");
				}
				line[index++] = STRING_COLOR_TAG;
				line[index++] = '3';
				msg++;
				Con_Rcon_AddChar(*msg);
			}
			// store timestamp
			for (;*timestamp;index++, timestamp++)
				if (index < (int)sizeof(line) - 2)
					line[index] = *timestamp;
		}
		// append the character
		line[index++] = *msg;
		// if this is a newline character, we have a complete line to print
		if (*msg == '\n' || index >= (int)sizeof(line) / 2)
		{
			// terminate the line
			line[index] = 0;
			// send to log file
			Log_ConPrint(line);
			// send to scrollable buffer
			if (con_initialized && cls.state != ca_dedicated)
				Con_PrintToHistory(line, mask);
			// send to terminal or dedicated server window
			if (!sys_nostdout)
			{
				unsigned char *p;
				if(sys_specialcharactertranslation.integer)
				{
					for (p = (unsigned char *) line;*p; p++)
						*p = qfont_table[*p];
				}

				if(sys_colortranslation.integer == 1) // ANSI
				{
					static char printline[MAX_INPUTLINE * 4 + 3];
						// 2 can become 7 bytes, rounding that up to 8, and 3 bytes are added at the end
						// a newline can transform into four bytes, but then prevents the three extra bytes from appearing
					int lastcolor = 0;
					const char *in;
					char *out;
					for(in = line, out = printline; *in; ++in)
					{
						switch(*in)
						{
							case STRING_COLOR_TAG:
								switch(in[1])
								{
									case STRING_COLOR_TAG:
										++in;
										*out++ = STRING_COLOR_TAG;
										break;
									case '0':
									case '7':
										// normal color
										++in;
										if(lastcolor == 0) break; else lastcolor = 0;
										*out++ = 0x1B; *out++ = '['; *out++ = 'm';
										break;
									case '1':
										// light red
										++in;
										if(lastcolor == 1) break; else lastcolor = 1;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '1'; *out++ = 'm';
										break;
									case '2':
										// light green
										++in;
										if(lastcolor == 2) break; else lastcolor = 2;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '2'; *out++ = 'm';
										break;
									case '3':
										// yellow
										++in;
										if(lastcolor == 3) break; else lastcolor = 3;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '3'; *out++ = 'm';
										break;
									case '4':
										// light blue
										++in;
										if(lastcolor == 4) break; else lastcolor = 4;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '4'; *out++ = 'm';
										break;
									case '5':
										// light cyan
										++in;
										if(lastcolor == 5) break; else lastcolor = 5;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '6'; *out++ = 'm';
										break;
									case '6':
										// light magenta
										++in;
										if(lastcolor == 6) break; else lastcolor = 6;
										*out++ = 0x1B; *out++ = '['; *out++ = '1'; *out++ = ';'; *out++ = '3'; *out++ = '5'; *out++ = 'm';
										break;
									// 7 handled above
									case '8':
									case '9':
										// bold normal color
										++in;
										if(lastcolor == 8) break; else lastcolor = 8;
										*out++ = 0x1B; *out++ = '['; *out++ = '0'; *out++ = ';'; *out++ = '1'; *out++ = 'm';
										break;
									default:
										*out++ = STRING_COLOR_TAG;
										break;
								}
								break;
							case '\n':
								if(lastcolor != 0)
								{
									*out++ = 0x1B; *out++ = '['; *out++ = 'm';
									lastcolor = 0;
								}
								*out++ = *in;
								break;
							default:
								*out++ = *in;
								break;
						}
					}
					if(lastcolor != 0)
					{
						*out++ = 0x1B;
						*out++ = '[';
						*out++ = 'm';
					}
					*out++ = 0;
					Sys_PrintToTerminal(printline);
				}
				else if(sys_colortranslation.integer == 2) // Quake
				{
					Sys_PrintToTerminal(line);
				}
				else // strip
				{
					static char printline[MAX_INPUTLINE]; // it can only get shorter here
					const char *in;
					char *out;
					for(in = line, out = printline; *in; ++in)
					{
						switch(*in)
						{
							case STRING_COLOR_TAG:
								switch(in[1])
								{
									case STRING_COLOR_TAG:
										++in;
										*out++ = STRING_COLOR_TAG;
										break;
									case '0':
									case '1':
									case '2':
									case '3':
									case '4':
									case '5':
									case '6':
									case '7':
									case '8':
									case '9':
										++in;
										break;
									default:
										*out++ = STRING_COLOR_TAG;
										break;
								}
								break;
							default:
								*out++ = *in;
								break;
						}
					}
					*out++ = 0;
					Sys_PrintToTerminal(printline);
				}
			}
			// empty the line buffer
			index = 0;
		}
	}
}


/*
================
Con_Printf

Prints to all appropriate console targets
================
*/
void Con_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Con_Print(msg);
}

/*
================
Con_DPrint

A Con_Print that only shows up if the "developer" cvar is set
================
*/
void Con_DPrint(const char *msg)
{
	if (!developer.integer)
		return;			// don't confuse non-developers with techie stuff...
	Con_Print(msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	if (!developer.integer)
		return;			// don't confuse non-developers with techie stuff...

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Con_Print(msg);
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
	int		y;
	int		i;
	char editlinecopy[MAX_INPUTLINE+1], *text;

	if (!key_consoleactive)
		return;		// don't draw anything

	strlcpy(editlinecopy, key_lines[edit_line], sizeof(editlinecopy));
	text = editlinecopy;

	// Advanced Console Editing by Radix radix@planetquake.com
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	// use strlen of edit_line instead of key_linepos to allow editing
	// of early characters w/o erasing

	y = (int)strlen(text);

// fill out remainder with spaces
	for (i = y; i < (int)sizeof(editlinecopy)-1; i++)
		text[i] = ' ';

	// add the cursor frame
	if ((int)(realtime*con_cursorspeed) & 1)		// cursor is visible
		text[key_linepos] = 11 + 130 * key_insert;	// either solid or triangle facing right

//	text[key_linepos + 1] = 0;

	// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	DrawQ_String(0, con_vislines - con_textsize.value*2, text, con_linewidth, con_textsize.value, con_textsize.value, 1.0, 1.0, 1.0, 1.0, 0, NULL, false );

	// remove cursor
//	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	float	x, v;
	char	*text;
	int		i, stop;
	float	time;
	char	temptext[MAX_INPUTLINE];
	int colorindex = -1; //-1 for default

	if (con_notify.integer < 0)
		Cvar_SetValueQuick(&con_notify, 0);
	if (con_notify.integer > MAX_NOTIFYLINES)
		Cvar_SetValueQuick(&con_notify, MAX_NOTIFYLINES);
	if (gamemode == GAME_TRANSFUSION)
		v = 8;
	else
		v = 0;
	// make a copy of con_current here so that we can't get in a runaway loop printing new messages while drawing the notify text
	stop = con_current;
	for (i= stop-con_notify.integer+1 ; i<=stop ; i++)
	{

		if (i < 0)
			continue;
		time = con_times[i % con_notify.integer];
		if (time == 0)
			continue;
		time = cl.time - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines)*con_linewidth;

		if (gamemode == GAME_NEXUIZ) {
			int chars = 0;
			int finalchars = 0;
			int j;

			// count up to the last non-whitespace, and ignore color codes
			for (j = 0;j < con_linewidth && text[j];j++)
			{
				if (text[j] == STRING_COLOR_TAG && (text[j+1] >= '0' && text[j+1] <= '9'))
				{
					j++;
					continue;
				}
				chars++;
				if (text[j] == ' ')
					continue;
				finalchars = chars;
			}
			// center the line using the calculated width
			x = (vid_conwidth.integer - finalchars * con_textsize.value) * 0.5;
		} else
			x = 0;

		DrawQ_String( x, v, text, con_linewidth, con_textsize.value, con_textsize.value, 1.0, 1.0, 1.0, 1.0, 0, &colorindex, false );

		v += con_textsize.value;
	}


	if (key_dest == key_message)
	{
		int colorindex = -1;

		x = 0;

		// LordHavoc: speedup, and other improvements
		if (chat_team)
			sprintf(temptext, "say_team:%s%c", chat_buffer, (int) 10+((int)(realtime*con_cursorspeed)&1));
		else
			sprintf(temptext, "say:%s%c", chat_buffer, (int) 10+((int)(realtime*con_cursorspeed)&1));
		while ((int)strlen(temptext) >= con_linewidth)
		{
			DrawQ_String( 0, v, temptext, con_linewidth, con_textsize.value, con_textsize.value, 1.0, 1.0, 1.0, 1.0, 0, &colorindex, false );
			strlcpy(temptext, &temptext[con_linewidth], sizeof(temptext));
			v += con_textsize.value;
		}
		if (strlen(temptext) > 0)
		{
			DrawQ_String( 0, v, temptext, 0, con_textsize.value, con_textsize.value, 1.0, 1.0, 1.0, 1.0, 0, &colorindex, false );
			v += con_textsize.value;
		}
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines)
{
	int i, rows, j, stop;
	float y;
	char *text;
	int colorindex = -1;

	if (lines <= 0)
		return;

// draw the background
	DrawQ_Pic(0, lines - vid_conheight.integer, scr_conbrightness.value >= 0.01f ? Draw_CachePic("gfx/conback", true) : NULL, vid_conwidth.integer, vid_conheight.integer, scr_conbrightness.value, scr_conbrightness.value, scr_conbrightness.value, scr_conalpha.value, 0);
	DrawQ_String(vid_conwidth.integer - strlen(engineversion) * con_textsize.value - con_textsize.value, lines - con_textsize.value, engineversion, 0, con_textsize.value, con_textsize.value, 1, 0, 0, 1, 0, NULL, true);

// draw the text
	con_vislines = lines;

	rows = (int)ceil((lines/con_textsize.value)-2);		// rows of text to draw
	y = lines - (rows+2)*con_textsize.value;	// may start slightly negative

	// make a copy of con_current here so that we can't get in a runaway loop printing new messages while drawing the notify text
	stop = con_current;
	for (i = stop - rows + 1;i <= stop;i++, y += con_textsize.value)
	{
		j = max(i - con_backscroll, 0);
		text = con_text + (j % con_totallines)*con_linewidth;

		DrawQ_String( 0, y, text, con_linewidth, con_textsize.value, con_textsize.value, 1.0, 1.0, 1.0, 1.0, 0, &colorindex, false );
	}

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
GetMapList

Made by [515]
Prints not only map filename, but also
its format (q1/q2/q3/hl) and even its message
*/
//[515]: here is an ugly hack.. two gotos... oh my... *but it works*
//LordHavoc: rewrote bsp type detection, added mcbsp support and rewrote message extraction to do proper worldspawn parsing
//LordHavoc: added .ent file loading, and redesigned error handling to still try the .ent file even if the map format is not recognized, this also eliminated one goto
//LordHavoc: FIXME: man this GetMapList is STILL ugly code even after my cleanups...
qboolean GetMapList (const char *s, char *completedname, int completednamebufferlength)
{
	fssearch_t	*t;
	char		message[64];
	int			i, k, max, p, o, min;
	unsigned char *len;
	qfile_t		*f;
	unsigned char buf[1024];

	sprintf(message, "maps/%s*.bsp", s);
	t = FS_Search(message, 1, true);
	if(!t)
		return false;
	if (t->numfilenames > 1)
		Con_Printf("^1 %i maps found :\n", t->numfilenames);
	len = (unsigned char *)Z_Malloc(t->numfilenames);
	min = 666;
	for(max=i=0;i<t->numfilenames;i++)
	{
		k = (int)strlen(t->filenames[i]);
		k -= 9;
		if(max < k)
			max = k;
		else
		if(min > k)
			min = k;
		len[i] = k;
	}
	o = (int)strlen(s);
	for(i=0;i<t->numfilenames;i++)
	{
		int lumpofs = 0, lumplen = 0;
		char *entities = NULL;
		const char *data = NULL;
		char keyname[64];
		char entfilename[MAX_QPATH];
		strlcpy(message, "^1**ERROR**^7", sizeof(message));
		p = 0;
		f = FS_Open(t->filenames[i], "rb", true, false);
		if(f)
		{
			memset(buf, 0, 1024);
			FS_Read(f, buf, 1024);
			if (!memcmp(buf, "IBSP", 4))
			{
				p = LittleLong(((int *)buf)[1]);
				if (p == Q3BSPVERSION)
				{
					q3dheader_t *header = (q3dheader_t *)buf;
					lumpofs = LittleLong(header->lumps[Q3LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[Q3LUMP_ENTITIES].filelen);
				}
				else if (p == Q2BSPVERSION)
				{
					q2dheader_t *header = (q2dheader_t *)buf;
					lumpofs = LittleLong(header->lumps[Q2LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[Q2LUMP_ENTITIES].filelen);
				}
			}
			else if (!memcmp(buf, "MCBSPpad", 8))
			{
				p = LittleLong(((int *)buf)[2]);
				if (p == MCBSPVERSION)
				{
					int numhulls = LittleLong(((int *)buf)[3]);
					lumpofs = LittleLong(((int *)buf)[3 + numhulls + LUMP_ENTITIES*2+0]);
					lumplen = LittleLong(((int *)buf)[3 + numhulls + LUMP_ENTITIES*2+1]);
				}
			}
			else if((p = LittleLong(((int *)buf)[0])) == BSPVERSION || p == 30)
			{
				dheader_t *header = (dheader_t *)buf;
				lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
				lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
			}
			else
				p = 0;
			strlcpy(entfilename, t->filenames[i], sizeof(entfilename));
			memcpy(entfilename + strlen(entfilename) - 4, ".ent", 5);
			entities = (char *)FS_LoadFile(entfilename, tempmempool, true, NULL);
			if (!entities && lumplen >= 10)
			{
				FS_Seek(f, lumpofs, SEEK_SET);
				entities = (char *)Z_Malloc(lumplen + 1);
				FS_Read(f, entities, lumplen);
			}
			if (entities)
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				message[0] = 0;
				data = entities;
				for (;;)
				{
					int l;
					if (!COM_ParseToken_Simple(&data, false, false))
						break;
					if (com_token[0] == '{')
						continue;
					if (com_token[0] == '}')
						break;
					// skip leading whitespace
					for (k = 0;com_token[k] && com_token[k] <= ' ';k++);
					for (l = 0;l < (int)sizeof(keyname) - 1 && com_token[k+l] && com_token[k+l] > ' ';l++)
						keyname[l] = com_token[k+l];
					keyname[l] = 0;
					if (!COM_ParseToken_Simple(&data, false, false))
						break;
					if (developer.integer >= 100)
						Con_Printf("key: %s %s\n", keyname, com_token);
					if (!strcmp(keyname, "message"))
					{
						// get the message contents
						strlcpy(message, com_token, sizeof(message));
						break;
					}
				}
			}
		}
		if (entities)
			Z_Free(entities);
		if(f)
			FS_Close(f);
		*(t->filenames[i]+len[i]+5) = 0;
		switch(p)
		{
		case Q3BSPVERSION:	strlcpy((char *)buf, "Q3", sizeof(buf));break;
		case Q2BSPVERSION:	strlcpy((char *)buf, "Q2", sizeof(buf));break;
		case BSPVERSION:	strlcpy((char *)buf, "Q1", sizeof(buf));break;
		case MCBSPVERSION:	strlcpy((char *)buf, "MC", sizeof(buf));break;
		case 30:			strlcpy((char *)buf, "HL", sizeof(buf));break;
		default:			strlcpy((char *)buf, "??", sizeof(buf));break;
		}
		Con_Printf("%16s (%s) %s\n", t->filenames[i]+5, buf, message);
	}
	Con_Print("\n");
	for(p=o;p<min;p++)
	{
		k = *(t->filenames[0]+5+p);
		if(k == 0)
			goto endcomplete;
		for(i=1;i<t->numfilenames;i++)
			if(*(t->filenames[i]+5+p) != k)
				goto endcomplete;
	}
endcomplete:
	if(p > o && completedname && completednamebufferlength > 0)
	{
		memset(completedname, 0, completednamebufferlength);
		memcpy(completedname, (t->filenames[0]+5), min(p, completednamebufferlength - 1));
	}
	Z_Free(len);
	FS_FreeSearch(t);
	return p > o;
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
		len = (int)strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = (int)strlen(*list);
		if (pos + maxlen >= width) {
			Con_Print("\n");
			pos = 0;
		}

		Con_Print(*list);
		for (i = 0; i < (maxlen - len); i++)
			Con_Print(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Con_Print("\n\n");
}

/* Nicks_CompleteCountPossible

   Count the number of possible nicks to complete
 */
//qboolean COM_StringDecolorize(const char *in, size_t size_in, char *out, size_t size_out, qboolean escape_carets);
void SanitizeString(char *in, char *out)
{
	while(*in)
	{
		if(*in == STRING_COLOR_TAG)
		{
			++in;
			if(!*in)
			{
				out[0] = STRING_COLOR_TAG;
				out[1] = 0;
				return;
			} else if(*in >= '0' && *in <= '9')
			{
				++in;
				if(!*in) // end
				{
					*out = 0;
					return;
				} else if (*in == STRING_COLOR_TAG)
					continue;
			} else if (*in != STRING_COLOR_TAG) {
				--in;
			}
		}
		*out = qfont_table[*(unsigned char*)in];
		++in;
		++out;
	}
	*out = 0;
}
int Sbar_GetPlayer (int index); // <- safety?

// Now it becomes TRICKY :D --blub
static char Nicks_list[MAX_SCOREBOARD][MAX_SCOREBOARDNAME];	// contains the nicks with colors and all that
static char Nicks_sanlist[MAX_SCOREBOARD][MAX_SCOREBOARDNAME];	// sanitized list for completion when there are other possible matches.
// means: when somebody uses a cvar's name as his name, we won't ever get his colors in there...
static int Nicks_offset[MAX_SCOREBOARD]; // when nicks use a space, we need this to move the completion list string starts to avoid invalid memcpys
static int Nicks_matchpos;

// co against <<:BLASTER:>> is true!?
int Nicks_strncasecmp_nospaces(char *a, char *b, unsigned int a_len)
{
	while(a_len)
	{
		if(tolower(*a) == tolower(*b))
		{
			if(*a == 0)
				return 0;
			--a_len;
			++a;
			++b;
			continue;
		}
		if(!*a)
			return -1;
		if(!*b)
			return 1;
		if(*a == ' ')
			return (*a < *b) ? -1 : 1;
		if(*b == ' ')
			++b;
		else
			return (*a < *b) ? -1 : 1;
	}
	return 0;
}
int Nicks_strncasecmp(char *a, char *b, unsigned int a_len)
{
	char space_char;
	if(!(con_nickcompletion_flags.integer & NICKS_ALPHANUMERICS_ONLY))
	{
		if(con_nickcompletion_flags.integer & NICKS_NO_SPACES)
			return Nicks_strncasecmp_nospaces(a, b, a_len);
		return strncasecmp(a, b, a_len);
	}
	
	space_char = (con_nickcompletion_flags.integer & NICKS_NO_SPACES) ? 'a' : ' ';
	
	// ignore non alphanumerics of B
	// if A contains a non-alphanumeric, B must contain it as well though!
	while(a_len)
	{
		qboolean alnum_a, alnum_b;
		
		if(tolower(*a) == tolower(*b))
		{
			if(*a == 0) // end of both strings, they're equal
				return 0;
			--a_len;
			++a;
			++b;
			continue;
		}
		// not equal, end of one string?
		if(!*a)
			return -1;
		if(!*b)
			return 1;
		// ignore non alphanumerics
		alnum_a = ( (*a >= 'a' && *a <= 'z') || (*a >= 'A' && *a <= 'Z') || (*a >= '0' && *a <= '9') || *a == space_char);
		alnum_b = ( (*b >= 'a' && *b <= 'z') || (*b >= 'A' && *b <= 'Z') || (*b >= '0' && *b <= '9') || *b == space_char);
		if(!alnum_a) // b must contain this
			return (*a < *b) ? -1 : 1;
		if(!alnum_b)
			++b;
		// otherwise, both are alnum, they're just not equal, return the appropriate number
		else
			return (*a < *b) ? -1 : 1;
	}
	return 0;
}

int Nicks_CompleteCountPossible(char *line, int pos, char *s, qboolean isCon)
{
	char name[128];
	int i, p;
	int length;
	int match;
	int spos;
	int count = 0;
	
	if(!con_nickcompletion.integer)
		return 0;

	// changed that to 1
	if(!line[0])// || !line[1]) // we want at least... 2 written characters
		return 0;
	
	for(i = 0; i < cl.maxclients; ++i)
	{
		/*p = Sbar_GetPlayer(i);
		if(p < 0)
		break;*/
		p = i;
		if(!cl.scores[p].name[0])
			continue;

		SanitizeString(cl.scores[p].name, name);
		//Con_Printf("Sanitized: %s^7 -> %s", cl.scores[p].name, name);
		
		if(!name[0])
			continue;
		
		length = strlen(name);
		match = -1;
		spos = pos - 1; // no need for a minimum of characters :)
		
		while(spos >= 0 && (spos - pos) < length) // search-string-length < name length
		{
			if(spos > 0 && line[spos-1] != ' ' && line[spos-1] != ';' && line[spos-1] != '\"' && line[spos-1] != '\'')
			{
				if(!(isCon && line[spos-1] == ']' && spos == 1) && // console start
				   !(spos > 1 && line[spos-1] >= '0' && line[spos-1] <= '9' && line[spos-2] == STRING_COLOR_TAG)) // color start
				{
					--spos;
					continue;
				}
			}
			if(isCon && spos == 0)
				break;
			if(Nicks_strncasecmp(line+spos, name, pos-spos) == 0)
				match = spos;
			--spos;
		}
		if(match < 0)
			continue;
		//Con_Printf("Possible match: %s|%s\n", cl.scores[p].name, name);
		strlcpy(Nicks_list[count], cl.scores[p].name, sizeof(Nicks_list[count]));

		// the sanitized list
		strlcpy(Nicks_sanlist[count], name, sizeof(Nicks_sanlist[count]));
		if(!count)
		{
			Nicks_matchpos = match;
		}
		
		Nicks_offset[count] = s - (&line[match]);
		//Con_Printf("offset for %s: %i\n", name, Nicks_offset[count]);

		++count;
	}
	return count;
}

void Cmd_CompleteNicksPrint(int count)
{
	int i;
	for(i = 0; i < count; ++i)
		Con_Printf("%s\n", Nicks_list[i]);
}

void Nicks_CutMatchesNormal(int count)
{
	// cut match 0 down to the longest possible completion
	int i;
	unsigned int c, l;
	c = strlen(Nicks_sanlist[0]) - 1;
	for(i = 1; i < count; ++i)
	{
		l = strlen(Nicks_sanlist[i]) - 1;
		if(l < c)
			c = l;
		
		for(l = 0; l <= c; ++l)
			if(tolower(Nicks_sanlist[0][l]) != tolower(Nicks_sanlist[i][l]))
			{
				c = l-1;
				break;
			}
	}
	Nicks_sanlist[0][c+1] = 0;
	//Con_Printf("List0: %s\n", Nicks_sanlist[0]);
}

unsigned int Nicks_strcleanlen(const char *s)
{
	unsigned int l = 0;
	while(*s)
	{
		if( (*s >= 'a' && *s <= 'z') ||
		    (*s >= 'A' && *s <= 'Z') ||
		    (*s >= '0' && *s <= '9') ||
		    *s == ' ')
			++l;
		++s;
	}
	return l;
}

void Nicks_CutMatchesAlphaNumeric(int count)
{
	// cut match 0 down to the longest possible completion
	int i;
	unsigned int c, l;
	char tempstr[sizeof(Nicks_sanlist[0])];
	char *a, *b;
	char space_char = (con_nickcompletion_flags.integer & NICKS_NO_SPACES) ? 'a' : ' '; // yes this is correct, we want NO spaces when no spaces
	
	c = strlen(Nicks_sanlist[0]);
	for(i = 0, l = 0; i < (int)c; ++i)
	{
		if( (Nicks_sanlist[0][i] >= 'a' && Nicks_sanlist[0][i] <= 'z') ||
		    (Nicks_sanlist[0][i] >= 'A' && Nicks_sanlist[0][i] <= 'Z') ||
		    (Nicks_sanlist[0][i] >= '0' && Nicks_sanlist[0][i] <= '9') || Nicks_sanlist[0][i] == space_char) // this is what's COPIED
		{
			tempstr[l++] = Nicks_sanlist[0][i];
		}
	}
	tempstr[l] = 0;
	
	for(i = 1; i < count; ++i)
	{
		a = tempstr;
		b = Nicks_sanlist[i];
		while(1)
		{
			if(!*a)
				break;
			if(!*b)
			{
				*a = 0;
				break;
			}
			if(tolower(*a) == tolower(*b))
			{
				++a;
				++b;
				continue;
			}
			if( (*b >= 'a' && *b <= 'z') || (*b >= 'A' && *b <= 'Z') || (*b >= '0' && *b <= '9') || *b == space_char)
			{
				// b is alnum, so cut
				*a = 0;
				break;
			}
			++b;
		}
	}
	// Just so you know, if cutmatchesnormal doesn't kill the first entry, then even the non-alnums fit
	Nicks_CutMatchesNormal(count);
	//if(!Nicks_sanlist[0][0])
	if(Nicks_strcleanlen(Nicks_sanlist[0]) < strlen(tempstr))
	{
		// if the clean sanitized one is longer than the current one, use it, it has crap chars which definitely are in there
		strlcpy(Nicks_sanlist[0], tempstr, sizeof(tempstr));
	}
}

void Nicks_CutMatchesNoSpaces(int count)
{
	// cut match 0 down to the longest possible completion
	int i;
	unsigned int c, l;
	char tempstr[sizeof(Nicks_sanlist[0])];
	char *a, *b;
	
	c = strlen(Nicks_sanlist[0]);
	for(i = 0, l = 0; i < (int)c; ++i)
	{
		if(Nicks_sanlist[0][i] != ' ') // here it's what's NOT copied
		{
			tempstr[l++] = Nicks_sanlist[0][i];
		}
	}
	tempstr[l] = 0;
	
	for(i = 1; i < count; ++i)
	{
		a = tempstr;
		b = Nicks_sanlist[i];
		while(1)
		{
			if(!*a)
				break;
			if(!*b)
			{
				*a = 0;
				break;
			}
			if(tolower(*a) == tolower(*b))
			{
				++a;
				++b;
				continue;
			}
			if(*b != ' ')
			{
				*a = 0;
				break;
			}
			++b;
		}
	}
	// Just so you know, if cutmatchesnormal doesn't kill the first entry, then even the non-alnums fit
	Nicks_CutMatchesNormal(count);
	//if(!Nicks_sanlist[0][0])
	//Con_Printf("TS: %s\n", tempstr);
	if(Nicks_strcleanlen(Nicks_sanlist[0]) < strlen(tempstr))
	{
		// if the clean sanitized one is longer than the current one, use it, it has crap chars which definitely are in there
		strlcpy(Nicks_sanlist[0], tempstr, sizeof(tempstr));
	}
}

void Nicks_CutMatches(int count)
{
	if(con_nickcompletion_flags.integer & NICKS_ALPHANUMERICS_ONLY)
		Nicks_CutMatchesAlphaNumeric(count);
	else if(con_nickcompletion_flags.integer & NICKS_NO_SPACES)
		Nicks_CutMatchesNoSpaces(count);
	else
		Nicks_CutMatchesNormal(count);
}

const char **Nicks_CompleteBuildList(int count)
{
	const char **buf;
	int bpos = 0;
	// the list is freed by Con_CompleteCommandLine, so create a char**
	buf = (const char **)Mem_Alloc(tempmempool, count * sizeof(const char *) + sizeof (const char *));

	for(; bpos < count; ++bpos)
		buf[bpos] = Nicks_sanlist[bpos] + Nicks_offset[bpos];

	Nicks_CutMatches(count);
	
	buf[bpos] = NULL;
	return buf;
}

int Nicks_AddLastColor(char *buffer, int pos)
{
	qboolean quote_added = false;
	int match;
	char color = '7';
	
	if(con_nickcompletion_flags.integer & NICKS_ADD_QUOTE && buffer[Nicks_matchpos-1] == '\"')
	{
		// we'll have to add a quote :)
		buffer[pos++] = '\"';
		quote_added = true;
	}
	
	if((!quote_added && con_nickcompletion_flags.integer & NICKS_ADD_COLOR) || con_nickcompletion_flags.integer & NICKS_FORCE_COLOR)
	{
		// add color when no quote was added, or when flags &4?
		// find last color
		for(match = Nicks_matchpos-1; match >= 0; --match)
		{
			if(buffer[match] == STRING_COLOR_TAG && buffer[match+1] >= '0' && buffer[match+1] <= '9')
			{
				color = buffer[match+1];
				break;
			}
		}
		if(!quote_added && buffer[pos-2] == STRING_COLOR_TAG && buffer[pos-1] >= '0' && buffer[pos-1] <= '9') // when thes use &4
			pos -= 2;
		buffer[pos++] = STRING_COLOR_TAG;
		buffer[pos++] = color;
	}
	return pos;
}

int Nicks_CompleteChatLine(char *buffer, size_t size, unsigned int pos)
{
	int n;
	/*if(!con_nickcompletion.integer)
	  return; is tested in Nicks_CompletionCountPossible */
	n = Nicks_CompleteCountPossible(buffer, pos, &buffer[pos], false);
	if(n == 1)
	{
		size_t len;
		char *msg;
		
		msg = Nicks_list[0];
		len = min(size - Nicks_matchpos - 3, strlen(msg));
		memcpy(&buffer[Nicks_matchpos], msg, len);
		if( len < (size - 4) ) // space for color and space and \0
			len = Nicks_AddLastColor(buffer, Nicks_matchpos+len);
		buffer[len++] = ' ';
		buffer[len] = 0;
		return len;
	} else if(n > 1)
	{
		int len;
		char *msg;
		Con_Printf("\n%i possible nick%s\n", n, (n > 1) ? "s: " : ":");
		Cmd_CompleteNicksPrint(n);

		Nicks_CutMatches(n);

		msg = Nicks_sanlist[0];
		len = min(size - Nicks_matchpos, strlen(msg));
		memcpy(&buffer[Nicks_matchpos], msg, len);
		buffer[Nicks_matchpos + len] = 0;
		//pos += len;
		return Nicks_matchpos + len;
	}
	return pos;
}


/*
	Con_CompleteCommandLine

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha
	Enhanced to tab-complete map names by [515]

*/
void Con_CompleteCommandLine (void)
{
	const char *cmd = "";
	char *s;
	const char **list[4] = {0, 0, 0, 0};
	char s2[512];
	int c, v, a, i, cmd_len, pos, k;
	int n; // nicks --blub
	
	//find what we want to complete
	pos = key_linepos;
	while(--pos)
	{
		k = key_lines[edit_line][pos];
		if(k == '\"' || k == ';' || k == ' ' || k == '\'')
			break;
	}
	pos++;

	s = key_lines[edit_line] + pos;
	strlcpy(s2, key_lines[edit_line] + key_linepos, sizeof(s2));	//save chars after cursor
	key_lines[edit_line][key_linepos] = 0;					//hide them

	//maps search
	for(k=pos-1;k>2;k--)
		if(key_lines[edit_line][k] != ' ')
		{
			if(key_lines[edit_line][k] == '\"' || key_lines[edit_line][k] == ';' || key_lines[edit_line][k] == '\'')
				break;
			if	((pos+k > 2 && !strncmp(key_lines[edit_line]+k-2, "map", 3))
				|| (pos+k > 10 && !strncmp(key_lines[edit_line]+k-10, "changelevel", 11)))
			{
				char t[MAX_QPATH];
				if (GetMapList(s, t, sizeof(t)))
				{
					// first move the cursor
					key_linepos += (int)strlen(t) - (int)strlen(s);

					// and now do the actual work
					*s = 0;
					strlcat(key_lines[edit_line], t, MAX_INPUTLINE);
					strlcat(key_lines[edit_line], s2, MAX_INPUTLINE); //add back chars after cursor

					// and fix the cursor
					if(key_linepos > (int) strlen(key_lines[edit_line]))
						key_linepos = (int) strlen(key_lines[edit_line]);
				}
				return;
			}
		}

	// Count number of possible matches and print them
	c = Cmd_CompleteCountPossible(s);
	if (c)
	{
		Con_Printf("\n%i possible command%s\n", c, (c > 1) ? "s: " : ":");
		Cmd_CompleteCommandPrint(s);
	}
	v = Cvar_CompleteCountPossible(s);
	if (v)
	{
		Con_Printf("\n%i possible variable%s\n", v, (v > 1) ? "s: " : ":");
		Cvar_CompleteCvarPrint(s);
	}
	a = Cmd_CompleteAliasCountPossible(s);
	if (a)
	{
		Con_Printf("\n%i possible aliases%s\n", a, (a > 1) ? "s: " : ":");
		Cmd_CompleteAliasPrint(s);
	}
	n = Nicks_CompleteCountPossible(key_lines[edit_line], key_linepos, s, true);
	if (n)
	{
		Con_Printf("\n%i possible nick%s\n", n, (n > 1) ? "s: " : ":");
		Cmd_CompleteNicksPrint(n);
	}

	if (!(c + v + a + n))	// No possible matches
	{		
		if(s2[0])
			strlcpy(&key_lines[edit_line][key_linepos], s2, sizeof(key_lines[edit_line]) - key_linepos);
		return;
	}

	if (c)
		cmd = *(list[0] = Cmd_CompleteBuildList(s));
	if (v)
		cmd = *(list[1] = Cvar_CompleteBuildList(s));
	if (a)
		cmd = *(list[2] = Cmd_CompleteAliasBuildList(s));
	if (n)
		cmd = *(list[3] = Nicks_CompleteBuildList(n));
	
	for (cmd_len = (int)strlen(s);;cmd_len++)
	{
		const char **l;
		for (i = 0; i < 3; i++)
			if (list[i])
				for (l = list[i];*l;l++)
					if ((*l)[cmd_len] != cmd[cmd_len])
						goto done;
		// all possible matches share this character, so we continue...
		if (!cmd[cmd_len])
		{
			// if all matches ended at the same position, stop
			// (this means there is only one match)
			break;
		}
	}
done:

	// prevent a buffer overrun by limiting cmd_len according to remaining space
	cmd_len = min(cmd_len, (int)sizeof(key_lines[edit_line]) - 1 - pos);
	if (cmd)
	{
		key_linepos = pos;
		memcpy(&key_lines[edit_line][key_linepos], cmd, cmd_len);
		key_linepos += cmd_len;
		// if there is only one match, add a space after it
		if (c + v + a + n == 1 && key_linepos < (int)sizeof(key_lines[edit_line]) - 1)
		{
			if(n)
			{ // was a nick, might have an offset, and needs colors ;) --blub
				key_linepos = pos - Nicks_offset[0];
				cmd_len = strlen(Nicks_list[0]);
				cmd_len = min(cmd_len, (int)sizeof(key_lines[edit_line]) - 3 - pos);

				memcpy(&key_lines[edit_line][key_linepos] , Nicks_list[0], cmd_len);
				key_linepos += cmd_len;
				if(key_linepos < (int)(sizeof(key_lines[edit_line])-4)) // space for ^, X and space and \0
					key_linepos = Nicks_AddLastColor(key_lines[edit_line], key_linepos);
			}
			key_lines[edit_line][key_linepos++] = ' ';
		}
	}

	// use strlcat to avoid a buffer overrun
	key_lines[edit_line][key_linepos] = 0;
	strlcat(key_lines[edit_line], s2, sizeof(key_lines[edit_line]));

	// free the command, cvar, and alias lists
	for (i = 0; i < 4; i++)
		if (list[i])
			Mem_Free((void *)list[i]);
}

