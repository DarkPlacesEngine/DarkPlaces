
// Shadow Volume BSP code written by Forest "LordHavoc" Hale on 2003-11-06 and placed into public domain.
// Modified by LordHavoc (to make it work and other nice things like that) on 2007-01-24 and 2007-01-25

#include <math.h>
#include <string.h>
#include "svbsp.h"
#include "polygon.h"

#define MAX_SVBSP_POLYGONPOINTS 64
#define SVBSP_CLIP_EPSILON (1.0f / 1024.0f)

#define SVBSP_DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])

static void SVBSP_PlaneFromPoints(float *plane4f, const float *p1, const float *p2, const float *p3)
{
	float ilength;
	// calculate unnormalized plane
	plane4f[0] = (p1[1] - p2[1]) * (p3[2] - p2[2]) - (p1[2] - p2[2]) * (p3[1] - p2[1]);
	plane4f[1] = (p1[2] - p2[2]) * (p3[0] - p2[0]) - (p1[0] - p2[0]) * (p3[2] - p2[2]);
	plane4f[2] = (p1[0] - p2[0]) * (p3[1] - p2[1]) - (p1[1] - p2[1]) * (p3[0] - p2[0]);
	plane4f[3] = SVBSP_DotProduct(plane4f, p1);
	// normalize the plane normal and adjust distance accordingly
	ilength = (float)sqrt(SVBSP_DotProduct(plane4f, plane4f));
	if (ilength)
		ilength = 1.0f / ilength;
	plane4f[0] *= ilength;
	plane4f[1] *= ilength;
	plane4f[2] *= ilength;
	plane4f[3] *= ilength;
}

void SVBSP_Init(svbsp_t *b, const float *origin, int maxnodes, svbsp_node_t *nodes)
{
	memset(b, 0, sizeof(*b));
	b->origin[0] = origin[0];
	b->origin[1] = origin[1];
	b->origin[2] = origin[2];
	b->numnodes = 3;
	b->maxnodes = maxnodes;
	b->nodes = nodes;
	b->ranoutofnodes = 0;
	b->stat_occluders_rejected = 0;
	b->stat_occluders_accepted = 0;
	b->stat_occluders_fragments_accepted = 0;
	b->stat_occluders_fragments_rejected = 0;
	b->stat_queries_rejected = 0;
	b->stat_queries_accepted = 0;
	b->stat_queries_fragments_accepted = 0;
	b->stat_queries_fragments_rejected = 0;

	// the bsp tree must be initialized to have two perpendicular splits axes
	// through origin, otherwise the polygon insertions would affect the
	// opposite side of the tree, which would be disasterous.
	//
	// so this code has to make 3 nodes and 4 leafs, and since the leafs are
	// represented by empty/solid state numbers in this system rather than
	// actual structs, we only need to make the 3 nodes.

	// root node
	// this one splits the world into +X and -X sides
	b->nodes[0].plane[0] = 1;
	b->nodes[0].plane[1] = 0;
	b->nodes[0].plane[2] = 0;
	b->nodes[0].plane[3] = origin[0];
	b->nodes[0].parent = -1;
	b->nodes[0].children[0] = 1;
	b->nodes[0].children[1] = 2;

	// +X side node
	// this one splits the +X half of the world into +Y and -Y
	b->nodes[1].plane[0] = 0;
	b->nodes[1].plane[1] = 1;
	b->nodes[1].plane[2] = 0;
	b->nodes[1].plane[3] = origin[1];
	b->nodes[1].parent = 0;
	b->nodes[1].children[0] = -1;
	b->nodes[1].children[1] = -1;

	// -X side node
	// this one splits the -X half of the world into +Y and -Y
	b->nodes[2].plane[0] = 0;
	b->nodes[2].plane[1] = 1;
	b->nodes[2].plane[2] = 0;
	b->nodes[2].plane[3] = origin[1];
	b->nodes[2].parent = 0;
	b->nodes[2].children[0] = -1;
	b->nodes[2].children[1] = -1;
}

static void SVBSP_InsertOccluderPolygonNodes(svbsp_t *b, int *parentnodenumpointer, int parentnodenum, int numpoints, const float *points, void (*fragmentcallback)(void *fragmentcallback_pointer1, int fragmentcallback_number1, svbsp_t *b, int numpoints, const float *points), void *fragmentcallback_pointer1, int fragmentcallback_number1)
{
	// now we need to create up to numpoints + 1 new nodes, forming a BSP tree
	// describing the occluder polygon's shadow volume
	int i, j, p, basenum;
	svbsp_node_t *node;
#if 0
	unsigned int sideflags[(MAX_SVBSP_POLYGONPOINTS+31)>>5];
	float *parentnodeplane;
	float plane[4];
#if 0
	float mindist;
	float maxdist;
	float d;
#endif
	int n;
#endif

	// if there aren't enough nodes remaining, skip it
	if (b->numnodes + numpoints + 1 >= b->maxnodes)
	{
		b->ranoutofnodes = 1;
		return;
	}

	// add one node per side, then the actual occluding face node

	// thread safety notes:
	// DO NOT multithread insertion, it could be made 'safe' but the results
	// would be inconsistent.
	//
	// it is completely safe to multithread queries in all cases.
	//
	// if an insertion is occurring the query will give intermediate results,
	// being blocked by some volumes but not others, which is perfectly okay
	// for visibility culling intended only to reduce rendering work

	// note down the first available nodenum for the *parentnodenumpointer
	// line which is done last to allow multithreaded queries during an
	// insertion
	basenum = b->numnodes;
#if 1
	for (i = 0, p = numpoints - 1;i < numpoints;p = i, i++)
	{
#if 1
		// see if a parent plane describes this side
		for (j = parentnodenum;j >= 0;j = b->nodes[j].parent)
		{
			float *parentnodeplane = b->nodes[j].plane;
		//	float v[3];
		//	v[0] = SVBSP_DotProduct(b->origin     , parentnodeplane) - parentnodeplane[3];
		//	v[1] = SVBSP_DotProduct(points + p * 3, parentnodeplane) - parentnodeplane[3];
		//	v[2] = SVBSP_DotProduct(points + i * 3, parentnodeplane) - parentnodeplane[3];
		//	if (SVBSP_DotProduct(v,v) < (SVBSP_CLIP_EPSILON*SVBSP_CLIP_EPSILON))
			if (fabs(SVBSP_DotProduct(points + p * 3, parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
			 && fabs(SVBSP_DotProduct(points + i * 3, parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
			 && fabs(SVBSP_DotProduct(b->origin     , parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON)
				break;
		}
		if (j >= 0)
			continue; // already have a matching parent plane
#endif

#else
#if 1
	// iterate parent planes and check if any sides of the polygon lie on their plane - if so the polygon can not contribute a new node for that side
	for (i = 0;i < (int)(sizeof(sideflags)/sizeof(sideflags[0]));i++)
		sideflags[i] = 0;
	for (j = parentnodenum;j >= 0;j = b->nodes[j].parent)
	{
		parentnodeplane = b->nodes[j].plane;
		plane[0] = parentnodeplane[0];
		plane[1] = parentnodeplane[1];
		plane[2] = parentnodeplane[2];
		plane[3] = parentnodeplane[3];
#if 0
		mindist = plane[3] - SVBSP_CLIP_EPSILON;
		maxdist = plane[3] + SVBSP_CLIP_EPSILON;
#endif
		// if a parent plane crosses the origin, it is a side plane
		// if it does not cross the origin, it is a face plane, and thus will
		// not match any side planes we could add
#if 1
		if (fabs(SVBSP_DotProduct(b->origin, plane) - plane[3]) > SVBSP_CLIP_EPSILON)
			continue;
#else
		d = SVBSP_DotProduct(b->origin     , plane);
		if (d < mindist || d > maxdist)
			continue;
#endif
		// classify each side as belonging to this parent plane or not
		// do a distance check on the last point of the polygon first, and
		// then one distance check per point, reusing the previous point
		// distance check to classify this side as being on or off the plane
		i = numpoints-1;
#if 1
		p = fabs(SVBSP_DotProduct(points + i * 3, plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
#else
		d = SVBSP_DotProduct(points + i * 3, plane);
		p = d >= mindist && d <= maxdist;
#endif
		for (i = 0;i < numpoints;i++)
		{
#if 1
			n = fabs(SVBSP_DotProduct(points + i * 3, plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
#else
			d = SVBSP_DotProduct(points + i * 3, plane);
			n = d >= mindist && d <= maxdist;
#endif
			if (p && n)
				sideflags[i>>5] |= 1<<(i&31);
			p = n;
		}
#endif
	}

	for (i = 0, p = numpoints - 1;i < numpoints;p = i, i++)
	{
#if 1
		// skip any sides that were classified as belonging to a parent plane
		if (sideflags[i>>5] & (1<<(i&31)))
			continue;
#endif
#endif
		// create a side plane
		// anything infront of this is not inside the shadow volume
		node = b->nodes + b->numnodes++;
		SVBSP_PlaneFromPoints(node->plane, b->origin, points + p * 3, points + i * 3);
		// we need to flip the plane if it puts any part of the polygon on the
		// wrong side
		// (in this way this code treats all polygons as float sided)
		//
		// because speed is important this stops as soon as it finds proof
		// that the orientation is right or wrong
		// (we know that the plane is on one edge of the polygon, so there is
		// never a case where points lie on both sides, so the first hint is
		// sufficient)
		for (j = 0;j < numpoints;j++)
		{
			float d = SVBSP_DotProduct(points + j * 3, node->plane) - node->plane[3];
			if (d < -SVBSP_CLIP_EPSILON)
				break;
			if (d > SVBSP_CLIP_EPSILON)
			{
				node->plane[0] *= -1;
				node->plane[1] *= -1;
				node->plane[2] *= -1;
				node->plane[3] *= -1;
				break;
			}
		}
		node->parent = parentnodenum;
		node->children[0] = -1; // empty
		node->children[1] = -1; // empty
		// link this child into the tree
		*parentnodenumpointer = parentnodenum = (int)(node - b->nodes);
		// now point to the child pointer for the next node to update later
		parentnodenumpointer = &node->children[1];
	}

#if 1
	// see if a parent plane describes the face plane
	for (j = parentnodenum;j >= 0;j = b->nodes[j].parent)
	{
		float *parentnodeplane = b->nodes[j].plane;
		if (fabs(SVBSP_DotProduct(points    , parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
		 && fabs(SVBSP_DotProduct(points + 3, parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
		 && fabs(SVBSP_DotProduct(points + 6, parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON)
			break;
	}
	if (j < 0)
#endif
	{
		// add the face-plane node
		// infront is empty, behind is shadow
		node = b->nodes + b->numnodes++;
		SVBSP_PlaneFromPoints(node->plane, points, points + 3, points + 6);
		// this is a flip check similar to the one above
		// this one checks if the plane faces the origin, if not, flip it
		if (SVBSP_DotProduct(b->origin, node->plane) - node->plane[3] < -SVBSP_CLIP_EPSILON)
		{
			node->plane[0] *= -1;
			node->plane[1] *= -1;
			node->plane[2] *= -1;
			node->plane[3] *= -1;
		}
		node->parent = parentnodenum;
		node->children[0] = -1; // empty
		node->children[1] = -2; // shadow
		// link this child into the tree
		// (with the addition of this node, queries will now be culled by it)
		*parentnodenumpointer = (int)(node - b->nodes);
	}
}

static int SVBSP_AddPolygonNode(svbsp_t *b, int *parentnodenumpointer, int parentnodenum, int numpoints, const float *points, int insertoccluder, void (*fragmentcallback)(void *fragmentcallback_pointer1, int fragmentcallback_number1, svbsp_t *b, int numpoints, const float *points), void *fragmentcallback_pointer1, int fragmentcallback_number1)
{
	int i;
	int frontnumpoints, backnumpoints;
	float plane[4];
	float frontpoints[MAX_SVBSP_POLYGONPOINTS * 3], backpoints[MAX_SVBSP_POLYGONPOINTS * 3];
	float d;
	if (numpoints < 3)
		return 0;
	// recurse through plane nodes
	while (*parentnodenumpointer >= 0)
	{
		// do a quick check to see if there is any need to split the polygon
		svbsp_node_t *node = b->nodes + *parentnodenumpointer;
		parentnodenum = *parentnodenumpointer;
#if 1
		plane[0] = node->plane[0];
		plane[1] = node->plane[1];
		plane[2] = node->plane[2];
		plane[3] = node->plane[3];
		d = SVBSP_DotProduct(points, plane) - plane[3];
		if (d >= SVBSP_CLIP_EPSILON)
		{
			for (i = 1;i < numpoints && SVBSP_DotProduct(points + i * 3, plane) - plane[3] >= SVBSP_CLIP_EPSILON;i++);
			if (i == numpoints)
			{
				// no need to split, just go to one side
				parentnodenumpointer = &node->children[0];
				continue;
			}
		}
		else if (d <= -SVBSP_CLIP_EPSILON)
		{
			for (i = 1;i < numpoints && SVBSP_DotProduct(points + i * 3, plane) - plane[3] <= -SVBSP_CLIP_EPSILON;i++);
			if (i == numpoints)
			{
				// no need to split, just go to one side
				parentnodenumpointer = &node->children[1];
				continue;
			}
		}
#endif
		// at this point we know it crosses the plane, so we need to split it
		PolygonF_Divide(numpoints, points, node->plane[0], node->plane[1], node->plane[2], node->plane[3], SVBSP_CLIP_EPSILON, MAX_SVBSP_POLYGONPOINTS, frontpoints, &frontnumpoints, MAX_SVBSP_POLYGONPOINTS, backpoints, &backnumpoints, NULL);
		if (frontnumpoints > MAX_SVBSP_POLYGONPOINTS)
			frontnumpoints = MAX_SVBSP_POLYGONPOINTS;
		if (backnumpoints > MAX_SVBSP_POLYGONPOINTS)
			backnumpoints = MAX_SVBSP_POLYGONPOINTS;
		// recurse the sides and return the resulting bit flags
		i = 0;
		if (frontnumpoints >= 3)
			i |= SVBSP_AddPolygonNode(b, &node->children[0], (int)(node - b->nodes), frontnumpoints, frontpoints, insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		if (backnumpoints >= 3)
			i |= SVBSP_AddPolygonNode(b, &node->children[1], (int)(node - b->nodes), backnumpoints , backpoints , insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		return i;
	}
	// leaf node
	if (*parentnodenumpointer == -1)
	{
		// empty leaf node; and some geometry survived
		// if inserting an occluder, replace this empty leaf with a shadow volume
#if 0
		for (i = 0;i < numpoints-2;i++)
		{
			Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
			Debug_PolygonVertex(points[0], points[1], points[2], 0, 0, 0.25, 0, 0, 1);
			Debug_PolygonVertex(points[0 + (i + 1) * 3], points[1 + (i + 1) * 3], points[2 + (i + 1) * 3], 0, 0, 0.25, 0, 0, 1);
			Debug_PolygonVertex(points[0 + (i + 2) * 3], points[1 + (i + 2) * 3], points[2 + (i + 2) * 3], 0, 0, 0.25, 0, 0, 1);
			Debug_PolygonEnd();
		}
#endif
		if (insertoccluder)
		{
			b->stat_occluders_fragments_accepted++;
			SVBSP_InsertOccluderPolygonNodes(b, parentnodenumpointer, parentnodenum, numpoints, points, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		}
		else
			b->stat_queries_fragments_accepted++;
		if (fragmentcallback)
			fragmentcallback(fragmentcallback_pointer1, fragmentcallback_number1, b, numpoints, points);
		return 2;
	}
	else
	{
		// otherwise it's a solid leaf which destroys all polygons inside it
		if (insertoccluder)
			b->stat_occluders_fragments_rejected++;
		else
			b->stat_queries_fragments_rejected++;
#if 0
		for (i = 0;i < numpoints-2;i++)
		{
			Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
			Debug_PolygonVertex(points[0], points[1], points[2], 0, 0, 0, 0, 0.25, 1);
			Debug_PolygonVertex(points[0 + (i + 1) * 3], points[1 + (i + 1) * 3], points[2 + (i + 1) * 3], 0, 0, 0, 0, 0.25, 1);
			Debug_PolygonVertex(points[0 + (i + 2) * 3], points[1 + (i + 2) * 3], points[2 + (i + 2) * 3], 0, 0, 0, 0, 0.25, 1);
			Debug_PolygonEnd();
		}
#endif
	}
	return 1;
}

int SVBSP_AddPolygon(svbsp_t *b, int numpoints, const float *points, int insertoccluder, void (*fragmentcallback)(void *fragmentcallback_pointer1, int fragmentcallback_number1, svbsp_t *b, int numpoints, const float *points), void *fragmentcallback_pointer1, int fragmentcallback_number1)
{
	int i;
	int nodenum;
	// don't even consider an empty polygon
	if (numpoints < 3)
		return 0;
#if 0
//if (insertoccluder)
	for (i = 0;i < numpoints-2;i++)
	{
		Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
		Debug_PolygonVertex(points[0], points[1], points[2], 0, 0, 0, 0.25, 0, 1);
		Debug_PolygonVertex(points[0 + (i + 1) * 3], points[1 + (i + 1) * 3], points[2 + (i + 1) * 3], 0, 0, 0, 0.25, 0, 1);
		Debug_PolygonVertex(points[0 + (i + 2) * 3], points[1 + (i + 2) * 3], points[2 + (i + 2) * 3], 0, 0, 0, 0.25, 0, 1);
		Debug_PolygonEnd();
	}
#endif
	nodenum = 0;
	i = SVBSP_AddPolygonNode(b, &nodenum, -1, numpoints, points, insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
	if (insertoccluder)
	{
		if (i & 2)
			b->stat_occluders_accepted++;
		else
			b->stat_occluders_rejected++;
	}
	else
	{
		if (i & 2)
			b->stat_queries_accepted++;
		else
			b->stat_queries_rejected++;
	}
	return i;
}

