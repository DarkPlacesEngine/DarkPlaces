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

cvar_t r_mipskins = {CVAR_SAVE, "r_mipskins", "0"};

model_t *loadmodel;

// LordHavoc: was 512
#define MAX_MOD_KNOWN (MAX_MODELS + 256)
static model_t mod_known[MAX_MOD_KNOWN];

static void mod_start(void)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);
	Mod_LoadModels();
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
	msurface_t *surface;
	int i, surfacenum, ssize, tsize;

	if (!cl_stainmaps_clearonload.integer)
		return;

	for (i = 0;i < MAX_MOD_KNOWN;i++)
	{
		if (mod_known[i].mempool && mod_known[i].data_surfaces)
		{
			for (surfacenum = 0, surface = mod_known[i].data_surfaces;surfacenum < mod_known[i].num_surfaces;surfacenum++, surface++)
			{
				if (surface->lightmapinfo && surface->lightmapinfo->stainsamples)
				{
					ssize = (surface->lightmapinfo->extents[0] >> 4) + 1;
					tsize = (surface->lightmapinfo->extents[1] >> 4) + 1;
					memset(surface->lightmapinfo->stainsamples, 255, ssize * tsize * 3);
					surface->cached_dlight = true;
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
void Mod_Init (void)
{
	Mod_BrushInit();
	Mod_AliasInit();
	Mod_SpriteInit();

	Cvar_RegisterVariable(&r_mipskins);
	Cmd_AddCommand ("modellist", Mod_Print);
	Cmd_AddCommand ("modelprecache", Mod_Precache);
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
	qboolean used;
	strcpy(name, mod->name);
	isworldmodel = mod->isworldmodel;
	used = mod->used;
	Mod_FreeModel(mod);
	strcpy(mod->name, name);
	mod->isworldmodel = isworldmodel;
	mod->used = used;
	mod->loaded = false;
}

/*
==================
Mod_LoadModel

Loads a model
==================
*/
model_t *Mod_LoadModel(model_t *mod, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	int num;
	unsigned int crc;
	void *buf;

	mod->used = true;

	if (mod->name[0] == '*') // submodel
		return mod;

	crc = 0;
	buf = NULL;
	if (mod->isworldmodel != isworldmodel)
		mod->loaded = false;
	if (!mod->loaded || checkdisk)
	{
		if (checkdisk && mod->loaded)
			Con_DPrintf("checking model %s\n", mod->name);
		buf = FS_LoadFile (mod->name, tempmempool, false);
		if (buf)
		{
			crc = CRC_Block(buf, fs_filesize);
			if (mod->crc != crc)
				mod->loaded = false;
		}
	}
	if (mod->loaded)
		return mod; // already loaded

	Con_DPrintf("loading model %s\n", mod->name);
	// LordHavoc: unload the existing model in this slot (if there is one)
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

	// all models use memory, so allocate a memory pool
	mod->mempool = Mem_AllocPool(mod->name, 0, NULL);
	// all models load textures, so allocate a texture pool
	if (cls.state != ca_dedicated)
		mod->texturepool = R_AllocTexturePool();

	if (buf)
	{
		char *bufend = (char *)buf + fs_filesize;
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
		else if (!strcmp(buf, "ACTRHEAD")) Mod_PSKMODEL_Load(mod, buf, bufend);
		else if (strlen(mod->name) >= 4 && !strcmp(mod->name - 4, ".map")) Mod_MAP_Load(mod, buf, bufend);
		else if (!memcmp(buf, "MCBSPpad", 8)) Mod_Q1BSP_Load(mod, buf, bufend);
		else if (num == BSPVERSION || num == 30) Mod_Q1BSP_Load(mod, buf, bufend);
		else Con_Printf("Mod_LoadModel: model \"%s\" is of unknown/unsupported type\n", mod->name);
		Mem_Free(buf);
	}
	else if (crash)
	{
		// LordHavoc: Sys_Error was *ANNOYING*
		Con_Printf ("Mod_LoadModel: %s not found\n", mod->name);
	}

	// no errors occurred
	mod->loaded = true;
	return mod;
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll(void)
{
}

void Mod_ClearUsed(void)
{
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			mod->used = false;
}

void Mod_PurgeUnused(void)
{
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			if (!mod->used)
				Mod_FreeModel(mod);
}

// only used during loading!
void Mod_RemoveStaleWorldModels(model_t *skip)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].isworldmodel && skip != &mod_known[i])
			Mod_UnloadModel(mod_known + i);
}

void Mod_LoadModels(void)
{
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			if (mod->used)
				Mod_CheckLoaded(mod);
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName(const char *name)
{
	int i;
	model_t *mod, *freemod;

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
		mod->loaded = false;
		mod->used = true;
		return mod;
	}

	Host_Error ("Mod_FindName: ran out of models\n");
	return NULL;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName(const char *name, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	return Mod_LoadModel(Mod_FindName(name), crash, checkdisk, isworldmodel);
}

qbyte *mod_base;


//=============================================================================

/*
================
Mod_Print
================
*/
static void Mod_Print(void)
{
	int		i;
	model_t	*mod;

	Con_Print("Loaded models:\n");
	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			Con_Printf("%4iK %s\n", mod->mempool ? (mod->mempool->totalsize + 1023) / 1024 : 0, mod->name);
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
	qbyte *used;
	used = Mem_Alloc(tempmempool, numvertices);
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
		edgehashentries = Mem_Alloc(tempmempool, numtriangles * 3 * sizeof(edgehashentry_t));
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

void Mod_ValidateElements(int *elements, int numtriangles, int numverts, const char *filename, int fileline)
{
	int i, warned = false;
	for (i = 0;i < numtriangles * 3;i++)
	{
		if ((unsigned int)elements[i] >= (unsigned int)numverts)
		{
			if (!warned)
			{
				warned = true;
				Con_Printf("Mod_ValidateElements: out of bounds elements detected at %s:%d\n", filename, fileline);
			}
			elements[i] = 0;
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
	normal3f[0] = v10[1] * v20[2] - v10[2] * v20[1];
	normal3f[1] = v10[2] * v20[0] - v10[0] * v20[2];
	normal3f[2] = v10[0] * v20[1] - v10[1] * v20[0];
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
void Mod_BuildTextureVectorsAndNormals(int firstvertex, int numvertices, int numtriangles, const float *vertex3f, const float *texcoord2f, const int *elements, float *svector3f, float *tvector3f, float *normal3f, qboolean areaweighting)
{
	int i, tnum;
	float sdir[3], tdir[3], normal[3], *v;
	const int *e;
	// clear the vectors
	if (svector3f)
		memset(svector3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	if (tvector3f)
		memset(tvector3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	if (normal3f)
		memset(normal3f + 3 * firstvertex, 0, numvertices * sizeof(float[3]));
	// process each vertex of each triangle and accumulate the results
	for (tnum = 0, e = elements;tnum < numtriangles;tnum++, e += 3)
	{
		Mod_BuildBumpVectors(vertex3f + e[0] * 3, vertex3f + e[1] * 3, vertex3f + e[2] * 3, texcoord2f + e[0] * 2, texcoord2f + e[1] * 2, texcoord2f + e[2] * 2, sdir, tdir, normal);
		if (!areaweighting)
		{
			VectorNormalize(sdir);
			VectorNormalize(tdir);
			VectorNormalize(normal);
		}
		if (svector3f)
			for (i = 0;i < 3;i++)
				VectorAdd(svector3f + e[i]*3, sdir, svector3f + e[i]*3);
		if (tvector3f)
			for (i = 0;i < 3;i++)
				VectorAdd(tvector3f + e[i]*3, tdir, tvector3f + e[i]*3);
		if (normal3f)
			for (i = 0;i < 3;i++)
				VectorAdd(normal3f + e[i]*3, normal, normal3f + e[i]*3);
	}
	// now we could divide the vectors by the number of averaged values on
	// each vertex...  but instead normalize them
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (svector3f)
		for (i = 0, v = svector3f + 3 * firstvertex;i < numvertices;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (tvector3f)
		for (i = 0, v = tvector3f + 3 * firstvertex;i < numvertices;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (normal3f)
		for (i = 0, v = normal3f + 3 * firstvertex;i < numvertices;i++, v += 3)
			VectorNormalize(v);
}

surfmesh_t *Mod_AllocSurfMesh(mempool_t *mempool, int numvertices, int numtriangles, qboolean lightmapoffsets, qboolean vertexcolors, qboolean neighbors)
{
	surfmesh_t *mesh;
	qbyte *data;
	mesh = Mem_Alloc(mempool, sizeof(surfmesh_t) + numvertices * (3 + 3 + 3 + 3 + 2 + 2 + (vertexcolors ? 4 : 0)) * sizeof(float) + numvertices * (lightmapoffsets ? 1 : 0) * sizeof(int) + numtriangles * (3 + 3 + (neighbors ? 3 : 0)) * sizeof(int));
	mesh->num_vertices = numvertices;
	mesh->num_triangles = numtriangles;
	data = (qbyte *)(mesh + 1);
	if (mesh->num_vertices)
	{
		mesh->data_vertex3f = (float *)data, data += sizeof(float[3]) * mesh->num_vertices;
		mesh->data_svector3f = (float *)data, data += sizeof(float[3]) * mesh->num_vertices;
		mesh->data_tvector3f = (float *)data, data += sizeof(float[3]) * mesh->num_vertices;
		mesh->data_normal3f = (float *)data, data += sizeof(float[3]) * mesh->num_vertices;
		mesh->data_texcoordtexture2f = (float *)data, data += sizeof(float[2]) * mesh->num_vertices;
		mesh->data_texcoordlightmap2f = (float *)data, data += sizeof(float[2]) * mesh->num_vertices;
		if (vertexcolors)
			mesh->data_lightmapcolor4f = (float *)data, data += sizeof(float[4]) * mesh->num_vertices;
		if (lightmapoffsets)
			mesh->data_lightmapoffsets = (int *)data, data += sizeof(int) * mesh->num_vertices;
	}
	if (mesh->num_triangles)
	{
		mesh->data_element3i = (int *)data, data += sizeof(int[3]) * mesh->num_triangles;
		if (neighbors)
			mesh->data_neighbor3i = (int *)data, data += sizeof(int[3]) * mesh->num_triangles;
	}
	return mesh;
}

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable)
{
	shadowmesh_t *newmesh;
	qbyte *data;
	int size;
	size = sizeof(shadowmesh_t);
	size += maxverts * sizeof(float[3]);
	if (light)
		size += maxverts * sizeof(float[11]);
	size += maxtriangles * sizeof(int[3]);
	if (neighbors)
		size += maxtriangles * sizeof(int[3]);
	if (expandable)
		size += SHADOWMESHVERTEXHASH * sizeof(shadowmeshvertexhash_t *) + maxverts * sizeof(shadowmeshvertexhash_t);
	data = Mem_Alloc(mempool, size);
	newmesh = (void *)data;data += sizeof(*newmesh);
	newmesh->map_diffuse = map_diffuse;
	newmesh->map_specular = map_specular;
	newmesh->map_normal = map_normal;
	newmesh->maxverts = maxverts;
	newmesh->maxtriangles = maxtriangles;
	newmesh->numverts = 0;
	newmesh->numtriangles = 0;

	newmesh->vertex3f = (void *)data;data += maxverts * sizeof(float[3]);
	if (light)
	{
		newmesh->svector3f = (void *)data;data += maxverts * sizeof(float[3]);
		newmesh->tvector3f = (void *)data;data += maxverts * sizeof(float[3]);
		newmesh->normal3f = (void *)data;data += maxverts * sizeof(float[3]);
		newmesh->texcoord2f = (void *)data;data += maxverts * sizeof(float[2]);
	}
	newmesh->element3i = (void *)data;data += maxtriangles * sizeof(int[3]);
	if (neighbors)
	{
		newmesh->neighbor3i = (void *)data;data += maxtriangles * sizeof(int[3]);
	}
	if (expandable)
	{
		newmesh->vertexhashtable = (void *)data;data += SHADOWMESHVERTEXHASH * sizeof(shadowmeshvertexhash_t *);
		newmesh->vertexhashentries = (void *)data;data += maxverts * sizeof(shadowmeshvertexhash_t);
	}
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
	hashindex = (unsigned int) (vertex14f[0] * 3 + vertex14f[1] * 5 + vertex14f[2] * 7) % SHADOWMESHVERTEXHASH;
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
}

shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable)
{
	return Mod_ShadowMesh_Alloc(mempool, maxverts, maxtriangles, map_diffuse, map_specular, map_normal, light, neighbors, expandable);
}

shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh, int light, int neighbors)
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
		}
		Mem_Free(mesh);
	}
	return firstmesh;
}

void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius)
{
	int i;
	shadowmesh_t *mesh;
	vec3_t nmins, nmaxs, ncenter, temp;
	float nradius2, dist2, *v;
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
		nextmesh = mesh->next;
		Mem_Free(mesh);
	}
}

static rtexture_t *GL_TextureForSkinLayer(const qbyte *in, int width, int height, const char *name, const unsigned int *palette, int textureflags)
{
	int i;
	for (i = 0;i < width*height;i++)
		if (((qbyte *)&palette[in[i]])[3] > 0)
			return R_LoadTexture2D (loadmodel->texturepool, name, width, height, in, TEXTYPE_PALETTE, textureflags, palette);
	return NULL;
}

int Mod_LoadSkinFrame(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int loadglowtexture)
{
	imageskin_t s;
	memset(skinframe, 0, sizeof(*skinframe));
	if (!image_loadskin(&s, basename))
		return false;
	skinframe->base = R_LoadTexture2D (loadmodel->texturepool, basename, s.basepixels_width, s.basepixels_height, s.basepixels, TEXTYPE_RGBA, textureflags, NULL);
	if (s.nmappixels != NULL)
		skinframe->nmap = R_LoadTexture2D (loadmodel->texturepool, va("%s_nmap", basename), s.nmappixels_width, s.nmappixels_height, s.nmappixels, TEXTYPE_RGBA, textureflags, NULL);
	if (s.glosspixels != NULL)
		skinframe->gloss = R_LoadTexture2D (loadmodel->texturepool, va("%s_gloss", basename), s.glosspixels_width, s.glosspixels_height, s.glosspixels, TEXTYPE_RGBA, textureflags, NULL);
	if (s.glowpixels != NULL && loadglowtexture)
		skinframe->glow = R_LoadTexture2D (loadmodel->texturepool, va("%s_glow", basename), s.glowpixels_width, s.glowpixels_height, s.glowpixels, TEXTYPE_RGBA, textureflags, NULL);
	if (s.maskpixels != NULL)
		skinframe->fog = R_LoadTexture2D (loadmodel->texturepool, va("%s_mask", basename), s.maskpixels_width, s.maskpixels_height, s.maskpixels, TEXTYPE_RGBA, textureflags, NULL);
	if (loadpantsandshirt)
	{
		if (s.pantspixels != NULL)
			skinframe->pants = R_LoadTexture2D (loadmodel->texturepool, va("%s_pants", basename), s.pantspixels_width, s.pantspixels_height, s.pantspixels, TEXTYPE_RGBA, textureflags, NULL);
		if (s.shirtpixels != NULL)
			skinframe->shirt = R_LoadTexture2D (loadmodel->texturepool, va("%s_shirt", basename), s.shirtpixels_width, s.shirtpixels_height, s.shirtpixels, TEXTYPE_RGBA, textureflags, NULL);
	}
	if (!skinframe->base)
		skinframe->base = r_texture_notexture;
	if (!skinframe->nmap)
		skinframe->nmap = r_texture_blanknormalmap;
	image_freeskin(&s);
	return true;
}

int Mod_LoadSkinFrame_Internal(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int loadglowtexture, qbyte *skindata, int width, int height)
{
	qbyte *temp1, *temp2;
	memset(skinframe, 0, sizeof(*skinframe));
	if (!skindata)
		return false;
	if (r_shadow_bumpscale_basetexture.value > 0)
	{
		temp1 = Mem_Alloc(loadmodel->mempool, width * height * 8);
		temp2 = temp1 + width * height * 4;
		Image_Copy8bitRGBA(skindata, temp1, width * height, palette_nofullbrights);
		Image_HeightmapToNormalmap(temp1, temp2, width, height, false, r_shadow_bumpscale_basetexture.value);
		skinframe->nmap = R_LoadTexture2D(loadmodel->texturepool, va("%s_nmap", basename), width, height, temp2, TEXTYPE_RGBA, textureflags, NULL);
		Mem_Free(temp1);
	}
	if (loadglowtexture)
	{
		skinframe->glow = GL_TextureForSkinLayer(skindata, width, height, va("%s_glow", basename), palette_onlyfullbrights, textureflags); // glow
		skinframe->base = skinframe->merged = GL_TextureForSkinLayer(skindata, width, height, va("%s_merged", basename), palette_nofullbrights, textureflags); // all but fullbrights
		if (loadpantsandshirt)
		{
			skinframe->pants = GL_TextureForSkinLayer(skindata, width, height, va("%s_pants", basename), palette_pantsaswhite, textureflags); // pants
			skinframe->shirt = GL_TextureForSkinLayer(skindata, width, height, va("%s_shirt", basename), palette_shirtaswhite, textureflags); // shirt
			if (skinframe->pants || skinframe->shirt)
				skinframe->base = GL_TextureForSkinLayer(skindata, width, height, va("%s_nospecial", basename), palette_nocolormapnofullbrights, textureflags); // no special colors
		}
	}
	else
	{
		skinframe->base = skinframe->merged = GL_TextureForSkinLayer(skindata, width, height, va("%s_merged", basename), palette_complete, textureflags); // all
		if (loadpantsandshirt)
		{
			skinframe->pants = GL_TextureForSkinLayer(skindata, width, height, va("%s_pants", basename), palette_pantsaswhite, textureflags); // pants
			skinframe->shirt = GL_TextureForSkinLayer(skindata, width, height, va("%s_shirt", basename), palette_shirtaswhite, textureflags); // shirt
			if (skinframe->pants || skinframe->shirt)
				skinframe->base = GL_TextureForSkinLayer(skindata, width, height, va("%s_nospecial", basename), palette_nocolormap, textureflags); // no pants or shirt
		}
	}
	if (!skinframe->nmap)
		skinframe->nmap = r_texture_blanknormalmap;
	return true;
}

void Mod_GetTerrainVertex3fTexCoord2fFromRGBA(const qbyte *imagepixels, int imagewidth, int imageheight, int ix, int iy, float *vertex3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
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

void Mod_GetTerrainVertexFromRGBA(const qbyte *imagepixels, int imagewidth, int imageheight, int ix, int iy, float *vertex3f, float *svector3f, float *tvector3f, float *normal3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
{
	float vup[3], vdown[3], vleft[3], vright[3];
	float tcup[3], tcdown[3], tcleft[3], tcright[3];
	float sv[3], tv[3], nl[3];
	Mod_GetTerrainVertex3fTexCoord2fFromRGBA(imagepixels, imagewidth, imageheight, ix, iy, vertex3f, texcoord2f, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromRGBA(imagepixels, imagewidth, imageheight, ix, iy - 1, vup, tcup, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromRGBA(imagepixels, imagewidth, imageheight, ix, iy + 1, vdown, tcdown, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromRGBA(imagepixels, imagewidth, imageheight, ix - 1, iy, vleft, tcleft, pixelstepmatrix, pixeltexturestepmatrix);
	Mod_GetTerrainVertex3fTexCoord2fFromRGBA(imagepixels, imagewidth, imageheight, ix + 1, iy, vright, tcright, pixelstepmatrix, pixeltexturestepmatrix);
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

void Mod_ConstructTerrainPatchFromRGBA(const qbyte *imagepixels, int imagewidth, int imageheight, int x1, int y1, int width, int height, int *element3i, int *neighbor3i, float *vertex3f, float *svector3f, float *tvector3f, float *normal3f, float *texcoord2f, matrix4x4_t *pixelstepmatrix, matrix4x4_t *pixeltexturestepmatrix)
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
			Mod_GetTerrainVertexFromRGBA(imagepixels, imagewidth, imageheight, ix, iy, vertex3f, texcoord2f, svector3f, tvector3f, normal3f, pixelstepmatrix, pixeltexturestepmatrix);
}

skinfile_t *Mod_LoadSkinFiles(void)
{
	int i, words, numtags, line, tagsetsused = false, wordsoverflow;
	char *text;
	const char *data;
	skinfile_t *skinfile = NULL, *first = NULL;
	skinfileitem_t *skinfileitem;
	char word[10][MAX_QPATH];
	overridetagnameset_t tagsets[MAX_SKINS];
	overridetagname_t tags[256];

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
	memset(tagsets, 0, sizeof(tagsets));
	memset(word, 0, sizeof(word));
	for (i = 0;i < MAX_SKINS && (data = text = FS_LoadFile(va("%s_%i.skin", loadmodel->name, i), tempmempool, true));i++)
	{
		numtags = 0;

		// If it's the first file we parse
		if (skinfile == NULL)
		{
			skinfile = Mem_Alloc(tempmempool, sizeof(skinfile_t));
			first = skinfile;
		}
		else
		{
			skinfile->next = Mem_Alloc(tempmempool, sizeof(skinfile_t));
			skinfile = skinfile->next;
		}
		skinfile->next = NULL;

		for(line = 0;;line++)
		{
			// parse line
			if (!COM_ParseToken(&data, true))
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
			while (COM_ParseToken(&data, true) && strcmp(com_token, "\n"));
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
					Con_DPrintf("Mod_LoadSkinFiles: parsed mesh \"%s\" shader replacement \"%s\"\n", word[1], word[2]);
					skinfileitem = Mem_Alloc(tempmempool, sizeof(skinfileitem_t));
					skinfileitem->next = skinfile->items;
					skinfile->items = skinfileitem;
					strlcpy (skinfileitem->name, word[1], sizeof (skinfileitem->name));
					strlcpy (skinfileitem->replacement, word[2], sizeof (skinfileitem->replacement));
				}
				else
					Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: wrong number of parameters to command \"%s\", see documentation in DP_GFX_SKINFILES extension in dpextensions.qc\n", loadmodel->name, i, line, word[0]);
			}
			else if (words == 2 && !strcmp(word[1], ","))
			{
				// tag name, like "tag_weapon,"
				Con_DPrintf("Mod_LoadSkinFiles: parsed tag #%i \"%s\"\n", numtags, word[0]);
				memset(tags + numtags, 0, sizeof(tags[numtags]));
				strlcpy (tags[numtags].name, word[0], sizeof (tags[numtags].name));
				numtags++;
			}
			else if (words == 3 && !strcmp(word[1], ","))
			{
				// mesh shader name, like "U_RArm,models/players/Legoman/BikerA1.tga"
				Con_DPrintf("Mod_LoadSkinFiles: parsed mesh \"%s\" shader replacement \"%s\"\n", word[0], word[2]);
				skinfileitem = Mem_Alloc(tempmempool, sizeof(skinfileitem_t));
				skinfileitem->next = skinfile->items;
				skinfile->items = skinfileitem;
				strlcpy (skinfileitem->name, word[0], sizeof (skinfileitem->name));
				strlcpy (skinfileitem->replacement, word[2], sizeof (skinfileitem->replacement));
			}
			else
				Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: does not look like tag or mesh specification, or replace command, see documentation in DP_GFX_SKINFILES extension in dpextensions.qc\n", loadmodel->name, i, line);
		}
		Mem_Free(text);

		if (numtags)
		{
			overridetagnameset_t *t;
			t = tagsets + i;
			t->num_overridetagnames = numtags;
			t->data_overridetagnames = Mem_Alloc(loadmodel->mempool, t->num_overridetagnames * sizeof(overridetagname_t));
			memcpy(t->data_overridetagnames, tags, t->num_overridetagnames * sizeof(overridetagname_t));
			tagsetsused = true;
		}
	}
	if (tagsetsused)
	{
		loadmodel->data_overridetagnamesforskin = Mem_Alloc(loadmodel->mempool, i * sizeof(overridetagnameset_t));
		memcpy(loadmodel->data_overridetagnamesforskin, tagsets, i * sizeof(overridetagnameset_t));
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
	float d, edgedir[3], temp[3];
	// a degenerate triangle is one with no width (thickness, surface area)
	// these are characterized by having all 3 points colinear (along a line)
	// or having two points identical
	for (i = 0, outtriangles = 0;i < numtriangles;i++, inelement3i += 3)
	{
		// calculate first edge
		VectorSubtract(vertex3f + inelement3i[1] * 3, vertex3f + inelement3i[0] * 3, edgedir);
		if (VectorLength2(edgedir) < 0.0001f)
			continue; // degenerate first edge (no length)
		VectorNormalize(edgedir);
		// check if third point is on the edge (colinear)
		d = -DotProduct(vertex3f + inelement3i[2] * 3, edgedir);
		VectorMA(vertex3f + inelement3i[2] * 3, d, edgedir, temp);
		if (VectorLength2(temp) < 0.0001f)
			continue; // third point colinear with first edge
		// valid triangle (no colinear points, no duplicate points)
		VectorCopy(inelement3i, outelement3i);
		outelement3i += 3;
		outtriangles++;
	}
	return outtriangles;
}

