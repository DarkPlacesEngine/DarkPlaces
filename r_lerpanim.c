#include "quakedef.h"

// LordHavoc: quite tempting to break apart this function to reuse the
//            duplicated code, but I suspect it is better for performance
//            this way
// LordHavoc: later note: made FRAMEBLENDINSERT macro
void R_LerpAnimation(entity_render_t *r)
{
	int sub1, sub2, numframes, f, i, data;
	double sublerp, lerp, d;
	animscene_t *scene, *scenes;
	frameblend_t *blend;
	blend = r->frameblend;

	numframes = r->model->numframes;

	if ((r->frame1 >= numframes))
	{
		Con_Printf ("CL_LerpAnimation: no such frame %d\n", r->frame1);
		r->frame1 = 0;
	}

	if ((r->frame2 >= numframes))
	{
		Con_Printf ("CL_LerpAnimation: no such frame %d\n", r->frame2);
		r->frame2 = 0;
	}

	// note: this could be removed, if the rendering code allows an empty blend array
	if (r->frame1 < 0)
		Host_Error ("CL_LerpAnimation: frame1 is NULL\n");

	// round off very close blend percentages
	if (r->framelerp < (1.0f / 65536.0f))
		r->framelerp = 0;
	if (r->framelerp >= (65535.0f / 65536.0f))
		r->framelerp = 1;

	blend[0].frame = blend[1].frame = blend[2].frame = blend[3].frame = -1;
	blend[0].lerp = blend[1].lerp = blend[2].lerp = blend[3].lerp = 0;
	if (r->model->ofs_scenes)
	{
		if (r->model->cachesize)
		{
			data = (int) Mod_Extradata(r->model);
			if (!data)
				Host_Error("CL_LerpAnimation: model not loaded\n");
			scenes = (animscene_t *) (r->model->ofs_scenes + data);
		}
		else
			scenes = (animscene_t *) r->model->ofs_scenes;

		if (r->framelerp < 1 && r->frame1 >= 0)
		{
			scene = scenes + r->frame1;
			lerp = 1 - r->framelerp;

			if (scene->framecount > 1)
			{
				sublerp = scene->framerate * (cl.time - r->frame1time);
				sub1 = (int) (sublerp);
				sub2 = sub1 + 1;
				sublerp -= sub1;
				if (sublerp < (1.0f / 65536.0f))
					sublerp = 0;
				if (sublerp >= (65535.0f / 65536.0f))
					sublerp = 1;
				if (scene->loop)
				{
					sub1 = (sub1 % scene->framecount) + scene->firstframe;
					sub2 = (sub2 % scene->framecount) + scene->firstframe;
				}
				else
				{
					sub1 = bound(0, sub1, (scene->framecount - 1)) + scene->firstframe;
					sub2 = bound(0, sub2, (scene->framecount - 1)) + scene->firstframe;
				}
				f = sub1;
				d = (1 - sublerp) * lerp;
#define FRAMEBLENDINSERT\
				if (d > 0)\
				{\
					for (i = 0;i < 4;i++)\
					{\
						if (blend[i].frame == f)\
						{\
							blend[i].lerp += d;\
							break;\
						}\
						if (blend[i].lerp <= 0)\
						{\
							blend[i].frame = f;\
							blend[i].lerp = d;\
							break;\
						}\
					}\
				}
				FRAMEBLENDINSERT
				f = sub2;
				d = sublerp * lerp;
			}
			else
			{
				f = scene->firstframe;
				d = lerp;
			}
			FRAMEBLENDINSERT
		}
		if (r->framelerp > 0 && r->frame2 >= 0)
		{
			scene = scenes + r->frame2;
			lerp = r->framelerp;

			if (scene->framecount > 1)
			{
				sublerp = scene->framerate * (cl.time - r->frame1time);
				sub1 = (int) (sublerp);
				sub2 = sub1 + 1;
				sublerp -= sub1;
				if (sublerp < (1.0f / 65536.0f))
					sublerp = 0;
				if (sublerp >= (65535.0f / 65536.0f))
					sublerp = 1;
				if (scene->loop)
				{
					sub1 = (sub1 % scene->framecount) + scene->firstframe;
					sub2 = (sub2 % scene->framecount) + scene->firstframe;
				}
				else
				{
					sub1 = bound(0, sub1, (scene->framecount - 1)) + scene->firstframe;
					sub2 = bound(0, sub2, (scene->framecount - 1)) + scene->firstframe;
				}
				f = sub1;
				d = (1 - sublerp) * lerp;
				FRAMEBLENDINSERT
				f = sub2;
				d = sublerp * lerp;
			}
			else
			{
				f = scene->firstframe;
				d = lerp;
			}
			FRAMEBLENDINSERT
		}
	}
	else
	{
		// if there are no scenes, assume it is all single-frame groups
		if (r->framelerp < 1 && r->frame1 >= 0)
		{
			f = r->frame1;
			d = 1 - r->framelerp;
			FRAMEBLENDINSERT
		}
		if (r->framelerp > 0 && r->frame2 >= 0)
		{
			f = r->frame2;
			d = r->framelerp;
			FRAMEBLENDINSERT
		}
	}
}

