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

cvar_t cl_movement = {CVAR_SAVE, "cl_movement", "0"};
cvar_t cl_movement_latency = {0, "cl_movement_latency", "0"};
cvar_t cl_movement_maxspeed = {0, "cl_movement_maxspeed", "320"};
cvar_t cl_movement_maxairspeed = {0, "cl_movement_maxairspeed", "30"};
cvar_t cl_movement_stopspeed = {0, "cl_movement_stopspeed", "100"};
cvar_t cl_movement_friction = {0, "cl_movement_friction", "4"};
cvar_t cl_movement_edgefriction = {0, "cl_movement_edgefriction", "2"};
cvar_t cl_movement_stepheight = {0, "cl_movement_stepheight", "18"};
cvar_t cl_movement_accelerate = {0, "cl_movement_accelerate", "10"};
cvar_t cl_gravity = {0, "cl_gravity", "800"};
cvar_t cl_slowmo = {0, "cl_slowmo", "1"};

cvar_t in_pitch_min = {0, "in_pitch_min", "-90"}; // quake used -70
cvar_t in_pitch_max = {0, "in_pitch_max", "90"}; // quake used 80

cvar_t m_filter = {CVAR_SAVE, "m_filter","0"};


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
	if (in_client_mouse)
	{
		if (cl_prydoncursor.integer)
		{
			// mouse interacting with the scene, mostly stationary view
			V_StopPitchDrift();
			cl.cmd.cursor_screen[0] += in_mouse_x * sensitivity.value / vid.realwidth;
			cl.cmd.cursor_screen[1] += in_mouse_y * sensitivity.value / vid.realheight;
		}
		else if (in_strafe.state & 1)
		{
			// strafing mode, all looking is movement
			V_StopPitchDrift();
			cl.cmd.sidemove += m_side.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			if (noclip_anglehack)
				cl.cmd.upmove -= m_forward.value * in_mouse_y * sensitivity.value * cl.viewzoom;
			else
				cl.cmd.forwardmove -= m_forward.value * in_mouse_y * sensitivity.value * cl.viewzoom;
		}
		else if ((in_mlook.state & 1) || freelook.integer)
		{
			// mouselook, lookstrafe causes turning to become strafing
			V_StopPitchDrift();
			if (lookstrafe.integer)
				cl.cmd.sidemove += m_side.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			else
				cl.viewangles[YAW] -= m_yaw.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			cl.viewangles[PITCH] += m_pitch.value * in_mouse_y * sensitivity.value * cl.viewzoom;
		}
		else
		{
			// non-mouselook, yaw turning and forward/back movement
			cl.viewangles[YAW] -= m_yaw.value * in_mouse_x * sensitivity.value * cl.viewzoom;
			cl.cmd.forwardmove -= m_forward.value * in_mouse_y * sensitivity.value * cl.viewzoom;
		}
	}

	// clamp after the move to prevent rendering with bad angles
	CL_AdjustAngles ();
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

void CL_ClientMovement(qboolean buttonjump, qboolean buttoncrouch)
{
	int i;
	int n;
	int bump;
	int contents;
	int crouch;
	double edgefriction;
	double simulatedtime;
	double currenttime;
	double newtime;
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
	// remove stale queue items
	n = cl.movement_numqueue;
	cl.movement_numqueue = 0;
	// calculate time to execute for
	currenttime = cl.mtime[0];
	simulatedtime = currenttime + cl_movement_latency.value / 1000.0;
	for (i = 0;i < n;i++)
		if (cl.movement_queue[i].time >= cl.mtime[0] && cl.movement_queue[i].time <= simulatedtime)
			cl.movement_queue[cl.movement_numqueue++] = cl.movement_queue[i];
	// add to input queue if there is room
	if (cl.movement_numqueue < sizeof(cl.movement_queue)/sizeof(cl.movement_queue[0]))
	{
		// add to input queue
		cl.movement_queue[cl.movement_numqueue].time = simulatedtime;
		VectorCopy(cl.viewangles, cl.movement_queue[cl.movement_numqueue].viewangles);
		cl.movement_queue[cl.movement_numqueue].move[0] = cl.cmd.forwardmove;
		cl.movement_queue[cl.movement_numqueue].move[1] = cl.cmd.sidemove;
		cl.movement_queue[cl.movement_numqueue].move[2] = cl.cmd.upmove;
		cl.movement_queue[cl.movement_numqueue].jump = buttonjump;
		cl.movement_queue[cl.movement_numqueue].crouch = buttoncrouch;
		cl.movement_numqueue++;
	}
	// fetch current starting values
	VectorCopy(cl_entities[cl.playerentity].state_current.origin, currentorigin);
	VectorCopy(cl.mvelocity[0], currentvelocity);
	// FIXME: try minor nudges in various directions if startsolid to find a
	// safe place to start the walk (due to network compression in some
	// protocols this starts in solid)
	//currentorigin[2] += (1.0 / 32.0); // slight nudge to get out of the floor
	crouch = false; // this will be updated on first move
	//Con_Printf("%f: ", currenttime);
	// replay input queue, and remove any stale queue items
	// note: this relies on the fact there's always one queue item at the end
	// abort if client movement is disabled
	cl.movement = cl_movement.integer && cl.stats[STAT_HEALTH] > 0 && !cls.demoplayback;
	if (!cl.movement)
		cl.movement_numqueue = 0;
	for (i = 0;i <= cl.movement_numqueue;i++)
	{
		newtime = (i >= cl.movement_numqueue) ? simulatedtime : cl.movement_queue[i].time;
		frametime = newtime - currenttime;
		if (frametime <= 0)
			continue;
		//Con_Printf(" %f", frametime);
		currenttime = newtime;
		if (i >= 1 && i <= cl.movement_numqueue)
		if (i > 0 || (cl_movement.integer & 8))
		if (i < cl.movement_numqueue - 1 || (cl_movement.integer & 16))
		{
			client_movementqueue_t *q = cl.movement_queue + i - 1;
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
					trace = CL_TraceBox(currentorigin, cl_playerstandmins, cl_playerstandmaxs, currentorigin, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
					if (!trace.startsolid)
						crouch = false;
				}
			}
			if (crouch)
			{
				playermins = cl_playercrouchmins;
				playermaxs = cl_playercrouchmaxs;
			}
			else
			{
				playermins = cl_playerstandmins;
				playermaxs = cl_playerstandmaxs;
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
				wishspeed = min(wishspeed, cl_movement_maxspeed.value);
				if (crouch)
					wishspeed *= 0.5;
				wishspeed *= 0.6;
				VectorScale(currentvelocity, (1 - frametime * cl_movement_friction.value), currentvelocity);
				f = wishspeed - DotProduct(currentvelocity, wishdir);
				if (f > 0)
				{
					f = min(f, cl_movement_accelerate.value * frametime * wishspeed);
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
				VectorSet(yawangles, 0, cl.viewangles[1], 0);
				AngleVectors(yawangles, forward, right, up);
				VectorMAM(q->move[0], forward, q->move[1], right, wishvel);
				wishspeed = VectorLength(wishvel);
				if (wishspeed)
					VectorScale(wishvel, 1 / wishspeed, wishdir);
				wishspeed = min(wishspeed, cl_movement_maxspeed.value);
				if (crouch)
					wishspeed *= 0.5;
				// check if onground
				VectorSet(currentorigin2, currentorigin[0], currentorigin[1], currentorigin[2] + 1);
				VectorSet(neworigin2, currentorigin[0], currentorigin[1], currentorigin[2] - 1);
				trace = CL_TraceBox(currentorigin2, playermins, playermaxs, neworigin2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
				if (trace.fraction < 1 && trace.plane.normal[2] > 0.7)
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
							edgefriction = cl_movement_edgefriction.value;
					}
					// apply friction
					f = 1 - frametime * edgefriction * ((f < cl_movement_stopspeed.value) ? (cl_movement_stopspeed.value / f) : 1) * cl_movement_friction.value;
					f = max(f, 0);
					VectorScale(currentvelocity, f, currentvelocity);
				}
				else
				{
					// apply air speed limit
					wishspeed = min(wishspeed, cl_movement_maxairspeed.value);
				}
				if (gamemode == GAME_NEXUIZ)
					addspeed = wishspeed;
				else
					addspeed = wishspeed - DotProduct(currentvelocity, wishdir);
				if (addspeed > 0)
				{
					accelspeed = min(cl_movement_accelerate.value * frametime * wishspeed, addspeed);
					VectorMA(currentvelocity, accelspeed, wishdir, currentvelocity);
				}
				currentvelocity[2] -= cl_gravity.value * frametime;
			}
		}
		if (i > 0 || (cl_movement.integer & 2))
		if (i < cl.movement_numqueue - 1 || (cl_movement.integer & 4))
		{
			if (crouch)
			{
				playermins = cl_playercrouchmins;
				playermaxs = cl_playercrouchmaxs;
			}
			else
			{
				playermins = cl_playerstandmins;
				playermaxs = cl_playerstandmaxs;
			}
			for (bump = 0, t = frametime;bump < 8 && VectorLength2(currentvelocity) > 0;bump++)
			{
				VectorMA(currentorigin, t, currentvelocity, neworigin);
				trace = CL_TraceBox(currentorigin, playermins, playermaxs, neworigin, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_PLAYERCLIP, true);
				if (trace.fraction < 1 && trace.plane.normal[2] == 0)
				{
					// may be a step or wall, try stepping up
					// first move forward at a higher level
					VectorSet(currentorigin2, currentorigin[0], currentorigin[1], currentorigin[2] + cl_movement_stepheight.value);
					VectorSet(neworigin2, neworigin[0], neworigin[1], currentorigin[2] + cl_movement_stepheight.value);
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
				t *= 1 - trace.fraction;
				if (trace.fraction >= 0.001)
					VectorCopy(trace.endpos, currentorigin);
				f = DotProduct(currentvelocity, trace.plane.normal);
				VectorMA(currentvelocity, -f, trace.plane.normal, currentvelocity);
			}
		}
	}
	//Con_Printf(" :%f\n", currenttime);
	// store replay location
	VectorCopy(cl.movement_origin, cl.movement_oldorigin);
	VectorCopy(currentorigin, cl.movement_origin);
	VectorCopy(currentvelocity, cl.movement_velocity);
	//VectorCopy(currentorigin, cl_entities[cl.playerentity].state_current.origin);
	//VectorSet(cl_entities[cl.playerentity].state_current.angles, 0, cl.viewangles[1], 0);
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
#define MOVEAVERAGING 0
#if MOVEAVERAGING
	static float forwardmove, sidemove, upmove, total; // accumulation
#else
	float forwardmove, sidemove, upmove;
#endif

#if MOVEAVERAGING
	// accumulate changes between messages
	forwardmove += cl.cmd.forwardmove;
	sidemove += cl.cmd.sidemove;
	upmove += cl.cmd.upmove;
	total++;
#endif
	if (cls.signon != SIGNONS)
		return;
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

	// FIXME: bits & 64 is +button5, Nexuiz specific
	CL_ClientMovement((bits & 2) != 0, (bits & 64) != 0);

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

	Cvar_RegisterVariable(&cl_movement);
	Cvar_RegisterVariable(&cl_movement_latency);
	Cvar_RegisterVariable(&cl_movement_maxspeed);
	Cvar_RegisterVariable(&cl_movement_maxairspeed);
	Cvar_RegisterVariable(&cl_movement_stopspeed);
	Cvar_RegisterVariable(&cl_movement_friction);
	Cvar_RegisterVariable(&cl_movement_edgefriction);
	Cvar_RegisterVariable(&cl_movement_stepheight);
	Cvar_RegisterVariable(&cl_movement_accelerate);
	Cvar_RegisterVariable(&cl_gravity);
	Cvar_RegisterVariable(&cl_slowmo);

	Cvar_RegisterVariable(&in_pitch_min);
	Cvar_RegisterVariable(&in_pitch_max);
	Cvar_RegisterVariable(&m_filter);
}

