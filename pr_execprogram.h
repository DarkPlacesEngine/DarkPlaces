
// This code isn't #ifdef/#define protectable, don't try.

		while (1)
		{
			st++;
			if (++profile > 1000000) // LordHavoc: increased runaway loop limit 10x
			{
				pr_xstatement = st - pr_statements;
				Host_Error ("runaway loop error");
			}

#if PRTRACE
			PR_PrintStatement (st);
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
				OPC->_float = !OPA->string || !pr_strings[OPA->string];
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
				OPC->_float = !strcmp(pr_strings+OPA->string,pr_strings+OPB->string);
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
				OPC->_float = strcmp(pr_strings+OPA->string,pr_strings+OPB->string);
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
				OPB->vector[0] = OPA->vector[0];
				OPB->vector[1] = OPA->vector[1];
				OPB->vector[2] = OPA->vector[2];
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:		// integers
			case OP_STOREP_S:
			case OP_STOREP_FNC:		// pointers
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 4 > pr_edictareasize)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an out of bounds edict (%i)\n", OPB->_int);
					return;
				}
#endif
				ptr = (eval_t *)((qbyte *)sv.edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_STOREP_V:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 12 > pr_edictareasize)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an out of bounds edict (%i)\n", OPB->_int);
					return;
				}
#endif
				ptr = (eval_t *)((qbyte *)sv.edictsfields + OPB->_int);
				ptr->vector[0] = OPA->vector[0];
				ptr->vector[1] = OPA->vector[1];
				ptr->vector[2] = OPA->vector[2];
				break;

			case OP_ADDRESS:
				pr_xstatement = st - pr_statements;
#if PRBOUNDSCHECK
				if ((unsigned int)OPB->_int >= progs->entityfields)
				{
					Host_Error("Progs attempted to address an invalid field (%i) in an edict\n", OPB->_int);
					return;
				}
#endif
				if (OPA->edict == 0 && sv.state == ss_active)
				{
					Host_Error ("assignment to world entity");
					return;
				}
				ed = PROG_TO_EDICT(OPA->edict);
				OPC->_int = (qbyte *)((int *)ed->v + OPB->_int) - (qbyte *)sv.edictsfields;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
				pr_xstatement = st - pr_statements;
#if PRBOUNDSCHECK
				if ((unsigned int)OPB->_int >= progs->entityfields)
				{
					Host_Error("Progs attempted to read an invalid field in an edict (%i)\n", OPB->_int);
					return;
				}
#endif
				ed = PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((eval_t *)((int *)ed->v + OPB->_int))->_int;
				break;

			case OP_LOAD_V:
				pr_xstatement = st - pr_statements;
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 2 >= progs->entityfields)
				{
					Host_Error("Progs attempted to read an invalid field in an edict (%i)\n", OPB->_int);
					return;
				}
#endif
				ed = PROG_TO_EDICT(OPA->edict);
				OPC->vector[0] = ((eval_t *)((int *)ed->v + OPB->_int))->vector[0];
				OPC->vector[1] = ((eval_t *)((int *)ed->v + OPB->_int))->vector[1];
				OPC->vector[2] = ((eval_t *)((int *)ed->v + OPB->_int))->vector[2];
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
				pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				pr_xstatement = st - pr_statements;
				pr_argc = st->op - OP_CALL0;
				if (!OPA->function)
					Host_Error ("NULL function");

				newf = &pr_functions[OPA->function];

				if (newf->first_statement < 0)
				{
					// negative statements are built in functions
					if ((-newf->first_statement) >= pr_numbuiltins)
						Host_Error ("Bad builtin call number");
					pr_builtins[-newf->first_statement] ();
				}
				else
					st = pr_statements + PR_EnterFunction(newf);
				break;

			case OP_DONE:
			case OP_RETURN:
				pr_globals[OFS_RETURN] = pr_globals[(unsigned short) st->a];
				pr_globals[OFS_RETURN+1] = pr_globals[(unsigned short) st->a+1];
				pr_globals[OFS_RETURN+2] = pr_globals[(unsigned short) st->a+2];

				st = pr_statements + PR_LeaveFunction();
				if (pr_depth <= exitdepth)
					return;		// all done
				if (pr_trace != cachedpr_trace)
					goto chooseexecprogram;
				break;

			case OP_STATE:
				ed = PROG_TO_EDICT(pr_global_struct->self);
				ed->v->nextthink = pr_global_struct->time + 0.1;
				ed->v->frame = OPA->_float;
				ed->v->think = OPB->function;
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
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an out of bounds edict\n");
					return;
				}
#endif
				ptr = (eval_t *)((qbyte *)sv.edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case OP_LOAD_I:
#if PRBOUNDSCHECK
				if (OPA->edict < 0 || OPA->edict >= pr_edictareasize)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an out of bounds edict number\n");
					return;
				}
				if (OPB->_int < 0 || OPB->_int >= progs->entityfields)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid field in an edict\n");
					return;
				}
#endif
				ed = PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((eval_t *)((int *)ed->v + OPB->_int))->_int;
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
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an invalid indexed global\n");
					return;
				}
#endif
				pr_globals[OPB->_int] = OPA->_float;
				break;
			case OP_GSTOREP_V:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 2 >= pr_globaldefs)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to write to an invalid indexed global\n");
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
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to address an out of bounds global\n");
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
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid indexed global\n");
					return;
				}
#endif
				OPC->_float = pr_globals[OPA->_int];
				break;

			case OP_GLOAD_V:
#if PRBOUNDSCHECK
				if (OPA->_int < 0 || OPA->_int + 2 >= pr_globaldefs)
				{
					pr_xstatement = st - pr_statements;
					Host_Error("Progs attempted to read an invalid indexed global\n");
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
					pr_xstatement = st - pr_statements;
					Host_Error("Progs boundcheck failed at line number %d, value is < 0 or >= %d\n", st->b, st->c);
					return;
				}
				break;

*/

			default:
				pr_xstatement = st - pr_statements;
				Host_Error ("Bad opcode %i", st->op);
			}
		}

