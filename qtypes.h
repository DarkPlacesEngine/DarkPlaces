
#ifndef QTYPES_H
#define QTYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

typedef bool qbool;

#ifndef NULL
#define NULL ((void *)0)
#endif

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#if defined(__GNUC__) || (__clang__) || (__TINYC__) || (_MSC_VER >= 1400)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif

// LadyHavoc: upgrade the prvm to double precision for better time values
// LadyHavoc: to be enabled when bugs are worked out...
//#define PRVM_64
#ifdef PRVM_64
typedef double prvm_vec_t;
typedef int64_t prvm_int_t;
typedef uint64_t prvm_uint_t;
#define PRVM_PRIi PRIi64
#define PRVM_PRIu PRIu64
#else
typedef float prvm_vec_t;
typedef int32_t prvm_int_t;
typedef uint32_t prvm_uint_t;
#define PRVM_PRIi PRIi32
#define PRVM_PRIu PRIu32
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
