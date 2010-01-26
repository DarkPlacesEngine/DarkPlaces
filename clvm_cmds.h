#ifndef __CLVM_CMDS_H__
#define __CLVM_CMDS_H__

int CL_GetPitchSign(prvm_edict_t *ent);
int CL_GetTagMatrix (matrix4x4_t *out, prvm_edict_t *ent, int tagindex);
void CL_GetEntityMatrix (prvm_edict_t *ent, matrix4x4_t *out, qboolean viewmatrix);

/* These are VM built-ins that originate in the client-side programs support
   but are reused by the other programs (usually the menu). */

void VM_CL_setmodel (void);
void VM_CL_precache_model (void);
void VM_CL_setorigin (void);

void VM_CL_R_AddDynamicLight (void);
void VM_CL_R_ClearScene (void);
void VM_CL_R_AddEntities (void);
void VM_CL_R_AddEntity (void);
void VM_CL_R_SetView (void);
void VM_CL_R_RenderScene (void);
void VM_CL_R_LoadWorldModel (void);

void VM_CL_R_PolygonBegin (void);
void VM_CL_R_PolygonVertex (void);
void VM_CL_R_PolygonEnd (void);
/* VMs exposing the polygon calls must call this on Init/Reset */
void VM_Polygons_Reset(void);

void VM_CL_setattachment(void);
void VM_CL_gettagindex(void);
void VM_CL_gettaginfo(void);

#endif /* __CLVM_CMDS_H__ */
