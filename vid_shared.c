
#include "quakedef.h"

// global video state
viddef_t vid;

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// LordHavoc: GL_ARB_multitexture support
int gl_textureunits = 0;
// LordHavoc: GL_ARB_texture_env_combine or GL_EXT_texture_env_combine support
int gl_combine_extension = false;
// LordHavoc: GL_EXT_compiled_vertex_array support
int gl_supportslockarrays = false;
// LordHavoc: GLX_SGI_video_sync and WGL_EXT_swap_control
int gl_videosyncavailable = false;

// LordHavoc: if window is hidden, don't update screen
int vid_hidden = false;
// LordHavoc: if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
int vid_activewindow = true;

cvar_t vid_fullscreen = {0, "vid_fullscreen", "1"};
cvar_t vid_width = {0, "vid_width", "800"};
cvar_t vid_height = {0, "vid_height", "600"};
cvar_t vid_bitsperpixel = {0, "vid_bitsperpixel", "32"};

cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1"};
cvar_t gl_combine = {0, "gl_combine", "1"};

cvar_t in_pitch_min = {0, "in_pitch_min", "-90"};
cvar_t in_pitch_max = {0, "in_pitch_max", "90"};

cvar_t m_filter = {CVAR_SAVE, "m_filter","0"};

// brand of graphics chip
const char *gl_vendor;
// graphics chip model and other information
const char *gl_renderer;
// begins with 1.0.0, 1.1.0, 1.2.0, 1.2.1, 1.3.0, 1.3.1, or 1.4.0
const char *gl_version;
// extensions list, space separated
const char *gl_extensions;
// WGL, GLX, or AGL
const char *gl_platform;
// another extensions list, containing platform-specific extensions that are
// not in the main list
const char *gl_platformextensions;
// name of driver library (opengl32.dll, libGL.so.1, or whatever)
char gl_driver[256];

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

// GL_EXT_compiled_vertex_array
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);


// general GL functions

void (GLAPIENTRY *qglClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void (GLAPIENTRY *qglClear)(GLbitfield mask);

//void (GLAPIENTRY *qglAlphaFunc)(GLenum func, GLclampf ref);
void (GLAPIENTRY *qglBlendFunc)(GLenum sfactor, GLenum dfactor);
void (GLAPIENTRY *qglCullFace)(GLenum mode);

void (GLAPIENTRY *qglDrawBuffer)(GLenum mode);
void (GLAPIENTRY *qglReadBuffer)(GLenum mode);
void (GLAPIENTRY *qglEnable)(GLenum cap);
void (GLAPIENTRY *qglDisable)(GLenum cap);
//GLboolean GLAPIENTRY *qglIsEnabled)(GLenum cap);

void (GLAPIENTRY *qglEnableClientState)(GLenum cap);
void (GLAPIENTRY *qglDisableClientState)(GLenum cap);

void (GLAPIENTRY *qglGetBooleanv)(GLenum pname, GLboolean *params);
void (GLAPIENTRY *qglGetDoublev)(GLenum pname, GLdouble *params);
void (GLAPIENTRY *qglGetFloatv)(GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetIntegerv)(GLenum pname, GLint *params);

GLenum (GLAPIENTRY *qglGetError)(void);
const GLubyte* (GLAPIENTRY *qglGetString)(GLenum name);
void (GLAPIENTRY *qglFinish)(void);
void (GLAPIENTRY *qglFlush)(void);

void (GLAPIENTRY *qglClearDepth)(GLclampd depth);
void (GLAPIENTRY *qglDepthFunc)(GLenum func);
void (GLAPIENTRY *qglDepthMask)(GLboolean flag);
void (GLAPIENTRY *qglDepthRange)(GLclampd near_val, GLclampd far_val);

void (GLAPIENTRY *qglDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void (GLAPIENTRY *qglVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
//void (GLAPIENTRY *qglNormalPointer)(GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
void (GLAPIENTRY *qglArrayElement)(GLint i);

void (GLAPIENTRY *qglColor4ub)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void (GLAPIENTRY *qglTexCoord2f)(GLfloat s, GLfloat t);
void (GLAPIENTRY *qglVertex2f)(GLfloat x, GLfloat y);
void (GLAPIENTRY *qglVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void (GLAPIENTRY *qglBegin)(GLenum mode);
void (GLAPIENTRY *qglEnd)(void);

void (GLAPIENTRY *qglMatrixMode)(GLenum mode);
void (GLAPIENTRY *qglOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglFrustum)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void (GLAPIENTRY *qglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
//void (GLAPIENTRY *qglPushMatrix)(void);
//void (GLAPIENTRY *qglPopMatrix)(void);
void (GLAPIENTRY *qglLoadIdentity)(void);
void (GLAPIENTRY *qglLoadMatrixd)(const GLdouble *m);
void (GLAPIENTRY *qglLoadMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglMultMatrixd)(const GLdouble *m);
//void (GLAPIENTRY *qglMultMatrixf)(const GLfloat *m);
//void (GLAPIENTRY *qglRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglScaled)(GLdouble x, GLdouble y, GLdouble z);
//void (GLAPIENTRY *qglScalef)(GLfloat x, GLfloat y, GLfloat z);
//void (GLAPIENTRY *qglTranslated)(GLdouble x, GLdouble y, GLdouble z);
void (GLAPIENTRY *qglTranslatef)(GLfloat x, GLfloat y, GLfloat z);

void (GLAPIENTRY *qglReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

//void (GLAPIENTRY *qglStencilFunc)(GLenum func, GLint ref, GLuint mask);
//void (GLAPIENTRY *qglStencilMask)(GLuint mask);
//void (GLAPIENTRY *qglStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
//void (GLAPIENTRY *qglClearStencil)(GLint s);

//void (GLAPIENTRY *qglTexEnvf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexEnvi)(GLenum target, GLenum pname, GLint param);

//void (GLAPIENTRY *qglTexParameterf)(GLenum target, GLenum pname, GLfloat param);
void (GLAPIENTRY *qglTexParameteri)(GLenum target, GLenum pname, GLint param);

void (GLAPIENTRY *qglBindTexture)(GLenum target, GLuint texture);
void (GLAPIENTRY *qglTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void (GLAPIENTRY *qglDeleteTextures)(GLsizei n, const GLuint *textures);
void (GLAPIENTRY *qglPixelStoref)(GLenum pname, GLfloat param);
void (GLAPIENTRY *qglPixelStorei)(GLenum pname, GLint param);

void (GLAPIENTRY *qglDrawRangeElementsEXT)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

//void (GLAPIENTRY *qglColorTableEXT)(int, int, int, int, int, const void *);

int GL_CheckExtension(const char *name, const gl_extensionfunctionlist_t *funcs, const char *disableparm, int silent)
{
	int failed = false;
	const gl_extensionfunctionlist_t *func;

	Con_Printf("checking for %s...  ", name);

	for (func = funcs;func && func->name;func++)
		*func->funcvariable = NULL;

	if (disableparm && COM_CheckParm(disableparm))
	{
		Con_Printf("disabled by commandline\n");
		return false;
	}

	if (strstr(gl_extensions, name) || strstr(gl_platformextensions, name) || (strncmp(name, "GL_", 3) && strncmp(name, "WGL_", 4) && strncmp(name, "GLX_", 4) && strncmp(name, "AGL_", 4)))
	{
		for (func = funcs;func && func->name != NULL;func++)
		{
			// functions are cleared before all the extensions are evaluated
			if (!(*func->funcvariable = (void *) GL_GetProcAddress(func->name)))
			{
				if (!silent)
					Con_Printf("missing function \"%s\" - broken driver!\n", func->name);
				failed = true;
			}
		}
		// delay the return so it prints all missing functions
		if (failed)
			return false;
		Con_Printf("enabled\n");
		return true;
	}
	else
	{
		Con_Printf("not detected\n");
		return false;
	}
}

static gl_extensionfunctionlist_t opengl110funcs[] =
{
	{"glClearColor", (void **) &qglClearColor},
	{"glClear", (void **) &qglClear},
//	{"glAlphaFunc", (void **) &qglAlphaFunc},
	{"glBlendFunc", (void **) &qglBlendFunc},
	{"glCullFace", (void **) &qglCullFace},
	{"glDrawBuffer", (void **) &qglDrawBuffer},
	{"glReadBuffer", (void **) &qglReadBuffer},
	{"glEnable", (void **) &qglEnable},
	{"glDisable", (void **) &qglDisable},
//	{"glIsEnabled", (void **) &qglIsEnabled},
	{"glEnableClientState", (void **) &qglEnableClientState},
	{"glDisableClientState", (void **) &qglDisableClientState},
	{"glGetBooleanv", (void **) &qglGetBooleanv},
	{"glGetDoublev", (void **) &qglGetDoublev},
	{"glGetFloatv", (void **) &qglGetFloatv},
	{"glGetIntegerv", (void **) &qglGetIntegerv},
	{"glGetError", (void **) &qglGetError},
	{"glGetString", (void **) &qglGetString},
	{"glFinish", (void **) &qglFinish},
	{"glFlush", (void **) &qglFlush},
	{"glClearDepth", (void **) &qglClearDepth},
	{"glDepthFunc", (void **) &qglDepthFunc},
	{"glDepthMask", (void **) &qglDepthMask},
	{"glDepthRange", (void **) &qglDepthRange},
	{"glDrawElements", (void **) &qglDrawElements},
	{"glVertexPointer", (void **) &qglVertexPointer},
//	{"glNormalPointer", (void **) &qglNormalPointer},
	{"glColorPointer", (void **) &qglColorPointer},
	{"glTexCoordPointer", (void **) &qglTexCoordPointer},
	{"glArrayElement", (void **) &qglArrayElement},
	{"glColor4ub", (void **) &qglColor4ub},
	{"glTexCoord2f", (void **) &qglTexCoord2f},
	{"glVertex2f", (void **) &qglVertex2f},
	{"glVertex3f", (void **) &qglVertex3f},
	{"glBegin", (void **) &qglBegin},
	{"glEnd", (void **) &qglEnd},
	{"glMatrixMode", (void **) &qglMatrixMode},
	{"glOrtho", (void **) &qglOrtho},
	{"glFrustum", (void **) &qglFrustum},
	{"glViewport", (void **) &qglViewport},
//	{"glPushMatrix", (void **) &qglPushMatrix},
//	{"glPopMatrix", (void **) &qglPopMatrix},
	{"glLoadIdentity", (void **) &qglLoadIdentity},
	{"glLoadMatrixd", (void **) &qglLoadMatrixd},
	{"glLoadMatrixf", (void **) &qglLoadMatrixf},
//	{"glMultMatrixd", (void **) &qglMultMatrixd},
//	{"glMultMatrixf", (void **) &qglMultMatrixf},
//	{"glRotated", (void **) &qglRotated},
	{"glRotatef", (void **) &qglRotatef},
//	{"glScaled", (void **) &qglScaled},
//	{"glScalef", (void **) &qglScalef},
//	{"glTranslated", (void **) &qglTranslated},
	{"glTranslatef", (void **) &qglTranslatef},
	{"glReadPixels", (void **) &qglReadPixels},
//	{"glStencilFunc", (void **) &qglStencilFunc},
//	{"glStencilMask", (void **) &qglStencilMask},
//	{"glStencilOp", (void **) &qglStencilOp},
//	{"glClearStencil", (void **) &qglClearStencil},
//	{"glTexEnvf", (void **) &qglTexEnvf},
	{"glTexEnvi", (void **) &qglTexEnvi},
//	{"glTexParameterf", (void **) &qglTexParameterf},
	{"glTexParameteri", (void **) &qglTexParameteri},
	{"glBindTexture", (void **) &qglBindTexture},
	{"glTexImage2D", (void **) &qglTexImage2D},
	{"glTexSubImage2D", (void **) &qglTexSubImage2D},
	{"glDeleteTextures", (void **) &qglDeleteTextures},
	{"glPixelStoref", (void **) &qglPixelStoref},
	{"glPixelStorei", (void **) &qglPixelStorei},
	{NULL, NULL}
};

static gl_extensionfunctionlist_t drawrangeelementsfuncs[] =
{
	{"glDrawRangeElements", (void **) &qglDrawRangeElements},
	{NULL, NULL}
};

static gl_extensionfunctionlist_t drawrangeelementsextfuncs[] =
{
	{"glDrawRangeElementsEXT", (void **) &qglDrawRangeElementsEXT},
	{NULL, NULL}
};

static gl_extensionfunctionlist_t multitexturefuncs[] =
{
	{"glMultiTexCoord2fARB", (void **) &qglMultiTexCoord2f},
	{"glActiveTextureARB", (void **) &qglActiveTexture},
	{"glClientActiveTextureARB", (void **) &qglClientActiveTexture},
	{NULL, NULL}
};

static gl_extensionfunctionlist_t compiledvertexarrayfuncs[] =
{
	{"glLockArraysEXT", (void **) &qglLockArraysEXT},
	{"glUnlockArraysEXT", (void **) &qglUnlockArraysEXT},
	{NULL, NULL}
};

void VID_CheckExtensions(void)
{
	gl_combine_extension = false;
	gl_supportslockarrays = false;
	gl_textureunits = 1;

	if (!GL_CheckExtension("OpenGL 1.1.0", opengl110funcs, NULL, false))
		Sys_Error("OpenGL 1.1.0 functions not found\n");

	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);
	Con_Printf ("%s_EXTENSIONS: %s\n", gl_platform, gl_platformextensions);

	Con_Printf("Checking OpenGL extensions...\n");

	if (!GL_CheckExtension("glDrawRangeElements", drawrangeelementsfuncs, "-nodrawrangeelements", true))
		GL_CheckExtension("GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "-nodrawrangeelements", false);

	if (GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false))
	{
		qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
		if (gl_textureunits > 1)
			gl_combine_extension = GL_CheckExtension("GL_ARB_texture_env_combine", NULL, "-nocombine", false) || GL_CheckExtension("GL_EXT_texture_env_combine", NULL, "-nocombine", false);
		else
		{
			Con_Printf("GL_ARB_multitexture with less than 2 units? - BROKEN DRIVER!\n");
			gl_textureunits = 1; // for sanity sake, make sure it's not 0
		}
	}

	gl_supportslockarrays = GL_CheckExtension("GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "-nocva", false);

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if (qglDrawRangeElements == NULL)
		qglDrawRangeElements = qglDrawRangeElementsEXT;
}

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void IN_PreMove(void)
{
}

void CL_AdjustAngles(void);
void IN_PostMove(void)
{
	// clamp after the move as well to prevent messed up rendering angles
	CL_AdjustAngles();
}

void IN_Mouse(usercmd_t *cmd, float mx, float my)
{
	int mouselook = (in_mlook.state & 1) || freelook.integer;
	float mouse_x, mouse_y;
	static float old_mouse_x = 0, old_mouse_y = 0;

	if (m_filter.integer)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	// LordHavoc: viewzoom affects mouse sensitivity for sniping
	mouse_x *= sensitivity.value * cl.viewzoom;
	mouse_y *= sensitivity.value * cl.viewzoom;

	// Add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe.integer && mouselook))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mouselook)
		V_StopPitchDrift();

	if (mouselook && !(in_strafe.state & 1))
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
}

void VID_InitCvars(void)
{
	int i;

	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&vid_width);
	Cvar_RegisterVariable(&vid_height);
	Cvar_RegisterVariable(&vid_bitsperpixel);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&gl_combine);
	Cvar_RegisterVariable(&in_pitch_min);
	Cvar_RegisterVariable(&in_pitch_max);
	Cvar_RegisterVariable(&m_filter);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);

// interpret command-line parameters
	if ((i = COM_CheckParm("-window")) != 0)
		Cvar_SetValueQuick(&vid_fullscreen, false);
	if ((i = COM_CheckParm("-fullscreen")) != 0)
		Cvar_SetValueQuick(&vid_fullscreen, true);
	if ((i = COM_CheckParm("-width")) != 0)
		Cvar_SetQuick(&vid_width, com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		Cvar_SetQuick(&vid_height, com_argv[i+1]);
	if ((i = COM_CheckParm("-bpp")) != 0)
		Cvar_SetQuick(&vid_bitsperpixel, com_argv[i+1]);
}

extern int VID_InitMode (int fullscreen, int width, int height, int bpp);
int VID_Mode(int fullscreen, int width, int height, int bpp)
{
	if (fullscreen)
		Con_Printf("Video: %dx%dx%d fullscreen\n", width, height, bpp);
	else
		Con_Printf("Video: %dx%d windowed\n", width, height);
	if (VID_InitMode(fullscreen, width, height, bpp))
		return true;
	else
		return false;
}
