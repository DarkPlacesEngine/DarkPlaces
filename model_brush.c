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

// note: model_shared.c sets up r_notexture, and r_surf_notexture

qbyte mod_novis[(MAX_MAP_LEAFS + 7)/ 8];

//cvar_t r_subdivide_size = {CVAR_SAVE, "r_subdivide_size", "128"};
cvar_t halflifebsp = {0, "halflifebsp", "0"};
cvar_t r_novis = {0, "r_novis", "0"};
cvar_t r_miplightmaps = {CVAR_SAVE, "r_miplightmaps", "0"};
cvar_t r_lightmaprgba = {0, "r_lightmaprgba", "1"};
cvar_t r_nosurftextures = {0, "r_nosurftextures", "0"};
cvar_t r_sortsurfaces = {0, "r_sortsurfaces", "0"};

#define NUM_DETAILTEXTURES 1
static rtexture_t *detailtextures[NUM_DETAILTEXTURES];
static rtexturepool_t *detailtexturepool;

/*
===============
Mod_BrushInit
===============
*/
void Mod_BrushInit (void)
{
//	Cvar_RegisterVariable(&r_subdivide_size);
	Cvar_RegisterVariable(&halflifebsp);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_miplightmaps);
	Cvar_RegisterVariable(&r_lightmaprgba);
	Cvar_RegisterVariable(&r_nosurftextures);
	Cvar_RegisterVariable(&r_sortsurfaces);
	memset(mod_novis, 0xff, sizeof(mod_novis));
}

void Mod_BrushStartup (void)
{
	int i, x, y, light;
	float vc[3], vx[3], vy[3], vn[3], lightdir[3];
#define DETAILRESOLUTION 256
	qbyte data[DETAILRESOLUTION][DETAILRESOLUTION][4], noise[DETAILRESOLUTION][DETAILRESOLUTION];
	detailtexturepool = R_AllocTexturePool();
	lightdir[0] = 0.5;
	lightdir[1] = 1;
	lightdir[2] = -0.25;
	VectorNormalize(lightdir);
	for (i = 0;i < NUM_DETAILTEXTURES;i++)
	{
		fractalnoise(&noise[0][0], DETAILRESOLUTION, DETAILRESOLUTION >> 4);
		for (y = 0;y < DETAILRESOLUTION;y++)
		{
			for (x = 0;x < DETAILRESOLUTION;x++)
			{
				vc[0] = x;
				vc[1] = y;
				vc[2] = noise[y][x] * (1.0f / 32.0f);
				vx[0] = x + 1;
				vx[1] = y;
				vx[2] = noise[y][(x + 1) % DETAILRESOLUTION] * (1.0f / 32.0f);
				vy[0] = x;
				vy[1] = y + 1;
				vy[2] = noise[(y + 1) % DETAILRESOLUTION][x] * (1.0f / 32.0f);
				VectorSubtract(vx, vc, vx);
				VectorSubtract(vy, vc, vy);
				CrossProduct(vx, vy, vn);
				VectorNormalize(vn);
				light = 128 - DotProduct(vn, lightdir) * 128;
				light = bound(0, light, 255);
				data[y][x][0] = data[y][x][1] = data[y][x][2] = light;
				data[y][x][3] = 255;
			}
		}
		detailtextures[i] = R_LoadTexture(detailtexturepool, va("detailtexture%i", i), DETAILRESOLUTION, DETAILRESOLUTION, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_PRECACHE);
	}
}

void Mod_BrushShutdown (void)
{
	int i;
	for (i = 0;i < NUM_DETAILTEXTURES;i++)
		R_FreeTexture(detailtextures[i]);
	R_FreeTexturePool(&detailtexturepool);
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (const vec3_t p, model_t *model)
{
	mnode_t *node;

	if (model == NULL)
		return NULL;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->nodes + model->hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];

	return (mleaf_t *)node;
}

int Mod_PointContents (const vec3_t p, model_t *model)
{
	mnode_t *node;

	if (model == NULL)
		return CONTENTS_EMPTY;

	Mod_CheckLoaded(model);

	// LordHavoc: modified to start at first clip node,
	// in other words: first node of the (sub)model
	node = model->nodes + model->hulls[0].firstclipnode;
	while (node->contents == 0)
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];

	return ((mleaf_t *)node)->contents;
}

void Mod_FindNonSolidLocation(vec3_t pos, model_t *mod)
{
	if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[0]-=1;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[0]+=2;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[0]-=1;
	pos[1]-=1;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[1]+=2;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[1]-=1;
	pos[2]-=1;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[2]+=2;if (Mod_PointContents(pos, mod) != CONTENTS_SOLID) return;
	pos[2]-=1;
}


/*
===================
Mod_DecompressVis
===================
*/
static qbyte *Mod_DecompressVis (qbyte *in, model_t *model)
{
	static qbyte decompressed[MAX_MAP_LEAFS/8];
	int c;
	qbyte *out;
	int row;

	row = (model->numleafs+7)>>3;
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

qbyte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (r_novis.integer || leaf == model->leafs || leaf->compressed_vis == NULL)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
=================
Mod_LoadTextures
=================
*/
static void Mod_LoadTextures (lump_t *l)
{
	int i, j, k, num, max, altmax, mtwidth, mtheight, *dofs, incomplete;
	miptex_t *dmiptex;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	qbyte *data, *mtdata, *data2;
	char name[256];

	loadmodel->textures = NULL;

	if (!l->filelen)
		return;

	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	// add two slots for notexture walls and notexture liquids
	loadmodel->numtextures = m->nummiptex + 2;
	loadmodel->textures = Mem_Alloc(loadmodel->mempool, loadmodel->numtextures * sizeof(texture_t));

	// fill out all slots with notexture
	for (i = 0, tx = loadmodel->textures;i < loadmodel->numtextures;i++, tx++)
	{
		tx->width = 16;
		tx->height = 16;
		tx->texture = r_notexture;
		tx->shader = &Cshader_wall_lightmap;
		if (i == loadmodel->numtextures - 1)
		{
			tx->flags = SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			tx->shader = &Cshader_water;
		}
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

		mtwidth = LittleLong (dmiptex->width);
		mtheight = LittleLong (dmiptex->height);
		mtdata = NULL;
		j = LittleLong (dmiptex->offsets[0]);
		if (j)
		{
			// texture included
			if (j < 40 || j + mtwidth * mtheight > l->filelen)
			{
				Con_Printf ("Texture \"%s\" in \"%s\"is corrupt or incomplete\n", dmiptex->name, loadmodel->name);
				continue;
			}
			mtdata = (qbyte *)dmiptex + j;
		}

		if ((mtwidth & 15) || (mtheight & 15))
			Con_Printf ("warning: texture \"%s\" in \"%s\" is not 16 aligned", dmiptex->name, loadmodel->name);

		// LordHavoc: force all names to lowercase
		for (j = 0;name[j];j++)
			if (name[j] >= 'A' && name[j] <= 'Z')
				name[j] += 'a' - 'A';

		tx = loadmodel->textures + i;
		strcpy(tx->name, name);
		tx->width = mtwidth;
		tx->height = mtheight;

		if (!tx->name[0])
		{
			sprintf(tx->name, "unnamed%i", i);
			Con_Printf("warning: unnamed texture in %s, renaming to %s\n", loadmodel->name, tx->name);
		}

		// LordHavoc: HL sky textures are entirely different than quake
		if (!loadmodel->ishlbsp && !strncmp(tx->name, "sky", 3) && mtwidth == 256 && mtheight == 128)
		{
			if (loadmodel->isworldmodel)
			{
				data = loadimagepixels(tx->name, false, 0, 0);
				if (data)
				{
					if (image_width == 256 && image_height == 128)
					{
 						R_InitSky (data, 4);
						Mem_Free(data);
					}
					else
					{
						Mem_Free(data);
						Con_Printf ("Invalid replacement texture for sky \"%s\" in %\"%s\", must be 256x128 pixels\n", tx->name, loadmodel->name);
						if (mtdata != NULL)
							R_InitSky (mtdata, 1);
					}
				}
				else if (mtdata != NULL)
					R_InitSky (mtdata, 1);
			}
		}
		else if ((tx->texture = loadtextureimagewithmask(loadmodel->texturepool, tx->name, 0, 0, false, true, true)))
		{
			tx->fogtexture = image_masktex;
			strcpy(name, tx->name);
			strcat(name, "_glow");
			tx->glowtexture = loadtextureimage(loadmodel->texturepool, name, 0, 0, false, true, true);
		}
		else
		{
			if (loadmodel->ishlbsp)
			{
				if (mtdata && (data = W_ConvertWAD3Texture(dmiptex)))
				{
					// texture included
					tx->texture = R_LoadTexture (loadmodel->texturepool, tx->name, image_width, image_height, data, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
					if (R_TextureHasAlpha(tx->texture))
					{
						// make mask texture
						for (j = 0;j < image_width * image_height;j++)
							data[j*4+0] = data[j*4+1] = data[j*4+2] = 255;
						strcpy(name, tx->name);
						strcat(name, "_fog");
						tx->fogtexture = R_LoadTexture (loadmodel->texturepool, name, image_width, image_height, data, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
					}
					Mem_Free(data);
				}
				else if ((data = W_GetTexture(tx->name)))
				{
					// get the size from the wad texture
					tx->width = image_width;
					tx->height = image_height;
					tx->texture = R_LoadTexture (loadmodel->texturepool, tx->name, image_width, image_height, data, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
					if (R_TextureHasAlpha(tx->texture))
					{
						// make mask texture
						for (j = 0;j < image_width * image_height;j++)
							data[j*4+0] = data[j*4+1] = data[j*4+2] = 255;
						strcpy(name, tx->name);
						strcat(name, "_fog");
						tx->fogtexture = R_LoadTexture (loadmodel->texturepool, name, image_width, image_height, data, TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_ALPHA | TEXF_PRECACHE);
					}
					Mem_Free(data);
				}
				else
				{
					tx->width = 16;
					tx->height = 16;
					tx->texture = r_notexture;
				}
			}
			else
			{
				if (mtdata) // texture included
				{
					int fullbrights;
					data = mtdata;
					fullbrights = false;
					if (r_fullbrights.value && tx->name[0] != '*')
					{
						for (j = 0;j < tx->width*tx->height;j++)
						{
							if (data[j] >= 224) // fullbright
							{
								fullbrights = true;
								break;
							}
						}
					}
					if (fullbrights)
					{
						data2 = Mem_Alloc(loadmodel->mempool, tx->width*tx->height);
						for (j = 0;j < tx->width*tx->height;j++)
							data2[j] = data[j] >= 224 ? 0 : data[j]; // no fullbrights
						tx->texture = R_LoadTexture (loadmodel->texturepool, tx->name, tx->width, tx->height, data2, TEXTYPE_QPALETTE, TEXF_MIPMAP | TEXF_PRECACHE);
						strcpy(name, tx->name);
						strcat(name, "_glow");
						for (j = 0;j < tx->width*tx->height;j++)
							data2[j] = data[j] >= 224 ? data[j] : 0; // only fullbrights
						tx->glowtexture = R_LoadTexture (loadmodel->texturepool, name, tx->width, tx->height, data2, TEXTYPE_QPALETTE, TEXF_MIPMAP | TEXF_PRECACHE);
						Mem_Free(data2);
					}
					else
						tx->texture = R_LoadTexture (loadmodel->texturepool, tx->name, tx->width, tx->height, data, TEXTYPE_QPALETTE, TEXF_MIPMAP | TEXF_PRECACHE);
				}
				else // no texture, and no external replacement texture was found
				{
					tx->width = 16;
					tx->height = 16;
					tx->texture = r_notexture;
				}
			}
		}

		if (tx->name[0] == '*')
		{
			tx->flags |= SURF_DRAWTURB | SURF_LIGHTBOTHSIDES;
			// LordHavoc: some turbulent textures should be fullbright and solid
			if (!strncmp(tx->name,"*lava",5)
			 || !strncmp(tx->name,"*teleport",9)
			 || !strncmp(tx->name,"*rift",5)) // Scourge of Armagon texture
				tx->flags |= SURF_DRAWFULLBRIGHT | SURF_DRAWNOALPHA;
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
			tx->shader = &Cshader_wall_lightmap;
		}

		tx->detailtexture = detailtextures[i % NUM_DETAILTEXTURES];
	}

	// sequence the animations
	for (i = 0;i < m->nummiptex;i++)
	{
		tx = loadmodel->textures + i;
		if (!tx || tx->name[0] != '+' || tx->name[1] == 0 || tx->name[2] == 0)
			continue;
		if (tx->anim_total[0] || tx->anim_total[1])
			continue;	// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		for (j = i;j < m->nummiptex;j++)
		{
			tx2 = loadmodel->textures + j;
			if (!tx2 || tx2->name[0] != '+' || strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= '0' && num <= '9')
				anims[num - '0'] = tx2;
			else if (num >= 'a' && num <= 'j')
				altanims[num - 'a'] = tx2;
			else
				Con_Printf ("Bad animating texture %s\n", tx->name);
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
				Con_Printf ("Missing frame %i of %s\n", j, tx->name);
				incomplete = true;
			}
		}
		for (j = 0;j < altmax;j++)
		{
			if (!altanims[j])
			{
				Con_Printf ("Missing altframe %i of %s\n", j, tx->name);
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

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting (lump_t *l)
{
	int i;
	qbyte *in, *out, *data, d;
	char litfilename[1024];
	loadmodel->lightdata = NULL;
	if (loadmodel->ishlbsp) // LordHavoc: load the colored lighting data straight
	{
		loadmodel->lightdata = Mem_Alloc(loadmodel->mempool, l->filelen);
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
	}
	else // LordHavoc: bsp version 29 (normal white lighting)
	{
		// LordHavoc: hope is not lost yet, check for a .lit file to load
		strcpy(litfilename, loadmodel->name);
		COM_StripExtension(litfilename, litfilename);
		strcat(litfilename, ".lit");
		data = (qbyte*) COM_LoadFile (litfilename, false);
		if (data)
		{
			if (loadsize > 8 && data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Con_DPrintf("%s loaded", litfilename);
					loadmodel->lightdata = Mem_Alloc(loadmodel->mempool, loadsize - 8);
					memcpy(loadmodel->lightdata, data + 8, loadsize - 8);
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
				if (loadsize == 8)
					Con_Printf("Empty .lit file, ignoring\n");
				else
					Con_Printf("Corrupt .lit file (old version?), ignoring\n");
				Mem_Free(data);
			}
		}
		// LordHavoc: oh well, expand the white lighting data
		if (!l->filelen)
			return;
		loadmodel->lightdata = Mem_Alloc(loadmodel->mempool, l->filelen*3);
		in = loadmodel->lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
		out = loadmodel->lightdata;
		memcpy (in, mod_base + l->fileofs, l->filelen);
		for (i = 0;i < l->filelen;i++)
		{
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	}
}

void Mod_LoadLightList(void)
{
	int a, n, numlights;
	char lightsfilename[1024], *s, *t, *lightsstring;
	mlight_t *e;

	strcpy(lightsfilename, loadmodel->name);
	COM_StripExtension(lightsfilename, lightsfilename);
	strcat(lightsfilename, ".lights");
	s = lightsstring = (char *) COM_LoadFile (lightsfilename, false);
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
		loadmodel->lights = Mem_Alloc(loadmodel->mempool, numlights * sizeof(mlight_t));
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
			e = loadmodel->lights + n;
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
		loadmodel->numlights = numlights;
		Mem_Free(lightsstring);
	}
}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (lump_t *l)
{
	loadmodel->visdata = NULL;
	if (!l->filelen)
		return;
	loadmodel->visdata = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

// used only for HalfLife maps
void Mod_ParseWadsFromEntityLump(const char *data)
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
			if (loadmodel->ishlbsp)
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
							W_LoadTextureWadFile (wadname, false);
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

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities (lump_t *l)
{
	loadmodel->entities = NULL;
	if (!l->filelen)
		return;
	loadmodel->entities = Mem_Alloc(loadmodel->mempool, l->filelen);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
	if (loadmodel->ishlbsp)
		Mod_ParseWadsFromEntityLump(loadmodel->entities);
}


/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count * sizeof(*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0;i < count;i++, in++, out++)
	{
		for (k = 0;k < 2;k++)
			for (j = 0;j < 4;j++)
				out->vecs[k][j] = LittleFloat (in->vecs[k][j]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		out->texture = NULL;
		if (loadmodel->textures)
		{
			if ((unsigned int) miptex >= (unsigned int) loadmodel->numtextures)
				Con_Printf ("error in model \"%s\": invalid miptex index %i (of %i)\n", loadmodel->name, miptex, loadmodel->numtextures);
			else
				out->texture = loadmodel->textures + miptex;
		}
		if (out->flags & TEX_SPECIAL)
		{
			// if texture chosen is NULL or the shader needs a lightmap,
			// force to notexture water shader
			if (out->texture == NULL || out->texture->shader->flags & SHADERFLAGS_NEEDLIGHTMAP)
				out->texture = loadmodel->textures + (loadmodel->numtextures - 1);
		}
		else
		{
			// if texture chosen is NULL, force to notexture
			if (out->texture == NULL)
				out->texture = loadmodel->textures + (loadmodel->numtextures - 2);
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999999;
	maxs[0] = maxs[1] = -999999999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}


void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
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

#if 0
#define MAX_SUBDIVPOLYTRIANGLES 4096
#define MAX_SUBDIVPOLYVERTS (MAX_SUBDIVPOLYTRIANGLES * 3)

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

static void SubdividePolygon (int numverts, float *verts)
{
	int		i, i1, i2, i3, f, b, c, p;
	vec3_t	mins, maxs, front[256], back[256];
	float	m, *pv, *cv, dist[256], frac;

	if (numverts > 250)
		Host_Error ("SubdividePolygon: ran out of verts in buffer");

	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0;i < 3;i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = r_subdivide_size.value * floor (m/r_subdivide_size.value + 0.5);
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
				VectorCopy (pv, front[f]);
				f++;
			}
			if (dist[p] <= 0)
			{
				VectorCopy (pv, back[b]);
				b++;
			}
			if (dist[p] == 0 || dist[c] == 0)
				continue;
			if ( (dist[p] > 0) != (dist[c] > 0) )
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

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
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

/*
================
Mod_GenerateWarpMesh

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void Mod_GenerateWarpMesh (msurface_t *surf)
{
	int i, j;
	surfvertex_t *v;
	surfmesh_t *mesh;

	subdivpolytriangles = 0;
	subdivpolyverts = 0;
	SubdividePolygon (surf->poly_numverts, surf->poly_verts);
	if (subdivpolytriangles < 1)
		Host_Error("Mod_GenerateWarpMesh: no triangles?\n");

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
		v->st[0] = DotProduct (v->v, surf->texinfo->vecs[0]);
		v->st[1] = DotProduct (v->v, surf->texinfo->vecs[1]);
	}
}
#endif

void Mod_GenerateWallMesh (msurface_t *surf, int vertexonly)
{
	int i, iu, iv, *index, smax, tmax;
	float *in, s, t, u, v, ubase, vbase, uscale, vscale;
	surfmesh_t *mesh;

	smax = surf->extents[0] >> 4;
	tmax = surf->extents[1] >> 4;

	if (vertexonly)
	{
		surf->lightmaptexturestride = 0;
		surf->lightmaptexture = NULL;
		uscale = 0;
		vscale = 0;
		ubase = 0;
		vbase = 0;
	}
	else
	{
		surf->flags |= SURF_LIGHTMAP;
		if (r_miplightmaps.integer)
		{
			surf->lightmaptexturestride = (surf->extents[0]>>4)+1;
			surf->lightmaptexture = R_LoadTexture(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_MIPMAP | TEXF_PRECACHE);
		}
		else
		{
			surf->lightmaptexturestride = R_CompatibleFragmentWidth((surf->extents[0]>>4)+1, loadmodel->lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, 0);
			surf->lightmaptexture = R_LoadTexture(loadmodel->texturepool, NULL, surf->lightmaptexturestride, (surf->extents[1]>>4)+1, NULL, loadmodel->lightmaprgba ? TEXTYPE_RGBA : TEXTYPE_RGB, TEXF_FRAGMENT | TEXF_PRECACHE);
		}
		R_FragmentLocation(surf->lightmaptexture, NULL, NULL, &ubase, &vbase, &uscale, &vscale);
		uscale = (uscale - ubase) * 16.0 / ((surf->extents[0] & ~15) + 16);
		vscale = (vscale - vbase) * 16.0 / ((surf->extents[1] & ~15) + 16);
	}

	surf->mesh = mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + (surf->poly_numverts - 2) * sizeof(int[3]) + surf->poly_numverts * (4 + 2 + 2 + 2 + 1) * sizeof(float));
	mesh->numverts = surf->poly_numverts;
	mesh->numtriangles = surf->poly_numverts - 2;
	mesh->verts = (float *)(mesh + 1);
	mesh->st = mesh->verts + mesh->numverts * 4;
	mesh->uv = mesh->st + mesh->numverts * 2;
	mesh->ab = mesh->uv + mesh->numverts * 2;
	mesh->lightmapoffsets = (int *)(mesh->ab + mesh->numverts * 2);
	mesh->index = mesh->lightmapoffsets + mesh->numverts;

	index = mesh->index;
	for (i = 0;i < mesh->numtriangles;i++)
	{
		*index++ = 0;
		*index++ = i + 1;
		*index++ = i + 2;
	}

	for (i = 0, in = surf->poly_verts;i < mesh->numverts;i++, in += 3)
	{
		s = DotProduct (in, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		t = DotProduct (in, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		u = (s + 8 - surf->texturemins[0]) * (1.0 / 16.0);
		v = (t + 8 - surf->texturemins[1]) * (1.0 / 16.0);
		// LordHavoc: calc lightmap data offset for vertex lighting to use
		iu = (int) u;
		iv = (int) v;
		iu = bound(0, iu, smax);
		iv = bound(0, iv, tmax);
		u = u * uscale + ubase;
		v = v * vscale + vbase;

		mesh->verts[i * 4 + 0] = in[0];
		mesh->verts[i * 4 + 1] = in[1];
		mesh->verts[i * 4 + 2] = in[2];
		mesh->st[i * 2 + 0] = s / surf->texinfo->texture->width;
		mesh->st[i * 2 + 1] = t / surf->texinfo->texture->height;
		mesh->uv[i * 2 + 0] = u;
		mesh->uv[i * 2 + 1] = v;
		mesh->ab[i * 2 + 0] = s * (1.0f / 16.0f);
		mesh->ab[i * 2 + 1] = t * (1.0f / 16.0f);
		mesh->lightmapoffsets[i] = ((iv * (smax+1) + iu) * 3);
	}
}

void Mod_GenerateVertexMesh (msurface_t *surf)
{
	int i, *index;
	float *in, s, t;
	surfmesh_t *mesh;

	surf->lightmaptexturestride = 0;
	surf->lightmaptexture = NULL;

	surf->mesh = mesh = Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t) + (surf->poly_numverts - 2) * sizeof(int[3]) + surf->poly_numverts * (4 + 2 + 2) * sizeof(float));
	mesh->numverts = surf->poly_numverts;
	mesh->numtriangles = surf->poly_numverts - 2;
	mesh->verts = (float *)(mesh + 1);
	mesh->st = mesh->verts + mesh->numverts * 4;
	mesh->ab = mesh->st + mesh->numverts * 2;
	mesh->index = (int *)(mesh->ab + mesh->numverts * 2);

	index = mesh->index;
	for (i = 0;i < mesh->numtriangles;i++)
	{
		*index++ = 0;
		*index++ = i + 1;
		*index++ = i + 2;
	}

	for (i = 0, in = surf->poly_verts;i < mesh->numverts;i++, in += 3)
	{
		s = (DotProduct (in, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
		t = (DotProduct (in, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);
		mesh->verts[i * 4 + 0] = in[0];
		mesh->verts[i * 4 + 1] = in[1];
		mesh->verts[i * 4 + 2] = in[2];
		mesh->st[i * 2 + 0] = s / surf->texinfo->texture->width;
		mesh->st[i * 2 + 1] = t / surf->texinfo->texture->height;
		mesh->ab[i * 2 + 0] = s * (1.0f / 16.0f);
		mesh->ab[i * 2 + 1] = t * (1.0f / 16.0f);
	}
}

void Mod_GenerateSurfacePolygon (msurface_t *surf)
{
	int i, lindex;
	float *vec, *vert, mins[3], maxs[3];

	// convert edges back to a normal polygon
	surf->poly_numverts = surf->numedges;
	vert = surf->poly_verts = Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * surf->numedges);
	for (i = 0;i < surf->numedges;i++)
	{
		lindex = loadmodel->surfedges[surf->firstedge + i];
		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, vert);
		vert += 3;
	}
	vert = surf->poly_verts;
	VectorCopy(vert, mins);
	VectorCopy(vert, maxs);
	vert += 3;
	for (i = 1;i < surf->poly_numverts;i++)
	{
		if (mins[0] > vert[0]) mins[0] = vert[0];if (maxs[0] < vert[0]) maxs[0] = vert[0];
		if (mins[1] > vert[1]) mins[1] = vert[1];if (maxs[1] < vert[1]) maxs[1] = vert[1];
		if (mins[2] > vert[2]) mins[2] = vert[2];if (maxs[2] < vert[2]) maxs[2] = vert[2];
		vert += 3;
	}
	VectorCopy(mins, surf->poly_mins);
	VectorCopy(maxs, surf->poly_maxs);
	surf->poly_center[0] = (mins[0] + maxs[0]) * 0.5f;
	surf->poly_center[1] = (mins[1] + maxs[1]) * 0.5f;
	surf->poly_center[2] = (mins[2] + maxs[2]) * 0.5f;
}

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces (lump_t *l)
{
	dface_t *in;
	msurface_t 	*out;
	int i, count, surfnum, planenum, ssize, tsize;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;
	loadmodel->surfacevisframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));
	loadmodel->surfacepvsframes = Mem_Alloc(loadmodel->mempool, count * sizeof(int));

	for (surfnum = 0;surfnum < count;surfnum++, in++, out++)
	{
		out->number = surfnum;
		// FIXME: validate edges, texinfo, etc?
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		if ((unsigned int) out->firstedge + (unsigned int) out->numedges > (unsigned int) loadmodel->numsurfedges)
			Host_Error("Mod_LoadFaces: invalid edge range (firstedge %i, numedges %i, model edges %i)\n", out->firstedge, out->numedges, loadmodel->numsurfedges);

		i = LittleShort (in->texinfo);
		if ((unsigned int) i >= (unsigned int) loadmodel->numtexinfo)
			Host_Error("Mod_LoadFaces: invalid texinfo index %i (model has %i texinfos)\n", i, loadmodel->numtexinfo);
		out->texinfo = loadmodel->texinfo + i;
		out->flags = out->texinfo->texture->flags;

		planenum = LittleShort(in->planenum);
		if ((unsigned int) planenum >= (unsigned int) loadmodel->numplanes)
			Host_Error("Mod_LoadFaces: invalid plane index %i (model has %i planes)\n", planenum, loadmodel->numplanes);

		if (LittleShort(in->side))
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		// clear lightmap (filled in later)
		out->lightmaptexture = NULL;

		// force lightmap upload on first time seeing the surface
		out->cached_dlight = true;

		CalcSurfaceExtents (out);

		ssize = (out->extents[0] >> 4) + 1;
		tsize = (out->extents[1] >> 4) + 1;

		// lighting info
		for (i = 0;i < MAXLIGHTMAPS;i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else if (loadmodel->ishlbsp) // LordHavoc: HalfLife map (bsp version 30)
			out->samples = loadmodel->lightdata + i;
		else // LordHavoc: white lighting (bsp version 29)
			out->samples = loadmodel->lightdata + (i * 3);

		Mod_GenerateSurfacePolygon(out);
		if (out->texinfo->texture->shader == &Cshader_wall_lightmap)
		{
			if ((out->extents[0] >> 4) + 1 > (256) || (out->extents[1] >> 4) + 1 > (256))
				Host_Error ("Bad surface extents");
			Mod_GenerateWallMesh (out, false);
			// stainmap for permanent marks on walls
			out->stainsamples = Mem_Alloc(loadmodel->mempool, ssize * tsize * 3);
			// clear to white
			memset(out->stainsamples, 255, ssize * tsize * 3);
		}
		else
			Mod_GenerateVertexMesh (out);
	}
}

/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// FIXME: Insert caustics here
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc(loadmodel->mempool, count*sizeof(*out));

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	if (loadmodel->ishlbsp)
	{
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
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
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;
		VectorSubtract(hull->clip_maxs, hull->clip_mins, hull->clip_size);

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
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
			Host_Error("Corrupt clipping hull (out of range child)\n");
	}
}

/*
=================
Mod_MakeHull0

Duplicate the drawing hull structure as a clipping hull
=================
*/
static void Mod_MakeHull0 (void)
{
	mnode_t		*in;
	dclipnode_t *out;
	int			i;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	out = Mem_Alloc(loadmodel->mempool, loadmodel->numnodes * sizeof(dclipnode_t));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = loadmodel->numnodes - 1;
	hull->planes = loadmodel->planes;

	for (i = 0;i < loadmodel->numnodes;i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		out->children[0] = in->children[0]->contents < 0 ? in->children[0]->contents : in->children[0] - loadmodel->nodes;
		out->children[1] = in->children[1]->contents < 0 ? in->children[1]->contents : in->children[1] - loadmodel->nodes;
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces (lump_t *l)
{
	int i, j;
	short *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	loadmodel->nummarksurfaces = l->filelen / sizeof(*in);
	loadmodel->marksurfaces = Mem_Alloc(loadmodel->mempool, loadmodel->nummarksurfaces * sizeof(int));

	for (i = 0;i < loadmodel->nummarksurfaces;i++)
	{
		j = (unsigned) LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Host_Error ("Mod_ParseMarksurfaces: bad surface number");
		loadmodel->marksurfaces[i] = j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges (lump_t *l)
{
	int		i;
	int		*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	loadmodel->numsurfedges = l->filelen / sizeof(*in);
	loadmodel->surfedges = Mem_Alloc(loadmodel->mempool, loadmodel->numsurfedges * sizeof(int));

	for (i = 0;i < loadmodel->numsurfedges;i++)
		loadmodel->surfedges[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes (lump_t *l)
{
	int			i;
	mplane_t	*out;
	dplane_t 	*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	loadmodel->numplanes = l->filelen / sizeof(*in);
	loadmodel->planes = out = Mem_Alloc(loadmodel->mempool, loadmodel->numplanes * sizeof(*out));

	for (i = 0;i < loadmodel->numplanes;i++, in++, out++)
	{
		out->normal[0] = LittleFloat (in->normal[0]);
		out->normal[1] = LittleFloat (in->normal[1]);
		out->normal[2] = LittleFloat (in->normal[2]);
		out->dist = LittleFloat (in->dist);

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
static winding_t *NewWinding (int points)
{
	winding_t *w;
	int size;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error("NewWinding: too many points\n");

	size = sizeof(winding_t) + sizeof(double[3]) * (points - 8);
	w = Mem_Alloc(loadmodel->mempool, size);
	memset (w, 0, size);

	return w;
}

static void FreeWinding (winding_t *w)
{
	Mem_Free(w);
}

/*
=================
BaseWindingForPlane
=================
*/
static winding_t *BaseWindingForPlane (mplane_t *p)
{
	double org[3], vright[3], vup[3], normal[3];
	winding_t *w;

	VectorCopy(p->normal, normal);
	VectorVectorsDouble(normal, vright, vup);

	VectorScale (vup, 1024.0*1024.0*1024.0, vup);
	VectorScale (vright, 1024.0*1024.0*1024.0, vright);

	// project a really big	axis aligned box onto the plane
	w = NewWinding (4);

	VectorScale (p->normal, p->dist, org);

	VectorSubtract (org, vright, w->points[0]);
	VectorAdd (w->points[0], vup, w->points[0]);

	VectorAdd (org, vright, w->points[1]);
	VectorAdd (w->points[1], vup, w->points[1]);

	VectorAdd (org, vright, w->points[2]);
	VectorSubtract (w->points[2], vup, w->points[2]);

	VectorSubtract (org, vright, w->points[3]);
	VectorSubtract (w->points[3], vup, w->points[3]);

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
static winding_t *ClipWinding (winding_t *in, mplane_t *split, int keepon)
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
		dists[i] = dot = DotProduct (in->points[i], split->normal) - split->dist;
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
		FreeWinding (in);
		return NULL;
	}
	if (!counts[1])
		return in;

	maxpts = in->numpoints+4;	// can't use counts[0]+2 because of fp grouping errors
	if (maxpts > MAX_POINTS_ON_WINDING)
		Sys_Error ("ClipWinding: maxpts > MAX_POINTS_ON_WINDING");

	neww = NewWinding (maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
		if (neww->numpoints >= maxpts)
			Sys_Error ("ClipWinding: points exceeded estimate");

		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
			neww->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, neww->points[neww->numpoints]);
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
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, neww->points[neww->numpoints]);
		neww->numpoints++;
	}

	// free the original winding
	FreeWinding (in);

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
static void DivideWinding (winding_t *in, mplane_t *split, winding_t **front, winding_t **back)
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
		dot = DotProduct (in->points[i], split->normal);
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
		Sys_Error ("ClipWinding: maxpts > MAX_POINTS_ON_WINDING");

	*front = f = NewWinding (maxpts);
	*back = b = NewWinding (maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
		if (f->numpoints >= maxpts || b->numpoints >= maxpts)
			Sys_Error ("DivideWinding: points exceeded estimate");

		p1 = in->points[i];

		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->points[b->numpoints]);
			b->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->points[f->numpoints]);
			f->numpoints++;
		}
		else if (sides[i] == SIDE_BACK)
		{
			VectorCopy (p1, b->points[b->numpoints]);
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
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, f->points[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->points[b->numpoints]);
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
static portal_t *AllocPortal (void)
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

static void Mod_RecursiveRecalcNodeBBox(mnode_t *node)
{
	// calculate children first
	if (node->children[0]->contents >= 0)
		Mod_RecursiveRecalcNodeBBox(node->children[0]);
	if (node->children[1]->contents >= 0)
		Mod_RecursiveRecalcNodeBBox(node->children[1]);

	// make combined bounding box from children
	node->mins[0] = min(node->children[0]->mins[0], node->children[1]->mins[0]);
	node->mins[1] = min(node->children[0]->mins[1], node->children[1]->mins[1]);
	node->mins[2] = min(node->children[0]->mins[2], node->children[1]->mins[2]);
	node->maxs[0] = max(node->children[0]->maxs[0], node->children[1]->maxs[0]);
	node->maxs[1] = max(node->children[0]->maxs[1], node->children[1]->maxs[1]);
	node->maxs[2] = max(node->children[0]->maxs[2], node->children[1]->maxs[2]);
}

static void Mod_FinalizePortals(void)
{
	int i, j, numportals, numpoints;
	portal_t *p, *pnext;
	mportal_t *portal;
	mvertex_t *point;
	mleaf_t *leaf, *endleaf;
	winding_t *w;

	// recalculate bounding boxes for all leafs (because qbsp is very sloppy)
	leaf = loadmodel->leafs;
	endleaf = leaf + loadmodel->numleafs;
	for (;leaf < endleaf;leaf++)
	{
		VectorSet(leaf->mins,  2000000000,  2000000000,  2000000000);
		VectorSet(leaf->maxs, -2000000000, -2000000000, -2000000000);
	}
	p = portalchain;
	while(p)
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

	Mod_RecursiveRecalcNodeBBox(loadmodel->nodes);

	// tally up portal and point counts
	p = portalchain;
	numportals = 0;
	numpoints = 0;
	while(p)
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
	loadmodel->portals = Mem_Alloc(loadmodel->mempool, numportals * sizeof(mportal_t) + numpoints * sizeof(mvertex_t));
	loadmodel->numportals = numportals;
	loadmodel->portalpoints = (void *) ((qbyte *) loadmodel->portals + numportals * sizeof(mportal_t));
	loadmodel->numportalpoints = numpoints;
	// clear all leaf portal chains
	for (i = 0;i < loadmodel->numleafs;i++)
		loadmodel->leafs[i].portals = NULL;
	// process all portals in the global portal chain, while freeing them
	portal = loadmodel->portals;
	point = loadmodel->portalpoints;
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
				// first make the back to front portal (forward portal)
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

				// then make the front to back portal (backward portal)
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
static void AddPortalToNodes (portal_t *p, mnode_t *front, mnode_t *back)
{
	if (!front)
		Host_Error ("AddPortalToNodes: NULL front node");
	if (!back)
		Host_Error ("AddPortalToNodes: NULL back node");
	if (p->nodes[0] || p->nodes[1])
		Host_Error ("AddPortalToNodes: already included");
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
				Host_Error ("RemovePortalFromNodes: portal not in leaf");

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
					Host_Error ("RemovePortalFromNodes: portal not bounding leaf");
				break;
			}

			if (t->nodes[0] == node)
				portalpointer = (void **) &t->next[0];
			else if (t->nodes[1] == node)
				portalpointer = (void **) &t->next[1];
			else
				Host_Error ("RemovePortalFromNodes: portal not bounding leaf");
		}
	}
}

static void Mod_RecursiveNodePortals (mnode_t *node)
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
		Host_Error("Mod_RecursiveNodePortals: corrupt node hierarchy");

	// create the new portal by generating a polygon for the node plane,
	// and clipping it by all of the other portals (which came from nodes above this one)
	nodeportal = AllocPortal ();
	nodeportal->plane = *node->plane;

	nodeportalwinding = BaseWindingForPlane (node->plane);
	side = 0;	// shut up compiler warning
	for (portal = (portal_t *)node->portals;portal;portal = portal->next[side])
	{
		clipplane = portal->plane;
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_RecursiveNodePortals: portal has same node on both sides (1)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
		{
			clipplane.dist = -clipplane.dist;
			VectorNegate (clipplane.normal, clipplane.normal);
			side = 1;
		}
		else
			Host_Error ("Mod_RecursiveNodePortals: mislinked portal");

		nodeportalwinding = ClipWinding (nodeportalwinding, &clipplane, true);
		if (!nodeportalwinding)
		{
			printf ("Mod_RecursiveNodePortals: WARNING: new portal was clipped away\n");
			break;
		}
	}

	if (nodeportalwinding)
	{
		// if the plane was not clipped on all sides, there was an error
		nodeportal->winding = nodeportalwinding;
		AddPortalToNodes (nodeportal, front, back);
	}

	// split the portals of this node along this node's plane and assign them to the children of this node
	// (migrating the portals downward through the tree)
	for (portal = (portal_t *)node->portals;portal;portal = nextportal)
	{
		if (portal->nodes[0] == portal->nodes[1])
			Host_Error("Mod_RecursiveNodePortals: portal has same node on both sides (2)");
		if (portal->nodes[0] == node)
			side = 0;
		else if (portal->nodes[1] == node)
			side = 1;
		else
			Host_Error ("Mod_RecursiveNodePortals: mislinked portal");
		nextportal = portal->next[side];

		other_node = portal->nodes[!side];
		RemovePortalFromNodes (portal);

		// cut the portal into two portals, one on each side of the node plane
		DivideWinding (portal->winding, plane, &frontwinding, &backwinding);

		if (!frontwinding)
		{
			if (side == 0)
				AddPortalToNodes (portal, back, other_node);
			else
				AddPortalToNodes (portal, other_node, back);
			continue;
		}
		if (!backwinding)
		{
			if (side == 0)
				AddPortalToNodes (portal, front, other_node);
			else
				AddPortalToNodes (portal, other_node, front);
			continue;
		}

		// the winding is split
		splitportal = AllocPortal ();
		temp = splitportal->chain;
		*splitportal = *portal;
		splitportal->chain = temp;
		splitportal->winding = backwinding;
		FreeWinding (portal->winding);
		portal->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes (portal, front, other_node);
			AddPortalToNodes (splitportal, back, other_node);
		}
		else
		{
			AddPortalToNodes (portal, other_node, front);
			AddPortalToNodes (splitportal, other_node, back);
		}
	}

	Mod_RecursiveNodePortals(front);
	Mod_RecursiveNodePortals(back);
}


static void Mod_MakePortals(void)
{
	portalchain = NULL;
	Mod_RecursiveNodePortals (loadmodel->nodes);
	Mod_FinalizePortals();
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	mempool_t	*mainmempool;
	char		*loadname;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION && i != 30)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake) or 30 (HalfLife))", mod->name, i, BSPVERSION);
	mod->ishlbsp = i == 30;
	if (loadmodel->isworldmodel)
	{
		Cvar_SetValue("halflifebsp", mod->ishlbsp);
		// until we get a texture for it...
		R_ResetQuakeSky();
	}

// swap all the lumps
	mod_base = (qbyte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap

	// store which lightmap format to use
	mod->lightmaprgba = r_lightmaprgba.integer;

	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();
	Mod_MakePortals();

	mod->numframes = 2;		// regular and alternate animation

	mainmempool = mod->mempool;
	loadname = mod->name;

	Mod_LoadLightList ();

//
// set up the submodels (FIXME: this is confusing)
//
	for (i = 0;i < mod->numsubmodels;i++)
	{
		int k, l;
		float dist, modelyawradius, modelradius, *vec;
		msurface_t *surf;

		mod->normalmins[0] = mod->normalmins[1] = mod->normalmins[2] = 1000000000.0f;
		mod->normalmaxs[0] = mod->normalmaxs[1] = mod->normalmaxs[2] = -1000000000.0f;
		modelyawradius = 0;
		modelradius = 0;

		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		mod->DrawSky = NULL;
		// LordHavoc: calculate bmodel bounding box rather than trusting what it says
		for (j = 0, surf = &mod->surfaces[mod->firstmodelsurface];j < mod->nummodelsurfaces;j++, surf++)
		{
			// we only need to have a drawsky function if it is used (usually only on world model)
			if (surf->texinfo->texture->shader == &Cshader_sky)
				mod->DrawSky = R_DrawBrushModelSky;
			for (k = 0;k < surf->numedges;k++)
			{
				l = mod->surfedges[k + surf->firstedge];
				if (l > 0)
					vec = mod->vertexes[mod->edges[l].v[0]].position;
				else
					vec = mod->vertexes[mod->edges[-l].v[1]].position;
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
		modelyawradius = sqrt(modelyawradius);
		modelradius = sqrt(modelradius);
		mod->yawmins[0] = mod->yawmins[1] = -(mod->yawmaxs[0] = mod->yawmaxs[1] = modelyawradius);
		mod->yawmins[2] = mod->normalmins[2];
		mod->yawmaxs[2] = mod->normalmaxs[2];
		mod->rotatedmins[0] = mod->rotatedmins[1] = mod->rotatedmins[2] = -modelradius;
		mod->rotatedmaxs[0] = mod->rotatedmaxs[1] = mod->rotatedmaxs[2] = modelradius;
		// LordHavoc: check for empty submodels (lacrima.bsp has such a glitch)
		if (mod->normalmins[0] > mod->normalmaxs[0] || mod->normalmins[1] > mod->normalmaxs[1] || mod->normalmins[2] > mod->normalmaxs[2])
		{
			Con_Printf("warning: empty submodel *%i in %s\n", i+1, loadname);
			VectorClear(mod->normalmins);
			VectorClear(mod->normalmaxs);
			VectorClear(mod->yawmins);
			VectorClear(mod->yawmaxs);
			VectorClear(mod->rotatedmins);
			VectorClear(mod->rotatedmaxs);
		}

		mod->numleafs = bm->visleafs;

		mod->Draw = R_DrawBrushModelNormal;
		mod->DrawFakeShadow = NULL;

		// LordHavoc: only register submodels if it is the world
		// (prevents bsp models from replacing world submodels)
		if (loadmodel->isworldmodel && i < (mod->numsubmodels - 1))
		{
			char	name[10];
			// duplicate the basic information
			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			// textures and memory belong to the main model
			loadmodel->texturepool = NULL;
			loadmodel->mempool = NULL;
			mod = loadmodel;
		}
	}
}

