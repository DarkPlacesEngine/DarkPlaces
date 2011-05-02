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
typedef struct mvertex_s
{
	vec3_t position;
}
mvertex_t;

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2


// plane_t structure
typedef struct mplane_s
{
	vec3_t normal;
	float dist;
	// for texture axis selection and fast side tests
	int type; // set by PlaneClassify()
	int signbits; // set by PlaneClassify()
}
mplane_t;

#define SHADERSTAGE_SKY 0
#define SHADERSTAGE_NORMAL 1
#define SHADERSTAGE_COUNT 2

//#define SURF_PLANEBACK 2

// indicates that all triangles of the surface should be added to the BIH collision system
#define MATERIALFLAG_MESHCOLLISIONS 1
// use alpha blend on this material
#define MATERIALFLAG_ALPHA 2
// use additive blend on this material
#define MATERIALFLAG_ADD 4
// turn off depth test on this material
#define MATERIALFLAG_NODEPTHTEST 8
// multiply alpha by r_wateralpha cvar
#define MATERIALFLAG_WATERALPHA 16
// draw with no lighting
#define MATERIALFLAG_FULLBRIGHT 32
// drawn as a normal surface (alternative to SKY)
#define MATERIALFLAG_WALL 64
// this surface shows the sky in its place, alternative to WALL
// skipped if transparent
#define MATERIALFLAG_SKY 128
// swirling water effect (used with MATERIALFLAG_WALL)
#define MATERIALFLAG_WATERSCROLL 256
// skips drawing the surface
#define MATERIALFLAG_NODRAW 512
// probably used only on q1bsp water
#define MATERIALFLAG_LIGHTBOTHSIDES 1024
// use alpha test on this material
#define MATERIALFLAG_ALPHATEST 2048
// treat this material as a blended transparency (as opposed to an alpha test
// transparency), this causes special fog behavior, and disables glDepthMask
#define MATERIALFLAG_BLENDED 4096
// render using a custom blendfunc
#define MATERIALFLAG_CUSTOMBLEND 8192
// do not cast shadows from this material
#define MATERIALFLAG_NOSHADOW 16384
// render using vertex alpha (q3bsp) as texture blend parameter between foreground (normal) skinframe and background skinframe
#define MATERIALFLAG_VERTEXTEXTUREBLEND 32768
// disables GL_CULL_FACE on this texture (making it double sided)
#define MATERIALFLAG_NOCULLFACE 65536
// render with a very short depth range (like 10% of normal), this causes entities to appear infront of most of the scene
#define MATERIALFLAG_SHORTDEPTHRANGE 131072
// render water, comprising refraction and reflection (note: this is always opaque, the shader does the alpha effect)
#define MATERIALFLAG_WATERSHADER 262144
// render refraction (note: this is just a way to distort the background, otherwise useless)
#define MATERIALFLAG_REFRACTION 524288
// render reflection
#define MATERIALFLAG_REFLECTION 1048576
// use model lighting on this material (q1bsp lightmap sampling or q3bsp lightgrid, implies FULLBRIGHT is false)
#define MATERIALFLAG_MODELLIGHT 4194304
// add directional model lighting to this material (q3bsp lightgrid only)
#define MATERIALFLAG_MODELLIGHT_DIRECTIONAL 8388608
// causes RSurf_GetCurrentTexture to leave alone certain fields
#define MATERIALFLAG_CUSTOMSURFACE 16777216
// causes MATERIALFLAG_BLENDED to render a depth pass before rendering, hiding backfaces and other hidden geometry
#define MATERIALFLAG_TRANSDEPTH 33554432
// like refraction, but doesn't distort etc.
#define MATERIALFLAG_CAMERA 67108864
// disable rtlight on surface, use R_LightPoint instead
#define MATERIALFLAG_NORTLIGHT 134217728
// combined mask of all attributes that require depth sorted rendering
#define MATERIALFLAGMASK_DEPTHSORTED (MATERIALFLAG_BLENDED | MATERIALFLAG_NODEPTHTEST)
// combined mask of all attributes that cause some sort of transparency
#define MATERIALFLAGMASK_TRANSLUCENT (MATERIALFLAG_WATERALPHA | MATERIALFLAG_SKY | MATERIALFLAG_NODRAW | MATERIALFLAG_ALPHATEST | MATERIALFLAG_BLENDED | MATERIALFLAG_WATERSHADER | MATERIALFLAG_REFRACTION)

typedef struct medge_s
{
	unsigned short v[2];
}
medge_t;

struct entity_render_s;
struct texture_s;
struct msurface_s;

typedef struct mnode_s
{
	//this part shared between node and leaf
	mplane_t *plane; // != NULL
	struct mnode_s *parent;
	struct mportal_s *portals;
	// for bounding box culling
	vec3_t mins;
	vec3_t maxs;
	// supercontents from all brushes inside this node or leaf
	int combinedsupercontents;

	// this part unique to node
	struct mnode_s *children[2];

	// q1bsp specific
	unsigned short firstsurface;
	unsigned short numsurfaces;
}
mnode_t;

typedef struct mleaf_s
{
	//this part shared between node and leaf
	mplane_t *plane; // == NULL
	struct mnode_s *parent;
	struct mportal_s *portals;
	// for bounding box culling
	vec3_t mins;
	vec3_t maxs;
	// supercontents from all brushes inside this node or leaf
	int combinedsupercontents;

	// this part unique to leaf
	// common
	int clusterindex; // -1 is not in pvs, >= 0 is pvs bit number
	int areaindex; // q3bsp
	int containscollisionsurfaces; // indicates whether the leafsurfaces contains q3 patches
	int numleafsurfaces;
	int *firstleafsurface;
	int numleafbrushes; // q3bsp
	int *firstleafbrush; // q3bsp
	unsigned char ambient_sound_level[NUM_AMBIENTS]; // q1bsp
	int contents; // q1bsp: // TODO: remove (only used temporarily during loading when making collision hull 0)
	int portalmarkid; // q1bsp // used by see-polygon-through-portals visibility checker
}
mleaf_t;

typedef struct mclipnode_s
{
	int			planenum;
	int			children[2];	// negative numbers are contents
} mclipnode_t;

typedef struct hull_s
{
	mclipnode_t *clipnodes;
	mplane_t *planes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
	vec3_t clip_size;
}
hull_t;

typedef struct mportal_s
{
	struct mportal_s *next; // the next portal on this leaf
	mleaf_t *here; // the leaf this portal is on
	mleaf_t *past; // the leaf through this portal (infront)
	int numpoints;
	mvertex_t *points;
	vec3_t mins, maxs; // culling
	mplane_t plane;
}
mportal_t;

typedef struct svbspmesh_s
{
	struct svbspmesh_s *next;
	int numverts, maxverts;
	int numtriangles, maxtriangles;
	float *verts;
	int *elements;
}
svbspmesh_t;

// Q2 bsp stuff

#define Q2BSPVERSION	38

// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits

//=============================================================================

#define	Q2LUMP_ENTITIES		0
#define	Q2LUMP_PLANES			1
#define	Q2LUMP_VERTEXES		2
#define	Q2LUMP_VISIBILITY		3
#define	Q2LUMP_NODES			4
#define	Q2LUMP_TEXINFO		5
#define	Q2LUMP_FACES			6
#define	Q2LUMP_LIGHTING		7
#define	Q2LUMP_LEAFS			8
#define	Q2LUMP_LEAFFACES		9
#define	Q2LUMP_LEAFBRUSHES	10
#define	Q2LUMP_EDGES			11
#define	Q2LUMP_SURFEDGES		12
#define	Q2LUMP_MODELS			13
#define	Q2LUMP_BRUSHES		14
#define	Q2LUMP_BRUSHSIDES		15
#define	Q2LUMP_POP			16
#define	Q2LUMP_AREAS			17
#define	Q2LUMP_AREAPORTALS	18
#define	Q2HEADER_LUMPS		19

typedef struct q2dheader_s
{
	int			ident;
	int			version;
	lump_t		lumps[Q2HEADER_LUMPS];
} q2dheader_t;

typedef struct q2dmodel_s
{
	float		mins[3], maxs[3];
	float		origin[3];		// for sounds or lights
	int			headnode;
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} q2dmodel_t;

// planes (x&~1) and (x&~1)+1 are always opposites

// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define	Q2CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	Q2CONTENTS_WINDOW			2		// translucent, but not watery
#define	Q2CONTENTS_AUX			4
#define	Q2CONTENTS_LAVA			8
#define	Q2CONTENTS_SLIME			16
#define	Q2CONTENTS_WATER			32
#define	Q2CONTENTS_MIST			64
#define	Q2LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes

#define	Q2CONTENTS_AREAPORTAL		0x8000

#define	Q2CONTENTS_PLAYERCLIP		0x10000
#define	Q2CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define	Q2CONTENTS_CURRENT_0		0x40000
#define	Q2CONTENTS_CURRENT_90		0x80000
#define	Q2CONTENTS_CURRENT_180	0x100000
#define	Q2CONTENTS_CURRENT_270	0x200000
#define	Q2CONTENTS_CURRENT_UP		0x400000
#define	Q2CONTENTS_CURRENT_DOWN	0x800000

#define	Q2CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	Q2CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	Q2CONTENTS_DEADMONSTER	0x4000000
#define	Q2CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	Q2CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	Q2CONTENTS_LADDER			0x20000000



#define	Q2SURF_LIGHT		0x1		// value will hold the light strength

#define	Q2SURF_SLICK		0x2		// effects game physics

#define	Q2SURF_SKY		0x4		// don't draw, but add to skybox
#define	Q2SURF_WARP		0x8		// turbulent water warp
#define	Q2SURF_TRANS33	0x10
#define	Q2SURF_TRANS66	0x20
#define	Q2SURF_FLOWING	0x40	// scroll towards angle
#define	Q2SURF_NODRAW		0x80	// don't bother referencing the texture




typedef struct q2dnode_s
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for frustom culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} q2dnode_t;


typedef struct q2texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/*.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} q2texinfo_t;

typedef struct q2dleaf_s
{
	int				contents;			// OR of all brushes (not needed?)

	short			cluster;
	short			area;

	short			mins[3];			// for frustum culling
	short			maxs[3];

	unsigned short	firstleafface;
	unsigned short	numleaffaces;

	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
} q2dleaf_t;

typedef struct q2dbrushside_s
{
	unsigned short	planenum;		// facing out of the leaf
	short	texinfo;
} q2dbrushside_t;

typedef struct q2dbrush_s
{
	int			firstside;
	int			numsides;
	int			contents;
} q2dbrush_t;


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	Q2DVIS_PVS	0
#define	Q2DVIS_PHS	1
typedef struct q2dvis_s
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} q2dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct q2dareaportal_s
{
	int		portalnum;
	int		otherarea;
} q2dareaportal_t;

typedef struct q2darea_s
{
	int		numareaportals;
	int		firstareaportal;
} q2darea_t;


//Q3 bsp stuff

#define Q3BSPVERSION	46
#define Q3BSPVERSION_LIVE 47
#define Q3BSPVERSION_IG	48

#define	Q3LUMP_ENTITIES		0 // entities to spawn (used by server and client)
#define	Q3LUMP_TEXTURES		1 // textures used (used by faces)
#define	Q3LUMP_PLANES		2 // planes used (used by bsp nodes)
#define	Q3LUMP_NODES		3 // bsp nodes (used by bsp nodes, bsp leafs, rendering, collisions)
#define	Q3LUMP_LEAFS		4 // bsp leafs (used by bsp nodes)
#define	Q3LUMP_LEAFFACES	5 // array of ints indexing faces (used by leafs)
#define	Q3LUMP_LEAFBRUSHES	6 // array of ints indexing brushes (used by leafs)
#define	Q3LUMP_MODELS		7 // models (used by rendering, collisions)
#define	Q3LUMP_BRUSHES		8 // brushes (used by effects, collisions)
#define	Q3LUMP_BRUSHSIDES	9 // brush faces (used by brushes)
#define	Q3LUMP_VERTICES		10 // mesh vertices (used by faces)
#define	Q3LUMP_TRIANGLES	11 // mesh triangles (used by faces)
#define	Q3LUMP_EFFECTS		12 // fog (used by faces)
#define	Q3LUMP_FACES		13 // surfaces (used by leafs)
#define	Q3LUMP_LIGHTMAPS	14 // lightmap textures (used by faces)
#define	Q3LUMP_LIGHTGRID	15 // lighting as a voxel grid (used by rendering)
#define	Q3LUMP_PVS			16 // potentially visible set; bit[clusters][clusters] (used by rendering)
#define	Q3HEADER_LUMPS		17
#define	Q3LUMP_ADVERTISEMENTS 17 // quake live stuff written by zeroradiant's q3map2 (ignored by DP)
#define	Q3HEADER_LUMPS_LIVE	18
#define	Q3HEADER_LUMPS_MAX	18

typedef struct q3dheader_s
{
	int			ident;
	int			version;
	lump_t		lumps[Q3HEADER_LUMPS_MAX];
} q3dheader_t;

typedef struct q3dtexture_s
{
	char name[Q3PATHLENGTH];
	int surfaceflags;
	int contents;
}
q3dtexture_t;

// note: planes are paired, the pair of planes with i and i ^ 1 are opposites.
typedef struct q3dplane_s
{
	float normal[3];
	float dist;
}
q3dplane_t;

typedef struct q3dnode_s
{
	int planeindex;
	int childrenindex[2];
	int mins[3];
	int maxs[3];
}
q3dnode_t;

typedef struct q3dleaf_s
{
	int clusterindex; // pvs index
	int areaindex; // area index
	int mins[3];
	int maxs[3];
	int firstleafface;
	int numleaffaces;
	int firstleafbrush;
	int numleafbrushes;
}
q3dleaf_t;

typedef struct q3dmodel_s
{
	float mins[3];
	float maxs[3];
	int firstface;
	int numfaces;
	int firstbrush;
	int numbrushes;
}
q3dmodel_t;

typedef struct q3dbrush_s
{
	int firstbrushside;
	int numbrushsides;
	int textureindex;
}
q3dbrush_t;

typedef struct q3dbrushside_s
{
	int planeindex;
	int textureindex;
}
q3dbrushside_t;

typedef struct q3dbrushside_ig_s
{
	int planeindex;
	int textureindex;
	int surfaceflags;
}
q3dbrushside_ig_t;

typedef struct q3dvertex_s
{
	float origin3f[3];
	float texcoord2f[2];
	float lightmap2f[2];
	float normal3f[3];
	unsigned char color4ub[4];
}
q3dvertex_t;

typedef struct q3dmeshvertex_s
{
	int offset; // first vertex index of mesh
}
q3dmeshvertex_t;

typedef struct q3deffect_s
{
	char shadername[Q3PATHLENGTH];
	int brushindex;
	int unknown; // I read this is always 5 except in q3dm8 which has one effect with -1
}
q3deffect_t;

#define Q3FACETYPE_FLAT 1 // common
#define Q3FACETYPE_PATCH 2 // common
#define Q3FACETYPE_MESH 3 // common
#define Q3FACETYPE_FLARE 4 // rare (is this ever used?)

typedef struct q3dface_s
{
	int textureindex;
	int effectindex; // -1 if none
	int type; // Q3FACETYPE
	int firstvertex;
	int numvertices;
	int firstelement;
	int numelements;
	int lightmapindex; // -1 if none
	int lightmap_base[2];
	int lightmap_size[2];
	union
	{
		struct
		{
			// corrupt or don't care
			int blah[14];
		}
		unknown;
		struct
		{
			// Q3FACETYPE_FLAT
			// mesh is a collection of triangles on a plane, renderable as a mesh (NOT a polygon)
			float lightmap_origin[3];
			float lightmap_vectors[2][3];
			float normal[3];
			int unused1[2];
		}
		flat;
		struct
		{
			// Q3FACETYPE_PATCH
			// patch renders as a bezier mesh, with adjustable tesselation
			// level (optionally based on LOD using the bbox and polygon
			// count to choose a tesselation level)
			// note: multiple patches may have the same bbox to cause them to
			// be LOD adjusted together as a group
			int unused1[3];
			float mins[3]; // LOD bbox
			float maxs[3]; // LOD bbox
			int unused2[3];
			int patchsize[2]; // dimensions of vertex grid
		}
		patch;
		struct
		{
			// Q3FACETYPE_MESH
			// mesh renders as simply a triangle mesh
			int unused1[3];
			float mins[3];
			float maxs[3];
			int unused2[5];
		}
		mesh;
		struct
		{
			// Q3FACETYPE_FLARE
			// flare renders as a simple sprite at origin, no geometry
			// exists, nor does it have a radius, a cvar controls the radius
			// and another cvar controls distance fade
			// (they were not used in Q3 I'm told)
			float origin[3];
			int unused1[11];
		}
		flare;
	}
	specific;
}
q3dface_t;

typedef struct q3dlightmap_s
{
	unsigned char rgb[128*128*3];
}
q3dlightmap_t;

typedef struct q3dlightgrid_s
{
	unsigned char ambientrgb[3];
	unsigned char diffusergb[3];
	unsigned char diffusepitch;
	unsigned char diffuseyaw;
}
q3dlightgrid_t;

typedef struct q3dpvs_s
{
	int numclusters;
	int chainlength;
	// unsigned char chains[];
	// containing bits in 0-7 order (not 7-0 order),
	// pvschains[mycluster * chainlength + (thatcluster >> 3)] & (1 << (thatcluster & 7))
}
q3dpvs_t;

// surfaceflags from bsp
#define Q3SURFACEFLAG_NODAMAGE 1
#define Q3SURFACEFLAG_SLICK 2
#define Q3SURFACEFLAG_SKY 4
#define Q3SURFACEFLAG_LADDER 8
#define Q3SURFACEFLAG_NOIMPACT 16
#define Q3SURFACEFLAG_NOMARKS 32
#define Q3SURFACEFLAG_FLESH 64
#define Q3SURFACEFLAG_NODRAW 128
#define Q3SURFACEFLAG_HINT 256
#define Q3SURFACEFLAG_SKIP 512
#define Q3SURFACEFLAG_NOLIGHTMAP 1024
#define Q3SURFACEFLAG_POINTLIGHT 2048
#define Q3SURFACEFLAG_METALSTEPS 4096
#define Q3SURFACEFLAG_NOSTEPS 8192
#define Q3SURFACEFLAG_NONSOLID 16384
#define Q3SURFACEFLAG_LIGHTFILTER 32768
#define Q3SURFACEFLAG_ALPHASHADOW 65536
#define Q3SURFACEFLAG_NODLIGHT 131072
#define Q3SURFACEFLAG_DUST 262144

// surfaceparms from shaders
#define Q3SURFACEPARM_ALPHASHADOW 1
#define Q3SURFACEPARM_AREAPORTAL 2
#define Q3SURFACEPARM_CLUSTERPORTAL 4
#define Q3SURFACEPARM_DETAIL 8
#define Q3SURFACEPARM_DONOTENTER 16
#define Q3SURFACEPARM_FOG 32
#define Q3SURFACEPARM_LAVA 64
#define Q3SURFACEPARM_LIGHTFILTER 128
#define Q3SURFACEPARM_METALSTEPS 256
#define Q3SURFACEPARM_NODAMAGE 512
#define Q3SURFACEPARM_NODLIGHT 1024
#define Q3SURFACEPARM_NODRAW 2048
#define Q3SURFACEPARM_NODROP 4096
#define Q3SURFACEPARM_NOIMPACT 8192
#define Q3SURFACEPARM_NOLIGHTMAP 16384
#define Q3SURFACEPARM_NOMARKS 32768
#define Q3SURFACEPARM_NOMIPMAPS 65536
#define Q3SURFACEPARM_NONSOLID 131072
#define Q3SURFACEPARM_ORIGIN 262144
#define Q3SURFACEPARM_PLAYERCLIP 524288
#define Q3SURFACEPARM_SKY 1048576
#define Q3SURFACEPARM_SLICK 2097152
#define Q3SURFACEPARM_SLIME 4194304
#define Q3SURFACEPARM_STRUCTURAL 8388608
#define Q3SURFACEPARM_TRANS 16777216
#define Q3SURFACEPARM_WATER 33554432
#define Q3SURFACEPARM_POINTLIGHT 67108864
#define Q3SURFACEPARM_HINT 134217728
#define Q3SURFACEPARM_DUST 268435456
#define Q3SURFACEPARM_BOTCLIP 536870912
#define Q3SURFACEPARM_LIGHTGRID 1073741824
#define Q3SURFACEPARM_ANTIPORTAL 2147483648u

typedef struct q3mbrush_s
{
	struct colbrushf_s *colbrushf;
	int numbrushsides;
	struct q3mbrushside_s *firstbrushside;
	struct texture_s *texture;
}
q3mbrush_t;

typedef struct q3mbrushside_s
{
	struct mplane_s *plane;
	struct texture_s *texture;
}
q3mbrushside_t;

// the first cast is to shut up a stupid warning by clang, the second cast is to make both sides have the same type
#define CHECKPVSBIT(pvs,b) ((b) >= 0 ? (unsigned char) ((pvs)[(b) >> 3] & (1 << ((b) & 7))) : (unsigned char) false)
#define SETPVSBIT(pvs,b) (void) ((b) >= 0 ? (unsigned char) ((pvs)[(b) >> 3] |= (1 << ((b) & 7))) : (unsigned char) false)
#define CLEARPVSBIT(pvs,b) (void) ((b) >= 0 ? (unsigned char) ((pvs)[(b) >> 3] &= ~(1 << ((b) & 7))) : (unsigned char) false)

#endif

