
#include "quakedef.h"
#include "r_shadow.h"

void GL_Models_Init(void)
{
}

void R_Model_Alias_GetMesh_Vertex3f(const entity_render_t *ent, const aliasmesh_t *mesh, float *out3f)
{
	if (mesh->num_vertexboneweights)
	{
		int i, k, blends;
		aliasvertexboneweight_t *v;
		float *out, *matrix, m[12], bonepose[256][12];
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = 0;i < ent->model->alias.aliasnum_bones;i++)
		{
			for (k = 0;k < 12;k++)
				m[k] = 0;
			for (blends = 0;blends < 4 && ent->frameblend[blends].lerp > 0;blends++)
			{
				matrix = ent->model->alias.aliasdata_poses + (ent->frameblend[blends].frame * ent->model->alias.aliasnum_bones + i) * 12;
				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * ent->frameblend[blends].lerp;
			}
			if (ent->model->alias.aliasdata_bones[i].parent >= 0)
				R_ConcatTransforms(bonepose[ent->model->alias.aliasdata_bones[i].parent], m, bonepose[i]);
			else
				for (k = 0;k < 12;k++)
					bonepose[i][k] = m[k];
		}
		// blend the vertex bone weights
		memset(out3f, 0, mesh->num_vertices * sizeof(float[3]));
		v = mesh->data_vertexboneweights;
		for (i = 0;i < mesh->num_vertexboneweights;i++, v++)
		{
			out = out3f + v->vertexindex * 3;
			matrix = bonepose[v->boneindex];
			// FIXME: this can very easily be optimized with SSE or 3DNow
			out[0] += v->origin[0] * matrix[0] + v->origin[1] * matrix[1] + v->origin[2] * matrix[ 2] + v->origin[3] * matrix[ 3];
			out[1] += v->origin[0] * matrix[4] + v->origin[1] * matrix[5] + v->origin[2] * matrix[ 6] + v->origin[3] * matrix[ 7];
			out[2] += v->origin[0] * matrix[8] + v->origin[1] * matrix[9] + v->origin[2] * matrix[10] + v->origin[3] * matrix[11];
		}                                                                                                              
	}
	else
	{
		int i, vertcount;
		float lerp1, lerp2, lerp3, lerp4;
		const float *vertsbase, *verts1, *verts2, *verts3, *verts4;
		// vertex morph
		vertsbase = mesh->data_morphvertex3f;
		vertcount = mesh->num_vertices;
		verts1 = vertsbase + ent->frameblend[0].frame * vertcount * 3;
		lerp1 = ent->frameblend[0].lerp;
		if (ent->frameblend[1].lerp)
		{
			verts2 = vertsbase + ent->frameblend[1].frame * vertcount * 3;
			lerp2 = ent->frameblend[1].lerp;
			if (ent->frameblend[2].lerp)
			{
				verts3 = vertsbase + ent->frameblend[2].frame * vertcount * 3;
				lerp3 = ent->frameblend[2].lerp;
				if (ent->frameblend[3].lerp)
				{
					verts4 = vertsbase + ent->frameblend[3].frame * vertcount * 3;
					lerp4 = ent->frameblend[3].lerp;
					for (i = 0;i < vertcount * 3;i++)
						VectorMAMAMAM(lerp1, verts1 + i, lerp2, verts2 + i, lerp3, verts3 + i, lerp4, verts4 + i, out3f + i);
				}
				else
					for (i = 0;i < vertcount * 3;i++)
						VectorMAMAM(lerp1, verts1 + i, lerp2, verts2 + i, lerp3, verts3 + i, out3f + i);
			}
			else
				for (i = 0;i < vertcount * 3;i++)
					VectorMAM(lerp1, verts1 + i, lerp2, verts2 + i, out3f + i);
		}
		else
			memcpy(out3f, verts1, vertcount * sizeof(float[3]));
	}
}

aliaslayer_t r_aliasnoskinlayers[2] = {{ALIASLAYER_DIFFUSE, NULL, NULL}, {ALIASLAYER_FOG | ALIASLAYER_FORCEDRAW_IF_FIRSTPASS, NULL, NULL}};
aliasskin_t r_aliasnoskin = {0, 2, r_aliasnoskinlayers};
aliasskin_t *R_FetchAliasSkin(const entity_render_t *ent, const aliasmesh_t *mesh)
{
	model_t *model = ent->model;
	if (model->numskins)
	{
		int s = ent->skinnum;
		if ((unsigned int)s >= (unsigned int)model->numskins)
			s = 0;
		if (model->skinscenes[s].framecount > 1)
			s = model->skinscenes[s].firstframe + (int) (cl.time * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
		else
			s = model->skinscenes[s].firstframe;
		if (s >= mesh->num_skins)
			s = 0;
		return mesh->data_skins + s;
	}
	else
	{
		r_aliasnoskinlayers[0].texture = r_notexture;
		return &r_aliasnoskin;
	}
}

void R_DrawAliasModelCallback (const void *calldata1, int calldata2)
{
	int c, fullbright, layernum, firstpass, generatenormals = true;
	float tint[3], fog, ifog, colorscale, ambientcolor4f[4], diffusecolor[3], diffusenormal[3];
	vec3_t diff;
	qbyte *bcolor;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	aliasmesh_t *mesh = ent->model->alias.aliasdata_meshes + calldata2;
	aliaslayer_t *layer;
	aliasskin_t *skin;

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

	firstpass = true;
	skin = R_FetchAliasSkin(ent, mesh);
	R_Model_Alias_GetMesh_Vertex3f(ent, mesh, varray_vertex3f);
	for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
	{
		if (!(layer->flags & ALIASLAYER_FORCEDRAW_IF_FIRSTPASS) || !firstpass)
		{
			if (((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0)
			 || ((layer->flags & ALIASLAYER_FOG) && !fogenabled)
			 ||  (layer->flags & ALIASLAYER_SPECULAR)
			 || ((layer->flags & ALIASLAYER_DIFFUSE) && (r_shadow_realtime_world.integer && r_shadow_realtime_world_lightmaps.value <= 0 && r_ambient.integer <= 0 && r_fullbright.integer == 0 && !(ent->effects & EF_FULLBRIGHT))))
				continue;
		}
		if (!firstpass || (ent->effects & EF_ADDITIVE))
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
		}
		else if ((skin->flags & ALIASSKIN_TRANSPARENT) || ent->alpha != 1.0)
		{
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
		}
		else
		{
			GL_BlendFunc(GL_ONE, GL_ZERO);
			GL_DepthMask(true);
		}
		GL_DepthTest(true);
		firstpass = false;
		colorscale = 1.0f;

		memset(&m, 0, sizeof(m));
		if (layer->texture != NULL)
		{
			m.tex[0] = R_GetTexture(layer->texture);
			m.pointer_texcoord[0] = mesh->data_texcoord2f;
			if (gl_combine.integer && layer->flags & (ALIASLAYER_DIFFUSE | ALIASLAYER_SPECULAR))
			{
				colorscale *= 0.25f;
				m.texrgbscale[0] = 4;
			}
		}
		m.pointer_vertex = varray_vertex3f;

		c_alias_polys += mesh->num_triangles;
		if (layer->flags & ALIASLAYER_FOG)
		{
			colorscale *= fog;
			GL_Color(fogcolor[0] * colorscale, fogcolor[1] * colorscale, fogcolor[2] * colorscale, ent->alpha);
		}
		else
		{
			fullbright = !(layer->flags & ALIASLAYER_DIFFUSE) || r_fullbright.integer || (ent->effects & EF_FULLBRIGHT);
			if (layer->flags & (ALIASLAYER_COLORMAP_PANTS | ALIASLAYER_COLORMAP_SHIRT))
			{
				// 128-224 are backwards ranges
				if (layer->flags & ALIASLAYER_COLORMAP_PANTS)
					c = (ent->colormap & 0xF) << 4;
				else //if (layer->flags & ALIASLAYER_COLORMAP_SHIRT)
					c = (ent->colormap & 0xF0);
				c += (c >= 128 && c < 224) ? 4 : 12;
				bcolor = (qbyte *) (&palette_complete[c]);
				fullbright = fullbright || c >= 224;
				VectorScale(bcolor, (1.0f / 255.0f), tint);
			}
			else
				tint[0] = tint[1] = tint[2] = 1;
			if (r_shadow_realtime_world.integer && !fullbright)
				VectorScale(tint, r_shadow_realtime_world_lightmaps.value, tint);
			colorscale *= ifog;
			if (fullbright)
				GL_Color(tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha);
			else
			{
				if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha, false))
				{
					m.pointer_color = varray_color4f;
					if (generatenormals)
					{
						generatenormals = false;
						Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, varray_vertex3f, mesh->data_texcoord2f, mesh->data_element3i, NULL, NULL, varray_normal3f);
					}
					R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, mesh->num_vertices, varray_vertex3f, varray_normal3f, varray_color4f);
				}
				else
					GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
			}
		}
		R_Mesh_State(&m);
		GL_LockArrays(0, mesh->num_vertices);
		R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
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
		if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_FetchAliasSkin(ent, mesh)->flags & ALIASSKIN_TRANSPARENT)
			R_MeshQueue_AddTransparent(ent->origin, R_DrawAliasModelCallback, ent, meshnum);
		else
			R_DrawAliasModelCallback(ent, meshnum);
	}
}

void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius)
{
	int meshnum;
	aliasmesh_t *mesh;
	aliasskin_t *skin;
	float projectdistance;
	if (ent->effects & EF_ADDITIVE || ent->alpha < 1)
		return;
	projectdistance = lightradius + ent->model->radius;// - sqrt(DotProduct(relativelightorigin, relativelightorigin));
	if (projectdistance > 0.1)
	{
		R_Mesh_Matrix(&ent->matrix);
		for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
		{
			skin = R_FetchAliasSkin(ent, mesh);
			if (skin->flags & ALIASSKIN_TRANSPARENT)
				continue;
			R_Model_Alias_GetMesh_Vertex3f(ent, mesh, varray_vertex3f);
			R_Shadow_VolumeFromSphere(mesh->num_vertices, mesh->num_triangles, varray_vertex3f, mesh->data_element3i, mesh->data_neighbor3i, relativelightorigin, projectdistance, lightradius);
		}
	}
}

void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap)
{
	int c, meshnum, layernum;
	float fog, ifog, lightcolor2[3];
	vec3_t diff;
	qbyte *bcolor;
	aliasmesh_t *mesh;
	aliaslayer_t *layer;
	aliasskin_t *skin;

	if (ent->effects & (EF_ADDITIVE | EF_FULLBRIGHT) || ent->alpha < 1)
		return;

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

	for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
	{
		skin = R_FetchAliasSkin(ent, mesh);
		if (skin->flags & ALIASSKIN_TRANSPARENT)
			continue;
		R_Model_Alias_GetMesh_Vertex3f(ent, mesh, varray_vertex3f);
		Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, varray_vertex3f, mesh->data_texcoord2f, mesh->data_element3i, varray_svector3f, varray_tvector3f, varray_normal3f);
		for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
		{
			if (!(layer->flags & (ALIASLAYER_DIFFUSE | ALIASLAYER_SPECULAR))
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0))
				continue;
			lightcolor2[0] = lightcolor[0] * ifog;
			lightcolor2[1] = lightcolor[1] * ifog;
			lightcolor2[2] = lightcolor[2] * ifog;
			if (layer->flags & ALIASLAYER_SPECULAR)
			{
				c_alias_polys += mesh->num_triangles;
				R_Shadow_RenderLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, varray_vertex3f, varray_svector3f, varray_tvector3f, varray_normal3f, mesh->data_texcoord2f, relativelightorigin, relativeeyeorigin, lightcolor2, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, layer->texture, layer->nmap, layer->texture, lightcubemap, LIGHTING_SPECULAR);
			}
			else if (layer->flags & ALIASLAYER_DIFFUSE)
			{
				if (layer->flags & ALIASLAYER_COLORMAP_PANTS)
				{
					// 128-224 are backwards ranges
					c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
					// fullbright passes were already taken care of, so skip them in realtime lighting passes
					if (c >= 224)
						continue;
					bcolor = (qbyte *) (&palette_complete[c]);
					lightcolor2[0] *= bcolor[0] * (1.0f / 255.0f);
					lightcolor2[1] *= bcolor[1] * (1.0f / 255.0f);
					lightcolor2[2] *= bcolor[2] * (1.0f / 255.0f);
				}
				else if (layer->flags & ALIASLAYER_COLORMAP_SHIRT)
				{
					// 128-224 are backwards ranges
					c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
					// fullbright passes were already taken care of, so skip them in realtime lighting passes
					if (c >= 224)
						continue;
					bcolor = (qbyte *) (&palette_complete[c]);
					lightcolor2[0] *= bcolor[0] * (1.0f / 255.0f);
					lightcolor2[1] *= bcolor[1] * (1.0f / 255.0f);
					lightcolor2[2] *= bcolor[2] * (1.0f / 255.0f);
				}
				c_alias_polys += mesh->num_triangles;
				R_Shadow_RenderLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, varray_vertex3f, varray_svector3f, varray_tvector3f, varray_normal3f, mesh->data_texcoord2f, relativelightorigin, relativeeyeorigin, lightcolor2, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, layer->texture, layer->nmap, layer->texture, lightcubemap, LIGHTING_DIFFUSE);
			}
		}
	}
}

