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
// sv_user.c -- server code for moving users

#include "quakedef.h"

edict_t *sv_player;

cvar_t sv_edgefriction = {0, "edgefriction", "2"};
cvar_t sv_deltacompress = {0, "sv_deltacompress", "1"};
cvar_t sv_idealpitchscale = {0, "sv_idealpitchscale","0.8"};
cvar_t sv_maxspeed = {CVAR_NOTIFY, "sv_maxspeed", "320"};
cvar_t sv_accelerate = {0, "sv_accelerate", "10"};

static vec3_t forward, right, up;

vec3_t wishdir;
float wishspeed;

// world
float *angles;
float *origin;
float *velocity;

qboolean onground;

usercmd_t cmd;


/*
===============
SV_SetIdealPitch
===============
*/
#define	MAX_FORWARD	6
void SV_SetIdealPitch (void)
{
	float	angleval, sinval, cosval;
	trace_t	tr;
	vec3_t	top, bottom;
	float	z[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;

	if (!((int)sv_player->v->flags & FL_ONGROUND))
		return;

	angleval = sv_player->v->angles[YAW] * M_PI*2 / 360;
	sinval = sin(angleval);
	cosval = cos(angleval);

	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		top[0] = sv_player->v->origin[0] + cosval*(i+3)*12;
		top[1] = sv_player->v->origin[1] + sinval*(i+3)*12;
		top[2] = sv_player->v->origin[2] + sv_player->v->view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = SV_Move (top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, sv_player);
		// if looking at a wall, leave ideal the way is was
		if (tr.allsolid)
			return;

		// near a dropoff
		if (tr.fraction == 1)
			return;

		z[i] = top[2] + tr.fraction*(bottom[2]-top[2]);
	}

	dir = 0;
	steps = 0;
	for (j=1 ; j<i ; j++)
	{
		step = z[j] - z[j-1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		// mixed changes
		if (dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ) )
			return;

		steps++;
		dir = step;
	}

	if (!dir)
	{
		sv_player->v->idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	sv_player->v->idealpitch = -dir * sv_idealpitchscale.value;
}


/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction (void)
{
	float *vel, speed, newspeed, control, friction;
	vec3_t start, stop;
	trace_t trace;

	vel = velocity;

	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0]/speed*16;
	start[1] = stop[1] = origin[1] + vel[1]/speed*16;
	start[2] = origin[2] + sv_player->v->mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction.value*sv_edgefriction.value;
	else
		friction = sv_friction.value;

	// apply friction
	control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
	newspeed = speed - sv.frametime*control*friction;

	if (newspeed < 0)
		newspeed = 0;
	else
		newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

/*
==============
SV_Accelerate
==============
*/
void SV_Accelerate (void)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*sv.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishdir[i];
}

void SV_AirAccelerate (vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalizeLength (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*wishspeed * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed*wishveloc[i];
}


void DropPunchAngle (void)
{
	float len;
	eval_t *val;

	len = VectorNormalizeLength (sv_player->v->punchangle);

	len -= 10*sv.frametime;
	if (len < 0)
		len = 0;
	VectorScale (sv_player->v->punchangle, len, sv_player->v->punchangle);

	if ((val = GETEDICTFIELDVALUE(sv_player, eval_punchvector)))
	{
		len = VectorNormalizeLength (val->vector);

		len -= 20*sv.frametime;
		if (len < 0)
			len = 0;
		VectorScale (val->vector, len, val->vector);
	}
}

/*
===================
SV_FreeMove
===================
*/
void SV_FreeMove (void)
{
	int i;
	float wishspeed;

	AngleVectors (sv_player->v->v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		velocity[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	velocity[2] += cmd.upmove;

	wishspeed = VectorLength (velocity);
	if (wishspeed > sv_maxspeed.value)
	{
		VectorScale (velocity, sv_maxspeed.value / wishspeed, velocity);
		wishspeed = sv_maxspeed.value;
	}
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove (void)
{
	int i;
	vec3_t wishvel;
	float speed, newspeed, wishspeed, addspeed, accelspeed, temp;

	// user intentions
	AngleVectors (sv_player->v->v_angle, forward, right, up);

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*cmd.forwardmove + right[i]*cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength(wishvel);
	if (wishspeed > sv_maxspeed.value)
	{
		temp = sv_maxspeed.value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed.value;
	}
	wishspeed *= 0.7;

	// water friction
	speed = VectorLength (velocity);
	if (speed)
	{
		newspeed = speed - sv.frametime * speed * sv_friction.value;
		if (newspeed < 0)
			newspeed = 0;
		temp = newspeed/speed;
		VectorScale (velocity, temp, velocity);
	}
	else
		newspeed = 0;

	// water acceleration
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalize (wishvel);
	accelspeed = sv_accelerate.value * wishspeed * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > sv_player->v->teleport_time || !sv_player->v->waterlevel)
	{
		sv_player->v->flags = (int)sv_player->v->flags & ~FL_WATERJUMP;
		sv_player->v->teleport_time = 0;
	}
	sv_player->v->velocity[0] = sv_player->v->movedir[0];
	sv_player->v->velocity[1] = sv_player->v->movedir[1];
}


/*
===================
SV_AirMove

===================
*/
void SV_AirMove (void)
{
	int i;
	vec3_t wishvel;
	float fmove, smove, temp;

	// LordHavoc: correct quake movement speed bug when looking up/down
	wishvel[0] = wishvel[2] = 0;
	wishvel[1] = sv_player->v->angles[1];
	AngleVectors (wishvel, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < sv_player->v->teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ((int)sv_player->v->movetype != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		temp = sv_maxspeed.value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	if (sv_player->v->movetype == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy (wishvel, velocity);
	}
	else if ( onground )
	{
		SV_UserFriction ();
		SV_Accelerate ();
	}
	else
	{
		// not on ground, so little effect on velocity
		SV_AirAccelerate (wishvel);
	}
}

/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink (void)
{
	vec3_t v_angle;

	if (sv_player->v->movetype == MOVETYPE_NONE)
		return;

	onground = (int)sv_player->v->flags & FL_ONGROUND;

	origin = sv_player->v->origin;
	velocity = sv_player->v->velocity;

	DropPunchAngle ();

	// if dead, behave differently
	if (sv_player->v->health <= 0)
		return;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	cmd = host_client->cmd;
	angles = sv_player->v->angles;

	VectorAdd (sv_player->v->v_angle, sv_player->v->punchangle, v_angle);
	angles[ROLL] = V_CalcRoll (sv_player->v->angles, sv_player->v->velocity)*4;
	if (!sv_player->v->fixangle)
	{
		// LordHavoc: pitch was ugly to begin with...  removed except in water
		if (sv_player->v->waterlevel >= 2)
			angles[PITCH] = -v_angle[PITCH]/3;
		else
			angles[PITCH] = 0;
		angles[YAW] = v_angle[YAW];
	}

	if ( (int)sv_player->v->flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}

	// Player is (somehow) outside of the map, or flying, or noclipping
	if (SV_TestEntityPosition (sv_player)
	 || sv_player->v->movetype == MOVETYPE_FLY
	 || sv_player->v->movetype == MOVETYPE_NOCLIP)
	{
		SV_FreeMove ();
		return;
	}

	// walk
	if ((sv_player->v->waterlevel >= 2) && (sv_player->v->movetype != MOVETYPE_NOCLIP))
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}


/*
===================
SV_ReadClientMove
===================
*/
void SV_ReadClientMove (usercmd_t *move)
{
	int i;
	vec3_t angle;
	int bits;
	eval_t *val;
	float total;

	// read ping time
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] = sv.time - MSG_ReadFloat ();
	host_client->num_pings++;
	for (i=0, total = 0;i < NUM_PING_TIMES;i++)
		total += host_client->ping_times[i];
	// can be used for prediction
	host_client->ping = total / NUM_PING_TIMES;
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_ping)))
		val->_float = host_client->ping * 1000.0;

	// read current angles
	// dpprotocol version 2
	for (i = 0;i < 3;i++)
		angle[i] = MSG_ReadFloat ();

	VectorCopy (angle, sv_player->v->v_angle);

	// read movement
	move->forwardmove = MSG_ReadShort ();
	move->sidemove = MSG_ReadShort ();
	move->upmove = MSG_ReadShort ();
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_movement)))
	{
		val->vector[0] = move->forwardmove;
		val->vector[1] = move->sidemove;
		val->vector[2] = move->upmove;
	}

	// read buttons
	bits = MSG_ReadByte ();
	sv_player->v->button0 = bits & 1;
	sv_player->v->button2 = (bits & 2)>>1;
	// LordHavoc: added 6 new buttons
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button3))) val->_float = ((bits >> 2) & 1);
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button4))) val->_float = ((bits >> 3) & 1);
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button5))) val->_float = ((bits >> 4) & 1);
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button6))) val->_float = ((bits >> 5) & 1);
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button7))) val->_float = ((bits >> 6) & 1);
	if ((val = GETEDICTFIELDVALUE(sv_player, eval_button8))) val->_float = ((bits >> 7) & 1);

	i = MSG_ReadByte ();
	if (i)
		sv_player->v->impulse = i;
}

/*
===================
SV_ReadClientMessage
===================
*/
extern void SV_SendServerinfo(client_t *client);
void SV_ReadClientMessage(void)
{
	int cmd;
	char *s;

	//MSG_BeginReading ();

	for(;;)
	{
		if (!host_client->active)
		{
			// a command caused an error
			SV_DropClient (false);
			return;
		}

		if (msg_badread)
		{
			Con_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (false);
			return;
		}

		cmd = MSG_ReadChar ();
		if (cmd == -1)
		{
			// end of message
			break;
		}

		switch (cmd)
		{
		default:
			Con_Printf ("SV_ReadClientMessage: unknown command char %i\n", cmd);
			SV_DropClient (false);
			return;

		case clc_nop:
			break;

		case clc_stringcmd:
			s = MSG_ReadString ();
			if (strncasecmp(s, "spawn", 5) == 0
			 || strncasecmp(s, "begin", 5) == 0
			 || strncasecmp(s, "prespawn", 8) == 0)
				Cmd_ExecuteString (s, src_client);
			else if (SV_ParseClientCommandQC)
			{
				G_INT(OFS_PARM0) = PR_SetString(s);
				pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
				PR_ExecuteProgram ((func_t)(SV_ParseClientCommandQC - pr_functions), "");
			}
			else if (strncasecmp(s, "status", 6) == 0
			 || strncasecmp(s, "name", 4) == 0
			 || strncasecmp(s, "say", 3) == 0
			 || strncasecmp(s, "say_team", 8) == 0
			 || strncasecmp(s, "tell", 4) == 0
			 || strncasecmp(s, "color", 5) == 0
			 || strncasecmp(s, "kill", 4) == 0
			 || strncasecmp(s, "pause", 5) == 0
			 || strncasecmp(s, "kick", 4) == 0
			 || strncasecmp(s, "ping", 4) == 0
			 || strncasecmp(s, "ban", 3) == 0
			 || strncasecmp(s, "pmodel", 6) == 0
			 || (gamemode == GAME_NEHAHRA && (strncasecmp(s, "max", 3) == 0 || strncasecmp(s, "monster", 7) == 0 || strncasecmp(s, "scrag", 5) == 0 || strncasecmp(s, "gimme", 5) == 0 || strncasecmp(s, "wraith", 6) == 0))
			 || (gamemode != GAME_NEHAHRA && (strncasecmp(s, "god", 3) == 0 || strncasecmp(s, "notarget", 8) == 0 || strncasecmp(s, "fly", 3) == 0 || strncasecmp(s, "give", 4) == 0 || strncasecmp(s, "noclip", 6) == 0)))
				Cmd_ExecuteString (s, src_client);
			else
				Con_Printf("%s tried to %s\n", host_client->name, s);
			break;

		case clc_disconnect:
			SV_DropClient (false); // client wants to disconnect
			return;

		case clc_move:
			SV_ReadClientMove (&host_client->cmd);
			break;

		case clc_ackentities:
			EntityFrame_AckFrame(&host_client->entitydatabase, MSG_ReadLong());
			break;
		}
	}
}

/*
==================
SV_RunClients
==================
*/
void SV_RunClients (void)
{
	int i;

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!host_client->spawned)
		{
			// clear client movement until a new packet is received
			memset (&host_client->cmd, 0, sizeof(host_client->cmd));
			continue;
		}

		if (sv.frametime)
		{
			// LordHavoc: QuakeC replacement for SV_ClientThink (player movement)
			if (SV_PlayerPhysicsQC)
			{
				pr_global_struct->time = sv.time;
				pr_global_struct->self = EDICT_TO_PROG(sv_player);
				PR_ExecuteProgram ((func_t)(SV_PlayerPhysicsQC - pr_functions), "");
			}
			else
				SV_ClientThink ();

			SV_CheckVelocity (sv_player);

			// LordHavoc: a hack to ensure that the (rather silly) id1 quakec
			// player_run/player_stand1 does not horribly malfunction if the
			// velocity becomes a number that is both == 0 and != 0
			// (sounds to me like NaN but to be absolutely safe...)
			if (DotProduct(sv_player->v->velocity, sv_player->v->velocity) < 0.0001)
				VectorClear(sv_player->v->velocity);
		}
	}
}

