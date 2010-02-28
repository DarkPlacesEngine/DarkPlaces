#ifndef CSPROGS_H
#define CSPROGS_H

#define CL_NAME "client"

// LordHavoc: changed to match MAX_EDICTS
#define CL_MAX_EDICTS MAX_EDICTS

#define ENTMASK_ENGINE				1
#define ENTMASK_ENGINEVIEWMODELS	2
#define ENTMASK_NORMAL				4

#define VF_MIN			1	//(vector)
#define VF_MIN_X		2	//(float)
#define VF_MIN_Y		3	//(float)
#define VF_SIZE			4	//(vector) (viewport size)
#define VF_SIZE_X		5	//(float)
#define VF_SIZE_Y		6	//(float)
#define VF_VIEWPORT		7	//(vector, vector)
#define VF_FOV			8	//(vector)
#define VF_FOVX			9	//(float)
#define VF_FOVY			10	//(float)
#define VF_ORIGIN		11	//(vector)
#define VF_ORIGIN_X		12	//(float)
#define VF_ORIGIN_Y		13	//(float)
#define VF_ORIGIN_Z		14	//(float)
#define VF_ANGLES		15	//(vector)
#define VF_ANGLES_X		16	//(float)
#define VF_ANGLES_Y		17	//(float)
#define VF_ANGLES_Z		18	//(float)

#define VF_DRAWWORLD		19	//(float)	//actually world model and sky
#define VF_DRAWENGINESBAR	20	//(float)
#define VF_DRAWCROSSHAIR	21	//(float)

#define VF_CL_VIEWANGLES	33	//(vector)	//sweet thing for RPGs/...
#define VF_CL_VIEWANGLES_X	34	//(float)
#define VF_CL_VIEWANGLES_Y	35	//(float)
#define VF_CL_VIEWANGLES_Z	36	//(float)

#define VF_PERSPECTIVE		200 //(float)
#define VF_CLEARSCREEN		201 //(float)

#define RF_VIEWMODEL		1	// The entity is never drawn in mirrors. In engines with realtime lighting, it casts no shadows.
#define RF_EXTERNALMODEL	2	// The entity is appears in mirrors but not in the normal view. It does still cast shadows in engines with realtime lighting.
#define RF_DEPTHHACK		4	// The entity appears closer to the view than normal, either by scaling it wierdly or by just using a depthrange. This will usually be found in conjunction with RF_VIEWMODEL
#define RF_ADDITIVE			8	// Add the entity acording to it's alpha values instead of the normal blend
#define RF_USEAXIS			16	// When set, the entity will use the v_forward, v_right and v_up globals instead of it's angles field for orientation. Angles will be ignored compleatly.
								// Note that to use this properly, you'll NEED to use the predraw function to set the globals.
//#define RF_DOUBLESIDED		32
#define RF_USETRANSPARENTOFFSET 64 // Allows QC to customize origin used for transparent sorting via transparent_origin global, helps to fix transparent sorting bugs on a very large entities
#define RF_NOCULL				128 // do not cull this entity using r_cullentities, for large outdoor entities (asteroids on the sky. etc)

#define RF_FULLBRIGHT			256
#define RF_NOSHADOW				512

extern cvar_t csqc_progname;	//[515]: csqc crc check and right csprogs name according to progs.dat
extern cvar_t csqc_progcrc;
extern cvar_t csqc_progsize;

void CL_VM_PreventInformationLeaks(void);

qboolean MakeDownloadPacket(const char *filename, unsigned char *data, size_t len, int crc, int cnt, sizebuf_t *buf, int protocol);

qboolean CL_VM_GetEntitySoundOrigin(int entnum, vec3_t out);

qboolean CL_VM_TransformView(int entnum, matrix4x4_t *viewmatrix, mplane_t *clipplane, vec3_t visorigin);

#endif
