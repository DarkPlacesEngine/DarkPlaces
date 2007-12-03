
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_debuglight;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_gloss2intensity;
extern cvar_t r_shadow_glossintensity;
extern cvar_t r_shadow_glossexponent;
extern cvar_t r_shadow_lightattenuationpower;
extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_lightradiusscale;
extern cvar_t r_shadow_portallight;
extern cvar_t r_shadow_projectdistance;
extern cvar_t r_shadow_frontsidecasting;
extern cvar_t r_shadow_realtime_dlight;
extern cvar_t r_shadow_realtime_dlight_shadows;
extern cvar_t r_shadow_realtime_dlight_svbspculling;
extern cvar_t r_shadow_realtime_dlight_portalculling;
extern cvar_t r_shadow_realtime_world;
extern cvar_t r_shadow_realtime_world_lightmaps;
extern cvar_t r_shadow_realtime_world_shadows;
extern cvar_t r_shadow_realtime_world_compile;
extern cvar_t r_shadow_realtime_world_compileshadow;
extern cvar_t r_shadow_realtime_world_compilesvbsp;
extern cvar_t r_shadow_realtime_world_compileportalculling;
extern cvar_t r_shadow_scissor;
extern cvar_t r_shadow_culltriangles;
extern cvar_t r_shadow_polygonfactor;
extern cvar_t r_shadow_polygonoffset;
extern cvar_t r_shadow_singlepassvolumegeneration;
extern cvar_t r_shadow_texture3d;
extern cvar_t gl_ext_separatestencil;
extern cvar_t gl_ext_stenciltwoside;

void R_Shadow_Init(void);
void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, const vec3_t projectdirection, float projectdistance, int nummarktris, const int *marktris);
void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, const vec3_t projectdirection, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs);
void R_Shadow_RenderLighting(int firstvertex, int numvertices, int numtriangles, const int *element3i, int element3i_bufferobject, size_t element3i_bufferoffset);
void R_Shadow_RenderMode_Begin(void);
void R_Shadow_RenderMode_ActiveLight(rtlight_t *rtlight);
void R_Shadow_RenderMode_Reset(void);
void R_Shadow_RenderMode_StencilShadowVolumes(qboolean clearstencil);
void R_Shadow_RenderMode_Lighting(qboolean stenciltest, qboolean transparent);
void R_Shadow_RenderMode_VisibleShadowVolumes(void);
void R_Shadow_RenderMode_VisibleLighting(qboolean stenciltest, qboolean transparent);
void R_Shadow_RenderMode_End(void);
void R_Shadow_SetupEntityLight(const entity_render_t *ent);

void R_Shadow_RenderVolume(int numvertices, int numtriangles, const float *vertex3f, const int *element3i);
qboolean R_Shadow_ScissorForBBox(const float *mins, const float *maxs);

// these never change, they are used to create attenuation matrices
extern matrix4x4_t matrix_attenuationxyz;
extern matrix4x4_t matrix_attenuationz;

rtexture_t *R_Shadow_Cubemap(const char *basename);

void R_Shadow_UpdateWorldLightSelection(void);

extern rtlight_t *r_shadow_compilingrtlight;

void R_RTLight_Update(rtlight_t *rtlight, int isstatic, matrix4x4_t *matrix, vec3_t color, int style, const char *cubemapname, qboolean shadow, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags);
void R_RTLight_Compile(rtlight_t *rtlight);
void R_RTLight_Uncompile(rtlight_t *rtlight);

void R_ShadowVolumeLighting(qboolean visible);
void R_DrawCoronas(void);

int *R_Shadow_ResizeShadowElements(int numtris);

extern int maxshadowmark;
extern int numshadowmark;
extern int *shadowmark;
extern int *shadowmarklist;
extern int shadowmarkcount;
void R_Shadow_PrepareShadowMark(int numtris);

void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic);

#endif
