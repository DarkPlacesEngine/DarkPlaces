#include "quakedef.h"

transvert_t *transvert;
transpoly_t *transpoly;
unsigned short *transpolyindex;
wallvert_t *wallvert;
wallpoly_t *wallpoly;
skyvert_t *skyvert;
skypoly_t *skypoly;

unsigned short currenttranspoly;
unsigned short currenttransvert;
unsigned short currentwallpoly;
unsigned short currentwallvert;
unsigned short currentskypoly;
unsigned short currentskyvert;

cvar_t gl_multitexture = {"gl_multitexture", "1"};
cvar_t gl_vertexarrays = {"gl_vertexarrays", "1"};

typedef struct translistitem_s
{
	transpoly_t *poly;
	struct translistitem_s *next;
}
translistitem;

translistitem translist[MAX_TRANSPOLYS];
translistitem *currenttranslist;

translistitem *translisthash[4096];

float transviewdist; // distance of view origin along the view normal

float transreciptable[256];

void glpoly_init()
{
	int i;
	Cvar_RegisterVariable (&gl_multitexture);
	Cvar_RegisterVariable (&gl_vertexarrays);
	transvert = malloc(MAX_TRANSVERTS * sizeof(transvert_t));
	transpoly = malloc(MAX_TRANSPOLYS * sizeof(transpoly_t));
	transpolyindex = malloc(MAX_TRANSPOLYS * sizeof(unsigned short));
	wallvert = malloc(MAX_WALLVERTS * sizeof(wallvert_t));
	wallpoly = malloc(MAX_WALLPOLYS * sizeof(wallpoly_t));
	skyvert = malloc(MAX_SKYVERTS * sizeof(skyvert_t));
	skypoly = malloc(MAX_SKYPOLYS * sizeof(skypoly_t));
	transreciptable[0] = 0.0f;
	for (i = 1;i < 256;i++)
		transreciptable[i] = 1.0f / i;
}

void transpolyclear()
{
	currenttranspoly = currenttransvert = 0;
	currenttranslist = translist;
	memset(translisthash, 0, sizeof(translisthash));
	transviewdist = DotProduct(r_refdef.vieworg, vpn);
}

void transpolybegin(int texnum, int glowtexnum, int fogtexnum, int transpolytype)
{
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	transpoly[currenttranspoly].texnum = (unsigned short) texnum;
	transpoly[currenttranspoly].glowtexnum = (unsigned short) glowtexnum;
	transpoly[currenttranspoly].fogtexnum = (unsigned short) fogtexnum;
	transpoly[currenttranspoly].transpolytype = (unsigned short) transpolytype;
	transpoly[currenttranspoly].firstvert = currenttransvert;
	transpoly[currenttranspoly].verts = 0;
//	transpoly[currenttranspoly].ndist = 0; // clear the normal
}

// turned into a #define
/*
void transpolyvert(float x, float y, float z, float s, float t, int r, int g, int b, int a)
{
	int i;
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	transvert[currenttransvert].s = s;
	transvert[currenttransvert].t = t;
	transvert[currenttransvert].r = bound(0, r, 255);
	transvert[currenttransvert].g = bound(0, g, 255);
	transvert[currenttransvert].b = bound(0, b, 255);
	transvert[currenttransvert].a = bound(0, a, 255);
	transvert[currenttransvert].v[0] = x;
	transvert[currenttransvert].v[1] = y;
	transvert[currenttransvert].v[2] = z;
	currenttransvert++;
	transpoly[currenttranspoly].verts++;
}
*/

void transpolyend()
{
	float center, d, maxdist;
	int i;
	transvert_t *v;
	if (currenttranspoly >= MAX_TRANSPOLYS || currenttransvert >= MAX_TRANSVERTS)
		return;
	if (transpoly[currenttranspoly].verts < 3) // skip invalid polygons
	{
		currenttransvert = transpoly[currenttranspoly].firstvert; // reset vert pointer
		return;
	}
	center = 0;
	maxdist = -1000000000000000.0f; // eh, it's definitely behind it, so...
	for (i = 0,v = &transvert[transpoly[currenttranspoly].firstvert];i < transpoly[currenttranspoly].verts;i++, v++)
	{
		d = DotProduct(v->v, vpn);
		center += d;
		if (d > maxdist)
			maxdist = d;
	}
	maxdist -= transviewdist;
	if (maxdist < 4.0f) // behind view
	{
		currenttransvert = transpoly[currenttranspoly].firstvert; // reset vert pointer
		return;
	}
	center *= transreciptable[transpoly[currenttranspoly].verts];
	center -= transviewdist;
	i = bound(0, (int) center, 4095);
	currenttranslist->next = translisthash[i];
	currenttranslist->poly = transpoly + currenttranspoly;
	translisthash[i] = currenttranslist;
	currenttranslist++;
	currenttranspoly++;
}

int transpolyindices;
extern qboolean isG200;

/*
void transpolyrenderminmax()
{
	int i, j, k, lastvert;
	vec_t d, min, max, viewdist, s, average;
	//vec_t ndist;
	//vec3_t v1, v2, n;
	transpolyindices = 0;
	viewdist = DotProduct(r_refdef.vieworg, vpn);
	for (i = 0;i < currenttranspoly;i++)
	{
		if (transpoly[i].verts < 3) // only process valid polygons
			continue;
		min = 1000000;max = -1000000;
		s = 1.0f / transpoly[i].verts;
		lastvert = transpoly[i].firstvert + transpoly[i].verts;
		average = 0;
		for (j = transpoly[i].firstvert;j < lastvert;j++)
		{
			d = DotProduct(transvert[j].v, vpn)-viewdist;
			if (d < min) min = d;
			if (d > max) max = d;
			average += d * s;
		}
		if (max < 4) // free to check here, so skip polys behind the view
			continue;
		transpoly[i].distance = average;
*/
		/*
		transpoly[i].mindistance = min;
		transpoly[i].maxdistance = max;
		// calculate normal (eek)
		VectorSubtract(transvert[transpoly[i].firstvert  ].v, transvert[transpoly[i].firstvert+1].v, v1);
		VectorSubtract(transvert[transpoly[i].firstvert+2].v, transvert[transpoly[i].firstvert+1].v, v2);
		VectorNormalize(v1);
		VectorNormalize(v2);
		if (transpoly[i].verts > 3 && fabs(DotProduct(v1, v2)) >= (1.0f - (1.0f / 256.0f))) // colinear edges, find a better triple
		{
			VectorSubtract(transvert[transpoly[i].firstvert + transpoly[i].verts - 1].v, transvert[transpoly[i].firstvert].v, v1);
			VectorSubtract(transvert[transpoly[i].firstvert + 1].v, transvert[transpoly[i].firstvert].v, v2);
			VectorNormalize(v1);
			VectorNormalize(v2);
			if (fabs(DotProduct(v1, v2)) < (1.0f - (1.0f / 256.0f))) // found a good triple
				goto foundtriple;
			for (k = transpoly[i].firstvert + 2;k < (transpoly[i].firstvert + transpoly[i].verts - 1);k++)
			{
				VectorSubtract(transvert[k-1].v, transvert[k].v, v1);
				VectorSubtract(transvert[k+1].v, transvert[k].v, v2);
				VectorNormalize(v1);
				VectorNormalize(v2);
				if (fabs(DotProduct(v1, v2)) < (1.0f - (1.0f / 256.0f))) // found a good triple
					goto foundtriple;
			}
			VectorSubtract(transvert[k-1].v, transvert[k].v, v1);
			VectorSubtract(transvert[transpoly[i].firstvert].v, transvert[k].v, v2);
			VectorNormalize(v1);
			VectorNormalize(v2);
			if (fabs(DotProduct(v1, v2)) >= (1.0f - (1.0f / 256.0f))) // no good triples; the polygon is a line, skip it
				continue;
		}
foundtriple:
		CrossProduct(v1, v2, n);
		VectorNormalize(n);
		ndist = DotProduct(transvert[transpoly[i].firstvert+1].v, n);
		// sorted insert
		for (j = 0;j < transpolyindices;j++)
		{
			// easy cases
			if (transpoly[transpolyindex[j]].mindistance > max)
				continue;
			if (transpoly[transpolyindex[j]].maxdistance < min)
				break;
			// hard case, check side
			for (k = transpoly[transpolyindex[j]].firstvert;k < (transpoly[transpolyindex[j]].firstvert + transpoly[transpolyindex[j]].verts);k++)
				if (DotProduct(transvert[k].v, n) < ndist)
					goto skip;
			break;
skip:
			;
		}
		*/
/*
		// sorted insert
		for (j = 0;j < transpolyindices;j++)
			if (transpoly[transpolyindex[j]].distance < average)
				break;
		for (k = transpolyindices;k > j;k--)
			transpolyindex[k] = transpolyindex[k-1];
		transpolyindices++;
		transpolyindex[j] = i;
	}
}
*/
/*
// LordHavoc: qsort compare function
int transpolyqsort(const void *ia, const void *ib)
{
	transpoly_t *a, *b;
	int i, j;
	a = &transpoly[*((unsigned short *)ia)];
	b = &transpoly[*((unsigned short *)ib)];
	// easy cases
	if (a->mindistance > b->mindistance && a->maxdistance > b->maxdistance)
		return -1; // behind
	if (a->mindistance < b->mindistance && a->maxdistance < b->maxdistance)
		return 1; // infront
	// hard case
	if (!a->ndist)
	{
		// calculate normal (eek)
		vec3_t v1, v2;
		VectorSubtract(transvert[a->firstvert  ].v, transvert[a->firstvert+1].v, v1);
		VectorSubtract(transvert[a->firstvert+2].v, transvert[a->firstvert+1].v, v2);
		CrossProduct(v1, v2, a->n);
		VectorNormalize(a->n);
		a->ndist = DotProduct(transvert[a->firstvert  ].v, a->n);
	}
	// check side
	for (i = b->firstvert, j = 0;i < (b->firstvert + b->verts);i++)
		j += DotProduct(transvert[i].v, a->n) < a->ndist; // (1) b is infront of a
	if (j == 0)
		return -1; // (-1) a is behind b
	return j == b->verts; // (1) a is infront of b    (0) a and b intersect
//	return (transpoly[*((unsigned short *)ib)].mindistance + transpoly[*((unsigned short *)ib)].maxdistance) - (transpoly[*((unsigned short *)ia)].mindistance + transpoly[*((unsigned short *)ia)].maxdistance);
	*/
/*
	return ((transpoly_t*)ia)->distance - ((transpoly_t*)ib)->distance;
}
*/

/*
int transpolyqsort(const void *ia, const void *ib)
{
	return (transpoly[*((unsigned short *)ib)].distance - transpoly[*((unsigned short *)ia)].distance);
}
*/

/*
void transpolyrenderminmax()
{
	int i, j, lastvert;
	vec_t d, max, viewdist, average;
	transpolyindices = 0;
	viewdist = DotProduct(r_refdef.vieworg, vpn);
	for (i = 0;i < currenttranspoly;i++)
	{
		if (transpoly[i].verts < 3) // only process valid polygons
			continue;
		max = -1000000;
		lastvert = transpoly[i].firstvert + transpoly[i].verts;
		average = 0;
		for (j = transpoly[i].firstvert;j < lastvert;j++)
		{
			d = DotProduct(transvert[j].v, vpn)-viewdist;
			average += d;
			if (d > max)
				max = d;
		}
		if (max < 4) // free to check here, so skip polys behind the view
			continue;
		transpoly[i].distance = average / transpoly[i].verts;
		transpolyindex[transpolyindices++] = i;
	}
	qsort(&transpolyindex[0], transpolyindices, sizeof(unsigned short), transpolyqsort);
}
*/
/*
	int i, j, a;
	a = true;
	while(a)
	{
		a = false;
		for (i = 1;i < transpolyindices;i++)
		{
			// easy cases
			if (transpoly[transpolyindex[i - 1]].mindistance > transpoly[transpolyindex[i]].mindistance && transpoly[transpolyindex[i - 1]].maxdistance > transpoly[transpolyindex[i]].maxdistance)
				continue; // previous is behind (no swap)
			if (transpoly[transpolyindex[i - 1]].mindistance < transpoly[transpolyindex[i]].mindistance && transpoly[transpolyindex[i - 1]].maxdistance < transpoly[transpolyindex[i]].maxdistance)
				goto swap; // previous is infront (swap)
			// hard case
*/
			/*
			if (!transpoly[transpolyindex[i - 1]].ndist)
			{
				// calculate normal (eek)
				vec3_t v1, v2;
				VectorSubtract(transvert[transpoly[transpolyindex[i - 1]].firstvert  ].v, transvert[transpoly[transpolyindex[i - 1]].firstvert+1].v, v1);
				VectorSubtract(transvert[transpoly[transpolyindex[i - 1]].firstvert+2].v, transvert[transpoly[transpolyindex[i - 1]].firstvert+1].v, v2);
				CrossProduct(v1, v2, transpoly[transpolyindex[i - 1]].n);
				VectorNormalize(transpoly[transpolyindex[i - 1]].n);
				transpoly[transpolyindex[i - 1]].ndist = DotProduct(transvert[transpoly[transpolyindex[i - 1]].firstvert  ].v, transpoly[transpolyindex[i - 1]].n);
			}
			if (DotProduct(transpoly[transpolyindex[i - 1]].n, vpn) >= 0.0f) // backface
				continue;
			*/
/*
			// check side
			for (i = transpoly[transpolyindex[i]].firstvert;i < (transpoly[transpolyindex[i]].firstvert + transpoly[transpolyindex[i]].verts);i++)
				if (DotProduct(transvert[i].v, transpoly[transpolyindex[i - 1]].n) >= transpoly[transpolyindex[i - 1]].ndist)
					goto noswap; // previous is behind or they intersect
swap:
			// previous is infront (swap)
			j = transpolyindex[i];
			transpolyindex[i] = transpolyindex[i - 1];
			transpolyindex[i - 1] = j;
			a = true;
noswap:
			;
		}
	}
}
*/

void transpolyrender()
{
	int i, j, tpolytype, texnum;
	transpoly_t *p;
	if (currenttranspoly < 1)
		return;
//	transpolyrenderminmax();
//	if (transpolyindices < 1)
//		return;
	// testing
//	Con_DPrintf("transpolyrender: %i polys %i infront %i vertices\n", currenttranspoly, transpolyindices, currenttransvert);
//	if (transpolyindices >= 2)
//		transpolysort();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	glDepthMask(0); // disable zbuffer updates
	if (isG200) // Matrox G200 cards can't handle per pixel alpha
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
	// later note: wasn't working on my TNT drivers...  strangely...  used a cheaper hack in transpolyvert
	//// offset by 16 depth units so decal sprites appear infront of walls
	//glPolygonOffset(1, -16);
	//glEnable(GL_POLYGON_OFFSET_FILL);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tpolytype = TPOLYTYPE_ALPHA;
	texnum = -1;
	/*
	if (gl_vertexarrays.value)
	{
		// set up the vertex array
		qglInterleavedArrays(GL_T2F_C4UB_V3F, 0, transvert);
		for (i = 0;i < transpolyindices;i++)
		{
			p = &transpoly[transpolyindex[i]];
			if (p->texnum != texnum || p->transpolytype != tpolytype)
			{
				if (p->texnum != texnum)
				{
					texnum = p->texnum;
					glBindTexture(GL_TEXTURE_2D, texnum);
				}
				if (p->transpolytype != tpolytype)
				{
					tpolytype = p->transpolytype;
					if (tpolytype == TPOLYTYPE_ADD) // additive
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					else // alpha
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
			}
			qglDrawArrays(GL_TRIANGLE_FAN, p->firstvert, p->verts);
			if (p->glowtexnum)
			{
				texnum = p->glowtexnum; // highly unlikely to match next poly, but...
				glBindTexture(GL_TEXTURE_2D, texnum);
				tpolytype = TPOLYTYPE_ADD; // might match next poly
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				qglDrawArrays(GL_TRIANGLE_FAN, p->firstvert, p->verts);
			}
		}
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	*/
	{
		int points = -1;
		translistitem *item;
		transvert_t *vert;
		for (i = 4095;i >= 0;i--)
		{
			item = translisthash[i];
			while (item)
			{
				p = item->poly;
				item = item->next;
				if (p->texnum != texnum || p->verts != points || p->transpolytype != tpolytype)
				{
					glEnd();
					if (isG200)
					{
						if (p->fogtexnum) // alpha
							glEnable(GL_ALPHA_TEST);
						else
							glDisable(GL_ALPHA_TEST);
					}
					if (p->texnum != texnum)
					{
						texnum = p->texnum;
						glBindTexture(GL_TEXTURE_2D, texnum);
					}
					if (p->transpolytype != tpolytype)
					{
						tpolytype = p->transpolytype;
						if (tpolytype == TPOLYTYPE_ADD) // additive
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						else // alpha
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					points = p->verts;
					switch (points)
					{
					case 3:
						glBegin(GL_TRIANGLES);
						break;
					case 4:
						glBegin(GL_QUADS);
						break;
					default:
						glBegin(GL_TRIANGLE_FAN);
						points = -1; // to force a reinit on the next poly
						break;
					}
				}
				for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
				{
					// would be 2fv, but windoze Matrox G200 and probably G400 drivers don't support that (dumb...)
					glTexCoord2f(vert->s, vert->t);
					// again, vector version isn't supported I think
					glColor4ub(vert->r, vert->g, vert->b, vert->a);
					glVertex3fv(vert->v);
				}
				if (p->glowtexnum)
				{
					glEnd();
					texnum = p->glowtexnum; // highly unlikely to match next poly, but...
					glBindTexture(GL_TEXTURE_2D, texnum);
					if (tpolytype != TPOLYTYPE_ADD)
					{
						tpolytype = TPOLYTYPE_ADD; // might match next poly
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					}
					points = -1;
					glBegin(GL_TRIANGLE_FAN);
					for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
					{
						glColor4ub(255,255,255,vert->a);
						// would be 2fv, but windoze Matrox G200 and probably G400 drivers don't support that (dumb...)
						glTexCoord2f(vert->s, vert->t);
						glVertex3fv(vert->v);
					}
					glEnd();
				}
				if (fogenabled && p->transpolytype == TPOLYTYPE_ALPHA)
				{
					vec3_t diff;
					glEnd();
					points = -1; // to force a reinit on the next poly
					if (tpolytype != TPOLYTYPE_ALPHA)
					{
						tpolytype = TPOLYTYPE_ALPHA; // probably matchs next poly
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					if (p->fogtexnum)
					{
						if (texnum != p->fogtexnum) // highly unlikely to match next poly, but...
						{
							texnum = p->fogtexnum;
							glBindTexture(GL_TEXTURE_2D, texnum);
						}
						glBegin(GL_TRIANGLE_FAN);
						for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
						{
							VectorSubtract(vert->v, r_refdef.vieworg,diff);
							glTexCoord2f(vert->s, vert->t);
							glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], vert->a*(1.0f/255.0f)*exp(fogdensity/DotProduct(diff,diff)));
							glVertex3fv(vert->v);
						}
						glEnd ();
					}
					else
					{
						glDisable(GL_TEXTURE_2D);
						glBegin(GL_TRIANGLE_FAN);
						for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
						{
							VectorSubtract(vert->v, r_refdef.vieworg,diff);
							glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], vert->a*(1.0f/255.0f)*exp(fogdensity/DotProduct(diff,diff)));
							glVertex3fv(vert->v);
						}
						glEnd ();
						glEnable(GL_TEXTURE_2D);
					}
				}
			}
		}
		glEnd();
	}

	//glDisable(GL_POLYGON_OFFSET_FILL);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1); // enable zbuffer updates
	glDisable(GL_ALPHA_TEST);
}

/*
void lightpolybegin(int texnum)
{
	if (currentlightpoly >= MAX_LIGHTPOLYS || currentlightvert >= MAX_LIGHTVERTS)
		return;
	lightpoly[currentlightpoly].texnum = (unsigned short) texnum;
	lightpoly[currentlightpoly].firstvert = currentlightvert;
	lightpoly[currentlightpoly].verts = 0;
}

// lightpolyvert is a #define

void lightpolyend()
{
	if (currentlightpoly >= MAX_LIGHTPOLYS)
		return;
	if (lightpoly[currentlightpoly].verts < 3) // skip invalid polygons
	{
		currentlightvert = lightpoly[currentlightpoly].firstvert; // reset vert pointer
		return;
	}
	if (currentlightvert >= MAX_LIGHTVERTS)
		return;
	currentlightpoly++;
}
*/

extern qboolean isG200;

void wallpolyclear()
{
	currentwallpoly = currentwallvert = 0;
}

extern qboolean lighthalf;
void wallpolyrender()
{
	int i, j, texnum, lighttexnum;
	wallpoly_t *p;
	wallvert_t *vert;
	if (currentwallpoly < 1)
		return;
	c_brush_polys += currentwallpoly;
	// testing
	//Con_DPrintf("wallpolyrender: %i polys %i vertices\n", currentwallpoly, currentwallvert);
	if (!gl_mtexable)
		gl_multitexture.value = 0;
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_FLAT);
	// make sure zbuffer is enabled
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDepthMask(1);
	glColor3f(1,1,1);
	if (r_fullbright.value) // LordHavoc: easy to do fullbright...
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		texnum = -1;
		for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
		{
			if (p->texnum != texnum)
			{
				texnum = p->texnum;
				glBindTexture(GL_TEXTURE_2D, texnum);
			}
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				glTexCoord2f (vert->s, vert->t);
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
	}
	else if (gl_multitexture.value)
	{
		qglSelectTexture(gl_mtex_enum+0);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_TEXTURE_2D);
		qglSelectTexture(gl_mtex_enum+1);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_TEXTURE_2D);
		texnum = -1;
		lighttexnum = -1;
		for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
		{
			if (p->texnum != texnum || p->lighttexnum != lighttexnum)
			{
				texnum = p->texnum;
				lighttexnum = p->lighttexnum;
				qglSelectTexture(gl_mtex_enum+0);
				glBindTexture(GL_TEXTURE_2D, texnum);
				qglSelectTexture(gl_mtex_enum+1);
				glBindTexture(GL_TEXTURE_2D, lighttexnum);
			}
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				qglMTexCoord2f(gl_mtex_enum, vert->s, vert->t); // texture
				qglMTexCoord2f((gl_mtex_enum+1), vert->u, vert->v); // lightmap
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}

		qglSelectTexture(gl_mtex_enum+1);
		glDisable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglSelectTexture(gl_mtex_enum+0);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		// first do the textures
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		texnum = -1;
		for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
		{
			if (p->texnum != texnum)
			{
				texnum = p->texnum;
				glBindTexture(GL_TEXTURE_2D, texnum);
			}
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				glTexCoord2f (vert->s, vert->t);
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
		// then modulate using the lightmaps
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		glEnable(GL_BLEND);
		texnum = -1;
		for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
		{
			if (p->lighttexnum != texnum)
			{
				texnum = p->lighttexnum;
				glBindTexture(GL_TEXTURE_2D, texnum);
			}
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				glTexCoord2f (vert->u, vert->v);
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
	}
	// switch to additive mode settings
	glDepthMask(0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glShadeModel(GL_SMOOTH);
	// render vertex lit overlays ontop
	texnum = -1;
	for (i = 0, p = wallpoly;i < currentwallpoly;i++, p++)
	{
		if (!p->lit)
			continue;
		for (j = 0,vert = &wallvert[p->firstvert];j < p->numverts;j++, vert++)
			if (vert->r || vert->g || vert->b)
				goto lit;
		continue;
lit:
		c_light_polys++;
		if (p->texnum != texnum)
		{
			texnum = p->texnum;
			glBindTexture(GL_TEXTURE_2D, texnum);
		}
		glBegin(GL_POLYGON);
		for (j = 0,vert = &wallvert[p->firstvert];j < p->numverts;j++, vert++)
		{
			// would be 2fv, but windoze Matrox G200 and probably G400 drivers don't support that (dumb...)
			glTexCoord2f(vert->s, vert->t);
			// again, vector version isn't supported I think
			glColor3ub(vert->r, vert->g, vert->b);
			glVertex3fv(vert->vert);
		}
		glEnd();
	}
	// render glow textures
	glShadeModel(GL_FLAT);
	glBlendFunc(GL_ONE, GL_ONE);
	if (lighthalf)
		glColor3f(0.5,0.5,0.5);
	else
		glColor3f(1,1,1);
	texnum = -1;
	for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
	{
		if (!p->glowtexnum)
			continue;
		if (p->glowtexnum != texnum)
		{
			texnum = p->glowtexnum;
			glBindTexture(GL_TEXTURE_2D, texnum);
		}
		vert = &wallvert[p->firstvert];
		glBegin(GL_POLYGON);
		for (j=0 ; j<p->numverts ; j++, vert++)
		{
			glTexCoord2f (vert->s, vert->t);
			glVertex3fv (vert->vert);
		}
		glEnd();
	}
	glColor3f(1,1,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);
	if (fogenabled)
	{
		vec3_t diff;
		glDisable(GL_TEXTURE_2D);
		for (i = 0,p = &wallpoly[0];i < currentwallpoly;i++, p++)
		{
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				VectorSubtract(vert->vert, r_refdef.vieworg,diff);
				glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
		glEnable(GL_TEXTURE_2D);
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_BLEND);
	glDepthMask(1);
}

void skypolyclear()
{
	currentskypoly = currentskyvert = 0;
}

extern qboolean isATI;

extern char skyname[];
extern int solidskytexture, alphaskytexture;
void skypolyrender()
{
	int i, j;
	skypoly_t *p;
	skyvert_t *vert;
	float length, speedscale;
	vec3_t dir;
	if (currentskypoly < 1)
		return;
	// testing
//	Con_DPrintf("skypolyrender: %i polys %i vertices\n", currentskypoly, currentskyvert);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	// make sure zbuffer is enabled
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	if (!fogenabled && !skyname[0]) // normal quake sky
	{
		glColor3f(0.5f, 0.5f, 0.5f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture(GL_TEXTURE_2D, solidskytexture); // upper clouds
		speedscale = realtime*8;
		speedscale -= (int)speedscale & ~127 ;
		for (i = 0,p = &skypoly[0];i < currentskypoly;i++, p++)
		{
			vert = &skyvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->verts ; j++, vert++)
			{
				VectorSubtract (vert->v, r_origin, dir);
				dir[2] *= 3;	// flatten the sphere

				length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
				length = sqrt (length);
				length = 6*63/length;

				glTexCoord2f ((speedscale + dir[0] * length) * (1.0/128), (speedscale + dir[1] * length) * (1.0/128));
				glVertex3fv (vert->v);
			}
			glEnd ();
		}
		glEnable(GL_BLEND);
		glDepthMask(0);
		glBindTexture(GL_TEXTURE_2D, alphaskytexture); // lower clouds
		speedscale = realtime*16;
		speedscale -= (int)speedscale & ~127 ;
		for (i = 0,p = &skypoly[0];i < currentskypoly;i++, p++)
		{
			vert = &skyvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->verts ; j++, vert++)
			{
				VectorSubtract (vert->v, r_origin, dir);
				dir[2] *= 3;	// flatten the sphere

				length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
				length = sqrt (length);
				length = 6*63/length;

				glTexCoord2f ((speedscale + dir[0] * length) * (1.0/128), (speedscale + dir[1] * length) * (1.0/128));
				glVertex3fv (vert->v);
			}
			glEnd ();
		}
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
		glDepthMask(1);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor3fv(fogcolor); // note: gets rendered over by skybox if fog is not enabled
		for (i = 0,p = &skypoly[0];i < currentskypoly;i++, p++)
		{
			vert = &skyvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->verts ; j++, vert++)
				glVertex3fv (vert->v);
			glEnd ();
		}
		glColor3f(1,1,1);
		glEnable(GL_TEXTURE_2D);
	}
}
