
#ifndef MATRIX4X4_H
#define MATRIX4X4_H

// to get the vec3_t and vec4_t types
#include "mathlib.h"

typedef struct matrix4x4_s
{
	float m[4][4];
}
matrix4x4_t;

void Matrix4x4_Copy (matrix4x4_t *out, matrix4x4_t *in);
void Matrix4x4_Concat (matrix4x4_t *out, const matrix4x4_t *in1, const matrix4x4_t *in2);
void Matrix4x4_Transpose (matrix4x4_t *out, const matrix4x4_t *in1);

void Matrix4x4_CreateIdentity (matrix4x4_t *out);
void Matrix4x4_CreateTranslate (matrix4x4_t *out, float x, float y, float z);
void Matrix4x4_CreateRotate (matrix4x4_t *out, float angle, float x, float y, float z);
void Matrix4x4_CreateScale (matrix4x4_t *out, float x);
void Matrix4x4_CreateScale3 (matrix4x4_t *out, float x, float y, float z);

void Matrix4x4_ToVectors(const matrix4x4_t *in, vec3_t vx, vec3_t vy, vec3_t vz, vec3_t t);
void Matrix4x4_ToVectors4(const matrix4x4_t *in, vec4_t vx, vec4_t vy, vec4_t vz, vec4_t t);
void Matrix4x4_FromVectors(matrix4x4_t *out, const vec3_t vx, const vec3_t vy, const vec3_t vz, const vec3_t t);
void Matrix4x4_FromVectors4(matrix4x4_t *out, const vec4_t vx, const vec4_t vy, const vec4_t vz, const vec4_t t);

void Matrix4x4_Transform (const matrix4x4_t *in, const vec3_t v, vec3_t out);
void Matrix4x4_Transform4 (const matrix4x4_t *in, const vec4_t v, vec4_t out);
void Matrix4x4_Untransform (const matrix4x4_t *in, const vec3_t v, vec3_t out);
void Matrix4x4_Untransform4 (const matrix4x4_t *in, const vec4_t v, vec4_t out);

void Matrix4x4_ConcatTranslate (matrix4x4_t *out, float x, float y, float z);
void Matrix4x4_ConcatRotate (matrix4x4_t *out, float angle, float x, float y, float z);
void Matrix4x4_ConcatScale (matrix4x4_t *out, float x);
void Matrix4x4_ConcatScale3 (matrix4x4_t *out, float x, float y, float z);

#endif
