
#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#include <io.h>
#include "conio.h"
#else
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <signal.h>

#include "darkplaces.h"

sys_t sys;

// =======================================================================
// General routines
// =======================================================================
void Sys_Shutdown (void)
{
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif
	fflush(stdout);
}

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[MAX_INPUTLINE];

// change stdin to non blocking
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif
	va_start (argptr,error);
	dpvsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Con_Printf(CON_ERROR "Engine Error: %s\n", string);

	//Host_Shutdown ();
	exit (1);
}

void Sys_Print(const char *text)
{
	if(sys.outfd < 0)
		return;
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	// this is because both go to /dev/tty by default!
	{
#ifndef WIN32
		int origflags = fcntl (sys.outfd, F_GETFL, 0);
		fcntl (sys.outfd, F_SETFL, origflags & ~O_NONBLOCK);
#else
#define write _write
#endif
		while(*text)
		{
			fs_offset_t written = (fs_offset_t)write(sys.outfd, text, (int)strlen(text));
			if(written <= 0)
				break; // sorry, I cannot do anything about this error - without an output
			text += written;
		}
#ifndef WIN32
		fcntl (sys.outfd, F_SETFL, origflags);
#endif
	}
	//fprintf(stdout, "%s", text);
}

char *Sys_ConsoleInput(void)
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
		len = read (0, text, sizeof(text) - 1);
		if (len >= 1)
		{
			// rip off the \n and terminate
			// div0: WHY? console code can deal with \n just fine
			// this caused problems with pasting stuff into a terminal window
			// so, not ripping off the \n, but STILL keeping a NUL terminator
			text[len] = 0;
			return text;
		}
	}
#endif
	return NULL;
}

char *Sys_GetClipboardData (void)
{
	return NULL;
}

int main (int argc, char **argv)
{
	signal(SIGFPE, SIG_IGN);
	sys.selffd = -1;
	sys.argc = argc;
	sys.argv = (const char **)argv;
	Sys_ProvideSelfFD();

	// COMMANDLINEOPTION: sdl: -noterminal disables console output on stdout
	if(Sys_CheckParm("-noterminal"))
		sys.outfd = -1;
	// COMMANDLINEOPTION: sdl: -stderr moves console output to stderr
	else if(Sys_CheckParm("-stderr"))
		sys.outfd = 2;
	else
		sys.outfd = 1;
#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | O_NONBLOCK);
#endif

	// used by everything
	Memory_Init();

	Host_Main();

	Sys_Quit(0);

	return 0;
}

qbool sys_supportsdlgetticks = false;
unsigned int Sys_SDL_GetTicks (void)
{
	Sys_Error("Called Sys_SDL_GetTicks on non-SDL target");
	return 0;
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	Sys_Error("Called Sys_SDL_Delay on non-SDL target");
}
