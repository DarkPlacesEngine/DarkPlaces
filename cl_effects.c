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

#include "quakedef.h"

#define MAX_EFFECTS 256

typedef struct effect_s
{
	int active;
	vec3_t origin;
	float starttime;
	float framerate;
	int modelindex;
	int startframe;
	int endframe;
	// these are for interpolation
	int frame;
	double frame1start;
	double frame2start;
}
effect_t;

effect_t effect[MAX_EFFECTS];

cvar_t r_draweffects = {"r_draweffects", "1"};

void r_effects_start()
{
	memset(effect, 0, sizeof(effect));
}

void r_effects_shutdown()
{
}

void r_effects_newmap()
{
	memset(effect, 0, sizeof(effect));
}

void CL_Effects_Init()
{
	Cvar_RegisterVariable(&r_draweffects);

	R_RegisterModule("R_Effects", r_effects_start, r_effects_shutdown, r_effects_newmap);
}

void CL_Effect(vec3_t org, int modelindex, int startframe, int framecount, float framerate)
{
	int i;
	effect_t *e;
	if (!modelindex) // sanity check
		return;
	for (i = 0, e = effect;i < MAX_EFFECTS;i++, e++)
	{
		if (e->active)
			continue;
		e->active = true;
		VectorCopy(org, e->origin);
		e->modelindex = modelindex;
		e->starttime = cl.time;
		e->startframe = startframe;
		e->endframe = startframe + framecount;
		e->framerate = framerate;

		e->frame = 0;
		e->frame1start = cl.time;
		e->frame2start = cl.time;
		break;
	}
}

void CL_DoEffects()
{
	int i, intframe;
	effect_t *e;
	entity_t *vis;
	float frame;

	for (i = 0, e = effect;i < MAX_EFFECTS;i++, e++)
	{
		if (e->active)
		{
			frame = (cl.time - e->starttime) * e->framerate + e->startframe;
			intframe = frame;
			if (intframe < 0 || intframe >= e->endframe)
			{
				e->active = false;
				memset(e, 0, sizeof(*e));
				continue;
			}

			if (intframe != e->frame)
			{
				e->frame = intframe;
				e->frame1start = e->frame2start;
				e->frame2start = cl.time;
			}

			vis = CL_NewTempEntity();
			if (!vis)
				continue;
			VectorCopy(e->origin, vis->origin);
			vis->lerp_model = vis->model = cl.model_precache[e->modelindex];
			vis->frame1 = e->frame;
			vis->frame2 = e->frame + 1;
			if (vis->frame2 >= e->endframe)
				vis->frame2 = -1; // disappear
			vis->frame = vis->frame2;
			vis->framelerp = frame - vis->frame1;
			vis->frame1start = e->frame1start;
			vis->frame2start = e->frame2start;
			vis->lerp_starttime = -1;
			vis->colormap = -1; // no special coloring
			vis->scale = 1;
			vis->alpha = 1;
			vis->colormod[0] = vis->colormod[1] = vis->colormod[2] = 1;
		}
	}
}
