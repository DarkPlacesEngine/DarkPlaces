
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

	// end - start
	double dist[3];
}
RecursiveHullCheckTraceInfo_t;

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON (0.03125)

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

	if (t1 < 0)
	{
		if (t2 < 0)
		{
			num = node->children[1];
			goto loc0;
		}
		side = 1;
	}
	else
	{
		if (t2 >= 0)
		{
			num = node->children[0];
			goto loc0;
		}
		side = 0;
	}

	// the line intersects, find intersection point
	// LordHavoc: this uses the original trace for maximum accuracy
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

	VectorMA(t->start, t->trace->fraction, t->dist, t->trace->endpos);

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

void Collision_RoundUpToHullSize(const model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs)
{
	vec3_t size;
	const hull_t *hull;

	VectorSubtract(inmaxs, inmins, size);
	if (cmodel->brushq1.ishlbsp)
	{
		if (size[0] < 3)
			hull = &cmodel->brushq1.hulls[0]; // 0x0x0
		else if (size[0] <= 32)
		{
			if (size[2] < 54) // pick the nearest of 36 or 72
				hull = &cmodel->brushq1.hulls[3]; // 32x32x36
			else
				hull = &cmodel->brushq1.hulls[1]; // 32x32x72
		}
		else
			hull = &cmodel->brushq1.hulls[2]; // 64x64x64
	}
	else
	{
		if (size[0] < 3)
			hull = &cmodel->brushq1.hulls[0]; // 0x0x0
		else if (size[0] <= 32)
			hull = &cmodel->brushq1.hulls[1]; // 32x32x56
		else
			hull = &cmodel->brushq1.hulls[2]; // 64x64x88
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

void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
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
	rhc.hull = &box_hull;
	rhc.trace = trace;
	rhc.trace->fraction = 1;
	rhc.trace->allsolid = true;
	VectorCopy(start, rhc.start);
	VectorCopy(end, rhc.end);
	VectorSubtract(rhc.end, rhc.start, rhc.dist);
	RecursiveHullCheck(&rhc, rhc.hull->firstclipnode, 0, 1, rhc.start, rhc.end);
}















void Collision_PrintBrushAsQHull(colbrushf_t *brush, const char *name)
{
	int i;
	Con_Printf("3 %s\n%i\n", name, brush->numpoints);
	for (i = 0;i < brush->numpoints;i++)
		Con_Printf("%g %g %g\n", brush->points[i].v[0], brush->points[i].v[1], brush->points[i].v[2]);
	// FIXME: optimize!
	Con_Printf("4\n%i\n", brush->numplanes);
	for (i = 0;i < brush->numplanes;i++)
		Con_Printf("%g %g %g %g\n", brush->planes[i].normal[0], brush->planes[i].normal[1], brush->planes[i].normal[2], brush->planes[i].dist);
}


colbrushf_t *Collision_AllocBrushFloat(mempool_t *mempool, int numpoints, int numplanes)
{
	colbrushf_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colpointf_t) * numpoints + sizeof(colplanef_t) * numplanes);
	brush->numpoints = numpoints;
	brush->numplanes = numplanes;
	brush->planes = (void *)(brush + 1);
	brush->points = (void *)(brush->planes + brush->numplanes);
	return brush;
}

void Collision_CalcPlanesForPolygonBrushFloat(colbrushf_t *brush)
{
	int i;
	float edge0[3], edge1[3], normal[3], dist, bestdist;
	colpointf_t *p, *p2;

	// choose best surface normal for polygon's plane
	bestdist = 0;
	for (i = 0, p = brush->points + 1;i < brush->numpoints - 2;i++, p++)
	{
		VectorSubtract(p[-1].v, p[0].v, edge0);
		VectorSubtract(p[1].v, p[0].v, edge1);
		CrossProduct(edge0, edge1, normal);
		dist = DotProduct(normal, normal);
		if (i == 0 || bestdist < dist)
		{
			bestdist = dist;
			VectorCopy(normal, brush->planes->normal);
		}
	}

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

#if 1
	// validity check - will be disabled later
	for (i = 0;i < brush->numplanes;i++)
	{
		int j;
		for (j = 0, p = brush->points;j < brush->numpoints;j++, p++)
			if (DotProduct(p->v, brush->planes[i].normal) > brush->planes[i].dist + (1.0 / 32.0))
				Con_Printf("Error in brush plane generation, plane %i\n", i);
	}
#endif
}

colbrushf_t *Collision_AllocBrushFromPermanentPolygonFloat(mempool_t *mempool, int numpoints, float *points)
{
	colbrushf_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colplanef_t) * (numpoints + 2));
	brush->numpoints = numpoints;
	brush->numplanes = numpoints + 2;
	brush->planes = (void *)(brush + 1);
	brush->points = (colpointf_t *)points;
	return brush;
}

float nearestplanedist_float(const float *normal, const colpointf_t *points, int numpoints)
{
	float dist, bestdist;
	bestdist = DotProduct(points->v, normal);
	points++;
	while(--numpoints)
	{
		dist = DotProduct(points->v, normal);
		if (bestdist > dist)
			bestdist = dist;
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
		if (bestdist < dist)
			bestdist = dist;
		points++;
	}
	return bestdist;
}

#define COLLISIONEPSILON (1.0f / 32.0f)
#define COLLISIONEPSILON2 0//(1.0f / 32.0f)

// NOTE: start and end of each brush pair must have same numplanes/numpoints
float Collision_TraceBrushBrushFloat(const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end, float *impactnormal, int *startsolid, int *allsolid)
{
	int nplane, nplane2, fstartsolid, fendsolid;
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
		}
		else
		{
			startplane = thatbrush_start->planes + nplane2;
			endplane = thatbrush_end->planes + nplane2;
		}
		d1 = nearestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
		d2 = nearestplanedist_float(endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - furthestplanedist_float(endplane->normal, thatbrush_end->points, thatbrush_end->numpoints) - COLLISIONEPSILON2;
		//Con_Printf("%c%i: d1 = %f, d2 = %f, d1 / (d1 - d2) = %f\n", nplane2 != nplane ? 'b' : 'a', nplane2, d1, d2, d1 / (d1 - d2));

		f = d1 - d2;
		if (f >= 0)
		{
			// moving into brush
			if (d2 > 0)
				return 1;
			if (d1 < 0)
				continue;
			// enter
			fstartsolid = false;
			f = (d1 - COLLISIONEPSILON) / f;
			f = bound(0, f, 1);
			if (enterfrac < f)
			{
				enterfrac = f;
				VectorBlend(startplane->normal, endplane->normal, enterfrac, newimpactnormal);
			}
		}
		else if (f < 0)
		{
			// moving out of brush
			if (d1 > 0)
				return 1;
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

	if (fstartsolid)
	{
		if (startsolid)
			*startsolid = true;
		if (fendsolid && allsolid)
			*allsolid = true;
	}

	// LordHavoc: we need an epsilon nudge here because for a point trace the
	// penetrating line segment is normally zero length if this brush was
	// generated from a polygon (infinitely thin), and could even be slightly
	// positive or negative due to rounding errors in that case.
	if (enterfrac > -1 && enterfrac < 1 && enterfrac - (1.0f / 1024.0f) <= leavefrac)
	{
		//if (enterfrac < (1.0f / 1024.0f))
		//	enterfrac = 0;
		enterfrac = bound(0, enterfrac, 1);
		VectorCopy(newimpactnormal, impactnormal);
		return enterfrac;
	}
	return 1;
}

static colplanef_t polyf_planes[256 + 2];
static colbrushf_t polyf_brush;

float Collision_TraceBrushPolygonFloat(const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, float *impactnormal, int *startsolid, int *allsolid)
{
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return 1;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	polyf_brush.points = (colpointf_t *)points;
	polyf_brush.planes = polyf_planes;
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
	//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
	return Collision_TraceBrushBrushFloat(thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush, impactnormal, startsolid, allsolid);
}

static colpointf_t polyf_pointsstart[256], polyf_pointsend[256];
static colplanef_t polyf_planesstart[256 + 2], polyf_planesend[256 + 2];
static colbrushf_t polyf_brushstart, polyf_brushend;

float Collision_TraceBrushPolygonTransformFloat(const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, float *impactnormal, const matrix4x4_t *polygonmatrixstart, const matrix4x4_t *polygonmatrixend, int *startsolid, int *allsolid)
{
	int i;
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return 1;
	}
	polyf_brushstart.numpoints = numpoints;
	polyf_brushstart.numplanes = numpoints + 2;
	polyf_brushstart.points = polyf_pointsstart;//(colpointf_t *)points;
	polyf_brushstart.planes = polyf_planesstart;
	for (i = 0;i < numpoints;i++)
		Matrix4x4_Transform(polygonmatrixstart, points + i * 3, polyf_brushstart.points[i].v);
	polyf_brushend.numpoints = numpoints;
	polyf_brushend.numplanes = numpoints + 2;
	polyf_brushend.points = polyf_pointsend;//(colpointf_t *)points;
	polyf_brushend.planes = polyf_planesend;
	for (i = 0;i < numpoints;i++)
		Matrix4x4_Transform(polygonmatrixend, points + i * 3, polyf_brushend.points[i].v);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushstart);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushend);

	//Collision_PrintBrushAsQHull(&polyf_brushstart, "polyf_brushstart");
	//Collision_PrintBrushAsQHull(&polyf_brushend, "polyf_brushend");

	return Collision_TraceBrushBrushFloat(thisbrush_start, thisbrush_end, &polyf_brushstart, &polyf_brushend, impactnormal, startsolid, allsolid);
}

colbrushd_t *Collision_AllocBrushDouble(mempool_t *mempool, int numpoints, int numplanes)
{
	colbrushd_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushd_t) + sizeof(colpointd_t) * numpoints + sizeof(colplaned_t) * numplanes);
	brush->numpoints = numpoints;
	brush->numplanes = numplanes;
	brush->planes = (void *)(brush + 1);
	brush->points = (void *)(brush->planes + brush->numplanes);
	return brush;
}

void Collision_CalcPlanesForPolygonBrushDouble(colbrushd_t *brush)
{
	int i;
	double edge0[3], edge1[3], normal[3], dist, bestdist;
	colpointd_t *p, *p2;

	// choose best surface normal for polygon's plane
	bestdist = 0;
	for (i = 2, p = brush->points + 2;i < brush->numpoints;i++, p++)
	{
		VectorSubtract(p[-1].v, p[0].v, edge0);
		VectorSubtract(p[1].v, p[0].v, edge1);
		CrossProduct(edge0, edge1, normal);
		dist = DotProduct(normal, normal);
		if (i == 2 || bestdist < dist)
		{
			bestdist = dist;
			VectorCopy(normal, brush->planes->normal);
		}
	}

	VectorNormalize(brush->planes->normal);
	brush->planes->dist = DotProduct(brush->points->v, brush->planes->normal);

	// negate plane to create other side
	VectorNegate(brush->planes[0].normal, brush->planes[1].normal);
	brush->planes[1].dist = -brush->planes[0].dist;
	for (i = 0, p = brush->points + (brush->numpoints - 1), p2 = brush->points + 2;i < brush->numpoints;i++, p = p2, p2++)
	{
		VectorSubtract(p->v, p2->v, edge0);
		CrossProduct(edge0, brush->planes->normal, brush->planes[i].normal);
		VectorNormalize(brush->planes[i].normal);
		brush->planes[i].dist = DotProduct(p->v, brush->planes[i].normal);
	}

#if 1
	// validity check - will be disabled later
	for (i = 0;i < brush->numplanes;i++)
	{
		int j;
		for (j = 0, p = brush->points;j < brush->numpoints;j++, p++)
			if (DotProduct(p->v, brush->planes[i].normal) > brush->planes[i].dist + (1.0 / 32.0))
				Con_Printf("Error in brush plane generation, plane %i\n");
	}
#endif
}

colbrushd_t *Collision_AllocBrushFromPermanentPolygonDouble(mempool_t *mempool, int numpoints, double *points)
{
	colbrushd_t *brush;
	brush = Mem_Alloc(mempool, sizeof(colbrushd_t) + sizeof(colplaned_t) * (numpoints + 2));
	brush->numpoints = numpoints;
	brush->numplanes = numpoints + 2;
	brush->planes = (void *)(brush + 1);
	brush->points = (colpointd_t *)points;
	return brush;
}


double nearestplanedist_double(const double *normal, const colpointd_t *points, int numpoints)
{
	double dist, bestdist;
	bestdist = DotProduct(points->v, normal);
	points++;
	while(--numpoints)
	{
		dist = DotProduct(points->v, normal);
		if (bestdist > dist)
			bestdist = dist;
		points++;
	}
	return bestdist;
}

double furthestplanedist_double(const double *normal, const colpointd_t *points, int numpoints)
{
	double dist, bestdist;
	bestdist = DotProduct(points->v, normal);
	points++;
	while(--numpoints)
	{
		dist = DotProduct(points->v, normal);
		if (bestdist < dist)
			bestdist = dist;
		points++;
	}
	return bestdist;
}

// NOTE: start and end of each brush pair must have same numplanes/numpoints
double Collision_TraceBrushBrushDouble(const colbrushd_t *thisbrush_start, const colbrushd_t *thisbrush_end, const colbrushd_t *thatbrush_start, const colbrushd_t *thatbrush_end, double *impactnormal)
{
	int nplane;
	double enterfrac, leavefrac, d1, d2, f, newimpactnormal[3];
	const colplaned_t *startplane, *endplane;

	enterfrac = -1;
	leavefrac = 1;

	for (nplane = 0;nplane < thatbrush_start->numplanes;nplane++)
	{
		startplane = thatbrush_start->planes + nplane;
		endplane = thatbrush_end->planes + nplane;
		d1 = nearestplanedist_double(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - furthestplanedist_double(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
		d2 = nearestplanedist_double(endplane->normal, thisbrush_end->points, thisbrush_start->numpoints) - furthestplanedist_double(endplane->normal, thatbrush_end->points, thatbrush_start->numpoints) - (1.0 / 32.0);

		f = d1 - d2;
		if (f >= 0)
		{
			// moving into brush
			if (d2 > 0)
				return 1;
			if (d1 < 0)
				continue;
			// enter
			f = d1 / f;
			if (enterfrac < f)
			{
				enterfrac = f;
				VectorSubtract(endplane->normal, startplane->normal, newimpactnormal);
				VectorMA(startplane->normal, enterfrac, impactnormal, newimpactnormal);
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return 1;
			if (d2 < 0)
				continue;
			// leave
			f = d1 / f;
			if (leavefrac > f)
				leavefrac = f;
		}
	}

	for (nplane = 0;nplane < thisbrush_start->numplanes;nplane++)
	{
		startplane = thisbrush_start->planes + nplane;
		endplane = thisbrush_end->planes + nplane;
		d1 = nearestplanedist_double(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - furthestplanedist_double(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
		d2 = nearestplanedist_double(endplane->normal, thisbrush_end->points, thisbrush_start->numpoints) - furthestplanedist_double(endplane->normal, thatbrush_end->points, thatbrush_start->numpoints) - (1.0 / 32.0);

		f = d1 - d2;
		if (f >= 0)
		{
			// moving into brush
			if (d2 > 0)
				return 1;
			if (d1 < 0)
				continue;
			// enter
			f = d1 / f;
			if (enterfrac < f)
			{
				enterfrac = f;
				VectorSubtract(endplane->normal, startplane->normal, newimpactnormal);
				VectorMA(startplane->normal, enterfrac, impactnormal, newimpactnormal);
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return 1;
			if (d2 < 0)
				continue;
			// leave
			f = d1 / f;
			if (leavefrac > f)
				leavefrac = f;
		}
	}

	// LordHavoc: we need an epsilon nudge here because for a point trace the
	// penetrating line segment is normally zero length if this brush was
	// generated from a polygon (infinitely thin), and could even be slightly
	// positive or negative due to rounding errors in that case.
	enterfrac -= (1.0 / 16384.0);
	if (leavefrac - enterfrac >= 0 && enterfrac > -1)
	{
		VectorCopy(newimpactnormal, impactnormal);
		enterfrac = bound(0, enterfrac, 1);
		return enterfrac;
	}
	return 1;
}

static colplaned_t polyd_planes[256 + 2];
static colbrushd_t polyd_brush;
double Collision_TraceBrushPolygonDouble(const colbrushd_t *thisbrush_start, const colbrushd_t *thisbrush_end, int numpoints, const double *points, double *impactnormal)
{
	if (numpoints > 256)
	{
		Con_Printf("Polygon with more than 256 points not supported yet (fixme!)\n");
		return 1;
	}
	polyd_brush.numpoints = numpoints;
	polyd_brush.numplanes = numpoints + 2;
	polyd_brush.points = (colpointd_t *)points;
	polyd_brush.planes = polyd_planes;
	Collision_CalcPlanesForPolygonBrushDouble(&polyd_brush);
	return Collision_TraceBrushBrushDouble(thisbrush_start, thisbrush_end, &polyd_brush, &polyd_brush, impactnormal);
}




typedef struct colbrushbmodelinfo_s
{
	model_t *model;
	const matrix4x4_t *modelmatrixstart;
	const matrix4x4_t *modelmatrixend;
	const colbrushf_t *thisbrush_start;
	const colbrushf_t *thisbrush_end;
	float impactnormal[3];
	float tempimpactnormal[3];
	float fraction;
	int startsolid;
	int allsolid;
}
colbrushbmodelinfo_t;

static int colframecount = 1;

void Collision_RecursiveTraceBrushNode(colbrushbmodelinfo_t *info, mnode_t *node)
{
	if (node->contents)
	{
		// collide with surfaces marked by this leaf
		int i, *mark;
		float result;
		mleaf_t *leaf = (mleaf_t *)node;
		msurface_t *surf;
		for (i = 0, mark = leaf->firstmarksurface;i < leaf->nummarksurfaces;i++, mark++)
		{
			surf = info->model->brushq1.surfaces + *mark;
			// don't check a surface twice
			if (surf->colframe != colframecount)
			{
				surf->colframe = colframecount;
				if (surf->flags & SURF_SOLIDCLIP)
				{
					result = Collision_TraceBrushPolygonFloat(info->thisbrush_start, info->thisbrush_end, surf->poly_numverts, surf->poly_verts, info->tempimpactnormal, &info->startsolid, &info->allsolid);
					//result = Collision_TraceBrushPolygonTransformFloat(info->thisbrush_start, info->thisbrush_end, surf->poly_numverts, surf->poly_verts, info->tempimpactnormal, info->modelmatrixstart, info->modelmatrixend, &info->startsolid, &info->allsolid);
					if (info->fraction > result)
					{
						info->fraction = result;
						// use the surface's plane instead of the actual
						// collision plane because the actual collision plane
						// might be to the side (on a seam between polygons)
						// or something, we want objects to bounce off the
						// front...
						//if (surf->flags & SURF_PLANEBACK)
						//	VectorNegate(surf->plane->normal, info->impactnormal);
						//else
						//	VectorCopy(surf->plane->normal, info->impactnormal);
						VectorCopy(info->tempimpactnormal, info->impactnormal);
					}
				}
			}
		}
	}
	else
	{
		// recurse down node sides
		int i, bits;
		float dist1, dist2;
		colpointf_t *ps, *pe;
		bits = 0;
		// FIXME? if TraceBrushPolygonTransform were to be made usable, the
		// node planes would need to be transformed too
		dist1 = node->plane->dist - (1.0f / 8.0f);
		dist2 = node->plane->dist + (1.0f / 8.0f);
		for (i = 0, ps = info->thisbrush_start->points, pe = info->thisbrush_end->points;i < info->thisbrush_start->numpoints;i++, ps++, pe++)
		{
			if (!(bits & 1) && (DotProduct(ps->v, node->plane->normal) > dist1 || DotProduct(pe->v, node->plane->normal) > dist1))
				bits |= 1;
			if (!(bits & 2) && (DotProduct(ps->v, node->plane->normal) < dist2 || DotProduct(pe->v, node->plane->normal) < dist2))
				bits |= 2;
		}
		if (bits & 1)
			Collision_RecursiveTraceBrushNode(info, node->children[0]);
		if (bits & 2)
			Collision_RecursiveTraceBrushNode(info, node->children[1]);
	}
}

float Collision_TraceBrushBModel(const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, model_t *model, float *impactnormal, int *startsolid, int *allsolid)
{
	colbrushbmodelinfo_t info;
	colframecount++;
	info.model = model;
	info.thisbrush_start = thisbrush_start;
	info.thisbrush_end = thisbrush_end;
	info.fraction = 1;
	info.startsolid = false;
	info.allsolid = false;
	Collision_RecursiveTraceBrushNode(&info, model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode);
	if (info.fraction < 1)
		VectorCopy(info.impactnormal, impactnormal);
	if (startsolid)
		*startsolid = info.startsolid;
	if (allsolid)
		*allsolid = info.allsolid;
	return info.fraction;
}

float Collision_TraceBrushBModelTransform(const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, model_t *model, float *impactnormal, const matrix4x4_t *modelmatrixstart, const matrix4x4_t *modelmatrixend, int *startsolid, int *allsolid)
{
	colbrushbmodelinfo_t info;
	colframecount++;
	info.model = model;
	info.modelmatrixstart = modelmatrixstart;
	info.modelmatrixend = modelmatrixend;
	info.thisbrush_start = thisbrush_start;
	info.thisbrush_end = thisbrush_end;
	info.fraction = 1;
	info.startsolid = false;
	info.allsolid = false;
	Collision_RecursiveTraceBrushNode(&info, model->brushq1.nodes);
	if (info.fraction < 1)
		VectorCopy(info.impactnormal, impactnormal);
	if (startsolid)
		*startsolid = info.startsolid;
	if (allsolid)
		*allsolid = info.allsolid;
	return info.fraction;
}



#define MAX_BRUSHFORBOX 16
static int brushforbox_index = 0;
static colpointf_t brushforbox_point[MAX_BRUSHFORBOX*8];
static colplanef_t brushforbox_plane[MAX_BRUSHFORBOX*6];
static colbrushf_t brushforbox_brush[MAX_BRUSHFORBOX];

void Collision_InitBrushForBox(void)
{
	int i;
	for (i = 0;i < MAX_BRUSHFORBOX;i++)
	{
		brushforbox_brush[i].numpoints = 8;
		brushforbox_brush[i].numplanes = 6;
		brushforbox_brush[i].points = brushforbox_point + i * 8;
		brushforbox_brush[i].planes = brushforbox_plane + i * 6;
	}
}

colbrushf_t *Collision_BrushForBox(const matrix4x4_t *matrix, const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t v;
	colbrushf_t *brush;
	if (brushforbox_brush[0].numpoints == 0)
		Collision_InitBrushForBox();
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
	return brush;
}

void Collision_PolygonClipTrace (trace_t *trace, const void *cent, model_t *cmodel, const vec3_t corigin, const vec3_t cangles, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
	vec3_t impactnormal;
	//vec3_t mins2, maxs2;
	matrix4x4_t cmatrix, cimatrix, startmatrix, endmatrix;
	matrix4x4_t mstartmatrix, mendmatrix, identitymatrix;
	colbrushf_t *thisbrush_start, *thisbrush_end, *cbrush;

	Matrix4x4_CreateFromQuakeEntity(&cmatrix, corigin[0], corigin[1], corigin[2], cangles[0], cangles[1], cangles[2], 1);
	Matrix4x4_Invert_Simple(&cimatrix, &cmatrix);
	Matrix4x4_CreateTranslate(&startmatrix, start[0], start[1], start[2]);
	Matrix4x4_CreateTranslate(&endmatrix, end[0], end[1], end[2]);

	Matrix4x4_CreateIdentity(&identitymatrix);
	Matrix4x4_Concat(&mstartmatrix, &cimatrix, &startmatrix);
	Matrix4x4_Concat(&mendmatrix, &cimatrix, &endmatrix);
	thisbrush_start = Collision_BrushForBox(&mstartmatrix, mins, maxs);
	//mins2[0] = mins[0] - 0.0625;mins2[1] = mins[1] - 0.0625;mins2[2] = mins[2] - 0.0625;
	//maxs2[0] = maxs[0] + 0.0625;maxs2[1] = maxs[1] + 0.0625;maxs2[2] = maxs[2] + 0.0625;
	thisbrush_end = Collision_BrushForBox(&mendmatrix, mins, maxs);

	//Collision_PrintBrushAsQHull(thisbrush_start, "thisbrush_start");
	//Collision_PrintBrushAsQHull(thisbrush_end, "thisbrush_end");
	memset (trace, 0, sizeof(trace_t));
	if (cmodel && cmodel->type == mod_brush)
	{
		// brush model
		trace->fraction = Collision_TraceBrushBModel(thisbrush_start, thisbrush_end, cmodel, impactnormal, &trace->startsolid, &trace->allsolid);
		//trace->fraction = Collision_TraceBrushBModelTransform(thisbrush_start, thisbrush_end, cmodel, trace->plane.normal, &cmatrix, &cmatrix, &trace->startsolid, &trace->allsolid);
	}
	else
	{
		// bounding box
		cbrush = Collision_BrushForBox(&identitymatrix, cmins, cmaxs);
		trace->fraction = Collision_TraceBrushBrushFloat(thisbrush_start, thisbrush_end, cbrush, cbrush, impactnormal, &trace->startsolid, &trace->allsolid);
		//cbrush = Collision_BrushForBox(&cmatrix, cmins, cmaxs);
		//trace->fraction = Collision_TraceBrushBrushFloat(thisbrush_start, thisbrush_end, cbrush, cbrush, trace->plane.normal, &trace->startsolid, &trace->allsolid);
	}

	if (trace->fraction < 0 || trace->fraction > 1)
		Con_Printf("fraction out of bounds %f %s:%d\n", trace->fraction, __FILE__, __LINE__);

	if (trace->fraction < 1)
	{
		trace->ent = (void *) cent;
		VectorBlend(start, end, trace->fraction, trace->endpos);
		Matrix4x4_Transform(&cmatrix, impactnormal, trace->plane.normal);
		VectorNormalize(trace->plane.normal);
		//Con_Printf("fraction %f normal %f %f %f\n", trace->fraction, trace->plane.normal[0], trace->plane.normal[1], trace->plane.normal[2]);
	}
	else
		VectorCopy(end, trace->endpos);
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

