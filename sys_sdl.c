#ifdef WIN32
#include <io.h> // Include this BEFORE darkplaces.h because it uses strncpy which trips DP_STATIC_ASSERT
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <signal.h>

/*
 * Include this BEFORE darkplaces.h because it breaks wrapping
 * _Static_assert. Cloudwalk has no idea how or why so don't ask.
 */
#include <SDL.h>

#include "darkplaces.h"

#ifdef WIN32
#ifdef _MSC_VER
#pragma comment(lib, "sdl2.lib")
#pragma comment(lib, "sdl2main.lib")
#endif
#endif

sys_t sys;

// =======================================================================
// General routines
// =======================================================================

void Sys_Shutdown (void)
{
#ifdef __ANDROID__
	Sys_AllowProfiling(false);
#endif
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif
	fflush(stdout);
	SDL_Quit();
}

static qbool nocrashdialog;
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
	
	if(!nocrashdialog)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Error", string, NULL);

	//Host_Shutdown ();
	exit (1);
}

void Sys_Print(const char *text)
{
#ifdef __ANDROID__
	if (developer.integer > 0)
	{
		__android_log_write(ANDROID_LOG_DEBUG, sys.argv[0], text);
	}
#else
	if(sys.outfd < 0)
		return;
#ifndef WIN32
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	// this is because both go to /dev/tty by default!
	{
		int origflags = fcntl (sys.outfd, F_GETFL, 0);
		fcntl (sys.outfd, F_SETFL, origflags & ~O_NONBLOCK);
#endif
#ifdef WIN32
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
	}
#endif
	//fprintf(stdout, "%s", text);
#endif
}

char *Sys_ConsoleInput(void)
{
	static char text[MAX_INPUTLINE];
	int len = 0;
#ifdef WIN32
	int c;

	// read a line out
	while (_kbhit ())
	{
		c = _getch ();
		_putch (c);
		if (c == '\r')
		{
			text[len] = 0;
			_putch ('\n');
			len = 0;
			return text;
		}
		if (c == 8)
		{
			if (len)
			{
				_putch (' ');
				_putch (c);
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
	return NULL;
}

char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	cliptext = SDL_GetClipboardText();
	if (cliptext != NULL) {
		size_t allocsize;
		allocsize = min(MAX_INPUTLINE, strlen(cliptext) + 1);
		data = (char *)Z_Malloc (allocsize);
		strlcpy (data, cliptext, allocsize);
		SDL_free(cliptext);
	}

	return data;
}

int main (int argc, char *argv[])
{
	signal(SIGFPE, SIG_IGN);

#ifdef __ANDROID__
	Sys_AllowProfiling(true);
#endif

	sys.selffd = -1;
	sys.argc = argc;
	sys.argv = (const char **)argv;

	// Sys_Error this early in startup might screw with automated
	// workflows or something if we show the dialog by default.
	nocrashdialog = true;

	Sys_ProvideSelfFD();

	// COMMANDLINEOPTION: -nocrashdialog disables "Engine Error" crash dialog boxes
	if(!Sys_CheckParm("-nocrashdialog"))
		nocrashdialog = false;
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

	// we don't know which systems we'll want to init, yet...
	SDL_Init(0);

	// used by everything
	Memory_Init();

	Host_Main();

	Sys_Quit(0);
	
	return 0;
}

qbool sys_supportsdlgetticks = true;
unsigned int Sys_SDL_GetTicks (void)
{
	return SDL_GetTicks();
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	SDL_Delay(milliseconds);
}
