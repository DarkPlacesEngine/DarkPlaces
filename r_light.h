
#ifndef R_LIGHT_H
#define R_LIGHT_H

extern int r_numdlights;
extern dlight_t r_dlight[MAX_DLIGHTS];

void R_UpdateLights(void);
#ifdef LHREMOVESOON
void R_MarkLights(entity_render_t *ent);
#endif
void R_DrawCoronas(void);
void R_CompleteLightPoint(vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal, const vec3_t p, int dynamic, const mleaf_t *leaf);
int R_LightModel(float *ambient4f, float *diffusecolor, float *diffusenormal, const entity_render_t *ent, float colorr, float colorg, float colorb, float colora, int worldcoords);
void R_LightModel_CalcVertexColors(const float *ambientcolor4f, const float *diffusecolor, const float *diffusenormal, int numverts, const float *vertex3f, const float *normal3f, float *color4f);
void R_UpdateEntLights(entity_render_t *ent);

#endif

