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
#include <SDL.h>
#include <stdio.h>

#include "quakedef.h"

// Tell startup code that we have a client
int cl_available = true;
static qboolean vid_usingmouse;
static qboolean vid_isfullscreen;

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
	0,			//SDLK_MODE		= 313,		/* "Alt Gr" key */
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

static void IN_Activate( qboolean grab )
{
	if (grab)
	{
		if (!vid_usingmouse)
		{
			vid_usingmouse = true;
			cl_ignoremousemove = true;
			SDL_WM_GrabInput( SDL_GRAB_ON );
			SDL_ShowCursor( SDL_DISABLE );
		}
	}
	else
	{
		if (vid_usingmouse)
		{
			vid_usingmouse = false;
			cl_ignoremousemove = true;
			SDL_WM_GrabInput( SDL_GRAB_OFF );
			SDL_ShowCursor( SDL_ENABLE );
		}
	}
}

void IN_Move( void )
{
	if( vid_usingmouse )
	{
		int x, y;
		SDL_GetRelativeMouseState( &x, &y );
		in_mouse_x = x;
		in_mouse_y = y;
	}
}

/////////////////////
// Message Handling
////

static int Sys_EventFilter( SDL_Event *event )
{
	//TODO: Add a quit query in linux, too - though linux user are more likely to know what they do
#ifdef WIN32
	if( event->type == SDL_QUIT && MessageBox( NULL, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION ) == IDNO )
		return 0;
	else
		return 1;
#else
	return 1;
#endif
}

void Sys_SendKeyEvents( void )
{
	SDL_Event event;

	while( SDL_PollEvent( &event ) )
		switch( event.type ) {
			case SDL_QUIT:
				Sys_Quit();
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				Key_Event( MapKey( event.key.keysym.sym ), (char)event.key.keysym.unicode, (event.key.state == SDL_PRESSED) );
				break;
			case SDL_ACTIVEEVENT:
				if( event.active.state == SDL_APPACTIVE )
				{
					if( event.active.gain )
						vid_hidden = false;
					else
						vid_hidden = true;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if( event.button.button == SDL_BUTTON_MIDDLE )
					event.button.button = SDL_BUTTON_RIGHT;
				else if( event.button.button == SDL_BUTTON_RIGHT )
					event.button.button = SDL_BUTTON_MIDDLE;
				Key_Event( K_MOUSE1 + event.button.button - 1, 0, true );
				break;
			case SDL_MOUSEBUTTONUP:
				if( event.button.button == SDL_BUTTON_MIDDLE )
					event.button.button = SDL_BUTTON_RIGHT;
				else if( event.button.button == SDL_BUTTON_RIGHT )
					event.button.button = SDL_BUTTON_MIDDLE;
				Key_Event( K_MOUSE1 + event.button.button - 1, 0, false );
				break;
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
void VID_Init (void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Failed to init video: %s", SDL_GetError());
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
	SetClassLong( info.window, GCL_HICON, (LONG) icon );
}
#else
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

int VID_InitMode(int fullscreen, int width, int height, int bpp)
{
	int i;
	int flags = SDL_OPENGL;
	const char *drivername;

	VID_OutputVersion();

	/*
	SDL Hack
		We cant switch from one OpenGL video mode to another.
		Thus we first switch to some stupid 2D mode and then back to OpenGL.
	*/
	SDL_SetVideoMode( 0, 0, 0, 0 );

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
		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 1);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 1);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 1);
		SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 16);
	}

	screen = SDL_SetVideoMode(width, height, bpp, flags);
	if (screen == NULL)
	{
		Con_Printf("Failed to set video mode to %ix%i: %s\n", width, height, SDL_GetError);
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

	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	gl_platform = "SDL";
	// Knghtbrd: should assign platform-specific extensions here
	//TODO: maybe ;)
	gl_platformextensions = "";
	gl_videosyncavailable = false;

	GL_Init();

	vid_hidden = false;
	vid_activewindow = false;
	vid_usingmouse = false;
	return true;
}

void VID_Shutdown (void)
{
	IN_Activate(false);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int VID_SetGamma (unsigned short *ramps)
{
	return !SDL_SetGammaRamp (ramps, ramps + 256, ramps + 512);
}

int VID_GetGamma (unsigned short *ramps)
{
	return !SDL_GetGammaRamp( ramps, ramps + 256, ramps + 512);
}

void VID_Finish (void)
{
	Uint8 appstate;
	qboolean vid_usemouse;

	if (r_speeds.integer || gl_finish.integer)
		qglFinish();
	SDL_GL_SwapBuffers();

	//react on appstate changes
	appstate = SDL_GetAppState();

	if( !( appstate & SDL_APPMOUSEFOCUS ) || !( appstate & SDL_APPINPUTFOCUS ) )
		vid_activewindow = false;
	else
		vid_activewindow = true;

	vid_usemouse = false;
	if( vid_mouse.integer && !key_consoleactive && !cls.demoplayback )
		vid_usemouse = true;
	if( vid_isfullscreen )
		vid_usemouse = true;
	if( !vid_activewindow )
		vid_usemouse = false;

	IN_Activate(vid_usemouse);
}
