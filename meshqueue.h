
#ifndef MESHQUEUE_H
#define MESHQUEUE_H

void R_MeshQueue_Init(void);
void R_MeshQueue_Add(void (*callback)(const void *data1, int data2), const void *data1, int data2);
void R_MeshQueue_AddTransparent(const vec3_t center, void (*callback)(const void *data1, int data2), const void *data1, int data2);
void R_MeshQueue_BeginScene(void);
void R_MeshQueue_Render(void);
void R_MeshQueue_RenderTransparent(void);
void R_MeshQueue_EndScene(void);

#endif
