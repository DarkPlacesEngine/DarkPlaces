
#include "quakedef.h"

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// LordHavoc: compiled vertex array support
qboolean gl_supportslockarrays = false;
// LordHavoc: ARB multitexture support
qboolean gl_mtexable = false;
int gl_mtex_enum = 0;
// LordHavoc: ARB texture_env_combine support
qboolean gl_combine_extension = false;

cvar_t vid_mode = {0, "vid_mode", "0"};
cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1"};
cvar_t vid_fullscreen = {0, "vid_fullscreen", "1"};
cvar_t gl_combine = {0, "gl_combine", "0"};

void (GLAPIENTRY *qglMTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglSelectTexture) (GLenum);
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void VID_CheckCombine(void)
{
	// LordHavoc: although texture_env_combine doesn't require multiple TMUs
	// (it does require the multitexture extension however), it is useless to
	// darkplaces without multiple TMUs...
	if (gl_mtexable && strstr(gl_extensions, "GL_ARB_texture_env_combine "))
	{
		gl_combine_extension = true;
		Cvar_SetValue("gl_combine", true);
		Con_Printf("GL_ARB_texture_env_combine detected\n");
	}
	else
	{
		gl_combine_extension = false;
		Cvar_SetValue("gl_combine", false);
		Con_Printf("GL_ARB_texture_env_combine not detected\n");
	}
}

void VID_InitCvars(void)
{
	Cvar_RegisterVariable(&vid_mode);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&gl_combine);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);
}
