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

unsigned int			csqc_drawmask = 0;
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

	Cvar_SetValueQuick(&csqc_progcrc, 0);

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
		VectorCopy(cl.entities[cl.viewentity].render.origin, csqc_origin);
		VectorCopy(csqc_origin, prog->globals.client->pmove_org);
		prog->globals.client->maxclients = cl.maxclients;
		//VectorCopy(cl.movement_velocity, prog->globals.client->pmove_vel);
		VectorCopy(cl.velocity, prog->globals.client->pmove_vel);
	CSQC_END
}

static void CSQC_Predraw (prvm_edict_t *ed)
{
	int b;
	if(!ed->fields.client->predraw)
		return;
	b = prog->globals.client->self;
	prog->globals.client->self = PRVM_EDICT_TO_PROG(ed);
	PRVM_ExecuteProgram(ed->fields.client->predraw, "CSQC_Predraw: NULL function\n");
	prog->globals.client->self = b;
}

static void CSQC_Think (prvm_edict_t *ed)
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

//[515]: weird too
static qboolean CSQC_EdictToEntity (prvm_edict_t *ed, entity_t *e)
{
	int i;
	prvm_eval_t *val;

	i = (int)ed->fields.client->modelindex;
	e->state_current.modelindex = 0;
	if(i >= MAX_MODELS || i <= -MAX_MODELS)	//[515]: make work as error ?
	{
		Con_Print("CSQC_EdictToEntity: modelindex >= MAX_MODELS\n");
		ed->fields.client->modelindex = 0;
	}
	else
		e->state_current.modelindex = i;
	if(!e->state_current.modelindex)
		return false;

	e->state_current.time = cl.time;

	i = 0;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_renderflags)) && val->_float)
	{
		i = (int)val->_float;
		if(i & RF_VIEWMODEL)	e->state_current.flags |= RENDER_VIEWMODEL;
		if(i & RF_EXTERNALMODEL)e->state_current.flags |= RENDER_EXTERIORMODEL;
		if(i & RF_DEPTHHACK)	e->state_current.effects |= EF_NODEPTHTEST;
		if(i & RF_ADDITIVE)		e->state_current.effects |= EF_ADDITIVE;
	}

	if(i & RF_USEAXIS)	//FIXME!!!
		VectorCopy(ed->fields.client->angles, e->persistent.newangles);
	else
		VectorCopy(ed->fields.client->angles, e->persistent.newangles);

	VectorCopy(ed->fields.client->origin, e->persistent.neworigin);
	VectorCopy(ed->fields.client->origin, e->state_current.origin);
	e->state_current.colormap = (int)ed->fields.client->colormap;
	e->state_current.effects = (int)ed->fields.client->effects;
	e->state_current.frame = (int)ed->fields.client->frame;
	e->state_current.skin = (int)ed->fields.client->skin;

	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_alpha)) && val->_float)		e->state_current.alpha = (int)(val->_float*255);
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_scale)) && val->_float)		e->state_current.scale = (int)(val->_float*16);
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_colormod)) && VectorLength2(val->vector))	{int j;for (j = 0;j < 3;j++) e->state_current.colormod[j] = (unsigned char)bound(0, val->vector[j] * 32.0f, 255);}
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_effects)) && val->_float)	e->state_current.effects = (int)val->_float;
	if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_tag_entity)) && val->edict)
	{
		e->state_current.tagentity = val->edict;
		if((val = PRVM_GETEDICTFIELDVALUE(ed, csqc_fieldoff_tag_index)) && val->_float)
			e->state_current.tagindex = (int)val->_float;
	}

	return true;
}

void CSQC_ClearCSQCEntities (void)
{
	memset(cl.csqcentities_active, 0, sizeof(cl.csqcentities_active));
	cl.num_csqcentities = 0;
	csqc_drawmask = 0;
}

void CL_ExpandCSQCEntities (int num);

void CSQC_RelinkCSQCEntities (void)
{
	int			i;
	entity_t	*e;
	prvm_edict_t *ed;

	*prog->time = cl.time;
	for(i=1;i<prog->num_edicts;i++)
	{
		if(i >= cl.max_csqcentities)
			CL_ExpandCSQCEntities(i);

		e = &cl.csqcentities[i];
		ed = &prog->edicts[i];
		if(ed->priv.required->free)
		{
			e->state_current.active = false;
			cl.csqcentities_active[i] = false;
			continue;
		}
		VectorAdd(ed->fields.client->origin, ed->fields.client->mins, ed->fields.client->absmin);
		VectorAdd(ed->fields.client->origin, ed->fields.client->maxs, ed->fields.client->absmax);
		CSQC_Think(ed);
		if(ed->priv.required->free)
		{
			e->state_current.active = false;
			cl.csqcentities_active[i] = false;
			continue;
		}
		CSQC_Predraw(ed);
		if(ed->priv.required->free)
		{
			e->state_current.active = false;
			cl.csqcentities_active[i] = false;
			continue;
		}
		if(!cl.csqcentities_active[i])
		if(!((int)ed->fields.client->drawmask & csqc_drawmask))
			continue;

		e->state_previous	= e->state_current;
		e->state_current	= defaultstate;
		if((cl.csqcentities_active[i] = CSQC_EdictToEntity(ed, e)))
		{
			if(!e->state_current.active)
			{
				if(!e->state_previous.active)
					VectorCopy(ed->fields.client->origin, e->persistent.trail_origin);//[515]: hack to make good gibs =/
				e->state_previous = e->state_current;
				e->state_current.active = true;
			}
			e->persistent.lerpdeltatime = 0;//prog->globals.client->frametime;
			cl.num_csqcentities++;
		}
	}
}

//[515]: omfg... it's all weird =/
void CSQC_AddEntity (int n)
{
	cl.csqcentities_active[n] = true;
}

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
		csqc_drawmask = 0;
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
	if(!csqc_loaded)	//[515]: add more here
	if(msg[0] == 'c')
	if(msg[1] == 's')
	if(msg[2] == 'q')
	if(msg[3] == 'c')
	{
		Cvar_SetQuick(&csqc_progcrc, "0");
		csqc_progcrc.flags = 0;
		Cmd_ExecuteString (msg, src_command);
		csqc_progcrc.flags = CVAR_READONLY;
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
			strcat(csqc_printtextbuf, msg);
		return;
	}
	strcat(csqc_printtextbuf, msg);
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
	entity_t *ent;

	csqc_loaded = false;
	memset(cl.csqc_model_precache, 0, sizeof(cl.csqc_model_precache));
	memset(&cl.csqc_vidvars, true, sizeof(csqc_vidvars_t));

	if(!FS_FileExists(csqc_progname.string))
	{
		if(!sv.active && csqc_progcrc.integer)
		{
			Con_Printf("CL_VM_Init: server requires CSQC, but \"%s\" wasn't found\n", csqc_progname.string);
			CL_Disconnect();
		}
		return;
	}
	else
	{
		if (developer.integer < 100)
		{
			Con_DPrintf("CL_VM_Init: CSQC is broken, and is not being loaded because developer is less than 100.\n");
			return;
		}
		if(!sv.active && !csqc_progcrc.integer)	//[515]: because cheaters may use csqc while server didn't allowed it !
		{
			Con_Printf("CL_VM_Init: server didn't sent CSQC crc, so CSQC is disabled\n");
			return;
		}
	}

	PRVM_Begin;
	PRVM_InitProg(PRVM_CLIENTPROG);

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool(csqc_progname.string, 0, NULL);
	prog->headercrc = CL_PROGHEADER_CRC;
	prog->edictprivate_size = 0; // no private struct used
	prog->name = csqc_progname.string;
	prog->num_edicts = 1;
	prog->limit_edicts = CL_MAX_EDICTS;
	prog->extensionstring = vm_cl_extensions;
	prog->builtins = vm_cl_builtins;
	prog->numbuiltins = vm_cl_numbuiltins;
	prog->init_cmd = VM_CL_Cmd_Init;
	prog->reset_cmd = VM_CL_Cmd_Reset;
	prog->error_cmd = CL_VM_Error;

	PRVM_LoadProgs(csqc_progname.string, cl_numrequiredfunc, cl_required_func, 0, NULL);

	if(!sv.active && !cls.demoplayback && prog->filecrc != (unsigned short)csqc_progcrc.integer)
	{
		Con_Printf("^1Your CSQC version differs from server's one (%i!=%i)\n", prog->filecrc, csqc_progcrc.integer);
		PRVM_ResetProg();
		CL_Disconnect();
		return;
	}

	if(prog->loaded)
	{
		Cvar_SetValueQuick(&csqc_progcrc, prog->filecrc);
		Con_Printf("CSQC ^5loaded (crc=%i)\n", csqc_progcrc.integer);
	}
	else
	{
		CL_VM_Error("CSQC ^2failed to load\n");
		if(!sv.active)
			CL_Disconnect();
		return;
	}

	csqc_mempool = Mem_AllocPool("CSQC", 0, NULL);

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
	ent->render.scale = 1; // some of the renderer still relies on scale
	ent->render.alpha = 1;
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);
}

void CL_VM_ShutDown (void)
{
	Cmd_ClearCsqcFuncs();
	Cvar_SetValueQuick(&csqc_progcrc, 0);
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
