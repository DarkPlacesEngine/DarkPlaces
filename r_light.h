
extern void R_CompleteLightPoint (vec3_t color, vec3_t p, int dynamic, mleaf_t *leaf);
extern void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
extern void R_DynamicLightPointNoMask(vec3_t color, vec3_t org);
extern void R_LightPoint (vec3_t color, vec3_t p);
extern void R_AnimateLight (void);
extern void R_LightModel (int numverts);
