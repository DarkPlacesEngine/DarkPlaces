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

#include "quakedef.h"
#include "hook.h"

mempool_t *hooks;

hook_t *_Hook_Register(hook_t *hook, const char *name, void *func, unsigned int argc)
{
	if (hook) {
		Con_Printf("Hook %s already registered\n",hook->name);
	} else {
		hook = (hook_t *)Mem_Alloc(hooks, sizeof(hook_t));
		hook->name = Mem_Alloc(hooks, strlen(name) + 1);
		hook->arg = Mem_Alloc(hooks, sizeof(hook_val_t) * argc);

		memcpy(hook->name, name, strlen(name) + 1);
		hook->func = func;
		hook->argc = argc;
	}
	return hook;
}

// Needs NULL pad to know when va_list ends.
hook_val_t *_Hook_Call(hook_t *hook, ... )
{
	uintptr_t arg_ptr; // Align to platform size
	va_list arg_list;
	unsigned int i = 0;

	if(!hook)
		return (hook_val_t *)NULL;

	va_start(arg_list, hook);

	arg_ptr = va_arg(arg_list,intptr_t);

	if((void *)arg_ptr && !hook->argc)
		goto overflow;

	// Loop until we encounter that NULL pad, but stop if we overflow.
	while ((void *)arg_ptr != NULL && i != hook->argc)
	{
		if (i > hook->argc)
			goto overflow;
		hook->arg[i].val = arg_ptr;
		arg_ptr = va_arg(arg_list,intptr_t);
		i++;
	}

	va_end(arg_list);

	// Should be fairly obvious why it's bad if args don't match
	if(i != hook->argc)
		goto underflow;
	// Call it
	hook->ret.uval = (uintptr_t)hook->func(hook->arg);
	
	if (hook->ret.val)
		return &hook->ret;
	return (hook_val_t *)NULL;

underflow:
	Sys_Error("Hook_Call: Attempt to call hook '%s' with incorrect number of arguments. Got %i, expected %i\n", hook->name, i, hook->argc);
overflow:
	Sys_Error("Hook_Call: Stack overflow calling hook '%s' (argc = %u)\n", hook->name, hook->argc);

}

void Hook_Init(void)
{
	hooks = Mem_AllocPool("hooks",0,NULL);
	return;
}