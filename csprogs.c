#include "quakedef.h"
#include "progsvm.h"
#include "clprogdefs.h"
#include "csprogs.h"
#include "cl_collision.h"
#include "snd_main.h"
#include "clvm_cmds.h"
#include "prvm_cmds.h"

//============================================================================
// Client prog handling
//[515]: omg !!! optimize it ! a lot of hacks here and there also :P

#define CSQC_RETURNVAL	prog->globals.generic[OFS_RETURN]
#define CSQC_BEGIN		csqc_tmpprog=prog;prog=0;PRVM_SetProg(PRVM_CLIENTPROG);
#define CSQC_END		prog=csqc_tmpprog;

static prvm_prog_t *csqc_tmpprog;

void CL_VM_PreventInformationLeaks(void)
{
	prvm_eval_t *val;
	if(!cl.csqc_loaded)
		return;
	CSQC_BEGIN
		VM_ClearTraceGlobals();
		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_networkentity)))
			val->_float = 0;
	CSQC_END
}

//[515]: these are required funcs
static char *cl_required_func[] =
{
	"CSQC_Init",
	"CSQC_InputEvent",
	"CSQC_UpdateView",
	"CSQC_ConsoleCommand",
};

static int cl_numrequiredfunc = sizeof(cl_required_func) / sizeof(char*);

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
	cl.csqc_loaded = false;

	Cvar_SetValueQuick(&csqc_progcrc, -1);
	Cvar_SetValueQuick(&csqc_progsize, -1);

//	Host_AbortCurrentFrame();	//[515]: hmmm... if server says it needs csqc then client MUST disconnect
	Host_Error("CL_VM_Error: %s", errorstring);
}
void CL_VM_UpdateDmgGlobals (int dmg_take, int dmg_save, vec3_t dmg_origin)
{
	prvm_eval_t *val;
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.dmg_take);
		if(val)
			val->_float = dmg_take;
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.dmg_save);
		if(val)
			val->_float = dmg_save;
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.dmg_origin);
		if(val)
		{
			val->vector[0] = dmg_origin[0];
			val->vector[1] = dmg_origin[1];
			val->vector[2] = dmg_origin[2];
		}
		CSQC_END
	}
}

void CSQC_UpdateNetworkTimes(double newtime, double oldtime)
{
	prvm_eval_t *val;
	if(!cl.csqc_loaded)
		return;
	CSQC_BEGIN
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.servertime)))
		val->_float = newtime;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.serverprevtime)))
		val->_float = oldtime;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.serverdeltatime)))
		val->_float = newtime - oldtime;
	CSQC_END
}

//[515]: set globals before calling R_UpdateView, WEIRD CRAP
static void CSQC_SetGlobals (void)
{
	prvm_eval_t *val;
	CSQC_BEGIN
		prog->globals.client->time = cl.time;
		prog->globals.client->frametime = max(0, cl.time - cl.oldtime);
		prog->globals.client->servercommandframe = cls.servermovesequence;
		prog->globals.client->clientcommandframe = cl.movecmd[0].sequence;
		VectorCopy(cl.viewangles, prog->globals.client->input_angles);
		VectorCopy(cl.viewangles, cl.csqc_angles);
		// // FIXME: this actually belongs into getinputstate().. [12/17/2007 Black]
		prog->globals.client->input_buttons = cl.movecmd[0].buttons;
		VectorSet(prog->globals.client->input_movevalues, cl.movecmd[0].forwardmove, cl.movecmd[0].sidemove, cl.movecmd[0].upmove);
		//VectorCopy(cl.movement_origin, cl.csqc_origin);
		Matrix4x4_OriginFromMatrix(&cl.entities[cl.viewentity].render.matrix, cl.csqc_origin);

		// LordHavoc: Spike says not to do this, but without pmove_org the
		// CSQC is useless as it can't alter the view origin without
		// completely replacing it
		VectorCopy(cl.csqc_origin, prog->globals.client->pmove_org);
		VectorCopy(cl.movement_velocity, prog->globals.client->pmove_vel);

		if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.view_angles)))
			VectorCopy(cl.viewangles, val->vector);
		prog->globals.client->maxclients = cl.maxclients;
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
	if(ed->fields.client->nextthink && ed->fields.client->nextthink <= prog->globals.client->time)
	{
		ed->fields.client->nextthink = 0;
		b = prog->globals.client->self;
		prog->globals.client->self = PRVM_EDICT_TO_PROG(ed);
		PRVM_ExecuteProgram(ed->fields.client->think, "CSQC_Think: NULL function\n");
		prog->globals.client->self = b;
	}
}

extern cvar_t cl_noplayershadow;
extern cvar_t r_equalize_entities_fullbright;
qboolean CSQC_AddRenderEdict(prvm_edict_t *ed, int edictnum)
{
	int renderflags;
	int c;
	float scale;
	prvm_eval_t *val;
	entity_render_t *entrender;
	dp_model_t *model;
	matrix4x4_t tagmatrix, matrix2;

	model = CL_GetModelFromEdict(ed);
	if (!model)
		return false;

	if (edictnum)
	{
		if (r_refdef.scene.numentities >= r_refdef.scene.maxentities)
			return false;
		entrender = cl.csqcrenderentities + edictnum;
		r_refdef.scene.entities[r_refdef.scene.numentities++] = entrender;
		entrender->entitynumber = edictnum;
		//entrender->shadertime = 0; // shadertime was set by spawn()
		entrender->alpha = 1;
		entrender->scale = 1;
		VectorSet(entrender->colormod, 1, 1, 1);
		VectorSet(entrender->glowmod, 1, 1, 1);
		entrender->allowdecals = true;
	}
	else
	{
		entrender = CL_NewTempEntity(0);
		if (!entrender)
			return false;
	}

	entrender->model = model;
	entrender->skinnum = (int)ed->fields.client->skin;
	entrender->effects |= entrender->model->effects;
	scale = 1;
	renderflags = 0;
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.renderflags)) && val->_float)	renderflags = (int)val->_float;
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.alpha)) && val->_float)		entrender->alpha = val->_float;
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.scale)) && val->_float)		entrender->scale = scale = val->_float;
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.colormod)) && VectorLength2(val->vector))	VectorCopy(val->vector, entrender->colormod);
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.glowmod)) && VectorLength2(val->vector))	VectorCopy(val->vector, entrender->glowmod);
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.effects)) && val->_float)	entrender->effects |= (int)val->_float;
	if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.tag_entity)) && val->edict)
	{
		int tagentity;
		int tagindex = 0;
		tagentity = val->edict;
		if((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.tag_index)) && val->_float)
			tagindex = (int)val->_float;
		CL_GetTagMatrix (&tagmatrix, PRVM_PROG_TO_EDICT(tagentity), tagindex);
	}
	else
		Matrix4x4_CreateIdentity(&tagmatrix);
	if (!VectorLength2(entrender->colormod))
		VectorSet(entrender->colormod, 1, 1, 1);
	if (!VectorLength2(entrender->glowmod))
		VectorSet(entrender->glowmod, 1, 1, 1);

	if (renderflags & RF_USEAXIS)
	{
		vec3_t left;
		VectorNegate(prog->globals.client->v_right, left);
		Matrix4x4_FromVectors(&matrix2, prog->globals.client->v_forward, left, prog->globals.client->v_up, ed->fields.client->origin);
		Matrix4x4_Scale(&matrix2, scale, 1);
	}
	else
	{
		vec3_t angles;
		VectorCopy(ed->fields.client->angles, angles);
		// if model is alias, reverse pitch direction
		if (entrender->model->type == mod_alias)
			angles[0] = -angles[0];

		// set up the render matrix
		Matrix4x4_CreateFromQuakeEntity(&matrix2, ed->fields.client->origin[0], ed->fields.client->origin[1], ed->fields.client->origin[2], angles[0], angles[1], angles[2], scale);
	}

	// set up the animation data
	VM_GenerateFrameGroupBlend(ed->priv.server->framegroupblend, ed);
	VM_FrameBlendFromFrameGroupBlend(ed->priv.server->frameblend, ed->priv.server->framegroupblend, model);
	VM_UpdateEdictSkeleton(ed, model, ed->priv.server->frameblend);
	if ((val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.shadertime))) entrender->shadertime = val->_float;

	// concat the matrices to make the entity relative to its tag
	Matrix4x4_Concat(&entrender->matrix, &tagmatrix, &matrix2);

	// transparent offset
	if ((renderflags & RF_USETRANSPARENTOFFSET) && (val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.transparent_offset)))
		entrender->transparent_offset = val->_float;

	if(renderflags)
	{
		if(renderflags & RF_VIEWMODEL)	entrender->flags |= RENDER_VIEWMODEL | RENDER_NODEPTHTEST;
		if(renderflags & RF_EXTERNALMODEL)entrender->flags |= RENDER_EXTERIORMODEL;
		if(renderflags & RF_NOCULL)		entrender->flags |= RENDER_NOCULL;
		if(renderflags & RF_DEPTHHACK)	entrender->flags |= RENDER_NODEPTHTEST;
		if(renderflags & RF_ADDITIVE)		entrender->flags |= RENDER_ADDITIVE;

	}

	c = (int)ed->fields.client->colormap;
	if (c <= 0)
		CL_SetEntityColormapColors(entrender, -1);
	else if (c <= cl.maxclients && cl.scores != NULL)
		CL_SetEntityColormapColors(entrender, cl.scores[c-1].colors);
	else
		CL_SetEntityColormapColors(entrender, c);

	entrender->flags &= ~(RENDER_SHADOW | RENDER_LIGHT | RENDER_NOSELFSHADOW);
	// either fullbright or lit
	if(!r_fullbright.integer)
	{
		if (!(entrender->effects & EF_FULLBRIGHT))
			entrender->flags |= RENDER_LIGHT;
		else if(r_equalize_entities_fullbright.integer)
			entrender->flags |= RENDER_LIGHT | RENDER_EQUALIZE;
	}
	// hide player shadow during intermission or nehahra movie
	if (!(entrender->effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST))
	 &&  (entrender->alpha >= 1)
	 && !(entrender->flags & RENDER_VIEWMODEL)
	 && (!(entrender->flags & RENDER_EXTERIORMODEL) || (!cl.intermission && cls.protocol != PROTOCOL_NEHAHRAMOVIE && !cl_noplayershadow.integer)))
		entrender->flags |= RENDER_SHADOW;
	if (entrender->flags & RENDER_VIEWMODEL)
		entrender->flags |= RENDER_NOSELFSHADOW;
	if (entrender->effects & EF_NOSELFSHADOW)
		entrender->flags |= RENDER_NOSELFSHADOW;
	if (entrender->effects & EF_NODEPTHTEST)
		entrender->flags |= RENDER_NODEPTHTEST;
	if (entrender->effects & EF_ADDITIVE)
		entrender->flags |= RENDER_ADDITIVE;
	if (entrender->effects & EF_DOUBLESIDED)
		entrender->flags |= RENDER_DOUBLESIDED;

	// make the other useful stuff
	memcpy(entrender->framegroupblend, ed->priv.server->framegroupblend, sizeof(ed->priv.server->framegroupblend));
	CL_UpdateRenderEntity(entrender);
	// override animation data with full control
	memcpy(entrender->frameblend, ed->priv.server->frameblend, sizeof(ed->priv.server->frameblend));
	if (ed->priv.server->skeleton.relativetransforms)
		entrender->skeleton = &ed->priv.server->skeleton;
	else
		entrender->skeleton = NULL;

	return true;
}

qboolean CL_VM_InputEvent (qboolean down, int key, int ascii)
{
	qboolean r;

	if(!cl.csqc_loaded)
		return false;

	CSQC_BEGIN
		if (!prog->funcoffsets.CSQC_InputEvent)
			r = false;
		else
		{
			prog->globals.client->time = cl.time;
			prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
			PRVM_G_FLOAT(OFS_PARM0) = !down; // 0 is down, 1 is up
			PRVM_G_FLOAT(OFS_PARM1) = key;
			PRVM_G_FLOAT(OFS_PARM2) = ascii;
			PRVM_ExecuteProgram(prog->funcoffsets.CSQC_InputEvent, "QC function CSQC_InputEvent is missing");
			r = CSQC_RETURNVAL != 0;
		}
	CSQC_END
	return r;
}

qboolean CL_VM_UpdateView (void)
{
	vec3_t emptyvector;
	emptyvector[0] = 0;
	emptyvector[1] = 0;
	emptyvector[2] = 0;
//	vec3_t oldangles;
	if(!cl.csqc_loaded)
		return false;
	CSQC_BEGIN
		//VectorCopy(cl.viewangles, oldangles);
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		CSQC_SetGlobals();
		// clear renderable entity and light lists to prevent crashes if the
		// CSQC_UpdateView function does not call R_ClearScene as it should
		r_refdef.scene.numentities = 0;
		r_refdef.scene.numlights = 0;
		// pass in width and height as parameters (EXT_CSQC_1)
		PRVM_G_FLOAT(OFS_PARM0) = vid.width;
		PRVM_G_FLOAT(OFS_PARM1) = vid.height;
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_UpdateView, "QC function CSQC_UpdateView is missing");
		//VectorCopy(oldangles, cl.viewangles);
		// Dresk : Reset Dmg Globals Here
		CL_VM_UpdateDmgGlobals(0, 0, emptyvector);
	CSQC_END
	return true;
}

extern sizebuf_t vm_tempstringsbuf;
qboolean CL_VM_ConsoleCommand (const char *cmd)
{
	int restorevm_tempstringsbuf_cursize;
	qboolean r = false;
	if(!cl.csqc_loaded)
		return false;
	CSQC_BEGIN
	if (prog->funcoffsets.CSQC_ConsoleCommand)
	{
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(cmd);
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_ConsoleCommand, "QC function CSQC_ConsoleCommand is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
		r = CSQC_RETURNVAL != 0;
	}
	CSQC_END
	return r;
}

qboolean CL_VM_Parse_TempEntity (void)
{
	int			t;
	qboolean	r = false;
	if(!cl.csqc_loaded)
		return false;
	CSQC_BEGIN
	if(prog->funcoffsets.CSQC_Parse_TempEntity)
	{
		t = msg_readcount;
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Parse_TempEntity, "QC function CSQC_Parse_TempEntity is missing");
		r = CSQC_RETURNVAL != 0;
		if(!r)
		{
			msg_readcount = t;
			msg_badread = false;
		}
	}
	CSQC_END
	return r;
}

void CL_VM_Parse_StuffCmd (const char *msg)
{
	int restorevm_tempstringsbuf_cursize;
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

	if(cls.demoplayback)
	if(!strncmp(msg, "curl --clear_autodownload\ncurl --pak --forthismap --as ", 55))
	{
		// special handling for map download commands
		// run these commands IMMEDIATELY, instead of waiting for a client frame
		// that way, there is no black screen when playing back demos
		// I know this is a really ugly hack, but I can't think of any better way
		// FIXME find the actual CAUSE of this, and make demo playback WAIT
		// until all maps are loaded, then remove this hack

		char buf[MAX_INPUTLINE];
		const char *p, *q;
		size_t l;

		p = msg;

		for(;;)
		{
			q = strchr(p, '\n');
			if(q)
				l = q - p;
			else
				l = strlen(p);
			if(l > sizeof(buf) - 1)
				l = sizeof(buf) - 1;
			strlcpy(buf, p, l + 1); // strlcpy needs a + 1 as it includes the newline!

			Cmd_ExecuteString(buf, src_command);

			p += l;
			if(*p == '\n')
				++p; // skip the newline and continue
			else
				break; // end of string or overflow
		}
		Cmd_ExecuteString("curl --clear_autodownload", src_command); // don't inhibit CSQC loading
		return;
	}

	if(!cl.csqc_loaded)
	{
		Cbuf_AddText(msg);
		return;
	}
	CSQC_BEGIN
	if(prog->funcoffsets.CSQC_Parse_StuffCmd)
	{
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Parse_StuffCmd, "QC function CSQC_Parse_StuffCmd is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}
	else
		Cbuf_AddText(msg);
	CSQC_END
}

static void CL_VM_Parse_Print (const char *msg)
{
	int restorevm_tempstringsbuf_cursize;
	prog->globals.client->time = cl.time;
	prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
	restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
	PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
	PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Parse_Print, "QC function CSQC_Parse_Print is missing");
	vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
}

void CSQC_AddPrintText (const char *msg)
{
	size_t i;
	if(!cl.csqc_loaded)
	{
		Con_Print(msg);
		return;
	}
	CSQC_BEGIN
	if(prog->funcoffsets.CSQC_Parse_Print)
	{
		// FIXME: is this bugged?
		i = strlen(msg)-1;
		if(msg[i] != '\n' && msg[i] != '\r')
		{
			if(strlen(cl.csqc_printtextbuf)+i >= MAX_INPUTLINE)
			{
				CL_VM_Parse_Print(cl.csqc_printtextbuf);
				cl.csqc_printtextbuf[0] = 0;
			}
			else
				strlcat(cl.csqc_printtextbuf, msg, MAX_INPUTLINE);
			return;
		}
		strlcat(cl.csqc_printtextbuf, msg, MAX_INPUTLINE);
		CL_VM_Parse_Print(cl.csqc_printtextbuf);
		cl.csqc_printtextbuf[0] = 0;
	}
	else
		Con_Print(msg);
	CSQC_END
}

void CL_VM_Parse_CenterPrint (const char *msg)
{
	int restorevm_tempstringsbuf_cursize;
	if(!cl.csqc_loaded)
	{
		SCR_CenterPrint(msg);
		return;
	}
	CSQC_BEGIN
	if(prog->funcoffsets.CSQC_Parse_CenterPrint)
	{
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Parse_CenterPrint, "QC function CSQC_Parse_CenterPrint is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}
	else
		SCR_CenterPrint(msg);
	CSQC_END
}

void CL_VM_UpdateIntermissionState (int intermission)
{
	prvm_eval_t *val;
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.intermission);
		if(val)
			val->_float = intermission;
		CSQC_END
	}
}
void CL_VM_UpdateShowingScoresState (int showingscores)
{
	prvm_eval_t *val;
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.sb_showscores);
		if(val)
			val->_float = showingscores;
		CSQC_END
	}
}
qboolean CL_VM_Event_Sound(int sound_num, float volume, int channel, float attenuation, int ent, vec3_t pos)
{
	qboolean r = false;
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		if(prog->funcoffsets.CSQC_Event_Sound)
		{
			prog->globals.client->time = cl.time;
			prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
			PRVM_G_FLOAT(OFS_PARM0) = ent;
			PRVM_G_FLOAT(OFS_PARM1) = channel;
			PRVM_G_INT(OFS_PARM2) = PRVM_SetTempString(cl.sound_name[sound_num] );
			PRVM_G_FLOAT(OFS_PARM3) = volume;
			PRVM_G_FLOAT(OFS_PARM4) = attenuation;
			VectorCopy(pos, PRVM_G_VECTOR(OFS_PARM5) );
			PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Event_Sound, "QC function CSQC_Event_Sound is missing");
			r = CSQC_RETURNVAL != 0;
		}
		CSQC_END
	}

	return r;
}
void CL_VM_UpdateCoopDeathmatchGlobals (int gametype)
{
	// Avoid global names for clean(er) coding
	int localcoop;
	int localdeathmatch;

	prvm_eval_t *val;
	if(cl.csqc_loaded)
	{
		if(gametype == GAME_COOP)
		{
			localcoop = 1;
			localdeathmatch = 0;
		}
		else
		if(gametype == GAME_DEATHMATCH)
		{
			localcoop = 0;
			localdeathmatch = 1;
		}
		else
		{
			// How did the ServerInfo send an unknown gametype?
			// Better just assign the globals as 0...
			localcoop = 0;
			localdeathmatch = 0;
		}
		CSQC_BEGIN
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.coop);
		if(val)
			val->_float = localcoop;
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.deathmatch);
		if(val)
			val->_float = localdeathmatch;
		CSQC_END
	}
}
float CL_VM_Event (float event)		//[515]: needed ? I'd say "YES", but don't know for what :D
{
	float r = 0;
	if(!cl.csqc_loaded)
		return 0;
	CSQC_BEGIN
	if(prog->funcoffsets.CSQC_Event)
	{
		prog->globals.client->time = cl.time;
		prog->globals.client->self = cl.csqc_server2csqcentitynumber[cl.playerentity];
		PRVM_G_FLOAT(OFS_PARM0) = event;
		PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Event, "QC function CSQC_Event is missing");
		r = CSQC_RETURNVAL;
	}
	CSQC_END
	return r;
}

void CSQC_ReadEntities (void)
{
	unsigned short entnum, oldself, realentnum;
	if(!cl.csqc_loaded)
	{
		Host_Error ("CSQC_ReadEntities: CSQC is not loaded");
		return;
	}

	CSQC_BEGIN
		prog->globals.client->time = cl.time;
		oldself = prog->globals.client->self;
		while(1)
		{
			entnum = MSG_ReadShort();
			if(!entnum || msg_badread)
				return;
			realentnum = entnum & 0x7FFF;
			prog->globals.client->self = cl.csqc_server2csqcentitynumber[realentnum];
			if(entnum & 0x8000)
			{
				if(prog->globals.client->self)
				{
					PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Ent_Remove, "QC function CSQC_Ent_Remove is missing");
					cl.csqc_server2csqcentitynumber[realentnum] = 0;
				}
				else
				{
					// LordHavoc: removing an entity that is already gone on
					// the csqc side is possible for legitimate reasons (such
					// as a repeat of the remove message), so no warning is
					// needed
					//Con_Printf("Bad csqc_server2csqcentitynumber map\n");	//[515]: never happens ?
				}
			}
			else
			{
				if(!prog->globals.client->self)
				{
					if(!prog->funcoffsets.CSQC_Ent_Spawn)
					{
						prvm_edict_t	*ed;
						ed = PRVM_ED_Alloc();
						ed->fields.client->entnum = realentnum;
						prog->globals.client->self = cl.csqc_server2csqcentitynumber[realentnum] = PRVM_EDICT_TO_PROG(ed);
					}
					else
					{
						// entity( float entnum ) CSQC_Ent_Spawn;
						// the qc function should set entnum, too (this way it also can return world [2/1/2008 Andreas]
						PRVM_G_FLOAT(OFS_PARM0) = (float) realentnum;
						// make sure no one gets wrong ideas
						prog->globals.client->self = 0;
						PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Ent_Spawn, "QC function CSQC_Ent_Spawn is missing");
						prog->globals.client->self = cl.csqc_server2csqcentitynumber[realentnum] = PRVM_EDICT( PRVM_G_INT( OFS_RETURN ) );
					}
					PRVM_G_FLOAT(OFS_PARM0) = 1;
					PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Ent_Update, "QC function CSQC_Ent_Update is missing");
				}
				else {
					PRVM_G_FLOAT(OFS_PARM0) = 0;
					PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Ent_Update, "QC function CSQC_Ent_Update is missing");
				}
			}
		}
		prog->globals.client->self = oldself;
	CSQC_END
}

void CL_VM_CB_BeginIncreaseEdicts(void)
{
	// links don't survive the transition, so unlink everything
	World_UnlinkAll(&cl.world);
}

void CL_VM_CB_EndIncreaseEdicts(void)
{
	int i;
	prvm_edict_t *ent;

	// link every entity except world
	for (i = 1, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
		if (!ent->priv.server->free)
			CL_LinkEdict(ent);
}

void CL_VM_CB_InitEdict(prvm_edict_t *e)
{
	int edictnum = PRVM_NUM_FOR_EDICT(e);
	entity_render_t *entrender;
	CL_ExpandCSQCRenderEntities(edictnum);
	entrender = cl.csqcrenderentities + edictnum;
	e->priv.server->move = false; // don't move on first frame
	memset(entrender, 0, sizeof(*entrender));
	entrender->shadertime = cl.time;
}

extern void R_DecalSystem_Reset(decalsystem_t *decalsystem);

void CL_VM_CB_FreeEdict(prvm_edict_t *ed)
{
	entity_render_t *entrender = cl.csqcrenderentities + PRVM_NUM_FOR_EDICT(ed);
	R_DecalSystem_Reset(&entrender->decalsystem);
	memset(entrender, 0, sizeof(*entrender));
	World_UnlinkEdict(ed);
	memset(ed->fields.client, 0, sizeof(*ed->fields.client));
	VM_RemoveEdictSkeleton(ed);
	World_Physics_RemoveFromEntity(&cl.world, ed);
	World_Physics_RemoveJointFromEntity(&cl.world, ed);
}

void CL_VM_CB_CountEdicts(void)
{
	int		i;
	prvm_edict_t	*ent;
	int		active = 0, models = 0, solid = 0;

	for (i=0 ; i<prog->num_edicts ; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.server->free)
			continue;
		active++;
		if (ent->fields.client->solid)
			solid++;
		if (ent->fields.client->model)
			models++;
	}

	Con_Printf("num_edicts:%3i\n", prog->num_edicts);
	Con_Printf("active    :%3i\n", active);
	Con_Printf("view      :%3i\n", models);
	Con_Printf("touch     :%3i\n", solid);
}

qboolean CL_VM_CB_LoadEdict(prvm_edict_t *ent)
{
	return true;
}

void Cmd_ClearCsqcFuncs (void);

// returns true if the packet is valid, false if end of file is reached
// used for dumping the CSQC download into demo files
qboolean MakeDownloadPacket(const char *filename, unsigned char *data, unsigned long len, int crc, int cnt, sizebuf_t *buf, int protocol)
{
	int packetsize = buf->maxsize - 7; // byte short long
	int npackets = (len + packetsize - 1) / (packetsize);

	if(protocol == PROTOCOL_QUAKEWORLD)
		return false; // CSQC can't run in QW anyway

	SZ_Clear(buf);
	if(cnt == 0)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("\ncl_downloadbegin %lu %s\n", len, filename));
		return true;
	}
	else if(cnt >= 1 && cnt <= npackets)
	{
		unsigned long thispacketoffset = (cnt - 1) * packetsize;
		int thispacketsize = len - thispacketoffset;
		if(thispacketsize > packetsize)
			thispacketsize = packetsize;

		MSG_WriteByte(buf, svc_downloaddata);
		MSG_WriteLong(buf, thispacketoffset);
		MSG_WriteShort(buf, thispacketsize);
		SZ_Write(buf, data + thispacketoffset, thispacketsize);

		return true;
	}
	else if(cnt == npackets + 1)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("\ncl_downloadfinished %lu %d\n", len, crc));
		return true;
	}
	return false;
}

void CL_VM_Init (void)
{
	const char* csprogsfn;
	unsigned char *csprogsdata;
	fs_offset_t csprogsdatasize;
	int csprogsdatacrc, requiredcrc;
	int requiredsize;
	prvm_eval_t *val;

	// reset csqc_progcrc after reading it, so that changing servers doesn't
	// expect csqc on the next server
	requiredcrc = csqc_progcrc.integer;
	requiredsize = csqc_progsize.integer;
	Cvar_SetValueQuick(&csqc_progcrc, -1);
	Cvar_SetValueQuick(&csqc_progsize, -1);

	// if the server is not requesting a csprogs, then we're done here
	if (requiredcrc < 0)
		return;

	// see if the requested csprogs.dat file matches the requested crc
	csprogsdatacrc = -1;
	csprogsfn = va("dlcache/%s.%i.%i", csqc_progname.string, requiredsize, requiredcrc);
	csprogsdata = FS_LoadFile(csprogsfn, tempmempool, true, &csprogsdatasize);
	if (!csprogsdata)
	{
		csprogsfn = csqc_progname.string;
		csprogsdata = FS_LoadFile(csprogsfn, tempmempool, true, &csprogsdatasize);
	}
	if (csprogsdata)
	{
		csprogsdatacrc = CRC_Block(csprogsdata, csprogsdatasize);
		if (csprogsdatacrc != requiredcrc || csprogsdatasize != requiredsize)
		{
			if (cls.demoplayback)
			{
				Con_Printf("^1Warning: Your %s is not the same version as the demo was recorded with (CRC/size are %i/%i but should be %i/%i)\n", csqc_progname.string, csprogsdatacrc, (int)csprogsdatasize, requiredcrc, requiredsize);
				// Mem_Free(csprogsdata);
				// return;
				// We WANT to continue here, and play the demo with different csprogs!
				// After all, this is just a warning. Sure things may go wrong from here.
			}
			else
			{
				Mem_Free(csprogsdata);
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

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool(csqc_progname.string, 0, NULL);
	prog->headercrc = CL_PROGHEADER_CRC;
	prog->edictprivate_size = 0; // no private struct used
	prog->name = CL_NAME;
	prog->num_edicts = 1;
	prog->max_edicts = 512;
	prog->limit_edicts = CL_MAX_EDICTS;
	prog->reserved_edicts = 0;
	prog->edictprivate_size = sizeof(edict_engineprivate_t);
	// TODO: add a shared extension string #define and add real support for csqc extension strings [12/5/2007 Black]
	prog->extensionstring = vm_sv_extensions;
	prog->builtins = vm_cl_builtins;
	prog->numbuiltins = vm_cl_numbuiltins;
	prog->begin_increase_edicts = CL_VM_CB_BeginIncreaseEdicts;
	prog->end_increase_edicts = CL_VM_CB_EndIncreaseEdicts;
	prog->init_edict = CL_VM_CB_InitEdict;
	prog->free_edict = CL_VM_CB_FreeEdict;
	prog->count_edicts = CL_VM_CB_CountEdicts;
	prog->load_edict = CL_VM_CB_LoadEdict;
	prog->init_cmd = VM_CL_Cmd_Init;
	prog->reset_cmd = VM_CL_Cmd_Reset;
	prog->error_cmd = CL_VM_Error;
	prog->ExecuteProgram = CLVM_ExecuteProgram;

	PRVM_LoadProgs(csprogsfn, cl_numrequiredfunc, cl_required_func, 0, NULL, 0, NULL);

	if (!prog->loaded)
	{
		CL_VM_Error("CSQC %s ^2failed to load\n", csprogsfn);
		if(!sv.active)
			CL_Disconnect();
		Mem_Free(csprogsdata);
		return;
	}

	Con_DPrintf("CSQC %s ^5loaded (crc=%i, size=%i)\n", csprogsfn, csprogsdatacrc, (int)csprogsdatasize);

	if(cls.demorecording)
	{
		if(cls.demo_lastcsprogssize != csprogsdatasize || cls.demo_lastcsprogscrc != csprogsdatacrc)
		{
			int i;
			char buf[NET_MAXMESSAGE];
			sizebuf_t sb;
			unsigned char *demobuf; fs_offset_t demofilesize;

			sb.data = (unsigned char *) buf;
			sb.maxsize = sizeof(buf);
			i = 0;

			CL_CutDemo(&demobuf, &demofilesize);
			while(MakeDownloadPacket(csqc_progname.string, csprogsdata, csprogsdatasize, csprogsdatacrc, i++, &sb, cls.protocol))
				CL_WriteDemoMessage(&sb);
			CL_PasteDemo(&demobuf, &demofilesize);

			cls.demo_lastcsprogssize = csprogsdatasize;
			cls.demo_lastcsprogscrc = csprogsdatacrc;
		}
	}
	Mem_Free(csprogsdata);

	// check if OP_STATE animation is possible in this dat file
	if (prog->fieldoffsets.nextthink >= 0 && prog->fieldoffsets.frame >= 0 && prog->fieldoffsets.think >= 0 && prog->globaloffsets.self >= 0)
		prog->flag |= PRVM_OP_STATE;

	// set time
	prog->globals.client->time = cl.time;
	prog->globals.client->self = 0;

	prog->globals.client->mapname = cl.worldmodel ? PRVM_SetEngineString(cl.worldmodel->name) : PRVM_SetEngineString("");
	prog->globals.client->player_localentnum = cl.playerentity;

	// set map description (use world entity 0)
	val = PRVM_EDICTFIELDVALUE(prog->edicts, prog->fieldoffsets.message);
	if(val)
		val->string = PRVM_SetEngineString(cl.levelname);
	VectorCopy(cl.world.mins, prog->edicts->fields.client->mins);
	VectorCopy(cl.world.maxs, prog->edicts->fields.client->maxs);

	// call the prog init
	PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Init, "QC function CSQC_Init is missing");

	PRVM_End;
	cl.csqc_loaded = true;

	cl.csqc_vidvars.drawcrosshair = false;
	cl.csqc_vidvars.drawenginesbar = false;

	// Update Coop and Deathmatch Globals (at this point the client knows them from ServerInfo)
	CL_VM_UpdateCoopDeathmatchGlobals(cl.gametype);
}

void CL_VM_ShutDown (void)
{
	Cmd_ClearCsqcFuncs();
	//Cvar_SetValueQuick(&csqc_progcrc, -1);
	//Cvar_SetValueQuick(&csqc_progsize, -1);
	if(!cl.csqc_loaded)
		return;
	CSQC_BEGIN
		prog->globals.client->time = cl.time;
		prog->globals.client->self = 0;
		if (prog->funcoffsets.CSQC_Shutdown)
			PRVM_ExecuteProgram(prog->funcoffsets.CSQC_Shutdown, "QC function CSQC_Shutdown is missing");
		PRVM_ResetProg();
	CSQC_END
	Con_DPrint("CSQC ^1unloaded\n");
	cl.csqc_loaded = false;
}

qboolean CL_VM_GetEntitySoundOrigin(int entnum, vec3_t out)
{
	prvm_edict_t *ed;
	dp_model_t *mod;
	matrix4x4_t matrix;
	qboolean r = 0;

	CSQC_BEGIN;

	// FIXME consider attachments here!

	ed = PRVM_EDICT_NUM(entnum - MAX_EDICTS);

	if(!ed->priv.required->free)
	{
		mod = CL_GetModelFromEdict(ed);
		VectorCopy(ed->fields.client->origin, out);
		if(CL_GetTagMatrix (&matrix, ed, 0) == 0)
			Matrix4x4_OriginFromMatrix(&matrix, out);
		if (mod && mod->soundfromcenter)
			VectorMAMAM(1.0f, out, 0.5f, mod->normalmins, 0.5f, mod->normalmaxs, out);
		r = 1;
	}

	CSQC_END;

	return r;
}
