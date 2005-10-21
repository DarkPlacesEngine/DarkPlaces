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
#include "wad.h"


void SwapPic (qpic_t *pic);


/*
==================
W_CleanupName

Lowercases name and pads with spaces and a terminating 0 to the length of
lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables.
Can safely be performed in place.
==================
*/
static void W_CleanupName (const char *in, char *out)
{
	int		i;
	int		c;

	for (i=0 ; i<16 ; i++ )
	{
		c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}

	for ( ; i< 16 ; i++ )
		out[i] = 0;
}

void *W_GetLumpName(const char *name)
{
	int i;
	lumpinfo_t *lump;
	char clean[16];
	wadinfo_t *header;
	int infotableofs;
	void *temp;
	static int wad_loaded = false;
	static int wad_numlumps = 0;
	static lumpinfo_t *wad_lumps = NULL;
	static qbyte *wad_base = NULL;

	W_CleanupName (name, clean);

	if (!wad_loaded)
	{
		wad_loaded = true;
		if ((temp = FS_LoadFile ("gfx.wad", tempmempool, false)))
		{
			if (memcmp(temp, "WAD2", 4))
				Con_Print("gfx.wad doesn't have WAD2 id\n");
			else
			{
				wad_base = (qbyte *)Mem_Alloc(cl_mempool, fs_filesize);

				memcpy(wad_base, temp, fs_filesize);
				Mem_Free(temp);

				header = (wadinfo_t *)wad_base;
				wad_numlumps = LittleLong(header->numlumps);
				infotableofs = LittleLong(header->infotableofs);
				wad_lumps = (lumpinfo_t *)(wad_base + infotableofs);

				for (i=0, lump = wad_lumps ; i<wad_numlumps ; i++,lump++)
				{
					lump->filepos = LittleLong(lump->filepos);
					lump->size = LittleLong(lump->size);
					W_CleanupName (lump->name, lump->name);
					if (lump->type == TYP_QPIC)
						SwapPic ( (qpic_t *)(wad_base + lump->filepos));
				}
			}
		}
	}

	for (lump = wad_lumps, i = 0;i < wad_numlumps;i++, lump++)
		if (!strcmp(clean, lump->name))
			return (void *)(wad_base + lump->filepos);

	if (wad_base)
		Con_DPrintf("W_GetLumpByName(\"%s\"): couldn't find file in gfx.wad\n", name);
	else
		Con_DPrintf("W_GetLumpByName(\"%s\"): couldn't load gfx.wad\n", name);
	return NULL;
}

/*
=============================================================================

automatic byte swapping

=============================================================================
*/

void SwapPic (qpic_t *pic)
{
	pic->width = LittleLong(pic->width);
	pic->height = LittleLong(pic->height);
}

// LordHavoc: added alternate WAD2/WAD3 system for HalfLife texture wads
#define TEXWAD_MAXIMAGES 16384
typedef struct texwadlump_s
{
	char name[16];
	qfile_t *file;
	int position;
	int size;
} texwadlump_t;

static texwadlump_t texwadlump[TEXWAD_MAXIMAGES];

/*
====================
W_LoadTextureWadFile
====================
*/
void W_LoadTextureWadFile (char *filename, int complain)
{
	lumpinfo_t		*lumps, *lump_p;
	wadinfo_t		header;
	int				i, j;
	int				infotableofs;
	qfile_t			*file;
	int				numlumps;

	file = FS_Open (filename, "rb", false, false);
	if (!file)
	{
		if (complain)
			Con_Printf("W_LoadTextureWadFile: couldn't find %s\n", filename);
		return;
	}

	if (FS_Read(file, &header, sizeof(wadinfo_t)) != sizeof(wadinfo_t))
	{Con_Print("W_LoadTextureWadFile: unable to read wad header\n");return;}

	if(memcmp(header.identification, "WAD3", 4))
	{Con_Printf("W_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);return;}

	numlumps = LittleLong(header.numlumps);
	if (numlumps < 1 || numlumps > TEXWAD_MAXIMAGES)
	{Con_Printf("W_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);return;}
	infotableofs = LittleLong(header.infotableofs);
	if (FS_Seek (file, infotableofs, SEEK_SET))
	{Con_Print("W_LoadTextureWadFile: unable to seek to lump table\n");return;}
	if (!(lumps = (lumpinfo_t *)Mem_Alloc(tempmempool, sizeof(lumpinfo_t)*numlumps)))
	{Con_Print("W_LoadTextureWadFile: unable to allocate temporary memory for lump table\n");return;}

	if (FS_Read(file, lumps, sizeof(lumpinfo_t) * numlumps) != (fs_offset_t)sizeof(lumpinfo_t) * numlumps)
	{Con_Print("W_LoadTextureWadFile: unable to read lump table\n");return;}

	for (i=0, lump_p = lumps ; i<numlumps ; i++,lump_p++)
	{
		W_CleanupName (lump_p->name, lump_p->name);
		for (j = 0;j < TEXWAD_MAXIMAGES;j++)
		{
			if (texwadlump[j].name[0]) // occupied slot, check the name
			{
				if (!strcmp(lump_p->name, texwadlump[j].name)) // name match, replace old one
					break;
			}
			else // empty slot
				break;
		}
		if (j >= TEXWAD_MAXIMAGES)
			break; // abort loading
		W_CleanupName (lump_p->name, texwadlump[j].name);
		texwadlump[j].file = file;
		texwadlump[j].position = LittleLong(lump_p->filepos);
		texwadlump[j].size = LittleLong(lump_p->disksize);
	}
	Mem_Free(lumps);
	// leaves the file open
}


qbyte *W_ConvertWAD3Texture(miptex_t *tex)
{
	qbyte *in, *data, *out, *pal;
	int d, p;

	in = (qbyte *)tex + tex->offsets[0];
	data = out = (qbyte *)Mem_Alloc(tempmempool, tex->width * tex->height * 4);
	if (!data)
		return NULL;
	image_width = tex->width;
	image_height = tex->height;
	pal = in + (((image_width * image_height) * 85) >> 6);
	pal += 2;
	for (d = 0;d < image_width * image_height;d++)
	{
		p = *in++;
		if (tex->name[0] == '{' && p == 255)
			out[0] = out[1] = out[2] = out[3] = 0;
		else
		{
			p *= 3;
			out[0] = pal[p];
			out[1] = pal[p+1];
			out[2] = pal[p+2];
			out[3] = 255;
		}
		out += 4;
	}
	return data;
}

qbyte *W_GetTexture(char *name)
{
	char texname[17];
	int i, j;
	qfile_t *file;
	miptex_t *tex;
	qbyte *data;

	texname[16] = 0;
	W_CleanupName (name, texname);
	for (i = 0;i < TEXWAD_MAXIMAGES;i++)
	{
		if (texwadlump[i].name[0])
		{
			if (!strcmp(texname, texwadlump[i].name)) // found it
			{
				file = texwadlump[i].file;
				if (FS_Seek(file, texwadlump[i].position, SEEK_SET))
				{Con_Print("W_GetTexture: corrupt WAD3 file\n");return NULL;}

				tex = (miptex_t *)Mem_Alloc(tempmempool, texwadlump[i].size);
				if (!tex)
					return NULL;
				if (FS_Read(file, tex, texwadlump[i].size) < texwadlump[i].size)
				{Con_Print("W_GetTexture: corrupt WAD3 file\n");return NULL;}

				tex->width = LittleLong(tex->width);
				tex->height = LittleLong(tex->height);
				for (j = 0;j < MIPLEVELS;j++)
					tex->offsets[j] = LittleLong(tex->offsets[j]);
				data = W_ConvertWAD3Texture(tex);
				Mem_Free(tex);
				return data;
			}
		}
		else
			break;
	}
	image_width = image_height = 0;
	return NULL;
}

