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

#ifndef MATHLIB_H
#define MATHLIB_H

#include "qtypes.h"

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

#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#ifndef min
#define min(A,B) ((A) < (B) ? (A) : (B))
#define max(A,B) ((A) > (B) ? (A) : (B))
#endif

//#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define lhrandom(MIN,MAX) (((double)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))

#define invpow(base,number) (log(number) / log(base))
#define log2i(n) ((((n) & 0xAAAAAAAA) != 0 ? 1 : 0) | (((n) & 0xCCCCCCCC) != 0 ? 2 : 0) | (((n) & 0xF0F0F0F0) != 0 ? 4 : 0) | (((n) & 0xFF00FF00) != 0 ? 8 : 0) | (((n) & 0xFFFF0000) != 0 ? 16 : 0))
#define bit2i(n) log2i((n) << 1)

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
#define VectorMultiply(a,b,c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define CrossProduct(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorNormalize(v) {float ilength = (float) sqrt(DotProduct(v,v));if (ilength) ilength = 1.0f / ilength;v[0] *= ilength;v[1] *= ilength;v[2] *= ilength;}
#define VectorNormalize2(v,dest) {float ilength = (float) sqrt(DotProduct(v,v));if (ilength) ilength = 1.0f / ilength;dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength;}
#define VectorNormalizeDouble(v) {double ilength = sqrt(DotProduct(v,v));if (ilength) ilength = 1.0 / ilength;v[0] *= ilength;v[1] *= ilength;v[2] *= ilength;}
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorDistance(a, b) (sqrt(VectorDistance2(a,b)))
#define VectorLength(a) (sqrt(DotProduct(a, a)))
#define VectorLength2(a) (DotProduct(a, a))
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorCompare(a,b) (((a)[0]==(b)[0])&&((a)[1]==(b)[1])&&((a)[2]==(b)[2]))
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorM(scale1, b1, c) ((c)[0] = (scale1) * (b1)[0],(c)[1] = (scale1) * (b1)[1],(c)[2] = (scale1) * (b1)[2])
#define VectorMAM(scale1, b1, scale2, b2, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2])
#define VectorMAMAM(scale1, b1, scale2, b2, scale3, b3, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2])
#define VectorMAMAMAM(scale1, b1, scale2, b2, scale3, b3, scale4, b4, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0] + (scale4) * (b4)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1] + (scale4) * (b4)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2] + (scale4) * (b4)[2])
#define VectorRandom(v) do{(v)[0] = lhrandom(-1, 1);(v)[1] = lhrandom(-1, 1);(v)[2] = lhrandom(-1, 1);}while(DotProduct(v, v) > 1)
#define VectorLerp(v1,lerp,v2,c) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
#define VectorReflect(a,r,b,c) do{double d;d = DotProduct((a), (b)) * -(1.0 + (r));VectorMA((a), (d), (b), (c));}while(0)
#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])

#define TriangleNormal(a,b,c,n) ( \
	(n)[0] = ((a)[1] - (b)[1]) * ((c)[2] - (b)[2]) - ((a)[2] - (b)[2]) * ((c)[1] - (b)[1]), \
	(n)[1] = ((a)[2] - (b)[2]) * ((c)[0] - (b)[0]) - ((a)[0] - (b)[0]) * ((c)[2] - (b)[2]), \
	(n)[2] = ((a)[0] - (b)[0]) * ((c)[1] - (b)[1]) - ((a)[1] - (b)[1]) * ((c)[0] - (b)[0]) \
	)

// fast PointInfrontOfTriangle
// subtracts v1 from v0 and v2, combined into a crossproduct, combined with a
// dotproduct of the light location relative to the first point of the
// triangle (any point works, since any triangle is obviously flat), and
// finally a comparison to determine if the light is infront of the triangle
// (the goal of this statement) we do not need to normalize the surface
// normal because both sides of the comparison use it, therefore they are
// both multiplied the same amount...  furthermore the subtract can be done
// on the vectors, saving a little bit of math in the dotproducts
#define PointInfrontOfTriangle(p,a,b,c) (((p)[0] - (a)[0]) * (((a)[1] - (b)[1]) * ((c)[2] - (b)[2]) - ((a)[2] - (b)[2]) * ((c)[1] - (b)[1])) + ((p)[1] - (a)[1]) * (((a)[2] - (b)[2]) * ((c)[0] - (b)[0]) - ((a)[0] - (b)[0]) * ((c)[2] - (b)[2])) + ((p)[2] - (a)[2]) * (((a)[0] - (b)[0]) * ((c)[1] - (b)[1]) - ((a)[1] - (b)[1]) * ((c)[0] - (b)[0])) > 0)
#if 0
// readable version, kept only for explanatory reasons
int PointInfrontOfTriangle(const float *p, const float *a, const float *b, const float *c)
{
	float dir0[3], dir1[3], normal[3];

	// calculate two mostly perpendicular edge directions
	VectorSubtract(a, b, dir0);
	VectorSubtract(c, b, dir1);

	// we have two edge directions, we can calculate a third vector from
	// them, which is the direction of the surface normal (it's magnitude
	// is not 1 however)
	CrossProduct(dir0, dir1, normal);

	// compare distance of light along normal, with distance of any point
	// of the triangle along the same normal (the triangle is planar,
	// I.E. flat, so all points give the same answer)
	return DotProduct(p, normal) > DotProduct(a, normal);
}
#endif

/*
// LordHavoc: quaternion math, untested, don't know if these are correct,
// need to add conversion to/from matrices
// LordHavoc: later note: the matrix faq is useful: http://skal.planet-d.net/demo/matrixfaq.htm
// LordHavoc: these are probably very wrong and I'm not sure I care, not used by anything

// returns length of quaternion
#define qlen(a) ((float) sqrt((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2]+(a)[3]*(a)[3]))
// returns squared length of quaternion
#define qlen2(a) ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2]+(a)[3]*(a)[3])
// makes a quaternion from x, y, z, and a rotation angle (in degrees)
#define QuatMake(x,y,z,r,c)\
{\
if (r == 0)\
{\
(c)[0]=(float) ((x) * (1.0f / 0.0f));\
(c)[1]=(float) ((y) * (1.0f / 0.0f));\
(c)[2]=(float) ((z) * (1.0f / 0.0f));\
(c)[3]=(float) 1.0f;\
}\
else\
{\
float r2 = (r) * 0.5 * (M_PI / 180);\
float r2is = 1.0f / sin(r2);\
(c)[0]=(float) ((x)/r2is);\
(c)[1]=(float) ((y)/r2is);\
(c)[2]=(float) ((z)/r2is);\
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
*/

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
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const struct mplane_s *p);
int BoxOnPlaneSide_Separate(const vec3_t emins, const vec3_t emaxs, const vec3_t normal, const vec_t dist);
void BoxPlaneCorners(const vec3_t emins, const vec3_t emaxs, const struct mplane_s *p, vec3_t outnear, vec3_t outfar);
void BoxPlaneCorners_Separate(const vec3_t emins, const vec3_t emaxs, const vec3_t normal, vec3_t outnear, vec3_t outfar);
void BoxPlaneCornerDistances(const vec3_t emins, const vec3_t emaxs, const struct mplane_s *p, vec_t *outnear, vec_t *outfar);
void BoxPlaneCornerDistances_Separate(const vec3_t emins, const vec3_t emaxs, const vec3_t normal, vec_t *outnear, vec_t *outfar);

#define PlaneDist(point,plane)  ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

// LordHavoc: minimal plane structure
typedef struct tinyplane_s
{
	float normal[3], dist;
}
tinyplane_t;

typedef struct tinydoubleplane_s
{
	double normal[3], dist;
}
tinydoubleplane_t;

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

float RadiusFromBounds (const vec3_t mins, const vec3_t maxs);
float RadiusFromBoundsAndOrigin (const vec3_t mins, const vec3_t maxs, const vec3_t origin);

// print a matrix to the console
struct matrix4x4_s;
void Matrix4x4_Print(const struct matrix4x4_s *in);
int Math_atov(const char *s, vec3_t out);

void BoxFromPoints(vec3_t mins, vec3_t maxs, int numpoints, vec_t *point3f);

#endif

