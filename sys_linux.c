#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <signal.h>
#include <limits.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "quakedef.h"

// =======================================================================
// General routines
// =======================================================================

void Sys_Quit (void)
{
	Host_Shutdown();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	fflush(stdout);
	exit(0);
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

// change stdin to non blocking
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	va_start (argptr,error);
	vsprintf (string,error,argptr);
	va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown ();
	exit (1);

}

void Sys_Warn (const char *warning, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,warning);
	vsprintf (string,warning,argptr);
	va_end (argptr);
	fprintf(stderr, "Warning: %s", string);
}

double Sys_DoubleTime (void)
{
	static int first = true;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
	struct timeval tp;

	gettimeofday(&tp, NULL);

	newtime = (double) tp.tv_sec + tp.tv_usec / 1000000.0;

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

// =======================================================================
// Sleeps for microseconds
// =======================================================================

static volatile int oktogo;

void alarm_handler(int x)
{
	oktogo=1;
}

void floating_point_exception_handler(int whatever)
{
	signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (cls.state == ca_dedicated)
	{
		FD_ZERO(&fdset);
		FD_SET(0, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
			return NULL;

		len = read (0, text, sizeof(text));
		if (len < 1)
			return NULL;
		text[len-1] = 0;    // rip off the /n and terminate

		return text;
	}
	return NULL;
}

void Sys_Sleep(void)
{
	usleep(1);
}

int main (int argc, const char **argv)
{
	double frameoldtime, framenewtime;

	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = argv;

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

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
