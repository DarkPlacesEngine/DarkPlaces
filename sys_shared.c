
#include "quakedef.h"
#include <time.h>
#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>
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


#define MAX_PRINT_MSG	16384
void Sys_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		start[MAX_PRINT_MSG];	// String we started with
	char		stamp[MAX_PRINT_MSG];	// Time stamp
	char		final[MAX_PRINT_MSG];	// String we print

	time_t		mytime = 0;
	struct tm	*local = NULL;

	unsigned char		*p;

	va_start (argptr, fmt);
	vsnprintf (start, sizeof(start), fmt, argptr);
	va_end (argptr);

	if (sys_nostdout)
		return;

	if (timestamps.integer)
	{
		mytime = time (NULL);
		local = localtime (&mytime);
		strftime (stamp, sizeof (stamp), timeformat.string, local);

		snprintf (final, sizeof (final), "%s%s", stamp, start);
	}
	else
		snprintf (final, sizeof (final), "%s", start);

	// LordHavoc: make sure the string is terminated
	final[MAX_PRINT_MSG - 1] = 0;
	for (p = (unsigned char *) final;*p; p++)
		*p = qfont_table[*p];
	Sys_Print(final);
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

#ifndef WIN32
#include <dlfcn.h>
#endif

dllhandle_t Sys_LoadLibrary (const char* name)
{
#ifdef WIN32
	return LoadLibrary (name);
#else
	return dlopen (name, RTLD_LAZY);
#endif
}

void Sys_UnloadLibrary (dllhandle_t handle)
{
#ifdef WIN32
	FreeLibrary (handle);
#else
	dlclose (handle);
#endif
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
#ifdef WIN32
	return (void *)GetProcAddress (handle, name);
#else
	return (void *)dlsym (handle, name);
#endif
}

