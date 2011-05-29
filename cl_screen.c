
#include "quakedef.h"
#include "cl_video.h"
#include "image.h"
#include "jpeg.h"
#include "image_png.h"
#include "cl_collision.h"
#include "libcurl.h"
#include "csprogs.h"
#include "cap_avi.h"
#include "cap_ogg.h"

// we have to include snd_main.h here only to get access to snd_renderbuffer->format.speed when writing the AVI headers
#include "snd_main.h"

cvar_t scr_viewsize = {CVAR_SAVE, "viewsize","100", "how large the view should be, 110 disables inventory bar, 120 disables status bar"};
cvar_t scr_fov = {CVAR_SAVE, "fov","90", "field of vision, 1-170 degrees, default 90, some players use 110-130"};
cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1", "opacity of console background gfx/conback"};
cvar_t scr_conalphafactor = {CVAR_SAVE, "scr_conalphafactor", "1", "opacity of console background gfx/conback relative to scr_conalpha; when 0, gfx/conback is not drawn"};
cvar_t scr_conalpha2factor = {CVAR_SAVE, "scr_conalpha2factor", "0", "opacity of console background gfx/conback2 relative to scr_conalpha; when 0, gfx/conback2 is not drawn"};
cvar_t scr_conalpha3factor = {CVAR_SAVE, "scr_conalpha3factor", "0", "opacity of console background gfx/conback3 relative to scr_conalpha; when 0, gfx/conback3 is not drawn"};
cvar_t scr_conbrightness = {CVAR_SAVE, "scr_conbrightness", "1", "brightness of console background (0 = black, 1 = image)"};
cvar_t scr_conforcewhiledisconnected = {0, "scr_conforcewhiledisconnected", "1", "forces fullscreen console while disconnected"};
cvar_t scr_conscroll_x = {CVAR_SAVE, "scr_conscroll_x", "0", "scroll speed of gfx/conback in x direction"};
cvar_t scr_conscroll_y = {CVAR_SAVE, "scr_conscroll_y", "0", "scroll speed of gfx/conback in y direction"};
cvar_t scr_conscroll2_x = {CVAR_SAVE, "scr_conscroll2_x", "0", "scroll speed of gfx/conback2 in x direction"};
cvar_t scr_conscroll2_y = {CVAR_SAVE, "scr_conscroll2_y", "0", "scroll speed of gfx/conback2 in y direction"};
cvar_t scr_conscroll3_x = {CVAR_SAVE, "scr_conscroll3_x", "0", "scroll speed of gfx/conback3 in x direction"};
cvar_t scr_conscroll3_y = {CVAR_SAVE, "scr_conscroll3_y", "0", "scroll speed of gfx/conback3 in y direction"};
cvar_t scr_menuforcewhiledisconnected = {0, "scr_menuforcewhiledisconnected", "0", "forces menu while disconnected"};
cvar_t scr_centertime = {0, "scr_centertime","2", "how long centerprint messages show"};
cvar_t scr_showram = {CVAR_SAVE, "showram","1", "show ram icon if low on surface cache memory (not used)"};
cvar_t scr_showturtle = {CVAR_SAVE, "showturtle","0", "show turtle icon when framerate is too low"};
cvar_t scr_showpause = {CVAR_SAVE, "showpause","1", "show pause icon when game is paused"};
cvar_t scr_showbrand = {0, "showbrand","0", "shows gfx/brand.tga in a corner of the screen (different values select different positions, including centered)"};
cvar_t scr_printspeed = {0, "scr_printspeed","0", "speed of intermission printing (episode end texts), a value of 0 disables the slow printing"};
cvar_t scr_loadingscreen_background = {0, "scr_loadingscreen_background","0", "show the last visible background during loading screen (costs one screenful of video memory)"};
cvar_t scr_loadingscreen_scale = {0, "scr_loadingscreen_scale","1", "scale factor of the background"};
cvar_t scr_loadingscreen_scale_base = {0, "scr_loadingscreen_scale_base","0", "0 = console pixels, 1 = video pixels"};
cvar_t scr_loadingscreen_scale_limit = {0, "scr_loadingscreen_scale_limit","0", "0 = no limit, 1 = until first edge hits screen edge, 2 = until last edge hits screen edge, 3 = until width hits screen width, 4 = until height hits screen height"};
cvar_t scr_loadingscreen_count = {0, "scr_loadingscreen_count","1", "number of loading screen files to use randomly (named loading.tga, loading2.tga, loading3.tga, ...)"};
cvar_t scr_loadingscreen_barcolor = {0, "scr_loadingscreen_barcolor", "0 0 1", "rgb color of loadingscreen progress bar"};
cvar_t scr_loadingscreen_barheight = {0, "scr_loadingscreen_barheight", "8", "the height of the loadingscreen progress bar"};
cvar_t scr_infobar_height = {0, "scr_infobar_height", "8", "the height of the infobar items"};
cvar_t vid_conwidth = {CVAR_SAVE, "vid_conwidth", "640", "virtual width of 2D graphics system"};
cvar_t vid_conheight = {CVAR_SAVE, "vid_conheight", "480", "virtual height of 2D graphics system"};
cvar_t vid_pixelheight = {CVAR_SAVE, "vid_pixelheight", "1", "adjusts vertical field of vision to account for non-square pixels (1280x1024 on a CRT monitor for example)"};
cvar_t scr_screenshot_jpeg = {CVAR_SAVE, "scr_screenshot_jpeg","1", "save jpeg instead of targa"};
cvar_t scr_screenshot_jpeg_quality = {CVAR_SAVE, "scr_screenshot_jpeg_quality","0.9", "image quality of saved jpeg"};
cvar_t scr_screenshot_png = {CVAR_SAVE, "scr_screenshot_png","0", "save png instead of targa"};
cvar_t scr_screenshot_gammaboost = {CVAR_SAVE, "scr_screenshot_gammaboost","1", "gamma correction on saved screenshots and videos, 1.0 saves unmodified images"};
cvar_t scr_screenshot_hwgamma = {CVAR_SAVE, "scr_screenshot_hwgamma","1", "apply the video gamma ramp to saved screenshots and videos"};
cvar_t scr_screenshot_alpha = {CVAR_SAVE, "scr_screenshot_alpha","0", "try to write an alpha channel to screenshots (debugging feature)"};
// scr_screenshot_name is defined in fs.c
cvar_t cl_capturevideo = {0, "cl_capturevideo", "0", "enables saving of video to a .avi file using uncompressed I420 colorspace and PCM audio, note that scr_screenshot_gammaboost affects the brightness of the output)"};
cvar_t cl_capturevideo_printfps = {CVAR_SAVE, "cl_capturevideo_printfps", "1", "prints the frames per second captured in capturevideo (is only written to the log file, not to the console, as that would be visible on the video)"};
cvar_t cl_capturevideo_width = {CVAR_SAVE, "cl_capturevideo_width", "0", "scales all frames to this resolution before saving the video"};
cvar_t cl_capturevideo_height = {CVAR_SAVE, "cl_capturevideo_height", "0", "scales all frames to this resolution before saving the video"};
cvar_t cl_capturevideo_realtime = {0, "cl_capturevideo_realtime", "0", "causes video saving to operate in realtime (mostly useful while playing, not while capturing demos), this can produce a much lower quality video due to poor sound/video sync and will abort saving if your machine stalls for over a minute"};
cvar_t cl_capturevideo_fps = {CVAR_SAVE, "cl_capturevideo_fps", "30", "how many frames per second to save (29.97 for NTSC, 30 for typical PC video, 15 can be useful)"};
cvar_t cl_capturevideo_nameformat = {CVAR_SAVE, "cl_capturevideo_nameformat", "dpvideo", "prefix for saved videos (the date is encoded using strftime escapes)"};
cvar_t cl_capturevideo_number = {CVAR_SAVE, "cl_capturevideo_number", "1", "number to append to video filename, incremented each time a capture begins"};
cvar_t cl_capturevideo_ogg = {CVAR_SAVE, "cl_capturevideo_ogg", "1", "save captured video data as Ogg/Vorbis/Theora streams"};
cvar_t cl_capturevideo_framestep = {CVAR_SAVE, "cl_capturevideo_framestep", "1", "when set to n >= 1, render n frames to capture one (useful for motion blur like effects)"};
cvar_t r_letterbox = {0, "r_letterbox", "0", "reduces vertical height of view to simulate a letterboxed movie effect (can be used by mods for cutscenes)"};
cvar_t r_stereo_separation = {0, "r_stereo_separation", "4", "separation distance of eyes in the world (negative values are only useful for cross-eyed viewing)"};
cvar_t r_stereo_sidebyside = {0, "r_stereo_sidebyside", "0", "side by side views for those who can't afford glasses but can afford eye strain (note: use a negative r_stereo_separation if you want cross-eyed viewing)"};
cvar_t r_stereo_horizontal = {0, "r_stereo_horizontal", "0", "aspect skewed side by side view for special decoder/display hardware"};
cvar_t r_stereo_vertical = {0, "r_stereo_vertical", "0", "aspect skewed top and bottom view for special decoder/display hardware"};
cvar_t r_stereo_redblue = {0, "r_stereo_redblue", "0", "red/blue anaglyph stereo glasses (note: most of these glasses are actually red/cyan, try that one too)"};
cvar_t r_stereo_redcyan = {0, "r_stereo_redcyan", "0", "red/cyan anaglyph stereo glasses, the kind given away at drive-in movies like Creature From The Black Lagoon In 3D"};
cvar_t r_stereo_redgreen = {0, "r_stereo_redgreen", "0", "red/green anaglyph stereo glasses (for those who don't mind yellow)"};
cvar_t r_stereo_angle = {0, "r_stereo_angle", "0", "separation angle of eyes (makes the views look different directions, as an example, 90 gives a 90 degree separation where the views are 45 degrees left and 45 degrees right)"};
cvar_t scr_zoomwindow = {CVAR_SAVE, "scr_zoomwindow", "0", "displays a zoomed in overlay window"};
cvar_t scr_zoomwindow_viewsizex = {CVAR_SAVE, "scr_zoomwindow_viewsizex", "20", "horizontal viewsize of zoom window"};
cvar_t scr_zoomwindow_viewsizey = {CVAR_SAVE, "scr_zoomwindow_viewsizey", "20", "vertical viewsize of zoom window"};
cvar_t scr_zoomwindow_fov = {CVAR_SAVE, "scr_zoomwindow_fov", "20", "fov of zoom window"};
cvar_t scr_stipple = {0, "scr_stipple", "0", "interlacing-like stippling of the display"};
cvar_t scr_refresh = {0, "scr_refresh", "1", "allows you to completely shut off rendering for benchmarking purposes"};
cvar_t scr_screenshot_name_in_mapdir = {CVAR_SAVE, "scr_screenshot_name_in_mapdir", "0", "if set to 1, screenshots are placed in a subdirectory named like the map they are from"};
cvar_t shownetgraph = {CVAR_SAVE, "shownetgraph", "0", "shows a graph of packet sizes and other information, 0 = off, 1 = show client netgraph, 2 = show client and server netgraphs (when hosting a server)"};
cvar_t cl_demo_mousegrab = {0, "cl_demo_mousegrab", "0", "Allows reading the mouse input while playing demos. Useful for camera mods developed in csqc. (0: never, 1: always)"};
cvar_t timedemo_screenshotframelist = {0, "timedemo_screenshotframelist", "", "when performing a timedemo, take screenshots of each frame in this space-separated list - example: 1 201 401"};
cvar_t vid_touchscreen_outlinealpha = {0, "vid_touchscreen_outlinealpha", "0.25", "opacity of touchscreen area outlines"};
cvar_t vid_touchscreen_overlayalpha = {0, "vid_touchscreen_overlayalpha", "0.25", "opacity of touchscreen area icons"};

extern cvar_t v_glslgamma;
extern cvar_t sbar_info_pos;
extern cvar_t r_fog_clear;
#define WANT_SCREENSHOT_HWGAMMA (scr_screenshot_hwgamma.integer && vid_usinghwgamma)

int jpeg_supported = false;

qboolean	scr_initialized;		// ready to draw

float		scr_con_current;
int			scr_con_margin_bottom;

extern int	con_vislines;

static void SCR_ScreenShot_f (void);
static void R_Envmap_f (void);

// backend
void R_ClearScreen(qboolean fogcolor);

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
char        scr_infobarstring[MAX_INPUTLINE];
float       scr_infobartime_off;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint(const char *str)
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
	int		x, y;
	int		remaining;
	int		color;

	if(cl.intermission == 2) // in finale,
		if(sb_showscores) // make TAB hide the finale message (sb_showscores overrides finale in sbar.c)
			return;

	if(scr_centertime.value <= 0 && !cl.intermission)
		return;

// the finale prints the characters one at a time, except if printspeed is an absurdly high value
	if (cl.intermission && scr_printspeed.value > 0 && scr_printspeed.value < 1000000)
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
		char *newline = strchr(start, '\n');
		int l = newline ? (newline - start) : (int)strlen(start);
		float width = DrawQ_TextWidth(start, l, 8, 8, false, FONT_CENTERPRINT);

		x = (int) (vid_conwidth.integer - width)/2;
		if (l > 0)
		{
			if (remaining < l)
				l = remaining;
			DrawQ_String(x, y, start, l, 8, 8, 1, 1, 1, 1, 0, &color, false, FONT_CENTERPRINT);
			remaining -= l;
			if (remaining <= 0)
				return;
		}
		y += 8;

		if (!newline)
			break;
		start = newline + 1; // skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	if (cl.time > cl.oldtime)
		scr_centertime_off -= cl.time - cl.oldtime;

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

void SCR_DrawNetGraph_DrawGraph (int graphx, int graphy, int graphwidth, int graphheight, float graphscale, const char *label, float textsize, int packetcounter, netgraphitem_t *netgraph)
{
	netgraphitem_t *graph;
	int j, x, y, numlines;
	int totalbytes = 0;
	char bytesstring[128];
	float g[NETGRAPH_PACKETS][6];
	float *a;
	float *b;
	float vertex3f[(NETGRAPH_PACKETS+2)*5*2*3];
	float color4f[(NETGRAPH_PACKETS+2)*5*2*4];
	float *v;
	float *c;
	DrawQ_Fill(graphx, graphy, graphwidth, graphheight + textsize * 2, 0, 0, 0, 0.5, 0);
	// draw the bar graph itself
	// advance the packet counter because it is the latest packet column being
	// built up and should come last
	packetcounter = (packetcounter + 1) % NETGRAPH_PACKETS;
	memset(g, 0, sizeof(g));
	for (j = 0;j < NETGRAPH_PACKETS;j++)
	{
		graph = netgraph + j;
		g[j][0] = 1.0f - 0.25f * (realtime - graph->time);
		g[j][1] = 1.0f;
		g[j][2] = 1.0f;
		g[j][3] = 1.0f;
		g[j][4] = 1.0f;
		g[j][5] = 1.0f;
		if (graph->unreliablebytes == NETGRAPH_LOSTPACKET)
			g[j][1] = 0.00f;
		else if (graph->unreliablebytes == NETGRAPH_CHOKEDPACKET)
			g[j][2] = 0.96f;
		else
		{
			g[j][3] = 1.0f    - graph->unreliablebytes * graphscale;
			g[j][4] = g[j][3] - graph->reliablebytes   * graphscale;
			g[j][5] = g[j][4] - graph->ackbytes        * graphscale;
			// count bytes in the last second
			if (realtime - graph->time < 1.0f)
				totalbytes += graph->unreliablebytes + graph->reliablebytes + graph->ackbytes;
		}
		g[j][1] = bound(0.0f, g[j][1], 1.0f);
		g[j][2] = bound(0.0f, g[j][2], 1.0f);
		g[j][3] = bound(0.0f, g[j][3], 1.0f);
		g[j][4] = bound(0.0f, g[j][4], 1.0f);
		g[j][5] = bound(0.0f, g[j][5], 1.0f);
	}
	// render the lines for the graph
	numlines = 0;
	v = vertex3f;
	c = color4f;
	for (j = 0;j < NETGRAPH_PACKETS;j++)
	{
		a = g[j];
		b = g[(j+1)%NETGRAPH_PACKETS];
		if (a[0] < 0.0f || b[0] > 1.0f || b[0] < a[0])
			continue;
		VectorSet(v, graphx + graphwidth * a[0], graphy + graphheight * a[2], 0.0f);v += 3;Vector4Set(c, 1.0f, 1.0f, 0.0f, 1.0f);c += 4;
		VectorSet(v, graphx + graphwidth * b[0], graphy + graphheight * b[2], 0.0f);v += 3;Vector4Set(c, 1.0f, 1.0f, 0.0f, 1.0f);c += 4;

		VectorSet(v, graphx + graphwidth * a[0], graphy + graphheight * a[1], 0.0f);v += 3;Vector4Set(c, 1.0f, 0.0f, 0.0f, 1.0f);c += 4;
		VectorSet(v, graphx + graphwidth * b[0], graphy + graphheight * b[1], 0.0f);v += 3;Vector4Set(c, 1.0f, 0.0f, 0.0f, 1.0f);c += 4;

		VectorSet(v, graphx + graphwidth * a[0], graphy + graphheight * a[5], 0.0f);v += 3;Vector4Set(c, 0.0f, 1.0f, 0.0f, 1.0f);c += 4;
		VectorSet(v, graphx + graphwidth * b[0], graphy + graphheight * b[5], 0.0f);v += 3;Vector4Set(c, 0.0f, 1.0f, 0.0f, 1.0f);c += 4;

		VectorSet(v, graphx + graphwidth * a[0], graphy + graphheight * a[4], 0.0f);v += 3;Vector4Set(c, 1.0f, 1.0f, 1.0f, 1.0f);c += 4;
		VectorSet(v, graphx + graphwidth * b[0], graphy + graphheight * b[4], 0.0f);v += 3;Vector4Set(c, 1.0f, 1.0f, 1.0f, 1.0f);c += 4;

		VectorSet(v, graphx + graphwidth * a[0], graphy + graphheight * a[3], 0.0f);v += 3;Vector4Set(c, 1.0f, 0.5f, 0.0f, 1.0f);c += 4;
		VectorSet(v, graphx + graphwidth * b[0], graphy + graphheight * b[3], 0.0f);v += 3;Vector4Set(c, 1.0f, 0.5f, 0.0f, 1.0f);c += 4;

		numlines += 5;
	}
	if (numlines > 0)
		DrawQ_Lines(0.0f, numlines, vertex3f, color4f, 0);
	x = graphx;
	y = graphy + graphheight;
	dpsnprintf(bytesstring, sizeof(bytesstring), "%i", totalbytes);
	DrawQ_String(x, y, label      , 0, textsize, textsize, 1.0f, 1.0f, 1.0f, 1.0f, 0, NULL, false, FONT_DEFAULT);y += textsize;
	DrawQ_String(x, y, bytesstring, 0, textsize, textsize, 1.0f, 1.0f, 1.0f, 1.0f, 0, NULL, false, FONT_DEFAULT);y += textsize;
}

/*
==============
SCR_DrawNetGraph
==============
*/
void SCR_DrawNetGraph (void)
{
	int i, separator1, separator2, graphwidth, graphheight, netgraph_x, netgraph_y, textsize, index, netgraphsperrow;
	float graphscale;
	netconn_t *c;

	if (cls.state != ca_connected)
		return;
	if (!cls.netcon)
		return;
	if (!shownetgraph.integer)
		return;

	separator1 = 2;
	separator2 = 4;
	textsize = 8;
	graphwidth = 120;
	graphheight = 70;
	graphscale = 1.0f / 1500.0f;

	netgraphsperrow = (vid_conwidth.integer + separator2) / (graphwidth * 2 + separator1 + separator2);
	netgraphsperrow = max(netgraphsperrow, 1);

	index = 0;
	netgraph_x = (vid_conwidth.integer + separator2) - (1 + (index % netgraphsperrow)) * (graphwidth * 2 + separator1 + separator2);
	netgraph_y = (vid_conheight.integer - 48 - sbar_info_pos.integer + separator2) - (1 + (index / netgraphsperrow)) * (graphheight + textsize + separator2);
	c = cls.netcon;
	SCR_DrawNetGraph_DrawGraph(netgraph_x                          , netgraph_y, graphwidth, graphheight, graphscale, "incoming", textsize, c->incoming_packetcounter, c->incoming_netgraph);
	SCR_DrawNetGraph_DrawGraph(netgraph_x + graphwidth + separator1, netgraph_y, graphwidth, graphheight, graphscale, "outgoing", textsize, c->outgoing_packetcounter, c->outgoing_netgraph);
	index++;

	if (sv.active && shownetgraph.integer >= 2)
	{
		for (i = 0;i < svs.maxclients;i++)
		{
			c = svs.clients[i].netconnection;
			if (!c)
				continue;
			netgraph_x = (vid_conwidth.integer + separator2) - (1 + (index % netgraphsperrow)) * (graphwidth * 2 + separator1 + separator2);
			netgraph_y = (vid_conheight.integer - 48 + separator2) - (1 + (index / netgraphsperrow)) * (graphheight + textsize + separator2);
			SCR_DrawNetGraph_DrawGraph(netgraph_x                          , netgraph_y, graphwidth, graphheight, graphscale, va("%s", svs.clients[i].name), textsize, c->outgoing_packetcounter, c->outgoing_netgraph);
			SCR_DrawNetGraph_DrawGraph(netgraph_x + graphwidth + separator1, netgraph_y, graphwidth, graphheight, graphscale, ""                           , textsize, c->incoming_packetcounter, c->incoming_netgraph);
			index++;
		}
	}
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

	DrawQ_Pic (0, 0, Draw_CachePic ("gfx/turtle"), 0, 0, 1, 1, 1, 1, 0);
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

	DrawQ_Pic (64, 0, Draw_CachePic ("gfx/net"), 0, 0, 1, 1, 1, 1, 0);
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

	pic = Draw_CachePic ("gfx/pause");
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

	pic = Draw_CachePic ("gfx/brand");

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
	// sync with SCR_InfobarHeight
	int len;
	float x, y;
	float size = scr_infobar_height.value;
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
		dpsnprintf(temp, sizeof(temp), "Downloading %s %3i%% (%i) at %i bytes/s", cls.qw_downloadname, cls.qw_downloadpercent, cls.qw_downloadmemorycursize, cls.qw_downloadspeedrate);
	else
		dpsnprintf(temp, sizeof(temp), "Downloading %s %3i%% (%i/%i) at %i bytes/s", cls.qw_downloadname, cls.qw_downloadpercent, cls.qw_downloadmemorycursize, cls.qw_downloadmemorymaxsize, cls.qw_downloadspeedrate);
	len = (int)strlen(temp);
	x = (vid_conwidth.integer - DrawQ_TextWidth(temp, len, size, size, true, FONT_INFOBAR)) / 2;
	y = vid_conheight.integer - size - offset;
	DrawQ_Fill(0, y, vid_conwidth.integer, size, 0, 0, 0, cls.signon == SIGNONS ? 0.5 : 1, 0);
	DrawQ_String(x, y, temp, len, size, size, 1, 1, 1, 1, 0, NULL, true, FONT_INFOBAR);
	return size;
}
/*
==============
SCR_DrawInfobarString
==============
*/
static int SCR_DrawInfobarString(int offset)
{
	int len;
	float x, y;
	float size = scr_infobar_height.value;

	len = (int)strlen(scr_infobarstring);
	x = (vid_conwidth.integer - DrawQ_TextWidth(scr_infobarstring, len, size, size, false, FONT_INFOBAR)) / 2;
	y = vid_conheight.integer - size - offset;
	DrawQ_Fill(0, y, vid_conwidth.integer, size, 0, 0, 0, cls.signon == SIGNONS ? 0.5 : 1, 0);
	DrawQ_String(x, y, scr_infobarstring, len, size, size, 1, 1, 1, 1, 0, NULL, false, FONT_INFOBAR);
	return size;
}

/*
==============
SCR_DrawCurlDownload
==============
*/
static int SCR_DrawCurlDownload(int offset)
{
	// sync with SCR_InfobarHeight
	int len;
	int nDownloads;
	int i;
	float x, y;
	float size = scr_infobar_height.value;
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
		x = (vid_conwidth.integer - DrawQ_TextWidth(addinfo, len, size, size, true, FONT_INFOBAR)) / 2;
		DrawQ_Fill(0, y - size, vid_conwidth.integer, size, 1, 1, 1, cls.signon == SIGNONS ? 0.8 : 1, 0);
		DrawQ_String(x, y - size, addinfo, len, size, size, 0, 0, 0, 1, 0, NULL, true, FONT_INFOBAR);
	}

	for(i = 0; i != nDownloads; ++i)
	{
		if(downinfo[i].queued)
			dpsnprintf(temp, sizeof(temp), "Still in queue: %s", downinfo[i].filename);
		else if(downinfo[i].progress <= 0)
			dpsnprintf(temp, sizeof(temp), "Downloading %s ...  ???.?%% @ %.1f KiB/s", downinfo[i].filename, downinfo[i].speed / 1024.0);
		else
			dpsnprintf(temp, sizeof(temp), "Downloading %s ...  %5.1f%% @ %.1f KiB/s", downinfo[i].filename, 100.0 * downinfo[i].progress, downinfo[i].speed / 1024.0);
		len = (int)strlen(temp);
		x = (vid_conwidth.integer - DrawQ_TextWidth(temp, len, size, size, true, FONT_INFOBAR)) / 2;
		DrawQ_Fill(0, y + i * size, vid_conwidth.integer, size, 0, 0, 0, cls.signon == SIGNONS ? 0.5 : 1, 0);
		DrawQ_String(x, y + i * size, temp, len, size, size, 1, 1, 1, 1, 0, NULL, true, FONT_INFOBAR);
	}

	Z_Free(downinfo);

	return size * (nDownloads + (addinfo ? 1 : 0));
}

/*
==============
SCR_DrawInfobar
==============
*/
static void SCR_DrawInfobar(void)
{
	int offset = 0;
	offset += SCR_DrawQWDownload(offset);
	offset += SCR_DrawCurlDownload(offset);
	if(scr_infobartime_off > 0)
		offset += SCR_DrawInfobarString(offset);
	if(offset != scr_con_margin_bottom)
		Con_DPrintf("broken console margin calculation: %d != %d\n", offset, scr_con_margin_bottom);
}

static int SCR_InfobarHeight(void)
{
	int offset = 0;
	Curl_downloadinfo_t *downinfo;
	const char *addinfo;
	int nDownloads;

	if (cl.time > cl.oldtime)
		scr_infobartime_off -= cl.time - cl.oldtime;
	if(scr_infobartime_off > 0)
		offset += 1;
	if(cls.qw_downloadname[0])
		offset += 1;

	downinfo = Curl_GetDownloadInfo(&nDownloads, &addinfo);
	if(downinfo)
	{
		offset += (nDownloads + (addinfo ? 1 : 0));
		Z_Free(downinfo);
	}
	offset *= scr_infobar_height.value;

	return offset;
}

/*
==============
SCR_InfoBar_f
==============
*/
void SCR_InfoBar_f(void)
{
	if(Cmd_Argc() == 3)
	{
		scr_infobartime_off = atof(Cmd_Argv(1));
		strlcpy(scr_infobarstring, Cmd_Argv(2), sizeof(scr_infobarstring));
	}
	else
	{
		Con_Printf("usage:\ninfobar expiretime \"string\"\n");
	}
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
			MR_ToggleMenu(1);
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
	scr_con_margin_bottom = SCR_InfobarHeight();
	if (key_consoleactive & KEY_CONSOLEACTIVE_FORCED)
	{
		// full screen
		Con_DrawConsole (vid_conheight.integer - scr_con_margin_bottom);
	}
	else if (scr_con_current)
		Con_DrawConsole (min((int)scr_con_current, vid_conheight.integer - scr_con_margin_bottom));
	else
		con_vislines = 0;
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
	SCR_UpdateLoadingScreen(false);
}

//=============================================================================

char r_speeds_timestring[4096];
int speedstringcount, r_timereport_active;
double r_timereport_temp = 0, r_timereport_current = 0, r_timereport_start = 0;
int r_speeds_longestitem = 0;

void R_TimeReport(const char *desc)
{
	char tempbuf[256];
	int length;
	int t;

	if (r_speeds.integer < 2 || !r_timereport_active)
		return;

	CHECKGLERROR
	if (r_speeds.integer == 2)
		GL_Finish();
	CHECKGLERROR
	r_timereport_temp = r_timereport_current;
	r_timereport_current = Sys_DoubleTime();
	t = (int) ((r_timereport_current - r_timereport_temp) * 1000000.0 + 0.5);

	length = dpsnprintf(tempbuf, sizeof(tempbuf), "%8i %s", t, desc);
	length = min(length, (int)sizeof(tempbuf) - 1);
	if (r_speeds_longestitem < length)
		r_speeds_longestitem = length;
	for (;length < r_speeds_longestitem;length++)
		tempbuf[length] = ' ';
	tempbuf[length] = 0;

	if (speedstringcount + length > (vid_conwidth.integer / 8))
	{
		strlcat(r_speeds_timestring, "\n", sizeof(r_speeds_timestring));
		speedstringcount = 0;
	}
	strlcat(r_speeds_timestring, tempbuf, sizeof(r_speeds_timestring));
	speedstringcount += length;
}

void R_TimeReport_BeginFrame(void)
{
	speedstringcount = 0;
	r_speeds_timestring[0] = 0;
	r_timereport_active = false;
	memset(&r_refdef.stats, 0, sizeof(r_refdef.stats));

	if (r_speeds.integer >= 2)
	{
		r_timereport_active = true;
		r_timereport_start = r_timereport_current = Sys_DoubleTime();
	}
}

static int R_CountLeafTriangles(const dp_model_t *model, const mleaf_t *leaf)
{
	int i, triangles = 0;
	for (i = 0;i < leaf->numleafsurfaces;i++)
		triangles += model->data_surfaces[leaf->firstleafsurface[i]].num_triangles;
	return triangles;
}

extern cvar_t r_viewscale;
extern float viewscalefpsadjusted;
void R_TimeReport_EndFrame(void)
{
	int i, j, lines, y;
	cl_locnode_t *loc;
	char string[1024+4096];
	mleaf_t *viewleaf;

	string[0] = 0;
	if (r_speeds.integer)
	{
		// put the location name in the r_speeds display as it greatly helps
		// when creating loc files
		loc = CL_Locs_FindNearest(cl.movement_origin);
		viewleaf = (r_refdef.scene.worldmodel && r_refdef.scene.worldmodel->brush.PointInLeaf) ? r_refdef.scene.worldmodel->brush.PointInLeaf(r_refdef.scene.worldmodel, r_refdef.view.origin) : NULL;
		dpsnprintf(string, sizeof(string),
"%6.0fus rendertime %3.0f%% viewscale %s%s %.3f cl.time\n"
"%3i renders org:'%+8.2f %+8.2f %+8.2f' dir:'%+2.3f %+2.3f %+2.3f'\n"
"%5i viewleaf%5i cluster%3i area%4i brushes%4i surfaces(%7i triangles)\n"
"%7i surfaces%7i triangles %5i entities (%7i surfaces%7i triangles)\n"
"%5i leafs%5i portals%6i/%6i particles%6i/%6i decals %3i%% quality\n"
"%7i lightmap updates (%7i pixels)%8iKB/%8iKB framedata\n"
"%4i lights%4i clears%4i scissored%7i light%7i shadow%7i dynamic\n"
"bouncegrid:%4i lights%6i particles%6i traces%6i hits%6i splats%6i bounces\n"
"collision cache efficiency:%6i cached%6i traced%6ianimated\n"
"%6i draws%8i vertices%8i triangles bloompixels%8i copied%8i drawn\n"
"updated%5i indexbuffers%8i bytes%5i vertexbuffers%8i bytes\n"
"%s"
, r_refdef.lastdrawscreentime * 1000000.0, r_viewscale.value * sqrt(viewscalefpsadjusted) * 100.0f, loc ? "Location: " : "", loc ? loc->name : "", cl.time
, r_refdef.stats.renders, r_refdef.view.origin[0], r_refdef.view.origin[1], r_refdef.view.origin[2], r_refdef.view.forward[0], r_refdef.view.forward[1], r_refdef.view.forward[2]
, viewleaf ? (int)(viewleaf - r_refdef.scene.worldmodel->brush.data_leafs) : -1, viewleaf ? viewleaf->clusterindex : -1, viewleaf ? viewleaf->areaindex : -1, viewleaf ? viewleaf->numleafbrushes : 0, viewleaf ? viewleaf->numleafsurfaces : 0, viewleaf ? R_CountLeafTriangles(r_refdef.scene.worldmodel, viewleaf) : 0
, r_refdef.stats.world_surfaces, r_refdef.stats.world_triangles, r_refdef.stats.entities, r_refdef.stats.entities_surfaces, r_refdef.stats.entities_triangles
, r_refdef.stats.world_leafs, r_refdef.stats.world_portals, r_refdef.stats.particles, cl.num_particles, r_refdef.stats.drawndecals, r_refdef.stats.totaldecals, (int)(100 * r_refdef.view.quality)
, r_refdef.stats.lightmapupdates, r_refdef.stats.lightmapupdatepixels, (r_refdef.stats.framedatacurrent+512) / 1024, (r_refdef.stats.framedatasize+512)/1024
, r_refdef.stats.lights, r_refdef.stats.lights_clears, r_refdef.stats.lights_scissored, r_refdef.stats.lights_lighttriangles, r_refdef.stats.lights_shadowtriangles, r_refdef.stats.lights_dynamicshadowtriangles
, r_refdef.stats.bouncegrid_lights, r_refdef.stats.bouncegrid_particles, r_refdef.stats.bouncegrid_traces, r_refdef.stats.bouncegrid_hits, r_refdef.stats.bouncegrid_splats, r_refdef.stats.bouncegrid_bounces
, r_refdef.stats.collisioncache_cached, r_refdef.stats.collisioncache_traced, r_refdef.stats.collisioncache_animated
, r_refdef.stats.draws, r_refdef.stats.draws_vertices, r_refdef.stats.draws_elements / 3, r_refdef.stats.bloom_copypixels, r_refdef.stats.bloom_drawpixels
, r_refdef.stats.indexbufferuploadcount, r_refdef.stats.indexbufferuploadsize, r_refdef.stats.vertexbufferuploadcount, r_refdef.stats.vertexbufferuploadsize
, r_speeds_timestring);

		memset(&r_refdef.stats, 0, sizeof(r_refdef.stats));

		speedstringcount = 0;
		r_speeds_timestring[0] = 0;
		r_timereport_active = false;

		if (r_speeds.integer >= 2)
		{
			r_timereport_active = true;
			r_timereport_start = r_timereport_current = Sys_DoubleTime();
		}
	}

	if (string[0])
	{
		if (string[strlen(string)-1] == '\n')
			string[strlen(string)-1] = 0;
		lines = 1;
		for (i = 0;string[i];i++)
			if (string[i] == '\n')
				lines++;
		y = vid_conheight.integer - sb_lines - lines * 8;
		i = j = 0;
		r_draw2d_force = true;
		DrawQ_Fill(0, y, vid_conwidth.integer, lines * 8, 0, 0, 0, 0.5, 0);
		while (string[i])
		{
			j = i;
			while (string[i] && string[i] != '\n')
				i++;
			if (i - j > 0)
				DrawQ_String(0, y, string + j, i - j, 8, 8, 1, 1, 1, 1, 0, NULL, true, FONT_DEFAULT);
			if (string[i] == '\n')
				i++;
			y += 8;
		}
		r_draw2d_force = false;
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

void SCR_CaptureVideo_EndVideo(void);
void CL_Screen_Shutdown(void)
{
	SCR_CaptureVideo_EndVideo();
}

void CL_Screen_Init(void)
{
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conalpha);
	Cvar_RegisterVariable (&scr_conalphafactor);
	Cvar_RegisterVariable (&scr_conalpha2factor);
	Cvar_RegisterVariable (&scr_conalpha3factor);
	Cvar_RegisterVariable (&scr_conscroll_x);
	Cvar_RegisterVariable (&scr_conscroll_y);
	Cvar_RegisterVariable (&scr_conscroll2_x);
	Cvar_RegisterVariable (&scr_conscroll2_y);
	Cvar_RegisterVariable (&scr_conscroll3_x);
	Cvar_RegisterVariable (&scr_conscroll3_y);
	Cvar_RegisterVariable (&scr_conbrightness);
	Cvar_RegisterVariable (&scr_conforcewhiledisconnected);
	Cvar_RegisterVariable (&scr_menuforcewhiledisconnected);
	Cvar_RegisterVariable (&scr_loadingscreen_background);
	Cvar_RegisterVariable (&scr_loadingscreen_scale);
	Cvar_RegisterVariable (&scr_loadingscreen_scale_base);
	Cvar_RegisterVariable (&scr_loadingscreen_scale_limit);
	Cvar_RegisterVariable (&scr_loadingscreen_count);
	Cvar_RegisterVariable (&scr_loadingscreen_barcolor);
	Cvar_RegisterVariable (&scr_loadingscreen_barheight);
	Cvar_RegisterVariable (&scr_infobar_height);
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
	Cvar_RegisterVariable (&scr_screenshot_png);
	Cvar_RegisterVariable (&scr_screenshot_gammaboost);
	Cvar_RegisterVariable (&scr_screenshot_hwgamma);
	Cvar_RegisterVariable (&scr_screenshot_name_in_mapdir);
	Cvar_RegisterVariable (&scr_screenshot_alpha);
	Cvar_RegisterVariable (&cl_capturevideo);
	Cvar_RegisterVariable (&cl_capturevideo_printfps);
	Cvar_RegisterVariable (&cl_capturevideo_width);
	Cvar_RegisterVariable (&cl_capturevideo_height);
	Cvar_RegisterVariable (&cl_capturevideo_realtime);
	Cvar_RegisterVariable (&cl_capturevideo_fps);
	Cvar_RegisterVariable (&cl_capturevideo_nameformat);
	Cvar_RegisterVariable (&cl_capturevideo_number);
	Cvar_RegisterVariable (&cl_capturevideo_ogg);
	Cvar_RegisterVariable (&cl_capturevideo_framestep);
	Cvar_RegisterVariable (&r_letterbox);
	Cvar_RegisterVariable(&r_stereo_separation);
	Cvar_RegisterVariable(&r_stereo_sidebyside);
	Cvar_RegisterVariable(&r_stereo_horizontal);
	Cvar_RegisterVariable(&r_stereo_vertical);
	Cvar_RegisterVariable(&r_stereo_redblue);
	Cvar_RegisterVariable(&r_stereo_redcyan);
	Cvar_RegisterVariable(&r_stereo_redgreen);
	Cvar_RegisterVariable(&r_stereo_angle);
	Cvar_RegisterVariable(&scr_zoomwindow);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizex);
	Cvar_RegisterVariable(&scr_zoomwindow_viewsizey);
	Cvar_RegisterVariable(&scr_zoomwindow_fov);
	Cvar_RegisterVariable(&scr_stipple);
	Cvar_RegisterVariable(&scr_refresh);
	Cvar_RegisterVariable(&shownetgraph);
	Cvar_RegisterVariable(&cl_demo_mousegrab);
	Cvar_RegisterVariable(&timedemo_screenshotframelist);
	Cvar_RegisterVariable(&vid_touchscreen_outlinealpha);
	Cvar_RegisterVariable(&vid_touchscreen_overlayalpha);

	Cmd_AddCommand ("sizeup",SCR_SizeUp_f, "increase view size (increases viewsize cvar)");
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f, "decrease view size (decreases viewsize cvar)");
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f, "takes a screenshot of the next rendered frame");
	Cmd_AddCommand ("envmap", R_Envmap_f, "render a cubemap (skybox) of the current scene");
	Cmd_AddCommand ("infobar", SCR_InfoBar_f, "display a text in the infobar (usage: infobar expiretime string)");

	SCR_CaptureVideo_Ogg_Init();

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
	static char old_prefix_name[MAX_QPATH];
	char prefix_name[MAX_QPATH];
	char filename[MAX_QPATH];
	unsigned char *buffer1;
	unsigned char *buffer2;
	qboolean jpeg = (scr_screenshot_jpeg.integer != 0);
	qboolean png = (scr_screenshot_png.integer != 0) && !jpeg;

	if (Cmd_Argc() == 2)
	{
		const char *ext;
		strlcpy(filename, Cmd_Argv(1), sizeof(filename));
		ext = FS_FileExtension(filename);
		if (!strcasecmp(ext, "jpg"))
		{
			jpeg = true;
			png = false;
		}
		else if (!strcasecmp(ext, "tga"))
		{
			jpeg = false;
			png = false;
		}
		else if (!strcasecmp(ext, "png"))
		{
			jpeg = false;
			png = true;
		}
		else
		{
			Con_Printf("screenshot: supplied filename must end in .jpg or .tga or .png\n");
			return;
		}
	}
	else
	{
		// TODO maybe make capturevideo and screenshot use similar name patterns?
		if (scr_screenshot_name_in_mapdir.integer && cl.worldbasename[0])
			dpsnprintf (prefix_name, sizeof(prefix_name), "%s/%s", cl.worldbasename, Sys_TimeString(scr_screenshot_name.string));
		else
			dpsnprintf (prefix_name, sizeof(prefix_name), "%s", Sys_TimeString(scr_screenshot_name.string));

		if (strcmp(old_prefix_name, prefix_name))
		{
			dpsnprintf(old_prefix_name, sizeof(old_prefix_name), "%s", prefix_name );
			shotnumber = 0;
		}

		// find a file name to save it to
		for (;shotnumber < 1000000;shotnumber++)
			if (!FS_SysFileExists(va("%s/screenshots/%s%06d.tga", fs_gamedir, prefix_name, shotnumber)) && !FS_SysFileExists(va("%s/screenshots/%s%06d.jpg", fs_gamedir, prefix_name, shotnumber)) && !FS_SysFileExists(va("%s/screenshots/%s%06d.png", fs_gamedir, prefix_name, shotnumber)))
				break;
		if (shotnumber >= 1000000)
		{
			Con_Print("Couldn't create the image file\n");
			return;
		}

		dpsnprintf(filename, sizeof(filename), "screenshots/%s%06d.%s", prefix_name, shotnumber, jpeg ? "jpg" : png ? "png" : "tga");
	}

	buffer1 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 4);
	buffer2 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * (scr_screenshot_alpha.integer ? 4 : 3));

	if (SCR_ScreenShot (filename, buffer1, buffer2, 0, 0, vid.width, vid.height, false, false, false, jpeg, png, true, scr_screenshot_alpha.integer != 0))
		Con_Printf("Wrote %s\n", filename);
	else
	{
		Con_Printf("Unable to write %s\n", filename);
		if(jpeg || png)
		{
			if(SCR_ScreenShot (filename, buffer1, buffer2, 0, 0, vid.width, vid.height, false, false, false, false, false, true, scr_screenshot_alpha.integer != 0))
			{
				strlcpy(filename + strlen(filename) - 3, "tga", 4);
				Con_Printf("Wrote %s\n", filename);
			}
		}
	}

	Mem_Free (buffer1);
	Mem_Free (buffer2);

	shotnumber++;
}

void SCR_CaptureVideo_BeginVideo(void)
{
	double r, g, b;
	unsigned int i;
	int width = cl_capturevideo_width.integer, height = cl_capturevideo_height.integer;
	if (cls.capturevideo.active)
		return;
	memset(&cls.capturevideo, 0, sizeof(cls.capturevideo));
	// soundrate is figured out on the first SoundFrame

	if(width == 0 && height != 0)
		width = (int) (height * (double)vid.width / ((double)vid.height * vid_pixelheight.value)); // keep aspect
	if(width != 0 && height == 0)
		height = (int) (width * ((double)vid.height * vid_pixelheight.value) / (double)vid.width); // keep aspect

	if(width < 2 || width > vid.width) // can't scale up
		width = vid.width;
	if(height < 2 || height > vid.height) // can't scale up
		height = vid.height;

	// ensure it's all even; if not, scale down a little
	if(width % 1)
		--width;
	if(height % 1)
		--height;

	cls.capturevideo.width = width;
	cls.capturevideo.height = height;
	cls.capturevideo.active = true;
	cls.capturevideo.framerate = bound(1, cl_capturevideo_fps.value, 1001) * bound(1, cl_capturevideo_framestep.integer, 64);
	cls.capturevideo.framestep = cl_capturevideo_framestep.integer;
	cls.capturevideo.soundrate = S_GetSoundRate();
	cls.capturevideo.soundchannels = S_GetSoundChannels();
	cls.capturevideo.startrealtime = realtime;
	cls.capturevideo.frame = cls.capturevideo.lastfpsframe = 0;
	cls.capturevideo.starttime = cls.capturevideo.lastfpstime = Sys_DoubleTime();
	cls.capturevideo.soundsampleframe = 0;
	cls.capturevideo.realtime = cl_capturevideo_realtime.integer != 0;
	cls.capturevideo.screenbuffer = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 4);
	cls.capturevideo.outbuffer = (unsigned char *)Mem_Alloc(tempmempool, width * height * (4+4) + 18);
	dpsnprintf(cls.capturevideo.basename, sizeof(cls.capturevideo.basename), "video/%s%03i", Sys_TimeString(cl_capturevideo_nameformat.string), cl_capturevideo_number.integer);
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

	if(WANT_SCREENSHOT_HWGAMMA)
	{
		VID_BuildGammaTables(&cls.capturevideo.vidramp[0], 256);
	}
	else
	{
		// identity gamma table
		BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, cls.capturevideo.vidramp, 256);
		BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, cls.capturevideo.vidramp + 256, 256);
		BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, cls.capturevideo.vidramp + 256*2, 256);
	}
	if(scr_screenshot_gammaboost.value != 1)
	{
		double igamma = 1 / scr_screenshot_gammaboost.value;
		for (i = 0;i < 256 * 3;i++)
			cls.capturevideo.vidramp[i] = (unsigned short) (0.5 + pow(cls.capturevideo.vidramp[i] * (1.0 / 65535.0), igamma) * 65535.0);
	}

	for (i = 0;i < 256;i++)
	{
		r = 255*cls.capturevideo.vidramp[i]/65535.0;
		g = 255*cls.capturevideo.vidramp[i+256]/65535.0;
		b = 255*cls.capturevideo.vidramp[i+512]/65535.0;
		// NOTE: we have to round DOWN here, or integer overflows happen. Sorry for slightly wrong looking colors sometimes...
		// Y weights from RGB
		cls.capturevideo.rgbtoyuvscaletable[0][0][i] = (short)(r *  0.299);
		cls.capturevideo.rgbtoyuvscaletable[0][1][i] = (short)(g *  0.587);
		cls.capturevideo.rgbtoyuvscaletable[0][2][i] = (short)(b *  0.114);
		// Cb weights from RGB
		cls.capturevideo.rgbtoyuvscaletable[1][0][i] = (short)(r * -0.169);
		cls.capturevideo.rgbtoyuvscaletable[1][1][i] = (short)(g * -0.332);
		cls.capturevideo.rgbtoyuvscaletable[1][2][i] = (short)(b *  0.500);
		// Cr weights from RGB
		cls.capturevideo.rgbtoyuvscaletable[2][0][i] = (short)(r *  0.500);
		cls.capturevideo.rgbtoyuvscaletable[2][1][i] = (short)(g * -0.419);
		cls.capturevideo.rgbtoyuvscaletable[2][2][i] = (short)(b * -0.0813);
		// range reduction of YCbCr to valid signal range
		cls.capturevideo.yuvnormalizetable[0][i] = 16 + i * (236-16) / 256;
		cls.capturevideo.yuvnormalizetable[1][i] = 16 + i * (240-16) / 256;
		cls.capturevideo.yuvnormalizetable[2][i] = 16 + i * (240-16) / 256;
	}

	if (cl_capturevideo_ogg.integer)
	{
		if(SCR_CaptureVideo_Ogg_Available())
		{
			SCR_CaptureVideo_Ogg_BeginVideo();
			return;
		}
		else
			Con_Print("cl_capturevideo_ogg: libraries not available. Capturing in AVI instead.\n");
	}

	SCR_CaptureVideo_Avi_BeginVideo();
}

void SCR_CaptureVideo_EndVideo(void)
{
	if (!cls.capturevideo.active)
		return;
	cls.capturevideo.active = false;

	Con_Printf("Finishing capture of %s.%s (%d frames, %d audio frames)\n", cls.capturevideo.basename, cls.capturevideo.formatextension, cls.capturevideo.frame, cls.capturevideo.soundsampleframe);

	if (cls.capturevideo.videofile)
	{
		cls.capturevideo.endvideo();
	}

	if (cls.capturevideo.screenbuffer)
	{
		Mem_Free (cls.capturevideo.screenbuffer);
		cls.capturevideo.screenbuffer = NULL;
	}

	if (cls.capturevideo.outbuffer)
	{
		Mem_Free (cls.capturevideo.outbuffer);
		cls.capturevideo.outbuffer = NULL;
	}

	memset(&cls.capturevideo, 0, sizeof(cls.capturevideo));
}

static void SCR_ScaleDownBGRA(unsigned char *in, int inw, int inh, unsigned char *out, int outw, int outh)
{
	// TODO optimize this function

	int x, y;
	float area;

	// memcpy is faster than me
	if(inw == outw && inh == outh)
	{
		memcpy(out, in, 4 * inw * inh);
		return;
	}

	// otherwise: a box filter
	area = (float)outw * (float)outh / (float)inw / (float)inh;
	for(y = 0; y < outh; ++y)
	{
		float iny0 =  y    / (float)outh * inh; int iny0_i = (int) floor(iny0);
		float iny1 = (y+1) / (float)outh * inh; int iny1_i = (int) ceil(iny1);
		for(x = 0; x < outw; ++x)
		{
			float inx0 =  x    / (float)outw * inw; int inx0_i = (int) floor(inx0);
			float inx1 = (x+1) / (float)outw * inw; int inx1_i = (int) ceil(inx1);
			float r = 0, g = 0, b = 0, alpha = 0;
			int xx, yy;

			for(yy = iny0_i; yy < iny1_i; ++yy)
			{
				float ya = min(yy+1, iny1) - max(iny0, yy);
				for(xx = inx0_i; xx < inx1_i; ++xx)
				{
					float a = ya * (min(xx+1, inx1) - max(inx0, xx));
					r += a * in[4*(xx + inw * yy)+0];
					g += a * in[4*(xx + inw * yy)+1];
					b += a * in[4*(xx + inw * yy)+2];
					alpha += a * in[4*(xx + inw * yy)+3];
				}
			}

			out[4*(x + outw * y)+0] = (unsigned char) (r * area);
			out[4*(x + outw * y)+1] = (unsigned char) (g * area);
			out[4*(x + outw * y)+2] = (unsigned char) (b * area);
			out[4*(x + outw * y)+3] = (unsigned char) (alpha * area);
		}
	}
}

void SCR_CaptureVideo_VideoFrame(int newframestepframenum)
{
	int x = 0, y = 0;
	int width = cls.capturevideo.width, height = cls.capturevideo.height;

	if(newframestepframenum == cls.capturevideo.framestepframe)
		return;

	CHECKGLERROR
	// speed is critical here, so do saving as directly as possible

	GL_ReadPixelsBGRA(x, y, vid.width, vid.height, cls.capturevideo.screenbuffer);

	SCR_ScaleDownBGRA (cls.capturevideo.screenbuffer, vid.width, vid.height, cls.capturevideo.outbuffer, width, height);

	cls.capturevideo.videoframes(newframestepframenum - cls.capturevideo.framestepframe);
	cls.capturevideo.framestepframe = newframestepframenum;

	if(cl_capturevideo_printfps.integer)
	{
		char buf[80];
		double t = Sys_DoubleTime();
		if(t > cls.capturevideo.lastfpstime + 1)
		{
			double fps1 = (cls.capturevideo.frame - cls.capturevideo.lastfpsframe) / (t - cls.capturevideo.lastfpstime + 0.0000001);
			double fps  = (cls.capturevideo.frame                                ) / (t - cls.capturevideo.starttime   + 0.0000001);
			dpsnprintf(buf, sizeof(buf), "capturevideo: (%.1fs) last second %.3ffps, total %.3ffps\n", cls.capturevideo.frame / cls.capturevideo.framerate, fps1, fps);
			Sys_PrintToTerminal(buf);
			cls.capturevideo.lastfpstime = t;
			cls.capturevideo.lastfpsframe = cls.capturevideo.frame;
		}
	}
}

void SCR_CaptureVideo_SoundFrame(const portable_sampleframe_t *paintbuffer, size_t length)
{
	cls.capturevideo.soundsampleframe += length;
	cls.capturevideo.soundframe(paintbuffer, length);
}

void SCR_CaptureVideo(void)
{
	int newframenum;
	if (cl_capturevideo.integer)
	{
		if (!cls.capturevideo.active)
			SCR_CaptureVideo_BeginVideo();
		if (cls.capturevideo.framerate != cl_capturevideo_fps.value * cl_capturevideo_framestep.integer)
		{
			Con_Printf("You can not change the video framerate while recording a video.\n");
			Cvar_SetValueQuick(&cl_capturevideo_fps, cls.capturevideo.framerate / (double) cl_capturevideo_framestep.integer);
		}
		// for AVI saving we have to make sure that sound is saved before video
		if (cls.capturevideo.soundrate && !cls.capturevideo.soundsampleframe)
			return;
		if (cls.capturevideo.realtime)
		{
			// preserve sound sync by duplicating frames when running slow
			newframenum = (int)((realtime - cls.capturevideo.startrealtime) * cls.capturevideo.framerate);
		}
		else
			newframenum = cls.capturevideo.frame + 1;
		// if falling behind more than one second, stop
		if (newframenum - cls.capturevideo.frame > 60 * (int)ceil(cls.capturevideo.framerate))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, your machine is too slow for this capture speed.\n", cls.capturevideo.frame);
			SCR_CaptureVideo_EndVideo();
			return;
		}
		// write frames
		SCR_CaptureVideo_VideoFrame(newframenum / cls.capturevideo.framestep);
		cls.capturevideo.frame = newframenum;
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
	const char *name;
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

	r_refdef.view.x = 0;
	r_refdef.view.y = 0;
	r_refdef.view.z = 0;
	r_refdef.view.width = size;
	r_refdef.view.height = size;
	r_refdef.view.depth = 1;
	r_refdef.view.useperspective = true;
	r_refdef.view.isoverlay = false;

	r_refdef.view.frustum_x = 1; // tan(45 * M_PI / 180.0);
	r_refdef.view.frustum_y = 1; // tan(45 * M_PI / 180.0);

	buffer1 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 4);
	buffer2 = (unsigned char *)Mem_Alloc(tempmempool, size * size * 3);

	for (j = 0;j < 12;j++)
	{
		dpsnprintf(filename, sizeof(filename), "env/%s%s.tga", basename, envmapinfo[j].name);
		Matrix4x4_CreateFromQuakeEntity(&r_refdef.view.matrix, r_refdef.view.origin[0], r_refdef.view.origin[1], r_refdef.view.origin[2], envmapinfo[j].angles[0], envmapinfo[j].angles[1], envmapinfo[j].angles[2], 1);
		r_refdef.view.quality = 1;
		r_refdef.view.clear = true;
		R_Mesh_Start();
		R_RenderView();
		R_Mesh_Finish();
		SCR_ScreenShot(filename, buffer1, buffer2, 0, vid.height - (r_refdef.view.y + r_refdef.view.height), size, size, envmapinfo[j].flipx, envmapinfo[j].flipy, envmapinfo[j].flipdiagonaly, false, false, false, false);
	}

	Mem_Free (buffer1);
	Mem_Free (buffer2);

	r_refdef.envmap = false;
}

//=============================================================================

void SHOWLMP_decodehide(void)
{
	int i;
	char *lmplabel;
	lmplabel = MSG_ReadString();
	for (i = 0;i < cl.num_showlmps;i++)
		if (cl.showlmps[i].isactive && strcmp(cl.showlmps[i].label, lmplabel) == 0)
		{
			cl.showlmps[i].isactive = false;
			return;
		}
}

void SHOWLMP_decodeshow(void)
{
	int k;
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
	if (!cl.showlmps || cl.num_showlmps >= cl.max_showlmps)
	{
		showlmp_t *oldshowlmps = cl.showlmps;
		cl.max_showlmps += 16;
		cl.showlmps = (showlmp_t *) Mem_Alloc(cls.levelmempool, cl.max_showlmps * sizeof(showlmp_t));
		if (cl.num_showlmps)
			memcpy(cl.showlmps, oldshowlmps, cl.num_showlmps * sizeof(showlmp_t));
		if (oldshowlmps)
			Mem_Free(oldshowlmps);
	}
	for (k = 0;k < cl.max_showlmps;k++)
		if (cl.showlmps[k].isactive && !strcmp(cl.showlmps[k].label, lmplabel))
			break;
	if (k == cl.max_showlmps)
		for (k = 0;k < cl.max_showlmps;k++)
			if (!cl.showlmps[k].isactive)
				break;
	cl.showlmps[k].isactive = true;
	strlcpy (cl.showlmps[k].label, lmplabel, sizeof (cl.showlmps[k].label));
	strlcpy (cl.showlmps[k].pic, picname, sizeof (cl.showlmps[k].pic));
	cl.showlmps[k].x = x;
	cl.showlmps[k].y = y;
	cl.num_showlmps = max(cl.num_showlmps, k + 1);
}

void SHOWLMP_drawall(void)
{
	int i;
	for (i = 0;i < cl.num_showlmps;i++)
		if (cl.showlmps[i].isactive)
			DrawQ_Pic(cl.showlmps[i].x, cl.showlmps[i].y, Draw_CachePic_Flags (cl.showlmps[i].pic, CACHEPICFLAG_NOTPERSISTENT), 0, 0, 1, 1, 1, 1, 0);
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

// buffer1: 4*w*h
// buffer2: 3*w*h (or 4*w*h if screenshotting alpha too)
qboolean SCR_ScreenShot(char *filename, unsigned char *buffer1, unsigned char *buffer2, int x, int y, int width, int height, qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean jpeg, qboolean png, qboolean gammacorrect, qboolean keep_alpha)
{
	int	indices[4] = {0,1,2,3}; // BGRA
	qboolean ret;

	GL_ReadPixelsBGRA(x, y, width, height, buffer1);

	if(gammacorrect && (scr_screenshot_gammaboost.value != 1 || WANT_SCREENSHOT_HWGAMMA))
	{
		int i;
		double igamma = 1.0 / scr_screenshot_gammaboost.value;
		unsigned short vidramp[256 * 3];
		if(WANT_SCREENSHOT_HWGAMMA)
		{
			VID_BuildGammaTables(&vidramp[0], 256);
		}
		else
		{
			// identity gamma table
			BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, vidramp, 256);
			BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, vidramp + 256, 256);
			BuildGammaTable16(1.0f, 1.0f, 1.0f, 0.0f, 1.0f, vidramp + 256*2, 256);
		}
		if(scr_screenshot_gammaboost.value != 1)
		{
			for (i = 0;i < 256 * 3;i++)
				vidramp[i] = (unsigned short) (0.5 + pow(vidramp[i] * (1.0 / 65535.0), igamma) * 65535.0);
		}
		for (i = 0;i < width*height*4;i += 4)
		{
			buffer1[i] = (unsigned char) (vidramp[buffer1[i] + 512] * 255.0 / 65535.0 + 0.5); // B
			buffer1[i+1] = (unsigned char) (vidramp[buffer1[i+1] + 256] * 255.0 / 65535.0 + 0.5); // G
			buffer1[i+2] = (unsigned char) (vidramp[buffer1[i+2]] * 255.0 / 65535.0 + 0.5); // R
			// A
		}
	}

	if(keep_alpha && !jpeg)
	{
		if(!png)
			flipy = !flipy; // TGA: not preflipped
		Image_CopyMux (buffer2, buffer1, width, height, flipx, flipy, flipdiagonal, 4, 4, indices);
		if (png)
			ret = PNG_SaveImage_preflipped (filename, width, height, true, buffer2);
		else
			ret = Image_WriteTGABGRA(filename, width, height, buffer2);
	}
	else
	{
		if(jpeg)
		{
			indices[0] = 2;
			indices[2] = 0; // RGB
		}
		Image_CopyMux (buffer2, buffer1, width, height, flipx, flipy, flipdiagonal, 3, 4, indices);
		if (jpeg)
			ret = JPEG_SaveImage_preflipped (filename, width, height, buffer2);
		else if (png)
			ret = PNG_SaveImage_preflipped (filename, width, height, false, buffer2);
		else
			ret = Image_WriteTGABGR_preflipped (filename, width, height, buffer2);
	}

	return ret;
}

//=============================================================================

int scr_numtouchscreenareas;
scr_touchscreenarea_t scr_touchscreenareas[16];

static void SCR_DrawTouchscreenOverlay(void)
{
	int i;
	scr_touchscreenarea_t *a;
	cachepic_t *pic;
	for (i = 0, a = scr_touchscreenareas;i < scr_numtouchscreenareas;i++, a++)
	{
		if (vid_touchscreen_outlinealpha.value > 0 && a->rect[0] >= 0 && a->rect[1] >= 0 && a->rect[2] >= 4 && a->rect[3] >= 4)
		{
			DrawQ_Fill(a->rect[0] +              2, a->rect[1]                 , a->rect[2] - 4,          1    , 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
			DrawQ_Fill(a->rect[0] +              1, a->rect[1] +              1, a->rect[2] - 2,          1    , 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
			DrawQ_Fill(a->rect[0]                 , a->rect[1] +              2,          2    , a->rect[3] - 2, 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
			DrawQ_Fill(a->rect[0] + a->rect[2] - 2, a->rect[1] +              2,          2    , a->rect[3] - 2, 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
			DrawQ_Fill(a->rect[0] +              1, a->rect[1] + a->rect[3] - 2, a->rect[2] - 2,          1    , 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
			DrawQ_Fill(a->rect[0] +              2, a->rect[1] + a->rect[3] - 1, a->rect[2] - 4,          1    , 1, 1, 1, vid_touchscreen_outlinealpha.value * (0.5f + 0.5f * a->active), 0);
		}
		pic = a->pic ? Draw_CachePic(a->pic) : NULL;
		if (pic && pic->tex != r_texture_notexture)
			DrawQ_Pic(a->rect[0], a->rect[1], Draw_CachePic(a->pic), a->rect[2], a->rect[3], 1, 1, 1, vid_touchscreen_overlayalpha.value * (0.5f + 0.5f * a->active), 0);
	}
}

extern void R_UpdateFogColor(void);
void R_ClearScreen(qboolean fogcolor)
{
	float clearcolor[4];
	// clear to black
	Vector4Clear(clearcolor);
	if (fogcolor)
	{
		R_UpdateFogColor();
		if (r_fog_clear.integer)
			VectorCopy(r_refdef.fogcolor, clearcolor);
	}
	// clear depth is 1.0
	// LordHavoc: we use a stencil centered around 128 instead of 0,
	// to avoid clamping interfering with strange shadow volume
	// drawing orders
	// clear the screen
	GL_Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (vid.stencil ? GL_STENCIL_BUFFER_BIT : 0), clearcolor, 1.0f, 128);
}

qboolean CL_VM_UpdateView (void);
void SCR_DrawConsole (void);
void R_Shadow_EditLights_DrawSelectedLightProperties(void);

int r_stereo_side;

extern void Sbar_ShowFPS(void);
void SCR_DrawScreen (void)
{
	Draw_Frame();

	R_Mesh_Start();

	R_UpdateVariables();

	// Quake uses clockwise winding, so these are swapped
	r_refdef.view.cullface_front = GL_BACK;
	r_refdef.view.cullface_back = GL_FRONT;

	if (cls.signon == SIGNONS)
	{
		float size;

		size = scr_viewsize.value * (1.0 / 100.0);
		size = min(size, 1);

		if (r_stereo_sidebyside.integer)
		{
			r_refdef.view.width = (int)(vid.width * size / 2.5);
			r_refdef.view.height = (int)(vid.height * size / 2.5 * (1 - bound(0, r_letterbox.value, 100) / 100));
			r_refdef.view.depth = 1;
			r_refdef.view.x = (int)((vid.width - r_refdef.view.width * 2.5) * 0.5);
			r_refdef.view.y = (int)((vid.height - r_refdef.view.height)/2);
			r_refdef.view.z = 0;
			if (r_stereo_side)
				r_refdef.view.x += (int)(r_refdef.view.width * 1.5);
		}
		else if (r_stereo_horizontal.integer)
		{
			r_refdef.view.width = (int)(vid.width * size / 2);
			r_refdef.view.height = (int)(vid.height * size * (1 - bound(0, r_letterbox.value, 100) / 100));
			r_refdef.view.depth = 1;
			r_refdef.view.x = (int)((vid.width - r_refdef.view.width * 2.0)/2);
			r_refdef.view.y = (int)((vid.height - r_refdef.view.height)/2);
			r_refdef.view.z = 0;
			if (r_stereo_side)
				r_refdef.view.x += (int)(r_refdef.view.width);
		}
		else if (r_stereo_vertical.integer)
		{
			r_refdef.view.width = (int)(vid.width * size);
			r_refdef.view.height = (int)(vid.height * size * (1 - bound(0, r_letterbox.value, 100) / 100) / 2);
			r_refdef.view.depth = 1;
			r_refdef.view.x = (int)((vid.width - r_refdef.view.width)/2);
			r_refdef.view.y = (int)((vid.height - r_refdef.view.height * 2.0)/2);
			r_refdef.view.z = 0;
			if (r_stereo_side)
				r_refdef.view.y += (int)(r_refdef.view.height);
		}
		else
		{
			r_refdef.view.width = (int)(vid.width * size);
			r_refdef.view.height = (int)(vid.height * size * (1 - bound(0, r_letterbox.value, 100) / 100));
			r_refdef.view.depth = 1;
			r_refdef.view.x = (int)((vid.width - r_refdef.view.width)/2);
			r_refdef.view.y = (int)((vid.height - r_refdef.view.height)/2);
			r_refdef.view.z = 0;
		}

		// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
		// LordHavoc: this is designed to produce widescreen fov values
		// when the screen is wider than 4/3 width/height aspect, to do
		// this it simply assumes the requested fov is the vertical fov
		// for a 4x3 display, if the ratio is not 4x3 this makes the fov
		// higher/lower according to the ratio
		r_refdef.view.useperspective = true;
		r_refdef.view.frustum_y = tan(scr_fov.value * M_PI / 360.0) * (3.0/4.0) * cl.viewzoom;
		r_refdef.view.frustum_x = r_refdef.view.frustum_y * (float)r_refdef.view.width / (float)r_refdef.view.height / vid_pixelheight.value;

		r_refdef.view.frustum_x *= r_refdef.frustumscale_x;
		r_refdef.view.frustum_y *= r_refdef.frustumscale_y;

		if(!CL_VM_UpdateView())
			R_RenderView();

		if (scr_zoomwindow.integer)
		{
			float sizex = bound(10, scr_zoomwindow_viewsizex.value, 100) / 100.0;
			float sizey = bound(10, scr_zoomwindow_viewsizey.value, 100) / 100.0;
			r_refdef.view.width = (int)(vid.width * sizex);
			r_refdef.view.height = (int)(vid.height * sizey);
			r_refdef.view.depth = 1;
			r_refdef.view.x = (int)((vid.width - r_refdef.view.width)/2);
			r_refdef.view.y = 0;
			r_refdef.view.z = 0;

			r_refdef.view.useperspective = true;
			r_refdef.view.frustum_y = tan(scr_zoomwindow_fov.value * M_PI / 360.0) * (3.0/4.0) * cl.viewzoom;
			r_refdef.view.frustum_x = r_refdef.view.frustum_y * vid_pixelheight.value * (float)r_refdef.view.width / (float)r_refdef.view.height;

			r_refdef.view.frustum_x *= r_refdef.frustumscale_x;
			r_refdef.view.frustum_y *= r_refdef.frustumscale_y;

			if(!CL_VM_UpdateView())
				R_RenderView();
		}
	}

	if (!r_stereo_sidebyside.integer && !r_stereo_horizontal.integer && !r_stereo_vertical.integer)
	{
		r_refdef.view.width = vid.width;
		r_refdef.view.height = vid.height;
		r_refdef.view.depth = 1;
		r_refdef.view.x = 0;
		r_refdef.view.y = 0;
		r_refdef.view.z = 0;
		r_refdef.view.useperspective = false;
	}

	if (cls.timedemo && cls.td_frames > 0 && timedemo_screenshotframelist.string && timedemo_screenshotframelist.string[0])
	{
		const char *t;
		int framenum;
		t = timedemo_screenshotframelist.string;
		while (*t)
		{
			while (*t == ' ')
				t++;
			if (!*t)
				break;
			framenum = atof(t);
			if (framenum == cls.td_frames)
				break;
			while (*t && *t != ' ')
				t++;
		}
		if (*t)
		{
			// we need to take a screenshot of this frame...
			char filename[MAX_QPATH];
			unsigned char *buffer1;
			unsigned char *buffer2;
			dpsnprintf(filename, sizeof(filename), "timedemoscreenshots/%s%06d.tga", cls.demoname, cls.td_frames);
			buffer1 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 4);
			buffer2 = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * 3);
			SCR_ScreenShot(filename, buffer1, buffer2, 0, 0, vid.width, vid.height, false, false, false, false, false, true, false);
			Mem_Free(buffer1);
			Mem_Free(buffer2);
		}
	}

	// draw 2D stuff
	if(!scr_con_current && !(key_consoleactive & KEY_CONSOLEACTIVE_FORCED))
		if ((key_dest == key_game || key_dest == key_message) && !r_letterbox.value)
			Con_DrawNotify ();	// only draw notify in game

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
	SCR_DrawNetGraph ();
	MR_Draw();
	CL_DrawVideo();
	R_Shadow_EditLights_DrawSelectedLightProperties();

	SCR_DrawConsole();

	SCR_DrawBrand();

	SCR_DrawInfobar();

	SCR_DrawTouchscreenOverlay();

	if (r_timereport_active)
		R_TimeReport("2d");

	R_TimeReport_EndFrame();
	R_TimeReport_BeginFrame();
	Sbar_ShowFPS();

	DrawQ_Finish();

	R_DrawGamma();

	R_Mesh_Finish();
}

typedef struct loadingscreenstack_s
{
	struct loadingscreenstack_s *prev;
	char msg[MAX_QPATH];
	float absolute_loading_amount_min; // this corresponds to relative completion 0 of this item
	float absolute_loading_amount_len; // this corresponds to relative completion 1 of this item
	float relative_completion; // 0 .. 1
}
loadingscreenstack_t;
static loadingscreenstack_t *loadingscreenstack = NULL;
static qboolean loadingscreendone = false;
static qboolean loadingscreencleared = false;
static float loadingscreenheight = 0;
rtexture_t *loadingscreentexture = NULL;
static float loadingscreentexture_vertex3f[12];
static float loadingscreentexture_texcoord2f[8];
static int loadingscreenpic_number = 0;

static void SCR_ClearLoadingScreenTexture(void)
{
	if(loadingscreentexture)
		R_FreeTexture(loadingscreentexture);
	loadingscreentexture = NULL;
}

extern rtexturepool_t *r_main_texturepool;
static void SCR_SetLoadingScreenTexture(void)
{
	int w, h;
	float loadingscreentexture_w;
	float loadingscreentexture_h;

	SCR_ClearLoadingScreenTexture();

	if (vid.support.arb_texture_non_power_of_two)
	{
		w = vid.width; h = vid.height;
		loadingscreentexture_w = loadingscreentexture_h = 1;
	}
	else
	{
		w = CeilPowerOf2(vid.width); h = CeilPowerOf2(vid.height);
		loadingscreentexture_w = vid.width / (float) w;
		loadingscreentexture_h = vid.height / (float) h;
	}

	loadingscreentexture = R_LoadTexture2D(r_main_texturepool, "loadingscreentexture", w, h, NULL, TEXTYPE_COLORBUFFER, TEXF_RENDERTARGET | TEXF_FORCENEAREST | TEXF_CLAMP, -1, NULL);
	R_Mesh_CopyToTexture(loadingscreentexture, 0, 0, 0, 0, vid.width, vid.height);

	loadingscreentexture_vertex3f[2] = loadingscreentexture_vertex3f[5] = loadingscreentexture_vertex3f[8] = loadingscreentexture_vertex3f[11] = 0;
	loadingscreentexture_vertex3f[0] = loadingscreentexture_vertex3f[9] = 0;
	loadingscreentexture_vertex3f[1] = loadingscreentexture_vertex3f[4] = 0;
	loadingscreentexture_vertex3f[3] = loadingscreentexture_vertex3f[6] = vid_conwidth.integer;
	loadingscreentexture_vertex3f[7] = loadingscreentexture_vertex3f[10] = vid_conheight.integer;
	loadingscreentexture_texcoord2f[0] = 0;loadingscreentexture_texcoord2f[1] = loadingscreentexture_h;
	loadingscreentexture_texcoord2f[2] = loadingscreentexture_w;loadingscreentexture_texcoord2f[3] = loadingscreentexture_h;
	loadingscreentexture_texcoord2f[4] = loadingscreentexture_w;loadingscreentexture_texcoord2f[5] = 0;
	loadingscreentexture_texcoord2f[6] = 0;loadingscreentexture_texcoord2f[7] = 0;
}

void SCR_UpdateLoadingScreenIfShown(void)
{
	if(loadingscreendone)
		SCR_UpdateLoadingScreen(loadingscreencleared);
}

void SCR_PushLoadingScreen (qboolean redraw, const char *msg, float len_in_parent)
{
	loadingscreenstack_t *s = (loadingscreenstack_t *) Z_Malloc(sizeof(loadingscreenstack_t));
	s->prev = loadingscreenstack;
	loadingscreenstack = s;

	strlcpy(s->msg, msg, sizeof(s->msg));
	s->relative_completion = 0;

	if(s->prev)
	{
		s->absolute_loading_amount_min = s->prev->absolute_loading_amount_min + s->prev->absolute_loading_amount_len * s->prev->relative_completion;
		s->absolute_loading_amount_len = s->prev->absolute_loading_amount_len * len_in_parent;
		if(s->absolute_loading_amount_len > s->prev->absolute_loading_amount_min + s->prev->absolute_loading_amount_len - s->absolute_loading_amount_min)
			s->absolute_loading_amount_len = s->prev->absolute_loading_amount_min + s->prev->absolute_loading_amount_len - s->absolute_loading_amount_min;
	}
	else
	{
		s->absolute_loading_amount_min = 0;
		s->absolute_loading_amount_len = 1;
	}

	if(redraw)
		SCR_UpdateLoadingScreenIfShown();
}

void SCR_PopLoadingScreen (qboolean redraw)
{
	loadingscreenstack_t *s = loadingscreenstack;

	if(!s)
	{
		Con_DPrintf("Popping a loading screen item from an empty stack!\n");
		return;
	}

	loadingscreenstack = s->prev;
	if(s->prev)
		s->prev->relative_completion = (s->absolute_loading_amount_min + s->absolute_loading_amount_len - s->prev->absolute_loading_amount_min) / s->prev->absolute_loading_amount_len;
	Z_Free(s);

	if(redraw)
		SCR_UpdateLoadingScreenIfShown();
}

void SCR_ClearLoadingScreen (qboolean redraw)
{
	while(loadingscreenstack)
		SCR_PopLoadingScreen(redraw && !loadingscreenstack->prev);
}

static float SCR_DrawLoadingStack_r(loadingscreenstack_t *s, float y, float size)
{
	float x;
	size_t len;
	float total;

	total = 0;
#if 0
	if(s)
	{
		total += SCR_DrawLoadingStack_r(s->prev, y, 8);
		y -= total;
		if(!s->prev || strcmp(s->msg, s->prev->msg))
		{
			len = strlen(s->msg);
			x = (vid_conwidth.integer - DrawQ_TextWidth(s->msg, len, size, size, true, FONT_INFOBAR)) / 2;
			y -= size;
			DrawQ_Fill(0, y, vid_conwidth.integer, size, 0, 0, 0, 1, 0);
			DrawQ_String(x, y, s->msg, len, size, size, 1, 1, 1, 1, 0, NULL, true, FONT_INFOBAR);
			total += size;
		}
	}
#else
	if(s)
	{
		len = strlen(s->msg);
		x = (vid_conwidth.integer - DrawQ_TextWidth(s->msg, len, size, size, true, FONT_INFOBAR)) / 2;
		y -= size;
		DrawQ_Fill(0, y, vid_conwidth.integer, size, 0, 0, 0, 1, 0);
		DrawQ_String(x, y, s->msg, len, size, size, 1, 1, 1, 1, 0, NULL, true, FONT_INFOBAR);
		total += size;
	}
#endif
	return total;
}

static void SCR_DrawLoadingStack(void)
{
	float verts[12];
	float colors[16];

	loadingscreenheight = SCR_DrawLoadingStack_r(loadingscreenstack, vid_conheight.integer, scr_loadingscreen_barheight.value);
	if(loadingscreenstack)
	{
		// height = 32; // sorry, using the actual one is ugly
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthRange(0, 1);
		GL_PolygonOffset(0, 0);
		GL_DepthTest(false);
//		R_Mesh_ResetTextureState();
		verts[2] = verts[5] = verts[8] = verts[11] = 0;
		verts[0] = verts[9] = 0;
		verts[1] = verts[4] = vid_conheight.integer - scr_loadingscreen_barheight.value;
		verts[3] = verts[6] = vid_conwidth.integer * loadingscreenstack->absolute_loading_amount_min;
		verts[7] = verts[10] = vid_conheight.integer;
		
#if _MSC_VER >= 1400
#define sscanf sscanf_s
#endif
		//                                        ^^^^^^^^^^ blue component
		//                              ^^^^^^ bottom row
		//          ^^^^^^^^^^^^ alpha is always on
		colors[0] = 0; colors[1] = 0; colors[2] = 0; colors[3] = 1;
		colors[4] = 0; colors[5] = 0; colors[6] = 0; colors[7] = 1;
		sscanf(scr_loadingscreen_barcolor.string, "%f %f %f", &colors[8], &colors[9], &colors[10]); colors[11] = 1;
		sscanf(scr_loadingscreen_barcolor.string, "%f %f %f", &colors[12], &colors[13], &colors[14]);  colors[15] = 1;

		R_Mesh_PrepareVertices_Generic_Arrays(4, verts, colors, NULL);
		R_SetupShader_Generic(NULL, NULL, GL_MODULATE, 1, true);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);

		// make sure everything is cleared, including the progress indicator
		if(loadingscreenheight < 8)
			loadingscreenheight = 8;
	}
}

static cachepic_t *loadingscreenpic;
static float loadingscreenpic_vertex3f[12];
static float loadingscreenpic_texcoord2f[8];

static void SCR_DrawLoadingScreen_SharedSetup (qboolean clear)
{
	r_viewport_t viewport;
	float x, y, w, h, sw, sh, f;
	// release mouse grab while loading
	if (!vid.fullscreen)
		VID_SetMouse(false, false, false);
//	CHECKGLERROR
	r_refdef.draw2dstage = true;
	R_Viewport_InitOrtho(&viewport, &identitymatrix, 0, 0, vid.width, vid.height, 0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100, NULL);
	R_Mesh_ResetRenderTargets();
	R_SetViewport(&viewport);
	GL_ColorMask(1,1,1,1);
	// when starting up a new video mode, make sure the screen is cleared to black
	if (clear || loadingscreentexture)
		GL_Clear(GL_COLOR_BUFFER_BIT, NULL, 1.0f, 0);
	R_Textures_Frame();
	R_Mesh_Start();
	R_EntityMatrix(&identitymatrix);
	// draw the loading plaque
	loadingscreenpic = Draw_CachePic_Flags (loadingscreenpic_number ? va("gfx/loading%d", loadingscreenpic_number+1) : "gfx/loading", loadingscreenpic_number ? CACHEPICFLAG_NOTPERSISTENT : 0);

	w = loadingscreenpic->width;
	h = loadingscreenpic->height;

	// apply scale
	w *= scr_loadingscreen_scale.value;
	h *= scr_loadingscreen_scale.value;

	// apply scale base
	if(scr_loadingscreen_scale_base.integer)
	{
		w *= vid_conwidth.integer / (float) vid.width;
		h *= vid_conheight.integer / (float) vid.height;
	}

	// apply scale limit
	sw = w / vid_conwidth.integer;
	sh = h / vid_conheight.integer;
	f = 1;
	switch(scr_loadingscreen_scale_limit.integer)
	{
		case 1:
			f = max(sw, sh);
			break;
		case 2:
			f = min(sw, sh);
			break;
		case 3:
			f = sw;
			break;
		case 4:
			f = sh;
			break;
	}
	if(f > 1)
	{
		w /= f;
		h /= f;
	}

	x = (vid_conwidth.integer - w)/2;
	y = (vid_conheight.integer - h)/2;
	loadingscreenpic_vertex3f[2] = loadingscreenpic_vertex3f[5] = loadingscreenpic_vertex3f[8] = loadingscreenpic_vertex3f[11] = 0;
	loadingscreenpic_vertex3f[0] = loadingscreenpic_vertex3f[9] = x;
	loadingscreenpic_vertex3f[1] = loadingscreenpic_vertex3f[4] = y;
	loadingscreenpic_vertex3f[3] = loadingscreenpic_vertex3f[6] = x + w;
	loadingscreenpic_vertex3f[7] = loadingscreenpic_vertex3f[10] = y + h;
	loadingscreenpic_texcoord2f[0] = 0;loadingscreenpic_texcoord2f[1] = 0;
	loadingscreenpic_texcoord2f[2] = 1;loadingscreenpic_texcoord2f[3] = 0;
	loadingscreenpic_texcoord2f[4] = 1;loadingscreenpic_texcoord2f[5] = 1;
	loadingscreenpic_texcoord2f[6] = 0;loadingscreenpic_texcoord2f[7] = 1;
}

static void SCR_DrawLoadingScreen (qboolean clear)
{
	// we only need to draw the image if it isn't already there
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(false);
//	R_Mesh_ResetTextureState();
	GL_Color(1,1,1,1);
	if(loadingscreentexture)
	{
		R_Mesh_PrepareVertices_Generic_Arrays(4, loadingscreentexture_vertex3f, NULL, loadingscreentexture_texcoord2f);
		R_SetupShader_Generic(loadingscreentexture, NULL, GL_MODULATE, 1, true);
		R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
	}
	R_Mesh_PrepareVertices_Generic_Arrays(4, loadingscreenpic_vertex3f, NULL, loadingscreenpic_texcoord2f);
	R_SetupShader_Generic(Draw_GetPicTexture(loadingscreenpic), NULL, GL_MODULATE, 1, true);
	R_Mesh_Draw(0, 4, 0, 2, polygonelement3i, NULL, 0, polygonelement3s, NULL, 0);
	SCR_DrawLoadingStack();
}

static void SCR_DrawLoadingScreen_SharedFinish (qboolean clear)
{
	R_Mesh_Finish();
	// refresh
	VID_Finish();
}

void SCR_UpdateLoadingScreen (qboolean clear)
{
	keydest_t	old_key_dest;
	int			old_key_consoleactive;

	// don't do anything if not initialized yet
	if (vid_hidden || cls.state == ca_dedicated)
		return;

	if(!scr_loadingscreen_background.integer)
		clear = true;
	
	if(loadingscreendone)
		clear |= loadingscreencleared;

	if(!loadingscreendone)
		loadingscreenpic_number = rand() % (scr_loadingscreen_count.integer > 1 ? scr_loadingscreen_count.integer : 1);

	if(clear)
	        SCR_ClearLoadingScreenTexture();
	else if(!loadingscreendone)
	        SCR_SetLoadingScreenTexture();

	if(!loadingscreendone)
	{
		loadingscreendone = true;
		loadingscreenheight = 0;
	}
	loadingscreencleared = clear;

	if (qglDrawBuffer)
		qglDrawBuffer(GL_BACK);
	SCR_DrawLoadingScreen_SharedSetup(clear);
	if (vid.stereobuffer)
	{
		qglDrawBuffer(GL_BACK_LEFT);
		SCR_DrawLoadingScreen(clear);
		qglDrawBuffer(GL_BACK_RIGHT);
		SCR_DrawLoadingScreen(clear);
	}
	else
	{
		if (qglDrawBuffer)
			qglDrawBuffer(GL_BACK);
		SCR_DrawLoadingScreen(clear);
	}
	SCR_DrawLoadingScreen_SharedFinish(clear);

	// this goes into the event loop, and should prevent unresponsive cursor on vista
	old_key_dest = key_dest;
	old_key_consoleactive = key_consoleactive;
	key_dest = key_void;
	key_consoleactive = false;
	Key_EventQueue_Block(); Sys_SendKeyEvents();
	key_dest = old_key_dest;
	key_consoleactive = old_key_consoleactive;
}

qboolean R_Stereo_ColorMasking(void)
{
	return r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer;
}

qboolean R_Stereo_Active(void)
{
	return (vid.stereobuffer || r_stereo_sidebyside.integer || r_stereo_horizontal.integer || r_stereo_vertical.integer || R_Stereo_ColorMasking());
}

extern cvar_t cl_minfps;
extern cvar_t cl_minfps_fade;
extern cvar_t cl_minfps_qualitymax;
extern cvar_t cl_minfps_qualitymin;
extern cvar_t cl_minfps_qualitypower;
extern cvar_t cl_minfps_qualityscale;
extern cvar_t r_viewscale_fpsscaling;
static double cl_updatescreen_rendertime = 0;
static double cl_updatescreen_quality = 1;
extern void Sbar_ShowFPS_Update(void);
void CL_UpdateScreen(void)
{
	vec3_t vieworigin;
	double rendertime1;
	double drawscreenstart;
	float conwidth, conheight;
	float f;
	r_viewport_t viewport;

	Sbar_ShowFPS_Update();

	if (!scr_initialized || !con_initialized || !scr_refresh.integer)
		return;				// not initialized yet

	loadingscreendone = false;

	if(gamemode == GAME_NEXUIZ || gamemode == GAME_XONOTIC)
	{
		// play a bit with the palette (experimental)
		palette_rgb_pantscolormap[15][0] = (unsigned char) (128 + 127 * sin(cl.time / exp(1.0f) + 0.0f*M_PI/3.0f));
		palette_rgb_pantscolormap[15][1] = (unsigned char) (128 + 127 * sin(cl.time / exp(1.0f) + 2.0f*M_PI/3.0f));
		palette_rgb_pantscolormap[15][2] = (unsigned char) (128 + 127 * sin(cl.time / exp(1.0f) + 4.0f*M_PI/3.0f));
		palette_rgb_shirtcolormap[15][0] = (unsigned char) (128 + 127 * sin(cl.time /  M_PI  + 5.0f*M_PI/3.0f));
		palette_rgb_shirtcolormap[15][1] = (unsigned char) (128 + 127 * sin(cl.time /  M_PI  + 3.0f*M_PI/3.0f));
		palette_rgb_shirtcolormap[15][2] = (unsigned char) (128 + 127 * sin(cl.time /  M_PI  + 1.0f*M_PI/3.0f));
		memcpy(palette_rgb_pantsscoreboard[15], palette_rgb_pantscolormap[15], sizeof(*palette_rgb_pantscolormap));
		memcpy(palette_rgb_shirtscoreboard[15], palette_rgb_shirtcolormap[15], sizeof(*palette_rgb_shirtcolormap));
	}

	if (vid_hidden)
	{
		VID_Finish();
		return;
	}

	rendertime1 = Sys_DoubleTime();

	conwidth = bound(160, vid_conwidth.value, 32768);
	conheight = bound(90, vid_conheight.value, 24576);
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

	Matrix4x4_OriginFromMatrix(&r_refdef.view.matrix, vieworigin);
	R_HDR_UpdateIrisAdaptation(vieworigin);

	r_refdef.view.colormask[0] = 1;
	r_refdef.view.colormask[1] = 1;
	r_refdef.view.colormask[2] = 1;

	SCR_SetUpToDrawConsole();

	if (qglDrawBuffer)
	{
		CHECKGLERROR
		qglDrawBuffer(GL_BACK);CHECKGLERROR
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

	R_Viewport_InitOrtho(&viewport, &identitymatrix, 0, 0, vid.width, vid.height, 0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100, NULL);
	R_Mesh_ResetRenderTargets();
	R_SetViewport(&viewport);
	GL_ScissorTest(false);
	GL_ColorMask(1,1,1,1);
	GL_DepthMask(true);

	R_ClearScreen(false);
	r_refdef.view.clear = false;
	r_refdef.view.isoverlay = false;
	f = pow((float)cl_updatescreen_quality, cl_minfps_qualitypower.value) * cl_minfps_qualityscale.value;
	r_refdef.view.quality = bound(cl_minfps_qualitymin.value, f, cl_minfps_qualitymax.value);

	if (qglPolygonStipple)
	{
		if(scr_stipple.integer)
		{
			GLubyte stipple[128];
			int i, s, width, parts;
			static int frame = 0;
			++frame;
	
			s = scr_stipple.integer;
			parts = (s & 007);
			width = (s & 070) >> 3;
	
			qglEnable(GL_POLYGON_STIPPLE);CHECKGLERROR // 0x0B42
			for(i = 0; i < 128; ++i)
			{
				int line = i/4;
				stipple[i] = (((line >> width) + frame) & ((1 << parts) - 1)) ? 0x00 : 0xFF;
			}
			qglPolygonStipple(stipple);CHECKGLERROR
		}
		else
		{
			qglDisable(GL_POLYGON_STIPPLE);CHECKGLERROR
		}
	}

	if (r_viewscale_fpsscaling.integer)
		GL_Finish();
	drawscreenstart = Sys_DoubleTime();
	if (R_Stereo_Active())
	{
		r_stereo_side = 0;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.view.colormask[0] = 1;
			r_refdef.view.colormask[1] = 0;
			r_refdef.view.colormask[2] = 0;
		}

		if (vid.stereobuffer)
			qglDrawBuffer(GL_BACK_RIGHT);

		SCR_DrawScreen();

		r_stereo_side = 1;

		if (r_stereo_redblue.integer || r_stereo_redgreen.integer || r_stereo_redcyan.integer)
		{
			r_refdef.view.colormask[0] = 0;
			r_refdef.view.colormask[1] = r_stereo_redcyan.integer || r_stereo_redgreen.integer;
			r_refdef.view.colormask[2] = r_stereo_redcyan.integer || r_stereo_redblue.integer;
		}

		if (vid.stereobuffer)
			qglDrawBuffer(GL_BACK_LEFT);

		SCR_DrawScreen();
	}
	else
		SCR_DrawScreen();
	if (r_viewscale_fpsscaling.integer)
		GL_Finish();
	r_refdef.lastdrawscreentime = Sys_DoubleTime() - drawscreenstart;

	SCR_CaptureVideo();

	if (qglFlush)
		qglFlush(); // FIXME: should we really be using qglFlush here?

	// quality adjustment according to render time
	cl_updatescreen_rendertime += ((Sys_DoubleTime() - rendertime1) - cl_updatescreen_rendertime) * bound(0, cl_minfps_fade.value, 1);
	if (cl_minfps.value > 0 && cl_updatescreen_rendertime > 0 && !cls.timedemo && (!cls.capturevideo.active || !cls.capturevideo.realtime))
		cl_updatescreen_quality = 1 / (cl_updatescreen_rendertime * cl_minfps.value);
	else
		cl_updatescreen_quality = 1;

	if (!vid_activewindow)
		VID_SetMouse(false, false, false);
	else if (key_consoleactive)
		VID_SetMouse(vid.fullscreen, false, false);
	else if (key_dest == key_menu_grabbed)
		VID_SetMouse(true, vid_mouse.integer && !in_client_mouse && !vid_touchscreen.integer, !vid_touchscreen.integer);
	else if (key_dest == key_menu)
		VID_SetMouse(vid.fullscreen, vid_mouse.integer && !in_client_mouse && !vid_touchscreen.integer, !vid_touchscreen.integer);
	else
		VID_SetMouse(vid.fullscreen, vid_mouse.integer && !cl.csqc_wantsmousemove && cl_prydoncursor.integer <= 0 && (!cls.demoplayback || cl_demo_mousegrab.integer) && !vid_touchscreen.integer, !vid_touchscreen.integer);

	VID_Finish();
}

void CL_Screen_NewMap(void)
{
}
