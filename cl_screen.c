
#include "quakedef.h"
#include "cl_video.h"
#include "image.h"
#include "jpeg.h"
#include "cl_collision.h"
#include "libcurl.h"
#include "csprogs.h"

// we have to include snd_main.h here only to get access to snd_renderbuffer->format.speed when writing the AVI headers
#include "snd_main.h"

cvar_t scr_viewsize = {CVAR_SAVE, "viewsize","100", "how large the view should be, 110 disables inventory bar, 120 disables status bar"};
cvar_t scr_fov = {CVAR_SAVE, "fov","90", "field of vision, 1-170 degrees, default 90, some players use 110-130"};	// 1 - 170
cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1", "opacity of console background"};
cvar_t scr_conbrightness = {CVAR_SAVE, "scr_conbrightness", "1", "brightness of console background (0 = black, 1 = image)"};
cvar_t scr_conforcewhiledisconnected = {0, "scr_conforcewhiledisconnected", "1", "forces fullscreen console while disconnected"};
cvar_t scr_menuforcewhiledisconnected = {0, "scr_menuforcewhiledisconnected", "0", "forces menu while disconnected"};
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
cvar_t scr_screenshot_gammaboost = {CVAR_SAVE, "scr_screenshot_gammaboost","1", "gamma correction on saved screenshots and videos, 1.0 saves unmodified images"};
// scr_screenshot_name is defined in fs.c
cvar_t cl_capturevideo = {0, "cl_capturevideo", "0", "enables saving of video to a .avi file using uncompressed I420 colorspace and PCM audio, note that scr_screenshot_gammaboost affects the brightness of the output)"};
cvar_t cl_capturevideo_realtime = {0, "cl_capturevideo_realtime", "0", "causes video saving to operate in realtime (mostly useful while playing, not while capturing demos), this can produce a much lower quality video due to poor sound/video sync and will abort saving if your machine stalls for over 1 second"};
cvar_t cl_capturevideo_fps = {0, "cl_capturevideo_fps", "30", "how many frames per second to save (29.97 for NTSC, 30 for typical PC video, 15 can be useful)"};
cvar_t cl_capturevideo_number = {CVAR_SAVE, "cl_capturevideo_number", "1", "number to append to video filename, incremented each time a capture begins"};
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
cvar_t scr_stipple = {0, "scr_stipple", "0", "interlacing-like stippling of the display"};
cvar_t scr_refresh = {0, "scr_refresh", "1", "allows you to completely shut off rendering for benchmarking purposes"};


int jpeg_supported = false;

qboolean	scr_initialized;		// ready to draw

float		scr_con_current;

extern int	con_vislines;

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
		remaining = (int)(scr_printspeed.value * (cl.time - scr_centertime_start));
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (remaining < 1)
		return;

	if (scr_center_lines <= 4)
		y = (int)(vid_conheight.integer*0.35);
	else
		y = 48;

	color = -1;
	do
	{
		// scan the number of characters on the line, not counting color codes
		int chars = 0;
		for (l=0 ; l<vid_conwidth.integer/8 ; l++)
		{
			if (start[l] == '\n' || !start[l])
				break;
			// color codes add no visible characters, so don't count them
			if (start[l] == STRING_COLOR_TAG && (start[l+1] >= '0' && start[l+1] <= '9'))
				l++;
			else
				chars++;
		}
		x = (vid_conwidth.integer - chars*8)/2;
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

	scr_centertime_off -= cl.realframetime;

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

	if (cl.realframetime < 0.1)
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
SCR_DrawQWDownload
==============
*/
static int SCR_DrawQWDownload(int offset)
{
	int len;
	float x, y;
	float size = 8;
	char temp[256];
	if (!cls.qw_downloadname[0])
	{
		cls.qw_downloadspeedrate = 0;
		cls.qw_downloadspeedtime = realtime;
		cls.qw_downloadspeedcount = 0;
		return 0;
	}
	if (realtime >= cls.qw_downloadspeedtime + 1)
	{
		cls.qw_downloadspeedrate = cls.qw_downloadspeedcount;
		cls.qw_downloadspeedtime = realtime;
		cls.qw_downloadspeedcount = 0;
	}
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		dpsnprintf(temp, sizeof(temp), "Downloading %s %3i%% (%i) at %i bytes/s\n", cls.qw_downloadname, cls.qw_downloadpercent, cls.qw_downloadmemorycursize, cls.qw_downloadspeedrate);
	else
		dpsnprintf(temp, sizeof(temp), "Downloading %s %3i%% (%i/%i) at %i bytes/s\n", cls.qw_downloadname, cls.qw_downloadpercent, cls.qw_downloadmemorycursize, cls.qw_downloadmemorymaxsize, cls.qw_downloadspeedrate);
	len = (int)strlen(temp);
	x = (vid_conwidth.integer - len*size) / 2;
	y = vid_conheight.integer - size - offset;
	DrawQ_Pic(0, y, NULL, vid_conwidth.integer, size, 0, 0, 0, 0.5, 0);
	DrawQ_String(x, y, temp, len, size, size, 1, 1, 1, 1, 0);
	return 8;
}

/*
==============
SCR_DrawCurlDownload
==============
*/
static int SCR_DrawCurlDownload(int offset)
{
	int len;
	int nDownloads;
	int i;
	float x, y;
	float size = 8;
	Curl_downloadinfo_t *downinfo;
	char temp[256];
	const char *addinfo;

	downinfo = Curl_GetDownloadInfo(&nDownloads, &addinfo);
	if(!downinfo)
		return 0;

	y = vid_conheight.integer - size * nDownloads - offset;

	if(addinfo)
	{
		len = (int)strlen(addinfo);
		x = (vid_conwidth.integer - len*size) / 2;
		DrawQ_Pic(0, y - size, NULL, vid_conwidth.integer, size, 1, 1, 1, 0.8, 0);
		DrawQ_String(x, y - size, addinfo, len, size, size, 0, 0, 0, 1, 0);
	}

	for(i = 0; i != nDownloads; ++i)
	{
		if(downinfo[i].queued)
			dpsnprintf(temp, sizeof(temp), "Still in queue: %s\n", downinfo[i].filename);
		else if(downinfo[i].progress <= 0)
			dpsnprintf(temp, sizeof(temp), "Downloading %s ...  ???.?%% @ %.1f KiB/s\n", downinfo[i].filename, downinfo[i].speed / 1024.0);
		else
			dpsnprintf(temp, sizeof(temp), "Downloading %s ...  %5.1f%% @ %.1f KiB/s\n", downinfo[i].filename, 100.0 * downinfo[i].progress, downinfo[i].speed / 1024.0);
		len = (int)strlen(temp);
		x = (vid_conwidth.integer - len*size) / 2;
		DrawQ_Pic(0, y + i * size, NULL, vid_conwidth.integer, size, 0, 0, 0, 0.8, 0);
		DrawQ_String(x, y + i * size, temp, len, size, size, 1, 1, 1, 1, 0);
	}

	Z_Free(downinfo);

	return 8 * (nDownloads + (addinfo ? 1 : 0));
}

/*
==============
SCR_DrawDownload
==============
*/
static void SCR_DrawDownload()
{
	int offset = 0;
	offset += SCR_DrawQWDownload(offset);
	offset += SCR_DrawCurlDownload(offset);
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
		Con_DrawConsole ((int)scr_con_current);
	else
	{
		con_vislines = 0;
		if ((key_dest == key_game || key_dest == key_message) && !r_letterbox.value)
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
	SCR_UpdateLoadingScreen(false);
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

	CHECKGLERROR
	qglFinish();CHECKGLERROR
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
	cl_locnode_t *loc;

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
		// put the location name in the r_speeds display as it greatly helps
		// when creating loc files
		loc = CL_Locs_FindNearest(cl.movement_origin);
		if (loc)
			sprintf(r_speeds_string + strlen(r_speeds_string), "Location: %s\n", loc->name);
		sprintf(r_speeds_string + strlen(r_speeds_string), "org:'%+8.2f %+8.2f %+8.2f' dir:'%+2.3f %+2.3f %+2.3f'\n", r_view.origin[0], r_view.origin[1], r_view.origin[2], r_view.forward[0], r_view.forward[1], r_view.forward[2]);
		sprintf(r_speeds_string + strlen(r_speeds_string), "%5i entities%6i surfaces%6i triangles%5i leafs%5i portals%6i particles\n", r_refdef.stats.entities, r_refdef.stats.entities_surfaces, r_refdef.stats.entities_triangles, r_refdef.stats.world_leafs, r_refdef.stats.world_portals, r_refdef.stats.particles);
		sprintf(r_speeds_string + strlen(r_speeds_string), "%4i lights%4i clears%4i scissored%7i light%7i shadow%7i dynamic\n", r_refdef.stats.lights, r_refdef.stats.lights_clears, r_refdef.stats.lights_scissored, r_refdef.stats.lights_lighttriangles, r_refdef.stats.lights_shadowtriangles, r_refdef.stats.lights_dynamicshadowtriangles);
		if (r_refdef.stats.bloom)
			sprintf(r_speeds_string + strlen(r_speeds_string), "rendered%6i meshes%8i triangles bloompixels%8i copied%8i drawn\n", r_refdef.stats.meshes, r_refdef.stats.meshes_elements / 3, r_refdef.stats.bloom_copypixels, r_refdef.stats.bloom_drawpixels);
		else
			sprintf(r_speeds_string + strlen(r_speeds_string), "rendered%6i meshes%8i triangles\n", r_refdef.stats.meshes, r_refdef.stats.meshes_elements / 3);

		memset(&r_refdef.stats, 0, sizeof(r_refdef.stats));

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
	Cvar_RegisterVariable (&scr_screenshot_gammaboost);
	Cvar_RegisterVariable (&cl_capturevideo);
	Cvar_RegisterVariable (&cl_capturevideo_realtime);
	Cvar_RegisterVariable (&cl_capturevideo_fps);
	Cvar_RegisterVariable (&cl_capturevideo_number);
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
	Cvar_RegisterVariable(&scr_stipple);
	Cvar_RegisterVariable(&scr_refresh);

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

static void SCR_CaptureVideo_RIFF_Start(void)
{
	memset(&cls.capturevideo.riffbuffer, 0, sizeof(sizebuf_t));
	cls.capturevideo.riffbuffer.maxsize = sizeof(cls.capturevideo.riffbufferdata);
	cls.capturevideo.riffbuffer.data = cls.capturevideo.riffbufferdata;
}

static void SCR_CaptureVideo_RIFF_Flush(void)
{
	if (cls.capturevideo.riffbuffer.cursize > 0)
	{
		if (!FS_Write(cls.capturevideo.videofile, cls.capturevideo.riffbuffer.data, cls.capturevideo.riffbuffer.cursize))
			cls.capturevideo.error = true;
		cls.capturevideo.riffbuffer.cursize = 0;
		cls.capturevideo.riffbuffer.overflowed = false;
	}
}

static void SCR_CaptureVideo_RIFF_WriteBytes(const unsigned char *data, size_t size)
{
	SCR_CaptureVideo_RIFF_Flush();
	if (!FS_Write(cls.capturevideo.videofile, data, size))
		cls.capturevideo.error = true;
}

static void SCR_CaptureVideo_RIFF_Write32(int n)
{
	if (cls.capturevideo.riffbuffer.cursize + 4 > cls.capturevideo.riffbuffer.maxsize)
		SCR_CaptureVideo_RIFF_Flush();
	MSG_WriteLong(&cls.capturevideo.riffbuffer, n);
}

static void SCR_CaptureVideo_RIFF_Write16(int n)
{
	if (cls.capturevideo.riffbuffer.cursize + 2 > cls.capturevideo.riffbuffer.maxsize)
		SCR_CaptureVideo_RIFF_Flush();
	MSG_WriteShort(&cls.capturevideo.riffbuffer, n);
}

static void SCR_CaptureVideo_RIFF_WriteFourCC(const char *chunkfourcc)
{
	if (cls.capturevideo.riffbuffer.cursize + (int)strlen(chunkfourcc) > cls.capturevideo.riffbuffer.maxsize)
		SCR_CaptureVideo_RIFF_Flush();
	MSG_WriteUnterminatedString(&cls.capturevideo.riffbuffer, chunkfourcc);
}

static void SCR_CaptureVideo_RIFF_WriteTerminatedString(const char *string)
{
	if (cls.capturevideo.riffbuffer.cursize + (int)strlen(string) > cls.capturevideo.riffbuffer.maxsize)
		SCR_CaptureVideo_RIFF_Flush();
	MSG_WriteString(&cls.capturevideo.riffbuffer, string);
}

static fs_offset_t SCR_CaptureVideo_RIFF_GetPosition(void)
{
	SCR_CaptureVideo_RIFF_Flush();
	return FS_Tell(cls.capturevideo.videofile);
}

static void SCR_CaptureVideo_RIFF_Push(const char *chunkfourcc, const char *listtypefourcc)
{
	SCR_CaptureVideo_RIFF_WriteFourCC(chunkfourcc);
	SCR_CaptureVideo_RIFF_Write32(0);
	SCR_CaptureVideo_RIFF_Flush();
	cls.capturevideo.riffstackstartoffset[cls.capturevideo.riffstacklevel++] = SCR_CaptureVideo_RIFF_GetPosition();
	if (listtypefourcc)
		SCR_CaptureVideo_RIFF_WriteFourCC(listtypefourcc);
}

static void SCR_CaptureVideo_RIFF_Pop(void)
{
	fs_offset_t offset;
	int x;
	unsigned char sizebytes[4];
	// write out the chunk size and then return to the current file position
	cls.capturevideo.riffstacklevel--;
	offset = SCR_CaptureVideo_RIFF_GetPosition();
	x = (int)(offset - (cls.capturevideo.riffstackstartoffset[cls.capturevideo.riffstacklevel]));
	sizebytes[0] = (x) & 0xff;sizebytes[1] = (x >> 8) & 0xff;sizebytes[2] = (x >> 16) & 0xff;sizebytes[3] = (x >> 24) & 0xff;
	FS_Seek(cls.capturevideo.videofile, -(x + 4), SEEK_END);
	FS_Write(cls.capturevideo.videofile, sizebytes, 4);
	FS_Seek(cls.capturevideo.videofile, 0, SEEK_END);
	if (offset & 1)
	{
		unsigned char c = 0;
		FS_Write(cls.capturevideo.videofile, &c, 1);
	}
}

static void SCR_CaptureVideo_RIFF_IndexEntry(const char *chunkfourcc, int chunksize, int flags)
{
	if (cls.capturevideo.riffstacklevel != 2)
		Sys_Error("SCR_Capturevideo_RIFF_IndexEntry: RIFF stack level is %i (should be 2)\n", cls.capturevideo.riffstacklevel);
	if (cls.capturevideo.riffindexbuffer.cursize + 16 > cls.capturevideo.riffindexbuffer.maxsize)
	{
		int oldsize = cls.capturevideo.riffindexbuffer.maxsize;
		unsigned char *olddata;
		olddata = cls.capturevideo.riffindexbuffer.data;
		cls.capturevideo.riffindexbuffer.maxsize = max(cls.capturevideo.riffindexbuffer.maxsize * 2, 4096);
		cls.capturevideo.riffindexbuffer.data = Mem_Alloc(tempmempool, cls.capturevideo.riffindexbuffer.maxsize);
		if (olddata)
		{
			memcpy(cls.capturevideo.riffindexbuffer.data, olddata, oldsize);
			Mem_Free(olddata);
		}
	}
	MSG_WriteUnterminatedString(&cls.capturevideo.riffindexbuffer, chunkfourcc);
	MSG_WriteLong(&cls.capturevideo.riffindexbuffer, flags);
	MSG_WriteLong(&cls.capturevideo.riffindexbuffer, (int)FS_Tell(cls.capturevideo.videofile) - cls.capturevideo.riffstackstartoffset[1]);
	MSG_WriteLong(&cls.capturevideo.riffindexbuffer, chunksize);
}

static void SCR_CaptureVideo_RIFF_Finish(void)
{
	// close the "movi" list
	SCR_CaptureVideo_RIFF_Pop();
	// write the idx1 chunk that we've been building while saving the frames
	SCR_CaptureVideo_RIFF_Push("idx1", NULL);
	SCR_CaptureVideo_RIFF_WriteBytes(cls.capturevideo.riffindexbuffer.data, cls.capturevideo.riffindexbuffer.cursize);
	SCR_CaptureVideo_RIFF_Pop();
	cls.capturevideo.riffindexbuffer.cursize = 0;
	// pop the RIFF chunk itself
	while (cls.capturevideo.riffstacklevel > 0)
		SCR_CaptureVideo_RIFF_Pop();
	SCR_CaptureVideo_RIFF_Flush();
}

static void SCR_CaptureVideo_RIFF_OverflowCheck(int framesize)
{
	fs_offset_t cursize;
	if (cls.capturevideo.riffstacklevel != 2)
		Sys_Error("SCR_CaptureVideo_RIFF_OverflowCheck: chunk stack leakage!\n");
	// check where we are in the file
	SCR_CaptureVideo_RIFF_Flush();
	cursize = SCR_CaptureVideo_RIFF_GetPosition() - cls.capturevideo.riffstackstartoffset[0];
	// if this would overflow the windows limit of 1GB per RIFF chunk, we need
	// to close the current RIFF chunk and open another for future frames
	if (8 + cursize + framesize + cls.capturevideo.riffindexbuffer.cursize + 8 > 1<<30)
	{
		SCR_CaptureVideo_RIFF_Finish();
		// begin a new 1GB extended section of the AVI
		SCR_CaptureVideo_RIFF_Push("RIFF", "AVIX");
		SCR_CaptureVideo_RIFF_Push("LIST", "movi");
	}
}

void SCR_CaptureVideo_BeginVideo(void)
{
	double gamma, g;
	int width = vid.width, height = vid.height, x;
	unsigned int i;
	if (cls.capturevideo.active)
		return;
	memset(&cls.capturevideo, 0, sizeof(cls.capturevideo));
	// soundrate is figured out on the first SoundFrame
	cls.capturevideo.active = true;
	cls.capturevideo.starttime = Sys_DoubleTime();
	cls.capturevideo.framerate = bound(1, cl_capturevideo_fps.value, 1000);
	cls.capturevideo.soundrate = S_GetSoundRate();
	cls.capturevideo.frame = 0;
	cls.capturevideo.soundsampleframe = 0;
	cls.capturevideo.realtime = cl_capturevideo_realtime.integer != 0;
	cls.capturevideo.buffer = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * (3+3+3) + 18);
	gamma = 1.0/scr_screenshot_gammaboost.value;
	dpsnprintf(cls.capturevideo.basename, sizeof(cls.capturevideo.basename), "video/dpvideo%03i", cl_capturevideo_number.integer);
	Cvar_SetValueQuick(&cl_capturevideo_number, cl_capturevideo_number.integer + 1);

	/*
	for (i = 0;i < 256;i++)
	{
		unsigned char j = (unsigned char)bound(0, 255*pow(i/255.0, gamma), 255);
		cls.capturevideo.rgbgammatable[0][i] = j;
		cls.capturevideo.rgbgammatable[1][i] = j;
		cls.capturevideo.rgbgammatable[2][i] = j;
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
		cls.capturevideo.rgbtoyuvscaletable[0][0][i] = (short)(g *  0.299);
		cls.capturevideo.rgbtoyuvscaletable[0][1][i] = (short)(g *  0.587);
		cls.capturevideo.rgbtoyuvscaletable[0][2][i] = (short)(g *  0.114);
		// Cb weights from RGB
		cls.capturevideo.rgbtoyuvscaletable[1][0][i] = (short)(g * -0.169);
		cls.capturevideo.rgbtoyuvscaletable[1][1][i] = (short)(g * -0.332);
		cls.capturevideo.rgbtoyuvscaletable[1][2][i] = (short)(g *  0.500);
		// Cr weights from RGB
		cls.capturevideo.rgbtoyuvscaletable[2][0][i] = (short)(g *  0.500);
		cls.capturevideo.rgbtoyuvscaletable[2][1][i] = (short)(g * -0.419);
		cls.capturevideo.rgbtoyuvscaletable[2][2][i] = (short)(g * -0.0813);
		// range reduction of YCbCr to valid signal range
		cls.capturevideo.yuvnormalizetable[0][i] = 16 + i * (236-16) / 256;
		cls.capturevideo.yuvnormalizetable[1][i] = 16 + i * (240-16) / 256;
		cls.capturevideo.yuvnormalizetable[2][i] = 16 + i * (240-16) / 256;
	}

	//if (cl_capturevideo_)
	//{
	//}
	//else
	{
		cls.capturevideo.format = CAPTUREVIDEOFORMAT_AVI_I420;
		cls.capturevideo.videofile = FS_Open (va("%s.avi", cls.capturevideo.basename), "wb", false, true);
		SCR_CaptureVideo_RIFF_Start();
		// enclosing RIFF chunk (there can be multiple of these in >1GB files, the later ones are "AVIX" instead of "AVI " and have no header/stream info)
		SCR_CaptureVideo_RIFF_Push("RIFF", "AVI ");
		// AVI main header
		SCR_CaptureVideo_RIFF_Push("LIST", "hdrl");
		SCR_CaptureVideo_RIFF_Push("avih", NULL);
		SCR_CaptureVideo_RIFF_Write32((int)(1000000.0 / cls.capturevideo.framerate)); // microseconds per frame
		SCR_CaptureVideo_RIFF_Write32(0); // max bytes per second
		SCR_CaptureVideo_RIFF_Write32(0); // padding granularity
		SCR_CaptureVideo_RIFF_Write32(0x910); // flags (AVIF_HASINDEX | AVIF_ISINTERLEAVED | AVIF_TRUSTCKTYPE)
		cls.capturevideo.videofile_totalframes_offset1 = SCR_CaptureVideo_RIFF_GetPosition();
		SCR_CaptureVideo_RIFF_Write32(0); // total frames
		SCR_CaptureVideo_RIFF_Write32(0); // initial frames
		if (cls.capturevideo.soundrate)
			SCR_CaptureVideo_RIFF_Write32(2); // number of streams
		else
			SCR_CaptureVideo_RIFF_Write32(1); // number of streams
		SCR_CaptureVideo_RIFF_Write32(0); // suggested buffer size
		SCR_CaptureVideo_RIFF_Write32(width); // width
		SCR_CaptureVideo_RIFF_Write32(height); // height
		SCR_CaptureVideo_RIFF_Write32(0); // reserved[0]
		SCR_CaptureVideo_RIFF_Write32(0); // reserved[1]
		SCR_CaptureVideo_RIFF_Write32(0); // reserved[2]
		SCR_CaptureVideo_RIFF_Write32(0); // reserved[3]
		SCR_CaptureVideo_RIFF_Pop();
		// video stream info
		SCR_CaptureVideo_RIFF_Push("LIST", "strl");
		SCR_CaptureVideo_RIFF_Push("strh", "vids");
		SCR_CaptureVideo_RIFF_WriteFourCC("I420"); // stream fourcc (I420 colorspace, uncompressed)
		SCR_CaptureVideo_RIFF_Write32(0); // flags
		SCR_CaptureVideo_RIFF_Write16(0); // priority
		SCR_CaptureVideo_RIFF_Write16(0); // language
		SCR_CaptureVideo_RIFF_Write32(0); // initial frames
		// find an ideal divisor for the framerate
		for (x = 1;x < 1000;x++)
			if (cls.capturevideo.framerate * x == floor(cls.capturevideo.framerate * x))
				break;
		SCR_CaptureVideo_RIFF_Write32(x); // samples/second divisor
		SCR_CaptureVideo_RIFF_Write32((int)(cls.capturevideo.framerate * x)); // samples/second multiplied by divisor
		SCR_CaptureVideo_RIFF_Write32(0); // start
		cls.capturevideo.videofile_totalframes_offset2 = SCR_CaptureVideo_RIFF_GetPosition();
		SCR_CaptureVideo_RIFF_Write32(0); // length
		SCR_CaptureVideo_RIFF_Write32(width*height+(width/2)*(height/2)*2); // suggested buffer size
		SCR_CaptureVideo_RIFF_Write32(0); // quality
		SCR_CaptureVideo_RIFF_Write32(0); // sample size
		SCR_CaptureVideo_RIFF_Write16(0); // frame left
		SCR_CaptureVideo_RIFF_Write16(0); // frame top
		SCR_CaptureVideo_RIFF_Write16(width); // frame right
		SCR_CaptureVideo_RIFF_Write16(height); // frame bottom
		SCR_CaptureVideo_RIFF_Pop();
		// video stream format
		SCR_CaptureVideo_RIFF_Push("strf", NULL);
		SCR_CaptureVideo_RIFF_Write32(40); // BITMAPINFO struct size
		SCR_CaptureVideo_RIFF_Write32(width); // width
		SCR_CaptureVideo_RIFF_Write32(height); // height
		SCR_CaptureVideo_RIFF_Write16(3); // planes
		SCR_CaptureVideo_RIFF_Write16(12); // bitcount
		SCR_CaptureVideo_RIFF_WriteFourCC("I420"); // compression
		SCR_CaptureVideo_RIFF_Write32(width*height+(width/2)*(height/2)*2); // size of image
		SCR_CaptureVideo_RIFF_Write32(0); // x pixels per meter
		SCR_CaptureVideo_RIFF_Write32(0); // y pixels per meter
		SCR_CaptureVideo_RIFF_Write32(0); // color used
		SCR_CaptureVideo_RIFF_Write32(0); // color important
		SCR_CaptureVideo_RIFF_Pop();
		SCR_CaptureVideo_RIFF_Pop();
		if (cls.capturevideo.soundrate)
		{
			// audio stream info
			SCR_CaptureVideo_RIFF_Push("LIST", "strl");
			SCR_CaptureVideo_RIFF_Push("strh", "auds");
			SCR_CaptureVideo_RIFF_Write32(1); // stream fourcc (PCM audio, uncompressed)
			SCR_CaptureVideo_RIFF_Write32(0); // flags
			SCR_CaptureVideo_RIFF_Write16(0); // priority
			SCR_CaptureVideo_RIFF_Write16(0); // language
			SCR_CaptureVideo_RIFF_Write32(0); // initial frames
			SCR_CaptureVideo_RIFF_Write32(1); // samples/second divisor
			SCR_CaptureVideo_RIFF_Write32((int)(cls.capturevideo.soundrate)); // samples/second multiplied by divisor
			SCR_CaptureVideo_RIFF_Write32(0); // start
			cls.capturevideo.videofile_totalsampleframes_offset = SCR_CaptureVideo_RIFF_GetPosition();
			SCR_CaptureVideo_RIFF_Write32(0); // length
			SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.soundrate * 2); // suggested buffer size (this is a half second)
			SCR_CaptureVideo_RIFF_Write32(0); // quality
			SCR_CaptureVideo_RIFF_Write32(4); // sample size
			SCR_CaptureVideo_RIFF_Write16(0); // frame left
			SCR_CaptureVideo_RIFF_Write16(0); // frame top
			SCR_CaptureVideo_RIFF_Write16(0); // frame right
			SCR_CaptureVideo_RIFF_Write16(0); // frame bottom
			SCR_CaptureVideo_RIFF_Pop();
			// audio stream format
			SCR_CaptureVideo_RIFF_Push("strf", NULL);
			SCR_CaptureVideo_RIFF_Write16(1); // format (uncompressed PCM?)
			SCR_CaptureVideo_RIFF_Write16(2); // channels (stereo)
			SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.soundrate); // sampleframes per second
			SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.soundrate * 4); // average bytes per second
			SCR_CaptureVideo_RIFF_Write16(4); // block align
			SCR_CaptureVideo_RIFF_Write16(16); // bits per sample
			SCR_CaptureVideo_RIFF_Write16(0); // size
			SCR_CaptureVideo_RIFF_Pop();
			SCR_CaptureVideo_RIFF_Pop();
		}
		// close the AVI header list
		SCR_CaptureVideo_RIFF_Pop();
		// software that produced this AVI video file
		SCR_CaptureVideo_RIFF_Push("LIST", "INFO");
		SCR_CaptureVideo_RIFF_Push("ISFT", NULL);
		SCR_CaptureVideo_RIFF_WriteTerminatedString(engineversion);
		SCR_CaptureVideo_RIFF_Pop();
		// enable this junk filler if you like the LIST movi to always begin at 4KB in the file (why?)
#if 0
		SCR_CaptureVideo_RIFF_Push("JUNK", NULL);
		x = 4096 - SCR_CaptureVideo_RIFF_GetPosition();
		while (x > 0)
		{
			const char *junkfiller = "[ DarkPlaces junk data ]";
			int i = min(x, (int)strlen(junkfiller));
			SCR_CaptureVideo_RIFF_WriteBytes((const unsigned char *)junkfiller, i);
			x -= i;
		}
		SCR_CaptureVideo_RIFF_Pop();
#endif
		SCR_CaptureVideo_RIFF_Pop();
		// begin the actual video section now
		SCR_CaptureVideo_RIFF_Push("LIST", "movi");
		// we're done with the headers now...
		SCR_CaptureVideo_RIFF_Flush();
		if (cls.capturevideo.riffstacklevel != 2)
			Sys_Error("SCR_CaptureVideo_BeginVideo: broken AVI writing code (stack level is %i (should be 2) at end of headers)\n", cls.capturevideo.riffstacklevel);
	}

	switch(cls.capturevideo.format)
	{
	case CAPTUREVIDEOFORMAT_AVI_I420:
		break;
	default:
		break;
	}
}

void SCR_CaptureVideo_EndVideo(void)
{
	if (!cls.capturevideo.active)
		return;
	cls.capturevideo.active = false;
	if (cls.capturevideo.videofile)
	{
		switch(cls.capturevideo.format)
		{
		case CAPTUREVIDEOFORMAT_AVI_I420:
			// close any open chunks
			SCR_CaptureVideo_RIFF_Finish();
			// go back and fix the video frames and audio samples fields
			FS_Seek(cls.capturevideo.videofile, cls.capturevideo.videofile_totalframes_offset1, SEEK_SET);
			SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.frame);
			SCR_CaptureVideo_RIFF_Flush();
			FS_Seek(cls.capturevideo.videofile, cls.capturevideo.videofile_totalframes_offset2, SEEK_SET);
			SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.frame);
			SCR_CaptureVideo_RIFF_Flush();
			if (cls.capturevideo.soundrate)
			{
				FS_Seek(cls.capturevideo.videofile, cls.capturevideo.videofile_totalsampleframes_offset, SEEK_SET);
				SCR_CaptureVideo_RIFF_Write32(cls.capturevideo.soundsampleframe);
				SCR_CaptureVideo_RIFF_Flush();
			}
			break;
		default:
			break;
		}
		FS_Close(cls.capturevideo.videofile);
		cls.capturevideo.videofile = NULL;
	}

	if (cls.capturevideo.buffer)
	{
		Mem_Free (cls.capturevideo.buffer);
		cls.capturevideo.buffer = NULL;
	}

	if (cls.capturevideo.riffindexbuffer.data)
	{
		Mem_Free(cls.capturevideo.riffindexbuffer.data);
		cls.capturevideo.riffindexbuffer.data = NULL;
	}

	memset(&cls.capturevideo, 0, sizeof(cls.capturevideo));
}

// converts from RGB24 to I420 colorspace (identical to YV12 except chroma plane order is reversed), this colorspace is handled by the Intel(r) 4:2:0 codec on Windows
void SCR_CaptureVideo_ConvertFrame_RGB_to_I420_flip(int width, int height, unsigned char *instart, unsigned char *outstart)
{
	int x, y;
	int outoffset = (width/2)*(height/2);
	unsigned char *b, *out;
	// process one line at a time, and CbCr every other line at 2 pixel intervals
	for (y = 0;y < height;y++)
	{
		// 1x1 Y
		for (b = instart + (height-1-y)*width*3, out = outstart + y*width, x = 0;x < width;x++, b += 3, out++)
			*out = cls.capturevideo.yuvnormalizetable[0][cls.capturevideo.rgbtoyuvscaletable[0][0][b[0]] + cls.capturevideo.rgbtoyuvscaletable[0][1][b[1]] + cls.capturevideo.rgbtoyuvscaletable[0][2][b[2]]];
		if ((y & 1) == 0)
		{
			// 2x2 Cr and Cb planes
#if 0
			// low quality, no averaging
			for (b = instart + (height-2-y)*width*3, out = outstart + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
			{
				// Cr
				out[0        ] = cls.capturevideo.yuvnormalizetable[1][cls.capturevideo.rgbtoyuvscaletable[1][0][b[0]] + cls.capturevideo.rgbtoyuvscaletable[1][1][b[1]] + cls.capturevideo.rgbtoyuvscaletable[1][2][b[2]] + 128];
				// Cb
				out[outoffset] = cls.capturevideo.yuvnormalizetable[2][cls.capturevideo.rgbtoyuvscaletable[2][0][b[0]] + cls.capturevideo.rgbtoyuvscaletable[2][1][b[1]] + cls.capturevideo.rgbtoyuvscaletable[2][2][b[2]] + 128];
			}
#else
			// high quality, averaging
			int inpitch = width*3;
			for (b = instart + (height-2-y)*width*3, out = outstart + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
			{
				int blockr, blockg, blockb;
				blockr = (b[0] + b[3] + b[inpitch+0] + b[inpitch+3]) >> 2;
				blockg = (b[1] + b[4] + b[inpitch+1] + b[inpitch+4]) >> 2;
				blockb = (b[2] + b[5] + b[inpitch+2] + b[inpitch+5]) >> 2;
				// Cr
				out[0        ] = cls.capturevideo.yuvnormalizetable[1][cls.capturevideo.rgbtoyuvscaletable[1][0][blockr] + cls.capturevideo.rgbtoyuvscaletable[1][1][blockg] + cls.capturevideo.rgbtoyuvscaletable[1][2][blockb] + 128];
				// Cb
				out[outoffset] = cls.capturevideo.yuvnormalizetable[2][cls.capturevideo.rgbtoyuvscaletable[2][0][blockr] + cls.capturevideo.rgbtoyuvscaletable[2][1][blockg] + cls.capturevideo.rgbtoyuvscaletable[2][2][blockb] + 128];
			}
#endif
		}
	}
}

qboolean SCR_CaptureVideo_VideoFrame(int newframenum)
{
	int x = 0, y = 0, width = vid.width, height = vid.height;
	unsigned char *in, *out;
	CHECKGLERROR
	//return SCR_ScreenShot(filename, cls.capturevideo.buffer, cls.capturevideo.buffer + vid.width * vid.height * 3, cls.capturevideo.buffer + vid.width * vid.height * 6, 0, 0, vid.width, vid.height, false, false, false, jpeg, true);
	// speed is critical here, so do saving as directly as possible
	switch (cls.capturevideo.format)
	{
	case CAPTUREVIDEOFORMAT_AVI_I420:
		// if there's no videofile we have to just give up, and abort saving
		if (!cls.capturevideo.videofile)
			return false;
		// FIXME: width/height must be multiple of 2, enforce this?
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cls.capturevideo.buffer);CHECKGLERROR
		in = cls.capturevideo.buffer;
		out = cls.capturevideo.buffer + width*height*3;
		SCR_CaptureVideo_ConvertFrame_RGB_to_I420_flip(width, height, in, out);
		x = width*height+(width/2)*(height/2)*2;
		SCR_CaptureVideo_RIFF_OverflowCheck(8 + x);
		for (;cls.capturevideo.frame < newframenum;cls.capturevideo.frame++)
		{
			SCR_CaptureVideo_RIFF_IndexEntry("00dc", x, 0x10); // AVIIF_KEYFRAME
			SCR_CaptureVideo_RIFF_Push("00dc", NULL);
			SCR_CaptureVideo_RIFF_WriteBytes(out, x);
			SCR_CaptureVideo_RIFF_Pop();
		}
		return true;
	default:
		return false;
	}
}

void SCR_CaptureVideo_SoundFrame(unsigned char *bufstereo16le, size_t length, int rate)
{
	int x;
	cls.capturevideo.soundrate = rate;
	cls.capturevideo.soundsampleframe += length;
	switch (cls.capturevideo.format)
	{
	case CAPTUREVIDEOFORMAT_AVI_I420:
		x = length*4;
		SCR_CaptureVideo_RIFF_OverflowCheck(8 + x);
		SCR_CaptureVideo_RIFF_IndexEntry("01wb", x, 0x10); // AVIIF_KEYFRAME
		SCR_CaptureVideo_RIFF_Push("01wb", NULL);
		SCR_CaptureVideo_RIFF_WriteBytes(bufstereo16le, x);
		SCR_CaptureVideo_RIFF_Pop();
		break;
	default:
		break;
	}
}

void SCR_CaptureVideo(void)
{
	int newframenum;
	if (cl_capturevideo.integer && r_render.integer)
	{
		if (!cls.capturevideo.active)
			SCR_CaptureVideo_BeginVideo();
		if (cls.capturevideo.framerate != cl_capturevideo_fps.value)
		{
			Con_Printf("You can not change the video framerate while recording a video.\n");
			Cvar_SetValueQuick(&cl_capturevideo_fps, cls.capturevideo.framerate);
		}
		// for AVI saving we have to make sure that sound is saved before video
		if (cls.capturevideo.soundrate && !cls.capturevideo.soundsampleframe)
			return;
		if (cls.capturevideo.realtime)
		{
			// preserve sound sync by duplicating frames when running slow
			newframenum = (int)((Sys_DoubleTime() - cls.capturevideo.starttime) * cls.capturevideo.framerate);
		}
		else
			newframenum = cls.capturevideo.frame + 1;
		// if falling behind more than one second, stop
		if (newframenum - cls.capturevideo.frame > (int)ceil(cls.capturevideo.framerate))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, your machine is too slow for this capture speed.\n", cls.capturevideo.frame);
			SCR_CaptureVideo_EndVideo();
			return;
		}
		// write frames
		SCR_CaptureVideo_VideoFrame(newframenum);
		if (cls.capturevideo.error)
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, out of disk space? stopping video capture.\n", cls.capturevideo.frame);
			SCR_CaptureVideo_EndVideo();
		}
	}
	else if (cls.capturevideo.active)
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

	r_refdef.envmap = true;

	R_UpdateVariables();

	r_view.x = 0;
	r_view.y = 0;
	r_view.z = 0;
	r_view.width = size;
	r_view.height = size;
	r_view.depth = 1;

	r_view.frustum_x = tan(90 * M_PI / 360.0);
	r_view.frustum_y = tan(90 * M_PI / 360.0);

	buffer1 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3);
	buffer2 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3);
	buffer3 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3 + 18);

	for (j = 0;j < 12;j++)
	{
		sprintf(filename, "env/%s%s.tga", basename, envmapinfo[j].name);
		Matrix4x4_CreateFromQuakeEntity(&r_view.matrix, r_view.origin[0], r_view.origin[1], r_view.origin[2], envmapinfo[j].angles[0], envmapinfo[j].angles[1], envmapinfo[j].angles[2], 1);
		R_ClearScreen();
		R_Mesh_Start();
		R_RenderView();
		R_Mesh_Finish();
		SCR_ScreenShot(filename, buffer1, buffer2, buffer3, 0, vid.height - (r_view.y + r_view.height), size, size, envmapinfo[j].flipx, envmapinfo[j].flipy, envmapinfo[j].flipdiagonaly, false, false);
	}

	Mem_Free (buffer1);
	Mem_Free (buffer2);
	Mem_Free (buffer3);

	r_refdef.envmap = false;
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

	CHECKGLERROR
	qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer1);CHECKGLERROR

	if (scr_screenshot_gammaboost.value != 1 && gammacorrect)
	{
		int i;
		double igamma = 1.0 / scr_screenshot_gammaboost.value;
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
	// clear to black
	CHECKGLERROR
	if (r_refdef.fogenabled)
	{
		qglClearColor(r_refdef.fogcolor[0],r_refdef.fogcolor[1],r_refdef.fogcolor[2],0);CHECKGLERROR
	}
	else
	{
		qglClearColor(0,0,0,0);CHECKGLERROR
	}
	qglClearDepth(1);CHECKGLERROR
	if (gl_stencil)
	{
		// LordHavoc: we use a stencil centered around 128 instead of 0,
		// to avoid clamping interfering with strange shadow volume
		// drawing orders
		qglClearStencil(128);CHECKGLERROR
	}
	// clear the screen
	if (r_render.integer)
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

qboolean CL_VM_UpdateView (void);
void SCR_DrawConsole (void);
void R_Shadow_EditLights_DrawSelectedLightProperties(void);

int r_stereo_side;

void SCR_DrawScreen (void)
{
	R_Mesh_Start();

	if (r_timereport_active)
		R_TimeReport("setup");

	R_UpdateVariables();

	if (cls.signon == SIGNONS)
	{
		float size;

		size = scr_viewsize.value * (1.0 / 100.0);
		size = min(size, 1);

		if (r_stereo_sidebyside.integer)
		{
			r_view.width = (int)(vid.width * size / 2.5);
			r_view.height = (int)(vid.height * size / 2.5 * (1 - bound(0, r_letterbox.value, 100) / 100));
			r_view.depth = 1;
			r_view.x = (int)((vid.width - r_view.width * 2.5) * 0.5);
			r_view.y = (int)((vid.height - r_view.height)/2);
			r_view.z = 0;
			if (r_stereo_side)
				r_view.x += (int)(r_view.width * 1.5);
		}
		else
		{
			r_view.width = (int)(vid.width * size);
			r_view.height = (int)(vid.height * size * (1 - bound(0, r_letterbox.value, 100) / 100));
			r_view.depth = 1;
			r_view.x = (int)((vid.width - r_view.width)/2);
			r_view.y = (int)((vid.height - r_view.height)/2);
			r_view.z = 0;
		}

		// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
		// LordHavoc: this is designed to produce widescreen fov values
		// when the screen is wider than 4/3 width/height aspect, to do
		// this it simply assumes the requested fov is the vertical fov
		// for a 4x3 display, if the ratio is not 4x3 this makes the fov
		// higher/lower according to the ratio
		r_view.frustum_y = tan(scr_fov.value * cl.viewzoom * M_PI / 360.0) * (3.0/4.0);
		r_view.frustum_x = r_view.frustum_y * (float)r_view.width / (float)r_view.height / vid_pixelheight.value;

		r_view.frustum_x *= r_refdef.frustumscale_x;
		r_view.frustum_y *= r_refdef.frustumscale_y;

		if(!CL_VM_UpdateView())
			R_RenderView();

		if (scr_zoomwindow.integer)
		{
			float sizex = bound(10, scr_zoomwindow_viewsizex.value, 100) / 100.0;
			float sizey = bound(10, scr_zoomwindow_viewsizey.value, 100) / 100.0;
			r_view.width = (int)(vid.width * sizex);
			r_view.height = (int)(vid.height * sizey);
			r_view.depth = 1;
			r_view.x = (int)((vid.width - r_view.width)/2);
			r_view.y = 0;
			r_view.z = 0;

			r_view.frustum_y = tan(scr_zoomwindow_fov.value * cl.viewzoom * M_PI / 360.0) * (3.0/4.0);
			r_view.frustum_x = r_view.frustum_y * vid_pixelheight.value * (float)r_view.width / (float)r_view.height;

			r_view.frustum_x *= r_refdef.frustumscale_x;
			r_view.frustum_y *= r_refdef.frustumscale_y;

			if(!CL_VM_UpdateView())
				R_RenderView();
		}
	}

	if (!r_stereo_sidebyside.integer)
	{
		r_view.width = vid.width;
		r_view.height = vid.height;
		r_view.depth = 1;
		r_view.x = 0;
		r_view.y = 0;
		r_view.z = 0;
	}

	// draw 2D stuff

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

void SCR_UpdateLoadingScreen (qboolean clear)
{
	float x, y;
	cachepic_t *pic;
	float vertex3f[12];
	float texcoord2f[8];
	// don't do anything if not initialized yet
	if (vid_hidden || !scr_refresh.integer)
		return;
	CHECKGLERROR
	qglViewport(0, 0, vid.width, vid.height);CHECKGLERROR
	//qglDisable(GL_SCISSOR_TEST);CHECKGLERROR
	//qglDepthMask(1);CHECKGLERROR
	qglColorMask(1,1,1,1);CHECKGLERROR
	qglClearColor(0,0,0,0);CHECKGLERROR
	// when starting up a new video mode, make sure the screen is cleared to black
	if (clear)
	{
		qglClear(GL_COLOR_BUFFER_BIT);CHECKGLERROR
	}
	//qglDisable(GL_CULL_FACE);CHECKGLERROR
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
	R_Mesh_VertexPointer(vertex3f);
	R_Mesh_ColorPointer(NULL);
	R_Mesh_ResetTextureState();
	R_Mesh_TexBind(0, R_GetTexture(pic->tex));
	R_Mesh_TexCoordPointer(0, 2, texcoord2f);
	vertex3f[2] = vertex3f[5] = vertex3f[8] = vertex3f[11] = 0;
	vertex3f[0] = vertex3f[9] = x;
	vertex3f[1] = vertex3f[4] = y;
	vertex3f[3] = vertex3f[6] = x + pic->width;
	vertex3f[7] = vertex3f[10] = y + pic->height;
	texcoord2f[0] = 0;texcoord2f[1] = 0;
	texcoord2f[2] = 1;texcoord2f[3] = 0;
	texcoord2f[4] = 1;texcoord2f[5] = 1;
	texcoord2f[6] = 0;texcoord2f[7] = 1;
	if (vid.stereobuffer)
	{
		qglDrawBuffer(GL_FRONT_LEFT);
		R_Mesh_Draw(0, 4, 2, polygonelements);
		qglDrawBuffer(GL_FRONT_RIGHT);
		R_Mesh_Draw(0, 4, 2, polygonelements);
	}
	else
	{
		qglDrawBuffer(GL_FRONT);
		R_Mesh_Draw(0, 4, 2, polygonelements);
	}
	R_Mesh_Finish();
	// refresh
	// not necessary when rendering to GL_FRONT buffers
	//VID_Finish(false);
}

void CL_UpdateScreen(void)
{
	float conwidth, conheight;

	if (vid_hidden || !scr_refresh.integer)
		return;

	if (!scr_initialized || !con_initialized)
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

	r_view.colormask[0] = 1;
	r_view.colormask[1] = 1;
	r_view.colormask[2] = 1;

	if (r_timereport_active)
		R_TimeReport("other");

	SCR_SetUpToDrawConsole();

	if (r_timereport_active)
		R_TimeReport("start");

	CHECKGLERROR
	qglViewport(0, 0, vid.width, vid.height);CHECKGLERROR
	qglDisable(GL_SCISSOR_TEST);CHECKGLERROR
	qglDepthMask(1);CHECKGLERROR
	qglColorMask(1,1,1,1);CHECKGLERROR
	qglClearColor(0,0,0,0);CHECKGLERROR
	qglClear(GL_COLOR_BUFFER_BIT);CHECKGLERROR

	if(scr_stipple.integer)
	{
		GLubyte stipple[128];
		int i, s, width, parts;
		static int frame = 0;
		++frame;

		s = scr_stipple.integer;
		parts = (s & 007);
		width = (s & 070) >> 3;

		qglEnable(GL_POLYGON_STIPPLE); // 0x0B42
		for(i = 0; i < 128; ++i)
		{
			int line = i/4;
			stipple[i] = (((line >> width) + frame) & ((1 << parts) - 1)) ? 0x00 : 0xFF;
		}
		qglPolygonStipple(stipple);
	}
	else
		qglDisable(GL_POLYGON_STIPPLE);

	if (r_timereport_active)
		R_TimeReport("clear");

	if (vid.stereobuffer || r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer || r_stereo_sidebyside.integer)
	{
		matrix4x4_t originalmatrix = r_view.matrix;
		matrix4x4_t offsetmatrix;
		Matrix4x4_CreateTranslate(&offsetmatrix, 0, r_stereo_separation.value * -0.5f, 0);
		Matrix4x4_Concat(&r_view.matrix, &originalmatrix, &offsetmatrix);

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 0;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_view.colormask[0] = 1;
			r_view.colormask[1] = 0;
			r_view.colormask[2] = 0;
		}

		if (vid.stereobuffer)
			qglDrawBuffer(GL_BACK_RIGHT);

		SCR_DrawScreen();

		Matrix4x4_CreateTranslate(&offsetmatrix, 0, r_stereo_separation.value * 0.5f, 0);
		Matrix4x4_Concat(&r_view.matrix, &originalmatrix, &offsetmatrix);

		if (r_stereo_sidebyside.integer)
			r_stereo_side = 1;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_view.colormask[0] = 0;
			r_view.colormask[1] = r_stereo_redcyan.integer || r_stereo_redgreen.integer;
			r_view.colormask[2] = r_stereo_redcyan.integer || r_stereo_redblue.integer;
		}

		if (vid.stereobuffer)
			qglDrawBuffer(GL_BACK_LEFT);

		SCR_DrawScreen();

		r_view.matrix = originalmatrix;
	}
	else
	{
		qglDrawBuffer(GL_BACK);
		SCR_DrawScreen();
	}

	SCR_CaptureVideo();

	VID_Finish(true);
	if (r_timereport_active)
		R_TimeReport("finish");
}

void CL_Screen_NewMap(void)
{
	SHOWLMP_clear();
}
