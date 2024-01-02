#ifdef WIN32
#include <io.h> // Include this BEFORE darkplaces.h because it uses strncpy which trips DP_STATIC_ASSERT
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

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

// Sys_Error early in startup might screw with automated
// workflows or something if we show the dialog by default.
static qbool nocrashdialog = true;
void Sys_SDL_Dialog(const char *title, const char *string)
{
	if(!nocrashdialog)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, string, NULL);
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

void Sys_SDL_Init(void)
{
	// we don't know which systems we'll want to init, yet...
	if (SDL_Init(0) < 0)
		Sys_Error("SDL_Init failed: %s\n", SDL_GetError());

	// COMMANDLINEOPTION: sdl: -nocrashdialog disables "Engine Error" crash dialog boxes
	if(!Sys_CheckParm("-nocrashdialog"))
		nocrashdialog = false;
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
