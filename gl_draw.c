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

//#define GL_COLOR_INDEX8_EXT     0x80E5

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

static byte pointerimage[256] =
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
	byte buffer[256][4];
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

static byte *crosshairtexdata[NUMCROSSHAIRS] =
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
	byte data[16*16][4];
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
			data[i][3] = (byte) ((int) (in[i] - '0') * 255 / 7);
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
	//for (pic = cachepics, i = 0;i < numcachepics;pic++, i++)
	//	if (!strcmp (path, pic->name))
	//		return pic;

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
			byte *pix;
			// conchars is a raw image and with the wrong transparent color
			pix = (byte *)p;
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

void GL_BrightenScreen(void)
{
	float f;

	if (r_brightness.value < 0.1f)
		Cvar_SetValue("r_brightness", 0.1f);
	if (r_brightness.value > 5.0f)
		Cvar_SetValue("r_brightness", 5.0f);

	if (r_contrast.value < 0.2f)
		Cvar_SetValue("r_contrast", 0.2f);
	if (r_contrast.value > 1.0f)
		Cvar_SetValue("r_contrast", 1.0f);

	if (!(lighthalf && !hardwaregammasupported) && r_brightness.value < 1.01f && r_contrast.value > 0.99f)
		return;

	if (!r_render.integer)
		return;

	glDisable(GL_TEXTURE_2D);
	CHECKGLERROR
	glEnable(GL_BLEND);
	CHECKGLERROR
	f = r_brightness.value;
	// only apply lighthalf using software color correction if hardware is not available (speed reasons)
	if (lighthalf && !hardwaregammasupported)
		f *= 2;
	if (f >= 1.01f)
	{
		glBlendFunc (GL_DST_COLOR, GL_ONE);
		CHECKGLERROR
		glBegin (GL_TRIANGLES);
		while (f >= 1.01f)
		{
			if (f >= 2)
				glColor3f (1, 1, 1);
			else
				glColor3f (f-1, f-1, f-1);
			glVertex2f (-5000, -5000);
			glVertex2f (10000, -5000);
			glVertex2f (-5000, 10000);
			f *= 0.5;
		}
		glEnd ();
		CHECKGLERROR
	}
	if (r_contrast.value <= 0.99f)
	{
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		CHECKGLERROR
		if (lighthalf && hardwaregammasupported)
			glColor4f (0.5, 0.5, 0.5, 1 - r_contrast.value);
		else
			glColor4f (1, 1, 1, 1 - r_contrast.value);
		CHECKGLERROR
		glBegin (GL_TRIANGLES);
		glVertex2f (-5000, -5000);
		glVertex2f (10000, -5000);
		glVertex2f (-5000, 10000);
		glEnd ();
		CHECKGLERROR
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGLERROR

	glEnable (GL_CULL_FACE);
	CHECKGLERROR
	glEnable (GL_DEPTH_TEST);
	CHECKGLERROR
	glDisable(GL_BLEND);
	CHECKGLERROR
	glEnable(GL_TEXTURE_2D);
	CHECKGLERROR
}

void R_DrawQueue(void)
{
	int pos, num, chartexnum;
	float x, y, w, h, s, t, u, v;
	cachepic_t *pic;
	drawqueue_t *dq;
	char *str, *currentpic;
	int batch, additive;
	unsigned int color;

	if (!r_render.integer)
		return;

	glViewport(vid.realx, vid.realy, vid.realwidth, vid.realheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0, vid.conwidth, vid.conheight, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	chartexnum = R_GetTexture(char_texture);

	additive = false;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	currentpic = "";
	pic = NULL;
	glBindTexture(GL_TEXTURE_2D, 0);
	color = 0;
	glColor4ub(0,0,0,0);

	// LordHavoc: NEAREST mode on text if not scaling up
	/*
	if (vid.realwidth <= (int) vid.conwidth)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		CHECKGLERROR
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		CHECKGLERROR
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		CHECKGLERROR
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		CHECKGLERROR
	}
	*/

	batch = false;
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
					glEnd();
				}
				additive = true;
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			}
		}
		else
		{
			if (additive)
			{
				if (batch)
				{
					batch = false;
					glEnd();
				}
				additive = false;
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
		if (color != dq->color)
		{
			color = dq->color;
			if (lighthalf)
				glColor4ub((byte)((color >> 25) & 0x7F), (byte)((color >> 17) & 0x7F), (byte)((color >> 9) & 0x7F), (byte)(color & 0xFF));
			else
				glColor4ub((byte)((color >> 24) & 0xFF), (byte)((color >> 16) & 0xFF), (byte)((color >> 8) & 0xFF), (byte)(color & 0xFF));
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
						glEnd();
					}
					currentpic = str;
					pic = Draw_CachePic(str);
					glBindTexture(GL_TEXTURE_2D, R_GetTexture(pic->tex));
				}
				if (w == 0)
					w = pic->width;
				if (h == 0)
					h = pic->height;
				if (!batch)
				{
					batch = true;
					glBegin(GL_QUADS);
				}
				//DrawQuad(dq->x, dq->y, w, h, 0, 0, 1, 1);
				glTexCoord2f (0, 0);glVertex2f (x  , y  );
				glTexCoord2f (1, 0);glVertex2f (x+w, y  );
				glTexCoord2f (1, 1);glVertex2f (x+w, y+h);
				glTexCoord2f (0, 1);glVertex2f (x  , y+h);
			}
			else
			{
				if (currentpic[0])
				{
					if (batch)
					{
						batch = false;
						glEnd();
					}
					currentpic = "";
					glBindTexture(GL_TEXTURE_2D, 0);
				}
				if (!batch)
				{
					batch = true;
					glBegin(GL_QUADS);
				}
				//DrawQuad(dq->x, dq->y, dq->scalex, dq->scaley, 0, 0, 1, 1);
				glTexCoord2f (0, 0);glVertex2f (x  , y  );
				glTexCoord2f (1, 0);glVertex2f (x+w, y  );
				glTexCoord2f (1, 1);glVertex2f (x+w, y+h);
				glTexCoord2f (0, 1);glVertex2f (x  , y+h);
			}
			break;
		case DRAWQUEUE_STRING:
			str = (char *)(dq + 1);
			if (strcmp("conchars", currentpic))
			{
				if (batch)
				{
					batch = false;
					glEnd();
				}
				currentpic = "conchars";
				glBindTexture(GL_TEXTURE_2D, chartexnum);
			}
			if (!batch)
			{
				batch = true;
				glBegin(GL_QUADS);
			}
			while ((num = *str++) && x < vid.conwidth)
			{
				if (num != ' ')
				{
					s = (num & 15)*0.0625f + (0.5f / 256.0f);
					t = (num >> 4)*0.0625f + (0.5f / 256.0f);
					u = 0.0625f - (1.0f / 256.0f);
					v = 0.0625f - (1.0f / 256.0f);
					//DrawQuad(x, y, w, h, (num & 15)*0.0625f + (0.5f / 256.0f), (num >> 4)*0.0625f + (0.5f / 256.0f), 0.0625f - (1.0f / 256.0f), 0.0625f - (1.0f / 256.0f));
					glTexCoord2f (s  , t  );glVertex2f (x  , y  );
					glTexCoord2f (s+u, t  );glVertex2f (x+w, y  );
					glTexCoord2f (s+u, t+v);glVertex2f (x+w, y+h);
					glTexCoord2f (s  , t+v);glVertex2f (x  , y+h);
				}
				x += w;
			}
			break;
		}
	}
	if (batch)
		glEnd();
	CHECKGLERROR
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGLERROR

	GL_BrightenScreen();

	glColor3f(1,1,1);
	CHECKGLERROR
}
