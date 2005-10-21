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
// sys.h -- non-portable functions

#ifndef SYS_H
#define SYS_H


//
// DLL management
//

// Win32 specific
#ifdef WIN32
# include <windows.h>
typedef HMODULE dllhandle_t;

// Other platforms
#else
  typedef void* dllhandle_t;
#endif

typedef struct dllfunction_s
{
	const char *name;
	void **funcvariable;
}
dllfunction_t;

// "dllnames" is an NULL terminated array of possible names for the DLL you want to load
qboolean Sys_LoadLibrary (const char** dllnames, dllhandle_t* handle, const dllfunction_t *fcts);
void Sys_UnloadLibrary (dllhandle_t* handle);
void* Sys_GetProcAddress (dllhandle_t handle, const char* name);

// called early in Host_Init
void Sys_InitConsole (void);
// called after command system is initialized but before first Con_Print
void Sys_Init_Commands (void);


// returns current timestamp
char *Sys_TimeString(const char *timeformat);

//
// system IO interface (these are the sys functions that need to be implemented in a new driver atm)
//
void Sys_Error (const char *error, ...);
// an error will cause the entire program to exit

void Sys_PrintToTerminal(const char *text);
// (may) output text to terminal which launched program

void Sys_Shutdown (void); //INFO: This is only called by Host_Shutdown so we dont need testing for recursion
void Sys_Quit (void);

double Sys_DoubleTime (void);

char *Sys_ConsoleInput (void);

void Sys_Sleep(int milliseconds);
// called to yield for a little bit so as
// not to hog cpu when paused or debugging

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

char *Sys_GetClipboardData (void);

#endif

