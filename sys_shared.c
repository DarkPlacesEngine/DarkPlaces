
#include "quakedef.h"
#include <time.h>
#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <errno.h>

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

#ifdef WIN32
extern HANDLE hinput, houtput;
#endif

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
#ifdef WIN32
	DWORD		dummy;
#endif

	va_start (argptr, fmt);
#ifdef HAVE_VSNPRINTF
	vsnprintf (start, sizeof(start), fmt, argptr);
#else
	vsprintf (start, fmt, argptr);
#endif
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
#ifdef WIN32
	if (cls.state == ca_dedicated)
		WriteFile(houtput, final, strlen (final), &dummy, NULL);
#else
	printf("%s", final);
#endif
}

// LordHavoc: 256 pak files (was 10)
#define MAX_HANDLES 256
QFile *sys_handles[MAX_HANDLES];

int findhandle (void)
{
	int i;

	for (i = 1;i < MAX_HANDLES;i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
Sys_FileLength
================
*/
int Sys_FileLength (QFile *f)
{
	int pos, end;

	pos = Qtell (f);
	Qseek (f, 0, SEEK_END);
	end = Qtell (f);
	Qseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (const char *path, int *handle)
{
	QFile *f;
	int i, retval;

	i = findhandle ();

	f = Qopen(path, "rbz");

	if (!f)
	{
		*handle = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*handle = i;
		retval = Sys_FileLength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (const char *path)
{
	QFile	*f;
	int		i;

	i = findhandle ();

	f = Qopen(path, "wb");
	if (!f)
	{
		Con_Printf("Sys_FileOpenWrite: Error opening %s: %s", path, strerror(errno));
		return 0;
	}
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose (int handle)
{
	Qclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	Qseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return Qread (sys_handles[handle], dest, count);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return Qwrite (sys_handles[handle], data, count);
}

int Sys_FileTime (const char *path)
{
#if WIN32
	QFile *f;

	f = Qopen(path, "rb");
	if (f)
	{
		Qclose(f);
		return 1;
	}

	return -1;
#else
	struct stat buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
#endif
}

void Sys_mkdir (const char *path)
{
#if WIN32
	_mkdir (path);
#else
	mkdir (path, 0777);
#endif
}

char engineversion[128];

void Sys_Shared_EarlyInit(void)
{
	Memory_Init ();

#if defined(__linux__)
	sprintf (engineversion, "%s Linux %s", gamename, buildstring);
#elif defined(WIN32)
	sprintf (engineversion, "%s Windows %s", gamename, buildstring);
#else
	sprintf (engineversion, "%s Unknown %s", gamename, buildstring);
#endif

	if (COM_CheckParm("-nostdout"))
		sys_nostdout = 1;
	else
		printf("%s\n", engineversion);
}

void Sys_Shared_LateInit(void)
{
}

