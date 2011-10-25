#include "quakedef.h"
#include "thread.h"
#include <pthread.h>
#include <stdint.h>

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
	pthread_mutex_t *mutexp = (pthread_mutex_t *) Z_Malloc(sizeof(pthread_mutex_t));
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p create %s:%i\n" , mutexp, filename, fileline);
#endif
	pthread_mutex_init(mutexp, NULL);
	return mutexp;
}

void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p destroy %s:%i\n", mutex, filename, fileline);
#endif
	pthread_mutex_destroy(mutexp);
	Z_Free(mutexp);
}

int _Thread_LockMutex(void *mutex, const char *filename, int fileline)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p lock %s:%i\n"   , mutex, filename, fileline);
#endif
	return pthread_mutex_lock(mutexp);
}

int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline)
{
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p unlock %s:%i\n" , mutex, filename, fileline);
#endif
	return pthread_mutex_unlock(mutexp);
}

void *Thread_CreateCond(void)
{
	pthread_cond_t *condp = (pthread_cond_t *) Z_Malloc(sizeof(pthread_cond_t));
	pthread_cond_init(condp, NULL);
	return condp;
}

void Thread_DestroyCond(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	pthread_cond_destroy(condp);
	Z_Free(condp);
}

int Thread_CondSignal(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	return pthread_cond_signal(condp);
}

int Thread_CondBroadcast(void *cond)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	return pthread_cond_broadcast(condp);
}

int Thread_CondWait(void *cond, void *mutex)
{
	pthread_cond_t *condp = (pthread_cond_t *) cond;
	pthread_mutex_t *mutexp = (pthread_mutex_t *) mutex;
	return pthread_cond_wait(condp, mutexp);
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	pthread_t *threadp = (pthread_t *) Z_Malloc(sizeof(pthread_t));
	int r = pthread_create(threadp, NULL, (void * (*) (void *)) fn, data);
	if(r)
	{
		Z_Free(threadp);
		return NULL;
	}
	return threadp;
}

int Thread_WaitThread(void *thread, int retval)
{
	pthread_t *threadp = (pthread_t *) thread;
	void *status = (void *) (intptr_t) retval;
	pthread_join(*threadp, &status);
	Z_Free(threadp);
	return (int) (intptr_t) status;
}


