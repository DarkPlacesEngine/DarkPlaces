
#ifndef CGAME_API_H
#define CGAME_API_H

// the CG state is reset quite harshly each time the client
// connects/disconnects (to enforce the idea of cgame dying between levels),
// and the Pre/PostNetworkFrame functions are only called while connected
// (this does mean that all memory is freed, Init will be called again, etc)

typedef struct cgdrawentity_s
{
	float origin[3];
	float angles[3];
	float alpha;
	float scale;
	int model; // index gotten from engine using CGVM_Model
	int frame1;
	int frame2;
	float framelerp;
	int skinnum;
}
cgdrawentity_t;

typedef struct cgdrawlight_s
{
	float origin[3];
	float light[3];
}
cgdrawlight_t;

typedef struct cgphysentity_s
{
	int entnum; // reported by tracing, for use by cgame code
	int padding; // unused
	float mins[3];
	float maxs[3];
}
cgphysentity_t;

// engine functions the CG code can call
void CGVM_RegisterNetworkCode(const unsigned char num, void (*netcode)(unsigned char num));
unsigned char CGVM_MSG_ReadByte(void);
short CGVM_MSG_ReadShort(void);
int CGVM_MSG_ReadLong(void);
float CGVM_MSG_ReadFloat(void);
float CGVM_MSG_ReadCoord(void);
float CGVM_MSG_ReadAngle(void);
float CGVM_MSG_ReadPreciseAngle(void);
void CGVM_Draw_Entity(const cgdrawentity_t *e);
void CGVM_Draw_Light(const cgdrawlight_t *l);
void *CGVM_Malloc(const int size);
void CGVM_Free(void *mem);
float CGVM_RandomRange(const float r1, const float r2);
float CGVM_TracePhysics(const float *start, const float *end, const float *worldmins, const float *worldmaxs, const float *entitymins, const float *entitymaxs, const cgphysentity_t *physentities, const int numphysentities, float *impactpos, float *impactnormal, int *impactentnum);
float CGVM_GetCvarFloat(const char *name);
int CGVM_GetCvarInt(const char *name);
char *CGVM_GetCvarString(const char *name);
double CGVM_Time(void);
int CGVM_Model(const char *name);
// more will be added

// engine called functions
void CG_Init(void);
void CG_Frame(double time);
// more might be added

#endif
