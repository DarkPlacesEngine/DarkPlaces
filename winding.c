
#include "quakedef.h"
#include "winding.h"

// this code came from qbsp source

#define MAX_POINTS_ON_WINDING 64

winding_t *Winding_New(int points)
{
	winding_t *w;
	w = Mem_Alloc(loadmodel->mempool, sizeof(winding_t) + sizeof(double[3]) * (points - 8));
	w->maxpoints = points;
	return w;
}

void Winding_Free(winding_t *w)
{
	Mem_Free(w);
}

winding_t *Winding_NewFromPlane(double normalx, double normaly, double normalz, double dist)
{
	winding_t *w;
	w = Winding_New(4);
	BufWinding_NewFromPlane(w, normalx, normaly, normalz, dist);
	return w;
}

//Clips the winding to the plane, returning the new winding on the positive side
//Frees the input winding.
//If keepon is true, an exactly on-plane winding will be saved, otherwise
//it will be clipped away.
winding_t *Winding_Clip(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, int keepon)
{
	winding_t *neww;
	double dot, *p1, *p2, mid[3], splitnormal[3], dists[MAX_POINTS_ON_WINDING + 1];
	int i, j, maxpts, counts[3], sides[MAX_POINTS_ON_WINDING + 1];

	splitnormal[0] = splitnormalx;
	splitnormal[1] = splitnormaly;
	splitnormal[2] = splitnormalz;
	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < in->numpoints;i++)
	{
		dists[i] = dot = DotProduct(in->points[i], splitnormal) - splitdist;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (keepon && !counts[0] && !counts[1])
		return in;

	if (!counts[0])
	{
		Winding_Free(in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = 0;
	for (i = 0;i < in->numpoints;i++)
	{
		if (sides[i] == SIDE_ON)
		{
			maxpts++;
			continue;
		}
		if (sides[i] == SIDE_FRONT)
			maxpts++;
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
		maxpts++;
	}

	if (maxpts > MAX_POINTS_ON_WINDING)
		Sys_Error("Winding_Clip: maxpts > MAX_POINTS_ON_WINDING");

	neww = Winding_New(maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy(p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy(p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (splitnormal[j] == 1)
				mid[j] = splitdist;
			else if (splitnormal[j] == -1)
				mid[j] = -splitdist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		VectorCopy(mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	// free the original winding
	Winding_Free(in);

	return neww;
}


//Divides a winding by a plane, producing one or two windings.  The
//original winding is not damaged or freed.  If only on one side, the
//returned winding will be the input winding.  If on both sides, two
//new windings will be created.
void Winding_Divide(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, winding_t **front, winding_t **back)
{
	winding_t *f, *b;
	double dot, *p1, *p2, mid[3], splitnormal[3], dists[MAX_POINTS_ON_WINDING + 1];
	int i, j, frontpts, backpts, counts[3], sides[MAX_POINTS_ON_WINDING + 1];

	splitnormal[0] = splitnormalx;
	splitnormal[1] = splitnormaly;
	splitnormal[2] = splitnormalz;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < in->numpoints;i++)
	{
		dot = DotProduct(in->points[i], splitnormal);
		dot -= splitdist;
		dists[i] = dot;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if (!counts[0])
	{
		*back = in;
		return;
	}
	if (!counts[1])
	{
		*front = in;
		return;
	}

	frontpts = 0;
	backpts = 0;

	for (i = 0;i < in->numpoints;i++)
	{
		if (sides[i] == SIDE_ON)
		{
			frontpts++;
			backpts++;
			continue;
		}
		if (sides[i] == SIDE_FRONT)
			frontpts++;
		else if (sides[i] == SIDE_BACK)
			backpts++;
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
		frontpts++;
		backpts++;
	}

	if (frontpts > MAX_POINTS_ON_WINDING)
		Sys_Error("Winding_Clip: frontpts > MAX_POINTS_ON_WINDING");
	if (backpts > MAX_POINTS_ON_WINDING)
		Sys_Error("Winding_Clip: backpts > MAX_POINTS_ON_WINDING");

	*front = f = Winding_New(frontpts);
	*back = b = Winding_New(backpts);

	for (i = 0;i < in->numpoints;i++)
	{
		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy(p1, f->points[f->numpoints]);
			f->numpoints++;
			VectorCopy(p1, b->points[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy(p1, f->points[f->numpoints]);
			f->numpoints++;
		}
		else if (sides[i] == SIDE_BACK)
		{
			VectorCopy(p1, b->points[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (splitnormal[j] == 1)
				mid[j] = splitdist;
			else if (splitnormal[j] == -1)
				mid[j] = -splitdist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		VectorCopy(mid, f->points[f->numpoints]);
		f->numpoints++;
		VectorCopy(mid, b->points[b->numpoints]);
		b->numpoints++;
	}
}

// LordHavoc: these functions are more efficient by not allocating/freeing memory all the time

void BufWinding_NewFromPlane(winding_t *w, double normalx, double normaly, double normalz, double dist)
{
	int x;
	double max, v, org[3], vright[3], vup[3], normal[3];

	w->numpoints = 0;
	if (w->maxpoints < 4)
		return;

	w->numpoints = 4;

	normal[0] = normalx;
	normal[1] = normaly;
	normal[2] = normalz;
#if 0
	VectorVectorsDouble(normal, vright, vup);
#else
	// find the major axis
	x = 0;
	max = fabs(normal[0]);
	v = fabs(normal[1]);
	if(v > max)
	{
		x = 1;
		max = v;
	}
	v = fabs(normal[2]);
	if(v > max)
	{
		x = 2;
		max = v;
	}

	VectorClear(vup);
	switch(x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = DotProduct(vup, normal);
	VectorMA(vup, -v, normal, vup);
	VectorNormalize(vup);
#endif

	VectorScale(normal, dist, org);

	CrossProduct(vup, normal, vright);

	VectorScale(vup, 1024.0*1024.0*1024.0, vup);
	VectorScale(vright, 1024.0*1024.0*1024.0, vright);

	// project a really big axis aligned box onto the plane
	VectorSubtract(org, vright, w->points[0]);
	VectorAdd(w->points[0], vup, w->points[0]);

	VectorAdd(org, vright, w->points[1]);
	VectorAdd(w->points[1], vup, w->points[1]);

	VectorAdd(org, vright, w->points[2]);
	VectorSubtract(w->points[2], vup, w->points[2]);

	VectorSubtract(org, vright, w->points[3]);
	VectorSubtract(w->points[3], vup, w->points[3]);

#if 0
	{
		double n[3];
		TriangleNormal(w->points[0], w->points[1], w->points[2], n);
		VectorNormalize(n);
		if (fabs(DotProduct(n, normal) - 1) > 0.01f)
			Con_Printf("%.0f %.0f %.0f (%.0f %.0f %.0f, %.0f %.0f %.0f) != %.0f %.0f %.0f (%.0f %.0f %.0f, %.0f %.0f %.0f, %.0f %.0f %.0f, %.0f %.0f %.0f)\n", normal[0], normal[1], normal[2], vright[0], vright[1], vright[2], vup[0], vup[1], vup[2], n[0], n[1], n[2], w->points[0][0], w->points[0][1], w->points[0][2], w->points[1][0], w->points[1][1], w->points[1][2], w->points[2][0], w->points[2][1], w->points[2][2], w->points[3][0], w->points[3][1], w->points[3][2]);
	}
#endif
}

void BufWinding_Divide(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, winding_t *outfront, int *neededfrontpoints, winding_t *outback, int *neededbackpoints)
{
	double dot, *p1, *p2, mid[3], splitnormal[3], dists[MAX_POINTS_ON_WINDING + 1];
	int i, j, frontpts, backpts, counts[3], sides[MAX_POINTS_ON_WINDING + 1];

	if (outfront)
		outfront->numpoints = 0;
	if (outback)
		outback->numpoints = 0;

	if (in->numpoints > MAX_POINTS_ON_WINDING || (!outfront && !outback))
	{
		if (neededfrontpoints)
			*neededfrontpoints = 0;
		if (neededbackpoints)
			*neededbackpoints = 0;
		return;
	}

	splitnormal[0] = splitnormalx;
	splitnormal[1] = splitnormaly;
	splitnormal[2] = splitnormalz;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < in->numpoints;i++)
	{
		dot = DotProduct(in->points[i], splitnormal);
		dot -= splitdist;
		dists[i] = dot;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	frontpts = 0;
	backpts = 0;
	for (i = 0;i < in->numpoints;i++)
	{
		if (sides[i] != SIDE_ON)
		{
			if (sides[i] == SIDE_FRONT)
				frontpts++;
			else if (sides[i] == SIDE_BACK)
				backpts++;
			if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
				continue;
		}
		frontpts++;
		backpts++;
	}

	if (neededfrontpoints)
		*neededfrontpoints = frontpts;
	if (neededbackpoints)
		*neededbackpoints = backpts;
	if ((outfront && outfront->maxpoints < frontpts) || (outback && outback->maxpoints < backpts))
		return;

	for (i = 0;i < in->numpoints;i++)
	{
		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			if (outfront)
			{
				VectorCopy(p1, outfront->points[outfront->numpoints]);
				outfront->numpoints++;
			}
			if (outback)
			{
				VectorCopy(p1, outback->points[outback->numpoints]);
				outback->numpoints++;
			}
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			if (outfront)
			{
				VectorCopy(p1, outfront->points[outfront->numpoints]);
				outfront->numpoints++;
			}
		}
		else if (sides[i] == SIDE_BACK)
		{
			if (outback)
			{
				VectorCopy(p1, outback->points[outback->numpoints]);
				outback->numpoints++;
			}
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (splitnormal[j] == 1)
				mid[j] = splitdist;
			else if (splitnormal[j] == -1)
				mid[j] = -splitdist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		if (outfront)
		{
			VectorCopy(mid, outfront->points[outfront->numpoints]);
			outfront->numpoints++;
		}
		if (outback)
		{
			VectorCopy(mid, outback->points[outback->numpoints]);
			outback->numpoints++;
		}
	}
}

void Polygon_Divide_Double(int innumpoints, const double *inpoints, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, int outfrontmaxpoints, double *outfrontpoints, int *neededfrontpoints, int outbackmaxpoints, double *outbackpoints, int *neededbackpoints)
{
	double dot, mid[3], splitnormal[3], dists[MAX_POINTS_ON_WINDING + 1];
	const double *p1, *p2;
	int i, j, frontpts, backpts, counts[3], sides[MAX_POINTS_ON_WINDING + 1];

	if (neededfrontpoints)
		*neededfrontpoints = 0;
	if (neededbackpoints)
		*neededbackpoints = 0;

	if (innumpoints > MAX_POINTS_ON_WINDING || (!outfrontmaxpoints && !outbackmaxpoints))
		return;

	splitnormal[0] = splitnormalx;
	splitnormal[1] = splitnormaly;
	splitnormal[2] = splitnormalz;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < innumpoints;i++)
	{
		dot = DotProduct(inpoints + i * 3, splitnormal);
		dot -= splitdist;
		dists[i] = dot;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	frontpts = 0;
	backpts = 0;
	for (i = 0;i < innumpoints;i++)
	{
		if (sides[i] != SIDE_ON)
		{
			if (sides[i] == SIDE_FRONT)
				frontpts++;
			else if (sides[i] == SIDE_BACK)
				backpts++;
			if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
				continue;
		}
		frontpts++;
		backpts++;
	}

	if (neededfrontpoints)
		*neededfrontpoints = frontpts;
	if (neededbackpoints)
		*neededbackpoints = backpts;
	if ((outfrontmaxpoints && outfrontmaxpoints < frontpts) || (outbackmaxpoints && outbackmaxpoints < backpts))
		return;

	for (i = 0;i < innumpoints;i++)
	{
		p1 = inpoints + i * 3;

		if (sides[i] == SIDE_ON)
		{
			if (outfrontmaxpoints)
			{
				VectorCopy(p1, outfrontpoints);
				outfrontpoints += 3;
			}
			if (outbackmaxpoints)
			{
				VectorCopy(p1, outbackpoints);
				outbackpoints += 3;
			}
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			if (outfrontmaxpoints)
			{
				VectorCopy(p1, outfrontpoints);
				outfrontpoints += 3;
			}
		}
		else if (sides[i] == SIDE_BACK)
		{
			if (outbackmaxpoints)
			{
				VectorCopy(p1, outbackpoints);
				outbackpoints += 3;
			}
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = inpoints + ((i+1)%innumpoints) * 3;

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (splitnormal[j] == 1)
				mid[j] = splitdist;
			else if (splitnormal[j] == -1)
				mid[j] = -splitdist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		if (outfrontmaxpoints)
		{
			VectorCopy(mid, outfrontpoints);
			outfrontpoints += 3;
		}
		if (outbackmaxpoints)
		{
			VectorCopy(mid, outbackpoints);
			outbackpoints += 3;
		}
	}
}

void Polygon_Divide_Float(int innumpoints, const float *inpoints, float splitnormalx, float splitnormaly, float splitnormalz, float splitdist, int outfrontmaxpoints, float *outfrontpoints, int *neededfrontpoints, int outbackmaxpoints, float *outbackpoints, int *neededbackpoints)
{
	float dot, mid[3], splitnormal[3], dists[MAX_POINTS_ON_WINDING + 1];
	const float *p1, *p2;
	int i, j, frontpts, backpts, counts[3], sides[MAX_POINTS_ON_WINDING + 1];

	if (neededfrontpoints)
		*neededfrontpoints = 0;
	if (neededbackpoints)
		*neededbackpoints = 0;

	if (innumpoints > MAX_POINTS_ON_WINDING || (!outfrontmaxpoints && !outbackmaxpoints))
		return;

	splitnormal[0] = splitnormalx;
	splitnormal[1] = splitnormaly;
	splitnormal[2] = splitnormalz;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < innumpoints;i++)
	{
		dot = DotProduct(inpoints + i * 3, splitnormal);
		dot -= splitdist;
		dists[i] = dot;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	frontpts = 0;
	backpts = 0;
	for (i = 0;i < innumpoints;i++)
	{
		if (sides[i] != SIDE_ON)
		{
			if (sides[i] == SIDE_FRONT)
				frontpts++;
			else if (sides[i] == SIDE_BACK)
				backpts++;
			if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
				continue;
		}
		frontpts++;
		backpts++;
	}

	if (neededfrontpoints)
		*neededfrontpoints = frontpts;
	if (neededbackpoints)
		*neededbackpoints = backpts;
	if ((outfrontmaxpoints && outfrontmaxpoints < frontpts) || (outbackmaxpoints && outbackmaxpoints < backpts))
		return;

	for (i = 0;i < innumpoints;i++)
	{
		p1 = inpoints + i * 3;

		if (sides[i] == SIDE_ON)
		{
			if (outfrontmaxpoints)
			{
				VectorCopy(p1, outfrontpoints);
				outfrontpoints += 3;
			}
			if (outbackmaxpoints)
			{
				VectorCopy(p1, outbackpoints);
				outbackpoints += 3;
			}
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			if (outfrontmaxpoints)
			{
				VectorCopy(p1, outfrontpoints);
				outfrontpoints += 3;
			}
		}
		else if (sides[i] == SIDE_BACK)
		{
			if (outbackmaxpoints)
			{
				VectorCopy(p1, outbackpoints);
				outbackpoints += 3;
			}
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = inpoints + ((i+1)%innumpoints) * 3;

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (splitnormal[j] == 1)
				mid[j] = splitdist;
			else if (splitnormal[j] == -1)
				mid[j] = -splitdist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		if (outfrontmaxpoints)
		{
			VectorCopy(mid, outfrontpoints);
			outfrontpoints += 3;
		}
		if (outbackmaxpoints)
		{
			VectorCopy(mid, outbackpoints);
			outbackpoints += 3;
		}
	}
}

