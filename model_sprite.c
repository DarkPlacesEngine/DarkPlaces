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
#include "image.h"

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

static int alphaonlytable[4] = {255 | 0x80000000, 255 | 0x80000000, 255 | 0x80000000, 3};
static void Mod_Sprite_SharedSetup(qbyte *datapointer, int version, int *palette, int *alphapalette)
{
	int					i, j, groupframes, realframes, x, y, origin[2], width, height;
	dspriteframetype_t	*pinframetype;
	dspriteframe_t		*pinframe;
	dspritegroup_t		*pingroup;
	dspriteinterval_t	*pinintervals;
	float				modelradius, interval;
	char				name[MAX_QPATH], fogname[MAX_QPATH];
	qbyte				*pixbuf;
	void				*startframes;
	modelradius = 0;

	if (loadmodel->numframes < 1)
		Host_Error ("Mod_Sprite_SharedSetup: Invalid # of frames: %d\n", loadmodel->numframes);

	loadmodel->type = mod_sprite;

	// LordHavoc: hack to allow sprites to be non-fullbright
	for (i = 0;i < MAX_QPATH && loadmodel->name[i];i++)
	{
		if (loadmodel->name[i] == '!')
		{
			loadmodel->flags2 &= ~EF_FULLBRIGHT;
			break;
		}
	}

	// LordHavoc: 32bit textures
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION && version != SPRITEHL_VERSION)
		Host_Error("Mod_Sprite_SharedSetup: unsupported version %i, only %i (quake), %i (HalfLife), and %i (sprite32) supported", version, SPRITE_VERSION, SPRITEHL_VERSION, SPRITE32_VERSION);

//
// load the frames
//
	startframes = datapointer;
	realframes = 0;
	for (i = 0;i < loadmodel->numframes;i++)
	{
		pinframetype = (dspriteframetype_t *)datapointer;
		datapointer += sizeof(dspriteframetype_t);

		if (LittleLong (pinframetype->type) == SPR_SINGLE)
			groupframes = 1;
		else
		{
			pingroup = (dspritegroup_t *)datapointer;
			datapointer += sizeof(dspritegroup_t);

			groupframes = LittleLong(pingroup->numframes);

			datapointer += sizeof(dspriteinterval_t) * groupframes;
		}

		for (j = 0;j < groupframes;j++)
		{
			pinframe = (dspriteframe_t *)datapointer;
			if (version == SPRITE32_VERSION)
				datapointer += sizeof(dspriteframe_t) + LittleLong(pinframe->width) * LittleLong(pinframe->height) * 4;
			else //if (version == SPRITE_VERSION || version == SPRITEHL_VERSION)
				datapointer += sizeof(dspriteframe_t) + LittleLong(pinframe->width) * LittleLong(pinframe->height);
		}
		realframes += groupframes;
	}

	loadmodel->animscenes = Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->sprite.sprdata_frames = Mem_Alloc(loadmodel->mempool, sizeof(mspriteframe_t) * realframes);

	datapointer = startframes;
	realframes = 0;
	for (i = 0;i < loadmodel->numframes;i++)
	{
		pinframetype = (dspriteframetype_t *)datapointer;
		datapointer += sizeof(dspriteframetype_t);

		if (LittleLong (pinframetype->type) == SPR_SINGLE)
		{
			groupframes = 1;
			interval = 0.1f;
		}
		else
		{
			pingroup = (dspritegroup_t *)datapointer;
			datapointer += sizeof(dspritegroup_t);

			groupframes = LittleLong(pingroup->numframes);

			pinintervals = (dspriteinterval_t *)datapointer;
			datapointer += sizeof(dspriteinterval_t) * groupframes;

			interval = LittleFloat(pinintervals[0].interval);
			if (interval < 0.01f)
				Host_Error("Mod_Sprite_SharedSetup: invalid interval");
		}

		sprintf(loadmodel->animscenes[i].name, "frame %i", i);
		loadmodel->animscenes[i].firstframe = realframes;
		loadmodel->animscenes[i].framecount = groupframes;
		loadmodel->animscenes[i].framerate = 1.0f / interval;
		loadmodel->animscenes[i].loop = true;

		for (j = 0;j < groupframes;j++)
		{
			pinframe = (dspriteframe_t *)datapointer;
			datapointer += sizeof(dspriteframe_t);

			origin[0] = LittleLong (pinframe->origin[0]);
			origin[1] = LittleLong (pinframe->origin[1]);
			width = LittleLong (pinframe->width);
			height = LittleLong (pinframe->height);

			loadmodel->sprite.sprdata_frames[realframes].left = origin[0];
			loadmodel->sprite.sprdata_frames[realframes].right = origin[0] + width;
			loadmodel->sprite.sprdata_frames[realframes].up = origin[1];
			loadmodel->sprite.sprdata_frames[realframes].down = origin[1] - height;

			x = max(loadmodel->sprite.sprdata_frames[realframes].left * loadmodel->sprite.sprdata_frames[realframes].left, loadmodel->sprite.sprdata_frames[realframes].right * loadmodel->sprite.sprdata_frames[realframes].right);
			y = max(loadmodel->sprite.sprdata_frames[realframes].up * loadmodel->sprite.sprdata_frames[realframes].up, loadmodel->sprite.sprdata_frames[realframes].down * loadmodel->sprite.sprdata_frames[realframes].down);
			if (modelradius < x + y)
				modelradius = x + y;

			if (width > 0 && height > 0)
			{
				if (groupframes > 1)
					sprintf (name, "%s_%i_%i", loadmodel->name, i, j);
				else
					sprintf (name, "%s_%i", loadmodel->name, i);
				loadmodel->sprite.sprdata_frames[realframes].texture = loadtextureimagewithmask(loadmodel->texturepool, name, 0, 0, false, (r_mipsprites.integer ? TEXF_MIPMAP : 0) | TEXF_ALPHA | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP);
				loadmodel->sprite.sprdata_frames[realframes].fogtexture = image_masktex;

				if (!loadmodel->sprite.sprdata_frames[realframes].texture)
				{
					if (groupframes > 1)
						sprintf (fogname, "%s_%i_%ifog", loadmodel->name, i, j);
					else
						sprintf (fogname, "%s_%ifog", loadmodel->name, i);
					if (version == SPRITE32_VERSION)
					{
						loadmodel->sprite.sprdata_frames[realframes].texture = R_LoadTexture2D(loadmodel->texturepool, name, width, height, datapointer, TEXTYPE_RGBA, TEXF_ALPHA | (r_mipsprites.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, NULL);
						// make fog version (just alpha)
						pixbuf = Mem_Alloc(tempmempool, width*height*4);
						Image_CopyMux(pixbuf, datapointer, width, height, false, false, false, 4, 4, alphaonlytable);
						loadmodel->sprite.sprdata_frames[realframes].fogtexture = R_LoadTexture2D(loadmodel->texturepool, fogname, width, height, pixbuf, TEXTYPE_RGBA, TEXF_ALPHA | (r_mipsprites.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, NULL);
						Mem_Free(pixbuf);
					}
					else //if (version == SPRITE_VERSION || version == SPRITEHL_VERSION)
					{
						loadmodel->sprite.sprdata_frames[realframes].texture = R_LoadTexture2D(loadmodel->texturepool, name, width, height, datapointer, TEXTYPE_PALETTE, TEXF_ALPHA | (r_mipsprites.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, palette);
						loadmodel->sprite.sprdata_frames[realframes].fogtexture = R_LoadTexture2D(loadmodel->texturepool, fogname, width, height, datapointer, TEXTYPE_PALETTE, TEXF_ALPHA | (r_mipsprites.integer ? TEXF_MIPMAP : 0) | TEXF_CLAMP | TEXF_PRECACHE | TEXF_PICMIP, alphapalette);
					}
				}
			}

			if (version == SPRITE32_VERSION)
				datapointer += width * height * 4;
			else //if (version == SPRITE_VERSION || version == SPRITEHL_VERSION)
				datapointer += width * height;
			realframes++;
		}
	}

	modelradius = sqrt(modelradius);
	for (i = 0;i < 3;i++)
	{
		loadmodel->normalmins[i] = loadmodel->yawmins[i] = loadmodel->rotatedmins[i] = -modelradius;
		loadmodel->normalmaxs[i] = loadmodel->yawmaxs[i] = loadmodel->rotatedmaxs[i] = modelradius;
	}
	loadmodel->radius = modelradius;
	loadmodel->radius2 = modelradius * modelradius;
}

extern void R_Model_Sprite_Draw(entity_render_t *ent);
void Mod_IDSP_Load(model_t *mod, void *buffer)
{
	int version, i, rendermode;
	qbyte palette[256][4], alphapalette[256][4], *in;
	dsprite_t *pinqsprite;
	dspritehl_t *pinhlsprite;
	qbyte *datapointer;

	datapointer = buffer;

	loadmodel->flags2 = EF_FULLBRIGHT;

	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Sprite_Draw;
	loadmodel->DrawShadowVolume = NULL;
	loadmodel->DrawLight = NULL;

	version = LittleLong(((dsprite_t *)buffer)->version);
	if (version == SPRITE_VERSION || version == SPRITE32_VERSION)
	{
		pinqsprite = (dsprite_t *)datapointer;
		datapointer += sizeof(dsprite_t);

		loadmodel->numframes = LittleLong (pinqsprite->numframes);
		loadmodel->sprite.sprnum_type = LittleLong (pinqsprite->type);
		loadmodel->synctype = LittleLong (pinqsprite->synctype);

		Mod_Sprite_SharedSetup(datapointer, LittleLong (pinqsprite->version), palette_complete, palette_alpha);
	}
	else if (version == SPRITEHL_VERSION)
	{
		pinhlsprite = (dspritehl_t *)datapointer;
		datapointer += sizeof(dspritehl_t);

		loadmodel->numframes = LittleLong (pinhlsprite->numframes);
		loadmodel->sprite.sprnum_type = LittleLong (pinhlsprite->type);
		loadmodel->synctype = LittleLong (pinhlsprite->synctype);
		rendermode = pinhlsprite->rendermode;

		in = datapointer;
		datapointer += 2;
		i = in[0] + in[1] * 256;
		if (i != 256)
			Host_Error ("Mod_IDSP_Load: unexpected number of palette colors %i (should be 256)", i);
		in = datapointer;
		datapointer += 768;
		switch(rendermode)
		{
		case SPRHL_NORMAL:
			for (i = 0;i < 255;i++)
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
			loadmodel->flags2 |= EF_ADDITIVE;
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
			for (i = 0;i < 255;i++)
			{
				palette[i][0] = *in++;
				palette[i][1] = *in++;
				palette[i][2] = *in++;
				palette[i][3] = 255;
			}
			palette[0][0] = palette[0][1] = palette[0][2] = palette[0][3] = 0;
			break;
		default:
			Host_Error("Mod_IDSP_Load: unknown texFormat (%i, should be 0, 1, 2, or 3)\n", i);
			return;
		}

		for (i = 0;i < 256;i++)
		{
			alphapalette[i][0] = 255;
			alphapalette[i][1] = 255;
			alphapalette[i][2] = 255;
			alphapalette[i][3] = palette[i][3];
		}

		Mod_Sprite_SharedSetup(datapointer, LittleLong (pinhlsprite->version), (int *)(&palette[0][0]), (int *)(&alphapalette[0][0]));
	}
	else
		Host_Error("Mod_IDSP_Load: %s has wrong version number (%i should be 1 (quake) or 32 (sprite32) or 2 (halflife)", loadmodel->name, version);
}

