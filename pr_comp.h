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
	OP_BITOR,

	OP_STORE_I = 113,

	OP_ADD_I = 116,
	OP_ADD_FI,
	OP_ADD_IF,

	OP_SUB_I,
	OP_SUB_FI,
	OP_SUB_IF,
	OP_CONV_IF,
	OP_CONV_FI,

	OP_LOAD_I = 126,
	OP_STOREP_I,

	OP_BITAND_I = 130,
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_NOT_I = 138,

	OP_DIV_VF,
	
	OP_STORE_P = 152,

	OP_LE_I = 161,
	OP_GE_I,
	OP_LT_I,
	OP_GT_I,
	
	OP_LE_IF,
	OP_GE_IF,
	OP_LT_IF,
	OP_GT_IF,

	OP_LE_FI,
	OP_GE_FI,
	OP_LT_FI,
	OP_GT_FI,

	OP_EQ_IF,
	OP_EQ_FI,

	OP_MUL_IF = 179,
	OP_MUL_FI,
	OP_MUL_VI,
	OP_DIV_IF = 183,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,
	OP_BITOR_FI,
	OP_AND_I,
	OP_OR_I,
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

	OP_GSTOREP_I,
	OP_GSTOREP_F,
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,		
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,
	OP_GLOAD_ENT,
	OP_GLOAD_S,
	OP_GLOAD_FNC,
	OP_BOUNDCHECK,
	OP_GLOAD_V = 216
}
opcode_t;


typedef struct statement16_s
{
	unsigned short	op;
	signed short	a,b,c;
}
dstatement16_t;
typedef struct statement32_s
{
	unsigned int	op;
	signed int	a,b,c;
}
dstatement32_t;

typedef struct ddef16_s
{
	unsigned short	type;		// if DEF_SAVEGLOBGAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	int			s_name;
}
ddef16_t;
typedef struct ddef32_s
{
	unsigned int	type;		// if DEF_SAVEGLOBGAL bit is set
								// the variable needs to be saved in savegames
	unsigned int	ofs;
	int			s_name;
}
ddef32_t, mdef_t;
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

	unsigned int		ofs_statements;
	unsigned int		numstatements;	// statement 0 is an error

	unsigned int		ofs_globaldefs;
	unsigned int		numglobaldefs;

	unsigned int		ofs_fielddefs;
	unsigned int		numfielddefs;

	unsigned int		ofs_functions;
	unsigned int		numfunctions;	// function 0 is an empty

	unsigned int		ofs_strings;
	unsigned int		numstrings;		// first string is a null string

	unsigned int		ofs_globals;
	unsigned int		numglobals;

	unsigned int		entityfields;
}
dprograms_t;

typedef struct dprograms_v7_s
{	//extended header written by fteqcc.
	dprograms_t	v6;	//for easier casting.

	//debug / version 7 extensions
	unsigned int	ofsfiles;			//ignored. deprecated, should be 0. source files can instead be embedded by simply treating the .dat as a zip.
	unsigned int	ofslinenums;		//ignored. alternative to external .lno files.
	unsigned int	ofsbodylessfuncs;	//unsupported. function names imported from other modules. must be 0.
	unsigned int	numbodylessfuncs;	//unsupported. must be 0.

	unsigned int	ofs_types;			//unsupported+deprecated. rich type info. must be 0.
	unsigned int	numtypes;			//unsupported+deprecated. rich type info. must be 0.
	unsigned int	blockscompressed;	//unsupported. per-block compression. must be 0.

	int	secondaryversion;				//if not known then its kkqwsv's v7, qfcc's v7, or uhexen2's v7, or something. abandon all hope when not recognised.
#define PROG_SECONDARYVERSION16 ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('P'<<0)|('R'<<8)|('O'<<16)|('G'<<24)))	//regular 16bit statements.
#define PROG_SECONDARYVERSION32 ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('3'<<0)|('2'<<8)|('B'<<16)|(' '<<24)))	//statements+globaldefs+fielddefs extended to 32bit.
}
dprograms_v7_t;

#endif

