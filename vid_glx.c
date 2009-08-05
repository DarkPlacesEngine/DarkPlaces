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

#include <signal.h>

#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h> // TODO possibly ifdef this out on non-supporting systems... Solaris (as always)?
#include <GL/glx.h>

#include "quakedef.h"

#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>

#include <X11/extensions/XShm.h>
#if !defined(__APPLE__) && !defined(__MACH__) && !defined(SUNOS)
#include <X11/extensions/xf86dga.h>
#endif
#include <X11/extensions/xf86vmode.h>

#include "nexuiz.xpm"
#include "darkplaces.xpm"

// Tell startup code that we have a client
int cl_available = true;

// note: if we used the XRandR extension we could support refresh rates
qboolean vid_supportrefreshrate = false;

//GLX prototypes
XVisualInfo *(GLAPIENTRY *qglXChooseVisual)(Display *dpy, int screen, int *attribList);
GLXContext (GLAPIENTRY *qglXCreateContext)(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
void (GLAPIENTRY *qglXDestroyContext)(Display *dpy, GLXContext ctx);
Bool (GLAPIENTRY *qglXMakeCurrent)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
void (GLAPIENTRY *qglXSwapBuffers)(Display *dpy, GLXDrawable drawable);
const char *(GLAPIENTRY *qglXQueryExtensionsString)(Display *dpy, int screen);

//GLX_ARB_get_proc_address
void *(GLAPIENTRY *qglXGetProcAddressARB)(const GLubyte *procName);

static dllfunction_t getprocaddressfuncs[] =
{
	{"glXGetProcAddressARB", (void **) &qglXGetProcAddressARB},
	{NULL, NULL}
};

//GLX_SGI_swap_control
GLint (GLAPIENTRY *qglXSwapIntervalSGI)(GLint interval);

static dllfunction_t swapcontrolfuncs[] =
{
	{"glXSwapIntervalSGI", (void **) &qglXSwapIntervalSGI},
	{NULL, NULL}
};

static Display *vidx11_display = NULL;
static int vidx11_screen;
static Window win, root;
static GLXContext ctx = NULL;

Atom wm_delete_window_atom;
Atom net_wm_state_atom;
Atom net_wm_state_hidden_atom;
Atom net_wm_state_fullscreen_atom;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask)
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | \
		StructureNotifyMask | FocusChangeMask | EnterWindowMask | \
		LeaveWindowMask)


static qboolean mouse_avail = true;
static qboolean vid_usingmousegrab = false;
static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_usingvsync = false;
static qboolean vid_usevsync = false;
static qboolean vid_x11_hardwaregammasupported = false;
static qboolean vid_x11_dgasupported = false;
static int vid_x11_gammarampsize = 0;

#if !defined(__APPLE__) && !defined(SUNOS)
cvar_t vid_dgamouse = {CVAR_SAVE, "vid_dgamouse", "0", "make use of DGA mouse input"};
static qboolean vid_usingdgamouse = false;
#endif
cvar_t vid_netwmfullscreen = {CVAR_SAVE, "vid_netwmfullscreen", "0", "make use _NET_WM_STATE_FULLSCREEN; turn this off if fullscreen does not work for you"};

qboolean vidmode_ext = false;

static int win_x, win_y;

static XF86VidModeModeInfo init_vidmode, game_vidmode;
static qboolean vid_isfullscreen = false;
static qboolean vid_isvidmodefullscreen = false;
static qboolean vid_isnetwmfullscreen = false;
static qboolean vid_isoverrideredirect = false;

static Visual *vidx11_visual;
static Colormap vidx11_colormap;

/*-----------------------------------------------------------------------*/

static int XLateKey(XKeyEvent *ev, char *ascii)
{
	int key = 0;
	char buf[64];
	KeySym keysym, shifted;

	keysym = XLookupKeysym (ev, 0);
	XLookupString(ev, buf, sizeof buf, &shifted, 0);
	*ascii = buf[0];

	switch(keysym)
	{
		case XK_KP_Page_Up:	 key = K_KP_PGUP; break;
		case XK_Page_Up:	 key = K_PGUP; break;

		case XK_KP_Page_Down: key = K_KP_PGDN; break;
		case XK_Page_Down:	 key = K_PGDN; break;

		case XK_KP_Home: key = K_KP_HOME; break;
		case XK_Home:	 key = K_HOME; break;

		case XK_KP_End:  key = K_KP_END; break;
		case XK_End:	 key = K_END; break;

		case XK_KP_Left: key = K_KP_LEFTARROW; break;
		case XK_Left:	 key = K_LEFTARROW; break;

		case XK_KP_Right: key = K_KP_RIGHTARROW; break;
		case XK_Right:	key = K_RIGHTARROW;		break;

		case XK_KP_Down: key = K_KP_DOWNARROW; break;
		case XK_Down:	 key = K_DOWNARROW; break;

		case XK_KP_Up:   key = K_KP_UPARROW; break;
		case XK_Up:		 key = K_UPARROW;	 break;

		case XK_Escape: key = K_ESCAPE;		break;

		case XK_KP_Enter: key = K_KP_ENTER;	break;
		case XK_Return: key = K_ENTER;		 break;

		case XK_Tab:		key = K_TAB;			 break;

		case XK_F1:		 key = K_F1;				break;

		case XK_F2:		 key = K_F2;				break;

		case XK_F3:		 key = K_F3;				break;

		case XK_F4:		 key = K_F4;				break;

		case XK_F5:		 key = K_F5;				break;

		case XK_F6:		 key = K_F6;				break;

		case XK_F7:		 key = K_F7;				break;

		case XK_F8:		 key = K_F8;				break;

		case XK_F9:		 key = K_F9;				break;

		case XK_F10:		key = K_F10;			 break;

		case XK_F11:		key = K_F11;			 break;

		case XK_F12:		key = K_F12;			 break;

		case XK_BackSpace: key = K_BACKSPACE; break;

		case XK_KP_Delete: key = K_KP_DEL; break;
		case XK_Delete: key = K_DEL; break;

		case XK_Pause:	key = K_PAUSE;		 break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT;		break;

		case XK_Execute:
		case XK_Control_L:
		case XK_Control_R:	key = K_CTRL;		 break;

		case XK_Alt_L:
		case XK_Meta_L:
		case XK_ISO_Level3_Shift:
		case XK_Alt_R:
		case XK_Meta_R: key = K_ALT;			break;

		case XK_KP_Begin: key = K_KP_5;	break;

		case XK_Insert:key = K_INS; break;
		case XK_KP_Insert: key = K_KP_INS; break;

		case XK_KP_Multiply: key = K_KP_MULTIPLY; break;
		case XK_KP_Add:  key = K_KP_PLUS; break;
		case XK_KP_Subtract: key = K_KP_MINUS; break;
		case XK_KP_Divide: key = K_KP_SLASH; break;

		case XK_section:	key = '~'; break;

		default:
			if (keysym < 32)
				break;

			if (keysym >= 'A' && keysym <= 'Z')
				key = keysym - 'A' + 'a';
			else
				key = keysym;

			break;
	}

	return key;
}

static Cursor CreateNullCursor(Display *display, Window root)
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(display, root, 1, 1, 1);
	xgc.function = GXclear;
	gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask, &dummycolour,&dummycolour, 0,0);
	XFreePixmap(display,cursormask);
	XFreeGC(display,gc);
	return cursor;
}

void VID_SetMouse(qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
	static int originalmouseparms_num;
	static int originalmouseparms_denom;
	static int originalmouseparms_threshold;
	static qboolean restore_spi;

#if !defined(__APPLE__) && !defined(SUNOS)
	qboolean usedgamouse;
#endif

	if (!vidx11_display || !win)
		return;

	if (relative)
		fullscreengrab = true;

	if (!mouse_avail)
		fullscreengrab = relative = hidecursor = false;

#if !defined(__APPLE__) && !defined(SUNOS)
	usedgamouse = relative && vid_dgamouse.integer;
	if (!vid_x11_dgasupported)
		usedgamouse = false;
	if (fullscreengrab && vid_usingmouse && (vid_usingdgamouse != usedgamouse))
		VID_SetMouse(false, false, false); // ungrab first!
#endif

	if (vid_usingmousegrab != fullscreengrab)
	{
		vid_usingmousegrab = fullscreengrab;
		cl_ignoremousemoves = 2;
		if (fullscreengrab)
		{
			XGrabPointer(vidx11_display, win,  True, 0, GrabModeAsync, GrabModeAsync, win, None, CurrentTime);
			if (vid_grabkeyboard.integer || vid_isoverrideredirect)
				XGrabKeyboard(vidx11_display, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
		}
		else
		{
			XUngrabPointer(vidx11_display, CurrentTime);
			XUngrabKeyboard(vidx11_display, CurrentTime);
		}
	}

	if (relative)
	{
		if (!vid_usingmouse)
		{
			XWindowAttributes attribs_1;
			XSetWindowAttributes attribs_2;

			XGetWindowAttributes(vidx11_display, win, &attribs_1);
			attribs_2.event_mask = attribs_1.your_event_mask | KEY_MASK | MOUSE_MASK;
			XChangeWindowAttributes(vidx11_display, win, CWEventMask, &attribs_2);

#if !defined(__APPLE__) && !defined(SUNOS)
			vid_usingdgamouse = usedgamouse;
			if (usedgamouse)
			{
				XF86DGADirectVideo(vidx11_display, DefaultScreen(vidx11_display), XF86DGADirectMouse);
				XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, 0, 0);
			}
			else
#endif
				XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, vid.width / 2, vid.height / 2);

// COMMANDLINEOPTION: X11 Input: -noforcemparms disables setting of mouse parameters (not used with DGA, windows only)
#if !defined(__APPLE__) && !defined(SUNOS)
			if (!COM_CheckParm ("-noforcemparms") && !usedgamouse)
#else
			if (!COM_CheckParm ("-noforcemparms"))
#endif
			{
				XGetPointerControl(vidx11_display, &originalmouseparms_num, &originalmouseparms_denom, &originalmouseparms_threshold);
				XChangePointerControl (vidx11_display, true, false, 1, 1, -1); // TODO maybe change threshold here, or remove this comment
				restore_spi = true;
			}
			else
				restore_spi = false;

			cl_ignoremousemoves = 2;
			vid_usingmouse = true;
		}
	}
	else
	{
		if (vid_usingmouse)
		{
#if !defined(__APPLE__) && !defined(SUNOS)
			if (vid_usingdgamouse)
				XF86DGADirectVideo(vidx11_display, DefaultScreen(vidx11_display), 0);
			vid_usingdgamouse = false;
#endif
			cl_ignoremousemoves = 2;

			if (restore_spi)
				XChangePointerControl (vidx11_display, true, true, originalmouseparms_num, originalmouseparms_denom, originalmouseparms_threshold);
			restore_spi = false;

			vid_usingmouse = false;
		}
	}

	if (vid_usinghidecursor != hidecursor)
	{
		vid_usinghidecursor = hidecursor;
		if (hidecursor)
			XDefineCursor(vidx11_display, win, CreateNullCursor(vidx11_display, win));
		else
			XUndefineCursor(vidx11_display, win);
	}
}

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

static void HandleEvents(void)
{
	XEvent event;
	int key;
	char ascii;
	qboolean dowarp = false;

	if (!vidx11_display)
		return;

	while (XPending(vidx11_display))
	{
		XNextEvent(vidx11_display, &event);

		switch (event.type)
		{
		case KeyPress:
			// key pressed
			key = XLateKey (&event.xkey, &ascii);
			Key_Event(key, ascii, true);
			break;

		case KeyRelease:
			// key released
			key = XLateKey (&event.xkey, &ascii);
			Key_Event(key, ascii, false);
			break;

		case MotionNotify:
			// mouse moved
			if (vid_usingmouse)
			{
#if !defined(__APPLE__) && !defined(SUNOS)
				if (vid_usingdgamouse)
				{
					in_mouse_x += event.xmotion.x_root;
					in_mouse_y += event.xmotion.y_root;
				}
				else
#endif
				{
					if (!event.xmotion.send_event)
					{
						in_mouse_x += event.xmotion.x - in_windowmouse_x;
						in_mouse_y += event.xmotion.y - in_windowmouse_y;
						//if (abs(vid.width/2 - event.xmotion.x) + abs(vid.height/2 - event.xmotion.y))
						if (vid_stick_mouse.integer || abs(vid.width/2 - event.xmotion.x) > vid.width / 4 || abs(vid.height/2 - event.xmotion.y) > vid.height / 4)
							dowarp = true;
					}
				}
			}
			in_windowmouse_x = event.xmotion.x;
			in_windowmouse_y = event.xmotion.y;
			break;

		case ButtonPress:
			// mouse button pressed
			if (event.xbutton.button <= 18)
				Key_Event(buttonremap[event.xbutton.button - 1], 0, true);
			else
				Con_Printf("HandleEvents: ButtonPress gave value %d, 1-18 expected\n", event.xbutton.button);
			break;

		case ButtonRelease:
			// mouse button released
			if (event.xbutton.button <= 18)
				Key_Event(buttonremap[event.xbutton.button - 1], 0, false);
			else
				Con_Printf("HandleEvents: ButtonRelease gave value %d, 1-18 expected\n", event.xbutton.button);
			break;

		case CreateNotify:
			// window created
			win_x = event.xcreatewindow.x;
			win_y = event.xcreatewindow.y;
			break;

		case ConfigureNotify:
			// window changed size/location
			win_x = event.xconfigure.x;
			win_y = event.xconfigure.y;
			if(vid_resizable.integer < 2 || vid_isnetwmfullscreen)
			{
				vid.width = event.xconfigure.width;
				vid.height = event.xconfigure.height;
				if(vid_isnetwmfullscreen)
					Con_Printf("NetWM fullscreen: actually using resolution %dx%d\n", vid.width, vid.height);
				else
					Con_DPrintf("Updating to ConfigureNotify resolution %dx%d\n", vid.width, vid.height);
			}
			break;
		case DestroyNotify:
			// window has been destroyed
			Sys_Quit(0);
			break;
		case ClientMessage:
			// window manager messages
			if ((event.xclient.format == 32) && ((unsigned int)event.xclient.data.l[0] == wm_delete_window_atom))
				Sys_Quit(0);
			break;
		case MapNotify:
			if (vid_isoverrideredirect)
				break;
			// window restored
			vid_hidden = false;
			VID_RestoreSystemGamma();

			if(vid_isvidmodefullscreen)
			{
				// set our video mode
				XF86VidModeSwitchToMode(vidx11_display, vidx11_screen, &game_vidmode);

				// Move the viewport to top left
				XF86VidModeSetViewPort(vidx11_display, vidx11_screen, 0, 0);
			}

			if(vid_isnetwmfullscreen)
			{
				// make sure it's fullscreen
				XEvent event;
				event.type = ClientMessage;
				event.xclient.serial = 0;
				event.xclient.send_event = True;
				event.xclient.message_type = net_wm_state_atom;
				event.xclient.window = win;
				event.xclient.format = 32;
				event.xclient.data.l[0] = 1;
				event.xclient.data.l[1] = net_wm_state_fullscreen_atom;
				event.xclient.data.l[2] = 0;
				event.xclient.data.l[3] = 1;
				event.xclient.data.l[4] = 0;
				XSendEvent(vidx11_display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
			}

			dowarp = true;

			break;
		case UnmapNotify:
			if (vid_isoverrideredirect)
				break;
			// window iconified/rolledup/whatever
			vid_hidden = true;
			VID_RestoreSystemGamma();

			if(vid_isvidmodefullscreen)
				XF86VidModeSwitchToMode(vidx11_display, vidx11_screen, &init_vidmode);

			break;
		case FocusIn:
			if (vid_isoverrideredirect)
				break;
			// window is now the input focus
			vid_activewindow = true;
			break;
		case FocusOut:
			if (vid_isoverrideredirect)
				break;

			if(vid_isnetwmfullscreen && event.xfocus.mode == NotifyNormal)
			{
				// iconify netwm fullscreen window when it loses focus
				// when the user selects it in the taskbar, the window manager will map it again and send MapNotify
				XEvent event;
				event.type = ClientMessage;
				event.xclient.serial = 0;
				event.xclient.send_event = True;
				event.xclient.message_type = net_wm_state_atom;
				event.xclient.window = win;
				event.xclient.format = 32;
				event.xclient.data.l[0] = 1;
				event.xclient.data.l[1] = net_wm_state_hidden_atom;
				event.xclient.data.l[2] = 0;
				event.xclient.data.l[3] = 1;
				event.xclient.data.l[4] = 0;
				XSendEvent(vidx11_display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
			}

			// window is no longer the input focus
			vid_activewindow = false;
			VID_RestoreSystemGamma();

			break;
		case EnterNotify:
			// mouse entered window
			break;
		case LeaveNotify:
			// mouse left window
			break;
		}
	}

	if (dowarp)
	{
		/* move the mouse to the window center again */
		// we'll catch the warp motion by its send_event flag, updating the
		// stored mouse position without adding any delta motion
		XEvent event;
		event.type = MotionNotify;
		event.xmotion.display = vidx11_display;
		event.xmotion.window = win;
		event.xmotion.x = vid.width / 2;
		event.xmotion.y = vid.height / 2;
		XSendEvent(vidx11_display, win, False, PointerMotionMask, &event);
		XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, vid.width / 2, vid.height / 2);
	}
}

static void *prjobj = NULL;

static void GL_CloseLibrary(void)
{
	if (prjobj)
		dlclose(prjobj);
	prjobj = NULL;
	gl_driver[0] = 0;
	qglXGetProcAddressARB = NULL;
	gl_extensions = "";
	gl_platform = "";
	gl_platformextensions = "";
}

static int GL_OpenLibrary(const char *name)
{
	Con_Printf("Loading OpenGL driver %s\n", name);
	GL_CloseLibrary();
	if (!(prjobj = dlopen(name, RTLD_LAZY | RTLD_GLOBAL)))
	{
		Con_Printf("Unable to open symbol list for %s\n", name);
		return false;
	}
	strlcpy(gl_driver, name, sizeof(gl_driver));
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	if (qglXGetProcAddressARB != NULL)
		p = (void *) qglXGetProcAddressARB((GLubyte *)name);
	if (p == NULL)
		p = (void *) dlsym(prjobj, name);
	return p;
}

void VID_Shutdown(void)
{
	if (!ctx || !vidx11_display)
		return;

	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

	// FIXME: glXDestroyContext here?
	if (vid_isvidmodefullscreen)
		XF86VidModeSwitchToMode(vidx11_display, vidx11_screen, &init_vidmode);
	if (win)
		XDestroyWindow(vidx11_display, win);
	XCloseDisplay(vidx11_display);

	vid_hidden = true;
	vid_isfullscreen = false;
	vid_isnetwmfullscreen = false;
	vid_isvidmodefullscreen = false;
	vid_isoverrideredirect = false;
	vidx11_display = NULL;
	win = 0;
	ctx = NULL;

	GL_CloseLibrary();
	Key_ClearStates ();
}

void signal_handler(int sig)
{
	Con_Printf("Received signal %d, exiting...\n", sig);
	VID_RestoreSystemGamma();
	Sys_Quit(1);
}

void InitSig(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

void VID_Finish (void)
{
	vid_usevsync = vid_vsync.integer && !cls.timedemo && gl_videosyncavailable;
	if (vid_usingvsync != vid_usevsync && gl_videosyncavailable)
	{
		vid_usingvsync = vid_usevsync;
		if (qglXSwapIntervalSGI (vid_usevsync))
			Con_Print("glXSwapIntervalSGI didn't accept the vid_vsync change, it will take effect on next vid_restart (GLX_SGI_swap_control does not allow turning off vsync)\n");
	}

	if (r_render.integer)
	{
		CHECKGLERROR
		if (r_speeds.integer == 2 || gl_finish.integer)
		{
			qglFinish();CHECKGLERROR
		}
		qglXSwapBuffers(vidx11_display, win);CHECKGLERROR
	}

	if (vid_x11_hardwaregammasupported)
		VID_UpdateGamma(false, vid_x11_gammarampsize);
}

int VID_SetGamma(unsigned short *ramps, int rampsize)
{
	return XF86VidModeSetGammaRamp(vidx11_display, vidx11_screen, rampsize, ramps, ramps + rampsize, ramps + rampsize*2);
}

int VID_GetGamma(unsigned short *ramps, int rampsize)
{
	return XF86VidModeGetGammaRamp(vidx11_display, vidx11_screen, rampsize, ramps, ramps + rampsize, ramps + rampsize*2);
}

void VID_Init(void)
{
#if !defined(__APPLE__) && !defined(SUNOS)
	Cvar_RegisterVariable (&vid_dgamouse);
#endif
	Cvar_RegisterVariable (&vid_netwmfullscreen);
	InitSig(); // trap evil signals
// COMMANDLINEOPTION: Input: -nomouse disables mouse support (see also vid_mouse cvar)
	if (COM_CheckParm ("-nomouse"))
		mouse_avail = false;
}

void VID_BuildGLXAttrib(int *attrib, qboolean stencil, qboolean stereobuffer, int samples)
{
	*attrib++ = GLX_RGBA;
	*attrib++ = GLX_RED_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = GLX_GREEN_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = GLX_BLUE_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = GLX_DOUBLEBUFFER;
	*attrib++ = GLX_DEPTH_SIZE;*attrib++ = stencil ? 24 : 16;
	// if stencil is enabled, ask for alpha too
	if (stencil)
	{
		*attrib++ = GLX_STENCIL_SIZE;*attrib++ = 8;
		*attrib++ = GLX_ALPHA_SIZE;*attrib++ = 8;
	}
	if (stereobuffer)
		*attrib++ = GLX_STEREO;
	if (samples > 1)
	{
		*attrib++ = GLX_SAMPLE_BUFFERS_ARB;
		*attrib++ = 1;
		*attrib++ = GLX_SAMPLES_ARB;
		*attrib++ = samples;
	}
	*attrib++ = None;
}

int VID_InitMode(int fullscreen, int *width, int *height, int bpp, int refreshrate, int stereobuffer, int samples)
{
	int i;
	int attrib[32];
	XSetWindowAttributes attr;
	XClassHint *clshints;
	XWMHints *wmhints;
	XSizeHints *szhints;
	unsigned long mask;
	XVisualInfo *visinfo;
	int MajorVersion, MinorVersion;
	const char *drivername;

	vid_isfullscreen = false;
	vid_isnetwmfullscreen = false;
	vid_isvidmodefullscreen = false;
	vid_isoverrideredirect = false;

#if defined(__APPLE__) && defined(__MACH__)
	drivername = "/usr/X11R6/lib/libGL.1.dylib";
#else
	drivername = "libGL.so.1";
#endif
// COMMANDLINEOPTION: Linux GLX: -gl_driver <drivername> selects a GL driver library, default is libGL.so.1, useful only for using fxmesa or similar, if you don't know what this is for, you don't need it
// COMMANDLINEOPTION: BSD GLX: -gl_driver <drivername> selects a GL driver library, default is libGL.so.1, useful only for using fxmesa or similar, if you don't know what this is for, you don't need it
// LordHavoc: although this works on MacOSX, it's useless there (as there is only one system libGL)
	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		drivername = com_argv[i + 1];
	if (!GL_OpenLibrary(drivername))
	{
		Con_Printf("Unable to load GL driver \"%s\"\n", drivername);
		return false;
	}

	if (!(vidx11_display = XOpenDisplay(NULL)))
	{
		Con_Print("Couldn't open the X display\n");
		return false;
	}

	// LordHavoc: making the close button on a window do the right thing
	// seems to involve this mess, sigh...
	wm_delete_window_atom = XInternAtom(vidx11_display, "WM_DELETE_WINDOW", false);
	net_wm_state_atom = XInternAtom(vidx11_display, "_NET_WM_STATE", false);
	net_wm_state_fullscreen_atom = XInternAtom(vidx11_display, "_NET_WM_STATE_FULLSCREEN", false);
	net_wm_state_hidden_atom = XInternAtom(vidx11_display, "_NET_WM_STATE_HIDDEN", false);

	// make autorepeat send keypress/keypress/.../keyrelease instead of intervening keyrelease
	XkbSetDetectableAutoRepeat(vidx11_display, true, NULL);

	vidx11_screen = DefaultScreen(vidx11_display);
	root = RootWindow(vidx11_display, vidx11_screen);

	// Get video mode list
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(vidx11_display, &MajorVersion, &MinorVersion))
		vidmode_ext = false;
	else
	{
		Con_DPrintf("Using XFree86-VidModeExtension Version %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}

	if ((qglXChooseVisual = (XVisualInfo *(GLAPIENTRY *)(Display *dpy, int screen, int *attribList))GL_GetProcAddress("glXChooseVisual")) == NULL
	 || (qglXCreateContext = (GLXContext (GLAPIENTRY *)(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct))GL_GetProcAddress("glXCreateContext")) == NULL
	 || (qglXDestroyContext = (void (GLAPIENTRY *)(Display *dpy, GLXContext ctx))GL_GetProcAddress("glXDestroyContext")) == NULL
	 || (qglXMakeCurrent = (Bool (GLAPIENTRY *)(Display *dpy, GLXDrawable drawable, GLXContext ctx))GL_GetProcAddress("glXMakeCurrent")) == NULL
	 || (qglXSwapBuffers = (void (GLAPIENTRY *)(Display *dpy, GLXDrawable drawable))GL_GetProcAddress("glXSwapBuffers")) == NULL
	 || (qglXQueryExtensionsString = (const char *(GLAPIENTRY *)(Display *dpy, int screen))GL_GetProcAddress("glXQueryExtensionsString")) == NULL)
	{
		Con_Printf("glX functions not found in %s\n", gl_driver);
		return false;
	}

	VID_BuildGLXAttrib(attrib, bpp == 32, stereobuffer, samples);
	visinfo = qglXChooseVisual(vidx11_display, vidx11_screen, attrib);
	if (!visinfo)
	{
		Con_Print("Couldn't get an RGB, Double-buffered, Depth visual\n");
		return false;
	}

	if (fullscreen)
	{
		if(vid_netwmfullscreen.integer)
		{
			// TODO detect WM support
			vid_isnetwmfullscreen = true;
			vid_isfullscreen = true;
			// width and height will be filled in later
			Con_DPrintf("Using NetWM fullscreen mode\n");
		}

		if(!vid_isfullscreen && vidmode_ext)
		{
			int best_fit, best_dist, dist, x, y;

			// Are we going fullscreen?  If so, let's change video mode
			XF86VidModeModeLine *current_vidmode;
			XF86VidModeModeInfo **vidmodes;
			int num_vidmodes;

			// This nice hack comes from the SDL source code
			current_vidmode = (XF86VidModeModeLine*)((char*)&init_vidmode + sizeof(init_vidmode.dotclock));
			XF86VidModeGetModeLine(vidx11_display, vidx11_screen, (int*)&init_vidmode.dotclock, current_vidmode);

			XF86VidModeGetAllModeLines(vidx11_display, vidx11_screen, &num_vidmodes, &vidmodes);
			best_dist = 0;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++)
			{
				if (*width > vidmodes[i]->hdisplay || *height > vidmodes[i]->vdisplay)
					continue;

				x = *width - vidmodes[i]->hdisplay;
				y = *height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (best_fit == -1 || dist < best_dist)
				{
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1)
			{
				// LordHavoc: changed from ActualWidth/ActualHeight =,
				// to width/height =, so the window will take the full area of
				// the mode chosen
				*width = vidmodes[best_fit]->hdisplay;
				*height = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(vidx11_display, vidx11_screen, vidmodes[best_fit]);
				memcpy(&game_vidmode, vidmodes[best_fit], sizeof(game_vidmode));
				vid_isvidmodefullscreen = true;
				vid_isfullscreen = true;

				// Move the viewport to top left
				XF86VidModeSetViewPort(vidx11_display, vidx11_screen, 0, 0);
				Con_DPrintf("Using XVidMode fullscreen mode at %dx%d\n", *width, *height);
			}

			free(vidmodes);
		}

		if(!vid_isfullscreen)
		{
			// sorry, no FS available
			// use the full desktop resolution
			vid_isfullscreen = true;
			// width and height will be filled in later
			*width = DisplayWidth(vidx11_display, vidx11_screen);
			*height = DisplayHeight(vidx11_display, vidx11_screen);
			Con_DPrintf("Using X11 fullscreen mode at %dx%d\n", *width, *height);
		}
	}

	// LordHavoc: save the visual for use in gamma ramp settings later
	vidx11_visual = visinfo->visual;

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	// LordHavoc: save the colormap for later, too
	vidx11_colormap = attr.colormap = XCreateColormap(vidx11_display, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;

	if (fullscreen)
	{
		if(vid_isnetwmfullscreen)
		{
			mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | CWEventMask;
			attr.backing_store = NotUseful;
			attr.save_under = False;
		}
		else
		{
			mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | CWEventMask | CWOverrideRedirect;
			attr.override_redirect = True;
			attr.backing_store = NotUseful;
			attr.save_under = False;
			vid_isoverrideredirect = true; // so it knows to grab
		}
	}
	else
	{
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
	}

	win = XCreateWindow(vidx11_display, root, 0, 0, *width, *height, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

	wmhints = XAllocWMHints();
	if(XpmCreatePixmapFromData(vidx11_display, win,
		(gamemode == GAME_NEXUIZ) ? nexuiz_xpm : darkplaces_xpm,
		&wmhints->icon_pixmap, &wmhints->icon_mask, NULL) == XpmSuccess)
		wmhints->flags |= IconPixmapHint | IconMaskHint;

	clshints = XAllocClassHint();
	clshints->res_name = strdup(gamename);
	clshints->res_class = strdup("DarkPlaces");

	szhints = XAllocSizeHints();
	if(vid_resizable.integer == 0 && !vid_isnetwmfullscreen)
	{
		szhints->min_width = szhints->max_width = *width;
		szhints->min_height = szhints->max_height = *height;
		szhints->flags |= PMinSize | PMaxSize;
	}

	XmbSetWMProperties(vidx11_display, win, gamename, gamename, (char **) com_argv, com_argc, szhints, wmhints, clshints);
	XFree(clshints);
	XFree(wmhints);
	XFree(szhints);

	//XStoreName(vidx11_display, win, gamename);
	XMapWindow(vidx11_display, win);

	XSetWMProtocols(vidx11_display, win, &wm_delete_window_atom, 1);

	if (vid_isoverrideredirect)
	{
		XMoveWindow(vidx11_display, win, 0, 0);
		XRaiseWindow(vidx11_display, win);
		XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, 0, 0);
		XFlush(vidx11_display);
	}

	if(vid_isvidmodefullscreen)
	{
		// Move the viewport to top left
		XF86VidModeSetViewPort(vidx11_display, vidx11_screen, 0, 0);
	}

	//XSync(vidx11_display, False);

	ctx = qglXCreateContext(vidx11_display, visinfo, NULL, True);
	if (!ctx)
	{
		Con_Printf ("glXCreateContext failed\n");
		return false;
	}

	if (!qglXMakeCurrent(vidx11_display, win, ctx))
	{
		Con_Printf ("glXMakeCurrent failed\n");
		return false;
	}

	XSync(vidx11_display, False);

	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
	{
		Con_Printf ("glGetString not found in %s\n", gl_driver);
		return false;
	}

	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	gl_platform = "GLX";
	gl_platformextensions = qglXQueryExtensionsString(vidx11_display, vidx11_screen);

	gl_videosyncavailable = false;

// COMMANDLINEOPTION: Linux GLX: -nogetprocaddress disables GLX_ARB_get_proc_address (not required, more formal method of getting extension functions)
// COMMANDLINEOPTION: BSD GLX: -nogetprocaddress disables GLX_ARB_get_proc_address (not required, more formal method of getting extension functions)
// COMMANDLINEOPTION: MacOSX GLX: -nogetprocaddress disables GLX_ARB_get_proc_address (not required, more formal method of getting extension functions)
	GL_CheckExtension("GLX_ARB_get_proc_address", getprocaddressfuncs, "-nogetprocaddress", false);
// COMMANDLINEOPTION: Linux GLX: -novideosync disables GLX_SGI_swap_control
// COMMANDLINEOPTION: BSD GLX: -novideosync disables GLX_SGI_swap_control
// COMMANDLINEOPTION: MacOSX GLX: -novideosync disables GLX_SGI_swap_control
	gl_videosyncavailable = GL_CheckExtension("GLX_SGI_swap_control", swapcontrolfuncs, "-novideosync", false);

	vid_usingmousegrab = false;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
	vid_usingvsync = false;
	vid_hidden = false;
	vid_activewindow = true;
	vid_x11_hardwaregammasupported = XF86VidModeGetGammaRampSize(vidx11_display, vidx11_screen, &vid_x11_gammarampsize) != 0;
#if !defined(__APPLE__) && !defined(SUNOS)
	vid_x11_dgasupported = XF86DGAQueryVersion(vidx11_display, &MajorVersion, &MinorVersion);
	if (!vid_x11_dgasupported)
		Con_Print( "Failed to detect XF86DGA Mouse extension\n" );
#endif

	GL_Init();
	return true;
}

void Sys_SendKeyEvents(void)
{
	static qboolean sound_active = true;

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

	HandleEvents();
}

void IN_Move (void)
{
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	if(vidmode_ext)
	{
		int i, bpp;
		size_t k;
		XF86VidModeModeInfo **vidmodes;
		int num_vidmodes;

		XF86VidModeGetAllModeLines(vidx11_display, vidx11_screen, &num_vidmodes, &vidmodes);
		k = 0;
		for (i = 0; i < num_vidmodes; i++)
		{
			if(k >= maxcount)
				break;
			// we don't get bpp info, so let's just assume all of 8, 15, 16, 24, 32 work
			for(bpp = 8; bpp <= 32; bpp = ((bpp == 8) ? 15 : (bpp & 0xF8) + 8))
			{
				if(k >= maxcount)
					break;
				modes[k].width = vidmodes[i]->hdisplay;
				modes[k].height = vidmodes[i]->vdisplay;
				modes[k].bpp = 8;
				modes[k].refreshrate = vidmodes[i]->dotclock / vidmodes[i]->htotal / vidmodes[i]->vtotal;
				modes[k].pixelheight_num = 1;
				modes[k].pixelheight_denom = 1; // xvidmode does not provide this
				++k;
			}
		}
		return k;
	}
	return 0; // FIXME implement this
}
