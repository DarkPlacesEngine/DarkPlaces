
#include "quakedef.h"

#define LERPSPRITES

static int R_SpriteSetup (int type, float org[3], float left[3], float up[3])
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
		matrix3[1][0] = matrix3[0][1];
		matrix3[1][1] = -matrix3[0][0];
		matrix3[1][2] = 0;
		matrix3[2][0] = 0;
		matrix3[2][1] = 0;
		matrix3[2][2] = 1;
		break;
	case SPR_FACING_UPRIGHT:
		// flames and such
		// vertical beam sprite, faces viewer's origin (not the view plane)
		VectorSubtract(currentrenderentity->origin, r_origin, matrix3[0]);
		matrix3[0][2] = 0;
		VectorNormalizeFast(matrix3[0]);
		matrix3[1][0] = matrix3[0][1];
		matrix3[1][1] = -matrix3[0][0];
		matrix3[1][2] = 0;
		matrix3[2][0] = 0;
		matrix3[2][1] = 0;
		matrix3[2][2] = 1;
		break;
	default:
		Con_Printf("R_SpriteSetup: unknown sprite type %i\n", type);
		// fall through to normal sprite
	case SPR_VP_PARALLEL:
		// normal sprite
		// faces view plane
		VectorCopy(vpn, matrix3[0]);
		VectorNegate(vright, matrix3[1]);
		VectorCopy(vup, matrix3[2]);
		break;
	case SPR_ORIENTED:
		// bullet marks on walls
		// ignores viewer entirely
		AngleVectorsFLU (currentrenderentity->angles, matrix3[0], matrix3[1], matrix3[2]);
		// nudge it toward the view, so it will be infront of the wall
		VectorSubtract(org, vpn, org);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		// I have no idea what people would use this for
		// oriented relative to view space
		// FIXME: test this and make sure it mimicks software
		AngleVectorsFLU (currentrenderentity->angles, matrix1[0], matrix1[1], matrix1[2]);
		VectorCopy(vpn, matrix2[0]);
		VectorNegate(vright, matrix2[1]);
		VectorCopy(vup, matrix2[2]);
		R_ConcatRotations (matrix1[0], matrix2[0], matrix3[0]);
		break;
	}

	if (currentrenderentity->scale != 1)
	{
		VectorScale(matrix3[1], currentrenderentity->scale, left);
		VectorScale(matrix3[2], currentrenderentity->scale, up);
	}
	else
	{
		VectorCopy(matrix3[1], left);
		VectorCopy(matrix3[2], up);
	}
	return false;
}

static void GL_DrawSpriteImage (int fog, mspriteframe_t *frame, int texture, vec3_t origin, vec3_t up, vec3_t left, float red, float green, float blue, float alpha)
{
	rmeshbufferinfo_t m;
	memset(&m, 0, sizeof(m));
	m.transparent = true;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	if ((currentrenderentity->effects & EF_ADDITIVE)
	 || (currentrenderentity->model->flags & EF_ADDITIVE)
	 || fog)
		m.blendfunc2 = GL_ONE;
	m.numtriangles = 2;
	m.numverts = 4;
	m.tex[0] = texture;
	if (R_Mesh_Draw_GetBuffer(&m, !(currentrenderentity->model->flags & EF_FULLBRIGHT || currentrenderentity->effects & EF_FULLBRIGHT)))
	{
		m.index[0] = 0;
		m.index[1] = 1;
		m.index[2] = 2;
		m.index[3] = 0;
		m.index[4] = 2;
		m.index[5] = 3;
		m.color[0] = m.color[4] = m.color[8] = m.color[12] = red * m.colorscale;
		m.color[1] = m.color[5] = m.color[9] = m.color[13] = green * m.colorscale;
		m.color[2] = m.color[6] = m.color[10] = m.color[14] = blue * m.colorscale;
		m.color[3] = m.color[7] = m.color[11] = m.color[15] = alpha;
		m.texcoords[0][0] = 0;
		m.texcoords[0][1] = 1;
		m.texcoords[0][2] = 0;
		m.texcoords[0][3] = 0;
		m.texcoords[0][4] = 1;
		m.texcoords[0][5] = 0;
		m.texcoords[0][6] = 1;
		m.texcoords[0][7] = 1;
		// FIXME: negate left and right in loader
		m.vertex[0] = origin[0] + frame->down * up[0] - frame->left  * left[0];
		m.vertex[1] = origin[1] + frame->down * up[1] - frame->left  * left[1];
		m.vertex[2] = origin[2] + frame->down * up[2] - frame->left  * left[2];
		m.vertex[4] = origin[0] + frame->up   * up[0] - frame->left  * left[0];
		m.vertex[5] = origin[1] + frame->up   * up[1] - frame->left  * left[1];
		m.vertex[6] = origin[2] + frame->up   * up[2] - frame->left  * left[2];
		m.vertex[8] = origin[0] + frame->up   * up[0] - frame->right * left[0];
		m.vertex[9] = origin[1] + frame->up   * up[1] - frame->right * left[1];
		m.vertex[10] = origin[2] + frame->up   * up[2] - frame->right * left[2];
		m.vertex[12] = origin[0] + frame->down * up[0] - frame->right * left[0];
		m.vertex[13] = origin[1] + frame->down * up[1] - frame->right * left[1];
		m.vertex[14] = origin[2] + frame->down * up[2] - frame->right * left[2];
	}
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel ()
{
	int			i;
	vec3_t		left, up, org, color;
	mspriteframe_t *frame;
	vec3_t diff;
	float		fog, ifog;

	if (currentrenderentity->frameblend[0].frame < 0)
		return;

	if (R_SpriteSetup(currentrenderentity->model->sprnum_type, org, left, up))
		return;

	c_sprites++;

	if ((currentrenderentity->model->flags & EF_FULLBRIGHT) || (currentrenderentity->effects & EF_FULLBRIGHT))
		color[0] = color[1] = color[2] = 1;
	else
		R_CompleteLightPoint(color, currentrenderentity->origin, true, NULL);

	if (fogenabled)
	{
		VectorSubtract(currentrenderentity->origin, r_origin, diff);
		fog = exp(fogdensity/DotProduct(diff,diff));
		if (fog > 1)
			fog = 1;
	}
	else
		fog = 0;
	ifog = 1 - fog;

#ifdef LERPSPRITES
	// LordHavoc: interpolated sprite rendering
	for (i = 0;i < 4;i++)
	{
		if (currentrenderentity->frameblend[i].lerp >= 0.01f)
		{
			frame = currentrenderentity->model->sprdata_frames + currentrenderentity->frameblend[i].frame;
			GL_DrawSpriteImage(false, frame, R_GetTexture(frame->texture), org, up, left, color[0] * ifog, color[1] * ifog, color[2] * ifog, currentrenderentity->alpha * currentrenderentity->frameblend[i].lerp);
			if (fog * currentrenderentity->frameblend[i].lerp >= 0.01f)
				GL_DrawSpriteImage(true, frame, R_GetTexture(frame->fogtexture), org, up, left, fogcolor[0],fogcolor[1],fogcolor[2], fog * currentrenderentity->alpha * currentrenderentity->frameblend[i].lerp);
		}
	}
#else
	// LordHavoc: no interpolation
	frame = NULL;
	for (i = 0;i < 4 && currentrenderentity->frameblend[i].lerp;i++)
		frame = currentrenderentity->model->sprdata_frames + currentrenderentity->frameblend[i].frame;

	GL_DrawSpriteImage(false, frame, R_GetTexture(frame->texture), org, up, left, color[0] * ifog, color[1] * ifog, color[2] * ifog, currentrenderentity->alpha);
	if (fog * currentrenderentity->frameblend[i].lerp >= 0.01f)
		GL_DrawSpriteImage(true, frame, R_GetTexture(frame->fogtexture), org, up, left, fogcolor[0],fogcolor[1],fogcolor[2], fog * currentrenderentity->alpha);
#endif
}

