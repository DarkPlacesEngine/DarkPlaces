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

#ifndef __MODEL__
#define __MODEL__

#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum {ST_SYNC=0, ST_RAND } synctype_t;
#endif

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

typedef enum {mod_invalid, mod_brush, mod_sprite, mod_alias, mod_brushq2, mod_brushq3} modtype_t;

typedef struct animscene_s
{
	char name[32]; // for viewthing support
	int firstframe;
	int framecount;
	int loop; // true or false
	float framerate;
}
animscene_t;

typedef struct skinframe_s
{
	rtexture_t *base; // original texture without pants/shirt/glow
	rtexture_t *pants; // pants only (in greyscale)
	rtexture_t *shirt; // shirt only (in greyscale)
	rtexture_t *glow; // glow only (fullbrights)
	rtexture_t *merged; // original texture without glow
	rtexture_t *fog; // alpha of the base texture (if not opaque)
	rtexture_t *nmap; // normalmap (bumpmap for dot3)
	rtexture_t *gloss; // glossmap (for dot3)
	rtexture_t *detail; // detail texture (silly bumps for non-dot3)
}
skinframe_t;

#define MAX_SKINS 256

#define SHADOWMESHVERTEXHASH 1024
typedef struct shadowmeshvertexhash_s
{
	struct shadowmeshvertexhash_s *next;
}
shadowmeshvertexhash_t;

typedef struct shadowmesh_s
{
	struct shadowmesh_s *next;
	int numverts, maxverts;
	int numtriangles, maxtriangles;
	float *vertex3f;
	int *element3i;
	int *neighbor3i;
	// these are NULL after Mod_ShadowMesh_Finish is performed, only used
	// while building meshes
	shadowmeshvertexhash_t **vertexhashtable, *vertexhashentries;
}
shadowmesh_t;


#include "model_brush.h"
#include "model_sprite.h"
#include "model_alias.h"

#include "matrixlib.h"

typedef struct model_alias_s
{
	// LordHavoc: Q2/ZYM model support
	int				aliastype;

	// mdl/md2/md3 models are the same after loading
	int				aliasnum_meshes;
	aliasmesh_t		*aliasdata_meshes;

	// for Zymotic models
	int				zymnum_verts;
	int				zymnum_tris;
	int				zymnum_shaders;
	int				zymnum_bones;
	int				zymnum_scenes;
	float			*zymdata_texcoords;
	rtexture_t		**zymdata_textures;
	qbyte			*zymdata_trizone;
	zymbone_t		*zymdata_bones;
	unsigned int	*zymdata_vertbonecounts;
	zymvertex_t		*zymdata_verts;
	unsigned int	*zymdata_renderlist;
	float			*zymdata_poses;
}
model_alias_t;

typedef struct model_sprite_s
{
	int				sprnum_type;
	mspriteframe_t	*sprdata_frames;
}
model_sprite_t;

typedef struct model_brush_s
{
	char *entities;
	void (*FindNonSolidLocation)(struct model_s *model, const vec3_t in, vec3_t out, vec_t radius);
	int (*PointContents)(struct model_s *model, const float *p);
	void (*TraceBox)(struct model_s *model, const vec3_t corigin, const vec3_t cangles, void *trace, const void *cent, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end);
}
model_brush_t;

typedef struct model_brushq1_s
{
	// true if this model is a HalfLife .bsp file
	qboolean		ishlbsp;

	int				firstmodelsurface, nummodelsurfaces;

	// lightmap format, set to r_lightmaprgba when model is loaded
	int				lightmaprgba;

	int				numsubmodels;
	dmodel_t		*submodels;

	int				numplanes;
	mplane_t		*planes;

	// number of visible leafs, not counting 0 (solid)
	int				numleafs;
	mleaf_t			*leafs;

	int				numvertexes;
	mvertex_t		*vertexes;

	int				numedges;
	medge_t			*edges;

	int				numnodes;
	mnode_t			*nodes;

	int				numtexinfo;
	mtexinfo_t		*texinfo;

	int				numsurfaces;
	msurface_t		*surfaces;
	int				*surfacevisframes;
	int				*surfacepvsframes;
	msurface_t		*surfacepvsnext;
	surfmesh_t		*entiremesh;
	surfmesh_t		*surfmeshes;

	int				numsurfedges;
	int				*surfedges;

	int				numclipnodes;
	dclipnode_t		*clipnodes;

	int				nummarksurfaces;
	int				*marksurfaces;

	hull_t			hulls[MAX_MAP_HULLS];

	int				numtextures;
	texture_t		*textures;

	qbyte			*visdata;
	qbyte			*lightdata;

	int				numportals;
	mportal_t		*portals;

	int				numportalpoints;
	mvertex_t		*portalpoints;

	int				numlights;
	mlight_t		*lights;

	// pvs visibility marking
	mleaf_t			*pvsviewleaf;
	int				pvsviewleafnovis;
	int				pvsframecount;
	mleaf_t			*pvsleafchain;
	int				*pvssurflist;
	int				pvssurflistlength;
	// these get rebuilt as the player moves around if this is the world,
	// otherwise they are left alone (no pvs for bmodels)
	msurface_t		***pvstexturechains;
	msurface_t		**pvstexturechainsbuffer;
	int				*pvstexturechainslength;

	// lightmap update chains for light styles
	int				light_styles;
	qbyte			*light_style;
	int				*light_stylevalue;
	msurface_t		***light_styleupdatechains;
	msurface_t		**light_styleupdatechainsbuffer;
	int				light_scalebit;
	float			light_ambient;

	mleaf_t *(*PointInLeaf)(struct model_s *model, const float *p);
	qbyte *(*LeafPVS)(struct model_s *model, mleaf_t *leaf);
	void (*BuildPVSTextureChains)(struct model_s *model);
}
model_brushq1_t;

/* MSVC can't compile empty structs, so this is commented out for now
typedef struct model_brushq2_s
{
}
model_brushq2_t;
*/

typedef struct q3mtexture_s
{
	char name[Q3PATHLENGTH];
	int surfaceflags;
	int contents;

	int number;
	skinframe_t skin;
}
q3mtexture_t;

typedef struct q3mnode_s
{
	int isnode; // true
	struct q3mnode_s *parent;
	struct mplane_s *plane;
	struct q3mnode_s *children[2];
}
q3mnode_t;

typedef struct q3mleaf_s
{
	int isnode; // false
	struct q3mnode_s *parent;
	int clusterindex;
	int areaindex;
	int numleaffaces;
	struct q3mface_s **firstleafface;
	int numleafbrushes;
	struct q3mbrush_s **firstleafbrush;
	vec3_t mins;
	vec3_t maxs;
}
q3mleaf_t;

typedef struct q3mmodel_s
{
	vec3_t mins;
	vec3_t maxs;
	int numfaces;
	struct q3mface_s *firstface;
	int numbrushes;
	struct q3mbrush_s *firstbrush;
}
q3mmodel_t;

typedef struct q3mbrush_s
{
	int numbrushsides;
	struct q3mbrushside_s *firstbrushside;
	struct q3mtexture_s *texture;
}
q3mbrush_t;

typedef struct q3mbrushside_s
{
	struct mplane_s *plane;
	struct q3mtexture_s *texture;
}
q3mbrushside_t;

typedef struct q3meffect_s
{
	char shadername[Q3PATHLENGTH];
	struct q3mbrush_s *brush;
	int unknown; // 5 or -1
}
q3meffect_t;

typedef struct q3mface_s
{
	struct q3mtexture_s *texture;
	struct q3meffect_s *effect;
	rtexture_t *lightmaptexture;
	int type;
	int firstvertex;
	int numvertices;
	int firstelement;
	int numelements;
	int patchsize[2];

	float *data_vertex3f;
	float *data_texturetexcoord2f;
	float *data_lightmaptexcoord2f;
	float *data_svector3f;
	float *data_tvector3f;
	float *data_normal3f;
	float *data_color4f;
	int numtriangles;
	int *data_element3i;
	int *data_neighbor3i;
}
q3mface_t;

typedef struct model_brushq3_s
{
	int num_textures;
	q3mtexture_t *data_textures;

	int num_planes;
	mplane_t *data_planes;

	int num_nodes;
	q3mnode_t *data_nodes;

	int num_leafs;
	q3mleaf_t *data_leafs;

	int num_leafbrushes;
	q3mbrush_t **data_leafbrushes;

	int num_leaffaces;
	q3mface_t **data_leaffaces;

	int num_models;
	q3mmodel_t *data_models;
	// each submodel gets its own model struct so this is different for each.
	q3mmodel_t data_thismodel;

	int num_brushes;
	q3mbrush_t *data_brushes;

	int num_brushsides;
	q3mbrushside_t *data_brushsides;

	int num_vertices;
	float *data_vertex3f;
	float *data_texturetexcoord2f;
	float *data_lightmaptexcoord2f;
	float *data_svector3f;
	float *data_tvector3f;
	float *data_normal3f;
	float *data_color4f;

	int num_triangles;
	int *data_element3i;
	int *data_neighbor3i;

	int num_effects;
	q3meffect_t *data_effects;

	int num_faces;
	q3mface_t *data_faces;

	// lightmap textures
	int num_lightmaps;
	rtexture_t **data_lightmaps;

	// voxel light data with directional shading
	int num_lightgrid;
	q3dlightgrid_t *data_lightgrid;
	// size of each cell (may vary by map, typically 64 64 128)
	float num_lightgrid_cellsize[3];
	// 1.0 / num_lightgrid_cellsize
	float num_lightgrid_scale[3];
	// dimensions of the world model in lightgrid cells
	int num_lightgrid_imins[3];
	int num_lightgrid_imaxs[3];
	int num_lightgrid_isize[3];
	// indexing/clamping
	int num_lightgrid_dimensions[3];
	// transform modelspace coordinates to lightgrid index
	matrix4x4_t num_lightgrid_indexfromworld;

	// pvs
	int num_pvsclusters;
	int num_pvschainlength;
	unsigned char *data_pvschains;
	// example
	//pvschain = model->brushq3.data_pvschains + mycluster * model->brushq3.num_pvschainlength;
	//if (pvschain[thatcluster >> 3] & (1 << (thatcluster & 7)))
}
model_brushq3_t;

typedef struct model_s
{
	// name and path of model, for example "progs/player.mdl"
	char			name[MAX_QPATH];
	// model needs to be loaded if this is true
	qboolean		needload;
	// set if the model is used in current map, models which are not, are purged
	qboolean		used;
	// true if this is the world model (I.E. defines what sky to use, and may contain submodels)
	qboolean		isworldmodel;
	// CRC of the file this model was loaded from, to reload if changed
	unsigned int	crc;
	// mod_brush, mod_alias, mod_sprite
	modtype_t		type;
	// memory pool for allocations
	mempool_t		*mempool;
	// all models use textures...
	rtexturepool_t	*texturepool;
	// flags from the model file
	int				flags;
	// engine calculated flags, ones that can not be set in the file
	int				flags2;
	// LordHavoc: if true (normally only for sprites) the model/sprite/bmodel is always rendered fullbright
	int				fullbright;
	// number of QC accessible frame(group)s in the model
	int				numframes;
	// number of QC accessible skin(group)s in the model
	int				numskins;
	// whether to randomize animated framegroups
	synctype_t		synctype;
	// bounding box at angles '0 0 0'
	vec3_t			normalmins, normalmaxs;
	// bounding box if yaw angle is not 0, but pitch and roll are
	vec3_t			yawmins, yawmaxs;
	// bounding box if pitch or roll are used
	vec3_t			rotatedmins, rotatedmaxs;
	// sphere radius, usable at any angles
	float			radius;
	// squared sphere radius for easier comparisons
	float			radius2;
	// skin animation info
	animscene_t		*skinscenes; // [numskins]
	// skin frame info
	skinframe_t		*skinframes;
	// skin animation info
	animscene_t		*animscenes; // [numframes]
	// draw the model's sky polygons (only used by brush models)
	void(*DrawSky)(struct entity_render_s *ent);
	// draw the model using lightmap/dlight shading
	void(*Draw)(struct entity_render_s *ent);
	// draw a fake shadow for the model
	void(*DrawFakeShadow)(struct entity_render_s *ent);
	// draw a shadow volume for the model based on light source
	void(*DrawShadowVolume)(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius);
	// draw the lighting on a model (through stencil)
	void(*DrawLight)(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltofilter, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);
	// fields belonging to each type of model
	model_alias_t	alias;
	model_sprite_t	sprite;
	model_brush_t	brush;
	model_brushq1_t	brushq1;
	/* MSVC can't handle an empty struct, so this is commented out for now
	model_brushq2_t	brushq2;
	*/
	model_brushq3_t	brushq3;
}
model_t;

//============================================================================

// this can be used for anything without a valid texture
extern rtexture_t *r_notexture;
#define NUM_DETAILTEXTURES 1
extern rtexture_t *mod_shared_detailtextures[NUM_DETAILTEXTURES];
// every texture must be in a pool...
extern rtexturepool_t *mod_shared_texturepool;

// model loading
extern model_t *loadmodel;
extern qbyte *mod_base;
// sky/water subdivision
//extern cvar_t gl_subdivide_size;
// texture fullbrights
extern cvar_t r_fullbrights;

void Mod_Init (void);
void Mod_CheckLoaded (model_t *mod);
void Mod_ClearAll (void);
model_t *Mod_FindName (const char *name);
model_t *Mod_ForName (const char *name, qboolean crash, qboolean checkdisk, qboolean isworldmodel);
void Mod_TouchModel (const char *name);
void Mod_UnloadModel (model_t *mod);

void Mod_ClearUsed(void);
void Mod_PurgeUnused(void);
void Mod_LoadModels(void);

extern model_t *loadmodel;
extern char loadname[32];	// for hunk tags

int Mod_FindTriangleWithEdge(const int *elements, int numtriangles, int start, int end, int ignore);
void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles);
void Mod_ValidateElements(const int *elements, int numtriangles, int numverts, const char *filename, int fileline);
void Mod_BuildTextureVectorsAndNormals(int numverts, int numtriangles, const float *vertex, const float *texcoord, const int *elements, float *svectors, float *tvectors, float *normals);

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts);
shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh);
int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *v);
void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, float *vert0, float *vert1, float *vert2);
void Mod_ShadowMesh_AddPolygon(mempool_t *mempool, shadowmesh_t *mesh, int numverts, float *verts);
void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, float *verts, int numtris, int *elements);
shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int initialnumtriangles);
shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh);
void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius);
void Mod_ShadowMesh_Free(shadowmesh_t *mesh);

int Mod_LoadSkinFrame(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture);
int Mod_LoadSkinFrame_Internal(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture, qbyte *skindata, int width, int height);

#endif	// __MODEL__

