#include "quakedef.h"

#ifdef WIN32
#include <io.h>
#include "conio.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

#include <signal.h>

#include <SDL.h>

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

void Sys_PrintToTerminal(const char *text)
{
#ifndef WIN32
	// BUG: for some reason, NDELAY also affects stdout (1) when used on stdin (0).
	int origflags = fcntl (1, F_GETFL, 0);
	fcntl (1, F_SETFL, origflags & ~FNDELAY);
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
	newtime = (double) SDL_GetTicks() / 1000.0;


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
		static char text[MAX_INPUTLINE];
		int len = 0;
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

void Sys_Sleep(int microseconds)
{
	SDL_Delay(microseconds / 1000);
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
			if ((cliptext = GlobalLock (hClipboardData)) != 0)
			{
				size_t allocsize;
				allocsize = GlobalSize (hClipboardData) + 1;
				data = Z_Malloc (allocsize);
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

void Sys_Init_Commands (void)
{
}

int main (int argc, char *argv[])
{
	signal(SIGFPE, SIG_IGN);

	com_argc = argc;
	com_argv = (const char **)argv;

#ifndef WIN32
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
#endif

	// we don't know which systems we'll want to init, yet...
	SDL_Init(0);

	Host_Main();

	return 0;
}
