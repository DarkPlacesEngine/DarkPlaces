#include "quakedef.h"

transvert_t *transvert;
transpoly_t *transpoly;
unsigned short *transpolyindex;
wallvert_t *wallvert;
wallvertcolor_t *wallvertcolor;
wallpoly_t *wallpoly;
skyvert_t *skyvert;
skypoly_t *skypoly;

int currenttranspoly;
int currenttransvert;
int currentwallpoly;
int currentwallvert;
int currentskypoly;
int currentskyvert;

void LoadSky_f(void);

cvar_t r_multitexture = {0, "r_multitexture", "1"};
cvar_t r_skyquality = {CVAR_SAVE, "r_skyquality", "2"};
cvar_t r_mergesky = {CVAR_SAVE, "r_mergesky", "0"};

static char skyworldname[1024];
static rtexture_t *mergeskytexture;
static rtexture_t *solidskytexture, *solidskytexture_half;
static rtexture_t *alphaskytexture, *alphaskytexture_half;
static qboolean skyavailable_quake;
static qboolean skyavailable_box;

void R_BuildSky (int scrollupper, int scrolllower);

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

void gl_poly_start(void)
{
	int i;
	transvert = qmalloc(MAX_TRANSVERTS * sizeof(transvert_t));
	transpoly = qmalloc(MAX_TRANSPOLYS * sizeof(transpoly_t));
	transpolyindex = qmalloc(MAX_TRANSPOLYS * sizeof(unsigned short));
	wallvert = qmalloc(MAX_WALLVERTS * sizeof(wallvert_t));
	wallvertcolor = qmalloc(MAX_WALLVERTS * sizeof(wallvertcolor_t));
	wallpoly = qmalloc(MAX_WALLPOLYS * sizeof(wallpoly_t));
	skyvert = qmalloc(MAX_SKYVERTS * sizeof(skyvert_t));
	skypoly = qmalloc(MAX_SKYPOLYS * sizeof(skypoly_t));
	transreciptable[0] = 0.0f;
	for (i = 1;i < 256;i++)
		transreciptable[i] = 1.0f / i;
}

void gl_poly_shutdown(void)
{
	qfree(transvert);
	qfree(transpoly);
	qfree(transpolyindex);
	qfree(wallvert);
	qfree(wallvertcolor);
	qfree(wallpoly);
	qfree(skyvert);
	qfree(skypoly);
}

void gl_poly_newmap(void)
{
	skyavailable_box = false;
	skyavailable_quake = false;
	if (!strcmp(skyworldname, cl.worldmodel->name))
		skyavailable_quake = true;
}

void GL_Poly_Init(void)
{
	Cmd_AddCommand ("loadsky", &LoadSky_f);
	Cvar_RegisterVariable (&r_multitexture);
	Cvar_RegisterVariable (&r_skyquality);
	Cvar_RegisterVariable (&r_mergesky);
	R_RegisterModule("GL_Poly", gl_poly_start, gl_poly_shutdown, gl_poly_newmap);
}

void transpolyclear(void)
{
	currenttranspoly = currenttransvert = 0;
	currenttranslist = translist;
	memset(translisthash, 0, sizeof(translisthash));
	transviewdist = DotProduct(r_origin, vpn);
}

// turned into a #define
/*
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
*/

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

void transpolyend(void)
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

void transpolyrender(void)
{
	int i, j, tpolytype, texnum;
	transpoly_t *p;
	if (!r_render.value)
		return;
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
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	tpolytype = TPOLYTYPE_ALPHA;
	texnum = -1;
	/*
	if (gl_vertexarrays.value)
	{
		// set up the vertex array
		glInterleavedArrays(GL_T2F_C4UB_V3F, 0, transvert);
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
			glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
			if (p->glowtexnum)
			{
				texnum = p->glowtexnum; // highly unlikely to match next poly, but...
				glBindTexture(GL_TEXTURE_2D, texnum);
				tpolytype = TPOLYTYPE_ADD; // might match next poly
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
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
						// LordHavoc: Matrox G200 cards can't handle per pixel alpha
						if (p->fogtexnum)
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
						glBegin(GL_POLYGON);
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
					glBegin(GL_POLYGON);
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
						glBegin(GL_POLYGON);
						for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
						{
							VectorSubtract(vert->v, r_origin, diff);
							glTexCoord2f(vert->s, vert->t);
							glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], vert->a*(1.0f/255.0f)*exp(fogdensity/DotProduct(diff,diff)));
							glVertex3fv(vert->v);
						}
						glEnd ();
					}
					else
					{
						glDisable(GL_TEXTURE_2D);
						glBegin(GL_POLYGON);
						for (j = 0,vert = &transvert[p->firstvert];j < p->verts;j++, vert++)
						{
							VectorSubtract(vert->v, r_origin, diff);
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

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1); // enable zbuffer updates
	glDisable(GL_ALPHA_TEST);
}

void wallpolyclear(void)
{
	currentwallpoly = currentwallvert = 0;
}

void wallpolyrender(void)
{
	int i, j, texnum, lighttexnum;
	wallpoly_t *p;
	wallvert_t *vert;
	wallvertcolor_t *vertcolor;
	if (!r_render.value)
		return;
	if (currentwallpoly < 1)
		return;
	c_brush_polys += currentwallpoly;
	// testing
	//Con_DPrintf("wallpolyrender: %i polys %i vertices\n", currentwallpoly, currentwallvert);
	if (!gl_mtexable)
		r_multitexture.value = 0;
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_FLAT);
	// make sure zbuffer is enabled
	glEnable(GL_DEPTH_TEST);
//	glDisable(GL_ALPHA_TEST);
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
				glTexCoord2f (vert->vert[3], vert->vert[4]);
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
	}
	else if (r_multitexture.value)
	{
		if (gl_combine.value)
		{
			qglActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);
			glEnable(GL_TEXTURE_2D);
			qglActiveTexture(GL_TEXTURE1_ARB);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 4.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);
			glEnable(GL_TEXTURE_2D);
		}
		else
		{
			qglActiveTexture(GL_TEXTURE0_ARB);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glEnable(GL_TEXTURE_2D);
			qglActiveTexture(GL_TEXTURE1_ARB);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glEnable(GL_TEXTURE_2D);
		}
		texnum = -1;
		lighttexnum = -1;
		for (i = 0,p = wallpoly;i < currentwallpoly;i++, p++)
		{
			if (p->texnum != texnum)
			{
				texnum = p->texnum;
				qglActiveTexture(GL_TEXTURE0_ARB);
				glBindTexture(GL_TEXTURE_2D, texnum);
				qglActiveTexture(GL_TEXTURE1_ARB);
			}
			if (p->lighttexnum != lighttexnum)
			{
				lighttexnum = p->lighttexnum;
				glBindTexture(GL_TEXTURE_2D, lighttexnum);
			}
			vert = &wallvert[p->firstvert];
			glBegin(GL_POLYGON);
			for (j=0 ; j<p->numverts ; j++, vert++)
			{
				qglMultiTexCoord2f(GL_TEXTURE0_ARB, vert->vert[3], vert->vert[4]); // texture
				qglMultiTexCoord2f(GL_TEXTURE1_ARB, vert->vert[5], vert->vert[6]); // lightmap
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}

		qglActiveTexture(GL_TEXTURE1_ARB);
		glDisable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglActiveTexture(GL_TEXTURE0_ARB);
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
				glTexCoord2f (vert->vert[3], vert->vert[4]);
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
				glTexCoord2f (vert->vert[5], vert->vert[6]);
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
//	glDisable(GL_ALPHA_TEST);
	glShadeModel(GL_SMOOTH);
	// render vertex lit overlays ontop
	texnum = -1;
	for (i = 0, p = wallpoly;i < currentwallpoly;i++, p++)
	{
		if (!p->lit)
			continue;
		for (j = 0,vertcolor = &wallvertcolor[p->firstvert];j < p->numverts;j++, vertcolor++)
			if (vertcolor->r || vertcolor->g || vertcolor->b)
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
		for (j = 0,vert = &wallvert[p->firstvert], vertcolor = &wallvertcolor[p->firstvert];j < p->numverts;j++, vert++, vertcolor++)
		{
			// would be 2fv, but windoze Matrox G200 and probably G400 drivers don't support that (dumb...)
			glTexCoord2f(vert->vert[3], vert->vert[4]);
			// again, vector version isn't supported I think
			glColor3ub(vertcolor->r, vertcolor->g, vertcolor->b);
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
			glTexCoord2f (vert->vert[3], vert->vert[4]);
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
				VectorSubtract(vert->vert, r_origin, diff);
				glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));
				glVertex3fv (vert->vert);
			}
			glEnd ();
		}
		glEnable(GL_TEXTURE_2D);
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glDisable(GL_ALPHA_TEST);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_BLEND);
	glDepthMask(1);
}

static int skyrendersphere;
static int skyrenderbox;
static int skyrenderglquakepolys;
static int skyrendertwolayers;
static int skyrendercombine;

void skypolyclear(void)
{
	currentskypoly = currentskyvert = 0;
	skyrendersphere = false;
	skyrenderbox = false;
	skyrenderglquakepolys = false;
	skyrendertwolayers = false;
	skyrendercombine = false;
	if (r_skyquality.value >= 1 && !fogenabled)
	{
		if (skyavailable_box)
			skyrenderbox = true;
		else if (skyavailable_quake)
		{
			switch((int) r_skyquality.value)
			{
			case 1:
				skyrenderglquakepolys = true;
				break;
			case 2:
				skyrenderglquakepolys = true;
				skyrendertwolayers = true;
				if (gl_combine.value)
					skyrendercombine = true;
				break;
			case 3:
				skyrendersphere = true;
				break;
			default:
			case 4:
				skyrendersphere = true;
				skyrendertwolayers = true;
				if (gl_combine.value)
					skyrendercombine = true;
				break;
			}
		}
	}
	if (r_mergesky.value && (skyrenderglquakepolys || skyrendersphere))
	{
		skyrendertwolayers = false;
		skyrendercombine = false;
//		R_BuildSky((int) (cl.time * 8.0), (int) (cl.time * 16.0));
//		R_BuildSky((int) (cl.time * -8.0), 0);
		R_BuildSky(0, (int) (cl.time * 8.0));
	}

}

void skypolyrender(void)
{
	int i, j;
	skypoly_t *p;
	skyvert_t *vert;
	float length, speedscale;
	vec3_t dir;
	float y, number;
	if (!r_render.value)
		return;
	if (currentskypoly < 1)
		return;
//	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	// make sure zbuffer is enabled
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	glVertexPointer(3, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].v[0]);
	glEnableClientState(GL_VERTEX_ARRAY);
	GL_LockArray(0, currentskyvert);
	speedscale = cl.time * (8.0/128.0);
	speedscale -= (int)speedscale;
	for (vert = skyvert, j = 0;j < currentskyvert;j++, vert++)
	{
		VectorSubtract (vert->v, r_origin, dir);
		// flatten the sphere
		dir[2] *= 3;

		// LordHavoc: fast version
		number = DotProduct(dir, dir);
		*((long *)&y) = 0x5f3759df - ((* (long *) &number) >> 1);
		length = 3.0f * (y * (1.5f - (number * 0.5f * y * y)));
		// LordHavoc: slow version
		//length = 3.0f / sqrt(DotProduct(dir, dir));

		vert->tex2[0] = speedscale + (vert->tex[0] = speedscale + dir[0] * length);
		vert->tex2[1] = speedscale + (vert->tex[1] = speedscale + dir[1] * length);
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (skyrenderglquakepolys)
	{
		glTexCoordPointer(2, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].tex[0]);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (skyrendercombine)
		{
			// upper clouds
			glBindTexture(GL_TEXTURE_2D, R_GetTexture(lighthalf ? solidskytexture_half : solidskytexture));

			// set up the second texcoord array
			// switch texcoord array selector to TMU 1
			qglClientActiveTexture(GL_TEXTURE1_ARB);
			glTexCoordPointer(2, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].tex2[0]);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);

			// render both layers as one pass using GL_ARB_texture_env_combine
			// TMU 0 is already selected, the TMU 0 texcoord array is already
			// set up, the texture is bound, and texturing is already enabled,
			// so just set up COMBINE

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);

			// set up TMU 1
			qglActiveTexture(GL_TEXTURE1_ARB);
			// lower clouds
			glBindTexture(GL_TEXTURE_2D, R_GetTexture(lighthalf ? alphaskytexture_half : alphaskytexture));

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);
			glEnable(GL_TEXTURE_2D);

			// draw it
			for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
				glDrawArrays(GL_POLYGON, p->firstvert, p->verts);

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglActiveTexture(GL_TEXTURE0_ARB);
			// switch texcoord array selector back to TMU 0
			qglClientActiveTexture(GL_TEXTURE0_ARB);
			// the TMU 0 texcoord array is disabled by the code below
		}
		else
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			if (r_mergesky.value)
				glBindTexture(GL_TEXTURE_2D, R_GetTexture(mergeskytexture)); // both layers in one texture
			else
				glBindTexture(GL_TEXTURE_2D, R_GetTexture(solidskytexture)); // upper clouds
			if(lighthalf)
				glColor3f(0.5f, 0.5f, 0.5f);
			else
				glColor3f(1.0f,1.0f,1.0f);
			glEnable(GL_TEXTURE_2D);
			GL_LockArray(0, currentskyvert);
			for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
				glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
			GL_UnlockArray();
			if (skyrendertwolayers)
			{
				glEnable(GL_BLEND);
				glDepthMask(0);
				glBindTexture(GL_TEXTURE_2D, R_GetTexture(alphaskytexture)); // lower clouds
				// switch to lower clouds texcoord array
				glTexCoordPointer(2, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].tex2[0]);
				for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
					glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
				glDepthMask(1);
				glDisable(GL_BLEND);
			}
			glColor3f(1,1,1);
		}
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glDisable(GL_TEXTURE_2D);
		// note: this color is not seen if skyrendersphere or skyrenderbox is on
		glColor3fv(fogcolor);
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
			glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
		glColor3f(1,1,1);
		glEnable(GL_TEXTURE_2D);
	}
	GL_UnlockArray();
	glDisableClientState(GL_VERTEX_ARRAY);
}

static char skyname[256];

/*
==================
R_SetSkyBox
==================
*/
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
rtexture_t *skyboxside[6];
int R_SetSkyBox(char *sky)
{
	int		i;
	char	name[1024];
	byte*	image_rgba;

	if (strcmp(sky, skyname) == 0) // no change
		return true;

	if (strlen(sky) > 1000)
	{
		Con_Printf ("sky name too long (%i, max is 1000)\n", strlen(sky));
		return false;
	}

	skyboxside[0] = skyboxside[1] = skyboxside[2] = skyboxside[3] = skyboxside[4] = skyboxside[5] = NULL;
	skyavailable_box = false;
	skyname[0] = 0;

	if (!sky[0])
		return true;

	for (i = 0;i < 6;i++)
	{
		sprintf (name, "env/%s%s", sky, suf[i]);
		if (!(image_rgba = loadimagepixels(name, false, 0, 0)))
		{
			sprintf (name, "gfx/env/%s%s", sky, suf[i]);
			if (!(image_rgba = loadimagepixels(name, false, 0, 0)))
			{
				Con_Printf ("Couldn't load %s\n", name);
				continue;
			}
		}
		skyboxside[i] = R_LoadTexture(va("skyboxside%d", i), image_width, image_height, image_rgba, TEXF_RGBA | TEXF_PRECACHE);
		qfree(image_rgba);
	}

	if (skyboxside[0] || skyboxside[1] || skyboxside[2] || skyboxside[3] || skyboxside[4] || skyboxside[5])
	{
		skyavailable_box = true;
		strcpy(skyname, sky);
		return true;
	}
	return false;
}

// LordHavoc: added LoadSky console command
void LoadSky_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		if (skyname[0])
			Con_Printf("current sky: %s\n", skyname);
		else
			Con_Printf("no skybox has been set\n");
		break;
	case 2:
		if (R_SetSkyBox(Cmd_Argv(1)))
		{
			if (skyname[0])
				Con_Printf("skybox set to %s\n", skyname);
			else
				Con_Printf("skybox disabled\n");
		}
		else
			Con_Printf("failed to load skybox %s\n", Cmd_Argv(1));
		break;
	default:
		Con_Printf("usage: loadsky skyname\n");
		break;
	}
}

#define R_SkyBoxPolyVec(s,t,x,y,z) \
	glTexCoord2f((s) * (254.0f/256.0f) + (1.0f/256.0f), (t) * (254.0f/256.0f) + (1.0f/256.0f));\
	glVertex3f((x) * 1024.0 + r_origin[0], (y) * 1024.0 + r_origin[1], (z) * 1024.0 + r_origin[2]);

void R_SkyBox(void)
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	if (lighthalf)
		glColor3f(0.5,0.5,0.5);
	else
		glColor3f(1,1,1);
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[3])); // front
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 1,  1,  1, -1);
	R_SkyBoxPolyVec(0, 0,  1,  1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[1])); // back
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0, -1,  1,  1);
	R_SkyBoxPolyVec(1, 1, -1,  1, -1);
	R_SkyBoxPolyVec(0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 0, -1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[0])); // right
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1,  1,  1);
	R_SkyBoxPolyVec(1, 1,  1,  1, -1);
	R_SkyBoxPolyVec(0, 1, -1,  1, -1);
	R_SkyBoxPolyVec(0, 0, -1,  1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[2])); // left
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0, -1, -1,  1);
	R_SkyBoxPolyVec(1, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 0,  1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[4])); // up
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1,  1,  1,  1);
	R_SkyBoxPolyVec(0, 1, -1,  1,  1);
	R_SkyBoxPolyVec(0, 0, -1, -1,  1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skyboxside[5])); // down
	glBegin(GL_QUADS);
	R_SkyBoxPolyVec(1, 0,  1,  1, -1);
	R_SkyBoxPolyVec(1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(0, 0, -1,  1, -1);
	glEnd();
	glDepthMask(1);
	glEnable (GL_DEPTH_TEST);
	glColor3f (1,1,1);
}

float skysphere[33*33*5];
int skysphereindices[32*32*6];
void skyspherecalc(float *sphere, float dx, float dy, float dz)
{
	float a, b, x, ax, ay, v[3], length;
	int i, j, *index;
	for (a = 0;a <= 1;a += (1.0 / 32.0))
	{
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (b = 0;b <= 1;b += (1.0 / 32.0))
		{
			x = cos(b * M_PI * 2);
			v[0] = ax*x * dx;
			v[1] = ay*x * dy;
			v[2] = -sin(b * M_PI * 2) * dz;
			length = 3.0f / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));
			*sphere++ = v[0] * length;
			*sphere++ = v[1] * length;
			*sphere++ = v[0];
			*sphere++ = v[1];
			*sphere++ = v[2];
		}
	}
	index = skysphereindices;
	for (j = 0;j < 32;j++)
	{
		for (i = 0;i < 32;i++)
		{
			*index++ =  j      * 33 + i;
			*index++ =  j      * 33 + i + 1;
			*index++ = (j + 1) * 33 + i;

			*index++ =  j      * 33 + i + 1;
			*index++ = (j + 1) * 33 + i + 1;
			*index++ = (j + 1) * 33 + i;
		}
		i++;
	}
}

void skyspherearrays(float *vert, float *tex, float *tex2, float *source, float s, float s2)
{
	float *v, *t, *t2;
	int i;
	v = vert;
	t = tex;
	t2 = tex2;
	for (i = 0;i < (33*33);i++)
	{
		*t++ = source[0] + s;
		*t++ = source[1] + s;
		*t2++ = source[0] + s2;
		*t2++ = source[1] + s2;
		*v++ = source[2] + r_origin[0];
		*v++ = source[3] + r_origin[1];
		*v++ = source[4] + r_origin[2];
		*v++ = 0;
		source += 5;
	}
}

void R_SkySphere(void)
{
	float speedscale, speedscale2;
	float vert[33*33*4], tex[33*33*2], tex2[33*33*2];
	static qboolean skysphereinitialized = false;
	if (!skysphereinitialized)
	{
		skysphereinitialized = true;
		skyspherecalc(skysphere, 1024, 1024, 1024 / 3);
	}
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (lighthalf)
		glColor3f(0.5,0.5,0.5);
	else
		glColor3f(1,1,1);
	speedscale = cl.time*8.0/128.0;
	speedscale -= (int)speedscale;
	speedscale2 = cl.time*16.0/128.0;
	speedscale2 -= (int)speedscale2;
	skyspherearrays(vert, tex, tex2, skysphere, speedscale, speedscale2);
	glVertexPointer(3, GL_FLOAT, sizeof(float) * 4, vert);
	glEnableClientState(GL_VERTEX_ARRAY);
	// do not lock the texcoord array, because it will be switched
	GL_LockArray(0, 32*32*6);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 2, tex);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if (r_mergesky.value)
	{
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(mergeskytexture)); // both layers in one texture
		glDrawElements(GL_TRIANGLES, 32*32*6, GL_UNSIGNED_INT, &skysphereindices[0]);
	}
	else
	{
		// LordHavoc: note that this combine operation does not use the color,
		// so it has to use alternate textures in lighthalf mode
		if (skyrendercombine)
		{
			// upper clouds
			glBindTexture(GL_TEXTURE_2D, R_GetTexture(lighthalf ? solidskytexture_half : solidskytexture));

			// set up the second texcoord array
			// switch texcoord array selector to TMU 1
			qglClientActiveTexture(GL_TEXTURE1_ARB);
			glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 2, tex2);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);


			// render both layers as one pass using GL_ARB_texture_env_combine
			// TMU 0 is already selected, the TMU 0 texcoord array is already
			// set up, the texture is bound, and texturing is already enabled,
			// so just set up COMBINE

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);

			// set up TMU 1
			qglActiveTexture(GL_TEXTURE1_ARB);
			// lower clouds
			glBindTexture(GL_TEXTURE_2D, R_GetTexture(lighthalf ? alphaskytexture_half : alphaskytexture));

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_ARB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA_ARB, GL_SRC_ALPHA);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0);
			glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0);
			glEnable(GL_TEXTURE_2D);

			// draw it
			glDrawElements(GL_TRIANGLES, 32*32*6, GL_UNSIGNED_INT, &skysphereindices[0]);

			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			qglActiveTexture(GL_TEXTURE0_ARB);
			// switch texcoord array selector back to TMU 0
			qglClientActiveTexture(GL_TEXTURE0_ARB);
			// the TMU 0 texcoord array is disabled by the code below
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, R_GetTexture(solidskytexture)); // upper clouds
			glDrawElements(GL_TRIANGLES, 32*32*6, GL_UNSIGNED_INT, &skysphereindices[0]);

			if (skyrendertwolayers)
			{
				glEnable (GL_BLEND);
				glBindTexture(GL_TEXTURE_2D, R_GetTexture(alphaskytexture)); // lower clouds
				glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 2, tex2);
				glDrawElements(GL_TRIANGLES, 32*32*6, GL_UNSIGNED_INT, &skysphereindices[0]);
				glDisable (GL_BLEND);
			}
		}
	}
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	GL_UnlockArray();
	glDisableClientState(GL_VERTEX_ARRAY);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(1);
	glEnable (GL_DEPTH_TEST);
	glColor3f (1,1,1);
}

void R_Sky(void)
{
	if (!r_render.value)
		return;
	if (skyrendersphere)
		R_SkySphere();
	else if (skyrenderbox)
		R_SkyBox();
}

//===============================================================

byte skyupperlayerpixels[128*128*4];
byte skylowerlayerpixels[128*128*4];
byte skymergedpixels[128*128*4];

void R_BuildSky (int scrollupper, int scrolllower)
{
	int x, y, ux, uy, lx, ly;
	byte *m, *u, *l;
	m = skymergedpixels;
	for (y = 0;y < 128;y++)
	{
		uy = (y + scrollupper) & 127;
		ly = (y + scrolllower) & 127;
		for (x = 0;x < 128;x++)
		{
			ux = (x + scrollupper) & 127;
			lx = (x + scrolllower) & 127;
			u = &skyupperlayerpixels[(uy * 128 + ux) * 4];
			l = &skylowerlayerpixels[(ly * 128 + lx) * 4];
			if (l[3])
			{
				if (l[3] == 255)
					*((int *)m) = *((int *)l);
				else
				{
					m[0] = ((((int) l[0] - (int) u[0]) * (int) l[3]) >> 8) + (int) u[0];
					m[1] = ((((int) l[1] - (int) u[1]) * (int) l[3]) >> 8) + (int) u[1];
					m[2] = ((((int) l[2] - (int) u[2]) * (int) l[3]) >> 8) + (int) u[2];
					m[3] = 255;
				}
			}
			else
				*((int *)m) = *((int *)u);
			m += 4;
		}
	}
	// FIXME: implement generated texture callbacks to speed this up?  (skip identifier lookup, CRC, memcpy, etc)
	if (mergeskytexture)
	{
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(mergeskytexture));
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, skymergedpixels);
	}
	else
		mergeskytexture = R_LoadTexture("mergedskytexture", 128, 128, skymergedpixels, TEXF_RGBA | TEXF_ALWAYSPRECACHE);
}

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (byte *src, int bytesperpixel)
{
	int			i, j, p;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;

	if (!isworldmodel)
		return;

	strcpy(skyworldname, loadmodel->name);
	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j+128];
	}
	else
	{
		// make an average value for the back to avoid
		// a fringe on the top level
		r = g = b = 0;
		for (i=0 ; i<128 ; i++)
		{
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j + 128];
				rgba = &d_8to24table[p];
				trans[(i*128) + j] = *rgba;
				r += ((byte *)rgba)[0];
				g += ((byte *)rgba)[1];
				b += ((byte *)rgba)[2];
			}
		}

		((byte *)&transpix)[0] = r/(128*128);
		((byte *)&transpix)[1] = g/(128*128);
		((byte *)&transpix)[2] = b/(128*128);
		((byte *)&transpix)[3] = 0;
	}

	memcpy(skyupperlayerpixels, trans, 128*128*4);

	solidskytexture = R_LoadTexture ("sky_solidtexture", 128, 128, (byte *) trans, TEXF_RGBA | TEXF_PRECACHE);
	for (i = 0;i < 128*128;i++)
	{
		((byte *)&trans[i])[0] >>= 1;
		((byte *)&trans[i])[1] >>= 1;
		((byte *)&trans[i])[2] >>= 1;
	}
	solidskytexture_half = R_LoadTexture ("sky_solidtexture_half", 128, 128, (byte *) trans, TEXF_RGBA | TEXF_PRECACHE);

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j];
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j];
				if (p == 0)
					trans[(i*128) + j] = transpix;
				else
					trans[(i*128) + j] = d_8to24table[p];
			}
		}
	}

	memcpy(skylowerlayerpixels, trans, 128*128*4);

	alphaskytexture = R_LoadTexture ("sky_alphatexture", 128, 128, (byte *) trans, TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
	for (i = 0;i < 128*128;i++)
	{
		((byte *)&trans[i])[0] >>= 1;
		((byte *)&trans[i])[1] >>= 1;
		((byte *)&trans[i])[2] >>= 1;
	}
	alphaskytexture_half = R_LoadTexture ("sky_alphatexture_half", 128, 128, (byte *) trans, TEXF_ALPHA | TEXF_RGBA | TEXF_PRECACHE);
}
