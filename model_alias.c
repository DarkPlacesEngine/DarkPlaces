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

static cvar_t r_mipskins = {CVAR_SAVE, "r_mipskins", "0"};

void Mod_AliasInit (void)
{
	Cvar_RegisterVariable(&r_mipskins);
}

// LordHavoc: proper bounding box considerations
static float aliasbboxmin[3], aliasbboxmax[3], modelyawradius, modelradius;

static float vertst[MAXALIASVERTS][2];
static int vertusage[MAXALIASVERTS];
static int vertonseam[MAXALIASVERTS];
static int vertremap[MAXALIASVERTS];
static int temptris[MAXALIASTRIS][3];

static void Mod_ConvertAliasVerts (int inverts, vec3_t scale, vec3_t translate, trivertx_t *v, trivertx_t *out)
{
	int i, j, invalidnormals = 0;
	float dist;
	vec3_t temp;
	for (i = 0;i < inverts;i++)
	{
		if (vertremap[i] < 0 && vertremap[i+inverts] < 0) // only used vertices need apply...
			continue;

		temp[0] = v[i].v[0] * scale[0] + translate[0];
		temp[1] = v[i].v[1] * scale[1] + translate[1];
		temp[2] = v[i].v[2] * scale[2] + translate[2];
		// update bounding box
		if (temp[0] < aliasbboxmin[0]) aliasbboxmin[0] = temp[0];
		if (temp[1] < aliasbboxmin[1]) aliasbboxmin[1] = temp[1];
		if (temp[2] < aliasbboxmin[2]) aliasbboxmin[2] = temp[2];
		if (temp[0] > aliasbboxmax[0]) aliasbboxmax[0] = temp[0];
		if (temp[1] > aliasbboxmax[1]) aliasbboxmax[1] = temp[1];
		if (temp[2] > aliasbboxmax[2]) aliasbboxmax[2] = temp[2];
		dist = temp[0]*temp[0]+temp[1]*temp[1];
		if (modelyawradius < dist)
			modelyawradius = dist;
		dist += temp[2]*temp[2];
		if (modelradius < dist)
			modelradius = dist;

		j = vertremap[i]; // not onseam
		if (j >= 0)
		{
			VectorCopy(v[i].v, out[j].v);
			out[j].lightnormalindex = v[i].lightnormalindex;
			if (out[j].lightnormalindex >= NUMVERTEXNORMALS)
			{
				invalidnormals++;
				out[j].lightnormalindex = 0;
			}
		}
		j = vertremap[i+inverts]; // onseam
		if (j >= 0)
		{
			VectorCopy(v[i].v, out[j].v);
			out[j].lightnormalindex = v[i].lightnormalindex;
			if (out[j].lightnormalindex >= NUMVERTEXNORMALS)
			{
				invalidnormals++;
				out[j].lightnormalindex = 0;
			}
		}
	}
	if (invalidnormals)
		Con_Printf("Mod_ConvertAliasVerts: \"%s\", %i invalid normal indices found\n", loadmodel->name, invalidnormals);
}

static void Mod_MDL_LoadFrames (qbyte * datapointer, int inverts, int outverts, vec3_t scale, vec3_t translate)
{
	daliasframetype_t	*pframetype;
	daliasframe_t		*pinframe;
	daliasgroup_t		*group;
	daliasinterval_t	*intervals;
	int					i, f, pose, groupframes;
	float				interval;
	animscene_t			*scene;
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
				Host_Error("Mod_LoadAliasGroup: invalid interval");
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

			// convert to MD2 frame headers
			strcpy(loadmodel->mdlmd2data_frames[pose].name, pinframe->name);
			VectorCopy(scale, loadmodel->mdlmd2data_frames[pose].scale);
			VectorCopy(translate, loadmodel->mdlmd2data_frames[pose].translate);

			Mod_ConvertAliasVerts(inverts, scale, translate, (trivertx_t *)datapointer, loadmodel->mdlmd2data_pose + pose * outverts);
			datapointer += sizeof(trivertx_t) * inverts;
			pose++;
		}
	}
}

static rtexture_t *GL_SkinSplitShirt(qbyte *in, qbyte *out, int width, int height, int bits, char *name, int precache)
{
	int i, pixels, passed;
	qbyte pixeltest[16];
	for (i = 0;i < 16;i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while(pixels--)
	{
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255)
		{
			passed++;
			// turn to white while copying
			if (*in >= 128 && *in < 224) // backwards ranges
				*out = (*in & 15) ^ 15;
			else
				*out = *in & 15;
		}
		else
			*out = 0;
		in++;
		out++;
	}
	if (passed)
		return R_LoadTexture (loadmodel->texturepool, name, width, height, out - width*height, TEXTYPE_QPALETTE, (r_mipskins.integer ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	else
		return NULL;
}

static rtexture_t *GL_SkinSplit(qbyte *in, qbyte *out, int width, int height, int bits, char *name, int precache)
{
	int i, pixels, passed;
	qbyte pixeltest[16];
	for (i = 0;i < 16;i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while(pixels--)
	{
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255)
		{
			passed++;
			*out = *in;
		}
		else
			*out = 0;
		in++;
		out++;
	}
	if (passed)
		return R_LoadTexture (loadmodel->texturepool, name, width, height, out - width*height, TEXTYPE_QPALETTE, (r_mipskins.integer ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	else
		return NULL;
}

static int Mod_LoadExternalSkin (char *basename, skinframe_t *skinframe, int precache)
{
	skinframe->base   = loadtextureimagewithmask(loadmodel->texturepool, va("%s_normal", basename), 0, 0, false, r_mipskins.integer, precache);
	skinframe->fog    = image_masktex;
	if (!skinframe->base)
	{
		skinframe->base   = loadtextureimagewithmask(loadmodel->texturepool, basename, 0, 0, false, r_mipskins.integer, precache);
		skinframe->fog    = image_masktex;
	}
	skinframe->pants  = loadtextureimage(loadmodel->texturepool, va("%s_pants" , basename), 0, 0, false, r_mipskins.integer, precache);
	skinframe->shirt  = loadtextureimage(loadmodel->texturepool, va("%s_shirt" , basename), 0, 0, false, r_mipskins.integer, precache);
	skinframe->glow   = loadtextureimage(loadmodel->texturepool, va("%s_glow"  , basename), 0, 0, false, r_mipskins.integer, precache);
	skinframe->merged = NULL;
	return skinframe->base != NULL || skinframe->pants != NULL || skinframe->shirt != NULL || skinframe->glow != NULL || skinframe->merged != NULL || skinframe->fog != NULL;
}

static int Mod_LoadInternalSkin (char *basename, qbyte *skindata, qbyte *skintemp, int width, int height, skinframe_t *skinframe, int precache)
{
	if (skindata && skintemp)
		return false;
	skinframe->pants  = GL_SkinSplitShirt(skindata, skintemp, width, height, 0x0040, va("%s_pants", basename), false); // pants
	skinframe->shirt  = GL_SkinSplitShirt(skindata, skintemp, width, height, 0x0002, va("%s_shirt", basename), false); // shirt
	skinframe->glow   = GL_SkinSplit     (skindata, skintemp, width, height, 0xC000, va("%s_glow", basename), precache); // glow
	if (skinframe->pants || skinframe->shirt)
	{
		skinframe->base   = GL_SkinSplit (skindata, skintemp, width, height, 0x3FBD, va("%s_normal", basename), false); // normal (no special colors)
		skinframe->merged = GL_SkinSplit (skindata, skintemp, width, height, 0x3FFF, va("%s_body", basename), precache); // body (normal + pants + shirt, but not glow)
	}
	else
		skinframe->base   = GL_SkinSplit (skindata, skintemp, width, height, 0x3FFF, va("%s_base", basename), precache); // no special colors
	// quake model skins don't have alpha
	skinframe->fog = NULL;
	return true;
}

#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%d exceeds %d - %d)\n", loadmodel->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%f exceeds %f - %f)\n", loadmodel->name, VALUE, MIN, MAX);
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int						i, j, version, numverts, totalposes, totalskins, skinwidth, skinheight, totalverts, groupframes, groupskins;
	mdl_t					*pinmodel;
	stvert_t				*pinstverts;
	dtriangle_t				*pintriangles;
	daliasskintype_t		*pinskintype;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	daliasframetype_t		*pinframetype;
	daliasgroup_t			*pinframegroup;
	float					scales, scalet, scale[3], translate[3], interval;
	qbyte					*datapointer, *startframes, *startskins;
	char					name[MAX_QPATH];
	qbyte					*skintemp = NULL;
	skinframe_t				tempskinframe;
	animscene_t				*tempskinscenes;
	skinframe_t				*tempskinframes;
	modelyawradius = 0;
	modelradius = 0;

	datapointer = buffer;
	pinmodel = (mdl_t *)datapointer;
	datapointer += sizeof(mdl_t);

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 loadmodel->name, version, ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_MDLMD2;

	loadmodel->numskins = LittleLong(pinmodel->numskins);
	BOUNDI(loadmodel->numskins,0,256);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,4096);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,4096);
	loadmodel->numverts = numverts = LittleLong(pinmodel->numverts);
	BOUNDI(loadmodel->numverts,0,MAXALIASVERTS);
	loadmodel->numtris = LittleLong(pinmodel->numtris);
	BOUNDI(loadmodel->numtris,0,MAXALIASTRIS);
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
	datapointer += sizeof(dtriangle_t) * loadmodel->numtris;

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
	skintemp = Mem_Alloc(tempmempool, skinwidth * skinheight);
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
				Host_Error("Mod_LoadAliasModel: invalid interval\n");
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
			if (!Mod_LoadExternalSkin(name, loadmodel->skinframes + totalskins, i == 0))
				Mod_LoadInternalSkin(name, (qbyte *)datapointer, skintemp, skinwidth, skinheight, loadmodel->skinframes + totalskins, i == 0);
			datapointer += skinwidth * skinheight;
			totalskins++;
		}
	}
	Mem_Free(skintemp);
	// check for skins that don't exist in the model, but do exist as external images
	// (this was added because yummyluv kept pestering me about support for it)
	for (;;)
	{
		sprintf (name, "%s_%i", loadmodel->name, loadmodel->numskins);
		if (Mod_LoadExternalSkin(name, &tempskinframe, loadmodel->numskins == 0))
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
	scales = 1.0 / skinwidth;
	scalet = 1.0 / skinheight;
	for (i = 0;i < numverts;i++)
	{
		vertonseam[i] = LittleLong(pinstverts[i].onseam);
		vertst[i][0] = LittleLong(pinstverts[i].s) * scales;
		vertst[i][1] = LittleLong(pinstverts[i].t) * scalet;
		vertst[i+numverts][0] = vertst[i][0] + 0.5;
		vertst[i+numverts][1] = vertst[i][1];
		vertusage[i] = 0;
		vertusage[i+numverts] = 0;
	}

// load triangle data
	loadmodel->mdlmd2data_indices = Mem_Alloc(loadmodel->mempool, sizeof(int[3]) * loadmodel->numtris);

	// count the vertices used
	for (i = 0;i < numverts*2;i++)
		vertusage[i] = 0;
	for (i = 0;i < loadmodel->numtris;i++)
	{
		temptris[i][0] = LittleLong(pintriangles[i].vertindex[0]);
		temptris[i][1] = LittleLong(pintriangles[i].vertindex[1]);
		temptris[i][2] = LittleLong(pintriangles[i].vertindex[2]);
		if (!LittleLong(pintriangles[i].facesfront)) // backface
		{
			if (vertonseam[temptris[i][0]]) temptris[i][0] += numverts;
			if (vertonseam[temptris[i][1]]) temptris[i][1] += numverts;
			if (vertonseam[temptris[i][2]]) temptris[i][2] += numverts;
		}
		vertusage[temptris[i][0]]++;
		vertusage[temptris[i][1]]++;
		vertusage[temptris[i][2]]++;
	}
	// build remapping table and compact array
	totalverts = 0;
	for (i = 0;i < numverts*2;i++)
	{
		if (vertusage[i])
		{
			vertremap[i] = totalverts;
			vertst[totalverts][0] = vertst[i][0];
			vertst[totalverts][1] = vertst[i][1];
			totalverts++;
		}
		else
			vertremap[i] = -1; // not used at all
	}
	loadmodel->numverts = totalverts;
	// remap the triangle references
	for (i = 0;i < loadmodel->numtris;i++)
	{
		loadmodel->mdlmd2data_indices[i*3+0] = vertremap[temptris[i][0]];
		loadmodel->mdlmd2data_indices[i*3+1] = vertremap[temptris[i][1]];
		loadmodel->mdlmd2data_indices[i*3+2] = vertremap[temptris[i][2]];
	}
	// store the texture coordinates
	loadmodel->mdlmd2data_texcoords = Mem_Alloc(loadmodel->mempool, sizeof(float[2]) * totalverts);
	for (i = 0;i < totalverts;i++)
	{
		loadmodel->mdlmd2data_texcoords[i*2+0] = vertst[i][0];
		loadmodel->mdlmd2data_texcoords[i*2+1] = vertst[i][1];
	}

// load the frames
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->mdlmd2data_frames = Mem_Alloc(loadmodel->mempool, sizeof(md2frame_t) * totalposes);
	loadmodel->mdlmd2data_pose = Mem_Alloc(loadmodel->mempool, sizeof(trivertx_t) * totalposes * totalverts);

	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;

	Mod_MDL_LoadFrames (startframes, numverts, totalverts, scale, translate);

	modelyawradius = sqrt(modelyawradius);
	modelradius = sqrt(modelradius);
	for (j = 0;j < 3;j++)
	{
		loadmodel->normalmins[j] = aliasbboxmin[j];
		loadmodel->normalmaxs[j] = aliasbboxmax[j];
		loadmodel->rotatedmins[j] = -modelradius;
		loadmodel->rotatedmaxs[j] = modelradius;
	}
	loadmodel->yawmins[0] = loadmodel->yawmins[1] = -(loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] = modelyawradius);
	loadmodel->yawmins[2] = loadmodel->normalmins[2];
	loadmodel->yawmaxs[2] = loadmodel->normalmaxs[2];

	loadmodel->Draw = R_DrawQ1Q2AliasModel;
	loadmodel->DrawSky = NULL;
	loadmodel->DrawShadow = NULL;
}

static void Mod_MD2_ConvertVerts (vec3_t scale, vec3_t translate, trivertx_t *v, trivertx_t *out, int *vertremap)
{
	int i, invalidnormals = 0;
	float dist;
	trivertx_t *in;
	vec3_t temp;
	for (i = 0;i < loadmodel->numverts;i++)
	{
		in = v + vertremap[i];
		VectorCopy(in->v, out[i].v);
		temp[0] = in->v[0] * scale[0] + translate[0];
		temp[1] = in->v[1] * scale[1] + translate[1];
		temp[2] = in->v[2] * scale[2] + translate[2];
		// update bounding box
		if (temp[0] < aliasbboxmin[0]) aliasbboxmin[0] = temp[0];
		if (temp[1] < aliasbboxmin[1]) aliasbboxmin[1] = temp[1];
		if (temp[2] < aliasbboxmin[2]) aliasbboxmin[2] = temp[2];
		if (temp[0] > aliasbboxmax[0]) aliasbboxmax[0] = temp[0];
		if (temp[1] > aliasbboxmax[1]) aliasbboxmax[1] = temp[1];
		if (temp[2] > aliasbboxmax[2]) aliasbboxmax[2] = temp[2];
		dist = temp[0]*temp[0]+temp[1]*temp[1];
		if (modelyawradius < dist)
			modelyawradius = dist;
		dist += temp[2]*temp[2];
		if (modelradius < dist)
			modelradius = dist;
		out[i].lightnormalindex = in->lightnormalindex;
		if (out[i].lightnormalindex >= NUMVERTEXNORMALS)
		{
			invalidnormals++;
			out[i].lightnormalindex = 0;
		}
	}
	if (invalidnormals)
		Con_Printf("Mod_MD2_ConvertVerts: \"%s\", %i invalid normal indices found\n", loadmodel->name, invalidnormals);
}

void Mod_LoadQ2AliasModel (model_t *mod, void *buffer)
{
	int *vertremap;
	md2_t *pinmodel;
	qbyte *base;
	int version, end;
	int i, j, k, hashindex, num, numxyz, numst, xyz, st;
	float *stverts, s, t;
	struct md2verthash_s
	{
		struct md2verthash_s *next;
		int xyz;
		float st[2];
	}
	*hash, **md2verthash, *md2verthashdata;
	qbyte *datapointer;
	md2frame_t *pinframe;
	char *inskin;
	md2triangle_t *intri;
	unsigned short *inst;
	int skinwidth, skinheight;

	pinmodel = buffer;
	base = buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
			loadmodel->name, version, MD2ALIAS_VERSION);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_MDLMD2;
	loadmodel->Draw = R_DrawQ1Q2AliasModel;
	loadmodel->DrawSky = NULL;
	loadmodel->DrawShadow = NULL;

	if (LittleLong(pinmodel->num_tris < 1) || LittleLong(pinmodel->num_tris) > MD2MAX_TRIANGLES)
		Host_Error ("%s has invalid number of triangles: %i", loadmodel->name, LittleLong(pinmodel->num_tris));
	if (LittleLong(pinmodel->num_xyz < 1) || LittleLong(pinmodel->num_xyz) > MD2MAX_VERTS)
		Host_Error ("%s has invalid number of vertices: %i", loadmodel->name, LittleLong(pinmodel->num_xyz));
	if (LittleLong(pinmodel->num_frames < 1) || LittleLong(pinmodel->num_frames) > MD2MAX_FRAMES)
		Host_Error ("%s has invalid number of frames: %i", loadmodel->name, LittleLong(pinmodel->num_frames));
	if (LittleLong(pinmodel->num_skins < 0) || LittleLong(pinmodel->num_skins) > MAX_SKINS)
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
	loadmodel->numtris = LittleLong(pinmodel->num_tris);
	loadmodel->numframes = LittleLong(pinmodel->num_frames);

	loadmodel->flags = 0; // there are no MD2 flags
	loadmodel->synctype = ST_RAND;

	// load the skins
	inskin = (void*)(base + LittleLong(pinmodel->ofs_skins));
	if (loadmodel->numskins)
	{
		loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numskins);
		loadmodel->skinframes = Mem_Alloc(loadmodel->mempool, sizeof(skinframe_t) * loadmodel->numskins);
		for (i = 0;i < loadmodel->numskins;i++)
		{
			loadmodel->skinscenes[i].firstframe = i;
			loadmodel->skinscenes[i].framecount = 1;
			loadmodel->skinscenes[i].loop = true;
			loadmodel->skinscenes[i].framerate = 10;
			loadmodel->skinframes[i].base = loadtextureimagewithmask (loadmodel->texturepool, inskin, 0, 0, true, r_mipskins.integer, true);
			loadmodel->skinframes[i].fog = image_masktex;
			loadmodel->skinframes[i].pants = NULL;
			loadmodel->skinframes[i].shirt = NULL;
			loadmodel->skinframes[i].glow = NULL;
			loadmodel->skinframes[i].merged = NULL;
			inskin += MD2MAX_SKINNAME;
		}
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
	md2verthashdata = Mem_Alloc(tempmempool, loadmodel->numtris * 3 * sizeof(*hash));
	// swap the triangle list
	num = 0;
	loadmodel->mdlmd2data_indices = Mem_Alloc(loadmodel->mempool, loadmodel->numtris * sizeof(int[3]));
	for (i = 0;i < loadmodel->numtris;i++)
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
			loadmodel->mdlmd2data_indices[i*3+j] = (hash - md2verthashdata);
		}
	}

	Mem_Free(stverts);

	loadmodel->numverts = num;
	vertremap = Mem_Alloc(loadmodel->mempool, num * sizeof(int));
	loadmodel->mdlmd2data_texcoords = Mem_Alloc(loadmodel->mempool, num * sizeof(float[2]));
	for (i = 0;i < num;i++)
	{
		hash = md2verthashdata + i;
		vertremap[i] = hash->xyz;
		loadmodel->mdlmd2data_texcoords[i*2+0] = hash->st[0];
		loadmodel->mdlmd2data_texcoords[i*2+1] = hash->st[1];
	}

	Mem_Free(md2verthash);
	Mem_Free(md2verthashdata);

	// load frames
	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;
	modelyawradius = 0;
	modelradius = 0;

	datapointer = (base + LittleLong(pinmodel->ofs_frames));
	// load the frames
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(animscene_t));
	loadmodel->mdlmd2data_frames = Mem_Alloc(loadmodel->mempool, loadmodel->numframes * sizeof(md2frame_t));
	loadmodel->mdlmd2data_pose = Mem_Alloc(loadmodel->mempool, loadmodel->numverts * loadmodel->numframes * sizeof(trivertx_t));

	for (i = 0;i < loadmodel->numframes;i++)
	{
		pinframe = (md2frame_t *)datapointer;
		datapointer += sizeof(md2frame_t);
		strcpy(loadmodel->mdlmd2data_frames[i].name, pinframe->name);
		for (j = 0;j < 3;j++)
		{
			loadmodel->mdlmd2data_frames[i].scale[j] = LittleFloat(pinframe->scale[j]);
			loadmodel->mdlmd2data_frames[i].translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_MD2_ConvertVerts (loadmodel->mdlmd2data_frames[i].scale, loadmodel->mdlmd2data_frames[i].translate, (void *)datapointer, &loadmodel->mdlmd2data_pose[i * loadmodel->numverts], vertremap);
		datapointer += numxyz * sizeof(trivertx_t);

		strcpy(loadmodel->animscenes[i].name, loadmodel->mdlmd2data_frames[i].name);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;
	}

	Mem_Free(vertremap);

	// LordHavoc: model bbox
	modelyawradius = sqrt(modelyawradius);
	modelradius = sqrt(modelradius);
	for (j = 0;j < 3;j++)
	{
		loadmodel->normalmins[j] = aliasbboxmin[j];
		loadmodel->normalmaxs[j] = aliasbboxmax[j];
		loadmodel->rotatedmins[j] = -modelradius;
		loadmodel->rotatedmaxs[j] = modelradius;
	}
	loadmodel->yawmins[0] = loadmodel->yawmins[1] = -(loadmodel->yawmaxs[0] = loadmodel->yawmaxs[1] = modelyawradius);
	loadmodel->yawmins[2] = loadmodel->normalmins[2];
	loadmodel->yawmaxs[2] = loadmodel->normalmaxs[2];
}

static void zymswapintblock(int *m, int size)
{
	size /= 4;
	while(size--)
	{
		*m = BigLong(*m);
		m++;
	}
}

void Mod_LoadZymoticModel(model_t *mod, void *buffer)
{
	int i, pbase, *bonecount, numposes;
	unsigned int count, a, b, c, *renderlist, *renderlistend;
	rtexture_t **texture;
	char *shadername;
	zymtype1header_t *pinmodel, *pheader;
	zymscene_t *scene;
	zymbone_t *bone;
	float corner[2], modelradius;

	pinmodel = (void *)buffer;

	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_LoadZymoticModel: %s is not a zymotic model\n");

	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_LoadZymoticModel: only type 1 (skeletal pose) models are currently supported (name = %s)\n", loadmodel->name);

	loadmodel->type = mod_alias;
	loadmodel->aliastype = ALIASTYPE_ZYM;

	loadmodel->zymdata_header = pheader = Mem_Alloc(loadmodel->mempool, BigLong(pinmodel->filesize));

	pbase = (int) pheader;

	memcpy(pheader, pinmodel, BigLong(pinmodel->filesize));

	// byteswap header
#define SWAPLONG(var) var = BigLong(var)
#define SWAPFLOAT(var) var = BigFloat(var)
#define SWAPVEC(var) SWAPFLOAT(var[0]);SWAPFLOAT(var[1]);SWAPFLOAT(var[2])
	SWAPLONG(pheader->type);
	SWAPLONG(pheader->filesize);
	SWAPVEC(pheader->mins);
	SWAPVEC(pheader->maxs);
	SWAPFLOAT(pheader->radius);
	SWAPLONG(pheader->numverts);
	SWAPLONG(pheader->numtris);
	SWAPLONG(pheader->numshaders);
	SWAPLONG(pheader->numbones);
	SWAPLONG(pheader->numscenes);

#define SWAPLUMPINFO(var) SWAPLONG(pheader->lump_##var.start);SWAPLONG(pheader->lump_##var.length)
	SWAPLUMPINFO(scenes);
	SWAPLUMPINFO(poses);
	SWAPLUMPINFO(bones);
	SWAPLUMPINFO(vertbonecounts);
	SWAPLUMPINFO(verts);
	SWAPLUMPINFO(texcoords);
	SWAPLUMPINFO(render);
	SWAPLUMPINFO(shaders);
	SWAPLUMPINFO(trizone);

	loadmodel->flags = 0; // there are no flags
	loadmodel->numframes = pheader->numscenes;
	loadmodel->synctype = ST_SYNC;
	loadmodel->numtris = pheader->numtris;

	// FIXME: add skin support and texturing and shaders and...
	loadmodel->skinscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) + sizeof(skinframe_t));
	loadmodel->skinscenes[0].firstframe = 0;
	loadmodel->skinscenes[0].framecount = 1;
	loadmodel->skinscenes[0].loop = true;
	loadmodel->skinscenes[0].framerate = 10;
	loadmodel->skinframes = (void *)(loadmodel->skinscenes + 1);
	loadmodel->skinframes->base = NULL;
	loadmodel->skinframes->fog = NULL;
	loadmodel->skinframes->pants = NULL;
	loadmodel->skinframes->shirt = NULL;
	loadmodel->skinframes->glow = NULL;
	loadmodel->skinframes->merged = NULL;
	loadmodel->numskins = 1;
	numposes = pheader->lump_poses.length / sizeof(float[3][4]) / pheader->numbones;

	// go through the lumps, swapping things

//	zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
	scene = (void *) (pheader->lump_scenes.start + pbase);
	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	for (i = 0;i < pheader->numscenes;i++)
	{
		SWAPVEC(scene->mins);
		SWAPVEC(scene->maxs);
		SWAPFLOAT(scene->radius);
		SWAPFLOAT(scene->framerate);
		SWAPLONG(scene->flags);
		SWAPLONG(scene->start);
		SWAPLONG(scene->length);

		memcpy(loadmodel->animscenes[i].name, scene->name, 32);
		loadmodel->animscenes[i].firstframe = scene->start;
		loadmodel->animscenes[i].framecount = scene->length;
		loadmodel->animscenes[i].framerate = scene->framerate;
		loadmodel->animscenes[i].loop = (scene->flags & ZYMSCENEFLAG_NOLOOP) == 0;

		if ((unsigned int) loadmodel->animscenes[i].firstframe >= numposes)
			Host_Error("Mod_LoadZymoticModel: scene firstframe (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, numposes);
		if ((unsigned int) loadmodel->animscenes[i].firstframe + (unsigned int) loadmodel->animscenes[i].framecount > numposes)
			Host_Error("Mod_LoadZymoticModel: scene firstframe (%i) + framecount (%i) >= numposes (%i)\n", loadmodel->animscenes[i].firstframe, loadmodel->animscenes[i].framecount, numposes);
		if (loadmodel->animscenes[i].framerate < 0)
			Host_Error("Mod_LoadZymoticModel: scene framerate (%f) < 0\n", loadmodel->animscenes[i].framerate);
		scene++;
	}

//	zymlump_t lump_poses; // float pose[numposes][numbones][3][4]; // animation data
	zymswapintblock((void *) (pheader->lump_poses.start + pbase), pheader->lump_poses.length);

//	zymlump_t lump_bones; // zymbone_t bone[numbones];
	bone = (void *) (pheader->lump_bones.start + pbase);
	for (i = 0;i < pheader->numbones;i++)
	{
		SWAPLONG(bone[i].flags);
		SWAPLONG(bone[i].parent);
		if (bone[i].parent >= i)
			Host_Error("Mod_LoadZymoticModel: bone[i].parent >= i in %s\n", loadmodel->name);
	}

//	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	zymswapintblock((void *) (pheader->lump_vertbonecounts.start + pbase), pheader->lump_vertbonecounts.length);
	bonecount = (void *) (pheader->lump_vertbonecounts.start + pbase);
	for (i = 0;i < pheader->numbones;i++)
		if (bonecount[i] < 1)
			Host_Error("Mod_LoadZymoticModel: bone vertex count < 1 in %s\n", loadmodel->name);

//	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	zymswapintblock((void *) (pheader->lump_verts.start + pbase), pheader->lump_verts.length);

//	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	zymswapintblock((void *) (pheader->lump_texcoords.start + pbase), pheader->lump_texcoords.length);

//	zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	zymswapintblock((void *) (pheader->lump_render.start + pbase), pheader->lump_render.length);
	// validate renderlist and swap winding order of tris
	renderlist = (void *) (pheader->lump_render.start + pbase);
	renderlistend = (void *) ((qbyte *) renderlist + pheader->lump_render.length);
	i = pheader->numshaders * sizeof(int) + pheader->numtris * sizeof(int[3]);
	if (pheader->lump_render.length != i)
		Host_Error("Mod_LoadZymoticModel: renderlist is wrong size in %s (is %i bytes, should be %i bytes)\n", loadmodel->name, pheader->lump_render.length, i);
	for (i = 0;i < pheader->numshaders;i++)
	{
		if (renderlist >= renderlistend)
			Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
		count = *renderlist++;
		if (renderlist + count * 3 > renderlistend)
			Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (wrong size)\n", loadmodel->name);
		while (count--)
		{
			a = renderlist[0];
			b = renderlist[1];
			c = renderlist[2];
			if (a >= pheader->numverts || b >= pheader->numverts || c >= pheader->numverts)
				Host_Error("Mod_LoadZymoticModel: corrupt renderlist in %s (out of bounds index)\n", loadmodel->name);
			renderlist[0] = c;
			renderlist[1] = b;
			renderlist[2] = a;
			renderlist += 3;
		}
	}

//	zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
	shadername = (void *) (pheader->lump_shaders.start + pbase);
	texture = (void *) shadername;
	for (i = 0;i < pheader->numshaders;i++)
	{
		rtexture_t *rt;
		rt = loadtextureimage(loadmodel->texturepool, shadername, 0, 0, true, r_mipskins.integer, true);
		shadername += 32;
		*texture++ = rt; // reuse shader name list for texture pointers
	}

//	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
	zymswapintblock((void *) (pheader->lump_trizone.start + pbase), pheader->lump_trizone.length);

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

	loadmodel->Draw = R_DrawZymoticModel;
	loadmodel->DrawSky = NULL;
	loadmodel->DrawShadow = NULL;
}

