
#ifdef WIN32
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <dlfcn.h>
#endif

#include <signal.h>

#include "quakedef.h"


#ifdef WIN32
cvar_t sys_usetimegettime = {CVAR_SAVE, "sys_usetimegettime", "1"};
#endif

/*
===============================================================================

DLL MANAGEMENT

===============================================================================
*/

dllhandle_t Sys_LoadLibrary (const char* name)
{
#ifdef WIN32
	return LoadLibrary (name);
#else
	return dlopen (name, RTLD_LAZY);
#endif
}

void Sys_UnloadLibrary (dllhandle_t handle)
{
#ifdef WIN32
	FreeLibrary (handle);
#else
	dlclose (handle);
#endif
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
#ifdef WIN32
	return (void *)GetProcAddress (handle, name);
#else
	return (void *)dlsym (handle, name);
#endif
}


// =======================================================================
// General routines
// =======================================================================

void Sys_Quit (void)
{
	Host_Shutdown();
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
	fflush(stdout);
	exit(0);
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

// change stdin to non blocking
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

	va_start (argptr,error);
	vsprintf (string,error,argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
}

void Sys_Print(const char *text)
{
	printf("%s", text);
}

double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
#ifdef WIN32
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
#else
	struct timeval tp;
	gettimeofday(&tp, NULL);
	newtime = (double) tp.tv_sec + tp.tv_usec / 1000000.0;
#endif

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

char *Sys_ConsoleInput(void)
{
	if (cls.state == ca_dedicated)
	{
		static char text[256];
		int len;
#ifdef WIN32
		int c;

		// read a line out
		while (_kbhit ())
		{
			c = _getch ();
			putch (c);
			if (c == '\r')
			{
				text[len] = 0;
				putch ('\n');
				len = 0;
				return text;
			}
			if (c == 8)
			{
				if (len)
				{
					putch (' ');
					putch (c);
					len--;
					text[len] = 0;
				}
				continue;
			}
			text[len] = c;
			len++;
			text[len] = 0;
			if (len == sizeof (text))
				len = 0;
		}
#else
		fd_set fdset;
		struct timeval timeout;
		FD_ZERO(&fdset);
		FD_SET(0, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select (1, &fdset, NULL, NULL, &timeout) != -1 && FD_ISSET(0, &fdset))
		{
			len = read (0, text, sizeof(text));
			if (len >= 1)
			{
				// rip off the \n and terminate
				text[len-1] = 0;
				return text;
			}
		}
#endif
	}
	return NULL;
}

void Sys_Sleep(void)
{
#ifdef WIN32
	Sleep (1);
#else
	usleep(1);
#endif
}

int main (int argc, const char **argv)
{
	double frameoldtime, framenewtime;

	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = argv;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	Sys_Shared_EarlyInit();

	Host_Init();

	Sys_Shared_LateInit();

	frameoldtime = Sys_DoubleTime () - 0.1;
	while (1)
	{
		// find time spent rendering last frame
		framenewtime = Sys_DoubleTime ();

		Host_Frame (framenewtime - frameoldtime);

		frameoldtime = framenewtime;
	}
	return 0;
}
