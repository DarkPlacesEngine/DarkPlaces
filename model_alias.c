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

aliashdr_t	*pheader;

typedef struct
{
	int v[3];
	vec3_t normal;
} temptris_t;
temptris_t *temptris;
//stvert_t	stverts[MAXALIASVERTS];
//mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
//trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

byte		**player_8bit_texels_tbl;
byte		*player_8bit_texels;

float		aliasbboxmin[3], aliasbboxmax[3]; // LordHavoc: proper bounding box considerations

void Mod_ConvertAliasVerts (int numverts, int numtris, vec3_t scale, vec3_t translate, trivertx_t *v, trivert2 *out)
{
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
}

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	int				i;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i]; // LordHavoc: was setting bboxmin a second time (bug)
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

//	poseverts[posenum] = pinframe;
	Mod_ConvertAliasVerts(pheader->numverts, pheader->numtris, pheader->scale, pheader->scale_origin, pinframe, (void *)((int) pheader + pheader->posedata + sizeof(trivert2) * pheader->numverts * posenum));
//	// LordHavoc: copy the frame data
//	memcpy((void *)((int) pheader + pheader->posedata + sizeof(trivertx_t) * pheader->numverts * posenum), pinframe, sizeof(trivertx_t) * pheader->numverts);
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i]; // LordHavoc: was setting bboxmin a second time (bug)
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
//		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		Mod_ConvertAliasVerts(pheader->numverts, pheader->numtris, pheader->scale, pheader->scale_origin, (void *)((daliasframe_t *)ptemp + 1), (void *)((int) pheader + pheader->posedata + sizeof(trivert2) * pheader->numverts * posenum));
//		// LordHavoc: copy the frame data
//		memcpy((void *)((int) pheader + pheader->posedata + sizeof(trivertx_t) * pheader->numverts * posenum), (void *)((daliasframe_t *)ptemp + 1), sizeof(trivertx_t) * pheader->numverts);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
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

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype, int bytesperpixel)
{
	int		i, j, k;
	char	name[32];
	int		s;
	byte	*skin;
	byte	*texels;
	daliasskingroup_t		*pinskingroup;
	int		groupskins;
	daliasskininterval_t	*pinskinintervals;
	
	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	for (i = 0;i < numskins;i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			if (bytesperpixel == 1)
				Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
	//		if (!strcmp(loadmodel->name,"progs/player.mdl")) {
				texels = Hunk_AllocName(s, loadname);
				pheader->texels[i] = texels - (byte *)pheader;
				memcpy (texels, (byte *)(pskintype + 1), s);
	//		}
			sprintf (name, "%s_%i", loadmodel->name, i);
			pheader->gl_texturenum[i][0] =
			pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] =
			pheader->gl_texturenum[i][3] =
				GL_LoadTexture (name, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), true, false, bytesperpixel);
			pskintype = (daliasskintype_t *)((byte *)(pskintype+1) + s);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for (j = 0;j < groupskins;j++)
			{
					if (bytesperpixel == 1)
						Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
					if (j == 0)
					{
						texels = Hunk_AllocName(s, loadname);
						pheader->texels[i] = texels - (byte *)pheader;
						memcpy (texels, (byte *)(pskintype), s);
					}
					sprintf (name, "%s_%i_%i", loadmodel->name, i,j);
					pheader->gl_texturenum[i][j&3] = 
						GL_LoadTexture (name, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), true, false, bytesperpixel);
					pskintype = (daliasskintype_t *)((byte *)(pskintype) + s);
			}
			k = j;
			for (;j < 4;j++)
				pheader->gl_texturenum[i][j&3] = pheader->gl_texturenum[i][j - k]; 
		}
	}

	return (void *)pskintype;
}

//=========================================================================

//void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

/*
=================
Mod_LoadAliasModel
=================
*/
#define BOUNDI(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid VALUE (%d exceeds %d - %d)\n", mod->name, VALUE, MIN, MAX);
#define BOUNDF(VALUE,MIN,MAX) if (VALUE < MIN || VALUE >= MAX) Host_Error("model %s has an invalid VALUE (%g exceeds %g - %g)\n", mod->name, VALUE, MIN, MAX);
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, size, start, end, total;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	// LordHavoc: 32bit textures
	int					bytesperpixel;
	unsigned short		*poutvertindices;
	float				*pouttexcoords, scales, scalet;
	temptris_t			*tris;

	start = Hunk_LowMark ();

	if (!temptris)
		temptris = malloc(sizeof(temptris_t) * MD2MAX_TRIANGLES);

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION && version != ALIAS32_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i or %i)",
				 mod->name, version, ALIAS_VERSION, ALIAS32_VERSION);

	mod->type = ALIASTYPE_MDL;

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
//	size = sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) * sizeof (pinmodel->frames[0]));
	size = sizeof (aliashdr_t);
	size += LittleLong (pinmodel->numverts) * sizeof(float[2][2]);
	size += LittleLong (pinmodel->numtris) * sizeof(unsigned short[3]);
	size += LittleLong (pinmodel->numframes) * (sizeof(trivert2) * LittleLong (pinmodel->numverts) + sizeof(maliasframedesc_t));
	BOUNDI(size,256,4194304);
	pheader = Hunk_AllocName (size, loadname);
	
	mod->flags = LittleLong (pinmodel->flags);
	mod->type = mod_alias;

// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	BOUNDF(pheader->boundingradius,0,65536);
	pheader->numskins = LittleLong (pinmodel->numskins);
	BOUNDI(pheader->numskins,0,256);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	BOUNDI(pheader->skinwidth,0,4096);
	pheader->skinheight = LittleLong (pinmodel->skinheight);
	BOUNDI(pheader->skinheight,0,1024);
//LordHavoc: 32bit textures
	bytesperpixel = version == ALIAS32_VERSION ? 4 : 1;

//	if (pheader->skinheight > MAX_LBM_HEIGHT)
//		Host_Error ("model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);
	BOUNDI(pheader->numverts,0,MAXALIASVERTS);
	/*
	if (pheader->numverts <= 0)
		Host_Error ("model %s has no vertices", mod->name);
	if (pheader->numverts > MAXALIASVERTS)
		Host_Error ("model %s has too many vertices", mod->name);
	*/

	pheader->numtris = LittleLong (pinmodel->numtris);
	BOUNDI(pheader->numtris,0,65536);
//	if (pheader->numtris <= 0)
//		Host_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	BOUNDI(pheader->numframes,0,65536);
	numframes = pheader->numframes;
//	if (numframes < 1)
//		Host_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	BOUNDF(pheader->size,0,65536);
	mod->synctype = LittleLong (pinmodel->synctype);
	BOUNDI(pheader->synctype,0,2);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		BOUNDF(pheader->scale[i],0,65536);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		BOUNDF(pheader->scale_origin[i],-65536,65536);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
		BOUNDF(pheader->eyeposition[i],-65536,65536);
	}

// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype, bytesperpixel);

// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;
	pouttexcoords = (float *)&pheader->frames[numframes];
	pheader->texcoords = (int) pouttexcoords - (int) pheader;

	// LordHavoc: byteswap and convert stvert data
	scales = 1.0 / pheader->skinwidth;
	scalet = 1.0 / pheader->skinheight;
	for (i = 0;i < pheader->numverts;i++)
	{
		pouttexcoords[i*2] = LittleLong (pinstverts[i].s) * scales;
		pouttexcoords[i*2+1] = LittleLong (pinstverts[i].t) * scalet;
		pouttexcoords[(i+pheader->numverts)*2] = LittleLong (pinstverts[i].s) * scales + 0.5;
		pouttexcoords[(i+pheader->numverts)*2+1] = LittleLong (pinstverts[i].t) * scalet;
		if (pouttexcoords[i*2] >= 0.5) // already a back side coordinate
		{
			pouttexcoords[i*2] -= 0.5;
			pouttexcoords[(i+pheader->numverts)*2] -= 0.5;
		}
		BOUNDF(pouttexcoords[i*2],0,1);
		BOUNDF(pouttexcoords[i*2+1],0,1);
		BOUNDF(pouttexcoords[(i+pheader->numverts)*2],0,1);
		BOUNDF(pouttexcoords[(i+pheader->numverts)*2+1],0,1);
	}

// load triangle data
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];
	poutvertindices = (unsigned short *)&pouttexcoords[pheader->numverts*4];
	pheader->vertindices = (int) poutvertindices - (int) pheader;
	// LordHavoc: sort triangles into front and back lists
	// so they can be drawn refering to different texture coordinate arrays,
	// but sharing vertex data
	pheader->frontfaces = 0;
	pheader->backfaces = 0;
	tris = temptris;
	for (i=0 ; i<pheader->numtris ; i++)
	{
		if (LittleLong(pintriangles[i].facesfront))
		{
			pheader->frontfaces++;
			for (j=0 ; j<3 ; j++)
				*poutvertindices++ = LittleLong (pintriangles[i].vertindex[j]);
		}
		for (j=0 ; j<3 ; j++)
			tris->v[j] = LittleLong (pintriangles[i].vertindex[j]);
		tris++;
	}
	for (i=0 ; i<pheader->numtris ; i++)
	{
		if (!LittleLong(pintriangles[i].facesfront))
		{
			pheader->backfaces++;
			for (j=0 ; j<3 ; j++)
				*poutvertindices++ = LittleLong (pintriangles[i].vertindex[j]);
		}
	}

// load the frames
	posenum = 0;
	pheader->posedata = (int) poutvertindices - (int) pheader;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	// LordHavoc: doing proper bbox for model
	aliasbboxmin[0] = aliasbboxmin[1] = aliasbboxmin[2] = 1000000000;
	aliasbboxmax[0] = aliasbboxmax[1] = aliasbboxmax[2] = -1000000000;

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
	}

	pheader->numposes = posenum;

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
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}

/*
=================
Mod_LoadQ2AliasModel
=================
*/
int loadtextureimage (int texnum, char* filename, qboolean complain, int matchwidth, int matchheight);
void Mod_LoadQ2AliasModel (model_t *mod, void *buffer)
{
	int					i, j, version, size, *pinglcmd, *poutglcmd, start, end, total, framesize;
	md2_t				*pinmodel;
	md2mem_t			*pheader;
	md2triangle_t		*pintriangles, *pouttriangles;
	md2frame_t			*pinframe;
	md2memframe_t		*poutframe;
	char				*pinskins;
	temptris_t			*tris;

	start = Hunk_LowMark ();

	if (!temptris)
		temptris = malloc(sizeof(temptris_t) * MD2MAX_TRIANGLES);

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
	if (LittleLong(pinmodel->num_frames < 1) || LittleLong(pinmodel->num_frames) > 256) //MD2MAX_FRAMES)
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
		pinskins = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_skins));
		for (i = 0;i < pheader->num_skins;i++)
		{
			pheader->gl_texturenum[i] = loadtextureimage (-1, pinskins, TRUE, 0, 0);
			pinskins += MD2MAX_SKINNAME;
		}
	}

// load triangles
	pintriangles = (void*)((int) pinmodel + LittleLong(pinmodel->ofs_tris));
	pouttriangles = (void*)&pheader[1];
	pheader->ofs_tris = (int) pouttriangles - (int) pheader;
	tris = temptris;
	// swap the triangle list
	for (i=0 ; i<pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			tris->v[j] = pouttriangles->index_xyz[j] = LittleShort (pintriangles->index_xyz[j]);
			pouttriangles->index_st[j] = LittleShort (pintriangles->index_st[j]);
			if (pouttriangles->index_xyz[j] >= pheader->num_xyz)
				Host_Error ("%s has invalid vertex indices", mod->name);
			if (pouttriangles->index_st[j] >= pheader->num_st)
				Host_Error ("%s has invalid vertex indices", mod->name);
		}
		pintriangles++;
		pouttriangles++;
		tris++;
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
		Mod_ConvertAliasVerts (pheader->num_xyz, pheader->num_tris, poutframe->scale, poutframe->translate, &pinframe->verts[0], &poutframe->verts[0]);
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
