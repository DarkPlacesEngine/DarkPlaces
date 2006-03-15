
#include "quakedef.h"
#include "cl_video.h"
#include "jpeg.h"
#include "cl_collision.h"
#include "csprogs.h"

cvar_t scr_viewsize = {CVAR_SAVE, "viewsize","100", "how large the view should be, 110 disables inventory bar, 120 disables status bar"};
cvar_t scr_fov = {CVAR_SAVE, "fov","90", "field of vision, 1-170 degrees, default 90, some players use 110-130"};	// 1 - 170
cvar_t scr_conspeed = {CVAR_SAVE, "scr_conspeed","900", "speed of console open/close"}; // LordHavoc: quake used 300
cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1", "opacity of console background"};
cvar_t scr_conbrightness = {CVAR_SAVE, "scr_conbrightness", "0.2", "brightness of console background (0 = black, 1 = image)"};
cvar_t scr_conforcewhiledisconnected = {CVAR_SAVE, "scr_conforcewhiledisconnected", "1", "forces fullscreen console while disconnected"};
cvar_t scr_centertime = {0, "scr_centertime","2", "how long centerprint messages show"};
cvar_t scr_showram = {CVAR_SAVE, "showram","1", "show ram icon if low on surface cache memory (not used)"};
cvar_t scr_showturtle = {CVAR_SAVE, "showturtle","0", "show turtle icon when framerate is too low (not used)"};
cvar_t scr_showpause = {CVAR_SAVE, "showpause","1" "show pause icon when game is paused"};
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
cvar_t r_textshadow = {0, "r_textshadow", "0" "draws a shadow on all text to improve readability"};
cvar_t r_letterbox = {0, "r_letterbox", "0", "reduces vertical height of view to simulate a letterboxed movie effect (can be used by mods for cutscenes)"};

int jpeg_supported = false;

qboolean	scr_initialized;		// ready to draw

float		scr_con_current;

extern int	con_vislines;

void DrawCrosshair(int num);
static void SCR_ScreenShot_f (void);
static void R_Envmap_f (void);

// backend
void R_ClearScreen(void);

// color tag printing
static vec4_t string_colors[] =
{
	// Quake3 colors
	// LordHavoc: why on earth is cyan before magenta in Quake3?
	// LordHavoc: note: Doom3 uses white for [0] and [7]
	{0.0, 0.0, 0.0, 1.0}, // black
	{1.0, 0.0, 0.0, 1.0}, // red
	{0.0, 1.0, 0.0, 1.0}, // green
	{1.0, 1.0, 0.0, 1.0}, // yellow
	{0.0, 0.0, 1.0, 1.0}, // blue
	{0.0, 1.0, 1.0, 1.0}, // cyan
	{1.0, 0.0, 1.0, 1.0}, // magenta
	{1.0, 1.0, 1.0, 1.0}, // white
	// [515]'s BX_COLOREDTEXT extension
	{1.0, 1.0, 1.0, 0.5}, // half transparent
	{0.5, 0.5, 0.5, 1.0}  // half brightness
	// Black's color table
	//{1.0, 1.0, 1.0, 1.0},
	//{1.0, 0.0, 0.0, 1.0},
	//{0.0, 1.0, 0.0, 1.0},
	//{0.0, 0.0, 1.0, 1.0},
	//{1.0, 1.0, 0.0, 1.0},
	//{0.0, 1.0, 1.0, 1.0},
	//{1.0, 0.0, 1.0, 1.0},
	//{0.1, 0.1, 0.1, 1.0}
};

#define STRING_COLORS_COUNT	(sizeof(string_colors) / sizeof(vec4_t))

// color is read and changed in the end
void DrawQ_ColoredString( float x, float y, const char *text, int maxlen, float scalex, float scaley, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor )
{
	vec_t *color;
	int len;
	int colorindex;
	const char *start, *current;

	if( !outcolor || *outcolor == -1 ) {
		colorindex = STRING_COLOR_DEFAULT;
	} else {
		colorindex = *outcolor;
	}
	color = string_colors[colorindex];

	if( maxlen < 1)
		len = (int)strlen( text );
	else
		len = min( maxlen, (int) strlen( text ) );

	start = current = text;
	while( len > 0 ) {
		// check for color control char
		if( *current == STRING_COLOR_TAG ) {
			// get next char
			current++;
			len--;
			if( len == 0 ) {
				break;
			}
			// display the tag char?
			if( *current == STRING_COLOR_TAG ) {
				// only display one of the two
				start = current;
				// get the next char
				current++;
				len--;
			} else if( '0' <= *current && *current <= '9' ) {
				colorindex = 0;
				do {
					colorindex = colorindex * 10 + (*current - '0');
					// only read as long as it makes a valid index
					if( colorindex >= (int)STRING_COLORS_COUNT ) {
						// undo the last operation
						colorindex /= 10;
						break;
					}
					current++;
					len--;
				} while( len > 0 && '0' <= *current && *current <= '9' );
				// set the color
				color = string_colors[colorindex];
				// we jump over the color tag
				start = current;
			}
		}
		// go on and read normal text in until the next control char
		while( len > 0 && *current != STRING_COLOR_TAG ) {
			current++;
			len--;
		}
		// display the text
		if( start != current ) {
			// draw the string
			DrawQ_String( x, y, start, current - start, scalex, scaley, basered * color[0], basegreen * color[1], baseblue * color[2], basealpha * color[3], flags );
			// update x to be at the new start position
			x += (current - start) * scalex;
			// set start accordingly
			start = current;
		}
	}

	// return the last colorindex
	if( outcolor ) {
		*outcolor = colorindex;
	}
}

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

	DrawQ_Pic (0, 0, Draw_CachePic("gfx/turtle", false), 0, 0, 1, 1, 1, 1, 0);
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

	DrawQ_Pic (64, 0, Draw_CachePic("gfx/net", false), 0, 0, 1, 1, 1, 1, 0);
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

	Con_CheckResize ();

	if (key_dest == key_game && cls.signon != SIGNONS && scr_conforcewhiledisconnected.integer)
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

	if (r_speeds.integer < 2 || !r_timereport_active || r_showtrispass)
		return;

	qglFinish();
	r_timereport_temp = r_timereport_current;
	r_timereport_current = Sys_DoubleTime();
	t = (int) ((r_timereport_current - r_timereport_temp) * 1000000.0);

	dpsnprintf(tempbuf, sizeof(tempbuf), "%8i %-11s", t, desc);
	length = (int)strlen(tempbuf);
	if (speedstringcount + length > (vid_conwidth.integer / 8))
	{
		strlcat(r_speeds_string, "\n", sizeof(r_speeds_string));
		speedstringcount = 0;
	}
	// skip the space at the beginning if it's the first on the line
	if (speedstringcount == 0)
	{
		strlcat(r_speeds_string, tempbuf + 1, sizeof(r_speeds_string));
		speedstringcount = length - 1;
	}
	else
	{
		strlcat(r_speeds_string, tempbuf, sizeof(r_speeds_string));
		speedstringcount += length;
	}
}

void R_TimeReport_Frame(void)
{
	int i, j, lines, y;

	if (r_speeds_string[0])
	{
		if (r_timereport_active)
			R_TimeReport("total");

		r_timereport_current = r_timereport_start;
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
			r_timereport_start = Sys_DoubleTime();
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
	Cvar_RegisterVariable (&r_textshadow);
	Cvar_RegisterVariable (&r_letterbox);

	Cmd_AddCommand ("sizeup",SCR_SizeUp_f, "increase view size (increases viewsize cvar)");
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f, "decrease view size (decreases viewsize cvar)");
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f, "takes a screenshot of the next rendered frame");
	Cmd_AddCommand ("envmap", R_Envmap_f, "render a cubemap (skybox) of the current scene");

	scr_initialized = true;
}

void DrawQ_Clear(void)
{
	r_refdef.drawqueuesize = 0;
}

static int picelements[6] = {0, 1, 2, 0, 2, 3};
void DrawQ_Pic(float x, float y, cachepic_t *pic, float width, float height, float red, float green, float blue, float alpha, int flags)
{
	DrawQ_SuperPic(x,y,pic,width,height,0,0,red,green,blue,alpha,1,0,red,green,blue,alpha,0,1,red,green,blue,alpha,1,1,red,green,blue,alpha,flags);
}

void DrawQ_String_Real(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags)
{
	int size, len;
	drawqueue_t *dq;
	char *out;
	if (alpha < (1.0f / 255.0f))
		return;
	if (maxlen < 1)
		len = (int)strlen(string);
	else
		for (len = 0;len < maxlen && string[len];len++);
	for (;len > 0 && string[0] == ' ';string++, x += scalex, len--);
	for (;len > 0 && string[len - 1] == ' ';len--);
	if (len < 1)
		return;
	if (x >= vid_conwidth.integer || y >= vid_conheight.integer || x < (-scalex * len) || y < (-scaley))
		return;
	size = sizeof(*dq) + ((len + 1 + 3) & ~3);
	if (r_refdef.drawqueuesize + size > r_refdef.maxdrawqueuesize)
		return;
	red = bound(0, red, 1);
	green = bound(0, green, 1);
	blue = bound(0, blue, 1);
	alpha = bound(0, alpha, 1);
	dq = (drawqueue_t *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_STRING;
	dq->flags = flags;
	dq->color = ((unsigned int) (red * 255.0f) << 24) | ((unsigned int) (green * 255.0f) << 16) | ((unsigned int) (blue * 255.0f) << 8) | ((unsigned int) (alpha * 255.0f));
	dq->x = x;
	dq->y = y;
	dq->scalex = scalex;
	dq->scaley = scaley;
	out = (char *)(dq + 1);
	memcpy(out, string, len);
	out[len] = 0;
	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_String(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags)
{
	if (r_textshadow.integer)
		DrawQ_String_Real(x+scalex*0.25,y+scaley*0.25,string,maxlen,scalex,scaley,0,0,0,alpha*0.8,flags);

	DrawQ_String_Real(x,y,string,maxlen,scalex,scaley,red,green,blue,alpha,flags);
}

void DrawQ_SuperPic(float x, float y, cachepic_t *pic, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags)
{
	float floats[36];
	drawqueuemesh_t mesh;
	memset(&mesh, 0, sizeof(mesh));
	if (pic)
	{
		if (width == 0)
			width = pic->width;
		if (height == 0)
			height = pic->height;
		mesh.texture = pic->tex;
	}
	mesh.num_triangles = 2;
	mesh.num_vertices = 4;
	mesh.data_element3i = picelements;
	mesh.data_vertex3f = floats;
	mesh.data_texcoord2f = floats + 12;
	mesh.data_color4f = floats + 20;
	memset(floats, 0, sizeof(floats));
	mesh.data_vertex3f[0] = mesh.data_vertex3f[9] = x;
	mesh.data_vertex3f[1] = mesh.data_vertex3f[4] = y;
	mesh.data_vertex3f[3] = mesh.data_vertex3f[6] = x + width;
	mesh.data_vertex3f[7] = mesh.data_vertex3f[10] = y + height;
	mesh.data_texcoord2f[0] = s1;mesh.data_texcoord2f[1] = t1;mesh.data_color4f[ 0] = r1;mesh.data_color4f[ 1] = g1;mesh.data_color4f[ 2] = b1;mesh.data_color4f[ 3] = a1;
	mesh.data_texcoord2f[2] = s2;mesh.data_texcoord2f[3] = t2;mesh.data_color4f[ 4] = r2;mesh.data_color4f[ 5] = g2;mesh.data_color4f[ 6] = b2;mesh.data_color4f[ 7] = a2;
	mesh.data_texcoord2f[4] = s4;mesh.data_texcoord2f[5] = t4;mesh.data_color4f[ 8] = r4;mesh.data_color4f[ 9] = g4;mesh.data_color4f[10] = b4;mesh.data_color4f[11] = a4;
	mesh.data_texcoord2f[6] = s3;mesh.data_texcoord2f[7] = t3;mesh.data_color4f[12] = r3;mesh.data_color4f[13] = g3;mesh.data_color4f[14] = b3;mesh.data_color4f[15] = a3;
	DrawQ_Mesh (&mesh, flags);
}

void DrawQ_Mesh (drawqueuemesh_t *mesh, int flags)
{
	int size;
	void *p;
	drawqueue_t *dq;
	drawqueuemesh_t *m;
	size = sizeof(*dq);
	size += sizeof(drawqueuemesh_t);
	size += sizeof(int[3]) * mesh->num_triangles;
	size += sizeof(float[3]) * mesh->num_vertices;
	size += sizeof(float[2]) * mesh->num_vertices;
	size += sizeof(float[4]) * mesh->num_vertices;
	if (r_refdef.drawqueuesize + size > r_refdef.maxdrawqueuesize)
		return;
	dq = (drawqueue_t *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_MESH;
	dq->flags = flags;
	dq->color = 0;
	dq->x = 0;
	dq->y = 0;
	dq->scalex = 0;
	dq->scaley = 0;
	p = (void *)(dq + 1);
	m = (drawqueuemesh_t *)p;p = (unsigned char*)p + sizeof(drawqueuemesh_t);
	m->num_triangles = mesh->num_triangles;
	m->num_vertices = mesh->num_vertices;
	m->texture = mesh->texture;
	m->data_element3i  = (int *)p;memcpy(m->data_element3i , mesh->data_element3i , m->num_triangles * sizeof(int[3]));p = (unsigned char*)p + m->num_triangles * sizeof(int[3]);
	m->data_vertex3f   = (float *)p;memcpy(m->data_vertex3f  , mesh->data_vertex3f  , m->num_vertices * sizeof(float[3]));p = (unsigned char*)p + m->num_vertices * sizeof(float[3]);
	m->data_texcoord2f = (float *)p;memcpy(m->data_texcoord2f, mesh->data_texcoord2f, m->num_vertices * sizeof(float[2]));p = (unsigned char*)p + m->num_vertices * sizeof(float[2]);
	m->data_color4f    = (float *)p;memcpy(m->data_color4f   , mesh->data_color4f   , m->num_vertices * sizeof(float[4]));p = (unsigned char*)p + m->num_vertices * sizeof(float[4]);
	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_LineLoop (drawqueuemesh_t *mesh, int flags)
{
	int size;
	void *p;
	drawqueue_t *dq;
	drawqueuemesh_t *m;
	size = sizeof(*dq);
	size += sizeof(drawqueuemesh_t);
	size += sizeof(int[3]) * mesh->num_triangles;
	size += sizeof(float[3]) * mesh->num_vertices;
	size += sizeof(float[2]) * mesh->num_vertices;
	size += sizeof(float[4]) * mesh->num_vertices;
	if (r_refdef.drawqueuesize + size > r_refdef.maxdrawqueuesize)
		return;
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_LINES;
	dq->flags = flags;
	dq->color = 0;
	dq->x = 0;
	dq->y = 0;
	dq->scalex = 0;
	dq->scaley = 0;
	p = (void *)(dq + 1);
	m = p;p = (unsigned char*)p + sizeof(drawqueuemesh_t);
	m->num_triangles = mesh->num_triangles;
	m->num_vertices = mesh->num_vertices;
	m->texture = mesh->texture;
	m->data_element3i  = p;memcpy(m->data_element3i , mesh->data_element3i , m->num_triangles * sizeof(int[3]));p = (unsigned char*)p + m->num_triangles * sizeof(int[3]);
	m->data_vertex3f   = p;memcpy(m->data_vertex3f  , mesh->data_vertex3f  , m->num_vertices * sizeof(float[3]));p = (unsigned char*)p + m->num_vertices * sizeof(float[3]);
	m->data_texcoord2f = p;memcpy(m->data_texcoord2f, mesh->data_texcoord2f, m->num_vertices * sizeof(float[2]));p = (unsigned char*)p + m->num_vertices * sizeof(float[2]);
	m->data_color4f    = p;memcpy(m->data_color4f   , mesh->data_color4f   , m->num_vertices * sizeof(float[4]));p = (unsigned char*)p + m->num_vertices * sizeof(float[4]);
	r_refdef.drawqueuesize += dq->size;
}

//LordHavoc: FIXME: this is nasty!
void DrawQ_LineWidth (float width)
{
	drawqueue_t *dq;
	static int linewidth = 1;
	if(width == linewidth)
		return;
	linewidth = width;
	if(r_refdef.drawqueuesize + (int)sizeof(*dq) > r_refdef.maxdrawqueuesize)
	{
		Con_DPrint("DrawQueue full !\n");
		return;
	}
	dq = (void*) (r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = sizeof(*dq);
	dq->command = DRAWQUEUE_LINEWIDTH;
	dq->x = width;

	r_refdef.drawqueuesize += dq->size;
}

//[515]: this is old, delete
void DrawQ_Line (float width, float x1, float y1, float x2, float y2, float r, float g, float b, float alpha, int flags)
{
	drawqueue_t *dq;
	if(width > 0)
		DrawQ_LineWidth(width);
	if(r_refdef.drawqueuesize + (int)sizeof(*dq) > r_refdef.maxdrawqueuesize)
	{
		Con_DPrint("DrawQueue full !\n");
		return;
	}
	dq = (void*) (r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = sizeof(*dq);
	dq->command = DRAWQUEUE_LINES;
	dq->x = x1;
	dq->y = y1;
	dq->scalex = x2;
	dq->scaley = y2;
	dq->flags = flags;
	dq->color =  ((unsigned int) (r * 255.0f) << 24) | ((unsigned int) (g * 255.0f) << 16) | ((unsigned int) (b * 255.0f) << 8) | ((unsigned int) (alpha * 255.0f));

	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_SetClipArea(float x, float y, float width, float height)
{
	drawqueue_t * dq;
	if(r_refdef.drawqueuesize + (int)sizeof(*dq) > r_refdef.maxdrawqueuesize)
	{
		Con_DPrint("DrawQueue full !\n");
		return;
	}
	dq = (drawqueue_t *) (r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = sizeof(*dq);
	dq->command = DRAWQUEUE_SETCLIP;
	dq->x = x;
	dq->y = y;
	dq->scalex = width;
	dq->scaley = height;
	dq->flags = 0;
	dq->color = 0;

	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_ResetClipArea(void)
{
	drawqueue_t *dq;
	if(r_refdef.drawqueuesize + (int)sizeof(*dq) > r_refdef.maxdrawqueuesize)
	{
		Con_DPrint("DrawQueue full !\n");
		return;
	}
	dq = (drawqueue_t *) (r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = sizeof(*dq);
	dq->command = DRAWQUEUE_RESETCLIP;
	dq->x = 0;
	dq->y = 0;
	dq->scalex = 0;
	dq->scaley = 0;
	dq->flags = 0;
	dq->color = 0;

	r_refdef.drawqueuesize += dq->size;
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

typedef enum capturevideoformat_e
{
	CAPTUREVIDEOFORMAT_TARGA,
	CAPTUREVIDEOFORMAT_JPEG,
	CAPTUREVIDEOFORMAT_RAWRGB,
	CAPTUREVIDEOFORMAT_RAWYV12
}
capturevideoformat_t;

qboolean cl_capturevideo_active = false;
capturevideoformat_t cl_capturevideo_format;
static double cl_capturevideo_starttime = 0;
double cl_capturevideo_framerate = 0;
static int cl_capturevideo_soundrate = 0;
static int cl_capturevideo_frame = 0;
static unsigned char *cl_capturevideo_buffer = NULL;
static qfile_t *cl_capturevideo_videofile = NULL;
qfile_t *cl_capturevideo_soundfile = NULL;
static short cl_capturevideo_rgbtoyuvscaletable[3][3][256];
static unsigned char cl_capturevideo_yuvnormalizetable[3][256];
//static unsigned char cl_capturevideo_rgbgammatable[3][256];

void SCR_CaptureVideo_BeginVideo(void)
{
	double gamma, g;
	unsigned int i;
	unsigned char out[44];
	if (cl_capturevideo_active)
		return;
	// soundrate is figured out on the first SoundFrame
	cl_capturevideo_active = true;
	cl_capturevideo_starttime = Sys_DoubleTime();
	cl_capturevideo_framerate = bound(1, cl_capturevideo_fps.value, 1000);
	cl_capturevideo_soundrate = 0;
	cl_capturevideo_frame = 0;
	cl_capturevideo_buffer = (unsigned char *)Mem_Alloc(tempmempool, vid.width * vid.height * (3+3+3) + 18);
	gamma = 1.0/scr_screenshot_gamma.value;

	/*
	for (i = 0;i < 256;i++)
	{
		unsigned char j = (unsigned char)bound(0, 255*pow(i/255.0, gamma), 255);
		cl_capturevideo_rgbgammatable[0][i] = j;
		cl_capturevideo_rgbgammatable[1][i] = j;
		cl_capturevideo_rgbgammatable[2][i] = j;
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
		cl_capturevideo_rgbtoyuvscaletable[0][0][i] = (short)(g *  0.299);
		cl_capturevideo_rgbtoyuvscaletable[0][1][i] = (short)(g *  0.587);
		cl_capturevideo_rgbtoyuvscaletable[0][2][i] = (short)(g *  0.114);
		// Cb weights from RGB
		cl_capturevideo_rgbtoyuvscaletable[1][0][i] = (short)(g * -0.169);
		cl_capturevideo_rgbtoyuvscaletable[1][1][i] = (short)(g * -0.332);
		cl_capturevideo_rgbtoyuvscaletable[1][2][i] = (short)(g *  0.500);
		// Cr weights from RGB
		cl_capturevideo_rgbtoyuvscaletable[2][0][i] = (short)(g *  0.500);
		cl_capturevideo_rgbtoyuvscaletable[2][1][i] = (short)(g * -0.419);
		cl_capturevideo_rgbtoyuvscaletable[2][2][i] = (short)(g * -0.0813);
		// range reduction of YCbCr to valid signal range
		cl_capturevideo_yuvnormalizetable[0][i] = 16 + i * (236-16) / 256;
		cl_capturevideo_yuvnormalizetable[1][i] = 16 + i * (240-16) / 256;
		cl_capturevideo_yuvnormalizetable[2][i] = 16 + i * (240-16) / 256;
	}

	if (cl_capturevideo_rawrgb.integer)
	{
		cl_capturevideo_format = CAPTUREVIDEOFORMAT_RAWRGB;
		cl_capturevideo_videofile = FS_Open ("video/dpvideo.rgb", "wb", false, true);
	}
	else if (cl_capturevideo_rawyv12.integer)
	{
		cl_capturevideo_format = CAPTUREVIDEOFORMAT_RAWYV12;
		cl_capturevideo_videofile = FS_Open ("video/dpvideo.yv12", "wb", false, true);
	}
	else if (scr_screenshot_jpeg.integer)
	{
		cl_capturevideo_format = CAPTUREVIDEOFORMAT_JPEG;
		cl_capturevideo_videofile = NULL;
	}
	else
	{
		cl_capturevideo_format = CAPTUREVIDEOFORMAT_TARGA;
		cl_capturevideo_videofile = NULL;
	}

	if (cl_capturevideo_sound.integer)
	{
		cl_capturevideo_soundfile = FS_Open ("video/dpvideo.wav", "wb", false, true);
		// wave header will be filled out when video ends
		memset(out, 0, 44);
		FS_Write (cl_capturevideo_soundfile, out, 44);
	}
	else
		cl_capturevideo_soundfile = NULL;
}

void SCR_CaptureVideo_EndVideo(void)
{
	int i, n;
	unsigned char out[44];
	if (!cl_capturevideo_active)
		return;
	cl_capturevideo_active = false;

	if (cl_capturevideo_videofile)
	{
		FS_Close(cl_capturevideo_videofile);
		cl_capturevideo_videofile = NULL;
	}

	// finish the wave file
	if (cl_capturevideo_soundfile)
	{
		i = (int)FS_Tell (cl_capturevideo_soundfile);
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
		n = cl_capturevideo_soundrate;
		out[24] = (n) & 0xFF;
		out[25] = (n >> 8) & 0xFF;
		out[26] = (n >> 16) & 0xFF;
		out[27] = (n >> 24) & 0xFF;
		// bytes per second (rate * channels * bytes per channel)
		n = cl_capturevideo_soundrate * 2 * 2;
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
		FS_Seek (cl_capturevideo_soundfile, 0, SEEK_SET);
		FS_Write (cl_capturevideo_soundfile, out, 44);
		FS_Close (cl_capturevideo_soundfile);
		cl_capturevideo_soundfile = NULL;
	}

	if (cl_capturevideo_buffer)
	{
		Mem_Free (cl_capturevideo_buffer);
		cl_capturevideo_buffer = NULL;
	}

	cl_capturevideo_starttime = 0;
	cl_capturevideo_framerate = 0;
	cl_capturevideo_frame = 0;
}

qboolean SCR_CaptureVideo_VideoFrame(int newframenum)
{
	int x = 0, y = 0, width = vid.width, height = vid.height;
	unsigned char *b, *out;
	char filename[32];
	int outoffset = (width/2)*(height/2);
	//return SCR_ScreenShot(filename, cl_capturevideo_buffer, cl_capturevideo_buffer + vid.width * vid.height * 3, cl_capturevideo_buffer + vid.width * vid.height * 6, 0, 0, vid.width, vid.height, false, false, false, jpeg, true);
	// speed is critical here, so do saving as directly as possible
	switch (cl_capturevideo_format)
	{
	case CAPTUREVIDEOFORMAT_RAWYV12:
		// FIXME: width/height must be multiple of 2, enforce this?
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cl_capturevideo_buffer);
		CHECKGLERROR
		// process one line at a time, and CbCr every other line at 2 pixel intervals
		for (y = 0;y < height;y++)
		{
			// 1x1 Y
			for (b = cl_capturevideo_buffer + (height-1-y)*width*3, out = cl_capturevideo_buffer + width*height*3 + y*width, x = 0;x < width;x++, b += 3, out++)
				*out = cl_capturevideo_yuvnormalizetable[0][cl_capturevideo_rgbtoyuvscaletable[0][0][b[0]] + cl_capturevideo_rgbtoyuvscaletable[0][1][b[1]] + cl_capturevideo_rgbtoyuvscaletable[0][2][b[2]]];
			if ((y & 1) == 0)
			{
				// 2x2 Cb and Cr planes
#if 1
				// low quality, no averaging
				for (b = cl_capturevideo_buffer + (height-2-y)*width*3, out = cl_capturevideo_buffer + width*height*3 + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
				{
					// Cr
					out[0        ] = cl_capturevideo_yuvnormalizetable[2][cl_capturevideo_rgbtoyuvscaletable[2][0][b[0]] + cl_capturevideo_rgbtoyuvscaletable[2][1][b[1]] + cl_capturevideo_rgbtoyuvscaletable[2][2][b[2]] + 128];
					// Cb
					out[outoffset] = cl_capturevideo_yuvnormalizetable[1][cl_capturevideo_rgbtoyuvscaletable[1][0][b[0]] + cl_capturevideo_rgbtoyuvscaletable[1][1][b[1]] + cl_capturevideo_rgbtoyuvscaletable[1][2][b[2]] + 128];
				}
#else
				// high quality, averaging
				int inpitch = width*3;
				for (b = cl_capturevideo_buffer + (height-2-y)*width*3, out = cl_capturevideo_buffer + width*height*3 + width*height + (y/2)*(width/2), x = 0;x < width/2;x++, b += 6, out++)
				{
					int blockr, blockg, blockb;
					blockr = (b[0] + b[3] + b[inpitch+0] + b[inpitch+3]) >> 2;
					blockg = (b[1] + b[4] + b[inpitch+1] + b[inpitch+4]) >> 2;
					blockb = (b[2] + b[5] + b[inpitch+2] + b[inpitch+5]) >> 2;
					// Cr
					out[0        ] = cl_capturevideo_yuvnormalizetable[2][cl_capturevideo_rgbtoyuvscaletable[2][0][blockr] + cl_capturevideo_rgbtoyuvscaletable[2][1][blockg] + cl_capturevideo_rgbtoyuvscaletable[2][2][blockb] + 128];
					// Cb
					out[outoffset] = cl_capturevideo_yuvnormalizetable[1][cl_capturevideo_rgbtoyuvscaletable[1][0][blockr] + cl_capturevideo_rgbtoyuvscaletable[1][1][blockg] + cl_capturevideo_rgbtoyuvscaletable[1][2][blockb] + 128];
				}
#endif
			}
		}
		for (;cl_capturevideo_frame < newframenum;cl_capturevideo_frame++)
			if (!FS_Write (cl_capturevideo_videofile, cl_capturevideo_buffer + width*height*3, width*height+(width/2)*(height/2)*2))
				return false;
		return true;
	case CAPTUREVIDEOFORMAT_RAWRGB:
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cl_capturevideo_buffer);
		CHECKGLERROR
		for (;cl_capturevideo_frame < newframenum;cl_capturevideo_frame++)
			if (!FS_Write (cl_capturevideo_videofile, cl_capturevideo_buffer, width*height*3))
				return false;
		return true;
	case CAPTUREVIDEOFORMAT_JPEG:
		qglReadPixels (x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, cl_capturevideo_buffer);
		CHECKGLERROR
		for (;cl_capturevideo_frame < newframenum;cl_capturevideo_frame++)
		{
			sprintf(filename, "video/dp%06d.jpg", cl_capturevideo_frame);
			if (!JPEG_SaveImage_preflipped (filename, width, height, cl_capturevideo_buffer))
				return false;
		}
		return true;
	case CAPTUREVIDEOFORMAT_TARGA:
		//return Image_WriteTGARGB_preflipped (filename, width, height, cl_capturevideo_buffer, cl_capturevideo_buffer + vid.width * vid.height * 3, );
		memset (cl_capturevideo_buffer, 0, 18);
		cl_capturevideo_buffer[2] = 2;		// uncompressed type
		cl_capturevideo_buffer[12] = (width >> 0) & 0xFF;
		cl_capturevideo_buffer[13] = (width >> 8) & 0xFF;
		cl_capturevideo_buffer[14] = (height >> 0) & 0xFF;
		cl_capturevideo_buffer[15] = (height >> 8) & 0xFF;
		cl_capturevideo_buffer[16] = 24;	// pixel size
		qglReadPixels (x, y, width, height, GL_BGR, GL_UNSIGNED_BYTE, cl_capturevideo_buffer + 18);
		CHECKGLERROR
		for (;cl_capturevideo_frame < newframenum;cl_capturevideo_frame++)
		{
			sprintf(filename, "video/dp%06d.tga", cl_capturevideo_frame);
			if (!FS_WriteFile (filename, cl_capturevideo_buffer, width*height*3 + 18))
				return false;
		}
		return true;
	default:
		return false;
	}
}

void SCR_CaptureVideo_SoundFrame(unsigned char *bufstereo16le, size_t length, int rate)
{
	if (!cl_capturevideo_soundfile)
		return;
	cl_capturevideo_soundrate = rate;
	if (FS_Write (cl_capturevideo_soundfile, bufstereo16le, 4 * length) < (fs_offset_t)(4 * length))
	{
		Cvar_SetValueQuick(&cl_capturevideo, 0);
		Con_Printf("video sound saving failed on frame %i, out of disk space? stopping video capture.\n", cl_capturevideo_frame);
		SCR_CaptureVideo_EndVideo();
	}
}

void SCR_CaptureVideo(void)
{
	int newframenum;
	if (cl_capturevideo.integer && r_render.integer)
	{
		if (!cl_capturevideo_active)
			SCR_CaptureVideo_BeginVideo();
		if (cl_capturevideo_framerate != cl_capturevideo_fps.value)
		{
			Con_Printf("You can not change the video framerate while recording a video.\n");
			Cvar_SetValueQuick(&cl_capturevideo_fps, cl_capturevideo_framerate);
		}
		if (cl_capturevideo_soundfile)
		{
			// preserve sound sync by duplicating frames when running slow
			newframenum = (Sys_DoubleTime() - cl_capturevideo_starttime) * cl_capturevideo_framerate;
		}
		else
			newframenum = cl_capturevideo_frame + 1;
		// if falling behind more than one second, stop
		if (newframenum - cl_capturevideo_frame > (int)ceil(cl_capturevideo_framerate))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, your machine is too slow for this capture speed.\n", cl_capturevideo_frame);
			SCR_CaptureVideo_EndVideo();
			return;
		}
		// write frames
		if (!SCR_CaptureVideo_VideoFrame(newframenum))
		{
			Cvar_SetValueQuick(&cl_capturevideo, 0);
			Con_Printf("video saving failed on frame %i, out of disk space? stopping video capture.\n", cl_capturevideo_frame);
			SCR_CaptureVideo_EndVideo();
		}
	}
	else if (cl_capturevideo_active)
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
			DrawQ_Pic(showlmp[i].x, showlmp[i].y, Draw_CachePic(showlmp[i].pic, false), 0, 0, 1, 1, 1, 1, 0);
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}

void CL_SetupScreenSize(void)
{
	float conwidth, conheight;

	VID_UpdateGamma(false);

	conwidth = bound(320, vid_conwidth.value, 2048);
	conheight = bound(200, vid_conheight.value, 1536);
	if (vid_conwidth.value != conwidth)
		Cvar_SetValue("vid_conwidth", conwidth);
	if (vid_conheight.value != conheight)
		Cvar_SetValue("vid_conheight", conheight);

	vid_conwidth.integer = vid_conwidth.integer;
	vid_conheight.integer = vid_conheight.integer;

	SCR_SetUpToDrawConsole();
}

extern void R_Shadow_EditLights_DrawSelectedLightProperties(void);
void CL_UpdateScreen(void)
{
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

	r_refdef.colormask[0] = 1;
	r_refdef.colormask[1] = 1;
	r_refdef.colormask[2] = 1;

	SCR_CaptureVideo();

	if (r_timereport_active)
		R_TimeReport("other");

	CL_SetupScreenSize();

	DrawQ_Clear();

	if (r_timereport_active)
		R_TimeReport("setup");

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
	if (cls.signon == SIGNONS)
	{
		if (r_timereport_active)
			R_TimeReport("2d");
		R_TimeReport_Frame();
	}
	R_Shadow_EditLights_DrawSelectedLightProperties();

	if(!csqc_loaded)
		SCR_DrawConsole();

	SCR_DrawBrand();

	SCR_DrawDownload();

	SCR_UpdateScreen();
}

void CL_Screen_NewMap(void)
{
	SHOWLMP_clear();
}
