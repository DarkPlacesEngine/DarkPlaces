#ifndef THREAD_H

// enable Sys_PrintfToTerminal calls on nearly every threading call
//#define THREADDEBUG
//#define THREADDISABLE
// use recursive mutex (non-posix) extensions in thread_pthread
#define THREADRECURSIVE

typedef int Thread_SpinLock;
typedef struct {int value;} Thread_Atomic;

#define Thread_CreateMutex()              (_Thread_CreateMutex(__FILE__, __LINE__))
#define Thread_DestroyMutex(m)            (_Thread_DestroyMutex(m, __FILE__, __LINE__))
#define Thread_LockMutex(m)               (_Thread_LockMutex(m, __FILE__, __LINE__))
#define Thread_UnlockMutex(m)             (_Thread_UnlockMutex(m, __FILE__, __LINE__))
#define Thread_CreateCond()               (_Thread_CreateCond(__FILE__, __LINE__))
#define Thread_DestroyCond(cond)          (_Thread_DestroyCond(cond, __FILE__, __LINE__))
#define Thread_CondSignal(cond)           (_Thread_CondSignal(cond, __FILE__, __LINE__))
#define Thread_CondBroadcast(cond)        (_Thread_CondBroadcast(cond, __FILE__, __LINE__))
#define Thread_CondWait(cond, mutex)      (_Thread_CondWait(cond, mutex, __FILE__, __LINE__))
#define Thread_CreateThread(fn, data)     (_Thread_CreateThread(fn, data, __FILE__, __LINE__))
#define Thread_WaitThread(thread, retval) (_Thread_WaitThread(thread, retval, __FILE__, __LINE__))
#define Thread_CreateBarrier(count)       (_Thread_CreateBarrier(count, __FILE__, __LINE__))
#define Thread_DestroyBarrier(barrier)    (_Thread_DestroyBarrier(barrier, __FILE__, __LINE__))
#define Thread_WaitBarrier(barrier)       (_Thread_WaitBarrier(barrier, __FILE__, __LINE__))
#define Thread_AtomicGet(a)               (_Thread_AtomicGet(a, __FILE__, __LINE__))
#define Thread_AtomicSet(a, v)            (_Thread_AtomicSet(a, v, __FILE__, __LINE__))
#define Thread_AtomicAdd(a, v)            (_Thread_AtomicAdd(a, v, __FILE__, __LINE__))
#define Thread_AtomicIncRef(a)            (_Thread_AtomicIncRef(a, __FILE__, __LINE__))
#define Thread_AtomicDecRef(a)            (_Thread_AtomicDecRef(a, __FILE__, __LINE__))
#define Thread_AtomicTryLock(lock)        (_Thread_AtomicTryLock(lock, __FILE__, __LINE__))
#define Thread_AtomicLock(lock)           (_Thread_AtomicLock(lock, __FILE__, __LINE__))
#define Thread_AtomicUnlock(lock)         (_Thread_AtomicUnlock(lock, __FILE__, __LINE__))

int Thread_Init(void);
void Thread_Shutdown(void);
qboolean Thread_HasThreads(void);
void *_Thread_CreateMutex(const char *filename, int fileline);
void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline);
int _Thread_LockMutex(void *mutex, const char *filename, int fileline);
int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline);
void *_Thread_CreateCond(const char *filename, int fileline);
void _Thread_DestroyCond(void *cond, const char *filename, int fileline);
int _Thread_CondSignal(void *cond, const char *filename, int fileline);
int _Thread_CondBroadcast(void *cond, const char *filename, int fileline);
int _Thread_CondWait(void *cond, void *mutex, const char *filename, int fileline);
void *_Thread_CreateThread(int (*fn)(void *), void *data, const char *filename, int fileline);
int _Thread_WaitThread(void *thread, int retval, const char *filename, int fileline);
void *_Thread_CreateBarrier(unsigned int count, const char *filename, int fileline);
void _Thread_DestroyBarrier(void *barrier, const char *filename, int fileline);
void _Thread_WaitBarrier(void *barrier, const char *filename, int fileline);
int _Thread_AtomicGet(Thread_Atomic *ref, const char *filename, int fileline);
int _Thread_AtomicSet(Thread_Atomic *ref, int v, const char *filename, int fileline);
int _Thread_AtomicAdd(Thread_Atomic *ref, int v, const char *filename, int fileline);
void _Thread_AtomicIncRef(Thread_Atomic *ref, const char *filename, int fileline);
qboolean _Thread_AtomicDecRef(Thread_Atomic *ref, const char *filename, int fileline);
qboolean _Thread_AtomicTryLock(Thread_SpinLock *lock, const char *filename, int fileline);
void _Thread_AtomicLock(Thread_SpinLock *lock, const char *filename, int fileline);
void _Thread_AtomicUnlock(Thread_SpinLock *lock, const char *filename, int fileline);

typedef struct taskqueue_task_s
{
	// doubly linked list
	struct taskqueue_task_s * volatile prev;
	struct taskqueue_task_s * volatile next;

	// if not NULL, this task must be done before this one will dequeue (faster than simply Yielding immediately)
	struct taskqueue_task_s *preceding;

	// see TaskQueue_IsDone() to use proper atomics to poll done status
	volatile int started;
	volatile int done;

	// function to call, and parameters for it to use
	void(*func)(struct taskqueue_task_s *task);
	void *p[4];
	size_t i[4];

	// stats:
	unsigned int yieldcount; // number of times this task has been requeued
}
taskqueue_task_t;

// immediately execute any pending tasks if threading is disabled (or if force is true)
// TRY NOT TO USE THIS IF POSSIBLE - poll task->done instead.
void TaskQueue_Execute(qboolean force);

// queue the tasks to be executed, or executes them immediately if threading is disabled.
void TaskQueue_Enqueue(int numtasks, taskqueue_task_t *tasks);

// if the task can not be completed due yet to preconditions, just enqueue it again...
void TaskQueue_Yield(taskqueue_task_t *t);

// polls for status of task and returns the result immediately - use this instead of checking ->done directly, as this uses atomics
qboolean TaskQueue_IsDone(taskqueue_task_t *t);

// polls for status of task and waits for it to be done
void TaskQueue_WaitForTaskDone(taskqueue_task_t *t);

// updates thread count based on the cvar.
void TaskQueue_Frame(qboolean shutdown);

// convenience function for setting up a task structure.  Does not do the Enqueue, just fills in the struct.
void TaskQueue_Setup(taskqueue_task_t *t, taskqueue_task_t *preceding, void(*func)(taskqueue_task_t *), size_t i0, size_t i1, void *p0, void *p1);

// general purpose tasks
// t->i[0] = number of tasks in array
// t->p[0] = array of taskqueue_task_t to check
void TaskQueue_Task_CheckTasksDone(taskqueue_task_t *t);

#endif
