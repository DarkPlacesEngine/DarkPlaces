#include "quakedef.h"

void R_ClipSpriteImage (entity_t *e, msprite_t *psprite, frameblend_t *blend, vec3_t origin, vec3_t up, vec3_t right)
{
	int i;
	mspriteframe_t *frame;
	vec3_t points[4];
	float fleft, fright, fdown, fup;
	frame = ((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[0].frame;
	fleft  = frame->left;
	fdown  = frame->down;
	fright = frame->right;
	fup    = frame->up;
	for (i = 1;i < 4 && blend[i].lerp;i++)
	{
		frame = ((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[i].frame;
		fleft  = min(fleft , frame->left );
		fdown  = min(fdown , frame->down );
		fright = max(fright, frame->right);
		fup    = max(fup   , frame->up   );
	}
	points[0][0] = origin[0] + fdown * up[0] + fleft  * right[0];points[0][1] = origin[1] + fdown * up[1] + fleft  * right[1];points[0][2] = origin[2] + fdown * up[2] + fleft  * right[2];
	points[1][0] = origin[0] + fup   * up[0] + fleft  * right[0];points[1][1] = origin[1] + fup   * up[1] + fleft  * right[1];points[1][2] = origin[2] + fup   * up[2] + fleft  * right[2];
	points[2][0] = origin[0] + fup   * up[0] + fright * right[0];points[2][1] = origin[1] + fup   * up[1] + fright * right[1];points[2][2] = origin[2] + fup   * up[2] + fright * right[2];
	points[3][0] = origin[0] + fdown * up[0] + fright * right[0];points[3][1] = origin[1] + fdown * up[1] + fright * right[1];points[3][2] = origin[2] + fdown * up[2] + fright * right[2];
	R_Clip_AddPolygon(&points[0][0], 4, sizeof(float[3]), false, R_Entity_Callback, e, NULL, NULL);
}

void R_ClipSprite (entity_t *e, frameblend_t *blend)
{
	vec3_t forward, right, up, org;
	msprite_t *psprite;

	if (!blend[0].lerp)
		return;

	psprite = Mod_Extradata(e->render.model);

	if (psprite->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors (e->render.angles, forward, right, up);
		// nudge it toward the view, so it will be infront of the wall
		VectorSubtract(e->render.origin, vpn, org);
		// don't draw if it's a backface
		if (DotProduct(r_origin, forward) > (DotProduct(org, forward) - 1.0f))
			return;
	}
	else
	{
		// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		VectorCopy(e->render.origin, org);
		// don't draw if it's a backface
		if (DotProduct(org, vpn) < (DotProduct(r_origin, vpn) + 1.0f))
			return;
	}


	if (e->render.scale != 1)
	{
		VectorScale(up, e->render.scale, up);
		VectorScale(right, e->render.scale, right);
	}

	// LordHavoc: interpolated sprite rendering
	R_ClipSpriteImage(e, psprite, blend, org, up, right);
}

void GL_DrawSpriteImage (mspriteframe_t *frame, vec3_t origin, vec3_t up, vec3_t right, byte red, byte green, byte blue, int alpha)
{
	byte alphaub;
	alphaub = bound(0, alpha, 255);
	transpolybegin(R_GetTexture(frame->texture), 0, R_GetTexture(frame->fogtexture), currententity->render.effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
	transpolyvertub(origin[0] + frame->down * up[0] + frame->left  * right[0], origin[1] + frame->down * up[1] + frame->left  * right[1], origin[2] + frame->down * up[2] + frame->left  * right[2], 0, 1, red, green, blue, alphaub);
	transpolyvertub(origin[0] + frame->up   * up[0] + frame->left  * right[0], origin[1] + frame->up   * up[1] + frame->left  * right[1], origin[2] + frame->up   * up[2] + frame->left  * right[2], 0, 0, red, green, blue, alphaub);
	transpolyvertub(origin[0] + frame->up   * up[0] + frame->right * right[0], origin[1] + frame->up   * up[1] + frame->right * right[1], origin[2] + frame->up   * up[2] + frame->right * right[2], 1, 0, red, green, blue, alphaub);
	transpolyvertub(origin[0] + frame->down * up[0] + frame->right * right[0], origin[1] + frame->down * up[1] + frame->right * right[1], origin[2] + frame->down * up[2] + frame->right * right[2], 1, 1, red, green, blue, alphaub);
	transpolyend();
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel (entity_t *e, frameblend_t *blend)
{
	vec3_t		forward, right, up, org, color, mins, maxs;
	byte		colorub[4];
	msprite_t	*psprite;

	if (!blend[0].lerp)
		return;

	VectorAdd (e->render.origin, e->render.model->mins, mins);
	VectorAdd (e->render.origin, e->render.model->maxs, maxs);

//	if (R_CullBox (mins, maxs))
//		return;

	c_sprites++;

	psprite = Mod_Extradata(e->render.model);
	//psprite = e->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors (e->render.angles, forward, right, up);
		VectorSubtract(e->render.origin, vpn, org);
	}
	else
	{
		// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		VectorCopy(e->render.origin, org);
	}

	if (e->render.scale != 1)
	{
		VectorScale(up, e->render.scale, up);
		VectorScale(right, e->render.scale, right);
	}

	if (e->render.model->flags & EF_FULLBRIGHT || e->render.effects & EF_FULLBRIGHT)
	{
		color[0] = e->render.colormod[0] * 255;
		color[1] = e->render.colormod[1] * 255;
		color[2] = e->render.colormod[2] * 255;
	}
	else
		R_CompleteLightPoint(color, e->render.origin, true);

	colorub[0] = bound(0, color[0], 255);
	colorub[1] = bound(0, color[1], 255);
	colorub[2] = bound(0, color[2], 255);

	// LordHavoc: interpolated sprite rendering
	if (blend[0].lerp)
		GL_DrawSpriteImage(((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[0].frame, org, up, right, colorub[0],colorub[1],colorub[2], e->render.alpha*255*blend[0].lerp);
	if (blend[1].lerp)
		GL_DrawSpriteImage(((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[1].frame, org, up, right, colorub[0],colorub[1],colorub[2], e->render.alpha*255*blend[1].lerp);
	if (blend[2].lerp)
		GL_DrawSpriteImage(((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[2].frame, org, up, right, colorub[0],colorub[1],colorub[2], e->render.alpha*255*blend[2].lerp);
	if (blend[3].lerp)
		GL_DrawSpriteImage(((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + blend[3].frame, org, up, right, colorub[0],colorub[1],colorub[2], e->render.alpha*255*blend[3].lerp);
}

