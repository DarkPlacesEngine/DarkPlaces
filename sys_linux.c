#include "quakedef.h"

#ifdef WIN32
#include <io.h>
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#endif

#include <signal.h>


cvar_t sys_usenoclockbutbenchmark = {CVAR_SAVE, "sys_usenoclockbutbenchmark", "0", "don't use ANY real timing, and simulate a clock (for benchmarking); the game then runs as fast as possible. Run a QC mod with bots that does some stuff, then does a quit at the end, to benchmark a server. NEVER do this on a public server."};
static unsigned long benchmark_time;

#ifdef WIN32
cvar_t sys_usetimegettime = {CVAR_SAVE, "sys_usetimegettime", "1", "use windows timeGetTime function (which has issues on some motherboards) for timing rather than QueryPerformanceCounter timer (which has issues on multicore/multiprocessor machines and processors which are designed to conserve power)"};
#else
# ifndef MACOSX
cvar_t sys_useclockgettime = {CVAR_SAVE, "sys_useclockgettime", "0", "use POSIX clock_gettime function (which has issues if the system clock speed is far off, as it can't get fixed by NTP) for timing rather than gettimeofday (which has issues if the system time is stepped by ntpdate, or apparently on some Xen installations)"};
# endif
#endif



// =======================================================================
// General routines
// =======================================================================
void Sys_Shutdown (void)
{
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
	fflush(stdout);
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[MAX_INPUTLINE];

// change stdin to non blocking
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif

	va_start (argptr,error);
	dpvsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Con_Printf ("Quake Error: %s\n", string);

	Host_Shutdown ();
	exit (1);
}

void Sys_PrintToTerminal(const char *text)
{
#ifndef WIN32
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	int origflags = fcntl (1, F_GETFL, 0);
	fcntl (1, F_SETFL, origflags & ~FNDELAY);
#else
#define write _write
#endif
	while(*text)
	{
		int written = (int)write(1, text, (int)strlen(text));
		if(written <= 0)
			break; // sorry, I cannot do anything about this error - without an output
		text += written;
	}
#ifndef WIN32
	fcntl (1, F_SETFL, origflags);
#endif
	//fprintf(stdout, "%s", text);
}

double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
	if(sys_usenoclockbutbenchmark.integer)
	{
		benchmark_time += 1;
		return benchmark_time / 1e6;
	}
#ifdef WIN32
#include <mmsystem.h>
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
#else
# ifndef MACOSX
	if (sys_useclockgettime.integer)
	{
		struct timespec ts;
#  ifdef SUNOS
		clock_gettime(CLOCK_HIGHRES, &ts);
#  else
		clock_gettime(CLOCK_MONOTONIC, &ts);
#  endif
		newtime = (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
	}
	else
# endif
	{
		struct timeval tp;
		gettimeofday(&tp, NULL);
		newtime = (double) tp.tv_sec + tp.tv_usec / 1000000.0;
	}
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
	else if (newtime > oldtime + 1800)
	{
		Con_Printf("Sys_DoubleTime: time stepped forward (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else
		curtime += newtime - oldtime;
	oldtime = newtime;

	return curtime;
}

char *Sys_ConsoleInput(void)
{
	//if (cls.state == ca_dedicated)
	{
		static char text[MAX_INPUTLINE];
		static unsigned int len = 0;
#ifdef WIN32
		int c;

		// read a line out
		while (_kbhit ())
		{
			c = _getch ();
			if (c == '\r')
			{
				text[len] = '\0';
				_putch ('\n');
				len = 0;
				return text;
			}
			if (c == '\b')
			{
				if (len)
				{
					_putch (c);
					_putch (' ');
					_putch (c);
					len--;
				}
				continue;
			}
			if (len < sizeof (text) - 1)
			{
				_putch (c);
				text[len] = c;
				len++;
			}
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

void Sys_Sleep(int microseconds)
{
	if(sys_usenoclockbutbenchmark.integer)
	{
		benchmark_time += microseconds;
		return;
	}
#ifdef WIN32
	Sleep(microseconds / 1000);
#else
	usleep(microseconds);
#endif
}

char *Sys_GetClipboardData (void)
{
	return NULL;
}

void Sys_InitConsole (void)
{
}

void Sys_Init_Commands (void)
{
	Cvar_RegisterVariable(&sys_usenoclockbutbenchmark);
#ifdef WIN32
	Cvar_RegisterVariable(&sys_usetimegettime);
#else
# ifndef MACOSX
	Cvar_RegisterVariable(&sys_useclockgettime);
# endif
#endif
}

int main (int argc, char **argv)
{
	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = (const char **)argv;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	Host_Main();

	return 0;
}
