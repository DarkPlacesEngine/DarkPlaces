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

#include "quakedef.h"
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include "errno.h"
#include "resource.h"
#include "conproc.h"
#include "direct.h"

extern void S_BlockSound (void);

cvar_t sys_usetimegettime = {CVAR_SAVE, "sys_usetimegettime", "1", "use windows timeGetTime function (which has issues on some motherboards) for timing rather than QueryPerformanceCounter timer (which has issues on multicore/multiprocessor machines and processors which are designed to conserve power)"};

HANDLE				hinput, houtput;

static HANDLE	tevent;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;


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
	if (tevent)
		CloseHandle (tevent);

	if (cls.state == ca_dedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();
}

void Sys_PrintToTerminal(const char *text)
{
	DWORD dummy;
	extern HANDLE houtput;
	if (cls.state == ca_dedicated)
		WriteFile(houtput, text, (DWORD) strlen(text), &dummy, NULL);
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
	// LordHavoc: note to people modifying this code, DWORD is specifically defined as an unsigned 32bit number, therefore the 65536.0 * 65536.0 is fine.
	if (sys_usetimegettime.integer)
	{
		static int firsttimegettime = true;
		// timeGetTime
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// reasonable accuracy (millisecond)
		// issues:
		// wraps around every 47 days or so (but this is non-fatal to us, odd times are rejected, only causes a one frame stutter)

		// make sure the timer is high precision, otherwise different versions of windows have varying accuracy
		if (firsttimegettime)
		{
			timeBeginPeriod (1);
			firsttimegettime = false;
		}

		newtime = (double) timeGetTime () / 1000.0;
	}
	else
	{
		// QueryPerformanceCounter
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// very accurate (CPU cycles)
		// known issues:
		// does not necessarily match realtime too well (tends to get faster and faster in win98)
		// wraps around occasionally on some platforms (depends on CPU speed and probably other unknown factors)
		double timescale;
		LARGE_INTEGER PerformanceFreq;
		LARGE_INTEGER PerformanceCount;

		if (!QueryPerformanceFrequency (&PerformanceFreq))
		{
			Con_Printf ("No hardware timer available\n");
			// fall back to timeGetTime
			Cvar_SetValueQuick(&sys_usetimegettime, true);
			return Sys_DoubleTime();
		}
		QueryPerformanceCounter (&PerformanceCount);

		#ifdef __BORLANDC__
		timescale = 1.0 / ((double) PerformanceFreq.u.LowPart + (double) PerformanceFreq.u.HighPart * 65536.0 * 65536.0);
		newtime = ((double) PerformanceCount.u.LowPart + (double) PerformanceCount.u.HighPart * 65536.0 * 65536.0) * timescale;
		#else
		timescale = 1.0 / ((double) PerformanceFreq.LowPart + (double) PerformanceFreq.HighPart * 65536.0 * 65536.0);
		newtime = ((double) PerformanceCount.LowPart + (double) PerformanceCount.HighPart * 65536.0 * 65536.0) * timescale;
		#endif
	}

	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
	{
		// warn if it's significant
		if (newtime - oldtime < -0.01)
			Con_Printf("Sys_DoubleTime: time stepped backwards (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else
		curtime += newtime - oldtime;
	oldtime = newtime;

	return curtime;
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
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

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
						if (ch >= ' ')
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

void Sys_Sleep(int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	Sleep(milliseconds);
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
			if ((cliptext = GlobalLock (hClipboardData)) != 0)
			{
				data = malloc (GlobalSize(hClipboardData)+1);
				strcpy (data, cliptext);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	return data;
}

void Sys_InitConsole (void)
{
	int t;

	// initialize the windows dedicated server console if needed
	tevent = CreateEvent(NULL, false, false, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	// LordHavoc: can't check cls.state because it hasn't been initialized yet
	// if (cls.state == ca_dedicated)
	if (COM_CheckParm("-dedicated"))
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console");

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

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
	}

// because sound is off until we become active
	S_BlockSound ();
}

void Sys_Init_Commands (void)
{
	Cvar_RegisterVariable(&sys_usetimegettime);
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
	double frameoldtime, framenewtime;
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
		while (*lpCmdLine && *lpCmdLine <= ' ')
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
			while (*lpCmdLine && *lpCmdLine > ' ')
				lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

	Host_Init ();

	frameoldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1)
	{
		framenewtime = Sys_DoubleTime ();
		Host_Frame (framenewtime - frameoldtime);
		frameoldtime = framenewtime;
	}

	/* return success of application */
	return true;
}
