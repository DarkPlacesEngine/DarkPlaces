#include "quakedef.h"
#include "thread.h"
#include <SDL.h>
#include <SDL_thread.h>

int Thread_Init(void)
{
#ifdef THREADDISABLE
	Con_Printf("Threading disabled in this build\n");
#endif
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
#ifdef THREADDISABLE
	return false;
#else
	return true;
#endif
}

void *_Thread_CreateMutex(const char *filename, int fileline)
{
	void *mutex = SDL_CreateMutex();
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p mutex create %s:%i\n" , mutex, filename, fileline);
#endif
	return mutex;
}

void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p mutex destroy %s:%i\n", mutex, filename, fileline);
#endif
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

int _Thread_LockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p mutex lock %s:%i\n"   , mutex, filename, fileline);
#endif
	return SDL_LockMutex((SDL_mutex *)mutex);
}

int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p mutex unlock %s:%i\n" , mutex, filename, fileline);
#endif
	return SDL_UnlockMutex((SDL_mutex *)mutex);
}

void *_Thread_CreateCond(const char *filename, int fileline)
{
	void *cond = (void *)SDL_CreateCond();
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p cond create %s:%i\n"   , cond, filename, fileline);
#endif
	return cond;
}

void _Thread_DestroyCond(void *cond, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p cond destroy %s:%i\n"   , cond, filename, fileline);
#endif
	SDL_DestroyCond((SDL_cond *)cond);
}

int _Thread_CondSignal(void *cond, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p cond signal %s:%i\n"   , cond, filename, fileline);
#endif
	return SDL_CondSignal((SDL_cond *)cond);
}

int _Thread_CondBroadcast(void *cond, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p cond broadcast %s:%i\n"   , cond, filename, fileline);
#endif
	return SDL_CondBroadcast((SDL_cond *)cond);
}

int _Thread_CondWait(void *cond, void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p cond wait %s:%i\n"   , cond, filename, fileline);
#endif
	return SDL_CondWait((SDL_cond *)cond, (SDL_mutex *)mutex);
}

void *_Thread_CreateThread(int (*fn)(void *), void *data, const char *filename, int fileline)
{
	void *thread = (void *)SDL_CreateThread(fn, data);
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p thread create %s:%i\n"   , thread, filename, fileline);
#endif
	return thread;
}

int _Thread_WaitThread(void *thread, int retval, const char *filename, int fileline)
{
	int status = retval;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p thread wait %s:%i\n"   , thread, filename, fileline);
#endif
	SDL_WaitThread((SDL_Thread *)thread, &status);
	return status;
}

	
