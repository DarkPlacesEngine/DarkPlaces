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


int		glx, gly, glwidth, glheight;

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
cvar_t	showfps = {CVAR_SAVE, "showfps", "0"};
cvar_t	r_render = {0, "r_render", "1"};
cvar_t	r_brightness = {CVAR_SAVE, "r_brightness", "1"}; // LordHavoc: a method of operating system independent color correction
cvar_t	r_contrast = {CVAR_SAVE, "r_contrast", "1"}; // LordHavoc: a method of operating system independent color correction

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

int			clearconsole;
int			clearnotify;

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
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		// LordHavoc: speedup
		if (l > 0)
		{
			if (remaining < l)
				l = remaining;
			Draw_String(x, y, start, l);
			remaining -= l;
			if (remaining <= 0)
				return;
		}
		/*
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
				return;
		}
		*/
			
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
        float   a;
        float   x;

        if (fov_x < 1 || fov_x > 179)
                Sys_Error ("Bad fov: %f", fov_x);

        x = width/tan(fov_x/360*M_PI);

        a = atan (height/x);

        a = a*360/M_PI;

        return a;
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
	float		size;
	int		h;
	qboolean		full = false;


	vid.recalc_refdef = 0;

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
		full = true;
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

		if (scr_viewsize.value >= 100.0)
		{
			full = true;
			size = 1.0f;
		}
		else
			size = scr_viewsize.value * (1.0f / 100.0f);
	}

	// LordHavoc: always fullscreen rendering
	h = vid.height/* - sb_lines*/;

	r_refdef.vrect.width = vid.width * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = vid.height * size;
	//if (r_refdef.vrect.height > vid.height - sb_lines)
	//	r_refdef.vrect.height = vid.height - sb_lines;
	if (r_refdef.vrect.height > (int) vid.height)
			r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;
	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	r_refdef.fov_x = scr_fov.value;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
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
	vid.recalc_refdef = 1;
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
	vid.recalc_refdef = 1;
}

//============================================================================

void gl_screen_start(void)
{
	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");
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
	Cvar_RegisterVariable (&showfps);
	Cvar_RegisterVariable (&r_render);
	Cvar_RegisterVariable (&r_brightness);
	Cvar_RegisterVariable (&r_contrast);
#ifdef NORENDER
	r_render.value = 0;
#endif

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
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
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_Pic (32, 0, scr_ram);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (cl.frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (0, 0, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (64, 0, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}



/*
==============
SCR_DrawLoading
==============
*/
/*
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
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
	
	//if (scr_drawloading)
	//	return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
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
		Con_DrawConsole (scr_con_current, true);
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
	byte		*buffer;
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

	buffer = qmalloc(glwidth*glheight*3);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer); 
	Image_WriteTGARGB_preflipped(filename, glwidth, glheight, buffer);

	qfree(buffer);
	Con_Printf ("Wrote %s\n", filename);
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

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		// LordHavoc: speedup
//		for (j=0 ; j<l ; j++, x+=8)
//			Draw_Character (x, y, start[j]);	
		Draw_String (x, y, start, l);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

//=============================================================================

void DrawCrosshair(int num);
void GL_Set2D (void);

void GL_BrightenScreen(void)
{
	float f;

	if (r_brightness.value < 0.1f)
		Cvar_SetValue("r_brightness", 0.1f);
	if (r_brightness.value > 5.0f)
		Cvar_SetValue("r_brightness", 5.0f);

	if (r_contrast.value < 0.2f)
		Cvar_SetValue("r_contrast", 0.2f);
	if (r_contrast.value > 1.0f)
		Cvar_SetValue("r_contrast", 1.0f);

	if (!(lighthalf && !hardwaregammasupported) && r_brightness.value < 1.01f && r_contrast.value > 0.99f)
		return;

	if (!r_render.value)
		return;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	f = r_brightness.value;
	// only apply lighthalf using software color correction if hardware is not available (speed reasons)
	if (lighthalf && !hardwaregammasupported)
		f *= 2;
	if (f >= 1.01f)
	{
		glBlendFunc (GL_DST_COLOR, GL_ONE);
		glBegin (GL_TRIANGLES);
		while (f >= 1.01f)
		{
			if (f >= 2)
				glColor3f (1, 1, 1);
			else
				glColor3f (f-1, f-1, f-1);
			glVertex2f (-5000, -5000);
			glVertex2f (10000, -5000);
			glVertex2f (-5000, 10000);
			f *= 0.5;
		}
		glEnd ();
	}
	if (r_contrast.value <= 0.99f)
	{
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (lighthalf && hardwaregammasupported)
			glColor4f (0.5, 0.5, 0.5, 1 - r_contrast.value);
		else
			glColor4f (1, 1, 1, 1 - r_contrast.value);
		glBegin (GL_TRIANGLES);
		glVertex2f (-5000, -5000);
		glVertex2f (10000, -5000);
		glVertex2f (-5000, 10000);
		glEnd ();
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable (GL_CULL_FACE);
	glEnable (GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

LordHavoc: due to my rewrite of R_WorldNode, it no longer takes 256k of stack space :)
==================
*/
void GL_Finish(void);
void R_Clip_DisplayBuffer(void);
void SCR_UpdateScreen (void)
{
	double	time1 = 0, time2;

	if (r_speeds.value)
		time1 = Sys_DoubleTime ();

	VID_UpdateGamma(false);

	if (scr_disabled_for_loading)
	{
		/*
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
		*/
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	//
	// determine size of refresh window
	//
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef();

	if (r_render.value)
	{
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // LordHavoc: clear the screen (around the view as well)
	}

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	R_Clip_DisplayBuffer();

	SCR_DrawRam();
	SCR_DrawNet();
	SCR_DrawTurtle();
	SCR_DrawPause();
	SCR_CheckDrawCenterString();
	Sbar_Draw();
	SHOWLMP_drawall();

	if (crosshair.value)
		DrawCrosshair(crosshair.value - 1);

	if (cl.intermission == 1)
		Sbar_IntermissionOverlay();
	else if (cl.intermission == 2)
		Sbar_FinaleOverlay();

	SCR_DrawConsole();
	M_Draw();

	ui_draw();

//	if (scr_drawloading)
//		SCR_DrawLoading();

	if (showfps.value)
	{
		static double currtime;
		double newtime;
		char temp[32];
		int calc;
		newtime = Sys_DoubleTime();
		calc = (int) ((1.0 / (newtime - currtime)) + 0.5);
		sprintf(temp, "%4i fps", calc);
		currtime = newtime;
		Draw_String(vid.width - (8*8), vid.height - sb_lines - 8, temp, 9999);
	}

	// LordHavoc: only print info if renderer is being used
	if (r_speeds2.value && !con_forcedup)
	{
		int i, j, lines, y;
		lines = 1;
		for (i = 0;r_speeds2_string[i];i++)
			if (r_speeds2_string[i] == '\n')
				lines++;
		y = vid.height - sb_lines - lines * 8 - 8;
		i = j = 0;
		while (r_speeds2_string[i])
		{
			j = i;
			while (r_speeds2_string[i] && r_speeds2_string[i] != '\n')
				i++;
			if (i - j > 0)
				Draw_String(0, y, r_speeds2_string + j, i - j);
			if (r_speeds2_string[i] == '\n')
				i++;
			y += 8;
		}
	}

	V_UpdateBlends();

	GL_BrightenScreen();

	GL_Finish();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly %4i transpoly %4i lightpoly %4i BSPnodes %4i BSPleafs %4i BSPfaces %4i models %4i bmodels %4i sprites %4i particles %3i dlights\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys, currenttranspoly, c_light_polys, c_nodes, c_leafs, c_faces, c_models, c_bmodels, c_sprites, c_particles, c_dlights);
	}
	GL_EndRendering ();
}

// for profiling, this is separated
void GL_Finish(void)
{
	if (!r_render.value)
		return;
	glFinish ();
}

