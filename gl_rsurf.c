/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_shadow.h"

#define MAX_LIGHTMAP_SIZE 256

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};
cvar_t r_detailtextures = {CVAR_SAVE, "r_detailtextures", "1"};
cvar_t r_surfaceworldnode = {0, "r_surfaceworldnode", "0"};
cvar_t r_drawcollisionbrushes_polygonfactor = {0, "r_drawcollisionbrushes_polygonfactor", "-1"};
cvar_t r_drawcollisionbrushes_polygonoffset = {0, "r_drawcollisionbrushes_polygonoffset", "0"};
cvar_t r_q3bsp_renderskydepth = {0, "r_q3bsp_renderskydepth", "0"};
cvar_t gl_lightmaps = {0, "gl_lightmaps", "0"};

// flag arrays used for visibility checking on world model
// (all other entities have no per-surface/per-leaf visibility checks)
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_clusters
qbyte r_pvsbits[(32768+7)>>3];
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_leafs
qbyte r_worldleafvisible[32768];
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_surfaces
qbyte r_worldsurfacevisible[262144];

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void R_BuildLightMap (const entity_render_t *ent, msurface_t *surface)
{
	int smax, tmax, i, j, size, size3, maps, stride, l;
	unsigned int *bl, scale;
	qbyte *lightmap, *out, *stain;
	static unsigned int intblocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting
	static qbyte templight[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*4];

	// update cached lighting info
	surface->cached_dlight = 0;

	smax = (surface->extents[0]>>4)+1;
	tmax = (surface->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;
	lightmap = surface->samples;

// set to full bright if no light data
	bl = intblocklights;
	if (!ent->model->brushq1.lightdata)
	{
		for (i = 0;i < size3;i++)
			bl[i] = 255*256;
	}
	else
	{
// clear to no light
		memset(bl, 0, size*3*sizeof(unsigned int));

// add all the lightmaps
		if (lightmap)
		{
			bl = intblocklights;
			for (maps = 0;maps < MAXLIGHTMAPS && surface->styles[maps] != 255;maps++, lightmap += size3)
				for (scale = d_lightstylevalue[surface->styles[maps]], i = 0;i < size3;i++)
					bl[i] += lightmap[i] * scale;
		}
	}

	stain = surface->stainsamples;
	bl = intblocklights;
	out = templight;
	// the >> 16 shift adjusts down 8 bits to account for the stainmap
	// scaling, and remaps the 0-65536 (2x overbright) to 0-256, it will
	// be doubled during rendering to achieve 2x overbright
	// (0 = 0.0, 128 = 1.0, 256 = 2.0)
	if (ent->model->brushq1.lightmaprgba)
	{
		stride = (surface->lightmaptexturestride - smax) * 4;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				*out++ = 255;
			}
		}
	}
	else
	{
		stride = (surface->lightmaptexturestride - smax) * 3;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
				l = (*bl++ * *stain++) >> 16;*out++ = min(l, 255);
			}
		}
	}

	R_UpdateTexture(surface->lightmaptexture, templight);
}

void R_StainNode (mnode_t *node, model_t *model, const vec3_t origin, float radius, const float fcolor[8])
{
	float ndist, a, ratio, maxdist, maxdist2, maxdist3, invradius, sdtable[256], td, dist2;
	msurface_t *surface, *endsurface;
	int i, s, t, smax, tmax, smax3, impacts, impactt, stained;
	qbyte *bl;
	vec3_t impact;

	maxdist = radius * radius;
	invradius = 1.0f / radius;

loc0:
	if (!node->plane)
		return;
	ndist = PlaneDiff(origin, node->plane);
	if (ndist > radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -radius)
	{
		node = node->children[1];
		goto loc0;
	}

	dist2 = ndist * ndist;
	maxdist3 = maxdist - dist2;

	if (node->plane->type < 3)
	{
		VectorCopy(origin, impact);
		impact[node->plane->type] -= ndist;
	}
	else
	{
		impact[0] = origin[0] - node->plane->normal[0] * ndist;
		impact[1] = origin[1] - node->plane->normal[1] * ndist;
		impact[2] = origin[2] - node->plane->normal[2] * ndist;
	}

	for (surface = model->brush.data_surfaces + node->firstsurface, endsurface = surface + node->numsurfaces;surface < endsurface;surface++)
	{
		if (surface->stainsamples)
		{
			smax = (surface->extents[0] >> 4) + 1;
			tmax = (surface->extents[1] >> 4) + 1;

			impacts = DotProduct (impact, surface->texinfo->vecs[0]) + surface->texinfo->vecs[0][3] - surface->texturemins[0];
			impactt = DotProduct (impact, surface->texinfo->vecs[1]) + surface->texinfo->vecs[1][3] - surface->texturemins[1];

			s = bound(0, impacts, smax * 16) - impacts;
			t = bound(0, impactt, tmax * 16) - impactt;
			i = s * s + t * t + dist2;
			if (i > maxdist)
				continue;

			// reduce calculations
			for (s = 0, i = impacts; s < smax; s++, i -= 16)
				sdtable[s] = i * i + dist2;

			bl = surface->stainsamples;
			smax3 = smax * 3;
			stained = false;

			i = impactt;
			for (t = 0;t < tmax;t++, i -= 16)
			{
				td = i * i;
				// make sure some part of it is visible on this line
				if (td < maxdist3)
				{
					maxdist2 = maxdist - td;
					for (s = 0;s < smax;s++)
					{
						if (sdtable[s] < maxdist2)
						{
							ratio = lhrandom(0.0f, 1.0f);
							a = (fcolor[3] + ratio * fcolor[7]) * (1.0f - sqrt(sdtable[s] + td) * invradius);
							if (a >= (1.0f / 64.0f))
							{
								if (a > 1)
									a = 1;
								bl[0] = (qbyte) ((float) bl[0] + a * ((fcolor[0] + ratio * fcolor[4]) - (float) bl[0]));
								bl[1] = (qbyte) ((float) bl[1] + a * ((fcolor[1] + ratio * fcolor[5]) - (float) bl[1]));
								bl[2] = (qbyte) ((float) bl[2] + a * ((fcolor[2] + ratio * fcolor[6]) - (float) bl[2]));
								stained = true;
							}
						}
						bl += 3;
					}
				}
				else // skip line
					bl += smax3;
			}
			// force lightmap upload
			if (stained)
				surface->cached_dlight = true;
		}
	}

	if (node->children[0]->plane)
	{
		if (node->children[1]->plane)
		{
			R_StainNode(node->children[0], model, origin, radius, fcolor);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->plane)
	{
		node = node->children[1];
		goto loc0;
	}
}

void R_Stain (const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	int n;
	float fcolor[8];
	entity_render_t *ent;
	model_t *model;
	vec3_t org;
	if (r_refdef.worldmodel == NULL || !r_refdef.worldmodel->brush.data_nodes)
		return;
	fcolor[0] = cr1;
	fcolor[1] = cg1;
	fcolor[2] = cb1;
	fcolor[3] = ca1 * (1.0f / 64.0f);
	fcolor[4] = cr2 - cr1;
	fcolor[5] = cg2 - cg1;
	fcolor[6] = cb2 - cb1;
	fcolor[7] = (ca2 - ca1) * (1.0f / 64.0f);

	R_StainNode(r_refdef.worldmodel->brush.data_nodes + r_refdef.worldmodel->brushq1.hulls[0].firstclipnode, r_refdef.worldmodel, origin, radius, fcolor);

	// look for embedded bmodels
	for (n = 0;n < cl_num_brushmodel_entities;n++)
	{
		ent = cl_brushmodel_entities[n];
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			Mod_CheckLoaded(model);
			if (model->brush.data_nodes)
			{
				Matrix4x4_Transform(&ent->inversematrix, origin, org);
				R_StainNode(model->brush.data_nodes + model->brushq1.hulls[0].firstclipnode, model, org, radius, fcolor);
			}
		}
	}
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

static float *RSurf_GetVertexPointer(const entity_render_t *ent, const msurface_t *surface)
{
	if (surface->texture->textureflags & (Q3TEXTUREFLAG_AUTOSPRITE | Q3TEXTUREFLAG_AUTOSPRITE2))
	{
		texture_t *texture = surface->texture;
		int i, j;
		float center[3], center2[3], forward[3], right[3], up[3], v[4][3];
		matrix4x4_t matrix1, imatrix1;
		R_Mesh_Matrix(&r_identitymatrix);
		// a single autosprite surface can contain multiple sprites...
		for (j = 0;j < surface->mesh.num_vertices - 3;j += 4)
		{
			VectorClear(center);
			for (i = 0;i < 4;i++)
				VectorAdd(center, surface->mesh.data_vertex3f + (j+i) * 3, center);
			VectorScale(center, 0.25f, center);
			Matrix4x4_Transform(&ent->matrix, center, center2);
			// FIXME: calculate vectors from triangle edges instead of using texture vectors as an easy way out?
			Matrix4x4_FromVectors(&matrix1, surface->mesh.data_normal3f + j*3, surface->mesh.data_svector3f + j*3, surface->mesh.data_tvector3f + j*3, center);
			Matrix4x4_Invert_Simple(&imatrix1, &matrix1);
			for (i = 0;i < 4;i++)
				Matrix4x4_Transform(&imatrix1, surface->mesh.data_vertex3f + (j+i)*3, v[i]);
			if (texture->textureflags & Q3TEXTUREFLAG_AUTOSPRITE2)
			{
				forward[0] = r_vieworigin[0] - center2[0];
				forward[1] = r_vieworigin[1] - center2[1];
				forward[2] = 0;
				VectorNormalize(forward);
				right[0] = forward[1];
				right[1] = -forward[0];
				right[2] = 0;
				up[0] = 0;
				up[1] = 0;
				up[2] = 1;
			}
			else
			{
				VectorCopy(r_viewforward, forward);
				VectorCopy(r_viewright, right);
				VectorCopy(r_viewup, up);
			}
			for (i = 0;i < 4;i++)
				VectorMAMAMAM(1, center2, v[i][0], forward, v[i][1], right, v[i][2], up, varray_vertex3f + (i+j) * 3);
		}
		return varray_vertex3f;
	}
	else
		return surface->mesh.data_vertex3f;
}

void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t)
{
	// we don't need to set currentframe if t->animated is false because
	// it was already set up by the texture loader for non-animating
	if (t->animated)
	{
		t->currentframe = t->anim_frames[ent->frame != 0][(t->anim_total[ent->frame != 0] >= 2) ? ((int)(r_refdef.time * 5.0f) % t->anim_total[ent->frame != 0]) : 0];
		t = t->currentframe;
	}
	t->currentmaterialflags = t->basematerialflags;
	t->currentalpha = ent->alpha;
	if (t->basematerialflags & MATERIALFLAG_WATERALPHA)
		t->currentalpha *= r_wateralpha.value;
	if (!(ent->flags & RENDER_LIGHT))
		t->currentmaterialflags |= MATERIALFLAG_FULLBRIGHT;
	if (ent->effects & EF_ADDITIVE)
		t->currentmaterialflags |= MATERIALFLAG_ADD | MATERIALFLAG_TRANSPARENT;
	else if (t->currentalpha < 1)
		t->currentmaterialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_TRANSPARENT;
}

matrix4x4_t r_surf_waterscrollmatrix;

void R_UpdateAllTextureInfo(entity_render_t *ent)
{
	int i;
	Matrix4x4_CreateTranslate(&r_surf_waterscrollmatrix, sin(r_refdef.time) * 0.025 * r_waterscroll.value, sin(r_refdef.time * 0.8f) * 0.025 * r_waterscroll.value, 0);
	if (ent->model)
		for (i = 0;i < ent->model->brush.num_textures;i++)
			R_UpdateTextureInfo(ent, ent->model->brush.data_textures + i);
}

static void R_DrawSurfaceList(const entity_render_t *ent, texture_t *texture, int texturenumsurfaces, const msurface_t **texturesurfacelist, const vec3_t modelorg)
{
	int i;
	int texturesurfaceindex;
	const float *v;
	float *c;
	float diff[3];
	float f, r, g, b, a, base, colorscale;
	const msurface_t *surface;
	qboolean dolightmap;
	qboolean dobase;
	qboolean doambient;
	qboolean dodetail;
	qboolean doglow;
	qboolean dofogpass;
	qboolean fogallpasses;
	qboolean waterscrolling;
	rmeshstate_t m;
	texture = texture->currentframe;
	if (texture->currentmaterialflags & MATERIALFLAG_NODRAW)
		return;
	c_faces += texturenumsurfaces;
	// gl_lightmaps debugging mode skips normal texturing
	if (gl_lightmaps.integer)
	{
		GL_BlendFunc(GL_ONE, GL_ZERO);
		GL_DepthMask(true);
		GL_DepthTest(true);
		qglDisable(GL_CULL_FACE);
		GL_Color(1, 1, 1, 1);
		memset(&m, 0, sizeof(m));
		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			surface = texturesurfacelist[texturesurfaceindex];
			m.tex[0] = R_GetTexture(surface->lightmaptexture);
			m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
			if (surface->lightmaptexture)
			{
				GL_Color(1, 1, 1, 1);
				m.pointer_color = NULL;
			}
			else
				m.pointer_color = surface->mesh.data_lightmapcolor4f;
			m.pointer_vertex = surface->mesh.data_vertex3f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surface->mesh.num_vertices);
			R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}
		qglEnable(GL_CULL_FACE);
		return;
	}
	GL_DepthTest(!(texture->currentmaterialflags & MATERIALFLAG_NODEPTHTEST));
	GL_DepthMask(!(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT));
	if (texture->currentmaterialflags & MATERIALFLAG_ADD)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	else if (texture->currentmaterialflags & MATERIALFLAG_ALPHA)
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else
		GL_BlendFunc(GL_ONE, GL_ZERO);
	// water waterscrolling in texture matrix
	waterscrolling = (texture->currentmaterialflags & MATERIALFLAG_WATER) && r_waterscroll.value != 0;
	if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglDisable(GL_CULL_FACE);
	if (texture->currentmaterialflags & MATERIALFLAG_SKY)
	{
		if (skyrendernow)
		{
			skyrendernow = false;
			if (skyrendermasked)
				R_Sky();
		}
		// LordHavoc: HalfLife maps have freaky skypolys...
		if (!ent->model->brush.ishlbsp)
		{
			R_Mesh_Matrix(&ent->matrix);
			GL_Color(fogcolor[0], fogcolor[1], fogcolor[2], 1);
			if (skyrendermasked)
			{
				// depth-only (masking)
				GL_ColorMask(0,0,0,0);
				// just to make sure that braindead drivers don't draw anything
				// despite that colormask...
				GL_BlendFunc(GL_ZERO, GL_ONE);
			}
			else
			{
				// fog sky
				GL_BlendFunc(GL_ONE, GL_ZERO);
			}
			GL_DepthMask(true);
			GL_DepthTest(true);
			memset(&m, 0, sizeof(m));
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = surface->mesh.data_vertex3f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
			GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);
		}
	}
	else if ((texture->currentmaterialflags & MATERIALFLAG_WATER) && r_watershader.value && gl_textureshader && !texture->skin.glow && !fogenabled && ent->colormod[0] == 1 && ent->colormod[1] == 1 && ent->colormod[2] == 1)
	{
		// NVIDIA Geforce3 distortion texture shader on water
		float args[4] = {0.05f,0,0,0.04f};
		memset(&m, 0, sizeof(m));
		m.tex[0] = R_GetTexture(mod_shared_distorttexture[(int)(r_refdef.time * 16)&63]);
		m.tex[1] = R_GetTexture(texture->skin.base);
		m.texcombinergb[0] = GL_REPLACE;
		m.texcombinergb[1] = GL_REPLACE;
		Matrix4x4_CreateFromQuakeEntity(&m.texmatrix[0], 0, 0, 0, 0, 0, 0, r_watershader.value);
		m.texmatrix[1] = r_surf_waterscrollmatrix;

		GL_ActiveTexture(0);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(1);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_OFFSET_TEXTURE_2D_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
		qglTexEnvfv(GL_TEXTURE_SHADER_NV, GL_OFFSET_TEXTURE_MATRIX_NV, &args[0]);
		qglEnable(GL_TEXTURE_SHADER_NV);

		for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
		{
			surface = texturesurfacelist[texturesurfaceindex];
			m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
			m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
			m.pointer_texcoord[1] = surface->mesh.data_texcoordtexture2f;
			R_Mesh_State(&m);
			GL_LockArrays(0, surface->mesh.num_vertices);
			R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
			GL_LockArrays(0, 0);
		}

		qglDisable(GL_TEXTURE_SHADER_NV);
		qglTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
		GL_ActiveTexture(0);
	}
	else if (texture->currentmaterialflags & (MATERIALFLAG_WATER | MATERIALFLAG_WALL))
	{
		// normal surface (wall or water)
		dobase = true;
		dolightmap = !(texture->currentmaterialflags & MATERIALFLAG_FULLBRIGHT);
		doambient = r_ambient.value >= (1/64.0f);
		dodetail = texture->skin.detail != NULL && !(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT);
		doglow = texture->skin.glow != NULL;
		dofogpass = fogenabled && !(texture->currentmaterialflags & MATERIALFLAG_ADD);
		fogallpasses = fogenabled && !(texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT);
		if (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT)
		{
			if (dobase && dolightmap && gl_combine.integer)
			{
				dobase = false;
				memset(&m, 0, sizeof(m));
				m.tex[1] = R_GetTexture(texture->skin.base);
				if (waterscrolling)
					m.texmatrix[1] = r_surf_waterscrollmatrix;
				m.texrgbscale[1] = 2;
				m.pointer_color = varray_color4f;
				colorscale = 1;
				r = ent->colormod[0] * colorscale;
				g = ent->colormod[1] * colorscale;
				b = ent->colormod[2] * colorscale;
				a = texture->currentalpha;
				base = r_ambient.value * (1.0f / 64.0f);
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
					m.pointer_texcoord[1] = surface->mesh.data_texcoordtexture2f;
					if (surface->lightmaptexture)
					{
						m.tex[0] = R_GetTexture(surface->lightmaptexture);
						if (fogallpasses)
						{
							m.pointer_color = varray_color4f;
							for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
							{
								VectorSubtract(v, modelorg, diff);
								f = 1 - exp(fogdensity/DotProduct(diff, diff));
								c[0] = f * r;
								c[1] = f * g;
								c[2] = f * b;
								c[3] = a;
							}
						}
						else
						{
							m.pointer_color = NULL;
							GL_Color(r, g, b, a);
						}
					}
					else
					{
						m.tex[0] = R_GetTexture(r_texture_white);
						m.pointer_color = varray_color4f;
						if (surface->styles[0] != 255)
						{
							for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
							{
								c[0] = 0;
								c[1] = 0;
								c[2] = 0;
								if (surface->styles[0] != 255)
								{
									if (surface->mesh.data_lightmapcolor4f)
									{
										float scale = d_lightstylevalue[surface->styles[0]] * (1.0f / 128.0f);
										VectorMA(c, scale, surface->mesh.data_lightmapcolor4f + i*4, c);
									}
									else if (surface->mesh.data_lightmapoffsets)
									{
										const qbyte *lm = surface->samples + surface->mesh.data_lightmapoffsets[i];
										float scale = d_lightstylevalue[surface->styles[0]] * (1.0f / 32768.0f);
										VectorMA(c, scale, lm, c);
										if (surface->styles[1] != 255)
										{
											int size3 = ((surface->extents[0]>>4)+1)*((surface->extents[1]>>4)+1)*3;
											lm += size3;
											scale = d_lightstylevalue[surface->styles[1]] * (1.0f / 32768.0f);
											VectorMA(c, scale, lm, c);
											if (surface->styles[2] != 255)
											{
												lm += size3;
												scale = d_lightstylevalue[surface->styles[2]] * (1.0f / 32768.0f);
												VectorMA(c, scale, lm, c);
												if (surface->styles[3] != 255)
												{
													lm += size3;
													scale = d_lightstylevalue[surface->styles[3]] * (1.0f / 32768.0f);
													VectorMA(c, scale, lm, c);
												}
											}
										}
									}
								}
								c[0] *= r;
								c[1] *= g;
								c[2] *= b;
								if (fogallpasses)
								{
									VectorSubtract(v, modelorg, diff);
									f = 1 - exp(fogdensity/DotProduct(diff, diff));
									VectorScale(c, f, c);
								}
								if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
									c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
								else
									c[3] = a;
							}
						}
						else
						{
							if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
							{
								for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
								{
									c[0] = 0;
									c[1] = 0;
									c[2] = 0;
									c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
								}
							}
							else
							{
								m.pointer_color = NULL;
								GL_Color(0, 0, 0, a);
							}
						}
					}
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			if (dobase)
			{
				dobase = false;
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(texture->skin.base);
				if (waterscrolling)
					m.texmatrix[0] = r_surf_waterscrollmatrix;
				m.texmatrix[0] = r_surf_waterscrollmatrix;
				m.pointer_color = varray_color4f;
				colorscale = 1;
				if (gl_combine.integer)
				{
					m.texrgbscale[0] = 4;
					colorscale *= 0.25f;
				}
				r = ent->colormod[0] * colorscale;
				g = ent->colormod[1] * colorscale;
				b = ent->colormod[2] * colorscale;
				a = texture->currentalpha;
				if (dolightmap)
				{
					for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
					{
						surface = texturesurfacelist[texturesurfaceindex];
						m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
						m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
						for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
						{
							c[0] = 0;
							c[1] = 0;
							c[2] = 0;
							if (surface->styles[0] != 255)
							{
								if (surface->mesh.data_lightmapcolor4f)
								{
									float scale = d_lightstylevalue[surface->styles[0]] * (1.0f / 128.0f);
									VectorMA(c, scale, surface->mesh.data_lightmapcolor4f + i*4, c);
								}
								else if (surface->mesh.data_lightmapoffsets)
								{
									const qbyte *lm = surface->samples + surface->mesh.data_lightmapoffsets[i];
									float scale = d_lightstylevalue[surface->styles[0]] * (1.0f / 32768.0f);
									VectorMA(c, scale, lm, c);
									if (surface->styles[1] != 255)
									{
										int size3 = ((surface->extents[0]>>4)+1)*((surface->extents[1]>>4)+1)*3;
										lm += size3;
										scale = d_lightstylevalue[surface->styles[1]] * (1.0f / 32768.0f);
										VectorMA(c, scale, lm, c);
										if (surface->styles[2] != 255)
										{
											lm += size3;
											scale = d_lightstylevalue[surface->styles[2]] * (1.0f / 32768.0f);
											VectorMA(c, scale, lm, c);
											if (surface->styles[3] != 255)
											{
												lm += size3;
												scale = d_lightstylevalue[surface->styles[3]] * (1.0f / 32768.0f);
												VectorMA(c, scale, lm, c);
											}
										}
									}
								}
							}
							c[0] *= r;
							c[1] *= g;
							c[2] *= b;
							if (fogallpasses)
							{
								VectorSubtract(v, modelorg, diff);
								f = 1 - exp(fogdensity/DotProduct(diff, diff));
								VectorScale(c, f, c);
							}
							if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
								c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
							else
								c[3] = a;
						}
						R_Mesh_State(&m);
						GL_LockArrays(0, surface->mesh.num_vertices);
						R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
						GL_LockArrays(0, 0);
					}
				}
				else
				{
					if (fogallpasses)
					{
						for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
						{
							surface = texturesurfacelist[texturesurfaceindex];
							m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
							m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
							if (m.tex[1])
								m.pointer_texcoord[1] = surface->mesh.data_texcoordtexture2f;
							if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
							{
								m.pointer_color = varray_color4f;
								for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
								{
									VectorSubtract(v, modelorg, diff);
									f = 1 - exp(fogdensity/DotProduct(diff, diff));
									c[0] = r * f;
									c[1] = g * f;
									c[2] = b * f;
									c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
								}
							}
							else
							{
								m.pointer_color = varray_color4f;
								for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
								{
									VectorSubtract(v, modelorg, diff);
									f = 1 - exp(fogdensity/DotProduct(diff, diff));
									c[0] = r * f;
									c[1] = g * f;
									c[2] = b * f;
									c[3] = a;
								}
							}
							R_Mesh_State(&m);
							GL_LockArrays(0, surface->mesh.num_vertices);
							R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
							GL_LockArrays(0, 0);
						}
					}
					else
					{
						for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
						{
							surface = texturesurfacelist[texturesurfaceindex];
							m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
							m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
							if (m.tex[1])
								m.pointer_texcoord[1] = surface->mesh.data_texcoordtexture2f;
							if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
							{
								m.pointer_color = varray_color4f;
								for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
								{
									c[0] = r;
									c[1] = g;
									c[2] = b;
									c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
								}
							}
							else
							{
								m.pointer_color = NULL;
								GL_Color(r, g, b, a);
							}
							R_Mesh_State(&m);
							GL_LockArrays(0, surface->mesh.num_vertices);
							R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
							GL_LockArrays(0, 0);
						}
					}
				}
			}
		}
		else
		{
			if (!dolightmap && dobase)
			{
				dolightmap = false;
				dobase = false;
				GL_Color(ent->colormod[0], ent->colormod[1], ent->colormod[2], 1);
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(texture->skin.base);
				if (waterscrolling)
					m.texmatrix[0] = r_surf_waterscrollmatrix;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			if (r_lightmapintensity <= 0 && dolightmap && dobase)
			{
				dolightmap = false;
				dobase = false;
				GL_Color(0, 0, 0, 1);
				memset(&m, 0, sizeof(m));
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			if (r_textureunits.integer >= 2 && gl_combine.integer && dolightmap && dobase)
			{
				// dualtexture combine
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_DepthMask(true);
				dolightmap = false;
				dobase = false;
				memset(&m, 0, sizeof(m));
				m.tex[1] = R_GetTexture(texture->skin.base);
				if (waterscrolling)
					m.texmatrix[1] = r_surf_waterscrollmatrix;
				m.texrgbscale[1] = 2;
				r = ent->colormod[0] * r_lightmapintensity;
				g = ent->colormod[1] * r_lightmapintensity;
				b = ent->colormod[2] * r_lightmapintensity;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					memset(&m, 0, sizeof(m));
					m.tex[1] = R_GetTexture(texture->skin.base);
					if (waterscrolling)
						m.texmatrix[1] = r_surf_waterscrollmatrix;
					m.texrgbscale[1] = 2;
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
					m.pointer_texcoord[1] = surface->mesh.data_texcoordtexture2f;
					if (surface->lightmaptexture)
					{
						m.tex[0] = R_GetTexture(surface->lightmaptexture);
						m.pointer_color = NULL;
						GL_Color(r, g, b, 1);
					}
					else if (r == 1 && g == 1 && b == 1)
					{
						m.tex[0] = R_GetTexture(r_texture_white);
						m.pointer_color = surface->mesh.data_lightmapcolor4f;
					}
					else
					{
						m.tex[0] = R_GetTexture(r_texture_white);
						m.pointer_color = varray_color4f;
						for (i = 0;i < surface->mesh.num_vertices;i++)
						{
							varray_color4f[i*4+0] = surface->mesh.data_lightmapcolor4f[i*4+0] * r;
							varray_color4f[i*4+1] = surface->mesh.data_lightmapcolor4f[i*4+1] * g;
							varray_color4f[i*4+2] = surface->mesh.data_lightmapcolor4f[i*4+2] * b;
							varray_color4f[i*4+3] = surface->mesh.data_lightmapcolor4f[i*4+3];
						}
					}
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			// single texture
			if (dolightmap)
			{
				GL_BlendFunc(GL_ONE, GL_ZERO);
				GL_DepthMask(true);
				GL_Color(1, 1, 1, 1);
				memset(&m, 0, sizeof(m));
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.tex[0] = R_GetTexture(surface->lightmaptexture);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordlightmap2f;
					if (surface->lightmaptexture)
						m.pointer_color = NULL;
					else
						m.pointer_color = surface->mesh.data_lightmapcolor4f;
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			if (dobase)
			{
				GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
				GL_DepthMask(false);
				GL_Color(r_lightmapintensity * ent->colormod[0], r_lightmapintensity * ent->colormod[1], r_lightmapintensity * ent->colormod[2], 1);
				memset(&m, 0, sizeof(m));
				m.tex[0] = R_GetTexture(texture->skin.base);
				if (waterscrolling)
					m.texmatrix[0] = r_surf_waterscrollmatrix;
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
		}
		if (doambient)
		{
			doambient = false;
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.base);
			if (waterscrolling)
				m.texmatrix[0] = r_surf_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 1;
			if (gl_combine.integer)
			{
				m.texrgbscale[0] = 4;
				colorscale *= 0.25f;
			}
			base = r_ambient.value * (1.0f / 64.0f);
			r = ent->colormod[0] * colorscale * base;
			g = ent->colormod[1] * colorscale * base;
			b = ent->colormod[2] * colorscale * base;
			a = texture->currentalpha;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
				{
					c[0] = r;
					c[1] = g;
					c[2] = b;
					if (fogallpasses)
					{
						VectorSubtract(v, modelorg, diff);
						f = 1 - exp(fogdensity/DotProduct(diff, diff));
						VectorScale(c, f, c);
					}
					if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
						c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
					else
						c[3] = a;
				}
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (dodetail)
		{
			GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			GL_DepthMask(false);
			GL_Color(1, 1, 1, 1);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.detail);
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
				m.pointer_texcoord[0] = surface->mesh.data_texcoorddetail2f;
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
		if (doglow)
		{
			// if glow was not already done using multitexture, do it now.
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.glow);
			if (waterscrolling)
				m.texmatrix[0] = r_surf_waterscrollmatrix;
			m.pointer_color = varray_color4f;
			colorscale = 1;
			r = ent->colormod[0] * colorscale;
			g = ent->colormod[1] * colorscale;
			b = ent->colormod[2] * colorscale;
			a = texture->currentalpha;
			if (fogallpasses)
			{
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
					if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
					{
						m.pointer_color = varray_color4f;
						for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
						{
							VectorSubtract(v, modelorg, diff);
							f = 1 - exp(fogdensity/DotProduct(diff, diff));
							c[0] = f * r;
							c[1] = f * g;
							c[2] = f * b;
							c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
						}
					}
					else
					{
						m.pointer_color = varray_color4f;
						for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
						{
							VectorSubtract(v, modelorg, diff);
							f = 1 - exp(fogdensity/DotProduct(diff, diff));
							c[0] = f * r;
							c[1] = f * g;
							c[2] = f * b;
							c[3] = a;
						}
					}
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
			else
			{
				for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
				{
					surface = texturesurfacelist[texturesurfaceindex];
					m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
					m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
					if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
					{
						m.pointer_color = varray_color4f;
						for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
						{
							c[0] = r;
							c[1] = g;
							c[2] = b;
							c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * a;
						}
					}
					else
					{
						m.pointer_color = NULL;
						GL_Color(r, g, b, a);
					}
					R_Mesh_State(&m);
					GL_LockArrays(0, surface->mesh.num_vertices);
					R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
					GL_LockArrays(0, 0);
				}
			}
		}
		if (dofogpass)
		{
			// if this is opaque use alpha blend which will darken the earlier
			// passes cheaply.
			//
			// if this is an alpha blended material, all the earlier passes
			// were darkened by fog already, so we only need to add the fog
			// color ontop through the fog mask texture
			//
			// if this is an additive blended material, all the earlier passes
			// were darkened by fog already, and we should not add fog color
			// (because the background was not darkened, there is no fog color
			// that was lost behind it).
			if (!fogallpasses)
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			GL_DepthMask(false);
			memset(&m, 0, sizeof(m));
			m.tex[0] = R_GetTexture(texture->skin.fog);
			if (waterscrolling)
				m.texmatrix[0] = r_surf_waterscrollmatrix;
			r = fogcolor[0];
			g = fogcolor[1];
			b = fogcolor[2];
			a = texture->currentalpha;
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				m.pointer_vertex = RSurf_GetVertexPointer(ent, surface);
				m.pointer_texcoord[0] = surface->mesh.data_texcoordtexture2f;
				m.pointer_color = varray_color4f;
				//RSurf_FogPassColors_Vertex3f_Color4f(surface->mesh.data_vertex3f, varray_color4f, fogcolor[0], fogcolor[1], fogcolor[2], texture->currentalpha, 1, surface->mesh.num_vertices, modelorg);
				if (surface->mesh.data_lightmapcolor4f && (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT))
				{
					m.pointer_color = varray_color4f;
					for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
					{
						VectorSubtract(v, modelorg, diff);
						f = exp(fogdensity/DotProduct(diff, diff));
						c[0] = r;
						c[1] = g;
						c[2] = b;
						c[3] = surface->mesh.data_lightmapcolor4f[i*4+3] * f * a;
					}
				}
				else
				{
					m.pointer_color = varray_color4f;
					for (i = 0, v = m.pointer_vertex, c = varray_color4f;i < surface->mesh.num_vertices;i++, v += 3, c += 4)
					{
						VectorSubtract(v, modelorg, diff);
						f = exp(fogdensity/DotProduct(diff, diff));
						c[0] = r;
						c[1] = g;
						c[2] = b;
						c[3] = f * a;
					}
				}
				R_Mesh_State(&m);
				GL_LockArrays(0, surface->mesh.num_vertices);
				R_Mesh_Draw(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i);
				GL_LockArrays(0, 0);
			}
		}
	}
	if (texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
		qglEnable(GL_CULL_FACE);
}

static void RSurfShader_Transparent_Callback(const void *calldata1, int calldata2)
{
	const entity_render_t *ent = calldata1;
	const msurface_t *surface = ent->model->brush.data_surfaces + calldata2;
	vec3_t modelorg;
	texture_t *texture;

	texture = surface->texture;
	if (texture->basematerialflags & MATERIALFLAG_SKY)
		return; // transparent sky is too difficult
	R_UpdateTextureInfo(ent, texture);

	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);
	R_DrawSurfaceList(ent, texture, 1, &surface, modelorg);
}

void R_QueueSurfaceList(entity_render_t *ent, texture_t *texture, int texturenumsurfaces, const msurface_t **texturesurfacelist, const vec3_t modelorg)
{
	int texturesurfaceindex;
	const msurface_t *surface;
	vec3_t tempcenter, center;
	if (texture->currentmaterialflags & MATERIALFLAG_TRANSPARENT)
	{
		// drawing sky transparently would be too difficult
		if (!(texture->currentmaterialflags & MATERIALFLAG_SKY))
		{
			for (texturesurfaceindex = 0;texturesurfaceindex < texturenumsurfaces;texturesurfaceindex++)
			{
				surface = texturesurfacelist[texturesurfaceindex];
				tempcenter[0] = (surface->mins[0] + surface->maxs[0]) * 0.5f;
				tempcenter[1] = (surface->mins[1] + surface->maxs[1]) * 0.5f;
				tempcenter[2] = (surface->mins[2] + surface->maxs[2]) * 0.5f;
				Matrix4x4_Transform(&ent->matrix, tempcenter, center);
				R_MeshQueue_AddTransparent(ent->effects & EF_NODEPTHTEST ? r_vieworigin : center, RSurfShader_Transparent_Callback, ent, surface - ent->model->brush.data_surfaces);
			}
		}
	}
	else
		R_DrawSurfaceList(ent, texture, texturenumsurfaces, texturesurfacelist, modelorg);
}

void R_DrawSurfaces(entity_render_t *ent, qboolean skysurfaces)
{
	int i, j, f, flagsmask;
	msurface_t *surface, **surfacechain;
	texture_t *t, *texture;
	model_t *model = ent->model;
	vec3_t modelorg;
	const int maxsurfacelist = 1024;
	int numsurfacelist = 0;
	const msurface_t *surfacelist[1024];
	if (model == NULL)
		return;
	R_Mesh_Matrix(&ent->matrix);
	Matrix4x4_Transform(&ent->inversematrix, r_vieworigin, modelorg);

	// update light styles
	if (!skysurfaces)
	{
		for (i = 0;i < model->brushq1.light_styles;i++)
		{
			if (model->brushq1.light_stylevalue[i] != d_lightstylevalue[model->brushq1.light_style[i]])
			{
				model->brushq1.light_stylevalue[i] = d_lightstylevalue[model->brushq1.light_style[i]];
				if ((surfacechain = model->brushq1.light_styleupdatechains[i]))
					for (;(surface = *surfacechain);surfacechain++)
						surface->cached_dlight = true;
			}
		}
	}

	R_UpdateAllTextureInfo(ent);
	flagsmask = skysurfaces ? MATERIALFLAG_SKY : (MATERIALFLAG_WATER | MATERIALFLAG_WALL);
	f = 0;
	t = NULL;
	texture = NULL;
	numsurfacelist = 0;
	for (i = 0, j = model->firstmodelsurface;i < model->nummodelsurfaces;i++, j++)
	{
		if (ent != r_refdef.worldentity || r_worldsurfacevisible[j])
		{
			surface = model->brush.data_surfaces + j;
			if (t != surface->texture)
			{
				if (numsurfacelist)
				{
					R_QueueSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
				t = surface->texture;
				f = t->currentmaterialflags & flagsmask;
				texture = t->currentframe;
			}
			if (f)
			{
				// if lightmap parameters changed, rebuild lightmap texture
				if (surface->cached_dlight && surface->samples)
					R_BuildLightMap(ent, surface);
				// add face to draw list
				surfacelist[numsurfacelist++] = surface;
				if (numsurfacelist >= maxsurfacelist)
				{
					R_QueueSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
					numsurfacelist = 0;
				}
			}
		}
	}
	if (numsurfacelist)
		R_QueueSurfaceList(ent, texture, numsurfacelist, surfacelist, modelorg);
}

static void R_DrawPortal_Callback(const void *calldata1, int calldata2)
{
	int i;
	float *v;
	rmeshstate_t m;
	const mportal_t *portal = calldata1;
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(false);
	GL_DepthTest(true);
	R_Mesh_Matrix(&r_identitymatrix);

	memset(&m, 0, sizeof(m));
	m.pointer_vertex = varray_vertex3f;
	R_Mesh_State(&m);

	i = calldata2;
	GL_Color(((i & 0x0007) >> 0) * (1.0f / 7.0f),
			 ((i & 0x0038) >> 3) * (1.0f / 7.0f),
			 ((i & 0x01C0) >> 6) * (1.0f / 7.0f),
			 0.125f);
	if (PlaneDiff(r_vieworigin, (&portal->plane)) < 0)
	{
		for (i = portal->numpoints - 1, v = varray_vertex3f;i >= 0;i--, v += 3)
			VectorCopy(portal->points[i].position, v);
	}
	else
		for (i = 0, v = varray_vertex3f;i < portal->numpoints;i++, v += 3)
			VectorCopy(portal->points[i].position, v);
	GL_LockArrays(0, portal->numpoints);
	R_Mesh_Draw(portal->numpoints, portal->numpoints - 2, polygonelements);
	GL_LockArrays(0, 0);
}

// LordHavoc: this is just a nice debugging tool, very slow
static void R_DrawPortals(void)
{
	int i, portalnum;
	mportal_t *portal;
	float center[3], f;
	model_t *model = r_refdef.worldmodel;
	if (model == NULL)
		return;
	for (portalnum = 0, portal = model->brush.data_portals;portalnum < model->brush.num_portals;portalnum++, portal++)
	{
		if (portal->numpoints <= POLYGONELEMENTS_MAXPOINTS)
		if (!R_CullBox(portal->mins, portal->maxs))
		{
			VectorClear(center);
			for (i = 0;i < portal->numpoints;i++)
				VectorAdd(center, portal->points[i].position, center);
			f = ixtable[portal->numpoints];
			VectorScale(center, f, center);
			R_MeshQueue_AddTransparent(center, R_DrawPortal_Callback, portal, portalnum);
		}
	}
}

static void R_DrawCollisionBrush(colbrushf_t *brush)
{
	int i;
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = brush->points->v;
	R_Mesh_State(&m);
	i = (int)(((size_t)brush) / sizeof(colbrushf_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, brush->numpoints);
	R_Mesh_Draw(brush->numpoints, brush->numtriangles, brush->elements);
	GL_LockArrays(0, 0);
}

static void R_DrawCollisionSurface(entity_render_t *ent, msurface_t *surface)
{
	int i;
	rmeshstate_t m;
	if (!surface->mesh.num_collisiontriangles)
		return;
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = surface->mesh.data_collisionvertex3f;
	R_Mesh_State(&m);
	i = (int)(((size_t)surface) / sizeof(msurface_t));
	GL_Color((i & 31) * (1.0f / 32.0f), ((i >> 5) & 31) * (1.0f / 32.0f), ((i >> 10) & 31) * (1.0f / 32.0f), 0.2f);
	GL_LockArrays(0, surface->mesh.num_collisionvertices);
	R_Mesh_Draw(surface->mesh.num_collisionvertices, surface->mesh.num_collisiontriangles, surface->mesh.data_collisionelement3i);
	GL_LockArrays(0, 0);
}

void R_WorldVisibility(void)
{
	int i, j, *mark;
	mleaf_t *leaf;
	mleaf_t *viewleaf;
	model_t *model = r_refdef.worldmodel;

	if (!model)
		return;

	// if possible find the leaf the view origin is in
	viewleaf = model->brushq1.PointInLeaf ? model->brushq1.PointInLeaf(model, r_vieworigin) : NULL;
	// if possible fetch the visible cluster bits
	if (model->brush.FatPVS)
		model->brush.FatPVS(model, r_vieworigin, 2, r_pvsbits, sizeof(r_pvsbits));

	// clear the visible surface and leaf flags arrays
	memset(r_worldsurfacevisible, 0, model->brush.num_surfaces);
	memset(r_worldleafvisible, 0, model->brush.num_leafs);

	// if the user prefers surfaceworldnode (testing?) or the viewleaf could
	// not be found, or the viewleaf is not part of the visible world
	// (floating around in the void), use the pvs method
	if (r_surfaceworldnode.integer || !viewleaf || viewleaf->clusterindex < 0)
	{
		// pvs method:
		// similar to quake's RecursiveWorldNode but without cache misses
		for (j = 0, leaf = model->brush.data_leafs;j < model->brush.num_leafs;j++, leaf++)
		{
			// if leaf is in current pvs and on the screen, mark its surfaces
			if (CHECKPVSBIT(r_pvsbits, leaf->clusterindex) && !R_CullBox(leaf->mins, leaf->maxs))
			{
				c_leafs++;
				r_worldleafvisible[j] = true;
				if (leaf->numleafsurfaces)
					for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
						r_worldsurfacevisible[*mark] = true;
			}
		}
	}
	else
	{
		int leafstackpos;
		mportal_t *p;
		mleaf_t *leafstack[8192];
		// portal method:
		// follows portals leading outward from viewleaf, does not venture
		// offscreen or into leafs that are not visible, faster than Quake's
		// RecursiveWorldNode and vastly better in unvised maps, often culls a
		// lot of surface that pvs alone would miss
		leafstack[0] = viewleaf;
		leafstackpos = 1;
		while (leafstackpos)
		{
			c_leafs++;
			leaf = leafstack[--leafstackpos];
			r_worldleafvisible[leaf - model->brush.data_leafs] = true;
			// mark any surfaces bounding this leaf
			if (leaf->numleafsurfaces)
				for (i = 0, mark = leaf->firstleafsurface;i < leaf->numleafsurfaces;i++, mark++)
					r_worldsurfacevisible[*mark] = true;
			// follow portals into other leafs
			// the checks are:
			// if viewer is behind portal (portal faces outward into the scene)
			// and the portal polygon's bounding box is on the screen
			// and the leaf has not been visited yet
			// and the leaf is visible in the pvs
			// (the first two checks won't cause as many cache misses as the leaf checks)
			for (p = leaf->portals;p;p = p->next)
				if (DotProduct(r_vieworigin, p->plane.normal) < (p->plane.dist + 1) && !R_CullBox(p->mins, p->maxs) && !r_worldleafvisible[p->past - model->brush.data_leafs] && CHECKPVSBIT(r_pvsbits, p->past->clusterindex))
					leafstack[leafstackpos++] = p->past;
		}
	}

	if (r_drawportals.integer)
		R_DrawPortals();
}

void R_Q1BSP_DrawSky(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	if (r_drawcollisionbrushes.integer < 2)
		R_DrawSurfaces(ent, true);
}

void R_Q1BSP_Draw(entity_render_t *ent)
{
	if (ent->model == NULL)
		return;
	c_bmodels++;
	if (r_drawcollisionbrushes.integer < 2)
		R_DrawSurfaces(ent, false);
	if (r_drawcollisionbrushes.integer >= 1 && ent->model->brush.num_brushes)
	{
		int i;
		model_t *model = ent->model;
		msurface_t *surface;
		q3mbrush_t *brush;
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		GL_DepthMask(false);
		GL_DepthTest(true);
		qglPolygonOffset(r_drawcollisionbrushes_polygonfactor.value, r_drawcollisionbrushes_polygonoffset.value);
		for (i = 0, brush = model->brush.data_brushes + model->firstmodelbrush;i < model->nummodelbrushes;i++, brush++)
			if (brush->colbrushf && brush->colbrushf->numtriangles)
				R_DrawCollisionBrush(brush->colbrushf);
		for (i = 0, surface = model->brush.data_surfaces + model->firstmodelsurface;i < model->nummodelsurfaces;i++, surface++)
			if (surface->mesh.num_collisiontriangles)
				R_DrawCollisionSurface(ent, surface);
		qglPolygonOffset(0, 0);
	}
}

void R_Q1BSP_GetLightInfo(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outclusterlist, qbyte *outclusterpvs, int *outnumclusterspointer, int *outsurfacelist, qbyte *outsurfacepvs, int *outnumsurfacespointer)
{
	model_t *model = ent->model;
	vec3_t lightmins, lightmaxs;
	int t, leafindex, leafsurfaceindex, surfaceindex, triangleindex, outnumclusters = 0, outnumsurfaces = 0;
	const int *e;
	const float *v[3];
	msurface_t *surface;
	mleaf_t *leaf;
	const qbyte *pvs;
	lightmins[0] = relativelightorigin[0] - lightradius;
	lightmins[1] = relativelightorigin[1] - lightradius;
	lightmins[2] = relativelightorigin[2] - lightradius;
	lightmaxs[0] = relativelightorigin[0] + lightradius;
	lightmaxs[1] = relativelightorigin[1] + lightradius;
	lightmaxs[2] = relativelightorigin[2] + lightradius;
	*outnumclusterspointer = 0;
	*outnumsurfacespointer = 0;
	memset(outclusterpvs, 0, model->brush.num_pvsclusterbytes);
	memset(outsurfacepvs, 0, (model->nummodelsurfaces + 7) >> 3);
	if (model == NULL)
	{
		VectorCopy(lightmins, outmins);
		VectorCopy(lightmaxs, outmaxs);
		return;
	}
	VectorCopy(relativelightorigin, outmins);
	VectorCopy(relativelightorigin, outmaxs);
	if (model->brush.GetPVS)
		pvs = model->brush.GetPVS(model, relativelightorigin);
	else
		pvs = NULL;
	R_UpdateAllTextureInfo(ent);
	// FIXME: use BSP recursion as lights are often small
	for (leafindex = 0, leaf = model->brush.data_leafs;leafindex < model->brush.num_leafs;leafindex++, leaf++)
	{
		if (BoxesOverlap(lightmins, lightmaxs, leaf->mins, leaf->maxs) && (pvs == NULL || CHECKPVSBIT(pvs, leaf->clusterindex)))
		{
			outmins[0] = min(outmins[0], leaf->mins[0]);
			outmins[1] = min(outmins[1], leaf->mins[1]);
			outmins[2] = min(outmins[2], leaf->mins[2]);
			outmaxs[0] = max(outmaxs[0], leaf->maxs[0]);
			outmaxs[1] = max(outmaxs[1], leaf->maxs[1]);
			outmaxs[2] = max(outmaxs[2], leaf->maxs[2]);
			if (outclusterpvs)
			{
				if (!CHECKPVSBIT(outclusterpvs, leaf->clusterindex))
				{
					SETPVSBIT(outclusterpvs, leaf->clusterindex);
					outclusterlist[outnumclusters++] = leaf->clusterindex;
				}
			}
			if (outsurfacepvs)
			{
				for (leafsurfaceindex = 0;leafsurfaceindex < leaf->numleafsurfaces;leafsurfaceindex++)
				{
					surfaceindex = leaf->firstleafsurface[leafsurfaceindex];
					if (!CHECKPVSBIT(outsurfacepvs, surfaceindex))
					{
						surface = model->brush.data_surfaces + surfaceindex;
						if (BoxesOverlap(lightmins, lightmaxs, surface->mins, surface->maxs))
						if ((surface->texture->currentmaterialflags & (MATERIALFLAG_WALL | MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT)) == MATERIALFLAG_WALL)
						{
							for (triangleindex = 0, t = surface->num_firstshadowmeshtriangle, e = model->brush.shadowmesh->element3i + t * 3;triangleindex < surface->mesh.num_triangles;triangleindex++, t++, e += 3)
							{
								v[0] = model->brush.shadowmesh->vertex3f + e[0] * 3;
								v[1] = model->brush.shadowmesh->vertex3f + e[1] * 3;
								v[2] = model->brush.shadowmesh->vertex3f + e[2] * 3;
								if (lightmaxs[0] > min(v[0][0], min(v[1][0], v[2][0])) && lightmins[0] < max(v[0][0], max(v[1][0], v[2][0])) && lightmaxs[1] > min(v[0][1], min(v[1][1], v[2][1])) && lightmins[1] < max(v[0][1], max(v[1][1], v[2][1])) && lightmaxs[2] > min(v[0][2], min(v[1][2], v[2][2])) && lightmins[2] < max(v[0][2], max(v[1][2], v[2][2])))
								{
									SETPVSBIT(outsurfacepvs, surfaceindex);
									outsurfacelist[outnumsurfaces++] = surfaceindex;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	// limit combined leaf box to light boundaries
	outmins[0] = max(outmins[0], lightmins[0]);
	outmins[1] = max(outmins[1], lightmins[1]);
	outmins[2] = max(outmins[2], lightmins[2]);
	outmaxs[0] = min(outmaxs[0], lightmaxs[0]);
	outmaxs[1] = min(outmaxs[1], lightmaxs[1]);
	outmaxs[2] = min(outmaxs[2], lightmaxs[2]);

	*outnumclusterspointer = outnumclusters;
	*outnumsurfacespointer = outnumsurfaces;
}

void R_Q1BSP_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs)
{
	model_t *model = ent->model;
	msurface_t *surface;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		R_Mesh_Matrix(&ent->matrix);
		R_Shadow_PrepareShadowMark(model->brush.shadowmesh->numtriangles);
		if (!r_shadow_compilingrtlight)
			R_UpdateAllTextureInfo(ent);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brush.data_surfaces + surfacelist[surfacelistindex];
			if ((surface->texture->currentmaterialflags & (MATERIALFLAG_NODRAW | MATERIALFLAG_TRANSPARENT | MATERIALFLAG_WALL)) != MATERIALFLAG_WALL)
				continue;
			if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
				continue;
			R_Shadow_MarkVolumeFromBox(surface->num_firstshadowmeshtriangle, surface->mesh.num_triangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, relativelightorigin, lightmins, lightmaxs, surface->mins, surface->maxs);
		}
		R_Shadow_VolumeFromList(model->brush.shadowmesh->numverts, model->brush.shadowmesh->numtriangles, model->brush.shadowmesh->vertex3f, model->brush.shadowmesh->element3i, model->brush.shadowmesh->neighbor3i, relativelightorigin, lightradius + model->radius + r_shadow_projectdistance.value, numshadowmark, shadowmarklist);
	}
}

void R_Q1BSP_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist)
{
	model_t *model = ent->model;
	msurface_t *surface;
	texture_t *t;
	int surfacelistindex;
	if (r_drawcollisionbrushes.integer < 2)
	{
		R_Mesh_Matrix(&ent->matrix);
		if (!r_shadow_compilingrtlight)
			R_UpdateAllTextureInfo(ent);
		for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			surface = model->brush.data_surfaces + surfacelist[surfacelistindex];
			if (surface->texture->basematerialflags & MATERIALFLAG_NODRAW || !surface->mesh.num_triangles)
				continue;
			if (r_shadow_compilingrtlight)
			{
				// if compiling an rtlight, capture the mesh
				t = surface->texture;
				if ((t->basematerialflags & (MATERIALFLAG_WALL | MATERIALFLAG_TRANSPARENT)) == MATERIALFLAG_WALL)
					Mod_ShadowMesh_AddMesh(r_shadow_mempool, r_shadow_compilingrtlight->static_meshchain_light, surface->texture->skin.base, surface->texture->skin.gloss, surface->texture->skin.nmap, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, surface->mesh.num_triangles, surface->mesh.data_element3i);
			}
			else if (ent != r_refdef.worldentity || r_worldsurfacevisible[surfacelist[surfacelistindex]])
			{
				t = surface->texture->currentframe;
				// FIXME: transparent surfaces need to be lit later
				if ((t->currentmaterialflags & (MATERIALFLAG_WALL | MATERIALFLAG_TRANSPARENT)) == MATERIALFLAG_WALL)
				{
					if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
						qglDisable(GL_CULL_FACE);
					R_Shadow_RenderLighting(surface->mesh.num_vertices, surface->mesh.num_triangles, surface->mesh.data_element3i, surface->mesh.data_vertex3f, surface->mesh.data_svector3f, surface->mesh.data_tvector3f, surface->mesh.data_normal3f, surface->mesh.data_texcoordtexture2f, relativelightorigin, relativeeyeorigin, lightcolor, matrix_modeltolight, matrix_modeltoattenuationxyz, matrix_modeltoattenuationz, t->skin.base, t->skin.nmap, t->skin.gloss, lightcubemap, ambientscale, diffusescale, specularscale);
					if (surface->texture->textureflags & Q3TEXTUREFLAG_TWOSIDED)
						qglEnable(GL_CULL_FACE);
				}
			}
		}
	}
}

#if 0
static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
}

static void gl_surf_newmap(void)
{
}
#endif

void GL_Surf_Init(void)
{

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);
	Cvar_RegisterVariable(&r_detailtextures);
	Cvar_RegisterVariable(&r_surfaceworldnode);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonfactor);
	Cvar_RegisterVariable(&r_drawcollisionbrushes_polygonoffset);
	Cvar_RegisterVariable(&r_q3bsp_renderskydepth);
	Cvar_RegisterVariable(&gl_lightmaps);

	//R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

