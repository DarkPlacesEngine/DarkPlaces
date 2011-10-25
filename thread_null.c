#include "quakedef.h"
#include "thread.h"

int Thread_Init(void)
{
	return 0;
}

void Thread_Shutdown(void)
{
}

qboolean Thread_HasThreads(void)
{
	return false;
}

void *_Thread_CreateMutex(const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p create %s:%i\n" , mutex, filename, fileline);
#endif
	return NULL;
}

void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p destroy %s:%i\n", mutex, filename, fileline);
#endif
}

int _Thread_LockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p lock %s:%i\n"   , mutex, filename, fileline);
#endif
	return -1;
}

int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p unlock %s:%i\n" , mutex, filename, fileline);
#endif
	return -1;
}

void *Thread_CreateCond(void)
{
	return NULL;
}

void Thread_DestroyCond(void *cond)
{
}

int Thread_CondSignal(void *cond)
{
	return -1;
}

int Thread_CondBroadcast(void *cond)
{
	return -1;
}

int Thread_CondWait(void *cond, void *mutex)
{
	return -1;
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	return NULL;
}

int Thread_WaitThread(void *thread, int retval)
{
	return retval;
}
	
