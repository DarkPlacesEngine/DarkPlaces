
#include "quakedef.h"
#include "r_shadow.h"

void GL_Models_Init(void)
{
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
			s = model->skinscenes[s].firstframe + (int) (r_refdef.time * model->skinscenes[s].framerate) % model->skinscenes[s].framecount;
		else
			s = model->skinscenes[s].firstframe;
		if (s >= mesh->num_skins)
			s = 0;
		return mesh->data_skins + s;
	}
	else
	{
		r_aliasnoskinlayers[0].texture = r_texture_notexture;
		return &r_aliasnoskin;
	}
}

void R_DrawAliasModelCallback (const void *calldata1, int calldata2)
{
	int c, fullbright, layernum, firstpass;
	float tint[3], fog, ifog, colorscale, ambientcolor4f[4], diffusecolor[3], diffusenormal[3];
	float *vertex3f, *normal3f;
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

	vertex3f = NULL;
	normal3f = NULL;
	for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
	{
		if (!(layer->flags & ALIASLAYER_FORCEDRAW_IF_FIRSTPASS) || !firstpass)
		{
			if (((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0)
			 || ((layer->flags & ALIASLAYER_FOG) && !fogenabled)
			 ||  (layer->flags & ALIASLAYER_SPECULAR)
			 || ((layer->flags & ALIASLAYER_DIFFUSE) && (ent->flags & RENDER_LIGHT) && r_lightmapintensity <= 0 && !(ent->flags & RENDER_TRANSPARENT) && r_ambient.integer <= 0))
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
		GL_DepthTest(!(ent->effects & EF_NODEPTHTEST));
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
		if (!vertex3f)
		{
			if (ent->frameblend[0].frame == 0 && ent->frameblend[0].lerp == 1)
				vertex3f = mesh->data_basevertex3f;
			else
			{
				vertex3f = varray_vertex3f;
				Mod_Alias_GetMesh_Vertex3f(ent->model, ent->frameblend, mesh, vertex3f);
			}
		}
		m.pointer_vertex = vertex3f;

		c_alias_polys += mesh->num_triangles;
		if (layer->flags & ALIASLAYER_FOG)
		{
			colorscale *= fog;
			GL_Color(fogcolor[0] * colorscale, fogcolor[1] * colorscale, fogcolor[2] * colorscale, ent->alpha);
		}
		else
		{
			fullbright = !(layer->flags & ALIASLAYER_DIFFUSE) || !(ent->flags & RENDER_LIGHT);
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
			tint[0] *= ent->colormod[0];
			tint[1] *= ent->colormod[1];
			tint[2] *= ent->colormod[2];
			if (!fullbright && !(ent->flags & RENDER_TRANSPARENT))
				colorscale *= r_lightmapintensity;
			colorscale *= ifog;
			if (fullbright)
				GL_Color(tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha);
			else if (R_LightModel(ambientcolor4f, diffusecolor, diffusenormal, ent, tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha, false))
			{
				m.pointer_color = varray_color4f;
				if (normal3f == NULL)
				{
					if (ent->frameblend[0].frame == 0 && ent->frameblend[0].lerp == 1)
						normal3f = mesh->data_basenormal3f;
					else
					{
						normal3f = varray_normal3f;
						Mod_BuildNormals(mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_element3i, normal3f);
					}
				}
				R_LightModel_CalcVertexColors(ambientcolor4f, diffusecolor, diffusenormal, mesh->num_vertices, vertex3f, normal3f, varray_color4f);
			}
			else
				GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
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
			R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : ent->origin, R_DrawAliasModelCallback, ent, meshnum);
		else
			R_DrawAliasModelCallback(ent, meshnum);
	}
}

void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
	int meshnum;
	aliasmesh_t *mesh;
	aliasskin_t *skin;
	float projectdistance, *vertex3f;
	if (!(ent->flags & RENDER_SHADOW))
		return;
	// check the box in modelspace, it was already checked in worldspace
	if (!BoxesOverlap(ent->model->normalmins, ent->model->normalmaxs, lightmins, lightmaxs))
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

void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist)
{
	int c, meshnum, layernum;
	float fog, ifog, lightcolor2[3], ambientscale2, diffusescale2, specularscale2;
	float *vertex3f, *svector3f, *tvector3f, *normal3f;
	vec3_t diff;
	qbyte *bcolor;
	aliasmesh_t *mesh;
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

	for (meshnum = 0, mesh = ent->model->alias.aliasdata_meshes;meshnum < ent->model->alias.aliasnum_meshes;meshnum++, mesh++)
	{
		skin = R_FetchAliasSkin(ent, mesh);
		// FIXME: transparent skins need to be lit during the transparent render
		if (skin->flags & ALIASSKIN_TRANSPARENT)
			continue;
		vertex3f = NULL;
		svector3f = NULL;
		tvector3f = NULL;
		normal3f = NULL;
		for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
		{
			if (!(layer->flags & (ALIASLAYER_DIFFUSE | ALIASLAYER_SPECULAR))
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0))
				continue;
			lightcolor2[0] = lightcolor[0] * ifog;
			lightcolor2[1] = lightcolor[1] * ifog;
			lightcolor2[2] = lightcolor[2] * ifog;
			ambientscale2 = 0;
			diffusescale2 = 0;
			specularscale2 = 0;
			if (layer->flags & ALIASLAYER_SPECULAR)
				specularscale2 = specularscale;
			if (layer->flags & ALIASLAYER_DIFFUSE)
			{
				if (layer->flags & ALIASLAYER_COLORMAP_PANTS)
				{
					// 128-224 are backwards ranges
					c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
					// fullbright passes were already taken care of, so skip them in realtime lighting passes
					if (c < 224)
					{
						bcolor = (qbyte *) (&palette_complete[c]);
						lightcolor2[0] *= bcolor[0] * (1.0f / 255.0f);
						lightcolor2[1] *= bcolor[1] * (1.0f / 255.0f);
						lightcolor2[2] *= bcolor[2] * (1.0f / 255.0f);
						ambientscale2 = ambientscale;
						diffusescale2 = diffusescale;
					}
				}
				else if (layer->flags & ALIASLAYER_COLORMAP_SHIRT)
				{
					// 128-224 are backwards ranges
					c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
					// fullbright passes were already taken care of, so skip them in realtime lighting passes
					if (c < 224)
					{
						bcolor = (qbyte *) (&palette_complete[c]);
						lightcolor2[0] *= bcolor[0] * (1.0f / 255.0f);
						lightcolor2[1] *= bcolor[1] * (1.0f / 255.0f);
						lightcolor2[2] *= bcolor[2] * (1.0f / 255.0f);
						ambientscale2 = ambientscale;
						diffusescale2 = diffusescale;
					}
				}
				else
				{
					ambientscale2 = ambientscale;
					diffusescale2 = diffusescale;
				}
			}
			if (!(ambientscale2 + diffusescale2 + specularscale2) || VectorLength2(lightcolor2) <= 0.01)
				continue;
			c_alias_polys += mesh->num_triangles;
			if (!vertex3f)
			{
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
					Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, vertex3f, mesh->data_texcoord2f, mesh->data_element3i, svector3f, tvector3f, normal3f);
				}
			}
			// TODO: make layer have ->gloss as well as ->texture, and merge specular layer for common non-colormapped case?
			R_Shadow_RenderLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, vertex3f, svector3f, tvector3f, normal3f, mesh->data_texcoord2f, relativelightorigin, relativeeyeorigin, lightcolor2, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, layer->texture, layer->nmap, layer->texture, lightcubemap, ambientscale2, diffusescale2, specularscale2);
		}
	}
}

