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

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t	cl_rollspeed = {0, "cl_rollspeed", "200"};
cvar_t	cl_rollangle = {0, "cl_rollangle", "2.0"};

cvar_t	cl_bob = {0, "cl_bob","0.02"};
cvar_t	cl_bobcycle = {0, "cl_bobcycle","0.6"};
cvar_t	cl_bobup = {0, "cl_bobup","0.5"};

cvar_t	v_kicktime = {0, "v_kicktime", "0.5"};
cvar_t	v_kickroll = {0, "v_kickroll", "0.6"};
cvar_t	v_kickpitch = {0, "v_kickpitch", "0.6"};

cvar_t	v_iyaw_cycle = {0, "v_iyaw_cycle", "2"};
cvar_t	v_iroll_cycle = {0, "v_iroll_cycle", "0.5"};
cvar_t	v_ipitch_cycle = {0, "v_ipitch_cycle", "1"};
cvar_t	v_iyaw_level = {0, "v_iyaw_level", "0.3"};
cvar_t	v_iroll_level = {0, "v_iroll_level", "0.1"};
cvar_t	v_ipitch_level = {0, "v_ipitch_level", "0.3"};

cvar_t	v_idlescale = {0, "v_idlescale", "0"};

cvar_t	crosshair = {CVAR_SAVE, "crosshair", "0"};

cvar_t	v_centermove = {0, "v_centermove", "0.15"};
cvar_t	v_centerspeed = {0, "v_centerspeed","500"};

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

static float V_CalcBob (void)
{
	double bob, cycle;

	// LordHavoc: easy case
	if (cl_bob.value == 0)
		return 0;
	if (cl_bobcycle.value == 0)
		return 0;

	// LordHavoc: FIXME: this code is *weird*, redesign it sometime
	cycle = cl.time  / cl_bobcycle.value;
	cycle -= (int) cycle;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

	// bob is proportional to velocity in the xy plane
	// (don't count Z, or jumping messes it up)

	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
	bob = bob*0.3 + bob*0.7*sin(cycle);
	bob = bound(-7, bob, 4);
	return bob;

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
static void V_DriftPitch (void)
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
			cl.driftmove += cl.frametime;

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

	move = cl.frametime * cl.pitchvel;
	cl.pitchvel += cl.frametime * v_centerspeed.value;

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
	int i, armor, blood;
	vec3_t from, forward, right;
	entity_t *ent;
	float side, count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// put sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
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

//
// calculate view angle kicks
//
	ent = &cl_entities[cl.viewentity];

	VectorSubtract (from, ent->render.origin, from);
	VectorNormalize (from);

	AngleVectors (ent->render.angles, forward, right, NULL);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;

	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}

static cshift_t v_cshift;

/*
==================
V_cshift_f
==================
*/
static void V_cshift_f (void)
{
	v_cshift.destcolor[0] = atoi(Cmd_Argv(1));
	v_cshift.destcolor[1] = atoi(Cmd_Argv(2));
	v_cshift.destcolor[2] = atoi(Cmd_Argv(3));
	v_cshift.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
static void V_BonusFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_UpdateBlends
=============
*/
void V_UpdateBlends (void)
{
	float	r, g, b, a, a2;
	int		j;

	if (cls.signon != SIGNONS)
	{
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
		cl.cshifts[CSHIFT_BONUS].percent = 0;
		cl.cshifts[CSHIFT_CONTENTS].percent = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
		r_refdef.viewblend[0] = 0;
		r_refdef.viewblend[1] = 0;
		r_refdef.viewblend[2] = 0;
		r_refdef.viewblend[3] = 0;
		return;
	}

	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime)*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime)*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	// set contents color
	switch (Mod_PointContents (r_refdef.vieworg, cl.worldmodel))
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = v_cshift.destcolor[0];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = v_cshift.destcolor[1];
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = v_cshift.destcolor[2];
		cl.cshifts[CSHIFT_CONTENTS].percent = v_cshift.percent;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 255;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 80;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 0;
		cl.cshifts[CSHIFT_CONTENTS].percent = 150 >> 1;
		break;
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 25;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 5;
		cl.cshifts[CSHIFT_CONTENTS].percent = 150 >> 1;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 130;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 80;
		cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 50;
		cl.cshifts[CSHIFT_CONTENTS].percent = 128 >> 1;
	}

	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	}
	else if (cl.items & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	}
	else if (cl.items & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;

	// LordHavoc: fixed V_CalcBlend
	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)
	{
		a2 = cl.cshifts[j].percent * (1.0f / 255.0f);

		if (a2 < 0)
			continue;
		if (a2 > 1)
			a2 = 1;
		r += (cl.cshifts[j].destcolor[0]-r) * a2;
		g += (cl.cshifts[j].destcolor[1]-g) * a2;
		b += (cl.cshifts[j].destcolor[2]-b) * a2;
		a = 1 - (1 - a) * (1 - a2); // correct alpha multiply...  took a while to find it on the web
	}
	// saturate color (to avoid blending in black)
	if (a)
	{
		a2 = 1 / a;
		r *= a2;
		g *= a2;
		b *= a2;
	}

	r_refdef.viewblend[0] = bound(0, r * (1.0/255.0), 1);
	r_refdef.viewblend[1] = bound(0, g * (1.0/255.0), 1);
	r_refdef.viewblend[2] = bound(0, b * (1.0/255.0), 1);
	r_refdef.viewblend[3] = bound(0, a              , 1);
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

/*
==============
V_AddIdle

Idle swaying
==============
*/
static void V_AddIdle (float idle)
{
	r_refdef.viewangles[ROLL] += idle * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += idle * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += idle * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent, *view;
	vec3_t		forward;
	vec3_t		angles;
	float		bob;
	float		side;

	if (cls.state != ca_connected || cls.signon != SIGNONS)
		return;

	// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
	// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	V_DriftPitch ();

	VectorCopy (cl.viewentorigin, r_refdef.vieworg);
	if (!intimerefresh)
		VectorCopy (cl.viewangles, r_refdef.viewangles);

	if (cl.intermission)
	{
		view->render.model = NULL;
		VectorCopy (ent->render.angles, r_refdef.viewangles);
		V_AddIdle (1);
	}
	else if (chase_active.value)
	{
		r_refdef.vieworg[2] += cl.viewheight;
		Chase_Update ();
		V_AddIdle (v_idlescale.value);
	}
	else
	{
		side = V_CalcRoll (cl_entities[cl.viewentity].render.angles, cl.velocity);
		r_refdef.viewangles[ROLL] += side;

		if (v_dmg_time > 0)
		{
			r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
			r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
			v_dmg_time -= cl.frametime;
		}

		if (cl.stats[STAT_HEALTH] <= 0)
			r_refdef.viewangles[ROLL] = 80;	// dead view angle

		V_AddIdle (v_idlescale.value);

		// offsets
		angles[PITCH] = -ent->render.angles[PITCH];	// because entity pitches are actually backward
		angles[YAW] = ent->render.angles[YAW];
		angles[ROLL] = ent->render.angles[ROLL];

		AngleVectors (angles, forward, NULL, NULL);

		bob = V_CalcBob ();

		r_refdef.vieworg[2] += cl.viewheight + bob;

		// set up gun
		view->state_current.modelindex = cl.stats[STAT_WEAPON];
		view->state_current.frame = cl.stats[STAT_WEAPONFRAME];
		VectorCopy(r_refdef.vieworg, view->render.origin);
		//view->render.origin[0] = ent->render.origin[0] + bob * 0.4 * forward[0];
		//view->render.origin[1] = ent->render.origin[1] + bob * 0.4 * forward[1];
		//view->render.origin[2] = ent->render.origin[2] + bob * 0.4 * forward[2] + cl.viewheight + bob;
		view->render.angles[PITCH] = -r_refdef.viewangles[PITCH] - v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
		view->render.angles[YAW] = r_refdef.viewangles[YAW] - v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
		view->render.angles[ROLL] = r_refdef.viewangles[ROLL] - v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
		// FIXME: this setup code is somewhat evil (CL_LerpUpdate should be private?)
		CL_LerpUpdate(view);
		CL_BoundingBoxForEntity(&view->render);
		view->render.colormap = -1; // no special coloring
		view->render.alpha = ent->render.alpha; // LordHavoc: if the player is transparent, so is the gun
		view->render.effects = ent->render.effects;
		view->render.scale = 1.0 / 3.0;

		// LordHavoc: origin view kick added
		if (!intimerefresh)
		{
			VectorAdd(r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);
			VectorAdd(r_refdef.vieworg, cl.punchvector, r_refdef.vieworg);
		}

		// copy to refdef
		r_refdef.viewent = view->render;
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
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

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

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);
}

