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
#include "image.h"
#include "r_shadow.h"
#include "polygon.h"

cvar_t r_enableshadowvolumes = {CVAR_SAVE, "r_enableshadowvolumes", "1", "Enables use of Stencil Shadow Volume shadowing methods, saves some memory if turned off"};
cvar_t r_mipskins = {CVAR_SAVE, "r_mipskins", "0", "mipmaps model skins so they render faster in the distance and do not display noise artifacts, can cause discoloration of skins if they contain undesirable border colors"};
cvar_t r_mipnormalmaps = {CVAR_SAVE, "r_mipnormalmaps", "1", "mipmaps normalmaps (turning it off looks sharper but may have aliasing)"};
cvar_t mod_generatelightmaps_unitspersample = {CVAR_SAVE, "mod_generatelightmaps_unitspersample", "8", "lightmap resolution"};
cvar_t mod_generatelightmaps_borderpixels = {CVAR_SAVE, "mod_generatelightmaps_borderpixels", "2", "extra space around polygons to prevent sampling artifacts"};
cvar_t mod_generatelightmaps_texturesize = {CVAR_SAVE, "mod_generatelightmaps_texturesize", "1024", "size of lightmap textures"};
cvar_t mod_generatelightmaps_lightmapsamples = {CVAR_SAVE, "mod_generatelightmaps_lightmapsamples", "16", "number of shadow tests done per lightmap pixel"};
cvar_t mod_generatelightmaps_vertexsamples = {CVAR_SAVE, "mod_generatelightmaps_vertexsamples", "16", "number of shadow tests done per vertex"};
cvar_t mod_generatelightmaps_gridsamples = {CVAR_SAVE, "mod_generatelightmaps_gridsamples", "64", "number of shadow tests done per lightgrid cell"};
cvar_t mod_generatelightmaps_lightmapradius = {CVAR_SAVE, "mod_generatelightmaps_lightmapradius", "16", "sampling area around each lightmap pixel"};
cvar_t mod_generatelightmaps_vertexradius = {CVAR_SAVE, "mod_generatelightmaps_vertexradius", "16", "sampling area around each vertex"};
cvar_t mod_generatelightmaps_gridradius = {CVAR_SAVE, "mod_generatelightmaps_gridradius", "64", "sampling area around each lightgrid cell center"};

dp_model_t *loadmodel;

static mempool_t *mod_mempool;
static memexpandablearray_t models;

static mempool_t* q3shaders_mem;
typedef struct q3shader_hash_entry_s
{
  q3shaderinfo_t shader;
  struct q3shader_hash_entry_s* chain;
} q3shader_hash_entry_t;
#define Q3SHADER_HASH_SIZE  1021
typedef struct q3shader_data_s
{
  memexpandablearray_t hash_entries;
  q3shader_hash_entry_t hash[Q3SHADER_HASH_SIZE];
  memexpandablearray_t char_ptrs;
} q3shader_data_t;
static q3shader_data_t* q3shader_data;

static void mod_start(void)
{
	int i, count;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	SCR_PushLoadingScreen(false, "Loading models", 1.0);
	count = 0;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*')
			if (mod->used)
				++count;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*')
			if (mod->used)
			{
				SCR_PushLoadingScreen(true, mod->name, 1.0 / count);
				Mod_LoadModel(mod, true, false);
				SCR_PopLoadingScreen(false);
			}
	SCR_PopLoadingScreen(false);
}

static void mod_shutdown(void)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && (mod->loaded || mod->mempool))
			Mod_UnloadModel(mod);

	Mod_FreeQ3Shaders();
	Mod_Skeletal_FreeBuffers();
}

static void mod_newmap(void)
{
	msurface_t *surface;
	int i, j, k, surfacenum, ssize, tsize;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->mempool)
		{
			for (j = 0;j < mod->num_textures && mod->data_textures;j++)
			{
				for (k = 0;k < mod->data_textures[j].numskinframes;k++)
					R_SkinFrame_MarkUsed(mod->data_textures[j].skinframes[k]);
				for (k = 0;k < mod->data_textures[j].backgroundnumskinframes;k++)
					R_SkinFrame_MarkUsed(mod->data_textures[j].backgroundskinframes[k]);
			}
			if (mod->brush.solidskyskinframe)
				R_SkinFrame_MarkUsed(mod->brush.solidskyskinframe);
			if (mod->brush.alphaskyskinframe)
				R_SkinFrame_MarkUsed(mod->brush.alphaskyskinframe);
		}
	}

	if (!cl_stainmaps_clearonload.integer)
		return;

	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->mempool && mod->data_surfaces)
		{
			for (surfacenum = 0, surface = mod->data_surfaces;surfacenum < mod->num_surfaces;surfacenum++, surface++)
			{
				if (surface->lightmapinfo && surface->lightmapinfo->stainsamples)
				{
					ssize = (surface->lightmapinfo->extents[0] >> 4) + 1;
					tsize = (surface->lightmapinfo->extents[1] >> 4) + 1;
					memset(surface->lightmapinfo->stainsamples, 255, ssize * tsize * 3);
					mod->brushq1.lightmapupdateflags[surfacenum] = true;
				}
			}
		}
	}
}

/*
===============
Mod_Init
===============
*/
static void Mod_Print(void);
static void Mod_Precache (void);
static void Mod_Decompile_f(void);
static void Mod_GenerateLightmaps_f(void);
void Mod_Init (void)
{
	mod_mempool = Mem_AllocPool("modelinfo", 0, NULL);
	Mem_ExpandableArray_NewArray(&models, mod_mempool, sizeof(dp_model_t), 16);

	Mod_BrushInit();
	Mod_AliasInit();
	Mod_SpriteInit();

	Cvar_RegisterVariable(&r_enableshadowvolumes);
	Cvar_RegisterVariable(&r_mipskins);
	Cvar_RegisterVariable(&r_mipnormalmaps);
	Cvar_RegisterVariable(&mod_generatelightmaps_unitspersample);
	Cvar_RegisterVariable(&mod_generatelightmaps_borderpixels);
	Cvar_RegisterVariable(&mod_generatelightmaps_texturesize);

	Cvar_RegisterVariable(&mod_generatelightmaps_lightmapsamples);
	Cvar_RegisterVariable(&mod_generatelightmaps_vertexsamples);
	Cvar_RegisterVariable(&mod_generatelightmaps_gridsamples);
	Cvar_RegisterVariable(&mod_generatelightmaps_lightmapradius);
	Cvar_RegisterVariable(&mod_generatelightmaps_vertexradius);
	Cvar_RegisterVariable(&mod_generatelightmaps_gridradius);

	Cmd_AddCommand ("modellist", Mod_Print, "prints a list of loaded models");
	Cmd_AddCommand ("modelprecache", Mod_Precache, "load a model");
	Cmd_AddCommand ("modeldecompile", Mod_Decompile_f, "exports a model in several formats for editing purposes");
	Cmd_AddCommand ("mod_generatelightmaps", Mod_GenerateLightmaps_f, "rebuilds lighting on current worldmodel");
}

void Mod_RenderInit(void)
{
	R_RegisterModule("Models", mod_start, mod_shutdown, mod_newmap, NULL, NULL);
}

void Mod_UnloadModel (dp_model_t *mod)
{
	char name[MAX_QPATH];
	qboolean used;
	dp_model_t *parentmodel;

	if (developer_loading.integer)
		Con_Printf("unloading model %s\n", mod->name);

	strlcpy(name, mod->name, sizeof(name));
	parentmodel = mod->brush.parentmodel;
	used = mod->used;
	if (mod->mempool)
	{
		if (mod->surfmesh.vertex3fbuffer)
			R_Mesh_DestroyMeshBuffer(mod->surfmesh.vertex3fbuffer);
		mod->surfmesh.vertex3fbuffer = NULL;
		if (mod->surfmesh.vertexmeshbuffer)
			R_Mesh_DestroyMeshBuffer(mod->surfmesh.vertexmeshbuffer);
		mod->surfmesh.vertexmeshbuffer = NULL;
		if (mod->surfmesh.data_element3i_indexbuffer)
			R_Mesh_DestroyMeshBuffer(mod->surfmesh.data_element3i_indexbuffer);
		mod->surfmesh.data_element3i_indexbuffer = NULL;
		if (mod->surfmesh.data_element3s_indexbuffer)
			R_Mesh_DestroyMeshBuffer(mod->surfmesh.data_element3s_indexbuffer);
		mod->surfmesh.data_element3s_indexbuffer = NULL;
		if (mod->surfmesh.vbo_vertexbuffer)
			R_Mesh_DestroyMeshBuffer(mod->surfmesh.vbo_vertexbuffer);
		mod->surfmesh.vbo_vertexbuffer = NULL;
	}
	// free textures/memory attached to the model
	R_FreeTexturePool(&mod->texturepool);
	Mem_FreePool(&mod->mempool);
	// clear the struct to make it available
	memset(mod, 0, sizeof(dp_model_t));
	// restore the fields we want to preserve
	strlcpy(mod->name, name, sizeof(mod->name));
	mod->brush.parentmodel = parentmodel;
	mod->used = used;
	mod->loaded = false;
}

void R_Model_Null_Draw(entity_render_t *ent)
{
	return;
}


typedef void (*mod_framegroupify_parsegroups_t) (unsigned int i, int start, int len, float fps, qboolean loop, void *pass);

int Mod_FrameGroupify_ParseGroups(const char *buf, mod_framegroupify_parsegroups_t cb, void *pass)
{
	const char *bufptr;
	int start, len;
	float fps;
	unsigned int i;
	qboolean loop;

	bufptr = buf;
	i = 0;
	for(;;)
	{
		// an anim scene!
		if (!COM_ParseToken_Simple(&bufptr, true, false))
			break;
		if (!strcmp(com_token, "\n"))
			continue; // empty line
		start = atoi(com_token);
		if (!COM_ParseToken_Simple(&bufptr, true, false))
			break;
		if (!strcmp(com_token, "\n"))
		{
			Con_Printf("framegroups file: missing number of frames\n");
			continue;
		}
		len = atoi(com_token);
		if (!COM_ParseToken_Simple(&bufptr, true, false))
			break;
		// we default to looping as it's usually wanted, so to NOT loop you append a 0
		if (strcmp(com_token, "\n"))
		{
			fps = atof(com_token);
			if (!COM_ParseToken_Simple(&bufptr, true, false))
				break;
			if (strcmp(com_token, "\n"))
				loop = atoi(com_token) != 0;
			else
				loop = true;
		}
		else
		{
			fps = 20;
			loop = true;
		}

		if(cb)
			cb(i, start, len, fps, loop, pass);
		++i;
	}

	return i;
}

void Mod_FrameGroupify_ParseGroups_Count (unsigned int i, int start, int len, float fps, qboolean loop, void *pass)
{
	unsigned int *cnt = (unsigned int *) pass;
	++*cnt;
}

void Mod_FrameGroupify_ParseGroups_Store (unsigned int i, int start, int len, float fps, qboolean loop, void *pass)
{
	dp_model_t *mod = (dp_model_t *) pass;
	animscene_t *anim = &mod->animscenes[i];
	dpsnprintf(anim->name, sizeof(anim[i].name), "groupified_%d_anim", i);
	anim->firstframe = bound(0, start, mod->num_poses - 1);
	anim->framecount = bound(1, len, mod->num_poses - anim->firstframe);
	anim->framerate = max(1, fps);
	anim->loop = !!loop;
	//Con_Printf("frame group %d is %d %d %f %d\n", i, start, len, fps, loop);
}

void Mod_FrameGroupify(dp_model_t *mod, const char *buf)
{
	unsigned int cnt;

	// 0. count
	cnt = Mod_FrameGroupify_ParseGroups(buf, NULL, NULL);
	if(!cnt)
	{
		Con_Printf("no scene found in framegroups file, aborting\n");
		return;
	}
	mod->numframes = cnt;

	// 1. reallocate
	// (we do not free the previous animscenes, but model unloading will free the pool owning them, so it's okay)
	mod->animscenes = (animscene_t *) Mem_Alloc(mod->mempool, sizeof(animscene_t) * mod->numframes);

	// 2. parse
	Mod_FrameGroupify_ParseGroups(buf, Mod_FrameGroupify_ParseGroups_Store, mod);
}

void Mod_FindPotentialDeforms(dp_model_t *mod)
{
	int i, j;
	texture_t *texture;
	mod->wantnormals = false;
	mod->wanttangents = false;
	for (i = 0;i < mod->num_textures;i++)
	{
		texture = mod->data_textures + i;
		if (texture->tcgen.tcgen == Q3TCGEN_ENVIRONMENT)
			mod->wantnormals = true;
		for (j = 0;j < Q3MAXDEFORMS;j++)
		{
			if (texture->deforms[j].deform == Q3DEFORM_AUTOSPRITE)
			{
				mod->wanttangents = true;
				mod->wantnormals = true;
				break;
			}
			if (texture->deforms[j].deform != Q3DEFORM_NONE)
				mod->wantnormals = true;
		}
	}
}

/*
==================
Mod_LoadModel

Loads a model
==================
*/
dp_model_t *Mod_LoadModel(dp_model_t *mod, qboolean crash, qboolean checkdisk)
{
	int num;
	unsigned int crc;
	void *buf;
	fs_offset_t filesize = 0;

	mod->used = true;

	if (mod->name[0] == '*') // submodel
		return mod;
	
	if (!strcmp(mod->name, "null"))
	{
		if(mod->loaded)
			return mod;

		if (mod->loaded || mod->mempool)
			Mod_UnloadModel(mod);

		if (developer_loading.integer)
			Con_Printf("loading model %s\n", mod->name);

		mod->used = true;
		mod->crc = (unsigned int)-1;
		mod->loaded = false;

		VectorClear(mod->normalmins);
		VectorClear(mod->normalmaxs);
		VectorClear(mod->yawmins);
		VectorClear(mod->yawmaxs);
		VectorClear(mod->rotatedmins);
		VectorClear(mod->rotatedmaxs);

		mod->modeldatatypestring = "null";
		mod->type = mod_null;
		mod->Draw = R_Model_Null_Draw;
		mod->numframes = 2;
		mod->numskins = 1;

		// no fatal errors occurred, so this model is ready to use.
		mod->loaded = true;

		return mod;
	}

	crc = 0;
	buf = NULL;

	// even if the model is loaded it still may need reloading...

	// if it is not loaded or checkdisk is true we need to calculate the crc
	if (!mod->loaded || checkdisk)
	{
		if (checkdisk && mod->loaded)
			Con_DPrintf("checking model %s\n", mod->name);
		buf = FS_LoadFile (mod->name, tempmempool, false, &filesize);
		if (buf)
		{
			crc = CRC_Block((unsigned char *)buf, filesize);
			// we need to reload the model if the crc does not match
			if (mod->crc != crc)
				mod->loaded = false;
		}
	}

	// if the model is already loaded and checks passed, just return
	if (mod->loaded)
	{
		if (buf)
			Mem_Free(buf);
		return mod;
	}

	if (developer_loading.integer)
		Con_Printf("loading model %s\n", mod->name);
	
	SCR_PushLoadingScreen(true, mod->name, 1);

	// LordHavoc: unload the existing model in this slot (if there is one)
	if (mod->loaded || mod->mempool)
		Mod_UnloadModel(mod);

	// load the model
	mod->used = true;
	mod->crc = crc;
	// errors can prevent the corresponding mod->loaded = true;
	mod->loaded = false;

	// default model radius and bounding box (mainly for missing models)
	mod->radius = 16;
	VectorSet(mod->normalmins, -mod->radius, -mod->radius, -mod->radius);
	VectorSet(mod->normalmaxs, mod->radius, mod->radius, mod->radius);
	VectorSet(mod->yawmins, -mod->radius, -mod->radius, -mod->radius);
	VectorSet(mod->yawmaxs, mod->radius, mod->radius, mod->radius);
	VectorSet(mod->rotatedmins, -mod->radius, -mod->radius, -mod->radius);
	VectorSet(mod->rotatedmaxs, mod->radius, mod->radius, mod->radius);

	if (!q3shaders_mem)
	{
		// load q3 shaders for the first time, or after a level change
		Mod_LoadQ3Shaders();
	}

	if (buf)
	{
		char *bufend = (char *)buf + filesize;

		// all models use memory, so allocate a memory pool
		mod->mempool = Mem_AllocPool(mod->name, 0, NULL);

		num = LittleLong(*((int *)buf));
		// call the apropriate loader
		loadmodel = mod;
		if (!strcasecmp(FS_FileExtension(mod->name), "obj")) Mod_OBJ_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDPO", 4)) Mod_IDP0_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDP2", 4)) Mod_IDP2_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDP3", 4)) Mod_IDP3_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDSP", 4)) Mod_IDSP_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDS2", 4)) Mod_IDS2_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IBSP", 4)) Mod_IBSP_Load(mod, buf, bufend);
		else if (!memcmp(buf, "ZYMOTICMODEL", 12)) Mod_ZYMOTICMODEL_Load(mod, buf, bufend);
		else if (!memcmp(buf, "DARKPLACESMODEL", 16)) Mod_DARKPLACESMODEL_Load(mod, buf, bufend);
		else if (!memcmp(buf, "ACTRHEAD", 8)) Mod_PSKMODEL_Load(mod, buf, bufend);
		else if (!memcmp(buf, "INTERQUAKEMODEL", 16)) Mod_INTERQUAKEMODEL_Load(mod, buf, bufend);
		else if (strlen(mod->name) >= 4 && !strcmp(mod->name + strlen(mod->name) - 4, ".map")) Mod_MAP_Load(mod, buf, bufend);
		else if (num == BSPVERSION || num == 30) Mod_Q1BSP_Load(mod, buf, bufend);
		else Con_Printf("Mod_LoadModel: model \"%s\" is of unknown/unsupported type\n", mod->name);
		Mem_Free(buf);

		Mod_FindPotentialDeforms(mod);
					
		buf = FS_LoadFile (va("%s.framegroups", mod->name), tempmempool, false, &filesize);
		if(buf)
		{
			Mod_FrameGroupify(mod, (const char *)buf);
			Mem_Free(buf);
		}

		Mod_BuildVBOs();
	}
	else if (crash)
	{
		// LordHavoc: Sys_Error was *ANNOYING*
		Con_Printf ("Mod_LoadModel: %s not found\n", mod->name);
	}

	// no fatal errors occurred, so this model is ready to use.
	mod->loaded = true;

	SCR_PopLoadingScreen(false);

	return mod;
}

void Mod_ClearUsed(void)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0])
			mod->used = false;
}

void Mod_PurgeUnused(void)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && !mod->used)
		{
			Mod_UnloadModel(mod);
			Mem_ExpandableArray_FreeRecord(&models, mod);
		}
	}
}

/*
==================
Mod_FindName

==================
*/
dp_model_t *Mod_FindName(const char *name, const char *parentname)
{
	int i;
	int nummodels;
	dp_model_t *mod;

	if (!parentname)
		parentname = "";

	// if we're not dedicatd, the renderer calls will crash without video
	Host_StartVideo();

	nummodels = Mem_ExpandableArray_IndexRange(&models);

	if (!name[0])
		Host_Error ("Mod_ForName: NULL name");

	// search the currently loaded models
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && !strcmp(mod->name, name) && ((!mod->brush.parentmodel && !parentname[0]) || (mod->brush.parentmodel && parentname[0] && !strcmp(mod->brush.parentmodel->name, parentname))))
		{
			mod->used = true;
			return mod;
		}
	}

	// no match found, create a new one
	mod = (dp_model_t *) Mem_ExpandableArray_AllocRecord(&models);
	strlcpy(mod->name, name, sizeof(mod->name));
	if (parentname[0])
		mod->brush.parentmodel = Mod_FindName(parentname, NULL);
	else
		mod->brush.parentmodel = NULL;
	mod->loaded = false;
	mod->used = true;
	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
dp_model_t *Mod_ForName(const char *name, qboolean crash, qboolean checkdisk, const char *parentname)
{
	dp_model_t *model;
	model = Mod_FindName(name, parentname);
	if (!model->loaded || checkdisk)
		Mod_LoadModel(model, crash, checkdisk);
	return model;
}

/*
==================
Mod_Reload

Reloads all models if they have changed
==================
*/
void Mod_Reload(void)
{
	int i, count;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	SCR_PushLoadingScreen(false, "Reloading models", 1.0);
	count = 0;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t *) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*' && mod->used)
			++count;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t *) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*' && mod->used)
		{
			SCR_PushLoadingScreen(true, mod->name, 1.0 / count);
			Mod_LoadModel(mod, true, true);
			SCR_PopLoadingScreen(false);
		}
	SCR_PopLoadingScreen(false);
}

unsigned char *mod_base;


//=============================================================================

/*
================
Mod_Print
================
*/
static void Mod_Print(void)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	Con_Print("Loaded models:\n");
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t *) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*')
		{
			if (mod->brush.numsubmodels)
				Con_Printf("%4iK %s (%i submodels)\n", mod->mempool ? (int)((mod->mempool->totalsize + 1023) / 1024) : 0, mod->name, mod->brush.numsubmodels);
			else
				Con_Printf("%4iK %s\n", mod->mempool ? (int)((mod->mempool->totalsize + 1023) / 1024) : 0, mod->name);
		}
	}
}

/*
================
Mod_Precache
================
*/
static void Mod_Precache(void)
{
	if (Cmd_Argc() == 2)
		Mod_ForName(Cmd_Argv(1), false, true, Cmd_Argv(1)[0] == '*' ? cl.model_name[1] : NULL);
	else
		Con_Print("usage: modelprecache <filename>\n");
}

int Mod_BuildVertexRemapTableFromElements(int numelements, const int *elements, int numvertices, int *remapvertices)
{
	int i, count;
	unsigned char *used;
	used = (unsigned char *)Mem_Alloc(tempmempool, numvertices);
	memset(used, 0, numvertices);
	for (i = 0;i < numelements;i++)
		used[elements[i]] = 1;
	for (i = 0, count = 0;i < numvertices;i++)
		remapvertices[i] = used[i] ? count++ : -1;
	Mem_Free(used);
	return count;
}

#if 1
// fast way, using an edge hash
#define TRIANGLEEDGEHASH 8192
void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles)
{
	int i, j, p, e1, e2, *n, hashindex, count, match;
	const int *e;
	typedef struct edgehashentry_s
	{
		struct edgehashentry_s *next;
		int triangle;
		int element[2];
	}
	edgehashentry_t;
	static edgehashentry_t **edgehash;
	edgehashentry_t *edgehashentries, *hash;
	if (!numtriangles)
		return;
	edgehash = (edgehashentry_t **)Mem_Alloc(tempmempool, TRIANGLEEDGEHASH * sizeof(*edgehash));
	// if there are too many triangles for the stack array, allocate larger buffer
	edgehashentries = (edgehashentry_t *)Mem_Alloc(tempmempool, numtriangles * 3 * sizeof(edgehashentry_t));
	// find neighboring triangles
	for (i = 0, e = elements, n = neighbors;i < numtriangles;i++, e += 3, n += 3)
	{
		for (j = 0, p = 2;j < 3;p = j, j++)
		{
			e1 = e[p];
			e2 = e[j];
			// this hash index works for both forward and backward edges
			hashindex = (unsigned int)(e1 + e2) % TRIANGLEEDGEHASH;
			hash = edgehashentries + i * 3 + j;
			hash->next = edgehash[hashindex];
			edgehash[hashindex] = hash;
			hash->triangle = i;
			hash->element[0] = e1;
			hash->element[1] = e2;
		}
	}
	for (i = 0, e = elements, n = neighbors;i < numtriangles;i++, e += 3, n += 3)
	{
		for (j = 0, p = 2;j < 3;p = j, j++)
		{
			e1 = e[p];
			e2 = e[j];
			// this hash index works for both forward and backward edges
			hashindex = (unsigned int)(e1 + e2) % TRIANGLEEDGEHASH;
			count = 0;
			match = -1;
			for (hash = edgehash[hashindex];hash;hash = hash->next)
			{
				if (hash->element[0] == e2 && hash->element[1] == e1)
				{
					if (hash->triangle != i)
						match = hash->triangle;
					count++;
				}
				else if ((hash->element[0] == e1 && hash->element[1] == e2))
					count++;
			}
			// detect edges shared by three triangles and make them seams
			if (count > 2)
				match = -1;
			n[p] = match;
		}

		// also send a keepalive here (this can take a while too!)
		CL_KeepaliveMessage(false);
	}
	// free the allocated buffer
	Mem_Free(edgehashentries);
	Mem_Free(edgehash);
}
#else
// very slow but simple way
static int Mod_FindTriangleWithEdge(const int *elements, int numtriangles, int start, int end, int ignore)
{
	int i, match, count;
	count = 0;
	match = -1;
	for (i = 0;i < numtriangles;i++, elements += 3)
	{
		     if ((elements[0] == start && elements[1] == end)
		      || (elements[1] == start && elements[2] == end)
		      || (elements[2] == start && elements[0] == end))
		{
			if (i != ignore)
				match = i;
			count++;
		}
		else if ((elements[1] == start && elements[0] == end)
		      || (elements[2] == start && elements[1] == end)
		      || (elements[0] == start && elements[2] == end))
			count++;
	}
	// detect edges shared by three triangles and make them seams
	if (count > 2)
		match = -1;
	return match;
}

void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles)
{
	int i, *n;
	const int *e;
	for (i = 0, e = elements, n = neighbors;i < numtriangles;i++, e += 3, n += 3)
	{
		n[0] = Mod_FindTriangleWithEdge(elements, numtriangles, e[1], e[0], i);
		n[1] = Mod_FindTriangleWithEdge(elements, numtriangles, e[2], e[1], i);
		n[2] = Mod_FindTriangleWithEdge(elements, numtriangles, e[0], e[2], i);
	}
}
#endif

void Mod_ValidateElements(int *elements, int numtriangles, int firstvertex, int numverts, const char *filename, int fileline)
{
	int i, warned = false, endvertex = firstvertex + numverts;
	for (i = 0;i < numtriangles * 3;i++)
	{
		if (elements[i] < firstvertex || elements[i] >= endvertex)
		{
			if (!warned)
			{
				warned = true;
				Con_Printf("Mod_ValidateElements: out of bounds elements detected at %s:%d\n", filename, fileline);
			}
			elements[i] = firstvertex;
		}
	}
}

// warning: this is an expensive function!
void Mod_BuildNormals(int firstvertex, int numvertices, int numtriangles, const float *vertex3f, const int *elements, float *normal3f, qboolean areaweighting)
{
	int i, j;
	const int *element;
	float *vectorNormal;
	float areaNormal[3];
	// clear the vectors
	memset(normal3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	// process each vertex of each triangle and accumulate the results
	// use area-averaging, to make triangles with a big area have a bigger
	// weighting on the vertex normal than triangles with a small area
	// to do so, just add the 'normals' together (the bigger the area
	// the greater the length of the normal is
	element = elements;
	for (i = 0; i < numtriangles; i++, element += 3)
	{
		TriangleNormal(
			vertex3f + element[0] * 3,
			vertex3f + element[1] * 3,
			vertex3f + element[2] * 3,
			areaNormal
			);

		if (!areaweighting)
			VectorNormalize(areaNormal);

		for (j = 0;j < 3;j++)
		{
			vectorNormal = normal3f + element[j] * 3;
			vectorNormal[0] += areaNormal[0];
			vectorNormal[1] += areaNormal[1];
			vectorNormal[2] += areaNormal[2];
		}
	}
	// and just normalize the accumulated vertex normal in the end
	vectorNormal = normal3f + 3 * firstvertex;
	for (i = 0; i < numvertices; i++, vectorNormal += 3)
		VectorNormalize(vectorNormal);
}

void Mod_BuildBumpVectors(const float *v0, const float *v1, const float *v2, const float *tc0, const float *tc1, const float *tc2, float *svector3f, float *tvector3f, float *normal3f)
{
	float f, tangentcross[3], v10[3], v20[3], tc10[2], tc20[2];
	// 79 add/sub/negate/multiply (1 cycle), 1 compare (3 cycle?), total cycles not counting load/store/exchange roughly 82 cycles
	// 6 add, 28 subtract, 39 multiply, 1 compare, 50% chance of 6 negates

	// 6 multiply, 9 subtract
	VectorSubtract(v1, v0, v10);
	VectorSubtract(v2, v0, v20);
	normal3f[0] = v20[1] * v10[2] - v20[2] * v10[1];
	normal3f[1] = v20[2] * v10[0] - v20[0] * v10[2];
	normal3f[2] = v20[0] * v10[1] - v20[1] * v10[0];
	// 12 multiply, 10 subtract
	tc10[1] = tc1[1] - tc0[1];
	tc20[1] = tc2[1] - tc0[1];
	svector3f[0] = tc10[1] * v20[0] - tc20[1] * v10[0];
	svector3f[1] = tc10[1] * v20[1] - tc20[1] * v10[1];
	svector3f[2] = tc10[1] * v20[2] - tc20[1] * v10[2];
	tc10[0] = tc1[0] - tc0[0];
	tc20[0] = tc2[0] - tc0[0];
	tvector3f[0] = tc10[0] * v20[0] - tc20[0] * v10[0];
	tvector3f[1] = tc10[0] * v20[1] - tc20[0] * v10[1];
	tvector3f[2] = tc10[0] * v20[2] - tc20[0] * v10[2];
	// 12 multiply, 4 add, 6 subtract
	f = DotProduct(svector3f, normal3f);
	svector3f[0] -= f * normal3f[0];
	svector3f[1] -= f * normal3f[1];
	svector3f[2] -= f * normal3f[2];
	f = DotProduct(tvector3f, normal3f);
	tvector3f[0] -= f * normal3f[0];
	tvector3f[1] -= f * normal3f[1];
	tvector3f[2] -= f * normal3f[2];
	// if texture is mapped the wrong way (counterclockwise), the tangents
	// have to be flipped, this is detected by calculating a normal from the
	// two tangents, and seeing if it is opposite the surface normal
	// 9 multiply, 2 add, 3 subtract, 1 compare, 50% chance of: 6 negates
	CrossProduct(tvector3f, svector3f, tangentcross);
	if (DotProduct(tangentcross, normal3f) < 0)
	{
		VectorNegate(svector3f, svector3f);
		VectorNegate(tvector3f, tvector3f);
	}
}

// warning: this is a very expensive function!
void Mod_BuildTextureVectorsFromNormals(int firstvertex, int numvertices, int numtriangles, const float *vertex3f, const float *texcoord2f, const float *normal3f, const int *elements, float *svector3f, float *tvector3f, qboolean areaweighting)
{
	int i, tnum;
	float sdir[3], tdir[3], normal[3], *sv, *tv;
	const float *v0, *v1, *v2, *tc0, *tc1, *tc2, *n;
	float f, tangentcross[3], v10[3], v20[3], tc10[2], tc20[2];
	const int *e;
	// clear the vectors
	memset(svector3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	memset(tvector3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	// process each vertex of each triangle and accumulate the results
	for (tnum = 0, e = elements;tnum < numtriangles;tnum++, e += 3)
	{
		v0 = vertex3f + e[0] * 3;
		v1 = vertex3f + e[1] * 3;
		v2 = vertex3f + e[2] * 3;
		tc0 = texcoord2f + e[0] * 2;
		tc1 = texcoord2f + e[1] * 2;
		tc2 = texcoord2f + e[2] * 2;

		// 79 add/sub/negate/multiply (1 cycle), 1 compare (3 cycle?), total cycles not counting load/store/exchange roughly 82 cycles
		// 6 add, 28 subtract, 39 multiply, 1 compare, 50% chance of 6 negates

		// calculate the edge directions and surface normal
		// 6 multiply, 9 subtract
		VectorSubtract(v1, v0, v10);
		VectorSubtract(v2, v0, v20);
		normal[0] = v20[1] * v10[2] - v20[2] * v10[1];
		normal[1] = v20[2] * v10[0] - v20[0] * v10[2];
		normal[2] = v20[0] * v10[1] - v20[1] * v10[0];

		// calculate the tangents
		// 12 multiply, 10 subtract
		tc10[1] = tc1[1] - tc0[1];
		tc20[1] = tc2[1] - tc0[1];
		sdir[0] = tc10[1] * v20[0] - tc20[1] * v10[0];
		sdir[1] = tc10[1] * v20[1] - tc20[1] * v10[1];
		sdir[2] = tc10[1] * v20[2] - tc20[1] * v10[2];
		tc10[0] = tc1[0] - tc0[0];
		tc20[0] = tc2[0] - tc0[0];
		tdir[0] = tc10[0] * v20[0] - tc20[0] * v10[0];
		tdir[1] = tc10[0] * v20[1] - tc20[0] * v10[1];
		tdir[2] = tc10[0] * v20[2] - tc20[0] * v10[2];

		// if texture is mapped the wrong way (counterclockwise), the tangents
		// have to be flipped, this is detected by calculating a normal from the
		// two tangents, and seeing if it is opposite the surface normal
		// 9 multiply, 2 add, 3 subtract, 1 compare, 50% chance of: 6 negates
		CrossProduct(tdir, sdir, tangentcross);
		if (DotProduct(tangentcross, normal) < 0)
		{
			VectorNegate(sdir, sdir);
			VectorNegate(tdir, tdir);
		}

		if (!areaweighting)
		{
			VectorNormalize(sdir);
			VectorNormalize(tdir);
		}
		for (i = 0;i < 3;i++)
		{
			VectorAdd(svector3f + e[i]*3, sdir, svector3f + e[i]*3);
			VectorAdd(tvector3f + e[i]*3, tdir, tvector3f + e[i]*3);
		}
	}
	// make the tangents completely perpendicular to the surface normal, and
	// then normalize them
	// 16 assignments, 2 divide, 2 sqrt, 2 negates, 14 adds, 24 multiplies
	for (i = 0, sv = svector3f + 3 * firstvertex, tv = tvector3f + 3 * firstvertex, n = normal3f + 3 * firstvertex;i < numvertices;i++, sv += 3, tv += 3, n += 3)
	{
		f = -DotProduct(sv, n);
		VectorMA(sv, f, n, sv);
		VectorNormalize(sv);
		f = -DotProduct(tv, n);
		VectorMA(tv, f, n, tv);
		VectorNormalize(tv);
	}
}

void Mod_AllocSurfMesh(mempool_t *mempool, int numvertices, int numtriangles, qboolean lightmapoffsets, qboolean vertexcolors, qboolean neighbors)
{
	unsigned char *data;
	data = (unsigned char *)Mem_Alloc(mempool, numvertices * (3 + 3 + 3 + 3 + 2 + 2 + (vertexcolors ? 4 : 0)) * sizeof(float) + numvertices * (lightmapoffsets ? 1 : 0) * sizeof(int) + numtriangles * (3 + (neighbors ? 3 : 0)) * sizeof(int) + (numvertices <= 65536 ? numtriangles * sizeof(unsigned short[3]) : 0));
	loadmodel->surfmesh.num_vertices = numvertices;
	loadmodel->surfmesh.num_triangles = numtriangles;
	if (loadmodel->surfmesh.num_vertices)
	{
		loadmodel->surfmesh.data_vertex3f = (float *)data, data += sizeof(float[3]) * loadmodel->surfmesh.num_vertices;
		loadmodel->surfmesh.data_svector3f = (float *)data, data += sizeof(float[3]) * loadmodel->surfmesh.num_vertices;
		loadmodel->surfmesh.data_tvector3f = (float *)data, data += sizeof(float[3]) * loadmodel->surfmesh.num_vertices;
		loadmodel->surfmesh.data_normal3f = (float *)data, data += sizeof(float[3]) * loadmodel->surfmesh.num_vertices;
		loadmodel->surfmesh.data_texcoordtexture2f = (float *)data, data += sizeof(float[2]) * loadmodel->surfmesh.num_vertices;
		loadmodel->surfmesh.data_texcoordlightmap2f = (float *)data, data += sizeof(float[2]) * loadmodel->surfmesh.num_vertices;
		if (vertexcolors)
			loadmodel->surfmesh.data_lightmapcolor4f = (float *)data, data += sizeof(float[4]) * loadmodel->surfmesh.num_vertices;
		if (lightmapoffsets)
			loadmodel->surfmesh.data_lightmapoffsets = (int *)data, data += sizeof(int) * loadmodel->surfmesh.num_vertices;
	}
	if (loadmodel->surfmesh.num_triangles)
	{
		loadmodel->surfmesh.data_element3i = (int *)data, data += sizeof(int[3]) * loadmodel->surfmesh.num_triangles;
		if (neighbors)
			loadmodel->surfmesh.data_neighbor3i = (int *)data, data += sizeof(int[3]) * loadmodel->surfmesh.num_triangles;
		if (loadmodel->surfmesh.num_vertices <= 65536)
			loadmodel->surfmesh.data_element3s = (unsigned short *)data, data += sizeof(unsigned short[3]) * loadmodel->surfmesh.num_triangles;
	}
}

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable)
{
	shadowmesh_t *newmesh;
	unsigned char *data;
	int size;
	size = sizeof(shadowmesh_t);
	size += maxverts * sizeof(float[3]);
	if (light)
		size += maxverts * sizeof(float[11]);
	size += maxtriangles * sizeof(int[3]);
	if (maxverts <= 65536)
		size += maxtriangles * sizeof(unsigned short[3]);
	if (neighbors)
		size += maxtriangles * sizeof(int[3]);
	if (expandable)
		size += SHADOWMESHVERTEXHASH * sizeof(shadowmeshvertexhash_t *) + maxverts * sizeof(shadowmeshvertexhash_t);
	data = (unsigned char *)Mem_Alloc(mempool, size);
	newmesh = (shadowmesh_t *)data;data += sizeof(*newmesh);
	newmesh->map_diffuse = map_diffuse;
	newmesh->map_specular = map_specular;
	newmesh->map_normal = map_normal;
	newmesh->maxverts = maxverts;
	newmesh->maxtriangles = maxtriangles;
	newmesh->numverts = 0;
	newmesh->numtriangles = 0;
	memset(newmesh->sideoffsets, 0, sizeof(newmesh->sideoffsets));
	memset(newmesh->sidetotals, 0, sizeof(newmesh->sidetotals));

	newmesh->vertex3f = (float *)data;data += maxverts * sizeof(float[3]);
	if (light)
	{
		newmesh->svector3f = (float *)data;data += maxverts * sizeof(float[3]);
		newmesh->tvector3f = (float *)data;data += maxverts * sizeof(float[3]);
		newmesh->normal3f = (float *)data;data += maxverts * sizeof(float[3]);
		newmesh->texcoord2f = (float *)data;data += maxverts * sizeof(float[2]);
	}
	newmesh->element3i = (int *)data;data += maxtriangles * sizeof(int[3]);
	if (neighbors)
	{
		newmesh->neighbor3i = (int *)data;data += maxtriangles * sizeof(int[3]);
	}
	if (expandable)
	{
		newmesh->vertexhashtable = (shadowmeshvertexhash_t **)data;data += SHADOWMESHVERTEXHASH * sizeof(shadowmeshvertexhash_t *);
		newmesh->vertexhashentries = (shadowmeshvertexhash_t *)data;data += maxverts * sizeof(shadowmeshvertexhash_t);
	}
	if (maxverts <= 65536)
		newmesh->element3s = (unsigned short *)data;data += maxtriangles * sizeof(unsigned short[3]);
	return newmesh;
}

shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh, int light, int neighbors)
{
	shadowmesh_t *newmesh;
	newmesh = Mod_ShadowMesh_Alloc(mempool, oldmesh->numverts, oldmesh->numtriangles, oldmesh->map_diffuse, oldmesh->map_specular, oldmesh->map_normal, light, neighbors, false);
	newmesh->numverts = oldmesh->numverts;
	newmesh->numtriangles = oldmesh->numtriangles;
	memcpy(newmesh->sideoffsets, oldmesh->sideoffsets, sizeof(oldmesh->sideoffsets));
	memcpy(newmesh->sidetotals, oldmesh->sidetotals, sizeof(oldmesh->sidetotals));

	memcpy(newmesh->vertex3f, oldmesh->vertex3f, oldmesh->numverts * sizeof(float[3]));
	if (newmesh->svector3f && oldmesh->svector3f)
	{
		memcpy(newmesh->svector3f, oldmesh->svector3f, oldmesh->numverts * sizeof(float[3]));
		memcpy(newmesh->tvector3f, oldmesh->tvector3f, oldmesh->numverts * sizeof(float[3]));
		memcpy(newmesh->normal3f, oldmesh->normal3f, oldmesh->numverts * sizeof(float[3]));
		memcpy(newmesh->texcoord2f, oldmesh->texcoord2f, oldmesh->numverts * sizeof(float[2]));
	}
	memcpy(newmesh->element3i, oldmesh->element3i, oldmesh->numtriangles * sizeof(int[3]));
	if (newmesh->neighbor3i && oldmesh->neighbor3i)
		memcpy(newmesh->neighbor3i, oldmesh->neighbor3i, oldmesh->numtriangles * sizeof(int[3]));
	return newmesh;
}

int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *vertex14f)
{
	int hashindex, vnum;
	shadowmeshvertexhash_t *hash;
	// this uses prime numbers intentionally
	hashindex = (unsigned int) (vertex14f[0] * 2003 + vertex14f[1] * 4001 + vertex14f[2] * 7919) % SHADOWMESHVERTEXHASH;
	for (hash = mesh->vertexhashtable[hashindex];hash;hash = hash->next)
	{
		vnum = (hash - mesh->vertexhashentries);
		if ((mesh->vertex3f == NULL || (mesh->vertex3f[vnum * 3 + 0] == vertex14f[0] && mesh->vertex3f[vnum * 3 + 1] == vertex14f[1] && mesh->vertex3f[vnum * 3 + 2] == vertex14f[2]))
		 && (mesh->svector3f == NULL || (mesh->svector3f[vnum * 3 + 0] == vertex14f[3] && mesh->svector3f[vnum * 3 + 1] == vertex14f[4] && mesh->svector3f[vnum * 3 + 2] == vertex14f[5]))
		 && (mesh->tvector3f == NULL || (mesh->tvector3f[vnum * 3 + 0] == vertex14f[6] && mesh->tvector3f[vnum * 3 + 1] == vertex14f[7] && mesh->tvector3f[vnum * 3 + 2] == vertex14f[8]))
		 && (mesh->normal3f == NULL || (mesh->normal3f[vnum * 3 + 0] == vertex14f[9] && mesh->normal3f[vnum * 3 + 1] == vertex14f[10] && mesh->normal3f[vnum * 3 + 2] == vertex14f[11]))
		 && (mesh->texcoord2f == NULL || (mesh->texcoord2f[vnum * 2 + 0] == vertex14f[12] && mesh->texcoord2f[vnum * 2 + 1] == vertex14f[13])))
			return hash - mesh->vertexhashentries;
	}
	vnum = mesh->numverts++;
	hash = mesh->vertexhashentries + vnum;
	hash->next = mesh->vertexhashtable[hashindex];
	mesh->vertexhashtable[hashindex] = hash;
	if (mesh->vertex3f) {mesh->vertex3f[vnum * 3 + 0] = vertex14f[0];mesh->vertex3f[vnum * 3 + 1] = vertex14f[1];mesh->vertex3f[vnum * 3 + 2] = vertex14f[2];}
	if (mesh->svector3f) {mesh->svector3f[vnum * 3 + 0] = vertex14f[3];mesh->svector3f[vnum * 3 + 1] = vertex14f[4];mesh->svector3f[vnum * 3 + 2] = vertex14f[5];}
	if (mesh->tvector3f) {mesh->tvector3f[vnum * 3 + 0] = vertex14f[6];mesh->tvector3f[vnum * 3 + 1] = vertex14f[7];mesh->tvector3f[vnum * 3 + 2] = vertex14f[8];}
	if (mesh->normal3f) {mesh->normal3f[vnum * 3 + 0] = vertex14f[9];mesh->normal3f[vnum * 3 + 1] = vertex14f[10];mesh->normal3f[vnum * 3 + 2] = vertex14f[11];}
	if (mesh->texcoord2f) {mesh->texcoord2f[vnum * 2 + 0] = vertex14f[12];mesh->texcoord2f[vnum * 2 + 1] = vertex14f[13];}
	return vnum;
}

void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, float *vertex14f)
{
	if (mesh->numtriangles == 0)
	{
		// set the properties on this empty mesh to be more favorable...
		// (note: this case only occurs for the first triangle added to a new mesh chain)
		mesh->map_diffuse = map_diffuse;
		mesh->map_specular = map_specular;
		mesh->map_normal = map_normal;
	}
	while (mesh->map_diffuse != map_diffuse || mesh->map_specular != map_specular || mesh->map_normal != map_normal || mesh->numverts + 3 > mesh->maxverts || mesh->numtriangles + 1 > mesh->maxtriangles)
	{
		if (mesh->next == NULL)
			mesh->next = Mod_ShadowMesh_Alloc(mempool, max(mesh->maxverts, 300), max(mesh->maxtriangles, 100), map_diffuse, map_specular, map_normal, mesh->svector3f != NULL, mesh->neighbor3i != NULL, true);
		mesh = mesh->next;
	}
	mesh->element3i[mesh->numtriangles * 3 + 0] = Mod_ShadowMesh_AddVertex(mesh, vertex14f + 14 * 0);
	mesh->element3i[mesh->numtriangles * 3 + 1] = Mod_ShadowMesh_AddVertex(mesh, vertex14f + 14 * 1);
	mesh->element3i[mesh->numtriangles * 3 + 2] = Mod_ShadowMesh_AddVertex(mesh, vertex14f + 14 * 2);
	mesh->numtriangles++;
}

void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, int numtris, const int *element3i)
{
	int i, j, e;
	float vbuf[3*14], *v;
	memset(vbuf, 0, sizeof(vbuf));
	for (i = 0;i < numtris;i++)
	{
		for (j = 0, v = vbuf;j < 3;j++, v += 14)
		{
			e = *element3i++;
			if (vertex3f)
			{
				v[0] = vertex3f[e * 3 + 0];
				v[1] = vertex3f[e * 3 + 1];
				v[2] = vertex3f[e * 3 + 2];
			}
			if (svector3f)
			{
				v[3] = svector3f[e * 3 + 0];
				v[4] = svector3f[e * 3 + 1];
				v[5] = svector3f[e * 3 + 2];
			}
			if (tvector3f)
			{
				v[6] = tvector3f[e * 3 + 0];
				v[7] = tvector3f[e * 3 + 1];
				v[8] = tvector3f[e * 3 + 2];
			}
			if (normal3f)
			{
				v[9] = normal3f[e * 3 + 0];
				v[10] = normal3f[e * 3 + 1];
				v[11] = normal3f[e * 3 + 2];
			}
			if (texcoord2f)
			{
				v[12] = texcoord2f[e * 2 + 0];
				v[13] = texcoord2f[e * 2 + 1];
			}
		}
		Mod_ShadowMesh_AddTriangle(mempool, mesh, map_diffuse, map_specular, map_normal, vbuf);
	}

	// the triangle calculation can take a while, so let's do a keepalive here
	CL_KeepaliveMessage(false);
}

shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable)
{
	// the preparation before shadow mesh initialization can take a while, so let's do a keepalive here
	CL_KeepaliveMessage(false);

	return Mod_ShadowMesh_Alloc(mempool, maxverts, maxtriangles, map_diffuse, map_specular, map_normal, light, neighbors, expandable);
}

static void Mod_ShadowMesh_CreateVBOs(shadowmesh_t *mesh, mempool_t *mempool)
{
	if (!mesh->numverts)
		return;

	// build r_vertexmesh_t array
	// (compressed interleaved array for D3D)
	if (!mesh->vertexmesh && mesh->texcoord2f && vid.useinterleavedarrays)
	{
		int vertexindex;
		int numvertices = mesh->numverts;
		r_vertexmesh_t *vertexmesh;
		mesh->vertexmesh = vertexmesh = (r_vertexmesh_t*)Mem_Alloc(mempool, numvertices * sizeof(*mesh->vertexmesh));
		for (vertexindex = 0;vertexindex < numvertices;vertexindex++, vertexmesh++)
		{
			VectorCopy(mesh->vertex3f + 3*vertexindex, vertexmesh->vertex3f);
			VectorScale(mesh->svector3f + 3*vertexindex, 1.0f, vertexmesh->svector3f);
			VectorScale(mesh->tvector3f + 3*vertexindex, 1.0f, vertexmesh->tvector3f);
			VectorScale(mesh->normal3f + 3*vertexindex, 1.0f, vertexmesh->normal3f);
			Vector2Copy(mesh->texcoord2f + 2*vertexindex, vertexmesh->texcoordtexture2f);
		}
	}

	// upload r_vertexmesh_t array as a buffer
	if (mesh->vertexmesh && !mesh->vertexmeshbuffer)
		mesh->vertexmeshbuffer = R_Mesh_CreateMeshBuffer(mesh->vertexmesh, mesh->numverts * sizeof(*mesh->vertexmesh), loadmodel->name, false, false, false);

	// upload vertex3f array as a buffer
	if (mesh->vertex3f && !mesh->vertex3fbuffer)
		mesh->vertex3fbuffer = R_Mesh_CreateMeshBuffer(mesh->vertex3f, mesh->numverts * sizeof(float[3]), loadmodel->name, false, false, false);

	// upload short indices as a buffer
	if (mesh->element3s && !mesh->element3s_indexbuffer)
		mesh->element3s_indexbuffer = R_Mesh_CreateMeshBuffer(mesh->element3s, mesh->numtriangles * sizeof(short[3]), loadmodel->name, true, false, true);

	// upload int indices as a buffer
	if (mesh->element3i && !mesh->element3i_indexbuffer && !mesh->element3s)
		mesh->element3i_indexbuffer = R_Mesh_CreateMeshBuffer(mesh->element3i, mesh->numtriangles * sizeof(int[3]), loadmodel->name, true, false, false);

	// vertex buffer is several arrays and we put them in the same buffer
	//
	// is this wise?  the texcoordtexture2f array is used with dynamic
	// vertex/svector/tvector/normal when rendering animated models, on the
	// other hand animated models don't use a lot of vertices anyway...
	if (!mesh->vbo_vertexbuffer && !vid.useinterleavedarrays)
	{
		size_t size;
		unsigned char *mem;
		size = 0;
		mesh->vbooffset_vertex3f           = size;if (mesh->vertex3f          ) size += mesh->numverts * sizeof(float[3]);
		mesh->vbooffset_svector3f          = size;if (mesh->svector3f         ) size += mesh->numverts * sizeof(float[3]);
		mesh->vbooffset_tvector3f          = size;if (mesh->tvector3f         ) size += mesh->numverts * sizeof(float[3]);
		mesh->vbooffset_normal3f           = size;if (mesh->normal3f          ) size += mesh->numverts * sizeof(float[3]);
		mesh->vbooffset_texcoord2f         = size;if (mesh->texcoord2f        ) size += mesh->numverts * sizeof(float[2]);
		mem = (unsigned char *)Mem_Alloc(tempmempool, size);
		if (mesh->vertex3f          ) memcpy(mem + mesh->vbooffset_vertex3f          , mesh->vertex3f          , mesh->numverts * sizeof(float[3]));
		if (mesh->svector3f         ) memcpy(mem + mesh->vbooffset_svector3f         , mesh->svector3f         , mesh->numverts * sizeof(float[3]));
		if (mesh->tvector3f         ) memcpy(mem + mesh->vbooffset_tvector3f         , mesh->tvector3f         , mesh->numverts * sizeof(float[3]));
		if (mesh->normal3f          ) memcpy(mem + mesh->vbooffset_normal3f          , mesh->normal3f          , mesh->numverts * sizeof(float[3]));
		if (mesh->texcoord2f        ) memcpy(mem + mesh->vbooffset_texcoord2f        , mesh->texcoord2f        , mesh->numverts * sizeof(float[2]));
		mesh->vbo_vertexbuffer = R_Mesh_CreateMeshBuffer(mem, size, "shadowmesh", false, false, false);
		Mem_Free(mem);
	}
}

shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh, qboolean light, qboolean neighbors, qboolean createvbo)
{
	shadowmesh_t *mesh, *newmesh, *nextmesh;
	// reallocate meshs to conserve space
	for (mesh = firstmesh, firstmesh = NULL;mesh;mesh = nextmesh)
	{
		nextmesh = mesh->next;
		if (mesh->numverts >= 3 && mesh->numtriangles >= 1)
		{
			newmesh = Mod_ShadowMesh_ReAlloc(mempool, mesh, light, neighbors);
			newmesh->next = firstmesh;
			firstmesh = newmesh;
			if (newmesh->element3s)
			{
				int i;
				for (i = 0;i < newmesh->numtriangles*3;i++)
					newmesh->element3s[i] = newmesh->element3i[i];
			}
			if (createvbo)
				Mod_ShadowMesh_CreateVBOs(newmesh, mempool);
		}
		Mem_Free(mesh);
	}

	// this can take a while, so let's do a keepalive here
	CL_KeepaliveMessage(false);

	return firstmesh;
}

void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius)
{
	int i;
	shadowmesh_t *mesh;
	vec3_t nmins, nmaxs, ncenter, temp;
	float nradius2, dist2, *v;
	VectorClear(nmins);
	VectorClear(nmaxs);
	// calculate bbox
	for (mesh = firstmesh;mesh;mesh = mesh->next)
	{
		if (mesh == firstmesh)
		{
			VectorCopy(mesh->vertex3f, nmins);
			VectorCopy(mesh->vertex3f, nmaxs);
		}
		for (i = 0, v = mesh->vertex3f;i < mesh->numverts;i++, v += 3)
		{
			if (nmins[0] > v[0]) nmins[0] = v[0];if (nmaxs[0] < v[0]) nmaxs[0] = v[0];
			if (nmins[1] > v[1]) nmins[1] = v[1];if (nmaxs[1] < v[1]) nmaxs[1] = v[1];
			if (nmins[2] > v[2]) nmins[2] = v[2];if (nmaxs[2] < v[2]) nmaxs[2] = v[2];
		}
	}
	// calculate center and radius
	ncenter[0] = (nmins[0] + nmaxs[0]) * 0.5f;
	ncenter[1] = (nmins[1] + nmaxs[1]) * 0.5f;
	ncenter[2] = (nmins[2] + nmaxs[2]) * 0.5f;
	nradius2 = 0;
	for (mesh = firstmesh;mesh;mesh = mesh->next)
	{
		for (i = 0, v = mesh->vertex3f;i < mesh->numverts;i++, v += 3)
		{
			VectorSubtract(v, ncenter, temp);
			dist2 = DotProduct(temp, temp);
			if (nradius2 < dist2)
				nradius2 = dist2;
		}
	}
	// return data
	if (mins)
		VectorCopy(nmins, mins);
	if (maxs)
		VectorCopy(nmaxs, maxs);
	if (center)
		VectorCopy(ncenter, center);
	if (radius)
		*radius = sqrt(nradius2);
}

void Mod_ShadowMesh_Free(shadowmesh_t *mesh)
{
	shadowmesh_t *nextmesh;
	for (;mesh;mesh = nextmesh)
	{
		if (mesh->vertex3fbuffer)
			R_Mesh_DestroyMeshBuffer(mesh->vertex3fbuffer);
		if (mesh->vertexmeshbuffer)
			R_Mesh_DestroyMeshBuffer(mesh->vertexmeshbuffer);
		if (mesh->element3i_indexbuffer)
			R_Mesh_DestroyMeshBuffer(mesh->element3i_indexbuffer);
		if (mesh->element3s_indexbuffer)
			R_Mesh_DestroyMeshBuffer(mesh->element3s_indexbuffer);
		if (mesh->vbo_vertexbuffer)
			R_Mesh_DestroyMeshBuffer(mesh->vbo_vertexbuffer);
		nextmesh = mesh->next;
		Mem_Free(mesh);
	}
}

void Mod_CreateCollisionMesh(dp_model_t *mod)
{
	int k, numcollisionmeshtriangles;
	qboolean usesinglecollisionmesh = false;
	const msurface_t *surface = NULL;

	mempool_t *mempool = mod->mempool;
	if (!mempool && mod->brush.parentmodel)
		mempool = mod->brush.parentmodel->mempool;
	// make a single combined collision mesh for physics engine use
	// TODO rewrite this to use the collision brushes as source, to fix issues with e.g. common/caulk which creates no drawsurface
	numcollisionmeshtriangles = 0;
	for (k = 0;k < mod->nummodelsurfaces;k++)
	{
		surface = mod->data_surfaces + mod->firstmodelsurface + k;
		if (!strcmp(surface->texture->name, "collision")) // found collision mesh
		{
			usesinglecollisionmesh = true;
			numcollisionmeshtriangles = surface->num_triangles;
			break;
		}
		if (!(surface->texture->supercontents & SUPERCONTENTS_SOLID))
			continue;
		numcollisionmeshtriangles += surface->num_triangles;
	}
	mod->brush.collisionmesh = Mod_ShadowMesh_Begin(mempool, numcollisionmeshtriangles * 3, numcollisionmeshtriangles, NULL, NULL, NULL, false, false, true);
	if (usesinglecollisionmesh)
		Mod_ShadowMesh_AddMesh(mempool, mod->brush.collisionmesh, NULL, NULL, NULL, mod->surfmesh.data_vertex3f, NULL, NULL, NULL, NULL, surface->num_triangles, (mod->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
	else
	{
		for (k = 0;k < mod->nummodelsurfaces;k++)
		{
			surface = mod->data_surfaces + mod->firstmodelsurface + k;
			if (!(surface->texture->supercontents & SUPERCONTENTS_SOLID))
				continue;
			Mod_ShadowMesh_AddMesh(mempool, mod->brush.collisionmesh, NULL, NULL, NULL, mod->surfmesh.data_vertex3f, NULL, NULL, NULL, NULL, surface->num_triangles, (mod->surfmesh.data_element3i + 3 * surface->num_firsttriangle));
		}
	}
	mod->brush.collisionmesh = Mod_ShadowMesh_Finish(mempool, mod->brush.collisionmesh, false, false, false);
}

void Mod_GetTerrainVertex3fTexCoord2fFromBGRA(const unsigned char *imagepixels, int imagewidth, int imageheight, int ix, int iy, float *vertex3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
{
	float v[3], tc[3];
	v[0] = ix;
	v[1] = iy;
	if (ix >= 0 && iy >= 0 && ix < imagewidth && iy < imageheight)
		v[2] = (imagepixels[((iy*imagewidth)+ix)*4+0] + imagepixels[((iy*imagewidth)+ix)*4+1] + imagepixels[((iy*imagewidth)+ix)*4+2]) * (1.0f / 765.0f);
	else
		v[2] = 0;
	Matrix4x4_Transform(pixelstepmatrix, v, vertex3f);
	Matrix4x4_Transform(pixeltexturestepmatrix, v, tc);
	texcoord2f[0] = tc[0];
	texcoord2f[1] = tc[1];
}

void Mod_GetTerrainVertexFromBGRA(const unsigned char *imagepixels, int imagewidth, int imageheight, int ix, int iy, float *vertex3f, float *svector3f, float *tvector3f, float *normal3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
{
	float vup[3], vdown[3], vleft[3], vright[3];
	float tcup[3], tcdown[3], tcleft[3], tcright[3];
	float sv[3], tv[3], nl[3];
	Mod_GetTerrainVertex3fTexCoord2fFromBGRA(imagepixels, imagewidth, imageheight, ix, iy, vertex3f, texcoord2f, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromBGRA(imagepixels, imagewidth, imageheight, ix, iy - 1, vup, tcup, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromBGRA(imagepixels, imagewidth, imageheight, ix, iy + 1, vdown, tcdown, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromBGRA(imagepixels, imagewidth, imageheight, ix - 1, iy, vleft, tcleft, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromBGRA(imagepixels, imagewidth, imageheight, ix + 1, iy, vright, tcright, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_BuildBumpVectors(vertex3f, vup, vright, texcoord2f, tcup, tcright, svector3f, tvector3f, normal3f);
	Mod_BuildBumpVectors(vertex3f, vright, vdown, texcoord2f, tcright, tcdown, sv, tv, nl);
	VectorAdd(svector3f, sv, svector3f);
	VectorAdd(tvector3f, tv, tvector3f);
	VectorAdd(normal3f, nl, normal3f);
	Mod_BuildBumpVectors(vertex3f, vdown, vleft, texcoord2f, tcdown, tcleft, sv, tv, nl);
	VectorAdd(svector3f, sv, svector3f);
	VectorAdd(tvector3f, tv, tvector3f);
	VectorAdd(normal3f, nl, normal3f);
	Mod_BuildBumpVectors(vertex3f, vleft, vup, texcoord2f, tcleft, tcup, sv, tv, nl);
	VectorAdd(svector3f, sv, svector3f);
	VectorAdd(tvector3f, tv, tvector3f);
	VectorAdd(normal3f, nl, normal3f);
}

void Mod_ConstructTerrainPatchFromBGRA(const unsigned char *imagepixels, int imagewidth, int imageheight, int x1, int y1, int width, int height, int *element3i, int *neighbor3i, float *vertex3f, float *svector3f, float *tvector3f, float *normal3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
{
	int x, y, ix, iy, *e;
	e = element3i;
	for (y = 0;y < height;y++)
	{
		for (x = 0;x < width;x++)
		{
			e[0] = (y + 1) * (width + 1) + (x + 0);
			e[1] = (y + 0) * (width + 1) + (x + 0);
			e[2] = (y + 1) * (width + 1) + (x + 1);
			e[3] = (y + 0) * (width + 1) + (x + 0);
			e[4] = (y + 0) * (width + 1) + (x + 1);
			e[5] = (y + 1) * (width + 1) + (x + 1);
			e += 6;
		}
	}
	Mod_BuildTriangleNeighbors(neighbor3i, element3i, width*height*2);
	for (y = 0, iy = y1;y < height + 1;y++, iy++)
		for (x = 0, ix = x1;x < width + 1;x++, ix++, vertex3f += 3, texcoord2f += 2, svector3f += 3, tvector3f += 3, normal3f += 3)
			Mod_GetTerrainVertexFromBGRA(imagepixels, imagewidth, imageheight, ix, iy, vertex3f, texcoord2f, svector3f, tvector3f, normal3f, pixelstepmatrix, pixeltexturestepmatrix);
}

#if 0
void Mod_Terrain_SurfaceRecurseChunk(dp_model_t *model, int stepsize, int x, int y)
{
	float mins[3];
	float maxs[3];
	float chunkwidth = min(stepsize, model->terrain.width - 1 - x);
	float chunkheight = min(stepsize, model->terrain.height - 1 - y);
	float viewvector[3];
	unsigned int firstvertex;
	unsigned int *e;
	float *v;
	if (chunkwidth < 2 || chunkheight < 2)
		return;
	VectorSet(mins, model->terrain.mins[0] +  x    * stepsize * model->terrain.scale[0], model->terrain.mins[1] +  y    * stepsize * model->terrain.scale[1], model->terrain.mins[2]);
	VectorSet(maxs, model->terrain.mins[0] + (x+1) * stepsize * model->terrain.scale[0], model->terrain.mins[1] + (y+1) * stepsize * model->terrain.scale[1], model->terrain.maxs[2]);
	viewvector[0] = bound(mins[0], localvieworigin, maxs[0]) - model->terrain.vieworigin[0];
	viewvector[1] = bound(mins[1], localvieworigin, maxs[1]) - model->terrain.vieworigin[1];
	viewvector[2] = bound(mins[2], localvieworigin, maxs[2]) - model->terrain.vieworigin[2];
	if (stepsize > 1 && VectorLength(viewvector) < stepsize*model->terrain.scale[0]*r_terrain_lodscale.value)
	{
		// too close for this stepsize, emit as 4 chunks instead
		stepsize /= 2;
		Mod_Terrain_SurfaceRecurseChunk(model, stepsize, x, y);
		Mod_Terrain_SurfaceRecurseChunk(model, stepsize, x+stepsize, y);
		Mod_Terrain_SurfaceRecurseChunk(model, stepsize, x, y+stepsize);
		Mod_Terrain_SurfaceRecurseChunk(model, stepsize, x+stepsize, y+stepsize);
		return;
	}
	// emit the geometry at stepsize into our vertex buffer / index buffer
	// we add two columns and two rows for skirt
	outwidth = chunkwidth+2;
	outheight = chunkheight+2;
	outwidth2 = outwidth-1;
	outheight2 = outheight-1;
	outwidth3 = outwidth+1;
	outheight3 = outheight+1;
	firstvertex = numvertices;
	e = model->terrain.element3i + numtriangles;
	numtriangles += chunkwidth*chunkheight*2+chunkwidth*2*2+chunkheight*2*2;
	v = model->terrain.vertex3f + numvertices;
	numvertices += (chunkwidth+1)*(chunkheight+1)+(chunkwidth+1)*2+(chunkheight+1)*2;
	// emit the triangles (note: the skirt is treated as two extra rows and two extra columns)
	for (ty = 0;ty < outheight;ty++)
	{
		for (tx = 0;tx < outwidth;tx++)
		{
			*e++ = firstvertex + (ty  )*outwidth3+(tx  );
			*e++ = firstvertex + (ty  )*outwidth3+(tx+1);
			*e++ = firstvertex + (ty+1)*outwidth3+(tx+1);
			*e++ = firstvertex + (ty  )*outwidth3+(tx  );
			*e++ = firstvertex + (ty+1)*outwidth3+(tx+1);
			*e++ = firstvertex + (ty+1)*outwidth3+(tx  );
		}
	}
	// TODO: emit surface vertices (x+tx*stepsize, y+ty*stepsize)
	for (ty = 0;ty <= outheight;ty++)
	{
		skirtrow = ty == 0 || ty == outheight;
		ry = y+bound(1, ty, outheight)*stepsize;
		for (tx = 0;tx <= outwidth;tx++)
		{
			skirt = skirtrow || tx == 0 || tx == outwidth;
			rx = x+bound(1, tx, outwidth)*stepsize;
			v[0] = rx*scale[0];
			v[1] = ry*scale[1];
			v[2] = heightmap[ry*terrainwidth+rx]*scale[2];
			v += 3;
		}
	}
	// TODO: emit skirt vertices
}

void Mod_Terrain_UpdateSurfacesForViewOrigin(dp_model_t *model)
{
	for (y = 0;y < model->terrain.size[1];y += model->terrain.
	Mod_Terrain_SurfaceRecurseChunk(model, model->terrain.maxstepsize, x, y);
	Mod_Terrain_BuildChunk(model, 
}
#endif

int Mod_LoadQ3Shaders_EnumerateWaveFunc(const char *s)
{
	int offset = 0;
	if (!strncasecmp(s, "user", 4)) // parse stuff like "user1sin", always user<n>func
	{
		offset = bound(0, s[4] - '0', 9);
		offset = (offset + 1) << Q3WAVEFUNC_USER_SHIFT;
		s += 4;
		if(*s)
			++s;
	}
	if (!strcasecmp(s, "sin"))             return offset | Q3WAVEFUNC_SIN;
	if (!strcasecmp(s, "square"))          return offset | Q3WAVEFUNC_SQUARE;
	if (!strcasecmp(s, "triangle"))        return offset | Q3WAVEFUNC_TRIANGLE;
	if (!strcasecmp(s, "sawtooth"))        return offset | Q3WAVEFUNC_SAWTOOTH;
	if (!strcasecmp(s, "inversesawtooth")) return offset | Q3WAVEFUNC_INVERSESAWTOOTH;
	if (!strcasecmp(s, "noise"))           return offset | Q3WAVEFUNC_NOISE;
	if (!strcasecmp(s, "none"))            return offset | Q3WAVEFUNC_NONE;
	Con_DPrintf("Mod_LoadQ3Shaders: unknown wavefunc %s\n", s);
	return offset | Q3WAVEFUNC_NONE;
}

void Mod_FreeQ3Shaders(void)
{
	Mem_FreePool(&q3shaders_mem);
}

static void Q3Shader_AddToHash (q3shaderinfo_t* shader)
{
	unsigned short hash = CRC_Block_CaseInsensitive ((const unsigned char *)shader->name, strlen (shader->name));
	q3shader_hash_entry_t* entry = q3shader_data->hash + (hash % Q3SHADER_HASH_SIZE);
	q3shader_hash_entry_t* lastEntry = NULL;
	while (entry != NULL)
	{
		if (strcasecmp (entry->shader.name, shader->name) == 0)
		{
			unsigned char *start, *end, *start2;
			start = (unsigned char *) (&shader->Q3SHADERINFO_COMPARE_START);
			end = ((unsigned char *) (&shader->Q3SHADERINFO_COMPARE_END)) + sizeof(shader->Q3SHADERINFO_COMPARE_END);
			start2 = (unsigned char *) (&entry->shader.Q3SHADERINFO_COMPARE_START);
			if(memcmp(start, start2, end - start))
				Con_DPrintf("Shader '%s' already defined, ignoring mismatching redeclaration\n", shader->name);
			else
				Con_DPrintf("Shader '%s' already defined\n", shader->name);
			return;
		}
		lastEntry = entry;
		entry = entry->chain;
	}
	if (entry == NULL)
	{
		if (lastEntry->shader.name[0] != 0)
		{
			/* Add to chain */
			q3shader_hash_entry_t* newEntry = (q3shader_hash_entry_t*)
			  Mem_ExpandableArray_AllocRecord (&q3shader_data->hash_entries);

			while (lastEntry->chain != NULL) lastEntry = lastEntry->chain;
			lastEntry->chain = newEntry;
			newEntry->chain = NULL;
			lastEntry = newEntry;
		}
		/* else: head of chain, in hash entry array */
		entry = lastEntry;
	}
	memcpy (&entry->shader, shader, sizeof (q3shaderinfo_t));
}

extern cvar_t mod_noshader_default_offsetmapping;
extern cvar_t mod_q3shader_default_offsetmapping;
extern cvar_t mod_q3shader_default_polygonoffset;
extern cvar_t mod_q3shader_default_polygonfactor;
void Mod_LoadQ3Shaders(void)
{
	int j;
	int fileindex;
	fssearch_t *search;
	char *f;
	const char *text;
	q3shaderinfo_t shader;
	q3shaderinfo_layer_t *layer;
	int numparameters;
	char parameter[TEXTURE_MAXFRAMES + 4][Q3PATHLENGTH];
	char *custsurfaceparmnames[256]; // VorteX: q3map2 has 64 but well, someone will need more
	unsigned long custsurfaceparms[256]; 
	int numcustsurfaceparms;

	Mod_FreeQ3Shaders();

	q3shaders_mem = Mem_AllocPool("q3shaders", 0, NULL);
	q3shader_data = (q3shader_data_t*)Mem_Alloc (q3shaders_mem,
		sizeof (q3shader_data_t));
	Mem_ExpandableArray_NewArray (&q3shader_data->hash_entries,
		q3shaders_mem, sizeof (q3shader_hash_entry_t), 256);
	Mem_ExpandableArray_NewArray (&q3shader_data->char_ptrs,
		q3shaders_mem, sizeof (char**), 256);

	// parse custinfoparms.txt
	numcustsurfaceparms = 0;
	if ((text = f = (char *)FS_LoadFile("scripts/custinfoparms.txt", tempmempool, false, NULL)) != NULL)
	{
		if (!COM_ParseToken_QuakeC(&text, false) || strcasecmp(com_token, "{"))
			Con_DPrintf("scripts/custinfoparms.txt: contentflags section parsing error - expected \"{\", found \"%s\"\n", com_token);
		else
		{
			while (COM_ParseToken_QuakeC(&text, false))
				if (!strcasecmp(com_token, "}"))
					break;
			// custom surfaceflags section
			if (!COM_ParseToken_QuakeC(&text, false) || strcasecmp(com_token, "{"))
				Con_DPrintf("scripts/custinfoparms.txt: surfaceflags section parsing error - expected \"{\", found \"%s\"\n", com_token);
			else
			{
				while(COM_ParseToken_QuakeC(&text, false))
				{
					if (!strcasecmp(com_token, "}"))
						break;	
					// register surfaceflag
					if (numcustsurfaceparms >= 256)
					{
						Con_Printf("scripts/custinfoparms.txt: surfaceflags section parsing error - max 256 surfaceflags exceeded\n");
						break;
					}
					// name
					j = strlen(com_token)+1;
					custsurfaceparmnames[numcustsurfaceparms] = (char *)Mem_Alloc(tempmempool, j);
					strlcpy(custsurfaceparmnames[numcustsurfaceparms], com_token, j+1);
					// value
					if (COM_ParseToken_QuakeC(&text, false))
						custsurfaceparms[numcustsurfaceparms] = strtol(com_token, NULL, 0);
					else
						custsurfaceparms[numcustsurfaceparms] = 0;
					numcustsurfaceparms++;
				}
			}
		}
		Mem_Free(f);
	}

	// parse shaders
	search = FS_Search("scripts/*.shader", true, false);
	if (!search)
		return;
	for (fileindex = 0;fileindex < search->numfilenames;fileindex++)
	{
		text = f = (char *)FS_LoadFile(search->filenames[fileindex], tempmempool, false, NULL);
		if (!f)
			continue;
		while (COM_ParseToken_QuakeC(&text, false))
		{
			memset (&shader, 0, sizeof(shader));
			shader.reflectmin = 0;
			shader.reflectmax = 1;
			shader.refractfactor = 1;
			Vector4Set(shader.refractcolor4f, 1, 1, 1, 1);
			shader.reflectfactor = 1;
			Vector4Set(shader.reflectcolor4f, 1, 1, 1, 1);
			shader.r_water_wateralpha = 1;
			shader.offsetmapping = (mod_q3shader_default_offsetmapping.value) ? OFFSETMAPPING_DEFAULT : OFFSETMAPPING_OFF;
			shader.offsetscale = 1;
			shader.specularscalemod = 1;
			shader.specularpowermod = 1;
			shader.biaspolygonoffset = mod_q3shader_default_polygonoffset.value;
			shader.biaspolygonfactor = mod_q3shader_default_polygonfactor.value;

			strlcpy(shader.name, com_token, sizeof(shader.name));
			if (!COM_ParseToken_QuakeC(&text, false) || strcasecmp(com_token, "{"))
			{
				Con_DPrintf("%s parsing error - expected \"{\", found \"%s\"\n", search->filenames[fileindex], com_token);
				break;
			}
			while (COM_ParseToken_QuakeC(&text, false))
			{
				if (!strcasecmp(com_token, "}"))
					break;
				if (!strcasecmp(com_token, "{"))
				{
					static q3shaderinfo_layer_t dummy;
					if (shader.numlayers < Q3SHADER_MAXLAYERS)
					{
						layer = shader.layers + shader.numlayers++;
					}
					else
					{
						// parse and process it anyway, just don't store it (so a map $lightmap or such stuff still is found)
						memset(&dummy, 0, sizeof(dummy));
						layer = &dummy;
					}
					layer->rgbgen.rgbgen = Q3RGBGEN_IDENTITY;
					layer->alphagen.alphagen = Q3ALPHAGEN_IDENTITY;
					layer->tcgen.tcgen = Q3TCGEN_TEXTURE;
					layer->blendfunc[0] = GL_ONE;
					layer->blendfunc[1] = GL_ZERO;
					while (COM_ParseToken_QuakeC(&text, false))
					{
						if (!strcasecmp(com_token, "}"))
							break;
						if (!strcasecmp(com_token, "\n"))
							continue;
						numparameters = 0;
						for (j = 0;strcasecmp(com_token, "\n") && strcasecmp(com_token, "}");j++)
						{
							if (j < TEXTURE_MAXFRAMES + 4)
							{
								// remap dp_water to dpwater, dp_reflect to dpreflect, etc.
								if(j == 0 && !strncasecmp(com_token, "dp_", 3))
									dpsnprintf(parameter[j], sizeof(parameter[j]), "dp%s", &com_token[3]);
								else
									strlcpy(parameter[j], com_token, sizeof(parameter[j]));
								numparameters = j + 1;
							}
							if (!COM_ParseToken_QuakeC(&text, true))
								break;
						}
						//for (j = numparameters;j < TEXTURE_MAXFRAMES + 4;j++)
						//	parameter[j][0] = 0;
						if (developer_insane.integer)
						{
							Con_DPrintf("%s %i: ", shader.name, shader.numlayers - 1);
							for (j = 0;j < numparameters;j++)
								Con_DPrintf(" %s", parameter[j]);
							Con_DPrint("\n");
						}
						if (numparameters >= 2 && !strcasecmp(parameter[0], "blendfunc"))
						{
							if (numparameters == 2)
							{
								if (!strcasecmp(parameter[1], "add"))
								{
									layer->blendfunc[0] = GL_ONE;
									layer->blendfunc[1] = GL_ONE;
								}
								else if (!strcasecmp(parameter[1], "filter"))
								{
									layer->blendfunc[0] = GL_DST_COLOR;
									layer->blendfunc[1] = GL_ZERO;
								}
								else if (!strcasecmp(parameter[1], "blend"))
								{
									layer->blendfunc[0] = GL_SRC_ALPHA;
									layer->blendfunc[1] = GL_ONE_MINUS_SRC_ALPHA;
								}
							}
							else if (numparameters == 3)
							{
								int k;
								for (k = 0;k < 2;k++)
								{
									if (!strcasecmp(parameter[k+1], "GL_ONE"))
										layer->blendfunc[k] = GL_ONE;
									else if (!strcasecmp(parameter[k+1], "GL_ZERO"))
										layer->blendfunc[k] = GL_ZERO;
									else if (!strcasecmp(parameter[k+1], "GL_SRC_COLOR"))
										layer->blendfunc[k] = GL_SRC_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_SRC_ALPHA"))
										layer->blendfunc[k] = GL_SRC_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_DST_COLOR"))
										layer->blendfunc[k] = GL_DST_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_DST_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_SRC_COLOR"))
										layer->blendfunc[k] = GL_ONE_MINUS_SRC_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_SRC_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_SRC_ALPHA;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_DST_COLOR"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_COLOR;
									else if (!strcasecmp(parameter[k+1], "GL_ONE_MINUS_DST_ALPHA"))
										layer->blendfunc[k] = GL_ONE_MINUS_DST_ALPHA;
									else
										layer->blendfunc[k] = GL_ONE; // default in case of parsing error
								}
							}
						}
						if (numparameters >= 2 && !strcasecmp(parameter[0], "alphafunc"))
							layer->alphatest = true;
						if (numparameters >= 2 && (!strcasecmp(parameter[0], "map") || !strcasecmp(parameter[0], "clampmap")))
						{
							if (!strcasecmp(parameter[0], "clampmap"))
								layer->clampmap = true;
							layer->numframes = 1;
							layer->framerate = 1;
							layer->texturename = (char**)Mem_ExpandableArray_AllocRecord (
								&q3shader_data->char_ptrs);
							layer->texturename[0] = Mem_strdup (q3shaders_mem, parameter[1]);
							if (!strcasecmp(parameter[1], "$lightmap"))
								shader.lighting = true;
						}
						else if (numparameters >= 3 && (!strcasecmp(parameter[0], "animmap") || !strcasecmp(parameter[0], "animclampmap")))
						{
							int i;
							layer->numframes = min(numparameters - 2, TEXTURE_MAXFRAMES);
							layer->framerate = atof(parameter[1]);
							layer->texturename = (char **) Mem_Alloc (q3shaders_mem, sizeof (char*) * layer->numframes);
							for (i = 0;i < layer->numframes;i++)
								layer->texturename[i] = Mem_strdup (q3shaders_mem, parameter[i + 2]);
						}
						else if (numparameters >= 2 && !strcasecmp(parameter[0], "rgbgen"))
						{
							int i;
							for (i = 0;i < numparameters - 2 && i < Q3RGBGEN_MAXPARMS;i++)
								layer->rgbgen.parms[i] = atof(parameter[i+2]);
							     if (!strcasecmp(parameter[1], "identity"))         layer->rgbgen.rgbgen = Q3RGBGEN_IDENTITY;
							else if (!strcasecmp(parameter[1], "const"))            layer->rgbgen.rgbgen = Q3RGBGEN_CONST;
							else if (!strcasecmp(parameter[1], "entity"))           layer->rgbgen.rgbgen = Q3RGBGEN_ENTITY;
							else if (!strcasecmp(parameter[1], "exactvertex"))      layer->rgbgen.rgbgen = Q3RGBGEN_EXACTVERTEX;
							else if (!strcasecmp(parameter[1], "identitylighting")) layer->rgbgen.rgbgen = Q3RGBGEN_IDENTITYLIGHTING;
							else if (!strcasecmp(parameter[1], "lightingdiffuse"))  layer->rgbgen.rgbgen = Q3RGBGEN_LIGHTINGDIFFUSE;
							else if (!strcasecmp(parameter[1], "oneminusentity"))   layer->rgbgen.rgbgen = Q3RGBGEN_ONEMINUSENTITY;
							else if (!strcasecmp(parameter[1], "oneminusvertex"))   layer->rgbgen.rgbgen = Q3RGBGEN_ONEMINUSVERTEX;
							else if (!strcasecmp(parameter[1], "vertex"))           layer->rgbgen.rgbgen = Q3RGBGEN_VERTEX;
							else if (!strcasecmp(parameter[1], "wave"))
							{
								layer->rgbgen.rgbgen = Q3RGBGEN_WAVE;
								layer->rgbgen.wavefunc = Mod_LoadQ3Shaders_EnumerateWaveFunc(parameter[2]);
								for (i = 0;i < numparameters - 3 && i < Q3WAVEPARMS;i++)
									layer->rgbgen.waveparms[i] = atof(parameter[i+3]);
							}
							else Con_DPrintf("%s parsing warning: unknown rgbgen %s\n", search->filenames[fileindex], parameter[1]);
						}
						else if (numparameters >= 2 && !strcasecmp(parameter[0], "alphagen"))
						{
							int i;
							for (i = 0;i < numparameters - 2 && i < Q3ALPHAGEN_MAXPARMS;i++)
								layer->alphagen.parms[i] = atof(parameter[i+2]);
							     if (!strcasecmp(parameter[1], "identity"))         layer->alphagen.alphagen = Q3ALPHAGEN_IDENTITY;
							else if (!strcasecmp(parameter[1], "const"))            layer->alphagen.alphagen = Q3ALPHAGEN_CONST;
							else if (!strcasecmp(parameter[1], "entity"))           layer->alphagen.alphagen = Q3ALPHAGEN_ENTITY;
							else if (!strcasecmp(parameter[1], "lightingspecular")) layer->alphagen.alphagen = Q3ALPHAGEN_LIGHTINGSPECULAR;
							else if (!strcasecmp(parameter[1], "oneminusentity"))   layer->alphagen.alphagen = Q3ALPHAGEN_ONEMINUSENTITY;
							else if (!strcasecmp(parameter[1], "oneminusvertex"))   layer->alphagen.alphagen = Q3ALPHAGEN_ONEMINUSVERTEX;
							else if (!strcasecmp(parameter[1], "portal"))           layer->alphagen.alphagen = Q3ALPHAGEN_PORTAL;
							else if (!strcasecmp(parameter[1], "vertex"))           layer->alphagen.alphagen = Q3ALPHAGEN_VERTEX;
							else if (!strcasecmp(parameter[1], "wave"))
							{
								layer->alphagen.alphagen = Q3ALPHAGEN_WAVE;
								layer->alphagen.wavefunc = Mod_LoadQ3Shaders_EnumerateWaveFunc(parameter[2]);
								for (i = 0;i < numparameters - 3 && i < Q3WAVEPARMS;i++)
									layer->alphagen.waveparms[i] = atof(parameter[i+3]);
							}
							else Con_DPrintf("%s parsing warning: unknown alphagen %s\n", search->filenames[fileindex], parameter[1]);
						}
						else if (numparameters >= 2 && (!strcasecmp(parameter[0], "texgen") || !strcasecmp(parameter[0], "tcgen")))
						{
							int i;
							// observed values: tcgen environment
							// no other values have been observed in real shaders
							for (i = 0;i < numparameters - 2 && i < Q3TCGEN_MAXPARMS;i++)
								layer->tcgen.parms[i] = atof(parameter[i+2]);
							     if (!strcasecmp(parameter[1], "base"))        layer->tcgen.tcgen = Q3TCGEN_TEXTURE;
							else if (!strcasecmp(parameter[1], "texture"))     layer->tcgen.tcgen = Q3TCGEN_TEXTURE;
							else if (!strcasecmp(parameter[1], "environment")) layer->tcgen.tcgen = Q3TCGEN_ENVIRONMENT;
							else if (!strcasecmp(parameter[1], "lightmap"))    layer->tcgen.tcgen = Q3TCGEN_LIGHTMAP;
							else if (!strcasecmp(parameter[1], "vector"))      layer->tcgen.tcgen = Q3TCGEN_VECTOR;
							else Con_DPrintf("%s parsing warning: unknown tcgen mode %s\n", search->filenames[fileindex], parameter[1]);
						}
						else if (numparameters >= 2 && !strcasecmp(parameter[0], "tcmod"))
						{
							int i, tcmodindex;
							// observed values:
							// tcmod rotate #
							// tcmod scale # #
							// tcmod scroll # #
							// tcmod stretch sin # # # #
							// tcmod stretch triangle # # # #
							// tcmod transform # # # # # #
							// tcmod turb # # # #
							// tcmod turb sin # # # #  (this is bogus)
							// no other values have been observed in real shaders
							for (tcmodindex = 0;tcmodindex < Q3MAXTCMODS;tcmodindex++)
								if (!layer->tcmods[tcmodindex].tcmod)
									break;
							if (tcmodindex < Q3MAXTCMODS)
							{
								for (i = 0;i < numparameters - 2 && i < Q3TCMOD_MAXPARMS;i++)
									layer->tcmods[tcmodindex].parms[i] = atof(parameter[i+2]);
									 if (!strcasecmp(parameter[1], "entitytranslate")) layer->tcmods[tcmodindex].tcmod = Q3TCMOD_ENTITYTRANSLATE;
								else if (!strcasecmp(parameter[1], "rotate"))          layer->tcmods[tcmodindex].tcmod = Q3TCMOD_ROTATE;
								else if (!strcasecmp(parameter[1], "scale"))           layer->tcmods[tcmodindex].tcmod = Q3TCMOD_SCALE;
								else if (!strcasecmp(parameter[1], "scroll"))          layer->tcmods[tcmodindex].tcmod = Q3TCMOD_SCROLL;
								else if (!strcasecmp(parameter[1], "page"))            layer->tcmods[tcmodindex].tcmod = Q3TCMOD_PAGE;
								else if (!strcasecmp(parameter[1], "stretch"))
								{
									layer->tcmods[tcmodindex].tcmod = Q3TCMOD_STRETCH;
									layer->tcmods[tcmodindex].wavefunc = Mod_LoadQ3Shaders_EnumerateWaveFunc(parameter[2]);
									for (i = 0;i < numparameters - 3 && i < Q3WAVEPARMS;i++)
										layer->tcmods[tcmodindex].waveparms[i] = atof(parameter[i+3]);
								}
								else if (!strcasecmp(parameter[1], "transform"))       layer->tcmods[tcmodindex].tcmod = Q3TCMOD_TRANSFORM;
								else if (!strcasecmp(parameter[1], "turb"))            layer->tcmods[tcmodindex].tcmod = Q3TCMOD_TURBULENT;
								else Con_DPrintf("%s parsing warning: unknown tcmod mode %s\n", search->filenames[fileindex], parameter[1]);
							}
							else
								Con_DPrintf("%s parsing warning: too many tcmods on one layer\n", search->filenames[fileindex]);
						}
						// break out a level if it was a closing brace (not using the character here to not confuse vim)
						if (!strcasecmp(com_token, "}"))
							break;
					}
					if (layer->rgbgen.rgbgen == Q3RGBGEN_LIGHTINGDIFFUSE || layer->rgbgen.rgbgen == Q3RGBGEN_VERTEX)
						shader.lighting = true;
					if (layer->alphagen.alphagen == Q3ALPHAGEN_VERTEX)
					{
						if (layer == shader.layers + 0)
						{
							// vertex controlled transparency
							shader.vertexalpha = true;
						}
						else
						{
							// multilayer terrain shader or similar
							shader.textureblendalpha = true;
						}
					}
					layer->texflags = TEXF_ALPHA;
					if (!(shader.surfaceparms & Q3SURFACEPARM_NOMIPMAPS))
						layer->texflags |= TEXF_MIPMAP;
					if (!(shader.textureflags & Q3TEXTUREFLAG_NOPICMIP))
						layer->texflags |= TEXF_PICMIP | TEXF_COMPRESS;
					if (layer->clampmap)
						layer->texflags |= TEXF_CLAMP;
					continue;
				}
				numparameters = 0;
				for (j = 0;strcasecmp(com_token, "\n") && strcasecmp(com_token, "}");j++)
				{
					if (j < TEXTURE_MAXFRAMES + 4)
					{
						// remap dp_water to dpwater, dp_reflect to dpreflect, etc.
						if(j == 0 && !strncasecmp(com_token, "dp_", 3))
							dpsnprintf(parameter[j], sizeof(parameter[j]), "dp%s", &com_token[3]);
						else
							strlcpy(parameter[j], com_token, sizeof(parameter[j]));
						numparameters = j + 1;
					}
					if (!COM_ParseToken_QuakeC(&text, true))
						break;
				}
				//for (j = numparameters;j < TEXTURE_MAXFRAMES + 4;j++)
				//	parameter[j][0] = 0;
				if (fileindex == 0 && !strcasecmp(com_token, "}"))
					break;
				if (developer_insane.integer)
				{
					Con_DPrintf("%s: ", shader.name);
					for (j = 0;j < numparameters;j++)
						Con_DPrintf(" %s", parameter[j]);
					Con_DPrint("\n");
				}
				if (numparameters < 1)
					continue;
				if (!strcasecmp(parameter[0], "surfaceparm") && numparameters >= 2)
				{
					if (!strcasecmp(parameter[1], "alphashadow"))
						shader.surfaceparms |= Q3SURFACEPARM_ALPHASHADOW;
					else if (!strcasecmp(parameter[1], "areaportal"))
						shader.surfaceparms |= Q3SURFACEPARM_AREAPORTAL;
					else if (!strcasecmp(parameter[1], "botclip"))
						shader.surfaceparms |= Q3SURFACEPARM_BOTCLIP;
					else if (!strcasecmp(parameter[1], "clusterportal"))
						shader.surfaceparms |= Q3SURFACEPARM_CLUSTERPORTAL;
					else if (!strcasecmp(parameter[1], "detail"))
						shader.surfaceparms |= Q3SURFACEPARM_DETAIL;
					else if (!strcasecmp(parameter[1], "donotenter"))
						shader.surfaceparms |= Q3SURFACEPARM_DONOTENTER;
					else if (!strcasecmp(parameter[1], "dust"))
						shader.surfaceparms |= Q3SURFACEPARM_DUST;
					else if (!strcasecmp(parameter[1], "hint"))
						shader.surfaceparms |= Q3SURFACEPARM_HINT;
					else if (!strcasecmp(parameter[1], "fog"))
						shader.surfaceparms |= Q3SURFACEPARM_FOG;
					else if (!strcasecmp(parameter[1], "lava"))
						shader.surfaceparms |= Q3SURFACEPARM_LAVA;
					else if (!strcasecmp(parameter[1], "lightfilter"))
						shader.surfaceparms |= Q3SURFACEPARM_LIGHTFILTER;
					else if (!strcasecmp(parameter[1], "lightgrid"))
						shader.surfaceparms |= Q3SURFACEPARM_LIGHTGRID;
					else if (!strcasecmp(parameter[1], "metalsteps"))
						shader.surfaceparms |= Q3SURFACEPARM_METALSTEPS;
					else if (!strcasecmp(parameter[1], "nodamage"))
						shader.surfaceparms |= Q3SURFACEPARM_NODAMAGE;
					else if (!strcasecmp(parameter[1], "nodlight"))
						shader.surfaceparms |= Q3SURFACEPARM_NODLIGHT;
					else if (!strcasecmp(parameter[1], "nodraw"))
						shader.surfaceparms |= Q3SURFACEPARM_NODRAW;
					else if (!strcasecmp(parameter[1], "nodrop"))
						shader.surfaceparms |= Q3SURFACEPARM_NODROP;
					else if (!strcasecmp(parameter[1], "noimpact"))
						shader.surfaceparms |= Q3SURFACEPARM_NOIMPACT;
					else if (!strcasecmp(parameter[1], "nolightmap"))
						shader.surfaceparms |= Q3SURFACEPARM_NOLIGHTMAP;
					else if (!strcasecmp(parameter[1], "nomarks"))
						shader.surfaceparms |= Q3SURFACEPARM_NOMARKS;
					else if (!strcasecmp(parameter[1], "nomipmaps"))
						shader.surfaceparms |= Q3SURFACEPARM_NOMIPMAPS;
					else if (!strcasecmp(parameter[1], "nonsolid"))
						shader.surfaceparms |= Q3SURFACEPARM_NONSOLID;
					else if (!strcasecmp(parameter[1], "origin"))
						shader.surfaceparms |= Q3SURFACEPARM_ORIGIN;
					else if (!strcasecmp(parameter[1], "playerclip"))
						shader.surfaceparms |= Q3SURFACEPARM_PLAYERCLIP;
					else if (!strcasecmp(parameter[1], "sky"))
						shader.surfaceparms |= Q3SURFACEPARM_SKY;
					else if (!strcasecmp(parameter[1], "slick"))
						shader.surfaceparms |= Q3SURFACEPARM_SLICK;
					else if (!strcasecmp(parameter[1], "slime"))
						shader.surfaceparms |= Q3SURFACEPARM_SLIME;
					else if (!strcasecmp(parameter[1], "structural"))
						shader.surfaceparms |= Q3SURFACEPARM_STRUCTURAL;
					else if (!strcasecmp(parameter[1], "trans"))
						shader.surfaceparms |= Q3SURFACEPARM_TRANS;
					else if (!strcasecmp(parameter[1], "water"))
						shader.surfaceparms |= Q3SURFACEPARM_WATER;
					else if (!strcasecmp(parameter[1], "pointlight"))
						shader.surfaceparms |= Q3SURFACEPARM_POINTLIGHT;
					else if (!strcasecmp(parameter[1], "antiportal"))
						shader.surfaceparms |= Q3SURFACEPARM_ANTIPORTAL;
					else
					{
						// try custom surfaceparms
						for (j = 0; j < numcustsurfaceparms; j++)
						{
							if (!strcasecmp(custsurfaceparmnames[j], parameter[1]))
							{
								shader.surfaceparms |= custsurfaceparms[j];
								break;
							}
						}
						// failed all
						if (j == numcustsurfaceparms)
							Con_DPrintf("%s parsing warning: unknown surfaceparm \"%s\"\n", search->filenames[fileindex], parameter[1]);
					}
				}
				else if (!strcasecmp(parameter[0], "dpshadow"))
					shader.dpshadow = true;
				else if (!strcasecmp(parameter[0], "dpnoshadow"))
					shader.dpnoshadow = true;
				else if (!strcasecmp(parameter[0], "dpnortlight"))
					shader.dpnortlight = true;
				else if (!strcasecmp(parameter[0], "dpreflectcube"))
					strlcpy(shader.dpreflectcube, parameter[1], sizeof(shader.dpreflectcube));
				else if (!strcasecmp(parameter[0], "dpmeshcollisions"))
					shader.dpmeshcollisions = true;
				else if (!strcasecmp(parameter[0], "sky") && numparameters >= 2)
				{
					// some q3 skies don't have the sky parm set
					shader.surfaceparms |= Q3SURFACEPARM_SKY;
					strlcpy(shader.skyboxname, parameter[1], sizeof(shader.skyboxname));
				}
				else if (!strcasecmp(parameter[0], "skyparms") && numparameters >= 2)
				{
					// some q3 skies don't have the sky parm set
					shader.surfaceparms |= Q3SURFACEPARM_SKY;
					if (!atoi(parameter[1]) && strcasecmp(parameter[1], "-"))
						strlcpy(shader.skyboxname, parameter[1], sizeof(shader.skyboxname));
				}
				else if (!strcasecmp(parameter[0], "cull") && numparameters >= 2)
				{
					if (!strcasecmp(parameter[1], "disable") || !strcasecmp(parameter[1], "none") || !strcasecmp(parameter[1], "twosided"))
						shader.textureflags |= Q3TEXTUREFLAG_TWOSIDED;
				}
				else if (!strcasecmp(parameter[0], "nomipmaps"))
					shader.surfaceparms |= Q3SURFACEPARM_NOMIPMAPS;
				else if (!strcasecmp(parameter[0], "nopicmip"))
					shader.textureflags |= Q3TEXTUREFLAG_NOPICMIP;
				else if (!strcasecmp(parameter[0], "polygonoffset"))
					shader.textureflags |= Q3TEXTUREFLAG_POLYGONOFFSET;
				else if (!strcasecmp(parameter[0], "dppolygonoffset"))
				{
					shader.textureflags |= Q3TEXTUREFLAG_POLYGONOFFSET;
					if(numparameters >= 2)
					{
						shader.biaspolygonfactor = atof(parameter[1]);
						if(numparameters >= 3)
							shader.biaspolygonoffset = atof(parameter[2]);
						else
							shader.biaspolygonoffset = 0;
					}
				}
				else if (!strcasecmp(parameter[0], "dprefract") && numparameters >= 5)
				{
					shader.textureflags |= Q3TEXTUREFLAG_REFRACTION;
					shader.refractfactor = atof(parameter[1]);
					Vector4Set(shader.refractcolor4f, atof(parameter[2]), atof(parameter[3]), atof(parameter[4]), 1);
				}
				else if (!strcasecmp(parameter[0], "dpreflect") && numparameters >= 6)
				{
					shader.textureflags |= Q3TEXTUREFLAG_REFLECTION;
					shader.reflectfactor = atof(parameter[1]);
					Vector4Set(shader.reflectcolor4f, atof(parameter[2]), atof(parameter[3]), atof(parameter[4]), atof(parameter[5]));
				}
				else if (!strcasecmp(parameter[0], "dpcamera"))
				{
					shader.textureflags |= Q3TEXTUREFLAG_CAMERA;
				}
				else if (!strcasecmp(parameter[0], "dpwater") && numparameters >= 12)
				{
					shader.textureflags |= Q3TEXTUREFLAG_WATERSHADER;
					shader.reflectmin = atof(parameter[1]);
					shader.reflectmax = atof(parameter[2]);
					shader.refractfactor = atof(parameter[3]);
					shader.reflectfactor = atof(parameter[4]);
					Vector4Set(shader.refractcolor4f, atof(parameter[5]), atof(parameter[6]), atof(parameter[7]), 1);
					Vector4Set(shader.reflectcolor4f, atof(parameter[8]), atof(parameter[9]), atof(parameter[10]), 1);
					shader.r_water_wateralpha = atof(parameter[11]);
				}
				else if (!strcasecmp(parameter[0], "dpwaterscroll") && numparameters >= 3)
				{
					shader.r_water_waterscroll[0] = 1/atof(parameter[1]);
					shader.r_water_waterscroll[1] = 1/atof(parameter[2]);
				}
				else if (!strcasecmp(parameter[0], "dpglossintensitymod") && numparameters >= 2)
				{
					shader.specularscalemod = atof(parameter[1]);
				}
				else if (!strcasecmp(parameter[0], "dpglossexponentmod") && numparameters >= 2)
				{
					shader.specularpowermod = atof(parameter[1]);
				}
				else if (!strcasecmp(parameter[0], "dpoffsetmapping") && numparameters >= 3)
				{
					if (!strcasecmp(parameter[1], "disable") || !strcasecmp(parameter[1], "none") || !strcasecmp(parameter[1], "off"))
						shader.offsetmapping = OFFSETMAPPING_OFF;
					else if (!strcasecmp(parameter[1], "default"))
						shader.offsetmapping = OFFSETMAPPING_DEFAULT;
					else if (!strcasecmp(parameter[1], "linear"))
						shader.offsetmapping = OFFSETMAPPING_LINEAR;
					else if (!strcasecmp(parameter[1], "relief"))
						shader.offsetmapping = OFFSETMAPPING_RELIEF;
					shader.offsetscale = atof(parameter[2]);
				}
				else if (!strcasecmp(parameter[0], "deformvertexes") && numparameters >= 2)
				{
					int i, deformindex;
					for (deformindex = 0;deformindex < Q3MAXDEFORMS;deformindex++)
						if (!shader.deforms[deformindex].deform)
							break;
					if (deformindex < Q3MAXDEFORMS)
					{
						for (i = 0;i < numparameters - 2 && i < Q3DEFORM_MAXPARMS;i++)
							shader.deforms[deformindex].parms[i] = atof(parameter[i+2]);
						     if (!strcasecmp(parameter[1], "projectionshadow")) shader.deforms[deformindex].deform = Q3DEFORM_PROJECTIONSHADOW;
						else if (!strcasecmp(parameter[1], "autosprite"      )) shader.deforms[deformindex].deform = Q3DEFORM_AUTOSPRITE;
						else if (!strcasecmp(parameter[1], "autosprite2"     )) shader.deforms[deformindex].deform = Q3DEFORM_AUTOSPRITE2;
						else if (!strcasecmp(parameter[1], "text0"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT0;
						else if (!strcasecmp(parameter[1], "text1"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT1;
						else if (!strcasecmp(parameter[1], "text2"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT2;
						else if (!strcasecmp(parameter[1], "text3"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT3;
						else if (!strcasecmp(parameter[1], "text4"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT4;
						else if (!strcasecmp(parameter[1], "text5"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT5;
						else if (!strcasecmp(parameter[1], "text6"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT6;
						else if (!strcasecmp(parameter[1], "text7"           )) shader.deforms[deformindex].deform = Q3DEFORM_TEXT7;
						else if (!strcasecmp(parameter[1], "bulge"           )) shader.deforms[deformindex].deform = Q3DEFORM_BULGE;
						else if (!strcasecmp(parameter[1], "normal"          )) shader.deforms[deformindex].deform = Q3DEFORM_NORMAL;
						else if (!strcasecmp(parameter[1], "wave"            ))
						{
							shader.deforms[deformindex].deform = Q3DEFORM_WAVE;
							shader.deforms[deformindex].wavefunc = Mod_LoadQ3Shaders_EnumerateWaveFunc(parameter[3]);
							for (i = 0;i < numparameters - 4 && i < Q3WAVEPARMS;i++)
								shader.deforms[deformindex].waveparms[i] = atof(parameter[i+4]);
						}
						else if (!strcasecmp(parameter[1], "move"            ))
						{
							shader.deforms[deformindex].deform = Q3DEFORM_MOVE;
							shader.deforms[deformindex].wavefunc = Mod_LoadQ3Shaders_EnumerateWaveFunc(parameter[5]);
							for (i = 0;i < numparameters - 6 && i < Q3WAVEPARMS;i++)
								shader.deforms[deformindex].waveparms[i] = atof(parameter[i+6]);
						}
					}
				}
			}
			// pick the primary layer to render with
			if (shader.numlayers)
			{
				shader.backgroundlayer = -1;
				shader.primarylayer = 0;
				// if lightmap comes first this is definitely an ordinary texture
				// if the first two layers have the correct blendfuncs and use vertex alpha, it is a blended terrain shader
				if ((shader.layers[shader.primarylayer].texturename != NULL)
				  && !strcasecmp(shader.layers[shader.primarylayer].texturename[0], "$lightmap"))
				{
					shader.backgroundlayer = -1;
					shader.primarylayer = 1;
				}
				else if (shader.numlayers >= 2
				&&   shader.layers[1].alphagen.alphagen == Q3ALPHAGEN_VERTEX
				&&  (shader.layers[0].blendfunc[0] == GL_ONE       && shader.layers[0].blendfunc[1] == GL_ZERO                && !shader.layers[0].alphatest)
				&& ((shader.layers[1].blendfunc[0] == GL_SRC_ALPHA && shader.layers[1].blendfunc[1] == GL_ONE_MINUS_SRC_ALPHA)
				||  (shader.layers[1].blendfunc[0] == GL_ONE       && shader.layers[1].blendfunc[1] == GL_ZERO                &&  shader.layers[1].alphatest)))
				{
					// terrain blending or other effects
					shader.backgroundlayer = 0;
					shader.primarylayer = 1;
				}
			}
			// fix up multiple reflection types
			if(shader.textureflags & Q3TEXTUREFLAG_WATERSHADER)
				shader.textureflags &= ~(Q3TEXTUREFLAG_REFRACTION | Q3TEXTUREFLAG_REFLECTION | Q3TEXTUREFLAG_CAMERA);

			Q3Shader_AddToHash (&shader);
		}
		Mem_Free(f);
	}
	FS_FreeSearch(search);
	// free custinfoparm values
	for (j = 0; j < numcustsurfaceparms; j++)
		Mem_Free(custsurfaceparmnames[j]);
}

q3shaderinfo_t *Mod_LookupQ3Shader(const char *name)
{
	unsigned short hash;
	q3shader_hash_entry_t* entry;
	if (!q3shaders_mem)
		Mod_LoadQ3Shaders();
	hash = CRC_Block_CaseInsensitive ((const unsigned char *)name, strlen (name));
	entry = q3shader_data->hash + (hash % Q3SHADER_HASH_SIZE);
	while (entry != NULL)
	{
		if (strcasecmp (entry->shader.name, name) == 0)
			return &entry->shader;
		entry = entry->chain;
	}
	return NULL;
}

qboolean Mod_LoadTextureFromQ3Shader(texture_t *texture, const char *name, qboolean warnmissing, qboolean fallback, int defaulttexflags)
{
	int j;
	int texflagsmask, texflagsor;
	qboolean success = true;
	q3shaderinfo_t *shader;
	if (!name)
		name = "";
	strlcpy(texture->name, name, sizeof(texture->name));
	shader = name[0] ? Mod_LookupQ3Shader(name) : NULL;

	texflagsmask = ~0;
	if(!(defaulttexflags & TEXF_PICMIP))
		texflagsmask &= ~TEXF_PICMIP;
	if(!(defaulttexflags & TEXF_COMPRESS))
		texflagsmask &= ~TEXF_COMPRESS;
	texflagsor = 0;
	if(defaulttexflags & TEXF_ISWORLD)
		texflagsor |= TEXF_ISWORLD;
	if(defaulttexflags & TEXF_ISSPRITE)
		texflagsor |= TEXF_ISSPRITE;
	// unless later loaded from the shader
	texture->offsetmapping = (mod_noshader_default_offsetmapping.value) ? OFFSETMAPPING_DEFAULT : OFFSETMAPPING_OFF;
	texture->offsetscale = 1;
	texture->specularscalemod = 1;
	texture->specularpowermod = 1; 
	// WHEN ADDING DEFAULTS HERE, REMEMBER TO SYNC TO SHADER LOADING ABOVE
	// HERE, AND Q1BSP LOADING
	// JUST GREP FOR "specularscalemod = 1".

	if (shader)
	{
		if (developer_loading.integer)
			Con_Printf("%s: loaded shader for %s\n", loadmodel->name, name);
		texture->surfaceparms = shader->surfaceparms;

		// allow disabling of picmip or compression by defaulttexflags
		texture->textureflags = (shader->textureflags & texflagsmask) | texflagsor;

		if (shader->surfaceparms & Q3SURFACEPARM_SKY)
		{
			texture->basematerialflags = MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
			if (shader->skyboxname[0])
			{
				// quake3 seems to append a _ to the skybox name, so this must do so as well
				dpsnprintf(loadmodel->brush.skybox, sizeof(loadmodel->brush.skybox), "%s_", shader->skyboxname);
			}
		}
		else if ((texture->surfaceflags & Q3SURFACEFLAG_NODRAW) || shader->numlayers == 0)
			texture->basematerialflags = MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
		else
			texture->basematerialflags = MATERIALFLAG_WALL;

		if (shader->layers[0].alphatest)
			texture->basematerialflags |= MATERIALFLAG_ALPHATEST | MATERIALFLAG_NOSHADOW;
		if (shader->textureflags & Q3TEXTUREFLAG_TWOSIDED)
			texture->basematerialflags |= MATERIALFLAG_NOSHADOW | MATERIALFLAG_NOCULLFACE;
		if (shader->textureflags & Q3TEXTUREFLAG_POLYGONOFFSET)
		{
			texture->biaspolygonoffset += shader->biaspolygonoffset;
			texture->biaspolygonfactor += shader->biaspolygonfactor;
		}
		if (shader->textureflags & Q3TEXTUREFLAG_REFRACTION)
			texture->basematerialflags |= MATERIALFLAG_REFRACTION;
		if (shader->textureflags & Q3TEXTUREFLAG_REFLECTION)
			texture->basematerialflags |= MATERIALFLAG_REFLECTION;
		if (shader->textureflags & Q3TEXTUREFLAG_WATERSHADER)
			texture->basematerialflags |= MATERIALFLAG_WATERSHADER;
		if (shader->textureflags & Q3TEXTUREFLAG_CAMERA)
			texture->basematerialflags |= MATERIALFLAG_CAMERA;
		texture->customblendfunc[0] = GL_ONE;
		texture->customblendfunc[1] = GL_ZERO;
		if (shader->numlayers > 0)
		{
			texture->customblendfunc[0] = shader->layers[0].blendfunc[0];
			texture->customblendfunc[1] = shader->layers[0].blendfunc[1];
/*
Q3 shader blendfuncs actually used in the game (* = supported by DP)
* additive               GL_ONE GL_ONE
additive weird         GL_ONE GL_SRC_ALPHA
additive weird 2       GL_ONE GL_ONE_MINUS_SRC_ALPHA
* alpha                  GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
alpha inverse          GL_ONE_MINUS_SRC_ALPHA GL_SRC_ALPHA
brighten               GL_DST_COLOR GL_ONE
brighten               GL_ONE GL_SRC_COLOR
brighten weird         GL_DST_COLOR GL_ONE_MINUS_DST_ALPHA
brighten weird 2       GL_DST_COLOR GL_SRC_ALPHA
* modulate               GL_DST_COLOR GL_ZERO
* modulate               GL_ZERO GL_SRC_COLOR
modulate inverse       GL_ZERO GL_ONE_MINUS_SRC_COLOR
modulate inverse alpha GL_ZERO GL_SRC_ALPHA
modulate weird inverse GL_ONE_MINUS_DST_COLOR GL_ZERO
* modulate x2            GL_DST_COLOR GL_SRC_COLOR
* no blend               GL_ONE GL_ZERO
nothing                GL_ZERO GL_ONE
*/
			// if not opaque, figure out what blendfunc to use
			if (shader->layers[0].blendfunc[0] != GL_ONE || shader->layers[0].blendfunc[1] != GL_ZERO)
			{
				if (shader->layers[0].blendfunc[0] == GL_ONE && shader->layers[0].blendfunc[1] == GL_ONE)
					texture->basematerialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
				else if (shader->layers[0].blendfunc[0] == GL_SRC_ALPHA && shader->layers[0].blendfunc[1] == GL_ONE)
					texture->basematerialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
				else if (shader->layers[0].blendfunc[0] == GL_SRC_ALPHA && shader->layers[0].blendfunc[1] == GL_ONE_MINUS_SRC_ALPHA)
					texture->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
				else
					texture->basematerialflags |= MATERIALFLAG_CUSTOMBLEND | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
			}
		}
		if (!shader->lighting)
			texture->basematerialflags |= MATERIALFLAG_FULLBRIGHT;
		if (shader->primarylayer >= 0)
		{
			q3shaderinfo_layer_t* primarylayer = shader->layers + shader->primarylayer;
			// copy over many primarylayer parameters
			texture->rgbgen = primarylayer->rgbgen;
			texture->alphagen = primarylayer->alphagen;
			texture->tcgen = primarylayer->tcgen;
			memcpy(texture->tcmods, primarylayer->tcmods, sizeof(texture->tcmods));
			// load the textures
			texture->numskinframes = primarylayer->numframes;
			texture->skinframerate = primarylayer->framerate;
			for (j = 0;j < primarylayer->numframes;j++)
			{
				if(cls.state == ca_dedicated)
				{
					texture->skinframes[j] = NULL;
				}
				else if (!(texture->skinframes[j] = R_SkinFrame_LoadExternal(primarylayer->texturename[j], (primarylayer->texflags & texflagsmask) | texflagsor, false)))
				{
					Con_Printf("^1%s:^7 could not load texture ^3\"%s\"^7 (frame %i) for shader ^2\"%s\"\n", loadmodel->name, primarylayer->texturename[j], j, texture->name);
					texture->skinframes[j] = R_SkinFrame_LoadMissing();
				}
			}
		}
		if (shader->backgroundlayer >= 0)
		{
			q3shaderinfo_layer_t* backgroundlayer = shader->layers + shader->backgroundlayer;
			// copy over one secondarylayer parameter
			memcpy(texture->backgroundtcmods, backgroundlayer->tcmods, sizeof(texture->backgroundtcmods));
			// load the textures
			texture->backgroundnumskinframes = backgroundlayer->numframes;
			texture->backgroundskinframerate = backgroundlayer->framerate;
			for (j = 0;j < backgroundlayer->numframes;j++)
			{
				if(cls.state == ca_dedicated)
				{
					texture->skinframes[j] = NULL;
				}
				else if (!(texture->backgroundskinframes[j] = R_SkinFrame_LoadExternal(backgroundlayer->texturename[j], (backgroundlayer->texflags & texflagsmask) | texflagsor, false)))
				{
					Con_Printf("^1%s:^7 could not load texture ^3\"%s\"^7 (background frame %i) for shader ^2\"%s\"\n", loadmodel->name, backgroundlayer->texturename[j], j, texture->name);
					texture->backgroundskinframes[j] = R_SkinFrame_LoadMissing();
				}
			}
		}
		if (shader->dpshadow)
			texture->basematerialflags &= ~MATERIALFLAG_NOSHADOW;
		if (shader->dpnoshadow)
			texture->basematerialflags |= MATERIALFLAG_NOSHADOW;
		if (shader->dpnortlight)
			texture->basematerialflags |= MATERIALFLAG_NORTLIGHT;
		memcpy(texture->deforms, shader->deforms, sizeof(texture->deforms));
		texture->reflectmin = shader->reflectmin;
		texture->reflectmax = shader->reflectmax;
		texture->refractfactor = shader->refractfactor;
		Vector4Copy(shader->refractcolor4f, texture->refractcolor4f);
		texture->reflectfactor = shader->reflectfactor;
		Vector4Copy(shader->reflectcolor4f, texture->reflectcolor4f);
		texture->r_water_wateralpha = shader->r_water_wateralpha;
		Vector2Copy(shader->r_water_waterscroll, texture->r_water_waterscroll);
		texture->offsetmapping = shader->offsetmapping;
		texture->offsetscale = shader->offsetscale;
		texture->specularscalemod = shader->specularscalemod;
		texture->specularpowermod = shader->specularpowermod;
		if (shader->dpreflectcube[0])
			texture->reflectcubetexture = R_GetCubemap(shader->dpreflectcube);

		// set up default supercontents (on q3bsp this is overridden by the q3bsp loader)
		texture->supercontents = SUPERCONTENTS_SOLID | SUPERCONTENTS_OPAQUE;
		if (shader->surfaceparms & Q3SURFACEPARM_LAVA         ) texture->supercontents  = SUPERCONTENTS_LAVA         ;
		if (shader->surfaceparms & Q3SURFACEPARM_SLIME        ) texture->supercontents  = SUPERCONTENTS_SLIME        ;
		if (shader->surfaceparms & Q3SURFACEPARM_WATER        ) texture->supercontents  = SUPERCONTENTS_WATER        ;
		if (shader->surfaceparms & Q3SURFACEPARM_NONSOLID     ) texture->supercontents  = 0                          ;
		if (shader->surfaceparms & Q3SURFACEPARM_PLAYERCLIP   ) texture->supercontents  = SUPERCONTENTS_PLAYERCLIP   ;
		if (shader->surfaceparms & Q3SURFACEPARM_BOTCLIP      ) texture->supercontents  = SUPERCONTENTS_MONSTERCLIP  ;
		if (shader->surfaceparms & Q3SURFACEPARM_SKY          ) texture->supercontents  = SUPERCONTENTS_SKY          ;

	//	if (shader->surfaceparms & Q3SURFACEPARM_ALPHASHADOW  ) texture->supercontents |= SUPERCONTENTS_ALPHASHADOW  ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_AREAPORTAL   ) texture->supercontents |= SUPERCONTENTS_AREAPORTAL   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_CLUSTERPORTAL) texture->supercontents |= SUPERCONTENTS_CLUSTERPORTAL;
	//	if (shader->surfaceparms & Q3SURFACEPARM_DETAIL       ) texture->supercontents |= SUPERCONTENTS_DETAIL       ;
		if (shader->surfaceparms & Q3SURFACEPARM_DONOTENTER   ) texture->supercontents |= SUPERCONTENTS_DONOTENTER   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_FOG          ) texture->supercontents |= SUPERCONTENTS_FOG          ;
		if (shader->surfaceparms & Q3SURFACEPARM_LAVA         ) texture->supercontents |= SUPERCONTENTS_LAVA         ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_LIGHTFILTER  ) texture->supercontents |= SUPERCONTENTS_LIGHTFILTER  ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_METALSTEPS   ) texture->supercontents |= SUPERCONTENTS_METALSTEPS   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NODAMAGE     ) texture->supercontents |= SUPERCONTENTS_NODAMAGE     ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NODLIGHT     ) texture->supercontents |= SUPERCONTENTS_NODLIGHT     ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NODRAW       ) texture->supercontents |= SUPERCONTENTS_NODRAW       ;
		if (shader->surfaceparms & Q3SURFACEPARM_NODROP       ) texture->supercontents |= SUPERCONTENTS_NODROP       ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NOIMPACT     ) texture->supercontents |= SUPERCONTENTS_NOIMPACT     ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NOLIGHTMAP   ) texture->supercontents |= SUPERCONTENTS_NOLIGHTMAP   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NOMARKS      ) texture->supercontents |= SUPERCONTENTS_NOMARKS      ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_NOMIPMAPS    ) texture->supercontents |= SUPERCONTENTS_NOMIPMAPS    ;
		if (shader->surfaceparms & Q3SURFACEPARM_NONSOLID     ) texture->supercontents &=~SUPERCONTENTS_SOLID        ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_ORIGIN       ) texture->supercontents |= SUPERCONTENTS_ORIGIN       ;
		if (shader->surfaceparms & Q3SURFACEPARM_PLAYERCLIP   ) texture->supercontents |= SUPERCONTENTS_PLAYERCLIP   ;
		if (shader->surfaceparms & Q3SURFACEPARM_SKY          ) texture->supercontents |= SUPERCONTENTS_SKY          ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_SLICK        ) texture->supercontents |= SUPERCONTENTS_SLICK        ;
		if (shader->surfaceparms & Q3SURFACEPARM_SLIME        ) texture->supercontents |= SUPERCONTENTS_SLIME        ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_STRUCTURAL   ) texture->supercontents |= SUPERCONTENTS_STRUCTURAL   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_TRANS        ) texture->supercontents |= SUPERCONTENTS_TRANS        ;
		if (shader->surfaceparms & Q3SURFACEPARM_WATER        ) texture->supercontents |= SUPERCONTENTS_WATER        ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_POINTLIGHT   ) texture->supercontents |= SUPERCONTENTS_POINTLIGHT   ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_HINT         ) texture->supercontents |= SUPERCONTENTS_HINT         ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_DUST         ) texture->supercontents |= SUPERCONTENTS_DUST         ;
		if (shader->surfaceparms & Q3SURFACEPARM_BOTCLIP      ) texture->supercontents |= SUPERCONTENTS_BOTCLIP      | SUPERCONTENTS_MONSTERCLIP;
	//	if (shader->surfaceparms & Q3SURFACEPARM_LIGHTGRID    ) texture->supercontents |= SUPERCONTENTS_LIGHTGRID    ;
	//	if (shader->surfaceparms & Q3SURFACEPARM_ANTIPORTAL   ) texture->supercontents |= SUPERCONTENTS_ANTIPORTAL   ;

		if (shader->dpmeshcollisions)
			texture->basematerialflags |= MATERIALFLAG_MESHCOLLISIONS;
	}
	else if (!strcmp(texture->name, "noshader") || !texture->name[0])
	{
		if (developer_extra.integer)
			Con_DPrintf("^1%s:^7 using fallback noshader material for ^3\"%s\"\n", loadmodel->name, name);
		texture->surfaceparms = 0;
		texture->supercontents = SUPERCONTENTS_SOLID | SUPERCONTENTS_OPAQUE;
	}
	else if (!strcmp(texture->name, "common/nodraw") || !strcmp(texture->name, "textures/common/nodraw"))
	{
		if (developer_extra.integer)
			Con_DPrintf("^1%s:^7 using fallback nodraw material for ^3\"%s\"\n", loadmodel->name, name);
		texture->surfaceparms = 0;
		texture->basematerialflags = MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
		texture->supercontents = SUPERCONTENTS_SOLID;
	}
	else
	{
		if (developer_extra.integer)
			Con_DPrintf("^1%s:^7 No shader found for texture ^3\"%s\"\n", loadmodel->name, texture->name);
		texture->surfaceparms = 0;
		if (texture->surfaceflags & Q3SURFACEFLAG_NODRAW)
		{
			texture->basematerialflags |= MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
			texture->supercontents = SUPERCONTENTS_SOLID;
		}
		else if (texture->surfaceflags & Q3SURFACEFLAG_SKY)
		{
			texture->basematerialflags |= MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
			texture->supercontents = SUPERCONTENTS_SKY;
		}
		else
		{
			texture->basematerialflags |= MATERIALFLAG_WALL;
			texture->supercontents = SUPERCONTENTS_SOLID | SUPERCONTENTS_OPAQUE;
		}
		texture->numskinframes = 1;
		if(cls.state == ca_dedicated)
		{
			texture->skinframes[0] = NULL;
			success = false;
		}
		else
		{
			if (fallback)
			{
				if ((texture->skinframes[0] = R_SkinFrame_LoadExternal(texture->name, defaulttexflags, false)))
				{
					if(texture->skinframes[0]->hasalpha)
						texture->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
				}
				else
					success = false;
			}
			else
				success = false;
			if (!success && warnmissing)
				Con_Printf("^1%s:^7 could not load texture ^3\"%s\"\n", loadmodel->name, texture->name);
		}
	}
	// init the animation variables
	texture->currentframe = texture;
	if (texture->numskinframes < 1)
		texture->numskinframes = 1;
	if (!texture->skinframes[0])
		texture->skinframes[0] = R_SkinFrame_LoadMissing();
	texture->currentskinframe = texture->skinframes[0];
	texture->backgroundcurrentskinframe = texture->backgroundskinframes[0];
	return success;
}

skinfile_t *Mod_LoadSkinFiles(void)
{
	int i, words, line, wordsoverflow;
	char *text;
	const char *data;
	skinfile_t *skinfile = NULL, *first = NULL;
	skinfileitem_t *skinfileitem;
	char word[10][MAX_QPATH];

/*
sample file:
U_bodyBox,models/players/Legoman/BikerA2.tga
U_RArm,models/players/Legoman/BikerA1.tga
U_LArm,models/players/Legoman/BikerA1.tga
U_armor,common/nodraw
U_sword,common/nodraw
U_shield,common/nodraw
U_homb,common/nodraw
U_backpack,common/nodraw
U_colcha,common/nodraw
tag_head,
tag_weapon,
tag_torso,
*/
	memset(word, 0, sizeof(word));
	for (i = 0;i < 256 && (data = text = (char *)FS_LoadFile(va("%s_%i.skin", loadmodel->name, i), tempmempool, true, NULL));i++)
	{
		// If it's the first file we parse
		if (skinfile == NULL)
		{
			skinfile = (skinfile_t *)Mem_Alloc(loadmodel->mempool, sizeof(skinfile_t));
			first = skinfile;
		}
		else
		{
			skinfile->next = (skinfile_t *)Mem_Alloc(loadmodel->mempool, sizeof(skinfile_t));
			skinfile = skinfile->next;
		}
		skinfile->next = NULL;

		for(line = 0;;line++)
		{
			// parse line
			if (!COM_ParseToken_QuakeC(&data, true))
				break;
			if (!strcmp(com_token, "\n"))
				continue;
			words = 0;
			wordsoverflow = false;
			do
			{
				if (words < 10)
					strlcpy(word[words++], com_token, sizeof (word[0]));
				else
					wordsoverflow = true;
			}
			while (COM_ParseToken_QuakeC(&data, true) && strcmp(com_token, "\n"));
			if (wordsoverflow)
			{
				Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: line with too many statements, skipping\n", loadmodel->name, i, line);
				continue;
			}
			// words is always >= 1
			if (!strcmp(word[0], "replace"))
			{
				if (words == 3)
				{
					if (developer_loading.integer)
						Con_Printf("Mod_LoadSkinFiles: parsed mesh \"%s\" shader replacement \"%s\"\n", word[1], word[2]);
					skinfileitem = (skinfileitem_t *)Mem_Alloc(loadmodel->mempool, sizeof(skinfileitem_t));
					skinfileitem->next = skinfile->items;
					skinfile->items = skinfileitem;
					strlcpy (skinfileitem->name, word[1], sizeof (skinfileitem->name));
					strlcpy (skinfileitem->replacement, word[2], sizeof (skinfileitem->replacement));
				}
				else
					Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: wrong number of parameters to command \"%s\", see documentation in DP_GFX_SKINFILES extension in dpextensions.qc\n", loadmodel->name, i, line, word[0]);
			}
			else if (words >= 2 && !strncmp(word[0], "tag_", 4))
			{
				// tag name, like "tag_weapon,"
				// not used for anything (not even in Quake3)
			}
			else if (words >= 2 && !strcmp(word[1], ","))
			{
				// mesh shader name, like "U_RArm,models/players/Legoman/BikerA1.tga"
				if (developer_loading.integer)
					Con_Printf("Mod_LoadSkinFiles: parsed mesh \"%s\" shader replacement \"%s\"\n", word[0], word[2]);
				skinfileitem = (skinfileitem_t *)Mem_Alloc(loadmodel->mempool, sizeof(skinfileitem_t));
				skinfileitem->next = skinfile->items;
				skinfile->items = skinfileitem;
				strlcpy (skinfileitem->name, word[0], sizeof (skinfileitem->name));
				strlcpy (skinfileitem->replacement, word[2], sizeof (skinfileitem->replacement));
			}
			else
				Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: does not look like tag or mesh specification, or replace command, see documentation in DP_GFX_SKINFILES extension in dpextensions.qc\n", loadmodel->name, i, line);
		}
		Mem_Free(text);
	}
	if (i)
		loadmodel->numskins = i;
	return first;
}

void Mod_FreeSkinFiles(skinfile_t *skinfile)
{
	skinfile_t *next;
	skinfileitem_t *skinfileitem, *nextitem;
	for (;skinfile;skinfile = next)
	{
		next = skinfile->next;
		for (skinfileitem = skinfile->items;skinfileitem;skinfileitem = nextitem)
		{
			nextitem = skinfileitem->next;
			Mem_Free(skinfileitem);
		}
		Mem_Free(skinfile);
	}
}

int Mod_CountSkinFiles(skinfile_t *skinfile)
{
	int i;
	for (i = 0;skinfile;skinfile = skinfile->next, i++);
	return i;
}

void Mod_SnapVertices(int numcomponents, int numvertices, float *vertices, float snap)
{
	int i;
	double isnap = 1.0 / snap;
	for (i = 0;i < numvertices*numcomponents;i++)
		vertices[i] = floor(vertices[i]*isnap)*snap;
}

int Mod_RemoveDegenerateTriangles(int numtriangles, const int *inelement3i, int *outelement3i, const float *vertex3f)
{
	int i, outtriangles;
	float edgedir1[3], edgedir2[3], temp[3];
	// a degenerate triangle is one with no width (thickness, surface area)
	// these are characterized by having all 3 points colinear (along a line)
	// or having two points identical
	// the simplest check is to calculate the triangle's area
	for (i = 0, outtriangles = 0;i < numtriangles;i++, inelement3i += 3)
	{
		// calculate first edge
		VectorSubtract(vertex3f + inelement3i[1] * 3, vertex3f + inelement3i[0] * 3, edgedir1);
		VectorSubtract(vertex3f + inelement3i[2] * 3, vertex3f + inelement3i[0] * 3, edgedir2);
		CrossProduct(edgedir1, edgedir2, temp);
		if (VectorLength2(temp) < 0.001f)
			continue; // degenerate triangle (no area)
		// valid triangle (has area)
		VectorCopy(inelement3i, outelement3i);
		outelement3i += 3;
		outtriangles++;
	}
	return outtriangles;
}

void Mod_VertexRangeFromElements(int numelements, const int *elements, int *firstvertexpointer, int *lastvertexpointer)
{
	int i, e;
	int firstvertex, lastvertex;
	if (numelements > 0 && elements)
	{
		firstvertex = lastvertex = elements[0];
		for (i = 1;i < numelements;i++)
		{
			e = elements[i];
			firstvertex = min(firstvertex, e);
			lastvertex = max(lastvertex, e);
		}
	}
	else
		firstvertex = lastvertex = 0;
	if (firstvertexpointer)
		*firstvertexpointer = firstvertex;
	if (lastvertexpointer)
		*lastvertexpointer = lastvertex;
}

void Mod_MakeSortedSurfaces(dp_model_t *mod)
{
	// make an optimal set of texture-sorted batches to draw...
	int j, t;
	int *firstsurfacefortexture;
	int *numsurfacesfortexture;
	if (!mod->sortedmodelsurfaces)
		mod->sortedmodelsurfaces = (int *) Mem_Alloc(loadmodel->mempool, mod->nummodelsurfaces * sizeof(*mod->sortedmodelsurfaces));
	firstsurfacefortexture = (int *) Mem_Alloc(tempmempool, mod->num_textures * sizeof(*firstsurfacefortexture));
	numsurfacesfortexture = (int *) Mem_Alloc(tempmempool, mod->num_textures * sizeof(*numsurfacesfortexture));
	memset(numsurfacesfortexture, 0, mod->num_textures * sizeof(*numsurfacesfortexture));
	for (j = 0;j < mod->nummodelsurfaces;j++)
	{
		const msurface_t *surface = mod->data_surfaces + j + mod->firstmodelsurface;
		int t = (int)(surface->texture - mod->data_textures);
		numsurfacesfortexture[t]++;
	}
	j = 0;
	for (t = 0;t < mod->num_textures;t++)
	{
		firstsurfacefortexture[t] = j;
		j += numsurfacesfortexture[t];
	}
	for (j = 0;j < mod->nummodelsurfaces;j++)
	{
		const msurface_t *surface = mod->data_surfaces + j + mod->firstmodelsurface;
		int t = (int)(surface->texture - mod->data_textures);
		mod->sortedmodelsurfaces[firstsurfacefortexture[t]++] = j + mod->firstmodelsurface;
	}
	Mem_Free(firstsurfacefortexture);
	Mem_Free(numsurfacesfortexture);
}

void Mod_BuildVBOs(void)
{
	if (!loadmodel->surfmesh.num_vertices)
		return;

	if (gl_paranoid.integer && loadmodel->surfmesh.data_element3s && loadmodel->surfmesh.data_element3i)
	{
		int i;
		for (i = 0;i < loadmodel->surfmesh.num_triangles*3;i++)
		{
			if (loadmodel->surfmesh.data_element3s[i] != loadmodel->surfmesh.data_element3i[i])
			{
				Con_Printf("Mod_BuildVBOs: element %u is incorrect (%u should be %u)\n", i, loadmodel->surfmesh.data_element3s[i], loadmodel->surfmesh.data_element3i[i]);
				loadmodel->surfmesh.data_element3s[i] = loadmodel->surfmesh.data_element3i[i];
			}
		}
	}

	// build r_vertexmesh_t array
	// (compressed interleaved array for D3D)
	if (!loadmodel->surfmesh.vertexmesh && vid.useinterleavedarrays)
	{
		int vertexindex;
		int numvertices = loadmodel->surfmesh.num_vertices;
		r_vertexmesh_t *vertexmesh;
		loadmodel->surfmesh.vertexmesh = vertexmesh = (r_vertexmesh_t*)Mem_Alloc(loadmodel->mempool, numvertices * sizeof(*loadmodel->surfmesh.vertexmesh));
		for (vertexindex = 0;vertexindex < numvertices;vertexindex++, vertexmesh++)
		{
			VectorCopy(loadmodel->surfmesh.data_vertex3f + 3*vertexindex, vertexmesh->vertex3f);
			VectorScale(loadmodel->surfmesh.data_svector3f + 3*vertexindex, 1.0f, vertexmesh->svector3f);
			VectorScale(loadmodel->surfmesh.data_tvector3f + 3*vertexindex, 1.0f, vertexmesh->tvector3f);
			VectorScale(loadmodel->surfmesh.data_normal3f + 3*vertexindex, 1.0f, vertexmesh->normal3f);
			if (loadmodel->surfmesh.data_lightmapcolor4f)
				Vector4Copy(loadmodel->surfmesh.data_lightmapcolor4f + 4*vertexindex, vertexmesh->color4f);
			Vector2Copy(loadmodel->surfmesh.data_texcoordtexture2f + 2*vertexindex, vertexmesh->texcoordtexture2f);
			if (loadmodel->surfmesh.data_texcoordlightmap2f)
				Vector2Scale(loadmodel->surfmesh.data_texcoordlightmap2f + 2*vertexindex, 1.0f, vertexmesh->texcoordlightmap2f);
		}
	}

	// upload r_vertexmesh_t array as a buffer
	if (loadmodel->surfmesh.vertexmesh && !loadmodel->surfmesh.vertexmeshbuffer)
		loadmodel->surfmesh.vertexmeshbuffer = R_Mesh_CreateMeshBuffer(loadmodel->surfmesh.vertexmesh, loadmodel->surfmesh.num_vertices * sizeof(*loadmodel->surfmesh.vertexmesh), loadmodel->name, false, false, false);

	// upload vertex3f array as a buffer
	if (loadmodel->surfmesh.data_vertex3f && !loadmodel->surfmesh.vertex3fbuffer)
		loadmodel->surfmesh.vertex3fbuffer = R_Mesh_CreateMeshBuffer(loadmodel->surfmesh.data_vertex3f, loadmodel->surfmesh.num_vertices * sizeof(float[3]), loadmodel->name, false, false, false);

	// upload short indices as a buffer
	if (loadmodel->surfmesh.data_element3s && !loadmodel->surfmesh.data_element3s_indexbuffer)
		loadmodel->surfmesh.data_element3s_indexbuffer = R_Mesh_CreateMeshBuffer(loadmodel->surfmesh.data_element3s, loadmodel->surfmesh.num_triangles * sizeof(short[3]), loadmodel->name, true, false, true);

	// upload int indices as a buffer
	if (loadmodel->surfmesh.data_element3i && !loadmodel->surfmesh.data_element3i_indexbuffer && !loadmodel->surfmesh.data_element3s)
		loadmodel->surfmesh.data_element3i_indexbuffer = R_Mesh_CreateMeshBuffer(loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.num_triangles * sizeof(int[3]), loadmodel->name, true, false, false);

	// only build a vbo if one has not already been created (this is important for brush models which load specially)
	// vertex buffer is several arrays and we put them in the same buffer
	//
	// is this wise?  the texcoordtexture2f array is used with dynamic
	// vertex/svector/tvector/normal when rendering animated models, on the
	// other hand animated models don't use a lot of vertices anyway...
	if (!loadmodel->surfmesh.vbo_vertexbuffer && !vid.useinterleavedarrays)
	{
		size_t size;
		unsigned char *mem;
		size = 0;
		loadmodel->surfmesh.vbooffset_vertex3f           = size;if (loadmodel->surfmesh.data_vertex3f          ) size += loadmodel->surfmesh.num_vertices * sizeof(float[3]);
		loadmodel->surfmesh.vbooffset_svector3f          = size;if (loadmodel->surfmesh.data_svector3f         ) size += loadmodel->surfmesh.num_vertices * sizeof(float[3]);
		loadmodel->surfmesh.vbooffset_tvector3f          = size;if (loadmodel->surfmesh.data_tvector3f         ) size += loadmodel->surfmesh.num_vertices * sizeof(float[3]);
		loadmodel->surfmesh.vbooffset_normal3f           = size;if (loadmodel->surfmesh.data_normal3f          ) size += loadmodel->surfmesh.num_vertices * sizeof(float[3]);
		loadmodel->surfmesh.vbooffset_texcoordtexture2f  = size;if (loadmodel->surfmesh.data_texcoordtexture2f ) size += loadmodel->surfmesh.num_vertices * sizeof(float[2]);
		loadmodel->surfmesh.vbooffset_texcoordlightmap2f = size;if (loadmodel->surfmesh.data_texcoordlightmap2f) size += loadmodel->surfmesh.num_vertices * sizeof(float[2]);
		loadmodel->surfmesh.vbooffset_lightmapcolor4f    = size;if (loadmodel->surfmesh.data_lightmapcolor4f   ) size += loadmodel->surfmesh.num_vertices * sizeof(float[4]);
		mem = (unsigned char *)Mem_Alloc(tempmempool, size);
		if (loadmodel->surfmesh.data_vertex3f          ) memcpy(mem + loadmodel->surfmesh.vbooffset_vertex3f          , loadmodel->surfmesh.data_vertex3f          , loadmodel->surfmesh.num_vertices * sizeof(float[3]));
		if (loadmodel->surfmesh.data_svector3f         ) memcpy(mem + loadmodel->surfmesh.vbooffset_svector3f         , loadmodel->surfmesh.data_svector3f         , loadmodel->surfmesh.num_vertices * sizeof(float[3]));
		if (loadmodel->surfmesh.data_tvector3f         ) memcpy(mem + loadmodel->surfmesh.vbooffset_tvector3f         , loadmodel->surfmesh.data_tvector3f         , loadmodel->surfmesh.num_vertices * sizeof(float[3]));
		if (loadmodel->surfmesh.data_normal3f          ) memcpy(mem + loadmodel->surfmesh.vbooffset_normal3f          , loadmodel->surfmesh.data_normal3f          , loadmodel->surfmesh.num_vertices * sizeof(float[3]));
		if (loadmodel->surfmesh.data_texcoordtexture2f ) memcpy(mem + loadmodel->surfmesh.vbooffset_texcoordtexture2f , loadmodel->surfmesh.data_texcoordtexture2f , loadmodel->surfmesh.num_vertices * sizeof(float[2]));
		if (loadmodel->surfmesh.data_texcoordlightmap2f) memcpy(mem + loadmodel->surfmesh.vbooffset_texcoordlightmap2f, loadmodel->surfmesh.data_texcoordlightmap2f, loadmodel->surfmesh.num_vertices * sizeof(float[2]));
		if (loadmodel->surfmesh.data_lightmapcolor4f   ) memcpy(mem + loadmodel->surfmesh.vbooffset_lightmapcolor4f   , loadmodel->surfmesh.data_lightmapcolor4f   , loadmodel->surfmesh.num_vertices * sizeof(float[4]));
		loadmodel->surfmesh.vbo_vertexbuffer = R_Mesh_CreateMeshBuffer(mem, size, loadmodel->name, false, false, false);
		Mem_Free(mem);
	}
}

static void Mod_Decompile_OBJ(dp_model_t *model, const char *filename, const char *mtlfilename, const char *originalfilename)
{
	int submodelindex, vertexindex, surfaceindex, triangleindex, textureindex, countvertices = 0, countsurfaces = 0, countfaces = 0, counttextures = 0;
	int a, b, c;
	const char *texname;
	const int *e;
	const float *v, *vn, *vt;
	size_t l;
	size_t outbufferpos = 0;
	size_t outbuffermax = 0x100000;
	char *outbuffer = (char *) Z_Malloc(outbuffermax), *oldbuffer;
	const msurface_t *surface;
	const int maxtextures = 256;
	char *texturenames = (char *) Z_Malloc(maxtextures * MAX_QPATH);
	dp_model_t *submodel;

	// construct the mtllib file
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "# mtllib for %s exported by darkplaces engine\n", originalfilename);
	if (l > 0)
		outbufferpos += l;
	for (surfaceindex = 0, surface = model->data_surfaces;surfaceindex < model->num_surfaces;surfaceindex++, surface++)
	{
		countsurfaces++;
		countvertices += surface->num_vertices;
		countfaces += surface->num_triangles;
		texname = (surface->texture && surface->texture->name[0]) ? surface->texture->name : "default";
		for (textureindex = 0;textureindex < counttextures;textureindex++)
			if (!strcmp(texturenames + textureindex * MAX_QPATH, texname))
				break;
		if (textureindex < counttextures)
			continue; // already wrote this material entry
		if (textureindex >= maxtextures)
			continue; // just a precaution
		textureindex = counttextures++;
		strlcpy(texturenames + textureindex * MAX_QPATH, texname, MAX_QPATH);
		if (outbufferpos >= outbuffermax >> 1)
		{
			outbuffermax *= 2;
			oldbuffer = outbuffer;
			outbuffer = (char *) Z_Malloc(outbuffermax);
			memcpy(outbuffer, oldbuffer, outbufferpos);
			Z_Free(oldbuffer);
		}
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "newmtl %s\nNs 96.078431\nKa 0 0 0\nKd 0.64 0.64 0.64\nKs 0.5 0.5 0.5\nNi 1\nd 1\nillum 2\nmap_Kd %s%s\n\n", texname, texname, strstr(texname, ".tga") ? "" : ".tga");
		if (l > 0)
			outbufferpos += l;
	}

	// write the mtllib file
	FS_WriteFile(mtlfilename, outbuffer, outbufferpos);

	// construct the obj file
	outbufferpos = 0;
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "# model exported from %s by darkplaces engine\n# %i vertices, %i faces, %i surfaces\nmtllib %s\n", originalfilename, countvertices, countfaces, countsurfaces, mtlfilename);
	if (l > 0)
		outbufferpos += l;

	for (vertexindex = 0, v = model->surfmesh.data_vertex3f, vn = model->surfmesh.data_normal3f, vt = model->surfmesh.data_texcoordtexture2f;vertexindex < model->surfmesh.num_vertices;vertexindex++, v += 3, vn += 3, vt += 2)
	{
		if (outbufferpos >= outbuffermax >> 1)
		{
			outbuffermax *= 2;
			oldbuffer = outbuffer;
			outbuffer = (char *) Z_Malloc(outbuffermax);
			memcpy(outbuffer, oldbuffer, outbufferpos);
			Z_Free(oldbuffer);
		}
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "v %f %f %f\nvn %f %f %f\nvt %f %f\n", v[0], v[2], v[1], vn[0], vn[2], vn[1], vt[0], 1-vt[1]);
		if (l > 0)
			outbufferpos += l;
	}

	for (submodelindex = 0;submodelindex < max(1, model->brush.numsubmodels);submodelindex++)
	{
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "o %i\n", submodelindex);
		if (l > 0)
			outbufferpos += l;
		submodel = model->brush.numsubmodels ? model->brush.submodels[submodelindex] : model;
		for (surfaceindex = 0;surfaceindex < submodel->nummodelsurfaces;surfaceindex++)
		{
			surface = model->data_surfaces + submodel->sortedmodelsurfaces[surfaceindex];
			l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "usemtl %s\n", (surface->texture && surface->texture->name[0]) ? surface->texture->name : "default");
			if (l > 0)
				outbufferpos += l;
			for (triangleindex = 0, e = model->surfmesh.data_element3i + surface->num_firsttriangle * 3;triangleindex < surface->num_triangles;triangleindex++, e += 3)
			{
				if (outbufferpos >= outbuffermax >> 1)
				{
					outbuffermax *= 2;
					oldbuffer = outbuffer;
					outbuffer = (char *) Z_Malloc(outbuffermax);
					memcpy(outbuffer, oldbuffer, outbufferpos);
					Z_Free(oldbuffer);
				}
				a = e[0]+1;
				b = e[1]+1;
				c = e[2]+1;
				l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", a,a,a,b,b,b,c,c,c);
				if (l > 0)
					outbufferpos += l;
			}
		}
	}

	// write the obj file
	FS_WriteFile(filename, outbuffer, outbufferpos);

	// clean up
	Z_Free(outbuffer);
	Z_Free(texturenames);

	// print some stats
	Con_Printf("Wrote %s (%i bytes, %i vertices, %i faces, %i surfaces with %i distinct textures)\n", filename, (int)outbufferpos, countvertices, countfaces, countsurfaces, counttextures);
}

static void Mod_Decompile_SMD(dp_model_t *model, const char *filename, int firstpose, int numposes, qboolean writetriangles)
{
	int countnodes = 0, counttriangles = 0, countframes = 0;
	int surfaceindex;
	int triangleindex;
	int transformindex;
	int poseindex;
	int cornerindex;
	const int *e;
	size_t l;
	size_t outbufferpos = 0;
	size_t outbuffermax = 0x100000;
	char *outbuffer = (char *) Z_Malloc(outbuffermax), *oldbuffer;
	const msurface_t *surface;
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "version 1\nnodes\n");
	if (l > 0)
		outbufferpos += l;
	for (transformindex = 0;transformindex < model->num_bones;transformindex++)
	{
		if (outbufferpos >= outbuffermax >> 1)
		{
			outbuffermax *= 2;
			oldbuffer = outbuffer;
			outbuffer = (char *) Z_Malloc(outbuffermax);
			memcpy(outbuffer, oldbuffer, outbufferpos);
			Z_Free(oldbuffer);
		}
		countnodes++;
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i \"%s\" %3i\n", transformindex, model->data_bones[transformindex].name, model->data_bones[transformindex].parent);
		if (l > 0)
			outbufferpos += l;
	}
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "end\nskeleton\n");
	if (l > 0)
		outbufferpos += l;
	for (poseindex = 0;poseindex < numposes;poseindex++)
	{
		countframes++;
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "time %i\n", poseindex);
		if (l > 0)
			outbufferpos += l;
		for (transformindex = 0;transformindex < model->num_bones;transformindex++)
		{
			float angles[3];
			float mtest[4][3];
			matrix4x4_t posematrix;
			if (outbufferpos >= outbuffermax >> 1)
			{
				outbuffermax *= 2;
				oldbuffer = outbuffer;
				outbuffer = (char *) Z_Malloc(outbuffermax);
				memcpy(outbuffer, oldbuffer, outbufferpos);
				Z_Free(oldbuffer);
			}

			// strangely the smd angles are for a transposed matrix, so we
			// have to generate a transposed matrix, then convert that...
			Matrix4x4_FromBonePose6s(&posematrix, model->num_posescale, model->data_poses6s + 6*(model->num_bones * poseindex + transformindex));
			Matrix4x4_ToArray12FloatGL(&posematrix, mtest[0]);
			AnglesFromVectors(angles, mtest[0], mtest[2], false);
			if (angles[0] >= 180) angles[0] -= 360;
			if (angles[1] >= 180) angles[1] -= 360;
			if (angles[2] >= 180) angles[2] -= 360;

#if 0
{
			float a = DEG2RAD(angles[ROLL]);
			float b = DEG2RAD(angles[PITCH]);
			float c = DEG2RAD(angles[YAW]);
			float cy, sy, cp, sp, cr, sr;
			float test[4][3];
			// smd matrix construction, for comparing
			sy = sin(c);
			cy = cos(c);
			sp = sin(b);
			cp = cos(b);
			sr = sin(a);
			cr = cos(a);

			test[0][0] = cp*cy;
			test[0][1] = cp*sy;
			test[0][2] = -sp;
			test[1][0] = sr*sp*cy+cr*-sy;
			test[1][1] = sr*sp*sy+cr*cy;
			test[1][2] = sr*cp;
			test[2][0] = (cr*sp*cy+-sr*-sy);
			test[2][1] = (cr*sp*sy+-sr*cy);
			test[2][2] = cr*cp;
			test[3][0] = pose[9];
			test[3][1] = pose[10];
			test[3][2] = pose[11];
}
#endif
			l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f\n", transformindex, mtest[3][0], mtest[3][1], mtest[3][2], DEG2RAD(angles[ROLL]), DEG2RAD(angles[PITCH]), DEG2RAD(angles[YAW]));
			if (l > 0)
				outbufferpos += l;
		}
	}
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "end\n");
	if (l > 0)
		outbufferpos += l;
	if (writetriangles)
	{
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "triangles\n");
		if (l > 0)
			outbufferpos += l;
		for (surfaceindex = 0, surface = model->data_surfaces;surfaceindex < model->num_surfaces;surfaceindex++, surface++)
		{
			for (triangleindex = 0, e = model->surfmesh.data_element3i + surface->num_firsttriangle * 3;triangleindex < surface->num_triangles;triangleindex++, e += 3)
			{
				counttriangles++;
				if (outbufferpos >= outbuffermax >> 1)
				{
					outbuffermax *= 2;
					oldbuffer = outbuffer;
					outbuffer = (char *) Z_Malloc(outbuffermax);
					memcpy(outbuffer, oldbuffer, outbufferpos);
					Z_Free(oldbuffer);
				}
				l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%s\n", surface->texture && surface->texture->name[0] ? surface->texture->name : "default.bmp");
				if (l > 0)
					outbufferpos += l;
				for (cornerindex = 0;cornerindex < 3;cornerindex++)
				{
					const int index = e[2-cornerindex];
					const float *v = model->surfmesh.data_vertex3f + index * 3;
					const float *vn = model->surfmesh.data_normal3f + index * 3;
					const float *vt = model->surfmesh.data_texcoordtexture2f + index * 2;
					const int b = model->surfmesh.blends[index];
					if (b < model->num_bones)
						l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f\n"                          , b, v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1]);
					else
					{
						const blendweights_t *w = model->surfmesh.data_blendweights + b - model->num_bones;
						const unsigned char *wi = w->index;
						const unsigned char *wf = w->influence;
					    if (wf[3]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 4 %i %f %i %f %i %f %i %f\n", wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0]/255.0f, wi[1], wf[1]/255.0f, wi[2], wf[2]/255.0f, wi[3], wf[3]/255.0f);
						else if (wf[2]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 3 %i %f %i %f %i %f\n"      , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0]/255.0f, wi[1], wf[1]/255.0f, wi[2], wf[2]/255.0f);
						else if (wf[1]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 2 %i %f %i %f\n"            , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0]/255.0f, wi[1], wf[1]/255.0f);
						else            l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f\n"                          , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1]);
					}
					if (l > 0)
						outbufferpos += l;
				}
			}
		}
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "end\n");
		if (l > 0)
			outbufferpos += l;
	}

	FS_WriteFile(filename, outbuffer, outbufferpos);
	Z_Free(outbuffer);

	Con_Printf("Wrote %s (%i bytes, %i nodes, %i frames, %i triangles)\n", filename, (int)outbufferpos, countnodes, countframes, counttriangles);
}

/*
================
Mod_Decompile_f

decompiles a model to editable files
================
*/
static void Mod_Decompile_f(void)
{
	int i, j, k, l, first, count;
	dp_model_t *mod;
	char inname[MAX_QPATH];
	char outname[MAX_QPATH];
	char mtlname[MAX_QPATH];
	char basename[MAX_QPATH];
	char animname[MAX_QPATH];
	char animname2[MAX_QPATH];
	char zymtextbuffer[16384];
	char dpmtextbuffer[16384];
	char framegroupstextbuffer[16384];
	int zymtextsize = 0;
	int dpmtextsize = 0;
	int framegroupstextsize = 0;

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: modeldecompile <filename>\n");
		return;
	}

	strlcpy(inname, Cmd_Argv(1), sizeof(inname));
	FS_StripExtension(inname, basename, sizeof(basename));

	mod = Mod_ForName(inname, false, true, inname[0] == '*' ? cl.model_name[1] : NULL);
	if (mod->brush.submodel)
	{
		// if we're decompiling a submodel, be sure to give it a proper name based on its parent
		FS_StripExtension(cl.model_name[1], outname, sizeof(outname));
		dpsnprintf(basename, sizeof(basename), "%s/%s", outname, mod->name);
		outname[0] = 0;
	}
	if (!mod)
	{
		Con_Print("No such model\n");
		return;
	}
	if (!mod->surfmesh.num_triangles)
	{
		Con_Print("Empty model (or sprite)\n");
		return;
	}

	// export OBJ if possible (not on sprites)
	if (mod->surfmesh.num_triangles)
	{
		dpsnprintf(outname, sizeof(outname), "%s_decompiled.obj", basename);
		dpsnprintf(mtlname, sizeof(mtlname), "%s_decompiled.mtl", basename);
		Mod_Decompile_OBJ(mod, outname, mtlname, inname);
	}

	// export SMD if possible (only for skeletal models)
	if (mod->surfmesh.num_triangles && mod->num_bones)
	{
		dpsnprintf(outname, sizeof(outname), "%s_decompiled/ref1.smd", basename);
		Mod_Decompile_SMD(mod, outname, 0, 1, true);
		l = dpsnprintf(zymtextbuffer + zymtextsize, sizeof(zymtextbuffer) - zymtextsize, "output out.zym\nscale 1\norigin 0 0 0\nmesh ref1.smd\n");
		if (l > 0) zymtextsize += l;
		l = dpsnprintf(dpmtextbuffer + dpmtextsize, sizeof(dpmtextbuffer) - dpmtextsize, "outputdir .\nmodel out\nscale 1\norigin 0 0 0\nscene ref1.smd\n");
		if (l > 0) dpmtextsize += l;
		for (i = 0;i < mod->numframes;i = j)
		{
			strlcpy(animname, mod->animscenes[i].name, sizeof(animname));
			first = mod->animscenes[i].firstframe;
			if (mod->animscenes[i].framecount > 1)
			{
				// framegroup anim
				count = mod->animscenes[i].framecount;
				j = i + 1;
			}
			else
			{
				// individual frame
				// check for additional frames with same name
				for (l = 0, k = strlen(animname);animname[l];l++)
					if(animname[l] < '0' || animname[l] > '9')
						k = l + 1;
				if(k > 0 && animname[k-1] == '_')
					--k;
				animname[k] = 0;
				count = mod->num_poses - first;
				for (j = i + 1;j < mod->numframes;j++)
				{
					strlcpy(animname2, mod->animscenes[j].name, sizeof(animname2));
					for (l = 0, k = strlen(animname2);animname2[l];l++)
						if(animname2[l] < '0' || animname2[l] > '9')
							k = l + 1;
					if(k > 0 && animname[k-1] == '_')
						--k;
					animname2[k] = 0;
					if (strcmp(animname2, animname) || mod->animscenes[j].framecount > 1)
					{
						count = mod->animscenes[j].firstframe - first;
						break;
					}
				}
				// if it's only one frame, use the original frame name
				if (j == i + 1)
					strlcpy(animname, mod->animscenes[i].name, sizeof(animname));
				
			}
			dpsnprintf(outname, sizeof(outname), "%s_decompiled/%s.smd", basename, animname);
			Mod_Decompile_SMD(mod, outname, first, count, false);
			if (zymtextsize < (int)sizeof(zymtextbuffer) - 100)
			{
				l = dpsnprintf(zymtextbuffer + zymtextsize, sizeof(zymtextbuffer) - zymtextsize, "scene %s.smd fps %g %s\n", animname, mod->animscenes[i].framerate, mod->animscenes[i].loop ? "" : " noloop");
				if (l > 0) zymtextsize += l;
			}
			if (dpmtextsize < (int)sizeof(dpmtextbuffer) - 100)
			{
				l = dpsnprintf(dpmtextbuffer + dpmtextsize, sizeof(dpmtextbuffer) - dpmtextsize, "scene %s.smd fps %g %s\n", animname, mod->animscenes[i].framerate, mod->animscenes[i].loop ? "" : " noloop");
				if (l > 0) dpmtextsize += l;
			}
			if (framegroupstextsize < (int)sizeof(framegroupstextbuffer) - 100)
			{
				l = dpsnprintf(framegroupstextbuffer + framegroupstextsize, sizeof(framegroupstextbuffer) - framegroupstextsize, "%d %d %f %d // %s\n", first, count, mod->animscenes[i].framerate, mod->animscenes[i].loop, animname);
				if (l > 0) framegroupstextsize += l;
			}
		}
		if (zymtextsize)
			FS_WriteFile(va("%s_decompiled/out_zym.txt", basename), zymtextbuffer, (fs_offset_t)zymtextsize);
		if (dpmtextsize)
			FS_WriteFile(va("%s_decompiled/out_dpm.txt", basename), dpmtextbuffer, (fs_offset_t)dpmtextsize);
		if (framegroupstextsize)
			FS_WriteFile(va("%s_decompiled.framegroups", basename), framegroupstextbuffer, (fs_offset_t)framegroupstextsize);
	}
}

void Mod_AllocLightmap_Init(mod_alloclightmap_state_t *state, int width, int height)
{
	int y;
	memset(state, 0, sizeof(*state));
	state->width = width;
	state->height = height;
	state->currentY = 0;
	state->rows = (mod_alloclightmap_row_t *)Mem_Alloc(loadmodel->mempool, state->height * sizeof(*state->rows));
	for (y = 0;y < state->height;y++)
	{
		state->rows[y].currentX = 0;
		state->rows[y].rowY = -1;
	}
}

void Mod_AllocLightmap_Reset(mod_alloclightmap_state_t *state)
{
	int y;
	state->currentY = 0;
	for (y = 0;y < state->height;y++)
	{
		state->rows[y].currentX = 0;
		state->rows[y].rowY = -1;
	}
}

void Mod_AllocLightmap_Free(mod_alloclightmap_state_t *state)
{
	if (state->rows)
		Mem_Free(state->rows);
	memset(state, 0, sizeof(*state));
}

qboolean Mod_AllocLightmap_Block(mod_alloclightmap_state_t *state, int blockwidth, int blockheight, int *outx, int *outy)
{
	mod_alloclightmap_row_t *row;
	int y;

	row = state->rows + blockheight;
	if ((row->rowY < 0) || (row->currentX + blockwidth > state->width))
	{
		if (state->currentY + blockheight <= state->height)
		{
			// use the current allocation position
			row->rowY = state->currentY;
			row->currentX = 0;
			state->currentY += blockheight;
		}
		else
		{
			// find another position
			for (y = blockheight;y < state->height;y++)
			{
				if ((state->rows[y].rowY >= 0) && (state->rows[y].currentX + blockwidth <= state->width))
				{
					row = state->rows + y;
					break;
				}
			}
			if (y == state->height)
				return false;
		}
	}
	*outy = row->rowY;
	*outx = row->currentX;
	row->currentX += blockwidth;

	return true;
}

typedef struct lightmapsample_s
{
	float pos[3];
	float sh1[4][3];
	float *vertex_color;
	unsigned char *lm_bgr;
	unsigned char *lm_dir;
}
lightmapsample_t;

typedef struct lightmapvertex_s
{
	int index;
	float pos[3];
	float normal[3];
	float texcoordbase[2];
	float texcoordlightmap[2];
	float lightcolor[4];
}
lightmapvertex_t;

typedef struct lightmaptriangle_s
{
	int triangleindex;
	int surfaceindex;
	int lightmapindex;
	int axis;
	int lmoffset[2];
	int lmsize[2];
	// 2D modelspace coordinates of min corner
	// snapped to lightmap grid but not in grid coordinates
	float lmbase[2];
	// 2D modelspace to lightmap coordinate scale
	float lmscale[2];
	float vertex[3][3];
	float mins[3];
	float maxs[3];
}
lightmaptriangle_t;

typedef struct lightmaplight_s
{
	float origin[3];
	float radius;
	float iradius;
	float radius2;
	float color[3];
	svbsp_t svbsp;
}
lightmaplight_t;

lightmaptriangle_t *mod_generatelightmaps_lightmaptriangles;

#define MAX_LIGHTMAPSAMPLES 64
static int mod_generatelightmaps_numoffsets[3];
static float mod_generatelightmaps_offsets[3][MAX_LIGHTMAPSAMPLES][3];

static int mod_generatelightmaps_numlights;
static lightmaplight_t *mod_generatelightmaps_lightinfo;

extern int R_Shadow_GetRTLightInfo(unsigned int lightindex, float *origin, float *radius, float *color);
extern cvar_t r_shadow_lightattenuationdividebias;
extern cvar_t r_shadow_lightattenuationlinearscale;

static void Mod_GenerateLightmaps_LightPoint(dp_model_t *model, const vec3_t pos, vec3_t ambient, vec3_t diffuse, vec3_t lightdir)
{
	int i;
	int index;
	int result;
	float relativepoint[3];
	float color[3];
	float dir[3];
	float dist;
	float dist2;
	float intensity;
	float sample[5*3];
	float lightorigin[3];
	float lightradius;
	float lightradius2;
	float lightiradius;
	float lightcolor[3];
	trace_t trace;
	for (i = 0;i < 5*3;i++)
		sample[i] = 0.0f;
	for (index = 0;;index++)
	{
		result = R_Shadow_GetRTLightInfo(index, lightorigin, &lightradius, lightcolor);
		if (result < 0)
			break;
		if (result == 0)
			continue;
		lightradius2 = lightradius * lightradius;
		VectorSubtract(lightorigin, pos, relativepoint);
		dist2 = VectorLength2(relativepoint);
		if (dist2 >= lightradius2)
			continue;
		lightiradius = 1.0f / lightradius;
		dist = sqrt(dist2) * lightiradius;
		intensity = (1.0f - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist);
		if (intensity <= 0.0f)
			continue;
		if (model && model->TraceLine)
		{
			model->TraceLine(model, NULL, NULL, &trace, pos, lightorigin, SUPERCONTENTS_VISBLOCKERMASK);
			if (trace.fraction < 1)
				continue;
		}
		// scale down intensity to add to both ambient and diffuse
		//intensity *= 0.5f;
		VectorNormalize(relativepoint);
		VectorScale(lightcolor, intensity, color);
		VectorMA(sample    , 0.5f            , color, sample    );
		VectorMA(sample + 3, relativepoint[0], color, sample + 3);
		VectorMA(sample + 6, relativepoint[1], color, sample + 6);
		VectorMA(sample + 9, relativepoint[2], color, sample + 9);
		// calculate a weighted average light direction as well
		intensity *= VectorLength(color);
		VectorMA(sample + 12, intensity, relativepoint, sample + 12);
	}
	// calculate the direction we'll use to reduce the sample to a directional light source
	VectorCopy(sample + 12, dir);
	//VectorSet(dir, sample[3] + sample[4] + sample[5], sample[6] + sample[7] + sample[8], sample[9] + sample[10] + sample[11]);
	VectorNormalize(dir);
	// extract the diffuse color along the chosen direction and scale it
	diffuse[0] = (dir[0]*sample[3] + dir[1]*sample[6] + dir[2]*sample[ 9] + sample[ 0]);
	diffuse[1] = (dir[0]*sample[4] + dir[1]*sample[7] + dir[2]*sample[10] + sample[ 1]);
	diffuse[2] = (dir[0]*sample[5] + dir[1]*sample[8] + dir[2]*sample[11] + sample[ 2]);
	// subtract some of diffuse from ambient
	VectorMA(sample, -0.333f, diffuse, ambient);
	// store the normalized lightdir
	VectorCopy(dir, lightdir);
}

static void Mod_GenerateLightmaps_CreateLights_ComputeSVBSP_InsertSurfaces(const dp_model_t *model, svbsp_t *svbsp, const float *mins, const float *maxs)
{
	int surfaceindex;
	int triangleindex;
	const msurface_t *surface;
	const float *vertex3f = model->surfmesh.data_vertex3f;
	const int *element3i = model->surfmesh.data_element3i;
	const int *e;
	float v2[3][3];
	for (surfaceindex = 0, surface = model->data_surfaces;surfaceindex < model->nummodelsurfaces;surfaceindex++, surface++)
	{
		if (!BoxesOverlap(surface->mins, surface->maxs, mins, maxs))
			continue;
		if (surface->texture->basematerialflags & MATERIALFLAG_NOSHADOW)
			continue;
		for (triangleindex = 0, e = element3i + 3*surface->num_firsttriangle;triangleindex < surface->num_triangles;triangleindex++, e += 3)
		{
			VectorCopy(vertex3f + 3*e[0], v2[0]);
			VectorCopy(vertex3f + 3*e[1], v2[1]);
			VectorCopy(vertex3f + 3*e[2], v2[2]);
			SVBSP_AddPolygon(svbsp, 3, v2[0], true, NULL, NULL, 0);
		}
	}
}

static void Mod_GenerateLightmaps_CreateLights_ComputeSVBSP(dp_model_t *model, lightmaplight_t *lightinfo)
{
	int maxnodes = 1<<14;
	svbsp_node_t *nodes;
	float origin[3];
	float mins[3];
	float maxs[3];
	svbsp_t svbsp;
	VectorSet(mins, lightinfo->origin[0] - lightinfo->radius, lightinfo->origin[1] - lightinfo->radius, lightinfo->origin[2] - lightinfo->radius);
	VectorSet(maxs, lightinfo->origin[0] + lightinfo->radius, lightinfo->origin[1] + lightinfo->radius, lightinfo->origin[2] + lightinfo->radius);
	VectorCopy(lightinfo->origin, origin);
	nodes = (svbsp_node_t *)Mem_Alloc(tempmempool, maxnodes * sizeof(*nodes));
	for (;;)
	{
		SVBSP_Init(&svbsp, origin, maxnodes, nodes);
		Mod_GenerateLightmaps_CreateLights_ComputeSVBSP_InsertSurfaces(model, &svbsp, mins, maxs);
		if (svbsp.ranoutofnodes)
		{
			maxnodes *= 16;
			if (maxnodes > 1<<22)
			{
				Mem_Free(nodes);
				return;
			}
			Mem_Free(nodes);
			nodes = (svbsp_node_t *)Mem_Alloc(tempmempool, maxnodes * sizeof(*nodes));
		}
		else
			break;
	}
	if (svbsp.numnodes > 0)
	{
		svbsp.nodes = (svbsp_node_t *)Mem_Alloc(tempmempool, svbsp.numnodes * sizeof(*nodes));
		memcpy(svbsp.nodes, nodes, svbsp.numnodes * sizeof(*nodes));
		lightinfo->svbsp = svbsp;
	}
	Mem_Free(nodes);
}

static void Mod_GenerateLightmaps_CreateLights(dp_model_t *model)
{
	int index;
	int result;
	lightmaplight_t *lightinfo;
	float origin[3];
	float radius;
	float color[3];
	mod_generatelightmaps_numlights = 0;
	for (index = 0;;index++)
	{
		result = R_Shadow_GetRTLightInfo(index, origin, &radius, color);
		if (result < 0)
			break;
		if (result > 0)
			mod_generatelightmaps_numlights++;
	}
	if (mod_generatelightmaps_numlights > 0)
	{
		mod_generatelightmaps_lightinfo = (lightmaplight_t *)Mem_Alloc(tempmempool, mod_generatelightmaps_numlights * sizeof(*mod_generatelightmaps_lightinfo));
		lightinfo = mod_generatelightmaps_lightinfo;
		for (index = 0;;index++)
		{
			result = R_Shadow_GetRTLightInfo(index, lightinfo->origin, &lightinfo->radius, lightinfo->color);
			if (result < 0)
				break;
			if (result > 0)
				lightinfo++;
		}
	}
	for (index = 0, lightinfo = mod_generatelightmaps_lightinfo;index < mod_generatelightmaps_numlights;index++, lightinfo++)
	{
		lightinfo->iradius = 1.0f / lightinfo->radius;
		lightinfo->radius2 = lightinfo->radius * lightinfo->radius;
		// TODO: compute svbsp
		Mod_GenerateLightmaps_CreateLights_ComputeSVBSP(model, lightinfo);
	}
}

static void Mod_GenerateLightmaps_DestroyLights(dp_model_t *model)
{
	int i;
	if (mod_generatelightmaps_lightinfo)
	{
		for (i = 0;i < mod_generatelightmaps_numlights;i++)
			if (mod_generatelightmaps_lightinfo[i].svbsp.nodes)
				Mem_Free(mod_generatelightmaps_lightinfo[i].svbsp.nodes);
		Mem_Free(mod_generatelightmaps_lightinfo);
	}
	mod_generatelightmaps_lightinfo = NULL;
	mod_generatelightmaps_numlights = 0;
}

static qboolean Mod_GenerateLightmaps_SamplePoint_SVBSP(const svbsp_t *svbsp, const float *pos)
{
	const svbsp_node_t *node;
	const svbsp_node_t *nodes = svbsp->nodes;
	int num = 0;
	while (num >= 0)
	{
		node = nodes + num;
		num = node->children[DotProduct(node->plane, pos) < node->plane[3]];
	}
	return num == -1; // true if empty, false if solid (shadowed)
}

static void Mod_GenerateLightmaps_SamplePoint(const float *pos, const float *normal, float *sample, int numoffsets, const float *offsets)
{
	int i;
	float relativepoint[3];
	float color[3];
	float offsetpos[3];
	float dist;
	float dist2;
	float intensity;
	int offsetindex;
	int hits;
	int tests;
	const lightmaplight_t *lightinfo;
	trace_t trace;
	for (i = 0;i < 5*3;i++)
		sample[i] = 0.0f;
	for (i = 0, lightinfo = mod_generatelightmaps_lightinfo;i < mod_generatelightmaps_numlights;i++, lightinfo++)
	{
		//R_SampleRTLights(pos, sample, numoffsets, offsets);
		VectorSubtract(lightinfo->origin, pos, relativepoint);
		// don't accept light from behind a surface, it causes bad shading
		if (normal && DotProduct(relativepoint, normal) <= 0)
			continue;
		dist2 = VectorLength2(relativepoint);
		if (dist2 >= lightinfo->radius2)
			continue;
		dist = sqrt(dist2) * lightinfo->iradius;
		intensity = dist < 1 ? ((1.0f - dist) * r_shadow_lightattenuationlinearscale.value / (r_shadow_lightattenuationdividebias.value + dist*dist)) : 0;
		if (intensity <= 0)
			continue;
		if (cl.worldmodel && cl.worldmodel->TraceLine && numoffsets > 0)
		{
			hits = 0;
			tests = 1;
			if (Mod_GenerateLightmaps_SamplePoint_SVBSP(&lightinfo->svbsp, pos))
				hits++;
			for (offsetindex = 1;offsetindex < numoffsets;offsetindex++)
			{
				VectorAdd(pos, offsets + 3*offsetindex, offsetpos);
				if (!normal)
				{
					// for light grid we'd better check visibility of the offset point
					cl.worldmodel->TraceLine(cl.worldmodel, NULL, NULL, &trace, pos, offsetpos, SUPERCONTENTS_VISBLOCKERMASK);
					if (trace.fraction < 1)
						VectorLerp(pos, trace.fraction, offsetpos, offsetpos);
				}
				tests++;
				if (Mod_GenerateLightmaps_SamplePoint_SVBSP(&lightinfo->svbsp, offsetpos))
					hits++;
			}
			if (!hits)
				continue;
			// scale intensity according to how many rays succeeded
			// we know one test is valid, half of the rest will fail...
			//if (normal && tests > 1)
			//	intensity *= (tests - 1.0f) / tests;
			intensity *= (float)hits / tests;
		}
		// scale down intensity to add to both ambient and diffuse
		//intensity *= 0.5f;
		VectorNormalize(relativepoint);
		VectorScale(lightinfo->color, intensity, color);
		VectorMA(sample    , 0.5f            , color, sample    );
		VectorMA(sample + 3, relativepoint[0], color, sample + 3);
		VectorMA(sample + 6, relativepoint[1], color, sample + 6);
		VectorMA(sample + 9, relativepoint[2], color, sample + 9);
		// calculate a weighted average light direction as well
		intensity *= VectorLength(color);
		VectorMA(sample + 12, intensity, relativepoint, sample + 12);
	}
}

static void Mod_GenerateLightmaps_LightmapSample(const float *pos, const float *normal, unsigned char *lm_bgr, unsigned char *lm_dir)
{
	float sample[5*3];
	float color[3];
	float dir[3];
	float f;
	Mod_GenerateLightmaps_SamplePoint(pos, normal, sample, mod_generatelightmaps_numoffsets[0], mod_generatelightmaps_offsets[0][0]);
	//VectorSet(dir, sample[3] + sample[4] + sample[5], sample[6] + sample[7] + sample[8], sample[9] + sample[10] + sample[11]);
	VectorCopy(sample + 12, dir);
	VectorNormalize(dir);
	//VectorAdd(dir, normal, dir);
	//VectorNormalize(dir);
	f = DotProduct(dir, normal);
	f = max(0, f) * 255.0f;
	VectorScale(sample, f, color);
	//VectorCopy(normal, dir);
	VectorSet(dir, (dir[0]+1.0f)*127.5f, (dir[1]+1.0f)*127.5f, (dir[2]+1.0f)*127.5f);
	lm_bgr[0] = (unsigned char)bound(0.0f, color[2], 255.0f);
	lm_bgr[1] = (unsigned char)bound(0.0f, color[1], 255.0f);
	lm_bgr[2] = (unsigned char)bound(0.0f, color[0], 255.0f);
	lm_bgr[3] = 255;
	lm_dir[0] = (unsigned char)dir[2];
	lm_dir[1] = (unsigned char)dir[1];
	lm_dir[2] = (unsigned char)dir[0];
	lm_dir[3] = 255;
}

static void Mod_GenerateLightmaps_VertexSample(const float *pos, const float *normal, float *vertex_color)
{
	float sample[5*3];
	Mod_GenerateLightmaps_SamplePoint(pos, normal, sample, mod_generatelightmaps_numoffsets[1], mod_generatelightmaps_offsets[1][0]);
	VectorCopy(sample, vertex_color);
}

static void Mod_GenerateLightmaps_GridSample(const float *pos, q3dlightgrid_t *s)
{
	float sample[5*3];
	float ambient[3];
	float diffuse[3];
	float dir[3];
	Mod_GenerateLightmaps_SamplePoint(pos, NULL, sample, mod_generatelightmaps_numoffsets[2], mod_generatelightmaps_offsets[2][0]);
	// calculate the direction we'll use to reduce the sample to a directional light source
	VectorCopy(sample + 12, dir);
	//VectorSet(dir, sample[3] + sample[4] + sample[5], sample[6] + sample[7] + sample[8], sample[9] + sample[10] + sample[11]);
	VectorNormalize(dir);
	// extract the diffuse color along the chosen direction and scale it
	diffuse[0] = (dir[0]*sample[3] + dir[1]*sample[6] + dir[2]*sample[ 9] + sample[ 0]) * 127.5f;
	diffuse[1] = (dir[0]*sample[4] + dir[1]*sample[7] + dir[2]*sample[10] + sample[ 1]) * 127.5f;
	diffuse[2] = (dir[0]*sample[5] + dir[1]*sample[8] + dir[2]*sample[11] + sample[ 2]) * 127.5f;
	// scale the ambient from 0-2 to 0-255 and subtract some of diffuse
	VectorScale(sample, 127.5f, ambient);
	VectorMA(ambient, -0.333f, diffuse, ambient);
	// encode to the grid format
	s->ambientrgb[0] = (unsigned char)bound(0.0f, ambient[0], 255.0f);
	s->ambientrgb[1] = (unsigned char)bound(0.0f, ambient[1], 255.0f);
	s->ambientrgb[2] = (unsigned char)bound(0.0f, ambient[2], 255.0f);
	s->diffusergb[0] = (unsigned char)bound(0.0f, diffuse[0], 255.0f);
	s->diffusergb[1] = (unsigned char)bound(0.0f, diffuse[1], 255.0f);
	s->diffusergb[2] = (unsigned char)bound(0.0f, diffuse[2], 255.0f);
	if (dir[2] >= 0.99f) {s->diffusepitch = 0;s->diffuseyaw = 0;}
	else if (dir[2] <= -0.99f) {s->diffusepitch = 128;s->diffuseyaw = 0;}
	else {s->diffusepitch = (unsigned char)(acos(dir[2]) * (127.5f/M_PI));s->diffuseyaw = (unsigned char)(atan2(dir[1], dir[0]) * (127.5f/M_PI));}
}

static void Mod_GenerateLightmaps_InitSampleOffsets(dp_model_t *model)
{
	float radius[3];
	float temp[3];
	int i, j;
	memset(mod_generatelightmaps_offsets, 0, sizeof(mod_generatelightmaps_offsets));
	mod_generatelightmaps_numoffsets[0] = min(MAX_LIGHTMAPSAMPLES, mod_generatelightmaps_lightmapsamples.integer);
	mod_generatelightmaps_numoffsets[1] = min(MAX_LIGHTMAPSAMPLES, mod_generatelightmaps_vertexsamples.integer);
	mod_generatelightmaps_numoffsets[2] = min(MAX_LIGHTMAPSAMPLES, mod_generatelightmaps_gridsamples.integer);
	radius[0] = mod_generatelightmaps_lightmapradius.value;
	radius[1] = mod_generatelightmaps_vertexradius.value;
	radius[2] = mod_generatelightmaps_gridradius.value;
	for (i = 0;i < 3;i++)
	{
		for (j = 1;j < mod_generatelightmaps_numoffsets[i];j++)
		{
			VectorRandom(temp);
			VectorScale(temp, radius[i], mod_generatelightmaps_offsets[i][j]);
		}
	}
}

static void Mod_GenerateLightmaps_DestroyLightmaps(dp_model_t *model)
{
	msurface_t *surface;
	int surfaceindex;
	int i;
	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		surface->lightmaptexture = NULL;
		surface->deluxemaptexture = NULL;
	}
	if (model->brushq3.data_lightmaps)
	{
		for (i = 0;i < model->brushq3.num_mergedlightmaps;i++)
			if (model->brushq3.data_lightmaps[i])
				R_FreeTexture(model->brushq3.data_lightmaps[i]);
		Mem_Free(model->brushq3.data_lightmaps);
		model->brushq3.data_lightmaps = NULL;
	}
	if (model->brushq3.data_deluxemaps)
	{
		for (i = 0;i < model->brushq3.num_mergedlightmaps;i++)
			if (model->brushq3.data_deluxemaps[i])
				R_FreeTexture(model->brushq3.data_deluxemaps[i]);
		Mem_Free(model->brushq3.data_deluxemaps);
		model->brushq3.data_deluxemaps = NULL;
	}
}

static void Mod_GenerateLightmaps_UnweldTriangles(dp_model_t *model)
{
	msurface_t *surface;
	int surfaceindex;
	int vertexindex;
	int outvertexindex;
	int i;
	const int *e;
	surfmesh_t oldsurfmesh;
	size_t size;
	unsigned char *data;
	oldsurfmesh = model->surfmesh;
	model->surfmesh.num_triangles = oldsurfmesh.num_triangles;
	model->surfmesh.num_vertices = oldsurfmesh.num_triangles * 3;
	size = 0;
	size += model->surfmesh.num_vertices * sizeof(float[3]);
	size += model->surfmesh.num_vertices * sizeof(float[3]);
	size += model->surfmesh.num_vertices * sizeof(float[3]);
	size += model->surfmesh.num_vertices * sizeof(float[3]);
	size += model->surfmesh.num_vertices * sizeof(float[2]);
	size += model->surfmesh.num_vertices * sizeof(float[2]);
	size += model->surfmesh.num_vertices * sizeof(float[4]);
	data = (unsigned char *)Mem_Alloc(model->mempool, size);
	model->surfmesh.data_vertex3f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[3]);
	model->surfmesh.data_normal3f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[3]);
	model->surfmesh.data_svector3f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[3]);
	model->surfmesh.data_tvector3f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[3]);
	model->surfmesh.data_texcoordtexture2f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[2]);
	model->surfmesh.data_texcoordlightmap2f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[2]);
	model->surfmesh.data_lightmapcolor4f = (float *)data;data += model->surfmesh.num_vertices * sizeof(float[4]);
	if (model->surfmesh.num_vertices > 65536)
		model->surfmesh.data_element3s = NULL;

	if (model->surfmesh.vertexmesh)
		Mem_Free(model->surfmesh.vertexmesh);
	model->surfmesh.vertexmesh = NULL;
	if (model->surfmesh.vertex3fbuffer)
		R_Mesh_DestroyMeshBuffer(model->surfmesh.vertex3fbuffer);
	model->surfmesh.vertex3fbuffer = NULL;
	if (model->surfmesh.vertexmeshbuffer)
		R_Mesh_DestroyMeshBuffer(model->surfmesh.vertexmeshbuffer);
	model->surfmesh.vertexmeshbuffer = NULL;
	if (model->surfmesh.data_element3i_indexbuffer)
		R_Mesh_DestroyMeshBuffer(model->surfmesh.data_element3i_indexbuffer);
	model->surfmesh.data_element3i_indexbuffer = NULL;
	if (model->surfmesh.data_element3s_indexbuffer)
		R_Mesh_DestroyMeshBuffer(model->surfmesh.data_element3s_indexbuffer);
	model->surfmesh.data_element3s_indexbuffer = NULL;
	if (model->surfmesh.vbo_vertexbuffer)
		R_Mesh_DestroyMeshBuffer(model->surfmesh.vbo_vertexbuffer);
	model->surfmesh.vbo_vertexbuffer = 0;

	// convert all triangles to unique vertex data
	outvertexindex = 0;
	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		surface->num_firstvertex = outvertexindex;
		surface->num_vertices = surface->num_triangles*3;
		e = oldsurfmesh.data_element3i + surface->num_firsttriangle*3;
		for (i = 0;i < surface->num_triangles*3;i++)
		{
			vertexindex = e[i];
			model->surfmesh.data_vertex3f[outvertexindex*3+0] = oldsurfmesh.data_vertex3f[vertexindex*3+0];
			model->surfmesh.data_vertex3f[outvertexindex*3+1] = oldsurfmesh.data_vertex3f[vertexindex*3+1];
			model->surfmesh.data_vertex3f[outvertexindex*3+2] = oldsurfmesh.data_vertex3f[vertexindex*3+2];
			model->surfmesh.data_normal3f[outvertexindex*3+0] = oldsurfmesh.data_normal3f[vertexindex*3+0];
			model->surfmesh.data_normal3f[outvertexindex*3+1] = oldsurfmesh.data_normal3f[vertexindex*3+1];
			model->surfmesh.data_normal3f[outvertexindex*3+2] = oldsurfmesh.data_normal3f[vertexindex*3+2];
			model->surfmesh.data_svector3f[outvertexindex*3+0] = oldsurfmesh.data_svector3f[vertexindex*3+0];
			model->surfmesh.data_svector3f[outvertexindex*3+1] = oldsurfmesh.data_svector3f[vertexindex*3+1];
			model->surfmesh.data_svector3f[outvertexindex*3+2] = oldsurfmesh.data_svector3f[vertexindex*3+2];
			model->surfmesh.data_tvector3f[outvertexindex*3+0] = oldsurfmesh.data_tvector3f[vertexindex*3+0];
			model->surfmesh.data_tvector3f[outvertexindex*3+1] = oldsurfmesh.data_tvector3f[vertexindex*3+1];
			model->surfmesh.data_tvector3f[outvertexindex*3+2] = oldsurfmesh.data_tvector3f[vertexindex*3+2];
			model->surfmesh.data_texcoordtexture2f[outvertexindex*2+0] = oldsurfmesh.data_texcoordtexture2f[vertexindex*2+0];
			model->surfmesh.data_texcoordtexture2f[outvertexindex*2+1] = oldsurfmesh.data_texcoordtexture2f[vertexindex*2+1];
			if (oldsurfmesh.data_texcoordlightmap2f)
			{
				model->surfmesh.data_texcoordlightmap2f[outvertexindex*2+0] = oldsurfmesh.data_texcoordlightmap2f[vertexindex*2+0];
				model->surfmesh.data_texcoordlightmap2f[outvertexindex*2+1] = oldsurfmesh.data_texcoordlightmap2f[vertexindex*2+1];
			}
			if (oldsurfmesh.data_lightmapcolor4f)
			{
				model->surfmesh.data_lightmapcolor4f[outvertexindex*4+0] = oldsurfmesh.data_lightmapcolor4f[vertexindex*4+0];
				model->surfmesh.data_lightmapcolor4f[outvertexindex*4+1] = oldsurfmesh.data_lightmapcolor4f[vertexindex*4+1];
				model->surfmesh.data_lightmapcolor4f[outvertexindex*4+2] = oldsurfmesh.data_lightmapcolor4f[vertexindex*4+2];
				model->surfmesh.data_lightmapcolor4f[outvertexindex*4+3] = oldsurfmesh.data_lightmapcolor4f[vertexindex*4+3];
			}
			else
				Vector4Set(model->surfmesh.data_lightmapcolor4f + 4*outvertexindex, 1, 1, 1, 1);
			model->surfmesh.data_element3i[surface->num_firsttriangle*3+i] = outvertexindex;
			outvertexindex++;
		}
	}
	if (model->surfmesh.data_element3s)
		for (i = 0;i < model->surfmesh.num_triangles*3;i++)
			model->surfmesh.data_element3s[i] = model->surfmesh.data_element3i[i];

	// find and update all submodels to use this new surfmesh data
	for (i = 0;i < model->brush.numsubmodels;i++)
		model->brush.submodels[i]->surfmesh = model->surfmesh;
}

static void Mod_GenerateLightmaps_CreateTriangleInformation(dp_model_t *model)
{
	msurface_t *surface;
	int surfaceindex;
	int i;
	int axis;
	float normal[3];
	const int *e;
	lightmaptriangle_t *triangle;
	// generate lightmap triangle structs
	mod_generatelightmaps_lightmaptriangles = (lightmaptriangle_t *)Mem_Alloc(model->mempool, model->surfmesh.num_triangles * sizeof(lightmaptriangle_t));
	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		e = model->surfmesh.data_element3i + surface->num_firsttriangle*3;
		for (i = 0;i < surface->num_triangles;i++)
		{
			triangle = &mod_generatelightmaps_lightmaptriangles[surface->num_firsttriangle+i];
			triangle->triangleindex = surface->num_firsttriangle+i;
			triangle->surfaceindex = surfaceindex;
			VectorCopy(model->surfmesh.data_vertex3f + 3*e[i*3+0], triangle->vertex[0]);
			VectorCopy(model->surfmesh.data_vertex3f + 3*e[i*3+1], triangle->vertex[1]);
			VectorCopy(model->surfmesh.data_vertex3f + 3*e[i*3+2], triangle->vertex[2]);
			// calculate bounds of triangle
			triangle->mins[0] = min(triangle->vertex[0][0], min(triangle->vertex[1][0], triangle->vertex[2][0]));
			triangle->mins[1] = min(triangle->vertex[0][1], min(triangle->vertex[1][1], triangle->vertex[2][1]));
			triangle->mins[2] = min(triangle->vertex[0][2], min(triangle->vertex[1][2], triangle->vertex[2][2]));
			triangle->maxs[0] = max(triangle->vertex[0][0], max(triangle->vertex[1][0], triangle->vertex[2][0]));
			triangle->maxs[1] = max(triangle->vertex[0][1], max(triangle->vertex[1][1], triangle->vertex[2][1]));
			triangle->maxs[2] = max(triangle->vertex[0][2], max(triangle->vertex[1][2], triangle->vertex[2][2]));
			// pick an axial projection based on the triangle normal
			TriangleNormal(triangle->vertex[0], triangle->vertex[1], triangle->vertex[2], normal);
			axis = 0;
			if (fabs(normal[1]) > fabs(normal[axis]))
				axis = 1;
			if (fabs(normal[2]) > fabs(normal[axis]))
				axis = 2;
			triangle->axis = axis;
		}
	}
}

static void Mod_GenerateLightmaps_DestroyTriangleInformation(dp_model_t *model)
{
	if (mod_generatelightmaps_lightmaptriangles)
		Mem_Free(mod_generatelightmaps_lightmaptriangles);
	mod_generatelightmaps_lightmaptriangles = NULL;
}

float lmaxis[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

static void Mod_GenerateLightmaps_CreateLightmaps(dp_model_t *model)
{
	msurface_t *surface;
	int surfaceindex;
	int lightmapindex;
	int lightmapnumber;
	int i;
	int j;
	int k;
	int x;
	int y;
	int axis;
	int axis1;
	int axis2;
	int retry;
	int pixeloffset;
	float trianglenormal[3];
	float samplecenter[3];
	float samplenormal[3];
	float temp[3];
	float lmiscale[2];
	float slopex;
	float slopey;
	float slopebase;
	float lmscalepixels;
	float lmmins;
	float lmmaxs;
	float lm_basescalepixels;
	int lm_borderpixels;
	int lm_texturesize;
	//int lm_maxpixels;
	const int *e;
	lightmaptriangle_t *triangle;
	unsigned char *lightmappixels;
	unsigned char *deluxemappixels;
	mod_alloclightmap_state_t lmstate;

	// generate lightmap projection information for all triangles
	if (model->texturepool == NULL)
		model->texturepool = R_AllocTexturePool();
	lm_basescalepixels = 1.0f / max(0.0001f, mod_generatelightmaps_unitspersample.value);
	lm_borderpixels = mod_generatelightmaps_borderpixels.integer;
	lm_texturesize = bound(lm_borderpixels*2+1, 64, (int)vid.maxtexturesize_2d);
	//lm_maxpixels = lm_texturesize-(lm_borderpixels*2+1);
	Mod_AllocLightmap_Init(&lmstate, lm_texturesize, lm_texturesize);
	lightmapnumber = 0;
	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		e = model->surfmesh.data_element3i + surface->num_firsttriangle*3;
		lmscalepixels = lm_basescalepixels;
		for (retry = 0;retry < 30;retry++)
		{
			// after a couple failed attempts, degrade quality to make it fit
			if (retry > 1)
				lmscalepixels *= 0.5f;
			for (i = 0;i < surface->num_triangles;i++)
			{
				triangle = &mod_generatelightmaps_lightmaptriangles[surface->num_firsttriangle+i];
				triangle->lightmapindex = lightmapnumber;
				// calculate lightmap bounds in 3D pixel coordinates, limit size,
				// pick two planar axes for projection
				// lightmap coordinates here are in pixels
				// lightmap projections are snapped to pixel grid explicitly, such
				// that two neighboring triangles sharing an edge and projection
				// axis will have identical sampl espacing along their shared edge
				k = 0;
				for (j = 0;j < 3;j++)
				{
					if (j == triangle->axis)
						continue;
					lmmins = floor(triangle->mins[j]*lmscalepixels)-lm_borderpixels;
					lmmaxs = floor(triangle->maxs[j]*lmscalepixels)+lm_borderpixels;
					triangle->lmsize[k] = (int)(lmmaxs-lmmins);
					triangle->lmbase[k] = lmmins/lmscalepixels;
					triangle->lmscale[k] = lmscalepixels;
					k++;
				}
				if (!Mod_AllocLightmap_Block(&lmstate, triangle->lmsize[0], triangle->lmsize[1], &triangle->lmoffset[0], &triangle->lmoffset[1]))
					break;
			}
			// if all fit in this texture, we're done with this surface
			if (i == surface->num_triangles)
				break;
			// if we haven't maxed out the lightmap size yet, we retry the
			// entire surface batch...
			if (lm_texturesize * 2 <= min(mod_generatelightmaps_texturesize.integer, (int)vid.maxtexturesize_2d))
			{
				lm_texturesize *= 2;
				surfaceindex = -1;
				lightmapnumber = 0;
				Mod_AllocLightmap_Free(&lmstate);
				Mod_AllocLightmap_Init(&lmstate, lm_texturesize, lm_texturesize);
				break;
			}
			// if we have maxed out the lightmap size, and this triangle does
			// not fit in the same texture as the rest of the surface, we have
			// to retry the entire surface in a new texture (can only use one)
			// with multiple retries, the lightmap quality degrades until it
			// fits (or gives up)
			if (surfaceindex > 0)
				lightmapnumber++;
			Mod_AllocLightmap_Reset(&lmstate);
		}
	}
	lightmapnumber++;
	Mod_AllocLightmap_Free(&lmstate);

	// now put triangles together into lightmap textures, and do not allow
	// triangles of a surface to go into different textures (as that would
	// require rewriting the surface list)
	model->brushq3.deluxemapping_modelspace = true;
	model->brushq3.deluxemapping = true;
	model->brushq3.num_mergedlightmaps = lightmapnumber;
	model->brushq3.data_lightmaps = (rtexture_t **)Mem_Alloc(model->mempool, model->brushq3.num_mergedlightmaps * sizeof(rtexture_t *));
	model->brushq3.data_deluxemaps = (rtexture_t **)Mem_Alloc(model->mempool, model->brushq3.num_mergedlightmaps * sizeof(rtexture_t *));
	lightmappixels = (unsigned char *)Mem_Alloc(tempmempool, model->brushq3.num_mergedlightmaps * lm_texturesize * lm_texturesize * 4);
	deluxemappixels = (unsigned char *)Mem_Alloc(tempmempool, model->brushq3.num_mergedlightmaps * lm_texturesize * lm_texturesize * 4);
	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		e = model->surfmesh.data_element3i + surface->num_firsttriangle*3;
		for (i = 0;i < surface->num_triangles;i++)
		{
			triangle = &mod_generatelightmaps_lightmaptriangles[surface->num_firsttriangle+i];
			TriangleNormal(triangle->vertex[0], triangle->vertex[1], triangle->vertex[2], trianglenormal);
			VectorNormalize(trianglenormal);
			VectorCopy(trianglenormal, samplenormal); // FIXME: this is supposed to be interpolated per pixel from vertices
			axis = triangle->axis;
			axis1 = axis == 0 ? 1 : 0;
			axis2 = axis == 2 ? 1 : 2;
			lmiscale[0] = 1.0f / triangle->lmscale[0];
			lmiscale[1] = 1.0f / triangle->lmscale[1];
			if (trianglenormal[axis] < 0)
				VectorNegate(trianglenormal, trianglenormal);
			CrossProduct(lmaxis[axis2], trianglenormal, temp);slopex = temp[axis] / temp[axis1];
			CrossProduct(lmaxis[axis1], trianglenormal, temp);slopey = temp[axis] / temp[axis2];
			slopebase = triangle->vertex[0][axis] - triangle->vertex[0][axis1]*slopex - triangle->vertex[0][axis2]*slopey;
			for (j = 0;j < 3;j++)
			{
				float *t2f = model->surfmesh.data_texcoordlightmap2f + e[i*3+j]*2;
				t2f[0] = ((triangle->vertex[j][axis1] - triangle->lmbase[0]) * triangle->lmscale[0] + triangle->lmoffset[0]) / lm_texturesize;
				t2f[1] = ((triangle->vertex[j][axis2] - triangle->lmbase[1]) * triangle->lmscale[1] + triangle->lmoffset[1]) / lm_texturesize;
#if 0
				samplecenter[axis1] = (t2f[0]*lm_texturesize-triangle->lmoffset[0])*lmiscale[0] + triangle->lmbase[0];
				samplecenter[axis2] = (t2f[1]*lm_texturesize-triangle->lmoffset[1])*lmiscale[1] + triangle->lmbase[1];
				samplecenter[axis] = samplecenter[axis1]*slopex + samplecenter[axis2]*slopey + slopebase;
				Con_Printf("%f:%f %f:%f %f:%f = %f %f\n", triangle->vertex[j][axis1], samplecenter[axis1], triangle->vertex[j][axis2], samplecenter[axis2], triangle->vertex[j][axis], samplecenter[axis], t2f[0], t2f[1]);
#endif
			}

#if 0
			switch (axis)
			{
			default:
			case 0:
				forward[0] = 0;
				forward[1] = 1.0f / triangle->lmscale[0];
				forward[2] = 0;
				left[0] = 0;
				left[1] = 0;
				left[2] = 1.0f / triangle->lmscale[1];
				up[0] = 1.0f;
				up[1] = 0;
				up[2] = 0;
				origin[0] = 0;
				origin[1] = triangle->lmbase[0];
				origin[2] = triangle->lmbase[1];
				break;
			case 1:
				forward[0] = 1.0f / triangle->lmscale[0];
				forward[1] = 0;
				forward[2] = 0;
				left[0] = 0;
				left[1] = 0;
				left[2] = 1.0f / triangle->lmscale[1];
				up[0] = 0;
				up[1] = 1.0f;
				up[2] = 0;
				origin[0] = triangle->lmbase[0];
				origin[1] = 0;
				origin[2] = triangle->lmbase[1];
				break;
			case 2:
				forward[0] = 1.0f / triangle->lmscale[0];
				forward[1] = 0;
				forward[2] = 0;
				left[0] = 0;
				left[1] = 1.0f / triangle->lmscale[1];
				left[2] = 0;
				up[0] = 0;
				up[1] = 0;
				up[2] = 1.0f;
				origin[0] = triangle->lmbase[0];
				origin[1] = triangle->lmbase[1];
				origin[2] = 0;
				break;
			}
			Matrix4x4_FromVectors(&backmatrix, forward, left, up, origin);
#endif
#define LM_DIST_EPSILON (1.0f / 32.0f)
			for (y = 0;y < triangle->lmsize[1];y++)
			{
				pixeloffset = ((triangle->lightmapindex * lm_texturesize + y + triangle->lmoffset[1]) * lm_texturesize + triangle->lmoffset[0]) * 4;
				for (x = 0;x < triangle->lmsize[0];x++, pixeloffset += 4)
				{
					samplecenter[axis1] = (x+0.5f)*lmiscale[0] + triangle->lmbase[0];
					samplecenter[axis2] = (y+0.5f)*lmiscale[1] + triangle->lmbase[1];
					samplecenter[axis] = samplecenter[axis1]*slopex + samplecenter[axis2]*slopey + slopebase;
					VectorMA(samplecenter, 0.125f, samplenormal, samplecenter);
					Mod_GenerateLightmaps_LightmapSample(samplecenter, samplenormal, lightmappixels + pixeloffset, deluxemappixels + pixeloffset);
				}
			}
		}
	}

	for (lightmapindex = 0;lightmapindex < model->brushq3.num_mergedlightmaps;lightmapindex++)
	{
		model->brushq3.data_lightmaps[lightmapindex] = R_LoadTexture2D(model->texturepool, va("lightmap%i", lightmapindex), lm_texturesize, lm_texturesize, lightmappixels + lightmapindex * lm_texturesize * lm_texturesize * 4, TEXTYPE_BGRA, TEXF_FORCELINEAR, -1, NULL);
		model->brushq3.data_deluxemaps[lightmapindex] = R_LoadTexture2D(model->texturepool, va("deluxemap%i", lightmapindex), lm_texturesize, lm_texturesize, deluxemappixels + lightmapindex * lm_texturesize * lm_texturesize * 4, TEXTYPE_BGRA, TEXF_FORCELINEAR, -1, NULL);
	}

	if (lightmappixels)
		Mem_Free(lightmappixels);
	if (deluxemappixels)
		Mem_Free(deluxemappixels);

	for (surfaceindex = 0;surfaceindex < model->num_surfaces;surfaceindex++)
	{
		surface = model->data_surfaces + surfaceindex;
		e = model->surfmesh.data_element3i + surface->num_firsttriangle*3;
		if (!surface->num_triangles)
			continue;
		lightmapindex = mod_generatelightmaps_lightmaptriangles[surface->num_firsttriangle].lightmapindex;
		surface->lightmaptexture = model->brushq3.data_lightmaps[lightmapindex];
		surface->deluxemaptexture = model->brushq3.data_deluxemaps[lightmapindex];
		surface->lightmapinfo = NULL;
	}

	model->brush.LightPoint = Mod_GenerateLightmaps_LightPoint;
	model->brushq1.lightdata = NULL;
	model->brushq1.lightmapupdateflags = NULL;
	model->brushq1.firstrender = false;
	model->brushq1.num_lightstyles = 0;
	model->brushq1.data_lightstyleinfo = NULL;
	for (i = 0;i < model->brush.numsubmodels;i++)
	{
		model->brush.submodels[i]->brushq1.lightmapupdateflags = NULL;
		model->brush.submodels[i]->brushq1.firstrender = false;
		model->brush.submodels[i]->brushq1.num_lightstyles = 0;
		model->brush.submodels[i]->brushq1.data_lightstyleinfo = NULL;
	}
}

static void Mod_GenerateLightmaps_UpdateVertexColors(dp_model_t *model)
{
	int i;
	for (i = 0;i < model->surfmesh.num_vertices;i++)
		Mod_GenerateLightmaps_VertexSample(model->surfmesh.data_vertex3f + 3*i, model->surfmesh.data_normal3f + 3*i, model->surfmesh.data_lightmapcolor4f + 4*i);
}

static void Mod_GenerateLightmaps_UpdateLightGrid(dp_model_t *model)
{
	int x;
	int y;
	int z;
	int index = 0;
	float pos[3];
	for (z = 0;z < model->brushq3.num_lightgrid_isize[2];z++)
	{
		pos[2] = (model->brushq3.num_lightgrid_imins[2] + z + 0.5f) * model->brushq3.num_lightgrid_cellsize[2];
		for (y = 0;y < model->brushq3.num_lightgrid_isize[1];y++)
		{
			pos[1] = (model->brushq3.num_lightgrid_imins[1] + y + 0.5f) * model->brushq3.num_lightgrid_cellsize[1];
			for (x = 0;x < model->brushq3.num_lightgrid_isize[0];x++, index++)
			{
				pos[0] = (model->brushq3.num_lightgrid_imins[0] + x + 0.5f) * model->brushq3.num_lightgrid_cellsize[0];
				Mod_GenerateLightmaps_GridSample(pos, model->brushq3.data_lightgrid + index);
			}
		}
	}
}

extern cvar_t mod_q3bsp_nolightmaps;
static void Mod_GenerateLightmaps(dp_model_t *model)
{
	//lightmaptriangle_t *lightmaptriangles = Mem_Alloc(model->mempool, model->surfmesh.num_triangles * sizeof(lightmaptriangle_t));
	dp_model_t *oldloadmodel = loadmodel;
	loadmodel = model;

	Mod_GenerateLightmaps_InitSampleOffsets(model);
	Mod_GenerateLightmaps_DestroyLightmaps(model);
	Mod_GenerateLightmaps_UnweldTriangles(model);
	Mod_GenerateLightmaps_CreateTriangleInformation(model);
	Mod_GenerateLightmaps_CreateLights(model);
	if(!mod_q3bsp_nolightmaps.integer)
		Mod_GenerateLightmaps_CreateLightmaps(model);
	Mod_GenerateLightmaps_UpdateVertexColors(model);
	Mod_GenerateLightmaps_UpdateLightGrid(model);
	Mod_GenerateLightmaps_DestroyLights(model);
	Mod_GenerateLightmaps_DestroyTriangleInformation(model);

	loadmodel = oldloadmodel;
}

static void Mod_GenerateLightmaps_f(void)
{
	if (Cmd_Argc() != 1)
	{
		Con_Printf("usage: mod_generatelightmaps\n");
		return;
	}
	if (!cl.worldmodel)
	{
		Con_Printf("no worldmodel loaded\n");
		return;
	}
	Mod_GenerateLightmaps(cl.worldmodel);
}
