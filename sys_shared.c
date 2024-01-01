#ifdef WIN32
# ifndef DONT_USE_SETDLLDIRECTORY
#  define _WIN32_WINNT 0x0502
# endif
#endif

#define SUPPORTDLL

#ifdef WIN32
# include <windows.h>
# include <mmsystem.h> // timeGetTime
# include <time.h> // localtime
# include <conio.h> // _kbhit, _getch, _putch
# include <io.h> // write; Include this BEFORE darkplaces.h because it uses strncpy which trips DP_STATIC_ASSERT
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif
#else
# ifdef __FreeBSD__
#  include <sys/sysctl.h>
# endif
# ifdef __ANDROID__
#  include <android/log.h>
# endif
# include <unistd.h>
# include <fcntl.h>
# include <sys/time.h>
# include <time.h>
# ifdef SUPPORTDLL
#  include <dlfcn.h>
# endif
#endif

#include <signal.h>

#include "quakedef.h"
#include "taskqueue.h"
#include "thread.h"
#include "libcurl.h"

sys_t sys;

static char sys_timestring[128];
char *Sys_TimeString(const char *timeformat)
{
	time_t mytime = time(NULL);
#if _MSC_VER >= 1400
	struct tm mytm;
	localtime_s(&mytm, &mytime);
	strftime(sys_timestring, sizeof(sys_timestring), timeformat, &mytm);
#else
	strftime(sys_timestring, sizeof(sys_timestring), timeformat, localtime(&mytime));
#endif
	return sys_timestring;
}


void Sys_Quit (int returnvalue)
{
	// Unlock mutexes because the quit command may jump directly here, causing a deadlock
	if ((cmd_local)->cbuf->lock)
		Cbuf_Unlock((cmd_local)->cbuf);
	SV_UnlockThreadMutex();
	TaskQueue_Frame(true);

	if (Sys_CheckParm("-profilegameonly"))
		Sys_AllowProfiling(false);
	host.state = host_shutdown;
	Host_Shutdown();

#ifdef __ANDROID__
	Sys_AllowProfiling(false);
#endif
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif
	fflush(stdout);

	exit(returnvalue);
}

#ifdef __cplusplus
extern "C"
#endif
void Sys_AllowProfiling(qbool enable)
{
#ifdef __ANDROID__
#ifdef USE_PROFILER
	extern void monstartup(const char *libname);
	extern void moncleanup(void);
	if (enable)
		monstartup("libmain.so");
	else
		moncleanup();
#endif
#elif (defined(__linux__) && (defined(__GLIBC__) || defined(__GNU_LIBRARY__))) || defined(__FreeBSD__)
	extern int moncontrol(int);
	moncontrol(enable);
#endif
}


/*
===============================================================================

DLL MANAGEMENT

===============================================================================
*/

static qbool Sys_LoadDependencyFunctions(dllhandle_t dllhandle, const dllfunction_t *fcts, qbool complain, qbool has_next)
{
	const dllfunction_t *func;
	if(dllhandle)
	{
		for (func = fcts; func && func->name != NULL; func++)
			if (!(*func->funcvariable = (void *) Sys_GetProcAddress (dllhandle, func->name)))
			{
				if(complain)
				{
					Con_DPrintf (" - missing function \"%s\" - broken library!", func->name);
					if(has_next)
						Con_DPrintf("\nContinuing with");
				}
				goto notfound;
			}
		return true;

	notfound:
		for (func = fcts; func && func->name != NULL; func++)
			*func->funcvariable = NULL;
	}
	return false;
}

qbool Sys_LoadSelf(dllhandle_t *handle)
{
	dllhandle_t dllhandle = 0;

	if (handle == NULL)
		return false;
#ifdef WIN32
	dllhandle = LoadLibrary (NULL);
#else
	dllhandle = dlopen (NULL, RTLD_NOW | RTLD_GLOBAL);
#endif
	*handle = dllhandle;
	return true;
}

qbool Sys_LoadDependency (const char** dllnames, dllhandle_t* handle, const dllfunction_t *fcts)
{
#ifdef SUPPORTDLL
	const dllfunction_t *func;
	dllhandle_t dllhandle = 0;
	unsigned int i;

	if (handle == NULL)
		return false;

#ifndef WIN32
#ifdef PREFER_PRELOAD
	dllhandle = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);
	if(Sys_LoadDependencyFunctions(dllhandle, fcts, false, false))
	{
		Con_DPrintf ("All of %s's functions were already linked in! Not loading dynamically...\n", dllnames[0]);
		*handle = dllhandle;
		return true;
	}
	else
		Sys_FreeLibrary(&dllhandle);
notfound:
#endif
#endif

	// Initializations
	for (func = fcts; func && func->name != NULL; func++)
		*func->funcvariable = NULL;

	// Try every possible name
	Con_DPrintf ("Trying to load library...");
	for (i = 0; dllnames[i] != NULL; i++)
	{
		Con_DPrintf (" \"%s\"", dllnames[i]);
#ifdef WIN32
# ifndef DONT_USE_SETDLLDIRECTORY
#  ifdef _WIN64
		SetDllDirectory("bin64");
#  else
		SetDllDirectory("bin32");
#  endif
# endif
#endif
		if(Sys_LoadLibrary(dllnames[i], &dllhandle))
		{
			if (Sys_LoadDependencyFunctions(dllhandle, fcts, true, (dllnames[i+1] != NULL) || (strrchr(sys.argv[0], '/'))))
				break;
			else
				Sys_FreeLibrary (&dllhandle);
		}
	}

	// see if the names can be loaded relative to the executable path
	// (this is for Mac OSX which does not check next to the executable)
	if (!dllhandle && strrchr(sys.argv[0], '/'))
	{
		char path[MAX_OSPATH];
		strlcpy(path, sys.argv[0], sizeof(path));
		strrchr(path, '/')[1] = 0;
		for (i = 0; dllnames[i] != NULL; i++)
		{
			char temp[MAX_OSPATH];
			strlcpy(temp, path, sizeof(temp));
			strlcat(temp, dllnames[i], sizeof(temp));
			Con_DPrintf (" \"%s\"", temp);

			if(Sys_LoadLibrary(temp, &dllhandle))
			{
				if (Sys_LoadDependencyFunctions(dllhandle, fcts, true, (dllnames[i+1] != NULL) || (strrchr(sys.argv[0], '/'))))
					break;
				else
					Sys_FreeLibrary (&dllhandle);
			}
		}
	}

	// No DLL found
	if (! dllhandle)
	{
		Con_DPrintf(" - failed.\n");
		return false;
	}

	Con_DPrintf(" - loaded.\n");
	Con_Printf("Loaded library \"%s\"\n", dllnames[i]);

	*handle = dllhandle;
	return true;
#else
	return false;
#endif
}

qbool Sys_LoadLibrary(const char *name, dllhandle_t *handle)
{
	dllhandle_t dllhandle = 0;

	if(handle == NULL)
		return false;

#ifdef SUPPORTDLL
# ifdef WIN32
	dllhandle = LoadLibrary (name);
# else
	dllhandle = dlopen (name, RTLD_LAZY | RTLD_GLOBAL);
# endif
#endif
	if(!dllhandle)
		return false;

	*handle = dllhandle;
	return true;
}

void Sys_FreeLibrary (dllhandle_t* handle)
{
#ifdef SUPPORTDLL
	if (handle == NULL || *handle == NULL)
		return;

#ifdef WIN32
	FreeLibrary (*handle);
#else
	dlclose (*handle);
#endif

	*handle = NULL;
#endif
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
#ifdef SUPPORTDLL
#ifdef WIN32
	return (void *)GetProcAddress (handle, name);
#else
	return (void *)dlsym (handle, name);
#endif
#else
	return NULL;
#endif
}

#ifdef WIN32
# define HAVE_TIMEGETTIME 1
# define HAVE_QUERYPERFORMANCECOUNTER 1
# define HAVE_Sleep 1
#endif

#ifndef WIN32
#if defined(CLOCK_MONOTONIC) || defined(CLOCK_HIRES)
# define HAVE_CLOCKGETTIME 1
#endif
// FIXME improve this check, manpage hints to DST_NONE
# define HAVE_GETTIMEOFDAY 1
#endif

#ifdef FD_SET
# define HAVE_SELECT 1
#endif

#ifndef WIN32
// FIXME improve this check
# define HAVE_USLEEP 1
#endif

// these are referenced elsewhere
cvar_t sys_usenoclockbutbenchmark = {CF_SHARED, "sys_usenoclockbutbenchmark", "0", "don't use ANY real timing, and simulate a clock (for benchmarking); the game then runs as fast as possible. Run a QC mod with bots that does some stuff, then does a quit at the end, to benchmark a server. NEVER do this on a public server."};
cvar_t sys_libdir = {CF_READONLY | CF_SHARED, "sys_libdir", "", "Default engine library directory"};

// these are not
static cvar_t sys_debugsleep = {CF_SHARED, "sys_debugsleep", "0", "write requested and attained sleep times to standard output, to be used with gnuplot"};
static cvar_t sys_usesdlgetticks = {CF_SHARED, "sys_usesdlgetticks", "0", "use SDL_GetTicks() timer (less accurate, for debugging)"};
static cvar_t sys_usesdldelay = {CF_SHARED, "sys_usesdldelay", "0", "use SDL_Delay() (less accurate, for debugging)"};
#if HAVE_QUERYPERFORMANCECOUNTER
static cvar_t sys_usequeryperformancecounter = {CF_SHARED | CF_ARCHIVE, "sys_usequeryperformancecounter", "0", "use windows QueryPerformanceCounter timer (which has issues on multicore/multiprocessor machines and processors which are designed to conserve power) for timing rather than timeGetTime function (which has issues on some motherboards)"};
#endif
#if HAVE_CLOCKGETTIME
static cvar_t sys_useclockgettime = {CF_SHARED | CF_ARCHIVE, "sys_useclockgettime", "1", "use POSIX clock_gettime function (not adjusted by NTP on some older Linux kernels) for timing rather than gettimeofday (which has issues if the system time is stepped by ntpdate, or apparently on some Xen installations)"};
#endif

static double benchmark_time; // actually always contains an integer amount of milliseconds, will eventually "overflow"

/*
================
Sys_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int Sys_CheckParm (const char *parm)
{
	int i;

	for (i=1 ; i<sys.argc ; i++)
	{
		if (!sys.argv[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,sys.argv[i]))
			return i;
	}

	return 0;
}

void Sys_Init_Commands (void)
{
	Cvar_RegisterVariable(&sys_debugsleep);
	Cvar_RegisterVariable(&sys_usenoclockbutbenchmark);
	Cvar_RegisterVariable(&sys_libdir);
#if HAVE_TIMEGETTIME || HAVE_QUERYPERFORMANCECOUNTER || HAVE_CLOCKGETTIME || HAVE_GETTIMEOFDAY
	if(sys_supportsdlgetticks)
	{
		Cvar_RegisterVariable(&sys_usesdlgetticks);
		Cvar_RegisterVariable(&sys_usesdldelay);
	}
#endif
#if HAVE_QUERYPERFORMANCECOUNTER
	Cvar_RegisterVariable(&sys_usequeryperformancecounter);
#endif
#if HAVE_CLOCKGETTIME
	Cvar_RegisterVariable(&sys_useclockgettime);
#endif
}

double Sys_DirtyTime(void)
{
	// first all the OPTIONAL timers

	// benchmark timer (fake clock)
	if(sys_usenoclockbutbenchmark.integer)
	{
		double old_benchmark_time = benchmark_time;
		benchmark_time += 1;
		if(benchmark_time == old_benchmark_time)
			Sys_Error("sys_usenoclockbutbenchmark cannot run any longer, sorry");
		return benchmark_time * 0.000001;
	}
#if HAVE_QUERYPERFORMANCECOUNTER
	if (sys_usequeryperformancecounter.integer)
	{
		// LadyHavoc: note to people modifying this code, DWORD is specifically defined as an unsigned 32bit number, therefore the 65536.0 * 65536.0 is fine.
		// QueryPerformanceCounter
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// very accurate (CPU cycles)
		// known issues:
		// does not necessarily match realtime too well (tends to get faster and faster in win98)
		// wraps around occasionally on some platforms (depends on CPU speed and probably other unknown factors)
		double timescale;
		LARGE_INTEGER PerformanceFreq;
		LARGE_INTEGER PerformanceCount;

		if (QueryPerformanceFrequency (&PerformanceFreq))
		{
			QueryPerformanceCounter (&PerformanceCount);
	
			timescale = 1.0 / ((double) PerformanceFreq.LowPart + (double) PerformanceFreq.HighPart * 65536.0 * 65536.0);
			return ((double) PerformanceCount.LowPart + (double) PerformanceCount.HighPart * 65536.0 * 65536.0) * timescale;
		}
		else
		{
			Con_Printf("No hardware timer available\n");
			// fall back to other clock sources
			Cvar_SetValueQuick(&sys_usequeryperformancecounter, false);
		}
	}
#endif

#if HAVE_CLOCKGETTIME
	if (sys_useclockgettime.integer)
	{
		struct timespec ts;
#  ifdef CLOCK_MONOTONIC
		// linux
		clock_gettime(CLOCK_MONOTONIC, &ts);
#  else
		// sunos
		clock_gettime(CLOCK_HIGHRES, &ts);
#  endif
		return (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
	}
#endif

	// now all the FALLBACK timers
	if(sys_supportsdlgetticks && sys_usesdlgetticks.integer)
		return (double) Sys_SDL_GetTicks() / 1000.0;
#if HAVE_GETTIMEOFDAY
	{
		struct timeval tp;
		gettimeofday(&tp, NULL);
		return (double) tp.tv_sec + tp.tv_usec / 1000000.0;
	}
#elif HAVE_TIMEGETTIME
	{
		static int firsttimegettime = true;
		// timeGetTime
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// reasonable accuracy (millisecond)
		// issues:
		// wraps around every 47 days or so (but this is non-fatal to us, odd times are rejected, only causes a one frame stutter)

		// make sure the timer is high precision, otherwise different versions of windows have varying accuracy
		if (firsttimegettime)
		{
			timeBeginPeriod(1);
			firsttimegettime = false;
		}

		return (double) timeGetTime() / 1000.0;
	}
#else
	// fallback for using the SDL timer if no other timer is available
	// this calls Sys_Error() if not linking against SDL
	return (double) Sys_SDL_GetTicks() / 1000.0;
#endif
}

extern cvar_t host_maxwait;
double Sys_Sleep(double time)
{
	double dt;
	uint32_t microseconds;

	// convert to microseconds
	time *= 1000000.0;

	if(host_maxwait.value <= 0)
		time = min(time, 1000000.0);
	else
		time = min(time, host_maxwait.value * 1000.0);

	if (time < 1 || host.restless)
		return 0; // not sleeping this frame

	microseconds = time; // post-validation to prevent overflow

	if(sys_usenoclockbutbenchmark.integer)
	{
		double old_benchmark_time = benchmark_time;
		benchmark_time += microseconds;
		if(benchmark_time == old_benchmark_time)
			Sys_Error("sys_usenoclockbutbenchmark cannot run any longer, sorry");
		return 0;
	}

	if(sys_debugsleep.integer)
		Con_Printf("sys_debugsleep: requesting %u ", microseconds);
	dt = Sys_DirtyTime();

	// less important on newer libcurl so no need to disturb dedicated servers
	if (cls.state != ca_dedicated && Curl_Select(microseconds))
	{
		// a transfer is ready or we finished sleeping
	}
	else if(sys_supportsdlgetticks && sys_usesdldelay.integer)
		Sys_SDL_Delay(microseconds / 1000);
#if HAVE_SELECT
	else
	{
		struct timeval tv;
		lhnetsocket_t *s;
		fd_set fdreadset;
		int lastfd = -1;

		FD_ZERO(&fdreadset);
		if (cls.state == ca_dedicated && sv_checkforpacketsduringsleep.integer)
		{
			List_For_Each_Entry(s, &lhnet_socketlist.list, lhnetsocket_t, list)
			{
				if (s->address.addresstype == LHNETADDRESSTYPE_INET4 || s->address.addresstype == LHNETADDRESSTYPE_INET6)
				{
					if (lastfd < s->inetsocket)
						lastfd = s->inetsocket;
	#if defined(WIN32) && !defined(_MSC_VER)
					FD_SET((int)s->inetsocket, &fdreadset);
	#else
					FD_SET((unsigned int)s->inetsocket, &fdreadset);
	#endif
				}
			}
		}
		tv.tv_sec = microseconds / 1000000;
		tv.tv_usec = microseconds % 1000000;
		// on Win32, select() cannot be used with all three FD list args being NULL according to MSDN
		// (so much for POSIX...)
		// bones_was_here: but a zeroed fd_set seems to be tolerated (tested on Win 7)
		select(lastfd + 1, &fdreadset, NULL, NULL, &tv);
	}
#elif HAVE_USLEEP
	else
		usleep(microseconds);
#elif HAVE_Sleep
	else
		Sleep(microseconds / 1000);
#else
	else
		Sys_SDL_Delay(microseconds / 1000);
#endif

	dt = Sys_DirtyTime() - dt;
	if(sys_debugsleep.integer)
		Con_Printf(" got %u oversleep %d\n", (unsigned int)(dt * 1000000), (unsigned int)(dt * 1000000) - microseconds);
	return (dt < 0 || dt >= 1800) ? 0 : dt;
}


/*
===============================================================================

STDIO

===============================================================================
*/

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
	}
  #endif
	//fprintf(stdout, "%s", text);
#endif
}

/// for the console to report failures inside Con_Printf()
void Sys_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Sys_Print(msg);
}

/// Reads a line from POSIX stdin or the Windows console
char *Sys_ConsoleInput(void)
{
	static char text[MAX_INPUTLINE];
#ifdef WIN32
	static unsigned int len = 0;
	int c;

	// read a line out
	while (_kbhit ())
	{
		c = _getch ();
		if (c == '\r')
		{
			text[len] = '\0';
			_putch ('\n');
			len = 0;
			return text;
		}
		if (c == '\b')
		{
			if (len)
			{
				_putch (c);
				_putch (' ');
				_putch (c);
				len--;
			}
			continue;
		}
		if (len < sizeof (text) - 1)
		{
			_putch (c);
			text[len] = c;
			len++;
		}
	}
#else
	fd_set fdset;
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

	FD_ZERO(&fdset);
	FD_SET(fileno(stdin), &fdset);
	if (select(1, &fdset, NULL, NULL, &timeout) != -1 && FD_ISSET(fileno(stdin), &fdset))
		return fgets(text, sizeof(text), stdin);
#endif
	return NULL;
}


/*
===============================================================================

Startup and Shutdown

===============================================================================
*/

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[MAX_INPUTLINE];

// change stdin to non blocking
#ifndef WIN32
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~O_NONBLOCK);
#endif

	va_start (argptr,error);
	dpvsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Con_Printf(CON_ERROR "Engine Error: %s\n", string);

	// don't want a dead window left blocking the OS UI or the crash dialog
	Host_Shutdown();

	Sys_SDL_Dialog("Engine Error", string);

	exit (1);
}

#ifndef WIN32
static const char *Sys_FindInPATH(const char *name, char namesep, const char *PATH, char pathsep, char *buf, size_t bufsize)
{
	const char *p = PATH;
	const char *q;
	if(p && name)
	{
		while((q = strchr(p, ':')))
		{
			dpsnprintf(buf, bufsize, "%.*s%c%s", (int)(q-p), p, namesep, name);
			if(FS_SysFileExists(buf))
				return buf;
			p = q + 1;
		}
		if(!q) // none found - try the last item
		{
			dpsnprintf(buf, bufsize, "%s%c%s", p, namesep, name);
			if(FS_SysFileExists(buf))
				return buf;
		}
	}
	return name;
}
#endif

static const char *Sys_FindExecutableName(void)
{
#if defined(WIN32)
	return sys.argv[0];
#else
	static char exenamebuf[MAX_OSPATH+1];
	ssize_t n = -1;
#if defined(__FreeBSD__)
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t exenamebuflen = sizeof(exenamebuf)-1;
	if (sysctl(mib, 4, exenamebuf, &exenamebuflen, NULL, 0) == 0)
	{
		n = exenamebuflen;
	}
#elif defined(__linux__)
	n = readlink("/proc/self/exe", exenamebuf, sizeof(exenamebuf)-1);
#endif
	if(n > 0 && (size_t)(n) < sizeof(exenamebuf))
	{
		exenamebuf[n] = 0;
		return exenamebuf;
	}
	if(strchr(sys.argv[0], '/'))
		return sys.argv[0]; // possibly a relative path
	else
		return Sys_FindInPATH(sys.argv[0], '/', getenv("PATH"), ':', exenamebuf, sizeof(exenamebuf));
#endif
}

void Sys_ProvideSelfFD(void)
{
	if(sys.selffd != -1)
		return;
	sys.selffd = FS_SysOpenFD(Sys_FindExecutableName(), "rb", false);
}

// for x86 cpus only...  (x64 has SSE2_PRESENT)
#if defined(SSE_POSSIBLE) && !defined(SSE2_PRESENT)
// code from SDL, shortened as we can expect CPUID to work
static int CPUID_Features(void)
{
	int features = 0;
# if (defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)) && defined(__i386__)
        __asm__ (
"        movl    %%ebx,%%edi\n"
"        xorl    %%eax,%%eax                                           \n"
"        incl    %%eax                                                 \n"
"        cpuid                       # Get family/model/stepping/features\n"
"        movl    %%edx,%0                                              \n"
"        movl    %%edi,%%ebx\n"
        : "=m" (features)
        :
        : "%eax", "%ecx", "%edx", "%edi"
        );
# elif (defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)
        __asm {
        xor     eax, eax
        inc     eax
        cpuid                       ; Get family/model/stepping/features
        mov     features, edx
        }
# else
#  error SSE_POSSIBLE set but no CPUID implementation
# endif
	return features;
}
#endif

#ifdef SSE_POSSIBLE
qbool Sys_HaveSSE(void)
{
	// COMMANDLINEOPTION: SSE: -nosse disables SSE support and detection
	if(Sys_CheckParm("-nosse"))
		return false;
#ifdef SSE_PRESENT
	return true;
#else
	// COMMANDLINEOPTION: SSE: -forcesse enables SSE support and disables detection
	if(Sys_CheckParm("-forcesse") || Sys_CheckParm("-forcesse2"))
		return true;
	if(CPUID_Features() & (1 << 25))
		return true;
	return false;
#endif
}

qbool Sys_HaveSSE2(void)
{
	// COMMANDLINEOPTION: SSE2: -nosse2 disables SSE2 support and detection
	if(Sys_CheckParm("-nosse") || Sys_CheckParm("-nosse2"))
		return false;
#ifdef SSE2_PRESENT
	return true;
#else
	// COMMANDLINEOPTION: SSE2: -forcesse2 enables SSE2 support and disables detection
	if(Sys_CheckParm("-forcesse2"))
		return true;
	if((CPUID_Features() & (3 << 25)) == (3 << 25)) // SSE is 1<<25, SSE2 is 1<<26
		return true;
	return false;
#endif
}
#endif

/// called to set process priority for dedicated servers
#if defined(__linux__)
#include <sys/resource.h>
#include <errno.h>

void Sys_InitProcessNice (void)
{
	struct rlimit lim;
	sys.nicepossible = false;
	if(Sys_CheckParm("-nonice"))
		return;
	errno = 0;
	sys.nicelevel = getpriority(PRIO_PROCESS, 0);
	if(errno)
	{
		Con_Printf("Kernel does not support reading process priority - cannot use niceness\n");
		return;
	}
	if(getrlimit(RLIMIT_NICE, &lim))
	{
		Con_Printf("Kernel does not support lowering nice level again - cannot use niceness\n");
		return;
	}
	if(lim.rlim_cur != RLIM_INFINITY && sys.nicelevel < (int) (20 - lim.rlim_cur))
	{
		Con_Printf("Current nice level is below the soft limit - cannot use niceness\n");
		return;
	}
	sys.nicepossible = true;
	sys.isnice = false;
}
void Sys_MakeProcessNice (void)
{
	if(!sys.nicepossible)
		return;
	if(sys.isnice)
		return;
	Con_DPrintf("Process is becoming 'nice'...\n");
	if(setpriority(PRIO_PROCESS, 0, 19))
		Con_Printf(CON_ERROR "Failed to raise nice level to %d\n", 19);
	sys.isnice = true;
}
void Sys_MakeProcessMean (void)
{
	if(!sys.nicepossible)
		return;
	if(!sys.isnice)
		return;
	Con_DPrintf("Process is becoming 'mean'...\n");
	if(setpriority(PRIO_PROCESS, 0, sys.nicelevel))
		Con_Printf(CON_ERROR "Failed to lower nice level to %d\n", sys.nicelevel);
	sys.isnice = false;
}
#else
void Sys_InitProcessNice (void)
{
}
void Sys_MakeProcessNice (void)
{
}
void Sys_MakeProcessMean (void)
{
}
#endif

/** Halt and try not to catch fire.
 * Writing to any file could corrupt it,
 * any uneccessary code could crash while we crash.
 * No malloc() (libgcc should be loaded already) or Con_Printf() allowed here.
 */
static void Sys_HandleCrash(int sig)
{
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
	// Before doing anything else grab the stack frame addresses
	#include <execinfo.h>
	void *stackframes[32];
	int framecount = backtrace(stackframes, 32);
#endif

	// Windows doesn't have strsignal()
	const char *sigdesc;
	switch (sig)
	{
#ifndef WIN32 // or SIGBUS
		case SIGBUS:  sigdesc = "Bus error"; break;
#endif
		case SIGILL:  sigdesc = "Illegal instruction"; break;
		case SIGABRT: sigdesc = "Aborted"; break;
		case SIGFPE:  sigdesc = "Floating point exception"; break;
		case SIGSEGV: sigdesc = "Segmentation fault"; break;
		default:      sigdesc = "Yo dawg, we hit a bug while hitting a bug";
	}

	fprintf(stderr, "\n\n\e[1;37;41m    Engine Crash: %s (%d)    \e[m\n", sigdesc, sig);
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
	// the first two addresses will be in this function and in signal() in libc
	backtrace_symbols_fd(stackframes + 2, framecount - 2, fileno(stderr));
#endif
	fprintf(stderr, "\e[1m%s\e[m\n", engineversion);

	// DP8 TODO: send a disconnect message indicating we crashed, see CL_DisconnectEx()

	// don't want a dead window left blocking the OS UI or the crash dialog
	VID_Shutdown();
	S_StopAllSounds();

	Sys_SDL_Dialog("Engine Crash", sigdesc);

	exit (sig);
}

static void Sys_HandleSignal(int sig)
{
#ifdef WIN32
	// Windows users will likely never see this so no point replicating strsignal()
	Con_Printf("\nReceived signal %d, exiting...\n", sig);
#else
	Con_Printf("\nReceived %s signal (%d), exiting...\n", strsignal(sig), sig);
#endif
	host.state = host_shutdown;
}

/// SDL2 only handles SIGINT and SIGTERM by default and doesn't log anything
static void Sys_InitSignals(void)
{
// Windows docs say its signal() only accepts these ones
	signal(SIGABRT, Sys_HandleCrash);
	signal(SIGFPE,  Sys_HandleCrash);
	signal(SIGILL,  Sys_HandleCrash);
	signal(SIGINT,  Sys_HandleSignal);
	signal(SIGSEGV, Sys_HandleCrash);
	signal(SIGTERM, Sys_HandleSignal);
#ifndef WIN32
	signal(SIGHUP,  Sys_HandleSignal);
	signal(SIGQUIT, Sys_HandleSignal);
	signal(SIGBUS,  Sys_HandleCrash);
	signal(SIGPIPE, Sys_HandleSignal);
#endif
}

int main (int argc, char **argv)
{
	sys.argc = argc;
	sys.argv = (const char **)argv;

	// COMMANDLINEOPTION: -noterminal disables console output on stdout
	if(Sys_CheckParm("-noterminal"))
		sys.outfd = -1;
	// COMMANDLINEOPTION: -stderr moves console output to stderr
	else if(Sys_CheckParm("-stderr"))
		sys.outfd = 2;
	else
		sys.outfd = 1;

	sys.selffd = -1;
	Sys_ProvideSelfFD(); // may call Con_Printf() so must be after sys.outfd is set

#ifndef WIN32
	fcntl(fileno(stdin), F_SETFL, fcntl (fileno(stdin), F_GETFL, 0) | O_NONBLOCK);
#endif

#ifdef __ANDROID__
	Sys_AllowProfiling(true);
#endif

	Sys_InitSignals();

	Host_Main();

	Sys_Quit(0);

	return 0;
}
