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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/


float	scr_con_current;
float	scr_conlines;		// lines of console to display

float	oldscreensize, oldfov;
cvar_t	scr_viewsize = {CVAR_SAVE, "viewsize","100"};
cvar_t	scr_fov = {CVAR_SAVE, "fov","90"};	// 10 - 170
cvar_t	scr_conspeed = {CVAR_SAVE, "scr_conspeed","900"}; // LordHavoc: quake used 300
cvar_t	scr_centertime = {0, "scr_centertime","2"};
cvar_t	scr_showram = {CVAR_SAVE, "showram","1"};
cvar_t	scr_showturtle = {CVAR_SAVE, "showturtle","0"};
cvar_t	scr_showpause = {CVAR_SAVE, "showpause","1"};
cvar_t	scr_printspeed = {0, "scr_printspeed","8"};
cvar_t	r_render = {0, "r_render", "1"};
cvar_t	r_brightness = {CVAR_SAVE, "r_brightness", "1"}; // LordHavoc: a method of operating system independent color correction
cvar_t	r_contrast = {CVAR_SAVE, "r_contrast", "1"}; // LordHavoc: a method of operating system independent color correction
cvar_t	gl_dither = {CVAR_SAVE, "gl_dither", "1"}; // whether or not to use dithering

qboolean	scr_initialized;		// ready to draw

int			clearconsole;
int			clearnotify;

int			lightscalebit;
float		lightscale;

qboolean	scr_disabled_for_loading;
//qboolean	scr_drawloading;
//float		scr_disabled_time;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.conheight*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l*8)/2;
		if (l > 0)
		{
			if (remaining < l)
				l = remaining;
			DrawQ_String(x, y, start, l, 8, 8, 1, 1, 1, 1, 0);
			remaining -= l;
			if (remaining <= 0)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	// calculate vision size and alter by aspect, then convert back to angle
	return atan (height / (width / tan(fov_x/360*M_PI))) * 360 / M_PI;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float size;

//	vid.recalc_refdef = 0;

//========================================

// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

// intermission is always full screen
	if (cl.intermission)
	{
		size = 1;
		sb_lines = 0;
	}
	else
	{
		if (scr_viewsize.value >= 120)
			sb_lines = 0;		// no status bar at all
		else if (scr_viewsize.value >= 110)
			sb_lines = 24;		// no inventory
		else
			sb_lines = 24+16+8;
		size = scr_viewsize.value * (1.0 / 100.0);
	}

	if (size >= 1)
	{
		r_refdef.width = vid.realwidth;
		r_refdef.height = vid.realheight;
		r_refdef.x = 0;
		r_refdef.y = 0;
	}
	else
	{
		r_refdef.width = vid.realwidth * size;
		r_refdef.height = vid.realheight * size;
		r_refdef.x = (vid.realwidth - r_refdef.width)/2;
		r_refdef.y = (vid.realheight - r_refdef.height)/2;
	}

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.width, r_refdef.height);

	r_refdef.width = bound(0, r_refdef.width, vid.realwidth);
	r_refdef.height = bound(0, r_refdef.height, vid.realheight);
	r_refdef.x = bound(0, r_refdef.x, vid.realwidth) + vid.realx;
	r_refdef.y = bound(0, r_refdef.y, vid.realheight) + vid.realy;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value-10);
}

//============================================================================

void gl_screen_start(void)
{
}

void gl_screen_shutdown(void)
{
}

void gl_screen_newmap(void)
{
}

/*
==================
SCR_Init
==================
*/
static void R_Envmap_f (void);
void GL_Screen_Init (void)
{
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&r_render);
	Cvar_RegisterVariable (&r_brightness);
	Cvar_RegisterVariable (&r_contrast);
	Cvar_RegisterVariable (&gl_dither);
#ifdef NORENDER
	Cvar_SetValue("r_render", 0);
#endif

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_initialized = true;

	R_RegisterModule("GL_Screen", gl_screen_start, gl_screen_shutdown, gl_screen_newmap);
}



/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam (void)
{
//	if (!scr_showram.integer)
//		return;
//	DrawQ_Pic (32, 0, "ram", 0, 0, 1, 1, 1, 1, 0);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;

	if (cls.state != ca_connected)
		return;

	if (!scr_showturtle.integer)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	DrawQ_Pic (0, 0, "turtle", 0, 0, 1, 1, 1, 1, 0);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.state != ca_connected)
		return;
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	DrawQ_Pic (64, 0, "net", 0, 0, 1, 1, 1, 1, 0);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	cachepic_t	*pic;

	if (cls.state != ca_connected)
		return;

	if (!scr_showpause.integer)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	DrawQ_Pic ((vid.conwidth - pic->width)/2, (vid.conheight - pic->height)/2, "gfx/pause.lmp", 0, 0, 1, 1, 1, 1, 0);
}



/*
==============
SCR_DrawLoading
==============
*/
/*
void SCR_DrawLoading (void)
{
	cachepic_t	*pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic ("gfx/loading.lmp");
	DrawQ_Pic ((vid.conwidth - pic->width)/2, (vid.conheight - pic->height)/2, "gfx/loading.lmp", 0, 0, 1, 1, 1, 1, 0);
}
*/



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.conheight;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.conheight/2;	// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_realframetime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_realframetime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	byte		*buffer, gamma[256];
	char		filename[80];
	char		checkname[MAX_OSPATH];
	int			i;
//
// find a file name to save it to
//
	strcpy(filename,"dp0000.tga");

	for (i=0 ; i<=9999 ; i++)
	{
		filename[2] = (i/1000)%10 + '0';
		filename[3] = (i/ 100)%10 + '0';
		filename[4] = (i/  10)%10 + '0';
		filename[5] = (i/   1)%10 + '0';
		sprintf (checkname, "%s/%s", com_gamedir, filename);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	}
	if (i==10000)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
 	}

	buffer = Mem_Alloc(tempmempool, vid.realwidth*vid.realheight*3);
	glReadPixels (vid.realx, vid.realy, vid.realwidth, vid.realheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	CHECKGLERROR

	// apply hardware gamma to the image
	BuildGammaTable8((lighthalf && hardwaregammasupported) ? 2.0f : 1.0f, 1, 1, 0, gamma);
	Image_GammaRemapRGB(buffer, buffer, vid.realwidth*vid.realheight, gamma, gamma, gamma);

	Image_WriteTGARGB_preflipped(filename, vid.realwidth, vid.realheight, buffer);

	Mem_Free(buffer);
	Con_Printf ("Wrote %s\n", filename);
}

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
static void R_Envmap_f (void)
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

	if (!r_render.integer)
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

	buffer = Mem_Alloc(tempmempool, size*size*3);
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

	Mem_Free(buffer);

	// cause refdef to be fixed
//	vid.recalc_refdef = 1;
}

//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
/*
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

//	if (cls.state != ca_connected)
//		return;
//	if (cls.signon != SIGNONS)
//		return;

// redraw with no console and the loading plaque
//	Con_ClearNotify ();
//	scr_centertime_off = 0;
//	scr_con_current = 0;

	scr_drawloading = true;
	SCR_UpdateScreen ();

//	scr_disabled_for_loading = true;
//	scr_disabled_time = realtime;
}
*/

/*
===============
SCR_EndLoadingPlaque

================
*/
/*
void SCR_EndLoadingPlaque (void)
{
//	scr_disabled_for_loading = false;
	scr_drawloading = false;
	Con_ClearNotify ();
}
*/

//=============================================================================

char	*scr_notifystring;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		x, y;

	start = scr_notifystring;

	y = vid.conheight*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.conwidth - l*8)/2;
		DrawQ_String (x, y, start, l, 8, 8, 1, 1, 1, 1, 0);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	}
	while (1);
}

//=============================================================================

void DrawCrosshair(int num);

char r_speeds_string[1024];
int speedstringcount, r_timereport_active;
double r_timereport_temp = 0, r_timereport_current = 0, r_timereport_start = 0;

void R_TimeReport(char *desc)
{
	char tempbuf[256];
	int length;
	int t;

	if (!r_timereport_active)
		return;

	r_timereport_temp = r_timereport_current;
	r_timereport_current = Sys_DoubleTime();
	t = (int) ((r_timereport_current - r_timereport_temp) * 1000000.0);

	sprintf(tempbuf, "%8i %s", t, desc);
	length = strlen(tempbuf);
	while (length < 20)
		tempbuf[length++] = ' ';
	tempbuf[length] = 0;
	if (speedstringcount + length > (vid.conwidth / 8))
	{
		strcat(r_speeds_string, "\n");
		speedstringcount = 0;
	}
	// skip the space at the beginning if it's the first on the line
	if (speedstringcount == 0)
	{
		strcat(r_speeds_string, tempbuf + 1);
		speedstringcount = length - 1;
	}
	else
	{
		strcat(r_speeds_string, tempbuf);
		speedstringcount += length;
	}
}

void R_TimeReport_Start(void)
{
	r_timereport_active = r_speeds.integer && cl.worldmodel && cls.state == ca_connected;
	r_speeds_string[0] = 0;
	if (r_timereport_active)
	{
		speedstringcount = 0;
		AngleVectors (r_refdef.viewangles, vpn, NULL, NULL);
		//sprintf(r_speeds_string, "org:'%c%6.2f %c%6.2f %c%6.2f' ang:'%c%3.0f %c%3.0f %c%3.0f' dir:'%c%2.3f %c%2.3f %c%2.3f'\n%6i walls %6i dlitwalls %7i modeltris %7i meshtris\nBSP: %6i faces %6i nodes %6i leafs\n%4i models %4i bmodels %4i sprites %5i particles %3i dlights\n",
		//	r_refdef.vieworg[0] < 0 ? '-' : ' ', fabs(r_refdef.vieworg[0]), r_refdef.vieworg[1] < 0 ? '-' : ' ', fabs(r_refdef.vieworg[1]), r_refdef.vieworg[2] < 0 ? '-' : ' ', fabs(r_refdef.vieworg[2]),
		//	r_refdef.viewangles[0] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[0]), r_refdef.viewangles[1] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[1]), r_refdef.viewangles[2] < 0 ? '-' : ' ', fabs(r_refdef.viewangles[2]),
		//	vpn[0] < 0 ? '-' : ' ', fabs(vpn[0]), vpn[1] < 0 ? '-' : ' ', fabs(vpn[1]), vpn[2] < 0 ? '-' : ' ', fabs(vpn[2]),
		sprintf(r_speeds_string, "org:'%+8.2f %+8.2f %+8.2f' ang:'%+4.0f %+4.0f %+4.0f' dir:'%+2.3f %+2.3f %+2.3f'\n%6i walls %6i dlitwalls %7i modeltris %7i meshtris\nBSP: %6i faces %6i nodes %6i leafs\n%4i models %4i bmodels %4i sprites %5i particles %3i dlights\n",
			r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2],
			r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2],
			vpn[0], vpn[1], vpn[2],
			c_brush_polys, c_light_polys, c_alias_polys, c_meshtris,
			c_faces, c_nodes, c_leafs,
			c_models, c_bmodels, c_sprites, c_particles, c_dlights);

		c_brush_polys = 0;
		c_alias_polys = 0;
		c_light_polys = 0;
		c_faces = 0;
		c_nodes = 0;
		c_leafs = 0;
		c_models = 0;
		c_bmodels = 0;
		c_sprites = 0;
		c_particles = 0;
	//	c_dlights = 0;

		r_timereport_start = Sys_DoubleTime();
	}
}

void R_TimeReport_End(void)
{
	r_timereport_current = r_timereport_start;
	R_TimeReport("total");

	if (r_timereport_active)
	{
		int i, j, lines, y;
		lines = 1;
		for (i = 0;r_speeds_string[i];i++)
			if (r_speeds_string[i] == '\n')
				lines++;
		y = vid.conheight - sb_lines - lines * 8/* - 8*/;
		i = j = 0;
		DrawQ_Fill(0, y, vid.conwidth, lines * 8, 0, 0, 0, 0.5, 0);
		while (r_speeds_string[i])
		{
			j = i;
			while (r_speeds_string[i] && r_speeds_string[i] != '\n')
				i++;
			if (i - j > 0)
				DrawQ_String(0, y, r_speeds_string + j, i - j, 8, 8, 1, 1, 1, 1, 0);
			if (r_speeds_string[i] == '\n')
				i++;
			y += 8;
		}
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

LordHavoc: due to my rewrite of R_WorldNode, it no longer takes 256k of stack space :)
==================
*/
void SCR_UpdateScreen (void)
{
	VID_UpdateGamma(false);

	if (scr_disabled_for_loading)
		return;

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

	//Mem_CheckSentinelsGlobal();
	//R_TimeReport("memtest");

	R_TimeReport("other");

	glFinish ();
	CHECKGLERROR

	GL_EndRendering ();

	R_TimeReport("finish");

	GL_BeginRendering (&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);

	if (gl_combine.integer && !gl_combine_extension)
		Cvar_SetValue("gl_combine", 0);

	lighthalf = gl_lightmode.integer;

	lightscalebit = 0;
	if (lighthalf)
		lightscalebit += 1;

	if (gl_combine.integer && r_multitexture.integer)
		lightscalebit += 2;

	lightscale = 1.0f / (float) (1 << lightscalebit);

	R_TimeReport("setup");

	// determine size of refresh window
	SCR_CalcRefdef();

	R_TimeReport("calcrefdef");

	if (r_render.integer)
	{
		glClearColor(0,0,0,0);
		CHECKGLERROR
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: clear the screen (around the view as well)
		CHECKGLERROR
		if (gl_dither.integer)
			glEnable(GL_DITHER);
		else
			glDisable(GL_DITHER);
		CHECKGLERROR
	}

	SCR_SetUpToDrawConsole();

	R_TimeReport("clear");

	if (scr_conlines < vid.conheight)
		R_RenderView();

	SCR_DrawRam();
	SCR_DrawNet();
	SCR_DrawTurtle();
	SCR_DrawPause();
	SCR_CheckDrawCenterString();
	Sbar_Draw();
	SHOWLMP_drawall();

	SCR_DrawConsole();
	M_Draw();

	ui_draw();

	R_TimeReport("2d");

	R_TimeReport_End();

	// draw 2D stuff
	R_DrawQueue();

	R_TimeReport_Start();
}
