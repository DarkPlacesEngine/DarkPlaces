/*
	vid_3dfxsvga.c

	OpenGL device driver for 3Dfx chipsets running Linux

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  Nelson Rush.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#include "quakedef.h"
#include "sys.h"
#include "console.h"
#include "sbar.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <dlfcn.h>

#include <GL/gl.h>
#include <GL/fxmesa.h>
#include <glide/sst1vid.h>


viddef_t	vid;	// global video state

static void	*dlhand = NULL;

static fxMesaContext fc = NULL;
static int	scr_width, scr_height;

int	VID_options_items = 0;

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
	if (!fc)
		return;

	fxMesaDestroyContext(fc);

	GL_CloseLibrary();
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	Host_Shutdown();
	abort();
	exit(0);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

typedef void (GLAPIENTRY *gl3DfxSetDitherModeEXT_FUNC) (GrDitherMode_t mode);

void VID_SetupDithering(void)
{
	Con_Printf ("Dithering: ");

	dlhand = dlopen (NULL, RTLD_LAZY);

	if (dlhand == NULL) {
		Con_SafePrintf ("unable to set.\n");
		return;
	}

	if (strstr(gl_extensions, "3DFX_set_dither_mode")) {
		gl3DfxSetDitherModeEXT_FUNC dither_select = NULL;

		dither_select = (void *) dlsym(dlhand, "gl3DfxSetDitherModeEXT");

		if (COM_CheckParm ("-dither_2x2")) {
			dither_select(GR_DITHER_2x2);
			Con_Printf ("2x2.\n");
		} else if (COM_CheckParm ("-dither_4x4")) {
			dither_select(GR_DITHER_4x4);
			Con_Printf ("4x4.\n");
		} else {
			qglDisable(GL_DITHER);
			Con_Printf ("disabled.\n");
		}
	}
	dlclose(dlhand);
	dlhand = NULL;
}

/*
=================
VID_GetWindowSize
=================
*/
void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

void VID_Finish (void)
{
	if (r_render.integer)
	{
		qglFinish();
		fxMesaSwapBuffers();
	}
}

static int resolutions[][3]={
	{ 320,	200,	GR_RESOLUTION_320x200 },
	{ 320,	240,	GR_RESOLUTION_320x240 },
	{ 400,	256,	GR_RESOLUTION_400x256 },
	{ 400,	300,	GR_RESOLUTION_400x300 },
	{ 512,	256,	GR_RESOLUTION_512x256 },
	{ 512,	384,	GR_RESOLUTION_512x384 },
	{ 640,	200,	GR_RESOLUTION_640x200 },
	{ 640,	350,	GR_RESOLUTION_640x350 },
	{ 640,	400,	GR_RESOLUTION_640x400 },
	{ 640,	480,	GR_RESOLUTION_640x480 },
	{ 800,	600,	GR_RESOLUTION_800x600 },
	{ 856,	480,	GR_RESOLUTION_856x480 },
	{ 960,	720,	GR_RESOLUTION_960x720 },
#ifdef GR_RESOLUTION_1024x768
	{ 1024,	768,	GR_RESOLUTION_1024x768 },
#endif
#ifdef GR_RESOLUTION_1152x864
	{ 1152,	864,	GR_RESOLUTION_1152x864 },
#endif
#ifdef GR_RESOLUTION_1280x960
	{ 1280,	960,	GR_RESOLUTION_1280x960 },
#endif
#ifdef GR_RESOLUTION_1280x1024
	{ 1280,	1024,	GR_RESOLUTION_1280x1024 },
#endif
#ifdef GR_RESOLUTION_1600x1024
	{ 1600,	1024,	GR_RESOLUTION_1600x1024 },
#endif
#ifdef GR_RESOLUTION_1600x1200
	{ 1600,	1200,	GR_RESOLUTION_1600x1200 },
#endif
#ifdef GR_RESOLUTION_1792x1344
	{ 1792,	1344,	GR_RESOLUTION_1792x1344 },
#endif
#ifdef GR_RESOLUTION_1856x1392
	{ 1856,	1392,	GR_RESOLUTION_1856x1392 },
#endif
#ifdef GR_RESOLUTION_1920x1440
	{ 1920,	1440,	GR_RESOLUTION_1920x1440 },
#endif
#ifdef GR_RESOLUTION_2048x1536
	{ 2048,	1536,	GR_RESOLUTION_2048x1536 },
#endif
#ifdef GR_RESOLUTION_2048x2048
	{ 2048,	2048,	GR_RESOLUTION_2048x2048 }
#endif
};

#define NUM_RESOLUTIONS		(sizeof(resolutions)/(sizeof(int)*3))


static int
findres(int *width, int *height)
{
	int i;

	for(i=0; i < NUM_RESOLUTIONS; i++) {
		if((*width <= resolutions[i][0]) &&
		   (*height <= resolutions[i][1])) {
			*width = resolutions[i][0];
			*height = resolutions[i][1];
			return resolutions[i][2];
		}
	}

	*width = 640;
	*height = 480;
	return GR_RESOLUTION_640x480;
}

int VID_SetGamma(float prescale, float gamma, float scale, float base)
{
	return FALSE;
}

void VID_Init(void)
{
	int i;
	GLint attribs[32];
	int width = 640, height = 480;

	GL_OpenLibrary();

// set vid parameters
	attribs[0] = FXMESA_DOUBLEBUFFER;
	attribs[1] = FXMESA_ALPHA_SIZE;
	attribs[2] = 1;
	attribs[3] = FXMESA_DEPTH_SIZE;
	attribs[4] = 1;
	attribs[5] = FXMESA_NONE;

	if ((i = COM_CheckParm("-width")) != 0)
		width = atoi(com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		height = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	fc = fxMesaCreateContext(0, findres(&width, &height), GR_REFRESH_75Hz,
		attribs);
	if (!fc)
		Sys_Error("Unable to create 3DFX context.\n");

	scr_width = width;
	scr_height = height;

	fxMesaMakeCurrent(fc);

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;

	InitSig(); // trap evil signals

	GL_Init();

	VID_SetupDithering(); // 3DFX specific

	vid_hidden = false;

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);
}

void VID_ExtraOptionDraw(unsigned int options_draw_cursor)
{
}

void VID_ExtraOptionCmd(int option_cursor)
{
}

void VID_SetCaption (char *text)
{
}

void VID_HandlePause (qboolean pause)
{
}

