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


unsigned		d_8to24table[256];
unsigned char	d_15to8table[65536];

cvar_t	vid_mode = {"vid_mode","0",false};
 
viddef_t	vid;				// global video state

static qboolean        mouse_avail = true;
static qboolean        mouse_active = false;
static float   mouse_x, mouse_y;
static float	old_mouse_x, old_mouse_y;
static int p_mouse_x, p_mouse_y;

static cvar_t in_mouse = {"in_mouse", "1", false};
static cvar_t in_dgamouse = {"in_dgamouse", "1", false};
static cvar_t m_filter = {"m_filter", "0"};

qboolean dgamouse = false;
qboolean vidmode_ext = false;

static int win_x, win_y;

static int scr_width, scr_height;

static XF86VidModeModeInfo **vidmodes;
//static int default_dotclock_vidmode;
static int num_vidmodes;
static qboolean vidmode_active = false;

/*-----------------------------------------------------------------------*/

int		texture_extension_number = 1;

float		gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

void (*qglMTexCoord2f) (GLenum, GLfloat, GLfloat);
void (*qglSelectTexture) (GLenum);

//static float vid_gamma = 1.0;

// LordHavoc: ARB multitexture support
int gl_mtex_enum = 0;

// LordHavoc: in GLX these are never set, simply provided to make the rest of the code work
qboolean isG200 = false;
qboolean isRagePro = false;
qboolean gl_mtexable = false;
qboolean gl_arrays = false;

/*-----------------------------------------------------------------------*/
static int
XLateKey(XKeyEvent *ev/*, qboolean modified*/)
{
	char tmp[2];
	int key = 0;
	KeySym keysym;

/*	if (!modified) {*/
		keysym = XLookupKeysym(ev, 0);
/*	} else {
		XLookupString(ev, tmp, 1, &keysym, NULL);
	}*/

	switch(keysym) {
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
			if (keysym < 128) {
				/* ASCII keys */
				key = keysym;
				if (/*!modified && */((key >= 'A') && (key <= 'Z'))) {
					key = key + ('a' - 'A');
				}
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
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
          &dummycolour,&dummycolour, 0,0);
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

	XGrabPointer(dpy, win,  True, 0, GrabModeAsync, GrabModeAsync,
		     win, None, CurrentTime);

/*	if (in_dgamouse.value) {
		int MajorVersion, MinorVersion;

		if (!XF86DGAQueryVersion(dpy, &MajorVersion, &MinorVersion)) { 
			// unable to query, probalby not supported
			Con_Printf( "Failed to detect XF86DGA Mouse\n" );
			in_dgamouse.value = 0;
		} else {
			dgamouse = true;
			XF86DGADirectVideo(dpy, DefaultScreen(dpy), XF86DGADirectMouse);
			XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		}
	} else {*/
		XWarpPointer(dpy, None, win,
					 0, 0, 0, 0,
					 vid.width / 2, vid.height / 2);
/*	}*/

	XGrabKeyboard(dpy, win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	mouse_active = true;
	mouse_x = mouse_y = 0;

//	XSync(dpy, True);
}

static void uninstall_grabs(void)
{
	if (!dpy || !win)
		return;

/*	if (dgamouse) {
		dgamouse = false;
		XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);
	}*/

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
	int b;
	qboolean dowarp = false;

	if (!dpy)
		return;

	while (XPending(dpy)) {
		XNextEvent(dpy, &event);

		switch (event.type) {
		case KeyPress:
		case KeyRelease:
			Key_Event(XLateKey(&event.xkey), event.type == KeyPress);
			break;

		case MotionNotify:
        		if (dgamouse) {
                		mouse_x += event.xmotion.x_root/* * in_dga_mouseaccel.value*/;
                		mouse_y += event.xmotion.y_root/* * in_dga_mouseaccel.value*/;
		        } else {
		                if (!p_mouse_x && !p_mouse_y) {
		                        Con_Printf("event->xmotion.x: %d\n", event.xmotion.x); 
		                        Con_Printf("event->xmotion.y: %d\n", event.xmotion.y); 
		                }
/*		                if (vid_fullscreen.value || _windowed_mouse.value) {*/
		                        if (!event.xmotion.send_event) {
		                                mouse_x += (event.xmotion.x - p_mouse_x);
		                                mouse_y += (event.xmotion.y - p_mouse_y);
		                                if (abs(vid.width/2 - event.xmotion.x) > vid.width / 4
		                                    || abs(vid.height/2 - event.xmotion.y) > vid.height / 4) {
		                                        dowarp = true;
		                                }
		                        }
/*		                } else {
		                        mouse_x += (event.xmotion.x - p_mouse_x);
		                        mouse_y += (event.xmotion.y - p_mouse_y);
		                }*/
                		p_mouse_x = event.xmotion.x;
                		p_mouse_y = event.xmotion.y;
        		}
        
/*			if (mouse_active) {
				if (dgamouse) {
					mouse_x += (event.xmotion.x + win_x) * 2;
					mouse_y += (event.xmotion.y + win_y) * 2;
				} 
				else
				{
					mouse_x += ((int)event.xmotion.x - mwx) * 2;
					mouse_y += ((int)event.xmotion.y - mwy) * 2;
					mwx = event.xmotion.x;
					mwy = event.xmotion.y;

					if (mouse_x || mouse_y)
						dowarp = true;
				}
			}*/
			break;

		case ButtonPress:
			b=-1;
			if (event.xbutton.button == 1)
				b = 0;
			else if (event.xbutton.button == 2)
				b = 2;
			else if (event.xbutton.button == 3)
				b = 1;
			if (b>=0)
				Key_Event(K_MOUSE1 + b, true);
			break;

		case ButtonRelease:
			b=-1;
			if (event.xbutton.button == 1)
				b = 0;
			else if (event.xbutton.button == 2)
				b = 2;
			else if (event.xbutton.button == 3)
				b = 1;
			if (b>=0)
				Key_Event(K_MOUSE1 + b, false);
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

	if (dowarp) {
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

	if (mouse_active) {
		uninstall_grabs();
		mouse_active = false;
	}
}

static void IN_ActivateMouse( void ) 
{
	if (!mouse_avail || !dpy || !win)
		return;

	if (!mouse_active) {
		mouse_x = mouse_y = 0; // don't spazz
		install_grabs();
		mouse_active = true;
	}
}


void VID_Shutdown(void)
{
	if (!ctx || !dpy)
		return;

	if (dpy) {
		uninstall_grabs();

		if (ctx)
			glXDestroyContext(dpy, ctx);
		if (win)
			XDestroyWindow(dpy, win);
		if (vidmode_active)
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[0]);
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
void (*qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (*qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (*qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (*qglArrayElement)(GLint i);
void (*qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (*qglMTexCoord2f)(GLenum, GLfloat, GLfloat);
void (*qglSelectTexture)(GLenum);

void VID_CheckVertexArrays (void)
{
	void *prjobj;
	if (COM_CheckParm("-novertex"))
	{
		Con_Printf("...vertex array support disabled\n");
		return;
	}
	if ((prjobj = dlopen(NULL, RTLD_LAZY)) == NULL)
	{
		Con_Printf("Unable to open symbol list for main program.\n");
		return;
	}
	qglMTexCoord2fSGIS = (void *) dlsym(prjobj, "glMTexCoord2fSGIS");
	if ((qglArrayElement = (void *) dlsym(prjobj, "glArrayElement"))
	 && (qglColorPointer = (void *) dlsym(prjobj, "glColorPointer"))
//	 && (qglDrawArrays = (void *) dlsym(prjobj, "glDrawArrays"))
	 && (qglDrawElements = (void *) dlsym(prjobj, "glDrawElements"))
//	 && (qglInterleavedArrays = (void *) dlsym(prjobj, "glInterleavedArrays"))
	 && (qglTexCoordPointer = (void *) dlsym(prjobj, "glTexCoordPointer"))
	 && (qglVertexPointer = (void *) dlsym(prjobj, "glVertexPointer"))
		)
	{
		Con_Printf("...vertex array support detected\n");
		gl_arrays = true;
		dlclose(prjobj);
		return;
	}

	Con_Printf("...vertex array support disabled (not detected - get a better driver)\n");
	dlclose(prjobj);
}
*/

// LordHavoc: require OpenGL 1.2.x
void VID_CheckVertexArrays (void)
{
	gl_arrays = true;
}

void VID_CheckMultiTexture(void) 
{
	void *prjobj;
	qglMTexCoord2f = NULL;
	qglSelectTexture = NULL;
	// Check to see if multitexture is disabled
	if (COM_CheckParm("-nomtex"))
	{
		Con_Printf("...multitexture disabled\n");
		return;
	}
	if ((prjobj = dlopen(NULL, RTLD_LAZY)) == NULL)
	{
		Con_Printf("Unable to open symbol list for main program.\n");
		return;
	}
	// Test for ARB_multitexture
	if (!COM_CheckParm("-SGISmtex") && strstr(gl_extensions, "GL_ARB_multitexture "))
	{
		Con_Printf("...using GL_ARB_multitexture\n");
		qglMTexCoord2f = (void *) dlsym(prjobj, "glMultiTexCoord2fARB");
		qglSelectTexture = (void *) dlsym(prjobj, "glActiveTextureARB");
		gl_mtexable = true;
		gl_mtex_enum = GL_TEXTURE0_ARB;
	}
	else if (strstr(gl_extensions, "GL_SGIS_multitexture ")) // Test for SGIS_multitexture (if ARB_multitexture not found)
	{
		Con_Printf("...using GL_SGIS_multitexture\n");
		qglMTexCoord2f = (void *) dlsym(prjobj, "glMTexCoord2fSGIS");
		qglSelectTexture = (void *) dlsym(prjobj, "glSelectTextureSGIS");
		gl_mtexable = true;
		gl_mtex_enum = TEXTURE0_SGIS;
	}
	if (!gl_mtexable)
		Con_Printf("...multitexture disabled (not detected)\n");
	dlclose(prjobj);
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
	if (!r_render.value)
		return;
	glFlush();
	glXSwapBuffers(dpy, win);
}

void VID_Init()
{
	int i;
	int attrib[] = {
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

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&in_mouse);
	Cvar_RegisterVariable (&in_dgamouse);
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

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error couldn't open the X display\n");
		exit(1);
	}

	scrnum = DefaultScreen(dpy);
	root = RootWindow(dpy, scrnum);

	// Get video mode list
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &MajorVersion, &MinorVersion)) { 
		vidmode_ext = false;
	} else {
		Con_Printf("Using XFree86-VidModeExtension Version %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}

	visinfo = glXChooseVisual(dpy, scrnum, attrib);
	if (!visinfo) {
		fprintf(stderr, "qkHack: Error couldn't get an RGB, Double-buffered, Depth visual\n");
		exit(1);
	}

	if (vidmode_ext) {
		int best_fit, best_dist, dist, x, y;
		
		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen) {
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++) {
				if (width > vidmodes[i]->hdisplay ||
					height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = (x * x) + (y * y);
				if (dist < best_dist) {
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1) {
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;

				// change to the mode
				XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
				vidmode_active = true;

				// Move the viewport to top left
				XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
			} else
				fullscreen = 0;
		}
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | 
			CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	} else
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(dpy, root, 0, 0, width, height,
						0, visinfo->depth, InputOutput,
						visinfo->visual, mask, &attr);
	XMapWindow(dpy, win);

	if (vidmode_active) {
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

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	InitSig(); // trap evil signals

	GL_Init();

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	install_grabs();
}

void Sys_SendKeyEvents(void)
{
	HandleEvents();
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
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
	if (!dpy || !win)
		return;

	if (vidmode_active || key_dest == key_game)
		IN_ActivateMouse();
	else
		IN_DeactivateMouse ();
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

        if (!mouse_avail)
                return;

        if (m_filter.value) {
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
                
/*        if (freelook)*/
                V_StopPitchDrift ();

        if (/*freelook && */!(in_strafe.state & 1)) {
                cl.viewangles[PITCH] += m_pitch.value * mouse_y;
                cl.viewangles[PITCH] = bound (-70, cl.viewangles[PITCH], 80);
        } else {
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


