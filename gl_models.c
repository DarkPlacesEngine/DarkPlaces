
#include "quakedef.h"

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
}

void gl_models_shutdown(void)
{
	Mem_FreePool(&gl_models_mempool);
}

void gl_models_newmap(void)
{
}

void GL_Models_Init(void)
{
	Cvar_RegisterVariable(&r_quickmodels);

	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

/*
void R_AliasTransformVerts(int vertcount)
{
	vec3_t point;
	float *av;
	av = aliasvert;
	while (vertcount >= 4)
	{
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		vertcount -= 4;
	}
	while(vertcount > 0)
	{
		VectorCopy(av, point);softwaretransform(point, av);av += 4;
		vertcount--;
	}
}
*/

void R_AliasLerpVerts(int vertcount,
		float lerp1, const trivertx_t *verts1, const vec3_t fscale1, const vec3_t translate1,
		float lerp2, const trivertx_t *verts2, const vec3_t fscale2, const vec3_t translate2,
		float lerp3, const trivertx_t *verts3, const vec3_t fscale3, const vec3_t translate3,
		float lerp4, const trivertx_t *verts4, const vec3_t fscale4, const vec3_t translate4)
{
	int i;
	vec3_t scale1, scale2, scale3, scale4, translate;
	const float *n1, *n2, *n3, *n4;
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

skinframe_t *R_FetchSkinFrame(const entity_render_t *ent)
{
	model_t *model = ent->model;
	unsigned int s = (unsigned int) ent->skinnum;
	if (s >= model->numskins)
		s = 0;
	if (model->skinscenes[s].framecount > 1)
		return &model->skinframes[model->skinscenes[s].firstframe + (int) (cl.time * 10) % model->skinscenes[s].framecount];
	else
		return &model->skinframes[model->skinscenes[s].firstframe];
}

void R_SetupMDLMD2Frames(const entity_render_t *ent, float colorr, float colorg, float colorb)
{
	const md2frame_t *frame1, *frame2, *frame3, *frame4;
	const trivertx_t *frame1verts, *frame2verts, *frame3verts, *frame4verts;
	const model_t *model = ent->model;

	frame1 = &model->mdlmd2data_frames[ent->frameblend[0].frame];
	frame2 = &model->mdlmd2data_frames[ent->frameblend[1].frame];
	frame3 = &model->mdlmd2data_frames[ent->frameblend[2].frame];
	frame4 = &model->mdlmd2data_frames[ent->frameblend[3].frame];
	frame1verts = &model->mdlmd2data_pose[ent->frameblend[0].frame * model->numverts];
	frame2verts = &model->mdlmd2data_pose[ent->frameblend[1].frame * model->numverts];
	frame3verts = &model->mdlmd2data_pose[ent->frameblend[2].frame * model->numverts];
	frame4verts = &model->mdlmd2data_pose[ent->frameblend[3].frame * model->numverts];
	R_AliasLerpVerts(model->numverts,
		ent->frameblend[0].lerp, frame1verts, frame1->scale, frame1->translate,
		ent->frameblend[1].lerp, frame2verts, frame2->scale, frame2->translate,
		ent->frameblend[2].lerp, frame3verts, frame3->scale, frame3->translate,
		ent->frameblend[3].lerp, frame4verts, frame4->scale, frame4->translate);

	R_LightModel(ent, model->numverts, colorr, colorg, colorb, false);

	//R_AliasTransformVerts(model->numverts);
}

void R_DrawQ1Q2AliasModelCallback (const void *calldata1, int calldata2)
{
	int c, pantsfullbright, shirtfullbright, colormapped;
	float pantscolor[3], shirtcolor[3];
	float fog;
	vec3_t diff;
	qbyte *bcolor;
	rmeshbufferinfo_t m;
	model_t *model;
	skinframe_t *skinframe;
	const entity_render_t *ent = calldata1;
	int blendfunc1, blendfunc2;

//	softwaretransformforentity(ent);

	fog = 0;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_origin, diff);
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

	model = ent->model;

	skinframe = R_FetchSkinFrame(ent);

	colormapped = !skinframe->merged || (ent->colormap >= 0 && skinframe->base && (skinframe->pants || skinframe->shirt));
	if (!colormapped && !fog && !skinframe->glow && !skinframe->fog)
	{
		// fastpath for the normal situation (one texture)
		memset(&m, 0, sizeof(m));
		if (ent->effects & EF_ADDITIVE)
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE;
		}
		else if (ent->alpha != 1.0 || skinframe->fog != NULL)
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ZERO;
		}
		m.numtriangles = model->numtris;
		m.numverts = model->numverts;
		m.tex[0] = R_GetTexture(skinframe->merged);
		m.matrix = ent->matrix;

		c_alias_polys += m.numtriangles;
		if (R_Mesh_Draw_GetBuffer(&m, true))
		{
			memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
			memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));

			aliasvert = m.vertex;
			aliasvertcolor = m.color;
			R_SetupMDLMD2Frames(ent, m.colorscale, m.colorscale, m.colorscale);
			aliasvert = aliasvertbuf;
			aliasvertcolor = aliasvertcolorbuf;

			R_Mesh_Render();
		}
		return;
	}

	R_SetupMDLMD2Frames(ent, 1 - fog, 1 - fog, 1 - fog);

	if (colormapped)
	{
		// 128-224 are backwards ranges
		c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
		bcolor = (qbyte *) (&d_8to24table[c]);
		pantsfullbright = c >= 224;
		VectorScale(bcolor, (1.0f / 255.0f), pantscolor);
		c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
		bcolor = (qbyte *) (&d_8to24table[c]);
		shirtfullbright = c >= 224;
		VectorScale(bcolor, (1.0f / 255.0f), shirtcolor);
	}
	else
	{
		pantscolor[0] = pantscolor[1] = pantscolor[2] = shirtcolor[0] = shirtcolor[1] = shirtcolor[2] = 1;
		pantsfullbright = shirtfullbright = false;
	}

	if (ent->effects & EF_ADDITIVE)
	{
		blendfunc1 = GL_SRC_ALPHA;
		blendfunc2 = GL_ONE;
	}
	else if (ent->alpha != 1.0 || skinframe->fog != NULL)
	{
		blendfunc1 = GL_SRC_ALPHA;
		blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		blendfunc1 = GL_ONE;
		blendfunc2 = GL_ZERO;
	}

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = blendfunc1;
	m.blendfunc2 = blendfunc2;
	m.numtriangles = model->numtris;
	m.numverts = model->numverts;
	m.matrix = ent->matrix;
	m.tex[0] = colormapped ? R_GetTexture(skinframe->base) : R_GetTexture(skinframe->merged);
	if (m.tex[0] && R_Mesh_Draw_GetBuffer(&m, true))
	{
		blendfunc1 = GL_SRC_ALPHA;
		blendfunc2 = GL_ONE;
		c_alias_polys += m.numtriangles;
		R_ModulateColors(aliasvertcolor, m.color, m.numverts, m.colorscale, m.colorscale, m.colorscale);
		memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
		memcpy(m.vertex, aliasvert, m.numverts * sizeof(float[4]));
		memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));
		R_Mesh_Render();
	}

	if (colormapped)
	{
		if (skinframe->pants)
		{
			memset(&m, 0, sizeof(m));
			m.blendfunc1 = blendfunc1;
			m.blendfunc2 = blendfunc2;
			m.numtriangles = model->numtris;
			m.numverts = model->numverts;
			m.matrix = ent->matrix;
			m.tex[0] = R_GetTexture(skinframe->pants);
			if (m.tex[0] && R_Mesh_Draw_GetBuffer(&m, true))
			{
				blendfunc1 = GL_SRC_ALPHA;
				blendfunc2 = GL_ONE;
				c_alias_polys += m.numtriangles;
				if (pantsfullbright)
					R_FillColors(m.color, m.numverts, pantscolor[0] * m.colorscale, pantscolor[1] * m.colorscale, pantscolor[2] * m.colorscale, ent->alpha);
				else
					R_ModulateColors(aliasvertcolor, m.color, m.numverts, pantscolor[0] * m.colorscale, pantscolor[1] * m.colorscale, pantscolor[2] * m.colorscale);
				memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
				memcpy(m.vertex, aliasvert, m.numverts * sizeof(float[4]));
				memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));
				R_Mesh_Render();
			}
		}
		if (skinframe->shirt)
		{
			memset(&m, 0, sizeof(m));
			m.blendfunc1 = blendfunc1;
			m.blendfunc2 = blendfunc2;
			m.numtriangles = model->numtris;
			m.numverts = model->numverts;
			m.matrix = ent->matrix;
			m.tex[0] = R_GetTexture(skinframe->shirt);
			if (m.tex[0] && R_Mesh_Draw_GetBuffer(&m, true))
			{
				blendfunc1 = GL_SRC_ALPHA;
				blendfunc2 = GL_ONE;
				c_alias_polys += m.numtriangles;
				if (shirtfullbright)
					R_FillColors(m.color, m.numverts, shirtcolor[0] * m.colorscale, shirtcolor[1] * m.colorscale, shirtcolor[2] * m.colorscale, ent->alpha);
				else
					R_ModulateColors(aliasvertcolor, m.color, m.numverts, shirtcolor[0] * m.colorscale, shirtcolor[1] * m.colorscale, shirtcolor[2] * m.colorscale);
				memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
				memcpy(m.vertex, aliasvert, m.numverts * sizeof(float[4]));
				memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));
				R_Mesh_Render();
			}
		}
	}
	if (skinframe->glow)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = blendfunc1;
		m.blendfunc2 = blendfunc2;
		m.numtriangles = model->numtris;
		m.numverts = model->numverts;
		m.matrix = ent->matrix;
		m.tex[0] = R_GetTexture(skinframe->glow);
		if (m.tex[0] && R_Mesh_Draw_GetBuffer(&m, true))
		{
			blendfunc1 = GL_SRC_ALPHA;
			blendfunc2 = GL_ONE;
			c_alias_polys += m.numtriangles;
			R_FillColors(m.color, m.numverts, (1 - fog) * m.colorscale, (1 - fog) * m.colorscale, (1 - fog) * m.colorscale, ent->alpha);
			memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
			memcpy(m.vertex, aliasvert, m.numverts * sizeof(float[4]));
			memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));
			R_Mesh_Render();
		}
	}
	if (fog)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
		m.numtriangles = model->numtris;
		m.numverts = model->numverts;
		m.matrix = ent->matrix;
		m.tex[0] = R_GetTexture(skinframe->fog);
		if (m.tex[0] && R_Mesh_Draw_GetBuffer(&m, true))
		{
			c_alias_polys += m.numtriangles;
			R_FillColors(m.color, m.numverts, fogcolor[0] * fog * m.colorscale, fogcolor[1] * fog * m.colorscale, fogcolor[2] * fog * m.colorscale, ent->alpha);
			memcpy(m.index, model->mdlmd2data_indices, m.numtriangles * sizeof(int[3]));
			memcpy(m.vertex, aliasvert, m.numverts * sizeof(float[4]));
			memcpy(m.texcoords[0], model->mdlmd2data_texcoords, m.numverts * sizeof(float[2]));
			R_Mesh_Render();
		}
	}
}

int ZymoticLerpBones(int count, const zymbonematrix *bonebase, const frameblend_t *blend, const zymbone_t *bone)
{
	int i;
	float lerp1, lerp2, lerp3, lerp4;
	zymbonematrix *out, rootmatrix, m;
	const zymbonematrix *bone1, *bone2, *bone3, *bone4;

	/*
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
	*/
	rootmatrix.m[0][0] = 1;
	rootmatrix.m[0][1] = 0;
	rootmatrix.m[0][2] = 0;
	rootmatrix.m[0][3] = 0;
	rootmatrix.m[1][0] = 0;
	rootmatrix.m[1][1] = 1;
	rootmatrix.m[1][2] = 0;
	rootmatrix.m[1][3] = 0;
	rootmatrix.m[2][0] = 0;
	rootmatrix.m[2][1] = 0;
	rootmatrix.m[2][2] = 1;
	rootmatrix.m[2][3] = 0;

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
						R_ConcatTransforms(&zymbonepose[bone->parent].m[0][0], &m.m[0][0], &out->m[0][0]);
					else
						R_ConcatTransforms(&rootmatrix.m[0][0], &m.m[0][0], &out->m[0][0]);
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
						R_ConcatTransforms(&zymbonepose[bone->parent].m[0][0], &m.m[0][0], &out->m[0][0]);
					else
						R_ConcatTransforms(&rootmatrix.m[0][0], &m.m[0][0], &out->m[0][0]);
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
					R_ConcatTransforms(&zymbonepose[bone->parent].m[0][0], &m.m[0][0], &out->m[0][0]);
				else
					R_ConcatTransforms(&rootmatrix.m[0][0], &m.m[0][0], &out->m[0][0]);
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
					R_ConcatTransforms(&zymbonepose[bone->parent].m[0][0], &m.m[0][0], &out->m[0][0]);
				else
					R_ConcatTransforms(&rootmatrix.m[0][0], &m.m[0][0], &out->m[0][0]);
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
					R_ConcatTransforms(&zymbonepose[bone->parent].m[0][0], &bone1->m[0][0], &out->m[0][0]);
				else
					R_ConcatTransforms(&rootmatrix.m[0][0], &bone1->m[0][0], &out->m[0][0]);
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

void R_DrawZymoticModelMeshCallback (const void *calldata1, int calldata2)
{
	float fog;
	vec3_t diff;
	int i, *renderlist;
	zymtype1header_t *m;
	rtexture_t *texture;
	rmeshbufferinfo_t mbuf;
	const entity_render_t *ent = calldata1;
	int shadernum = calldata2;

	// find the vertex index list and texture
	m = ent->model->zymdata_header;
	renderlist = (int *)(m->lump_render.start + (int) m);
	for (i = 0;i < shadernum;i++)
		renderlist += renderlist[0] * 3 + 1;
	texture = ((rtexture_t **)(m->lump_shaders.start + (int) m))[shadernum];

	fog = 0;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_origin, diff);
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

	ZymoticLerpBones(m->numbones, (zymbonematrix *)(m->lump_poses.start + (int) m), ent->frameblend, (zymbone_t *)(m->lump_bones.start + (int) m));
	ZymoticTransformVerts(m->numverts, (int *)(m->lump_vertbonecounts.start + (int) m), (zymvertex_t *)(m->lump_verts.start + (int) m));
	ZymoticCalcNormals(m->numverts, m->numshaders, (int *)(m->lump_render.start + (int) m));

	R_LightModel(ent, m->numverts, 1 - fog, 1 - fog, 1 - fog, false);

	memset(&mbuf, 0, sizeof(mbuf));
	mbuf.numverts = m->numverts;
	mbuf.numtriangles = renderlist[0];
	if (ent->effects & EF_ADDITIVE)
	{
		mbuf.blendfunc1 = GL_SRC_ALPHA;
		mbuf.blendfunc2 = GL_ONE;
	}
	else if (ent->alpha != 1.0 || R_TextureHasAlpha(texture))
	{
		mbuf.blendfunc1 = GL_SRC_ALPHA;
		mbuf.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		mbuf.blendfunc1 = GL_ONE;
		mbuf.blendfunc2 = GL_ZERO;
	}
	mbuf.tex[0] = R_GetTexture(texture);
	mbuf.matrix = ent->matrix;
	if (R_Mesh_Draw_GetBuffer(&mbuf, true))
	{
		c_alias_polys += mbuf.numtriangles;
		memcpy(mbuf.index, renderlist + 1, mbuf.numtriangles * sizeof(int[3]));
		memcpy(mbuf.vertex, aliasvert, mbuf.numverts * sizeof(float[4]));
		R_ModulateColors(aliasvertcolor, mbuf.color, mbuf.numverts, mbuf.colorscale, mbuf.colorscale, mbuf.colorscale);
		//memcpy(mbuf.color, aliasvertcolor, mbuf.numverts * sizeof(float[4]));
		memcpy(mbuf.texcoords[0], (float *)(m->lump_texcoords.start + (int) m), mbuf.numverts * sizeof(float[2]));
		R_Mesh_Render();
	}

	if (fog)
	{
		memset(&mbuf, 0, sizeof(mbuf));
		mbuf.numverts = m->numverts;
		mbuf.numtriangles = renderlist[0];
		mbuf.blendfunc1 = GL_SRC_ALPHA;
		mbuf.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		// FIXME: need alpha mask for fogging...
		//mbuf.tex[0] = R_GetTexture(texture);
		mbuf.matrix = ent->matrix;
		if (R_Mesh_Draw_GetBuffer(&mbuf, false))
		{
			c_alias_polys += mbuf.numtriangles;
			memcpy(mbuf.index, renderlist + 1, mbuf.numtriangles * sizeof(int[3]));
			memcpy(mbuf.vertex, aliasvert, mbuf.numverts * sizeof(float[4]));
			R_FillColors(mbuf.color, mbuf.numverts, fogcolor[0] * mbuf.colorscale, fogcolor[1] * mbuf.colorscale, fogcolor[2] * mbuf.colorscale, ent->alpha * fog);
			//memcpy(mbuf.texcoords[0], (float *)(m->lump_texcoords.start + (int) m), mbuf.numverts * sizeof(float[2]));
			R_Mesh_Render();
		}
	}
}

void R_DrawZymoticModel (entity_render_t *ent)
{
	int i;
	zymtype1header_t *m;
	rtexture_t *texture;

	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	m = ent->model->zymdata_header;
	for (i = 0;i < m->numshaders;i++)
	{
		texture = ((rtexture_t **)(m->lump_shaders.start + (int) m))[i];
		if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_TextureHasAlpha(texture))
			R_MeshQueue_AddTransparent(ent->origin, R_DrawZymoticModelMeshCallback, ent, i);
		else
			R_MeshQueue_Add(R_DrawZymoticModelMeshCallback, ent, i);
	}
}

void R_DrawQ1Q2AliasModel(entity_render_t *ent)
{
	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_FetchSkinFrame(ent)->fog != NULL)
		R_MeshQueue_AddTransparent(ent->origin, R_DrawQ1Q2AliasModelCallback, ent, 0);
	else
		R_MeshQueue_Add(R_DrawQ1Q2AliasModelCallback, ent, 0);
}

