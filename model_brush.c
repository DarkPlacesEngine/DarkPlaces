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

#include "quakedef.h"
#include "image.h"
#include "r_shadow.h"

// note: model_shared.c sets up r_notexture, and r_surf_notexture

qbyte mod_q1bsp_novis[(MAX_MAP_LEAFS + 7)/ 8];

//cvar_t r_subdivide_size = {CVAR_SAVE, "r_subdivide_size", "128"};
cvar_t halflifebsp = {0, "halflifebsp", "0"};
cvar_t r_novis = {0, "r_novis", "0"};
cvar_t r_miplightmaps = {CVAR_SAVE, "r_miplightmaps", "0"};
cvar_t r_lightmaprgba = {0, "r_lightmaprgba", "1"};
cvar_t r_nosurftextures = {0, "r_nosurftextures", "0"};
cvar_t r_sortsurfaces = {0, "r_sortsurfaces", "0"};

void Mod_BrushInit(void)
{
//	Cvar_RegisterVariable(&r_subdivide_size);
	Cvar_RegisterVariable(&halflifebsp);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_miplightmaps);
	Cvar_RegisterVariable(&r_lightmaprgba);
	Cvar_RegisterVariable(&r_nosurftextures);
	Cvar_RegisterVariable(&r_sortsurfaces);
	memset(mod_q1bsp_novis, 0xff, sizeof(mod_q1bsp_novis));
}

static mleaf_t *Mod_Q1BSP_PointInLeaf(model_t *model, const vec3_t p)
{
	mnode_t *node;

	if (model == NULL)
		return NULL;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];

	return (mleaf_t *)node;
}

static int Mod_Q1BSP_PointContents(model_t *model, const vec3_t p)
{
	mnode_t *node;

	if (model == NULL)
		return CONTENTS_EMPTY;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p,node->plane->normal)) < node->plane->dist];

	return ((mleaf_t *)node)->contents;
}

typedef struct findnonsolidlocationinfo_s
{
	vec3_t center;
	vec_t radius;
	vec3_t nudge;
	vec_t bestdist;
	model_t *model;
}
findnonsolidlocationinfo_t;

#if 0
extern cvar_t samelevel;
#endif
static void Mod_Q1BSP_FindNonSolidLocation_r_Leaf(findnonsolidlocationinfo_t *info, mleaf_t *leaf)
{
	int i, surfnum, k, *tri, *mark;
	float dist, f, vert[3][3], edge[3][3], facenormal[3], edgenormal[3][3], point[3];
#if 0
	float surfnormal[3];
#endif
	msurface_t *surf;
	surfmesh_t *mesh;
	for (surfnum = 0, mark = leaf->firstmarksurface;surfnum < leaf->nummarksurfaces;surfnum++, mark++)
	{
		surf = info->model->brushq1.surfaces + *mark;
		if (surf->flags & SURF_SOLIDCLIP)
		{
#if 0
			VectorCopy(surf->plane->normal, surfnormal);
			if (surf->flags & SURF_PLANEBACK)
				VectorNegate(surfnormal, surfnormal);
#endif
			for (mesh = surf->mesh;mesh;mesh = mesh->chain)
			{
				for (k = 0;k < mesh->numtriangles;k++)
				{
					tri = mesh->element3i + k * 3;
					VectorCopy((mesh->vertex3f + tri[0] * 3), vert[0]);
					VectorCopy((mesh->vertex3f + tri[1] * 3), vert[1]);
					VectorCopy((mesh->vertex3f + tri[2] * 3), vert[2]);
					VectorSubtract(vert[1], vert[0], edge[0]);
					VectorSubtract(vert[2], vert[1], edge[1]);
					CrossProduct(edge[1], edge[0], facenormal);
					if (facenormal[0] || facenormal[1] || facenormal[2])
					{
						VectorNormalize(facenormal);
#if 0
						if (VectorDistance(facenormal, surfnormal) > 0.01f)
							Con_Printf("a2! %f %f %f != %f %f %f\n", facenormal[0], facenormal[1], facenormal[2], surfnormal[0], surfnormal[1], surfnormal[2]);
#endif
						f = DotProduct(info->center, facenormal) - DotProduct(vert[0], facenormal);
						if (f <= info->bestdist && f >= -info->bestdist)
						{
							VectorSubtract(vert[0], vert[2], edge[2]);
							VectorNormalize(edge[0]);
							VectorNormalize(edge[1]);
							VectorNormalize(edge[2]);
							CrossProduct(facenormal, edge[0], edgenormal[0]);
							CrossProduct(facenormal, edge[1], edgenormal[1]);
							CrossProduct(facenormal, edge[2], edgenormal[2]);
#if 0
							if (samelevel.integer & 1)
								VectorNegate(edgenormal[0], edgenormal[0]);
							if (samelevel.integer & 2)
								VectorNegate(edgenormal[1], edgenormal[1]);
							if (samelevel.integer & 4)
								VectorNegate(edgenormal[2], edgenormal[2]);
							for (i = 0;i < 3;i++)
								if (DotProduct(vert[0], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f
								 || DotProduct(vert[1], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f
								 || DotProduct(vert[2], edgenormal[i]) > DotProduct(vert[i], edgenormal[i]) + 0.1f)
									Con_Printf("a! %i : %f %f %f (%f %f %f)\n", i, edgenormal[i][0], edgenormal[i][1], edgenormal[i][2], facenormal[0], facenormal[1], facenormal[2]);
#endif
							// face distance
							if (DotProduct(info->center, edgenormal[0]) < DotProduct(vert[0], edgenormal[0])
							 && DotProduct(info->center, edgenormal[1]) < DotProduct(vert[1], edgenormal[1])
							 && DotProduct(info->center, edgenormal[2]) < DotProduct(vert[2], edgenormal[2]))
							{
								// we got lucky, the center is within the face
								dist = DotProduct(info->center, facenormal) - DotProduct(vert[0], facenormal);
								if (dist < 0)
								{
									dist = -dist;
									if (info->bestdist > dist)
									{
										info->bestdist = dist;
										VectorScale(facenormal, (info->radius - -dist), info->nudge);
									}
								}
								else
								{
									if (info->bestdist > dist)
									{
										info->bestdist = dist;
										VectorScale(facenormal, (info->radius - dist), info->nudge);
									}
								}
							}
							else
							{
								// check which edge or vertex the center is nearest
								for (i = 0;i < 3;i++)
								{
									f = DotProduct(info->center, edge[i]);
									if (f >= DotProduct(vert[0], edge[i])
									 && f <= DotProduct(vert[1], edge[i]))
									{
										// on edge
										VectorMA(info->center, -f, edge[i], point);
										dist = sqrt(DotProduct(point, point));
										if (info->bestdist > dist)
										{
											info->bestdist = dist;
											VectorScale(point, (info->radius / dist), info->nudge);
										}
										// skip both vertex checks
										// (both are further away than this edge)
										i++;
									}
									else
									{
										// not on edge, check first vertex of edge
										VectorSubtract(info->center, vert[i], point);
										dist = sqrt(DotProduct(point, point));
										if (info->bestdist > dist)
										{
											info->bestdist = dist;
											VectorScale(point, (info->radius / dist), info->nudge);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static void Mod_Q1BSP_FindNonSolidLocation_r(findnonsolidlocationinfo_t *info, mnode_t *node)
{
	if (node->contents)
	{
		if (((mleaf_t *)node)->nummarksurfaces)
			Mod_Q1BSP_FindNonSolidLocation_r_Leaf(info, (mleaf_t *)node);
	}
	else
	{
		float f = PlaneDiff(info->center, node->plane);
		if (f >= -info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[0]);
		if (f <= info->bestdist)
			Mod_Q1BSP_FindNonSolidLocation_r(info, node->children[1]);
	}
}

static void Mod_Q1BSP_FindNonSolidLocation(model_t *model, const vec3_t in, vec3_t out, float radius)
{
	int i;
	findnonsolidlocationinfo_t info;
	if (model == NULL)
	{
		VectorCopy(in, out);
		return;
	}
	VectorCopy(in, info.center);
	info.radius = radius;
	info.model = model;
	i = 0;
	do
	{
		VectorClear(info.nudge);
		info.bestdist = radius;
		Mod_Q1BSP_FindNonSolidLocation_r(&info, model->brushq1.nodes + model->brushq1.hulls[0].firstclipnode);
		VectorAdd(info.center, info.nudge, info.center);
	}
	while (info.bestdist < radius && ++i < 10);
	VectorCopy(info.center, out);
}

static qbyte *Mod_Q1BSP_DecompressVis(model_t *model, qbyte *in)
{
	static qbyte decompressed[MAX_MAP_LEAFS/8];
	int c;
	qbyte *out;
	int row;

	row = (model->brushq1.numleafs+7)>>3;
	out = decompressed;

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

static qbyte *Mod_Q1BSP_LeafPVS(model_t *model, mleaf_t *leaf)
{
	if (r_novis.integer || leaf == model->brushq1.leafs || leaf->compressed_vis == NULL)
		return mod_q1bsp_novis;
	return Mod_Q1BSP_DecompressVis(model, leaf->compressed_vis);
}

static void Mod_Q1BSP_LoadTextures(lump_t *l)
{
	int i, j, k, num, max, altmax, mtwidth, mtheight, *dofs, incomplete;
	miptex_t *dmiptex;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	qbyte *data, *mtdata;
	char name[256];

	loadmodel->brushq1.textures = NULL;

	if (!l->filelen)
		return;

	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	// add two slots for notexture walls and notexture liquids
	loadmodel->brushq1.numtextures = m->nummiptex + 2;
	loadmodel->brushq1.textures = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numtextures * sizeof(texture_t));

	// fill out all slots with notexture
	for (i = 0, tx = loadmodel->brushq1.textures;i < loadmodel->brushq1.numtextures;i++, tx++)
	{
		tx->number = i;
		strcpy(tx->name, "NO TEXTURE FOUND");
		tx->width = 16;
		tx->height = 16;
		tx->skin.base = r_notexture;
		tx->shader = &Cshader_wall_lightmap;
		tx->flags = SURF_SOLIDCLIP;
		if (i == loadmodel->brushq1.numtextures - 1)
		{
			tx->flags |= SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			tx->shader = &Cshader_water;
		}
		tx->currentframe = tx;
	}

	// just to work around bounds checking when debugging with it (array index out of bounds error thing)
	dofs = m->dataofs;
	// LordHavoc: mostly rewritten map texture loader
	for (i = 0;i < m->nummiptex;i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1 || r_nosurftextures.integer)
			continue;
		dmiptex = (miptex_t *)((qbyte *)m + dofs[i]);

		// make sure name is no more than 15 characters
		for (j = 0;dmiptex->name[j] && j < 15;j++)
			name[j] = dmiptex->name[j];
		name[j] = 0;

		mtwidth = LittleLong(dmiptex->width);
		mtheight = LittleLong(dmiptex->height);
		mtdata = NULL;
		j = LittleLong(dmiptex->offsets[0]);
		if (j)
		{
			// texture included
			if (j < 40 || j + mtwidth * mtheight > l->filelen)
			{
				Con_Printf("Texture \"%s\" in \"%s\"is corrupt or incomplete\n", dmiptex->name, loadmodel->name);
				continue;
			}
			mtdata = (qbyte *)dmiptex + j;
		}

		if ((mtwidth & 15) || (mtheight & 15))
			Con_Printf("warning: texture \"%s\" in \"%s\" is not 16 aligned", dmiptex->name, loadmodel->name);

		// LordHavoc: force all names to lowercase
		for (j = 0;name[j];j++)
			if (name[j] >= 'A' && name[j] <= 'Z')
				name[j] += 'a' - 'A';

		tx = loadmodel->brushq1.textures + i;
		strcpy(tx->name, name);
		tx->width = mtwidth;
		tx->height = mtheight;

		if (!tx->name[0])
		{
			sprintf(tx->name, "unnamed%i", i);
			Con_Printf("warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		// LordHavoc: HL sky textures are entirely different than quake
		if (!loadmodel->brushq1.ishlbsp && !strncmp(tx->name, "sky", 3) && mtwidth == 256 && mtheight == 128)
		{
			if (loadmodel->isworldmodel)
			{
				data = loadimagepixels(tx->name, false, 0, 0);
				if (data)
				{
					if (image_width == 256 && image_height == 128)
					{
						R_InitSky(data, 4);
						Mem_Free(data);
					}
					else
					{
						Mem_Free(data);
						Con_Printf("Invalid replacement texture for sky \"%s\" in %\"%s\", must be 256x128 pixels\n", tx->name, loadmodel->name);
						if (mtdata != NULL)
							R_InitSky(mtdata, 1);
					}
				}
				else if (mtdata != NULL)
					R_InitSky(mtdata, 1);
			}
		}
		else
		{
			if (!Mod_LoadSkinFrame(&tx->skin, tx->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, false, true, true))
			{
				// did not find external texture, load it from the bsp or wad3
				if (loadmodel->brushq1.ishlbsp)
				{
					// internal texture overrides wad
					qbyte *pixels, *freepixels, *fogpixels;
					pixels = freepixels = NULL;
					if (mtdata)
						pixels = W_ConvertWAD3Texture(dmiptex);
					if (pixels == NULL)
						pixels = freepixels = W_GetTexture(tx->name);
					if (pixels != NULL)
					{
						tx->width = image_width;
						tx->height = image_height;
						tx->skin.base = tx->skin.merged = R_LoadTexture2D(loadmodel->texturepool, tx->name, image_width, image_height, pixels, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
						if (Image_CheckAlpha(pixels, image_width * image_height, true))
						{
							fogpixels = Mem_Alloc(tempmempool, image_width * image_height * 4);
							for (j = 0;j < image_width * image_height * 4;j += 4)
							{
								fogpixels[j + 0] = 255;
								fogpixels[j + 1] = 255;
								fogpixels[j + 2] = 255;
								fogpixels[j + 3] = pixels[j + 3];
							}
							tx->skin.fog = R_LoadTexture2D(loadmodel->texturepool, tx->name, image_width, image_height, pixels, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, NULL);
							Mem_Free(fogpixels);
						}
					}
					if (freepixels)
						Mem_Free(freepixels);
				}
				else if (mtdata) // texture included
					Mod_LoadSkinFrame_Internal(&tx->skin, tx->name, TEXF_MIPMAP | TEXF_PRECACHE, false, true, tx->name[0] != '*' && r_fullbrights.integer, mtdata, tx->width, tx->height);
			}
		}
		if (tx->skin.base == NULL)
		{
			// no texture found
			tx->width = 16;
			tx->height = 16;
			tx->skin.base = r_notexture;
		}

		if (tx->name[0] == '*')
		{
			// turb does not block movement
			tx->flags &= ~SURF_SOLIDCLIP;
			tx->flags |= SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			// LordHavoc: some turbulent textures should be fullbright and solid
			if (!strncmp(tx->name,"*lava",5)
			 || !strncmp(tx->name,"*teleport",9)
			 || !strncmp(tx->name,"*rift",5)) // Scourge of Armagon texture
				tx->flags |= SURF_DRAWFULLBRIGHT | SURF_DRAWNOALPHA;
			else
				tx->flags |= SURF_WATERALPHA;
			tx->shader = &Cshader_water;
		}
		else if (tx->name[0] == 's' && tx->name[1] == 'k' && tx->name[2] == 'y')
		{
			tx->flags |= SURF_DRAWSKY;
			tx->shader = &Cshader_sky;
		}
		else
		{
			tx->flags |= SURF_LIGHTMAP;
			if (!tx->skin.fog)
				tx->flags |= SURF_SHADOWCAST | SURF_SHADOWLIGHT;
			tx->shader = &Cshader_wall_lightmap;
		}

		// start out with no animation
		tx->currentframe = tx;
	}

	// sequence the animations
	for (i = 0;i < m->nummiptex;i++)
	{
		tx = loadmodel->brushq1.textures + i;
		if (!tx || tx->name[0] != '+' || tx->name[1] == 0 || tx->name[2] == 0)
			continue;
		if (tx->anim_total[0] || tx->anim_total[1])
			continue;	// already sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		for (j = i;j < m->nummiptex;j++)
		{
			tx2 = loadmodel->brushq1.textures + j;
			if (!tx2 || tx2->name[0] != '+' || strcmp(tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= '0' && num <= '9')
				anims[num - '0'] = tx2;
			else if (num >= 'a' && num <= 'j')
				altanims[num - 'a'] = tx2;
			else
				Con_Printf("Bad animating texture %s\n", tx->name);
		}

		max = altmax = 0;
		for (j = 0;j < 10;j++)
		{
			if (anims[j])
				max = j + 1;
			if (altanims[j])
				altmax = j + 1;
		}
		//Con_Printf("linking animation %s (%i:%i frames)\n\n", tx->name, max, altmax);

		incomplete = false;
		for (j = 0;j < max;j++)
		{
			if (!anims[j])
			{
				Con_Printf("Missing frame %i of %s\n", j, tx->name);
				incomplete = true;
			}
		}
		for (j = 0;j < altmax;j++)
		{
			if (!altanims[j])
			{
				Con_Printf("Missing altframe %i of %s\n", j, tx->name);
				incomplete = true;
			}
		}
		if (incomplete)
			continue;

		if (altmax < 1)
		{
			// if there is no alternate animation, duplicate the primary
			// animation into the alternate
			altmax = max;
			for (k = 0;k < 10;k++)
				altanims[k] = anims[k];
		}

		// link together the primary animation
		for (j = 0;j < max;j++)
		{
			tx2 = anims[j];
			tx2->animated = true;
			tx2->anim_total[0] = max;
			tx2->anim_total[1] = altmax;
			for (k = 0;k < 10;k++)
			{
				tx2->anim_frames[0][k] = anims[k];
				tx2->anim_frames[1][k] = altanims[k];
			}
		}

		// if there really is an alternate anim...
		if (anims[0] != altanims[0])
		{
			// link together the alternate animation
			for (j = 0;j < altmax;j++)
			{
				tx2 = altanims[j];
				tx2->animated = true;
				// the primary/alternate are reversed here
				tx2->anim_total[0] = altmax;
				tx2->anim_total[1] = max;
				for (k = 0;k < 10;k++)
				{
					tx2->anim_frames[0][k] = altanims[k];
					tx2->anim_frames[1][k] = anims[k];
				}
			}
		}
	}
}

static void Mod_Q1BSP_LoadLighting(lump_t *l)
{
	int i;
	qbyte *in, *out, *data, d;
	char litfilename[1024];
	loadmodel->brushq1.lightdata = NULL;
	if (loadmodel->brushq1.ishlbsp) // LordHavoc: load the colored lighting data straight
	{
		loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, l->filelen);
		memcpy(loadmodel->brushq1.lightdata, mod_base + l->fileofs, l->filelen);
	}
	else // LordHavoc: bsp version 29 (normal white lighting)
	{
		// LordHavoc: hope is not lost yet, check for a .lit file to load
		strcpy(litfilename, loadmodel->name);
		FS_StripExtension(litfilename, litfilename);
		strcat(litfilename, ".lit");
		data = (qbyte*) FS_LoadFile(litfilename, false);
		if (data)
		{
			if (fs_filesize > 8 && data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Con_DPrintf("loaded %s\n", litfilename);
					loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, fs_filesize - 8);
					memcpy(loadmodel->brushq1.lightdata, data + 8, fs_filesize - 8);
					Mem_Free(data);
					return;
				}
				else
				{
					Con_Printf("Unknown .lit file version (%d)\n", i);
					Mem_Free(data);
				}
			}
			else
			{
				if (fs_filesize == 8)
					Con_Printf("Empty .lit file, ignoring\n");
				else
					Con_Printf("Corrupt .lit file (old version?), ignoring\n");
				Mem_Free(data);
			}
		}
		// LordHavoc: oh well, expand the white lighting data
		if (!l->filelen)
			return;
		loadmodel->brushq1.lightdata = Mem_Alloc(loadmodel->mempool, l->filelen*3);
		in = loadmodel->brushq1.lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
		out = loadmodel->brushq1.lightdata;
		memcpy(in, mod_base + l->fileofs, l->filelen);
		for (i = 0;i < l->filelen;i++)
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	}
}

static void Mod_Q1BSP_LoadLightList(void)
{
	int a, n, numlights;
	char lightsfilename[1024], *s, *t, *lightsstring;
	mlight_t *e;

	strcpy(lightsfilename, loadmodel->name);
	FS_StripExtension(lightsfilename, lightsfilename);
	strcat(lightsfilename, ".lights");
	s = lightsstring = (char *) FS_LoadFile(lightsfilename, false);
	if (s)
	{
		numlights = 0;
		while (*s)
		{
			while (*s && *s != '\n')
				s++;
			if (!*s)
			{
				Mem_Free(lightsstring);
				Host_Error("lights file must end with a newline\n");
			}
			s++;
			numlights++;
		}
		loadmodel->brushq1.lights = Mem_Alloc(loadmodel->mempool, numlights * sizeof(mlight_t));
		s = lightsstring;
		n = 0;
		while (*s && n < numlights)
		{
			t = s;
			while (*s && *s != '\n')
				s++;
			if (!*s)
			{
				Mem_Free(lightsstring);
				Host_Error("misparsed lights file!\n");
			}
			e = loadmodel->brushq1.lights + n;
			*s = 0;
			a = sscanf(t, "%f %f %f %f %f %f %f %f %f %f %f %f %f %d", &e->origin[0], &e->origin[1], &e->origin[2], &e->falloff, &e->light[0], &e->light[1], &e->light[2], &e->subtract, &e->spotdir[0], &e->spotdir[1], &e->spotdir[2], &e->spotcone, &e->distbias, &e->style);
			*s = '\n';
			if (a != 14)
			{
				Mem_Free(lightsstring);
				Host_Error("invalid lights file, found %d parameters on line %i, should be 14 parameters (origin[0] origin[1] origin[2] falloff light[0] light[1] light[2] subtract spotdir[0] spotdir[1] spotdir[2] spotcone distancebias style)\n", a, n + 1);
			}
			s++;
			n++;
		}
		if (*s)
		{
			Mem_Free(lightsstring);
			Host_Error("misparsed lights file!\n");
		}
		loadmodel->brushq1.numlights = numlights;
		Mem_Free(lightsstring);
	}
}

/*
static int castshadowcount = 0;
static void Mod_Q1BSP_ProcessLightList(void)
{
	int j, k, l, *mark, lnum;
	mlight_t *e;
	msurface_t *surf;
	float dist;
	mleaf_t *leaf;
	qbyte *pvs;
	vec3_t temp;
	float *v, radius2;
	for (lnum = 0, e = loadmodel->brushq1.lights;lnum < loadmodel->brushq1.numlights;lnum++, e++)
	{
		e->cullradius2 = DotProduct(e->light, e->light) / (e->falloff * e->falloff * 8192.0f * 8192.0f * 2.0f * 2.0f);// + 4096.0f;
		if (e->cullradius2 > 4096.0f * 4096.0f)
			e->cullradius2 = 4096.0f * 4096.0f;
		e->cullradius = e->lightradius = sqrt(e->cullradius2);
		leaf = Mod_Q1BSP_PointInLeaf(e->origin, loadmodel);
		if (leaf->compressed_vis)
			pvs = Mod_Q1BSP_DecompressVis(leaf->compressed_vis, loadmodel);
		else
			pvs = mod_q1bsp_novis;
		for (j = 0;j < loadmodel->brushq1.numsurfaces;j++)
			loadmodel->brushq1.surfacevisframes[j] = -1;
		for (j = 0, leaf = loadmodel->brushq1.leafs + 1;j < loadmodel->brushq1.numleafs - 1;j++, leaf++)
		{
			if (pvs[j >> 3] & (1 << (j & 7)))
			{
				for (k = 0, mark = leaf->firstmarksurface;k < leaf->nummarksurfaces;k++, mark++)
				{
					surf = loadmodel->brushq1.surfaces + *mark;
					if (surf->number != *mark)
						Con_Printf("%d != %d\n", surf->number, *mark);
					dist = DotProduct(e->origin, surf->plane->normal) - surf->plane->dist;
					if (surf->flags & SURF_PLANEBACK)
						dist = -dist;
					if (dist > 0 && dist < e->cullradius)
					{
						temp[0] = bound(surf->poly_mins[0], e->origin[0], surf->poly_maxs[0]) - e->origin[0];
						temp[1] = bound(surf->poly_mins[1], e->origin[1], surf->poly_maxs[1]) - e->origin[1];
						temp[2] = bound(surf->poly_mins[2], e->origin[2], surf->poly_maxs[2]) - e->origin[2];
						if (DotProduct(temp, temp) < lightradius2)
							loadmodel->brushq1.surfacevisframes[*mark] = -2;
					}
				}
			}
		}
		// build list of light receiving surfaces
		e->numsurfaces = 0;
		for (j = 0;j < loadmodel->brushq1.numsurfaces;j++)
			if (loadmodel->brushq1.surfacevisframes[j] == -2)
				e->numsurfaces++;
		e->surfaces = NULL;
		if (e->numsurfaces > 0)
		{
			e->surfaces = Mem_Alloc(loadmodel->mempool, sizeof(msurface_t *) * e->numsurfaces);
			e->numsurfaces = 0;
			for (j = 0;j < loadmodel->brushq1.numsurfaces;j++)
				if (loadmodel->brushq1.surfacevisframes[j] == -2)
					e->surfaces[e->numsurfaces++] = loadmodel->brushq1.surfaces + j;
		}
		// find bounding box and sphere of lit surfaces
		// (these will be used for creating a shape to clip the light)
		radius2 = 0;
		for (j = 0;j < e->numsurfaces;j++)
		{
			surf = e->surfaces[j];
			if (j == 0)
			{
				VectorCopy(surf->poly_verts, e->mins);
				VectorCopy(surf->poly_verts, e->maxs);
			}
			for (k = 0, v = surf->poly_verts;k < surf->poly_numverts;k++, v += 3)
			{
				if (e->mins[0] > v[0]) e->mins[0] = v[0];if (e->maxs[0] < v[0]) e->maxs[0] = v[0];
				if (e->mins[1] > v[1]) e->mins[1] = v[1];if (e->maxs[1] < v[1]) e->maxs[1] = v[1];
				if (e->mins[2] > v[2]) e->mins[2] = v[2];if (e->maxs[2] < v[2]) e->maxs[2] = v[2];
				VectorSubtract(v, e->origin, temp);
				dist = DotProduct(temp, temp);
				if (radius2 < dist)
					radius2 = dist;
			}
		}
		if (e->cullradius2 > radius2)
		{
			e->cullradius2 = radius2;
			e->cullradius = sqrt(e->cullradius2);
		}
		if (e->mins[0] < e->origin[0] - e->lightradius) e->mins[0] = e->origin[0] - e->lightradius;
		if (e->maxs[0] > e->origin[0] + e->lightradius) e->maxs[0] = e->origin[0] + e->lightradius;
		if (e->mins[1] < e->origin[1] - e->lightradius) e->mins[1] = e->origin[1] - e->lightradius;
		if (e->maxs[1] > e->origin[1] + e->lightradius) e->maxs[1] = e->origin[1] + e->lightradius;
		if (e->mins[2] < e->origin[2] - e->lightradius) e->mins[2] = e->origin[2] - e->lightradius;
		if (e->maxs[2] > e->origin[2] + e->lightradius) e->maxs[2] = e->origin[2] + e->lightradius;
		// clip shadow volumes against eachother to remove unnecessary
		// polygons(and sections of polygons)
		{
			//vec3_t polymins, polymaxs;
			int maxverts = 4;
			float *verts = Mem_Alloc(loadmodel->mempool, maxverts * sizeof(float[3]));
			float f, *v0, *v1, projectdistance;

			e->shadowvolume = Mod_ShadowMesh_Begin(loadmodel->mempool, 1024);
#if 0
			{
			vec3_t outermins, outermaxs, innermins, innermaxs;
			innermins[0] = e->mins[0] - 1;
			innermins[1] = e->mins[1] - 1;
			innermins[2] = e->mins[2] - 1;
			innermaxs[0] = e->maxs[0] + 1;
			innermaxs[1] = e->maxs[1] + 1;
			innermaxs[2] = e->maxs[2] + 1;
			outermins[0] = loadmodel->normalmins[0] - 1;
			outermins[1] = loadmodel->normalmins[1] - 1;
			outermins[2] = loadmodel->normalmins[2] - 1;
			outermaxs[0] = loadmodel->normalmaxs[0] + 1;
			outermaxs[1] = loadmodel->normalmaxs[1] + 1;
			outermaxs[2] = loadmodel->normalmaxs[2] + 1;
			// add bounding box around the whole shadow volume set,
			// facing inward to limit light area, with an outer bounding box
			// facing outward (this is needed by the shadow rendering method)
			// X major
			verts[ 0] = innermaxs[0];verts[ 1] = innermins[1];verts[ 2] = innermaxs[2];
			verts[ 3] = innermaxs[0];verts[ 4] = innermins[1];verts[ 5] = innermins[2];
			verts[ 6] = innermaxs[0];verts[ 7] = innermaxs[1];verts[ 8] = innermins[2];
			verts[ 9] = innermaxs[0];verts[10] = innermaxs[1];verts[11] = innermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermaxs[0];verts[ 1] = outermaxs[1];verts[ 2] = outermaxs[2];
			verts[ 3] = outermaxs[0];verts[ 4] = outermaxs[1];verts[ 5] = outermins[2];
			verts[ 6] = outermaxs[0];verts[ 7] = outermins[1];verts[ 8] = outermins[2];
			verts[ 9] = outermaxs[0];verts[10] = outermins[1];verts[11] = outermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			// X minor
			verts[ 0] = innermins[0];verts[ 1] = innermaxs[1];verts[ 2] = innermaxs[2];
			verts[ 3] = innermins[0];verts[ 4] = innermaxs[1];verts[ 5] = innermins[2];
			verts[ 6] = innermins[0];verts[ 7] = innermins[1];verts[ 8] = innermins[2];
			verts[ 9] = innermins[0];verts[10] = innermins[1];verts[11] = innermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermins[0];verts[ 1] = outermins[1];verts[ 2] = outermaxs[2];
			verts[ 3] = outermins[0];verts[ 4] = outermins[1];verts[ 5] = outermins[2];
			verts[ 6] = outermins[0];verts[ 7] = outermaxs[1];verts[ 8] = outermins[2];
			verts[ 9] = outermins[0];verts[10] = outermaxs[1];verts[11] = outermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			// Y major
			verts[ 0] = innermaxs[0];verts[ 1] = innermaxs[1];verts[ 2] = innermaxs[2];
			verts[ 3] = innermaxs[0];verts[ 4] = innermaxs[1];verts[ 5] = innermins[2];
			verts[ 6] = innermins[0];verts[ 7] = innermaxs[1];verts[ 8] = innermins[2];
			verts[ 9] = innermins[0];verts[10] = innermaxs[1];verts[11] = innermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermins[0];verts[ 1] = outermaxs[1];verts[ 2] = outermaxs[2];
			verts[ 3] = outermins[0];verts[ 4] = outermaxs[1];verts[ 5] = outermins[2];
			verts[ 6] = outermaxs[0];verts[ 7] = outermaxs[1];verts[ 8] = outermins[2];
			verts[ 9] = outermaxs[0];verts[10] = outermaxs[1];verts[11] = outermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			// Y minor
			verts[ 0] = innermins[0];verts[ 1] = innermins[1];verts[ 2] = innermaxs[2];
			verts[ 3] = innermins[0];verts[ 4] = innermins[1];verts[ 5] = innermins[2];
			verts[ 6] = innermaxs[0];verts[ 7] = innermins[1];verts[ 8] = innermins[2];
			verts[ 9] = innermaxs[0];verts[10] = innermins[1];verts[11] = innermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermaxs[0];verts[ 1] = outermins[1];verts[ 2] = outermaxs[2];
			verts[ 3] = outermaxs[0];verts[ 4] = outermins[1];verts[ 5] = outermins[2];
			verts[ 6] = outermins[0];verts[ 7] = outermins[1];verts[ 8] = outermins[2];
			verts[ 9] = outermins[0];verts[10] = outermins[1];verts[11] = outermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			// Z major
			verts[ 0] = innermaxs[0];verts[ 1] = innermins[1];verts[ 2] = innermaxs[2];
			verts[ 3] = innermaxs[0];verts[ 4] = innermaxs[1];verts[ 5] = innermaxs[2];
			verts[ 6] = innermins[0];verts[ 7] = innermaxs[1];verts[ 8] = innermaxs[2];
			verts[ 9] = innermins[0];verts[10] = innermins[1];verts[11] = innermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermaxs[0];verts[ 1] = outermaxs[1];verts[ 2] = outermaxs[2];
			verts[ 3] = outermaxs[0];verts[ 4] = outermins[1];verts[ 5] = outermaxs[2];
			verts[ 6] = outermins[0];verts[ 7] = outermins[1];verts[ 8] = outermaxs[2];
			verts[ 9] = outermins[0];verts[10] = outermaxs[1];verts[11] = outermaxs[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			// Z minor
			verts[ 0] = innermaxs[0];verts[ 1] = innermaxs[1];verts[ 2] = innermins[2];
			verts[ 3] = innermaxs[0];verts[ 4] = innermins[1];verts[ 5] = innermins[2];
			verts[ 6] = innermins[0];verts[ 7] = innermins[1];verts[ 8] = innermins[2];
			verts[ 9] = innermins[0];verts[10] = innermaxs[1];verts[11] = innermins[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			verts[ 0] = outermaxs[0];verts[ 1] = outermins[1];verts[ 2] = outermins[2];
			verts[ 3] = outermaxs[0];verts[ 4] = outermaxs[1];verts[ 5] = outermins[2];
			verts[ 6] = outermins[0];verts[ 7] = outermaxs[1];verts[ 8] = outermins[2];
			verts[ 9] = outermins[0];verts[10] = outermins[1];verts[11] = outermins[2];
			Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
			}
#endif
			castshadowcount++;
			for (j = 0;j < e->numsurfaces;j++)
			{
				surf = e->surfaces[j];
				if (surf->flags & SURF_SHADOWCAST)
					surf->castshadow = castshadowcount;
			}
			for (j = 0;j < e->numsurfaces;j++)
			{
				surf = e->surfaces[j];
				if (surf->castshadow != castshadowcount)
					continue;
				f = DotProduct(e->origin, surf->plane->normal) - surf->plane->dist;
				if (surf->flags & SURF_PLANEBACK)
					f = -f;
				projectdistance = e->lightradius;
				if (maxverts < surf->poly_numverts)
				{
					maxverts = surf->poly_numverts;
					if (verts)
						Mem_Free(verts);
					verts = Mem_Alloc(loadmodel->mempool, maxverts * sizeof(float[3]));
				}
				// copy the original polygon, for the front cap of the volume
				for (k = 0, v0 = surf->poly_verts, v1 = verts;k < surf->poly_numverts;k++, v0 += 3, v1 += 3)
					VectorCopy(v0, v1);
				Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, surf->poly_numverts, verts);
				// project the original polygon, reversed, for the back cap of the volume
				for (k = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = verts;k < surf->poly_numverts;k++, v0 -= 3, v1 += 3)
				{
					VectorSubtract(v0, e->origin, temp);
					VectorNormalize(temp);
					VectorMA(v0, projectdistance, temp, v1);
				}
				Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, surf->poly_numverts, verts);
				// project the shadow volume sides
				for (l = surf->poly_numverts - 1, k = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = surf->poly_verts;k < surf->poly_numverts;l = k, k++, v0 = v1, v1 += 3)
				{
					if (!surf->neighborsurfaces[l] || surf->neighborsurfaces[l]->castshadow != castshadowcount)
					{
						VectorCopy(v1, &verts[0]);
						VectorCopy(v0, &verts[3]);
						VectorCopy(v0, &verts[6]);
						VectorCopy(v1, &verts[9]);
						VectorSubtract(&verts[6], e->origin, temp);
						VectorNormalize(temp);
						VectorMA(&verts[6], projectdistance, temp, &verts[6]);
						VectorSubtract(&verts[9], e->origin, temp);
						VectorNormalize(temp);
						VectorMA(&verts[9], projectdistance, temp, &verts[9]);
						Mod_ShadowMesh_AddPolygon(loadmodel->mempool, e->shadowvolume, 4, verts);
					}
				}
			}
			// build the triangle mesh
			e->shadowvolume = Mod_ShadowMesh_Finish(loadmodel->mempool, e->shadowvolume);
			{
				shadowmesh_t *mesh;
				l = 0;
				for (mesh = e->shadowvolume;mesh;mesh = mesh->next)
					l += mesh->numtriangles;
				Con_Printf("light %i shadow volume built containing %i triangles\n", lnum, l);
			}
		}
	}
}
*/


static void Mod_Q1BSP_LoadVisibility(lump_t *l)
{
	loadmodel->brushq1.visdata = NULL;
	if (!l->filelen)
		return;
	loadmodel->brushq1.visdata = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brushq1.visdata, mod_base + l->fileofs, l->filelen);
}

// used only for HalfLife maps
static void Mod_Q1BSP_ParseWadsFromEntityLump(const char *data)
{
	char key[128], value[4096];
	char wadname[128];
	int i, j, k;
	if (!data)
		return;
	if (!COM_ParseToken(&data))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseToken(&data))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseToken(&data))
			return; // error
		strcpy(value, com_token);
		if (!strcmp("wad", key)) // for HalfLife maps
		{
			if (loadmodel->brushq1.ishlbsp)
			{
				j = 0;
				for (i = 0;i < 4096;i++)
					if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
						break;
				if (value[i])
				{
					for (;i < 4096;i++)
					{
						// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
						if (value[i] == '\\' || value[i] == '/' || value[i] == ':')
							j = i+1;
						else if (value[i] == ';' || value[i] == 0)
						{
							k = value[i];
							value[i] = 0;
							strcpy(wadname, "textures/");
							strcat(wadname, &value[j]);
							W_LoadTextureWadFile(wadname, false);
							j = i+1;
							if (!k)
								break;
						}
					}
				}
			}
		}
	}
}

static void Mod_Q1BSP_LoadEntities(lump_t *l)
{
	loadmodel->brush.entities = NULL;
	if (!l->filelen)
		return;
	loadmodel->brush.entities = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	if (loadmodel->brushq1.ishlbsp)
		Mod_Q1BSP_ParseWadsFromEntityLump(loadmodel->brush.entities);
}


static void Mod_Q1BSP_LoadVertexes(lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadVertexes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.vertexes = out;
	loadmodel->brushq1.numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void Mod_Q1BSP_LoadSubmodels(lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadSubmodels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.submodels = out;
	loadmodel->brushq1.numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);
		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

static void Mod_Q1BSP_LoadEdges(lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq1.edges = out;
	loadmodel->brushq1.numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

static void Mod_Q1BSP_LoadTexinfo(lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadTexinfo: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq1.texinfo = out;
	loadmodel->brushq1.numtexinfo = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		for (k = 0;k < 2;k++)
			for (j = 0;j < 4;j++)
				out->vecs[k][j] = LittleFloat(in->vecs[k][j]);

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		out->texture = NULL;
		if (loadmodel->brushq1.textures)
		{
			if ((unsigned int) miptex >= (unsigned int) loadmodel->brushq1.numtextures)
				Con_Printf("error in model \"%s\": invalid miptex index %i(of %i)\n", loadmodel->name, miptex, loadmodel->brushq1.numtextures);
			else
				out->texture = loadmodel->brushq1.textures + miptex;
		}
		if (out->flags & TEX_SPECIAL)
		{
			// if texture chosen is NULL or the shader needs a lightmap,
			// force to notexture water shader
			if (out->texture == NULL || out->texture->shader->flags & SHADERFLAGS_NEEDLIGHTMAP)
				out->texture = loadmodel->brushq1.textures + (loadmodel->brushq1.numtextures - 1);
		}
		else
		{
			// if texture chosen is NULL, force to notexture
			if (out->texture == NULL)
				out->texture = loadmodel->brushq1.textures + (loadmodel->brushq1.numtextures - 2);
		}
	}
}

#if 0
void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0;i < numverts;i++)
	{
		for (j = 0;j < 3;j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

#define MAX_SUBDIVPOLYTRIANGLES 4096
#define MAX_SUBDIVPOLYVERTS(MAX_SUBDIVPOLYTRIANGLES * 3)

static int subdivpolyverts, subdivpolytriangles;
static int subdivpolyindex[MAX_SUBDIVPOLYTRIANGLES][3];
static float subdivpolyvert[MAX_SUBDIVPOLYVERTS][3];

static int subdivpolylookupvert(vec3_t v)
{
	int i;
	for (i = 0;i < subdivpolyverts;i++)
		if (subdivpolyvert[i][0] == v[0]
		 && subdivpolyvert[i][1] == v[1]
		 && subdivpolyvert[i][2] == v[2])
			return i;
	if (subdivpolyverts >= MAX_SUBDIVPOLYVERTS)
		Host_Error("SubDividePolygon: ran out of vertices in buffer, please increase your r_subdivide_size");
	VectorCopy(v, subdivpolyvert[subdivpolyverts]);
	return subdivpolyverts++;
}

static void SubdividePolygon(int numverts, float *verts)
{
	int		i, i1, i2, i3, f, b, c, p;
	vec3_t	mins, maxs, front[256], back[256];
	float	m, *pv, *cv, dist[256], frac;

	if (numverts > 250)
		Host_Error("SubdividePolygon: ran out of verts in buffer");

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0;i < 3;i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = r_subdivide_size.value * floor(m/r_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		for (cv = verts, c = 0;c < numverts;c++, cv += 3)
			dist[c] = cv[i] - m;

		f = b = 0;
		for (p = numverts - 1, c = 0, pv = verts + p * 3, cv = verts;c < numverts;p = c, c++, pv = cv, cv += 3)
		{
			if (dist[p] >= 0)
			{
				VectorCopy(pv, front[f]);
				f++;
			}
			if (dist[p] <= 0)
			{
				VectorCopy(pv, back[b]);
				b++;
			}
			if (dist[p] == 0 || dist[c] == 0)
				continue;
			if ((dist[p] > 0) != (dist[c] > 0) )
			{
				// clip point
				frac = dist[p] / (dist[p] - dist[c]);
				front[f][0] = back[b][0] = pv[0] + frac * (cv[0] - pv[0]);
				front[f][1] = back[b][1] = pv[1] + frac * (cv[1] - pv[1]);
				front[f][2] = back[b][2] = pv[2] + frac * (cv[2] - pv[2]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		return;
	}

	i1 = subdivpolylookupvert(verts);
	i2 = subdivpolylookupvert(verts + 3);
	for (i = 2;i < numverts;i++)
	{
		if (subdivpolytriangles >= MAX_SUBDIVPOLYTRIANGLES)
		{
			Con_Printf("SubdividePolygon: ran out of triangles in buffer, please increase your r_subdivide_size\n");
			return;
		}

		i3 = subdivpolylookupvert(verts + i * 3);
		subdivpolyindex[subdivpolytriangles][0] = i1;
		subdivpolyindex[subdivpolytriangles][1] = i2;
		subdivpolyindex[subdivpolytriangles][2] = i3;
		i2 = i3;
		subdivpolytriangles++;
	}
}

//Breaks a polygon up along axial 64 unit
//boundaries so that turbulent and sky warps
//can be done reasonably.
static void Mod_Q1BSP_GenerateWarpMesh(msurface_t *surf)
{
	int i, j;
	surfvertex_t *v;
	surfmesh_t *mesh;

	subdivpolytriangles = 0;
	subdivpolyverts = 0;
	SubdividePolygon(surf->poly_numverts, surf->poly_verts);
	if (subdivpolytriangles < 1)
		Host_Error("Mod_Q1BSP_GenerateWarpMesh: no triangles?\n");

	surf->mesh = mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + subdivpolytriangles * sizeof(int[3]) + subdivpolyverts * sizeof(surfvertex_t));
	mesh->numverts = subdivpolyverts;
	mesh->numtriangles = subdivpolytriangles;
	mesh->vertex = (surfvertex_t *)(mesh + 1);
	mesh->index = (int *)(mesh->vertex + mesh->numverts);
	memset(mesh->vertex, 0, mesh->numverts * sizeof(surfvertex_t));

	for (i = 0;i < mesh->numtriangles;i++)
		for (j = 0;j < 3;j++)
			mesh->index[i*3+j] = subdivpolyindex[i][j];

	for (i = 0, v = mesh->vertex;i < subdivpolyverts;i++, v++)
	{
		VectorCopy(subdivpolyvert[i], v->v);
		v->st[0] = DotProduct(v->v, surf->texinfo->vecs[0]);
		v->st[1] = DotProduct(v->v, surf->texinfo->vecs[1]);
	}
}
#endif

static surfmesh_t *Mod_Q1BSP_AllocSurfMesh(int numverts, int numtriangles)
{
	surfmesh_t *mesh;
	mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + numtriangles * sizeof(int[6]) + numverts * (3 + 2 + 2 + 2 + 3 + 3 + 3 + 1) * sizeof(float));
	mesh->numverts = numverts;
	mesh->numtriangles = numtriangles;
	mesh->vertex3f = (float *)(mesh + 1);
	mesh->texcoordtexture2f = mesh->vertex3f + mesh->numverts * 3;
	mesh->texcoordlightmap2f = mesh->texcoordtexture2f + mesh->numverts * 2;
	mesh->texcoorddetail2f = mesh->texcoordlightmap2f + mesh->numverts * 2;
	mesh->svector3f = (float *)(mesh->texcoorddetail2f + mesh->numverts * 2);
	mesh->tvector3f = mesh->svector3f + mesh->numverts * 3;
	mesh->normal3f = mesh->tvector3f + mesh->numverts * 3;
	mesh->lightmapoffsets = (int *)(mesh->normal3f + mesh->numverts * 3);
	mesh->element3i = mesh->lightmapoffsets + mesh->numverts;
	mesh->neighbor3i = mesh->element3i + mesh->numtriangles * 3;
	return mesh;
}

static void Mod_Q1BSP_GenerateSurfacePolygon(msurface_t *surf, int firstedge, int numedges)
{
	int i, lindex, j;
	float *vec, *vert, mins[3], maxs[3], val, *v;
	mtexinfo_t *tex;

	// convert edges back to a normal polygon
	surf->poly_numverts = numedges;
	vert = surf->poly_verts = Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * numedges);
	for (i = 0;i < numedges;i++)
	{
		lindex = loadmodel->brushq1.surfedges[firstedge + i];
		if (lindex > 0)
			vec = loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[lindex].v[0]].position;
		else
			vec = loadmodel->brushq1.vertexes[loadmodel->brushq1.edges[-lindex].v[1]].position;
		VectorCopy(vec, vert);
		vert += 3;
	}

	// calculate polygon bounding box and center
	vert = surf->poly_verts;
	VectorCopy(vert, mins);
	VectorCopy(vert, maxs);
	vert += 3;
	for (i = 1;i < surf->poly_numverts;i++, vert += 3)
	{
		if (mins[0] > vert[0]) mins[0] = vert[0];if (maxs[0] < vert[0]) maxs[0] = vert[0];
		if (mins[1] > vert[1]) mins[1] = vert[1];if (maxs[1] < vert[1]) maxs[1] = vert[1];
		if (mins[2] > vert[2]) mins[2] = vert[2];if (maxs[2] < vert[2]) maxs[2] = vert[2];
	}
	VectorCopy(mins, surf->poly_mins);
	VectorCopy(maxs, surf->poly_maxs);
	surf->poly_center[0] = (mins[0] + maxs[0]) * 0.5f;
	surf->poly_center[1] = (mins[1] + maxs[1]) * 0.5f;
	surf->poly_center[2] = (mins[2] + maxs[2]) * 0.5f;

	// generate surface extents information
	tex = surf->texinfo;
	mins[0] = maxs[0] = DotProduct(surf->poly_verts, tex->vecs[0]) + tex->vecs[0][3];
	mins[1] = maxs[1] = DotProduct(surf->poly_verts, tex->vecs[1]) + tex->vecs[1][3];
	for (i = 1, v = surf->poly_verts + 3;i < surf->poly_numverts;i++, v += 3)
	{
		for (j = 0;j < 2;j++)
		{
			val = DotProduct(v, tex->vecs[j]) + tex->vecs[j][3];
			if (mins[j] > val)
				mins[j] = val;
			if (maxs[j] < val)
				maxs[j] = val;
		}
	}
	for (i = 0;i < 2;i++)
	{
		surf->texturemins[i] = (int) floor(mins[i] / 16) * 16;
		surf->extents[i] = (int) ceil(maxs[i] / 16) * 16 - surf->texturemins[i];
	}
}

static void Mod_Q1BSP_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *surf;
	int i, count, surfnum, planenum, ssize, tsize, firstedge, numedges, totalverts, totaltris, totalmeshes;
	surfmesh_t *mesh;
	float s, t;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	loadmodel->brushq1.surfaces = Mem_Alloc(loadmodel->mempool, count*sizeof(msurface_t));

	loadmodel->brushq1.numsurfaces = count;
	loadmodel->brushq1.surfacevisframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));
	loadmodel->brushq1.surfacepvsframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));
	loadmodel->brushq1.pvssurflist = Mem_Alloc(loadmodel->mempool, count * sizeof(int));

	for (surfnum = 0, surf = loadmodel->brushq1.surfaces, totalverts = 0, totaltris = 0, totalmeshes = 0;surfnum < count;surfnum++, totalverts += surf->poly_numverts, totaltris += surf->poly_numverts - 2, totalmeshes++, in++, surf++)
	{
		surf->number = surfnum;
		// FIXME: validate edges, texinfo, etc?
		firstedge = LittleLong(in->firstedge);
		numedges = LittleShort(in->numedges);
		if ((unsigned int) firstedge > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges || (unsigned int) firstedge + (unsigned int) numedges > (unsigned int) loadmodel->brushq1.numsurfedges)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid edge range (firstedge %i, numedges %i, model edges %i)\n", firstedge, numedges, loadmodel->brushq1.numsurfedges);
		i = LittleShort(in->texinfo);
		if ((unsigned int) i >= (unsigned int) loadmodel->brushq1.numtexinfo)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid texinfo index %i(model has %i texinfos)\n", i, loadmodel->brushq1.numtexinfo);
		surf->texinfo = loadmodel->brushq1.texinfo + i;
		surf->flags = surf->texinfo->texture->flags;

		planenum = LittleShort(in->planenum);
		if ((unsigned int) planenum >= (unsigned int) loadmodel->brushq1.numplanes)
			Host_Error("Mod_Q1BSP_LoadFaces: invalid plane index %i (model has %i planes)\n", planenum, loadmodel->brushq1.numplanes);

		if (LittleShort(in->side))
			surf->flags |= SURF_PLANEBACK;

		surf->plane = loadmodel->brushq1.planes + planenum;

		// clear lightmap (filled in later)
		surf->lightmaptexture = NULL;

		// force lightmap upload on first time seeing the surface
		surf->cached_dlight = true;

		Mod_Q1BSP_GenerateSurfacePolygon(surf, firstedge, numedges);

		ssize = (surf->extents[0] >> 4) + 1;
		tsize = (surf->extents[1] >> 4) + 1;

		// lighting info
		for (i = 0;i < MAXLIGHTMAPS;i++)
			surf->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			surf->samples = NULL;
		else if (loadmodel->brushq1.ishlbsp) // LordHavoc: HalfLife map (bsp version 30)
			surf->samples = loadmodel->brushq1.lightdata + i;
		else // LordHavoc: white lighting (bsp version 29)
			surf->samples = loadmodel->brushq1.lightdata + (i * 3);

		if (surf->texinfo->texture->shader == &Cshader_wall_lightmap)
		{
			if ((surf->extents[0] >> 4) + 1 > (256) || (surf->extents[1] >> 4) + 1 > (256))
				Host_Error("Bad surface extents");
			// stainmap for permanent marks on walls
			surf->stainsamples = Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
			// clear to white
			memset(surf->stainsamples, 255, ssize * tsize * 3);
		}
	}

	loadmodel->brushq1.entiremesh = Mod_Q1BSP_AllocSurfMesh(totalverts, totaltris);
	loadmodel->brushq1.surfmeshes = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) * totalmeshes);

	for (surfnum = 0, surf = loadmodel->brushq1.surfaces, totalverts = 0, totaltris = 0, totalmeshes = 0;surfnum < count;surfnum++, totalverts += surf->poly_numverts, totaltris += surf->poly_numverts - 2, totalmeshes++, surf++)
	{
		mesh = surf->mesh = loadmodel->brushq1.surfmeshes + totalmeshes;
		mesh->numverts = surf->poly_numverts;
		mesh->numtriangles = surf->poly_numverts - 2;
		mesh->vertex3f = loadmodel->brushq1.entiremesh->vertex3f + totalverts * 3;
		mesh->texcoordtexture2f = loadmodel->brushq1.entiremesh->texcoordtexture2f + totalverts * 2;
		mesh->texcoordlightmap2f = loadmodel->brushq1.entiremesh->texcoordlightmap2f + totalverts * 2;
		mesh->texcoorddetail2f = loadmodel->brushq1.entiremesh->texcoorddetail2f + totalverts * 2;
		mesh->svector3f = loadmodel->brushq1.entiremesh->svector3f + totalverts * 3;
		mesh->tvector3f = loadmodel->brushq1.entiremesh->tvector3f + totalverts * 3;
		mesh->normal3f = loadmodel->brushq1.entiremesh->normal3f + totalverts * 3;
		mesh->lightmapoffsets = loadmodel->brushq1.entiremesh->lightmapoffsets + totalverts;
		mesh->element3i = loadmodel->brushq1.entiremesh->element3i + totaltris * 3;
		mesh->neighbor3i = loadmodel->brushq1.entiremesh->neighbor3i + totaltris * 3;

		surf->lightmaptexturestride = 0;
		surf->lightmaptexture = NULL;

		for (i = 0;i < mesh->numverts;i++)
		{
			mesh->vertex3f[i * 3 + 0] = surf->poly_verts[i * 3 + 0];
			mesh->vertex3f[i * 3 + 1] = surf->poly_verts[i * 3 + 1];
			mesh->vertex3f[i * 3 + 2] = surf->poly_verts[i * 3 + 2];
			s = DotProduct((mesh->vertex3f + i * 3), surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			t = DotProduct((mesh->vertex3f + i * 3), surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			mesh->texcoordtexture2f[i * 2 + 0] = s / surf->texinfo->texture->width;
			mesh->texcoordtexture2f[i * 2 + 1] = t / surf->texinfo->texture->height;
			mesh->texcoorddetail2f[i * 2 + 0] = s * (1.0f / 16.0f);
			mesh->texcoorddetail2f[i * 2 + 1] = t * (1.0f / 16.0f);
			mesh->texcoordlightmap2f[i * 2 + 0] = 0;
			mesh->texcoordlightmap2f[i * 2 + 1] = 0;
			mesh->lightmapoffsets[i] = 0;
		}

		for (i = 0;i < mesh->numtriangles;i++)
		{
			mesh->element3i[i * 3 + 0] = 0;
			mesh->element3i[i * 3 + 1] = i + 1;
			mesh->element3i[i * 3 + 2] = i + 2;
		}

		Mod_BuildTriangleNeighbors(mesh->neighbor3i, mesh->element3i, mesh->numtriangles);
		Mod_BuildTextureVectorsAndNormals(mesh->numverts, mesh->numtriangles, mesh->vertex3f, mesh->texcoordtexture2f, mesh->element3i, mesh->svector3f, mesh->tvector3f, mesh->normal3f);

		if (surf->texinfo->texture->shader == &Cshader_wall_lightmap)
		{
			int i, iu, iv, smax, tmax;
			float u, v, ubase, vbase, uscale, vscale;

			smax = surf->extents[0] >> 4;
			tmax = surf->extents[1] >> 4;

			surf->flags |= SURF_LIGHTMAP;
			if (r_miplightmaps.integer)
			{
				surf->lightmaptexturestride = smax+1;
				surf->lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_MIPMAP | TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
			}
			else
			{
				surf->lightmaptexturestride = R_CompatibleFragmentWidth(smax+1, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, 0);
				surf->lightmaptexture = R_LoadTexture2D(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->brushq1.lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_FRAGMENT | TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
			}
			R_FragmentLocation(surf->lightmaptexture, NULL, NULL, &ubase, &vbase, &uscale, &vscale);
			uscale = (uscale - ubase) / (smax + 1);
			vscale = (vscale - vbase) / (tmax + 1);

			for (i = 0;i < mesh->numverts;i++)
			{
				u = ((DotProduct((mesh->vertex3f + i * 3), surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) + 8 - surf->texturemins[0]) * (1.0 / 16.0);
				v = ((DotProduct((mesh->vertex3f + i * 3), surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) + 8 - surf->texturemins[1]) * (1.0 / 16.0);
				mesh->texcoordlightmap2f[i * 2 + 0] = u * uscale + ubase;
				mesh->texcoordlightmap2f[i * 2 + 1] = v * vscale + vbase;
				// LordHavoc: calc lightmap data offset for vertex lighting to use
				iu = (int) u;
				iv = (int) v;
				mesh->lightmapoffsets[i] = (bound(0, iv, tmax) * (smax+1) + bound(0, iu, smax)) * 3;
			}
		}
	}
}

static void Mod_Q1BSP_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_Q1BSP_SetParent(node->children[0], node);
	Mod_Q1BSP_SetParent(node->children[1], node);
}

static void Mod_Q1BSP_LoadNodes(lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.nodes = out;
	loadmodel->brushq1.numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->brushq1.planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->brushq1.nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->brushq1.leafs + (-1 - p));
		}
	}

	Mod_Q1BSP_SetParent(loadmodel->brushq1.nodes, NULL);	// sets nodes and leafs
}

static void Mod_Q1BSP_LoadLeafs(lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.leafs = out;
	loadmodel->brushq1.numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->brushq1.marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->brushq1.visdata + p;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// FIXME: Insert caustics here
	}
}

static void Mod_Q1BSP_LoadClipnodes(lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadClipnodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->brushq1.clipnodes = out;
	loadmodel->brushq1.numclipnodes = count;

	if (loadmodel->brushq1.ishlbsp)
	{
		hull = &loadmodel->brushq1.hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -18;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 18;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);
	}
	else
	{
		hull = &loadmodel->brushq1.hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->brushq1.hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->brushq1.planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 64;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);
	}

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
		if (out->children[0] >= count || out->children[1] >= count)
			Host_Error("Corrupt clipping hull(out of range child)\n");
	}
}

//Duplicate the drawing hull structure as a clipping hull
static void Mod_Q1BSP_MakeHull0(void)
{
	mnode_t		*in;
	dclipnode_t *out;
	int			i;
	hull_t		*hull;

	hull = &loadmodel->brushq1.hulls[0];

	in = loadmodel->brushq1.nodes;
	out = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numnodes * sizeof(dclipnode_t));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = loadmodel->brushq1.numnodes - 1;
	hull->planes = loadmodel->brushq1.planes;

	for (i = 0;i < loadmodel->brushq1.numnodes;i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->brushq1.planes;
		out->children[0] = in->children[0]->contents < 0 ? in->children[0]->contents : in->children[0] - loadmodel->brushq1.nodes;
		out->children[1] = in->children[1]->contents < 0 ? in->children[1]->contents : in->children[1] - loadmodel->brushq1.nodes;
	}
}

static void Mod_Q1BSP_LoadMarksurfaces(lump_t *l)
{
	int i, j;
	short *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadMarksurfaces: funny lump size in %s",loadmodel->name);
	loadmodel->brushq1.nummarksurfaces = l->filelen / sizeof(*in);
	loadmodel->brushq1.marksurfaces = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.nummarksurfaces * sizeof(int));

	for (i = 0;i < loadmodel->brushq1.nummarksurfaces;i++)
	{
		j = (unsigned) LittleShort(in[i]);
		if (j >= loadmodel->brushq1.numsurfaces)
			Host_Error("Mod_Q1BSP_LoadMarksurfaces: bad surface number");
		loadmodel->brushq1.marksurfaces[i] = j;
	}
}

static void Mod_Q1BSP_LoadSurfedges(lump_t *l)
{
	int		i;
	int		*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadSurfedges: funny lump size in %s",loadmodel->name);
	loadmodel->brushq1.numsurfedges = l->filelen / sizeof(*in);
	loadmodel->brushq1.surfedges = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numsurfedges * sizeof(int));

	for (i = 0;i < loadmodel->brushq1.numsurfedges;i++)
		loadmodel->brushq1.surfedges[i] = LittleLong(in[i]);
}


static void Mod_Q1BSP_LoadPlanes(lump_t *l)
{
	int			i;
	mplane_t	*out;
	dplane_t 	*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q1BSP_LoadPlanes: funny lump size in %s", loadmodel->name);

	loadmodel->brushq1.numplanes = l->filelen / sizeof(*in);
	loadmodel->brushq1.planes = out = Mem_Alloc(loadmodel->mempool, loadmodel->brushq1.numplanes * sizeof(*out));

	for (i = 0;i < loadmodel->brushq1.numplanes;i++, in++, out++)
	{
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
		out->dist = LittleFloat(in->dist);

		PlaneClassify(out);
	}
}

#define MAX_POINTS_ON_WINDING 64

typedef struct
{
	int numpoints;
	int padding;
	double points[8][3]; // variable sized
}
winding_t;

/*
==================
NewWinding
==================
*/
static winding_t *NewWinding(int points)
{
	winding_t *w;
	int size;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error("NewWinding: too many points\n");

	size = sizeof(winding_t) + sizeof(double[3]) * (points - 8);
	w = Mem_Alloc(loadmodel->mempool, size);
	memset(w, 0, size);

	return w;
}

static void FreeWinding(winding_t *w)
{
	Mem_Free(w);
}

/*
=================
BaseWindingForPlane
=================
*/
static winding_t *BaseWindingForPlane(mplane_t *p)
{
	double org[3], vright[3], vup[3], normal[3];
	winding_t *w;

	VectorCopy(p->normal, normal);
	VectorVectorsDouble(normal, vright, vup);

	VectorScale(vup, 1024.0*1024.0*1024.0, vup);
	VectorScale(vright, 1024.0*1024.0*1024.0, vright);

	// project a really big	axis aligned box onto the plane
	w = NewWinding(4);

	VectorScale(p->normal, p->dist, org);

	VectorSubtract(org, vright, w->points[0]);
	VectorAdd(w->points[0], vup, w->points[0]);

	VectorAdd(org, vright, w->points[1]);
	VectorAdd(w->points[1], vup, w->points[1]);

	VectorAdd(org, vright, w->points[2]);
	VectorSubtract(w->points[2], vup, w->points[2]);

	VectorSubtract(org, vright, w->points[3]);
	VectorSubtract(w->points[3], vup, w->points[3]);

	w->numpoints = 4;

	return w;
}

/*
==================
ClipWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
If keepon is true, an exactly on-plane winding will be saved, otherwise
it will be clipped away.
==================
*/
static winding_t *ClipWinding(winding_t *in, mplane_t *split, int keepon)
{
	double	dists[MAX_POINTS_ON_WINDING + 1];
	int		sides[MAX_POINTS_ON_WINDING + 1];
	int		counts[3];
	double	dot;
	int		i, j;
	double	*p1, *p2;
	double	mid[3];
	winding_t	*neww;
	int		maxpts;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < in->numpoints;i++)
	{
		dists[i] = dot = DotProduct(in->points[i], split->normal) - split->dist;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (keepon && !counts[0] && !counts[1])
		return in;

	if (!counts[0])
	{
		FreeWinding(in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because of fp grouping errors
	if (maxpts > MAX_POINTS_ON_WINDING)
		Sys_Error("ClipWinding: maxpts > MAX_POINTS_ON_WINDING");

	neww = NewWinding(maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
		if (neww->numpoints >= maxpts)
			Sys_Error("ClipWinding: points exceeded estimate");

		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy(p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy(p1, neww->points[neww->numpoints]);
			neww->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		VectorCopy(mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	// free the original winding
	FreeWinding(in);

	return neww;
}


/*
==================
DivideWinding

Divides a winding by a plane, producing one or two windings.  The
original winding is not damaged or freed.  If only on one side, the
returned winding will be the input winding.  If on both sides, two
new windings will be created.
==================
*/
static void DivideWinding(winding_t *in, mplane_t *split, winding_t **front, winding_t **back)
{
	double	dists[MAX_POINTS_ON_WINDING + 1];
	int		sides[MAX_POINTS_ON_WINDING + 1];
	int		counts[3];
	double	dot;
	int		i, j;
	double	*p1, *p2;
	double	mid[3];
	winding_t	*f, *b;
	int		maxpts;

	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

	// determine sides for each point
	for (i = 0;i < in->numpoints;i++)
	{
		dot = DotProduct(in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON) sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON) sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if (!counts[0])
	{
		*back = in;
		return;
	}
	if (!counts[1])
	{
		*front = in;
		return;
	}

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because of fp grouping errors

	if (maxpts > MAX_POINTS_ON_WINDING)
		Sys_Error("ClipWinding: maxpts > MAX_POINTS_ON_WINDING");

	*front = f = NewWinding(maxpts);
	*back = b = NewWinding(maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
		if (f->numpoints >= maxpts || b->numpoints >= maxpts)
			Sys_Error("DivideWinding: points exceeded estimate");

		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy(p1, f->points[f->numpoints]);
			f->numpoints++;
			VectorCopy(p1, b->points[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy(p1, f->points[f->numpoints]);
			f->numpoints++;
		}
		else if (sides[i] == SIDE_BACK)
		{
			VectorCopy(p1, b->points[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		// generate a split point
		p2 = in->points[(i+1)%in->numpoints];

		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j = 0;j < 3;j++)
		{	// avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			else
				mid[j] = p1[j] + dot* (p2[j]-p1[j]);
		}

		VectorCopy(mid, f->points[f->numpoints]);
		f->numpoints++;
		VectorCopy(mid, b->points[b->numpoints]);
		b->numpoints++;
	}
}

typedef struct portal_s
{
	mplane_t plane;
	mnode_t *nodes[2];		// [0] = front side of plane
	struct portal_s *next[2];
	winding_t *winding;
	struct portal_s *chain; // all portals are linked into a list
}
portal_t;

static portal_t *portalchain;

/*
===========
AllocPortal
===========
*/
static portal_t *AllocPortal(void)
{
	portal_t *p;
	p = Mem_Alloc(loadmodel->mempool, sizeof(portal_t));
	p->chain = portalchain;
	portalchain = p;
	return p;
}

static void FreePortal(portal_t *p)
{
	Mem_Free(p);
}

static void Mod_Q1BSP_RecursiveRecalcNodeBBox(mnode_t *node)
{
	// calculate children first
	if (node->children[0]->contents >= 0)
		Mod_Q1BSP_RecursiveRecalcNodeBBox(node->children[0]);
	if (node->children[1]->contents >= 0)
		Mod_Q1BSP_RecursiveRecalcNodeBBox(node->children[1]);

	// make combined bounding box from children
	node->mins[0] = min(node->children[0]->mins[0], node->children[1]->mins[0]);
	node->mins[1] = min(node->children[0]->mins[1], node->children[1]->mins[1]);
	node->mins[2] = min(node->children[0]->mins[2], node->children[1]->mins[2]);
	node->maxs[0] = max(node->children[0]->maxs[0], node->children[1]->maxs[0]);
	node->maxs[1] = max(node->children[0]->maxs[1], node->children[1]->maxs[1]);
	node->maxs[2] = max(node->children[0]->maxs[2], node->children[1]->maxs[2]);
}

static void Mod_Q1BSP_FinalizePortals(void)
{
	int i, j, numportals, numpoints;
	portal_t *p, *pnext;
	mportal_t *portal;
	mvertex_t *point;
	mleaf_t *leaf, *endleaf;
	winding_t *w;

	// recalculate bounding boxes for all leafs(because qbsp is very sloppy)
	leaf = loadmodel->brushq1.leafs;
	endleaf = leaf + loadmodel->brushq1.numleafs;
	for (;leaf < endleaf;leaf++)
	{
		VectorSet(leaf->mins,  2000000000,  2000000000,  2000000000);
		VectorSet(leaf->maxs, -2000000000, -2000000000, -2000000000);
	}
	p = portalchain;
	while (p)
	{
		if (p->winding)
		{
			for (i = 0;i < 2;i++)
			{
				leaf = (mleaf_t *)p->nodes[i];
				w = p->winding;
				for (j = 0;j < w->numpoints;j++)
				{
					if (leaf->mins[0] > w->points[j][0]) leaf->mins[0] = w->points[j][0];
					if (leaf->mins[1] > w->points[j][1]) leaf->mins[1] = w->points[j][1];
					if (leaf->mins[2] > w->points[j][2]) leaf->mins[2] = w->points[j][2];
					if (leaf->maxs[0] < w->points[j][0]) leaf->maxs[0] = w->points[j][0];
					if (leaf->maxs[1] < w->points[j][1]) leaf->maxs[1] = w->points[j][1];
					if (leaf->maxs[2] < w->points[j][2]) leaf->maxs[2] = w->points[j][2];
				}
			}
		}
		p = p->chain;
	}

	Mod_Q1BSP_RecursiveRecalcNodeBBox(loadmodel->brushq1.nodes);

	// tally up portal and point counts
	p = portalchain;
	numportals = 0;
	numpoints = 0;
	while (p)
	{
		// note: this check must match the one below or it will usually corrupt memory
		// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
		if (p->winding && p->nodes[0] != p->nodes[1]
		 && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID
		 && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY)
		{
			numportals += 2;
			numpoints += p->winding->numpoints * 2;
		}
		p = p->chain;
	}
	loadmodel->brushq1.portals = Mem_Alloc(loadmodel->mempool, numportals * sizeof(mportal_t) + numpoints * sizeof(mvertex_t));
	loadmodel->brushq1.numportals = numportals;
	loadmodel->brushq1.portalpoints = (void *)((qbyte *) loadmodel->brushq1.portals + numportals * sizeof(mportal_t));
	loadmodel->brushq1.numportalpoints = numpoints;
	// clear all leaf portal chains
	for (i = 0;i < loadmodel->brushq1.numleafs;i++)
		loadmodel->brushq1.leafs[i].portals = NULL;
	// process all portals in the global portal chain, while freeing them
	portal = loadmodel->brushq1.portals;
	point = loadmodel->brushq1.portalpoints;
	p = portalchain;
	portalchain = NULL;
	while (p)
	{
		pnext = p->chain;

		if (p->winding)
		{
			// note: this check must match the one above or it will usually corrupt memory
			// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
			if (p->nodes[0] != p->nodes[1]
			 && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID
			 && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY)
			{
				// first make the back to front portal(forward portal)
				portal->points = point;
				portal->numpoints = p->winding->numpoints;
				portal->plane.dist = p->plane.dist;
				VectorCopy(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[1];
				portal->past = (mleaf_t *)p->nodes[0];
				// copy points
				for (j = 0;j < portal->numpoints;j++)
				{
					VectorCopy(p->winding->points[j], point->position);
					point++;
				}
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;

				// then make the front to back portal(backward portal)
				portal->points = point;
				portal->numpoints = p->winding->numpoints;
				portal->plane.dist = -p->plane.dist;
				VectorNegate(p->plane.normal, portal->plane.normal);
				portal->here = (mleaf_t *)p->nodes[0];
				portal->past = (mleaf_t *)p->nodes[1];
				// copy points
				for (j = portal->numpoints - 1;j >= 0;j--)
				{
					VectorCopy(p->winding->points[j], point->position);
					point++;
				}
				PlaneClassify(&portal->plane);

				// link into leaf's portal chain
				portal->next = portal->here->portals;
				portal->here->portals = portal;

				// advance to next portal
				portal++;
			}
			FreeWinding(p->winding);
		}
		FreePortal(p);
		p = pnext;
	}
}

/*
=============
AddPortalToNodes
=============
*/
static void AddPortalToNodes(portal_t *p, mnode_t *front, mnode_t *back)
{
	if (!front)
		Host_Error("AddPortalToNodes: NULL front node");
	if (!back)
		Host_Error("AddPortalToNodes: NULL back node");
	if (p->nodes[0] || p->nodes[1])
		Host_Error("AddPortalToNodes: already included");
	// note: front == back is handled gracefully, because leaf 0 is the shared solid leaf, it can often have portals with the same leaf on both sides

	p->nodes[0] = front;
	p->next[0] = (portal_t *)front->portals;
	front->portals = (mportal_t *)p;

	p->nodes[1] = back;
	p->next[1] = (portal_t *)back->portals;
	back->portals = (mportal_t *)p;
}

/*
=============
RemovePortalFromNode
=============
*/
static void RemovePortalFromNodes(portal_t *portal)
{
	int i;
	mnode_t *node;
	void **portalpointer;
	portal_t *t;
	for (i = 0;i < 2;i++)
	{
		node = portal->nodes[i];

		portalpointer = (void **) &node->portals;
		while (1)
		{
			t = *portalpointer;
			if (!t)
				Host_Error("RemovePortalFromNodes: portal not in leaf");

			if (t == portal)
			{
				if (portal->nodes[0] == node)
				{
					*portalpointer = portal->next[0];
					portal->nodes[0] = NULL;
				}
				else if (portal->nodes[1] == node)
				{
					*portalpointer = portal->next[1];
					portal->nodes[1] = NULL;
				}
				else
					Host_Error("RemovePortalFromNodes: portal not bounding leaf");
				break;
			}

			if (t->nodes[0] == node)
				portalpointer = (void **) &t->next[0];
			else if (t->nodes[1] == node)
				portalpointer = (void **) &t->next[1];
			else
				Host_Error("RemovePortalFromNodes: portal not bounding leaf");
		}
	}
}

static void Mod_Q1BSP_RecursiveNodePortals(mnode_t *node)
{
	int side;
	mnode_t *front, *back, *other_node;
	mplane_t clipplane, *plane;
	portal_t *portal, *nextportal, *nodeportal, *splitportal, *temp;
	winding_t *nodeportalwinding, *frontwinding, *backwinding;

	// if a leaf, we're done
	if (node->contents)
		return;

	plane = node->plane;

	front = node->children[0];
	back = node->children[1];
	if (front == back)
		Host_Error("Mod_Q1BSP_RecursiveNodePortals: corrupt node hierarchy");

	// create the new portal by generating a polygon for the node plane,
	// and clipping it by all of the other portals(which came from nodes above this one)
	nodeportal = AllocPortal();
	nodeportal->plane = *node->plane;

	nodeportalwinding = BaseWindingForPlane(node->plane);
	side = 0;	// shut up compiler warning
	for (portal = (portal_t *)node->portals;portal;portal = portal->next[side])
	{
		clipplane = portal->plane;
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: portal has same node on both sides(1)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
		{
			clipplane.dist = -clipplane.dist;
			VectorNegate(clipplane.normal, clipplane.normal);
			side = 1;
		}
		else
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: mislinked portal");

		nodeportalwinding = ClipWinding(nodeportalwinding, &clipplane, true);
		if (!nodeportalwinding)
		{
			Con_Printf("Mod_Q1BSP_RecursiveNodePortals: WARNING: new portal was clipped away\n");
			break;
		}
	}

	if (nodeportalwinding)
	{
		// if the plane was not clipped on all sides, there was an error
		nodeportal->winding = nodeportalwinding;
		AddPortalToNodes(nodeportal, front, back);
	}

	// split the portals of this node along this node's plane and assign them to the children of this node
	// (migrating the portals downward through the tree)
	for (portal = (portal_t *)node->portals;portal;portal = nextportal)
	{
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: portal has same node on both sides(2)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
			side = 1;
		else
			Host_Error("Mod_Q1BSP_RecursiveNodePortals: mislinked portal");
		nextportal = portal->next[side];

		other_node = portal->nodes[!side];
		RemovePortalFromNodes(portal);

		// cut the portal into two portals, one on each side of the node plane
		DivideWinding(portal->winding, plane, &frontwinding, &backwinding);

		if (!frontwinding)
		{
			if (side == 0)
				AddPortalToNodes(portal, back, other_node);
			else
				AddPortalToNodes(portal, other_node, back);
			continue;
		}
		if (!backwinding)
		{
			if (side == 0)
				AddPortalToNodes(portal, front, other_node);
			else
				AddPortalToNodes(portal, other_node, front);
			continue;
		}

		// the winding is split
		splitportal = AllocPortal();
		temp = splitportal->chain;
		*splitportal = *portal;
		splitportal->chain = temp;
		splitportal->winding = backwinding;
		FreeWinding(portal->winding);
		portal->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes(portal, front, other_node);
			AddPortalToNodes(splitportal, back, other_node);
		}
		else
		{
			AddPortalToNodes(portal, other_node, front);
			AddPortalToNodes(splitportal, other_node, back);
		}
	}

	Mod_Q1BSP_RecursiveNodePortals(front);
	Mod_Q1BSP_RecursiveNodePortals(back);
}


static void Mod_Q1BSP_MakePortals(void)
{
	portalchain = NULL;
	Mod_Q1BSP_RecursiveNodePortals(loadmodel->brushq1.nodes);
	Mod_Q1BSP_FinalizePortals();
}

static void Mod_Q1BSP_BuildSurfaceNeighbors(msurface_t *surfaces, int numsurfaces, mempool_t *mempool)
{
#if 0
	int surfnum, vertnum, vertnum2, snum, vnum, vnum2;
	msurface_t *surf, *s;
	float *v0, *v1, *v2, *v3;
	for (surf = surfaces, surfnum = 0;surfnum < numsurfaces;surf++, surfnum++)
		surf->neighborsurfaces = Mem_Alloc(mempool, surf->poly_numverts * sizeof(msurface_t *));
	for (surf = surfaces, surfnum = 0;surfnum < numsurfaces;surf++, surfnum++)
	{
		for (vertnum = surf->poly_numverts - 1, vertnum2 = 0, v0 = surf->poly_verts + (surf->poly_numverts - 1) * 3, v1 = surf->poly_verts;vertnum2 < surf->poly_numverts;vertnum = vertnum2, vertnum2++, v0 = v1, v1 += 3)
		{
			if (surf->neighborsurfaces[vertnum])
				continue;
			surf->neighborsurfaces[vertnum] = NULL;
			for (s = surfaces, snum = 0;snum < numsurfaces;s++, snum++)
			{
				if (s->poly_mins[0] > (surf->poly_maxs[0] + 1) || s->poly_maxs[0] < (surf->poly_mins[0] - 1)
				 || s->poly_mins[1] > (surf->poly_maxs[1] + 1) || s->poly_maxs[1] < (surf->poly_mins[1] - 1)
				 || s->poly_mins[2] > (surf->poly_maxs[2] + 1) || s->poly_maxs[2] < (surf->poly_mins[2] - 1)
				 || s == surf)
					continue;
				for (vnum = 0;vnum < s->poly_numverts;vnum++)
					if (s->neighborsurfaces[vnum] == surf)
						break;
				if (vnum < s->poly_numverts)
					continue;
				for (vnum = s->poly_numverts - 1, vnum2 = 0, v2 = s->poly_verts + (s->poly_numverts - 1) * 3, v3 = s->poly_verts;vnum2 < s->poly_numverts;vnum = vnum2, vnum2++, v2 = v3, v3 += 3)
				{
					if (s->neighborsurfaces[vnum] == NULL
					 && ((v0[0] == v2[0] && v0[1] == v2[1] && v0[2] == v2[2] && v1[0] == v3[0] && v1[1] == v3[1] && v1[2] == v3[2])
					  || (v1[0] == v2[0] && v1[1] == v2[1] && v1[2] == v2[2] && v0[0] == v3[0] && v0[1] == v3[1] && v0[2] == v3[2])))
					{
						surf->neighborsurfaces[vertnum] = s;
						s->neighborsurfaces[vnum] = surf;
						break;
					}
				}
				if (vnum < s->poly_numverts)
					break;
			}
		}
	}
#endif
}

static void Mod_Q1BSP_BuildLightmapUpdateChains(mempool_t *mempool, model_t *model)
{
	int i, j, stylecounts[256], totalcount, remapstyles[256];
	msurface_t *surf;
	memset(stylecounts, 0, sizeof(stylecounts));
	for (i = 0;i < model->brushq1.nummodelsurfaces;i++)
	{
		surf = model->brushq1.surfaces + model->brushq1.firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			stylecounts[surf->styles[j]]++;
	}
	totalcount = 0;
	model->brushq1.light_styles = 0;
	for (i = 0;i < 255;i++)
	{
		if (stylecounts[i])
		{
			remapstyles[i] = model->brushq1.light_styles++;
			totalcount += stylecounts[i] + 1;
		}
	}
	if (!totalcount)
		return;
	model->brushq1.light_style = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(qbyte));
	model->brushq1.light_stylevalue = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(int));
	model->brushq1.light_styleupdatechains = Mem_Alloc(mempool, model->brushq1.light_styles * sizeof(msurface_t **));
	model->brushq1.light_styleupdatechainsbuffer = Mem_Alloc(mempool, totalcount * sizeof(msurface_t *));
	model->brushq1.light_styles = 0;
	for (i = 0;i < 255;i++)
		if (stylecounts[i])
			model->brushq1.light_style[model->brushq1.light_styles++] = i;
	j = 0;
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		model->brushq1.light_styleupdatechains[i] = model->brushq1.light_styleupdatechainsbuffer + j;
		j += stylecounts[model->brushq1.light_style[i]] + 1;
	}
	for (i = 0;i < model->brushq1.nummodelsurfaces;i++)
	{
		surf = model->brushq1.surfaces + model->brushq1.firstmodelsurface + i;
		for (j = 0;j < MAXLIGHTMAPS;j++)
			if (surf->styles[j] != 255)
				*model->brushq1.light_styleupdatechains[remapstyles[surf->styles[j]]]++ = surf;
	}
	j = 0;
	for (i = 0;i < model->brushq1.light_styles;i++)
	{
		*model->brushq1.light_styleupdatechains[i] = NULL;
		model->brushq1.light_styleupdatechains[i] = model->brushq1.light_styleupdatechainsbuffer + j;
		j += stylecounts[model->brushq1.light_style[i]] + 1;
	}
}

static void Mod_Q1BSP_BuildPVSTextureChains(model_t *model)
{
	int i, j;
	for (i = 0;i < model->brushq1.numtextures;i++)
		model->brushq1.pvstexturechainslength[i] = 0;
	for (i = 0, j = model->brushq1.firstmodelsurface;i < model->brushq1.nummodelsurfaces;i++, j++)
	{
		if (model->brushq1.surfacepvsframes[j] == model->brushq1.pvsframecount)
		{
			model->brushq1.pvssurflist[model->brushq1.pvssurflistlength++] = j;
			model->brushq1.pvstexturechainslength[model->brushq1.surfaces[j].texinfo->texture->number]++;
		}
	}
	for (i = 0, j = 0;i < model->brushq1.numtextures;i++)
	{
		if (model->brushq1.pvstexturechainslength[i])
		{
			model->brushq1.pvstexturechains[i] = model->brushq1.pvstexturechainsbuffer + j;
			j += model->brushq1.pvstexturechainslength[i] + 1;
		}
		else
			model->brushq1.pvstexturechains[i] = NULL;
	}
	for (i = 0, j = model->brushq1.firstmodelsurface;i < model->brushq1.nummodelsurfaces;i++, j++)
		if (model->brushq1.surfacepvsframes[j] == model->brushq1.pvsframecount)
			*model->brushq1.pvstexturechains[model->brushq1.surfaces[j].texinfo->texture->number]++ = model->brushq1.surfaces + j;
	for (i = 0;i < model->brushq1.numtextures;i++)
	{
		if (model->brushq1.pvstexturechainslength[i])
		{
			*model->brushq1.pvstexturechains[i] = NULL;
			model->brushq1.pvstexturechains[i] -= model->brushq1.pvstexturechainslength[i];
		}
	}
}

extern void R_Model_Brush_DrawSky(entity_render_t *ent);
extern void R_Model_Brush_Draw(entity_render_t *ent);
extern void R_Model_Brush_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Brush_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_Q1BSP_Load(model_t *mod, void *buffer)
{
	int i, j, k;
	dheader_t *header;
	dmodel_t *bm;
	mempool_t *mainmempool;
	char *loadname;
	model_t *originalloadmodel;
	float dist, modelyawradius, modelradius, *vec;
	msurface_t *surf;
	surfmesh_t *mesh;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != BSPVERSION && i != 30)
		Host_Error("Mod_Q1BSP_Load: %s has wrong version number(%i should be %i(Quake) or 30(HalfLife))", mod->name, i, BSPVERSION);
	mod->brushq1.ishlbsp = i == 30;

	mod->brush.FindNonSolidLocation = Mod_Q1BSP_FindNonSolidLocation;
	mod->brush.PointContents = Mod_Q1BSP_PointContents;
	mod->brushq1.PointInLeaf = Mod_Q1BSP_PointInLeaf;
	mod->brushq1.LeafPVS = Mod_Q1BSP_LeafPVS;
	mod->brushq1.BuildPVSTextureChains = Mod_Q1BSP_BuildPVSTextureChains;

	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brushq1.ishlbsp);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i = 0;i < (int) sizeof(dheader_t) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

// load into heap

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	Mod_Q1BSP_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_Q1BSP_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_Q1BSP_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_Q1BSP_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_Q1BSP_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_Q1BSP_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_Q1BSP_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_Q1BSP_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_Q1BSP_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_Q1BSP_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_Q1BSP_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_Q1BSP_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_Q1BSP_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_Q1BSP_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_Q1BSP_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	Mod_Q1BSP_MakeHull0();
	Mod_Q1BSP_MakePortals();

	mod->numframes = 2;		// regular and alternate animation

	mainmempool = mod->mempool;
	loadname = mod->name;

	Mod_Q1BSP_LoadLightList();
	originalloadmodel = loadmodel;

//
// set up the submodels(FIXME: this is confusing)
//
	for (i = 0;i < mod->brushq1.numsubmodels;i++)
	{
		bm = &mod->brushq1.submodels[i];

		mod->brushq1.hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->brushq1.hulls[j].firstclipnode = bm->headnode[j];
			mod->brushq1.hulls[j].lastclipnode = mod->brushq1.numclipnodes - 1;
		}

		mod->brushq1.firstmodelsurface = bm->firstface;
		mod->brushq1.nummodelsurfaces = bm->numfaces;

		// this gets altered below if sky is used
		mod->DrawSky = NULL;
		mod->Draw = R_Model_Brush_Draw;
		mod->DrawFakeShadow = NULL;
		mod->DrawShadowVolume = R_Model_Brush_DrawShadowVolume;
		mod->DrawLight = R_Model_Brush_DrawLight;
		mod->brushq1.pvstexturechains = Mem_Alloc(originalloadmodel->mempool, mod->brushq1.numtextures * sizeof(msurface_t **));
		mod->brushq1.pvstexturechainsbuffer = Mem_Alloc(originalloadmodel->mempool,(mod->brushq1.nummodelsurfaces + mod->brushq1.numtextures) * sizeof(msurface_t *));
		mod->brushq1.pvstexturechainslength = Mem_Alloc(originalloadmodel->mempool, mod->brushq1.numtextures * sizeof(int));
		Mod_Q1BSP_BuildPVSTextureChains(mod);
		Mod_Q1BSP_BuildLightmapUpdateChains(originalloadmodel->mempool, mod);
		if (mod->brushq1.nummodelsurfaces)
		{
			// LordHavoc: calculate bmodel bounding box rather than trusting what it says
			mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
			mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
			modelyawradius = 0;
			modelradius = 0;
			for (j = 0, surf = &mod->brushq1.surfaces[mod->brushq1.firstmodelsurface];j < mod->brushq1.nummodelsurfaces;j++, surf++)
			{
				// we only need to have a drawsky function if it is used(usually only on world model)
				if (surf->texinfo->texture->shader == &Cshader_sky)
					mod->DrawSky = R_Model_Brush_DrawSky;
				// LordHavoc: submodels always clip, even if water
				if (mod->brushq1.numsubmodels - 1)
					surf->flags |= SURF_SOLIDCLIP;
				// calculate bounding shapes
				for (mesh = surf->mesh;mesh;mesh = mesh->chain)
				{
					for (k = 0, vec = mesh->vertex3f;k < mesh->numverts;k++, vec += 3)
					{
						if (mod->normalmins[0] > vec[0]) mod->normalmins[0] = vec[0];
						if (mod->normalmins[1] > vec[1]) mod->normalmins[1] = vec[1];
						if (mod->normalmins[2] > vec[2]) mod->normalmins[2] = vec[2];
						if (mod->normalmaxs[0] < vec[0]) mod->normalmaxs[0] = vec[0];
						if (mod->normalmaxs[1] < vec[1]) mod->normalmaxs[1] = vec[1];
						if (mod->normalmaxs[2] < vec[2]) mod->normalmaxs[2] = vec[2];
						dist = vec[0]*vec[0]+vec[1]*vec[1];
						if (modelyawradius < dist)
							modelyawradius = dist;
						dist += vec[2]*vec[2];
						if (modelradius < dist)
							modelradius = dist;
					}
				}
			}
			modelyawradius = sqrt(modelyawradius);
			modelradius = sqrt(modelradius);
			mod->yawmins[0] = mod->yawmins[1] = - (mod->yawmaxs[0] = mod->yawmaxs[1] = modelyawradius);
			mod->yawmins[2] = mod->normalmins[2];
			mod->yawmaxs[2] = mod->normalmaxs[2];
			mod->rotatedmins[0] = mod->rotatedmins[1] = mod->rotatedmins[2] = -modelradius;
			mod->rotatedmaxs[0] = mod->rotatedmaxs[1] = mod->rotatedmaxs[2] = modelradius;
			mod->radius = modelradius;
			mod->radius2 = modelradius * modelradius;
		}
		else
		{
			// LordHavoc: empty submodel(lacrima.bsp has such a glitch)
			Con_Printf("warning: empty submodel *%i in %s\n", i+1, loadname);
		}
		Mod_Q1BSP_BuildSurfaceNeighbors(mod->brushq1.surfaces + mod->brushq1.firstmodelsurface, mod->brushq1.nummodelsurfaces, originalloadmodel->mempool);

		mod->brushq1.numleafs = bm->visleafs;

		// LordHavoc: only register submodels if it is the world
		// (prevents bsp models from replacing world submodels)
		if (loadmodel->isworldmodel && i < (mod->brushq1.numsubmodels - 1))
		{
			char	name[10];
			// duplicate the basic information
			sprintf(name, "*%i", i+1);
			loadmodel = Mod_FindName(name);
			*loadmodel = *mod;
			strcpy(loadmodel->name, name);
			// textures and memory belong to the main model
			loadmodel->texturepool = NULL;
			loadmodel->mempool = NULL;
			mod = loadmodel;
		}
	}

	loadmodel = originalloadmodel;
	//Mod_Q1BSP_ProcessLightList();
}

static void Mod_Q2BSP_LoadEntities(lump_t *l)
{
}

static void Mod_Q2BSP_LoadPlanes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadVertices(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadVertices: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadVisibility(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadVisibility: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadNodes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadTexInfo(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadTexInfo: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadFaces(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLighting(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLighting: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafs(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafFaces(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadLeafBrushes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadLeafBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadEdges(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadSurfEdges(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadSurfEdges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadBrushes(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadBrushSides(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadBrushSides: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadAreas(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadAreas: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadAreaPortals(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadAreaPortals: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

static void Mod_Q2BSP_LoadModels(lump_t *l)
{
/*
	d_t *in;
	m_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q2BSP_LoadModels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel-> = out;
	loadmodel->num = count;

	for (i = 0;i < count;i++, in++, out++)
	{
	}
*/
}

void Mod_Q2BSP_Load(model_t *mod, void *buffer)
{
	int i;
	q2dheader_t *header;

	Host_Error("Mod_Q2BSP_Load: not yet implemented\n");

	mod->type = mod_brushq2;

	header = (q2dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q2BSPVERSION)
		Host_Error("Mod_Q2BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q2BSPVERSION);
	mod->brushq1.ishlbsp = false;
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->brushq1.ishlbsp);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

	mod_base = (qbyte *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	// store which lightmap format to use
	mod->brushq1.lightmaprgba = r_lightmaprgba.integer;

	Mod_Q2BSP_LoadEntities(&header->lumps[Q2LUMP_ENTITIES]);
	Mod_Q2BSP_LoadPlanes(&header->lumps[Q2LUMP_PLANES]);
	Mod_Q2BSP_LoadVertices(&header->lumps[Q2LUMP_VERTEXES]);
	Mod_Q2BSP_LoadVisibility(&header->lumps[Q2LUMP_VISIBILITY]);
	Mod_Q2BSP_LoadNodes(&header->lumps[Q2LUMP_NODES]);
	Mod_Q2BSP_LoadTexInfo(&header->lumps[Q2LUMP_TEXINFO]);
	Mod_Q2BSP_LoadFaces(&header->lumps[Q2LUMP_FACES]);
	Mod_Q2BSP_LoadLighting(&header->lumps[Q2LUMP_LIGHTING]);
	Mod_Q2BSP_LoadLeafs(&header->lumps[Q2LUMP_LEAFS]);
	Mod_Q2BSP_LoadLeafFaces(&header->lumps[Q2LUMP_LEAFFACES]);
	Mod_Q2BSP_LoadLeafBrushes(&header->lumps[Q2LUMP_LEAFBRUSHES]);
	Mod_Q2BSP_LoadEdges(&header->lumps[Q2LUMP_EDGES]);
	Mod_Q2BSP_LoadSurfEdges(&header->lumps[Q2LUMP_SURFEDGES]);
	Mod_Q2BSP_LoadBrushes(&header->lumps[Q2LUMP_BRUSHES]);
	Mod_Q2BSP_LoadBrushSides(&header->lumps[Q2LUMP_BRUSHSIDES]);
	Mod_Q2BSP_LoadAreas(&header->lumps[Q2LUMP_AREAS]);
	Mod_Q2BSP_LoadAreaPortals(&header->lumps[Q2LUMP_AREAPORTALS]);
	// LordHavoc: must go last because this makes the submodels
	Mod_Q2BSP_LoadModels(&header->lumps[Q2LUMP_MODELS]);
}


static void Mod_Q3BSP_LoadEntities(lump_t *l)
{
	const char *data;
	char key[128], value[4096];
	float v[3];
	loadmodel->brushq3.num_lightgrid_cellsize[0] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[1] = 64;
	loadmodel->brushq3.num_lightgrid_cellsize[2] = 128;
	if (!l->filelen)
		return;
	loadmodel->brush.entities = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy(loadmodel->brush.entities, mod_base + l->fileofs, l->filelen);
	data = loadmodel->brush.entities;
	// some Q3 maps override the lightgrid_cellsize with a worldspawn key
	if (data && COM_ParseToken(&data) && com_token[0] == '{')
	{
		while (1)
		{
			if (!COM_ParseToken(&data))
				break; // error
			if (com_token[0] == '}')
				break; // end of worldspawn
			if (com_token[0] == '_')
				strcpy(key, com_token + 1);
			else
				strcpy(key, com_token);
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			if (!COM_ParseToken(&data))
				break; // error
			strcpy(value, com_token);
			if (!strcmp("gridsize", key))
			{
				if (sscanf(value, "%f %f %f", &v[0], &v[1], &v[2]) == 3 && v[0] != 0 && v[1] != 0 && v[2] != 0)
					VectorCopy(v, loadmodel->brushq3.num_lightgrid_cellsize);
			}
		}
	}
}

static void Mod_Q3BSP_LoadTextures(lump_t *l)
{
	q3dtexture_t *in;
	q3mtexture_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadTextures: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_textures = out;
	loadmodel->brushq3.num_textures = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		strncpy(out->name, in->name, sizeof(out->name) - 1);
		out->surfaceflags = LittleLong(in->surfaceflags);
		out->contents = LittleLong(in->contents);

		out->number = i;
		Mod_LoadSkinFrame(&out->skin, out->name, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE, false, true, true);
	}
}

static void Mod_Q3BSP_LoadPlanes(lump_t *l)
{
	q3dplane_t *in;
	mplane_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_planes = out;
	loadmodel->brushq3.num_planes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->normal[0] = LittleLong(in->normal[0]);
		out->normal[1] = LittleLong(in->normal[1]);
		out->normal[2] = LittleLong(in->normal[2]);
		out->dist = LittleLong(in->dist);
		PlaneClassify(out);
	}
}

static void Mod_Q3BSP_LoadBrushSides(lump_t *l)
{
	q3dbrushside_t *in;
	q3mbrushside_t *out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushSides: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_brushsides = out;
	loadmodel->brushq3.num_brushsides = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brushq3.num_planes)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid planeindex %i (%i planes)\n", n, loadmodel->brushq3.num_planes);
		out->plane = loadmodel->brushq3.data_planes + n;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushSides: invalid textureindex %i (%i textures)\n", n, loadmodel->brushq3.num_textures);
		out->texture = loadmodel->brushq3.data_textures + n;
	}
}

static void Mod_Q3BSP_LoadBrushes(lump_t *l)
{
	q3dbrush_t *in;
	q3mbrush_t *out;
	int i, n, c, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_brushes = out;
	loadmodel->brushq3.num_brushes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(in->firstbrushside);
		c = LittleLong(in->numbrushsides);
		if (n < 0 || n + c > loadmodel->brushq3.num_brushsides)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid brushside range %i : %i (%i brushsides)\n", n, n + c, loadmodel->brushq3.num_brushsides);
		out->firstbrushside = loadmodel->brushq3.data_brushsides + n;
		out->numbrushsides = c;
		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
			Host_Error("Mod_Q3BSP_LoadBrushes: invalid textureindex %i (%i textures)\n", n, loadmodel->brushq3.num_textures);
		out->texture = loadmodel->brushq3.data_textures + n;
	}
}

static void Mod_Q3BSP_LoadEffects(lump_t *l)
{
	q3deffect_t *in;
	q3meffect_t *out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadEffects: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_effects = out;
	loadmodel->brushq3.num_effects = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		strncpy(out->shadername, in->shadername, sizeof(out->shadername) - 1);
		n = LittleLong(in->brushindex);
		if (n < 0 || n >= loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadEffects: invalid brushindex %i (%i brushes)\n", n, loadmodel->brushq3.num_brushes);
		out->brush = loadmodel->brushq3.data_brushes + n;
		out->unknown = LittleLong(in->unknown);
	}
}

static void Mod_Q3BSP_LoadVertices(lump_t *l)
{
	q3dvertex_t *in;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadVertices: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_vertices = count = l->filelen / sizeof(*in);
	loadmodel->brushq3.data_vertex3f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[3]));
	loadmodel->brushq3.data_texturetexcoord2f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[2]));
	loadmodel->brushq3.data_lightmaptexcoord2f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[2]));
	loadmodel->brushq3.data_svector3f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[3]));
	loadmodel->brushq3.data_tvector3f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[3]));
	loadmodel->brushq3.data_normal3f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[3]));
	loadmodel->brushq3.data_color4f = Mem_Alloc(loadmodel->mempool, count * sizeof(float[4]));

	for (i = 0;i < count;i++, in++)
	{
		loadmodel->brushq3.data_vertex3f[i * 3 + 0] = LittleFloat(in->origin3f[0]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 1] = LittleFloat(in->origin3f[1]);
		loadmodel->brushq3.data_vertex3f[i * 3 + 2] = LittleFloat(in->origin3f[2]);
		loadmodel->brushq3.data_texturetexcoord2f[i * 2 + 0] = LittleFloat(in->texcoord2f[0]);
		loadmodel->brushq3.data_texturetexcoord2f[i * 2 + 1] = LittleFloat(in->texcoord2f[1]);
		loadmodel->brushq3.data_lightmaptexcoord2f[i * 2 + 0] = LittleFloat(in->lightmap2f[0]);
		loadmodel->brushq3.data_lightmaptexcoord2f[i * 2 + 1] = LittleFloat(in->lightmap2f[1]);
		// svector/tvector are calculated later in face loading
		loadmodel->brushq3.data_svector3f[i * 3 + 0] = 0;
		loadmodel->brushq3.data_svector3f[i * 3 + 1] = 0;
		loadmodel->brushq3.data_svector3f[i * 3 + 2] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 0] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 1] = 0;
		loadmodel->brushq3.data_tvector3f[i * 3 + 2] = 0;
		loadmodel->brushq3.data_normal3f[i * 3 + 0] = LittleFloat(in->normal3f[0]);
		loadmodel->brushq3.data_normal3f[i * 3 + 1] = LittleFloat(in->normal3f[1]);
		loadmodel->brushq3.data_normal3f[i * 3 + 2] = LittleFloat(in->normal3f[2]);
		loadmodel->brushq3.data_color4f[i * 4 + 0] = in->color4ub[0] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 1] = in->color4ub[1] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 2] = in->color4ub[2] * (1.0f / 255.0f);
		loadmodel->brushq3.data_color4f[i * 4 + 3] = in->color4ub[3] * (1.0f / 255.0f);
	}
}

static void Mod_Q3BSP_LoadTriangles(lump_t *l)
{
	int *in;
	int *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(int[3]))
		Host_Error("Mod_Q3BSP_LoadTriangles: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.num_triangles = count / 3;
	loadmodel->brushq3.data_element3i = out;
	loadmodel->brushq3.data_neighbor3i = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	for (i = 0;i < count;i++, in++, out++)
	{
		*out = LittleLong(*in);
		if (*out < 0 || *out >= loadmodel->brushq3.num_vertices)
			Host_Error("Mod_Q3BSP_LoadTriangles: invalid vertexindex %i (%i vertices)\n", *out, loadmodel->brushq3.num_vertices);
	}
}

static void Mod_Q3BSP_LoadLightmaps(lump_t *l)
{
	q3dlightmap_t *in;
	rtexture_t **out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightmaps: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_lightmaps = out;
	loadmodel->brushq3.num_lightmaps = count;

	for (i = 0;i < count;i++, in++, out++)
		*out = R_LoadTexture2D(loadmodel->texturepool, va("lightmap%04i", i), 128, 128, in->rgb, TEXTYPE_RGB, TEXF_FORCELINEAR | TEXF_PRECACHE, NULL);
}

static void Mod_Q3BSP_LoadFaces(lump_t *l)
{
	q3dface_t *in;
	q3mface_t *out;
	int i, j, n, count, invalidelements, patchsize[2];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_faces = out;
	loadmodel->brushq3.num_faces = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		// check face type first
		out->type = LittleLong(in->type);
		if (out->type != Q3FACETYPE_POLYGON
		 && out->type != Q3FACETYPE_PATCH
		 && out->type != Q3FACETYPE_MESH
		 && out->type != Q3FACETYPE_FLARE)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i: unknown face type %i\n", i, out->type);
			out->type = 0; // error
			continue;
		}

		n = LittleLong(in->textureindex);
		if (n < 0 || n >= loadmodel->brushq3.num_textures)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i: invalid textureindex %i (%i textures)\n", i, n, loadmodel->brushq3.num_textures);
			out->type = 0; // error
			continue;
			n = 0;
		}
		out->texture = loadmodel->brushq3.data_textures + n;
		n = LittleLong(in->effectindex);
		if (n < -1 || n >= loadmodel->brushq3.num_effects)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid effectindex %i (%i effects)\n", i, out->texture->name, n, loadmodel->brushq3.num_effects);
			n = -1;
		}
		if (n == -1)
			out->effect = NULL;
		else
			out->effect = loadmodel->brushq3.data_effects + n;
		n = LittleLong(in->lightmapindex);
		if (n < -1 || n >= loadmodel->brushq3.num_lightmaps)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid lightmapindex %i (%i lightmaps)\n", i, out->texture->name, n, loadmodel->brushq3.num_lightmaps);
			n = -1;
		}
		if (n == -1)
			out->lightmaptexture = NULL;
		else
			out->lightmaptexture = loadmodel->brushq3.data_lightmaps[n];

		out->firstvertex = LittleLong(in->firstvertex);
		out->numvertices = LittleLong(in->numvertices);
		out->firstelement = LittleLong(in->firstelement);
		out->numelements = LittleLong(in->numelements);
		out->numtriangles = out->numelements / 3;
		if (out->firstvertex < 0 || out->firstvertex + out->numvertices > loadmodel->brushq3.num_vertices)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid vertex range %i : %i (%i vertices)\n", i, out->texture->name, out->firstvertex, out->firstvertex + out->numvertices, loadmodel->brushq3.num_vertices);
			out->type = 0; // error
			continue;
		}
		if (out->firstelement < 0 || out->firstelement + out->numelements > loadmodel->brushq3.num_triangles * 3)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid element range %i : %i (%i elements)\n", i, out->texture->name, out->firstelement, out->firstelement + out->numelements, loadmodel->brushq3.num_triangles * 3);
			out->type = 0; // error
			continue;
		}
		if (out->numtriangles * 3 != out->numelements)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): numelements %i is not a multiple of 3\n", i, out->texture->name, out->numelements);
			out->type = 0; // error
			continue;
		}
		switch(out->type)
		{
		case Q3FACETYPE_POLYGON:
		case Q3FACETYPE_MESH:
			out->data_vertex3f = loadmodel->brushq3.data_vertex3f + out->firstvertex * 3;
			out->data_texturetexcoord2f = loadmodel->brushq3.data_texturetexcoord2f + out->firstvertex * 2;
			out->data_lightmaptexcoord2f = loadmodel->brushq3.data_lightmaptexcoord2f + out->firstvertex * 2;
			out->data_svector3f = loadmodel->brushq3.data_svector3f + out->firstvertex * 3;
			out->data_tvector3f = loadmodel->brushq3.data_tvector3f + out->firstvertex * 3;
			out->data_normal3f = loadmodel->brushq3.data_normal3f + out->firstvertex * 3;
			out->data_color4f = loadmodel->brushq3.data_color4f + out->firstvertex * 4;
			out->data_element3i = loadmodel->brushq3.data_element3i + out->firstelement;
			out->data_neighbor3i = loadmodel->brushq3.data_neighbor3i + out->firstelement;
			break;
		case Q3FACETYPE_PATCH:
			patchsize[0] = LittleLong(in->specific.patch.patchsize[0]);
			patchsize[1] = LittleLong(in->specific.patch.patchsize[1]);
			if (patchsize[0] < 1 || patchsize[1] < 1)
			{
				Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): invalid patchsize %ix%i\n", i, out->texture->name, patchsize[0], patchsize[1]);
				out->type = 0; // error
				continue;
			}
			// FIXME: convert patch to triangles here!
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): Q3FACETYPE_PATCH not supported (yet)\n", i, out->texture->name);
			out->type = 0;
			continue;
			break;
		case Q3FACETYPE_FLARE:
			Con_Printf("Mod_Q3BSP_LoadFaces: face #%i (texture \"%s\"): Q3FACETYPE_FLARE not supported (yet)\n", i, out->texture->name);
			out->type = 0;
			continue;
			break;
		}
		for (j = 0, invalidelements = 0;j < out->numelements;j++)
			if (out->data_element3i[j] < 0 || out->data_element3i[j] >= out->numvertices)
				invalidelements++;
		if (invalidelements)
		{
			Con_Printf("Mod_Q3BSP_LoadFaces: Warning: face #%i has %i invalid elements, type = %i, texture->name = \"%s\", texture->surfaceflags = %i, texture->contents = %i, firstvertex = %i, numvertices = %i, firstelement = %i, numelements = %i, elements list:\n", i, invalidelements, out->type, out->texture->name, out->texture->surfaceflags, out->texture->contents, out->firstvertex, out->numvertices, out->firstelement, out->numelements);
			for (j = 0;j < out->numelements;j++)
			{
				Con_Printf(" %i", out->data_element3i[j]);
				if (out->data_element3i[j] < 0 || out->data_element3i[j] >= out->numvertices)
					out->data_element3i[j] = 0;
			}
			Con_Printf("\n");
		}
	}
}

static void Mod_Q3BSP_LoadModels(lump_t *l)
{
	q3dmodel_t *in;
	q3mmodel_t *out;
	int i, j, n, c, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadModels: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_models = out;
	loadmodel->brushq3.num_models = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		for (j = 0;j < 3;j++)
		{
			out->mins[j] = LittleFloat(in->mins[j]);
			out->maxs[j] = LittleFloat(in->maxs[j]);
		}
		n = LittleLong(in->firstface);
		c = LittleLong(in->numfaces);
		if (n < 0 || n + c > loadmodel->brushq3.num_faces)
			Host_Error("Mod_Q3BSP_LoadModels: invalid face range %i : %i (%i faces)\n", n, n + c, loadmodel->brushq3.num_faces);
		out->firstface = loadmodel->brushq3.data_faces + n;
		out->numfaces = c;
		n = LittleLong(in->firstbrush);
		c = LittleLong(in->numbrushes);
		if (n < 0 || n + c > loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadModels: invalid brush range %i : %i (%i brushes)\n", n, n + c, loadmodel->brushq3.num_brushes);
		out->firstbrush = loadmodel->brushq3.data_brushes + n;
		out->numbrushes = c;
	}
}

static void Mod_Q3BSP_LoadLeafBrushes(lump_t *l)
{
	int *in;
	q3mbrush_t **out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafBrushes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leafbrushes = out;
	loadmodel->brushq3.num_leafbrushes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->brushq3.num_brushes)
			Host_Error("Mod_Q3BSP_LoadLeafBrushes: invalid brush index %i (%i brushes)\n", n, loadmodel->brushq3.num_brushes);
		*out = loadmodel->brushq3.data_brushes + n;
	}
}

static void Mod_Q3BSP_LoadLeafFaces(lump_t *l)
{
	int *in;
	q3mface_t **out;
	int i, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafFaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leaffaces = out;
	loadmodel->brushq3.num_leaffaces = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		n = LittleLong(*in);
		if (n < 0 || n >= loadmodel->brushq3.num_faces)
			Host_Error("Mod_Q3BSP_LoadLeafFaces: invalid face index %i (%i faces)\n", n, loadmodel->brushq3.num_faces);
		*out = loadmodel->brushq3.data_faces + n;
	}
}

static void Mod_Q3BSP_LoadLeafs(lump_t *l)
{
	q3dleaf_t *in;
	q3mleaf_t *out;
	int i, j, n, c, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLeafs: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_leafs = out;
	loadmodel->brushq3.num_leafs = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->isnode = false;
		out->parent = NULL;
		out->clusterindex = LittleLong(in->clusterindex);
		out->areaindex = LittleLong(in->areaindex);
		for (j = 0;j < 3;j++)
		{
			// yes the mins/maxs are ints
			out->mins[j] = LittleLong(in->mins[j]);
			out->maxs[j] = LittleLong(in->maxs[j]);
		}
		n = LittleLong(in->firstleafface);
		c = LittleLong(in->numleaffaces);
		if (n < 0 || n + c > loadmodel->brushq3.num_leaffaces)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafface range %i : %i (%i leaffaces)\n", n, n + c, loadmodel->brushq3.num_leaffaces);
		out->firstleafface = loadmodel->brushq3.data_leaffaces + n;
		out->numleaffaces = c;
		n = LittleLong(in->firstleafbrush);
		c = LittleLong(in->numleafbrushes);
		if (n < 0 || n + c > loadmodel->brushq3.num_leafbrushes)
			Host_Error("Mod_Q3BSP_LoadLeafs: invalid leafbrush range %i : %i (%i leafbrushes)\n", n, n + c, loadmodel->brushq3.num_leafbrushes);
		out->firstleafbrush = loadmodel->brushq3.data_leafbrushes + n;
		out->numleafbrushes = c;
	}
}

static void Mod_Q3BSP_LoadNodes_RecursiveSetParent(q3mnode_t *node, q3mnode_t *parent)
{
	if (node->parent)
		Host_Error("Mod_Q3BSP_LoadNodes_RecursiveSetParent: runaway recursion\n");
	node->parent = parent;
	if (node->isnode)
	{
		Mod_Q3BSP_LoadNodes_RecursiveSetParent(node->children[0], node);
		Mod_Q3BSP_LoadNodes_RecursiveSetParent(node->children[1], node);
	}
}

static void Mod_Q3BSP_LoadNodes(lump_t *l)
{
	q3dnode_t *in;
	q3mnode_t *out;
	int i, j, n, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadNodes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->brushq3.data_nodes = out;
	loadmodel->brushq3.num_nodes = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		out->isnode = true;
		out->parent = NULL;
		n = LittleLong(in->planeindex);
		if (n < 0 || n >= loadmodel->brushq3.num_planes)
			Host_Error("Mod_Q3BSP_LoadNodes: invalid planeindex %i (%i planes)\n", n, loadmodel->brushq3.num_planes);
		out->plane = loadmodel->brushq3.data_planes + n;
		for (j = 0;j < 2;j++)
		{
			n = LittleLong(in->childrenindex[j]);
			if (n >= 0)
			{
				if (n >= loadmodel->brushq3.num_nodes)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child node index %i (%i nodes)\n", n, loadmodel->brushq3.num_nodes);
				out->children[j] = loadmodel->brushq3.data_nodes + n;
			}
			else
			{
				n = 1 - n;
				if (n >= loadmodel->brushq3.num_leafs)
					Host_Error("Mod_Q3BSP_LoadNodes: invalid child leaf index %i (%i leafs)\n", n, loadmodel->brushq3.num_leafs);
				out->children[j] = (q3mnode_t *)(loadmodel->brushq3.data_leafs + n);
			}
		}
		// we don't load the mins/maxs
	}

	// set the parent pointers
	Mod_Q3BSP_LoadNodes_RecursiveSetParent(loadmodel->brushq3.data_nodes, NULL);
}

static void Mod_Q3BSP_LoadLightGrid(lump_t *l)
{
	q3dlightgrid_t *in;
	q3dlightgrid_t *out;
	int count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightGrid: funny lump size in %s",loadmodel->name);
	loadmodel->brushq3.num_lightgrid_scale[0] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[0];
	loadmodel->brushq3.num_lightgrid_scale[1] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[1];
	loadmodel->brushq3.num_lightgrid_scale[2] = 1.0f / loadmodel->brushq3.num_lightgrid_cellsize[2];
	loadmodel->brushq3.num_lightgrid_imins[0] = ceil(loadmodel->brushq3.data_models->mins[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imins[1] = ceil(loadmodel->brushq3.data_models->mins[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imins[2] = ceil(loadmodel->brushq3.data_models->mins[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_imaxs[0] = floor(loadmodel->brushq3.data_models->maxs[0] * loadmodel->brushq3.num_lightgrid_scale[0]);
	loadmodel->brushq3.num_lightgrid_imaxs[1] = floor(loadmodel->brushq3.data_models->maxs[1] * loadmodel->brushq3.num_lightgrid_scale[1]);
	loadmodel->brushq3.num_lightgrid_imaxs[2] = floor(loadmodel->brushq3.data_models->maxs[2] * loadmodel->brushq3.num_lightgrid_scale[2]);
	loadmodel->brushq3.num_lightgrid_isize[0] = loadmodel->brushq3.num_lightgrid_imaxs[0] - loadmodel->brushq3.num_lightgrid_imins[0] + 1;
	loadmodel->brushq3.num_lightgrid_isize[1] = loadmodel->brushq3.num_lightgrid_imaxs[1] - loadmodel->brushq3.num_lightgrid_imins[1] + 1;
	loadmodel->brushq3.num_lightgrid_isize[2] = loadmodel->brushq3.num_lightgrid_imaxs[2] - loadmodel->brushq3.num_lightgrid_imins[2] + 1;
	count = loadmodel->brushq3.num_lightgrid_isize[0] * loadmodel->brushq3.num_lightgrid_isize[1] * loadmodel->brushq3.num_lightgrid_isize[2];
	if (l->filelen < count * (int)sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadLightGrid: invalid lightgrid lump size %i bytes, should be %i bytes (%ix%ix%i)\n", l->filelen, count * sizeof(*in), loadmodel->brushq3.num_lightgrid_dimensions[0], loadmodel->brushq3.num_lightgrid_dimensions[1], loadmodel->brushq3.num_lightgrid_dimensions[2]);
	if (l->filelen != count * (int)sizeof(*in))
		Con_Printf("Mod_Q3BSP_LoadLightGrid: Warning: calculated lightgrid size %i bytes does not match lump size %i\n", count * sizeof(*in), l->filelen);

	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));
	loadmodel->brushq3.data_lightgrid = out;
	loadmodel->brushq3.num_lightgrid = count;

	// no swapping or validation necessary
	memcpy(out, in, count * (int)sizeof(*out));

	Matrix4x4_CreateScale3(&loadmodel->brushq3.num_lightgrid_indexfromworld, loadmodel->brushq3.num_lightgrid_scale[0], loadmodel->brushq3.num_lightgrid_scale[1], loadmodel->brushq3.num_lightgrid_scale[2]);
	Matrix4x4_ConcatTranslate(&loadmodel->brushq3.num_lightgrid_indexfromworld, -loadmodel->brushq3.num_lightgrid_imins[0] * loadmodel->brushq3.num_lightgrid_cellsize[0], -loadmodel->brushq3.num_lightgrid_imins[1] * loadmodel->brushq3.num_lightgrid_cellsize[1], -loadmodel->brushq3.num_lightgrid_imins[2] * loadmodel->brushq3.num_lightgrid_cellsize[2]);
}

static void Mod_Q3BSP_LoadPVS(lump_t *l)
{
	q3dpvs_t *in;
	int totalchains;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen < 9)
		Host_Error("Mod_Q3BSP_LoadPVS: funny lump size in %s",loadmodel->name);

	loadmodel->brushq3.num_pvsclusters = LittleLong(in->numclusters);
	loadmodel->brushq3.num_pvschainlength = LittleLong(in->chainlength);
	if (loadmodel->brushq3.num_pvschainlength < ((loadmodel->brushq3.num_pvsclusters + 7) / 8))
		Host_Error("Mod_Q3BSP_LoadPVS: (chainlength = %i) < ((numclusters = %i) + 7) / 8\n", loadmodel->brushq3.num_pvschainlength, loadmodel->brushq3.num_pvsclusters);
	totalchains = loadmodel->brushq3.num_pvschainlength * loadmodel->brushq3.num_pvsclusters;
	if (l->filelen < totalchains + (int)sizeof(*in))
		Host_Error("Mod_Q3BSP_LoadPVS: lump too small ((numclusters = %i) * (chainlength = %i) + sizeof(q3dpvs_t) == %i bytes, lump is %i bytes)\n", loadmodel->brushq3.num_pvsclusters, loadmodel->brushq3.num_pvschainlength, totalchains + sizeof(*in), l->filelen);

	loadmodel->brushq3.data_pvschains = Mem_Alloc(loadmodel->mempool, totalchains);
	memcpy(loadmodel->brushq3.data_pvschains, (qbyte *)(in + 1), totalchains);
}

void Mod_Q3BSP_Load(model_t *mod, void *buffer)
{
	int i;
	q3dheader_t *header;

	mod->type = mod_brushq2;

	header = (q3dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != Q3BSPVERSION)
		Host_Error("Mod_Q3BSP_Load: %s has wrong version number (%i, should be %i)", mod->name, i, Q3BSPVERSION);
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", false);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

	mod_base = (qbyte *)header;

	// swap all the lumps
	for (i = 0;i < (int) sizeof(*header) / 4;i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	Mod_Q3BSP_LoadEntities(&header->lumps[Q3LUMP_ENTITIES]);
	Mod_Q3BSP_LoadTextures(&header->lumps[Q3LUMP_TEXTURES]);
	Mod_Q3BSP_LoadPlanes(&header->lumps[Q3LUMP_PLANES]);
	Mod_Q3BSP_LoadBrushSides(&header->lumps[Q3LUMP_BRUSHSIDES]);
	Mod_Q3BSP_LoadBrushes(&header->lumps[Q3LUMP_BRUSHES]);
	Mod_Q3BSP_LoadEffects(&header->lumps[Q3LUMP_EFFECTS]);
	Mod_Q3BSP_LoadVertices(&header->lumps[Q3LUMP_VERTICES]);
	Mod_Q3BSP_LoadTriangles(&header->lumps[Q3LUMP_TRIANGLES]);
	Mod_Q3BSP_LoadLightmaps(&header->lumps[Q3LUMP_LIGHTMAPS]);
	Mod_Q3BSP_LoadFaces(&header->lumps[Q3LUMP_FACES]);
	Mod_Q3BSP_LoadModels(&header->lumps[Q3LUMP_MODELS]);
	Mod_Q3BSP_LoadLeafBrushes(&header->lumps[Q3LUMP_LEAFBRUSHES]);
	Mod_Q3BSP_LoadLeafFaces(&header->lumps[Q3LUMP_LEAFFACES]);
	Mod_Q3BSP_LoadLeafs(&header->lumps[Q3LUMP_LEAFS]);
	Mod_Q3BSP_LoadNodes(&header->lumps[Q3LUMP_NODES]);
	Mod_Q3BSP_LoadLightGrid(&header->lumps[Q3LUMP_LIGHTGRID]);
	Mod_Q3BSP_LoadPVS(&header->lumps[Q3LUMP_PVS]);
}

void Mod_IBSP_Load(model_t *mod, void *buffer)
{
	int i = LittleLong(((int *)buffer)[1]);
	if (i == Q3BSPVERSION)
		Mod_Q3BSP_Load(mod,buffer);
	else if (i == Q2BSPVERSION)
		Mod_Q2BSP_Load(mod,buffer);
	else
		Host_Error("Mod_IBSP_Load: unknown/unsupported version %i\n", i);
}

void Mod_MAP_Load(model_t *mod, void *buffer)
{
	Host_Error("Mod_MAP_Load: not yet implemented\n");
}

