
#include "quakedef.h"

//cvar_t gl_transform = {0, "gl_transform", "1"};
cvar_t r_quickmodels = {0, "r_quickmodels", "1"};

typedef struct
{
	float m[3][4];
} zymbonematrix;

// LordHavoc: vertex arrays

float *aliasvertbuf;
float *aliasvertcolorbuf;
float *aliasvert; // this may point at aliasvertbuf or at vertex arrays in the mesh backend
float *aliasvertcolor; // this may point at aliasvertcolorbuf or at vertex arrays in the mesh backend

float *aliasvertcolor2;
float *aliasvertnorm;
int *aliasvertusage;
zymbonematrix *zymbonepose;

rmeshinfo_t aliasmeshinfo;

/*
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
*/

/*
rtexturepool_t *chrometexturepool;
rtexture_t *chrometexture;

// currently unused reflection effect texture
void makechrometexture(void)
{
	int i;
	qbyte noise[64*64];
	qbyte data[64*64][4];

	fractalnoise(noise, 64, 8);

	// convert to RGBA data
	for (i = 0;i < 64*64;i++)
	{
		data[i][0] = data[i][1] = data[i][2] = noise[i];
		data[i][3] = 255;
	}

	chrometexture = R_LoadTexture (chrometexturepool, "chrometexture", 64, 64, &data[0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_PRECACHE);
}
*/

mempool_t *gl_models_mempool;

void gl_models_start(void)
{
	// allocate vertex processing arrays
	gl_models_mempool = Mem_AllocPool("GL_Models");
	aliasvert = aliasvertbuf = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
	aliasvertcolor = aliasvertcolorbuf = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
	aliasvertnorm = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][3]));
	aliasvertcolor2 = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4])); // used temporarily for tinted coloring
	zymbonepose = Mem_Alloc(gl_models_mempool, sizeof(zymbonematrix[256]));
	aliasvertusage = Mem_Alloc(gl_models_mempool, sizeof(int[MD2MAX_VERTS]));
	//chrometexturepool = R_AllocTexturePool();
	//makechrometexture();
}

void gl_models_shutdown(void)
{
	//R_FreeTexturePool(&chrometexturepool);
	Mem_FreePool(&gl_models_mempool);
}

void gl_models_newmap(void)
{
}

void GL_Models_Init(void)
{
//	Cvar_RegisterVariable(&gl_transform);
	Cvar_RegisterVariable(&r_quickmodels);

	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

void R_AliasTransformVerts(int vertcount)
{
	vec3_t point;
	float *av;
//	float *avn;
	av = aliasvert;
//	avn = aliasvertnorm;
	while (vertcount >= 4)
	{
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
//		VectorCopy(avn, point);softwaretransformdirection(point, avn);avn += 3;
//		VectorCopy(avn, point);softwaretransformdirection(point, avn);avn += 3;
//		VectorCopy(avn, point);softwaretransformdirection(point, avn);avn += 3;
//		VectorCopy(avn, point);softwaretransformdirection(point, avn);avn += 3;
		vertcount -= 4;
	}
	while(vertcount > 0)
	{
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
//		VectorCopy(avn, point);softwaretransformdirection(point, avn);avn += 3;
		vertcount--;
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
	VectorScale(fscale1, lerp1, scale1);
	if (lerp2)
	{
		VectorScale(fscale2, lerp2, scale2);
		if (lerp3)
		{
			VectorScale(fscale3, lerp3, scale3);
			if (lerp4)
			{
				VectorScale(fscale4, lerp4, scale4);
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
					av += 4;
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
					av += 4;
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
				av += 4;
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
				av += 4;
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
				av += 4;
				avn += 3;
				verts1++;
			}
		}
	}
}

void R_DrawModelMesh(rtexture_t *skin, float *colors, float cred, float cgreen, float cblue)
{
	aliasmeshinfo.tex[0] = R_GetTexture(skin);
	aliasmeshinfo.color = colors;
	if (colors == NULL)
	{
		aliasmeshinfo.cr = cred;
		aliasmeshinfo.cg = cgreen;
		aliasmeshinfo.cb = cblue;
		aliasmeshinfo.ca = currentrenderentity->alpha;
	}

	c_alias_polys += aliasmeshinfo.numtriangles;
	R_Mesh_Draw(&aliasmeshinfo);

	// leave it in a state for additional passes
	aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
	aliasmeshinfo.blendfunc2 = GL_ONE;
}

void R_TintModel(float *in, float *out, int verts, float r, float g, float b)
{
	int i;
	for (i = 0;i < verts;i++)
	{
		out[0] = in[0] * r;
		out[1] = in[1] * g;
		out[2] = in[2] * b;
		out[3] = in[3];
		in += 4;
		out += 4;
	}
}

skinframe_t *R_FetchSkinFrame(void)
{
	model_t *model = currentrenderentity->model;
	if (model->skinscenes[currentrenderentity->skinnum].framecount > 1)
		return &model->skinframes[model->skinscenes[currentrenderentity->skinnum].firstframe + (int) (cl.time * 10) % model->skinscenes[currentrenderentity->skinnum].framecount];
	else
		return &model->skinframes[model->skinscenes[currentrenderentity->skinnum].firstframe];
}

void R_SetupMDLMD2Frames(float colorr, float colorg, float colorb)
{
	md2frame_t *frame1, *frame2, *frame3, *frame4;
	trivertx_t *frame1verts, *frame2verts, *frame3verts, *frame4verts;
	model_t *model;
	model = currentrenderentity->model;

	frame1 = &model->mdlmd2data_frames[currentrenderentity->frameblend[0].frame];
	frame2 = &model->mdlmd2data_frames[currentrenderentity->frameblend[1].frame];
	frame3 = &model->mdlmd2data_frames[currentrenderentity->frameblend[2].frame];
	frame4 = &model->mdlmd2data_frames[currentrenderentity->frameblend[3].frame];
	frame1verts = &model->mdlmd2data_pose[currentrenderentity->frameblend[0].frame * model->numverts];
	frame2verts = &model->mdlmd2data_pose[currentrenderentity->frameblend[1].frame * model->numverts];
	frame3verts = &model->mdlmd2data_pose[currentrenderentity->frameblend[2].frame * model->numverts];
	frame4verts = &model->mdlmd2data_pose[currentrenderentity->frameblend[3].frame * model->numverts];
	R_AliasLerpVerts(model->numverts,
		currentrenderentity->frameblend[0].lerp, frame1verts, frame1->scale, frame1->translate,
		currentrenderentity->frameblend[1].lerp, frame2verts, frame2->scale, frame2->translate,
		currentrenderentity->frameblend[2].lerp, frame3verts, frame3->scale, frame3->translate,
		currentrenderentity->frameblend[3].lerp, frame4verts, frame4->scale, frame4->translate);

	R_LightModel(model->numverts, colorr, colorg, colorb, false);

	R_AliasTransformVerts(model->numverts);
}

void R_DrawQ1Q2AliasModel (float fog)
{
	model_t *model;
	skinframe_t *skinframe;

	model = currentrenderentity->model;

	skinframe = R_FetchSkinFrame();
	if (fog && !(currentrenderentity->effects & EF_ADDITIVE))
	{
		R_SetupMDLMD2Frames(1 - fog, 1 - fog, 1 - fog);

		memset(&aliasmeshinfo, 0, sizeof(aliasmeshinfo));

		aliasmeshinfo.vertex = aliasvert;
		aliasmeshinfo.vertexstep = sizeof(float[4]);
		aliasmeshinfo.numverts = model->numverts;
		aliasmeshinfo.numtriangles = model->numtris;
		aliasmeshinfo.index = model->mdlmd2data_indices;
		aliasmeshinfo.colorstep = sizeof(float[4]);
		aliasmeshinfo.texcoords[0] = model->mdlmd2data_texcoords;
		aliasmeshinfo.texcoordstep[0] = sizeof(float[2]);

		if (currentrenderentity->effects & EF_ADDITIVE)
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE;
		}
		else if (currentrenderentity->alpha != 1.0 || skinframe->fog != NULL)
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			aliasmeshinfo.transparent = false;
			aliasmeshinfo.blendfunc1 = GL_ONE;
			aliasmeshinfo.blendfunc2 = GL_ZERO;
		}

		if (skinframe->base || skinframe->pants || skinframe->shirt || skinframe->glow || skinframe->merged)
		{
			if (currentrenderentity->colormap >= 0 && (skinframe->base || skinframe->pants || skinframe->shirt))
			{
				int c;
				qbyte *color;
				if (skinframe->base)
					R_DrawModelMesh(skinframe->base, aliasvertcolor, 0, 0, 0);
				if (skinframe->pants)
				{
					c = (currentrenderentity->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
					color = (qbyte *) (&d_8to24table[c]);
					if (c >= 224) // fullbright ranges
						R_DrawModelMesh(skinframe->pants, NULL, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
					else
					{
						R_TintModel(aliasvertcolor, aliasvertcolor2, model->numverts, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
						R_DrawModelMesh(skinframe->pants, aliasvertcolor2, 0, 0, 0);
					}
				}
				if (skinframe->shirt)
				{
					c = currentrenderentity->colormap & 0xF0      ;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
					color = (qbyte *) (&d_8to24table[c]);
					if (c >= 224) // fullbright ranges
						R_DrawModelMesh(skinframe->shirt, NULL, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
					else
					{
						R_TintModel(aliasvertcolor, aliasvertcolor2, model->numverts, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
						R_DrawModelMesh(skinframe->shirt, aliasvertcolor2, 0, 0, 0);
					}
				}
			}
			else
			{
				if (skinframe->merged)
					R_DrawModelMesh(skinframe->merged, aliasvertcolor, 0, 0, 0);
				else
				{
					if (skinframe->base) R_DrawModelMesh(skinframe->base, aliasvertcolor, 0, 0, 0);
					if (skinframe->pants) R_DrawModelMesh(skinframe->pants, aliasvertcolor, 0, 0, 0);
					if (skinframe->shirt) R_DrawModelMesh(skinframe->shirt, aliasvertcolor, 0, 0, 0);
				}
			}
			if (skinframe->glow) R_DrawModelMesh(skinframe->glow, NULL, 1 - fog, 1 - fog, 1 - fog);
		}
		else
			R_DrawModelMesh(0, NULL, 1 - fog, 1 - fog, 1 - fog);

		aliasmeshinfo.tex[0] = R_GetTexture(skinframe->fog);
		aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
		aliasmeshinfo.blendfunc2 = GL_ONE;
		aliasmeshinfo.color = NULL;

		aliasmeshinfo.cr = fogcolor[0];
		aliasmeshinfo.cg = fogcolor[1];
		aliasmeshinfo.cb = fogcolor[2];
		aliasmeshinfo.ca = currentrenderentity->alpha * fog;

		c_alias_polys += aliasmeshinfo.numtriangles;
		R_Mesh_Draw(&aliasmeshinfo);
	}
	else if (currentrenderentity->colormap >= 0 || !skinframe->merged || skinframe->glow || !r_quickmodels.integer)
	{
		R_SetupMDLMD2Frames(1 - fog, 1 - fog, 1 - fog);

		memset(&aliasmeshinfo, 0, sizeof(aliasmeshinfo));

		aliasmeshinfo.vertex = aliasvert;
		aliasmeshinfo.vertexstep = sizeof(float[4]);
		aliasmeshinfo.numverts = model->numverts;
		aliasmeshinfo.numtriangles = model->numtris;
		aliasmeshinfo.index = model->mdlmd2data_indices;
		aliasmeshinfo.colorstep = sizeof(float[4]);
		aliasmeshinfo.texcoords[0] = model->mdlmd2data_texcoords;
		aliasmeshinfo.texcoordstep[0] = sizeof(float[2]);

		if (currentrenderentity->effects & EF_ADDITIVE)
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE;
		}
		else if (currentrenderentity->alpha != 1.0 || skinframe->fog != NULL)
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			aliasmeshinfo.transparent = false;
			aliasmeshinfo.blendfunc1 = GL_ONE;
			aliasmeshinfo.blendfunc2 = GL_ZERO;
		}

		if (skinframe->base || skinframe->pants || skinframe->shirt || skinframe->glow || skinframe->merged)
		{
			if (currentrenderentity->colormap >= 0 && (skinframe->base || skinframe->pants || skinframe->shirt))
			{
				int c;
				qbyte *color;
				if (skinframe->base)
					R_DrawModelMesh(skinframe->base, aliasvertcolor, 0, 0, 0);
				if (skinframe->pants)
				{
					c = (currentrenderentity->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
					color = (qbyte *) (&d_8to24table[c]);
					if (c >= 224) // fullbright ranges
						R_DrawModelMesh(skinframe->pants, NULL, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
					else
					{
						R_TintModel(aliasvertcolor, aliasvertcolor2, model->numverts, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
						R_DrawModelMesh(skinframe->pants, aliasvertcolor2, 0, 0, 0);
					}
				}
				if (skinframe->shirt)
				{
					c = currentrenderentity->colormap & 0xF0      ;c += (c >= 128 && c < 224) ? 4 : 12; // 128-224 are backwards ranges
					color = (qbyte *) (&d_8to24table[c]);
					if (c >= 224) // fullbright ranges
						R_DrawModelMesh(skinframe->shirt, NULL, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
					else
					{
						R_TintModel(aliasvertcolor, aliasvertcolor2, model->numverts, color[0] * (1.0f / 255.0f), color[1] * (1.0f / 255.0f), color[2] * (1.0f / 255.0f));
						R_DrawModelMesh(skinframe->shirt, aliasvertcolor2, 0, 0, 0);
					}
				}
			}
			else
			{
				if (skinframe->merged)
					R_DrawModelMesh(skinframe->merged, aliasvertcolor, 0, 0, 0);
				else
				{
					if (skinframe->base) R_DrawModelMesh(skinframe->base, aliasvertcolor, 0, 0, 0);
					if (skinframe->pants) R_DrawModelMesh(skinframe->pants, aliasvertcolor, 0, 0, 0);
					if (skinframe->shirt) R_DrawModelMesh(skinframe->shirt, aliasvertcolor, 0, 0, 0);
				}
			}
			if (skinframe->glow) R_DrawModelMesh(skinframe->glow, NULL, 1 - fog, 1 - fog, 1 - fog);
		}
		else
			R_DrawModelMesh(0, NULL, 1 - fog, 1 - fog, 1 - fog);
	}
	else
	{
		rmeshbufferinfo_t bufmesh;
		memset(&bufmesh, 0, sizeof(bufmesh));
		if (currentrenderentity->effects & EF_ADDITIVE)
		{
			bufmesh.transparent = true;
			bufmesh.blendfunc1 = GL_SRC_ALPHA;
			bufmesh.blendfunc2 = GL_ONE;
		}
		else if (currentrenderentity->alpha != 1.0 || skinframe->fog != NULL)
		{
			bufmesh.transparent = true;
			bufmesh.blendfunc1 = GL_SRC_ALPHA;
			bufmesh.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			bufmesh.transparent = false;
			bufmesh.blendfunc1 = GL_ONE;
			bufmesh.blendfunc2 = GL_ZERO;
		}
		bufmesh.numtriangles = model->numtris;
		bufmesh.numverts = model->numverts;
		bufmesh.tex[0] = R_GetTexture(skinframe->merged);

		R_Mesh_Draw_GetBuffer(&bufmesh);

		aliasvert = bufmesh.vertex;
		aliasvertcolor = bufmesh.color;
		memcpy(bufmesh.index, model->mdlmd2data_indices, bufmesh.numtriangles * sizeof(int[3]));
		memcpy(bufmesh.texcoords[0], model->mdlmd2data_texcoords, bufmesh.numverts * sizeof(float[2]));

		fog = bufmesh.colorscale * (1 - fog);
		R_SetupMDLMD2Frames(fog, fog, fog);

		aliasvert = aliasvertbuf;
		aliasvertcolor = aliasvertcolorbuf;
	}
}

int ZymoticLerpBones(int count, zymbonematrix *bonebase, frameblend_t *blend, zymbone_t *bone)
{
	int i;
	float lerp1, lerp2, lerp3, lerp4;
	zymbonematrix *out, rootmatrix, m, *bone1, *bone2, *bone3, *bone4;

	/*
	m.m[0][0] = 0;
	m.m[0][1] = -1;
	m.m[0][2] = 0;
	m.m[0][3] = 0;
	m.m[1][0] = 1;
	m.m[1][1] = 0;
	m.m[1][2] = 0;
	m.m[1][3] = 0;
	m.m[2][0] = 0;
	m.m[2][1] = 0;
	m.m[2][2] = 1;
	m.m[2][3] = 0;
	R_ConcatTransforms(&softwaretransform_matrix[0], &m.m[0], &rootmatrix.m[0]);
	*/

	// LordHavoc: combine transform from zym coordinate space to quake coordinate space with model to world transform matrix
	rootmatrix.m[0][0] = softwaretransform_matrix[0][1];
	rootmatrix.m[0][1] = -softwaretransform_matrix[0][0];
	rootmatrix.m[0][2] = softwaretransform_matrix[0][2];
	rootmatrix.m[0][3] = softwaretransform_matrix[0][3];
	rootmatrix.m[1][0] = softwaretransform_matrix[1][1];
	rootmatrix.m[1][1] = -softwaretransform_matrix[1][0];
	rootmatrix.m[1][2] = softwaretransform_matrix[1][2];
	rootmatrix.m[1][3] = softwaretransform_matrix[1][3];
	rootmatrix.m[2][0] = softwaretransform_matrix[2][1];
	rootmatrix.m[2][1] = -softwaretransform_matrix[2][0];
	rootmatrix.m[2][2] = softwaretransform_matrix[2][2];
	rootmatrix.m[2][3] = softwaretransform_matrix[2][3];

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
				for (i = 0, out = zymbonepose;i < count;i++, out++)
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
				}
			}
			else
			{
				// 3 poses
				for (i = 0, out = zymbonepose;i < count;i++, out++)
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
				}
			}
		}
		else
		{
			// 2 poses
			for (i = 0, out = zymbonepose;i < count;i++, out++)
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
			}
		}
	}
	else
	{
		// 1 pose
		if (lerp1 != 1)
		{
			// lerp != 1.0
			for (i = 0, out = zymbonepose;i < count;i++, out++)
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
			}
		}
		else
		{
			// lerp == 1.0
			for (i = 0, out = zymbonepose;i < count;i++, out++)
			{
				if (bone->parent >= 0)
					R_ConcatTransforms(&zymbonepose[bone->parent].m[0], &bone1->m[0], &out->m[0]);
				else
					R_ConcatTransforms(&rootmatrix.m[0], &bone1->m[0], &out->m[0]);
				bone1++;
				bone++;
			}
		}
	}
	return true;
}

void ZymoticTransformVerts(int vertcount, int *bonecounts, zymvertex_t *vert)
{
	int c;
	float *out = aliasvert;
	zymbonematrix *matrix;
	while(vertcount--)
	{
		c = *bonecounts++;
		// FIXME: validate bonecounts at load time (must be >= 1)
		// FIXME: need 4th component in origin, for how much of the translate to blend in
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
		out += 4;
	}
}

void ZymoticCalcNormals(int vertcount, int shadercount, int *renderlist)
{
	int a, b, c, d;
	float *out, v1[3], v2[3], normal[3], s;
	int *u;
	// clear normals
	memset(aliasvertnorm, 0, sizeof(float) * vertcount * 3);
	memset(aliasvertusage, 0, sizeof(int) * vertcount);
	// parse render list and accumulate surface normals
	while(shadercount--)
	{
		d = *renderlist++;
		while (d--)
		{
			a = renderlist[0]*4;
			b = renderlist[1]*4;
			c = renderlist[2]*4;
			v1[0] = aliasvert[a+0] - aliasvert[b+0];
			v1[1] = aliasvert[a+1] - aliasvert[b+1];
			v1[2] = aliasvert[a+2] - aliasvert[b+2];
			v2[0] = aliasvert[c+0] - aliasvert[b+0];
			v2[1] = aliasvert[c+1] - aliasvert[b+1];
			v2[2] = aliasvert[c+2] - aliasvert[b+2];
			CrossProduct(v1, v2, normal);
			VectorNormalizeFast(normal);
			// add surface normal to vertices
			a = renderlist[0] * 3;
			aliasvertnorm[a+0] += normal[0];
			aliasvertnorm[a+1] += normal[1];
			aliasvertnorm[a+2] += normal[2];
			aliasvertusage[renderlist[0]]++;
			a = renderlist[1] * 3;
			aliasvertnorm[a+0] += normal[0];
			aliasvertnorm[a+1] += normal[1];
			aliasvertnorm[a+2] += normal[2];
			aliasvertusage[renderlist[1]]++;
			a = renderlist[2] * 3;
			aliasvertnorm[a+0] += normal[0];
			aliasvertnorm[a+1] += normal[1];
			aliasvertnorm[a+2] += normal[2];
			aliasvertusage[renderlist[2]]++;
			renderlist += 3;
		}
	}
	// FIXME: precalc this
	// average surface normals
	out = aliasvertnorm;
	u = aliasvertusage;
	while(vertcount--)
	{
		if (*u > 1)
		{
			s = ixtable[*u];
			out[0] *= s;
			out[1] *= s;
			out[2] *= s;
		}
		u++;
		out += 3;
	}
}

void R_DrawZymoticModelMesh(zymtype1header_t *m)
{
	int i, *renderlist;
	rtexture_t **texture;

	// FIXME: do better fog
	renderlist = (int *)(m->lump_render.start + (int) m);
	texture = (rtexture_t **)(m->lump_shaders.start + (int) m);

	aliasmeshinfo.vertex = aliasvert;
	aliasmeshinfo.vertexstep = sizeof(float[4]);
	aliasmeshinfo.color = aliasvertcolor;
	aliasmeshinfo.colorstep = sizeof(float[4]);
	aliasmeshinfo.texcoords[0] = (float *)(m->lump_texcoords.start + (int) m);
	aliasmeshinfo.texcoordstep[0] = sizeof(float[2]);

	for (i = 0;i < m->numshaders;i++)
	{
		aliasmeshinfo.tex[0] = R_GetTexture(texture[i]);
		if (currentrenderentity->effects & EF_ADDITIVE)
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE;
		}
		else if (currentrenderentity->alpha != 1.0 || R_TextureHasAlpha(texture[i]))
		{
			aliasmeshinfo.transparent = true;
			aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
			aliasmeshinfo.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			aliasmeshinfo.transparent = false;
			aliasmeshinfo.blendfunc1 = GL_ONE;
			aliasmeshinfo.blendfunc2 = GL_ZERO;
		}
		aliasmeshinfo.numtriangles = *renderlist++;
		aliasmeshinfo.index = renderlist;
		c_alias_polys += aliasmeshinfo.numtriangles;
		R_Mesh_Draw(&aliasmeshinfo);
		renderlist += aliasmeshinfo.numtriangles * 3;
	}
}

void R_DrawZymoticModelMeshFog(vec3_t org, zymtype1header_t *m, float fog)
{
	int i, *renderlist;

	// FIXME: do better fog
	renderlist = (int *)(m->lump_render.start + (int) m);

	aliasmeshinfo.tex[0] = 0;
	aliasmeshinfo.blendfunc1 = GL_SRC_ALPHA;
	aliasmeshinfo.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;

	aliasmeshinfo.cr = fogcolor[0];
	aliasmeshinfo.cg = fogcolor[1];
	aliasmeshinfo.cb = fogcolor[2];
	aliasmeshinfo.ca = currentrenderentity->alpha * fog;

	for (i = 0;i < m->numshaders;i++)
	{
		aliasmeshinfo.numtriangles = *renderlist++;
		aliasmeshinfo.index = renderlist;
		c_alias_polys += aliasmeshinfo.numtriangles;
		R_Mesh_Draw(&aliasmeshinfo);
		renderlist += aliasmeshinfo.numtriangles * 3;
	}
}

void R_DrawZymoticModel (float fog)
{
	zymtype1header_t *m;

	// FIXME: do better fog
	m = currentrenderentity->model->zymdata_header;
	ZymoticLerpBones(m->numbones, (zymbonematrix *)(m->lump_poses.start + (int) m), currentrenderentity->frameblend, (zymbone_t *)(m->lump_bones.start + (int) m));
	ZymoticTransformVerts(m->numverts, (int *)(m->lump_vertbonecounts.start + (int) m), (zymvertex_t *)(m->lump_verts.start + (int) m));
	ZymoticCalcNormals(m->numverts, m->numshaders, (int *)(m->lump_render.start + (int) m));

	R_LightModel(m->numverts, 1 - fog, 1 - fog, 1 - fog, true);

	memset(&aliasmeshinfo, 0, sizeof(aliasmeshinfo));
	aliasmeshinfo.numverts = m->numverts;

	R_DrawZymoticModelMesh(m);

	if (fog)
		R_DrawZymoticModelMeshFog(currentrenderentity->origin, m, fog);
}

void R_DrawAliasModel (void)
{
	float fog;
	vec3_t diff;

	if (currentrenderentity->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	softwaretransformforentity(currentrenderentity);

	fog = 0;
	if (fogenabled)
	{
		VectorSubtract(currentrenderentity->origin, r_origin, diff);
		fog = DotProduct(diff,diff);
		if (fog < 0.01f)
			fog = 0.01f;
		fog = exp(fogdensity/fog);
		if (fog > 1)
			fog = 1;
		if (fog < 0.01f)
			fog = 0;
		// fog method: darken, additive fog
		// 1. render model as normal, scaled by inverse of fog alpha (darkens it)
		// 2. render fog as additive
	}

	if (currentrenderentity->model->aliastype == ALIASTYPE_ZYM)
		R_DrawZymoticModel(fog);
	else
		R_DrawQ1Q2AliasModel(fog);
}
