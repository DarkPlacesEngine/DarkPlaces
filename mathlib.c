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
// mathlib.c -- math primitives

#include <math.h>
#include "quakedef.h"

vec3_t vec3_origin = {0,0,0};
float ixtable[4096];

/*-----------------------------------------------------------------*/

float m_bytenormals[NUMVERTEXNORMALS][3] =
{
{-0.525731, 0.000000, 0.850651}, {-0.442863, 0.238856, 0.864188}, 
{-0.295242, 0.000000, 0.955423}, {-0.309017, 0.500000, 0.809017}, 
{-0.162460, 0.262866, 0.951056}, {0.000000, 0.000000, 1.000000},
{0.000000, 0.850651, 0.525731}, {-0.147621, 0.716567, 0.681718}, 
{0.147621, 0.716567, 0.681718}, {0.000000, 0.525731, 0.850651}, 
{0.309017, 0.500000, 0.809017}, {0.525731, 0.000000, 0.850651}, 
{0.295242, 0.000000, 0.955423}, {0.442863, 0.238856, 0.864188}, 
{0.162460, 0.262866, 0.951056}, {-0.681718, 0.147621, 0.716567}, 
{-0.809017, 0.309017, 0.500000}, {-0.587785, 0.425325, 0.688191}, 
{-0.850651, 0.525731, 0.000000}, {-0.864188, 0.442863, 0.238856},
{-0.716567, 0.681718, 0.147621}, {-0.688191, 0.587785, 0.425325}, 
{-0.500000, 0.809017, 0.309017}, {-0.238856, 0.864188, 0.442863}, 
{-0.425325, 0.688191, 0.587785}, {-0.716567, 0.681718, -0.147621}, 
{-0.500000, 0.809017, -0.309017}, {-0.525731, 0.850651, 0.000000}, 
{0.000000, 0.850651, -0.525731}, {-0.238856, 0.864188, -0.442863},
{0.000000, 0.955423, -0.295242}, {-0.262866, 0.951056, -0.162460}, 
{0.000000, 1.000000, 0.000000}, {0.000000, 0.955423, 0.295242},
{-0.262866, 0.951056, 0.162460}, {0.238856, 0.864188, 0.442863}, 
{0.262866, 0.951056, 0.162460}, {0.500000, 0.809017, 0.309017}, 
{0.238856, 0.864188, -0.442863}, {0.262866, 0.951056, -0.162460}, 
{0.500000, 0.809017, -0.309017}, {0.850651, 0.525731, 0.000000}, 
{0.716567, 0.681718, 0.147621}, {0.716567, 0.681718, -0.147621}, 
{0.525731, 0.850651, 0.000000}, {0.425325, 0.688191, 0.587785}, 
{0.864188, 0.442863, 0.238856}, {0.688191, 0.587785, 0.425325}, 
{0.809017, 0.309017, 0.500000}, {0.681718, 0.147621, 0.716567},
{0.587785, 0.425325, 0.688191}, {0.955423, 0.295242, 0.000000}, 
{1.000000, 0.000000, 0.000000}, {0.951056, 0.162460, 0.262866},
{0.850651, -0.525731, 0.000000}, {0.955423, -0.295242, 0.000000}, 
{0.864188, -0.442863, 0.238856}, {0.951056, -0.162460, 0.262866}, 
{0.809017, -0.309017, 0.500000}, {0.681718, -0.147621, 0.716567},
{0.850651, 0.000000, 0.525731}, {0.864188, 0.442863, -0.238856},
{0.809017, 0.309017, -0.500000}, {0.951056, 0.162460, -0.262866}, 
{0.525731, 0.000000, -0.850651}, {0.681718, 0.147621, -0.716567}, 
{0.681718, -0.147621, -0.716567}, {0.850651, 0.000000, -0.525731},
{0.809017, -0.309017, -0.500000}, {0.864188, -0.442863, -0.238856}, 
{0.951056, -0.162460, -0.262866}, {0.147621, 0.716567, -0.681718},
{0.309017, 0.500000, -0.809017}, {0.425325, 0.688191, -0.587785}, 
{0.442863, 0.238856, -0.864188}, {0.587785, 0.425325, -0.688191}, 
{0.688191, 0.587785, -0.425325}, {-0.147621, 0.716567, -0.681718}, 
{-0.309017, 0.500000, -0.809017}, {0.000000, 0.525731, -0.850651},
{-0.525731, 0.000000, -0.850651}, {-0.442863, 0.238856, -0.864188},
{-0.295242, 0.000000, -0.955423}, {-0.162460, 0.262866, -0.951056}, 
{0.000000, 0.000000, -1.000000}, {0.295242, 0.000000, -0.955423}, 
{0.162460, 0.262866, -0.951056}, {-0.442863, -0.238856, -0.864188}, 
{-0.309017, -0.500000, -0.809017}, {-0.162460, -0.262866, -0.951056}, 
{0.000000, -0.850651, -0.525731}, {-0.147621, -0.716567, -0.681718}, 
{0.147621, -0.716567, -0.681718}, {0.000000, -0.525731, -0.850651}, 
{0.309017, -0.500000, -0.809017}, {0.442863, -0.238856, -0.864188}, 
{0.162460, -0.262866, -0.951056}, {0.238856, -0.864188, -0.442863}, 
{0.500000, -0.809017, -0.309017}, {0.425325, -0.688191, -0.587785}, 
{0.716567, -0.681718, -0.147621}, {0.688191, -0.587785, -0.425325}, 
{0.587785, -0.425325, -0.688191}, {0.000000, -0.955423, -0.295242},
{0.000000, -1.000000, 0.000000}, {0.262866, -0.951056, -0.162460}, 
{0.000000, -0.850651, 0.525731}, {0.000000, -0.955423, 0.295242}, 
{0.238856, -0.864188, 0.442863}, {0.262866, -0.951056, 0.162460}, 
{0.500000, -0.809017, 0.309017}, {0.716567, -0.681718, 0.147621}, 
{0.525731, -0.850651, 0.000000}, {-0.238856, -0.864188, -0.442863}, 
{-0.500000, -0.809017, -0.309017}, {-0.262866, -0.951056, -0.162460}, 
{-0.850651, -0.525731, 0.000000}, {-0.716567, -0.681718, -0.147621},
{-0.716567, -0.681718, 0.147621}, {-0.525731, -0.850651, 0.000000}, 
{-0.500000, -0.809017, 0.309017}, {-0.238856, -0.864188, 0.442863},
{-0.262866, -0.951056, 0.162460}, {-0.864188, -0.442863, 0.238856},
{-0.809017, -0.309017, 0.500000}, {-0.688191, -0.587785, 0.425325},
{-0.681718, -0.147621, 0.716567}, {-0.442863, -0.238856, 0.864188},
{-0.587785, -0.425325, 0.688191}, {-0.309017, -0.500000, 0.809017},
{-0.147621, -0.716567, 0.681718}, {-0.425325, -0.688191, 0.587785},
{-0.162460, -0.262866, 0.951056}, {0.442863, -0.238856, 0.864188},
{0.162460, -0.262866, 0.951056}, {0.309017, -0.500000, 0.809017},
{0.147621, -0.716567, 0.681718}, {0.000000, -0.525731, 0.850651},
{0.425325, -0.688191, 0.587785}, {0.587785, -0.425325, 0.688191},
{0.688191, -0.587785, 0.425325}, {-0.955423, 0.295242, 0.000000},
{-0.951056, 0.162460, 0.262866}, {-1.000000, 0.000000, 0.000000},
{-0.850651, 0.000000, 0.525731}, {-0.955423, -0.295242, 0.000000},
{-0.951056, -0.162460, 0.262866}, {-0.864188, 0.442863, -0.238856},
{-0.951056, 0.162460, -0.262866}, {-0.809017, 0.309017, -0.500000},
{-0.864188, -0.442863, -0.238856}, {-0.951056, -0.162460, -0.262866},
{-0.809017, -0.309017, -0.500000}, {-0.681718, 0.147621, -0.716567},
{-0.681718, -0.147621, -0.716567}, {-0.850651, 0.000000, -0.525731},
{-0.688191, 0.587785, -0.425325}, {-0.587785, 0.425325, -0.688191},
{-0.425325, 0.688191, -0.587785}, {-0.425325, -0.688191, -0.587785},
{-0.587785, -0.425325, -0.688191}, {-0.688191, -0.587785, -0.425325},
};

qbyte NormalToByte(const vec3_t n)
{
	int i, best;
	float bestdistance, distance;

	best = 0;
	bestdistance = DotProduct (n, m_bytenormals[0]);
	for (i = 1;i < NUMVERTEXNORMALS;i++)
	{
		distance = DotProduct (n, m_bytenormals[i]);
		if (distance > bestdistance)
		{
			bestdistance = distance;
			best = i;
		}
	}
	return best;
}

// note: uses byte partly to force unsigned for the validity check
void ByteToNormal(qbyte num, vec3_t n)
{
	if (num < NUMVERTEXNORMALS)
		VectorCopy(m_bytenormals[num], n);
	else
		VectorClear(n); // FIXME: complain?
}

float Q_RSqrt(float number)
{
	float y;

	if (number == 0.0f)
		return 0.0f;

	*((int *)&y) = 0x5f3759df - ((* (int *) &number) >> 1);
	return y * (1.5f - (number * 0.5f * y * y));
}


// assumes "src" is normalized
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	// LordHavoc: optimized to death and beyond
	int pos;
	float minelem;

	if (src[0])
	{
		dst[0] = 0;
		if (src[1])
		{
			dst[1] = 0;
			if (src[2])
			{
				dst[2] = 0;
				pos = 0;
				minelem = fabs(src[0]);
				if (fabs(src[1]) < minelem)
				{
					pos = 1;
					minelem = fabs(src[1]);
				}
				if (fabs(src[2]) < minelem)
					pos = 2;

				dst[pos] = 1;
				dst[0] -= src[pos] * src[0];
				dst[1] -= src[pos] * src[1];
				dst[2] -= src[pos] * src[2];

				// normalize the result
				VectorNormalize(dst);
			}
			else
				dst[2] = 1;
		}
		else
		{
			dst[1] = 1;
			dst[2] = 0;
		}
	}
	else
	{
		dst[0] = 1;
		dst[1] = 0;
		dst[2] = 0;
	}
}


// LordHavoc: like AngleVectors, but taking a forward vector instead of angles, useful!
void VectorVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	right[0] -= d * forward[0];
	right[1] -= d * forward[1];
	right[2] -= d * forward[2];
	VectorNormalizeFast(right);
	CrossProduct(right, forward, up);
}

void VectorVectorsDouble(const double *forward, double *right, double *up)
{
	double d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	right[0] -= d * forward[0];
	right[1] -= d * forward[1];
	right[2] -= d * forward[2];
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float t0, t1;
	float angle, c, s;
	vec3_t vr, vu, vf;

	angle = DEG2RAD(degrees);
	c = cos(angle);
	s = sin(angle);
	VectorCopy(dir, vf);
	VectorVectors(vf, vr, vu);

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

/*-----------------------------------------------------------------*/


void BoxOnPlaneSideClassify(mplane_t *p)
{
	p->signbits = 0;
	if (p->normal[0] < 0) // 1
		p->signbits |= 1;
	if (p->normal[1] < 0) // 2
		p->signbits |= 2;
	if (p->normal[2] < 0) // 4
		p->signbits |= 4;
}

void PlaneClassify(mplane_t *p)
{
	if (p->normal[0] == 1)
		p->type = 0;
	else if (p->normal[1] == 1)
		p->type = 1;
	else if (p->normal[2] == 1)
		p->type = 2;
	else
		p->type = 3;
	BoxOnPlaneSideClassify(p);
}

int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, const mplane_t *p)
{
	if (p->type < 3)
		return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch(p->signbits)
	{
	default:
	case 0:
		return (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) < p->dist) << 1));
	case 1:
		return (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) < p->dist) << 1));
	case 2:
		return (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) < p->dist) << 1));
	case 3:
		return (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) < p->dist) << 1));
	case 4:
		return (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));
	case 5:
		return (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));
	case 6:
		return (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));
	case 7:
		return (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));
	}
}

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	double angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right || up)
	{
		angle = angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
		if (right)
		{
			right[0] = -1*(sr*sp*cy+cr*-sy);
			right[1] = -1*(sr*sp*sy+cr*cy);
			right[2] = -1*(sr*cp);
		}
		if (up)
		{
			up[0] = (cr*sp*cy+-sr*-sy);
			up[1] = (cr*sp*sy+-sr*cy);
			up[2] = cr*cp;
		}
	}
}

void AngleVectorsFLU (const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
{
	double angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (left || up)
	{
		angle = angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
		if (left)
		{
			left[0] = sr*sp*cy+cr*-sy;
			left[1] = sr*sp*sy+cr*cy;
			left[2] = sr*cp;
		}
		if (up)
		{
			up[0] = cr*sp*cy+-sr*-sy;
			up[1] = cr*sp*sy+-sr*cy;
			up[2] = cr*cp;
		}
	}
}

void AngleMatrix (const vec3_t angles, const vec3_t translate, vec_t matrix[][4])
{
	double angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);
	matrix[0][0] = cp*cy;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[0][2] = cr*sp*cy+-sr*-sy;
	matrix[0][3] = translate[0];
	matrix[1][0] = cp*sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[1][2] = cr*sp*sy+-sr*cy;
	matrix[1][3] = translate[1];
	matrix[2][0] = -sp;
	matrix[2][1] = sr*cp;
	matrix[2][2] = cr*cp;
	matrix[2][3] = translate[2];
}


// LordHavoc: renamed this to Length, and made the normal one a #define
float VectorNormalizeLength (vec3_t v)
{
	float length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}


/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (const float in1[3*3], const float in2[3*3], float out[3*3])
{
	out[0*3+0] = in1[0*3+0] * in2[0*3+0] + in1[0*3+1] * in2[1*3+0] + in1[0*3+2] * in2[2*3+0];
	out[0*3+1] = in1[0*3+0] * in2[0*3+1] + in1[0*3+1] * in2[1*3+1] + in1[0*3+2] * in2[2*3+1];
	out[0*3+2] = in1[0*3+0] * in2[0*3+2] + in1[0*3+1] * in2[1*3+2] + in1[0*3+2] * in2[2*3+2];
	out[1*3+0] = in1[1*3+0] * in2[0*3+0] + in1[1*3+1] * in2[1*3+0] + in1[1*3+2] * in2[2*3+0];
	out[1*3+1] = in1[1*3+0] * in2[0*3+1] + in1[1*3+1] * in2[1*3+1] + in1[1*3+2] * in2[2*3+1];
	out[1*3+2] = in1[1*3+0] * in2[0*3+2] + in1[1*3+1] * in2[1*3+2] + in1[1*3+2] * in2[2*3+2];
	out[2*3+0] = in1[2*3+0] * in2[0*3+0] + in1[2*3+1] * in2[1*3+0] + in1[2*3+2] * in2[2*3+0];
	out[2*3+1] = in1[2*3+0] * in2[0*3+1] + in1[2*3+1] * in2[1*3+1] + in1[2*3+2] * in2[2*3+1];
	out[2*3+2] = in1[2*3+0] * in2[0*3+2] + in1[2*3+1] * in2[1*3+2] + in1[2*3+2] * in2[2*3+2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (const float in1[3*4], const float in2[3*4], float out[3*4])
{
	out[0*4+0] = in1[0*4+0] * in2[0*4+0] + in1[0*4+1] * in2[1*4+0] + in1[0*4+2] * in2[2*4+0];
	out[0*4+1] = in1[0*4+0] * in2[0*4+1] + in1[0*4+1] * in2[1*4+1] + in1[0*4+2] * in2[2*4+1];
	out[0*4+2] = in1[0*4+0] * in2[0*4+2] + in1[0*4+1] * in2[1*4+2] + in1[0*4+2] * in2[2*4+2];
	out[0*4+3] = in1[0*4+0] * in2[0*4+3] + in1[0*4+1] * in2[1*4+3] + in1[0*4+2] * in2[2*4+3] + in1[0*4+3];
	out[1*4+0] = in1[1*4+0] * in2[0*4+0] + in1[1*4+1] * in2[1*4+0] + in1[1*4+2] * in2[2*4+0];
	out[1*4+1] = in1[1*4+0] * in2[0*4+1] + in1[1*4+1] * in2[1*4+1] + in1[1*4+2] * in2[2*4+1];
	out[1*4+2] = in1[1*4+0] * in2[0*4+2] + in1[1*4+1] * in2[1*4+2] + in1[1*4+2] * in2[2*4+2];
	out[1*4+3] = in1[1*4+0] * in2[0*4+3] + in1[1*4+1] * in2[1*4+3] + in1[1*4+2] * in2[2*4+3] + in1[1*4+3];
	out[2*4+0] = in1[2*4+0] * in2[0*4+0] + in1[2*4+1] * in2[1*4+0] + in1[2*4+2] * in2[2*4+0];
	out[2*4+1] = in1[2*4+0] * in2[0*4+1] + in1[2*4+1] * in2[1*4+1] + in1[2*4+2] * in2[2*4+1];
	out[2*4+2] = in1[2*4+0] * in2[0*4+2] + in1[2*4+1] * in2[1*4+2] + in1[2*4+2] * in2[2*4+2];
	out[2*4+3] = in1[2*4+0] * in2[0*4+3] + in1[2*4+1] * in2[1*4+3] + in1[2*4+2] * in2[2*4+3] + in1[2*4+3];
}


void Mathlib_Init(void)
{
	int a;

	// LordHavoc: setup 1.0f / N table for quick recipricols of integers
	ixtable[0] = 0;
	for (a = 1;a < 4096;a++)
		ixtable[a] = 1.0f / a;
}

#include "matrixlib.h"

void Matrix4x4_Print (const matrix4x4_t *in)
{
	Con_Printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n"
	, in->m[0][0], in->m[0][1], in->m[0][2], in->m[0][3]
	, in->m[1][0], in->m[1][1], in->m[1][2], in->m[1][3]
	, in->m[2][0], in->m[2][1], in->m[2][2], in->m[2][3]
	, in->m[3][0], in->m[3][1], in->m[3][2], in->m[3][3]);
}
