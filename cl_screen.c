
#include "quakedef.h"
#include "cl_video.h"
#include "jpeg.h"
#include "cl_collision.h"

cvar_t scr_viewsize = {CVAR_SAVE, "viewsize","100"};
cvar_t scr_fov = {CVAR_SAVE, "fov","90"};	// 10 - 170
cvar_t scr_conspeed = {CVAR_SAVE, "scr_conspeed","900"}; // LordHavoc: quake used 300
cvar_t scr_conalpha = {CVAR_SAVE, "scr_conalpha", "1"};
cvar_t scr_conbrightness = {CVAR_SAVE, "scr_conbrightness", "0.2"};
cvar_t scr_centertime = {0, "scr_centertime","2"};
cvar_t scr_showram = {CVAR_SAVE, "showram","1"};
cvar_t scr_showturtle = {CVAR_SAVE, "showturtle","0"};
cvar_t scr_showpause = {CVAR_SAVE, "showpause","1"};
cvar_t scr_printspeed = {0, "scr_printspeed","8"};
cvar_t scr_2dresolution = {CVAR_SAVE, "scr_2dresolution", "1"};
cvar_t scr_screenshot_jpeg = {CVAR_SAVE, "scr_screenshot_jpeg","0"};
cvar_t cl_avidemo = {0, "cl_avidemo", "0"};

qboolean	scr_initialized;		// ready to draw

float		scr_con_current;
float		scr_conlines;		// lines of console to display

int			clearconsole;
int			clearnotify;

qboolean	scr_drawloading = false;

static qbyte menuplyr_pixels[4096];

void DrawCrosshair(int num);
void V_CalcRefdef (void);
static void SCR_ScreenShot_f (void);
static void R_Envmap_f (void);

// backend
void R_ClearScreen(void);

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

	DrawQ_Pic (0, 0, "gfx/turtle.lmp", 0, 0, 1, 1, 1, 1, 0);
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

	DrawQ_Pic (64, 0, "gfx/net.lmp", 0, 0, 1, 1, 1, 1, 0);
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
void SCR_DrawLoading (void)
{
	cachepic_t	*pic;

	pic = Draw_CachePic ("gfx/loading.lmp");
	DrawQ_Pic ((vid.conwidth - pic->width)/2, (vid.conheight - pic->height)/2, "gfx/loading.lmp", 0, 0, 1, 1, 1, 1, 0);
}



//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	Con_CheckResize ();

	if (key_dest == key_game && cls.signon != SIGNONS)
		key_consoleactive |= KEY_CONSOLEACTIVE_FORCED;
	else
		key_consoleactive &= ~KEY_CONSOLEACTIVE_FORCED;

// decide on the height of the console
	if (key_consoleactive & KEY_CONSOLEACTIVE_FORCED)
		scr_conlines = vid.conheight; // full screen
	else if (key_consoleactive & KEY_CONSOLEACTIVE_USER)
		scr_conlines = vid.conheight/2;	// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conspeed.value)
	{
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
	else
		scr_con_current = scr_conlines;
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
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	if (scr_drawloading)
		return;

	S_StopAllSounds (true);

	scr_drawloading = true;
	CL_UpdateScreen ();
	scr_drawloading = true;
	CL_UpdateScreen ();
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

extern int c_rt_lights, c_rt_clears, c_rt_scissored;
extern int c_rt_shadowmeshes, c_rt_shadowtris, c_rt_lightmeshes, c_rt_lighttris;
extern int c_rtcached_shadowmeshes, c_rtcached_shadowtris;
void R_TimeReport_Start(void)
{
	r_timereport_active = r_speeds.integer && cls.signon == SIGNONS && cls.state == ca_connected;
	r_speeds_string[0] = 0;
	if (r_timereport_active)
	{
		speedstringcount = 0;
		AngleVectors (r_refdef.viewangles, vpn, NULL, NULL);
		sprintf(r_speeds_string,
			"org:'%+8.2f %+8.2f %+8.2f' ang:'%+4.0f %+4.0f %+4.0f' dir:'%+2.3f %+2.3f %+2.3f'\n"
			"world:%6i faces%6i nodes%6i leafs%6i dlitwalls\n"
			"%5i models%5i bmodels%5i sprites%6i particles%4i dlights\n"
			"%6i modeltris%6i meshs%6i meshtris\n",
			r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2], r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2], vpn[0], vpn[1], vpn[2],
			c_faces, c_nodes, c_leafs, c_light_polys,
			c_models, c_bmodels, c_sprites, c_particles, c_dlights,
			c_alias_polys, c_meshs, c_meshelements / 3);

		sprintf(r_speeds_string + strlen(r_speeds_string),
			"realtime lighting:%4i lights%4i clears%4i scissored\n"
			"dynamic: %6i shadowmeshes%6i shadowtris%6i lightmeshes%6i lighttris\n"
			"precomputed: %6i shadowmeshes%6i shadowtris\n",
			c_rt_lights, c_rt_clears, c_rt_scissored,
			c_rt_shadowmeshes, c_rt_shadowtris, c_rt_lightmeshes, c_rt_lighttris,
			c_rtcached_shadowmeshes, c_rtcached_shadowtris);

		c_alias_polys = 0;
		c_light_polys = 0;
		c_faces = 0;
		c_nodes = 0;
		c_leafs = 0;
		c_models = 0;
		c_bmodels = 0;
		c_sprites = 0;
		c_particles = 0;
		c_meshs = 0;
		c_meshelements = 0;

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
		y = vid.conheight - sb_lines - lines * 8;
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
	qpic_t *dat;

	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_conalpha);
	Cvar_RegisterVariable (&scr_conbrightness);
	Cvar_RegisterVariable (&scr_showram);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&scr_2dresolution);
	Cvar_RegisterVariable (&scr_screenshot_jpeg);
	Cvar_RegisterVariable (&cl_avidemo);

	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);

	scr_initialized = true;

	// HACK HACK HACK
	// load the image data for the player image in the config menu
	dat = (qpic_t *)FS_LoadFile ("gfx/menuplyr.lmp", false);
	if (!dat)
		Sys_Error("unable to load gfx/menuplyr.lmp");
	SwapPic (dat);

	if (dat->width*dat->height <= 4096)
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);
	else
		Con_Printf("gfx/menuplyr.lmp larger than 4k buffer");
	Mem_Free(dat);
}

void DrawQ_Clear(void)
{
	r_refdef.drawqueuesize = 0;
}

static int picelements[6] = {0, 1, 2, 0, 2, 3};
void DrawQ_Pic(float x, float y, char *picname, float width, float height, float red, float green, float blue, float alpha, int flags)
{
	DrawQ_SuperPic(x,y,picname,width,height,0,0,red,green,blue,alpha,1,0,red,green,blue,alpha,0,1,red,green,blue,alpha,1,1,red,green,blue,alpha,flags);
}

void DrawQ_String(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags)
{
	int size, len;
	drawqueue_t *dq;
	char *out;
	if (alpha < (1.0f / 255.0f))
		return;
	if (maxlen < 1)
		len = strlen(string);
	else
		for (len = 0;len < maxlen && string[len];len++);
	for (;len > 0 && string[0] == ' ';string++, x += scalex, len--);
	for (;len > 0 && string[len - 1] == ' ';len--);
	if (len < 1)
		return;
	if (x >= vid.conwidth || y >= vid.conheight || x < (-scalex * maxlen) || y < (-scaley))
		return;
	size = sizeof(*dq) + ((len + 1 + 3) & ~3);
	if (r_refdef.drawqueuesize + size > r_refdef.maxdrawqueuesize)
		return;
	red = bound(0, red, 1);
	green = bound(0, green, 1);
	blue = bound(0, blue, 1);
	alpha = bound(0, alpha, 1);
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
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

void DrawQ_Fill (float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags)
{
	DrawQ_SuperPic(x,y,NULL,w,h,0,0,red,green,blue,alpha,1,0,red,green,blue,alpha,0,1,red,green,blue,alpha,1,1,red,green,blue,alpha,flags);
}

void DrawQ_SuperPic(float x, float y, char *picname, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags)
{
	float floats[36];
	cachepic_t *pic;
	drawqueuemesh_t mesh;
	memset(&mesh, 0, sizeof(mesh));
	if (picname && picname[0])
	{
		pic = Draw_CachePic(picname);
		if (width == 0)
			width = pic->width;
		if (height == 0)
			height = pic->height;
		mesh.texture = pic->tex;
	}
	mesh.numtriangles = 2;
	mesh.numvertices = 4;
	mesh.element3i = picelements;
	mesh.vertex3f = floats;
	mesh.texcoord2f = floats + 12;
	mesh.color4f = floats + 20;
	memset(floats, 0, sizeof(floats));
	mesh.vertex3f[0] = mesh.vertex3f[9] = x;
	mesh.vertex3f[1] = mesh.vertex3f[4] = y;
	mesh.vertex3f[3] = mesh.vertex3f[6] = x + width;
	mesh.vertex3f[7] = mesh.vertex3f[10] = y + height;
	mesh.texcoord2f[0] = s1;mesh.texcoord2f[1] = t1;mesh.color4f[ 0] = r1;mesh.color4f[ 1] = g1;mesh.color4f[ 2] = b1;mesh.color4f[ 3] = a1;
	mesh.texcoord2f[2] = s2;mesh.texcoord2f[3] = t2;mesh.color4f[ 4] = r2;mesh.color4f[ 5] = g2;mesh.color4f[ 6] = b2;mesh.color4f[ 7] = a2;
	mesh.texcoord2f[4] = s4;mesh.texcoord2f[5] = t4;mesh.color4f[ 8] = r4;mesh.color4f[ 9] = g4;mesh.color4f[10] = b4;mesh.color4f[11] = a4;
	mesh.texcoord2f[6] = s3;mesh.texcoord2f[7] = t3;mesh.color4f[12] = r3;mesh.color4f[13] = g3;mesh.color4f[14] = b3;mesh.color4f[15] = a3;
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
	size += sizeof(int[3]) * mesh->numtriangles;
	size += sizeof(float[3]) * mesh->numvertices;
	size += sizeof(float[2]) * mesh->numvertices;
	size += sizeof(float[4]) * mesh->numvertices;
	if (r_refdef.drawqueuesize + size > r_refdef.maxdrawqueuesize)
		return;
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_MESH;
	dq->flags = flags;
	dq->color = 0;
	dq->x = 0;
	dq->y = 0;
	dq->scalex = 0;
	dq->scaley = 0;
	p = (void *)(dq + 1);
	m = p;(qbyte *)p += sizeof(drawqueuemesh_t);
	m->numtriangles = mesh->numtriangles;
	m->numvertices = mesh->numvertices;
	m->texture = mesh->texture;
	m->element3i  = p;memcpy(m->element3i , mesh->element3i , m->numtriangles * sizeof(int[3]));(qbyte *)p += m->numtriangles * sizeof(int[3]);
	m->vertex3f   = p;memcpy(m->vertex3f  , mesh->vertex3f  , m->numvertices * sizeof(float[3]));(qbyte *)p += m->numvertices * sizeof(float[3]);
	m->texcoord2f = p;memcpy(m->texcoord2f, mesh->texcoord2f, m->numvertices * sizeof(float[2]));(qbyte *)p += m->numvertices * sizeof(float[2]);
	m->color4f    = p;memcpy(m->color4f   , mesh->color4f   , m->numvertices * sizeof(float[4]));(qbyte *)p += m->numvertices * sizeof(float[4]);
	r_refdef.drawqueuesize += dq->size;
}

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
	int contents;

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

	r_refdef.width = bound(0, r_refdef.width, vid.realwidth);
	r_refdef.height = bound(0, r_refdef.height, vid.realheight);
	r_refdef.x = bound(0, r_refdef.x, vid.realwidth - r_refdef.width) + vid.realx;
	r_refdef.y = bound(0, r_refdef.y, vid.realheight - r_refdef.height) + vid.realy;

	// LordHavoc: viewzoom (zoom in for sniper rifles, etc)
	r_refdef.fov_x = scr_fov.value * cl.viewzoom;
	r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.width, r_refdef.height);

	if (cl.worldmodel)
	{
		Mod_CheckLoaded(cl.worldmodel);
		contents = CL_PointContents(r_refdef.vieworg);
		if (contents != CONTENTS_EMPTY && contents != CONTENTS_SOLID)
		{
			r_refdef.fov_x *= (sin(cl.time * 4.7) * 0.015 + 0.985);
			r_refdef.fov_y *= (sin(cl.time * 3.0) * 0.015 + 0.985);
		}
	}
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f (void)
{
	static int i = 0;
	char filename[16];
	char checkname[MAX_OSPATH];
	const char* extens;
	qboolean jpeg = (scr_screenshot_jpeg.integer != 0);

	if (jpeg)
		extens = "jpg";
	else
		extens = "tga";

	// find a file name to save it to
	for (; i<=9999 ; i++)
	{
		sprintf (filename, "dp%04i.%s", i, extens);
		sprintf (checkname, "%s/%s", fs_gamedir, filename);
		if (!FS_SysFileExists(checkname))
			break;
	}
	if (i==10000)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't create the image file\n");
		return;
 	}

	if (SCR_ScreenShot (filename, vid.realx, vid.realy, vid.realwidth, vid.realheight, jpeg))
		Con_Printf ("Wrote %s\n", filename);
	else
		Con_Printf ("unable to write %s\n", filename);
}

static int cl_avidemo_frame = 0;

void SCR_CaptureAVIDemo(void)
{
	char filename[32];
	qboolean jpeg = (scr_screenshot_jpeg.integer != 0);

	if (jpeg)
		sprintf(filename, "dpavi%06d.jpg", cl_avidemo_frame);
	else
		sprintf(filename, "dpavi%06d.tga", cl_avidemo_frame);

	if (SCR_ScreenShot(filename, vid.realx, vid.realy, vid.realwidth, vid.realheight, jpeg))
		cl_avidemo_frame++;
	else
	{
		Cvar_SetValueQuick(&cl_avidemo, 0);
		Con_Printf("avi saving failed on frame %i, out of disk space?  stopping avi demo catpure.\n", cl_avidemo_frame);
		cl_avidemo_frame = 0;
	}
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
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
	int j, size;
	char filename[256], basename[256];

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("envmap <basename> <size>: save out 6 cubic environment map images, usable with loadsky, note that size must one of 128, 256, 512, or 1024 and can't be bigger than your current resolution\n");
		return;
	}

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

	envmap = true;

	r_refdef.x = 0;
	r_refdef.y = 0;
	r_refdef.width = size;
	r_refdef.height = size;

	r_refdef.fov_x = 90;
	r_refdef.fov_y = 90;

	for (j = 0;j < 6;j++)
	{
		sprintf(filename, "env/%s%s.tga", basename, envmapinfo[j].name);
		VectorCopy(envmapinfo[j].angles, r_refdef.viewangles);
		R_ClearScreen();
		R_RenderView ();
		SCR_ScreenShot(filename, vid.realx, vid.realy, size, size, false);
	}

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
	qbyte *lmplabel;
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
	qbyte lmplabel[256], picname[256];
	float x, y;
	strcpy(lmplabel,MSG_ReadString());
	strcpy(picname, MSG_ReadString());
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
	strcpy(showlmp[k].label, lmplabel);
	strcpy(showlmp[k].pic, picname);
	showlmp[k].x = x;
	showlmp[k].y = y;
}

void SHOWLMP_drawall(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
			DrawQ_Pic(showlmp[i].x, showlmp[i].y, showlmp[i].pic, 0, 0, 1, 1, 1, 1, 0);
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}

void CL_SetupScreenSize(void)
{
	static float old2dresolution = -1;

	VID_GetWindowSize (&vid.realx, &vid.realy, &vid.realwidth, &vid.realheight);

	VID_UpdateGamma(false);

	if (scr_2dresolution.value != old2dresolution)
	{
		Cvar_SetValue("scr_2dresolution", bound(0.0f, scr_2dresolution.value, 1.0f));
		old2dresolution = scr_2dresolution.value;
	}

	if (vid.realwidth > 320)
	{
		vid.conwidth = (vid.realwidth - 320) * scr_2dresolution.value + 320;
		vid.conwidth = bound(320, vid.conwidth, vid.realwidth);
	}
	else
		vid.conwidth = 320;

	if (vid.realheight > 240)
	{
		vid.conheight = (vid.realheight - 240) * scr_2dresolution.value + 240;
		vid.conheight = bound(240, vid.conheight, vid.realheight);
	}
	else
		vid.conheight = 240;

	SCR_SetUpToDrawConsole();

	// determine size of refresh window
	SCR_CalcRefdef();
}

extern void R_Shadow_EditLights_DrawSelectedLightProperties(void);
void CL_UpdateScreen(void)
{
	if (!scr_initialized || !con_initialized || vid_hidden)
		return;				// not initialized yet

	if (cl_avidemo.integer)
		SCR_CaptureAVIDemo();
	else
		cl_avidemo_frame = 0;

	if (cls.signon == SIGNONS)
		R_TimeReport("other");

	CL_SetupScreenSize();

	DrawQ_Clear();

	if (cls.signon == SIGNONS)
		R_TimeReport("setup");

	//FIXME: force menu if nothing else to look at?
	//if (key_dest == key_game && cls.signon != SIGNONS && cls.state == ca_disconnected)

	if (scr_drawloading)
	{
		scr_drawloading = false;
		SCR_DrawLoading();
	}
	else
	{
		if (cls.signon == SIGNONS)
		{
			SCR_DrawNet ();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
			Sbar_Draw();
			SHOWLMP_drawall();
			SCR_CheckDrawCenterString();
		}
		ui_draw();
		CL_DrawVideo();
		M_Draw();
		if (cls.signon == SIGNONS)
		{
			R_TimeReport("2d");
			R_TimeReport_End();
			R_TimeReport_Start();
		}
		R_Shadow_EditLights_DrawSelectedLightProperties();
	}
	SCR_DrawConsole();

	SCR_UpdateScreen();
}

void CL_Screen_NewMap(void)
{
	SHOWLMP_clear();
}

