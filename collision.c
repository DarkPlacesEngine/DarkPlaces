
#include "quakedef.h"
#include "winding.h"

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

#if 0
typedef struct
{
	// the hull we're tracing through
	const hull_t *hull;

	// the trace structure to fill in
	trace_t *trace;

	// start and end of the trace (in model space)
	double start[3];
	double end[3];

	// end - start
	double dist[3];

	// overrides the CONTENTS_SOLID in the box bsp tree
	int boxsupercontents;
}
RecursiveHullCheckTraceInfo_t;

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

static int RecursiveHullCheck(RecursiveHullCheckTraceInfo_t *t, int num, double p1f, double p2f, double p1[3], double p2[3])
{
	// status variables, these don't need to be saved on the stack when
	// recursing...  but are because this should be thread-safe
	// (note: tracing against a bbox is not thread-safe, yet)
	int ret;
	mplane_t *plane;
	double t1, t2;

	// variables that need to be stored on the stack when recursing
	dclipnode_t *node;
	int side;
	double midf, mid[3];

	// LordHavoc: a goto!  everyone flee in terror... :)
loc0:
	// check for empty
	if (num < 0)
	{
		num = Mod_Q1BSP_SuperContentsFromNativeContents(NULL, num);
		if (!t->trace->startfound)
		{
			t->trace->startfound = true;
			t->trace->startsupercontents |= num;
		}
		if (num & SUPERCONTENTS_LIQUIDSMASK)
			t->trace->inwater = true;
		if (num == 0)
			t->trace->inopen = true;
		if (num & t->trace->hitsupercontentsmask)
		{
			// if the first leaf is solid, set startsolid
			if (t->trace->allsolid)
				t->trace->startsolid = true;
#if COLLISIONPARANOID >= 3
			Con_Printf("S");
#endif
			return HULLCHECKSTATE_SOLID;
		}
		else
		{
			t->trace->allsolid = false;
#if COLLISIONPARANOID >= 3
			Con_Printf("E");
#endif
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

	if (t1 < 0)
	{
		if (t2 < 0)
		{
#if COLLISIONPARANOID >= 3
			Con_Printf("<");
#endif
			num = node->children[1];
			goto loc0;
		}
		side = 1;
	}
	else
	{
		if (t2 >= 0)
		{
#if COLLISIONPARANOID >= 3
			Con_Printf(">");
#endif
			num = node->children[0];
			goto loc0;
		}
		side = 0;
	}

	// the line intersects, find intersection point
	// LordHavoc: this uses the original trace for maximum accuracy
#if COLLISIONPARANOID >= 3
	Con_Printf("M");
#endif
	if (plane->type < 3)
	{
		t1 = t->start[plane->type] - plane->dist;
		t2 = t->end[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, t->start) - plane->dist;
		t2 = DotProduct (plane->normal, t->end) - plane->dist;
	}

	midf = t1 / (t1 - t2);
	midf = bound(p1f, midf, p2f);
	VectorMA(t->start, midf, t->dist, mid);

	// recurse both sides, front side first
	ret = RecursiveHullCheck (t, node->children[side], p1f, midf, p1, mid);
	// if this side is not empty, return what it is (solid or done)
	if (ret != HULLCHECKSTATE_EMPTY)
		return ret;

	ret = RecursiveHullCheck (t, node->children[side ^ 1], midf, p2f, mid, p2);
	// if other side is not solid, return what it is (empty or done)
	if (ret != HULLCHECKSTATE_SOLID)
		return ret;

	// front is air and back is solid, this is the impact point...
	if (side)
	{
		t->trace->plane.dist = -plane->dist;
		VectorNegate (plane->normal, t->trace->plane.normal);
	}
	else
	{
		t->trace->plane.dist = plane->dist;
		VectorCopy (plane->normal, t->trace->plane.normal);
	}

	// bias away from surface a bit
	t1 = DotProduct(t->trace->plane.normal, t->start) - (t->trace->plane.dist + DIST_EPSILON);
	t2 = DotProduct(t->trace->plane.normal, t->end) - (t->trace->plane.dist + DIST_EPSILON);

	midf = t1 / (t1 - t2);
	t->trace->fraction = bound(0.0f, midf, 1.0);

#if COLLISIONPARANOID >= 3
	Con_Printf("D");
#endif
	return HULLCHECKSTATE_DONE;
}

#if 0
// used if start and end are the same
static void RecursiveHullCheckPoint (RecursiveHullCheckTraceInfo_t *t, int num)
{
	// If you can read this, you understand BSP trees
	while (num >= 0)
		num = t->hull->clipnodes[num].children[((t->hull->planes[t->hull->clipnodes[num].planenum].type < 3) ? (t->start[t->hull->planes[t->hull->clipnodes[num].planenum].type]) : (DotProduct(t->hull->planes[t->hull->clipnodes[num].planenum].normal, t->start))) < t->hull->planes[t->hull->clipnodes[num].planenum].dist];

	// check for empty
	t->trace->endcontents = num;
	if (t->trace->thiscontents)
	{
		if (num == t->trace->thiscontents)
			t->trace->allsolid = false;
		else
		{
			// if the first leaf is solid, set startsolid
			if (t->trace->allsolid)
				t->trace->startsolid = true;
		}
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
		}
	}
}
#endif

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

void Mod_Q1BSP_Collision_Init (void)
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

void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int boxsupercontents)
{
	RecursiveHullCheckTraceInfo_t rhc;
	// fill in a default trace
	memset(&rhc, 0, sizeof(rhc));
	memset(trace, 0, sizeof(trace_t));
	//To keep everything totally uniform, bounding boxes are turned into small
	//BSP trees instead of being compared directly.
	// create a temp hull from bounding box sizes
	box_planes[0].dist = cmaxs[0] - mins[0];
	box_planes[1].dist = cmins[0] - maxs[0];
	box_planes[2].dist = cmaxs[1] - mins[1];
	box_planes[3].dist = cmins[1] - maxs[1];
	box_planes[4].dist = cmaxs[2] - mins[2];
	box_planes[5].dist = cmins[2] - maxs[2];
	// trace a line through the generated clipping hull
	rhc.boxsupercontents = boxsupercontents;
	rhc.hull = &box_hull;
	rhc.trace = trace;
	rhc.trace->hitsupercontentsmask = hitsupercontentsmask;
	rhc.trace->fraction = 1;
	rhc.trace->allsolid = true;
	VectorCopy(start, rhc.start);
	VectorCopy(end, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
	Mod_Q1BSP_RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
	VectorMA(rhc.start, rhc.trace->fraction, rhc.dist, rhc.trace->endpos);
	if (rhc.trace->startsupercontents)
		rhc.trace->startsupercontents = boxsupercontents;
}
#endif

void Collision_Init (void)
{
}














void Collision_PrintBrushAsQHull(colbrushf_t *brush, const char *name)
{
	int i;
	Con_Printf("3 %s\n%i\n", name, brush->numpoints);
	for (i = 0;i < brush->numpoints;i++)
		Con_Printf("%f %f %f\n", brush->points[i].v[0], brush->points[i].v[1], brush->points[i].v[2]);
	// FIXME: optimize!
	Con_Printf("4\n%i\n", brush->numplanes);
	for (i = 0;i < brush->numplanes;i++)
		Con_Printf("%f %f %f %f\n", brush->planes[i].normal[0], brush->planes[i].normal[1], brush->planes[i].normal[2], brush->planes[i].dist);
}

void Collision_ValidateBrush(colbrushf_t *brush)
{
	int j, k, pointsoffplanes, printbrush;
	float d;
	printbrush = false;
	if (!brush->numpoints)
	{
		Con_Printf("Collision_ValidateBrush: brush with no points!\n");
		printbrush = true;
	}
#if 0
	// it's ok for a brush to have one point and no planes...
	if (brush->numplanes == 0 && brush->numpoints != 1)
	{
		Con_Printf("Collision_ValidateBrush: brush with no planes and more than one point!\n");
		printbrush = true;
	}
#endif
	if (brush->numplanes)
	{
		pointsoffplanes = 0;
		for (k = 0;k < brush->numplanes;k++)
		{
			if (DotProduct(brush->planes[k].normal, brush->planes[k].normal) < 0.0001f)
				Con_Printf("Collision_ValidateBrush: plane #%i (%f %f %f %f) is degenerate\n", k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist);
			for (j = 0;j < brush->numpoints;j++)
			{
				d = DotProduct(brush->points[j].v, brush->planes[k].normal) - brush->planes[k].dist;
				if (d > (1.0f / 8.0f))
				{
					Con_Printf("Collision_ValidateBrush: point #%i (%f %f %f) infront of plane #%i (%f %f %f %f)\n", j, brush->points[j].v[0], brush->points[j].v[1], brush->points[j].v[2], k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist);
					printbrush = true;
				}
				if (fabs(d) > 0.01f)
					pointsoffplanes++;
			}
		}
		if (pointsoffplanes == 0) // all points are on all planes
		{
			Con_Printf("Collision_ValidateBrush: all points lie on all planes (degenerate, no brush volume!)\n");
			printbrush = true;
		}
	}
	if (printbrush)
		Collision_PrintBrushAsQHull(brush, "unnamed");
}

float nearestplanedist_float(const float *normal, const colpointf_t *points, int numpoints)
{
	float dist, bestdist;
	bestdist = DotProduct(points->v, normal);
	points++;
	while(--numpoints)
	{
		dist = DotProduct(points->v, normal);
		bestdist = min(bestdist, dist);
		points++;
	}
	return bestdist;
}

float furthestplanedist_float(const float *normal, const colpointf_t *points, int numpoints)
{
	float dist, bestdist;
	bestdist = DotProduct(points->v, normal);
	points++;
	while(--numpoints)
	{
		dist = DotProduct(points->v, normal);
		bestdist = max(bestdist, dist);
		points++;
	}
	return bestdist;
}


colbrushf_t *Collision_NewBrushFromPlanes(mempool_t *mempool, int numoriginalplanes, const mplane_t *originalplanes, int supercontents, winding_t *temp1, winding_t *temp2)
{
	int j, k, m;
	int numpoints, maxpoints, numplanes, maxplanes, numelements, maxelements, numtriangles, numpolypoints, maxpolypoints;
	winding_t *w, *temp, *othertemp;
	colbrushf_t *brush;
	colpointf_t pointsbuf[256];
	colplanef_t planesbuf[256];
	int elementsbuf[1024];
	int polypointbuf[256];
	float mins[3], maxs[3];
	// construct a collision brush (points, planes, and renderable mesh) from
	// a set of planes, this also optimizes out any unnecessary planes (ones
	// whose polygon is clipped away by the other planes)
	numpoints = 0;maxpoints = 256;
	numplanes = 0;maxplanes = 256;
	numelements = 0;maxelements = 1024;
	numtriangles = 0;
	maxpolypoints = 256;
	for (j = 0;j < numoriginalplanes;j++)
	{
		// add the plane uniquely (no duplicates)
		for (k = 0;k < numplanes;k++)
			if (VectorCompare(planesbuf[k].normal, originalplanes[j].normal) && planesbuf[k].dist == originalplanes[j].dist)
				break;
		// if the plane is a duplicate, skip it
		if (k < numplanes)
			continue;
		// check if there are too many and skip the brush
		if (numplanes >= 256)
		{
			Con_Printf("Mod_Q3BSP_LoadBrushes: failed to build collision brush: too many planes for buffer\n");
			return NULL;
		}

		// create a large polygon from the plane
		w = temp1;
		othertemp = temp2;
		BufWinding_NewFromPlane(w, originalplanes[j].normal[0], originalplanes[j].normal[1], originalplanes[j].normal[2], originalplanes[j].dist);
		// clip it by all other planes
		for (k = 0;k < numoriginalplanes && w->numpoints;k++)
		{
			if (k != j)
			{
				// we want to keep the inside of the brush plane so we flip
				// the cutting plane
				BufWinding_Divide(w, -originalplanes[k].normal[0], -originalplanes[k].normal[1], -originalplanes[k].normal[2], -originalplanes[k].dist, othertemp, NULL, NULL, NULL);
				temp = w;
				w = othertemp;
				othertemp = temp;
			}
		}
		// if nothing is left, skip it
		if (!w->numpoints)
			continue;

		// copy off the number of points for later when the winding is freed
		numpolypoints = w->numpoints;

		// check if there are too many polygon vertices for buffer
		if (numpolypoints > maxpolypoints)
		{
			Con_Printf("Collision_NewBrushFromPlanes: failed to build collision brush: too many points for buffer\n");
			return NULL;
		}

		// check if there are too many triangle elements for buffer
		if (numelements + (w->numpoints - 2) * 3 > maxelements)
		{
			Con_Printf("Collision_NewBrushFromPlanes: failed to build collision brush: too many triangle elements for buffer\n");
			return NULL;
		}

		for (k = 0;k < w->numpoints;k++)
		{
			// check if there is already a matching point (no duplicates)
			for (m = 0;m < numpoints;m++)
				if (VectorDistance2(w->points[k], pointsbuf[m].v) < DIST_EPSILON)
					break;

			// if there is no match, add a new one
			if (m == numpoints)
			{
				// check if there are too many and skip the brush
				if (numpoints >= 256)
				{
					Con_Printf("Collision_NewBrushFromPlanes: failed to build collision brush: too many points for buffer\n");
					Winding_Free(w);
					return NULL;
				}
				// add the new one
				VectorCopy(w->points[k], pointsbuf[numpoints].v);
				numpoints++;
			}

			// store the index into a buffer
			polypointbuf[k] = m;
		}
		w = NULL;
		othertemp = NULL;
		temp = NULL;

		// add the triangles for the polygon
		// (this particular code makes a triangle fan)
		for (k = 0;k < numpolypoints - 2;k++)
		{
			numtriangles++;
			elementsbuf[numelements++] = polypointbuf[0];
			elementsbuf[numelements++] = polypointbuf[k + 1];
			elementsbuf[numelements++] = polypointbuf[k + 2];
		}

		// add the new plane
		VectorCopy(originalplanes[j].normal, planesbuf[numplanes].normal);
		planesbuf[numplanes].dist = originalplanes[j].dist;
		numplanes++;
	}

	// recalc distances
	for (j = 0;j < numplanes;j++)
		planesbuf[j].dist = furthestplanedist_float(planesbuf[j].normal, pointsbuf, numpoints);

	if (numpoints)
	{
		VectorCopy(pointsbuf[0].v, mins);
		VectorCopy(pointsbuf[0].v, maxs);
		for (j = 1;j < numpoints;j++)
		{
			mins[0] = min(mins[0], pointsbuf[j].v[0]);
			mins[1] = min(mins[1], pointsbuf[j].v[1]);
			mins[2] = min(mins[2], pointsbuf[j].v[2]);
			maxs[0] = max(maxs[0], pointsbuf[j].v[0]);
			maxs[1] = max(maxs[1], pointsbuf[j].v[1]);
			maxs[2] = max(maxs[2], pointsbuf[j].v[2]);
		}
	}

	// if nothing is left, there's nothing to allocate
	if (numtriangles < 4 || numplanes < 4 || numpoints < 4)
		return NULL;

	// allocate the brush and copy to it
	brush = Collision_AllocBrushFloat(mempool, numpoints, numplanes, numtriangles, supercontents);
	memcpy(brush->points, pointsbuf, numpoints * sizeof(colpointf_t));
	memcpy(brush->planes, planesbuf, numplanes * sizeof(colplanef_t));
	memcpy(brush->elements, elementsbuf, numtriangles * sizeof(int[3]));
	VectorCopy(mins, brush->mins);
	VectorCopy(maxs, brush->maxs);
	Collision_ValidateBrush(brush);
	return brush;
}



colbrushf_t *Collision_AllocBrushFloat(mempool_t *mempool, int numpoints, int numplanes, int numtriangles, int supercontents)
{
	colbrushf_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colpointf_t) * numpoints + sizeof(colplanef_t) * numplanes + sizeof(int[3]) * numtriangles);
	brush->supercontents = supercontents;
	brush->numplanes = numplanes;
	brush->numpoints = numpoints;
	brush->numtriangles = numtriangles;
	brush->planes = (void *)(brush + 1);
	brush->points = (void *)(brush->planes + brush->numplanes);
	brush->elements = (void *)(brush->points + brush->numpoints);
	return brush;
}

void Collision_CalcPlanesForPolygonBrushFloat(colbrushf_t *brush)
{
	int i;
	float edge0[3], edge1[3], edge2[3], normal[3], dist, bestdist, temp[3];
	colpointf_t *p, *p2;

	if (brush->numpoints == 3)
	{
		// optimized triangle case
		TriangleNormal(brush->points[0].v, brush->points[1].v, brush->points[2].v, brush->planes[0].normal);
		if (DotProduct(brush->planes[0].normal, brush->planes[0].normal) < 0.0001f)
		{
			// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
			brush->numplanes = 0;
			return;
		}
		else
		{
			brush->numplanes = 5;
			VectorNormalize(brush->planes[0].normal);
			brush->planes[0].dist = DotProduct(brush->points->v, brush->planes[0].normal);
			VectorNegate(brush->planes[0].normal, brush->planes[1].normal);
			brush->planes[1].dist = -brush->planes[0].dist;
			VectorSubtract(brush->points[2].v, brush->points[0].v, edge0);
			VectorSubtract(brush->points[0].v, brush->points[1].v, edge1);
			VectorSubtract(brush->points[1].v, brush->points[2].v, edge2);
			CrossProduct(edge0, brush->planes->normal, brush->planes[2].normal);
			CrossProduct(edge1, brush->planes->normal, brush->planes[3].normal);
			CrossProduct(edge2, brush->planes->normal, brush->planes[4].normal);
			VectorNormalize(brush->planes[2].normal);
			VectorNormalize(brush->planes[3].normal);
			VectorNormalize(brush->planes[4].normal);
			brush->planes[2].dist = DotProduct(brush->points[2].v, brush->planes[2].normal);
			brush->planes[3].dist = DotProduct(brush->points[0].v, brush->planes[3].normal);
			brush->planes[4].dist = DotProduct(brush->points[1].v, brush->planes[4].normal);

			if (developer.integer)
			{
				// validation code
				//VectorSubtract(brush->points[0].v, brush->points[1].v, edge0);
				//VectorSubtract(brush->points[2].v, brush->points[1].v, edge1);
				CrossProduct(edge1, edge0, normal);
				VectorNormalize(normal);
				VectorSubtract(normal, brush->planes[0].normal, temp);
				if (VectorLength(temp) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: TriangleNormal gave wrong answer (%f %f %f != correct answer %f %f %f)\n", brush->planes->normal[0], brush->planes->normal[1], brush->planes->normal[2], normal[0], normal[1], normal[2]);
				if (fabs(DotProduct(brush->planes[1].normal, brush->planes[0].normal) - -1.0f) > 0.01f || fabs(brush->planes[1].dist - -brush->planes[0].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 1 (%f %f %f %f) is not opposite plane 0 (%f %f %f %f)\n", brush->planes[1].normal[0], brush->planes[1].normal[1], brush->planes[1].normal[2], brush->planes[1].dist, brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[0].dist);
				if (fabs(DotProduct(brush->planes[2].normal, brush->planes[0].normal)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 2 (%f %f %f %f) is not perpendicular to plane 0 (%f %f %f %f)\n", brush->planes[2].normal[0], brush->planes[2].normal[1], brush->planes[2].normal[2], brush->planes[2].dist, brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[2].dist);
				if (fabs(DotProduct(brush->planes[3].normal, brush->planes[0].normal)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 3 (%f %f %f %f) is not perpendicular to plane 0 (%f %f %f %f)\n", brush->planes[3].normal[0], brush->planes[3].normal[1], brush->planes[3].normal[2], brush->planes[3].dist, brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[3].dist);
				if (fabs(DotProduct(brush->planes[4].normal, brush->planes[0].normal)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 4 (%f %f %f %f) is not perpendicular to plane 0 (%f %f %f %f)\n", brush->planes[4].normal[0], brush->planes[4].normal[1], brush->planes[4].normal[2], brush->planes[4].dist, brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[4].dist);
				if (fabs(DotProduct(brush->planes[2].normal, edge0)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 2 (%f %f %f %f) is not perpendicular to edge 0 (%f %f %f to %f %f %f)\n", brush->planes[2].normal[0], brush->planes[2].normal[1], brush->planes[2].normal[2], brush->planes[2].dist, brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2]);
				if (fabs(DotProduct(brush->planes[3].normal, edge1)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 3 (%f %f %f %f) is not perpendicular to edge 1 (%f %f %f to %f %f %f)\n", brush->planes[3].normal[0], brush->planes[3].normal[1], brush->planes[3].normal[2], brush->planes[3].dist, brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2]);
				if (fabs(DotProduct(brush->planes[4].normal, edge2)) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 4 (%f %f %f %f) is not perpendicular to edge 2 (%f %f %f to %f %f %f)\n", brush->planes[4].normal[0], brush->planes[4].normal[1], brush->planes[4].normal[2], brush->planes[4].dist, brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2]);
				if (fabs(DotProduct(brush->points[0].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[0].normal) - brush->planes[0].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: edges (%f %f %f to %f %f %f to %f %f %f) off front plane 0 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[0].dist);
				if (fabs(DotProduct(brush->points[0].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[1].normal) - brush->planes[1].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: edges (%f %f %f to %f %f %f to %f %f %f) off back plane 1 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[1].normal[0], brush->planes[1].normal[1], brush->planes[1].normal[2], brush->planes[1].dist);
				if (fabs(DotProduct(brush->points[2].v, brush->planes[2].normal) - brush->planes[2].dist) > 0.01f || fabs(DotProduct(brush->points[0].v, brush->planes[2].normal) - brush->planes[2].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->planes[2].normal[0], brush->planes[2].normal[1], brush->planes[2].normal[2], brush->planes[2].dist);
				if (fabs(DotProduct(brush->points[0].v, brush->planes[3].normal) - brush->planes[3].dist) > 0.01f || fabs(DotProduct(brush->points[1].v, brush->planes[3].normal) - brush->planes[3].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[0].v[0], brush->points[0].v[1], brush->points[0].v[2], brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->planes[3].normal[0], brush->planes[3].normal[1], brush->planes[3].normal[2], brush->planes[3].dist);
				if (fabs(DotProduct(brush->points[1].v, brush->planes[4].normal) - brush->planes[4].dist) > 0.01f || fabs(DotProduct(brush->points[2].v, brush->planes[4].normal) - brush->planes[4].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: edge 0 (%f %f %f to %f %f %f) off front plane 2 (%f %f %f %f)\n", brush->points[1].v[0], brush->points[1].v[1], brush->points[1].v[2], brush->points[2].v[0], brush->points[2].v[1], brush->points[2].v[2], brush->planes[4].normal[0], brush->planes[4].normal[1], brush->planes[4].normal[2], brush->planes[4].dist);
			}
		}
	}
	else
	{
		// choose best surface normal for polygon's plane
		bestdist = 0;
		for (i = 0, p = brush->points + 1;i < brush->numpoints - 2;i++, p++)
		{
			VectorSubtract(p[-1].v, p[0].v, edge0);
			VectorSubtract(p[1].v, p[0].v, edge1);
			CrossProduct(edge0, edge1, normal);
			//TriangleNormal(p[-1].v, p[0].v, p[1].v, normal);
			dist = DotProduct(normal, normal);
			if (i == 0 || bestdist < dist)
			{
				bestdist = dist;
				VectorCopy(normal, brush->planes->normal);
			}
		}
		if (bestdist < 0.0001f)
		{
			// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
			brush->numplanes = 0;
			return;
		}
		else
		{
			brush->numplanes = brush->numpoints + 2;
			VectorNormalize(brush->planes->normal);
			brush->planes->dist = DotProduct(brush->points->v, brush->planes->normal);

			// negate plane to create other side
			VectorNegate(brush->planes[0].normal, brush->planes[1].normal);
			brush->planes[1].dist = -brush->planes[0].dist;
			for (i = 0, p = brush->points + (brush->numpoints - 1), p2 = brush->points;i < brush->numpoints;i++, p = p2, p2++)
			{
				VectorSubtract(p->v, p2->v, edge0);
				CrossProduct(edge0, brush->planes->normal, brush->planes[i + 2].normal);
				VectorNormalize(brush->planes[i + 2].normal);
				brush->planes[i + 2].dist = DotProduct(p->v, brush->planes[i + 2].normal);
			}
		}
	}

	if (developer.integer)
	{
		// validity check - will be disabled later
		Collision_ValidateBrush(brush);
		for (i = 0;i < brush->numplanes;i++)
		{
			int j;
			for (j = 0, p = brush->points;j < brush->numpoints;j++, p++)
				if (DotProduct(p->v, brush->planes[i].normal) > brush->planes[i].dist + (1.0 / 32.0))
					Con_Printf("Error in brush plane generation, plane %i\n", i);
		}
	}
}

colbrushf_t *Collision_AllocBrushFromPermanentPolygonFloat(mempool_t *mempool, int numpoints, float *points, int supercontents)
{
	colbrushf_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colplanef_t) * (numpoints + 2));
	brush->supercontents = supercontents;
	brush->numpoints = numpoints;
	brush->numplanes = numpoints + 2;
	brush->planes = (void *)(brush + 1);
	brush->points = (colpointf_t *)points;
	Host_Error("Collision_AllocBrushFromPermanentPolygonFloat: FIXME: this code needs to be updated to generate a mesh...\n");
	return brush;
}

#define COLLISIONEPSILON (1.0f / 32.0f)
#define COLLISIONEPSILON2 0//(1.0f / 32.0f)

// NOTE: start and end of each brush pair must have same numplanes/numpoints
void Collision_TraceBrushBrushFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end)
{
	int nplane, nplane2, fstartsolid, fendsolid, brushsolid;
	float enterfrac, leavefrac, d1, d2, f, newimpactnormal[3];
	const colplanef_t *startplane, *endplane;

	enterfrac = -1;
	leavefrac = 1;
	fstartsolid = true;
	fendsolid = true;

	for (nplane = 0;nplane < thatbrush_start->numplanes + thisbrush_start->numplanes;nplane++)
	{
		nplane2 = nplane;
		if (nplane2 >= thatbrush_start->numplanes)
		{
			nplane2 -= thatbrush_start->numplanes;
			startplane = thisbrush_start->planes + nplane2;
			endplane = thisbrush_end->planes + nplane2;
			if (developer.integer)
			{
				// any brush with degenerate planes is not worth handling
				if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
				{
					Con_Printf("Collision_TraceBrushBrushFloat: degenerate thisbrush plane!\n");
					return;
				}
				f = furthestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints);
				if (fabs(f - startplane->dist) > 0.01f)
					Con_Printf("startplane->dist %f != calculated %f (thisbrush_start)\n", startplane->dist, f);
			}
			d1 = nearestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
			d2 = nearestplanedist_float(endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - furthestplanedist_float(endplane->normal, thatbrush_end->points, thatbrush_end->numpoints) - COLLISIONEPSILON2;
		}
		else
		{
			startplane = thatbrush_start->planes + nplane2;
			endplane = thatbrush_end->planes + nplane2;
			if (developer.integer)
			{
				// any brush with degenerate planes is not worth handling
				if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
				{
					Con_Printf("Collision_TraceBrushBrushFloat: degenerate thatbrush plane!\n");
					return;
				}
				f = furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
				if (fabs(f - startplane->dist) > 0.01f)
					Con_Printf("startplane->dist %f != calculated %f (thatbrush_start)\n", startplane->dist, f);
			}
			d1 = nearestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - startplane->dist;
			d2 = nearestplanedist_float(endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - startplane->dist - COLLISIONEPSILON2;
		}
		//Con_Printf("%c%i: d1 = %f, d2 = %f, d1 / (d1 - d2) = %f\n", nplane2 != nplane ? 'b' : 'a', nplane2, d1, d2, d1 / (d1 - d2));

		f = d1 - d2;
		if (f >= 0)
		{
			// moving into brush
			if (d2 > 0)
				return;
			if (d1 < 0)
				continue;
			// enter
			fstartsolid = false;
			f = (d1 - COLLISIONEPSILON) / f;
			f = bound(0, f, 1);
			if (enterfrac < f)
			{
				enterfrac = f;
				VectorLerp(startplane->normal, enterfrac, endplane->normal, newimpactnormal);
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return;
			if (d2 < 0)
				continue;
			// leave
			fendsolid = false;
			f = (d1 + COLLISIONEPSILON) / f;
			f = bound(0, f, 1);
			if (leavefrac > f)
				leavefrac = f;
		}
	}

	brushsolid = trace->hitsupercontentsmask & thatbrush_start->supercontents;
	if (fstartsolid)
	{
		trace->startsupercontents |= thatbrush_start->supercontents;
		if (brushsolid)
		{
			trace->startsolid = true;
			if (fendsolid)
				trace->allsolid = true;
		}
	}

	// LordHavoc: we need an epsilon nudge here because for a point trace the
	// penetrating line segment is normally zero length if this brush was
	// generated from a polygon (infinitely thin), and could even be slightly
	// positive or negative due to rounding errors in that case.
	if (brushsolid && enterfrac > -1 && enterfrac < trace->fraction && enterfrac - (1.0f / 1024.0f) <= leavefrac)
	{
		trace->fraction = bound(0, enterfrac, 1);
		VectorCopy(newimpactnormal, trace->plane.normal);
	}
}

// NOTE: start and end brush pair must have same numplanes/numpoints
void Collision_TraceLineBrushFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end)
{
	int nplane, fstartsolid, fendsolid, brushsolid;
	float enterfrac, leavefrac, d1, d2, f, newimpactnormal[3];
	const colplanef_t *startplane, *endplane;

	enterfrac = -1;
	leavefrac = 1;
	fstartsolid = true;
	fendsolid = true;

	for (nplane = 0;nplane < thatbrush_start->numplanes;nplane++)
	{
		startplane = thatbrush_start->planes + nplane;
		endplane = thatbrush_end->planes + nplane;
		d1 = DotProduct(startplane->normal, linestart) - startplane->dist;
		d2 = DotProduct(endplane->normal, lineend) - endplane->dist - COLLISIONEPSILON2;
		if (developer.integer)
		{
			// any brush with degenerate planes is not worth handling
			if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
			{
				Con_Printf("Collision_TraceLineBrushFloat: degenerate plane!\n");
				return;
			}
			if (thatbrush_start->numpoints)
			{
				f = furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
				if (fabs(f - startplane->dist) > 0.01f)
					Con_Printf("startplane->dist %f != calculated %f\n", startplane->dist, f);
			}
		}

		f = d1 - d2;
		if (f >= 0)
		{
			// moving into brush
			if (d2 > 0)
				return;
			if (d1 < 0)
				continue;
			// enter
			fstartsolid = false;
			f = (d1 - COLLISIONEPSILON) / f;
			f = bound(0, f, 1);
			if (enterfrac < f)
			{
				enterfrac = f;
				VectorLerp(startplane->normal, enterfrac, endplane->normal, newimpactnormal);
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return;
			if (d2 < 0)
				continue;
			// leave
			fendsolid = false;
			f = (d1 + COLLISIONEPSILON) / f;
			f = bound(0, f, 1);
			if (leavefrac > f)
				leavefrac = f;
		}
	}

	brushsolid = trace->hitsupercontentsmask & thatbrush_start->supercontents;
	if (fstartsolid)
	{
		trace->startsupercontents |= thatbrush_start->supercontents;
		if (brushsolid)
		{
			trace->startsolid = true;
			if (fendsolid)
				trace->allsolid = true;
		}
	}

	// LordHavoc: we need an epsilon nudge here because for a point trace the
	// penetrating line segment is normally zero length if this brush was
	// generated from a polygon (infinitely thin), and could even be slightly
	// positive or negative due to rounding errors in that case.
	if (brushsolid && enterfrac > -1 && enterfrac < trace->fraction && enterfrac - (1.0f / 1024.0f) <= leavefrac)
	{
		trace->fraction = bound(0, enterfrac, 1);
		VectorCopy(newimpactnormal, trace->plane.normal);
	}
}

static colpointf_t polyf_points[256];
static colplanef_t polyf_planes[256 + 2];
static colbrushf_t polyf_brush;

void Collision_TraceBrushPolygonFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, int supercontents)
{
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	polyf_brush.points = (colpointf_t *)points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
	//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush);
}

void Collision_TraceBrushTriangleMeshFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i;
	float facemins[3], facemaxs[3];
	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	for (i = 0;i < numtriangles;i++, element3i += 3)
	{
		VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
		VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
		VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
		facemins[0] = min(polyf_points[0].v[0], min(polyf_points[1].v[0], polyf_points[2].v[0]));
		facemins[1] = min(polyf_points[0].v[1], min(polyf_points[1].v[1], polyf_points[2].v[1]));
		facemins[2] = min(polyf_points[0].v[2], min(polyf_points[1].v[2], polyf_points[2].v[2]));
		facemaxs[0] = max(polyf_points[0].v[0], max(polyf_points[1].v[0], polyf_points[2].v[0]));
		facemaxs[1] = max(polyf_points[0].v[1], max(polyf_points[1].v[1], polyf_points[2].v[1]));
		facemaxs[2] = max(polyf_points[0].v[2], max(polyf_points[1].v[2], polyf_points[2].v[2]));
		if (BoxesOverlap(segmentmins, segmentmaxs, facemins, facemaxs))
		{
			Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
			//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
			Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush);
		}
	}
}

void Collision_TraceLinePolygonFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numpoints, const float *points, int supercontents)
{
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	polyf_brush.points = (colpointf_t *)points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
	//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
	Collision_TraceLineBrushFloat(trace, linestart, lineend, &polyf_brush, &polyf_brush);
}

void Collision_TraceLineTriangleMeshFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i;
	float facemins[3], facemaxs[3];
	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	for (i = 0;i < numtriangles;i++, element3i += 3)
	{
		VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
		VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
		VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
		facemins[0] = min(polyf_points[0].v[0], min(polyf_points[1].v[0], polyf_points[2].v[0]));
		facemins[1] = min(polyf_points[0].v[1], min(polyf_points[1].v[1], polyf_points[2].v[1]));
		facemins[2] = min(polyf_points[0].v[2], min(polyf_points[1].v[2], polyf_points[2].v[2]));
		facemaxs[0] = max(polyf_points[0].v[0], max(polyf_points[1].v[0], polyf_points[2].v[0]));
		facemaxs[1] = max(polyf_points[0].v[1], max(polyf_points[1].v[1], polyf_points[2].v[1]));
		facemaxs[2] = max(polyf_points[0].v[2], max(polyf_points[1].v[2], polyf_points[2].v[2]));
		if (BoxesOverlap(segmentmins, segmentmaxs, facemins, facemaxs))
		{
			Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
			//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
			Collision_TraceLineBrushFloat(trace, linestart, lineend, &polyf_brush, &polyf_brush);
		}
	}
}


static colpointf_t polyf_pointsstart[256], polyf_pointsend[256];
static colplanef_t polyf_planesstart[256 + 2], polyf_planesend[256 + 2];
static colbrushf_t polyf_brushstart, polyf_brushend;

void Collision_TraceBrushPolygonTransformFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4_t *polygonmatrixstart, const matrix4x4_t *polygonmatrixend, int supercontents)
{
	int i;
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return;
	}
	polyf_brushstart.numpoints = numpoints;
	polyf_brushstart.numplanes = numpoints + 2;
	polyf_brushstart.points = polyf_pointsstart;//(colpointf_t *)points;
	polyf_brushstart.planes = polyf_planesstart;
	polyf_brushstart.supercontents = supercontents;
	for (i = 0;i < numpoints;i++)
		Matrix4x4_Transform(polygonmatrixstart, points + i * 3, polyf_brushstart.points[i].v);
	polyf_brushend.numpoints = numpoints;
	polyf_brushend.numplanes = numpoints + 2;
	polyf_brushend.points = polyf_pointsend;//(colpointf_t *)points;
	polyf_brushend.planes = polyf_planesend;
	polyf_brushend.supercontents = supercontents;
	for (i = 0;i < numpoints;i++)
		Matrix4x4_Transform(polygonmatrixend, points + i * 3, polyf_brushend.points[i].v);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushstart);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushend);

	//Collision_PrintBrushAsQHull(&polyf_brushstart, "polyf_brushstart");
	//Collision_PrintBrushAsQHull(&polyf_brushend, "polyf_brushend");

	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brushstart, &polyf_brushend);
}



#define MAX_BRUSHFORBOX 16
static int brushforbox_index = 0;
static colpointf_t brushforbox_point[MAX_BRUSHFORBOX*8];
static colplanef_t brushforbox_plane[MAX_BRUSHFORBOX*6];
static colbrushf_t brushforbox_brush[MAX_BRUSHFORBOX];
static colbrushf_t brushforpoint_brush[MAX_BRUSHFORBOX];

void Collision_InitBrushForBox(void)
{
	int i;
	for (i = 0;i < MAX_BRUSHFORBOX;i++)
	{
		brushforbox_brush[i].supercontents = SUPERCONTENTS_SOLID;
		brushforbox_brush[i].numpoints = 8;
		brushforbox_brush[i].numplanes = 6;
		brushforbox_brush[i].points = brushforbox_point + i * 8;
		brushforbox_brush[i].planes = brushforbox_plane + i * 6;
		brushforpoint_brush[i].supercontents = SUPERCONTENTS_SOLID;
		brushforpoint_brush[i].numpoints = 1;
		brushforpoint_brush[i].numplanes = 0;
		brushforpoint_brush[i].points = brushforbox_point + i * 8;
		brushforpoint_brush[i].planes = brushforbox_plane + i * 6;
	}
}

colbrushf_t *Collision_BrushForBox(const matrix4x4_t *matrix, const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t v;
	colbrushf_t *brush;
	if (brushforbox_brush[0].numpoints == 0)
		Collision_InitBrushForBox();
	if (VectorCompare(mins, maxs))
	{
		// point brush
		brush = brushforpoint_brush + ((brushforbox_index++) % MAX_BRUSHFORBOX);
		VectorCopy(mins, brush->points->v);
	}
	else
	{
		brush = brushforbox_brush + ((brushforbox_index++) % MAX_BRUSHFORBOX);
		// FIXME: optimize
		for (i = 0;i < 8;i++)
		{
			v[0] = i & 1 ? maxs[0] : mins[0];
			v[1] = i & 2 ? maxs[1] : mins[1];
			v[2] = i & 4 ? maxs[2] : mins[2];
			Matrix4x4_Transform(matrix, v, brush->points[i].v);
		}
		// FIXME: optimize!
		for (i = 0;i < 6;i++)
		{
			VectorClear(v);
			v[i >> 1] = i & 1 ? 1 : -1;
			Matrix4x4_Transform3x3(matrix, v, brush->planes[i].normal);
			VectorNormalize(brush->planes[i].normal);
			brush->planes[i].dist = furthestplanedist_float(brush->planes[i].normal, brush->points, brush->numpoints);
		}
	}
	Collision_ValidateBrush(brush);
	return brush;
}

void Collision_ClipTrace_BrushBox(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask)
{
	colbrushf_t *boxbrush, *thisbrush_start, *thisbrush_end;
	matrix4x4_t identitymatrix;
	vec3_t startmins, startmaxs, endmins, endmaxs;

	// create brushes for the collision
	VectorAdd(start, mins, startmins);
	VectorAdd(start, maxs, startmaxs);
	VectorAdd(end, mins, endmins);
	VectorAdd(end, maxs, endmaxs);
	Matrix4x4_CreateIdentity(&identitymatrix);
	boxbrush = Collision_BrushForBox(&identitymatrix, cmins, cmaxs);
	thisbrush_start = Collision_BrushForBox(&identitymatrix, startmins, startmaxs);
	thisbrush_end = Collision_BrushForBox(&identitymatrix, endmins, endmaxs);

	memset(trace, 0, sizeof(trace_t));
	trace->hitsupercontentsmask = hitsupercontentsmask;
	trace->fraction = 1;
	trace->allsolid = true;
	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, boxbrush, boxbrush);
}

// LordHavoc: currently unused and not yet tested
// note: this can be used for tracing a moving sphere vs a stationary sphere,
// by simply adding the moving sphere's radius to the sphereradius parameter,
// all the results are correct (impactpoint, impactnormal, and fraction)
float Collision_ClipTrace_Line_Sphere(double *linestart, double *lineend, double *sphereorigin, double sphereradius, double *impactpoint, double *impactnormal)
{
	double dir[3], scale, v[3], deviationdist, impactdist, linelength;
	// make sure the impactpoint and impactnormal are valid even if there is
	// no collision
	impactpoint[0] = lineend[0];
	impactpoint[1] = lineend[1];
	impactpoint[2] = lineend[2];
	impactnormal[0] = 0;
	impactnormal[1] = 0;
	impactnormal[2] = 0;
	// calculate line direction
	dir[0] = lineend[0] - linestart[0];
	dir[1] = lineend[1] - linestart[1];
	dir[2] = lineend[2] - linestart[2];
	// normalize direction
	linelength = sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
	if (linelength)
	{
		scale = 1.0 / linelength;
		dir[0] *= scale;
		dir[1] *= scale;
		dir[2] *= scale;
	}
	// this dotproduct calculates the distance along the line at which the
	// sphere origin is (nearest point to the sphere origin on the line)
	impactdist = dir[0] * (sphereorigin[0] - linestart[0]) + dir[1] * (sphereorigin[1] - linestart[1]) + dir[2] * (sphereorigin[2] - linestart[2]);
	// calculate point on line at that distance, and subtract the
	// sphereorigin from it, so we have a vector to measure for the distance
	// of the line from the sphereorigin (deviation, how off-center it is)
	v[0] = linestart[0] + impactdist * dir[0] - sphereorigin[0];
	v[1] = linestart[1] + impactdist * dir[1] - sphereorigin[1];
	v[2] = linestart[2] + impactdist * dir[2] - sphereorigin[2];
	deviationdist = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	// if outside the radius, it's a miss for sure
	// (we do this comparison using squared radius to avoid a sqrt)
	if (deviationdist > sphereradius*sphereradius)
		return 1; // miss (off to the side)
	// nudge back to find the correct impact distance
	impactdist += (sqrt(deviationdist) - sphereradius);
	if (impactdist >= linelength)
		return 1; // miss (not close enough)
	if (impactdist < 0)
		return 1; // miss (linestart is past or inside sphere)
	// calculate new impactpoint
	impactpoint[0] = linestart[0] + impactdist * dir[0];
	impactpoint[1] = linestart[1] + impactdist * dir[1];
	impactpoint[2] = linestart[2] + impactdist * dir[2];
	// calculate impactnormal (surface normal at point of impact)
	impactnormal[0] = impactpoint[0] - sphereorigin[0];
	impactnormal[1] = impactpoint[1] - sphereorigin[1];
	impactnormal[2] = impactpoint[2] - sphereorigin[2];
	// normalize impactnormal
	scale = impactnormal[0] * impactnormal[0] + impactnormal[1] * impactnormal[1] + impactnormal[2] * impactnormal[2];
	if (scale)
	{
		scale = 1.0 / sqrt(scale);
		impactnormal[0] *= scale;
		impactnormal[1] *= scale;
		impactnormal[2] *= scale;
	}
	// return fraction of movement distance
	return impactdist / linelength;
}

typedef struct colbspnode_s
{
	mplane_t plane;
	struct colbspnode_s *children[2];
	// the node is reallocated or split if max is reached
	int numcolbrushf;
	int maxcolbrushf;
	colbrushf_t **colbrushflist;
	//int numcolbrushd;
	//int maxcolbrushd;
	//colbrushd_t **colbrushdlist;
}
colbspnode_t;

typedef struct colbsp_s
{
	mempool_t *mempool;
	colbspnode_t *nodes;
}
colbsp_t;

colbsp_t *Collision_CreateCollisionBSP(mempool_t *mempool)
{
	colbsp_t *bsp;
	bsp = Mem_Alloc(mempool, sizeof(colbsp_t));
	bsp->mempool = mempool;
	bsp->nodes = Mem_Alloc(bsp->mempool, sizeof(colbspnode_t));
	return bsp;
}

void Collision_FreeCollisionBSPNode(colbspnode_t *node)
{
	if (node->children[0])
		Collision_FreeCollisionBSPNode(node->children[0]);
	if (node->children[1])
		Collision_FreeCollisionBSPNode(node->children[1]);
	while (--node->numcolbrushf)
		Mem_Free(node->colbrushflist[node->numcolbrushf]);
	//while (--node->numcolbrushd)
	//	Mem_Free(node->colbrushdlist[node->numcolbrushd]);
	Mem_Free(node);
}

void Collision_FreeCollisionBSP(colbsp_t *bsp)
{
	Collision_FreeCollisionBSPNode(bsp->nodes);
	Mem_Free(bsp);
}

void Collision_BoundingBoxOfBrushTraceSegment(const colbrushf_t *start, const colbrushf_t *end, vec3_t mins, vec3_t maxs, float startfrac, float endfrac)
{
	int i;
	colpointf_t *ps, *pe;
	float tempstart[3], tempend[3];
	VectorLerp(start->points[0].v, startfrac, end->points[0].v, mins);
	VectorCopy(mins, maxs);
	for (i = 0, ps = start->points, pe = end->points;i < start->numpoints;i++, ps++, pe++)
	{
		VectorLerp(ps->v, startfrac, pe->v, tempstart);
		VectorLerp(ps->v, endfrac, pe->v, tempend);
		mins[0] = min(mins[0], min(tempstart[0], tempend[0]));
		mins[1] = min(mins[1], min(tempstart[1], tempend[1]));
		mins[2] = min(mins[2], min(tempstart[2], tempend[2]));
		maxs[0] = min(maxs[0], min(tempstart[0], tempend[0]));
		maxs[1] = min(maxs[1], min(tempstart[1], tempend[1]));
		maxs[2] = min(maxs[2], min(tempstart[2], tempend[2]));
	}
}

