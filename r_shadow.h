
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_realtime_world;
extern cvar_t r_shadow_realtime_world_lightmaps;
extern cvar_t r_shadow_realtime_dlight;
extern cvar_t r_shadow_visiblevolumes;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_debuglight;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_shadow_worldshadows;
extern cvar_t r_shadow_dlightshadows;

void R_Shadow_Init(void);
void R_Shadow_Volume(int numverts, int numtris, const float *invertex3f, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance);
void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *vertices, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_worldtolight, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *vertices, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_worldtolight, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderShadowMeshVolume(shadowmesh_t *mesh);
void R_Shadow_Stage_Begin(void);
void R_Shadow_LoadWorldLightsIfNeeded(void);
void R_Shadow_Stage_ShadowVolumes(void);
void R_Shadow_Stage_LightWithShadows(void);
void R_Shadow_Stage_LightWithoutShadows(void);
void R_Shadow_Stage_End(void);
int R_Shadow_ScissorForBBox(const float *mins, const float *maxs);

// these never change, they are used to create attenuation matrices
extern matrix4x4_t matrix_attenuationxyz;
extern matrix4x4_t matrix_attenuationz;

rtexture_t *R_Shadow_Cubemap(const char *basename);

typedef struct worldlight_s
{
	// saved properties
	vec3_t origin;
	vec3_t angles;
	vec3_t color;
	vec_t radius;
	vec_t corona;
	int drawshadows;
	char *cubemapname;

	// shadow volumes are done entirely in model space, so there are no matrices for dealing with them...

	// note that the world to light matrices are inversely scaled (divided) by lightradius

	// matrix for transforming world coordinates to light filter coordinates
	//matrix4x4_t matrix_worldtolight;
	// based on worldtolight this transforms -1 to +1 to 0 to 1 for purposes
	// of attenuation texturing in full 3D (z result often ignored)
	//matrix4x4_t matrix_worldtoattenuationxyz;
	// this transforms only the Z to S, and T is always 0.5
	//matrix4x4_t matrix_worldtoattenuationz;

	// generated properties
	vec3_t mins;
	vec3_t maxs;
	vec_t cullradius;
	struct worldlight_s *next;
	rtexture_t *cubemap;
	int style;
	int selected;

	matrix4x4_t matrix_lighttoworld;
	matrix4x4_t matrix_worldtolight;
	matrix4x4_t matrix_worldtoattenuationxyz;
	matrix4x4_t matrix_worldtoattenuationz;

	// premade shadow volumes and lit surfaces to render
	shadowmesh_t *meshchain_shadow;
	shadowmesh_t *meshchain_light;
	
	// used for visibility testing
	int numclusters;
	int *clusterindices;
}
worldlight_t;

extern worldlight_t *r_shadow_worldlightchain;

void R_Shadow_UpdateWorldLightSelection(void);

void R_Shadow_DrawStaticWorldLight_Shadow(worldlight_t *light, matrix4x4_t *matrix);
void R_Shadow_DrawStaticWorldLight_Light(worldlight_t *light, matrix4x4_t *matrix, vec3_t relativelightorigin, vec3_t relativeeyeorigin, float lightradius, float *lightcolor, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz);

#endif
