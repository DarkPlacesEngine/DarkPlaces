
#ifndef CL_COLLISION_H
#define CL_COLLISION_H

trace_t CL_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitbmodels, int *hitent, int hitsupercontentsmask, qboolean hitplayers);
float CL_SelectTraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int *hitent, entity_render_t *ignoreent);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);
int CL_PointSuperContents(const vec3_t p);

#endif
