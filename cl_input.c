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
// cl.input.c  -- builds an intended movement command to send to the server

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t	in_mlook, in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_jump, in_attack, in_use;
kbutton_t	in_up, in_down;
// LordHavoc: added 6 new buttons
kbutton_t	in_button3, in_button4, in_button5, in_button6, in_button7, in_button8;

int			in_impulse;

extern cvar_t sys_ticrate;


void KeyDown (kbutton_t *b)
{
	int k;
	const char *c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Con_Print("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int k;
	const char *c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_MLookUp (void)
{
	KeyUp(&in_mlook);
	if ( !(in_mlook.state&1) && lookspring.value)
		V_StartPitchDrift();
}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown(void) {KeyDown(&in_use);}
void IN_UseUp(void) {KeyUp(&in_use);}

// LordHavoc: added 6 new buttons
void IN_Button3Down(void) {KeyDown(&in_button3);}
void IN_Button3Up(void) {KeyUp(&in_button3);}
void IN_Button4Down(void) {KeyDown(&in_button4);}
void IN_Button4Up(void) {KeyUp(&in_button4);}
void IN_Button5Down(void) {KeyDown(&in_button5);}
void IN_Button5Up(void) {KeyUp(&in_button5);}
void IN_Button6Down(void) {KeyDown(&in_button6);}
void IN_Button6Up(void) {KeyUp(&in_button6);}
void IN_Button7Down(void) {KeyDown(&in_button7);}
void IN_Button7Up(void) {KeyUp(&in_button7);}
void IN_Button8Down(void) {KeyDown(&in_button8);}
void IN_Button8Up(void) {KeyUp(&in_button8);}

void IN_JumpDown (void) {KeyDown(&in_jump);}
void IN_JumpUp (void) {KeyUp(&in_jump);}

void IN_Impulse (void) {in_impulse=atoi(Cmd_Argv(1));}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	qboolean	impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;

	if (impulsedown && !impulseup)
	{
		if (down)
			val = 0.5;	// pressed and held this frame
		else
			val = 0;	//	I_Error ();
	}
	if (impulseup && !impulsedown)
	{
		if (down)
			val = 0;	//	I_Error ();
		else
			val = 0;	// released this frame
	}
	if (!impulsedown && !impulseup)
	{
		if (down)
			val = 1.0;	// held the entire frame
		else
			val = 0;	// up the entire frame
	}
	if (impulsedown && impulseup)
	{
		if (down)
			val = 0.75;	// released and re-pressed this frame
		else
			val = 0.25;	// pressed and released this frame
	}

	key->state &= 1;		// clear impulses

	return val;
}




//==========================================================================

cvar_t cl_upspeed = {CVAR_SAVE, "cl_upspeed","400"};
cvar_t cl_forwardspeed = {CVAR_SAVE, "cl_forwardspeed","400"};
cvar_t cl_backspeed = {CVAR_SAVE, "cl_backspeed","400"};
cvar_t cl_sidespeed = {CVAR_SAVE, "cl_sidespeed","350"};

cvar_t cl_movespeedkey = {CVAR_SAVE, "cl_movespeedkey","2.0"};

cvar_t cl_yawspeed = {CVAR_SAVE, "cl_yawspeed","140"};
cvar_t cl_pitchspeed = {CVAR_SAVE, "cl_pitchspeed","150"};

cvar_t cl_anglespeedkey = {CVAR_SAVE, "cl_anglespeedkey","1.5"};

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (void)
{
	float	speed;
	float	up, down;

	if (in_speed.state & 1)
		speed = host_realframetime * cl_anglespeedkey.value;
	else
		speed = host_realframetime;

	if (!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed*cl_yawspeed.value*CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed*cl_yawspeed.value*CL_KeyState (&in_left);
	}
	if (in_klook.state & 1)
	{
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -= speed*cl_pitchspeed.value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += speed*cl_pitchspeed.value * CL_KeyState (&in_back);
	}

	up = CL_KeyState (&in_lookup);
	down = CL_KeyState(&in_lookdown);

	cl.viewangles[PITCH] -= speed*cl_pitchspeed.value * up;
	cl.viewangles[PITCH] += speed*cl_pitchspeed.value * down;

	if (up || down)
		V_StopPitchDrift ();

	cl.viewangles[YAW] = ANGLEMOD(cl.viewangles[YAW]);
	cl.viewangles[PITCH] = ANGLEMOD(cl.viewangles[PITCH]);
	cl.viewangles[ROLL] = ANGLEMOD(cl.viewangles[ROLL]);
	if (cl.viewangles[YAW] >= 180)
		cl.viewangles[YAW] -= 360;
	if (cl.viewangles[PITCH] >= 180)
		cl.viewangles[PITCH] -= 360;
	if (cl.viewangles[ROLL] >= 180)
		cl.viewangles[ROLL] -= 360;

	cl.viewangles[PITCH] = bound (in_pitch_min.value, cl.viewangles[PITCH], in_pitch_max.value);
	cl.viewangles[ROLL] = bound(-50, cl.viewangles[ROLL], 50);
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (void)
{
	vec3_t temp;
	if (cls.signon != SIGNONS)
		return;

	CL_AdjustAngles ();

	// PRYDON_CLIENTCURSOR needs to survive basemove resets
	VectorCopy (cl.cmd.cursor_screen, temp);
	memset (&cl.cmd, 0, sizeof(cl.cmd));
	VectorCopy (temp, cl.cmd.cursor_screen);

	if (in_strafe.state & 1)
	{
		cl.cmd.sidemove += cl_sidespeed.value * CL_KeyState (&in_right);
		cl.cmd.sidemove -= cl_sidespeed.value * CL_KeyState (&in_left);
	}

	cl.cmd.sidemove += cl_sidespeed.value * CL_KeyState (&in_moveright);
	cl.cmd.sidemove -= cl_sidespeed.value * CL_KeyState (&in_moveleft);

	cl.cmd.upmove += cl_upspeed.value * CL_KeyState (&in_up);
	cl.cmd.upmove -= cl_upspeed.value * CL_KeyState (&in_down);

	if (! (in_klook.state & 1) )
	{
		cl.cmd.forwardmove += cl_forwardspeed.value * CL_KeyState (&in_forward);
		cl.cmd.forwardmove -= cl_backspeed.value * CL_KeyState (&in_back);
	}

//
// adjust for speed key
//
	if (in_speed.state & 1)
	{
		cl.cmd.forwardmove *= cl_movespeedkey.value;
		cl.cmd.sidemove *= cl_movespeedkey.value;
		cl.cmd.upmove *= cl_movespeedkey.value;
	}
}


#include "cl_collision.h"

void CL_UpdatePrydonCursor(void)
{
	vec3_t temp, scale;

	if (!cl_prydoncursor.integer)
		VectorClear(cl.cmd.cursor_screen);

	/*
	if (cl.cmd.cursor_screen[0] < -1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - -1) * vid.realwidth * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = -1;
	}
	if (cl.cmd.cursor_screen[0] > 1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - 1) * vid.realwidth * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = 1;
	}
	if (cl.cmd.cursor_screen[1] < -1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - -1) * vid.realheight * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = -1;
	}
	if (cl.cmd.cursor_screen[1] > 1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - 1) * vid.realheight * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = 1;
	}
	*/
	cl.cmd.cursor_screen[0] = bound(-1, cl.cmd.cursor_screen[0], 1);
	cl.cmd.cursor_screen[1] = bound(-1, cl.cmd.cursor_screen[1], 1);
	cl.cmd.cursor_screen[2] = 1;

	scale[0] = -tan(r_refdef.fov_x * M_PI / 360.0);
	scale[1] = -tan(r_refdef.fov_y * M_PI / 360.0);
	scale[2] = 1;

	// trace distance
	VectorScale(scale, 1000000, scale);

	// FIXME: use something other than renderer variables here
	// (but they need to match)
	VectorCopy(r_vieworigin, cl.cmd.cursor_start);
	VectorSet(temp, cl.cmd.cursor_screen[2] * scale[2], cl.cmd.cursor_screen[0] * scale[0], cl.cmd.cursor_screen[1] * scale[1]);
	Matrix4x4_Transform(&r_view_matrix, temp, cl.cmd.cursor_end);
	cl.cmd.cursor_fraction = CL_SelectTraceLine(cl.cmd.cursor_start, cl.cmd.cursor_end, cl.cmd.cursor_impact, cl.cmd.cursor_normal, &cl.cmd.cursor_entitynumber, (chase_active.integer || cl.intermission) ? &cl_entities[cl.playerentity].render : NULL);
	// makes sparks where cursor is
	//CL_SparkShower(cl.cmd.cursor_impact, cl.cmd.cursor_normal, 5, 0);
}

/*
==============
CL_SendMove
==============
*/
void CL_SendMove(void)
{
	int i;
	int bits;
	sizebuf_t buf;
	qbyte data[128];
	static double nextmovetime = 0;
#define MOVEAVERAGING 0
#if MOVEAVERAGING
	static float forwardmove, sidemove, upmove, total; // accumulation
#else
	float forwardmove, sidemove, upmove;
#endif

	CL_UpdatePrydonCursor();

#if MOVEAVERAGING
	// accumulate changes between messages
	forwardmove += cl.cmd.forwardmove;
	sidemove += cl.cmd.sidemove;
	upmove += cl.cmd.upmove;
	total++;
#endif
	// LordHavoc: cap outgoing movement messages to sys_ticrate
	nextmovetime = bound(realtime - sys_ticrate.value, nextmovetime, realtime + sys_ticrate.value);
	if (!cl.islocalgame && realtime < nextmovetime)
		return;
	nextmovetime += sys_ticrate.value;
#if MOVEAVERAGING
	// average the accumulated changes
	total = 1.0f / total;
	forwardmove *= total;
	sidemove *= total;
	upmove *= total;
	total = 0;
#else
	// use the latest values
	forwardmove = cl.cmd.forwardmove;
	sidemove = cl.cmd.sidemove;
	upmove = cl.cmd.upmove;
#endif

	buf.maxsize = 128;
	buf.cursize = 0;
	buf.data = data;

	// set button bits
	// LordHavoc: added 6 new buttons and use and chat buttons, and prydon cursor active button
	bits = 0;
	if (in_attack.state   & 3) bits |=   1;in_attack.state  &= ~2;
	if (in_jump.state     & 3) bits |=   2;in_jump.state    &= ~2;
	if (in_button3.state  & 3) bits |=   4;in_button3.state &= ~2;
	if (in_button4.state  & 3) bits |=   8;in_button4.state &= ~2;
	if (in_button5.state  & 3) bits |=  16;in_button5.state &= ~2;
	if (in_button6.state  & 3) bits |=  32;in_button6.state &= ~2;
	if (in_button7.state  & 3) bits |=  64;in_button7.state &= ~2;
	if (in_button8.state  & 3) bits |= 128;in_button8.state &= ~2;
	if (in_use.state      & 3) bits |= 256;in_use.state     &= ~2;
	if (key_dest != key_game || key_consoleactive) bits |= 512;
	if (cl_prydoncursor.integer) bits |= 1024;
	// button bits 11-31 unused currently
	// rotate/zoom view serverside if PRYDON_CLIENTCURSOR cursor is at edge of screen
	if (cl.cmd.cursor_screen[0] <= -1) bits |= 8;
	if (cl.cmd.cursor_screen[0] >=  1) bits |= 16;
	if (cl.cmd.cursor_screen[1] <= -1) bits |= 32;
	if (cl.cmd.cursor_screen[1] >=  1) bits |= 64;

	// always dump the first two messages, because they may contain leftover inputs from the last level
	if (++cl.movemessages >= 2)
	{
		// send the movement message
		// PROTOCOL_QUAKE       clc_move = 16 bytes total
		// PROTOCOL_DARKPLACES1 clc_move = 19 bytes total
		// PROTOCOL_DARKPLACES2 clc_move = 25 bytes total
		// PROTOCOL_DARKPLACES3 clc_move = 25 bytes total
		// PROTOCOL_DARKPLACES4 clc_move = 19 bytes total
		// PROTOCOL_DARKPLACES5 clc_move = 19 bytes total
		// PROTOCOL_DARKPLACES6 clc_move = 52 bytes total
		// 5 bytes
		MSG_WriteByte (&buf, clc_move);
		MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times
		if (cl.protocol == PROTOCOL_DARKPLACES6)
		{
			// 6 bytes
			for (i = 0;i < 3;i++)
				MSG_WriteAngle16i (&buf, cl.viewangles[i]);
			// 6 bytes
			MSG_WriteCoord16i (&buf, forwardmove);
			MSG_WriteCoord16i (&buf, sidemove);
			MSG_WriteCoord16i (&buf, upmove);
			// 5 bytes
			MSG_WriteLong (&buf, bits);
			MSG_WriteByte (&buf, in_impulse);
			// PRYDON_CLIENTCURSOR
			// 30 bytes
			MSG_WriteShort (&buf, cl.cmd.cursor_screen[0] * 32767.0f);
			MSG_WriteShort (&buf, cl.cmd.cursor_screen[1] * 32767.0f);
			MSG_WriteFloat (&buf, cl.cmd.cursor_start[0]);
			MSG_WriteFloat (&buf, cl.cmd.cursor_start[1]);
			MSG_WriteFloat (&buf, cl.cmd.cursor_start[2]);
			MSG_WriteFloat (&buf, cl.cmd.cursor_impact[0]);
			MSG_WriteFloat (&buf, cl.cmd.cursor_impact[1]);
			MSG_WriteFloat (&buf, cl.cmd.cursor_impact[2]);
			MSG_WriteShort (&buf, cl.cmd.cursor_entitynumber);
		}
		else
		{
			if (cl.protocol == PROTOCOL_QUAKE || cl.protocol == PROTOCOL_NEHAHRAMOVIE)
			{
				// 3 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle8i (&buf, cl.viewangles[i]);
			}
			else if (cl.protocol == PROTOCOL_DARKPLACES2 || cl.protocol == PROTOCOL_DARKPLACES3)
			{
				// 12 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle32f (&buf, cl.viewangles[i]);
			}
			else if (cl.protocol == PROTOCOL_DARKPLACES1 || cl.protocol == PROTOCOL_DARKPLACES4 || cl.protocol == PROTOCOL_DARKPLACES5)
			{
				// 6 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle16i (&buf, cl.viewangles[i]);
			}
			else
				Host_Error("CL_SendMove: unknown cl.protocol %i\n", cl.protocol);
			// 6 bytes
			MSG_WriteCoord16i (&buf, forwardmove);
			MSG_WriteCoord16i (&buf, sidemove);
			MSG_WriteCoord16i (&buf, upmove);
			// 2 bytes
			MSG_WriteByte (&buf, bits);
			MSG_WriteByte (&buf, in_impulse);
		}
	}

#if MOVEAVERAGING
	forwardmove = sidemove = upmove = 0;
#endif
	in_impulse = 0;

	// ack the last few frame numbers
	// (redundent to improve handling of client->server packet loss)
	// for LATESTFRAMENUMS == 3 case this is 15 bytes
	for (i = 0;i < LATESTFRAMENUMS;i++)
	{
		if (cl.latestframenums[i] > 0)
		{
			if (developer_networkentities.integer >= 1)
				Con_Printf("send clc_ackframe %i\n", cl.latestframenums[i]);
			MSG_WriteByte(&buf, clc_ackframe);
			MSG_WriteLong(&buf, cl.latestframenums[i]);
		}
	}

	// PROTOCOL_DARKPLACES6 = 67 bytes per packet

	// deliver the message
	if (cls.demoplayback)
		return;
	// nothing to send
	if (!buf.cursize)
		return;

	if (NetConn_SendUnreliableMessage(cls.netcon, &buf) == -1)
	{
		Con_Print("CL_SendMove: lost server connection\n");
		CL_Disconnect();
		Host_ShutdownServer(false);
	}
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+jump", IN_JumpDown);
	Cmd_AddCommand ("-jump", IN_JumpUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	// LordHavoc: added use button
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);

	// LordHavoc: added 6 new buttons
	Cmd_AddCommand ("+button3", IN_Button3Down);
	Cmd_AddCommand ("-button3", IN_Button3Up);
	Cmd_AddCommand ("+button4", IN_Button4Down);
	Cmd_AddCommand ("-button4", IN_Button4Up);
	Cmd_AddCommand ("+button5", IN_Button5Down);
	Cmd_AddCommand ("-button5", IN_Button5Up);
	Cmd_AddCommand ("+button6", IN_Button6Down);
	Cmd_AddCommand ("-button6", IN_Button6Up);
	Cmd_AddCommand ("+button7", IN_Button7Down);
	Cmd_AddCommand ("-button7", IN_Button7Up);
	Cmd_AddCommand ("+button8", IN_Button8Down);
	Cmd_AddCommand ("-button8", IN_Button8Up);
}

