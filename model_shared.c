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
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
void Mod_LoadQ2AliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

extern void Mod_BrushInit();
extern void Mod_AliasInit();
extern void Mod_SpriteInit();

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	Mod_BrushInit();
	Mod_AliasInit();
	Mod_SpriteInit();
}

/*
===============
Mod_Init

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Host_Error ("Mod_Extradata: caching failed");
	return mod->cache.data;
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;
	
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (mod->type != mod_alias)
			mod->needload = true;
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	
	if (!name[0])
		Host_Error ("Mod_ForName: NULL name");
		
//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			break;
			
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Host_Error ("mod_numknown == MAX_MOD_KNOWN");
		strcpy (mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
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
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	void	*d;
	unsigned *buf;
//	byte	stackbuf[1024];		// avoid dirtying the cache heap
	int		blah = 0;

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			d = Cache_Check (&mod->cache);
			if (d)
				return mod;
		}
		else
			return mod;		// not cached at all
	}

	Con_DPrintf("loading model %s\n", mod->name);

// load the file
	buf = (unsigned *)COM_LoadMallocFile (mod->name, false);
//	buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf), false);
	if (!buf)
	{
		if (crash)
			Host_Error ("Mod_NumForName: %s not found", mod->name); // LordHavoc: Sys_Error was *ANNOYING*
		return NULL;
	}
	
// allocate a new model
	COM_FileBase (mod->name, loadname);
	
	loadmodel = mod;

// call the apropriate loader
	mod->needload = false;
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case MD2IDALIASHEADER: // LordHavoc: added Quake2 model support
		Mod_LoadQ2AliasModel (mod, buf);
		break;
		
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;
	
	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}
	free(buf);

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, crash);
}

byte	*mod_base;

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return Length (corner);
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}


