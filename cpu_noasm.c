
#include "quakedef.h"

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	
//	if (!model || !model->nodes)
//		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	if (node->contents < 0)
		return (mleaf_t *)node;
	while (1)
	{
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];
		if (node->contents < 0)
			return (mleaf_t *)node;
	}
	
	return NULL;	// never reached
}
/*
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}
*/

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	while (num >= 0)
		num = hull->clipnodes[num].children[(hull->planes[hull->clipnodes[num].planenum].type < 3 ? p[hull->planes[hull->clipnodes[num].planenum].type] : DotProduct (hull->planes[hull->clipnodes[num].planenum].normal, p)) < hull->planes[hull->clipnodes[num].planenum].dist];
	
	return num;
}
