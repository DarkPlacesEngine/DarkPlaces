
#include "quakedef.h"
#include "cl_collision.h"
#include "r_shadow.h"

typedef struct
{
	float m[3][4];
} zymbonematrix;

// LordHavoc: vertex arrays

float *aliasvertcolorbuf;
float *aliasvertcolor; // this may point at aliasvertcolorbuf or at vertex arrays in the mesh backend
float *aliasvert_svectors;
float *aliasvert_tvectors;
float *aliasvert_normals;

float *aliasvertcolor2;
int *aliasvertusage;
zymbonematrix *zymbonepose;

mempool_t *gl_models_mempool;

void gl_models_start(void)
{
	// allocate vertex processing arrays
	gl_models_mempool = Mem_AllocPool("GL_Models");
	aliasvertcolor = aliasvertcolorbuf = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
	aliasvert_svectors = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
	aliasvert_tvectors = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
	aliasvert_normals = Mem_Alloc(gl_models_mempool, sizeof(float[MD2MAX_VERTS][4]));
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

void R_Model_Alias_GetMeshVerts(const entity_render_t *ent, aliasmesh_t *mesh, float *vertices, float *normals, float *svectors, float *tvectors)
{
	int i, vertcount;
	float lerp1, lerp2, lerp3, lerp4;
	const aliasvertex_t *verts1, *verts2, *verts3, *verts4;

	if (vertices == NULL)
	 	Host_Error("R_Model_Alias_GetMeshVerts: vertices == NULL.\n");
	if (svectors != NULL && (tvectors == NULL || normals == NULL))
	 	Host_Error("R_Model_Alias_GetMeshVerts: svectors requires tvectors and normals.\n");
	if (tvectors != NULL && (svectors == NULL || normals == NULL))
	 	Host_Error("R_Model_Alias_GetMeshVerts: tvectors requires svectors and normals.\n");

	vertcount = mesh->num_vertices;
	verts1 = mesh->data_vertices + ent->frameblend[0].frame * vertcount;
	lerp1 = ent->frameblend[0].lerp;
	if (ent->frameblend[1].lerp)
	{
		verts2 = mesh->data_vertices + ent->frameblend[1].frame * vertcount;
		lerp2 = ent->frameblend[1].lerp;
		if (ent->frameblend[2].lerp)
		{
			verts3 = mesh->data_vertices + ent->frameblend[2].frame * vertcount;
			lerp3 = ent->frameblend[2].lerp;
			if (ent->frameblend[3].lerp)
			{
				verts4 = mesh->data_vertices + ent->frameblend[3].frame * vertcount;
				lerp4 = ent->frameblend[3].lerp;
				// generate vertices
				if (svectors != NULL)
				{
					for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, svectors += 4, tvectors += 4, verts1++, verts2++, verts3++, verts4++)
					{
						VectorMAMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, lerp4, verts4->origin, vertices);
						VectorMAMAMAM(lerp1, verts1->normal, lerp2, verts2->normal, lerp3, verts3->normal, lerp4, verts4->normal, normals);
						VectorMAMAMAM(lerp1, verts1->svector, lerp2, verts2->svector, lerp3, verts3->svector, lerp4, verts4->svector, svectors);
						CrossProduct(svectors, normals, tvectors);
					}
				}
				else if (normals != NULL)
				{
					for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, verts1++, verts2++, verts3++, verts4++)
					{
						VectorMAMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, lerp4, verts4->origin, vertices);
						VectorMAMAMAM(lerp1, verts1->normal, lerp2, verts2->normal, lerp3, verts3->normal, lerp4, verts4->normal, normals);
					}
				}
				else
					for (i = 0;i < vertcount;i++, vertices += 4, verts1++, verts2++, verts3++, verts4++)
						VectorMAMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, lerp4, verts4->origin, vertices);
			}
			else
			{
				// generate vertices
				if (svectors != NULL)
				{
					for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, svectors += 4, tvectors += 4, verts1++, verts2++, verts3++)
					{
						VectorMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, vertices);
						VectorMAMAM(lerp1, verts1->normal, lerp2, verts2->normal, lerp3, verts3->normal, normals);
						VectorMAMAM(lerp1, verts1->svector, lerp2, verts2->svector, lerp3, verts3->svector, svectors);
						CrossProduct(svectors, normals, tvectors);
					}
				}
				else if (normals != NULL)
				{
					for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, verts1++, verts2++, verts3++)
					{
						VectorMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, vertices);
						VectorMAMAM(lerp1, verts1->normal, lerp2, verts2->normal, lerp3, verts3->normal, normals);
					}
				}
				else
					for (i = 0;i < vertcount;i++, vertices += 4, verts1++, verts2++, verts3++)
						VectorMAMAM(lerp1, verts1->origin, lerp2, verts2->origin, lerp3, verts3->origin, vertices);
			}
		}
		else
		{
			// generate vertices
			if (svectors != NULL)
			{
				for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, svectors += 4, tvectors += 4, verts1++, verts2++)
				{
					VectorMAM(lerp1, verts1->origin, lerp2, verts2->origin, vertices);
					VectorMAM(lerp1, verts1->normal, lerp2, verts2->normal, normals);
					VectorMAM(lerp1, verts1->svector, lerp2, verts2->svector, svectors);
					CrossProduct(svectors, normals, tvectors);
				}
			}
			else if (normals != NULL)
			{
				for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, verts1++, verts2++)
				{
					VectorMAM(lerp1, verts1->origin, lerp2, verts2->origin, vertices);
					VectorMAM(lerp1, verts1->normal, lerp2, verts2->normal, normals);
				}
			}
			else
				for (i = 0;i < vertcount;i++, vertices += 4, verts1++, verts2++)
					VectorMAM(lerp1, verts1->origin, lerp2, verts2->origin, vertices);
		}
	}
	else
	{
		// generate vertices
		if (svectors != NULL)
		{
			for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, svectors += 4, tvectors += 4, verts1++)
			{
				VectorM(lerp1, verts1->origin, vertices);
				VectorM(lerp1, verts1->normal, normals);
				VectorM(lerp1, verts1->svector, svectors);
				CrossProduct(svectors, normals, tvectors);
			}
		}
		else if (normals != NULL)
		{
			for (i = 0;i < vertcount;i++, vertices += 4, normals += 4, verts1++)
			{
				VectorM(lerp1, verts1->origin, vertices);
				VectorM(lerp1, verts1->normal, normals);
			}
		}
		else if (lerp1 != 1)
		{
			for (i = 0;i < vertcount;i++, vertices += 4, verts1++)
				VectorM(lerp1, verts1->origin, vertices);
		}
		else
			for (i = 0;i < vertcount;i++, vertices += 4, verts1++)
				VectorCopy(verts1->origin, vertices);
	}
}

aliasskin_t *R_FetchAliasSkin(const entity_render_t *ent, const aliasmesh_t *mesh)
{
	model_t *model = ent->model;
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

void R_DrawAliasModelCallback (const void *calldata1, int calldata2)
{
	int c, fullbright, layernum;
	float tint[3], fog, ifog, colorscale;
	vec3_t diff;
	qbyte *bcolor;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	aliasmesh_t *mesh = ent->model->aliasdata_meshes + calldata2;
	aliaslayer_t *layer;
	aliasskin_t *skin;

	R_Mesh_Matrix(&ent->matrix);

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

	memset(&m, 0, sizeof(m));
	skin = R_FetchAliasSkin(ent, mesh);
	R_Mesh_ResizeCheck(mesh->num_vertices);
	R_Model_Alias_GetMeshVerts(ent, mesh, varray_vertex, aliasvert_normals, NULL, NULL);
	memcpy(varray_texcoord[0], mesh->data_texcoords, mesh->num_vertices * sizeof(float[4]));
	for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
	{
		if (((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
		 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0)
		 ||  (layer->flags & ALIASLAYER_DRAW_PER_LIGHT))
			continue;
		if (layer->flags & ALIASLAYER_FOG)
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE;
			colorscale = r_colorscale;
			m.texrgbscale[0] = 1;
			m.tex[0] = R_GetTexture(layer->texture);
			R_Mesh_State(&m);
			GL_Color(fogcolor[0] * fog * colorscale, fogcolor[1] * fog * colorscale, fogcolor[2] * fog * colorscale, ent->alpha);
			c_alias_polys += mesh->num_triangles;
			R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_elements);
			continue;
		}
		if ((layer->flags & ALIASLAYER_ADD) || ((layer->flags & ALIASLAYER_ALPHA) && (ent->effects & EF_ADDITIVE)))
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE;
		}
		else if ((layer->flags & ALIASLAYER_ALPHA) || ent->alpha != 1.0)
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ZERO;
		}
		colorscale = r_colorscale;
		m.texrgbscale[0] = 1;
		if (gl_combine.integer)
		{
			colorscale *= 0.25f;
			m.texrgbscale[0] = 4;
		}
		m.tex[0] = R_GetTexture(layer->texture);
		R_Mesh_State(&m);
		if (layer->flags & ALIASLAYER_COLORMAP_PANTS)
		{
			// 128-224 are backwards ranges
			c = (ent->colormap & 0xF) << 4;c += (c >= 128 && c < 224) ? 4 : 12;
			bcolor = (qbyte *) (&palette_complete[c]);
			fullbright = c >= 224;
			VectorScale(bcolor, (1.0f / 255.0f), tint);
		}
		else if (layer->flags & ALIASLAYER_COLORMAP_SHIRT)
		{
			// 128-224 are backwards ranges
			c = (ent->colormap & 0xF0);c += (c >= 128 && c < 224) ? 4 : 12;
			bcolor = (qbyte *) (&palette_complete[c]);
			fullbright = c >= 224;
			VectorScale(bcolor, (1.0f / 255.0f), tint);
		}
		else
		{
			tint[0] = tint[1] = tint[2] = 1;
			fullbright = false;
		}
		VectorScale(tint, ifog * colorscale, tint);
		if (!(layer->flags & ALIASLAYER_DIFFUSE))
			fullbright = true;
		if (ent->effects & EF_FULLBRIGHT)
			fullbright = true;
		if (fullbright)
			GL_Color(tint[0], tint[1], tint[2], ent->alpha);
		else
			R_LightModel(ent, mesh->num_vertices, varray_vertex, aliasvert_normals, varray_color, tint[0], tint[1], tint[2], false);
		c_alias_polys += mesh->num_triangles;
		R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_elements);
	}
}

void R_Model_Alias_Draw(entity_render_t *ent)
{
	int meshnum;
	aliasmesh_t *mesh;
	if (ent->alpha < (1.0f / 64.0f))
		return; // basically completely transparent

	c_models++;

	for (meshnum = 0, mesh = ent->model->aliasdata_meshes;meshnum < ent->model->aliasnum_meshes;meshnum++, mesh++)
	{
		if (ent->effects & EF_ADDITIVE || ent->alpha != 1.0 || R_FetchAliasSkin(ent, mesh)->flags & ALIASSKIN_TRANSPARENT)
			R_MeshQueue_AddTransparent(ent->origin, R_DrawAliasModelCallback, ent, meshnum);
		else
			R_DrawAliasModelCallback(ent, meshnum);
	}
}

void R_Model_Alias_DrawFakeShadow (entity_render_t *ent)
{
	int i, meshnum;
	aliasmesh_t *mesh;
	aliasskin_t *skin;
	rmeshstate_t m;
	float *v, planenormal[3], planedist, dist, projection[3], floororigin[3], surfnormal[3], lightdirection[3], v2[3];

	if ((ent->effects & EF_ADDITIVE) || ent->alpha < 1)
		return;

	lightdirection[0] = 0.5;
	lightdirection[1] = 0.2;
	lightdirection[2] = -1;
	VectorNormalizeFast(lightdirection);

	VectorMA(ent->origin, 65536.0f, lightdirection, v2);
	if (CL_TraceLine(ent->origin, v2, floororigin, surfnormal, 0, false, NULL) == 1)
		return;

	R_Mesh_Matrix(&ent->matrix);

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	R_Mesh_State(&m);
	GL_Color(0, 0, 0, 0.5);

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
	for (meshnum = 0, mesh = ent->model->aliasdata_meshes;meshnum < ent->model->aliasnum_meshes;meshnum++)
	{
		skin = R_FetchAliasSkin(ent, mesh);
		if (skin->flags & ALIASSKIN_TRANSPARENT)
			continue;
		R_Mesh_ResizeCheck(mesh->num_vertices);
		R_Model_Alias_GetMeshVerts(ent, mesh, varray_vertex, NULL, NULL, NULL);
		for (i = 0, v = varray_vertex;i < mesh->num_vertices;i++, v += 4)
		{
			dist = DotProduct(v, planenormal) - planedist;
			if (dist > 0)
				VectorMA(v, dist, projection, v);
		}
		c_alias_polys += mesh->num_triangles;
		R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_elements);
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
	projectdistance = lightradius + ent->model->radius - sqrt(DotProduct(relativelightorigin, relativelightorigin));
	if (projectdistance > 0.1)
	{
		R_Mesh_Matrix(&ent->matrix);
		for (meshnum = 0, mesh = ent->model->aliasdata_meshes;meshnum < ent->model->aliasnum_meshes;meshnum++, mesh++)
		{
			skin = R_FetchAliasSkin(ent, mesh);
			if (skin->flags & ALIASSKIN_TRANSPARENT)
				continue;
			R_Mesh_ResizeCheck(mesh->num_vertices * 2);
			R_Model_Alias_GetMeshVerts(ent, mesh, varray_vertex, NULL, NULL, NULL);
			R_Shadow_Volume(mesh->num_vertices, mesh->num_triangles, mesh->data_elements, mesh->data_neighbors, relativelightorigin, lightradius, projectdistance);
		}
	}
}

void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor)
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

	for (meshnum = 0, mesh = ent->model->aliasdata_meshes;meshnum < ent->model->aliasnum_meshes;meshnum++, mesh++)
	{
		skin = R_FetchAliasSkin(ent, mesh);
		if (skin->flags & ALIASSKIN_TRANSPARENT)
			continue;
		R_Mesh_ResizeCheck(mesh->num_vertices);
		R_Model_Alias_GetMeshVerts(ent, mesh, varray_vertex, aliasvert_normals, aliasvert_svectors, aliasvert_tvectors);
		for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
		{
			if (!(layer->flags & ALIASLAYER_DRAW_PER_LIGHT)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0))
				continue;
			lightcolor2[0] = lightcolor[0] * ifog;
			lightcolor2[1] = lightcolor[1] * ifog;
			lightcolor2[2] = lightcolor[2] * ifog;
			if (layer->flags & ALIASLAYER_SPECULAR)
			{
				c_alias_polys += mesh->num_triangles;
				R_Shadow_SpecularLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_elements, aliasvert_svectors, aliasvert_tvectors, aliasvert_normals, mesh->data_texcoords, relativelightorigin, relativeeyeorigin, lightradius, lightcolor2, layer->texture, layer->nmap, NULL);
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
				R_Shadow_DiffuseLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_elements, aliasvert_svectors, aliasvert_tvectors, aliasvert_normals, mesh->data_texcoords, relativelightorigin, lightradius, lightcolor2, layer->texture, layer->nmap, NULL);
			}
		}
	}
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
	ZymoticCalcNormals(numverts, varray_vertex, aliasvert_normals, ent->model->zymnum_shaders, ent->model->zymdata_renderlist);
	memcpy(varray_texcoord[0], ent->model->zymdata_texcoords, ent->model->zymnum_verts * sizeof(float[4]));
	GL_UseColorArray();
	R_LightModel(ent, numverts, varray_vertex, aliasvert_normals, varray_color, ifog * colorscale, ifog * colorscale, ifog * colorscale, false);
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
