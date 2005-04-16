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

cvar_t sv_edgefriction = {0, "edgefriction", "2"};
cvar_t sv_deltacompress = {0, "sv_deltacompress", "1"};
cvar_t sv_idealpitchscale = {0, "sv_idealpitchscale","0.8"};
cvar_t sv_maxspeed = {CVAR_NOTIFY, "sv_maxspeed", "320"};
cvar_t sv_accelerate = {0, "sv_accelerate", "10"};

static usercmd_t cmd;


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

	if (!((int)host_client->edict->v->flags & FL_ONGROUND))
		return;

	angleval = host_client->edict->v->angles[YAW] * M_PI*2 / 360;
	sinval = sin(angleval);
	cosval = cos(angleval);

	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		top[0] = host_client->edict->v->origin[0] + cosval*(i+3)*12;
		top[1] = host_client->edict->v->origin[1] + sinval*(i+3)*12;
		top[2] = host_client->edict->v->origin[2] + host_client->edict->v->view_ofs[2];

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
		host_client->edict->v->idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	host_client->edict->v->idealpitch = -dir * sv_idealpitchscale.value;
}

#if 1
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

	speed = sqrt(host_client->edict->v->velocity[0]*host_client->edict->v->velocity[0]+host_client->edict->v->velocity[1]*host_client->edict->v->velocity[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = host_client->edict->v->origin[0] + host_client->edict->v->velocity[0]/speed*16;
	start[1] = stop[1] = host_client->edict->v->origin[1] + host_client->edict->v->velocity[1]/speed*16;
	start[2] = host_client->edict->v->origin[2] + host_client->edict->v->mins[2];
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

	VectorScale(host_client->edict->v->velocity, newspeed, host_client->edict->v->velocity);
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

	currentspeed = DotProduct (host_client->edict->v->velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*sv.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		host_client->edict->v->velocity[i] += accelspeed*wishdir[i];
}

void SV_AirAccelerate (vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalizeLength (wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (host_client->edict->v->velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*wishspeed * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		host_client->edict->v->velocity[i] += accelspeed*wishveloc[i];
}


void DropPunchAngle (void)
{
	float len;
	eval_t *val;

	len = VectorNormalizeLength (host_client->edict->v->punchangle);

	len -= 10*sv.frametime;
	if (len < 0)
		len = 0;
	VectorScale (host_client->edict->v->punchangle, len, host_client->edict->v->punchangle);

	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_punchvector)))
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

	AngleVectors (host_client->edict->v->v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		host_client->edict->v->velocity[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	host_client->edict->v->velocity[2] += cmd.upmove;

	wishspeed = VectorLength(host_client->edict->v->velocity);
	if (wishspeed > sv_maxspeed.value)
		VectorScale(host_client->edict->v->velocity, sv_maxspeed.value / wishspeed, host_client->edict->v->velocity);
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
	AngleVectors (host_client->edict->v->v_angle, forward, right, up);

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
	speed = VectorLength(host_client->edict->v->velocity);
	if (speed)
	{
		newspeed = speed - sv.frametime * speed * sv_friction.value;
		if (newspeed < 0)
			newspeed = 0;
		temp = newspeed/speed;
		VectorScale(host_client->edict->v->velocity, temp, host_client->edict->v->velocity);
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
		host_client->edict->v->velocity[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > host_client->edict->v->teleport_time || !host_client->edict->v->waterlevel)
	{
		host_client->edict->v->flags = (int)host_client->edict->v->flags & ~FL_WATERJUMP;
		host_client->edict->v->teleport_time = 0;
	}
	host_client->edict->v->velocity[0] = host_client->edict->v->movedir[0];
	host_client->edict->v->velocity[1] = host_client->edict->v->movedir[1];
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
	wishvel[1] = host_client->edict->v->angles[1];
	AngleVectors (wishvel, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < host_client->edict->v->teleport_time && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ((int)host_client->edict->v->movetype != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		temp = sv_maxspeed.value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	if (host_client->edict->v->movetype == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy (wishvel, host_client->edict->v->velocity);
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

	if (host_client->edict->v->movetype == MOVETYPE_NONE)
		return;

	onground = (int)host_client->edict->v->flags & FL_ONGROUND;

	DropPunchAngle ();

	// if dead, behave differently
	if (host_client->edict->v->health <= 0)
		return;

	cmd = host_client->cmd;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	VectorAdd (host_client->edict->v->v_angle, host_client->edict->v->punchangle, v_angle);
	host_client->edict->v->angles[ROLL] = V_CalcRoll (host_client->edict->v->angles, host_client->edict->v->velocity)*4;
	if (!host_client->edict->v->fixangle)
	{
		host_client->edict->v->angles[PITCH] = -v_angle[PITCH]/3;
		host_client->edict->v->angles[YAW] = v_angle[YAW];
	}

	if ( (int)host_client->edict->v->flags & FL_WATERJUMP )
	{
		SV_WaterJump ();
		return;
	}

	/*
	// Player is (somehow) outside of the map, or flying, or noclipping
	if (host_client->edict->v->movetype != MOVETYPE_NOCLIP && (host_client->edict->v->movetype == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict)))
	//if (host_client->edict->v->movetype == MOVETYPE_NOCLIP || host_client->edict->v->movetype == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict))
	{
		SV_FreeMove ();
		return;
	}
	*/

	// walk
	if ((host_client->edict->v->waterlevel >= 2) && (host_client->edict->v->movetype != MOVETYPE_NOCLIP))
	{
		SV_WaterMove ();
		return;
	}

	SV_AirMove ();
}

#else

extern cvar_t cl_rollspeed;
extern cvar_t cl_rollangle;
void SV_ClientThink(void)
{
	int j;
	vec3_t wishvel, wishdir, v, v_forward, v_right, v_up, start, stop;
	float wishspeed, f, limit;
	trace_t trace;

	if (host_client->edict->v->movetype == MOVETYPE_NONE)
		return;

	f = DotProduct(host_client->edict->v->punchangle, host_client->edict->v->punchangle);
	if (f)
	{
		limit = sqrt(f);
		f = (limit - 10 * sv.frametime);
		f /= limit;
		f = max(0, f);
		VectorScale(host_client->edict->v->punchangle, f, host_client->edict->v->punchangle);
	}

	// if dead, behave differently
	if (host_client->edict->v->health <= 0)
		return;

	AngleVectors(host_client->edict->v->v_angle, v_forward, v_right, v_up);
	// show 1/3 the pitch angle and all the roll angle
	f = DotProduct(host_client->edict->v->velocity, v_right) * (1.0 / cl_rollspeed.value);
	host_client->edict->v->angles[2] = bound(-1, f, 1) * cl_rollangle.value * 4;
	if (!host_client->edict->v->fixangle)
	{
		host_client->edict->v->angles[0] = (host_client->edict->v->v_angle[0] + host_client->edict->v->punchangle[0]) * -0.333;
		host_client->edict->v->angles[1] = host_client->edict->v->v_angle[1] + host_client->edict->v->punchangle[1];
	}

	if ((int)host_client->edict->v->flags & FL_WATERJUMP)
	{
		host_client->edict->v->velocity[0] = host_client->edict->v->movedir[0];
		host_client->edict->v->velocity[1] = host_client->edict->v->movedir[1];
		if (sv.time > host_client->edict->v->teleport_time || host_client->edict->v->waterlevel == 0)
		{
			host_client->edict->v->flags = (int)host_client->edict->v->flags - ((int)host_client->edict->v->flags & FL_WATERJUMP);
			host_client->edict->v->teleport_time = 0;
		}
		return;
	}

	// swim
	if (host_client->edict->v->waterlevel >= 2)
	if (host_client->edict->v->movetype != MOVETYPE_NOCLIP)
	{
		if (host_client->cmd.forwardmove == 0 && host_client->cmd.sidemove == 0 && host_client->cmd.upmove == 0)
		{
			// drift towards bottom
			wishvel[0] = 0;
			wishvel[1] = 0;
			wishvel[2] = -60;
		}
		else
		{
			for (j = 0;j < 3;j++)
				wishvel[j] = v_forward[j] * host_client->cmd.forwardmove + v_right[j] * host_client->cmd.sidemove;
			wishvel[2] += host_client->cmd.upmove;
		}

		wishspeed = VectorLength(wishvel);
		wishspeed = min(wishspeed, sv_maxspeed.value) * 0.7;

		// water friction
		f = VectorLength(host_client->edict->v->velocity) * (1 - sv.frametime * sv_friction.value);
		if (f > 0)
			f /= VectorLength(host_client->edict->v->velocity);
		else
			f = 0;
		VectorScale(host_client->edict->v->velocity, f, host_client->edict->v->velocity);

		// water acceleration
		if (wishspeed <= f)
			return;

		f = wishspeed - f;
		limit = sv_accelerate.value * wishspeed * sv.frametime;
		if (f > limit)
			f = limit;
		limit = VectorLength(wishvel);
		if (limit)
			f /= limit;
		VectorMA(host_client->edict->v->velocity, f, wishvel, host_client->edict->v->velocity);
		return;
	}

	// if not flying, move horizontally only
	if (host_client->edict->v->movetype != MOVETYPE_FLY)
	{
		VectorClear(wishvel);
		wishvel[1] = host_client->edict->v->v_angle[1];
		AngleVectors(wishvel, v_forward, v_right, v_up);
	}

	// hack to not let you back into teleporter
	VectorScale(v_right, host_client->cmd.sidemove, wishvel);
	if (sv.time >= host_client->edict->v->teleport_time || host_client->cmd.forwardmove > 0)
		VectorMA(wishvel, host_client->cmd.forwardmove, v_forward, wishvel);
	if (host_client->edict->v->movetype != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy(wishvel, wishdir);
	VectorNormalize(wishdir);
	wishspeed = VectorLength(wishvel);
	if (wishspeed > sv_maxspeed.value)
		wishspeed = sv_maxspeed.value;

	if (host_client->edict->v->movetype == MOVETYPE_NOCLIP || host_client->edict->v->movetype == MOVETYPE_FLY)
	{
		VectorScale(wishdir, wishspeed, host_client->edict->v->velocity);
		return;
	}

	if ((int)host_client->edict->v->flags & FL_ONGROUND) // walking
	{
		// friction
		f = host_client->edict->v->velocity[0] * host_client->edict->v->velocity[0] + host_client->edict->v->velocity[1] * host_client->edict->v->velocity[1];
		if (f)
		{
			f = sqrt(f);
			VectorCopy(host_client->edict->v->velocity, v);
			v[2] = 0;

			// if the leading edge is over a dropoff, increase friction
			limit = 16.0f / f;
			VectorMA(host_client->edict->v->origin, limit, v, v);
			v[2] += host_client->edict->v->mins[2];

			VectorCopy(v, start);
			VectorCopy(v, stop);
			stop[2] -= 34;
			trace = SV_Move(start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, host_client->edict);

			// apply friction
			if (f < sv_stopspeed.value)
				f = sv_stopspeed.value / f;
			else
				f = 1;
			if (trace.fraction == 1)
				f *= sv_edgefriction.value;
			f = 1 - sv.frametime * f * sv_friction.value;

			if (f < 0)
				f = 0;
			VectorScale(host_client->edict->v->velocity, f, host_client->edict->v->velocity);
		}
	}
	else // airborn
		wishspeed = min(wishspeed, 30);

	// ground or air acceleration
	f = wishspeed - DotProduct(host_client->edict->v->velocity, wishdir);
	if (f > 0)
	{
		limit = sv_accelerate.value * sv.frametime * wishspeed;
		if (f > limit)
			f = limit;
		VectorMA(host_client->edict->v->velocity, f, wishdir, host_client->edict->v->velocity);
	}
}
#endif

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
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] = sv.time - MSG_ReadFloat ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	host_client->num_pings++;
	for (i=0, total = 0;i < NUM_PING_TIMES;i++)
		total += host_client->ping_times[i];
	// can be used for prediction
	host_client->ping = total / NUM_PING_TIMES;
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_ping)))
		val->_float = host_client->ping * 1000.0;

	// read current angles
	for (i = 0;i < 3;i++)
	{
		if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
		if (sv.protocol == PROTOCOL_QUAKE)
			angle[i] = MSG_ReadAngle8i();
		else if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3)
			angle[i] = MSG_ReadAngle32f();
		else if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
			angle[i] = MSG_ReadAngle16i();
		if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	}

	VectorCopy (angle, host_client->edict->v->v_angle);

	// read movement
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	move->forwardmove = MSG_ReadCoord16i ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	move->sidemove = MSG_ReadCoord16i ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	move->upmove = MSG_ReadCoord16i ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_movement)))
	{
		val->vector[0] = move->forwardmove;
		val->vector[1] = move->sidemove;
		val->vector[2] = move->upmove;
	}

	// read buttons
	if (sv.protocol == PROTOCOL_DARKPLACES6)
		bits = MSG_ReadLong ();
	else
		bits = MSG_ReadByte ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	host_client->edict->v->button0 = bits & 1;
	host_client->edict->v->button2 = (bits & 2)>>1;
	// LordHavoc: added 6 new buttons
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button3))) val->_float = ((bits >> 2) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button4))) val->_float = ((bits >> 3) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button5))) val->_float = ((bits >> 4) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button6))) val->_float = ((bits >> 5) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button7))) val->_float = ((bits >> 6) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_button8))) val->_float = ((bits >> 7) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_buttonuse))) val->_float = ((bits >> 8) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_buttonchat))) val->_float = ((bits >> 9) & 1);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_cursor_active))) val->_float = ((bits >> 10) & 1);

	i = MSG_ReadByte ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	if (i)
		host_client->edict->v->impulse = i;

	// PRYDON_CLIENTCURSOR
	if (sv.protocol == PROTOCOL_DARKPLACES6)
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
		if (move->cursor_entitynumber >= sv.max_edicts)
		{
			Con_DPrintf("SV_ReadClientMessage: client send bad cursor_entitynumber\n");
			move->cursor_entitynumber = 0;
		}
		// as requested by FrikaC, cursor_trace_ent is reset to world if the
		// entity is free at time of receipt
		if (EDICT_NUM(move->cursor_entitynumber)->e->free)
			move->cursor_entitynumber = 0;
		if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	}
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_cursor_screen))) VectorCopy(move->cursor_screen, val->vector);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_start))) VectorCopy(move->cursor_start, val->vector);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_endpos))) VectorCopy(move->cursor_impact, val->vector);
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_cursor_trace_ent))) val->edict = EDICT_TO_PROG(EDICT_NUM(move->cursor_entitynumber));
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
void SV_ReadClientMessage(void)
{
	int cmd, num;
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

		cmd = MSG_ReadChar ();
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
				G_INT(OFS_PARM0) = PR_SetString(s);
				pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
				PR_ExecuteProgram ((func_t)(SV_ParseClientCommandQC - pr_functions), "QC function SV_ParseClientCommand is missing");
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
			 || strncasecmp(s, "rate", 4) == 0
			 || strncasecmp(s, "playermodel", 11) == 0
			 || strncasecmp(s, "playerskin", 10) == 0
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

		case clc_ackframe:
			if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
			num = MSG_ReadLong();
			if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
			if (developer_networkentities.integer >= 1)
				Con_Printf("recv clc_ackframe %i\n", num);
			if (host_client->latestframenum < num)
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

/*
==================
SV_RunClients
==================
*/
void SV_RunClients (void)
{
	int i;

	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;

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
				pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
				PR_ExecuteProgram ((func_t)(SV_PlayerPhysicsQC - pr_functions), "QC function SV_PlayerPhysics is missing");
			}
			else
				SV_ClientThink ();

			SV_CheckVelocity(host_client->edict);

			// LordHavoc: a hack to ensure that the (rather silly) id1 quakec
			// player_run/player_stand1 does not horribly malfunction if the
			// velocity becomes a number that is both == 0 and != 0
			// (sounds to me like NaN but to be absolutely safe...)
			if (DotProduct(host_client->edict->v->velocity, host_client->edict->v->velocity) < 0.0001)
				VectorClear(host_client->edict->v->velocity);
		}
	}
}

