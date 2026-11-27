#include "quakedef.h"
#include "taskqueue.h"

cvar_t taskqueue_minthreads = {CF_CLIENT | CF_SERVER | CF_ARCHIVE, "taskqueue_minthreads", "0", "minimum number of threads to keep active for executing tasks"};
cvar_t taskqueue_maxthreads = {CF_CLIENT | CF_SERVER | CF_ARCHIVE, "taskqueue_maxthreads", "32", "maximum number of threads to start up as needed based on task count"};
cvar_t taskqueue_tasksperthread = {CF_CLIENT | CF_SERVER | CF_ARCHIVE, "taskqueue_tasksperthread", "4000", "expected amount of work that a single thread can do in a frame - the number of threads being used depends on the average workload in recent frames"};

#define MAXTHREADS 1024
#define RECENTFRAMES 64 // averaging thread activity over this many frames to decide how many threads we need
#define THREADTASKS 256 // thread can hold this many tasks in its own queue
#define THREADBATCH 64 // thread will run this many tasks before checking status again
#define THREADSLEEPCOUNT 10000 // thread will sleep until woken by the distributor if it checks this many times and has no work to do
#define CACHELINESIZE 128 // largest cache line size of the supported platforms (POWER and Apple ARM64 use 128, most others 64)

typedef struct taskqueue_state_thread_s
{
	void *handle;
	unsigned int thread_index;

	// used by the thread to sleep when it has no work to do
	void *mutex;
	void *cond;
	qbool wake; // protected by mutex

	// these are written by the distributor (while holding command_lock)
	Thread_Atomic quit;
	qbool sleeping;
	Thread_Atomic enqueueposition;

	// keep the thread's own writes off the cache line the distributor writes to
	unsigned char padding1[CACHELINESIZE];

	// these are written by the thread
	Thread_Atomic dequeueposition;
	unsigned int tasks_completed;

	unsigned char padding2[CACHELINESIZE];

	taskqueue_task_t *queue[THREADTASKS];
}
taskqueue_state_thread_t;

typedef struct taskqueue_state_s
{
	// TaskQueue_DistributeTasks cycles through the threads when assigning, each has its own queue
	unsigned int enqueuethread;
	int numthreads;
	taskqueue_state_thread_t threads[MAXTHREADS];

	// synchronization point for enqueue and some other memory access
	Thread_SpinLock command_lock;

	// distributor queue (not assigned to threads yet, or waiting on other tasks)
	unsigned int queue_enqueueposition;
	unsigned int queue_dequeueposition;
	unsigned int queue_size;
	taskqueue_task_t **queue_data;

	// metrics to balance workload vs cpu resources
	unsigned int tasks_recentframesindex;
	unsigned int tasks_recentframes[RECENTFRAMES];
	unsigned int tasks_thisframe;
	unsigned int tasks_averageperframe;
}
taskqueue_state_t;

static taskqueue_state_t taskqueue_state;

void TaskQueue_Init(void)
{
	Cvar_RegisterVariable(&taskqueue_minthreads);
	Cvar_RegisterVariable(&taskqueue_maxthreads);
	Cvar_RegisterVariable(&taskqueue_tasksperthread);
}

void TaskQueue_Shutdown(void)
{
	if (taskqueue_state.numthreads)
		TaskQueue_Frame(true);
}

static void TaskQueue_ExecuteTask(taskqueue_task_t *t)
{
	// see if t is waiting on something
	if (t->preceding && !TaskQueue_IsDone(t->preceding))
		TaskQueue_Yield(t);
	else
		t->func(t);
}

// FIXME: don't use mutex
// FIXME: this is basically fibers but less featureful - context switching for yield is not implemented
static int TaskQueue_ThreadFunc(void *d)
{
	taskqueue_state_thread_t *s = (taskqueue_state_thread_t *)d;
	unsigned int dequeueposition = (unsigned int)Thread_AtomicGet(&s->dequeueposition);
	unsigned int sleepcounter = 0;
	for (;;)
	{
		qbool gotosleep;
		while (dequeueposition != (unsigned int)Thread_AtomicGet(&s->enqueueposition))
		{
			taskqueue_task_t *t = s->queue[dequeueposition % THREADTASKS];
			TaskQueue_ExecuteTask(t);
			// when we advance, also clear the pointer for good measure
			s->queue[dequeueposition++ % THREADTASKS] = NULL;
			// Add rather than Set because we need a full barrier here (see TaskQueue_DistributeTasks)
			Thread_AtomicAdd(&s->dequeueposition, 1);
			sleepcounter = 0;
		}
		if (Thread_AtomicGet(&s->quit))
			break;
		if (++sleepcounter >= THREADSLEEPCOUNT)
		{
			// the distributor only gives us tasks or tells us to quit while holding command_lock, so if we set sleeping here it can not miss that we need a wakeup
			Thread_AtomicLock(&taskqueue_state.command_lock);
			gotosleep = !Thread_AtomicGet(&s->quit) && dequeueposition == (unsigned int)Thread_AtomicGet(&s->enqueueposition);
			if (gotosleep)
				s->sleeping = true;
			Thread_AtomicUnlock(&taskqueue_state.command_lock);
			if (!gotosleep)
				continue;
			Thread_LockMutex(s->mutex);
			while (!s->wake)
				Thread_CondWait(s->cond, s->mutex);
			s->wake = false;
			Thread_UnlockMutex(s->mutex);
			sleepcounter = 0;
		}
	}
	return 0;
}

// wakes up a thread that went to sleep for lack of work, call this with command_lock held
static void TaskQueue_WakeThread(taskqueue_state_thread_t *s)
{
	if (!s->sleeping)
		return;
	s->sleeping = false;
	Thread_LockMutex(s->mutex);
	s->wake = true;
	Thread_CondSignal(s->cond);
	Thread_UnlockMutex(s->mutex);
}

void TaskQueue_Enqueue(int numtasks, taskqueue_task_t *tasks)
{
	int i;
	unsigned int pending;
	Thread_AtomicLock(&taskqueue_state.command_lock);
	pending = (taskqueue_state.queue_enqueueposition < taskqueue_state.queue_dequeueposition ? taskqueue_state.queue_size : 0) +
		taskqueue_state.queue_enqueueposition - taskqueue_state.queue_dequeueposition;
	// the queue must never become completely full, because then it would look empty
	if (taskqueue_state.queue_size <= pending + numtasks)
	{
		// we have to grow the queue...
		unsigned int j;
		unsigned int newsize = (taskqueue_state.queue_size + numtasks) * 2;
		taskqueue_task_t **newdata;
		if (newsize < 1024)
			newsize = 1024;
		newdata = (taskqueue_task_t **)Mem_Alloc(zonemempool, sizeof(*newdata) * newsize);
		// the pending tasks may wrap around the end of the old queue, so move them to the start of the new one
		for (j = 0; j < pending; j++)
			newdata[j] = taskqueue_state.queue_data[(taskqueue_state.queue_dequeueposition + j) % taskqueue_state.queue_size];
		if (taskqueue_state.queue_data)
			Mem_Free(taskqueue_state.queue_data);
		taskqueue_state.queue_data = newdata;
		taskqueue_state.queue_size = newsize;
		taskqueue_state.queue_dequeueposition = 0;
		taskqueue_state.queue_enqueueposition = pending;
	}
	for (i = 0; i < numtasks; i++)
	{
		if (tasks[i].yieldcount == 0)
			taskqueue_state.tasks_thisframe++;
		taskqueue_state.queue_data[taskqueue_state.queue_enqueueposition] = &tasks[i];
		taskqueue_state.queue_enqueueposition++;
		if (taskqueue_state.queue_enqueueposition >= taskqueue_state.queue_size)
			taskqueue_state.queue_enqueueposition = 0;
	}
	Thread_AtomicUnlock(&taskqueue_state.command_lock);
}

// if the task can not be completed due yet to preconditions, just enqueue it again...
void TaskQueue_Yield(taskqueue_task_t *t)
{
	t->yieldcount++;
	TaskQueue_Enqueue(1, t);
}

qbool TaskQueue_IsDone(taskqueue_task_t *t)
{
	return Thread_AtomicGet(&t->done) != 0;
}

void TaskQueue_SetDone(taskqueue_task_t *t)
{
	// Add rather than Set because we need a full barrier here: on weakly ordered CPUs (POWER, ARM)
	// another thread must not see the done status before it can see the results the task wrote
	Thread_AtomicAdd(&t->done, 1);
}

static void TaskQueue_DistributeTasks(void)
{
	taskqueue_task_t *t;
	Thread_AtomicLock(&taskqueue_state.command_lock);
	if (taskqueue_state.numthreads > 0)
	{
		unsigned int attempts = taskqueue_state.numthreads;
		while (attempts-- > 0 && taskqueue_state.queue_enqueueposition != taskqueue_state.queue_dequeueposition)
		{
			t = taskqueue_state.queue_data[taskqueue_state.queue_dequeueposition];
			if (t->preceding && !TaskQueue_IsDone(t->preceding))
			{
				// task is waiting on something
				// first dequeue it properly
				taskqueue_state.queue_data[taskqueue_state.queue_dequeueposition] = NULL;
				taskqueue_state.queue_dequeueposition++;
				if (taskqueue_state.queue_dequeueposition >= taskqueue_state.queue_size)
					taskqueue_state.queue_dequeueposition = 0;
				// now put it back in the distributor queue - we know there is room because we just made room
				taskqueue_state.queue_data[taskqueue_state.queue_enqueueposition] = t;
				taskqueue_state.queue_enqueueposition++;
				if (taskqueue_state.queue_enqueueposition >= taskqueue_state.queue_size)
					taskqueue_state.queue_enqueueposition = 0;
				// we do not refresh the attempt counter here to avoid deadlock - quite often the only things sitting in the distributor queue are waiting on other tasks
			}
			else
			{
				// cycle through the threads, if this one has no room we try the next one on the next attempt
				taskqueue_state_thread_t *s = &taskqueue_state.threads[taskqueue_state.enqueuethread++ % taskqueue_state.numthreads];
				unsigned int enqueueposition = (unsigned int)Thread_AtomicGet(&s->enqueueposition);
				if (enqueueposition - (unsigned int)Thread_AtomicGet(&s->dequeueposition) < THREADTASKS)
				{
					// add the task to the thread's queue
					// the thread reads its queue without holding command_lock, so the task pointer has to be visible to it before the new enqueueposition is,
					// Add rather than Set because Set is not a full barrier in some implementations
					s->queue[enqueueposition % THREADTASKS] = t;
					Thread_AtomicAdd(&s->enqueueposition, 1);
					TaskQueue_WakeThread(s);
					// since we succeeded in assigning the task, advance the distributor queue
					taskqueue_state.queue_data[taskqueue_state.queue_dequeueposition] = NULL;
					taskqueue_state.queue_dequeueposition++;
					if (taskqueue_state.queue_dequeueposition >= taskqueue_state.queue_size)
						taskqueue_state.queue_dequeueposition = 0;
					// refresh our attempt counter because we did manage to assign something to a thread
					attempts = taskqueue_state.numthreads;
				}
			}
		}
	}
	// execute one pending task on the distributor queue, this matters if numthreads is 0
	// (dequeue it while we still hold the lock, because threads can enqueue tasks which may move queue_data)
	t = NULL;
	if (taskqueue_state.queue_dequeueposition != taskqueue_state.queue_enqueueposition)
	{
		t = taskqueue_state.queue_data[taskqueue_state.queue_dequeueposition];
		taskqueue_state.queue_dequeueposition++;
		if (taskqueue_state.queue_dequeueposition >= taskqueue_state.queue_size)
			taskqueue_state.queue_dequeueposition = 0;
	}
	Thread_AtomicUnlock(&taskqueue_state.command_lock);
	if (t)
		TaskQueue_ExecuteTask(t);
}

void TaskQueue_WaitForTaskDone(taskqueue_task_t *t)
{
	while (!TaskQueue_IsDone(t))
		TaskQueue_DistributeTasks();
}

void TaskQueue_Frame(qbool shutdown)
{
	int i;
	unsigned long long int avg;
	int maxthreads = bound(0, taskqueue_maxthreads.integer, MAXTHREADS);
	int numthreads = maxthreads;
	int tasksperthread = bound(10, taskqueue_tasksperthread.integer, 100000);
#ifdef THREADDISABLE
	numthreads = 0;
#endif

	Thread_AtomicLock(&taskqueue_state.command_lock);
	taskqueue_state.tasks_recentframesindex = (taskqueue_state.tasks_recentframesindex + 1) % RECENTFRAMES;
	taskqueue_state.tasks_recentframes[taskqueue_state.tasks_recentframesindex] = taskqueue_state.tasks_thisframe;
	taskqueue_state.tasks_thisframe = 0;
	avg = 0;
	for (i = 0; i < RECENTFRAMES; i++)
		avg += taskqueue_state.tasks_recentframes[i];
	taskqueue_state.tasks_averageperframe = avg / RECENTFRAMES;
	Thread_AtomicUnlock(&taskqueue_state.command_lock);

	numthreads = taskqueue_state.tasks_averageperframe / tasksperthread;
	numthreads = bound(taskqueue_minthreads.integer, numthreads, taskqueue_maxthreads.integer);

	if (shutdown)
		numthreads = 0;

	// check if we need to close some threads
	if (taskqueue_state.numthreads > numthreads)
	{
		// tell extra threads to quit
		Thread_AtomicLock(&taskqueue_state.command_lock);
		for (i = numthreads; i < taskqueue_state.numthreads; i++)
		{
			Thread_AtomicSet(&taskqueue_state.threads[i].quit, 1);
			TaskQueue_WakeThread(&taskqueue_state.threads[i]);
		}
		Thread_AtomicUnlock(&taskqueue_state.command_lock);
		for (i = numthreads; i < taskqueue_state.numthreads; i++)
		{
			if (taskqueue_state.threads[i].handle)
				Thread_WaitThread(taskqueue_state.threads[i].handle, 0);
			taskqueue_state.threads[i].handle = NULL;
			Thread_DestroyCond(taskqueue_state.threads[i].cond);
			Thread_DestroyMutex(taskqueue_state.threads[i].mutex);
		}
		// okay we're at the new state now
		taskqueue_state.numthreads = numthreads;
	}

	// check if we need to start more threads
	if (taskqueue_state.numthreads < numthreads)
	{
		// make sure we're not telling new threads to just quit on startup
		Thread_AtomicLock(&taskqueue_state.command_lock);
		for (i = taskqueue_state.numthreads; i < numthreads; i++)
			Thread_AtomicSet(&taskqueue_state.threads[i].quit, 0);
		Thread_AtomicUnlock(&taskqueue_state.command_lock);

		// start new threads
		for (i = taskqueue_state.numthreads; i < numthreads; i++)
		{
			taskqueue_state.threads[i].thread_index = i;
			taskqueue_state.threads[i].sleeping = false;
			taskqueue_state.threads[i].wake = false;
			taskqueue_state.threads[i].mutex = Thread_CreateMutex();
			taskqueue_state.threads[i].cond = Thread_CreateCond();
			taskqueue_state.threads[i].handle = Thread_CreateThread(TaskQueue_ThreadFunc, &taskqueue_state.threads[i]);
		}

		// okay we're at the new state now
		taskqueue_state.numthreads = numthreads;
	}

	// just for good measure, distribute any pending tasks that span across frames
	TaskQueue_DistributeTasks();
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
	taskqueue_task_t *tasks = (taskqueue_task_t *)t->p[0];
	while (numtasks > 0)
	{
		// check the last task first as it's usually going to be the last to finish, so we do the least work by checking it first
		if (!TaskQueue_IsDone(&tasks[numtasks - 1]))
		{
			// update our partial progress, then yield to another pending task.
			t->i[0] = numtasks;
			// set our preceding task to one of the ones we are watching for
			t->preceding = &tasks[numtasks - 1];
			TaskQueue_Yield(t);
			return;
		}
		numtasks--;
	}
	TaskQueue_SetDone(t);
}
