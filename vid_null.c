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

#include <signal.h>
#include <dlfcn.h>
#include "quakedef.h"

int cl_available = false;

// global video state
viddef_t vid;

void VID_Shutdown(void)
{
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	Sys_Quit();
	exit(0);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = 1;
	*height = 1;
}

void VID_Finish (void)
{
}

int VID_SetGamma(float prescale, float gamma, float scale, float base)
{
	return FALSE;
}

void VID_Init(int fullscreen, int width, int height, int bpp)
{
	InitSig(); // trap evil signals
}

int GL_OpenLibrary(const char *name)
{
	return false;
}

void GL_CloseLibrary(void)
{
}

void *GL_GetProcAddress(const char *name)
{
	return NULL;
}

void Sys_SendKeyEvents(void)
{
}

void IN_Commands(void)
{
}

void IN_Init(void)
{
}

void IN_Shutdown(void)
{
}

void IN_Move(usercmd_t *cmd)
{
}
