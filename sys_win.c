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

#define WIN32_USETIMEGETTIME 0

#include "quakedef.h"
#include "winquake.h"
#include "errno.h"
#include "resource.h"
#include "conproc.h"
#include "direct.h"

// # of seconds to wait on Sys_Error running dedicated before exiting
#define CONSOLE_ERROR_TIMEOUT	60.0
// sleep time on pause or minimization
#define PAUSE_SLEEP		50
// sleep time when not focus
#define NOT_FOCUS_SLEEP	20

int			starttime;
qboolean	ActiveApp, Minimized;

static qboolean		sc_return_on_enter = false;
HANDLE				hinput, houtput;

static HANDLE	tevent;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;

/*
===============================================================================

QFile IO

===============================================================================
*/

// LordHavoc: 256 pak files (was 10)
#define	MAX_HANDLES		256
QFile	*sys_handles[MAX_HANDLES];

int		findhandle (void)
{
	int		i;

	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
Sys_FileLength
================
*/
int Sys_FileLength (QFile *f)
{
	int		pos;
	int		end;

	pos = Qtell (f);
	Qseek (f, 0, SEEK_END);
	end = Qtell (f);
	Qseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	QFile	*f;
	int		i, retval;

	i = findhandle ();

	f = Qopen(path, "rbz");

	if (!f)
	{
		*hndl = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = Sys_FileLength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (char *path)
{
	QFile	*f;
	int		i;

	i = findhandle ();

	f = Qopen(path, "wb");
	if (!f)
	{
		Con_Printf("Sys_FileOpenWrite: Error opening %s: %s", path, strerror(errno));
		return 0;
	}
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose (int handle)
{
	Qclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	Qseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return Qread (sys_handles[handle], dest, count);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return Qwrite (sys_handles[handle], data, count);
}

int	Sys_FileTime (char *path)
{
	QFile	*f;

	f = Qopen(path, "rb");
	if (f)
	{
		Qclose(f);
		return 1;
	}

	return -1;
}

void Sys_mkdir (char *path)
{
	_mkdir (path);
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void SleepUntilInput (int time);

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024], text2[1024];
	char		*text3 = "Press Enter to exit\n";
	char		*text4 = "***********************************\n";
	char		*text5 = "\n";
	DWORD		dummy;
	double		starttime;
	static int	in_sys_error0 = 0;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;
	static int	in_sys_error3 = 0;

	if (!in_sys_error3)
		in_sys_error3 = 1;

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	if (cls.state == ca_dedicated)
	{
		va_start (argptr, error);
		vsprintf (text, error, argptr);
		va_end (argptr);

		sprintf (text2, "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen (text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen (text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen (text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);


		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here

		while (!Sys_ConsoleInput () && ((Sys_DoubleTime () - starttime) < CONSOLE_ERROR_TIMEOUT))
			SleepUntilInput(1);
	}
	else
	{
	// switch to windowed so the message box is visible, unless we already
	// tried that and failed
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			VID_SetDefaultMode ();
			MessageBox(NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
		else
			MessageBox(NULL, text, "Double Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
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
		DeinitConProc ();
	}

	exit (1);
}

void Sys_Quit (void)
{
	Host_Shutdown();

	if (tevent)
		CloseHandle (tevent);

	if (cls.state == ca_dedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
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
#if WIN32_USETIMEGETTIME
	// timeGetTime
	// platform:
	// Windows 95/98/ME/NT/2000
	// features:
	// reasonable accuracy (millisecond)
	// issues:
	// wraps around every 47 days or so (but this is non-fatal to us, odd times are rejected, only causes a one frame stutter)

	// make sure the timer is high precision, otherwise different versions of windows have varying accuracy
	if (first)
		timeBeginPeriod (1);

	newtime = (double) timeGetTime () / 1000.0;
#else
	// QueryPerformanceCounter
	// platform:
	// Windows 95/98/ME/NT/2000
	// features:
	// very accurate (CPU cycles)
	// known issues:
	// does not necessarily match realtime too well (tends to get faster and faster in win98)
	// wraps around occasionally on some platforms (depends on CPU speed and probably other unknown factors)
	static double timescale = 0.0;
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
#endif

	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
		Con_Printf("Sys_DoubleTime: time running backwards??\n");
	else
	{
		curtime += newtime - oldtime;
		oldtime = newtime;
	}

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

void Sys_Sleep (void)
{
	Sleep (1);
}


void Sys_SendKeyEvents (void)
{
	MSG msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();

		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
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
int			global_nCmdShow;
char		*argv[MAX_NUM_ARGVS];
static char	*empty_string = "";

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	double			oldtime, newtime;
	MEMORYSTATUS	lpBuffer;
	static	char	cwd[1024];
	int				t;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	if (!GetCurrentDirectory (sizeof(cwd), cwd))
		Sys_Error ("Couldn't determine current directory");

	if (cwd[strlen(cwd)-1] == '/')
		cwd[strlen(cwd)-1] = 0;

	memset(&host_parms, 0, sizeof(host_parms));

	host_parms.basedir = cwd;

	host_parms.argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (host_parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[host_parms.argc] = lpCmdLine;
			host_parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}

		}
	}

	host_parms.argv = argv;

	COM_InitArgv (host_parms.argc, host_parms.argv);

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	Sys_Shared_EarlyInit();

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

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1)
	{
		if (cls.state != ca_dedicated)
		{
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl.paused && !ActiveApp) || Minimized)
			{
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			}
			else if (!ActiveApp)
				SleepUntilInput (NOT_FOCUS_SLEEP);
		}

		newtime = Sys_DoubleTime ();
		Host_Frame (newtime - oldtime);
		oldtime = newtime;
	}

	/* return success of application */
	return true;
}

