#include "quakedef.h"

void R_ClipSpriteImage (msprite_t *psprite, vec3_t origin, vec3_t right, vec3_t up)
{
	int i;
	mspriteframe_t *frame;
	vec3_t points[4];
	float fleft, fright, fdown, fup;
	frame = ((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + currentrenderentity->frameblend[0].frame;
	fleft  = frame->left;
	fdown  = frame->down;
	fright = frame->right;
	fup    = frame->up;
	for (i = 1;i < 4 && currentrenderentity->frameblend[i].lerp;i++)
	{
		frame = ((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + currentrenderentity->frameblend[i].frame;
		fleft  = min(fleft , frame->left );
		fdown  = min(fdown , frame->down );
		fright = max(fright, frame->right);
		fup    = max(fup   , frame->up   );
	}
	points[0][0] = origin[0] + fdown * up[0] + fleft  * right[0];points[0][1] = origin[1] + fdown * up[1] + fleft  * right[1];points[0][2] = origin[2] + fdown * up[2] + fleft  * right[2];
	points[1][0] = origin[0] + fup   * up[0] + fleft  * right[0];points[1][1] = origin[1] + fup   * up[1] + fleft  * right[1];points[1][2] = origin[2] + fup   * up[2] + fleft  * right[2];
	points[2][0] = origin[0] + fup   * up[0] + fright * right[0];points[2][1] = origin[1] + fup   * up[1] + fright * right[1];points[2][2] = origin[2] + fup   * up[2] + fright * right[2];
	points[3][0] = origin[0] + fdown * up[0] + fright * right[0];points[3][1] = origin[1] + fdown * up[1] + fright * right[1];points[3][2] = origin[2] + fdown * up[2] + fright * right[2];
	R_Clip_AddPolygon(&points[0][0], 4, sizeof(float[3]), false, R_Entity_Callback, currentrenderentity, NULL, NULL);
}

int R_SpriteSetup (int type, float org[3], float right[3], float up[3])
{
	float matrix1[3][3], matrix2[3][3], matrix3[3][3];

	VectorCopy(currentrenderentity->origin, org);
	switch(type)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
		// flames and such
		// vertical beam sprite, faces view plane
		VectorNegate(vpn, matrix3[0]);
		matrix3[0][2] = 0;
		VectorNormalizeFast(matrix3[0]);
		VectorVectors(matrix3[0], matrix3[1], matrix3[2]);
		break;
	case SPR_FACING_UPRIGHT:
		// flames and such
		// vertical beam sprite, faces viewer's origin (not the view plane)
		VectorSubtract(r_origin, currentrenderentity->origin, matrix3[0]);
		matrix3[0][2] = 0;
		VectorNormalizeFast(matrix3[0]);
		VectorVectors(matrix3[0], matrix3[1], matrix3[2]);
		break;
	default:
		Con_Printf("R_SpriteSetup: unknown sprite type %i\n", type);
		// fall through to normal sprite
	case SPR_VP_PARALLEL:
		// normal sprite
		// faces view plane
		VectorCopy(vpn, matrix3[0]);
		VectorCopy(vright, matrix3[1]);
		VectorCopy(vup, matrix3[2]);
		break;
	case SPR_ORIENTED:
		// bullet marks on walls
		// ignores viewer entirely
		AngleVectors (currentrenderentity->angles, matrix3[0], matrix3[1], matrix3[2]);
		// nudge it toward the view, so it will be infront of the wall
		VectorSubtract(org, vpn, org);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		// I have no idea what people would use this for
		// oriented relative to view space
		// FIXME: test this and make sure it mimicks software
		AngleVectors (currentrenderentity->angles, matrix1[0], matrix1[1], matrix1[2]);
		VectorCopy(vpn, matrix2[0]);
		VectorCopy(vright, matrix2[1]);
		VectorCopy(vup, matrix2[2]);
		R_ConcatRotations (matrix1, matrix2, matrix3);
		break;
	}

	// don't draw if view origin is behind it
	if (DotProduct(org, matrix3[0]) < (DotProduct(r_origin, matrix3[0]) - 1.0f))
		return true;

	if (currentrenderentity->scale != 1)
	{
		VectorScale(matrix3[1], currentrenderentity->scale, matrix3[1]);
		VectorScale(matrix3[2], currentrenderentity->scale, matrix3[2]);
	}

	VectorCopy(matrix3[1], right);
	VectorCopy(matrix3[2], up);
	return false;
}

void R_ClipSprite (void)
{
	vec3_t org, right, up;
	msprite_t *psprite;

	if (currentrenderentity->frameblend[0].frame < 0)
		return;

	psprite = Mod_Extradata(currentrenderentity->model);
	if (R_SpriteSetup(psprite->type, org, right, up))
		return;

	// LordHavoc: interpolated sprite rendering
	R_ClipSpriteImage(psprite, org, right, up);
}

void GL_DrawSpriteImage (mspriteframe_t *frame, vec3_t origin, vec3_t up, vec3_t right, byte red, byte green, byte blue, int alpha)
{
	byte alphaub;
	alphaub = bound(0, alpha, 255);
	transpolybegin(R_GetTexture(frame->texture), 0, R_GetTexture(frame->fogtexture), ((currentrenderentity->effects & EF_ADDITIVE) || (currentrenderentity->model->flags & EF_ADDITIVE)) ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
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
void R_DrawSpriteModel ()
{
	int			i;
	vec3_t		right, up, org, color;
	byte		colorub[4];
	msprite_t	*psprite;

	if (currentrenderentity->frameblend[0].frame < 0)
		return;

	psprite = Mod_Extradata(currentrenderentity->model);
	if (R_SpriteSetup(psprite->type, org, right, up))
		return;

	c_sprites++;

	if ((currentrenderentity->model->flags & EF_FULLBRIGHT) || (currentrenderentity->effects & EF_FULLBRIGHT))
	{
		color[0] = currentrenderentity->colormod[0] * 255;
		color[1] = currentrenderentity->colormod[1] * 255;
		color[2] = currentrenderentity->colormod[2] * 255;
	}
	else
		R_CompleteLightPoint(color, currentrenderentity->origin, true, NULL);

	colorub[0] = bound(0, color[0], 255);
	colorub[1] = bound(0, color[1], 255);
	colorub[2] = bound(0, color[2], 255);

	// LordHavoc: interpolated sprite rendering
	for (i = 0;i < 4;i++)
		if (currentrenderentity->frameblend[i].lerp)
			GL_DrawSpriteImage(((mspriteframe_t *)(psprite->ofs_frames + (int) psprite)) + currentrenderentity->frameblend[i].frame, org, up, right, colorub[0],colorub[1],colorub[2], currentrenderentity->alpha*255*currentrenderentity->frameblend[i].lerp);
}

