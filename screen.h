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
// screen.h

#ifndef SCREEN_H
#define SCREEN_H

void CL_Screen_Init (void);
void CL_UpdateScreen (void);
void SCR_CenterPrint(const char *str);

void SCR_BeginLoadingPlaque (void);

// invoke refresh of loading plaque (nothing else seen)
void SCR_UpdateLoadingScreen(qboolean clear);
void SCR_UpdateLoadingScreenIfShown(void);

// pushes an item on the loading screen
void SCR_PushLoadingScreen (qboolean redraw, const char *msg, float len_in_parent);
void SCR_PopLoadingScreen (qboolean redraw);
void SCR_ClearLoadingScreen (qboolean redraw);

extern float scr_con_current; // current height of displayed console

extern int sb_lines;

extern cvar_t scr_viewsize;
extern cvar_t scr_fov;
extern cvar_t showfps;
extern cvar_t showtime;
extern cvar_t showdate;

extern cvar_t crosshair;
extern cvar_t crosshair_size;

extern cvar_t scr_conalpha;
extern cvar_t scr_conalphafactor;
extern cvar_t scr_conalpha2factor;
extern cvar_t scr_conalpha3factor;
extern cvar_t scr_conscroll_x;
extern cvar_t scr_conscroll_y;
extern cvar_t scr_conscroll2_x;
extern cvar_t scr_conscroll2_y;
extern cvar_t scr_conscroll3_x;
extern cvar_t scr_conscroll3_y;
extern cvar_t scr_conbrightness;
extern cvar_t r_letterbox;

extern cvar_t scr_refresh;
extern cvar_t scr_stipple;

extern cvar_t r_stereo_separation;
extern cvar_t r_stereo_angle;
qboolean R_Stereo_Active(void);
extern int r_stereo_side;

typedef struct scr_touchscreenarea_s
{
	const char *pic;
	float rect[4];
	float active;
}
scr_touchscreenarea_t;

extern int scr_numtouchscreenareas;
extern scr_touchscreenarea_t scr_touchscreenareas[16];

#endif

