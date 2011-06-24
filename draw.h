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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifndef DRAW_H
#define DRAW_H

// FIXME: move this stuff to cl_screen
typedef struct cachepic_s
{
	// size of pic
	int width, height;
	// this flag indicates that it should be loaded and unloaded on demand
	int autoload;
	// texture flags to upload with
	int texflags;
	// texture may be freed after a while
	int lastusedframe;
	// renderer texture to use
	rtexture_t *tex;
	// used for hash lookups
	struct cachepic_s *chain;
	// flags - CACHEPICFLAG_NEWPIC for example
	unsigned int flags;
	// has alpha?
	qboolean hasalpha;
	// name of pic
	char name[MAX_QPATH];
}
cachepic_t;

typedef enum cachepicflags_e
{
	CACHEPICFLAG_NOTPERSISTENT = 1,
	CACHEPICFLAG_QUIET = 2,
	CACHEPICFLAG_NOCOMPRESSION = 4,
	CACHEPICFLAG_NOCLAMP = 8,
	CACHEPICFLAG_NEWPIC = 16 // disables matching texflags check, because a pic created with Draw_NewPic should not be subject to that
}
cachepicflags_t;

void Draw_Init (void);
void Draw_Frame (void);
cachepic_t *Draw_CachePic_Flags (const char *path, unsigned int cachepicflags);
cachepic_t *Draw_CachePic (const char *path); // standard function with no options, used throughout engine
// create or update a pic's image
cachepic_t *Draw_NewPic(const char *picname, int width, int height, int alpha, unsigned char *pixels);
// free the texture memory used by a pic
void Draw_FreePic(const char *picname);

// a triangle mesh..
// each vertex is 3 floats
// each texcoord is 2 floats
// each color is 4 floats
typedef struct drawqueuemesh_s
{
	rtexture_t *texture;
	int num_triangles;
	int num_vertices;
	int *data_element3i;
	unsigned short *data_element3s;
	float *data_vertex3f;
	float *data_texcoord2f;
	float *data_color4f;
}
drawqueuemesh_t;

enum drawqueue_drawflag_e {
DRAWFLAG_NORMAL,
DRAWFLAG_ADDITIVE,
DRAWFLAG_MODULATE,
DRAWFLAG_2XMODULATE,
DRAWFLAG_SCREEN,
DRAWFLAG_NUMFLAGS,
DRAWFLAG_MASK = 0xFF,   // ONLY R_BeginPolygon()
DRAWFLAG_MIPMAP = 0x100 // ONLY R_BeginPolygon()
};

typedef struct ft2_settings_s
{
	float scale, voffset;
	// cvar parameters (only read on loadfont command)
	int antialias, hinting;
	float outline, blur, shadowx, shadowy, shadowz;
} ft2_settings_t;

#define MAX_FONT_SIZES 16
#define MAX_FONT_FALLBACKS 3
typedef struct dp_font_s
{
	rtexture_t *tex;
	float width_of[256]; // width_of[0] == max width of any char; 1.0f is base width (1/16 of texture width); therefore, all widths have to be <= 1 (does not include scale)
	float maxwidth; // precalculated max width of the font (includes scale)
	char texpath[MAX_QPATH];
	char title[MAX_QPATH];

	int req_face; // requested face index, usually 0
	float req_sizes[MAX_FONT_SIZES]; // sizes to render the font with, 0 still defaults to 16 (backward compatibility when loadfont doesn't get a size parameter) and -1 = disabled
	char fallbacks[MAX_FONT_FALLBACKS][MAX_QPATH];
	int fallback_faces[MAX_FONT_FALLBACKS];
	struct ft2_font_s *ft2;

	ft2_settings_t settings;
}
dp_font_t;

typedef struct dp_fonts_s
{
	dp_font_t *f;
	int maxsize;
}
dp_fonts_t;
extern dp_fonts_t dp_fonts;

#define MAX_FONTS         16 // fonts at the start
#define FONTS_EXPAND       8  // fonts grow when no free slots
#define FONT_DEFAULT     (&dp_fonts.f[0]) // should be fixed width
#define FONT_CONSOLE     (&dp_fonts.f[1]) // REALLY should be fixed width (ls!)
#define FONT_SBAR        (&dp_fonts.f[2]) // must be fixed width
#define FONT_NOTIFY      (&dp_fonts.f[3]) // free
#define FONT_CHAT        (&dp_fonts.f[4]) // free
#define FONT_CENTERPRINT (&dp_fonts.f[5]) // free
#define FONT_INFOBAR     (&dp_fonts.f[6]) // free
#define FONT_MENU        (&dp_fonts.f[7]) // should be fixed width
#define FONT_USER(i)     (&dp_fonts.f[8+i]) // userdefined fonts
#define MAX_USERFONTS    (dp_fonts.maxsize - 8)

// shared color tag printing constants
#define STRING_COLOR_TAG			'^'
#define STRING_COLOR_DEFAULT		7
#define STRING_COLOR_DEFAULT_STR	"^7"
#define STRING_COLOR_RGB_TAG_CHAR	'x'
#define STRING_COLOR_RGB_TAG		"^x"

// all of these functions will set r_defdef.draw2dstage if not in 2D rendering mode (and of course prepare for 2D rendering in that case)

// draw an image (or a filled rectangle if pic == NULL)
void DrawQ_Pic(float x, float y, cachepic_t *pic, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a rotated image
void DrawQ_RotPic(float x, float y, cachepic_t *pic, float width, float height, float org_x, float org_y, float angle, float red, float green, float blue, float alpha, int flags);
// draw a filled rectangle (slightly faster than DrawQ_Pic with pic = NULL)
void DrawQ_Fill(float x, float y, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a text string,
// with optional color tag support,
// returns final unclipped x coordinate
// if outcolor is provided the initial color is read from it, and it is updated at the end with the new value at the end of the text (not at the end of the clipped part)
// the color is tinted by the provided base color
// if r_textshadow is not zero, an additional instance of the text is drawn first at an offset with an inverted shade of gray (black text produces a white shadow, brightly colored text produces a black shadow)
extern float DrawQ_Color[4];
float DrawQ_String(float x, float y, const char *text, size_t maxlen, float scalex, float scaley, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt);
float DrawQ_String_Scale(float x, float y, const char *text, size_t maxlen, float sizex, float sizey, float scalex, float scaley, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt);
float DrawQ_TextWidth(const char *text, size_t maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt);
float DrawQ_TextWidth_UntilWidth(const char *text, size_t *maxlen, float w, float h, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxWidth);
float DrawQ_TextWidth_UntilWidth_TrackColors(const char *text, size_t *maxlen, float w, float h, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth);
float DrawQ_TextWidth_UntilWidth_TrackColors_Scale(const char *text, size_t *maxlen, float w, float h, float sw, float sh, int *outcolor, qboolean ignorecolorcodes, const dp_font_t *fnt, float maxwidth);
// draw a very fancy pic (per corner texcoord/color control), the order is tl, tr, bl, br
void DrawQ_SuperPic(float x, float y, cachepic_t *pic, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags);
// draw a triangle mesh
void DrawQ_Mesh(drawqueuemesh_t *mesh, int flags, qboolean hasalpha);
// set the clipping area
void DrawQ_SetClipArea(float x, float y, float width, float height);
// reset the clipping area
void DrawQ_ResetClipArea(void);
// draw a line
void DrawQ_Line(float width, float x1, float y1, float x2, float y2, float r, float g, float b, float alpha, int flags);
// draw a lot of lines
void DrawQ_Lines (float width, int numlines, const float *vertex3f, const float *color4f, int flags);
// draw a line loop
void DrawQ_LineLoop(drawqueuemesh_t *mesh, int flags);
// resets r_refdef.draw2dstage
void DrawQ_Finish(void);
void DrawQ_ProcessDrawFlag(int flags, qboolean alpha); // sets GL_DepthMask and GL_BlendFunc
void DrawQ_RecalcView(void); // use this when changing r_refdef.view.* from e.g. csqc

rtexture_t *Draw_GetPicTexture(cachepic_t *pic);

void R_DrawGamma(void);

extern rtexturepool_t *drawtexturepool; // used by ft2.c

#endif

