
#ifndef MATRIX3X4_H
#define MATRIX3X4_H

// to get the vec3_t type
#include "mathlib.h"

typedef struct matrix3x4_s
{
	float m[3][4];
}
matrix3x4_t;

void Matrix3x4_Copy (matrix3x4_t *out, matrix3x4_t *in);
void Matrix3x4_Concat (matrix3x4_t *out, const matrix3x4_t *in1, const matrix3x4_t *in2);
void Matrix3x4_Transpose3x3 (matrix3x4_t *out, const matrix3x4_t *in1);

void Matrix3x4_CreateIdentity (matrix3x4_t *out);
void Matrix3x4_CreateTranslate (matrix3x4_t *out, float x, float y, float z);
void Matrix3x4_CreateRotate (matrix3x4_t *out, float angle, float x, float y, float z);
void Matrix3x4_CreateScale (matrix3x4_t *out, float x);
void Matrix3x4_CreateScale3 (matrix3x4_t *out, float x, float y, float z);

void Matrix3x4_ToVectors(const matrix3x4_t *in, vec3_t vx, vec3_t vy, vec3_t vz, vec3_t t);
void Matrix3x4_FromVectors(matrix3x4_t *out, const vec3_t vx, const vec3_t vy, const vec3_t vz, const vec3_t t);

void Matrix3x4_Transform (const matrix3x4_t *in, const vec3_t v, vec3_t out);
void Matrix3x4_Untransform (const matrix3x4_t *in, const vec3_t v, vec3_t out);

void Matrix3x4_ConcatTranslate (matrix3x4_t *out, float x, float y, float z);
void Matrix3x4_ConcatRotate (matrix3x4_t *out, float angle, float x, float y, float z);
void Matrix3x4_ConcatScale (matrix3x4_t *out, float x);
void Matrix3x4_ConcatScale3 (matrix3x4_t *out, float x, float y, float z);

#endif
