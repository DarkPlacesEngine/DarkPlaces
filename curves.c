
/*
this code written by Forest Hale, on 2004-10-17, and placed into public domain
this implements Quadratic BSpline surfaces as seen in Quake3 by id Software

a small rant on misuse of the name 'bezier': many people seem to think that
bezier is a generic term for splines, but it is not, it is a term for a
specific type of bspline (4 control points, cubic bspline), bsplines are the
generalization of the bezier spline to support dimensions other than cubic.

example equations for 1-5 control point bsplines being sampled as t=0...1
1: flat (0th dimension)
o = a
2: linear (1st dimension)
o = a * (1 - t) + b * t
3: quadratic bspline (2nd dimension)
o = a * (1 - t) * (1 - t) + 2 * b * (1 - t) * t + c * t * t
4: cubic (bezier) bspline (3rd dimension)
o = a * (1 - t) * (1 - t) * (1 - t) + 3 * b * (1 - t) * (1 - t) * t + 3 * c * (1 - t) * t * t + d * t * t * t
5: quartic bspline (4th dimension)
o = a * (1 - t) * (1 - t) * (1 - t) * (1 - t) + 4 * b * (1 - t) * (1 - t) * (1 - t) * t + 6 * c * (1 - t) * (1 - t) * t * t + 4 * d * (1 - t) * t * t * t + e * t * t * t * t

arbitrary dimension bspline
double factorial(int n)
{
	int i;
	double f;
	f = 1;
	for (i = 1;i < n;i++)
		f = f * i;
	return f;
}
double bsplinesample(int dimensions, double t, double *param)
{
	double o = 0;
	for (i = 0;i < dimensions + 1;i++)
		o += param[i] * factorial(dimensions)/(factorial(i)*factorial(dimensions-i)) * pow(t, i) * pow(1 - t, dimensions - i);
	return o;
}
*/

#include "quakedef.h"

#include <math.h>
#include "curves.h"

// usage:
// to expand a 5x5 patch to 21x21 vertices (4x4 tesselation), one might use this call:
// Q3PatchSubdivideFloat(3, sizeof(float[3]), outvertices, 5, 5, sizeof(float[3]), patchvertices, 4, 4);
void Q3PatchTesselateFloat(int numcomponents, int outputstride, float *outputvertices, int patchwidth, int patchheight, int inputstride, float *patchvertices, int tesselationwidth, int tesselationheight)
{
	int k, l, x, y, component, outputwidth = (patchwidth-1)*tesselationwidth+1;
	float px, py, *v, a, b, c, *cp[3][3], temp[3][64];
	// iterate over the individual 3x3 quadratic spline surfaces one at a time
	// expanding them to fill the output array (with some overlap to ensure
	// the edges are filled)
	for (k = 0;k < patchheight-1;k += 2)
	{
		for (l = 0;l < patchwidth-1;l += 2)
		{
			// set up control point pointers for quicker lookup later
			for (y = 0;y < 3;y++)
				for (x = 0;x < 3;x++)
					cp[y][x] = (float *)((unsigned char *)patchvertices + ((k+y)*patchwidth+(l+x)) * inputstride);
			// for each row...
			for (y = 0;y <= tesselationheight*2;y++)
			{
				// calculate control points for this row by collapsing the 3
				// rows of control points to one row using py
				py = (float)y / (float)(tesselationheight*2);
				// calculate quadratic spline weights for py
				a = ((1.0f - py) * (1.0f - py));
				b = ((1.0f - py) * (2.0f * py));
				c = ((       py) * (       py));
				for (component = 0;component < numcomponents;component++)
				{
					temp[0][component] = cp[0][0][component] * a + cp[1][0][component] * b + cp[2][0][component] * c;
					temp[1][component] = cp[0][1][component] * a + cp[1][1][component] * b + cp[2][1][component] * c;
					temp[2][component] = cp[0][2][component] * a + cp[1][2][component] * b + cp[2][2][component] * c;
				}
				// fetch a pointer to the beginning of the output vertex row
				v = (float *)((unsigned char *)outputvertices + ((k * tesselationheight + y) * outputwidth + l * tesselationwidth) * outputstride);
				// for each column of the row...
				for (x = 0;x <= (tesselationwidth*2);x++)
				{
					// calculate point based on the row control points
					px = (float)x / (float)(tesselationwidth*2);
					// calculate quadratic spline weights for px
					// (could be precalculated)
					a = ((1.0f - px) * (1.0f - px));
					b = ((1.0f - px) * (2.0f * px));
					c = ((       px) * (       px));
					for (component = 0;component < numcomponents;component++)
						v[component] = temp[0][component] * a + temp[1][component] * b + temp[2][component] * c;
					// advance to next output vertex using outputstride
					// (the next vertex may not be directly following this
					// one, as this may be part of a larger structure)
					v = (float *)((unsigned char *)v + outputstride);
				}
			}
		}
	}
#if 0
	// enable this if you want results printed out
	printf("vertices[%i][%i] =\n{\n", (patchheight-1)*tesselationheight+1, (patchwidth-1)*tesselationwidth+1);
	for (y = 0;y < (patchheight-1)*tesselationheight+1;y++)
	{
		for (x = 0;x < (patchwidth-1)*tesselationwidth+1;x++)
		{
			printf("(");
			for (component = 0;component < numcomponents;component++)
				printf("%f ", outputvertices[(y*((patchwidth-1)*tesselationwidth+1)+x)*numcomponents+component]);
			printf(") ");
		}
		printf("\n");
	}
	printf("}\n");
#endif
}

// returns how much tesselation of each segment is needed to remain under tolerance
int Q3PatchTesselationOnX(int patchwidth, int patchheight, int components, const float *in, float tolerance)
{
	int c, x, y;
	const float *patch;
	float deviation, squareddeviation, bestsquareddeviation;
	bestsquareddeviation = 0;
	for (y = 0;y < patchheight;y++)
	{
		for (x = 0;x < patchwidth-1;x += 2)
		{
			squareddeviation = 0;
			for (c = 0, patch = in + ((y * patchwidth) + x) * components;c < components;c++, patch++)
			{
				deviation = patch[components] * 0.5f - patch[0] * 0.25f - patch[2*components] * 0.25f;
				squareddeviation += deviation*deviation;
			}
			if (bestsquareddeviation < squareddeviation)
				bestsquareddeviation = squareddeviation;
		}
	}
	return (int)floor(log(sqrt(bestsquareddeviation) / tolerance) / log(2)) + 1;
}

// returns how much tesselation of each segment is needed to remain under tolerance
int Q3PatchTesselationOnY(int patchwidth, int patchheight, int components, const float *in, float tolerance)
{
	int c, x, y;
	const float *patch;
	float deviation, squareddeviation, bestsquareddeviation;
	bestsquareddeviation = 0;
	for (y = 0;y < patchheight-1;y += 2)
	{
		for (x = 0;x < patchwidth;x++)
		{
			squareddeviation = 0;
			for (c = 0, patch = in + ((y * patchwidth) + x) * components;c < components;c++, patch++)
			{
				deviation = patch[patchwidth*components] * 0.5f - patch[0] * 0.25f - patch[2*patchwidth*components] * 0.25f;
				squareddeviation += deviation*deviation;
			}
			if (bestsquareddeviation < squareddeviation)
				bestsquareddeviation = squareddeviation;
		}
	}
	return (int)floor(log(sqrt(bestsquareddeviation) / tolerance) / log(2)) + 1;
}

// calculates elements for a grid of vertices
// (such as those produced by Q3PatchTesselate)
// (note: width and height are the actual vertex size, this produces
//  (width-1)*(height-1)*2 triangles, 3 elements each)
void Q3PatchTriangleElements(int *elements, int width, int height, int firstvertex)
{
	int x, y, row0, row1;
	for (y = 0;y < height - 1;y++)
	{
		row0 = firstvertex + (y + 0) * width;
		row1 = firstvertex + (y + 1) * width;
		for (x = 0;x < width - 1;x++)
		{
			*elements++ = row0;
			*elements++ = row1;
			*elements++ = row0 + 1;
			*elements++ = row1;
			*elements++ = row1 + 1;
			*elements++ = row0 + 1;
			row0++;
			row1++;
		}
	}
}

