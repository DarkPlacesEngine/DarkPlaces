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

#include "quakedef.h"

cvar_t	sv_aim = {CVAR_SAVE, "sv_aim", "2"}; //"0.93"}; // LordHavoc: disabled autoaim by default

#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))


/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/


char *PF_VarString (int	first)
{
	int		i;
	static char out[256];

	out[0] = 0;
	for (i=first ; i<pr_argc ; i++)
	{
		strcat (out, G_STRING((OFS_PARM0+i*3)));
	}
	return out;
}

char *ENGINE_EXTENSIONS = "\
DP_ENT_ALPHA \
DP_ENT_COLORMOD \
DP_ENT_DELTACOMPRESS \
DP_ENT_GLOW \
DP_ENT_SCALE \
DP_ENT_VIEWMODEL \
DP_GFX_FOG \
DP_HALFLIFE_MAP \
DP_INPUTBUTTONS \
DP_MONSTERWALK \
DP_MOVETYPEFOLLOW \
DP_SOLIDCORPSE \
DP_REGISTERCVAR \
DP_SPRITE32 \
DP_SV_DRAWONLYTOCLIENT \
DP_SV_NODRAWTOCLIENT \
DP_SV_EXTERIORMODELTOCLIENT \
DP_SV_SETCOLOR \
DP_SV_EFFECT \
DP_TE_BLOOD \
DP_TE_BLOODSHOWER \
DP_TE_EXPLOSIONRGB \
DP_TE_PARTICLECUBE \
DP_TE_PARTICLERAIN \
DP_TE_PARTICLESNOW \
DP_TE_SPARK \
DP_QC_CHANGEPITCH \
DP_QC_COPYENTITY \
DP_QC_ETOS \
DP_QC_FINDFLOAT \
DP_QC_FINDCHAIN \
DP_QC_FINDCHAINFLOAT \
DP_QC_GETLIGHT \
DP_QC_SINCOSSQRTPOW \
DP_QC_MINMAXBOUND \
DP_QC_RANDOMVEC \
DP_QC_TRACEBOX \
DP_QC_TRACETOSS \
DP_QUAKE2_MODEL \
NEH_CMD_PLAY2 \
";

qboolean checkextension(char *name)
{
	int len;
	char *e, *start;
	len = strlen(name);
	for (e = ENGINE_EXTENSIONS;*e;e++)
	{
		while (*e == ' ')
			e++;
		if (!*e)
			break;
		start = e;
		while (*e && *e != ' ')
			e++;
		if (e - start == len)
			if (!strncasecmp(e, name, len))
				return true;
	}
	return false;
}

/*
=================
PF_checkextension

returns true if the extension is supported by the server

checkextension(extensionname)
=================
*/
void PF_checkextension (void)
{
	G_FLOAT(OFS_RETURN) = checkextension(G_STRING(OFS_PARM0));
}

/*
=================
PF_error

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error (void)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n", pr_strings + pr_xfunction->s_name, s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (void)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", pr_strings + pr_xfunction->s_name, s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

// LordHavoc: bug fix - no longer kills server
//	Host_Error ("Program error");
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
==============
PF_vectorvectors

Writes new values for v_forward, v_up, and v_right based on the given forward vector
vectorvectors(vector, vector)
==============
*/
void PF_vectorvectors (void)
{
	VectorNormalize2(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward);
	VectorVectors(pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;
	
	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}


void SetMinMaxSize (edict_t *e, float *min, float *max, qboolean rotate)
{
	/*
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;
	*/
	int		i;
	
	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PR_RunError ("backwards mins/maxs");

	/*
	rotate = false;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy (min, rmin);
		VectorCopy (max, rmax);
	}
	else
	{
	// find min / max for rotations
		angles = e->v.angles;
		
		a = angles[1]/180 * M_PI;
		
		xvector[0] = cos(a);
		xvector[1] = sin(a);
		yvector[0] = -sin(a);
		yvector[1] = cos(a);
		
		VectorCopy (min, bounds[0]);
		VectorCopy (max, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;
		
		for (i=0 ; i<= 1 ; i++)
		{
			base[0] = bounds[i][0];
			for (j=0 ; j<= 1 ; j++)
			{
				base[1] = bounds[j][1];
				for (k=0 ; k<= 1 ; k++)
				{
					base[2] = bounds[k][2];
					
				// transform the point
					transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
					transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
					transformed[2] = base[2];
					
					for (l=0 ; l<3 ; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}
	
// set derived values
	VectorCopy (rmin, e->v.mins);
	VectorCopy (rmax, e->v.maxs);
	VectorSubtract (max, min, e->v.size);
	*/
	
// set derived values
	VectorCopy (min, e->v.mins);
	VectorCopy (max, e->v.maxs);
	VectorSubtract (max, min, e->v.size);

	SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle
LordHavoc: no it isn't...

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
	edict_t	*e;
	float	*min, *max;
	
	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	SetMinMaxSize (e, min, max, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel (void)
{
	edict_t	*e;
	char	*m, **check;
	model_t	*mod;
	int		i;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
	for (i=0, check = sv.model_precache ; *check ; i++, check++)
		if (!strcmp(*check, m))
			break;

	if (!*check)
		PR_RunError ("no precache: %s\n", m);


	e->v.model = m - pr_strings;
	e->v.modelindex = i; //SV_ModelIndex (m);

	mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, true);

	if (mod)
	/*
	{ // LordHavoc: corrected model bounding box, but for compatibility that means I have to break it here
		vec3_t min, max;
		if (mod->type == ALIASTYPE_MDL)
		{
			min[0] = min[1] = min[2] = -16;
			max[0] = max[1] = max[2] = 16;
			SetMinMaxSize (e, min, max, true);
		}
		else
			SetMinMaxSize (e, mod->mins, mod->maxs, true);
	}
	*/
		SetMinMaxSize (e, mod->normalmins, mod->normalmaxs, true);
	else
		SetMinMaxSize (e, vec3_origin, vec3_origin, true);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
void PF_bprint (void)
{
	char		*s;

	s = PF_VarString(0);
	SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);
	
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}
		
	client = &svs.clients[entnum-1];
		
	MSG_WriteChar (&client->message,svc_print);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);
	
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}
		
	client = &svs.clients[entnum-1];
		
	MSG_WriteChar (&client->message,svc_centerprint);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);
	
	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1/new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}
	
	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));	
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
	float	*value1;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);
	
	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		// LordHavoc: optimized a bit
		if (value1[0])
		{
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
		}
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		forward = sqrt(value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (void)
{
	float		num;
		
	num = (rand ()&0x7fff) / ((float)0x7fff);
	
	G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (void)
{
	float		*org, *dir;
	float		color;
	float		count;
			
	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
	char		**check;
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum, large;

	pos = G_VECTOR (OFS_PARM0);			
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	
// check to see if samp was properly precached
	for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
		if (!strcmp(*check,samp))
			break;
			
	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

	large = false;
	if (soundnum >= 256)
		large = true;

	// add an svc_spawnambient command to the level signon packet

	if (large)
		MSG_WriteByte (&sv.signon, svc_spawnstaticsound2);
	else
		MSG_WriteByte (&sv.signon, svc_spawnstaticsound);

	for (i=0 ; i<3 ; i++)
		MSG_WriteFloatCoord(&sv.signon, pos[i]);

	if (large)
		MSG_WriteShort (&sv.signon, soundnum);
	else
		MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;
		
	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	
	if (volume < 0 || volume > 255)
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
void PF_break (void)
{
//	Con_Printf ("break statement\n");
//	*(int *)-4 = 0;	// dump to debugger
	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters ? MOVE_NOMONSTERS : MOVE_NORMAL, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
	// FIXME: add trace_endcontents
}


/*
=================
PF_tracebox

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

tracebox (vector1, vector mins, vector maxs, vector2, tryents)
=================
*/
// LordHavoc: added this for my own use, VERY useful, similar to traceline
void PF_tracebox (void)
{
	float	*v1, *v2, *m1, *m2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	m1 = G_VECTOR(OFS_PARM1);
	m2 = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(OFS_PARM5);

	trace = SV_Move (v1, m1, m2, v2, nomonsters ? MOVE_NOMONSTERS : MOVE_NORMAL, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);
void PF_TraceToss (void)
{
	trace_t	trace;
	edict_t	*ent;
	edict_t	*ignore;

	ent = G_EDICT(OFS_PARM0);
	ignore = G_EDICT(OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}


/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (void)
{
}

//============================================================================

byte	checkpvs[MAX_MAP_LEAFS/8];

int PF_newcheckclient (int check)
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == svs.maxclients+1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	memcpy (checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there is more than one valid option, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
int c_invis, c_notvis;
void PF_checkclient (void)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;
	
// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible	
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
c_notvis++;
		RETURN_EDICT(sv.edicts);
		return;
	}

// might be able to see it
c_invis++;
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
	int		entnum;
	char	*str;
	client_t	*old;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING(OFS_PARM1);	
	
	old = host_client;
	host_client = &svs.clients[entnum-1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd (void)
{
	char	*str;
	
	str = G_STRING(OFS_PARM0);	
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
	char	*str;
	
	str = G_STRING(OFS_PARM0);
	
	G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (void)
{
	char	*var, *val;
	
	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);
	
	Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (void)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)sv.edicts;
	
	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);			
		if (Length(eorg) > rad)
			continue;
			
		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
	Con_DPrintf ("%s",PF_VarString(0));
}

char	pr_string_temp[128];

void PF_ftos (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);

	// LordHavoc: ftos improvement
	sprintf (pr_string_temp, "%g", v);
	/*
	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else
		sprintf (pr_string_temp, "%5.1f",v);
	*/
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos (void)
{
	sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_etos (void)
{
	sprintf (pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_Spawn (void)
{
	edict_t	*ed;
	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

void PF_Remove (void)
{
	edict_t	*ed;

	ed = G_EDICT(OFS_PARM0);
	if (ed == sv.edicts)
		PR_RunError("remove: tried to remove world\n");
	if (NUM_FOR_EDICT(ed) <= svs.maxclients)
		PR_RunError("remove: tried to remove a client\n");
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
{
	int		e;	
	int		f;
	char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s || !s[0])
	{
		RETURN_EDICT(sv.edicts);
		return;
	}
		
	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// LordHavoc: added this for searching float, int, and entity reference fields
void PF_FindFloat (void)
{
	int		e;	
	int		f;
	float	s;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);
		
	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		if (E_FLOAT(ed,f) == s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void PF_findchain (void)
{
	int		i;	
	int		f;
	char	*s, *t;
	edict_t	*ent, *chain;

	chain = (edict_t *)sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_STRING(OFS_PARM1);
	if (!s || !s[0])
	{
		RETURN_EDICT(sv.edicts);
		return;
	}
		
	ent = NEXT_EDICT(sv.edicts);
	for (i = 1;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		t = E_STRING(ent,f);
		if (!t)
			continue;
		if (strcmp(t,s))
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

// LordHavoc: chained search for float, int, and entity reference fields
// entity(.string field, float match) findchainfloat = #403;
void PF_findchainfloat (void)
{
	int		i;	
	int		f;
	float	s;
	edict_t	*ent, *chain;

	chain = (edict_t *)sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_FLOAT(OFS_PARM1);
		
	ent = NEXT_EDICT(sv.edicts);
	for (i = 1;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		if (E_FLOAT(ent,f) != s)
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);
	
	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

void PF_precache_model (void)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");
		
	s = G_STRING(OFS_PARM0);
	if (hlbsp && ((!s) || (!s[0])))
		return;
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, true);
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}


void PF_coredump (void)
{
	ED_PrintEdicts ();
}

void PF_traceon (void)
{
	pr_trace = true;
}

void PF_traceoff (void)
{
	pr_trace = false;
}

void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int 	oldself;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	
	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;
	
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;
	
	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);
	
	
// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;
	
	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;
	
	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;
	
// send message to all clients on this server
	if (sv.state != ss_active)
		return;
	
	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
		if (client->active || client->spawned)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message,style);
			MSG_WriteString (&client->message, val);
		}
}

void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	G_FLOAT(OFS_RETURN) = SV_CheckBottom (G_EDICT(OFS_PARM0));
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	G_FLOAT(OFS_RETURN) = SV_PointContents (G_VECTOR(OFS_PARM0));
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (void)
{
	int		i;
	edict_t	*ent;
	
	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim (void)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;
	
	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;
	
	check = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, check = NEXT_EDICT(check) )
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v.origin[j]
			+ 0.5*(check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}
	
	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));	
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = ANGLEMOD(ent->v.angles[1]);
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;
	
	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	
	ent->v.angles[1] = ANGLEMOD (current + move);
}

/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	eval_t		*val;
	
	ent = G_EDICT(OFS_PARM0);
	current = ANGLEMOD( ent->v.angles[0] );
	if ((val = GETEDICTFIELDVALUE(ent, eval_idealpitch)))
		ideal = val->_float;
	else
	{
		PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
		return;
	}
	if ((val = GETEDICTFIELDVALUE(ent, eval_pitch_speed)))
		speed = val->_float;
	else
	{
		PR_RunError ("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch");
		return;
	}
	
	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	
	ent->v.angles[0] = ANGLEMOD (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *WriteDest (void)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	
	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError ("WriteDest: not a client");
		return &svs.clients[entnum-1].message;
		
	case MSG_ALL:
		return &sv.reliable_datagram;
	
	case MSG_INIT:
		return &sv.signon;

	default:
		PR_RunError ("WriteDest: bad destination");
		break;
	}
	
	return NULL;
}

void PF_WriteByte (void)
{
	MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord (void)
{
	MSG_WriteFloatCoord (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString (void)
{
	MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (void)
{
	edict_t	*ent;
	int		i, large;
	
	ent = G_EDICT(OFS_PARM0);

	large = false;
	if (ent->v.modelindex >= 256 || ent->v.frame >= 256)
		large = true;

	if (large)
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic2);
		MSG_WriteShort (&sv.signon, ent->v.modelindex);
		MSG_WriteShort (&sv.signon, ent->v.frame);
	}
	else
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic);
		MSG_WriteByte (&sv.signon, ent->v.modelindex);
		MSG_WriteByte (&sv.signon, ent->v.frame);
	}

	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteFloatCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
	char	*s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;
	
	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}

void PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}

void PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}

void PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}

/*
=================
PF_RandomVec

Returns a vector of length < 1

randomvec()
=================
*/
void PF_randomvec (void)
{
	vec3_t		temp;
	do
	{
		temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	}
	while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));	
}

void SV_LightPoint (vec3_t color, vec3_t p);
/*
=================
PF_GetLight

Returns a color vector indicating the lighting at the requested point.

(Internal Operation note: actually measures the light beneath the point, just like
                          the model lighting on the client)

getlight(vector)
=================
*/
void PF_GetLight (void)
{
	vec3_t		color;
	vec_t*		p;
	p = G_VECTOR(OFS_PARM0);
	SV_LightPoint (color, p);
	VectorCopy (color, G_VECTOR(OFS_RETURN));	
}

#define MAX_QC_CVARS 128
cvar_t qc_cvar[MAX_QC_CVARS];
int currentqc_cvar;

void PF_registercvar (void)
{
	char	*name, *value;
	cvar_t	*variable;
	name = G_STRING(OFS_PARM1);
	value = G_STRING(OFS_PARM2);
	G_FLOAT(OFS_RETURN) = 0;
// first check to see if it has already been defined
	if (Cvar_FindVar (name))
		return;
	
// check for overlap with a command
	if (Cmd_Exists (name))
	{
		Con_Printf ("PF_registercvar: %s is a command\n", name);
		return;
	}

	if (currentqc_cvar >= MAX_QC_CVARS)
		PR_RunError ("PF_registercvar: ran out of cvar slots (%i)\n", MAX_QC_CVARS);

// copy the name and value
	variable = &qc_cvar[currentqc_cvar++];
	variable->name = Z_Malloc (strlen(name)+1);	
	strcpy (variable->name, name);
	variable->string = Z_Malloc (strlen(value)+1);	
	strcpy (variable->string, value);
	variable->value = atof (value);
	
// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
	G_FLOAT(OFS_RETURN) = 1; // success
}

/*
=================
PF_min

returns the minimum of two supplied floats

min(a, b)
=================
*/
void PF_min (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) < f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_RunError("min: must supply at least 2 floats\n");
}

/*
=================
PF_max

returns the maximum of two supplied floats

max(a, b)
=================
*/
void PF_max (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) > f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		PR_RunError("max: must supply at least 2 floats\n");
}

/*
=================
PF_bound

returns number bounded by supplied range

min(min, value, max)
=================
*/
void PF_bound (void)
{
	G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

/*
=================
PF_pow

returns a raised to power b

pow(a, b)
=================
*/
void PF_pow (void)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

/*
=================
PF_copyentity

copies data from one entity to another

copyentity(src, dst)
=================
*/
void PF_copyentity (void)
{
	edict_t *in, *out;
	in = G_EDICT(OFS_PARM0);
	out = G_EDICT(OFS_PARM1);
	memcpy(out, in, pr_edict_size);
}

/*
=================
PF_setcolor

sets the color of a client and broadcasts the update to all connected clients

setcolor(clientent, value)
=================
*/
void PF_setcolor (void)
{
	client_t	*client;
	int			entnum, i;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	i = G_FLOAT(OFS_PARM1);
	
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to setcolor a non-client\n");
		return;
	}
		
	client = &svs.clients[entnum-1];
	client->colors = i;
	client->edict->v.team = (i & 15) + 1;
		
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, entnum - 1);
	MSG_WriteByte (&sv.reliable_datagram, i);
}

/*
=================
PF_effect

effect(origin, modelname, startframe, framecount, framerate)
=================
*/
void PF_effect (void)
{
	char *s;
	s = G_STRING(OFS_PARM1);
	if (!s || !s[0])
		PR_RunError("effect: no model specified\n");

	SV_StartEffect(G_VECTOR(OFS_PARM0), SV_ModelIndex(s), G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

void PF_te_blood (void)
{
	if (G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOOD);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) G_FLOAT(OFS_PARM2), 255));
}

void PF_te_bloodshower (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOODSHOWER);
	// min
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// max
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// speed
	MSG_WriteFloatCoord(&sv.datagram, G_FLOAT(OFS_PARM2));
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
}

void PF_te_explosionrgb (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONRGB);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// color
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[0] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[1] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[2] * 255), 255));
}

void PF_te_particlecube (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLECUBE);
	// min
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// max
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// velocity
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
	// gravity true/false
	MSG_WriteByte(&sv.datagram, ((int) G_FLOAT(OFS_PARM5)) != 0);
	// randomvel
	MSG_WriteFloatCoord(&sv.datagram, G_FLOAT(OFS_PARM6));
}

void PF_te_particlerain (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLERAIN);
	// min
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// max
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// velocity
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
}

void PF_te_particlesnow (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLESNOW);
	// min
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// max
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// velocity
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
}

void PF_te_spark (void)
{
	if (G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPARK);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) G_FLOAT(OFS_PARM2), 255));
}

void PF_te_gunshotquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOTQUAD);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_spikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKEQUAD);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_superspikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKEQUAD);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_explosionquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONQUAD);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_smallflash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SMALLFLASH);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_customflash (void)
{
	if (G_FLOAT(OFS_PARM1) < 8 || G_FLOAT(OFS_PARM2) < (1.0 / 256.0))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_CUSTOMFLASH);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// radius
	MSG_WriteByte(&sv.datagram, bound(0, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.datagram, bound(0, G_FLOAT(OFS_PARM2) / 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));
}

void PF_te_gunshot (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOT);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_spike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKE);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_superspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKE);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_explosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_tarexplosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TAREXPLOSION);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_wizspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_WIZSPIKE);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_knightspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_KNIGHTSPIKE);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_lavasplash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LAVASPLASH);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_teleport (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TELEPORT);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
}

void PF_te_explosion2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION2);
	// origin
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2]);
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM1));
}

void PF_te_lightning1 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING1);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// end
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
}

void PF_te_lightning2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING2);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// end
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
}

void PF_te_lightning3 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING3);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// end
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
}

void PF_te_beam (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BEAM);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2]);
	// end
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1]);
	MSG_WriteFloatCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2]);
}

void PF_Fixme (void)
{
	PR_RunError ("unimplemented builtin"); // LordHavoc: was misspelled (bulitin)
}



builtin_t pr_builtin[] =
{
PF_Fixme,
PF_makevectors,	// void(entity e)	makevectors 		= #1;
PF_setorigin,	// void(entity e, vector o) setorigin	= #2;
PF_setmodel,	// void(entity e, string m) setmodel	= #3;
PF_setsize,	// void(entity e, vector min, vector max) setsize = #4;
PF_Fixme,	// void(entity e, vector min, vector max) setabssize = #5;
PF_break,	// void() break						= #6;
PF_random,	// float() random						= #7;
PF_sound,	// void(entity e, float chan, string samp) sound = #8;
PF_normalize,	// vector(vector v) normalize			= #9;
PF_error,	// void(string e) error				= #10;
PF_objerror,	// void(string e) objerror				= #11;
PF_vlen,	// float(vector v) vlen				= #12;
PF_vectoyaw,	// float(vector v) vectoyaw		= #13;
PF_Spawn,	// entity() spawn						= #14;
PF_Remove,	// void(entity e) remove				= #15;
PF_traceline,	// float(vector v1, vector v2, float tryents) traceline = #16;
PF_checkclient,	// entity() clientlist					= #17;
PF_Find,	// entity(entity start, .string fld, string match) find = #18;
PF_precache_sound,	// void(string s) precache_sound		= #19;
PF_precache_model,	// void(string s) precache_model		= #20;
PF_stuffcmd,	// void(entity client, string s)stuffcmd = #21;
PF_findradius,	// entity(vector org, float rad) findradius = #22;
PF_bprint,	// void(string s) bprint				= #23;
PF_sprint,	// void(entity client, string s) sprint = #24;
PF_dprint,	// void(string s) dprint				= #25;
PF_ftos,	// void(string s) ftos				= #26;
PF_vtos,	// void(string s) vtos				= #27;
PF_coredump,
PF_traceon,
PF_traceoff,
PF_eprint,	// void(entity e) debug print an entire entity
PF_walkmove, // float(float yaw, float dist) walkmove
PF_Fixme, // float(float yaw, float dist) walkmove
PF_droptofloor,
PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
PF_checkbottom,
PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_aim,
PF_cvar,
PF_localcmd,
PF_nextent,
PF_particle,
PF_changeyaw,
PF_Fixme,
PF_vectoangles,

PF_WriteByte,
PF_WriteChar,
PF_WriteShort,
PF_WriteLong,
PF_WriteCoord,
PF_WriteAngle,
PF_WriteString,
PF_WriteEntity,

PF_sin,
PF_cos,
PF_sqrt,
PF_changepitch,
PF_TraceToss,
PF_etos,
PF_Fixme,

SV_MoveToGoal,
PF_precache_file,
PF_makestatic,

PF_changelevel,
PF_Fixme,

PF_cvar_set,
PF_centerprint,

PF_ambientsound,

PF_precache_model,
PF_precache_sound,		// precache_sound2 is different only for qcc
PF_precache_file,

PF_setspawnparms,

PF_Fixme,				// #79 LordHavoc: dunno who owns 79-89, so these are just padding
PF_Fixme,				// #80 
PF_Fixme,				// #81
PF_Fixme,				// #82
PF_Fixme,				// #83
PF_Fixme,				// #84
PF_Fixme,				// #85
PF_Fixme,				// #86
PF_Fixme,				// #87
PF_Fixme,				// #88
PF_Fixme,				// #89

PF_tracebox,			// #90 LordHavoc builtin range (9x)
PF_randomvec,			// #91
PF_GetLight,			// #92
PF_registercvar,		// #93
PF_min,					// #94
PF_max,					// #95
PF_bound,				// #96
PF_pow,					// #97
PF_FindFloat,			// #98
PF_checkextension,		// #99
#define a PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme,
#define aa a a a a a a a a a a
aa // #200
aa // #300
aa // #400
PF_copyentity,			// #400 LordHavoc: builtin range (4xx)
PF_setcolor,			// #401
PF_findchain,			// #402
PF_findchainfloat,		// #403
PF_effect,				// #404
PF_te_blood,			// #405
PF_te_bloodshower,		// #406
PF_te_explosionrgb,		// #407
PF_te_particlecube,		// #408
PF_te_particlerain,		// #409
PF_te_particlesnow,		// #410
PF_te_spark,			// #411
PF_te_gunshotquad,		// #412
PF_te_spikequad,		// #413
PF_te_superspikequad,	// #414
PF_te_explosionquad,	// #415
PF_te_smallflash,		// #416
PF_te_customflash,		// #417
PF_te_gunshot,			// #418
PF_te_spike,			// #419
PF_te_superspike,		// #420
PF_te_explosion,		// #421
PF_te_tarexplosion,		// #422
PF_te_wizspike,			// #423
PF_te_knightspike,		// #424
PF_te_lavasplash,		// #425
PF_te_teleport,			// #426
PF_te_explosion2,		// #427
PF_te_lightning1,		// #428
PF_te_lightning2,		// #429
PF_te_lightning3,		// #430
PF_te_beam,				// #431
PF_vectorvectors,		// #432
};

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);
