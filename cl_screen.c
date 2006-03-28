
#include "quakedef.h"
#include "cl_video.h"
#include "image.h"
#include "jpeg.h"
#include "cl_collision.h"
#include "csprogs.h"

cvar_t scr_viewsize = {CVAR_SAVE, "viewsize","100", "how large the view should be, 110 disables inventory bar, 120 disables status bar"};
cvar_t scr_fov = {CVAR_SAVE, "fov","90", "field of vision, 1-170 degrees, default 90, some players use 110-130"};	// 1 - 170
cvar_t scr_conspeed = {CVAR_SAVE, "scr_conspeed","900", "speed of console open/close"}; // LordHavoc: quake used 300
cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1", "opacity of console background"};
cvar_t scr_conbrightness = {CVAR_SAVE, "scr_conbrightness", "0.2", "brightness of console background (0 = black, 1 = image)"};
cvar_t scr_conforcewhiledisconnected = {0, "scr_conforcewhiledisconnected", "1", "forces fullscreen console while disconnected"};
cvar_t scr_menuforcewhiledisconnected = {0, "scr_menuforcewhiledisconnected", "1", "forces menu while disconnected"};
cvar_t scr_centertime = {0, "scr_centertime","2", "how long centerprint messages show"};
cvar_t scr_showram = {CVAR_SAVE, "showram","1", "show ram icon if low on surface cache memory (not used)"};
cvar_t scr_showturtle = {CVAR_SAVE, "showturtle","0", "show turtle icon when framerate is too low (not used)"};
cvar_t scr_showpause = {CVAR_SAVE, "showpause","1", "show pause icon when game is paused"};
cvar_t scr_showbrand = {0, "showbrand","0", "shows gfx/brand.tga in a corner of the screen (different values select different positions, including centered)"};
cvar_t scr_printspeed = {0, "scr_printspeed","8", "speed of intermission printing (episode end texts)"};
cvar_t vid_conwidth = {CVAR_SAVE, "vid_conwidth", "640", "virtual width of 2D graphics system"};
cvar_t vid_conheight = {CVAR_SAVE, "vid_conheight", "480", "virtual height of 2D graphics system"};
cvar_t vid_pixelheight = {CVAR_SAVE, "vid_pixelheight", "1", "adjusts vertical field of vision to account for non-square pixels (1280x1024 on a CRT monitor for example)"};
cvar_t scr_screenshot_jpeg = {CVAR_SAVE, "scr_screenshot_jpeg","1", "save jpeg instead of targa"};
cvar_t scr_screenshot_jpeg_quality = {CVAR_SAVE, "scr_screenshot_jpeg_quality","0.9", "image quality of saved jpeg"};
cvar_t scr_screenshot_gamma = {CVAR_SAVE, "scr_screenshot_gamma","2.2", "gamma correction on saved screenshots and videos, 1.0 saves unmodified images"};
// scr_screenshot_name is defined in fs.c
cvar_t cl_capturevideo = {0, "cl_capturevideo", "0", "enables saving of video to a file or files (default is .tga files, if scr_screenshot_jpeg is on it saves .jpg files (VERY SLOW), if any rawrgb or rawyv12 are on it saves those formats instead, note that scr_screenshot_gamma affects the brightness of the output)"};
cvar_t cl_capturevideo_sound = {0, "cl_capturevideo_sound", "0", "enables saving of sound to a .wav file (warning: this requires exact sync, if your hard drive can't keep up it will abort, if your graphics can't keep up it will save duplicate frames to maintain sound sync)"};
cvar_t cl_capturevideo_fps = {0, "cl_capturevideo_fps", "30", "how many frames per second to save (29.97 for NTSC, 30 for typical PC video, 15 can be useful)"};
cvar_t cl_capturevideo_rawrgb = {0, "cl_capturevideo_rawrgb", "0", "saves a single .rgb video file containing raw RGB images (you'll need special processing tools to encode this to something more useful)"};
cvar_t cl_capturevideo_rawyv12 = {0, "cl_capturevideo_rawyv12", "0", "saves a single .yv12 video file containing raw YV12 (luma plane, then half resolution chroma planes, first chroma blue then chroma red, this is the format used internally by many encoders, some tools can read it directly)"};
cvar_t r_letterbox = {0, "r_letterbox", "0", "reduces vertical height of view to simulate a letterboxed movie effect (can be used by mods for cutscenes)"};
cvar_t r_stereo_separation = {0, "r_stereo_separation", "4", "separation of eyes in the world (try negative values too)"};
cvar_t r_stereo_sidebyside = {0, "r_stereo_sidebyside", "0", "side by side views (for those who can't afford glasses but can afford eye strain)"};
cvar_t r_stereo_redblue = {0, "r_stereo_redblue", "0", "red/blue anaglyph stereo glasses (note: most of these glasses are actually red/cyan, try that one too)"};
cvar_t r_stereo_redcyan = {0, "r_stereo_redcyan", "0", "red/cyan anaglyph stereo glasses, the kind given away at drive-in movies like Creature From The Black Lagoon In 3D"};
cvar_t r_stereo_redgreen = {0, "r_stereo_redgreen", "0", "red/green anaglyph stereo glasses (for those who don't mind yellow)"};
cvar_t scr_zoomwindow = {CVAR_SAVE, "scr_zoomwindow", "0", "displays a zoomed in overlay window"};
cvar_t scr_zoomwindow_viewsizex = {CVAR_SAVE, "scr_zoomwindow_viewsizex", "20", "horizontal viewsize of zoom window"};
cvar_t scr_zoomwindow_viewsizey = {CVAR_SAVE, "scr_zoomwindow_viewsizey", "20", "vertical viewsize of zoom window"};
cvar_t scr_zoomwindow_fov = {CVAR_SAVE, "scr_zoomwindow_fov", "20", "fov of zoom window"};


int jpeg_supported = false;

qboolean	scr_initialized;		// ready to draw

float		scr_con_current;

extern int	con_vislines;

void DrawCrosshair(int num);
static void SCR_ScreenShot_f (void);
static void R_Envmap_f (void);

// backend
void R_ClearScreen(void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[MAX_INPUTLINE];
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
void SCR_CenterPrint(char *str)
{
	strlcpy (scr_centerstring, str, sizeof (scr_centerstring));
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
	int		color;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (remaining < 1)
		return;

	if (scr_center_lines <= 4)
		y = vid_conheight.integer*0.35;
	else
		y = 48;

	color = -1;
	do
	{
	// scan the width of the line
		for (l=0 ; l<vid_conwidth.integer/8 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid_conwidth.integer - l*8)/2;
		if (l > 0)
		{
			if (remaining < l)
				l = remaining;
			DrawQ_ColoredString(x, y, start, l, 8, 8, 1, 1, 1, 1, 0, &color);
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

	// don't draw if this is a normal stats-screen intermission,
	// only if it is not an intermission, or a finale intermission
	if (cl.intermission == 1)
		return;
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
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

	DrawQ_Pic (0, 0, Draw_CachePic("gfx/turtle", true), 0, 0, 1, 1, 1, 1, 0);
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

	DrawQ_Pic (64, 0, Draw_CachePic("gfx/net", true), 0, 0, 1, 1, 1, 1, 0);
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

	pic = Draw_CachePic ("gfx/pause", true);
	DrawQ_Pic ((vid_conwidth.integer - pic->width)/2, (vid_conheight.integer - pic->height)/2, pic, 0, 0, 1, 1, 1, 1, 0);
}

/*
==============
SCR_DrawBrand
==============
*/
void SCR_DrawBrand (void)
{
	cachepic_t	*pic;
	float		x, y;

	if (!scr_showbrand.value)
		return;

	pic = Draw_CachePic ("gfx/brand", true);

	switch ((int)scr_showbrand.value)
	{
	case 1:	// bottom left
		x = 0;
		y = vid_conheight.integer - pic->height;
		break;
	case 2:	// bottom centre
		x = (vid_conwidth.integer - pic->width) / 2;
		y = vid_conheight.integer - pic->height;
		break;
	case 3:	// bottom right
		x = vid_conwidth.integer - pic->width;
		y = vid_conheight.integer - pic->height;
		break;
	case 4:	// centre right
		x = vid_conwidth.integer - pic->width;
		y = (vid_conheight.integer - pic->height) / 2;
		break;
	case 5:	// top right
		x = vid_conwidth.integer - pic->width;
		y = 0;
		break;
	case 6:	// top centre
		x = (vid_conwidth.integer - pic->width) / 2;
		y = 0;
		break;
	case 7:	// top left
		x = 0;
		y = 0;
		break;
	case 8:	// centre left
		x = 0;
		y = (vid_conheight.integer - pic->height) / 2;
		break;
	default:
		return;
	}

	DrawQ_Pic (x, y, pic, 0, 0, 1, 1, 1, 1, 0);
}

/*
==============
SCR_DrawDownload
==============
*/
static void SCR_DrawDownload(void)
{
	int len;
	float x, y;
	float size = 8;
	char temp[256];
	if (!cls.qw_downloadname[0])
		return;
	dpsnprintf(temp, sizeof(temp), "Downloading %s ...  %3i%%\n", cls.qw_downloadname, cls.qw_downloadpercent);
	len = (int)strlen(temp);
	x = (vid_conwidth.integer - len*size) / 2;
	y = vid_conheight.integer - size;
	DrawQ_Pic(0, y, NULL, vid_conwidth.integer, size, 0, 0, 0, 0.5, 0);
	DrawQ_String(x, y, temp, len, size, size, 1, 1, 1, 1, 0);
}

//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	// lines of console to display
	float conlines;
	static int framecounter = 0;

	Con_CheckResize ();

	if (scr_menuforcewhiledisconnected.integer && key_dest == key_game && cls.state == ca_disconnected)
	{
		if (framecounter >= 2)
			MR_ToggleMenu_f();
		else
			framecounter++;
	}
	else
		framecounter = 0;

	if (scr_conforcewhiledisconnected.integer && key_dest == key_game && cls.signon != SIGNONS)
		key_consoleactive |= KEY_CONSOLEACTIVE_FORCED;
	else
		key_consoleactive &= ~KEY_CONSOLEACTIVE_FORCED;

// decide on the height of the console
	if (key_consoleactive & KEY_CONSOLEACTIVE_USER)
		conlines = vid_conheight.integer/2;	// half screen
	else
		conlines = 0;				// none visible

	if (scr_conspeed.value)
	{
		if (scr_con_current > conlines)
		{
			scr_con_current -= scr_conspeed.value*host_realframetime;
			if (scr_con_current < conlines)
				scr_con_current = conlines;

		}
		else if (scr_con_current < conlines)
		{
			scr_con_current += scr_conspeed.value*host_realframetime;
			if (scr_con_current > conlines)
				scr_con_current = conlines;
		}
	}
	else
		scr_con_current = conlines;
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (key_consoleactive & KEY_CONSOLEACTIVE_FORCED)
	{
		// full screen
		Con_DrawConsole (vid_conheight.integer);
	}
	else if (scr_con_current)
		Con_DrawConsole (scr_con_current);
	else
	{
		con_vislines = 0;
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	// save console log up to this point to log_file if it was set by configs
	Log_Start();

	Host_StartVideo();
	S_StopAllSounds();
	SCR_UpdateLoadingScreen();
}

//=============================================================================

char r_speeds_string[1024];
int speedstringcount, r_timereport_active;
double r_timereport_temp = 0, r_timereport_current = 0, r_timereport_start = 0;

void R_TimeReport(char *desc)
{
	char tempbuf[256];
	int length;
	int t;

	if (r_speeds.integer < 2 || !r_timereport_active)
		return;

	qglFinish();
	r_timereport_temp = r_timereport_current;
	r_timereport_current = Sys_DoubleTime();
	t = (int) ((r_timereport_current - r_timereport_temp) * 1000000.0 + 0.5);

	dpsnprintf(tempbuf, sizeof(tempbuf), "%8i %-11s", t, desc);
	length = (int)strlen(tempbuf);
	if (speedstringcount + length > (vid_conwidth.integer / 8))
	{
		strlcat(r_speeds_string, "\n", sizeof(r_speeds_string));
		speedstringcount = 0;
	}
	strlcat(r_speeds_string, tempbuf, sizeof(r_speeds_string));
	speedstringcount += length;
}

void R_TimeReport_Frame(void)
{
	int i, j, lines, y;

	if (r_speeds_string[0])
	{
		if (r_timereport_active)
		{
			r_timereport_current = r_timereport_start;
			R_TimeReport("total");
		}

		if (r_speeds_string[strlen(r_speeds_string)-1] == '\n')
			r_speeds_string[strlen(r_speeds_string)-1] = 0;
		lines = 1;
		for (i = 0;r_speeds_string[i];i++)
			if (r_speeds_string[i] == '\n')
				lines++;
		y = vid_conheight.integer - sb_lines - lines * 8;
		i = j = 0;
		DrawQ_Pic(0, y, NULL, vid_conwidth.integer, lines * 8, 0, 0, 0, 0.5, 0);
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
		r_speeds_string[0] = 0;
		r_timereport_active = false;
	}
	if (r_speeds.integer && cls.signon == SIGNONS && cls.state == ca_connected)
	{
		speedstringcount = 0;
		r_speeds_string[0] = 0;
		r_timereport_active = false;
		sprintf(r_speeds_string + strlen(r_speeds_string), "org:'%+8.2f %+8.2f %+8.2f' dir:'%+2.3f %+2.3f %+2.3f'\n", r_vieworigin[0], r_vieworigin[1], r_vieworigin[2], r_viewforward[0], r_viewforward[1], r_viewforward[2]);
		sprintf(r_speeds_string + strlen(r_speeds_string), "%5i entities%6i surfaces%6i triangles%5i leafs%5i portals%6i particles\n", renderstats.entities, renderstats.entities_surfaces, renderstats.entities_triangles, renderstats.world_leafs, renderstats.world_portals, renderstats.particles);
		sprintf(r_speeds_string + strlen(r_speeds_string), "%4i lights%4i clears%4i scissored%7i light%7i shadow%7i dynamic\n", renderstats.lights, renderstats.lights_clears, renderstats.lights_scissored, renderstats.lights_lighttriangles, renderstats.lights_shadowtriangles, renderstats.lights_dynamicshadowtriangles);
		if (renderstats.bloom)
			sprintf(r_speeds_string + strlen(r_speeds_string), "rendered%6i meshes%8i triangles bloompixels%8i copied%8i drawn\n", renderstats.meshes, renderstats.meshes_elements / 3, renderstats.bloom_copypixels, renderstats.bloom_drawpixels);
		else
			sprintf(r_speeds_string + strlen(r_speeds_string), "rendered%6i meshes%8i triangles\n", renderstats.meshes, renderstats.meshes_elements / 3);

		memset(&renderstats, 0, sizeof(renderstats));

		if (r_speeds.integer >= 2)
		{
			r_timereport_active = true;
			r_timereport_start = r_timereport_current = Sys_DoubleTime();
		}
	}
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

void CL_Screen_Init(void)
{
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_conalpha);
	Cvar_RegisterVariable (&scr_conbrightness);
	Cvar_RegisterVariable (&scr_conforcewhiledisconnected);
	Cvar_RegisterVariable (&scr_menuforcewhiledisconnected);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_showbrand);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&vid_conwidth);
	Cvar_RegisterVariable (&vid_conheight);
	Cvar_RegisterVariable (&vid_pixelheight);
	Cvar_RegisterVariable (&scr_screenshot_jpeg);
	Cvar_RegisterVariable (&scr_screenshot_jpeg_quality);
	Cvar_RegisterVariable (&scr_screenshot_gamma);
	Cvar_RegisterVariable (&cl_capturevideo);
	Cvar_RegisterVariable (&cl_capturevideo_sound);
	Cvar_RegisterVariable (&cl_capturevideo_fps);
	Cvar_RegisterVariable (&cl_capturevideo_rawrgb);
	Cvar_RegisterVariable (&cl_capturevideo_rawyv12);
	Cvar_RegisterVariable (&r_letterbox);
	Cvar_RegisterVariable(&r_stereo_separation);
	Cvar_RegisterVariable(&r_stereo_sidebyside);
	Cvar_RegisterVariable(&r_stereo_redblue);
	Cvar_RegisterVariable(&r_stereo_redcyan);
	Cvar_RegisterVariable(&r_stereo_redgreen);
	Cvar_RegisterVariable(&scr_zoomwindow);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizex);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizey);
	Cvar_RegisterVariable(&scr_zoomwindow_fov);

	Cmd_AddCommand ("sizeup",SCR_SizeUp_f, "increase view size (increases viewsize cvar)");
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f, "decrease view size (decreases viewsize cvar)");
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f, "takes a screenshot of the next rendered frame");
	Cmd_AddCommand ("envmap", R_Envmap_f, "render a cubemap (skybox) of the current scene");

	scr_initialized = true;
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	static int shotnumber;
	static char oldname[MAX_QPATH];
	char base[MAX_QPATH];
	char filename[MAX_QPATH];
	unsigned char *buffer1;
	unsigned char *buffer2;
	unsigned char *buffer3;
	qboolean jpeg = (scr_screenshot_jpeg.integer != 0);

	sprintf (base, "screenshots/%s", scr_screenshot_name.string);

	if (strcmp (oldname, scr_screenshot_name.string))
	{
		sprintf(oldname, "%s", scr_screenshot_name.string);
		shotnumber = 0;
	}

	// find a file name to save it to
	for (;shotnumber < 1000000;shotnumber++)
		if (!FS_SysFileExists(va("%s/%s%06d.tga", fs_gamedir, base, shotnumber)) && !FS_SysFileExists(va("%s/%s%06d.jpg", fs_gamedir, base, shotnumber)))
			break;
	if (shotnumber >= 1000000)
	{
		Con_Print("SCR_ScreenShot_f: Couldn't create the image file\n");
		return;
 	}

	sprintf(filename, "%s%06d.%s", base, shotnumber, jpeg ? "jpg" : "tga");

	buffer1 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 3);
	buffer2 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 3);
	buffer3 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 3 + 18);

	if (SCR_ScreenShot (filename, buffer1, buffer2, buffer3, 0, 0, vid.width, vid.height, false, false, false, jpeg, true))
		Con_Printf("Wrote %s\n", filename);
	else
		Con_Printf("unable to write %s\n", filename);

	Mem_Free (buffer1);
	Mem_Free (buffer2);
	Mem_Free (buffer3);

	shotnumber++;
}

void SCR_CaptureVideo_BeginVideo(void)
{
	double gamma, g;
	unsigned int i;
	unsigned char out[44];
	if (cls.capturevideo_active)
		return;
	// soundrate is figured out on the first SoundFrame
	cls.capturevideo_active = true;
	cls.capturevideo_starttime = Sys_DoubleTime();
	cls.capturevideo_framerate = bound(1, cl_capturevideo_fps.value, 1000);
	cls.capturevideo_soundrate = 0;
	cls.capturevideo_frame = 0;
	cls.capturevideo_buffer = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * (3+3+3) + 18);
	gamma = 1.0/scr_screenshot_gamma.value;

	/*
	for (i = 0;i < 256;i++)
	{
		unsigned char j = (unsigned char)bound(0, 255*pow(i/255.0, gamma), 255);
		cls.capturevideo_rgbgammatable[0][i] = j;
		cls.capturevideo_rgbgammatable[1][i] = j;
		cls.capturevideo_rgbgammatable[2][i] = j;
	}
	*/
/*
R = Y + 1.4075 * (Cr - 128);
G = Y + -0.3455 * (Cb - 128) + -0.7169 * (Cr - 128);
B = Y + 1.7790 * (Cb - 128);
Y = R *  .299 + G *  .587 + B *  .114;
Cb = R * -.169 + G * -.332 + B *  .500 + 128.;
Cr = R *  .500 + G * -.419 + B * -.0813 + 128.;
*/
	for (i = 0;i < 256;i++)
	{
		g = 255*pow(i/255.0, gamma);
		// Y weights from RGB
		cls.capturevideo_rgbtoyuvscaletable[0][0][i] = (short)(g *  0.299);
		cls.capturevideo_rgbtoyuvscaletable[0][1][i] = (short)(g *  0.587);
		cls.capturevideo_rgbtoyuvscaletable[0][2][i] = (short)(g *  0.114);
		// Cb weights from RGB
		cls.capturevideo_rgbtoyuvscaletable[1][0][i] = (short)(g * -0.169);
		cls.capturevideo_rgbtoyuvscaletable[1][1][i] = (short)(g * -0.332);
		cls.capturevideo_rgbtoyuvscaletable[1][2][i] = (short)(g *  0.500);
		// Cr weights from RGB
		cls.capturevideo_rgbtoyuvscaletable[2][0][i] = (short)(g *  0.500);
		cls.capturevideo_rgbtoyuvscaletable[2][1][i] = (short)(g * -0.419);
		cls.capturevideo_rgbtoyuvscaletable[2][2][i] = (short)(g * -0.0813);
		// range reduction of YCbCr to valid signal range
		cls.capturevideo_yuvnormalizetable[0][i] = 16 + i * (236-16) / 256;
		cls.capturevideo_yuvnormalizetable[1][i] = 16 + i * (240-16) / 256;
		cls.capturevideo_yuvnormalizetable[2][i] = 16 + i * (240-16) / 256;
	}

	if (cl_capturevideo_rawrgb.integer)
	{
		cls.capturevideo_format = CAPTUREVIDEOFORMAT_RAWRGB;
		cls.capturevideo_videofile = FS_Open ("video/dpvideo.rgb", "wb", false, true);
	}
	else if (cl_capturevideo_rawyv12.integer)
	{
		cls.capturevideo_format = CAPTUREVIDEOFORMAT_RAWYV12;
		cls.capturevideo_videofile = FS_Open ("video/dpvideo.yv12", "wb", false, true);
	}
	else if (scr_screenshot_jpeg.integer)
	{
		cls.capturevideo_format = CAPTUREVIDEOFORMAT_JPEG;
		cls.capturevideo_videofile = NULL;
	}
	else
	{
		cls.capturevideo_format = CAPTUREVIDEOFORMAT_TARGA;
		cls.capturevideo_videofile = NULL;
	}

	if (cl_capturevideo_sound.integer)
	{
		cls.capturevideo_soundfile = FS_Open ("video/dpvideo.wav", "wb", false, true);
		// wave header will be filled out when video ends
		memset(out, 0, 44);
		FS_Write (cls.capturevideo_soundfile, out, 44);
	}
	else
		cls.capturevideo_soundfile = NULL;
}

void SCR_CaptureVideo_EndVideo(void)
{
	int i, n;
	unsigned char out[44];
	if (!cls.capturevideo_active)
		return;
	cls.capturevideo_active = false;

	if (cls.capturevideo_videofile)
	{
		FS_Close(cls.capturevideo_videofile);
		cls.capturevideo_videofile = NULL;
	}

	// finish the wave file
	if (cls.capturevideo_soundfile)
	{
		i = (int)FS_Tell (cls.capturevideo_soundfile);
		//"RIFF", (int) unknown (chunk size), "WAVE",
		//"fmt ", (int) 16 (chunk size), (short) format 1 (uncompressed PCM), (short) 2 channels, (int) unknown rate, (int) unknown bytes per second, (short) 4 bytes per sample (channels * bytes per channel), (short) 16 bits per channel
		//"data", (int) unknown (chunk size)
		memcpy (out, "RIFF****WAVEfmt \x10\x00\x00\x00\x01\x00\x02\x00********\x04\x00\x10\0data****", 44);
		// the length of the whole RIFF chunk
		n = i - 8;
		out[4] = (n) & 0xFF;
		out[5] = (n >> 8) & 0xFF;
		out[6] = (n >> 16) & 0xFF;
		out[7] = (n >> 24) & 0xFF;
		// rate
		n = cls.capturevideo_soundrate;
		out[24] = (n) & 0xFF;
		out[25] = (n >> 8) & 0xFF;
		out[26] = (n >> 16) & 0xFF;
		out[27] = (n >> 24) & 0xFF;
		// bytes per second (rate * channels * bytes per channel)
		n = cls.capturevideo_soundrate * 2 * 2;
		out[28] = (n) & 0xFF;
		out[29] = (n >> 8) & 0xFF;
		out[30] = (n >> 16) & 0xFF;
		out[31] = (n >> 24) & 0xFF;
		// the length of the data chunk
		n = i - 44;
		out[40] = (n) & 0xFF;
		out[41] = (n >> 8) & 0xFF;
		out[42] = (n >> 16) & 0xFF;
		out[43] = (n >> 24) & 0xFF;
		FS_Seek (cls.capturevideo_soundfile, 0, SEEK_SET);
		FS_Write (cls.capturevideo_soundfile, out, 44);
		FS_Close (cls.capturevideo_soundfile);
		cls.capturevideo_soundfile = NULL;
	}

	if (cls.capturevideo_buffer)
	{
		Mem_Free (cls.capturevideo_buffer);
		cls.capturevideo_buffer = NULL;
	}

	cls.capturevideo_starttime = 0;
	cls.capturevideo_framerate = 0;
	cls.capturevideo_frame = 0;
}

qboolean SCR_CaptureVideo_VideoFrame(int newframenum)
{
	int x = 0, y = 0, width = vid.width, height = vid.height;
	unsigned char *b, *out;
	char filename[32];
	int outoffset = (width/2)*(height/2);
	//return SCR_ScreenShot(filename, cls.capturevideo_buffer, cls.capturevideo_buffer + vid.width * vid.height * 3, cls.capturevideo_buffer + vid.width * vid.height * 6, 0, 0, vid.width, vid.height, false, false, false, jpeg, true);
	// speed is critical here, so do saving as directly as possible
	switch (cls.capturevideo_format)
	{
	case CAPTUREVIDEOFORMAT_RAWYV12:
		// FIXME: width/height must be multiple of 2, enforce this?
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cls.capturevideo_buffer);
		CHECKGLERROR
		// process one line at a time, and CbCr every other line at 2 pixel intervals
		for (y = 0;y < height;y++)
		{
			// 1x1 Y
			for (b = cls.capturevideo_buffer + (height-1-y)*width*3, out = cls.capturevideo_buffer + width*height*3 + y*width, x = 0;x < width;x++, b += 3, out++)
				*out = cls.capturevideo_yuvnormalizetable[0][cls.capturevideo_rgbtoyuvscaletable[0][0][b[0]] + cls.capturevideo_rgbtoyuvscaletable[0][1][b[1]] + cls.capturevideo_rgbtoyuvscaletable[0][2][b[2]]];
			if ((y & 1) == 0)
			{
				// 2x2 Cb and Cr planes
#if 1
				// low quality, no averaging
				for (b = cls.capturevideo_buffer + (height-2-y)*width*3, out = cls.capturevideo_buffer + width*height*3 + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
				{
					// Cr
					out[0        ] = cls.capturevideo_yuvnormalizetable[2][cls.capturevideo_rgbtoyuvscaletable[2][0][b[0]] + cls.capturevideo_rgbtoyuvscaletable[2][1][b[1]] + cls.capturevideo_rgbtoyuvscaletable[2][2][b[2]] + 128];
					// Cb
					out[outoffset] = cls.capturevideo_yuvnormalizetable[1][cls.capturevideo_rgbtoyuvscaletable[1][0][b[0]] + cls.capturevideo_rgbtoyuvscaletable[1][1][b[1]] + cls.capturevideo_rgbtoyuvscaletable[1][2][b[2]] + 128];
				}
#else
				// high quality, averaging
				int inpitch = width*3;
				for (b = cls.capturevideo_buffer + (height-2-y)*width*3, out = cls.capturevideo_buffer + width*height*3 + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
				{
					int blockr, blockg, blockb;
					blockr = (b[0] + b[3] + b[inpitch+0] + b[inpitch+3]) >> 2;
					blockg = (b[1] + b[4] + b[inpitch+1] + b[inpitch+4]) >> 2;
					blockb = (b[2] + b[5] + b[inpitch+2] + b[inpitch+5]) >> 2;
					// Cr
					out[0        ] = cls.capturevideo_yuvnormalizetable[2][cls.capturevideo_rgbtoyuvscaletable[2][0][blockr] + cls.capturevideo_rgbtoyuvscaletable[2][1][blockg] + cls.capturevideo_rgbtoyuvscaletable[2][2][blockb] + 128];
					// Cb
					out[outoffset] = cls.capturevideo_yuvnormalizetable[1][cls.capturevideo_rgbtoyuvscaletable[1][0][blockr] + cls.capturevideo_rgbtoyuvscaletable[1][1][blockg] + cls.capturevideo_rgbtoyuvscaletable[1][2][blockb] + 128];
				}
#endif
			}
		}
		for (;cls.capturevideo_frame < newframenum;cls.capturevideo_frame++)
			if (!FS_Write (cls.capturevideo_videofile, cls.capturevideo_buffer + width*height*3, width*height+(width/2)*(height/2)*2))
				return false;
		return true;
	case CAPTUREVIDEOFORMAT_RAWRGB:
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cls.capturevideo_buffer);
		CHECKGLERROR
		for (;cls.capturevideo_frame < newframenum;cls.capturevideo_frame++)
			if (!FS_Write (cls.capturevideo_videofile, cls.capturevideo_buffer, width*height*3))
				return false;
		return true;
	case CAPTUREVIDEOFORMAT_JPEG:
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cls.capturevideo_buffer);
		CHECKGLERROR
		for (;cls.capturevideo_frame < newframenum;cls.capturevideo_frame++)
		{
			sprintf(filename, "video/dp%06d.jpg", cls.capturevideo_frame);
			if (!JPEG_SaveImage_preflipped (filename, width, height, cls.capturevideo_buffer))
				return false;
		}
		return true;
	case CAPTUREVIDEOFORMAT_TARGA:
		//return Image_WriteTGARGB_preflipped (filename, width, height, cls.capturevideo_buffer, cls.capturevideo_buffer + vid.width * vid.height * 3, );
		memset (cls.capturevideo_buffer, 0, 18);
		cls.capturevideo_buffer[2] = 2;		// uncompressed type
		cls.capturevideo_buffer[12] = (width >> 0) & 0xFF;
		cls.capturevideo_buffer[13] = (width >> 8) & 0xFF;
		cls.capturevideo_buffer[14] = (height >> 0) & 0xFF;
		cls.capturevideo_buffer[15] = (height >> 8) & 0xFF;
		cls.capturevideo_buffer[16] = 24;	// pixel size
		qglReadPixels (x, y, width, height, GL_BGR, GL_UNSIGNED_BYTE, cls.capturevideo_buffer + 18);
		CHECKGLERROR
		for (;cls.capturevideo_frame < newframenum;cls.capturevideo_frame++)
		{
			sprintf(filename, "video/dp%06d.tga", cls.capturevideo_frame);
			if (!FS_WriteFile (filename, cls.capturevideo_buffer, width*height*3 + 18))
				return false;
		}
		return true;
	default:
		return false;
	}
}

void SCR_CaptureVideo_SoundFrame(unsigned char *bufstereo16le, size_t length, int rate)
{
	if (!cls.capturevideo_soundfile)
		return;
	cls.capturevideo_soundrate = rate;
	if (FS_Write (cls.capturevideo_soundfile, bufstereo16le, 4 * length) < (fs_offset_t)(4 * length))
	{
		Cvar_SetValueQuick(&cl_capturevideo, 0);
		Con_Printf("video sound saving failed on frame %i, out of disk space? stopping video capture.\n", cls.capturevideo_frame);
		SCR_CaptureVideo_EndVideo();
	}
}

void SCR_CaptureVideo(void)
{
	int newframenum;
	if (cl_capturevideo.integer && r_render.integer)
	{
		if (!cls.capturevideo_active)
			SCR_CaptureVideo_BeginVideo();
		if (cls.capturevideo_framerate != cl_capturevideo_fps.value)
		{
			Con_Printf("You can not change the video framerate while recording a video.\n");
			Cvar_SetValueQuick(&cl_capturevideo_fps, cls.capturevideo_framerate);
		}
		if (cls.capturevideo_soundfile)
		{
			// preserve sound sync by duplicating frames when running slow
			newframenum = (Sys_DoubleTime() - cls.capturevideo_starttime) * cls.capturevideo_framerate;
		}
		else
			newframenum = cls.capturevideo_frame + 1;
		// if falling behind more than one second, stop
		if (newframenum - cls.capturevideo_frame > (int)ceil(cls.capturevideo_framerate))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, your machine is too slow for this capture speed.\n", cls.capturevideo_frame);
			SCR_CaptureVideo_EndVideo();
			return;
		}
		// write frames
		if (!SCR_CaptureVideo_VideoFrame(newframenum))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, out of disk space? stopping video capture.\n", cls.capturevideo_frame);
			SCR_CaptureVideo_EndVideo();
		}
	}
	else if (cls.capturevideo_active)
		SCR_CaptureVideo_EndVideo();
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
struct envmapinfo_s
{
	float angles[3];
	char *name;
	qboolean flipx, flipy, flipdiagonaly;
}
envmapinfo[12] =
{
	{{  0,   0, 0}, "rt", false, false, false},
	{{  0, 270, 0}, "ft", false, false, false},
	{{  0, 180, 0}, "lf", false, false, false},
	{{  0,  90, 0}, "bk", false, false, false},
	{{-90, 180, 0}, "up",  true,  true, false},
	{{ 90, 180, 0}, "dn",  true,  true, false},

	{{  0,   0, 0}, "px",  true,  true,  true},
	{{  0,  90, 0}, "py", false,  true, false},
	{{  0, 180, 0}, "nx", false, false,  true},
	{{  0, 270, 0}, "ny",  true, false, false},
	{{-90, 180, 0}, "pz", false, false,  true},
	{{ 90, 180, 0}, "nz", false, false,  true}
};

static void R_Envmap_f (void)
{
	int j, size;
	char filename[MAX_QPATH], basename[MAX_QPATH];
	unsigned char *buffer1;
	unsigned char *buffer2;
	unsigned char *buffer3;

	if (Cmd_Argc() != 3)
	{
		Con_Print("envmap <basename> <size>: save out 6 cubic environment map images, usable with loadsky, note that size must one of 128, 256, 512, or 1024 and can't be bigger than your current resolution\n");
		return;
	}

	strlcpy (basename, Cmd_Argv(1), sizeof (basename));
	size = atoi(Cmd_Argv(2));
	if (size != 128 && size != 256 && size != 512 && size != 1024)
	{
		Con_Print("envmap: size must be one of 128, 256, 512, or 1024\n");
		return;
	}
	if (size > vid.width || size > vid.height)
	{
		Con_Print("envmap: your resolution is not big enough to render that size\n");
		return;
	}

	envmap = true;

	r_refdef.x = 0;
	r_refdef.y = 0;
	r_refdef.width = size;
	r_refdef.height = size;

	r_refdef.frustum_x = tan(90 * M_PI / 360.0);
	r_refdef.frustum_y = tan(90 * M_PI / 360.0);

	buffer1 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3);
	buffer2 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3);
	buffer3 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3 + 18);

	for (j = 0;j < 12;j++)
	{
		sprintf(filename, "env/%s%s.tga", basename, envmapinfo[j].name);
		Matrix4x4_CreateFromQuakeEntity(&r_refdef.viewentitymatrix, r_vieworigin[0], r_vieworigin[1], r_vieworigin[2], envmapinfo[j].angles[0], envmapinfo[j].angles[1], envmapinfo[j].angles[2], 1);
		R_ClearScreen();
		R_Mesh_Start();
		R_RenderView();
		R_Mesh_Finish();
		SCR_ScreenShot(filename, buffer1, buffer2, buffer3, 0, vid.height - (r_refdef.y + r_refdef.height), size, size, envmapinfo[j].flipx, envmapinfo[j].flipy, envmapinfo[j].flipdiagonaly, false, false);
	}

	Mem_Free (buffer1);
	Mem_Free (buffer2);
	Mem_Free (buffer3);

	envmap = false;
}

//=============================================================================

// LordHavoc: SHOWLMP stuff
#define SHOWLMP_MAXLABELS 256
typedef struct showlmp_s
{
	qboolean	isactive;
	float		x;
	float		y;
	char		label[32];
	char		pic[128];
}
showlmp_t;

showlmp_t showlmp[SHOWLMP_MAXLABELS];

void SHOWLMP_decodehide(void)
{
	int i;
	char *lmplabel;
	lmplabel = MSG_ReadString();
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive && strcmp(showlmp[i].label, lmplabel) == 0)
		{
			showlmp[i].isactive = false;
			return;
		}
}

void SHOWLMP_decodeshow(void)
{
	int i, k;
	char lmplabel[256], picname[256];
	float x, y;
	strlcpy (lmplabel,MSG_ReadString(), sizeof (lmplabel));
	strlcpy (picname, MSG_ReadString(), sizeof (picname));
	if (gamemode == GAME_NEHAHRA) // LordHavoc: nasty old legacy junk
	{
		x = MSG_ReadByte();
		y = MSG_ReadByte();
	}
	else
	{
		x = MSG_ReadShort();
		y = MSG_ReadShort();
	}
	k = -1;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
		{
			if (strcmp(showlmp[i].label, lmplabel) == 0)
			{
				k = i;
				break; // drop out to replace it
			}
		}
		else if (k < 0) // find first empty one to replace
			k = i;
	if (k < 0)
		return; // none found to replace
	// change existing one
	showlmp[k].isactive = true;
	strlcpy (showlmp[k].label, lmplabel, sizeof (showlmp[k].label));
	strlcpy (showlmp[k].pic, picname, sizeof (showlmp[k].pic));
	showlmp[k].x = x;
	showlmp[k].y = y;
}

void SHOWLMP_drawall(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
			DrawQ_Pic(showlmp[i].x, showlmp[i].y, Draw_CachePic(showlmp[i].pic, true), 0, 0, 1, 1, 1, 1, 0);
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

qboolean SCR_ScreenShot(char *filename, unsigned char *buffer1, unsigned char *buffer2, unsigned char *buffer3, int x, int y, int width, int height, qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean jpeg, qboolean gammacorrect)
{
	int	indices[3] = {0,1,2};
	qboolean ret;

	if (!r_render.integer)
		return false;

	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer1);
	CHECKGLERROR

	if (scr_screenshot_gamma.value != 1 && gammacorrect)
	{
		int i;
		double igamma = 1.0 / scr_screenshot_gamma.value;
		unsigned char ramp[256];
		for (i = 0;i < 256;i++)
			ramp[i] = (unsigned char) (pow(i * (1.0 / 255.0), igamma) * 255.0);
		for (i = 0;i < width*height*3;i++)
			buffer1[i] = ramp[buffer1[i]];
	}

	Image_CopyMux (buffer2, buffer1, width, height, flipx, flipy, flipdiagonal, 3, 3, indices);

	if (jpeg)
		ret = JPEG_SaveImage_preflipped (filename, width, height, buffer2);
	else
		ret = Image_WriteTGARGB_preflipped (filename, width, height, buffer2, buffer3);

	return ret;
}

//=============================================================================

void R_ClearScreen(void)
{
	if (r_render.integer)
	{
		// clear to black
		if (fogenabled)
			qglClearColor(fogcolor[0],fogcolor[1],fogcolor[2],0);
		else
			qglClearColor(0,0,0,0);
		CHECKGLERROR
		qglClearDepth(1);CHECKGLERROR
		if (gl_stencil)
		{
			// LordHavoc: we use a stencil centered around 128 instead of 0,
			// to avoid clamping interfering with strange shadow volume
			// drawing orders
			qglClearStencil(128);CHECKGLERROR
		}
		// clear the screen
		GL_Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (gl_stencil ? GL_STENCIL_BUFFER_BIT : 0));
		// set dithering mode
		if (gl_dither.integer)
		{
			qglEnable(GL_DITHER);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_DITHER);CHECKGLERROR
		}
	}
}

qboolean CL_VM_UpdateView (void);
void SCR_DrawConsole (void);
void R_Shadow_EditLights_DrawSelectedLightProperties(void);

int r_stereo_side;

void SCR_DrawScreen (void)
{
	R_Mesh_Start();

	if (r_timereport_active)
		R_TimeReport("setup");

	if (cls.signon == SIGNONS)
	{
		float size;

		size = scr_viewsize.value * (1.0 / 100.0);
		size = min(size, 1);

		if (r_stereo_sidebyside.integer)
		{
			r_refdef.width = vid.width * size / 2.5;
			r_refdef.height = vid.height * size / 2.5 * (1 - bound(0, r_letterbox.value, 100) / 100);
			r_refdef.x = (vid.width - r_refdef.width * 2.5) * 0.5;
			r_refdef.y = (vid.height - r_refdef.height)/2;
			if (r_stereo_side)
				r_refdef.x += r_refdef.width * 1.5;
		}
		else
		{
			r_refdef.width = vid.width * size;
			r_refdef.height = vid.height * size * (1 - bound(0, r_letterbox.value, 100) / 100);
			r_refdef.x = (vid.width - r_refdef.width)/2;
			r_refdef.y = (vid.height - r_refdef.height)/2;
		}

		// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
		// LordHavoc: this is designed to produce widescreen fov values
		// when the screen is wider than 4/3 width/height aspect, to do
		// this it simply assumes the requested fov is the vertical fov
		// for a 4x3 display, if the ratio is not 4x3 this makes the fov
		// higher/lower according to the ratio
		r_refdef.frustum_y = tan(scr_fov.value * cl.viewzoom * M_PI / 360.0) * (3.0/4.0);
		r_refdef.frustum_x = r_refdef.frustum_y * (float)r_refdef.width / (float)r_refdef.height / vid_pixelheight.value;

		r_refdef.frustum_x *= r_refdef.frustumscale_x;
		r_refdef.frustum_y *= r_refdef.frustumscale_y;

		if(!CL_VM_UpdateView())
			R_RenderView();
		else
			SCR_DrawConsole();

		if (scr_zoomwindow.integer)
		{
			float sizex = bound(10, scr_zoomwindow_viewsizex.value, 100) / 100.0;
			float sizey = bound(10, scr_zoomwindow_viewsizey.value, 100) / 100.0;
			r_refdef.width = vid.width * sizex;
			r_refdef.height = vid.height * sizey;
			r_refdef.x = (vid.width - r_refdef.width)/2;
			r_refdef.y = 0;

			r_refdef.frustum_y = tan(scr_zoomwindow_fov.value * cl.viewzoom * M_PI / 360.0) * (3.0/4.0);
			r_refdef.frustum_x = r_refdef.frustum_y * vid_pixelheight.value * (float)r_refdef.width / (float)r_refdef.height;

			r_refdef.frustum_x *= r_refdef.frustumscale_x;
			r_refdef.frustum_y *= r_refdef.frustumscale_y;

			if(!CL_VM_UpdateView())
				R_RenderView();
		}
	}

	if (!r_stereo_sidebyside.integer)
	{
		r_refdef.width = vid.width;
		r_refdef.height = vid.height;
		r_refdef.x = 0;
		r_refdef.y = 0;
	}

	// draw 2D stuff
	DrawQ_Begin();

	//FIXME: force menu if nothing else to look at?
	//if (key_dest == key_game && cls.signon != SIGNONS && cls.state == ca_disconnected)

	if (cls.signon == SIGNONS)
	{
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		if (!r_letterbox.value)
			Sbar_Draw();
		SHOWLMP_drawall();
		SCR_CheckDrawCenterString();
	}
	MR_Draw();
	CL_DrawVideo();
	R_Shadow_EditLights_DrawSelectedLightProperties();

	if(!csqc_loaded)
		SCR_DrawConsole();

	SCR_DrawBrand();

	SCR_DrawDownload();

	if (r_timereport_active)
		R_TimeReport("2d");

	if (cls.signon == SIGNONS)
		R_TimeReport_Frame();

	DrawQ_Finish();

	R_DrawGamma();

	R_Mesh_Finish();

	if (r_timereport_active)
		R_TimeReport("meshfinish");
}

void SCR_UpdateLoadingScreen (void)
{
	float x, y;
	cachepic_t *pic;
	rmeshstate_t m;
	float vertex3f[12];
	float texcoord2f[8];
	// don't do anything if not initialized yet
	if (vid_hidden)
		return;
	qglViewport(0, 0, vid.width, vid.height);
	//qglDisable(GL_SCISSOR_TEST);
	//qglDepthMask(1);
	qglColorMask(1,1,1,1);
	//qglClearColor(0,0,0,0);
	//qglClear(GL_COLOR_BUFFER_BIT);
	//qglCullFace(GL_FRONT);
	//qglDisable(GL_CULL_FACE);
	//R_ClearScreen();
	R_Textures_Frame();
	GL_SetupView_Mode_Ortho(0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100);
	R_Mesh_Start();
	R_Mesh_Matrix(&identitymatrix);
	// draw the loading plaque
	pic = Draw_CachePic("gfx/loading", true);
	x = (vid_conwidth.integer - pic->width)/2;
	y = (vid_conheight.integer - pic->height)/2;
	GL_Color(1,1,1,1);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthTest(false);
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = vertex3f;
	m.pointer_texcoord[0] = texcoord2f;
	m.tex[0] = R_GetTexture(pic->tex);
	R_Mesh_State(&m);
	vertex3f[2] = vertex3f[5] = vertex3f[8] = vertex3f[11] = 0;
	vertex3f[0] = vertex3f[9] = x;
	vertex3f[1] = vertex3f[4] = y;
	vertex3f[3] = vertex3f[6] = x + pic->width;
	vertex3f[7] = vertex3f[10] = y + pic->height;
	texcoord2f[0] = 0;texcoord2f[1] = 0;
	texcoord2f[2] = 1;texcoord2f[3] = 0;
	texcoord2f[4] = 1;texcoord2f[5] = 1;
	texcoord2f[6] = 0;texcoord2f[7] = 1;
	R_Mesh_Draw(0, 4, 2, polygonelements);
	R_Mesh_Finish();
	// refresh
	VID_Finish(false);
}

void CL_UpdateScreen(void)
{
	float conwidth, conheight;

	if (vid_hidden)
		return;

	if (!scr_initialized || !con_initialized || vid_hidden)
		return;				// not initialized yet

	// don't allow cheats in multiplayer
	if (!cl.islocalgame && cl.worldmodel)
	{
		if (r_fullbright.integer != 0)
			Cvar_Set ("r_fullbright", "0");
		if (r_ambient.value != 0)
			Cvar_Set ("r_ambient", "0");
	}

	conwidth = bound(320, vid_conwidth.value, 2048);
	conheight = bound(200, vid_conheight.value, 1536);
	if (vid_conwidth.value != conwidth)
		Cvar_SetValue("vid_conwidth", conwidth);
	if (vid_conheight.value != conheight)
		Cvar_SetValue("vid_conheight", conheight);

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

	// bound field of view
	if (scr_fov.value < 1)
		Cvar_Set ("fov","1");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

	// validate r_textureunits cvar
	if (r_textureunits.integer > gl_textureunits)
		Cvar_SetValueQuick(&r_textureunits, gl_textureunits);
	if (r_textureunits.integer < 1)
		Cvar_SetValueQuick(&r_textureunits, 1);

	// validate gl_combine cvar
	if (gl_combine.integer && !gl_combine_extension)
		Cvar_SetValueQuick(&gl_combine, 0);

	// intermission is always full screen
	if (cl.intermission)
		sb_lines = 0;
	else
	{
		if (scr_viewsize.value >= 120)
			sb_lines = 0;		// no status bar at all
		else if (scr_viewsize.value >= 110)
			sb_lines = 24;		// no inventory
		else
			sb_lines = 24+16+8;
	}

	r_refdef.colormask[0] = 1;
	r_refdef.colormask[1] = 1;
	r_refdef.colormask[2] = 1;

	if (r_timereport_active)
		R_TimeReport("other");

	SCR_SetUpToDrawConsole();

	if (r_timereport_active)
		R_TimeReport("start");

	CHECKGLERROR
	qglViewport(0, 0, vid.width, vid.height);
	qglDisable(GL_SCISSOR_TEST);
	qglDepthMask(1);
	qglColorMask(1,1,1,1);
	qglClearColor(0,0,0,0);
	qglClear(GL_COLOR_BUFFER_BIT);
	CHECKGLERROR

	if (r_timereport_active)
		R_TimeReport("clear");

	if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer || r_stereo_sidebyside.integer)
	{
		matrix4x4_t originalmatrix = r_refdef.viewentitymatrix;
		r_refdef.viewentitymatrix.m[0][3] = originalmatrix.m[0][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[0][1];
		r_refdef.viewentitymatrix.m[1][3] = originalmatrix.m[1][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[1][1];
		r_refdef.viewentitymatrix.m[2][3] = originalmatrix.m[2][3] + r_stereo_separation.value * -0.5f * r_refdef.viewentitymatrix.m[2][1];

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 0;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.colormask[0] = 1;
			r_refdef.colormask[1] = 0;
			r_refdef.colormask[2] = 0;
		}

		SCR_DrawScreen();

		r_refdef.viewentitymatrix.m[0][3] = originalmatrix.m[0][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[0][1];
		r_refdef.viewentitymatrix.m[1][3] = originalmatrix.m[1][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[1][1];
		r_refdef.viewentitymatrix.m[2][3] = originalmatrix.m[2][3] + r_stereo_separation.value * 0.5f * r_refdef.viewentitymatrix.m[2][1];

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 1;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.colormask[0] = 0;
			r_refdef.colormask[1] = r_stereo_redcyan.integer || r_stereo_redgreen.integer;
			r_refdef.colormask[2] = r_stereo_redcyan.integer || r_stereo_redblue.integer;
		}

		SCR_DrawScreen();

		r_refdef.viewentitymatrix = originalmatrix;
	}
	else
		SCR_DrawScreen();

	SCR_CaptureVideo();

	VID_Finish(true);
	if (r_timereport_active)
		R_TimeReport("finish");
}

void CL_Screen_NewMap(void)
{
	SHOWLMP_clear();
}
