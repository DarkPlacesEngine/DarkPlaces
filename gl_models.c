
#include "quakedef.h"
#include "r_shadow.h"

void GL_Models_Init(void)
{
}

static texture_t r_aliasnotexture;
static texture_t *R_FetchAliasSkin(const entity_render_t *ent, const aliasmesh_t *mesh)
{
	model_t *model = ent->model;
	if (model->numskins)
	{
		int s = ent->skinnum;
		if ((unsigned int)s >= (unsigned int)model->numskins)
			s = 0;
		if (model->skinscenes[s].framecount > 1)
			s = model->skinscenes[s].firstframe + (unsigned int) (r_refdef.time * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
		else
			s = model->skinscenes[s].firstframe;
		if (s >= mesh->num_skins)
			s = 0;
		return mesh->data_skins + s;
	}
	else
	{
		memset(&r_aliasnotexture, 0, sizeof(r_aliasnotexture));
		r_aliasnotexture.skin.base = r_texture_notexture;
		return &r_aliasnotexture;
	}
}

static void R_DrawAliasModelCallback (const void *calldata1, int calldata2)
{
	int c, fbbase, fbpants, fbshirt, doglow;
	float tint[3], fog, ifog, colorscale, ambientcolor4f[4], diffusecolor[3], diffusenormal[3], colorbase[3], colorpants[3], colorshirt[3];
	float *vertex3f, *normal3f;
	vec3_t diff;
	qbyte *bcolor;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	aliasmesh_t *mesh = ent->model->alias.aliasdata_meshes + calldata2;
	texture_t *texture;

	R_Mesh_Matrix(&ent->matrix);

	fog = 0;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_vieworigin, diff);
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

	VectorScale(ent->colormod, ifog, colorbase);
	VectorClear(colorpants);
	VectorClear(colorshirt);
	fbbase = ent->effects & EF_FULLBRIGHT;
	fbpants = fbbase;
	fbshirt = fbbase;
	if (ent->colormap >= 0)
	{
		// 128-224 are backwards ranges
		c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
		if (c >= 224)
			fbpants = true;
		bcolor = (qbyte *) (&palette_complete[c]);
		colorpants[0] = colorbase[0] * bcolor[0] * (1.0f / 255.0f);
		colorpants[1] = colorbase[1] * bcolor[1] * (1.0f / 255.0f);
		colorpants[2] = colorbase[2] * bcolor[2] * (1.0f / 255.0f);
		// 128-224 are backwards ranges
		c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
		if (c >= 224)
			fbshirt = true;
		bcolor = (qbyte *) (&palette_complete[c]);
		colorshirt[0] = colorbase[0] * bcolor[0] * (1.0f / 255.0f);
		colorshirt[1] = colorbase[1] * bcolor[1] * (1.0f / 255.0f);
		colorshirt[2] = colorbase[2] * bcolor[2] * (1.0f / 255.0f);
	}

	texture = R_FetchAliasSkin(ent, mesh);

	if ((ent->effects & EF_ADDITIVE))
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
	}
	else if (texture->skin.fog || ent->alpha != 1.0)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_DepthMask(false);
	}
	else
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
	}
	GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
	colorscale = 1.0f;
	if (ent->frameblend[0].frame == 0 && ent->frameblend[0].lerp == 1)
	{
		vertex3f = mesh->data_basevertex3f;
		normal3f = mesh->data_basenormal3f;
	}
	else
	{
		vertex3f = varray_vertex3f;
		Mod_Alias_GetMesh_Vertex3f(ent->model, ent->frameblend, mesh, vertex3f);
		normal3f = NULL;
	}

	doglow = texture->skin.glow != NULL;

	memset(&m, 0, sizeof(m));
	m.pointer_vertex = vertex3f;
	m.pointer_texcoord[0] = mesh->data_texcoord2f;
	if (gl_combine.integer)
	{
		colorscale *= 0.25f;
		m.texrgbscale[0] = 4;
	}

	m.tex[0] = R_GetTexture((ent->colormap >= 0 || !texture->skin.merged) ? texture->skin.base : texture->skin.merged);
	VectorScale(colorbase, colorscale, tint);
	m.pointer_color = NULL;
	if (fbbase)
		GL_Color(tint[0], tint[1], tint[2], ent->alpha);
	else if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, tint[0], tint[1], tint[2], ent->alpha, false))
	{
		m.pointer_color = varray_color4f;
		if (normal3f == NULL)
		{
			normal3f = varray_normal3f;
			Mod_BuildNormals(0, mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_element3i, normal3f);
		}
		R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, mesh->num_vertices, vertex3f, normal3f, varray_color4f);
	}
	else
		GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
	if (gl_combine.integer && doglow)
	{
		doglow = false;
		m.tex[1] = R_GetTexture(texture->skin.glow);
		m.pointer_texcoord[1] = mesh->data_texcoord2f;
		m.texcombinergb[1] = GL_ADD;
	}
	R_Mesh_State(&m);
	c_alias_polys += mesh->num_triangles;
	GL_LockArrays(0, mesh->num_vertices);
	R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
	GL_LockArrays(0, 0);
	m.tex[1] = 0;
	m.pointer_texcoord[1] = NULL;
	m.texcombinergb[1] = 0;

	VectorScale(colorpants, colorscale, tint);
	if (ent->colormap >= 0 && texture->skin.pants && VectorLength2(tint) >= 0.001)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		m.tex[0] = R_GetTexture(texture->skin.pants);
		m.pointer_color = NULL;
		if (fbpants)
			GL_Color(tint[0], tint[1], tint[2], ent->alpha);
		else if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, tint[0], tint[1], tint[2], ent->alpha, false))
		{
			m.pointer_color = varray_color4f;
			if (normal3f == NULL)
			{
				normal3f = varray_normal3f;
				Mod_BuildNormals(0, mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_element3i, normal3f);
			}
			R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, mesh->num_vertices, vertex3f, normal3f, varray_color4f);
		}
		else
			GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
		R_Mesh_State(&m);
		c_alias_polys += mesh->num_triangles;
		GL_LockArrays(0, mesh->num_vertices);
		R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
		GL_LockArrays(0, 0);
	}

	VectorScale(colorshirt, colorscale, tint);
	if (ent->colormap >= 0 && texture->skin.shirt && VectorLength2(tint) >= 0.001)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		m.tex[0] = R_GetTexture(texture->skin.shirt);
		m.pointer_color = NULL;
		if (fbshirt)
			GL_Color(tint[0], tint[1], tint[2], ent->alpha);
		else if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, tint[0], tint[1], tint[2], ent->alpha, false))
		{
			m.pointer_color = varray_color4f;
			if (normal3f == NULL)
			{
				normal3f = varray_normal3f;
				Mod_BuildNormals(0, mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_element3i, normal3f);
			}
			R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, mesh->num_vertices, vertex3f, normal3f, varray_color4f);
		}
		else
			GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
		R_Mesh_State(&m);
		c_alias_polys += mesh->num_triangles;
		GL_LockArrays(0, mesh->num_vertices);
		R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
		GL_LockArrays(0, 0);
	}

	colorscale = 1;
	m.texrgbscale[0] = 0;
	m.pointer_color = NULL;

	if (doglow)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		m.tex[0] = R_GetTexture(texture->skin.glow);
		GL_Color(1, 1, 1, ent->alpha);
		R_Mesh_State(&m);
		c_alias_polys += mesh->num_triangles;
		GL_LockArrays(0, mesh->num_vertices);
		R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
		GL_LockArrays(0, 0);
	}

	if (fog > 0)
	{
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		m.tex[0] = R_GetTexture(texture->skin.fog);
		GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], fog * ent->alpha);
		R_Mesh_State(&m);
		c_alias_polys += mesh->num_triangles;
		GL_LockArrays(0, mesh->num_vertices);
		R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
		GL_LockArrays(0, 0);
	}
}

void R_Model_Alias_Draw(entity_render_t *ent)
{
	int meshnum;
	aliasmesh_t *mesh;
	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
	{
		if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_FetchAliasSkin(ent, mesh)->skin.fog)
			R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : ent->origin, R_DrawAliasModelCallback, ent, meshnum);
		else
			R_DrawAliasModelCallback(ent, meshnum);
	}
}

void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
	int meshnum;
	aliasmesh_t *mesh;
	texture_t *texture;
	float projectdistance, *vertex3f;
	if (!(ent->flags & RENDER_SHADOW))
		return;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(ent->model->normalmins, ent->model->normalmaxs, lightmins, lightmaxs))
		return;
	projectdistance = lightradius + ent->model->radius;// - sqrt(DotProduct(relativelightorigin, relativelightorigin));
	if (projectdistance > 0.1)
	{
		for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
		{
			texture = R_FetchAliasSkin(ent, mesh);
			if (texture->skin.fog)
				continue;
			if (ent->frameblend[0].frame == 0 && ent->frameblend[0].lerp == 1)
				vertex3f = mesh->data_basevertex3f;
			else
			{
				vertex3f = varray_vertex3f;
				Mod_Alias_GetMesh_Vertex3f(ent->model, ent->frameblend, mesh, vertex3f);
			}
			// identify lit faces within the bounding box
			R_Shadow_PrepareShadowMark(mesh->num_triangles);
			R_Shadow_MarkVolumeFromBox(0, mesh->num_triangles, vertex3f, mesh->data_element3i, relativelightorigin, lightmins, lightmaxs, ent->model->normalmins, ent->model->normalmaxs);
			R_Shadow_VolumeFromList(mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_element3i, mesh->data_neighbor3i, relativelightorigin, projectdistance, numshadowmark, shadowmarklist);
		}
	}
}

void R_Model_Alias_DrawLight(entity_render_t *ent, float *lightcolor, int numsurfaces, const int *surfacelist)
{
	int c, meshnum;
	float fog, ifog, lightcolorbase[3], lightcolorpants[3], lightcolorshirt[3];
	float *vertex3f, *svector3f, *tvector3f, *normal3f;
	vec3_t diff;
	qbyte *bcolor;
	aliasmesh_t *mesh;
	texture_t *texture;

	fog = 0;
	if (fogenabled)
	{
		VectorSubtract(ent->origin, r_vieworigin, diff);
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

	VectorScale(lightcolor, ifog, lightcolorbase);
	if (VectorLength2(lightcolorbase) < 0.001)
		return;
	VectorClear(lightcolorpants);
	VectorClear(lightcolorshirt);
	if (ent->colormap >= 0)
	{
		// 128-224 are backwards ranges
		c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
		// fullbright passes were already taken care of, so skip them in realtime lighting passes
		if (c < 224)
		{
			bcolor = (qbyte *) (&palette_complete[c]);
			lightcolorpants[0] = lightcolorbase[0] * bcolor[0] * (1.0f / 255.0f);
			lightcolorpants[1] = lightcolorbase[1] * bcolor[1] * (1.0f / 255.0f);
			lightcolorpants[2] = lightcolorbase[2] * bcolor[2] * (1.0f / 255.0f);
		}
		// 128-224 are backwards ranges
		c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
		// fullbright passes were already taken care of, so skip them in realtime lighting passes
		if (c < 224)
		{
			bcolor = (qbyte *) (&palette_complete[c]);
			lightcolorshirt[0] = lightcolorbase[0] * bcolor[0] * (1.0f / 255.0f);
			lightcolorshirt[1] = lightcolorbase[1] * bcolor[1] * (1.0f / 255.0f);
			lightcolorshirt[2] = lightcolorbase[2] * bcolor[2] * (1.0f / 255.0f);
		}
	}

	for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
	{
		texture = R_FetchAliasSkin(ent, mesh);
		// FIXME: transparent skins need to be lit during the transparent render
		if (texture->skin.fog)
			continue;
		if (ent->frameblend[0].frame == 0 && ent->frameblend[0].lerp == 1)
		{
			vertex3f = mesh->data_basevertex3f;
			svector3f = mesh->data_basesvector3f;
			tvector3f = mesh->data_basetvector3f;
			normal3f = mesh->data_basenormal3f;
		}
		else
		{
			vertex3f = varray_vertex3f;
			svector3f = varray_svector3f;
			tvector3f = varray_tvector3f;
			normal3f = varray_normal3f;
			Mod_Alias_GetMesh_Vertex3f(ent->model, ent->frameblend, mesh, vertex3f);
			Mod_BuildTextureVectorsAndNormals(0, mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_texcoord2f, mesh->data_element3i, svector3f, tvector3f, normal3f);
		}
		c_alias_polys += mesh->num_triangles;
		R_Shadow_RenderLighting(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, vertex3f, svector3f, tvector3f, normal3f, mesh->data_texcoord2f, lightcolorbase, lightcolorpants, lightcolorshirt, (ent->colormap >= 0 || !texture->skin.merged) ? texture->skin.base : texture->skin.merged, ent->colormap >= 0 ? texture->skin.pants : 0, ent->colormap >= 0 ? texture->skin.shirt : 0, texture->skin.nmap, texture->skin.gloss);
	}
}

