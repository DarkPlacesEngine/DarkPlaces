
#include "quakedef.h"
#include "polygon.h"

#define COLLISION_SNAPSCALE (32.0f)
#define COLLISION_SNAP (1.0f / COLLISION_SNAPSCALE)
#define COLLISION_SNAP2 (2.0f / COLLISION_SNAPSCALE)
#define COLLISION_PLANE_DIST_EPSILON (2.0f / COLLISION_SNAPSCALE)

cvar_t collision_impactnudge = {0, "collision_impactnudge", "0.03125", "how much to back off from the impact"};
cvar_t collision_startnudge = {0, "collision_startnudge", "0", "how much to bias collision trace start"};
cvar_t collision_endnudge = {0, "collision_endnudge", "0", "how much to bias collision trace end"};
cvar_t collision_enternudge = {0, "collision_enternudge", "0", "how much to bias collision entry fraction"};
cvar_t collision_leavenudge = {0, "collision_leavenudge", "0", "how much to bias collision exit fraction"};
cvar_t collision_prefernudgedfraction = {0, "collision_prefernudgedfraction", "1", "whether to sort collision events by nudged fraction (1) or real fraction (0)"};

void Collision_Init (void)
{
	Cvar_RegisterVariable(&collision_impactnudge);
	Cvar_RegisterVariable(&collision_startnudge);
	Cvar_RegisterVariable(&collision_endnudge);
	Cvar_RegisterVariable(&collision_enternudge);
	Cvar_RegisterVariable(&collision_leavenudge);
	Cvar_RegisterVariable(&collision_prefernudgedfraction);
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
	int j, k, pointsoffplanes, pointonplanes, pointswithinsufficientplanes, printbrush;
	float d;
	printbrush = false;
	if (!brush->numpoints)
	{
		Con_Print("Collision_ValidateBrush: brush with no points!\n");
		printbrush = true;
	}
#if 0
	// it's ok for a brush to have one point and no planes...
	if (brush->numplanes == 0 && brush->numpoints != 1)
	{
		Con_Print("Collision_ValidateBrush: brush with no planes and more than one point!\n");
		printbrush = true;
	}
#endif
	if (brush->numplanes)
	{
		pointsoffplanes = 0;
		pointswithinsufficientplanes = 0;
		for (k = 0;k < brush->numplanes;k++)
			if (DotProduct(brush->planes[k].normal, brush->planes[k].normal) < 0.0001f)
				Con_Printf("Collision_ValidateBrush: plane #%i (%f %f %f %f) is degenerate\n", k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist);
		for (j = 0;j < brush->numpoints;j++)
		{
			pointonplanes = 0;
			for (k = 0;k < brush->numplanes;k++)
			{
				d = DotProduct(brush->points[j].v, brush->planes[k].normal) - brush->planes[k].dist;
				if (d > COLLISION_PLANE_DIST_EPSILON)
				{
					Con_Printf("Collision_ValidateBrush: point #%i (%f %f %f) infront of plane #%i (%f %f %f %f)\n", j, brush->points[j].v[0], brush->points[j].v[1], brush->points[j].v[2], k, brush->planes[k].normal[0], brush->planes[k].normal[1], brush->planes[k].normal[2], brush->planes[k].dist);
					printbrush = true;
				}
				if (fabs(d) > COLLISION_PLANE_DIST_EPSILON)
					pointsoffplanes++;
				else
					pointonplanes++;
			}
			if (pointonplanes < 3)
				pointswithinsufficientplanes++;
		}
		if (pointswithinsufficientplanes)
		{
			Con_Print("Collision_ValidateBrush: some points have insufficient planes, every point must be on at least 3 planes to form a corner.\n");
			printbrush = true;
		}
		if (pointsoffplanes == 0) // all points are on all planes
		{
			Con_Print("Collision_ValidateBrush: all points lie on all planes (degenerate, no brush volume!)\n");
			printbrush = true;
		}
	}
	if (printbrush)
		Collision_PrintBrushAsQHull(brush, "unnamed");
}

float nearestplanedist_float(const float *normal, const colpointf_t *points, int numpoints)
{
	float dist, bestdist;
	if (!numpoints)
		return 0;
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
	if (!numpoints)
		return 0;
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


colbrushf_t *Collision_NewBrushFromPlanes(mempool_t *mempool, int numoriginalplanes, const colplanef_t *originalplanes, int supercontents)
{
	// TODO: planesbuf could be replaced by a remapping table
	int j, k, m, w, xyzflags;
	int numpointsbuf = 0, maxpointsbuf = 256, numplanesbuf = 0, maxplanesbuf = 256, numelementsbuf = 0, maxelementsbuf = 256;
	double maxdist;
	colbrushf_t *brush;
	colpointf_t pointsbuf[256];
	colplanef_t planesbuf[256];
	int elementsbuf[1024];
	int polypointbuf[256];
	int pmaxpoints = 64;
	int pnumpoints;
	double p[2][3*64];
#if 0
	// enable these if debugging to avoid seeing garbage in unused data
	memset(pointsbuf, 0, sizeof(pointsbuf));
	memset(planesbuf, 0, sizeof(planesbuf));
	memset(elementsbuf, 0, sizeof(elementsbuf));
	memset(polypointbuf, 0, sizeof(polypointbuf));
	memset(p, 0, sizeof(p));
#endif
	// figure out how large a bounding box we need to properly compute this brush
	maxdist = 0;
	for (j = 0;j < numoriginalplanes;j++)
		maxdist = max(maxdist, fabs(originalplanes[j].dist));
	// now make it large enough to enclose the entire brush, and round it off to a reasonable multiple of 1024
	maxdist = floor(maxdist * (4.0 / 1024.0) + 2) * 1024.0;
	// construct a collision brush (points, planes, and renderable mesh) from
	// a set of planes, this also optimizes out any unnecessary planes (ones
	// whose polygon is clipped away by the other planes)
	for (j = 0;j < numoriginalplanes;j++)
	{
		// add the plane uniquely (no duplicates)
		for (k = 0;k < numplanesbuf;k++)
			if (VectorCompare(planesbuf[k].normal, originalplanes[j].normal) && planesbuf[k].dist == originalplanes[j].dist)
				break;
		// if the plane is a duplicate, skip it
		if (k < numplanesbuf)
			continue;
		// check if there are too many and skip the brush
		if (numplanesbuf >= maxplanesbuf)
		{
			Con_DPrint("Collision_NewBrushFromPlanes: failed to build collision brush: too many planes for buffer\n");
			return NULL;
		}

		// add the new plane
		VectorCopy(originalplanes[j].normal, planesbuf[numplanesbuf].normal);
		planesbuf[numplanesbuf].dist = originalplanes[j].dist;
		planesbuf[numplanesbuf].q3surfaceflags = originalplanes[j].q3surfaceflags;
		planesbuf[numplanesbuf].texture = originalplanes[j].texture;
		numplanesbuf++;

		// create a large polygon from the plane
		w = 0;
		PolygonD_QuadForPlane(p[w], originalplanes[j].normal[0], originalplanes[j].normal[1], originalplanes[j].normal[2], originalplanes[j].dist, maxdist);
		pnumpoints = 4;
		// clip it by all other planes
		for (k = 0;k < numoriginalplanes && pnumpoints >= 3 && pnumpoints <= pmaxpoints;k++)
		{
			// skip the plane this polygon
			// (nothing happens if it is processed, this is just an optimization)
			if (k != j)
			{
				// we want to keep the inside of the brush plane so we flip
				// the cutting plane
				PolygonD_Divide(pnumpoints, p[w], -originalplanes[k].normal[0], -originalplanes[k].normal[1], -originalplanes[k].normal[2], -originalplanes[k].dist, COLLISION_PLANE_DIST_EPSILON, pmaxpoints, p[!w], &pnumpoints, 0, NULL, NULL, NULL);
				w = !w;
			}
		}

		// if nothing is left, skip it
		if (pnumpoints < 3)
		{
			//Con_DPrintf("Collision_NewBrushFromPlanes: warning: polygon for plane %f %f %f %f clipped away\n", originalplanes[j].normal[0], originalplanes[j].normal[1], originalplanes[j].normal[2], originalplanes[j].dist);
			continue;
		}

		for (k = 0;k < pnumpoints;k++)
		{
			int l, m;
			m = 0;
			for (l = 0;l < numoriginalplanes;l++)
				if (fabs(DotProduct(&p[w][k*3], originalplanes[l].normal) - originalplanes[l].dist) < COLLISION_PLANE_DIST_EPSILON)
					m++;
			if (m < 3)
				break;
		}
		if (k < pnumpoints)
		{
			Con_DPrintf("Collision_NewBrushFromPlanes: warning: polygon point does not lie on at least 3 planes\n");
			//return NULL;
		}

		// check if there are too many polygon vertices for buffer
		if (pnumpoints > pmaxpoints)
		{
			Con_DPrint("Collision_NewBrushFromPlanes: failed to build collision brush: too many points for buffer\n");
			return NULL;
		}

		// check if there are too many triangle elements for buffer
		if (numelementsbuf + (pnumpoints - 2) * 3 > maxelementsbuf)
		{
			Con_DPrint("Collision_NewBrushFromPlanes: failed to build collision brush: too many triangle elements for buffer\n");
			return NULL;
		}

		for (k = 0;k < pnumpoints;k++)
		{
			float v[3];
			// downgrade to float precision before comparing
			VectorCopy(&p[w][k*3], v);

			// check if there is already a matching point (no duplicates)
			for (m = 0;m < numpointsbuf;m++)
				if (VectorDistance2(v, pointsbuf[m].v) < COLLISION_SNAP2)
					break;

			// if there is no match, add a new one
			if (m == numpointsbuf)
			{
				// check if there are too many and skip the brush
				if (numpointsbuf >= maxpointsbuf)
				{
					Con_DPrint("Collision_NewBrushFromPlanes: failed to build collision brush: too many points for buffer\n");
					return NULL;
				}
				// add the new one
				VectorCopy(&p[w][k*3], pointsbuf[numpointsbuf].v);
				numpointsbuf++;
			}

			// store the index into a buffer
			polypointbuf[k] = m;
		}

		// add the triangles for the polygon
		// (this particular code makes a triangle fan)
		for (k = 0;k < pnumpoints - 2;k++)
		{
			elementsbuf[numelementsbuf++] = polypointbuf[0];
			elementsbuf[numelementsbuf++] = polypointbuf[k + 1];
			elementsbuf[numelementsbuf++] = polypointbuf[k + 2];
		}
	}

	// if nothing is left, there's nothing to allocate
	if (numplanesbuf < 4)
	{
		Con_DPrintf("Collision_NewBrushFromPlanes: failed to build collision brush: %i triangles, %i planes (input was %i planes), %i vertices\n", numelementsbuf / 3, numplanesbuf, numoriginalplanes, numpointsbuf);
		return NULL;
	}

	// if no triangles or points could be constructed, then this routine failed but the brush is not discarded
	if (numelementsbuf < 12 || numpointsbuf < 4)
		Con_DPrintf("Collision_NewBrushFromPlanes: unable to rebuild triangles/points for collision brush: %i triangles, %i planes (input was %i planes), %i vertices\n", numelementsbuf / 3, numplanesbuf, numoriginalplanes, numpointsbuf);

	// validate plane distances
	for (j = 0;j < numplanesbuf;j++)
	{
		float d = furthestplanedist_float(planesbuf[j].normal, pointsbuf, numpointsbuf);
		if (fabs(planesbuf[j].dist - d) > COLLISION_PLANE_DIST_EPSILON)
			Con_DPrintf("plane %f %f %f %f mismatches dist %f\n", planesbuf[j].normal[0], planesbuf[j].normal[1], planesbuf[j].normal[2], planesbuf[j].dist, d);
	}

	// allocate the brush and copy to it
	brush = (colbrushf_t *)Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colpointf_t) * numpointsbuf + sizeof(colplanef_t) * numplanesbuf + sizeof(int) * numelementsbuf);
	brush->supercontents = supercontents;
	brush->numplanes = numplanesbuf;
	brush->numpoints = numpointsbuf;
	brush->numtriangles = numelementsbuf / 3;
	brush->planes = (colplanef_t *)(brush + 1);
	brush->points = (colpointf_t *)(brush->planes + brush->numplanes);
	brush->elements = (int *)(brush->points + brush->numpoints);
	for (j = 0;j < brush->numpoints;j++)
	{
		brush->points[j].v[0] = pointsbuf[j].v[0];
		brush->points[j].v[1] = pointsbuf[j].v[1];
		brush->points[j].v[2] = pointsbuf[j].v[2];
	}
	for (j = 0;j < brush->numplanes;j++)
	{
		brush->planes[j].normal[0] = planesbuf[j].normal[0];
		brush->planes[j].normal[1] = planesbuf[j].normal[1];
		brush->planes[j].normal[2] = planesbuf[j].normal[2];
		brush->planes[j].dist = planesbuf[j].dist;
		brush->planes[j].q3surfaceflags = planesbuf[j].q3surfaceflags;
		brush->planes[j].texture = planesbuf[j].texture;
	}
	for (j = 0;j < brush->numtriangles * 3;j++)
		brush->elements[j] = elementsbuf[j];

	xyzflags = 0;
	VectorClear(brush->mins);
	VectorClear(brush->maxs);
	for (j = 0;j < min(6, numoriginalplanes);j++)
	{
		     if (originalplanes[j].normal[0] ==  1) {xyzflags |=  1;brush->maxs[0] =  originalplanes[j].dist;}
		else if (originalplanes[j].normal[0] == -1) {xyzflags |=  2;brush->mins[0] = -originalplanes[j].dist;}
		else if (originalplanes[j].normal[1] ==  1) {xyzflags |=  4;brush->maxs[1] =  originalplanes[j].dist;}
		else if (originalplanes[j].normal[1] == -1) {xyzflags |=  8;brush->mins[1] = -originalplanes[j].dist;}
		else if (originalplanes[j].normal[2] ==  1) {xyzflags |= 16;brush->maxs[2] =  originalplanes[j].dist;}
		else if (originalplanes[j].normal[2] == -1) {xyzflags |= 32;brush->mins[2] = -originalplanes[j].dist;}
	}
	// if not all xyzflags were set, then this is not a brush from q3map/q3map2, and needs reconstruction of the bounding box
	// (this case works for any brush with valid points, but sometimes brushes are not reconstructed properly and hence the points are not valid, so this is reserved as a fallback case)
	if (xyzflags != 63)
	{
		VectorCopy(brush->points[0].v, brush->mins);
		VectorCopy(brush->points[0].v, brush->maxs);
		for (j = 1;j < brush->numpoints;j++)
		{
			brush->mins[0] = min(brush->mins[0], brush->points[j].v[0]);
			brush->mins[1] = min(brush->mins[1], brush->points[j].v[1]);
			brush->mins[2] = min(brush->mins[2], brush->points[j].v[2]);
			brush->maxs[0] = max(brush->maxs[0], brush->points[j].v[0]);
			brush->maxs[1] = max(brush->maxs[1], brush->points[j].v[1]);
			brush->maxs[2] = max(brush->maxs[2], brush->points[j].v[2]);
		}
	}
	brush->mins[0] -= 1;
	brush->mins[1] -= 1;
	brush->mins[2] -= 1;
	brush->maxs[0] += 1;
	brush->maxs[1] += 1;
	brush->maxs[2] += 1;
	Collision_ValidateBrush(brush);
	return brush;
}



void Collision_CalcPlanesForPolygonBrushFloat(colbrushf_t *brush)
{
	int i;
	float edge0[3], edge1[3], edge2[3], normal[3], dist, bestdist;
	colpointf_t *p, *p2;

	// FIXME: these probably don't actually need to be normalized if the collision code does not care
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
#if 1
			{
				float projectionnormal[3], projectionedge0[3], projectionedge1[3], projectionedge2[3];
				int i, best;
				float dist, bestdist;
				bestdist = fabs(brush->planes[0].normal[0]);
				best = 0;
				for (i = 1;i < 3;i++)
				{
					dist = fabs(brush->planes[0].normal[i]);
					if (bestdist < dist)
					{
						bestdist = dist;
						best = i;
					}
				}
				VectorClear(projectionnormal);
				if (brush->planes[0].normal[best] < 0)
					projectionnormal[best] = -1;
				else
					projectionnormal[best] = 1;
				VectorCopy(edge0, projectionedge0);
				VectorCopy(edge1, projectionedge1);
				VectorCopy(edge2, projectionedge2);
				projectionedge0[best] = 0;
				projectionedge1[best] = 0;
				projectionedge2[best] = 0;
				CrossProduct(projectionedge0, projectionnormal, brush->planes[2].normal);
				CrossProduct(projectionedge1, projectionnormal, brush->planes[3].normal);
				CrossProduct(projectionedge2, projectionnormal, brush->planes[4].normal);
			}
#else
			CrossProduct(edge0, brush->planes->normal, brush->planes[2].normal);
			CrossProduct(edge1, brush->planes->normal, brush->planes[3].normal);
			CrossProduct(edge2, brush->planes->normal, brush->planes[4].normal);
#endif
			VectorNormalize(brush->planes[2].normal);
			VectorNormalize(brush->planes[3].normal);
			VectorNormalize(brush->planes[4].normal);
			brush->planes[2].dist = DotProduct(brush->points[2].v, brush->planes[2].normal);
			brush->planes[3].dist = DotProduct(brush->points[0].v, brush->planes[3].normal);
			brush->planes[4].dist = DotProduct(brush->points[1].v, brush->planes[4].normal);

			if (developer.integer >= 100)
			{
				// validation code
#if 0
				float temp[3];

				VectorSubtract(brush->points[0].v, brush->points[1].v, edge0);
				VectorSubtract(brush->points[2].v, brush->points[1].v, edge1);
				CrossProduct(edge0, edge1, normal);
				VectorNormalize(normal);
				VectorSubtract(normal, brush->planes[0].normal, temp);
				if (VectorLength(temp) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: TriangleNormal gave wrong answer (%f %f %f != correct answer %f %f %f)\n", brush->planes->normal[0], brush->planes->normal[1], brush->planes->normal[2], normal[0], normal[1], normal[2]);
				if (fabs(DotProduct(brush->planes[1].normal, brush->planes[0].normal) - -1.0f) > 0.01f || fabs(brush->planes[1].dist - -brush->planes[0].dist) > 0.01f)
					Con_Printf("Collision_CalcPlanesForPolygonBrushFloat: plane 1 (%f %f %f %f) is not opposite plane 0 (%f %f %f %f)\n", brush->planes[1].normal[0], brush->planes[1].normal[1], brush->planes[1].normal[2], brush->planes[1].dist, brush->planes[0].normal[0], brush->planes[0].normal[1], brush->planes[0].normal[2], brush->planes[0].dist);
#if 0
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
#endif
#endif
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

	if (developer.integer >= 100)
	{
		// validity check - will be disabled later
		Collision_ValidateBrush(brush);
		for (i = 0;i < brush->numplanes;i++)
		{
			int j;
			for (j = 0, p = brush->points;j < brush->numpoints;j++, p++)
				if (DotProduct(p->v, brush->planes[i].normal) > brush->planes[i].dist + COLLISION_PLANE_DIST_EPSILON)
					Con_Printf("Error in brush plane generation, plane %i\n", i);
		}
	}
}

colbrushf_t *Collision_AllocBrushFromPermanentPolygonFloat(mempool_t *mempool, int numpoints, float *points, int supercontents)
{
	colbrushf_t *brush;
	brush = (colbrushf_t *)Mem_Alloc(mempool, sizeof(colbrushf_t) + sizeof(colplanef_t) * (numpoints + 2));
	brush->supercontents = supercontents;
	brush->numpoints = numpoints;
	brush->numplanes = numpoints + 2;
	brush->planes = (colplanef_t *)(brush + 1);
	brush->points = (colpointf_t *)points;
	Sys_Error("Collision_AllocBrushFromPermanentPolygonFloat: FIXME: this code needs to be updated to generate a mesh...");
	return brush;
}

// NOTE: start and end of each brush pair must have same numplanes/numpoints
void Collision_TraceBrushBrushFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end)
{
	int nplane, nplane2, hitq3surfaceflags = 0;
	float enterfrac = -1, leavefrac = 1, d1, d2, s, e, ie, f, imove, enterfrac2 = -1;
	const colplanef_t *startplane, *endplane;
	plane_t newimpactplane;
	texture_t *hittexture = NULL;

	VectorClear(newimpactplane.normal);
	newimpactplane.dist = 0;

	for (nplane = 0;nplane < thatbrush_start->numplanes + thisbrush_start->numplanes;nplane++)
	{
		nplane2 = nplane;
		if (nplane2 >= thatbrush_start->numplanes)
		{
			nplane2 -= thatbrush_start->numplanes;
			startplane = thisbrush_start->planes + nplane2;
			endplane = thisbrush_end->planes + nplane2;
			if (developer.integer >= 100)
			{
				// any brush with degenerate planes is not worth handling
				if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
				{
					Con_Print("Collision_TraceBrushBrushFloat: degenerate thisbrush plane!\n");
					return;
				}
				f = furthestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints);
				if (fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON)
					Con_Printf("startplane->dist %f != calculated %f (thisbrush_start)\n", startplane->dist, f);
			}
			s = furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
			e = furthestplanedist_float(endplane->normal, thatbrush_end->points, thatbrush_end->numpoints);
			d1 = nearestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - s - collision_startnudge.value;
			d2 = nearestplanedist_float(endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - e - collision_endnudge.value;
		}
		else
		{
			startplane = thatbrush_start->planes + nplane2;
			endplane = thatbrush_end->planes + nplane2;
			if (developer.integer >= 100)
			{
				// any brush with degenerate planes is not worth handling
				if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
				{
					Con_Print("Collision_TraceBrushBrushFloat: degenerate thatbrush plane!\n");
					return;
				}
				f = furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
				if (fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON)
					Con_Printf("startplane->dist %f != calculated %f (thatbrush_start)\n", startplane->dist, f);
			}
			s = startplane->dist;
			e = endplane->dist;
			d1 = nearestplanedist_float(startplane->normal, thisbrush_start->points, thisbrush_start->numpoints) - s - collision_startnudge.value;
			d2 = nearestplanedist_float(endplane->normal, thisbrush_end->points, thisbrush_end->numpoints) - e - collision_endnudge.value;
		}
		//Con_Printf("%c%i: d1 = %f, d2 = %f, d1 / (d1 - d2) = %f\n", nplane2 != nplane ? 'b' : 'a', nplane2, d1, d2, d1 / (d1 - d2));

		if (d1 > d2)
		{
			// moving into brush
			if (d2 >= collision_enternudge.value)
				return;
			if (d1 > 0)
			{
				// enter
				imove = 1 / (d1 - d2);
				f = (d1 - collision_enternudge.value) * imove;
				if (f < 0)
					f = 0;
				// check if this will reduce the collision time range
				if (enterfrac < f)
				{
					// reduced collision time range
					enterfrac = f;
					// if the collision time range is now empty, no collision
					if (enterfrac > leavefrac)
						return;
					// if the collision would be further away than the trace's
					// existing collision data, we don't care about this
					// collision
					if (enterfrac > trace->realfraction)
						return;
					// calculate the nudged fraction and impact normal we'll
					// need if we accept this collision later
					enterfrac2 = (d1 - collision_impactnudge.value) * imove;
					ie = 1.0f - enterfrac;
					newimpactplane.normal[0] = startplane->normal[0] * ie + endplane->normal[0] * enterfrac;
					newimpactplane.normal[1] = startplane->normal[1] * ie + endplane->normal[1] * enterfrac;
					newimpactplane.normal[2] = startplane->normal[2] * ie + endplane->normal[2] * enterfrac;
					newimpactplane.dist      = s                     * ie + e                   * enterfrac;
					hitq3surfaceflags = startplane->q3surfaceflags;
					hittexture = startplane->texture;
				}
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return;
			if (d2 > 0)
			{
				// leave
				f = (d1 + collision_leavenudge.value) / (d1 - d2);
				if (f > 1)
					f = 1;
				// check if this will reduce the collision time range
				if (leavefrac > f)
				{
					// reduced collision time range
					leavefrac = f;
					// if the collision time range is now empty, no collision
					if (enterfrac > leavefrac)
						return;
				}
			}
		}
	}

	// at this point we know the trace overlaps the brush because it was not
	// rejected at any point in the loop above

	// see if the trace started outside the brush or not
	if (enterfrac > -1)
	{
		// started outside, and overlaps, therefore there is a collision here
		// store out the impact information
		if (trace->hitsupercontentsmask & thatbrush_start->supercontents)
		{
			trace->hitsupercontents = thatbrush_start->supercontents;
			trace->hitq3surfaceflags = hitq3surfaceflags;
			trace->hittexture = hittexture;
			trace->realfraction = bound(0, enterfrac, 1);
			trace->fraction = bound(0, enterfrac2, 1);
			if (collision_prefernudgedfraction.integer)
				trace->realfraction = trace->fraction;
			trace->plane = newimpactplane;
		}
	}
	else
	{
		// started inside, update startsolid and friends
		trace->startsupercontents |= thatbrush_start->supercontents;
		if (trace->hitsupercontentsmask & thatbrush_start->supercontents)
		{
			trace->startsolid = true;
			if (leavefrac < 1)
				trace->allsolid = true;
			trace->plane = newimpactplane;
		}
	}
}

// NOTE: start and end brush pair must have same numplanes/numpoints
void Collision_TraceLineBrushFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end)
{
	int nplane, hitq3surfaceflags = 0;
	float enterfrac = -1, leavefrac = 1, d1, d2, ie, f, imove, enterfrac2 = -1;
	const colplanef_t *startplane, *endplane;
	plane_t newimpactplane;
	texture_t *hittexture = NULL;

	VectorClear(newimpactplane.normal);
	newimpactplane.dist = 0;

	for (nplane = 0;nplane < thatbrush_start->numplanes;nplane++)
	{
		startplane = thatbrush_start->planes + nplane;
		endplane = thatbrush_end->planes + nplane;
		d1 = DotProduct(startplane->normal, linestart) - startplane->dist - collision_startnudge.value;
		d2 = DotProduct(endplane->normal, lineend) - endplane->dist - collision_endnudge.value;
		if (developer.integer >= 100)
		{
			// any brush with degenerate planes is not worth handling
			if (DotProduct(startplane->normal, startplane->normal) < 0.9f || DotProduct(endplane->normal, endplane->normal) < 0.9f)
			{
				Con_Print("Collision_TraceLineBrushFloat: degenerate plane!\n");
				return;
			}
			if (thatbrush_start->numpoints)
			{
				f = furthestplanedist_float(startplane->normal, thatbrush_start->points, thatbrush_start->numpoints);
				if (fabs(f - startplane->dist) > COLLISION_PLANE_DIST_EPSILON)
					Con_Printf("startplane->dist %f != calculated %f\n", startplane->dist, f);
			}
		}

		if (d1 > d2)
		{
			// moving into brush
			if (d2 >= collision_enternudge.value)
				return;
			if (d1 > 0)
			{
				// enter
				imove = 1 / (d1 - d2);
				f = (d1 - collision_enternudge.value) * imove;
				if (f < 0)
					f = 0;
				// check if this will reduce the collision time range
				if (enterfrac < f)
				{
					// reduced collision time range
					enterfrac = f;
					// if the collision time range is now empty, no collision
					if (enterfrac > leavefrac)
						return;
					// if the collision would be further away than the trace's
					// existing collision data, we don't care about this
					// collision
					if (enterfrac > trace->realfraction)
						return;
					// calculate the nudged fraction and impact normal we'll
					// need if we accept this collision later
					enterfrac2 = (d1 - collision_impactnudge.value) * imove;
					ie = 1.0f - enterfrac;
					newimpactplane.normal[0] = startplane->normal[0] * ie + endplane->normal[0] * enterfrac;
					newimpactplane.normal[1] = startplane->normal[1] * ie + endplane->normal[1] * enterfrac;
					newimpactplane.normal[2] = startplane->normal[2] * ie + endplane->normal[2] * enterfrac;
					newimpactplane.dist      = startplane->dist      * ie + endplane->dist      * enterfrac;
					hitq3surfaceflags = startplane->q3surfaceflags;
					hittexture = startplane->texture;
				}
			}
		}
		else
		{
			// moving out of brush
			if (d1 > 0)
				return;
			if (d2 > 0)
			{
				// leave
				f = (d1 + collision_leavenudge.value) / (d1 - d2);
				// check if this will reduce the collision time range
				if (leavefrac > f)
				{
					// reduced collision time range
					leavefrac = f;
					// if the collision time range is now empty, no collision
					if (enterfrac > leavefrac)
						return;
				}
			}
		}
	}

	// at this point we know the trace overlaps the brush because it was not
	// rejected at any point in the loop above

	// see if the trace started outside the brush or not
	if (enterfrac > -1)
	{
		// started outside, and overlaps, therefore there is a collision here
		// store out the impact information
		if (trace->hitsupercontentsmask & thatbrush_start->supercontents)
		{
			trace->hitsupercontents = thatbrush_start->supercontents;
			trace->hitq3surfaceflags = hitq3surfaceflags;
			trace->hittexture = hittexture;
			trace->realfraction = bound(0, enterfrac, 1);
			trace->fraction = bound(0, enterfrac2, 1);
			if (collision_prefernudgedfraction.integer)
				trace->realfraction = trace->fraction;
			trace->plane = newimpactplane;
		}
	}
	else
	{
		// started inside, update startsolid and friends
		trace->startsupercontents |= thatbrush_start->supercontents;
		if (trace->hitsupercontentsmask & thatbrush_start->supercontents)
		{
			trace->startsolid = true;
			if (leavefrac < 1)
				trace->allsolid = true;
			trace->plane = newimpactplane;
		}
	}
}

qboolean Collision_PointInsideBrushFloat(const vec3_t point, const colbrushf_t *brush)
{
	int nplane;
	const colplanef_t *plane;

	if (!BoxesOverlap(point, point, brush->mins, brush->maxs))
		return false;
	for (nplane = 0, plane = brush->planes;nplane < brush->numplanes;nplane++, plane++)
		if (DotProduct(plane->normal, point) > plane->dist)
			return false;
	return true;
}

void Collision_TracePointBrushFloat(trace_t *trace, const vec3_t point, const colbrushf_t *thatbrush)
{
	if (!Collision_PointInsideBrushFloat(point, thatbrush))
		return;

	trace->startsupercontents |= thatbrush->supercontents;
	if (trace->hitsupercontentsmask & thatbrush->supercontents)
	{
		trace->startsolid = true;
		trace->allsolid = true;
	}
}

static colpointf_t polyf_points[256];
static colplanef_t polyf_planes[256 + 2];
static colbrushf_t polyf_brush;

void Collision_SnapCopyPoints(int numpoints, const colpointf_t *in, colpointf_t *out, float fractionprecision, float invfractionprecision)
{
	int i;
	for (i = 0;i < numpoints;i++)
	{
		out[i].v[0] = floor(in[i].v[0] * fractionprecision + 0.5f) * invfractionprecision;
		out[i].v[1] = floor(in[i].v[1] * fractionprecision + 0.5f) * invfractionprecision;
		out[i].v[2] = floor(in[i].v[2] * fractionprecision + 0.5f) * invfractionprecision;
	}
}

void Collision_TraceBrushPolygonFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, int supercontents)
{
	if (numpoints > 256)
	{
		Con_Print("Polygon with more than 256 points not supported yet (fixme!)\n");
		return;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	//polyf_brush.points = (colpointf_t *)points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	polyf_brush.points = polyf_points;
	Collision_SnapCopyPoints(polyf_brush.numpoints, (colpointf_t *)points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
	//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush);
}

void Collision_TraceBrushTriangleMeshFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numtriangles, const int *element3i, const float *vertex3f, int stride, float *bbox6f, int supercontents, int q3surfaceflags, texture_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i;
	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	for (i = 0;i < polyf_brush.numplanes;i++)
	{
		polyf_brush.planes[i].q3surfaceflags = q3surfaceflags;
		polyf_brush.planes[i].texture = texture;
	}
	if(stride)
	{
		int k, cnt, tri;
		cnt = (numtriangles + stride - 1) / stride;
		for(i = 0; i < cnt; ++i)
		{
			if(BoxesOverlap(bbox6f + i * 6, bbox6f + i * 6 + 3, segmentmins, segmentmaxs))
			{
				for(k = 0; k < stride; ++k)
				{
					tri = i * stride + k;
					if(tri >= numtriangles)
						break;
					VectorCopy(vertex3f + element3i[tri * 3 + 0] * 3, polyf_points[0].v);
					VectorCopy(vertex3f + element3i[tri * 3 + 1] * 3, polyf_points[1].v);
					VectorCopy(vertex3f + element3i[tri * 3 + 2] * 3, polyf_points[2].v);
					Collision_SnapCopyPoints(polyf_brush.numpoints, polyf_points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP);
					Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
					//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
					Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush);
				}
			}
		}
	}
	else
	{
		for (i = 0;i < numtriangles;i++, element3i += 3)
		{
			if (TriangleOverlapsBox(vertex3f + element3i[0]*3, vertex3f + element3i[1]*3, vertex3f + element3i[2]*3, segmentmins, segmentmaxs))
			{
				VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
				VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
				VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
				Collision_SnapCopyPoints(polyf_brush.numpoints, polyf_points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP);
				Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
				//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
				Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brush, &polyf_brush);
			}
		}
	}
}

void Collision_TraceLinePolygonFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numpoints, const float *points, int supercontents)
{
	if (numpoints > 256)
	{
		Con_Print("Polygon with more than 256 points not supported yet (fixme!)\n");
		return;
	}
	polyf_brush.numpoints = numpoints;
	polyf_brush.numplanes = numpoints + 2;
	//polyf_brush.points = (colpointf_t *)points;
	polyf_brush.points = polyf_points;
	Collision_SnapCopyPoints(polyf_brush.numpoints, (colpointf_t *)points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP);
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
	//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
	Collision_TraceLineBrushFloat(trace, linestart, lineend, &polyf_brush, &polyf_brush);
}

void Collision_TraceLineTriangleMeshFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numtriangles, const int *element3i, const float *vertex3f, int stride, float *bbox6f, int supercontents, int q3surfaceflags, texture_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs)
{
	int i;
#if 1
	// FIXME: snap vertices?
	if(stride)
	{
		int k, cnt, tri;
		cnt = (numtriangles + stride - 1) / stride;
		for(i = 0; i < cnt; ++i)
		{
			if(BoxesOverlap(bbox6f + i * 6, bbox6f + i * 6 + 3, segmentmins, segmentmaxs))
			{
				for(k = 0; k < stride; ++k)
				{
					tri = i * stride + k;
					if(tri >= numtriangles)
						break;
					Collision_TraceLineTriangleFloat(trace, linestart, lineend, vertex3f + element3i[tri * 3 + 0] * 3, vertex3f + element3i[tri * 3 + 1] * 3, vertex3f + element3i[tri * 3 + 2] * 3, supercontents, q3surfaceflags, texture);
				}
			}
		}
	}
	else
	{
		for (i = 0;i < numtriangles;i++, element3i += 3)
			Collision_TraceLineTriangleFloat(trace, linestart, lineend, vertex3f + element3i[0] * 3, vertex3f + element3i[1] * 3, vertex3f + element3i[2] * 3, supercontents, q3surfaceflags, texture);
	}
#else
	polyf_brush.numpoints = 3;
	polyf_brush.numplanes = 5;
	polyf_brush.points = polyf_points;
	polyf_brush.planes = polyf_planes;
	polyf_brush.supercontents = supercontents;
	for (i = 0;i < polyf_brush.numplanes;i++)
	{
		polyf_brush.planes[i].supercontents = supercontents;
		polyf_brush.planes[i].q3surfaceflags = q3surfaceflags;
		polyf_brush.planes[i].texture = texture;
	}
	for (i = 0;i < numtriangles;i++, element3i += 3)
	{
		if (TriangleOverlapsBox(vertex3f + element3i[0]*3, vertex3 + [element3i[1]*3, vertex3f + element3i[2]*3, segmentmins, segmentmaxs))
		{
			VectorCopy(vertex3f + element3i[0] * 3, polyf_points[0].v);
			VectorCopy(vertex3f + element3i[1] * 3, polyf_points[1].v);
			VectorCopy(vertex3f + element3i[2] * 3, polyf_points[2].v);
			Collision_SnapCopyPoints(polyf_brush.numpoints, polyf_points, polyf_points, COLLISION_SNAPSCALE, COLLISION_SNAP);
			Collision_CalcPlanesForPolygonBrushFloat(&polyf_brush);
			//Collision_PrintBrushAsQHull(&polyf_brush, "polyf_brush");
			Collision_TraceLineBrushFloat(trace, linestart, lineend, &polyf_brush, &polyf_brush);
		}
	}
#endif
}


static colpointf_t polyf_pointsstart[256], polyf_pointsend[256];
static colplanef_t polyf_planesstart[256 + 2], polyf_planesend[256 + 2];
static colbrushf_t polyf_brushstart, polyf_brushend;

void Collision_TraceBrushPolygonTransformFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4_t *polygonmatrixstart, const matrix4x4_t *polygonmatrixend, int supercontents, int q3surfaceflags, texture_t *texture)
{
	int i;
	if (numpoints > 256)
	{
		Con_Print("Polygon with more than 256 points not supported yet (fixme!)\n");
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
	for (i = 0;i < polyf_brushstart.numplanes;i++)
	{
		polyf_brushstart.planes[i].q3surfaceflags = q3surfaceflags;
		polyf_brushstart.planes[i].texture = texture;
	}
	Collision_SnapCopyPoints(polyf_brushstart.numpoints, polyf_pointsstart, polyf_pointsstart, COLLISION_SNAPSCALE, COLLISION_SNAP);
	Collision_SnapCopyPoints(polyf_brushend.numpoints, polyf_pointsend, polyf_pointsend, COLLISION_SNAPSCALE, COLLISION_SNAP);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushstart);
	Collision_CalcPlanesForPolygonBrushFloat(&polyf_brushend);

	//Collision_PrintBrushAsQHull(&polyf_brushstart, "polyf_brushstart");
	//Collision_PrintBrushAsQHull(&polyf_brushend, "polyf_brushend");

	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, &polyf_brushstart, &polyf_brushend);
}



#define MAX_BRUSHFORBOX 16
static unsigned int brushforbox_index = 0;
// note: this relies on integer overflow to be consistent with modulo
// MAX_BRUSHFORBOX, or in other words, MAX_BRUSHFORBOX must be a power of two!
static colpointf_t brushforbox_point[MAX_BRUSHFORBOX*8];
static colplanef_t brushforbox_plane[MAX_BRUSHFORBOX*6];
static colbrushf_t brushforbox_brush[MAX_BRUSHFORBOX];
static colbrushf_t brushforpoint_brush[MAX_BRUSHFORBOX];

void Collision_InitBrushForBox(void)
{
	int i;
	for (i = 0;i < MAX_BRUSHFORBOX;i++)
	{
		brushforbox_brush[i].numpoints = 8;
		brushforbox_brush[i].numplanes = 6;
		brushforbox_brush[i].points = brushforbox_point + i * 8;
		brushforbox_brush[i].planes = brushforbox_plane + i * 6;
		brushforpoint_brush[i].numpoints = 1;
		brushforpoint_brush[i].numplanes = 0;
		brushforpoint_brush[i].points = brushforbox_point + i * 8;
		brushforpoint_brush[i].planes = brushforbox_plane + i * 6;
	}
}

colbrushf_t *Collision_BrushForBox(const matrix4x4_t *matrix, const vec3_t mins, const vec3_t maxs, int supercontents, int q3surfaceflags, texture_t *texture)
{
	int i, j;
	vec3_t v;
	colbrushf_t *brush;
	if (brushforbox_brush[0].numpoints == 0)
		Collision_InitBrushForBox();
	// FIXME: these probably don't actually need to be normalized if the collision code does not care
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
		}
	}
	brush->supercontents = supercontents;
	for (j = 0;j < brush->numplanes;j++)
	{
		brush->planes[j].q3surfaceflags = q3surfaceflags;
		brush->planes[j].texture = texture;
		brush->planes[j].dist = furthestplanedist_float(brush->planes[j].normal, brush->points, brush->numpoints);
	}
	VectorCopy(brush->points[0].v, brush->mins);
	VectorCopy(brush->points[0].v, brush->maxs);
	for (j = 1;j < brush->numpoints;j++)
	{
		brush->mins[0] = min(brush->mins[0], brush->points[j].v[0]);
		brush->mins[1] = min(brush->mins[1], brush->points[j].v[1]);
		brush->mins[2] = min(brush->mins[2], brush->points[j].v[2]);
		brush->maxs[0] = max(brush->maxs[0], brush->points[j].v[0]);
		brush->maxs[1] = max(brush->maxs[1], brush->points[j].v[1]);
		brush->maxs[2] = max(brush->maxs[2], brush->points[j].v[2]);
	}
	brush->mins[0] -= 1;
	brush->mins[1] -= 1;
	brush->mins[2] -= 1;
	brush->maxs[0] += 1;
	brush->maxs[1] += 1;
	brush->maxs[2] += 1;
	Collision_ValidateBrush(brush);
	return brush;
}

void Collision_ClipTrace_BrushBox(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int supercontents, int q3surfaceflags, texture_t *texture)
{
	colbrushf_t *boxbrush, *thisbrush_start, *thisbrush_end;
	vec3_t startmins, startmaxs, endmins, endmaxs;

	// create brushes for the collision
	VectorAdd(start, mins, startmins);
	VectorAdd(start, maxs, startmaxs);
	VectorAdd(end, mins, endmins);
	VectorAdd(end, maxs, endmaxs);
	boxbrush = Collision_BrushForBox(&identitymatrix, cmins, cmaxs, supercontents, q3surfaceflags, texture);
	thisbrush_start = Collision_BrushForBox(&identitymatrix, startmins, startmaxs, 0, 0, NULL);
	thisbrush_end = Collision_BrushForBox(&identitymatrix, endmins, endmaxs, 0, 0, NULL);

	memset(trace, 0, sizeof(trace_t));
	trace->hitsupercontentsmask = hitsupercontentsmask;
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->allsolid = true;
	Collision_TraceBrushBrushFloat(trace, thisbrush_start, thisbrush_end, boxbrush, boxbrush);
}

//pseudocode for detecting line/sphere overlap without calculating an impact point
//linesphereorigin = sphereorigin - linestart;linediff = lineend - linestart;linespherefrac = DotProduct(linesphereorigin, linediff) / DotProduct(linediff, linediff);return VectorLength2(linesphereorigin - bound(0, linespherefrac, 1) * linediff) >= sphereradius*sphereradius;

// LordHavoc: currently unused, but tested
// note: this can be used for tracing a moving sphere vs a stationary sphere,
// by simply adding the moving sphere's radius to the sphereradius parameter,
// all the results are correct (impactpoint, impactnormal, and fraction)
float Collision_ClipTrace_Line_Sphere(double *linestart, double *lineend, double *sphereorigin, double sphereradius, double *impactpoint, double *impactnormal)
{
	double dir[3], scale, v[3], deviationdist, impactdist, linelength;
	// make sure the impactpoint and impactnormal are valid even if there is
	// no collision
	VectorCopy(lineend, impactpoint);
	VectorClear(impactnormal);
	// calculate line direction
	VectorSubtract(lineend, linestart, dir);
	// normalize direction
	linelength = VectorLength(dir);
	if (linelength)
	{
		scale = 1.0 / linelength;
		VectorScale(dir, scale, dir);
	}
	// this dotproduct calculates the distance along the line at which the
	// sphere origin is (nearest point to the sphere origin on the line)
	impactdist = DotProduct(sphereorigin, dir) - DotProduct(linestart, dir);
	// calculate point on line at that distance, and subtract the
	// sphereorigin from it, so we have a vector to measure for the distance
	// of the line from the sphereorigin (deviation, how off-center it is)
	VectorMA(linestart, impactdist, dir, v);
	VectorSubtract(v, sphereorigin, v);
	deviationdist = VectorLength2(v);
	// if outside the radius, it's a miss for sure
	// (we do this comparison using squared radius to avoid a sqrt)
	if (deviationdist > sphereradius*sphereradius)
		return 1; // miss (off to the side)
	// nudge back to find the correct impact distance
	impactdist -= sphereradius - deviationdist/sphereradius;
	if (impactdist >= linelength)
		return 1; // miss (not close enough)
	if (impactdist < 0)
		return 1; // miss (linestart is past or inside sphere)
	// calculate new impactpoint
	VectorMA(linestart, impactdist, dir, impactpoint);
	// calculate impactnormal (surface normal at point of impact)
	VectorSubtract(impactpoint, sphereorigin, impactnormal);
	// normalize impactnormal
	VectorNormalize(impactnormal);
	// return fraction of movement distance
	return impactdist / linelength;
}

void Collision_TraceLineTriangleFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, const float *point0, const float *point1, const float *point2, int supercontents, int q3surfaceflags, texture_t *texture)
{
#if 1
	// more optimized
	float d1, d2, d, f, impact[3], edgenormal[3], faceplanenormal[3], faceplanedist, faceplanenormallength2, edge01[3], edge21[3], edge02[3];

	// this function executes:
	// 32 ops when line starts behind triangle
	// 38 ops when line ends infront of triangle
	// 43 ops when line fraction is already closer than this triangle
	// 72 ops when line is outside edge 01
	// 92 ops when line is outside edge 21
	// 115 ops when line is outside edge 02
	// 123 ops when line impacts triangle and updates trace results

	// this code is designed for clockwise triangles, conversion to
	// counterclockwise would require swapping some things around...
	// it is easier to simply swap the point0 and point2 parameters to this
	// function when calling it than it is to rewire the internals.

	// calculate the faceplanenormal of the triangle, this represents the front side
	// 15 ops
	VectorSubtract(point0, point1, edge01);
	VectorSubtract(point2, point1, edge21);
	CrossProduct(edge01, edge21, faceplanenormal);
	// there's no point in processing a degenerate triangle (GIGO - Garbage In, Garbage Out)
	// 6 ops
	faceplanenormallength2 = DotProduct(faceplanenormal, faceplanenormal);
	if (faceplanenormallength2 < 0.0001f)
		return;
	// calculate the distance
	// 5 ops
	faceplanedist = DotProduct(point0, faceplanenormal);

	// if start point is on the back side there is no collision
	// (we don't care about traces going through the triangle the wrong way)

	// calculate the start distance
	// 6 ops
	d1 = DotProduct(faceplanenormal, linestart);
	if (d1 <= faceplanedist)
		return;

	// calculate the end distance
	// 6 ops
	d2 = DotProduct(faceplanenormal, lineend);
	// if both are in front, there is no collision
	if (d2 >= faceplanedist)
		return;

	// from here on we know d1 is >= 0 and d2 is < 0
	// this means the line starts infront and ends behind, passing through it

	// calculate the recipricol of the distance delta,
	// so we can use it multiple times cheaply (instead of division)
	// 2 ops
	d = 1.0f / (d1 - d2);
	// calculate the impact fraction by taking the start distance (> 0)
	// and subtracting the face plane distance (this is the distance of the
	// triangle along that same normal)
	// then multiply by the recipricol distance delta
	// 2 ops
	f = (d1 - faceplanedist) * d;
	// skip out if this impact is further away than previous ones
	// 1 ops
	if (f > trace->realfraction)
		return;
	// calculate the perfect impact point for classification of insidedness
	// 9 ops
	impact[0] = linestart[0] + f * (lineend[0] - linestart[0]);
	impact[1] = linestart[1] + f * (lineend[1] - linestart[1]);
	impact[2] = linestart[2] + f * (lineend[2] - linestart[2]);

	// calculate the edge normal and reject if impact is outside triangle
	// (an edge normal faces away from the triangle, to get the desired normal
	//  a crossproduct with the faceplanenormal is used, and because of the way
	// the insidedness comparison is written it does not need to be normalized)

	// first use the two edges from the triangle plane math
	// the other edge only gets calculated if the point survives that long

	// 20 ops
	CrossProduct(edge01, faceplanenormal, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point1, edgenormal))
		return;

	// 20 ops
	CrossProduct(faceplanenormal, edge21, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point2, edgenormal))
		return;

	// 23 ops
	VectorSubtract(point0, point2, edge02);
	CrossProduct(faceplanenormal, edge02, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point0, edgenormal))
		return;

	// 8 ops (rare)

	// store the new trace fraction
	trace->realfraction = f;

	// calculate a nudged fraction to keep it out of the surface
	// (the main fraction remains perfect)
	trace->fraction = f - collision_impactnudge.value * d;

	if (collision_prefernudgedfraction.integer)
		trace->realfraction = trace->fraction;

	// store the new trace plane (because collisions only happen from
	// the front this is always simply the triangle normal, never flipped)
	d = 1.0 / sqrt(faceplanenormallength2);
	VectorScale(faceplanenormal, d, trace->plane.normal);
	trace->plane.dist = faceplanedist * d;

	trace->hitsupercontents = supercontents;
	trace->hitq3surfaceflags = q3surfaceflags;
	trace->hittexture = texture;
#else
	float d1, d2, d, f, fnudged, impact[3], edgenormal[3], faceplanenormal[3], faceplanedist, edge[3];

	// this code is designed for clockwise triangles, conversion to
	// counterclockwise would require swapping some things around...
	// it is easier to simply swap the point0 and point2 parameters to this
	// function when calling it than it is to rewire the internals.

	// calculate the unnormalized faceplanenormal of the triangle,
	// this represents the front side
	TriangleNormal(point0, point1, point2, faceplanenormal);
	// there's no point in processing a degenerate triangle
	// (GIGO - Garbage In, Garbage Out)
	if (DotProduct(faceplanenormal, faceplanenormal) < 0.0001f)
		return;
	// calculate the unnormalized distance
	faceplanedist = DotProduct(point0, faceplanenormal);

	// calculate the unnormalized start distance
	d1 = DotProduct(faceplanenormal, linestart) - faceplanedist;
	// if start point is on the back side there is no collision
	// (we don't care about traces going through the triangle the wrong way)
	if (d1 <= 0)
		return;

	// calculate the unnormalized end distance
	d2 = DotProduct(faceplanenormal, lineend) - faceplanedist;
	// if both are in front, there is no collision
	if (d2 >= 0)
		return;

	// from here on we know d1 is >= 0 and d2 is < 0
	// this means the line starts infront and ends behind, passing through it

	// calculate the recipricol of the distance delta,
	// so we can use it multiple times cheaply (instead of division)
	d = 1.0f / (d1 - d2);
	// calculate the impact fraction by taking the start distance (> 0)
	// and subtracting the face plane distance (this is the distance of the
	// triangle along that same normal)
	// then multiply by the recipricol distance delta
	f = d1 * d;
	// skip out if this impact is further away than previous ones
	if (f > trace->realfraction)
		return;
	// calculate the perfect impact point for classification of insidedness
	impact[0] = linestart[0] + f * (lineend[0] - linestart[0]);
	impact[1] = linestart[1] + f * (lineend[1] - linestart[1]);
	impact[2] = linestart[2] + f * (lineend[2] - linestart[2]);

	// calculate the edge normal and reject if impact is outside triangle
	// (an edge normal faces away from the triangle, to get the desired normal
	//  a crossproduct with the faceplanenormal is used, and because of the way
	// the insidedness comparison is written it does not need to be normalized)

	VectorSubtract(point2, point0, edge);
	CrossProduct(edge, faceplanenormal, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point0, edgenormal))
		return;

	VectorSubtract(point0, point1, edge);
	CrossProduct(edge, faceplanenormal, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point1, edgenormal))
		return;

	VectorSubtract(point1, point2, edge);
	CrossProduct(edge, faceplanenormal, edgenormal);
	if (DotProduct(impact, edgenormal) > DotProduct(point2, edgenormal))
		return;

	// store the new trace fraction
	trace->realfraction = bound(0, f, 1);

	// store the new trace plane (because collisions only happen from
	// the front this is always simply the triangle normal, never flipped)
	VectorNormalize(faceplanenormal);
	VectorCopy(faceplanenormal, trace->plane.normal);
	trace->plane.dist = DotProduct(point0, faceplanenormal);

	// calculate the normalized start and end distances
	d1 = DotProduct(trace->plane.normal, linestart) - trace->plane.dist;
	d2 = DotProduct(trace->plane.normal, lineend) - trace->plane.dist;

	// calculate a nudged fraction to keep it out of the surface
	// (the main fraction remains perfect)
	fnudged = (d1 - collision_impactnudge.value) / (d1 - d2);
	trace->fraction = bound(0, fnudged, 1);

	// store the new trace endpos
	// not needed, it's calculated later when the trace is finished
	//trace->endpos[0] = linestart[0] + fnudged * (lineend[0] - linestart[0]);
	//trace->endpos[1] = linestart[1] + fnudged * (lineend[1] - linestart[1]);
	//trace->endpos[2] = linestart[2] + fnudged * (lineend[2] - linestart[2]);
	trace->hitsupercontents = supercontents;
	trace->hitq3surfaceflags = q3surfaceflags;
	trace->hittexture = texture;
#endif
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
	bsp = (colbsp_t *)Mem_Alloc(mempool, sizeof(colbsp_t));
	bsp->mempool = mempool;
	bsp->nodes = (colbspnode_t *)Mem_Alloc(bsp->mempool, sizeof(colbspnode_t));
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
	mins[0] -= 1;
	mins[1] -= 1;
	mins[2] -= 1;
	maxs[0] += 1;
	maxs[1] += 1;
	maxs[2] += 1;
}

//===========================================

void Collision_ClipToGenericEntity(trace_t *trace, dp_model_t *model, int frame, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4_t *matrix, matrix4x4_t *inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask)
{
	float starttransformed[3], endtransformed[3];

	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;

	Matrix4x4_Transform(inversematrix, start, starttransformed);
	Matrix4x4_Transform(inversematrix, end, endtransformed);
#if COLLISIONPARANOID >= 3
	Con_Printf("trans(%f %f %f -> %f %f %f, %f %f %f -> %f %f %f)", start[0], start[1], start[2], starttransformed[0], starttransformed[1], starttransformed[2], end[0], end[1], end[2], endtransformed[0], endtransformed[1], endtransformed[2]);
#endif

	if (model && model->TraceBox)
		model->TraceBox(model, bound(0, frame, (model->numframes - 1)), trace, starttransformed, mins, maxs, endtransformed, hitsupercontentsmask);
	else
		Collision_ClipTrace_Box(trace, bodymins, bodymaxs, starttransformed, mins, maxs, endtransformed, hitsupercontentsmask, bodysupercontents, 0, NULL);
	trace->fraction = bound(0, trace->fraction, 1);
	trace->realfraction = bound(0, trace->realfraction, 1);

	VectorLerp(start, trace->fraction, end, trace->endpos);
	// transform plane
	// NOTE: this relies on plane.dist being directly after plane.normal
	Matrix4x4_TransformPositivePlane(matrix, trace->plane.normal[0], trace->plane.normal[1], trace->plane.normal[2], trace->plane.dist, trace->plane.normal);
}

void Collision_ClipToWorld(trace_t *trace, dp_model_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontents)
{
	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;
	if (model && model->TraceBox)
		model->TraceBox(model, 0, trace, start, mins, maxs, end, hitsupercontents);
	trace->fraction = bound(0, trace->fraction, 1);
	trace->realfraction = bound(0, trace->realfraction, 1);
	VectorLerp(start, trace->fraction, end, trace->endpos);
}

void Collision_ClipLineToGenericEntity(trace_t *trace, dp_model_t *model, int frame, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4_t *matrix, matrix4x4_t *inversematrix, const vec3_t start, const vec3_t end, int hitsupercontentsmask)
{
	float starttransformed[3], endtransformed[3];

	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;

	Matrix4x4_Transform(inversematrix, start, starttransformed);
	Matrix4x4_Transform(inversematrix, end, endtransformed);
#if COLLISIONPARANOID >= 3
	Con_Printf("trans(%f %f %f -> %f %f %f, %f %f %f -> %f %f %f)", start[0], start[1], start[2], starttransformed[0], starttransformed[1], starttransformed[2], end[0], end[1], end[2], endtransformed[0], endtransformed[1], endtransformed[2]);
#endif

	if (model && model->TraceLine)
		model->TraceLine(model, bound(0, frame, (model->numframes - 1)), trace, starttransformed, endtransformed, hitsupercontentsmask);
	else
		Collision_ClipTrace_Box(trace, bodymins, bodymaxs, starttransformed, vec3_origin, vec3_origin, endtransformed, hitsupercontentsmask, bodysupercontents, 0, NULL);
	trace->fraction = bound(0, trace->fraction, 1);
	trace->realfraction = bound(0, trace->realfraction, 1);

	VectorLerp(start, trace->fraction, end, trace->endpos);
	// transform plane
	// NOTE: this relies on plane.dist being directly after plane.normal
	Matrix4x4_TransformPositivePlane(matrix, trace->plane.normal[0], trace->plane.normal[1], trace->plane.normal[2], trace->plane.dist, trace->plane.normal);
}

void Collision_ClipLineToWorld(trace_t *trace, dp_model_t *model, const vec3_t start, const vec3_t end, int hitsupercontents)
{
	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;
	if (model && model->TraceLine)
		model->TraceLine(model, 0, trace, start, end, hitsupercontents);
	trace->fraction = bound(0, trace->fraction, 1);
	trace->realfraction = bound(0, trace->realfraction, 1);
	VectorLerp(start, trace->fraction, end, trace->endpos);
}

void Collision_ClipPointToGenericEntity(trace_t *trace, dp_model_t *model, int frame, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4_t *matrix, matrix4x4_t *inversematrix, const vec3_t start, int hitsupercontentsmask)
{
	float starttransformed[3];

	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;

	Matrix4x4_Transform(inversematrix, start, starttransformed);
#if COLLISIONPARANOID >= 3
	Con_Printf("trans(%f %f %f -> %f %f %f)", start[0], start[1], start[2], starttransformed[0], starttransformed[1], starttransformed[2]);
#endif

	if (model && model->TracePoint)
		model->TracePoint(model, bound(0, frame, (model->numframes - 1)), trace, starttransformed, hitsupercontentsmask);
	else
		Collision_ClipTrace_Point(trace, bodymins, bodymaxs, starttransformed, hitsupercontentsmask, bodysupercontents, 0, NULL);

	VectorCopy(start, trace->endpos);
	// transform plane
	// NOTE: this relies on plane.dist being directly after plane.normal
	Matrix4x4_TransformPositivePlane(matrix, trace->plane.normal[0], trace->plane.normal[1], trace->plane.normal[2], trace->plane.dist, trace->plane.normal);
}

void Collision_ClipPointToWorld(trace_t *trace, dp_model_t *model, const vec3_t start, int hitsupercontents)
{
	memset(trace, 0, sizeof(*trace));
	trace->fraction = trace->realfraction = 1;
	if (model && model->TracePoint)
		model->TracePoint(model, 0, trace, start, hitsupercontents);
	VectorCopy(start, trace->endpos);
}

void Collision_CombineTraces(trace_t *cliptrace, const trace_t *trace, void *touch, qboolean isbmodel)
{
	// take the 'best' answers from the new trace and combine with existing data
	if (trace->allsolid)
		cliptrace->allsolid = true;
	if (trace->startsolid)
	{
		if (isbmodel)
			cliptrace->bmodelstartsolid = true;
		cliptrace->startsolid = true;
		if (cliptrace->realfraction == 1)
			cliptrace->ent = touch;
	}
	// don't set this except on the world, because it can easily confuse
	// monsters underwater if there's a bmodel involved in the trace
	// (inopen && inwater is how they check water visibility)
	//if (trace->inopen)
	//	cliptrace->inopen = true;
	if (trace->inwater)
		cliptrace->inwater = true;
	if ((trace->realfraction <= cliptrace->realfraction) && (VectorLength2(trace->plane.normal) > 0))
	{
		cliptrace->fraction = trace->fraction;
		cliptrace->realfraction = trace->realfraction;
		VectorCopy(trace->endpos, cliptrace->endpos);
		cliptrace->plane = trace->plane;
		cliptrace->ent = touch;
		cliptrace->hitsupercontents = trace->hitsupercontents;
		cliptrace->hitq3surfaceflags = trace->hitq3surfaceflags;
		cliptrace->hittexture = trace->hittexture;
	}
	cliptrace->startsupercontents |= trace->startsupercontents;
}

void Collision_ShortenTrace(trace_t *trace, float shorten_factor, const vec3_t end)
{
	// now undo our moving end 1 qu farther...
	trace->fraction = bound(trace->fraction, trace->fraction / shorten_factor - 1e-6, 1); // we subtract 1e-6 to guard for roundoff errors
	trace->realfraction = bound(trace->realfraction, trace->realfraction / shorten_factor - 1e-6, 1); // we subtract 1e-6 to guard for roundoff errors
	if(trace->fraction >= 1) // trace would NOT hit if not expanded!
	{
		trace->fraction = 1;
		trace->realfraction = 1;
		VectorCopy(end, trace->endpos);
		memset(&trace->plane, 0, sizeof(trace->plane));
		trace->ent = NULL;
		trace->hitsupercontentsmask = 0;
		trace->hitsupercontents = 0;
		trace->hitq3surfaceflags = 0;
		trace->hittexture = NULL;
	}
}
