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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"

//#define GL_COLOR_INDEX8_EXT     0x80E5

cvar_t		qsg_version = {"qsg_version", "1"};
cvar_t		scr_conalpha = {"scr_conalpha", "1"};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;

int			char_texture;

typedef struct
{
	int		texnum;
} glpic_t;

int			conbacktexnum;

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	gl->texnum = GL_LoadTexture (name, p->width, p->height, p->data, false, true, 1);
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadMallocFile (path, false);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = loadtextureimage(path, 0, 0, false, false);
	if (!gl->texnum)
		gl->texnum = GL_LoadTexture (path, dat->width, dat->height, dat->data, false, true, 1);

	qfree(dat);

	return &pic->pic;
}

extern void LoadSky_f(void);

/*
===============
Draw_Init
===============
*/
void rmain_registercvars();

void gl_draw_start()
{
	int		i;

	char_texture = loadtextureimage ("conchars", 0, 0, false, false);
	if (!char_texture)
	{
		draw_chars = W_GetLumpName ("conchars");
		for (i=0 ; i<128*128 ; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;	// proper transparent color

		// now turn them into textures
		char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);
	}

	conbacktexnum = loadtextureimage("gfx/conback", 0, 0, false, false);

	// get the other pics we need
	draw_disc = Draw_PicFromWad ("disc");
}

void gl_draw_shutdown()
{
}

char engineversion[40];
int engineversionx, engineversiony;

extern void GL_Textures_Init();
void GL_Draw_Init (void)
{
	int i;
	Cvar_RegisterVariable (&qsg_version);
	Cvar_RegisterVariable (&scr_conalpha);

	Cmd_AddCommand ("loadsky", &LoadSky_f);

#if defined(__linux__)
	sprintf (engineversion, "DarkPlaces Linux   GL %.2f build %3i", (float) VERSION, buildnumber);
#elif defined(WIN32)
	sprintf (engineversion, "DarkPlaces Windows GL %.2f build %3i", (float) VERSION, buildnumber);
#else
	sprintf (engineversion, "DarkPlaces Unknown GL %.2f build %3i", (float) VERSION, buildnumber);
#endif
	for (i = 0;i < 40 && engineversion[i];i++)
		engineversion[i] += 0x80; // shift to orange
	engineversionx = vid.width - strlen(engineversion) * 8 - 8;
	engineversiony = vid.height - 8;

	GL_Textures_Init();
	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown);
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

	if (!r_render.value)
		return;
	glBindTexture(GL_TEXTURE_2D, char_texture);
	// LordHavoc: NEAREST mode on text if not scaling up
	if (glwidth < (int) vid.width)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glColor3f(1,1,1);
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

	// LordHavoc: revert to LINEAR mode
	if (glwidth < (int) vid.width)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
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
	if (!r_render.value)
		return;
	if (y <= -8 || y >= (int) vid.height || x >= (int) vid.width || *str == 0) // completely offscreen or no text to print
		return;
	if (maxlen < 1)
		maxlen = strlen(str);
	else if (maxlen > (int) strlen(str))
		maxlen = strlen(str);
	glBindTexture(GL_TEXTURE_2D, char_texture);

	// LordHavoc: NEAREST mode on text if not scaling up
	if (glwidth < (int) vid.width)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glColor3f(1,1,1);
	glBegin (GL_QUADS);
	while (maxlen-- && x < (int) vid.width) // stop rendering when out of characters or room
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

	// LordHavoc: revert to LINEAR mode
	if (glwidth < (int) vid.width)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

void Draw_GenericPic (int texnum, float red, float green, float blue, float alpha, int x, int y, int width, int height)
{
	if (!r_render.value)
		return;
	glColor4f(red,green,blue,alpha);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);glVertex2f (x, y);
	glTexCoord2f (1, 0);glVertex2f (x+width, y);
	glTexCoord2f (1, 1);glVertex2f (x+width, y+height);
	glTexCoord2f (0, 1);glVertex2f (x, y+height);
	glEnd ();
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	Draw_GenericPic(((glpic_t *)pic->data)->texnum, 1,1,1,alpha, x,y,pic->width, pic->height);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	Draw_GenericPic(((glpic_t *)pic->data)->texnum, 1,1,1,1, x,y,pic->width, pic->height);
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

	c = pic->width * pic->height;
	src = menuplyr_pixels;
	dest = trans = qmalloc(c);
	for (i = 0;i < c;i++)
		*dest++ = translation[*src++];

	c = GL_LoadTexture ("translatedplayerpic", pic->width, pic->height, trans, false, true, 1);
	qfree(trans);

	if (!r_render.value)
		return;
	Draw_GenericPic (c, 1,1,1,1, x, y, pic->width, pic->height);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_GenericPic (conbacktexnum, 1,1,1,scr_conalpha.value*lines/vid.height, 0, lines - vid.height, vid.width, vid.height);
	// LordHavoc: draw version
	Draw_String(engineversionx, lines - vid.height + engineversiony, engineversion, 9999);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	if (!r_render.value)
		return;
	glDisable (GL_TEXTURE_2D);
	glColor3f (host_basepal[c*3]/255.0, host_basepal[c*3+1]/255.0, host_basepal[c*3+2]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f(1,1,1);
	glEnable (GL_TEXTURE_2D);
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
	if (!r_render.value)
		return;
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glEnable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);

	// LordHavoc: added this
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glColor3f(1,1,1);
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

void SHOWLMP_decodehide()
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

void SHOWLMP_decodeshow()
{
	int i, k;
	byte lmplabel[256], picname[256];
	float x, y;
	strcpy(lmplabel,MSG_ReadString());
	strcpy(picname, MSG_ReadString());
	x = MSG_ReadByte();
	y = MSG_ReadByte();
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

void SHOWLMP_drawall()
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
			Draw_Pic(showlmp[i].x, showlmp[i].y, Draw_CachePic(showlmp[i].pic));
}

void SHOWLMP_clear()
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}
