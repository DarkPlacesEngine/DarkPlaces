
#include "cgame_api.h"
#include "cg_math.h"


#ifndef NULL
#define NULL ((void *)0)
#endif

static double gametime, frametime;

struct localentity_s;
typedef struct localentity_s
{
	int active; // true if the entity is alive (not freed)
	float freetime; // time this entity was freed
	float dietime;
	vec3_t velocity;
	vec3_t avelocity;
	vec3_t worldmins;
	vec3_t worldmaxs;
	vec3_t entitymins;
	vec3_t entitymaxs;
	float bouncescale;
	float airfrictionscale;
	float gravityscale;
	void (*framethink)(struct localentity_s *e);
	//int solid;
	//void (*touch)(struct localentity_s *self, struct localentity_s *other);
	//void (*touchnetwork)(struct *localentity_s *self);
	cgdrawentity_t draw;
}
localentity_t;

#define MAX_LOCALENTITIES 1024
static localentity_t *localentity;

static cgphysentity_t *phys_entity;
static int phys_entities;

static float cg_gravity;

static void readvector(vec3_t v)
{
	v[0] = CGVM_MSG_ReadFloat();
	v[1] = CGVM_MSG_ReadFloat();
	v[2] = CGVM_MSG_ReadFloat();
}

static localentity_t *entspawn(void)
{
	int i;
	localentity_t *l;
	for (i = 0;i < MAX_LOCALENTITIES;i++)
	{
		l = localentity + i;
		if (!l->active && l->freetime < gametime)
		{
			memset(l, 0, sizeof(*l));
			l->active = true;
			return l;
		}
	}
	for (i = 0;i < MAX_LOCALENTITIES;i++)
	{
		l = localentity + i;
		if (!l->active)
		{
			memset(l, 0, sizeof(*l));
			l->active = true;
			return l;
		}
	}
	return NULL;
}

static void entremove(localentity_t *e)
{
	memset(e, 0, sizeof(*e));
	e->freetime = (float)gametime + 1.0f;
}

static void phys_setupphysentities(void)
{
	phys_entities = 0;
	/*
	for (i = 0;i < MAX_LOCALENTITIES;i++)
	{
		l = localentities + i;
		if (l->active && l->solid)
		{
		}
	}
	*/
}

static void phys_moveentities(void)
{
	int i;
	localentity_t *l;
	for (i = 0;i < MAX_LOCALENTITIES;i++)
	{
		l = localentity + i;
		if (l->active)
		{
			if (l->framethink)
				l->framethink(l);
			if (l->active && l->draw.model)
				CGVM_Draw_Entity(&l->draw);
		}
	}
}

static void phys_updateentities(void)
{
	phys_setupphysentities();
	phys_moveentities();
}

static void phys_update(localentity_t *e)
{
	vec3_t impactpos, impactnormal, end;
	int impactentnum;
	float t, f, frac, bounce;
	t = (float)frametime;
	if (t == 0)
		return;
	VectorMA(e->draw.angles, t, e->avelocity, e->draw.angles);
	VectorMA(e->draw.origin, t, e->velocity, end);
	frac = CGVM_TracePhysics(e->draw.origin, end, e->worldmins, e->worldmaxs, e->entitymins, e->entitymaxs, phys_entity, phys_entities, impactpos, impactnormal, &impactentnum);
	VectorCopy(impactpos, e->draw.origin);
	if (frac < 1)
	{
		bounce = DotProduct(e->velocity, impactnormal) * -e->bouncescale;
		VectorMA(e->velocity, bounce, impactnormal, e->velocity);
		// FIXME: do some kind of touch code here if physentities get implemented

		if (impactnormal[2] >= 0.7 && DotProduct(e->velocity, e->velocity) < 100*100)
		{
			VectorClear(e->velocity);
			VectorClear(e->avelocity);
		}
	}

	if (e->airfrictionscale)
	{
		if (DotProduct(e->velocity, e->velocity) < 10*10)
		{
			VectorClear(e->velocity);
			VectorClear(e->avelocity);
		}
		else
		{
			f = 1 - (t * e->airfrictionscale);
			if (f > 0)
			{
				VectorScale(e->velocity, f, e->velocity);
				if (DotProduct(e->avelocity, e->avelocity) < 10*10)
				{
					VectorClear(e->avelocity);
				}
				else
				{
					VectorScale(e->avelocity, f, e->avelocity);
				}
			}
			else
			{
				VectorClear(e->velocity);
				VectorClear(e->avelocity);
			}
		}
	}
	if (e->gravityscale)
		e->velocity[2] += cg_gravity * e->gravityscale * t;
}

static void explosiondebris_framethink(localentity_t *self)
{
	if (gametime > self->dietime)
	{
		self->draw.scale -= (float)frametime * 3.0f;
		if (self->draw.scale < 0.05f)
		{
			entremove(self);
			return;
		}
	}
	phys_update(self);
}

static void net_explosion(unsigned char num)
{
	int i;
	float r;
	vec3_t org;
	double time;
	localentity_t *e;
	// need the time to know when the rubble should fade
	time = CGVM_Time();
	// read the network data
	readvector(org);

	for (i = 0;i < 40;i++)
	{
		e = entspawn();
		if (!e)
			return;

		VectorCopy(org, e->draw.origin);
		e->draw.angles[0] = CGVM_RandomRange(0, 360);
		e->draw.angles[1] = CGVM_RandomRange(0, 360);
		e->draw.angles[2] = CGVM_RandomRange(0, 360);
		VectorRandom(e->velocity);
		VectorScale(e->velocity, 300, e->velocity);
		e->velocity[2] -= (float)cg_gravity * 0.1f;
		e->avelocity[0] = CGVM_RandomRange(0, 1440);
		e->avelocity[1] = CGVM_RandomRange(0, 1440);
		e->avelocity[2] = CGVM_RandomRange(0, 1440);
		r = CGVM_RandomRange(0, 3);
		if (r < 1)
			e->draw.model = CGVM_Model("progs/rubble1.mdl");
		else if (r < 2)
			e->draw.model = CGVM_Model("progs/rubble2.mdl");
		else
			e->draw.model = CGVM_Model("progs/rubble3.mdl");
		e->draw.alpha = 1;
		e->draw.scale = 1;
		e->draw.frame1 = 0;
		e->draw.frame2 = 0;
		e->draw.framelerp = 0;
		e->draw.skinnum = 5;
		VectorSet(e->worldmins, 0, 0, -8);
		VectorSet(e->worldmaxs, 0, 0, -8);
		VectorSet(e->entitymins, -8, -8, -8);
		VectorSet(e->entitymaxs, 8, 8, 8);
		e->bouncescale = 1.4f;
		e->gravityscale = 1;
		e->airfrictionscale = 1;
		e->framethink = explosiondebris_framethink;
		e->dietime = (float)time + 5.0f;
	}
}

// called by engine
void CG_Init(void)
{
	localentity = CGVM_Malloc(sizeof(localentity_t) * MAX_LOCALENTITIES);
	phys_entity = CGVM_Malloc(sizeof(cgphysentity_t) * MAX_LOCALENTITIES);
	CGVM_RegisterNetworkCode(1, net_explosion);
	gametime = 0;
}

// called by engine
void CG_Frame(double time)
{
	cg_gravity = -CGVM_GetCvarFloat("sv_gravity");
	frametime = time - gametime;
	gametime = time;
	phys_updateentities();
}

