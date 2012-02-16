#ifndef THREAD_H

// enable Sys_PrintfToTerminal calls on nearly every threading call
//#define THREADDEBUG
//#define THREADDISABLE
// use recursive mutex (non-posix) extensions in thread_pthread
#define THREADRECURSIVE

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

#endif
