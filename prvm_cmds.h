// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here


/*
============================================================================
common cmd list:
=================

		checkextension(string)
		error(...[string])
		objerror(...[string)
		print(...[strings])
		bprint(...[string])
		sprint(float clientnum,...[string])
		centerprint(...[string])
vector	normalize(vector)
float	vlen(vector)
float	vectoyaw(vector)
vector	vectoangles(vector)
float	random()
		cmd(string)
		float cvar (string)
		cvar_set (string,string)
		dprint(...[string])
string	ftos(float)
float	fabs(float)
string	vtos(vector)
string	etos(entity)
float	stof(...[string])
entity	spawn()
		remove(entity e)
entity	find(entity start, .string field, string match)

entity	findfloat(entity start, .float field, float match)
entity	findentity(entity start, .entity field, entity match)

entity	findchain(.string field, string match)

entity	findchainfloat(.string field, float match)
entity	findchainentity(.string field, entity match)

string	precache_file(string)
string	precache_sound (string sample)
		coredump()
		traceon()
		traceoff()
		eprint(entity e)
float	rint(float)
float	floor(float)
float	ceil(float)
entity	nextent(entity)
float	sin(float)
float	cos(float)
float	sqrt(float)
vector	randomvec()
float	registercvar (string name, string value, float flags)
float	min(float a, float b, ...[float])
float	max(float a, float b, ...[float])
float	bound(float min, float value, float max)
float	pow(float a, float b)
		copyentity(entity src, entity dst)
float	fopen(string filename, float mode)
		fclose(float fhandle)
string	fgets(float fhandle)
		fputs(float fhandle, string s)
float	strlen(string s)
string	strcat(string,string,...[string])
string	substring(string s, float start, float length)
vector	stov(string s)
string	strzone(string s)
		strunzone(string s)
float	tokenize(string s)
string	argv(float n)
float	isserver()
float	clientcount()
float	clientstate()
		clientcommand(float client, string s) (for client and menu)
		changelevel(string map)
		localsound(string sample)
vector	getmousepos()
float	gettime()
		loadfromdata(string data)
		loadfromfile(string file)
		parseentitydata(entity ent, string data)
float	mod(float val, float m)
const string	cvar_string (string)
		crash()
		stackdump()

float	search_begin(string pattern, float caseinsensitive, float quiet)
void	search_end(float handle)
float	search_getsize(float handle)
string	search_getfilename(float handle, float num)

string	chr(float ascii)

float	itof(intt ent)
entity	ftoe(float num)

-------will be removed soon----------
float	altstr_count(string)
string	altstr_prepare(string)
string	altstr_get(string,float)
string	altstr_set(string altstr, float num, string set)
string	altstr_ins(string altstr, float num, string set)
--------------------------------------

entity	findflags(entity start, .float field, float match)
entity	findchainflags(.float field, float match)

const string	VM_cvar_defstring (string)

perhaps only : Menu : WriteMsg
===============================

		WriteByte(float data, float dest, float desto)
		WriteChar(float data, float dest, float desto)
		WriteShort(float data, float dest, float desto)
		WriteLong(float data, float dest, float desto)
		WriteAngle(float data, float dest, float desto)
		WriteCoord(float data, float dest, float desto)
		WriteString(string data, float dest, float desto)
		WriteEntity(entity data, float dest, float desto)

Client & Menu : draw functions & video functions
===================================================

float	iscachedpic(string pic)
string	precache_pic(string pic)
		freepic(string s)
float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag)
float	drawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag)
float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag)
float	drawfill(vector position, vector size, vector rgb, float alpha, float flag)
		drawsetcliparea(float x, float y, float width, float height)
		drawresetcliparea()
vector	getimagesize(string pic)

float	cin_open(string file, string name)
void	cin_close(string name)
void	cin_setstate(string name, float type)
float	cin_getstate(string name)
void	cin_restart(string name)

==============================================================================
menu cmd list:
===============

		setkeydest(float dest)
float	getkeydest()
		setmousetarget(float target)
float	getmousetarget()

		callfunction(...,string function_name)
		writetofile(float fhandle, entity ent)
float	isfunction(string function_name)
vector	getresolution(float number)
string	keynumtostring(float keynum)
string	findkeysforcommand(string command)
float	getserverliststat(float type)
string	getserverliststring(float fld, float hostnr)

float	stringtokeynum(string key)

		resetserverlistmasks()
		setserverlistmaskstring(float mask, float fld, string str)
		setserverlistmasknumber(float mask, float fld, float num, float op)
		resortserverlist()
		setserverlistsort(float field, float descending)
		refreshserverlist()
float	getserverlistnumber(float fld, float hostnr)
float	getserverlistindexforkey(string key)
		addwantedserverlistkey(string key)
*/

#include "quakedef.h"
#include "progdefs.h"
#include "progsvm.h"
#include "clprogdefs.h"
#include "mprogdefs.h"

#include "cl_video.h"

//============================================================================
// nice helper macros

#ifndef VM_NOPARMCHECK
#define VM_SAFEPARMCOUNTRANGE(p1,p2,f)	if(prog->argc < p1 || prog->argc > p2) PRVM_ERROR(#f " wrong parameter count %i (" #p1 " to " #p2 " expected ) !", prog->argc)
#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f " wrong parameter count %i (" #p " expected ) !", prog->argc)
#else
#define VM_SAFEPARMCOUNTRANGE(p1,p2,f)
#define VM_SAFEPARMCOUNT(p,f)
#endif

#define	VM_RETURN_EDICT(e)		(((int *)prog->globals.generic)[OFS_RETURN] = PRVM_EDICT_TO_PROG(e))

#define VM_STRINGTEMP_LENGTH MAX_INPUTLINE

// builtins and other general functions

void VM_CheckEmptyString (const char *s);
void VM_VarString(int first, char *out, int outlength);

void VM_checkextension (void);
void VM_error (void);
void VM_objerror (void);
void VM_print (void);
void VM_bprint (void);
void VM_sprint (void);
void VM_centerprint (void);
void VM_normalize (void);
void VM_vlen (void);
void VM_vectoyaw (void);
void VM_vectoangles (void);
void VM_random (void);
void VM_localsound(void);
void VM_break (void);
void VM_localcmd (void);
void VM_cvar (void);
void VM_cvar_string(void);
void VM_cvar_defstring (void);
void VM_cvar_set (void);
void VM_dprint (void);
void VM_ftos (void);
void VM_fabs (void);
void VM_vtos (void);
void VM_etos (void);
void VM_stof(void);
void VM_itof(void);
void VM_ftoe(void);
void VM_strftime(void);
void VM_spawn (void);
void VM_remove (void);
void VM_find (void);
void VM_findfloat (void);
void VM_findchain (void);
void VM_findchainfloat (void);
void VM_findflags (void);
void VM_findchainflags (void);
void VM_precache_file (void);
void VM_precache_sound (void);
void VM_coredump (void);

void VM_stackdump (void);
void VM_crash(void); // REMOVE IT
void VM_traceon (void);
void VM_traceoff (void);
void VM_eprint (void);
void VM_rint (void);
void VM_floor (void);
void VM_ceil (void);
void VM_nextent (void);

void VM_changelevel (void);
void VM_sin (void);
void VM_cos (void);
void VM_sqrt (void);
void VM_randomvec (void);
void VM_registercvar (void);
void VM_min (void);
void VM_max (void);
void VM_bound (void);
void VM_pow (void);
void VM_asin (void);
void VM_acos (void);
void VM_atan (void);
void VM_atan2 (void);
void VM_tan (void);

void VM_Files_Init(void);
void VM_Files_CloseAll(void);

void VM_fopen(void);
void VM_fclose(void);
void VM_fgets(void);
void VM_fputs(void);
void VM_writetofile(void); // only used by menu

void VM_strlen(void);
void VM_strcat(void);
void VM_substring(void);
void VM_stov(void);
void VM_strzone(void);
void VM_strunzone(void);

// DRESK - String Length (not counting color codes)
void VM_strlennocol(void);
// DRESK - Decolorized String
void VM_strdecolorize(void);

void VM_clcommand (void);

void VM_tokenize (void);
void VM_tokenizebyseparator (void);
void VM_argv (void);

void VM_isserver(void);
void VM_clientcount(void);
void VM_clientstate(void);
// not used at the moment -> not included in the common list
void VM_getostype(void);
void VM_getmousepos(void);
void VM_gettime(void);
void VM_loadfromdata(void);
void VM_parseentitydata(void);
void VM_loadfromfile(void);
void VM_modulo(void);

void VM_search_begin(void);
void VM_search_end(void);
void VM_search_getsize(void);
void VM_search_getfilename(void);
void VM_chr(void);
void VM_iscachedpic(void);
void VM_precache_pic(void);
void VM_freepic(void);
void VM_drawcharacter(void);
void VM_drawstring(void);
void VM_drawpic(void);
void VM_drawfill(void);
void VM_drawsetcliparea(void);
void VM_drawresetcliparea(void);
void VM_getimagesize(void);

void VM_makevectors (void);
void VM_vectorvectors (void);

void VM_keynumtostring (void);
void VM_stringtokeynum (void);

void VM_cin_open( void );
void VM_cin_close( void );
void VM_cin_setstate( void );
void VM_cin_getstate( void );
void VM_cin_restart( void );

void VM_drawline (void);

void VM_bitshift (void);

void VM_altstr_count( void );
void VM_altstr_prepare( void );
void VM_altstr_get( void );
void VM_altstr_set( void );
void VM_altstr_ins(void);

void VM_buf_create(void);
void VM_buf_del (void);
void VM_buf_getsize (void);
void VM_buf_copy (void);
void VM_buf_sort (void);
void VM_buf_implode (void);
void VM_bufstr_get (void);
void VM_bufstr_set (void);
void VM_bufstr_add (void);
void VM_bufstr_free (void);

void VM_changeyaw (void);
void VM_changepitch (void);

void VM_uncolorstring (void);
void VM_str2chr (void);
void VM_chr2str (void);
void VM_strncmp (void);
void VM_registercvar (void);
void VM_wasfreed (void);

void VM_SetTraceGlobals(const trace_t *trace);

void VM_Cmd_Init(void);
void VM_Cmd_Reset(void);
