
#include "quakedef.h"
# include <time.h>
#ifndef WIN32
# include <unistd.h>
# include <fcntl.h>
# include <dlfcn.h>
#endif

qboolean sys_nostdout = false;

static char sys_timestring[128];
char *Sys_TimeString(const char *timeformat)
{
	time_t mytime = time(NULL);
	strftime(sys_timestring, sizeof(sys_timestring), timeformat, localtime(&mytime));
	return sys_timestring;
}


extern qboolean host_shuttingdown;
void Sys_Quit (void)
{
	host_shuttingdown = true;
	Host_Shutdown();
	exit(0);
}

char engineversion[128];

void Sys_Shared_EarlyInit(void)
{
	const char* os;

	Memory_Init ();
	Log_Init ();

	COM_InitArgv();
	COM_InitGameType();

#if defined(__linux__)
	os = "Linux";
#elif defined(WIN32)
	os = "Windows";
#elif defined(__FreeBSD__)
	os = "FreeBSD";
#elif defined(__NetBSD__)
	os = "NetBSD";
#elif defined(__OpenBSD__)
	os = "OpenBSD";
#elif defined(MACOSX)
	os = "Mac OS X";
#else
	os = "Unknown";
#endif
	dpsnprintf (engineversion, sizeof (engineversion), "%s %s %s", gamename, os, buildstring);

// COMMANDLINEOPTION: Console: -nostdout disables text output to the terminal the game was launched from
	if (COM_CheckParm("-nostdout"))
		sys_nostdout = 1;
	else
		Con_Printf("%s\n", engineversion);
}

void Sys_Shared_LateInit(void)
{
}

/*
===============================================================================

DLL MANAGEMENT

===============================================================================
*/

qboolean Sys_LoadLibrary (const char** dllnames, dllhandle_t* handle, const dllfunction_t *fcts)
{
	const dllfunction_t *func;
	dllhandle_t dllhandle = 0;
	unsigned int i;

	if (handle == NULL)
		return false;

	// Initializations
	for (func = fcts; func && func->name != NULL; func++)
		*func->funcvariable = NULL;

	// Try every possible name
	for (i = 0; dllnames[i] != NULL; i++)
	{
#ifdef WIN32
		dllhandle = LoadLibrary (dllnames[i]);
#else
		dllhandle = dlopen (dllnames[i], RTLD_LAZY);
#endif
		if (dllhandle)
			break;

		Con_Printf ("Can't load \"%s\".\n", dllnames[i]);
	}

	// No DLL found
	if (! dllhandle)
		return false;

	Con_Printf("\"%s\" loaded.\n", dllnames[i]);

	// Get the function adresses
	for (func = fcts; func && func->name != NULL; func++)
		if (!(*func->funcvariable = (void *) Sys_GetProcAddress (dllhandle, func->name)))
		{
			Con_Printf ("Missing function \"%s\" - broken library!\n", func->name);
			Sys_UnloadLibrary (&dllhandle);
			return false;
		}

	*handle = dllhandle;
	return true;
}

void Sys_UnloadLibrary (dllhandle_t* handle)
{
	if (handle == NULL || *handle == NULL)
		return;

#ifdef WIN32
	FreeLibrary (*handle);
#else
	dlclose (*handle);
#endif

	*handle = NULL;
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
#ifdef WIN32
	return (void *)GetProcAddress (handle, name);
#else
	return (void *)dlsym (handle, name);
#endif
}

