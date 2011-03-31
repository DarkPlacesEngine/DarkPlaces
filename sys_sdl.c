
#ifdef WIN32
#ifdef _MSC_VER
#pragma comment(lib, "sdl.lib")
#pragma comment(lib, "sdlmain.lib")
#endif
#include <io.h>
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

#include <signal.h>

#include <SDL.h>

#include "quakedef.h"

// =======================================================================
// General routines
// =======================================================================

void Sys_Shutdown (void)
{
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
#endif
	fflush(stdout);
	SDL_Quit();
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

static int outfd = 1;
void Sys_PrintToTerminal(const char *text)
{
	if(outfd < 0)
		return;
#ifdef FNDELAY
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	// this is because both go to /dev/tty by default!
	{
		int origflags = fcntl (outfd, F_GETFL, 0);
		fcntl (outfd, F_SETFL, origflags & ~FNDELAY);
#endif
#ifdef WIN32
#define write _write
#endif
		while(*text)
		{
			fs_offset_t written = (fs_offset_t)write(outfd, text, strlen(text));
			if(written <= 0)
				break; // sorry, I cannot do anything about this error - without an output
			text += written;
		}
#ifdef FNDELAY
		fcntl (outfd, F_SETFL, origflags);
	}
#endif
	//fprintf(stdout, "%s", text);
}

char *Sys_ConsoleInput(void)
{
	if (cls.state == ca_dedicated)
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
	}
	return NULL;
}

char *Sys_GetClipboardData (void)
{
#ifdef WIN32
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
#else
	return NULL;
#endif
}

void Sys_InitConsole (void)
{
}

int main (int argc, char *argv[])
{
	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = (const char **)argv;
	Sys_ProvideSelfFD();

	// COMMANDLINEOPTION: sdl: -noterminal disables console output on stdout
	if(COM_CheckParm("-noterminal"))
		outfd = -1;
	// COMMANDLINEOPTION: sdl: -stderr moves console output to stderr
	else if(COM_CheckParm("-stderr"))
		outfd = 2;
	else
		outfd = 1;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	// we don't know which systems we'll want to init, yet...
	SDL_Init(0);

	Host_Main();

	return 0;
}

qboolean sys_supportsdlgetticks = true;
unsigned int Sys_SDL_GetTicks (void)
{
	return SDL_GetTicks();
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	SDL_Delay(milliseconds);
}
