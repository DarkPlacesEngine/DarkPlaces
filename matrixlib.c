
#include "matrixlib.h"
#include <math.h>

void Matrix4x4_Copy (matrix4x4_t *out, matrix4x4_t *in)
{
	*out = *in;
}

void Matrix4x4_CopyRotateOnly (matrix4x4_t *out, matrix4x4_t *in)
{
	out->m[0][0] = in->m[0][0];
	out->m[0][1] = in->m[0][1];
	out->m[0][2] = in->m[0][2];
	out->m[0][3] = 0.0f;
	out->m[1][0] = in->m[1][0];
	out->m[1][1] = in->m[1][1];
	out->m[1][2] = in->m[1][2];
	out->m[1][3] = 0.0f;
	out->m[2][0] = in->m[2][0];
	out->m[2][1] = in->m[2][1];
	out->m[2][2] = in->m[2][2];
	out->m[2][3] = 0.0f;
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_CopyTranslateOnly (matrix4x4_t *out, matrix4x4_t *in)
{
	out->m[0][0] = 0.0f;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = in->m[0][3];
	out->m[1][0] = 0.0f;
	out->m[1][1] = 0.0f;
	out->m[1][2] = 0.0f;
	out->m[1][3] = in->m[0][3];
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = 0.0f;
	out->m[2][3] = in->m[0][3];
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_FromMatrix3x4 (matrix4x4_t *out, matrix3x4_t *in)
{
	out->m[0][0] = in->m[0][0];
	out->m[0][1] = in->m[0][1];
	out->m[0][2] = in->m[0][2];
	out->m[0][3] = in->m[0][3];
	out->m[1][0] = in->m[1][0];
	out->m[1][1] = in->m[1][1];
	out->m[1][2] = in->m[1][2];
	out->m[1][3] = in->m[1][3];
	out->m[2][0] = in->m[2][0];
	out->m[2][1] = in->m[2][1];
	out->m[2][2] = in->m[2][2];
	out->m[2][3] = in->m[2][3];
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_Concat (matrix4x4_t *out, const matrix4x4_t *in1, const matrix4x4_t *in2)
{
	out->m[0][0] = in1->m[0][0] * in2->m[0][0] + in1->m[0][1] * in2->m[1][0] + in1->m[0][2] * in2->m[2][0] + in1->m[0][3] * in2->m[3][0];
	out->m[0][1] = in1->m[0][0] * in2->m[0][1] + in1->m[0][1] * in2->m[1][1] + in1->m[0][2] * in2->m[2][1] + in1->m[0][3] * in2->m[3][1];
	out->m[0][2] = in1->m[0][0] * in2->m[0][2] + in1->m[0][1] * in2->m[1][2] + in1->m[0][2] * in2->m[2][2] + in1->m[0][3] * in2->m[3][2];
	out->m[0][3] = in1->m[0][0] * in2->m[0][3] + in1->m[0][1] * in2->m[1][3] + in1->m[0][2] * in2->m[2][3] + in1->m[0][3] * in2->m[3][3];
	out->m[1][0] = in1->m[1][0] * in2->m[0][0] + in1->m[1][1] * in2->m[1][0] + in1->m[1][2] * in2->m[2][0] + in1->m[1][3] * in2->m[3][0];
	out->m[1][1] = in1->m[1][0] * in2->m[0][1] + in1->m[1][1] * in2->m[1][1] + in1->m[1][2] * in2->m[2][1] + in1->m[1][3] * in2->m[3][1];
	out->m[1][2] = in1->m[1][0] * in2->m[0][2] + in1->m[1][1] * in2->m[1][2] + in1->m[1][2] * in2->m[2][2] + in1->m[1][3] * in2->m[3][2];
	out->m[1][3] = in1->m[1][0] * in2->m[0][3] + in1->m[1][1] * in2->m[1][3] + in1->m[1][2] * in2->m[2][3] + in1->m[1][3] * in2->m[3][3];
	out->m[2][0] = in1->m[2][0] * in2->m[0][0] + in1->m[2][1] * in2->m[1][0] + in1->m[2][2] * in2->m[2][0] + in1->m[2][3] * in2->m[3][0];
	out->m[2][1] = in1->m[2][0] * in2->m[0][1] + in1->m[2][1] * in2->m[1][1] + in1->m[2][2] * in2->m[2][1] + in1->m[2][3] * in2->m[3][1];
	out->m[2][2] = in1->m[2][0] * in2->m[0][2] + in1->m[2][1] * in2->m[1][2] + in1->m[2][2] * in2->m[2][2] + in1->m[2][3] * in2->m[3][2];
	out->m[2][3] = in1->m[2][0] * in2->m[0][3] + in1->m[2][1] * in2->m[1][3] + in1->m[2][2] * in2->m[2][3] + in1->m[2][3] * in2->m[3][3];
	out->m[3][0] = in1->m[3][0] * in2->m[0][0] + in1->m[3][1] * in2->m[1][0] + in1->m[3][2] * in2->m[2][0] + in1->m[3][3] * in2->m[3][0];
	out->m[3][1] = in1->m[3][0] * in2->m[0][1] + in1->m[3][1] * in2->m[1][1] + in1->m[3][2] * in2->m[2][1] + in1->m[3][3] * in2->m[3][1];
	out->m[3][2] = in1->m[3][0] * in2->m[0][2] + in1->m[3][1] * in2->m[1][2] + in1->m[3][2] * in2->m[2][2] + in1->m[3][3] * in2->m[3][2];
	out->m[3][3] = in1->m[3][0] * in2->m[0][3] + in1->m[3][1] * in2->m[1][3] + in1->m[3][2] * in2->m[2][3] + in1->m[3][3] * in2->m[3][3];
}

void Matrix4x4_Transpose (matrix4x4_t *out, const matrix4x4_t *in1)
{
	float scale;
	scale = 3.0 / (in1->m[0][0] * in1->m[0][0]
	             + in1->m[0][1] * in1->m[0][1]
	             + in1->m[0][2] * in1->m[0][2]
	             + in1->m[1][0] * in1->m[1][0]
	             + in1->m[1][1] * in1->m[1][1]
	             + in1->m[1][2] * in1->m[1][2]
	             + in1->m[2][0] * in1->m[2][0]
	             + in1->m[2][1] * in1->m[2][1]
	             + in1->m[2][2] * in1->m[2][2]);
	out->m[0][0] = in1->m[0][0] * scale;
	out->m[0][1] = in1->m[1][0] * scale;
	out->m[0][2] = in1->m[2][0] * scale;
	out->m[0][3] = in1->m[3][0];
	out->m[1][0] = in1->m[0][1] * scale;
	out->m[1][1] = in1->m[1][1] * scale;
	out->m[1][2] = in1->m[2][1] * scale;
	out->m[1][3] = in1->m[3][1];
	out->m[2][0] = in1->m[0][2] * scale;
	out->m[2][1] = in1->m[1][2] * scale;
	out->m[2][2] = in1->m[2][2] * scale;
	out->m[2][3] = in1->m[3][2];
	out->m[3][0] = in1->m[0][3];
	out->m[3][1] = in1->m[1][3];
	out->m[3][2] = in1->m[2][3];
	out->m[3][3] = in1->m[3][3];
}

void Matrix4x4_CreateIdentity (matrix4x4_t *out)
{
	out->m[0][0]=1.0f;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=1.0f;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=1.0f;
	out->m[2][3]=0.0f;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_CreateTranslate (matrix4x4_t *out, float x, float y, float z)
{
	out->m[0][0]=1.0f;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=x;
	out->m[1][0]=0.0f;
	out->m[1][1]=1.0f;
	out->m[1][2]=0.0f;
	out->m[1][3]=y;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=1.0f;
	out->m[2][3]=z;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_CreateRotate (matrix4x4_t *out, float angle, float x, float y, float z)
{
	float len, c, s;

	len = x*x+y*y+z*z;
	if (len != 0.0f)
		len = 1.0f / sqrt(len);
	x *= len;
	y *= len;
	z *= len;

	angle *= M_PI / 180.0;
	c = cos(angle);
	s = sin(angle);

	out->m[0][0]=x * x + c * (1 - x * x);
	out->m[0][1]=x * y * (1 - c) + z * s;
	out->m[0][2]=z * x * (1 - c) - y * s;
	out->m[0][3]=0.0f;
	out->m[1][0]=x * y * (1 - c) - z * s;
	out->m[1][1]=y * y + c * (1 - y * y);
	out->m[1][2]=y * z * (1 - c) + x * s;
	out->m[1][3]=0.0f;
	out->m[2][0]=z * x * (1 - c) + y * s;
	out->m[2][1]=y * z * (1 - c) - x * s;
	out->m[2][2]=z * z + c * (1 - z * z);
	out->m[2][3]=0.0f;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_CreateScale (matrix4x4_t *out, float x)
{
	out->m[0][0]=x;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=x;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=x;
	out->m[2][3]=0.0f;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_CreateScale3 (matrix4x4_t *out, float x, float y, float z)
{
	out->m[0][0]=x;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=y;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=z;
	out->m[2][3]=0.0f;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_ToVectors(const matrix4x4_t *in, float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = in->m[0][0];
	vx[1] = in->m[1][0];
	vx[2] = in->m[2][0];
	vy[0] = in->m[0][1];
	vy[1] = in->m[1][1];
	vy[2] = in->m[2][1];
	vz[0] = in->m[0][2];
	vz[1] = in->m[1][2];
	vz[2] = in->m[2][2];
	t[0] = in->m[0][3];
	t[1] = in->m[1][3];
	t[2] = in->m[2][3];
}

void Matrix4x4_FromVectors(matrix4x4_t *out, const float vx[3], const float vy[3], const float vz[3], const float t[3])
{
	out->m[0][0] = vx[0];
	out->m[0][1] = vy[0];
	out->m[0][2] = vz[0];
	out->m[0][3] = t[0];
	out->m[1][0] = vx[1];
	out->m[1][1] = vy[1];
	out->m[1][2] = vz[1];
	out->m[1][3] = t[1];
	out->m[2][0] = vx[2];
	out->m[2][1] = vy[2];
	out->m[2][2] = vz[2];
	out->m[2][3] = t[2];
	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

void Matrix4x4_Transform (const matrix4x4_t *in, const float v[3], float out[3])
{
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + in->m[2][3];
}

void Matrix4x4_Transform4 (const matrix4x4_t *in, const float v[4], float out[4])
{
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + v[3] * in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + v[3] * in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + v[3] * in->m[2][3];
	out[3] = v[0] * in->m[3][0] + v[1] * in->m[3][1] + v[2] * in->m[3][2] + v[3] * in->m[3][3];
}

void Matrix4x4_SimpleUntransform (const matrix4x4_t *in, const float v[3], float out[3])
{
	float t[3];
	t[0] = v[0] - in->m[0][3];
	t[1] = v[1] - in->m[1][3];
	t[2] = v[2] - in->m[2][3];
	out[0] = t[0] * in->m[0][0] + t[1] * in->m[1][0] + t[2] * in->m[2][0];
	out[1] = t[0] * in->m[0][1] + t[1] * in->m[1][1] + t[2] * in->m[2][1];
	out[2] = t[0] * in->m[0][2] + t[1] * in->m[1][2] + t[2] * in->m[2][2];
}

// FIXME: optimize
void Matrix4x4_ConcatTranslate (matrix4x4_t *out, float x, float y, float z)
{
	matrix4x4_t base, temp;
	Matrix4x4_Copy(out, &base);
	Matrix4x4_CreateTranslate(&temp, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatRotate (matrix4x4_t *out, float angle, float x, float y, float z)
{
	matrix4x4_t base, temp;
	Matrix4x4_Copy(out, &base);
	Matrix4x4_CreateRotate(&temp, angle, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatScale (matrix4x4_t *out, float x)
{
	matrix4x4_t base, temp;
	Matrix4x4_Copy(out, &base);
	Matrix4x4_CreateScale(&temp, x);
	Matrix4x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix4x4_ConcatScale3 (matrix4x4_t *out, float x, float y, float z)
{
	matrix4x4_t base, temp;
	Matrix4x4_Copy(out, &base);
	Matrix4x4_CreateScale3(&temp, x, y, z);
	Matrix4x4_Concat(out, &base, &temp);
}








void Matrix3x4_Copy (matrix3x4_t *out, matrix3x4_t *in)
{
	*out = *in;
}

void Matrix3x4_CopyRotateOnly (matrix3x4_t *out, matrix3x4_t *in)
{
	out->m[0][0] = in->m[0][0];
	out->m[0][1] = in->m[0][1];
	out->m[0][2] = in->m[0][2];
	out->m[0][3] = 0.0f;
	out->m[1][0] = in->m[1][0];
	out->m[1][1] = in->m[1][1];
	out->m[1][2] = in->m[1][2];
	out->m[1][3] = 0.0f;
	out->m[2][0] = in->m[2][0];
	out->m[2][1] = in->m[2][1];
	out->m[2][2] = in->m[2][2];
	out->m[2][3] = 0.0f;
}

void Matrix3x4_CopyTranslateOnly (matrix3x4_t *out, matrix3x4_t *in)
{
	out->m[0][0] = 0.0f;
	out->m[0][1] = 0.0f;
	out->m[0][2] = 0.0f;
	out->m[0][3] = in->m[0][3];
	out->m[1][0] = 0.0f;
	out->m[1][1] = 0.0f;
	out->m[1][2] = 0.0f;
	out->m[1][3] = in->m[0][3];
	out->m[2][0] = 0.0f;
	out->m[2][1] = 0.0f;
	out->m[2][2] = 0.0f;
	out->m[2][3] = in->m[0][3];
}

void Matrix3x4_FromMatrix4x4 (matrix3x4_t *out, matrix4x4_t *in)
{
	out->m[0][0] = in->m[0][0];
	out->m[0][1] = in->m[0][1];
	out->m[0][2] = in->m[0][2];
	out->m[0][3] = in->m[0][3];
	out->m[1][0] = in->m[1][0];
	out->m[1][1] = in->m[1][1];
	out->m[1][2] = in->m[1][2];
	out->m[1][3] = in->m[1][3];
	out->m[2][0] = in->m[2][0];
	out->m[2][1] = in->m[2][1];
	out->m[2][2] = in->m[2][2];
	out->m[2][3] = in->m[2][3];
}

void Matrix3x4_Concat (matrix3x4_t *out, const matrix3x4_t *in1, const matrix3x4_t *in2)
{
	out->m[0][0] = in1->m[0][0] * in2->m[0][0] + in1->m[0][1] * in2->m[1][0] + in1->m[0][2] * in2->m[2][0];
	out->m[0][1] = in1->m[0][0] * in2->m[0][1] + in1->m[0][1] * in2->m[1][1] + in1->m[0][2] * in2->m[2][1];
	out->m[0][2] = in1->m[0][0] * in2->m[0][2] + in1->m[0][1] * in2->m[1][2] + in1->m[0][2] * in2->m[2][2];
	out->m[0][3] = in1->m[0][0] * in2->m[0][3] + in1->m[0][1] * in2->m[1][3] + in1->m[0][2] * in2->m[2][3] + in1->m[0][3];
	out->m[1][0] = in1->m[1][0] * in2->m[0][0] + in1->m[1][1] * in2->m[1][0] + in1->m[1][2] * in2->m[2][0];
	out->m[1][1] = in1->m[1][0] * in2->m[0][1] + in1->m[1][1] * in2->m[1][1] + in1->m[1][2] * in2->m[2][1];
	out->m[1][2] = in1->m[1][0] * in2->m[0][2] + in1->m[1][1] * in2->m[1][2] + in1->m[1][2] * in2->m[2][2];
	out->m[1][3] = in1->m[1][0] * in2->m[0][3] + in1->m[1][1] * in2->m[1][3] + in1->m[1][2] * in2->m[2][3] + in1->m[1][3];
	out->m[2][0] = in1->m[2][0] * in2->m[0][0] + in1->m[2][1] * in2->m[1][0] + in1->m[2][2] * in2->m[2][0];
	out->m[2][1] = in1->m[2][0] * in2->m[0][1] + in1->m[2][1] * in2->m[1][1] + in1->m[2][2] * in2->m[2][1];
	out->m[2][2] = in1->m[2][0] * in2->m[0][2] + in1->m[2][1] * in2->m[1][2] + in1->m[2][2] * in2->m[2][2];
	out->m[2][3] = in1->m[2][0] * in2->m[0][3] + in1->m[2][1] * in2->m[1][3] + in1->m[2][2] * in2->m[2][3] + in1->m[2][3];
}

void Matrix3x4_Transpose3x3 (matrix3x4_t *out, const matrix3x4_t *in1)
{
	float scale;
	scale = 3.0 / (in1->m[0][0] * in1->m[0][0]
	             + in1->m[0][1] * in1->m[0][1]
	             + in1->m[0][2] * in1->m[0][2]
	             + in1->m[1][0] * in1->m[1][0]
	             + in1->m[1][1] * in1->m[1][1]
	             + in1->m[1][2] * in1->m[1][2]
	             + in1->m[2][0] * in1->m[2][0]
	             + in1->m[2][1] * in1->m[2][1]
	             + in1->m[2][2] * in1->m[2][2]);
	out->m[0][0] = in1->m[0][0] * scale;
	out->m[0][1] = in1->m[1][0] * scale;
	out->m[0][2] = in1->m[2][0] * scale;
	out->m[0][3] = 0.0f;
	out->m[1][0] = in1->m[0][1] * scale;
	out->m[1][1] = in1->m[1][1] * scale;
	out->m[1][2] = in1->m[2][1] * scale;
	out->m[1][3] = 0.0f;
	out->m[2][0] = in1->m[0][2] * scale;
	out->m[2][1] = in1->m[1][2] * scale;
	out->m[2][2] = in1->m[2][2] * scale;
	out->m[2][3] = 0.0f;
}

void Matrix3x4_CreateIdentity (matrix3x4_t *out)
{
	out->m[0][0]=1.0f;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=1.0f;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=1.0f;
	out->m[2][3]=0.0f;
}

void Matrix3x4_CreateTranslate (matrix3x4_t *out, float x, float y, float z)
{
	out->m[0][0]=1.0f;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=x;
	out->m[1][0]=0.0f;
	out->m[1][1]=1.0f;
	out->m[1][2]=0.0f;
	out->m[1][3]=y;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=1.0f;
	out->m[2][3]=z;
}

void Matrix3x4_CreateRotate (matrix3x4_t *out, float angle, float x, float y, float z)
{
	float len, c, s;

	len = x*x+y*y+z*z;
	if (len != 0.0f)
		len = 1.0f / sqrt(len);
	x *= len;
	y *= len;
	z *= len;

	angle *= M_PI / 180.0;
	c = cos(angle);
	s = sin(angle);

	out->m[0][0]=x * x + c * (1 - x * x);
	out->m[0][1]=x * y * (1 - c) + z * s;
	out->m[0][2]=z * x * (1 - c) - y * s;
	out->m[0][3]=0.0f;
	out->m[1][0]=x * y * (1 - c) - z * s;
	out->m[1][1]=y * y + c * (1 - y * y);
	out->m[1][2]=y * z * (1 - c) + x * s;
	out->m[1][3]=0.0f;
	out->m[2][0]=z * x * (1 - c) + y * s;
	out->m[2][1]=y * z * (1 - c) - x * s;
	out->m[2][2]=z * z + c * (1 - z * z);
	out->m[2][3]=0.0f;
}

void Matrix3x4_CreateScale (matrix3x4_t *out, float x)
{
	out->m[0][0]=x;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=x;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=x;
	out->m[2][3]=0.0f;
}

void Matrix3x4_CreateScale3 (matrix3x4_t *out, float x, float y, float z)
{
	out->m[0][0]=x;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=y;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=z;
	out->m[2][3]=0.0f;
}

void Matrix3x4_ToVectors(const matrix3x4_t *in, float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = in->m[0][0];
	vx[1] = in->m[1][0];
	vx[2] = in->m[2][0];
	vy[0] = in->m[0][1];
	vy[1] = in->m[1][1];
	vy[2] = in->m[2][1];
	vz[0] = in->m[0][2];
	vz[1] = in->m[1][2];
	vz[2] = in->m[2][2];
	t[0] = in->m[0][3];
	t[1] = in->m[1][3];
	t[2] = in->m[2][3];
}

void Matrix3x4_FromVectors(matrix3x4_t *out, const float vx[3], const float vy[3], const float vz[3], const float t[3])
{
	out->m[0][0] = vx[0];
	out->m[0][1] = vy[0];
	out->m[0][2] = vz[0];
	out->m[0][3] = t[0];
	out->m[1][0] = vx[1];
	out->m[1][1] = vy[1];
	out->m[1][2] = vz[1];
	out->m[1][3] = t[1];
	out->m[2][0] = vx[2];
	out->m[2][1] = vy[2];
	out->m[2][2] = vz[2];
	out->m[2][3] = t[2];
}

void Matrix3x4_Transform (const matrix3x4_t *in, const float v[3], float out[3])
{
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + in->m[2][3];
}

void Matrix3x4_SimpleUntransform (const matrix3x4_t *in, const float v[3], float out[3])
{
	float t[3];
	t[0] = v[0] - in->m[0][3];
	t[1] = v[1] - in->m[1][3];
	t[2] = v[2] - in->m[2][3];
	out[0] = t[0] * in->m[0][0] + t[1] * in->m[1][0] + t[2] * in->m[2][0];
	out[1] = t[0] * in->m[0][1] + t[1] * in->m[1][1] + t[2] * in->m[2][1];
	out[2] = t[0] * in->m[0][2] + t[1] * in->m[1][2] + t[2] * in->m[2][2];
}

// FIXME: optimize
void Matrix3x4_ConcatTranslate (matrix3x4_t *out, float x, float y, float z)
{
	matrix3x4_t base, temp;
	Matrix3x4_Copy(out, &base);
	Matrix3x4_CreateTranslate(&temp, x, y, z);
	Matrix3x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix3x4_ConcatRotate (matrix3x4_t *out, float angle, float x, float y, float z)
{
	matrix3x4_t base, temp;
	Matrix3x4_Copy(out, &base);
	Matrix3x4_CreateRotate(&temp, angle, x, y, z);
	Matrix3x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix3x4_ConcatScale (matrix3x4_t *out, float x)
{
	matrix3x4_t base, temp;
	Matrix3x4_Copy(out, &base);
	Matrix3x4_CreateScale(&temp, x);
	Matrix3x4_Concat(out, &base, &temp);
}

// FIXME: optimize
void Matrix3x4_ConcatScale3 (matrix3x4_t *out, float x, float y, float z)
{
	matrix3x4_t base, temp;
	Matrix3x4_Copy(out, &base);
	Matrix3x4_CreateScale3(&temp, x, y, z);
	Matrix3x4_Concat(out, &base, &temp);
}
