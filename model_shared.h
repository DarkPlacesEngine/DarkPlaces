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

typedef enum {mod_invalid, mod_brush, mod_sprite, mod_alias} modtype_t;

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
	rtexture_t *base; // original texture minus pants/shirt/glow
	rtexture_t *pants; // pants only (in greyscale)
	rtexture_t *shirt; // shirt only (in greyscale)
	rtexture_t *glow; // glow only
	rtexture_t *merged; // original texture minus glow
	rtexture_t *fog; // white texture with alpha of the base texture, NULL if not transparent
	rtexture_t *nmap; // normalmap (bumpmap for dot3)
	rtexture_t *gloss; // glossmap (for dot3)
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
	float *verts;
	int *elements;
	int *neighbors;
	// these are NULL after Mod_ShadowMesh_Finish is performed, only used
	// while building meshes
	shadowmeshvertexhash_t **vertexhashtable, *vertexhashentries;
}
shadowmesh_t;


#include "model_brush.h"
#include "model_sprite.h"
#include "model_alias.h"

typedef struct model_s
{
	char			name[MAX_QPATH];
	// model needs to be loaded if this is true
	qboolean		needload;
	// set if the model is used in current map, models which are not, are purged
	qboolean		used;
	// CRC of the file this model was loaded from, to reload if changed
	qboolean		crc;
	// true if this is the world model (I.E. defines what sky to use, and may contain submodels)
	qboolean		isworldmodel;
	// true if this model is a HalfLife .bsp file
	qboolean		ishlbsp;
	// true if this model was not successfully loaded and should be purged
	qboolean		error;

	// mod_brush, mod_alias, mod_sprite
	modtype_t		type;
	// LordHavoc: Q2/ZYM model support
	int				aliastype;
	// LordHavoc: if true (normally only for sprites) the model/sprite/bmodel is always rendered fullbright
	int				fullbright;
	// number of QC accessable frame(group)s in the model
	int				numframes;
	// whether to randomize animated framegroups
	synctype_t		synctype;

	// used for sprites and models
	int				numtris;
	// used for models
	int				numskins;
	// used by models
	int				numverts;

	// flags from the model file
	int				flags;
	// engine calculated flags, ones that can not be set in the file
	int				flags2;

	// all models use textures...
	rtexturepool_t	*texturepool;

	// volume occupied by the model
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

	// brush model specific
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

	int				numsurfedges;
	int				*surfedges;

	int				numclipnodes;
	dclipnode_t		*clipnodes;

	int				nummarksurfaces;
	int				*marksurfaces;

	hull_t			hulls[MAX_MAP_HULLS];

	int				numtextures;
	texture_t		*textures;

	msurface_t		**texturesurfacechains;

	qbyte			*visdata;
	qbyte			*lightdata;
	char			*entities;

	int				numportals;
	mportal_t		*portals;

	int				numportalpoints;
	mvertex_t		*portalpoints;

	int				numlights;
	mlight_t		*lights;

	// used only for casting dynamic shadow volumes
	shadowmesh_t	*shadowmesh;
	vec3_t			shadowmesh_mins, shadowmesh_maxs, shadowmesh_center;
	float			shadowmesh_radius;

	// pvs visibility marking
	mleaf_t			*pvsviewleaf;
	int				pvsviewleafnovis;
	int				pvsframecount;
	mleaf_t			*pvsleafchain;
	int				*pvssurflist;
	int				pvssurflistlength;

	// skin animation info
	animscene_t		*skinscenes; // [numskins]
	// skin frame info
	skinframe_t		*skinframes;

	animscene_t		*animscenes; // [numframes]

	// Q1 and Q2 models are the same after loading
	int				*mdlmd2data_indices;
	float			*mdlmd2data_texcoords;
	md2frame_t		*mdlmd2data_frames;
	trivertx_t		*mdlmd2data_pose;
	int				*mdlmd2data_triangleneighbors;

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

	int				sprnum_type;
	mspriteframe_t	*sprdata_frames;


	// functions used in both rendering modes
	// draw the model's sky polygons (only used by brush models)
	void(*DrawSky)(struct entity_render_s *ent);

	// functions used only in normal rendering mode
	// draw the model
	void(*Draw)(struct entity_render_s *ent);
	// draw a fake shadow for the model
	void(*DrawFakeShadow)(struct entity_render_s *ent);

	// functions used only in shadow volume rendering mode
	// draw a shadow volume for the model based on light source
	void(*DrawShadowVolume)(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius);
	// draw the lighting on a model (through stencil)
	void(*DrawLight)(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor);

	// memory pool for allocations
	mempool_t		*mempool;
}
model_t;

//============================================================================

// this can be used for anything without a valid texture
extern rtexture_t *r_notexture;
// every texture must be in a pool...
extern rtexturepool_t *r_notexturepool;

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
void Mod_ClearErrorModels (void);
model_t *Mod_FindName (const char *name);
model_t *Mod_ForName (const char *name, qboolean crash, qboolean checkdisk, qboolean isworldmodel);
void Mod_TouchModel (const char *name);
void Mod_UnloadModel (model_t *mod);

void Mod_ClearUsed(void);
void Mod_PurgeUnused(void);
void Mod_LoadModels(void);

extern model_t *loadmodel;
extern char loadname[32];	// for hunk tags

int Mod_FindTriangleWithEdge(const int *elements, int numtriangles, int start, int end);
void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles);
void Mod_ValidateElements(const int *elements, int numtriangles, int numverts, const char *filename, int fileline);
void Mod_BuildTextureVectorsAndNormals(int numverts, int numtriangles, const float *vertex, const float *texcoord, const int *elements, float *svectors, float *tvectors, float *normals);

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts);
shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh);
int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *v);
void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, float *vert0, float *vert1, float *vert2);
void Mod_ShadowMesh_AddPolygon(mempool_t *mempool, shadowmesh_t *mesh, int numverts, float *verts);
void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, int numverts, float *verts, int numtris, int *elements);
shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int initialnumtriangles);
shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh);
void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius);
void Mod_ShadowMesh_Free(shadowmesh_t *mesh);

#endif	// __MODEL__

