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
// r_misc.c

#include "quakedef.h"


/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
float CalcFov (float fov_x, float width, float height);
struct
{
	float angles[3];
	char *name;
}
envmapinfo[6] =
{
	{{  0,   0, 0}, "ft"},
	{{  0,  90, 0}, "rt"},
	{{  0, 180, 0}, "bk"},
	{{  0, 270, 0}, "lf"},
	{{-90,  90, 0}, "up"},
	{{ 90,  90, 0}, "dn"}
};
void R_Envmap_f (void)
{
	int		i, size;
	char	filename[256];
	char	basename[256];
	byte	*buffer, gamma[256];

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("envmap <basename> <size>: save out 6 cubic environment map images, usable with loadsky, note that size must one of 128, 256, 512, or 1024 and can't be bigger than your current resolution\n");
		return;
	}

	if (!r_render.value)
		return;

	strcpy(basename, Cmd_Argv(1));
	size = atoi(Cmd_Argv(2));
	if (size != 128 && size != 256 && size != 512 && size != 1024)
	{
		Con_Printf("envmap: size must be one of 128, 256, 512, or 1024\n");
		return;
	}
	if (size > vid.realwidth || size > vid.realheight)
	{
		Con_Printf("envmap: your resolution is not big enough to render that size\n");
		return;
	}

	buffer = malloc(size*size*3);
	if (buffer == NULL)
	{
		Con_Printf("envmap: unable to allocate memory for image\n");
		return;
	}

	BuildGammaTable8((lighthalf && hardwaregammasupported) ? 2.0f : 1.0f, 1, 1, 0, gamma);

//	glDrawBuffer  (GL_FRONT);
//	glReadBuffer  (GL_FRONT);
	glDrawBuffer  (GL_BACK);
	glReadBuffer  (GL_BACK);
	envmap = true;

	r_refdef.x = 0;
	r_refdef.y = 0;
	r_refdef.width = size;
	r_refdef.height = size;

	r_refdef.fov_x = 90;
	r_refdef.fov_y = 90;

	for (i = 0;i < 6;i++)
	{
		VectorCopy(envmapinfo[i].angles, r_refdef.viewangles);
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: clear the screen (around the view as well)
		R_RenderView ();
		glReadPixels (0, 0, size, size, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		sprintf(filename, "env/%s%s.tga", basename, envmapinfo[i].name);
		Image_GammaRemapRGB(buffer, buffer, size * size, gamma, gamma, gamma);
		Image_WriteTGARGB_preflipped(filename, size, size, buffer);
	}

	envmap = false;
	glDrawBuffer  (GL_BACK);
	glReadBuffer  (GL_BACK);

	free(buffer);

	// cause refdef to be fixed
//	vid.recalc_refdef = 1;
}

static void gl_misc_start(void)
{
}

static void gl_misc_shutdown(void)
{
}

static void gl_misc_newmap(void)
{
}

/*
===============
R_Init
===============
*/
static void R_TimeRefresh_f (void);
void GL_Misc_Init (void)
{
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);

	R_RegisterModule("GL_Misc", gl_misc_start, gl_misc_shutdown, gl_misc_newmap);
}

extern void GL_BuildLightmaps (void);

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	r_viewleaf = NULL;
	R_Modules_NewMap();

	GL_BuildLightmaps ();

	SHOWLMP_clear();
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
qboolean intimerefresh = 0;
static void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	intimerefresh = 1;
	start = Sys_DoubleTime ();
	glDrawBuffer (GL_FRONT);
	for (i = 0;i < 128;i++)
	{
		r_refdef.viewangles[0] = 0;
		r_refdef.viewangles[1] = i/128.0*360.0;
		r_refdef.viewangles[2] = 0;
		R_RenderView();
	}
	glDrawBuffer  (GL_BACK);

	stop = Sys_DoubleTime ();
	intimerefresh = 0;
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}


