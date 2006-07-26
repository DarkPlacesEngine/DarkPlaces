#include "prvm_cmds.h"

//============================================================================
// Menu

char *vm_m_extensions =
"DP_CINEMATIC_DPV";

/*
=========
VM_M_precache_file

string	precache_file(string)
=========
*/
void VM_M_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	VM_SAFEPARMCOUNT(1,VM_precache_file);

	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
=========
VM_M_preache_error

used instead of the other VM_precache_* functions in the builtin list
=========
*/

void VM_M_precache_error (void)
{
	PRVM_ERROR ("PF_Precache_*: Precache can only be done in spawn functions");
}

/*
=========
VM_M_precache_sound

string	precache_sound (string sample)
=========
*/
void VM_M_precache_sound (void)
{
	const char	*s;

	VM_SAFEPARMCOUNT(1, VM_precache_sound);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
	VM_CheckEmptyString (s);

	if(snd_initialized.integer && !S_PrecacheSound (s,true, true))
	{
		VM_Warning("VM_precache_sound: Failed to load %s for %s\n", s, PRVM_NAME);
		return;
	}
}

/*
=========
VM_M_setmousetarget

setmousetarget(float target)
=========
*/
void VM_M_setmousetarget(void)
{
	VM_SAFEPARMCOUNT(1, VM_M_setmousetarget);

	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case 1:
		in_client_mouse = false;
		break;
	case 2:
		in_client_mouse = true;
		break;
	default:
		PRVM_ERROR("VM_M_setmousetarget: wrong destination %f !",PRVM_G_FLOAT(OFS_PARM0));
	}
}

/*
=========
VM_M_getmousetarget

float	getmousetarget
=========
*/
void VM_M_getmousetarget(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_getmousetarget);

	if(in_client_mouse)
		PRVM_G_FLOAT(OFS_RETURN) = 2;
	else
		PRVM_G_FLOAT(OFS_RETURN) = 1;
}



/*
=========
VM_M_setkeydest

setkeydest(float dest)
=========
*/
void VM_M_setkeydest(void)
{
	VM_SAFEPARMCOUNT(1,VM_M_setkeydest);

	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case 0:
		// key_game
		key_dest = key_game;
		break;
	case 2:
		// key_menu
		key_dest = key_menu;
		break;
	case 1:
		// key_message
		// key_dest = key_message
		// break;
	default:
		PRVM_ERROR("VM_M_setkeydest: wrong destination %f !", PRVM_G_FLOAT(OFS_PARM0));
	}
}

/*
=========
VM_M_getkeydest

float	getkeydest
=========
*/
void VM_M_getkeydest(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_getkeydest);

	// key_game = 0, key_message = 1, key_menu = 2, unknown = 3
	switch(key_dest)
	{
	case key_game:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		break;
	case key_message:
		// not supported
		// PRVM_G_FLOAT(OFS_RETURN) = 1;
		// break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = 3;
	}
}

/*
=========
VM_M_callfunction

	callfunction(...,string function_name)
Extension: pass
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_M_callfunction(void)
{
	mfunction_t *func;
	const char *s;

	if(prog->argc == 0)
		PRVM_ERROR("VM_M_callfunction: 1 parameter is required !");

	s = PRVM_G_STRING(OFS_PARM0 + (prog->argc - 1));

	if(!s)
		PRVM_ERROR("VM_M_callfunction: null string !");

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_ERROR("VM_M_callfunciton: function %s not found !", s);
	else if (func->first_statement < 0)
	{
		// negative statements are built in functions
		int builtinnumber = -func->first_statement;
		prog->xfunction->builtinsprofile++;
		if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
			prog->builtins[builtinnumber]();
		else
			PRVM_ERROR("No such builtin #%i in %s", builtinnumber, PRVM_NAME);
	}
	else if(func > 0)
	{
		prog->argc--;
		PRVM_ExecuteProgram(func - prog->functions,"");
		prog->argc++;
	}
}

/*
=========
VM_M_isfunction

float	isfunction(string function_name)
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_M_isfunction(void)
{
	mfunction_t *func;
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_M_isfunction);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!s)
		PRVM_ERROR("VM_M_isfunction: null string !");

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_G_FLOAT(OFS_RETURN) = false;
	else
		PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
VM_M_writetofile

	writetofile(float fhandle, entity ent)
=========
*/
void VM_M_writetofile(void)
{
	prvm_edict_t * ent;
	qfile_t *file;

	VM_SAFEPARMCOUNT(2, VM_M_writetofile);

	file = VM_GetFileHandle( (int)PRVM_G_FLOAT(OFS_PARM0) );
	if( !file )
	{
		VM_Warning("VM_M_writetofile: invalid or closed file handle\n");
		return;
	}

	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->priv.required->free)
	{
		VM_Warning("VM_M_writetofile: %s: entity %i is free !\n", PRVM_NAME, PRVM_EDICT_NUM(OFS_PARM1));
		return;
	}

	PRVM_ED_Write (file, ent);
}

/*
=========
VM_M_getresolution

vector	getresolution(float number)
=========
*/
extern unsigned short video_resolutions[][2];
void VM_M_getresolution(void)
{
	int nr;
	VM_SAFEPARMCOUNT(1, VM_getresolution);

	nr = (int)PRVM_G_FLOAT(OFS_PARM0);


	PRVM_G_VECTOR(OFS_RETURN)[0] = video_resolutions[nr][0];
	PRVM_G_VECTOR(OFS_RETURN)[1] = video_resolutions[nr][1];
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_M_findkeysforcommand

string	findkeysforcommand(string command)

the returned string is an altstring
=========
*/
#define NUMKEYS 5 // TODO: merge the constant in keys.c with this one somewhen

void M_FindKeysForCommand(const char *command, int *keys);
void VM_M_findkeysforcommand(void)
{
	const char *cmd;
	char *ret;
	int keys[NUMKEYS];
	int i;

	VM_SAFEPARMCOUNT(1, VM_M_findkeysforcommand);

	cmd = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(cmd);

	(ret = VM_GetTempString())[0] = 0;

	M_FindKeysForCommand(cmd, keys);

	for(i = 0; i < NUMKEYS; i++)
		ret = strcat(ret, va(" \'%i\'", keys[i]));

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(ret);
}

/*
=========
VM_M_getserverliststat

float	getserverliststat(float type)
=========
*/
/*
	type:
0	serverlist_viewcount
1   serverlist_totalcount
2	masterquerycount
3	masterreplycount
4	serverquerycount
5	serverreplycount
6	sortfield
7	sortdescending
*/
void VM_M_getserverliststat( void )
{
	int type;
	VM_SAFEPARMCOUNT ( 1, VM_M_getserverliststat );

	PRVM_G_FLOAT( OFS_RETURN ) = 0;

	type = (int)PRVM_G_FLOAT( OFS_PARM0 );
	switch(type)
	{
	case 0:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_viewcount;
		return;
	case 1:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_cachecount;
	case 2:
		PRVM_G_FLOAT ( OFS_RETURN ) = masterquerycount;
		return;
	case 3:
		PRVM_G_FLOAT ( OFS_RETURN ) = masterreplycount;
		return;
	case 4:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverquerycount;
		return;
	case 5:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverreplycount;
		return;
	case 6:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_sortbyfield;
		return;
	case 7:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_sortdescending;
		return;
	default:
		VM_Warning( "VM_M_getserverliststat: bad type %i!\n", type );
	}
}

/*
========================
VM_M_resetserverlistmasks

resetserverlistmasks()
========================
*/
void VM_M_resetserverlistmasks( void )
{
	ServerList_ResetMasks();
}


/*
========================
VM_M_setserverlistmaskstring

setserverlistmaskstring(float mask, float fld, string str, float op)
0-511		and
512 - 1024	or
========================
*/
void VM_M_setserverlistmaskstring( void )
{
	const char *str;
	int masknr;
	serverlist_mask_t *mask;
	int field;

	VM_SAFEPARMCOUNT( 4, VM_M_setserverlistmaskstring );
	str = PRVM_G_STRING( OFS_PARM2 );
	if( !str )
		PRVM_ERROR( "VM_M_setserverlistmaskstring: null string passed!" );

	masknr = (int)PRVM_G_FLOAT( OFS_PARM0 );
	if( masknr >= 0 && masknr <= SERVERLIST_ANDMASKCOUNT )
		mask = &serverlist_andmasks[masknr];
	else if( masknr >= 512 && masknr - 512 <= SERVERLIST_ORMASKCOUNT )
		mask = &serverlist_ormasks[masknr - 512 ];
	else
	{
		VM_Warning( "VM_M_setserverlistmaskstring: invalid mask number %i\n", masknr );
		return;
	}

	field = (int) PRVM_G_FLOAT( OFS_PARM1 );

	switch( field ) {
		case SLIF_CNAME:
			strncpy( mask->info.cname, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.cname) );
			break;
		case SLIF_NAME:
			strncpy( mask->info.name, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.name)  );
			break;
		case SLIF_MAP:
			strncpy( mask->info.map, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.map)  );
			break;
		case SLIF_MOD:
			strncpy( mask->info.mod, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.mod)  );
			break;
		case SLIF_GAME:
			strncpy( mask->info.game, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.game)  );
			break;
		default:
			VM_Warning( "VM_M_setserverlistmaskstring: Bad field number %i passed!\n", field );
			return;
	}

	mask->active = true;
	mask->tests[field] = (serverlist_maskop_t)((int)PRVM_G_FLOAT( OFS_PARM3 ));
}

/*
========================
VM_M_setserverlistmasknumber

setserverlistmasknumber(float mask, float fld, float num, float op)

0-511		and
512 - 1024	or
========================
*/
void VM_M_setserverlistmasknumber( void )
{
	int number;
	serverlist_mask_t *mask;
	int	masknr;
	int field;
	VM_SAFEPARMCOUNT( 4, VM_M_setserverlistmasknumber );

	masknr = (int)PRVM_G_FLOAT( OFS_PARM0 );
	if( masknr >= 0 && masknr <= SERVERLIST_ANDMASKCOUNT )
		mask = &serverlist_andmasks[masknr];
	else if( masknr >= 512 && masknr - 512 <= SERVERLIST_ORMASKCOUNT )
		mask = &serverlist_ormasks[masknr - 512 ];
	else
	{
		VM_Warning( "VM_M_setserverlistmasknumber: invalid mask number %i\n", masknr );
		return;
	}

	number = (int)PRVM_G_FLOAT( OFS_PARM2 );
	field = (int) PRVM_G_FLOAT( OFS_PARM1 );

	switch( field ) {
		case SLIF_MAXPLAYERS:
			mask->info.maxplayers = number;
			break;
		case SLIF_NUMPLAYERS:
			mask->info.numplayers = number;
			break;
		case SLIF_PING:
			mask->info.ping = number;
			break;
		case SLIF_PROTOCOL:
			mask->info.protocol = number;
			break;
		default:
			VM_Warning( "VM_M_setserverlistmasknumber: Bad field number %i passed!\n", field );
			return;
	}

	mask->active = true;
	mask->tests[field] = (serverlist_maskop_t)((int)PRVM_G_FLOAT( OFS_PARM3 ));
}


/*
========================
VM_M_resortserverlist

resortserverlist
========================
*/
void VM_M_resortserverlist( void )
{
	ServerList_RebuildViewList();
}

/*
=========
VM_M_getserverliststring

string	getserverliststring(float field, float hostnr)
=========
*/
void VM_M_getserverliststring(void)
{
	serverlist_entry_t *cache;
	int hostnr;

	VM_SAFEPARMCOUNT(2, VM_M_getserverliststring);

	PRVM_G_INT(OFS_RETURN) = 0;

	hostnr = (int)PRVM_G_FLOAT(OFS_PARM1);

	if(hostnr < 0 || hostnr >= serverlist_viewcount)
	{
		Con_Print("VM_M_getserverliststring: bad hostnr passed!\n");
		return;
	}
	cache = serverlist_viewlist[hostnr];
	switch( (int) PRVM_G_FLOAT(OFS_PARM0) ) {
		case SLIF_CNAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->info.cname );
			break;
		case SLIF_NAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->info.name );
			break;
		case SLIF_GAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->info.game );
			break;
		case SLIF_MOD:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->info.mod );
			break;
		case SLIF_MAP:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->info.map );
			break;
		// TODO remove this again
		case 1024:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->line1 );
			break;
		case 1025:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( cache->line2 );
			break;
		default:
			Con_Print("VM_M_getserverliststring: bad field number passed!\n");
	}
}

/*
=========
VM_M_getserverlistnumber

float	getserverlistnumber(float field, float hostnr)
=========
*/
void VM_M_getserverlistnumber(void)
{
	serverlist_entry_t *cache;
	int hostnr;

	VM_SAFEPARMCOUNT(2, VM_M_getserverliststring);

	PRVM_G_INT(OFS_RETURN) = 0;

	hostnr = (int)PRVM_G_FLOAT(OFS_PARM1);

	if(hostnr < 0 || hostnr >= serverlist_viewcount)
	{
		Con_Print("VM_M_getserverliststring: bad hostnr passed!\n");
		return;
	}
	cache = serverlist_viewlist[hostnr];
	switch( (int) PRVM_G_FLOAT(OFS_PARM0) ) {
		case SLIF_MAXPLAYERS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.maxplayers;
			break;
		case SLIF_NUMPLAYERS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.numplayers;
			break;
		case SLIF_PING:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.ping;
			break;
		case SLIF_PROTOCOL:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.protocol;
			break;
		default:
			Con_Print("VM_M_getserverlistnumber: bad field number passed!\n");
	}
}

/*
========================
VM_M_setserverlistsort

setserverlistsort(float field, float descending)
========================
*/
void VM_M_setserverlistsort( void )
{
	VM_SAFEPARMCOUNT( 2, VM_M_setserverlistsort );

	serverlist_sortbyfield = (serverlist_infofield_t)((int)PRVM_G_FLOAT( OFS_PARM0 ));
	serverlist_sortdescending = (qboolean) PRVM_G_FLOAT( OFS_PARM1 );
}

/*
========================
VM_M_refreshserverlist

refreshserverlist()
========================
*/
void VM_M_refreshserverlist( void )
{
	VM_SAFEPARMCOUNT( 0, VM_M_refreshserverlist );
	ServerList_QueryList(true, false);
}

/*
========================
VM_M_getserverlistindexforkey

float getserverlistindexforkey(string key)
========================
*/
void VM_M_getserverlistindexforkey( void )
{
	const char *key;
	VM_SAFEPARMCOUNT( 1, VM_M_getserverlistindexforkey );

	key = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( key );

	if( !strcmp( key, "cname" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_CNAME;
	else if( !strcmp( key, "ping" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PING;
	else if( !strcmp( key, "game" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_GAME;
	else if( !strcmp( key, "mod" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MOD;
	else if( !strcmp( key, "map" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MAP;
	else if( !strcmp( key, "name" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NAME;
	else if( !strcmp( key, "maxplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MAXPLAYERS;
	else if( !strcmp( key, "numplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NUMPLAYERS;
	else if( !strcmp( key, "protocol" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PROTOCOL;
	else
		PRVM_G_FLOAT( OFS_RETURN ) = -1;
}

/*
========================
VM_M_addwantedserverlistkey

addwantedserverlistkey(string key)
========================
*/
void VM_M_addwantedserverlistkey( void )
{
	VM_SAFEPARMCOUNT( 1, VM_M_addwantedserverlistkey );
}

/*
===============================================================================
MESSAGE WRITING

used only for client and menu
severs uses VM_SV_...

Write*(* data, float type, float to)

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *VM_WriteDest (void)
{
	int		dest;
	int		destclient;

	if(!sv.active)
		PRVM_ERROR("VM_WriteDest: game is not server (%s)", PRVM_NAME);

	dest = (int)PRVM_G_FLOAT(OFS_PARM1);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		destclient = (int) PRVM_G_FLOAT(OFS_PARM2);
		if (destclient < 0 || destclient >= svs.maxclients || !svs.clients[destclient].active || !svs.clients[destclient].netconnection)
			PRVM_ERROR("VM_clientcommand: %s: invalid client !", PRVM_NAME);

		return &svs.clients[destclient].netconnection->message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;

	default:
		PRVM_ERROR ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

void VM_M_WriteByte (void)
{
	MSG_WriteByte (VM_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteChar (void)
{
	MSG_WriteChar (VM_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteShort (void)
{
	MSG_WriteShort (VM_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteLong (void)
{
	MSG_WriteLong (VM_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteAngle (void)
{
	MSG_WriteAngle (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_M_WriteCoord (void)
{
	MSG_WriteCoord (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_M_WriteString (void)
{
	MSG_WriteString (VM_WriteDest(), PRVM_G_STRING(OFS_PARM0));
}

void VM_M_WriteEntity (void)
{
	MSG_WriteShort (VM_WriteDest(), PRVM_G_EDICTNUM(OFS_PARM0));
}

prvm_builtin_t vm_m_builtins[] = {
	0, // to be consistent with the old vm
	// common builtings (mostly)
	VM_checkextension,
	VM_error,
	VM_objerror,
	VM_print,
	VM_bprint,
	VM_sprint,
	VM_centerprint,
	VM_normalize,
	VM_vlen,
	VM_vectoyaw,	// #10
	VM_vectoangles,
	VM_random,
	VM_localcmd,
	VM_cvar,
	VM_cvar_set,
	VM_dprint,
	VM_ftos,
	VM_fabs,
	VM_vtos,
	VM_etos,		// 20
	VM_stof,
	VM_spawn,
	VM_remove,
	VM_find,
	VM_findfloat,
	VM_findchain,
	VM_findchainfloat,
	VM_M_precache_file,
	VM_M_precache_sound,
	VM_coredump,	// 30
	VM_traceon,
	VM_traceoff,
	VM_eprint,
	VM_rint,
	VM_floor,
	VM_ceil,
	VM_nextent,
	VM_sin,
	VM_cos,
	VM_sqrt,		// 40
	VM_randomvec,
	VM_registercvar,
	VM_min,
	VM_max,
	VM_bound,
	VM_pow,
	VM_copyentity,
	VM_fopen,
	VM_fclose,
	VM_fgets,		// 50
	VM_fputs,
	VM_strlen,
	VM_strcat,
	VM_substring,
	VM_stov,
	VM_strzone,
	VM_strunzone,
	VM_tokenize,
	VM_argv,
	VM_isserver,	// 60
	VM_clientcount,
	VM_clientstate,
	VM_clcommand,
	VM_changelevel,
	VM_localsound,
	VM_getmousepos,
	VM_gettime,
	VM_loadfromdata,
	VM_loadfromfile,
	VM_modulo,		// 70
	VM_cvar_string,
	VM_crash,
	VM_stackdump,	// 73
	VM_search_begin,
	VM_search_end,
	VM_search_getsize,
	VM_search_getfilename, // 77
	VM_chr,
	VM_itof,
	VM_ftoi,		// 80
	VM_itof,		// isString
	VM_altstr_count,
	VM_altstr_prepare,
	VM_altstr_get,
	VM_altstr_set,
	VM_altstr_ins,
	VM_findflags,
	VM_findchainflags,
	VM_cvar_defstring, // 89
	0, // 90
	e10,			// 100
	e100,			// 200
	e100,			// 300
	e100,			// 400
	// msg functions
	VM_M_WriteByte,
	VM_M_WriteChar,
	VM_M_WriteShort,
	VM_M_WriteLong,
	VM_M_WriteAngle,
	VM_M_WriteCoord,
	VM_M_WriteString,
	VM_M_WriteEntity,	// 408
	0,
	0,				// 410
	e10,			// 420
	e10,			// 430
	e10,			// 440
	e10,			// 450
	// draw functions
	VM_iscachedpic,
	VM_precache_pic,
	VM_freepic,
	VM_drawcharacter,
	VM_drawstring,
	VM_drawpic,
	VM_drawfill,
	VM_drawsetcliparea,
	VM_drawresetcliparea,
	VM_getimagesize,// 460
	VM_cin_open,
	VM_cin_close,
	VM_cin_setstate,
	VM_cin_getstate,
	VM_cin_restart, // 465
	VM_drawline,	// 466
	0,0,0,0,	// 470
	e10,			// 480
	e10,			// 490
	e10,			// 500
	e100,			// 600
	// menu functions
	VM_M_setkeydest,
	VM_M_getkeydest,
	VM_M_setmousetarget,
	VM_M_getmousetarget,
	VM_M_callfunction,
	VM_M_writetofile,
	VM_M_isfunction,
	VM_M_getresolution,
	VM_keynumtostring,
	VM_M_findkeysforcommand,// 610
	VM_M_getserverliststat,
	VM_M_getserverliststring,
	VM_parseentitydata,
	VM_stringtokeynum,
	VM_M_resetserverlistmasks,
	VM_M_setserverlistmaskstring,
	VM_M_setserverlistmasknumber,
	VM_M_resortserverlist,
	VM_M_setserverlistsort,
	VM_M_refreshserverlist,
	VM_M_getserverlistnumber,
	VM_M_getserverlistindexforkey,
	VM_M_addwantedserverlistkey // 623
};

const int vm_m_numbuiltins = sizeof(vm_m_builtins) / sizeof(prvm_builtin_t);

void VM_M_Cmd_Init(void)
{
	VM_Cmd_Init();
}

void VM_M_Cmd_Reset(void)
{
	//VM_Cmd_Init();
	VM_Cmd_Reset();
}
