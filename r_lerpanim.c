
#include "quakedef.h"

// LordHavoc: quite tempting to break apart this function to reuse the
//            duplicated code, but I suspect it is better for performance
//            this way
void R_LerpAnimation(model_t *mod, int frame1, int frame2, double frame1start, double frame2start, double framelerp, frameblend_t *blend)
{
	int sub1, sub2, numframes, f, i, data;
	double sublerp, lerp, l;
	animscene_t *scene, *scenes;

	data = (int) Mod_Extradata(mod);
	if (!data)
		Host_Error("R_LerpAnimation: model not loaded\n");
	scenes = (animscene_t *) (mod->ofs_scenes + data);

	numframes = mod->numframes;

	if ((frame1 >= numframes))
	{
		Con_Printf ("R_LerpAnimation: no such frame %d\n", frame1);
		frame1 = 0;
	}

	if ((frame2 >= numframes))
	{
		Con_Printf ("R_LerpAnimation: no such frame %d\n", frame2);
		frame2 = 0;
	}

	if (frame1 < 0)
		Host_Error ("R_LerpAnimation: frame1 is NULL\n");

	// round off very close blend percentages
	if (framelerp < (1.0f / 65536.0f))
		framelerp = 0;
	if (framelerp >= (65535.0f / 65536.0f))
		framelerp = 1;

	blend[0].frame = blend[1].frame = blend[2].frame = blend[3].frame = -1;
	blend[0].lerp = blend[1].lerp = blend[2].lerp = blend[3].lerp = 0;
	if (framelerp < 1)
	{
		scene = scenes + frame1;
		lerp = 1 - framelerp;

		if (scene->framecount > 1)
		{
			sublerp = scene->framerate * (cl.time - frame1start);
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
			l = (1 - sublerp) * lerp;
			if (l > 0)
			{
				for (i = 0;i < 4;i++)
				{
					if (blend[i].frame == f)
					{
						blend[i].lerp += l;
						break;
					}
					if (blend[i].lerp <= 0)
					{
						blend[i].frame = f;
						blend[i].lerp = l;
						break;
					}
				}
			}
			f = sub2;
			l = sublerp * lerp;
		}
		else
		{
			f = scene->firstframe;
			l = lerp;
		}
		if (l > 0)
		{
			for (i = 0;i < 4;i++)
			{
				if (blend[i].frame == f)
				{
					blend[i].lerp += l;
					break;
				}
				if (blend[i].lerp <= 0)
				{
					blend[i].frame = f;
					blend[i].lerp = l;
					break;
				}
			}
		}
	}
	if (framelerp > 0 && frame2 >= 0)
	{
		scene = scenes + frame2;
		lerp = framelerp;

		if (scene->framecount > 1)
		{
			sublerp = scene->framerate * (cl.time - frame1start);
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
			l = (1 - sublerp) * lerp;
			if (l > 0)
			{
				for (i = 0;i < 4;i++)
				{
					if (blend[i].frame == f)
					{
						blend[i].lerp += l;
						break;
					}
					if (blend[i].lerp <= 0)
					{
						blend[i].frame = f;
						blend[i].lerp = l;
						break;
					}
				}
			}
			f = sub2;
			l = sublerp * lerp;
		}
		else
		{
			f = scene->firstframe;
			l = lerp;
		}
		if (l > 0)
		{
			for (i = 0;i < 4;i++)
			{
				if (blend[i].frame == f)
				{
					blend[i].lerp += l;
					break;
				}
				if (blend[i].lerp <= 0)
				{
					blend[i].frame = f;
					blend[i].lerp = l;
					break;
				}
			}
		}
	}
}
