// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin list and extension lists can be found here

#include "quakedef.h"
#include "progdefs.h"
#include "clprogdefs.h"
#include "mprogdefs.h"

//============================================================================
// nice helper macros

#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f "wrong parameter count (" #p "expected ) !\n") 

#define e10 0,0,0,0,0,0,0,0,0,0
#define e100 e10,e10,e10,e10,e10,e10,e10,e10,e10,e10
#define e1000 e100,e100,e100,e100,e100,e100,e100,e100,e100,e100

//============================================================================
// Common 

void VM_Cmd_Init(void)
{
}

void VM_Cmd_Reset(void)
{
}

//============================================================================
// Server 

char *vm_sv_extensions = 
""; 

prvm_builtin_t vm_sv_builtins[] = {
0  // to be consistent with the old vm
};

const int vm_sv_numbuiltins = sizeof(vm_sv_builtins) / sizeof(prvm_builtin_t);

void VM_SV_Cmd_Init(void)
{
}

void VM_SV_Cmd_Reset(void)
{
}

//============================================================================
// Client 

char *vm_cl_extensions = 
"";

prvm_builtin_t vm_cl_builtins[] = {
0  // to be consistent with the old vm
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t);

void VM_CL_Cmd_Init(void)
{
}

void VM_CL_Cmd_Reset(void)
{
}

//============================================================================
// Menu 

char *vm_m_extensions = 
"";

// void setkeydest(float dest)
void VM_M_SetKeyDest(void)
{
	VM_SAFEPARMCOUNT(1,VM_M_SetKeyDest);

	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case 0:
		// key_game
		key_dest = key_game;
		break;
	case 2:
		// key_menu
		key_dest = key_menu;
		break;
	case 1:
		// key_message
		// key_dest = key_message
		// break;
	default:
		PRVM_ERROR("VM_M_SetKeyDest: wrong destination %i !\n",prog->globals[OFS_PARM0]);
	}

	return;
}

// float getkeydest(void)
void VM_M_GetKeyDest(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_GetKeyDest);

	// key_game = 0, key_message = 1, key_menu = 2, unknown = 3
	switch(key_dest)
	{
	case key_game:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		break;
	case key_message:
		// not supported
		// PRVM_G_FLOAT(OFS_RETURN) = 1;
		// break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = 3;
	}		
}

prvm_builtin_t vm_m_builtins[] = {
0, // to be consistent with the old vm
e1000,
VM_M_SetKeyDest,
VM_M_GetKeyDest
};

const int vm_m_numbuiltins = sizeof(vm_m_builtins) / sizeof(prvm_builtin_t);

void VM_M_Cmd_Init(void)
{
}

void VM_M_Cmd_Reset(void)
{
}

