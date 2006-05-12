
#ifndef MESHQUEUE_H
#define MESHQUEUE_H

void R_MeshQueue_BeginScene(void);
void R_MeshQueue_AddTransparent(const vec3_t center, void (*callback)(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist), const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight);
void R_MeshQueue_RenderTransparent(void);

#endif
