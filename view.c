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

cvar_t cl_stairsmoothspeed = {CVAR_SAVE, "cl_stairsmoothspeed", "160"};

cvar_t chase_back = {CVAR_SAVE, "chase_back", "48"};
cvar_t chase_up = {CVAR_SAVE, "chase_up", "24"};
cvar_t chase_active = {CVAR_SAVE, "chase_active", "0"};
// GAME_GOODVSBAD2
cvar_t chase_stevie = {0, "chase_stevie", "0"};

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
	int armor, blood;
	vec3_t from;
	//vec3_t forward, right;
	vec3_t localfrom;
	entity_t *ent;
	//float side;
	float count;

	armor = MSG_ReadByte ();
	blood = MSG_ReadByte ();
	MSG_ReadVector(from);

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

	// calculate view angle kicks
	if (cl.viewentity >= 0 && cl.viewentity < MAX_EDICTS && cl_entities[cl.viewentity].state_current.active)
	{
		ent = &cl_entities[cl.viewentity];
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
==============================================================================

						VIEW RENDERING

==============================================================================
*/

extern matrix4x4_t viewmodelmatrix;

#include "cl_collision.h"

/*
==================
V_CalcRefdef

==================
*/
extern float timerefreshangle;
void V_CalcRefdef (void)
{
	static float oldz;
	entity_t *ent;
	float vieworg[3], viewangles[3];
	Matrix4x4_CreateIdentity(&viewmodelmatrix);
	Matrix4x4_CreateIdentity(&r_refdef.viewentitymatrix);
	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		// ent is the view entity (visible when out of body)
		ent = &cl_entities[cl.viewentity];
		if (cl.intermission)
		{
			// entity is a fixed camera, just copy the matrix
			Matrix4x4_Copy(&r_refdef.viewentitymatrix, &ent->render.matrix);
			Matrix4x4_Copy(&viewmodelmatrix, &ent->render.matrix);
		}
		else
		{
			// player can look around, so take the origin from the entity,
			// and the angles from the input system
			Matrix4x4_OriginFromMatrix(&ent->render.matrix, vieworg);
			VectorCopy(cl.viewangles, viewangles);

			// stair smoothing
			if (cl.onground && oldz < vieworg[2])
			{
				oldz += (cl.time - cl.oldtime) * cl_stairsmoothspeed.value;
				oldz = vieworg[2] = bound(vieworg[2] - 16, oldz, vieworg[2]);
			}
			else
				oldz = vieworg[2];

			if (chase_active.value)
			{
				// observing entity from third person
				vec_t camback, camup, dist, forward[3], stop[3], chase_dest[3], normal[3];

				camback = bound(0, chase_back.value, 128);
				if (chase_back.value != camback)
					Cvar_SetValueQuick(&chase_back, camback);
				camup = bound(-48, chase_up.value, 96);
				if (chase_up.value != camup)
					Cvar_SetValueQuick(&chase_up, camup);

				// this + 22 is to match view_ofs for compatibility with older versions
				camup += 22;

				if (gamemode == GAME_GOODVSBAD2 && chase_stevie.integer)
				{
					// look straight down from high above
					viewangles[0] = 90;
					camback = 2048;
				}
				AngleVectors(viewangles, forward, NULL, NULL);

				// trace a little further so it hits a surface more consistently (to avoid 'snapping' on the edge of the range)
				dist = -camback - 8;
				chase_dest[0] = vieworg[0] + forward[0] * dist;
				chase_dest[1] = vieworg[1] + forward[1] * dist;
				chase_dest[2] = vieworg[2] + forward[2] * dist + camup;
				CL_TraceLine(vieworg, chase_dest, stop, normal, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY);
				vieworg[0] = stop[0] + forward[0] * 8 + normal[0] * 4;
				vieworg[1] = stop[1] + forward[1] * 8 + normal[1] * 4;
				vieworg[2] = stop[2] + forward[2] * 8 + normal[2] * 4;
			}
			else
			{
				// first person view from entity
				// angles
				if (cl.stats[STAT_HEALTH] <= 0 && gamemode != GAME_FNIGGIUM)
					viewangles[ROLL] = 80;	// dead view angle
				VectorAdd(viewangles, cl.punchangle, viewangles);
				viewangles[ROLL] += V_CalcRoll(cl.viewangles, cl.velocity);
				if (v_dmg_time > 0)
				{
					viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
					viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
					v_dmg_time -= cl.frametime;
				}
				// origin
				VectorAdd(vieworg, cl.punchvector, vieworg);
				vieworg[2] += cl.viewheight;
				if (cl.stats[STAT_HEALTH] > 0 && cl_bob.value && cl_bobcycle.value)
				{
					double bob, cycle;
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
					bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
					bob = bob*0.3 + bob*0.7*cycle;
					vieworg[2] += bound(-7, bob, 4);
				}
			}
			// calculate a view matrix for rendering the scene
			if (v_idlescale.value)
				Matrix4x4_CreateFromQuakeEntity(&r_refdef.viewentitymatrix, vieworg[0], vieworg[1], vieworg[2], viewangles[0] + v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value, viewangles[1] + v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value, viewangles[2] + v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value, 1);
			else
				Matrix4x4_CreateFromQuakeEntity(&r_refdef.viewentitymatrix, vieworg[0], vieworg[1], vieworg[2], viewangles[0], viewangles[1], viewangles[2] + v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value, 1);
			// calculate a viewmodel matrix for use in view-attached entities
			Matrix4x4_CreateFromQuakeEntity(&viewmodelmatrix, vieworg[0], vieworg[1], vieworg[2], viewangles[0], viewangles[1], viewangles[2], 0.3);
		}
	}
}

void V_FadeViewFlashs(void)
{
	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= (cl.time - cl.oldtime)*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= (cl.time - cl.oldtime)*100;
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
	if (cls.state == ca_connected && cls.signon == SIGNONS && gl_polyblend.value > 0)
	{
		// set contents color
		switch (CL_PointQ1Contents(r_vieworigin))
		{
		case CONTENTS_EMPTY:
		case CONTENTS_SOLID:
			cl.cshifts[CSHIFT_CONTENTS].destcolor[0] = 0;
			cl.cshifts[CSHIFT_CONTENTS].destcolor[1] = 0;
			cl.cshifts[CSHIFT_CONTENTS].destcolor[2] = 0;
			cl.cshifts[CSHIFT_CONTENTS].percent = 0;
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

		if (gamemode != GAME_TRANSFUSION)
		{
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

		r_refdef.viewblend[0] = bound(0.0f, r_refdef.viewblend[0] * (1.0f/255.0f), 1.0f);
		r_refdef.viewblend[1] = bound(0.0f, r_refdef.viewblend[1] * (1.0f/255.0f), 1.0f);
		r_refdef.viewblend[2] = bound(0.0f, r_refdef.viewblend[2] * (1.0f/255.0f), 1.0f);
		r_refdef.viewblend[3] = bound(0.0f, r_refdef.viewblend[3] * gl_polyblend.value, 1.0f);
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

	Cvar_RegisterVariable (&cl_stairsmoothspeed);

	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_active);
	if (gamemode == GAME_GOODVSBAD2)
		Cvar_RegisterVariable (&chase_stevie);
}

