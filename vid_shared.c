
#include "quakedef.h"

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// LordHavoc: compiled vertex array support
qboolean gl_supportslockarrays = false;
// LordHavoc: ARB multitexture support
qboolean gl_mtexable = false;
int gl_mtex_enum = 0;

cvar_t vid_mode = {"vid_mode", "0", false};
cvar_t vid_mouse = {"vid_mouse", "1", true};
cvar_t vid_fullscreen = {"vid_fullscreen", "1"};

void (GLAPIENTRY *qglMTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglSelectTexture) (GLenum);
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void VID_InitCvars(void)
{
	Cvar_RegisterVariable(&vid_mode);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&vid_fullscreen);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);
}
