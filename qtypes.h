
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

// LordHavoc: upgrade the prvm to double precision for better time values
// LordHavoc: to be enabled when bugs are worked out...
//#define PRVM_64
#ifdef PRVM_64
typedef double prvm_vec_t;
typedef long long prvm_int_t;
#else
typedef float prvm_vec_t;
typedef int prvm_int_t;
#endif
typedef prvm_vec_t prvm_vec3_t[3];

#ifdef VEC_64
typedef double vec_t;
#else
typedef float vec_t;
#endif
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef vec_t vec6_t[6];
typedef vec_t vec7_t[7];
typedef vec_t vec8_t[8];

#endif
