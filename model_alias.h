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

#ifndef MODEL_ALIAS_H
#define MODEL_ALIAS_H

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#include "modelgen.h"

typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} daliashdr_t;

#define	MAXALIASVERTS	4096
#define	MAXALIASFRAMES	1024
#define	MAXALIASTRIS	4096

/*
========================================================================

.MD2 triangle model file format

========================================================================
*/

// LordHavoc: grabbed this from the Q2 utility source,
// renamed a things to avoid conflicts

#define MD2ALIAS_VERSION	8

#define	MD2MAX_TRIANGLES	4096
#define MD2MAX_VERTS		4096
#define MD2MAX_FRAMES		1024
#define	MD2MAX_SKINNAME	64
// sanity checking size
#define MD2MAX_SIZE	(16777216)

typedef struct
{
	short	s;
	short	t;
} md2stvert_t;

typedef struct 
{
	short	index_xyz[3];
	short	index_st[3];
} md2triangle_t;

typedef struct
{
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
} md2frame_t;

// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.


typedef struct
{
	int			ident;
	int			version;

	int			skinwidth;
	int			skinheight;
	int			framesize;		// byte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_skins;		// each skin is a MAX_SKINNAME string
	int			ofs_st;			// byte offset from start for stverts
	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;
	int			ofs_end;		// end of file
} md2_t;

// LordHavoc: mdl, md2 and md3 models are converted to the same internal format
#define ALIASTYPE_ALIAS 1
#define ALIASTYPE_ZYM 2

extern void Mod_LoadQ1AliasModel (struct model_s *mod, void *buffer);
extern void Mod_LoadQ2AliasModel (struct model_s *mod, void *buffer);
extern void Mod_LoadQ3AliasModel (struct model_s *mod, void *buffer);

extern void Mod_AliasInit(void);

#include "model_zymotic.h"

// all md3 ints, floats, and shorts, are little endian, and thus need to be
// passed through LittleLong/LittleFloat/LittleShort to avoid breaking on
// bigendian machines (Macs for example)
#define MD3VERSION 15
#define MD3NAME 64
#define MD3FRAMENAME 16

// the origin is at 1/64th scale
// the pitch and yaw are encoded as 8 bits each
typedef struct md3vertex_s
{
	short origin[3], normalpitchyaw;
}
md3vertex_t;

// one per frame
typedef struct md3frameinfo_s
{
	float mins[3];
	float maxs[3];
	float origin[3];
	float radius;
	char name[MD3FRAMENAME];
}
md3frameinfo_t;

// one per tag per frame
typedef struct md3tag_s
{
	char name[MD3NAME];
	float origin[3];
	float rotationmatrix[9];
}
md3tag_t;

// one per shader per mesh
typedef struct md3shader_s
{
	char name[MD3NAME];
	// engine field (yes this empty int does exist in the file)
	int shadernum;
}
md3shader_t;

// one per mesh per model
//
// note that the lump_ offsets in this struct are relative to the beginning
// of the mesh struct
//
// to find the next mesh in the file, you must go to lump_end, which puts you
// at the beginning of the next mesh
typedef struct md3mesh_s
{
	char identifier[4]; // "IDP3"
	char name[MD3NAME];
	int flags;
	int num_frames;
	int num_shaders;
	int num_vertices;
	int num_triangles;
	int lump_elements;
	int lump_shaders;
	int lump_texcoords;
	int lump_framevertices;
	int lump_end;
}
md3mesh_t;

// this struct is at the beginning of the md3 file
//
// note that the lump_ offsets in this struct are relative to the beginning
// of the header struct (which is the beginning of the file)
typedef struct md3modelheader_s
{
	char identifier[4]; // "IDP3"
	int version; // 15
	char name[MD3NAME];
	int flags;
	int num_frames;
	int num_tags;
	int num_meshes;
	int num_skins;
	int lump_frameinfo;
	int lump_tags;
	int lump_meshes;
	int lump_end;
}
md3modelheader_t;

// LordHavoc: all quake series 'alias' models (mdl, md2, md3) are converted to this vertex format
typedef struct aliasvertex_s
{
	// location
	float origin[3];
	// surface normal
	float normal[3];
	// S texture vector
	float svector[3];
}
aliasvertex_t;

// this layer is fog (completely specialized behavior)
#define ALIASLAYER_FOG 1
// alpha blending
#define ALIASLAYER_ALPHA 2
// additive blending
#define ALIASLAYER_ADD 4
// apply diffuse lighting
#define ALIASLAYER_DIFFUSE 8
// apply specular lighting
#define ALIASLAYER_SPECULAR 16
// tint with pants color
#define ALIASLAYER_COLORMAP_PANTS 32
// tint with shirt color
#define ALIASLAYER_COLORMAP_SHIRT 64
// don't draw this layer if colormap is not used
#define ALIASLAYER_NODRAW_IF_NOTCOLORMAPPED 128
// don't draw this layer if colormap is used
#define ALIASLAYER_NODRAW_IF_COLORMAPPED 256
// draw this layer for realtime lighting passes, otherwise don't
#define ALIASLAYER_DRAW_PER_LIGHT 512

typedef struct aliaslayer_s
{
	int flags;
	rtexture_t *texture;
	rtexture_t *nmap;
}
aliaslayer_t;

// indicates this skin is transparent
#define ALIASSKIN_TRANSPARENT 1

typedef struct aliasskin_s
{
	int flags;
	int num_layers;
	aliaslayer_t *data_layers;
}
aliasskin_t;

typedef struct aliasmesh_s
{
	int num_skins;
	int num_triangles;
	int num_frames;
	int num_vertices;
	aliasskin_t *data_skins;
	int *data_elements;
	int *data_neighbors;
	float *data_texcoords;
	aliasvertex_t *data_vertices;
}
aliasmesh_t;

#endif

