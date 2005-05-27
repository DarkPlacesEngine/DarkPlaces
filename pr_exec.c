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


typedef struct
{
	int				s;
	mfunction_t		*f;
} prstack_t;

#define	MAX_STACK_DEPTH		256
// stacktrace writes into pr_stack[MAX_STACK_DEPTH]
// thus increase the array, so depth wont be overwritten
prstack_t	pr_stack[MAX_STACK_DEPTH+1];
int			pr_depth = 0;

#define	LOCALSTACK_SIZE		2048
int			localstack[LOCALSTACK_SIZE];
int			localstack_used;


int			pr_trace;
mfunction_t	*pr_xfunction;
int			pr_xstatement;


int		pr_argc;

char *pr_opnames[] =
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

char *PR_GlobalString (int ofs);
char *PR_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PR_PrintStatement
=================
*/
void PR_PrintStatement (dstatement_t *s)
{
	int		i;

	if ( (unsigned)s->op < sizeof(pr_opnames)/sizeof(pr_opnames[0]))
	{
		Con_Printf("%s ",  pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i<10 ; i++)
			Con_Print(" ");
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf("%sbranch %i",PR_GlobalString((unsigned short) s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Con_Printf("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Print(PR_GlobalString((unsigned short) s->a));
		Con_Print(PR_GlobalStringNoContents((unsigned short) s->b));
	}
	else
	{
		if (s->a)
			Con_Print(PR_GlobalString((unsigned short) s->a));
		if (s->b)
			Con_Print(PR_GlobalString((unsigned short) s->b));
		if (s->c)
			Con_Print(PR_GlobalStringNoContents((unsigned short) s->c));
	}
	Con_Print("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace (void)
{
	mfunction_t	*f;
	int			i;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	for (i = pr_depth;i > 0;i--)
	{
		f = pr_stack[i].f;

		if (!f)
			Con_Print("<NULL FUNCTION>\n");
		else
			Con_Printf("%12s : %s : statement %i\n", PRVM_GetString(f->s_file), PRVM_GetString(f->s_name), pr_stack[i].s - f->first_statement);
	}
}


/*
============
PR_Profile_f

============
*/
void PR_Profile_f (void)
{
	mfunction_t *f, *best;
	int i, num, max/*, howmany*/;

	if (!sv.active)
	{
		Con_Printf("no server running, can't profile\n");
		return;
	}

	Con_Print( "Server Profile:\n[Profile] [BuiltinProfile] [CallCount]\n" );

	//howmany = 10;
	//if (Cmd_Argc() == 2)
	//	howmany = atoi(Cmd_Argv(1));
	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<progs->numfunctions ; i++)
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
}

void PR_PrintState(void)
{
	int i;
	if (pr_xfunction)
	{
		for (i = -7;i <= 0;i++)
			if (pr_xstatement + i >= pr_xfunction->first_statement)
				PR_PrintStatement (pr_statements + pr_xstatement + i);
	}
	else
		Con_Print("null function executing??\n");
	PR_StackTrace ();
}

void PR_Crash(void)
{
	if (pr_depth > 0)
	{
		Con_Print("QuakeC crash report:\n");
		PR_PrintState();
	}

	// dump the stack so host_error can shutdown functions
	pr_depth = 0;
	localstack_used = 0;
}

/*
============================================================================
PRVM_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int PR_EnterFunction (mfunction_t *f)
{
	int		i, j, c, o;

	if (!f)
		Host_Error ("PR_EnterFunction: NULL function\n");

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		Host_Error ("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
		Host_Error ("PRVM_ExecuteProgram: locals stack overflow\n");

	for (i=0 ; i < c ; i++)
		localstack[localstack_used+i] = ((int *)pr_globals)[f->parm_start + i];
	localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
int PR_LeaveFunction (void)
{
	int		i, c;

	if (pr_depth <= 0)
		Host_Error ("prog stack underflow");

	if (!pr_xfunction)
		Host_Error ("PR_LeaveFunction: NULL function\n");
// restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
		Host_Error ("PRVM_ExecuteProgram: locals stack underflow\n");

	for (i=0 ; i < c ; i++)
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used+i];

// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}

void PR_ReInitStrings (void);
void PR_Execute_ProgsLoaded(void)
{
	// dump the stack
	pr_depth = 0;
	localstack_used = 0;
	// reset the string table
	PR_ReInitStrings();
}

/*
====================
PRVM_ExecuteProgram
====================
*/
// LordHavoc: optimized
#define OPA ((prvm_eval_t *)&pr_globals[(unsigned short) st->a])
#define OPB ((prvm_eval_t *)&pr_globals[(unsigned short) st->b])
#define OPC ((prvm_eval_t *)&pr_globals[(unsigned short) st->c])
extern cvar_t pr_boundscheck;
extern cvar_t pr_traceqc;
void PRVM_ExecuteProgram (func_t fnum, const char *errormessage)
{
	dstatement_t	*st;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		profile, startprofile, cachedpr_trace, exitdepth;

	if (!fnum || fnum >= (unsigned) progs->numfunctions)
	{
		if (prog->globals.server->self)
			ED_Print(PRVM_PROG_TO_EDICT(prog->globals.server->self));
		Host_Error ("PRVM_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	pr_trace = pr_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = pr_depth;

// make a stack frame
	st = &pr_statements[PR_EnterFunction (f)];
	startprofile = profile = 0;

chooseexecprogram:
	cachedpr_trace = pr_trace;
	if (pr_boundscheck.integer)
	{
#define PRBOUNDSCHECK 1
		if (pr_trace)
		{
#define PRTRACE 1
#include "pr_execprogram.h"
		}
		else
		{
#undef PRTRACE
#include "pr_execprogram.h"
		}
	}
	else
	{
#undef PRBOUNDSCHECK
		if (pr_trace)
		{
#define PRTRACE 1
#include "pr_execprogram.h"
		}
		else
		{
#undef PRTRACE
#include "pr_execprogram.h"
		}
	}
}

void PR_ReInitStrings (void)
{
}
