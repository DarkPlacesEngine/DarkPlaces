/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// mathlib.h

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef vec_t vec6_t[6];
typedef vec_t vec7_t[7];
typedef vec_t vec8_t[8];
struct mplane_s;
extern vec3_t vec3_origin;

#define nanmask (255<<23)
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#define bound(min,num,max) (num >= min ? (num < max ? num : max) : min)

#ifndef min
#define min(A,B) (A < B ? A : B)
#define max(A,B) (A > B ? A : B)
#endif

#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))

#define DEG2RAD(a) ((a) * ((float) M_PI / 180.0f))
#define RAD2DEG(a) ((a) * (180.0f / (float) M_PI))
#define ANGLEMOD(a) (((int) ((a) * (65536.0f / 360.0f)) & 65535) * (360.0f / 65536.0f))

#define VectorNegate(a,b) ((b)[0]=-((a)[0]),(b)[1]=-((a)[1]),(b)[2]=-((a)[2]))
#define VectorSet(a,b,c,d) ((a)[0]=(b),(a)[1]=(c),(a)[2]=(d))
#define VectorClear(a) ((a)[0]=(a)[1]=(a)[2]=0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define CrossProduct(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorNormalize(v) {float ilength = 1.0f / (float) sqrt(DotProduct(v,v));v[0] *= ilength;v[1] *= ilength;v[2] *= ilength;}
#define VectorNormalize2(v,dest) {float ilength = 1.0f / (float) sqrt(DotProduct(v,v));dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength;}
#define VectorNormalizeDouble(v) {double ilength = 1.0 / (float) sqrt(DotProduct(v,v));v[0] *= ilength;v[1] *= ilength;v[2] *= ilength;}
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorDistance(a, b) (sqrt(VectorDistance2(a,b)))
#define VectorLength(a) sqrt(DotProduct(a, a))
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorCompare(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2]))
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorNormalizeFast(_v)\
{\
	float _y, _number;\
	_number = DotProduct(_v, _v);\
	if (_number != 0.0)\
	{\
		*((long *)&_y) = 0x5f3759df - ((* (long *) &_number) >> 1);\
		_y = _y * (1.5f - (_number * 0.5f * _y * _y));\
		VectorScale(_v, _y, _v);\
	}\
}
#define VectorRandom(v) {do{(v)[0] = lhrandom(-1, 1);(v)[1] = lhrandom(-1, 1);(v)[2] = lhrandom(-1, 1);}while(DotProduct(v, v) > 1);}

// LordHavoc: quaternion math, untested, don't know if these are correct,
// need to add conversion to/from matrices

// returns length of quaternion
#define qlen(a) ((float) sqrt((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2]+(a)[3]*(a)[3]))
// returns squared length of quaternion
#define qlen2(a) ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2]+(a)[3]*(a)[3])
// makes a quaternion from x, y, z, and a rotation angle (in degrees)
// FIXME: this is almost definitely broken, need a rewrite
#define QuatMake(x,y,z,r,c)\
{\
r2 = (r) * M_PI / 360;\
if (r == 0)\
{\
(c)[0]=(float) ((x)*sin(r2));\
(c)[1]=(float) ((y)*sin(r2));\
(c)[2]=(float) ((z)*sin(r2));\
(c)[3]=(float) 1;\
}\
else\
{\
float r2 = (r) * 0.5 * (M_PI / 180);\
(c)[0]=(float) ((x)*sin(r2));\
(c)[1]=(float) ((y)*sin(r2));\
(c)[2]=(float) ((z)*sin(r2));\
(c)[3]=(float) (cos(r2));\
}\
}
// makes a quaternion from a vector and a rotation angle (in degrees)
#define QuatFromVec(a,r,c) QuatMake((a)[0],(a)[1],(a)[2],(r))
// copies a quaternion
#define QuatCopy(a,c) {(c)[0]=(a)[0];(c)[1]=(a)[1];(c)[2]=(a)[2];(c)[3]=(a)[3];}
#define QuatSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];(c)[3]=(a)[3]-(b)[3];}
#define QuatAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];(c)[3]=(a)[3]+(b)[3];}
#define QuatScale(a,b,c) {(c)[0]=(a)[0]*b;(c)[1]=(a)[1]*b;(c)[2]=(a)[2]*b;(c)[3]=(a)[3]*b;}
// FIXME: this is wrong, do some more research on quaternions
//#define QuatMultiply(a,b,c) {(c)[0]=(a)[0]*(b)[0];(c)[1]=(a)[1]*(b)[1];(c)[2]=(a)[2]*(b)[2];(c)[3]=(a)[3]*(b)[3];}
// FIXME: this is wrong, do some more research on quaternions
//#define QuatMultiplyAdd(a,b,d,c) {(c)[0]=(a)[0]*(b)[0]+d[0];(c)[1]=(a)[1]*(b)[1]+d[1];(c)[2]=(a)[2]*(b)[2]+d[2];(c)[3]=(a)[3]*(b)[3]+d[3];}
#define qdist(a,b) ((float) sqrt(((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2])+((b)[3]-(a)[3])*((b)[3]-(a)[3])))
#define qdist2(a,b) (((b)[0]-(a)[0])*((b)[0]-(a)[0])+((b)[1]-(a)[1])*((b)[1]-(a)[1])+((b)[2]-(a)[2])*((b)[2]-(a)[2])+((b)[3]-(a)[3])*((b)[3]-(a)[3]))

#define VectorCopy4(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];(b)[3]=(a)[3];}

vec_t Length (vec3_t v);
float VectorNormalizeLength (vec3_t v);		// returns vector length
float VectorNormalizeLength2 (vec3_t v, vec3_t dest);		// returns vector length

#define NUMVERTEXNORMALS	162
extern float m_bytenormals[NUMVERTEXNORMALS][3];

qbyte NormalToByte(const vec3_t n);
void ByteToNormal(qbyte num, vec3_t n);

void R_ConcatRotations (const float in1[3*3], const float in2[3*3], float out[3*3]);
void R_ConcatTransforms (const float in1[3*4], const float in2[3*4], float out[3*4]);

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
// LordHavoc: proper matrix version of AngleVectors
void AngleVectorsFLU (const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up);
// LordHavoc: builds a [3][4] matrix
void AngleMatrix (const vec3_t angles, const vec3_t translate, vec_t matrix[][4]);

// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);
void VectorVectorsDouble(const double *forward, double *right, double *up);

void PlaneClassify(struct mplane_s *p);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		(p)->BoxOnPlaneSideFunc( (emins), (emaxs), (p)))

#define PlaneDist(point,plane)  ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
//#define PlaneDist(point,plane)  (DotProduct((point), (plane)->normal))
//#define PlaneDiff(point,plane) (DotProduct((point), (plane)->normal) - (plane)->dist)

// LordHavoc: minimal plane structure
typedef struct
{
	float normal[3], dist;
}
tinyplane_t;

typedef struct
{
	double normal[3], dist;
}
tinydoubleplane_t;

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
