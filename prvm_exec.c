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
"^5DONE",

"MUL_F",
"MUL_V",
"MUL_FV",
"MUL_VF",

"DIV",

"ADD_F",
"ADD_V",

"SUB_F",
"SUB_V",

"^2EQ_F",
"^2EQ_V",
"^2EQ_S",
"^2EQ_E",
"^2EQ_FNC",

"^2NE_F",
"^2NE_V",
"^2NE_S",
"^2NE_E",
"^2NE_FNC",

"^2LE",
"^2GE",
"^2LT",
"^2GT",

"^6FIELD_F",
"^6FIELD_V",
"^6FIELD_S",
"^6FIELD_ENT",
"^6FIELD_FLD",
"^6FIELD_FNC",

"^1ADDRESS",

"STORE_F",
"STORE_V",
"STORE_S",
"STORE_ENT",
"STORE_FLD",
"STORE_FNC",

"^1STOREP_F",
"^1STOREP_V",
"^1STOREP_S",
"^1STOREP_ENT",
"^1STOREP_FLD",
"^1STOREP_FNC",

"^5RETURN",

"^2NOT_F",
"^2NOT_V",
"^2NOT_S",
"^2NOT_ENT",
"^2NOT_FNC",

"^5IF",
"^5IFNOT",

"^3CALL0",
"^3CALL1",
"^3CALL2",
"^3CALL3",
"^3CALL4",
"^3CALL5",
"^3CALL6",
"^3CALL7",
"^3CALL8",

"^1STATE",

"^5GOTO",

"^2AND",
"^2OR",

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
extern cvar_t prvm_statementprofiling;
void PRVM_PrintStatement (dstatement_t *s)
{
	size_t i;
	int opnum = (int)(s - prog->statements);

	Con_Printf("s%i: ", opnum);
	if( prog->statement_linenums )
		Con_Printf( "%s:%i: ", PRVM_GetString( prog->xfunction->s_file ), prog->statement_linenums[ opnum ] );

	if (prvm_statementprofiling.integer)
		Con_Printf("%7.0f ", prog->statement_profile[s - prog->statements]);

	if ( (unsigned)s->op < sizeof(prvm_opnames)/sizeof(prvm_opnames[0]))
	{
		Con_Printf("%s ",  prvm_opnames[s->op]);
		i = strlen(prvm_opnames[s->op]);
		// don't count a preceding color tag when padding the name
		if (prvm_opnames[s->op][0] == STRING_COLOR_TAG)
			i -= 2;
		for ( ; i<10 ; i++)
			Con_Print(" ");
	}
	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf("%s, s%i",PRVM_GlobalString((unsigned short) s->a),(signed short)s->b + opnum);
	else if (s->op == OP_GOTO)
		Con_Printf("s%i",(signed short)s->a + opnum);
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Con_Print(PRVM_GlobalString((unsigned short) s->a));
		Con_Print(", ");
		Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->b));
	}
	else if (s->op == OP_ADDRESS || (unsigned)(s->op - OP_LOAD_F) < 6)
	{
		if (s->a)
			Con_Print(PRVM_GlobalString((unsigned short) s->a));
		if (s->b)
		{
			Con_Print(", ");
			Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->b));
		}
		if (s->c)
		{
			Con_Print(", ");
			Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->c));
		}
	}
	else
	{
		if (s->a)
			Con_Print(PRVM_GlobalString((unsigned short) s->a));
		if (s->b)
		{
			Con_Print(", ");
			Con_Print(PRVM_GlobalString((unsigned short) s->b));
		}
		if (s->c)
		{
			Con_Print(", ");
			Con_Print(PRVM_GlobalStringNoContents((unsigned short) s->c));
		}
	}
	Con_Print("\n");
}

void PRVM_PrintFunctionStatements (const char *name)
{
	int i, firststatement, endstatement;
	mfunction_t *func;
	func = PRVM_ED_FindFunction (name);
	if (!func)
	{
		Con_Printf("%s progs: no function named %s\n", PRVM_NAME, name);
		return;
	}
	firststatement = func->first_statement;
	if (firststatement < 0)
	{
		Con_Printf("%s progs: function %s is builtin #%i\n", PRVM_NAME, name, -firststatement);
		return;
	}

	// find the end statement
	endstatement = prog->progs->numstatements;
	for (i = 0;i < prog->progs->numfunctions;i++)
		if (endstatement > prog->functions[i].first_statement && firststatement < prog->functions[i].first_statement)
			endstatement = prog->functions[i].first_statement;

	// now print the range of statements
	Con_Printf("%s progs: disassembly of function %s (statements %i-%i):\n", PRVM_NAME, name, firststatement, endstatement);
	for (i = firststatement;i < endstatement;i++)
	{
		PRVM_PrintStatement(prog->statements + i);
		prog->statement_profile[i] = 0;
	}
}

/*
============
PRVM_PrintFunction_f

============
*/
void PRVM_PrintFunction_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Con_Printf("usage: prvm_printfunction <program name> <function name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	PRVM_PrintFunctionStatements(Cmd_Argv(2));

	PRVM_End;
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


void PRVM_Profile (int maxfunctions, int mininstructions)
{
	mfunction_t *f, *best;
	int i, num;
	double max;

	Con_Printf( "%s Profile:\n[CallCount] [Statements] [BuiltinCost]\n", PRVM_NAME );

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<prog->progs->numfunctions ; i++)
		{
			f = &prog->functions[i];
			if (max < f->profile + f->builtinsprofile + f->callcount)
			{
				max = f->profile + f->builtinsprofile + f->callcount;
				best = f;
			}
		}
		if (best)
		{
			if (num < maxfunctions && max >= mininstructions)
			{
				if (best->first_statement < 0)
					Con_Printf("%9.0f ----- builtin ----- %s\n", best->callcount, PRVM_GetString(best->s_name));
				else
					Con_Printf("%9.0f %9.0f %9.0f %s\n", best->callcount, best->profile, best->builtinsprofile, PRVM_GetString(best->s_name));
			}
			num++;
			best->profile = 0;
			best->builtinsprofile = 0;
			best->callcount = 0;
		}
	} while (best);
}

/*
============
PRVM_Profile_f

============
*/
void PRVM_Profile_f (void)
{
	int howmany;

	howmany = 1<<30;
	if (Cmd_Argc() == 3)
		howmany = atoi(Cmd_Argv(2));
	else if (Cmd_Argc() != 2)
	{
		Con_Print("prvm_profile <program name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	PRVM_Profile(howmany, 1);

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

extern sizebuf_t vm_tempstringsbuf;
void PRVM_Crash(void)
{
	if (prog == NULL)
		return;

	if( prog->depth > 0 )
	{
		Con_Printf("QuakeC crash report for %s:\n", PRVM_NAME);
		PRVM_PrintState();
	}

	// dump the stack so host_error can shutdown functions
	prog->depth = 0;
	prog->localstack_used = 0;

	// delete all tempstrings (FIXME: is this safe in VM->engine->VM recursion?)
	vm_tempstringsbuf.cursize = 0;

	// reset the prog pointer
	prog = NULL;
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
		PRVM_ERROR ("PRVM_EnterFunction: NULL function in %s", PRVM_NAME);

	prog->stack[prog->depth].s = prog->xstatement;
	prog->stack[prog->depth].f = prog->xfunction;
	prog->depth++;
	if (prog->depth >=PRVM_MAX_STACK_DEPTH)
		PRVM_ERROR ("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (prog->localstack_used + c > PRVM_LOCALSTACK_SIZE)
		PRVM_ERROR ("PRVM_ExecuteProgram: locals stack overflow in %s", PRVM_NAME);

	for (i=0 ; i < c ; i++)
		prog->localstack[prog->localstack_used+i] = ((int *)prog->globals.generic)[f->parm_start + i];
	prog->localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)prog->globals.generic)[o] = ((int *)prog->globals.generic)[OFS_PARM0+i*3+j];
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
		PRVM_ERROR ("PR_LeaveFunction: NULL function in %s", PRVM_NAME);
// restore locals from the stack
	c = prog->xfunction->locals;
	prog->localstack_used -= c;
	if (prog->localstack_used < 0)
		PRVM_ERROR ("PRVM_ExecuteProgram: locals stack underflow in %s", PRVM_NAME);

	for (i=0 ; i < c ; i++)
		((int *)prog->globals.generic)[prog->xfunction->parm_start + i] = prog->localstack[prog->localstack_used+i];

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
#define OPA ((prvm_eval_t *)&prog->globals.generic[(unsigned short) st->a])
#define OPB ((prvm_eval_t *)&prog->globals.generic[(unsigned short) st->b])
#define OPC ((prvm_eval_t *)&prog->globals.generic[(unsigned short) st->c])
extern cvar_t prvm_boundscheck;
extern cvar_t prvm_traceqc;
extern cvar_t prvm_statementprofiling;
extern sizebuf_t vm_tempstringsbuf;
void PRVM_ExecuteProgram (func_t fnum, const char *errormessage)
{
	dstatement_t	*st, *startst;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		jumpcount, cachedpr_trace, exitdepth;
	int		restorevm_tempstringsbuf_cursize;

	if (!fnum || fnum >= (unsigned int)prog->progs->numfunctions)
	{
		if (prog->globaloffsets.self >= 0 && PRVM_GLOBALFIELDVALUE(prog->globaloffsets.self)->edict)
			PRVM_ED_Print(PRVM_PROG_TO_EDICT(PRVM_GLOBALFIELDVALUE(prog->globaloffsets.self)->edict), NULL);
		PRVM_ERROR ("PRVM_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	// after executing this function, delete all tempstrings it created
	restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;

	prog->trace = prvm_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = prog->depth;

// make a stack frame
	st = &prog->statements[PRVM_EnterFunction (f)];
	// save the starting statement pointer for profiling
	// (when the function exits or jumps, the (st - startst) integer value is
	// added to the function's profile counter)
	startst = st;
	// instead of counting instructions, we count jumps
	jumpcount = 0;
	// add one to the callcount of this function because otherwise engine-called functions aren't counted
	prog->xfunction->callcount++;

chooseexecprogram:
	cachedpr_trace = prog->trace;
	if (prvm_statementprofiling.integer)
	{
#define PRVMSTATEMENTPROFILING 1
		if (prvm_boundscheck.integer)
		{
#define PRVMBOUNDSCHECK 1
			if (prog->trace)
			{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
#undef PRVMTRACE
			}
			else
			{
#include "prvm_execprogram.h"
			}
#undef PRVMBOUNDSCHECK
		}
		else
		{
			if (prog->trace)
			{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
#undef PRVMTRACE
			}
			else
			{
#include "prvm_execprogram.h"
			}
		}
#undef PRVMSTATEMENTPROFILING
	}
	else
	{
		if (prvm_boundscheck.integer)
		{
#define PRVMBOUNDSCHECK 1
			if (prog->trace)
			{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
#undef PRVMTRACE
			}
			else
			{
#include "prvm_execprogram.h"
			}
#undef PRVMBOUNDSCHECK
		}
		else
		{
			if (prog->trace)
			{
#define PRVMTRACE 1
#include "prvm_execprogram.h"
#undef PRVMTRACE
			}
			else
			{
#include "prvm_execprogram.h"
			}
		}
	}

cleanup:
	if (developer.integer >= 200 && vm_tempstringsbuf.cursize > restorevm_tempstringsbuf_cursize)
		Con_Printf("PRVM_ExecuteProgram: %s used %i bytes of tempstrings\n", PRVM_GetString(prog->functions[fnum].s_name), vm_tempstringsbuf.cursize - restorevm_tempstringsbuf_cursize);
	// delete tempstrings created by this function
	vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;

	SV_FlushBroadcastMessages();
}
