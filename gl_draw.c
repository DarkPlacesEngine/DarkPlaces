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

cvar_t		scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1"};

rtexture_t	*char_texture;

typedef struct
{
	rtexture_t	*tex;
} glpic_t;

rtexture_t	*conbacktex;

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	// FIXME: qpic is evil
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
}
cachepic_t;

#define	MAX_CACHED_PICS		256
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int			pic_texels;
int			pic_count;

rtexturepool_t *drawtexturepool;

/*
================
Draw_CachePic
================
*/
// FIXME: qpic is evil
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;
	rtexture_t	*tex;

	for (pic = menu_cachepics, i = 0;i < menu_numcachepics;pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

	// FIXME: move this to menu code
	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
	{
		dat = (qpic_t *)COM_LoadFile (path, false);
		if (!dat)
			Sys_Error("unable to load gfx/menuplyr.lmp");
		SwapPic (dat);

		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);
	}

	// load the pic from disk
	if ((tex = loadtextureimage(drawtexturepool, path, 0, 0, false, false, true)))
	{
		// load the pic from an image file
		pic->pic.width = image_width;
		pic->pic.height = image_height;
		gl = (glpic_t *)pic->pic.data;
		gl->tex = tex;
		return &pic->pic;
	}
	else
	{
		qpic_t *p;
		// load the pic from gfx.wad
		p = W_GetLumpName (path);
		if (!p)
			Sys_Error ("Draw_CachePic: failed to load %s", path);
		pic->pic.width = p->width;
		pic->pic.height = p->height;
		gl = (glpic_t *)pic->pic.data;
		gl->tex = R_LoadTexture (drawtexturepool, path, p->width, p->height, p->data, TEXTYPE_QPALETTE, TEXF_ALPHA | TEXF_PRECACHE);
		return &pic->pic;
	}
}

/*
===============
Draw_Init
===============
*/
static void gl_draw_start(void)
{
	int i;
	byte *draw_chars;

	menu_numcachepics = 0;

	drawtexturepool = R_AllocTexturePool();
	char_texture = loadtextureimage (drawtexturepool, "conchars", 0, 0, false, false, true);
	if (!char_texture)
	{
		draw_chars = W_GetLumpName ("conchars");
		// convert font to proper transparent color
		for (i = 0;i < 128 * 128;i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;

		// now turn into texture
		char_texture = R_LoadTexture (drawtexturepool, "charset", 128, 128, draw_chars, TEXTYPE_QPALETTE, TEXF_ALPHA | TEXF_PRECACHE);
	}

	conbacktex = loadtextureimage(drawtexturepool, "gfx/conback", 0, 0, false, false, true);
}

static void gl_draw_shutdown(void)
{
	R_FreeTexturePool(&drawtexturepool);

	menu_numcachepics = 0;
}

void SHOWLMP_clear(void);
static void gl_draw_newmap(void)
{
	SHOWLMP_clear();
}

extern char engineversion[40];
int engineversionx, engineversiony;

void GL_Draw_Init (void)
{
	int i;
	Cvar_RegisterVariable (&scr_conalpha);

	for (i = 0;i < 40 && engineversion[i];i++)
		engineversion[i] |= 0x80; // shift to orange
	engineversionx = vid.conwidth - strlen(engineversion) * 8 - 8;
	engineversiony = vid.conheight - 8;

	menu_numcachepics = 0;

	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	if (!r_render.integer)
		return;
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(char_texture));
	CHECKGLERROR
	// LordHavoc: NEAREST mode on text if not scaling up
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

	if (lighthalf)
		glColor3f(0.5f,0.5f,0.5f);
	else
		glColor3f(1.0f,1.0f,1.0f);
	CHECKGLERROR
	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
	glEnd ();
	CHECKGLERROR

	// LordHavoc: revert to LINEAR mode
//	if (vid.realwidth <= (int) vid.conwidth)
//	{
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	}
}

/*
================
Draw_String
================
*/
// LordHavoc: sped this up a lot, and added maxlen
void Draw_String (int x, int y, char *str, int maxlen)
{
	int num;
	float frow, fcol;
	if (!r_render.integer)
		return;
	if (y <= -8 || y >= (int) vid.conheight || x >= (int) vid.conwidth || *str == 0) // completely offscreen or no text to print
		return;
	if (maxlen < 1)
		maxlen = strlen(str);
	else if (maxlen > (int) strlen(str))
		maxlen = strlen(str);
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(char_texture));

	// LordHavoc: NEAREST mode on text if not scaling up
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

	if (lighthalf)
		glColor3f(0.5f,0.5f,0.5f);
	else
		glColor3f(1.0f,1.0f,1.0f);
	CHECKGLERROR
	glBegin (GL_QUADS);
	while (maxlen-- && x < (int) vid.conwidth) // stop rendering when out of characters or room
	{
		if ((num = *str++) != 32) // skip spaces
		{
			frow = (float) ((int) num >> 4)*0.0625;
			fcol = (float) ((int) num & 15)*0.0625;
			glTexCoord2f (fcol         , frow         );glVertex2f (x, y);
			glTexCoord2f (fcol + 0.0625, frow         );glVertex2f (x+8, y);
			glTexCoord2f (fcol + 0.0625, frow + 0.0625);glVertex2f (x+8, y+8);
			glTexCoord2f (fcol         , frow + 0.0625);glVertex2f (x, y+8);
		}
		x += 8;
	}
	glEnd ();
	CHECKGLERROR

	// LordHavoc: revert to LINEAR mode
//	if (vid.realwidth < (int) vid.conwidth)
//	{
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	}
}

void Draw_AdditiveString (int x, int y, char *str, int maxlen)
{
	if (!r_render.integer)
		return;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	CHECKGLERROR
	Draw_String(x, y, str, maxlen);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGLERROR
}

void Draw_GenericPic (rtexture_t *tex, float red, float green, float blue, float alpha, int x, int y, int width, int height)
{
	if (!r_render.integer)
		return;
	if (lighthalf)
		glColor4f(red * 0.5f, green * 0.5f, blue * 0.5f, alpha);
	else
		glColor4f(red, green, blue, alpha);
	CHECKGLERROR
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(tex));
	CHECKGLERROR
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);glVertex2f (x, y);
	glTexCoord2f (1, 0);glVertex2f (x+width, y);
	glTexCoord2f (1, 1);glVertex2f (x+width, y+height);
	glTexCoord2f (0, 1);glVertex2f (x, y+height);
	glEnd ();
	CHECKGLERROR
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	if (pic)
		Draw_GenericPic(((glpic_t *)pic->data)->tex, 1,1,1,alpha, x,y,pic->width, pic->height);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	if (pic)
		Draw_GenericPic(((glpic_t *)pic->data)->tex, 1,1,1,1, x,y,pic->width, pic->height);
}


void Draw_AdditivePic (int x, int y, qpic_t *pic)
{
	if (pic)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		CHECKGLERROR
		Draw_GenericPic(((glpic_t *)pic->data)->tex, 1,1,1,1, x,y,pic->width, pic->height);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		CHECKGLERROR
	}
}


/*
=============
Draw_PicTranslate

Only used for the player color selection menu
=============
*/
void Draw_PicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				i, c;
	byte			*trans, *src, *dest;
	rtexture_t		*rt;

	if (pic == NULL)
		return;

	c = pic->width * pic->height;
	src = menuplyr_pixels;
	dest = trans = Mem_Alloc(tempmempool, c);
	for (i = 0;i < c;i++)
		*dest++ = translation[*src++];

	rt = R_LoadTexture (drawtexturepool, "translatedplayerpic", pic->width, pic->height, trans, TEXTYPE_QPALETTE, TEXF_ALPHA | TEXF_PRECACHE);
	Mem_Free(trans);

	if (!r_render.integer)
		return;
	Draw_GenericPic (rt, 1,1,1,1, x, y, pic->width, pic->height);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_GenericPic (conbacktex, 1,1,1,scr_conalpha.value * lines / vid.conheight, 0, lines - vid.conheight, vid.conwidth, vid.conheight);
	// LordHavoc: draw version
	Draw_String(engineversionx, lines - vid.conheight + engineversiony, engineversion, 9999);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	if (!r_render.integer)
		return;
	glDisable (GL_TEXTURE_2D);
	CHECKGLERROR
	if (lighthalf)
	{
		byte *tempcolor = (byte *)&d_8to24table[c];
		glColor4ub ((byte) (tempcolor[0] >> 1), (byte) (tempcolor[1] >> 1), (byte) (tempcolor[2] >> 1), tempcolor[3]);
	}
	else
		glColor4ubv ((byte *)&d_8to24table[c]);
	CHECKGLERROR

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	CHECKGLERROR
	glColor3f(1,1,1);
	CHECKGLERROR
	glEnable (GL_TEXTURE_2D);
	CHECKGLERROR
}
//=============================================================================

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	if (!r_render.integer)
		return;
	glViewport (vid.realx, vid.realy, vid.realwidth, vid.realheight);
	CHECKGLERROR

	glMatrixMode(GL_PROJECTION);
	CHECKGLERROR
    glLoadIdentity ();
	CHECKGLERROR
	glOrtho  (0, vid.conwidth, vid.conheight, 0, -99999, 99999);
	CHECKGLERROR

	glMatrixMode(GL_MODELVIEW);
	CHECKGLERROR
    glLoadIdentity ();
	CHECKGLERROR

	glDisable (GL_DEPTH_TEST);
	CHECKGLERROR
	glDisable (GL_CULL_FACE);
	CHECKGLERROR
	glEnable (GL_BLEND);
	CHECKGLERROR
	glEnable(GL_TEXTURE_2D);
	CHECKGLERROR

	// LordHavoc: added this
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	CHECKGLERROR
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	CHECKGLERROR

	glColor3f(1,1,1);
	CHECKGLERROR
}

// LordHavoc: SHOWLMP stuff
#define SHOWLMP_MAXLABELS 256
typedef struct showlmp_s
{
	qboolean	isactive;
	float		x;
	float		y;
	char		label[32];
	char		pic[128];
} showlmp_t;

showlmp_t showlmp[SHOWLMP_MAXLABELS];

void SHOWLMP_decodehide(void)
{
	int i;
	byte *lmplabel;
	lmplabel = MSG_ReadString();
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive && strcmp(showlmp[i].label, lmplabel) == 0)
		{
			showlmp[i].isactive = false;
			return;
		}
}

void SHOWLMP_decodeshow(void)
{
	int i, k;
	byte lmplabel[256], picname[256];
	float x, y;
	strcpy(lmplabel,MSG_ReadString());
	strcpy(picname, MSG_ReadString());
	if (gamemode == GAME_NEHAHRA) // LordHavoc: nasty old legacy junk
	{
		x = MSG_ReadByte();
		y = MSG_ReadByte();
	}
	else
	{
		x = MSG_ReadShort();
		y = MSG_ReadShort();
	}
	k = -1;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
		{
			if (strcmp(showlmp[i].label, lmplabel) == 0)
			{
				k = i;
				break; // drop out to replace it
			}
		}
		else if (k < 0) // find first empty one to replace
			k = i;
	if (k < 0)
		return; // none found to replace
	// change existing one
	showlmp[k].isactive = true;
	strcpy(showlmp[k].label, lmplabel);
	strcpy(showlmp[k].pic, picname);
	showlmp[k].x = x;
	showlmp[k].y = y;
}

void SHOWLMP_drawall(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
			Draw_Pic(showlmp[i].x, showlmp[i].y, Draw_CachePic(showlmp[i].pic));
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}
