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
#include "r_shadow.h"

static cvar_t r_mipskins = {CVAR_SAVE, "r_mipskins", "0"};

void Mod_AliasInit (void)
{
	Cvar_RegisterVariable(&r_mipskins);
}

static void Mod_CalcAliasModelBBoxes (void)
{
	int vnum, meshnum;
	float dist, yawradius, radius;
	aliasmesh_t *mesh;
	aliasvertex_t *v;
	VectorClear(loadmodel->normalmins);
	VectorClear(loadmodel->normalmaxs);
	yawradius = 0;
	radius = 0;
	for (meshnum = 0, mesh = loadmodel->aliasdata_meshes;meshnum < loadmodel->aliasnum_meshes;meshnum++, mesh++)
	{
		for (vnum = 0, v = mesh->data_aliasvertex;vnum < mesh->num_vertices * mesh->num_frames;vnum++, v++)
		{
			if (loadmodel->normalmins[0] > v->origin[0]) loadmodel->normalmins[0] = v->origin[0];
			if (loadmodel->normalmins[1] > v->origin[1]) loadmodel->normalmins[1] = v->origin[1];
			if (loadmodel->normalmins[2] > v->origin[2]) loadmodel->normalmins[2] = v->origin[2];
			if (loadmodel->normalmaxs[0] < v->origin[0]) loadmodel->normalmaxs[0] = v->origin[0];
			if (loadmodel->normalmaxs[1] < v->origin[1]) loadmodel->normalmaxs[1] = v->origin[1];
			if (loadmodel->normalmaxs[2] < v->origin[2]) loadmodel->normalmaxs[2] = v->origin[2];
			dist = v->origin[0] * v->origin[0] + v->origin[1] * v->origin[1];
			if (yawradius < dist)
				yawradius = dist;
			dist += v->origin[2] * v->origin[2];
			if (radius < dist)
				radius = dist;
		}
	}
	radius = sqrt(radius);
	yawradius = sqrt(yawradius);
	loadmodel->yawmins[0] = loadmodel->yawmins[1] = -yawradius;
	loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] = yawradius;
	loadmodel->yawmins[2] = loadmodel->normalmins[2];
	loadmodel->yawmaxs[2] = loadmodel->normalmaxs[2];
	loadmodel->rotatedmins[0] = loadmodel->rotatedmins[1] = loadmodel->rotatedmins[2] = -radius;
	loadmodel->rotatedmaxs[0] = loadmodel->rotatedmaxs[1] = loadmodel->rotatedmaxs[2] = radius;
	loadmodel->radius = radius;
	loadmodel->radius2 = radius * radius;
}

static void Mod_ConvertAliasVerts (int inverts, vec3_t scale, vec3_t translate, trivertx_t *v, aliasvertex_t *out, int *vertremap)
{
	int i, j;
	vec3_t temp;
	for (i = 0;i < inverts;i++)
	{
		if (vertremap[i] < 0 && vertremap[i+inverts] < 0) // only used vertices need apply...
			continue;
		temp[0] = v[i].v[0] * scale[0] + translate[0];
		temp[1] = v[i].v[1] * scale[1] + translate[1];
		temp[2] = v[i].v[2] * scale[2] + translate[2];
		j = vertremap[i]; // not onseam
		if (j >= 0)
			VectorCopy(temp, out[j].origin);
		j = vertremap[i+inverts]; // onseam
		if (j >= 0)
			VectorCopy(temp, out[j].origin);
	}
}

static void Mod_BuildAliasVertexTextureVectors(int numverts, aliasvertex_t *vertices, const float *texcoords, float *vertexbuffer, float *svectorsbuffer, float *tvectorsbuffer, float *normalsbuffer, int numtriangles, const int *elements)
{
	int i;
	for (i = 0;i < numverts;i++)
		VectorCopy(vertices[i].origin, &vertexbuffer[i * 3]);
	Mod_BuildTextureVectorsAndNormals(numverts, numtriangles, vertexbuffer, texcoords, elements, svectorsbuffer, tvectorsbuffer, normalsbuffer);
	for (i = 0;i < numverts;i++)
	{
		vertices[i].normal[0] = normalsbuffer[i * 3 + 0];
		vertices[i].normal[1] = normalsbuffer[i * 3 + 1];
		vertices[i].normal[2] = normalsbuffer[i * 3 + 2];
		vertices[i].svector[0] = svectorsbuffer[i * 3 + 0];
		vertices[i].svector[1] = svectorsbuffer[i * 3 + 1];
		vertices[i].svector[2] = svectorsbuffer[i * 3 + 2];
	}
}

static void Mod_MDL_LoadFrames (qbyte* datapointer, int inverts, int outverts, vec3_t scale, vec3_t translate, float *texcoords, aliasvertex_t *posedata, int numtris, int *elements, int *vertremap)
{
	int i, f, pose, groupframes;
	float interval, *vertexbuffer, *svectorsbuffer, *tvectorsbuffer, *normalsbuffer;
	daliasframetype_t *pframetype;
	daliasframe_t *pinframe;
	daliasgroup_t *group;
	daliasinterval_t *intervals;
	animscene_t *scene;
	pose = 0;
	scene = loadmodel->animscenes;
	vertexbuffer = Mem_Alloc(tempmempool, outverts * sizeof(float[3+3+3+3]));
	svectorsbuffer = vertexbuffer + outverts * 3;
	tvectorsbuffer = svectorsbuffer + outverts * 3;
	normalsbuffer = tvectorsbuffer + outverts * 3;
	for (f = 0;f < loadmodel->numframes;f++)
	{
		pframetype = (daliasframetype_t *)datapointer;
		datapointer += sizeof(daliasframetype_t);
		if (LittleLong (pframetype->type) == ALIAS_SINGLE)
		{
			// a single frame is still treated as a group
			interval = 0.1f;
			groupframes = 1;
		}
		else
		{
			// read group header
			group = (daliasgroup_t *)datapointer;
			datapointer += sizeof(daliasgroup_t);
			groupframes = LittleLong (group->numframes);

			// intervals (time per frame)
			intervals = (daliasinterval_t *)datapointer;
			datapointer += sizeof(daliasinterval_t) * groupframes;

			interval = LittleFloat (intervals->interval); // FIXME: support variable framerate groups
			if (interval < 0.01f)
				Host_Error("Mod_MDL_LoadFrames: invalid interval");
		}

		// get scene name from first frame
		pinframe = (daliasframe_t *)datapointer;

		strcpy(scene->name, pinframe->name);
		scene->firstframe = pose;
		scene->framecount = groupframes;
		scene->framerate = 1.0f / interval;
		scene->loop = true;
		scene++;

		// read frames
		for (i = 0;i < groupframes;i++)
		{
			pinframe = (daliasframe_t *)datapointer;
			datapointer += sizeof(daliasframe_t);
			Mod_ConvertAliasVerts(inverts, scale, translate, (trivertx_t *)datapointer, posedata + pose * outverts, vertremap);
			Mod_BuildAliasVertexTextureVectors(outverts, posedata + pose * outverts, texcoords, vertexbuffer, svectorsbuffer, tvectorsbuffer, normalsbuffer, numtris, elements);
			datapointer += sizeof(trivertx_t) * inverts;
			pose++;
		}
	}
	Mem_Free(vertexbuffer);
}

aliaslayer_t mod_alias_layersbuffer[16]; // 7 currently used
void Mod_BuildAliasSkinFromSkinFrame(aliasskin_t *skin, skinframe_t *skinframe)
{
	aliaslayer_t *layer;

	memset(&mod_alias_layersbuffer, 0, sizeof(mod_alias_layersbuffer));
	layer = mod_alias_layersbuffer;
	layer->flags = ALIASLAYER_SPECULAR;
	layer->texture = skinframe->gloss;
	layer->nmap = skinframe->nmap;
	layer++;
	if (skinframe->merged != NULL)
	{
		layer->flags = ALIASLAYER_DIFFUSE | ALIASLAYER_NODRAW_IF_COLORMAPPED;
		layer->texture = skinframe->merged;
		layer->nmap = skinframe->nmap;
		layer++;
	}
	if (skinframe->base != NULL)
	{
		layer->flags = ALIASLAYER_DIFFUSE;
		if (skinframe->merged != NULL)
			layer->flags |= ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED;
		layer->texture = skinframe->base;
		layer->nmap = skinframe->nmap;
		layer++;
	}
	if (skinframe->pants != NULL)
	{
		layer->flags = ALIASLAYER_DIFFUSE | ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED | ALIASLAYER_COLORMAP_PANTS;
		layer->texture = skinframe->pants;
		layer->nmap = skinframe->nmap;
		layer++;
	}
	if (skinframe->shirt != NULL)
	{
		layer->flags = ALIASLAYER_DIFFUSE | ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED | ALIASLAYER_COLORMAP_SHIRT;
		layer->texture = skinframe->shirt;
		layer->nmap = skinframe->nmap;
		layer++;
	}

	if (skinframe->glow != NULL)
	{
		layer->flags = 0;
		layer->texture = skinframe->glow;
		layer++;
		layer->flags = ALIASLAYER_FOG;
		layer->texture = skinframe->fog;
		layer++;
	}
	else
	{
		layer->flags = ALIASLAYER_FOG | ALIASLAYER_FORCEDRAW_IF_FIRSTPASS;
		layer->texture = skinframe->fog;
		layer++;
	}

	skin->flags = 0;
	// fog texture only exists if some pixels are transparent...
	if (skinframe->fog != NULL)
		skin->flags |= ALIASSKIN_TRANSPARENT;

	skin->num_layers = layer - mod_alias_layersbuffer;
	skin->data_layers = Mem_Alloc(loadmodel->mempool, skin->num_layers * sizeof(aliaslayer_t));
	memcpy(skin->data_layers, mod_alias_layersbuffer, skin->num_layers * sizeof(aliaslayer_t));
}

void Mod_BuildMDLMD2MeshInfo(int numverts, int numtris, int *elements, float *texcoord2f, aliasvertex_t *posedata)
{
	int i;
	aliasmesh_t *mesh;

	loadmodel->aliasnum_meshes = 1;
	mesh = loadmodel->aliasdata_meshes = Mem_Alloc(loadmodel->mempool, loadmodel->aliasnum_meshes * sizeof(aliasmesh_t));
	mesh->num_skins = 0;
	mesh->num_frames = 0;
	for (i = 0;i < loadmodel->numframes;i++)
		mesh->num_frames += loadmodel->animscenes[i].framecount;
	for (i = 0;i < loadmodel->numskins;i++)
		mesh->num_skins += loadmodel->skinscenes[i].framecount;
	mesh->num_triangles = numtris;
	mesh->num_vertices = numverts;
	mesh->data_skins = Mem_Alloc(loadmodel->mempool, mesh->num_skins * sizeof(aliasskin_t));
	mesh->data_element3i = elements;
	mesh->data_neighbor3i = Mem_Alloc(loadmodel->mempool, numtris * sizeof(int[3]));
	Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
	Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
	mesh->data_texcoord2f = texcoord2f;
	mesh->data_aliasvertex = posedata;
	for (i = 0;i < mesh->num_skins;i++)
		Mod_BuildAliasSkinFromSkinFrame(mesh->data_skins + i, loadmodel->skinframes + i);
	Mod_CalcAliasModelBBoxes();
}

#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%d exceeds %d - %d)\n", loadmodel->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%f exceeds %f - %f)\n", loadmodel->name, VALUE, MIN, MAX);
extern void R_Model_Alias_Draw(entity_render_t *ent);
extern void R_Model_Alias_DrawFakeShadow(entity_render_t *ent);
extern void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_LoadQ1AliasModel (model_t *mod, void *buffer)
{
	int i, j, version, totalposes, totalskins, skinwidth, skinheight, totalverts, groupframes, groupskins, *elements, numverts, numtris;
	float scales, scalet, scale[3], translate[3], interval, *texcoords;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasskintype_t *pinskintype;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;
	daliasframetype_t *pinframetype;
	daliasgroup_t *pinframegroup;
	aliasvertex_t *posedata;
	qbyte *datapointer, *startframes, *startskins;
	char name[MAX_QPATH];
	skinframe_t tempskinframe;
	animscene_t *tempskinscenes;
	skinframe_t *tempskinframes;
	float *vertst;
	int *vertonseam, *vertusage, *vertremap, *temptris;

	datapointer = buffer;
	pinmodel = (mdl_t *)datapointer;
	datapointer += sizeof(mdl_t);

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 loadmodel->name, version, ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_ALIAS;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawFakeShadow = R_Model_Alias_DrawFakeShadow;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;

	loadmodel->numskins = LittleLong(pinmodel->numskins);
	BOUNDI(loadmodel->numskins,0,256);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,4096);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,4096);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,65536);
	numtris = LittleLong(pinmodel->numtris);
	BOUNDI(numtris,0,65536 / 3); // max elements limit, rather than max triangles limit
	loadmodel->numframes = LittleLong(pinmodel->numframes);
	BOUNDI(loadmodel->numframes,0,65536);
	loadmodel->synctype = LittleLong (pinmodel->synctype);
	BOUNDI(loadmodel->synctype,0,2);
	loadmodel->flags = LittleLong (pinmodel->flags);

	for (i = 0;i < 3;i++)
	{
		scale[i] = LittleFloat (pinmodel->scale[i]);
		translate[i] = LittleFloat (pinmodel->scale_origin[i]);
	}

	startskins = datapointer;
	totalskins = 0;
	for (i = 0;i < loadmodel->numskins;i++)
	{
		pinskintype = (daliasskintype_t *)datapointer;
		datapointer += sizeof(daliasskintype_t);
		if (LittleLong(pinskintype->type) == ALIAS_SKIN_SINGLE)
			groupskins = 1;
		else
		{
			pinskingroup = (daliasskingroup_t *)datapointer;
			datapointer += sizeof(daliasskingroup_t);
			groupskins = LittleLong(pinskingroup->numskins);
			datapointer += sizeof(daliasskininterval_t) * groupskins;
		}

		for (j = 0;j < groupskins;j++)
		{
			datapointer += skinwidth * skinheight;
			totalskins++;
		}
	}

	pinstverts = (stvert_t *)datapointer;
	datapointer += sizeof(stvert_t) * numverts;

	pintriangles = (dtriangle_t *)datapointer;
	datapointer += sizeof(dtriangle_t) * numtris;

	startframes = datapointer;
	totalposes = 0;
	for (i = 0;i < loadmodel->numframes;i++)
	{
		pinframetype = (daliasframetype_t *)datapointer;
		datapointer += sizeof(daliasframetype_t);
		if (LittleLong (pinframetype->type) == ALIAS_SINGLE)
			groupframes = 1;
		else
		{
			pinframegroup = (daliasgroup_t *)datapointer;
			datapointer += sizeof(daliasgroup_t);
			groupframes = LittleLong(pinframegroup->numframes);
			datapointer += sizeof(daliasinterval_t) * groupframes;
		}

		for (j = 0;j < groupframes;j++)
		{
			datapointer += sizeof(daliasframe_t);
			datapointer += sizeof(trivertx_t) * numverts;
			totalposes++;
		}
	}

	// load the skins
	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numskins * sizeof(animscene_t));
	loadmodel->skinframes = Mem_Alloc(loadmodel->mempool, totalskins * sizeof(skinframe_t));
	totalskins = 0;
	datapointer = startskins;
	for (i = 0;i < loadmodel->numskins;i++)
	{
		pinskintype = (daliasskintype_t *)datapointer;
		datapointer += sizeof(daliasskintype_t);

		if (pinskintype->type == ALIAS_SKIN_SINGLE)
		{
			groupskins = 1;
			interval = 0.1f;
		}
		else
		{
			pinskingroup = (daliasskingroup_t *)datapointer;
			datapointer += sizeof(daliasskingroup_t);

			groupskins = LittleLong (pinskingroup->numskins);

			pinskinintervals = (daliasskininterval_t *)datapointer;
			datapointer += sizeof(daliasskininterval_t) * groupskins;

			interval = LittleFloat(pinskinintervals[0].interval);
			if (interval < 0.01f)
				Host_Error("Mod_LoadQ1AliasModel: invalid interval\n");
		}

		sprintf(loadmodel->skinscenes[i].name, "skin %i", i);
		loadmodel->skinscenes[i].firstframe = totalskins;
		loadmodel->skinscenes[i].framecount = groupskins;
		loadmodel->skinscenes[i].framerate = 1.0f / interval;
		loadmodel->skinscenes[i].loop = true;

		for (j = 0;j < groupskins;j++)
		{
			if (groupskins > 1)
				sprintf (name, "%s_%i_%i", loadmodel->name, i, j);
			else
				sprintf (name, "%s_%i", loadmodel->name, i);
			if (!Mod_LoadSkinFrame(loadmodel->skinframes + totalskins, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA, true, false, true))
				Mod_LoadSkinFrame_Internal(loadmodel->skinframes + totalskins, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA, true, false, true, (qbyte *)datapointer, skinwidth, skinheight);
			datapointer += skinwidth * skinheight;
			totalskins++;
		}
	}
	// check for skins that don't exist in the model, but do exist as external images
	// (this was added because yummyluv kept pestering me about support for it)
	for (;;)
	{
		sprintf (name, "%s_%i", loadmodel->name, loadmodel->numskins);
		if (Mod_LoadSkinFrame (&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA, true, false, true))
		{
			// expand the arrays to make room
			tempskinscenes = loadmodel->skinscenes;
			tempskinframes = loadmodel->skinframes;
			loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, (loadmodel->numskins + 1) * sizeof(animscene_t));
			loadmodel->skinframes = Mem_Alloc(loadmodel->mempool, (totalskins + 1) * sizeof(skinframe_t));
			memcpy(loadmodel->skinscenes, tempskinscenes, loadmodel->numskins * sizeof(animscene_t));
			memcpy(loadmodel->skinframes, tempskinframes, totalskins * sizeof(skinframe_t));
			Mem_Free(tempskinscenes);
			Mem_Free(tempskinframes);
			// store the info about the new skin
			strcpy(loadmodel->skinscenes[loadmodel->numskins].name, name);
			loadmodel->skinscenes[loadmodel->numskins].firstframe = totalskins;
			loadmodel->skinscenes[loadmodel->numskins].framecount = 1;
			loadmodel->skinscenes[loadmodel->numskins].framerate = 10.0f;
			loadmodel->skinscenes[loadmodel->numskins].loop = true;
			loadmodel->skinframes[totalskins] = tempskinframe;
			loadmodel->numskins++;
			totalskins++;
		}
		else
			break;
	}

	// store texture coordinates into temporary array, they will be stored after usage is determined (triangle data)
	vertst = Mem_Alloc(tempmempool, numverts * 2 * sizeof(float[2]));
	vertonseam = Mem_Alloc(tempmempool, numverts * sizeof(int));
	vertusage = Mem_Alloc(tempmempool, numverts * 2 * sizeof(int));
	vertremap = Mem_Alloc(tempmempool, numverts * 2 * sizeof(int));
	temptris = Mem_Alloc(tempmempool, numtris * sizeof(int[3]));

	scales = 1.0 / skinwidth;
	scalet = 1.0 / skinheight;
	for (i = 0;i < numverts;i++)
	{
		vertonseam[i] = LittleLong(pinstverts[i].onseam);
		vertst[i*2+0] = (LittleLong(pinstverts[i].s) + 0.5) * scales;
		vertst[i*2+1] = (LittleLong(pinstverts[i].t) + 0.5) * scalet;
		vertst[(i+numverts)*2+0] = vertst[i*2+0] + 0.5;
		vertst[(i+numverts)*2+1] = vertst[i*2+1];
		vertusage[i] = 0;
		vertusage[i+numverts] = 0;
	}

// load triangle data
	elements = Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * numtris);

	// count the vertices used
	for (i = 0;i < numverts*2;i++)
		vertusage[i] = 0;
	for (i = 0;i < numtris;i++)
	{
		temptris[i*3+0] = LittleLong(pintriangles[i].vertindex[0]);
		temptris[i*3+1] = LittleLong(pintriangles[i].vertindex[1]);
		temptris[i*3+2] = LittleLong(pintriangles[i].vertindex[2]);
		if (!LittleLong(pintriangles[i].facesfront)) // backface
		{
			if (vertonseam[temptris[i*3+0]]) temptris[i*3+0] += numverts;
			if (vertonseam[temptris[i*3+1]]) temptris[i*3+1] += numverts;
			if (vertonseam[temptris[i*3+2]]) temptris[i*3+2] += numverts;
		}
		vertusage[temptris[i*3+0]]++;
		vertusage[temptris[i*3+1]]++;
		vertusage[temptris[i*3+2]]++;
	}
	// build remapping table and compact array
	totalverts = 0;
	for (i = 0;i < numverts*2;i++)
	{
		if (vertusage[i])
		{
			vertremap[i] = totalverts;
			vertst[totalverts*2+0] = vertst[i*2+0];
			vertst[totalverts*2+1] = vertst[i*2+1];
			totalverts++;
		}
		else
			vertremap[i] = -1; // not used at all
	}
	// remap the triangle references
	for (i = 0;i < numtris * 3;i++)
		elements[i] = vertremap[temptris[i]];
	// store the texture coordinates
	texcoords = Mem_Alloc(loadmodel->mempool, sizeof(float[2]) * totalverts);
	for (i = 0;i < totalverts;i++)
	{
		texcoords[i*2+0] = vertst[i*2+0];
		texcoords[i*2+1] = vertst[i*2+1];
	}

// load the frames
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	posedata = Mem_Alloc(loadmodel->mempool, sizeof(aliasvertex_t) * totalposes * totalverts);
	Mod_MDL_LoadFrames (startframes, numverts, totalverts, scale, translate, texcoords, posedata, numtris, elements, vertremap);
	Mod_BuildMDLMD2MeshInfo(totalverts, numtris, elements, texcoords, posedata);

	Mem_Free(vertst);
	Mem_Free(vertonseam);
	Mem_Free(vertusage);
	Mem_Free(vertremap);
	Mem_Free(temptris);
}

static void Mod_MD2_ConvertVerts (vec3_t scale, vec3_t translate, trivertx_t *v, aliasvertex_t *out, int numverts, int *vertremap)
{
	int i;
	trivertx_t *in;
	for (i = 0;i < numverts;i++)
	{
		in = v + vertremap[i];
		out[i].origin[0] = in->v[0] * scale[0] + translate[0];
		out[i].origin[1] = in->v[1] * scale[1] + translate[1];
		out[i].origin[2] = in->v[2] * scale[2] + translate[2];
	}
}

void Mod_LoadQ2AliasModel (model_t *mod, void *buffer)
{
	int i, j, k, hashindex, num, numxyz, numst, xyz, st, skinwidth, skinheight, *vertremap, version, end, *elements, numverts, numtris;
	float *stverts, s, t, scale[3], translate[3], *vertexbuffer, *svectorsbuffer, *tvectorsbuffer, *normalsbuffer, *texcoords;
	aliasvertex_t *posedata;
	md2_t *pinmodel;
	qbyte *base, *datapointer;
	md2frame_t *pinframe;
	char *inskin;
	md2triangle_t *intri;
	unsigned short *inst;
	struct md2verthash_s
	{
		struct md2verthash_s *next;
		int xyz;
		float st[2];
	}
	*hash, **md2verthash, *md2verthashdata;

	pinmodel = buffer;
	base = buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD2ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_ALIAS;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawFakeShadow = R_Model_Alias_DrawFakeShadow;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;

	if (LittleLong(pinmodel->num_tris < 1) || LittleLong(pinmodel->num_tris) > (65536 / 3))
		Host_Error ("%s has invalid number of triangles: %i", loadmodel->name, LittleLong(pinmodel->num_tris));
	if (LittleLong(pinmodel->num_xyz < 1) || LittleLong(pinmodel->num_xyz) > 65536)
		Host_Error ("%s has invalid number of vertices: %i", loadmodel->name, LittleLong(pinmodel->num_xyz));
	if (LittleLong(pinmodel->num_frames < 1) || LittleLong(pinmodel->num_frames) > 65536)
		Host_Error ("%s has invalid number of frames: %i", loadmodel->name, LittleLong(pinmodel->num_frames));
	if (LittleLong(pinmodel->num_skins < 0) || LittleLong(pinmodel->num_skins) > 256)
		Host_Error ("%s has invalid number of skins: %i", loadmodel->name, LittleLong(pinmodel->num_skins));

	end = LittleLong(pinmodel->ofs_end);
	if (LittleLong(pinmodel->num_skins) >= 1 && (LittleLong(pinmodel->ofs_skins <= 0) || LittleLong(pinmodel->ofs_skins) >= end))
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_st <= 0) || LittleLong(pinmodel->ofs_st) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_tris <= 0) || LittleLong(pinmodel->ofs_tris) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_frames <= 0) || LittleLong(pinmodel->ofs_frames) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_glcmds <= 0) || LittleLong(pinmodel->ofs_glcmds) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);

	loadmodel->numskins = LittleLong(pinmodel->num_skins);
	numxyz = LittleLong(pinmodel->num_xyz);
	numst = LittleLong(pinmodel->num_st);
	numtris = LittleLong(pinmodel->num_tris);
	loadmodel->numframes = LittleLong(pinmodel->num_frames);

	loadmodel->flags = 0; // there are no MD2 flags
	loadmodel->synctype = ST_RAND;

	// load the skins
	inskin = (void*)(base + LittleLong(pinmodel->ofs_skins));
	if (loadmodel->numskins)
	{
		// skins found (most likely not a player model)
		loadmodel->skinframes = Mem_Alloc(loadmodel->mempool, sizeof(skinframe_t) * loadmodel->numskins);
		for (i = 0;i < loadmodel->numskins;i++, inskin += MD2_SKINNAME)
			Mod_LoadSkinFrame (loadmodel->skinframes + i, inskin, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE, true, false, true);
	}
	else
	{
		// no skins (most likely a player model)
		loadmodel->numskins = 1;
		loadmodel->skinframes = Mem_Alloc(loadmodel->mempool, sizeof(skinframe_t) * loadmodel->numskins);
	}

	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

	// load the triangles and stvert data
	inst = (void*)(base + LittleLong(pinmodel->ofs_st));
	intri = (void*)(base + LittleLong(pinmodel->ofs_tris));
	skinwidth = LittleLong(pinmodel->skinwidth);
	skinheight = LittleLong(pinmodel->skinheight);

	stverts = Mem_Alloc(tempmempool, numst * sizeof(float[2]));
	s = 1.0f / skinwidth;
	t = 1.0f / skinheight;
	for (i = 0;i < numst;i++)
	{
		j = (unsigned short) LittleShort(inst[i*2+0]);
		k = (unsigned short) LittleShort(inst[i*2+1]);
		if (j >= skinwidth || k >= skinheight)
		{
			Mem_Free(stverts);
			Host_Error("Mod_MD2_LoadGeometry: invalid skin coordinate (%i %i) on vert %i of model %s\n", j, k, i, loadmodel->name);
		}
		stverts[i*2+0] = j * s;
		stverts[i*2+1] = k * t;
	}

	md2verthash = Mem_Alloc(tempmempool, 256 * sizeof(hash));
	md2verthashdata = Mem_Alloc(tempmempool, numtris * 3 * sizeof(*hash));
	// swap the triangle list
	num = 0;
	elements = Mem_Alloc(loadmodel->mempool, numtris * sizeof(int[3]));
	for (i = 0;i < numtris;i++)
	{
		for (j = 0;j < 3;j++)
		{
			xyz = (unsigned short) LittleShort (intri[i].index_xyz[j]);
			st = (unsigned short) LittleShort (intri[i].index_st[j]);
			if (xyz >= numxyz || st >= numst)
			{
				Mem_Free(md2verthash);
				Mem_Free(md2verthashdata);
				Mem_Free(stverts);
				if (xyz >= numxyz)
					Host_Error("Mod_MD2_LoadGeometry: invalid xyz index (%i) on triangle %i of model %s\n", xyz, i, loadmodel->name);
				if (st >= numst)
					Host_Error("Mod_MD2_LoadGeometry: invalid st index (%i) on triangle %i of model %s\n", st, i, loadmodel->name);
			}
			s = stverts[st*2+0];
			t = stverts[st*2+1];
			hashindex = (xyz * 17 + st) & 255;
			for (hash = md2verthash[hashindex];hash;hash = hash->next)
				if (hash->xyz == xyz && hash->st[0] == s && hash->st[1] == t)
					break;
			if (hash == NULL)
			{
				hash = md2verthashdata + num++;
				hash->xyz = xyz;
				hash->st[0] = s;
				hash->st[1] = t;
				hash->next = md2verthash[hashindex];
				md2verthash[hashindex] = hash;
			}
			elements[i*3+j] = (hash - md2verthashdata);
		}
	}

	Mem_Free(stverts);

	numverts = num;
	vertremap = Mem_Alloc(loadmodel->mempool, num * sizeof(int));
	texcoords = Mem_Alloc(loadmodel->mempool, num * sizeof(float[2]));
	for (i = 0;i < num;i++)
	{
		hash = md2verthashdata + i;
		vertremap[i] = hash->xyz;
		texcoords[i*2+0] = hash->st[0];
		texcoords[i*2+1] = hash->st[1];
	}

	Mem_Free(md2verthash);
	Mem_Free(md2verthashdata);

	// load the frames
	datapointer = (base + LittleLong(pinmodel->ofs_frames));
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));
	posedata = Mem_Alloc(loadmodel->mempool, numverts * loadmodel->numframes * sizeof(aliasvertex_t));

	vertexbuffer = Mem_Alloc(tempmempool, numverts * sizeof(float[3+3+3+3]));
	svectorsbuffer = vertexbuffer + numverts * 3;
	tvectorsbuffer = svectorsbuffer + numverts * 3;
	normalsbuffer = tvectorsbuffer + numverts * 3;
	for (i = 0;i < loadmodel->numframes;i++)
	{
		pinframe = (md2frame_t *)datapointer;
		datapointer += sizeof(md2frame_t);
		for (j = 0;j < 3;j++)
		{
			scale[j] = LittleFloat(pinframe->scale[j]);
			translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_MD2_ConvertVerts(scale, translate, (void *)datapointer, posedata + i * numverts, numverts, vertremap);
		Mod_BuildAliasVertexTextureVectors(numverts, posedata + i * numverts, texcoords, vertexbuffer, svectorsbuffer, tvectorsbuffer, normalsbuffer, numtris, elements);
		datapointer += numxyz * sizeof(trivertx_t);

		strcpy(loadmodel->animscenes[i].name, pinframe->name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}
	Mem_Free(vertexbuffer);

	Mem_Free(vertremap);

	Mod_BuildMDLMD2MeshInfo(numverts, numtris, elements, texcoords, posedata);
}

void Mod_LoadQ3AliasModel(model_t *mod, void *buffer)
{
	int i, j, version;
	float *vertexbuffer, *svectorsbuffer, *tvectorsbuffer, *normalsbuffer;
	md3modelheader_t *pinmodel;
	md3frameinfo_t *pinframe;
	md3mesh_t *pinmesh;
	aliasmesh_t *mesh;
	skinframe_t tempskinframe;

	pinmodel = buffer;

	if (memcmp(pinmodel->identifier, "IDP3", 4))
		Host_Error ("%s is not a MD3 (IDP3) file\n", loadmodel->name);
	version = LittleLong (pinmodel->version);
	if (version != MD3VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD3VERSION);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_ALIAS;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawFakeShadow = R_Model_Alias_DrawFakeShadow;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	loadmodel->flags = 0;
	loadmodel->synctype = ST_RAND;

	// set up some global info about the model
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->numskins = 1;
	loadmodel->aliasnum_meshes = LittleLong(pinmodel->num_meshes);
	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numskins * sizeof(animscene_t));
	loadmodel->skinscenes[0].firstframe = 0;
	loadmodel->skinscenes[0].framecount = 1;
	loadmodel->skinscenes[0].loop = true;
	loadmodel->skinscenes[0].framerate = 10;

	// load frameinfo
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));
	for (i = 0, pinframe = (md3frameinfo_t *)((qbyte *)pinmodel + LittleLong(pinmodel->lump_frameinfo));i < loadmodel->numframes;i++, pinframe++)
	{
		strcpy(loadmodel->animscenes[i].name, pinframe->name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}

	// tags are not loaded yet

	// load meshes
	loadmodel->aliasdata_meshes = Mem_Alloc(loadmodel->mempool, loadmodel->aliasnum_meshes * sizeof(aliasmesh_t));
	for (i = 0, pinmesh = (md3mesh_t *)((qbyte *)pinmodel + LittleLong(pinmodel->lump_meshes));i < loadmodel->aliasnum_meshes;i++, pinmesh = (md3mesh_t *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_end)))
	{
		if (memcmp(pinmesh->identifier, "IDP3", 4))
			Host_Error("Mod_LoadQ3AliasModel: invalid mesh identifier (not IDP3)\n");
		mesh = loadmodel->aliasdata_meshes + i;
		mesh->num_skins = loadmodel->numskins;
		mesh->num_frames = LittleLong(pinmesh->num_frames);
		mesh->num_vertices = LittleLong(pinmesh->num_vertices);
		mesh->num_triangles = LittleLong(pinmesh->num_triangles);
		mesh->data_skins = Mem_Alloc(loadmodel->mempool, mesh->num_skins * sizeof(aliasskin_t));
		mesh->data_element3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_texcoord2f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		mesh->data_aliasvertex = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * mesh->num_frames * sizeof(aliasvertex_t));
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = LittleLong(((int *)((qbyte *)pinmesh + pinmesh->lump_elements))[j]);
		for (j = 0;j < mesh->num_vertices;j++)
		{
			mesh->data_texcoord2f[j * 2 + 0] = LittleLong(((float *)((qbyte *)pinmesh + pinmesh->lump_texcoords))[j * 2 + 0]);
			mesh->data_texcoord2f[j * 2 + 1] = LittleLong(((float *)((qbyte *)pinmesh + pinmesh->lump_texcoords))[j * 2 + 1]);
		}
		for (j = 0;j < mesh->num_vertices * mesh->num_frames;j++)
		{
			mesh->data_aliasvertex[j].origin[0] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 0]) * (1.0f / 64.0f);
			mesh->data_aliasvertex[j].origin[1] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 1]) * (1.0f / 64.0f);
			mesh->data_aliasvertex[j].origin[2] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 2]) * (1.0f / 64.0f);
		}
		vertexbuffer = Mem_Alloc(tempmempool, mesh->num_vertices * sizeof(float[3+3+3+3]));
		svectorsbuffer = vertexbuffer + mesh->num_vertices * 3;
		tvectorsbuffer = svectorsbuffer + mesh->num_vertices * 3;
		normalsbuffer = tvectorsbuffer + mesh->num_vertices * 3;
		for (j = 0;j < mesh->num_frames;j++)
			Mod_BuildAliasVertexTextureVectors(mesh->num_vertices, mesh->data_aliasvertex + j * mesh->num_vertices, mesh->data_texcoord2f, vertexbuffer, svectorsbuffer, tvectorsbuffer, normalsbuffer, mesh->num_triangles, mesh->data_element3i);
		Mem_Free(vertexbuffer);

		memset(&tempskinframe, 0, sizeof(tempskinframe));
		if (LittleLong(pinmesh->num_shaders) >= 1 && ((md3shader_t *)((qbyte *) pinmesh + pinmesh->lump_shaders))->name[0])
			Mod_LoadSkinFrame (&tempskinframe, ((md3shader_t *)((qbyte *) pinmesh + pinmesh->lump_shaders))->name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE, true, false, true);
		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_BuildAliasSkinFromSkinFrame(mesh->data_skins, &tempskinframe);
	}
	Mod_CalcAliasModelBBoxes();
}

extern void R_Model_Zymotic_DrawSky(entity_render_t *ent);
extern void R_Model_Zymotic_Draw(entity_render_t *ent);
extern void R_Model_Zymotic_DrawFakeShadow(entity_render_t *ent);
extern void R_Model_Zymotic_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Zymotic_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_LoadZymoticModel(model_t *mod, void *buffer)
{
	zymtype1header_t *pinmodel, *pheader;
	qbyte *pbase;

	pinmodel = (void *)buffer;
	pbase = buffer;
	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_LoadZymoticModel: %s is not a zymotic model\n");
	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_LoadZymoticModel: only type 1 (skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_ZYM;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Zymotic_Draw;
	loadmodel->DrawFakeShadow = NULL;//R_Model_Zymotic_DrawFakeShadow;
	loadmodel->DrawShadowVolume = NULL;//R_Model_Zymotic_DrawShadowVolume;
	loadmodel->DrawLight = NULL;//R_Model_Zymotic_DrawLight;

	// byteswap header
	pheader = pinmodel;
	pheader->type = BigLong(pinmodel->type);
	pheader->filesize = BigLong(pinmodel->filesize);
	pheader->mins[0] = BigFloat(pinmodel->mins[0]);
	pheader->mins[1] = BigFloat(pinmodel->mins[1]);
	pheader->mins[2] = BigFloat(pinmodel->mins[2]);
	pheader->maxs[0] = BigFloat(pinmodel->maxs[0]);
	pheader->maxs[1] = BigFloat(pinmodel->maxs[1]);
	pheader->maxs[2] = BigFloat(pinmodel->maxs[2]);
	pheader->radius = BigFloat(pinmodel->radius);
	pheader->numverts = loadmodel->zymnum_verts = BigLong(pinmodel->numverts);
	pheader->numtris = loadmodel->zymnum_tris = BigLong(pinmodel->numtris);
	pheader->numshaders = loadmodel->zymnum_shaders = BigLong(pinmodel->numshaders);
	pheader->numbones = loadmodel->zymnum_bones = BigLong(pinmodel->numbones);
	pheader->numscenes = loadmodel->zymnum_scenes = BigLong(pinmodel->numscenes);
	pheader->lump_scenes.start = BigLong(pinmodel->lump_scenes.start);
	pheader->lump_scenes.length = BigLong(pinmodel->lump_scenes.length);
	pheader->lump_poses.start = BigLong(pinmodel->lump_poses.start);
	pheader->lump_poses.length = BigLong(pinmodel->lump_poses.length);
	pheader->lump_bones.start = BigLong(pinmodel->lump_bones.start);
	pheader->lump_bones.length = BigLong(pinmodel->lump_bones.length);
	pheader->lump_vertbonecounts.start = BigLong(pinmodel->lump_vertbonecounts.start);
	pheader->lump_vertbonecounts.length = BigLong(pinmodel->lump_vertbonecounts.length);
	pheader->lump_verts.start = BigLong(pinmodel->lump_verts.start);
	pheader->lump_verts.length = BigLong(pinmodel->lump_verts.length);
	pheader->lump_texcoords.start = BigLong(pinmodel->lump_texcoords.start);
	pheader->lump_texcoords.length = BigLong(pinmodel->lump_texcoords.length);
	pheader->lump_render.start = BigLong(pinmodel->lump_render.start);
	pheader->lump_render.length = BigLong(pinmodel->lump_render.length);
	pheader->lump_shaders.start = BigLong(pinmodel->lump_shaders.start);
	pheader->lump_shaders.length = BigLong(pinmodel->lump_shaders.length);
	pheader->lump_trizone.start = BigLong(pinmodel->lump_trizone.start);
	pheader->lump_trizone.length = BigLong(pinmodel->lump_trizone.length);

	loadmodel->flags = 0; // there are no flags
	loadmodel->numframes = pheader->numscenes;
	loadmodel->synctype = ST_SYNC;
	//loadmodel->numtris = pheader->numtris;
	//loadmodel->numverts = 0;

	{
		unsigned int i;
		float modelradius, corner[2];
		// model bbox
		modelradius = pheader->radius;
		for (i = 0;i < 3;i++)
		{
			loadmodel->normalmins[i] = pheader->mins[i];
			loadmodel->normalmaxs[i] = pheader->maxs[i];
			loadmodel->rotatedmins[i] = -modelradius;
			loadmodel->rotatedmaxs[i] = modelradius;
		}
		corner[0] = max(fabs(loadmodel->normalmins[0]), fabs(loadmodel->normalmaxs[0]));
		corner[1] = max(fabs(loadmodel->normalmins[1]), fabs(loadmodel->normalmaxs[1]));
		loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] = sqrt(corner[0]*corner[0]+corner[1]*corner[1]);
		if (loadmodel->yawmaxs[0] > modelradius)
			loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] = modelradius;
		loadmodel->yawmins[0] = loadmodel->yawmins[1] = -loadmodel->yawmaxs[0];
		loadmodel->yawmins[2] = loadmodel->normalmins[2];
		loadmodel->yawmaxs[2] = loadmodel->normalmaxs[2];
		loadmodel->radius = modelradius;
		loadmodel->radius2 = modelradius * modelradius;
	}

	{
		// FIXME: add shaders, and make them switchable shader sets and...
		loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) + sizeof(skinframe_t));
		loadmodel->skinscenes[0].firstframe = 0;
		loadmodel->skinscenes[0].framecount = 1;
		loadmodel->skinscenes[0].loop = true;
		loadmodel->skinscenes[0].framerate = 10;
		loadmodel->skinframes = (void *)(loadmodel->skinscenes + 1);
		loadmodel->numskins = 1;
	}

	// go through the lumps, swapping things

	{
		int i, numposes;
		zymscene_t *scene;
	//	zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
		loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
		scene = (void *) (pheader->lump_scenes.start + pbase);
		numposes = pheader->lump_poses.length / pheader->numbones / sizeof(float[3][4]);
		for (i = 0;i < pheader->numscenes;i++)
		{
			memcpy(loadmodel->animscenes[i].name, scene->name, 32);
			loadmodel->animscenes[i].firstframe = BigLong(scene->start);
			loadmodel->animscenes[i].framecount = BigLong(scene->length);
			loadmodel->animscenes[i].framerate = BigFloat(scene->framerate);
			loadmodel->animscenes[i].loop = (BigLong(scene->flags) & ZYMSCENEFLAG_NOLOOP) == 0;
			if ((unsigned int) loadmodel->animscenes[i].firstframe >= (unsigned int) numposes)
				Host_Error("Mod_LoadZymoticModel: scene firstframe (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, numposes);
			if ((unsigned int) loadmodel->animscenes[i].firstframe + (unsigned int) loadmodel->animscenes[i].framecount > (unsigned int) numposes)
				Host_Error("Mod_LoadZymoticModel: scene firstframe (%i) + framecount (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, loadmodel->animscenes[i].framecount, numposes);
			if (loadmodel->animscenes[i].framerate < 0)
				Host_Error("Mod_LoadZymoticModel: scene framerate (%f) < 0\n", loadmodel->animscenes[i].framerate);
			scene++;
		}
	}

	{
		int i;
		float *poses;
	//	zymlump_t lump_poses; // float pose[numposes][numbones][3][4]; // animation data
		loadmodel->zymdata_poses = Mem_Alloc(loadmodel->mempool, pheader->lump_poses.length);
		poses = (void *) (pheader->lump_poses.start + pbase);
		for (i = 0;i < pheader->lump_poses.length / 4;i++)
			loadmodel->zymdata_poses[i] = BigFloat(poses[i]);
	}

	{
		int i;
		zymbone_t *bone;
	//	zymlump_t lump_bones; // zymbone_t bone[numbones];
		loadmodel->zymdata_bones = Mem_Alloc(loadmodel->mempool, pheader->numbones * sizeof(zymbone_t));
		bone = (void *) (pheader->lump_bones.start + pbase);
		for (i = 0;i < pheader->numbones;i++)
		{
			memcpy(loadmodel->zymdata_bones[i].name, bone[i].name, sizeof(bone[i].name));
			loadmodel->zymdata_bones[i].flags = BigLong(bone[i].flags);
			loadmodel->zymdata_bones[i].parent = BigLong(bone[i].parent);
			if (loadmodel->zymdata_bones[i].parent >= i)
				Host_Error("Mod_LoadZymoticModel: bone[%i].parent >= %i in %s\n", i, i, loadmodel->name);
		}
	}

	{
		int i, *bonecount;
	//	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
		loadmodel->zymdata_vertbonecounts = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
		bonecount = (void *) (pheader->lump_vertbonecounts.start + pbase);
		for (i = 0;i < pheader->numverts;i++)
		{
			loadmodel->zymdata_vertbonecounts[i] = BigLong(bonecount[i]);
			if (loadmodel->zymdata_vertbonecounts[i] < 1)
				Host_Error("Mod_LoadZymoticModel: bone vertex count < 1 in %s\n", loadmodel->name);
		}
	}

	{
		int i;
		zymvertex_t *vertdata;
	//	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
		loadmodel->zymdata_verts = Mem_Alloc(loadmodel->mempool, pheader->lump_verts.length);
		vertdata = (void *) (pheader->lump_verts.start + pbase);
		for (i = 0;i < pheader->lump_verts.length / (int) sizeof(zymvertex_t);i++)
		{
			loadmodel->zymdata_verts[i].bonenum = BigLong(vertdata[i].bonenum);
			loadmodel->zymdata_verts[i].origin[0] = BigFloat(vertdata[i].origin[0]);
			loadmodel->zymdata_verts[i].origin[1] = BigFloat(vertdata[i].origin[1]);
			loadmodel->zymdata_verts[i].origin[2] = BigFloat(vertdata[i].origin[2]);
		}
	}

	{
		int i;
		float *intexcoord2f, *outtexcoord2f;
	//	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
		loadmodel->zymdata_texcoords = outtexcoord2f = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(float[2]));
		intexcoord2f = (void *) (pheader->lump_texcoords.start + pbase);
		for (i = 0;i < pheader->numverts;i++)
		{
			outtexcoord2f[i*2+0] = BigFloat(intexcoord2f[i*2+0]);
			// flip T coordinate for OpenGL
			outtexcoord2f[i*2+1] = 1 - BigFloat(intexcoord2f[i*2+1]);
		}
	}

	{
		int i, count, *renderlist, *renderlistend, *outrenderlist;
	//	zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
		loadmodel->zymdata_renderlist = Mem_Alloc(loadmodel->mempool, pheader->lump_render.length);
		// byteswap, validate, and swap winding order of tris
		count = pheader->numshaders * sizeof(int) + pheader->numtris * sizeof(int[3]);
		if (pheader->lump_render.length != count)
			Host_Error("Mod_LoadZymoticModel: renderlist is wrong size in %s (is %i bytes, should be %i bytes)\n", loadmodel->name, pheader->lump_render.length, count);
		outrenderlist = loadmodel->zymdata_renderlist = Mem_Alloc(loadmodel->mempool, count);
		renderlist = (void *) (pheader->lump_render.start + pbase);
		renderlistend = (void *) ((qbyte *) renderlist + pheader->lump_render.length);
		for (i = 0;i < pheader->numshaders;i++)
		{
			if (renderlist >= renderlistend)
				Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
			count = BigLong(*renderlist);renderlist++;
			if (renderlist + count * 3 > renderlistend)
				Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
			*outrenderlist++ = count;
			while (count--)
			{
				outrenderlist[2] = BigLong(renderlist[0]);
				outrenderlist[1] = BigLong(renderlist[1]);
				outrenderlist[0] = BigLong(renderlist[2]);
				if ((unsigned int)outrenderlist[0] >= (unsigned int)pheader->numverts
				 || (unsigned int)outrenderlist[1] >= (unsigned int)pheader->numverts
				 || (unsigned int)outrenderlist[2] >= (unsigned int)pheader->numverts)
					Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (out of bounds index)\n", loadmodel->name);
				renderlist += 3;
				outrenderlist += 3;
			}
		}
	}

	{
		int i;
		char *shadername;
	//	zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
		loadmodel->zymdata_textures = Mem_Alloc(loadmodel->mempool, pheader->numshaders * sizeof(rtexture_t *));
		shadername = (void *) (pheader->lump_shaders.start + pbase);
		for (i = 0;i < pheader->numshaders;i++)
			loadmodel->zymdata_textures[i] = loadtextureimage(loadmodel->texturepool, shadername + i * 32, 0, 0, true, TEXF_ALPHA | TEXF_PRECACHE | (r_mipskins.integer ? TEXF_MIPMAP : 0));
	}

	{
	//	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
		loadmodel->zymdata_trizone = Mem_Alloc(loadmodel->mempool, pheader->numtris);
		memcpy(loadmodel->zymdata_trizone, (void *) (pheader->lump_trizone.start + pbase), pheader->numtris);
	}
}
