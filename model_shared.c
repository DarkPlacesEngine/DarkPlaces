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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "quakedef.h"

model_t	*loadmodel;

// LordHavoc: increased from 512 to 2048
#define	MAX_MOD_KNOWN	2048
static model_t mod_known[MAX_MOD_KNOWN];

static void mod_start(void)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);
}

static void mod_shutdown(void)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);
}

static void mod_newmap(void)
{
}

/*
===============
Mod_Init
===============
*/
static void Mod_Print (void);
static void Mod_Flush (void);
void Mod_Init (void)
{
	Mod_BrushInit();
	Mod_AliasInit();
	Mod_SpriteInit();

	Cmd_AddCommand ("modellist", Mod_Print);
	Cmd_AddCommand ("modelflush", Mod_Flush);
}

void Mod_RenderInit(void)
{
	R_RegisterModule("Models", mod_start, mod_shutdown, mod_newmap);
}

void Mod_FreeModel (model_t *mod)
{
	R_FreeTexturePool(&mod->texturepool);
	Mem_FreePool(&mod->mempool);

	// clear the struct to make it available
	memset(mod, 0, sizeof(model_t));
}

void Mod_UnloadModel (model_t *mod)
{
	char name[MAX_QPATH];
	qboolean isworldmodel;
	strcpy(name, mod->name);
	isworldmodel = mod->isworldmodel;
	Mod_FreeModel(mod);
	strcpy(mod->name, name);
	mod->isworldmodel = isworldmodel;
	mod->needload = true;
}

/*
==================
Mod_LoadModel

Loads a model
==================
*/
static model_t *Mod_LoadModel (model_t *mod, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	int crc;
	void *buf;

	mod->used = true;

	if (mod->name[0] == '*') // submodel
		return mod;

	crc = 0;
	buf = NULL;
	if (!mod->needload)
	{
		if (checkdisk)
		{
			buf = COM_LoadFile (mod->name, false);
			if (!buf)
			{
				if (crash)
					Host_Error ("Mod_LoadModel: %s not found", mod->name); // LordHavoc: Sys_Error was *ANNOYING*
				return NULL;
			}

			crc = CRC_Block(buf, com_filesize);
		}
		else
			crc = mod->crc;

		if (mod->crc == crc && mod->isworldmodel == isworldmodel)
		{
			if (buf)
				Mem_Free(buf);
			return mod; // already loaded
		}
	}

	Con_DPrintf("loading model %s\n", mod->name);

	if (!buf)
	{
		buf = COM_LoadFile (mod->name, false);
		if (!buf)
		{
			if (crash)
				Host_Error ("Mod_LoadModel: %s not found", mod->name);
			return NULL;
		}
		crc = CRC_Block(buf, com_filesize);
	}

	// make sure nothing got trashed
	//Mem_CheckSentinelsGlobal();

	// allocate a new model
	loadmodel = mod;

	// LordHavoc: unload the existing model in this slot (if there is one)
	Mod_UnloadModel(mod);
	mod->isworldmodel = isworldmodel;
	mod->needload = false;
	mod->used = true;
	mod->crc = crc;

	// all models use memory, so allocate a memory pool
	mod->mempool = Mem_AllocPool(mod->name);
	// all models load textures, so allocate a texture pool
	if (cls.state != ca_dedicated)
		mod->texturepool = R_AllocTexturePool();

	// call the apropriate loader
	     if (!memcmp(buf, "IDPO"    , 4)) Mod_LoadAliasModel  (mod, buf);
	else if (!memcmp(buf, "IDP2"    , 4)) Mod_LoadQ2AliasModel(mod, buf);
	else if (!memcmp(buf, "ZYMOTIC" , 7)) Mod_LoadZymoticModel(mod, buf);
	else if (!memcmp(buf, "IDSP"    , 4)) Mod_LoadSpriteModel (mod, buf);
	else                                  Mod_LoadBrushModel  (mod, buf);

	Mem_Free(buf);

	// make sure nothing got trashed
	//Mem_CheckSentinelsGlobal();

	return mod;
}

void Mod_CheckLoaded (model_t *mod)
{
	if (mod)
	{
		if (mod->needload)
			Mod_LoadModel(mod, true, true, mod->isworldmodel);
		else
		{
			if (mod->type == mod_invalid)
				Host_Error("Mod_CheckLoaded: invalid model\n");
			mod->used = true;
			return;
		}
	}
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	/*
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			if (mod->usesheap)
				Mod_FreeModel(mod);
	*/
}

void Mod_ClearUsed(void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			mod->used = false;
}

void Mod_PurgeUnused(void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			if (!mod->used)
				Mod_FreeModel(mod);
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod, *freemod;

	if (!name[0])
		Host_Error ("Mod_ForName: NULL name");

// search the currently loaded models
	freemod = NULL;
	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
	{
		if (mod->name[0])
		{
			if (!strcmp (mod->name, name))
			{
				mod->used = true;
				return mod;
			}
		}
		else if (freemod == NULL)
			freemod = mod;
	}

	if (freemod)
	{
		mod = freemod;
		strcpy (mod->name, name);
		mod->needload = true;
		mod->used = true;
		return mod;
	}

	Host_Error ("Mod_FindName: ran out of models\n");
	return NULL;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);
	mod->used = true;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	return Mod_LoadModel (Mod_FindName (name), crash, checkdisk, isworldmodel);
}

byte	*mod_base;

/*
=================
RadiusFromBounds
=================
*/
/*
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return Length (corner);
}
*/

//=============================================================================

/*
================
Mod_Print
================
*/
static void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Loaded models:\n");
	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			Con_Printf ("%4iK %s\n", mod->mempool ? (mod->mempool->totalsize + 1023) / 1024 : 0, mod->name);
}

static void Mod_Flush (void)
{
	int		i;

	Con_Printf ("Unloading models\n");
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);
}

