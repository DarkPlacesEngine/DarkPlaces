
#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <sys/time.h>
#endif

#include "darkplaces.h"

sys_t sys;

// =======================================================================
// General routines
// =======================================================================
void Sys_SDL_Shutdown(void)
{
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
