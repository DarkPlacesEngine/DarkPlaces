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

#ifndef MODEL_BRUSH_H
#define MODEL_BRUSH_H

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
	// name
	char name[16];
	// size
	unsigned int width, height;
	// SURF_ flags
	unsigned int flags;

	// base texture without fullbrights, never NULL
	rtexture_t *texture;
	// fullbrights texture, NULL if no fullbrights used
	rtexture_t *glowtexture;
	// alpha texture (used for fogging), NULL if opaque
	rtexture_t *fogtexture;
	// detail texture (usually not used if transparent)
	rtexture_t *detailtexture;

	// total frames in sequence and alternate sequence
	int anim_total[2];
	// direct pointers to each of the frames in the sequences
	// (indexed as [alternate][frame])
	struct texture_s *anim_frames[2][10];
	// set if animated or there is an alternate frame set
	// (this is an optimization in the renderer)
	int animated;
}
texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x10
#define SURF_LIGHTMAP		0x20
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
	// detail texture coordinates
	float ab[2];
}
surfvertex_t;

// LordHavoc: replaces glpoly, triangle mesh
typedef struct surfmesh_s
{
	// can be multiple meshs per surface
	struct surfmesh_s *chain;
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
	qbyte		styles[MAXLIGHTMAPS];
	// RGB lighting data [numstyles][height][width][3]
	qbyte		*samples;
	// stain to apply on lightmap (soot/dirt/blood/whatever)
	qbyte		*stainsamples;

	// these fields are generated during model loading
	// the lightmap texture fragment to use on the surface
	rtexture_t *lightmaptexture;
	// the stride when building lightmaps to comply with fragment update
	int			lightmaptexturestride;
	// mesh for rendering
	surfmesh_t	*mesh;

	// these are just 3D points defining the outline of the polygon,
	// no texcoord info (that can be generated from these)
	int			poly_numverts;
	float		*poly_verts;
	// the center is useful for sorting
	float		poly_center[3];

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
	// to cause lightmap to be rerendered when v_overbrightbits changes
	short		cached_lightscalebit;
	// rerender lightmaps when r_ambient changes
	float		cached_ambient;
}
msurface_t;

#define SHADERSTAGE_SKY 0
#define SHADERSTAGE_NORMAL 1
#define SHADERSTAGE_COUNT 2

struct entity_render_s;
// change this stuff when real shaders are added
typedef struct Cshader_s
{
	void (*shaderfunc[SHADERSTAGE_COUNT])(const struct entity_render_s *ent, const msurface_t *firstsurf);
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

	qbyte				*compressed_vis;

	msurface_t			**firstmarksurface;
	int					nummarksurfaces;
	qbyte				ambient_sound_level[NUM_AMBIENTS];
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
	vec3_t		clip_size;
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

typedef struct mlight_s
{
	vec3_t origin;
	float falloff;
	vec3_t light;
	float subtract;
	vec3_t spotdir;
	float spotcone; // cosine of spotlight cone angle (or 0 if not a spotlight)
	float distbias;
	int style;
	int numleafs; // used only for loading calculations, number of leafs this shines on
}
mlight_t;

extern rtexture_t *r_notexture;
extern texture_t r_notexture_mip;

struct model_s;
void Mod_LoadBrushModel (struct model_s *mod, void *buffer);
void Mod_BrushInit(void);
void Mod_FindNonSolidLocation(vec3_t pos, struct model_s *mod);

#endif

