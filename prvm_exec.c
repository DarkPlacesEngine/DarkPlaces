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

const char *prvm_opnames[] =
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



//=============================================================================

/*
=================
PRVM_PrintStatement
=================
*/
extern cvar_t prvm_coverage;
extern cvar_t prvm_statementprofiling;
extern cvar_t prvm_timeprofiling;
static void PRVM_PrintStatement(prvm_prog_t *prog, mstatement_t *s)
{
	size_t i;
	int opnum = (int)(s - prog->statements);
	char valuebuf[MAX_INPUTLINE];

	Con_Printf("s%i: ", opnum);
	if( prog->statement_linenums )
	{
		if ( prog->statement_columnnums )
			Con_Printf( "%s:%i:%i: ", PRVM_GetString( prog, prog->xfunction->s_file ), prog->statement_linenums[ opnum ], prog->statement_columnnums[ opnum ] );
		else
			Con_Printf( "%s:%i: ", PRVM_GetString( prog, prog->xfunction->s_file ), prog->statement_linenums[ opnum ] );
	}

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
	if (s->operand[0] >= 0) Con_Printf(  "%s", PRVM_GlobalString(prog, s->operand[0], valuebuf, sizeof(valuebuf)));
	if (s->operand[1] >= 0) Con_Printf(", %s", PRVM_GlobalString(prog, s->operand[1], valuebuf, sizeof(valuebuf)));
	if (s->operand[2] >= 0) Con_Printf(", %s", PRVM_GlobalString(prog, s->operand[2], valuebuf, sizeof(valuebuf)));
	if (s->jumpabsolute >= 0) Con_Printf(", statement %i", s->jumpabsolute);
	Con_Print("\n");
}

void PRVM_PrintFunctionStatements (prvm_prog_t *prog, const char *name)
{
	int i, firststatement, endstatement;
	mfunction_t *func;
	func = PRVM_ED_FindFunction (prog, name);
	if (!func)
	{
		Con_Printf("%s progs: no function named %s\n", prog->name, name);
		return;
	}
	firststatement = func->first_statement;
	if (firststatement < 0)
	{
		Con_Printf("%s progs: function %s is builtin #%i\n", prog->name, name, -firststatement);
		return;
	}

	// find the end statement
	endstatement = prog->numstatements;
	for (i = 0;i < prog->numfunctions;i++)
		if (endstatement > prog->functions[i].first_statement && firststatement < prog->functions[i].first_statement)
			endstatement = prog->functions[i].first_statement;

	// now print the range of statements
	Con_Printf("%s progs: disassembly of function %s (statements %i-%i, locals %i-%i):\n", prog->name, name, firststatement, endstatement, func->parm_start, func->parm_start + func->locals - 1);
	prog->xfunction = func;
	for (i = firststatement;i < endstatement;i++)
	{
		PRVM_PrintStatement(prog, prog->statements + i);
		if (!(prvm_coverage.integer & 4))
			prog->statement_profile[i] = 0;
	}
	if (prvm_coverage.integer & 4)
		Con_Printf("Collecting statement coverage, not flushing statement profile.\n");
}

/*
============
PRVM_PrintFunction_f

============
*/
void PRVM_PrintFunction_f (void)
{
	prvm_prog_t *prog;
	if (Cmd_Argc() != 3)
	{
		Con_Printf("usage: prvm_printfunction <program name> <function name>\n");
		return;
	}

	if (!(prog = PRVM_FriendlyProgFromString(Cmd_Argv(1))))
		return;

	PRVM_PrintFunctionStatements(prog, Cmd_Argv(2));
}

/*
============
PRVM_StackTrace
============
*/
void PRVM_StackTrace (prvm_prog_t *prog)
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
		{
			if (prog->statement_linenums)
			{
				if (prog->statement_columnnums)
					Con_Printf("%12s:%i:%i : %s : statement %i\n", PRVM_GetString(prog, f->s_file), prog->statement_linenums[prog->stack[i].s], prog->statement_columnnums[prog->stack[i].s], PRVM_GetString(prog, f->s_name), prog->stack[i].s - f->first_statement);
				else
					Con_Printf("%12s:%i : %s : statement %i\n", PRVM_GetString(prog, f->s_file), prog->statement_linenums[prog->stack[i].s], PRVM_GetString(prog, f->s_name), prog->stack[i].s - f->first_statement);
			}
			else
				Con_Printf("%12s : %s : statement %i\n", PRVM_GetString(prog, f->s_file), PRVM_GetString(prog, f->s_name), prog->stack[i].s - f->first_statement);
		}
	}
}

void PRVM_ShortStackTrace(prvm_prog_t *prog, char *buf, size_t bufsize)
{
	mfunction_t	*f;
	int			i;
	char vabuf[1024];

	if(prog)
	{
		dpsnprintf(buf, bufsize, "(%s) ", prog->name);
	}
	else
	{
		strlcpy(buf, "<NO PROG>", bufsize);
		return;
	}

	prog->stack[prog->depth].s = prog->xstatement;
	prog->stack[prog->depth].f = prog->xfunction;
	for (i = prog->depth;i > 0;i--)
	{
		f = prog->stack[i].f;

		if(strlcat(buf,
			f
				? va(vabuf, sizeof(vabuf), "%s:%s(%i) ", PRVM_GetString(prog, f->s_file), PRVM_GetString(prog, f->s_name), prog->stack[i].s - f->first_statement)
				: "<NULL> ",
			bufsize
		) >= bufsize)
			break;
	}
}


static void PRVM_CallProfile (prvm_prog_t *prog)
{
	mfunction_t *f, *best;
	int i;
	double max;
	double sum;
	double newprofiletime;

	Con_Printf( "%s Call Profile:\n", prog->name );

	sum = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<prog->numfunctions ; i++)
		{
			f = &prog->functions[i];
			if (max < f->totaltime)
			{
				max = f->totaltime;
				best = f;
			}
		}
		if (best)
		{
			sum += best->totaltime;
			Con_Printf("%9.4f %s\n", best->totaltime, PRVM_GetString(prog, best->s_name));
			best->totaltime = 0;
		}
	} while (best);

	newprofiletime = Sys_DirtyTime();
	Con_Printf("Total time since last profile reset: %9.4f\n", newprofiletime - prog->profiletime);
	Con_Printf("       - used by QC code of this VM: %9.4f\n", sum);

	prog->profiletime = newprofiletime;
}

void PRVM_Profile (prvm_prog_t *prog, int maxfunctions, double mintime, int sortby)
{
	mfunction_t *f, *best;
	int i, num;
	double max;

	if(!prvm_timeprofiling.integer)
		mintime *= 10000000; // count each statement as about 0.1µs

	if(prvm_timeprofiling.integer)
		Con_Printf( "%s Profile:\n[CallCount]      [Time] [BuiltinTm] [Statement] [BuiltinCt] [TimeTotal] [StmtTotal] [BltnTotal] [self]\n", prog->name );
		//                        12345678901 12345678901 12345678901 12345678901 12345678901 12345678901 12345678901 123.45%
	else
		Con_Printf( "%s Profile:\n[CallCount] [Statement] [BuiltinCt] [StmtTotal] [BltnTotal] [self]\n", prog->name );
		//                        12345678901 12345678901 12345678901 12345678901 12345678901 123.45%

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<prog->numfunctions ; i++)
		{
			f = &prog->functions[i];
			if(prvm_timeprofiling.integer)
			{
				if(sortby)
				{
					if(f->first_statement < 0)
					{
						if (max < f->tprofile)
						{
							max = f->tprofile;
							best = f;
						}
					}
					else
					{
						if (max < f->tprofile_total)
						{
							max = f->tprofile_total;
							best = f;
						}
					}
				}
				else
				{
					if (max < f->tprofile + f->tbprofile)
					{
						max = f->tprofile + f->tbprofile;
						best = f;
					}
				}
			}
			else
			{
				if(sortby)
				{
					if (max < f->profile_total + f->builtinsprofile_total + f->callcount)
					{
						max = f->profile_total + f->builtinsprofile_total + f->callcount;
						best = f;
					}
				}
				else
				{
					if (max < f->profile + f->builtinsprofile + f->callcount)
					{
						max = f->profile + f->builtinsprofile + f->callcount;
						best = f;
					}
				}
			}
		}
		if (best)
		{
			if (num < maxfunctions && max > mintime)
			{
				if(prvm_timeprofiling.integer)
				{
					if (best->first_statement < 0)
						Con_Printf("%11.0f %11.6f ------------- builtin ------------- %11.6f ----------- builtin ----------- %s\n", best->callcount, best->tprofile, best->tprofile, PRVM_GetString(prog, best->s_name));
					//                 %11.6f 12345678901 12345678901 12345678901 %11.6f 12345678901 12345678901 123.45%
					else
						Con_Printf("%11.0f %11.6f %11.6f %11.0f %11.0f %11.6f %11.0f %11.0f %6.2f%% %s\n", best->callcount, best->tprofile, best->tbprofile, best->profile, best->builtinsprofile, best->tprofile_total, best->profile_total, best->builtinsprofile_total, (best->tprofile_total > 0) ? ((best->tprofile) * 100.0 / (best->tprofile_total)) : -99.99, PRVM_GetString(prog, best->s_name));
				}
				else
				{
					if (best->first_statement < 0)
						Con_Printf("%11.0f ----------------------- builtin ----------------------- %s\n", best->callcount, PRVM_GetString(prog, best->s_name));
					//                 12345678901 12345678901 12345678901 12345678901 123.45%
					else
						Con_Printf("%11.0f %11.0f %11.0f %11.0f %11.0f %6.2f%% %s\n", best->callcount, best->profile, best->builtinsprofile, best->profile_total, best->builtinsprofile_total, (best->profile + best->builtinsprofile) * 100.0 / (best->profile_total + best->builtinsprofile_total), PRVM_GetString(prog, best->s_name));
				}
			}
			num++;
			best->profile = 0;
			best->tprofile = 0;
			best->tbprofile = 0;
			best->builtinsprofile = 0;
			best->profile_total = 0;
			best->tprofile_total = 0;
			best->builtinsprofile_total = 0;
			best->callcount = 0;
		}
	} while (best);
}

/*
============
PRVM_CallProfile_f

============
*/
void PRVM_CallProfile_f (void)
{
	prvm_prog_t *prog;
	if (Cmd_Argc() != 2)
	{
		Con_Print("prvm_callprofile <program name>\n");
		return;
	}

	if (!(prog = PRVM_FriendlyProgFromString(Cmd_Argv(1))))
		return;

	PRVM_CallProfile(prog);
}

/*
============
PRVM_Profile_f

============
*/
void PRVM_Profile_f (void)
{
	prvm_prog_t *prog;
	int howmany;

	if (prvm_coverage.integer & 1)
	{
		Con_Printf("Collecting function coverage, cannot profile - sorry!\n");
		return;
	}

	howmany = 1<<30;
	if (Cmd_Argc() == 3)
		howmany = atoi(Cmd_Argv(2));
	else if (Cmd_Argc() != 2)
	{
		Con_Print("prvm_profile <program name>\n");
		return;
	}

	if (!(prog = PRVM_FriendlyProgFromString(Cmd_Argv(1))))
		return;

	PRVM_Profile(prog, howmany, 0, 0);
}

void PRVM_ChildProfile_f (void)
{
	prvm_prog_t *prog;
	int howmany;

	if (prvm_coverage.integer & 1)
	{
		Con_Printf("Collecting function coverage, cannot profile - sorry!\n");
		return;
	}

	howmany = 1<<30;
	if (Cmd_Argc() == 3)
		howmany = atoi(Cmd_Argv(2));
	else if (Cmd_Argc() != 2)
	{
		Con_Print("prvm_childprofile <program name>\n");
		return;
	}

	if (!(prog = PRVM_FriendlyProgFromString(Cmd_Argv(1))))
		return;

	PRVM_Profile(prog, howmany, 0, 1);
}

void PRVM_PrintState(prvm_prog_t *prog, int stack_index)
{
	int i;
	mfunction_t *func = prog->xfunction;
	int st = prog->xstatement;
	if (stack_index > 0 && stack_index <= prog->depth)
	{
		func = prog->stack[prog->depth - stack_index].f;
		st = prog->stack[prog->depth - stack_index].s;
	}
	if (prog->statestring)
	{
		Con_Printf("Caller-provided information: %s\n", prog->statestring);
	}
	if (func)
	{
		for (i = -7; i <= 0;i++)
			if (st + i >= func->first_statement)
				PRVM_PrintStatement(prog, prog->statements + st + i);
	}
	PRVM_StackTrace(prog);
}

extern cvar_t prvm_errordump;
void PRVM_Crash(prvm_prog_t *prog)
{
	char vabuf[1024];
	if (prog == NULL)
		return;
	if (!prog->loaded)
		return;

	PRVM_serverfunction(SV_Shutdown) = 0; // don't call SV_Shutdown on crash

	if( prog->depth > 0 )
	{
		Con_Printf("QuakeC crash report for %s:\n", prog->name);
		PRVM_PrintState(prog, 0);
	}

	if(prvm_errordump.integer)
	{
		// make a savegame
		Host_Savegame_to(prog, va(vabuf, sizeof(vabuf), "crash-%s.dmp", prog->name));
	}

	// dump the stack so host_error can shutdown functions
	prog->depth = 0;
	prog->localstack_used = 0;

	// delete all tempstrings (FIXME: is this safe in VM->engine->VM recursion?)
	prog->tempstringsbuf.cursize = 0;

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
static int PRVM_EnterFunction (prvm_prog_t *prog, mfunction_t *f)
{
	int		i, j, c, o;

	if (!f)
		prog->error_cmd("PRVM_EnterFunction: NULL function in %s", prog->name);

	prog->stack[prog->depth].s = prog->xstatement;
	prog->stack[prog->depth].f = prog->xfunction;
	prog->stack[prog->depth].profile_acc = -f->profile;
	prog->stack[prog->depth].tprofile_acc = -f->tprofile + -f->tbprofile;
	prog->stack[prog->depth].builtinsprofile_acc = -f->builtinsprofile;
	prog->depth++;
	if (prog->depth >=PRVM_MAX_STACK_DEPTH)
		prog->error_cmd("stack overflow");

// save off any locals that the new function steps on
	c = f->locals;
	if (prog->localstack_used + c > PRVM_LOCALSTACK_SIZE)
		prog->error_cmd("PRVM_ExecuteProgram: locals stack overflow in %s", prog->name);

	for (i=0 ; i < c ; i++)
		prog->localstack[prog->localstack_used+i] = prog->globals.ip[f->parm_start + i];
	prog->localstack_used += c;

// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			prog->globals.ip[o] = prog->globals.ip[OFS_PARM0+i*3+j];
			o++;
		}
	}

	++f->recursion;
	prog->xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PRVM_LeaveFunction
====================
*/
static int PRVM_LeaveFunction (prvm_prog_t *prog)
{
	int		i, c;
	mfunction_t *f;

	if (prog->depth <= 0)
		prog->error_cmd("prog stack underflow in %s", prog->name);

	if (!prog->xfunction)
		prog->error_cmd("PR_LeaveFunction: NULL function in %s", prog->name);
// restore locals from the stack
	c = prog->xfunction->locals;
	prog->localstack_used -= c;
	if (prog->localstack_used < 0)
		prog->error_cmd("PRVM_ExecuteProgram: locals stack underflow in %s", prog->name);

	for (i=0 ; i < c ; i++)
		prog->globals.ip[prog->xfunction->parm_start + i] = prog->localstack[prog->localstack_used+i];

// up stack
	prog->depth--;
	f = prog->xfunction;
	--f->recursion;
	prog->xfunction = prog->stack[prog->depth].f;
	prog->stack[prog->depth].profile_acc += f->profile;
	prog->stack[prog->depth].tprofile_acc += f->tprofile + f->tbprofile;
	prog->stack[prog->depth].builtinsprofile_acc += f->builtinsprofile;
	if(prog->depth > 0)
	{
		prog->stack[prog->depth-1].profile_acc += prog->stack[prog->depth].profile_acc;
		prog->stack[prog->depth-1].tprofile_acc += prog->stack[prog->depth].tprofile_acc;
		prog->stack[prog->depth-1].builtinsprofile_acc += prog->stack[prog->depth].builtinsprofile_acc;
	}
	if(!f->recursion)
	{
		// if f is already on the call stack...
		// we cannot add this profile data to it now
		// or we would add it more than once
		// so, let's only add to the function's profile if it is the outermost call
		f->profile_total += prog->stack[prog->depth].profile_acc;
		f->tprofile_total += prog->stack[prog->depth].tprofile_acc;
		f->builtinsprofile_total += prog->stack[prog->depth].builtinsprofile_acc;
	}
	
	return prog->stack[prog->depth].s;
}

void PRVM_Init_Exec(prvm_prog_t *prog)
{
	// dump the stack
	prog->depth = 0;
	prog->localstack_used = 0;
	// reset the string table
	// nothing here yet
}

/*
==================
Coverage
==================
*/
// Note: in these two calls, prog->xfunction is assumed to be sane.
static const char *PRVM_WhereAmI(char *buf, size_t bufsize, prvm_prog_t *prog, mfunction_t *func, int statement)
{
	if (prog->statement_linenums)
	{
		if (prog->statement_columnnums)
			return va(buf, bufsize, "%s:%i:%i(%s, %i)", PRVM_GetString(prog, func->s_file), prog->statement_linenums[statement], prog->statement_columnnums[statement], PRVM_GetString(prog, func->s_name), statement - func->first_statement);
		else
			return va(buf, bufsize, "%s:%i(%s, %i)", PRVM_GetString(prog, func->s_file), prog->statement_linenums[statement], PRVM_GetString(prog, func->s_name), statement - func->first_statement);
	}
	else
		return va(buf, bufsize, "%s(%s, %i)", PRVM_GetString(prog, func->s_file), PRVM_GetString(prog, func->s_name), statement - func->first_statement);
}
static void PRVM_FunctionCoverageEvent(prvm_prog_t *prog, mfunction_t *func)
{
	++prog->functions_covered;
	Con_Printf("prvm_coverage: %s just called %s for the first time. Coverage: %.2f%%.\n", prog->name, PRVM_GetString(prog, func->s_name), prog->functions_covered * 100.0 / prog->numfunctions);
}
void PRVM_ExplicitCoverageEvent(prvm_prog_t *prog, mfunction_t *func, int statement)
{
	char vabuf[128];
	++prog->explicit_covered;
	Con_Printf("prvm_coverage: %s just executed a coverage() statement at %s for the first time. Coverage: %.2f%%.\n", prog->name, PRVM_WhereAmI(vabuf, sizeof(vabuf), prog, func, statement), prog->explicit_covered * 100.0 / prog->numexplicitcoveragestatements);
}
static void PRVM_StatementCoverageEvent(prvm_prog_t *prog, mfunction_t *func, int statement)
{
	char vabuf[128];
	++prog->statements_covered;
	Con_Printf("prvm_coverage: %s just executed a statement at %s for the first time. Coverage: %.2f%%.\n", prog->name, PRVM_WhereAmI(vabuf, sizeof(vabuf), prog, func, statement), prog->statements_covered * 100.0 / prog->numstatements);
}

#ifdef __GNUC__
#define HAVE_COMPUTED_GOTOS 1
#endif

#define OPA ((prvm_eval_t *)&prog->globals.fp[st->operand[0]])
#define OPB ((prvm_eval_t *)&prog->globals.fp[st->operand[1]])
#define OPC ((prvm_eval_t *)&prog->globals.fp[st->operand[2]])
extern cvar_t prvm_traceqc;
extern cvar_t prvm_statementprofiling;
extern qboolean prvm_runawaycheck;

#ifdef PROFILING
#ifdef CONFIG_MENU
/*
====================
MVM_ExecuteProgram
====================
*/
void MVM_ExecuteProgram (prvm_prog_t *prog, func_t fnum, const char *errormessage)
{
	mstatement_t	*st, *startst;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		jumpcount, cachedpr_trace, exitdepth;
	int		restorevm_tempstringsbuf_cursize;
	double  calltime;
	double tm, starttm;
	prvm_vec_t tempfloat;
	// these may become out of date when a builtin is called, and are updated accordingly
	prvm_vec_t *cached_edictsfields = prog->edictsfields;
	unsigned int cached_entityfields = prog->entityfields;
	unsigned int cached_entityfields_3 = prog->entityfields - 3;
	unsigned int cached_entityfieldsarea = prog->entityfieldsarea;
	unsigned int cached_entityfieldsarea_entityfields = prog->entityfieldsarea - prog->entityfields;
	unsigned int cached_entityfieldsarea_3 = prog->entityfieldsarea - 3;
	unsigned int cached_entityfieldsarea_entityfields_3 = prog->entityfieldsarea - prog->entityfields - 3;
	unsigned int cached_max_edicts = prog->max_edicts;
	// these do not change
	mstatement_t *cached_statements = prog->statements;
	qboolean cached_allowworldwrites = prog->allowworldwrites;
	unsigned int cached_flag = prog->flag;

	calltime = Sys_DirtyTime();

	if (!fnum || fnum >= (unsigned int)prog->numfunctions)
	{
		if (PRVM_allglobaledict(self))
			PRVM_ED_Print(prog, PRVM_PROG_TO_EDICT(PRVM_allglobaledict(self)), NULL);
		prog->error_cmd("MVM_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	// after executing this function, delete all tempstrings it created
	restorevm_tempstringsbuf_cursize = prog->tempstringsbuf.cursize;

	prog->trace = prvm_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = prog->depth;

// make a stack frame
	st = &prog->statements[PRVM_EnterFunction(prog, f)];
	// save the starting statement pointer for profiling
	// (when the function exits or jumps, the (st - startst) integer value is
	// added to the function's profile counter)
	startst = st;
	starttm = calltime;
	// instead of counting instructions, we count jumps
	jumpcount = 0;
	// add one to the callcount of this function because otherwise engine-called functions aren't counted
	if (prog->xfunction->callcount++ == 0 && (prvm_coverage.integer & 1))
		PRVM_FunctionCoverageEvent(prog, prog->xfunction);

chooseexecprogram:
	cachedpr_trace = prog->trace;
	if (prog->trace || prog->watch_global_type != ev_void || prog->watch_field_type != ev_void || prog->break_statement >= 0)
	{
#define PRVMSLOWINTERPRETER 1
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
#undef PRVMSLOWINTERPRETER
	}
	else
	{
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
	}

cleanup:
	if (developer_insane.integer && prog->tempstringsbuf.cursize > restorevm_tempstringsbuf_cursize)
		Con_DPrintf("MVM_ExecuteProgram: %s used %i bytes of tempstrings\n", PRVM_GetString(prog, prog->functions[fnum].s_name), prog->tempstringsbuf.cursize - restorevm_tempstringsbuf_cursize);
	// delete tempstrings created by this function
	prog->tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;

	tm = Sys_DirtyTime() - calltime;if (tm < 0 || tm >= 1800) tm = 0;
	f->totaltime += tm;

	if (prog == SVVM_prog)
		SV_FlushBroadcastMessages();
}
#endif

/*
====================
CLVM_ExecuteProgram
====================
*/
void CLVM_ExecuteProgram (prvm_prog_t *prog, func_t fnum, const char *errormessage)
{
	mstatement_t	*st, *startst;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		jumpcount, cachedpr_trace, exitdepth;
	int		restorevm_tempstringsbuf_cursize;
	double  calltime;
	double tm, starttm;
	prvm_vec_t tempfloat;
	// these may become out of date when a builtin is called, and are updated accordingly
	prvm_vec_t *cached_edictsfields = prog->edictsfields;
	unsigned int cached_entityfields = prog->entityfields;
	unsigned int cached_entityfields_3 = prog->entityfields - 3;
	unsigned int cached_entityfieldsarea = prog->entityfieldsarea;
	unsigned int cached_entityfieldsarea_entityfields = prog->entityfieldsarea - prog->entityfields;
	unsigned int cached_entityfieldsarea_3 = prog->entityfieldsarea - 3;
	unsigned int cached_entityfieldsarea_entityfields_3 = prog->entityfieldsarea - prog->entityfields - 3;
	unsigned int cached_max_edicts = prog->max_edicts;
	// these do not change
	mstatement_t *cached_statements = prog->statements;
	qboolean cached_allowworldwrites = prog->allowworldwrites;
	unsigned int cached_flag = prog->flag;

	calltime = Sys_DirtyTime();

	if (!fnum || fnum >= (unsigned int)prog->numfunctions)
	{
		if (PRVM_allglobaledict(self))
			PRVM_ED_Print(prog, PRVM_PROG_TO_EDICT(PRVM_allglobaledict(self)), NULL);
		prog->error_cmd("CLVM_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	// after executing this function, delete all tempstrings it created
	restorevm_tempstringsbuf_cursize = prog->tempstringsbuf.cursize;

	prog->trace = prvm_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = prog->depth;

// make a stack frame
	st = &prog->statements[PRVM_EnterFunction(prog, f)];
	// save the starting statement pointer for profiling
	// (when the function exits or jumps, the (st - startst) integer value is
	// added to the function's profile counter)
	startst = st;
	starttm = calltime;
	// instead of counting instructions, we count jumps
	jumpcount = 0;
	// add one to the callcount of this function because otherwise engine-called functions aren't counted
	if (prog->xfunction->callcount++ == 0 && (prvm_coverage.integer & 1))
		PRVM_FunctionCoverageEvent(prog, prog->xfunction);

chooseexecprogram:
	cachedpr_trace = prog->trace;
	if (prog->trace || prog->watch_global_type != ev_void || prog->watch_field_type != ev_void || prog->break_statement >= 0)
	{
#define PRVMSLOWINTERPRETER 1
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
#undef PRVMSLOWINTERPRETER
	}
	else
	{
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
	}

cleanup:
	if (developer_insane.integer && prog->tempstringsbuf.cursize > restorevm_tempstringsbuf_cursize)
		Con_DPrintf("CLVM_ExecuteProgram: %s used %i bytes of tempstrings\n", PRVM_GetString(prog, prog->functions[fnum].s_name), prog->tempstringsbuf.cursize - restorevm_tempstringsbuf_cursize);
	// delete tempstrings created by this function
	prog->tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;

	tm = Sys_DirtyTime() - calltime;if (tm < 0 || tm >= 1800) tm = 0;
	f->totaltime += tm;

	if (prog == SVVM_prog)
		SV_FlushBroadcastMessages();
}
#endif

/*
====================
SVVM_ExecuteProgram
====================
*/
#ifdef PROFILING
void SVVM_ExecuteProgram (prvm_prog_t *prog, func_t fnum, const char *errormessage)
#else
void PRVM_ExecuteProgram (prvm_prog_t *prog, func_t fnum, const char *errormessage)
#endif
{
	mstatement_t	*st, *startst;
	mfunction_t	*f, *newf;
	prvm_edict_t	*ed;
	prvm_eval_t	*ptr;
	int		jumpcount, cachedpr_trace, exitdepth;
	int		restorevm_tempstringsbuf_cursize;
	double  calltime;
	double tm, starttm;
	prvm_vec_t tempfloat;
	// these may become out of date when a builtin is called, and are updated accordingly
	prvm_vec_t *cached_edictsfields = prog->edictsfields;
	unsigned int cached_entityfields = prog->entityfields;
	unsigned int cached_entityfields_3 = prog->entityfields - 3;
	unsigned int cached_entityfieldsarea = prog->entityfieldsarea;
	unsigned int cached_entityfieldsarea_entityfields = prog->entityfieldsarea - prog->entityfields;
	unsigned int cached_entityfieldsarea_3 = prog->entityfieldsarea - 3;
	unsigned int cached_entityfieldsarea_entityfields_3 = prog->entityfieldsarea - prog->entityfields - 3;
	unsigned int cached_max_edicts = prog->max_edicts;
	// these do not change
	mstatement_t *cached_statements = prog->statements;
	qboolean cached_allowworldwrites = prog->allowworldwrites;
	unsigned int cached_flag = prog->flag;

	calltime = Sys_DirtyTime();

	if (!fnum || fnum >= (unsigned int)prog->numfunctions)
	{
		if (PRVM_allglobaledict(self))
			PRVM_ED_Print(prog, PRVM_PROG_TO_EDICT(PRVM_allglobaledict(self)), NULL);
		prog->error_cmd("SVVM_ExecuteProgram: %s", errormessage);
	}

	f = &prog->functions[fnum];

	// after executing this function, delete all tempstrings it created
	restorevm_tempstringsbuf_cursize = prog->tempstringsbuf.cursize;

	prog->trace = prvm_traceqc.integer;

	// we know we're done when pr_depth drops to this
	exitdepth = prog->depth;

// make a stack frame
	st = &prog->statements[PRVM_EnterFunction(prog, f)];
	// save the starting statement pointer for profiling
	// (when the function exits or jumps, the (st - startst) integer value is
	// added to the function's profile counter)
	startst = st;
	starttm = calltime;
	// instead of counting instructions, we count jumps
	jumpcount = 0;
	// add one to the callcount of this function because otherwise engine-called functions aren't counted
	if (prog->xfunction->callcount++ == 0 && (prvm_coverage.integer & 1))
		PRVM_FunctionCoverageEvent(prog, prog->xfunction);

chooseexecprogram:
	cachedpr_trace = prog->trace;
	if (prog->trace || prog->watch_global_type != ev_void || prog->watch_field_type != ev_void || prog->break_statement >= 0)
	{
#define PRVMSLOWINTERPRETER 1
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
#undef PRVMSLOWINTERPRETER
	}
	else
	{
		if (prvm_timeprofiling.integer)
		{
#define PRVMTIMEPROFILING 1
#include "prvm_execprogram.h"
#undef PRVMTIMEPROFILING
		}
		else
		{
#include "prvm_execprogram.h"
		}
	}

cleanup:
	if (developer_insane.integer && prog->tempstringsbuf.cursize > restorevm_tempstringsbuf_cursize)
		Con_DPrintf("SVVM_ExecuteProgram: %s used %i bytes of tempstrings\n", PRVM_GetString(prog, prog->functions[fnum].s_name), prog->tempstringsbuf.cursize - restorevm_tempstringsbuf_cursize);
	// delete tempstrings created by this function
	prog->tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;

	tm = Sys_DirtyTime() - calltime;if (tm < 0 || tm >= 1800) tm = 0;
	f->totaltime += tm;

	if (prog == SVVM_prog)
		SV_FlushBroadcastMessages();
}
