
#include "quakedef.h"

cvar_t gl_transform = {"gl_transform", "1"};
cvar_t gl_lockarrays = {"gl_lockarrays", "1"};

typedef struct
{
	float m[3][4];
} zymbonematrix;

// LordHavoc: vertex array
float *aliasvert;
float *aliasvertnorm;
byte *aliasvertcolor;
byte *aliasvertcolor2;
zymbonematrix *zymbonepose;
int *aliasvertusage;

rtexture_t *chrometexture;

int arraylocked = false;
void GL_LockArray(int first, int count)
{
	if (gl_supportslockarrays && gl_lockarrays.value)
	{
		qglLockArraysEXT(first, count);
		arraylocked = true;
	}
}

void GL_UnlockArray()
{
	if (arraylocked)
	{
		qglUnlockArraysEXT();
		arraylocked = false;
	}
}

void GL_SetupModelTransform (vec3_t origin, vec3_t angles, vec_t scale)
{
    glTranslatef (origin[0], origin[1], origin[2]);

	if (scale != 1)
		glScalef (scale, scale, scale);
	if (angles[1])
	    glRotatef (angles[1],  0, 0, 1);
	if (angles[0])
	    glRotatef (-angles[0],  0, 1, 0);
	if (angles[2])
	    glRotatef (angles[2],  1, 0, 0);
}

void makechrometexture()
{
	int i;
	byte noise[64*64];
	byte data[64*64][4];

	fractalnoise(noise, 64, 8);

	// convert to RGBA data
	for (i = 0;i < 64*64;i++)
	{
		data[i][0] = data[i][1] = data[i][2] = noise[i];
		data[i][3] = 255;
	}

	chrometexture = R_LoadTexture ("chrometexture", 64, 64, &data[0][0], TEXF_MIPMAP | TEXF_RGBA | TEXF_PRECACHE);
}

void gl_models_start()
{
	// allocate vertex processing arrays
	aliasvert = qmalloc(sizeof(float[MD2MAX_VERTS][3]));
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

void gl_models_newmap()
{
}

void GL_Models_Init()
{
	Cvar_RegisterVariable(&gl_transform);
	Cvar_RegisterVariable(&gl_lockarrays);

	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

extern vec3_t softwaretransform_x;
extern vec3_t softwaretransform_y;
extern vec3_t softwaretransform_z;
extern vec_t softwaretransform_scale;
extern vec3_t softwaretransform_offset;
void R_AliasTransformVerts(int vertcount)
{
	int i;
	vec3_t point, matrix_x, matrix_y, matrix_z;
	float *av, *avn;
	av = aliasvert;
	avn = aliasvertnorm;
	matrix_x[0] = softwaretransform_x[0] * softwaretransform_scale;
	matrix_x[1] = softwaretransform_y[0] * softwaretransform_scale;
	matrix_x[2] = softwaretransform_z[0] * softwaretransform_scale;
	matrix_y[0] = softwaretransform_x[1] * softwaretransform_scale;
	matrix_y[1] = softwaretransform_y[1] * softwaretransform_scale;
	matrix_y[2] = softwaretransform_z[1] * softwaretransform_scale;
	matrix_z[0] = softwaretransform_x[2] * softwaretransform_scale;
	matrix_z[1] = softwaretransform_y[2] * softwaretransform_scale;
	matrix_z[2] = softwaretransform_z[2] * softwaretransform_scale;
	for (i = 0;i < vertcount;i++)
	{
		// rotate, scale, and translate the vertex locations
		VectorCopy(av, point);
		av[0] = DotProduct(point, matrix_x) + softwaretransform_offset[0];
		av[1] = DotProduct(point, matrix_y) + softwaretransform_offset[1];
		av[2] = DotProduct(point, matrix_z) + softwaretransform_offset[2];
		// rotate the normals
		VectorCopy(avn, point);
		avn[0] = point[0] * softwaretransform_x[0] + point[1] * softwaretransform_y[0] + point[2] * softwaretransform_z[0];
		avn[1] = point[0] * softwaretransform_x[1] + point[1] * softwaretransform_y[1] + point[2] * softwaretransform_z[1];
		avn[2] = point[0] * softwaretransform_x[2] + point[1] * softwaretransform_y[2] + point[2] * softwaretransform_z[2];
		av += 3;
		avn += 3;
	}
}

void R_AliasLerpVerts(int vertcount,
					  float lerp1, trivertx_t *verts1, vec3_t fscale1, vec3_t translate1,
					  float lerp2, trivertx_t *verts2, vec3_t fscale2, vec3_t translate2,
					  float lerp3, trivertx_t *verts3, vec3_t fscale3, vec3_t translate3,
					  float lerp4, trivertx_t *verts4, vec3_t fscale4, vec3_t translate4)
{
	int i;
	vec3_t scale1, scale2, scale3, scale4, translate;
	float *n1, *n2, *n3, *n4;
	float *av, *avn;
	av = aliasvert;
	avn = aliasvertnorm;
	VectorScaleQuick(fscale1, lerp1, scale1);
	if (lerp2)
	{
		VectorScaleQuick(fscale2, lerp2, scale2);
		if (lerp3)
		{
			VectorScaleQuick(fscale3, lerp3, scale3);
			if (lerp4)
			{
				VectorScaleQuick(fscale4, lerp4, scale4);
				translate[0] = translate1[0] * lerp1 + translate2[0] * lerp2 + translate3[0] * lerp3 + translate4[0] * lerp4;
				translate[1] = translate1[1] * lerp1 + translate2[1] * lerp2 + translate3[1] * lerp3 + translate4[1] * lerp4;
				translate[2] = translate1[2] * lerp1 + translate2[2] * lerp2 + translate3[2] * lerp3 + translate4[2] * lerp4;
				// generate vertices
				for (i = 0;i < vertcount;i++)
				{
					av[0] = verts1->v[0] * scale1[0] + verts2->v[0] * scale2[0] + verts3->v[0] * scale3[0] + verts4->v[0] * scale4[0] + translate[0];
					av[1] = verts1->v[1] * scale1[1] + verts2->v[1] * scale2[1] + verts3->v[1] * scale3[1] + verts4->v[1] * scale4[1] + translate[1];
					av[2] = verts1->v[2] * scale1[2] + verts2->v[2] * scale2[2] + verts3->v[2] * scale3[2] + verts4->v[2] * scale4[2] + translate[2];
					n1 = m_bytenormals[verts1->lightnormalindex];
					n2 = m_bytenormals[verts2->lightnormalindex];
					n3 = m_bytenormals[verts3->lightnormalindex];
					n4 = m_bytenormals[verts4->lightnormalindex];
					avn[0] = n1[0] * lerp1 + n2[0] * lerp2 + n3[0] * lerp3 + n4[0] * lerp4;
					avn[1] = n1[1] * lerp1 + n2[1] * lerp2 + n3[1] * lerp3 + n4[1] * lerp4;
					avn[2] = n1[2] * lerp1 + n2[2] * lerp2 + n3[2] * lerp3 + n4[2] * lerp4;
					av += 3;
					avn += 3;
					verts1++;verts2++;verts3++;verts4++;
				}
			}
			else
			{
				translate[0] = translate1[0] * lerp1 + translate2[0] * lerp2 + translate3[0] * lerp3;
				translate[1] = translate1[1] * lerp1 + translate2[1] * lerp2 + translate3[1] * lerp3;
				translate[2] = translate1[2] * lerp1 + translate2[2] * lerp2 + translate3[2] * lerp3;
				// generate vertices
				for (i = 0;i < vertcount;i++)
				{
					av[0] = verts1->v[0] * scale1[0] + verts2->v[0] * scale2[0] + verts3->v[0] * scale3[0] + translate[0];
					av[1] = verts1->v[1] * scale1[1] + verts2->v[1] * scale2[1] + verts3->v[1] * scale3[1] + translate[1];
					av[2] = verts1->v[2] * scale1[2] + verts2->v[2] * scale2[2] + verts3->v[2] * scale3[2] + translate[2];
					n1 = m_bytenormals[verts1->lightnormalindex];
					n2 = m_bytenormals[verts2->lightnormalindex];
					n3 = m_bytenormals[verts3->lightnormalindex];
					avn[0] = n1[0] * lerp1 + n2[0] * lerp2 + n3[0] * lerp3;
					avn[1] = n1[1] * lerp1 + n2[1] * lerp2 + n3[1] * lerp3;
					avn[2] = n1[2] * lerp1 + n2[2] * lerp2 + n3[2] * lerp3;
					av += 3;
					avn += 3;
					verts1++;verts2++;verts3++;
				}
			}
		}
		else
		{
			translate[0] = translate1[0] * lerp1 + translate2[0] * lerp2;
			translate[1] = translate1[1] * lerp1 + translate2[1] * lerp2;
			translate[2] = translate1[2] * lerp1 + translate2[2] * lerp2;
			// generate vertices
			for (i = 0;i < vertcount;i++)
			{
				av[0] = verts1->v[0] * scale1[0] + verts2->v[0] * scale2[0] + translate[0];
				av[1] = verts1->v[1] * scale1[1] + verts2->v[1] * scale2[1] + translate[1];
				av[2] = verts1->v[2] * scale1[2] + verts2->v[2] * scale2[2] + translate[2];
				n1 = m_bytenormals[verts1->lightnormalindex];
				n2 = m_bytenormals[verts2->lightnormalindex];
				avn[0] = n1[0] * lerp1 + n2[0] * lerp2;
				avn[1] = n1[1] * lerp1 + n2[1] * lerp2;
				avn[2] = n1[2] * lerp1 + n2[2] * lerp2;
				av += 3;
				avn += 3;
				verts1++;verts2++;
			}
		}
	}
	else
	{
		translate[0] = translate1[0] * lerp1;
		translate[1] = translate1[1] * lerp1;
		translate[2] = translate1[2] * lerp1;
		// generate vertices
		if (lerp1 != 1)
		{
			// general but almost never used case
			for (i = 0;i < vertcount;i++)
			{
				av[0] = verts1->v[0] * scale1[0] + translate[0];
				av[1] = verts1->v[1] * scale1[1] + translate[1];
				av[2] = verts1->v[2] * scale1[2] + translate[2];
				n1 = m_bytenormals[verts1->lightnormalindex];
				avn[0] = n1[0] * lerp1;
				avn[1] = n1[1] * lerp1;
				avn[2] = n1[2] * lerp1;
				av += 3;
				avn += 3;
				verts1++;
			}
		}
		else
		{
			// fast normal case
			for (i = 0;i < vertcount;i++)
			{
				av[0] = verts1->v[0] * scale1[0] + translate[0];
				av[1] = verts1->v[1] * scale1[1] + translate[1];
				av[2] = verts1->v[2] * scale1[2] + translate[2];
				VectorCopy(m_bytenormals[verts1->lightnormalindex], avn);
				av += 3;
				avn += 3;
				verts1++;
			}
		}
	}
}

void GL_DrawModelMesh(rtexture_t *skin, byte *colors, maliashdr_t *maliashdr)
{
	if (!r_render.value)
		return;
	glBindTexture(GL_TEXTURE_2D, R_GetTexture(skin));
	if (!colors)
	{
		if (lighthalf)
			glColor3f(0.5f, 0.5f, 0.5f);
		else
			glColor3f(1.0f, 1.0f, 1.0f);
	}
	if (colors)
	{
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
		glEnableClientState(GL_COLOR_ARRAY);
	}

	glDrawElements(GL_TRIANGLES, maliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) maliashdr + maliashdr->tridata));

	if (colors)
		glDisableClientState(GL_COLOR_ARRAY);
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
void R_LightModel(entity_t *ent, int numverts, vec3_t center, vec3_t basecolor);
void R_DrawAliasFrame (maliashdr_t *maliashdr, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, vec_t scale, frameblend_t *blend, rtexture_t **skin, int colormap, int effects, int flags)
{
	if (gl_transform.value)
	{
		if (r_render.value)
		{
			glPushMatrix();
			GL_SetupModelTransform(org, angles, scale);
		}
	}
	// always needed, for model lighting
	softwaretransformforentity(ent);

	R_AliasLerpVerts(maliashdr->numverts,
		blend[0].lerp, ((trivertx_t *)((int) maliashdr + maliashdr->posedata)) + blend[0].frame * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin,
		blend[1].lerp, ((trivertx_t *)((int) maliashdr + maliashdr->posedata)) + blend[1].frame * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin,
		blend[2].lerp, ((trivertx_t *)((int) maliashdr + maliashdr->posedata)) + blend[2].frame * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin,
		blend[3].lerp, ((trivertx_t *)((int) maliashdr + maliashdr->posedata)) + blend[3].frame * maliashdr->numverts, maliashdr->scale, maliashdr->scale_origin);
	if (!gl_transform.value)
		R_AliasTransformVerts(maliashdr->numverts);

	// prep the vertex array as early as possible
	if (r_render.value)
	{
		glVertexPointer(3, GL_FLOAT, 0, aliasvert);
		glEnableClientState(GL_VERTEX_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, (void *)((int) maliashdr->texdata + (int) maliashdr));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		GL_LockArray(0, maliashdr->numverts);
	}

	R_LightModel(ent, maliashdr->numverts, org, color);

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

		glDrawElements(GL_TRIANGLES, maliashdr->numtris * 3, GL_UNSIGNED_SHORT, (void *)((int) maliashdr + maliashdr->tridata));

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}
	GL_UnlockArray();
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);

	glPopMatrix();
}

/*
=================
R_DrawQ2AliasFrame

=================
*/
void R_DrawQ2AliasFrame (md2mem_t *pheader, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, vec_t scale, frameblend_t *blend, rtexture_t *skin, int effects, int flags)
{
	int *order, count;
	md2frame_t *frame1, *frame2, *frame3, *frame4;

	if (r_render.value)
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(skin));

	if (gl_transform.value)
	{
		if (r_render.value)
		{
			glPushMatrix();
			GL_SetupModelTransform(org, angles, scale);
		}
	}
	// always needed, for model lighting
	softwaretransformforentity(ent);

	frame1 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * blend[0].frame));
	frame2 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * blend[1].frame));
	frame3 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * blend[2].frame));
	frame4 = (void *)((int) pheader + pheader->ofs_frames + (pheader->framesize * blend[3].frame));
	R_AliasLerpVerts(pheader->num_xyz,
		blend[0].lerp, frame1->verts, frame1->scale, frame1->translate,
		blend[1].lerp, frame2->verts, frame2->scale, frame2->translate,
		blend[2].lerp, frame3->verts, frame3->scale, frame3->translate,
		blend[3].lerp, frame4->verts, frame4->scale, frame4->translate);
	if (!gl_transform.value)
		R_AliasTransformVerts(pheader->num_xyz);

	R_LightModel(ent, pheader->num_xyz, org, color);

	if (!r_render.value)
		return;
	// LordHavoc: big mess...
	// using vertex arrays only slightly, although it is enough to prevent duplicates
	// (saving half the transforms)
	glVertexPointer(3, GL_FLOAT, 0, aliasvert);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, aliasvertcolor);
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
			glArrayElement(order[2]);
			order += 3;
		}
		while (count--);
	}

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

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

		// LordHavoc: big mess...
		// using vertex arrays only slightly, although it is enough to prevent duplicates
		// (saving half the transforms)
		glVertexPointer(3, GL_FLOAT, 0, aliasvert);
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
				glArrayElement(order[2]);
				order += 3;
			}
			while (count--);
		}

		glDisableClientState(GL_VERTEX_ARRAY);

		glEnable (GL_TEXTURE_2D);
		glColor3f (1,1,1);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);

	if (gl_transform.value)
		glPopMatrix();
}

void ZymoticLerpBones(int count, zymbonematrix *bonebase, frameblend_t *blend, zymbone_t *bone, float rootorigin[3], float rootangles[3], float rootscale)
{
	float lerp1, lerp2, lerp3, lerp4;
	zymbonematrix *out, rootmatrix, m, *bone1, *bone2, *bone3, *bone4;
	lerp1 = 1 - lerp2;
	out = zymbonepose;
	AngleVectors(rootangles, rootmatrix.m[0], rootmatrix.m[1], rootmatrix.m[2]);
	VectorScale(rootmatrix.m[0], rootscale, rootmatrix.m[0]);
	VectorScale(rootmatrix.m[1], rootscale, rootmatrix.m[1]);
	VectorScale(rootmatrix.m[2], rootscale, rootmatrix.m[2]);
	rootmatrix.m[0][3] = rootorigin[0];
	rootmatrix.m[1][3] = rootorigin[1];
	rootmatrix.m[2][3] = rootorigin[2];
	bone1 = bonebase + blend[0].frame * count;
	lerp1 = blend[0].lerp;
	if (blend[1].lerp)
	{
		bone2 = bonebase + blend[1].frame * count;
		lerp2 = blend[1].lerp;
		if (blend[2].lerp)
		{
			bone3 = bonebase + blend[2].frame * count;
			lerp3 = blend[2].lerp;
			if (blend[3].lerp)
			{
				// 4 poses
				bone4 = bonebase + blend[3].frame * count;
				lerp4 = blend[3].lerp;
				while(count--)
				{
					// interpolate matrices
					m.m[0][0] = bone1->m[0][0] * lerp1 + bone2->m[0][0] * lerp2 + bone3->m[0][0] * lerp3 + bone4->m[0][0] * lerp4;
					m.m[0][1] = bone1->m[0][1] * lerp1 + bone2->m[0][1] * lerp2 + bone3->m[0][1] * lerp3 + bone4->m[0][1] * lerp4;
					m.m[0][2] = bone1->m[0][2] * lerp1 + bone2->m[0][2] * lerp2 + bone3->m[0][2] * lerp3 + bone4->m[0][2] * lerp4;
					m.m[0][3] = bone1->m[0][3] * lerp1 + bone2->m[0][3] * lerp2 + bone3->m[0][3] * lerp3 + bone4->m[0][3] * lerp4;
					m.m[1][0] = bone1->m[1][0] * lerp1 + bone2->m[1][0] * lerp2 + bone3->m[1][0] * lerp3 + bone4->m[1][0] * lerp4;
					m.m[1][1] = bone1->m[1][1] * lerp1 + bone2->m[1][1] * lerp2 + bone3->m[1][1] * lerp3 + bone4->m[1][1] * lerp4;
					m.m[1][2] = bone1->m[1][2] * lerp1 + bone2->m[1][2] * lerp2 + bone3->m[1][2] * lerp3 + bone4->m[1][2] * lerp4;
					m.m[1][3] = bone1->m[1][3] * lerp1 + bone2->m[1][3] * lerp2 + bone3->m[1][3] * lerp3 + bone4->m[1][3] * lerp4;
					m.m[2][0] = bone1->m[2][0] * lerp1 + bone2->m[2][0] * lerp2 + bone3->m[2][0] * lerp3 + bone4->m[2][0] * lerp4;
					m.m[2][1] = bone1->m[2][1] * lerp1 + bone2->m[2][1] * lerp2 + bone3->m[2][1] * lerp3 + bone4->m[2][1] * lerp4;
					m.m[2][2] = bone1->m[2][2] * lerp1 + bone2->m[2][2] * lerp2 + bone3->m[2][2] * lerp3 + bone4->m[2][2] * lerp4;
					m.m[2][3] = bone1->m[2][3] * lerp1 + bone2->m[2][3] * lerp2 + bone3->m[2][3] * lerp3 + bone4->m[2][3] * lerp4;
					if (bone->parent >= 0)
						R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &m.m[0], &out->m[0]);
					else
						R_ConcatTransforms(&rootmatrix.m[0], &m.m[0], &out->m[0]);
					bone1++;
					bone2++;
					bone3++;
					bone4++;
					bone++;
					out++;
				}
			}
			else
			{
				// 3 poses
				while(count--)
				{
					// interpolate matrices
					m.m[0][0] = bone1->m[0][0] * lerp1 + bone2->m[0][0] * lerp2 + bone3->m[0][0] * lerp3;
					m.m[0][1] = bone1->m[0][1] * lerp1 + bone2->m[0][1] * lerp2 + bone3->m[0][1] * lerp3;
					m.m[0][2] = bone1->m[0][2] * lerp1 + bone2->m[0][2] * lerp2 + bone3->m[0][2] * lerp3;
					m.m[0][3] = bone1->m[0][3] * lerp1 + bone2->m[0][3] * lerp2 + bone3->m[0][3] * lerp3;
					m.m[1][0] = bone1->m[1][0] * lerp1 + bone2->m[1][0] * lerp2 + bone3->m[1][0] * lerp3;
					m.m[1][1] = bone1->m[1][1] * lerp1 + bone2->m[1][1] * lerp2 + bone3->m[1][1] * lerp3;
					m.m[1][2] = bone1->m[1][2] * lerp1 + bone2->m[1][2] * lerp2 + bone3->m[1][2] * lerp3;
					m.m[1][3] = bone1->m[1][3] * lerp1 + bone2->m[1][3] * lerp2 + bone3->m[1][3] * lerp3;
					m.m[2][0] = bone1->m[2][0] * lerp1 + bone2->m[2][0] * lerp2 + bone3->m[2][0] * lerp3;
					m.m[2][1] = bone1->m[2][1] * lerp1 + bone2->m[2][1] * lerp2 + bone3->m[2][1] * lerp3;
					m.m[2][2] = bone1->m[2][2] * lerp1 + bone2->m[2][2] * lerp2 + bone3->m[2][2] * lerp3;
					m.m[2][3] = bone1->m[2][3] * lerp1 + bone2->m[2][3] * lerp2 + bone3->m[2][3] * lerp3;
					if (bone->parent >= 0)
						R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &m.m[0], &out->m[0]);
					else
						R_ConcatTransforms(&rootmatrix.m[0], &m.m[0], &out->m[0]);
					bone1++;
					bone2++;
					bone3++;
					bone++;
					out++;
				}
			}
		}
		else
		{
			// 2 poses
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
	}
	else
	{
		// 1 pose
		if (lerp1 != 1)
		{
			// lerp != 1.0
			while(count--)
			{
				// interpolate matrices
				m.m[0][0] = bone1->m[0][0] * lerp1;
				m.m[0][1] = bone1->m[0][1] * lerp1;
				m.m[0][2] = bone1->m[0][2] * lerp1;
				m.m[0][3] = bone1->m[0][3] * lerp1;
				m.m[1][0] = bone1->m[1][0] * lerp1;
				m.m[1][1] = bone1->m[1][1] * lerp1;
				m.m[1][2] = bone1->m[1][2] * lerp1;
				m.m[1][3] = bone1->m[1][3] * lerp1;
				m.m[2][0] = bone1->m[2][0] * lerp1;
				m.m[2][1] = bone1->m[2][1] * lerp1;
				m.m[2][2] = bone1->m[2][2] * lerp1;
				m.m[2][3] = bone1->m[2][3] * lerp1;
				if (bone->parent >= 0)
					R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &m.m[0], &out->m[0]);
				else
					R_ConcatTransforms(&rootmatrix.m[0], &m.m[0], &out->m[0]);
				bone1++;
				bone++;
				out++;
			}
		}
		else
		{
			// lerp == 1.0
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
	int i, c, *renderlist;
	rtexture_t **texture;
	if (!r_render.value)
		return;
	renderlist = (int *)(m->lump_render.start + (int) m);
	texture = (rtexture_t **)(m->lump_shaders.start + (int) m);
	glVertexPointer(3, GL_FLOAT, 0, aliasvert);
	glEnableClientState(GL_VERTEX_ARRAY);

	glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
	glEnableClientState(GL_COLOR_ARRAY);

	glTexCoordPointer(2, GL_FLOAT, 0, (float *)(m->lump_texcoords.start + (int) m));
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	for (i = 0;i < m->numshaders;i++)
	{
		c = (*renderlist++) * 3;
		glBindTexture(GL_TEXTURE_2D, R_GetTexture(*texture));
		texture++;
		glDrawElements(GL_TRIANGLES, c, GL_UNSIGNED_INT, renderlist);
		renderlist += c;
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisableClientState(GL_COLOR_ARRAY);

	glDisableClientState(GL_VERTEX_ARRAY);
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

	glVertexPointer(3, GL_FLOAT, 0, aliasvert);
	glEnableClientState(GL_VERTEX_ARRAY);

	for (i = 0;i < m->numshaders;i++)
	{
		c = (*renderlist++) * 3;
		glDrawElements(GL_TRIANGLES, c, GL_UNSIGNED_INT, renderlist);
		renderlist += c;
	}

	glDisableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glColor3f (1,1,1);
}

/*
=================
R_DrawZymoticFrame
=================
*/
void R_DrawZymoticFrame (zymtype1header_t *m, float alpha, vec3_t color, entity_t *ent, int shadow, vec3_t org, vec3_t angles, vec_t scale, frameblend_t *blend, int skinblah, int effects, int flags)
{
	ZymoticLerpBones(m->numbones, (zymbonematrix *)(m->lump_poses.start + (int) m), blend, (zymbone_t *)(m->lump_bones.start + (int) m), org, angles, scale);
	ZymoticTransformVerts(m->numverts, (int *)(m->lump_vertbonecounts.start + (int) m), (zymvertex_t *)(m->lump_verts.start + (int) m));
	ZymoticCalcNormals(m->numverts, m->numshaders, (int *)(m->lump_render.start + (int) m));

	R_LightModel(ent, m->numverts, org, color);

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

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	glDepthMask(1);
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *ent, int cull, float alpha, model_t *clmodel, frameblend_t *blend, int skin, vec3_t org, vec3_t angles, vec_t scale, int effects, int flags, int colormap)
{
	int			i;
	vec3_t		mins, maxs, color;
	void		*modelheader;
	rtexture_t	**skinset;

	if (alpha < (1.0 / 64.0))
		return; // basically completely transparent

	VectorAdd (org, clmodel->mins, mins);
	VectorAdd (org, clmodel->maxs, maxs);

	if (cull && R_CullBox (mins, maxs))
		return;

	c_models++;

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
		rtexture_t **skinanim = clmodel->skinanim;
		i = skinanimrange[0];
		if (skinanimrange[1] > 1) // animated
			i += ((int) (cl.time * 10) % skinanimrange[1]);
		skinset = skinanim + i*5;
	}

	if (r_render.value)
		glEnable (GL_TEXTURE_2D);

	c_alias_polys += clmodel->numtris;
	if (clmodel->aliastype == ALIASTYPE_ZYM)
		R_DrawZymoticFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, scale, blend, 0                   , effects, flags);
	else if (clmodel->aliastype == ALIASTYPE_MD2)
		R_DrawQ2AliasFrame (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, scale, blend, skinset[0]          , effects, flags);
	else
		R_DrawAliasFrame   (modelheader, alpha, color, ent, ent != &cl.viewent, org, angles, scale, blend, skinset   , colormap, effects, flags);
}
