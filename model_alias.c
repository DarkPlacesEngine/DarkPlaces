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

cvar_t r_skeletal_debugbone = {0, "r_skeletal_debugbone", "-1"};
cvar_t r_skeletal_debugbonecomponent = {0, "r_skeletal_debugbonecomponent", "3"};
cvar_t r_skeletal_debugbonevalue = {0, "r_skeletal_debugbonevalue", "100"};
cvar_t r_skeletal_debugtranslatex = {0, "r_skeletal_debugtranslatex", "1"};
cvar_t r_skeletal_debugtranslatey = {0, "r_skeletal_debugtranslatey", "1"};
cvar_t r_skeletal_debugtranslatez = {0, "r_skeletal_debugtranslatez", "1"};

void Mod_AliasInit (void)
{
	Cvar_RegisterVariable(&r_skeletal_debugbone);
	Cvar_RegisterVariable(&r_skeletal_debugbonecomponent);
	Cvar_RegisterVariable(&r_skeletal_debugbonevalue);
	Cvar_RegisterVariable(&r_skeletal_debugtranslatex);
	Cvar_RegisterVariable(&r_skeletal_debugtranslatey);
	Cvar_RegisterVariable(&r_skeletal_debugtranslatez);
}

void Mod_Alias_GetMesh_Vertex3f(const model_t *model, const frameblend_t *frameblend, const surfmesh_t *mesh, float *out3f)
{
	if (mesh->num_vertexboneweights)
	{
		int i, k, blends;
		surfmeshvertexboneweight_t *v;
		float *out, *matrix, m[12], bonepose[256][12];
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = 0;i < model->num_bones;i++)
		{
			for (k = 0;k < 12;k++)
				m[k] = 0;
			for (blends = 0;blends < 4 && frameblend[blends].lerp > 0;blends++)
			{
				matrix = model->data_poses + (frameblend[blends].frame * model->num_bones + i) * 12;
				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * frameblend[blends].lerp;
			}
			if (i == r_skeletal_debugbone.integer)
				m[r_skeletal_debugbonecomponent.integer % 12] += r_skeletal_debugbonevalue.value;
			m[3] *= r_skeletal_debugtranslatex.value;
			m[7] *= r_skeletal_debugtranslatey.value;
			m[11] *= r_skeletal_debugtranslatez.value;
			if (model->data_bones[i].parent >= 0)
				R_ConcatTransforms(bonepose[model->data_bones[i].parent], m, bonepose[i]);
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
		if (!mesh->data_morphvertex3f)
			Host_Error("model %s has no skeletal or vertex morph animation data\n", model->name);
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
	if (model->num_bones)
	{
		if (tagindex < 0 || tagindex >= model->num_bones)
			return 4;
		if (poseframe >= model->num_poses)
			return 6;
		boneframe = model->data_poses + poseframe * model->num_bones * 12;
		memcpy(bonematrix, boneframe + tagindex * 12, sizeof(float[12]));
		while (model->data_bones[tagindex].parent >= 0)
		{
			memcpy(tempbonematrix, bonematrix, sizeof(float[12]));
			R_ConcatTransforms(boneframe + model->data_bones[tagindex].parent * 12, tempbonematrix, bonematrix);
			tagindex = model->data_bones[tagindex].parent;
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
	else if (model->num_tags)
	{
		if (tagindex < 0 || tagindex >= model->num_tags)
			return 4;
		if (poseframe >= model->num_tagframes)
			return 6;
		*outmatrix = model->data_tags[poseframe * model->num_tags + tagindex].matrix;
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
	if (model->num_bones)
		for (i = 0;i < model->num_bones;i++)
			if (!strcasecmp(tagname, model->data_bones[i].name))
				return i + 1;
	if (model->num_tags)
		for (i = 0;i < model->num_tags;i++)
			if (!strcasecmp(tagname, model->data_tags[i].name))
				return i + 1;
	return 0;
}

static void Mod_Alias_Mesh_CompileFrameZero(surfmesh_t *mesh)
{
	frameblend_t frameblend[4] = {{0, 1}, {0, 0}, {0, 0}, {0, 0}};
	mesh->data_vertex3f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[3][4]));
	mesh->data_svector3f = mesh->data_vertex3f + mesh->num_vertices * 3;
	mesh->data_tvector3f = mesh->data_vertex3f + mesh->num_vertices * 6;
	mesh->data_normal3f = mesh->data_vertex3f + mesh->num_vertices * 9;
	Mod_Alias_GetMesh_Vertex3f(loadmodel, frameblend, mesh, mesh->data_vertex3f);
	Mod_BuildTextureVectorsAndNormals(0, mesh->num_vertices, mesh->num_triangles, mesh->data_vertex3f, mesh->data_texcoordtexture2f, mesh->data_element3i, mesh->data_svector3f, mesh->data_tvector3f, mesh->data_normal3f, true);
}

static void Mod_MDLMD2MD3_TraceBox(model_t *model, int frame, trace_t *trace, const vec3_t boxstartmins, const vec3_t boxstartmaxs, const vec3_t boxendmins, const vec3_t boxendmaxs, int hitsupercontentsmask)
{
	int i, linetrace;
	float segmentmins[3], segmentmaxs[3];
	frameblend_t frameblend[4];
	msurface_t *surface;
	surfmesh_t *mesh;
	colbrushf_t *thisbrush_start = NULL, *thisbrush_end = NULL;
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
	linetrace = VectorCompare(boxstartmins, boxstartmaxs) && VectorCompare(boxendmins, boxendmaxs);
	if (!linetrace)
	{
		// box trace, performed as brush trace
		Matrix4x4_CreateIdentity(&startmatrix);
		Matrix4x4_CreateIdentity(&endmatrix);
		thisbrush_start = Collision_BrushForBox(&startmatrix, boxstartmins, boxstartmaxs);
		thisbrush_end = Collision_BrushForBox(&endmatrix, boxendmins, boxendmaxs);
	}
	memset(frameblend, 0, sizeof(frameblend));
	frameblend[0].frame = frame;
	frameblend[0].lerp = 1;
	for (i = 0, surface = model->data_surfaces;i < model->num_surfaces;i++, surface++)
	{
		mesh = surface->groupmesh;
		Mod_Alias_GetMesh_Vertex3f(model, frameblend, mesh, varray_vertex3f);
		if (linetrace)
			Collision_TraceLineTriangleMeshFloat(trace, boxstartmins, boxendmins, mesh->num_triangles, mesh->data_element3i, varray_vertex3f, SUPERCONTENTS_SOLID, segmentmins, segmentmaxs);
		else
			Collision_TraceBrushTriangleMeshFloat(trace, thisbrush_start, thisbrush_end, mesh->num_triangles, mesh->data_element3i, varray_vertex3f, SUPERCONTENTS_SOLID, segmentmins, segmentmaxs);
	}
}

static void Mod_CalcAliasModelBBoxes (void)
{
	int vnum, meshnum;
	float dist, yawradius, radius;
	surfmesh_t *mesh;
	float *v;
	VectorClear(loadmodel->normalmins);
	VectorClear(loadmodel->normalmaxs);
	yawradius = 0;
	radius = 0;
	for (meshnum = 0;meshnum < loadmodel->num_surfaces;meshnum++)
	{
		mesh = loadmodel->meshlist[meshnum];
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

static void Mod_MDL_LoadFrames (unsigned char* datapointer, int inverts, vec3_t scale, vec3_t translate, int *vertremap)
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
			Mod_ConvertAliasVerts(inverts, scale, translate, (trivertx_t *)datapointer, loadmodel->meshlist[0]->data_morphvertex3f + pose * loadmodel->meshlist[0]->num_vertices * 3, vertremap);
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
	skin->currentframe = skin;
	skin->basematerialflags = MATERIALFLAG_WALL;
	if (skin->skin.fog)
		skin->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_TRANSPARENT;
	skin->currentmaterialflags = skin->basematerialflags;
}

static void Mod_BuildAliasSkinsFromSkinFiles(texture_t *skin, skinfile_t *skinfile, char *meshname, char *shadername)
{
	int i;
	skinfileitem_t *skinfileitem;
	skinframe_t tempskinframe;
	if (skinfile)
	{
		// the skin += loadmodel->num_surfaces part of this is because data_textures on alias models is arranged as [numskins][numsurfaces]
		for (i = 0;skinfile;skinfile = skinfile->next, i++, skin += loadmodel->num_surfaces)
		{
			memset(skin, 0, sizeof(*skin));
			for (skinfileitem = skinfile->items;skinfileitem;skinfileitem = skinfileitem->next)
			{
				// leave the skin unitialized (nodraw) if the replacement is "common/nodraw" or "textures/common/nodraw"
				if (!strcmp(skinfileitem->name, meshname) && strcmp(skinfileitem->replacement, "common/nodraw") && strcmp(skinfileitem->replacement, "textures/common/nodraw"))
				{
					memset(&tempskinframe, 0, sizeof(tempskinframe));
					if (Mod_LoadSkinFrame(&tempskinframe, skinfileitem->replacement, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, true, true))
						Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
					else
					{
						if (cls.state != ca_dedicated)
							Con_Printf("mesh \"%s\": failed to load skin #%i \"%s\", falling back to mesh's internal shader name \"%s\"\n", meshname, i, skinfileitem->replacement, shadername);
						if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, true, true))
							Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
						else
						{
							if (cls.state != ca_dedicated)
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
		if (Mod_LoadSkinFrame(&tempskinframe, shadername, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, true, true))
			Mod_BuildAliasSkinFromSkinFrame(skin, &tempskinframe);
		else
		{
			if (cls.state != ca_dedicated)
				Con_Printf("Can't find texture \"%s\" for mesh \"%s\", using grey checkerboard\n", shadername, meshname);
			Mod_BuildAliasSkinFromSkinFrame(skin, NULL);
		}
	}
}

#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%d exceeds %d - %d)\n", loadmodel->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%f exceeds %f - %f)\n", loadmodel->name, VALUE, MIN, MAX);
void Mod_IDP0_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, version, totalskins, skinwidth, skinheight, groupframes, groupskins, numverts;
	float scales, scalet, scale[3], translate[3], interval;
	msurface_t *surface;
	unsigned char *data;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasskintype_t *pinskintype;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;
	daliasframetype_t *pinframetype;
	daliasgroup_t *pinframegroup;
	unsigned char *datapointer, *startframes, *startskins;
	char name[MAX_QPATH];
	skinframe_t tempskinframe;
	animscene_t *tempskinscenes;
	texture_t *tempaliasskins;
	float *vertst;
	int *vertonseam, *vertremap;
	skinfile_t *skinfiles;

	datapointer = (unsigned char *)buffer;
	pinmodel = (mdl_t *)datapointer;
	datapointer += sizeof(mdl_t);

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 loadmodel->name, version, ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;

	loadmodel->num_surfaces = 1;
	loadmodel->nummodelsurfaces = loadmodel->num_surfaces;
	loadmodel->nummeshes = loadmodel->num_surfaces;
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t) + loadmodel->num_surfaces * sizeof(int) + loadmodel->nummeshes * sizeof(surfmesh_t *) + loadmodel->nummeshes * sizeof(surfmesh_t));
	loadmodel->data_surfaces = (msurface_t *)data;data += loadmodel->num_surfaces * sizeof(msurface_t);
	loadmodel->surfacelist = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->meshlist = (surfmesh_t **)data;data += loadmodel->num_surfaces * sizeof(surfmesh_t *);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		loadmodel->surfacelist[i] = i;
		loadmodel->meshlist[i] = (surfmesh_t *)data;data += sizeof(surfmesh_t);
	}

	loadmodel->numskins = LittleLong(pinmodel->numskins);
	BOUNDI(loadmodel->numskins,0,65536);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,65536);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,65536);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,65536);
	loadmodel->meshlist[0]->num_triangles = LittleLong(pinmodel->numtris);
	BOUNDI(loadmodel->meshlist[0]->num_triangles,0,65536);
	loadmodel->numframes = LittleLong(pinmodel->numframes);
	BOUNDI(loadmodel->numframes,0,65536);
	loadmodel->synctype = (synctype_t)LittleLong (pinmodel->synctype);
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
	datapointer += sizeof(dtriangle_t) * loadmodel->meshlist[0]->num_triangles;

	startframes = datapointer;
	loadmodel->meshlist[0]->num_morphframes = 0;
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
			loadmodel->meshlist[0]->num_morphframes++;
		}
	}

	// store texture coordinates into temporary array, they will be stored
	// after usage is determined (triangle data)
	vertst = (float *)Mem_Alloc(tempmempool, numverts * 2 * sizeof(float[2]));
	vertremap = (int *)Mem_Alloc(tempmempool, numverts * 3 * sizeof(int));
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
	loadmodel->meshlist[0]->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * loadmodel->meshlist[0]->num_triangles);

	// read the triangle elements
	for (i = 0;i < loadmodel->meshlist[0]->num_triangles;i++)
		for (j = 0;j < 3;j++)
			loadmodel->meshlist[0]->data_element3i[i*3+j] = LittleLong(pintriangles[i].vertindex[j]);
	// validate (note numverts is used because this is the original data)
	Mod_ValidateElements(loadmodel->meshlist[0]->data_element3i, loadmodel->meshlist[0]->num_triangles, numverts, __FILE__, __LINE__);
	// now butcher the elements according to vertonseam and tri->facesfront
	// and then compact the vertex set to remove duplicates
	for (i = 0;i < loadmodel->meshlist[0]->num_triangles;i++)
		if (!LittleLong(pintriangles[i].facesfront)) // backface
			for (j = 0;j < 3;j++)
				if (vertonseam[loadmodel->meshlist[0]->data_element3i[i*3+j]])
					loadmodel->meshlist[0]->data_element3i[i*3+j] += numverts;
	// count the usage
	// (this uses vertremap to count usage to save some memory)
	for (i = 0;i < numverts*2;i++)
		vertremap[i] = 0;
	for (i = 0;i < loadmodel->meshlist[0]->num_triangles*3;i++)
		vertremap[loadmodel->meshlist[0]->data_element3i[i]]++;
	// build remapping table and compact array
	loadmodel->meshlist[0]->num_vertices = 0;
	for (i = 0;i < numverts*2;i++)
	{
		if (vertremap[i])
		{
			vertremap[i] = loadmodel->meshlist[0]->num_vertices;
			vertst[loadmodel->meshlist[0]->num_vertices*2+0] = vertst[i*2+0];
			vertst[loadmodel->meshlist[0]->num_vertices*2+1] = vertst[i*2+1];
			loadmodel->meshlist[0]->num_vertices++;
		}
		else
			vertremap[i] = -1; // not used at all
	}
	// remap the elements to the new vertex set
	for (i = 0;i < loadmodel->meshlist[0]->num_triangles * 3;i++)
		loadmodel->meshlist[0]->data_element3i[i] = vertremap[loadmodel->meshlist[0]->data_element3i[i]];
	// store the texture coordinates
	loadmodel->meshlist[0]->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, sizeof(float[2]) * loadmodel->meshlist[0]->num_vertices);
	for (i = 0;i < loadmodel->meshlist[0]->num_vertices;i++)
	{
		loadmodel->meshlist[0]->data_texcoordtexture2f[i*2+0] = vertst[i*2+0];
		loadmodel->meshlist[0]->data_texcoordtexture2f[i*2+1] = vertst[i*2+1];
	}

// load the frames
	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->meshlist[0]->data_morphvertex3f = (float *)Mem_Alloc(loadmodel->mempool, sizeof(float[3]) * loadmodel->meshlist[0]->num_morphframes * loadmodel->meshlist[0]->num_vertices);
	loadmodel->meshlist[0]->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->meshlist[0]->num_triangles * sizeof(int[3]));
	Mod_MDL_LoadFrames (startframes, numverts, scale, translate, vertremap);
	Mod_BuildTriangleNeighbors(loadmodel->meshlist[0]->data_neighbor3i, loadmodel->meshlist[0]->data_element3i, loadmodel->meshlist[0]->num_triangles);
	Mod_CalcAliasModelBBoxes();
	Mod_Alias_Mesh_CompileFrameZero(loadmodel->meshlist[0]);

	Mem_Free(vertst);
	Mem_Free(vertremap);

	// load the skins
	skinfiles = Mod_LoadSkinFiles();
	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, loadmodel->numskins * sizeof(animscene_t));
	loadmodel->num_textures = loadmodel->num_surfaces;
	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * totalskins * sizeof(texture_t));
	if (skinfiles)
	{
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures, skinfiles, "default", "");
		Mod_FreeSkinFiles(skinfiles);
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
				if (!Mod_LoadSkinFrame(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PICMIP, true, true))
					Mod_LoadSkinFrame_Internal(&tempskinframe, name, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PICMIP, true, r_fullbrights.integer, (unsigned char *)datapointer, skinwidth, skinheight);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + totalskins * loadmodel->num_surfaces, &tempskinframe);
				datapointer += skinwidth * skinheight;
				totalskins++;
			}
		}
		// check for skins that don't exist in the model, but do exist as external images
		// (this was added because yummyluv kept pestering me about support for it)
		while (Mod_LoadSkinFrame(&tempskinframe, va("%s_%i", loadmodel->name, loadmodel->numskins), (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PICMIP, true, true))
		{
			// expand the arrays to make room
			tempskinscenes = loadmodel->skinscenes;
			loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, (loadmodel->numskins + 1) * sizeof(animscene_t));
			memcpy(loadmodel->skinscenes, tempskinscenes, loadmodel->numskins * sizeof(animscene_t));
			Mem_Free(tempskinscenes);

			tempaliasskins = loadmodel->data_textures;
			loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * (totalskins + 1) * sizeof(texture_t));
			memcpy(loadmodel->data_textures, tempaliasskins, loadmodel->num_surfaces * totalskins * sizeof(texture_t));
			Mem_Free(tempaliasskins);

			// store the info about the new skin
			Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + totalskins * loadmodel->num_surfaces, &tempskinframe);
			strcpy(loadmodel->skinscenes[loadmodel->numskins].name, name);
			loadmodel->skinscenes[loadmodel->numskins].firstframe = totalskins;
			loadmodel->skinscenes[loadmodel->numskins].framecount = 1;
			loadmodel->skinscenes[loadmodel->numskins].framerate = 10.0f;
			loadmodel->skinscenes[loadmodel->numskins].loop = true;

			//increase skin counts
			loadmodel->numskins++;
			totalskins++;
		}
	}

	surface = loadmodel->data_surfaces;
	surface->texture = loadmodel->data_textures;
	surface->groupmesh = loadmodel->meshlist[0];
	surface->num_firsttriangle = 0;
	surface->num_triangles = surface->groupmesh->num_triangles;
	surface->num_firstvertex = 0;
	surface->num_vertices = surface->groupmesh->num_vertices;
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

void Mod_IDP2_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, k, hashindex, num, numxyz, numst, xyz, st, skinwidth, skinheight, *vertremap, version, end, numverts;
	float *stverts, s, t, scale[3], translate[3];
	unsigned char *data;
	msurface_t *surface;
	md2_t *pinmodel;
	unsigned char *base, *datapointer;
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

	pinmodel = (md2_t *)buffer;
	base = (unsigned char *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD2ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
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

	loadmodel->num_surfaces = 1;
	loadmodel->nummodelsurfaces = loadmodel->num_surfaces;
	loadmodel->nummeshes = loadmodel->num_surfaces;
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t) + loadmodel->num_surfaces * sizeof(int) + loadmodel->nummeshes * sizeof(surfmesh_t *) + loadmodel->nummeshes * sizeof(surfmesh_t));
	loadmodel->data_surfaces = (msurface_t *)data;data += loadmodel->num_surfaces * sizeof(msurface_t);
	loadmodel->surfacelist = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->meshlist = (surfmesh_t **)data;data += loadmodel->num_surfaces * sizeof(surfmesh_t *);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		loadmodel->surfacelist[i] = i;
		loadmodel->meshlist[i] = (surfmesh_t *)data;data += sizeof(surfmesh_t);
	}

	loadmodel->numskins = LittleLong(pinmodel->num_skins);
	numxyz = LittleLong(pinmodel->num_xyz);
	numst = LittleLong(pinmodel->num_st);
	loadmodel->meshlist[0]->num_triangles = LittleLong(pinmodel->num_tris);
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->meshlist[0]->num_morphframes = loadmodel->numframes;
	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));

	loadmodel->flags = 0; // there are no MD2 flags
	loadmodel->synctype = ST_RAND;

	// load the skins
	inskin = (char *)(base + LittleLong(pinmodel->ofs_skins));
	skinfiles = Mod_LoadSkinFiles();
	if (skinfiles)
	{
		loadmodel->num_textures = loadmodel->num_surfaces;
		loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
		Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures, skinfiles, "default", "");
		Mod_FreeSkinFiles(skinfiles);
	}
	else if (loadmodel->numskins)
	{
		// skins found (most likely not a player model)
		loadmodel->num_textures = loadmodel->num_surfaces;
		loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
		for (i = 0;i < loadmodel->numskins;i++, inskin += MD2_SKINNAME)
		{
			if (Mod_LoadSkinFrame(&tempskinframe, inskin, (r_mipskins.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_PRECACHE | TEXF_PICMIP, true, true))
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + i * loadmodel->num_surfaces, &tempskinframe);
			else
			{
				Con_Printf("%s is missing skin \"%s\"\n", loadmodel->name, inskin);
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + i * loadmodel->num_surfaces, NULL);
			}
		}
	}
	else
	{
		// no skins (most likely a player model)
		loadmodel->numskins = 1;
		loadmodel->num_textures = loadmodel->num_surfaces;
		loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
		Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures, NULL);
	}

	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

	// load the triangles and stvert data
	inst = (unsigned short *)(base + LittleLong(pinmodel->ofs_st));
	intri = (md2triangle_t *)(base + LittleLong(pinmodel->ofs_tris));
	skinwidth = LittleLong(pinmodel->skinwidth);
	skinheight = LittleLong(pinmodel->skinheight);

	stverts = (float *)Mem_Alloc(tempmempool, numst * sizeof(float[2]));
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

	md2verthash = (struct md2verthash_s **)Mem_Alloc(tempmempool, 256 * sizeof(hash));
	md2verthashdata = (struct md2verthash_s *)Mem_Alloc(tempmempool, loadmodel->meshlist[0]->num_triangles * 3 * sizeof(*hash));
	// swap the triangle list
	num = 0;
	loadmodel->meshlist[0]->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->meshlist[0]->num_triangles * sizeof(int[3]));
	for (i = 0;i < loadmodel->meshlist[0]->num_triangles;i++)
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
			loadmodel->meshlist[0]->data_element3i[i*3+j] = (hash - md2verthashdata);
		}
	}

	Mem_Free(stverts);

	numverts = num;
	loadmodel->meshlist[0]->num_vertices = numverts;
	vertremap = (int *)Mem_Alloc(loadmodel->mempool, num * sizeof(int));
	loadmodel->meshlist[0]->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, num * sizeof(float[2]));
	for (i = 0;i < num;i++)
	{
		hash = md2verthashdata + i;
		vertremap[i] = hash->xyz;
		loadmodel->meshlist[0]->data_texcoordtexture2f[i*2+0] = hash->st[0];
		loadmodel->meshlist[0]->data_texcoordtexture2f[i*2+1] = hash->st[1];
	}

	Mem_Free(md2verthash);
	Mem_Free(md2verthashdata);

	// load the frames
	datapointer = (base + LittleLong(pinmodel->ofs_frames));
	loadmodel->meshlist[0]->data_morphvertex3f = (float *)Mem_Alloc(loadmodel->mempool, numverts * loadmodel->meshlist[0]->num_morphframes * sizeof(float[3]));
	for (i = 0;i < loadmodel->meshlist[0]->num_morphframes;i++)
	{
		pinframe = (md2frame_t *)datapointer;
		datapointer += sizeof(md2frame_t);
		for (j = 0;j < 3;j++)
		{
			scale[j] = LittleFloat(pinframe->scale[j]);
			translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_MD2_ConvertVerts(scale, translate, (trivertx_t *)datapointer, loadmodel->meshlist[0]->data_morphvertex3f + i * numverts * 3, numverts, vertremap);
		datapointer += numxyz * sizeof(trivertx_t);

		strcpy(loadmodel->animscenes[i].name, pinframe->name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}

	Mem_Free(vertremap);

	loadmodel->meshlist[0]->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->meshlist[0]->num_triangles * sizeof(int[3]));
	Mod_BuildTriangleNeighbors(loadmodel->meshlist[0]->data_neighbor3i, loadmodel->meshlist[0]->data_element3i, loadmodel->meshlist[0]->num_triangles);
	Mod_CalcAliasModelBBoxes();
	Mod_Alias_Mesh_CompileFrameZero(loadmodel->meshlist[0]);

	surface = loadmodel->data_surfaces;
	surface->groupmesh = loadmodel->meshlist[0];
	surface->texture = loadmodel->data_textures;
	surface->num_firsttriangle = 0;
	surface->num_triangles = surface->groupmesh->num_triangles;
	surface->num_firstvertex = 0;
	surface->num_vertices = surface->groupmesh->num_vertices;
}

void Mod_IDP3_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, k, version;
	unsigned char *data;
	msurface_t *surface;
	surfmesh_t *mesh;
	md3modelheader_t *pinmodel;
	md3frameinfo_t *pinframe;
	md3mesh_t *pinmesh;
	md3tag_t *pintag;
	skinfile_t *skinfiles;

	pinmodel = (md3modelheader_t *)buffer;

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
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;
	loadmodel->flags = LittleLong(pinmodel->flags);
	loadmodel->synctype = ST_RAND;

	// set up some global info about the model
	loadmodel->numframes = LittleLong(pinmodel->num_frames);
	loadmodel->num_surfaces = LittleLong(pinmodel->num_meshes);

	// make skinscenes for the skins (no groups)
	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

	// load frameinfo
	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));
	for (i = 0, pinframe = (md3frameinfo_t *)((unsigned char *)pinmodel + LittleLong(pinmodel->lump_frameinfo));i < loadmodel->numframes;i++, pinframe++)
	{
		strcpy(loadmodel->animscenes[i].name, pinframe->name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}

	// load tags
	loadmodel->num_tagframes = loadmodel->numframes;
	loadmodel->num_tags = LittleLong(pinmodel->num_tags);
	loadmodel->data_tags = (aliastag_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_tagframes * loadmodel->num_tags * sizeof(aliastag_t));
	for (i = 0, pintag = (md3tag_t *)((unsigned char *)pinmodel + LittleLong(pinmodel->lump_tags));i < loadmodel->num_tagframes * loadmodel->num_tags;i++, pintag++)
	{
		strcpy(loadmodel->data_tags[i].name, pintag->name);
		Matrix4x4_CreateIdentity(&loadmodel->data_tags[i].matrix);
		for (j = 0;j < 3;j++)
		{
			for (k = 0;k < 3;k++)
				loadmodel->data_tags[i].matrix.m[j][k] = LittleFloat(pintag->rotationmatrix[k * 3 + j]);
			loadmodel->data_tags[i].matrix.m[j][3] = LittleFloat(pintag->origin[j]);
		}
		//Con_Printf("model \"%s\" frame #%i tag #%i \"%s\"\n", loadmodel->name, i / loadmodel->num_tags, i % loadmodel->num_tags, loadmodel->data_tags[i].name);
	}

	// load meshes
	loadmodel->nummodelsurfaces = loadmodel->num_surfaces;
	loadmodel->nummeshes = loadmodel->num_surfaces;
	loadmodel->num_textures = loadmodel->num_surfaces;
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t) + loadmodel->num_surfaces * sizeof(int) + loadmodel->nummeshes * sizeof(surfmesh_t *) + loadmodel->nummeshes * sizeof(surfmesh_t) + loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
	loadmodel->data_surfaces = (msurface_t *)data;data += loadmodel->num_surfaces * sizeof(msurface_t);
	loadmodel->surfacelist = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->meshlist = (surfmesh_t **)data;data += loadmodel->num_surfaces * sizeof(surfmesh_t *);
	loadmodel->data_textures = (texture_t *)data;data += loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		loadmodel->surfacelist[i] = i;
		loadmodel->meshlist[i] = (surfmesh_t *)data;data += sizeof(surfmesh_t);
	}
	for (i = 0, pinmesh = (md3mesh_t *)((unsigned char *)pinmodel + LittleLong(pinmodel->lump_meshes));i < loadmodel->num_surfaces;i++, pinmesh = (md3mesh_t *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_end)))
	{
		if (memcmp(pinmesh->identifier, "IDP3", 4))
			Host_Error("Mod_IDP3_Load: invalid mesh identifier (not IDP3)\n");
		mesh = loadmodel->meshlist[i];
		mesh->num_morphframes = LittleLong(pinmesh->num_frames);
		mesh->num_vertices = LittleLong(pinmesh->num_vertices);
		mesh->num_triangles = LittleLong(pinmesh->num_triangles);
		mesh->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		mesh->data_morphvertex3f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * mesh->num_morphframes * sizeof(float[3]));
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = LittleLong(((int *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_elements)))[j]);
		for (j = 0;j < mesh->num_vertices;j++)
		{
			mesh->data_texcoordtexture2f[j * 2 + 0] = LittleFloat(((float *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_texcoords)))[j * 2 + 0]);
			mesh->data_texcoordtexture2f[j * 2 + 1] = LittleFloat(((float *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_texcoords)))[j * 2 + 1]);
		}
		for (j = 0;j < mesh->num_vertices * mesh->num_morphframes;j++)
		{
			mesh->data_morphvertex3f[j * 3 + 0] = LittleShort(((short *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 0]) * (1.0f / 64.0f);
			mesh->data_morphvertex3f[j * 3 + 1] = LittleShort(((short *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 1]) * (1.0f / 64.0f);
			mesh->data_morphvertex3f[j * 3 + 2] = LittleShort(((short *)((unsigned char *)pinmesh + LittleLong(pinmesh->lump_framevertices)))[j * 4 + 2]) * (1.0f / 64.0f);
		}

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_Alias_Mesh_CompileFrameZero(mesh);

		if (LittleLong(pinmesh->num_shaders) >= 1)
			Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures + i, skinfiles, pinmesh->name, ((md3shader_t *)((unsigned char *) pinmesh + LittleLong(pinmesh->lump_shaders)))->name);
		else
			for (j = 0;j < loadmodel->numskins;j++)
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + i + j * loadmodel->num_surfaces, NULL);

		surface = loadmodel->data_surfaces + i;
		surface->groupmesh = mesh;
		surface->texture = loadmodel->data_textures + i;
		surface->num_firsttriangle = 0;
		surface->num_triangles = mesh->num_triangles;
		surface->num_firstvertex = 0;
		surface->num_vertices = mesh->num_vertices;
	}
	Mod_CalcAliasModelBBoxes();
	Mod_FreeSkinFiles(skinfiles);
}

void Mod_ZYMOTICMODEL_Load(model_t *mod, void *buffer, void *bufferend)
{
	zymtype1header_t *pinmodel, *pheader;
	unsigned char *pbase;
	int i, j, k, l, numposes, *bonecount, *vertbonecounts, count, *renderlist, *renderlistend, *outelements, *remapvertices;
	float modelradius, corner[2], *poses, *intexcoord2f, *outtexcoord2f;
	zymvertex_t *verts, *vertdata;
	zymscene_t *scene;
	zymbone_t *bone;
	char *shadername;
	skinfile_t *skinfiles;
	unsigned char *data;
	msurface_t *surface;
	surfmesh_t *mesh;

	pinmodel = (zymtype1header_t *)buffer;
	pbase = (unsigned char *)buffer;
	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_ZYMOTICMODEL_Load: %s is not a zymotic model\n");
	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_ZYMOTICMODEL_Load: only type 1 (skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;
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
	loadmodel->num_surfaces = pheader->numshaders;

	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;

	// make skinscenes for the skins (no groups)
	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
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
	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	scene = (zymscene_t *) (pheader->lump_scenes.start + pbase);
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
	loadmodel->num_poses = pheader->lump_poses.length / sizeof(float[3][4]);
	loadmodel->data_poses = (float *)Mem_Alloc(loadmodel->mempool, pheader->lump_poses.length);
	poses = (float *) (pheader->lump_poses.start + pbase);
	for (i = 0;i < pheader->lump_poses.length / 4;i++)
		loadmodel->data_poses[i] = BigFloat(poses[i]);

	//zymlump_t lump_bones; // zymbone_t bone[numbones];
	loadmodel->num_bones = pheader->numbones;
	loadmodel->data_bones = (aliasbone_t *)Mem_Alloc(loadmodel->mempool, pheader->numbones * sizeof(aliasbone_t));
	bone = (zymbone_t *) (pheader->lump_bones.start + pbase);
	for (i = 0;i < pheader->numbones;i++)
	{
		memcpy(loadmodel->data_bones[i].name, bone[i].name, sizeof(bone[i].name));
		loadmodel->data_bones[i].flags = BigLong(bone[i].flags);
		loadmodel->data_bones[i].parent = BigLong(bone[i].parent);
		if (loadmodel->data_bones[i].parent >= i)
			Host_Error("%s bone[%i].parent >= %i\n", loadmodel->name, i, i);
	}

	//zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	vertbonecounts = (int *)Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
	bonecount = (int *) (pheader->lump_vertbonecounts.start + pbase);
	for (i = 0;i < pheader->numverts;i++)
	{
		vertbonecounts[i] = BigLong(bonecount[i]);
		if (vertbonecounts[i] < 1)
			Host_Error("%s bonecount[%i] < 1\n", loadmodel->name, i);
	}

	//zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	verts = (zymvertex_t *)Mem_Alloc(loadmodel->mempool, pheader->lump_verts.length);
	vertdata = (zymvertex_t *) (pheader->lump_verts.start + pbase);
	for (i = 0;i < pheader->lump_verts.length / (int) sizeof(zymvertex_t);i++)
	{
		verts[i].bonenum = BigLong(vertdata[i].bonenum);
		verts[i].origin[0] = BigFloat(vertdata[i].origin[0]);
		verts[i].origin[1] = BigFloat(vertdata[i].origin[1]);
		verts[i].origin[2] = BigFloat(vertdata[i].origin[2]);
	}

	//zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	outtexcoord2f = (float *)Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(float[2]));
	intexcoord2f = (float *) (pheader->lump_texcoords.start + pbase);
	for (i = 0;i < pheader->numverts;i++)
	{
		outtexcoord2f[i*2+0] = BigFloat(intexcoord2f[i*2+0]);
		// flip T coordinate for OpenGL
		outtexcoord2f[i*2+1] = 1 - BigFloat(intexcoord2f[i*2+1]);
	}

	//zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
	//loadmodel->alias.zymdata_trizone = Mem_Alloc(loadmodel->mempool, pheader->numtris);
	//memcpy(loadmodel->alias.zymdata_trizone, (void *) (pheader->lump_trizone.start + pbase), pheader->numtris);

	loadmodel->nummodelsurfaces = loadmodel->num_surfaces;
	loadmodel->nummeshes = loadmodel->num_surfaces;
	loadmodel->num_textures = loadmodel->num_surfaces;
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t) + loadmodel->num_surfaces * sizeof(int) + loadmodel->nummeshes * sizeof(surfmesh_t *) + loadmodel->nummeshes * sizeof(surfmesh_t) + loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
	loadmodel->data_surfaces = (msurface_t *)data;data += loadmodel->num_surfaces * sizeof(msurface_t);
	loadmodel->surfacelist = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->meshlist = (surfmesh_t **)data;data += loadmodel->num_surfaces * sizeof(surfmesh_t *);
	loadmodel->data_textures = (texture_t *)data;data += loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		loadmodel->surfacelist[i] = i;
		loadmodel->meshlist[i] = (surfmesh_t *)data;data += sizeof(surfmesh_t);
	}

	//zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
	//zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	// byteswap, validate, and swap winding order of tris
	count = pheader->numshaders * sizeof(int) + pheader->numtris * sizeof(int[3]);
	if (pheader->lump_render.length != count)
		Host_Error("%s renderlist is wrong size (%i bytes, should be %i bytes)\n", loadmodel->name, pheader->lump_render.length, count);
	renderlist = (int *) (pheader->lump_render.start + pbase);
	renderlistend = (int *) ((unsigned char *) renderlist + pheader->lump_render.length);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		if (renderlist >= renderlistend)
			Host_Error("%s corrupt renderlist (wrong size)\n", loadmodel->name);
		count = BigLong(*renderlist);renderlist++;
		if (renderlist + count * 3 > renderlistend || (i == pheader->numshaders - 1 && renderlist + count * 3 != renderlistend))
			Host_Error("%s corrupt renderlist (wrong size)\n", loadmodel->name);
		mesh = loadmodel->meshlist[i];
		mesh->num_triangles = count;
		mesh->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
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
			if (vertbonecounts[outelements[0]] == 0 || vertbonecounts[outelements[1]] == 0 || vertbonecounts[outelements[2]] == 0)
				Host_Error("%s corrupt renderlist (references vertex with no bone weights\n", loadmodel->name);
			renderlist += 3;
			outelements += 3;
		}
		remapvertices = (int *)Mem_Alloc(loadmodel->mempool, pheader->numverts * sizeof(int));
		mesh->num_vertices = Mod_BuildVertexRemapTableFromElements(mesh->num_triangles * 3, mesh->data_element3i, pheader->numverts, remapvertices);
		for (j = 0;j < mesh->num_triangles * 3;j++)
			mesh->data_element3i[j] = remapvertices[mesh->data_element3i[j]];
		mesh->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));
		for (j = 0;j < pheader->numverts;j++)
		{
			if (remapvertices[j] >= 0)
			{
				mesh->data_texcoordtexture2f[remapvertices[j]*2+0] = outtexcoord2f[j*2+0];
				mesh->data_texcoordtexture2f[remapvertices[j]*2+1] = outtexcoord2f[j*2+1];
			}
		}
		mesh->num_vertexboneweights = 0;
		for (j = 0;j < pheader->numverts;j++)
			if (remapvertices[j] >= 0)
				mesh->num_vertexboneweights += vertbonecounts[remapvertices[j]];
		mesh->data_vertexboneweights = (surfmeshvertexboneweight_t *)Mem_Alloc(loadmodel->mempool, mesh->num_vertexboneweights * sizeof(surfmeshvertexboneweight_t));
		mesh->num_vertexboneweights = 0;
		// note this vertexboneweight ordering requires that the remapvertices array is sequential numbers (separated by -1 values for omitted vertices)
		l = 0;
		for (j = 0;j < pheader->numverts;j++)
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
		shadername = (char *) (pheader->lump_shaders.start + pbase) + i * 32;
		// since zym models do not have named sections, reuse their shader
		// name as the section name
		if (shadername[0])
			Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures + i, skinfiles, shadername, shadername);
		else
			for (j = 0;j < loadmodel->numskins;j++)
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + i + j * loadmodel->num_surfaces, NULL);

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_Alias_Mesh_CompileFrameZero(mesh);

		surface = loadmodel->data_surfaces + i;
		surface->groupmesh = mesh;
		surface->texture = loadmodel->data_textures + i;
		surface->num_firsttriangle = 0;
		surface->num_triangles = mesh->num_triangles;
		surface->num_firstvertex = 0;
		surface->num_vertices = mesh->num_vertices;
	}

	Mem_Free(vertbonecounts);
	Mem_Free(verts);
	Mem_Free(outtexcoord2f);
}

void Mod_DARKPLACESMODEL_Load(model_t *mod, void *buffer, void *bufferend)
{
	dpmheader_t *pheader;
	dpmframe_t *frame;
	dpmbone_t *bone;
	dpmmesh_t *dpmmesh;
	unsigned char *pbase;
	int i, j, k;
	skinfile_t *skinfiles;
	unsigned char *data;

	pheader = (dpmheader_t *)buffer;
	pbase = (unsigned char *)buffer;
	if (memcmp(pheader->id, "DARKPLACESMODEL\0", 16))
		Host_Error ("Mod_DARKPLACESMODEL_Load: %s is not a darkplaces model\n");
	if (BigLong(pheader->type) != 2)
		Host_Error ("Mod_DARKPLACESMODEL_Load: only type 2 (hierarchical skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;
	loadmodel->flags = 0; // there are no flags on zym models
	loadmodel->synctype = ST_RAND;

	// byteswap header
	pheader->type = BigLong(pheader->type);
	pheader->filesize = BigLong(pheader->filesize);
	pheader->mins[0] = BigFloat(pheader->mins[0]);
	pheader->mins[1] = BigFloat(pheader->mins[1]);
	pheader->mins[2] = BigFloat(pheader->mins[2]);
	pheader->maxs[0] = BigFloat(pheader->maxs[0]);
	pheader->maxs[1] = BigFloat(pheader->maxs[1]);
	pheader->maxs[2] = BigFloat(pheader->maxs[2]);
	pheader->yawradius = BigFloat(pheader->yawradius);
	pheader->allradius = BigFloat(pheader->allradius);
	pheader->num_bones = BigLong(pheader->num_bones);
	pheader->num_meshs = BigLong(pheader->num_meshs);
	pheader->num_frames = BigLong(pheader->num_frames);
	pheader->ofs_bones = BigLong(pheader->ofs_bones);
	pheader->ofs_meshs = BigLong(pheader->ofs_meshs);
	pheader->ofs_frames = BigLong(pheader->ofs_frames);

	// model bbox
	for (i = 0;i < 3;i++)
	{
		loadmodel->normalmins[i] = pheader->mins[i];
		loadmodel->normalmaxs[i] = pheader->maxs[i];
		loadmodel->yawmins[i] = i != 2 ? -pheader->yawradius : pheader->mins[i];
		loadmodel->yawmaxs[i] = i != 2 ? pheader->yawradius : pheader->maxs[i];
		loadmodel->rotatedmins[i] = -pheader->allradius;
		loadmodel->rotatedmaxs[i] = pheader->allradius;
	}
	loadmodel->radius = pheader->allradius;
	loadmodel->radius2 = pheader->allradius * pheader->allradius;

	// load external .skin files if present
	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;

	loadmodel->numframes = pheader->num_frames;
	loadmodel->num_bones = pheader->num_bones;
	loadmodel->num_poses = loadmodel->num_bones * loadmodel->numframes;
	loadmodel->num_textures = loadmodel->nummeshes = loadmodel->nummodelsurfaces = loadmodel->num_surfaces = pheader->num_meshs;

	// do most allocations as one merged chunk
	data = (unsigned char *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t) + loadmodel->num_surfaces * sizeof(int) + loadmodel->nummeshes * sizeof(surfmesh_t *) + loadmodel->nummeshes * sizeof(surfmesh_t) + loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t) + loadmodel->numskins * sizeof(animscene_t) + loadmodel->num_bones * sizeof(aliasbone_t) + loadmodel->num_poses * sizeof(float[12]) + loadmodel->numframes * sizeof(animscene_t));
	loadmodel->data_surfaces = (msurface_t *)data;data += loadmodel->num_surfaces * sizeof(msurface_t);
	loadmodel->surfacelist = (int *)data;data += loadmodel->num_surfaces * sizeof(int);
	loadmodel->meshlist = (surfmesh_t **)data;data += loadmodel->num_surfaces * sizeof(surfmesh_t *);
	loadmodel->data_textures = (texture_t *)data;data += loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t);
	loadmodel->skinscenes = (animscene_t *)data;data += loadmodel->numskins * sizeof(animscene_t);
	loadmodel->data_bones = (aliasbone_t *)data;data += loadmodel->num_bones * sizeof(aliasbone_t);
	loadmodel->data_poses = (float *)data;data += loadmodel->num_poses * sizeof(float[12]);
	loadmodel->animscenes = (animscene_t *)data;data += loadmodel->numframes * sizeof(animscene_t);
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		loadmodel->surfacelist[i] = i;
		loadmodel->meshlist[i] = (surfmesh_t *)data;data += sizeof(surfmesh_t);
	}

	// load the bone info
	bone = (dpmbone_t *) (pbase + pheader->ofs_bones);
	for (i = 0;i < loadmodel->num_bones;i++)
	{
		memcpy(loadmodel->data_bones[i].name, bone[i].name, sizeof(bone[i].name));
		loadmodel->data_bones[i].flags = BigLong(bone[i].flags);
		loadmodel->data_bones[i].parent = BigLong(bone[i].parent);
		if (loadmodel->data_bones[i].parent >= i)
			Host_Error("%s bone[%i].parent >= %i\n", loadmodel->name, i, i);
	}

	// load the frames
	frame = (dpmframe_t *) (pbase + pheader->ofs_frames);
	for (i = 0;i < loadmodel->numframes;i++)
	{
		const float *poses;
		memcpy(loadmodel->animscenes[i].name, frame->name, sizeof(frame->name));
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].loop = true;
		loadmodel->animscenes[i].framerate = 10;
		// load the bone poses for this frame
		poses = (float *) (pbase + BigLong(frame->ofs_bonepositions));
		for (j = 0;j < loadmodel->num_bones*12;j++)
			loadmodel->data_poses[i * loadmodel->num_bones*12 + j] = BigFloat(poses[j]);
		// stuff not processed here: mins, maxs, yawradius, allradius
		frame++;
	}

	// load the meshes now
	dpmmesh = (dpmmesh_t *) (pbase + pheader->ofs_meshs);
	for (i = 0;i < loadmodel->num_surfaces;i++)
	{
		const int *inelements;
		int *outelements;
		const float *intexcoord;
		surfmesh_t *mesh;
		msurface_t *surface;

		mesh = loadmodel->meshlist[i];
		mesh->num_triangles = BigLong(dpmmesh->num_tris);
		mesh->num_vertices = BigLong(dpmmesh->num_verts);

		// to find out how many weights exist we two a two-stage load...
		mesh->num_vertexboneweights = 0;
		data = (unsigned char *) (pbase + BigLong(dpmmesh->ofs_verts));
		for (j = 0;j < mesh->num_vertices;j++)
		{
			int numweights = BigLong(((dpmvertex_t *)data)->numbones);
			mesh->num_vertexboneweights += numweights;
			data += sizeof(dpmvertex_t);
			data += numweights * sizeof(dpmbonevert_t);
		}

		// allocate things now that we know how many
		mesh->data_vertexboneweights = (surfmeshvertexboneweight_t *)Mem_Alloc(loadmodel->mempool, mesh->num_vertexboneweights * sizeof(surfmeshvertexboneweight_t));
		mesh->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
		mesh->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));

		inelements = (int *) (pbase + BigLong(dpmmesh->ofs_indices));
		outelements = mesh->data_element3i;
		for (j = 0;j < mesh->num_triangles;j++)
		{
			// swap element order to flip triangles, because Quake uses clockwise (rare) and dpm uses counterclockwise (standard)
			outelements[0] = BigLong(inelements[2]);
			outelements[1] = BigLong(inelements[1]);
			outelements[2] = BigLong(inelements[0]);
			inelements += 3;
			outelements += 3;
		}

		intexcoord = (float *) (pbase + BigLong(dpmmesh->ofs_texcoords));
		for (j = 0;j < mesh->num_vertices*2;j++)
			mesh->data_texcoordtexture2f[j] = BigFloat(intexcoord[j]);

		// now load them for real
		mesh->num_vertexboneweights = 0;
		data = (unsigned char *) (pbase + BigLong(dpmmesh->ofs_verts));
		for (j = 0;j < mesh->num_vertices;j++)
		{
			int numweights = BigLong(((dpmvertex_t *)data)->numbones);
			data += sizeof(dpmvertex_t);
			for (k = 0;k < numweights;k++)
			{
				const dpmbonevert_t *vert = (dpmbonevert_t *) data;
				// stuff not processed here: normal
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].vertexindex = j;
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].boneindex = BigLong(vert->bonenum);
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[0] = BigFloat(vert->origin[0]);
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[1] = BigFloat(vert->origin[1]);
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[2] = BigFloat(vert->origin[2]);
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin[3] = BigFloat(vert->influence);
				mesh->num_vertexboneweights++;
				data += sizeof(dpmbonevert_t);
			}
		}

		// since dpm models do not have named sections, reuse their shader name as the section name
		if (dpmmesh->shadername[0])
			Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures + i, skinfiles, dpmmesh->shadername, dpmmesh->shadername);
		else
			for (j = 0;j < loadmodel->numskins;j++)
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + i + j * loadmodel->num_surfaces, NULL);

		Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
		Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
		Mod_Alias_Mesh_CompileFrameZero(mesh);

		surface = loadmodel->data_surfaces + i;
		surface->groupmesh = mesh;
		surface->texture = loadmodel->data_textures + i;
		surface->num_firsttriangle = 0;
		surface->num_triangles = mesh->num_triangles;
		surface->num_firstvertex = 0;
		surface->num_vertices = mesh->num_vertices;

		dpmmesh++;
	}
}

static void Mod_PSKMODEL_AnimKeyToMatrix(float *origin, float *quat, matrix4x4_t *m)
{
	float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
	m->m[0][0]=1-2*(y*y+z*z);m->m[0][1]=  2*(x*y-z*w);m->m[0][2]=  2*(x*z+y*w);m->m[0][3]=origin[0];
	m->m[1][0]=  2*(x*y+z*w);m->m[1][1]=1-2*(x*x+z*z);m->m[1][2]=  2*(y*z-x*w);m->m[1][3]=origin[1];
	m->m[2][0]=  2*(x*z-y*w);m->m[2][1]=  2*(y*z+x*w);m->m[2][2]=1-2*(x*x+y*y);m->m[2][3]=origin[2];
	m->m[3][0]=  0          ;m->m[3][1]=  0          ;m->m[3][2]=  0          ;m->m[3][3]=1;
}

// no idea why PSK/PSA files contain weird quaternions but they do...
#define PSKQUATNEGATIONS
void Mod_PSKMODEL_Load(model_t *mod, void *buffer, void *bufferend)
{
	int i, j, index, version, recordsize, numrecords;
	int numpnts, numvtxw, numfaces, nummatts, numbones, numrawweights, numanimbones, numanims, numanimkeys;
	fs_offset_t filesize;
	pskpnts_t *pnts;
	pskvtxw_t *vtxw;
	pskface_t *faces;
	pskmatt_t *matts;
	pskboneinfo_t *bones;
	pskrawweights_t *rawweights;
	pskboneinfo_t *animbones;
	pskaniminfo_t *anims;
	pskanimkeys_t *animkeys;
	void *animfilebuffer, *animbuffer, *animbufferend;
	pskchunk_t *pchunk;
	surfmesh_t *mesh;
	skinfile_t *skinfiles;
	char animname[MAX_QPATH];

	pchunk = (pskchunk_t *)buffer;
	if (strcmp(pchunk->id, "ACTRHEAD"))
		Host_Error ("Mod_PSKMODEL_Load: %s is not a ActorX model\n");

	loadmodel->type = mod_alias;
	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Q1BSP_Draw;
	loadmodel->CompileShadowVolume = R_Q1BSP_CompileShadowVolume;
	loadmodel->DrawShadowVolume = R_Q1BSP_DrawShadowVolume;
	loadmodel->DrawLight = R_Q1BSP_DrawLight;
	loadmodel->TraceBox = Mod_MDLMD2MD3_TraceBox;
	loadmodel->flags = 0; // there are no flags on zym models
	loadmodel->synctype = ST_RAND;

	// load external .skin files if present
	skinfiles = Mod_LoadSkinFiles();
	if (loadmodel->numskins < 1)
		loadmodel->numskins = 1;
	loadmodel->skinscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, loadmodel->numskins * sizeof(animscene_t));
	for (i = 0;i < loadmodel->numskins;i++)
	{
		loadmodel->skinscenes[i].firstframe = i;
		loadmodel->skinscenes[i].framecount = 1;
		loadmodel->skinscenes[i].loop = true;
		loadmodel->skinscenes[i].framerate = 10;
	}

	FS_StripExtension(loadmodel->name, animname, sizeof(animname));
	strlcat(animname, ".psa", sizeof(animname));
	animbuffer = animfilebuffer = FS_LoadFile(animname, loadmodel->mempool, false, &filesize);
	animbufferend = (void *)((unsigned char*)animbuffer + (int)filesize);
	if (animbuffer == NULL)
		Host_Error("%s: can't find .psa file (%s)\n", loadmodel->name, animname);

	numpnts = 0;
	pnts = NULL;
	numvtxw = 0;
	vtxw = NULL;
	numfaces = 0;
	faces = NULL;
	nummatts = 0;
	matts = NULL;
	numbones = 0;
	bones = NULL;
	numrawweights = 0;
	rawweights = NULL;
	numanims = 0;
	anims = NULL;
	numanimkeys = 0;
	animkeys = NULL;

	while (buffer < bufferend)
	{
		pchunk = (pskchunk_t *)buffer;
		buffer = (void *)((unsigned char *)buffer + sizeof(pskchunk_t));
		version = LittleLong(pchunk->version);
		recordsize = LittleLong(pchunk->recordsize);
		numrecords = LittleLong(pchunk->numrecords);
		if (developer.integer)
			Con_Printf("%s: %s %x: %i * %i = %i\n", loadmodel->name, pchunk->id, version, recordsize, numrecords, recordsize * numrecords);
		if (version != 0x1e83b9 && version != 0x1e9179 && version != 0x2e)
			Con_Printf ("%s: chunk %s has unknown version %x (0x1e83b9, 0x1e9179 and 0x2e are currently supported), trying to load anyway!\n", loadmodel->name, pchunk->id, version);
		if (!strcmp(pchunk->id, "ACTRHEAD"))
		{
			// nothing to do
		}
		else if (!strcmp(pchunk->id, "PNTS0000"))
		{
			pskpnts_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			numpnts = numrecords;
			pnts = (pskpnts_t *)buffer;
			for (index = 0, p = (pskpnts_t *)buffer;index < numrecords;index++, p++)
			{
				p->origin[0] = LittleFloat(p->origin[0]);
				p->origin[1] = LittleFloat(p->origin[1]);
				p->origin[2] = LittleFloat(p->origin[2]);
			}
			buffer = p;
		}
		else if (!strcmp(pchunk->id, "VTXW0000"))
		{
			pskvtxw_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			numvtxw = numrecords;
			vtxw = (pskvtxw_t *)buffer;
			for (index = 0, p = (pskvtxw_t *)buffer;index < numrecords;index++, p++)
			{
				p->pntsindex = LittleShort(p->pntsindex);
				p->texcoord[0] = LittleFloat(p->texcoord[0]);
				p->texcoord[1] = LittleFloat(p->texcoord[1]);
				if (p->pntsindex >= numpnts)
				{
					Con_Printf("%s: vtxw->pntsindex %i >= numpnts %i\n", loadmodel->name, p->pntsindex, numpnts);
					p->pntsindex = 0;
				}
			}
			buffer = p;
		}
		else if (!strcmp(pchunk->id, "FACE0000"))
		{
			pskface_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			numfaces = numrecords;
			faces = (pskface_t *)buffer;
			for (index = 0, p = (pskface_t *)buffer;index < numrecords;index++, p++)
			{
				p->vtxwindex[0] = LittleShort(p->vtxwindex[0]);
				p->vtxwindex[1] = LittleShort(p->vtxwindex[1]);
				p->vtxwindex[2] = LittleShort(p->vtxwindex[2]);
				p->group = LittleLong(p->group);
				if (p->vtxwindex[0] >= numvtxw)
				{
					Con_Printf("%s: face->vtxwindex[0] %i >= numvtxw %i\n", loadmodel->name, p->vtxwindex[0], numvtxw);
					p->vtxwindex[0] = 0;
				}
				if (p->vtxwindex[1] >= numvtxw)
				{
					Con_Printf("%s: face->vtxwindex[1] %i >= numvtxw %i\n", loadmodel->name, p->vtxwindex[1], numvtxw);
					p->vtxwindex[1] = 0;
				}
				if (p->vtxwindex[2] >= numvtxw)
				{
					Con_Printf("%s: face->vtxwindex[2] %i >= numvtxw %i\n", loadmodel->name, p->vtxwindex[2], numvtxw);
					p->vtxwindex[2] = 0;
				}
			}
			buffer = p;
		}
		else if (!strcmp(pchunk->id, "MATT0000"))
		{
			pskmatt_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			nummatts = numrecords;
			matts = (pskmatt_t *)buffer;
			for (index = 0, p = (pskmatt_t *)buffer;index < numrecords;index++, p++)
			{
			}
			buffer = p;
		}
		else if (!strcmp(pchunk->id, "REFSKELT"))
		{
			pskboneinfo_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			numbones = numrecords;
			bones = (pskboneinfo_t *)buffer;
			for (index = 0, p = (pskboneinfo_t *)buffer;index < numrecords;index++, p++)
			{
				p->numchildren = LittleLong(p->numchildren);
				p->parent = LittleLong(p->parent);
				p->basepose.quat[0] = LittleFloat(p->basepose.quat[0]);
				p->basepose.quat[1] = LittleFloat(p->basepose.quat[1]);
				p->basepose.quat[2] = LittleFloat(p->basepose.quat[2]);
				p->basepose.quat[3] = LittleFloat(p->basepose.quat[3]);
				p->basepose.origin[0] = LittleFloat(p->basepose.origin[0]);
				p->basepose.origin[1] = LittleFloat(p->basepose.origin[1]);
				p->basepose.origin[2] = LittleFloat(p->basepose.origin[2]);
				p->basepose.unknown = LittleFloat(p->basepose.unknown);
				p->basepose.size[0] = LittleFloat(p->basepose.size[0]);
				p->basepose.size[1] = LittleFloat(p->basepose.size[1]);
				p->basepose.size[2] = LittleFloat(p->basepose.size[2]);
#ifdef PSKQUATNEGATIONS
				if (index)
				{
					p->basepose.quat[0] *= -1;
					p->basepose.quat[1] *= -1;
					p->basepose.quat[2] *= -1;
				}
				else
				{
					p->basepose.quat[0] *=  1;
					p->basepose.quat[1] *= -1;
					p->basepose.quat[2] *=  1;
				}
#endif
				if (p->parent < 0 || p->parent >= numbones)
				{
					Con_Printf("%s: bone->parent %i >= numbones %i\n", loadmodel->name, p->parent, numbones);
					p->parent = 0;
				}
			}
			buffer = p;
		}
		else if (!strcmp(pchunk->id, "RAWWEIGHTS"))
		{
			pskrawweights_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", loadmodel->name, pchunk->id);
			// byteswap in place and keep the pointer
			numrawweights = numrecords;
			rawweights = (pskrawweights_t *)buffer;
			for (index = 0, p = (pskrawweights_t *)buffer;index < numrecords;index++, p++)
			{
				p->weight = LittleFloat(p->weight);
				p->pntsindex = LittleLong(p->pntsindex);
				p->boneindex = LittleLong(p->boneindex);
				if (p->pntsindex < 0 || p->pntsindex >= numpnts)
				{
					Con_Printf("%s: weight->pntsindex %i >= numpnts %i\n", loadmodel->name, p->pntsindex, numpnts);
					p->pntsindex = 0;
				}
				if (p->boneindex < 0 || p->boneindex >= numbones)
				{
					Con_Printf("%s: weight->boneindex %i >= numbones %i\n", loadmodel->name, p->boneindex, numbones);
					p->boneindex = 0;
				}
			}
			buffer = p;
		}
	}

	while (animbuffer < animbufferend)
	{
		pchunk = (pskchunk_t *)animbuffer;
		animbuffer = (void *)((unsigned char *)animbuffer + sizeof(pskchunk_t));
		version = LittleLong(pchunk->version);
		recordsize = LittleLong(pchunk->recordsize);
		numrecords = LittleLong(pchunk->numrecords);
		if (developer.integer)
			Con_Printf("%s: %s %x: %i * %i = %i\n", animname, pchunk->id, version, recordsize, numrecords, recordsize * numrecords);
		if (version != 0x1e83b9 && version != 0x1e9179 && version != 0x2e)
			Con_Printf ("%s: chunk %s has unknown version %x (0x1e83b9, 0x1e9179 and 0x2e are currently supported), trying to load anyway!\n", animname, pchunk->id, version);
		if (!strcmp(pchunk->id, "ANIMHEAD"))
		{
			// nothing to do
		}
		else if (!strcmp(pchunk->id, "BONENAMES"))
		{
			pskboneinfo_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", animname, pchunk->id);
			// byteswap in place and keep the pointer
			numanimbones = numrecords;
			animbones = (pskboneinfo_t *)animbuffer;
			// NOTE: supposedly psa does not need to match the psk model, the
			// bones missing from the psa would simply use their base
			// positions from the psk, but this is hard for me to implement
			// and people can easily make animations that match.
			if (numanimbones != numbones)
				Host_Error("%s: this loader only supports animations with the same bones as the mesh\n");
			for (index = 0, p = (pskboneinfo_t *)animbuffer;index < numrecords;index++, p++)
			{
				p->numchildren = LittleLong(p->numchildren);
				p->parent = LittleLong(p->parent);
				p->basepose.quat[0] = LittleFloat(p->basepose.quat[0]);
				p->basepose.quat[1] = LittleFloat(p->basepose.quat[1]);
				p->basepose.quat[2] = LittleFloat(p->basepose.quat[2]);
				p->basepose.quat[3] = LittleFloat(p->basepose.quat[3]);
				p->basepose.origin[0] = LittleFloat(p->basepose.origin[0]);
				p->basepose.origin[1] = LittleFloat(p->basepose.origin[1]);
				p->basepose.origin[2] = LittleFloat(p->basepose.origin[2]);
				p->basepose.unknown = LittleFloat(p->basepose.unknown);
				p->basepose.size[0] = LittleFloat(p->basepose.size[0]);
				p->basepose.size[1] = LittleFloat(p->basepose.size[1]);
				p->basepose.size[2] = LittleFloat(p->basepose.size[2]);
#ifdef PSKQUATNEGATIONS
				if (index)
				{
					p->basepose.quat[0] *= -1;
					p->basepose.quat[1] *= -1;
					p->basepose.quat[2] *= -1;
				}
				else
				{
					p->basepose.quat[0] *=  1;
					p->basepose.quat[1] *= -1;
					p->basepose.quat[2] *=  1;
				}
#endif
				if (p->parent < 0 || p->parent >= numanimbones)
				{
					Con_Printf("%s: bone->parent %i >= numanimbones %i\n", animname, p->parent, numanimbones);
					p->parent = 0;
				}
				// check that bones are the same as in the base
				if (strcmp(p->name, bones[index].name) || p->parent != bones[index].parent)
					Host_Error("%s: this loader only supports animations with the same bones as the mesh\n", animname);
			}
			animbuffer = p;
		}
		else if (!strcmp(pchunk->id, "ANIMINFO"))
		{
			pskaniminfo_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", animname, pchunk->id);
			// byteswap in place and keep the pointer
			numanims = numrecords;
			anims = (pskaniminfo_t *)animbuffer;
			for (index = 0, p = (pskaniminfo_t *)animbuffer;index < numrecords;index++, p++)
			{
				p->numbones = LittleLong(p->numbones);
				p->playtime = LittleFloat(p->playtime);
				p->fps = LittleFloat(p->fps);
				p->firstframe = LittleLong(p->firstframe);
				p->numframes = LittleLong(p->numframes);
				if (p->numbones != numbones)
				{
					Con_Printf("%s: animinfo->numbones != numbones, trying to load anyway!\n", animname);
				}
			}
			animbuffer = p;
		}
		else if (!strcmp(pchunk->id, "ANIMKEYS"))
		{
			pskanimkeys_t *p;
			if (recordsize != sizeof(*p))
				Host_Error("%s: %s has unsupported recordsize\n", animname, pchunk->id);
			numanimkeys = numrecords;
			animkeys = (pskanimkeys_t *)animbuffer;
			for (index = 0, p = (pskanimkeys_t *)animbuffer;index < numrecords;index++, p++)
			{
				p->origin[0] = LittleFloat(p->origin[0]);
				p->origin[1] = LittleFloat(p->origin[1]);
				p->origin[2] = LittleFloat(p->origin[2]);
				p->quat[0] = LittleFloat(p->quat[0]);
				p->quat[1] = LittleFloat(p->quat[1]);
				p->quat[2] = LittleFloat(p->quat[2]);
				p->quat[3] = LittleFloat(p->quat[3]);
				p->frametime = LittleFloat(p->frametime);
#ifdef PSKQUATNEGATIONS
				if (index % numbones)
				{
					p->quat[0] *= -1;
					p->quat[1] *= -1;
					p->quat[2] *= -1;
				}
				else
				{
					p->quat[0] *=  1;
					p->quat[1] *= -1;
					p->quat[2] *=  1;
				}
#endif
			}
			animbuffer = p;
			// TODO: allocate bonepose stuff
		}
		else
			Con_Printf("%s: unknown chunk ID \"%s\"\n", animname, pchunk->id);
	}

	if (!numpnts || !pnts || !numvtxw || !vtxw || !numfaces || !faces || !nummatts || !matts || !numbones || !bones || !numrawweights || !rawweights || !numanims || !anims || !numanimkeys || !animkeys)
		Host_Error("%s: missing required chunks\n", loadmodel->name);

	// FIXME: model bbox
	// model bbox
	for (i = 0;i < 3;i++)
	{
		loadmodel->normalmins[i] = -128;
		loadmodel->normalmaxs[i] = 128;
		loadmodel->yawmins[i] = -128;
		loadmodel->yawmaxs[i] = 128;
		loadmodel->rotatedmins[i] = -128;
		loadmodel->rotatedmaxs[i] = 128;
	}
	loadmodel->radius = 128;
	loadmodel->radius2 = loadmodel->radius * loadmodel->radius;

	loadmodel->numframes = 0;
	for (index = 0;index < numanims;index++)
		loadmodel->numframes += anims[index].numframes;

	loadmodel->num_bones = numbones;
	loadmodel->num_poses = loadmodel->num_bones * loadmodel->numframes;
	loadmodel->num_textures = loadmodel->nummeshes = loadmodel->nummodelsurfaces = loadmodel->num_surfaces = nummatts;

	if (numanimkeys != loadmodel->num_bones * loadmodel->numframes)
		Host_Error("%s: %s has incorrect number of animation keys\n", animname, pchunk->id);

	loadmodel->data_poses = (float *)Mem_Alloc(loadmodel->mempool, loadmodel->num_poses * sizeof(float[12]));
	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));
	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * loadmodel->numskins * sizeof(texture_t));
	loadmodel->data_surfaces = (msurface_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(msurface_t));
	loadmodel->surfacelist = (int *)Mem_Alloc(loadmodel->mempool, loadmodel->num_surfaces * sizeof(int));
	loadmodel->data_bones = (aliasbone_t *)Mem_Alloc(loadmodel->mempool, loadmodel->num_bones * sizeof(aliasbone_t));

	loadmodel->meshlist = (surfmesh_t **)Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t *));
	mesh = loadmodel->meshlist[0] = (surfmesh_t *)Mem_Alloc(loadmodel->mempool, sizeof(surfmesh_t));
	mesh->num_vertices = numvtxw;
	mesh->num_triangles = numfaces;
	mesh->data_element3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
	mesh->data_neighbor3i = (int *)Mem_Alloc(loadmodel->mempool, mesh->num_triangles * sizeof(int[3]));
	mesh->data_texcoordtexture2f = (float *)Mem_Alloc(loadmodel->mempool, mesh->num_vertices * sizeof(float[2]));

	// create surfaces
	for (index = 0, i = 0;index < nummatts;index++)
	{
		// since psk models do not have named sections, reuse their shader name as the section name
		if (matts[index].name[0])
			Mod_BuildAliasSkinsFromSkinFiles(loadmodel->data_textures + index, skinfiles, matts[index].name, matts[index].name);
		else
			for (j = 0;j < loadmodel->numskins;j++)
				Mod_BuildAliasSkinFromSkinFrame(loadmodel->data_textures + index + j * loadmodel->num_surfaces, NULL);
		loadmodel->surfacelist[index] = index;
		loadmodel->data_surfaces[index].groupmesh = loadmodel->meshlist[0];
		loadmodel->data_surfaces[index].texture = loadmodel->data_textures + index;
		loadmodel->data_surfaces[index].num_firstvertex = 0;
		loadmodel->data_surfaces[index].num_vertices = loadmodel->meshlist[0]->num_vertices;
	}

	// copy over the texcoords
	for (index = 0;index < numvtxw;index++)
	{
		mesh->data_texcoordtexture2f[index*2+0] = vtxw[index].texcoord[0];
		mesh->data_texcoordtexture2f[index*2+1] = vtxw[index].texcoord[1];
	}

	// loading the faces is complicated because we need to sort them into surfaces by mattindex
	for (index = 0;index < numfaces;index++)
		loadmodel->data_surfaces[faces[index].mattindex].num_triangles++;
	for (index = 0, i = 0;index < nummatts;index++)
	{
		loadmodel->data_surfaces[index].num_firsttriangle = i;
		i += loadmodel->data_surfaces[index].num_triangles;
		loadmodel->data_surfaces[index].num_triangles = 0;
	}
	for (index = 0;index < numfaces;index++)
	{
		i = (loadmodel->data_surfaces[faces[index].mattindex].num_firsttriangle + loadmodel->data_surfaces[faces[index].mattindex].num_triangles++)*3;
		mesh->data_element3i[i+0] = faces[index].vtxwindex[0];
		mesh->data_element3i[i+1] = faces[index].vtxwindex[1];
		mesh->data_element3i[i+2] = faces[index].vtxwindex[2];
	}

	// copy over the bones
	for (index = 0;index < numbones;index++)
	{
		strlcpy(loadmodel->data_bones[index].name, bones[index].name, sizeof(loadmodel->data_bones[index].name));
		loadmodel->data_bones[index].parent = (index || bones[index].parent > 0) ? bones[index].parent : -1;
		if (loadmodel->data_bones[index].parent >= index)
			Host_Error("%s bone[%i].parent >= %i\n", loadmodel->name, index, index);
	}

	// build bone-relative vertex weights from the psk point weights
	mesh->num_vertexboneweights = 0;
	for (index = 0;index < numvtxw;index++)
		for (j = 0;j < numrawweights;j++)
			if (rawweights[j].pntsindex == vtxw[index].pntsindex)
				mesh->num_vertexboneweights++;
	mesh->data_vertexboneweights = (surfmeshvertexboneweight_t *)Mem_Alloc(loadmodel->mempool, mesh->num_vertexboneweights * sizeof(surfmeshvertexboneweight_t));
	mesh->num_vertexboneweights = 0;
	for (index = 0;index < numvtxw;index++)
	{
		for (j = 0;j < numrawweights;j++)
		{
			if (rawweights[j].pntsindex == vtxw[index].pntsindex)
			{
				matrix4x4_t matrix, inversematrix;
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].vertexindex = index;
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].boneindex = rawweights[j].boneindex;
				mesh->data_vertexboneweights[mesh->num_vertexboneweights].weight = rawweights[j].weight;
				Matrix4x4_CreateIdentity(&matrix);
				for (i = rawweights[j].boneindex;i >= 0;i = loadmodel->data_bones[i].parent)
				{
					matrix4x4_t childmatrix, tempmatrix;
					Mod_PSKMODEL_AnimKeyToMatrix(bones[i].basepose.origin, bones[i].basepose.quat, &tempmatrix);
					childmatrix = matrix;
					Matrix4x4_Concat(&matrix, &tempmatrix, &childmatrix);
				}
				Matrix4x4_Invert_Simple(&inversematrix, &matrix);
				Matrix4x4_Transform(&inversematrix, pnts[rawweights[j].pntsindex].origin, mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin);
				VectorScale(mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin, mesh->data_vertexboneweights[mesh->num_vertexboneweights].weight, mesh->data_vertexboneweights[mesh->num_vertexboneweights].origin);
				mesh->num_vertexboneweights++;
			}
		}
	}

	// set up the animscenes based on the anims
	for (index = 0, i = 0;index < numanims;index++)
	{
		for (j = 0;j < anims[index].numframes;j++, i++)
		{
			dpsnprintf(loadmodel->animscenes[i].name, sizeof(loadmodel->animscenes[i].name), "%s_%d", anims[index].name, j);
			loadmodel->animscenes[i].firstframe = i;
			loadmodel->animscenes[i].framecount = 1;
			loadmodel->animscenes[i].loop = true;
			loadmodel->animscenes[i].framerate = 10;
		}
	}

	// load the poses from the animkeys
	for (index = 0;index < numanimkeys;index++)
	{
		matrix4x4_t matrix;
		Mod_PSKMODEL_AnimKeyToMatrix(animkeys[index].origin, animkeys[index].quat, &matrix);
		loadmodel->data_poses[index*12+0] = matrix.m[0][0];
		loadmodel->data_poses[index*12+1] = matrix.m[0][1];
		loadmodel->data_poses[index*12+2] = matrix.m[0][2];
		loadmodel->data_poses[index*12+3] = matrix.m[0][3];
		loadmodel->data_poses[index*12+4] = matrix.m[1][0];
		loadmodel->data_poses[index*12+5] = matrix.m[1][1];
		loadmodel->data_poses[index*12+6] = matrix.m[1][2];
		loadmodel->data_poses[index*12+7] = matrix.m[1][3];
		loadmodel->data_poses[index*12+8] = matrix.m[2][0];
		loadmodel->data_poses[index*12+9] = matrix.m[2][1];
		loadmodel->data_poses[index*12+10] = matrix.m[2][2];
		loadmodel->data_poses[index*12+11] = matrix.m[2][3];
	}

	// compile extra data we want
	Mod_ValidateElements(mesh->data_element3i, mesh->num_triangles, mesh->num_vertices, __FILE__, __LINE__);
	Mod_BuildTriangleNeighbors(mesh->data_neighbor3i, mesh->data_element3i, mesh->num_triangles);
	Mod_Alias_Mesh_CompileFrameZero(mesh);

	Mem_Free(animfilebuffer);
}

