
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_realtime;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_debuglight;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_bumpscale_basetexture;

void R_Shadow_Init(void);
void R_Shadow_Volume(int numverts, int numtris, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance);
void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_worldtofilter, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, const matrix4x4_t *matrix_worldtofilter, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements);
void R_Shadow_RenderShadowMeshVolume(shadowmesh_t *mesh);
void R_Shadow_Stage_Begin(void);
void R_Shadow_Stage_ShadowVolumes(void);
void R_Shadow_Stage_LightWithShadows(void);
void R_Shadow_Stage_LightWithoutShadows(void);
void R_Shadow_Stage_End(void);
//int R_Shadow_ScissorForBBoxAndSphere(const float *mins, const float *maxs, const float *origin, float radius);
int R_Shadow_ScissorForBBox(const float *mins, const float *maxs);

typedef struct worldlight_s
{
	// saved properties
	vec3_t origin;
	vec_t lightradius;
	vec3_t light;
	vec3_t angles;
	int castshadows;
	char *cubemapname;

	// shadow volumes are done entirely in model space, so there are no matrices for dealing with them...

	// note that the world to light matrices are inversely scaled (divided) by lightradius

	// matrix for transforming world coordinates to light filter coordinates
	//matrix4x4_t matrix_worldtofilter;
	// based on worldtofilter this transforms -1 to +1 to 0 to 1 for purposes
	// of attenuation texturing in full 3D (z result often ignored)
	//matrix4x4_t matrix_worldtoattenuationxyz;
	// this transforms only the Z to S, and T is always 0.5
	//matrix4x4_t matrix_worldtoattenuationz;

	// generated properties
	vec3_t mins;
	vec3_t maxs;
	vec_t cullradius;
	struct worldlight_s *next;
	msurface_t **surfaces;
	int numsurfaces;
	mleaf_t **leafs;
	int numleafs;
	rtexture_t *cubemap;
	int style;
	shadowmesh_t *shadowvolume;
	int selected;
}
worldlight_t;

extern worldlight_t *r_shadow_worldlightchain;

// 0 = normal, 1 = dynamic light shadows, 2 = world and dynamic light shadows
extern int r_shadow_lightingmode;
void R_Shadow_UpdateLightingMode(void);

void R_Shadow_UpdateWorldLightSelection(void);

#endif
