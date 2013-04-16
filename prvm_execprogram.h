#ifdef PRVMTIMEPROFILING 
#define PreError() \
	prog->xstatement = st - cached_statements; \
	tm = Sys_DirtyTime(); \
	prog->xfunction->profile += (st - startst); \
	prog->xfunction->tprofile += (tm - starttm >= 0 && tm - starttm < 1800) ? (tm - starttm) : 0;
#else
#define PreError() \
	prog->xstatement = st - cached_statements; \
	prog->xfunction->profile += (st - startst);
#endif

// This code isn't #ifdef/#define protectable, don't try.

#if PRVMSLOWINTERPRETER
		{
			if (prog->watch_global_type != ev_void)
			{
				prvm_eval_t *f = PRVM_GLOBALFIELDVALUE(prog->watch_global);
				prog->xstatement = st + 1 - cached_statements;
				PRVM_Watchpoint(prog, 1, "Global watchpoint hit by engine", prog->watch_global_type, &prog->watch_global_value, f);
			}
			if (prog->watch_field_type != ev_void && prog->watch_edict < prog->max_edicts)
			{
				prvm_eval_t *f = PRVM_EDICTFIELDVALUE(prog->edicts + prog->watch_edict, prog->watch_field);
				prog->xstatement = st + 1 - cached_statements;
				PRVM_Watchpoint(prog, 1, "Entityfield watchpoint hit by engine", prog->watch_field_type, &prog->watch_edictfield_value, f);
			}
		}
#endif

		while (1)
		{
			st++;

#if PRVMSLOWINTERPRETER
			if (prog->trace)
				PRVM_PrintStatement(prog, st);
			prog->statement_profile[st - cached_statements]++;
			if (prog->break_statement >= 0)
				if ((st - cached_statements) == prog->break_statement)
				{
					prog->xstatement = st - cached_statements;
					PRVM_Breakpoint(prog, prog->break_stack_index, "Breakpoint hit");
				}
#endif

			switch (st->op)
			{
			case OP_ADD_F:
				OPC->_float = OPA->_float + OPB->_float;
				break;
			case OP_ADD_V:
				OPC->vector[0] = OPA->vector[0] + OPB->vector[0];
				OPC->vector[1] = OPA->vector[1] + OPB->vector[1];
				OPC->vector[2] = OPA->vector[2] + OPB->vector[2];
				break;
			case OP_SUB_F:
				OPC->_float = OPA->_float - OPB->_float;
				break;
			case OP_SUB_V:
				OPC->vector[0] = OPA->vector[0] - OPB->vector[0];
				OPC->vector[1] = OPA->vector[1] - OPB->vector[1];
				OPC->vector[2] = OPA->vector[2] - OPB->vector[2];
				break;
			case OP_MUL_F:
				OPC->_float = OPA->_float * OPB->_float;
				break;
			case OP_MUL_V:
				OPC->_float = OPA->vector[0]*OPB->vector[0] + OPA->vector[1]*OPB->vector[1] + OPA->vector[2]*OPB->vector[2];
				break;
			case OP_MUL_FV:
				tempfloat = OPA->_float;
				OPC->vector[0] = tempfloat * OPB->vector[0];
				OPC->vector[1] = tempfloat * OPB->vector[1];
				OPC->vector[2] = tempfloat * OPB->vector[2];
				break;
			case OP_MUL_VF:
				tempfloat = OPB->_float;
				OPC->vector[0] = tempfloat * OPA->vector[0];
				OPC->vector[1] = tempfloat * OPA->vector[1];
				OPC->vector[2] = tempfloat * OPA->vector[2];
				break;
			case OP_DIV_F:
				if( OPB->_float != 0.0f )
				{
					OPC->_float = OPA->_float / OPB->_float;
				}
				else
				{
					if (developer.integer)
					{
						prog->xfunction->profile += (st - startst);
						startst = st;
						prog->xstatement = st - cached_statements;
						VM_Warning(prog, "Attempted division by zero in %s\n", prog->name );
					}
					OPC->_float = 0.0f;
				}
				break;
			case OP_BITAND:
				OPC->_float = (prvm_int_t)OPA->_float & (prvm_int_t)OPB->_float;
				break;
			case OP_BITOR:
				OPC->_float = (prvm_int_t)OPA->_float | (prvm_int_t)OPB->_float;
				break;
			case OP_GE:
				OPC->_float = OPA->_float >= OPB->_float;
				break;
			case OP_LE:
				OPC->_float = OPA->_float <= OPB->_float;
				break;
			case OP_GT:
				OPC->_float = OPA->_float > OPB->_float;
				break;
			case OP_LT:
				OPC->_float = OPA->_float < OPB->_float;
				break;
			case OP_AND:
				OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) && FLOAT_IS_TRUE_FOR_INT(OPB->_int); // TODO change this back to float, and add AND_I to be used by fteqcc for anything not a float
				break;
			case OP_OR:
				OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) || FLOAT_IS_TRUE_FOR_INT(OPB->_int); // TODO change this back to float, and add OR_I to be used by fteqcc for anything not a float
				break;
			case OP_NOT_F:
				OPC->_float = !FLOAT_IS_TRUE_FOR_INT(OPA->_int);
				break;
			case OP_NOT_V:
				OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
				break;
			case OP_NOT_S:
				OPC->_float = !OPA->string || !*PRVM_GetString(prog, OPA->string);
				break;
			case OP_NOT_FNC:
				OPC->_float = !OPA->function;
				break;
			case OP_NOT_ENT:
				OPC->_float = (OPA->edict == 0);
				break;
			case OP_EQ_F:
				OPC->_float = OPA->_float == OPB->_float;
				break;
			case OP_EQ_V:
				OPC->_float = (OPA->vector[0] == OPB->vector[0]) && (OPA->vector[1] == OPB->vector[1]) && (OPA->vector[2] == OPB->vector[2]);
				break;
			case OP_EQ_S:
				OPC->_float = !strcmp(PRVM_GetString(prog, OPA->string),PRVM_GetString(prog, OPB->string));
				break;
			case OP_EQ_E:
				OPC->_float = OPA->_int == OPB->_int;
				break;
			case OP_EQ_FNC:
				OPC->_float = OPA->function == OPB->function;
				break;
			case OP_NE_F:
				OPC->_float = OPA->_float != OPB->_float;
				break;
			case OP_NE_V:
				OPC->_float = (OPA->vector[0] != OPB->vector[0]) || (OPA->vector[1] != OPB->vector[1]) || (OPA->vector[2] != OPB->vector[2]);
				break;
			case OP_NE_S:
				OPC->_float = strcmp(PRVM_GetString(prog, OPA->string),PRVM_GetString(prog, OPB->string));
				break;
			case OP_NE_E:
				OPC->_float = OPA->_int != OPB->_int;
				break;
			case OP_NE_FNC:
				OPC->_float = OPA->function != OPB->function;
				break;

		//==================
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:		// integers
			case OP_STORE_S:
			case OP_STORE_FNC:		// pointers
				OPB->_int = OPA->_int;
				break;
			case OP_STORE_V:
				OPB->ivector[0] = OPA->ivector[0];
				OPB->ivector[1] = OPA->ivector[1];
				OPB->ivector[2] = OPA->ivector[2];
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:		// integers
			case OP_STOREP_S:
			case OP_STOREP_FNC:		// pointers
				if ((prvm_uint_t)OPB->_int - cached_entityfields >= cached_entityfieldsarea_entityfields)
				{
					if ((prvm_uint_t)OPB->_int >= cached_entityfieldsarea)
					{
						PreError();
						prog->error_cmd("%s attempted to write to an out of bounds edict (%i)", prog->name, (int)OPB->_int);
						goto cleanup;
					}
					if ((prvm_uint_t)OPB->_int < cached_entityfields && !cached_allowworldwrites)
					{
						prog->xstatement = st - cached_statements;
						VM_Warning(prog, "assignment to world.%s (field %i) in %s\n", PRVM_GetString(prog, PRVM_ED_FieldAtOfs(prog, OPB->_int)->s_name), (int)OPB->_int, prog->name);
					}
				}
				ptr = (prvm_eval_t *)(cached_edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_STOREP_V:
				if ((prvm_uint_t)OPB->_int - cached_entityfields > (prvm_uint_t)cached_entityfieldsarea_entityfields_3)
				{
					if ((prvm_uint_t)OPB->_int > cached_entityfieldsarea_3)
					{
						PreError();
						prog->error_cmd("%s attempted to write to an out of bounds edict (%i)", prog->name, (int)OPB->_int);
						goto cleanup;
					}
					if ((prvm_uint_t)OPB->_int < cached_entityfields && !cached_allowworldwrites)
					{
						prog->xstatement = st - cached_statements;
						VM_Warning(prog, "assignment to world.%s (field %i) in %s\n", PRVM_GetString(prog, PRVM_ED_FieldAtOfs(prog, OPB->_int)->s_name), (int)OPB->_int, prog->name);
					}
				}
				ptr = (prvm_eval_t *)(cached_edictsfields + OPB->_int);
				ptr->ivector[0] = OPA->ivector[0];
				ptr->ivector[1] = OPA->ivector[1];
				ptr->ivector[2] = OPA->ivector[2];
				break;

			case OP_ADDRESS:
				if ((prvm_uint_t)OPA->edict >= cached_max_edicts)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to address an out of bounds edict number", prog->name);
					goto cleanup;
				}
				if ((prvm_uint_t)OPB->_int >= cached_entityfields)
				{
					PreError();
					prog->error_cmd("%s attempted to address an invalid field (%i) in an edict", prog->name, (int)OPB->_int);
					goto cleanup;
				}
#if 0
				if (OPA->edict == 0 && !cached_allowworldwrites)
				{
					PreError();
					prog->error_cmd("forbidden assignment to null/world entity in %s", prog->name);
					goto cleanup;
				}
#endif
				OPC->_int = OPA->edict * cached_entityfields + OPB->_int;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
				if ((prvm_uint_t)OPA->edict >= cached_max_edicts)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an out of bounds edict number", prog->name);
					goto cleanup;
				}
				if ((prvm_uint_t)OPB->_int >= cached_entityfields)
				{
					PreError();
					prog->error_cmd("%s attempted to read an invalid field in an edict (%i)", prog->name, (int)OPB->_int);
					goto cleanup;
				}
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((prvm_eval_t *)(ed->fields.ip + OPB->_int))->_int;
				break;

			case OP_LOAD_V:
				if ((prvm_uint_t)OPA->edict >= cached_max_edicts)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an out of bounds edict number", prog->name);
					goto cleanup;
				}
				if ((prvm_uint_t)OPB->_int > cached_entityfields_3)
				{
					PreError();
					prog->error_cmd("%s attempted to read an invalid field in an edict (%i)", prog->name, (int)OPB->_int);
					goto cleanup;
				}
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				ptr = (prvm_eval_t *)(ed->fields.ip + OPB->_int);
				OPC->ivector[0] = ptr->ivector[0];
				OPC->ivector[1] = ptr->ivector[1];
				OPC->ivector[2] = ptr->ivector[2];
				break;

		//==================

			case OP_IFNOT:
				if(!FLOAT_IS_TRUE_FOR_INT(OPA->_int))
				// TODO add an "int-if", and change this one to OPA->_float
				// although mostly unneeded, thanks to the only float being false being 0x0 and 0x80000000 (negative zero)
				// and entity, string, field values can never have that value
				{
					prog->xfunction->profile += (st - startst);
					st = cached_statements + st->jumpabsolute - 1;	// offset the st++
					startst = st;
					// no bounds check needed, it is done when loading progs
					if (++jumpcount == 10000000 && prvm_runawaycheck)
					{
						prog->xstatement = st - cached_statements;
						PRVM_Profile(prog, 1<<30, 1000000, 0);
						prog->error_cmd("%s runaway loop counter hit limit of %d jumps\ntip: read above for list of most-executed functions", prog->name, jumpcount);
					}
				}
				break;

			case OP_IF:
				if(FLOAT_IS_TRUE_FOR_INT(OPA->_int))
				// TODO add an "int-if", and change this one, as well as the FLOAT_IS_TRUE_FOR_INT usages, to OPA->_float
				// although mostly unneeded, thanks to the only float being false being 0x0 and 0x80000000 (negative zero)
				// and entity, string, field values can never have that value
				{
					prog->xfunction->profile += (st - startst);
					st = cached_statements + st->jumpabsolute - 1;	// offset the st++
					startst = st;
					// no bounds check needed, it is done when loading progs
					if (++jumpcount == 10000000 && prvm_runawaycheck)
					{
						prog->xstatement = st - cached_statements;
						PRVM_Profile(prog, 1<<30, 0.01, 0);
						prog->error_cmd("%s runaway loop counter hit limit of %d jumps\ntip: read above for list of most-executed functions", prog->name, jumpcount);
					}
				}
				break;

			case OP_GOTO:
				prog->xfunction->profile += (st - startst);
				st = cached_statements + st->jumpabsolute - 1;	// offset the st++
				startst = st;
				// no bounds check needed, it is done when loading progs
				if (++jumpcount == 10000000 && prvm_runawaycheck)
				{
					prog->xstatement = st - cached_statements;
					PRVM_Profile(prog, 1<<30, 0.01, 0);
					prog->error_cmd("%s runaway loop counter hit limit of %d jumps\ntip: read above for list of most-executed functions", prog->name, jumpcount);
				}
				break;

			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
#ifdef PRVMTIMEPROFILING 
				tm = Sys_DirtyTime();
				prog->xfunction->tprofile += (tm - starttm >= 0 && tm - starttm < 1800) ? (tm - starttm) : 0;
				starttm = tm;
#endif
				prog->xfunction->profile += (st - startst);
				startst = st;
				prog->xstatement = st - cached_statements;
				prog->argc = st->op - OP_CALL0;
				if (!OPA->function)
					prog->error_cmd("NULL function in %s", prog->name);

				if(!OPA->function || OPA->function < 0 || OPA->function >= prog->numfunctions)
				{
					PreError();
					prog->error_cmd("%s CALL outside the program", prog->name);
					goto cleanup;
				}

				newf = &prog->functions[OPA->function];
				newf->callcount++;

				if (newf->first_statement < 0)
				{
					// negative first_statement values are built in functions
					int builtinnumber = -newf->first_statement;
					prog->xfunction->builtinsprofile++;
					if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
					{
						prog->builtins[builtinnumber](prog);
#ifdef PRVMTIMEPROFILING 
						tm = Sys_DirtyTime();
						newf->tprofile += (tm - starttm >= 0 && tm - starttm < 1800) ? (tm - starttm) : 0;
						prog->xfunction->tbprofile += (tm - starttm >= 0 && tm - starttm < 1800) ? (tm - starttm) : 0;
						starttm = tm;
#endif
						// builtins may cause ED_Alloc() to be called, update cached variables
						cached_edictsfields = prog->edictsfields;
						cached_entityfields = prog->entityfields;
						cached_entityfields_3 = prog->entityfields - 3;
						cached_entityfieldsarea = prog->entityfieldsarea;
						cached_entityfieldsarea_entityfields = prog->entityfieldsarea - prog->entityfields;
						cached_entityfieldsarea_3 = prog->entityfieldsarea - 3;
						cached_entityfieldsarea_entityfields_3 = prog->entityfieldsarea - prog->entityfields - 3;
						cached_max_edicts = prog->max_edicts;
						// these do not change
						//cached_statements = prog->statements;
						//cached_allowworldwrites = prog->allowworldwrites;
						//cached_flag = prog->flag;
						// if prog->trace changed we need to change interpreter path
						if (prog->trace != cachedpr_trace)
							goto chooseexecprogram;
					}
					else
						prog->error_cmd("No such builtin #%i in %s; most likely cause: outdated engine build. Try updating!", builtinnumber, prog->name);
				}
				else
					st = cached_statements + PRVM_EnterFunction(prog, newf);
				startst = st;
				break;

			case OP_DONE:
			case OP_RETURN:
#ifdef PRVMTIMEPROFILING 
				tm = Sys_DirtyTime();
				prog->xfunction->tprofile += (tm - starttm >= 0 && tm - starttm < 1800) ? (tm - starttm) : 0;
				starttm = tm;
#endif
				prog->xfunction->profile += (st - startst);
				prog->xstatement = st - cached_statements;

				prog->globals.ip[OFS_RETURN  ] = prog->globals.ip[st->operand[0]  ];
				prog->globals.ip[OFS_RETURN+1] = prog->globals.ip[st->operand[0]+1];
				prog->globals.ip[OFS_RETURN+2] = prog->globals.ip[st->operand[0]+2];

				st = cached_statements + PRVM_LeaveFunction(prog);
				startst = st;
				if (prog->depth <= exitdepth)
					goto cleanup; // all done
				break;

			case OP_STATE:
				if(cached_flag & PRVM_OP_STATE)
				{
					ed = PRVM_PROG_TO_EDICT(PRVM_gameglobaledict(self));
					PRVM_gameedictfloat(ed,nextthink) = PRVM_gameglobalfloat(time) + 0.1;
					PRVM_gameedictfloat(ed,frame) = OPA->_float;
					PRVM_gameedictfunction(ed,think) = OPB->function;
				}
				else
				{
					PreError();
					prog->xstatement = st - cached_statements;
					prog->error_cmd("OP_STATE not supported by %s", prog->name);
				}
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_ADD_I:
				OPC->_int = OPA->_int + OPB->_int;
				break;
			case OP_ADD_IF:
				OPC->_int = OPA->_int + (prvm_int_t) OPB->_float;
				break;
			case OP_ADD_FI:
				OPC->_float = OPA->_float + (prvm_vec_t) OPB->_int;
				break;
			case OP_SUB_I:
				OPC->_int = OPA->_int - OPB->_int;
				break;
			case OP_SUB_IF:
				OPC->_int = OPA->_int - (prvm_int_t) OPB->_float;
				break;
			case OP_SUB_FI:
				OPC->_float = OPA->_float - (prvm_vec_t) OPB->_int;
				break;
			case OP_MUL_I:
				OPC->_int = OPA->_int * OPB->_int;
				break;
			case OP_MUL_IF:
				OPC->_int = OPA->_int * (prvm_int_t) OPB->_float;
				break;
			case OP_MUL_FI:
				OPC->_float = OPA->_float * (prvm_vec_t) OPB->_int;
				break;
			case OP_MUL_VI:
				OPC->vector[0] = (prvm_vec_t) OPB->_int * OPA->vector[0];
				OPC->vector[1] = (prvm_vec_t) OPB->_int * OPA->vector[1];
				OPC->vector[2] = (prvm_vec_t) OPB->_int * OPA->vector[2];
				break;
			case OP_DIV_VF:
				{
					float temp = 1.0f / OPB->_float;
					OPC->vector[0] = temp * OPA->vector[0];
					OPC->vector[1] = temp * OPA->vector[1];
					OPC->vector[2] = temp * OPA->vector[2];
				}
				break;
			case OP_DIV_I:
				OPC->_int = OPA->_int / OPB->_int;
				break;
			case OP_DIV_IF:
				OPC->_int = OPA->_int / (prvm_int_t) OPB->_float;
				break;
			case OP_DIV_FI:
				OPC->_float = OPA->_float / (prvm_vec_t) OPB->_int;
				break;
			case OP_CONV_IF:
				OPC->_float = OPA->_int;
				break;
			case OP_CONV_FI:
				OPC->_int = OPA->_float;
				break;
			case OP_BITAND_I:
				OPC->_int = OPA->_int & OPB->_int;
				break;
			case OP_BITOR_I:
				OPC->_int = OPA->_int | OPB->_int;
				break;
			case OP_BITAND_IF:
				OPC->_int = OPA->_int & (prvm_int_t)OPB->_float;
				break;
			case OP_BITOR_IF:
				OPC->_int = OPA->_int | (prvm_int_t)OPB->_float;
				break;
			case OP_BITAND_FI:
				OPC->_float = (prvm_int_t)OPA->_float & OPB->_int;
				break;
			case OP_BITOR_FI:
				OPC->_float = (prvm_int_t)OPA->_float | OPB->_int;
				break;
			case OP_GE_I:
				OPC->_float = OPA->_int >= OPB->_int;
				break;
			case OP_LE_I:
				OPC->_float = OPA->_int <= OPB->_int;
				break;
			case OP_GT_I:
				OPC->_float = OPA->_int > OPB->_int;
				break;
			case OP_LT_I:
				OPC->_float = OPA->_int < OPB->_int;
				break;
			case OP_AND_I:
				OPC->_float = OPA->_int && OPB->_int;
				break;
			case OP_OR_I:
				OPC->_float = OPA->_int || OPB->_int;
				break;
			case OP_GE_IF:
				OPC->_float = (prvm_vec_t)OPA->_int >= OPB->_float;
				break;
			case OP_LE_IF:
				OPC->_float = (prvm_vec_t)OPA->_int <= OPB->_float;
				break;
			case OP_GT_IF:
				OPC->_float = (prvm_vec_t)OPA->_int > OPB->_float;
				break;
			case OP_LT_IF:
				OPC->_float = (prvm_vec_t)OPA->_int < OPB->_float;
				break;
			case OP_AND_IF:
				OPC->_float = (prvm_vec_t)OPA->_int && OPB->_float;
				break;
			case OP_OR_IF:
				OPC->_float = (prvm_vec_t)OPA->_int || OPB->_float;
				break;
			case OP_GE_FI:
				OPC->_float = OPA->_float >= (prvm_vec_t)OPB->_int;
				break;
			case OP_LE_FI:
				OPC->_float = OPA->_float <= (prvm_vec_t)OPB->_int;
				break;
			case OP_GT_FI:
				OPC->_float = OPA->_float > (prvm_vec_t)OPB->_int;
				break;
			case OP_LT_FI:
				OPC->_float = OPA->_float < (prvm_vec_t)OPB->_int;
				break;
			case OP_AND_FI:
				OPC->_float = OPA->_float && (prvm_vec_t)OPB->_int;
				break;
			case OP_OR_FI:
				OPC->_float = OPA->_float || (prvm_vec_t)OPB->_int;
				break;
			case OP_NOT_I:
				OPC->_float = !OPA->_int;
				break;
			case OP_EQ_I:
				OPC->_float = OPA->_int == OPB->_int;
				break;
			case OP_EQ_IF:
				OPC->_float = (prvm_vec_t)OPA->_int == OPB->_float;
				break;
			case OP_EQ_FI:
				OPC->_float = OPA->_float == (prvm_vec_t)OPB->_int;
				break;
			case OP_NE_I:
				OPC->_float = OPA->_int != OPB->_int;
				break;
			case OP_NE_IF:
				OPC->_float = (prvm_vec_t)OPA->_int != OPB->_float;
				break;
			case OP_NE_FI:
				OPC->_float = OPA->_float != (prvm_vec_t)OPB->_int;
				break;
			case OP_STORE_I:
				OPB->_int = OPA->_int;
				break;
			case OP_STOREP_I:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 4 > pr_edictareasize)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to write to an out of bounds edict", prog->name);
					goto cleanup;
				}
#endif
				ptr = (prvm_eval_t *)(prog->edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_LOAD_I:
#if PRBOUNDSCHECK
				if (OPA->edict < 0 || OPA->edict >= prog->max_edicts)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an out of bounds edict number", prog->name);
					goto cleanup;
				}
				if (OPB->_int < 0 || OPB->_int >= progs->entityfields)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an invalid field in an edict", prog->name);
					goto cleanup;
				}
#endif
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((prvm_eval_t *)((int *)ed->v + OPB->_int))->_int;
				break;

			case OP_GSTOREP_I:
			case OP_GSTOREP_F:
			case OP_GSTOREP_ENT:
			case OP_GSTOREP_FLD:		// integers
			case OP_GSTOREP_S:
			case OP_GSTOREP_FNC:		// pointers
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int >= pr_globaldefs)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to write to an invalid indexed global", prog->name);
					goto cleanup;
				}
#endif
				pr_iglobals[OPB->_int] = OPA->_int;
				break;
			case OP_GSTOREP_V:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 2 >= pr_globaldefs)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to write to an invalid indexed global", prog->name);
					goto cleanup;
				}
#endif
				pr_iglobals[OPB->_int  ] = OPA->ivector[0];
				pr_iglobals[OPB->_int+1] = OPA->ivector[1];
				pr_iglobals[OPB->_int+2] = OPA->ivector[2];
				break;

			case OP_GADDRESS:
				i = OPA->_int + (prvm_int_t) OPB->_float;
#if PRBOUNDSCHECK
				if (i < 0 || i >= pr_globaldefs)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to address an out of bounds global", prog->name);
					goto cleanup;
				}
#endif
				OPC->_int = pr_iglobals[i];
				break;

			case OP_GLOAD_I:
			case OP_GLOAD_F:
			case OP_GLOAD_FLD:
			case OP_GLOAD_ENT:
			case OP_GLOAD_S:
			case OP_GLOAD_FNC:
#if PRBOUNDSCHECK
				if (OPA->_int < 0 || OPA->_int >= pr_globaldefs)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an invalid indexed global", prog->name);
					goto cleanup;
				}
#endif
				OPC->_int = pr_iglobals[OPA->_int];
				break;

			case OP_GLOAD_V:
#if PRBOUNDSCHECK
				if (OPA->_int < 0 || OPA->_int + 2 >= pr_globaldefs)
				{
					PreError();
					prog->error_cmd("%s Progs attempted to read an invalid indexed global", prog->name);
					goto cleanup;
				}
#endif
				OPC->ivector[0] = pr_iglobals[OPA->_int  ];
				OPC->ivector[1] = pr_iglobals[OPA->_int+1];
				OPC->ivector[2] = pr_iglobals[OPA->_int+2];
				break;

			case OP_BOUNDCHECK:
				if (OPA->_int < 0 || OPA->_int >= st->b)
				{
					PreError();
					prog->error_cmd("%s Progs boundcheck failed at line number %d, value is < 0 or >= %d", prog->name, st->b, st->c);
					goto cleanup;
				}
				break;

*/

			default:
				PreError();
				prog->error_cmd("Bad opcode %i in %s", st->op, prog->name);
				goto cleanup;
			}
#if PRVMSLOWINTERPRETER
			{
				if (prog->watch_global_type != ev_void)
				{
					prvm_eval_t *f = PRVM_GLOBALFIELDVALUE(prog->watch_global);
					prog->xstatement = st - cached_statements;
					PRVM_Watchpoint(prog, 0, "Global watchpoint hit", prog->watch_global_type, &prog->watch_global_value, f);
				}
				if (prog->watch_field_type != ev_void && prog->watch_edict < prog->max_edicts)
				{
					prvm_eval_t *f = PRVM_EDICTFIELDVALUE(prog->edicts + prog->watch_edict, prog->watch_field);
					prog->xstatement = st - cached_statements;
					PRVM_Watchpoint(prog, 0, "Entityfield watchpoint hit", prog->watch_field_type, &prog->watch_edictfield_value, f);
				}
			}
#endif
		}

#undef PreError
