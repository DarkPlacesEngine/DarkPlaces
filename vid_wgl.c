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
// vid_wgl.c -- NT GL vid component

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#endif

#ifdef SUPPORTDIRECTX
// Include DX libs
#ifdef _MSC_VER
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#endif
#ifndef DIRECTINPUT_VERSION
#	define DIRECTINPUT_VERSION 0x0500  /* Version 5.0 */
#endif
#endif

#include "quakedef.h"
#include <windows.h>
#include <mmsystem.h>
#ifdef SUPPORTDIRECTX
#include <dsound.h>
#endif
#include "resource.h"
#include <commctrl.h>
#ifdef SUPPORTDIRECTX
#include <dinput.h>
#endif
#include "dpsoftrast.h"

#ifdef SUPPORTD3D
#include <d3d9.h>

cvar_t vid_dx9 = {CVAR_SAVE, "vid_dx9", "0", "use Microsoft Direct3D9(r) for rendering"};
cvar_t vid_dx9_hal = {CVAR_SAVE, "vid_dx9_hal", "1", "enables hardware rendering (1), otherwise software reference rasterizer (0 - very slow), note that 0 is necessary when using NVPerfHUD (which renders in hardware but requires this option to enable it)"};
cvar_t vid_dx9_softvertex = {CVAR_SAVE, "vid_dx9_softvertex", "0", "enables software vertex processing (for compatibility testing?  or if you have a very fast CPU), usually you want this off"};
cvar_t vid_dx9_triplebuffer = {CVAR_SAVE, "vid_dx9_triplebuffer", "0", "enables triple buffering when using vid_vsync in fullscreen, this options adds some latency and only helps when framerate is below 60 so you usually don't want it"};
//cvar_t vid_dx10 = {CVAR_SAVE, "vid_dx10", "1", "use Microsoft Direct3D10(r) for rendering"};
//cvar_t vid_dx11 = {CVAR_SAVE, "vid_dx11", "1", "use Microsoft Direct3D11(r) for rendering"};

D3DPRESENT_PARAMETERS vid_d3dpresentparameters;

// we declare this in vid_shared.c because it is required by dedicated server and all clients when SUPPORTD3D is defined
extern LPDIRECT3DDEVICE9 vid_d3d9dev;

LPDIRECT3D9 vid_d3d9;
D3DCAPS9 vid_d3d9caps;
qboolean vid_d3ddevicelost;
#endif

extern HINSTANCE global_hInstance;

static HINSTANCE gldll;

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif

// Tell startup code that we have a client
int cl_available = true;

qboolean vid_supportrefreshrate = true;

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
static BOOL (WINAPI *qwglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
static BOOL (WINAPI *qwglGetPixelFormatAttribivARB)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues);

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

static dllfunction_t wglpixelformatfuncs[] =
{
	{"wglChoosePixelFormatARB", (void **) &qwglChoosePixelFormatARB},
	{"wglGetPixelFormatAttribivARB", (void **) &qwglGetPixelFormatAttribivARB},
	{NULL, NULL}
};

static DEVMODE gdevmode, initialdevmode;
static qboolean vid_initialized = false;
static qboolean vid_wassuspended = false;
static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_usingvsync = false;
static qboolean vid_usevsync = false;
static HICON hIcon;

// used by cd_win.c and snd_win.c
HWND mainwindow;

static HDC	 baseDC;
static HGLRC baseRC;

static HDC vid_softhdc;
static HGDIOBJ vid_softhdc_backup;
static BITMAPINFO vid_softbmi;
static HBITMAP vid_softdibhandle;

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

static qboolean mouseinitialized;

#ifdef SUPPORTDIRECTX
static qboolean dinput;
#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

static HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);
#endif

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
#endif
#ifndef MK_XBUTTON3
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

static unsigned int uiWheelMessage;
#ifdef SUPPORTDIRECTX
static qboolean	dinput_acquired;

static unsigned int		mstate_di;
#endif

static cvar_t vid_forcerefreshrate = {0, "vid_forcerefreshrate", "0", "try to set the given vid_refreshrate even if Windows doesn't list it as valid video mode"};

#ifdef SUPPORTDIRECTX
static LPDIRECTINPUT		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;
static HINSTANCE hInstDI;
#endif

// forward-referenced functions
static void IN_StartupMouse (void);


//====================================

qboolean vid_reallyhidden = true;
#ifdef SUPPORTD3D
qboolean vid_begunscene = false;
#endif
void VID_Finish (void)
{
#ifdef SUPPORTD3D
	HRESULT hr;
#endif
	vid_hidden = vid_reallyhidden;

	vid_usevsync = vid_vsync.integer && !cls.timedemo && qwglSwapIntervalEXT;

	if (!vid_hidden)
	{
		switch(vid.renderpath)
		{
		case RENDERPATH_GL11:
		case RENDERPATH_GL13:
		case RENDERPATH_GL20:
		case RENDERPATH_GLES1:
		case RENDERPATH_GLES2:
			if (vid_usingvsync != vid_usevsync)
			{
				vid_usingvsync = vid_usevsync;
				qwglSwapIntervalEXT (vid_usevsync);
			}
			if (r_speeds.integer == 2 || gl_finish.integer)
				GL_Finish();
			SwapBuffers(baseDC);
			break;
		case RENDERPATH_D3D9:
#ifdef SUPPORTD3D
			if (vid_begunscene)
			{
				IDirect3DDevice9_EndScene(vid_d3d9dev);
				vid_begunscene = false;
			}
			if (!vid_reallyhidden)
			{
				if (!vid_d3ddevicelost)
				{
					vid_hidden = vid_reallyhidden;
					hr = IDirect3DDevice9_Present(vid_d3d9dev, NULL, NULL, NULL, NULL);
					if (hr == D3DERR_DEVICELOST)
					{
						vid_d3ddevicelost = true;
						vid_hidden = true;
						Sleep(100);
					}
				}
				else
				{
					hr = IDirect3DDevice9_TestCooperativeLevel(vid_d3d9dev);
					switch(hr)
					{
					case D3DERR_DEVICELOST:
						vid_d3ddevicelost = true;
						vid_hidden = true;
						Sleep(100);
						break;
					case D3DERR_DEVICENOTRESET:
						vid_d3ddevicelost = false;
						vid_hidden = vid_reallyhidden;
						R_Modules_DeviceLost();
						IDirect3DDevice9_Reset(vid_d3d9dev, &vid_d3dpresentparameters);
						R_Modules_DeviceRestored();
						break;
					case D3D_OK:
						vid_hidden = vid_reallyhidden;
						IDirect3DDevice9_Present(vid_d3d9dev, NULL, NULL, NULL, NULL);
						break;
					}
				}
				if (!vid_begunscene && !vid_hidden)
				{
					IDirect3DDevice9_BeginScene(vid_d3d9dev);
					vid_begunscene = true;
				}
			}
#endif
			break;
		case RENDERPATH_D3D10:
			break;
		case RENDERPATH_D3D11:
			break;
		case RENDERPATH_SOFT:
			DPSOFTRAST_Finish();
//			baseDC = GetDC(mainwindow);
			BitBlt(baseDC, 0, 0, vid.width, vid.height, vid_softhdc, 0, 0, SRCCOPY);
//			ReleaseDC(mainwindow, baseDC);
//			baseDC = NULL;
			break;
		}
	}

	// make sure a context switch can happen every frame - Logitech drivers
	// input drivers sometimes eat cpu time every 3 seconds or lag badly
	// without this help
	Sleep(0);

	VID_UpdateGamma(false, 256);
}

//==========================================================================




static unsigned char scantokey[128] =
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
	static qboolean sound_active = false;  // initially blocked by Sys_InitConsole()

	vid_activewindow = fActive != FALSE;
	vid_reallyhidden = minimize != FALSE;

	// enable/disable sound on focus gain/loss
	if ((!vid_reallyhidden && vid_activewindow) || !snd_mutewhenidle.integer)
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

	if (fActive)
	{
		if (vid_isfullscreen)
		{
			if (vid_wassuspended)
			{
				vid_wassuspended = false;
				if (gldll)
				{
					ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
					ShowWindow(mainwindow, SW_SHOWNORMAL);
				}
			}

			// LordHavoc: from dabb, fix for alt-tab bug in NVidia drivers
			if (gldll)
				MoveWindow(mainwindow,0,0,gdevmode.dmPelsWidth,gdevmode.dmPelsHeight,false);
		}
	}

	if (!fActive)
	{
		VID_SetMouse(false, false, false);
		if (vid_isfullscreen)
		{
			if (gldll)
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
			Sys_Quit (1);

		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static keynum_t buttonremap[16] =
{
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
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

/* main window procedure */
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM lParam)
{
	LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	unsigned char state[256];
	unsigned char asciichar[4];
	int		vkey;
	int		charlength;
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
			VID_SetMouse(false, false, false);
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
			charlength = ToAscii (wParam, lParam >> 16, state, (LPWORD)asciichar, 0);
			if (vkey == K_ALT || vkey == K_CTRL || vkey == K_SHIFT || charlength == 0)
				asciichar[0] = 0;
			else if( charlength == 2 ) {
				asciichar[0] = asciichar[1];
			}
			if (!VID_JoyBlockEmulatedKeys(vkey))
				Key_Event (vkey, asciichar[0], down);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

		case WM_SYSCOMMAND:
			// prevent screensaver from occuring while the active window
			// note: password-locked screensavers on Vista still work
			if (vid_activewindow && ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER))
				lRet = 0;
			else
				lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
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

#ifdef SUPPORTDIRECTX
			if (!dinput_acquired)
#endif
			{
				// perform button actions
				int i;
				for (i=0 ; i<mouse_buttons && i < 16 ; i++)
					if ((temp ^ mouse_oldbuttonstate) & (1<<i))
						Key_Event (buttonremap[i], 0, (temp & (1<<i)) != 0);
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
				Sys_Quit (0);

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

int VID_SetGamma(unsigned short *ramps, int rampsize)
{
	if (qwglMakeCurrent)
	{
		HDC hdc = GetDC (NULL);
		int i = SetDeviceGammaRamp(hdc, ramps);
		ReleaseDC (NULL, hdc);
		return i; // return success or failure
	}
	else
		return 0;
}

int VID_GetGamma(unsigned short *ramps, int rampsize)
{
	if (qwglMakeCurrent)
	{
		HDC hdc = GetDC (NULL);
		int i = GetDeviceGammaRamp(hdc, ramps);
		ReleaseDC (NULL, hdc);
		return i; // return success or failure
	}
	else
		return 0;
}

static void GL_CloseLibrary(void)
{
	if (gldll)
	{
		FreeLibrary(gldll);
		gldll = 0;
		gl_driver[0] = 0;
		qwglGetProcAddress = NULL;
		gl_extensions = "";
		gl_platform = "";
		gl_platformextensions = "";
	}
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
	strlcpy(gl_driver, name, sizeof(gl_driver));
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	if (gldll)
	{
		void *p = NULL;
		if (qwglGetProcAddress != NULL)
			p = (void *) qwglGetProcAddress(name);
		if (p == NULL)
			p = (void *) GetProcAddress(gldll, name);
		return p;
	}
	else
		return NULL;
}

#ifndef WGL_ARB_pixel_format
#define WGL_NUMBER_PIXEL_FORMATS_ARB   0x2000
#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_DRAW_TO_BITMAP_ARB         0x2002
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_NEED_PALETTE_ARB           0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB    0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB     0x2006
#define WGL_SWAP_METHOD_ARB            0x2007
#define WGL_NUMBER_OVERLAYS_ARB        0x2008
#define WGL_NUMBER_UNDERLAYS_ARB       0x2009
#define WGL_TRANSPARENT_ARB            0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB  0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB 0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB 0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB 0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB 0x203B
#define WGL_SHARE_DEPTH_ARB            0x200C
#define WGL_SHARE_STENCIL_ARB          0x200D
#define WGL_SHARE_ACCUM_ARB            0x200E
#define WGL_SUPPORT_GDI_ARB            0x200F
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_STEREO_ARB                 0x2012
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_RED_BITS_ARB               0x2015
#define WGL_RED_SHIFT_ARB              0x2016
#define WGL_GREEN_BITS_ARB             0x2017
#define WGL_GREEN_SHIFT_ARB            0x2018
#define WGL_BLUE_BITS_ARB              0x2019
#define WGL_BLUE_SHIFT_ARB             0x201A
#define WGL_ALPHA_BITS_ARB             0x201B
#define WGL_ALPHA_SHIFT_ARB            0x201C
#define WGL_ACCUM_BITS_ARB             0x201D
#define WGL_ACCUM_RED_BITS_ARB         0x201E
#define WGL_ACCUM_GREEN_BITS_ARB       0x201F
#define WGL_ACCUM_BLUE_BITS_ARB        0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB       0x2021
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_AUX_BUFFERS_ARB            0x2024
#define WGL_NO_ACCELERATION_ARB        0x2025
#define WGL_GENERIC_ACCELERATION_ARB   0x2026
#define WGL_FULL_ACCELERATION_ARB      0x2027
#define WGL_SWAP_EXCHANGE_ARB          0x2028
#define WGL_SWAP_COPY_ARB              0x2029
#define WGL_SWAP_UNDEFINED_ARB         0x202A
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_TYPE_COLORINDEX_ARB        0x202C
#endif

#ifndef WGL_ARB_multisample
#define WGL_SAMPLE_BUFFERS_ARB         0x2041
#define WGL_SAMPLES_ARB                0x2042
#endif


static void IN_Init(void);
void VID_Init(void)
{
	WNDCLASS wc;

#ifdef SUPPORTD3D
	Cvar_RegisterVariable(&vid_dx9);
	Cvar_RegisterVariable(&vid_dx9_hal);
	Cvar_RegisterVariable(&vid_dx9_softvertex);
	Cvar_RegisterVariable(&vid_dx9_triplebuffer);
//	Cvar_RegisterVariable(&vid_dx10);
//	Cvar_RegisterVariable(&vid_dx11);
#endif

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
		Con_Printf ("Couldn't register window class\n");

	memset(&initialdevmode, 0, sizeof(initialdevmode));
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &initialdevmode);

	IN_Init();
}

qboolean VID_InitModeGL(viddef_mode_t *mode)
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
	int windowpass;
	int pixelformat, newpixelformat;
	UINT numpixelformats;
	DWORD WindowStyle, ExWindowStyle;
	int CenterX, CenterY;
	const char *gldrivername;
	int depth;
	DEVMODE thismode;
	qboolean foundmode, foundgoodmode;
	int *a;
	float *af;
	int attribs[128];
	float attribsf[16];
	int bpp = mode->bitsperpixel;
	int width = mode->width;
	int height = mode->height;
	int refreshrate = (int)floor(mode->refreshrate+0.5);
	int stereobuffer = mode->stereobuffer;
	int samples = mode->samples;
	int fullscreen = mode->fullscreen;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised");

	// if stencil is enabled, ask for alpha too
	if (bpp >= 32)
	{
		pfd.cRedBits = 8;
		pfd.cGreenBits = 8;
		pfd.cBlueBits = 8;
		pfd.cAlphaBits = 8;
		pfd.cDepthBits = 24;
		pfd.cStencilBits = 8;
	}
	else
	{
		pfd.cRedBits = 5;
		pfd.cGreenBits = 5;
		pfd.cBlueBits = 5;
		pfd.cAlphaBits = 0;
		pfd.cDepthBits = 16;
		pfd.cStencilBits = 0;
	}

	if (stereobuffer)
		pfd.dwFlags |= PFD_STEREO;

	a = attribs;
	af = attribsf;
	*a++ = WGL_DRAW_TO_WINDOW_ARB;
	*a++ = GL_TRUE;
	*a++ = WGL_ACCELERATION_ARB;
	*a++ = WGL_FULL_ACCELERATION_ARB;
	*a++ = WGL_DOUBLE_BUFFER_ARB;
	*a++ = true;

	if (bpp >= 32)
	{
		*a++ = WGL_RED_BITS_ARB;
		*a++ = 8;
		*a++ = WGL_GREEN_BITS_ARB;
		*a++ = 8;
		*a++ = WGL_BLUE_BITS_ARB;
		*a++ = 8;
		*a++ = WGL_ALPHA_BITS_ARB;
		*a++ = 8;
		*a++ = WGL_DEPTH_BITS_ARB;
		*a++ = 24;
		*a++ = WGL_STENCIL_BITS_ARB;
		*a++ = 8;
	}
	else
	{
		*a++ = WGL_RED_BITS_ARB;
		*a++ = 1;
		*a++ = WGL_GREEN_BITS_ARB;
		*a++ = 1;
		*a++ = WGL_BLUE_BITS_ARB;
		*a++ = 1;
		*a++ = WGL_DEPTH_BITS_ARB;
		*a++ = 16;
	}

	if (stereobuffer)
	{
		*a++ = WGL_STEREO_ARB;
		*a++ = GL_TRUE;
	}

	if (samples > 1)
	{
		*a++ = WGL_SAMPLE_BUFFERS_ARB;
		*a++ = 1;
		*a++ = WGL_SAMPLES_ARB;
		*a++ = samples;
	}

	*a = 0;
	*af = 0;

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
		if(vid_forcerefreshrate.integer)
		{
			foundmode = true;
			gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			gdevmode.dmBitsPerPel = bpp;
			gdevmode.dmPelsWidth = width;
			gdevmode.dmPelsHeight = height;
			gdevmode.dmSize = sizeof (gdevmode);
			if(refreshrate)
			{
				gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
				gdevmode.dmDisplayFrequency = refreshrate;
			}
		}
		else
		{
			if(refreshrate == 0)
				refreshrate = initialdevmode.dmDisplayFrequency; // default vid_refreshrate to the rate of the desktop

			foundmode = false;
			foundgoodmode = false;

			thismode.dmSize = sizeof(thismode);
			thismode.dmDriverExtra = 0;
			for(i = 0; EnumDisplaySettings(NULL, i, &thismode); ++i)
			{
				if(~thismode.dmFields & (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY))
				{
					Con_DPrintf("enumerating modes yielded a bogus item... please debug this\n");
					continue;
				}
				if(developer_extra.integer)
					Con_DPrintf("Found mode %dx%dx%dbpp %dHz... ", (int)thismode.dmPelsWidth, (int)thismode.dmPelsHeight, (int)thismode.dmBitsPerPel, (int)thismode.dmDisplayFrequency);
				if(thismode.dmBitsPerPel != (DWORD)bpp)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong bpp\n");
					continue;
				}
				if(thismode.dmPelsWidth != (DWORD)width)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong width\n");
					continue;
				}
				if(thismode.dmPelsHeight != (DWORD)height)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong height\n");
					continue;
				}

				if(foundgoodmode)
				{
					// if we have a good mode, make sure this mode is better than the previous one, and allowed by the refreshrate
					if(thismode.dmDisplayFrequency > (DWORD)refreshrate)
					{
						if(developer_extra.integer)
							Con_DPrintf("too high refresh rate\n");
						continue;
					}
					else if(thismode.dmDisplayFrequency <= gdevmode.dmDisplayFrequency)
					{
						if(developer_extra.integer)
							Con_DPrintf("doesn't beat previous best match (too low)\n");
						continue;
					}
				}
				else if(foundmode)
				{
					// we do have one, but it isn't good... make sure it has a lower frequency than the previous one
					if(thismode.dmDisplayFrequency >= gdevmode.dmDisplayFrequency)
					{
						if(developer_extra.integer)
							Con_DPrintf("doesn't beat previous best match (too high)\n");
						continue;
					}
				}
				// otherwise, take anything

				memcpy(&gdevmode, &thismode, sizeof(gdevmode));
				if(thismode.dmDisplayFrequency <= (DWORD)refreshrate)
					foundgoodmode = true;
				else
				{
					if(developer_extra.integer)
						Con_DPrintf("(out of range)\n");
				}
				foundmode = true;
				if(developer_extra.integer)
					Con_DPrintf("accepted\n");
			}
		}

		if (!foundmode)
		{
			VID_Shutdown();
			Con_Printf("Unable to find the requested mode %dx%dx%dbpp\n", width, height, bpp);
			return false;
		}
		else if(ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
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

	pixelformat = 0;
	newpixelformat = 0;
	// start out at the final windowpass if samples is 1 as it's the only feature we need extended pixel formats for
	for (windowpass = samples == 1;windowpass < 2;windowpass++)
	{
		gl_extensions = "";
		gl_platformextensions = "";

		mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlacesWindowClass", gamename, WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
		if (!mainwindow)
		{
			Con_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %p, %p) failed\n", (int)ExWindowStyle, "DarkPlacesWindowClass", gamename, (int)WindowStyle, (int)(rect.left), (int)(rect.top), (int)(rect.right - rect.left), (int)(rect.bottom - rect.top), (void *)NULL, (void *)NULL, (void *)global_hInstance, (void *)NULL);
			VID_Shutdown();
			return false;
		}

		baseDC = GetDC(mainwindow);

		if (!newpixelformat)
			newpixelformat = ChoosePixelFormat(baseDC, &pfd);
		pixelformat = newpixelformat;
		if (!pixelformat)
		{
			VID_Shutdown();
			Con_Printf("ChoosePixelFormat(%p, %p) failed\n", (void *)baseDC, (void *)&pfd);
			return false;
		}

		if (SetPixelFormat(baseDC, pixelformat, &pfd) == false)
		{
			VID_Shutdown();
			Con_Printf("SetPixelFormat(%p, %d, %p) failed\n", (void *)baseDC, pixelformat, (void *)&pfd);
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
			Con_Printf("wglMakeCurrent(%p, %p) failed\n", (void *)baseDC, (void *)baseRC);
			return false;
		}

		if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
		{
			VID_Shutdown();
			Con_Print("glGetString not found\n");
			return false;
		}
		if ((qwglGetExtensionsStringARB = (const char *(WINAPI *)(HDC hdc))GL_GetProcAddress("wglGetExtensionsStringARB")) == NULL)
			Con_Print("wglGetExtensionsStringARB not found\n");

		gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
		gl_platform = "WGL";
		gl_platformextensions = "";

		if (qwglGetExtensionsStringARB)
			gl_platformextensions = (const char *)qwglGetExtensionsStringARB(baseDC);

		if (!gl_extensions)
			gl_extensions = "";
		if (!gl_platformextensions)
			gl_platformextensions = "";

		// now some nice Windows pain:
		// we have created a window, we needed one to find out if there are
		// any multisample pixel formats available, the problem is that to
		// actually use one of those multisample formats we now have to
		// recreate the window (yes Microsoft OpenGL really is that bad)

		if (windowpass == 0)
		{
			if (!GL_CheckExtension("WGL_ARB_pixel_format", wglpixelformatfuncs, "-noarbpixelformat", false) || !qwglChoosePixelFormatARB(baseDC, attribs, attribsf, 1, &newpixelformat, &numpixelformats) || !newpixelformat)
				break;
			// ok we got one - do it all over again with newpixelformat
			qwglMakeCurrent(NULL, NULL);
			qwglDeleteContext(baseRC);baseRC = 0;
			ReleaseDC(mainwindow, baseDC);baseDC = 0;
			// eat up any messages waiting for us
			while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
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

// COMMANDLINEOPTION: Windows WGL: -novideosync disables WGL_EXT_swap_control
	GL_CheckExtension("WGL_EXT_swap_control", wglswapintervalfuncs, "-novideosync", false);

	GL_Init ();

	//vid_menudrawfn = VID_MenuDraw;
	//vid_menukeyfn = VID_MenuKey;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
	vid_usingvsync = false;
	vid_reallyhidden = vid_hidden = false;
	vid_initialized = true;

	IN_StartupMouse ();

	if (qwglSwapIntervalEXT)
	{
		vid_usevsync = vid_vsync.integer != 0;
		vid_usingvsync = vid_vsync.integer != 0;
		qwglSwapIntervalEXT (vid_usevsync);
	}

	return true;
}

#ifdef SUPPORTD3D
static D3DADAPTER_IDENTIFIER9 d3d9adapteridentifier;

extern cvar_t gl_info_extensions;
extern cvar_t gl_info_vendor;
extern cvar_t gl_info_renderer;
extern cvar_t gl_info_version;
extern cvar_t gl_info_platform;
extern cvar_t gl_info_driver;
qboolean VID_InitModeDX(viddef_mode_t *mode, int version)
{
	int deviceindex;
	RECT rect;
	MSG msg;
	DWORD WindowStyle, ExWindowStyle;
	int CenterX, CenterY;
	int bpp = mode->bitsperpixel;
	int width = mode->width;
	int height = mode->height;
	int refreshrate = (int)floor(mode->refreshrate+0.5);
//	int stereobuffer = mode->stereobuffer;
	int samples = mode->samples;
	int fullscreen = mode->fullscreen;
	int numdevices;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised");

	vid_isfullscreen = fullscreen != 0;
	if (fullscreen)
	{
		WindowStyle = WS_POPUP;
		ExWindowStyle = WS_EX_TOPMOST;
	}
	else
	{
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

	gl_extensions = "";
	gl_platformextensions = "";

	mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlacesWindowClass", gamename, WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
	{
		Con_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %p, %p) failed\n", (int)ExWindowStyle, "DarkPlacesWindowClass", gamename, (int)WindowStyle, (int)(rect.left), (int)(rect.top), (int)(rect.right - rect.left), (int)(rect.bottom - rect.top), (void *)NULL, (void *)NULL, global_hInstance, (void *)NULL);
		VID_Shutdown();
		return false;
	}

	baseDC = GetDC(mainwindow);

	vid_d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!vid_d3d9)
		Sys_Error("VID_InitMode: Direct3DCreate9 failed");

	numdevices = IDirect3D9_GetAdapterCount(vid_d3d9);
	vid_d3d9dev = NULL;
	memset(&d3d9adapteridentifier, 0, sizeof(d3d9adapteridentifier));
	for (deviceindex = 0;deviceindex < numdevices && !vid_d3d9dev;deviceindex++)
	{
		memset(&vid_d3dpresentparameters, 0, sizeof(vid_d3dpresentparameters));
//		vid_d3dpresentparameters.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
		vid_d3dpresentparameters.Flags = 0;
		vid_d3dpresentparameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
		vid_d3dpresentparameters.hDeviceWindow = mainwindow;
		vid_d3dpresentparameters.BackBufferWidth = width;
		vid_d3dpresentparameters.BackBufferHeight = height;
		vid_d3dpresentparameters.MultiSampleType = samples > 1 ? (D3DMULTISAMPLE_TYPE)samples : D3DMULTISAMPLE_NONE;
		vid_d3dpresentparameters.BackBufferCount = fullscreen ? (vid_dx9_triplebuffer.integer ? 3 : 2) : 1;
		vid_d3dpresentparameters.FullScreen_RefreshRateInHz = fullscreen ? refreshrate : 0;
		vid_d3dpresentparameters.Windowed = !fullscreen;
		vid_d3dpresentparameters.EnableAutoDepthStencil = true;
		vid_d3dpresentparameters.AutoDepthStencilFormat = bpp > 16 ? D3DFMT_D24S8 : D3DFMT_D16;
		vid_d3dpresentparameters.BackBufferFormat = fullscreen?D3DFMT_X8R8G8B8:D3DFMT_UNKNOWN;
		vid_d3dpresentparameters.PresentationInterval = vid_vsync.integer ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		memset(&d3d9adapteridentifier, 0, sizeof(d3d9adapteridentifier));
		IDirect3D9_GetAdapterIdentifier(vid_d3d9, deviceindex, 0, &d3d9adapteridentifier);

		IDirect3D9_CreateDevice(vid_d3d9, deviceindex, vid_dx9_hal.integer ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF, mainwindow, vid_dx9_softvertex.integer ? D3DCREATE_SOFTWARE_VERTEXPROCESSING : D3DCREATE_HARDWARE_VERTEXPROCESSING, &vid_d3dpresentparameters, &vid_d3d9dev);
	}

	if (!vid_d3d9dev)
	{
		VID_Shutdown();
		return false;
	}

	IDirect3DDevice9_GetDeviceCaps(vid_d3d9dev, &vid_d3d9caps);

	Con_Printf("Using D3D9 device: %s\n", d3d9adapteridentifier.Description);
	gl_extensions = "";
	gl_platform = "D3D9";
	gl_platformextensions = "";

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

	gl_renderer = d3d9adapteridentifier.Description;
	gl_vendor = d3d9adapteridentifier.Driver;
	gl_version = "";
	gl_extensions = "";

	Con_Printf("D3D9 adapter info:\n");
	Con_Printf("Description: %s\n", d3d9adapteridentifier.Description);
	Con_Printf("DeviceId: %x\n", (unsigned int)d3d9adapteridentifier.DeviceId);
	Con_Printf("DeviceName: %p\n", d3d9adapteridentifier.DeviceName);
	Con_Printf("Driver: %s\n", d3d9adapteridentifier.Driver);
	Con_Printf("DriverVersion: %08x%08x\n", (unsigned int)d3d9adapteridentifier.DriverVersion.u.HighPart, (unsigned int)d3d9adapteridentifier.DriverVersion.u.LowPart);
	Con_DPrintf("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_DPrintf("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	// clear the extension flags
	memset(&vid.support, 0, sizeof(vid.support));
	Cvar_SetQuick(&gl_info_extensions, "");

	// D3D9 requires BGRA
	vid.forcetextype = TEXTYPE_BGRA;

	vid.forcevbo = false;
	vid.support.arb_depth_texture = true;
	vid.support.arb_draw_buffers = vid_d3d9caps.NumSimultaneousRTs > 1;
	vid.support.arb_occlusion_query = true; // can't find a cap for this
	vid.support.arb_shadow = true;
	vid.support.arb_texture_compression = true;
	vid.support.arb_texture_cube_map = true;
	vid.support.arb_texture_non_power_of_two = (vid_d3d9caps.TextureCaps & D3DPTEXTURECAPS_POW2) == 0;
	vid.support.arb_vertex_buffer_object = true;
	vid.support.ext_blend_subtract = true;
	vid.support.ext_draw_range_elements = true;
	vid.support.ext_framebuffer_object = true;
	vid.support.ext_texture_3d = true;
	vid.support.ext_texture_compression_s3tc = true;
	vid.support.ext_texture_filter_anisotropic = true;
	vid.support.ati_separate_stencil = (vid_d3d9caps.StencilCaps & D3DSTENCILCAPS_TWOSIDED) != 0;
	vid.support.ext_texture_srgb = false; // FIXME use D3DSAMP_SRGBTEXTURE if CheckDeviceFormat agrees

	vid.maxtexturesize_2d = min(vid_d3d9caps.MaxTextureWidth, vid_d3d9caps.MaxTextureHeight);
	vid.maxtexturesize_3d = vid_d3d9caps.MaxVolumeExtent;
	vid.maxtexturesize_cubemap = vid.maxtexturesize_2d;
	vid.texunits = 4;
	vid.teximageunits = vid_d3d9caps.MaxSimultaneousTextures;
	vid.texarrayunits = 8; // can't find a caps field for this?
	vid.max_anisotropy = vid_d3d9caps.MaxAnisotropy;
	vid.maxdrawbuffers = vid_d3d9caps.NumSimultaneousRTs;

	vid.texunits = bound(4, vid.texunits, MAX_TEXTUREUNITS);
	vid.teximageunits = bound(16, vid.teximageunits, MAX_TEXTUREUNITS);
	vid.texarrayunits = bound(8, vid.texarrayunits, MAX_TEXTUREUNITS);
	Con_DPrintf("Using D3D9.0 rendering path - %i texture matrix, %i texture images, %i texcoords, shadowmapping supported%s\n", vid.texunits, vid.teximageunits, vid.texarrayunits, vid.maxdrawbuffers > 1 ? ", MRT detected (allows prepass deferred lighting)" : "");
	vid.renderpath = RENDERPATH_D3D9;
	vid.sRGBcapable2D = false;
	vid.sRGBcapable3D = true;
	vid.useinterleavedarrays = true;

	Cvar_SetQuick(&gl_info_vendor, gl_vendor);
	Cvar_SetQuick(&gl_info_renderer, gl_renderer);
	Cvar_SetQuick(&gl_info_version, gl_version);
	Cvar_SetQuick(&gl_info_platform, gl_platform ? gl_platform : "");
	Cvar_SetQuick(&gl_info_driver, gl_driver);

	// LordHavoc: report supported extensions
	Con_DPrintf("\nQuakeC extensions for server and client: %s\nQuakeC extensions for menu: %s\n", vm_sv_extensions, vm_m_extensions );

	// clear to black (loading plaque will be seen over this)
	IDirect3DDevice9_Clear(vid_d3d9dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	IDirect3DDevice9_BeginScene(vid_d3d9dev);
	IDirect3DDevice9_EndScene(vid_d3d9dev);
	IDirect3DDevice9_Present(vid_d3d9dev, NULL, NULL, NULL, NULL);
	// because the only time we end/begin scene is in VID_Finish, we'd better start a scene now...
	IDirect3DDevice9_BeginScene(vid_d3d9dev);
	vid_begunscene = true;

	//vid_menudrawfn = VID_MenuDraw;
	//vid_menukeyfn = VID_MenuKey;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
	vid_usingvsync = false;
	vid_hidden = vid_reallyhidden = false;
	vid_initialized = true;

	IN_StartupMouse ();

	return true;
}
#endif

qboolean VID_InitModeSOFT(viddef_mode_t *mode)
{
	int i;
	HDC hdc;
	RECT rect;
	MSG msg;
	int pixelformat, newpixelformat;
	DWORD WindowStyle, ExWindowStyle;
	int CenterX, CenterY;
	int depth;
	DEVMODE thismode;
	qboolean foundmode, foundgoodmode;
	int bpp = mode->bitsperpixel;
	int width = mode->width;
	int height = mode->height;
	int refreshrate = (int)floor(mode->refreshrate+0.5);
	int fullscreen = mode->fullscreen;

	if (vid_initialized)
		Sys_Error("VID_InitMode called when video is already initialised");

	memset(&gdevmode, 0, sizeof(gdevmode));

	vid_isfullscreen = false;
	if (fullscreen)
	{
		if(vid_forcerefreshrate.integer)
		{
			foundmode = true;
			gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			gdevmode.dmBitsPerPel = bpp;
			gdevmode.dmPelsWidth = width;
			gdevmode.dmPelsHeight = height;
			gdevmode.dmSize = sizeof (gdevmode);
			if(refreshrate)
			{
				gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
				gdevmode.dmDisplayFrequency = refreshrate;
			}
		}
		else
		{
			if(refreshrate == 0)
				refreshrate = initialdevmode.dmDisplayFrequency; // default vid_refreshrate to the rate of the desktop

			foundmode = false;
			foundgoodmode = false;

			thismode.dmSize = sizeof(thismode);
			thismode.dmDriverExtra = 0;
			for(i = 0; EnumDisplaySettings(NULL, i, &thismode); ++i)
			{
				if(~thismode.dmFields & (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY))
				{
					Con_DPrintf("enumerating modes yielded a bogus item... please debug this\n");
					continue;
				}
				if(developer_extra.integer)
					Con_DPrintf("Found mode %dx%dx%dbpp %dHz... ", (int)thismode.dmPelsWidth, (int)thismode.dmPelsHeight, (int)thismode.dmBitsPerPel, (int)thismode.dmDisplayFrequency);
				if(thismode.dmBitsPerPel != (DWORD)bpp)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong bpp\n");
					continue;
				}
				if(thismode.dmPelsWidth != (DWORD)width)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong width\n");
					continue;
				}
				if(thismode.dmPelsHeight != (DWORD)height)
				{
					if(developer_extra.integer)
						Con_DPrintf("wrong height\n");
					continue;
				}

				if(foundgoodmode)
				{
					// if we have a good mode, make sure this mode is better than the previous one, and allowed by the refreshrate
					if(thismode.dmDisplayFrequency > (DWORD)refreshrate)
					{
						if(developer_extra.integer)
							Con_DPrintf("too high refresh rate\n");
						continue;
					}
					else if(thismode.dmDisplayFrequency <= gdevmode.dmDisplayFrequency)
					{
						if(developer_extra.integer)
							Con_DPrintf("doesn't beat previous best match (too low)\n");
						continue;
					}
				}
				else if(foundmode)
				{
					// we do have one, but it isn't good... make sure it has a lower frequency than the previous one
					if(thismode.dmDisplayFrequency >= gdevmode.dmDisplayFrequency)
					{
						if(developer_extra.integer)
							Con_DPrintf("doesn't beat previous best match (too high)\n");
						continue;
					}
				}
				// otherwise, take anything

				memcpy(&gdevmode, &thismode, sizeof(gdevmode));
				if(thismode.dmDisplayFrequency <= (DWORD)refreshrate)
					foundgoodmode = true;
				else
				{
					if(developer_extra.integer)
						Con_DPrintf("(out of range)\n");
				}
				foundmode = true;
				if(developer_extra.integer)
					Con_DPrintf("accepted\n");
			}
		}

		if (!foundmode)
		{
			VID_Shutdown();
			Con_Printf("Unable to find the requested mode %dx%dx%dbpp\n", width, height, bpp);
			return false;
		}
		else if(ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
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

	pixelformat = 0;
	newpixelformat = 0;
	gl_extensions = "";
	gl_platformextensions = "";

	mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlacesWindowClass", gamename, WindowStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
	{
		Con_Printf("CreateWindowEx(%d, %s, %s, %d, %d, %d, %d, %d, %p, %p, %p, %p) failed\n", (int)ExWindowStyle, "DarkPlacesWindowClass", gamename, (int)WindowStyle, (int)(rect.left), (int)(rect.top), (int)(rect.right - rect.left), (int)(rect.bottom - rect.top), (void *)NULL, (void *)NULL, (void *)global_hInstance, (void *)NULL);
		VID_Shutdown();
		return false;
	}

	baseDC = GetDC(mainwindow);
	vid.softpixels = NULL;
	memset(&vid_softbmi, 0, sizeof(vid_softbmi));
	vid_softbmi.bmiHeader.biSize = sizeof(vid_softbmi.bmiHeader);
	vid_softbmi.bmiHeader.biWidth = width;
	vid_softbmi.bmiHeader.biHeight = -height; // negative to make a top-down bitmap
	vid_softbmi.bmiHeader.biPlanes = 1;
	vid_softbmi.bmiHeader.biBitCount = 32;
	vid_softbmi.bmiHeader.biCompression = BI_RGB;
	vid_softbmi.bmiHeader.biSizeImage = width*height*4;
	vid_softbmi.bmiHeader.biClrUsed = 256;
	vid_softbmi.bmiHeader.biClrImportant = 256;
	vid_softdibhandle = CreateDIBSection(baseDC, &vid_softbmi, DIB_RGB_COLORS, (void **)&vid.softpixels, NULL, 0);
	if (!vid_softdibhandle)
	{
		Con_Printf("CreateDIBSection failed\n");
		VID_Shutdown();
		return false;
	}

	vid_softhdc = CreateCompatibleDC(baseDC);
	vid_softhdc_backup = SelectObject(vid_softhdc, vid_softdibhandle);
	if (!vid_softhdc_backup)
	{
		Con_Printf("SelectObject failed\n");
		VID_Shutdown();
		return false;
	}
//	ReleaseDC(mainwindow, baseDC);
//	baseDC = NULL;

	vid.softdepthpixels = (unsigned int *)calloc(1, mode->width * mode->height * 4);
	if (DPSOFTRAST_Init(mode->width, mode->height, vid_soft_threads.integer, vid_soft_interlace.integer, (unsigned int *)vid.softpixels, (unsigned int *)vid.softdepthpixels) < 0)
	{
		Con_Printf("Failed to initialize software rasterizer\n");
		VID_Shutdown();
		return false;
	}

	VID_Soft_SharedSetup();

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

	//vid_menudrawfn = VID_MenuDraw;
	//vid_menukeyfn = VID_MenuKey;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
	vid_usingvsync = false;
	vid_reallyhidden = vid_hidden = false;
	vid_initialized = true;

	IN_StartupMouse ();

	return true;
}

qboolean VID_InitMode(viddef_mode_t *mode)
{
#ifdef SSE_POSSIBLE
	if (vid_soft.integer)
		return VID_InitModeSOFT(mode);
#endif
#ifdef SUPPORTD3D
//	if (vid_dx11.integer)
//		return VID_InitModeDX(mode, 11);
//	if (vid_dx10.integer)
//		return VID_InitModeDX(mode, 10);
	if (vid_dx9.integer)
		return VID_InitModeDX(mode, 9);
#endif
	return VID_InitModeGL(mode);
}


static void IN_Shutdown(void);
void VID_Shutdown (void)
{
	qboolean isgl;
	if(vid_initialized == false)
		return;

	VID_EnableJoystick(false);
	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

	vid_initialized = false;
	isgl = gldll != NULL;
	IN_Shutdown();
	gl_driver[0] = 0;
	gl_extensions = "";
	gl_platform = "";
	gl_platformextensions = "";
	if (vid_softhdc)
	{
		SelectObject(vid_softhdc, vid_softhdc_backup);
		ReleaseDC(mainwindow, vid_softhdc);
	}
	vid_softhdc = NULL;
	vid_softhdc_backup = NULL;
	if (vid_softdibhandle)
		DeleteObject(vid_softdibhandle);
	vid_softdibhandle = NULL;
	vid.softpixels = NULL;
	if (vid.softdepthpixels)
		free(vid.softdepthpixels);
	vid.softdepthpixels = NULL;
#ifdef SUPPORTD3D
	if (vid_d3d9dev)
	{
		if (vid_begunscene)
			IDirect3DDevice9_EndScene(vid_d3d9dev);
		vid_begunscene = false;
//		Cmd_ExecuteString("r_texturestats", src_command);
//		Cmd_ExecuteString("memlist", src_command);
		IDirect3DDevice9_Release(vid_d3d9dev);
	}
	vid_d3d9dev = NULL;
	if (vid_d3d9)
		IDirect3D9_Release(vid_d3d9);
	vid_d3d9 = NULL;
#endif
	if (qwglMakeCurrent)
		qwglMakeCurrent(NULL, NULL);
	qwglMakeCurrent = NULL;
	if (baseRC && qwglDeleteContext)
		qwglDeleteContext(baseRC);
	qwglDeleteContext = NULL;
	// close the library before we get rid of the window
	GL_CloseLibrary();
	if (baseDC && mainwindow)
		ReleaseDC(mainwindow, baseDC);
	baseDC = NULL;
	AppActivate(false, false);
	if (mainwindow)
		DestroyWindow(mainwindow);
	mainwindow = 0;
	if (vid_isfullscreen && isgl)
		ChangeDisplaySettings (NULL, 0);
	vid_isfullscreen = false;
}

void VID_SetMouse(qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
	static qboolean restore_spi;
	static int originalmouseparms[3];

	if (!mouseinitialized)
		return;

	if (relative)
	{
		if (!vid_usingmouse)
		{
			vid_usingmouse = true;
			cl_ignoremousemoves = 2;
#ifdef SUPPORTDIRECTX
			if (dinput && g_pMouse)
			{
				IDirectInputDevice_Acquire(g_pMouse);
				dinput_acquired = true;
			}
			else
#endif
			{
				RECT window_rect;
				window_rect.left = window_x;
				window_rect.top = window_y;
				window_rect.right = window_x + vid.width;
				window_rect.bottom = window_y + vid.height;

				// change mouse settings to turn off acceleration
// COMMANDLINEOPTION: Windows GDI Input: -noforcemparms disables setting of mouse parameters (not used with -dinput, windows only)
				if (!COM_CheckParm ("-noforcemparms") && SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0))
				{
					int newmouseparms[3];
					newmouseparms[0] = 0; // threshold to double movement (only if accel level is >= 1)
					newmouseparms[1] = 0; // threshold to quadruple movement (only if accel level is >= 2)
					newmouseparms[2] = 0; // maximum level of acceleration (0 = off)
					restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0) != FALSE;
				}
				else
					restore_spi = false;
				SetCursorPos ((window_x + vid.width / 2), (window_y + vid.height / 2));

				SetCapture (mainwindow);
				ClipCursor (&window_rect);
			}
		}
	}
	else
	{
		if (vid_usingmouse)
		{
			vid_usingmouse = false;
			cl_ignoremousemoves = 2;
#ifdef SUPPORTDIRECTX
			if (dinput_acquired)
			{
				IDirectInputDevice_Unacquire(g_pMouse);
				dinput_acquired = false;
			}
			else
#endif
			{
				// restore system mouseparms if we changed them
				if (restore_spi)
					SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);
				restore_spi = false;
				ClipCursor (NULL);
				ReleaseCapture ();
			}
		}
	}

	if (vid_usinghidecursor != hidecursor)
	{
		vid_usinghidecursor = hidecursor;
		ShowCursor (!hidecursor);
	}
}

void VID_BuildJoyState(vid_joystate_t *joystate)
{
	VID_Shared_BuildJoyState_Begin(joystate);
	VID_Shared_BuildJoyState_Finish(joystate);
}

void VID_EnableJoystick(qboolean enable)
{
	int index = joy_enable.integer > 0 ? joy_index.integer : -1;
	qboolean success = false;
	int sharedcount = 0;
	sharedcount = VID_Shared_SetJoystick(index);
	if (index >= 0 && index < sharedcount)
		success = true;

	// update cvar containing count of XInput joysticks
	if (joy_detected.integer != sharedcount)
		Cvar_SetValueQuick(&joy_detected, sharedcount);

	if (joy_active.integer != (success ? 1 : 0))
		Cvar_SetValueQuick(&joy_active, success ? 1 : 0);
}

#ifdef SUPPORTDIRECTX
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
		pDirectInputCreate = (HRESULT (__stdcall *)(HINSTANCE,DWORD,LPDIRECTINPUT *,LPUNKNOWN))GetProcAddress(hInstDI,"DirectInputCreateA");

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
#ifdef __cplusplus
	hr = IDirectInput_CreateDevice(g_pdi, GUID_SysMouse, &g_pMouse, NULL);
#else
	hr = IDirectInput_CreateDevice(g_pdi, &GUID_SysMouse, &g_pMouse, NULL);
#endif

	if (FAILED(hr))
	{
		Con_Print("Couldn't open DI mouse device\n");
		return false;
	}

// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat(g_pMouse, &c_dfDIMouse);

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
#endif


/*
===========
IN_StartupMouse
===========
*/
static void IN_StartupMouse (void)
{
	if (COM_CheckParm ("-nomouse"))
		return;

	mouseinitialized = true;

#ifdef SUPPORTDIRECTX
// COMMANDLINEOPTION: Windows Input: -dinput enables DirectInput for mouse input
	if (COM_CheckParm ("-dinput"))
		dinput = IN_InitDInput ();

	if (dinput)
		Con_Print("DirectInput initialized\n");
	else
		Con_Print("DirectInput not initialized\n");
#endif

	mouse_buttons = 10;
}


/*
===========
IN_MouseMove
===========
*/
static void IN_MouseMove (void)
{
	POINT current_pos;

	GetCursorPos (&current_pos);
	in_windowmouse_x = current_pos.x - window_x;
	in_windowmouse_y = current_pos.y - window_y;

	if (!vid_usingmouse)
		return;

#ifdef SUPPORTDIRECTX
	if (dinput_acquired)
	{
		int i;
		DIDEVICEOBJECTDATA	od;
		DWORD				dwElements;
		HRESULT				hr;

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

			if ((int)od.dwOfs == DIMOFS_X)
				in_mouse_x += (LONG) od.dwData;
			if ((int)od.dwOfs == DIMOFS_Y)
				in_mouse_y += (LONG) od.dwData;
			if ((int)od.dwOfs == DIMOFS_Z)
			{
				if((LONG)od.dwData < 0)
				{
					Key_Event(K_MWHEELDOWN, 0, true);
					Key_Event(K_MWHEELDOWN, 0, false);
				}
				else if((LONG)od.dwData > 0)
				{
					Key_Event(K_MWHEELUP, 0, true);
					Key_Event(K_MWHEELUP, 0, false);
				}
			}
			if ((int)od.dwOfs == DIMOFS_BUTTON0)
				mstate_di = (mstate_di & ~1) | ((od.dwData & 0x80) >> 7);
			if ((int)od.dwOfs == DIMOFS_BUTTON1)
				mstate_di = (mstate_di & ~2) | ((od.dwData & 0x80) >> 6);
			if ((int)od.dwOfs == DIMOFS_BUTTON2)
				mstate_di = (mstate_di & ~4) | ((od.dwData & 0x80) >> 5);
			if ((int)od.dwOfs == DIMOFS_BUTTON3)
				mstate_di = (mstate_di & ~8) | ((od.dwData & 0x80) >> 4);
		}

		// perform button actions
		for (i=0 ; i<mouse_buttons && i < 16 ; i++)
			if ((mstate_di ^ mouse_oldbuttonstate) & (1<<i))
				Key_Event (buttonremap[i], 0, (mstate_di & (1<<i)) != 0);
		mouse_oldbuttonstate = mstate_di;
	}
	else
#endif
	{
		in_mouse_x += in_windowmouse_x - (int)(vid.width / 2);
		in_mouse_y += in_windowmouse_y - (int)(vid.height / 2);

		// if the mouse has moved, force it to the center, so there's room to move
		if (in_mouse_x || in_mouse_y)
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
	vid_joystate_t joystate;
	if (vid_activewindow && !vid_reallyhidden)
		IN_MouseMove ();
	VID_EnableJoystick(true);
	VID_BuildJoyState(&joystate);
	VID_ApplyJoyState(&joystate);
}


static void IN_Init(void)
{
	uiWheelMessage = RegisterWindowMessage ( "MSWHEEL_ROLLMSG" );
	Cvar_RegisterVariable (&vid_forcerefreshrate);
}

static void IN_Shutdown(void)
{
#ifdef SUPPORTDIRECTX
	if (g_pMouse)
		IDirectInputDevice_Release(g_pMouse);
	g_pMouse = NULL;

	if (g_pdi)
		IDirectInput_Release(g_pdi);
	g_pdi = NULL;
#endif
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	int i;
	size_t k;
	DEVMODE thismode;

	thismode.dmSize = sizeof(thismode);
	thismode.dmDriverExtra = 0;
	k = 0;
	for(i = 0; EnumDisplaySettings(NULL, i, &thismode); ++i)
	{
		if(~thismode.dmFields & (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY))
		{
			Con_DPrintf("enumerating modes yielded a bogus item... please debug this\n");
			continue;
		}
		if(k >= maxcount)
			break;
		modes[k].width = thismode.dmPelsWidth;
		modes[k].height = thismode.dmPelsHeight;
		modes[k].bpp = thismode.dmBitsPerPel;
		modes[k].refreshrate = thismode.dmDisplayFrequency;
		modes[k].pixelheight_num = 1;
		modes[k].pixelheight_denom = 1; // Win32 apparently does not provide this (FIXME)
		++k;
	}
	return k;
}
