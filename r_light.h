
#ifndef R_LIGHT_H
#define R_LIGHT_H

typedef struct
{
	vec3_t origin;
	vec_t cullradius2; // only for culling comparisons, squared version
	vec3_t light; // the brightness of the light
	vec_t cullradius; // only for culling comparisons
	vec_t subtract; // to avoid sudden brightness change at cullradius, subtract this
	entity_render_t *ent; // owner of this light
}
rdlight_t;

extern int r_numdlights;
extern rdlight_t r_dlight[MAX_DLIGHTS];

void R_BuildLightList(void);
void R_AnimateLight(void);
void R_MarkLights(entity_render_t *ent);
void R_DrawCoronas(void);
void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic, const mleaf_t *leaf);
int R_LightModel(float *ambient4f, float *diffusecolor, float *diffusenormal, const entity_render_t *ent, float colorr, float colorg, float colorb, float colora, int worldcoords);
void R_LightModel_CalcVertexColors(const float *ambientcolor4f, const float *diffusecolor, const float *diffusenormal, int numverts, const float *vertex3f, const float *normal3f, float *color4f);
void R_UpdateEntLights(entity_render_t *ent);

#endif

