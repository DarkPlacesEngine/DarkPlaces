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
} mvertex_t;

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
} mplane_t;

typedef struct texture_s
{
	char				name[16];
	unsigned			width, height;
	rtexture_t			*texture;
	rtexture_t			*glowtexture;		// LordHavoc: fullbrights on walls
	int					anim_total;			// total frames in sequence (0 = not animated)
	struct texture_s	*anim_frames[10];	// LordHavoc: direct pointers to each of the frames in the sequence
	struct texture_s	*alternate_anims;	// bmodels in frame 1 use these
	int					transparent;		// LordHavoc: transparent texture support
} texture_t;


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
//#define SURF_UNDERWATER		0x80
// LordHavoc: added these for lava and teleport textures
#define SURF_DRAWNOALPHA	0x100
#define SURF_DRAWFULLBRIGHT	0x200
// LordHavoc: light both sides
#define SURF_LIGHTBOTHSIDES		0x400

typedef struct
{
	unsigned short	v[2];
} medge_t;

typedef struct
{
	float		vecs[2][4];
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

// LordHavoc: was 7, I added one more for raw lightmap position
#define	VERTEXSIZE	8

typedef struct glpoly_s
{
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;
	int		numverts;
	int		flags;			// for SURF_UNDERWATER
	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	mplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	short		texturemins[2];
	short		extents[2];

	short		light_s, light_t;	// gl lightmap coordinates

	glpoly_t	*polys;				// multiple if warped

	mtexinfo_t	*texinfo;
	
// lighting info
	int			dlightframe;
	int			dlightbits[8];

	int			lightframe; // avoid redundent addition of dlights
	int			worldnodeframe; // only render each surface once

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	unsigned short	cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	short		cached_dlight;				// LordHavoc: if lightmap was lit by dynamic lights, update on frame after end of effect to erase it
	short		cached_lighthalf;			// LordHavoc: to cause lightmap to be rerendered when lighthalf changes
	float		cached_ambient;				// LordHavoc: rerender lightmaps when r_ambient changes
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

// warning: if this is changed, references must be updated in cpu_* assembly files
typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs

	struct mnode_s	*parent;
	struct mportal_s *portals;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int			contents;		// will be a negative contents number

	struct mnode_s	*parent;
	struct mportal_s *portals;

// leaf specific
	int			visframe;		// visible if current (r_framecount)
	int			worldnodeframe; // used by certain worldnode variants to avoid processing the same leaf twice in a frame

	// for bounding box culling
	vec3_t		mins;
	vec3_t		maxs;

	// LordHavoc: leaf based dynamic lighting
	int			dlightbits[8];
	int			dlightframe;

	byte		*compressed_vis;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct
{
	dclipnode_t	*clipnodes;
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

typedef struct mportal_s
{
	struct mportal_s *next; // the next portal on this leaf
	mleaf_t *here; // the leaf this portal is on
	mleaf_t *past; // the leaf through this portal (infront)
	mvertex_t *points;
	int numpoints;
	mplane_t plane;
}
mportal_t;

extern rtexture_t *r_notexture;
extern texture_t r_notexture_mip;
