
#include "quakedef.h"
#include "cl_collision.h"
#include "r_shadow.h"

typedef struct
{
	float m[3][4];
} zymbonematrix;

// LordHavoc: vertex arrays
int aliasvertmax = 0;
void *aliasvertarrays = NULL;
float *aliasvertcolor4fbuf = NULL;
float *aliasvertcolor4f = NULL; // this may point at aliasvertcolorbuf or at vertex arrays in the mesh backend
float *aliasvert_svector3f = NULL;
float *aliasvert_tvector3f = NULL;
float *aliasvert_normal3f = NULL;

float *aliasvertcolor2_4f = NULL;
int *aliasvertusage;
zymbonematrix *zymbonepose;

mempool_t *gl_models_mempool;

#define expandaliasvert(newmax) if ((newmax) > aliasvertmax) gl_models_allocarrays(newmax)

void gl_models_allocarrays(int newmax)
{
	qbyte *data;
	aliasvertmax = newmax;
	if (aliasvertarrays != NULL)
		Mem_Free(aliasvertarrays);
	aliasvertarrays = Mem_Alloc(gl_models_mempool, aliasvertmax * (sizeof(float[4+4+3+3+3]) + sizeof(int[3])));
	data = aliasvertarrays;
	aliasvertcolor4f = aliasvertcolor4fbuf = (void *)data;data += aliasvertmax * sizeof(float[4]);
	aliasvertcolor2_4f = (void *)data;data += aliasvertmax * sizeof(float[4]); // used temporarily for tinted coloring
	aliasvert_svector3f = (void *)data;data += aliasvertmax * sizeof(float[3]);
	aliasvert_tvector3f = (void *)data;data += aliasvertmax * sizeof(float[3]);
	aliasvert_normal3f = (void *)data;data += aliasvertmax * sizeof(float[3]);
	aliasvertusage = (void *)data;data += aliasvertmax * sizeof(int[3]);
}

void gl_models_freearrays(void)
{
	aliasvertmax = 0;
	if (aliasvertarrays != NULL)
		Mem_Free(aliasvertarrays);
	aliasvertarrays = NULL;
	aliasvertcolor4f = aliasvertcolor4fbuf = NULL;
	aliasvertcolor2_4f = NULL;
	aliasvert_svector3f = NULL;
	aliasvert_tvector3f = NULL;
	aliasvert_normal3f = NULL;
	aliasvertusage = NULL;
}

void gl_models_start(void)
{
	// allocate vertex processing arrays
	gl_models_mempool = Mem_AllocPool("GL_Models");
	zymbonepose = Mem_Alloc(gl_models_mempool, sizeof(zymbonematrix[256]));
	gl_models_allocarrays(4096);
}

void gl_models_shutdown(void)
{
	gl_models_freearrays();
	Mem_FreePool(&gl_models_mempool);
}

void gl_models_newmap(void)
{
}

void GL_Models_Init(void)
{
	R_RegisterModule("GL_Models", gl_models_start, gl_models_shutdown, gl_models_newmap);
}

#define MODELARRAY_VERTEX 0
#define MODELARRAY_SVECTOR 1
#define MODELARRAY_TVECTOR 2
#define MODELARRAY_NORMAL 3

void R_Model_Alias_GetMesh_Array3f(const entity_render_t *ent, aliasmesh_t *mesh, int whicharray, float *out3f)
{
	int i, vertcount;
	float lerp1, lerp2, lerp3, lerp4;
	const float *vertsbase, *verts1, *verts2, *verts3, *verts4;

	switch(whicharray)
	{
	case MODELARRAY_VERTEX:vertsbase = mesh->data_aliasvertex3f;break;
	case MODELARRAY_SVECTOR:vertsbase = mesh->data_aliassvector3f;break;
	case MODELARRAY_TVECTOR:vertsbase = mesh->data_aliastvector3f;break;
	case MODELARRAY_NORMAL:vertsbase = mesh->data_aliasnormal3f;break;
	default:
		Host_Error("R_Model_Alias_GetBlendedArray: unknown whicharray %i\n", whicharray);
		return;
	}

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
	int c, fullbright, layernum, firstpass;
	float tint[3], fog, ifog, colorscale, ambientcolor4f[4];
	vec3_t diff;
	qbyte *bcolor;
	rmeshstate_t m;
	const entity_render_t *ent = calldata1;
	aliasmesh_t *mesh = ent->model->aliasdata_meshes + calldata2;
	aliaslayer_t *layer;
	aliasskin_t *skin;
	rcachearrayrequest_t request;

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

	firstpass = true;
	skin = R_FetchAliasSkin(ent, mesh);
	for (layernum = 0, layer = skin->data_layers;layernum < skin->num_layers;layernum++, layer++)
	{
		if (!(layer->flags & ALIASLAYER_FORCEDRAW_IF_FIRSTPASS) || !firstpass)
		{
			if (((layer->flags & ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED) && ent->colormap < 0)
			 || ((layer->flags & ALIASLAYER_NODRAW_IF_COLORMAPPED) && ent->colormap >= 0)
			 || ((layer->flags & ALIASLAYER_FOG) && !fogenabled)
			 ||  (layer->flags & ALIASLAYER_SPECULAR)
			 || ((layer->flags & ALIASLAYER_DIFFUSE) && (r_shadow_realtime_world.integer && r_ambient.integer <= 0 && r_fullbright.integer == 0 && !(ent->effects & EF_FULLBRIGHT))))
				continue;
		}
		memset(&m, 0, sizeof(m));
		if (!firstpass || (ent->effects & EF_ADDITIVE))
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE;
		}
		else if ((skin->flags & ALIASSKIN_TRANSPARENT) || ent->alpha != 1.0)
		{
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ZERO;
		}
		firstpass = false;
		expandaliasvert(mesh->num_vertices);
		colorscale = r_colorscale;
		m.texrgbscale[0] = 1;
		m.tex[0] = R_GetTexture(layer->texture);
		if (gl_combine.integer && layer->flags & (ALIASLAYER_DIFFUSE | ALIASLAYER_SPECULAR))
		{
			colorscale *= 0.25f;
			m.texrgbscale[0] = 4;
		}
		c_alias_polys += mesh->num_triangles;
		if (gl_mesh_copyarrays.integer)
		{
			R_Mesh_State(&m);
			R_Mesh_GetSpace(mesh->num_vertices);
			if (layer->texture != NULL)
				R_Mesh_CopyTexCoord2f(0, mesh->data_texcoord2f, mesh->num_vertices);
			R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_VERTEX, varray_vertex3f);
		}
		else
		{
			m.pointervertexcount = mesh->num_vertices;
			memset(&request, 0, sizeof(request));
			request.data_size = mesh->num_vertices * sizeof(float[3]);
			request.id_pointer2 = mesh->data_aliasvertex3f;
			request.id_number1 = layernum;
			request.id_number2 = 0;
			request.id_number3 = CRC_Block((void *)ent->frameblend, sizeof(ent->frameblend));
			if (R_Mesh_CacheArray(&request))
				R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_VERTEX, request.data);
			m.pointer_vertex = request.data;
			m.pointer_texcoord[0] = layer->texture != NULL ? mesh->data_texcoord2f : NULL;
		}
		if (layer->flags & ALIASLAYER_FOG)
		{
			colorscale *= fog;
			GL_Color(fogcolor[0] * colorscale, fogcolor[1] * colorscale, fogcolor[2] * colorscale, ent->alpha);
		}
		else
		{
			if (layer->flags & (ALIASLAYER_COLORMAP_PANTS | ALIASLAYER_COLORMAP_SHIRT))
			{
				// 128-224 are backwards ranges
				if (layer->flags & ALIASLAYER_COLORMAP_PANTS)
					c = (ent->colormap & 0xF) << 4;
				else //if (layer->flags & ALIASLAYER_COLORMAP_SHIRT)
					c = (ent->colormap & 0xF0);
				c += (c >= 128 && c < 224) ? 4 : 12;
				bcolor = (qbyte *) (&palette_complete[c]);
				fullbright = c >= 224;
				VectorScale(bcolor, (1.0f / 255.0f), tint);
			}
			else
			{
				tint[0] = tint[1] = tint[2] = 1;
				fullbright = false;
			}
			colorscale *= ifog;
			if (fullbright || !(layer->flags & ALIASLAYER_DIFFUSE) || r_fullbright.integer || (ent->effects & EF_FULLBRIGHT))
				GL_Color(tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha);
			else if (r_shadow_realtime_world.integer)
			{
				colorscale *= r_ambient.value * (2.0f / 128.0f);
				GL_Color(tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha);
			}
			else
			{
				if (R_LightModel(ambientcolor4f, ent, tint[0] * colorscale, tint[1] * colorscale, tint[2] * colorscale, ent->alpha, false))
				{
					GL_UseColorArray();
					if (gl_mesh_copyarrays.integer)
					{
						R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_NORMAL, aliasvert_normal3f);
						R_LightModel_CalcVertexColors(ambientcolor4f, mesh->num_vertices, varray_vertex3f, aliasvert_normal3f, varray_color4f);
					}
					else
					{
						// request color4f cache
						request.data_size = mesh->num_vertices * sizeof(float[4]);
						request.id_pointer1 = ent;
						request.id_number2 = 2;
						request.id_number3 = CRC_Block((void *)ent->frameblend, sizeof(ent->frameblend)) + CRC_Block((void *)&ent->entlightstime, sizeof(ent->entlightstime));
						if (R_Mesh_CacheArray(&request))
						{
							// save off the color pointer before we blow away the request
							m.pointer_color = request.data;
							// request normal3f cache
							request.data_size = mesh->num_vertices * sizeof(float[3]);
							request.id_pointer1 = NULL;
							request.id_number2 = 3;
							request.id_number3 = CRC_Block((void *)ent->frameblend, sizeof(ent->frameblend));
							if (R_Mesh_CacheArray(&request))
								R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_NORMAL, request.data);
							R_LightModel_CalcVertexColors(ambientcolor4f, mesh->num_vertices, m.pointer_vertex, request.data, m.pointer_color);
						}
						else
							m.pointer_color = request.data;
					}
				}
				else
					GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
			}
		}
		if (!gl_mesh_copyarrays.integer)
			R_Mesh_State(&m);
		R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
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
		R_Mesh_GetSpace(mesh->num_vertices);
		R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_VERTEX, varray_vertex3f);
		for (i = 0, v = varray_vertex3f;i < mesh->num_vertices;i++, v += 3)
		{
			dist = DotProduct(v, planenormal) - planedist;
			if (dist > 0)
				VectorMA(v, dist, projection, v);
		}
		c_alias_polys += mesh->num_triangles;
		R_Mesh_Draw(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
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
			R_Mesh_GetSpace(mesh->num_vertices * 2);
			R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_VERTEX, varray_vertex3f);
			R_Shadow_Volume(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, mesh->data_neighbor3i, relativelightorigin, lightradius, projectdistance);
		}
	}
}

void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz)
{
	int c, meshnum, layernum;
	float fog, ifog, lightcolor2[3], *vertices;
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
		expandaliasvert(mesh->num_vertices);
		vertices = R_Shadow_VertexBuffer(mesh->num_vertices);
		R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_VERTEX, vertices);
		R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_SVECTOR, aliasvert_svector3f);
		R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_TVECTOR, aliasvert_tvector3f);
		R_Model_Alias_GetMesh_Array3f(ent, mesh, MODELARRAY_NORMAL, aliasvert_normal3f);
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
				R_Shadow_SpecularLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, vertices, aliasvert_svector3f, aliasvert_tvector3f, aliasvert_normal3f, mesh->data_texcoord2f, relativelightorigin, relativeeyeorigin, lightradius, lightcolor2, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, layer->texture, layer->nmap, NULL);
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
				R_Shadow_DiffuseLighting(mesh->num_vertices, mesh->num_triangles, mesh->data_element3i, vertices, aliasvert_svector3f, aliasvert_tvector3f, aliasvert_normal3f, mesh->data_texcoord2f, relativelightorigin, lightradius, lightcolor2, matrix_modeltofilter, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, layer->texture, layer->nmap, NULL);
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
		out += 3;
	}
}

void ZymoticCalcNormal3f(int vertcount, float *vertex3f, float *normal3f, int shadercount, int *renderlist)
{
	int a, b, c, d;
	float *out, v1[3], v2[3], normal[3], s;
	int *u;
	// clear normals
	memset(normal3f, 0, sizeof(float) * vertcount * 3);
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
			v1[0] = vertex3f[a+0] - vertex3f[b+0];
			v1[1] = vertex3f[a+1] - vertex3f[b+1];
			v1[2] = vertex3f[a+2] - vertex3f[b+2];
			v2[0] = vertex3f[c+0] - vertex3f[b+0];
			v2[1] = vertex3f[c+1] - vertex3f[b+1];
			v2[2] = vertex3f[c+2] - vertex3f[b+2];
			CrossProduct(v1, v2, normal);
			VectorNormalizeFast(normal);
			// add surface normal to vertices
			a = renderlist[0] * 3;
			normal3f[a+0] += normal[0];
			normal3f[a+1] += normal[1];
			normal3f[a+2] += normal[2];
			aliasvertusage[renderlist[0]]++;
			a = renderlist[1] * 3;
			normal3f[a+0] += normal[0];
			normal3f[a+1] += normal[1];
			normal3f[a+2] += normal[2];
			aliasvertusage[renderlist[1]]++;
			a = renderlist[2] * 3;
			normal3f[a+0] += normal[0];
			normal3f[a+1] += normal[1];
			normal3f[a+2] += normal[2];
			aliasvertusage[renderlist[2]]++;
			renderlist += 3;
		}
	}
	// FIXME: precalc this
	// average surface normals
	out = normal3f;
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
	float fog, ifog, colorscale, ambientcolor4f[4];
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

	expandaliasvert(numverts);

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

	R_Mesh_GetSpace(numverts);
	ZymoticTransformVerts(numverts, varray_vertex3f, ent->model->zymdata_vertbonecounts, ent->model->zymdata_verts);
	R_Mesh_CopyTexCoord2f(0, ent->model->zymdata_texcoords, ent->model->zymnum_verts);
	ZymoticCalcNormal3f(numverts, varray_vertex3f, aliasvert_normal3f, ent->model->zymnum_shaders, ent->model->zymdata_renderlist);
	if (R_LightModel(ambientcolor4f, ent, ifog * colorscale, ifog * colorscale, ifog * colorscale, ent->alpha, false))
	{
		GL_UseColorArray();
		R_LightModel_CalcVertexColors(ambientcolor4f, numverts, varray_vertex3f, aliasvert_normal3f, varray_color4f);
	}
	else
		GL_Color(ambientcolor4f[0], ambientcolor4f[1], ambientcolor4f[2], ambientcolor4f[3]);
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
		R_Mesh_GetSpace(numverts);
		ZymoticTransformVerts(numverts, varray_vertex3f, ent->model->zymdata_vertbonecounts, ent->model->zymdata_verts);
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

