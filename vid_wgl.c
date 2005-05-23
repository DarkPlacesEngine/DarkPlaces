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
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include <windows.h>
#include <dsound.h>
#include "resource.h"
#include <commctrl.h>

extern void S_BlockSound (void);
extern void S_UnblockSound (void);
extern HINSTANCE global_hInstance;


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif

// Tell startup code that we have a client
int cl_available = true;

static int (WINAPI *qwglChoosePixelFormat)(HDC, CONST PIXELFORMATDESCRIPTOR *);
static int (WINAPI *qwglDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
//static int (WINAPI *qwglGetPixelFormat)(HDC);
static BOOL (WINAPI *qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
static BOOL (WINAPI *qwglSwapBuffers)(HDC);
static HGLRC (WINAPI *qwglCreateContext)(HDC);
static BOOL (WINAPI *qwglDeleteContext)(HGLRC);
static HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
static HDC (WINAPI *qwglGetCurrentDC)(VOID);
static PROC (WINAPI *qwglGetProcAddress)(LPCSTR);
static BOOL (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
static BOOL (WINAPI *qwglSwapIntervalEXT)(int interval);
static const char *(WINAPI *qwglGetExtensionsStringARB)(HDC hdc);

static dllfunction_t wglfuncs[] =
{
	{"wglChoosePixelFormat", (void **) &qwglChoosePixelFormat},
	{"wglDescribePixelFormat", (void **) &qwglDescribePixelFormat},
//	{"wglGetPixelFormat", (void **) &qwglGetPixelFormat},
	{"wglSetPixelFormat", (void **) &qwglSetPixelFormat},
	{"wglSwapBuffers", (void **) &qwglSwapBuffers},
	{"wglCreateContext", (void **) &qwglCreateContext},
	{"wglDeleteContext", (void **) &qwglDeleteContext},
	{"wglGetProcAddress", (void **) &qwglGetProcAddress},
	{"wglMakeCurrent", (void **) &qwglMakeCurrent},
	{"wglGetCurrentContext", (void **) &qwglGetCurrentContext},
	{"wglGetCurrentDC", (void **) &qwglGetCurrentDC},
	{NULL, NULL}
};

static dllfunction_t wglswapintervalfuncs[] =
{
	{"wglSwapIntervalEXT", (void **) &qwglSwapIntervalEXT},
	{NULL, NULL}
};

static DEVMODE gdevmode;
static qboolean vid_initialized = false;
static qboolean vid_wassuspended = false;
static qboolean vid_usingmouse = false;
static qboolean vid_usingvsync = false;
static qboolean vid_usevsync = false;
static HICON hIcon;

// used by cd_win.c and snd_win.c
HWND mainwindow;

static HDC	 baseDC;
static HGLRC baseRC;

//HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

static qboolean vid_isfullscreen;

//void VID_MenuDraw (void);
//void VID_MenuKey (int key);

//LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
//void AppActivate(BOOL fActive, BOOL minimize);
//void ClearAllStates (void);
//void VID_UpdateWindowStatus (void);

//====================================

static int window_x, window_y;

static void IN_Activate (qboolean grab);

static qboolean mouseinitialized;
static qboolean dinput;

// input code

#include <dinput.h>

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

static HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);

// LordHavoc: thanks to backslash for this support for mouse buttons 4 and 5
/* backslash :: imouse explorer buttons */
/* These are #ifdefed out for non-Win2K in the February 2001 version of
   MS's platform SDK, but we need them for compilation. . . */
#ifndef WM_XBUTTONDOWN
   #define WM_XBUTTONDOWN      0x020B
   #define WM_XBUTTONUP      0x020C
#endif
#ifndef MK_XBUTTON1
   #define MK_XBUTTON1         0x0020
   #define MK_XBUTTON2         0x0040
// LordHavoc: lets hope this allows more buttons in the future...
   #define MK_XBUTTON3         0x0080
   #define MK_XBUTTON4         0x0100
   #define MK_XBUTTON5         0x0200
   #define MK_XBUTTON6         0x0400
   #define MK_XBUTTON7         0x0800
#endif
/* :: backslash */

// mouse variables
static int			mouse_buttons;
static int			mouse_oldbuttonstate;

static qboolean	restore_spi;
static int		originalmouseparms[3], newmouseparms[3] = {0, 0, 1};

static unsigned int uiWheelMessage;
static qboolean	mouseparmsvalid;
static qboolean	dinput_acquired;

static unsigned int		mstate_di;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn
};

static DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

static DWORD	dwAxisMap[JOY_MAX_AXES];
static DWORD	dwControlMap[JOY_MAX_AXES];
static PDWORD	pdwRawValue[JOY_MAX_AXES];

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
static cvar_t	in_joystick = {CVAR_SAVE, "joystick","0"};
static cvar_t	joy_name = {0, "joyname", "joystick"};
static cvar_t	joy_advanced = {0, "joyadvanced", "0"};
static cvar_t	joy_advaxisx = {0, "joyadvaxisx", "0"};
static cvar_t	joy_advaxisy = {0, "joyadvaxisy", "0"};
static cvar_t	joy_advaxisz = {0, "joyadvaxisz", "0"};
static cvar_t	joy_advaxisr = {0, "joyadvaxisr", "0"};
static cvar_t	joy_advaxisu = {0, "joyadvaxisu", "0"};
static cvar_t	joy_advaxisv = {0, "joyadvaxisv", "0"};
static cvar_t	joy_forwardthreshold = {0, "joyforwardthreshold", "0.15"};
static cvar_t	joy_sidethreshold = {0, "joysidethreshold", "0.15"};
static cvar_t	joy_pitchthreshold = {0, "joypitchthreshold", "0.15"};
static cvar_t	joy_yawthreshold = {0, "joyyawthreshold", "0.15"};
static cvar_t	joy_forwardsensitivity = {0, "joyforwardsensitivity", "-1.0"};
static cvar_t	joy_sidesensitivity = {0, "joysidesensitivity", "-1.0"};
static cvar_t	joy_pitchsensitivity = {0, "joypitchsensitivity", "1.0"};
static cvar_t	joy_yawsensitivity = {0, "joyyawsensitivity", "-1.0"};
static cvar_t	joy_wwhack1 = {0, "joywwhack1", "0.0"};
static cvar_t	joy_wwhack2 = {0, "joywwhack2", "0.0"};

static qboolean	joy_avail, joy_advancedinit, joy_haspov;
static DWORD		joy_oldbuttonstate, joy_oldpovstate;

static int			joy_id;
static DWORD		joy_flags;
static DWORD		joy_numbuttons;

static LPDIRECTINPUT		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;

static JOYINFOEX	ji;

static HINSTANCE hInstDI;

//static qboolean	dinput;

typedef struct MYDATA {
	LONG  lX;                   // X axis goes here
	LONG  lY;                   // Y axis goes here
	LONG  lZ;                   // Z axis goes here
	BYTE  bButtonA;             // One button goes here
	BYTE  bButtonB;             // Another button goes here
	BYTE  bButtonC;             // Another button goes here
	BYTE  bButtonD;             // Another button goes here
} MYDATA;

static DIOBJECTDATAFORMAT rgodf[] = {
  { &GUID_XAxis,    FIELD_OFFSET(MYDATA, lX),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_YAxis,    FIELD_OFFSET(MYDATA, lY),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &GUID_ZAxis,    FIELD_OFFSET(MYDATA, lZ),       0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonA), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonB), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonC), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonD), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT	df = {
	sizeof(DIDATAFORMAT),       // this structure
	sizeof(DIOBJECTDATAFORMAT), // size of object data format
	DIDF_RELAXIS,               // absolute axis coordinates
	sizeof(MYDATA),             // device data size
	NUM_OBJECTS,                // number of objects
	rgodf,                      // and here they are
};

// forward-referenced functions
static void IN_StartupJoystick (void);
static void Joy_AdvancedUpdate_f (void);
static void IN_JoyMove (void);
static void IN_StartupMouse (void);


//====================================

void VID_Finish (void)
{
	qboolean vid_usemouse;

	vid_usevsync = vid_vsync.integer && !cls.timedemo && gl_videosyncavailable;
	if (vid_usingvsync != vid_usevsync && gl_videosyncavailable)
	{
		vid_usingvsync = vid_usevsync;
		qwglSwapIntervalEXT (vid_usevsync);
	}

// handle the mouse state when windowed if that's changed
	vid_usemouse = false;
	if (vid_mouse.integer && !key_consoleactive && !cls.demoplayback)
		vid_usemouse = true;
	if (vid_isfullscreen)
		vid_usemouse = true;
	if (!vid_activewindow)
		vid_usemouse = false;
	IN_Activate(vid_usemouse);

	if (r_render.integer && !vid_hidden)
	{
		if (r_speeds.integer || gl_finish.integer)
			qglFinish();
		SwapBuffers(baseDC);
	}
}

//==========================================================================




static qbyte scantokey[128] =
{
//  0           1       2    3     4     5       6       7      8         9      A          B           C       D           E           F
	0          ,27    ,'1'  ,'2'  ,'3'  ,'4'    ,'5'    ,'6'   ,'7'      ,'8'   ,'9'       ,'0'        ,'-'   ,'='         ,K_BACKSPACE,9    ,//0
	'q'        ,'w'   ,'e'  ,'r'  ,'t'  ,'y'    ,'u'    ,'i'   ,'o'      ,'p'   ,'['       ,']'        ,13    ,K_CTRL      ,'a'        ,'s'  ,//1
	'd'        ,'f'   ,'g'  ,'h'  ,'j'  ,'k'    ,'l'    ,';'   ,'\''     ,'`'   ,K_SHIFT   ,'\\'       ,'z'   ,'x'         ,'c'        ,'v'  ,//2
	'b'        ,'n'   ,'m'  ,','  ,'.'  ,'/'    ,K_SHIFT,'*'   ,K_ALT    ,' '   ,0         ,K_F1       ,K_F2  ,K_F3        ,K_F4       ,K_F5 ,//3
	K_F6       ,K_F7  ,K_F8 ,K_F9 ,K_F10,K_PAUSE,0      ,K_HOME,K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,K_RIGHTARROW,K_KP_PLUS  ,K_END,//4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0    ,0      ,0      ,K_F11 ,K_F12    ,0     ,0         ,0          ,0     ,0           ,0          ,0    ,//5
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0         ,0          ,0     ,0           ,0          ,0    ,//6
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0         ,0          ,0     ,0           ,0          ,0     //7
};


/*
=======
MapKey

Map from windows to quake keynums
=======
*/
static int MapKey (int key, int virtualkey)
{
	int result;
	int modified = (key >> 16) & 255;
	qboolean is_extended = false;

	if (modified < 128 && scantokey[modified])
		result = scantokey[modified];
	else
	{
		result = 0;
		Con_DPrintf("key 0x%02x (0x%8x, 0x%8x) has no translation\n", modified, key, virtualkey);
	}

	if (key & (1 << 24))
		is_extended = true;

	if ( !is_extended )
	{
		switch ( result )
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		default:
			return result;
		}
	}
	else
	{
		switch ( result )
		{
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		}
		return result;
	}
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
static void ClearAllStates (void)
{
	Key_ClearStates ();
	if (vid_usingmouse)
		mouse_oldbuttonstate = 0;
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	vid_activewindow = fActive;
	vid_hidden = minimize;

// enable/disable sound on focus gain/loss
	if (!vid_activewindow && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (vid_activewindow && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (vid_isfullscreen)
		{
			if (vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
			}

			// LordHavoc: from dabb, fix for alt-tab bug in NVidia drivers
			MoveWindow(mainwindow,0,0,gdevmode.dmPelsWidth,gdevmode.dmPelsHeight,false);
		}
	}

	if (!fActive)
	{
		IN_Activate (false);
		if (vid_isfullscreen)
		{
			ChangeDisplaySettings (NULL, 0);
			vid_wassuspended = true;
		}
		VID_RestoreSystemGamma();
	}
}

//TODO: move it around in vid_wgl.c since I dont think this is the right position
void Sys_SendKeyEvents (void)
{
	MSG msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();

		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* main window procedure */
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM lParam)
{
	LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	char	state[256];
	char	asciichar[4];
	int		vkey;
	qboolean down = false;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
		case WM_KILLFOCUS:
			if (vid_isfullscreen)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			IN_Activate(false);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			down = true;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			vkey = MapKey(lParam, wParam);
			GetKeyboardState (state);
			// alt/ctrl/shift tend to produce funky ToAscii values,
			// and if it's not a single character we don't know care about it
			if (vkey == K_ALT || vkey == K_CTRL || vkey == K_SHIFT || ToAscii (wParam, lParam >> 16, state, (unsigned short *)asciichar, 0) != 1)
				asciichar[0] = 0;
			Key_Event (vkey, asciichar[0], down);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:   // backslash :: imouse explorer buttons
		case WM_XBUTTONUP:      // backslash :: imouse explorer buttons
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			/* backslash :: imouse explorer buttons */
			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;
			/* :: backslash */

			// LordHavoc: lets hope this allows more buttons in the future...
			if (wParam & MK_XBUTTON3)
				temp |= 32;
			if (wParam & MK_XBUTTON4)
				temp |= 64;
			if (wParam & MK_XBUTTON5)
				temp |= 128;
			if (wParam & MK_XBUTTON6)
				temp |= 256;
			if (wParam & MK_XBUTTON7)
				temp |= 512;

			if (vid_usingmouse && !dinput_acquired)
			{
				// perform button actions
				int i;
				for (i=0 ; i<mouse_buttons ; i++)
					if ((temp ^ mouse_oldbuttonstate) & (1<<i))
						Key_Event (K_MOUSE1 + i, 0, (temp & (1<<i)) != 0);
				mouse_oldbuttonstate = temp;
			}

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, 0, true);
				Key_Event(K_MWHEELUP, 0, false);
			} else {
				Key_Event(K_MWHEELDOWN, 0, true);
				Key_Event(K_MWHEELDOWN, 0, false);
			}
			break;

		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
				Sys_Quit ();

			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

		//case WM_DESTROY:
		//	PostQuitMessage (0);
		//	break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}

int VID_SetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (NULL);
	int i = SetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

int VID_GetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (NULL);
	int i = GetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

static HINSTANCE gldll;

static void GL_CloseLibrary(void)
{
	FreeLibrary(gldll);
	gldll = 0;
	gl_driver[0] = 0;
	qwglGetProcAddress = NULL;
	gl_extensions = "";
	gl_platform = "";
	gl_platformextensions = "";
}

static int GL_OpenLibrary(const char *name)
{
	Con_Printf("Loading OpenGL driver %s\n", name);
	GL_CloseLibrary();
	if (!(gldll = LoadLibrary(name)))
	{
		Con_Printf("Unable to LoadLibrary %s\n", name);
		return false;
	}
	strcpy(gl_driver, name);
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	if (qwglGetProcAddress != NULL)
		p = (void *) qwglGetProcAddress(name);
	if (p == NULL)
		p = (void *) GetProcAddress(gldll, name);
	return p;
}

static void IN_Init(void);
void VID_Init(void)
{
	WNDCLASS wc;

	InitCommonControls();
	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));

	// Register the frame class
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = hIcon;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "DarkPlacesWindowClass";

	if (!RegisterClass (&wc))
		Sys_Error("Couldn't register window class\n");

	IN_Init();
}

int VID_InitMode (int fullscreen, int width, int height, int bpp)
{
	int i;
	HDC hdc;
	RECT rect;
	MSG msg;
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,				// version number
		PFD_DRAW_TO_WINDOW 		// support window
		|  PFD_SUPPORT_OPENGL 	// support OpenGL
		|  PFD_DOUBLEBUFFER ,	// double buffered
		PFD_TYPE_RGBA,			// RGBA type
		24,				// 24-bit color depth
		0, 0, 0, 0, 0, 0,		// color bits ignored
		0,				// no alpha buffer
		0,				// shift bit ignored
		0,				// no accumulation buffer
		0, 0, 0, 0, 			// accum bits ignored
		32,				// 32-bit z-buffer
		0,				// no stencil buffer
		0,				// no auxiliary buffer
		PFD_MAIN_PLANE,			// main layer
		0,				// reserved
		0, 0, 0				// layer masks ignored
	};
	int pixelformat;
	DWORD WindowStyle, ExWindowStyle;
	int CenterX, CenterY;
	const char *gldrivername;
	int depth;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised\n");

	// if stencil is enabled, ask for alpha too
	if (bpp >= 32)
	{
		pfd.cStencilBits = 8;
		pfd.cAlphaBits = 8;
	}
	else
	{
		pfd.cStencilBits = 0;
		pfd.cAlphaBits = 0;
	}

	gldrivername = "opengl32.dll";
// COMMANDLINEOPTION: Windows WGL: -gl_driver <drivername> selects a GL driver library, default is opengl32.dll, useful only for 3dfxogl.dll or 3dfxvgl.dll, if you don't know what this is for, you don't need it
	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		gldrivername = com_argv[i + 1];
	if (!GL_OpenLibrary(gldrivername))
	{
		Con_Printf("Unable to load GL driver %s\n", gldrivername);
		return false;
	}

	memset(&gdevmode, 0, sizeof(gdevmode));

	vid_isfullscreen = false;
	if (fullscreen)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = bpp;
		gdevmode.dmPelsWidth = width;
		gdevmode.dmPelsHeight = height;
		gdevmode.dmSize = sizeof (gdevmode);
		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			VID_Shutdown();
			Con_Printf("Unable to change to requested mode %dx%dx%dbpp\n", width, height, bpp);
			return false;
		}

		vid_isfullscreen = true;
		WindowStyle = WS_POPUP;
		ExWindowStyle = WS_EX_TOPMOST;
	}
	else
	{
		hdc = GetDC (NULL);
		i = GetDeviceCaps(hdc, RASTERCAPS);
		depth = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
		ReleaseDC (NULL, hdc);
		if (i & RC_PALETTE)
		{
			VID_Shutdown();
			Con_Print("Can't run in non-RGB mode\n");
			return false;
		}
		if (bpp > depth)
		{
			VID_Shutdown();
			Con_Print("A higher desktop depth is required to run this video mode\n");
			return false;
		}

		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		ExWindowStyle = 0;
	}

	rect.top = 0;
	rect.left = 0;
	rect.right = width;
	rect.bottom = height;
	AdjustWindowRectEx(&rect, WindowStyle, false, 0);

	if (fullscreen)
	{
		CenterX = 0;
		CenterY = 0;
	}
	else
	{
		CenterX = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
		CenterY = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;
	}
	CenterX = max(0, CenterX);
	CenterY = max(0, CenterY);

	// x and y may be changed by WM_MOVE messages
	window_x = CenterX;
	window_y = CenterY;
	rect.left += CenterX;
	rect.right += CenterX;
	rect.top += CenterY;
	rect.bottom += CenterY;

	mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlacesWindowClass", gamename, WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
	{
		Con_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %d, %p) failed\n", ExWindowStyle, "DarkPlacesWindowClass", gamename, WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
		VID_Shutdown();
		return false;
	}

	/*
	if (!fullscreen)
		SetWindowPos (mainwindow, NULL, CenterX, CenterY, 0, 0,SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	*/

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// now we try to make sure we get the focus on the mode switch, because
	// sometimes in some systems we don't.  We grab the foreground, then
	// finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put
	// ourselves at the top of the z order, then grab the foreground again,
	// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	baseDC = GetDC(mainwindow);

	if ((pixelformat = ChoosePixelFormat(baseDC, &pfd)) == 0)
	{
		VID_Shutdown();
		Con_Printf("ChoosePixelFormat(%d, %p) failed\n", baseDC, &pfd);
		return false;
	}

	if (SetPixelFormat(baseDC, pixelformat, &pfd) == false)
	{
		VID_Shutdown();
		Con_Printf("SetPixelFormat(%d, %d, %p) failed\n", baseDC, pixelformat, &pfd);
		return false;
	}

	if (!GL_CheckExtension("wgl", wglfuncs, NULL, false))
	{
		VID_Shutdown();
		Con_Print("wgl functions not found\n");
		return false;
	}

	baseRC = qwglCreateContext(baseDC);
	if (!baseRC)
	{
		VID_Shutdown();
		Con_Print("Could not initialize GL (wglCreateContext failed).\n\nMake sure you are in 65536 color mode, and try running -window.\n");
		return false;
	}
	if (!qwglMakeCurrent(baseDC, baseRC))
	{
		VID_Shutdown();
		Con_Printf("wglMakeCurrent(%d, %d) failed\n", baseDC, baseRC);
		return false;
	}

	qglGetString = GL_GetProcAddress("glGetString");
	qwglGetExtensionsStringARB = GL_GetProcAddress("wglGetExtensionsStringARB");
	if (qglGetString == NULL)
	{
		VID_Shutdown();
		Con_Print("glGetString not found\n");
		return false;
	}
	gl_renderer = qglGetString(GL_RENDERER);
	gl_vendor = qglGetString(GL_VENDOR);
	gl_version = qglGetString(GL_VERSION);
	gl_extensions = qglGetString(GL_EXTENSIONS);
	gl_platform = "WGL";
	gl_platformextensions = "";

	gl_videosyncavailable = false;

	if (qwglGetExtensionsStringARB)
		gl_platformextensions = qwglGetExtensionsStringARB(baseDC);

// COMMANDLINEOPTION: Windows WGL: -novideosync disables WGL_EXT_swap_control
	gl_videosyncavailable = GL_CheckExtension("WGL_EXT_swap_control", wglswapintervalfuncs, "-novideosync", false);
	//ReleaseDC(mainwindow, hdc);

	GL_Init ();

	// LordHavoc: special differences for ATI (broken 8bit color when also using 32bit? weird!)
	if (strncasecmp(gl_vendor,"ATI",3)==0)
	{
		if (strncasecmp(gl_renderer,"Rage Pro",8)==0)
			isRagePro = true;
	}
	if (strncasecmp(gl_renderer,"Matrox G200 Direct3D",20)==0) // a D3D driver for GL? sigh...
		isG200 = true;

	//vid_menudrawfn = VID_MenuDraw;
	//vid_menukeyfn = VID_MenuKey;
	vid_usingmouse = false;
	vid_usingvsync = false;
	vid_hidden = false;
	vid_initialized = true;

	IN_StartupMouse ();
	IN_StartupJoystick ();

	return true;
}

static void IN_Shutdown(void);
void VID_Shutdown (void)
{
	if(vid_initialized == false)
		return;

	VID_RestoreSystemGamma();

	vid_initialized = false;
	IN_Shutdown();
	if (qwglMakeCurrent)
		qwglMakeCurrent(NULL, NULL);
	if (baseRC && qwglDeleteContext)
		qwglDeleteContext(baseRC);
	// close the library before we get rid of the window
	GL_CloseLibrary();
	if (baseDC && mainwindow)
		ReleaseDC(mainwindow, baseDC);
	AppActivate(false, false);
	if (mainwindow)
		DestroyWindow(mainwindow);
	mainwindow = 0;
	if (vid_isfullscreen)
		ChangeDisplaySettings (NULL, 0);
	vid_isfullscreen = false;
}

static void IN_Activate (qboolean grab)
{
	if (!mouseinitialized)
		return;

	if (grab)
	{
		if (!vid_usingmouse)
		{
			vid_usingmouse = true;
			cl_ignoremousemove = true;
			if (dinput && g_pMouse)
			{
				IDirectInputDevice_Acquire(g_pMouse);
				dinput_acquired = true;
			}
			else
			{
				RECT window_rect;
				window_rect.left = window_x;
				window_rect.top = window_y;
				window_rect.right = window_x + vid.width;
				window_rect.bottom = window_y + vid.height;
				if (mouseparmsvalid)
					restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
				SetCursorPos ((window_x + vid.width / 2), (window_y + vid.height / 2));
				SetCapture (mainwindow);
				ClipCursor (&window_rect);
			}
			ShowCursor (false);
		}
	}
	else
	{
		if (vid_usingmouse)
		{
			vid_usingmouse = false;
			cl_ignoremousemove = true;
			if (dinput_acquired)
			{
				IDirectInputDevice_Unacquire(g_pMouse);
				dinput_acquired = false;
			}
			else
			{
				if (restore_spi)
					SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);
				ClipCursor (NULL);
				ReleaseCapture ();
			}
			ShowCursor (true);
		}
	}
}


/*
===========
IN_InitDInput
===========
*/
static qboolean IN_InitDInput (void)
{
    HRESULT		hr;
	DIPROPDWORD	dipdw = {
		{
			sizeof(DIPROPDWORD),        // diph.dwSize
			sizeof(DIPROPHEADER),       // diph.dwHeaderSize
			0,                          // diph.dwObj
			DIPH_DEVICE,                // diph.dwHow
		},
		DINPUT_BUFFERSIZE,              // dwData
	};

	if (!hInstDI)
	{
		hInstDI = LoadLibrary("dinput.dll");

		if (hInstDI == NULL)
		{
			Con_Print("Couldn't load dinput.dll\n");
			return false;
		}
	}

	if (!pDirectInputCreate)
	{
		pDirectInputCreate = (void *)GetProcAddress(hInstDI,"DirectInputCreateA");

		if (!pDirectInputCreate)
		{
			Con_Print("Couldn't get DI proc addr\n");
			return false;
		}
	}

// register with DirectInput and get an IDirectInput to play with.
	hr = iDirectInputCreate(global_hInstance, DIRECTINPUT_VERSION, &g_pdi, NULL);

	if (FAILED(hr))
	{
		return false;
	}

// obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice(g_pdi, &GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr))
	{
		Con_Print("Couldn't open DI mouse device\n");
		return false;
	}

// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat(g_pMouse, &df);

	if (FAILED(hr))
	{
		Con_Print("Couldn't set DI mouse format\n");
		return false;
	}

// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, mainwindow,
			DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr))
	{
		Con_Print("Couldn't set DI coop level\n");
		return false;
	}


// set the buffer size to DINPUT_BUFFERSIZE elements.
// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr))
	{
		Con_Print("Couldn't set DI buffersize\n");
		return false;
	}

	return true;
}


/*
===========
IN_StartupMouse
===========
*/
static void IN_StartupMouse (void)
{
	if (COM_CheckParm ("-nomouse") || COM_CheckParm("-safe"))
		return;

	mouseinitialized = true;

// COMMANDLINEOPTION: Windows Input: -dinput enables DirectInput for mouse/joystick input
	if (COM_CheckParm ("-dinput"))
		dinput = IN_InitDInput ();

	if (dinput)
		Con_Print("DirectInput initialized\n");
	else
		Con_Print("DirectInput not initialized\n");

	mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);

	if (mouseparmsvalid)
	{
// COMMANDLINEOPTION: Windows GDI Input: -noforcemspd disables setting of mouse speed (not used with -dinput, windows only)
		if ( COM_CheckParm ("-noforcemspd") )
			newmouseparms[2] = originalmouseparms[2];

// COMMANDLINEOPTION: Windows GDI Input: -noforcemaccel disables setting of mouse acceleration (not used with -dinput, windows only)
		if ( COM_CheckParm ("-noforcemaccel") )
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
		}

// COMMANDLINEOPTION: Windows GDI Input: -noforcemparms disables setting of mouse parameters (not used with -dinput, windows only)
		if ( COM_CheckParm ("-noforcemparms") )
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
			newmouseparms[2] = originalmouseparms[2];
		}
	}

	mouse_buttons = 10;
}


/*
===========
IN_MouseMove
===========
*/
static void IN_MouseMove (void)
{
	int i, mx, my;
	POINT current_pos;

	if (!vid_usingmouse)
	{
		//GetCursorPos (&current_pos);
		//ui_mouseupdate(current_pos.x - window_x, current_pos.y - window_y);
		return;
	}

	if (dinput_acquired)
	{
		DIDEVICEOBJECTDATA	od;
		DWORD				dwElements;
		HRESULT				hr;
		mx = 0;
		my = 0;

		for (;;)
		{
			dwElements = 1;

			hr = IDirectInputDevice_GetDeviceData(g_pMouse,
					sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, 0);

			if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
			{
				IDirectInputDevice_Acquire(g_pMouse);
				break;
			}

			/* Unable to read data or no data available */
			if (FAILED(hr) || dwElements == 0)
				break;

			/* Look at the element to see what happened */

			switch (od.dwOfs)
			{
				case DIMOFS_X:
					mx += od.dwData;
					break;

				case DIMOFS_Y:
					my += od.dwData;
					break;

				case DIMOFS_BUTTON0:
					if (od.dwData & 0x80)
						mstate_di |= 1;
					else
						mstate_di &= ~1;
					break;

				case DIMOFS_BUTTON1:
					if (od.dwData & 0x80)
						mstate_di |= (1<<1);
					else
						mstate_di &= ~(1<<1);
					break;

				case DIMOFS_BUTTON2:
					if (od.dwData & 0x80)
						mstate_di |= (1<<2);
					else
						mstate_di &= ~(1<<2);
					break;
			}
		}

		// perform button actions
		for (i=0 ; i<mouse_buttons ; i++)
			if ((mstate_di ^ mouse_oldbuttonstate) & (1<<i))
				Key_Event (K_MOUSE1 + i, 0, (mstate_di & (1<<i)) != 0);
		mouse_oldbuttonstate = mstate_di;

		in_mouse_x = mx;
		in_mouse_y = my;
	}
	else
	{
		GetCursorPos (&current_pos);
		mx = current_pos.x - (window_x + vid.width / 2);
		my = current_pos.y - (window_y + vid.height / 2);

		in_mouse_x = mx;
		in_mouse_y = my;

		// if the mouse has moved, force it to the center, so there's room to move
		if (mx || my)
			SetCursorPos ((window_x + vid.width / 2), (window_y + vid.height / 2));
	}
}


/*
===========
IN_Move
===========
*/
void IN_Move (void)
{
	if (vid_activewindow && !vid_hidden)
	{
		IN_MouseMove ();
		IN_JoyMove ();
	}
}


/*
===============
IN_StartupJoystick
===============
*/
static void IN_StartupJoystick (void)
{
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr;
	mmr = 0;

 	// assume no joystick
	joy_avail = false;

	// abort startup if user requests no joystick
// COMMANDLINEOPTION: Windows Input: -nojoy disables joystick support, may be a small speed increase
	if (COM_CheckParm ("-nojoy") || COM_CheckParm("-safe"))
		return;

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		Con_Print("\njoystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Con_Printf("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Con_Printf("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr);
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = true;
	joy_advancedinit = false;

	Con_Print("\njoystick detected\n\n");
}


/*
===========
RawValuePointer
===========
*/
static PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}
	return NULL; // LordHavoc: hush compiler warning
}


/*
===========
Joy_AdvancedUpdate_f
===========
*/
static void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced.integer == 0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (strcmp (joy_name.string, "joystick") != 0)
		{
			// notify user of advanced controller
			Con_Printf("\n%s configured\n\n", joy_name.string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy.value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz.value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr.value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu.value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv.value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
}

/*
===============
IN_ReadJoystick
===============
*/
static qboolean IN_ReadJoystick (void)
{

	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if (joy_wwhack1.integer != 0.0)
		{
			ji.dwUpos += 100;
		}
		return true;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,
		// but what should be done?
		return false;
	}
}


/*
===========
IN_JoyMove
===========
*/
static void IN_JoyMove (void)
{
	float	speed, aspeed;
	float	fAxisValue, fTemp;
	int		i, mouselook = (in_mlook.state & 1) || freelook.integer;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	if (joy_avail)
	{
		int		i, key_index;
		DWORD	buttonstate, povstate;

		// loop through the joystick buttons
		// key a joystick event or auxillary event for higher number buttons for each state change
		buttonstate = ji.dwButtons;
		for (i=0 ; i < (int) joy_numbuttons ; i++)
		{
			if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
			{
				key_index = (i < 16) ? K_JOY1 : K_AUX1;
				Key_Event (key_index + i, 0, true);
			}

			if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
			{
				key_index = (i < 16) ? K_JOY1 : K_AUX1;
				Key_Event (key_index + i, 0, false);
			}
		}
		joy_oldbuttonstate = buttonstate;

		if (joy_haspov)
		{
			// convert POV information into 4 bits of state information
			// this avoids any potential problems related to moving from one
			// direction to another without going through the center position
			povstate = 0;
			if(ji.dwPOV != JOY_POVCENTERED)
			{
				if (ji.dwPOV == JOY_POVFORWARD)
					povstate |= 0x01;
				if (ji.dwPOV == JOY_POVRIGHT)
					povstate |= 0x02;
				if (ji.dwPOV == JOY_POVBACKWARD)
					povstate |= 0x04;
				if (ji.dwPOV == JOY_POVLEFT)
					povstate |= 0x08;
			}
			// determine which bits have changed and key an auxillary event for each change
			for (i=0 ; i < 4 ; i++)
			{
				if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
				{
					Key_Event (K_AUX29 + i, 0, true);
				}

				if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
				{
					Key_Event (K_AUX29 + i, 0, false);
				}
			}
			joy_oldpovstate = povstate;
		}
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick.integer)
	{
		return;
	}

	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
	{
		return;
	}

	if (in_speed.state & 1)
		speed = cl_movespeedkey.value;
	else
		speed = 1;
	// LordHavoc: viewzoom affects sensitivity for sniping
	aspeed = speed * host_realframetime * cl.viewzoom;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		if (joy_wwhack2.integer != 0.0)
		{
			if (dwAxisMap[i] == AxisTurn)
			{
				// this is a special formula for the Logitech WingMan Warrior
				// y=ax^b; where a = 300 and b = 1.3
				// also x values are in increments of 800 (so this is factored out)
				// then bounds check result to level out excessively high spin rates
				fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
				if (fTemp > 14000.0)
					fTemp = 14000.0;
				// restore direction information
				fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
			}
		}

		// convert range from -32768..32767 to -1..1
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced.integer == 0) && mouselook)
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
					{
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift();
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold.value)
				{
					cl.cmd.forwardmove += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold.value)
			{
				cl.cmd.sidemove += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe.integer && mouselook))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold.value)
				{
					cl.cmd.sidemove -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold.value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					}
					else
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (mouselook)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					}
					V_StopPitchDrift();
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.integer == 0)
						V_StopPitchDrift();
				}
			}
			break;

		default:
			break;
		}
	}
}

static void IN_Init(void)
{
	uiWheelMessage = RegisterWindowMessage ( "MSWHEEL_ROLLMSG" );

	// joystick variables
	Cvar_RegisterVariable (&in_joystick);
	Cvar_RegisterVariable (&joy_name);
	Cvar_RegisterVariable (&joy_advanced);
	Cvar_RegisterVariable (&joy_advaxisx);
	Cvar_RegisterVariable (&joy_advaxisy);
	Cvar_RegisterVariable (&joy_advaxisz);
	Cvar_RegisterVariable (&joy_advaxisr);
	Cvar_RegisterVariable (&joy_advaxisu);
	Cvar_RegisterVariable (&joy_advaxisv);
	Cvar_RegisterVariable (&joy_forwardthreshold);
	Cvar_RegisterVariable (&joy_sidethreshold);
	Cvar_RegisterVariable (&joy_pitchthreshold);
	Cvar_RegisterVariable (&joy_yawthreshold);
	Cvar_RegisterVariable (&joy_forwardsensitivity);
	Cvar_RegisterVariable (&joy_sidesensitivity);
	Cvar_RegisterVariable (&joy_pitchsensitivity);
	Cvar_RegisterVariable (&joy_yawsensitivity);
	Cvar_RegisterVariable (&joy_wwhack1);
	Cvar_RegisterVariable (&joy_wwhack2);
	Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);
}

static void IN_Shutdown(void)
{
	IN_Activate (false);

	if (g_pMouse)
		IDirectInputDevice_Release(g_pMouse);
	g_pMouse = NULL;

	if (g_pdi)
		IDirectInput_Release(g_pdi);
	g_pdi = NULL;
}
