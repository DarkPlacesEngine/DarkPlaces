
#ifndef MESHQUEUE_H
#define MESHQUEUE_H

void R_MeshQueue_Init(void);
void R_MeshQueue_Add(void (*callback)(void *data1, int data2), void *data1, int data2);
void R_MeshQueue_AddTransparent(vec3_t center, void (*callback)(void *data1, int data2), void *data1, int data2);
void R_MeshQueue_BeginScene(void);
void R_MeshQueue_EndScene(void);

#endif
