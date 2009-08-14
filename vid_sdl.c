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
#undef WIN32_LEAN_AND_MEAN  //hush a warning, SDL.h redefines this
#include <SDL.h>
#include <stdio.h>

#include "quakedef.h"

#ifdef WIN32
#define SDL_R_RESTART
#endif

// Tell startup code that we have a client
int cl_available = true;

qboolean vid_supportrefreshrate = false;

cvar_t joy_detected = {CVAR_READONLY, "joy_detected", "0", "number of joysticks detected by engine"};
cvar_t joy_enable = {CVAR_SAVE, "joy_enable", "0", "enables joystick support"};
cvar_t joy_index = {0, "joy_index", "0", "selects which joystick to use if you have multiple"};
cvar_t joy_axisforward = {0, "joy_axisforward", "1", "which joystick axis to query for forward/backward movement"};
cvar_t joy_axisside = {0, "joy_axisside", "0", "which joystick axis to query for right/left movement"};
cvar_t joy_axisup = {0, "joy_axisup", "-1", "which joystick axis to query for up/down movement"};
cvar_t joy_axispitch = {0, "joy_axispitch", "3", "which joystick axis to query for looking up/down"};
cvar_t joy_axisyaw = {0, "joy_axisyaw", "2", "which joystick axis to query for looking right/left"};
cvar_t joy_axisroll = {0, "joy_axisroll", "-1", "which joystick axis to query for tilting head right/left"};
cvar_t joy_deadzoneforward = {0, "joy_deadzoneforward", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneside = {0, "joy_deadzoneside", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneup = {0, "joy_deadzoneup", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzonepitch = {0, "joy_deadzonepitch", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneyaw = {0, "joy_deadzoneyaw", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_deadzoneroll = {0, "joy_deadzoneroll", "0", "deadzone tolerance, suggested values are in the range 0 to 0.01"};
cvar_t joy_sensitivityforward = {0, "joy_sensitivityforward", "-1", "movement multiplier"};
cvar_t joy_sensitivityside = {0, "joy_sensitivityside", "1", "movement multiplier"};
cvar_t joy_sensitivityup = {0, "joy_sensitivityup", "1", "movement multiplier"};
cvar_t joy_sensitivitypitch = {0, "joy_sensitivitypitch", "1", "movement multiplier"};
cvar_t joy_sensitivityyaw = {0, "joy_sensitivityyaw", "-1", "movement multiplier"};
cvar_t joy_sensitivityroll = {0, "joy_sensitivityroll", "1", "movement multiplier"};

static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_isfullscreen;
static int vid_numjoysticks = 0;
#define MAX_JOYSTICKS 8
static SDL_Joystick *vid_joysticks[MAX_JOYSTICKS];

static int win_half_width = 50;
static int win_half_height = 50;
static int video_bpp, video_flags;

static SDL_Surface *screen;

/////////////////////////
// Input handling
////
//TODO: Add joystick support
//TODO: Add error checking


//keysym to quake keysym mapping
#define tenoh	0,0,0,0,0, 0,0,0,0,0
#define fiftyoh tenoh, tenoh, tenoh, tenoh, tenoh
#define hundredoh fiftyoh, fiftyoh
static unsigned int tbl_sdltoquake[] =
{
	0,0,0,0,		//SDLK_UNKNOWN		= 0,
	0,0,0,0,		//SDLK_FIRST		= 0,
	K_BACKSPACE,	//SDLK_BACKSPACE	= 8,
	K_TAB,			//SDLK_TAB			= 9,
	0,0,
	0,				//SDLK_CLEAR		= 12,
	K_ENTER,		//SDLK_RETURN		= 13,
    0,0,0,0,0,
	K_PAUSE,		//SDLK_PAUSE		= 19,
	0,0,0,0,0,0,0,
	K_ESCAPE,		//SDLK_ESCAPE		= 27,
	0,0,0,0,
	K_SPACE,		//SDLK_SPACE		= 32,
	'!',			//SDLK_EXCLAIM		= 33,
	'"',			//SDLK_QUOTEDBL		= 34,
	'#',			//SDLK_HASH			= 35,
	'$',			//SDLK_DOLLAR		= 36,
	0,
	'&',			//SDLK_AMPERSAND	= 38,
	'\'',			//SDLK_QUOTE		= 39,
	'(',			//SDLK_LEFTPAREN	= 40,
	')',			//SDLK_RIGHTPAREN	= 41,
	'*',			//SDLK_ASTERISK		= 42,
	'+',			//SDLK_PLUS			= 43,
	',',			//SDLK_COMMA		= 44,
	'-',			//SDLK_MINUS		= 45,
	'.',			//SDLK_PERIOD		= 46,
	'/',			//SDLK_SLASH		= 47,
	'0',			//SDLK_0			= 48,
	'1',			//SDLK_1			= 49,
	'2',			//SDLK_2			= 50,
	'3',			//SDLK_3			= 51,
	'4',			//SDLK_4			= 52,
	'5',			//SDLK_5			= 53,
	'6',			//SDLK_6			= 54,
	'7',			//SDLK_7			= 55,
	'8',			//SDLK_8			= 56,
	'9',			//SDLK_9			= 57,
	':',			//SDLK_COLON		= 58,
	';',			//SDLK_SEMICOLON	= 59,
	'<',			//SDLK_LESS			= 60,
	'=',			//SDLK_EQUALS		= 61,
	'>',			//SDLK_GREATER		= 62,
	'?',			//SDLK_QUESTION		= 63,
	'@',			//SDLK_AT			= 64,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	'[',		//SDLK_LEFTBRACKET	= 91,
	'\\',		//SDLK_BACKSLASH	= 92,
	']',		//SDLK_RIGHTBRACKET	= 93,
	'^',		//SDLK_CARET		= 94,
	'_',		//SDLK_UNDERSCORE	= 95,
	'`',		//SDLK_BACKQUOTE	= 96,
	'a',		//SDLK_a			= 97,
	'b',		//SDLK_b			= 98,
	'c',		//SDLK_c			= 99,
	'd',		//SDLK_d			= 100,
	'e',		//SDLK_e			= 101,
	'f',		//SDLK_f			= 102,
	'g',		//SDLK_g			= 103,
	'h',		//SDLK_h			= 104,
	'i',		//SDLK_i			= 105,
	'j',		//SDLK_j			= 106,
	'k',		//SDLK_k			= 107,
	'l',		//SDLK_l			= 108,
	'm',		//SDLK_m			= 109,
	'n',		//SDLK_n			= 110,
	'o',		//SDLK_o			= 111,
	'p',		//SDLK_p			= 112,
	'q',		//SDLK_q			= 113,
	'r',		//SDLK_r			= 114,
	's',		//SDLK_s			= 115,
	't',		//SDLK_t			= 116,
	'u',		//SDLK_u			= 117,
	'v',		//SDLK_v			= 118,
	'w',		//SDLK_w			= 119,
	'x',		//SDLK_x			= 120,
	'y',		//SDLK_y			= 121,
	'z',		//SDLK_z			= 122,
	0,0,0,0,
	K_DEL, 		//SDLK_DELETE		= 127,
	hundredoh /*227*/, tenoh, tenoh, 0,0,0,0,0,0,0,0,
	K_KP_0,		//SDLK_KP0		= 256,
	K_KP_1,		//SDLK_KP1		= 257,
	K_KP_2,		//SDLK_KP2		= 258,
	K_KP_3,		//SDLK_KP3		= 259,
	K_KP_4,		//SDLK_KP4		= 260,
	K_KP_5,		//SDLK_KP5		= 261,
	K_KP_6,		//SDLK_KP6		= 262,
	K_KP_7,		//SDLK_KP7		= 263,
	K_KP_8,		//SDLK_KP8		= 264,
	K_KP_9,		//SDLK_KP9		= 265,
	K_KP_PERIOD,//SDLK_KP_PERIOD	= 266,
	K_KP_DIVIDE,//SDLK_KP_DIVIDE	= 267,
	K_KP_MULTIPLY,//SDLK_KP_MULTIPLY= 268,
	K_KP_MINUS,	//SDLK_KP_MINUS		= 269,
	K_KP_PLUS,	//SDLK_KP_PLUS		= 270,
	K_KP_ENTER,	//SDLK_KP_ENTER		= 271,
	K_KP_EQUALS,//SDLK_KP_EQUALS	= 272,
	K_UPARROW,	//SDLK_UP		= 273,
	K_DOWNARROW,//SDLK_DOWN		= 274,
	K_RIGHTARROW,//SDLK_RIGHT	= 275,
	K_LEFTARROW,//SDLK_LEFT		= 276,
	K_INS,		//SDLK_INSERT	= 277,
	K_HOME,		//SDLK_HOME		= 278,
	K_END,		//SDLK_END		= 279,
	K_PGUP, 	//SDLK_PAGEUP	= 280,
	K_PGDN,		//SDLK_PAGEDOWN	= 281,
	K_F1,		//SDLK_F1		= 282,
	K_F2,		//SDLK_F2		= 283,
	K_F3,		//SDLK_F3		= 284,
	K_F4,		//SDLK_F4		= 285,
	K_F5,		//SDLK_F5		= 286,
	K_F6,		//SDLK_F6		= 287,
	K_F7,		//SDLK_F7		= 288,
	K_F8,		//SDLK_F8		= 289,
	K_F9,		//SDLK_F9		= 290,
	K_F10,		//SDLK_F10		= 291,
	K_F11,		//SDLK_F11		= 292,
	K_F12,		//SDLK_F12		= 293,
	0,			//SDLK_F13		= 294,
	0,			//SDLK_F14		= 295,
	0,			//SDLK_F15		= 296,
	0,0,0,
	K_NUMLOCK,	//SDLK_NUMLOCK	= 300,
	K_CAPSLOCK,	//SDLK_CAPSLOCK	= 301,
	K_SCROLLOCK,//SDLK_SCROLLOCK= 302,
	K_SHIFT,	//SDLK_RSHIFT	= 303,
	K_SHIFT,	//SDLK_LSHIFT	= 304,
	K_CTRL,		//SDLK_RCTRL	= 305,
	K_CTRL,		//SDLK_LCTRL	= 306,
	K_ALT,		//SDLK_RALT		= 307,
	K_ALT,		//SDLK_LALT		= 308,
	0,			//SDLK_RMETA	= 309,
	0,			//SDLK_LMETA	= 310,
	0,			//SDLK_LSUPER	= 311,		/* Left "Windows" key */
	0,			//SDLK_RSUPER	= 312,		/* Right "Windows" key */
	K_ALT,			//SDLK_MODE		= 313,		/* "Alt Gr" key */
	0,			//SDLK_COMPOSE	= 314,		/* Multi-key compose key */
	0,			//SDLK_HELP		= 315,
	0,			//SDLK_PRINT	= 316,
	0,			//SDLK_SYSREQ	= 317,
	K_PAUSE,	//SDLK_BREAK	= 318,
	0,			//SDLK_MENU		= 319,
	0,			//SDLK_POWER	= 320,		/* Power Macintosh power key */
	'e',		//SDLK_EURO		= 321,		/* Some european keyboards */
	0			//SDLK_UNDO		= 322,		/* Atari keyboard has Undo */
};
#undef tenoh
#undef fiftyoh
#undef hundredoh

static int MapKey( unsigned int sdlkey )
{
	if( sdlkey > sizeof(tbl_sdltoquake)/ sizeof(int) )
		return 0;
    return tbl_sdltoquake[ sdlkey ];
}

void VID_SetMouse(qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
	if (vid_usingmouse != relative)
	{
		vid_usingmouse = relative;
		cl_ignoremousemoves = 2;
		SDL_WM_GrabInput( relative ? SDL_GRAB_ON : SDL_GRAB_OFF );
	}
	if (vid_usinghidecursor != hidecursor)
	{
		vid_usinghidecursor = hidecursor;
		SDL_ShowCursor( hidecursor ? SDL_DISABLE : SDL_ENABLE);
	}
}

static double IN_JoystickGetAxis(SDL_Joystick *joy, int axis, double sensitivity, double deadzone)
{
	double value;
	if (axis < 0 || axis >= SDL_JoystickNumAxes(joy))
		return 0; // no such axis on this joystick
	value = SDL_JoystickGetAxis(joy, axis) * (1.0 / 32767.0);
	value = bound(-1, value, 1);
	if (fabs(value) < deadzone)
		return 0; // within deadzone around center
	return value * sensitivity;
}

void IN_Move( void )
{
	int j;
	static int old_x = 0, old_y = 0;
	static int stuck = 0;
	int x, y;
	if (vid_usingmouse)
	{
		if(vid_stick_mouse.integer)
		{
			// have the mouse stuck in the middle, example use: prevent expose effect of beryl during the game when not using
			// window grabbing. --blub

			// we need 2 frames to initialize the center position
			if(!stuck)
			{
				SDL_WarpMouse(win_half_width, win_half_height);
				SDL_GetMouseState(&x, &y);
				SDL_GetRelativeMouseState(&x, &y);
				++stuck;
			} else {
				SDL_GetRelativeMouseState(&x, &y);
				in_mouse_x = x + old_x;
				in_mouse_y = y + old_y;
				SDL_GetMouseState(&x, &y);
				old_x = x - win_half_width;
				old_y = y - win_half_height;
				SDL_WarpMouse(win_half_width, win_half_height);
			}
		} else {
			SDL_GetRelativeMouseState( &x, &y );
			in_mouse_x = x;
			in_mouse_y = y;
		}
	}

	SDL_GetMouseState(&x, &y);
	in_windowmouse_x = x;
	in_windowmouse_y = y;

	if (vid_numjoysticks && joy_enable.integer && joy_index.integer >= 0 && joy_index.integer < vid_numjoysticks)
	{
		SDL_Joystick *joy = vid_joysticks[joy_index.integer];
		int numballs = SDL_JoystickNumBalls(joy);
		for (j = 0;j < numballs;j++)
		{
			SDL_JoystickGetBall(joy, j, &x, &y);
			in_mouse_x += x;
			in_mouse_y += y;
		}
		cl.cmd.forwardmove += IN_JoystickGetAxis(joy, joy_axisforward.integer, joy_sensitivityforward.value, joy_deadzoneforward.value) * cl_forwardspeed.value;
		cl.cmd.sidemove    += IN_JoystickGetAxis(joy, joy_axisside.integer, joy_sensitivityside.value, joy_deadzoneside.value) * cl_sidespeed.value;
		cl.cmd.upmove      += IN_JoystickGetAxis(joy, joy_axisup.integer, joy_sensitivityup.value, joy_deadzoneup.value) * cl_upspeed.value;
		cl.viewangles[0]   += IN_JoystickGetAxis(joy, joy_axispitch.integer, joy_sensitivitypitch.value, joy_deadzonepitch.value) * cl.realframetime * cl_pitchspeed.value;
		cl.viewangles[1]   += IN_JoystickGetAxis(joy, joy_axisyaw.integer, joy_sensitivityyaw.value, joy_deadzoneyaw.value) * cl.realframetime * cl_yawspeed.value;
		//cl.viewangles[2]   += IN_JoystickGetAxis(joy, joy_axisroll.integer, joy_sensitivityroll.value, joy_deadzoneroll.value) * cl.realframetime * cl_rollspeed.value;
	}
}

/////////////////////
// Message Handling
////

static int Sys_EventFilter( SDL_Event *event )
{
	//TODO: Add a quit query in linux, too - though linux user are more likely to know what they do
	if (event->type == SDL_QUIT)
	{
#ifdef WIN32
		if (MessageBox( NULL, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION ) == IDNO)
			return 0;
#endif
	}
	return 1;
}

#ifdef SDL_R_RESTART
static qboolean sdl_needs_restart;
static void sdl_start()
{
}
static void sdl_shutdown()
{
	sdl_needs_restart = false;
}
static void sdl_newmap()
{
}
#endif

static keynum_t buttonremap[18] =
{
	K_MOUSE1,
	K_MOUSE3,
	K_MOUSE2,
	K_MWHEELUP,
	K_MWHEELDOWN,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10,
	K_MOUSE11,
	K_MOUSE12,
	K_MOUSE13,
	K_MOUSE14,
	K_MOUSE15,
	K_MOUSE16,
};

void Sys_SendKeyEvents( void )
{
	static qboolean sound_active = true;
	SDL_Event event;

	while( SDL_PollEvent( &event ) )
		switch( event.type ) {
			case SDL_QUIT:
				Sys_Quit(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				Key_Event( MapKey( event.key.keysym.sym ), (char)event.key.keysym.unicode, (event.key.state == SDL_PRESSED) );
				break;
			case SDL_ACTIVEEVENT:
				if( event.active.state & SDL_APPACTIVE )
				{
					if( event.active.gain )
						vid_hidden = false;
					else
						vid_hidden = true;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (event.button.button <= 18)
					Key_Event( buttonremap[event.button.button - 1], 0, event.button.state == SDL_PRESSED );
				break;
			case SDL_JOYBUTTONDOWN:
				if (!joy_enable.integer)
					break; // ignore down events if joystick has been disabled
			case SDL_JOYBUTTONUP:
				if (event.jbutton.button < 48)
					Key_Event( event.jbutton.button + (event.jbutton.button < 16 ? K_JOY1 : K_AUX1 - 16), 0, (event.jbutton.state == SDL_PRESSED) );
				break;
			case SDL_VIDEORESIZE:
				if(vid_resizable.integer < 2)
				{
					vid.width = event.resize.w;
					vid.height = event.resize.h;
					SDL_SetVideoMode(vid.width, vid.height, video_bpp, video_flags);
#ifdef SDL_R_RESTART
					// better not call R_Modules_Restart from here directly, as this may wreak havoc...
					// so, let's better queue it for next frame
					if(!sdl_needs_restart)
					{
						Cbuf_AddText("\nr_restart\n");
						sdl_needs_restart = true;
					}
#endif
				}
				break;
		}

	// enable/disable sound on focus gain/loss
	if (!vid_hidden && (vid_activewindow || !snd_mutewhenidle.integer))
	{
		if (!sound_active)
		{
			S_UnblockSound ();
			sound_active = true;
		}
	}
	else
	{
		if (sound_active)
		{
			S_BlockSound ();
			sound_active = false;
		}
	}
}

/////////////////
// Video system
////

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	p = SDL_GL_GetProcAddress(name);
	return p;
}

static int Sys_EventFilter( SDL_Event *event );
static qboolean vid_sdl_initjoysticksystem = false;

void VID_Init (void)
{
	Cvar_RegisterVariable(&joy_detected);
	Cvar_RegisterVariable(&joy_enable);
	Cvar_RegisterVariable(&joy_index);
	Cvar_RegisterVariable(&joy_axisforward);
	Cvar_RegisterVariable(&joy_axisside);
	Cvar_RegisterVariable(&joy_axisup);
	Cvar_RegisterVariable(&joy_axispitch);
	Cvar_RegisterVariable(&joy_axisyaw);
	//Cvar_RegisterVariable(&joy_axisroll);
	Cvar_RegisterVariable(&joy_deadzoneforward);
	Cvar_RegisterVariable(&joy_deadzoneside);
	Cvar_RegisterVariable(&joy_deadzoneup);
	Cvar_RegisterVariable(&joy_deadzonepitch);
	Cvar_RegisterVariable(&joy_deadzoneyaw);
	//Cvar_RegisterVariable(&joy_deadzoneroll);
	Cvar_RegisterVariable(&joy_sensitivityforward);
	Cvar_RegisterVariable(&joy_sensitivityside);
	Cvar_RegisterVariable(&joy_sensitivityup);
	Cvar_RegisterVariable(&joy_sensitivitypitch);
	Cvar_RegisterVariable(&joy_sensitivityyaw);
	//Cvar_RegisterVariable(&joy_sensitivityroll);
	
#ifdef SDL_R_RESTART
	R_RegisterModule("SDL", sdl_start, sdl_shutdown, sdl_newmap);
#endif

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Failed to init SDL video subsystem: %s", SDL_GetError());
	vid_sdl_initjoysticksystem = SDL_Init(SDL_INIT_JOYSTICK) >= 0;
	if (vid_sdl_initjoysticksystem)
		Con_Printf("Failed to init SDL joystick subsystem: %s\n", SDL_GetError());
	vid_isfullscreen = false;
}

// set the icon (we dont use SDL here since it would be too much a PITA)
#ifdef WIN32
#include "resource.h"
#include <SDL_syswm.h>
static void VID_SetCaption()
{
    SDL_SysWMinfo	info;
	HICON			icon;

	// set the caption
	SDL_WM_SetCaption( gamename, NULL );

	// get the HWND handle
    SDL_VERSION( &info.version );
	if( !SDL_GetWMInfo( &info ) )
		return;

	icon = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON1 ) );
#ifndef _W64 //If Windows 64bit data types don't exist
#ifndef SetClassLongPtr
#define SetClassLongPtr SetClassLong
#endif
#ifndef GCLP_HICON
#define GCLP_HICON GCL_HICON
#endif
#ifndef LONG_PTR
#define LONG_PTR LONG
#endif
#endif
	SetClassLongPtr( info.window, GCLP_HICON, (LONG_PTR)icon );
}
static void VID_SetIcon()
{
}
#else
// Adding the OS independent XPM version --blub
#include "darkplaces.xpm"
#include "nexuiz.xpm"
static SDL_Surface *icon = NULL;
static void VID_SetIcon()
{
	/*
	 * Somewhat restricted XPM reader. Only supports XPMs saved by GIMP 2.4 at
	 * default settings with less than 91 colors and transparency.
	 */

	int width, height, colors, isize, i, j;
	int thenone = -1;
	static SDL_Color palette[256];
	unsigned short palenc[256]; // store color id by char

	char **idata = ENGINE_ICON;
	char *data = idata[0];

	if(sscanf(data, "%i %i %i %i", &width, &height, &colors, &isize) != 4)
	{
		// NOTE: Only 1-char colornames are supported
		Con_Printf("Sorry, but this does not even look similar to an XPM.\n");
		return;
	}

	if(isize != 1)
	{
		// NOTE: Only 1-char colornames are supported
		Con_Printf("This XPM's palette is either huge or idiotically unoptimized. It's key size is %i\n", isize);
		return;
	}

	for(i = 0; i < colors; ++i)
	{
		unsigned int r, g, b;
		char idx;

		if(sscanf(idata[i+1], "%c c #%02x%02x%02x", &idx, &r, &g, &b) != 4)
		{
			char foo[2];
			if(sscanf(idata[i+1], "%c c Non%1[e]", &idx, foo) != 2) // I take the DailyWTF credit for this. --div0
			{
				Con_Printf("This XPM's palette looks odd. Can't continue.\n");
				return;
			}
			else
			{
				palette[i].r = 255; // color key
				palette[i].g = 0;
				palette[i].b = 255;
				thenone = i; // weeeee
			}
		}
		else
		{
			palette[i].r = r - (r == 255 && g == 0 && b == 255); // change 255/0/255 pink to 254/0/255 for color key
			palette[i].g = g;
			palette[i].b = b;
		}

		palenc[(unsigned char) idx] = i;
	}

	// allocate the image data
	data = (char*) malloc(width*height);

	for(j = 0; j < height; ++j)
	{
		for(i = 0; i < width; ++i)
		{
			// casting to the safest possible datatypes ^^
			data[j * width + i] = palenc[((unsigned char*)idata[colors+j+1])[i]];
		}
	}

	if(icon != NULL)
	{
		// SDL_FreeSurface should free the data too
		// but for completeness' sake...
		if(icon->flags & SDL_PREALLOC)
		{
			free(icon->pixels);
			icon->pixels = NULL; // safety
		}
		SDL_FreeSurface(icon);
	}

	icon = SDL_CreateRGBSurface(SDL_SRCCOLORKEY, width, height, 8, 0,0,0,0);// rmask, gmask, bmask, amask); no mask needed
	// 8 bit surfaces get an empty palette allocated according to the docs
	// so it's a palette image for sure :) no endian check necessary for the mask

	if(icon == NULL) {
		Con_Printf(	"Failed to create surface for the window Icon!\n"
				"%s\n", SDL_GetError());
		free(data);
		return;
	}
	icon->pixels = data;
	SDL_SetPalette(icon, SDL_PHYSPAL|SDL_LOGPAL, palette, 0, colors);
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, thenone);

	SDL_WM_SetIcon(icon, NULL);
}


static void VID_SetCaption()
{
	SDL_WM_SetCaption( gamename, NULL );
}
#endif

static void VID_OutputVersion()
{
	const SDL_version *version;
	version = SDL_Linked_Version();
	Con_Printf(	"Linked against SDL version %d.%d.%d\n"
					"Using SDL library version %d.%d.%d\n",
					SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
					version->major, version->minor, version->patch );
}

int VID_InitMode(int fullscreen, int *width, int *height, int bpp, int refreshrate, int stereobuffer, int samples)
{
	int i;
	static int notfirstvideomode = false;
	int flags = SDL_OPENGL;
	const char *drivername;

	win_half_width = *width>>1;
	win_half_height = *height>>1;

	if(vid_resizable.integer)
		flags |= SDL_RESIZABLE;

	VID_OutputVersion();

	/*
	SDL Hack
		We cant switch from one OpenGL video mode to another.
		Thus we first switch to some stupid 2D mode and then back to OpenGL.
	*/
	if (notfirstvideomode)
		SDL_SetVideoMode( 0, 0, 0, 0 );
	notfirstvideomode = true;

	// SDL usually knows best
	drivername = NULL;

// COMMANDLINEOPTION: SDL GL: -gl_driver <drivername> selects a GL driver library, default is whatever SDL recommends, useful only for 3dfxogl.dll/3dfxvgl.dll or fxmesa or similar, if you don't know what this is for, you don't need it
	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		drivername = com_argv[i + 1];
	if (SDL_GL_LoadLibrary(drivername) < 0)
	{
		Con_Printf("Unable to load GL driver \"%s\": %s\n", drivername, SDL_GetError());
		return false;
	}

	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
	{
		VID_Shutdown();
		Con_Print("Required OpenGL function glGetString not found\n");
		return false;
	}

	// Knghtbrd: should do platform-specific extension string function here

	vid_isfullscreen = false;
	if (fullscreen) {
		flags |= SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}
	//flags |= SDL_HWSURFACE;

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	if (bpp >= 32)
	{
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 8);
	}
	else
	{
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 16);
	}
	if (stereobuffer)
		SDL_GL_SetAttribute (SDL_GL_STEREO, 1);
	if (vid_vsync.integer)
		SDL_GL_SetAttribute (SDL_GL_SWAP_CONTROL, 1);
	else
		SDL_GL_SetAttribute (SDL_GL_SWAP_CONTROL, 0);
	if (samples > 1)
	{
		SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, samples);
	}

	video_bpp = bpp;
	video_flags = flags;
	VID_SetIcon();
	screen = SDL_SetVideoMode(*width, *height, bpp, flags);

	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", *width, *height, SDL_GetError());
		VID_Shutdown();
		return false;
	}

	// set window title
	VID_SetCaption();
	// set up an event filter to ask confirmation on close button in WIN32
	SDL_SetEventFilter( (SDL_EventFilter) Sys_EventFilter );
	// init keyboard
	SDL_EnableUNICODE( SDL_ENABLE );
	// enable key repeat since everyone expects it
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	gl_platform = "SDL";
	gl_platformextensions = "";
	gl_videosyncavailable = true;

	GL_Init();

	vid_numjoysticks = SDL_NumJoysticks();
	vid_numjoysticks = bound(0, vid_numjoysticks, MAX_JOYSTICKS);
	Cvar_SetValueQuick(&joy_detected, vid_numjoysticks);
	Con_Printf("%d SDL joystick(s) found:\n", vid_numjoysticks);
	memset(vid_joysticks, 0, sizeof(vid_joysticks));
	for (i = 0;i < vid_numjoysticks;i++)
	{
		SDL_Joystick *joy;
		joy = vid_joysticks[i] = SDL_JoystickOpen(i);
		if (!joy)
		{
			Con_Printf("joystick #%i: open failed: %s\n", i, SDL_GetError());
			continue;
		}
		Con_Printf("joystick #%i: opened \"%s\" with %i axes, %i buttons, %i balls\n", i, SDL_JoystickName(i), (int)SDL_JoystickNumAxes(joy), (int)SDL_JoystickNumButtons(joy), (int)SDL_JoystickNumBalls(joy));
	}

	vid_hidden = false;
	vid_activewindow = false;
	vid_usingmouse = false;
	vid_usinghidecursor = false;

	SDL_WM_GrabInput(SDL_GRAB_OFF);
	return true;
}

void VID_Shutdown (void)
{
	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	gl_driver[0] = 0;
	gl_extensions = "";
	gl_platform = "";
	gl_platformextensions = "";
}

int VID_SetGamma (unsigned short *ramps, int rampsize)
{
	return !SDL_SetGammaRamp (ramps, ramps + rampsize, ramps + rampsize*2);
}

int VID_GetGamma (unsigned short *ramps, int rampsize)
{
	return !SDL_GetGammaRamp (ramps, ramps + rampsize, ramps + rampsize*2);
}

void VID_Finish (void)
{
	Uint8 appstate;

	//react on appstate changes
	appstate = SDL_GetAppState();

	vid_hidden = !(appstate & SDL_APPACTIVE);

	if( vid_hidden || !( appstate & SDL_APPMOUSEFOCUS ) || !( appstate & SDL_APPINPUTFOCUS ) )
		vid_activewindow = false;
	else
		vid_activewindow = true;

	VID_UpdateGamma(false, 256);

	if (r_render.integer && !vid_hidden)
	{
		CHECKGLERROR
		if (r_speeds.integer == 2 || gl_finish.integer)
		{
			qglFinish();CHECKGLERROR
		}
		SDL_GL_SwapBuffers();
	}
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	size_t k;
	SDL_Rect **vidmodes;
	int bpp = SDL_GetVideoInfo()->vfmt->BitsPerPixel;

	k = 0;
	for(vidmodes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE); *vidmodes; ++vidmodes)
	{
		if(k >= maxcount)
			break;
		modes[k].width = (*vidmodes)->w;
		modes[k].height = (*vidmodes)->h;
		modes[k].bpp = bpp;
		modes[k].refreshrate = 60; // no support for refresh rate in SDL
		modes[k].pixelheight_num = 1;
		modes[k].pixelheight_denom = 1; // SDL does not provide this
		++k;
	}
	return k;
}
