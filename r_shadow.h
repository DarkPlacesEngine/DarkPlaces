
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
extern cvar_t r_shadow_projectdistance;

extern mempool_t *r_shadow_mempool;

void R_Shadow_Init(void);
void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, float projectdistance, int nummarktris, const int *marktris);
void R_Shadow_VolumeFromBox(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, float projectdistance, const vec3_t mins, const vec3_t maxs);
void R_Shadow_VolumeFromSphere(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, float projectdistance, float radius);
#define LIGHTING_DIFFUSE 1
#define LIGHTING_SPECULAR 2
void R_Shadow_RenderLighting(int numverts, int numtriangles, const int *elements, const float *vertices, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, const float *lightcolor, const matrix4x4_t *matrix_worldtolight, const matrix4x4_t *matrix_worldtoattenuationxyz, const matrix4x4_t *matrix_worldtoattenuationz, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *glosstexture, rtexture_t *lightcubemap, int lightingflags);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderVolume(int numvertices, int numtriangles, const float *vertex3f, const int *element3i);
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

extern rtlight_t *r_shadow_compilingrtlight;

void R_RTLight_UpdateFromDLight(rtlight_t *rtlight, const dlight_t *light, int isstatic);
void R_RTLight_Compile(rtlight_t *rtlight);
void R_RTLight_Uncompile(rtlight_t *rtlight);

void R_ShadowVolumeLighting(int visiblevolumes);

int *R_Shadow_ResizeShadowElements(int numtris);

extern int maxshadowmark;
extern int numshadowmark;
extern int *shadowmark;
extern int *shadowmarklist;
extern int shadowmarkcount;
void R_Shadow_PrepareShadowMark(int numtris);

#endif
