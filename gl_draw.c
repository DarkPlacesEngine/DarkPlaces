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
#include "image.h"

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
	return R_LoadTexture2D(drawtexturepool, "mousepointer", 16, 16, &buffer[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
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
	return R_LoadTexture2D(drawtexturepool, va("crosshair%i", num), 16, 16, &data[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
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
	pic->tex = loadtextureimage(drawtexturepool, path, 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (pic->tex == NULL && !strncmp(path, "gfx/", 4))
	{
		// compatibility with older versions
		pic->tex = loadtextureimage(drawtexturepool, path + 4, 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
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
				pic->tex = R_LoadTexture2D(drawtexturepool, path, 128, 128, pix, TEXTYPE_PALETTE, TEXF_ALPHA | TEXF_PRECACHE, palette_complete);
			}
			else
				pic->tex = R_LoadTexture2D(drawtexturepool, path, p->width, p->height, p->data, TEXTYPE_PALETTE, TEXF_ALPHA | TEXF_PRECACHE, palette_complete);
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
		Con_Printf ("Draw_CachePic: failed to load %s\n", path);
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
	pic->tex = R_LoadTexture2D(drawtexturepool, picname, width, height, pixels, TEXTYPE_RGBA, alpha ? TEXF_ALPHA : 0, NULL);
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

int quadelements[768];
void R_DrawQueue(void)
{
	int pos, num, chartexnum, overbright, texnum, additive, batch;
	float x, y, w, h, s, t, u, v, cr, cg, cb, ca, *av, *at;
	cachepic_t *pic;
	drawqueue_t *dq;
	char *str, *currentpic;
	int batchcount;
	unsigned int color;
	drawqueuemesh_t *mesh;
	rmeshstate_t m;

	if (!r_render.integer)
		return;

	if (!quadelements[1])
	{
		// elements for rendering a series of quads as triangles
		for (batch = 0, pos = 0, num = 0;batch < 128;batch++, num += 4)
		{
			quadelements[pos++] = num;
			quadelements[pos++] = num + 1;
			quadelements[pos++] = num + 2;
			quadelements[pos++] = num;
			quadelements[pos++] = num + 2;
			quadelements[pos++] = num + 3;
		}
	}
	GL_SetupView_ViewPort(vid.realx, vid.realy, vid.realwidth, vid.realheight);
	GL_SetupView_Mode_Ortho(0, 0, vid.conwidth, vid.conheight, -10, 100);
	qglDepthFunc(GL_LEQUAL);
	R_Mesh_Start();
	R_Mesh_Matrix(&r_identitymatrix);

	memset(&m, 0, sizeof(m));
	chartexnum = R_GetTexture(char_texture);
	m.tex[0] = chartexnum;
	R_Mesh_TextureState(&m);

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
				currentpic = str;
				if (*str)
				{
					pic = Draw_CachePic(str);
					m.tex[0] = R_GetTexture(pic->tex);
				}
				else
					m.tex[0] = 0;
				R_Mesh_TextureState(&m);
			}
			if (*str)
			{
				if (w == 0)
					w = pic->width;
				if (h == 0)
					h = pic->height;
			}
			varray_texcoord[0][ 0] = 0;varray_texcoord[0][ 1] = 0;
			varray_texcoord[0][ 4] = 1;varray_texcoord[0][ 5] = 0;
			varray_texcoord[0][ 8] = 1;varray_texcoord[0][ 9] = 1;
			varray_texcoord[0][12] = 0;varray_texcoord[0][13] = 1;
			varray_vertex[ 0] = x  ;varray_vertex[ 1] = y  ;varray_vertex[ 2] = 10;
			varray_vertex[ 4] = x+w;varray_vertex[ 5] = y  ;varray_vertex[ 6] = 10;
			varray_vertex[ 8] = x+w;varray_vertex[ 9] = y+h;varray_vertex[10] = 10;
			varray_vertex[12] = x  ;varray_vertex[13] = y+h;varray_vertex[14] = 10;
			GL_Color(cr, cg, cb, ca);
			R_Mesh_Draw(4, 2, quadelements);
			break;
		case DRAWQUEUE_STRING:
			str = (char *)(dq + 1);
			if (strcmp("gfx/conchars", currentpic))
			{
				currentpic = "gfx/conchars";
				m.tex[0] = chartexnum;
				R_Mesh_TextureState(&m);
			}
			batchcount = 0;
			at = varray_texcoord[0];
			av = varray_vertex;
			GL_Color(cr, cg, cb, ca);
			while ((num = *str++) && x < vid.conwidth)
			{
				if (num != ' ')
				{
					s = (num & 15)*0.0625f + (0.5f / 256.0f);
					t = (num >> 4)*0.0625f + (0.5f / 256.0f);
					u = 0.0625f - (1.0f / 256.0f);
					v = 0.0625f - (1.0f / 256.0f);
					at[ 0] = s  ;at[ 1] = t  ;
					at[ 4] = s+u;at[ 5] = t  ;
					at[ 8] = s+u;at[ 9] = t+v;
					at[12] = s  ;at[13] = t+v;
					av[ 0] = x  ;av[ 1] = y  ;av[ 2] = 10;
					av[ 4] = x+w;av[ 5] = y  ;av[ 6] = 10;
					av[ 8] = x+w;av[ 9] = y+h;av[10] = 10;
					av[12] = x  ;av[13] = y+h;av[14] = 10;
					at += 16;
					av += 16;
					batchcount++;
					if (batchcount >= 128)
					{
						R_Mesh_Draw(batchcount * 4, batchcount * 2, quadelements);
						batchcount = 0;
						at = varray_texcoord[0];
						av = varray_vertex;
					}
				}
				x += w;
			}
			if (batchcount > 0)
				R_Mesh_Draw(batchcount * 4, batchcount * 2, quadelements);
			break;
		case DRAWQUEUE_MESH:
			mesh = (void *)(dq + 1);
			m.tex[0] = R_GetTexture(mesh->texture);
			R_Mesh_TextureState(&m);
			R_Mesh_ResizeCheck(mesh->numvertices);
			memcpy(varray_vertex, mesh->vertices, sizeof(float[4]) * mesh->numvertices);
			memcpy(varray_texcoord[0], mesh->texcoords, sizeof(float[4]) * mesh->numvertices);
			memcpy(varray_color, mesh->colors, sizeof(float[4]) * mesh->numvertices);
			GL_UseColorArray();
			R_Mesh_Draw(mesh->numvertices, mesh->numtriangles, mesh->indices);
			currentpic = "\0";
			break;
		}
	}

	if (!v_hwgamma.integer)
	{
		// we use one big triangle for all the screen blends
		varray_texcoord[0][0] = 0;varray_texcoord[0][1] = 0;
		varray_texcoord[0][4] = 0;varray_texcoord[0][5] = 0;
		varray_texcoord[0][8] = 0;varray_texcoord[0][9] = 0;
		varray_vertex[0] = -5000;varray_vertex[1] = -5000;varray_vertex[2] = 10;
		varray_vertex[4] = 10000;varray_vertex[5] = -5000;varray_vertex[6] = 10;
		varray_vertex[8] = -5000;varray_vertex[9] = 10000;varray_vertex[10] = 10;
		// all the blends ignore depth
		memset(&m, 0, sizeof(m));
		m.depthdisable = true;
		t = v_contrast.value * (float) (1 << v_overbrightbits.integer);
		if (t >= 1.01f)
		{
			m.blendfunc1 = GL_DST_COLOR;
			m.blendfunc2 = GL_ONE;
			R_Mesh_State(&m);
			while (t >= 1.01f)
			{
				cr = t - 1.0f;
				if (cr > 1.0f)
					cr = 1.0f;
				GL_Color(cr, cr, cr, 1);
				R_Mesh_Draw(3, 1, polygonelements);
				t *= 0.5;
			}
		}
		else if (t <= 0.99f)
		{
			m.blendfunc1 = GL_ZERO;
			m.blendfunc2 = GL_SRC_COLOR;
			R_Mesh_State(&m);
			GL_Color(t, t, t, 1);
			R_Mesh_Draw(3, 1, polygonelements);
		}
		if (v_brightness.value >= 0.01f)
		{
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ONE;
			R_Mesh_State(&m);
			GL_Color(v_brightness.value, v_brightness.value, v_brightness.value, 1);
			R_Mesh_Draw(3, 1, polygonelements);
		}
	}
	R_Mesh_Finish();
}

