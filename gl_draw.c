/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"

cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1"};

static rtexture_t *char_texture;

//=============================================================================
/* Support Routines */

#define MAX_CACHED_PICS 256
#define CACHEPICHASHSIZE 256
static cachepic_t *cachepichash[CACHEPICHASHSIZE];
static cachepic_t cachepics[MAX_CACHED_PICS];
static int numcachepics;

static rtexturepool_t *drawtexturepool;

static qbyte pointerimage[256] =
{
	"333333332......."
	"26777761........"
	"2655541........."
	"265541.........."
	"2654561........."
	"26414561........"
	"251.14561......."
	"21...14561......"
	"1.....141......."
	".......1........"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
};

static rtexture_t *draw_generatemousepointer(void)
{
	int i;
	qbyte buffer[256][4];
	for (i = 0;i < 256;i++)
	{
		if (pointerimage[i] == '.')
		{
			buffer[i][0] = 0;
			buffer[i][1] = 0;
			buffer[i][2] = 0;
			buffer[i][3] = 0;
		}
		else
		{
			buffer[i][0] = (pointerimage[i] - '0') * 16;
			buffer[i][1] = (pointerimage[i] - '0') * 16;
			buffer[i][2] = (pointerimage[i] - '0') * 16;
			buffer[i][3] = 255;
		}
	}
	return R_LoadTexture(drawtexturepool, "mousepointer", 16, 16, &buffer[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
}

// must match NUMCROSSHAIRS in r_crosshairs.c
#define NUMCROSSHAIRS 5

static qbyte *crosshairtexdata[NUMCROSSHAIRS] =
{
	"................"
	"................"
	"................"
	"...33......33..."
	"...355....553..."
	"....577..775...."
	".....77..77....."
	"................"
	"................"
	".....77..77....."
	"....577..775...."
	"...355....553..."
	"...33......33..."
	"................"
	"................"
	"................"
	,
	"................"
	"................"
	"................"
	"...3........3..."
	"....5......5...."
	".....7....7....."
	"......7..7......"
	"................"
	"................"
	"......7..7......"
	".....7....7....."
	"....5......5...."
	"...3........3..."
	"................"
	"................"
	"................"
	,
	"................"
	".......77......."
	".......77......."
	"................"
	"................"
	".......44......."
	".......44......."
	".77..44..44..77."
	".77..44..44..77."
	".......44......."
	".......44......."
	"................"
	".......77......."
	".......77......."
	"................"
	"................"
	,
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"........7777777."
	"........752....."
	"........72......"
	"........7......."
	"........7......."
	"........7......."
	"................"
	"................"
	,
	"................"
	"................"
	"................"
	"................"
	"................"
	"........7......."
	"................"
	"........4......."
	".....7.4.4.7...."
	"........4......."
	"................"
	"........7......."
	"................"
	"................"
	"................"
	"................"
};

static rtexture_t *draw_generatecrosshair(int num)
{
	int i;
	char *in;
	qbyte data[16*16][4];
	in = crosshairtexdata[num];
	for (i = 0;i < 16*16;i++)
	{
		if (in[i] == '.')
		{
			data[i][0] = 255;
			data[i][1] = 255;
			data[i][2] = 255;
			data[i][3] = 0;
		}
		else
		{
			data[i][0] = 255;
			data[i][1] = 255;
			data[i][2] = 255;
			data[i][3] = (qbyte) ((int) (in[i] - '0') * 255 / 7);
		}
	}
	return R_LoadTexture(drawtexturepool, va("crosshair%i", num), 16, 16, &data[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
}

/*
================
Draw_CachePic
================
*/
// FIXME: move this to client somehow
cachepic_t	*Draw_CachePic (char *path)
{
	int i, crc, hashkey;
	cachepic_t *pic;
	qpic_t *p;

	crc = CRC_Block(path, strlen(path));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
		if (!strcmp (path, pic->name))
			return pic;

	if (numcachepics == MAX_CACHED_PICS)
		Sys_Error ("numcachepics == MAX_CACHED_PICS");
	pic = cachepics + (numcachepics++);
	strcpy (pic->name, path);
	// link into list
	pic->chain = cachepichash[hashkey];
	cachepichash[hashkey] = pic;

	// load the pic from disk
	pic->tex = loadtextureimage(drawtexturepool, path, 0, 0, false, false, true);
	if (pic->tex == NULL && (p = W_GetLumpName (path)))
	{
		if (!strcmp(path, "conchars"))
		{
			qbyte *pix;
			// conchars is a raw image and with the wrong transparent color
			pix = (qbyte *)p;
			for (i = 0;i < 128 * 128;i++)
				if (pix[i] == 0)
					pix[i] = 255;
			pic->tex = R_LoadTexture (drawtexturepool, path, 128, 128, pix, TEXTYPE_QPALETTE, TEXF_ALPHA | TEXF_PRECACHE);
		}
		else
			pic->tex = R_LoadTexture (drawtexturepool, path, p->width, p->height, p->data, TEXTYPE_QPALETTE, TEXF_ALPHA | TEXF_PRECACHE);
	}
	if (pic->tex == NULL && !strcmp(path, "ui/mousepointer.tga"))
		pic->tex = draw_generatemousepointer();
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair1.tga"))
		pic->tex = draw_generatecrosshair(0);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair2.tga"))
		pic->tex = draw_generatecrosshair(1);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair3.tga"))
		pic->tex = draw_generatecrosshair(2);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair4.tga"))
		pic->tex = draw_generatecrosshair(3);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair5.tga"))
		pic->tex = draw_generatecrosshair(4);
	if (pic->tex == NULL)
		Sys_Error ("Draw_CachePic: failed to load %s", path);

	pic->width = R_TextureWidth(pic->tex);
	pic->height = R_TextureHeight(pic->tex);
	return pic;
}

cachepic_t *Draw_NewPic(char *picname, int width, int height, int alpha, qbyte *pixels)
{
	int crc, hashkey;
	cachepic_t *pic;

	crc = CRC_Block(picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
		if (!strcmp (picname, pic->name))
			break;

	if (pic)
	{
		if (pic->tex && pic->width == width && pic->height == height)
		{
			R_UpdateTexture(pic->tex, pixels);
			return pic;
		}
	}
	else
	{
		if (pic == NULL)
		{
			if (numcachepics == MAX_CACHED_PICS)
				Sys_Error ("numcachepics == MAX_CACHED_PICS");
			pic = cachepics + (numcachepics++);
			strcpy (pic->name, picname);
			// link into list
			pic->chain = cachepichash[hashkey];
			cachepichash[hashkey] = pic;
		}
	}

	pic->width = width;
	pic->height = height;
	if (pic->tex)
		R_FreeTexture(pic->tex);
	pic->tex = R_LoadTexture (drawtexturepool, picname, width, height, pixels, TEXTYPE_RGBA, alpha ? TEXF_ALPHA : 0);
	return pic;
}

void Draw_FreePic(char *picname)
{
	int crc;
	int hashkey;
	cachepic_t *pic;
	// this doesn't really free the pic, but does free it's texture
	crc = CRC_Block(picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
	{
		if (!strcmp (picname, pic->name))
		{
			R_FreeTexture(pic->tex);
			pic->width = 0;
			pic->height = 0;
			return;
		}
	}
}

/*
===============
Draw_Init
===============
*/
static void gl_draw_start(void)
{
	drawtexturepool = R_AllocTexturePool();

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));

	char_texture = Draw_CachePic("conchars")->tex;
}

static void gl_draw_shutdown(void)
{
	R_FreeTexturePool(&drawtexturepool);

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));
}

static void gl_draw_newmap(void)
{
}

void GL_Draw_Init (void)
{
	Cvar_RegisterVariable (&scr_conalpha);

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));

	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap);
}

extern cvar_t gl_mesh_drawmode;
extern int gl_maxdrawrangeelementsvertices;
extern int gl_maxdrawrangeelementsindices;

void R_DrawQueue(void)
{
	int pos, num, chartexnum, overbright;
	float x, y, w, h, s, t, u, v;
	cachepic_t *pic;
	drawqueue_t *dq;
	char *str, *currentpic;
	int batch, batchcount, additive;
	unsigned int color;
	drawqueuemesh_t *mesh;

	if (!r_render.integer)
		return;

	qglViewport(vid.realx, vid.realy, vid.realwidth, vid.realheight);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity();
	qglOrtho(0, vid.conwidth, vid.conheight, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity();

	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);
	qglEnable(GL_BLEND);
	qglEnable(GL_TEXTURE_2D);
	qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	chartexnum = R_GetTexture(char_texture);

	additive = false;
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	currentpic = "";
	pic = NULL;
	qglBindTexture(GL_TEXTURE_2D, 0);
	color = 0;
	qglColor4ub(0,0,0,0);

	overbright = v_overbrightbits.integer;
	batch = false;
	batchcount = 0;
	for (pos = 0;pos < r_refdef.drawqueuesize;pos += ((drawqueue_t *)(r_refdef.drawqueue + pos))->size)
	{
		dq = (drawqueue_t *)(r_refdef.drawqueue + pos);
		if (dq->flags & DRAWFLAG_ADDITIVE)
		{
			if (!additive)
			{
				if (batch)
				{
					batch = false;
					qglEnd();
				}
				additive = true;
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
			}
		}
		else
		{
			if (additive)
			{
				if (batch)
				{
					batch = false;
					qglEnd();
				}
				additive = false;
				qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
		if (color != dq->color)
		{
			color = dq->color;
			qglColor4ub((qbyte)(((color >> 24) & 0xFF) >> overbright), (qbyte)(((color >> 16) & 0xFF) >> overbright), (qbyte)(((color >> 8) & 0xFF) >> overbright), (qbyte)(color & 0xFF));
		}
		if (batch && batchcount > 128)
		{
			batch = false;
			qglEnd();
		}
		x = dq->x;
		y = dq->y;
		w = dq->scalex;
		h = dq->scaley;
		switch(dq->command)
		{
		case DRAWQUEUE_PIC:
			str = (char *)(dq + 1);
			if (*str)
			{
				if (strcmp(str, currentpic))
				{
					if (batch)
					{
						batch = false;
						qglEnd();
					}
					currentpic = str;
					pic = Draw_CachePic(str);
					qglBindTexture(GL_TEXTURE_2D, R_GetTexture(pic->tex));
				}
				if (w == 0)
					w = pic->width;
				if (h == 0)
					h = pic->height;
				if (!batch)
				{
					batch = true;
					qglBegin(GL_TRIANGLES);
					batchcount = 0;
				}
				qglTexCoord2f (0, 0);qglVertex2f (x  , y  );
				qglTexCoord2f (1, 0);qglVertex2f (x+w, y  );
				qglTexCoord2f (1, 1);qglVertex2f (x+w, y+h);
				qglTexCoord2f (0, 0);qglVertex2f (x  , y  );
				qglTexCoord2f (1, 1);qglVertex2f (x+w, y+h);
				qglTexCoord2f (0, 1);qglVertex2f (x  , y+h);
				batchcount++;
			}
			else
			{
				if (currentpic[0])
				{
					if (batch)
					{
						batch = false;
						qglEnd();
					}
					currentpic = "";
					qglBindTexture(GL_TEXTURE_2D, 0);
				}
				if (!batch)
				{
					batch = true;
					qglBegin(GL_TRIANGLES);
					batchcount = 0;
				}
				qglTexCoord2f (0, 0);qglVertex2f (x  , y  );
				qglTexCoord2f (1, 0);qglVertex2f (x+w, y  );
				qglTexCoord2f (1, 1);qglVertex2f (x+w, y+h);
				qglTexCoord2f (0, 0);qglVertex2f (x  , y  );
				qglTexCoord2f (1, 1);qglVertex2f (x+w, y+h);
				qglTexCoord2f (0, 1);qglVertex2f (x  , y+h);
				batchcount++;
			}
			break;
		case DRAWQUEUE_STRING:
			str = (char *)(dq + 1);
			if (strcmp("conchars", currentpic))
			{
				if (batch)
				{
					batch = false;
					qglEnd();
				}
				currentpic = "conchars";
				qglBindTexture(GL_TEXTURE_2D, chartexnum);
			}
			if (!batch)
			{
				batch = true;
				qglBegin(GL_TRIANGLES);
				batchcount = 0;
			}
			while ((num = *str++) && x < vid.conwidth)
			{
				if (num != ' ')
				{
					s = (num & 15)*0.0625f + (0.5f / 256.0f);
					t = (num >> 4)*0.0625f + (0.5f / 256.0f);
					u = 0.0625f - (1.0f / 256.0f);
					v = 0.0625f - (1.0f / 256.0f);
					qglTexCoord2f (s  , t  );qglVertex2f (x  , y  );
					qglTexCoord2f (s+u, t  );qglVertex2f (x+w, y  );
					qglTexCoord2f (s+u, t+v);qglVertex2f (x+w, y+h);
					qglTexCoord2f (s  , t  );qglVertex2f (x  , y  );
					qglTexCoord2f (s+u, t+v);qglVertex2f (x+w, y+h);
					qglTexCoord2f (s  , t+v);qglVertex2f (x  , y+h);
					batchcount++;
				}
				x += w;
			}
			break;
		case DRAWQUEUE_MESH:
			if (batch)
			{
				batch = false;
				qglEnd();
			}
			mesh = (void *)(dq + 1);
			qglBindTexture(GL_TEXTURE_2D, R_GetTexture(mesh->texture));

			if (gl_mesh_drawmode.integer < 0)
				Cvar_SetValueQuick(&gl_mesh_drawmode, 0);
			if (gl_mesh_drawmode.integer > 3)
				Cvar_SetValueQuick(&gl_mesh_drawmode, 3);
			if (gl_mesh_drawmode.integer >= 3 && qglDrawRangeElements == NULL)
				Cvar_SetValueQuick(&gl_mesh_drawmode, 2);

			if (gl_mesh_drawmode.integer > 0)
			{
				qglVertexPointer(3, GL_FLOAT, sizeof(float[3]), mesh->vertices);CHECKGLERROR
				qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), mesh->texcoords);CHECKGLERROR
				qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(qbyte[4]), mesh->colors);CHECKGLERROR
				qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
			}

			if (gl_mesh_drawmode.integer >= 3/* && (mesh->numvertices) <= gl_maxdrawrangeelementsvertices && (mesh->numindices) <= gl_maxdrawrangeelementsindices*/)
			{
				// GL 1.2 or GL 1.1 with extension
				GL_LockArray(0, mesh->numvertices);
				qglDrawRangeElements(GL_TRIANGLES, 0, mesh->numvertices, mesh->numindices, GL_UNSIGNED_INT, mesh->indices);
				CHECKGLERROR
				GL_UnlockArray();
			}
			else if (gl_mesh_drawmode.integer >= 2)
			{
				// GL 1.1
				GL_LockArray(0, mesh->numvertices);
				qglDrawElements(GL_TRIANGLES, mesh->numindices, GL_UNSIGNED_INT, mesh->indices);
				CHECKGLERROR
				GL_UnlockArray();
			}
			else if (gl_mesh_drawmode.integer >= 1)
			{
				int i;
				// GL 1.1
				// feed it manually using glArrayElement
				GL_LockArray(0, mesh->numvertices);
				qglBegin(GL_TRIANGLES);
				for (i = 0;i < mesh->numindices;i++)
					qglArrayElement(mesh->indices[i]);
				qglEnd();
				CHECKGLERROR
				GL_UnlockArray();
			}
			else
			{
				int i, in;
				// GL 1.1 but not using vertex arrays - 3dfx glquake minigl driver
				// feed it manually
				qglBegin(GL_TRIANGLES);
				for (i = 0;i < mesh->numindices;i++)
				{
					in = mesh->indices[i];
					qglColor4ub(mesh->colors[in * 4], mesh->colors[in * 4 + 1], mesh->colors[in * 4 + 2], mesh->colors[in * 4 + 3]);
					qglTexCoord2f(mesh->texcoords[in * 2], mesh->texcoords[in * 2 + 1]);
					qglVertex3f(mesh->vertices[in * 3], mesh->vertices[in * 3 + 1], mesh->vertices[in * 3 + 2]);
				}
				qglEnd();
				CHECKGLERROR
			}
			if (gl_mesh_drawmode.integer > 0)
			{
				qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
				qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR
			}
			// restore color, since it got trashed by using color array
			qglColor4ub((qbyte)(((color >> 24) & 0xFF) >> overbright), (qbyte)(((color >> 16) & 0xFF) >> overbright), (qbyte)(((color >> 8) & 0xFF) >> overbright), (qbyte)(color & 0xFF));
			CHECKGLERROR
			currentpic = "\0";
			break;
		}
	}
	if (batch)
		qglEnd();
	CHECKGLERROR

	if (!v_hwgamma.integer)
	{
		qglDisable(GL_TEXTURE_2D);
		CHECKGLERROR
		t = v_contrast.value * (float) (1 << v_overbrightbits.integer);
		if (t >= 1.01f)
		{
			qglBlendFunc (GL_DST_COLOR, GL_ONE);
			CHECKGLERROR
			qglBegin (GL_TRIANGLES);
			while (t >= 1.01f)
			{
				num = (int) ((t - 1.0f) * 255.0f);
				if (num > 255)
					num = 255;
				qglColor4ub ((qbyte) num, (qbyte) num, (qbyte) num, 255);
				qglVertex2f (-5000, -5000);
				qglVertex2f (10000, -5000);
				qglVertex2f (-5000, 10000);
				t *= 0.5;
			}
			qglEnd ();
			CHECKGLERROR
		}
		else if (t <= 0.99f)
		{
			qglBlendFunc(GL_ZERO, GL_SRC_COLOR);
			CHECKGLERROR
			qglBegin(GL_TRIANGLES);
			num = (int) (t * 255.0f);
			qglColor4ub ((qbyte) num, (qbyte) num, (qbyte) num, 255);
			qglVertex2f (-5000, -5000);
			qglVertex2f (10000, -5000);
			qglVertex2f (-5000, 10000);
			qglEnd();
			CHECKGLERROR
		}
		if (v_brightness.value >= 0.01f)
		{
			qglBlendFunc (GL_ONE, GL_ONE);
			CHECKGLERROR
			num = (int) (v_brightness.value * 255.0f);
			qglColor4ub ((qbyte) num, (qbyte) num, (qbyte) num, 255);
			CHECKGLERROR
			qglBegin (GL_TRIANGLES);
			qglVertex2f (-5000, -5000);
			qglVertex2f (10000, -5000);
			qglVertex2f (-5000, 10000);
			qglEnd ();
			CHECKGLERROR
		}
		qglEnable(GL_TEXTURE_2D);
		CHECKGLERROR
	}

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGLERROR
	qglEnable (GL_CULL_FACE);
	CHECKGLERROR
	qglEnable (GL_DEPTH_TEST);
	CHECKGLERROR
	qglDisable (GL_BLEND);
	CHECKGLERROR
	qglColor4ub (255, 255, 255, 255);
	CHECKGLERROR
}

