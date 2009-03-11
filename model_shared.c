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

cvar_t r_mipskins = {CVAR_SAVE, "r_mipskins", "0", "mipmaps model skins so they render faster in the distance and do not display noise artifacts, can cause discoloration of skins if they contain undesirable border colors"};

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
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	// parse the Q3 shader files
	Mod_LoadQ3Shaders();

	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*')
			if (mod->used)
				Mod_LoadModel(mod, true, false, mod->isworldmodel);
}

static void mod_shutdown(void)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && (mod->loaded || mod->mempool))
			Mod_UnloadModel(mod);

	Mem_FreePool (&q3shaders_mem);
}

static void mod_newmap(void)
{
	msurface_t *surface;
	int i, j, k, surfacenum, ssize, tsize;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;

	R_SkinFrame_PrepareForPurge();
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->mempool && mod->data_textures)
		{
			for (j = 0;j < mod->num_textures;j++)
			{
				for (k = 0;k < mod->data_textures[j].numskinframes;k++)
					R_SkinFrame_MarkUsed(mod->data_textures[j].skinframes[k]);
				for (k = 0;k < mod->data_textures[j].backgroundnumskinframes;k++)
					R_SkinFrame_MarkUsed(mod->data_textures[j].backgroundskinframes[k]);
			}
		}
	}
	R_SkinFrame_Purge();

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
static void Mod_BuildVBOs(void);
void Mod_Init (void)
{
	mod_mempool = Mem_AllocPool("modelinfo", 0, NULL);
	Mem_ExpandableArray_NewArray(&models, mod_mempool, sizeof(dp_model_t), 16);

	Mod_BrushInit();
	Mod_AliasInit();
	Mod_SpriteInit();

	Cvar_RegisterVariable(&r_mipskins);
	Cmd_AddCommand ("modellist", Mod_Print, "prints a list of loaded models");
	Cmd_AddCommand ("modelprecache", Mod_Precache, "load a model");
	Cmd_AddCommand ("modeldecompile", Mod_Decompile_f, "exports a model in several formats for editing purposes");
}

void Mod_RenderInit(void)
{
	R_RegisterModule("Models", mod_start, mod_shutdown, mod_newmap);
}

void Mod_UnloadModel (dp_model_t *mod)
{
	char name[MAX_QPATH];
	qboolean isworldmodel;
	qboolean used;

	if (developer_loading.integer)
		Con_Printf("unloading model %s\n", mod->name);

	strlcpy(name, mod->name, sizeof(name));
	isworldmodel = mod->isworldmodel;
	used = mod->used;
	if (mod->surfmesh.ebo3i)
		R_Mesh_DestroyBufferObject(mod->surfmesh.ebo3i);
	if (mod->surfmesh.ebo3s)
		R_Mesh_DestroyBufferObject(mod->surfmesh.ebo3s);
	if (mod->surfmesh.vbo)
		R_Mesh_DestroyBufferObject(mod->surfmesh.vbo);
	// free textures/memory attached to the model
	R_FreeTexturePool(&mod->texturepool);
	Mem_FreePool(&mod->mempool);
	// clear the struct to make it available
	memset(mod, 0, sizeof(dp_model_t));
	// restore the fields we want to preserve
	strlcpy(mod->name, name, sizeof(mod->name));
	mod->isworldmodel = isworldmodel;
	mod->used = used;
	mod->loaded = false;
}

void R_Model_Null_Draw(entity_render_t *ent)
{
	return;
}

/*
==================
Mod_LoadModel

Loads a model
==================
*/
dp_model_t *Mod_LoadModel(dp_model_t *mod, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	int num;
	unsigned int crc;
	void *buf;
	fs_offset_t filesize;

	mod->used = true;

	if (mod->name[0] == '*') // submodel
		return mod;
	
	if (!strcmp(mod->name, "null"))
	{
		if (mod->isworldmodel != isworldmodel)
			mod->loaded = false;

		if(mod->loaded)
			return mod;

		if (mod->loaded || mod->mempool)
			Mod_UnloadModel(mod);

		if (developer_loading.integer)
			Con_Printf("loading model %s\n", mod->name);

		mod->isworldmodel = isworldmodel;
		mod->used = true;
		mod->crc = -1;
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

	// if the model is a worldmodel and is being referred to as a
	// non-worldmodel here, then it needs reloading to get rid of the
	// submodels
	if (mod->isworldmodel != isworldmodel)
		mod->loaded = false;

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

	// LordHavoc: unload the existing model in this slot (if there is one)
	if (mod->loaded || mod->mempool)
		Mod_UnloadModel(mod);

	// load the model
	mod->isworldmodel = isworldmodel;
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

	// if we're loading a worldmodel, then this is a level change
	if (mod->isworldmodel)
	{
		// clear out any stale submodels or worldmodels lying around
		// if we did this clear before now, an error might abort loading and
		// leave things in a bad state
		Mod_RemoveStaleWorldModels(mod);
		// reload q3 shaders, to make sure they are ready to go for this level
		// (including any models loaded for this level)
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
		     if (!memcmp(buf, "IDPO", 4)) Mod_IDP0_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDP2", 4)) Mod_IDP2_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDP3", 4)) Mod_IDP3_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDSP", 4)) Mod_IDSP_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IDS2", 4)) Mod_IDS2_Load(mod, buf, bufend);
		else if (!memcmp(buf, "IBSP", 4)) Mod_IBSP_Load(mod, buf, bufend);
		else if (!memcmp(buf, "ZYMOTICMODEL", 12)) Mod_ZYMOTICMODEL_Load(mod, buf, bufend);
		else if (!memcmp(buf, "DARKPLACESMODEL", 16)) Mod_DARKPLACESMODEL_Load(mod, buf, bufend);
		else if (!memcmp(buf, "ACTRHEAD", 8)) Mod_PSKMODEL_Load(mod, buf, bufend);
		else if (strlen(mod->name) >= 4 && !strcmp(mod->name + strlen(mod->name) - 4, ".map")) Mod_MAP_Load(mod, buf, bufend);
		else if (num == BSPVERSION || num == 30) Mod_Q1BSP_Load(mod, buf, bufend);
		else Con_Printf("Mod_LoadModel: model \"%s\" is of unknown/unsupported type\n", mod->name);
		Mem_Free(buf);

		Mod_BuildVBOs();

		// no fatal errors occurred, so this model is ready to use.
		mod->loaded = true;
	}
	else if (crash)
	{
		// LordHavoc: Sys_Error was *ANNOYING*
		Con_Printf ("Mod_LoadModel: %s not found\n", mod->name);
	}
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

// only used during loading!
void Mod_RemoveStaleWorldModels(dp_model_t *skip)
{
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->isworldmodel && mod->loaded && skip != mod)
		{
			Mod_UnloadModel(mod);
			mod->isworldmodel = false;
			mod->used = false;
		}
	}
}

/*
==================
Mod_FindName

==================
*/
dp_model_t *Mod_FindName(const char *name)
{
	int i;
	int nummodels;
	dp_model_t *mod;

	// if we're not dedicatd, the renderer calls will crash without video
	Host_StartVideo();

	nummodels = Mem_ExpandableArray_IndexRange(&models);

	if (!name[0])
		Host_Error ("Mod_ForName: NULL name");

	// search the currently loaded models
	for (i = 0;i < nummodels;i++)
	{
		if ((mod = (dp_model_t*) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && !strcmp(mod->name, name))
		{
			mod->used = true;
			return mod;
		}
	}

	// no match found, create a new one
	mod = (dp_model_t *) Mem_ExpandableArray_AllocRecord(&models);
	strlcpy(mod->name, name, sizeof(mod->name));
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
dp_model_t *Mod_ForName(const char *name, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	dp_model_t *model;
	model = Mod_FindName(name);
	if (model->name[0] != '*' && (!model->loaded || checkdisk))
		Mod_LoadModel(model, crash, checkdisk, isworldmodel);
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
	int i;
	int nummodels = Mem_ExpandableArray_IndexRange(&models);
	dp_model_t *mod;
	for (i = 0;i < nummodels;i++)
		if ((mod = (dp_model_t *) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0] && mod->name[0] != '*' && mod->used)
			Mod_LoadModel(mod, true, true, mod->isworldmodel);
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
		if ((mod = (dp_model_t *) Mem_ExpandableArray_RecordAtIndex(&models, i)) && mod->name[0])
			Con_Printf("%4iK %s\n", mod->mempool ? (int)((mod->mempool->totalsize + 1023) / 1024) : 0, mod->name);
}

/*
================
Mod_Precache
================
*/
static void Mod_Precache(void)
{
	if (Cmd_Argc() == 2)
		Mod_ForName(Cmd_Argv(1), false, true, cl.worldmodel && !strcasecmp(Cmd_Argv(1), cl.worldmodel->name));
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
	edgehashentry_t *edgehash[TRIANGLEEDGEHASH], *edgehashentries, edgehashentriesbuffer[TRIANGLEEDGEHASH*3], *hash;
	memset(edgehash, 0, sizeof(edgehash));
	edgehashentries = edgehashentriesbuffer;
	// if there are too many triangles for the stack array, allocate larger buffer
	if (numtriangles > TRIANGLEEDGEHASH)
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
	if (edgehashentries != edgehashentriesbuffer)
		Mem_Free(edgehashentries);
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

static void Mod_ShadowMesh_CreateVBOs(shadowmesh_t *mesh)
{
	if (!gl_support_arb_vertex_buffer_object)
		return;

	// element buffer is easy because it's just one array
	if (mesh->numtriangles)
	{
		if (mesh->element3s)
			mesh->ebo3s = R_Mesh_CreateStaticBufferObject(GL_ELEMENT_ARRAY_BUFFER_ARB, mesh->element3s, mesh->numtriangles * sizeof(unsigned short[3]), "shadowmesh");
		else
			mesh->ebo3i = R_Mesh_CreateStaticBufferObject(GL_ELEMENT_ARRAY_BUFFER_ARB, mesh->element3i, mesh->numtriangles * sizeof(unsigned int[3]), "shadowmesh");
	}

	// vertex buffer is several arrays and we put them in the same buffer
	//
	// is this wise?  the texcoordtexture2f array is used with dynamic
	// vertex/svector/tvector/normal when rendering animated models, on the
	// other hand animated models don't use a lot of vertices anyway...
	if (mesh->numverts)
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
		mesh->vbo = R_Mesh_CreateStaticBufferObject(GL_ARRAY_BUFFER_ARB, mem, size, "shadowmesh");
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
				Mod_ShadowMesh_CreateVBOs(newmesh);
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
		if (mesh->ebo3i)
			R_Mesh_DestroyBufferObject(mesh->ebo3i);
		if (mesh->ebo3s)
			R_Mesh_DestroyBufferObject(mesh->ebo3s);
		if (mesh->vbo)
			R_Mesh_DestroyBufferObject(mesh->vbo);
		nextmesh = mesh->next;
		Mem_Free(mesh);
	}
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

q3wavefunc_t Mod_LoadQ3Shaders_EnumerateWaveFunc(const char *s)
{
	if (!strcasecmp(s, "sin"))             return Q3WAVEFUNC_SIN;
	if (!strcasecmp(s, "square"))          return Q3WAVEFUNC_SQUARE;
	if (!strcasecmp(s, "triangle"))        return Q3WAVEFUNC_TRIANGLE;
	if (!strcasecmp(s, "sawtooth"))        return Q3WAVEFUNC_SAWTOOTH;
	if (!strcasecmp(s, "inversesawtooth")) return Q3WAVEFUNC_INVERSESAWTOOTH;
	if (!strcasecmp(s, "noise"))           return Q3WAVEFUNC_NOISE;
	Con_DPrintf("Mod_LoadQ3Shaders: unknown wavefunc %s\n", s);
	return Q3WAVEFUNC_NONE;
}

static void Q3Shaders_Clear()
{
	/* Just clear out everything... */
	Mem_FreePool (&q3shaders_mem);
	/* ...and alloc the structs again. */
	q3shaders_mem = Mem_AllocPool("q3shaders", 0, NULL);
	q3shader_data = (q3shader_data_t*)Mem_Alloc (q3shaders_mem,
		sizeof (q3shader_data_t));
	Mem_ExpandableArray_NewArray (&q3shader_data->hash_entries,
		q3shaders_mem, sizeof (q3shader_hash_entry_t), 256);
	Mem_ExpandableArray_NewArray (&q3shader_data->char_ptrs,
		q3shaders_mem, sizeof (char**), 256);
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
			Con_Printf("Shader '%s' already defined\n", shader->name);
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

extern cvar_t r_picmipworld;
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

	Q3Shaders_Clear();

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

			strlcpy(shader.name, com_token, sizeof(shader.name));
			if (!COM_ParseToken_QuakeC(&text, false) || strcasecmp(com_token, "{"))
			{
				Con_Printf("%s parsing error - expected \"{\", found \"%s\"\n", search->filenames[fileindex], com_token);
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
								strlcpy(parameter[j], com_token, sizeof(parameter[j]));
								numparameters = j + 1;
							}
							if (!COM_ParseToken_QuakeC(&text, true))
								break;
						}
						for (j = numparameters;j < TEXTURE_MAXFRAMES + 4;j++)
							parameter[j][0] = 0;
						if (developer.integer >= 100)
						{
							Con_Printf("%s %i: ", shader.name, shader.numlayers - 1);
							for (j = 0;j < numparameters;j++)
								Con_Printf(" %s", parameter[j]);
							Con_Print("\n");
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
					layer->texflags = TEXF_ALPHA | TEXF_PRECACHE;
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
						strlcpy(parameter[j], com_token, sizeof(parameter[j]));
						numparameters = j + 1;
					}
					if (!COM_ParseToken_QuakeC(&text, true))
						break;
				}
				for (j = numparameters;j < TEXTURE_MAXFRAMES + 4;j++)
					parameter[j][0] = 0;
				if (fileindex == 0 && !strcasecmp(com_token, "}"))
					break;
				if (developer.integer >= 100)
				{
					Con_Printf("%s: ", shader.name);
					for (j = 0;j < numparameters;j++)
						Con_Printf(" %s", parameter[j]);
					Con_Print("\n");
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
						Con_DPrintf("%s parsing warning: unknown surfaceparm \"%s\"\n", search->filenames[fileindex], parameter[1]);
				}
				else if (!strcasecmp(parameter[0], "dpshadow"))
					shader.dpshadow = true;
				else if (!strcasecmp(parameter[0], "dpnoshadow"))
					shader.dpnoshadow = true;
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
				else if (!strcasecmp(parameter[0], "dp_refract") && numparameters >= 5)
				{
					shader.textureflags |= Q3TEXTUREFLAG_REFRACTION;
					shader.refractfactor = atof(parameter[1]);
					Vector4Set(shader.refractcolor4f, atof(parameter[2]), atof(parameter[3]), atof(parameter[4]), 1);
				}
				else if (!strcasecmp(parameter[0], "dp_reflect") && numparameters >= 6)
				{
					shader.textureflags |= Q3TEXTUREFLAG_REFLECTION;
					shader.reflectfactor = atof(parameter[1]);
					Vector4Set(shader.reflectcolor4f, atof(parameter[2]), atof(parameter[3]), atof(parameter[4]), atof(parameter[5]));
				}
				else if (!strcasecmp(parameter[0], "dp_water") && numparameters >= 12)
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
				shader.textureflags &= ~(Q3TEXTUREFLAG_REFRACTION | Q3TEXTUREFLAG_REFLECTION);

			Q3Shader_AddToHash (&shader);
		}
		Mem_Free(f);
	}
}

q3shaderinfo_t *Mod_LookupQ3Shader(const char *name)
{
	unsigned short hash = CRC_Block_CaseInsensitive ((const unsigned char *)name, strlen (name));
	q3shader_hash_entry_t* entry = q3shader_data->hash + (hash % Q3SHADER_HASH_SIZE);
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
	qboolean success = true;
	q3shaderinfo_t *shader;
	if (!name)
		name = "";
	strlcpy(texture->name, name, sizeof(texture->name));
	shader = name[0] ? Mod_LookupQ3Shader(name) : NULL;
	if (shader)
	{
		if (developer_loading.integer)
			Con_Printf("%s: loaded shader for %s\n", loadmodel->name, name);
		texture->surfaceparms = shader->surfaceparms;
		texture->textureflags = shader->textureflags;

		// allow disabling of picmip or compression by defaulttexflags
		if(!(defaulttexflags & TEXF_PICMIP))
			texture->textureflags &= ~TEXF_PICMIP;
		if(!(defaulttexflags & TEXF_COMPRESS))
			texture->textureflags &= ~TEXF_COMPRESS;

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
			texture->biaspolygonoffset -= 2;
		if (shader->textureflags & Q3TEXTUREFLAG_REFRACTION)
			texture->basematerialflags |= MATERIALFLAG_REFRACTION;
		if (shader->textureflags & Q3TEXTUREFLAG_REFLECTION)
			texture->basematerialflags |= MATERIALFLAG_REFLECTION;
		if (shader->textureflags & Q3TEXTUREFLAG_WATERSHADER)
			texture->basematerialflags |= MATERIALFLAG_WATERSHADER;
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
				else if (!(texture->skinframes[j] = R_SkinFrame_LoadExternal(primarylayer->texturename[j], primarylayer->texflags, false)))
				{
					Con_Printf("^1%s:^7 could not load texture ^3\"%s\"^7 (frame %i) for shader ^2\"%s\"\n", loadmodel->name, primarylayer->texturename[j], j, texture->name);
					texture->skinframes[j] = R_SkinFrame_LoadMissing();
				}
			}
		}
		if (shader->backgroundlayer >= 0)
		{
			q3shaderinfo_layer_t* backgroundlayer = shader->layers + shader->backgroundlayer;
			texture->backgroundnumskinframes = backgroundlayer->numframes;
			texture->backgroundskinframerate = backgroundlayer->framerate;
			for (j = 0;j < backgroundlayer->numframes;j++)
			{
				if(cls.state == ca_dedicated)
				{
					texture->skinframes[j] = NULL;
				}
				else if (!(texture->backgroundskinframes[j] = R_SkinFrame_LoadExternal(backgroundlayer->texturename[j], backgroundlayer->texflags, false)))
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
		memcpy(texture->deforms, shader->deforms, sizeof(texture->deforms));
		texture->reflectmin = shader->reflectmin;
		texture->reflectmax = shader->reflectmax;
		texture->refractfactor = shader->refractfactor;
		Vector4Copy(shader->refractcolor4f, texture->refractcolor4f);
		texture->reflectfactor = shader->reflectfactor;
		Vector4Copy(shader->reflectcolor4f, texture->reflectcolor4f);
		texture->r_water_wateralpha = shader->r_water_wateralpha;
	}
	else if (!strcmp(texture->name, "noshader") || !texture->name[0])
	{
		if (developer.integer >= 100)
			Con_Printf("^1%s:^7 using fallback noshader material for ^3\"%s\"\n", loadmodel->name, name);
		texture->surfaceparms = 0;
	}
	else if (!strcmp(texture->name, "common/nodraw") || !strcmp(texture->name, "textures/common/nodraw"))
	{
		if (developer.integer >= 100)
			Con_Printf("^1%s:^7 using fallback nodraw material for ^3\"%s\"\n", loadmodel->name, name);
		texture->surfaceparms = 0;
		texture->basematerialflags = MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
	}
	else
	{
		if (developer.integer >= 100)
			Con_Printf("^1%s:^7 No shader found for texture ^3\"%s\"\n", loadmodel->name, texture->name);
		texture->surfaceparms = 0;
		if (texture->surfaceflags & Q3SURFACEFLAG_NODRAW)
			texture->basematerialflags |= MATERIALFLAG_NODRAW | MATERIALFLAG_NOSHADOW;
		else if (texture->surfaceflags & Q3SURFACEFLAG_SKY)
			texture->basematerialflags |= MATERIALFLAG_SKY | MATERIALFLAG_NOSHADOW;
		else
			texture->basematerialflags |= MATERIALFLAG_WALL;
		texture->numskinframes = 1;
		if(cls.state == ca_dedicated)
		{
			texture->skinframes[0] = NULL;
		}
		else
		{
			if (fallback)
			{
				qboolean has_alpha;
				if ((texture->skinframes[0] = R_SkinFrame_LoadExternal_CheckAlpha(texture->name, defaulttexflags, false, &has_alpha)))
				{
					if(has_alpha && (defaulttexflags & TEXF_ALPHA))
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
	for (i = 0;i < MAX_SKINS && (data = text = (char *)FS_LoadFile(va("%s_%i.skin", loadmodel->name, i), tempmempool, true, NULL));i++)
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

static void Mod_BuildVBOs(void)
{
	if (!gl_support_arb_vertex_buffer_object)
		return;

	// element buffer is easy because it's just one array
	if (loadmodel->surfmesh.num_triangles)
	{
		if (loadmodel->surfmesh.data_element3s)
		{
			int i;
			for (i = 0;i < loadmodel->surfmesh.num_triangles*3;i++)
				loadmodel->surfmesh.data_element3s[i] = loadmodel->surfmesh.data_element3i[i];
			loadmodel->surfmesh.ebo3s = R_Mesh_CreateStaticBufferObject(GL_ELEMENT_ARRAY_BUFFER_ARB, loadmodel->surfmesh.data_element3s, loadmodel->surfmesh.num_triangles * sizeof(unsigned short[3]), loadmodel->name);
		}
		else
			loadmodel->surfmesh.ebo3i = R_Mesh_CreateStaticBufferObject(GL_ELEMENT_ARRAY_BUFFER_ARB, loadmodel->surfmesh.data_element3i, loadmodel->surfmesh.num_triangles * sizeof(unsigned int[3]), loadmodel->name);
	}

	// vertex buffer is several arrays and we put them in the same buffer
	//
	// is this wise?  the texcoordtexture2f array is used with dynamic
	// vertex/svector/tvector/normal when rendering animated models, on the
	// other hand animated models don't use a lot of vertices anyway...
	if (loadmodel->surfmesh.num_vertices)
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
		loadmodel->surfmesh.vbo = R_Mesh_CreateStaticBufferObject(GL_ARRAY_BUFFER_ARB, mem, size, loadmodel->name);
		Mem_Free(mem);
	}
}

static void Mod_Decompile_OBJ(dp_model_t *model, const char *filename, const char *mtlfilename, const char *originalfilename)
{
	int vertexindex, surfaceindex, triangleindex, textureindex, countvertices = 0, countsurfaces = 0, countfaces = 0, counttextures = 0;
	const char *texname;
	const int *e;
	const float *v, *vn, *vt;
	size_t l;
	size_t outbufferpos = 0;
	size_t outbuffermax = 0x100000;
	char *outbuffer = Z_Malloc(outbuffermax), *oldbuffer;
	const msurface_t *surface;
	const int maxtextures = 256;
	char *texturenames = Z_Malloc(maxtextures * MAX_QPATH);

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
		for (textureindex = 0;textureindex < maxtextures && texturenames[textureindex*MAX_QPATH];textureindex++)
			if (!strcmp(texturenames + textureindex * MAX_QPATH, texname))
				break;
		if (textureindex >= maxtextures)
			continue; // just a precaution
		if (counttextures < textureindex + 1)
			counttextures = textureindex + 1;
		strlcpy(texturenames + textureindex * MAX_QPATH, texname, MAX_QPATH);
		if (outbufferpos >= outbuffermax >> 1)
		{
			outbuffermax *= 2;
			oldbuffer = outbuffer;
			outbuffer = Z_Malloc(outbuffermax);
			memcpy(outbuffer, oldbuffer, outbufferpos);
			Z_Free(oldbuffer);
		}
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "newmtl %s\nNs 96.078431\nKa 0 0 0\nKd 0.64 0.64 0.64\nKs 0.5 0.5 0.5\nNi 1\nd 1\nillum 2\nmap_Kd %s%s\n\n", texname, texname, strstr(texname, ".tga") ? "" : ".tga");
		if (l > 0)
			outbufferpos += l;
	}

	// write the mtllib file
	FS_WriteFile(mtlfilename, outbuffer, outbufferpos);
	outbufferpos = 0;

	// construct the obj file
	l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "# model exported from %s by darkplaces engine\n# %i vertices, %i faces, %i surfaces\nmtllib %s\n", originalfilename, countvertices, countfaces, countsurfaces, mtlfilename);
	if (l > 0)
		outbufferpos += l;
	for (vertexindex = 0, v = model->surfmesh.data_vertex3f, vn = model->surfmesh.data_normal3f, vt = model->surfmesh.data_texcoordtexture2f;vertexindex < model->surfmesh.num_vertices;vertexindex++, v += 3, vn += 3, vt += 2)
	{
		if (outbufferpos >= outbuffermax >> 1)
		{
			outbuffermax *= 2;
			oldbuffer = outbuffer;
			outbuffer = Z_Malloc(outbuffermax);
			memcpy(outbuffer, oldbuffer, outbufferpos);
			Z_Free(oldbuffer);
		}
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "v %f %f %f\nvn %f %f %f\nvt %f %f\n", v[0], v[2], -v[1], vn[0], vn[2], -vn[1], vt[0], 1-vt[1]);
		if (l > 0)
			outbufferpos += l;
	}
	for (surfaceindex = 0, surface = model->data_surfaces;surfaceindex < model->num_surfaces;surfaceindex++, surface++)
	{
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "usemtl %s\n", (surface->texture && surface->texture->name[0]) ? surface->texture->name : "default");
		if (l > 0)
			outbufferpos += l;
		for (triangleindex = 0, e = model->surfmesh.data_element3i + surface->num_firsttriangle * 3;triangleindex < surface->num_triangles;triangleindex++, e += 3)
		{
			if (outbufferpos >= outbuffermax >> 1)
			{
				outbuffermax *= 2;
				oldbuffer = outbuffer;
				outbuffer = Z_Malloc(outbuffermax);
				memcpy(outbuffer, oldbuffer, outbufferpos);
				Z_Free(oldbuffer);
			}
			l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", e[0]+1, e[0]+1, e[0]+1, e[2]+1, e[2]+1, e[2]+1, e[1]+1, e[1]+1, e[1]+1);
			if (l > 0)
				outbufferpos += l;
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
	const float *pose;
	size_t l;
	size_t outbufferpos = 0;
	size_t outbuffermax = 0x100000;
	char *outbuffer = Z_Malloc(outbuffermax), *oldbuffer;
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
			outbuffer = Z_Malloc(outbuffermax);
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
	for (poseindex = 0, pose = model->data_poses + model->num_bones * 12 * firstpose;poseindex < numposes;poseindex++)
	{
		countframes++;
		l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "time %i\n", poseindex);
		if (l > 0)
			outbufferpos += l;
		for (transformindex = 0;transformindex < model->num_bones;transformindex++, pose += 12)
		{
			float a, b, c;
			float angles[3];
			float mtest[3][4];
			if (outbufferpos >= outbuffermax >> 1)
			{
				outbuffermax *= 2;
				oldbuffer = outbuffer;
				outbuffer = Z_Malloc(outbuffermax);
				memcpy(outbuffer, oldbuffer, outbufferpos);
				Z_Free(oldbuffer);
			}

			// strangely the smd angles are for a transposed matrix, so we
			// have to generate a transposed matrix, then convert that...
			mtest[0][0] = pose[ 0];
			mtest[0][1] = pose[ 4];
			mtest[0][2] = pose[ 8];
			mtest[0][3] = pose[ 3];
			mtest[1][0] = pose[ 1];
			mtest[1][1] = pose[ 5];
			mtest[1][2] = pose[ 9];
			mtest[1][3] = pose[ 7];
			mtest[2][0] = pose[ 2];
			mtest[2][1] = pose[ 6];
			mtest[2][2] = pose[10];
			mtest[2][3] = pose[11];
			AnglesFromVectors(angles, mtest[0], mtest[2], false);
			if (angles[0] >= 180) angles[0] -= 360;
			if (angles[1] >= 180) angles[1] -= 360;
			if (angles[2] >= 180) angles[2] -= 360;

			a = DEG2RAD(angles[ROLL]);
			b = DEG2RAD(angles[PITCH]);
			c = DEG2RAD(angles[YAW]);

#if 0
{
			float cy, sy, cp, sp, cr, sr;
			float test[3][4];
			// smd matrix construction, for comparing to non-transposed m
			sy = sin(c);
			cy = cos(c);
			sp = sin(b);
			cp = cos(b);
			sr = sin(a);
			cr = cos(a);

			test[0][0] = cp*cy;
			test[1][0] = cp*sy;
			test[2][0] = -sp;
			test[0][1] = sr*sp*cy+cr*-sy;
			test[1][1] = sr*sp*sy+cr*cy;
			test[2][1] = sr*cp;
			test[0][2] = (cr*sp*cy+-sr*-sy);
			test[1][2] = (cr*sp*sy+-sr*cy);
			test[2][2] = cr*cp;
			test[0][3] = pose[3];
			test[1][3] = pose[7];
			test[2][3] = pose[11];
}
#endif
			l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f\n", transformindex, pose[3], pose[7], pose[11], DEG2RAD(angles[ROLL]), DEG2RAD(angles[PITCH]), DEG2RAD(angles[YAW]));
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
					outbuffer = Z_Malloc(outbuffermax);
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
					const int *wi = model->surfmesh.data_vertexweightindex4i + index * 4;
					const float *wf = model->surfmesh.data_vertexweightinfluence4f + index * 4;
					     if (wf[3]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 4 %i %f %i %f %i %f %i %f\n", wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0], wi[1], wf[1], wi[2], wf[2], wi[3], wf[3]);
					else if (wf[2]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 3 %i %f %i %f %i %f\n"      , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0], wi[1], wf[1], wi[2], wf[2]);
					else if (wf[1]) l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f 2 %i %f %i %f\n"            , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1], wi[0], wf[0], wi[1], wf[1]);
					else            l = dpsnprintf(outbuffer + outbufferpos, outbuffermax - outbufferpos, "%3i %f %f %f %f %f %f %f %f\n"                          , wi[0], v[0], v[1], v[2], vn[0], vn[1], vn[2], vt[0], 1 - vt[1]);
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
	int zymtextsize = 0;
	int dpmtextsize = 0;

	if (Cmd_Argc() != 2)
	{
		Con_Print("usage: modeldecompile <filename>\n");
		return;
	}

	strlcpy(inname, Cmd_Argv(1), sizeof(inname));
	FS_StripExtension(inname, basename, sizeof(basename));

	mod = Mod_ForName(inname, false, true, cl.worldmodel && !strcasecmp(inname, cl.worldmodel->name));
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
	if (mod->surfmesh.num_triangles && mod->num_poses)
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
					if ((animname[l] < '0' || animname[l] > '9') && animname[l] != '_')
						k = l + 1;
				animname[k] = 0;
				count = (mod->num_poses / mod->num_bones) - first;
				for (j = i + 1;j < mod->numframes;j++)
				{
					strlcpy(animname2, mod->animscenes[j].name, sizeof(animname2));
					for (l = 0, k = strlen(animname2);animname2[l];l++)
						if ((animname2[l] < '0' || animname2[l] > '9') && animname2[l] != '_')
							k = l + 1;
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
				l = dpsnprintf(zymtextbuffer + zymtextsize, sizeof(zymtextbuffer) - zymtextsize, "scene %s.smd fps %g\n", animname, mod->animscenes[i].framerate);
				if (l > 0) zymtextsize += l;
			}
			if (dpmtextsize < (int)sizeof(dpmtextbuffer) - 100)
			{
				l = dpsnprintf(dpmtextbuffer + dpmtextsize, sizeof(dpmtextbuffer) - dpmtextsize, "scene %s.smd\n", animname);
				if (l > 0) dpmtextsize += l;
			}
		}
		if (zymtextsize)
			FS_WriteFile(va("%s_decompiled/out_zym.txt", basename), zymtextbuffer, (fs_offset_t)zymtextsize);
		if (dpmtextsize)
			FS_WriteFile(va("%s_decompiled/out_dpm.txt", basename), dpmtextbuffer, (fs_offset_t)dpmtextsize);
	}
}

