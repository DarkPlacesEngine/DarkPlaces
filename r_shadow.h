
#ifndef R_SHADOW_H
#define R_SHADOW_H

extern cvar_t r_light_realtime;
extern cvar_t r_light_quality;
extern cvar_t r_light_gloss;
extern cvar_t r_light_debuglight;

void R_Shadow_Init(void);
void R_Shadow_Volume(int numverts, int numtris, float *vertex, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance, int visiblevolume);
void R_Shadow_RenderLighting(int numverts, int numtriangles, const int *elements, const float *svectors, const float *tvectors, const float *normals, const float *texcoords, const float *relativelightorigin, const float *relativeeyeorigin, float lightradius, const float *lightcolor, rtexture_t *basetexture, rtexture_t *glosstexture, rtexture_t *bumptexture, rtexture_t *lightcubemap);
void R_Shadow_ClearStencil(void);

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements, int visiblevolume);
void R_Shadow_Stage_Begin(void);
void R_Shadow_Stage_ShadowVolumes(void);
void R_Shadow_Stage_Light(void);
void R_Shadow_Stage_End(void);

#endif
