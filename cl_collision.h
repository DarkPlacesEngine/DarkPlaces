
#ifndef CL_COLLISION_H
#define CL_COLLISION_H

// if contents is not zero, it will impact on content changes
// (leafs matching contents are considered empty, others are solid)
extern int cl_traceline_endcontents; // set by TraceLine

// need to call this sometime before using TraceLine with hitbmodels
void CL_TraceLine_ScanForBModels(void);

float CL_TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int contents, int hitbmodels);

#endif
