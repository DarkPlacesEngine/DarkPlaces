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

cvar_t r_mipsprites = {"r_mipsprites", "1", true};

/*
===============
Mod_SpriteInit
===============
*/
void Mod_SpriteInit (void)
{
	Cvar_RegisterVariable(&r_mipsprites);
}

void Mod_Sprite_StripExtension(char *in, char *out)
{
	char *end;
	end = in + strlen(in);
	if ((end - in) >= 6)
		if (strcmp(end - 6, ".spr32") == 0)
			end -= 6;
	if ((end - in) >= 4)
		if (strcmp(end - 4, ".spr") == 0)
			end -= 4;
	while (in < end)
		*out++ = *in++;
	*out++ = 0;
}

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t *frame, int framenum, int bytesperpixel)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	int					i, width, height, size, origin[2];
	char				name[256], tempname[256];
	byte				*pixbuf, *pixel, *inpixel;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height * bytesperpixel;

	pspriteframe = frame;

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

//	pspriteframe->width = width;
//	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	Mod_Sprite_StripExtension(loadmodel->name, tempname);
	sprintf (name, "%s_%i", tempname, framenum);
	pspriteframe->texture = loadtextureimagewithmask(name, 0, 0, false, r_mipsprites.value, true);
	pspriteframe->fogtexture = image_masktex;
	if (!pspriteframe->texture)
	{
		pspriteframe->texture = R_LoadTexture (name, width, height, (byte *)(pinframe + 1), TEXF_ALPHA | (bytesperpixel > 1 ? TEXF_RGBA : 0) | (r_mipsprites.value ? TEXF_MIPMAP : 0) | TEXF_PRECACHE);
		// make fog version (just alpha)
		pixbuf = pixel = qmalloc(width*height*4);
		inpixel = (byte *)(pinframe + 1);
		if (bytesperpixel == 1)
		{
			for (i = 0;i < width*height;i++)
			{
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = 255;
				if (*inpixel++ != 255)
					*pixel++ = 255;
				else
					*pixel++ = 0;
			}
		}
		else
		{
			inpixel+=3;
			for (i = 0;i < width*height;i++)
			{
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = 255;
				*pixel++ = *inpixel;
				inpixel+=4;
			}
		}
		sprintf (name, "%s_%ifog", loadmodel->name, framenum);
		pspriteframe->fogtexture = R_LoadTexture (name, width, height, pixbuf, TEXF_ALPHA | TEXF_RGBA | (r_mipsprites.value ? TEXF_MIPMAP : 0) | TEXF_PRECACHE);
		qfree(pixbuf);
	}

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void *Mod_LoadSpriteGroup (void * pin, mspriteframe_t *frame, int numframes, int framenum, int bytesperpixel)
{
	int i;
	void *ptemp;

	ptemp = (void *)(sizeof(dspriteinterval_t) * numframes + sizeof(dspritegroup_t) + (int) pin);

	for (i = 0;i < numframes;i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, frame++, framenum * 100 + i, bytesperpixel);

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, realframes, size, bytesperpixel, start, end, total, maxwidth, maxheight;
	dsprite_t			*pin;
	msprite_t			*psprite;
	dspriteframetype_t	*pframetype;
	dspriteframe_t		*pframe;
	animscene_t			*animscenes;
	mspriteframe_t		*frames;
	dspriteframe_t		**framedata;

	start = Hunk_LowMark ();

	mod->flags = EF_FULLBRIGHT;
	// LordHavoc: hack to allow sprites to be non-fullbright
	for (i = 0;i < MAX_QPATH && mod->name[i];i++)
	{
		if (mod->name[i] == '!')
		{
			mod->flags &= ~EF_FULLBRIGHT;
			break;
		}
	}

	// build a list of frames while parsing
	framedata = qmalloc(65536*sizeof(dspriteframe_t));

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version == 2)
	{
		version = 32;
		Con_Printf("warning: %s is a version 2 sprite (RGBA), supported for now, please hex edit to version 32 incase HalfLife sprites might be supported at some point.\n", mod->name);
	}
	// LordHavoc: 32bit textures
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION)
		Host_Error ("%s has wrong version number (%i should be %i or %i)", mod->name, version, SPRITE_VERSION, SPRITE32_VERSION);
	bytesperpixel = 1;
	if (version == SPRITE32_VERSION)
		bytesperpixel = 4;

	numframes = LittleLong (pin->numframes);

	psprite = Hunk_AllocName (sizeof(msprite_t), va("%s info", loadname));

//	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	maxwidth = LittleLong (pin->width);
	maxheight = LittleLong (pin->height);
//	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
//	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = maxwidth/2;
	mod->mins[2] = -maxheight/2;
	mod->maxs[2] = maxheight/2;
	
//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	animscenes = Hunk_AllocName(sizeof(animscene_t) * mod->numframes, va("%s sceneinfo", loadname));

	realframes = 0;

	for (i = 0;i < numframes;i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		sprintf(animscenes[i].name, "frame%i", i);
		animscenes[i].firstframe = realframes;
		animscenes[i].loop = true;
		if (frametype == SPR_SINGLE)
		{
			animscenes[i].framecount = 1;
			animscenes[i].framerate = 10;
			pframe = (dspriteframe_t *) (pframetype + 1);
			framedata[realframes] = pframe;
			size = LittleLong(pframe->width) * LittleLong(pframe->height) * bytesperpixel;
			pframetype = (dspriteframetype_t *) (size + sizeof(dspriteframe_t) + (int) pframe);
			realframes++;
		}
		else
		{
			j = LittleLong(((dspritegroup_t *) (sizeof(dspriteframetype_t) + (int) pframetype))->numframes);
			animscenes[i].framecount = j;
			// FIXME: support variable framerate?
			animscenes[i].framerate = 1.0f / LittleFloat(((dspriteinterval_t *) (sizeof(dspritegroup_t) + sizeof(dspriteframetype_t) + (int) pframetype))->interval);
			pframe = (dspriteframe_t *) (sizeof(dspriteinterval_t) * j + sizeof(dspritegroup_t) + sizeof(dspriteframetype_t) + (int) pframetype);
			while (j--)
			{
				framedata[realframes] = pframe;
				size = LittleLong(pframe->width) * LittleLong(pframe->height) * bytesperpixel;
				pframe = (dspriteframe_t *) (size + sizeof(dspriteframe_t) + (int) pframe);
				realframes++;
			}
			pframetype = (dspriteframetype_t *) pframe;
		}
	}

	mod->ofs_scenes = (int) animscenes - (int) psprite;

	frames = Hunk_AllocName(sizeof(mspriteframe_t) * realframes, va("%s frames", loadname));

	realframes = 0;
	for (i = 0;i < numframes;i++)
	{
		for (j = 0;j < animscenes[i].framecount;j++)
		{
			Mod_LoadSpriteFrame (framedata[realframes], frames + realframes, i, bytesperpixel);
			realframes++;
		}
	}

	psprite->ofs_frames = (int) frames - (int) psprite;

	qfree(framedata);

	mod->type = mod_sprite;

// move the complete, relocatable sprite model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, psprite, total);

	Hunk_FreeToLowMark (start);
}
