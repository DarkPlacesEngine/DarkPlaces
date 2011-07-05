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
	if(!cl.csqc_loaded)
		return;
	CSQC_BEGIN
		VM_ClearTraceGlobals();
		PRVM_clientglobalfloat(trace_networkentity) = 0;
	CSQC_END
}

//[515]: these are required funcs
static const char *cl_required_func[] =
{
	"CSQC_Init",
	"CSQC_InputEvent",
	"CSQC_UpdateView",
	"CSQC_ConsoleCommand",
};

static int cl_numrequiredfunc = sizeof(cl_required_func) / sizeof(char*);

#define CL_REQFIELDS (sizeof(cl_reqfields) / sizeof(prvm_required_field_t))

prvm_required_field_t cl_reqfields[] =
{
#define PRVM_DECLARE_serverglobalfloat(x)
#define PRVM_DECLARE_serverglobalvector(x)
#define PRVM_DECLARE_serverglobalstring(x)
#define PRVM_DECLARE_serverglobaledict(x)
#define PRVM_DECLARE_serverglobalfunction(x)
#define PRVM_DECLARE_clientglobalfloat(x)
#define PRVM_DECLARE_clientglobalvector(x)
#define PRVM_DECLARE_clientglobalstring(x)
#define PRVM_DECLARE_clientglobaledict(x)
#define PRVM_DECLARE_clientglobalfunction(x)
#define PRVM_DECLARE_menuglobalfloat(x)
#define PRVM_DECLARE_menuglobalvector(x)
#define PRVM_DECLARE_menuglobalstring(x)
#define PRVM_DECLARE_menuglobaledict(x)
#define PRVM_DECLARE_menuglobalfunction(x)
#define PRVM_DECLARE_serverfieldfloat(x)
#define PRVM_DECLARE_serverfieldvector(x)
#define PRVM_DECLARE_serverfieldstring(x)
#define PRVM_DECLARE_serverfieldedict(x)
#define PRVM_DECLARE_serverfieldfunction(x)
#define PRVM_DECLARE_clientfieldfloat(x) {ev_float, #x },
#define PRVM_DECLARE_clientfieldvector(x) {ev_vector, #x },
#define PRVM_DECLARE_clientfieldstring(x) {ev_string, #x },
#define PRVM_DECLARE_clientfieldedict(x) {ev_entity, #x },
#define PRVM_DECLARE_clientfieldfunction(x) {ev_function, #x },
#define PRVM_DECLARE_menufieldfloat(x)
#define PRVM_DECLARE_menufieldvector(x)
#define PRVM_DECLARE_menufieldstring(x)
#define PRVM_DECLARE_menufieldedict(x)
#define PRVM_DECLARE_menufieldfunction(x)
#define PRVM_DECLARE_serverfunction(x)
#define PRVM_DECLARE_clientfunction(x)
#define PRVM_DECLARE_menufunction(x)
#define PRVM_DECLARE_field(x)
#define PRVM_DECLARE_global(x)
#define PRVM_DECLARE_function(x)
#include "prvm_offsets.h"
#undef PRVM_DECLARE_serverglobalfloat
#undef PRVM_DECLARE_serverglobalvector
#undef PRVM_DECLARE_serverglobalstring
#undef PRVM_DECLARE_serverglobaledict
#undef PRVM_DECLARE_serverglobalfunction
#undef PRVM_DECLARE_clientglobalfloat
#undef PRVM_DECLARE_clientglobalvector
#undef PRVM_DECLARE_clientglobalstring
#undef PRVM_DECLARE_clientglobaledict
#undef PRVM_DECLARE_clientglobalfunction
#undef PRVM_DECLARE_menuglobalfloat
#undef PRVM_DECLARE_menuglobalvector
#undef PRVM_DECLARE_menuglobalstring
#undef PRVM_DECLARE_menuglobaledict
#undef PRVM_DECLARE_menuglobalfunction
#undef PRVM_DECLARE_serverfieldfloat
#undef PRVM_DECLARE_serverfieldvector
#undef PRVM_DECLARE_serverfieldstring
#undef PRVM_DECLARE_serverfieldedict
#undef PRVM_DECLARE_serverfieldfunction
#undef PRVM_DECLARE_clientfieldfloat
#undef PRVM_DECLARE_clientfieldvector
#undef PRVM_DECLARE_clientfieldstring
#undef PRVM_DECLARE_clientfieldedict
#undef PRVM_DECLARE_clientfieldfunction
#undef PRVM_DECLARE_menufieldfloat
#undef PRVM_DECLARE_menufieldvector
#undef PRVM_DECLARE_menufieldstring
#undef PRVM_DECLARE_menufieldedict
#undef PRVM_DECLARE_menufieldfunction
#undef PRVM_DECLARE_serverfunction
#undef PRVM_DECLARE_clientfunction
#undef PRVM_DECLARE_menufunction
#undef PRVM_DECLARE_field
#undef PRVM_DECLARE_global
#undef PRVM_DECLARE_function
};

#define CL_REQGLOBALS (sizeof(cl_reqglobals) / sizeof(prvm_required_field_t))

prvm_required_field_t cl_reqglobals[] =
{
#define PRVM_DECLARE_serverglobalfloat(x)
#define PRVM_DECLARE_serverglobalvector(x)
#define PRVM_DECLARE_serverglobalstring(x)
#define PRVM_DECLARE_serverglobaledict(x)
#define PRVM_DECLARE_serverglobalfunction(x)
#define PRVM_DECLARE_clientglobalfloat(x) {ev_float, #x},
#define PRVM_DECLARE_clientglobalvector(x) {ev_vector, #x},
#define PRVM_DECLARE_clientglobalstring(x) {ev_string, #x},
#define PRVM_DECLARE_clientglobaledict(x) {ev_entity, #x},
#define PRVM_DECLARE_clientglobalfunction(x) {ev_function, #x},
#define PRVM_DECLARE_menuglobalfloat(x)
#define PRVM_DECLARE_menuglobalvector(x)
#define PRVM_DECLARE_menuglobalstring(x)
#define PRVM_DECLARE_menuglobaledict(x)
#define PRVM_DECLARE_menuglobalfunction(x)
#define PRVM_DECLARE_serverfieldfloat(x)
#define PRVM_DECLARE_serverfieldvector(x)
#define PRVM_DECLARE_serverfieldstring(x)
#define PRVM_DECLARE_serverfieldedict(x)
#define PRVM_DECLARE_serverfieldfunction(x)
#define PRVM_DECLARE_clientfieldfloat(x)
#define PRVM_DECLARE_clientfieldvector(x)
#define PRVM_DECLARE_clientfieldstring(x)
#define PRVM_DECLARE_clientfieldedict(x)
#define PRVM_DECLARE_clientfieldfunction(x)
#define PRVM_DECLARE_menufieldfloat(x)
#define PRVM_DECLARE_menufieldvector(x)
#define PRVM_DECLARE_menufieldstring(x)
#define PRVM_DECLARE_menufieldedict(x)
#define PRVM_DECLARE_menufieldfunction(x)
#define PRVM_DECLARE_serverfunction(x)
#define PRVM_DECLARE_clientfunction(x)
#define PRVM_DECLARE_menufunction(x)
#define PRVM_DECLARE_field(x)
#define PRVM_DECLARE_global(x)
#define PRVM_DECLARE_function(x)
#include "prvm_offsets.h"
#undef PRVM_DECLARE_serverglobalfloat
#undef PRVM_DECLARE_serverglobalvector
#undef PRVM_DECLARE_serverglobalstring
#undef PRVM_DECLARE_serverglobaledict
#undef PRVM_DECLARE_serverglobalfunction
#undef PRVM_DECLARE_clientglobalfloat
#undef PRVM_DECLARE_clientglobalvector
#undef PRVM_DECLARE_clientglobalstring
#undef PRVM_DECLARE_clientglobaledict
#undef PRVM_DECLARE_clientglobalfunction
#undef PRVM_DECLARE_menuglobalfloat
#undef PRVM_DECLARE_menuglobalvector
#undef PRVM_DECLARE_menuglobalstring
#undef PRVM_DECLARE_menuglobaledict
#undef PRVM_DECLARE_menuglobalfunction
#undef PRVM_DECLARE_serverfieldfloat
#undef PRVM_DECLARE_serverfieldvector
#undef PRVM_DECLARE_serverfieldstring
#undef PRVM_DECLARE_serverfieldedict
#undef PRVM_DECLARE_serverfieldfunction
#undef PRVM_DECLARE_clientfieldfloat
#undef PRVM_DECLARE_clientfieldvector
#undef PRVM_DECLARE_clientfieldstring
#undef PRVM_DECLARE_clientfieldedict
#undef PRVM_DECLARE_clientfieldfunction
#undef PRVM_DECLARE_menufieldfloat
#undef PRVM_DECLARE_menufieldvector
#undef PRVM_DECLARE_menufieldstring
#undef PRVM_DECLARE_menufieldedict
#undef PRVM_DECLARE_menufieldfunction
#undef PRVM_DECLARE_serverfunction
#undef PRVM_DECLARE_clientfunction
#undef PRVM_DECLARE_menufunction
#undef PRVM_DECLARE_field
#undef PRVM_DECLARE_global
#undef PRVM_DECLARE_function
};

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
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		PRVM_clientglobalfloat(dmg_take) = dmg_take;
		PRVM_clientglobalfloat(dmg_save) = dmg_save;
		VectorCopy(dmg_origin, PRVM_clientglobalvector(dmg_origin));
		CSQC_END
	}
}

void CSQC_UpdateNetworkTimes(double newtime, double oldtime)
{
	if(!cl.csqc_loaded)
		return;
	CSQC_BEGIN
	PRVM_clientglobalfloat(servertime) = newtime;
	PRVM_clientglobalfloat(serverprevtime) = oldtime;
	PRVM_clientglobalfloat(serverdeltatime) = newtime - oldtime;
	CSQC_END
}

//[515]: set globals before calling R_UpdateView, WEIRD CRAP
void CSQC_R_RecalcView (void);
static void CSQC_SetGlobals (void)
{
	CSQC_BEGIN
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobalfloat(frametime) = max(0, cl.time - cl.oldtime);
		PRVM_clientglobalfloat(servercommandframe) = cls.servermovesequence;
		PRVM_clientglobalfloat(clientcommandframe) = cl.movecmd[0].sequence;
		VectorCopy(cl.viewangles, PRVM_clientglobalvector(input_angles));
		// // FIXME: this actually belongs into getinputstate().. [12/17/2007 Black]
		PRVM_clientglobalfloat(input_buttons) = cl.movecmd[0].buttons;
		VectorSet(PRVM_clientglobalvector(input_movevalues), cl.movecmd[0].forwardmove, cl.movecmd[0].sidemove, cl.movecmd[0].upmove);
		VectorCopy(cl.csqc_vieworiginfromengine, cl.csqc_vieworigin);
		VectorCopy(cl.csqc_viewanglesfromengine, cl.csqc_viewangles);

		// LordHavoc: Spike says not to do this, but without pmove_org the
		// CSQC is useless as it can't alter the view origin without
		// completely replacing it
		Matrix4x4_OriginFromMatrix(&cl.entities[cl.viewentity].render.matrix, PRVM_clientglobalvector(pmove_org));
		VectorCopy(cl.movement_velocity, PRVM_clientglobalvector(pmove_vel));
		PRVM_clientglobalfloat(pmove_onground) = cl.onground;
		PRVM_clientglobalfloat(pmove_inwater) = cl.inwater;

		VectorCopy(cl.viewangles, PRVM_clientglobalvector(view_angles));
		VectorCopy(cl.punchangle, PRVM_clientglobalvector(view_punchangle));
		VectorCopy(cl.punchvector, PRVM_clientglobalvector(view_punchvector));
		PRVM_clientglobalfloat(maxclients) = cl.maxclients;

		CSQC_R_RecalcView();
	CSQC_END
}

void CSQC_Predraw (prvm_edict_t *ed)
{
	int b;
	if(!PRVM_clientedictfunction(ed, predraw))
		return;
	b = PRVM_clientglobaledict(self);
	PRVM_clientglobaledict(self) = PRVM_EDICT_TO_PROG(ed);
	PRVM_ExecuteProgram(PRVM_clientedictfunction(ed, predraw), "CSQC_Predraw: NULL function\n");
	PRVM_clientglobaledict(self) = b;
}

void CSQC_Think (prvm_edict_t *ed)
{
	int b;
	if(PRVM_clientedictfunction(ed, think))
	if(PRVM_clientedictfloat(ed, nextthink) && PRVM_clientedictfloat(ed, nextthink) <= PRVM_clientglobalfloat(time))
	{
		PRVM_clientedictfloat(ed, nextthink) = 0;
		b = PRVM_clientglobaledict(self);
		PRVM_clientglobaledict(self) = PRVM_EDICT_TO_PROG(ed);
		PRVM_ExecuteProgram(PRVM_clientedictfunction(ed, think), "CSQC_Think: NULL function\n");
		PRVM_clientglobaledict(self) = b;
	}
}

extern cvar_t cl_noplayershadow;
extern cvar_t r_equalize_entities_fullbright;
qboolean CSQC_AddRenderEdict(prvm_edict_t *ed, int edictnum)
{
	int renderflags;
	int c;
	float scale;
	entity_render_t *entrender;
	dp_model_t *model;

	model = CL_GetModelFromEdict(ed);
	if (!model)
		return false;

	if (edictnum)
	{
		if (r_refdef.scene.numentities >= r_refdef.scene.maxentities)
			return false;
		entrender = cl.csqcrenderentities + edictnum;
		r_refdef.scene.entities[r_refdef.scene.numentities++] = entrender;
		entrender->entitynumber = edictnum + MAX_EDICTS;
		//entrender->shadertime = 0; // shadertime was set by spawn()
		entrender->flags = 0;
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

	entrender->userwavefunc_param[0] = PRVM_clientedictfloat(ed, userwavefunc_param0);
	entrender->userwavefunc_param[1] = PRVM_clientedictfloat(ed, userwavefunc_param1);
	entrender->userwavefunc_param[2] = PRVM_clientedictfloat(ed, userwavefunc_param2);
	entrender->userwavefunc_param[3] = PRVM_clientedictfloat(ed, userwavefunc_param3);

	entrender->model = model;
	entrender->skinnum = (int)PRVM_clientedictfloat(ed, skin);
	entrender->effects |= entrender->model->effects;
	renderflags = (int)PRVM_clientedictfloat(ed, renderflags);
	entrender->alpha = PRVM_clientedictfloat(ed, alpha);
	entrender->scale = scale = PRVM_clientedictfloat(ed, scale);
	VectorCopy(PRVM_clientedictvector(ed, colormod), entrender->colormod);
	VectorCopy(PRVM_clientedictvector(ed, glowmod), entrender->glowmod);
	if(PRVM_clientedictfloat(ed, effects))	entrender->effects |= (int)PRVM_clientedictfloat(ed, effects);
	if (!entrender->alpha)
		entrender->alpha = 1.0f;
	if (!entrender->scale)
		entrender->scale = scale = 1.0f;
	if (!VectorLength2(entrender->colormod))
		VectorSet(entrender->colormod, 1, 1, 1);
	if (!VectorLength2(entrender->glowmod))
		VectorSet(entrender->glowmod, 1, 1, 1);

	// LordHavoc: use the CL_GetTagMatrix function on self to ensure consistent behavior (duplicate code would be bad)
	CL_GetTagMatrix(&entrender->matrix, ed, 0);

	// set up the animation data
	VM_GenerateFrameGroupBlend(ed->priv.server->framegroupblend, ed);
	VM_FrameBlendFromFrameGroupBlend(ed->priv.server->frameblend, ed->priv.server->framegroupblend, model);
	VM_UpdateEdictSkeleton(ed, model, ed->priv.server->frameblend);
	if (PRVM_clientedictfloat(ed, shadertime)) // hack for csprogs.dat files that do not set shadertime, leaves the value at entity spawn time
		entrender->shadertime = PRVM_clientedictfloat(ed, shadertime);

	// transparent offset
	if (renderflags & RF_USETRANSPARENTOFFSET)
		entrender->transparent_offset = PRVM_clientglobalfloat(transparent_offset);

	if(renderflags)
	{
		if(renderflags & RF_VIEWMODEL)	entrender->flags |= RENDER_VIEWMODEL | RENDER_NODEPTHTEST;
		if(renderflags & RF_EXTERNALMODEL)entrender->flags |= RENDER_EXTERIORMODEL;
		if(renderflags & RF_NOCULL)		entrender->flags |= RENDER_NOCULL;
		if(renderflags & RF_DEPTHHACK)	entrender->flags |= RENDER_NODEPTHTEST;
		if(renderflags & RF_ADDITIVE)		entrender->flags |= RENDER_ADDITIVE;
	}

	c = (int)PRVM_clientedictfloat(ed, colormap);
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
		if (!(entrender->effects & EF_FULLBRIGHT) && !(renderflags & RF_FULLBRIGHT))
			entrender->flags |= RENDER_LIGHT;
		else if(r_equalize_entities_fullbright.integer)
			entrender->flags |= RENDER_LIGHT | RENDER_EQUALIZE;
	}
	// hide player shadow during intermission or nehahra movie
	if (!(entrender->effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST))
	 &&  (entrender->alpha >= 1)
	 && !(renderflags & RF_NOSHADOW)
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
		if (!PRVM_clientfunction(CSQC_InputEvent))
			r = false;
		else
		{
			PRVM_clientglobalfloat(time) = cl.time;
			PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
			PRVM_G_FLOAT(OFS_PARM0) = !down; // 0 is down, 1 is up
			PRVM_G_FLOAT(OFS_PARM1) = key;
			PRVM_G_FLOAT(OFS_PARM2) = ascii;
			PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_InputEvent), "QC function CSQC_InputEvent is missing");
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
	R_TimeReport("pre-UpdateView");
	CSQC_BEGIN
		//VectorCopy(cl.viewangles, oldangles);
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		CSQC_SetGlobals();
		// clear renderable entity and light lists to prevent crashes if the
		// CSQC_UpdateView function does not call R_ClearScene as it should
		r_refdef.scene.numentities = 0;
		r_refdef.scene.numlights = 0;
		// pass in width and height as parameters (EXT_CSQC_1)
		PRVM_G_FLOAT(OFS_PARM0) = vid.width;
		PRVM_G_FLOAT(OFS_PARM1) = vid.height;
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_UpdateView), "QC function CSQC_UpdateView is missing");
		//VectorCopy(oldangles, cl.viewangles);
		// Dresk : Reset Dmg Globals Here
		CL_VM_UpdateDmgGlobals(0, 0, emptyvector);
	CSQC_END
	R_TimeReport("UpdateView");
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
	if (PRVM_clientfunction(CSQC_ConsoleCommand))
	{
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(cmd);
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_ConsoleCommand), "QC function CSQC_ConsoleCommand is missing");
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
	if(PRVM_clientfunction(CSQC_Parse_TempEntity))
	{
		t = msg_readcount;
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Parse_TempEntity), "QC function CSQC_Parse_TempEntity is missing");
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
	if(PRVM_clientfunction(CSQC_Parse_StuffCmd))
	{
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Parse_StuffCmd), "QC function CSQC_Parse_StuffCmd is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}
	else
		Cbuf_AddText(msg);
	CSQC_END
}

static void CL_VM_Parse_Print (const char *msg)
{
	int restorevm_tempstringsbuf_cursize;
	PRVM_clientglobalfloat(time) = cl.time;
	PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
	restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
	PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
	PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Parse_Print), "QC function CSQC_Parse_Print is missing");
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
	if(PRVM_clientfunction(CSQC_Parse_Print))
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
	if(PRVM_clientfunction(CSQC_Parse_CenterPrint))
	{
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(msg);
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Parse_CenterPrint), "QC function CSQC_Parse_CenterPrint is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}
	else
		SCR_CenterPrint(msg);
	CSQC_END
}

void CL_VM_UpdateIntermissionState (int intermission)
{
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		PRVM_clientglobalfloat(intermission) = intermission;
		CSQC_END
	}
}
void CL_VM_UpdateShowingScoresState (int showingscores)
{
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		PRVM_clientglobalfloat(sb_showscores) = showingscores;
		CSQC_END
	}
}
qboolean CL_VM_Event_Sound(int sound_num, float volume, int channel, float attenuation, int ent, vec3_t pos)
{
	qboolean r = false;
	if(cl.csqc_loaded)
	{
		CSQC_BEGIN
		if(PRVM_clientfunction(CSQC_Event_Sound))
		{
			PRVM_clientglobalfloat(time) = cl.time;
			PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
			PRVM_G_FLOAT(OFS_PARM0) = ent;
			PRVM_G_FLOAT(OFS_PARM1) = CHAN_ENGINE2USER(channel);
			PRVM_G_INT(OFS_PARM2) = PRVM_SetTempString(cl.sound_name[sound_num] );
			PRVM_G_FLOAT(OFS_PARM3) = volume;
			PRVM_G_FLOAT(OFS_PARM4) = attenuation;
			VectorCopy(pos, PRVM_G_VECTOR(OFS_PARM5) );
			PRVM_G_FLOAT(OFS_PARM6) = 0; // pitch shift not supported yet
			PRVM_G_FLOAT(OFS_PARM7) = 0; // flags - none can come in at this point yet
			PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Event_Sound), "QC function CSQC_Event_Sound is missing");
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
		PRVM_clientglobalfloat(coop) = localcoop;
		PRVM_clientglobalfloat(deathmatch) = localdeathmatch;
		CSQC_END
	}
}
float CL_VM_Event (float event)		//[515]: needed ? I'd say "YES", but don't know for what :D
{
	float r = 0;
	if(!cl.csqc_loaded)
		return 0;
	CSQC_BEGIN
	if(PRVM_clientfunction(CSQC_Event))
	{
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[cl.playerentity];
		PRVM_G_FLOAT(OFS_PARM0) = event;
		PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Event), "QC function CSQC_Event is missing");
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
		PRVM_clientglobalfloat(time) = cl.time;
		oldself = PRVM_clientglobaledict(self);
		while(1)
		{
			entnum = MSG_ReadShort();
			if(!entnum || msg_badread)
				break;
			realentnum = entnum & 0x7FFF;
			PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[realentnum];
			if(entnum & 0x8000)
			{
				if(PRVM_clientglobaledict(self))
				{
					PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Ent_Remove), "QC function CSQC_Ent_Remove is missing");
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
				if(!PRVM_clientglobaledict(self))
				{
					if(!PRVM_clientfunction(CSQC_Ent_Spawn))
					{
						prvm_edict_t	*ed;
						ed = PRVM_ED_Alloc();
						PRVM_clientedictfloat(ed, entnum) = realentnum;
						PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[realentnum] = PRVM_EDICT_TO_PROG(ed);
					}
					else
					{
						// entity( float entnum ) CSQC_Ent_Spawn;
						// the qc function should set entnum, too (this way it also can return world [2/1/2008 Andreas]
						PRVM_G_FLOAT(OFS_PARM0) = (float) realentnum;
						// make sure no one gets wrong ideas
						PRVM_clientglobaledict(self) = 0;
						PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Ent_Spawn), "QC function CSQC_Ent_Spawn is missing");
						PRVM_clientglobaledict(self) = cl.csqc_server2csqcentitynumber[realentnum] = PRVM_EDICT( PRVM_G_INT( OFS_RETURN ) );
					}
					PRVM_G_FLOAT(OFS_PARM0) = 1;
					PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Ent_Update), "QC function CSQC_Ent_Update is missing");
				}
				else {
					PRVM_G_FLOAT(OFS_PARM0) = 0;
					PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Ent_Update), "QC function CSQC_Ent_Update is missing");
				}
			}
		}
		PRVM_clientglobaledict(self) = oldself;
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
	for (i = 1, ent = prog->edicts;i < prog->num_edicts;i++, ent++)
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
	memset(ed->fields.vp, 0, prog->entityfields * 4);
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
		if (PRVM_clientedictfloat(ent, solid))
			solid++;
		if (PRVM_clientedictstring(ent, model))
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
qboolean MakeDownloadPacket(const char *filename, unsigned char *data, size_t len, int crc, int cnt, sizebuf_t *buf, int protocol)
{
	int packetsize = buf->maxsize - 7; // byte short long
	int npackets = (len + packetsize - 1) / (packetsize);

	if(protocol == PROTOCOL_QUAKEWORLD)
		return false; // CSQC can't run in QW anyway

	SZ_Clear(buf);
	if(cnt == 0)
	{
		MSG_WriteByte(buf, svc_stufftext);
		MSG_WriteString(buf, va("\ncl_downloadbegin %lu %s\n", (unsigned long)len, filename));
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
		MSG_WriteString(buf, va("\ncl_downloadfinished %lu %d\n", (unsigned long)len, crc));
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
		csprogsdatacrc = CRC_Block(csprogsdata, (size_t)csprogsdatasize);
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

	PRVM_LoadProgs(csprogsfn, cl_numrequiredfunc, cl_required_func, CL_REQFIELDS, cl_reqfields, CL_REQGLOBALS, cl_reqglobals);

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
			static char buf[NET_MAXMESSAGE];
			sizebuf_t sb;
			unsigned char *demobuf; fs_offset_t demofilesize;

			sb.data = (unsigned char *) buf;
			sb.maxsize = sizeof(buf);
			i = 0;

			CL_CutDemo(&demobuf, &demofilesize);
			while(MakeDownloadPacket(csqc_progname.string, csprogsdata, (size_t)csprogsdatasize, csprogsdatacrc, i++, &sb, cls.protocol))
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
	PRVM_clientglobalfloat(time) = cl.time;
	PRVM_clientglobaledict(self) = 0;

	PRVM_clientglobalstring(mapname) = PRVM_SetEngineString(cl.worldname);
	PRVM_clientglobalfloat(player_localentnum) = cl.playerentity;

	// set map description (use world entity 0)
	PRVM_clientedictstring(prog->edicts, message) = PRVM_SetEngineString(cl.worldmessage);
	VectorCopy(cl.world.mins, PRVM_clientedictvector(prog->edicts, mins));
	VectorCopy(cl.world.maxs, PRVM_clientedictvector(prog->edicts, maxs));

	// call the prog init
	PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Init), "QC function CSQC_Init is missing");

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
		PRVM_clientglobalfloat(time) = cl.time;
		PRVM_clientglobaledict(self) = 0;
		if (PRVM_clientfunction(CSQC_Shutdown))
			PRVM_ExecuteProgram(PRVM_clientfunction(CSQC_Shutdown), "QC function CSQC_Shutdown is missing");
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
		VectorCopy(PRVM_clientedictvector(ed, origin), out);
		if(CL_GetTagMatrix (&matrix, ed, 0) == 0)
			Matrix4x4_OriginFromMatrix(&matrix, out);
		if (mod && mod->soundfromcenter)
			VectorMAMAM(1.0f, out, 0.5f, mod->normalmins, 0.5f, mod->normalmaxs, out);
		r = 1;
	}

	CSQC_END;

	return r;
}

qboolean CL_VM_TransformView(int entnum, matrix4x4_t *viewmatrix, mplane_t *clipplane, vec3_t visorigin)
{
	qboolean ret = false;
	prvm_edict_t *ed;
	vec3_t forward, left, up, origin, ang;
	matrix4x4_t mat, matq;

	CSQC_BEGIN
		ed = PRVM_EDICT_NUM(entnum);
		// camera:
		//   camera_transform
		if(PRVM_clientedictfunction(ed, camera_transform))
		{
			ret = true;
			if(viewmatrix || clipplane || visorigin)
			{
				Matrix4x4_ToVectors(viewmatrix, forward, left, up, origin);
				AnglesFromVectors(ang, forward, up, false);
				PRVM_clientglobalfloat(time) = cl.time;
				PRVM_clientglobaledict(self) = entnum;
				VectorCopy(origin, PRVM_G_VECTOR(OFS_PARM0));
				VectorCopy(ang, PRVM_G_VECTOR(OFS_PARM1));
				VectorCopy(forward, PRVM_clientglobalvector(v_forward));
				VectorScale(left, -1, PRVM_clientglobalvector(v_right));
				VectorCopy(up, PRVM_clientglobalvector(v_up));
				VectorCopy(origin, PRVM_clientglobalvector(trace_endpos));
				PRVM_ExecuteProgram(PRVM_clientedictfunction(ed, camera_transform), "QC function e.camera_transform is missing");
				VectorCopy(PRVM_G_VECTOR(OFS_RETURN), origin);
				VectorCopy(PRVM_clientglobalvector(v_forward), forward);
				VectorScale(PRVM_clientglobalvector(v_right), -1, left);
				VectorCopy(PRVM_clientglobalvector(v_up), up);
				VectorCopy(PRVM_clientglobalvector(trace_endpos), visorigin);
				Matrix4x4_Invert_Full(&mat, viewmatrix);
				Matrix4x4_FromVectors(viewmatrix, forward, left, up, origin);
				Matrix4x4_Concat(&matq, viewmatrix, &mat);
				Matrix4x4_TransformPositivePlane(&matq, clipplane->normal[0], clipplane->normal[1], clipplane->normal[2], clipplane->dist, &clipplane->normal[0]);
			}
		}
	CSQC_END

	return ret;
}
