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

void Mod_Alias_GetMesh_Vertex3f(const model_t *model, const frameblend_t *frameblend, const aliasmesh_t *mesh, float *out3f)
{
	if (mesh->num_vertexboneweights)
	{
		int i, k, blends;
		aliasvertexboneweight_t *v;
		float *out, *matrix, m[12], bonepose[256][12];
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = 0;i < model->alias.aliasnum_bones;i++)
		{
			for (k = 0;k < 12;k++)
				m[k] = 0;
			for (blends = 0;blends < 4 && frameblend[blends].lerp > 0;blends++)
			{
				matrix = model->alias.aliasdata_poses + (frameblend[blends].frame * model->alias.aliasnum_bones + i) * 12;
				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * frameblend[blends].lerp;
			}
			if (model->alias.aliasdata_bones[i].parent >= 0)
				R_ConcatTransforms(bonepose[model->alias.aliasdata_bones[i].parent], m, bonepose[i]);
			else
				for (k = 0;k < 12;k++)
					bonepose[i][k] = m[k];
		}
		// blend the vertex bone weights
		memset(out3f, 0, mesh->num_vertices * sizeof(float[3]));
		v = mesh->data_vertexboneweights;
		for (i = 0;i < mesh->num_vertexboneweights;i++, v++)
		{
			out = out3f + v->vertexindex * 3;
			matrix = bonepose[v->boneindex];
			// FIXME: this can very easily be optimized with SSE or 3DNow
			out[0] += v->origin[0] * matrix[0] + v->origin[1] * matrix[1] + v->origin[2] * matrix[ 2] + v->origin[3] * matrix[ 3];
			out[1] += v->origin[0] * matrix[4] + v->origin[1] * matrix[5] + v->origin[2] * matrix[ 6] + v->origin[3] * matrix[ 7];
			out[2] += v->origin[0] * matrix[8] + v->origin[1] * matrix[9] + v->origin[2] * matrix[10] + v->origin[3] * matrix[11];
		}
	}
	else
	{
		int i, vertcount;
		float lerp1, lerp2, lerp3, lerp4;
		const float *vertsbase, *verts1, *verts2, *verts3, *verts4;
		// vertex morph
		vertsbase = mesh->data_morphvertex3f;
		vertcount = mesh->num_vertices;
		verts1 = vertsbase + frameblend[0].frame * vertcount * 3;
		lerp1 = frameblend[0].lerp;
		if (frameblend[1].lerp)
		{
			verts2 = vertsbase + frameblend[1].frame * vertcount * 3;
			lerp2 = frameblend[1].lerp;
			if (frameblend[2].lerp)
			{
				verts3 = vertsbase + frameblend[2].frame * vertcount * 3;
				lerp3 = frameblend[2].lerp;
				if (frameblend[3].lerp)
				{
					verts4 = vertsbase + frameblend[3].frame * vertcount * 3;
					lerp4 = frameblend[3].lerp;
					for (i = 0;i < vertcount * 3;i++)
						VectorMAMAMAM(lerp1, verts1 + i, lerp2, verts2 + i, lerp3, verts3 + i, lerp4, verts4 + i, out3f + i);
				}
				else
					for (i = 0;i < vertcount * 3;i++)
						VectorMAMAM(lerp1, verts1 + i, lerp2, verts2 + i, lerp3, verts3 + i, out3f + i);
			}
			else
				for (i = 0;i < vertcount * 3;i++)
					VectorMAM(lerp1, verts1 + i, lerp2, verts2 + i, out3f + i);
		}
		else
			memcpy(out3f, verts1, vertcount * sizeof(float[3]));
	}
}

int Mod_Alias_GetTagMatrix(const model_t *model, int poseframe, int tagindex, matrix4x4_t *outmatrix)
{
	const float *boneframe;
	float tempbonematrix[12], bonematrix[12];
	Matrix4x4_CreateIdentity(outmatrix);
	if (model->alias.aliasnum_bones)
	{
		if (tagindex < 0 || tagindex >= model->alias.aliasnum_bones)
			return 4;
		if (poseframe >= model->alias.aliasnum_poses)
			return 6;
		boneframe = model->alias.aliasdata_poses + poseframe * model->alias.aliasnum_bones * 12;
		memcpy(bonematrix, boneframe + tagindex * 12, sizeof(float[12]));
		while (model->alias.aliasdata_bones[tagindex].parent >= 0)
		{
			memcpy(tempbonematrix, bonematrix, sizeof(float[12]));
			R_ConcatTransforms(boneframe + model->alias.aliasdata_bones[tagindex].parent * 12, tempbonematrix, bonematrix);
			tagindex = model->alias.aliasdata_bones[tagindex].parent;
		}
		outmatrix->m[0][0] = bonematrix[0];
		outmatrix->m[0][1] = bonematrix[1];
		outmatrix->m[0][2] = bonematrix[2];
		outmatrix->m[0][3] = bonematrix[3];
		outmatrix->m[1][0] = bonematrix[4];
		outmatrix->m[1][1] = bonematrix[5];
		outmatrix->m[1][2] = bonematrix[6];
		outmatrix->m[1][3] = bonematrix[7];
		outmatrix->m[2][0] = bonematrix[8];
		outmatrix->m[2][1] = bonematrix[9];
		outmatrix->m[2][2] = bonematrix[10];
		outmatrix->m[2][3] = bonematrix[11];
		outmatrix->m[3][0] = 0;
		outmatrix->m[3][1] = 0;
		outmatrix->m[3][2] = 0;
		outmatrix->m[3][3] = 1;
	}
	else if (model->alias.aliasnum_tags)
	{
		if (tagindex < 0 || tagindex >= model->alias.aliasnum_tags)
			return 4;
		if (poseframe >= model->alias.aliasnum_tagframes)
			return 6;
		*outmatrix = model->alias.aliasdata_tags[poseframe * model->alias.aliasnum_tags + tagindex].matrix;
	}
	return 0;
}

int Mod_Alias_GetTagIndexForName(const model_t *model, unsigned int skin, const char *tagname)
{
	int i;
	if (model->data_overridetagnamesforskin && skin < (unsigned int)model->numskins && model->data_overridetagnamesforskin[(unsigned int)skin].num_overridetagnames)
		for (i = 0;i < model->data_overridetagnamesforskin[skin].num_overridetagnames;i++)
			if (!strcasecmp(tagname, model->data_overridetagnamesforskin[skin].data_overridetagnames[i].name))
				return i + 1;
	if (model->alias.aliasnum_bones)
		for (i = 0;i < model->alias.aliasnum_bones;i++)
			if (!strcasecmp(tagname, model->alias.aliasdata_bones[i].name))
				return i + 1;
	if (model->alias.aliasnum_tags)
		for (i = 0;i < model->alias.aliasnum_tags;i++)
			if (!strcasecmp(tagname, model->alias.aliasdata_tags[i].name))
				return i + 1;
	return 0;
}

static void Mod_Alias_Mesh_CompileFrameZero(aliasmesh_t *mesh)
{
	frameblend_t frameblend[4] = {{0, 1}, {0, 0}, {0, 0}, {0, 0}};
	mesh->data_basevertex3f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[3][4]));
	mesh->data_basesvector3f = mesh->data_basevertex3f + mesh->num_vertices * 3;
	mesh->data_basetvector3f = mesh->data_basevertex3f + mesh->num_vertices * 6;
	mesh->data_basenormal3f = mesh->data_basevertex3f + mesh->num_vertices * 9;
	Mod_Alias_GetMesh_Vertex3f(loadmodel, frameblend, mesh, mesh->data_basevertex3f);
	Mod_BuildTextureVectorsAndNormals(mesh->num_vertices, mesh->num_triangles, mesh->data_basevertex3f, mesh->data_texcoord2f, mesh->data_element3i, mesh->data_basesvector3f, mesh->data_basetvector3f, mesh->data_basenormal3f);
}

static void Mod_MDLMD2MD3_TraceBox(model_t *model, int frame, trace_t *trace, const vec3_t boxstartmins, const vec3_t boxstartmaxs, const vec3_t boxendmins, const vec3_t boxendmaxs, int hitsupercontentsmask)
{
	int i, framenum;
	float segmentmins[3], segmentmaxs[3];
	colbrushf_t *thisbrush_start, *thisbrush_end;
	matrix4x4_t startmatrix, endmatrix;
	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1;
	trace->realfraction = 1;
	trace->hitsupercontentsmask = hitsupercontentsmask;
	segmentmins[0] = min(boxstartmins[0], boxendmins[0]);
	segmentmins[1] = min(boxstartmins[1], boxendmins[1]);
	segmentmins[2] = min(boxstartmins[2], boxendmins[2]);
	segmentmaxs[0] = max(boxstartmaxs[0], boxendmaxs[0]);
	segmentmaxs[1] = max(boxstartmaxs[1], boxendmaxs[1]);
	segmentmaxs[2] = max(boxstartmaxs[2], boxendmaxs[2]);
	if (VectorCompare(boxstartmins, boxstartmaxs) && VectorCompare(boxendmins, boxendmaxs))
	{
		// line trace
		for (i = 0;i < model->alias.aliasnum_meshes;i++)
		{
			framenum = frame;
			if (framenum < 0 || framenum > model->alias.aliasdata_meshes[i].num_morphframes)
				framenum = 0;
			if (model->alias.aliasdata_meshes[i].data_morphvertex3f)
				Collision_TraceLineTriangleMeshFloat(trace, boxstartmins, boxendmins, model->alias.aliasdata_meshes[i].num_triangles, model->alias.aliasdata_meshes[i].data_element3i, model->alias.aliasdata_meshes[i].data_morphvertex3f + framenum * model->alias.aliasdata_meshes[i].num_vertices * 3, SUPERCONTENTS_SOLID, segmentmins, segmentmaxs);
			//else
	 			// FIXME!!!  this needs to handle skeletal!
		}
	}
	else
	{
		// box trace, performed as brush trace
		Matrix4x4_CreateIdentity(&startmatrix);
		Matrix4x4_CreateIdentity(&endmatrix);
		thisbrush_start = Collision_BrushForBox(&startmatrix, boxstartmins, boxstartmaxs);
		thisbrush_end = Collision_BrushForBox(&endmatrix, boxendmins, boxendmaxs);
		for (i = 0;i < model->alias.aliasnum_meshes;i++)
		{
			framenum = frame;
			if (framenum < 0 || framenum > model->alias.aliasdata_meshes[i].num_morphframes)
				framenum = 0;
			if (model->alias.aliasdata_meshes[i].data_morphvertex3f)
				Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, model->alias.aliasdata_meshes[i].num_triangles, model->alias.aliasdata_meshes[i].data_element3i, model->alias.aliasdata_meshes[i].data_morphvertex3f + framenum * model->alias.aliasdata_meshes[i].num_vertices * 3, SUPERCONTENTS_SOLID, segmentmins, segmentmaxs);
			//else
	 			// FIXME!!!  this needs to handle skeletal!
		}
	}
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
		for (vnum = 0, v = mesh->data_morphvertex3f;vnum < mesh->num_vertices * mesh->num_morphframes;vnum++, v += 3)
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
			{
				Con_Printf("%s has an invalid interval %f, changing to 0.1\n", loadmodel->name, interval);
				interval = 0.1f;
			}
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
			Mod_ConvertAliasVerts(inverts, scale, translate, (trivertx_t *)datapointer, loadmodel->alias.aliasdata_meshes->data_morphvertex3f + pose * loadmodel->alias.aliasdata_meshes->num_vertices * 3, vertremap);
			datapointer += sizeof(trivertx_t) * inverts;
			pose++;
		}
	}
}

static skinframe_t missingskinframe;
static void Mod_BuildAliasSkinFromSkinFrame(texture_t *skin, skinframe_t *skinframe)
{
	// hack
	if (skinframe == NULL)
	{
		skinframe = &missingskinframe;
		memset(skinframe, 0, sizeof(*skinframe));
		skinframe->base = r_texture_notexture;
	}

	skin->skin = *skinframe;
}

static void Mod_BuildAliasSkinsFromSkinFiles(texture_t *skin, skinfile_t *skinfile, char *meshname, char *shadername)
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
					if (Mod_LoadSkinFrame(&tempskinframe, skinfileitem->replacement, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, true, false, true))
						Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
					else
					{
						Con_Printf("mesh \"%s\": failed to load skin #%i \"%s\", falling back to mesh's internal shader name \"%s\"\n", meshname, i, skinfileitem->replacement, shadername);
						if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, true, false, true))
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
		if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, true, false, true))
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
extern void R_Model_Alias_DrawShadowVolume(entity_render_t *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs);
extern void R_Model_Alias_DrawLight(entity_render_t *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist);
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
	texture_t *tempaliasskins;
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
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;

	loadmodel->alias.aliasnum_meshes = 1;
	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, sizeof(aliasmesh_t));

	loadmodel->numskins = LittleLong(pinmodel->numskins);
	BOUNDI(loadmodel->numskins,0,65536);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,65536);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,65536);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,65536);
	loadmodel->alias.aliasdata_meshes->num_triangles = LittleLong(pinmodel->numtris);
	BOUNDI(loadmodel->alias.aliasdata_meshes->num_triangles,0,65536);
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
	loadmodel->alias.aliasdata_meshes->num_morphframes = 0;
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
			loadmodel->alias.aliasdata_meshes->num_morphframes++;
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
	loadmodel->alias.aliasdata_meshes->data_morphvertex3f = Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * loadmodel->alias.aliasdata_meshes->num_morphframes * loadmodel->alias.aliasdata_meshes->num_vertices);
	loadmodel->alias.aliasdata_meshes->data_neighbor3i = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_triangles * sizeof(int[3]));
	Mod_MDL_LoadFrames (startframes, numverts, scale, translate, vertremap);
	Mod_BuildTriangleNeighbors(loadmodel->alias.aliasdata_meshes->data_neighbor3i, loadmodel->alias.aliasdata_meshes->data_element3i, loadmodel->alias.aliasdata_meshes->num_triangles);
	Mod_CalcAliasModelBBoxes();
	Mod_Alias_Mesh_CompileFrameZero(loadmodel->alias.aliasdata_meshes);

	Mem_Free(vertst);
	Mem_Free(vertremap);

	// load the skins
	if ((skinfiles = Mod_LoadSkinFiles()))
	{
		loadmodel->alias.aliasdata_meshes->num_skins = totalskins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(texture_t));
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
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(texture_t));
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
				{
					Con_Printf("%s has an invalid interval %f, changing to 0.1\n", loadmodel->name, interval);
					interval = 0.1f;
				}
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
				if (!Mod_LoadSkinFrame(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA | TEXF_PICMIP, true, false, true))
					Mod_LoadSkinFrame_Internal(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA | TEXF_PICMIP, true, false, r_fullbrights.integer, (qbyte *)datapointer, skinwidth, skinheight);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + totalskins, &tempskinframe);
				datapointer += skinwidth * skinheight;
				totalskins++;
			}
		}
		// check for skins that don't exist in the model, but do exist as external images
		// (this was added because yummyluv kept pestering me about support for it)
		while (Mod_LoadSkinFrame(&tempskinframe, va("%s_%i", loadmodel->name, loadmodel->numskins), (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_ALPHA | TEXF_PICMIP, true, false, true))
		{
			// expand the arrays to make room
			tempskinscenes = loadmodel->skinscenes;
			loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, (loadmodel->numskins + 1) * sizeof(animscene_t));
			memcpy(loadmodel->skinscenes, tempskinscenes, loadmodel->numskins * sizeof(animscene_t));
			Mem_Free(tempskinscenes);

			tempaliasskins = loadmodel->alias.aliasdata_meshes->data_skins;
			loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, (totalskins + 1) * sizeof(texture_t));
			memcpy(loadmodel->alias.aliasdata_meshes->data_skins, tempaliasskins, totalskins * sizeof(texture_t));
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
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;

	if (LittleLong(pinmodel->num_tris) < 1 || LittleLong(pinmodel->num_tris) > 65536)
		Host_Error ("%s has invalid number of triangles: %i", loadmodel->name, LittleLong(pinmodel->num_tris));
	if (LittleLong(pinmodel->num_xyz) < 1 || LittleLong(pinmodel->num_xyz) > 65536)
		Host_Error ("%s has invalid number of vertices: %i", loadmodel->name, LittleLong(pinmodel->num_xyz));
	if (LittleLong(pinmodel->num_frames) < 1 || LittleLong(pinmodel->num_frames) > 65536)
		Host_Error ("%s has invalid number of frames: %i", loadmodel->name, LittleLong(pinmodel->num_frames));
	if (LittleLong(pinmodel->num_skins) < 0 || LittleLong(pinmodel->num_skins) > 256)
		Host_Error ("%s has invalid number of skins: %i", loadmodel->name, LittleLong(pinmodel->num_skins));

	end = LittleLong(pinmodel->ofs_end);
	if (LittleLong(pinmodel->num_skins) >= 1 && (LittleLong(pinmodel->ofs_skins) <= 0 || LittleLong(pinmodel->ofs_skins) >= end))
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_st) <= 0 || LittleLong(pinmodel->ofs_st) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_tris) <= 0 || LittleLong(pinmodel->ofs_tris) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_frames) <= 0 || LittleLong(pinmodel->ofs_frames) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);
	if (LittleLong(pinmodel->ofs_glcmds) <= 0 || LittleLong(pinmodel->ofs_glcmds) >= end)
		Host_Error ("%s is not a valid model", loadmodel->name);

	loadmodel->alias.aliasnum_meshes = 1;
	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, sizeof(aliasmesh_t));

	loadmodel->numskins = LittleLong(pinmodel->num_skins);
	numxyz = LittleLong(pinmodel->num_xyz);
	numst = LittleLong(pinmodel->num_st);
	loadmodel->alias.aliasdata_meshes->num_triangles = LittleLong(pinmodel->num_tris);
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->alias.aliasdata_meshes->num_morphframes = loadmodel->numframes;
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));

	loadmodel->flags = 0; // there are no MD2 flags
	loadmodel->synctype = ST_RAND;

	// load the skins
	inskin = (void*)(base + LittleLong(pinmodel->ofs_skins));
	if ((skinfiles = Mod_LoadSkinFiles()))
	{
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(texture_t));
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->alias.aliasdata_meshes->data_skins, skinfiles, "default", "");
		Mod_FreeSkinFiles(skinfiles);
	}
	else if (loadmodel->numskins)
	{
		// skins found (most likely not a player model)
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(texture_t));
		for (i = 0;i < loadmodel->numskins;i++, inskin += MD2_SKINNAME)
		{
			if (Mod_LoadSkinFrame(&tempskinframe, inskin, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, true, false, true))
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + i, &tempskinframe);
			else
			{
				Con_Printf("%s is missing skin \"%s\"\n", loadmodel->name, inskin);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->alias.aliasdata_meshes->data_skins + i, NULL);
			}
		}
	}
	else
	{
		// no skins (most likely a player model)
		loadmodel->numskins = 1;
		loadmodel->alias.aliasdata_meshes->num_skins = loadmodel->numskins;
		loadmodel->alias.aliasdata_meshes->data_skins = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasdata_meshes->num_skins * sizeof(texture_t));
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
			Con_Printf("%s has an invalid skin coordinate (%i %i) on vert %i, changing to 0 0\n", loadmodel->name, j, k, i);
			j = 0;
			k = 0;
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
			if (xyz >= numxyz)
			{
				Con_Printf("%s has an invalid xyz index (%i) on triangle %i, resetting to 0\n", loadmodel->name, xyz, i);
				xyz = 0;
			}
			if (st >= numst)
			{
				Con_Printf("%s has an invalid st index (%i) on triangle %i, resetting to 0\n", loadmodel->name, st, i);
				st = 0;
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
	loadmodel->alias.aliasdata_meshes->data_morphvertex3f = Mem_Alloc(loadmodel->mempool, numverts * loadmodel->alias.aliasdata_meshes->num_morphframes * sizeof(float[3]));
	for (i = 0;i < loadmodel->alias.aliasdata_meshes->num_morphframes;i++)
	{
		pinframe = (md2frame_t *)datapointer;
		datapointer += sizeof(md2frame_t);
		for (j = 0;j < 3;j++)
		{
			scale[j] = LittleFloat(pinframe->scale[j]);
			translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_MD2_ConvertVerts(scale, translate, (void *)datapointer, loadmodel->alias.aliasdata_meshes->data_morphvertex3f + i * numverts * 3, numverts, vertremap);
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
	Mod_Alias_Mesh_CompileFrameZero(loadmodel->alias.aliasdata_meshes);
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
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;
	loadmodel->flags = LittleLong(pinmodel->flags);
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
		mesh->num_morphframes = LittleLong(pinmesh->num_frames);
		mesh->num_vertices = LittleLong(pinmesh->num_vertices);
		mesh->num_triangles = LittleLong(pinmesh->num_triangles);
		mesh->data_skins = Mem_Alloc(loadmodel->mempool, mesh->num_skins * sizeof(texture_t));
		mesh->data_element3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_texcoord2f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		mesh->data_morphvertex3f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * mesh->num_morphframes * sizeof(float[3]));
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = LittleLong(((int *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_elements)))[j]);
		for (j = 0;j < mesh->num_vertices;j++)
		{
			mesh->data_texcoord2f[j * 2 + 0] = LittleFloat(((float *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_texcoords)))[j * 2 + 0]);
			mesh->data_texcoord2f[j * 2 + 1] = LittleFloat(((float *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_texcoords)))[j * 2 + 1]);
		}
		for (j = 0;j < mesh->num_vertices * mesh->num_morphframes;j++)
		{
			mesh->data_morphvertex3f[j * 3 + 0] = LittleShort(((short *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 0]) * (1.0f / 64.0f);
			mesh->data_morphvertex3f[j * 3 + 1] = LittleShort(((short *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 1]) * (1.0f / 64.0f);
			mesh->data_morphvertex3f[j * 3 + 2] = LittleShort(((short *)((qbyte *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 2]) * (1.0f / 64.0f);
		}

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_Alias_Mesh_CompileFrameZero(mesh);

		if (LittleLong(pinmesh->num_shaders) >= 1)
			Mod_BuildAliasSkinsFromSkinFiles(mesh->data_skins, skinfiles, pinmesh->name, ((md3shader_t *)((qbyte *) pinmesh + LittleLong(pinmesh->lump_shaders)))->name);
		else
			for (j = 0;j < mesh->num_skins;j++)
				Mod_BuildAliasSkinFromSkinFrame(mesh->data_skins + j, NULL);
	}
	Mod_CalcAliasModelBBoxes();
	Mod_FreeSkinFiles(skinfiles);
}

void Mod_ZYMOTICMODEL_Load(model_t *mod, void *buffer)
{
	zymtype1header_t *pinmodel, *pheader;
	qbyte *pbase;
	int i, j, k, l, numposes, *bonecount, *vertbonecounts, count, *renderlist, *renderlistend, *outelements, *remapvertices;
	float modelradius, corner[2], *poses, *intexcoord2f, *outtexcoord2f;
	zymvertex_t *verts, *vertdata;
	zymscene_t *scene;
	zymbone_t *bone;
	char *shadername;
	skinfile_t *skinfiles;
	aliasmesh_t *mesh;

	pinmodel = (void *)buffer;
	pbase = buffer;
	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_ZYMOTICMODEL_Load: %s is not a zymotic model\n");
	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_ZYMOTICMODEL_Load: only type 1 (skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Alias_Draw;
	loadmodel->DrawShadowVolume = R_Model_Alias_DrawShadowVolume;
	loadmodel->DrawLight = R_Model_Alias_DrawLight;
	//loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox; // FIXME: implement collisions
	loadmodel->flags = 0; // there are no flags on zym models
	loadmodel->synctype = ST_RAND;

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
	pheader->numverts = BigLong(pinmodel->numverts);
	pheader->numtris = BigLong(pinmodel->numtris);
	pheader->numshaders = BigLong(pinmodel->numshaders);
	pheader->numbones = BigLong(pinmodel->numbones);
	pheader->numscenes = BigLong(pinmodel->numscenes);
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

	loadmodel->numframes = pheader->numscenes;
	loadmodel->alias.aliasnum_meshes = pheader->numshaders;

	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;

	// make skinscenes for the skins (no groups)
	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

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

	// go through the lumps, swapping things

	//zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
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
			Host_Error("%s scene->firstframe (%i) >= numposes (%i)\n", loadmodel->name, loadmodel->animscenes[i].firstframe, numposes);
		if ((unsigned int) loadmodel->animscenes[i].firstframe + (unsigned int) loadmodel->animscenes[i].framecount > (unsigned int) numposes)
			Host_Error("%s scene->firstframe (%i) + framecount (%i) >= numposes (%i)\n", loadmodel->name, loadmodel->animscenes[i].firstframe, loadmodel->animscenes[i].framecount, numposes);
		if (loadmodel->animscenes[i].framerate < 0)
			Host_Error("%s scene->framerate (%f) < 0\n", loadmodel->name, loadmodel->animscenes[i].framerate);
		scene++;
	}

	//zymlump_t lump_poses; // float pose[numposes][numbones][3][4]; // animation data
	loadmodel->alias.aliasnum_poses = pheader->lump_poses.length / sizeof(float[3][4]);
	loadmodel->alias.aliasdata_poses = Mem_Alloc(loadmodel->mempool, pheader->lump_poses.length);
	poses = (void *) (pheader->lump_poses.start + pbase);
	for (i = 0;i < pheader->lump_poses.length / 4;i++)
		loadmodel->alias.aliasdata_poses[i] = BigFloat(poses[i]);

	//zymlump_t lump_bones; // zymbone_t bone[numbones];
	loadmodel->alias.aliasnum_bones = pheader->numbones;
	loadmodel->alias.aliasdata_bones = Mem_Alloc(loadmodel->mempool, pheader->numbones * sizeof(aliasbone_t));
	bone = (void *) (pheader->lump_bones.start + pbase);
	for (i = 0;i < pheader->numbones;i++)
	{
		memcpy(loadmodel->alias.aliasdata_bones[i].name, bone[i].name, sizeof(bone[i].name));
		loadmodel->alias.aliasdata_bones[i].flags = BigLong(bone[i].flags);
		loadmodel->alias.aliasdata_bones[i].parent = BigLong(bone[i].parent);
		if (loadmodel->alias.aliasdata_bones[i].parent >= i)
			Host_Error("%s bone[%i].parent >= %i\n", loadmodel->name, i, i);
	}

	//zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	vertbonecounts = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
	bonecount = (void *) (pheader->lump_vertbonecounts.start + pbase);
	for (i = 0;i < pheader->numverts;i++)
	{
		vertbonecounts[i] = BigLong(bonecount[i]);
		if (vertbonecounts[i] < 1)
			Host_Error("%s bonecount[%i] < 1\n", loadmodel->name, i);
	}

	//zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	verts = Mem_Alloc(loadmodel->mempool, pheader->lump_verts.length);
	vertdata = (void *) (pheader->lump_verts.start + pbase);
	for (i = 0;i < pheader->lump_verts.length / (int) sizeof(zymvertex_t);i++)
	{
		verts[i].bonenum = BigLong(vertdata[i].bonenum);
		verts[i].origin[0] = BigFloat(vertdata[i].origin[0]);
		verts[i].origin[1] = BigFloat(vertdata[i].origin[1]);
		verts[i].origin[2] = BigFloat(vertdata[i].origin[2]);
	}

	//zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	outtexcoord2f = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(float[2]));
	intexcoord2f = (void *) (pheader->lump_texcoords.start + pbase);
	for (i = 0;i < pheader->numverts;i++)
	{
		outtexcoord2f[i*2+0] = BigFloat(intexcoord2f[i*2+0]);
		// flip T coordinate for OpenGL
		outtexcoord2f[i*2+1] = 1 - BigFloat(intexcoord2f[i*2+1]);
	}

	//zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
	//loadmodel->alias.zymdata_trizone = Mem_Alloc(loadmodel->mempool, pheader->numtris);
	//memcpy(loadmodel->alias.zymdata_trizone, (void *) (pheader->lump_trizone.start + pbase), pheader->numtris);

	loadmodel->alias.aliasdata_meshes = Mem_Alloc(loadmodel->mempool, loadmodel->alias.aliasnum_meshes * sizeof(aliasmesh_t));

	//zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
	//zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	// byteswap, validate, and swap winding order of tris
	count = pheader->numshaders * sizeof(int) + pheader->numtris * sizeof(int[3]);
	if (pheader->lump_render.length != count)
		Host_Error("%s renderlist is wrong size (%i bytes, should be %i bytes)\n", loadmodel->name, pheader->lump_render.length, count);
	renderlist = (void *) (pheader->lump_render.start + pbase);
	renderlistend = (void *) ((qbyte *) renderlist + pheader->lump_render.length);
	for (i = 0;i < loadmodel->alias.aliasnum_meshes;i++)
	{
		if (renderlist >= renderlistend)
			Host_Error("%s corrupt renderlist (wrong size)\n", loadmodel->name);
		count = BigLong(*renderlist);renderlist++;
		if (renderlist + count * 3 > renderlistend || (i == pheader->numshaders - 1 && renderlist + count * 3 != renderlistend))
			Host_Error("%s corrupt renderlist (wrong size)\n", loadmodel->name);
		mesh = loadmodel->alias.aliasdata_meshes + i;
		mesh->num_skins = loadmodel->numskins;
		mesh->num_triangles = count;
		mesh->data_skins = Mem_Alloc(loadmodel->mempool, mesh->num_skins * sizeof(texture_t));
		mesh->data_element3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		outelements = mesh->data_element3i;
		for (j = 0;j < mesh->num_triangles;j++)
		{
			outelements[2] = BigLong(renderlist[0]);
			outelements[1] = BigLong(renderlist[1]);
			outelements[0] = BigLong(renderlist[2]);
			if ((unsigned int)outelements[0] >= (unsigned int)pheader->numverts
			 || (unsigned int)outelements[1] >= (unsigned int)pheader->numverts
			 || (unsigned int)outelements[2] >= (unsigned int)pheader->numverts)
				Host_Error("%s corrupt renderlist (out of bounds index)\n", loadmodel->name);
			renderlist += 3;
			outelements += 3;
		}
		remapvertices = Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
		mesh->num_vertices = Mod_BuildVertexRemapTableFromElements(mesh->num_triangles * 3, mesh->data_element3i, pheader->numverts, remapvertices);
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = remapvertices[mesh->data_element3i[j]];
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		mesh->data_texcoord2f = Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		for (j = 0;j < pheader->numverts;j++)
		{
			if (remapvertices[j] >= 0)
			{
				mesh->data_texcoord2f[remapvertices[j]*2+0] = outtexcoord2f[j*2+0];
				mesh->data_texcoord2f[remapvertices[j]*2+1] = outtexcoord2f[j*2+1];
			}
		}
		mesh->num_vertexboneweights = 0;
		for (j = 0;j < mesh->num_vertices;j++)
			if (remapvertices[j] >= 0)
				mesh->num_vertexboneweights += vertbonecounts[remapvertices[j]];
		mesh->data_vertexboneweights = Mem_Alloc(loadmodel->mempool, mesh->num_vertexboneweights * sizeof(aliasvertexboneweight_t));
		mesh->num_vertexboneweights = 0;
		// note this vertexboneweight ordering requires that the remapvertices array is sequential numbers (separated by -1 values for omitted vertices)
		l = 0;
		for (j = 0;j < mesh->num_vertices;j++)
		{
			if (remapvertices[j] < 0)
			{
				l += vertbonecounts[j];
				continue;
			}
			for (k = 0;k < vertbonecounts[j];k++)
			{
				// this format really should have had a per vertexweight weight value...
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].vertexindex = remapvertices[j];
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].boneindex = verts[l].bonenum;
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[3] = 1.0f / vertbonecounts[j];
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[0] = verts[l].origin[0] * mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[3];
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[1] = verts[l].origin[1] * mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[3];
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[2] = verts[l].origin[2] * mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[3];
				mesh->num_vertexboneweights++;
				l++;
			}
		}

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_Alias_Mesh_CompileFrameZero(mesh);

		// since zym models do not have named sections, reuse their shader
		// name as the section name
		shadername = (char *) (pheader->lump_shaders.start + pbase) + i * 32;
		if (shadername[0])
			Mod_BuildAliasSkinsFromSkinFiles(mesh->data_skins, skinfiles, shadername, shadername);
		else
			for (j = 0;j < mesh->num_skins;j++)
				Mod_BuildAliasSkinFromSkinFrame(mesh->data_skins + j, NULL);
	}

	Mem_Free(vertbonecounts);
	Mem_Free(verts);
	Mem_Free(outtexcoord2f);
}

