
#include "quakedef.h"

#define MAXRECURSIVEPORTALPLANES 1024
#define MAXRECURSIVEPORTALS 256

static tinyplane_t portalplanes[MAXRECURSIVEPORTALPLANES];
static int portalplanecount;
static int ranoutofportalplanes;
static int ranoutofportals;
static float portaltemppoints[2][256][3];
static float portaltemppoints2[256][3];
static int portal_markid = 0;
static float boxpoints[4*3];

int Portal_ClipPolygonToPlane(float *in, float *out, int inpoints, int maxoutpoints, tinyplane_t *p)
{
	int i, outpoints, prevside, side;
	float *prevpoint, prevdist, dist, dot;

	if (inpoints < 3)
		return inpoints;
	// begin with the last point, then enter the loop with the first point as current
	prevpoint = in + 3 * (inpoints - 1);
	prevdist = DotProduct(prevpoint, p->normal) - p->dist;
	prevside = prevdist >= 0 ? SIDE_FRONT : SIDE_BACK;
	i = 0;
	outpoints = 0;
	goto begin;
	for (;i < inpoints;i++)
	{
		prevpoint = in;
		prevdist = dist;
		prevside = side;
		in += 3;

begin:
		dist = DotProduct(in, p->normal) - p->dist;
		side = dist >= 0 ? SIDE_FRONT : SIDE_BACK;

		if (prevside == SIDE_FRONT)
		{
			if (outpoints >= maxoutpoints)
				return -1;
			VectorCopy(prevpoint, out);
			out += 3;
			outpoints++;
			if (side == SIDE_FRONT)
				continue;
		}
		else if (side == SIDE_BACK)
			continue;

		// generate a split point
		if (outpoints >= maxoutpoints)
			return -1;
		dot = prevdist / (prevdist - dist);
		out[0] = prevpoint[0] + dot * (in[0] - prevpoint[0]);
		out[1] = prevpoint[1] + dot * (in[1] - prevpoint[1]);
		out[2] = prevpoint[2] + dot * (in[2] - prevpoint[2]);
		out += 3;
		outpoints++;
	}

	return outpoints;
}


int Portal_PortalThroughPortalPlanes(tinyplane_t *clipplanes, int clipnumplanes, float *targpoints, int targnumpoints, float *out, int maxpoints)
{
	int numpoints, i;
	if (targnumpoints < 3)
		return targnumpoints;
	if (maxpoints < 3)
		return -1;
	numpoints = targnumpoints;
	memcpy(&portaltemppoints[0][0][0], targpoints, numpoints * 3 * sizeof(float));
	for (i = 0;i < clipnumplanes;i++)
	{
		numpoints = Portal_ClipPolygonToPlane(&portaltemppoints[0][0][0], &portaltemppoints[1][0][0], numpoints, 256, clipplanes + i);
		if (numpoints < 3)
			return numpoints;
		memcpy(&portaltemppoints[0][0][0], &portaltemppoints[1][0][0], numpoints * 3 * sizeof(float));
	}
	if (numpoints > maxpoints)
		return -1;
	memcpy(out, &portaltemppoints[1][0][0], numpoints * 3 * sizeof(float));
	return numpoints;
}

int Portal_RecursiveFlowSearch (mleaf_t *leaf, vec3_t eye, int firstclipplane, int numclipplanes)
{
	mportal_t *p;
	int newpoints, i, prev;
	vec3_t center, v1, v2;
	tinyplane_t *newplanes;

	if (leaf->portalmarkid == portal_markid)
		return true;

	// follow portals into other leafs
	for (p = leaf->portals;p;p = p->next)
	{
		// only flow through portals facing away from the viewer
		if (PlaneDiff(eye, (&p->plane)) < 0)
		{
			newpoints = Portal_PortalThroughPortalPlanes(&portalplanes[firstclipplane], numclipplanes, (float *) p->points, p->numpoints, &portaltemppoints2[0][0], 256);
			if (newpoints < 3)
				continue;
			else if (firstclipplane + numclipplanes + newpoints > MAXRECURSIVEPORTALPLANES)
				ranoutofportalplanes = true;
			else
			{
				// find the center by averaging
				VectorClear(center);
				for (i = 0;i < newpoints;i++)
					VectorAdd(center, portaltemppoints2[i], center);
				// ixtable is a 1.0f / N table
				VectorScale(center, ixtable[newpoints], center);
				// calculate the planes, and make sure the polygon can see it's own center
				newplanes = &portalplanes[firstclipplane + numclipplanes];
				for (prev = newpoints - 1, i = 0;i < newpoints;prev = i, i++)
				{
					VectorSubtract(eye, portaltemppoints2[i], v1);
					VectorSubtract(portaltemppoints2[prev], portaltemppoints2[i], v2);
					CrossProduct(v1, v2, newplanes[i].normal);
					VectorNormalizeFast(newplanes[i].normal);
					newplanes[i].dist = DotProduct(eye, newplanes[i].normal);
					if (DotProduct(newplanes[i].normal, center) <= newplanes[i].dist)
					{
						// polygon can't see it's own center, discard and use parent portal
						break;
					}
				}
				if (i == newpoints)
				{
					if (Portal_RecursiveFlowSearch(p->past, eye, firstclipplane + numclipplanes, newpoints))
						return true;
    			}
				else
				{
					if (Portal_RecursiveFlowSearch(p->past, eye, firstclipplane, numclipplanes))
						return true;
    			}
			}
		}
	}

	return false;
}

void Portal_PolygonRecursiveMarkLeafs(mnode_t *node, float *polypoints, int numpoints)
{
	int i, front;
	float *p;

loc0:
	if (node->contents < 0)
	{
		((mleaf_t *)node)->portalmarkid = portal_markid;
		return;
	}

	front = 0;
	for (i = 0, p = polypoints;i < numpoints;i++, p += 3)
	{
		if (DotProduct(p, node->plane->normal) > node->plane->dist)
			front++;
	}
	if (front > 0)
	{
		if (front == numpoints)
		{
			node = node->children[0];
			goto loc0;
		}
		else
			Portal_PolygonRecursiveMarkLeafs(node->children[0], polypoints, numpoints);
	}
	node = node->children[1];
	goto loc0;
}

int Portal_CheckPolygon(model_t *model, vec3_t eye, float *polypoints, int numpoints)
{
	int i, prev, returnvalue;
	mleaf_t *eyeleaf;
	vec3_t center, v1, v2;

	portal_markid++;

	Mod_CheckLoaded(model);
	Portal_PolygonRecursiveMarkLeafs(model->nodes, polypoints, numpoints);

	eyeleaf = Mod_PointInLeaf(eye, model);

	// find the center by averaging
	VectorClear(center);
	for (i = 0;i < numpoints;i++)
		VectorAdd(center, (&polypoints[i * 3]), center);
	// ixtable is a 1.0f / N table
	VectorScale(center, ixtable[numpoints], center);

	// calculate the planes, and make sure the polygon can see it's own center
	for (prev = numpoints - 1, i = 0;i < numpoints;prev = i, i++)
	{
		VectorSubtract(eye, (&polypoints[i * 3]), v1);
		VectorSubtract((&polypoints[prev * 3]), (&polypoints[i * 3]), v2);
		CrossProduct(v1, v2, portalplanes[i].normal);
		VectorNormalizeFast(portalplanes[i].normal);
		portalplanes[i].dist = DotProduct(eye, portalplanes[i].normal);
		if (DotProduct(portalplanes[i].normal, center) <= portalplanes[i].dist)
		{
			// polygon can't see it's own center, discard
			return false;
		}
	}

	portalplanecount = 0;
	ranoutofportalplanes = false;
	ranoutofportals = false;

	returnvalue = Portal_RecursiveFlowSearch(eyeleaf, eye, 0, numpoints);

	if (ranoutofportalplanes)
		Con_Printf("Portal_RecursiveFlowSearch: ran out of %d plane stack when recursing through portals\n", MAXRECURSIVEPORTALPLANES);
	if (ranoutofportals)
		Con_Printf("Portal_RecursiveFlowSearch: ran out of %d portal stack when recursing through portals\n", MAXRECURSIVEPORTALS);

	return returnvalue;
}

#define Portal_MinsBoxPolygon(axis, axisvalue, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4) \
{\
	if (eye[(axis)] < ((axisvalue) - 0.5f))\
	{\
		boxpoints[ 0] = x1;boxpoints[ 1] = y1;boxpoints[ 2] = z1;\
		boxpoints[ 3] = x2;boxpoints[ 4] = y2;boxpoints[ 5] = z2;\
		boxpoints[ 6] = x3;boxpoints[ 7] = y3;boxpoints[ 8] = z3;\
		boxpoints[ 9] = x4;boxpoints[10] = y4;boxpoints[11] = z4;\
		if (Portal_CheckPolygon(model, eye, boxpoints, 4))\
			return true;\
	}\
}

#define Portal_MaxsBoxPolygon(axis, axisvalue, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4) \
{\
	if (eye[(axis)] > ((axisvalue) + 0.5f))\
	{\
		boxpoints[ 0] = x1;boxpoints[ 1] = y1;boxpoints[ 2] = z1;\
		boxpoints[ 3] = x2;boxpoints[ 4] = y2;boxpoints[ 5] = z2;\
		boxpoints[ 6] = x3;boxpoints[ 7] = y3;boxpoints[ 8] = z3;\
		boxpoints[ 9] = x4;boxpoints[10] = y4;boxpoints[11] = z4;\
		if (Portal_CheckPolygon(model, eye, boxpoints, 4))\
			return true;\
	}\
}

int Portal_CheckBox(model_t *model, vec3_t eye, vec3_t a, vec3_t b)
{
	if (eye[0] >= (a[0] - 1.0f) && eye[0] < (b[0] + 1.0f)
	 && eye[1] >= (a[1] - 1.0f) && eye[1] < (b[1] + 1.0f)
	 && eye[2] >= (a[2] - 1.0f) && eye[2] < (b[2] + 1.0f))
		return true;

	Portal_MinsBoxPolygon
	(
		0, a[0],
		a[0], a[1], a[2],
		a[0], b[1], a[2],
		a[0], b[1], b[2],
		a[0], a[1], b[2]
	);
	Portal_MaxsBoxPolygon
	(
		0, b[0],
		b[0], b[1], a[2],
		b[0], a[1], a[2],
		b[0], a[1], b[2],
		b[0], b[1], b[2]
	);
	Portal_MinsBoxPolygon
	(
		1, a[1],
		b[0], a[1], a[2],
		a[0], a[1], a[2],
		a[0], a[1], b[2],
		b[0], a[1], b[2]
	);
	Portal_MaxsBoxPolygon
	(
		1, b[1],
		a[0], b[1], a[2],
		b[0], b[1], a[2],
		b[0], b[1], b[2],
		a[0], b[1], b[2]
	);
	Portal_MinsBoxPolygon
	(
		2, a[2],
		a[0], a[1], a[2],
		b[0], a[1], a[2],
		b[0], b[1], a[2],
		a[0], b[1], a[2]
	);
	Portal_MaxsBoxPolygon
	(
		2, b[2],
		b[0], a[1], b[2],
		a[0], a[1], b[2],
		a[0], b[1], b[2],
		b[0], b[1], b[2]
	);

	return false;
}

