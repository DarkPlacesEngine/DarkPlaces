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
	double frame1time;
	double frame2time;
}
effect_t;

static effect_t effect[MAX_EFFECTS];

static cvar_t r_draweffects = {0, "r_draweffects", "1"};

static void r_effects_start(void)
{
	memset(effect, 0, sizeof(effect));
}

static void r_effects_shutdown(void)
{
}

static void r_effects_newmap(void)
{
	memset(effect, 0, sizeof(effect));
}

void CL_Effects_Init(void)
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
		e->frame1time = cl.time;
		e->frame2time = cl.time;
		break;
	}
}

extern void CL_LerpAnimation(entity_t *e);

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
				e->frame1time = e->frame2time;
				e->frame2time = cl.time;
			}

			if ((vis = CL_NewTempEntity()))
			{
				// interpolation stuff
				vis->render.frame1 = intframe;
				vis->render.frame2 = intframe + 1;
				if (vis->render.frame2 >= e->endframe)
					vis->render.frame2 = -1; // disappear
				vis->render.framelerp = frame - intframe;
				vis->render.frame1time = e->frame1time;
				vis->render.frame2time = e->frame2time;

				// normal stuff
				VectorCopy(e->origin, vis->render.origin);
				vis->render.model = cl.model_precache[e->modelindex];
				vis->render.frame = vis->render.frame2;
				vis->render.colormap = -1; // no special coloring
				vis->render.scale = 1;
				vis->render.alpha = 1;
				vis->render.colormod[0] = vis->render.colormod[1] = vis->render.colormod[2] = 1;
			}
		}
	}
}
