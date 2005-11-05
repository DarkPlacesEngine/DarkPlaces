
#ifndef QTYPES_H
#define QTYPES_H

#undef true
#undef false

#ifndef __cplusplus
typedef enum qboolean_e {false, true} qboolean;
#else
typedef bool qboolean;
#endif

#if defined(WIN32) && !defined(WIN64)
# define ssize_t long
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef FALSE
#define FALSE false
#define TRUE true
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#endif
