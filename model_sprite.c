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

cvar_t r_mipsprites = {CVAR_SAVE, "r_mipsprites", "1"};

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
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t *frame, int framenum, int bytesperpixel, byte *palette, float *modelradius)
{
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	float				dist;
	int					i, width, height, size, origin[2];
	char				name[256], tempname[256];
	byte				*pixbuf, *pixel, *inpixel;

	pinframe = (dspriteframe_t *)pin;

	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);
	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height * bytesperpixel;

	pspriteframe = frame;

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	pspriteframe->left = origin[0];
	pspriteframe->right = origin[0] + width;
	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;

	dist = pspriteframe->left*pspriteframe->left+pspriteframe->up*pspriteframe->up;
	if (*modelradius < dist)
		*modelradius = dist;
	dist = pspriteframe->right*pspriteframe->right+pspriteframe->down*pspriteframe->down;
	if (*modelradius < dist)
		*modelradius = dist;

	Mod_Sprite_StripExtension(loadmodel->name, tempname);
	sprintf (name, "%s_%i", tempname, framenum);
	pspriteframe->texture = loadtextureimagewithmask(name, 0, 0, false, r_mipsprites.value, true);
	pspriteframe->fogtexture = image_masktex;

	pixbuf = qmalloc(width*height*4);

	inpixel = (byte *)(pinframe + 1);
	pixel = pixbuf;
	if (bytesperpixel == 1)
	{
		for (i = 0;i < width * height;i++)
		{
			*pixel++ = palette[inpixel[i]*4+0];
			*pixel++ = palette[inpixel[i]*4+1];
			*pixel++ = palette[inpixel[i]*4+2];
			*pixel++ = palette[inpixel[i]*4+3];
		}
	}
	else
		memcpy(pixel, inpixel, width*height*4);

	if (!pspriteframe->texture)
	{
		pspriteframe->texture = R_LoadTexture (name, width, height, pixbuf, TEXF_ALPHA | TEXF_RGBA | (r_mipsprites.value ? TEXF_MIPMAP : 0) | TEXF_PRECACHE);
		// make fog version (just alpha)
		pixel = pixbuf;
		for (i = 0;i < width*height;i++)
		{
			*pixel++ = 255;
			*pixel++ = 255;
			*pixel++ = 255;
			pixel++;
		}
		sprintf (name, "%s_%ifog", loadmodel->name, framenum);
		pspriteframe->fogtexture = R_LoadTexture (name, width, height, pixbuf, TEXF_ALPHA | TEXF_RGBA | (r_mipsprites.value ? TEXF_MIPMAP : 0) | TEXF_PRECACHE);
	}

	qfree(pixbuf);

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
/*
void *Mod_LoadSpriteGroup (void * pin, mspriteframe_t *frame, int numframes, int framenum, int bytesperpixel)
{
	int i;
	void *ptemp;

	ptemp = (void *)(sizeof(dspriteinterval_t) * numframes + sizeof(dspritegroup_t) + (int) pin);

	for (i = 0;i < numframes;i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, frame++, framenum * 100 + i, bytesperpixel);

	return ptemp;
}
*/

// this actually handles both quake sprite and darkplaces sprite32
void Mod_LoadQuakeSprite (model_t *mod, void *buffer)
{
	int					i, j, version, numframes, realframes, size, bytesperpixel, start, end, total;
	dsprite_t			*pin;
	msprite_t			*psprite;
	dspriteframetype_t	*pframetype;
	dspriteframe_t		*pframe;
	animscene_t			*animscenes;
	mspriteframe_t		*frames;
	dspriteframe_t		**framedata;
	float				modelradius;

	modelradius = 0;

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
	// LordHavoc: 32bit textures
	bytesperpixel = 1;
	if (version == SPRITE32_VERSION)
		bytesperpixel = 4;

	numframes = LittleLong (pin->numframes);

	psprite = Hunk_AllocName (sizeof(msprite_t), va("%s info", loadname));

//	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
//	maxwidth = LittleLong (pin->width);
//	maxheight = LittleLong (pin->height);
//	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
//	psprite->numframes = numframes;

//	mod->mins[0] = mod->mins[1] = -maxwidth/2;
//	mod->maxs[0] = mod->maxs[1] = maxwidth/2;
//	mod->mins[2] = -maxheight/2;
//	mod->maxs[2] = maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadQuakeSprite: Invalid # of frames: %d\n", numframes);

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
			Mod_LoadSpriteFrame (framedata[realframes], frames + realframes, i, bytesperpixel, (byte *)&d_8to24table, &modelradius);
			realframes++;
		}
	}

	psprite->ofs_frames = (int) frames - (int) psprite;

	qfree(framedata);

	mod->type = mod_sprite;

	modelradius = sqrt(modelradius);
	for (i = 0;i < 3;i++)
	{
		mod->normalmins[i] = mod->yawmins[i] = mod->rotatedmins[i] = -modelradius;
		mod->normalmaxs[i] = mod->yawmaxs[i] = mod->rotatedmaxs[i] = modelradius;
	}
//	mod->modelradius = modelradius;

// move the complete, relocatable sprite model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, psprite, total);

	Hunk_FreeToLowMark (start);
}

void Mod_LoadHLSprite (model_t *mod, void *buffer)
{
	int					i, j, numframes, realframes, size, start, end, total, rendermode;
	byte				palette[256][4], *in;
	dspritehl_t			*pin;
	msprite_t			*psprite;
	dspriteframetype_t	*pframetype;
	dspriteframe_t		*pframe;
	animscene_t			*animscenes;
	mspriteframe_t		*frames;
	dspriteframe_t		**framedata;
	float				modelradius;

	modelradius = 0;

	start = Hunk_LowMark ();

	mod->flags = EF_FULLBRIGHT;

	// build a list of frames while parsing
	framedata = qmalloc(65536*sizeof(dspriteframe_t));

	pin = (dspritehl_t *)buffer;

	numframes = LittleLong (pin->numframes);

	psprite = Hunk_AllocName (sizeof(msprite_t), va("%s info", loadname));

	psprite->type = LittleLong (pin->type);
//	maxwidth = LittleLong (pin->width);
//	maxheight = LittleLong (pin->height);
	mod->synctype = LittleLong (pin->synctype);
	rendermode = pin->rendermode;

//	mod->mins[0] = mod->mins[1] = -maxwidth/2;
//	mod->maxs[0] = mod->maxs[1] = maxwidth/2;
//	mod->mins[2] = -maxheight/2;
//	mod->maxs[2] = maxheight/2;

//
// load the frames
//
	if (numframes < 1)
		Host_Error ("Mod_LoadHLSprite: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	in = (byte *)(pin + 1);
	i = in[0] + in[1] * 256;
	if (i != 256)
		Host_Error ("Mod_LoadHLSprite: unexpected number of palette colors %i (should be 256)", i);
	in += 2;
	switch(rendermode)
	{
	case SPRHL_NORMAL:
		for (i = 0;i < 256;i++)
		{
			palette[i][0] = *in++;
			palette[i][1] = *in++;
			palette[i][2] = *in++;
			palette[i][3] = 255;
		}
		palette[255][0] = palette[255][1] = palette[255][2] = palette[255][3] = 0;
		break;
	case SPRHL_ADDITIVE:
		for (i = 0;i < 256;i++)
		{
			palette[i][0] = *in++;
			palette[i][1] = *in++;
			palette[i][2] = *in++;
			palette[i][3] = 255;
		}
//		palette[255][0] = palette[255][1] = palette[255][2] = palette[255][3] = 0;
		mod->flags |= EF_ADDITIVE;
		break;
	case SPRHL_INDEXALPHA:
		for (i = 0;i < 256;i++)
		{
			palette[i][0] = 255;
			palette[i][1] = 255;
			palette[i][2] = 255;
			palette[i][3] = i;
			in += 3;
		}
		break;
	case SPRHL_ALPHATEST:
		for (i = 0;i < 256;i++)
		{
			palette[i][0] = *in++;
			palette[i][1] = *in++;
			palette[i][2] = *in++;
			palette[i][3] = 255;
		}
		palette[0][0] = palette[0][1] = palette[0][2] = palette[0][3] = 0;
		break;
	default:
		Host_Error("Mod_LoadHLSprite: unknown texFormat (%i, should be 0, 1, 2, or 3)\n", i);
		return;
	}

	pframetype = (dspriteframetype_t *)in;

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
			size = LittleLong(pframe->width) * LittleLong(pframe->height);
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
				size = LittleLong(pframe->width) * LittleLong(pframe->height);
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
			Mod_LoadSpriteFrame (framedata[realframes], frames + realframes, i, 1, &palette[0][0], &modelradius);
			realframes++;
		}
	}

	psprite->ofs_frames = (int) frames - (int) psprite;

	qfree(framedata);

	mod->type = mod_sprite;

	modelradius = sqrt(modelradius);
	for (i = 0;i < 3;i++)
	{
		mod->normalmins[i] = mod->yawmins[i] = mod->rotatedmins[i] = -modelradius;
		mod->normalmaxs[i] = mod->yawmaxs[i] = mod->rotatedmaxs[i] = modelradius;
	}

// move the complete, relocatable sprite model to the cache
	end = Hunk_LowMark ();
	mod->cachesize = total = end - start;

	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, psprite, total);

	Hunk_FreeToLowMark (start);
}

void Mod_Sprite_SERAddEntity(void)
{
	R_ClipSprite();
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int version;
	version = ((dsprite_t *)buffer)->version;
	switch(version)
	{
	case SPRITEHL_VERSION:
		Mod_LoadHLSprite (mod, buffer);
		break;
	case SPRITE_VERSION:
	case SPRITE32_VERSION:
		Mod_LoadQuakeSprite(mod, buffer);
		break;
	default:
		Host_Error ("Mod_LoadSpriteModel: %s has wrong version number (%i should be 1 (quake) or 32 (sprite32) or 2 (halflife)", mod->name, version);
		break;
	}
	mod->SERAddEntity = Mod_Sprite_SERAddEntity;
	mod->DrawEarly = R_DrawSpriteModel;
	mod->DrawLate = NULL;
	mod->DrawShadow = NULL;
}
