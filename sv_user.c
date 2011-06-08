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
#include "sv_demo.h"
#define DEBUGMOVES 0

static usercmd_t cmd;
extern cvar_t sv_autodemo_perclient;

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

	if (!((int)PRVM_serveredictfloat(host_client->edict, flags) & FL_ONGROUND))
		return;

	angleval = PRVM_serveredictvector(host_client->edict, angles)[YAW] * M_PI*2 / 360;
	sinval = sin(angleval);
	cosval = cos(angleval);

	for (i=0 ; i<MAX_FORWARD ; i++)
	{
		top[0] = PRVM_serveredictvector(host_client->edict, origin)[0] + cosval*(i+3)*12;
		top[1] = PRVM_serveredictvector(host_client->edict, origin)[1] + sinval*(i+3)*12;
		top[2] = PRVM_serveredictvector(host_client->edict, origin)[2] + PRVM_serveredictvector(host_client->edict, view_ofs)[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = SV_TraceLine(top, bottom, MOVE_NOMONSTERS, host_client->edict, SUPERCONTENTS_SOLID);
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
		PRVM_serveredictfloat(host_client->edict, idealpitch) = 0;
		return;
	}

	if (steps < 2)
		return;
	PRVM_serveredictfloat(host_client->edict, idealpitch) = -dir * sv_idealpitchscale.value;
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

	speed = sqrt(PRVM_serveredictvector(host_client->edict, velocity)[0]*PRVM_serveredictvector(host_client->edict, velocity)[0]+PRVM_serveredictvector(host_client->edict, velocity)[1]*PRVM_serveredictvector(host_client->edict, velocity)[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = PRVM_serveredictvector(host_client->edict, origin)[0] + PRVM_serveredictvector(host_client->edict, velocity)[0]/speed*16;
	start[1] = stop[1] = PRVM_serveredictvector(host_client->edict, origin)[1] + PRVM_serveredictvector(host_client->edict, velocity)[1]/speed*16;
	start[2] = PRVM_serveredictvector(host_client->edict, origin)[2] + PRVM_serveredictvector(host_client->edict, mins)[2];
	stop[2] = start[2] - 34;

	trace = SV_TraceLine(start, stop, MOVE_NOMONSTERS, host_client->edict, SV_GenericHitSuperContentsMask(host_client->edict));

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

	VectorScale(PRVM_serveredictvector(host_client->edict, velocity), newspeed, PRVM_serveredictvector(host_client->edict, velocity));
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

	currentspeed = DotProduct (PRVM_serveredictvector(host_client->edict, velocity), wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value*sv.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		PRVM_serveredictvector(host_client->edict, velocity)[i] += accelspeed*wishdir[i];
}

extern cvar_t sv_gameplayfix_q2airaccelerate;
void SV_AirAccelerate (vec3_t wishveloc)
{
	int i;
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalizeLength (wishveloc);
	if (wishspd > sv_maxairspeed.value)
		wishspd = sv_maxairspeed.value;
	currentspeed = DotProduct (PRVM_serveredictvector(host_client->edict, velocity), wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = (sv_airaccelerate.value < 0 ? sv_accelerate.value : sv_airaccelerate.value)*(sv_gameplayfix_q2airaccelerate.integer ? wishspd : wishspeed) * sv.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		PRVM_serveredictvector(host_client->edict, velocity)[i] += accelspeed*wishveloc[i];
}


void DropPunchAngle (void)
{
	float len;
	vec3_t v;

	len = VectorNormalizeLength (PRVM_serveredictvector(host_client->edict, punchangle));

	len -= 10*sv.frametime;
	if (len < 0)
		len = 0;
	VectorScale (PRVM_serveredictvector(host_client->edict, punchangle), len, PRVM_serveredictvector(host_client->edict, punchangle));

	VectorCopy(PRVM_serveredictvector(host_client->edict, punchvector), v);
	len = VectorNormalizeLength(v);
	if (len > 0)
	{
		len -= 20*sv.frametime;
		if (len < 0)
			len = 0;
		VectorScale(v, len, v);
	}
	VectorCopy(v, PRVM_serveredictvector(host_client->edict, punchvector));
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

	AngleVectors (PRVM_serveredictvector(host_client->edict, v_angle), forward, right, up);

	for (i = 0; i < 3; i++)
		PRVM_serveredictvector(host_client->edict, velocity)[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	PRVM_serveredictvector(host_client->edict, velocity)[2] += cmd.upmove;

	wishspeed = VectorLength(PRVM_serveredictvector(host_client->edict, velocity));
	if (wishspeed > sv_maxspeed.value)
		VectorScale(PRVM_serveredictvector(host_client->edict, velocity), sv_maxspeed.value / wishspeed, PRVM_serveredictvector(host_client->edict, velocity));
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
	AngleVectors (PRVM_serveredictvector(host_client->edict, v_angle), forward, right, up);

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
	speed = VectorLength(PRVM_serveredictvector(host_client->edict, velocity));
	if (speed)
	{
		newspeed = speed - sv.frametime * speed * (sv_waterfriction.value < 0 ? sv_friction.value : sv_waterfriction.value);
		if (newspeed < 0)
			newspeed = 0;
		temp = newspeed/speed;
		VectorScale(PRVM_serveredictvector(host_client->edict, velocity), temp, PRVM_serveredictvector(host_client->edict, velocity));
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
		PRVM_serveredictvector(host_client->edict, velocity)[i] += accelspeed * wishvel[i];
}

void SV_WaterJump (void)
{
	if (sv.time > PRVM_serveredictfloat(host_client->edict, teleport_time) || !PRVM_serveredictfloat(host_client->edict, waterlevel))
	{
		PRVM_serveredictfloat(host_client->edict, flags) = (int)PRVM_serveredictfloat(host_client->edict, flags) & ~FL_WATERJUMP;
		PRVM_serveredictfloat(host_client->edict, teleport_time) = 0;
	}
	PRVM_serveredictvector(host_client->edict, velocity)[0] = PRVM_serveredictvector(host_client->edict, movedir)[0];
	PRVM_serveredictvector(host_client->edict, velocity)[1] = PRVM_serveredictvector(host_client->edict, movedir)[1];
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
	wishvel[1] = PRVM_serveredictvector(host_client->edict, angles)[1];
	AngleVectors (wishvel, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

// hack to not let you back into teleporter
	if (sv.time < PRVM_serveredictfloat(host_client->edict, teleport_time) && fmove < 0)
		fmove = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	if ((int)PRVM_serveredictfloat(host_client->edict, movetype) != MOVETYPE_WALK)
		wishvel[2] += cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		temp = sv_maxspeed.value/wishspeed;
		VectorScale (wishvel, temp, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	if (PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy (wishvel, PRVM_serveredictvector(host_client->edict, velocity));
	}
	else if (onground)
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

	//Con_Printf("clientthink for %ims\n", (int) (sv.frametime * 1000));

	SV_ApplyClientMove();
	// make sure the velocity is sane (not a NaN)
	SV_CheckVelocity(host_client->edict);

	// LordHavoc: QuakeC replacement for SV_ClientThink (player movement)
	if (PRVM_serverfunction(SV_PlayerPhysics) && sv_playerphysicsqc.integer)
	{
		PRVM_serverglobalfloat(time) = sv.time;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (PRVM_serverfunction(SV_PlayerPhysics), "QC function SV_PlayerPhysics is missing");
		SV_CheckVelocity(host_client->edict);
		return;
	}

	if (PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_NONE)
		return;

	onground = ((int)PRVM_serveredictfloat(host_client->edict, flags) & FL_ONGROUND) != 0;

	DropPunchAngle ();

	// if dead, behave differently
	if (PRVM_serveredictfloat(host_client->edict, health) <= 0)
		return;

	cmd = host_client->cmd;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	VectorAdd (PRVM_serveredictvector(host_client->edict, v_angle), PRVM_serveredictvector(host_client->edict, punchangle), v_angle);
	PRVM_serveredictvector(host_client->edict, angles)[ROLL] = V_CalcRoll (PRVM_serveredictvector(host_client->edict, angles), PRVM_serveredictvector(host_client->edict, velocity))*4;
	if (!PRVM_serveredictfloat(host_client->edict, fixangle))
	{
		PRVM_serveredictvector(host_client->edict, angles)[PITCH] = -v_angle[PITCH]/3;
		PRVM_serveredictvector(host_client->edict, angles)[YAW] = v_angle[YAW];
	}

	if ( (int)PRVM_serveredictfloat(host_client->edict, flags) & FL_WATERJUMP )
	{
		SV_WaterJump ();
		SV_CheckVelocity(host_client->edict);
		return;
	}

	/*
	// Player is (somehow) outside of the map, or flying, or noclipping
	if (PRVM_serveredictfloat(host_client->edict, movetype) != MOVETYPE_NOCLIP && (PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict)))
	//if (PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_NOCLIP || PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_FLY || SV_TestEntityPosition (host_client->edict))
	{
		SV_FreeMove ();
		return;
	}
	*/

	// walk
	if ((PRVM_serveredictfloat(host_client->edict, waterlevel) >= 2) && (PRVM_serveredictfloat(host_client->edict, movetype) != MOVETYPE_NOCLIP))
	{
		SV_WaterMove ();
		SV_CheckVelocity(host_client->edict);
		return;
	}

	SV_AirMove ();
	SV_CheckVelocity(host_client->edict);
}

/*
===================
SV_ReadClientMove
===================
*/
int sv_numreadmoves = 0;
usercmd_t sv_readmoves[CL_MAX_USERCMDS];
void SV_ReadClientMove (void)
{
	int i;
	usercmd_t newmove;
	usercmd_t *move = &newmove;

	memset(move, 0, sizeof(*move));

	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read ping time
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3 && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5 && sv.protocol != PROTOCOL_DARKPLACES6)
		move->sequence = MSG_ReadLong ();
	move->time = move->clienttime = MSG_ReadFloat ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);
	move->receivetime = (float)sv.time;

#if DEBUGMOVES
	Con_Printf("%s move%i #%i %ims (%ims) %i %i '%i %i %i' '%i %i %i'\n", move->time > move->receivetime ? "^3read future" : "^4read normal", sv_numreadmoves + 1, move->sequence, (int)floor((move->time - host_client->cmd.time) * 1000.0 + 0.5), (int)floor(move->time * 1000.0 + 0.5), move->impulse, move->buttons, (int)move->viewangles[0], (int)move->viewangles[1], (int)move->viewangles[2], (int)move->forwardmove, (int)move->sidemove, (int)move->upmove);
#endif
	// limit reported time to current time
	// (incase the client is trying to cheat)
	move->time = min(move->time, move->receivetime + sv.frametime);

	// read current angles
	for (i = 0;i < 3;i++)
	{
		if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
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
	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
		move->buttons = MSG_ReadByte ();
	else
		move->buttons = MSG_ReadLong ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// read impulse
	move->impulse = MSG_ReadByte ();
	if (msg_badread) Con_Printf("SV_ReadClientMessage: badread at %s:%i\n", __FILE__, __LINE__);

	// PRYDON_CLIENTCURSOR
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3 && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5)
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

	// if the previous move has not been applied yet, we need to accumulate
	// the impulse/buttons from it
	if (!host_client->cmd.applied)
	{
		if (!move->impulse)
			move->impulse = host_client->cmd.impulse;
		move->buttons |= host_client->cmd.buttons;
	}

	// now store this move for later execution
	// (we have to buffer the moves because of old ones being repeated)
	if (sv_numreadmoves < CL_MAX_USERCMDS)
		sv_readmoves[sv_numreadmoves++] = *move;

	// movement packet loss tracking
	if(move->sequence)
	{
		if(move->sequence > host_client->movement_highestsequence_seen)
		{
			if(host_client->movement_highestsequence_seen)
			{
				// mark moves in between as lost
				if(move->sequence - host_client->movement_highestsequence_seen - 1 < NETGRAPH_PACKETS)
					for(i = host_client->movement_highestsequence_seen + 1; i < move->sequence; ++i)
						host_client->movement_count[i % NETGRAPH_PACKETS] = -1;
				else
					memset(host_client->movement_count, -1, sizeof(host_client->movement_count));
			}
			// mark THIS move as seen for the first time
			host_client->movement_count[move->sequence % NETGRAPH_PACKETS] = 1;
			// update highest sequence seen
			host_client->movement_highestsequence_seen = move->sequence;
		}
		else
			if(host_client->movement_count[move->sequence % NETGRAPH_PACKETS] >= 0)
				++host_client->movement_count[move->sequence % NETGRAPH_PACKETS];
	}
	else
	{
		host_client->movement_highestsequence_seen = 0;
		memset(host_client->movement_count, 0, sizeof(host_client->movement_count));
	}
}

void SV_ExecuteClientMoves(void)
{
	int moveindex;
	float moveframetime;
	double oldframetime;
	double oldframetime2;
#ifdef NUM_PING_TIMES
	double total;
#endif
	if (sv_numreadmoves < 1)
		return;
	// only start accepting input once the player is spawned
	if (!host_client->spawned)
		return;
#if DEBUGMOVES
	Con_Printf("SV_ExecuteClientMoves: read %i moves at sv.time %f\n", sv_numreadmoves, (float)sv.time);
#endif
	// disable clientside movement prediction in some cases
	if (ceil(max(sv_readmoves[sv_numreadmoves-1].receivetime - sv_readmoves[sv_numreadmoves-1].time, 0) * 1000.0) < sv_clmovement_minping.integer)
		host_client->clmovement_disabletimeout = realtime + sv_clmovement_minping_disabletime.value / 1000.0;
	// several conditions govern whether clientside movement prediction is allowed
	if (sv_readmoves[sv_numreadmoves-1].sequence && sv_clmovement_enable.integer && sv_clmovement_inputtimeout.value > 0 && host_client->clmovement_disabletimeout <= realtime && PRVM_serveredictfloat(host_client->edict, movetype) == MOVETYPE_WALK && (!PRVM_serveredictfloat(host_client->edict, disableclientprediction)))
	{
		// process the moves in order and ignore old ones
		// but always trust the latest move
		// (this deals with bogus initial move sequences after level change,
		//  where the client will eventually catch up with the level change
		//  and reset its move sequence)
		for (moveindex = 0;moveindex < sv_numreadmoves;moveindex++)
		{
			usercmd_t *move = sv_readmoves + moveindex;
			if (host_client->movesequence < move->sequence || moveindex == sv_numreadmoves - 1)
			{
#if DEBUGMOVES
				Con_Printf("%smove #%i %ims (%ims) %i %i '%i %i %i' '%i %i %i'\n", (move->time - host_client->cmd.time) > sv.frametime * 1.01 ? "^1" : "^2", move->sequence, (int)floor((move->time - host_client->cmd.time) * 1000.0 + 0.5), (int)floor(move->time * 1000.0 + 0.5), move->impulse, move->buttons, (int)move->viewangles[0], (int)move->viewangles[1], (int)move->viewangles[2], (int)move->forwardmove, (int)move->sidemove, (int)move->upmove);
#endif
				// this is a new move
				move->time = bound(sv.time - 1, move->time, sv.time); // prevent slowhack/speedhack combos
				move->time = max(move->time, host_client->cmd.time); // prevent backstepping of time
				moveframetime = bound(0, move->time - host_client->cmd.time, min(0.1, sv_clmovement_inputtimeout.value));

				// discard (treat like lost) moves with too low distance from
				// the previous one to prevent hacks using float inaccuracy
				// clients will see this as packet loss in the netgraph
				// this should also apply if a move cannot get
				// executed because it came too late and
				// already was performed serverside
				if(moveframetime < 0.0005)
				{
					// count the move as LOST if we don't
					// execute it but it has higher
					// sequence count
					if(host_client->movesequence)
						if(move->sequence > host_client->movesequence)
							host_client->movement_count[(move->sequence) % NETGRAPH_PACKETS] = -1;
					continue;
				}

				//Con_Printf("movesequence = %i (%i lost), moveframetime = %f\n", move->sequence, move->sequence ? move->sequence - host_client->movesequence - 1 : 0, moveframetime);
				host_client->cmd = *move;
				host_client->movesequence = move->sequence;

				// if using prediction, we need to perform moves when packets are
				// received, even if multiple occur in one frame
				// (they can't go beyond the current time so there is no cheat issue
				//  with this approach, and if they don't send input for a while they
				//  start moving anyway, so the longest 'lagaport' possible is
				//  determined by the sv_clmovement_inputtimeout cvar)
				if (moveframetime <= 0)
					continue;
				oldframetime = PRVM_serverglobalfloat(frametime);
				oldframetime2 = sv.frametime;
				// update ping time for qc to see while executing this move
				host_client->ping = host_client->cmd.receivetime - host_client->cmd.time;
				// the server and qc frametime values must be changed temporarily
				PRVM_serverglobalfloat(frametime) = sv.frametime = moveframetime;
				// if move is more than 50ms, split it into two moves (this matches QWSV behavior and the client prediction)
				if (sv.frametime > 0.05)
				{
					PRVM_serverglobalfloat(frametime) = sv.frametime = moveframetime * 0.5f;
					SV_Physics_ClientMove();
				}
				SV_Physics_ClientMove();
				sv.frametime = oldframetime2;
				PRVM_serverglobalfloat(frametime) = oldframetime;
				host_client->clmovement_inputtimeout = sv_clmovement_inputtimeout.value;
			}
		}
	}
	else
	{
		// try to gather button bits from old moves, but only if their time is
		// advancing (ones with the same timestamp can't be trusted)
		for (moveindex = 0;moveindex < sv_numreadmoves-1;moveindex++)
		{
			usercmd_t *move = sv_readmoves + moveindex;
			if (host_client->cmd.time < move->time)
			{
				sv_readmoves[sv_numreadmoves-1].buttons |= move->buttons;
				if (move->impulse)
					sv_readmoves[sv_numreadmoves-1].impulse = move->impulse;
			}
		}
		// now copy the new move
		host_client->cmd = sv_readmoves[sv_numreadmoves-1];
		host_client->cmd.time = max(host_client->cmd.time, sv.time);
			// physics will run up to sv.time, so allow no predicted moves
			// before that otherwise, there is a speedhack by turning
			// prediction on and off repeatedly on client side because the
			// engine would run BOTH client and server physics for the same
			// time
		host_client->movesequence = 0;
		// make sure that normal physics takes over immediately
		host_client->clmovement_inputtimeout = 0;
	}

	// calculate average ping time
	host_client->ping = host_client->cmd.receivetime - host_client->cmd.clienttime;
#ifdef NUM_PING_TIMES
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] = host_client->cmd.receivetime - host_client->cmd.clienttime;
	host_client->num_pings++;
	for (i=0, total = 0;i < NUM_PING_TIMES;i++)
		total += host_client->ping_times[i];
	host_client->ping = total / NUM_PING_TIMES;
#endif
}

void SV_ApplyClientMove (void)
{
	usercmd_t *move = &host_client->cmd;
	int j, movementloss, packetloss;

	if (!move->receivetime)
		return;

	// note: a move can be applied multiple times if the client packets are
	// not coming as often as the physics is executed, and the move must be
	// applied before running qc each time because the id1 qc had a bug where
	// it clears self.button2 in PlayerJump, causing pogostick behavior if
	// moves are not applied every time before calling qc
	move->applied = true;

	// set the edict fields
	PRVM_serveredictfloat(host_client->edict, button0) = move->buttons & 1;
	PRVM_serveredictfloat(host_client->edict, button2) = (move->buttons & 2)>>1;
	if (move->impulse)
		PRVM_serveredictfloat(host_client->edict, impulse) = move->impulse;
	// only send the impulse to qc once
	move->impulse = 0;

	movementloss = packetloss = 0;
	if(host_client->netconnection)
	{
		for (j = 0;j < NETGRAPH_PACKETS;j++)
			if (host_client->netconnection->incoming_netgraph[j].unreliablebytes == NETGRAPH_LOSTPACKET)
				packetloss++;
		for (j = 0;j < NETGRAPH_PACKETS;j++)
			if (host_client->movement_count[j] < 0)
				movementloss++;
	}

	VectorCopy(move->viewangles, PRVM_serveredictvector(host_client->edict, v_angle));
	PRVM_serveredictfloat(host_client->edict, button3) = ((move->buttons >> 2) & 1);
	PRVM_serveredictfloat(host_client->edict, button4) = ((move->buttons >> 3) & 1);
	PRVM_serveredictfloat(host_client->edict, button5) = ((move->buttons >> 4) & 1);
	PRVM_serveredictfloat(host_client->edict, button6) = ((move->buttons >> 5) & 1);
	PRVM_serveredictfloat(host_client->edict, button7) = ((move->buttons >> 6) & 1);
	PRVM_serveredictfloat(host_client->edict, button8) = ((move->buttons >> 7) & 1);
	PRVM_serveredictfloat(host_client->edict, button9) = ((move->buttons >> 11) & 1);
	PRVM_serveredictfloat(host_client->edict, button10) = ((move->buttons >> 12) & 1);
	PRVM_serveredictfloat(host_client->edict, button11) = ((move->buttons >> 13) & 1);
	PRVM_serveredictfloat(host_client->edict, button12) = ((move->buttons >> 14) & 1);
	PRVM_serveredictfloat(host_client->edict, button13) = ((move->buttons >> 15) & 1);
	PRVM_serveredictfloat(host_client->edict, button14) = ((move->buttons >> 16) & 1);
	PRVM_serveredictfloat(host_client->edict, button15) = ((move->buttons >> 17) & 1);
	PRVM_serveredictfloat(host_client->edict, button16) = ((move->buttons >> 18) & 1);
	PRVM_serveredictfloat(host_client->edict, buttonuse) = ((move->buttons >> 8) & 1);
	PRVM_serveredictfloat(host_client->edict, buttonchat) = ((move->buttons >> 9) & 1);
	PRVM_serveredictfloat(host_client->edict, cursor_active) = ((move->buttons >> 10) & 1);
	VectorSet(PRVM_serveredictvector(host_client->edict, movement), move->forwardmove, move->sidemove, move->upmove);
	VectorCopy(move->cursor_screen, PRVM_serveredictvector(host_client->edict, cursor_screen));
	VectorCopy(move->cursor_start, PRVM_serveredictvector(host_client->edict, cursor_trace_start));
	VectorCopy(move->cursor_impact, PRVM_serveredictvector(host_client->edict, cursor_trace_endpos));
	PRVM_serveredictedict(host_client->edict, cursor_trace_ent) = PRVM_EDICT_TO_PROG(PRVM_EDICT_NUM(move->cursor_entitynumber));
	PRVM_serveredictfloat(host_client->edict, ping) = host_client->ping * 1000.0;
	PRVM_serveredictfloat(host_client->edict, ping_packetloss) = packetloss / (float) NETGRAPH_PACKETS;
	PRVM_serveredictfloat(host_client->edict, ping_movementloss) = movementloss / (float) NETGRAPH_PACKETS;
}

void SV_FrameLost(int framenum)
{
	if (host_client->entitydatabase5)
	{
		EntityFrame5_LostFrame(host_client->entitydatabase5, framenum);
		EntityFrameCSQC_LostFrame(host_client, framenum);
	}
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
	char *s, *p, *q;

	if(sv_autodemo_perclient.integer >= 2)
		SV_WriteDemoMessage(host_client, &(host_client->netconnection->message), true);

	//MSG_BeginReading ();
	sv_numreadmoves = 0;

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
			// apply the moves that were read this frame
			SV_ExecuteClientMoves();
			break;
		}

		switch (cmd)
		{
		default:
			Con_Printf("SV_ReadClientMessage: unknown command char %i (at offset 0x%x)\n", cmd, msg_readcount);
			if (developer_networking.integer)
				Com_HexDumpToConsole(net_message.data, net_message.cursize);
			SV_DropClient (false);
			return;

		case clc_nop:
			break;

		case clc_stringcmd:
			// allow reliable messages now as the client is done with initial loading
			if (host_client->sendsignon == 2)
				host_client->sendsignon = 0;
			s = MSG_ReadString ();
			q = NULL;
			for(p = s; *p; ++p) switch(*p)
			{
				case 10:
				case 13:
					if(!q)
						q = p;
					break;
				default:
					if(q)
						goto clc_stringcmd_invalid; // newline seen, THEN something else -> possible exploit
					break;
			}
			if(q)
				*q = 0;
			if (strncasecmp(s, "spawn", 5) == 0
			 || strncasecmp(s, "begin", 5) == 0
			 || strncasecmp(s, "prespawn", 8) == 0)
				Cmd_ExecuteString (s, src_client);
			else if (PRVM_serverfunction(SV_ParseClientCommand))
			{
				int restorevm_tempstringsbuf_cursize;
				restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
				PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(s);
				PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
				PRVM_ExecuteProgram (PRVM_serverfunction(SV_ParseClientCommand), "QC function SV_ParseClientCommand is missing");
				vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
			}
			else
				Cmd_ExecuteString (s, src_client);
			break;

clc_stringcmd_invalid:
			Con_Printf("Received invalid stringcmd from %s\n", host_client->name);
			if(developer.integer > 0)
				Com_HexDumpToConsole((unsigned char *) s, strlen(s));
			break;

		case clc_disconnect:
			SV_DropClient (false); // client wants to disconnect
			return;

		case clc_move:
			SV_ReadClientMove();
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
						temp = (unsigned char *) Mem_Alloc(tempmempool, size);
						FS_Read(host_client->download_file, temp, size);
						crc = CRC_Block(temp, size);
						Mem_Free(temp);
						// calculated crc, send the file info to the client
						// (so that it can verify the data)
						Host_ClientCommands("\ncl_downloadfinished %i %i %s\n", size, crc, host_client->download_name);
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
			if (developer_networkentities.integer >= 10)
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

