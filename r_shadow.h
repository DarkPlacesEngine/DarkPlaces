
#ifndef R_SHADOW_H
#define R_SHADOW_H

void R_Shadow_Init(void);
void R_Shadow_Volume(int numverts, int numtris, float *vertex, int *elements, int *neighbors, vec3_t relativelightorigin, float projectdistance, int visiblevolume);
void R_Shadow_VertexLight(int numverts, float *vertex, float *normals, vec3_t relativelightorigin, float lightradius2, float lightdistbias, float lightsubtract, float *lightcolor);
void R_Shadow_RenderLightThroughStencil(int numverts, int numtris, int *elements, vec3_t relativelightorigin, float *normals);
void R_Shadow_ClearStencil(void);

#endif
