
#include "quakedef.h"
#include "cl_collision.h"
#include "r_shadow.h"

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
	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

void R_AliasLerpVerts(int vertcount, float *vertices, float *normals,
		float lerp1, const trivertx_t *verts1, const vec3_t fscale1, const vec3_t translate1,
		float lerp2, const trivertx_t *verts2, const vec3_t fscale2, const vec3_t translate2,
		float lerp3, const trivertx_t *verts3, const vec3_t fscale3, const vec3_t translate3,
		float lerp4, const trivertx_t *verts4, const vec3_t fscale4, const vec3_t translate4)
{
	int i;
	vec3_t scale1, scale2, scale3, scale4, translate;
	const float *n1, *n2, *n3, *n4;
	float *av, *avn;
	av = vertices;
	avn = normals;
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

void R_LerpMDLMD2Vertices(const entity_render_t *ent, float *vertices, float *normals)
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
	R_AliasLerpVerts(model->numverts, vertices, normals,
		ent->frameblend[0].lerp, frame1verts, frame1->scale, frame1->translate,
		ent->frameblend[1].lerp, frame2verts, frame2->scale, frame2->translate,
		ent->frameblend[2].lerp, frame3verts, frame3->scale, frame3->translate,
		ent->frameblend[3].lerp, frame4verts, frame4->scale, frame4->translate);
}

void R_DrawQ1Q2AliasModelCallback (const void *calldata1, int calldata2)
{
	int i, c, fullbright, pantsfullbright, shirtfullbright, colormapped, tex;
	float pantscolor[3], shirtcolor[3];
	float fog, ifog, colorscale;
	vec3_t diff;
	qbyte *bcolor;
	rmeshstate_t m;
	model_t *model;
	skinframe_t *skinframe;
	const entity_render_t *ent = calldata1;
	int blendfunc1, blendfunc2;

	R_Mesh_Matrix(&ent->matrix);

	model = ent->model;
	R_Mesh_ResizeCheck(model->numverts);

	skinframe = R_FetchSkinFrame(ent);

	fullbright = (ent->effects & EF_FULLBRIGHT) != 0;

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
	ifog = 1 - fog;

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

	R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
	memcpy(varray_texcoord[0], model->mdlmd2data_texcoords, model->numverts * sizeof(float[4]));
	if (!skinframe->base && !skinframe->pants && !skinframe->shirt && !skinframe->glow)
	{
		// untextured
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = blendfunc1;
		m.blendfunc2 = blendfunc2;
		colorscale = r_colorscale;
		if (gl_combine.integer)
		{
			colorscale *= 0.25f;
			m.texrgbscale[0] = 4;
		}
		m.tex[0] = R_GetTexture(r_notexture);
		R_Mesh_State(&m);
		c_alias_polys += model->numtris;
		for (i = 0;i < model->numverts * 4;i += 4)
		{
			varray_texcoord[0][i + 0] *= 8.0f;
			varray_texcoord[0][i + 1] *= 8.0f;
		}
		R_LightModel(ent, model->numverts, varray_vertex, aliasvertnorm, varray_color, colorscale, colorscale, colorscale, false);
		GL_UseColorArray();
		R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
		return;
	}

	colormapped = !skinframe->merged || (ent->colormap >= 0 && skinframe->base && (skinframe->pants || skinframe->shirt));
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

	tex = colormapped ? R_GetTexture(skinframe->base) : R_GetTexture(skinframe->merged);
	if (tex)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = blendfunc1;
		m.blendfunc2 = blendfunc2;
		colorscale = r_colorscale;
		if (gl_combine.integer)
		{
			colorscale *= 0.25f;
			m.texrgbscale[0] = 4;
		}
		m.tex[0] = tex;
		R_Mesh_State(&m);
		if (fullbright)
			GL_Color(colorscale * ifog, colorscale * ifog, colorscale * ifog, ent->alpha);
		else
		{
			GL_UseColorArray();
			R_LightModel(ent, model->numverts, varray_vertex, aliasvertnorm, varray_color, colorscale * ifog, colorscale * ifog, colorscale * ifog, false);
		}
		R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
		c_alias_polys += model->numtris;
		blendfunc1 = GL_SRC_ALPHA;
		blendfunc2 = GL_ONE;
	}

	if (colormapped)
	{
		if (skinframe->pants)
		{
			tex = R_GetTexture(skinframe->pants);
			if (tex)
			{
				memset(&m, 0, sizeof(m));
				m.blendfunc1 = blendfunc1;
				m.blendfunc2 = blendfunc2;
				colorscale = r_colorscale;
				if (gl_combine.integer)
				{
					colorscale *= 0.25f;
					m.texrgbscale[0] = 4;
				}
				m.tex[0] = tex;
				R_Mesh_State(&m);
				if (pantsfullbright)
					GL_Color(pantscolor[0] * colorscale * ifog, pantscolor[1] * colorscale * ifog, pantscolor[2] * colorscale * ifog, ent->alpha);
				else
				{
					GL_UseColorArray();
					R_LightModel(ent, model->numverts, varray_vertex, aliasvertnorm, varray_color, pantscolor[0] * colorscale * ifog, pantscolor[1] * colorscale * ifog, pantscolor[2] * colorscale * ifog, false);
				}
				R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
				c_alias_polys += model->numtris;
				blendfunc1 = GL_SRC_ALPHA;
				blendfunc2 = GL_ONE;
			}
		}
		if (skinframe->shirt)
		{
			tex = R_GetTexture(skinframe->shirt);
			if (tex)
			{
				memset(&m, 0, sizeof(m));
				m.blendfunc1 = blendfunc1;
				m.blendfunc2 = blendfunc2;
				colorscale = r_colorscale;
				if (gl_combine.integer)
				{
					colorscale *= 0.25f;
					m.texrgbscale[0] = 4;
				}
				m.tex[0] = tex;
				R_Mesh_State(&m);
				if (shirtfullbright)
					GL_Color(shirtcolor[0] * colorscale * ifog, shirtcolor[1] * colorscale * ifog, shirtcolor[2] * colorscale * ifog, ent->alpha);
				else
				{
					GL_UseColorArray();
					R_LightModel(ent, model->numverts, varray_vertex, aliasvertnorm, varray_color, shirtcolor[0] * colorscale * ifog, shirtcolor[1] * colorscale * ifog, shirtcolor[2] * colorscale * ifog, false);
				}
				R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
				c_alias_polys += model->numtris;
				blendfunc1 = GL_SRC_ALPHA;
				blendfunc2 = GL_ONE;
			}
		}
	}
	if (skinframe->glow)
	{
		tex = R_GetTexture(skinframe->glow);
		if (tex)
		{
			memset(&m, 0, sizeof(m));
			m.blendfunc1 = blendfunc1;
			m.blendfunc2 = blendfunc2;
			m.tex[0] = tex;
			R_Mesh_State(&m);

			blendfunc1 = GL_SRC_ALPHA;
			blendfunc2 = GL_ONE;
			c_alias_polys += model->numtris;
			GL_Color(ifog * r_colorscale, ifog * r_colorscale, ifog * r_colorscale, ent->alpha);
			R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
		}
	}
	if (fog)
	{
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
		m.tex[0] = R_GetTexture(skinframe->fog);
		R_Mesh_State(&m);

		c_alias_polys += model->numtris;
		GL_Color(fogcolor[0] * fog * r_colorscale, fogcolor[1] * fog * r_colorscale, fogcolor[2] * fog * r_colorscale, ent->alpha);
		R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
	}
}

void R_Model_Alias_Draw(entity_render_t *ent)
{
	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_FetchSkinFrame(ent)->fog != NULL)
		R_MeshQueue_AddTransparent(ent->origin, R_DrawQ1Q2AliasModelCallback, ent, 0);
	else
		R_DrawQ1Q2AliasModelCallback(ent, 0);
}

extern cvar_t r_shadows;
void R_Model_Alias_DrawFakeShadow (entity_render_t *ent)
{
	int i;
	rmeshstate_t m;
	model_t *model;
	float *v, planenormal[3], planedist, dist, projection[3], floororigin[3], surfnormal[3], lightdirection[3], v2[3];

	/*
	if (r_shadows.integer > 1)
	{
		float f, lightscale, lightcolor[3];
		vec3_t temp;
		mlight_t *sl;
		rdlight_t *rd;
		memset(&m, 0, sizeof(m));
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ONE;
		R_Mesh_State(&m);
		R_Mesh_Matrix(&ent->matrix);
		for (i = 0, sl = cl.worldmodel->lights;i < cl.worldmodel->numlights;i++, sl++)
		{
			if (d_lightstylevalue[sl->style] > 0)
			{
				VectorSubtract(ent->origin, sl->origin, temp);
				f = DotProduct(temp,temp);
				if (f < (ent->model->radius2 + sl->cullradius2))
				{
					model = ent->model;
					R_Mesh_ResizeCheck(model->numverts * 2);
					R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
					Matrix4x4_Transform(&ent->inversematrix, sl->origin, temp);
					GL_Color(0.1 * r_colorscale, 0.025 * r_colorscale, 0.0125 * r_colorscale, 1);
					R_Shadow_Volume(model->numverts, model->numtris, varray_vertex, model->mdlmd2data_indices, model->mdlmd2data_triangleneighbors, temp, sl->cullradius + model->radius - sqrt(f), true);
					GL_UseColorArray();
					lightscale = d_lightstylevalue[sl->style] * (1.0f / 65536.0f);
					VectorScale(sl->light, lightscale, lightcolor);
					R_Shadow_VertexLight(model->numverts, varray_vertex, aliasvertnorm, temp, sl->cullradius2, sl->distbias, sl->subtract, lightcolor);
					R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
				}
			}
		}
		for (i = 0, rd = r_dlight;i < r_numdlights;i++, rd++)
		{
			if (ent != rd->ent)
			{
				VectorSubtract(ent->origin, rd->origin, temp);
				f = DotProduct(temp,temp);
				if (f < (ent->model->radius2 + rd->cullradius2))
				{
					model = ent->model;
					R_Mesh_ResizeCheck(model->numverts * 2);
					R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
					Matrix4x4_Transform(&ent->inversematrix, rd->origin, temp);
					GL_Color(0.1 * r_colorscale, 0.025 * r_colorscale, 0.0125 * r_colorscale, 1);
					R_Shadow_Volume(model->numverts, model->numtris, varray_vertex, model->mdlmd2data_indices, model->mdlmd2data_triangleneighbors, temp, rd->cullradius + model->radius - sqrt(f), true);
					GL_UseColorArray();
					R_Shadow_VertexLight(model->numverts, varray_vertex, aliasvertnorm, temp, rd->cullradius2, LIGHTOFFSET, rd->subtract, rd->light);
					R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
				}
			}
		}
		return;
	}
	*/

	lightdirection[0] = 0.5;
	lightdirection[1] = 0.2;
	lightdirection[2] = -1;
	VectorNormalizeFast(lightdirection);

	VectorMA(ent->origin, 65536.0f, lightdirection, v2);
	if (CL_TraceLine(ent->origin, v2, floororigin, surfnormal, 0, false, NULL) == 1)
		return;

	R_Mesh_Matrix(&ent->matrix);

	model = ent->model;
	R_Mesh_ResizeCheck(model->numverts);

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	R_Mesh_State(&m);

	c_alias_polys += model->numtris;
	R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);

	// put a light direction in the entity's coordinate space
	Matrix4x4_Transform3x3(&ent->inversematrix, lightdirection, projection);
	VectorNormalizeFast(projection);

	// put the plane's normal in the entity's coordinate space
	Matrix4x4_Transform3x3(&ent->inversematrix, surfnormal, planenormal);
	VectorNormalizeFast(planenormal);

	// put the plane's distance in the entity's coordinate space
	VectorSubtract(floororigin, ent->origin, floororigin);
	planedist = DotProduct(floororigin, surfnormal) + 2;

	dist = -1.0f / DotProduct(projection, planenormal);
	VectorScale(projection, dist, projection);
	for (i = 0, v = varray_vertex;i < model->numverts;i++, v += 4)
	{
		dist = DotProduct(v, planenormal) - planedist;
		if (dist > 0)
		//if (i & 1)
			VectorMA(v, dist, projection, v);
	}
	GL_Color(0, 0, 0, 0.5);
	R_Mesh_Draw(model->numverts, model->numtris, model->mdlmd2data_indices);
}

void R_Model_Alias_DrawDepth(entity_render_t *ent)
{
	R_Mesh_ResizeCheck(ent->model->numverts);
	R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
	R_Mesh_Draw(ent->model->numverts, ent->model->numtris, ent->model->mdlmd2data_indices);
}

void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int visiblevolume)
{
	float projectdistance;
	projectdistance = lightradius + ent->model->radius - sqrt(DotProduct(relativelightorigin, relativelightorigin));
	if (projectdistance > 0.1)
	{
		R_Mesh_ResizeCheck(ent->model->numverts * 2);
		R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
		R_Shadow_Volume(ent->model->numverts, ent->model->numtris, varray_vertex, ent->model->mdlmd2data_indices, ent->model->mdlmd2data_triangleneighbors, relativelightorigin, lightradius, projectdistance, visiblevolume);
	}
}

void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, float lightdistbias, float lightsubtract, float *lightcolor)
{
	R_Mesh_ResizeCheck(ent->model->numverts);
	R_LerpMDLMD2Vertices(ent, varray_vertex, aliasvertnorm);
	R_Shadow_VertexLight(ent->model->numverts, varray_vertex, aliasvertnorm, relativelightorigin, lightradius * lightradius, lightdistbias, lightsubtract, lightcolor);
	GL_UseColorArray();
	R_Mesh_Draw(ent->model->numverts, ent->model->numtris, ent->model->mdlmd2data_indices);
}

void R_Model_Alias_DrawOntoLight(entity_render_t *ent)
{
	// FIXME
}

int ZymoticLerpBones(int count, const zymbonematrix *bonebase, const frameblend_t *blend, const zymbone_t *bone)
{
	int i;
	float lerp1, lerp2, lerp3, lerp4;
	zymbonematrix *out, rootmatrix, m;
	const zymbonematrix *bone1, *bone2, *bone3, *bone4;

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

void ZymoticTransformVerts(int vertcount, float *vertex, int *bonecounts, zymvertex_t *vert)
{
	int c;
	float *out = vertex;
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

void ZymoticCalcNormals(int vertcount, float *vertex, float *normals, int shadercount, int *renderlist)
{
	int a, b, c, d;
	float *out, v1[3], v2[3], normal[3], s;
	int *u;
	// clear normals
	memset(normals, 0, sizeof(float) * vertcount * 3);
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
			v1[0] = vertex[a+0] - vertex[b+0];
			v1[1] = vertex[a+1] - vertex[b+1];
			v1[2] = vertex[a+2] - vertex[b+2];
			v2[0] = vertex[c+0] - vertex[b+0];
			v2[1] = vertex[c+1] - vertex[b+1];
			v2[2] = vertex[c+2] - vertex[b+2];
			CrossProduct(v1, v2, normal);
			VectorNormalizeFast(normal);
			// add surface normal to vertices
			a = renderlist[0] * 3;
			normals[a+0] += normal[0];
			normals[a+1] += normal[1];
			normals[a+2] += normal[2];
			aliasvertusage[renderlist[0]]++;
			a = renderlist[1] * 3;
			normals[a+0] += normal[0];
			normals[a+1] += normal[1];
			normals[a+2] += normal[2];
			aliasvertusage[renderlist[1]]++;
			a = renderlist[2] * 3;
			normals[a+0] += normal[0];
			normals[a+1] += normal[1];
			normals[a+2] += normal[2];
			aliasvertusage[renderlist[2]]++;
			renderlist += 3;
		}
	}
	// FIXME: precalc this
	// average surface normals
	out = normals;
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
	float fog, ifog, colorscale;
	vec3_t diff;
	int i, *renderlist, *elements;
	rtexture_t *texture;
	rmeshstate_t mstate;
	const entity_render_t *ent = calldata1;
	int shadernum = calldata2;
	int numverts, numtriangles;

	R_Mesh_Matrix(&ent->matrix);

	// find the vertex index list and texture
	renderlist = ent->model->zymdata_renderlist;
	for (i = 0;i < shadernum;i++)
		renderlist += renderlist[0] * 3 + 1;
	texture = ent->model->zymdata_textures[shadernum];

	numverts = ent->model->zymnum_verts;
	numtriangles = *renderlist++;
	elements = renderlist;
	R_Mesh_ResizeCheck(numverts);

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
	ifog = 1 - fog;

	memset(&mstate, 0, sizeof(mstate));
	if (ent->effects & EF_ADDITIVE)
	{
		mstate.blendfunc1 = GL_SRC_ALPHA;
		mstate.blendfunc2 = GL_ONE;
	}
	else if (ent->alpha != 1.0 || R_TextureHasAlpha(texture))
	{
		mstate.blendfunc1 = GL_SRC_ALPHA;
		mstate.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		mstate.blendfunc1 = GL_ONE;
		mstate.blendfunc2 = GL_ZERO;
	}
	colorscale = r_colorscale;
	if (gl_combine.integer)
	{
		mstate.texrgbscale[0] = 4;
		colorscale *= 0.25f;
	}
	mstate.tex[0] = R_GetTexture(texture);
	R_Mesh_State(&mstate);
	ZymoticLerpBones(ent->model->zymnum_bones, (zymbonematrix *) ent->model->zymdata_poses, ent->frameblend, ent->model->zymdata_bones);
	ZymoticTransformVerts(numverts, varray_vertex, ent->model->zymdata_vertbonecounts, ent->model->zymdata_verts);
	ZymoticCalcNormals(numverts, varray_vertex, aliasvertnorm, ent->model->zymnum_shaders, ent->model->zymdata_renderlist);
	memcpy(varray_texcoord[0], ent->model->zymdata_texcoords, ent->model->zymnum_verts * sizeof(float[4]));
	GL_UseColorArray();
	R_LightModel(ent, numverts, varray_vertex, aliasvertnorm, varray_color, ifog * colorscale, ifog * colorscale, ifog * colorscale, false);
	R_Mesh_Draw(numverts, numtriangles, elements);
	c_alias_polys += numtriangles;

	if (fog)
	{
		memset(&mstate, 0, sizeof(mstate));
		mstate.blendfunc1 = GL_SRC_ALPHA;
		mstate.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		// FIXME: need alpha mask for fogging...
		//mstate.tex[0] = R_GetTexture(texture);
		R_Mesh_State(&mstate);
		GL_Color(fogcolor[0] * r_colorscale, fogcolor[1] * r_colorscale, fogcolor[2] * r_colorscale, ent->alpha * fog);
		R_Mesh_Draw(numverts, numtriangles, elements);
		c_alias_polys += numtriangles;
	}
}

void R_Model_Zymotic_Draw(entity_render_t *ent)
{
	int i;

	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	for (i = 0;i < ent->model->zymnum_shaders;i++)
	{
		if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_TextureHasAlpha(ent->model->zymdata_textures[i]))
			R_MeshQueue_AddTransparent(ent->origin, R_DrawZymoticModelMeshCallback, ent, i);
		else
			R_DrawZymoticModelMeshCallback(ent, i);
	}
}

void R_Model_Zymotic_DrawFakeShadow(entity_render_t *ent)
{
	// FIXME
}

void R_Model_Zymotic_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, float lightradius2, float lightdistbias, float lightsubtract, float *lightcolor)
{
	// FIXME
}

void R_Model_Zymotic_DrawOntoLight(entity_render_t *ent)
{
	// FIXME
}
