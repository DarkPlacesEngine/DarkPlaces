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

cvar_t	v_punch = {0, "v_punch", "1"};

cvar_t	v_iyaw_cycle = {0, "v_iyaw_cycle", "2"};
cvar_t	v_iroll_cycle = {0, "v_iroll_cycle", "0.5"};
cvar_t	v_ipitch_cycle = {0, "v_ipitch_cycle", "1"};
cvar_t	v_iyaw_level = {0, "v_iyaw_level", "0.3"};
cvar_t	v_iroll_level = {0, "v_iroll_level", "0.1"};
cvar_t	v_ipitch_level = {0, "v_ipitch_level", "0.3"};

cvar_t	v_idlescale = {0, "v_idlescale", "0"};

cvar_t	crosshair = {CVAR_SAVE, "crosshair", "0"};

//cvar_t	gl_cshiftpercent = {0, "gl_cshiftpercent", "100"};
cvar_t	gl_polyblend = {CVAR_SAVE, "gl_polyblend", "1"};

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
//	if (cl.inwater)
//		value *= 6;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;

	return side*sign;

}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (void)
{
	float	bob;
	float	cycle;
	
	cycle = cl.time - (int)(cl.time/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
//Con_Printf ("speed: %5.1f\n", Length(cl.velocity));
	bob = bob*0.3 + bob*0.7*sin(cycle);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;
	
}


//=============================================================================


void V_StartPitchDrift (void)
{
#if 1
	if (cl.laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}
#endif
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
	
//Con_Printf ("move: %f (%f)\n", move, cl.frametime);

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
 
 
cshift_t	cshift_empty = { {130,80,50}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

byte		ramps[3][256];
float		v_blend[4];		// rgba 0.0 - 1.0

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right;
	entity_t	*ent;
	float	side;
	float	count;
	
	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord ();

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// put sbar face into pain frame

	if (gl_polyblend.value)
	{
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


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	if (gl_polyblend.value)
	{
		cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
		cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
		cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
		cl.cshifts[CSHIFT_BONUS].percent = 50;
	}
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	cshift_t* c;
	c = &cl.cshifts[CSHIFT_CONTENTS]; // just to shorten the code below
	if (!gl_polyblend.value)
	{
		c->percent = 0;
		return;
	}
	switch (contents)
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
		//cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		c->destcolor[0] = cshift_empty.destcolor[0];
		c->destcolor[1] = cshift_empty.destcolor[1];
		c->destcolor[2] = cshift_empty.destcolor[2];
		c->percent = cshift_empty.percent;
		break;
	case CONTENTS_LAVA:
		//cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		c->destcolor[0] = cshift_lava.destcolor[0];
		c->destcolor[1] = cshift_lava.destcolor[1];
		c->destcolor[2] = cshift_lava.destcolor[2];
		c->percent = cshift_lava.percent;
		break;
	case CONTENTS_SLIME:
		//cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		c->destcolor[0] = cshift_slime.destcolor[0];
		c->destcolor[1] = cshift_slime.destcolor[1];
		c->destcolor[2] = cshift_slime.destcolor[2];
		c->percent = cshift_slime.percent;
		break;
	default:
		//cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
		c->destcolor[0] = cshift_water.destcolor[0];
		c->destcolor[1] = cshift_water.destcolor[1];
		c->destcolor[2] = cshift_water.destcolor[2];
		c->percent = cshift_water.percent;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	if (!gl_polyblend.value)
	{
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
		return;
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
}

/*
=============
V_CalcBlend
=============
*/
// LordHavoc: fixed V_CalcBlend
void V_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

//	if (gl_cshiftpercent.value)
//	{
		for (j=0 ; j<NUM_CSHIFTS ; j++)	
		{
//			a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
			a2 = cl.cshifts[j].percent * (1.0f / 255.0f);

			if (!a2)
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
//	}

	v_blend[0] = bound(0, r * (1.0/255.0), 1);
	v_blend[1] = bound(0, g * (1.0/255.0), 1);
	v_blend[2] = bound(0, b * (1.0/255.0), 1);
	v_blend[3] = bound(0, a              , 1);
}

/*
=============
V_UpdateBlends
=============
*/
void V_UpdateBlends (void)
{
	int		i, j;
	qboolean	new;

	V_CalcPowerupCshift ();
	
	new = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}
	
// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime)*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime)*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	if (!new)
		return;

	V_CalcBlend ();
}

/* 
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = ANGLEMOD(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{
	cl.viewent.render.angles[YAW] = r_refdef.viewangles[YAW];
	cl.viewent.render.angles[PITCH] = -r_refdef.viewangles[PITCH];

	cl.viewent.render.angles[ROLL] -= v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	cl.viewent.render.angles[PITCH] -= v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.render.angles[YAW] -= v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;
	
	ent = &cl_entities[cl.viewentity];

// absolutely bound refresh relative to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef.vieworg[0] < ent->render.origin[0] - 14)
		r_refdef.vieworg[0] = ent->render.origin[0] - 14;
	else if (r_refdef.vieworg[0] > ent->render.origin[0] + 14)
		r_refdef.vieworg[0] = ent->render.origin[0] + 14;
	if (r_refdef.vieworg[1] < ent->render.origin[1] - 14)
		r_refdef.vieworg[1] = ent->render.origin[1] - 14;
	else if (r_refdef.vieworg[1] > ent->render.origin[1] + 14)
		r_refdef.vieworg[1] = ent->render.origin[1] + 14;
	if (r_refdef.vieworg[2] < ent->render.origin[2] - 22)
		r_refdef.vieworg[2] = ent->render.origin[2] - 22;
	else if (r_refdef.vieworg[2] > ent->render.origin[2] + 30)
		r_refdef.vieworg[2] = ent->render.origin[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float		side;
		
	side = V_CalcRoll (cl_entities[cl.viewentity].render.angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
		r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		v_dmg_time -= cl.frametime;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		r_refdef.viewangles[ROLL] = 80;	// dead view angle
		return;
	}

}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->render.origin, r_refdef.vieworg);
	VectorCopy (ent->render.angles, r_refdef.viewangles);
	view->render.model = NULL;

// always idle in intermission
	old = v_idlescale.value;
	v_idlescale.value = 1;
	V_AddIdle ();
	v_idlescale.value = old;
}

/*
==================
V_CalcRefdef

==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent, *view;
	int			i;
	vec3_t		forward;
	vec3_t		angles;
	float		bob;
//	static float oldz = 0;

	V_DriftPitch ();

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;


	if (chase_active.value)
	{
		VectorCopy (ent->render.origin, r_refdef.vieworg);
		VectorCopy (cl.viewangles, r_refdef.viewangles);
		Chase_Update ();
		V_AddIdle ();
	}
	else
	{
	// transform the view offset by the model's matrix to get the offset from model origin for the view
	//	if (!chase_active.value) // LordHavoc: get rid of angle problems in chase_active mode
	//	{
	//		ent->render.angles[YAW] = cl.viewangles[YAW];	// the model should face the view dir
	//		ent->render.angles[PITCH] = -cl.viewangles[PITCH];	// the model should face the view dir
	//	}

		bob = V_CalcBob ();

	// refresh position
		VectorCopy (ent->render.origin, r_refdef.vieworg);
		r_refdef.vieworg[2] += cl.viewheight + bob;

		// LordHavoc: the protocol has changed...  so this is an obsolete approach
	// never let it sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	//	r_refdef.vieworg[0] += 1.0/32;
	//	r_refdef.vieworg[1] += 1.0/32;
	//	r_refdef.vieworg[2] += 1.0/32;

		if (!intimerefresh)
			VectorCopy (cl.viewangles, r_refdef.viewangles);
		V_CalcViewRoll ();
		V_AddIdle ();

	// offsets
		angles[PITCH] = -ent->render.angles[PITCH];	// because entity pitches are actually backward
		angles[YAW] = ent->render.angles[YAW];
		angles[ROLL] = ent->render.angles[ROLL];

		AngleVectors (angles, forward, NULL, NULL);

		V_BoundOffsets ();

	// set up gun position
		VectorCopy (ent->render.origin, view->render.origin);
		view->render.origin[2] += cl.viewheight;
		VectorCopy (cl.viewangles, view->render.angles);

		CalcGunAngle ();

		for (i=0 ; i<3 ; i++)
		{
			view->render.origin[i] += forward[i]*bob*0.4;
	//		view->render.origin[i] += right[i]*bob*0.4;
	//		view->render.origin[i] += up[i]*bob*0.8;
		}
		view->render.origin[2] += bob;

		// FIXME: this setup code is somewhat evil (CL_LerpUpdate should be private)
		CL_LerpUpdate(view, cl.stats[STAT_WEAPONFRAME], cl.stats[STAT_WEAPON]);

		view->render.model = cl.model_precache[cl.stats[STAT_WEAPON]];
		view->render.frame = cl.stats[STAT_WEAPONFRAME];
		view->render.colormap = -1; // no special coloring
		view->render.alpha = ent->render.alpha; // LordHavoc: if the player is transparent, so is the gun
		view->render.effects = ent->render.effects;
		view->render.scale = 1;

	// set up the refresh position

		// LordHavoc: this never looked all that good to begin with...
		/*
	// smooth out stair step ups
	if (cl.onground && ent->render.origin[2] - oldz > 0)
	{
		float steptime;

		steptime = cl.time - cl.oldtime;
		if (steptime < 0)
	//FIXME		I_Error ("steptime < 0");
			steptime = 0;

		oldz += steptime * 80;
		if (oldz > ent->render.origin[2])
			oldz = ent->render.origin[2];
		if (ent->render.origin[2] - oldz > 12)
			oldz = ent->render.origin[2] - 12;
		r_refdef.vieworg[2] += oldz - ent->render.origin[2];
		view->render.origin[2] += oldz - ent->render.origin[2];
	}
	else
		oldz = ent->render.origin[2];
		*/

	// LordHavoc: origin view kick added
		if (!intimerefresh && v_punch.value)
		{
			VectorAdd(r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);
			VectorAdd(r_refdef.vieworg, cl.punchvector, r_refdef.vieworg);
		}
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView (void)
{
	if (scr_con_current >= vid.conheight)
		return;

	if (cl.intermission)
		V_CalcIntermissionRefdef ();	
	else
		V_CalcRefdef ();

	R_RenderView ();
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
//	Cvar_RegisterVariable (&gl_cshiftpercent);
	Cvar_RegisterVariable (&gl_polyblend);

	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);	

	Cvar_RegisterVariable (&v_punch);
}


