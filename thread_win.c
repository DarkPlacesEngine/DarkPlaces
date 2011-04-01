#include "quakedef.h"
#include "thread.h"
#include <process.h>

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
	return (void *)CreateMutex(NULL, FALSE, NULL);
}

void Thread_DestroyMutex(void *mutex)
{
	CloseHandle(mutex);
}

int Thread_LockMutex(void *mutex)
{
	return (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) ? -1 : 0;
}

int Thread_UnlockMutex(void *mutex)
{
	return (ReleaseMutex(mutex) == FALSE) ? -1 : 0;
}

typedef struct thread_semaphore_s
{
	HANDLE semaphore;
	volatile LONG value;
}
thread_semaphore_t;

static thread_semaphore_t *Thread_CreateSemaphore(unsigned int v)
{
	thread_semaphore_t *s = (thread_semaphore_t *)calloc(sizeof(*s), 1);
	s->semaphore = CreateSemaphore(NULL, v, 32768, NULL);
	s->value = v;
	return s;
}

static void Thread_DestroySemaphore(thread_semaphore_t *s)
{
	CloseHandle(s->semaphore);
	free(s);
}

static int Thread_WaitSemaphore(thread_semaphore_t *s, unsigned int msec)
{
	int r = WaitForSingleObject(s->semaphore, msec);
	if (r == WAIT_OBJECT_0)
	{
		InterlockedDecrement(&s->value);
		return 0;
	}
	if (r == WAIT_TIMEOUT)
		return 1;
	return -1;
}

static int Thread_PostSemaphore(thread_semaphore_t *s)
{
	InterlockedIncrement(&s->value);
	if (ReleaseSemaphore(s->semaphore, 1, NULL))
		return 0;
	InterlockedDecrement(&s->value);
	return -1;
}

typedef struct thread_cond_s
{
	HANDLE mutex;
	int waiting;
	int signals;
	thread_semaphore_t *sem;
	thread_semaphore_t *done;
}
thread_cond_t;

void *Thread_CreateCond(void)
{
	thread_cond_t *c = (thread_cond_t *)calloc(sizeof(*c), 1);
	c->mutex = CreateMutex(NULL, FALSE, NULL);
	c->sem = Thread_CreateSemaphore(0);
	c->done = Thread_CreateSemaphore(0);
	c->waiting = 0;
	c->signals = 0;
	return c;
}

void Thread_DestroyCond(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	Thread_DestroySemaphore(c->sem);
	Thread_DestroySemaphore(c->done);
	CloseHandle(c->mutex);
}

int Thread_CondSignal(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int n;
	WaitForSingleObject(c->mutex, INFINITE);
	n = c->waiting - c->signals;
	if (n > 0)
	{
		c->signals++;
		Thread_PostSemaphore(c->sem);
	}
	ReleaseMutex(c->mutex);
	if (n > 0)
		Thread_WaitSemaphore(c->done, INFINITE);
	return 0;
}

int Thread_CondBroadcast(void *cond)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int i = 0;
	int n = 0;
	WaitForSingleObject(c->mutex, INFINITE);
	n = c->waiting - c->signals;
	if (n > 0)
	{
		c->signals += n;
		for (i = 0;i < n;i++)
			Thread_PostSemaphore(c->sem);
	}
	ReleaseMutex(c->mutex);
	for (i = 0;i < n;i++)
		Thread_WaitSemaphore(c->done, INFINITE);
	return 0;
}

int Thread_CondWait(void *cond, void *mutex)
{
	thread_cond_t *c = (thread_cond_t *)cond;
	int waitresult;

	WaitForSingleObject(c->mutex, INFINITE);
	c->waiting++;
	ReleaseMutex(c->mutex);

	ReleaseMutex(mutex);

	waitresult = Thread_WaitSemaphore(c->sem, INFINITE);
	WaitForSingleObject(c->mutex, INFINITE);
	if (c->signals > 0)
	{
		if (waitresult > 0)
			Thread_WaitSemaphore(c->sem, INFINITE);
		Thread_PostSemaphore(c->done);
		c->signals--;
	}
	c->waiting--;
	ReleaseMutex(c->mutex);

	WaitForSingleObject(mutex, INFINITE);
	return waitresult;
}

typedef struct threadwrapper_s
{
	HANDLE handle;
	unsigned int threadid;
	int result;
	int (*fn)(void *);
	void *data;
}
threadwrapper_t;

unsigned int __stdcall Thread_WrapperFunc(void *d)
{
	threadwrapper_t *w = (threadwrapper_t *)d;
	w->result = w->fn(w->data);
	_endthreadex(w->result);
	return w->result;
}

void *Thread_CreateThread(int (*fn)(void *), void *data)
{
	threadwrapper_t *w = (threadwrapper_t *)calloc(sizeof(*w), 1);
	w->fn = fn;
	w->data = data;
	w->threadid = 0;
	w->result = 0;
	w->handle = (HANDLE)_beginthreadex(NULL, 0, Thread_WrapperFunc, (void *)w, 0, &w->threadid);
	return (void *)w;
}

int Thread_WaitThread(void *d, int retval)
{
	threadwrapper_t *w = (threadwrapper_t *)d;
	WaitForSingleObject(w->handle, INFINITE);
	CloseHandle(w->handle);
	retval = w->result;
	free(w);
	return retval;
}
