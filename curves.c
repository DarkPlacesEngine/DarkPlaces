
// this code written by Forest Hale, on 2003-08-23, and placed into public domain
// this code deals with quadratic splines (minimum of 3 points), the same kind used in Quake3 maps.

// LordHavoc's rant on misuse of the name 'bezier': many people seem to think that bezier is a generic term for splines, but it is not, it is a term for a specific type of spline (minimum of 4 control points, cubic spline).

#include "curves.h"

void QuadraticSplineSubdivideFloat(int inpoints, int components, const float *in, int instride, float *out, int outstride)
{
	int s;
	// the input (control points) is read as a stream of points, and buffered
	// by the cpprev, cpcurr, and cpnext variables (to allow subdivision in
	// overlapping memory buffers, even subdivision in-place with pre-spaced
	// control points in the buffer)
	// the output (resulting curve) is written as a stream of points
	// this subdivision is meant to be repeated until the desired flatness
	// level is reached
	if (components == 1 && instride == (int)sizeof(float) && outstride == instride)
	{
		// simple case, single component and no special stride
		float cpprev0 = 0, cpcurr0 = 0, cpnext0;
		cpnext0 = *in++;
		for (s = 0;s < inpoints - 1;s++)
		{
			cpprev0 = cpcurr0;
			cpcurr0 = cpnext0;
			if (s < inpoints - 1)
				cpnext0 = *in++;
			if (s > 0)
			{
				// 50% flattened control point
				// cp1 = average(cp1, average(cp0, cp2));
				*out++ = (cpcurr0 + (cpprev0 + cpnext0) * 0.5f) * 0.5f;
			}
			else
			{
				// copy the control point directly
				*out++ = cpcurr0;
			}
			// midpoint
			// mid = average(cp0, cp1);
			*out++ = (cpcurr0 + cpnext0) * 0.5f;
		}
		// copy the final control point
		*out++ = cpnext0;
	}
	else
	{
		// multiple components or stride is used (complex case)
		int c;
		float cpprev[4], cpcurr[4], cpnext[4];
		// check if there are too many components for the buffers
		if (components > 1)
		{
			// more components can be handled, but slowly, by calling self multiple times...
			for (c = 0;c < components;c++, in++, out++)
				QuadraticSplineSubdivideFloat(inpoints, 1, in, instride, out, outstride);
			return;
		}
		for (c = 0;c < components;c++)
			cpnext[c] = in[c];
		(unsigned char *)in += instride;
		for (s = 0;s < inpoints - 1;s++)
		{
			for (c = 0;c < components;c++)
				cpprev[c] = cpcurr[c];
			for (c = 0;c < components;c++)
				cpcurr[c] = cpnext[c];
			for (c = 0;c < components;c++)
				cpnext[c] = in[c];
			(unsigned char *)in += instride;
			// the end points are copied as-is
			if (s > 0)
			{
				// 50% flattened control point
				// cp1 = average(cp1, average(cp0, cp2));
				for (c = 0;c < components;c++)
					out[c] = (cpcurr[c] + (cpprev[c] + cpnext[c]) * 0.5f) * 0.5f;
			}
			else
			{
				// copy the control point directly
				for (c = 0;c < components;c++)
					out[c] = cpcurr[c];
			}
			(unsigned char *)out += outstride;
			// midpoint
			// mid = average(cp0, cp1);
			for (c = 0;c < components;c++)
				out[c] = (cpcurr[c] + cpnext[c]) * 0.5f;
			(unsigned char *)out += outstride;
		}
		// copy the final control point
		for (c = 0;c < components;c++)
			out[c] = cpnext[c];
		//(unsigned char *)out += outstride;
	}
}

// note: out must have enough room!
// (see finalwidth/finalheight calcs below)
void QuadraticSplinePatchSubdivideFloatBuffer(int cpwidth, int cpheight, int xlevel, int ylevel, int components, const float *in, float *out)
{
	int finalwidth, finalheight, xstep, ystep, x, y, c;
	float *o;

	// error out on various bogus conditions
	if (xlevel < 0 || ylevel < 0 || xlevel > 16 || ylevel > 16 || cpwidth < 3 || cpheight < 3)
		return;

	xstep = 1 << xlevel;
	ystep = 1 << ylevel;
	finalwidth = (cpwidth - 1) * xstep + 1;
	finalheight = (cpheight - 1) * ystep + 1;

	for (y = 0;y < finalheight;y++)
		for (x = 0;x < finalwidth;x++)
			for (c = 0, o = out + (y * finalwidth + x) * components;c < components;c++)
				o[c] = 0;

	if (xlevel == 1 && ylevel == 0)
	{
		for (y = 0;y < finalheight;y++)
			QuadraticSplineSubdivideFloat(cpwidth, components, in + y * cpwidth * components, sizeof(float) * components, out + y * finalwidth * components, sizeof(float) * components);
		return;
	}
	if (xlevel == 0 && ylevel == 1)
	{
		for (x = 0;x < finalwidth;x++)
			QuadraticSplineSubdivideFloat(cpheight, components, in + x * components, sizeof(float) * cpwidth * components, out + x * components, sizeof(float) * finalwidth * components);
		return;
	}

	// copy control points into correct positions in destination buffer
	for (y = 0;y < finalheight;y += ystep)
		for (x = 0;x < finalwidth;x += xstep)
			for (c = 0, o = out + (y * finalwidth + x) * components;c < components;c++)
				o[c] = *in++;

	// subdivide in place in the destination buffer
	while (xstep > 1 || ystep > 1)
	{
		if (xstep > 1)
		{
			xstep >>= 1;
			for (y = 0;y < finalheight;y += ystep)
				QuadraticSplineSubdivideFloat(cpwidth, components, out + y * finalwidth * components, sizeof(float) * xstep * 2 * components, out + y * finalwidth * components, sizeof(float) * xstep * components);
			cpwidth = (cpwidth - 1) * 2 + 1;
		}
		if (ystep > 1)
		{
			ystep >>= 1;
			for (x = 0;x < finalwidth;x += xstep)
				QuadraticSplineSubdivideFloat(cpheight, components, out + x * components, sizeof(float) * ystep * 2 * finalwidth * components, out + x * components, sizeof(float) * ystep * finalwidth * components);
			cpheight = (cpheight - 1) * 2 + 1;
		}
	}
}


