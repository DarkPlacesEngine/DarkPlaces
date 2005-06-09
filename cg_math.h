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

#ifndef CG_MATH_H
#define CG_MATH_H


#ifndef true
#define true 1
#define false 0
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#ifndef NULL
#define NULL ((void *)0)
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
#define VectorRandom(v) {do{(v)[0] = CGVM_RandomRange(-1, 1);(v)[1] = CGVM_RandomRange(-1, 1);(v)[2] = CGVM_RandomRange(-1, 1);}while(DotProduct(v, v) > 1);}

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
// LordHavoc: proper matrix version of AngleVectors
void AngleVectorsFLU (vec3_t angles, vec3_t forward, vec3_t left, vec3_t up);
// LordHavoc: builds a [3][4] matrix
void AngleMatrix (vec3_t angles, vec3_t translate, vec_t matrix[][4]);

// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up);

#endif

