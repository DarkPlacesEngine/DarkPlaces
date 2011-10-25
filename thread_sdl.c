#include "quakedef.h"
#include "thread.h"
#include <SDL.h>
#include <SDL_thread.h>

int Thread_Init(void)
{
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
	return true;
}

void *_Thread_CreateMutex(const char *filename, int fileline)
{
	void *mutex = SDL_CreateMutex();
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p create %s:%i\n" , mutex, filename, fileline);
#endif
	return mutex;
}

void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p destroy %s:%i\n", mutex, filename, fileline);
#endif
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

int _Thread_LockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p lock %s:%i\n"   , mutex, filename, fileline);
#endif
	return SDL_LockMutex((SDL_mutex *)mutex);
}

int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p unlock %s:%i\n" , mutex, filename, fileline);
#endif
	return SDL_UnlockMutex((SDL_mutex *)mutex);
}

void *Thread_CreateCond(void)
{
	return SDL_CreateCond();
}

void Thread_DestroyCond(void *cond)
{
	SDL_DestroyCond((SDL_cond *)cond);
}

int Thread_CondSignal(void *cond)
{
	return SDL_CondSignal((SDL_cond *)cond);
}

int Thread_CondBroadcast(void *cond)
{
	return SDL_CondBroadcast((SDL_cond *)cond);
}

int Thread_CondWait(void *cond, void *mutex)
{
	return SDL_CondWait((SDL_cond *)cond, (SDL_mutex *)mutex);
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	return SDL_CreateThread(fn, data);
}

int Thread_WaitThread(void *thread, int retval)
{
	int status = retval;
	SDL_WaitThread((SDL_Thread *)thread, &status);
	return status;
}

	
