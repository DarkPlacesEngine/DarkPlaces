
#ifndef QTYPES_H
#define QTYPES_H

typedef unsigned char qbyte;

#undef true
#undef false

typedef enum {false, true} qboolean;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

//define	PARANOID			// speed sapping error checking
#ifdef _DEBUG
#define ASSERT(condition) if (!(condition)) Sys_Error("assertion (##condition) failed at " __FILE__ ":" __LINE__ "\n");
#else
#define ASSERT(condition)
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#endif
