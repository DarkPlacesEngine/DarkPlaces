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
#include <dsound.h>
#include "errno.h"
#include "resource.h"
#include "conproc.h"
#include "direct.h"

extern void S_BlockSound (void);

cvar_t sys_usetimegettime = {CVAR_SAVE, "sys_usetimegettime", "1"};

// # of seconds to wait on Sys_Error running dedicated before exiting
#define CONSOLE_ERROR_TIMEOUT	60.0
// sleep time on pause or minimization
#define PAUSE_SLEEP		50
// sleep time when not focus
#define NOT_FOCUS_SLEEP	20

static qboolean		sc_return_on_enter = false;
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

void SleepUntilInput (int time);

void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	static int	in_sys_error0 = 0;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	// close video so the message box is visible, unless we already tried that
	if (!in_sys_error0 && cls.state != ca_dedicated)
	{
		in_sys_error0 = 1;
		VID_Shutdown();     
	}
	MessageBox(NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);

	Con_Print ("Quake Error: ");
	Con_Print (text);
	Con_Print ("\n");

	if (!in_sys_error1)
	{
		in_sys_error1 = 1;
		Host_Shutdown ();
	}

// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		DeinitConProc ();
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
		WriteFile(houtput, text, strlen (text), &dummy, NULL);
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
			Sys_Error ("No hardware timer available");
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
	static char text[256];
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
						else if (sc_return_on_enter)
						{
						// special case to allow exiting from the error handler on Enter
							text[0] = '\r';
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

/*
==============================================================================

WINDOWS CRAP

==============================================================================
*/


void SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects(1, &tevent, false, time, QS_ALLINPUT);
}


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
	int t;

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

		if (*lpCmdLine)
		{
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
	}

	Sys_Shared_EarlyInit();

	Cvar_RegisterVariable(&sys_usetimegettime);

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

	Host_Init ();

	Sys_Shared_LateInit();

	frameoldtime = Sys_DoubleTime ();
	
	/* main window message loop */
	while (1)
	{
		if (cls.state != ca_dedicated)
		{
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl.paused && !vid_activewindow) || vid_hidden)
			{
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			}
			else if (!vid_activewindow)
				SleepUntilInput (NOT_FOCUS_SLEEP);
		}

		framenewtime = Sys_DoubleTime ();
		Host_Frame (framenewtime - frameoldtime);
		frameoldtime = framenewtime;
	}

	/* return success of application */
	return true;
}
