
#ifndef R_SHADOW_H
#define R_SHADOW_H

void R_Shadow_Init(void);
void R_ShadowVolume(int numverts, int numtris, int *elements, int *neighbors, vec3_t relativelightorigin, float projectdistance, int visiblevolume);

#endif
