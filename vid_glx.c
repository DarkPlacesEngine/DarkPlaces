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

static Display *dpy = NULL;
static int scrnum;
static Window win;
static GLXContext ctx = NULL;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )


viddef_t	vid;				// global video state

static qboolean		mouse_avail = true;
static qboolean		mouse_active = false, usingmouse = false;
// static qboolean		dga_active;
static float	mouse_x, mouse_y;
static float	old_mouse_x, old_mouse_y;
static int p_mouse_x, p_mouse_y;

cvar_t vid_dga = {CVAR_SAVE, "vid_dga", "1"};
cvar_t vid_dga_mouseaccel = {0, "vid_dga_mouseaccel", "1"};
cvar_t m_filter = {0, "m_filter", "0"};

qboolean vidmode_ext = false;

static int win_x, win_y;

static int scr_width, scr_height;

static XF86VidModeModeInfo **vidmodes;
//static int default_dotclock_vidmode;
static int num_vidmodes;
static qboolean vidmode_active = false;

/*-----------------------------------------------------------------------*/

float		gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

/*-----------------------------------------------------------------------*/
static int
XLateKey(XKeyEvent *ev/*, qboolean modified*/)
{
//	char tmp[2];
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
				if (/*!modified && */((key >= 'A') && (key <= 'Z')))
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

	cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
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

	XGetWindowAttributes(dpy, win, &attribs_1);
	attribs_2.event_mask = attribs_1.your_event_mask | KEY_MASK | MOUSE_MASK;
	XChangeWindowAttributes(dpy, win, CWEventMask, &attribs_2);

// inviso cursor
	XDefineCursor(dpy, win, CreateNullCursor(dpy, win));

	XGrabPointer(dpy, win,  True, 0, GrabModeAsync, GrabModeAsync, win, None, CurrentTime);

	if (vid_dga.value)
	{
		int MajorVersion, MinorVersion;

		if (!XF86DGAQueryVersion(dpy, &MajorVersion, &MinorVersion))
		{
			// unable to query, probalby not supported
			Con_Printf( "Failed to detect XF86DGA Mouse\n" );
			vid_dga.value = 0;
		}
		else
		{
			vid_dga.value = 1;
			XF86DGADirectVideo(dpy, DefaultScreen(dpy), XF86DGADirectMouse);
			XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		}
	}
	else
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, vid.width / 2, vid.height / 2);

	XGrabKeyboard(dpy, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	mouse_active = true;
	mouse_x = mouse_y = 0;

//	XSync(dpy, True);
}

static void uninstall_grabs(void)
{
	if (!dpy || !win)
		return;

	if (vid_dga.value == 1)
		XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);

	XUngrabPointer(dpy, CurrentTime);
	XUngrabKeyboard(dpy, CurrentTime);

// inviso cursor
	XUndefineCursor(dpy, win);

	mouse_active = false;
}

static void HandleEvents(void)
{
	XEvent event;
//	KeySym ks;
	qboolean dowarp = false;

	if (!dpy)
		return;

	while (XPending(dpy))
	{
		XNextEvent(dpy, &event);

		switch (event.type)
		{
		case KeyPress:
		case KeyRelease:
			Key_Event(XLateKey(&event.xkey), event.type == KeyPress);
			break;

		case MotionNotify:
			if (usingmouse)
			{
				if (vid_dga.value == 1)
				{
					mouse_x += event.xmotion.x_root * vid_dga_mouseaccel.value;
					mouse_y += event.xmotion.y_root * vid_dga_mouseaccel.value;
				}
				else
				{
					/*
					if (!p_mouse_x && !p_mouse_y)
					{
						Con_Printf("event->xmotion.x: %d\n", event.xmotion.x);
						Con_Printf("event->xmotion.y: %d\n", event.xmotion.y);
					}
					*/
					//if (usingmouse)
					{
						if (!event.xmotion.send_event)
						{
							mouse_x += event.xmotion.x - p_mouse_x;
							mouse_y += event.xmotion.y - p_mouse_y;
							if (abs(vid.width/2 - event.xmotion.x) > vid.width / 4 || abs(vid.height/2 - event.xmotion.y) > vid.height / 4)
								dowarp = true;
						}
					}
					/*
					else
					{
						mouse_x += (event.xmotion.x - p_mouse_x);
						mouse_y += (event.xmotion.y - p_mouse_y);
					}
					*/
					p_mouse_x = event.xmotion.x;
					p_mouse_y = event.xmotion.y;
				}
			}
			else
				ui_mouseupdate(event.xmotion.x, event.xmotion.y);
			break;

		case ButtonPress:
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

		case CreateNotify :
			win_x = event.xcreatewindow.x;
			win_y = event.xcreatewindow.y;
			break;

		case ConfigureNotify :
			win_x = event.xconfigure.x;
			win_y = event.xconfigure.y;
			break;
		}
	}

	if (dowarp)
	{
		/* move the mouse to the window center again */
		p_mouse_x = vid.width / 2;
		p_mouse_y = vid.height / 2;
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, p_mouse_x, p_mouse_y);
	}

}

static void IN_DeactivateMouse( void )
{
	if (!mouse_avail || !dpy || !win)
		return;

	if (mouse_active)
	{
		uninstall_grabs();
		mouse_active = false;
	}
}

static void IN_ActivateMouse( void )
{
	if (!mouse_avail || !dpy || !win)
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
	if (!ctx || !dpy)
		return;

	if (dpy)
	{
		uninstall_grabs();

		if (vidmode_active)
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[0]);
/* Disabled, causes a segfault during shutdown.
		if (ctx)
			glXDestroyContext(dpy, ctx);
*/
		if (win)
			XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}
	vidmode_active = false;
	dpy = NULL;
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
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;

//	glViewport (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	int usemouse;
	if (!r_render.value)
		return;
	glFlush();
	glXSwapBuffers(dpy, win);

// handle the mouse state when windowed if that's changed
	usemouse = false;
	if (vid_mouse.value && key_dest == key_game)
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

int VID_SetGamma(float prescale, float gamma, float scale, float base)
{
	return FALSE;
}

void VID_Init(void)
{
	int i;
	int attrib[] =
	{
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
//	char	gldir[MAX_OSPATH];
	int width = 640, height = 480;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
	qboolean fullscreen = true;
	int MajorVersion, MinorVersion;
	int actualWidth, actualHeight;

	Cvar_RegisterVariable (&vid_mouse);
	Cvar_RegisterVariable (&vid_dga);
	Cvar_RegisterVariable (&vid_dga_mouseaccel);
	Cvar_RegisterVariable (&m_filter);

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

	if (!(dpy = XOpenDisplay(NULL)))
	{
		fprintf(stderr, "Error couldn't open the X display\n");
		exit(1);
	}

	scrnum = DefaultScreen(dpy);
	root = RootWindow(dpy, scrnum);

	// Get video mode list
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &MajorVersion, &MinorVersion))
		vidmode_ext = false;
	else
	{
		Con_Printf("Using XFree86-VidModeExtension Version %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}

	visinfo = glXChooseVisual(dpy, scrnum, attrib);
	if (!visinfo)
	{
		fprintf(stderr, "qkHack: Error couldn't get an RGB, Double-buffered, Depth visual\n");
		exit(1);
	}

	if (vidmode_ext)
	{
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

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
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
				vidmode_active = true;

				// Move the viewport to top left
				XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
			}
			else
				fullscreen = 0;
		}
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
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

	win = XCreateWindow(dpy, root, 0, 0, width, height, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);
	XStoreName(dpy, win, "DarkPlaces-GLX");
	XMapWindow(dpy, win);

	if (vidmode_active)
	{
		XMoveWindow(dpy, win, 0, 0);
		XRaiseWindow(dpy, win);
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		XFlush(dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
	}

	XFlush(dpy);

	ctx = glXCreateContext(dpy, visinfo, NULL, True);

	glXMakeCurrent(dpy, win, ctx);

	scr_width = width;
	scr_height = height;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	InitSig(); // trap evil signals

	GL_Init();

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	// force a surface cache flush
	vid.recalc_refdef = 1;
}

void Sys_SendKeyEvents(void)
{
	HandleEvents();
}

void IN_Init(void)
{
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

/*
===========
IN_Move
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	if (!mouse_avail)
		return;

	if (m_filter.value)
	{
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;

		old_mouse_x = mouse_x;
		old_mouse_y = mouse_y;
	}

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	if (in_strafe.state & 1)
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	//if (freelook)
		V_StopPitchDrift ();

	if (/*freelook && */!(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);
}


