/*
Copyright (c) 2021 Ashley Rose Hale (LadyHavoc)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "convex.h"

typedef struct convex_builder_state_s
{
	// planes chosen to describe the volume
	int numplanes;
	float planes[64][4];

	// corners of the solid described by the planes chosen so far
	int numcorners;
	float corners[128][3];

	// provided point cloud which we are trying to find an optimal fit for
	int numpoints;
	const float* points3f;

	// we consider points to be equivalent if they are within this distance
	float epsilon;
}
convex_builder_state_t;

float convex_normal_distance(const float *normal3f, int numpoints, const float* points3f)
{
	int i;
	float d;
	float best = 0;
	best = points3f[0] * normal3f[0] + points3f[1] * normal3f[1] + points3f[2] * normal3f[2];
	for (i = 1; i < numpoints; i++)
	{
		d = points3f[i * 3 + 0] * normal3f[0] + points3f[i * 3 + 1] * normal3f[1] + points3f[i * 3 + 2] * normal3f[2];
		if (best < d)
			best = d;
	}
	return best;
}

void convex_builder_initialize_for_point_cloud(convex_builder_state_t* b, int numpoints, const float* points3f)
{
	int i, j, k, l;
	float aabb[2][3], e;

	// we'll be continuing to read the points provided by the caller
	b->numpoints = numpoints;
	b->points3f = points3f;

	// figure out the bounding box first as a starting point, this can be a
	// reasonable fit on its own, but more importantly it ensures we never
	// produce an unbounded solid
	aabb[0][0] = aabb[1][0] = points3f[0];
	aabb[0][1] = aabb[1][1] = points3f[1];
	aabb[0][2] = aabb[1][2] = points3f[2];
	b->epsilon = 0.0f;
	for (i = 0; i < numpoints; i++)
	{
		for (j = 0; j < 3; j++)
		{
			e = fabs(points3f[i * 3 + j]) * (1.0f / 1048576.0f);
			if (b->epsilon < e)
				b->epsilon = e;
			if (aabb[0][j] > points3f[i * 3 + j])
				aabb[0][j] = points3f[i * 3 + j];
			if (aabb[0][j] < points3f[i * 3 + j])
				aabb[0][j] = points3f[i * 3 + j];
		}
	}
	b->numplanes = 6;
	for (i = 0; i < 6; i++)
		for (j = 0;j < 4;j++)
			b->planes[i][j] = 0;
	for (i = 0;i < 3;i++)
	{
		b->planes[i * 2 + 0][i] = 1;
		b->planes[i * 2 + 0][3] = aabb[1][i];
		b->planes[i * 2 + 1][i] = -1;
		b->planes[i * 2 + 1][3] = -aabb[0][i];
	}

	// create the corners of the box
	b->numcorners = 8;
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				b->corners[i * 4 + j * 2 + k][0] = aabb[i][0];
				b->corners[i * 4 + j * 2 + k][1] = aabb[j][1];
				b->corners[i * 4 + j * 2 + k][2] = aabb[k][2];
			}
		}
	}
}


void convex_builder_pick_best_planes(convex_builder_state_t* b, int maxplanes)
{
	int i, j, k, l;
	int numplanes = 0;
	float planes[64][4];
	float aabb[2][3], ca[3], cb[3], cn[3], plane[2][4], p[3][3], d[2];
	float volume = 0, clen2, inv;

	// iterate all possible planes we could construct from the
	// provided points
	for (i = 0; i < b->numpoints - 2; i++)
	{
		for (j = i + 1; j < b->numpoints - 1; j++)
		{
			for (k = j + 1; k < b->numpoints; k++)
			{
				// for each unique triplet of points [i,j,k] we visit only the
				// canonical ordering i<j<k, so we have to produce two opposite
				// planes; it would be worse to visit all orderings of [i,j,k]
				// because that would produce 6 planes using 6 cross products,
				// this way we produce two planes using one cross product.

				// calculate the edge directions
				for (l = 0; l < 3; l++)
				{
					p[0][l] = b->points3f[i * 3 + l];
					p[1][l] = b->points3f[j * 3 + l];
					p[2][l] = b->points3f[k * 3 + l];
					ca[l] = p[1][l] - p[0][l];
					cb[l] = p[2][l] - p[0][l];
				}
				// cross product to produce a normal; this is not unit length,
				// its length is the volume of the triangle *2
				cn[0] = ca[1] * cb[2] - ca[2] * cb[1];
				cn[1] = ca[2] * cb[0] - ca[0] * cb[2];
				cn[2] = ca[0] * cb[1] - ca[1] * cb[0];
				clen2 = cn[0] * cn[0] + cn[1] * cn[1] + cn[2] * cn[2];
				if (clen2 == 0.0f)
				{
					// we can't do anything with a degenerate plane
					continue;
				}
				// normalize the plane normal
				inv = 1.0f / sqrt(clen2);
				for (l = 0; l < 3; l++)
				{
					plane[0][l] = cn[l] * inv;
					plane[1][l] = plane[0][l] * -1.0f;
				}
				// calculate the plane distance of the point triplet
				plane[0][3] = convex_normal_distance(plane[0], 3, p);
				plane[1][3] = plane[0][3] * -1.0f;
				for (l = 0; l < 2; l++)
				{
					// reject the plane if it puts any points outside of the solid
					d[l] = convex_normal_distance(plane[l], b->numpoints, b->points3f);
					if (d[l] - plane[l][3] > b->epsilon)
						continue;
					// measure how much this plane carves the volume
					TODO;
				}
			}
		}
	}
}

void convex_planes_for_point_cloud(int* outnumplanes, float* outplanes4f, int maxplanes, int numpoints, float* points3f)
{
	// The algorithm here is starting with a suboptimal fit such as an axis-aligned bounding box, and then attempting to carve the largest portions of it away by picking better planes (i.e. largest volume removed) from triangles composed of the arbitrary points, so this means we need a way to measure volume of the carved space.
	convex_builder_state_t b;

	// return early if there are no points, rather than crash
	*outnumplanes = 0;
	if (numpoints < 1)
		return;

	// first we create a box from the points
	convex_builder_initialize_for_point_cloud(&b, numpoints, points3f);

	// optimize the convex solid as best we can
	convex_builder_pick_best_planes(&b, maxplanes);

}
