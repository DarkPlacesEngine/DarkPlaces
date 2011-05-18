/*
	vid_agl.c

	Mac OS X OpenGL and input module, using Carbon and AGL

	Copyright (C) 2005-2006  Mathieu Olivier

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
#include <OpenGL/OpenGL.h>
#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
#include "quakedef.h"
#include "vid_agl_mackeys.h" // this is SDL/src/video/maccommon/SDL_mackeys.h

#ifndef kCGLCEMPEngine
#define kCGLCEMPEngine 313
#endif

// Tell startup code that we have a client
int cl_available = true;

qboolean vid_supportrefreshrate = true;

// AGL prototypes
AGLPixelFormat (*qaglChoosePixelFormat) (const AGLDevice *gdevs, GLint ndev, const GLint *attribList);
AGLContext (*qaglCreateContext) (AGLPixelFormat pix, AGLContext share);
GLboolean (*qaglDestroyContext) (AGLContext ctx);
void (*qaglDestroyPixelFormat) (AGLPixelFormat pix);
const GLubyte* (*qaglErrorString) (GLenum code);
GLenum (*qaglGetError) (void);
GLboolean (*qaglSetCurrentContext) (AGLContext ctx);
GLboolean (*qaglSetDrawable) (AGLContext ctx, AGLDrawable draw);
GLboolean (*qaglSetFullScreen) (AGLContext ctx, GLsizei width, GLsizei height, GLsizei freq, GLint device);
GLboolean (*qaglSetInteger) (AGLContext ctx, GLenum pname, const GLint *params);
void (*qaglSwapBuffers) (AGLContext ctx);
CGLError (*qCGLEnable) (CGLContextObj ctx, CGLContextEnable pname);
CGLError (*qCGLDisable) (CGLContextObj ctx, CGLContextEnable pname);
CGLContextObj (*qCGLGetCurrentContext) (void);

static qboolean multithreadedgl;
static qboolean mouse_avail = true;
static qboolean vid_usingmouse = false;
static qboolean vid_usinghidecursor = false;
static qboolean vid_usingnoaccel = false;

static qboolean vid_isfullscreen = false;
static qboolean vid_usingvsync = false;

static qboolean sound_active = true;

static cvar_t apple_multithreadedgl = {CVAR_SAVE, "apple_multithreadedgl", "1", "makes use of a second thread for the OpenGL driver (if possible) rather than using the engine thread (note: this is done automatically on most other operating systems)"};
static cvar_t apple_mouse_noaccel = {CVAR_SAVE, "apple_mouse_noaccel", "1", "disables mouse acceleration while DarkPlaces is active"};

static AGLContext context;
static WindowRef window;

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

void VID_SetMouse(qboolean fullscreengrab, qboolean relative, qboolean hidecursor)
{
	if (!mouse_avail || !window)
		relative = hidecursor = false;

	if (relative)
	{
		if(vid_usingmouse && (vid_usingnoaccel != !!apple_mouse_noaccel.integer))
			VID_SetMouse(false, false, false); // ungrab first!
		if (!vid_usingmouse)
		{
			Rect winBounds;
			CGPoint winCenter;

			SelectWindow(window);

			// Put the mouse cursor at the center of the window
			GetWindowBounds (window, kWindowContentRgn, &winBounds);
			winCenter.x = (winBounds.left + winBounds.right) / 2;
			winCenter.y = (winBounds.top + winBounds.bottom) / 2;
			CGWarpMouseCursorPosition(winCenter);

			// Lock the mouse pointer at its current position
			CGAssociateMouseAndMouseCursorPosition(false);

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

			vid_usingmouse = true;
			vid_usingnoaccel = !!apple_mouse_noaccel.integer;
		}
	}
	else
	{
		if (vid_usingmouse)
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

			CGAssociateMouseAndMouseCursorPosition(true);

			vid_usingmouse = false;
		}
	}

	if (vid_usinghidecursor != hidecursor)
	{
		vid_usinghidecursor = hidecursor;
		if (hidecursor)
			CGDisplayHideCursor(CGMainDisplayID());
		else
			CGDisplayShowCursor(CGMainDisplayID());
	}
}

#define GAMMA_TABLE_SIZE 256
void VID_Finish (void)
{
	qboolean vid_usevsync;

	// handle changes of the vsync option
	vid_usevsync = (vid_vsync.integer && !cls.timedemo);
	if (vid_usingvsync != vid_usevsync)
	{
		GLint sync = (vid_usevsync ? 1 : 0);

		if (qaglSetInteger(context, AGL_SWAP_INTERVAL, &sync) == GL_TRUE)
		{
			vid_usingvsync = vid_usevsync;
			Con_DPrintf("Vsync %s\n", vid_usevsync ? "activated" : "deactivated");
		}
		else
			Con_Printf("ERROR: can't %s vsync\n", vid_usevsync ? "activate" : "deactivate");
	}

	if (!vid_hidden)
	{
		if (r_speeds.integer == 2 || gl_finish.integer)
			GL_Finish();
		qaglSwapBuffers(context);
	}
	VID_UpdateGamma(false, GAMMA_TABLE_SIZE);

	if (apple_multithreadedgl.integer)
	{
		if (!multithreadedgl)
		{
			if(qCGLGetCurrentContext && qCGLEnable && qCGLDisable)
			{
				CGLContextObj ctx = qCGLGetCurrentContext();
				CGLError e = qCGLEnable(ctx, kCGLCEMPEngine);
				if(e == kCGLNoError)
					multithreadedgl = true;
				else
				{
					Con_Printf("WARNING: can't enable multithreaded GL, error %d\n", (int) e);
					Cvar_SetValueQuick(&apple_multithreadedgl, 0);
				}
			}
			else
			{
				Con_Printf("WARNING: can't enable multithreaded GL, CGL functions not present\n");
				Cvar_SetValueQuick(&apple_multithreadedgl, 0);
			}
		}
	}
	else
	{
		if (multithreadedgl)
		{
			if(qCGLGetCurrentContext && qCGLEnable && qCGLDisable)
			{
				CGLContextObj ctx = qCGLGetCurrentContext();
				qCGLDisable(ctx, kCGLCEMPEngine);
				multithreadedgl = false;
			}
		}
	}
}

int VID_SetGamma(unsigned short *ramps, int rampsize)
{
	CGGammaValue table_red [GAMMA_TABLE_SIZE];
	CGGammaValue table_green [GAMMA_TABLE_SIZE];
	CGGammaValue table_blue [GAMMA_TABLE_SIZE];
	int i;

	// Convert the unsigned short table into 3 float tables
	for (i = 0; i < rampsize; i++)
		table_red[i] = (float)ramps[i] / 65535.0f;
	for (i = 0; i < rampsize; i++)
		table_green[i] = (float)ramps[i + rampsize] / 65535.0f;
	for (i = 0; i < rampsize; i++)
		table_blue[i] = (float)ramps[i + 2 * rampsize] / 65535.0f;

	if (CGSetDisplayTransferByTable(CGMainDisplayID(), rampsize, table_red, table_green, table_blue) != CGDisplayNoErr)
	{
		Con_Print("VID_SetGamma: ERROR: CGSetDisplayTransferByTable failed!\n");
		return false;
	}

	return true;
}

int VID_GetGamma(unsigned short *ramps, int rampsize)
{
	CGGammaValue table_red [GAMMA_TABLE_SIZE];
	CGGammaValue table_green [GAMMA_TABLE_SIZE];
	CGGammaValue table_blue [GAMMA_TABLE_SIZE];
	CGTableCount actualsize = 0;
	int i;

	// Get the gamma ramps from the system
	if (CGGetDisplayTransferByTable(CGMainDisplayID(), rampsize, table_red, table_green, table_blue, &actualsize) != CGDisplayNoErr)
	{
		Con_Print("VID_GetGamma: ERROR: CGGetDisplayTransferByTable failed!\n");
		return false;
	}
	if (actualsize != (unsigned int)rampsize)
	{
		Con_Printf("VID_GetGamma: ERROR: invalid gamma table size (%u != %u)\n", actualsize, rampsize);
		return false;
	}

	// Convert the 3 float tables into 1 unsigned short table
	for (i = 0; i < rampsize; i++)
		ramps[i] = table_red[i] * 65535.0f;
	for (i = 0; i < rampsize; i++)
		ramps[i + rampsize] = table_green[i] * 65535.0f;
	for (i = 0; i < rampsize; i++)
		ramps[i + 2 * rampsize] = table_blue[i] * 65535.0f;

	return true;
}

void signal_handler(int sig)
{
	printf("Received signal %d, exiting...\n", sig);
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

void VID_Init(void)
{
	InitSig(); // trap evil signals
	Cvar_RegisterVariable(&apple_multithreadedgl);
	Cvar_RegisterVariable(&apple_mouse_noaccel);
// COMMANDLINEOPTION: Input: -nomouse disables mouse support (see also vid_mouse cvar)
	if (COM_CheckParm ("-nomouse"))
		mouse_avail = false;
}

static void *prjobj = NULL;
static void *cglobj = NULL;

static void GL_CloseLibrary(void)
{
	if (cglobj)
		dlclose(cglobj);
	cglobj = NULL;
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
	const char *name2 = "/System/Library/Frameworks/OpenGL.framework/OpenGL";

	Con_Printf("Loading OpenGL driver %s\n", name);
	GL_CloseLibrary();
	if (!(prjobj = dlopen(name, RTLD_LAZY)))
	{
		Con_Printf("Unable to open symbol list for %s\n", name);
		return false;
	}
	strlcpy(gl_driver, name, sizeof(gl_driver));

	Con_Printf("Loading OpenGL driver %s\n", name2);
	if (!(cglobj = dlopen(name2, RTLD_LAZY)))
		Con_Printf("Unable to open symbol list for %s; multithreaded GL disabled\n", name);

	return true;
}

void *GL_GetProcAddress(const char *name)
{
	return dlsym(prjobj, name);
}

static void *CGL_GetProcAddress(const char *name)
{
	if(!cglobj)
		return NULL;
	return dlsym(cglobj, name);
}

void VID_Shutdown(void)
{
	if (context == NULL && window == NULL)
		return;

	VID_EnableJoystick(false);
	VID_SetMouse(false, false, false);
	VID_RestoreSystemGamma();

	if (context != NULL)
	{
		qaglDestroyContext(context);
		context = NULL;
	}

	if (vid_isfullscreen)
		CGReleaseAllDisplays();

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
			AsyncEvent_Quitting = true;
			break;

		// Docked (start)
		case kEventWindowCollapsing:
			AsyncEvent_Collapsed = true;
			break;

		// Undocked / restored (end)
		case kEventWindowExpanded:
			AsyncEvent_Collapsed = false;
			break;

		default:
			err = eventNotHandledErr;
			break;
	}

	return err;
}

static void VID_AppFocusChanged(qboolean windowIsActive)
{
	if (vid_activewindow != windowIsActive)
	{
		vid_activewindow = windowIsActive;
		if (!vid_activewindow)
			VID_RestoreSystemGamma();
	}

	if (windowIsActive || !snd_mutewhenidle.integer)
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

static void VID_ProcessPendingAsyncEvents (void)
{
	// Collapsed / expanded
	if (AsyncEvent_Collapsed != vid_hidden)
	{
		vid_hidden = !vid_hidden;
		VID_AppFocusChanged(!vid_hidden);
	}

	// Closed
	if (AsyncEvent_Quitting)
		Sys_Quit(0);
}

static void VID_BuildAGLAttrib(GLint *attrib, qboolean stencil, qboolean fullscreen, qboolean stereobuffer, int samples)
{
	*attrib++ = AGL_RGBA;
	*attrib++ = AGL_RED_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = AGL_GREEN_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = AGL_BLUE_SIZE;*attrib++ = stencil ? 8 : 5;
	*attrib++ = AGL_DOUBLEBUFFER;
	*attrib++ = AGL_DEPTH_SIZE;*attrib++ = stencil ? 24 : 16;

	// if stencil is enabled, ask for alpha too
	if (stencil)
	{
		*attrib++ = AGL_STENCIL_SIZE;*attrib++ = 8;
		*attrib++ = AGL_ALPHA_SIZE;*attrib++ = 8;
	}
	if (fullscreen)
		*attrib++ = AGL_FULLSCREEN;
	if (stereobuffer)
		*attrib++ = AGL_STEREO;
#ifdef AGL_SAMPLE_BUFFERS_ARB
#ifdef AGL_SAMPLES_ARB
	if (samples > 1)
	{
		*attrib++ = AGL_SAMPLE_BUFFERS_ARB;
		*attrib++ = 1;
		*attrib++ = AGL_SAMPLES_ARB;
		*attrib++ = samples;
	}
#endif
#endif

	*attrib++ = AGL_NONE;
}

qboolean VID_InitMode(viddef_mode_t *mode)
{
	const EventTypeSpec winEvents[] =
	{
		{ kEventClassWindow, kEventWindowClosed },
		{ kEventClassWindow, kEventWindowCollapsing },
		{ kEventClassWindow, kEventWindowExpanded },
	};
	OSStatus carbonError;
	Rect windowBounds;
	CFStringRef windowTitle;
	AGLPixelFormat pixelFormat;
	GLint attributes [32];
	GLenum error;

	if (!GL_OpenLibrary())
	{
		Con_Printf("Unable to load GL driver\n");
		return false;
	}

	if ((qaglChoosePixelFormat = (AGLPixelFormat (*) (const AGLDevice *gdevs, GLint ndev, const GLint *attribList))GL_GetProcAddress("aglChoosePixelFormat")) == NULL
	 || (qaglCreateContext = (AGLContext (*) (AGLPixelFormat pix, AGLContext share))GL_GetProcAddress("aglCreateContext")) == NULL
	 || (qaglDestroyContext = (GLboolean (*) (AGLContext ctx))GL_GetProcAddress("aglDestroyContext")) == NULL
	 || (qaglDestroyPixelFormat = (void (*) (AGLPixelFormat pix))GL_GetProcAddress("aglDestroyPixelFormat")) == NULL
	 || (qaglErrorString = (const GLubyte* (*) (GLenum code))GL_GetProcAddress("aglErrorString")) == NULL
	 || (qaglGetError = (GLenum (*) (void))GL_GetProcAddress("aglGetError")) == NULL
	 || (qaglSetCurrentContext = (GLboolean (*) (AGLContext ctx))GL_GetProcAddress("aglSetCurrentContext")) == NULL
	 || (qaglSetDrawable = (GLboolean (*) (AGLContext ctx, AGLDrawable draw))GL_GetProcAddress("aglSetDrawable")) == NULL
	 || (qaglSetFullScreen = (GLboolean (*) (AGLContext ctx, GLsizei width, GLsizei height, GLsizei freq, GLint device))GL_GetProcAddress("aglSetFullScreen")) == NULL
	 || (qaglSetInteger = (GLboolean (*) (AGLContext ctx, GLenum pname, const GLint *params))GL_GetProcAddress("aglSetInteger")) == NULL
	 || (qaglSwapBuffers = (void (*) (AGLContext ctx))GL_GetProcAddress("aglSwapBuffers")) == NULL
	)
	{
		Con_Printf("AGL functions not found\n");
		ReleaseWindow(window);
		return false;
	}

	qCGLEnable = (CGLError (*) (CGLContextObj ctx, CGLContextEnable pname)) CGL_GetProcAddress("CGLEnable");
	qCGLDisable = (CGLError (*) (CGLContextObj ctx, CGLContextEnable pname)) CGL_GetProcAddress("CGLDisable");
	qCGLGetCurrentContext = (CGLContextObj (*) (void)) CGL_GetProcAddress("CGLGetCurrentContext");
	if(!qCGLEnable || !qCGLDisable || !qCGLGetCurrentContext)
		Con_Printf("CGL functions not found; disabling multithreaded OpenGL\n");

	// Ignore the events from the previous window
	AsyncEvent_Quitting = false;
	AsyncEvent_Collapsed = false;

	// Create the window, a bit towards the center of the screen
	windowBounds.left = 100;
	windowBounds.top = 100;
	windowBounds.right = mode->width + 100;
	windowBounds.bottom = mode->height + 100;
	carbonError = CreateNewWindow(kDocumentWindowClass, kWindowStandardFloatingAttributes | kWindowStandardHandlerAttribute, &windowBounds, &window);
	if (carbonError != noErr || window == NULL)
	{
		Con_Printf("Unable to create window (error %u)\n", (unsigned)carbonError);
		return false;
	}

	// Set the window title
	windowTitle = CFSTR("DarkPlaces AGL");
	SetWindowTitleWithCFString(window, windowTitle);

	// Install the callback function for the window events we can't get
	// through ReceiveNextEvent (i.e. close, collapse, and expand)
	InstallWindowEventHandler (window, NewEventHandlerUPP (MainWindowEventHandler),
							   GetEventTypeCount(winEvents), winEvents, window, NULL);

	// Create the desired attribute list
	VID_BuildAGLAttrib(attributes, mode->bitsperpixel == 32, mode->fullscreen, mode->stereobuffer, mode->samples);

	if (!mode->fullscreen)
	{
		// Output to Window
		pixelFormat = qaglChoosePixelFormat(NULL, 0, attributes);
		error = qaglGetError();
		if (error != AGL_NO_ERROR)
		{
			Con_Printf("qaglChoosePixelFormat FAILED: %s\n",
					(char *)qaglErrorString(error));
			ReleaseWindow(window);
			return false;
		}
	}
	else  // Output is fullScreen
	{
		CGDirectDisplayID mainDisplay;
		CFDictionaryRef refDisplayMode;
		GDHandle gdhDisplay;

		// Get the mainDisplay and set resolution to current
		mainDisplay = CGMainDisplayID();
		CGDisplayCapture(mainDisplay);

		// TOCHECK: not sure whether or not it's necessary to change the resolution
		// "by hand", or if aglSetFullscreen does the job anyway
		refDisplayMode = CGDisplayBestModeForParametersAndRefreshRateWithProperty(mainDisplay, mode->bitsperpixel, mode->width, mode->height, mode->refreshrate, kCGDisplayModeIsSafeForHardware, NULL);
		CGDisplaySwitchToMode(mainDisplay, refDisplayMode);
		DMGetGDeviceByDisplayID((DisplayIDType)mainDisplay, &gdhDisplay, false);

		// Set pixel format with built attribs
		// Note: specifying a device is *required* for AGL_FullScreen
		pixelFormat = qaglChoosePixelFormat(&gdhDisplay, 1, attributes);
		error = qaglGetError();
		if (error != AGL_NO_ERROR)
		{
			Con_Printf("qaglChoosePixelFormat FAILED: %s\n",
						(char *)qaglErrorString(error));
			ReleaseWindow(window);
			return false;
		}
	}

	// Create a context using the pform
	context = qaglCreateContext(pixelFormat, NULL);
	error = qaglGetError();
	if (error != AGL_NO_ERROR)
	{
		Con_Printf("qaglCreateContext FAILED: %s\n",
					(char *)qaglErrorString(error));
	}

	// Make the context the current one ('enable' it)
	qaglSetCurrentContext(context);
	error = qaglGetError();
	if (error != AGL_NO_ERROR)
	{
		Con_Printf("qaglSetCurrentContext FAILED: %s\n",
					(char *)qaglErrorString(error));
		ReleaseWindow(window);
		return false;
	}

	// Discard pform
	qaglDestroyPixelFormat(pixelFormat);

	// Attempt fullscreen if requested
	if (mode->fullscreen)
	{
		qaglSetFullScreen (context, mode->width, mode->height, mode->refreshrate, 0);
		error = qaglGetError();
		if (error != AGL_NO_ERROR)
		{
			Con_Printf("qaglSetFullScreen FAILED: %s\n",
						(char *)qaglErrorString(error));
			return false;
		}
	}
	else
	{
		// Set Window as Drawable
		qaglSetDrawable(context, GetWindowPort(window));
		error = qaglGetError();
		if (error != AGL_NO_ERROR)
		{
			Con_Printf("qaglSetDrawable FAILED: %s\n",
						(char *)qaglErrorString(error));
			ReleaseWindow(window);
			return false;
		}
	}

	if ((qglGetString = (const GLubyte* (GLAPIENTRY *)(GLenum name))GL_GetProcAddress("glGetString")) == NULL)
		Sys_Error("glGetString not found in %s", gl_driver);

	gl_platformextensions = "";
	gl_platform = "AGL";

	multithreadedgl = false;
	vid_isfullscreen = mode->fullscreen;
	vid_usingmouse = false;
	vid_usinghidecursor = false;
	vid_hidden = false;
	vid_activewindow = true;
	sound_active = true;
	GL_Init();

	SelectWindow(window);
	ShowWindow(window);

	return true;
}

static void Handle_KeyMod(UInt32 keymod)
{
	const struct keymod_to_event_s { UInt32 keybit; keynum_t event; } keymod_events [] =
	{
		{ cmdKey,						K_AUX1 },
		{ shiftKey,						K_SHIFT },
		{ alphaLock,					K_CAPSLOCK },
		{ optionKey,					K_ALT },
		{ controlKey,					K_CTRL },
		{ kEventKeyModifierNumLockMask,	K_NUMLOCK },
		{ kEventKeyModifierFnMask,		K_AUX2 }
	};
	static UInt32 prev_keymod = 0;
	unsigned int i;
	UInt32 modChanges;

	modChanges = prev_keymod ^ keymod;
	if (modChanges == 0)
		return;

	for (i = 0; i < sizeof(keymod_events) / sizeof(keymod_events[0]); i++)
	{
		UInt32 keybit = keymod_events[i].keybit;

		if ((modChanges & keybit) != 0)
			Key_Event(keymod_events[i].event, '\0', (keymod & keybit) != 0);
	}

	prev_keymod = keymod;
}

static void Handle_Key(unsigned char charcode, UInt32 mackeycode, qboolean keypressed)
{
	unsigned int keycode = 0;
	char ascii = '\0';

	switch (mackeycode)
	{
		case MK_ESCAPE:
			keycode = K_ESCAPE;
			break;
		case MK_F1:
			keycode = K_F1;
			break;
		case MK_F2:
			keycode = K_F2;
			break;
		case MK_F3:
			keycode = K_F3;
			break;
		case MK_F4:
			keycode = K_F4;
			break;
		case MK_F5:
			keycode = K_F5;
			break;
		case MK_F6:
			keycode = K_F6;
			break;
		case MK_F7:
			keycode = K_F7;
			break;
		case MK_F8:
			keycode = K_F8;
			break;
		case MK_F9:
			keycode = K_F9;
			break;
		case MK_F10:
			keycode = K_F10;
			break;
		case MK_F11:
			keycode = K_F11;
			break;
		case MK_F12:
			keycode = K_F12;
			break;
		case MK_SCROLLOCK:
			keycode = K_SCROLLOCK;
			break;
		case MK_PAUSE:
			keycode = K_PAUSE;
			break;
		case MK_BACKSPACE:
			keycode = K_BACKSPACE;
			break;
		case MK_INSERT:
			keycode = K_INS;
			break;
		case MK_HOME:
			keycode = K_HOME;
			break;
		case MK_PAGEUP:
			keycode = K_PGUP;
			break;
		case MK_NUMLOCK:
			keycode = K_NUMLOCK;
			break;
		case MK_KP_EQUALS:
			keycode = K_KP_EQUALS;
			break;
		case MK_KP_DIVIDE:
			keycode = K_KP_DIVIDE;
			break;
		case MK_KP_MULTIPLY:
			keycode = K_KP_MULTIPLY;
			break;
		case MK_TAB:
			keycode = K_TAB;
			break;
		case MK_DELETE:
			keycode = K_DEL;
			break;
		case MK_END:
			keycode = K_END;
			break;
		case MK_PAGEDOWN:
			keycode = K_PGDN;
			break;
		case MK_KP7:
			keycode = K_KP_7;
			break;
		case MK_KP8:
			keycode = K_KP_8;
			break;
		case MK_KP9:
			keycode = K_KP_9;
			break;
		case MK_KP_MINUS:
			keycode = K_KP_MINUS;
			break;
		case MK_CAPSLOCK:
			keycode = K_CAPSLOCK;
			break;
		case MK_RETURN:
			keycode = K_ENTER;
			break;
		case MK_KP4:
			keycode = K_KP_4;
			break;
		case MK_KP5:
			keycode = K_KP_5;
			break;
		case MK_KP6:
			keycode = K_KP_6;
			break;
		case MK_KP_PLUS:
			keycode = K_KP_PLUS;
			break;
		case MK_KP1:
			keycode = K_KP_1;
			break;
		case MK_KP2:
			keycode = K_KP_2;
			break;
		case MK_KP3:
			keycode = K_KP_3;
			break;
		case MK_KP_ENTER:
		case MK_IBOOK_ENTER:
			keycode = K_KP_ENTER;
			break;
		case MK_KP0:
			keycode = K_KP_0;
			break;
		case MK_KP_PERIOD:
			keycode = K_KP_PERIOD;
			break;
		default:
			switch(charcode)
			{
				case kUpArrowCharCode:
					keycode = K_UPARROW;
					break;
				case kLeftArrowCharCode:
					keycode = K_LEFTARROW;
					break;
				case kDownArrowCharCode:
					keycode = K_DOWNARROW;
					break;
				case kRightArrowCharCode:
					keycode = K_RIGHTARROW;
					break;
				case 0:
				case 191:
					// characters 0 and 191 are sent by the mouse buttons (?!)
					break;
				default:
					if ('A' <= charcode && charcode <= 'Z')
					{
						keycode = charcode + ('a' - 'A');  // lowercase it
						ascii = charcode;
					}
					else if (charcode >= 32)
					{
						keycode = charcode;
						ascii = charcode;
					}
					else
						Con_DPrintf(">> UNKNOWN char/keycode: %d/%u <<\n", charcode, (unsigned) mackeycode);
			}
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
								break;
							case kEventMouseButtonSecondary:
								key = K_MOUSE2;
								break;
							case kEventMouseButtonTertiary:
								key = K_MOUSE3;
								break;
						}
						Key_Event(key, '\0', eventKind == kEventMouseDown);
						break;

					// Note: These two events are mutual exclusives
					// Treat MouseDragged in the same statement, so we don't block MouseMoved while a mousebutton is held
					case kEventMouseMoved:
					case kEventMouseDragged:
					{
						HIPoint deltaPos;
						HIPoint windowPos;

						GetEventParameter(theEvent, kEventParamMouseDelta, typeHIPoint, NULL, sizeof(deltaPos), NULL, &deltaPos);
						GetEventParameter(theEvent, kEventParamWindowMouseLocation, typeHIPoint, NULL, sizeof(windowPos), NULL, &windowPos);

						if (vid_usingmouse)
						{
							in_mouse_x += deltaPos.x;
							in_mouse_y += deltaPos.y;
						}

						in_windowmouse_x = windowPos.x;
						in_windowmouse_y = windowPos.y;
						break;
					}

					case kEventMouseWheelMoved:
					{
						SInt32 delta;
						unsigned int wheelEvent;

						GetEventParameter(theEvent, kEventParamMouseWheelDelta, typeSInt32, NULL, sizeof(delta), NULL, &delta);

						wheelEvent = (delta > 0) ? K_MWHEELUP : K_MWHEELDOWN;
						Key_Event(wheelEvent, 0, true);
						Key_Event(wheelEvent, 0, false);
						break;
					}

					default:
						Con_Printf (">> kEventClassMouse (UNKNOWN eventKind: %u) <<\n", (unsigned)eventKind);
						break;
				}
			}

			case kEventClassKeyboard:
			{
				char charcode;
				UInt32 keycode;

				switch (eventKind)
				{
					case kEventRawKeyDown:
						GetEventParameter(theEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(charcode), NULL, &charcode);
						GetEventParameter(theEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(keycode), NULL, &keycode);
						Handle_Key(charcode, keycode, true);
						break;

					case kEventRawKeyRepeat:
						break;

					case kEventRawKeyUp:
						GetEventParameter(theEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(charcode), NULL, &charcode);
						GetEventParameter(theEvent, kEventParamKeyCode, typeUInt32, NULL, sizeof(keycode), NULL, &keycode);
						Handle_Key(charcode, keycode, false);
						break;

					case kEventRawKeyModifiersChanged:
					{
						UInt32 keymod = 0;
						GetEventParameter(theEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(keymod), NULL, &keymod);
						Handle_KeyMod(keymod);
						break;
					}

					case kEventHotKeyPressed:
						break;

					case kEventHotKeyReleased:
						break;

					case kEventMouseWheelMoved:
						break;

					default:
						Con_Printf (">> kEventClassKeyboard (UNKNOWN eventKind: %u) <<\n", (unsigned)eventKind);
						break;
				}
				break;
			}

			case kEventClassTextInput:
				Con_Printf(">> kEventClassTextInput (%d) <<\n", (int)eventKind);
				break;

			case kEventClassApplication:
				switch (eventKind)
				{
					case kEventAppActivated :
						VID_AppFocusChanged(true);
						break;
					case kEventAppDeactivated:
						VID_AppFocusChanged(false);
						break;
					case kEventAppQuit:
						Sys_Quit(0);
						break;
					case kEventAppActiveWindowChanged:
						break;
					default:
						Con_Printf(">> kEventClassApplication (UNKNOWN eventKind: %u) <<\n", (unsigned)eventKind);
						break;
				}
				break;

			case kEventClassAppleEvent:
				switch (eventKind)
				{
					case kEventAppleEvent :
						break;
					default:
						Con_Printf(">> kEventClassAppleEvent (UNKNOWN eventKind: %u) <<\n", (unsigned)eventKind);
						break;
				}
				break;

			case kEventClassWindow:
				switch (eventKind)
				{
					case kEventWindowUpdate :
						break;
					default:
						Con_Printf(">> kEventClassWindow (UNKNOWN eventKind: %u) <<\n", (unsigned)eventKind);
						break;
				}
				break;

			case kEventClassControl:
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

	Cvar_SetValueQuick(&joy_active, success ? 1 : 0);
}

void IN_Move (void)
{
	vid_joystate_t joystate;
	VID_EnableJoystick(true);
	VID_BuildJoyState(&joystate);
	VID_ApplyJoyState(&joystate);
}

static bool GetDictionaryBoolean(CFDictionaryRef d, const void *key)
{
    CFBooleanRef ref = (CFBooleanRef) CFDictionaryGetValue(d, key);
    if(ref)
        return CFBooleanGetValue(ref);
    return false;
}

long GetDictionaryLong(CFDictionaryRef d, const void *key)
{
	long value = 0;
    CFNumberRef ref = (CFNumberRef) CFDictionaryGetValue(d, key);
    if(ref)
        CFNumberGetValue(ref, kCFNumberLongType, &value);
    return value;
}

size_t VID_ListModes(vid_mode_t *modes, size_t maxcount)
{
	CGDirectDisplayID mainDisplay = CGMainDisplayID();
	CFArrayRef vidmodes = CGDisplayAvailableModes(mainDisplay);
	CFDictionaryRef thismode;
	unsigned int n = CFArrayGetCount(vidmodes);
	unsigned int i;
	size_t k;

	k = 0;
	for(i = 0; i < n; ++i)
	{
		thismode = (CFDictionaryRef) CFArrayGetValueAtIndex(vidmodes, i);
		if(!GetDictionaryBoolean(thismode, kCGDisplayModeIsSafeForHardware))
			continue;

		if(k >= maxcount)
			break;
		modes[k].width = GetDictionaryLong(thismode, kCGDisplayWidth);
		modes[k].height = GetDictionaryLong(thismode, kCGDisplayHeight);
		modes[k].bpp = GetDictionaryLong(thismode, kCGDisplayBitsPerPixel);
		modes[k].refreshrate = GetDictionaryLong(thismode, kCGDisplayRefreshRate);
		modes[k].pixelheight_num = 1;
		modes[k].pixelheight_denom = 1; // OS X doesn't expose this either
		++k;
	}
	return k;
}
