#ifndef THREAD_H

int Thread_Init(void);
void Thread_Shutdown(void);
qboolean Thread_HasThreads(void);
void *Thread_CreateMutex(void);
void Thread_DestroyMutex(void *mutex);
int Thread_LockMutex(void *mutex);
int Thread_UnlockMutex(void *mutex);
void *Thread_CreateCond(void);
void Thread_DestroyCond(void *cond);
int Thread_CondSignal(void *cond);
int Thread_CondBroadcast(void *cond);
int Thread_CondWait(void *cond, void *mutex);
void *Thread_CreateThread(int (*fn)(void *), void *data);
int Thread_WaitThread(void *thread, int retval);

#endif

