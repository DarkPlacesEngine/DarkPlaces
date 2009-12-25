
// Shadow Volume BSP code written by Forest "LordHavoc" Hale on 2003-11-06 and placed into public domain.
// Modified by LordHavoc (to make it work and other nice things like that) on 2007-01-24 and 2007-01-25
// Optimized by LordHavoc on 2009-12-24 and 2009-12-25

#include <math.h>
#include <string.h>
#include "svbsp.h"
#include "polygon.h"

#define MAX_SVBSP_POLYGONPOINTS 64
#define SVBSP_CLIP_EPSILON (1.0f / 1024.0f)

#define SVBSP_DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])

typedef struct svbsp_polygon_s
{
	float points[MAX_SVBSP_POLYGONPOINTS][3];
	//unsigned char splitflags[MAX_SVBSP_POLYGONPOINTS];
	int facesplitflag;
	int numpoints;
}
svbsp_polygon_t;

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

static void SVBSP_DividePolygon(int innumpoints, const float *inpoints, float planenormalx, float planenormaly, float planenormalz, float planedist, float epsilon, int outfrontmaxpoints, float *outfrontpoints, int *neededfrontpoints, int outbackmaxpoints, float *outbackpoints, int *neededbackpoints, int *oncountpointer)
{
	int i, frontcount = 0, backcount = 0, oncount = 0;
	const float *n, *p;
	float frac, pdist, ndist;
	if (innumpoints)
	{
		n = inpoints;
		ndist = n[0] * planenormalx + n[1] * planenormaly + n[2] * planenormalz - planedist;
		for(i = 0;i < innumpoints;i++)
		{
			p = n;
			pdist = ndist;
			n = inpoints + ((i + 1) < innumpoints ? (i + 1) : 0) * 3;
			ndist = n[0] * planenormalx + n[1] * planenormaly + n[2] * planenormalz - planedist;
			if (pdist >= -epsilon)
			{
				if (pdist <= epsilon)
					oncount++;
				if (frontcount < outfrontmaxpoints)
				{
					*outfrontpoints++ = p[0];
					*outfrontpoints++ = p[1];
					*outfrontpoints++ = p[2];
				}
				frontcount++;
			}
			if (pdist <= epsilon)
			{
				if (backcount < outbackmaxpoints)
				{
					*outbackpoints++ = p[0];
					*outbackpoints++ = p[1];
					*outbackpoints++ = p[2];
				}
				backcount++;
			}
			if ((pdist > epsilon && ndist < -epsilon) || (pdist < -epsilon && ndist > epsilon))
			{
				oncount++;
				frac = pdist / (pdist - ndist);
				if (frontcount < outfrontmaxpoints)
				{
					*outfrontpoints++ = p[0] + frac * (n[0] - p[0]);
					*outfrontpoints++ = p[1] + frac * (n[1] - p[1]);
					*outfrontpoints++ = p[2] + frac * (n[2] - p[2]);
				}
				frontcount++;
				if (backcount < outbackmaxpoints)
				{
					*outbackpoints++ = p[0] + frac * (n[0] - p[0]);
					*outbackpoints++ = p[1] + frac * (n[1] - p[1]);
					*outbackpoints++ = p[2] + frac * (n[2] - p[2]);
				}
				backcount++;
			}
		}
	}
	if (neededfrontpoints)
		*neededfrontpoints = frontcount;
	if (neededbackpoints)
		*neededbackpoints = backcount;
	if (oncountpointer)
		*oncountpointer = oncount;
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

static void SVBSP_InsertOccluderPolygonNodes(svbsp_t *b, int *parentnodenumpointer, int parentnodenum, const svbsp_polygon_t *poly, void (*fragmentcallback)(void *fragmentcallback_pointer1, int fragmentcallback_number1, svbsp_t *b, int numpoints, const float *points), void *fragmentcallback_pointer1, int fragmentcallback_number1)
{
	// now we need to create up to numpoints + 1 new nodes, forming a BSP tree
	// describing the occluder polygon's shadow volume
	int i, j, p, basenum;
	svbsp_node_t *node;

	// if there aren't enough nodes remaining, skip it
	if (b->numnodes + poly->numpoints + 1 >= b->maxnodes)
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
	for (i = 0, p = poly->numpoints - 1;i < poly->numpoints;p = i, i++)
	{
#if 1
		// see if a parent plane describes this side
		for (j = parentnodenum;j >= 0;j = b->nodes[j].parent)
		{
			float *parentnodeplane = b->nodes[j].plane;
			if (fabs(SVBSP_DotProduct(poly->points[p], parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
			 && fabs(SVBSP_DotProduct(poly->points[i], parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON
			 && fabs(SVBSP_DotProduct(b->origin      , parentnodeplane) - parentnodeplane[3]) < SVBSP_CLIP_EPSILON)
				break;
		}
		if (j >= 0)
			continue; // already have a matching parent plane
#endif
#if 0
		// skip any sides that were classified as belonging to a parent plane
		if (poly->splitflags[i])
			continue;
#endif
		// create a side plane
		// anything infront of this is not inside the shadow volume
		node = b->nodes + b->numnodes++;
		SVBSP_PlaneFromPoints(node->plane, b->origin, poly->points[p], poly->points[i]);
		// we need to flip the plane if it puts any part of the polygon on the
		// wrong side
		// (in this way this code treats all polygons as float sided)
		//
		// because speed is important this stops as soon as it finds proof
		// that the orientation is right or wrong
		// (we know that the plane is on one edge of the polygon, so there is
		// never a case where points lie on both sides, so the first hint is
		// sufficient)
		for (j = 0;j < poly->numpoints;j++)
		{
			float d = SVBSP_DotProduct(poly->points[j], node->plane) - node->plane[3];
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
	// skip the face plane if it lies on a parent plane
	if (!poly->facesplitflag)
#endif
	{
		// add the face-plane node
		// infront is empty, behind is shadow
		node = b->nodes + b->numnodes++;
		SVBSP_PlaneFromPoints(node->plane, poly->points[0], poly->points[1], poly->points[2]);
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

static int SVBSP_AddPolygonNode(svbsp_t *b, int *parentnodenumpointer, int parentnodenum, const svbsp_polygon_t *poly, int insertoccluder, void (*fragmentcallback)(void *fragmentcallback_pointer1, int fragmentcallback_number1, svbsp_t *b, int numpoints, const float *points), void *fragmentcallback_pointer1, int fragmentcallback_number1)
{
	int i;
	int countonpoints;
	int facesplitflag = poly->facesplitflag;
	float plane[4];
	svbsp_polygon_t front;
	svbsp_polygon_t back;
	svbsp_node_t *node;
	if (poly->numpoints < 1)
		return 0;
	// recurse through plane nodes
	while (*parentnodenumpointer >= 0)
	{
		// do a quick check to see if there is any need to split the polygon
		node = b->nodes + *parentnodenumpointer;
		parentnodenum = *parentnodenumpointer;
		plane[0] = node->plane[0];
		plane[1] = node->plane[1];
		plane[2] = node->plane[2];
		plane[3] = node->plane[3];
#if 1
		{
			int onback;
			int onfront;
			float d;
			onback = 0;
			onfront = 0;
			for (i = 0;i < poly->numpoints;i++)
			{
				d = SVBSP_DotProduct(poly->points[i], plane) - plane[3];
				if (d <= -SVBSP_CLIP_EPSILON)
					onback = 1;
				if (d >= SVBSP_CLIP_EPSILON)
					onfront = 1;
			}
			if (onfront)
			{
				if (!onback)
				{
					// no need to split, just go to one side
					parentnodenumpointer = &node->children[0];
					continue;
				}
			}
			else if (onback)
			{
				// no need to split, just go to one side
				parentnodenumpointer = &node->children[1];
				continue;
			}
			else
			{
				// no need to split, this polygon is on the plane
				// this case only occurs for polygons on the face plane, usually
				// the same polygon (inserted twice - once as occluder, once as
				// tester)
				// if this is an occluder, it is redundant
				if (insertoccluder)
					return 1; // occluded
				// if this is a tester, test the front side, because it is
				// probably the same polygon that created this node...
				facesplitflag = 1;
				parentnodenumpointer = &node->children[0];
				continue;
			}
		}
		// at this point we know it crosses the plane, so we need to split it
		SVBSP_DividePolygon(poly->numpoints, poly->points[0], plane[0], plane[1], plane[2], plane[3], SVBSP_CLIP_EPSILON, MAX_SVBSP_POLYGONPOINTS, front.points[0], &front.numpoints, MAX_SVBSP_POLYGONPOINTS, back.points[0], &back.numpoints, &countonpoints);
#else
		// at this point we know it crosses the plane, so we need to split it
		SVBSP_DividePolygon(poly->numpoints, poly->points[0], plane[0], plane[1], plane[2], plane[3], SVBSP_CLIP_EPSILON, MAX_SVBSP_POLYGONPOINTS, front.points[0], &front.numpoints, MAX_SVBSP_POLYGONPOINTS, back.points[0], &back.numpoints, &countonpoints);
		if (countonpoints == poly->numpoints)
		{
			// polygon lies on plane - this only occurs on face planes
			// if it is an occluder, it is redundant
			if (insertoccluder)
				return 1; // occluded
			// if it is a tester, it should take the front side only
			facesplitflag = 1;
			parentnodenumpointer = &node->children[0];
			continue;
		}
#endif
		if (front.numpoints > MAX_SVBSP_POLYGONPOINTS)
			front.numpoints = MAX_SVBSP_POLYGONPOINTS;
		front.facesplitflag = facesplitflag;
		if (back.numpoints > MAX_SVBSP_POLYGONPOINTS)
			back.numpoints = MAX_SVBSP_POLYGONPOINTS;
		back.facesplitflag = facesplitflag;
#if 0
		if (insertoccluder)
		{
			int pnum;
			int f;
			int pf;
			svbsp_node_t *pnode;
			// set splitflags based on this node and parent nodes
			for (i = 0;i < front.numpoints;i++)
				front.splitflags[i] = 0;
			for (i = 0;i < front.numpoints;i++)
				back.splitflags[i] = 0;
			pnum = *parentnodenumpointer;
			while (pnum >= 0)
			{
				pnode = b->nodes + pnum;
				plane[0] = pnode->plane[0];
				plane[1] = pnode->plane[1];
				plane[2] = pnode->plane[2];
				plane[3] = pnode->plane[3];
				if (front.numpoints)
				{
					i = front.numpoints-1;
					pf = fabs(SVBSP_DotProduct(front.points[i], plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
					for (i = 0;i < front.numpoints;pf = f, i++)
					{
						f = fabs(SVBSP_DotProduct(front.points[i], plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
						front.splitflags[i] |= (f & pf);
					}
				}
				if (back.numpoints)
				{
					i = back.numpoints-1;
					pf = fabs(SVBSP_DotProduct(back.points[i], plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
					for (i = 0;i < back.numpoints;pf = f, i++)
					{
						f = fabs(SVBSP_DotProduct(back.points[i], plane) - plane[3]) <= SVBSP_CLIP_EPSILON;
						back.splitflags[i] |= (f & pf);
					}
				}
				pnum = pnode->parent;
			}
		}
#endif
		// recurse the sides and return the resulting occlusion flags
		i  = SVBSP_AddPolygonNode(b, &node->children[0], *parentnodenumpointer, &front, insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		i |= SVBSP_AddPolygonNode(b, &node->children[1], *parentnodenumpointer, &back , insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		return i;
	}
	// leaf node
	if (*parentnodenumpointer == -1)
	{
		// empty leaf node; and some geometry survived
		// if inserting an occluder, replace this empty leaf with a shadow volume
#if 0
		for (i = 0;i < poly->numpoints-2;i++)
		{
			Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
			Debug_PolygonVertex(poly->points[  0][0], poly->points[  0][1], poly->points[  0][2], 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f);
			Debug_PolygonVertex(poly->points[i+1][0], poly->points[i+1][1], poly->points[i+1][2], 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f);
			Debug_PolygonVertex(poly->points[i+2][0], poly->points[i+2][1], poly->points[i+2][2], 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f);
			Debug_PolygonEnd();
		}
#endif
		if (insertoccluder)
		{
			b->stat_occluders_fragments_accepted++;
			SVBSP_InsertOccluderPolygonNodes(b, parentnodenumpointer, parentnodenum, poly, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
		}
		else
			b->stat_queries_fragments_accepted++;
		if (fragmentcallback)
			fragmentcallback(fragmentcallback_pointer1, fragmentcallback_number1, b, poly->numpoints, poly->points[0]);
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
		for (i = 0;i < poly->numpoints-2;i++)
		{
			Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
			Debug_PolygonVertex(poly->points[  0][0], poly->points[  0][1], poly->points[  0][2], 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 1.0f);
			Debug_PolygonVertex(poly->points[i+1][0], poly->points[i+1][1], poly->points[i+1][2], 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 1.0f);
			Debug_PolygonVertex(poly->points[i+2][0], poly->points[i+2][1], poly->points[i+2][2], 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 1.0f);
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
	svbsp_polygon_t poly;
	// don't even consider an empty polygon
	// note we still allow points and lines to be tested...
	if (numpoints < 1)
		return 0;
	poly.numpoints = numpoints;
	for (i = 0;i < numpoints;i++)
	{
		poly.points[i][0] = points[i*3+0];
		poly.points[i][1] = points[i*3+1];
		poly.points[i][2] = points[i*3+2];
		//poly.splitflags[i] = 0; // this edge is a valid BSP splitter - clipped edges are not (because they lie on a bsp plane)
		poly.facesplitflag = 0; // this face is a valid BSP Splitter - if it lies on a bsp plane it is not
	}
#if 0
//if (insertoccluder)
	for (i = 0;i < poly.numpoints-2;i++)
	{
		Debug_PolygonBegin(NULL, DRAWFLAG_ADDITIVE);
		Debug_PolygonVertex(poly.points[  0][0], poly.points[  0][1], poly.points[  0][2], 0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 1.0f);
		Debug_PolygonVertex(poly.points[i+1][0], poly.points[i+1][1], poly.points[i+1][2], 0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 1.0f);
		Debug_PolygonVertex(poly.points[i+2][0], poly.points[i+2][1], poly.points[i+2][2], 0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 1.0f);
		Debug_PolygonEnd();
	}
#endif
	nodenum = 0;
	i = SVBSP_AddPolygonNode(b, &nodenum, -1, &poly, insertoccluder, fragmentcallback, fragmentcallback_pointer1, fragmentcallback_number1);
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

