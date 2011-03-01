
#ifndef CL_COLLISION_H
#define CL_COLLISION_H

float CL_SelectTraceLine(const vec3_t start, const vec3_t end, vec3_t impact, vec3_t normal, int *hitent, entity_render_t *ignoreent);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);

dp_model_t *CL_GetModelByIndex(int modelindex);
dp_model_t *CL_GetModelFromEdict(prvm_edict_t *ed);

void CL_LinkEdict(prvm_edict_t *ent);
int CL_GenericHitSuperContentsMask(const prvm_edict_t *edict);
trace_t CL_TraceBox(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask, qboolean hitnetworkbrushmodels, qboolean hitnetworkplayers, int *hitnetworkentity, qboolean hitcsqcentities);
trace_t CL_TraceLine(const vec3_t start, const vec3_t end, int type, prvm_edict_t *passedict, int hitsupercontentsmask, qboolean hitnetworkbrushmodels, qboolean hitnetworkplayers, int *hitnetworkentity, qboolean hitcsqcentities, qboolean hitsurfaces);
trace_t CL_TracePoint(const vec3_t start, int type, prvm_edict_t *passedict, int hitsupercontentsmask, qboolean hitnetworkbrushmodels, qboolean hitnetworkplayers, int *hitnetworkentity, qboolean hitcsqcentities);
#define CL_PointSuperContents(point) (CL_TracePoint((point), sv_gameplayfix_swiminbmodels.integer ? MOVE_NOMONSTERS : MOVE_WORLDONLY, NULL, 0, true, false, NULL, false).startsupercontents)

#endif
