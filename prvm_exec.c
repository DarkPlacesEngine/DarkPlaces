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

#include "quakedef.h"
#include "progsvm.h"

char *prvm_opnames[] =
{
"DONE",

"MUL_F",
"MUL_V",
"MUL_FV",
"MUL_VF",

"DIV",

"ADD_F",
"ADD_V",

"SUB_F",
"SUB_V",

"EQ_F",
"EQ_V",
"EQ_S",
"EQ_E",
"EQ_FNC",

"NE_F",
"NE_V",
"NE_S",
"NE_E",
"NE_FNC",

"LE",
"GE",
"LT",
"GT",

"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",
"INDIRECT",

"ADDRESS",

"STORE_F",
"STORE_V",
"STORE_S",
"STORE_ENT",
"STORE_FLD",
"STORE_FNC",

"STOREP_F",
"STOREP_V",
"STOREP_S",
"STOREP_ENT",
"STOREP_FLD",
"STOREP_FNC",

"RETURN",

"NOT_F",
"NOT_V",
"NOT_S",
"NOT_ENT",
"NOT_FNC",

"IF",
"IFNOT",

"CALL0",
"CALL1",
"CALL2",
"CALL3",
"CALL4",
"CALL5",
"CALL6",
"CALL7",
"CALL8",

"STATE",

"GOTO",

"AND",
"OR",

"BITAND",
"BITOR"
};

char *PRVM_GlobalString (int ofs);
char *PRVM_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PRVM_PrintStatement
=================
*/
void PRVM_PrintStatement (dstatement_t *s)
{
	int		i;

	if ( (unsigned)s->op < sizeof(prvm_opnames)/sizeof(prvm_opnames[0]))
	{
		Con_Printf("%s ",  prvm_opnames[s->op]);
		i = strlen(prvm_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Print(" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf("%sbranch %i",PRVM_GlobalString((unsigned short) s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Print(PRVM_GlobalString((unsigned short) s->a));
		Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->b));
	}
	else
	{
		if (s->a)
			Con_Print(PRVM_GlobalString((unsigned short) s->a));
		if (s->b)
			Con_Print(PRVM_GlobalString((unsigned short) s->b));
		if (s->c)
			Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->c));
	}
	Con_Print("\n");
}

/*
============
PRVM_StackTrace
============
*/
void PRVM_StackTrace (void)
{
	mfunction_t	*f;
	int			i;

	prog->stack[prog->depth].s = prog->xstatement;
	prog->stack[prog->depth].f = prog->xfunction;
	for (i = prog->depth;i > 0;i--)
	{
		f = prog->stack[i].f;

		if (!f)
			Con_Print("<NULL FUNCTION>\n");
		else
			Con_Printf("%12s : %s : statement %i\n", PRVM_GetString(f->s_file), PRVM_GetString(f->s_name), prog->stack[i].s - f->first_statement);
	}
}


/*
============
PRVM_Profile_f

============
*/
void PRVM_Profile_f (void)
{
	mfunction_t *f, *best;
	int i, num, max/*, howmany*/;

	//howmany = 10;
	//if (Cmd_Argc() == 2)
	//	howmany = atoi(Cmd_Argv(1));
	if(Cmd_Argc() != 2)
	{
		Con_Print("prvm_profile <program name>\n");
		return;
	}
	
	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	Con_Printf( "%s Profile:\n[Profile] [BuiltinProfile] [CallCount]\n", PRVM_NAME );

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<prog->progs->numfunctions ; i++)
		{
			f = &prog->functions[i];
			if (f->profile > max)
			{
				max = f->profile;
				best = f;
			}
		}
		if (best)
		{
			//if (num < howmany)
				Con_Printf("%7i %7i %7i %s\n", best->profile, best->builtinsprofile, best->callcount, PRVM_GetString(best->s_name));
			num++;
			best->profile = 0;
			best->builtinsprofile = 0;
		}
	} while (best);

	PRVM_End;
}

void PRVM_CrashAll()
{
	int i;
	prvm_prog_t *oldprog = prog;

	for(i = 0; i < PRVM_MAXPROGS; i++)
	{
		if(!PRVM_ProgLoaded(i))
			continue;
		PRVM_SetProg(i);
		PRVM_Crash();
	}
	
	prog = oldprog;
}

void PRVM_PrintState(void)
{
	int i;
	if (prog->xfunction)
	{
		for (i = -7; i <= 0;i++)
			if (prog->xstatement + i >= prog->xfunction->first_statement)
				PRVM_PrintStatement (prog->statements + prog->xstatement + i);
	}
	else
		Con_Print("null function executing??\n");
	PRVM_StackTrace ();
}

void PRVM_Crash()
{
	if (prog->depth < 1)
	{
		// kill the stack just to be sure
		prog->depth = 0;
		prog->localstack_used = 0;
		return;
	}

	Con_Printf("QuakeC crash report for %s:\n", PRVM_NAME);
	PRVM_PrintState();

	// dump the stack so host_error can shutdown functions
	prog->depth = 0;
	prog->localstack_used = 0;

}

/*
============================================================================
PRVM_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PRVM_EnterFunction

Returns the new program statement counter
====================
*/
int PRVM_EnterFunction (mfunction_t *f)
{
	int		i, j, c, o;

	if (!f)
		PRVM_ERROR ("PRVM_EnterFunction: NULL function in %s\n", PRVM_NAME);

	prog->stack[prog->depth].s = prog->xstatement;
	prog->stack[prog->depth].f = prog->xfunction;
	prog->depth++;
	if (prog->depth >=PRVM_MAX_STACK_DEPTH)
		PRVM_ERROR ("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (prog->localstack_used + c > PRVM_LOCALSTACK_SIZE)
		PRVM_ERROR ("PRVM_ExecuteProgram: locals stack overflow in %s\n", PRVM_NAME);

	for (i=0 ; i < c ; i++)
		prog->localstack[prog->localstack_used+i] = ((int *)prog->globals)[f->parm_start + i];
	prog->localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)prog->globals)[o] = ((int *)prog->globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	prog->xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PRVM_LeaveFunction
====================
*/
int PRVM_LeaveFunction (void)
{
	int		i, c;

	if (prog->depth <= 0)
		PRVM_ERROR ("prog stack underflow in %s", PRVM_NAME);

	if (!prog->xfunction)
		PRVM_ERROR ("PR_LeaveFunction: NULL function in %s\n", PRVM_NAME);
// restore locals from the stack
	c = prog->xfunction->locals;
	prog->localstack_used -= c;
	if (prog->localstack_used < 0)
		PRVM_ERROR ("PRVM_ExecuteProgram: locals stack underflow in %s\n", PRVM_NAME);

	for (i=0 ; i < c ; i++)
		((int *)prog->globals)[prog->xfunction->parm_start + i] = prog->localstack[prog->localstack_used+i];

// up stack
	prog->depth--;
	prog->xfunction = prog->stack[prog->depth].f;
	return prog->stack[prog->depth].s;
}

void PRVM_Init_Exec(void)
{
	// dump the stack
	prog->depth = 0;
	prog->localstack_used = 0;
	// reset the string table
	// nothing here yet
}

/*
====================
PRVM_ExecuteProgram
====================
*/
// LordHavoc: optimized
#define OPA ((prvm_eval_t *)&prog->globals[(unsigned short) st->a])
#define OPB ((prvm_eval_t *)&prog->globals[(unsigned short) st->b])
#define OPC ((prvm_eval_t *)&prog->globals[(unsigned short) st->c])
extern cvar_t prvm_boundscheck;
extern cvar_t prvm_traceqc;
extern int		PRVM_ED_FindFieldOffset (const char *field);
extern ddef_t*	PRVM_ED_FindGlobal(const char *name); 
void PRVM_ExecuteProgram (func_t fnum, const char *errormessage)
{
	dstatement_t	*st;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		profile, startprofile, cachedpr_trace, exitdepth;

	if (!fnum || fnum >= (unsigned int)prog->progs->numfunctions)
	{
		if (prog->self && PRVM_G_INT(prog->self->ofs))
			PRVM_ED_Print(PRVM_PROG_TO_EDICT(PRVM_G_INT(prog->self->ofs)));
		PRVM_ERROR ("PR_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	prog->trace = prvm_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = prog->depth;

// make a stack frame
	st = &prog->statements[PRVM_EnterFunction (f)];
	startprofile = profile = 0;

chooseexecprogram:
	cachedpr_trace = prog->trace;
	if (prvm_boundscheck.integer)
	{
#define PRVMBOUNDSCHECK 1
		if (prog->trace)
		{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
		}
		else
		{
#undef PRVMTRACE
#include "prvm_execprogram.h"
		}
	}
	else
	{
#undef PRVMBOUNDSCHECK
		if (prog->trace)
		{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
		}
		else
		{
#undef PRVMTRACE
#include "prvm_execprogram.h"
		}
	}
}
