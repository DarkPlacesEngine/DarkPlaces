
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_cull;
extern cvar_t r_shadow_debuglight;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_gloss2intensity;
extern cvar_t r_shadow_glossintensity;
extern cvar_t r_shadow_lightattenuationpower;
extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_portallight;
extern cvar_t r_shadow_projectdistance;
extern cvar_t r_shadow_realtime_dlight;
extern cvar_t r_shadow_realtime_dlight_shadows;
extern cvar_t r_shadow_realtime_dlight_portalculling;
extern cvar_t r_shadow_realtime_world;
extern cvar_t r_shadow_realtime_world_dlightshadows;
extern cvar_t r_shadow_realtime_world_lightmaps;
extern cvar_t r_shadow_realtime_world_shadows;
extern cvar_t r_shadow_realtime_world_compile;
extern cvar_t r_shadow_realtime_world_compilelight;
extern cvar_t r_shadow_realtime_world_compileshadow;
extern cvar_t r_shadow_scissor;
extern cvar_t r_shadow_shadow_polygonfactor;
extern cvar_t r_shadow_shadow_polygonoffset;
extern cvar_t r_shadow_singlepassvolumegeneration;
extern cvar_t r_shadow_texture3d;
extern cvar_t r_shadow_visiblelighting;
extern cvar_t r_shadow_visiblevolumes;
extern cvar_t gl_ext_stenciltwoside;

extern mempool_t *r_shadow_mempool;

void R_Shadow_Init(void);
void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, float projectdistance, int nummarktris, const int *marktris);
void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs);
void R_Shadow_RenderLighting(int firstvertex, int numvertices, int numtriangles, const int *elements, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, const float *relativelightorigin, const float *relativeeyeorigin, const float *lightcolorbase, const float *lightcolorpants, const float *lightcolorshirt, const matrix4x4_t *matrix_modeltolight, const matrix4x4_t *matrix_modeltoattenuationxyz, const matrix4x4_t *matrix_modeltoattenuationz, rtexture_t *basetexture, rtexture_t *pantstexture, rtexture_t *shirttexture, rtexture_t *bumptexture, rtexture_t *glosstexture, rtexture_t *lightcubemap, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int visiblelighting);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderVolume(int numvertices, int numtriangles, const float *vertex3f, const int *element3i);
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

void R_ShadowVolumeLighting(int visiblelighting, int visiblevolumes);

int *R_Shadow_ResizeShadowElements(int numtris);

extern int maxshadowmark;
extern int numshadowmark;
extern int *shadowmark;
extern int *shadowmarklist;
extern int shadowmarkcount;
void R_Shadow_PrepareShadowMark(int numtris);

#endif
