#include "quakedef.h"

#include "prvm_cmds.h"
#include "clvm_cmds.h"
#include "menu.h"

// TODO check which strings really should be engine strings

//============================================================================
// Menu

char *vm_m_extensions =
"BX_WAL_SUPPORT "
"DP_CINEMATIC_DPV "
"DP_FONT_VARIABLEWIDTH "
"DP_GECKO_SUPPORT "
"DP_MENU_EXTRESPONSEPACKET "
"DP_QC_ASINACOSATANATAN2TAN "
"DP_QC_CMD "
"DP_QC_CRC16 "
"DP_QC_CVAR_TYPE "
"DP_QC_CVAR_DESCRIPTION "
"DP_QC_FINDCHAIN_TOFIELD "
"DP_QC_RENDER_SCENE "
"DP_QC_STRFTIME "
"DP_QC_STRINGBUFFERS "
"DP_QC_STRINGBUFFERS_CVARLIST "
"DP_QC_STRINGCOLORFUNCTIONS "
"DP_QC_STRING_CASE_FUNCTIONS "
"DP_QC_STRREPLACE "
"DP_QC_TOKENIZEBYSEPARATOR "
"DP_QC_TOKENIZE_CONSOLE "
"DP_QC_UNLIMITEDTEMPSTRINGS "
"DP_QC_URI_ESCAPE "
"DP_QC_URI_GET "
"DP_QC_WHICHPACK "
"FTE_STRINGS "
;

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
	case 3:
		// key_menu_grabbed
		key_dest = key_menu_grabbed;
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

	// key_game = 0, key_message = 1, key_menu = 2, key_menu_grabbed = 3, unknown = -1
	switch(key_dest)
	{
	case key_game:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		break;
	case key_menu_grabbed:
		PRVM_G_FLOAT(OFS_RETURN) = 3;
		break;
	case key_message:
		// not supported
		// PRVM_G_FLOAT(OFS_RETURN) = 1;
		// break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = -1;
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

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_M_callfunction);

	s = PRVM_G_STRING(OFS_PARM0+(prog->argc - 1)*3);

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
			PRVM_ERROR("No such builtin #%i in %s; most likely cause: outdated engine build. Try updating!", builtinnumber, PRVM_NAME);
	}
	else if(func - prog->functions > 0)
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

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_G_FLOAT(OFS_RETURN) = false;
	else
		PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
VM_M_getresolution

vector	getresolution(float number)
=========
*/
void VM_M_getresolution(void)
{
	int nr, fs;
	VM_SAFEPARMCOUNTRANGE(1, 2, VM_getresolution);

	nr = (int)PRVM_G_FLOAT(OFS_PARM0);

	fs = ((prog->argc <= 1) || ((int)PRVM_G_FLOAT(OFS_PARM1)));

	if(nr < 0 || nr >= (fs ? video_resolutions_count : video_resolutions_hardcoded_count))
	{
		PRVM_G_VECTOR(OFS_RETURN)[0] = 0;
		PRVM_G_VECTOR(OFS_RETURN)[1] = 0;
		PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
	}
	else
	{
		PRVM_G_VECTOR(OFS_RETURN)[0] = (fs ? video_resolutions : video_resolutions_hardcoded)[nr].width;
		PRVM_G_VECTOR(OFS_RETURN)[1] = (fs ? video_resolutions : video_resolutions_hardcoded)[nr].height;
		PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
	}
}

void VM_M_getgamedirinfo(void)
{
	int nr, item;
	VM_SAFEPARMCOUNT(2, VM_getgamedirinfo);

	nr = (int)PRVM_G_FLOAT(OFS_PARM0);
	item = (int)PRVM_G_FLOAT(OFS_PARM1);

	PRVM_G_INT( OFS_RETURN ) = OFS_NULL;

	if(nr >= 0 && nr < fs_all_gamedirs_count)
	{
		if(item == 0)
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( fs_all_gamedirs[nr].name );
		else if(item == 1)
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( fs_all_gamedirs[nr].description );
	}
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
7	sortflags
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
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_sortflags;
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
	VM_SAFEPARMCOUNT(0, VM_M_resetserverlistmasks);
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
			strlcpy( mask->info.cname, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.cname) );
			break;
		case SLIF_NAME:
			strlcpy( mask->info.name, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.name)  );
			break;
		case SLIF_QCSTATUS:
			strlcpy( mask->info.qcstatus, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.qcstatus)  );
			break;
		case SLIF_PLAYERS:
			strlcpy( mask->info.players, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.players)  );
			break;
		case SLIF_MAP:
			strlcpy( mask->info.map, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.map)  );
			break;
		case SLIF_MOD:
			strlcpy( mask->info.mod, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.mod)  );
			break;
		case SLIF_GAME:
			strlcpy( mask->info.game, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.game)  );
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
		case SLIF_NUMBOTS:
			mask->info.numbots = number;
			break;
		case SLIF_NUMHUMANS:
			mask->info.numhumans = number;
			break;
		case SLIF_PING:
			mask->info.ping = number;
			break;
		case SLIF_PROTOCOL:
			mask->info.protocol = number;
			break;
		case SLIF_FREESLOTS:
			mask->info.freeslots = number;
			break;
		case SLIF_ISFAVORITE:
			mask->info.isfavorite = number;
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
	VM_SAFEPARMCOUNT(0, VM_M_resortserverlist);
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

	PRVM_G_INT(OFS_RETURN) = OFS_NULL;

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
		case SLIF_QCSTATUS:
			PRVM_G_INT (OFS_RETURN ) = PRVM_SetEngineString (cache->info.qcstatus );
			break;
		case SLIF_PLAYERS:
			PRVM_G_INT (OFS_RETURN ) = PRVM_SetEngineString (cache->info.players );
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

	PRVM_G_INT(OFS_RETURN) = OFS_NULL;

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
		case SLIF_NUMBOTS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.numbots;
			break;
		case SLIF_NUMHUMANS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.numhumans;
			break;
		case SLIF_FREESLOTS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.freeslots;
			break;
		case SLIF_PING:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.ping;
			break;
		case SLIF_PROTOCOL:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.protocol;
			break;
		case SLIF_ISFAVORITE:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.isfavorite;
			break;
		default:
			Con_Print("VM_M_getserverlistnumber: bad field number passed!\n");
	}
}

/*
========================
VM_M_setserverlistsort

setserverlistsort(float field, float flags)
========================
*/
void VM_M_setserverlistsort( void )
{
	VM_SAFEPARMCOUNT( 2, VM_M_setserverlistsort );

	serverlist_sortbyfield = (serverlist_infofield_t)((int)PRVM_G_FLOAT( OFS_PARM0 ));
	serverlist_sortflags = (int) PRVM_G_FLOAT( OFS_PARM1 );
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
	ServerList_QueryList(false, true, false, false);
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
	else if( !strcmp( key, "qcstatus" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_QCSTATUS;
	else if( !strcmp( key, "players" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PLAYERS;
	else if( !strcmp( key, "maxplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MAXPLAYERS;
	else if( !strcmp( key, "numplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NUMPLAYERS;
	else if( !strcmp( key, "numbots" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NUMBOTS;
	else if( !strcmp( key, "numhumans" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NUMHUMANS;
	else if( !strcmp( key, "freeslots" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_FREESLOTS;
	else if( !strcmp( key, "protocol" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PROTOCOL;
	else if( !strcmp( key, "isfavorite" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_ISFAVORITE;
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
server uses VM_SV_...

Write*(* data, float type, float to)

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *VM_M_WriteDest (void)
{
	int		dest;
	int		destclient;

	if(!sv.active)
		PRVM_ERROR("VM_M_WriteDest: game is not server (%s)", PRVM_NAME);

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
	VM_SAFEPARMCOUNT(1, VM_M_WriteByte);
	MSG_WriteByte (VM_M_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteChar (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteChar);
	MSG_WriteChar (VM_M_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteShort (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteShort);
	MSG_WriteShort (VM_M_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteLong (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteLong);
	MSG_WriteLong (VM_M_WriteDest(), (int)PRVM_G_FLOAT(OFS_PARM0));
}

void VM_M_WriteAngle (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteAngle);
	MSG_WriteAngle (VM_M_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_M_WriteCoord (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteCoord);
	MSG_WriteCoord (VM_M_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_M_WriteString (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteString);
	MSG_WriteString (VM_M_WriteDest(), PRVM_G_STRING(OFS_PARM0));
}

void VM_M_WriteEntity (void)
{
	VM_SAFEPARMCOUNT(1, VM_M_WriteEntity);
	MSG_WriteShort (VM_M_WriteDest(), PRVM_G_EDICTNUM(OFS_PARM0));
}

/*
=================
VM_M_copyentity

copies data from one entity to another

copyentity(entity src, entity dst)
=================
*/
static void VM_M_copyentity (void)
{
	prvm_edict_t *in, *out;
	VM_SAFEPARMCOUNT(2,VM_M_copyentity);
	in = PRVM_G_EDICT(OFS_PARM0);
	out = PRVM_G_EDICT(OFS_PARM1);
	memcpy(out->fields.vp, in->fields.vp, prog->progs->entityfields * 4);
}

//#66 vector() getmousepos (EXT_CSQC)
static void VM_M_getmousepos(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_getmousepos);

	if (key_consoleactive || (key_dest != key_menu && key_dest != key_menu_grabbed))
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), 0, 0, 0);
	else if (in_client_mouse)
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), in_windowmouse_x * vid_conwidth.integer / vid.width, in_windowmouse_y * vid_conheight.integer / vid.height, 0);
	else
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), in_mouse_x * vid_conwidth.integer / vid.width, in_mouse_y * vid_conheight.integer / vid.height, 0);
}

//#349 float() isdemo (EXT_CSQC)
static void VM_M_isdemo (void)
{
	VM_SAFEPARMCOUNT(0, VM_M_isdemo);
	PRVM_G_FLOAT(OFS_RETURN) = cls.demoplayback;
}

prvm_builtin_t vm_m_builtins[] = {
NULL,									//   #0 NULL function (not callable)
VM_checkextension,				//   #1
VM_error,							//   #2
VM_objerror,						//   #3
VM_print,							//   #4
VM_bprint,							//   #5
VM_sprint,							//   #6
VM_centerprint,					//   #7
VM_normalize,						//   #8
VM_vlen,								//   #9
VM_vectoyaw,						//  #10
VM_vectoangles,					//  #11
VM_random,							//  #12
VM_localcmd,						//  #13
VM_cvar,								//  #14
VM_cvar_set,						//  #15
VM_dprint,							//  #16
VM_ftos,								//  #17
VM_fabs,								//  #18
VM_vtos,								//  #19
VM_etos,								//  #20
VM_stof,								//  #21
VM_spawn,							//  #22
VM_remove,							//  #23
VM_find,								//  #24
VM_findfloat,						//  #25
VM_findchain,						//  #26
VM_findchainfloat,				//  #27
VM_precache_file,					//  #28
VM_precache_sound,				//  #29
VM_coredump,						//  #30
VM_traceon,							//  #31
VM_traceoff,						//  #32
VM_eprint,							//  #33
VM_rint,								//  #34
VM_floor,							//  #35
VM_ceil,								//  #36
VM_nextent,							//  #37
VM_sin,								//  #38
VM_cos,								//  #39
VM_sqrt,								//  #40
VM_randomvec,						//  #41
VM_registercvar,					//  #42
VM_min,								//  #43
VM_max,								//  #44
VM_bound,							//  #45
VM_pow,								//  #46
VM_M_copyentity,					//  #47
VM_fopen,							//  #48
VM_fclose,							//  #49
VM_fgets,							//  #50
VM_fputs,							//  #51
VM_strlen,							//  #52
VM_strcat,							//  #53
VM_substring,						//  #54
VM_stov,								//  #55
VM_strzone,							//  #56
VM_strunzone,						//  #57
VM_tokenize,						//  #58
VM_argv,								//  #59
VM_isserver,						//  #60
VM_clientcount,					//  #61
VM_clientstate,					//  #62
VM_clcommand,						//  #63
VM_changelevel,					//  #64
VM_localsound,						//  #65
VM_M_getmousepos,					//  #66
VM_gettime,							//  #67
VM_loadfromdata,					//  #68
VM_loadfromfile,					//  #69
VM_modulo,							//  #70
VM_cvar_string,					//  #71
VM_crash,							//  #72
VM_stackdump,						//  #73
VM_search_begin,					//  #74
VM_search_end,						//  #75
VM_search_getsize,				//  #76
VM_search_getfilename,			//  #77
VM_chr,								//  #78
VM_itof,								//  #79
VM_ftoe,								//  #80
VM_itof,								//  #81 isString
VM_altstr_count,					//  #82
VM_altstr_prepare,				//  #83
VM_altstr_get,						//  #84
VM_altstr_set,						//  #85
VM_altstr_ins,						//  #86
VM_findflags,						//  #87
VM_findchainflags,				//  #88
VM_cvar_defstring,				//  #89
// deactivate support for model rendering in the menu until someone has time to do it right [3/2/2008 Andreas]
#if 0
VM_CL_setmodel,					// #90 void(entity e, string m) setmodel (QUAKE)
VM_CL_precache_model,			// #91 void(string s) precache_model (QUAKE)
VM_CL_setorigin,				// #92 void(entity e, vector o) setorigin (QUAKE)
#else
NULL,
NULL,
NULL,
#endif
NULL,									//  #93
NULL,									//  #94
NULL,									//  #95
NULL,									//  #96
NULL,									//  #97
NULL,									//  #98
NULL,									//  #99
NULL,									// #100
NULL,									// #101
NULL,									// #102
NULL,									// #103
NULL,									// #104
NULL,									// #105
NULL,									// #106
NULL,									// #107
NULL,									// #108
NULL,									// #109
NULL,									// #110
NULL,									// #111
NULL,									// #112
NULL,									// #113
NULL,									// #114
NULL,									// #115
NULL,									// #116
NULL,									// #117
NULL,									// #118
NULL,									// #119
NULL,									// #120
NULL,									// #121
NULL,									// #122
NULL,									// #123
NULL,									// #124
NULL,									// #125
NULL,									// #126
NULL,									// #127
NULL,									// #128
NULL,									// #129
NULL,									// #130
NULL,									// #131
NULL,									// #132
NULL,									// #133
NULL,									// #134
NULL,									// #135
NULL,									// #136
NULL,									// #137
NULL,									// #138
NULL,									// #139
NULL,									// #140
NULL,									// #141
NULL,									// #142
NULL,									// #143
NULL,									// #144
NULL,									// #145
NULL,									// #146
NULL,									// #147
NULL,									// #148
NULL,									// #149
NULL,									// #150
NULL,									// #151
NULL,									// #152
NULL,									// #153
NULL,									// #154
NULL,									// #155
NULL,									// #156
NULL,									// #157
NULL,									// #158
NULL,									// #159
NULL,									// #160
NULL,									// #161
NULL,									// #162
NULL,									// #163
NULL,									// #164
NULL,									// #165
NULL,									// #166
NULL,									// #167
NULL,									// #168
NULL,									// #169
NULL,									// #170
NULL,									// #171
NULL,									// #172
NULL,									// #173
NULL,									// #174
NULL,									// #175
NULL,									// #176
NULL,									// #177
NULL,									// #178
NULL,									// #179
NULL,									// #180
NULL,									// #181
NULL,									// #182
NULL,									// #183
NULL,									// #184
NULL,									// #185
NULL,									// #186
NULL,									// #187
NULL,									// #188
NULL,									// #189
NULL,									// #190
NULL,									// #191
NULL,									// #192
NULL,									// #193
NULL,									// #194
NULL,									// #195
NULL,									// #196
NULL,									// #197
NULL,									// #198
NULL,									// #199
NULL,									// #200
NULL,									// #201
NULL,									// #202
NULL,									// #203
NULL,									// #204
NULL,									// #205
NULL,									// #206
NULL,									// #207
NULL,									// #208
NULL,									// #209
NULL,									// #210
NULL,									// #211
NULL,									// #212
NULL,									// #213
NULL,									// #214
NULL,									// #215
NULL,									// #216
NULL,									// #217
NULL,									// #218
NULL,									// #219
NULL,									// #220
VM_strstrofs,						// #221 float(string str, string sub[, float startpos]) strstrofs (FTE_STRINGS)
VM_str2chr,						// #222 float(string str, float ofs) str2chr (FTE_STRINGS)
VM_chr2str,						// #223 string(float c, ...) chr2str (FTE_STRINGS)
VM_strconv,						// #224 string(float ccase, float calpha, float cnum, string s, ...) strconv (FTE_STRINGS)
VM_strpad,						// #225 string(float chars, string s, ...) strpad (FTE_STRINGS)
VM_infoadd,						// #226 string(string info, string key, string value, ...) infoadd (FTE_STRINGS)
VM_infoget,						// #227 string(string info, string key) infoget (FTE_STRINGS)
VM_strncmp,							// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
VM_strncasecmp,					// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
VM_strncasecmp,					// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
NULL,									// #231
NULL,									// #232
NULL,									// #233
NULL,									// #234
NULL,									// #235
NULL,									// #236
NULL,									// #237
NULL,									// #238
NULL,									// #239
NULL,									// #240
NULL,									// #241
NULL,									// #242
NULL,									// #243
NULL,									// #244
NULL,									// #245
NULL,									// #246
NULL,									// #247
NULL,									// #248
NULL,									// #249
NULL,									// #250
NULL,									// #251
NULL,									// #252
NULL,									// #253
NULL,									// #254
NULL,									// #255
NULL,									// #256
NULL,									// #257
NULL,									// #258
NULL,									// #259
NULL,									// #260
NULL,									// #261
NULL,									// #262
NULL,									// #263
NULL,									// #264
NULL,									// #265
NULL,									// #266
NULL,									// #267
NULL,									// #268
NULL,									// #269
NULL,									// #270
NULL,									// #271
NULL,									// #272
NULL,									// #273
NULL,									// #274
NULL,									// #275
NULL,									// #276
NULL,									// #277
NULL,									// #278
NULL,									// #279
NULL,									// #280
NULL,									// #281
NULL,									// #282
NULL,									// #283
NULL,									// #284
NULL,									// #285
NULL,									// #286
NULL,									// #287
NULL,									// #288
NULL,									// #289
NULL,									// #290
NULL,									// #291
NULL,									// #292
NULL,									// #293
NULL,									// #294
NULL,									// #295
NULL,									// #296
NULL,									// #297
NULL,									// #298
NULL,									// #299
// deactivate support for model rendering in the menu until someone has time to do it right [3/2/2008 Andreas]
#if 0
// CSQC range #300-#399
VM_CL_R_ClearScene,				// #300 void() clearscene (DP_QC_RENDER_SCENE)
VM_CL_R_AddEntities,			// #301 void(float mask) addentities (DP_QC_RENDER_SCENE)
VM_CL_R_AddEntity,				// #302 void(entity ent) addentity (DP_QC_RENDER_SCENE)
VM_CL_R_SetView,				// #303 float(float property, ...) setproperty (DP_QC_RENDER_SCENE)
VM_CL_R_RenderScene,			// #304 void() renderscene (DP_QC_RENDER_SCENE)
VM_CL_R_AddDynamicLight,		// #305 void(vector org, float radius, vector lightcolours) adddynamiclight (DP_QC_RENDER_SCENE)
VM_CL_R_PolygonBegin,			// #306 void(string texturename, float flag[, float is2d, float lines]) R_BeginPolygon (DP_QC_RENDER_SCENE)
VM_CL_R_PolygonVertex,			// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (DP_QC_RENDER_SCENE)
VM_CL_R_PolygonEnd,				// #308 void() R_EndPolygon
NULL/*VM_CL_R_LoadWorldModel*/,				// #309 void(string modelname) R_LoadWorldModel
// TODO: rearrange and merge all builtin lists and share as many extensions as possible between all VM instances [1/27/2008 Andreas]
VM_CL_setattachment,				// #310 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS) (DP_QC_RENDER_SCENE)
VM_CL_gettagindex,				// #311 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO) (DP_QC_RENDER_SCENE)
VM_CL_gettaginfo,					// #312 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO) (DP_QC_RENDER_SCENE)
#else
// CSQC range #300-#399
NULL,		
NULL,		
NULL,		
NULL,		
NULL,		
NULL,		
NULL,		
NULL,	
NULL,	
NULL,
NULL,	
NULL,	
NULL,	
#endif
NULL,									// #313
NULL,									// #314
NULL,									// #315
NULL,									// #316
NULL,									// #317
NULL,									// #318
NULL,									// #319
NULL,									// #320
NULL,									// #321
NULL,									// #322
NULL,									// #323
NULL,									// #324
NULL,									// #325
NULL,									// #326
NULL,									// #327
NULL,									// #328
NULL,									// #329
NULL,									// #330
NULL,									// #331
NULL,									// #332
NULL,									// #333
NULL,									// #334
NULL,									// #335
NULL,									// #336
NULL,									// #337
NULL,									// #338
NULL,									// #339
NULL,									// #340
NULL,									// #341
NULL,									// #342
NULL,									// #343
NULL,									// #344
NULL,									// #345
NULL,									// #346
NULL,									// #347
NULL,									// #348
VM_M_isdemo,								// #349
NULL,									// #350
NULL,									// #351
NULL,									// #352
NULL,									// #353
NULL,									// #354
NULL,									// #355
NULL,									// #356
NULL,									// #357
NULL,									// #358
NULL,									// #359
NULL,									// #360
NULL,									// #361
NULL,									// #362
NULL,									// #363
NULL,									// #364
NULL,									// #365
NULL,									// #366
NULL,									// #367
NULL,									// #368
NULL,									// #369
NULL,									// #370
NULL,									// #371
NULL,									// #372
NULL,									// #373
NULL,									// #374
NULL,									// #375
NULL,									// #376
NULL,									// #377
NULL,									// #378
NULL,									// #379
NULL,									// #380
NULL,									// #381
NULL,									// #382
NULL,									// #383
NULL,									// #384
NULL,									// #385
NULL,									// #386
NULL,									// #387
NULL,									// #388
NULL,									// #389
NULL,									// #390
NULL,									// #391
NULL,									// #392
NULL,									// #393
NULL,									// #394
NULL,									// #395
NULL,									// #396
NULL,									// #397
NULL,									// #398
NULL,									// #399
NULL,									// #400
VM_M_WriteByte,					// #401
VM_M_WriteChar,					// #402
VM_M_WriteShort,					// #403
VM_M_WriteLong,					// #404
VM_M_WriteAngle,					// #405
VM_M_WriteCoord,					// #406
VM_M_WriteString,					// #407
VM_M_WriteEntity,					// #408
NULL,									// #409
NULL,									// #410
NULL,									// #411
NULL,									// #412
NULL,									// #413
NULL,									// #414
NULL,									// #415
NULL,									// #416
NULL,									// #417
NULL,									// #418
NULL,									// #419
NULL,									// #420
NULL,									// #421
NULL,									// #422
NULL,									// #423
NULL,									// #424
NULL,									// #425
NULL,									// #426
NULL,									// #427
NULL,									// #428
NULL,									// #429
NULL,									// #430
NULL,									// #431
NULL,									// #432
NULL,									// #433
NULL,									// #434
NULL,									// #435
NULL,									// #436
NULL,									// #437
NULL,									// #438
NULL,									// #439
VM_buf_create,					// #440 float() buf_create (DP_QC_STRINGBUFFERS)
VM_buf_del,						// #441 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
VM_buf_getsize,					// #442 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
VM_buf_copy,					// #443 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
VM_buf_sort,					// #444 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
VM_buf_implode,					// #445 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
VM_bufstr_get,					// #446 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
VM_bufstr_set,					// #447 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
VM_bufstr_add,					// #448 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
VM_bufstr_free,					// #449 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
NULL,									// #450
VM_iscachedpic,					// #451 draw functions...
VM_precache_pic,					// #452
VM_freepic,							// #453
VM_drawcharacter,					// #454
VM_drawstring,						// #455
VM_drawpic,							// #456
VM_drawfill,						// #457
VM_drawsetcliparea,				// #458
VM_drawresetcliparea,			// #459
VM_getimagesize,					// #460
VM_cin_open,						// #461
VM_cin_close,						// #462
VM_cin_setstate,					// #463
VM_cin_getstate,					// #464
VM_cin_restart, 					// #465
VM_drawline,						// #466
VM_drawcolorcodedstring,		// #467
VM_stringwidth,					// #468
VM_drawsubpic,						// #469
VM_drawrotpic,						// #470
VM_asin,								// #471 float(float s) VM_asin (DP_QC_ASINACOSATANATAN2TAN)
VM_acos,								// #472 float(float c) VM_acos (DP_QC_ASINACOSATANATAN2TAN)
VM_atan,								// #473 float(float t) VM_atan (DP_QC_ASINACOSATANATAN2TAN)
VM_atan2,							// #474 float(float c, float s) VM_atan2 (DP_QC_ASINACOSATANATAN2TAN)
VM_tan,								// #475 float(float a) VM_tan (DP_QC_ASINACOSATANATAN2TAN)
VM_strlennocol,					// #476 float(string s) : DRESK - String Length (not counting color codes) (DP_QC_STRINGCOLORFUNCTIONS)
VM_strdecolorize,					// #477 string(string s) : DRESK - Decolorized String (DP_QC_STRINGCOLORFUNCTIONS)
VM_strftime,						// #478 string(float uselocaltime, string format, ...) (DP_QC_STRFTIME)
VM_tokenizebyseparator,			// #479 float(string s) tokenizebyseparator (DP_QC_TOKENIZEBYSEPARATOR)
VM_strtolower,						// #480 string(string s) VM_strtolower : DRESK - Return string as lowercase
VM_strtoupper,						// #481 string(string s) VM_strtoupper : DRESK - Return string as uppercase
NULL,									// #482
NULL,									// #483
VM_strreplace,						// #484 string(string search, string replace, string subject) strreplace (DP_QC_STRREPLACE)
VM_strireplace,					// #485 string(string search, string replace, string subject) strireplace (DP_QC_STRREPLACE)
NULL,									// #486
VM_gecko_create,					// #487 float gecko_create( string name )
VM_gecko_destroy,					// #488 void gecko_destroy( string name )
VM_gecko_navigate,				// #489 void gecko_navigate( string name, string URI )
VM_gecko_keyevent,				// #490 float gecko_keyevent( string name, float key, float eventtype )
VM_gecko_movemouse,				// #491 void gecko_mousemove( string name, float x, float y )
VM_gecko_resize,					// #492 void gecko_resize( string name, float w, float h )
VM_gecko_get_texture_extent,	// #493 vector gecko_get_texture_extent( string name )
VM_crc16,						// #494 float(float caseinsensitive, string s, ...) crc16 = #494 (DP_QC_CRC16)
VM_cvar_type,					// #495 float(string name) cvar_type = #495; (DP_QC_CVAR_TYPE)
NULL,									// #496
NULL,									// #497
NULL,									// #498
NULL,									// #499
NULL,									// #500
NULL,									// #501
NULL,									// #502
VM_whichpack,					// #503 string(string) whichpack = #503;
NULL,									// #504
NULL,									// #505
NULL,									// #506
NULL,									// #507
NULL,									// #508
NULL,									// #509
VM_uri_escape,					// #510 string(string in) uri_escape = #510;
VM_uri_unescape,				// #511 string(string in) uri_unescape = #511;
VM_etof,					// #512 float(entity ent) num_for_edict = #512 (DP_QC_NUM_FOR_EDICT)
VM_uri_get,						// #513 float(string uril, float id) uri_get = #513; (DP_QC_URI_GET)
VM_tokenize_console,					// #514 float(string str) tokenize_console = #514; (DP_QC_TOKENIZE_CONSOLE)
VM_argv_start_index,					// #515 float(float idx) argv_start_index = #515; (DP_QC_TOKENIZE_CONSOLE)
VM_argv_end_index,						// #516 float(float idx) argv_end_index = #516; (DP_QC_TOKENIZE_CONSOLE)
VM_buf_cvarlist,						// #517 void(float buf, string prefix, string antiprefix) buf_cvarlist = #517; (DP_QC_STRINGBUFFERS_CVARLIST)
VM_cvar_description,					// #518 float(string name) cvar_description = #518; (DP_QC_CVAR_DESCRIPTION)
NULL,									// #519
NULL,									// #520
NULL,									// #521
NULL,									// #522
NULL,									// #523
NULL,									// #524
NULL,									// #525
NULL,									// #526
NULL,									// #527
NULL,									// #528
NULL,									// #529
NULL,									// #530
NULL,									// #531
NULL,									// #532
NULL,									// #533
NULL,									// #534
NULL,									// #535
NULL,									// #536
NULL,									// #537
NULL,									// #538
NULL,									// #539
NULL,									// #540
NULL,									// #541
NULL,									// #542
NULL,									// #543
NULL,									// #544
NULL,									// #545
NULL,									// #546
NULL,									// #547
NULL,									// #548
NULL,									// #549
NULL,									// #550
NULL,									// #551
NULL,									// #552
NULL,									// #553
NULL,									// #554
NULL,									// #555
NULL,									// #556
NULL,									// #557
NULL,									// #558
NULL,									// #559
NULL,									// #560
NULL,									// #561
NULL,									// #562
NULL,									// #563
NULL,									// #564
NULL,									// #565
NULL,									// #566
NULL,									// #567
NULL,									// #568
NULL,									// #569
NULL,									// #570
NULL,									// #571
NULL,									// #572
NULL,									// #573
NULL,									// #574
NULL,									// #575
NULL,									// #576
NULL,									// #577
NULL,									// #578
NULL,									// #579
NULL,									// #580
NULL,									// #581
NULL,									// #582
NULL,									// #583
NULL,									// #584
NULL,									// #585
NULL,									// #586
NULL,									// #587
NULL,									// #588
NULL,									// #589
NULL,									// #590
NULL,									// #591
NULL,									// #592
NULL,									// #593
NULL,									// #594
NULL,									// #595
NULL,									// #596
NULL,									// #597
NULL,									// #598
NULL,									// #599
NULL,									// #600
VM_M_setkeydest,					// #601 void setkeydest(float dest)
VM_M_getkeydest,					// #602 float getkeydest(void)
VM_M_setmousetarget,				// #603 void setmousetarget(float trg)
VM_M_getmousetarget,				// #604 float getmousetarget(void)
VM_M_callfunction,				// #605 void callfunction(...)
VM_writetofile,					// #606 void writetofile(float fhandle, entity ent)
VM_M_isfunction,					// #607 float isfunction(string function_name)
VM_M_getresolution,				// #608 vector getresolution(float number, [float forfullscreen])
VM_keynumtostring,				// #609 string keynumtostring(float keynum)
VM_findkeysforcommand,		// #610 string findkeysforcommand(string command)
VM_M_getserverliststat,			// #611 float gethostcachevalue(float type)
VM_M_getserverliststring,		// #612 string gethostcachestring(float type, float hostnr)
VM_parseentitydata,				// #613 void parseentitydata(entity ent, string data)
VM_stringtokeynum,				// #614 float stringtokeynum(string key)
VM_M_resetserverlistmasks,		// #615 void resethostcachemasks(void)
VM_M_setserverlistmaskstring,	// #616 void sethostcachemaskstring(float mask, float fld, string str, float op)
VM_M_setserverlistmasknumber,	// #617 void sethostcachemasknumber(float mask, float fld, float num, float op)
VM_M_resortserverlist,			// #618 void resorthostcache(void)
VM_M_setserverlistsort,			// #619 void sethostcachesort(float fld, float descending)
VM_M_refreshserverlist,			// #620 void refreshhostcache(void)
VM_M_getserverlistnumber,		// #621 float gethostcachenumber(float fld, float hostnr)
VM_M_getserverlistindexforkey,// #622 float gethostcacheindexforkey(string key)
VM_M_addwantedserverlistkey,	// #623 void addwantedhostcachekey(string key)
VM_getextresponse,				// #624 string getextresponse(void)
VM_netaddress_resolve,          // #625 string netaddress_resolve(string, float)
VM_M_getgamedirinfo,            // #626 string getgamedirinfo(float n, float prop)
NULL
};

const int vm_m_numbuiltins = sizeof(vm_m_builtins) / sizeof(prvm_builtin_t);

void VM_M_Cmd_Init(void)
{
	r_refdef_scene_t *scene;

	VM_Cmd_Init();
	VM_Polygons_Reset();

	scene = R_GetScenePointer( RST_MENU );

	memset (scene, 0, sizeof (*scene));

	scene->maxtempentities = 128;
	scene->tempentities = (entity_render_t*) Mem_Alloc(prog->progs_mempool, sizeof(entity_render_t) * scene->maxtempentities);

	scene->maxentities = MAX_EDICTS + 256 + 512;
	scene->entities = (entity_render_t **)Mem_Alloc(prog->progs_mempool, sizeof(entity_render_t *) * scene->maxentities);

	scene->ambient = 32.0f;
}

void VM_M_Cmd_Reset(void)
{
	// note: the menu's render entities are automatically freed when the prog's pool is freed

	//VM_Cmd_Init();
	VM_Cmd_Reset();
	VM_Polygons_Reset();
}
