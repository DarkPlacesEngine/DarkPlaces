
typedef struct
{
	vec3_t origin;
	vec_t cullradius2; // only for culling comparisons, squared version
	vec3_t light; // the brightness of the light
	vec_t cullradius; // only for culling comparisons
	vec_t lightsubtract; // to avoid sudden brightness change at cullradius, subtract this
	entity_render_t *ent; // owner of this light
}
rdlight_t;

extern int r_numdlights;
extern rdlight_t r_dlight[MAX_DLIGHTS];

void R_BuildLightList(void);
void R_AnimateLight(void);
void R_MarkLights(void);
void R_DrawCoronas(void);
void R_CompleteLightPoint(vec3_t color, vec3_t p, int dynamic, mleaf_t *leaf);
void R_LightModel(int numverts, float colorr, float colorg, float colorb, int worldcoords);
