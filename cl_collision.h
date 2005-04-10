
#ifndef CL_COLLISION_H
#define CL_COLLISION_H

// if contents is not zero, it will impact on content changes
// (leafs matching contents are considered empty, others are solid)
extern int cl_traceline_startcontents; // set by TraceLine

float CL_TraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int hitbmodels, int *hitent, int hitsupercontentsmask);
float CL_SelectTraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int *hitent, entity_render_t *ignoreent);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);
int CL_PointQ1Contents(const vec3_t p);
int CL_PointSuperContents(const vec3_t p);

#endif
