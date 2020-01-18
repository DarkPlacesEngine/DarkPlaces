
#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include "qtypes.h"
#include "thread.h"

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

// convenience function for setting up a task structure.  Does not do the Enqueue, just fills in the struct.
void TaskQueue_Setup(taskqueue_task_t *t, taskqueue_task_t *preceding, void(*func)(taskqueue_task_t *), size_t i0, size_t i1, void *p0, void *p1);

// general purpose tasks
// t->i[0] = number of tasks in array
// t->p[0] = array of taskqueue_task_t to check
void TaskQueue_Task_CheckTasksDone(taskqueue_task_t *t);

void TaskQueue_Init(void);
void TaskQueue_Shutdown(void);
void TaskQueue_Frame(qboolean shutdown);

#endif
