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

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct
{
	vec3_t		position;
}
mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	int		type;			// for texture axis selection and fast side tests
	// LordHavoc: faster than id's signbits system
	int (*BoxOnPlaneSideFunc) (vec3_t emins, vec3_t emaxs, struct mplane_s *p);
}
mplane_t;

typedef struct texture_s
{
	char				name[16];
	unsigned			width, height;
	int					flags;				// LordHavoc: SURF_ flags

	rtexture_t			*texture;
	rtexture_t			*glowtexture;
	rtexture_t			*fogtexture;		// alpha-only version of main texture

	int					anim_total;			// total frames in sequence (< 2 = not animated)
	struct texture_s	*anim_frames[10];	// LordHavoc: direct pointers to each of the frames in the sequence
	struct texture_s	*alternate_anims;	// bmodels in frame 1 use these
}
texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
//#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_LIGHTMAP		0x20
//#define SURF_DRAWBACKGROUND	0x40
//#define SURF_UNDERWATER		0x80
#define SURF_DRAWNOALPHA	0x100
#define SURF_DRAWFULLBRIGHT	0x200
#define SURF_LIGHTBOTHSIDES	0x400
#define SURF_CLIPSOLID		0x800 // this polygon can obscure other polygons

typedef struct
{
	unsigned short	v[2];
}
medge_t;

typedef struct
{
	float		vecs[2][4];
	texture_t	*texture;
	int			flags;
}
mtexinfo_t;

typedef struct surfvertex_s
{
	// position
	float v[3];
	// offset into lightmap (used by vertex lighting)
	int lightmapoffset;
	// texture coordinates
	float st[2];
	// lightmap coordinates
	float uv[2];
}
surfvertex_t;

// LordHavoc: replaces glpoly, triangle mesh
typedef struct surfmesh_s
{
	int numverts;
	int numtriangles;
	surfvertex_t *vertex;
	int *index;
}
surfmesh_t;

typedef struct msurface_s
{
	// should be drawn if visframe == r_framecount (set by WorldNode functions)
	int			visframe;

	// the node plane this is on, backwards if SURF_PLANEBACK flag set
	mplane_t	*plane;
	// SURF_ flags
	int			flags;
	struct Cshader_s	*shader;
	struct msurface_s	*chain; // shader rendering chain

	// look up in model->surfedges[], negative numbers are backwards edges
	int			firstedge;
	int			numedges;

	short		texturemins[2];
	short		extents[2];

	mtexinfo_t	*texinfo;
	texture_t	*currenttexture; // updated (animated) during early surface processing each frame

	// index into d_lightstylevalue array, 255 means not used (black)
	byte		styles[MAXLIGHTMAPS];
	// RGB lighting data [numstyles][height][width][3]
	byte		*samples;
	// stain to apply on lightmap (soot/dirt/blood/whatever)
	byte		*stainsamples;

	// these fields are generated during model loading
	// the lightmap texture fragment to use on the surface
	rtexture_t *lightmaptexture;
	// the stride when building lightmaps to comply with fragment update
	int			lightmaptexturestride;
	// mesh for rendering
	surfmesh_t	mesh;

	// these are just 3D points defining the outline of the polygon,
	// no texcoord info (that can be generated from these)
	int			poly_numverts;
	float		*poly_verts;

	// these are regenerated every frame
	// lighting info
	int			dlightframe;
	int			dlightbits[8];
	// avoid redundent addition of dlights
	int			lightframe;
	// only render each surface once
	int			worldnodeframe;
	// marked when surface is prepared for the frame
	int			insertframe;

	// these cause lightmap updates if regenerated
	// values currently used in lightmap
	unsigned short cached_light[MAXLIGHTMAPS];
	// if lightmap was lit by dynamic lights, force update on next frame
	short		cached_dlight;
	// to cause lightmap to be rerendered when lighthalf changes
	short		cached_lightscalebit;
	// rerender lightmaps when r_ambient changes
	float		cached_ambient;
}
msurface_t;

#define SHADERSTAGE_SKY 0
#define SHADERSTAGE_NORMAL 1
#define SHADERSTAGE_FOG 2
#define SHADERSTAGE_COUNT 3

// change this stuff when real shaders are added
typedef struct Cshader_s
{
	int (*shaderfunc[SHADERSTAGE_COUNT])(int stage, msurface_t *s);
	// list of surfaces using this shader (used during surface rendering)
	msurface_t *chain;
}
Cshader_t;

extern Cshader_t Cshader_wall_vertex;
extern Cshader_t Cshader_wall_lightmap;
extern Cshader_t Cshader_wall_fullbright;
extern Cshader_t Cshader_water;
extern Cshader_t Cshader_sky;

// warning: if this is changed, references must be updated in cpu_* assembly files
typedef struct mnode_s
{
// common with leaf
	int					contents;		// 0, to differentiate from leafs

	struct mnode_s		*parent;
	struct mportal_s	*portals;

	// for bounding box culling
	vec3_t				mins;
	vec3_t				maxs;

// node specific
	mplane_t			*plane;
	struct mnode_s		*children[2];

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
}
mnode_t;

typedef struct mleaf_s
{
// common with node
	int					contents;		// will be a negative contents number

	struct mnode_s		*parent;
	struct mportal_s	*portals;

	// for bounding box culling
	vec3_t				mins;
	vec3_t				maxs;

// leaf specific
	int					visframe;		// visible if current (r_framecount)
	int					worldnodeframe; // used by certain worldnode variants to avoid processing the same leaf twice in a frame
	int					portalmarkid;	// used by polygon-through-portals visibility checker

	// LordHavoc: leaf based dynamic lighting
	int					dlightbits[8];
	int					dlightframe;

	byte				*compressed_vis;

	msurface_t			**firstmarksurface;
	int					nummarksurfaces;
	byte				ambient_sound_level[NUM_AMBIENTS];
}
mleaf_t;

typedef struct
{
	dclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
}
hull_t;

typedef struct mportal_s
{
	struct mportal_s *next; // the next portal on this leaf
	mleaf_t *here; // the leaf this portal is on
	mleaf_t *past; // the leaf through this portal (infront)
	mvertex_t *points;
	int numpoints;
	mplane_t plane;
	int visframe; // is this portal visible this frame?
}
mportal_t;

extern rtexture_t *r_notexture;
extern texture_t r_notexture_mip;

struct model_s;
void Mod_LoadBrushModel (struct model_s *mod, void *buffer);
void Mod_BrushInit(void);
void Mod_FindNonSolidLocation(vec3_t pos, struct model_s *mod);
