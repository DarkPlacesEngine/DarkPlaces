
#include "quakedef.h"

typedef struct
{
	// the hull we're tracing through
	const hull_t *hull;

	// the trace structure to fill in
	trace_t *trace;

	// start and end of the trace (in model space)
	double start[3];
	double end[3];

	// end - start (for quick fraction -> vector conversions)
	double dist[3];
}
RecursiveHullCheckTraceInfo_t;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

static void RecursiveHullCheck_Impact (RecursiveHullCheckTraceInfo_t *t, const mplane_t *plane, const int side)
{
	// LordHavoc: using doubles for extra accuracy
	double t1, t2, frac, pdist;

	// LordHavoc: now that we have found the impact, recalculate the impact
	// point from scratch for maximum accuracy, with an epsilon bias on the
	// surface distance
	pdist = plane->dist;
	if (side)
	{
		pdist -= DIST_EPSILON;
		VectorNegate (plane->normal, t->trace->plane.normal);
		t->trace->plane.dist = -plane->dist;
	}
	else
	{
		pdist += DIST_EPSILON;
		VectorCopy (plane->normal, t->trace->plane.normal);
		t->trace->plane.dist = plane->dist;
	}

	if (plane->type < 3)
	{
		t1 = t->start[plane->type] - pdist;
		t2 = t->start[plane->type] + t->dist[plane->type] - pdist;
	}
	else
	{
		t1 = plane->normal[0] * t->start[0] + plane->normal[1] * t->start[1] + plane->normal[2] * t->start[2] - pdist;
		t2 = plane->normal[0] * (t->start[0] + t->dist[0]) + plane->normal[1] * (t->start[1] + t->dist[1]) + plane->normal[2] * (t->start[2] + t->dist[2]) - pdist;
	}

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0);

	t->trace->fraction = frac;
	VectorMA(t->start, frac, t->dist, t->trace->endpos);
}

static int RecursiveHullCheck (RecursiveHullCheckTraceInfo_t *t, int num, double p1f, double p2f, const double p1[3], const double p2[3])
{
	// status variables, these don't need to be saved on the stack when
	// recursing...  but are because this should be thread-safe
	// (note: tracing against a bbox is not thread-safe, yet)
	int ret;
	mplane_t *plane;
	double t1, t2, frac;

	// variables that need to be stored on the stack when recursing
	dclipnode_t	*node;
	int			side;
	double		midf, mid[3];

	// LordHavoc: a goto!  everyone flee in terror... :)
loc0:
	// check for empty
	if (num < 0)
	{
		t->trace->endcontents = num;
		if (t->trace->startcontents)
		{
			if (num == t->trace->startcontents)
				t->trace->allsolid = false;
			else
			{
				// if the first leaf is solid, set startsolid
				if (t->trace->allsolid)
					t->trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
		else
		{
			if (num != CONTENTS_SOLID)
			{
				t->trace->allsolid = false;
				if (num == CONTENTS_EMPTY)
					t->trace->inopen = true;
				else
					t->trace->inwater = true;
			}
			else
			{
				// if the first leaf is solid, set startsolid
				if (t->trace->allsolid)
					t->trace->startsolid = true;
				return HULLCHECKSTATE_SOLID;
			}
			return HULLCHECKSTATE_EMPTY;
		}
	}

	// find the point distances
	node = t->hull->clipnodes + num;

	plane = t->hull->planes + node->planenum;
	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	side = t1 < 0;

	if (side)
	{
		if (t1 < -DIST_EPSILON && t2 < -DIST_EPSILON)
		{
			num = node->children[1];
			goto loc0;
		}
	}
	else
	{
		if (t1 > DIST_EPSILON && t2 > DIST_EPSILON)
		{
			num = node->children[0];
			goto loc0;
		}
	}

	// the line (almost) intersects, recurse both sides

	frac = t1 / (t1 - t2);
	frac = bound(0.0f, frac, 1.0);

	midf = p1f + ((p2f - p1f) * frac);
	VectorMA(t->start, midf, t->dist, mid);

	// front side first
	ret = RecursiveHullCheck (t, node->children[side], p1f, midf, p1, mid);
	if (ret != HULLCHECKSTATE_EMPTY)
		return ret; // solid or done
	ret = RecursiveHullCheck (t, node->children[!side], midf, p2f, mid, p2);
	if (ret != HULLCHECKSTATE_SOLID)
		return ret; // empty or done

	// front is air and back is solid, this is the impact point...
	RecursiveHullCheck_Impact(t, t->hull->planes + node->planenum, side);

	return HULLCHECKSTATE_DONE;
}

void Collision_RoundUpToHullSize(const model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs)
{
	vec3_t size;
	const hull_t *hull;

	VectorSubtract(inmaxs, inmins, size);
	if (cmodel->ishlbsp)
	{
		if (size[0] < 3)
			hull = &cmodel->hulls[0]; // 0x0x0
		else if (size[0] <= 32)
		{
			if (size[2] < 54) // pick the nearest of 36 or 72
				hull = &cmodel->hulls[3]; // 32x32x36
			else
				hull = &cmodel->hulls[1]; // 32x32x72
		}
		else
			hull = &cmodel->hulls[2]; // 64x64x64
	}
	else
	{
		if (size[0] < 3)
			hull = &cmodel->hulls[0]; // 0x0x0
		else if (size[0] <= 32)
			hull = &cmodel->hulls[1]; // 32x32x56
		else
			hull = &cmodel->hulls[2]; // 64x64x88
	}
	VectorCopy(inmins, outmins);
	VectorAdd(inmins, hull->clip_size, outmaxs);
}

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

void Collision_Init (void)
{
	int		i;
	int		side;

	//Set up the planes and clipnodes so that the six floats of a bounding box
	//can just be stored out and get a proper hull_t structure.

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0;i < 6;i++)
	{
		box_clipnodes[i].planenum = i;

		side = i&1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;

		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}
}


static hull_t *HullForBBoxEntity (const vec3_t corigin, const vec3_t cmins, const vec3_t cmaxs, const vec3_t mins, const vec3_t maxs, vec3_t offset)
{
	vec3_t hullmins, hullmaxs;

	// create a temp hull from bounding box sizes
	VectorCopy (corigin, offset);
	VectorSubtract (cmins, maxs, hullmins);
	VectorSubtract (cmaxs, mins, hullmaxs);

	//To keep everything totally uniform, bounding boxes are turned into small
	//BSP trees instead of being compared directly.
	box_planes[0].dist = hullmaxs[0];
	box_planes[1].dist = hullmins[0];
	box_planes[2].dist = hullmaxs[1];
	box_planes[3].dist = hullmins[1];
	box_planes[4].dist = hullmaxs[2];
	box_planes[5].dist = hullmins[2];
	return &box_hull;
}

static const hull_t *HullForBrushModel (const model_t *cmodel, const vec3_t corigin, const vec3_t mins, const vec3_t maxs, vec3_t offset)
{
	vec3_t size;
	const hull_t *hull;

	// decide which clipping hull to use, based on the size
	// explicit hulls in the BSP model
	VectorSubtract (maxs, mins, size);
	// LordHavoc: FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (cmodel->ishlbsp)
	{
		if (size[0] < 3)
			hull = &cmodel->hulls[0]; // 0x0x0
		else if (size[0] <= 32)
		{
			if (size[2] < 54) // pick the nearest of 36 or 72
				hull = &cmodel->hulls[3]; // 32x32x36
			else
				hull = &cmodel->hulls[1]; // 32x32x72
		}
		else
			hull = &cmodel->hulls[2]; // 64x64x64
	}
	else
	{
		if (size[0] < 3)
			hull = &cmodel->hulls[0]; // 0x0x0
		else if (size[0] <= 32)
			hull = &cmodel->hulls[1]; // 32x32x56
		else
			hull = &cmodel->hulls[2]; // 64x64x88
	}

	// calculate an offset value to center the origin
	VectorSubtract (hull->clip_mins, mins, offset);
	VectorAdd (offset, corigin, offset);

	return hull;
}

void Collision_ClipTrace (trace_t *trace, const void *cent, const model_t *cmodel, const vec3_t corigin, const vec3_t cangles, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
	RecursiveHullCheckTraceInfo_t rhc;
	vec3_t offset, forward, left, up;
	double startd[3], endd[3], tempd[3];

	// fill in a default trace
	memset (&rhc, 0, sizeof(rhc));
	memset (trace, 0, sizeof(trace_t));

	rhc.trace = trace;

	rhc.trace->fraction = 1;
	rhc.trace->allsolid = true;

	if (cmodel && cmodel->type == mod_brush)
	{
		// brush model

		// get the clipping hull
		rhc.hull = HullForBrushModel (cmodel, corigin, mins, maxs, offset);

		VectorSubtract(start, offset, startd);
		VectorSubtract(end, offset, endd);

		// rotate start and end into the model's frame of reference
		if (cangles[0] || cangles[1] || cangles[2])
		{
			AngleVectorsFLU (cangles, forward, left, up);
			VectorCopy(startd, tempd);
			startd[0] = DotProduct (tempd, forward);
			startd[1] = DotProduct (tempd, left);
			startd[2] = DotProduct (tempd, up);
			VectorCopy(endd, tempd);
			endd[0] = DotProduct (tempd, forward);
			endd[1] = DotProduct (tempd, left);
			endd[2] = DotProduct (tempd, up);
		}

		VectorCopy(end, rhc.trace->endpos);

		// trace a line through the appropriate clipping hull
		VectorCopy(startd, rhc.start);
		VectorCopy(endd, rhc.end);

		VectorSubtract(rhc.end, rhc.start, rhc.dist);
		RecursiveHullCheck (&rhc, rhc.hull->firstclipnode, 0, 1, startd, endd);

		// if we hit, unrotate endpos and normal, and store the entity we hit
		if (rhc.trace->fraction != 1)
		{
			// rotate endpos back to world frame of reference
			if (cangles[0] || cangles[1] || cangles[2])
			{
				VectorNegate (cangles, offset);
				AngleVectorsFLU (offset, forward, left, up);

				VectorCopy (rhc.trace->endpos, tempd);
				rhc.trace->endpos[0] = DotProduct (tempd, forward);
				rhc.trace->endpos[1] = DotProduct (tempd, left);
				rhc.trace->endpos[2] = DotProduct (tempd, up);

				VectorCopy (rhc.trace->plane.normal, tempd);
				rhc.trace->plane.normal[0] = DotProduct (tempd, forward);
				rhc.trace->plane.normal[1] = DotProduct (tempd, left);
				rhc.trace->plane.normal[2] = DotProduct (tempd, up);
			}
			// fix offset
			VectorAdd (rhc.trace->endpos, offset, rhc.trace->endpos);
			rhc.trace->ent = (void *) cent;
		}
		else if (rhc.trace->allsolid || rhc.trace->startsolid)
			rhc.trace->ent = (void *) cent;
	}
	else
	{
		// bounding box

		rhc.hull = HullForBBoxEntity (corigin, cmins, cmaxs, mins, maxs, offset);

		VectorSubtract(start, offset, startd);
		VectorSubtract(end, offset, endd);
		VectorCopy(end, rhc.trace->endpos);

		// trace a line through the generated clipping hull
		VectorCopy(startd, rhc.start);
		VectorSubtract(endd, startd, rhc.dist);
		RecursiveHullCheck (&rhc, rhc.hull->firstclipnode, 0, 1, startd, endd);

		// if we hit, store the entity we hit
		if (rhc.trace->fraction != 1)
		{
			// fix offset
			VectorAdd (rhc.trace->endpos, offset, rhc.trace->endpos);
			rhc.trace->ent = (void *) cent;
		}
		else if (rhc.trace->allsolid || rhc.trace->startsolid)
			rhc.trace->ent = (void *) cent;
	}
}
