
#include "quakedef.h"

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// LordHavoc: GL_ARB_multitexture support
int gl_mtexable = false;
// LordHavoc: GL_ARB_texture_env_combine support
int gl_combine_extension = false;
// LordHavoc: GL_EXT_compiled_vertex_array support
int gl_supportslockarrays = false;

cvar_t vid_mode = {0, "vid_mode", "0"};
cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1"};
cvar_t vid_fullscreen = {0, "vid_fullscreen", "1"};
cvar_t gl_combine = {0, "gl_combine", "0"};

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

// GL_EXT_compiled_vertex_array
void (GLAPIENTRY *qglLockArraysEXT) (GLint first, GLint count);
void (GLAPIENTRY *qglUnlockArraysEXT) (void);

typedef struct
{
	char *name;
	void **funcvariable;
}
gl_extensionfunctionlist_t;

typedef struct
{
	char *name;
	gl_extensionfunctionlist_t *funcs;
	int *enablevariable;
	char *disableparm;
}
gl_extensioninfo_t;

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

static gl_extensioninfo_t gl_extensioninfo[] =
{
	{"GL_ARB_multitexture", multitexturefuncs, &gl_mtexable, "-nomtex"},
	{"GL_ARB_texture_env_combine", NULL, &gl_combine_extension, "-nocombine"},
	{"GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, &gl_supportslockarrays, "-nocva"},
	{NULL, NULL, NULL, NULL}
};

#ifndef WIN32
#include <dlfcn.h>
#endif

void VID_CheckExtensions(void)
{
#ifndef WIN32
	void *prjobj;
#endif
	gl_extensioninfo_t *info;
	gl_extensionfunctionlist_t *func;
//	multitexturefuncs[0].funcvariable = (void **)&qglMultiTexCoord2f;
	Con_Printf("Checking OpenGL extensions...\n");
#ifndef WIN32
	if ((prjobj = dlopen(NULL, RTLD_LAZY)) == NULL)
	{
		Con_Printf("Unable to open symbol list for main program.\n");
		return;
	}
#endif
	for (info = gl_extensioninfo;info && info->name;info++)
	{
		*info->enablevariable = false;
		for (func = info->funcs;func && func->name;func++)
			*func->funcvariable = NULL;
		Con_Printf("checking for %s...  ", info->name);
		if (info->disableparm && COM_CheckParm(info->disableparm))
		{
			Con_Printf("disabled by commandline\n");
			continue;
		}
		if (strstr(gl_extensions, info->name))
		{
			for (func = info->funcs;func && func->name != NULL;func++)
			{
#ifdef WIN32
				if (!(*func->funcvariable = (void *) wglGetProcAddress(func->name)))
#else
				if (!(*func->funcvariable = (void *) dlsym(prjobj, func->name)))
#endif
				{
					Con_Printf("missing function \"%s\"!\n", func->name);
					goto missingfunc;
				}
			}
			Con_Printf("enabled\n");
			*info->enablevariable = true;
			missingfunc:;
		}
		else
			Con_Printf("not detected\n");
	}
#ifndef WIN32
	dlclose(prjobj);
#endif
}

/*
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
*/

void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void VID_InitCvars(void)
{
	Cvar_RegisterVariable(&vid_mode);
	Cvar_RegisterVariable(&vid_mouse);
	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_RegisterVariable(&gl_combine);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);
}
