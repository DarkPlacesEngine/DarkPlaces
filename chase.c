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
// chase.c -- chase camera code

#include "quakedef.h"

cvar_t	chase_back = {CVAR_SAVE, "chase_back", "48"};
cvar_t	chase_up = {CVAR_SAVE, "chase_up", "48"};
cvar_t	chase_active = {CVAR_SAVE, "chase_active", "0"};

void Chase_Init (void)
{
	Cvar_RegisterVariable (&chase_back);
	Cvar_RegisterVariable (&chase_up);
	Cvar_RegisterVariable (&chase_active);
}

void Chase_Reset (void)
{
	// for respawning and teleporting
//	start position 12 units behind head
}

float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t	trace;

	memset (&trace, 0, sizeof(trace));
	VectorCopy (end, trace.endpos);
	trace.fraction = 1;
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);
	VectorCopy (trace.endpos, impact);
	VectorCopy (trace.plane.normal, normal);
	return trace.fraction;
}

void Chase_Update (void)
{
	vec3_t	forward, stop, chase_dest, normal;
	float	dist;

	chase_back.value = bound(0, chase_back.value, 128);
	chase_up.value = bound(-48, chase_up.value, 96);

	AngleVectors (cl.viewangles, forward, NULL, NULL);

	dist = -chase_back.value - 8;
	chase_dest[0] = r_refdef.vieworg[0] + forward[0] * dist;
	chase_dest[1] = r_refdef.vieworg[1] + forward[1] * dist;
	chase_dest[2] = r_refdef.vieworg[2] + forward[2] * dist + chase_up.value;

	TraceLine (r_refdef.vieworg, chase_dest, stop, normal);
	chase_dest[0] = stop[0] + forward[0] * 8;
	chase_dest[1] = stop[1] + forward[1] * 8;
	chase_dest[2] = stop[2] + forward[2] * 8;

	VectorCopy (chase_dest, r_refdef.vieworg);
}

