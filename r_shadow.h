
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_shadow_lightattenuationscale;
extern cvar_t r_shadow_lightintensityscale;
extern cvar_t r_shadow_realtime;
extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_debuglight;

void R_Shadow_Init(void);
void R_Shadow_Volume(int numverts, int numtris, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance);
void R_Shadow_DiffuseLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, float lightradius, const float *lightcolor, rtexture_t *basetexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_SpecularLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements);
void R_Shadow_RenderShadowMeshVolume(shadowmesh_t *mesh);
void R_Shadow_Stage_Begin(void);
void R_Shadow_Stage_ShadowVolumes(void);
void R_Shadow_Stage_Light(void);
void R_Shadow_Stage_End(void);
int R_Shadow_ScissorForBBoxAndSphere(const float *mins, const float *maxs, const float *origin, float radius);

typedef struct worldlight_s
{
	vec3_t origin;
	vec3_t light;
	vec3_t mins;
	vec3_t maxs;
	vec_t lightradius;
	vec_t cullradius;
	struct worldlight_s *next;
	msurface_t **surfaces;
	int numsurfaces;
	mleaf_t **leafs;
	int numleafs;
	rtexture_t *cubemap;
	char *cubemapname;
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
