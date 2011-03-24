
#ifndef R_SHADOW_H
#define R_SHADOW_H

#define R_SHADOW_SHADOWMAP_NUMCUBEMAPS 8

extern cvar_t r_shadow_bumpscale_basetexture;
extern cvar_t r_shadow_bumpscale_bumpmap;
extern cvar_t r_shadow_debuglight;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_gloss2intensity;
extern cvar_t r_shadow_glossintensity;
extern cvar_t r_shadow_glossexponent;
extern cvar_t r_shadow_gloss2exponent;
extern cvar_t r_shadow_glossexact;
extern cvar_t r_shadow_lightattenuationpower;
extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_lightradiusscale;
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
extern cvar_t r_shadow_polygonfactor;
extern cvar_t r_shadow_polygonoffset;
extern cvar_t r_shadow_texture3d;
extern cvar_t gl_ext_separatestencil;
extern cvar_t gl_ext_stenciltwoside;

// used by shader for bouncegrid feature
extern rtexture_t *r_shadow_bouncegridtexture;
extern matrix4x4_t r_shadow_bouncegridmatrix;
extern vec_t r_shadow_bouncegridintensity;
extern qboolean r_shadow_bouncegriddirectional;

void R_Shadow_Init(void);
qboolean R_Shadow_ShadowMappingEnabled(void);
void R_Shadow_VolumeFromList(int numverts, int numtris, const float *invertex3f, const int *elements, const int *neighbors, const vec3_t projectorigin, const vec3_t projectdirection, float projectdistance, int nummarktris, const int *marktris, vec3_t trismins, vec3_t trismaxs);
void R_Shadow_ShadowMapFromList(int numverts, int numtris, const float *vertex3f, const int *elements, int numsidetris, const int *sidetotals, const unsigned char *sides, const int *sidetris);
void R_Shadow_MarkVolumeFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const vec3_t projectorigin, const vec3_t projectdirection, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs);
int R_Shadow_CalcTriangleSideMask(const vec3_t p1, const vec3_t p2, const vec3_t p3, float bias);
int R_Shadow_CalcSphereSideMask(const vec3_t p1, float radius, float bias);
int R_Shadow_ChooseSidesFromBox(int firsttriangle, int numtris, const float *invertex3f, const int *elements, const matrix4x4_t *worldtolight, const vec3_t projectorigin, const vec3_t projectdirection, const vec3_t lightmins, const vec3_t lightmaxs, const vec3_t surfacemins, const vec3_t surfacemaxs, int *totals);
void R_Shadow_RenderLighting(int texturenumsurfaces, const msurface_t **texturesurfacelist);
void R_Shadow_RenderMode_Begin(void);
void R_Shadow_RenderMode_ActiveLight(const rtlight_t *rtlight);
void R_Shadow_RenderMode_Reset(void);
void R_Shadow_RenderMode_StencilShadowVolumes(qboolean zpass);
void R_Shadow_RenderMode_Lighting(qboolean stenciltest, qboolean transparent, qboolean shadowmapping);
void R_Shadow_RenderMode_DrawDeferredLight(qboolean stenciltest, qboolean shadowmapping);
void R_Shadow_RenderMode_VisibleShadowVolumes(void);
void R_Shadow_RenderMode_VisibleLighting(qboolean stenciltest, qboolean transparent);
void R_Shadow_RenderMode_End(void);
void R_Shadow_ClearStencil(void);
void R_Shadow_SetupEntityLight(const entity_render_t *ent);

qboolean R_Shadow_ScissorForBBox(const float *mins, const float *maxs);

// these never change, they are used to create attenuation matrices
extern matrix4x4_t matrix_attenuationxyz;
extern matrix4x4_t matrix_attenuationz;

void R_Shadow_UpdateWorldLightSelection(void);

extern rtlight_t *r_shadow_compilingrtlight;

void R_RTLight_Update(rtlight_t *rtlight, int isstatic, matrix4x4_t *matrix, vec3_t color, int style, const char *cubemapname, int shadow, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags);
void R_RTLight_Compile(rtlight_t *rtlight);
void R_RTLight_Uncompile(rtlight_t *rtlight);

void R_Shadow_PrepareLights(void);
void R_Shadow_DrawPrepass(void);
void R_Shadow_DrawLights(void);
void R_Shadow_DrawCoronas(void);

extern int maxshadowmark;
extern int numshadowmark;
extern int *shadowmark;
extern int *shadowmarklist;
extern int shadowmarkcount;
void R_Shadow_PrepareShadowMark(int numtris);

extern int maxshadowsides;
extern int numshadowsides;
extern unsigned char *shadowsides;
extern int *shadowsideslist;
void R_Shadow_PrepareShadowSides(int numtris);

void R_Shadow_PrepareModelShadows(void);

#define LP_LIGHTMAP		1
#define LP_RTWORLD		2
#define LP_DYNLIGHT		4
void R_LightPoint(vec3_t color, const vec3_t p, const int flags);
void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, const int flags);

#endif
