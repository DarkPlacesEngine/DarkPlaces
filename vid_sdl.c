/*
Copyright (C) 2003  T. Joseph Carter

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

#include <SDL.h>
#include <stdio.h>

#include "quakedef.h"

// Tell startup code that we have a client
int cl_available = true;

static SDL_Surface *screen;

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	p = SDL_GL_GetProcAddress(name);
	return p;
}

void VID_Init (void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		printf ("Failed to init video: %s\n", SDL_GetError());
}

int VID_InitMode(int fullscreen, int width, int height, int bpp)
{
	int i;
	int flags = SDL_OPENGL;
	char *drivername;

#ifdef WIN32
	drivername = "opengl32.dll";
#elif defined(__APPLE__) && defined(__MACH__)
	drivername = "OpenGL.framework";
#else
	drivername = "libGL.so.1";
#endif

	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		drivername = com_argv[i + 1];
	if (!SDL_GL_LoadLibrary(drivername))
	{   
		Con_Printf("Unable to load GL driver \"%s\"\n: ", drivername, SDL_GetError());
		return false;
	}

	qglGetString = GL_GetProcAddress("glGetString");
	
	// Knghtbrd: should do platform-specific extension string function here

	if (qglGetString == NULL)
	{
		VID_Shutdown();
		Con_Printf("Required OpenGL function glGetString not found\n");
		return false;
	}

//	if (fullscreen)
//		flags |= SDL_FULLSCREEN;

	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);

	screen = SDL_SetVideoMode(width, height, bpp, flags);
	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", width, height, SDL_GetError);
		VID_Shutdown();
		return false;
	}
	
	gl_renderer = qglGetString(GL_RENDERER);
	gl_vendor = qglGetString(GL_VENDOR);
	gl_version = qglGetString(GL_VERSION);
	gl_extensions = qglGetString(GL_EXTENSIONS);
	gl_platform = "SDL";
	// Knghtbrd: should assign platform-specific extensions here
	gl_platformextensions = "";
	
	GL_Init();

	vid_hidden = false;
	return true;
}

void VID_Shutdown (void)
{
//	SDL_Quit();
}

int VID_SetGamma (unsigned short *ramps)
{
	return false;
}

int VID_GetGamma (unsigned short *ramps)
{
	return false;
}

void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = screen->w;
	*height = screen->h;
}

void VID_Finish (void)
{
	qglFinish();
	SDL_GL_SwapBuffers();
}

void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
}

void Sys_SendKeyEvents (void)
{
}

