
#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <sys/time.h>
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
