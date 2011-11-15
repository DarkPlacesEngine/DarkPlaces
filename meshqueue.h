
#ifndef MESHQUEUE_H
#define MESHQUEUE_H

// VorteX: seems this value is hardcoded in other several defines as it's changing makes mess
#define MESHQUEUE_TRANSPARENT_BATCHSIZE 256

typedef enum meshqueue_sortcategory_e
{
	MESHQUEUE_SORT_SKY,
	MESHQUEUE_SORT_DISTANCE,
	MESHQUEUE_SORT_HUD,
}
meshqueue_sortcategory_t;

void R_MeshQueue_BeginScene(void);
void R_MeshQueue_AddTransparent(meshqueue_sortcategory_t category, const vec3_t center, void (*callback)(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist), const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight);
void R_MeshQueue_RenderTransparent(void);

#endif
