
#include "quakedef.h"

void R_DrawSpriteModelCallback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = (entity_render_t *)calldata1;
	int i;
	vec3_t left, up, org, color, diffusecolor, diffusenormal;
	mspriteframe_t *frame;
	float scale;

	// nudge it toward the view to make sure it isn't in a wall
	org[0] = ent->matrix.m[0][3] - r_viewforward[0];
	org[1] = ent->matrix.m[1][3] - r_viewforward[1];
	org[2] = ent->matrix.m[2][3] - r_viewforward[2];
	switch(ent->model->sprite.sprnum_type)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
		// flames and such
		// vertical beam sprite, faces view plane
		scale = ent->scale / sqrt(r_viewforward[0]*r_viewforward[0]+r_viewforward[1]*r_viewforward[1]);
		left[0] = -r_viewforward[1] * scale;
		left[1] = r_viewforward[0] * scale;
		left[2] = 0;
		up[0] = 0;
		up[1] = 0;
		up[2] = ent->scale;
		break;
	case SPR_FACING_UPRIGHT:
		// flames and such
		// vertical beam sprite, faces viewer's origin (not the view plane)
		scale = ent->scale / sqrt((org[0] - r_vieworigin[0])*(org[0] - r_vieworigin[0])+(org[1] - r_vieworigin[1])*(org[1] - r_vieworigin[1]));
		left[0] = (org[1] - r_vieworigin[1]) * scale;
		left[1] = -(org[0] - r_vieworigin[0]) * scale;
		left[2] = 0;
		up[0] = 0;
		up[1] = 0;
		up[2] = ent->scale;
		break;
	default:
		Con_Printf("R_SpriteSetup: unknown sprite type %i\n", ent->model->sprite.sprnum_type);
		// fall through to normal sprite
	case SPR_VP_PARALLEL:
		// normal sprite
		// faces view plane
		left[0] = r_viewleft[0] * ent->scale;
		left[1] = r_viewleft[1] * ent->scale;
		left[2] = r_viewleft[2] * ent->scale;
		up[0] = r_viewup[0] * ent->scale;
		up[1] = r_viewup[1] * ent->scale;
		up[2] = r_viewup[2] * ent->scale;
		break;
	case SPR_ORIENTED:
		// bullet marks on walls
		// ignores viewer entirely
		left[0] = ent->matrix.m[0][1];
		left[1] = ent->matrix.m[1][1];
		left[2] = ent->matrix.m[2][1];
		up[0] = ent->matrix.m[0][2];
		up[1] = ent->matrix.m[1][2];
		up[2] = ent->matrix.m[2][2];
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		// I have no idea what people would use this for...
		// oriented relative to view space
		// FIXME: test this and make sure it mimicks software
		left[0] = ent->matrix.m[0][1] * r_viewforward[0] + ent->matrix.m[1][1] * r_viewleft[0] + ent->matrix.m[2][1] * r_viewup[0];
		left[1] = ent->matrix.m[0][1] * r_viewforward[1] + ent->matrix.m[1][1] * r_viewleft[1] + ent->matrix.m[2][1] * r_viewup[1];
		left[2] = ent->matrix.m[0][1] * r_viewforward[2] + ent->matrix.m[1][1] * r_viewleft[2] + ent->matrix.m[2][1] * r_viewup[2];
		up[0] = ent->matrix.m[0][2] * r_viewforward[0] + ent->matrix.m[1][2] * r_viewleft[0] + ent->matrix.m[2][2] * r_viewup[0];
		up[1] = ent->matrix.m[0][2] * r_viewforward[1] + ent->matrix.m[1][2] * r_viewleft[1] + ent->matrix.m[2][2] * r_viewup[1];
		up[2] = ent->matrix.m[0][2] * r_viewforward[2] + ent->matrix.m[1][2] * r_viewleft[2] + ent->matrix.m[2][2] * r_viewup[2];
		break;
	}

	R_Mesh_Matrix(&r_identitymatrix);

	if (!(ent->flags & RENDER_LIGHT))
		color[0] = color[1] = color[2] = 1;
	else
	{
		R_CompleteLightPoint(color, diffusecolor, diffusenormal, ent->origin, true);
		VectorMA(color, 0.5f, diffusecolor, color);
	}
	color[0] *= ent->colormod[0];
	color[1] *= ent->colormod[1];
	color[2] *= ent->colormod[2];

	// LordHavoc: interpolated sprite rendering
	for (i = 0;i < 4;i++)
	{
		if (ent->frameblend[i].lerp >= 0.01f)
		{
			frame = ent->model->sprite.sprdata_frames + ent->frameblend[i].frame;
			// FIXME: negate left and right in loader
			R_DrawSprite(GL_SRC_ALPHA, (ent->effects & EF_ADDITIVE) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, frame->texture, frame->fogtexture, (ent->effects & EF_NODEPTHTEST), org, left, up, frame->left, frame->right, frame->down, frame->up, color[0], color[1], color[2], ent->alpha * ent->frameblend[i].lerp);
		}
	}
}

void R_Model_Sprite_Draw(entity_render_t *ent)
{
	if (ent->frameblend[0].frame < 0)
		return;

	R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : ent->origin, R_DrawSpriteModelCallback, ent, 0);
}

