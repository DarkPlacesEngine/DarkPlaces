
#include "quakedef.h"
# include <time.h>
#ifndef WIN32
# include <unistd.h>
# include <fcntl.h>
# include <dlfcn.h>
#endif

extern cvar_t	timestamps;
extern cvar_t	timeformat;

static int sys_nostdout = false;

/* The translation table between the graphical font and plain ASCII  --KB */
static char qfont_table[256] = {
	'\0', '#',  '#',  '#',  '#',  '.',  '#',  '#',
	'#',  9,    10,   '#',  ' ',  13,   '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<',

	'<',  '=',  '>',  '#',  '#',  '.',  '#',  '#',
	'#',  '#',  ' ',  '#',  ' ',  '>',  '.',  '.',
	'[',  ']',  '0',  '1',  '2',  '3',  '4',  '5',
	'6',  '7',  '8',  '9',  '.',  '<',  '=',  '>',
	' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
	'(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
	'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
	'8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
	'@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '[',  '\\', ']',  '^',  '_',
	'`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '{',  '|',  '}',  '~',  '<'
};

static char sys_timestring[128];
char *Sys_TimeString(const char *timeformat)
{
	time_t mytime = time(NULL);
	strftime(sys_timestring, sizeof(sys_timestring), timeformat, localtime(&mytime));
	return sys_timestring;
}


#define MAXPRINTMSG 16384

void Sys_Print(const char *msg)
{
	unsigned char *p;
	// String we print
	char final[MAXPRINTMSG];

	if (sys_nostdout)
		return;

	if (timestamps.integer)
		snprintf(final, sizeof(final), "%s%s", Sys_TimeString(timeformat.string), msg);
	else
		strlcpy (final, msg, sizeof (final));

	// LordHavoc: make sure the string is terminated
	final[MAXPRINTMSG-1] = 0;
	for (p = (unsigned char *) final;*p; p++)
		*p = qfont_table[*p];
	Sys_PrintToTerminal(final);
}

void Sys_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];	// String we started with

	va_start(argptr,fmt);
	vsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Sys_Print(msg);
}

void Sys_Quit (void)
{
	Host_Shutdown();
	exit(0);
}

char engineversion[128];

void Sys_Shared_EarlyInit(void)
{
	const char* os;

	Memory_Init ();

	COM_InitArgv();
	COM_InitGameType();

#if defined(__linux__)
	os = "Linux";
#elif defined(WIN32)
	os = "Windows";
#elif defined(__NetBSD__)
	os = "NetBSD";
#elif defined(__OpenBSD__)
	os = "OpenBSD";
#else
	os = "Unknown";
#endif
	snprintf (engineversion, sizeof (engineversion), "%s %s %s", gamename, os, buildstring);

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

qboolean Sys_LoadLibrary (const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts)
{
	const dllfunction_t *func;
	dllhandle_t dllhandle;

	if (handle == NULL)
		return false;

	// Initializations
	for (func = fcts; func && func->name != NULL; func++)
		*func->funcvariable = NULL;

	// Load the DLL
#ifdef WIN32
	dllhandle = LoadLibrary (dllname);
#else
	dllhandle = dlopen (dllname, RTLD_LAZY);
#endif
	if (! dllhandle)
	{
		Con_Printf ("Can't load \"%s\".\n", dllname);
		return false;
	}

	// Get the function adresses
	for (func = fcts; func && func->name != NULL; func++)
		if (!(*func->funcvariable = (void *) Sys_GetProcAddress (dllhandle, func->name)))
		{
			Con_Printf ("Missing function \"%s\" - broken library!\n", func->name);
			Sys_UnloadLibrary (&dllhandle);
			return false;
		}

	*handle = dllhandle;
	Con_DPrintf("\"%s\" loaded.\n", dllname);
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

