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

#ifndef MODEL_SHARED_H
#define MODEL_SHARED_H

typedef enum {ST_SYNC=0, ST_RAND } synctype_t;

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

typedef enum {mod_invalid, mod_brushq1, mod_sprite, mod_alias, mod_brushq2, mod_brushq3} modtype_t;

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
	rtexture_t *stain; // inverse modulate with background (used for decals and such)
	rtexture_t *merged; // original texture without glow
	rtexture_t *base; // original texture without pants/shirt/glow
	rtexture_t *pants; // pants only (in greyscale)
	rtexture_t *shirt; // shirt only (in greyscale)
	rtexture_t *nmap; // normalmap (bumpmap for dot3)
	rtexture_t *gloss; // glossmap (for dot3)
	rtexture_t *detail; // detail texture (silly bumps for non-dot3)
	rtexture_t *glow; // glow only (fullbrights)
	rtexture_t *fog; // alpha of the base texture (if not opaque)
}
skinframe_t;

#define MAX_SKINS 256

typedef struct overridetagname_s
{
	char name[MAX_QPATH];
}
overridetagname_t;

// a replacement set of tag names, per skin
typedef struct overridetagnameset_s
{
	int num_overridetagnames;
	overridetagname_t *data_overridetagnames;
}
overridetagnameset_t;

#define SHADOWMESHVERTEXHASH 1024
typedef struct shadowmeshvertexhash_s
{
	struct shadowmeshvertexhash_s *next;
}
shadowmeshvertexhash_t;

typedef struct shadowmesh_s
{
	// next mesh in chain
	struct shadowmesh_s *next;
	// used for light mesh (NULL on shadow mesh)
	rtexture_t *map_diffuse;
	rtexture_t *map_specular;
	rtexture_t *map_normal;
	// buffer sizes
	int numverts, maxverts;
	int numtriangles, maxtriangles;
	// used always
	float *vertex3f;
	// used for light mesh (NULL on shadow mesh)
	float *svector3f;
	float *tvector3f;
	float *normal3f;
	float *texcoord2f;
	// used always
	int *element3i;
	// used for shadow mesh (NULL on light mesh)
	int *neighbor3i;
	// these are NULL after Mod_ShadowMesh_Finish is performed, only used
	// while building meshes
	shadowmeshvertexhash_t **vertexhashtable, *vertexhashentries;
}
shadowmesh_t;


#include "matrixlib.h"

#include "model_brush.h"
#include "model_sprite.h"
#include "model_alias.h"

typedef struct model_alias_s
{
	// mdl/md2/md3/zym model formats are treated the same after loading

	// the shader meshes comprising this model
	int				aliasnum_meshes;
	aliasmesh_t		*aliasdata_meshes;

	// for md3 models
	int				aliasnum_tags;
	int				aliasnum_tagframes;
	aliastag_t		*aliasdata_tags;

	// for skeletal models
	int				aliasnum_bones;
	aliasbone_t		*aliasdata_bones;
	int				aliasnum_poses;
	float			*aliasdata_poses;
}
model_alias_t;

typedef struct model_sprite_s
{
	int				sprnum_type;
	mspriteframe_t	*sprdata_frames;
}
model_sprite_t;

struct trace_s;

typedef struct model_brush_s
{
	// true if this model is a HalfLife .bsp file
	qboolean ishlbsp;
	// string of entity definitions (.map format)
	char *entities;
	// number of submodels in this map (just used by server to know how many
	// submodels to load)
	int numsubmodels;

	// pvs
	int num_pvsclusters;
	int num_pvsclusterbytes;
	unsigned char *data_pvsclusters;
	// example
	//pvschain = model->brush.data_pvsclusters + mycluster * model->brush.num_pvsclusterbytes;
	//if (pvschain[thatcluster >> 3] & (1 << (thatcluster & 7)))

	// a mesh containing all shadow casting geometry for the whole model (including submodels), portions of this are referenced by each surface's num_firstshadowmeshtriangle 
	shadowmesh_t *shadowmesh;

	// common functions
	int (*SuperContentsFromNativeContents)(struct model_s *model, int nativecontents);
	int (*NativeContentsFromSuperContents)(struct model_s *model, int supercontents);
	qbyte *(*GetPVS)(struct model_s *model, const vec3_t p);
	int (*FatPVS)(struct model_s *model, const vec3_t org, vec_t radius, qbyte *pvsbuffer, int pvsbufferlength);
	int (*BoxTouchingPVS)(struct model_s *model, const qbyte *pvs, const vec3_t mins, const vec3_t maxs);
	void (*LightPoint)(struct model_s *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal);
	void (*FindNonSolidLocation)(struct model_s *model, const vec3_t in, vec3_t out, vec_t radius);
	// these are actually only found on brushq1, but NULL is handled gracefully
	void (*AmbientSoundLevelsForPoint)(struct model_s *model, const vec3_t p, qbyte *out, int outsize);
	void (*RoundUpToHullSize)(struct model_s *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs);

	char skybox[64];

	rtexture_t *solidskytexture;
	rtexture_t *alphaskytexture;
}
model_brush_t;

typedef struct model_brushq1_s
{
	// lightmap format, set to r_lightmaprgba when model is loaded
	int				lightmaprgba;

	dmodel_t		*submodels;

	int				numplanes;
	mplane_t		*planes;

	// number of actual leafs (including 0 which is solid)
	int				num_leafs;
	// visible leafs, not counting 0 (solid)
	int				num_visleafs;
	mleaf_t			*data_leafs;

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

	int				numsurfedges;
	int				*surfedges;

	int				numclipnodes;
	dclipnode_t		*clipnodes;

	int				nummarksurfaces;
	int				*marksurfaces;

	hull_t			hulls[MAX_MAP_HULLS];

	int				numtextures;
	texture_t		*textures;

	int				num_compressedpvs;
	qbyte			*data_compressedpvs;

	int				num_lightdata;
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

	mleaf_t *(*PointInLeaf)(struct model_s *model, const float *p);
	void (*BuildPVSTextureChains)(struct model_s *model);
}
model_brushq1_t;

/* MSVC can't compile empty structs, so this is commented out for now
typedef struct model_brushq2_s
{
}
model_brushq2_t;
*/

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
#define Q3SURFACEPARM_SLICK 2197152
#define Q3SURFACEPARM_SLIME 4194304
#define Q3SURFACEPARM_STRUCTURAL 8388608
#define Q3SURFACEPARM_TRANS 16777216
#define Q3SURFACEPARM_WATER 33554432

// various flags from shaders
#define Q3TEXTUREFLAG_TWOSIDED 1
#define Q3TEXTUREFLAG_ADDITIVE 2
#define Q3TEXTUREFLAG_NOMIPMAPS 4
#define Q3TEXTUREFLAG_NOPICMIP 8
#define Q3TEXTUREFLAG_AUTOSPRITE 16
#define Q3TEXTUREFLAG_AUTOSPRITE2 32
#define Q3TEXTUREFLAG_ALPHATEST 64

struct q3msurface_s;
typedef struct q3mtexture_s
{
	char name[Q3PATHLENGTH];
	char firstpasstexturename[Q3PATHLENGTH];
	int surfaceflags;
	int nativecontents;
	int supercontents;
	int surfaceparms;
	int textureflags;

	int number;
	skinframe_t skin;

	int numfaces;
	struct q3msurface_s **facelist;
	int *facenumlist;
}
q3mtexture_t;

typedef struct q3mnode_s
{
	//this part shared between node and leaf
	struct mplane_s *plane; // != NULL
	struct q3mnode_s *parent;
	vec3_t mins;
	vec3_t maxs;
	// this part unique to nodes
	struct q3mnode_s *children[2];
}
q3mnode_t;

typedef struct q3mleaf_s
{
	//this part shared between node and leaf
	struct mplane_s *plane; // == NULL
	struct q3mnode_s *parent;
	vec3_t mins;
	vec3_t maxs;
	// this part unique to leafs
	int clusterindex;
	int areaindex;
	int numleaffaces;
	struct q3msurface_s **firstleafface;
	int *firstleaffacenum;
	int numleafbrushes;
	struct q3mbrush_s **firstleafbrush;
}
q3mleaf_t;

typedef struct q3mmodel_s
{
	vec3_t mins;
	vec3_t maxs;
	int numfaces;
	struct q3msurface_s *firstface;
	int numbrushes;
	struct q3mbrush_s *firstbrush;
}
q3mmodel_t;

typedef struct q3mbrush_s
{
	struct colbrushf_s *colbrushf;
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

typedef struct q3msurface_s
{
	// FIXME: collisionmarkframe should be kept in a separate array
	// FIXME: visframe should be kept in a separate array
	// FIXME: shadowmark should be kept in a separate array

	struct q3mtexture_s *texture;
	struct q3meffect_s *effect;
	rtexture_t *lightmaptexture;
	int collisionmarkframe; // don't collide twice in one trace
	int visframe; // visframe == r_framecount means it is visible this frame
	// bounding box for culling
	float mins[3];
	float maxs[3];

	int num_vertices;
	int num_triangles;
	float *data_vertex3f;
	float *data_texcoordtexture2f;
	float *data_texcoordlightmap2f;
	float *data_svector3f;
	float *data_tvector3f;
	float *data_normal3f;
	float *data_color4f;
	int *data_element3i;
	int *data_neighbor3i;

	int num_collisionvertices;
	int num_collisiontriangles;
	float *data_collisionvertex3f;
	int *data_collisionelement3i;

	// index into model->brush.shadowmesh
	int num_firstshadowmeshtriangle;

	// used for shadow volume generation
	int shadowmark;
}
q3msurface_t;

typedef struct model_brushq3_s
{
	// if non-zero this is a submodel
	// (this is the number of the submodel, an index into data_models)
	int submodel;

	int num_textures;
	q3mtexture_t *data_textures;
	q3msurface_t **data_texturefaces;
	int *data_texturefacenums;

	int num_planes;
	mplane_t *data_planes;

	int num_nodes;
	q3mnode_t *data_nodes;

	int num_leafs;
	q3mleaf_t *data_leafs;

	int num_leafbrushes;
	q3mbrush_t **data_leafbrushes;

	int num_leaffaces;
	q3msurface_t **data_leaffaces;
	int *data_leaffacenums;

	int num_models;
	q3mmodel_t *data_models;
	// each submodel gets its own model struct so this is different for each.
	q3mmodel_t *data_thismodel;

	int num_brushes;
	q3mbrush_t *data_brushes;

	int num_brushsides;
	q3mbrushside_t *data_brushsides;

	int num_vertices;
	float *data_vertex3f;
	float *data_texcoordtexture2f;
	float *data_texcoordlightmap2f;
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
	q3msurface_t *data_faces;

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
}
model_brushq3_t;

typedef struct model_s
{
	// name and path of model, for example "progs/player.mdl"
	char			name[MAX_QPATH];
	// model needs to be loaded if this is false
	qboolean		loaded;
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
	// skin animation info
	animscene_t		*animscenes; // [numframes]
	// range of surface numbers in this (sub)model
	int				firstmodelsurface;
	int				nummodelsurfaces;
	// list of surface numbers in this (sub)model
	int				*surfacelist;
	// draw the model's sky polygons (only used by brush models)
	void(*DrawSky)(struct entity_render_s *ent);
	// draw the model using lightmap/dlight shading
	void(*Draw)(struct entity_render_s *ent);
	// gathers info on which clusters and surfaces are lit by light, as well as calculating a bounding box
	void(*GetLightInfo)(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outclusterlist, qbyte *outclusterpvs, int *outnumclusterspointer, int *outsurfacelist, qbyte *outsurfacepvs, int *outnumsurfacespointer);
	// draw a shadow volume for the model based on light source
	void(*DrawShadowVolume)(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius, int numsurfaces, const int *surfacelist);
	// draw the lighting on a model (through stencil)
	void(*DrawLight)(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int numsurfaces, const int *surfacelist);
	// trace a box against this model
	void (*TraceBox)(struct model_s *model, int frame, struct trace_s *trace, const vec3_t boxstartmins, const vec3_t boxstartmaxs, const vec3_t boxendmins, const vec3_t boxendmaxs, int hitsupercontentsmask);
	// fields belonging to each type of model
	model_alias_t	alias;
	model_sprite_t	sprite;
	model_brush_t	brush;
	model_brushq1_t	brushq1;
	/* MSVC can't handle an empty struct, so this is commented out for now
	model_brushq2_t	brushq2;
	*/
	model_brushq3_t	brushq3;
	// skin files can have different tags for each skin
	overridetagnameset_t	*data_overridetagnamesforskin;
	// flags this model for offseting sounds to the model center (used by brush models)
	int soundfromcenter;
}
model_t;

//============================================================================

// this can be used for anything without a valid texture
extern rtexture_t *r_notexture;
#define NUM_DETAILTEXTURES 1
extern rtexture_t *mod_shared_detailtextures[NUM_DETAILTEXTURES];
// every texture must be in a pool...
extern rtexturepool_t *mod_shared_texturepool;

extern rtexture_t *mod_shared_distorttexture[64];

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
void Mod_UnloadModel (model_t *mod);

void Mod_ClearUsed(void);
void Mod_PurgeUnused(void);
void Mod_LoadModels(void);

extern model_t *loadmodel;
extern char loadname[32];	// for hunk tags

int Mod_BuildVertexRemapTableFromElements(int numelements, const int *elements, int numvertices, int *remapvertices);
void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles);
void Mod_ValidateElements(const int *elements, int numtriangles, int numverts, const char *filename, int fileline);
void Mod_BuildNormals(int numverts, int numtriangles, const float *vertex3f, const int *elements, float *normal3f);
void Mod_BuildTextureVectorsAndNormals(int numverts, int numtriangles, const float *vertex, const float *texcoord, const int *elements, float *svectors, float *tvectors, float *normals);

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable);
shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh, int light, int neighbors);
int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *vertex14f);
void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, float *vertex14f);
void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, int numtris, const int *element3i);
shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable);
shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh, int light, int neighbors);
void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius);
void Mod_ShadowMesh_Free(shadowmesh_t *mesh);

int Mod_LoadSkinFrame(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture);
int Mod_LoadSkinFrame_Internal(skinframe_t *skinframe, char *basename, int textureflags, int loadpantsandshirt, int usedetailtexture, int loadglowtexture, qbyte *skindata, int width, int height);

// used for talking to the QuakeC mainly
int Mod_Q1BSP_NativeContentsFromSuperContents(struct model_s *model, int supercontents);
int Mod_Q1BSP_SuperContentsFromNativeContents(struct model_s *model, int nativecontents);

extern cvar_t r_mipskins;

typedef struct skinfileitem_s
{
	struct skinfileitem_s *next;
	char name[MAX_QPATH];
	char replacement[MAX_QPATH];
}
skinfileitem_t;

typedef struct skinfile_s
{
	struct skinfile_s *next;
	skinfileitem_t *items;
}
skinfile_t;

skinfile_t *Mod_LoadSkinFiles(void);
void Mod_FreeSkinFiles(skinfile_t *skinfile);
int Mod_CountSkinFiles(skinfile_t *skinfile);

void Mod_SnapVertices(int numcomponents, int numvertices, float *vertices, float snap);
int Mod_RemoveDegenerateTriangles(int numtriangles, const int *inelement3i, int *outelement3i, const float *vertex3f);

#endif	// MODEL_SHARED_H

