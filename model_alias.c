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

cvar_t r_mipskins = {"r_mipskins", "1", true};

/*
===============
Mod_AliasInit
===============
*/
void Mod_AliasInit (void)
{
	Cvar_RegisterVariable(&r_mipskins);
}

int			posenum;

float		aliasbboxmin[3], aliasbboxmax[3]; // LordHavoc: proper bounding box considerations

float vertst[MAXALIASVERTS][2];
int vertusage[MAXALIASVERTS];
int vertonseam[MAXALIASVERTS];
int vertremap[MAXALIASVERTS];
unsigned short temptris[MAXALIASTRIS][3];

void Mod_ConvertAliasVerts (int inverts, vec3_t scale, vec3_t translate, trivertx_t *v, trivertx_t *out)
{
	int i, j, invalidnormals = 0;
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
		Con_Printf("Mod_ConvertAliasVerts: \"%s\", %i invalid normal indices found\n", loadname, invalidnormals);
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void *pin, maliashdr_t *mheader, int inverts, int outverts, trivertx_t **posevert, animscene_t *scene)
{
	trivertx_t		*pinframe;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy(scene->name, pdaliasframe->name);
	scene->firstframe = posenum;
	scene->framecount = 1;
	scene->framerate = 10.0f; // unnecessary but...
	scene->loop = true;

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	Mod_ConvertAliasVerts(inverts, mheader->scale, mheader->scale_origin, pinframe, *posevert);
	*posevert += outverts;
	posenum++;

	pinframe += inverts;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void *pin, maliashdr_t *mheader, int inverts, int outverts, trivertx_t **posevert, animscene_t *scene)
{
	int		i, numframes;
	void	*ptemp;
	float	interval;
	
	numframes = LittleLong (((daliasgroup_t *)pin)->numframes);

	strcpy(scene->name, ((daliasframe_t *) (sizeof(daliasinterval_t) * numframes + sizeof(daliasgroup_t) + (int) pin))->name);
	scene->firstframe = posenum;
	scene->framecount = numframes;
	interval = LittleFloat (((daliasinterval_t *)(((daliasgroup_t *)pin) + 1))->interval); // FIXME: support variable framerate groups?
	if (interval < 0.01f)
		Host_Error("Mod_LoadAliasGroup: invalid interval");
	scene->framerate = 1.0f / interval;
	scene->loop = true;

	ptemp = (void *)(((daliasinterval_t *)(((daliasgroup_t *)pin) + 1)) + numframes);

	for (i=0 ; i<numframes ; i++)
	{
		((daliasframe_t *)ptemp)++;
		Mod_ConvertAliasVerts(inverts, mheader->scale, mheader->scale_origin, ptemp, *posevert);
		*posevert += outverts;
		posenum++;
		ptemp = (trivertx_t *)ptemp + inverts;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

rtexture_t *GL_SkinSplitShirt(byte *in, byte *out, int width, int height, unsigned short bits, char *name, int precache)
{
	int i, pixels, passed;
	byte pixeltest[16];
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
		return R_LoadTexture (name, width, height, out - width*height, (r_mipskins.value ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	else
		return NULL;
}

rtexture_t *GL_SkinSplit(byte *in, byte *out, int width, int height, unsigned short bits, char *name, int precache)
{
	int i, pixels, passed;
	byte pixeltest[16];
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
		return R_LoadTexture (name, width, height, out - width*height, (r_mipskins.value ? TEXF_MIPMAP : 0) | (precache ? TEXF_PRECACHE : 0));
	else
		return NULL;
}

int GL_SkinCheck(byte *in, int width, int height, unsigned short bits)
{
	int i, pixels, passed;
	byte pixeltest[16];
	for (i = 0;i < 16;i++)
		pixeltest[i] = (bits & (1 << i)) != 0;
	pixels = width*height;
	passed = 0;
	while(pixels--)
	{
		if (pixeltest[*in >> 4] && *in != 0 && *in != 255)
			return true;
		in++;
	}
	return false;
}

void Mod_LoadSkin (maliashdr_t *mheader, char *basename, byte *skindata, byte *skintemp, int width, int height, rtexture_t **skintex)
{
#if 0
	int skin_normal, skin_pants, skin_shirt, skin_glow, skin_body, temp;
	skin_normal = loadtextureimage(va("%s_normal", basename));
	skin_pants  = loadtextureimage(va("%s_pants" , basename));
	skin_shirt  = loadtextureimage(va("%s_shirt" , basename));
	skin_glow   = loadtextureimage(va("%s_glow"  , basename));
	skin_body   = loadtextureimage(va("%s_body"  , basename));
	if (!(skin_normal || skin_pants || skin_shirt || skin_glow || skin_body))
		skin_body = loadtextureimage(name);
	if (skin_normal || skin_pants || skin_shirt || skin_glow || skin_body)
	{
		skintexnum[0] = skin_normal;
		skintexnum[1] = skin_pants;
		skintexnum[2] = skin_shirt;
		skintexnum[3] = skin_glow;
		skintexnum[4] = skin_body;
	}
	else
	{
		Mod_FloodFillSkin(skin, width, height);
		skin_normal = GL_SkinCheck((byte *)pskintype, width, height, 0x3FBD);
		skin_pants = GL_SkinCheck((byte *)pskintype, width, height, 0x0040);
		skin_shirt = GL_SkinCheck((byte *)pskintype, width, height, 0x0002);
		skin_glow = GL_SkinCheck((byte *)pskintype, width, height, 0xC000);
		skin_body = GL_SkinCheck((byte *)pskintype, width, height, 0x3FFF);
		if (skin_pants || skin_shirt)
		{
			byte *saveskin;
			saveskin = Hunk_AllocName(width*height, va("%s skin", loadname));
			memcpy((saveskin, byte *)pskintype, width*height);
			temp = (int) saveskin - (int) mheader;
			skintexnum[0] = skin_normal ? -temp : 0;
			skintexnum[1] = skin_pants ? -temp : 0;
			skintexnum[2] = skin_shirt ? -temp : 0;
			skintexnum[3] = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0xC000, va("%s_glow", basename)); // glow
			skintexnum[4] = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FFF, va("%s_body", basename)); // body (normal + pants + shirt, but not glow)
		}
		else
		{
			skintexnum[0] = 0;
			skintexnum[1] = 0;
			skintexnum[2] = 0;
			skintexnum[3] = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0xC000, va("%s_glow", basename)); // glow
			skintexnum[4] = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FFF, va("%s_body", basename)); // body (normal + pants + shirt, but not glow)
		}
	}
#else
	skintex[0] = loadtextureimage(va("%s_normal", basename), 0, 0, false, r_mipskins.value, true);
	skintex[1] = NULL;
	skintex[2] = NULL;
	skintex[3] = loadtextureimage(va("%s_glow"  , basename), 0, 0, false, r_mipskins.value, true);
	skintex[4] = NULL;
	if (skintex[0])
	{
		skintex[1] = loadtextureimage(va("%s_pants" , basename), 0, 0, false, r_mipskins.value, true);
		skintex[2] = loadtextureimage(va("%s_shirt" , basename), 0, 0, false, r_mipskins.value, true);
	}
	else
	{
		skintex[0] = loadtextureimage(basename, 0, 0, false, true, true);
		if (!skintex[0])
		{
			skintex[1] = GL_SkinSplitShirt(skindata, skintemp, width, height, 0x0040, va("%s_pants", basename), false); // pants
			skintex[2] = GL_SkinSplitShirt(skindata, skintemp, width, height, 0x0002, va("%s_shirt", basename), false); // shirt
			skintex[3] = GL_SkinSplit(skindata, skintemp, width, height, 0xC000, va("%s_glow", basename), true); // glow
			if (skintex[1] || skintex[2])
			{
				skintex[0] = GL_SkinSplit(skindata, skintemp, width, height, 0x3FBD, va("%s_normal", basename), false); // normal (no special colors)
				skintex[4] = GL_SkinSplit(skindata, skintemp, width, height, 0x3FFF, va("%s_body", basename), true); // body (normal + pants + shirt, but not glow)
			}
			else
				skintex[0] = GL_SkinSplit(skindata, skintemp, width, height, 0x3FFF, va("%s_base", basename), true); // no special colors
		}
	}
#endif
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (maliashdr_t *mheader, int numskins, daliasskintype_t *pskintype, int width, int height)
{
	int		i, j;
	char	name[32];
	int		s;
	byte	*skin;
	daliasskingroup_t		*pinskingroup;
	int		groupskins;
	daliasskininterval_t	*pinskinintervals;
	int		skinranges, skincount, *skinrange, skinnum;
	rtexture_t **skintex;
	void	*temp;
	byte	*skintemp = NULL;
	
	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = width * height;
	skintemp = qmalloc(s);

	// LordHavoc: skim the data, measure the number of skins and number of groups
	skinranges = numskins;
	skincount = 0;
	temp = pskintype;
	for (i = 0;i < numskins;i++)
	{
		pskintype++;
		if (pskintype[-1].type == ALIAS_SKIN_SINGLE)
		{
			skincount++;
			(byte *)pskintype += s;
		}
		else
		{
			groupskins = LittleLong (((daliasskingroup_t *)pskintype)->numskins);
			skincount += groupskins;
			(byte *)pskintype += (s + sizeof(daliasskininterval_t)) * groupskins + sizeof(daliasskingroup_t);
		}
	}
	pskintype = temp;

	skinrange = loadmodel->skinanimrange;
	skintex = loadmodel->skinanim;
//	skinrange = Hunk_AllocName (sizeof(int) * (skinranges + skincount), loadname);	
//	skintexnum = skinrange + skinranges * 2;
//	loadmodel->skinanimrange = (int) skinrange - (int) pheader;
//	loadmodel->skinanim = (int) skintexnum - (int) pheader;
	skinnum = 0;
	for (i = 0;i < numskins;i++)
	{
		*skinrange++ = skinnum; // start of the range
		pskintype++;
		if (pskintype[-1].type == ALIAS_SKIN_SINGLE)
		{
			*skinrange++ = 1; // single skin
			skinnum++;
			sprintf (name, "%s_%i", loadmodel->name, i);
			Mod_LoadSkin(mheader, name, (byte *)pskintype, skintemp, width, height, skintex);
			skintex += 5;
			pskintype = (daliasskintype_t *)((byte *)(pskintype) + s);
		}
		else
		{
			// animating skin group.  yuck.
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			*skinrange++ = groupskins; // number of skins
			skinnum += groupskins;
			for (j = 0;j < groupskins;j++)
			{
				sprintf (name, "%s_%i_%i", loadmodel->name, i, j);
				Mod_LoadSkin(mheader, name, (byte *)pskintype, skintemp, width, height, skintex);
				skintex += 5;
				pskintype = (daliasskintype_t *)((byte *)(pskintype) + s);
			}
		}
	}
	loadmodel->numskins = numskins;
	qfree(skintemp);

	return (void *)pskintype;
}

void *Mod_SkipAllSkins (int numskins, daliasskintype_t *pskintype, int skinsize)
{
	int		i;
	for (i = 0;i < numskins;i++)
	{
		pskintype++;
		if (pskintype[-1].type == ALIAS_SKIN_SINGLE)
			(byte *)pskintype += skinsize;
		else
			(byte *)pskintype += (skinsize + sizeof(daliasskininterval_t)) * LittleLong (((daliasskingroup_t *)pskintype)->numskins) + sizeof(daliasskingroup_t);
	}
	return pskintype;
}

//=========================================================================

//void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

/*
=================
Mod_LoadAliasModel
=================
*/
#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%d exceeds %d - %d)\n", mod->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid ##VALUE (%f exceeds %f - %f)\n", mod->name, VALUE, MIN, MAX);
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, start, end, total, numverts, numtris, numposes, numskins, skinwidth, skinheight, f, totalverts;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	float				*pouttexcoords, scales, scalet;
	maliashdr_t			*mheader;
	unsigned short		*pouttris;
	trivertx_t			*posevert;
	animscene_t			*animscenes;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MDL;

	numframes = LittleLong(pinmodel->numframes);
	BOUNDI(numframes,0,65536);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,MAXALIASVERTS);
	numtris = LittleLong(pinmodel->numtris);
	BOUNDI(numtris,0,MAXALIASTRIS);
	numskins = LittleLong(pinmodel->numskins);
	BOUNDI(numskins,0,256);
	skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(skinwidth,0,4096);
	skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(skinheight,0,1024);
	
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pinstverts = (stvert_t *)Mod_SkipAllSkins (numskins, pskintype, skinwidth * skinheight);
	pintriangles = (dtriangle_t *)&pinstverts[numverts];
	pframetype = (daliasframetype_t *)&pintriangles[numtris];

	numposes = 0;
	for (i=0 ; i<numframes ; i++)
	{
		if ((aliasframetype_t) LittleLong (pframetype->type) == ALIAS_SINGLE)
		{
			numposes++;
			pframetype = (daliasframetype_t *)((int)pframetype + sizeof(daliasframetype_t)                         + (sizeof(daliasframe_t)                            + sizeof(trivertx_t) * numverts)    );
		}
		else
		{
			f = LittleLong (((daliasgroup_t *)((int)pframetype + sizeof(daliasframetype_t)))->numframes);
			numposes += f;
			pframetype = (daliasframetype_t *)((int)pframetype + sizeof(daliasframetype_t) + sizeof(daliasgroup_t) + (sizeof(daliasframe_t) + sizeof(daliasinterval_t) + sizeof(trivertx_t) * numverts) * f);
		}
	}

	// rebuild the model
	mheader = Hunk_AllocName (sizeof(maliashdr_t), va("%s model header", loadname));
	mod->flags = LittleLong (pinmodel->flags);
// endian-adjust and copy the data, starting with the alias model header
	mheader->numverts = numverts;
	mod->numtris = mheader->numtris = numtris;
	mod->numframes = mheader->numframes = numframes;
	mod->synctype = LittleLong (pinmodel->synctype);
	BOUNDI(mod->synctype,0,2);

	for (i=0 ; i<3 ; i++)
	{
		mheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		BOUNDF(mheader->scale[i],0,65536);
		mheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		BOUNDF(mheader->scale_origin[i],-65536,65536);
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins(mheader, numskins, pskintype, skinwidth, skinheight);

	// store texture coordinates into temporary array, they will be stored after usage is determined (triangle data)
	pinstverts = (stvert_t *)pskintype;

	// LordHavoc: byteswap and convert stvert data
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
	pouttris = Hunk_AllocName(sizeof(unsigned short[3]) * numtris, va("%s triangles", loadname));
	mheader->tridata = (int) pouttris - (int) mheader;
	pintriangles = (dtriangle_t *)&pinstverts[mheader->numverts];

	// count the vertices used
	for (i = 0;i < numverts*2;i++)
		vertusage[i] = 0;
	for (i = 0;i < numtris;i++)
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
	mheader->numverts = totalverts;
	// remap the triangle references
	for (i = 0;i < numtris;i++)
	{
		*pouttris++ = vertremap[temptris[i][0]];
		*pouttris++ = vertremap[temptris[i][1]];
		*pouttris++ = vertremap[temptris[i][2]];
	}
	// store the texture coordinates
	pouttexcoords = Hunk_AllocName(sizeof(float[2]) * totalverts, va("%s texcoords", loadname));
	mheader->texdata = (int) pouttexcoords - (int) mheader;
	for (i = 0;i < totalverts;i++)
	{
		*pouttexcoords++ = vertst[i][0];
		*pouttexcoords++ = vertst[i][1];
	}

// load the frames
	posenum = 0;
	posevert = Hunk_AllocName(sizeof(trivertx_t) * numposes * totalverts, va("%s vertex data", loadname));
	mheader->posedata = (int) posevert - (int) mheader;
	pframetype = (daliasframetype_t *)&pintriangles[numtris];

	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;

	animscenes = Hunk_AllocName(sizeof(animscene_t) * mod->numframes, va("%s sceneinfo", loadname));

	for (i = 0;i < numframes;i++)
	{
		if ((aliasframetype_t) LittleLong (pframetype->type) == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, mheader, numverts, totalverts, &posevert, animscenes + i);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, mheader, numverts, totalverts, &posevert, animscenes + i);
	}

	mod->ofs_scenes = (int) animscenes - (int) mheader;

	// LordHavoc: fixed model bbox - was //FIXME: do this right
	//mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	//mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;
	for (j = 0;j < 3;j++)
	{
		mod->mins[j] = aliasbboxmin[j];
		mod->maxs[j] = aliasbboxmax[j];
	}

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, mheader, total);

	Hunk_FreeToLowMark (start);
}

void Mod_ConvertQ2AliasVerts (int numverts, vec3_t scale, vec3_t translate, trivertx_t *v, trivertx_t *out)
{
	int i, invalidnormals = 0;
	vec3_t temp;
	for (i = 0;i < numverts;i++)
	{
		VectorCopy(v[i].v, out[i].v);
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
		out[i].lightnormalindex = v[i].lightnormalindex;
		if (out[i].lightnormalindex >= NUMVERTEXNORMALS)
		{
			invalidnormals++;
			out[i].lightnormalindex = 0;
		}
	}
	if (invalidnormals)
		Con_Printf("Mod_ConvertQ2AliasVerts: \"%s\", %i invalid normal indices found\n", loadname, invalidnormals);
}

/*
=================
Mod_LoadQ2AliasModel
=================
*/
void Mod_LoadQ2AliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, size, *pinglcmd, *poutglcmd, start, end, total, framesize;
	md2_t				*pinmodel;
	md2mem_t			*pheader;
	md2triangle_t		*pintriangles, *pouttriangles;
	md2frame_t			*pinframe;
	md2frame_t			*poutframe;
	char				*pinskins;
//	temptris_t			*tris;
	animscene_t			*animscenes;

	start = Hunk_LowMark ();

//	if (!temptris)
//		temptris = qmalloc(sizeof(temptris_t) * MD2MAX_TRIANGLES);

	pinmodel = (md2_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2ALIAS_VERSION);

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MD2;

	framesize = sizeof(md2framesize_t) + LittleLong(pinmodel->num_xyz) * sizeof(trivertx_t);
	// LordHavoc: calculate size for in memory version
	size = sizeof(md2mem_t)
		 + LittleLong(pinmodel->num_st) * sizeof(md2stvert_t)
		 + LittleLong(pinmodel->num_tris) * sizeof(md2triangle_t)
		 + LittleLong(pinmodel->num_frames) * framesize
		 + LittleLong(pinmodel->num_glcmds) * sizeof(int);
	if (size <= 0 || size >= MD2MAX_SIZE)
		Host_Error ("%s is not a valid model", mod->name);
	pheader = Hunk_AllocName (size, va("%s Quake2 model", loadname));
	
	mod->flags = 0; // there are no MD2 flags
	mod->numframes = LittleLong(pinmodel->num_frames);
	mod->synctype = ST_RAND;
	mod->numtris = LittleLong(pinmodel->num_tris); // LordHavoc: to simplify renderer decisions

	if (LittleLong(pinmodel->num_skins) >= 1 && (LittleLong(pinmodel->ofs_skins <= 0) || LittleLong(pinmodel->ofs_skins) >= LittleLong(pinmodel->ofs_end)))
		Host_Error ("%s is not a valid model", mod->name);
	if (LittleLong(pinmodel->ofs_st <= 0) || LittleLong(pinmodel->ofs_st) >= LittleLong(pinmodel->ofs_end))
		Host_Error ("%s is not a valid model", mod->name);
	if (LittleLong(pinmodel->ofs_tris <= 0) || LittleLong(pinmodel->ofs_tris) >= LittleLong(pinmodel->ofs_end))
		Host_Error ("%s is not a valid model", mod->name);
	if (LittleLong(pinmodel->ofs_frames <= 0) || LittleLong(pinmodel->ofs_frames) >= LittleLong(pinmodel->ofs_end))
		Host_Error ("%s is not a valid model", mod->name);
	if (LittleLong(pinmodel->ofs_glcmds <= 0) || LittleLong(pinmodel->ofs_glcmds) >= LittleLong(pinmodel->ofs_end))
		Host_Error ("%s is not a valid model", mod->name);

	if (LittleLong(pinmodel->num_tris < 1) || LittleLong(pinmodel->num_tris) > MD2MAX_TRIANGLES)
		Host_Error ("%s has invalid number of triangles: %i", mod->name, LittleLong(pinmodel->num_tris));
	if (LittleLong(pinmodel->num_xyz < 1) || LittleLong(pinmodel->num_xyz) > MD2MAX_VERTS)
		Host_Error ("%s has invalid number of vertices: %i", mod->name, LittleLong(pinmodel->num_xyz));
	if (LittleLong(pinmodel->num_frames < 1) || LittleLong(pinmodel->num_frames) > MD2MAX_FRAMES)
		Host_Error ("%s has invalid number of frames: %i", mod->name, LittleLong(pinmodel->num_frames));
	if (LittleLong(pinmodel->num_skins < 0) || LittleLong(pinmodel->num_skins) > MAX_SKINS)
		Host_Error ("%s has invalid number of skins: %i", mod->name, LittleLong(pinmodel->num_skins));

	pheader->framesize = framesize;
	pheader->num_skins = LittleLong(pinmodel->num_skins);
	pheader->num_xyz = LittleLong(pinmodel->num_xyz);
	pheader->num_st = LittleLong(pinmodel->num_st);
	pheader->num_tris = LittleLong(pinmodel->num_tris);
	pheader->num_frames = LittleLong(pinmodel->num_frames);
	pheader->num_glcmds = LittleLong(pinmodel->num_glcmds);

// load the skins
	if (pheader->num_skins)
	{
		rtexture_t **skin;
		int *skinrange;
		skinrange = loadmodel->skinanimrange;
		skin = loadmodel->skinanim;
//		skinrange = Hunk_AllocName (sizeof(int) * (pheader->num_skins * 2), loadname);	
//		skin = skinrange + pheader->num_skins * 2;
//		loadmodel->skinanimrange = (int) skinrange - (int) pheader;
//		loadmodel->skinanim = (int) skin - (int) pheader;
		pinskins = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_skins));
		for (i = 0;i < pheader->num_skins;i++)
		{
			*skinrange++ = i;
			*skinrange++ = 1;
			*skin++ = loadtextureimage (pinskins, 0, 0, true, r_mipskins.value, true);
			*skin++ = NULL; // the extra 4 layers are currently unused
			*skin++ = NULL;
			*skin++ = NULL;
			*skin++ = NULL;
			pinskins += MD2MAX_SKINNAME;
		}
	}
	loadmodel->numskins = pheader->num_skins;

// load triangles
	pintriangles = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_tris));
	pouttriangles = (void*)&pheader[1];
	pheader->ofs_tris = (int) pouttriangles - (int) pheader;
//	tris = temptris;
	// swap the triangle list
	for (i = 0;i < pheader->num_tris;i++)
	{
		for (j = 0;j < 3;j++)
		{
			temptris[i][j] = pouttriangles->index_xyz[j] = LittleShort (pintriangles->index_xyz[j]);
			pouttriangles->index_st[j] = LittleShort (pintriangles->index_st[j]);
			if (pouttriangles->index_xyz[j] >= pheader->num_xyz)
				Host_Error ("%s has invalid vertex indices", mod->name);
			if (pouttriangles->index_st[j] >= pheader->num_st)
				Host_Error ("%s has invalid vertex indices", mod->name);
		}
		pintriangles++;
		pouttriangles++;
	}

	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;

// load the frames
	pinframe = (void*) ((int) pinmodel + LittleLong(pinmodel->ofs_frames));
	poutframe = (void*) pouttriangles;
	pheader->ofs_frames = (int) poutframe - (int) pheader;

	animscenes = Hunk_AllocName(sizeof(animscene_t) * mod->numframes, va("%s sceneinfo", loadname));

	for (i = 0;i < pheader->num_frames;i++)
	{
		strcpy(poutframe->name, pinframe->name);
		for (j = 0;j < 3;j++)
		{
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_ConvertQ2AliasVerts (pheader->num_xyz, poutframe->scale, poutframe->translate, &pinframe->verts[0], &poutframe->verts[0]);

		strcpy(animscenes[i].name, poutframe->name);
		animscenes[i].firstframe = i;
		animscenes[i].framecount = 1;
		animscenes[i].framerate = 10;
		animscenes[i].loop = true;

		pinframe = (void*) &pinframe->verts[j];
		poutframe = (void*) &poutframe->verts[j];
	}

	mod->ofs_scenes = (int) animscenes - (int) pheader;

	// LordHavoc: model bbox
	for (j = 0;j < 3;j++)
	{
		mod->mins[j] = aliasbboxmin[j];
		mod->maxs[j] = aliasbboxmax[j];
	}

	// load the draw list
	pinglcmd = (void*) ((int) pinmodel + LittleLong(pinmodel->ofs_glcmds));
	poutglcmd = (void*) poutframe;
	pheader->ofs_glcmds = (int) poutglcmd - (int) pheader;
	for (i = 0;i < pheader->num_glcmds;i++)
		*poutglcmd++ = LittleLong(*pinglcmd++);

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

void swapintblock(int *m, int size)
{
	size /= 4;
	while(size--)
		*m++ = BigLong(*m);
}

void Mod_LoadZymoticModel (model_t *mod, void *buffer)
{
	int i, pbase, start, end, total, *skinrange;
	rtexture_t **texture, **skin;
	char *shadername;
	zymtype1header_t *pinmodel, *pheader;
	zymscene_t *scene;
	zymbone_t *bone;
	animscene_t *animscenes;

	start = Hunk_LowMark ();

	pinmodel = (void *)buffer;

	if (memcmp(pinmodel->id, "ZYMOTICMODEL", 12))
		Host_Error ("Mod_LoadZymoticModel: %s is not a zymotic model\n");

	if (BigLong(pinmodel->type) != 1)
		Host_Error ("Mod_LoadZymoticModel: only type 1 (skeletal pose) models are currently supported\n");

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_ZYM;

	pheader = Hunk_AllocName (BigLong(pinmodel->filesize), va("%s Zymotic model", loadname));

	pbase = (int) pheader;

	memcpy(pheader, pinmodel, BigLong(pinmodel->filesize));

	// byteswap header
	memcpy(pheader->id, pinmodel->id, 12);
	pheader->type = BigLong(pheader->type);
	pheader->filesize = BigLong(pheader->filesize);
	pheader->mins[0] = BigFloat(pheader->mins[0]);
	pheader->mins[1] = BigFloat(pheader->mins[1]);
	pheader->mins[2] = BigFloat(pheader->mins[2]);
	pheader->maxs[0] = BigFloat(pheader->maxs[0]);
	pheader->maxs[1] = BigFloat(pheader->maxs[1]);
	pheader->maxs[2] = BigFloat(pheader->maxs[2]);
	pheader->radius = BigFloat(pheader->radius);
	pheader->numverts = BigLong(pheader->numverts);
	pheader->numtris = BigLong(pheader->numtris);
	pheader->numshaders = BigLong(pheader->numshaders);
	pheader->numbones = BigLong(pheader->numbones);
	pheader->numscenes = BigLong(pheader->numscenes);


	pheader->lump_scenes.start = BigLong(pheader->lump_scenes.start);pheader->lump_scenes.length = BigLong(pheader->lump_scenes.length);
	pheader->lump_poses.start = BigLong(pheader->lump_poses.start);pheader->lump_poses.length = BigLong(pheader->lump_poses.length);
	pheader->lump_bones.start = BigLong(pheader->lump_bones.start);pheader->lump_bones.length = BigLong(pheader->lump_bones.length);
	pheader->lump_vertbonecounts.start = BigLong(pheader->lump_vertbonecounts.start);pheader->lump_vertbonecounts.length = BigLong(pheader->lump_vertbonecounts.length);
	pheader->lump_verts.start = BigLong(pheader->lump_verts.start);pheader->lump_verts.length = BigLong(pheader->lump_verts.length);
	pheader->lump_texcoords.start = BigLong(pheader->lump_texcoords.start);pheader->lump_texcoords.length = BigLong(pheader->lump_texcoords.length);
	pheader->lump_render.start = BigLong(pheader->lump_render.start);pheader->lump_render.length = BigLong(pheader->lump_render.length);
	pheader->lump_shaders.start = BigLong(pheader->lump_shaders.start);pheader->lump_shaders.length = BigLong(pheader->lump_shaders.length);
	pheader->lump_trizone.start = BigLong(pheader->lump_trizone.start);pheader->lump_trizone.length = BigLong(pheader->lump_trizone.length);

	mod->flags = 0; // there are no flags
	mod->numframes = pheader->numscenes;
	mod->synctype = ST_SYNC;
	mod->numtris = pheader->numtris;

	// FIXME: add skin support and texturing and shaders and...
// load the skins
	skinrange = loadmodel->skinanimrange;
	skin = loadmodel->skinanim;
//	skinrange = Hunk_AllocName (sizeof(int) * (pheader->num_skins * 2), loadname);	
//	skin = skinrange + pheader->num_skins * 2;
//	loadmodel->skinanimrange = (int) skinrange - (int) pheader;
//	loadmodel->skinanim = (int) skin - (int) pheader;
	*skinrange++ = 0;
	*skinrange++ = 1;
	*skin++ = NULL;
	*skin++ = NULL;
	*skin++ = NULL;
	*skin++ = NULL;
	*skin++ = NULL;
	loadmodel->numskins = 1;

	// go through the lumps, swapping things

//	zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
	scene = (void *) (pheader->lump_scenes.start + pbase);
	animscenes = Hunk_AllocName(sizeof(animscene_t) * mod->numframes, va("%s sceneinfo", loadname));
	for (i = 0;i < pheader->numscenes;i++)
	{
		scene->mins[0] = BigFloat(scene->mins[0]);
		scene->mins[1] = BigFloat(scene->mins[1]);
		scene->mins[2] = BigFloat(scene->mins[2]);
		scene->maxs[0] = BigFloat(scene->maxs[0]);
		scene->maxs[1] = BigFloat(scene->maxs[1]);
		scene->maxs[2] = BigFloat(scene->maxs[2]);
		scene->radius = BigFloat(scene->radius);
		scene->framerate = BigFloat(scene->framerate);
		scene->flags = BigLong(scene->flags);
		scene->start = BigLong(scene->start);
		scene->length = BigLong(scene->length);

		memcpy(animscenes[i].name, scene->name, 32);
		animscenes[i].firstframe = scene->start;
		animscenes[i].framecount = scene->length;
		animscenes[i].framerate = scene->framerate;
		animscenes[i].loop = (scene->flags & ZYMSCENEFLAG_NOLOOP) == 0;

		scene++;
	}
	mod->ofs_scenes = (int) animscenes - pbase;

//	zymlump_t lump_poses; // float pose[numposes][numbones][3][4]; // animation data
	swapintblock((void *) (pheader->lump_poses.start + pbase), pheader->lump_poses.length);

//	zymlump_t lump_bones; // zymbone_t bone[numbones];
	bone = (void *) (pheader->lump_bones.start + pbase);
	for (i = 0;i < pheader->numbones;i++)
	{
		bone->flags = BigLong(bone->flags);
		bone->parent = BigLong(bone->parent);
		bone++;
	}

//	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	swapintblock((void *) (pheader->lump_vertbonecounts.start + pbase), pheader->lump_vertbonecounts.length);

//	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	swapintblock((void *) (pheader->lump_verts.start + pbase), pheader->lump_verts.length);

//	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	swapintblock((void *) (pheader->lump_texcoords.start + pbase), pheader->lump_texcoords.length);

//	zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	swapintblock((void *) (pheader->lump_render.start + pbase), pheader->lump_render.length);

//	zymlump_t lump_shaders; // char shadername[numshaders][32]; // shaders used on this model
	shadername = (void *) (pheader->lump_shaders.start + pbase);
	texture = (void *) shadername;
	for (i = 0;i < pheader->numshaders;i++)
	{
		rtexture_t *rt;
		rt = loadtextureimage(shadername, 0, 0, true, r_mipskins.value, true);
		shadername += 32;
		*texture++ = rt; // reuse shader name list for texture pointers
	}

//	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
	swapintblock((void *) (pheader->lump_trizone.start + pbase), pheader->lump_trizone.length);

	// model bbox
	for (i = 0;i < 3;i++)
	{
		mod->mins[i] = pheader->mins[i];
		mod->maxs[i] = pheader->maxs[i];
	}

// move the complete, relocatable alias model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}
