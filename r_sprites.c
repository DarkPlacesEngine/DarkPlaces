#include "quakedef.h"

/*
================
R_GetSpriteFrame
================
*/
void R_GetSpriteFrame (entity_t *currententity, mspriteframe_t **oldframe, mspriteframe_t **newframe, float *framelerp)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	int				i, j, numframes, frame;
	float			*pintervals, fullinterval, targettime, time, jtime, jinterval;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		if (currententity->draw_lastmodel == currententity->model && currententity->draw_lerpstart < cl.time)
		{
			if (frame != currententity->draw_pose)
			{
				currententity->draw_lastpose = currententity->draw_pose;
				currententity->draw_pose = frame;
				currententity->draw_lerpstart = cl.time;
				*framelerp = 0;
			}
			else
				*framelerp = (cl.time - currententity->draw_lerpstart) * 10.0;
		}
		else // uninitialized
		{
			currententity->draw_lastmodel = currententity->model;
			currententity->draw_lastpose = currententity->draw_pose = frame;
			currententity->draw_lerpstart = cl.time;
			*framelerp = 0;
		}
		*oldframe = psprite->frames[currententity->draw_lastpose].frameptr;
		*newframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		// LordHavoc: since I can't measure the time properly when it loops from numframes-1 to 0,
		//            I instead measure the time of the first frame, hoping it is consistent
		j = numframes-1;jtime = 0;jinterval = pintervals[1] - pintervals[0];
		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
			j = i;jinterval = pintervals[i] - jtime;jtime = pintervals[i];
		}
		*framelerp = (targettime - jtime) / jinterval;

		*oldframe = pspritegroup->frames[j];
		*newframe = pspritegroup->frames[i];
	}
}

void GL_DrawSpriteImage (mspriteframe_t *frame, vec3_t origin, vec3_t up, vec3_t right, int red, int green, int blue, int alpha)
{
	// LordHavoc: rewrote this to use the transparent poly system
	transpolybegin(frame->gl_texturenum, 0, frame->gl_fogtexturenum, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
	transpolyvert(origin[0] + frame->down * up[0] + frame->left * right[0], origin[1] + frame->down * up[1] + frame->left * right[1], origin[2] + frame->down * up[2] + frame->left * right[2], 0, 1, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->up * up[0] + frame->left * right[0], origin[1] + frame->up * up[1] + frame->left * right[1], origin[2] + frame->up * up[2] + frame->left * right[2], 0, 0, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->up * up[0] + frame->right * right[0], origin[1] + frame->up * up[1] + frame->right * right[1], origin[2] + frame->up * up[2] + frame->right * right[2], 1, 0, red, green, blue, alpha);
	transpolyvert(origin[0] + frame->down * up[0] + frame->right * right[0], origin[1] + frame->down * up[1] + frame->right * right[1], origin[2] + frame->down * up[2] + frame->right * right[2], 1, 1, red, green, blue, alpha);
	transpolyend();
}

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	mspriteframe_t	*oldframe, *newframe;
	float			lerp, ilerp;
	vec3_t			forward, right, up, org, color;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	R_GetSpriteFrame (e, &oldframe, &newframe, &lerp);
	if (lerp < 0) lerp = 0;
	if (lerp > 1) lerp = 1;
	if (isRagePro) // LordHavoc: no alpha scaling supported on per pixel alpha images on ATI Rage Pro... ACK!
		lerp = 1;
	ilerp = 1.0 - lerp;
	psprite = e->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (e->angles, forward, right, up);
		VectorSubtract(e->origin, vpn, org);
	}
	else
	{	// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		VectorCopy(e->origin, org);
	}
	if (e->scale != 1)
	{
		VectorScale(up, e->scale, up);
		VectorScale(right, e->scale, right);
	}

	if (e->model->flags & EF_FULLBRIGHT || e->effects & EF_FULLBRIGHT)
	{
		color[0] = e->colormod[0] * 255;
		color[1] = e->colormod[1] * 255;
		color[2] = e->colormod[2] * 255;
	}
	else
	{
		R_LightPoint (color, e->origin);
		R_DynamicLightPointNoMask(color, e->origin);
	}

	// LordHavoc: interpolated sprite rendering
	if (ilerp != 0)
		GL_DrawSpriteImage(oldframe, org, up, right, color[0],color[1],color[2],e->alpha*255*ilerp);
	if (lerp != 0)
		GL_DrawSpriteImage(newframe, org, up, right, color[0],color[1],color[2],e->alpha*255*lerp);
}

