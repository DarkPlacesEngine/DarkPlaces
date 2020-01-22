#include "quakedef.h"
#include "taskqueue.h"

cvar_t taskqueue_maxthreads = {CVAR_CLIENT | CVAR_SERVER | CVAR_SAVE, "taskqueue_maxthreads", "32", "how many threads to use for executing tasks"};

typedef struct taskqueue_state_thread_s
{
	void *handle;
}
taskqueue_state_thread_t;

typedef struct taskqueue_state_s
{
	int numthreads;
	taskqueue_state_thread_t threads[1024];

	// command 
	Thread_SpinLock command_lock;

	int threads_quit;

	// doubly linked list - enqueue pushes to list.prev, dequeue pops from list.next
	taskqueue_task_t list;
}
taskqueue_state_t;

static taskqueue_state_t taskqueue_state;

void TaskQueue_Init(void)
{
	Cvar_RegisterVariable(&taskqueue_maxthreads);
	// initialize the doubly-linked list header
	taskqueue_state.list.next = &taskqueue_state.list;
	taskqueue_state.list.prev = &taskqueue_state.list;
}

void TaskQueue_Shutdown(void)
{
	if (taskqueue_state.numthreads)
		TaskQueue_Frame(true);
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
		// push to list.prev
		t->next = &taskqueue_state.list;
		t->prev = taskqueue_state.list.prev;
		t->next->prev = t;
		t->prev->next = t;
	}
	Thread_AtomicUnlock(&taskqueue_state.command_lock);
}

// if the task can not be completed due yet to preconditions, just enqueue it again...
void TaskQueue_Yield(taskqueue_task_t *t)
{
	t->yieldcount++;
	TaskQueue_Enqueue(1, t);
}

qboolean TaskQueue_IsDone(taskqueue_task_t *t)
{
	return !t->done != 0;
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
	int numthreads = shutdown ? 0 : bound(0, taskqueue_maxthreads.integer, sizeof(taskqueue_state.threads) / sizeof(taskqueue_state.threads[0]));
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
