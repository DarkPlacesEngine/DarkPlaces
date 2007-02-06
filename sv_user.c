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

cvar_t sv_edgefriction = {0, "edgefriction", "2", "how much you slow down when nearing a ledge you might fall off"};
cvar_t sv_idealpitchscale = {0, "sv_idealpitchscale","0.8", "how much to look up/down slopes and stairs when not using freelook"};
cvar_t sv_maxspeed = {CVAR_NOTIFY, "sv_maxspeed", "320", "maximum speed a player can accelerate to when on ground (can be exceeded by tricks)"};
cvar_t sv_maxairspeed = {0, "sv_maxairspeed", "30", "maximum speed a player can accelerate to when airborn (note that it is possible to completely stop by moving the opposite direction)"};
cvar_t sv_accelerate = {0, "sv_accelerate", "10", "rate at which a player accelerates to sv_maxspeed"};
cvar_t sv_airaccelerate = {0, "sv_airaccelerate", "-1", "rate at which a player accelerates to sv_maxairspeed while in the air, if less than 0 the sv_accelerate variable is used instead"};
cvar_t sv_wateraccelerate = {0, "sv_wateraccelerate", "-1", "rate at which a player accelerates to sv_maxspeed while in the air, if less than 0 the sv_accelerate variable is used instead"};
cvar_t sv_clmovement_enable = {0, "sv_clmovement_enable", "1", "whether to allow clients to use cl_movement prediction, which can cause choppy movement on the server which may annoy other players"};
cvar_t sv_clmovement_minping = {0, "sv_clmovement_minping", "0", "if client ping is below this time in milliseconds, then their ability to use cl_movement prediction is disabled for a while (as they don't need it)"};
cvar_t sv_clmovement_minping_disabletime = {0, "sv_clmovement_minping_disabletime", "1000", "when client falls below minping, disable their prediction for this many milliseconds (should be at least 1000 or else their prediction may turn on/off frequently)"};
cvar_t sv_clmovement_waitforinput = {0, "sv_clmovement_waitforinput", "16", "when a client does not send input for this many frames, force them to move anyway (unlike QuakeWorld)"};

static usercmd_t cmd;


/*
===============
SV_SetIdealPitch
===============
*/
#define	MAX_FORWARD	6
void SV_SetIdealPitch (void)
{
	float	angleval, sinval, cosval, step, dir;
	trace_t	tr;
	vec3_t	top, bottom;
	float	z[MAX_FORWARD];
	int		i, j;
	int		steps;

	if (!((int)host_client->edict->fields.server->flags & FL_ONGROUND))
		return;

	angleval = host_client->edict->fields.server->angles[YAW] * M_PI*2 / 360;
	sinval = sin(angleval);
	cosval = cos(angleval);

	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		top[0] = host_client->edict->fields.server->origin[0] + cosval*(i+3)*12;
		top[1] = host_client->edict->fields.server->origin[1] + sinval*(i+3)*12;
		top[2] = host_client->edict->fields.server->origin[2] + host_client->edict->fields.server->view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = SV_Move (top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, host_client->edict);
		// if looking at a wall, leave ideal the way is was
		if (tr.startsolid)
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
		host_client->edict->fields.server->idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	host_client->edict->fields.server->idealpitch = -dir * sv_idealpitchscale.value;
}

static vec3_t wishdir, forward, right, up;
static float wishspeed;

static qboolean onground;

/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction (void)
{
	float speed, newspeed, control, friction;
	vec3_t start, stop;
	trace_t trace;

	speed = sqrt(host_client->edict->fields.server->velocity[0]*host_client->edict->fields.server->velocity[0]+host_client->edict->fields.server->velocity[1]*host_client->edict->fields.server->velocity[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = host_client->edict->fields.server->origin[0] + host_client->edict->fields.server->velocity[0]/speed*16;
	start[1] = stop[1] = host_client->edict->fields.server->origin[1] + host_client->edict->fields.server->velocity[1]/speed*16;
	start[2] = host_client->edict->fields.server->origin[2] + host_client->edict->fields.server->mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move (start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, host_client->edict);

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

	VectorScale(host_client->edict->fields.server->velocity, newspeed, host_client->edict->fields.server->velocity);
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

	currentspeed = DotProduct (host_client->edict->fields.server->velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*sv.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		host_client->edict->fields.server->velocity[i] += accelspeed*wishdir[i];
}

void SV_AirAccelerate (vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalizeLength (wishveloc);
	if (wishspd > sv_maxairspeed.value)
		wishspd = sv_maxairspeed.value;
	currentspeed = DotProduct (host_client->edict->fields.server->velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = (sv_airaccelerate.value < 0 ? sv_accelerate.value : sv_airaccelerate.value)*wishspeed * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		host_client->edict->fields.server->velocity[i] += accelspeed*wishveloc[i];
}


void DropPunchAngle (void)
{
	float len;
	prvm_eval_t *val;

	len = VectorNormalizeLength (host_client->edict->fields.server->punchangle);

	len -= 10*sv.frametime;
	if (len < 0)
		len = 0;
	VectorScale (host_client->edict->fields.server->punchangle, len, host_client->edict->fields.server->punchangle);

	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_punchvector)))
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

	AngleVectors (host_client->edict->fields.server->v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		host_client->edict->fields.server->velocity[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	host_client->edict->fields.server->velocity[2] += cmd.upmove;

	wishspeed = VectorLength(host_client->edict->fields.server->velocity);
	if (wishspeed > sv_maxspeed.value)
		VectorScale(host_client->edict->fields.server->velocity, sv_maxspeed.value / wishspeed, host_client->edict->fields.server->velocity);
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
	AngleVectors (host_client->edict->fields.server->v_angle, forward, right, up);

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
	speed = VectorLength(host_client->edict->fields.server->velocity);
	if (speed)
	{
		newspeed = speed - sv.frametime * speed * sv_waterfriction.value;
		if (newspeed < 0)
			newspeed = 0;
		temp = newspeed/speed;
		VectorScale(host_client->edict->fields.server->velocity, temp, host_client->edict->fields.server->velocity);
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
	accelspeed = (sv_wateraccelerate.value < 0 ? sv_accelerate.value : sv_wateraccelerate.value) * wishspeed * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		host_client->edict->fields.server->velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > host_client->edict->fields.server->teleport_time || !host_client->edict->fields.server->waterlevel)
	{
		host_client->edict->fields.server->flags = (int)host_client->edict->fields.server->flags & ~FL_WATERJUMP;
		host_client->edict->fields.server->teleport_time = 0;
	}
	host_client->edict->fields.server->velocity[0] = host_client->edict->fields.server->movedir[0];
	host_client->edict->fields.server->velocity[1] = host_client->edict->fields.server->movedir[1];
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
	wishvel[1] = host_client->edict->fields.server->angles[1];
	AngleVectors (wishvel, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < host_client->edict->fields.server->teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ((int)host_client->edict->fields.server->movetype != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		temp = sv_maxspeed.value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	if (host_client->edict->fields.server->movetype == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy (wishvel, host_client->edict->fields.server->velocity);
	}
	else if (onground && (!sv_gameplayfix_qwplayerphysics.integer || !(host_client->edict->fields.server->button2 || !((int)host_client->edict->fields.server->flags & FL_JUMPRELEASED))))
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

	if (host_client->edict->fields.server->movetype == MOVETYPE_NONE)
		return;

	onground = (int)host_client->edict->fields.server->flags & FL_ONGROUND;

	DropPunchAngle ();

	// if dead, behave differently
	if (host_client->edict->fields.server->health <= 0)
		return;

	cmd = host_client->cmd;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	VectorAdd (host_client->edict->fields.server->v_angle, host_client->edict->fields.server->punchangle, v_angle);
	host_client->edict->fields.server->angles[ROLL] = V_CalcRoll (host_client->edict->fields.server->angles, host_client->edict->fields.server->velocity)*4;
	if (!host_client->edict->fields.server->fixangle)
	{
		host_client->edict->fields.server->angles[PITCH] = -v_angle[PITCH]/3;
		host_client->edict->fields.server->angles[YAW] = v_angle[YAW];
	}

	if ( (int)host_client->edict->fields.server->flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}

	/*
	// Player is (somehow) outside of the map, or flying, or noclipping
	if (host_client->edict->fields.server->movetype != MOVETYPE_NOCLIP && (host_client->edict->fields.server->movetype == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict)))
	//if (host_client->edict->fields.server->movetype == MOVETYPE_NOCLIP || host_client->edict->fields.server->movetype == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict))
	{
		SV_FreeMove ();
		return;
	}
	*/

	// walk
	if ((host_client->edict->fields.server->waterlevel >= 2) && (host_client->edict->fields.server->movetype != MOVETYPE_NOCLIP))
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
qboolean SV_ReadClientMove (void)
{
	qboolean kickplayer = false;
	int i;
#ifdef NUM_PING_TIMES
	double total;
#endif
	double moveframetime;
	usercmd_t newmove;
	usercmd_t *move = &newmove;

	memset(move, 0, sizeof(*move));

	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read ping time
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5 && sv.protocol != PROTOCOL_DARKPLACES6)
		move->sequence = MSG_ReadLong ();
	move->time = MSG_ReadFloat ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	move->receivetime = (float)sv.time;

	// limit reported time to current time
	// (incase the client is trying to cheat)
	move->time = min(move->time, move->receivetime);

	// read current angles
	for (i = 0;i < 3;i++)
	{
		if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE)
			move->viewangles[i] = MSG_ReadAngle8i();
		else if (sv.protocol == PROTOCOL_DARKPLACES1)
			move->viewangles[i] = MSG_ReadAngle16i();
		else if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3)
			move->viewangles[i] = MSG_ReadAngle32f();
		else
			move->viewangles[i] = MSG_ReadAngle16i();
	}
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read movement
	move->forwardmove = MSG_ReadCoord16i ();
	move->sidemove = MSG_ReadCoord16i ();
	move->upmove = MSG_ReadCoord16i ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read buttons
	// be sure to bitwise OR them into the move->buttons because we want to
	// accumulate button presses from multiple packets per actual move
	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
		move->buttons = MSG_ReadByte ();
	else
		move->buttons = MSG_ReadLong ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read impulse
	move->impulse = MSG_ReadByte ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// PRYDON_CLIENTCURSOR
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5)
	{
		// 30 bytes
		move->cursor_screen[0] = MSG_ReadShort() * (1.0f / 32767.0f);
		move->cursor_screen[1] = MSG_ReadShort() * (1.0f / 32767.0f);
		move->cursor_start[0] = MSG_ReadFloat();
		move->cursor_start[1] = MSG_ReadFloat();
		move->cursor_start[2] = MSG_ReadFloat();
		move->cursor_impact[0] = MSG_ReadFloat();
		move->cursor_impact[1] = MSG_ReadFloat();
		move->cursor_impact[2] = MSG_ReadFloat();
		move->cursor_entitynumber = (unsigned short)MSG_ReadShort();
		if (move->cursor_entitynumber >= prog->max_edicts)
		{
			Con_DPrintf("SV_ReadClientMessage: client send bad cursor_entitynumber\n");
			move->cursor_entitynumber = 0;
		}
		// as requested by FrikaC, cursor_trace_ent is reset to world if the
		// entity is free at time of receipt
		if (PRVM_EDICT_NUM(move->cursor_entitynumber)->priv.server->free)
			move->cursor_entitynumber = 0;
		if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	}

	if (move->sequence && move->sequence <= host_client->movesequence)
	{
		// repeat of old input (to fight packet loss)
		return kickplayer;
	}

	// if the previous move has not been applied yet, we need to accumulate
	// the impulse/buttons from it
	if (!host_client->cmd.applied)
	{
		if (!move->impulse)
			move->impulse = host_client->cmd.impulse;
		move->buttons |= host_client->cmd.buttons;
	}

	moveframetime = bound(0, move->time - host_client->cmd.time, 0.1);
	//Con_Printf("movesequence = %i (%i lost), moveframetime = %f\n", move->sequence, move->sequence ? move->sequence - host_client->movesequence - 1 : 0, moveframetime);

	// disable clientside movement prediction in some cases
	if (ceil((move->receivetime - move->time) * 1000.0) < sv_clmovement_minping.integer)
		host_client->clmovement_disabletimeout = realtime + sv_clmovement_minping_disabletime.value / 1000.0;
	if (!sv_clmovement_enable.integer || host_client->clmovement_disabletimeout > realtime)
		move->sequence = 0;

	// calculate average ping time
	host_client->ping = move->receivetime - move->time;
#ifdef NUM_PING_TIMES
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] = move->receivetime - move->time;
	host_client->num_pings++;
	for (i=0, total = 0;i < NUM_PING_TIMES;i++)
		total += host_client->ping_times[i];
	host_client->ping = total / NUM_PING_TIMES;
#endif

	// only start accepting input once the player is spawned
	if (host_client->spawned)
	{
		// at this point we know this input is new and should be stored
		host_client->cmd = *move;
		host_client->movesequence = move->sequence;
		// if using prediction, we need to perform moves when packets are
		// received, even if multiple occur in one frame
		// (they can't go beyond the current time so there is no cheat issue
		//  with this approach, and if they don't send input for a while they
		//  start moving anyway, so the longest 'lagaport' possible is
		//  determined by the sv_clmovement_waitforinput cvar)
		if (host_client->movesequence && sv_clmovement_waitforinput.integer > 0 && moveframetime > 0)
		{
			double oldframetime = prog->globals.server->frametime;
			double oldframetime2 = sv.frametime;
			// the server and qc frametime values must be changed temporarily
			sv.frametime = moveframetime;
			prog->globals.server->frametime = moveframetime;
			SV_Physics_ClientEntity(host_client->edict);
			sv.frametime = oldframetime2;
			prog->globals.server->frametime = oldframetime;
			host_client->clmovement_skipphysicsframes = sv_clmovement_waitforinput.integer;
		}
	}

	return kickplayer;
}

void SV_ApplyClientMove (void)
{
	prvm_eval_t *val;
	usercmd_t *move = &host_client->cmd;

	if (!move->receivetime)
		return;

	// note: a move can be applied multiple times if the client packets are
	// not coming as often as the physics is executed, and the move must be
	// applied before running qc each time because the id1 qc had a bug where
	// it clears self.button2 in PlayerJump, causing pogostick behavior if
	// moves are not applied every time before calling qc
	move->applied = true;

	// set the edict fields
	host_client->edict->fields.server->button0 = move->buttons & 1;
	host_client->edict->fields.server->button2 = (move->buttons & 2)>>1;
	if (move->impulse)
		host_client->edict->fields.server->impulse = move->impulse;
	// only send the impulse to qc once
	move->impulse = 0;
	VectorCopy(move->viewangles, host_client->edict->fields.server->v_angle);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button3))) val->_float = ((move->buttons >> 2) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button4))) val->_float = ((move->buttons >> 3) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button5))) val->_float = ((move->buttons >> 4) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button6))) val->_float = ((move->buttons >> 5) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button7))) val->_float = ((move->buttons >> 6) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button8))) val->_float = ((move->buttons >> 7) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button9))) val->_float = ((move->buttons >> 11) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button10))) val->_float = ((move->buttons >> 12) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button11))) val->_float = ((move->buttons >> 13) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button12))) val->_float = ((move->buttons >> 14) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button13))) val->_float = ((move->buttons >> 15) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button14))) val->_float = ((move->buttons >> 16) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button15))) val->_float = ((move->buttons >> 17) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_button16))) val->_float = ((move->buttons >> 18) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_buttonuse))) val->_float = ((move->buttons >> 8) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_buttonchat))) val->_float = ((move->buttons >> 9) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_cursor_active))) val->_float = ((move->buttons >> 10) & 1);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_movement))) VectorSet(val->vector, move->forwardmove, move->sidemove, move->upmove);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_cursor_screen))) VectorCopy(move->cursor_screen, val->vector);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_start))) VectorCopy(move->cursor_start, val->vector);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_endpos))) VectorCopy(move->cursor_impact, val->vector);
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_ent))) val->edict = PRVM_EDICT_TO_PROG(PRVM_EDICT_NUM(move->cursor_entitynumber));
	if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ping))) val->_float = host_client->ping * 1000.0;
}

void SV_FrameLost(int framenum)
{
	if (host_client->entitydatabase5)
		EntityFrame5_LostFrame(host_client->entitydatabase5, framenum);
}

void SV_FrameAck(int framenum)
{
	if (host_client->entitydatabase)
		EntityFrame_AckFrame(host_client->entitydatabase, framenum);
	else if (host_client->entitydatabase4)
		EntityFrame4_AckFrame(host_client->entitydatabase4, framenum, true);
	else if (host_client->entitydatabase5)
		EntityFrame5_AckFrame(host_client->entitydatabase5, framenum);
}

/*
===================
SV_ReadClientMessage
===================
*/
extern void SV_SendServerinfo(client_t *client);
extern sizebuf_t vm_tempstringsbuf;
void SV_ReadClientMessage(void)
{
	int cmd, num, start;
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
			Con_Print("SV_ReadClientMessage: badread\n");
			SV_DropClient (false);
			return;
		}

		cmd = MSG_ReadByte ();
		if (cmd == -1)
		{
			// end of message
			break;
		}

		switch (cmd)
		{
		default:
			Con_Printf("SV_ReadClientMessage: unknown command char %i\n", cmd);
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
				int restorevm_tempstringsbuf_cursize;
				restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
				PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(s);
				prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
				PRVM_ExecuteProgram ((func_t)(SV_ParseClientCommandQC - prog->functions), "QC function SV_ParseClientCommand is missing");
				vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
			}
			else
				Cmd_ExecuteString (s, src_client);
			break;

		case clc_disconnect:
			SV_DropClient (false); // client wants to disconnect
			return;

		case clc_move:
			// if ReadClientMove returns true, the client tried to speed cheat
			if (SV_ReadClientMove ())
				SV_DropClient (false);
			break;

		case clc_ackdownloaddata:
			start = MSG_ReadLong();
			num = MSG_ReadShort();
			if (host_client->download_file && host_client->download_started)
			{
				if (host_client->download_expectedposition == start)
				{
					int size = (int)FS_FileSize(host_client->download_file);
					// a data block was successfully received by the client,
					// update the expected position on the next data block
					host_client->download_expectedposition = start + num;
					// if this was the last data block of the file, it's done
					if (host_client->download_expectedposition >= FS_FileSize(host_client->download_file))
					{
						// tell the client that the download finished
						// we need to calculate the crc now
						//
						// note: at this point the OS probably has the file
						// entirely in memory, so this is a faster operation
						// now than it was when the download started.
						//
						// it is also preferable to do this at the end of the
						// download rather than the start because it reduces
						// potential for Denial Of Service attacks against the
						// server.
						int crc;
						unsigned char *temp;
						FS_Seek(host_client->download_file, 0, SEEK_SET);
						temp = Mem_Alloc(tempmempool, size);
						FS_Read(host_client->download_file, temp, size);
						crc = CRC_Block(temp, size);
						Mem_Free(temp);
						// calculated crc, send the file info to the client
						// (so that it can verify the data)
						Host_ClientCommands(va("\ncl_downloadfinished %i %i %s\n", size, crc, host_client->download_name));
						Con_DPrintf("Download of %s by %s has finished\n", host_client->download_name, host_client->name);
						FS_Close(host_client->download_file);
						host_client->download_file = NULL;
						host_client->download_name[0] = 0;
						host_client->download_expectedposition = 0;
						host_client->download_started = false;
					}
				}
				else
				{
					// a data block was lost, reset to the expected position
					// and resume sending from there
					FS_Seek(host_client->download_file, host_client->download_expectedposition, SEEK_SET);
				}
			}
			break;

		case clc_ackframe:
			if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
			num = MSG_ReadLong();
			if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
			if (developer_networkentities.integer >= 1)
				Con_Printf("recv clc_ackframe %i\n", num);
			// if the client hasn't progressed through signons yet,
			// ignore any clc_ackframes we get (they're probably from the
			// previous level)
			if (host_client->spawned && host_client->latestframenum < num)
			{
				int i;
				for (i = host_client->latestframenum + 1;i < num;i++)
					SV_FrameLost(i);
				SV_FrameAck(num);
				host_client->latestframenum = num;
			}
			break;
		}
	}
}

