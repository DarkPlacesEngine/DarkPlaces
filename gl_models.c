
#include "quakedef.h"

// LordHavoc: vertex array
float *aliasvert;
float *aliasvertnorm;
byte *aliasvertcolor;
byte *aliasvertcolor2;

int chrometexture;

void makechrometexture()
{
	int i;
	byte noise[64*64];
	byte data[64*64][4];

	fractalnoise(noise, 64);

	// convert to RGBA data
	for (i = 0;i < 64*64;i++)
	{
		data[i][0] = data[i][1] = data[i][2] = noise[i];
		data[i][3] = 255;
	}

	chrometexture = GL_LoadTexture ("chrometexture", 64, 64, &data[0][0], true, false, 4);
}

void gl_models_start()
{
	// allocate vertex processing arrays
	aliasvert = malloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertnorm = malloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertcolor = malloc(sizeof(byte[MD2MAX_VERTS][4]));
	aliasvertcolor2 = malloc(sizeof(byte[MD2MAX_VERTS][4])); // used temporarily for tinted coloring
	makechrometexture();
}

void gl_models_shutdown()
{
	free(aliasvert);
	free(aliasvertnorm);
	free(aliasvertcolor);
	free(aliasvertcolor2);
}

void GL_Models_Init()
{
	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown);
}

extern vec3_t softwaretransform_x;
extern vec3_t softwaretransform_y;
extern vec3_t softwaretransform_z;
extern vec_t softwaretransform_scale;
extern vec3_t softwaretransform_offset;
void R_AliasLerpVerts(int vertcount, float lerp, trivert2 *verts1, vec3_t scale1, vec3_t translate1, trivert2 *verts2, vec3_t scale2, vec3_t translate2)
{
	int i;
	vec3_t point, matrix_x, matrix_y, matrix_z;
	float *av, *avn;
	av = aliasvert;
	avn = aliasvertnorm;
	VectorScale(softwaretransform_x, softwaretransform_scale, matrix_x);
	VectorScale(softwaretransform_y, softwaretransform_scale, matrix_y);
	VectorScale(softwaretransform_z, softwaretransform_scale, matrix_z);
	if (lerp < 0) lerp = 0;
	if (lerp > 1) lerp = 1;
	if (lerp != 0)
	{
		float ilerp, ilerp127, lerp127, scalex1, scalex2, translatex, scaley1, scaley2, translatey, scalez1, scalez2, translatez;
		if (lerp < 0) lerp = 0;
		if (lerp > 1) lerp = 1;
		ilerp = 1 - lerp;
		ilerp127 = ilerp * (1.0 / 127.0);
		lerp127 = lerp * (1.0 / 127.0);
		// calculate combined interpolation variables
		scalex1 = scale1[0] * ilerp;scalex2 = scale2[0] *  lerp;translatex = translate1[0] * ilerp + translate2[0] *  lerp;
		scaley1 = scale1[1] * ilerp;scaley2 = scale2[1] *  lerp;translatey = translate1[1] * ilerp + translate2[1] *  lerp;
		scalez1 = scale1[2] * ilerp;scalez2 = scale2[2] *  lerp;translatez = translate1[2] * ilerp + translate2[2] *  lerp;
		// generate vertices
		for (i = 0;i < vertcount;i++)
		{
			// rotate, scale, and translate the vertex locations
			point[0] = verts1->v[0] * scalex1 + verts2->v[0] * scalex2 + translatex;
			point[1] = verts1->v[1] * scaley1 + verts2->v[1] * scaley2 + translatey;
			point[2] = verts1->v[2] * scalez1 + verts2->v[2] * scalez2 + translatez;
			*av++ = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
			*av++ = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
			*av++ = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
			// rotate the normals
			point[0] = verts1->n[0] * ilerp127 + verts2->n[0] * lerp127;
			point[1] = verts1->n[1] * ilerp127 + verts2->n[1] * lerp127;
			point[2] = verts1->n[2] * ilerp127 + verts2->n[2] * lerp127;
			*avn++ = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
			*avn++ = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
			*avn++ = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
			verts1++;verts2++;
		}
	}
	else
	{
		// generate vertices
		for (i = 0;i < vertcount;i++)
		{
			// rotate, scale, and translate the vertex locations
			point[0] = verts1->v[0] * scale1[0] + translate1[0];
			point[1] = verts1->v[1] * scale1[1] + translate1[1];
			point[2] = verts1->v[2] * scale1[2] + translate1[2];
			*av++ = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
			*av++ = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
			*av++ = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
			// rotate the normals
			point[0] = verts1->n[0] * (1.0f / 127.0f);
			point[1] = verts1->n[1] * (1.0f / 127.0f);
			point[2] = verts1->n[2] * (1.0f / 127.0f);
			*avn++ = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
			*avn++ = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
			*avn++ = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
			verts1++;
		}
	}
}

float R_CalcAnimLerp(entity_t *ent, int pose, float lerpscale)
{
	if (ent->draw_lastmodel == ent->model && ent->draw_lerpstart <= cl.time)
	{
		if (pose != ent->draw_pose)
		{
			ent->draw_lastpose = ent->draw_pose;
			ent->draw_pose = pose;
			ent->draw_lerpstart = cl.time;
			return 0;
		}
		else
			return ((cl.time - ent->draw_lerpstart) * lerpscale);
	}
	else // uninitialized
	{
		ent->draw_lastmodel = ent->model;
		ent->draw_lastpose = ent->draw_pose = pose;
		ent->draw_lerpstart = cl.time;
		return 0;
	}
}

extern cvar_t gl_vertexarrays;
extern qboolean lighthalf;
void GL_DrawModelMesh(int skin, byte *colors, maliashdr_t *maliashdr)
{
	int i;
	if (!r_render.value)
		return;
	glBindTexture(GL_TEXTURE_2D, skin);
	if (!colors)
	{
		if (lighthalf)
			glColor3f(0.5f, 0.5f, 0.5f);
		else
			glColor3f(1.0f, 1.0f, 1.0f);
	}
	if (gl_vertexarrays.value)
	{
		if (colors)
		{
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
			glEnableClientState(GL_COLOR_ARRAY);
		}

		qglTexCoordPointer(2, GL_FLOAT, 0, (void *)((int) maliashdr->texdata + (int) maliashdr));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		qglDrawElements(GL_TRIANGLES, maliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) maliashdr + maliashdr->tridata));

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		if (colors)
			glDisableClientState(GL_COLOR_ARRAY);
	}
	else
	{
		unsigned short *in, index;
		float *tex;
		in = (void *)((int) maliashdr + maliashdr->tridata);
		glBegin(GL_TRIANGLES);
		tex = (void *)((int) maliashdr + maliashdr->texdata);
		for (i = 0;i < maliashdr->numtris * 3;i++)
		{
			index = *in++;
			glTexCoord2f(tex[index*2], tex[index*2+1]);
			if (colors)
				glColor4f(colors[index*4] * (1.0f / 255.0f), colors[index*4+1] * (1.0f / 255.0f), colors[index*4+2] * (1.0f / 255.0f), colors[index*4+3] * (1.0f / 255.0f));
			glVertex3fv(&aliasvert[index*3]);
		}
		glEnd();
	}
	// leave it in a state for additional passes
	glDepthMask(0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive
}

void R_TintModel(byte *in, byte *out, int verts, byte *color)
{
	int i;
	byte r = color[0];
	byte g = color[1];
	byte b = color[2];
	for (i = 0;i < verts;i++)
	{
		out[0] = (byte) ((in[0] * r) >> 8);
		out[1] = (byte) ((in[1] * g) >> 8);
		out[2] = (byte) ((in[2] * b) >> 8);
		out[3] =          in[3];
		in += 4;
		out += 4;
	}
}

/*
=================
R_DrawAliasFrame

=================
*/
extern vec3_t lightspot;
void R_LightModel(int numverts, vec3_t center, vec3_t basecolor);
void R_DrawAliasFrame (maliashdr_t *maliashdr, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, int frame, int *skin, int colormap, int effects, int flags)
{
	int		i, pose;
	float	lerpscale, lerp;
	maliasframe_t *frameinfo;

	softwaretransformforentity(ent);

	if ((frame >= maliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	frameinfo = ((maliasframe_t *)((int) maliashdr + maliashdr->framedata)) + frame;
	pose = frameinfo->start;

	if (frameinfo->length > 1)
	{
		lerpscale = frameinfo->rate;
		pose += (int)(cl.time * frameinfo->rate) % frameinfo->length;
	}
	else
		lerpscale = 10.0f;

	lerp = R_CalcAnimLerp(ent, pose, lerpscale);

	R_AliasLerpVerts(maliashdr->numverts, lerp, (trivert2 *)((int) maliashdr + maliashdr->posedata) + ent->draw_lastpose * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin, (trivert2 *)((int) maliashdr + maliashdr->posedata) + ent->draw_pose * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin);

	// prep the vertex array as early as possible
	if (r_render.value)
	{
		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
		}
	}

	R_LightModel(maliashdr->numverts, org, color);

	if (!r_render.value)
		return;
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glShadeModel(GL_SMOOTH);
	if (effects & EF_ADDITIVE)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive rendering
		glEnable(GL_BLEND);
		glDepthMask(0);
	}
	else if (alpha != 1.0)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glDepthMask(0);
	}
	else
	{
		glDisable(GL_BLEND);
		glDepthMask(1);
	}

	if (colormap >= 0)
	{
		if (!skin[0] && !skin[1] && !skin[2] && !skin[3])
			GL_DrawModelMesh(0, NULL, maliashdr);
		else
		{
			int c;
			if (skin[0])
				GL_DrawModelMesh(skin[0], aliasvertcolor, maliashdr);
			if (skin[1])
			{
				c = (colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
				R_TintModel(aliasvertcolor, aliasvertcolor2, maliashdr->numverts, (byte *) (&d_8to24table[c]));
				GL_DrawModelMesh(skin[1], aliasvertcolor2, maliashdr);
			}
			if (skin[2])
			{
				c = colormap & 0xF0      ;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
				R_TintModel(aliasvertcolor, aliasvertcolor2, maliashdr->numverts, (byte *) (&d_8to24table[c]));
				GL_DrawModelMesh(skin[2], aliasvertcolor2, maliashdr);
			}
			if (skin[3]) GL_DrawModelMesh(skin[3], NULL, maliashdr);
		}
	}
	else
	{
		if (!skin[3] && !skin[4])
			GL_DrawModelMesh(0, NULL, maliashdr);
		else
		{
			if (skin[4]) GL_DrawModelMesh(skin[4], aliasvertcolor, maliashdr);
			if (skin[3]) GL_DrawModelMesh(skin[3], NULL, maliashdr);
		}
	}

	if (fogenabled)
	{
		vec3_t diff;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates

		VectorSubtract(org, r_refdef.vieworg, diff);
		glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));

		if (gl_vertexarrays.value)
		{
			qglDrawElements(GL_TRIANGLES, maliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) maliashdr + maliashdr->tridata));
		}
		else
		{
			unsigned short *in;
			in = (void *)((int) maliashdr + maliashdr->tridata);
			glBegin(GL_TRIANGLES);
			for (i = 0;i < maliashdr->numtris * 3;i++)
				glVertex3fv(&aliasvert[*in++ * 3]);
			glEnd();
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}
	if (gl_vertexarrays.value)
		glDisableClientState(GL_VERTEX_ARRAY);

	if (!fogenabled && r_shadows.value && !(effects & EF_ADDITIVE) && shadow)
	{
		// flatten it to make a shadow
		float *av = aliasvert + 2, l = lightspot[2] + 0.125;
		av = aliasvert + 2;
		for (i = 0;i < maliashdr->numverts;i++, av+=3)
			if (*av > l)
				*av = l;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		glColor4f (0,0,0,0.5 * alpha);

		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
			qglDrawElements(GL_TRIANGLES, maliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) maliashdr + maliashdr->tridata));
			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			unsigned short *in;
			in = (void *)((int) maliashdr + maliashdr->tridata);
			glBegin(GL_TRIANGLES);
			for (i = 0;i < maliashdr->numtris * 3;i++)
				glVertex3fv(&aliasvert[*in++ * 3]);
			glEnd();
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

/*
=================
R_DrawQ2AliasFrame

=================
*/
void R_DrawQ2AliasFrame (md2mem_t *pheader, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, int frame, int skin, int effects, int flags)
{
	int *order, count;
	float lerp;
	md2memframe_t *frame1, *frame2;

	if (r_render.value)
		glBindTexture(GL_TEXTURE_2D, skin);

	softwaretransformforentity(ent);

	if ((frame >= pheader->num_frames) || (frame < 0))
	{
		Con_DPrintf ("R_SetupQ2AliasFrame: no such frame %d\n", frame);
		frame = 0;
	}

	lerp = R_CalcAnimLerp(ent, frame, 10);

	frame1 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * ent->draw_lastpose));
	frame2 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * ent->draw_pose));
	R_AliasLerpVerts(pheader->num_xyz, lerp, frame1->verts, frame1->scale, frame1->translate, frame2->verts, frame2->scale, frame2->translate);

	R_LightModel(pheader->num_xyz, org, color);

	if (!r_render.value)
		return;
	if (gl_vertexarrays.value)
	{
		// LordHavoc: big mess...
		// using arrays only slightly, although it is enough to prevent duplicates
		// (saving half the transforms)
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, aliasvertcolor);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		order = (int *)((int)pheader + pheader->ofs_glcmds);
		while(1)
		{
			if (!(count = *order++))
				break;
			if (count > 0)
				glBegin(GL_TRIANGLE_STRIP);
			else
			{
				glBegin(GL_TRIANGLE_FAN);
				count = -count;
			}
			do
			{
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
				qglArrayElement(order[2]);
				order += 3;
			}
			while (count--);
		}

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		order = (int *)((int)pheader + pheader->ofs_glcmds);
		while(1)
		{
			if (!(count = *order++))
				break;
			if (count > 0)
				glBegin(GL_TRIANGLE_STRIP);
			else
			{
				glBegin(GL_TRIANGLE_FAN);
				count = -count;
			}
			do
			{
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
				glColor4f(aliasvertcolor[order[2] * 4] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 1] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 2] * (1.0f / 255.0f), aliasvertcolor[order[2] * 4 + 3] * (1.0f / 255.0f));
				glVertex3fv(&aliasvert[order[2] * 3]);
				order += 3;
			}
			while (count--);
		}
	}

	if (fogenabled)
	{
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		{
			vec3_t diff;
			VectorSubtract(org, r_refdef.vieworg, diff);
			glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));
		}

		if (gl_vertexarrays.value)
		{
			// LordHavoc: big mess...
			// using arrays only slightly, although it is enough to prevent duplicates
			// (saving half the transforms)
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);

			order = (int *)((int)pheader + pheader->ofs_glcmds);
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					qglArrayElement(order[2]);
					order += 3;
				}
				while (count--);
			}

			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			order = (int *)((int)pheader + pheader->ofs_glcmds);
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			}
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	if (!fogenabled && r_shadows.value && !(effects & EF_ADDITIVE) && shadow)
	{
		int i;
		float *av = aliasvert + 2, l = lightspot[2] + 0.125;
		av = aliasvert + 2;
		for (i = 0;i < pheader->num_xyz;i++, av+=3)
			if (*av > l)
				*av = l;
		glDisable (GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_BLEND);
		glDepthMask(0); // disable zbuffer updates
		glColor4f (0,0,0,0.5 * alpha);

		if (gl_vertexarrays.value)
		{
			qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
			glEnableClientState(GL_VERTEX_ARRAY);
						
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					qglArrayElement(order[2]);
					order += 3;
				}
				while (count--);
			}

			glDisableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			while(1)
			{
				if (!(count = *order++))
					break;
				if (count > 0)
					glBegin(GL_TRIANGLE_STRIP);
				else
				{
					glBegin(GL_TRIANGLE_FAN);
					count = -count;
				}
				do
				{
					glVertex3fv(&aliasvert[order[2] * 3]);
					order += 3;
				}
				while (count--);
			}
		}

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

int modeldlightbits[8];
extern int r_dlightframecount;
extern void R_LightPoint (vec3_t color, vec3_t p);

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *ent, int cull, float alpha, model_t *clmodel, int frame, int skin, vec3_t org, int effects, int flags, int colormap)
{
	int			i;
	vec3_t		mins, maxs, color;
	mleaf_t		*leaf;
	void		*modelheader;
	int			*skinset;

	if (alpha < (1.0 / 64.0))
		return; // basically completely transparent

	VectorAdd (org, clmodel->mins, mins);
	VectorAdd (org, clmodel->maxs, maxs);

	if (cull && R_CullBox (mins, maxs))
		return;

	leaf = Mod_PointInLeaf (org, cl.worldmodel);
	if (leaf->dlightframe == r_dlightframecount)
		for (i = 0;i < 8;i++)
			modeldlightbits[i] = leaf->dlightbits[i];
	else
		for (i = 0;i < 8;i++)
			modeldlightbits[i] = 0;

	// get lighting information

	if ((flags & EF_FULLBRIGHT) || (effects & EF_FULLBRIGHT))
		color[0] = color[1] = color[2] = 256;
	else
	{
		// HACK HACK HACK -- no fullbright colors, so make torches full light
		if (!strcmp (clmodel->name, "progs/flame2.mdl") || !strcmp (clmodel->name, "progs/flame.mdl") )
			color[0] = color[1] = color[2] = 128;
		else
			R_LightPoint (color, org);
	}

	if (r_render.value)
		glDisable(GL_ALPHA_TEST);

	if (frame < 0 || frame >= clmodel->numframes)
	{
		frame = 0;
		Con_DPrintf("invalid skin number %d for model %s\n", frame, clmodel->name);
	}

	if (skin < 0 || skin >= clmodel->numskins)
	{
		skin = 0;
		Con_DPrintf("invalid skin number %d for model %s\n", skin, clmodel->name);
	}

	modelheader = Mod_Extradata (clmodel);

	{
//		int *skinanimrange = (int *) (clmodel->skinanimrange + (int) modelheader) + skin * 2;
//		int *skinanim = (int *) (clmodel->skinanim + (int) modelheader);
		int *skinanimrange = clmodel->skinanimrange + skin * 2;
		int *skinanim = clmodel->skinanim;
		i = skinanimrange[0];
		if (skinanimrange[1] > 1) // animated
			i += ((int) (cl.time * 10) % skinanimrange[1]);
		skinset = skinanim + i*5;
	}

	if (r_render.value)
		glEnable (GL_TEXTURE_2D);

	c_alias_polys += clmodel->numtris;
	if (clmodel->aliastype == ALIASTYPE_MD2)
		R_DrawQ2AliasFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, frame, skinset[0], effects, flags);
	else
		R_DrawAliasFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, frame, skinset, colormap, effects, flags);
}
