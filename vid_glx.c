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

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include <dlfcn.h>

#include "quakedef.h"

#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <X11/extensions/XShm.h>
#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86vmode.h>

static Display *vidx11_display = NULL;
static int scrnum;
static Window win;
static GLXContext ctx = NULL;

Atom wm_delete_window_atom;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )


viddef_t	vid;				// global video state

static qboolean		mouse_avail = true;
static qboolean		mouse_active = false, usingmouse = false;
static float	mouse_x, mouse_y;
static int p_mouse_x, p_mouse_y;

cvar_t vid_dga = {CVAR_SAVE, "vid_dga", "1"};
cvar_t vid_dga_mouseaccel = {0, "vid_dga_mouseaccel", "1"};

qboolean vidmode_ext = false;

static int win_x, win_y;

static int scr_width, scr_height;

static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qboolean vidmode_active = false;

static Visual *vidx11_visual;
static Colormap vidx11_colormap;

/*-----------------------------------------------------------------------*/

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

/*-----------------------------------------------------------------------*/
static int
XLateKey(XKeyEvent *ev)
{
	int key = 0;
	KeySym keysym;

	keysym = XLookupKeysym(ev, 0);

	switch(keysym)
	{
		case XK_KP_Page_Up:	key = KP_PGUP; break;
		case XK_Page_Up:	key = K_PGUP; break;

		case XK_KP_Page_Down:	key = KP_PGDN; break;
		case XK_Page_Down:	key = K_PGDN; break;

		case XK_KP_Home:	key = KP_HOME; break;
		case XK_Home:		key = K_HOME; break;

		case XK_KP_End:		key = KP_END; break;
		case XK_End:		key = K_END; break;

		case XK_KP_Left:	key = KP_LEFTARROW; break;
		case XK_Left:		key = K_LEFTARROW; break;

		case XK_KP_Right:	key = KP_RIGHTARROW; break;
		case XK_Right:		key = K_RIGHTARROW; break;

		case XK_KP_Down:	key = KP_DOWNARROW; break;
		case XK_Down:		key = K_DOWNARROW; break;

		case XK_KP_Up:		key = KP_UPARROW; break;
		case XK_Up:			key = K_UPARROW; break;

		case XK_Escape:		key = K_ESCAPE; break;

		case XK_KP_Enter:	key = KP_ENTER; break;
		case XK_Return:		key = K_ENTER; break;

		case XK_Tab:		key = K_TAB; break;

		case XK_F1:			key = K_F1; break;
		case XK_F2:			key = K_F2; break;
		case XK_F3:			key = K_F3; break;
		case XK_F4:			key = K_F4; break;
		case XK_F5:			key = K_F5; break;
		case XK_F6:			key = K_F6; break;
		case XK_F7:			key = K_F7; break;
		case XK_F8:			key = K_F8; break;
		case XK_F9:			key = K_F9; break;
		case XK_F10:		key = K_F10; break;
		case XK_F11:		key = K_F11; break;
		case XK_F12:		key = K_F12; break;

		case XK_BackSpace:	key = K_BACKSPACE; break;

		case XK_KP_Delete:	key = KP_DEL; break;
		case XK_Delete:		key = K_DEL; break;

		case XK_Pause:		key = K_PAUSE; break;

		case XK_Shift_L:
		case XK_Shift_R:	key = K_SHIFT; break;

		case XK_Execute:
		case XK_Control_L:
		case XK_Control_R:	key = K_CTRL; break;

		case XK_Mode_switch:
		case XK_Alt_L:
		case XK_Meta_L:
		case XK_Alt_R:
		case XK_Meta_R:		key = K_ALT; break;

		case XK_Caps_Lock:	key = K_CAPSLOCK; break;
		case XK_KP_Begin:	key = KP_5; break;

		case XK_Insert:		key = K_INS; break;
		case XK_KP_Insert:	key = KP_INS; break;

		case XK_KP_Multiply:	key = KP_MULTIPLY; break;
		case XK_KP_Add:		key = KP_PLUS; break;
		case XK_KP_Subtract:	key = KP_MINUS; break;
		case XK_KP_Divide:	key = KP_DIVIDE; break;

		/* For Sun keyboards */
		case XK_F27:		key = K_HOME; break;
		case XK_F29:		key = K_PGUP; break;
		case XK_F33:		key = K_END; break;
		case XK_F35:		key = K_PGDN; break;

		default:
			if (keysym < 128)
			{
				/* ASCII keys */
				key = keysym;
				if ((key >= 'A') && (key <= 'Z'))
					key = key + ('a' - 'A');
			}
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

static void install_grabs(void)
{
	XWindowAttributes attribs_1;
	XSetWindowAttributes attribs_2;

	XGetWindowAttributes(vidx11_display, win, &attribs_1);
	attribs_2.event_mask = attribs_1.your_event_mask | KEY_MASK | MOUSE_MASK;
	XChangeWindowAttributes(vidx11_display, win, CWEventMask, &attribs_2);

// inviso cursor
	XDefineCursor(vidx11_display, win, CreateNullCursor(vidx11_display, win));

	XGrabPointer(vidx11_display, win,  True, 0, GrabModeAsync, GrabModeAsync, win, None, CurrentTime);

	if (vid_dga.integer)
	{
		int MajorVersion, MinorVersion;

		if (!XF86DGAQueryVersion(vidx11_display, &MajorVersion, &MinorVersion))
		{
			// unable to query, probalby not supported
			Con_Printf( "Failed to detect XF86DGA Mouse\n" );
			vid_dga.integer = 0;
		}
		else
		{
			vid_dga.integer = 1;
			XF86DGADirectVideo(vidx11_display, DefaultScreen(vidx11_display), XF86DGADirectMouse);
			XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, 0, 0);
		}
	}
	else
		XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, scr_width / 2, scr_height / 2);

	XGrabKeyboard(vidx11_display, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	mouse_active = true;
	mouse_x = mouse_y = 0;
}

static void uninstall_grabs(void)
{
	if (!vidx11_display || !win)
		return;

	if (vid_dga.integer == 1)
		XF86DGADirectVideo(vidx11_display, DefaultScreen(vidx11_display), 0);

	XUngrabPointer(vidx11_display, CurrentTime);
	XUngrabKeyboard(vidx11_display, CurrentTime);

// inviso cursor
	XUndefineCursor(vidx11_display, win);

	mouse_active = false;
}

static void HandleEvents(void)
{
	XEvent event;
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
			Key_Event(XLateKey(&event.xkey), true);
			break;

		case KeyRelease:
			// key released
			Key_Event(XLateKey(&event.xkey), false);
			break;

		case MotionNotify:
			// mouse moved
			if (usingmouse)
			{
				if (vid_dga.integer == 1)
				{
					mouse_x += event.xmotion.x_root * vid_dga_mouseaccel.value;
					mouse_y += event.xmotion.y_root * vid_dga_mouseaccel.value;
				}
				else
				{

					if (!event.xmotion.send_event)
					{
						mouse_x += event.xmotion.x - p_mouse_x;
						mouse_y += event.xmotion.y - p_mouse_y;
						if (abs(scr_width/2 - event.xmotion.x) > scr_width / 4 || abs(scr_height/2 - event.xmotion.y) > scr_height / 4)
							dowarp = true;
					}
					p_mouse_x = event.xmotion.x;
					p_mouse_y = event.xmotion.y;
				}
			}
			else
				ui_mouseupdate(event.xmotion.x, event.xmotion.y);
			break;

		case ButtonPress:
			// mouse button pressed
			switch(event.xbutton.button)
			{
			case 1:
				Key_Event(K_MOUSE1, true);
				break;
			case 2:
				Key_Event(K_MOUSE3, true);
				break;
			case 3:
				Key_Event(K_MOUSE2, true);
				break;
			case 4:
				Key_Event(K_MWHEELUP, true);
				break;
			case 5:
				Key_Event(K_MWHEELDOWN, true);
				break;
    		default:
				Con_Printf("HandleEvents: ButtonPress gave value %d, 1-5 expected\n", event.xbutton.button);
				break;
			}
			break;

		case ButtonRelease:
			// mouse button released
			switch(event.xbutton.button)
			{
			case 1:
				Key_Event(K_MOUSE1, false);
				break;
			case 2:
				Key_Event(K_MOUSE3, false);
				break;
			case 3:
				Key_Event(K_MOUSE2, false);
				break;
			case 4:
				Key_Event(K_MWHEELUP, false);
				break;
			case 5:
				Key_Event(K_MWHEELDOWN, false);
				break;
    		default:
				Con_Printf("HandleEvents: ButtonRelease gave value %d, 1-5 expected\n", event.xbutton.button);
				break;
			}
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
			break;
		case DestroyNotify:
			// window has been destroyed
			Sys_Quit();
			break;
		case ClientMessage:
			// window manager messages
			if ((event.xclient.format == 32) && (event.xclient.data.l[0] == wm_delete_window_atom))
				Sys_Quit();
			break;
		case MapNotify:
			// window restored
			vid_hidden = false;
			break;
		case UnmapNotify:
			// window iconified/rolledup/whatever
			vid_hidden = true;
			break;
		case FocusIn:
			// window is now the input focus
			break;
		case FocusOut:
			// window is no longer the input focus
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
		p_mouse_x = scr_width / 2;
		p_mouse_y = scr_height / 2;
		XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, p_mouse_x, p_mouse_y);
	}

}

static void IN_DeactivateMouse( void )
{
	if (!mouse_avail || !vidx11_display || !win)
		return;

	if (mouse_active)
	{
		uninstall_grabs();
		mouse_active = false;
	}
}

static void IN_ActivateMouse( void )
{
	if (!mouse_avail || !vidx11_display || !win)
		return;

	if (!mouse_active)
	{
		mouse_x = mouse_y = 0; // don't spazz
		install_grabs();
		mouse_active = true;
	}
}


void VID_Shutdown(void)
{
	if (!ctx || !vidx11_display)
		return;

	if (vidx11_display)
	{
		uninstall_grabs();

		if (vidmode_active)
			XF86VidModeSwitchToMode(vidx11_display, scrnum, vidmodes[0]);
		if (win)
			XDestroyWindow(vidx11_display, win);
		XCloseDisplay(vidx11_display);
	}
	vidmode_active = false;
	vidx11_display = NULL;
	win = 0;
	ctx = NULL;
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	Sys_Quit();
	exit(0);
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

/*
=================
VID_GetWindowSize
=================
*/
void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

void VID_Finish (void)
{
	int usemouse;
	if (r_render.integer)
	{
		qglFinish();
		glXSwapBuffers(vidx11_display, win);
	}

// handle the mouse state when windowed if that's changed
	usemouse = false;
	if (vid_mouse.integer && key_dest == key_game)
		usemouse = true;
	if (vidmode_active)
		usemouse = true;
	if (usemouse)
	{
		if (!usingmouse)
		{
			usingmouse = true;
			IN_ActivateMouse ();
		}
	}
	else
	{
		if (usingmouse)
		{
			usingmouse = false;
			IN_DeactivateMouse ();
		}
	}
}

// LordHavoc: ported from SDL 1.2.2, this was far more difficult to port from
// SDL than to simply use the XFree gamma ramp extension, but that affects the
// whole screen even when the game window is inactive, this only affects the
// screen while the window is active, very desirable behavior :)
int VID_SetGamma(float prescale, float gamma, float scale, float base)
{
// LordHavoc: FIXME: finish this code, we need to allocate colors before we can store them
#if 1
	return FALSE;
#else
	int i, ncolors, c;
	unsigned int Rmask, Gmask, Bmask, Rloss, Gloss, Bloss, Rshift, Gshift, Bshift, mask;
	XColor xcmap[256];
	unsigned short ramp[256];

	if (COM_CheckParm("-nogamma"))
		return FALSE;

	if (vidx11_visual->class != DirectColor)
	{
		Con_Printf("X11 Visual class is %d, can only do gamma on %d\n", vidx11_visual->class, DirectColor);
		return FALSE;
	}

	Rmask = vidx11_visual->red_mask;
	Gmask = vidx11_visual->green_mask;
	Bmask = vidx11_visual->blue_mask;

	Rshift = 0;
	Rloss = 8;
	if ((mask = Rmask))
	{
		for (;!(mask & 1);mask >>= 1)
			++Rshift;
		for (;(mask & 1);mask >>= 1)
			--Rloss;
	}
	Gshift = 0;
	Gloss = 8;
	if ((mask = Gmask))
	{
		for (;!(mask & 1);mask >>= 1)
			++Gshift;
		for (;(mask & 1);mask >>= 1)
			--Gloss;
	}
	Bshift = 0;
	Bloss = 8;
	if ((mask = Bmask))
	{
		for (;!(mask & 1);mask >>= 1)
			++Bshift;
		for (;(mask & 1);mask >>= 1)
			--Bloss;
	}

	BuildGammaTable16(prescale, gamma, scale, base, ramp);

	// convert gamma ramp to palette (yes this seems odd)
	ncolors = vidx11_visual->map_entries;
	for (i = 0;i < ncolors;i++)
	{
		c = (256 * i / ncolors);
		xcmap[i].pixel = ((c >> Rloss) << Rshift) | ((c >> Gloss) << Gshift) | ((c >> Bloss) << Bshift);
		xcmap[i].red   = ramp[c];
		xcmap[i].green = ramp[c];
		xcmap[i].blue  = ramp[c];
		xcmap[i].flags = (DoRed|DoGreen|DoBlue);
	}
	XStoreColors(vidx11_display, vidx11_colormap, xcmap, ncolors);
	XSync(vidx11_display, false);
	// FIXME: should this check for BadAccess/BadColor/BadValue errors produced by XStoreColors before setting this true?
	return TRUE;
#endif
}

void VID_Init(void)
{
	int i;
// LordHavoc: FIXME: finish this code, we need to allocate colors before we can store them
#if 0
	int gammaattrib[] =
	{
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		GLX_X_VISUAL_TYPE, GLX_DIRECT_COLOR,
		None
	};
#endif
	int nogammaattrib[] =
	{
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
	int width = 640, height = 480;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
	qboolean fullscreen = true;
	int MajorVersion, MinorVersion;

	Cvar_RegisterVariable (&vid_dga);
	Cvar_RegisterVariable (&vid_dga_mouseaccel);

// interpret command-line params

// set vid parameters
	if ((i = COM_CheckParm("-window")) != 0)
		fullscreen = false;

	if ((i = COM_CheckParm("-width")) != 0)
		width = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-height")) != 0)
		height = atoi(com_argv[i+1]);

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

	if (!(vidx11_display = XOpenDisplay(NULL)))
	{
		fprintf(stderr, "Error couldn't open the X display\n");
		exit(1);
	}

	scrnum = DefaultScreen(vidx11_display);
	root = RootWindow(vidx11_display, scrnum);

	// Get video mode list
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(vidx11_display, &MajorVersion, &MinorVersion))
		vidmode_ext = false;
	else
	{
		Con_Printf("Using XFree86-VidModeExtension Version %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}

	visinfo = NULL;
// LordHavoc: FIXME: finish this code, we need to allocate colors before we can store them
#if 0
	if (!COM_CheckParm("-nogamma"))
		visinfo = glXChooseVisual(vidx11_display, scrnum, gammaattrib);
#endif
	if (!visinfo)
	{
		visinfo = glXChooseVisual(vidx11_display, scrnum, nogammaattrib);
		if (!visinfo)
		{
			fprintf(stderr, "qkHack: Error couldn't get an RGB, Double-buffered, Depth visual\n");
			exit(1);
		}
	}

	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(vidx11_display, scrnum, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen)
		{
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++)
			{
				if (width > vidmodes[i]->hdisplay || height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist)
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
				width = vidmodes[best_fit]->hdisplay;
				height = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(vidx11_display, scrnum, vidmodes[best_fit]);
				vidmode_active = true;

				// Move the viewport to top left
				XF86VidModeSetViewPort(vidx11_display, scrnum, 0, 0);
			}
			else
				fullscreen = 0;
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
	if (vidmode_active)
	{
		mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	}
	else
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(vidx11_display, root, 0, 0, width, height, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);
	XStoreName(vidx11_display, win, gamename);
	XMapWindow(vidx11_display, win);

	// LordHavoc: making the close button on a window do the right thing
	// seems to involve this mess, sigh...
	wm_delete_window_atom = XInternAtom(vidx11_display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(vidx11_display, win, &wm_delete_window_atom, 1);

	if (vidmode_active)
	{
		XMoveWindow(vidx11_display, win, 0, 0);
		XRaiseWindow(vidx11_display, win);
		XWarpPointer(vidx11_display, None, win, 0, 0, 0, 0, 0, 0);
		XFlush(vidx11_display);
		// Move the viewport to top left
		XF86VidModeSetViewPort(vidx11_display, scrnum, 0, 0);
	}

	XFlush(vidx11_display);

	ctx = glXCreateContext(vidx11_display, visinfo, NULL, True);

	glXMakeCurrent(vidx11_display, win, ctx);

	scr_width = width;
	scr_height = height;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;

	InitSig(); // trap evil signals

	vid_hidden = false;

	GL_Init();

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);
}

void Sys_SendKeyEvents(void)
{
	HandleEvents();
}

void IN_Init(void)
{
	if (COM_CheckParm ("-nomouse"))
		mouse_avail = false;
}

void IN_Shutdown(void)
{
}

/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	if (mouse_avail)
		IN_Mouse(cmd, mouse_x, mouse_y);
	mouse_x = 0;
	mouse_y = 0;
}

