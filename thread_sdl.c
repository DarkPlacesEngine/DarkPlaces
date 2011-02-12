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

void *Thread_CreateMutex(void)
{
	return SDL_CreateMutex();
}

void Thread_DestroyMutex(void *mutex)
{
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

int Thread_LockMutex(void *mutex)
{
	return SDL_LockMutex((SDL_mutex *)mutex);
}

int Thread_UnlockMutex(void *mutex)
{
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

	
