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

cvar_t r_mipsprites = {CVAR_SAVE, "r_mipsprites", "1", "mipmaps sprites so they render faster in the distance and do not display noise artifacts"};
cvar_t r_picmipsprites = {CVAR_SAVE, "r_picmipsprites", "1", "make gl_picmip affect sprites too (saves some graphics memory in sprite heavy games)"};
cvar_t r_labelsprites_scale = {CVAR_SAVE, "r_labelsprites_scale", "1", "global scale to apply to label sprites before conversion to HUD coordinates"};
cvar_t r_labelsprites_roundtopixels = {CVAR_SAVE, "r_labelsprites_roundtopixels", "1", "try to make label sprites sharper by rounding their size to 0.5x or 1x and by rounding their position to whole pixels if possible"};

/*
===============
Mod_SpriteInit
===============
*/
void Mod_SpriteInit (void)
{
	Cvar_RegisterVariable(&r_mipsprites);
	Cvar_RegisterVariable(&r_picmipsprites);
	Cvar_RegisterVariable(&r_labelsprites_scale);
	Cvar_RegisterVariable(&r_labelsprites_roundtopixels);
}

static void Mod_SpriteSetupTexture(texture_t *texture, skinframe_t *skinframe, qboolean fullbright, qboolean additive)
{
	if (!skinframe)
		skinframe = R_SkinFrame_LoadMissing();
	texture->offsetmapping = OFFSETMAPPING_OFF;
	texture->offsetscale = 1;
	texture->specularscalemod = 1;
	texture->specularpowermod = 1;
	texture->basematerialflags = MATERIALFLAG_WALL;
	if (fullbright)
		texture->basematerialflags |= MATERIALFLAG_FULLBRIGHT;
	if (additive)
		texture->basematerialflags |= MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	else if (skinframe->hasalpha)
		texture->basematerialflags |= MATERIALFLAG_ALPHA | MATERIALFLAG_BLENDED | MATERIALFLAG_NOSHADOW;
	texture->currentmaterialflags = texture->basematerialflags;
	texture->numskinframes = 1;
	texture->currentskinframe = texture->skinframes[0] = skinframe;
	texture->surfaceflags = 0;
	texture->supercontents = SUPERCONTENTS_SOLID;
	if (!(texture->basematerialflags & MATERIALFLAG_BLENDED))
		texture->supercontents |= SUPERCONTENTS_OPAQUE;
}

static void Mod_Sprite_SharedSetup(const unsigned char *datapointer, int version, const unsigned int *palette, qboolean additive)
{
	int					i, j, groupframes, realframes, x, y, origin[2], width, height;
	qboolean			fullbright;
	dspriteframetype_t	*pinframetype;
	dspriteframe_t		*pinframe;
	dspritegroup_t		*pingroup;
	dspriteinterval_t	*pinintervals;
	skinframe_t			*skinframe;
	float				modelradius, interval;
	char				name[MAX_QPATH], fogname[MAX_QPATH];
	const void			*startframes;
	int                 texflags = (r_mipsprites.integer ? TEXF_MIPMAP : 0) | (r_picmipsprites.integer ? TEXF_PICMIP : 0) | TEXF_ALPHA | TEXF_CLAMP;
	modelradius = 0;

	if (loadmodel->numframes < 1)
		Host_Error ("Mod_Sprite_SharedSetup: Invalid # of frames: %d", loadmodel->numframes);

	// LordHavoc: hack to allow sprites to be non-fullbright
	fullbright = true;
	for (i = 0;i < MAX_QPATH && loadmodel->name[i];i++)
		if (loadmodel->name[i] == '!')
			fullbright = false;

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

	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->sprite.sprdata_frames = (mspriteframe_t *)Mem_Alloc(loadmodel->mempool, sizeof(mspriteframe_t) * realframes);
	loadmodel->num_textures = realframes;
	loadmodel->num_texturesperskin = 1;
	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, sizeof(texture_t) * loadmodel->num_textures);

	datapointer = (unsigned char *)startframes;
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

		dpsnprintf(loadmodel->animscenes[i].name, sizeof(loadmodel->animscenes[i].name), "frame %i", i);
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

			x = (int)max(loadmodel->sprite.sprdata_frames[realframes].left * loadmodel->sprite.sprdata_frames[realframes].left, loadmodel->sprite.sprdata_frames[realframes].right * loadmodel->sprite.sprdata_frames[realframes].right);
			y = (int)max(loadmodel->sprite.sprdata_frames[realframes].up * loadmodel->sprite.sprdata_frames[realframes].up, loadmodel->sprite.sprdata_frames[realframes].down * loadmodel->sprite.sprdata_frames[realframes].down);
			if (modelradius < x + y)
				modelradius = x + y;

			if (cls.state != ca_dedicated)
			{
				skinframe = NULL;
				// note: Nehahra's null.spr has width == 0 and height == 0
				if (width > 0 && height > 0)
				{
					if (groupframes > 1)
					{
						dpsnprintf (name, sizeof(name), "%s_%i_%i", loadmodel->name, i, j);
						dpsnprintf (fogname, sizeof(fogname), "%s_%i_%ifog", loadmodel->name, i, j);
					}
					else
					{
						dpsnprintf (name, sizeof(name), "%s_%i", loadmodel->name, i);
						dpsnprintf (fogname, sizeof(fogname), "%s_%ifog", loadmodel->name, i);
					}
					if (!(skinframe = R_SkinFrame_LoadExternal(name, texflags | TEXF_COMPRESS, false)))
					{
						unsigned char *pixels = (unsigned char *) Mem_Alloc(loadmodel->mempool, width*height*4);
						if (version == SPRITE32_VERSION)
						{
							for (x = 0;x < width*height;x++)
							{
								pixels[x*4+2] = datapointer[x*4+0];
								pixels[x*4+1] = datapointer[x*4+1];
								pixels[x*4+0] = datapointer[x*4+2];
								pixels[x*4+3] = datapointer[x*4+3];
							}
						}
						else //if (version == SPRITEHL_VERSION || version == SPRITE_VERSION)
							Image_Copy8bitBGRA(datapointer, pixels, width*height, palette ? palette : palette_bgra_transparent);
						skinframe = R_SkinFrame_LoadInternalBGRA(name, texflags, pixels, width, height);
						Mem_Free(pixels);
					}
				}
				if (skinframe == NULL)
					skinframe = R_SkinFrame_LoadMissing();
				Mod_SpriteSetupTexture(&loadmodel->data_textures[realframes], skinframe, fullbright, additive);
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
void Mod_IDSP_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int version;
	const unsigned char *datapointer;

	datapointer = (unsigned char *)buffer;

	loadmodel->modeldatatypestring = "SPR1";

	loadmodel->type = mod_sprite;

	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Sprite_Draw;
	loadmodel->DrawDepth = NULL;
	loadmodel->CompileShadowVolume = NULL;
	loadmodel->DrawShadowVolume = NULL;
	loadmodel->DrawLight = NULL;
	loadmodel->DrawAddWaterPlanes = NULL;

	version = LittleLong(((dsprite_t *)buffer)->version);
	if (version == SPRITE_VERSION || version == SPRITE32_VERSION)
	{
		dsprite_t *pinqsprite;

		pinqsprite = (dsprite_t *)datapointer;
		datapointer += sizeof(dsprite_t);

		loadmodel->numframes = LittleLong (pinqsprite->numframes);
		loadmodel->sprite.sprnum_type = LittleLong (pinqsprite->type);
		loadmodel->synctype = (synctype_t)LittleLong (pinqsprite->synctype);

		Mod_Sprite_SharedSetup(datapointer, LittleLong (pinqsprite->version), NULL, false);
	}
	else if (version == SPRITEHL_VERSION)
	{
		int i, rendermode;
		unsigned char palette[256][4];
		const unsigned char *in;
		dspritehl_t *pinhlsprite;

		pinhlsprite = (dspritehl_t *)datapointer;
		datapointer += sizeof(dspritehl_t);

		loadmodel->numframes = LittleLong (pinhlsprite->numframes);
		loadmodel->sprite.sprnum_type = LittleLong (pinhlsprite->type);
		loadmodel->synctype = (synctype_t)LittleLong (pinhlsprite->synctype);
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
		case SPRHL_OPAQUE:
			for (i = 0;i < 256;i++)
			{
				palette[i][2] = in[i*3+0];
				palette[i][1] = in[i*3+1];
				palette[i][0] = in[i*3+2];
				palette[i][3] = 255;
			}
			break;
		case SPRHL_ADDITIVE:
			for (i = 0;i < 256;i++)
			{
				palette[i][2] = in[i*3+0];
				palette[i][1] = in[i*3+1];
				palette[i][0] = in[i*3+2];
				palette[i][3] = 255;
			}
			// also passes additive == true to Mod_Sprite_SharedSetup
			break;
		case SPRHL_INDEXALPHA:
			for (i = 0;i < 256;i++)
			{
				palette[i][2] = in[765];
				palette[i][1] = in[766];
				palette[i][0] = in[767];
				palette[i][3] = i;
				in += 3;
			}
			break;
		case SPRHL_ALPHATEST:
			for (i = 0;i < 256;i++)
			{
				palette[i][2] = in[i*3+0];
				palette[i][1] = in[i*3+1];
				palette[i][0] = in[i*3+2];
				palette[i][3] = 255;
			}
			palette[255][0] = palette[255][1] = palette[255][2] = palette[255][3] = 0;
			// should this use alpha test or alpha blend?  (currently blend)
			break;
		default:
			Host_Error("Mod_IDSP_Load: unknown texFormat (%i, should be 0, 1, 2, or 3)", i);
			return;
		}

		Mod_Sprite_SharedSetup(datapointer, LittleLong (pinhlsprite->version), (unsigned int *)(&palette[0][0]), rendermode == SPRHL_ADDITIVE);
	}
	else
		Host_Error("Mod_IDSP_Load: %s has wrong version number (%i). Only %i (quake), %i (HalfLife), and %i (sprite32) supported",
					loadmodel->name, version, SPRITE_VERSION, SPRITEHL_VERSION, SPRITE32_VERSION);

	loadmodel->surfmesh.isanimated = loadmodel->numframes > 1 || loadmodel->animscenes[0].framecount > 1;
}


void Mod_IDS2_Load(dp_model_t *mod, void *buffer, void *bufferend)
{
	int i, version;
	qboolean fullbright;
	const dsprite2_t *pinqsprite;
	skinframe_t *skinframe;
	float modelradius;
	int texflags = (r_mipsprites.integer ? TEXF_MIPMAP : 0) | (r_picmipsprites.integer ? TEXF_PICMIP : 0) | TEXF_COMPRESS | TEXF_ALPHA | TEXF_CLAMP;

	loadmodel->modeldatatypestring = "SPR2";

	loadmodel->type = mod_sprite;

	loadmodel->DrawSky = NULL;
	loadmodel->Draw = R_Model_Sprite_Draw;
	loadmodel->DrawDepth = NULL;
	loadmodel->CompileShadowVolume = NULL;
	loadmodel->DrawShadowVolume = NULL;
	loadmodel->DrawLight = NULL;
	loadmodel->DrawAddWaterPlanes = NULL;

	pinqsprite = (dsprite2_t *)buffer;

	version = LittleLong(pinqsprite->version);
	if (version != SPRITE2_VERSION)
		Host_Error("Mod_IDS2_Load: %s has wrong version number (%i should be 2 (quake 2)", loadmodel->name, version);

	loadmodel->numframes = LittleLong (pinqsprite->numframes);
	if (loadmodel->numframes < 1)
		Host_Error ("Mod_IDS2_Load: Invalid # of frames: %d", loadmodel->numframes);
	loadmodel->sprite.sprnum_type = SPR_VP_PARALLEL;
	loadmodel->synctype = ST_SYNC;

	// LordHavoc: hack to allow sprites to be non-fullbright
	fullbright = true;
	for (i = 0;i < MAX_QPATH && loadmodel->name[i];i++)
		if (loadmodel->name[i] == '!')
			fullbright = false;

	loadmodel->animscenes = (animscene_t *)Mem_Alloc(loadmodel->mempool, sizeof(animscene_t) * loadmodel->numframes);
	loadmodel->sprite.sprdata_frames = (mspriteframe_t *)Mem_Alloc(loadmodel->mempool, sizeof(mspriteframe_t) * loadmodel->numframes);
	loadmodel->num_textures = loadmodel->numframes;
	loadmodel->num_texturesperskin = 1;
	loadmodel->data_textures = (texture_t *)Mem_Alloc(loadmodel->mempool, sizeof(texture_t) * loadmodel->num_textures);

	modelradius = 0;
	for (i = 0;i < loadmodel->numframes;i++)
	{
		int origin[2], x, y, width, height;
		const dsprite2frame_t *pinframe;
		mspriteframe_t *sprframe;

		dpsnprintf(loadmodel->animscenes[i].name, sizeof(loadmodel->animscenes[i].name), "frame %i", i);
		loadmodel->animscenes[i].firstframe = i;
		loadmodel->animscenes[i].framecount = 1;
		loadmodel->animscenes[i].framerate = 10;
		loadmodel->animscenes[i].loop = true;

		pinframe = &pinqsprite->frames[i];

		origin[0] = LittleLong (pinframe->origin_x);
		origin[1] = LittleLong (pinframe->origin_y);
		width = LittleLong (pinframe->width);
		height = LittleLong (pinframe->height);

		sprframe = &loadmodel->sprite.sprdata_frames[i];

		// note that sp2 origin[0] is positive, where as it is negative in
		// spr/spr32/hlspr
		sprframe->left = -origin[0];
		sprframe->right = -origin[0] + width;
		sprframe->up = origin[1];
		sprframe->down = origin[1] - height;

		x = (int)max(sprframe->left * sprframe->left, sprframe->right * sprframe->right);
		y = (int)max(sprframe->up * sprframe->up, sprframe->down * sprframe->down);
		if (modelradius < x + y)
			modelradius = x + y;
	}

	if (cls.state != ca_dedicated)
	{
		for (i = 0;i < loadmodel->numframes;i++)
		{
			const dsprite2frame_t *pinframe;
			pinframe = &pinqsprite->frames[i];
			if (!(skinframe = R_SkinFrame_LoadExternal(pinframe->name, texflags, false)))
			{
				Con_Printf("Mod_IDS2_Load: failed to load %s", pinframe->name);
				skinframe = R_SkinFrame_LoadMissing();
			}
			Mod_SpriteSetupTexture(&loadmodel->data_textures[i], skinframe, fullbright, false);
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

	loadmodel->surfmesh.isanimated = loadmodel->numframes > 1 || loadmodel->animscenes[0].framecount > 1;
}
