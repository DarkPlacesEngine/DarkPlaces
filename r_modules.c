
#include "quakedef.h"

typedef struct rendermodule_s
{
	int active; // set by start, cleared by shutdown
	char *name;
	void(*start)();
	void(*shutdown)();
}
rendermodule_t;

rendermodule_t rendermodule[64];

void R_Modules_Init()
{
	int i;
	for (i = 0;i < 64;i++)
		rendermodule[i].name = NULL;
}

void R_RegisterModule(char *name, void(*start)(), void(*shutdown)())
{
	int i;
	for (i = 0;i < 64;i++)
	{
		if (rendermodule[i].name == NULL)
			break;
		if (!strcmp(name, rendermodule[i].name))
			Sys_Error("R_RegisterModule: module \"%s\" registered twice\n", name);
	}
	if (i >= 64)
		Sys_Error("R_RegisterModule: ran out of renderer module slots (64)\n");
	rendermodule[i].active = 0;
	rendermodule[i].name = name;
	rendermodule[i].start = start;
	rendermodule[i].shutdown = shutdown;
}

void R_StartModules ()
{
	int i;
	for (i = 0;i < 64;i++)
	{
		if (rendermodule[i].name == NULL)
			continue;
		if (rendermodule[i].active)
			Sys_Error("R_StartModules: module \"%s\" already active\n", rendermodule[i].name);
		rendermodule[i].active = 1;
		rendermodule[i].start();
	}
}

void R_ShutdownModules ()
{
	int i;
	for (i = 0;i < 64;i++)
	{
		if (rendermodule[i].name == NULL)
			continue;
		if (!rendermodule[i].active)
			continue;
		rendermodule[i].active = 0;
		rendermodule[i].shutdown();
	}
}

void R_Restart ()
{
	R_ShutdownModules();
	R_StartModules();
}
