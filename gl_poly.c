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

cvar_t gl_multitexture = {"gl_multitexture", "1"};

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

void gl_poly_start()
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

void gl_poly_shutdown()
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

void gl_poly_newmap()
{
}

void GL_Poly_Init()
{
	Cvar_RegisterVariable (&gl_multitexture);
	R_RegisterModule("GL_Poly", gl_poly_start, gl_poly_shutdown, gl_poly_newmap);
}

void transpolyclear()
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

void transpolyrender()
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

void wallpolyclear()
{
	currentwallpoly = currentwallvert = 0;
}

void wallpolyrender()
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
		gl_multitexture.value = 0;
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
				qglMTexCoord2f(gl_mtex_enum, vert->vert[3], vert->vert[4]); // texture
				qglMTexCoord2f((gl_mtex_enum+1), vert->vert[5], vert->vert[6]); // lightmap
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

void skypolyclear()
{
	currentskypoly = currentskyvert = 0;
}

extern char skyname[];
extern rtexture_t *solidskytexture, *alphaskytexture;
void skypolyrender()
{
	int i, j;
	skypoly_t *p;
	skyvert_t *vert;
	float length, speedscale;
	vec3_t dir;
	if (!r_render.value)
		return;
	if (currentskypoly < 1)
		return;
	// testing
//	Con_DPrintf("skypolyrender: %i polys %i vertices\n", currentskypoly, currentskyvert);
//	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	// make sure zbuffer is enabled
	glEnable(GL_DEPTH_TEST);
	glDepthMask(1);
	if (!fogenabled && !skyname[0]) // normal quake sky
	{
		glInterleavedArrays(GL_T2F_V3F, 0, skyvert);
//		glTexCoordPointer(2, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].tex[0]);
//		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
//		glVertexPointer(3, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].v[0]);
//		glEnableClientState(GL_VERTEX_ARRAY);
		if(lighthalf)
			glColor3f(0.5f, 0.5f, 0.5f);
		else
			glColor3f(1.0f,1.0f,1.0f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(solidskytexture)); // upper clouds
		speedscale = cl.time*8;
		speedscale -= (int)speedscale & ~127 ;
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
		{
			vert = skyvert + p->firstvert;
			for (j = 0;j < p->verts;j++, vert++)
			{
				VectorSubtract (vert->v, r_origin, dir);
				dir[2] *= 3;	// flatten the sphere

				length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
				length = sqrt (length);
				length = 6*63/length;

				vert->tex[0] = (speedscale + dir[0] * length) * (1.0/128);
				vert->tex[1] = (speedscale + dir[1] * length) * (1.0/128);
			}
		}
		GL_LockArray(0, currentskyvert);
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
			glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
		GL_UnlockArray();
		glEnable(GL_BLEND);
		glDepthMask(0);
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(alphaskytexture)); // lower clouds
		speedscale = cl.time*16;
		speedscale -= (int)speedscale & ~127 ;
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
		{
			vert = skyvert + p->firstvert;
			for (j = 0;j < p->verts;j++, vert++)
			{
				VectorSubtract (vert->v, r_origin, dir);
				dir[2] *= 3;	// flatten the sphere

				length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
				length = sqrt (length);
				length = 6*63/length;

				vert->tex[0] = (speedscale + dir[0] * length) * (1.0/128);
				vert->tex[1] = (speedscale + dir[1] * length) * (1.0/128);
			}
		}
		GL_LockArray(0, currentskyvert);
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
			glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
		GL_UnlockArray();
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
		glDepthMask(1);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		glVertexPointer(3, GL_FLOAT, sizeof(skyvert_t), &skyvert[0].v[0]);
		glEnableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor3fv(fogcolor); // note: gets rendered over by skybox if fog is not enabled
		GL_LockArray(0, currentskyvert);
		for (i = 0, p = &skypoly[0];i < currentskypoly;i++, p++)
			glDrawArrays(GL_POLYGON, p->firstvert, p->verts);
		GL_UnlockArray();
		glColor3f(1,1,1);
		glEnable(GL_TEXTURE_2D);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
}
