
#include "quakedef.h"

#define MAXRENDERMODULES 64

typedef struct rendermodule_s
{
	int active; // set by start, cleared by shutdown
	char *name;
	void(*start)(void);
	void(*shutdown)(void);
	void(*newmap)(void);
}
rendermodule_t;

rendermodule_t rendermodule[MAXRENDERMODULES];

void R_Modules_Init(void)
{
	int i;
	for (i = 0;i < MAXRENDERMODULES;i++)
		rendermodule[i].name = NULL;
}

void R_RegisterModule(char *name, void(*start)(void), void(*shutdown)(void), void(*newmap)(void))
{
	int i;
	for (i = 0;i < MAXRENDERMODULES;i++)
	{
		if (rendermodule[i].name == NULL)
			break;
		if (!strcmp(name, rendermodule[i].name))
			Sys_Error("R_RegisterModule: module \"%s\" registered twice\n", name);
	}
	if (i >= MAXRENDERMODULES)
		Sys_Error("R_RegisterModule: ran out of renderer module slots (%i)\n", MAXRENDERMODULES);
	rendermodule[i].active = 0;
	rendermodule[i].name = name;
	rendermodule[i].start = start;
	rendermodule[i].shutdown = shutdown;
	rendermodule[i].newmap = newmap;
}

void R_Modules_Start(void)
{
	int i;
	for (i = 0;i < MAXRENDERMODULES;i++)
	{
		if (rendermodule[i].name == NULL)
			continue;
		if (rendermodule[i].active)
			Sys_Error("R_StartModules: module \"%s\" already active\n", rendermodule[i].name);
		rendermodule[i].active = 1;
		rendermodule[i].start();
	}
}

void R_Modules_Shutdown(void)
{
	int i;
	for (i = 0;i < MAXRENDERMODULES;i++)
	{
		if (rendermodule[i].name == NULL)
			continue;
		if (!rendermodule[i].active)
			continue;
		rendermodule[i].active = 0;
		rendermodule[i].shutdown();
	}
}

void R_Modules_Restart(void)
{
	R_Modules_Shutdown();
	R_Modules_Start();
}

void R_Modules_NewMap(void)
{
	int i;
	for (i = 0;i < MAXRENDERMODULES;i++)
	{
		if (rendermodule[i].name == NULL)
			continue;
		if (!rendermodule[i].active)
			continue;
		rendermodule[i].newmap();
	}
}
