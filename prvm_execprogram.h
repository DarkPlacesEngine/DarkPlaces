
// This code isn't #ifdef/#define protectable, don't try.

		while (1)
		{
			st++;
			if (++profile > 10000000) // LordHavoc: increased runaway loop limit 100x
			{
				// LordHavoc: update profile counter for debugging reasons
				// (identifying erroneous loops and recursion patterns)
				prog->xfunction->profile += profile - startprofile;
				startprofile = profile;
				// update the statement number before we error out
				prog->xstatement = st - prog->statements;
				PRVM_ERROR("runaway loop counter hit limit of %d opcodes\ntip: if having trouble identifying the problem, try typing profile now in %s", profile, PRVM_NAME);
			}

#if PRVMTRACE
			prog->xfunction->profile += profile - startprofile;
			startprofile = profile;
			prog->xstatement = st - prog->statements;
			PRVM_PrintStatement(st);
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
				OPC->vector[0] = OPA->_float * OPB->vector[0];
				OPC->vector[1] = OPA->_float * OPB->vector[1];
				OPC->vector[2] = OPA->_float * OPB->vector[2];
				break;
			case OP_MUL_VF:
				OPC->vector[0] = OPB->_float * OPA->vector[0];
				OPC->vector[1] = OPB->_float * OPA->vector[1];
				OPC->vector[2] = OPB->_float * OPA->vector[2];
				break;
			case OP_DIV_F:
				OPC->_float = OPA->_float / OPB->_float;
				break;
			case OP_BITAND:
				OPC->_float = (int)OPA->_float & (int)OPB->_float;
				break;
			case OP_BITOR:
				OPC->_float = (int)OPA->_float | (int)OPB->_float;
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
				OPC->_float = OPA->_float && OPB->_float;
				break;
			case OP_OR:
				OPC->_float = OPA->_float || OPB->_float;
				break;
			case OP_NOT_F:
				OPC->_float = !OPA->_float;
				break;
			case OP_NOT_V:
				OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
				break;
			case OP_NOT_S:
				OPC->_float = !OPA->string || !*PRVM_GetString(OPA->string);
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
				OPC->_float = !strcmp(PRVM_GetString(OPA->string),PRVM_GetString(OPB->string));
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
				OPC->_float = strcmp(PRVM_GetString(OPA->string),PRVM_GetString(OPB->string));
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
#if PRVMBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 4 > prog->edictareasize)
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, OPB->_int);
					return;
				}
#endif
				ptr = (prvm_eval_t *)((unsigned char *)prog->edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_STOREP_V:
#if PRVMBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 12 > prog->edictareasize)
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, OPB->_int);
					return;
				}
#endif
				ptr = (prvm_eval_t *)((unsigned char *)prog->edictsfields + OPB->_int);
				ptr->vector[0] = OPA->vector[0];
				ptr->vector[1] = OPA->vector[1];
				ptr->vector[2] = OPA->vector[2];
				break;

			case OP_ADDRESS:
#if PRVMBOUNDSCHECK
				if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->progs->entityfields))
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("%s attempted to address an invalid field (%i) in an edict", PRVM_NAME, OPB->_int);
					return;
				}
#endif
				if (OPA->edict == 0 && !prog->allowworldwrites)
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("forbidden assignment to null/world entity in %s", PRVM_NAME);
					return;
				}
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->_int = (unsigned char *)((int *)ed->fields.vp + OPB->_int) - (unsigned char *)prog->edictsfields;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
#if PRVMBOUNDSCHECK
				if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->progs->entityfields))
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
					return;
				}
#endif
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((prvm_eval_t *)((int *)ed->fields.vp + OPB->_int))->_int;
				break;

			case OP_LOAD_V:
#if PRVMBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 2 >= prog->progs->entityfields)
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
					return;
				}
#endif
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->vector[0] = ((prvm_eval_t *)((int *)ed->fields.vp + OPB->_int))->vector[0];
				OPC->vector[1] = ((prvm_eval_t *)((int *)ed->fields.vp + OPB->_int))->vector[1];
				OPC->vector[2] = ((prvm_eval_t *)((int *)ed->fields.vp + OPB->_int))->vector[2];
				break;

		//==================

			case OP_IFNOT:
				if (!OPA->_int)
					st += st->b - 1;	// offset the s++
				break;

			case OP_IF:
				if (OPA->_int)
					st += st->b - 1;	// offset the s++
				break;

			case OP_GOTO:
				st += st->a - 1;	// offset the s++
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
				prog->xfunction->profile += profile - startprofile;
				startprofile = profile;
				prog->xstatement = st - prog->statements;
				prog->argc = st->op - OP_CALL0;
				if (!OPA->function)
					PRVM_ERROR("NULL function in %s", PRVM_NAME);

				newf = &prog->functions[OPA->function];
				newf->callcount++;

				if (newf->first_statement < 0)
				{
					// negative statements are built in functions
					int builtinnumber = -newf->first_statement;
					prog->xfunction->builtinsprofile++;
					if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
						prog->builtins[builtinnumber]();
					else
						PRVM_ERROR("No such builtin #%i in %s", builtinnumber, PRVM_NAME);
				}
				else
					st = prog->statements + PRVM_EnterFunction(newf);
				break;

			case OP_DONE:
			case OP_RETURN:
				prog->xfunction->profile += profile - startprofile;
				startprofile = profile;
				prog->xstatement = st - prog->statements;

				prog->globals.generic[OFS_RETURN] = prog->globals.generic[(unsigned short) st->a];
				prog->globals.generic[OFS_RETURN+1] = prog->globals.generic[(unsigned short) st->a+1];
				prog->globals.generic[OFS_RETURN+2] = prog->globals.generic[(unsigned short) st->a+2];

				st = prog->statements + PRVM_LeaveFunction();
				if (prog->depth <= exitdepth)
					return;		// all done
				if (prog->trace != cachedpr_trace)
					goto chooseexecprogram;
				break;

			case OP_STATE:
				if(prog->flag & PRVM_OP_STATE)
				{
					prog->xfunction->profile += profile - startprofile;
					startprofile = profile;
					prog->xstatement = st - prog->statements;
					ed = PRVM_PROG_TO_EDICT(PRVM_G_INT(prog->self->ofs));
					PRVM_E_FLOAT(ed,PRVM_ED_FindField ("nextthink")->ofs) = *prog->time + 0.1;
					PRVM_E_FLOAT(ed,PRVM_ED_FindField ("frame")->ofs) = OPA->_float;
					*(func_t *)((float*)ed->fields.vp + PRVM_ED_FindField ("think")->ofs) = OPB->function;
				}
				else
					PRVM_ERROR("OP_STATE not supported by %s", PRVM_NAME);
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_ADD_I:
				OPC->_int = OPA->_int + OPB->_int;
				break;
			case OP_ADD_IF:
				OPC->_int = OPA->_int + (int) OPB->_float;
				break;
			case OP_ADD_FI:
				OPC->_float = OPA->_float + (float) OPB->_int;
				break;
			case OP_SUB_I:
				OPC->_int = OPA->_int - OPB->_int;
				break;
			case OP_SUB_IF:
				OPC->_int = OPA->_int - (int) OPB->_float;
				break;
			case OP_SUB_FI:
				OPC->_float = OPA->_float - (float) OPB->_int;
				break;
			case OP_MUL_I:
				OPC->_int = OPA->_int * OPB->_int;
				break;
			case OP_MUL_IF:
				OPC->_int = OPA->_int * (int) OPB->_float;
				break;
			case OP_MUL_FI:
				OPC->_float = OPA->_float * (float) OPB->_int;
				break;
			case OP_MUL_VI:
				OPC->vector[0] = (float) OPB->_int * OPA->vector[0];
				OPC->vector[1] = (float) OPB->_int * OPA->vector[1];
				OPC->vector[2] = (float) OPB->_int * OPA->vector[2];
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
				OPC->_int = OPA->_int / (int) OPB->_float;
				break;
			case OP_DIV_FI:
				OPC->_float = OPA->_float / (float) OPB->_int;
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
				OPC->_int = OPA->_int & (int)OPB->_float;
				break;
			case OP_BITOR_IF:
				OPC->_int = OPA->_int | (int)OPB->_float;
				break;
			case OP_BITAND_FI:
				OPC->_float = (int)OPA->_float & OPB->_int;
				break;
			case OP_BITOR_FI:
				OPC->_float = (int)OPA->_float | OPB->_int;
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
				OPC->_float = (float)OPA->_int >= OPB->_float;
				break;
			case OP_LE_IF:
				OPC->_float = (float)OPA->_int <= OPB->_float;
				break;
			case OP_GT_IF:
				OPC->_float = (float)OPA->_int > OPB->_float;
				break;
			case OP_LT_IF:
				OPC->_float = (float)OPA->_int < OPB->_float;
				break;
			case OP_AND_IF:
				OPC->_float = (float)OPA->_int && OPB->_float;
				break;
			case OP_OR_IF:
				OPC->_float = (float)OPA->_int || OPB->_float;
				break;
			case OP_GE_FI:
				OPC->_float = OPA->_float >= (float)OPB->_int;
				break;
			case OP_LE_FI:
				OPC->_float = OPA->_float <= (float)OPB->_int;
				break;
			case OP_GT_FI:
				OPC->_float = OPA->_float > (float)OPB->_int;
				break;
			case OP_LT_FI:
				OPC->_float = OPA->_float < (float)OPB->_int;
				break;
			case OP_AND_FI:
				OPC->_float = OPA->_float && (float)OPB->_int;
				break;
			case OP_OR_FI:
				OPC->_float = OPA->_float || (float)OPB->_int;
				break;
			case OP_NOT_I:
				OPC->_float = !OPA->_int;
				break;
			case OP_EQ_I:
				OPC->_float = OPA->_int == OPB->_int;
				break;
			case OP_EQ_IF:
				OPC->_float = (float)OPA->_int == OPB->_float;
				break;
			case OP_EQ_FI:
				OPC->_float = OPA->_float == (float)OPB->_int;
				break;
			case OP_NE_I:
				OPC->_float = OPA->_int != OPB->_int;
				break;
			case OP_NE_IF:
				OPC->_float = (float)OPA->_int != OPB->_float;
				break;
			case OP_NE_FI:
				OPC->_float = OPA->_float != (float)OPB->_int;
				break;
			case OP_STORE_I:
				OPB->_int = OPA->_int;
				break;
			case OP_STOREP_I:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 4 > pr_edictareasize)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an out of bounds edict");
					return;
				}
#endif
				ptr = (prvm_eval_t *)((unsigned char *)prog->edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_LOAD_I:
#if PRBOUNDSCHECK
				if (OPA->edict < 0 || OPA->edict >= pr_edictareasize)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an out of bounds edict number");
					return;
				}
				if (OPB->_int < 0 || OPB->_int >= progs->entityfields)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid field in an edict");
					return;
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
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an invalid indexed global");
					return;
				}
#endif
				pr_globals[OPB->_int] = OPA->_float;
				break;
			case OP_GSTOREP_V:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 2 >= pr_globaldefs)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an invalid indexed global");
					return;
				}
#endif
				pr_globals[OPB->_int  ] = OPA->vector[0];
				pr_globals[OPB->_int+1] = OPA->vector[1];
				pr_globals[OPB->_int+2] = OPA->vector[2];
				break;

			case OP_GADDRESS:
				i = OPA->_int + (int) OPB->_float;
#if PRBOUNDSCHECK
				if (i < 0 || i >= pr_globaldefs)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to address an out of bounds global");
					return;
				}
#endif
				OPC->_float = pr_globals[i];
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
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid indexed global");
					return;
				}
#endif
				OPC->_float = pr_globals[OPA->_int];
				break;

			case OP_GLOAD_V:
#if PRBOUNDSCHECK
				if (OPA->_int < 0 || OPA->_int + 2 >= pr_globaldefs)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid indexed global");
					return;
				}
#endif
				OPC->vector[0] = pr_globals[OPA->_int  ];
				OPC->vector[1] = pr_globals[OPA->_int+1];
				OPC->vector[2] = pr_globals[OPA->_int+2];
				break;

			case OP_BOUNDCHECK:
				if (OPA->_int < 0 || OPA->_int >= st->b)
				{
					pr_xfunction->profile += profile - startprofile;
					startprofile = profile;
					pr_xstatement = st - pr_statements;
					Host_Error("Progs boundcheck failed at line number %d, value is < 0 or >= %d", st->b, st->c);
					return;
				}
				break;

*/

			default:
				prog->xfunction->profile += profile - startprofile;
				startprofile = profile;
				prog->xstatement = st - prog->statements;
				PRVM_ERROR ("Bad opcode %i in %s", st->op, PRVM_NAME);
			}
		}

