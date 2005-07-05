
#ifndef QTYPES_H
#define QTYPES_H

typedef unsigned char qbyte;

#undef true
#undef false

typedef enum {false, true} qboolean;

#ifdef WIN64
# define ssize_t long long
#elifdef WIN32
# define ssize_t long
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#endif
