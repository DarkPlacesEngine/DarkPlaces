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

void Mod_AliasInit (void)
{
}

static void Mod_CalcAliasModelBBoxes (void)
{
	int vnum, meshnum;
	float dist, yawradius, radius;
	aliasmesh_t *mesh;
	float *v;
	VectorClear(loadmodel->normalmins);
	VectorClear(loadmodel->normalmaxs);
	yawradius = 0;
	radius = 0;
	for (meshnum = 0, mesh = loadmodel->alias.aliasdata_meshes;meshnum < loadmodel->alias.aliasnum_meshes;meshnum++, mesh++)
	{
		for (vnum = 0, v = mesh->data_aliasvertex3f;vnum < mesh->num_vertices * mesh->num_frames;vnum++, v += 3)
		{
			if (loadmodel->normalmins[0] > v[0]) loadmodel->normalmins[0] = v[0];
			if (loadmodel->normalmins[1] > v[1]) loadmodel->normalmins[1] = v[1];
			if (loadmodel->normalmins[2] > v[2]) loadmodel->normalmins[2] = v[2];
			if (loadmodel->normalmaxs[0] < v[0]) loadmodel->normalmaxs[0] = v[0];
			if (loadmodel->normalmaxs[1] < v[1]) loadmodel->normalmaxs[1] = v[1];
			if (loadmodel->normalmaxs[2] < v[2]) loadmodel->normalmaxs[2] = v[2];
			dist = v[0] * v[0] + v[1] * v[1];
			if (yawradius < dist)
				yawradius = dist;
			dist += v[2] * v[2];
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

static void Mod_ConvertAliasVerts (int inverts, vec3_t scale, vec3_t translate, trivertx_t *v, float *out3f, int *vertremap)
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
			VectorCopy(temp, out3f + j * 3);
		j = vertremap[i+inverts]; // onseam
		if (j >= 0)
			VectorCopy(temp, out3f + j * 3);
	}
}

static void Mod_MDL_LoadFrames (qbyte* datapointer, int inverts, vec3_t scale, vec3_t translate, int *vertremap)
{
	int i, f, pose, groupframes;
	float interval;
	daliasframetype_t *pframetype;
	daliasframe_t *pinframe;
	daliasgroup_t *group;
	daliasinterval_t *intervals;
	animscene_t *scene;
	pose = 0;
	scene = loadmodel->animscenes;
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
			Mod_ConvertAliasVerts(inverts, scale, translate, (trivertx_t *)datapointer, loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3, vertremap);
			Mod_BuildTextureVectorsAndNormals(loadmodel->alias.aliasdata_meshes->num_vertices, loadmodel->alias.aliasdata_meshes->num_triangles, loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_texcoord2f, loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->data_aliassvector3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_aliastvector3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_aliasnormal3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3);
			datapointer += sizeof(trivertx_t) * inverts;
			pose++;
		}
	}
}

static skinframe_t missingskinframe;
aliaslayer_t mod_alias_layersbuffer[16]; // 7 currently used
void Mod_BuildAliasSkinFromSkinFrame(aliasskin_t *skin, skinframe_t *skinframe)
{
	aliaslayer_t *layer;

	// hack
	if (skinframe == NULL)
	{
		skinframe = &missingskinframe;
		memset(skinframe, 0, sizeof(*skinframe));
		skinframe->base = r_notexture;
	}

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
	}

	layer->flags = ALIASLAYER_FOG | ALIASLAYER_FORCEDRAW_IF_FIRSTPASS;
	layer->texture = skinframe->fog;
	layer++;

	skin->flags = 0;
	// fog texture only exists if some pixels are transparent...
	if (skinframe->fog != NULL)
		skin->flags |= ALIASSKIN_TRANSPARENT;

	skin->num_layers = layer - mod_alias_layersbuffer;
	skin->data_layers = Mem_Alloc(loadmodel->mempool, skin->num_layers * sizeof(aliaslayer_t));
	memcpy(skin->data_layers, mod_alias_layersbuffer, skin->num_layers * sizeof(aliaslayer_t));
}

void Mod_BuildAliasSkinsFromSkinFiles(aliasskin_t *skin, skinfile_t *skinfile, char *meshname, char *shadername)
{
	int i;
	skinfileitem_t *skinfileitem;
	skinframe_t tempskinframe;
	if (skinfile)
	{
		for (i = 0;skinfile;skinfile = skinfile->next, i++, skin++)
		{
			memset(skin, 0, sizeof(*skin));
			for (skinfileitem = skinfile->items;skinfileitem;skinfileitem = skinfileitem->next)
			{
				// leave the skin unitialized (nodraw) if the replacement is "common/nodraw" or "textures/common/nodraw"
				if (!strcmp(skinfileitem->name, meshname) && strcmp(skinfileitem->replacement, "common/nodraw") && strcmp(skinfileitem->replacement, "textures/common/nodraw"))
				{
					memset(&tempskinframe, 0, sizeof(tempskinframe));
					if (Mod_LoadSkinFrame(&tempskinframe, skinfileitem->replacement, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE, true, false, true))
						Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
					else
					{
						Con_Printf("mesh \"%s\": failed to load skin #%i \"%s\", falling back to mesh's internal shader name \"%s\"\n", meshname, i, skinfileitem->replacement, shadername);
						if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE, true, false, true))
							Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
						else
						{
							Con_Printf("failed to load skin \"%s\"\n", shadername);
							Mod_BuildAliasSkinFromSkinFrame(skin, NULL);
						}
					}
				}
			}
		}
	}
	else
	{
		memset(&tempskinframe, 0, sizeof(tempskinframe));
		if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE, true, false, true))
			Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
		else
		{
			Con_Printf("failed to load mesh \"%s\" shader \"%s\"\n", meshname, shadername);
			Mod_BuildAliasSkinFromSkinFrame(skin, NULL);
		}
	}
}

#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%d exceeds %d - %d)\n", loadmodel->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%f exceeds %f - %f)\n", loadmodel->name, VALUE, MIN, MAX);
extern void R_Model_Alias_Draw(entity_render_t *ent);
extern void R_Model_Alias_DrawFakeShadow(entity_render_t *ent);
extern void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_IDP0_Load(model_t *mod, void *buffer)
{
	int i, j, version, totalskins, skinwidth, skinheight, groupframes, groupskins, numverts;
	float scales, scalet, scale[3], translate[3], interval;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasskintype_t *pinskintype;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;
	daliasframetype_t *pinframetype;
	daliasgroup_t *pinframegroup;
	qbyte *datapointer, *startframes, *startskins;
	char name[MAX_QPATH];
	skinframe_t tempskinframe;
	animscene_t *tempskinscenes;
	aliasskin_t *tempaliasskins;
	float *vertst;
	int *vertonseam, *vertremap;
	skinfile_t *skinfiles;

	datapointer = buffer;
	pinmodel = (mdl_t *)datapointer;
	datapointer += sizeof(mdl_t);

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 loadmodel->name, version, ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->alias.aliastype = ALIASTYPE_ALIAS;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawFakeShadow = R_Model_Alias_DrawFakeShadow;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;

	loadmodel->alias.aliasnum_meshes = 1;
	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, sizeof(aliasmesh_t));

	loadmodel->numskins = LittleLong(pinmodel->numskins);
	BOUNDI(loadmodel->numskins,0,256);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,4096);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,4096);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,65536);
	loadmodel->alias.aliasdata_meshes->num_triangles = LittleLong(pinmodel->numtris);
	BOUNDI(loadmodel->alias.aliasdata_meshes->num_triangles,0,65536 / 3); // max elements limit, rather than max triangles limit
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
	datapointer += sizeof(dtriangle_t) * loadmodel->alias.aliasdata_meshes->num_triangles;

	startframes = datapointer;
	loadmodel->alias.aliasdata_meshes->num_frames = 0;
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
			loadmodel->alias.aliasdata_meshes->num_frames++;
		}
	}

	// store texture coordinates into temporary array, they will be stored
	// after usage is determined (triangle data)
	vertst = Mem_Alloc(tempmempool, numverts * 2 * sizeof(float[2]));
	vertremap = Mem_Alloc(tempmempool, numverts * 3 * sizeof(int));
	vertonseam = vertremap + numverts * 2;

	scales = 1.0 / skinwidth;
	scalet = 1.0 / skinheight;
	for (i = 0;i < numverts;i++)
	{
		vertonseam[i] = LittleLong(pinstverts[i].onseam);
		vertst[i*2+0] = (LittleLong(pinstverts[i].s) + 0.5) * scales;
		vertst[i*2+1] = (LittleLong(pinstverts[i].t) + 0.5) * scalet;
		vertst[(i+numverts)*2+0] = vertst[i*2+0] + 0.5;
		vertst[(i+numverts)*2+1] = vertst[i*2+1];
	}

// load triangle data
	loadmodel->alias.aliasdata_meshes->data_element3i = Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * loadmodel->alias.aliasdata_meshes->num_triangles);

	// read the triangle elements
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_triangles;i++)
		for (j = 0;j < 3;j++)
			loadmodel->alias.aliasdata_meshes->data_element3i[i*3+j] = LittleLong(pintriangles[i].vertindex[j]);
	// validate (note numverts is used because this is the original data)
	Mod_ValidateElements(loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->num_triangles, numverts, __FILE__, __LINE__);
	// now butcher the elements according to vertonseam and tri->facesfront
	// and then compact the vertex set to remove duplicates
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_triangles;i++)
		if (!LittleLong(pintriangles[i].facesfront)) // backface
			for (j = 0;j < 3;j++)
				if (vertonseam[loadmodel->alias.aliasdata_meshes->data_element3i[i*3+j]])
					loadmodel->alias.aliasdata_meshes->data_element3i[i*3+j] += numverts;
	// count the usage
	// (this uses vertremap to count usage to save some memory)
	for (i = 0;i < numverts*2;i++)
		vertremap[i] = 0;
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_triangles*3;i++)
		vertremap[loadmodel->alias.aliasdata_meshes->data_element3i[i]]++;
	// build remapping table and compact array
	loadmodel->alias.aliasdata_meshes->num_vertices = 0;
	for (i = 0;i < numverts*2;i++)
	{
		if (vertremap[i])
		{
			vertremap[i] = loadmodel->alias.aliasdata_meshes->num_vertices;
			vertst[loadmodel->alias.aliasdata_meshes->num_vertices*2+0] = vertst[i*2+0];
			vertst[loadmodel->alias.aliasdata_meshes->num_vertices*2+1] = vertst[i*2+1];
			loadmodel->alias.aliasdata_meshes->num_vertices++;
		}
		else
			vertremap[i] = -1; // not used at all
	}
	// remap the elements to the new vertex set
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_triangles * 3;i++)
		loadmodel->alias.aliasdata_meshes->data_element3i[i] = vertremap[loadmodel->alias.aliasdata_meshes->data_element3i[i]];
	// store the texture coordinates
	loadmodel->alias.aliasdata_meshes->data_texcoord2f = Mem_Alloc(loadmodel->mempool, sizeof(float[2]) * loadmodel->alias.aliasdata_meshes->num_vertices);
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_vertices;i++)
	{
		loadmodel->alias.aliasdata_meshes->data_texcoord2f[i*2+0] = vertst[i*2+0];
		loadmodel->alias.aliasdata_meshes->data_texcoord2f[i*2+1] = vertst[i*2+1];
	}

// load the frames
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->alias.aliasdata_meshes->data_aliasvertex3f = Mem_Alloc(loadmodel->mempool, sizeof(float[4][3]) * loadmodel->alias.aliasdata_meshes->num_frames * loadmodel->alias.aliasdata_meshes->num_vertices);
	loadmodel->alias.aliasdata_meshes->data_aliassvector3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + loadmodel->alias.aliasdata_meshes->num_frames * loadmodel->alias.aliasdata_meshes->num_vertices * 3 * 1;
	loadmodel->alias.aliasdata_meshes->data_aliastvector3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + loadmodel->alias.aliasdata_meshes->num_frames * loadmodel->alias.aliasdata_meshes->num_vertices * 3 * 2;
	loadmodel->alias.aliasdata_meshes->data_aliasnormal3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + loadmodel->alias.aliasdata_meshes->num_frames * loadmodel->alias.aliasdata_meshes->num_vertices * 3 * 3;
	Mod_MDL_LoadFrames (startframes, numverts, scale, translate, vertremap);
	loadmodel->alias.aliasdata_meshes->data_neighbor3i = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_triangles * sizeof(int[3]));
	Mod_BuildTriangleNeighbors(loadmodel->alias.aliasdata_meshes->data_neighbor3i, loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->num_triangles);
	Mod_CalcAliasModelBBoxes();

	Mem_Free(vertst);
	Mem_Free(vertremap);

	// load the skins
	if ((skinfiles = Mod_LoadSkinFiles()))
	{
		loadmodel->alias.aliasdata_meshes->num_skins = totalskins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(aliasskin_t));
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->alias.aliasdata_meshes->data_skins, skinfiles, "default", "");
		Mod_FreeSkinFiles(skinfiles);
		loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
		for (i = 0;i < loadmodel->numskins;i++)
		{
			loadmodel->skinscenes[i].firstframe = i;
			loadmodel->skinscenes[i].framecount = 1;
			loadmodel->skinscenes[i].loop = true;
			loadmodel->skinscenes[i].framerate = 10;
		}
	}
	else
	{
		loadmodel->alias.aliasdata_meshes->num_skins = totalskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(aliasskin_t));
		loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numskins * sizeof(animscene_t));
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
					Host_Error("Mod_IDP0_Load: invalid interval\n");
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
				if (!Mod_LoadSkinFrame(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA, true, false, true))
					Mod_LoadSkinFrame_Internal(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA, true, false, true, (qbyte *)datapointer, skinwidth, skinheight);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + totalskins, &tempskinframe);
				datapointer += skinwidth * skinheight;
				totalskins++;
			}
		}
		// check for skins that don't exist in the model, but do exist as external images
		// (this was added because yummyluv kept pestering me about support for it)
		while (Mod_LoadSkinFrame(&tempskinframe, va("%s_%i", loadmodel->name, loadmodel->numskins), (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA, true, false, true))
		{
			// expand the arrays to make room
			tempskinscenes = loadmodel->skinscenes;
			loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, (loadmodel->numskins + 1) * sizeof(animscene_t));
			memcpy(loadmodel->skinscenes, tempskinscenes, loadmodel->numskins * sizeof(animscene_t));
			Mem_Free(tempskinscenes);

			tempaliasskins = loadmodel->alias.aliasdata_meshes->data_skins;
			loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, (totalskins + 1) * sizeof(aliasskin_t));
			memcpy(loadmodel->alias.aliasdata_meshes->data_skins, tempaliasskins, totalskins * sizeof(aliasskin_t));
			Mem_Free(tempaliasskins);

			// store the info about the new skin
			Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + totalskins, &tempskinframe);
			strcpy(loadmodel->skinscenes[loadmodel->numskins].name, name);
			loadmodel->skinscenes[loadmodel->numskins].firstframe = totalskins;
			loadmodel->skinscenes[loadmodel->numskins].framecount = 1;
			loadmodel->skinscenes[loadmodel->numskins].framerate = 10.0f;
			loadmodel->skinscenes[loadmodel->numskins].loop = true;

			//increase skin counts
			loadmodel->alias.aliasdata_meshes->num_skins++;
			loadmodel->numskins++;
			totalskins++;
		}
	}
}

static void Mod_MD2_ConvertVerts (vec3_t scale, vec3_t translate, trivertx_t *v, float *out3f, int numverts, int *vertremap)
{
	int i;
	trivertx_t *in;
	for (i = 0;i < numverts;i++, out3f += 3)
	{
		in = v + vertremap[i];
		out3f[0] = in->v[0] * scale[0] + translate[0];
		out3f[1] = in->v[1] * scale[1] + translate[1];
		out3f[2] = in->v[2] * scale[2] + translate[2];
	}
}

void Mod_IDP2_Load(model_t *mod, void *buffer)
{
	int i, j, k, hashindex, num, numxyz, numst, xyz, st, skinwidth, skinheight, *vertremap, version, end, numverts;
	float *stverts, s, t, scale[3], translate[3];
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
	skinframe_t tempskinframe;
	skinfile_t *skinfiles;

	pinmodel = buffer;
	base = buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD2ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->alias.aliastype = ALIASTYPE_ALIAS;
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

	loadmodel->alias.aliasnum_meshes = 1;
	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, sizeof(aliasmesh_t));

	loadmodel->numskins = LittleLong(pinmodel->num_skins);
	numxyz = LittleLong(pinmodel->num_xyz);
	numst = LittleLong(pinmodel->num_st);
	loadmodel->alias.aliasdata_meshes->num_triangles = LittleLong(pinmodel->num_tris);
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->alias.aliasdata_meshes->num_frames = loadmodel->numframes;
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));

	loadmodel->flags = 0; // there are no MD2 flags
	loadmodel->synctype = ST_RAND;

	// load the skins
	inskin = (void*)(base + LittleLong(pinmodel->ofs_skins));
	if ((skinfiles = Mod_LoadSkinFiles()))
	{
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(aliasskin_t));
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->alias.aliasdata_meshes->data_skins, skinfiles, "default", "");
		Mod_FreeSkinFiles(skinfiles);
	}
	else if (loadmodel->numskins)
	{
		// skins found (most likely not a player model)
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(aliasskin_t));
		for (i = 0;i < loadmodel->numskins;i++, inskin += MD2_SKINNAME)
		{
			if (Mod_LoadSkinFrame(&tempskinframe, inskin, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE, true, false, true))
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + i, &tempskinframe);
			else
			{
				Con_Printf("Mod_IDP2_Load: missing skin \"%s\"\n", inskin);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + i, NULL);
			}
		}
	}
	else
	{
		// no skins (most likely a player model)
		loadmodel->numskins = 1;
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(aliasskin_t));
		Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins, NULL);
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
	md2verthashdata = Mem_Alloc(tempmempool, loadmodel->alias.aliasdata_meshes->num_triangles * 3 * sizeof(*hash));
	// swap the triangle list
	num = 0;
	loadmodel->alias.aliasdata_meshes->data_element3i = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_triangles * sizeof(int[3]));
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_triangles;i++)
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
			loadmodel->alias.aliasdata_meshes->data_element3i[i*3+j] = (hash - md2verthashdata);
		}
	}

	Mem_Free(stverts);

	numverts = num;
	loadmodel->alias.aliasdata_meshes->num_vertices = numverts;
	vertremap = Mem_Alloc(loadmodel->mempool, num * sizeof(int));
	loadmodel->alias.aliasdata_meshes->data_texcoord2f = Mem_Alloc(loadmodel->mempool, num * sizeof(float[2]));
	for (i = 0;i < num;i++)
	{
		hash = md2verthashdata + i;
		vertremap[i] = hash->xyz;
		loadmodel->alias.aliasdata_meshes->data_texcoord2f[i*2+0] = hash->st[0];
		loadmodel->alias.aliasdata_meshes->data_texcoord2f[i*2+1] = hash->st[1];
	}

	Mem_Free(md2verthash);
	Mem_Free(md2verthashdata);

	// load the frames
	datapointer = (base + LittleLong(pinmodel->ofs_frames));
	loadmodel->alias.aliasdata_meshes->data_aliasvertex3f = Mem_Alloc(loadmodel->mempool, numverts * loadmodel->alias.aliasdata_meshes->num_frames * sizeof(float[4][3]));
	loadmodel->alias.aliasdata_meshes->data_aliassvector3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + numverts * loadmodel->alias.aliasdata_meshes->num_frames * 3 * 1;
	loadmodel->alias.aliasdata_meshes->data_aliastvector3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + numverts * loadmodel->alias.aliasdata_meshes->num_frames * 3 * 2;
	loadmodel->alias.aliasdata_meshes->data_aliasnormal3f = loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + numverts * loadmodel->alias.aliasdata_meshes->num_frames * 3 * 3;
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_frames;i++)
	{
		pinframe = (md2frame_t *)datapointer;
		datapointer += sizeof(md2frame_t);
		for (j = 0;j < 3;j++)
		{
			scale[j] = LittleFloat(pinframe->scale[j]);
			translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_MD2_ConvertVerts(scale, translate, (void *)datapointer, loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + i * numverts * 3, numverts, vertremap);
		Mod_BuildTextureVectorsAndNormals(loadmodel->alias.aliasdata_meshes->num_vertices, loadmodel->alias.aliasdata_meshes->num_triangles, loadmodel->alias.aliasdata_meshes->data_aliasvertex3f + i * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_texcoord2f, loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->data_aliassvector3f + i * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_aliastvector3f + i * loadmodel->alias.aliasdata_meshes->num_vertices * 3, loadmodel->alias.aliasdata_meshes->data_aliasnormal3f + i * loadmodel->alias.aliasdata_meshes->num_vertices * 3);
		datapointer += numxyz * sizeof(trivertx_t);

		strcpy(loadmodel->animscenes[i].name, pinframe->name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}

	Mem_Free(vertremap);

	loadmodel->alias.aliasdata_meshes->data_neighbor3i = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_triangles * sizeof(int[3]));
	Mod_BuildTriangleNeighbors(loadmodel->alias.aliasdata_meshes->data_neighbor3i, loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->num_triangles);
	Mod_CalcAliasModelBBoxes();
}

void Mod_IDP3_Load(model_t *mod, void *buffer)
{
	int i, j, k, version;
	md3modelheader_t *pinmodel;
	md3frameinfo_t *pinframe;
	md3mesh_t *pinmesh;
	md3tag_t *pintag;
	aliasmesh_t *mesh;
	skinfile_t *skinfiles;

	pinmodel = buffer;

	if (memcmp(pinmodel->identifier, "IDP3", 4))
		Host_Error ("%s is not a MD3 (IDP3) file\n", loadmodel->name);
	version = LittleLong (pinmodel->version);
	if (version != MD3VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD3VERSION);

	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;

	loadmodel->type = mod_alias;
	loadmodel->alias.aliastype = ALIASTYPE_ALIAS;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawFakeShadow = R_Model_Alias_DrawFakeShadow;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	loadmodel->flags = 0;
	loadmodel->synctype = ST_RAND;

	// set up some global info about the model
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->alias.aliasnum_meshes = LittleLong(pinmodel->num_meshes);

	// make skinscenes for the skins (no groups)
	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

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

	// load tags
	loadmodel->alias.aliasnum_tagframes = loadmodel->numframes;
	loadmodel->alias.aliasnum_tags = LittleLong(pinmodel->num_tags);
	loadmodel->alias.aliasdata_tags = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasnum_tagframes * loadmodel->alias.aliasnum_tags * sizeof(aliastag_t));
	for (i = 0, pintag = (md3tag_t *)((qbyte *)pinmodel + LittleLong(pinmodel->lump_tags));i < loadmodel->alias.aliasnum_tagframes * loadmodel->alias.aliasnum_tags;i++, pintag++)
	{
		strcpy(loadmodel->alias.aliasdata_tags[i].name, pintag->name);
		Matrix4x4_CreateIdentity(&loadmodel->alias.aliasdata_tags[i].matrix);
		for (j = 0;j < 3;j++)
		{
			for (k = 0;k < 3;k++)
				loadmodel->alias.aliasdata_tags[i].matrix.m[j][k] = LittleFloat(pintag->rotationmatrix[k * 3 + j]);
			loadmodel->alias.aliasdata_tags[i].matrix.m[j][3] = LittleFloat(pintag->origin[j]);
		}
		//Con_Printf("model \"%s\" frame #%i tag #%i \"%s\"\n", loadmodel->name, i / loadmodel->alias.aliasnum_tags, i % loadmodel->alias.aliasnum_tags, loadmodel->alias.aliasdata_tags[i].name);
	}

	// load meshes
	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasnum_meshes * sizeof(aliasmesh_t));
	for (i = 0, pinmesh = (md3mesh_t *)((qbyte *)pinmodel + LittleLong(pinmodel->lump_meshes));i < loadmodel->alias.aliasnum_meshes;i++, pinmesh = (md3mesh_t *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_end)))
	{
		if (memcmp(pinmesh->identifier, "IDP3", 4))
			Host_Error("Mod_IDP3_Load: invalid mesh identifier (not IDP3)\n");
		mesh = loadmodel->alias.aliasdata_meshes + i;
		mesh->num_skins = loadmodel->numskins;
		mesh->num_frames = LittleLong(pinmesh->num_frames);
		mesh->num_vertices = LittleLong(pinmesh->num_vertices);
		mesh->num_triangles = LittleLong(pinmesh->num_triangles);
		mesh->data_skins = Mem_Alloc(loadmodel->mempool, mesh->num_skins * sizeof(aliasskin_t));
		mesh->data_element3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_texcoord2f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		mesh->data_aliasvertex3f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * mesh->num_frames * sizeof(float[4][3]));
		mesh->data_aliassvector3f = mesh->data_aliasvertex3f + mesh->num_vertices * mesh->num_frames * 3 * 1;
		mesh->data_aliastvector3f = mesh->data_aliasvertex3f + mesh->num_vertices * mesh->num_frames * 3 * 2;
		mesh->data_aliasnormal3f = mesh->data_aliasvertex3f + mesh->num_vertices * mesh->num_frames * 3 * 3;
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = LittleLong(((int *)((qbyte *)pinmesh + pinmesh->lump_elements))[j]);
		for (j = 0;j < mesh->num_vertices;j++)
		{
			mesh->data_texcoord2f[j * 2 + 0] = LittleFloat(((float *)((qbyte *)pinmesh + pinmesh->lump_texcoords))[j * 2 + 0]);
			mesh->data_texcoord2f[j * 2 + 1] = LittleFloat(((float *)((qbyte *)pinmesh + pinmesh->lump_texcoords))[j * 2 + 1]);
		}
		for (j = 0;j < mesh->num_vertices * mesh->num_frames;j++)
		{
			mesh->data_aliasvertex3f[j * 3 + 0] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 0]) * (1.0f / 64.0f);
			mesh->data_aliasvertex3f[j * 3 + 1] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 1]) * (1.0f / 64.0f);
			mesh->data_aliasvertex3f[j * 3 + 2] = LittleShort(((short *)((qbyte *)pinmesh + pinmesh->lump_framevertices))[j * 4 + 2]) * (1.0f / 64.0f);
		}
		for (j = 0;j < mesh->num_frames;j++)
			Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, mesh->data_aliasvertex3f + j * mesh->num_vertices * 3, mesh->data_texcoord2f, mesh->data_element3i, mesh->data_aliassvector3f + j * mesh->num_vertices * 3, mesh->data_aliastvector3f + j * mesh->num_vertices * 3, mesh->data_aliasnormal3f + j * mesh->num_vertices * 3);

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);

		if (LittleLong(pinmesh->num_shaders) >= 1 && ((md3shader_t *)((qbyte *) pinmesh + pinmesh->lump_shaders))->name[0])
			Mod_BuildAliasSkinsFromSkinFiles(mesh->data_skins, skinfiles, pinmesh->name, ((md3shader_t *)((qbyte *) pinmesh + pinmesh->lump_shaders))->name);
		else
			for (j = 0;j < mesh->num_skins;j++)
				Mod_BuildAliasSkinFromSkinFrame(mesh->data_skins + j, NULL);
	}
	Mod_CalcAliasModelBBoxes();
	Mod_FreeSkinFiles(skinfiles);
}

extern void R_Model_Zymotic_DrawSky(entity_render_t *ent);
extern void R_Model_Zymotic_Draw(entity_render_t *ent);
extern void R_Model_Zymotic_DrawFakeShadow(entity_render_t *ent);
extern void R_Model_Zymotic_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius);
extern void R_Model_Zymotic_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
void Mod_ZYMOTICMODEL_Load(model_t *mod, void *buffer)
{
	zymtype1header_t *pinmodel, *pheader;
	qbyte *pbase;

	pinmodel = (void *)buffer;
	pbase = buffer;
	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_ZYMOTICMODEL_Load: %s is not a zymotic model\n");
	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_ZYMOTICMODEL_Load: only type 1 (skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->alias.aliastype = ALIASTYPE_ZYM;
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
	pheader->numverts = loadmodel->alias.zymnum_verts = BigLong(pinmodel->numverts);
	pheader->numtris = loadmodel->alias.zymnum_tris = BigLong(pinmodel->numtris);
	pheader->numshaders = loadmodel->alias.zymnum_shaders = BigLong(pinmodel->numshaders);
	pheader->numbones = loadmodel->alias.zymnum_bones = BigLong(pinmodel->numbones);
	pheader->numscenes = loadmodel->alias.zymnum_scenes = BigLong(pinmodel->numscenes);
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
		loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t));
		loadmodel->skinscenes[0].firstframe = 0;
		loadmodel->skinscenes[0].framecount = 1;
		loadmodel->skinscenes[0].loop = true;
		loadmodel->skinscenes[0].framerate = 10;
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
				Host_Error("Mod_ZYMOTICMODEL_Load: scene firstframe (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, numposes);
			if ((unsigned int) loadmodel->animscenes[i].firstframe + (unsigned int) loadmodel->animscenes[i].framecount > (unsigned int) numposes)
				Host_Error("Mod_ZYMOTICMODEL_Load: scene firstframe (%i) + framecount (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, loadmodel->animscenes[i].framecount, numposes);
			if (loadmodel->animscenes[i].framerate < 0)
				Host_Error("Mod_ZYMOTICMODEL_Load: scene framerate (%f) < 0\n", loadmodel->animscenes[i].framerate);
			scene++;
		}
	}

	{
		int i;
		float *poses;
	//	zymlump_t lump_poses; // float pose[numposes][numbones][3][4]; // animation data
		loadmodel->alias.zymdata_poses = Mem_Alloc(loadmodel->mempool, pheader->lump_poses.length);
		poses = (void *) (pheader->lump_poses.start + pbase);
		for (i = 0;i < pheader->lump_poses.length / 4;i++)
			loadmodel->alias.zymdata_poses[i] = BigFloat(poses[i]);
	}

	{
		int i;
		zymbone_t *bone;
	//	zymlump_t lump_bones; // zymbone_t bone[numbones];
		loadmodel->alias.zymdata_bones = Mem_Alloc(loadmodel->mempool, pheader->numbones * sizeof(zymbone_t));
		bone = (void *) (pheader->lump_bones.start + pbase);
		for (i = 0;i < pheader->numbones;i++)
		{
			memcpy(loadmodel->alias.zymdata_bones[i].name, bone[i].name, sizeof(bone[i].name));
			loadmodel->alias.zymdata_bones[i].flags = BigLong(bone[i].flags);
			loadmodel->alias.zymdata_bones[i].parent = BigLong(bone[i].parent);
			if (loadmodel->alias.zymdata_bones[i].parent >= i)
				Host_Error("Mod_ZYMOTICMODEL_Load: bone[%i].parent >= %i in %s\n", i, i, loadmodel->name);
		}
	}

	{
		int i, *bonecount;
	//	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
		loadmodel->alias.zymdata_vertbonecounts = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
		bonecount = (void *) (pheader->lump_vertbonecounts.start + pbase);
		for (i = 0;i < pheader->numverts;i++)
		{
			loadmodel->alias.zymdata_vertbonecounts[i] = BigLong(bonecount[i]);
			if (loadmodel->alias.zymdata_vertbonecounts[i] < 1)
				Host_Error("Mod_ZYMOTICMODEL_Load: bone vertex count < 1 in %s\n", loadmodel->name);
		}
	}

	{
		int i;
		zymvertex_t *vertdata;
	//	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
		loadmodel->alias.zymdata_verts = Mem_Alloc(loadmodel->mempool, pheader->lump_verts.length);
		vertdata = (void *) (pheader->lump_verts.start + pbase);
		for (i = 0;i < pheader->lump_verts.length / (int) sizeof(zymvertex_t);i++)
		{
			loadmodel->alias.zymdata_verts[i].bonenum = BigLong(vertdata[i].bonenum);
			loadmodel->alias.zymdata_verts[i].origin[0] = BigFloat(vertdata[i].origin[0]);
			loadmodel->alias.zymdata_verts[i].origin[1] = BigFloat(vertdata[i].origin[1]);
			loadmodel->alias.zymdata_verts[i].origin[2] = BigFloat(vertdata[i].origin[2]);
		}
	}

	{
		int i;
		float *intexcoord2f, *outtexcoord2f;
	//	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
		loadmodel->alias.zymdata_texcoords = outtexcoord2f = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(float[2]));
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
		loadmodel->alias.zymdata_renderlist = Mem_Alloc(loadmodel->mempool, pheader->lump_render.length);
		// byteswap, validate, and swap winding order of tris
		count = pheader->numshaders * sizeof(int) + pheader->numtris * sizeof(int[3]);
		if (pheader->lump_render.length != count)
			Host_Error("Mod_ZYMOTICMODEL_Load: renderlist is wrong size in %s (is %i bytes, should be %i bytes)\n", loadmodel->name, pheader->lump_render.length, count);
		outrenderlist = loadmodel->alias.zymdata_renderlist = Mem_Alloc(loadmodel->mempool, count);
		renderlist = (void *) (pheader->lump_render.start + pbase);
		renderlistend = (void *) ((qbyte *) renderlist + pheader->lump_render.length);
		for (i = 0;i < pheader->numshaders;i++)
		{
			if (renderlist >= renderlistend)
				Host_Error("Mod_ZYMOTICMODEL_Load: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
			count = BigLong(*renderlist);renderlist++;
			if (renderlist + count * 3 > renderlistend)
				Host_Error("Mod_ZYMOTICMODEL_Load: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
			*outrenderlist++ = count;
			while (count--)
			{
				outrenderlist[2] = BigLong(renderlist[0]);
				outrenderlist[1] = BigLong(renderlist[1]);
				outrenderlist[0] = BigLong(renderlist[2]);
				if ((unsigned int)outrenderlist[0] >= (unsigned int)pheader->numverts
				 || (unsigned int)outrenderlist[1] >= (unsigned int)pheader->numverts
				 || (unsigned int)outrenderlist[2] >= (unsigned int)pheader->numverts)
					Host_Error("Mod_ZYMOTICMODEL_Load: corrupt renderlist in %s (out of bounds index)\n", loadmodel->name);
				renderlist += 3;
				outrenderlist += 3;
			}
		}
	}

	{
		int i;
		char *shadername;
	//	zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
		loadmodel->alias.zymdata_textures = Mem_Alloc(loadmodel->mempool, pheader->numshaders * sizeof(rtexture_t *));
		shadername = (void *) (pheader->lump_shaders.start + pbase);
		for (i = 0;i < pheader->numshaders;i++)
			loadmodel->alias.zymdata_textures[i] = loadtextureimage(loadmodel->texturepool, shadername + i * 32, 0, 0, true, TEXF_ALPHA | TEXF_PRECACHE | (r_mipskins.integer ? TEXF_MIPMAP : 0));
	}

	{
	//	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
		loadmodel->alias.zymdata_trizone = Mem_Alloc(loadmodel->mempool, pheader->numtris);
		memcpy(loadmodel->alias.zymdata_trizone, (void *) (pheader->lump_trizone.start + pbase), pheader->numtris);
	}
}

