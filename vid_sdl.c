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

#ifdef USE_GLES2
# define SETVIDEOMODE 0
#else
# if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
#  define SETVIDEOMODE 1
# else
// LordHavoc: SDL 1.3's SDL_CreateWindow API is not finished enough to use yet, but you can set this to 0 if you want to try it...
#  ifndef SETVIDEOMODE
#   define SETVIDEOMODE 1
#  endif
# endif
#endif

static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_hasfocus = false;
static qboolean vid_isfullscreen;
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
#else
static qboolean vid_usingvsync = false;
#endif
static SDL_Joystick *vid_sdljoystick = NULL;

static int win_half_width = 50;
static int win_half_height = 50;
static int video_bpp;

#if SETVIDEOMODE
static SDL_Surface *screen;
static int video_flags;
#else
static SDL_GLContext *context;
static SDL_Window *window;
static int window_flags;
#endif
static SDL_Surface *vid_softsurface;

/////////////////////////
// Input handling
////
//TODO: Add error checking

#ifndef SDLK_PERCENT
#define SDLK_PERCENT '%'
#define SDLK_PRINTSCREEN SDLK_PRINT
#define SDLK_SCROLLLOCK SDLK_SCROLLOCK
#define SDLK_NUMLOCKCLEAR SDLK_NUMLOCK
#define SDLK_KP_1 SDLK_KP1
#define SDLK_KP_2 SDLK_KP2
#define SDLK_KP_3 SDLK_KP3
#define SDLK_KP_4 SDLK_KP4
#define SDLK_KP_5 SDLK_KP5
#define SDLK_KP_6 SDLK_KP6
#define SDLK_KP_7 SDLK_KP7
#define SDLK_KP_8 SDLK_KP8
#define SDLK_KP_9 SDLK_KP9
#define SDLK_KP_0 SDLK_KP0
#endif

static int MapKey( unsigned int sdlkey )
{
	switch(sdlkey)
	{
	default: return 0;
//	case SDLK_UNKNOWN:            return K_UNKNOWN;
	case SDLK_RETURN:             return K_ENTER;
	case SDLK_ESCAPE:             return K_ESCAPE;
	case SDLK_BACKSPACE:          return K_BACKSPACE;
	case SDLK_TAB:                return K_TAB;
	case SDLK_SPACE:              return K_SPACE;
	case SDLK_EXCLAIM:            return '!';
	case SDLK_QUOTEDBL:           return '"';
	case SDLK_HASH:               return '#';
	case SDLK_PERCENT:            return '%';
	case SDLK_DOLLAR:             return '$';
	case SDLK_AMPERSAND:          return '&';
	case SDLK_QUOTE:              return '\'';
	case SDLK_LEFTPAREN:          return '(';
	case SDLK_RIGHTPAREN:         return ')';
	case SDLK_ASTERISK:           return '*';
	case SDLK_PLUS:               return '+';
	case SDLK_COMMA:              return ',';
	case SDLK_MINUS:              return '-';
	case SDLK_PERIOD:             return '.';
	case SDLK_SLASH:              return '/';
	case SDLK_0:                  return '0';
	case SDLK_1:                  return '1';
	case SDLK_2:                  return '2';
	case SDLK_3:                  return '3';
	case SDLK_4:                  return '4';
	case SDLK_5:                  return '5';
	case SDLK_6:                  return '6';
	case SDLK_7:                  return '7';
	case SDLK_8:                  return '8';
	case SDLK_9:                  return '9';
	case SDLK_COLON:              return ':';
	case SDLK_SEMICOLON:          return ';';
	case SDLK_LESS:               return '<';
	case SDLK_EQUALS:             return '=';
	case SDLK_GREATER:            return '>';
	case SDLK_QUESTION:           return '?';
	case SDLK_AT:                 return '@';
	case SDLK_LEFTBRACKET:        return '[';
	case SDLK_BACKSLASH:          return '\\';
	case SDLK_RIGHTBRACKET:       return ']';
	case SDLK_CARET:              return '^';
	case SDLK_UNDERSCORE:         return '_';
	case SDLK_BACKQUOTE:          return '`';
	case SDLK_a:                  return 'a';
	case SDLK_b:                  return 'b';
	case SDLK_c:                  return 'c';
	case SDLK_d:                  return 'd';
	case SDLK_e:                  return 'e';
	case SDLK_f:                  return 'f';
	case SDLK_g:                  return 'g';
	case SDLK_h:                  return 'h';
	case SDLK_i:                  return 'i';
	case SDLK_j:                  return 'j';
	case SDLK_k:                  return 'k';
	case SDLK_l:                  return 'l';
	case SDLK_m:                  return 'm';
	case SDLK_n:                  return 'n';
	case SDLK_o:                  return 'o';
	case SDLK_p:                  return 'p';
	case SDLK_q:                  return 'q';
	case SDLK_r:                  return 'r';
	case SDLK_s:                  return 's';
	case SDLK_t:                  return 't';
	case SDLK_u:                  return 'u';
	case SDLK_v:                  return 'v';
	case SDLK_w:                  return 'w';
	case SDLK_x:                  return 'x';
	case SDLK_y:                  return 'y';
	case SDLK_z:                  return 'z';
	case SDLK_CAPSLOCK:           return K_CAPSLOCK;
	case SDLK_F1:                 return K_F1;
	case SDLK_F2:                 return K_F2;
	case SDLK_F3:                 return K_F3;
	case SDLK_F4:                 return K_F4;
	case SDLK_F5:                 return K_F5;
	case SDLK_F6:                 return K_F6;
	case SDLK_F7:                 return K_F7;
	case SDLK_F8:                 return K_F8;
	case SDLK_F9:                 return K_F9;
	case SDLK_F10:                return K_F10;
	case SDLK_F11:                return K_F11;
	case SDLK_F12:                return K_F12;
	case SDLK_PRINTSCREEN:        return K_PRINTSCREEN;
	case SDLK_SCROLLLOCK:         return K_SCROLLOCK;
	case SDLK_PAUSE:              return K_PAUSE;
	case SDLK_INSERT:             return K_INS;
	case SDLK_HOME:               return K_HOME;
	case SDLK_PAGEUP:             return K_PGUP;
#ifdef __IPHONEOS__
	case SDLK_DELETE:             return K_BACKSPACE;
#else
	case SDLK_DELETE:             return K_DEL;
#endif
	case SDLK_END:                return K_END;
	case SDLK_PAGEDOWN:           return K_PGDN;
	case SDLK_RIGHT:              return K_RIGHTARROW;
	case SDLK_LEFT:               return K_LEFTARROW;
	case SDLK_DOWN:               return K_DOWNARROW;
	case SDLK_UP:                 return K_UPARROW;
	case SDLK_NUMLOCKCLEAR:       return K_NUMLOCK;
	case SDLK_KP_DIVIDE:          return K_KP_DIVIDE;
	case SDLK_KP_MULTIPLY:        return K_KP_MULTIPLY;
	case SDLK_KP_MINUS:           return K_KP_MINUS;
	case SDLK_KP_PLUS:            return K_KP_PLUS;
	case SDLK_KP_ENTER:           return K_KP_ENTER;
	case SDLK_KP_1:               return K_KP_1;
	case SDLK_KP_2:               return K_KP_2;
	case SDLK_KP_3:               return K_KP_3;
	case SDLK_KP_4:               return K_KP_4;
	case SDLK_KP_5:               return K_KP_5;
	case SDLK_KP_6:               return K_KP_6;
	case SDLK_KP_7:               return K_KP_7;
	case SDLK_KP_8:               return K_KP_8;
	case SDLK_KP_9:               return K_KP_9;
	case SDLK_KP_0:               return K_KP_0;
	case SDLK_KP_PERIOD:          return K_KP_PERIOD;
//	case SDLK_APPLICATION:        return K_APPLICATION;
//	case SDLK_POWER:              return K_POWER;
	case SDLK_KP_EQUALS:          return K_KP_EQUALS;
//	case SDLK_F13:                return K_F13;
//	case SDLK_F14:                return K_F14;
//	case SDLK_F15:                return K_F15;
//	case SDLK_F16:                return K_F16;
//	case SDLK_F17:                return K_F17;
//	case SDLK_F18:                return K_F18;
//	case SDLK_F19:                return K_F19;
//	case SDLK_F20:                return K_F20;
//	case SDLK_F21:                return K_F21;
//	case SDLK_F22:                return K_F22;
//	case SDLK_F23:                return K_F23;
//	case SDLK_F24:                return K_F24;
//	case SDLK_EXECUTE:            return K_EXECUTE;
//	case SDLK_HELP:               return K_HELP;
//	case SDLK_MENU:               return K_MENU;
//	case SDLK_SELECT:             return K_SELECT;
//	case SDLK_STOP:               return K_STOP;
//	case SDLK_AGAIN:              return K_AGAIN;
//	case SDLK_UNDO:               return K_UNDO;
//	case SDLK_CUT:                return K_CUT;
//	case SDLK_COPY:               return K_COPY;
//	case SDLK_PASTE:              return K_PASTE;
//	case SDLK_FIND:               return K_FIND;
//	case SDLK_MUTE:               return K_MUTE;
//	case SDLK_VOLUMEUP:           return K_VOLUMEUP;
//	case SDLK_VOLUMEDOWN:         return K_VOLUMEDOWN;
//	case SDLK_KP_COMMA:           return K_KP_COMMA;
//	case SDLK_KP_EQUALSAS400:     return K_KP_EQUALSAS400;
//	case SDLK_ALTERASE:           return K_ALTERASE;
//	case SDLK_SYSREQ:             return K_SYSREQ;
//	case SDLK_CANCEL:             return K_CANCEL;
//	case SDLK_CLEAR:              return K_CLEAR;
//	case SDLK_PRIOR:              return K_PRIOR;
//	case SDLK_RETURN2:            return K_RETURN2;
//	case SDLK_SEPARATOR:          return K_SEPARATOR;
//	case SDLK_OUT:                return K_OUT;
//	case SDLK_OPER:               return K_OPER;
//	case SDLK_CLEARAGAIN:         return K_CLEARAGAIN;
//	case SDLK_CRSEL:              return K_CRSEL;
//	case SDLK_EXSEL:              return K_EXSEL;
//	case SDLK_KP_00:              return K_KP_00;
//	case SDLK_KP_000:             return K_KP_000;
//	case SDLK_THOUSANDSSEPARATOR: return K_THOUSANDSSEPARATOR;
//	case SDLK_DECIMALSEPARATOR:   return K_DECIMALSEPARATOR;
//	case SDLK_CURRENCYUNIT:       return K_CURRENCYUNIT;
//	case SDLK_CURRENCYSUBUNIT:    return K_CURRENCYSUBUNIT;
//	case SDLK_KP_LEFTPAREN:       return K_KP_LEFTPAREN;
//	case SDLK_KP_RIGHTPAREN:      return K_KP_RIGHTPAREN;
//	case SDLK_KP_LEFTBRACE:       return K_KP_LEFTBRACE;
//	case SDLK_KP_RIGHTBRACE:      return K_KP_RIGHTBRACE;
//	case SDLK_KP_TAB:             return K_KP_TAB;
//	case SDLK_KP_BACKSPACE:       return K_KP_BACKSPACE;
//	case SDLK_KP_A:               return K_KP_A;
//	case SDLK_KP_B:               return K_KP_B;
//	case SDLK_KP_C:               return K_KP_C;
//	case SDLK_KP_D:               return K_KP_D;
//	case SDLK_KP_E:               return K_KP_E;
//	case SDLK_KP_F:               return K_KP_F;
//	case SDLK_KP_XOR:             return K_KP_XOR;
//	case SDLK_KP_POWER:           return K_KP_POWER;
//	case SDLK_KP_PERCENT:         return K_KP_PERCENT;
//	case SDLK_KP_LESS:            return K_KP_LESS;
//	case SDLK_KP_GREATER:         return K_KP_GREATER;
//	case SDLK_KP_AMPERSAND:       return K_KP_AMPERSAND;
//	case SDLK_KP_DBLAMPERSAND:    return K_KP_DBLAMPERSAND;
//	case SDLK_KP_VERTICALBAR:     return K_KP_VERTICALBAR;
//	case SDLK_KP_DBLVERTICALBAR:  return K_KP_DBLVERTICALBAR;
//	case SDLK_KP_COLON:           return K_KP_COLON;
//	case SDLK_KP_HASH:            return K_KP_HASH;
//	case SDLK_KP_SPACE:           return K_KP_SPACE;
//	case SDLK_KP_AT:              return K_KP_AT;
//	case SDLK_KP_EXCLAM:          return K_KP_EXCLAM;
//	case SDLK_KP_MEMSTORE:        return K_KP_MEMSTORE;
//	case SDLK_KP_MEMRECALL:       return K_KP_MEMRECALL;
//	case SDLK_KP_MEMCLEAR:        return K_KP_MEMCLEAR;
//	case SDLK_KP_MEMADD:          return K_KP_MEMADD;
//	case SDLK_KP_MEMSUBTRACT:     return K_KP_MEMSUBTRACT;
//	case SDLK_KP_MEMMULTIPLY:     return K_KP_MEMMULTIPLY;
//	case SDLK_KP_MEMDIVIDE:       return K_KP_MEMDIVIDE;
//	case SDLK_KP_PLUSMINUS:       return K_KP_PLUSMINUS;
//	case SDLK_KP_CLEAR:           return K_KP_CLEAR;
//	case SDLK_KP_CLEARENTRY:      return K_KP_CLEARENTRY;
//	case SDLK_KP_BINARY:          return K_KP_BINARY;
//	case SDLK_KP_OCTAL:           return K_KP_OCTAL;
//	case SDLK_KP_DECIMAL:         return K_KP_DECIMAL;
//	case SDLK_KP_HEXADECIMAL:     return K_KP_HEXADECIMAL;
	case SDLK_LCTRL:              return K_CTRL;
	case SDLK_LSHIFT:             return K_SHIFT;
	case SDLK_LALT:               return K_ALT;
//	case SDLK_LGUI:               return K_LGUI;
	case SDLK_RCTRL:              return K_CTRL;
	case SDLK_RSHIFT:             return K_SHIFT;
	case SDLK_RALT:               return K_ALT;
//	case SDLK_RGUI:               return K_RGUI;
//	case SDLK_MODE:               return K_MODE;
//	case SDLK_AUDIONEXT:          return K_AUDIONEXT;
//	case SDLK_AUDIOPREV:          return K_AUDIOPREV;
//	case SDLK_AUDIOSTOP:          return K_AUDIOSTOP;
//	case SDLK_AUDIOPLAY:          return K_AUDIOPLAY;
//	case SDLK_AUDIOMUTE:          return K_AUDIOMUTE;
//	case SDLK_MEDIASELECT:        return K_MEDIASELECT;
//	case SDLK_WWW:                return K_WWW;
//	case SDLK_MAIL:               return K_MAIL;
//	case SDLK_CALCULATOR:         return K_CALCULATOR;
//	case SDLK_COMPUTER:           return K_COMPUTER;
//	case SDLK_AC_SEARCH:          return K_AC_SEARCH;
//	case SDLK_AC_HOME:            return K_AC_HOME;
//	case SDLK_AC_BACK:            return K_AC_BACK;
//	case SDLK_AC_FORWARD:         return K_AC_FORWARD;
//	case SDLK_AC_STOP:            return K_AC_STOP;
//	case SDLK_AC_REFRESH:         return K_AC_REFRESH;
//	case SDLK_AC_BOOKMARKS:       return K_AC_BOOKMARKS;
//	case SDLK_BRIGHTNESSDOWN:     return K_BRIGHTNESSDOWN;
//	case SDLK_BRIGHTNESSUP:       return K_BRIGHTNESSUP;
//	case SDLK_DISPLAYSWITCH:      return K_DISPLAYSWITCH;
//	case SDLK_KBDILLUMTOGGLE:     return K_KBDILLUMTOGGLE;
//	case SDLK_KBDILLUMDOWN:       return K_KBDILLUMDOWN;
//	case SDLK_KBDILLUMUP:         return K_KBDILLUMUP;
//	case SDLK_EJECT:              return K_EJECT;
//	case SDLK_SLEEP:              return K_SLEEP;
	}
}

#ifdef __IPHONEOS__
int SDL_iPhoneKeyboardShow(SDL_Window * window);  // reveals the onscreen keyboard.  Returns 0 on success and -1 on error.
int SDL_iPhoneKeyboardHide(SDL_Window * window);  // hides the onscreen keyboard.  Returns 0 on success and -1 on error.
SDL_bool SDL_iPhoneKeyboardIsShown(SDL_Window * window);  // returns whether or not the onscreen keyboard is currently visible.
int SDL_iPhoneKeyboardToggle(SDL_Window * window); // toggles the visibility of the onscreen keyboard.  Returns 0 on success and -1 on error.
#endif

void VID_ShowKeyboard(qboolean show)
{
#ifdef __IPHONEOS__
	if (show)
	{
		if (!SDL_iPhoneKeyboardIsShown(window))
			SDL_iPhoneKeyboardShow(window);
	}
	else
	{
		if (SDL_iPhoneKeyboardIsShown(window))
			SDL_iPhoneKeyboardHide(window);
	}
#endif
}

#ifdef __IPHONEOS__
qboolean VID_ShowingKeyboard(void)
{
	return SDL_iPhoneKeyboardIsShown(window);
}
#endif

void VID_SetMouse(qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
#ifndef __IPHONEOS__
#ifdef MACOSX
	if(relative)
		if(vid_usingmouse && (vid_usingnoaccel != !!apple_mouse_noaccel.integer))
			VID_SetMouse(false, false, false); // ungrab first!
#endif
	if (vid_usingmouse != relative)
	{
		vid_usingmouse = relative;
		cl_ignoremousemoves = 2;
#if SETVIDEOMODE
		SDL_WM_GrabInput( relative ? SDL_GRAB_ON : SDL_GRAB_OFF );
#else
		SDL_SetRelativeMouseMode(relative ? SDL_TRUE : SDL_FALSE);
#endif
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
	}
	if (vid_usinghidecursor != hidecursor)
	{
		vid_usinghidecursor = hidecursor;
		SDL_ShowCursor( hidecursor ? SDL_DISABLE : SDL_ENABLE);
	}
#endif
}

// multitouch[10][] represents the mouse pointer
// X and Y coordinates are 0-32767 as per SDL spec
#define MAXFINGERS 11
int multitouch[MAXFINGERS][3];

qboolean VID_TouchscreenArea(int corner, float px, float py, float pwidth, float pheight, const char *icon, float *resultmove, qboolean *resultbutton, keynum_t key)
{
	int finger;
	float fx, fy, fwidth, fheight;
	float rel[3];
	qboolean button = false;
	VectorClear(rel);
	if (pwidth > 0 && pheight > 0)
#ifdef __IPHONEOS__
	if (!VID_ShowingKeyboard())
#endif
	{
		if (corner & 1) px += vid_conwidth.value;
		if (corner & 2) py += vid_conheight.value;
		if (corner & 4) px += vid_conwidth.value * 0.5f;
		if (corner & 8) py += vid_conheight.value * 0.5f;
		if (corner & 16) {px *= vid_conwidth.value * (1.0f / 640.0f);py *= vid_conheight.value * (1.0f / 480.0f);pwidth *= vid_conwidth.value * (1.0f / 640.0f);pheight *= vid_conheight.value * (1.0f / 480.0f);}
		fx = px * 32768.0f / vid_conwidth.value;
		fy = py * 32768.0f / vid_conheight.value;
		fwidth = pwidth * 32768.0f / vid_conwidth.value;
		fheight = pheight * 32768.0f / vid_conheight.value;
		for (finger = 0;finger < MAXFINGERS;finger++)
		{
			if (multitouch[finger][0] && multitouch[finger][1] >= fx && multitouch[finger][2] >= fy && multitouch[finger][1] < fx + fwidth && multitouch[finger][2] < fy + fheight)
			{
				rel[0] = (multitouch[finger][1] - (fx + 0.5f * fwidth)) * (2.0f / fwidth);
				rel[1] = (multitouch[finger][2] - (fy + 0.5f * fheight)) * (2.0f / fheight);
				rel[2] = 0;
				button = true;
				break;
			}
		}
		if (scr_numtouchscreenareas < 16)
		{
			scr_touchscreenareas[scr_numtouchscreenareas].pic = icon;
			scr_touchscreenareas[scr_numtouchscreenareas].rect[0] = px;
			scr_touchscreenareas[scr_numtouchscreenareas].rect[1] = py;
			scr_touchscreenareas[scr_numtouchscreenareas].rect[2] = pwidth;
			scr_touchscreenareas[scr_numtouchscreenareas].rect[3] = pheight;
			scr_touchscreenareas[scr_numtouchscreenareas].active = button;
			scr_numtouchscreenareas++;
		}
	}
	if (resultmove)
	{
		if (button)
			VectorCopy(rel, resultmove);
		else
			VectorClear(resultmove);
	}
	if (resultbutton)
	{
		if (*resultbutton != button && (int)key > 0)
			Key_Event(key, 0, button);
		*resultbutton = button;
	}
	return button;
}

void VID_BuildJoyState(vid_joystate_t *joystate)
{
	VID_Shared_BuildJoyState_Begin(joystate);

	if (vid_sdljoystick)
	{
		SDL_Joystick *joy = vid_sdljoystick;
		int j;
		int numaxes;
		int numbuttons;
		numaxes = SDL_JoystickNumAxes(joy);
		for (j = 0;j < numaxes;j++)
			joystate->axis[j] = SDL_JoystickGetAxis(joy, j) * (1.0f / 32767.0f);
		numbuttons = SDL_JoystickNumButtons(joy);
		for (j = 0;j < numbuttons;j++)
			joystate->button[j] = SDL_JoystickGetButton(joy, j);
	}

	VID_Shared_BuildJoyState_Finish(joystate);
}

/////////////////////
// Movement handling
////

void IN_Move( void )
{
	static int old_x = 0, old_y = 0;
	static int stuck = 0;
	int x, y;
	vid_joystate_t joystate;

	scr_numtouchscreenareas = 0;
	if (vid_touchscreen.integer)
	{
		vec3_t move, aim, click;
		static qboolean buttons[16];
		static keydest_t oldkeydest;
		keydest_t keydest = (key_consoleactive & KEY_CONSOLEACTIVE_USER) ? key_console : key_dest;
		multitouch[MAXFINGERS-1][0] = SDL_GetMouseState(&x, &y);
		multitouch[MAXFINGERS-1][1] = x * 32768 / vid.width;
		multitouch[MAXFINGERS-1][2] = y * 32768 / vid.height;
		if (oldkeydest != keydest)
		{
			switch(keydest)
			{
			case key_game: VID_ShowKeyboard(false);break;
			case key_console: VID_ShowKeyboard(true);break;
			case key_message: VID_ShowKeyboard(true);break;
			default: break;
			}
		}
		oldkeydest = keydest;
		// top of screen is toggleconsole and K_ESCAPE
		switch(keydest)
		{
		case key_console:
#ifdef __IPHONEOS__
			VID_TouchscreenArea( 0,   0,   0,  64,  64, NULL                         , NULL, &buttons[13], (keynum_t)'`');
			VID_TouchscreenArea( 0,  64,   0,  64,  64, "gfx/touch_menu.tga"         , NULL, &buttons[14], K_ESCAPE);
			if (!VID_ShowingKeyboard())
			{
				// user entered a command, close the console now
				Con_ToggleConsole_f();
			}
#endif
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[15], (keynum_t)0);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , move, &buttons[0], K_MOUSE4);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , aim,  &buttons[1], K_MOUSE5);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , click,&buttons[2], K_MOUSE1);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[3], K_SPACE);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[4], K_MOUSE2);
			break;
		case key_game:
#ifdef __IPHONEOS__
			VID_TouchscreenArea( 0,   0,   0,  64,  64, NULL                         , NULL, &buttons[13], (keynum_t)'`');
			VID_TouchscreenArea( 0,  64,   0,  64,  64, "gfx/touch_menu.tga"         , NULL, &buttons[14], K_ESCAPE);
#endif
			VID_TouchscreenArea( 2,   0,-128, 128, 128, "gfx/touch_movebutton.tga"   , move, &buttons[0], K_MOUSE4);
			VID_TouchscreenArea( 3,-128,-128, 128, 128, "gfx/touch_aimbutton.tga"    , aim,  &buttons[1], K_MOUSE5);
			VID_TouchscreenArea( 2,   0,-160,  64,  32, "gfx/touch_jumpbutton.tga"   , NULL, &buttons[3], K_SPACE);
			VID_TouchscreenArea( 3,-128,-160,  64,  32, "gfx/touch_attackbutton.tga" , NULL, &buttons[2], K_MOUSE1);
			VID_TouchscreenArea( 3, -64,-160,  64,  32, "gfx/touch_attack2button.tga", NULL, &buttons[4], K_MOUSE2);
			buttons[15] = false;
			break;
		default:
#ifdef __IPHONEOS__
			VID_TouchscreenArea( 0,   0,   0,  64,  64, NULL                         , NULL, &buttons[13], (keynum_t)'`');
			VID_TouchscreenArea( 0,  64,   0,  64,  64, "gfx/touch_menu.tga"         , NULL, &buttons[14], K_ESCAPE);
			// in menus, an icon in the corner activates keyboard
			VID_TouchscreenArea( 2,   0, -32,  32,  32, "gfx/touch_keyboard.tga"     , NULL, &buttons[15], (keynum_t)0);
			if (buttons[15])
				VID_ShowKeyboard(true);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , move, &buttons[0], K_MOUSE4);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , aim,  &buttons[1], K_MOUSE5);
			VID_TouchscreenArea(16, -320,-480,640, 960, NULL                         , click,&buttons[2], K_MOUSE1);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[3], K_SPACE);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[4], K_MOUSE2);
			if (buttons[2])
			{
				in_windowmouse_x = x;
				in_windowmouse_y = y;
			}
#else
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[15], (keynum_t)0);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , move, &buttons[0], K_MOUSE4);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , aim,  &buttons[1], K_MOUSE5);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , click,&buttons[2], K_MOUSE1);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[3], K_SPACE);
			VID_TouchscreenArea( 0,   0,   0,   0,   0, NULL                         , NULL, &buttons[4], K_MOUSE2);
#endif
			break;
		}
		cl.cmd.forwardmove -= move[1] * cl_forwardspeed.value;
		cl.cmd.sidemove += move[0] * cl_sidespeed.value;
		cl.viewangles[0] += aim[1] * cl_pitchspeed.value * cl.realframetime;
		cl.viewangles[1] -= aim[0] * cl_yawspeed.value * cl.realframetime;
	}
	else
	{
		if (vid_usingmouse)
		{
			if (vid_stick_mouse.integer)
			{
				// have the mouse stuck in the middle, example use: prevent expose effect of beryl during the game when not using
				// window grabbing. --blub
	
				// we need 2 frames to initialize the center position
				if(!stuck)
				{
#if SETVIDEOMODE
					SDL_WarpMouse(win_half_width, win_half_height);
#else
					SDL_WarpMouseInWindow(window, win_half_width, win_half_height);
#endif
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
#if SETVIDEOMODE
					SDL_WarpMouse(win_half_width, win_half_height);
#else
					SDL_WarpMouseInWindow(window, win_half_width, win_half_height);
#endif
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
	}

	VID_BuildJoyState(&joystate);
	VID_ApplyJoyState(&joystate);
}

/////////////////////
// Message Handling
////

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

#ifndef __IPHONEOS__
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
#endif

#if SETVIDEOMODE
// SDL 1.2
void Sys_SendKeyEvents( void )
{
	static qboolean sound_active = true;
	int keycode;
	SDL_Event event;

	VID_EnableJoystick(true);

	while( SDL_PollEvent( &event ) )
		switch( event.type ) {
			case SDL_QUIT:
				Sys_Quit(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keycode = MapKey(event.key.keysym.sym);
				if (!VID_JoyBlockEmulatedKeys(keycode))
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
				if (!vid_touchscreen.integer)
				if (event.button.button <= 18)
					Key_Event( buttonremap[event.button.button - 1], 0, event.button.state == SDL_PRESSED );
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_JOYAXISMOTION:
			case SDL_JOYBALLMOTION:
			case SDL_JOYHATMOTION:
				break;
			case SDL_VIDEOEXPOSE:
				break;
			case SDL_VIDEORESIZE:
				if(vid_resizable.integer < 2)
				{
					vid.width = event.resize.w;
					vid.height = event.resize.h;
					screen = SDL_SetVideoMode(vid.width, vid.height, video_bpp, video_flags);
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
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
#else
			case SDL_TEXTEDITING:
				// unused when SETVIDEOMODE API is used
				break;
			case SDL_TEXTINPUT:
				// this occurs with SETVIDEOMODE but we are not using it
				break;
#endif
			case SDL_MOUSEMOTION:
				break;
			default:
				Con_DPrintf("Received unrecognized SDL_Event type 0x%x\n", event.type);
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

#else

// SDL 1.3
void Sys_SendKeyEvents( void )
{
	static qboolean sound_active = true;
	static qboolean missingunicodehack = true;
	int keycode;
	int i;
	int j;
	int unicode;
	SDL_Event event;

	VID_EnableJoystick(true);

	while( SDL_PollEvent( &event ) )
		switch( event.type ) {
			case SDL_QUIT:
				Sys_Quit(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keycode = MapKey(event.key.keysym.sym);
				if (!VID_JoyBlockEmulatedKeys(keycode))
					Key_Event(keycode, 0, (event.key.state == SDL_PRESSED));
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (!vid_touchscreen.integer)
				if (event.button.button <= 18)
					Key_Event( buttonremap[event.button.button - 1], 0, event.button.state == SDL_PRESSED );
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_JOYAXISMOTION:
			case SDL_JOYBALLMOTION:
			case SDL_JOYHATMOTION:
				break;
			case SDL_VIDEOEXPOSE:
				break;
			case SDL_WINDOWEVENT:
				//if (event.window.windowID == window) // how to compare?
				{
					switch(event.window.event)
					{
					case SDL_WINDOWEVENT_SHOWN:
						vid_hidden = false;
						break;
					case  SDL_WINDOWEVENT_HIDDEN:
						vid_hidden = true;
						break;
					case SDL_WINDOWEVENT_EXPOSED:
						break;
					case SDL_WINDOWEVENT_MOVED:
						break;
					case SDL_WINDOWEVENT_RESIZED:
						if(vid_resizable.integer < 2)
						{
							vid.width = event.window.data1;
							vid.height = event.window.data2;
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
					case SDL_WINDOWEVENT_MINIMIZED:
						break;
					case SDL_WINDOWEVENT_MAXIMIZED:
						break;
					case SDL_WINDOWEVENT_RESTORED:
						break;
					case SDL_WINDOWEVENT_ENTER:
						break;
					case SDL_WINDOWEVENT_LEAVE:
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						vid_hasfocus = true;
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						vid_hasfocus = false;
						break;
					case SDL_WINDOWEVENT_CLOSE:
						Sys_Quit(0);
						break;
					}
				}
				break;
			case SDL_TEXTEDITING:
				// FIXME!  this is where composition gets supported
				break;
			case SDL_TEXTINPUT:
				// we have some characters to parse
				missingunicodehack = false;
				{
					unicode = 0;
					for (i = 0;event.text.text[i];)
					{
						unicode = event.text.text[i++];
						if (unicode & 0x80)
						{
							// UTF-8 character
							// strip high bits (we could count these to validate character length but we don't)
							for (j = 0x80;unicode & j;j >>= 1)
								unicode ^= j;
							for (;(event.text.text[i] & 0xC0) == 0x80;i++)
								unicode = (unicode << 6) | (event.text.text[i] & 0x3F);
							// low characters are invalid and could be bad, so replace them
							if (unicode < 0x80)
								unicode = '?'; // we could use 0xFFFD instead, the unicode substitute character
						}
						//Con_DPrintf("SDL_TEXTINPUT: K_TEXT %i \n", unicode);
						Key_Event(K_TEXT, unicode, true);
						Key_Event(K_TEXT, unicode, false);
					}
				}
				break;
			case SDL_MOUSEMOTION:
				break;
			case SDL_FINGERDOWN:
				Con_DPrintf("SDL_FINGERDOWN for finger %i\n", (int)event.tfinger.fingerId);
				for (i = 0;i < MAXFINGERS-1;i++)
				{
					if (!multitouch[i][0])
					{
						multitouch[i][0] = event.tfinger.fingerId;
						multitouch[i][1] = event.tfinger.x;
						multitouch[i][2] = event.tfinger.y;
						// TODO: use event.tfinger.pressure?
						break;
					}
				}
				if (i == MAXFINGERS-1)
					Con_DPrintf("Too many fingers at once!\n");
				break;
			case SDL_FINGERUP:
				Con_DPrintf("SDL_FINGERUP for finger %i\n", (int)event.tfinger.fingerId);
				for (i = 0;i < MAXFINGERS-1;i++)
				{
					if (multitouch[i][0] == event.tfinger.fingerId)
					{
						multitouch[i][0] = 0;
						break;
					}
				}
				if (i == MAXFINGERS-1)
					Con_DPrintf("No SDL_FINGERDOWN event matches this SDL_FINGERMOTION event\n");
				break;
			case SDL_FINGERMOTION:
				Con_DPrintf("SDL_FINGERMOTION for finger %i\n", (int)event.tfinger.fingerId);
				for (i = 0;i < MAXFINGERS-1;i++)
				{
					if (multitouch[i][0] == event.tfinger.fingerId)
					{
						multitouch[i][1] = event.tfinger.x;
						multitouch[i][2] = event.tfinger.y;
						break;
					}
				}
				if (i == MAXFINGERS-1)
					Con_DPrintf("No SDL_FINGERDOWN event matches this SDL_FINGERMOTION event\n");
				break;
			case SDL_TOUCHBUTTONDOWN:
				// not sure what to do with this...
				break;
			case SDL_TOUCHBUTTONUP:
				// not sure what to do with this...
				break;
			default:
				Con_DPrintf("Received unrecognized SDL_Event type 0x%x\n", event.type);
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
#endif

/////////////////
// Video system
////

#ifdef USE_GLES2
#ifdef __IPHONEOS__
#include <OpenGLES/ES2/gl.h>
#else
#include <SDL_opengles.h>
#endif

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
//void wrapglBegin(GLenum mode) {Con_Printf("glBegin(mode)\n");}
//void wrapglBeginQuery(GLenum target, GLuint qid) {glBeginQuery(target, qid);}
void wrapglBindAttribLocation(GLuint programObj, GLuint index, const GLchar *name) {glBindAttribLocation(programObj, index, name);}
void wrapglBindFragDataLocation(GLuint programObj, GLuint index, const GLchar *name) {glBindFragDataLocation(programObj, index, name);}
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
//void wrapglEnd(void) {Con_Printf("glEnd()\n");}
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
//void wrapglPolygonMode(GLenum face, GLenum mode) {Con_Printf("glPolygonMode(face, mode)\n");}
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
//	qglBegin = wrapglBegin;
//	qglBeginQueryARB = wrapglBeginQuery;
	qglBindAttribLocation = wrapglBindAttribLocation;
	qglBindFragDataLocation = wrapglBindFragDataLocation;
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
//	qglEnd = wrapglEnd;
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
//	qglPolygonMode = wrapglPolygonMode;
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

	// GLES devices in general do not like GL_BGRA, so use GL_RGBA
	vid.forcetextype = TEXTYPE_RGBA;
	
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
	vid.support.ext_texture_srgb = false;

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
	vid.sRGBcapable2D = false;
	vid.sRGBcapable3D = false;

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

static qboolean vid_sdl_initjoysticksystem = false;

void VID_Init (void)
{
#ifndef __IPHONEOS__
#ifdef MACOSX
	Cvar_RegisterVariable(&apple_mouse_noaccel);
#endif
#endif
#ifdef __IPHONEOS__
	Cvar_SetValueQuick(&vid_touchscreen, 1);
#endif

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

static int vid_sdljoystickindex = -1;
void VID_EnableJoystick(qboolean enable)
{
	int index = joy_enable.integer > 0 ? joy_index.integer : -1;
	int numsdljoysticks;
	qboolean success = false;
	int sharedcount = 0;
	int sdlindex = -1;
	sharedcount = VID_Shared_SetJoystick(index);
	if (index >= 0 && index < sharedcount)
		success = true;
	sdlindex = index - sharedcount;

	numsdljoysticks = SDL_NumJoysticks();
	if (sdlindex < 0 || sdlindex >= numsdljoysticks)
		sdlindex = -1;

	// update cvar containing count of XInput joysticks + SDL joysticks
	if (joy_detected.integer != sharedcount + numsdljoysticks)
		Cvar_SetValueQuick(&joy_detected, sharedcount + numsdljoysticks);

	if (vid_sdljoystickindex != sdlindex)
	{
		vid_sdljoystickindex = sdlindex;
		// close SDL joystick if active
		if (vid_sdljoystick)
			SDL_JoystickClose(vid_sdljoystick);
		vid_sdljoystick = NULL;
		if (sdlindex >= 0)
		{
			vid_sdljoystick = SDL_JoystickOpen(sdlindex);
			if (vid_sdljoystick)
				Con_Printf("Joystick %i opened (SDL_Joystick %i is \"%s\" with %i axes, %i buttons, %i balls)\n", index, sdlindex, SDL_JoystickName(sdlindex), (int)SDL_JoystickNumAxes(vid_sdljoystick), (int)SDL_JoystickNumButtons(vid_sdljoystick), (int)SDL_JoystickNumBalls(vid_sdljoystick));
			else
			{
				Con_Printf("Joystick %i failed (SDL_JoystickOpen(%i) returned: %s)\n", index, sdlindex, SDL_GetError());
				sdlindex = -1;
			}
		}
	}

	if (sdlindex >= 0)
		success = true;

	if (joy_active.integer != (success ? 1 : 0))
		Cvar_SetValueQuick(&joy_active, success ? 1 : 0);
}

#if SETVIDEOMODE
// set the icon (we dont use SDL here since it would be too much a PITA)
#ifdef WIN32
#include "resource.h"
#include <SDL_syswm.h>
static SDL_Surface *VID_WrapSDL_SetVideoMode(int screenwidth, int screenheight, int screenbpp, int screenflags)
{
	SDL_Surface *screen = NULL;
	SDL_SysWMinfo info;
	HICON icon;
	SDL_WM_SetCaption( gamename, NULL );
	screen = SDL_SetVideoMode(screenwidth, screenheight, screenbpp, screenflags);
	if (screen)
	{
		// get the HWND handle
		SDL_VERSION( &info.version );
		if (SDL_GetWMInfo(&info))
		{
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
	}
	return screen;
}
#elif defined(MACOSX)
static SDL_Surface *VID_WrapSDL_SetVideoMode(int screenwidth, int screenheight, int screenbpp, int screenflags)
{
	SDL_Surface *screen = NULL;
	SDL_WM_SetCaption( gamename, NULL );
	screen = SDL_SetVideoMode(screenwidth, screenheight, screenbpp, screenflags);
	// we don't use SDL_WM_SetIcon here because the icon in the .app should be used
	return screen;
}
#else
// Adding the OS independent XPM version --blub
#include "darkplaces.xpm"
#include "nexuiz.xpm"
static SDL_Surface *icon = NULL;
static SDL_Surface *VID_WrapSDL_SetVideoMode(int screenwidth, int screenheight, int screenbpp, int screenflags)
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
	SDL_Surface *screen = NULL;

	if (icon)
		SDL_FreeSurface(icon);
	icon = NULL;
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

			if (icon)
				icon->pixels = data;
			else
			{
				Con_Printf(	"Failed to create surface for the window Icon!\n"
						"%s\n", SDL_GetError());
				free(data);
			}
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

		if(sscanf(data, "%i %i %i %i", &width, &height, &colors, &isize) == 4)
		{
			if(isize == 1)
			{
				for(i = 0; i < colors; ++i)
				{
					unsigned int r, g, b;
					char idx;

					if(sscanf(idata[i+1], "%c c #%02x%02x%02x", &idx, &r, &g, &b) != 4)
					{
						char foo[2];
						if(sscanf(idata[i+1], "%c c Non%1[e]", &idx, foo) != 2) // I take the DailyWTF credit for this. --div0
							break;
						else
						{
							palette[i].r = 255; // color key
							palette[i].g = 0;
							palette[i].b = 255;
							thenone = i; // weeeee
							palenc[(unsigned char) idx] = i;
						}
					}
					else
					{
						palette[i].r = r - (r == 255 && g == 0 && b == 255); // change 255/0/255 pink to 254/0/255 for color key
						palette[i].g = g;
						palette[i].b = b;
						palenc[(unsigned char) idx] = i;
					}
				}

				if (i == colors)
				{
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

					if(icon)
					{
						icon->pixels = data;
						SDL_SetPalette(icon, SDL_PHYSPAL|SDL_LOGPAL, palette, 0, colors);
						SDL_SetColorKey(icon, SDL_SRCCOLORKEY, thenone);
					}
					else
					{
						Con_Printf(	"Failed to create surface for the window Icon!\n"
								"%s\n", SDL_GetError());
						free(data);
					}
				}
				else
				{
					Con_Printf("This XPM's palette looks odd. Can't continue.\n");
				}
			}
			else
			{
				// NOTE: Only 1-char colornames are supported
				Con_Printf("This XPM's palette is either huge or idiotically unoptimized. It's key size is %i\n", isize);
			}
		}
		else
		{
			// NOTE: Only 1-char colornames are supported
			Con_Printf("Sorry, but this does not even look similar to an XPM.\n");
		}
	}

	if (icon)
		SDL_WM_SetIcon(icon, NULL);

	SDL_WM_SetCaption( gamename, NULL );
	screen = SDL_SetVideoMode(screenwidth, screenheight, screenbpp, screenflags);

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
// LordHavoc: info.info.x11.lock_func and accompanying code do not seem to compile with SDL 1.3
#if SDL_VIDEO_DRIVER_X11 && !SDL_VIDEO_DRIVER_QUARTZ

	version = SDL_Linked_Version();
	// only use non-XPM icon support in SDL v1.3 and higher
	// SDL v1.2 does not support "smooth" transparency, and thus is better
	// off the xpm way
	if(screen && (!(version->major >= 2 || (version->major == 1 && version->minor >= 3))))
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
	return screen;
}

#endif
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
#if SETVIDEOMODE
	static int notfirstvideomode = false;
	int flags = SDL_OPENGL;
#else
	int windowflags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
#endif
	const char *drivername;

	win_half_width = mode->width>>1;
	win_half_height = mode->height>>1;

	if(vid_resizable.integer)
#if SETVIDEOMODE
		flags |= SDL_RESIZABLE;
#else
		windowflags |= SDL_WINDOW_RESIZABLE;
#endif

	VID_OutputVersion();

#if SETVIDEOMODE
	/*
	SDL 1.2 Hack
		We cant switch from one OpenGL video mode to another.
		Thus we first switch to some stupid 2D mode and then back to OpenGL.
	*/
	if (notfirstvideomode)
		SDL_SetVideoMode( 0, 0, 0, 0 );
	notfirstvideomode = true;
#endif

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

#ifdef __IPHONEOS__
	// mobile platforms are always fullscreen, we'll get the resolution after opening the window
	mode->fullscreen = true;
	// hide the menu with SDL_WINDOW_BORDERLESS
	windowflags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
#endif
#ifndef USE_GLES2
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
#if SETVIDEOMODE
		flags |= SDL_FULLSCREEN;
#else
		windowflags |= SDL_WINDOW_FULLSCREEN;
#endif
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
#ifdef USE_GLES2
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute (SDL_GL_RETAINED_BACKING, 1);
#endif
#endif

	video_bpp = mode->bitsperpixel;
#if SETVIDEOMODE
	video_flags = flags;
	screen = VID_WrapSDL_SetVideoMode(mode->width, mode->height, mode->bitsperpixel, flags);
	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}
	mode->width = screen->w;
	mode->height = screen->h;
#else
	window_flags = windowflags;
	window = SDL_CreateWindow(gamename, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode->width, mode->height, windowflags);
	if (window == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}
	SDL_GetWindowSize(window, &mode->width, &mode->height);
	context = SDL_GL_CreateContext(window);
	if (context == NULL)
	{
		Con_Printf("Failed to initialize OpenGL context: %s\n", SDL_GetError());
		VID_Shutdown();
		return false;
	}
#endif

	vid_softsurface = NULL;
	vid.softpixels = NULL;

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

#ifdef USE_GLES2
	GLES_Init();
#else
	GL_Init();
#endif

	vid_hidden = false;
	vid_activewindow = false;
	vid_hasfocus = true;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
		
#if SETVIDEOMODE
	SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
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
#if SETVIDEOMODE
	int flags = SDL_HWSURFACE;
	if(!COM_CheckParm("-noasyncblit")) flags |= SDL_ASYNCBLIT;
#else
	int windowflags = SDL_WINDOW_SHOWN;
#endif

	win_half_width = mode->width>>1;
	win_half_height = mode->height>>1;

	if(vid_resizable.integer)
#if SETVIDEOMODE
		flags |= SDL_RESIZABLE;
#else
		windowflags |= SDL_WINDOW_RESIZABLE;
#endif

	VID_OutputVersion();

	vid_isfullscreen = false;
	if (mode->fullscreen) {
#if SETVIDEOMODE
		flags |= SDL_FULLSCREEN;
#else
		windowflags |= SDL_WINDOW_FULLSCREEN;
#endif
		vid_isfullscreen = true;
	}

	video_bpp = mode->bitsperpixel;
#if SETVIDEOMODE
	video_flags = flags;
	screen = VID_WrapSDL_SetVideoMode(mode->width, mode->height, mode->bitsperpixel, flags);
	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}
	mode->width = screen->w;
	mode->height = screen->h;
#else
	window_flags = windowflags;
	window = SDL_CreateWindow(gamename, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode->width, mode->height, windowflags);
	if (window == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", mode->width, mode->height, SDL_GetError());
		VID_Shutdown();
		return false;
	}
	SDL_GetWindowSize(window, &mode->width, &mode->height);
#endif

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

	// init keyboard
	SDL_EnableUNICODE( SDL_ENABLE );
	// enable key repeat since everyone expects it
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	VID_Soft_SharedSetup();

	vid_hidden = false;
	vid_activewindow = false;
	vid_hasfocus = true;
	vid_usingmouse = false;
	vid_usinghidecursor = false;

#if SETVIDEOMODE
	SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
	return true;
}

qboolean VID_InitMode(viddef_mode_t *mode)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Failed to init SDL video subsystem: %s", SDL_GetError());
#ifdef SSE_POSSIBLE
	if (vid_soft.integer)
		return VID_InitModeSoft(mode);
	else
#endif
		return VID_InitModeGL(mode);
}

void VID_Shutdown (void)
{
	VID_EnableJoystick(false);
	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

#if SETVIDEOMODE
#ifndef WIN32
#ifndef MACOSX
	if (icon)
		SDL_FreeSurface(icon);
	icon = NULL;
#endif
#endif
#endif

	if (vid_softsurface)
		SDL_FreeSurface(vid_softsurface);
	vid_softsurface = NULL;
	vid.softpixels = NULL;
	if (vid.softdepthpixels)
		free(vid.softdepthpixels);
	vid.softdepthpixels = NULL;

#if SETVIDEOMODE
#else
	SDL_DestroyWindow(window);
	window = NULL;
#endif

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
#if SETVIDEOMODE
	Uint8 appstate;

	//react on appstate changes
	appstate = SDL_GetAppState();

	vid_hidden = !(appstate & SDL_APPACTIVE);
	vid_hasfocus = (appstate & SDL_APPINPUTFOCUS) != 0;
#endif
	vid_activewindow = !vid_hidden && vid_hasfocus;

	VID_UpdateGamma(false, 256);

	if (!vid_hidden)
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			CHECKGLERROR
			if (r_speeds.integer == 2 || gl_finish.integer)
				GL_Finish();
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
#else
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
#if SETVIDEOMODE
			SDL_GL_SwapBuffers();
#else
			SDL_GL_SwapWindow(window);
#endif
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_Finish();
#if SETVIDEOMODE
//		if (!r_test.integer)
		{
			SDL_BlitSurface(vid_softsurface, NULL, screen, NULL);
			SDL_Flip(screen);
		}
#else
			{
				SDL_Surface *screen = SDL_GetWindowSurface(window);
				SDL_BlitSurface(vid_softsurface, NULL, screen, NULL);
				SDL_UpdateWindowSurface(window);
			}
#endif
			break;
		case RENDERPATH_D3D9:
		case RENDERPATH_D3D10:
		case RENDERPATH_D3D11:
			if (r_speeds.integer == 2 || gl_finish.integer)
				GL_Finish();
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
	for(vidmodes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE); vidmodes && *vidmodes; ++vidmodes)
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
