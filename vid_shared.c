
#include "quakedef.h"

// LordHavoc: these are only set in wgl
qboolean isG200 = false; // LordHavoc: the Matrox G200 can't do per pixel alpha, and it uses a D3D driver for GL... ugh...
qboolean isRagePro = false; // LordHavoc: the ATI Rage Pro has limitations with per pixel alpha (the color scaler does not apply to per pixel alpha images...), although not as bad as a G200.

// LordHavoc: GL_ARB_multitexture support
int gl_textureunits;
// LordHavoc: GL_ARB_texture_env_combine or GL_EXT_texture_env_combine support
int gl_combine_extension = false;
// LordHavoc: GL_EXT_compiled_vertex_array support
int gl_supportslockarrays = false;

cvar_t vid_mode = {0, "vid_mode", "0"};
cvar_t vid_mouse = {CVAR_SAVE, "vid_mouse", "1"};
cvar_t vid_fullscreen = {0, "vid_fullscreen", "1"};
cvar_t gl_combine = {0, "gl_combine", "1"};

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

#ifndef WIN32
#include <dlfcn.h>
#endif

static void *prjobj = NULL;

static void gl_getfuncs_begin(void)
{
#ifndef WIN32
	if (prjobj)
		dlclose(prjobj);

	prjobj = dlopen(NULL, RTLD_LAZY);
	if (prjobj == NULL)
	{
		Con_Printf("Unable to open symbol list for main program.\n");
		return;
	}
#endif
}

static void gl_getfuncs_end(void)
{
#ifndef WIN32
	if (prjobj)
	{
		dlclose(prjobj);
		prjobj = NULL;
	}
#endif
}

static void *gl_getfuncaddress(char *name)
{
#ifdef WIN32
	return (void *) wglGetProcAddress(func->name);
#else
	return (void *) dlsym(prjobj, name);
#endif
}

static int gl_checkextension(char *name, gl_extensionfunctionlist_t *funcs, char *disableparm)
{
	gl_extensionfunctionlist_t *func;

	Con_Printf("checking for %s...  ", name);

	for (func = funcs;func && func->name;func++)
		*func->funcvariable = NULL;

	if (disableparm && COM_CheckParm(disableparm))
	{
		Con_Printf("disabled by commandline\n");
		return false;
	}

	if (strstr(gl_extensions, name))
	{
		for (func = funcs;func && func->name != NULL;func++)
		{
			if (!(*func->funcvariable = (void *) gl_getfuncaddress(func->name)))
			{
				Con_Printf("missing function \"%s\"!\n", func->name);
				return false;
			}
		}
		Con_Printf("enabled\n");
		return true;
	}
	else
	{
		Con_Printf("not detected\n");
		return false;
	}
}

void VID_CheckExtensions(void)
{
	Con_Printf("Checking OpenGL extensions...\n");

	gl_getfuncs_begin();

	gl_combine_extension = false;
	gl_supportslockarrays = false;
	gl_textureunits = 1;

	if (gl_checkextension("GL_ARB_multitexture", multitexturefuncs, "-nomtex"))
	{
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
		if (gl_textureunits > 1)
			gl_combine_extension = gl_checkextension("GL_ARB_texture_env_combine", NULL, "-nocombine") || gl_checkextension("GL_EXT_texture_env_combine", NULL, "-nocombine");
		else
			gl_textureunits = 1; // for sanity sake, make sure it's not 0
	}

	gl_supportslockarrays = gl_checkextension("GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, "-nocva");

	gl_getfuncs_end();
}

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
