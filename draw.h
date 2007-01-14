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
	// renderer texture to use
	rtexture_t *tex;
	// used for hash lookups
	struct cachepic_s *chain;
	// name of pic
	char name[MAX_QPATH];
}
cachepic_t;

void Draw_Init (void);
cachepic_t *Draw_CachePic (const char *path, qboolean persistent);
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
DRAWFLAG_NUMFLAGS
};

// shared color tag printing constants
#define STRING_COLOR_TAG			'^'
#define STRING_COLOR_DEFAULT		7
#define STRING_COLOR_DEFAULT_STR	"^7"

// all of these functions will set r_defdef.draw2dstage if not in 2D rendering mode (and of course prepare for 2D rendering in that case)

// draw an image (or a filled rectangle if pic == NULL)
void DrawQ_Pic(float x, float y, cachepic_t *pic, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a text string
void DrawQ_String(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags);
// draw a text string that supports color tags (colorindex can either be NULL, -1 to make it choose the default color or valid index to start with)
float DrawQ_ColoredString( float x, float y, const char *text, int maxlen, float scalex, float scaley, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor );
// draw a very fancy pic (per corner texcoord/color control), the order is tl, tr, bl, br
void DrawQ_SuperPic(float x, float y, cachepic_t *pic, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags);
// draw a triangle mesh
void DrawQ_Mesh(drawqueuemesh_t *mesh, int flags);
// set the clipping area
void DrawQ_SetClipArea(float x, float y, float width, float height);
// reset the clipping area
void DrawQ_ResetClipArea(void);
// draw a line
void DrawQ_Line(float width, float x1, float y1, float x2, float y2, float r, float g, float b, float alpha, int flags);
// draw a line loop
void DrawQ_LineLoop(drawqueuemesh_t *mesh, int flags);
// resets r_refdef.draw2dstage
void DrawQ_Finish(void);

void R_DrawGamma(void);

#endif

