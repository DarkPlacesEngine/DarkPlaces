/*
	vid_agl.c

	Mac OS X OpenGL and input module, using Carbon and AGL

	Copyright (C) 2005  Mathieu Olivier

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <dlfcn.h>
#include <signal.h>
#include <AGL/agl.h>
#include <Carbon/Carbon.h>
#include "quakedef.h"


// Tell startup code that we have a client
int cl_available = true;

// AGL prototypes
AGLPixelFormat (*qaglChoosePixelFormat) (const AGLDevice *gdevs, GLint ndev, const GLint *attribList);
AGLContext (*qaglCreateContext) (AGLPixelFormat pix, AGLContext share);
GLboolean (*qaglDestroyContext) (AGLContext ctx);
void (*qaglDestroyPixelFormat) (AGLPixelFormat pix);
GLboolean (*qaglSetCurrentContext) (AGLContext ctx);
GLboolean (*qaglSetDrawable) (AGLContext ctx, AGLDrawable draw);
GLboolean (*qaglSetFullScreen) (AGLContext ctx, GLsizei width, GLsizei height, GLsizei freq, GLint device);
void (*qaglSwapBuffers) (AGLContext ctx);

static qboolean mouse_avail = true;
static qboolean vid_usingmouse = false;
static float mouse_x, mouse_y;

static qboolean vid_isfullscreen = false;

static int scr_width, scr_height;

static AGLContext context;
static 	WindowRef window;


void VID_GetWindowSize (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

static void IN_Activate( qboolean grab )
{
	if (grab)
	{
		if (!vid_usingmouse && mouse_avail && window)
		{
			Rect winBounds;
			CGPoint winCenter;

			SelectWindow(window);
			HideCursor();

			// Put the mouse cursor at the center of the window
			GetWindowBounds (window, kWindowContentRgn, &winBounds);
			winCenter.x = (winBounds.left + winBounds.right) / 2;
			winCenter.y = (winBounds.top + winBounds.bottom) / 2;
			CGWarpMouseCursorPosition(winCenter);

			// Lock the mouse pointer at its current position
			CGAssociateMouseAndMouseCursorPosition(false);

			mouse_x = mouse_y = 0;
			vid_usingmouse = true;
		}
	}
	else
	{
		if (vid_usingmouse)
		{
			CGAssociateMouseAndMouseCursorPosition(true);
			ShowCursor();

			vid_usingmouse = false;
		}
	}
}

void VID_Finish (void)
{
	qboolean vid_usemouse;

	// handle the mouse state when windowed if that's changed
	vid_usemouse = false;
	if (vid_mouse.integer && !key_consoleactive && !cls.demoplayback)
		vid_usemouse = true;
	if (!vid_activewindow)
		vid_usemouse = false;
	if (vid_isfullscreen)
		vid_usemouse = true;
	IN_Activate(vid_usemouse);

	if (r_render.integer)
	{
		if (r_speeds.integer || gl_finish.integer)
			qglFinish();
		qaglSwapBuffers(context);
	}
}

int VID_SetGamma(unsigned short *ramps)
{
	// TODO
	return false;
}

int VID_GetGamma(unsigned short *ramps)
{
	// TODO
	return false;
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
	VID_RestoreSystemGamma();
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

void VID_Init(void)
{
	InitSig(); // trap evil signals
// COMMANDLINEOPTION: Input: -nomouse disables mouse support (see also vid_mouse cvar)
	if (COM_CheckParm ("-nomouse") || COM_CheckParm("-safe"))
		mouse_avail = false;
}

static void *prjobj = NULL;

static void GL_CloseLibrary(void)
{
	if (prjobj)
		dlclose(prjobj);
	prjobj = NULL;
	gl_driver[0] = 0;
	gl_extensions = "";
	gl_platform = "";
	gl_platformextensions = "";
}

static int GL_OpenLibrary(void)
{
	const char *name = "/System/Library/Frameworks/AGL.framework/AGL";

	Con_Printf("Loading OpenGL driver %s\n", name);
	GL_CloseLibrary();
	if (!(prjobj = dlopen(name, RTLD_LAZY)))
	{
		Con_Printf("Unable to open symbol list for %s\n", name);
		return false;
	}
	strcpy(gl_driver, name);
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	return dlsym(prjobj, name);
}

void VID_Shutdown(void)
{
	if (context == NULL || window == NULL)
		return;

	IN_Activate(false);
	VID_RestoreSystemGamma();

	if (context != NULL)
	{
		qaglDestroyContext(context);
		context = NULL;
	}

	if (window != NULL)
	{
		DisposeWindow(window);
		window = NULL;
	}

	vid_hidden = true;
	vid_isfullscreen = false;

	GL_CloseLibrary();
	Key_ClearStates ();
}

// Since the event handler can be called at any time, we store the events for later processing
static qboolean AsyncEvent_Quitting = false;
static qboolean AsyncEvent_Collapsed = false;
static OSStatus MainWindowEventHandler (EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
	OSStatus err = noErr;

	switch (GetEventKind (event))
	{
		case kEventWindowClosed:
			//Con_Printf(">> kEventWindowClosed (received) <<\n");
			AsyncEvent_Quitting = true;
			break;

		// Docked (start)
		case kEventWindowCollapsing:
			//Con_Printf(">> kEventWindowCollapsing (received) <<\n");
			AsyncEvent_Collapsed = true;
			break;

		// Undocked / restored (end)
		case kEventWindowExpanded:
			//Con_Printf(">> kEventWindowExpanded (received) <<\n");
			AsyncEvent_Collapsed = false;
			break;

		default:
			err = eventNotHandledErr;
			break;
	}

	return err;
}

static void VID_ProcessPendingAsyncEvents (void)
{
	// Collapsed / expanded
	if (AsyncEvent_Collapsed != vid_hidden)
	{
		/*
		if (vid_hidden)
			Con_Printf(">> kEventWindowExpanded (processed) <<\n");
		else
			Con_Printf(">> kEventWindowCollapsing (processed) <<\n");
		*/

		vid_hidden = !vid_hidden;
		vid_activewindow = false;
		VID_RestoreSystemGamma();
	}

	// Closed
	if (AsyncEvent_Quitting)
	{
		//Con_Printf(">> kEventWindowClosed (processed) <<\n");
		Sys_Quit();
	}
}

static void VID_BuildAGLAttrib(GLint *attrib, int stencil, qboolean fullscreen)
{
	*attrib++ = AGL_RGBA;
	*attrib++ = AGL_RED_SIZE;*attrib++ = 1;
	*attrib++ = AGL_GREEN_SIZE;*attrib++ = 1;
	*attrib++ = AGL_BLUE_SIZE;*attrib++ = 1;
	*attrib++ = AGL_DOUBLEBUFFER;
	*attrib++ = AGL_DEPTH_SIZE;*attrib++ = 1;
	// if stencil is enabled, ask for alpha too
	if (stencil)
	{
		*attrib++ = AGL_STENCIL_SIZE;*attrib++ = 8;
		*attrib++ = AGL_ALPHA_SIZE;*attrib++ = 1;
	}
	if (fullscreen)
		*attrib++ = AGL_FULLSCREEN;
	*attrib++ = AGL_NONE;
}

int VID_InitMode(int fullscreen, int width, int height, int bpp)
{
    const EventTypeSpec winEvents[] =
	{
		{ kEventClassWindow, kEventWindowClosed },
		{ kEventClassWindow, kEventWindowCollapsing },
		{ kEventClassWindow, kEventWindowExpanded },
	};
	OSStatus carbonError;
	Rect windowBounds;
	GDHandle screen;
	AGLPixelFormat pixelFormat;
	GLint attributes [32];

	if (!GL_OpenLibrary())
	{
		Con_Printf("Unable to load GL driver\n");
		return false;
	}

	if ((qaglChoosePixelFormat = GL_GetProcAddress("aglChoosePixelFormat")) == NULL
	 || (qaglCreateContext = GL_GetProcAddress("aglCreateContext")) == NULL
	 || (qaglDestroyContext = GL_GetProcAddress("aglDestroyContext")) == NULL
	 || (qaglDestroyPixelFormat = GL_GetProcAddress("aglDestroyPixelFormat")) == NULL
	 || (qaglSetCurrentContext = GL_GetProcAddress("aglSetCurrentContext")) == NULL
	 || (qaglSetDrawable = GL_GetProcAddress("aglSetDrawable")) == NULL
	 || (qaglSetFullScreen = GL_GetProcAddress("aglSetFullScreen")) == NULL
	 || (qaglSwapBuffers = GL_GetProcAddress("aglSwapBuffers")) == NULL
	)
	{
		Con_Printf("AGL functions not found\n");
		ReleaseWindow(window);
		return false;
	}

	// Ignore the events from the previous window
	AsyncEvent_Quitting = false;
	AsyncEvent_Collapsed = false;

	// Create the window
	SetRect(&windowBounds, 0, 0, width, height);
	OffsetRect(&windowBounds, 100, 100);  // move it a bit towards the center of the screen
	carbonError = CreateNewWindow(kDocumentWindowClass, kWindowStandardFloatingAttributes | kWindowStandardHandlerAttribute, &windowBounds, &window);
	if (carbonError != noErr || window == NULL)
	{
		Con_Printf("Unable to create window (error %d)\n", carbonError);
		return false;
	}

	// Set the window title
	CFStringRef windowTitle = CFSTR("DarkPlaces AGL");
	SetWindowTitleWithCFString(window, windowTitle);
	CFRelease(windowTitle);

	// Install the callback function for the window events we can't get
	// through ReceiveNextEvent (i.e. close, collapse, and expand)
	InstallWindowEventHandler (window, NewEventHandlerUPP (MainWindowEventHandler),
							   GetEventTypeCount(winEvents), winEvents, window, NULL);

	screen = GetGWorldDevice(GetWindowPort(window));
	if (screen == NULL)
	{
		Con_Printf("Unable to get GDevice for window\n");
		ReleaseWindow(window);
		return false;
	}

	// Create and set pixel format
	VID_BuildAGLAttrib(attributes, bpp == 32, fullscreen);
	pixelFormat = qaglChoosePixelFormat(&screen, 1, attributes);
	if (pixelFormat == NULL)
	{
		Con_Printf("Unable to create pixel format\n");
		ReleaseWindow(window);
		return false;
	}

	// Set context and show the window
	context = qaglCreateContext(pixelFormat, NULL);
	qaglDestroyPixelFormat(pixelFormat);
	if (context == NULL)
		Sys_Error ("aglCreateContext failed\n");
	if (!qaglSetDrawable(context, GetWindowPort(window)))
		Sys_Error ("aglSetDrawable failed\n");
	if (!qaglSetCurrentContext(context))
		Sys_Error ("aglSetCurrentContext failed\n");

	scr_width = width;
	scr_height = height;

	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
		Sys_Error("glGetString not found in %s", gl_driver);

	if (fullscreen)
	{
		if (!qaglSetFullScreen (context, width, height, 0, 0))
			Sys_Error("aglSetFullScreen failed\n");
		vid_isfullscreen = true;
	}

	gl_renderer = (const char *)qglGetString(GL_RENDERER);
	gl_vendor = (const char *)qglGetString(GL_VENDOR);
	gl_version = (const char *)qglGetString(GL_VERSION);
	gl_extensions = (const char *)qglGetString(GL_EXTENSIONS);
	gl_platform = "AGL";
	gl_videosyncavailable = false;

	vid_usingmouse = false;
	vid_hidden = false;
	vid_activewindow = true;
	GL_Init();

	SelectWindow(window);
	ShowWindow(window);

	return true;
}

static void Handle_KeyMod(UInt32 keymod)
{
	static UInt32 prev_keymod = 0;
	UInt32 modChanges = prev_keymod ^ keymod;

	if ((modChanges & cmdKey) != 0)
	{
		Key_Event(K_AUX1, '\0', (keymod & cmdKey) != 0);
	}
	if ((modChanges & shiftKey) != 0 || (modChanges & rightShiftKey) != 0)
	{
		Key_Event(K_SHIFT, '\0', (keymod & shiftKey) != 0);
	}
	if ((modChanges & alphaLock) != 0)
	{
		Key_Event(K_CAPSLOCK, '\0', (keymod & alphaLock) != 0);
	}
	if ((modChanges & optionKey) != 0 || (modChanges & rightOptionKey) != 0)
	{
		Key_Event(K_ALT, '\0', (keymod & optionKey) != 0);
	}
	if ((modChanges & controlKey) != 0 || (modChanges & rightControlKey) != 0)
	{
		Key_Event(K_CTRL, '\0', (keymod & controlKey) != 0);
	}
	if ((modChanges & kEventKeyModifierNumLockMask) != 0)
	{
		Key_Event(K_NUMLOCK, '\0', (keymod & kEventKeyModifierNumLockMask) != 0);
	}
	if ((modChanges & kEventKeyModifierFnMask) != 0)
	{
		Key_Event(K_AUX2, '\0', (keymod & kEventKeyModifierFnMask) != 0);
	}

	prev_keymod = keymod;
}

static void Handle_Key(qbyte charcode, qboolean keypressed)
{
	unsigned int keycode = 0;
	char ascii = '\0';

	switch (charcode)
	{
		case kHomeCharCode:
			keycode = K_HOME;
			break;
		case kEnterCharCode:
			keycode = K_KP_ENTER;
			break;
		case kEndCharCode:
			keycode = K_END;
			break;
		case kBackspaceCharCode:
			keycode = K_BACKSPACE;
			break;
		case kTabCharCode:
			keycode = K_TAB;
			break;
		case kPageUpCharCode:
			keycode = K_PGUP;
			break;
		case kPageDownCharCode:
			keycode = K_PGDN;
			break;
		case kReturnCharCode:
			keycode = K_ENTER;
			break;
		case kEscapeCharCode:
			keycode = K_ESCAPE;
			break;
		case kLeftArrowCharCode:
			keycode = K_LEFTARROW;
			break;
		case kRightArrowCharCode:
			keycode = K_RIGHTARROW;
			break;
		case kUpArrowCharCode:
			keycode = K_UPARROW;
			break;
		case kDownArrowCharCode :
			keycode = K_DOWNARROW;
			break;
		case kDeleteCharCode:
			keycode = K_DEL;
			break;
		case 191:
			// char 191 is sent by the mouse buttons (?!)
			break;
		default:
			if ('A' <= charcode && charcode <= 'Z')
			{
				keycode = charcode + ('a' - 'A');  // lowercase it
				ascii = charcode;
			}
			else if (32 <= charcode && charcode <= 126)
			{
				keycode = charcode;
				ascii = charcode;
			}
			else
				Con_Printf(">> UNKNOWN charcode: %d <<\n", charcode);
	}

	if (keycode != 0)
		Key_Event(keycode, ascii, keypressed);
}

void Sys_SendKeyEvents(void)
{
	EventRef theEvent;
	EventTargetRef theTarget;

	// Start by processing the asynchronous events we received since the previous frame
	VID_ProcessPendingAsyncEvents();

	theTarget = GetEventDispatcherTarget();
	while (ReceiveNextEvent(0, NULL, kEventDurationNoWait, true, &theEvent) == noErr)
	{
		UInt32 eventClass = GetEventClass(theEvent);
		UInt32 eventKind = GetEventKind(theEvent);

		switch (eventClass)
		{
			case kEventClassMouse:
			{
				EventMouseButton theButton;
				int key;

				switch (eventKind)
				{
					case kEventMouseDown:
					case kEventMouseUp:
						GetEventParameter(theEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof(theButton), NULL, &theButton);
						switch (theButton)
						{
							default:
							case kEventMouseButtonPrimary:
								key = K_MOUSE1;
								//Con_Printf(">> kEventMouseButtonPrimary <<\n");
								break;
							case kEventMouseButtonSecondary:
								key = K_MOUSE2;
								//Con_Printf(">> kEventMouseButtonSecondary <<\n");
								break;
							case kEventMouseButtonTertiary:
								key = K_MOUSE3;
								//Con_Printf(">> kEventMouseButtonTertiary <<\n");
								break;
						}
						Key_Event(key, '\0', eventKind == kEventMouseDown);
						break;

					case kEventMouseMoved:
					{
						HIPoint deltaPos;

						GetEventParameter(theEvent, kEventParamMouseDelta, typeHIPoint, NULL, sizeof(deltaPos), NULL, &deltaPos);
						//Con_Printf(">> kEventMouseMoved (%f, %f) <<\n", deltaPos.x, deltaPos.y);

						mouse_x += deltaPos.x;
						mouse_y += deltaPos.y;
						break;
					}

					case kEventMouseWheelMoved:
					{
						SInt32 delta;
						unsigned int wheelEvent;

						GetEventParameter(theEvent, kEventParamMouseWheelDelta, typeSInt32, NULL, sizeof(delta), NULL, &delta);
						//Con_Printf(">> kEventMouseWheelMoved (delta: %d) <<\n", delta);

						wheelEvent = (delta > 0) ? K_MWHEELUP : K_MWHEELDOWN;
						Key_Event(wheelEvent, 0, true);
						Key_Event(wheelEvent, 0, false);
						break;
					}

					case kEventMouseDragged:
						//Con_Printf(">> kEventMouseDragged <<\n");
						break;

					default:
						Con_Printf (">> kEventClassMouse (UNKNOWN eventKind: %d) <<\n", eventKind);
						break;
				}
			}

			case kEventClassKeyboard:
			{
				char keycode;

				switch (eventKind)
				{
					case kEventRawKeyDown:
						GetEventParameter(theEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(keycode), NULL, &keycode);
						Handle_Key(keycode, true);
						//Con_Printf(">> kEventRawKeyDown (%d) <<\n", keycode);
						break;

					case kEventRawKeyRepeat:
						//Con_Printf(">> kEventRawKeyRepeat <<\n");
						break;

					case kEventRawKeyUp:
						GetEventParameter(theEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(keycode), NULL, &keycode);
						Handle_Key(keycode, false);
						//Con_Printf(">> kEventRawKeyUp (%d) <<\n", keycode);
						break;

					case kEventRawKeyModifiersChanged:
					{
						UInt32 keymod = 0;
						GetEventParameter(theEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(keymod), NULL, &keymod);
						Handle_KeyMod(keymod);
						//Con_Printf(">> kEventRawKeyModifiersChanged (0x%08X) <<\n", keymod);
						break;
					}

					case kEventHotKeyPressed:
						//Con_Printf(">> kEventHotKeyPressed <<\n");
						break;

					case kEventHotKeyReleased:
						//Con_Printf(">> kEventHotKeyReleased <<\n");
						break;

					case kEventMouseWheelMoved:
						//Con_Printf(">> kEventMouseWheelMoved - via a keyboard event (!?) <<\n");
						break;

					default:
						Con_Printf (">> kEventClassKeyboard (UNKNOWN eventKind: %d) <<\n", eventKind);
						break;
				}
				break;
			}

			case kEventClassTextInput:
				Con_Printf(">> kEventClassTextInput (%d) <<\n", eventKind);
				break;

			case kEventClassApplication:
				switch (eventKind)
				{
					case kEventAppActivated :
						//Con_Printf(">> kEventAppActivated <<\n");
						vid_activewindow = true;
						break;
					case kEventAppDeactivated:
						//Con_Printf(">> kEventAppDeactivated <<\n");
						vid_activewindow = false;
						VID_RestoreSystemGamma();
						break;
					case kEventAppQuit:
						//Con_Printf(">> kEventAppQuit <<\n");
						Sys_Quit();
						break;
					case kEventAppActiveWindowChanged:
						//Con_Printf(">> kEventAppActiveWindowChanged <<\n");
						break;
					default:
						Con_Printf(">> kEventClassApplication (UNKNOWN eventKind: %d) <<\n", eventKind);
						break;
				}
				break;

			case kEventClassAppleEvent:
				switch (eventKind)
				{
					case kEventAppleEvent :
						//Con_Printf(">> kEventAppleEvent <<\n");
						break;
					default:
						Con_Printf(">> kEventClassAppleEvent (UNKNOWN eventKind: %d) <<\n", eventKind);
						break;
				}
				break;

			case kEventClassWindow:
				switch (eventKind)
				{
					case kEventWindowUpdate :
						//Con_Printf(">> kEventWindowUpdate <<\n");
						break;
					default:
						Con_Printf(">> kEventClassWindow (UNKNOWN eventKind: %d) <<\n", eventKind);
						break;
				}
				break;

			case kEventClassControl:
				//Con_Printf(">> kEventClassControl (%d) <<\n", eventKind);
				break;

			default:
				/*Con_Printf(">> UNKNOWN eventClass: %c%c%c%c, eventKind: %d <<\n",
							eventClass >> 24, (eventClass >> 16) & 0xFF,
							(eventClass >> 8) & 0xFF, eventClass & 0xFF,
							eventKind);*/
				break;
		}

		SendEventToEventTarget (theEvent, theTarget);
		ReleaseEvent(theEvent);
	}
}

void IN_Move (void)
{
	if (mouse_avail)
	{
		in_mouse_x = mouse_x;
		in_mouse_y = mouse_y;
	}
	mouse_x = 0;
	mouse_y = 0;
}
