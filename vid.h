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
// vid.h -- video driver defs

#ifndef VID_H
#define VID_H

typedef struct
{
	// these are set with VID_GetWindowSize and can change from frame to frame
	int		realx;
	int		realy;
	int		realwidth;
	int		realheight;

	int		conwidth;
	int		conheight;
} viddef_t;

extern	viddef_t	vid;				// global video state
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);

extern cvar_t vid_mode;
extern cvar_t vid_mouse;
extern cvar_t vid_fullscreen;

void VID_InitCvars(void);

void GL_Init (void);

void VID_CheckExtensions(void);

void	VID_Init (void);
// Called at startup

void	VID_Shutdown (void);
// Called at shutdown

int VID_SetMode (int modenum);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

// sets hardware gamma correction, returns false if the device does not support gamma control
int VID_SetGamma (float prescale, float gamma, float scale, float base);

void VID_GetWindowSize (int *x, int *y, int *width, int *height);

void VID_Finish (void);

#endif

