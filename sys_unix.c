
#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#include <io.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#endif

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

void Sys_SDL_Dialog(const char *title, const char *string)
{
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

char *Sys_GetClipboardData (void)
{
	return NULL;
}

void Sys_SDL_Init(void)
{
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
