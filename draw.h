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
cachepic_t *Draw_NewPic(const char *picname, int width, int height, int alpha, qbyte *pixels);
// free the texture memory used by a pic
void Draw_FreePic(const char *picname);

void R_DrawQueue(void);

#endif

