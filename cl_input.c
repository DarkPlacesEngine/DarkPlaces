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
#include "csprogs.h"

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
//even more
kbutton_t	in_button9, in_button10, in_button11, in_button12, in_button13, in_button14, in_button15, in_button16;

int			in_impulse;



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

void IN_Button9Down(void) {KeyDown(&in_button9);}
void IN_Button9Up(void) {KeyUp(&in_button9);}
void IN_Button10Down(void) {KeyDown(&in_button10);}
void IN_Button10Up(void) {KeyUp(&in_button10);}
void IN_Button11Down(void) {KeyDown(&in_button11);}
void IN_Button11Up(void) {KeyUp(&in_button11);}
void IN_Button12Down(void) {KeyDown(&in_button12);}
void IN_Button12Up(void) {KeyUp(&in_button12);}
void IN_Button13Down(void) {KeyDown(&in_button13);}
void IN_Button13Up(void) {KeyUp(&in_button13);}
void IN_Button14Down(void) {KeyDown(&in_button14);}
void IN_Button14Up(void) {KeyUp(&in_button14);}
void IN_Button15Down(void) {KeyDown(&in_button15);}
void IN_Button15Up(void) {KeyUp(&in_button15);}
void IN_Button16Down(void) {KeyDown(&in_button16);}
void IN_Button16Up(void) {KeyUp(&in_button16);}

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

cvar_t cl_upspeed = {CVAR_SAVE, "cl_upspeed","400","vertical movement speed (while swimming or flying)"};
cvar_t cl_forwardspeed = {CVAR_SAVE, "cl_forwardspeed","400","forward movement speed"};
cvar_t cl_backspeed = {CVAR_SAVE, "cl_backspeed","400","backward movement speed"};
cvar_t cl_sidespeed = {CVAR_SAVE, "cl_sidespeed","350","strafe movement speed"};

cvar_t cl_movespeedkey = {CVAR_SAVE, "cl_movespeedkey","2.0","how much +speed multiplies keyboard movement speed"};

cvar_t cl_yawspeed = {CVAR_SAVE, "cl_yawspeed","140","keyboard yaw turning speed"};
cvar_t cl_pitchspeed = {CVAR_SAVE, "cl_pitchspeed","150","keyboard pitch turning speed"};

cvar_t cl_anglespeedkey = {CVAR_SAVE, "cl_anglespeedkey","1.5","how much +speed multiplies keyboard turning speed"};

cvar_t cl_movement = {CVAR_SAVE, "cl_movement", "0", "enables clientside prediction of your player movement"};
cvar_t cl_movement_latency = {0, "cl_movement_latency", "0", "compensates for this much latency (ping time) on quake servers which do not really support prediction, no effect on darkplaces7 protocol servers"};
cvar_t cl_movement_maxspeed = {0, "cl_movement_maxspeed", "320", "how fast you can move (should match sv_maxspeed)"};
cvar_t cl_movement_maxairspeed = {0, "cl_movement_maxairspeed", "30", "how fast you can move while in the air (should match sv_maxairspeed)"};
cvar_t cl_movement_stopspeed = {0, "cl_movement_stopspeed", "100", "speed below which you will be slowed rapidly to a stop rather than sliding endlessly (should match sv_stopspeed)"};
cvar_t cl_movement_friction = {0, "cl_movement_friction", "4", "how fast you slow down (should match sv_friction)"};
cvar_t cl_movement_edgefriction = {0, "cl_movement_edgefriction", "2", "how much to slow down when you may be about to fall off a ledge (should match edgefriction)"};
cvar_t cl_movement_stepheight = {0, "cl_movement_stepheight", "18", "how tall a step you can step in one instant (should match sv_stepheight)"};
cvar_t cl_movement_accelerate = {0, "cl_movement_accelerate", "10", "how fast you accelerate (should match sv_accelerate)"};
cvar_t cl_movement_jumpvelocity = {0, "cl_movement_jumpvelocity", "270", "how fast you move upward when you begin a jump (should match the quakec code)"};
cvar_t cl_gravity = {0, "cl_gravity", "800", "how much gravity to apply in client physics (should match sv_gravity)"};
cvar_t cl_slowmo = {0, "cl_slowmo", "1", "speed of game time (should match slowmo)"};

cvar_t in_pitch_min = {0, "in_pitch_min", "-90", "how far downward you can aim (quake used -70"}; // quake used -70
cvar_t in_pitch_max = {0, "in_pitch_max", "90", "how far upward you can aim (quake used 80"}; // quake used 80

cvar_t m_filter = {CVAR_SAVE, "m_filter","0", "smoothes mouse movement, less responsive but smoother aiming"};

cvar_t cl_netinputpacketspersecond = {CVAR_SAVE, "cl_netinputpacketspersecond","50", "how many input packets to send to server each second"};

cvar_t cl_nodelta = {0, "cl_nodelta", "0", "disables delta compression of non-player entities in QW network protocol"};


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

qboolean cl_ignoremousemove = false;

/*
================
CL_Move

Send the intended movement message to the server
================
*/
void CL_Move (void)
{
	vec3_t temp;
	float mx, my;
	static float old_mouse_x = 0, old_mouse_y = 0;

	// clamp before the move to prevent starting with bad angles
	CL_AdjustAngles ();

	// get basic movement from keyboard
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

	// adjust for speed key
	if (in_speed.state & 1)
	{
		cl.cmd.forwardmove *= cl_movespeedkey.value;
		cl.cmd.sidemove *= cl_movespeedkey.value;
		cl.cmd.upmove *= cl_movespeedkey.value;
	}

	in_mouse_x = 0;
	in_mouse_y = 0;

	// allow mice or other external controllers to add to the move
	IN_Move ();

	// ignore a mouse move if mouse was activated/deactivated this frame
	if (cl_ignoremousemove)
	{
		cl_ignoremousemove = false;
		in_mouse_x = 0;
		in_mouse_y = 0;
	}

	// apply m_filter if it is on
	mx = in_mouse_x;
	my = in_mouse_y;
	if (m_filter.integer)
	{
		in_mouse_x = (mx + old_mouse_x) * 0.5;
		in_mouse_y = (my + old_mouse_y) * 0.5;
	}
	old_mouse_x = mx;
	old_mouse_y = my;

	// if not in menu, apply mouse move to viewangles/movement
	if (!cl.csqc_wantsmousemove && in_client_mouse)
	{
		if (cl_prydoncursor.integer)
		{
			// mouse interacting with the scene, mostly stationary view
			V_StopPitchDrift();
			cl.cmd.cursor_screen[0] += in_mouse_x * sensitivity.value / vid.width;
			cl.cmd.cursor_screen[1] += in_mouse_y * sensitivity.value / vid.height;
		}
		else if (in_strafe.state & 1)
		{
			// strafing mode, all looking is movement
			V_StopPitchDrift();
			cl.cmd.sidemove += m_side.value * in_mouse_x * sensitivity.value;
			if (noclip_anglehack)
				cl.cmd.upmove -= m_forward.value * in_mouse_y * sensitivity.value;
			else
				cl.cmd.forwardmove -= m_forward.value * in_mouse_y * sensitivity.value;
		}
		else if ((in_mlook.state & 1) || freelook.integer)
		{
			// mouselook, lookstrafe causes turning to become strafing
			V_StopPitchDrift();
			if (lookstrafe.integer)
				cl.cmd.sidemove += m_side.value * in_mouse_x * sensitivity.value;
			else
				cl.viewangles[YAW] -= m_yaw.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			cl.viewangles[PITCH] += m_pitch.value * in_mouse_y * sensitivity.value * cl.viewzoom;
		}
		else
		{
			// non-mouselook, yaw turning and forward/back movement
			cl.viewangles[YAW] -= m_yaw.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			cl.cmd.forwardmove -= m_forward.value * in_mouse_y * sensitivity.value;
		}
	}

	// clamp after the move to prevent rendering with bad angles
	CL_AdjustAngles ();
}

#include "cl_collision.h"

extern void V_CalcRefdef(void);
void CL_UpdatePrydonCursor(void)
{
	vec3_t temp, scale;

	if (!cl_prydoncursor.integer)
		VectorClear(cl.cmd.cursor_screen);

	/*
	if (cl.cmd.cursor_screen[0] < -1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - -1) * vid.width * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = -1;
	}
	if (cl.cmd.cursor_screen[0] > 1)
	{
		cl.viewangles[YAW] -= m_yaw.value * (cl.cmd.cursor_screen[0] - 1) * vid.width * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[0] = 1;
	}
	if (cl.cmd.cursor_screen[1] < -1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - -1) * vid.height * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = -1;
	}
	if (cl.cmd.cursor_screen[1] > 1)
	{
		cl.viewangles[PITCH] += m_pitch.value * (cl.cmd.cursor_screen[1] - 1) * vid.height * sensitivity.value * cl.viewzoom;
		cl.cmd.cursor_screen[1] = 1;
	}
	*/
	cl.cmd.cursor_screen[0] = bound(-1, cl.cmd.cursor_screen[0], 1);
	cl.cmd.cursor_screen[1] = bound(-1, cl.cmd.cursor_screen[1], 1);
	cl.cmd.cursor_screen[2] = 1;

	scale[0] = -r_refdef.frustum_x;
	scale[1] = -r_refdef.frustum_y;
	scale[2] = 1;

	// trace distance
	VectorScale(scale, 1000000, scale);

	// calculate current view matrix
	V_CalcRefdef();
	VectorClear(temp);
	Matrix4x4_Transform(&r_refdef.viewentitymatrix, temp, cl.cmd.cursor_start);
	VectorSet(temp, cl.cmd.cursor_screen[2] * scale[2], cl.cmd.cursor_screen[0] * scale[0], cl.cmd.cursor_screen[1] * scale[1]);
	Matrix4x4_Transform(&r_refdef.viewentitymatrix, temp, cl.cmd.cursor_end);
	// trace from view origin to the cursor
	cl.cmd.cursor_fraction = CL_SelectTraceLine(cl.cmd.cursor_start, cl.cmd.cursor_end, cl.cmd.cursor_impact, cl.cmd.cursor_normal, &cl.cmd.cursor_entitynumber, (chase_active.integer || cl.intermission) ? &cl.entities[cl.playerentity].render : NULL, false);
	// makes sparks where cursor is
	//CL_SparkShower(cl.cmd.cursor_impact, cl.cmd.cursor_normal, 5, 0);
}

void CL_ClientMovement_InputQW(qw_usercmd_t *cmd)
{
	int i;
	int n;
	// remove stale queue items
	n = cl.movement_numqueue;
	cl.movement_numqueue = 0;
	if (cl.movement)
	{
		for (i = 0;i < n;i++)
			if (cl.movement_queue[i].sequence > cls.netcon->qw.incoming_sequence)
				cl.movement_queue[cl.movement_numqueue++] = cl.movement_queue[i];
		// add to input queue if there is room
		if (cl.movement_numqueue < (int)(sizeof(cl.movement_queue)/sizeof(cl.movement_queue[0])) && cl.mtime[0] > cl.mtime[1])
		{
			// add to input queue
			cl.movement_queue[cl.movement_numqueue].sequence = cls.netcon->qw.outgoing_sequence;
			cl.movement_queue[cl.movement_numqueue].time = cl.mtime[0] + cl_movement_latency.value / 1000.0;
			cl.movement_queue[cl.movement_numqueue].frametime = cmd->msec * 0.001;
			VectorCopy(cmd->angles, cl.movement_queue[cl.movement_numqueue].viewangles);
			cl.movement_queue[cl.movement_numqueue].move[0] = cmd->forwardmove;
			cl.movement_queue[cl.movement_numqueue].move[1] = cmd->sidemove;
			cl.movement_queue[cl.movement_numqueue].move[2] = cmd->upmove;
			cl.movement_queue[cl.movement_numqueue].jump = (cmd->buttons & 2) != 0;
			cl.movement_queue[cl.movement_numqueue].crouch = false;
			cl.movement_numqueue++;
		}
	}
	cl.movement_replay = true;
}

void CL_ClientMovement_Input(qboolean buttonjump, qboolean buttoncrouch)
{
	int i;
	int n;
	// remove stale queue items
	n = cl.movement_numqueue;
	cl.movement_numqueue = 0;
	if (cl.movement)
	{
		if (cl.servermovesequence)
		{
			for (i = 0;i < n;i++)
				if (cl.movement_queue[i].sequence > cl.servermovesequence)
					cl.movement_queue[cl.movement_numqueue++] = cl.movement_queue[i];
		}
		else
		{
			double simulatedtime = cl.mtime[0] + cl_movement_latency.value / 1000.0;
			for (i = 0;i < n;i++)
				if (cl.movement_queue[i].time >= cl.mtime[0] && cl.movement_queue[i].time <= simulatedtime)
					cl.movement_queue[cl.movement_numqueue++] = cl.movement_queue[i];
		}
		// add to input queue if there is room
		if (cl.movement_numqueue < (int)(sizeof(cl.movement_queue)/sizeof(cl.movement_queue[0])) && cl.mtime[0] > cl.mtime[1])
		{
			// add to input queue
			cl.movement_queue[cl.movement_numqueue].sequence = cl.movesequence;
			cl.movement_queue[cl.movement_numqueue].time = cl.mtime[0] + cl_movement_latency.value / 1000.0;
			cl.movement_queue[cl.movement_numqueue].frametime = cl.mtime[0] - cl.mtime[1];
			VectorCopy(cl.viewangles, cl.movement_queue[cl.movement_numqueue].viewangles);
			cl.movement_queue[cl.movement_numqueue].move[0] = cl.cmd.forwardmove;
			cl.movement_queue[cl.movement_numqueue].move[1] = cl.cmd.sidemove;
			cl.movement_queue[cl.movement_numqueue].move[2] = cl.cmd.upmove;
			cl.movement_queue[cl.movement_numqueue].jump = buttonjump;
			cl.movement_queue[cl.movement_numqueue].crouch = buttoncrouch;
			cl.movement_numqueue++;
		}
	}
	cl.movement_replay = true;
}

void CL_ClientMovement_Replay(void)
{
	int i;
	int bump;
	int contents;
	int crouch;
	int onground;
	int canjump;
	float movevars_gravity;
	float movevars_stopspeed;
	float movevars_maxspeed;
	float movevars_spectatormaxspeed;
	float movevars_accelerate;
	float movevars_airaccelerate;
	float movevars_wateraccelerate;
	float movevars_friction;
	float movevars_waterfriction;
	float movevars_entgravity;
	float movevars_jumpvelocity;
	float movevars_edgefriction;
	float movevars_maxairspeed;
	float movevars_stepheight;
	double edgefriction;
	double frametime;
	double t;
	vec_t wishspeed;
	vec_t addspeed;
	vec_t accelspeed;
	vec_t f;
	vec_t *playermins;
	vec_t *playermaxs;
	vec3_t currentorigin;
	vec3_t currentvelocity;
	vec3_t forward;
	vec3_t right;
	vec3_t up;
	vec3_t wishvel;
	vec3_t wishdir;
	vec3_t neworigin;
	vec3_t currentorigin2;
	vec3_t neworigin2;
	vec3_t yawangles;
	trace_t trace;
	trace_t trace2;
	trace_t trace3;


	if (!cl.movement_replay)
		return;
	cl.movement_replay = false;

	if (cls.protocol == PROTOCOL_QUAKEWORLD)
	{
		movevars_gravity = cl.qw_movevars_gravity;
		movevars_stopspeed = cl.qw_movevars_stopspeed;
		movevars_maxspeed = cl.qw_movevars_maxspeed;
		movevars_spectatormaxspeed = cl.qw_movevars_spectatormaxspeed;
		movevars_accelerate = cl.qw_movevars_accelerate;
		movevars_airaccelerate = cl.qw_movevars_airaccelerate;
		movevars_wateraccelerate = cl.qw_movevars_wateraccelerate;
		movevars_friction = cl.qw_movevars_friction;
		movevars_waterfriction = cl.qw_movevars_waterfriction;
		movevars_entgravity = cl.qw_movevars_entgravity;
		movevars_jumpvelocity = cl_movement_jumpvelocity.value;
		movevars_edgefriction = cl_movement_edgefriction.value;
		movevars_maxairspeed = cl_movement_maxairspeed.value;
		movevars_stepheight = cl_movement_stepheight.value;
	}
	else
	{
		movevars_gravity = sv_gravity.value;
		movevars_stopspeed = cl_movement_stopspeed.value;
		movevars_maxspeed = cl_movement_maxspeed.value;
		movevars_spectatormaxspeed = cl_movement_maxspeed.value;
		movevars_accelerate = cl_movement_accelerate.value;
		movevars_airaccelerate = cl_movement_accelerate.value;
		movevars_wateraccelerate = cl_movement_accelerate.value;
		movevars_friction = cl_movement_friction.value;
		movevars_waterfriction = cl_movement_friction.value;
		movevars_entgravity = 1;
		movevars_jumpvelocity = cl_movement_jumpvelocity.value;
		movevars_edgefriction = cl_movement_edgefriction.value;
		movevars_maxairspeed = cl_movement_maxairspeed.value;
		movevars_stepheight = cl_movement_stepheight.value;
	}

	// fetch current starting values
	VectorCopy(cl.entities[cl.playerentity].state_current.origin, currentorigin);
	VectorCopy(cl.mvelocity[0], currentvelocity);
	// FIXME: try minor nudges in various directions if startsolid to find a
	// safe place to start the walk (due to network compression in some
	// protocols this starts in solid)
	//currentorigin[2] += (1.0 / 32.0); // slight nudge to get out of the floor

	// check if onground
	VectorSet(currentorigin2, currentorigin[0], currentorigin[1], currentorigin[2] + 1);
	VectorSet(neworigin2, currentorigin[0], currentorigin[1], currentorigin[2] - 1);
	if (cl.movement)
	{
		trace = CL_TraceBox(currentorigin2, cl.playercrouchmins, cl.playercrouchmaxs, neworigin2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
		onground = trace.fraction < 1 && trace.plane.normal[2] > 0.7;
		crouch = false; // this will be updated on first move
		canjump = true;
		//Con_Printf("%f: ", cl.mtime[0]);

		// replay the input queue to predict current location
		// note: this relies on the fact there's always one queue item at the end

		for (i = 0;i < cl.movement_numqueue;i++)
		{
			client_movementqueue_t *q = cl.movement_queue + bound(0, i, cl.movement_numqueue - 1);
			frametime = q->frametime;
			//Con_Printf(" %f", frametime);
			//if (frametime > 0)
			{
				if (q->crouch)
				{
					// wants to crouch, this always works...
					if (!crouch)
						crouch = true;
				}
				else
				{
					// wants to stand, if currently crouching we need to check for a
					// low ceiling first
					if (crouch)
					{
						trace = CL_TraceBox(currentorigin, cl.playerstandmins, cl.playerstandmaxs, currentorigin, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
						if (!trace.startsolid)
							crouch = false;
					}
				}
				if (crouch)
				{
					playermins = cl.playercrouchmins;
					playermaxs = cl.playercrouchmaxs;
				}
				else
				{
					playermins = cl.playerstandmins;
					playermaxs = cl.playerstandmaxs;
				}
				// change velocity according to q->viewangles and q->move
				contents = CL_PointSuperContents(currentorigin);
				if (contents & SUPERCONTENTS_LIQUIDSMASK)
				{
					// swim
					AngleVectors(q->viewangles, forward, right, up);
					VectorSet(up, 0, 0, 1);
					VectorMAMAM(q->move[0], forward, q->move[1], right, q->move[2], up, wishvel);
					wishspeed = VectorLength(wishvel);
					if (wishspeed)
						VectorScale(wishvel, 1 / wishspeed, wishdir);
					else
						VectorSet( wishdir, 0.0, 0.0, 0.0 );
					wishspeed = min(wishspeed, movevars_maxspeed) * 0.6;
					if (crouch)
						wishspeed *= 0.5;
					VectorScale(currentvelocity, (1 - frametime * movevars_waterfriction), currentvelocity);
					f = wishspeed - DotProduct(currentvelocity, wishdir);
					if (f > 0)
					{
						f = min(f, movevars_wateraccelerate * frametime * wishspeed);
						VectorMA(currentvelocity, f, wishdir, currentvelocity);
					}
					if (q->jump)
					{
						if (contents & SUPERCONTENTS_LAVA)
							currentvelocity[2] =  50;
						else if (contents & SUPERCONTENTS_SLIME)
							currentvelocity[2] =  80;
						else
						{
							if (gamemode == GAME_NEXUIZ)
								currentvelocity[2] = 200;
							else
								currentvelocity[2] = 100;
						}
					}
				}
				else
				{
					// walk
					if (onground && q->jump && canjump)
					{
						currentvelocity[2] += movevars_jumpvelocity;
						onground = false;
						canjump = false;
					}
					if (!q->jump)
						canjump = true;
					VectorSet(yawangles, 0, q->viewangles[1], 0);
					AngleVectors(yawangles, forward, right, up);
					VectorMAM(q->move[0], forward, q->move[1], right, wishvel);
					wishspeed = VectorLength(wishvel);
					if (wishspeed)
						VectorScale(wishvel, 1 / wishspeed, wishdir);
					else
						VectorSet( wishdir, 0.0, 0.0, 0.0 );
					wishspeed = min(wishspeed, movevars_maxspeed);
					if (crouch)
						wishspeed *= 0.5;
					// check if onground
					if (onground)
					{
						// apply ground friction
						f = sqrt(currentvelocity[0] * currentvelocity[0] + currentvelocity[1] * currentvelocity[1]);
						edgefriction = 1;
						if (f > 0)
						{
							VectorSet(currentorigin2, currentorigin[0] + currentvelocity[0]*(16/f), currentorigin[1] + currentvelocity[1]*(16/f), currentorigin[2] + playermins[2]);
							VectorSet(neworigin2, currentorigin2[0], currentorigin2[1], currentorigin2[2] - 34);
							trace = CL_TraceBox(currentorigin2, vec3_origin, vec3_origin, neworigin2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
							if (trace.fraction == 1)
								edgefriction = movevars_edgefriction;
						}
						// apply friction
						f = 1 - frametime * edgefriction * ((f < movevars_stopspeed) ? (movevars_stopspeed / f) : 1) * movevars_friction;
						f = max(f, 0);
						VectorScale(currentvelocity, f, currentvelocity);
					}
					else
					{
						// apply air speed limit
						wishspeed = min(wishspeed, movevars_maxairspeed);
					}
					if (gamemode == GAME_NEXUIZ)
						addspeed = wishspeed;
					else
						addspeed = wishspeed - DotProduct(currentvelocity, wishdir);
					if (addspeed > 0)
					{
						accelspeed = min(movevars_accelerate * frametime * wishspeed, addspeed);
						VectorMA(currentvelocity, accelspeed, wishdir, currentvelocity);
					}
					currentvelocity[2] -= cl_gravity.value * frametime;
				}
			}
			//if (i < cl.movement_numqueue - 1 || (cl_movement.integer & 4))
			{
				if (crouch)
				{
					playermins = cl.playercrouchmins;
					playermaxs = cl.playercrouchmaxs;
				}
				else
				{
					playermins = cl.playerstandmins;
					playermaxs = cl.playerstandmaxs;
				}
				onground = false;
				for (bump = 0, t = frametime;bump < 8 && VectorLength2(currentvelocity) > 0;bump++)
				{
					VectorMA(currentorigin, t, currentvelocity, neworigin);
					trace = CL_TraceBox(currentorigin, playermins, playermaxs, neworigin, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
					if (trace.fraction < 1 && trace.plane.normal[2] == 0)
					{
						// may be a step or wall, try stepping up
						// first move forward at a higher level
						VectorSet(currentorigin2, currentorigin[0], currentorigin[1], currentorigin[2] + movevars_stepheight);
						VectorSet(neworigin2, neworigin[0], neworigin[1], currentorigin[2] + movevars_stepheight);
						trace2 = CL_TraceBox(currentorigin2, playermins, playermaxs, neworigin2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
						// then move down from there
						VectorCopy(trace2.endpos, currentorigin2);
						VectorSet(neworigin2, trace2.endpos[0], trace2.endpos[1], currentorigin[2]);
						trace3 = CL_TraceBox(currentorigin2, playermins, playermaxs, neworigin2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
						//Con_Printf("%f %f %f %f : %f %f %f %f : %f %f %f %f\n", trace.fraction, trace.endpos[0], trace.endpos[1], trace.endpos[2], trace2.fraction, trace2.endpos[0], trace2.endpos[1], trace2.endpos[2], trace3.fraction, trace3.endpos[0], trace3.endpos[1], trace3.endpos[2]);
						// accept the new trace if it made some progress
						if (fabs(trace3.endpos[0] - trace.endpos[0]) >= 0.03125 || fabs(trace3.endpos[1] - trace.endpos[1]) >= 0.03125)
						{
							trace = trace2;
							VectorCopy(trace3.endpos, trace.endpos);
						}
					}
					if (trace.fraction == 1)
					{
						VectorCopy(trace.endpos, currentorigin);
						break;
					}
					if (trace.plane.normal[2] > 0.7)
						onground = true;
					t *= 1 - trace.fraction;
					if (trace.fraction >= 0.001)
						VectorCopy(trace.endpos, currentorigin);
					f = DotProduct(currentvelocity, trace.plane.normal);
					VectorMA(currentvelocity, -f, trace.plane.normal, currentvelocity);
				}
			}
		}
		// store replay location
		cl.onground = onground;
	}
	VectorCopy(cl.movement_origin, cl.movement_oldorigin);
	VectorCopy(currentorigin, cl.movement_origin);
	VectorCopy(currentvelocity, cl.movement_velocity);
	//VectorCopy(currentorigin, cl.entities[cl.playerentity].state_current.origin);
	//VectorSet(cl.entities[cl.playerentity].state_current.angles, 0, cl.viewangles[1], 0);
}

void QW_MSG_WriteDeltaUsercmd(sizebuf_t *buf, qw_usercmd_t *from, qw_usercmd_t *to)
{
	int bits;

	bits = 0;
	if (to->angles[0] != from->angles[0])
		bits |= QW_CM_ANGLE1;
	if (to->angles[1] != from->angles[1])
		bits |= QW_CM_ANGLE2;
	if (to->angles[2] != from->angles[2])
		bits |= QW_CM_ANGLE3;
	if (to->forwardmove != from->forwardmove)
		bits |= QW_CM_FORWARD;
	if (to->sidemove != from->sidemove)
		bits |= QW_CM_SIDE;
	if (to->upmove != from->upmove)
		bits |= QW_CM_UP;
	if (to->buttons != from->buttons)
		bits |= QW_CM_BUTTONS;
	if (to->impulse != from->impulse)
		bits |= QW_CM_IMPULSE;

	MSG_WriteByte(buf, bits);
	if (bits & QW_CM_ANGLE1)
		MSG_WriteAngle16i(buf, to->angles[0]);
	if (bits & QW_CM_ANGLE2)
		MSG_WriteAngle16i(buf, to->angles[1]);
	if (bits & QW_CM_ANGLE3)
		MSG_WriteAngle16i(buf, to->angles[2]);
	if (bits & QW_CM_FORWARD)
		MSG_WriteShort(buf, to->forwardmove);
	if (bits & QW_CM_SIDE)
		MSG_WriteShort(buf, to->sidemove);
	if (bits & QW_CM_UP)
		MSG_WriteShort(buf, to->upmove);
	if (bits & QW_CM_BUTTONS)
		MSG_WriteByte(buf, to->buttons);
	if (bits & QW_CM_IMPULSE)
		MSG_WriteByte(buf, to->impulse);
	MSG_WriteByte(buf, to->msec);
}

/*
==============
CL_SendMove
==============
*/
extern cvar_t cl_netinputpacketspersecond;
void CL_SendMove(void)
{
	int i;
	int bits;
	int impulse;
	sizebuf_t buf;
	unsigned char data[128];
	static double lastsendtime = 0;
#define MOVEAVERAGING 0
#if MOVEAVERAGING
	static float accumforwardmove = 0, accumsidemove = 0, accumupmove = 0, accumtotal = 0; // accumulation
#endif
	float forwardmove, sidemove, upmove;

	// if playing a demo, do nothing
	if (!cls.netcon)
		return;

#if MOVEAVERAGING
	// accumulate changes between messages
	accumforwardmove += cl.cmd.forwardmove;
	accumsidemove += cl.cmd.sidemove;
	accumupmove += cl.cmd.upmove;
	accumtotal++;
#endif

	if (cl_movement.integer && cls.signon == SIGNONS && cls.protocol != PROTOCOL_QUAKEWORLD)
	{
		if (!cl.movement_needupdate)
			return;
		cl.movement_needupdate = false;
		cl.movement = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
	}
	else
	{
		cl.movement = false;
		if (realtime < lastsendtime + 1.0 / bound(10, cl_netinputpacketspersecond.value, 100))
			return;
		// don't let it fall behind if CL_SendMove hasn't been called recently
		// (such is the case when framerate is too low for instance)
		lastsendtime = max(lastsendtime + 1.0 / bound(10, cl_netinputpacketspersecond.value, 100), realtime);
	}
#if MOVEAVERAGING
	// average the accumulated changes
	accumtotal = 1.0f / accumtotal;
	forwardmove = accumforwardmove * accumtotal;
	sidemove = accumsidemove * accumtotal;
	upmove = accumupmove * accumtotal;
	accumforwardmove = 0;
	accumsidemove = 0;
	accumupmove = 0;
	accumtotal = 0;
#else
	// use the latest values
	forwardmove = cl.cmd.forwardmove;
	sidemove = cl.cmd.sidemove;
	upmove = cl.cmd.upmove;
#endif

	if (cls.signon == SIGNONS)
		CL_UpdatePrydonCursor();

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
	if (in_button9.state  & 3)  bits |=   2048;in_button9.state  &= ~2;
	if (in_button10.state  & 3) bits |=   4096;in_button10.state &= ~2;
	if (in_button11.state  & 3) bits |=   8192;in_button11.state &= ~2;
	if (in_button12.state  & 3) bits |=  16384;in_button12.state &= ~2;
	if (in_button13.state  & 3) bits |=  32768;in_button13.state &= ~2;
	if (in_button14.state  & 3) bits |=  65536;in_button14.state &= ~2;
	if (in_button15.state  & 3) bits |= 131072;in_button15.state &= ~2;
	if (in_button16.state  & 3) bits |= 262144;in_button16.state &= ~2;
	// button bits 19-31 unused currently
	// rotate/zoom view serverside if PRYDON_CLIENTCURSOR cursor is at edge of screen
	if (cl.cmd.cursor_screen[0] <= -1) bits |= 8;
	if (cl.cmd.cursor_screen[0] >=  1) bits |= 16;
	if (cl.cmd.cursor_screen[1] <= -1) bits |= 32;
	if (cl.cmd.cursor_screen[1] >=  1) bits |= 64;

	impulse = in_impulse;
	in_impulse = 0;

	csqc_buttons = bits;

	if (cls.signon == SIGNONS)
	{
		// always dump the first two messages, because they may contain leftover inputs from the last level
		if (++cl.movemessages >= 2)
		{
			// send the movement message
			// PROTOCOL_QUAKE        clc_move = 16 bytes total
			// PROTOCOL_QUAKEDP      clc_move = 16 bytes total
			// PROTOCOL_NEHAHRAMOVIE clc_move = 16 bytes total
			// PROTOCOL_DARKPLACES1  clc_move = 19 bytes total
			// PROTOCOL_DARKPLACES2  clc_move = 25 bytes total
			// PROTOCOL_DARKPLACES3  clc_move = 25 bytes total
			// PROTOCOL_DARKPLACES4  clc_move = 19 bytes total
			// PROTOCOL_DARKPLACES5  clc_move = 19 bytes total
			// PROTOCOL_DARKPLACES6  clc_move = 52 bytes total
			// PROTOCOL_DARKPLACES7  clc_move = 56 bytes total
			// PROTOCOL_QUAKEWORLD   clc_move = 34 bytes total (typically, but can reach 43 bytes, or even 49 bytes with roll)
			if (cls.protocol == PROTOCOL_QUAKEWORLD)
			{
				int checksumindex;
				double msectime;
				static double oldmsectime;
				qw_usercmd_t *cmd, *oldcmd;
				qw_usercmd_t nullcmd;

				//Con_Printf("code qw_clc_move\n");

				i = cls.netcon->qw.outgoing_sequence & QW_UPDATE_MASK;
				cmd = &cl.qw_moves[i];
				memset(&nullcmd, 0, sizeof(nullcmd));
				memset(cmd, 0, sizeof(*cmd));
				cmd->buttons = bits;
				cmd->impulse = impulse;
				cmd->forwardmove = (short)bound(-32768, forwardmove, 32767);
				cmd->sidemove = (short)bound(-32768, sidemove, 32767);
				cmd->upmove = (short)bound(-32768, upmove, 32767);
				VectorCopy(cl.viewangles, cmd->angles);
				msectime = realtime * 1000;
				cmd->msec = (unsigned char)bound(0, msectime - oldmsectime, 255);
				// ridiculous value rejection (matches qw)
				if (cmd->msec > 250)
					cmd->msec = 100;
				oldmsectime = msectime;

				CL_ClientMovement_InputQW(cmd);

				MSG_WriteByte(&buf, qw_clc_move);
				// save the position for a checksum byte
				checksumindex = buf.cursize;
				MSG_WriteByte(&buf, 0);
				// packet loss percentage
				// FIXME: netgraph stuff
				MSG_WriteByte(&buf, 0);
				// write most recent 3 moves
				i = (cls.netcon->qw.outgoing_sequence-2) & QW_UPDATE_MASK;
				cmd = &cl.qw_moves[i];
				QW_MSG_WriteDeltaUsercmd(&buf, &nullcmd, cmd);
				oldcmd = cmd;
				i = (cls.netcon->qw.outgoing_sequence-1) & QW_UPDATE_MASK;
				cmd = &cl.qw_moves[i];
				QW_MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
				oldcmd = cmd;
				i = cls.netcon->qw.outgoing_sequence & QW_UPDATE_MASK;
				cmd = &cl.qw_moves[i];
				QW_MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
				// calculate the checksum
				buf.data[checksumindex] = COM_BlockSequenceCRCByteQW(buf.data + checksumindex + 1, buf.cursize - checksumindex - 1, cls.netcon->qw.outgoing_sequence);
				// if delta compression history overflows, request no delta
				if (cls.netcon->qw.outgoing_sequence - cl.qw_validsequence >= QW_UPDATE_BACKUP-1)
					cl.qw_validsequence = 0;
				// request delta compression if appropriate
				if (cl.qw_validsequence && !cl_nodelta.integer && cls.state == ca_connected && !cls.demorecording)
				{
					cl.qw_deltasequence[cls.netcon->qw.outgoing_sequence & QW_UPDATE_MASK] = cl.qw_validsequence;
					MSG_WriteByte(&buf, qw_clc_delta);
					MSG_WriteByte(&buf, cl.qw_validsequence & 255);
				}
				else
					cl.qw_deltasequence[cls.netcon->qw.outgoing_sequence & QW_UPDATE_MASK] = -1;
			}
			else if (cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE)
			{
				// 5 bytes
				MSG_WriteByte (&buf, clc_move);
				MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times
				// 3 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle8i (&buf, cl.viewangles[i]);
				// 6 bytes
				MSG_WriteCoord16i (&buf, forwardmove);
				MSG_WriteCoord16i (&buf, sidemove);
				MSG_WriteCoord16i (&buf, upmove);
				// 2 bytes
				MSG_WriteByte (&buf, bits);
				MSG_WriteByte (&buf, impulse);

				CL_ClientMovement_Input((bits & 2) != 0, false);
			}
			else if (cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3)
			{
				// 5 bytes
				MSG_WriteByte (&buf, clc_move);
				MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times
				// 12 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle32f (&buf, cl.viewangles[i]);
				// 6 bytes
				MSG_WriteCoord16i (&buf, forwardmove);
				MSG_WriteCoord16i (&buf, sidemove);
				MSG_WriteCoord16i (&buf, upmove);
				// 2 bytes
				MSG_WriteByte (&buf, bits);
				MSG_WriteByte (&buf, impulse);

				CL_ClientMovement_Input((bits & 2) != 0, false);
			}
			else if (cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES4 || cls.protocol == PROTOCOL_DARKPLACES5)
			{
				// 5 bytes
				MSG_WriteByte (&buf, clc_move);
				MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times
				// 6 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle16i (&buf, cl.viewangles[i]);
				// 6 bytes
				MSG_WriteCoord16i (&buf, forwardmove);
				MSG_WriteCoord16i (&buf, sidemove);
				MSG_WriteCoord16i (&buf, upmove);
				// 2 bytes
				MSG_WriteByte (&buf, bits);
				MSG_WriteByte (&buf, impulse);

				CL_ClientMovement_Input((bits & 2) != 0, false);
			}
			else
			{
				// 5 bytes
				MSG_WriteByte (&buf, clc_move);
				if (cls.protocol != PROTOCOL_DARKPLACES6)
				{
					if (cl_movement.integer)
					{
						cl.movesequence++;
						MSG_WriteLong (&buf, cl.movesequence);
					}
					else
						MSG_WriteLong (&buf, 0);
				}
				MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times
				// 6 bytes
				for (i = 0;i < 3;i++)
					MSG_WriteAngle16i (&buf, cl.viewangles[i]);
				// 6 bytes
				MSG_WriteCoord16i (&buf, forwardmove);
				MSG_WriteCoord16i (&buf, sidemove);
				MSG_WriteCoord16i (&buf, upmove);
				// 5 bytes
				MSG_WriteLong (&buf, bits);
				MSG_WriteByte (&buf, impulse);
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

				// FIXME: bits & 16 is +button5, Nexuiz specific
				CL_ClientMovement_Input((bits & 2) != 0, (bits & 16) != 0);
			}
		}

		if (cls.protocol != PROTOCOL_QUAKEWORLD)
		{
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
		}

		// PROTOCOL_DARKPLACES6 = 67 bytes per packet
		// PROTOCOL_DARKPLACES7 = 71 bytes per packet
	}

	// send the reliable message (forwarded commands) if there is one
	NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol);

	if (cls.netcon->message.overflowed)
	{
		Con_Print("CL_SendMove: lost server connection\n");
		CL_Disconnect();
		Host_ShutdownServer();
	}
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("+moveup",IN_UpDown, "swim upward");
	Cmd_AddCommand ("-moveup",IN_UpUp, "stop swimming upward");
	Cmd_AddCommand ("+movedown",IN_DownDown, "swim downward");
	Cmd_AddCommand ("-movedown",IN_DownUp, "stop swimming downward");
	Cmd_AddCommand ("+left",IN_LeftDown, "turn left");
	Cmd_AddCommand ("-left",IN_LeftUp, "stop turning left");
	Cmd_AddCommand ("+right",IN_RightDown, "turn right");
	Cmd_AddCommand ("-right",IN_RightUp, "stop turning right");
	Cmd_AddCommand ("+forward",IN_ForwardDown, "move forward");
	Cmd_AddCommand ("-forward",IN_ForwardUp, "stop moving forward");
	Cmd_AddCommand ("+back",IN_BackDown, "move backward");
	Cmd_AddCommand ("-back",IN_BackUp, "stop moving backward");
	Cmd_AddCommand ("+lookup", IN_LookupDown, "look upward");
	Cmd_AddCommand ("-lookup", IN_LookupUp, "stop looking upward");
	Cmd_AddCommand ("+lookdown", IN_LookdownDown, "look downward");
	Cmd_AddCommand ("-lookdown", IN_LookdownUp, "stop looking downward");
	Cmd_AddCommand ("+strafe", IN_StrafeDown, "activate strafing mode (move instead of turn)\n");
	Cmd_AddCommand ("-strafe", IN_StrafeUp, "deactivate strafing mode");
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown, "strafe left");
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp, "stop strafing left");
	Cmd_AddCommand ("+moveright", IN_MoverightDown, "strafe right");
	Cmd_AddCommand ("-moveright", IN_MoverightUp, "stop strafing right");
	Cmd_AddCommand ("+speed", IN_SpeedDown, "activate run mode (faster movement and turning)");
	Cmd_AddCommand ("-speed", IN_SpeedUp, "deactivate run mode");
	Cmd_AddCommand ("+attack", IN_AttackDown, "begin firing");
	Cmd_AddCommand ("-attack", IN_AttackUp, "stop firing");
	Cmd_AddCommand ("+jump", IN_JumpDown, "jump");
	Cmd_AddCommand ("-jump", IN_JumpUp, "end jump (so you can jump again)");
	Cmd_AddCommand ("impulse", IN_Impulse, "send an impulse number to server (select weapon, use item, etc)");
	Cmd_AddCommand ("+klook", IN_KLookDown, "activate keyboard looking mode, do not recenter view");
	Cmd_AddCommand ("-klook", IN_KLookUp, "deactivate keyboard looking mode");
	Cmd_AddCommand ("+mlook", IN_MLookDown, "activate mouse looking mode, do not recenter view");
	Cmd_AddCommand ("-mlook", IN_MLookUp, "deactivate mouse looking mode");

	// LordHavoc: added use button
	Cmd_AddCommand ("+use", IN_UseDown, "use something (may be used by some mods)");
	Cmd_AddCommand ("-use", IN_UseUp, "stop using something");

	// LordHavoc: added 6 new buttons
	Cmd_AddCommand ("+button3", IN_Button3Down, "activate button3 (behavior depends on mod)");
	Cmd_AddCommand ("-button3", IN_Button3Up, "deactivate button3");
	Cmd_AddCommand ("+button4", IN_Button4Down, "activate button4 (behavior depends on mod)");
	Cmd_AddCommand ("-button4", IN_Button4Up, "deactivate button4");
	Cmd_AddCommand ("+button5", IN_Button5Down, "activate button5 (behavior depends on mod)");
	Cmd_AddCommand ("-button5", IN_Button5Up, "deactivate button5");
	Cmd_AddCommand ("+button6", IN_Button6Down, "activate button6 (behavior depends on mod)");
	Cmd_AddCommand ("-button6", IN_Button6Up, "deactivate button6");
	Cmd_AddCommand ("+button7", IN_Button7Down, "activate button7 (behavior depends on mod)");
	Cmd_AddCommand ("-button7", IN_Button7Up, "deactivate button7");
	Cmd_AddCommand ("+button8", IN_Button8Down, "activate button8 (behavior depends on mod)");
	Cmd_AddCommand ("-button8", IN_Button8Up, "deactivate button8");
	Cmd_AddCommand ("+button9", IN_Button9Down, "activate button9 (behavior depends on mod)");
	Cmd_AddCommand ("-button9", IN_Button9Up, "deactivate button9");
	Cmd_AddCommand ("+button10", IN_Button10Down, "activate button10 (behavior depends on mod)");
	Cmd_AddCommand ("-button10", IN_Button10Up, "deactivate button10");
	Cmd_AddCommand ("+button11", IN_Button11Down, "activate button11 (behavior depends on mod)");
	Cmd_AddCommand ("-button11", IN_Button11Up, "deactivate button11");
	Cmd_AddCommand ("+button12", IN_Button12Down, "activate button12 (behavior depends on mod)");
	Cmd_AddCommand ("-button12", IN_Button12Up, "deactivate button12");
	Cmd_AddCommand ("+button13", IN_Button13Down, "activate button13 (behavior depends on mod)");
	Cmd_AddCommand ("-button13", IN_Button13Up, "deactivate button13");
	Cmd_AddCommand ("+button14", IN_Button14Down, "activate button14 (behavior depends on mod)");
	Cmd_AddCommand ("-button14", IN_Button14Up, "deactivate button14");
	Cmd_AddCommand ("+button15", IN_Button15Down, "activate button15 (behavior depends on mod)");
	Cmd_AddCommand ("-button15", IN_Button15Up, "deactivate button15");
	Cmd_AddCommand ("+button16", IN_Button16Down, "activate button16 (behavior depends on mod)");
	Cmd_AddCommand ("-button16", IN_Button16Up, "deactivate button16");

	Cvar_RegisterVariable(&cl_movement);
	Cvar_RegisterVariable(&cl_movement_latency);
	Cvar_RegisterVariable(&cl_movement_maxspeed);
	Cvar_RegisterVariable(&cl_movement_maxairspeed);
	Cvar_RegisterVariable(&cl_movement_stopspeed);
	Cvar_RegisterVariable(&cl_movement_friction);
	Cvar_RegisterVariable(&cl_movement_edgefriction);
	Cvar_RegisterVariable(&cl_movement_stepheight);
	Cvar_RegisterVariable(&cl_movement_accelerate);
	Cvar_RegisterVariable(&cl_movement_jumpvelocity);
	Cvar_RegisterVariable(&cl_gravity);
	Cvar_RegisterVariable(&cl_slowmo);

	Cvar_RegisterVariable(&in_pitch_min);
	Cvar_RegisterVariable(&in_pitch_max);
	Cvar_RegisterVariable(&m_filter);

	Cvar_RegisterVariable(&cl_netinputpacketspersecond);

	Cvar_RegisterVariable(&cl_nodelta);
}

