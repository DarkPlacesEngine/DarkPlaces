#include "quakedef.h"

// LordHavoc: quite tempting to break apart this function to reuse the
//            duplicated code, but I suspect it is better for performance
//            this way
// LordHavoc: later note: made FRAMEBLENDINSERT macro
void R_LerpAnimation(entity_render_t *r)
{
	int sub2, numframes, f, i, k;
	int isfirstframegroup = true;
	int nolerp;
	double sublerp, lerp, d;
	animscene_t *scene;
	framegroupblend_t *g;
	frameblend_t *blend;
	dp_model_t *model = r->model;

	memset(r->frameblend, 0, sizeof(r->frameblend));

	if (!model || !model->surfmesh.isanimated)
	{
		r->frameblend[0].lerp = 1;
		return;
	}

	blend = r->frameblend;
	nolerp = (model->type == mod_sprite) ? !r_lerpsprites.integer : !r_lerpmodels.integer;
	numframes = model->numframes;
	for (k = 0, g = r->framegroupblend;k < MAX_FRAMEGROUPBLENDS;k++, g++)
	{
		if ((unsigned int)g->frame >= (unsigned int)numframes)
		{
			Con_DPrintf("CL_LerpAnimation: no such frame %d\n", g->frame);
			g->frame = 0;
		}
		f = g->frame;
		d = lerp = g->lerp;
		if (lerp <= 0)
			continue;
		if (nolerp)
		{
			if (isfirstframegroup)
			{
				d = lerp = 1;
				isfirstframegroup = false;
			}
			else
				continue;
		}
		if (model->animscenes)
		{
			scene = model->animscenes + f;
			f = scene->firstframe;
			if (scene->framecount > 1)
			{
				// this code path is only used on .zym models and torches
				sublerp = scene->framerate * (cl.time - g->start);
				f = (int) floor(sublerp);
				sublerp -= f;
				sub2 = f + 1;
				if (sublerp < (1.0 / 65536.0f))
					sublerp = 0;
				if (sublerp > (65535.0f / 65536.0f))
					sublerp = 1;
				if (nolerp)
					sublerp = 0;
				if (scene->loop)
				{
					f = (f % scene->framecount);
					sub2 = (sub2 % scene->framecount);
				}
				f = bound(0, f, (scene->framecount - 1)) + scene->firstframe;
				sub2 = bound(0, sub2, (scene->framecount - 1)) + scene->firstframe;
				d = sublerp * lerp;
				// two framelerps produced from one animation
				if (d > 0)
				{
					for (i = 0;i < MAX_FRAMEBLENDS;i++)
					{
						if (blend[i].lerp <= 0 || blend[i].subframe == sub2)
						{
							blend[i].subframe = sub2;
							blend[i].lerp += d;
							break;
						}
					}
				}
				d = (1 - sublerp) * lerp;
			}
		}
		if (d > 0)
		{
			for (i = 0;i < MAX_FRAMEBLENDS;i++)
			{
				if (blend[i].lerp <= 0 || blend[i].subframe == f)
				{
					blend[i].subframe = f;
					blend[i].lerp += d;
					break;
				}
			}
		}
	}
}

