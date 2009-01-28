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

typedef struct mwad_s
{
	qfile_t *file;
	int numlumps;
	lumpinfo_t *lumps;
}
mwad_t;

typedef struct wadstate_s
{
	unsigned char *gfx_base;
	mwad_t gfx;
	memexpandablearray_t hlwads;
}
wadstate_t;

static wadstate_t wad;

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

static void W_SwapLumps(int numlumps, lumpinfo_t *lumps)
{
	int i;
	for (i = 0;i < numlumps;i++)
	{
		lumps[i].filepos = LittleLong(lumps[i].filepos);
		lumps[i].disksize = LittleLong(lumps[i].disksize);
		lumps[i].size = LittleLong(lumps[i].size);
		W_CleanupName(lumps[i].name, lumps[i].name);
	}
}

void W_UnloadAll(void)
{
	unsigned int i;
	mwad_t *w;
	// free gfx.wad if it is loaded
	if (wad.gfx_base)
		Mem_Free(wad.gfx_base);
	wad.gfx_base = NULL;
	// close all hlwad files and free their lumps data
	for (i = 0;i < Mem_ExpandableArray_IndexRange(&wad.hlwads);i++)
	{
		w = (mwad_t *) Mem_ExpandableArray_RecordAtIndex(&wad.hlwads, i);
		if (!w)
			continue;
		if (w->file)
			FS_Close(w->file);
		w->file = NULL;
		if (w->lumps)
			Mem_Free(w->lumps);
		w->lumps = NULL;
	}
	// free the hlwads array
	Mem_ExpandableArray_FreeArray(&wad.hlwads);
	// clear all state
	memset(&wad, 0, sizeof(wad));
}

unsigned char *W_GetLumpName(const char *name)
{
	int i;
	fs_offset_t filesize;
	lumpinfo_t *lump;
	char clean[16];
	wadinfo_t *header;
	int infotableofs;

	W_CleanupName (name, clean);

	if (!wad.gfx_base)
	{
		if ((wad.gfx_base = FS_LoadFile ("gfx.wad", cls.permanentmempool, false, &filesize)))
		{
			if (memcmp(wad.gfx_base, "WAD2", 4))
			{
				Con_Print("gfx.wad doesn't have WAD2 id\n");
				Mem_Free(wad.gfx_base);
				wad.gfx_base = NULL;
			}
			else
			{
				header = (wadinfo_t *)wad.gfx_base;
				wad.gfx.numlumps = LittleLong(header->numlumps);
				infotableofs = LittleLong(header->infotableofs);
				wad.gfx.lumps = (lumpinfo_t *)(wad.gfx_base + infotableofs);

				// byteswap the gfx.wad lumps in place
				W_SwapLumps(wad.gfx.numlumps, wad.gfx.lumps);
			}
		}
	}

	for (lump = wad.gfx.lumps, i = 0;i < wad.gfx.numlumps;i++, lump++)
		if (!strcmp(clean, lump->name))
			return (wad.gfx_base + lump->filepos);
	return NULL;
}

/*
====================
W_LoadTextureWadFile
====================
*/
void W_LoadTextureWadFile (char *filename, int complain)
{
	wadinfo_t		header;
	int				infotableofs;
	qfile_t			*file;
	int				numlumps;
	mwad_t			*w;

	file = FS_OpenVirtualFile(filename, false);
	if (!file)
	{
		if (complain)
			Con_Printf("W_LoadTextureWadFile: couldn't find %s\n", filename);
		return;
	}

	if (FS_Read(file, &header, sizeof(wadinfo_t)) != sizeof(wadinfo_t))
	{Con_Print("W_LoadTextureWadFile: unable to read wad header\n");FS_Close(file);file = NULL;return;}

	if(memcmp(header.identification, "WAD3", 4))
	{Con_Printf("W_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);FS_Close(file);file = NULL;return;}

	numlumps = LittleLong(header.numlumps);
	if (numlumps < 1 || numlumps > 65536)
	{Con_Printf("W_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);FS_Close(file);file = NULL;return;}
	infotableofs = LittleLong(header.infotableofs);
	if (FS_Seek (file, infotableofs, SEEK_SET))
	{Con_Print("W_LoadTextureWadFile: unable to seek to lump table\n");FS_Close(file);file = NULL;return;}

	if (!wad.hlwads.mempool)
		Mem_ExpandableArray_NewArray(&wad.hlwads, cls.permanentmempool, sizeof(mwad_t), 16);
	w = (mwad_t *) Mem_ExpandableArray_AllocRecord(&wad.hlwads);
	w->file = file;
	w->numlumps = numlumps;
	w->lumps = (lumpinfo_t *) Mem_Alloc(cls.permanentmempool, w->numlumps * sizeof(lumpinfo_t));

	if (!w->lumps)
	{
		Con_Print("W_LoadTextureWadFile: unable to allocate temporary memory for lump table\n");
		FS_Close(w->file);
		w->file = NULL;
		w->numlumps = 0;
		return;
	}

	if (FS_Read(file, w->lumps, sizeof(lumpinfo_t) * w->numlumps) != (fs_offset_t)sizeof(lumpinfo_t) * numlumps)
	{
		Con_Print("W_LoadTextureWadFile: unable to read lump table\n");
		FS_Close(w->file);
		w->file = NULL;
		w->numlumps = 0;
		Mem_Free(w->lumps);
		w->lumps = NULL;
		return;
	}

	W_SwapLumps(w->numlumps, w->lumps);

	// leaves the file open
}

unsigned char *W_ConvertWAD3TextureBGRA(miptex_t *tex)
{
	unsigned char *in, *data, *out, *pal;
	int d, p;

	in = (unsigned char *)tex + tex->offsets[0];
	data = out = (unsigned char *)Mem_Alloc(tempmempool, tex->width * tex->height * 4);
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
			out[2] = pal[p];
			out[1] = pal[p+1];
			out[0] = pal[p+2];
			out[3] = 255;
		}
		out += 4;
	}
	return data;
}

unsigned char *W_GetTextureBGRA(char *name)
{
	unsigned int i, j, k;
	miptex_t *tex;
	unsigned char *data;
	mwad_t *w;
	char texname[17];
	size_t range;

	texname[16] = 0;
	W_CleanupName(name, texname);
	if (!wad.hlwads.mempool)
		Mem_ExpandableArray_NewArray(&wad.hlwads, cls.permanentmempool, sizeof(mwad_t), 16);
	range = Mem_ExpandableArray_IndexRange(&wad.hlwads);
	for (k = 0;k < range;k++)
	{
		w = (mwad_t *)Mem_ExpandableArray_RecordAtIndex(&wad.hlwads, k);
		if (!w)
			continue;
		for (i = 0;i < (unsigned int)w->numlumps;i++)
		{
			if (!strcmp(texname, w->lumps[i].name)) // found it
			{
				if (FS_Seek(w->file, w->lumps[i].filepos, SEEK_SET))
				{Con_Print("W_GetTexture: corrupt WAD3 file\n");return NULL;}

				tex = (miptex_t *)Mem_Alloc(tempmempool, w->lumps[i].disksize);
				if (!tex)
					return NULL;
				if (FS_Read(w->file, tex, w->lumps[i].size) < w->lumps[i].disksize)
				{Con_Print("W_GetTexture: corrupt WAD3 file\n");return NULL;}

				tex->width = LittleLong(tex->width);
				tex->height = LittleLong(tex->height);
				for (j = 0;j < MIPLEVELS;j++)
					tex->offsets[j] = LittleLong(tex->offsets[j]);
				data = W_ConvertWAD3TextureBGRA(tex);
				Mem_Free(tex);
				return data;
			}
		}
	}
	image_width = image_height = 0;
	return NULL;
}

