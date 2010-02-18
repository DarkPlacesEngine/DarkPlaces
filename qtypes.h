
#ifndef QTYPES_H
#define QTYPES_H

#undef true
#undef false

#ifndef __cplusplus
typedef enum qboolean_e {false, true} qboolean;
#else
typedef bool qboolean;
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

#if defined(__GNUC__) || (defined(_MSC_VER) && _MSC_VER >= 1400)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif

#endif
