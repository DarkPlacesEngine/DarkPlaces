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

/*
===============
Mod_AliasInit
===============
*/
void Mod_AliasInit (void)
{
}

//aliashdr_t	*pheader;

//typedef struct
//{
//	int v[3];
//	vec3_t normal;
//} temptris_t;
//temptris_t *temptris;
//stvert_t	stverts[MAXALIASVERTS];
//mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
//trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

byte		**player_8bit_texels_tbl;
byte		*player_8bit_texels;

float		aliasbboxmin[3], aliasbboxmax[3]; // LordHavoc: proper bounding box considerations

#define MAXVERTS 8192
float vertst[MAXVERTS][2];
int vertusage[MAXVERTS];
int vertonseam[MAXVERTS];
int vertremap[MAXVERTS];
unsigned short temptris[MAXVERTS][3];

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
void Mod_ConvertAliasVerts (int inverts, vec3_t scale, vec3_t translate, trivertx_t *v, trivert2 *out)
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
			out[j].n[0] = (signed char) (r_avertexnormals[v[i].lightnormalindex][0] * 127.0);
			out[j].n[1] = (signed char) (r_avertexnormals[v[i].lightnormalindex][1] * 127.0);
			out[j].n[2] = (signed char) (r_avertexnormals[v[i].lightnormalindex][2] * 127.0);
		}
		j = vertremap[i+inverts]; // onseam
		if (j >= 0)
		{
			VectorCopy(v[i].v, out[j].v);
			out[j].n[0] = (signed char) (r_avertexnormals[v[i].lightnormalindex][0] * 127.0);
			out[j].n[1] = (signed char) (r_avertexnormals[v[i].lightnormalindex][1] * 127.0);
			out[j].n[2] = (signed char) (r_avertexnormals[v[i].lightnormalindex][2] * 127.0);
		}
	}
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void *pin, maliasframe_t *frame, maliashdr_t *mheader, int inverts, int outverts, trivert2 **posevert)
{
	trivertx_t		*pinframe;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy(frame->name, pdaliasframe->name);
	frame->start = posenum;
	frame->length = 1;
	frame->rate = 10.0f; // unnecessary but...

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
void *Mod_LoadAliasGroup (void *pin, maliasframe_t *frame, maliashdr_t *mheader, int inverts, int outverts, trivert2 **posevert)
{
	int		i, numframes;
	void	*ptemp;
	float	interval;
	
	numframes = LittleLong (((daliasgroup_t *)pin)->numframes);

	strcpy(frame->name, "group");
	frame->start = posenum;
	frame->length = numframes;
	interval = LittleFloat (((daliasinterval_t *)(((daliasgroup_t *)pin) + 1))->interval); // FIXME: support variable framerate groups?
	if (interval < 0.01f)
		Host_Error("Mod_LoadAliasGroup: invalid interval");
	frame->rate = 1.0f / interval;

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

extern unsigned d_8to24table[];

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

int GL_SkinSplitShirt(byte *in, byte *out, int width, int height, unsigned short bits, char *name)
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
		return GL_LoadTexture (name, width, height, out - width*height, true, false, 1);
	else
		return 0;
}

int GL_SkinSplit(byte *in, byte *out, int width, int height, unsigned short bits, char *name)
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
		return GL_LoadTexture (name, width, height, out - width*height, true, false, 1);
	else
		return 0;
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype, int width, int height)
{
	int		i, j;
	char	name[32];
	int		s;
	byte	*skin;
	daliasskingroup_t		*pinskingroup;
	int		groupskins;
	daliasskininterval_t	*pinskinintervals;
	int		skinranges, skincount, *skintexnum, *skinrange, skinnum;
	void	*temp;
	byte	*skintemp = NULL;
	
	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = width * height;
	skintemp = malloc(s);

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
	skintexnum = loadmodel->skinanim;
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

			Mod_FloodFillSkin( skin, width, height );
			*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FBD, va("%s_normal", name)); // normal (no special colors)
			*skintexnum++ = GL_SkinSplitShirt((byte *)pskintype, skintemp, width, height, 0x0040, va("%s_pants",  name)); // pants
			*skintexnum++ = GL_SkinSplitShirt((byte *)pskintype, skintemp, width, height, 0x0002, va("%s_shirt",  name)); // shirt
			*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0xC000, va("%s_glow",   name)); // glow
			*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FFF, va("%s_body",   name)); // body (normal + pants + shirt, but not glow)
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
				sprintf (name, "%s_%i_%i", loadmodel->name, i,j);

				Mod_FloodFillSkin( skin, width, height );
				*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FBD, va("%s_normal", name)); // normal (no special colors)
				*skintexnum++ = GL_SkinSplitShirt((byte *)pskintype, skintemp, width, height, 0x0040, va("%s_pants",  name)); // pants
				*skintexnum++ = GL_SkinSplitShirt((byte *)pskintype, skintemp, width, height, 0x0002, va("%s_shirt",  name)); // shirt
				*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0xC000, va("%s_glow",   name)); // glow
				*skintexnum++ = GL_SkinSplit((byte *)pskintype, skintemp, width, height, 0x3FFF, va("%s_body",   name)); // body (normal + pants + shirt, but not glow)
				pskintype = (daliasskintype_t *)((byte *)(pskintype) + s);
			}
		}
	}
	loadmodel->numskins = numskins;
	free(skintemp);

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
#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid VALUE (%d exceeds %d - %d)\n", mod->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid VALUE (%f exceeds %f - %f)\n", mod->name, VALUE, MIN, MAX);
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
	maliasframe_t		*frame;
	trivert2			*posevert;

	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	mod->type = ALIASTYPE_MDL;

	numframes = LittleLong(pinmodel->numframes);
	BOUNDI(numframes,0,65536);
	numverts = LittleLong(pinmodel->numverts);
	BOUNDI(numverts,0,MAXALIASVERTS);
	numtris = LittleLong(pinmodel->numtris);
	BOUNDI(numtris,0,65536);
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
	mheader = Hunk_AllocName (sizeof(maliashdr_t), loadname);
	mod->flags = LittleLong (pinmodel->flags);
	mod->type = mod_alias;
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
	pskintype = Mod_LoadAllSkins(numskins, pskintype, skinwidth, skinheight);

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
	pouttris = Hunk_AllocName(sizeof(unsigned short[3]) * numtris, loadname);
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
	pouttexcoords = Hunk_AllocName(sizeof(float[2]) * totalverts, loadname);
	mheader->texdata = (int) pouttexcoords - (int) mheader;
	for (i = 0;i < totalverts;i++)
	{
		*pouttexcoords++ = vertst[i][0];
		*pouttexcoords++ = vertst[i][1];
	}

// load the frames
	posenum = 0;
	frame = Hunk_AllocName(sizeof(maliasframe_t) * numframes, loadname);
	mheader->framedata = (int) frame - (int) mheader;
	posevert = Hunk_AllocName(sizeof(trivert2) * numposes * totalverts, loadname);
	mheader->posedata = (int) posevert - (int) mheader;
	pframetype = (daliasframetype_t *)&pintriangles[numtris];

	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;

	for (i=0 ; i<numframes ; i++)
	{
		if ((aliasframetype_t) LittleLong (pframetype->type) == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, frame++, mheader, numverts, totalverts, &posevert);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, frame++, mheader, numverts, totalverts, &posevert);
	}

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
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, mheader, total);

	Hunk_FreeToLowMark (start);
}

void Mod_ConvertQ2AliasVerts (int numverts, vec3_t scale, vec3_t translate, trivertx_t *v, trivert2 *out)
{
	int i;
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
		out[i].n[0] = (signed char) (r_avertexnormals[v[i].lightnormalindex][0] * 127.0);
		out[i].n[1] = (signed char) (r_avertexnormals[v[i].lightnormalindex][1] * 127.0);
		out[i].n[2] = (signed char) (r_avertexnormals[v[i].lightnormalindex][2] * 127.0);
	}		
	/*
	int i, j;
	vec3_t t1, t2;
	struct
	{
		vec3_t v;
		vec3_t normal;
		int count;
	} tempvert[MD2MAX_VERTS];
	temptris_t *tris;
	// decompress vertices
	for (i = 0;i < numverts;i++)
	{
		VectorCopy(v[i].v, out[i].v);
		tempvert[i].v[0] = v[i].v[0] * scale[0] + translate[0];
		tempvert[i].v[1] = v[i].v[1] * scale[1] + translate[1];
		tempvert[i].v[2] = v[i].v[2] * scale[2] + translate[2];
		tempvert[i].normal[0] = tempvert[i].normal[1] = tempvert[i].normal[2] = 0;
		tempvert[i].count = 0;
		// update bounding box
		if (tempvert[i].v[0] < aliasbboxmin[0]) aliasbboxmin[0] = tempvert[i].v[0];
		if (tempvert[i].v[1] < aliasbboxmin[1]) aliasbboxmin[1] = tempvert[i].v[1];
		if (tempvert[i].v[2] < aliasbboxmin[2]) aliasbboxmin[2] = tempvert[i].v[2];
		if (tempvert[i].v[0] > aliasbboxmax[0]) aliasbboxmax[0] = tempvert[i].v[0];
		if (tempvert[i].v[1] > aliasbboxmax[1]) aliasbboxmax[1] = tempvert[i].v[1];
		if (tempvert[i].v[2] > aliasbboxmax[2]) aliasbboxmax[2] = tempvert[i].v[2];
	}
	// calculate surface normals
	tris = temptris;
	for (i = 0;i < numtris;i++)
	{
		VectorSubtract(tempvert[tris->v[0]].v, tempvert[tris->v[1]].v, t1);
		VectorSubtract(tempvert[tris->v[2]].v, tempvert[tris->v[1]].v, t2);
		CrossProduct(t1, t2, tris->normal);
		VectorNormalize(tris->normal);
		// add surface normal to vertices
		for (j = 0;j < 3;j++)
		{
			VectorAdd(tris->normal, tempvert[tris->v[j]].normal, tempvert[tris->v[j]].normal);
			tempvert[tris->v[j]].count++;
		}
		tris++;
	}
	// average normals and write out 1.7bit format
	for (i = 0;i < pheader->numtris;i++)
	{
		VectorNormalize(tempvert[i].normal);
		out[i].n[0] = (signed char) (tempvert[i].normal[0] * 127.0);
		out[i].n[1] = (signed char) (tempvert[i].normal[1] * 127.0);
		out[i].n[2] = (signed char) (tempvert[i].normal[2] * 127.0);
	}
	*/
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
	md2memframe_t		*poutframe;
	char				*pinskins;
//	temptris_t			*tris;

	start = Hunk_LowMark ();

//	if (!temptris)
//		temptris = malloc(sizeof(temptris_t) * MD2MAX_TRIANGLES);

	pinmodel = (md2_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != MD2ALIAS_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2ALIAS_VERSION);

	mod->type = mod_alias;
	mod->aliastype = ALIASTYPE_MD2;

	framesize = sizeof(md2memframe_t) + LittleLong(pinmodel->num_xyz) * sizeof(trivert2);
	// LordHavoc: calculate size for in memory version
	size = sizeof(md2mem_t)
		 + LittleLong(pinmodel->num_st) * sizeof(md2stvert_t)
		 + LittleLong(pinmodel->num_tris) * sizeof(md2triangle_t)
		 + LittleLong(pinmodel->num_frames) * framesize
		 + LittleLong(pinmodel->num_glcmds) * sizeof(int);
	if (size <= 0 || size >= MD2MAX_SIZE)
		Host_Error ("%s is not a valid model", mod->name);
	pheader = Hunk_AllocName (size, loadname);
	
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
	if (LittleLong(pinmodel->num_skins < 0) || LittleLong(pinmodel->num_skins) > MD2MAX_SKINS)
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
		int *skin, *skinrange;
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
			*skin++ = loadtextureimage (pinskins, 0, 0, true, true);
			*skin++ = 0; // the extra 4 layers are currently unused
			*skin++ = 0;
			*skin++ = 0;
			*skin++ = 0;
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
	for (i=0 ; i<pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
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
	for (i=0 ; i<pheader->num_frames ; i++)
	{
		for (j = 0;j < 3;j++)
		{
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}
		Mod_ConvertQ2AliasVerts (pheader->num_xyz, poutframe->scale, poutframe->translate, &pinframe->verts[0], &poutframe->verts[0]);
		pinframe = (void*) &pinframe->verts[j];
		poutframe = (void*) &poutframe->verts[j];
	}

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
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}
