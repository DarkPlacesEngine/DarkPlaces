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

// this file is shared by quake and qcc

#ifndef PR_COMP_H
#define PR_COMP_H

typedef unsigned int	func_t;
typedef int	string_t;

typedef enum etype_e {ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer} etype_t;


#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	OFS_PARM5		19
#define	OFS_PARM6		22
#define	OFS_PARM7		25
#define	RESERVED_OFS	28


typedef enum opcode_e
{
	OP_DONE,
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,

	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,

	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,

	OP_LE,
	OP_GE,
	OP_LT,
	OP_GT,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,
	OP_GOTO,
	OP_AND,
	OP_OR,

	OP_BITAND,
	OP_BITOR
}
opcode_t;


typedef struct statement_s
{
	unsigned short	op;
	signed short	a,b,c;
}
dstatement_t;

typedef struct ddef_s
{
	unsigned short	type;		// if DEF_SAVEGLOBGAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	int			s_name;
}
ddef_t;
#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

typedef struct dfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;				// total ints of parms + locals

	int		profile;		// runtime

	int		s_name;
	int		s_file;			// source file defined in

	int		numparms;
	unsigned char	parm_size[MAX_PARMS];
}
dfunction_t;

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;				// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit
	double  tprofile;           // realtime in this function
	double  tbprofile;          // realtime in builtins called by this function (NOTE: builtins also have a tprofile!)
	double	profile;		// runtime
	double	builtinsprofile; // cost of builtin functions called by this function
	double	callcount; // times the functions has been called since the last profile call
	double  totaltime; // total execution time of this function DIRECTLY FROM THE ENGINE
	double	tprofile_total;		// runtime (NOTE: tbprofile_total makes no real sense, so not accumulating that)
	double	profile_total;		// runtime
	double	builtinsprofile_total; // cost of builtin functions called by this function
	int     recursion;

	int		s_name;
	int		s_file;			// source file defined in

	int		numparms;
	unsigned char	parm_size[MAX_PARMS];
}
mfunction_t;

typedef struct mstatement_s
{
	opcode_t	op;
	int			operand[3]; // always a global or -1 for unused
	int			jumpabsolute; // only used by IF, IFNOT, GOTO
}
mstatement_t;


#define	PROG_VERSION	6
typedef struct dprograms_s
{
	int		version;
	int		crc;			// check of header file

	int		ofs_statements;
	int		numstatements;	// statement 0 is an error

	int		ofs_globaldefs;
	int		numglobaldefs;

	int		ofs_fielddefs;
	int		numfielddefs;

	int		ofs_functions;
	int		numfunctions;	// function 0 is an empty

	int		ofs_strings;
	int		numstrings;		// first string is a null string

	int		ofs_globals;
	int		numglobals;

	int		entityfields;
}
dprograms_t;

#endif

