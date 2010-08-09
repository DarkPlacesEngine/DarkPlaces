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
// sys_win.c -- Win32 system interface code

#include <windows.h>
#include <mmsystem.h>
#include <direct.h>
#ifdef SUPPORTDIRECTX
#include <dsound.h>
#endif

#include "qtypes.h"

#include "quakedef.h"
#include "errno.h"
#include "resource.h"
#include "conproc.h"

HANDLE				hinput, houtput;

#ifdef QHOST
static HANDLE	tevent;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;
#endif


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_INPUTLINE];
	static int	in_sys_error0 = 0;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;
	static int	in_sys_error3 = 0;

	va_start (argptr, error);
	dpvsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	Con_Printf ("Quake Error: %s\n", text);

	// close video so the message box is visible, unless we already tried that
	if (!in_sys_error0 && cls.state != ca_dedicated)
	{
		in_sys_error0 = 1;
		VID_Shutdown();
	}

	if (!in_sys_error3 && cls.state != ca_dedicated)
	{
		in_sys_error3 = true;
		MessageBox(NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
	}

	if (!in_sys_error1)
	{
		in_sys_error1 = 1;
		Host_Shutdown ();
	}

// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		Sys_Shutdown ();
	}

	exit (1);
}

void Sys_Shutdown (void)
{
#ifdef QHOST
	if (tevent)
		CloseHandle (tevent);
#endif

	if (cls.state == ca_dedicated)
		FreeConsole ();

#ifdef QHOST
// shut down QHOST hooks if necessary
	DeinitConProc ();
#endif
}

void Sys_PrintToTerminal(const char *text)
{
	DWORD dummy;
	extern HANDLE houtput;

	if ((houtput != 0) && (houtput != INVALID_HANDLE_VALUE))
		WriteFile(houtput, text, (DWORD) strlen(text), &dummy, NULL);
}

char *Sys_ConsoleInput (void)
{
	static char text[MAX_INPUTLINE];
	static int len;
	INPUT_RECORD recs[1024];
	int ch;
	DWORD numread, numevents, dummy;

	if (cls.state != ca_dedicated)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
		{
			cls.state = ca_disconnected;
			Sys_Error ("Error getting # of console events (error code %x)", (unsigned int)GetLastError());
		}

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
		{
			cls.state = ca_disconnected;
			Sys_Error ("Error reading console input (error code %x)", (unsigned int)GetLastError());
		}

		if (numread != 1)
		{
			cls.state = ca_disconnected;
			Sys_Error ("Couldn't read console input (error code %x)", (unsigned int)GetLastError());
		}

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}

						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						if (len)
						{
							len--;
						}
						break;

					default:
						if (ch >= (int) (unsigned char) ' ')
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}

	return NULL;
}

char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard (NULL) != 0)
	{
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData (CF_TEXT)) != 0)
		{
			if ((cliptext = (char *)GlobalLock (hClipboardData)) != 0)
			{
				size_t allocsize;
				allocsize = GlobalSize (hClipboardData) + 1;
				data = (char *)Z_Malloc (allocsize);
				strlcpy (data, cliptext, allocsize);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	return data;
}

void Sys_InitConsole (void)
{
#ifdef QHOST
	int t;

	// initialize the windows dedicated server console if needed
	tevent = CreateEvent(NULL, false, false, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");
#endif

	houtput = GetStdHandle (STD_OUTPUT_HANDLE);
	hinput = GetStdHandle (STD_INPUT_HANDLE);

	// LordHavoc: can't check cls.state because it hasn't been initialized yet
	// if (cls.state == ca_dedicated)
	if (COM_CheckParm("-dedicated"))
	{
		//if ((houtput == 0) || (houtput == INVALID_HANDLE_VALUE)) // LordHavoc: on Windows XP this is never 0 or invalid, but hinput is invalid
		{
			if (!AllocConsole ())
				Sys_Error ("Couldn't create dedicated server console (error code %x)", (unsigned int)GetLastError());
			houtput = GetStdHandle (STD_OUTPUT_HANDLE);
			hinput = GetStdHandle (STD_INPUT_HANDLE);
		}
		if ((houtput == 0) || (houtput == INVALID_HANDLE_VALUE))
			Sys_Error ("Couldn't create dedicated server console");


#ifdef QHOST
#ifdef _WIN64
#define atoi _atoi64
#endif
	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)atoi (com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
#endif
	}

// because sound is off until we become active
	S_BlockSound ();
}

/*
==============================================================================

WINDOWS CRAP

==============================================================================
*/


/*
==================
WinMain
==================
*/
HINSTANCE	global_hInstance;
const char	*argv[MAX_NUM_ARGVS];
char		program_name[MAX_OSPATH];

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MEMORYSTATUS lpBuffer;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	program_name[sizeof(program_name)-1] = 0;
	GetModuleFileNameA(NULL, program_name, sizeof(program_name) - 1);

	com_argc = 1;
	com_argv = argv;
	argv[0] = program_name;

	// FIXME: this tokenizer is rather redundent, call a more general one
	while (*lpCmdLine && (com_argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ISWHITESPACE(*lpCmdLine))
			lpCmdLine++;

		if (!*lpCmdLine)
			break;

		if (*lpCmdLine == '\"')
		{
			// quoted string
			lpCmdLine++;
			argv[com_argc] = lpCmdLine;
			com_argc++;
			while (*lpCmdLine && (*lpCmdLine != '\"'))
				lpCmdLine++;
		}
		else
		{
			// unquoted word
			argv[com_argc] = lpCmdLine;
			com_argc++;
			while (*lpCmdLine && !ISWHITESPACE(*lpCmdLine))
				lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

	Sys_ProvideSelfFD();

	Host_Main();

	/* return success of application */
	return true;
}

#if 0
// unused, this file is only used when building windows client and vid_wgl provides WinMain() instead
int main (int argc, const char* argv[])
{
	MEMORYSTATUS lpBuffer;

	global_hInstance = GetModuleHandle (0);

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	program_name[sizeof(program_name)-1] = 0;
	GetModuleFileNameA(NULL, program_name, sizeof(program_name) - 1);

	com_argc = argc;
	com_argv = argv;

	Host_Main();

	return true;
}
#endif

qboolean sys_supportsdlgetticks = false;
unsigned int Sys_SDL_GetTicks (void)
{
	Sys_Error("Called Sys_SDL_GetTicks on non-SDL target");
	return 0;
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	Sys_Error("Called Sys_SDL_Delay on non-SDL target");
}
