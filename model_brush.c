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

byte	mod_novis[MAX_MAP_LEAFS/8];

qboolean	hlbsp; // LordHavoc: true if it is a HalfLife BSP file (version 30)

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};
cvar_t halflifebsp = {"halflifebsp", "0"};
cvar_t r_novis = {"r_novis", "0"};

/*
===============
Mod_BrushInit
===============
*/
void Mod_BrushInit (void)
{
	Cvar_RegisterVariable (&gl_subdivide_size);
	Cvar_RegisterVariable (&halflifebsp);
	Cvar_RegisterVariable (&r_novis);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	
//	if (!model || !model->nodes)
//		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	do
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];
	while (node->contents == 0);

	return (mleaf_t *)node;
}
/*
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}
*/

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;
	out = decompressed;

	/*
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}
	*/

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

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (r_novis.value || leaf == model->leafs || leaf->compressed_vis == NULL)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

rtexture_t *r_notexture;
texture_t r_notexture_mip;

void Mod_SetupNoTexture(void)
{
	int		x, y;
	byte	pix[16][16][4];

	// create a simple checkerboard texture for the default
	// LordHavoc: redesigned this to remove reliance on the palette and texture_t
	for (y = 0;y < 16;y++)
	{
		for (x = 0;x < 16;x++)
		{
			if ((y < 8) ^ (x < 8))
			{
				pix[y][x][0] = 128;
				pix[y][x][1] = 128;
				pix[y][x][2] = 128;
				pix[y][x][3] = 255;
			}
			else
			{
				pix[y][x][0] = 64;
				pix[y][x][1] = 64;
				pix[y][x][2] = 64;
				pix[y][x][3] = 255;
			}
		}
	}

	r_notexture = R_LoadTexture("notexture", 16, 16, &pix[0][0][0], TEXF_MIPMAP | TEXF_RGBA);

	strcpy(r_notexture_mip.name, "notexture");
	r_notexture_mip.width = 16;
	r_notexture_mip.height = 16;
	r_notexture_mip.transparent = false;
	r_notexture_mip.texture = r_notexture;
	r_notexture_mip.glowtexture = NULL;
}

/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int				i, j, k, num, max, altmax, mtwidth, mtheight, *dofs;
	miptex_t		*dmiptex;
	texture_t		*tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t	*m;
	byte			*data, *mtdata;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	
	m->nummiptex = LittleLong (m->nummiptex);
	
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures), va("%s texture headers", loadname));

	// just to work around bounds checking when debugging with it (array index out of bounds error thing)
	dofs = m->dataofs;
	for (i = 0;i < m->nummiptex;i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1)
			continue;
		dmiptex = (miptex_t *)((byte *)m + dofs[i]);
		mtwidth = LittleLong (dmiptex->width);
		mtheight = LittleLong (dmiptex->height);
		mtdata = NULL;
		j = LittleLong (dmiptex->offsets[0]);
		if (j)
		{
			// texture included
			if (j < 40 || j + mtwidth * mtheight > l->filelen)
				Host_Error ("Texture %s is corrupt or incomplete\n", dmiptex->name);
			mtdata = (byte *)dmiptex + j;
		}
		
		if ((mtwidth & 15) || (mtheight & 15))
			Host_Error ("Texture %s is not 16 aligned", dmiptex->name);
		// LordHavoc: rewriting the map texture loader for GLQuake
		tx = Hunk_AllocName (sizeof(texture_t), va("%s textures", loadname));
		loadmodel->textures[i] = tx;

		// LordHavoc: force all names to lowercase and make sure they are terminated while copying
		for (j = 0;dmiptex->name[j] && j < 15;j++)
		{
			if (dmiptex->name[j] >= 'A' && dmiptex->name[j] <= 'Z')
				tx->name[j] = dmiptex->name[j] + ('a' - 'A');
			else
				tx->name[j] = dmiptex->name[j];
		}
		for (;j < 16;j++)
			tx->name[j] = 0;

		if (!tx->name[0])
		{
			Con_Printf("warning: unnamed texture in %s\n", loadname);
			sprintf(tx->name, "unnamed%i", i);
		}

		tx->transparent = false;
		data = loadimagepixels(tx->name, false, 0, 0);
		if (data)
		{
			if (!hlbsp && !strncmp(tx->name,"sky",3) && image_width == 256 && image_height == 128) // LordHavoc: HL sky textures are entirely unrelated
			{
				tx->width = 0;
				tx->height = 0;
				tx->transparent = false;
				tx->texture = NULL;
				tx->glowtexture = NULL;
				R_InitSky (data, 4);
			}
			else
			{
				tx->width = mtwidth;
				tx->height = mtheight;
				tx->transparent = Image_CheckAlpha(data, image_width * image_height, true);
				tx->texture = R_LoadTexture (tx->name, image_width, image_height, data, TEXF_MIPMAP | (tx->transparent ? TEXF_ALPHA : 0) | TEXF_RGBA | TEXF_PRECACHE);
				tx->glowtexture = NULL;
			}
			qfree(data);
		}
		else
		{
			if (hlbsp)
			{
				if (mtdata) // texture included
				{
					data = W_ConvertWAD3Texture(dmiptex);
					if (data)
					{
						tx->width = mtwidth;
						tx->height = mtheight;
						tx->transparent = Image_CheckAlpha(data, mtwidth * mtheight, true);
						tx->texture = R_LoadTexture (tx->name, mtwidth, mtheight, data, TEXF_MIPMAP | (tx->transparent ? TEXF_ALPHA : 0) | TEXF_RGBA | TEXF_PRECACHE);
						tx->glowtexture = NULL;
						qfree(data);
					}
				}
				if (!data)
				{
					data = W_GetTexture(tx->name);
					// get the size from the wad texture
					if (data)
					{
						tx->width = image_width;
						tx->height = image_height;
						tx->transparent = Image_CheckAlpha(data, image_width * image_height, true);
						tx->texture = R_LoadTexture (tx->name, image_width, image_height, data, TEXF_MIPMAP | (tx->transparent ? TEXF_ALPHA : 0) | TEXF_RGBA | TEXF_PRECACHE);
						tx->glowtexture = NULL;
						qfree(data);
					}
				}
				if (!data)
				{
					tx->width = 16;
					tx->height = 16;
					tx->transparent = false;
					tx->texture = r_notexture;
					tx->glowtexture = NULL;
				}
			}
			else
			{
				if (!strncmp(tx->name,"sky",3) && mtwidth == 256 && mtheight == 128)
				{
					tx->width = mtwidth;
					tx->height = mtheight;
					tx->transparent = false;
					tx->texture = NULL;
					tx->glowtexture = NULL;
					R_InitSky (mtdata, 1);
				}
				else
				{
					if (mtdata) // texture included
					{
						int fullbrights;
						data = mtdata;
						tx->width = mtwidth;
						tx->height = mtheight;
						tx->transparent = false;
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
							char name[64];
							byte *data2;
							data2 = qmalloc(tx->width*tx->height);
							for (j = 0;j < tx->width*tx->height;j++)
								data2[j] = data[j] >= 224 ? 0 : data[j]; // no fullbrights
							tx->texture = R_LoadTexture (tx->name, tx->width, tx->height, data2, TEXF_MIPMAP | TEXF_PRECACHE);
							strcpy(name, tx->name);
							strcat(name, "_glow");
							for (j = 0;j < tx->width*tx->height;j++)
								data2[j] = data[j] >= 224 ? data[j] : 0; // only fullbrights
							tx->glowtexture = R_LoadTexture (name, tx->width, tx->height, data2, TEXF_MIPMAP | TEXF_PRECACHE);
							qfree(data2);
						}
						else
						{
							tx->texture = R_LoadTexture (tx->name, tx->width, tx->height, data, TEXF_MIPMAP | TEXF_PRECACHE);
							tx->glowtexture = NULL;
						}
					}
					else // no texture, and no external replacement texture was found
					{
						tx->width = 16;
						tx->height = 16;
						tx->transparent = false;
						tx->texture = r_notexture;
						tx->glowtexture = NULL;
					}
				}
			}
		}
	}

//
// sequence the animations
//
	for (i = 0;i < m->nummiptex;i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_total)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'a' && max <= 'j')
		{
			altmax = max - 'a';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Host_Error ("Bad animating texture %s", tx->name);

		for (j = i + 1;j < m->nummiptex;j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'a' && num <= 'j')
			{
				num = num - 'a';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Host_Error ("Bad animating texture %s", tx->name);
		}

		// link them all together
		for (j = 0;j < max;j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max;
			if (altmax)
				tx2->alternate_anims = altanims[0];
			for (k = 0;k < 10;k++)
				tx2->anim_frames[k] = anims[j];
		}
		for (j = 0;j < altmax;j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_Error ("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax;
			if (max)
				tx2->alternate_anims = anims[0];
			for (k = 0;k < 10;k++)
				tx2->anim_frames[k] = altanims[j];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	int i;
	byte *in, *out, *data;
	byte d;
	char litfilename[1024];
	loadmodel->lightdata = NULL;
	if (hlbsp) // LordHavoc: load the colored lighting data straight
	{
		loadmodel->lightdata = Hunk_AllocName ( l->filelen, va("%s lightmaps", loadname));
		memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
	}
	else // LordHavoc: bsp version 29 (normal white lighting)
	{
		// LordHavoc: hope is not lost yet, check for a .lit file to load
		strcpy(litfilename, loadmodel->name);
		COM_StripExtension(litfilename, litfilename);
		strcat(litfilename, ".lit");
		data = (byte*) COM_LoadHunkFile (litfilename, false);
		if (data)
		{
			if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
			{
				i = LittleLong(((int *)data)[1]);
				if (i == 1)
				{
					Con_DPrintf("%s loaded", litfilename);
					loadmodel->lightdata = data + 8;
					return;
				}
				else
					Con_Printf("Unknown .lit file version (%d)\n", i);
			}
			else
				Con_Printf("Corrupt .lit file (old version?), ignoring\n");
		}
		// LordHavoc: oh well, expand the white lighting data
		if (!l->filelen)
			return;
		loadmodel->lightdata = Hunk_AllocName ( l->filelen*3, va("%s lightmaps", loadname));
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


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( l->filelen, va("%s visdata", loadname));
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName ( l->filelen, va("%s entities", loadname));
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);

	if (isworldmodel)
		CL_ParseEntityLump(loadmodel->entities);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s vertices", loadname));

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
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s submodels", loadname));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
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
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1) * sizeof(*out), va("%s edges", loadname));

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
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, k, count;
	int		miptex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s texinfo", loadname));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (k=0 ; k<2 ; k++)
			for (j=0 ; j<4 ; j++)
				out->vecs[k][j] = LittleFloat (in->vecs[k][j]);

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures)
		{
			out->texture = &r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Host_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = &r_notexture_mip; // checkerboard texture
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

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
//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
		if ((tex->flags & TEX_SPECIAL) == 0 && (s->extents[i]+1) > (256*16))
			Host_Error ("Bad surface extents");
	}
}

void GL_SubdivideSurface (msurface_t *fa);

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s faces", loadname));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else if (hlbsp) // LordHavoc: HalfLife map (bsp version 30)
			out->samples = loadmodel->lightdata + i;
		else // LordHavoc: white lighting (bsp version 29)
			out->samples = loadmodel->lightdata + (i * 3); 
		
	// set the drawing flags flag
		
//		if (!strncmp(out->texinfo->texture->name,"sky",3))	// sky
		// LordHavoc: faster check
		if ((out->texinfo->texture->name[0] == 's' || out->texinfo->texture->name[0] == 'S')
		 && (out->texinfo->texture->name[1] == 'k' || out->texinfo->texture->name[1] == 'K')
		 && (out->texinfo->texture->name[2] == 'y' || out->texinfo->texture->name[2] == 'Y'))
		{
			// LordHavoc: for consistency reasons, mark sky as fullbright and solid as well
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED | SURF_DRAWFULLBRIGHT | SURF_DRAWNOALPHA | SURF_CLIPSOLID);
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}
		
//		if (!strncmp(out->texinfo->texture->name,"*",1))		// turbulent
		if (out->texinfo->texture->name[0] == '*') // LordHavoc: faster check
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED | SURF_LIGHTBOTHSIDES);
			// LordHavoc: some turbulent textures should be fullbright and solid
			if (!strncmp(out->texinfo->texture->name,"*lava",5)
			 || !strncmp(out->texinfo->texture->name,"*teleport",9)
			 || !strncmp(out->texinfo->texture->name,"*rift",5)) // Scourge of Armagon texture
				out->flags |= (SURF_DRAWFULLBRIGHT | SURF_DRAWNOALPHA);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}
		
		out->flags |= SURF_CLIPSOLID;
	}
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
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
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s nodes", loadname));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
//		for (j=0 ; j<3 ; j++)
//		{
//			out->mins[j] = LittleShort (in->mins[j]);
//			out->maxs[j] = LittleShort (in->maxs[j]);
//		}
	
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
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s leafs", loadname));

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

		// gl underwater warp
		// LordHavoc: disabled underwater warping
		/*
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
		*/
	}	
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s clipnodes", loadname));

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	if (hlbsp)
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
void Mod_MakeHull0 (void)
{
	mnode_t		*in;
	dclipnode_t *out;
	int			i, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), va("%s hull0", loadname));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0;i < count;i++, out++, in++)
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
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s marksurfaces", loadname));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Host_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), va("%s surfedges", loadname));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*2*sizeof(*out), va("%s planes", loadname));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
			out->normal[j] = LittleFloat (in->normal[j]);

		out->dist = LittleFloat (in->dist);
		// LordHavoc: recalculated by PlaneClassify, FIXME: validate type and report error if type does not match normal?
//		out->type = LittleLong (in->type);
		PlaneClassify(out);
	}
}

#define MAX_POINTS_ON_WINDING 64

typedef struct
{
	int numpoints;
	vec3_t points[8]; // variable sized
}
winding_t;

/*
==================
NewWinding
==================
*/
winding_t *NewWinding (int points)
{
	winding_t *w;
	int size;

	if (points > MAX_POINTS_ON_WINDING)
		Host_Error("NewWinding: too many points\n");

	size = (int)((winding_t *)0)->points[points];
	w = malloc (size);
	memset (w, 0, size);

	return w;
}

void FreeWinding (winding_t *w)
{
	free (w);
}

/*
=================
BaseWindingForPlane
=================
*/
winding_t *BaseWindingForPlane (mplane_t *p)
{
	vec3_t	org, vright, vup;
	winding_t	*w;

	VectorVectors(p->normal, vright, vup);

	VectorScale (vup, 65536, vup);
	VectorScale (vright, 65536, vright);

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
winding_t *ClipWinding (winding_t *in, mplane_t *split, int keepon)
{
	vec_t	dists[MAX_POINTS_ON_WINDING + 1];
	int		sides[MAX_POINTS_ON_WINDING + 1];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
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
	neww = NewWinding (maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
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

	if (neww->numpoints > maxpts)
		Host_Error ("ClipWinding: points exceeded estimate");

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
void DivideWinding (winding_t *in, mplane_t *split, winding_t **front, winding_t **back)
{
	vec_t	dists[MAX_POINTS_ON_WINDING + 1];
	int		sides[MAX_POINTS_ON_WINDING + 1];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
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

	*front = f = NewWinding (maxpts);
	*back = b = NewWinding (maxpts);

	for (i = 0;i < in->numpoints;i++)
	{
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

	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Host_Error ("DivideWinding: points exceeded estimate");
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
portal_t *AllocPortal (void)
{
	portal_t *p;
	p = malloc(sizeof(portal_t));
	memset(p, 0, sizeof(portal_t));
	p->chain = portalchain;
	portalchain = p;
	return p;
}

void Mod_FinalizePortals(void)
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
		VectorSet( 2000000000,  2000000000,  2000000000, leaf->mins);
		VectorSet(-2000000000, -2000000000, -2000000000, leaf->maxs);
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

	// tally up portal and point counts
	p = portalchain;
	numportals = 0;
	numpoints = 0;
	while(p)
	{
		if (p->winding && p->nodes[0] != p->nodes[1] && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID)
		{
			numportals += 2;
			numpoints += p->winding->numpoints * 2;
		}
		p = p->chain;
	}
	loadmodel->portals = Hunk_AllocName(numportals * sizeof(mportal_t), va("%s portals", loadmodel->name));
	loadmodel->numportals = numportals;
	loadmodel->portalpoints = Hunk_AllocName(numpoints * sizeof(mvertex_t), va("%s portals", loadmodel->name));
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
			// the nodes[0] != nodes[1] check is because leaf 0 is the shared solid leaf, it can have many portals inside with leaf 0 on both sides
			if (p->nodes[0] != p->nodes[1] && p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID)
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
		free(p);
		p = pnext;
	}
}

/*
=============
AddPortalToNodes
=============
*/
void AddPortalToNodes (portal_t *p, mnode_t *front, mnode_t *back)
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
void RemovePortalFromNodes(portal_t *portal)
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

void Mod_RecursiveNodePortals (mnode_t *node)
{
	int side;
	mnode_t *front, *back, *other_node;
	mplane_t clipplane, *plane;
	portal_t *portal, *nextportal, *nodeportal, *splitportal, *temp;
	winding_t *nodeportalwinding, *frontwinding, *backwinding;

	//	CheckLeafPortalConsistancy (node);

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

/*
void Mod_MakeOutsidePortals(mnode_t *node)
{
	int			i, j;
	portal_t	*p, *portals[6];
	mnode_t		*outside_node;

	outside_node = Hunk_AllocName(sizeof(mnode_t), loadmodel->name);
	outside_node->contents = CONTENTS_SOLID;
	outside_node->portals = NULL;

	for (i = 0;i < 3;i++)
	{
		for (j = 0;j < 2;j++)
		{
			portals[j*3 + i] = p = AllocPortal ();
			memset (&p->plane, 0, sizeof(mplane_t));
			p->plane.normal[i] = j ? -1 : 1;
			p->plane.dist = -65536;
			p->winding = BaseWindingForPlane (&p->plane);
			if (j)
				AddPortalToNodes (p, outside_node, node);
			else
				AddPortalToNodes (p, node, outside_node);
		}
	}

	// clip the basewindings by all the other planes
	for (i = 0;i < 6;i++)
	{
		for (j = 0;j < 6;j++)
		{
			if (j == i)
				continue;
			portals[i]->winding = ClipWinding (portals[i]->winding, &portals[j]->plane, true);
		}
	}
}
*/

void Mod_MakePortals(void)
{
//	Con_Printf("building portals for %s\n", loadmodel->name);

	portalchain = NULL;
//	Mod_MakeOutsidePortals (loadmodel->nodes);
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
	
	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION && i != 30)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i or 30 (HalfLife))", mod->name, i, BSPVERSION);
	hlbsp = i == 30;
	halflifebsp.value = hlbsp;

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	// LordHavoc: had to move entity loading above everything to allow parsing various settings from worldspawn
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
//	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	Mod_MakePortals();
	
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels (FIXME: this is confusing)
//
	for (i = 0;i < mod->numsubmodels;i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		
		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (isworldmodel && i < (mod->numsubmodels - 1)) // LordHavoc: only register submodels if it is the world (prevents bsp models from replacing world submodels)
		{	// duplicate the basic information
			char	name[10];

			sprintf (name, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}
