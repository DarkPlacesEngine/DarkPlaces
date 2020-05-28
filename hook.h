/*
Copyright (C) 2020 Cloudwalk

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

// hook.h

#ifndef HOOK_H
#define HOOK_H

typedef union hook_val_s
{
	intptr_t val;
	uintptr_t uval;
	void *ptr;
	char *str;
	int ival;
	unsigned int uival;
	double fval;
	qboolean bval;
} hook_val_t;

typedef struct hook_s
{
	char *name;
	hook_val_t *(*func)(hook_val_t *hook);
	hook_val_t *arg;
	hook_val_t ret;
	unsigned int argc;
} hook_t;

hook_t *_Hook_Register(hook_t *hook, const char *name, void *func, unsigned int argc);
hook_val_t *_Hook_Call(hook_t *hook, ... );
void Hook_Init(void);
void Hook_Shutdown(void);

// For your convenience
#define Hook_Register(hook, func, argc) _Hook_Register(hook, #hook, func, argc)
#define Hook_Call(hook, ... ) _Hook_Call(hook, __VA_ARGS__, NULL)

#endif