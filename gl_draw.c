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

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t		qsg_version = {"qsg_version", "1"};
cvar_t		gl_conalpha = {"gl_conalpha", "1"};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;

//int			translate_texture;
int			char_texture;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

int	scrap_uploads;
int scraptexnum[MAX_SCRAPS];

void Scrap_Upload (void)
{
	int		texnum;

	scrap_uploads++;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
		scraptexnum[texnum] = GL_LoadTexture (va("scrapslot%d", texnum), BLOCK_WIDTH, BLOCK_HEIGHT, scrap_texels[texnum], false, true, 1);
	scrap_dirty = false;
}

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

int GL_LoadPicTexture (qpic_t *pic);

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		if (!scraptexnum[texnum])
			scraptexnum[texnum] = GL_LoadTexture (va("scrapslot%d", texnum), BLOCK_WIDTH, BLOCK_HEIGHT, scrap_texels[texnum], false, true, 1);
		gl->texnum = scraptexnum[texnum];
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture (p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
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
	dat = (qpic_t *)COM_LoadTempFile (path, false);
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
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

/*
void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}
*/

extern void LoadSky_f(void);

extern char *QSG_EXTENSIONS;

/*
===============
Draw_Init
===============
*/
void rmain_registercvars();
extern int buildnumber;

void gl_draw_start()
{
	int		i;
	glpic_t	*gl;

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	char_texture = loadtextureimage ("conchars", 0, 0, false, false);
	if (!char_texture)
	{
		draw_chars = W_GetLumpName ("conchars");
		for (i=0 ; i<256*64 ; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;	// proper transparent color

		// now turn them into textures
		char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);
	}

	gl = (glpic_t *)conback->data;
	gl->texnum = loadtextureimage("gfx/conback", 0, 0, false, false);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	memset(scraptexnum, 0, sizeof(scraptexnum));

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
	Cvar_RegisterVariable (&gl_conalpha);

	Cmd_AddCommand ("loadsky", &LoadSky_f);

#ifdef NEHAHRA
#if defined(__linux__)
	sprintf (engineversion, "DPNehahra Linux   GL %.2f build %3i", (float) VERSION, buildnumber);
#elif defined(WIN32)
	sprintf (engineversion, "DPNehahra Windows GL %.2f build %3i", (float) VERSION, buildnumber);
#else
	sprintf (engineversion, "DPNehahra Unknown GL %.2f build %3i", (float) VERSION, buildnumber);
#endif
#else
#if defined(__linux__)
	sprintf (engineversion, "DarkPlaces Linux   GL %.2f build %3i", (float) VERSION, buildnumber);
#elif defined(WIN32)
	sprintf (engineversion, "DarkPlaces Windows GL %.2f build %3i", (float) VERSION, buildnumber);
#else
	sprintf (engineversion, "DarkPlaces Unknown GL %.2f build %3i", (float) VERSION, buildnumber);
#endif
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

	glBindTexture(GL_TEXTURE_2D, char_texture);
	// LordHavoc: NEAREST mode on text if not scaling up
	if ((int) vid.width < glwidth)
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
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
	if (y <= -8 || y >= (int) vid.height || x >= (int) vid.width || *str == 0) // completely offscreen or no text to print
		return;
	if (maxlen < 1)
		maxlen = strlen(str);
	else if (maxlen > (int) strlen(str))
		maxlen = strlen(str);
	glBindTexture(GL_TEXTURE_2D, char_texture);

	// LordHavoc: NEAREST mode on text if not scaling up
	if ((int) vid.width < glwidth)
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
			glTexCoord2f (fcol, frow);
			glVertex2f (x, y);
			glTexCoord2f (fcol + 0.0625, frow);
			glVertex2f (x+8, y);
			glTexCoord2f (fcol + 0.0625, frow + 0.0625);
			glVertex2f (x+8, y+8);
			glTexCoord2f (fcol, frow + 0.0625);
			glVertex2f (x, y+8);
		}
		x += 8;
	}
	glEnd ();

	// LordHavoc: revert to LINEAR mode
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Draw_GenericPic (int texnum, float red, float green, float blue, float alpha, float x, float y, float width, float height)
{
	glDisable(GL_ALPHA_TEST);
//	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glCullFace(GL_FRONT);
	glColor4f(red,green,blue,alpha);
	glBindTexture(GL_TEXTURE_2D, texnum);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+width, y+height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+height);
	glEnd ();
	glColor3f(1,1,1);
//	glEnable(GL_ALPHA_TEST);
//	glDisable (GL_BLEND);
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
//	glDisable(GL_ALPHA_TEST);
//	glEnable (GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glCullFace(GL_FRONT);
	glColor4f(0.8,0.8,0.8,alpha);
	glBindTexture(GL_TEXTURE_2D, gl->texnum);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glColor3f(1,1,1);
//	glEnable(GL_ALPHA_TEST);
//	glDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glColor3f(0.8,0.8,0.8);
	glBindTexture(GL_TEXTURE_2D, gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 || (unsigned)(y + pic->height) > vid.height)
		Sys_Error ("Draw_TransPic: bad coordinates");

//	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glDisable(GL_ALPHA_TEST);
	Draw_Pic (x, y, pic);
//	glDisable(GL_BLEND);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	glBindTexture(GL_TEXTURE_2D, GL_LoadTexture ("translatedplayerpic", 64, 64, (void *) trans, false, true, 1));

	glColor3f(0.8,0.8,0.8);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	// LordHavoc: changed alpha
	//int y = (vid.height >> 1);

	if (lines >= (int) vid.height)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic (0, lines - vid.height, conback, gl_conalpha.value*lines/vid.height);
	//	Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
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
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glEnable (GL_BLEND); // was Disable
//	glEnable (GL_ALPHA_TEST);
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
			Draw_TransPic(showlmp[i].x, showlmp[i].y, Draw_CachePic(showlmp[i].pic));
}

void SHOWLMP_clear()
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}
