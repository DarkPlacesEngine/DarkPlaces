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

/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#include "modelgen.h"

/*
typedef struct
{
	int					firstpose;
	int					numposes;
	float				interval;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;
*/

// LordHavoc: new vertex format
typedef struct {
	byte v[3]; // location
	signed char n[3]; // surface normal for lighting *127.0
} trivert2;

#define	MAX_SKINS	32
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

typedef struct
{
	char name[16]; // LordHavoc: only kept this for reasons of viewthing support
	unsigned short start;
	unsigned short length;
	float rate; // in poses per second
} maliasframe_t;

typedef struct
{
	vec3_t		scale;
	vec3_t		scale_origin;
	int			numverts;
	int			numtris;
	int			numframes;
	int			numposes;
	int			framedata; // LordHavoc: unsigned short start
	int			texdata; // LordHavoc: texture coordinate array
	int			posedata; // LordHavoc: vertex data for all the poses
	int			tridata; // LordHavoc: vertex indices for the triangles
} maliashdr_t;

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

#define MD2IDALIASHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD2ALIAS_VERSION	8

#define	MD2MAX_TRIANGLES	4096
#define MD2MAX_VERTS		4096
#define MD2MAX_FRAMES		1024
#define MD2MAX_SKINS	32
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
	trivertx_t	verts[1];	// variable sized
} md2frame_t;

// LordHavoc: memory representation is different than disk
typedef struct
{
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
	trivert2	verts[1];	// variable sized
} md2memframe_t;

// must match md2memframe_t, this is just used for sizeof()
typedef struct
{
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
} md2memframesize_t;


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

typedef struct
{
	int			framesize;		// byte size of each frame

	int			num_skins;
	int			num_xyz;
	int			num_st;			// greater than num_xyz for seams
	int			num_tris;
	int			num_glcmds;		// dwords in strip/fan command list
	int			num_frames;

	int			ofs_tris;		// offset for dtriangles
	int			ofs_frames;		// offset for first frame
	int			ofs_glcmds;	
} md2mem_t;

#define ALIASTYPE_MDL 1
#define ALIASTYPE_MD2 2
