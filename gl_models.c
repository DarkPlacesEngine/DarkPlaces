
#include "quakedef.h"

typedef struct
{
	float m[3][4];
} zymbonematrix;

// LordHavoc: vertex array
float *aliasvert;
float *modelaliasvert;
float *aliasvertnorm;
byte *aliasvertcolor;
byte *aliasvertcolor2;
zymbonematrix *zymbonepose;
int *aliasvertusage;

int chrometexture;

void makechrometexture()
{
	int i;
	byte noise[64*64];
	byte data[64*64][4];

	fractalnoise(noise, 64, 16);

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
	aliasvert = qmalloc(sizeof(float[MD2MAX_VERTS][3]));
	modelaliasvert = qmalloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertnorm = qmalloc(sizeof(float[MD2MAX_VERTS][3]));
	aliasvertcolor = qmalloc(sizeof(byte[MD2MAX_VERTS][4]));
	aliasvertcolor2 = qmalloc(sizeof(byte[MD2MAX_VERTS][4])); // used temporarily for tinted coloring
	zymbonepose = qmalloc(sizeof(zymbonematrix[256]));
	aliasvertusage = qmalloc(sizeof(int[MD2MAX_VERTS]));
	makechrometexture();
}

void gl_models_shutdown()
{
	qfree(aliasvert);
	qfree(aliasvertnorm);
	qfree(aliasvertcolor);
	qfree(aliasvertcolor2);
	qfree(zymbonepose);
	qfree(aliasvertusage);
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
extern cvar_t r_modelsdonttransformnormals;
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
		ilerp = 1 - lerp;
		ilerp127 = ilerp * (1.0 / 127.0);
		lerp127 = lerp * (1.0 / 127.0);
		// calculate combined interpolation variables
		scalex1 = scale1[0] * ilerp;scalex2 = scale2[0] *  lerp;translatex = translate1[0] * ilerp + translate2[0] *  lerp;
		scaley1 = scale1[1] * ilerp;scaley2 = scale2[1] *  lerp;translatey = translate1[1] * ilerp + translate2[1] *  lerp;
		scalez1 = scale1[2] * ilerp;scalez2 = scale2[2] *  lerp;translatez = translate1[2] * ilerp + translate2[2] *  lerp;
		// generate vertices
		if (r_modelsdonttransformnormals.value)
		{
			float *modelav = modelaliasvert;
			for (i = 0;i < vertcount;i++)
			{
				// rotate, scale, and translate the vertex locations
				point[0] = verts1->v[0] * scalex1 + verts2->v[0] * scalex2 + translatex;
				point[1] = verts1->v[1] * scaley1 + verts2->v[1] * scaley2 + translatey;
				point[2] = verts1->v[2] * scalez1 + verts2->v[2] * scalez2 + translatez;
				// save mostly un-transformed copy for lighting
				modelav[0] = point[0] * softwaretransform_scale;
				modelav[1] = point[1] * softwaretransform_scale;
				modelav[2] = point[2] * softwaretransform_scale;
				av[0] = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
				av[1] = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
				av[2] = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
				// decompress but do not transform the normals
				avn[0] = verts1->n[0] * ilerp127 + verts2->n[0] * lerp127;
				avn[1] = verts1->n[1] * ilerp127 + verts2->n[1] * lerp127;
				avn[2] = verts1->n[2] * ilerp127 + verts2->n[2] * lerp127;
				modelav += 3;
				av += 3;
				avn += 3;
				verts1++;verts2++;
			}
		}
		else
		{
			for (i = 0;i < vertcount;i++)
			{
				// rotate, scale, and translate the vertex locations
				point[0] = verts1->v[0] * scalex1 + verts2->v[0] * scalex2 + translatex;
				point[1] = verts1->v[1] * scaley1 + verts2->v[1] * scaley2 + translatey;
				point[2] = verts1->v[2] * scalez1 + verts2->v[2] * scalez2 + translatez;
				av[0] = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
				av[1] = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
				av[2] = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
				// rotate the normals
				point[0] = verts1->n[0] * ilerp127 + verts2->n[0] * lerp127;
				point[1] = verts1->n[1] * ilerp127 + verts2->n[1] * lerp127;
				point[2] = verts1->n[2] * ilerp127 + verts2->n[2] * lerp127;
				avn[0] = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
				avn[1] = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
				avn[2] = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
				av += 3;
				avn += 3;
				verts1++;verts2++;
			}
		}
	}
	else
	{
		// generate vertices
		if (r_modelsdonttransformnormals.value)
		{
			float *modelav = modelaliasvert;
			for (i = 0;i < vertcount;i++)
			{
				// rotate, scale, and translate the vertex locations
				point[0] = verts1->v[0] * scale1[0] + translate1[0];
				point[1] = verts1->v[1] * scale1[1] + translate1[1];
				point[2] = verts1->v[2] * scale1[2] + translate1[2];
				// save mostly un-transformed copy for lighting
				modelav[0] = point[0] * softwaretransform_scale;
				modelav[1] = point[1] * softwaretransform_scale;
				modelav[2] = point[2] * softwaretransform_scale;
				av[0] = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
				av[1] = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
				av[2] = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
				// decompress normal but do not rotate it
				avn[0] = verts1->n[0] * (1.0f / 127.0f);
				avn[1] = verts1->n[1] * (1.0f / 127.0f);
				avn[2] = verts1->n[2] * (1.0f / 127.0f);
				modelav += 3;
				av += 3;
				avn += 3;
				verts1++;
			}
		}
		else
		{
			for (i = 0;i < vertcount;i++)
			{
				// rotate, scale, and translate the vertex locations
				point[0] = verts1->v[0] * scale1[0] + translate1[0];
				point[1] = verts1->v[1] * scale1[1] + translate1[1];
				point[2] = verts1->v[2] * scale1[2] + translate1[2];
				av[0] = point[0] * matrix_x[0] + point[1] * matrix_y[0] + point[2] * matrix_z[0] + softwaretransform_offset[0];
				av[1] = point[0] * matrix_x[1] + point[1] * matrix_y[1] + point[2] * matrix_z[1] + softwaretransform_offset[1];
				av[2] = point[0] * matrix_x[2] + point[1] * matrix_y[2] + point[2] * matrix_z[2] + softwaretransform_offset[2];
				// rotate the normals
				point[0] = verts1->n[0] * (1.0f / 127.0f);
				point[1] = verts1->n[1] * (1.0f / 127.0f);
				point[2] = verts1->n[2] * (1.0f / 127.0f);
				avn[0] = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
				avn[1] = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
				avn[2] = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
				av += 3;
				avn += 3;
				verts1++;
			}
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
void R_DrawAliasFrame (maliashdr_t *maliashdr, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, int frame, int *skin, int colormap, int effects, int flags)
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

	if (skin[0] || skin[1] || skin[2] || skin[3] || skin[4])
	{
		if (colormap >= 0 && (skin[0] || skin[1] || skin[2]))
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
		}
		else
		{
			if (skin[4]) GL_DrawModelMesh(skin[4], aliasvertcolor, maliashdr);
			else
			{
				if (skin[0]) GL_DrawModelMesh(skin[0], aliasvertcolor, maliashdr);
				if (skin[1]) GL_DrawModelMesh(skin[1], aliasvertcolor, maliashdr);
				if (skin[2]) GL_DrawModelMesh(skin[2], aliasvertcolor, maliashdr);
			}
		}
		if (skin[3]) GL_DrawModelMesh(skin[3], NULL, maliashdr);
	}
	else
		GL_DrawModelMesh(0, NULL, maliashdr);

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
void R_DrawQ2AliasFrame (md2mem_t *pheader, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, int frame, int skin, int effects, int flags)
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

void ZymoticLerpBones(int count, float lerp2, zymbonematrix *bone1, zymbonematrix *bone2, zymbone_t *bone, float rootorigin[3], float rootangles[3])
{
	float lerp1;
	zymbonematrix *out, rootmatrix, m;
	lerp1 = 1 - lerp2;
	out = zymbonepose;
	AngleVectors(rootangles, rootmatrix.m[0], rootmatrix.m[1], rootmatrix.m[2]);
	rootmatrix.m[0][3] = rootorigin[0];
	rootmatrix.m[1][3] = rootorigin[1];
	rootmatrix.m[2][3] = rootorigin[2];
	if (lerp1 != 1) // interpolation
	{
		while(count--)
		{
			// interpolate matrices
			m.m[0][0] = bone1->m[0][0] * lerp1 + bone2->m[0][0] * lerp2;
			m.m[0][1] = bone1->m[0][1] * lerp1 + bone2->m[0][1] * lerp2;
			m.m[0][2] = bone1->m[0][2] * lerp1 + bone2->m[0][2] * lerp2;
			m.m[0][3] = bone1->m[0][3] * lerp1 + bone2->m[0][3] * lerp2;
			m.m[1][0] = bone1->m[1][0] * lerp1 + bone2->m[1][0] * lerp2;
			m.m[1][1] = bone1->m[1][1] * lerp1 + bone2->m[1][1] * lerp2;
			m.m[1][2] = bone1->m[1][2] * lerp1 + bone2->m[1][2] * lerp2;
			m.m[1][3] = bone1->m[1][3] * lerp1 + bone2->m[1][3] * lerp2;
			m.m[2][0] = bone1->m[2][0] * lerp1 + bone2->m[2][0] * lerp2;
			m.m[2][1] = bone1->m[2][1] * lerp1 + bone2->m[2][1] * lerp2;
			m.m[2][2] = bone1->m[2][2] * lerp1 + bone2->m[2][2] * lerp2;
			m.m[2][3] = bone1->m[2][3] * lerp1 + bone2->m[2][3] * lerp2;
			if (bone->parent >= 0)
				R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &m.m[0], &out->m[0]);
			else
				R_ConcatTransforms(&rootmatrix.m[0], &m.m[0], &out->m[0]);
			bone1++;
			bone2++;
			bone++;
			out++;
		}
	}
	else // no interpolation
	{
		while(count--)
		{
			if (bone->parent >= 0)
				R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &bone1->m[0], &out->m[0]);
			else
				R_ConcatTransforms(&rootmatrix.m[0], &bone1->m[0], &out->m[0]);
			bone1++;
			bone++;
			out++;
		}
	}
}

void ZymoticTransformVerts(int vertcount, int *bonecounts, zymvertex_t *vert)
{
	int c;
	float *out = aliasvert;
	zymbonematrix *matrix;
	while(vertcount--)
	{
		c = *bonecounts++;
		if (c == 1)
		{
			matrix = &zymbonepose[vert->bonenum];
			out[0] = vert->origin[0] * matrix->m[0][0] + vert->origin[1] * matrix->m[0][1] + vert->origin[2] * matrix->m[0][2] + matrix->m[0][3];
			out[1] = vert->origin[0] * matrix->m[1][0] + vert->origin[1] * matrix->m[1][1] + vert->origin[2] * matrix->m[1][2] + matrix->m[1][3];
			out[2] = vert->origin[0] * matrix->m[2][0] + vert->origin[1] * matrix->m[2][1] + vert->origin[2] * matrix->m[2][2] + matrix->m[2][3];
			vert++;
		}
		else
		{
			VectorClear(out);
			while(c--)
			{
				matrix = &zymbonepose[vert->bonenum];
				out[0] += vert->origin[0] * matrix->m[0][0] + vert->origin[1] * matrix->m[0][1] + vert->origin[2] * matrix->m[0][2] + matrix->m[0][3];
				out[1] += vert->origin[0] * matrix->m[1][0] + vert->origin[1] * matrix->m[1][1] + vert->origin[2] * matrix->m[1][2] + matrix->m[1][3];
				out[2] += vert->origin[0] * matrix->m[2][0] + vert->origin[1] * matrix->m[2][1] + vert->origin[2] * matrix->m[2][2] + matrix->m[2][3];
				vert++;
			}
		}
		out += 3;
	}
}

float ixtable[4096];

void ZymoticCalcNormals(int vertcount, int shadercount, int *renderlist)
{
	int a, b, c, d;
	float *out, v1[3], v2[3], normal[3];
	int *u;
	if (!ixtable[1])
	{
		ixtable[0] = 0;
		for (a = 1;a < 4096;a++)
			ixtable[a] = 1.0f / a;
	}
	// clear normals
	memset(aliasvertnorm, 0, sizeof(float[3]) * vertcount);
	memset(aliasvertusage, 0, sizeof(int) * vertcount);
	// parse render list and accumulate surface normals
	while(shadercount--)
	{
		d = *renderlist++;
		while (d--)
		{
			a = renderlist[0]*3;
			b = renderlist[1]*3;
			c = renderlist[2]*3;
			v1[0] = aliasvert[a+0] - aliasvert[b+0];
			v1[1] = aliasvert[a+1] - aliasvert[b+1];
			v1[2] = aliasvert[a+2] - aliasvert[b+2];
			v2[0] = aliasvert[c+0] - aliasvert[b+0];
			v2[1] = aliasvert[c+1] - aliasvert[b+1];
			v2[2] = aliasvert[c+2] - aliasvert[b+2];
			CrossProduct(v1, v2, normal);
			VectorNormalize(normal);
			// add surface normal to vertices
			aliasvertnorm[a+0] += normal[0];
			aliasvertnorm[a+1] += normal[1];
			aliasvertnorm[a+2] += normal[2];
			aliasvertusage[a]++;
			aliasvertnorm[b+0] += normal[0];
			aliasvertnorm[b+1] += normal[1];
			aliasvertnorm[b+2] += normal[2];
			aliasvertusage[b]++;
			aliasvertnorm[c+0] += normal[0];
			aliasvertnorm[c+1] += normal[1];
			aliasvertnorm[c+2] += normal[2];
			aliasvertusage[c]++;
			renderlist += 3;
		}
	}
	// average surface normals
	out = aliasvertnorm;
	u = aliasvertusage;
	while(vertcount--)
	{
		if (*u > 1)
		{
			a = ixtable[*u];
			out[0] *= a;
			out[1] *= a;
			out[2] *= a;
		}
		u++;
		out += 3;
	}
}

void GL_DrawZymoticModelMesh(byte *colors, zymtype1header_t *m)
{
	int i, c, *renderlist, *texturenum;
	if (!r_render.value)
		return;
	renderlist = (int *)(m->lump_render.start + (int) m);
	texturenum = (int *)(m->lump_shaders.start + (int) m);
	if (gl_vertexarrays.value)
	{
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		glEnableClientState(GL_VERTEX_ARRAY);

		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
		glEnableClientState(GL_COLOR_ARRAY);

		qglTexCoordPointer(2, GL_FLOAT, 0, (float *)(m->lump_texcoords.start + (int) m));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		for (i = 0;i < m->numshaders;i++)
		{
			c = (*renderlist++) * 3;
			glBindTexture(GL_TEXTURE_2D, *texturenum++);
			qglDrawElements(GL_TRIANGLES, c, GL_UNSIGNED_INT, renderlist);
			renderlist += c;
		}

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glDisableClientState(GL_COLOR_ARRAY);

		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		int index;
		float *tex;
		tex = (float *)(m->lump_texcoords.start + (int) m);

		for (i = 0;i < m->numshaders;i++)
		{
			c = *renderlist++;
			glBindTexture(GL_TEXTURE_2D, *texturenum++);
			glBegin(GL_TRIANGLES);
			while (c--)
			{
				index = *renderlist++;
				glTexCoord2fv(tex + index*2);
				glColor4ubv(colors + index*4);
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glTexCoord2fv(tex + index*2);
				glColor4ubv(colors + index*4);
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glTexCoord2fv(tex + index*2);
				glColor4ubv(colors + index*4);
				glVertex3fv(aliasvert + index*3);
			}
			glEnd();
		}
	}
}

void GL_DrawZymoticModelMeshFog(vec3_t org, zymtype1header_t *m)
{
	vec3_t diff;
	int i, c, *renderlist;
	if (!r_render.value)
		return;
	renderlist = (int *)(m->lump_render.start + (int) m);
	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(0); // disable zbuffer updates

	VectorSubtract(org, r_refdef.vieworg, diff);
	glColor4f(fogcolor[0], fogcolor[1], fogcolor[2], exp(fogdensity/DotProduct(diff,diff)));
	if (gl_vertexarrays.value)
	{
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		glEnableClientState(GL_VERTEX_ARRAY);

		for (i = 0;i < m->numshaders;i++)
		{
			c = (*renderlist++) * 3;
			qglDrawElements(GL_TRIANGLES, c, GL_UNSIGNED_INT, renderlist);
			renderlist += c;
		}

		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		int index;
		float *tex;
		tex = (float *)(m->lump_texcoords.start + (int) m);

		glBegin(GL_TRIANGLES);
		for (i = 0;i < m->numshaders;i++)
		{
			c = *renderlist++;
			while (c--)
			{
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
			}
		}
		glEnd();
	}
	glEnable(GL_TEXTURE_2D);
	glColor3f (1,1,1);
}

void GL_DrawZymoticModelMeshShadow(zymtype1header_t *m)
{
	int i, c, *renderlist;
	float *av, l;
	if (!r_render.value)
		return;

	// flatten it to make a shadow
	av = aliasvert + 2;
	l = lightspot[2] + 0.125;
	for (i = 0;i < m->numverts;i++, av+=3)
		if (*av > l)
			*av = l;

	renderlist = (int *)(m->lump_render.start + (int) m);
	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(0); // disable zbuffer updates

	glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
	if (gl_vertexarrays.value)
	{
		qglVertexPointer(3, GL_FLOAT, 0, aliasvert);
		glEnableClientState(GL_VERTEX_ARRAY);

		for (i = 0;i < m->numshaders;i++)
		{
			c = (*renderlist++) * 3;
			qglDrawElements(GL_TRIANGLES, c, GL_UNSIGNED_INT, renderlist);
			renderlist += c;
		}

		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		int index;
		float *tex;
		tex = (float *)(m->lump_texcoords.start + (int) m);

		glBegin(GL_TRIANGLES);
		for (i = 0;i < m->numshaders;i++)
		{
			c = *renderlist++;
			while (c--)
			{
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
				index = *renderlist++;
				glVertex3fv(aliasvert + index*3);
			}
		}
		glEnd();
	}
	glEnable(GL_TEXTURE_2D);
	glColor3f (1,1,1);
}

/*
=================
R_DrawZymoticFrame
=================
*/
void R_DrawZymoticFrame (zymtype1header_t *m, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, int frame, int skinblah, int effects, int flags)
{
	zymscene_t *scene;
	float scenetime, scenefrac;
	int sceneframe1, sceneframe2;
	zymbonematrix *basebonepose;
	if ((frame >= m->numscenes) || (frame < 0))
	{
		Con_DPrintf ("R_ZymoticSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	scene = (zymscene_t *)(m->lump_scenes.start + (int) m) + frame;
	if (ent->draw_lastmodel != ent->model || ent->draw_pose != frame || ent->draw_lerpstart >= cl.time)
	{
		ent->draw_lastmodel = ent->model;
		ent->draw_lastpose = -1;
		ent->draw_pose = frame;
		ent->draw_lerpstart = cl.time;
	}
	scenetime = (cl.time - ent->draw_lerpstart) * scene->framerate;
	sceneframe1 = (int) scenetime;
	sceneframe2 = sceneframe1 + 1;
	scenefrac = scenetime - sceneframe1;
	if (scene->flags & ZYMSCENEFLAG_NOLOOP)
	{
		if (sceneframe1 > (scene->length - 1))
			sceneframe1 = (scene->length - 1);
		if (sceneframe2 > (scene->length - 1))
			sceneframe2 = (scene->length - 1);
	}
	else
	{
		sceneframe1 %= scene->length;
		sceneframe2 %= scene->length;
	}
	if (sceneframe2 == sceneframe1)
		scenefrac = 0;

	basebonepose = (zymbonematrix *)(m->lump_poses.start + (int) m);
	ZymoticLerpBones(m->numbones, scenefrac, basebonepose + sceneframe1 * m->numbones, basebonepose + sceneframe2 * m->numbones, (zymbone_t *)(m->lump_bones.start + (int) m), org, angles);
	ZymoticTransformVerts(m->numverts, (int *)(m->lump_vertbonecounts.start + (int) m), (zymvertex_t *)(m->lump_verts.start + (int) m));
	ZymoticCalcNormals(m->numverts, m->numshaders, (int *)(m->lump_render.start + (int) m));

	R_LightModel(m->numverts, org, color);

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

	GL_DrawZymoticModelMesh(aliasvertcolor, m);

	if (fogenabled)
		GL_DrawZymoticModelMeshFog(org, m);

	if (!fogenabled && r_shadows.value && !(effects & EF_ADDITIVE) && shadow)
		GL_DrawZymoticModelMeshShadow(m);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

int modeldlightbits[8];
extern int r_dlightframecount;

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *ent, int cull, float alpha, model_t *clmodel, int frame, int skin, vec3_t org, vec3_t angles, int effects, int flags, int colormap)
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

	c_models++;

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
		R_LightPoint (color, org);

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
	if (clmodel->aliastype == ALIASTYPE_ZYM)
		R_DrawZymoticFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, frame, 0, effects, flags);
	else if (clmodel->aliastype == ALIASTYPE_MD2)
		R_DrawQ2AliasFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, frame, skinset[0], effects, flags);
	else
		R_DrawAliasFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, frame, skinset, colormap, effects, flags);
}
