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
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define MAXWIDTH		10000
#define MAXHEIGHT		10000

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean		DDActive;
qboolean		scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	*pcurrentmode;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int		usingmouse;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow;

int			vid_modenum = NO_MODE;
int			vid_realmode;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;
unsigned char	vid_curpal[256*3];

HGLRC	baseRC;
HDC		maindc;

glvert_t glv;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);

//====================================

// Note that 0 is MODE_WINDOWED
//cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
//cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

// direct draw software compatability stuff

void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean VID_SetWindowedMode (int modenum)
{
	int				lastmodestate, width, height;
	RECT			rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, false, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlaces", "DarkPlacesGL", WindowStyle, rect.left, rect.top, width, height, NULL, NULL, global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(mainwindow, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, false);

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	modestate = MS_WINDOWED;

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)true, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)false, (LPARAM)hIcon);

	return true;
}


qboolean VID_SetFullDIBMode (int modenum)
{
	int				lastmodestate, width, height;
	RECT			rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width <<
							   modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode");
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, false, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindowEx (ExWindowStyle, "DarkPlaces", "DarkPlacesGL", WindowStyle, rect.left, rect.top, width, height, NULL, NULL, global_hInstance, NULL);

	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)true, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)false, (LPARAM)hIcon);

	return true;
}


int VID_SetMode (int modenum)
{
	int				original_mode, temp;
	qboolean		stat;
    MSG				msg;

	if ((windowed && (modenum != 0)) || (!windowed && (modenum < 1)) || (!windowed && (modenum >= nummodes)))
		Sys_Error ("Bad video mode\n");

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
//		if (vid_mouse.value && key_dest == key_game)
//		{
//			stat = VID_SetWindowedMode(modenum);
//			usingmouse = true;
//			IN_ActivateMouse ();
//			IN_HideMouse ();
//		}
//		else
//		{
//			usingmouse = false;
//			IN_DeactivateMouse ();
//			IN_ShowMouse ();
//			stat = VID_SetWindowedMode(modenum);
//		}
		stat = VID_SetWindowedMode(modenum);
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
//		usingmouse = true;
//		IN_ActivateMouse ();
//		IN_HideMouse ();
	}
	else
		Sys_Error ("VID_SetMode: Bad mode type in modelist");

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
		Sys_Error ("Couldn't set video mode");

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

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

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized.\n", VID_GetModeDescription (vid_modenum));

	vid.recalc_refdef = 1;

	return true;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================

void VID_CheckMultitexture(void) 
{
	qglMTexCoord2f = NULL;
	qglSelectTexture = NULL;
	gl_mtexable = false;
	// Check to see if multitexture is disabled
	if (COM_CheckParm("-nomtex"))
	{
		Con_Printf("...multitexture disabled\n");
		return;
	}
	// Test for ARB_multitexture
	if (!COM_CheckParm("-SGISmtex") && strstr(gl_extensions, "GL_ARB_multitexture "))
	{
		Con_Printf("...using GL_ARB_multitexture\n");
		qglMTexCoord2f = (void *) wglGetProcAddress("glMultiTexCoord2fARB");
		qglSelectTexture = (void *) wglGetProcAddress("glActiveTextureARB");
		gl_mtexable = true;
		gl_mtex_enum = GL_TEXTURE0_ARB;
	}
	else if (strstr(gl_extensions, "GL_SGIS_multitexture ")) // Test for SGIS_multitexture (if ARB_multitexture not found)
	{
		Con_Printf("...using GL_SGIS_multitexture\n");
		qglMTexCoord2f = (void *) wglGetProcAddress("glMTexCoord2fSGIS");
		qglSelectTexture = (void *) wglGetProcAddress("glSelectTextureSGIS");
		gl_mtexable = true;
		gl_mtex_enum = TEXTURE0_SGIS;
	}
	else
		Con_Printf("...multitexture disabled (not detected)\n");
}

void VID_CheckCVA(void)
{
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	gl_supportslockarrays = false;
	if (COM_CheckParm("-nocva"))
	{
		Con_Printf("...compiled vertex arrays disabled\n");
		return;
	}
	if (strstr(gl_extensions, "GL_EXT_compiled_vertex_array"))
	{
		Con_Printf("...using compiled vertex arrays\n");
		qglLockArraysEXT = (void *) wglGetProcAddress("glLockArraysEXT");
		qglUnlockArraysEXT = (void *) wglGetProcAddress("glUnlockArraysEXT");
		gl_supportslockarrays = true;
	}
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;

//	if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	int usemouse;
	if (r_render.value && !scr_skipupdate)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	usemouse = false;
	if (vid_mouse.value && key_dest == key_game)
		usemouse = true;
	if (modestate == MS_FULLDIB)
		usemouse = true;
	if (!ActiveApp)
		usemouse = false;
	if (usemouse)
	{
		if (!usingmouse)
		{
			usingmouse = true;
			IN_ActivateMouse ();
			IN_HideMouse();
		}
	}
	else
	{
		if (usingmouse)
		{
			usingmouse = false;
			IN_DeactivateMouse ();
			IN_ShowMouse();
		}
	}
}

void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}

void VID_RestoreSystemGamma();

void	VID_Shutdown (void)
{
   	HGLRC hRC;
   	HDC	  hDC;
	int i;
	GLuint temp[8192];

	if (vid_initialized)
	{
		vid_canalttab = false;
		hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

    	wglMakeCurrent(NULL, NULL);

		// LordHavoc: free textures before closing (may help NVIDIA)
		for (i = 0;i < 8192;i++)
			temp[i] = i+1;
		glDeleteTextures(8192, temp);

    	if (hRC)
    	    wglDeleteContext(hRC);

		if (hDC && mainwindow)
			ReleaseDC(mainwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);

		AppActivate(false, false);

		VID_RestoreSystemGamma();
	}
}


//==========================================================================


BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
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

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return false;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == false)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return false;
    }

    return true;
}



byte scantokey[128] =
{
//	0           1      2     3     4     5       6       7      8         9      A       B           C     D            E           F
	0          ,27    ,'1'  ,'2'  ,'3'  ,'4'    ,'5'    ,'6'   ,'7'      ,'8'   ,'9'    ,'0'        ,'-'  ,'='         ,K_BACKSPACE,9     , // 0
	'q'        ,'w'   ,'e'  ,'r'  ,'t'  ,'y'    ,'u'    ,'i'   ,'o'      ,'p'   ,'['    ,']'        ,13   ,K_CTRL      ,'a'        ,'s'   , // 1
	'd'        ,'f'   ,'g'  ,'h'  ,'j'  ,'k'    ,'l'    ,';'   ,'\''     ,'`'   ,K_SHIFT,'\\'       ,'z'  ,'x'         ,'c'        ,'v'   , // 2
	'b'        ,'n'   ,'m'  ,','  ,'.'  ,'/'    ,K_SHIFT,'*'   ,K_ALT    ,' '   ,0      ,K_F1       ,K_F2 ,K_F3        ,K_F4       ,K_F5  , // 3
	K_F6       ,K_F7  ,K_F8 ,K_F9 ,K_F10,K_PAUSE,0      ,K_HOME,K_UPARROW,K_PGUP,'-'    ,K_LEFTARROW,'5'  ,K_RIGHTARROW,'+'        ,K_END , // 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0    ,0      ,0      ,K_F11 ,K_F12    ,0     ,0      ,0          ,0    ,0           ,0          ,0     , // 5
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0      ,0          ,0    ,0           ,0          ,0     , // 6
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0      ,0          ,0    ,0           ,0          ,0       // 7
};

/*
byte shiftscantokey[128] =
{ 
//	0           1      2     3     4     5       6       7      8         9      A       B           C    D            E           F 
	0          ,27    ,'!'  ,'@'  ,'#'  ,'$'    ,'%'    ,'^'   ,'&'      ,'*'   ,'('    ,')'        ,'_' ,'+'         ,K_BACKSPACE,9    , // 0
	'Q'        ,'W'   ,'E'  ,'R'  ,'T'  ,'Y'    ,'U'    ,'I'   ,'O'      ,'P'   ,'{'    ,'}'        ,13  ,K_CTRL      ,'A'        ,'S'  , // 1
	'D'        ,'F'   ,'G'  ,'H'  ,'J'  ,'K'    ,'L'    ,':'   ,'"'      ,'~'   ,K_SHIFT,'|'        ,'Z' ,'X'         ,'C'        ,'V'  , // 2
	'B'        ,'N'   ,'M'  ,'<'  ,'>'  ,'?'    ,K_SHIFT,'*'   ,K_ALT    ,' '   ,0      ,K_F1       ,K_F2,K_F3        ,K_F4       ,K_F5 , // 3
	K_F6       ,K_F7  ,K_F8 ,K_F9 ,K_F10,K_PAUSE,0      ,K_HOME,K_UPARROW,K_PGUP,'_'    ,K_LEFTARROW,'%' ,K_RIGHTARROW,'+'        ,K_END, // 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0    ,0      ,0      ,K_F11 ,K_F12    ,0     ,0      ,0          ,0   ,0           ,0          ,0    , // 5
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0      ,0          ,0   ,0           ,0          ,0    , // 6
	0          ,0     ,0    ,0    ,0    ,0      ,0      ,0     ,0        ,0     ,0      ,0          ,0   ,0           ,0          ,0      // 7 
}; 
*/

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key, int virtualkey)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
//	if (scantokey[key] >= 0x20 && scantokey[key] < 0x7F)
//		return realchar;
	return scantokey[key];
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
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void VID_RestoreGameGamma();

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

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
//			usingmouse = true;
//			IN_ActivateMouse ();
//			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
			}

			// LordHavoc: from dabb, fix for alt-tab bug in NVidia drivers
			MoveWindow(mainwindow,0,0,gdevmode.dmPelsWidth,gdevmode.dmPelsHeight,false);
		}
//		else if ((modestate == MS_WINDOWED) && vid_mouse.value && key_dest == key_game)
//		{
//			usingmouse = true;
//			IN_ActivateMouse ();
//			IN_HideMouse ();
//		}
		VID_RestoreGameGamma();
	}

	if (!fActive)
	{
		usingmouse = false;
		IN_DeactivateMouse ();
		IN_ShowMouse ();
		if (modestate == MS_FULLDIB)
		{
//			usingmouse = false;
//			IN_DeactivateMouse ();
//			IN_ShowMouse ();
			if (vid_canalttab)
			{ 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
//		else if ((modestate == MS_WINDOWED) && vid_mouse.value)
//		{
//			usingmouse = false;
//			IN_DeactivateMouse ();
//			IN_ShowMouse ();
//		}
		VID_RestoreSystemGamma();
	}
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

    switch (uMsg)
    {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey(lParam, wParam), true);
			break;
			
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey(lParam, wParam), false);
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
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
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

   	    case WM_DESTROY:
        {
			if (mainwindow)
				DestroyWindow (mainwindow);

            PostQuitMessage (0);
        }
        break;

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


/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}

	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		else
			sprintf (pinfo, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		t, modenum;
	
	modenum = atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	vmode_t		*pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

void VID_AddMode(int type, int width, int height, int modenum, int halfscreen, int dib, int fullscreen, int bpp)
{
	int i;
	if (nummodes >= MAX_MODE_LIST)
		return;
	modelist[nummodes].type = type;
	modelist[nummodes].width = width;
	modelist[nummodes].height = height;
	modelist[nummodes].modenum = modenum;
	modelist[nummodes].halfscreen = halfscreen;
	modelist[nummodes].dib = dib;
	modelist[nummodes].fullscreen = fullscreen;
	modelist[nummodes].bpp = bpp;
	if (bpp == 0)
		sprintf (modelist[nummodes].modedesc, "%dx%d", width, height);
	else
		sprintf (modelist[nummodes].modedesc, "%dx%dx%d", width, height, bpp);
	for (i = 0;i < nummodes;i++)
	{
		if (!memcmp(&modelist[i], &modelist[nummodes], sizeof(vmode_t)))
			return;
	}
	nummodes++;
}

void VID_InitDIB (HINSTANCE hInstance)
{
	int w, h;
	WNDCLASS		wc;

	// Register the frame class
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "DarkPlaces";

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	/*
	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
	*/
	if (COM_CheckParm("-width"))
		w = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		w = 640;

	if (w < 320)
		w = 320;

	if (COM_CheckParm("-height"))
		h = atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		h = w * 240/320;

	if (h < 240)
		h = 240;

	VID_AddMode(MS_WINDOWED, w, h, 0, 0, 1, 0, 0);
}


/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
//	int		i;
	int		modenum;
	int		originalnummodes;
//	int		existingmode;
	int		numlowresmodes;
	int		j;
	int		bpp;
	int		done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) && (devmode.dmPelsWidth <= MAXWIDTH) && (devmode.dmPelsHeight <= MAXHEIGHT) && (nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if ((!COM_CheckParm("-noadjustaspect")) && (devmode.dmPelsWidth > (devmode.dmPelsHeight << 1)))
					VID_AddMode(MS_FULLDIB, devmode.dmPelsWidth >> 1, devmode.dmPelsHeight, 0, 1, 1, 1, devmode.dmBitsPerPel);
				else
					VID_AddMode(MS_FULLDIB, devmode.dmPelsWidth, devmode.dmPelsHeight, 0, 0, 1, 1, devmode.dmBitsPerPel);
				/*
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d", modelist[nummodes].width, modelist[nummodes].height, modelist[nummodes].bpp);
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) && (modelist[nummodes].height == modelist[i].height) && (modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
				*/
			}
		}

		modenum++;
	}
	while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do
	{
		for (j=0 ; (j<numlowresmodes) && (nummodes < MAX_MODE_LIST) ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
				VID_AddMode(MS_FULLDIB, devmode.dmPelsWidth, devmode.dmPelsHeight, 0, 0, 1, 1, devmode.dmBitsPerPel);
			/*
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width) && (modelist[nummodes].height == modelist[i].height) && (modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
			}
			*/
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	}
	while (!done);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

static int grabsysgamma = true;
WORD systemgammaramps[3][256], currentgammaramps[3][256];

int VID_SetGamma(float prescale, float gamma, float scale, float base)
{
	int i;
	HDC hdc;
	hdc = GetDC (NULL);

	BuildGammaTable16(prescale, gamma, scale, base, &currentgammaramps[0][0]);
	for (i = 0;i < 256;i++)
		currentgammaramps[1][i] = currentgammaramps[2][i] = currentgammaramps[0][i];

	i = SetDeviceGammaRamp(hdc, &currentgammaramps[0][0]);

	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

void VID_RestoreGameGamma()
{
	VID_UpdateGamma(true);
}

void VID_GetSystemGamma()
{
	HDC hdc;
	hdc = GetDC (NULL);

	GetDeviceGammaRamp(hdc, &systemgammaramps[0][0]);

	ReleaseDC (NULL, hdc);
}

void VID_RestoreSystemGamma()
{
	HDC hdc;
	hdc = GetDC (NULL);

	SetDeviceGammaRamp(hdc, &systemgammaramps[0][0]);

	ReleaseDC (NULL, hdc);
}

/*
===================
VID_Init
===================
*/
void	VID_Init ()
{
	int		i;
//	int		existingmode;
	int		basenummodes, width, height, bpp, findbpp, done;
	HDC		hdc;
	DEVMODE	devmode;

	memset(&devmode, 0, sizeof(devmode));

//	Cvar_RegisterVariable (&_vid_default_mode);
//	Cvar_RegisterVariable (&_vid_default_mode_win);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	VID_GetSystemGamma();

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	InitCommonControls();

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error ("Can't run in non-RGB mode");

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
			vid_default = atoi(com_argv[COM_CheckParm("-mode")+1]);
		else
		{
			if (COM_CheckParm("-current"))
			{
				modelist[MODE_FULLSCREEN_DEFAULT].width = GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height = GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
					width = atoi(com_argv[COM_CheckParm("-width")+1]);
				else
					width = 640;

				if (COM_CheckParm("-bpp"))
				{
					bpp = atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = 0;
				}
				else
				{
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm("-height"))
					height = atoi(com_argv[COM_CheckParm("-height")+1]);

			// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
					VID_AddMode(MS_FULLDIB, width, height, 0, 0, 1, 1, bpp);
				/*
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width) && (modelist[nummodes].height == modelist[i].height) && (modelist[nummodes].bpp == modelist[i].bpp))
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
						nummodes++;
				}
				*/

				done = 0;

				do
				{
					if (COM_CheckParm("-height"))
					{
						height = atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].height == height) && (modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15: bpp = 16;break;
							case 16: bpp = 32;break;
							case 32: bpp = 24;break;
							case 24: done = 1;break;
							}
						}
						else
							done = 1;
					}
				}
				while (!done);

				if (!vid_default)
					Sys_Error ("Specified video mode not available");
			}
		}
	}

	vid_initialized = true;

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	VID_SetMode (vid_default);

	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you are in 65536 color mode, and try running -window.");
	if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	// LordHavoc: special differences for ATI (broken 8bit color when also using 32bit? weird!)
	if (strncasecmp(gl_vendor,"ATI",3)==0)
	{
		if (strncasecmp(gl_renderer,"Rage Pro",8)==0)
			isRagePro = true;
	}
	if (strncasecmp(gl_renderer,"Matrox G200 Direct3D",20)==0) // a D3D driver for GL? sigh...
		isG200 = true;

//	sprintf (gldir, "%s/glquake", com_gamedir);
//	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
	char		*ptr;
	int			lnummodes, i, k, column, row;
	vmode_t		*pv;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();
	
	for (i=1 ; (i<lnummodes) && (vid_wmodes < MAX_MODEDESCS) ; i++)
	{
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0)
	{
		M_Print (2*8, 36+0*8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");

		column = 8;
		row = 36+2*8;

		for (i=0 ; i<vid_wmodes ; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*2,
			 "Video modes must be set from the");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3,
			 "command line with -width <width>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4,
			 "and -bpp <bits-per-pixel>");
	M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6,
			 "Select windowed mode with -window");
}


/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}
