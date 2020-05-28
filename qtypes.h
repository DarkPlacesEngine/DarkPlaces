
#ifndef QTYPES_H
#define QTYPES_H

#include <stdint.h>

#ifndef __cplusplus
#ifdef _MSC_VER
typedef enum {false, true} bool;
#else
#include <stdbool.h>
#endif
#endif
typedef bool qboolean;


#ifndef NULL
#define NULL ((void *)0)
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

typedef long long dpint64;
typedef unsigned long long dpuint64;

// LadyHavoc: upgrade the prvm to double precision for better time values
// LadyHavoc: to be enabled when bugs are worked out...
#define PRVM_64
#ifdef PRVM_64
typedef double prvm_vec_t;
typedef long long prvm_int_t;
typedef unsigned long long prvm_uint_t;
#else
typedef float prvm_vec_t;
typedef int prvm_int_t;
typedef unsigned int prvm_uint_t;
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
