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

void Sys_Error (char *error, ...);

vec3_t vec3_origin = {0,0,0};
int nanmask = 255<<23;

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

byte NormalToByte(vec3_t n)
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
void ByteToNormal(byte num, vec3_t n)
{
	if (num < NUMVERTEXNORMALS)
		VectorCopy(m_bytenormals[num], n)
	else
		VectorClear(n) // FIXME: complain?
}

float Q_RSqrt(float number)
{
	float y;

	if (number == 0.0f)
		return 0.0f;

	*((long *)&y) = 0x5f3759df - ((* (long *) &number) >> 1);
	return y * (1.5f - (number * 0.5f * y * y));
}

void _VectorNormalizeFast(vec3_t v)
{
	float y, number;

	number = DotProduct(v, v);

	if (number != 0.0)
	{
		*((long *)&y) = 0x5f3759df - ((* (long *) &number) >> 1);
		y = y * (1.5f - (number * 0.5f * y * y));

		VectorScale(v, y, v);
	}
}

#if 0
// LordHavoc: no longer used at all
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
#if 0
	// LordHavoc: the old way...
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
#else
	// LordHavoc: optimized to death and beyond
	float d;

	// LordHavoc: the normal is a unit vector by definition,
	//            therefore inv_denom was pointless.
	d = DotProduct(normal, p);
	dst[0] = p[0] - d * normal[0];
	dst[1] = p[1] - d * normal[1];
	dst[2] = p[2] - d * normal[2];
#endif
}
#endif

// assumes "src" is normalized
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
#if 0
	// LordHavoc: the old way...
	int	pos;
	int i;
	float minelem, d;
	vec3_t tempvec;

	// find the smallest magnitude axially aligned vector
	minelem = 1.0F;
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	VectorClear(tempvec);
	tempvec[pos] = 1.0F;

	// project the point onto the plane defined by src
	ProjectPointOnPlane( dst, tempvec, src );

	// normalize the result
	VectorNormalize(dst);
#else
	// LordHavoc: optimized to death and beyond
	int	pos;
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
#endif
}


#ifdef _WIN32
#pragma optimize( "", off )
#endif


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
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
#if 0
	// LordHavoc: the old way, cryptic brute force...
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos( DEG2RAD( degrees ) );
	zrot[0][1] = sin( DEG2RAD( degrees ) );
	zrot[1][0] = -sin( DEG2RAD( degrees ) );
	zrot[1][1] = cos( DEG2RAD( degrees ) );

	R_ConcatRotations( m, zrot, tmpmat );
	R_ConcatRotations( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ )
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
#elif 0
	// LordHavoc: on the path to unintelligible code...
//	float	m[3][3];
//	float	im[3][3];
//	float	zrot[3][3];
	float	tmpmat[3][3];
//	float	rot[3][3];
	float	angle, c, s;
//	int	i;
	vec3_t vr, vu, vf;

	angle = DEG2RAD(degrees);

	c = cos(angle);
	s = sin(angle);

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vu);

//	m   [0][0] = vr[0];m   [0][1] = vu[0];m   [0][2] = vf[0];
//	m   [1][0] = vr[1];m   [1][1] = vu[1];m   [1][2] = vf[1];
//	m   [2][0] = vr[2];m   [2][1] = vu[2];m   [2][2] = vf[2];
//	im  [0][0] = vr[0];im  [0][1] = vr[1];im  [0][2] = vr[2];
//	im  [1][0] = vu[0];im  [1][1] = vu[1];im  [1][2] = vu[2];
//	im  [2][0] = vf[0];im  [2][1] = vf[1];im  [2][2] = vf[2];
//	zrot[0][0] =     c;zrot[0][1] =     s;zrot[0][2] =     0;
//	zrot[1][0] =    -s;zrot[1][1] =     c;zrot[1][2] =     0;
//	zrot[2][0] =     0;zrot[2][1] =     0;zrot[2][2] =     1;

//	tmpmat[0][0] = m[0][0] * zrot[0][0] + m[0][1] * zrot[1][0] + m[0][2] * zrot[2][0];
//	tmpmat[0][1] = m[0][0] * zrot[0][1] + m[0][1] * zrot[1][1] + m[0][2] * zrot[2][1];
//	tmpmat[0][2] = m[0][0] * zrot[0][2] + m[0][1] * zrot[1][2] + m[0][2] * zrot[2][2];
//	tmpmat[1][0] = m[1][0] * zrot[0][0] + m[1][1] * zrot[1][0] + m[1][2] * zrot[2][0];
//	tmpmat[1][1] = m[1][0] * zrot[0][1] + m[1][1] * zrot[1][1] + m[1][2] * zrot[2][1];
//	tmpmat[1][2] = m[1][0] * zrot[0][2] + m[1][1] * zrot[1][2] + m[1][2] * zrot[2][2];
//	tmpmat[2][0] = m[2][0] * zrot[0][0] + m[2][1] * zrot[1][0] + m[2][2] * zrot[2][0];
//	tmpmat[2][1] = m[2][0] * zrot[0][1] + m[2][1] * zrot[1][1] + m[2][2] * zrot[2][1];
//	tmpmat[2][2] = m[2][0] * zrot[0][2] + m[2][1] * zrot[1][2] + m[2][2] * zrot[2][2];

	tmpmat[0][0] = vr[0] *  c + vu[0] * -s;
	tmpmat[0][1] = vr[0] *  s + vu[0] *  c;
//	tmpmat[0][2] =                           vf[0];
	tmpmat[1][0] = vr[1] *  c + vu[1] * -s;
	tmpmat[1][1] = vr[1] *  s + vu[1] *  c;
//	tmpmat[1][2] =                           vf[1];
	tmpmat[2][0] = vr[2] *  c + vu[2] * -s;
	tmpmat[2][1] = vr[2] *  s + vu[2] *  c;
//	tmpmat[2][2] =                           vf[2];

//	rot[0][0] = tmpmat[0][0] * vr[0] + tmpmat[0][1] * vu[0] + tmpmat[0][2] * vf[0];
//	rot[0][1] = tmpmat[0][0] * vr[1] + tmpmat[0][1] * vu[1] + tmpmat[0][2] * vf[1];
//	rot[0][2] = tmpmat[0][0] * vr[2] + tmpmat[0][1] * vu[2] + tmpmat[0][2] * vf[2];
//	rot[1][0] = tmpmat[1][0] * vr[0] + tmpmat[1][1] * vu[0] + tmpmat[1][2] * vf[0];
//	rot[1][1] = tmpmat[1][0] * vr[1] + tmpmat[1][1] * vu[1] + tmpmat[1][2] * vf[1];
//	rot[1][2] = tmpmat[1][0] * vr[2] + tmpmat[1][1] * vu[2] + tmpmat[1][2] * vf[2];
//	rot[2][0] = tmpmat[2][0] * vr[0] + tmpmat[2][1] * vu[0] + tmpmat[2][2] * vf[0];
//	rot[2][1] = tmpmat[2][0] * vr[1] + tmpmat[2][1] * vu[1] + tmpmat[2][2] * vf[1];
//	rot[2][2] = tmpmat[2][0] * vr[2] + tmpmat[2][1] * vu[2] + tmpmat[2][2] * vf[2];

//	rot[0][0] = tmpmat[0][0] * vr[0] + tmpmat[0][1] * vu[0] + vf[0] * vf[0];
//	rot[0][1] = tmpmat[0][0] * vr[1] + tmpmat[0][1] * vu[1] + vf[0] * vf[1];
//	rot[0][2] = tmpmat[0][0] * vr[2] + tmpmat[0][1] * vu[2] + vf[0] * vf[2];
//	rot[1][0] = tmpmat[1][0] * vr[0] + tmpmat[1][1] * vu[0] + vf[1] * vf[0];
//	rot[1][1] = tmpmat[1][0] * vr[1] + tmpmat[1][1] * vu[1] + vf[1] * vf[1];
//	rot[1][2] = tmpmat[1][0] * vr[2] + tmpmat[1][1] * vu[2] + vf[1] * vf[2];
//	rot[2][0] = tmpmat[2][0] * vr[0] + tmpmat[2][1] * vu[0] + vf[2] * vf[0];
//	rot[2][1] = tmpmat[2][0] * vr[1] + tmpmat[2][1] * vu[1] + vf[2] * vf[1];
//	rot[2][2] = tmpmat[2][0] * vr[2] + tmpmat[2][1] * vu[2] + vf[2] * vf[2];

//	dst[0] = rot[0][0] * point[0] + rot[0][1] * point[1] + rot[0][2] * point[2];
//	dst[1] = rot[1][0] * point[0] + rot[1][1] * point[1] + rot[1][2] * point[2];
//	dst[2] = rot[2][0] * point[0] + rot[2][1] * point[1] + rot[2][2] * point[2];

	dst[0] = (tmpmat[0][0] * vr[0] + tmpmat[0][1] * vu[0] + vf[0] * vf[0]) * point[0]
	       + (tmpmat[0][0] * vr[1] + tmpmat[0][1] * vu[1] + vf[0] * vf[1]) * point[1]
	       + (tmpmat[0][0] * vr[2] + tmpmat[0][1] * vu[2] + vf[0] * vf[2]) * point[2];
	dst[1] = (tmpmat[1][0] * vr[0] + tmpmat[1][1] * vu[0] + vf[1] * vf[0]) * point[0]
	       + (tmpmat[1][0] * vr[1] + tmpmat[1][1] * vu[1] + vf[1] * vf[1]) * point[1]
	       + (tmpmat[1][0] * vr[2] + tmpmat[1][1] * vu[2] + vf[1] * vf[2]) * point[2];
	dst[2] = (tmpmat[2][0] * vr[0] + tmpmat[2][1] * vu[0] + vf[2] * vf[0]) * point[0]
	       + (tmpmat[2][0] * vr[1] + tmpmat[2][1] * vu[1] + vf[2] * vf[1]) * point[1]
	       + (tmpmat[2][0] * vr[2] + tmpmat[2][1] * vu[2] + vf[2] * vf[2]) * point[2];
#else
	// LordHavoc: optimized to death and beyond, cryptic in an entirely new way
	float	t0, t1;
	float	angle, c, s;
	vec3_t	vr, vu, vf;

	angle = DEG2RAD(degrees);

	c = cos(angle);
	s = sin(angle);

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

//	PerpendicularVector(vr, dir);
//	CrossProduct(vr, vf, vu);
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
#endif
}

#ifdef _WIN32
#pragma optimize( "", on )
#endif

/*-----------------------------------------------------------------*/


// LordHavoc note 1:
// BoxOnPlaneSide did a switch on a 'signbits' value and had optimized
// assembly in an attempt to accelerate it further, very inefficient
// considering that signbits of the frustum planes only changed each
// frame, and the world planes changed only at load time.
// So, to optimize it further I took the obvious route of storing a function
// pointer in the plane struct itself, and shrunk each of the individual
// cases to a single return statement.
// LordHavoc note 2:
// realized axial cases would be a nice speedup for world geometry, although
// never useful for the frustum planes.
int BoxOnPlaneSideX (vec3_t emins, vec3_t emaxs, mplane_t *p) {return p->dist <= emins[0] ? 1 : (p->dist >= emaxs[0] ? 2 : 3);}
int BoxOnPlaneSideY (vec3_t emins, vec3_t emaxs, mplane_t *p) {return p->dist <= emins[1] ? 1 : (p->dist >= emaxs[1] ? 2 : 3);}
int BoxOnPlaneSideZ (vec3_t emins, vec3_t emaxs, mplane_t *p) {return p->dist <= emins[2] ? 1 : (p->dist >= emaxs[2] ? 2 : 3);}
int BoxOnPlaneSide0 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) < p->dist) << 1));}
int BoxOnPlaneSide1 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) < p->dist) << 1));}
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) < p->dist) << 1));}
int BoxOnPlaneSide3 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) < p->dist) << 1));}
int BoxOnPlaneSide4 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));}
int BoxOnPlaneSide5 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));}
int BoxOnPlaneSide6 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));}
int BoxOnPlaneSide7 (vec3_t emins, vec3_t emaxs, mplane_t *p) {return (((p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2]) >= p->dist) | (((p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2]) < p->dist) << 1));}

void BoxOnPlaneSideClassify(mplane_t *p)
{
	switch(p->type)
	{
	case 0: // x axis
		p->BoxOnPlaneSideFunc = BoxOnPlaneSideX;
		break;
	case 1: // y axis
		p->BoxOnPlaneSideFunc = BoxOnPlaneSideY;
		break;
	case 2: // z axis
		p->BoxOnPlaneSideFunc = BoxOnPlaneSideZ;
		break;
	default:
		if (p->normal[2] < 0) // 4
		{
			if (p->normal[1] < 0) // 2
			{
				if (p->normal[0] < 0) // 1
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide7;
				else
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide6;
			}
			else
			{
				if (p->normal[0] < 0) // 1
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide5;
				else
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide4;
			}
		}
		else
		{
			if (p->normal[1] < 0) // 2
			{
				if (p->normal[0] < 0) // 1
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide3;
				else
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide2;
			}
			else
			{
				if (p->normal[0] < 0) // 1
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide1;
				else
					p->BoxOnPlaneSideFunc = BoxOnPlaneSide0;
			}
		}
		break;
	}
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

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	// LordHavoc: this is only to hush up gcc complaining about 'might be used uninitialized' variables
	// (they are NOT used uninitialized, but oh well)
	cr = 0;
	sr = 0;
	if (right || up)
	{
		angle = angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
	}

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
}

int VectorCompare (vec3_t v1, vec3_t v2)
{
	int		i;
	
	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;
			
	return 1;
}

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

// LordHavoc: changed CrossProduct to a #define
/*
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
*/

double sqrt(double x);

vec_t Length(vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

// LordHavoc: renamed these to Length, and made the normal ones #define
float VectorNormalizeLength (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

float VectorNormalizeLength2 (vec3_t v, vec3_t dest) // LordHavoc: added to allow copying while doing the calculation...
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		dest[0] = v[0] * ilength;
		dest[1] = v[1] * ilength;
		dest[2] = v[2] * ilength;
	}
	else
		dest[0] = dest[1] = dest[2] = 0;
		
	return length;

}

void _VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void _VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}


int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}


/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}


/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
====================
*/

void FloorDivMod (double numer, double denom, int *quotient,
		int *rem)
{
	int		q, r;
	double	x;

#ifndef PARANOID
	if (denom <= 0.0)
		Sys_Error ("FloorDivMod: bad denominator %d\n", denom);

//	if ((floor(numer) != numer) || (floor(denom) != denom))
//		Sys_Error ("FloorDivMod: non-integer numer or denom %f %f\n",
//				numer, denom);
#endif

	if (numer >= 0.0)
	{

		x = floor(numer / denom);
		q = (int)x;
		r = (int)floor(numer - (x * denom));
	}
	else
	{
	//
	// perform operations with positive values, and fix mod to make floor-based
	//
		x = floor(-numer / denom);
		q = -(int)x;
		r = (int)floor(-numer - (x * denom));
		if (r != 0)
		{
			q--;
			r = (int)denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}


/*
===================
GreatestCommonDivisor
====================
*/
int GreatestCommonDivisor (int i1, int i2)
{
	if (i1 > i2)
	{
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor (i2, i1 % i2);
	}
	else
	{
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor (i1, i2 % i1);
	}
}
