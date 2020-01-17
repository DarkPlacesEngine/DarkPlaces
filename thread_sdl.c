#include <SDL.h>
#include <SDL_thread.h>
#include "quakedef.h"
#include "thread.h"

cvar_t taskqueue_maxthreads = {CVAR_SAVE, "taskqueue_maxthreads", "32", "how many threads to use for executing tasks"};
cvar_t taskqueue_linkedlist = {CVAR_SAVE, "taskqueue_linkedlist", "1", "whether to use a doubly linked list or an array for the FIFO queue"};

typedef struct taskqueue_state_thread_s
{
	void *handle;
}
taskqueue_state_thread_t;

typedef struct taskqueue_state_s
{
	int numthreads;
	taskqueue_state_thread_t threads[1024];

	// we can enqueue this many tasks before execution of them must proceed
	int queue_used;
	int queue_max; // size of queue array
	taskqueue_task_t **queue_tasks;

	// command 
	Thread_SpinLock command_lock;

	volatile uint64_t threads_quit;

	// doubly linked list - enqueue pushes to list.prev, dequeue pops from list.next
	taskqueue_task_t list;
}
taskqueue_state_t;

static taskqueue_state_t taskqueue_state;

int Thread_Init(void)
{
	Cvar_RegisterVariable(&taskqueue_maxthreads);
	Cvar_RegisterVariable(&taskqueue_linkedlist);
#ifdef THREADDISABLE
	Con_Printf("Threading disabled in this build\n");
#endif
	// initialize the doubly-linked list header
	taskqueue_state.list.next = &taskqueue_state.list;
	taskqueue_state.list.prev = &taskqueue_state.list;
	return 0;
}

void Thread_Shutdown(void)
{
	if (taskqueue_state.numthreads)
		TaskQueue_Frame(true);
	if (taskqueue_state.queue_tasks)
		Mem_Free(taskqueue_state.queue_tasks);
	taskqueue_state.queue_tasks = NULL;
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
	void *thread = (void *)SDL_CreateThread(fn, filename, data);
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

// standard barrier implementation using conds and mutexes
// see: http://www.howforge.com/implementing-barrier-in-pthreads
typedef struct {
	unsigned int needed;
	unsigned int called;
	void *mutex;
	void *cond;
} barrier_t;

void *_Thread_CreateBarrier(unsigned int count, const char *filename, int fileline)
{
	volatile barrier_t *b = (volatile barrier_t *) Z_Malloc(sizeof(barrier_t));
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p barrier create(%d) %s:%i\n", b, count, filename, fileline);
#endif
	b->needed = count;
	b->called = 0;
	b->mutex = Thread_CreateMutex();
	b->cond = Thread_CreateCond();
	return (void *) b;
}

void _Thread_DestroyBarrier(void *barrier, const char *filename, int fileline)
{
	volatile barrier_t *b = (volatile barrier_t *) barrier;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p barrier destroy %s:%i\n", b, filename, fileline);
#endif
	Thread_DestroyMutex(b->mutex);
	Thread_DestroyCond(b->cond);
}

void _Thread_WaitBarrier(void *barrier, const char *filename, int fileline)
{
	volatile barrier_t *b = (volatile barrier_t *) barrier;
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p barrier wait %s:%i\n", b, filename, fileline);
#endif
	Thread_LockMutex(b->mutex);
	b->called++;
	if (b->called == b->needed) {
		b->called = 0;
		Thread_CondBroadcast(b->cond);
	} else {
		do {
			Thread_CondWait(b->cond, b->mutex);
		} while(b->called);
	}
	Thread_UnlockMutex(b->mutex);
}

int _Thread_AtomicGet(Thread_Atomic *a, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic get at %s:%i\n", a, v, filename, fileline);
#endif
	return SDL_AtomicGet((SDL_atomic_t *)a);
}

int _Thread_AtomicSet(Thread_Atomic *a, int v, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic set %v at %s:%i\n", a, v, filename, fileline);
#endif
	return SDL_AtomicSet((SDL_atomic_t *)a, v);
}

int _Thread_AtomicAdd(Thread_Atomic *a, int v, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic add %v at %s:%i\n", a, v, filename, fileline);
#endif
	return SDL_AtomicAdd((SDL_atomic_t *)a, v);
}

void _Thread_AtomicIncRef(Thread_Atomic *a, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic incref %s:%i\n", lock, filename, fileline);
#endif
	SDL_AtomicIncRef((SDL_atomic_t *)a);
}

qboolean _Thread_AtomicDecRef(Thread_Atomic *a, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic decref %s:%i\n", lock, filename, fileline);
#endif
	return SDL_AtomicDecRef((SDL_atomic_t *)a) != SDL_FALSE;
}

qboolean _Thread_AtomicTryLock(Thread_SpinLock *lock, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic try lock %s:%i\n", lock, filename, fileline);
#endif
	return SDL_AtomicTryLock(lock) != SDL_FALSE;
}

void _Thread_AtomicLock(Thread_SpinLock *lock, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic lock %s:%i\n", lock, filename, fileline);
#endif
	SDL_AtomicLock(lock);
}

void _Thread_AtomicUnlock(Thread_SpinLock *lock, const char *filename, int fileline)
{
#ifdef THREADDEBUG
	Sys_PrintfToTerminal("%p atomic unlock %s:%i\n", lock, filename, fileline);
#endif
	SDL_AtomicUnlock(lock);
}

static taskqueue_task_t *TaskQueue_GetPending(void)
{
	taskqueue_task_t *t = NULL;
	if (taskqueue_state.list.next != &taskqueue_state.list)
	{
		// pop from list.next
		t = taskqueue_state.list.next;
		t->next->prev = t->prev;
		t->prev->next = t->next;
		t->prev = t->next = NULL;
	}
	if (t == NULL)
	{
		if (taskqueue_state.queue_used > 0)
		{
			t = taskqueue_state.queue_tasks[0];
			taskqueue_state.queue_used--;
			memmove(taskqueue_state.queue_tasks, taskqueue_state.queue_tasks + 1, taskqueue_state.queue_used * sizeof(taskqueue_task_t *));
			taskqueue_state.queue_tasks[taskqueue_state.queue_used] = NULL;
		}
	}
	return t;
}

static void TaskQueue_ExecuteTask(taskqueue_task_t *t)
{
	// see if t is waiting on something
	if (t->preceding && t->preceding->done == 0)
		TaskQueue_Yield(t);
	else
		t->func(t);
}

// FIXME: don't use mutex
// FIXME: this is basically fibers but less featureful - context switching for yield is not implemented
static int TaskQueue_ThreadFunc(void *d)
{
	for (;;)
	{
		qboolean quit;
		taskqueue_task_t *t = NULL;
		Thread_AtomicLock(&taskqueue_state.command_lock);
		quit = taskqueue_state.threads_quit != 0;
		t = TaskQueue_GetPending();
		Thread_AtomicUnlock(&taskqueue_state.command_lock);
		if (t)
			TaskQueue_ExecuteTask(t);
		else if (quit)
			break;
	}
	return 0;
}

void TaskQueue_Execute(qboolean force)
{
	// if we have no threads to run the tasks, just start executing them now
	if (taskqueue_state.numthreads == 0 || force)
	{
		for (;;)
		{
			taskqueue_task_t *t = NULL;
			Thread_AtomicLock(&taskqueue_state.command_lock);
			t = TaskQueue_GetPending();
			Thread_AtomicUnlock(&taskqueue_state.command_lock);
			if (!t)
				break;
			TaskQueue_ExecuteTask(t);
		}
	}
}

void TaskQueue_Enqueue(int numtasks, taskqueue_task_t *tasks)
{
	int i;
	// try not to spinlock for a long time by breaking up large enqueues
	while (numtasks > 64)
	{
		TaskQueue_Enqueue(64, tasks);
		tasks += 64;
		numtasks -= 64;
	}
	Thread_AtomicLock(&taskqueue_state.command_lock);
	for (i = 0; i < numtasks; i++)
	{
		taskqueue_task_t *t = &tasks[i];
		if (taskqueue_linkedlist.integer)
		{
			// push to list.prev
			t->next = &taskqueue_state.list;
			t->prev = taskqueue_state.list.prev;
			t->next->prev = t;
			t->prev->next = t;
		}
		else
		{
			if (taskqueue_state.queue_used >= taskqueue_state.queue_max)
			{
				taskqueue_state.queue_max *= 2;
				if (taskqueue_state.queue_max < 1024)
					taskqueue_state.queue_max = 1024;
				taskqueue_state.queue_tasks = (taskqueue_task_t **)Mem_Realloc(cls.permanentmempool, taskqueue_state.queue_tasks, taskqueue_state.queue_max * sizeof(taskqueue_task_t *));
			}
			taskqueue_state.queue_tasks[taskqueue_state.queue_used++] = t;
		}
	}
	Thread_AtomicUnlock(&taskqueue_state.command_lock);
}

// if the task can not be completed due yet to preconditions, just enqueue it again...
void TaskQueue_Yield(taskqueue_task_t *t)
{
	t->yieldcount++;
	TaskQueue_Enqueue(1, t);
}

void TaskQueue_WaitForTaskDone(taskqueue_task_t *t)
{
	qboolean done = false;
	while (!done)
	{
		Thread_AtomicLock(&taskqueue_state.command_lock);
		done = t->done != 0;
		Thread_AtomicUnlock(&taskqueue_state.command_lock);
		// if there are no threads, just execute the tasks immediately
		if (!done && taskqueue_state.numthreads == 0)
			TaskQueue_Execute(true);
	}
}

void TaskQueue_Frame(qboolean shutdown)
{
	int numthreads = shutdown ? 0 : bound(0, taskqueue_maxthreads.integer, sizeof(taskqueue_state.threads)/sizeof(taskqueue_state.threads[0]));
#ifdef THREADDISABLE
	numthreads = 0;
#endif
	if (taskqueue_state.numthreads != numthreads)
	{
		int i;
		Thread_AtomicLock(&taskqueue_state.command_lock);
		taskqueue_state.threads_quit = 1;
		Thread_AtomicUnlock(&taskqueue_state.command_lock);
		for (i = 0; i < taskqueue_state.numthreads; i++)
		{
			if (taskqueue_state.threads[i].handle)
				Thread_WaitThread(taskqueue_state.threads[i].handle, 0);
			taskqueue_state.threads[i].handle = NULL;
		}
		Thread_AtomicLock(&taskqueue_state.command_lock);
		taskqueue_state.threads_quit = 0;
		Thread_AtomicUnlock(&taskqueue_state.command_lock);
		taskqueue_state.numthreads = numthreads;
		for (i = 0; i < taskqueue_state.numthreads; i++)
			taskqueue_state.threads[i].handle = Thread_CreateThread(TaskQueue_ThreadFunc, &taskqueue_state.threads[i]);
		// if there are still pending tasks (e.g. no threads), execute them on main thread now
		TaskQueue_Execute(true);
	}
}

void TaskQueue_Setup(taskqueue_task_t *t, taskqueue_task_t *preceding, void(*func)(taskqueue_task_t *), size_t i0, size_t i1, void *p0, void *p1)
{
	memset(t, 0, sizeof(*t));
	t->preceding = preceding;
	t->func = func;
	t->i[0] = i0;
	t->i[1] = i1;
	t->p[0] = p0;
	t->p[1] = p1;
}

void TaskQueue_Task_CheckTasksDone(taskqueue_task_t *t)
{
	size_t numtasks = t->i[0];
	taskqueue_task_t *tasks = t->p[0];
	while (numtasks > 0)
	{
		// check the last task first as it's usually going to be the last to finish, so we do the least work by checking it first
		if (!tasks[numtasks - 1].done)
		{
			// update our partial progress, then yield to another pending task.
			t->i[0] = numtasks;
			TaskQueue_Yield(t);
			return;
		}
		numtasks--;
	}
	t->started = 1;
	t->done = 1;
}
