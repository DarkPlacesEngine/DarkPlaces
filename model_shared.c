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

// LordHavoc: increased from 512 to 2048
#define	MAX_MOD_KNOWN	2048
static model_t mod_known[MAX_MOD_KNOWN];

rtexturepool_t *mod_shared_texturepool;
rtexture_t *r_notexture;
rtexture_t *mod_shared_detailtextures[NUM_DETAILTEXTURES];

void Mod_BuildDetailTextures (void)
{
	int i, x, y, light;
	float vc[3], vx[3], vy[3], vn[3], lightdir[3];
#define DETAILRESOLUTION 256
	qbyte data[DETAILRESOLUTION][DETAILRESOLUTION][4], noise[DETAILRESOLUTION][DETAILRESOLUTION];
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
		mod_shared_detailtextures[i] = R_LoadTexture2D(mod_shared_texturepool, va("detailtexture%i", i), DETAILRESOLUTION, DETAILRESOLUTION, &data[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP | TEXF_PRECACHE, NULL);
	}
}

texture_t r_surf_notexture;

void Mod_SetupNoTexture(void)
{
	int x, y;
	qbyte pix[16][16][4];

	// this makes a light grey/dark grey checkerboard texture
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

	r_notexture = R_LoadTexture2D(mod_shared_texturepool, "notexture", 16, 16, &pix[0][0][0], TEXTYPE_RGBA, TEXF_MIPMAP, NULL);
}

static void mod_start(void)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);
	Mod_LoadModels();

	mod_shared_texturepool = R_AllocTexturePool();
	Mod_SetupNoTexture();
	Mod_BuildDetailTextures();
}

static void mod_shutdown(void)
{
	int i;
	for (i = 0;i < MAX_MOD_KNOWN;i++)
		if (mod_known[i].name[0])
			Mod_UnloadModel(&mod_known[i]);

	R_FreeTexturePool(&mod_shared_texturepool);
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
static model_t *Mod_LoadModel(model_t *mod, qboolean crash, qboolean checkdisk, qboolean isworldmodel)
{
	int num;
	unsigned int crc;
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
			buf = FS_LoadFile (mod->name, false);
			if (!buf)
			{
				if (crash)
					Host_Error ("Mod_LoadModel: %s not found", mod->name); // LordHavoc: Sys_Error was *ANNOYING*
				return NULL;
			}

			crc = CRC_Block(buf, fs_filesize);
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
		buf = FS_LoadFile (mod->name, false);
		if (!buf)
		{
			if (crash)
				Host_Error ("Mod_LoadModel: %s not found", mod->name);
			return NULL;
		}
		crc = CRC_Block(buf, fs_filesize);
	}

	// allocate a new model
	loadmodel = mod;

	// LordHavoc: unload the existing model in this slot (if there is one)
	Mod_UnloadModel(mod);
	mod->isworldmodel = isworldmodel;
	mod->used = true;
	mod->crc = crc;
	// errors can prevent the corresponding mod->needload = false;
	mod->needload = true;

	// all models use memory, so allocate a memory pool
	mod->mempool = Mem_AllocPool(mod->name);
	// all models load textures, so allocate a texture pool
	if (cls.state != ca_dedicated)
		mod->texturepool = R_AllocTexturePool();

	// call the apropriate loader
	num = LittleLong(*((int *)buf));
	     if (!memcmp(buf, "IDPO", 4)) Mod_IDP0_Load(mod, buf);
	else if (!memcmp(buf, "IDP2", 4)) Mod_IDP2_Load(mod, buf);
	else if (!memcmp(buf, "IDP3", 4)) Mod_IDP3_Load(mod, buf);
	else if (!memcmp(buf, "IDSP", 4)) Mod_IDSP_Load(mod, buf);
	else if (!memcmp(buf, "IBSP", 4)) Mod_IBSP_Load(mod, buf);
	else if (!memcmp(buf, "ZYMOTICMODEL", 12)) Mod_ZYMOTICMODEL_Load(mod, buf);
	else if (strlen(mod->name) >= 4 && !strcmp(mod->name - 4, ".map")) Mod_MAP_Load(mod, buf);
	else if (num == BSPVERSION || num == 30) Mod_Q1BSP_Load(mod, buf);
	else Host_Error("Mod_LoadModel: model \"%s\" is of unknown/unsupported type\n", mod->name);

	Mem_Free(buf);

	// no errors occurred
	mod->needload = false;
	return mod;
}

void Mod_CheckLoaded(model_t *mod)
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
void Mod_TouchModel(const char *name)
{
	model_t	*mod;

	mod = Mod_FindName(name);
	mod->used = true;
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

	Con_Printf ("Loaded models:\n");
	for (i = 0, mod = mod_known;i < MAX_MOD_KNOWN;i++, mod++)
		if (mod->name[0])
			Con_Printf ("%4iK %s\n", mod->mempool ? (mod->mempool->totalsize + 1023) / 1024 : 0, mod->name);
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
		Con_Printf("usage: modelprecache <filename>\n");
}

int Mod_FindTriangleWithEdge(const int *elements, int numtriangles, int start, int end, int ignore)
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

void Mod_ValidateElements(const int *elements, int numtriangles, int numverts, const char *filename, int fileline)
{
	int i;
	for (i = 0;i < numtriangles * 3;i++)
		if ((unsigned int)elements[i] >= (unsigned int)numverts)
			Con_Printf("Mod_ValidateElements: out of bounds element detected at %s:%d\n", filename, fileline);
}

void Mod_BuildBumpVectors(const float *v0, const float *v1, const float *v2, const float *tc0, const float *tc1, const float *tc2, float *svector3f, float *tvector3f, float *normal3f)
{
	float f;
	normal3f[0] = (v1[1] - v0[1]) * (v2[2] - v0[2]) - (v1[2] - v0[2]) * (v2[1] - v0[1]);
	normal3f[1] = (v1[2] - v0[2]) * (v2[0] - v0[0]) - (v1[0] - v0[0]) * (v2[2] - v0[2]);
	normal3f[2] = (v1[0] - v0[0]) * (v2[1] - v0[1]) - (v1[1] - v0[1]) * (v2[0] - v0[0]);
	VectorNormalize(normal3f);
	tvector3f[0] = ((tc1[0] - tc0[0]) * (v2[0] - v0[0]) - (tc2[0] - tc0[0]) * (v1[0] - v0[0]));
	tvector3f[1] = ((tc1[0] - tc0[0]) * (v2[1] - v0[1]) - (tc2[0] - tc0[0]) * (v1[1] - v0[1]));
	tvector3f[2] = ((tc1[0] - tc0[0]) * (v2[2] - v0[2]) - (tc2[0] - tc0[0]) * (v1[2] - v0[2]));
	f = -DotProduct(tvector3f, normal3f);
	VectorMA(tvector3f, f, normal3f, tvector3f);
	VectorNormalize(tvector3f);
	CrossProduct(normal3f, tvector3f, svector3f);
}

// warning: this is an expensive function!
void Mod_BuildTextureVectorsAndNormals(int numverts, int numtriangles, const float *vertex3f, const float *texcoord2f, const int *elements, float *svector3f, float *tvector3f, float *normal3f)
{
	int i, tnum;
	float sdir[3], tdir[3], normal[3], *v;
	const int *e;
	// clear the vectors
	if (svector3f)
		memset(svector3f, 0, numverts * sizeof(float[3]));
	if (tvector3f)
		memset(tvector3f, 0, numverts * sizeof(float[3]));
	if (normal3f)
		memset(normal3f, 0, numverts * sizeof(float[3]));
	// process each vertex of each triangle and accumulate the results
	for (tnum = 0, e = elements;tnum < numtriangles;tnum++, e += 3)
	{
		Mod_BuildBumpVectors(vertex3f + e[0] * 3, vertex3f + e[1] * 3, vertex3f + e[2] * 3, texcoord2f + e[0] * 2, texcoord2f + e[1] * 2, texcoord2f + e[2] * 2, sdir, tdir, normal);
		if (svector3f)
		{
			for (i = 0;i < 3;i++)
			{
				svector3f[e[i]*3  ] += sdir[0];
				svector3f[e[i]*3+1] += sdir[1];
				svector3f[e[i]*3+2] += sdir[2];
			}
		}
		if (tvector3f)
		{
			for (i = 0;i < 3;i++)
			{
				tvector3f[e[i]*3  ] += tdir[0];
				tvector3f[e[i]*3+1] += tdir[1];
				tvector3f[e[i]*3+2] += tdir[2];
			}
		}
		if (normal3f)
		{
			for (i = 0;i < 3;i++)
			{
				normal3f[e[i]*3  ] += normal[0];
				normal3f[e[i]*3+1] += normal[1];
				normal3f[e[i]*3+2] += normal[2];
			}
		}
	}
	// now we could divide the vectors by the number of averaged values on
	// each vertex...  but instead normalize them
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (svector3f)
		for (i = 0, v = svector3f;i < numverts;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (tvector3f)
		for (i = 0, v = tvector3f;i < numverts;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (normal3f)
		for (i = 0, v = normal3f;i < numverts;i++, v += 3)
			VectorNormalize(v);
}

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts)
{
	shadowmesh_t *mesh;
	mesh = Mem_Alloc(mempool, sizeof(shadowmesh_t) + maxverts * sizeof(float[3]) + maxverts * sizeof(int[3]) + maxverts * sizeof(int[3]) + SHADOWMESHVERTEXHASH * sizeof(shadowmeshvertexhash_t *) + maxverts * sizeof(shadowmeshvertexhash_t));
	mesh->maxverts = maxverts;
	mesh->maxtriangles = maxverts;
	mesh->numverts = 0;
	mesh->numtriangles = 0;
	mesh->vertex3f = (float *)(mesh + 1);
	mesh->element3i = (int *)(mesh->vertex3f + mesh->maxverts * 3);
	mesh->neighbor3i = (int *)(mesh->element3i + mesh->maxtriangles * 3);
	mesh->vertexhashtable = (shadowmeshvertexhash_t **)(mesh->neighbor3i + mesh->maxtriangles * 3);
	mesh->vertexhashentries = (shadowmeshvertexhash_t *)(mesh->vertexhashtable + SHADOWMESHVERTEXHASH);
	return mesh;
}

shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh)
{
	shadowmesh_t *newmesh;
	newmesh = Mem_Alloc(mempool, sizeof(shadowmesh_t) + oldmesh->numverts * sizeof(float[3]) + oldmesh->numtriangles * sizeof(int[3]) + oldmesh->numtriangles * sizeof(int[3]));
	newmesh->maxverts = newmesh->numverts = oldmesh->numverts;
	newmesh->maxtriangles = newmesh->numtriangles = oldmesh->numtriangles;
	newmesh->vertex3f = (float *)(newmesh + 1);
	newmesh->element3i = (int *)(newmesh->vertex3f + newmesh->maxverts * 3);
	newmesh->neighbor3i = (int *)(newmesh->element3i + newmesh->maxtriangles * 3);
	memcpy(newmesh->vertex3f, oldmesh->vertex3f, newmesh->numverts * sizeof(float[3]));
	memcpy(newmesh->element3i, oldmesh->element3i, newmesh->numtriangles * sizeof(int[3]));
	memcpy(newmesh->neighbor3i, oldmesh->neighbor3i, newmesh->numtriangles * sizeof(int[3]));
	return newmesh;
}

int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *v)
{
	int hashindex;
	float *m;
	shadowmeshvertexhash_t *hash;
	// this uses prime numbers intentionally
	hashindex = (int) (v[0] * 3 + v[1] * 5 + v[2] * 7) % SHADOWMESHVERTEXHASH;
	for (hash = mesh->vertexhashtable[hashindex];hash;hash = hash->next)
	{
		m = mesh->vertex3f + (hash - mesh->vertexhashentries) * 3;
		if (m[0] == v[0] && m[1] == v[1] &&  m[2] == v[2])
			return hash - mesh->vertexhashentries;
	}
	hash = mesh->vertexhashentries + mesh->numverts;
	hash->next = mesh->vertexhashtable[hashindex];
	mesh->vertexhashtable[hashindex] = hash;
	m = mesh->vertex3f + (hash - mesh->vertexhashentries) * 3;
	VectorCopy(v, m);
	mesh->numverts++;
	return mesh->numverts - 1;
}

void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, float *vert0, float *vert1, float *vert2)
{
	while (mesh->numverts + 3 > mesh->maxverts || mesh->numtriangles + 1 > mesh->maxtriangles)
	{
		if (mesh->next == NULL)
			mesh->next = Mod_ShadowMesh_Alloc(mempool, max(mesh->maxtriangles, 1));
		mesh = mesh->next;
	}
	mesh->element3i[mesh->numtriangles * 3 + 0] = Mod_ShadowMesh_AddVertex(mesh, vert0);
	mesh->element3i[mesh->numtriangles * 3 + 1] = Mod_ShadowMesh_AddVertex(mesh, vert1);
	mesh->element3i[mesh->numtriangles * 3 + 2] = Mod_ShadowMesh_AddVertex(mesh, vert2);
	mesh->numtriangles++;
}

void Mod_ShadowMesh_AddPolygon(mempool_t *mempool, shadowmesh_t *mesh, int numverts, float *verts)
{
	int i;
	float *v;
	for (i = 0, v = verts + 3;i < numverts - 2;i++, v += 3)
		Mod_ShadowMesh_AddTriangle(mempool, mesh, verts, v, v + 3);
	/*
	int i, i1, i2, i3;
	float *v;
	while (mesh->numverts + numverts > mesh->maxverts || mesh->numtriangles + (numverts - 2) > mesh->maxtriangles)
	{
		if (mesh->next == NULL)
			mesh->next = Mod_ShadowMesh_Alloc(mempool, max(mesh->maxtriangles, numverts));
		mesh = mesh->next;
	}
	i1 = Mod_ShadowMesh_AddVertex(mesh, verts);
	i2 = 0;
	i3 = Mod_ShadowMesh_AddVertex(mesh, verts + 3);
	for (i = 0, v = verts + 6;i < numverts - 2;i++, v += 3)
	{
		i2 = i3;
		i3 = Mod_ShadowMesh_AddVertex(mesh, v);
		mesh->elements[mesh->numtriangles * 3 + 0] = i1;
		mesh->elements[mesh->numtriangles * 3 + 1] = i2;
		mesh->elements[mesh->numtriangles * 3 + 2] = i3;
		mesh->numtriangles++;
	}
	*/
}

void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, float *verts, int numtris, int *elements)
{
	int i;
	for (i = 0;i < numtris;i++, elements += 3)
		Mod_ShadowMesh_AddTriangle(mempool, mesh, verts + elements[0] * 3, verts + elements[1] * 3, verts + elements[2] * 3);
}

shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int initialnumtriangles)
{
	return Mod_ShadowMesh_Alloc(mempool, initialnumtriangles);
}

shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh)
{
#if 1
	//int i;
	shadowmesh_t *mesh, *newmesh, *nextmesh;
	// reallocate meshs to conserve space
	for (mesh = firstmesh, firstmesh = NULL;mesh;mesh = nextmesh)
	{
		nextmesh = mesh->next;
		if (mesh->numverts >= 3 && mesh->numtriangles >= 1)
		{
			newmesh = Mod_ShadowMesh_ReAlloc(mempool, mesh);
			newmesh->next = firstmesh;
			firstmesh = newmesh;
			//Con_Printf("mesh\n");
			//for (i = 0;i < newmesh->numtriangles;i++)
			//	Con_Printf("tri %d %d %d\n", newmesh->elements[i * 3 + 0], newmesh->elements[i * 3 + 1], newmesh->elements[i * 3 + 2]);
			Mod_ValidateElements(newmesh->element3i, newmesh->numtriangles, newmesh->numverts, __FILE__, __LINE__);
			Mod_BuildTriangleNeighbors(newmesh->neighbor3i, newmesh->element3i, newmesh->numtriangles);
		}
		Mem_Free(mesh);
	}
#else
	shadowmesh_t *mesh;
	for (mesh = firstmesh;mesh;mesh = mesh->next)
	{
		Mod_ValidateElements(mesh->elements, mesh->numtriangles, mesh->numverts, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->neighbors, mesh->elements, mesh->numtriangles);
	}
#endif
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

static int detailtexturecycle = 0;
int Mod_LoadSkinFrame(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture)
{
	imageskin_t s;
	memset(skinframe, 0, sizeof(*skinframe));
	if (!image_loadskin(&s, basename))
		return false;
	if (usedetailtexture)
		skinframe->detail = mod_shared_detailtextures[(detailtexturecycle++) % NUM_DETAILTEXTURES];
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
	image_freeskin(&s);
	return true;
}

int Mod_LoadSkinFrame_Internal(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture, qbyte *skindata, int width, int height)
{
	qbyte *temp1, *temp2;
	memset(skinframe, 0, sizeof(*skinframe));
	if (!skindata)
		return false;
	if (usedetailtexture)
		skinframe->detail = mod_shared_detailtextures[(detailtexturecycle++) % NUM_DETAILTEXTURES];
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
	skinfile_t *skinfile, *first = NULL;
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
	for (i = 0;i < MAX_SKINS && (data = text = FS_LoadFile(va("%s_%i.skin", loadmodel->name, i), true));i++)
	{
		numtags = 0;
		skinfile = Mem_Alloc(tempmempool, sizeof(skinfile_t));
		skinfile->next = first;
		first = skinfile;
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
					strncpy(word[words++], com_token, MAX_QPATH - 1);
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
					strncpy(skinfileitem->name, word[1], sizeof(skinfileitem->name) - 1);
					strncpy(skinfileitem->replacement, word[2], sizeof(skinfileitem->replacement) - 1);
				}
				else
					Con_Printf("Mod_LoadSkinFiles: parsing error in file \"%s_%i.skin\" on line #%i: wrong number of parameters to command \"%s\", see documentation in DP_GFX_SKINFILES extension in dpextensions.qc\n", loadmodel->name, i, line, word[0]);
			}
			else if (words == 2 && !strcmp(word[1], ","))
			{
				// tag name, like "tag_weapon,"
				Con_DPrintf("Mod_LoadSkinFiles: parsed tag #%i \"%s\"\n", numtags, word[0]);
				memset(tags + numtags, 0, sizeof(tags[numtags]));
				strncpy(tags[numtags].name, word[0], sizeof(tags[numtags].name) - 1);
				numtags++;
			}
			else if (words == 3 && !strcmp(word[1], ","))
			{
				// mesh shader name, like "U_RArm,models/players/Legoman/BikerA1.tga"
				Con_DPrintf("Mod_LoadSkinFiles: parsed mesh \"%s\" shader replacement \"%s\"\n", word[0], word[2]);
				skinfileitem = Mem_Alloc(tempmempool, sizeof(skinfileitem_t));
				skinfileitem->next = skinfile->items;
				skinfile->items = skinfileitem;
				strncpy(skinfileitem->name, word[0], sizeof(skinfileitem->name) - 1);
				strncpy(skinfileitem->replacement, word[2], sizeof(skinfileitem->replacement) - 1);
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


