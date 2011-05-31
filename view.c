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
// view.c -- player eye positioning

#include "quakedef.h"
#include "cl_collision.h"
#include "image.h"

void CL_VM_UpdateDmgGlobals (int dmg_take, int dmg_save, vec3_t dmg_origin);

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t cl_rollspeed = {0, "cl_rollspeed", "200", "how much strafing is necessary to tilt the view"};
cvar_t cl_rollangle = {0, "cl_rollangle", "2.0", "how much to tilt the view when strafing"};

cvar_t cl_bob = {CVAR_SAVE, "cl_bob","0.02", "view bobbing amount"};
cvar_t cl_bobcycle = {CVAR_SAVE, "cl_bobcycle","0.6", "view bobbing speed"};
cvar_t cl_bobup = {CVAR_SAVE, "cl_bobup","0.5", "view bobbing adjustment that makes the up or down swing of the bob last longer"};
cvar_t cl_bob2 = {CVAR_SAVE, "cl_bob2","0", "sideways view bobbing amount"};
cvar_t cl_bob2cycle = {CVAR_SAVE, "cl_bob2cycle","0.6", "sideways view bobbing speed"};
cvar_t cl_bob2smooth = {CVAR_SAVE, "cl_bob2smooth","0.05", "how fast the view goes back when you stop touching the ground"};
cvar_t cl_bobfall = {CVAR_SAVE, "cl_bobfall","0", "how much the view swings down when falling (influenced by the speed you hit the ground with)"};
cvar_t cl_bobfallcycle = {CVAR_SAVE, "cl_bobfallcycle","3", "speed of the bobfall swing"};
cvar_t cl_bobfallminspeed = {CVAR_SAVE, "cl_bobfallminspeed","200", "necessary amount of speed for bob-falling to occur"};
cvar_t cl_bobmodel = {CVAR_SAVE, "cl_bobmodel", "1", "enables gun bobbing"};
cvar_t cl_bobmodel_side = {CVAR_SAVE, "cl_bobmodel_side", "0.15", "gun bobbing sideways sway amount"};
cvar_t cl_bobmodel_up = {CVAR_SAVE, "cl_bobmodel_up", "0.06", "gun bobbing upward movement amount"};
cvar_t cl_bobmodel_speed = {CVAR_SAVE, "cl_bobmodel_speed", "7", "gun bobbing speed"};

cvar_t cl_leanmodel = {CVAR_SAVE, "cl_leanmodel", "0", "enables gun leaning"};
cvar_t cl_leanmodel_side_speed = {CVAR_SAVE, "cl_leanmodel_side_speed", "0.7", "gun leaning sideways speed"};
cvar_t cl_leanmodel_side_limit = {CVAR_SAVE, "cl_leanmodel_side_limit", "35", "gun leaning sideways limit"};
cvar_t cl_leanmodel_side_highpass1 = {CVAR_SAVE, "cl_leanmodel_side_highpass1", "30", "gun leaning sideways pre-highpass in 1/s"};
cvar_t cl_leanmodel_side_highpass = {CVAR_SAVE, "cl_leanmodel_side_highpass", "3", "gun leaning sideways highpass in 1/s"};
cvar_t cl_leanmodel_side_lowpass = {CVAR_SAVE, "cl_leanmodel_side_lowpass", "20", "gun leaning sideways lowpass in 1/s"};
cvar_t cl_leanmodel_up_speed = {CVAR_SAVE, "cl_leanmodel_up_speed", "0.65", "gun leaning upward speed"};
cvar_t cl_leanmodel_up_limit = {CVAR_SAVE, "cl_leanmodel_up_limit", "50", "gun leaning upward limit"};
cvar_t cl_leanmodel_up_highpass1 = {CVAR_SAVE, "cl_leanmodel_up_highpass1", "5", "gun leaning upward pre-highpass in 1/s"};
cvar_t cl_leanmodel_up_highpass = {CVAR_SAVE, "cl_leanmodel_up_highpass", "15", "gun leaning upward highpass in 1/s"};
cvar_t cl_leanmodel_up_lowpass = {CVAR_SAVE, "cl_leanmodel_up_lowpass", "20", "gun leaning upward lowpass in 1/s"};

cvar_t cl_followmodel = {CVAR_SAVE, "cl_followmodel", "0", "enables gun following"};
cvar_t cl_followmodel_side_speed = {CVAR_SAVE, "cl_followmodel_side_speed", "0.25", "gun following sideways speed"};
cvar_t cl_followmodel_side_limit = {CVAR_SAVE, "cl_followmodel_side_limit", "6", "gun following sideways limit"};
cvar_t cl_followmodel_side_highpass1 = {CVAR_SAVE, "cl_followmodel_side_highpass1", "30", "gun following sideways pre-highpass in 1/s"};
cvar_t cl_followmodel_side_highpass = {CVAR_SAVE, "cl_followmodel_side_highpass", "5", "gun following sideways highpass in 1/s"};
cvar_t cl_followmodel_side_lowpass = {CVAR_SAVE, "cl_followmodel_side_lowpass", "10", "gun following sideways lowpass in 1/s"};
cvar_t cl_followmodel_up_speed = {CVAR_SAVE, "cl_followmodel_up_speed", "0.5", "gun following upward speed"};
cvar_t cl_followmodel_up_limit = {CVAR_SAVE, "cl_followmodel_up_limit", "5", "gun following upward limit"};
cvar_t cl_followmodel_up_highpass1 = {CVAR_SAVE, "cl_followmodel_up_highpass1", "60", "gun following upward pre-highpass in 1/s"};
cvar_t cl_followmodel_up_highpass = {CVAR_SAVE, "cl_followmodel_up_highpass", "2", "gun following upward highpass in 1/s"};
cvar_t cl_followmodel_up_lowpass = {CVAR_SAVE, "cl_followmodel_up_lowpass", "10", "gun following upward lowpass in 1/s"};

cvar_t cl_viewmodel_scale = {0, "cl_viewmodel_scale", "1", "changes size of gun model, lower values prevent poking into walls but cause strange artifacts on lighting and especially r_stereo/vid_stereobuffer options where the size of the gun becomes visible"};

cvar_t v_kicktime = {0, "v_kicktime", "0.5", "how long a view kick from damage lasts"};
cvar_t v_kickroll = {0, "v_kickroll", "0.6", "how much a view kick from damage rolls your view"};
cvar_t v_kickpitch = {0, "v_kickpitch", "0.6", "how much a view kick from damage pitches your view"};

cvar_t v_iyaw_cycle = {0, "v_iyaw_cycle", "2", "v_idlescale yaw speed"};
cvar_t v_iroll_cycle = {0, "v_iroll_cycle", "0.5", "v_idlescale roll speed"};
cvar_t v_ipitch_cycle = {0, "v_ipitch_cycle", "1", "v_idlescale pitch speed"};
cvar_t v_iyaw_level = {0, "v_iyaw_level", "0.3", "v_idlescale yaw amount"};
cvar_t v_iroll_level = {0, "v_iroll_level", "0.1", "v_idlescale roll amount"};
cvar_t v_ipitch_level = {0, "v_ipitch_level", "0.3", "v_idlescale pitch amount"};

cvar_t v_idlescale = {0, "v_idlescale", "0", "how much of the quake 'drunken view' effect to use"};

cvar_t crosshair = {CVAR_SAVE, "crosshair", "0", "selects crosshair to use (0 is none)"};

cvar_t v_centermove = {0, "v_centermove", "0.15", "how long before the view begins to center itself (if freelook/+mlook/+jlook/+klook are off)"};
cvar_t v_centerspeed = {0, "v_centerspeed","500", "how fast the view centers itself"};

cvar_t cl_stairsmoothspeed = {CVAR_SAVE, "cl_stairsmoothspeed", "160", "how fast your view moves upward/downward when running up/down stairs"};

cvar_t cl_smoothviewheight = {CVAR_SAVE, "cl_smoothviewheight", "0", "time of the averaging to the viewheight value so that it creates a smooth transition. higher values = longer transition, 0 for instant transition."};

cvar_t chase_back = {CVAR_SAVE, "chase_back", "48", "chase cam distance from the player"};
cvar_t chase_up = {CVAR_SAVE, "chase_up", "24", "chase cam distance from the player"};
cvar_t chase_active = {CVAR_SAVE, "chase_active", "0", "enables chase cam"};
cvar_t chase_overhead = {CVAR_SAVE, "chase_overhead", "0", "chase cam looks straight down if this is not zero"};
// GAME_GOODVSBAD2
cvar_t chase_stevie = {0, "chase_stevie", "0", "(GOODVSBAD2 only) chase cam view from above"};

cvar_t v_deathtilt = {0, "v_deathtilt", "1", "whether to use sideways view when dead"};
cvar_t v_deathtiltangle = {0, "v_deathtiltangle", "80", "what roll angle to use when tilting the view while dead"};

// Prophecy camera pitchangle by Alexander "motorsep" Zubov
cvar_t chase_pitchangle = {CVAR_SAVE, "chase_pitchangle", "55", "chase cam pitch angle"};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;


/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	right;
	float	sign;
	float	side;
	float	value;

	AngleVectors (angles, NULL, right, NULL);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);

	value = cl_rollangle.value;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}

void V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time)
		return;		// something else is keeping it from drifting

	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch (void)
{
	float		delta, move;

	if (noclip_anglehack || !cl.onground || cls.demoplayback )
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += cl.realframetime;

		if ( cl.driftmove > v_centermove.value)
		{
			V_StartPitchDrift ();
		}
		return;
	}

	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = cl.realframetime * cl.pitchvel;
	cl.pitchvel += cl.realframetime * v_centerspeed.value;

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}


/*
==============================================================================

						SCREEN FLASHES

==============================================================================
*/


/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int armor, blood;
	vec3_t from;
	//vec3_t forward, right;
	vec3_t localfrom;
	entity_t *ent;
	//float side;
	float count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	MSG_ReadVector(from, cls.protocol);

	// Send the Dmg Globals to CSQC
	CL_VM_UpdateDmgGlobals(blood, armor, from);

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// put sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	cl.cshifts[CSHIFT_DAMAGE].alphafade = 150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

	// calculate view angle kicks
	if (cl.entities[cl.viewentity].state_current.active)
	{
		ent = &cl.entities[cl.viewentity];
		Matrix4x4_Transform(&ent->render.inversematrix, from, localfrom);
		VectorNormalize(localfrom);
		v_dmg_pitch = count * localfrom[0] * v_kickpitch.value;
		v_dmg_roll = count * localfrom[1] * v_kickroll.value;
		v_dmg_time = v_kicktime.value;
	}
}

static cshift_t v_cshift;

/*
==================
V_cshift_f
==================
*/
static void V_cshift_f (void)
{
	v_cshift.destcolor[0] = atof(Cmd_Argv(1));
	v_cshift.destcolor[1] = atof(Cmd_Argv(2));
	v_cshift.destcolor[2] = atof(Cmd_Argv(3));
	v_cshift.percent = atof(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
static void V_BonusFlash_f (void)
{
	if(Cmd_Argc() == 1)
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
		cl.cshifts[CSHIFT_BONUS].percent = 50;
		cl.cshifts[CSHIFT_BONUS].alphafade = 100;
	}
	else if(Cmd_Argc() >= 4 && Cmd_Argc() <= 6)
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = atof(Cmd_Argv(1)) * 255;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = atof(Cmd_Argv(2)) * 255;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = atof(Cmd_Argv(3)) * 255;
		if(Cmd_Argc() >= 5)
			cl.cshifts[CSHIFT_BONUS].percent = atof(Cmd_Argv(4)) * 255; // yes, these are HEXADECIMAL percent ;)
		else
			cl.cshifts[CSHIFT_BONUS].percent = 50;
		if(Cmd_Argc() >= 6)
			cl.cshifts[CSHIFT_BONUS].alphafade = atof(Cmd_Argv(5)) * 255;
		else
			cl.cshifts[CSHIFT_BONUS].alphafade = 100;
	}
	else
		Con_Printf("usage:\nbf, or bf R G B [A [alphafade]]\n");
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

extern matrix4x4_t viewmodelmatrix_nobob;
extern matrix4x4_t viewmodelmatrix_withbob;

#include "cl_collision.h"
#include "csprogs.h"

/*
==================
V_CalcRefdef

==================
*/
#if 0
static vec3_t eyeboxmins = {-16, -16, -24};
static vec3_t eyeboxmaxs = { 16,  16,  32};
#endif

static vec_t lowpass(vec_t value, vec_t frac, vec_t *store)
{
	frac = bound(0, frac, 1);
	return (*store = *store * (1 - frac) + value * frac);
}

static vec_t lowpass_limited(vec_t value, vec_t frac, vec_t limit, vec_t *store)
{
	lowpass(value, frac, store);
	return (*store = bound(value - limit, *store, value + limit));
}

static vec_t highpass(vec_t value, vec_t frac, vec_t *store)
{
	return value - lowpass(value, frac, store);
}

static vec_t highpass_limited(vec_t value, vec_t frac, vec_t limit, vec_t *store)
{
	return value - lowpass_limited(value, frac, limit, store);
}

static void lowpass3(vec3_t value, vec_t fracx, vec_t fracy, vec_t fracz, vec3_t store, vec3_t out)
{
	out[0] = lowpass(value[0], fracx, &store[0]);
	out[1] = lowpass(value[1], fracy, &store[1]);
	out[2] = lowpass(value[2], fracz, &store[2]);
}

static void highpass3(vec3_t value, vec_t fracx, vec_t fracy, vec_t fracz, vec3_t store, vec3_t out)
{
	out[0] = highpass(value[0], fracx, &store[0]);
	out[1] = highpass(value[1], fracy, &store[1]);
	out[2] = highpass(value[2], fracz, &store[2]);
}

static void highpass3_limited(vec3_t value, vec_t fracx, vec_t limitx, vec_t fracy, vec_t limity, vec_t fracz, vec_t limitz, vec3_t store, vec3_t out)
{
	out[0] = highpass_limited(value[0], fracx, limitx, &store[0]);
	out[1] = highpass_limited(value[1], fracy, limity, &store[1]);
	out[2] = highpass_limited(value[2], fracz, limitz, &store[2]);
}

void V_CalcRefdef (void)
{
	entity_t *ent;
	float vieworg[3], viewangles[3], smoothtime;
	float gunorg[3], gunangles[3];
	matrix4x4_t tmpmatrix;
	
	static float viewheightavg;
	float viewheight;	
#if 0
// begin of chase camera bounding box size for proper collisions by Alexander Zubov
	vec3_t camboxmins = {-3, -3, -3};
	vec3_t camboxmaxs = {3, 3, 3};
// end of chase camera bounding box size for proper collisions by Alexander Zubov
#endif
	trace_t trace;
	VectorClear(gunorg);
	viewmodelmatrix_nobob = identitymatrix;
	viewmodelmatrix_withbob = identitymatrix;
	r_refdef.view.matrix = identitymatrix;
	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		// ent is the view entity (visible when out of body)
		ent = &cl.entities[cl.viewentity];
		// player can look around, so take the origin from the entity,
		// and the angles from the input system
		Matrix4x4_OriginFromMatrix(&ent->render.matrix, vieworg);
		VectorCopy(cl.viewangles, viewangles);

		// calculate how much time has passed since the last V_CalcRefdef
		smoothtime = bound(0, cl.time - cl.stairsmoothtime, 0.1);
		cl.stairsmoothtime = cl.time;

		// fade damage flash
		if (v_dmg_time > 0)
			v_dmg_time -= bound(0, smoothtime, 0.1);

		if (cl.intermission)
		{
			// entity is a fixed camera, just copy the matrix
			if (cls.protocol == PROTOCOL_QUAKEWORLD)
				Matrix4x4_CreateFromQuakeEntity(&r_refdef.view.matrix, cl.qw_intermission_origin[0], cl.qw_intermission_origin[1], cl.qw_intermission_origin[2], cl.qw_intermission_angles[0], cl.qw_intermission_angles[1], cl.qw_intermission_angles[2], 1);
			else
			{
				r_refdef.view.matrix = ent->render.matrix;
				Matrix4x4_AdjustOrigin(&r_refdef.view.matrix, 0, 0, cl.stats[STAT_VIEWHEIGHT]);
			}
			Matrix4x4_Copy(&viewmodelmatrix_nobob, &r_refdef.view.matrix);
			Matrix4x4_ConcatScale(&viewmodelmatrix_nobob, cl_viewmodel_scale.value);
			Matrix4x4_Copy(&viewmodelmatrix_withbob, &viewmodelmatrix_nobob);
		}
		else
		{
			// smooth stair stepping, but only if onground and enabled
			if (!cl.onground || cl_stairsmoothspeed.value <= 0 || !ent->persistent.trail_allowed) // FIXME use a better way to detect teleport/warp
				cl.stairsmoothz = vieworg[2];
			else
			{
				if (cl.stairsmoothz < vieworg[2])
					vieworg[2] = cl.stairsmoothz = bound(vieworg[2] - cl.movevars_stepheight, cl.stairsmoothz + smoothtime * cl_stairsmoothspeed.value, vieworg[2]);
				else if (cl.stairsmoothz > vieworg[2])
					vieworg[2] = cl.stairsmoothz = bound(vieworg[2], cl.stairsmoothz - smoothtime * cl_stairsmoothspeed.value, vieworg[2] + cl.movevars_stepheight);
			}

			// apply qw weapon recoil effect (this did not work in QW)
			// TODO: add a cvar to disable this
			viewangles[PITCH] += cl.qw_weaponkick;

			// apply the viewofs (even if chasecam is used)
			// Samual: Lets add smoothing for this too so that things like crouching are done with a transition.
			viewheight = bound(0, (cl.time - cl.oldtime) / max(0.0001, cl_smoothviewheight.value), 1);
			viewheightavg = viewheightavg * (1 - viewheight) + cl.stats[STAT_VIEWHEIGHT] * viewheight;
			vieworg[2] += viewheightavg;

			if (chase_active.value)
			{
				// observing entity from third person. Added "campitch" by Alexander "motorsep" Zubov
				vec_t camback, camup, dist, campitch, forward[3], chase_dest[3];

				camback = chase_back.value;
				camup = chase_up.value;
				campitch = chase_pitchangle.value;

				AngleVectors(viewangles, forward, NULL, NULL);

				if (chase_overhead.integer)
				{
#if 1
					vec3_t offset;
					vec3_t bestvieworg;
#endif
					vec3_t up;
					viewangles[PITCH] = 0;
					AngleVectors(viewangles, forward, NULL, up);
					// trace a little further so it hits a surface more consistently (to avoid 'snapping' on the edge of the range)
					chase_dest[0] = vieworg[0] - forward[0] * camback + up[0] * camup;
					chase_dest[1] = vieworg[1] - forward[1] * camback + up[1] * camup;
					chase_dest[2] = vieworg[2] - forward[2] * camback + up[2] * camup;
#if 0
#if 1
					//trace = CL_TraceLine(vieworg, eyeboxmins, eyeboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
					trace = CL_TraceLine(vieworg, camboxmins, camboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
#else
					//trace = CL_TraceBox(vieworg, eyeboxmins, eyeboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
					trace = CL_TraceBox(vieworg, camboxmins, camboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
#endif
					VectorCopy(trace.endpos, vieworg);
					vieworg[2] -= 8;
#else
					// trace from first person view location to our chosen third person view location
#if 1
					trace = CL_TraceLine(vieworg, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false, true);
#else
					trace = CL_TraceBox(vieworg, camboxmins, camboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
#endif
					VectorCopy(trace.endpos, bestvieworg);
					offset[2] = 0;
					for (offset[0] = -16;offset[0] <= 16;offset[0] += 8)
					{
						for (offset[1] = -16;offset[1] <= 16;offset[1] += 8)
						{
							AngleVectors(viewangles, NULL, NULL, up);
							chase_dest[0] = vieworg[0] - forward[0] * camback + up[0] * camup + offset[0];
							chase_dest[1] = vieworg[1] - forward[1] * camback + up[1] * camup + offset[1];
							chase_dest[2] = vieworg[2] - forward[2] * camback + up[2] * camup + offset[2];
#if 1
							trace = CL_TraceLine(vieworg, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false, true);
#else
							trace = CL_TraceBox(vieworg, camboxmins, camboxmaxs, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false);
#endif
							if (bestvieworg[2] > trace.endpos[2])
								bestvieworg[2] = trace.endpos[2];
						}
					}
					bestvieworg[2] -= 8;
					VectorCopy(bestvieworg, vieworg);
#endif
					viewangles[PITCH] = campitch;
				}
				else
				{
					if (gamemode == GAME_GOODVSBAD2 && chase_stevie.integer)
					{
						// look straight down from high above
						viewangles[PITCH] = 90;
						camback = 2048;
						VectorSet(forward, 0, 0, -1);
					}

					// trace a little further so it hits a surface more consistently (to avoid 'snapping' on the edge of the range)
					dist = -camback - 8;
					chase_dest[0] = vieworg[0] + forward[0] * dist;
					chase_dest[1] = vieworg[1] + forward[1] * dist;
					chase_dest[2] = vieworg[2] + forward[2] * dist + camup;
					trace = CL_TraceLine(vieworg, chase_dest, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY | SUPERCONTENTS_SKY, true, false, NULL, false, true);
					VectorMAMAM(1, trace.endpos, 8, forward, 4, trace.plane.normal, vieworg);
				}
			}
			else
			{
				// first person view from entity
				// angles
				if (cl.stats[STAT_HEALTH] <= 0 && v_deathtilt.integer)
					viewangles[ROLL] = v_deathtiltangle.value;
				VectorAdd(viewangles, cl.punchangle, viewangles);
				viewangles[ROLL] += V_CalcRoll(cl.viewangles, cl.velocity);
				if (v_dmg_time > 0)
				{
					viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
					viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
				}
				// origin
				VectorAdd(vieworg, cl.punchvector, vieworg);
				if (cl.stats[STAT_HEALTH] > 0)
				{
					double xyspeed, bob, bobfall;
					float cycle;
					vec_t frametime;

					//frametime = cl.realframetime * cl.movevars_timescale;
					frametime = (cl.time - cl.oldtime) * cl.movevars_timescale;
					
					// 1. if we teleported, clear the frametime... the lowpass will recover the previous value then
					if(!ent->persistent.trail_allowed) // FIXME improve this check
					{
						// try to fix the first highpass; result is NOT
						// perfect! TODO find a better fix
						VectorCopy(viewangles, cl.gunangles_prev);
						VectorCopy(vieworg, cl.gunorg_prev);
					}

					// 2. for the gun origin, only keep the high frequency (non-DC) parts, which is "somewhat like velocity"
					VectorAdd(cl.gunorg_highpass, cl.gunorg_prev, cl.gunorg_highpass);
					highpass3_limited(vieworg, frametime*cl_followmodel_side_highpass1.value, cl_followmodel_side_limit.value, frametime*cl_followmodel_side_highpass1.value, cl_followmodel_side_limit.value, frametime*cl_followmodel_up_highpass1.value, cl_followmodel_up_limit.value, cl.gunorg_highpass, gunorg);
					VectorCopy(vieworg, cl.gunorg_prev);
					VectorSubtract(cl.gunorg_highpass, cl.gunorg_prev, cl.gunorg_highpass);

					// in the highpass, we _store_ the DIFFERENCE to the actual view angles...
					VectorAdd(cl.gunangles_highpass, cl.gunangles_prev, cl.gunangles_highpass);
					cl.gunangles_highpass[PITCH] += 360 * floor((viewangles[PITCH] - cl.gunangles_highpass[PITCH]) / 360 + 0.5);
					cl.gunangles_highpass[YAW] += 360 * floor((viewangles[YAW] - cl.gunangles_highpass[YAW]) / 360 + 0.5);
					cl.gunangles_highpass[ROLL] += 360 * floor((viewangles[ROLL] - cl.gunangles_highpass[ROLL]) / 360 + 0.5);
					highpass3_limited(viewangles, frametime*cl_leanmodel_up_highpass1.value, cl_leanmodel_up_limit.value, frametime*cl_leanmodel_side_highpass1.value, cl_leanmodel_side_limit.value, 0, 0, cl.gunangles_highpass, gunangles);
					VectorCopy(viewangles, cl.gunangles_prev);
					VectorSubtract(cl.gunangles_highpass, cl.gunangles_prev, cl.gunangles_highpass);

					// 3. calculate the RAW adjustment vectors
					gunorg[0] *= (cl_followmodel.value ? -cl_followmodel_side_speed.value : 0);
					gunorg[1] *= (cl_followmodel.value ? -cl_followmodel_side_speed.value : 0);
					gunorg[2] *= (cl_followmodel.value ? -cl_followmodel_up_speed.value : 0);

					gunangles[PITCH] *= (cl_leanmodel.value ? -cl_leanmodel_up_speed.value : 0);
					gunangles[YAW] *= (cl_leanmodel.value ? -cl_leanmodel_side_speed.value : 0);
					gunangles[ROLL] = 0;

					// 4. perform highpass/lowpass on the adjustment vectors (turning velocity into acceleration!)
					//    trick: we must do the lowpass LAST, so the lowpass vector IS the final vector!
					highpass3(gunorg, frametime*cl_followmodel_side_highpass.value, frametime*cl_followmodel_side_highpass.value, frametime*cl_followmodel_up_highpass.value, cl.gunorg_adjustment_highpass, gunorg);
					lowpass3(gunorg, frametime*cl_followmodel_side_lowpass.value, frametime*cl_followmodel_side_lowpass.value, frametime*cl_followmodel_up_lowpass.value, cl.gunorg_adjustment_lowpass, gunorg);
					// we assume here: PITCH = 0, YAW = 1, ROLL = 2
					highpass3(gunangles, frametime*cl_leanmodel_up_highpass.value, frametime*cl_leanmodel_side_highpass.value, 0, cl.gunangles_adjustment_highpass, gunangles);
					lowpass3(gunangles, frametime*cl_leanmodel_up_lowpass.value, frametime*cl_leanmodel_side_lowpass.value, 0, cl.gunangles_adjustment_lowpass, gunangles);

					// 5. use the adjusted vectors
					VectorAdd(vieworg, gunorg, gunorg);
					VectorAdd(viewangles, gunangles, gunangles);

					// bounded XY speed, used by several effects below
					xyspeed = bound (0, sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]), 400);

					// vertical view bobbing code
					if (cl_bob.value && cl_bobcycle.value)
					{
						// LordHavoc: this code is *weird*, but not replacable (I think it
						// should be done in QC on the server, but oh well, quake is quake)
						// LordHavoc: figured out bobup: the time at which the sin is at 180
						// degrees (which allows lengthening or squishing the peak or valley)
						cycle = cl.time / cl_bobcycle.value;
						cycle -= (int) cycle;
						if (cycle < cl_bobup.value)
							cycle = sin(M_PI * cycle / cl_bobup.value);
						else
							cycle = sin(M_PI + M_PI * (cycle-cl_bobup.value)/(1.0 - cl_bobup.value));
						// bob is proportional to velocity in the xy plane
						// (don't count Z, or jumping messes it up)
						bob = xyspeed * bound(0, cl_bob.value, 0.05);
						bob = bob*0.3 + bob*0.7*cycle;
						vieworg[2] += bob;
						// we also need to adjust gunorg, or this appears like pushing the gun!
						// In the old code, this was applied to vieworg BEFORE copying to gunorg,
						// but this is not viable with the new followmodel code as that would mean
						// that followmodel would work on the munged-by-bob vieworg and do feedback
						gunorg[2] += bob;
					}

					// horizontal view bobbing code
					if (cl_bob2.value && cl_bob2cycle.value)
					{
						vec3_t bob2vel;
						vec3_t forward, right, up;
						float side, front;

						cycle = cl.time / cl_bob2cycle.value;
						cycle -= (int) cycle;
						if (cycle < 0.5)
							cycle = cos(M_PI * cycle / 0.5); // cos looks better here with the other view bobbing using sin
						else
							cycle = cos(M_PI + M_PI * (cycle-0.5)/0.5);
						bob = bound(0, cl_bob2.value, 0.05) * cycle;

						// this value slowly decreases from 1 to 0 when we stop touching the ground.
						// The cycle is later multiplied with it so the view smooths back to normal
						if (cl.onground && !cl.cmd.jump) // also block the effect while the jump button is pressed, to avoid twitches when bunny-hopping
							cl.bob2_smooth = 1;
						else
						{
							if(cl.bob2_smooth > 0)
								cl.bob2_smooth -= bound(0, cl_bob2smooth.value, 1);
							else
								cl.bob2_smooth = 0;
						}

						// calculate the front and side of the player between the X and Y axes
						AngleVectors(viewangles, forward, right, up);
						// now get the speed based on those angles. The bounds should match the same value as xyspeed's
						side = bound(-400, DotProduct (cl.velocity, right) * cl.bob2_smooth, 400);
						front = bound(-400, DotProduct (cl.velocity, forward) * cl.bob2_smooth, 400);
						VectorScale(forward, bob, forward);
						VectorScale(right, bob, right);
						// we use side with forward and front with right, so the bobbing goes
						// to the side when we walk forward and to the front when we strafe
						VectorMAMAM(side, forward, front, right, 0, up, bob2vel);
						vieworg[0] += bob2vel[0];
						vieworg[1] += bob2vel[1];
						// we also need to adjust gunorg, or this appears like pushing the gun!
						// In the old code, this was applied to vieworg BEFORE copying to gunorg,
						// but this is not viable with the new followmodel code as that would mean
						// that followmodel would work on the munged-by-bob vieworg and do feedback
						gunorg[0] += bob2vel[0];
						gunorg[1] += bob2vel[1];
					}

					// fall bobbing code
					// causes the view to swing down and back up when touching the ground
					if (cl_bobfall.value && cl_bobfallcycle.value)
					{
						if (!cl.onground)
						{
							cl.bobfall_speed = bound(-400, cl.velocity[2], 0) * bound(0, cl_bobfall.value, 0.1);
							if (cl.velocity[2] < -cl_bobfallminspeed.value)
								cl.bobfall_swing = 1;
							else
								cl.bobfall_swing = 0; // TODO really?
						}
						else
						{
							cl.bobfall_swing = max(0, cl.bobfall_swing - cl_bobfallcycle.value * frametime);

							bobfall = sin(M_PI * cl.bobfall_swing) * cl.bobfall_speed;
							vieworg[2] += bobfall;
							gunorg[2] += bobfall;
						}
					}

					// gun model bobbing code
					if (cl_bobmodel.value)
					{
						// calculate for swinging gun model
						// the gun bobs when running on the ground, but doesn't bob when you're in the air.
						// Sajt: I tried to smooth out the transitions between bob and no bob, which works
						// for the most part, but for some reason when you go through a message trigger or
						// pick up an item or anything like that it will momentarily jolt the gun.
						vec3_t forward, right, up;
						float bspeed;
						float s;
						float t;

						s = cl.time * cl_bobmodel_speed.value;
						if (cl.onground)
						{
							if (cl.time - cl.hitgroundtime < 0.2)
							{
								// just hit the ground, speed the bob back up over the next 0.2 seconds
								t = cl.time - cl.hitgroundtime;
								t = bound(0, t, 0.2);
								t *= 5;
							}
							else
								t = 1;
						}
						else
						{
							// recently left the ground, slow the bob down over the next 0.2 seconds
							t = cl.time - cl.lastongroundtime;
							t = 0.2 - bound(0, t, 0.2);
							t *= 5;
						}

						bspeed = xyspeed * 0.01f;
						AngleVectors (gunangles, forward, right, up);
						bob = bspeed * cl_bobmodel_side.value * cl_viewmodel_scale.value * sin (s) * t;
						VectorMA (gunorg, bob, right, gunorg);
						bob = bspeed * cl_bobmodel_up.value * cl_viewmodel_scale.value * cos (s * 2) * t;
						VectorMA (gunorg, bob, up, gunorg);
					}
				}
			}
			// calculate a view matrix for rendering the scene
			if (v_idlescale.value)
			{
				viewangles[0] += v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
				viewangles[1] += v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
				viewangles[2] += v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
			}
			Matrix4x4_CreateFromQuakeEntity(&r_refdef.view.matrix, vieworg[0], vieworg[1], vieworg[2], viewangles[0], viewangles[1], viewangles[2], 1);

			// calculate a viewmodel matrix for use in view-attached entities
			Matrix4x4_Copy(&viewmodelmatrix_nobob, &r_refdef.view.matrix);
			Matrix4x4_ConcatScale(&viewmodelmatrix_nobob, cl_viewmodel_scale.value);

			Matrix4x4_CreateFromQuakeEntity(&viewmodelmatrix_withbob, gunorg[0], gunorg[1], gunorg[2], gunangles[0], gunangles[1], gunangles[2], cl_viewmodel_scale.value);
			VectorCopy(vieworg, cl.csqc_vieworiginfromengine);
			VectorCopy(viewangles, cl.csqc_viewanglesfromengine);

			Matrix4x4_Invert_Simple(&tmpmatrix, &r_refdef.view.matrix);
			Matrix4x4_Concat(&cl.csqc_viewmodelmatrixfromengine, &tmpmatrix, &viewmodelmatrix_withbob);
		}
	}
}

void V_FadeViewFlashs(void)
{
	// don't flash if time steps backwards
	if (cl.time <= cl.oldtime)
		return;
	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime)*cl.cshifts[CSHIFT_DAMAGE].alphafade;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime)*cl.cshifts[CSHIFT_BONUS].alphafade;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;
}

void V_CalcViewBlend(void)
{
	float a2;
	int j;
	r_refdef.viewblend[0] = 0;
	r_refdef.viewblend[1] = 0;
	r_refdef.viewblend[2] = 0;
	r_refdef.viewblend[3] = 0;
	r_refdef.frustumscale_x = 1;
	r_refdef.frustumscale_y = 1;
	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		// set contents color
		int supercontents;
		vec3_t vieworigin;
		Matrix4x4_OriginFromMatrix(&r_refdef.view.matrix, vieworigin);
		supercontents = CL_PointSuperContents(vieworigin);
		if (supercontents & SUPERCONTENTS_LIQUIDSMASK)
		{
			r_refdef.frustumscale_x *= 1 - (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
			r_refdef.frustumscale_y *= 1 - (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
			if (supercontents & SUPERCONTENTS_LAVA)
			{
				cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 255;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 80;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 0;
			}
			else if (supercontents & SUPERCONTENTS_SLIME)
			{
				cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 25;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 5;
			}
			else
			{
				cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 130;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 80;
				cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 50;
			}
			cl.cshifts[CSHIFT_CONTENTS].percent = 150 * 0.5;
		}
		else
		{
			cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
			cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 0;
			cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 0;
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;
		}

		if (gamemode != GAME_TRANSFUSION)
		{
			if (cl.stats[STAT_ITEMS] & IT_QUAD)
			{
				cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
				cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
				cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
				cl.cshifts[CSHIFT_POWERUP].percent = 30;
			}
			else if (cl.stats[STAT_ITEMS] & IT_SUIT)
			{
				cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
				cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
				cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
				cl.cshifts[CSHIFT_POWERUP].percent = 20;
			}
			else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
			{
				cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
				cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
				cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
				cl.cshifts[CSHIFT_POWERUP].percent = 100;
			}
			else if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
			{
				cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
				cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
				cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
				cl.cshifts[CSHIFT_POWERUP].percent = 30;
			}
			else
				cl.cshifts[CSHIFT_POWERUP].percent = 0;
		}

		cl.cshifts[CSHIFT_VCSHIFT].destcolor[0] = v_cshift.destcolor[0];
		cl.cshifts[CSHIFT_VCSHIFT].destcolor[1] = v_cshift.destcolor[1];
		cl.cshifts[CSHIFT_VCSHIFT].destcolor[2] = v_cshift.destcolor[2];
		cl.cshifts[CSHIFT_VCSHIFT].percent = v_cshift.percent;

		// LordHavoc: fixed V_CalcBlend
		for (j = 0;j < NUM_CSHIFTS;j++)
		{
			a2 = bound(0.0f, cl.cshifts[j].percent * (1.0f / 255.0f), 1.0f);
			if (a2 > 0)
			{
				VectorLerp(r_refdef.viewblend, a2, cl.cshifts[j].destcolor, r_refdef.viewblend);
				r_refdef.viewblend[3] = (1 - (1 - r_refdef.viewblend[3]) * (1 - a2)); // correct alpha multiply...  took a while to find it on the web
			}
		}
		// saturate color (to avoid blending in black)
		if (r_refdef.viewblend[3])
		{
			a2 = 1 / r_refdef.viewblend[3];
			VectorScale(r_refdef.viewblend, a2, r_refdef.viewblend);
		}
		r_refdef.viewblend[0] = bound(0.0f, r_refdef.viewblend[0], 255.0f);
		r_refdef.viewblend[1] = bound(0.0f, r_refdef.viewblend[1], 255.0f);
		r_refdef.viewblend[2] = bound(0.0f, r_refdef.viewblend[2], 255.0f);
		r_refdef.viewblend[3] = bound(0.0f, r_refdef.viewblend[3] * gl_polyblend.value, 1.0f);
		if (vid.sRGB3D)
		{
			r_refdef.viewblend[0] = Image_LinearFloatFromsRGB(r_refdef.viewblend[0]);
			r_refdef.viewblend[1] = Image_LinearFloatFromsRGB(r_refdef.viewblend[1]);
			r_refdef.viewblend[2] = Image_LinearFloatFromsRGB(r_refdef.viewblend[2]);
		}
		else
		{
			r_refdef.viewblend[0] *= (1.0f/256.0f);
			r_refdef.viewblend[1] *= (1.0f/256.0f);
			r_refdef.viewblend[2] *= (1.0f/256.0f);
		}
		
		// Samual: Ugly hack, I know. But it's the best we can do since
		// there is no way to detect client states from the engine.
		if (cl.stats[STAT_HEALTH] <= 0 && cl.stats[STAT_HEALTH] != -666 && 
			cl.stats[STAT_HEALTH] != -2342 && cl_deathfade.value > 0)
		{
			cl.deathfade += cl_deathfade.value * max(0.00001, cl.time - cl.oldtime);
			cl.deathfade = bound(0.0f, cl.deathfade, 0.9f);
		}
		else
			cl.deathfade = 0.0f;

		if(cl.deathfade > 0)
		{
			float a;
			float deathfadevec[3] = {0.3f, 0.0f, 0.0f};
			a = r_refdef.viewblend[3] + cl.deathfade - r_refdef.viewblend[3]*cl.deathfade;
			if(a > 0)
				VectorMAM(r_refdef.viewblend[3] * (1 - cl.deathfade) / a, r_refdef.viewblend, cl.deathfade / a, deathfadevec, r_refdef.viewblend);
			r_refdef.viewblend[3] = a;
		}
	}
}

//============================================================================

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f, "sets tint color of view");
	Cmd_AddCommand ("bf", V_BonusFlash_f, "briefly flashes a bright color tint on view (used when items are picked up); optionally takes R G B [A [alphafade]] arguments to specify how the flash looks");
	Cmd_AddCommand ("centerview", V_StartPitchDrift, "gradually recenter view (stop looking up/down)");

	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);

	Cvar_RegisterVariable (&v_idlescale);
	Cvar_RegisterVariable (&crosshair);

	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);
	Cvar_RegisterVariable (&cl_bob2);
	Cvar_RegisterVariable (&cl_bob2cycle);
	Cvar_RegisterVariable (&cl_bob2smooth);
	Cvar_RegisterVariable (&cl_bobfall);
	Cvar_RegisterVariable (&cl_bobfallcycle);
	Cvar_RegisterVariable (&cl_bobfallminspeed);
	Cvar_RegisterVariable (&cl_bobmodel);
	Cvar_RegisterVariable (&cl_bobmodel_side);
	Cvar_RegisterVariable (&cl_bobmodel_up);
	Cvar_RegisterVariable (&cl_bobmodel_speed);

	Cvar_RegisterVariable (&cl_leanmodel);
	Cvar_RegisterVariable (&cl_leanmodel_side_speed);
	Cvar_RegisterVariable (&cl_leanmodel_side_limit);
	Cvar_RegisterVariable (&cl_leanmodel_side_highpass1);
	Cvar_RegisterVariable (&cl_leanmodel_side_lowpass);
	Cvar_RegisterVariable (&cl_leanmodel_side_highpass);
	Cvar_RegisterVariable (&cl_leanmodel_up_speed);
	Cvar_RegisterVariable (&cl_leanmodel_up_limit);
	Cvar_RegisterVariable (&cl_leanmodel_up_highpass1);
	Cvar_RegisterVariable (&cl_leanmodel_up_lowpass);
	Cvar_RegisterVariable (&cl_leanmodel_up_highpass);

	Cvar_RegisterVariable (&cl_followmodel);
	Cvar_RegisterVariable (&cl_followmodel_side_speed);
	Cvar_RegisterVariable (&cl_followmodel_side_limit);
	Cvar_RegisterVariable (&cl_followmodel_side_highpass1);
	Cvar_RegisterVariable (&cl_followmodel_side_lowpass);
	Cvar_RegisterVariable (&cl_followmodel_side_highpass);
	Cvar_RegisterVariable (&cl_followmodel_up_speed);
	Cvar_RegisterVariable (&cl_followmodel_up_limit);
	Cvar_RegisterVariable (&cl_followmodel_up_highpass1);
	Cvar_RegisterVariable (&cl_followmodel_up_lowpass);
	Cvar_RegisterVariable (&cl_followmodel_up_highpass);

	Cvar_RegisterVariable (&cl_viewmodel_scale);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);

	Cvar_RegisterVariable (&cl_stairsmoothspeed);
	
	Cvar_RegisterVariable (&cl_smoothviewheight);

	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_active);
	Cvar_RegisterVariable (&chase_overhead);
	Cvar_RegisterVariable (&chase_pitchangle);
	Cvar_RegisterVariable (&chase_stevie);

	Cvar_RegisterVariable (&v_deathtilt);
	Cvar_RegisterVariable (&v_deathtiltangle);
}

