
#include "quakedef.h"
#include "polygon.h"

#define MAXRECURSIVEPORTALPLANES 1024
#define MAXRECURSIVEPORTALS 256

static tinyplane_t portalplanes[MAXRECURSIVEPORTALPLANES];
static int ranoutofportalplanes;
static int ranoutofportals;
static float portaltemppoints[2][256][3];
static float portaltemppoints2[256][3];
static int portal_markid = 0;
static float boxpoints[4*3];

static int Portal_PortalThroughPortalPlanes(tinyplane_t *clipplanes, int clipnumplanes, float *targpoints, int targnumpoints, float *out, int maxpoints)
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
		PolygonF_Divide(numpoints, &portaltemppoints[0][0][0], clipplanes[i].normal[0], clipplanes[i].normal[1], clipplanes[i].normal[2], clipplanes[i].dist, 1.0f/32.0f, 256, &portaltemppoints[1][0][0], &numpoints, 0, NULL, NULL, NULL);
		if (numpoints < 3)
			return numpoints;
		memcpy(&portaltemppoints[0][0][0], &portaltemppoints[1][0][0], numpoints * 3 * sizeof(float));
	}
	if (numpoints > maxpoints)
		return -1;
	memcpy(out, &portaltemppoints[0][0][0], numpoints * 3 * sizeof(float));
	return numpoints;
}

static int Portal_RecursiveFlowSearch (mleaf_t *leaf, vec3_t eye, int firstclipplane, int numclipplanes)
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
					VectorNormalize(newplanes[i].normal);
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

static void Portal_PolygonRecursiveMarkLeafs(mnode_t *node, float *polypoints, int numpoints)
{
	int i, front;
	float *p;

loc0:
	if (!node->plane)
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

	// if there is no model, it can not block visibility
	if (model == NULL || !model->brush.PointInLeaf)
		return true;

	portal_markid++;

	Portal_PolygonRecursiveMarkLeafs(model->brush.data_nodes, polypoints, numpoints);

	eyeleaf = model->brush.PointInLeaf(model, eye);

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
		VectorNormalize(portalplanes[i].normal);
		portalplanes[i].dist = DotProduct(eye, portalplanes[i].normal);
		if (DotProduct(portalplanes[i].normal, center) <= portalplanes[i].dist)
		{
			// polygon can't see it's own center, discard
			return false;
		}
	}

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

typedef struct portalrecursioninfo_s
{
	int exact;
	int numfrustumplanes;
	vec3_t boxmins;
	vec3_t boxmaxs;
	int numsurfaces;
	int *surfacelist;
	unsigned char *surfacepvs;
	int numleafs;
	int *leaflist;
	unsigned char *leafpvs;
	model_t *model;
	vec3_t eye;
	float *updateleafsmins;
	float *updateleafsmaxs;
}
portalrecursioninfo_t;

static void Portal_RecursiveFlow (portalrecursioninfo_t *info, mleaf_t *leaf, int firstclipplane, int numclipplanes)
{
	mportal_t *p;
	int newpoints, i, prev;
	float dist;
	vec3_t center;
	tinyplane_t *newplanes;

	for (i = 0;i < 3;i++)
	{
		if (info->updateleafsmins && info->updateleafsmins[i] > leaf->mins[i]) info->updateleafsmins[i] = leaf->mins[i];
		if (info->updateleafsmaxs && info->updateleafsmaxs[i] < leaf->maxs[i]) info->updateleafsmaxs[i] = leaf->maxs[i];
	}


	if (info->leafpvs)
	{
		int leafindex = leaf - info->model->brush.data_leafs;
		if (!CHECKPVSBIT(info->leafpvs, leafindex))
		{
			SETPVSBIT(info->leafpvs, leafindex);
			info->leaflist[info->numleafs++] = leafindex;
		}
	}

	// mark surfaces in leaf that can be seen through portal
	if (leaf->numleafsurfaces && info->surfacepvs)
	{
		for (i = 0;i < leaf->numleafsurfaces;i++)
		{
			int surfaceindex = leaf->firstleafsurface[i];
			if (!CHECKPVSBIT(info->surfacepvs, surfaceindex))
			{
				msurface_t *surface = info->model->data_surfaces + surfaceindex;
				if (BoxesOverlap(surface->mins, surface->maxs, info->boxmins, info->boxmaxs))
				{
					if (info->exact)
					{
						int j;
						const int *elements;
						const float *vertex3f;
						float v[9], trimins[3], trimaxs[3];
						vertex3f = surface->groupmesh->data_vertex3f;
						elements = (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle);
						for (j = 0;j < surface->num_triangles;j++, elements += 3)
						{
							VectorCopy(vertex3f + elements[0] * 3, v + 0);
							VectorCopy(vertex3f + elements[1] * 3, v + 3);
							VectorCopy(vertex3f + elements[2] * 3, v + 6);
							if (PointInfrontOfTriangle(info->eye, v + 0, v + 3, v + 6))
							{
								trimins[0] = min(v[0], min(v[3], v[6]));
								trimaxs[0] = max(v[0], max(v[3], v[6]));
								trimins[1] = min(v[1], min(v[4], v[7]));
								trimaxs[1] = max(v[1], max(v[4], v[7]));
								trimins[2] = min(v[2], min(v[5], v[8]));
								trimaxs[2] = max(v[2], max(v[5], v[8]));
								if (BoxesOverlap(trimins, trimaxs, info->boxmins, info->boxmaxs) && Portal_PortalThroughPortalPlanes(&portalplanes[firstclipplane], numclipplanes, v, 3, &portaltemppoints2[0][0], 256) >= 3)
								{
									SETPVSBIT(info->surfacepvs, surfaceindex);
									info->surfacelist[info->numsurfaces++] = surfaceindex;
									break;
								}
							}
						}
					}
					else
					{
						SETPVSBIT(info->surfacepvs, surfaceindex);
						info->surfacelist[info->numsurfaces++] = surfaceindex;
					}
				}
			}
		}
	}

	// follow portals into other leafs
	for (p = leaf->portals;p;p = p->next)
	{
		// only flow through portals facing the viewer
		dist = PlaneDiff(info->eye, (&p->plane));
		if (dist < 0 && BoxesOverlap(p->past->mins, p->past->maxs, info->boxmins, info->boxmaxs))
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
				// calculate the planes, and make sure the polygon can see its own center
				newplanes = &portalplanes[firstclipplane + numclipplanes];
				for (prev = newpoints - 1, i = 0;i < newpoints;prev = i, i++)
				{
					TriangleNormal(portaltemppoints2[prev], portaltemppoints2[i], info->eye, newplanes[i].normal);
					VectorNormalize(newplanes[i].normal);
					newplanes[i].dist = DotProduct(info->eye, newplanes[i].normal);
					if (DotProduct(newplanes[i].normal, center) <= newplanes[i].dist)
					{
						// polygon can't see its own center, discard and use parent portal
						break;
					}
				}
				if (i == newpoints)
					Portal_RecursiveFlow(info, p->past, firstclipplane + numclipplanes, newpoints);
				else
					Portal_RecursiveFlow(info, p->past, firstclipplane, numclipplanes);
			}
		}
	}
}

static void Portal_RecursiveFindLeafForFlow(portalrecursioninfo_t *info, mnode_t *node)
{
	if (node->plane)
	{
		float f = DotProduct(info->eye, node->plane->normal) - node->plane->dist;
		if (f > -0.1)
			Portal_RecursiveFindLeafForFlow(info, node->children[0]);
		if (f < 0.1)
			Portal_RecursiveFindLeafForFlow(info, node->children[1]);
	}
	else
	{
		mleaf_t *leaf = (mleaf_t *)node;
		if (leaf->clusterindex >= 0)
			Portal_RecursiveFlow(info, leaf, 0, info->numfrustumplanes);
	}
}

void Portal_Visibility(model_t *model, const vec3_t eye, int *leaflist, unsigned char *leafpvs, int *numleafspointer, int *surfacelist, unsigned char *surfacepvs, int *numsurfacespointer, const mplane_t *frustumplanes, int numfrustumplanes, int exact, const float *boxmins, const float *boxmaxs, float *updateleafsmins, float *updateleafsmaxs)
{
	int i;
	portalrecursioninfo_t info;

	// if there is no model, it can not block visibility
	if (model == NULL)
	{
		Con_Print("Portal_Visibility: NULL model\n");
		return;
	}

	if (!model->brush.data_nodes)
	{
		Con_Print("Portal_Visibility: not a brush model\n");
		return;
	}

	// put frustum planes (if any) into tinyplane format at start of buffer
	for (i = 0;i < numfrustumplanes;i++)
	{
		VectorCopy(frustumplanes[i].normal, portalplanes[i].normal);
		portalplanes[i].dist = frustumplanes[i].dist;
	}

	ranoutofportalplanes = false;
	ranoutofportals = false;

	VectorCopy(boxmins, info.boxmins);
	VectorCopy(boxmaxs, info.boxmaxs);
	info.exact = exact;
	info.numsurfaces = 0;
	info.surfacelist = surfacelist;
	info.surfacepvs = surfacepvs;
	info.numleafs = 0;
	info.leaflist = leaflist;
	info.leafpvs = leafpvs;
	info.model = model;
	VectorCopy(eye, info.eye);
	info.numfrustumplanes = numfrustumplanes;
	info.updateleafsmins = updateleafsmins;
	info.updateleafsmaxs = updateleafsmaxs;

	Portal_RecursiveFindLeafForFlow(&info, model->brush.data_nodes);

	if (ranoutofportalplanes)
		Con_Printf("Portal_RecursiveFlow: ran out of %d plane stack when recursing through portals\n", MAXRECURSIVEPORTALPLANES);
	if (ranoutofportals)
		Con_Printf("Portal_RecursiveFlow: ran out of %d portal stack when recursing through portals\n", MAXRECURSIVEPORTALS);
	if (numsurfacespointer)
		*numsurfacespointer = info.numsurfaces;
	if (numleafspointer)
		*numleafspointer = info.numleafs;
}

