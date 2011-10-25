#ifndef THREAD_H

//#define THREADDEBUG

#define Thread_CreateMutex() (_Thread_CreateMutex(__FILE__, __LINE__))
#define Thread_DestroyMutex(m) (_Thread_DestroyMutex(m, __FILE__, __LINE__))
#define Thread_LockMutex(m) (_Thread_LockMutex(m, __FILE__, __LINE__))
#define Thread_UnlockMutex(m) (_Thread_UnlockMutex(m, __FILE__, __LINE__))

int Thread_Init(void);
void Thread_Shutdown(void);
qboolean Thread_HasThreads(void);
void *_Thread_CreateMutex(const char *filename, int fileline);
void _Thread_DestroyMutex(void *mutex, const char *filename, int fileline);
int _Thread_LockMutex(void *mutex, const char *filename, int fileline);
int _Thread_UnlockMutex(void *mutex, const char *filename, int fileline);
void *Thread_CreateCond(void);
void Thread_DestroyCond(void *cond);
int Thread_CondSignal(void *cond);
int Thread_CondBroadcast(void *cond);
int Thread_CondWait(void *cond, void *mutex);
void *Thread_CreateThread(int (*fn)(void *), void *data);
int Thread_WaitThread(void *thread, int retval);

#endif

