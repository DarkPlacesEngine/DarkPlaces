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
	if (pic->tex == NULL && !strncmp(path, "gfx/", 4))
	{
		// compatibility with older versions
		pic->tex = loadtextureimage(drawtexturepool, path + 4, 0, 0, false, false, true);
		// failed to find gfx/whatever.tga or similar, try the wad
		if (pic->tex == NULL && (p = W_GetLumpName (path + 4)))
		{
			if (!strcmp(path, "gfx/conchars"))
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
	{
		Con_Printf ("Draw_CachePic: failed to load %s", path);
		pic->tex = r_notexture;
	}

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

	char_texture = Draw_CachePic("gfx/conchars")->tex;
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

extern cvar_t gl_mesh_drawrangeelements;
extern int gl_maxdrawrangeelementsvertices;
extern int gl_maxdrawrangeelementsindices;

#if 0
void R_DrawQueue(void)
{
	int pos, num, chartexnum, overbright;
	float x, y, w, h, s, t, u, v;
	cachepic_t *pic;
	drawqueue_t *dq;
	char *str, *currentpic;
	int batchcount;
	unsigned int color;
	drawqueuemesh_t *mesh;

	if (!r_render.integer)
		return;

	GL_SetupView_ViewPort(vid.realx, vid.realy, vid.realwidth, vid.realheight);
	GL_SetupView_Mode_Ortho(0, 0, vid.conwidth, vid.conheight, -10, 100);
	GL_SetupView_Orientation_Identity();
	GL_DepthFunc(GL_LEQUAL);
	R_Mesh_Start();

	chartexnum = R_GetTexture(char_texture);

	currentpic = "";
	pic = NULL;
	texnum = 0;
	color = 0;

	overbright = v_overbrightbits.integer;
	batch = false;
	batchcount = 0;
	for (pos = 0;pos < r_refdef.drawqueuesize;pos += ((drawqueue_t *)(r_refdef.drawqueue + pos))->size)
	{
		dq = (drawqueue_t *)(r_refdef.drawqueue + pos);
		additive = (dq->flags & DRAWFLAG_ADDITIVE) != 0;
		color = dq->color;
		m.blendfunc1 = GL_SRC_ALPHA;
		if (additive)
			m.blendfunc2 = GL_ONE;
		else
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		m.depthdisable = true;
		R_Mesh_MainState(&m);

		cr = (float) ((color >> 24) & 0xFF) * (1.0f / 255.0f) * r_colorscale;
		cg = (float) ((color >> 16) & 0xFF) * (1.0f / 255.0f) * r_colorscale;
		cb = (float) ((color >>  8) & 0xFF) * (1.0f / 255.0f) * r_colorscale;
		ca = (float) ( color        & 0xFF) * (1.0f / 255.0f);
		x = dq->x;
		y = dq->y;
		w = dq->scalex;
		h = dq->scaley;
		
		switch(dq->command)
		{
		case DRAWQUEUE_PIC:
			str = (char *)(dq + 1);
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
			if (*str)
			{
				if (w == 0)
					w = pic->width;
				if (h == 0)
					h = pic->height;
			}
			varray_color[0] = varray_color[4] = varray_color[ 8] = varray_color[12] = cr;
			varray_color[1] = varray_color[5] = varray_color[ 9] = varray_color[13] = cg;
			varray_color[2] = varray_color[6] = varray_color[10] = varray_color[14] = cb;
			varray_color[3] = varray_color[7] = varray_color[11] = varray_color[15] = ca;
			varray_texcoord[0] = 0;varray_texcoord[1] = 0;
			varray_texcoord[2] = 1;varray_texcoord[3] = 0;
			varray_texcoord[4] = 1;varray_texcoord[5] = 1;
			varray_texcoord[6] = 0;varray_texcoord[7] = 1;
			varray_vertex[ 0] = x  ;varray_vertex[ 1] = y  ;varray_vertex[ 2] = 0;
			varray_vertex[ 4] = x+w;varray_vertex[ 5] = y  ;varray_vertex[ 6] = 0;
			varray_vertex[ 8] = x+w;varray_vertex[ 9] = y+h;varray_vertex[10] = 0;
			varray_vertex[12] = x  ;varray_vertex[13] = y+h;varray_vertex[14] = 0;
			R_Mesh_Draw(4, 2, polygonelements);
			break;
		case DRAWQUEUE_STRING:
			str = (char *)(dq + 1);
			if (strcmp("gfx/conchars", currentpic))
			{
				if (batch)
				{
					batch = false;
					qglEnd();
				}
				currentpic = "gfx/conchars";
				qglBindTexture(GL_TEXTURE_2D, chartexnum);
			}
			batchcount = 0;
			ac = varray_color;
			at = varray_texcoord;
			av = varray_vertex;
			while ((num = *str++) && x < vid.conwidth)
			{
				if (num != ' ')
				{
					s = (num & 15)*0.0625f + (0.5f / 256.0f);
					t = (num >> 4)*0.0625f + (0.5f / 256.0f);
					u = 0.0625f - (1.0f / 256.0f);
					v = 0.0625f - (1.0f / 256.0f);
					ac[0] = ac[4] = ac[ 8] = ac[12] = cr;
					ac[1] = ac[5] = ac[ 9] = ac[13] = cg;
					ac[2] = ac[6] = ac[10] = ac[14] = cb;
					ac[3] = ac[7] = ac[11] = ac[15] = ca;
					at[0] = s  ;at[1] = t  ;
					at[2] = s+u;at[3] = t  ;
					at[4] = s+u;at[5] = t+v;
					at[6] = s  ;at[7] = t+v;
					av[0] = x  ;av[1] = y  ;av[2] = 0;
					av[4] = x+w;av[1] = y  ;av[2] = 0;
					av[8] = x+w;av[1] = y+h;av[2] = 0;
					av[0] = x  ;av[1] = y+h;av[2] = 0;
					ac += 16;
					at += 8;
					av += 16;
					batchcount++;
					if (batchcount >= 128)
					{
						R_Mesh_Draw(batchcount * 4, batchcount * 2, polygonelements);
						batchcount = 0;
						ac = varray_color;
						at = varray_texcoord;
						av = varray_vertex;
					}
				}
				x += w;
			}
			R_Mesh_Draw(batchcount * 4, batchcount * 2, polygonelements);
			break;
		case DRAWQUEUE_MESH:
			mesh = (void *)(dq + 1);
			m.tex[0] = R_GetTexture(mesh->texture);
			R_Mesh_ResizeCheck(mesh->numvertices);
			memcpy(varray_vertex, mesh->vertices, sizeof(float[4]) * mesh->numvertices);
			memcpy(varray_texcoord, mesh->texcoords, sizeof(float[2]) * mesh->numvertices);
			memcpy(varray_color, mesh->colors, sizeof(float[4]) * mesh->numvertices);
			R_Mesh_Draw(mesh->numverts, mesh->numtriangles, mesh->indices);
			currentpic = "\0";
			break;
		}
	}

	if (!v_hwgamma.integer)
	{
		t = v_contrast.value * (float) (1 << v_overbrightbits.integer);
		if (t >= 1.01f)
		{
			m.blendfunc1 = GL_DST_COLOR;
			m.blendfunc2 = GL_ONE;
			m.depthdisable = true;
			R_Mesh_State(&m);
			while (t >= 1.01f)
			{
				cr = t - 1.0f;
				if (cr > 1.0f)
					cr = 1.0f;
				varray_color[0] = varray_color[4] = varray_color[ 8] = varray_color[12] = cr;
				varray_color[1] = varray_color[5] = varray_color[ 9] = varray_color[13] = cr;
				varray_color[2] = varray_color[6] = varray_color[10] = varray_color[14] = cr;
				varray_texcoord[0] = 0;varray_texcoord[1] = 0;
				varray_texcoord[2] = 0;varray_texcoord[3] = 0;
				varray_texcoord[4] = 0;varray_texcoord[5] = 0;
				varray_vertex[0] = -5000;varray_vertex[1] = -5000;varray_vertex[2] = 0;
				varray_vertex[4] = 10000;varray_vertex[1] = -5000;varray_vertex[2] = 0;
				varray_vertex[8] = -5000;varray_vertex[1] = 10000;varray_vertex[2] = 0;
				R_Mesh_Draw(3, 1, polygonelements);
				t *= 0.5;
			}
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
#else
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
			if (strcmp("gfx/conchars", currentpic))
			{
				if (batch)
				{
					batch = false;
					qglEnd();
				}
				currentpic = "gfx/conchars";
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
			qglVertexPointer(3, GL_FLOAT, sizeof(float[4]), mesh->vertices);CHECKGLERROR
			qglTexCoordPointer(2, GL_FLOAT, sizeof(float[2]), mesh->texcoords);CHECKGLERROR
			qglColorPointer(4, GL_FLOAT, sizeof(float[4]), mesh->colors);CHECKGLERROR
			qglEnableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			qglEnableClientState(GL_COLOR_ARRAY);CHECKGLERROR
			GL_DrawRangeElements(0, mesh->numvertices, mesh->numindices, mesh->indices);
			qglDisableClientState(GL_VERTEX_ARRAY);CHECKGLERROR
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);CHECKGLERROR
			qglDisableClientState(GL_COLOR_ARRAY);CHECKGLERROR

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
#endif

