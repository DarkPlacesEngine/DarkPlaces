#include "quakedef.h"
#include "progsvm.h"
#include "clprogdefs.h"
#include "csprogs.h"

//============================================================================
// Client prog handling
//[515]: omg !!! optimize it ! a lot of hacks here and there also :P

#define CSQC_RETURNVAL	prog->globals.generic[OFS_RETURN]
#define CSQC_BEGIN		csqc_tmpprog=prog;prog=0;PRVM_SetProg(PRVM_CLIENTPROG);
#define CSQC_END		prog=csqc_tmpprog;
static prvm_prog_t *csqc_tmpprog;

//[515]: these are required funcs
#define CL_F_INIT				"CSQC_Init"
#define CL_F_INPUTEVENT			"CSQC_InputEvent"
#define CL_F_UPDATEVIEW			"CSQC_UpdateView"
#define CL_F_CONSOLECOMMAND		"CSQC_ConsoleCommand"
#define CL_F_SHUTDOWN			"CSQC_Shutdown"

//[515]: these are optional
#define CL_F_PARSE_TEMPENTITY	"CSQC_Parse_TempEntity"	//[515]: very helpfull when you want to create your own particles/decals/etc for effects that allready exist
#define CL_F_PARSE_STUFFCMD		"CSQC_Parse_StuffCmd"
#define CL_F_PARSE_PRINT		"CSQC_Parse_Print"
#define CL_F_PARSE_CENTERPRINT	"CSQC_Parse_CenterPrint"
#define CL_F_ENT_UPDATE			"CSQC_Ent_Update"
#define CL_F_ENT_REMOVE			"CSQC_Ent_Remove"
#define CL_F_EVENT				"CSQC_Event"	//[515]: engine call this for its own needs
												//so csqc can do some things according to what engine it's running on
												//example: to say about edicts increase, whatever...

#define CSQC_PRINTBUFFERLEN		8192	//[515]: enough ?

static char *cl_required_func[] =
{
	CL_F_INIT,
	CL_F_INPUTEVENT,
	CL_F_UPDATEVIEW,
	CL_F_CONSOLECOMMAND,
	CL_F_SHUTDOWN
};

static int cl_numrequiredfunc = sizeof(cl_required_func) / sizeof(char*);

static char				*csqc_printtextbuf = NULL;
static unsigned short	*csqc_sv2csqcents;	//[515]: server entities numbers on client side. FIXME : make pointers instead of numbers ?

static mfunction_t	*CSQC_Parse_TempEntity;
static mfunction_t	*CSQC_Parse_StuffCmd;
static mfunction_t	*CSQC_Parse_Print;
static mfunction_t	*CSQC_Parse_CenterPrint;
static mfunction_t	*CSQC_Ent_Update;
static mfunction_t	*CSQC_Ent_Remove;
static mfunction_t	*CSQC_Event;

static int csqc_fieldoff_alpha;
static int csqc_fieldoff_colormod;
static int csqc_fieldoff_effects;
int csqc_fieldoff_scale;
int csqc_fieldoff_renderflags;
int csqc_fieldoff_tag_entity;
int csqc_fieldoff_tag_index;
int csqc_fieldoff_dphitcontentsmask;

qboolean csqc_loaded = false;

vec3_t csqc_origin, csqc_angles;
static double csqc_frametime = 0;
int csqc_buttons;

static mempool_t *csqc_mempool;

static void CL_VM_FindEdictFieldOffsets (void)
{
	csqc_fieldoff_alpha			= PRVM_ED_FindFieldOffset("alpha");
	csqc_fieldoff_scale			= PRVM_ED_FindFieldOffset("scale");
	csqc_fieldoff_colormod		= PRVM_ED_FindFieldOffset("colormod");
	csqc_fieldoff_renderflags	= PRVM_ED_FindFieldOffset("renderflags");
	csqc_fieldoff_effects		= PRVM_ED_FindFieldOffset("effects");
	csqc_fieldoff_tag_entity	= PRVM_ED_FindFieldOffset("tag_entity");
	csqc_fieldoff_tag_index		= PRVM_ED_FindFieldOffset("tag_index");

	CSQC_Parse_TempEntity		= PRVM_ED_FindFunction (CL_F_PARSE_TEMPENTITY);
	CSQC_Parse_StuffCmd			= PRVM_ED_FindFunction (CL_F_PARSE_STUFFCMD);
	CSQC_Parse_Print			= PRVM_ED_FindFunction (CL_F_PARSE_PRINT);
	CSQC_Parse_CenterPrint		= PRVM_ED_FindFunction (CL_F_PARSE_CENTERPRINT);
	CSQC_Ent_Update				= PRVM_ED_FindFunction (CL_F_ENT_UPDATE);
	CSQC_Ent_Remove				= PRVM_ED_FindFunction (CL_F_ENT_REMOVE);
	CSQC_Event					= PRVM_ED_FindFunction (CL_F_EVENT);

	if(CSQC_Parse_Print)
	{
		csqc_printtextbuf = (char *)Mem_Alloc(csqc_mempool, CSQC_PRINTBUFFERLEN);
		csqc_printtextbuf[0] = 0;
	}
}

void CL_VM_Error (const char *format, ...) DP_FUNC_PRINTF(1);
void CL_VM_Error (const char *format, ...)	//[515]: hope it will be never executed =)
{
	char errorstring[4096];
	va_list argptr;

	va_start (argptr, format);
	dpvsnprintf (errorstring, sizeof(errorstring), format, argptr);
	va_end (argptr);
//	Con_Printf( "CL_VM_Error: %s\n", errorstring );

	PRVM_Crash();
	csqc_loaded = false;
	Mem_FreePool(&csqc_mempool);

	Cvar_SetValueQuick(&csqc_progcrc, -1);
	Cvar_SetValueQuick(&csqc_progsize, -1);

//	Host_AbortCurrentFrame();	//[515]: hmmm... if server says it needs csqc then client MUST disconnect
	Host_Error(va("CL_VM_Error: %s", errorstring));
}

//[515]: set globals before calling R_UpdateView, WEIRD CRAP
static void CSQC_SetGlobals (void)
{
	//extern cvar_t sv_accelerate, sv_friction, sv_gravity, sv_stopspeed, sv_maxspeed;

	CSQC_BEGIN
		*prog->time = cl.time;
		prog->globals.client->frametime = cl.time - csqc_frametime;
		csqc_frametime = cl.time;
		prog->globals.client->servercommandframe = cl.servermovesequence;
		prog->globals.client->clientcommandframe = cl.movemessages;
		VectorCopy(cl.viewangles, prog->globals.client->input_angles);
		VectorCopy(cl.viewangles, csqc_angles);
		prog->globals.client->input_buttons = csqc_buttons;
		VectorSet(prog->globals.client->input_movevalues, cl.cmd.forwardmove, cl.cmd.sidemove, cl.cmd.upmove);
		//VectorCopy(cl.movement_origin, csqc_origin);
		Matrix4x4_OriginFromMatrix(&cl.entities[cl.viewentity].render.matrix, csqc_origin);
		VectorCopy(csqc_origin, prog->globals.client->pmove_org);
		prog->globals.client->maxclients = cl.maxclients;
		//VectorCopy(cl.movement_velocity, prog->globals.client->pmove_vel);
		VectorCopy(cl.velocity, prog->globals.client->pmove_vel);
	CSQC_END
}

void CSQC_Predraw (prvm_edict_t *ed)
{
	int b;
	if(!ed->fields.client->predraw)
		return;
	b = prog->globals.client->self;
	prog->globals.client->self = PRVM_EDICT_TO_PROG(ed);
	PRVM_ExecuteProgram(ed->fields.client->predraw, "CSQC_Predraw: NULL function\n");
	prog->globals.client->self = b;
}

void CSQC_Think (prvm_edict_t *ed)
{
	int b;
	if(ed->fields.client->think)
	if(ed->fields.client->nextthink && ed->fields.client->nextthink <= *prog->time)
	{
		ed->fields.client->nextthink = 0;
		b = prog->globals.client->self;
		prog->globals.client->self = PRVM_EDICT_TO_PROG(ed);
		PRVM_ExecuteProgram(ed->fields.client->think, "CSQC_Think: NULL function\n");
		prog->globals.client->self = b;
	}
}

extern cvar_t cl_noplayershadow;
qboolean CSQC_AddRenderEdict(prvm_edict_t *ed)
{
	int i;
	prvm_eval_t *val;
	entity_t *e;
	matrix4x4_t tagmatrix, matrix2;

	e = CL_NewTempEntity();
	if (!e)
		return false;

	i = (int)ed->fields.client->modelindex;
	if(i >= MAX_MODELS || i <= -MAX_MODELS)	//[515]: make work as error ?
	{
		Con_Print("CSQC_AddRenderEdict: modelindex >= MAX_MODELS\n");
		ed->fields.client->modelindex = i = 0;
	}

	// model setup and some modelflags
	if (i < MAX_MODELS)
		e->render.model = cl.model_precache[e->state_current.modelindex];
	else
		e->render.model = cl.csqc_model_precache[65536-e->state_current.modelindex];

	if (!e->render.model)
		return false;

	e->render.colormap = (int)ed->fields.client->colormap;
	e->render.frame = (int)ed->fields.client->frame;
	e->render.skinnum = (int)ed->fields.client->skin;
	e->render.effects |= e->render.model->flags2 & (EF_FULLBRIGHT | EF_ADDITIVE);

	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_alpha)) && val->_float)		e->render.alpha = val->_float;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_scale)) && val->_float)		e->render.scale = val->_float;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_colormod)) && VectorLength2(val->vector))	VectorCopy(val->vector, e->render.colormod);
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_effects)) && val->_float)	e->render.effects = (int)val->_float;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_tag_entity)) && val->edict)
	{
		int tagentity;
		int tagindex;
		tagentity = val->edict;
		if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_tag_index)) && val->_float)
			tagindex = (int)val->_float;
		// FIXME: calculate tag matrix
		Matrix4x4_CreateIdentity(&tagmatrix);
	}
	else
		Matrix4x4_CreateIdentity(&tagmatrix);

	if(i & RF_USEAXIS)	//FIXME!!!
	{
		vec3_t left;
		VectorNegate(prog->globals.client->v_right, left);
		Matrix4x4_FromVectors(&matrix2, prog->globals.client->v_forward, left, prog->globals.client->v_up, ed->fields.client->origin);
	}
	else
	{
		vec3_t angles;
		VectorCopy(ed->fields.client->angles, angles);
		// if model is alias, reverse pitch direction
		if (e->render.model->type == mod_alias)
			angles[0] = -angles[0];

		// set up the render matrix
		// FIXME: e->render.scale should go away
		Matrix4x4_CreateFromQuakeEntity(&matrix2, ed->fields.client->origin[0], ed->fields.client->origin[1], ed->fields.client->origin[2], angles[0], angles[1], angles[2], e->render.scale);
	}

	// FIXME: csqc has frame1/frame2/frame1time/frame2time/lerpfrac but this implementation's cl_entvars_t lacks those fields
	e->render.frame1 = e->render.frame = ed->fields.client->frame;
	e->render.frame1time = e->render.frame2time = 0;
	e->render.framelerp = 0;

	// concat the matrices to make the entity relative to its tag
	Matrix4x4_Concat(&e->render.matrix, &tagmatrix, &matrix2);
	// make the other useful stuff
	CL_UpdateRenderEntity(&e->render);

	i = 0;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_renderflags)) && val->_float)
	{
		i = (int)val->_float;
		if(i & RF_VIEWMODEL)	e->render.flags |= RENDER_VIEWMODEL;
		if(i & RF_EXTERNALMODEL)e->render.flags |= RENDER_EXTERIORMODEL;
		if(i & RF_DEPTHHACK)	e->render.effects |= EF_NODEPTHTEST;
		if(i & RF_ADDITIVE)		e->render.effects |= EF_ADDITIVE;
	}

	// transparent stuff can't be lit during the opaque stage
	if (e->render.effects & (EF_ADDITIVE | EF_NODEPTHTEST) || e->render.alpha < 1)
		e->render.flags |= RENDER_TRANSPARENT;
	// double sided rendering mode causes backfaces to be visible
	// (mostly useful on transparent stuff)
	if (e->render.effects & EF_DOUBLESIDED)
		e->render.flags |= RENDER_NOCULLFACE;
	// either fullbright or lit
	if (!(e->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
		e->render.flags |= RENDER_LIGHT;
	// hide player shadow during intermission or nehahra movie
	if (!(e->render.effects & EF_NOSHADOW)
	 && !(e->render.flags & (RENDER_VIEWMODEL | RENDER_TRANSPARENT))
	 && (!(e->render.flags & RENDER_EXTERIORMODEL) || (!cl.intermission && cls.protocol != PROTOCOL_NEHAHRAMOVIE && !cl_noplayershadow.integer)))
		e->render.flags |= RENDER_SHADOW;

	return true;
}

void CSQC_ClearCSQCEntities (void)
{
	memset(cl.csqcentities_active, 0, sizeof(cl.csqcentities_active));
	cl.num_csqcentities = 0;
}

void CL_ExpandCSQCEntities (int num);

qboolean CL_VM_InputEvent (qboolean pressed, int key)
{
	qboolean r;
	if(!csqc_loaded)
		return false;
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_FLOAT(OFS_PARM0) = pressed;
		PRVM_G_FLOAT(OFS_PARM1) = key;
		PRVM_ExecuteProgram (prog->globals.client->CSQC_InputEvent, CL_F_INPUTEVENT);
		r = CSQC_RETURNVAL;
	CSQC_END
	return r;
}

qboolean CL_VM_UpdateView (void)
{
//	vec3_t oldangles;
	if(!csqc_loaded)
		return false;
	CSQC_BEGIN
		//VectorCopy(cl.viewangles, oldangles);
		*prog->time = cl.time;
		CSQC_SetGlobals();
		cl.num_csqcentities = 0;
		PRVM_ExecuteProgram (prog->globals.client->CSQC_UpdateView, CL_F_UPDATEVIEW);
		//VectorCopy(oldangles, cl.viewangles);
	CSQC_END
	csqc_frame = false;
	return true;
}

qboolean CL_VM_ConsoleCommand (const char *cmd)
{
	qboolean r;
	if(!csqc_loaded)
		return false;
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetEngineString(cmd);
		PRVM_ExecuteProgram (prog->globals.client->CSQC_ConsoleCommand, CL_F_CONSOLECOMMAND);
		r = CSQC_RETURNVAL;
	CSQC_END
	return r;
}

qboolean CL_VM_Parse_TempEntity (void)
{
	int			t;
	qboolean	r;
	if(!csqc_loaded || !CSQC_Parse_TempEntity)
		return false;
	t = msg_readcount;
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_ExecuteProgram ((func_t)(CSQC_Parse_TempEntity - prog->functions), CL_F_PARSE_TEMPENTITY);
		r = CSQC_RETURNVAL;
	CSQC_END
	if(!r)
	{
		msg_readcount = t;
		msg_badread = false;
	}
	return r;
}

void CL_VM_Parse_StuffCmd (const char *msg)
{
	if(msg[0] == 'c')
	if(msg[1] == 's')
	if(msg[2] == 'q')
	if(msg[3] == 'c')
	{
		// if this is setting a csqc variable, deprotect csqc_progcrc
		// temporarily so that it can be set by the cvar command,
		// and then reprotect it afterwards
		int crcflags = csqc_progcrc.flags;
		int sizeflags = csqc_progcrc.flags;
		csqc_progcrc.flags &= ~CVAR_READONLY;
		csqc_progsize.flags &= ~CVAR_READONLY;
		Cmd_ExecuteString (msg, src_command);
		csqc_progcrc.flags = crcflags;
		csqc_progsize.flags = sizeflags;
		return;
	}
	if(!csqc_loaded || !CSQC_Parse_StuffCmd)
	{
		Cbuf_AddText(msg);
		return;
	}
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetEngineString(msg);
		PRVM_ExecuteProgram ((func_t)(CSQC_Parse_StuffCmd - prog->functions), CL_F_PARSE_STUFFCMD);
	CSQC_END
}

static void CL_VM_Parse_Print (const char *msg)
{
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetEngineString(msg);
		PRVM_ExecuteProgram ((func_t)(CSQC_Parse_Print - prog->functions), CL_F_PARSE_PRINT);
	CSQC_END
}

void CSQC_AddPrintText (const char *msg)
{
	size_t i;
	if(!csqc_loaded || !CSQC_Parse_Print)
	{
		Con_Print(msg);
		return;
	}
	// FIXME: is this bugged?
	i = strlen(msg)-1;
	if(msg[i] != '\n' && msg[i] != '\r')
	{
		if(strlen(csqc_printtextbuf)+i >= CSQC_PRINTBUFFERLEN)
		{
			CL_VM_Parse_Print(csqc_printtextbuf);
			csqc_printtextbuf[0] = 0;
		}
		else
			strlcat(csqc_printtextbuf, msg, CSQC_PRINTBUFFERLEN);
		return;
	}
	strlcat(csqc_printtextbuf, msg, CSQC_PRINTBUFFERLEN);
	CL_VM_Parse_Print(csqc_printtextbuf);
	csqc_printtextbuf[0] = 0;
}

void CL_VM_Parse_CenterPrint (const char *msg)
{
	if(!csqc_loaded || !CSQC_Parse_CenterPrint)
	{
		SCR_CenterPrint((char*)msg);
		return;
	}
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetEngineString(msg);
		PRVM_ExecuteProgram ((func_t)(CSQC_Parse_CenterPrint - prog->functions), CL_F_PARSE_CENTERPRINT);
	CSQC_END
}

float CL_VM_Event (float event)		//[515]: needed ? I'd say "YES", but don't know for what :D
{
	float r;
	if(!csqc_loaded || !CSQC_Event)
		return 0;
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_G_FLOAT(OFS_PARM0) = event;
		PRVM_ExecuteProgram ((func_t)(CSQC_Event - prog->functions), CL_F_EVENT);
		r = CSQC_RETURNVAL;
	CSQC_END
	return r;
}

void CSQC_ReadEntities (void)
{
	unsigned short entnum, oldself, realentnum;
	CSQC_BEGIN
		*prog->time = cl.time;
		oldself = prog->globals.client->self;
		while(1)
		{
			entnum = MSG_ReadShort();
			if(!entnum)
				return;
			realentnum = entnum & 0x7FFF;
			prog->globals.client->self = csqc_sv2csqcents[realentnum];
			if(entnum & 0x8000)
			{
				if(prog->globals.client->self)
				{
					PRVM_ExecuteProgram((func_t)(CSQC_Ent_Remove - prog->functions), CL_F_ENT_REMOVE);
					csqc_sv2csqcents[realentnum] = 0;
				}
				else
					Con_Printf("Smth bad happens in csqc...\n");	//[515]: never happens ?
			}
			else
			{
				if(!prog->globals.client->self)
				{
					prvm_edict_t	*ed;
					ed = PRVM_ED_Alloc();
					ed->fields.client->entnum = realentnum;
					prog->globals.client->self = csqc_sv2csqcents[realentnum] = PRVM_EDICT_TO_PROG(ed);
					PRVM_G_FLOAT(OFS_PARM0) = 1;
				}
				else
					PRVM_G_FLOAT(OFS_PARM0) = 0;
				PRVM_ExecuteProgram((func_t)(CSQC_Ent_Update - prog->functions), CL_F_ENT_UPDATE);
			}
		}
		prog->globals.client->self = oldself;
	CSQC_END
}

void Cmd_ClearCsqcFuncs (void);

void CL_VM_Init (void)
{
	unsigned char *csprogsdata;
	fs_offset_t csprogsdatasize;
	int csprogsdatacrc, requiredcrc;
	int requiredsize;
	entity_t *ent;

	// reset csqc_progcrc after reading it, so that changing servers doesn't
	// expect csqc on the next server
	requiredcrc = csqc_progcrc.integer;
	requiredsize = csqc_progsize.integer;
	Cvar_SetValueQuick(&csqc_progcrc, -1);
	Cvar_SetValueQuick(&csqc_progsize, -1);

	csqc_loaded = false;
	memset(cl.csqc_model_precache, 0, sizeof(cl.csqc_model_precache));
	memset(&cl.csqc_vidvars, true, sizeof(csqc_vidvars_t));

	// if the server is not requesting a csprogs, then we're done here
	if (requiredcrc < 0)
		return;

	// see if the requested csprogs.dat file matches the requested crc
	csprogsdatacrc = -1;
	csprogsdata = FS_LoadFile(va("dlcache/%s.%i.%i", csqc_progname.string, requiredsize, requiredcrc), tempmempool, true, &csprogsdatasize);
	if (!csprogsdata)
		csprogsdata = FS_LoadFile(csqc_progname.string, tempmempool, true, &csprogsdatasize);
	if (csprogsdata)
	{
		csprogsdatacrc = CRC_Block(csprogsdata, csprogsdatasize);
		Mem_Free(csprogsdata);
		if (csprogsdatacrc != requiredcrc || csprogsdatasize != requiredsize)
		{
			if (cls.demoplayback)
			{
				Con_Printf("^1Warning: Your %s is not the same version as the demo was recorded with (CRC/size are %i/%i but should be %i/%i)\n", csqc_progname.string, csprogsdatacrc, (int)csprogsdatasize, requiredcrc, requiredsize);
				return;
			}
			else
			{
				Con_Printf("^1Your %s is not the same version as the server (CRC is %i/%i but should be %i/%i)\n", csqc_progname.string, csprogsdatacrc, (int)csprogsdatasize, requiredcrc, requiredsize);
				CL_Disconnect();
				return;
			}
		}
	}
	else
	{
		if (requiredcrc >= 0)
		{
			if (cls.demoplayback)
				Con_Printf("CL_VM_Init: demo requires CSQC, but \"%s\" wasn't found\n", csqc_progname.string);
			else
				Con_Printf("CL_VM_Init: server requires CSQC, but \"%s\" wasn't found\n", csqc_progname.string);
			CL_Disconnect();
		}
		return;
	}

	PRVM_Begin;
	PRVM_InitProg(PRVM_CLIENTPROG);

	csqc_mempool = Mem_AllocPool("CSQC", 0, NULL);

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool(csqc_progname.string, 0, NULL);
	prog->headercrc = CL_PROGHEADER_CRC;
	prog->edictprivate_size = 0; // no private struct used
	prog->name = CL_NAME;
	prog->num_edicts = 1;
	prog->limit_edicts = CL_MAX_EDICTS;
	prog->extensionstring = vm_cl_extensions;
	prog->builtins = vm_cl_builtins;
	prog->numbuiltins = vm_cl_numbuiltins;
	prog->init_cmd = VM_CL_Cmd_Init;
	prog->reset_cmd = VM_CL_Cmd_Reset;
	prog->error_cmd = CL_VM_Error;

	PRVM_LoadProgs(csqc_progname.string, cl_numrequiredfunc, cl_required_func, 0, NULL);

	if(prog->loaded)
		Con_Printf("CSQC ^5loaded (crc=%i, size=%i)\n", csprogsdatacrc, (int)csprogsdatasize);
	else
	{
		CL_VM_Error("CSQC ^2failed to load\n");
		if(!sv.active)
			CL_Disconnect();
		return;
	}

	//[515]: optional fields & funcs
	CL_VM_FindEdictFieldOffsets();

	// set time
	*prog->time = cl.time;
	csqc_frametime = 0;

	prog->globals.client->mapname = PRVM_SetEngineString(cl.worldmodel->name);
	prog->globals.client->player_localentnum = cl.playerentity;

	// call the prog init
	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(CL_F_INIT) - prog->functions), CL_F_INIT);

	PRVM_End;
	csqc_loaded = true;

	csqc_sv2csqcents = (unsigned short *)Mem_Alloc(csqc_mempool, MAX_EDICTS*sizeof(unsigned short));
	memset(csqc_sv2csqcents, 0, MAX_EDICTS*sizeof(unsigned short));

	cl.csqc_vidvars.drawcrosshair = false;
	cl.csqc_vidvars.drawenginesbar = false;

	// local state
	ent = &cl.csqcentities[0];
	// entire entity array was cleared, so just fill in a few fields
	ent->state_current.active = true;
	ent->render.model = cl.worldmodel = cl.model_precache[1];
	ent->render.alpha = 1;
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	CL_UpdateRenderEntity(&ent->render);
}

void CL_VM_ShutDown (void)
{
	Cmd_ClearCsqcFuncs();
	Cvar_SetValueQuick(&csqc_progcrc, -1);
	Cvar_SetValueQuick(&csqc_progsize, -1);
	if(!csqc_loaded)
		return;
	CSQC_BEGIN
		*prog->time = cl.time;
		PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(CL_F_SHUTDOWN) - prog->functions), CL_F_SHUTDOWN);
		PRVM_ResetProg();
	CSQC_END
	Con_Print("CSQC ^1unloaded\n");
	csqc_loaded = false;
	Mem_FreePool(&csqc_mempool);
}
