
#include "quakedef.h"
#include "cgame_api.h"
#include "cl_collision.h"

#define CGVM_RENDERENTITIES 1024

static entity_render_t cgvm_renderentities[CGVM_RENDERENTITIES];
static int cgvm_renderentity;

static mempool_t *cgvm_mempool;

static void (*cgvm_networkcode[256])(unsigned char num);

static unsigned char *cgvm_netbuffer;
static int cgvm_netbufferlength;
static int cgvm_netbufferpos;

#define MAX_CGVM_MODELS 128
#define MAX_CGVM_MODELNAME 32
static char cgvm_modelname[MAX_CGVM_MODELS][MAX_CGVM_MODELNAME];
static model_t *cgvm_model[MAX_CGVM_MODELS];

void CL_CGVM_Init(void)
{
	cgvm_mempool = Mem_AllocPool("CGVM", 0, NULL);
}

void CL_CGVM_Shutdown(void)
{
	Mem_FreePool (&cgvm_mempool);
}

void CL_CGVM_Clear(void)
{
	Mem_EmptyPool(cgvm_mempool);
	memset(cgvm_networkcode, 0, sizeof(cgvm_networkcode));
	memset(cgvm_modelname, 0, sizeof(cgvm_modelname));
	memset(cgvm_model, 0, sizeof(cgvm_model));
}

void CL_CGVM_Frame(void)
{
	cgvm_renderentity = 0;
	CG_Frame(cl.time); // API call
}

// starts the cgame code
void CL_CGVM_Start(void)
{
	CL_CGVM_Clear();
	CG_Init(); // API call
}

void CL_CGVM_ParseNetwork(unsigned char *netbuffer, int length)
{
	int num;
	cgvm_netbuffer = netbuffer;
	cgvm_netbufferlength = length;
	cgvm_netbufferpos = 0;
	while (cgvm_netbufferpos < cgvm_netbufferlength)
	{
		num = CGVM_MSG_ReadByte();
		if (cgvm_networkcode[num])
			cgvm_networkcode[num]((unsigned char)num);
		else
			Host_Error("CL_CGVM_ParseNetwork: unregistered network code %i", num);
	}
}








void CGVM_RegisterNetworkCode(const unsigned char num, void (*netcode)(unsigned char num))
{
	if (cgvm_networkcode[num])
		Host_Error("CGVM_RegisterNetworkCode: value %i already registered", num);
	cgvm_networkcode[num] = netcode;
}

unsigned char CGVM_MSG_ReadByte(void)
{
	if (cgvm_netbufferpos < cgvm_netbufferlength)
		return cgvm_netbuffer[cgvm_netbufferpos++];
	else
		return 0;
}

short CGVM_MSG_ReadShort(void)
{
	short num;
	num = CGVM_MSG_ReadByte() | (CGVM_MSG_ReadByte() << 8);
	return num;
}

int CGVM_MSG_ReadLong(void)
{
	int num;
	num = CGVM_MSG_ReadByte() | (CGVM_MSG_ReadByte() << 8) | (CGVM_MSG_ReadByte() << 16) | (CGVM_MSG_ReadByte() << 24);
	return num;
}

float CGVM_MSG_ReadFloat(void)
{
	unsigned int num;
	num = CGVM_MSG_ReadByte() | (CGVM_MSG_ReadByte() << 8) | (CGVM_MSG_ReadByte() << 16) | (CGVM_MSG_ReadByte() << 24);
	return *((float *)&num);
}

float CGVM_MSG_ReadCoord(void)
{
	return CGVM_MSG_ReadFloat();
}

float CGVM_MSG_ReadAngle(void)
{
	return CGVM_MSG_ReadByte() * 360.0f / 256.0f;
}

float CGVM_MSG_ReadPreciseAngle(void)
{
	return ((unsigned short)CGVM_MSG_ReadShort()) * 360.0f / 65536.0f;
}

void CGVM_Draw_Entity(const cgdrawentity_t *e)
{
	entity_render_t *r;
	//Con_Printf("CGVM_Draw_Entity: origin %f %f %f angles %f %f %f alpha %f scale %f model %i frame1 %i frame2 %i framelerp %f skinnum %i\n", e->origin[0], e->origin[1], e->origin[2], e->angles[0], e->angles[1], e->angles[2], e->alpha, e->scale, e->model, e->frame1, e->frame2, e->framelerp, e->skinnum);

	if (!e->model)
		return;

	if (cgvm_renderentity >= CGVM_RENDERENTITIES
	 || r_refdef.numentities >= r_refdef.maxentities)
		return;

	r = cgvm_renderentities + cgvm_renderentity;
	VectorCopy(e->origin, r->origin);
	VectorCopy(e->angles, r->angles);
	r->alpha = e->alpha;
	r->scale = e->scale;
	if (e->model < 0 || e->model >= MAX_CGVM_MODELS || !cgvm_model[e->model])
	{
		Con_Printf("CGVM_Draw_Entity: invalid model index %i\n", e->model);
		return;
	}
	r->model = cgvm_model[e->model];

	r->frame = e->frame2;
	// FIXME: support colormapping?
	r->colormap = -1;
	// FIXME: support effects?
	r->effects = 0;
	r->skinnum = e->skinnum;
	// FIXME: any flags worth setting?
	r->flags = 0;

	r->frame1 = e->frame1;
	r->frame2 = e->frame2;
	r->framelerp = e->framelerp;
	r->frame1time = 0;
	r->frame2time = 0;

	r_refdef.entities[r_refdef.numentities++] = r;

	cgvm_renderentity++;
}

void CGVM_Draw_Light(const cgdrawlight_t *l)
{
	matrix4x4_t matrix;
	Matrix4x4_CreateTranslate(&matrix, l->origin[0], l->origin[1], l->origin[2]);
	CL_AllocDlight(NULL, &matrix, l->radius, l->color[0], l->color[1], l->color[2], 0, 0, 0, -1, true, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

void *CGVM_Malloc(const int size)
{
	return Mem_Alloc(cgvm_mempool, size);
}

void CGVM_Free(void *mem)
{
	Mem_Free(mem);
}

float CGVM_RandomRange(const float r1, const float r2)
{
	return lhrandom(r1, r2);
}

float CGVM_TracePhysics(const float *start, const float *end, const float *worldmins, const float *worldmaxs, const float *entitymins, const float *entitymaxs, const cgphysentity_t *physentities, const int numphysentities, float *impactpos, float *impactnormal, int *impactentnum)
{
	trace_t trace;
	// FIXME: do tracing agains network entities and physentities here
	trace = CL_TraceBox(start, vec3_origin, vec3_origin, end, true, NULL, SUPERCONTENTS_SOLID, false);
	VectorCopy(trace.endpos, impactpos);
	VectorCopy(trace.plane.normal, impactnormal);
	*impactentnum = -1;
	return trace.fraction;
}

char *CGVM_GetCvarString(const char *name)
{
	cvar_t *cvar;
	cvar = Cvar_FindVar((char *)name);
	if (cvar)
		return cvar->string;
	else
		return 0;
}

float CGVM_GetCvarFloat(const char *name)
{
	cvar_t *cvar;
	cvar = Cvar_FindVar((char *)name);
	if (cvar)
		return cvar->value;
	else
		return 0;
}

int CGVM_GetCvarInt(const char *name)
{
	cvar_t *cvar;
	cvar = Cvar_FindVar((char *)name);
	if (cvar)
		return cvar->integer;
	else
		return 0;
}

double CGVM_Time(void)
{
	return cl.time;
}

int CGVM_Model(const char *name)
{
	int i;
	model_t *model;
	if (strlen(name) > (MAX_CGVM_MODELNAME - 1))
		return 0;
	for (i = 1;i < MAX_CGVM_MODELS;i++)
	{
		if (!cgvm_modelname[i][0])
			break;
		if (!strcmp(name, cgvm_modelname[i]))
			return i;
	}
	if (i >= MAX_CGVM_MODELS)
		return 0;
	model = Mod_ForName(name, false, false, false);
	if (!model)
		return 0;
	strcpy(cgvm_modelname[i], name);
	cgvm_model[i] = model;
	return i;
}

void CGVM_Stain(const float *origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	if (cl_stainmaps.integer)
		R_Stain((float *)origin, radius, cr1, cg1, cb1, ca1, cr2, cg2, cb2, ca2);
}

