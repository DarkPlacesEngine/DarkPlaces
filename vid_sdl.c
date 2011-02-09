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
#include <SDL_syswm.h>
#include <stdio.h>

#include "quakedef.h"
#include "image.h"
#include "dpsoftrast.h"

#ifndef __IPHONEOS__
#ifdef MACOSX
#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
static cvar_t apple_mouse_noaccel = {CVAR_SAVE, "apple_mouse_noaccel", "1", "disables mouse acceleration while DarkPlaces is active"};
static qboolean vid_usingnoaccel;
static double originalMouseSpeed = -1.0;
io_connect_t IN_GetIOHandle(void)
{
	io_connect_t iohandle = MACH_PORT_NULL;
	kern_return_t status;
	io_service_t iohidsystem = MACH_PORT_NULL;
	mach_port_t masterport;

	status = IOMasterPort(MACH_PORT_NULL, &masterport);
	if(status != KERN_SUCCESS)
		return 0;

	iohidsystem = IORegistryEntryFromPath(masterport, kIOServicePlane ":/IOResources/IOHIDSystem");
	if(!iohidsystem)
		return 0;

	status = IOServiceOpen(iohidsystem, mach_task_self(), kIOHIDParamConnectType, &iohandle);
	IOObjectRelease(iohidsystem);

	return iohandle;
}
#endif
#endif

#ifdef WIN32
#define SDL_R_RESTART
#endif

// Tell startup code that we have a client
int cl_available = true;

qboolean vid_supportrefreshrate = false;

cvar_t vid_soft = {CVAR_SAVE, "vid_soft", "0", "enables use of the DarkPlaces Software Rasterizer rather than OpenGL or Direct3D"};
cvar_t vid_soft_threads = {CVAR_SAVE, "vid_soft_threads", "2", "the number of threads the DarkPlaces Software Rasterizer should use"}; 
cvar_t vid_soft_interlace = {CVAR_SAVE, "vid_soft_interlace", "1", "whether the DarkPlaces Software Rasterizer shoud interlace the screen bands occupied by each thread"};
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
cvar_t joy_axiskeyevents = {CVAR_SAVE, "joy_axiskeyevents", "0", "generate uparrow/leftarrow etc. keyevents for joystick axes, use if your joystick driver is not generating them"};

static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_isfullscreen;
#if !(SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2)
static qboolean vid_usingvsync = false;
#endif
static int vid_numjoysticks = 0;
#define MAX_JOYSTICKS 8
static SDL_Joystick *vid_joysticks[MAX_JOYSTICKS];

static int win_half_width = 50;
static int win_half_height = 50;
static int video_bpp, video_flags;

static SDL_Surface *screen;
static SDL_Surface *vid_softsurface;

// joystick axes state
#define MAX_JOYSTICK_AXES	16
typedef struct
{
	float oldmove;
	float move;
	double keytime;
}joy_axiscache_t;
static joy_axiscache_t joy_axescache[MAX_JOYSTICK_AXES];

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
#ifndef __IPHONEOS__
#ifdef MACOSX
	if(relative)
		if(vid_usingmouse && (vid_usingnoaccel != !!apple_mouse_noaccel.integer))
			VID_SetMouse(false, false, false); // ungrab first!
#endif
#endif
	if (vid_usingmouse != relative)
	{
		vid_usingmouse = relative;
		cl_ignoremousemoves = 2;
		SDL_WM_GrabInput( relative ? SDL_GRAB_ON : SDL_GRAB_OFF );
#ifndef __IPHONEOS__
#ifdef MACOSX
		if(relative)
		{
			// Save the status of mouse acceleration
			originalMouseSpeed = -1.0; // in case of error
			if(apple_mouse_noaccel.integer)
			{
				io_connect_t mouseDev = IN_GetIOHandle();
				if(mouseDev != 0)
				{
					if(IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &originalMouseSpeed) == kIOReturnSuccess)
					{
						Con_DPrintf("previous mouse acceleration: %f\n", originalMouseSpeed);
						if(IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
						{
							Con_Print("Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
							Cvar_SetValueQuick(&apple_mouse_noaccel, 0);
						}
					}
					else
					{
						Con_Print("Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
						Cvar_SetValueQuick(&apple_mouse_noaccel, 0);
					}
					IOServiceClose(mouseDev);
				}
				else
				{
					Con_Print("Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
					Cvar_SetValueQuick(&apple_mouse_noaccel, 0);
				}
			}

			vid_usingnoaccel = !!apple_mouse_noaccel.integer;
		}
		else
		{
			if(originalMouseSpeed != -1.0)
			{
				io_connect_t mouseDev = IN_GetIOHandle();
				if(mouseDev != 0)
				{
					Con_DPrintf("restoring mouse acceleration to: %f\n", originalMouseSpeed);
					if(IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), originalMouseSpeed) != kIOReturnSuccess)
						Con_Print("Could not re-enable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
					IOServiceClose(mouseDev);
				}
				else
					Con_Print("Could not re-enable mouse acceleration (failed at IO_GetIOHandle).\n");
			}
		}
#endif
#endif
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

/////////////////////
// Joystick axis keyevents
// a sort of hack emulating Arrow keys for joystick axises
// as some drives dont send such keyevents for them
// additionally we should block drivers that do send arrow keyevents to prevent double events
////

static void IN_JoystickKeyeventForAxis(SDL_Joystick *joy, int axis, int key_pos, int key_neg)
{
	double joytime;

	if (axis < 0 || axis >= SDL_JoystickNumAxes(joy))
		return; // no such axis on this joystick

	joytime = Sys_DoubleTime();
	// no key event, continuous keydown event
	if (joy_axescache[axis].move == joy_axescache[axis].oldmove)
	{
		if (joy_axescache[axis].move != 0 && joytime > joy_axescache[axis].keytime)
		{
			//Con_Printf("joy %s %i %f\n", Key_KeynumToString((joy_axescache[axis].move > 0) ? key_pos : key_neg), 1, cl.time);
			Key_Event((joy_axescache[axis].move > 0) ? key_pos : key_neg, 0, 1);
			joy_axescache[axis].keytime = joytime + 0.5 / 20;
		}
		return;
	}
	// generate key up event
	if (joy_axescache[axis].oldmove)
	{
		//Con_Printf("joy %s %i %f\n", Key_KeynumToString((joy_axescache[axis].oldmove > 0) ? key_pos : key_neg), 1, cl.time);
		Key_Event((joy_axescache[axis].oldmove > 0) ? key_pos : key_neg, 0, 0);
	}
	// generate key down event
	if (joy_axescache[axis].move)
	{
		//Con_Printf("joy %s %i %f\n", Key_KeynumToString((joy_axescache[axis].move > 0) ? key_pos : key_neg), 1, cl.time);
		Key_Event((joy_axescache[axis].move > 0) ? key_pos : key_neg, 0, 1);
		joy_axescache[axis].keytime = joytime + 0.5;
	}
}

static qboolean IN_JoystickBlockDoubledKeyEvents(int keycode)
{
	if (!joy_axiskeyevents.integer)
		return false;

	// block keyevent if it's going to be provided by joystick keyevent system
	if (vid_numjoysticks && joy_enable.integer && joy_index.integer >= 0 && joy_index.integer < vid_numjoysticks)
	{
		SDL_Joystick *joy = vid_joysticks[joy_index.integer];

		if (keycode == K_UPARROW || keycode == K_DOWNARROW)
			if (IN_JoystickGetAxis(joy, joy_axisforward.integer, 1, 0.01) || joy_axescache[joy_axisforward.integer].move || joy_axescache[joy_axisforward.integer].oldmove)
				return true;
		if (keycode == K_RIGHTARROW || keycode == K_LEFTARROW)
			if (IN_JoystickGetAxis(joy, joy_axisside.integer, 1, 0.01) || joy_axescache[joy_axisside.integer].move || joy_axescache[joy_axisside.integer].oldmove)
				return true;
	}

	return false;
}

/////////////////////
// Movement handling
////

void IN_Move( void )
{
	int j;
	static int old_x = 0, old_y = 0;
	static int stuck = 0;
	int x, y, numaxes, numballs;

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

		// balls convert to mousemove
		numballs = SDL_JoystickNumBalls(joy);
		for (j = 0;j < numballs;j++)
		{
			SDL_JoystickGetBall(joy, j, &x, &y);
			in_mouse_x += x;
			in_mouse_y += y;
		}

		// axes
		cl.cmd.forwardmove += IN_JoystickGetAxis(joy, joy_axisforward.integer, joy_sensitivityforward.value, joy_deadzoneforward.value) * cl_forwardspeed.value;
		cl.cmd.sidemove    += IN_JoystickGetAxis(joy, joy_axisside.integer, joy_sensitivityside.value, joy_deadzoneside.value) * cl_sidespeed.value;
		cl.cmd.upmove      += IN_JoystickGetAxis(joy, joy_axisup.integer, joy_sensitivityup.value, joy_deadzoneup.value) * cl_upspeed.value;
		cl.viewangles[0]   += IN_JoystickGetAxis(joy, joy_axispitch.integer, joy_sensitivitypitch.value, joy_deadzonepitch.value) * cl.realframetime * cl_pitchspeed.value;
		cl.viewangles[1]   += IN_JoystickGetAxis(joy, joy_axisyaw.integer, joy_sensitivityyaw.value, joy_deadzoneyaw.value) * cl.realframetime * cl_yawspeed.value;
		//cl.viewangles[2]   += IN_JoystickGetAxis(joy, joy_axisroll.integer, joy_sensitivityroll.value, joy_deadzoneroll.value) * cl.realframetime * cl_rollspeed.value;
	
		// cache state of axes to emulate button events for them
		numaxes = min(MAX_JOYSTICK_AXES, SDL_JoystickNumAxes(joy));
		for (j = 0; j < numaxes; j++)
		{
			joy_axescache[j].oldmove = joy_axescache[j].move;
			joy_axescache[j].move = IN_JoystickGetAxis(joy, j, 1, 0.01);
		}

		// run keyevents
		if (joy_axiskeyevents.integer)
		{
			IN_JoystickKeyeventForAxis(joy, joy_axisforward.integer, K_DOWNARROW, K_UPARROW);
			IN_JoystickKeyeventForAxis(joy, joy_axisside.integer, K_RIGHTARROW, K_LEFTARROW);
		}
	}
}

/////////////////////
// Message Handling
////

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
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
#endif

#ifdef SDL_R_RESTART
static qboolean sdl_needs_restart;
static void sdl_start(void)
{
}
static void sdl_shutdown(void)
{
	sdl_needs_restart = false;
}
static void sdl_newmap(void)
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
	int keycode;
	SDL_Event event;

	while( SDL_PollEvent( &event ) )
		switch( event.type ) {
			case SDL_QUIT:
#if !(SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2)
#ifdef WIN32
				if (MessageBox( NULL, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION ) == IDNO)
					return 0;
#endif
#endif
				Sys_Quit(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keycode = MapKey(event.key.keysym.sym);
				if (!IN_JoystickBlockDoubledKeyEvents(keycode))
					Key_Event(keycode, event.key.keysym.unicode, (event.key.state == SDL_PRESSED));
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
					if (vid_softsurface)
					{
						SDL_FreeSurface(vid_softsurface);
						vid_softsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, vid.width, vid.height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
						vid.softpixels = (unsigned int *)vid_softsurface->pixels;
						SDL_SetAlpha(vid_softsurface, 0, 255);
						if (vid.softdepthpixels)
							free(vid.softdepthpixels);
						vid.softdepthpixels = (unsigned int*)calloc(1, vid.width * vid.height * 4);
					}
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
	if ((!vid_hidden && vid_activewindow) || !snd_mutewhenidle.integer)
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

#ifdef __IPHONEOS__
//#include <SDL_opengles.h>
#include <OpenGLES/ES2/gl.h>

GLboolean wrapglIsBuffer(GLuint buffer) {return glIsBuffer(buffer);}
GLboolean wrapglIsEnabled(GLenum cap) {return glIsEnabled(cap);}
GLboolean wrapglIsFramebuffer(GLuint framebuffer) {return glIsFramebuffer(framebuffer);}
//GLboolean wrapglIsQuery(GLuint qid) {return glIsQuery(qid);}
GLboolean wrapglIsRenderbuffer(GLuint renderbuffer) {return glIsRenderbuffer(renderbuffer);}
//GLboolean wrapglUnmapBuffer(GLenum target) {return glUnmapBuffer(target);}
GLenum wrapglCheckFramebufferStatus(GLenum target) {return glCheckFramebufferStatus(target);}
GLenum wrapglGetError(void) {return glGetError();}
GLuint wrapglCreateProgram(void) {return glCreateProgram();}
GLuint wrapglCreateShader(GLenum shaderType) {return glCreateShader(shaderType);}
//GLuint wrapglGetHandle(GLenum pname) {return glGetHandle(pname);}
GLint wrapglGetAttribLocation(GLuint programObj, const GLchar *name) {return glGetAttribLocation(programObj, name);}
GLint wrapglGetUniformLocation(GLuint programObj, const GLchar *name) {return glGetUniformLocation(programObj, name);}
//GLvoid* wrapglMapBuffer(GLenum target, GLenum access) {return glMapBuffer(target, access);}
const GLubyte* wrapglGetString(GLenum name) {return glGetString(name);}
void wrapglActiveStencilFace(GLenum e) {Con_Printf("glActiveStencilFace(e)\n");}
void wrapglActiveTexture(GLenum e) {glActiveTexture(e);}
void wrapglAlphaFunc(GLenum func, GLclampf ref) {Con_Printf("glAlphaFunc(func, ref)\n");}
void wrapglArrayElement(GLint i) {Con_Printf("glArrayElement(i)\n");}
void wrapglAttachShader(GLuint containerObj, GLuint obj) {glAttachShader(containerObj, obj);}
void wrapglBegin(GLenum mode) {Con_Printf("glBegin(mode)\n");}
//void wrapglBeginQuery(GLenum target, GLuint qid) {glBeginQuery(target, qid);}
void wrapglBindAttribLocation(GLuint programObj, GLuint index, const GLchar *name) {glBindAttribLocation(programObj, index, name);}
void wrapglBindBuffer(GLenum target, GLuint buffer) {glBindBuffer(target, buffer);}
void wrapglBindFramebuffer(GLenum target, GLuint framebuffer) {glBindFramebuffer(target, framebuffer);}
void wrapglBindRenderbuffer(GLenum target, GLuint renderbuffer) {glBindRenderbuffer(target, renderbuffer);}
void wrapglBindTexture(GLenum target, GLuint texture) {glBindTexture(target, texture);}
void wrapglBlendEquation(GLenum e) {glBlendEquation(e);}
void wrapglBlendFunc(GLenum sfactor, GLenum dfactor) {glBlendFunc(sfactor, dfactor);}
void wrapglBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {glBufferData(target, size, data, usage);}
void wrapglBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) {glBufferSubData(target, offset, size, data);}
void wrapglClear(GLbitfield mask) {glClear(mask);}
void wrapglClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {glClearColor(red, green, blue, alpha);}
void wrapglClearDepth(GLclampd depth) {glClearDepthf((float)depth);}
void wrapglClearStencil(GLint s) {glClearStencil(s);}
void wrapglClientActiveTexture(GLenum target) {Con_Printf("glClientActiveTexture(target)\n");}
void wrapglColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {Con_Printf("glColor4f(red, green, blue, alpha)\n");}
void wrapglColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {Con_Printf("glColor4ub(red, green, blue, alpha)\n");}
void wrapglColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {glColorMask(red, green, blue, alpha);}
void wrapglColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {Con_Printf("glColorPointer(size, type, stride, ptr)\n");}
void wrapglCompileShader(GLuint shaderObj) {glCompileShader(shaderObj);}
void wrapglCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border,  GLsizei imageSize, const void *data) {glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);}
void wrapglCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data) {Con_Printf("glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data)\n");}
void wrapglCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);}
void wrapglCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data) {Con_Printf("glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data)\n");}
void wrapglCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);}
void wrapglCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);}
void wrapglCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {Con_Printf("glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height)\n");}
void wrapglCullFace(GLenum mode) {glCullFace(mode);}
void wrapglDeleteBuffers(GLsizei n, const GLuint *buffers) {glDeleteBuffers(n, buffers);}
void wrapglDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {glDeleteFramebuffers(n, framebuffers);}
void wrapglDeleteShader(GLuint obj) {glDeleteShader(obj);}
void wrapglDeleteProgram(GLuint obj) {glDeleteProgram(obj);}
//void wrapglDeleteQueries(GLsizei n, const GLuint *ids) {glDeleteQueries(n, ids);}
void wrapglDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {glDeleteRenderbuffers(n, renderbuffers);}
void wrapglDeleteTextures(GLsizei n, const GLuint *textures) {glDeleteTextures(n, textures);}
void wrapglDepthFunc(GLenum func) {glDepthFunc(func);}
void wrapglDepthMask(GLboolean flag) {glDepthMask(flag);}
void wrapglDepthRange(GLclampd near_val, GLclampd far_val) {glDepthRangef((float)near_val, (float)far_val);}
void wrapglDetachShader(GLuint containerObj, GLuint attachedObj) {glDetachShader(containerObj, attachedObj);}
void wrapglDisable(GLenum cap) {glDisable(cap);}
void wrapglDisableClientState(GLenum cap) {Con_Printf("glDisableClientState(cap)\n");}
void wrapglDisableVertexAttribArray(GLuint index) {glDisableVertexAttribArray(index);}
void wrapglDrawArrays(GLenum mode, GLint first, GLsizei count) {glDrawArrays(mode, first, count);}
void wrapglDrawBuffer(GLenum mode) {Con_Printf("glDrawBuffer(mode)\n");}
void wrapglDrawBuffers(GLsizei n, const GLenum *bufs) {Con_Printf("glDrawBuffers(n, bufs)\n");}
void wrapglDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {glDrawElements(mode, count, type, indices);}
//void wrapglDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices) {glDrawRangeElements(mode, start, end, count, type, indices);}
//void wrapglDrawRangeElementsEXT(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices) {glDrawRangeElements(mode, start, end, count, type, indices);}
void wrapglEnable(GLenum cap) {glEnable(cap);}
void wrapglEnableClientState(GLenum cap) {Con_Printf("glEnableClientState(cap)\n");}
void wrapglEnableVertexAttribArray(GLuint index) {glEnableVertexAttribArray(index);}
void wrapglEnd(void) {Con_Printf("glEnd()\n");}
//void wrapglEndQuery(GLenum target) {glEndQuery(target);}
void wrapglFinish(void) {glFinish();}
void wrapglFlush(void) {glFlush();}
void wrapglFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);}
void wrapglFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {glFramebufferTexture2D(target, attachment, textarget, texture, level);}
void wrapglFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {Con_Printf("glFramebufferTexture3D()\n");}
void wrapglGenBuffers(GLsizei n, GLuint *buffers) {glGenBuffers(n, buffers);}
void wrapglGenFramebuffers(GLsizei n, GLuint *framebuffers) {glGenFramebuffers(n, framebuffers);}
//void wrapglGenQueries(GLsizei n, GLuint *ids) {glGenQueries(n, ids);}
void wrapglGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {glGenRenderbuffers(n, renderbuffers);}
void wrapglGenTextures(GLsizei n, GLuint *textures) {glGenTextures(n, textures);}
void wrapglGenerateMipmap(GLenum target) {glGenerateMipmap(target);}
void wrapglGetActiveAttrib(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {glGetActiveAttrib(programObj, index, maxLength, length, size, type, name);}
void wrapglGetActiveUniform(GLuint programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {glGetActiveUniform(programObj, index, maxLength, length, size, type, name);}
void wrapglGetAttachedShaders(GLuint containerObj, GLsizei maxCount, GLsizei *count, GLuint *obj) {glGetAttachedShaders(containerObj, maxCount, count, obj);}
void wrapglGetBooleanv(GLenum pname, GLboolean *params) {glGetBooleanv(pname, params);}
void wrapglGetCompressedTexImage(GLenum target, GLint lod, void *img) {Con_Printf("glGetCompressedTexImage(target, lod, img)\n");}
void wrapglGetDoublev(GLenum pname, GLdouble *params) {Con_Printf("glGetDoublev(pname, params)\n");}
void wrapglGetFloatv(GLenum pname, GLfloat *params) {glGetFloatv(pname, params);}
void wrapglGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);}
void wrapglGetShaderInfoLog(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {glGetShaderInfoLog(obj, maxLength, length, infoLog);}
void wrapglGetProgramInfoLog(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {glGetProgramInfoLog(obj, maxLength, length, infoLog);}
void wrapglGetIntegerv(GLenum pname, GLint *params) {glGetIntegerv(pname, params);}
void wrapglGetShaderiv(GLuint obj, GLenum pname, GLint *params) {glGetShaderiv(obj, pname, params);}
void wrapglGetProgramiv(GLuint obj, GLenum pname, GLint *params) {glGetProgramiv(obj, pname, params);}
//void wrapglGetQueryObjectiv(GLuint qid, GLenum pname, GLint *params) {glGetQueryObjectiv(qid, pname, params);}
//void wrapglGetQueryObjectuiv(GLuint qid, GLenum pname, GLuint *params) {glGetQueryObjectuiv(qid, pname, params);}
//void wrapglGetQueryiv(GLenum target, GLenum pname, GLint *params) {glGetQueryiv(target, pname, params);}
void wrapglGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {glGetRenderbufferParameteriv(target, pname, params);}
void wrapglGetShaderSource(GLuint obj, GLsizei maxLength, GLsizei *length, GLchar *source) {glGetShaderSource(obj, maxLength, length, source);}
void wrapglGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) {Con_Printf("glGetTexImage(target, level, format, type, pixels)\n");}
void wrapglGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {Con_Printf("glGetTexLevelParameterfv(target, level, pname, params)\n");}
void wrapglGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {Con_Printf("glGetTexLevelParameteriv(target, level, pname, params)\n");}
void wrapglGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {glGetTexParameterfv(target, pname, params);}
void wrapglGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {glGetTexParameteriv(target, pname, params);}
void wrapglGetUniformfv(GLuint programObj, GLint location, GLfloat *params) {glGetUniformfv(programObj, location, params);}
void wrapglGetUniformiv(GLuint programObj, GLint location, GLint *params) {glGetUniformiv(programObj, location, params);}
void wrapglHint(GLenum target, GLenum mode) {glHint(target, mode);}
void wrapglLineWidth(GLfloat width) {glLineWidth(width);}
void wrapglLinkProgram(GLuint programObj) {glLinkProgram(programObj);}
void wrapglLoadIdentity(void) {Con_Printf("glLoadIdentity()\n");}
void wrapglLoadMatrixf(const GLfloat *m) {Con_Printf("glLoadMatrixf(m)\n");}
void wrapglMatrixMode(GLenum mode) {Con_Printf("glMatrixMode(mode)\n");}
void wrapglMultiTexCoord1f(GLenum target, GLfloat s) {Con_Printf("glMultiTexCoord1f(target, s)\n");}
void wrapglMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {Con_Printf("glMultiTexCoord2f(target, s, t)\n");}
void wrapglMultiTexCoord3f(GLenum target, GLfloat s, GLfloat t, GLfloat r) {Con_Printf("glMultiTexCoord3f(target, s, t, r)\n");}
void wrapglMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {Con_Printf("glMultiTexCoord4f(target, s, t, r, q)\n");}
void wrapglNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr) {Con_Printf("glNormalPointer(type, stride, ptr)\n");}
void wrapglPixelStorei(GLenum pname, GLint param) {glPixelStorei(pname, param);}
void wrapglPointSize(GLfloat size) {Con_Printf("glPointSize(size)\n");}
void wrapglPolygonMode(GLenum face, GLenum mode) {Con_Printf("glPolygonMode(face, mode)\n");}
void wrapglPolygonOffset(GLfloat factor, GLfloat units) {glPolygonOffset(factor, units);}
void wrapglPolygonStipple(const GLubyte *mask) {Con_Printf("glPolygonStipple(mask)\n");}
void wrapglReadBuffer(GLenum mode) {Con_Printf("glReadBuffer(mode)\n");}
void wrapglReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels) {glReadPixels(x, y, width, height, format, type, pixels);}
void wrapglRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {glRenderbufferStorage(target, internalformat, width, height);}
void wrapglScissor(GLint x, GLint y, GLsizei width, GLsizei height) {glScissor(x, y, width, height);}
void wrapglShaderSource(GLuint shaderObj, GLsizei count, const GLchar **string, const GLint *length) {glShaderSource(shaderObj, count, string, length);}
void wrapglStencilFunc(GLenum func, GLint ref, GLuint mask) {glStencilFunc(func, ref, mask);}
void wrapglStencilFuncSeparate(GLenum func1, GLenum func2, GLint ref, GLuint mask) {Con_Printf("glStencilFuncSeparate(func1, func2, ref, mask)\n");}
void wrapglStencilMask(GLuint mask) {glStencilMask(mask);}
void wrapglStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {glStencilOp(fail, zfail, zpass);}
void wrapglStencilOpSeparate(GLenum e1, GLenum e2, GLenum e3, GLenum e4) {Con_Printf("glStencilOpSeparate(e1, e2, e3, e4)\n");}
void wrapglTexCoord1f(GLfloat s) {Con_Printf("glTexCoord1f(s)\n");}
void wrapglTexCoord2f(GLfloat s, GLfloat t) {Con_Printf("glTexCoord2f(s, t)\n");}
void wrapglTexCoord3f(GLfloat s, GLfloat t, GLfloat r) {Con_Printf("glTexCoord3f(s, t, r)\n");}
void wrapglTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {Con_Printf("glTexCoord4f(s, t, r, q)\n");}
void wrapglTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {Con_Printf("glTexCoordPointer(size, type, stride, ptr)\n");}
void wrapglTexEnvf(GLenum target, GLenum pname, GLfloat param) {Con_Printf("glTexEnvf(target, pname, param)\n");}
void wrapglTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {Con_Printf("glTexEnvfv(target, pname, params)\n");}
void wrapglTexEnvi(GLenum target, GLenum pname, GLint param) {Con_Printf("glTexEnvi(target, pname, param)\n");}
void wrapglTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {glTexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);}
void wrapglTexImage3D(GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {Con_Printf("glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels)\n");}
void wrapglTexParameterf(GLenum target, GLenum pname, GLfloat param) {glTexParameterf(target, pname, param);}
void wrapglTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {glTexParameterfv(target, pname, params);}
void wrapglTexParameteri(GLenum target, GLenum pname, GLint param) {glTexParameteri(target, pname, param);}
void wrapglTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);}
void wrapglTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels) {Con_Printf("glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels)\n");}
void wrapglUniform1f(GLint location, GLfloat v0) {glUniform1f(location, v0);}
void wrapglUniform1fv(GLint location, GLsizei count, const GLfloat *value) {glUniform1fv(location, count, value);}
void wrapglUniform1i(GLint location, GLint v0) {glUniform1i(location, v0);}
void wrapglUniform1iv(GLint location, GLsizei count, const GLint *value) {glUniform1iv(location, count, value);}
void wrapglUniform2f(GLint location, GLfloat v0, GLfloat v1) {glUniform2f(location, v0, v1);}
void wrapglUniform2fv(GLint location, GLsizei count, const GLfloat *value) {glUniform2fv(location, count, value);}
void wrapglUniform2i(GLint location, GLint v0, GLint v1) {glUniform2i(location, v0, v1);}
void wrapglUniform2iv(GLint location, GLsizei count, const GLint *value) {glUniform2iv(location, count, value);}
void wrapglUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {glUniform3f(location, v0, v1, v2);}
void wrapglUniform3fv(GLint location, GLsizei count, const GLfloat *value) {glUniform3fv(location, count, value);}
void wrapglUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {glUniform3i(location, v0, v1, v2);}
void wrapglUniform3iv(GLint location, GLsizei count, const GLint *value) {glUniform3iv(location, count, value);}
void wrapglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {glUniform4f(location, v0, v1, v2, v3);}
void wrapglUniform4fv(GLint location, GLsizei count, const GLfloat *value) {glUniform4fv(location, count, value);}
void wrapglUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {glUniform4i(location, v0, v1, v2, v3);}
void wrapglUniform4iv(GLint location, GLsizei count, const GLint *value) {glUniform4iv(location, count, value);}
void wrapglUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {glUniformMatrix2fv(location, count, transpose, value);}
void wrapglUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {glUniformMatrix3fv(location, count, transpose, value);}
void wrapglUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {glUniformMatrix4fv(location, count, transpose, value);}
void wrapglUseProgram(GLuint programObj) {glUseProgram(programObj);}
void wrapglValidateProgram(GLuint programObj) {glValidateProgram(programObj);}
void wrapglVertex2f(GLfloat x, GLfloat y) {Con_Printf("glVertex2f(x, y)\n");}
void wrapglVertex3f(GLfloat x, GLfloat y, GLfloat z) {Con_Printf("glVertex3f(x, y, z)\n");}
void wrapglVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {Con_Printf("glVertex4f(x, y, z, w)\n");}
void wrapglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) {glVertexAttribPointer(index, size, type, normalized, stride, pointer);}
void wrapglVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {Con_Printf("glVertexPointer(size, type, stride, ptr)\n");}
void wrapglViewport(GLint x, GLint y, GLsizei width, GLsizei height) {glViewport(x, y, width, height);}
void wrapglVertexAttrib1f(GLuint index, GLfloat v0) {glVertexAttrib1f(index, v0);}
//void wrapglVertexAttrib1s(GLuint index, GLshort v0) {glVertexAttrib1s(index, v0);}
//void wrapglVertexAttrib1d(GLuint index, GLdouble v0) {glVertexAttrib1d(index, v0);}
void wrapglVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {glVertexAttrib2f(index, v0, v1);}
//void wrapglVertexAttrib2s(GLuint index, GLshort v0, GLshort v1) {glVertexAttrib2s(index, v0, v1);}
//void wrapglVertexAttrib2d(GLuint index, GLdouble v0, GLdouble v1) {glVertexAttrib2d(index, v0, v1);}
void wrapglVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {glVertexAttrib3f(index, v0, v1, v2);}
//void wrapglVertexAttrib3s(GLuint index, GLshort v0, GLshort v1, GLshort v2) {glVertexAttrib3s(index, v0, v1, v2);}
//void wrapglVertexAttrib3d(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2) {glVertexAttrib3d(index, v0, v1, v2);}
void wrapglVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {glVertexAttrib4f(index, v0, v1, v2, v3);}
//void wrapglVertexAttrib4s(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3) {glVertexAttrib4s(index, v0, v1, v2, v3);}
//void wrapglVertexAttrib4d(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3) {glVertexAttrib4d(index, v0, v1, v2, v3);}
//void wrapglVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) {glVertexAttrib4Nub(index, x, y, z, w);}
void wrapglVertexAttrib1fv(GLuint index, const GLfloat *v) {glVertexAttrib1fv(index, v);}
//void wrapglVertexAttrib1sv(GLuint index, const GLshort *v) {glVertexAttrib1sv(index, v);}
//void wrapglVertexAttrib1dv(GLuint index, const GLdouble *v) {glVertexAttrib1dv(index, v);}
void wrapglVertexAttrib2fv(GLuint index, const GLfloat *v) {glVertexAttrib2fv(index, v);}
//void wrapglVertexAttrib2sv(GLuint index, const GLshort *v) {glVertexAttrib2sv(index, v);}
//void wrapglVertexAttrib2dv(GLuint index, const GLdouble *v) {glVertexAttrib2dv(index, v);}
void wrapglVertexAttrib3fv(GLuint index, const GLfloat *v) {glVertexAttrib3fv(index, v);}
//void wrapglVertexAttrib3sv(GLuint index, const GLshort *v) {glVertexAttrib3sv(index, v);}
//void wrapglVertexAttrib3dv(GLuint index, const GLdouble *v) {glVertexAttrib3dv(index, v);}
void wrapglVertexAttrib4fv(GLuint index, const GLfloat *v) {glVertexAttrib4fv(index, v);}
//void wrapglVertexAttrib4sv(GLuint index, const GLshort *v) {glVertexAttrib4sv(index, v);}
//void wrapglVertexAttrib4dv(GLuint index, const GLdouble *v) {glVertexAttrib4dv(index, v);}
//void wrapglVertexAttrib4iv(GLuint index, const GLint *v) {glVertexAttrib4iv(index, v);}
//void wrapglVertexAttrib4bv(GLuint index, const GLbyte *v) {glVertexAttrib4bv(index, v);}
//void wrapglVertexAttrib4ubv(GLuint index, const GLubyte *v) {glVertexAttrib4ubv(index, v);}
//void wrapglVertexAttrib4usv(GLuint index, const GLushort *v) {glVertexAttrib4usv(index, GLushort v);}
//void wrapglVertexAttrib4uiv(GLuint index, const GLuint *v) {glVertexAttrib4uiv(index, v);}
//void wrapglVertexAttrib4Nbv(GLuint index, const GLbyte *v) {glVertexAttrib4Nbv(index, v);}
//void wrapglVertexAttrib4Nsv(GLuint index, const GLshort *v) {glVertexAttrib4Nsv(index, v);}
//void wrapglVertexAttrib4Niv(GLuint index, const GLint *v) {glVertexAttrib4Niv(index, v);}
//void wrapglVertexAttrib4Nubv(GLuint index, const GLubyte *v) {glVertexAttrib4Nubv(index, v);}
//void wrapglVertexAttrib4Nusv(GLuint index, const GLushort *v) {glVertexAttrib4Nusv(index, GLushort v);}
//void wrapglVertexAttrib4Nuiv(GLuint index, const GLuint *v) {glVertexAttrib4Nuiv(index, v);}
//void wrapglGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params) {glGetVertexAttribdv(index, pname, params);}
void wrapglGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {glGetVertexAttribfv(index, pname, params);}
void wrapglGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {glGetVertexAttribiv(index, pname, params);}
void wrapglGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid **pointer) {glGetVertexAttribPointerv(index, pname, pointer);}

void GLES_Init(void)
{
	qglIsBufferARB = wrapglIsBuffer;
	qglIsEnabled = wrapglIsEnabled;
	qglIsFramebufferEXT = wrapglIsFramebuffer;
//	qglIsQueryARB = wrapglIsQuery;
	qglIsRenderbufferEXT = wrapglIsRenderbuffer;
//	qglUnmapBufferARB = wrapglUnmapBuffer;
	qglCheckFramebufferStatusEXT = wrapglCheckFramebufferStatus;
	qglGetError = wrapglGetError;
	qglCreateProgram = wrapglCreateProgram;
	qglCreateShader = wrapglCreateShader;
//	qglGetHandleARB = wrapglGetHandle;
	qglGetAttribLocation = wrapglGetAttribLocation;
	qglGetUniformLocation = wrapglGetUniformLocation;
//	qglMapBufferARB = wrapglMapBuffer;
	qglGetString = wrapglGetString;
//	qglActiveStencilFaceEXT = wrapglActiveStencilFace;
	qglActiveTexture = wrapglActiveTexture;
	qglAlphaFunc = wrapglAlphaFunc;
	qglArrayElement = wrapglArrayElement;
	qglAttachShader = wrapglAttachShader;
	qglBegin = wrapglBegin;
//	qglBeginQueryARB = wrapglBeginQuery;
	qglBindAttribLocation = wrapglBindAttribLocation;
	qglBindBufferARB = wrapglBindBuffer;
	qglBindFramebufferEXT = wrapglBindFramebuffer;
	qglBindRenderbufferEXT = wrapglBindRenderbuffer;
	qglBindTexture = wrapglBindTexture;
	qglBlendEquationEXT = wrapglBlendEquation;
	qglBlendFunc = wrapglBlendFunc;
	qglBufferDataARB = wrapglBufferData;
	qglBufferSubDataARB = wrapglBufferSubData;
	qglClear = wrapglClear;
	qglClearColor = wrapglClearColor;
	qglClearDepth = wrapglClearDepth;
	qglClearStencil = wrapglClearStencil;
	qglClientActiveTexture = wrapglClientActiveTexture;
	qglColor4f = wrapglColor4f;
	qglColor4ub = wrapglColor4ub;
	qglColorMask = wrapglColorMask;
	qglColorPointer = wrapglColorPointer;
	qglCompileShader = wrapglCompileShader;
	qglCompressedTexImage2DARB = wrapglCompressedTexImage2D;
	qglCompressedTexImage3DARB = wrapglCompressedTexImage3D;
	qglCompressedTexSubImage2DARB = wrapglCompressedTexSubImage2D;
	qglCompressedTexSubImage3DARB = wrapglCompressedTexSubImage3D;
	qglCopyTexImage2D = wrapglCopyTexImage2D;
	qglCopyTexSubImage2D = wrapglCopyTexSubImage2D;
	qglCopyTexSubImage3D = wrapglCopyTexSubImage3D;
	qglCullFace = wrapglCullFace;
	qglDeleteBuffersARB = wrapglDeleteBuffers;
	qglDeleteFramebuffersEXT = wrapglDeleteFramebuffers;
	qglDeleteProgram = wrapglDeleteProgram;
	qglDeleteShader = wrapglDeleteShader;
//	qglDeleteQueriesARB = wrapglDeleteQueries;
	qglDeleteRenderbuffersEXT = wrapglDeleteRenderbuffers;
	qglDeleteTextures = wrapglDeleteTextures;
	qglDepthFunc = wrapglDepthFunc;
	qglDepthMask = wrapglDepthMask;
	qglDepthRange = wrapglDepthRange;
	qglDetachShader = wrapglDetachShader;
	qglDisable = wrapglDisable;
	qglDisableClientState = wrapglDisableClientState;
	qglDisableVertexAttribArray = wrapglDisableVertexAttribArray;
	qglDrawArrays = wrapglDrawArrays;
//	qglDrawBuffer = wrapglDrawBuffer;
//	qglDrawBuffersARB = wrapglDrawBuffers;
	qglDrawElements = wrapglDrawElements;
//	qglDrawRangeElements = wrapglDrawRangeElements;
	qglEnable = wrapglEnable;
	qglEnableClientState = wrapglEnableClientState;
	qglEnableVertexAttribArray = wrapglEnableVertexAttribArray;
	qglEnd = wrapglEnd;
//	qglEndQueryARB = wrapglEndQuery;
	qglFinish = wrapglFinish;
	qglFlush = wrapglFlush;
	qglFramebufferRenderbufferEXT = wrapglFramebufferRenderbuffer;
	qglFramebufferTexture2DEXT = wrapglFramebufferTexture2D;
	qglFramebufferTexture3DEXT = wrapglFramebufferTexture3D;
	qglGenBuffersARB = wrapglGenBuffers;
	qglGenFramebuffersEXT = wrapglGenFramebuffers;
//	qglGenQueriesARB = wrapglGenQueries;
	qglGenRenderbuffersEXT = wrapglGenRenderbuffers;
	qglGenTextures = wrapglGenTextures;
	qglGenerateMipmapEXT = wrapglGenerateMipmap;
	qglGetActiveAttrib = wrapglGetActiveAttrib;
	qglGetActiveUniform = wrapglGetActiveUniform;
	qglGetAttachedShaders = wrapglGetAttachedShaders;
	qglGetBooleanv = wrapglGetBooleanv;
//	qglGetCompressedTexImageARB = wrapglGetCompressedTexImage;
	qglGetDoublev = wrapglGetDoublev;
	qglGetFloatv = wrapglGetFloatv;
	qglGetFramebufferAttachmentParameterivEXT = wrapglGetFramebufferAttachmentParameteriv;
	qglGetProgramInfoLog = wrapglGetProgramInfoLog;
	qglGetShaderInfoLog = wrapglGetShaderInfoLog;
	qglGetIntegerv = wrapglGetIntegerv;
	qglGetShaderiv = wrapglGetShaderiv;
	qglGetProgramiv = wrapglGetProgramiv;
//	qglGetQueryObjectivARB = wrapglGetQueryObjectiv;
//	qglGetQueryObjectuivARB = wrapglGetQueryObjectuiv;
//	qglGetQueryivARB = wrapglGetQueryiv;
	qglGetRenderbufferParameterivEXT = wrapglGetRenderbufferParameteriv;
	qglGetShaderSource = wrapglGetShaderSource;
	qglGetTexImage = wrapglGetTexImage;
	qglGetTexLevelParameterfv = wrapglGetTexLevelParameterfv;
	qglGetTexLevelParameteriv = wrapglGetTexLevelParameteriv;
	qglGetTexParameterfv = wrapglGetTexParameterfv;
	qglGetTexParameteriv = wrapglGetTexParameteriv;
	qglGetUniformfv = wrapglGetUniformfv;
	qglGetUniformiv = wrapglGetUniformiv;
	qglHint = wrapglHint;
	qglLineWidth = wrapglLineWidth;
	qglLinkProgram = wrapglLinkProgram;
	qglLoadIdentity = wrapglLoadIdentity;
	qglLoadMatrixf = wrapglLoadMatrixf;
	qglMatrixMode = wrapglMatrixMode;
	qglMultiTexCoord1f = wrapglMultiTexCoord1f;
	qglMultiTexCoord2f = wrapglMultiTexCoord2f;
	qglMultiTexCoord3f = wrapglMultiTexCoord3f;
	qglMultiTexCoord4f = wrapglMultiTexCoord4f;
	qglNormalPointer = wrapglNormalPointer;
	qglPixelStorei = wrapglPixelStorei;
	qglPointSize = wrapglPointSize;
	qglPolygonMode = wrapglPolygonMode;
	qglPolygonOffset = wrapglPolygonOffset;
//	qglPolygonStipple = wrapglPolygonStipple;
	qglReadBuffer = wrapglReadBuffer;
	qglReadPixels = wrapglReadPixels;
	qglRenderbufferStorageEXT = wrapglRenderbufferStorage;
	qglScissor = wrapglScissor;
	qglShaderSource = wrapglShaderSource;
	qglStencilFunc = wrapglStencilFunc;
	qglStencilFuncSeparate = wrapglStencilFuncSeparate;
	qglStencilMask = wrapglStencilMask;
	qglStencilOp = wrapglStencilOp;
	qglStencilOpSeparate = wrapglStencilOpSeparate;
	qglTexCoord1f = wrapglTexCoord1f;
	qglTexCoord2f = wrapglTexCoord2f;
	qglTexCoord3f = wrapglTexCoord3f;
	qglTexCoord4f = wrapglTexCoord4f;
	qglTexCoordPointer = wrapglTexCoordPointer;
	qglTexEnvf = wrapglTexEnvf;
	qglTexEnvfv = wrapglTexEnvfv;
	qglTexEnvi = wrapglTexEnvi;
	qglTexImage2D = wrapglTexImage2D;
	qglTexImage3D = wrapglTexImage3D;
	qglTexParameterf = wrapglTexParameterf;
	qglTexParameterfv = wrapglTexParameterfv;
	qglTexParameteri = wrapglTexParameteri;
	qglTexSubImage2D = wrapglTexSubImage2D;
	qglTexSubImage3D = wrapglTexSubImage3D;
	qglUniform1f = wrapglUniform1f;
	qglUniform1fv = wrapglUniform1fv;
	qglUniform1i = wrapglUniform1i;
	qglUniform1iv = wrapglUniform1iv;
	qglUniform2f = wrapglUniform2f;
	qglUniform2fv = wrapglUniform2fv;
	qglUniform2i = wrapglUniform2i;
	qglUniform2iv = wrapglUniform2iv;
	qglUniform3f = wrapglUniform3f;
	qglUniform3fv = wrapglUniform3fv;
	qglUniform3i = wrapglUniform3i;
	qglUniform3iv = wrapglUniform3iv;
	qglUniform4f = wrapglUniform4f;
	qglUniform4fv = wrapglUniform4fv;
	qglUniform4i = wrapglUniform4i;
	qglUniform4iv = wrapglUniform4iv;
	qglUniformMatrix2fv = wrapglUniformMatrix2fv;
	qglUniformMatrix3fv = wrapglUniformMatrix3fv;
	qglUniformMatrix4fv = wrapglUniformMatrix4fv;
	qglUseProgram = wrapglUseProgram;
	qglValidateProgram = wrapglValidateProgram;
	qglVertex2f = wrapglVertex2f;
	qglVertex3f = wrapglVertex3f;
	qglVertex4f = wrapglVertex4f;
	qglVertexAttribPointer = wrapglVertexAttribPointer;
	qglVertexPointer = wrapglVertexPointer;
	qglViewport = wrapglViewport;
	qglVertexAttrib1f = wrapglVertexAttrib1f;
//	qglVertexAttrib1s = wrapglVertexAttrib1s;
//	qglVertexAttrib1d = wrapglVertexAttrib1d;
	qglVertexAttrib2f = wrapglVertexAttrib2f;
//	qglVertexAttrib2s = wrapglVertexAttrib2s;
//	qglVertexAttrib2d = wrapglVertexAttrib2d;
	qglVertexAttrib3f = wrapglVertexAttrib3f;
//	qglVertexAttrib3s = wrapglVertexAttrib3s;
//	qglVertexAttrib3d = wrapglVertexAttrib3d;
	qglVertexAttrib4f = wrapglVertexAttrib4f;
//	qglVertexAttrib4s = wrapglVertexAttrib4s;
//	qglVertexAttrib4d = wrapglVertexAttrib4d;
//	qglVertexAttrib4Nub = wrapglVertexAttrib4Nub;
	qglVertexAttrib1fv = wrapglVertexAttrib1fv;
//	qglVertexAttrib1sv = wrapglVertexAttrib1sv;
//	qglVertexAttrib1dv = wrapglVertexAttrib1dv;
	qglVertexAttrib2fv = wrapglVertexAttrib2fv;
//	qglVertexAttrib2sv = wrapglVertexAttrib2sv;
//	qglVertexAttrib2dv = wrapglVertexAttrib2dv;
	qglVertexAttrib3fv = wrapglVertexAttrib3fv;
//	qglVertexAttrib3sv = wrapglVertexAttrib3sv;
//	qglVertexAttrib3dv = wrapglVertexAttrib3dv;
	qglVertexAttrib4fv = wrapglVertexAttrib4fv;
//	qglVertexAttrib4sv = wrapglVertexAttrib4sv;
//	qglVertexAttrib4dv = wrapglVertexAttrib4dv;
//	qglVertexAttrib4iv = wrapglVertexAttrib4iv;
//	qglVertexAttrib4bv = wrapglVertexAttrib4bv;
//	qglVertexAttrib4ubv = wrapglVertexAttrib4ubv;
//	qglVertexAttrib4usv = wrapglVertexAttrib4usv;
//	qglVertexAttrib4uiv = wrapglVertexAttrib4uiv;
//	qglVertexAttrib4Nbv = wrapglVertexAttrib4Nbv;
//	qglVertexAttrib4Nsv = wrapglVertexAttrib4Nsv;
//	qglVertexAttrib4Niv = wrapglVertexAttrib4Niv;
//	qglVertexAttrib4Nubv = wrapglVertexAttrib4Nubv;
//	qglVertexAttrib4Nusv = wrapglVertexAttrib4Nusv;
//	qglVertexAttrib4Nuiv = wrapglVertexAttrib4Nuiv;
//	qglGetVertexAttribdv = wrapglGetVertexAttribdv;
	qglGetVertexAttribfv = wrapglGetVertexAttribfv;
	qglGetVertexAttribiv = wrapglGetVertexAttribiv;
	qglGetVertexAttribPointerv = wrapglGetVertexAttribPointerv;

	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	
	if (!gl_extensions)
		gl_extensions = "";
	if (!gl_platformextensions)
		gl_platformextensions = "";
	
	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	Con_DPrintf("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_DPrintf("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);
	
	// LordHavoc: report supported extensions
	Con_DPrintf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );
	
	vid.support.gl20shaders = true;
	vid.support.amd_texture_texture4 = false;
	vid.support.arb_depth_texture = false;
	vid.support.arb_draw_buffers = false;
	vid.support.arb_multitexture = false;
	vid.support.arb_occlusion_query = false;
	vid.support.arb_shadow = false;
	vid.support.arb_texture_compression = false; // different (vendor-specific) formats than on desktop OpenGL...
	vid.support.arb_texture_cube_map = true;
	vid.support.arb_texture_env_combine = false;
	vid.support.arb_texture_gather = false;
	vid.support.arb_texture_non_power_of_two = strstr(gl_extensions, "GL_OES_texture_npot") != NULL;
	vid.support.arb_vertex_buffer_object = true;
	vid.support.ati_separate_stencil = false;
	vid.support.ext_blend_minmax = false;
	vid.support.ext_blend_subtract = true;
	vid.support.ext_draw_range_elements = true;
	vid.support.ext_framebuffer_object = false;//true;
	vid.support.ext_stencil_two_side = false;
	vid.support.ext_texture_3d = false;//SDL_GL_ExtensionSupported("GL_OES_texture_3D"); // iPhoneOS does not support 3D textures, odd...
	vid.support.ext_texture_compression_s3tc = false;
	vid.support.ext_texture_edge_clamp = true;
	vid.support.ext_texture_filter_anisotropic = false; // probably don't want to use it...

	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_2d);
	if (vid.support.ext_texture_filter_anisotropic)
		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*)&vid.max_anisotropy);
	if (vid.support.arb_texture_cube_map)
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, (GLint*)&vid.maxtexturesize_cubemap);
	if (vid.support.ext_texture_3d)
		qglGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, (GLint*)&vid.maxtexturesize_3d);
	Con_Printf("GL_MAX_CUBE_MAP_TEXTURE_SIZE = %i\n", vid.maxtexturesize_cubemap);
	Con_Printf("GL_MAX_3D_TEXTURE_SIZE = %i\n", vid.maxtexturesize_3d);

	// verify that cubemap textures are really supported
	if (vid.support.arb_texture_cube_map && vid.maxtexturesize_cubemap < 256)
		vid.support.arb_texture_cube_map = false;
	
	// verify that 3d textures are really supported
	if (vid.support.ext_texture_3d && vid.maxtexturesize_3d < 32)
	{
		vid.support.ext_texture_3d = false;
		Con_Printf("GL_OES_texture_3d reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n");
	}

	vid.texunits = 4;
	vid.teximageunits = 8;
	vid.texarrayunits = 5;
	vid.texunits = bound(1, vid.texunits, MAX_TEXTUREUNITS);
	vid.teximageunits = bound(1, vid.teximageunits, MAX_TEXTUREUNITS);
	vid.texarrayunits = bound(1, vid.texarrayunits, MAX_TEXTUREUNITS);
	Con_DPrintf("Using GLES2.0 rendering path - %i texture matrix, %i texture images, %i texcoords%s\n", vid.texunits, vid.teximageunits, vid.texarrayunits, vid.support.ext_framebuffer_object ? ", shadowmapping supported" : "");
	vid.renderpath = RENDERPATH_GLES2;
	vid.useinterleavedarrays = false;

	// VorteX: set other info (maybe place them in VID_InitMode?)
	extern cvar_t gl_info_vendor;
	extern cvar_t gl_info_renderer;
	extern cvar_t gl_info_version;
	extern cvar_t gl_info_platform;
	extern cvar_t gl_info_driver;
	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);
}
#endif

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	p = SDL_GL_GetProcAddress(name);
	return p;
}

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
static int Sys_EventFilter( SDL_Event *event );
#endif
static qboolean vid_sdl_initjoysticksystem = false;

void VID_Init (void)
{
#ifndef __IPHONEOS__
#ifdef MACOSX
	Cvar_RegisterVariable(&apple_mouse_noaccel);
#endif
#endif
	Cvar_RegisterVariable(&vid_soft);
	Cvar_RegisterVariable(&vid_soft_threads);
	Cvar_RegisterVariable(&vid_soft_interlace);
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
	Cvar_RegisterVariable(&joy_axiskeyevents);
	
#ifdef SDL_R_RESTART
	R_RegisterModule("SDL", sdl_start, sdl_shutdown, sdl_newmap, NULL, NULL);
#endif

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Failed to init SDL video subsystem: %s", SDL_GetError());
	vid_sdl_initjoysticksystem = SDL_InitSubSystem(SDL_INIT_JOYSTICK) >= 0;
	if (vid_sdl_initjoysticksystem)
		Con_Printf("Failed to init SDL joystick subsystem: %s\n", SDL_GetError());
	vid_isfullscreen = false;
}

// set the icon (we dont use SDL here since it would be too much a PITA)
#ifdef WIN32
#include "resource.h"
#include <SDL_syswm.h>
static void VID_SetCaption(void)
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
static void VID_SetIcon_Pre(void)
{
}
static void VID_SetIcon_Post(void)
{
}
#else
// Adding the OS independent XPM version --blub
#include "darkplaces.xpm"
#include "nexuiz.xpm"
static SDL_Surface *icon = NULL;
static void VID_SetIcon_Pre(void)
{
	/*
	 * Somewhat restricted XPM reader. Only supports XPMs saved by GIMP 2.4 at
	 * default settings with less than 91 colors and transparency.
	 */

	int width, height, colors, isize, i, j;
	int thenone = -1;
	static SDL_Color palette[256];
	unsigned short palenc[256]; // store color id by char
	char *xpm;
	char **idata, *data;
	const SDL_version *version;

	version = SDL_Linked_Version();
	// only use non-XPM icon support in SDL v1.3 and higher
	// SDL v1.2 does not support "smooth" transparency, and thus is better
	// off the xpm way
	if(version->major >= 2 || (version->major == 1 && version->minor >= 3))
	{
		data = (char *) loadimagepixelsbgra("darkplaces-icon", false, false, false, NULL);
		if(data)
		{
			unsigned int red = 0x00FF0000;
			unsigned int green = 0x0000FF00;
			unsigned int blue = 0x000000FF;
			unsigned int alpha = 0xFF000000;
			width = image_width;
			height = image_height;

			// reallocate with malloc, as this is in tempmempool (do not want)
			xpm = data;
			data = malloc(width * height * 4);
			memcpy(data, xpm, width * height * 4);
			Mem_Free(xpm);
			xpm = NULL;

			icon = SDL_CreateRGBSurface(SDL_SRCALPHA, width, height, 32, LittleLong(red), LittleLong(green), LittleLong(blue), LittleLong(alpha));

			if(icon == NULL) {
				Con_Printf(	"Failed to create surface for the window Icon!\n"
						"%s\n", SDL_GetError());
				free(data);
				return;
			}

			icon->pixels = data;
		}
	}

	// we only get here if non-XPM icon was missing, or SDL version is not
	// sufficient for transparent non-XPM icons
	if(!icon)
	{
		xpm = (char *) FS_LoadFile("darkplaces-icon.xpm", tempmempool, false, NULL);
		idata = NULL;
		if(xpm)
			idata = XPM_DecodeString(xpm);
		if(!idata)
			idata = ENGINE_ICON;
		if(xpm)
			Mem_Free(xpm);

		data = idata[0];

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
	}

	SDL_WM_SetIcon(icon, NULL);
}
static void VID_SetIcon_Post(void)
{
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
// LordHavoc: info.info.x11.lock_func and accompanying code do not seem to compile with SDL 1.3
#if SDL_VIDEO_DRIVER_X11 && !SDL_VIDEO_DRIVER_QUARTZ
	int j;
	char *data;
	const SDL_version *version;

	version = SDL_Linked_Version();
	// only use non-XPM icon support in SDL v1.3 and higher
	// SDL v1.2 does not support "smooth" transparency, and thus is better
	// off the xpm way
	if(!(version->major >= 2 || (version->major == 1 && version->minor >= 3)))
	{
		// in this case, we did not set the good icon yet
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if(SDL_GetWMInfo(&info) == 1 && info.subsystem == SDL_SYSWM_X11)
		{
			data = (char *) loadimagepixelsbgra("darkplaces-icon", false, false, false, NULL);
			if(data)
			{
				// use _NET_WM_ICON too
				static long netwm_icon[MAX_NETWM_ICON];
				int pos = 0;
				int i = 1;

				while(data)
				{
					if(pos + 2 * image_width * image_height < MAX_NETWM_ICON)
					{
						netwm_icon[pos++] = image_width;
						netwm_icon[pos++] = image_height;
						for(i = 0; i < image_height; ++i)
							for(j = 0; j < image_width; ++j)
								netwm_icon[pos++] = BuffLittleLong((unsigned char *) &data[(i*image_width+j)*4]);
					}
					else
					{
						Con_Printf("Skipping NETWM icon #%d because there is no space left\n", i);
					}
					++i;
					Mem_Free(data);
					data = (char *) loadimagepixelsbgra(va("darkplaces-icon%d", i), false, false, false, NULL);
				}

				info.info.x11.lock_func();
				{
					Atom net_wm_icon = XInternAtom(info.info.x11.display, "_NET_WM_ICON", false);
					XChangeProperty(info.info.x11.display, info.info.x11.wmwindow, net_wm_icon, XA_CARDINAL, 32, PropModeReplace, (const unsigned char *) netwm_icon, pos);
				}
				info.info.x11.unlock_func();
			}
		}
	}
#endif
#endif
}


static void VID_SetCaption(void)
{
	SDL_WM_SetCaption( gamename, NULL );
}
#endif

static void VID_OutputVersion(void)
{
	const SDL_version *version;
	version = SDL_Linked_Version();
	Con_Printf(	"Linked against SDL version %d.%d.%d\n"
					"Using SDL library version %d.%d.%d\n",
					SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
					version->major, version->minor, version->patch );
}

qboolean VID_InitModeGL(viddef_mode_t *mode)
{
	int i;
// FIXME SDL_SetVideoMode
	static int notfirstvideomode = false;
	int flags = SDL_OPENGL;
	const char *drivername;

	win_half_width = mode->width>>1;
	win_half_height = mode->height>>1;

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

#ifndef __IPHONEOS__
	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
	{
		VID_Shutdown();
		Con_Print("Required OpenGL function glGetString not found\n");
		return false;
	}
#endif

	// Knghtbrd: should do platform-specific extension string function here

	vid_isfullscreen = false;
	if (mode->fullscreen) {
		flags |= SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}
	//flags |= SDL_HWSURFACE;

	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	if (mode->bitsperpixel >= 32)
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
	if (mode->stereobuffer)
		SDL_GL_SetAttribute (SDL_GL_STEREO, 1);
	if (mode->samples > 1)
	{
		SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, mode->samples);
	}
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
	if (vid_vsync.integer)
		SDL_GL_SetAttribute (SDL_GL_SWAP_CONTROL, 1);
	else
		SDL_GL_SetAttribute (SDL_GL_SWAP_CONTROL, 0);
#else
#ifdef __IPHONEOS__
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute (SDL_GL_RETAINED_BACKING, 1);
	// FIXME: get proper resolution from OS somehow (iPad for instance...)
	mode->width = 320;
	mode->height = 480;
#endif
#endif

	video_bpp = mode->bitsperpixel;
	video_flags = flags;
	VID_SetIcon_Pre();
	screen = SDL_SetVideoMode(mode->width, mode->height, mode->bitsperpixel, flags);
	VID_SetIcon_Post();

	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}

	mode->width = screen->w;
	mode->height = screen->h;
	vid_softsurface = NULL;
	vid.softpixels = NULL;

	// set window title
	VID_SetCaption();
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
	// set up an event filter to ask confirmation on close button in WIN32
	SDL_SetEventFilter( (SDL_EventFilter) Sys_EventFilter );
#endif
	// init keyboard
	SDL_EnableUNICODE( SDL_ENABLE );
	// enable key repeat since everyone expects it
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

#if !(SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2)
	SDL_GL_SetSwapInterval(vid_vsync.integer != 0);
	vid_usingvsync = (vid_vsync.integer != 0);
#endif

	gl_platform = "SDL";
	gl_platformextensions = "";

#ifdef __IPHONEOS__
	GLES_Init();
#else
	GL_Init();
#endif

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

extern cvar_t gl_info_extensions;
extern cvar_t gl_info_vendor;
extern cvar_t gl_info_renderer;
extern cvar_t gl_info_version;
extern cvar_t gl_info_platform;
extern cvar_t gl_info_driver;

qboolean VID_InitModeSoft(viddef_mode_t *mode)
{
// FIXME SDL_SetVideoMode
	int i;
	int flags = SDL_HWSURFACE;

	win_half_width = mode->width>>1;
	win_half_height = mode->height>>1;

	if(vid_resizable.integer)
		flags |= SDL_RESIZABLE;

	VID_OutputVersion();

	vid_isfullscreen = false;
	if (mode->fullscreen) {
		flags |= SDL_FULLSCREEN;
		vid_isfullscreen = true;
	}

	video_bpp = mode->bitsperpixel;
	video_flags = flags;
	VID_SetIcon_Pre();
	screen = SDL_SetVideoMode(mode->width, mode->height, mode->bitsperpixel, flags);
	VID_SetIcon_Post();

	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}

	// create a framebuffer using our specific color format, we let the SDL blit function convert it in VID_Finish
	vid_softsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, mode->width, mode->height, 32, 0x00FF0000, 0x0000FF00, 0x00000000FF, 0xFF000000);
	if (vid_softsurface == NULL)
	{
		Con_Printf("Failed to setup software rasterizer framebuffer %ix%ix32bpp: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}
	SDL_SetAlpha(vid_softsurface, 0, 255);

	vid.softpixels = (unsigned int *)vid_softsurface->pixels;
	vid.softdepthpixels = (unsigned int *)calloc(1, mode->width * mode->height * 4);
	if (DPSOFTRAST_Init(mode->width, mode->height, vid_soft_threads.integer, vid_soft_interlace.integer, (unsigned int *)vid_softsurface->pixels, (unsigned int *)vid.softdepthpixels) < 0)
	{
		Con_Printf("Failed to initialize software rasterizer\n");
		VID_Shutdown();
		return false;
	}

	// set window title
	VID_SetCaption();
	// set up an event filter to ask confirmation on close button in WIN32
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
	SDL_SetEventFilter( (SDL_EventFilter) Sys_EventFilter );
#endif
	// init keyboard
	SDL_EnableUNICODE( SDL_ENABLE );
	// enable key repeat since everyone expects it
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	gl_platform = "SDLSoft";
	gl_platformextensions = "";

	gl_renderer = "DarkPlaces-Soft";
	gl_vendor = "Forest Hale";
	gl_version = "0.0";
	gl_extensions = "";

	// clear the extension flags
	memset(&vid.support, 0, sizeof(vid.support));
	Cvar_SetQuick(&gl_info_extensions, "");

	vid.forcevbo = false;
	vid.support.arb_depth_texture = true;
	vid.support.arb_draw_buffers = true;
	vid.support.arb_occlusion_query = true;
	vid.support.arb_shadow = true;
	//vid.support.arb_texture_compression = true;
	vid.support.arb_texture_cube_map = true;
	vid.support.arb_texture_non_power_of_two = false;
	vid.support.arb_vertex_buffer_object = true;
	vid.support.ext_blend_subtract = true;
	vid.support.ext_draw_range_elements = true;
	vid.support.ext_framebuffer_object = true;
	vid.support.ext_texture_3d = true;
	//vid.support.ext_texture_compression_s3tc = true;
	vid.support.ext_texture_filter_anisotropic = true;
	vid.support.ati_separate_stencil = true;

	vid.maxtexturesize_2d = 16384;
	vid.maxtexturesize_3d = 512;
	vid.maxtexturesize_cubemap = 16384;
	vid.texunits = 4;
	vid.teximageunits = 32;
	vid.texarrayunits = 8;
	vid.max_anisotropy = 1;
	vid.maxdrawbuffers = 4;

	vid.texunits = bound(4, vid.texunits, MAX_TEXTUREUNITS);
	vid.teximageunits = bound(16, vid.teximageunits, MAX_TEXTUREUNITS);
	vid.texarrayunits = bound(8, vid.texarrayunits, MAX_TEXTUREUNITS);
	Con_DPrintf("Using DarkPlaces Software Rasterizer rendering path\n");
	vid.renderpath = RENDERPATH_SOFT;
	vid.useinterleavedarrays = false;

	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);

	// LordHavoc: report supported extensions
	Con_DPrintf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );

	// clear to black (loading plaque will be seen over this)
	GL_Clear(GL_COLOR_BUFFER_BIT, NULL, 1.0f, 128);

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

qboolean VID_InitMode(viddef_mode_t *mode)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Failed to init SDL video subsystem: %s", SDL_GetError());
#ifdef SSE2_PRESENT
	if (vid_soft.integer)
		return VID_InitModeSoft(mode);
	else
#endif
		return VID_InitModeGL(mode);
}

void VID_Shutdown (void)
{
	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

#ifndef WIN32
	if (icon)
		SDL_FreeSurface(icon);
	icon = NULL;
#endif

	if (vid_softsurface)
		SDL_FreeSurface(vid_softsurface);
	vid_softsurface = NULL;
	vid.softpixels = NULL;
	if (vid.softdepthpixels)
		free(vid.softdepthpixels);
	vid.softdepthpixels = NULL;

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

	if (!vid_hidden)
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			if (r_speeds.integer == 2 || gl_finish.integer)
			{
				qglFinish();CHECKGLERROR
			}
#if !(SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2)
{
	qboolean vid_usevsync;
	vid_usevsync = (vid_vsync.integer && !cls.timedemo);
	if (vid_usingvsync != vid_usevsync)
	{
		if (SDL_GL_SetSwapInterval(vid_usevsync != 0) >= 0)
			Con_DPrintf("Vsync %s\n", vid_usevsync ? "activated" : "deactivated");
		else
			Con_DPrintf("ERROR: can't %s vsync\n", vid_usevsync ? "activate" : "deactivate");
	}
}
#endif
			SDL_GL_SwapBuffers();
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_Finish();
			SDL_BlitSurface(vid_softsurface, NULL, screen, NULL);
			SDL_Flip(screen);
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
			break;
		}
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
