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

/*
===============
Mod_SpriteInit
===============
*/
void Mod_SpriteInit (void)
{
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
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum, int bytesperpixel)
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

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t),loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	Mod_Sprite_StripExtension(loadmodel->name, tempname);
	sprintf (name, "%s_%i", tempname, framenum);
	pspriteframe->gl_texturenum = loadtextureimagewithmask(name, 0, 0, false, true);
	pspriteframe->gl_fogtexturenum = image_masktexnum;
	if (pspriteframe->gl_texturenum == 0)
	{
		pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), true, true, bytesperpixel);
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
		pspriteframe->gl_fogtexturenum = GL_LoadTexture (name, width, height, pixbuf, true, true, 4);
		qfree(pixbuf);
	}

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum, int bytesperpixel)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i, bytesperpixel);

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	// LordHavoc: 32bit textures
	int		bytesperpixel;

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

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version == 2)
	{
		version = 32;
		Con_Printf("warning: %s is a version 2 sprite (RGBA), supported for now, please hex edit to version 32 incase HalfLife sprites might be supported at some point.\n", mod->name);
	}
	// LordHavoc: 32bit textures
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION)
		Host_Error ("%s has wrong version number "
				 "(%i should be %i or %i)", mod->name, version, SPRITE_VERSION, SPRITE32_VERSION);
	bytesperpixel = 1;
	if (version == SPRITE32_VERSION)
		bytesperpixel = 4;

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;
	
//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i, bytesperpixel);
		else
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i, bytesperpixel);
	}

	mod->type = mod_sprite;
}
