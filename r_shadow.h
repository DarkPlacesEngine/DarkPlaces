
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
void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *vertices, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *lightcolor, const matrix4x4_t *matrix_worldtolight, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *vertices, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, const float *lightcolor, const matrix4x4_t *matrix_worldtolight, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
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

extern dlight_t *r_shadow_worldlightchain;

void R_Shadow_UpdateWorldLightSelection(void);

void R_RTLight_UpdateFromDLight(rtlight_t *rtlight, const dlight_t *light, int isstatic);
void R_RTLight_Compile(rtlight_t *rtlight);
void R_RTLight_Uncompile(rtlight_t *rtlight);

void R_ShadowVolumeLighting(int visiblevolumes);

#endif
