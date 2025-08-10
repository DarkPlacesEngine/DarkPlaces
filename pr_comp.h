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
	// NOTE: List mostly generated using `./fteqcc.bin -TDP_20250104 -Fdumpopcodes`.
	OP_DONE = 0,
	OP_MUL_F = 1,
	OP_MUL_V = 2,
	OP_MUL_FV = 3,
	OP_MUL_VF = 4,
	OP_DIV_F = 5,
	OP_ADD_F = 6,
	OP_ADD_V = 7,
	OP_SUB_F = 8,
	OP_SUB_V = 9,
	OP_EQ_F = 10,
	OP_EQ_V = 11,
	OP_EQ_S = 12,
	OP_EQ_E = 13,
	OP_EQ_FNC = 14,
	OP_NE_F = 15,
	OP_NE_V = 16,
	OP_NE_S = 17,
	OP_NE_E = 18,
	OP_NE_FNC = 19,
	OP_LE_F = 20,
	OP_GE_F = 21,
	OP_LT_F = 22,
	OP_GT_F = 23,
	OP_LOAD_F = 24,
	OP_LOAD_V = 25,
	OP_LOAD_S = 26,
	OP_LOAD_ENT = 27,
	OP_LOAD_FLD = 28,
	OP_LOAD_FNC = 29,
	OP_ADDRESS = 30,
	OP_STORE_F = 31,
	OP_STORE_V = 32,
	OP_STORE_S = 33,
	OP_STORE_ENT = 34,
	OP_STORE_FLD = 35,
	OP_STORE_FNC = 36,
	OP_STOREP_F = 37,
	OP_STOREP_V = 38,
	OP_STOREP_S = 39,
	OP_STOREP_ENT = 40,
	OP_STOREP_FLD = 41,
	OP_STOREP_FNC = 42,
	OP_RETURN = 43,
	OP_NOT_F = 44,
	OP_NOT_V = 45,
	OP_NOT_S = 46,
	OP_NOT_ENT = 47,
	OP_NOT_FNC = 48,
	OP_IF = 49,
	OP_IFNOT = 50,
	OP_CALL0 = 51,
	OP_CALL1 = 52,
	OP_CALL2 = 53,
	OP_CALL3 = 54,
	OP_CALL4 = 55,
	OP_CALL5 = 56,
	OP_CALL6 = 57,
	OP_CALL7 = 58,
	OP_CALL8 = 59,
	OP_STATE = 60,
	OP_GOTO = 61,
	OP_AND_F = 62,
	OP_OR_F = 63,
	OP_BITAND_F = 64,
	OP_BITOR_F = 65,
	// OP_MULSTORE_F = 66,
	// OP_MULSTORE_VF = 67,
	// OP_MULSTOREP_F = 68,
	// OP_MULSTOREP_VF = 69,
	// OP_DIVSTORE_F = 70,
	// OP_DIVSTOREP_F = 71,
	// OP_ADDSTORE_F = 72,
	// OP_ADDSTORE_V = 73,
	// OP_ADDSTOREP_F = 74,
	// OP_ADDSTOREP_V = 75,
	// OP_SUBSTORE_F = 76,
	// OP_SUBSTORE_V = 77,
	// OP_SUBSTOREP_F = 78,
	// OP_SUBSTOREP_V = 79,
	// OP_FETCH_GBL_F = 80,
	// OP_FETCH_GBL_V = 81,
	// OP_FETCH_GBL_S = 82,
	// OP_FETCH_GBL_E = 83,
	// OP_FETCH_GBL_FNC = 84,
	// OP_CSTATE = 85,
	// OP_CWSTATE = 86,
	// OP_THINKTIME = 87,
	// OP_BITSETSTORE_F = 88,
	// OP_BITSETSTOREP_F = 89,
	// OP_BITCLRSTORE_F = 90,
	// OP_BITCLRSTOREP_F = 91,
	// OP_RAND0 = 92,
	// OP_RAND1 = 93,
	// OP_RAND2 = 94,
	// OP_RANDV0 = 95,
	// OP_RANDV1 = 96,
	// OP_RANDV2 = 97,
	// OP_SWITCH_F = 98,
	// OP_SWITCH_V = 99,
	// OP_SWITCH_S = 100,
	// OP_SWITCH_E = 101,
	// OP_SWITCH_FNC = 102,
	// OP_CASE = 103,
	// OP_CASERANGE = 104,
	// OP_CALL1H = 105,
	// OP_CALL2H = 106,
	// OP_CALL3H = 107,
	// OP_CALL4H = 108,
	// OP_CALL5H = 109,
	// OP_CALL6H = 110,
	// OP_CALL7H = 111,
	// OP_CALL8H = 112,
	OP_STORE_I = 113,
	// OP_STORE_IF = 114,
	// OP_STORE_FI = 115,
	OP_ADD_I = 116,
	OP_ADD_FI = 117,
	OP_ADD_IF = 118,
	OP_SUB_I = 119,
	OP_SUB_FI = 120,
	OP_SUB_IF = 121,
	OP_CONV_ITOF = 122,
	OP_CONV_FTOI = 123,
	// OP_CONVP_IF = 124,
	// OP_CONVP_FI = 125,
	OP_LOAD_I = 126,
	OP_STOREP_I = 127,
	// OP_STOREP_IF = 128,
	// OP_STOREP_FI = 129,
	OP_BITAND_I = 130,
	OP_BITOR_I = 131,
	OP_MUL_I = 132,
	OP_DIV_I = 133,
	OP_EQ_I = 134,
	OP_NE_I = 135,
	// OP_IFNOTS = 136,
	// OP_IFS = 137,
	OP_NOT_I = 138,
	OP_DIV_VF = 139,
	// OP_BITXOR_I = 140,
	OP_RSHIFT_I = 141,
	OP_LSHIFT_I = 142,
	OP_GLOBALADDRESS = 143,
	OP_ADD_PIW = 144,
	OP_LOADA_F = 145,
	OP_LOADA_V = 146,
	OP_LOADA_S = 147,
	OP_LOADA_ENT = 148,
	OP_LOADA_FLD = 149,
	OP_LOADA_FNC = 150,
	OP_LOADA_I = 151,
	OP_STORE_P = 152,
	OP_LOAD_P = 153,
	OP_LOADP_F = 154,
	OP_LOADP_V = 155,
	OP_LOADP_S = 156,
	OP_LOADP_ENT = 157,
	OP_LOADP_FLD = 158,
	OP_LOADP_FNC = 159,
	OP_LOADP_I = 160,
	OP_LE_I = 161,
	OP_GE_I = 162,
	OP_LT_I = 163,
	OP_GT_I = 164,
	OP_LE_IF = 165,
	OP_GE_IF = 166,
	OP_LT_IF = 167,
	OP_GT_IF = 168,
	OP_LE_FI = 169,
	OP_GE_FI = 170,
	OP_LT_FI = 171,
	OP_GT_FI = 172,
	OP_EQ_IF = 173,
	OP_EQ_FI = 174,
	// OP_ADD_SF = 175,
	// OP_SUB_S = 176,
	// OP_STOREP_C = 177,
	// OP_LOADP_C = 178,
	OP_MUL_IF = 179,
	OP_MUL_FI = 180,
	OP_MUL_VI = 181,
	// OP_MUL_IV = 182,
	OP_DIV_IF = 183,
	OP_DIV_FI = 184,
	OP_BITAND_IF = 185,
	OP_BITOR_IF = 186,
	OP_BITAND_FI = 187,
	OP_BITOR_FI = 188,
	OP_AND_I = 189,
	OP_OR_I = 190,
	OP_AND_IF = 191,
	OP_OR_IF = 192,
	OP_AND_FI = 193,
	OP_OR_FI = 194,
	OP_NE_IF = 195,
	OP_NE_FI = 196,
	OP_GSTOREP_I = 197,
	OP_GSTOREP_F = 198,
	OP_GSTOREP_ENT = 199,
	OP_GSTOREP_FLD = 200,
	OP_GSTOREP_S = 201,
	OP_GSTOREP_FNC = 202,
	OP_GSTOREP_V = 203,
	// OP_GADDRESS = 204,
	OP_GLOAD_I = 205,
	OP_GLOAD_F = 206,
	OP_GLOAD_FLD = 207,
	OP_GLOAD_ENT = 208,
	OP_GLOAD_S = 209,
	OP_GLOAD_FNC = 210,
	OP_BOUNDCHECK = 211,
	// OP_UNUSED = 212,
	// OP_PUSH = 213,
	// OP_POP = 214,
	// OP_SWITCH_I = 215,
	OP_GLOAD_V = 216,
	// OP_IF_F = 217,
	// OP_IFNOT_F = 218,
	// OP_STOREF_V = 219,
	// OP_STOREF_F = 220,
	// OP_STOREF_S = 221,
	// OP_STOREF_I = 222,
	// OP_STOREP_I8 = 223,
	// OP_LOADP_U8 = 224,
	OP_LE_U = 225,
	OP_LT_U = 226,
	OP_DIV_U = 227,
	OP_RSHIFT_U = 228,
	// OP_ADD_I64 = 229,
	// OP_SUB_I64 = 230,
	// OP_MUL_I64 = 231,
	// OP_DIV_I64 = 232,
	// OP_BITAND_I64 = 233,
	// OP_BITOR_I64 = 234,
	// OP_BITXOR_I64 = 235,
	// OP_LSHIFT_I64I = 236,
	// OP_RSHIFT_I64I = 237,
	// OP_LE_I64 = 238,
	// OP_LT_I64 = 239,
	// OP_EQ_I64 = 240,
	// OP_NE_I64 = 241,
	// OP_LE_U64 = 242,
	// OP_LT_U64 = 243,
	// OP_DIV_U64 = 244,
	// OP_RSHIFT_U64I = 245,
	// OP_STORE_I64 = 246,
	// OP_STOREP_I64 = 247,
	// OP_STOREF_I64 = 248,
	// OP_LOADF_I64 = 249,
	// OP_LOADA_I64 = 250,
	// OP_LOADP_I64 = 251,
	// OP_CONV_UI64 = 252,
	// OP_CONV_II64 = 253,
	// OP_CONV_I64I = 254,
	// OP_CONV_FD = 255,
	// OP_CONV_DF = 256,
	// OP_CONV_I64F = 257,
	// OP_CONV_FI64 = 258,
	// OP_CONV_I64D = 259,
	// OP_CONV_DI64 = 260,
	// OP_ADD_D = 261,
	// OP_SUB_D = 262,
	// OP_MUL_D = 263,
	// OP_DIV_D = 264,
	// OP_LE_D = 265,
	// OP_LT_D = 266,
	// OP_EQ_D = 267,
	// OP_NE_D = 268,
	// OP_STOREP_I16 = 269,
	// OP_LOADP_I16 = 270,
	// OP_LOADP_U16 = 271,
	// OP_LOADP_I8 = 272,
	// OP_BITEXTEND_I = 273,
	// OP_BITEXTEND_U = 274,
	// OP_BITCOPY_I = 275,
	// OP_CONV_UF = 276,
	// OP_CONV_FU = 277,
	// OP_CONV_U64D = 278,
	// OP_CONV_DU64 = 279,
	// OP_CONV_U64F = 280,
	// OP_CONV_FU64 = 281,
	OP_NUMREALOPS = 282
}
opcode_t;

// Statements (16 bit format) - 8 bytes each
typedef struct statement16_s
{
	uint16_t	op;
	int16_t		a, b, c;
}
dstatement16_t;

// Statements (32 bit format) - 16 bytes each
typedef struct statement32_s
{
	uint32_t	op;
	int32_t		a, b, c;
}
dstatement32_t;

// Global and fielddefs (16 bit format) - 8 bytes each
typedef struct ddef16_s
{
	uint16_t	type;		// if DEF_SAVEGLOBAL bit is set
							// the variable needs to be saved in savegames
	uint16_t	ofs;
	int32_t	s_name;
}
ddef16_t, dfield16_t;

// Global and fielddefs (32 bit format) - 12 bytes each
typedef struct ddef32_s
{
	uint32_t	type;		// if DEF_SAVEGLOBAL bit is set
							// the variable needs to be saved in savegames
	uint32_t	ofs;
	int32_t	s_name;
}
ddef32_t, dfield32_t, mdef_t;

#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

// Functions - 36 bytes each
typedef struct dfunction_s
{
	int32_t			first_statement;	// negative numbers are builtins
	int32_t		parm_start;			// first local
	int32_t		locals;				// total ints of parms + locals

	int32_t		profile;		// runtime

	int32_t		s_name;			// function name
	int32_t		s_file;			// source file defined in

	int32_t			numparms;		// number of args
	uint8_t			parm_size[MAX_PARMS]; // and size
}
dfunction_t;

typedef struct mfunction_s
{
	int32_t			first_statement;	// negative numbers are builtins
	int32_t		parm_start;
	int32_t		locals;				// total ints of parms + locals

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

	int32_t		s_name;
	int32_t		s_file;			// source file defined in

	int32_t		numparms;
	uint8_t		parm_size[MAX_PARMS];
}
mfunction_t;

typedef struct mstatement_s
{
	opcode_t	op;
	int			operand[3]; // always a global, or a relative statement offset ([0] for GOTO, [1] for IF/IFNOT), or -1 for unused
}
mstatement_t;

// Header - 64 bytes
#define	PROG_VERSION	6
typedef struct dprograms_s
{
	int32_t	version;		// Version (usually 6)
	int32_t	crc;			// CRC-16 of header file. Last 2 bytes are skipped

	// Sizes of and offsets to each section
	uint32_t	ofs_statements;
	uint32_t	numstatements;	// statement 0 is an error

	uint32_t	ofs_globaldefs;
	uint32_t	numglobaldefs;

	uint32_t	ofs_fielddefs;
	uint32_t	numfielddefs;

	uint32_t	ofs_functions;
	uint32_t	numfunctions;	// function 0 is an empty

	uint32_t	ofs_strings;
	uint32_t	numstrings;		// first string is a null string

	uint32_t	ofs_globals;
	uint32_t	numglobals;

	uint32_t	entityfields;
}
dprograms_t;

typedef struct dprograms_v7_s
{	//extended header written by fteqcc.
	dprograms_t	v6;	//for easier casting.

	//debug / version 7 extensions
	uint32_t	ofsfiles;			//ignored. deprecated, should be 0. source files can instead be embedded by simply treating the .dat as a zip.
	uint32_t	ofslinenums;		//ignored. alternative to external .lno files.
	uint32_t	ofsbodylessfuncs;	//unsupported. function names imported from other modules. must be 0.
	uint32_t	numbodylessfuncs;	//unsupported. must be 0.

	uint32_t	ofs_types;			//unsupported+deprecated. rich type info. must be 0.
	uint32_t	numtypes;			//unsupported+deprecated. rich type info. must be 0.
	uint32_t	blockscompressed;	//unsupported. per-block compression. must be 0.

	int32_t		secondaryversion;	//if not known then its kkqwsv's v7, qfcc's v7, or uhexen2's v7, or something. abandon all hope when not recognised.
#define PROG_SECONDARYVERSION16 ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('P'<<0)|('R'<<8)|('O'<<16)|('G'<<24)))	//regular 16bit statements.
#define PROG_SECONDARYVERSION32 ((('1'<<0)|('F'<<8)|('T'<<16)|('E'<<24))^(('3'<<0)|('2'<<8)|('B'<<16)|(' '<<24)))	//statements+globaldefs+fielddefs extended to 32bit.
}
dprograms_v7_t;

#endif

