/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// world.c -- world query functions

#include "quakedef.h"

/*

entities never clip against themselves, or their owner

line of sight checks trace->inopen and trace->inwater, but bullets don't

*/

static void World_Physics_Init(void);
void World_Init(void)
{
	Collision_Init();
	World_Physics_Init();
}

static void World_Physics_Shutdown(void);
void World_Shutdown(void)
{
	World_Physics_Shutdown();
}

static void World_Physics_Start(world_t *world);
void World_Start(world_t *world)
{
	World_Physics_Start(world);
}

static void World_Physics_End(world_t *world);
void World_End(world_t *world)
{
	World_Physics_End(world);
}

//============================================================================

/// World_ClearLink is used for new headnodes
void World_ClearLink (link_t *l)
{
	l->entitynumber = 0;
	l->prev = l->next = l;
}

void World_RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void World_InsertLinkBefore (link_t *l, link_t *before, int entitynumber)
{
	l->entitynumber = entitynumber;
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

void World_PrintAreaStats(world_t *world, const char *worldname)
{
	Con_Printf("%s areagrid check stats: %d calls %d nodes (%f per call) %d entities (%f per call)\n", worldname, world->areagrid_stats_calls, world->areagrid_stats_nodechecks, (double) world->areagrid_stats_nodechecks / (double) world->areagrid_stats_calls, world->areagrid_stats_entitychecks, (double) world->areagrid_stats_entitychecks / (double) world->areagrid_stats_calls);
	world->areagrid_stats_calls = 0;
	world->areagrid_stats_nodechecks = 0;
	world->areagrid_stats_entitychecks = 0;
}

/*
===============
World_SetSize

===============
*/
void World_SetSize(world_t *world, const char *filename, const vec3_t mins, const vec3_t maxs)
{
	int i;

	strlcpy(world->filename, filename, sizeof(world->filename));
	VectorCopy(mins, world->mins);
	VectorCopy(maxs, world->maxs);

	// the areagrid_marknumber is not allowed to be 0
	if (world->areagrid_marknumber < 1)
		world->areagrid_marknumber = 1;
	// choose either the world box size, or a larger box to ensure the grid isn't too fine
	world->areagrid_size[0] = max(world->areagrid_maxs[0] - world->areagrid_mins[0], AREA_GRID * sv_areagrid_mingridsize.value);
	world->areagrid_size[1] = max(world->areagrid_maxs[1] - world->areagrid_mins[1], AREA_GRID * sv_areagrid_mingridsize.value);
	world->areagrid_size[2] = max(world->areagrid_maxs[2] - world->areagrid_mins[2], AREA_GRID * sv_areagrid_mingridsize.value);
	// figure out the corners of such a box, centered at the center of the world box
	world->areagrid_mins[0] = (world->areagrid_mins[0] + world->areagrid_maxs[0] - world->areagrid_size[0]) * 0.5f;
	world->areagrid_mins[1] = (world->areagrid_mins[1] + world->areagrid_maxs[1] - world->areagrid_size[1]) * 0.5f;
	world->areagrid_mins[2] = (world->areagrid_mins[2] + world->areagrid_maxs[2] - world->areagrid_size[2]) * 0.5f;
	world->areagrid_maxs[0] = (world->areagrid_mins[0] + world->areagrid_maxs[0] + world->areagrid_size[0]) * 0.5f;
	world->areagrid_maxs[1] = (world->areagrid_mins[1] + world->areagrid_maxs[1] + world->areagrid_size[1]) * 0.5f;
	world->areagrid_maxs[2] = (world->areagrid_mins[2] + world->areagrid_maxs[2] + world->areagrid_size[2]) * 0.5f;
	// now calculate the actual useful info from that
	VectorNegate(world->areagrid_mins, world->areagrid_bias);
	world->areagrid_scale[0] = AREA_GRID / world->areagrid_size[0];
	world->areagrid_scale[1] = AREA_GRID / world->areagrid_size[1];
	world->areagrid_scale[2] = AREA_GRID / world->areagrid_size[2];
	World_ClearLink(&world->areagrid_outside);
	for (i = 0;i < AREA_GRIDNODES;i++)
		World_ClearLink(&world->areagrid[i]);
	if (developer.integer >= 10)
		Con_Printf("areagrid settings: divisions %ix%ix1 : box %f %f %f : %f %f %f size %f %f %f grid %f %f %f (mingrid %f)\n", AREA_GRID, AREA_GRID, world->areagrid_mins[0], world->areagrid_mins[1], world->areagrid_mins[2], world->areagrid_maxs[0], world->areagrid_maxs[1], world->areagrid_maxs[2], world->areagrid_size[0], world->areagrid_size[1], world->areagrid_size[2], 1.0f / world->areagrid_scale[0], 1.0f / world->areagrid_scale[1], 1.0f / world->areagrid_scale[2], sv_areagrid_mingridsize.value);
}

/*
===============
World_UnlinkAll

===============
*/
void World_UnlinkAll(world_t *world)
{
	int i;
	link_t *grid;
	// unlink all entities one by one
	grid = &world->areagrid_outside;
	while (grid->next != grid)
		World_UnlinkEdict(PRVM_EDICT_NUM(grid->next->entitynumber));
	for (i = 0, grid = world->areagrid;i < AREA_GRIDNODES;i++, grid++)
		while (grid->next != grid)
			World_UnlinkEdict(PRVM_EDICT_NUM(grid->next->entitynumber));
}

/*
===============

===============
*/
void World_UnlinkEdict(prvm_edict_t *ent)
{
	int i;
	for (i = 0;i < ENTITYGRIDAREAS;i++)
	{
		if (ent->priv.server->areagrid[i].prev)
		{
			World_RemoveLink (&ent->priv.server->areagrid[i]);
			ent->priv.server->areagrid[i].prev = ent->priv.server->areagrid[i].next = NULL;
		}
	}
}

int World_EntitiesInBox(world_t *world, const vec3_t mins, const vec3_t maxs, int maxlist, prvm_edict_t **list)
{
	int numlist;
	link_t *grid;
	link_t *l;
	prvm_edict_t *ent;
	int igrid[3], igridmins[3], igridmaxs[3];

	// FIXME: if areagrid_marknumber wraps, all entities need their
	// ent->priv.server->areagridmarknumber reset
	world->areagrid_stats_calls++;
	world->areagrid_marknumber++;
	igridmins[0] = (int) floor((mins[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]);
	igridmins[1] = (int) floor((mins[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]);
	//igridmins[2] = (int) ((mins[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]);
	igridmaxs[0] = (int) floor((maxs[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) floor((maxs[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) ((maxs[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]) + 1;
	igridmins[0] = max(0, igridmins[0]);
	igridmins[1] = max(0, igridmins[1]);
	//igridmins[2] = max(0, igridmins[2]);
	igridmaxs[0] = min(AREA_GRID, igridmaxs[0]);
	igridmaxs[1] = min(AREA_GRID, igridmaxs[1]);
	//igridmaxs[2] = min(AREA_GRID, igridmaxs[2]);

	numlist = 0;
	// add entities not linked into areagrid because they are too big or
	// outside the grid bounds
	if (world->areagrid_outside.next != &world->areagrid_outside)
	{
		grid = &world->areagrid_outside;
		for (l = grid->next;l != grid;l = l->next)
		{
			ent = PRVM_EDICT_NUM(l->entitynumber);
			if (ent->priv.server->areagridmarknumber != world->areagrid_marknumber)
			{
				ent->priv.server->areagridmarknumber = world->areagrid_marknumber;
				if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs))
				{
					if (numlist < maxlist)
						list[numlist] = ent;
					numlist++;
				}
				world->areagrid_stats_entitychecks++;
			}
		}
	}
	// add grid linked entities
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = world->areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++)
		{
			if (grid->next != grid)
			{
				for (l = grid->next;l != grid;l = l->next)
				{
					ent = PRVM_EDICT_NUM(l->entitynumber);
					if (ent->priv.server->areagridmarknumber != world->areagrid_marknumber)
					{
						ent->priv.server->areagridmarknumber = world->areagrid_marknumber;
						if (!ent->priv.server->free && BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs))
						{
							if (numlist < maxlist)
								list[numlist] = ent;
							numlist++;
						}
						//Con_Printf("%d %f %f %f %f %f %f : %d : %f %f %f %f %f %f\n", BoxesOverlap(mins, maxs, ent->priv.server->areamins, ent->priv.server->areamaxs), ent->priv.server->areamins[0], ent->priv.server->areamins[1], ent->priv.server->areamins[2], ent->priv.server->areamaxs[0], ent->priv.server->areamaxs[1], ent->priv.server->areamaxs[2], PRVM_NUM_FOR_EDICT(ent), mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
					}
					world->areagrid_stats_entitychecks++;
				}
			}
		}
	}
	return numlist;
}

void World_LinkEdict_AreaGrid(world_t *world, prvm_edict_t *ent)
{
	link_t *grid;
	int igrid[3], igridmins[3], igridmaxs[3], gridnum, entitynumber = PRVM_NUM_FOR_EDICT(ent);

	if (entitynumber <= 0 || entitynumber >= prog->max_edicts || PRVM_EDICT_NUM(entitynumber) != ent)
	{
		Con_Printf ("World_LinkEdict_AreaGrid: invalid edict %p (edicts is %p, edict compared to prog->edicts is %i)\n", (void *)ent, (void *)prog->edicts, entitynumber);
		return;
	}

	igridmins[0] = (int) floor((ent->priv.server->areamins[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]);
	igridmins[1] = (int) floor((ent->priv.server->areamins[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]);
	//igridmins[2] = (int) floor((ent->priv.server->areamins[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]);
	igridmaxs[0] = (int) floor((ent->priv.server->areamaxs[0] + world->areagrid_bias[0]) * world->areagrid_scale[0]) + 1;
	igridmaxs[1] = (int) floor((ent->priv.server->areamaxs[1] + world->areagrid_bias[1]) * world->areagrid_scale[1]) + 1;
	//igridmaxs[2] = (int) floor((ent->priv.server->areamaxs[2] + world->areagrid_bias[2]) * world->areagrid_scale[2]) + 1;
	if (igridmins[0] < 0 || igridmaxs[0] > AREA_GRID || igridmins[1] < 0 || igridmaxs[1] > AREA_GRID || ((igridmaxs[0] - igridmins[0]) * (igridmaxs[1] - igridmins[1])) > ENTITYGRIDAREAS)
	{
		// wow, something outside the grid, store it as such
		World_InsertLinkBefore (&ent->priv.server->areagrid[0], &world->areagrid_outside, entitynumber);
		return;
	}

	gridnum = 0;
	for (igrid[1] = igridmins[1];igrid[1] < igridmaxs[1];igrid[1]++)
	{
		grid = world->areagrid + igrid[1] * AREA_GRID + igridmins[0];
		for (igrid[0] = igridmins[0];igrid[0] < igridmaxs[0];igrid[0]++, grid++, gridnum++)
			World_InsertLinkBefore (&ent->priv.server->areagrid[gridnum], grid, entitynumber);
	}
}

/*
===============
World_LinkEdict

===============
*/
void World_LinkEdict(world_t *world, prvm_edict_t *ent, const vec3_t mins, const vec3_t maxs)
{
	// unlink from old position first
	if (ent->priv.server->areagrid[0].prev)
		World_UnlinkEdict(ent);

	// don't add the world
	if (ent == prog->edicts)
		return;

	// don't add free entities
	if (ent->priv.server->free)
		return;

	VectorCopy(mins, ent->priv.server->areamins);
	VectorCopy(maxs, ent->priv.server->areamaxs);
	World_LinkEdict_AreaGrid(world, ent);
}




//============================================================================
// physics engine support
//============================================================================

cvar_t physics_ode_quadtree_depth = {0, "physics_ode_quadtree_depth","5", "desired subdivision level of quadtree culling space"};
cvar_t physics_ode_contactsurfacelayer = {0, "physics_ode_contactsurfacelayer","0", "allows objects to overlap this many units to reduce jitter"};
cvar_t physics_ode_worldquickstep = {0, "physics_ode_worldquickstep","1", "use dWorldQuickStep rather than dWorldStepFast1 or dWorldStep"};
cvar_t physics_ode_worldquickstep_iterations = {0, "physics_ode_worldquickstep_iterations","20", "parameter to dWorldQuickStep"};
cvar_t physics_ode_worldstepfast = {0, "physics_ode_worldstepfast","0", "use dWorldStepFast1 rather than dWorldStep"};
cvar_t physics_ode_worldstepfast_iterations = {0, "physics_ode_worldstepfast_iterations","20", "parameter to dWorldStepFast1"};
cvar_t physics_ode_contact_mu = {0, "physics_ode_contact_mu", "1", "contact solver mu parameter - friction pyramid approximation 1 (see ODE User Guide)"};
cvar_t physics_ode_contact_erp = {0, "physics_ode_contact_erp", "0.96", "contact solver erp parameter - Error Restitution Percent (see ODE User Guide)"};
cvar_t physics_ode_contact_cfm = {0, "physics_ode_contact_cfm", "0", "contact solver cfm parameter - Constraint Force Mixing (see ODE User Guide)"};
cvar_t physics_ode_iterationsperframe = {0, "physics_ode_iterationsperframe", "4", "divisor for time step, runs multiple physics steps per frame"};
cvar_t physics_ode_movelimit = {0, "physics_ode_movelimit", "0.5", "clamp velocity if a single move would exceed this percentage of object thickness, to prevent flying through walls"};
cvar_t physics_ode_spinlimit = {0, "physics_ode_spinlimit", "10000", "reset spin velocity if it gets too large"};

// LordHavoc: this large chunk of definitions comes from the ODE library
// include files.

//#ifndef ODE_STATIC
//#define ODE_DYNAMIC 1
//#endif

#if defined(ODE_STATIC) || defined(ODE_DYNAMIC)
#define USEODE 1
#endif

#ifdef USEODE
#ifdef ODE_STATIC
#include "ode/ode.h"
#else
#ifdef WINAPI
#define ODE_API WINAPI
#else
#define ODE_API
#endif

// note: dynamic builds of ODE tend to be double precision, this is not used
// for static builds
typedef double dReal;

typedef dReal dVector3[4];
typedef dReal dVector4[4];
typedef dReal dMatrix3[4*3];
typedef dReal dMatrix4[4*4];
typedef dReal dMatrix6[8*6];
typedef dReal dQuaternion[4];

struct dxWorld;		/* dynamics world */
struct dxSpace;		/* collision space */
struct dxBody;		/* rigid body (dynamics object) */
struct dxGeom;		/* geometry (collision object) */
struct dxJoint;
struct dxJointNode;
struct dxJointGroup;
struct dxTriMeshData;

typedef struct dxWorld *dWorldID;
typedef struct dxSpace *dSpaceID;
typedef struct dxBody *dBodyID;
typedef struct dxGeom *dGeomID;
typedef struct dxJoint *dJointID;
typedef struct dxJointGroup *dJointGroupID;
typedef struct dxTriMeshData *dTriMeshDataID;

typedef struct dJointFeedback
{
	dVector3 f1;		/* force applied to body 1 */
	dVector3 t1;		/* torque applied to body 1 */
	dVector3 f2;		/* force applied to body 2 */
	dVector3 t2;		/* torque applied to body 2 */
}
dJointFeedback;

typedef enum dJointType
{
	dJointTypeNone = 0,
	dJointTypeBall,
	dJointTypeHinge,
	dJointTypeSlider,
	dJointTypeContact,
	dJointTypeUniversal,
	dJointTypeHinge2,
	dJointTypeFixed,
	dJointTypeNull,
	dJointTypeAMotor,
	dJointTypeLMotor,
	dJointTypePlane2D,
	dJointTypePR,
	dJointTypePU,
	dJointTypePiston
}
dJointType;

typedef struct dMass
{
	dReal mass;
	dVector3 c;
	dMatrix3 I;
}
dMass;

enum
{
	dContactMu2			= 0x001,
	dContactFDir1		= 0x002,
	dContactBounce		= 0x004,
	dContactSoftERP		= 0x008,
	dContactSoftCFM		= 0x010,
	dContactMotion1		= 0x020,
	dContactMotion2		= 0x040,
	dContactMotionN		= 0x080,
	dContactSlip1		= 0x100,
	dContactSlip2		= 0x200,
	
	dContactApprox0		= 0x0000,
	dContactApprox1_1	= 0x1000,
	dContactApprox1_2	= 0x2000,
	dContactApprox1		= 0x3000
};

typedef struct dSurfaceParameters
{
	/* must always be defined */
	int mode;
	dReal mu;

	/* only defined if the corresponding flag is set in mode */
	dReal mu2;
	dReal bounce;
	dReal bounce_vel;
	dReal soft_erp;
	dReal soft_cfm;
	dReal motion1,motion2,motionN;
	dReal slip1,slip2;
} dSurfaceParameters;

typedef struct dContactGeom
{
	dVector3 pos;          ///< contact position
	dVector3 normal;       ///< normal vector
	dReal depth;           ///< penetration depth
	dGeomID g1,g2;         ///< the colliding geoms
	int side1,side2;       ///< (to be documented)
}
dContactGeom;

typedef struct dContact
{
	dSurfaceParameters surface;
	dContactGeom geom;
	dVector3 fdir1;
}
dContact;

typedef void dNearCallback (void *data, dGeomID o1, dGeomID o2);

// SAP
// Order XZY or ZXY usually works best, if your Y is up.
#define dSAP_AXES_XYZ  ((0)|(1<<2)|(2<<4))
#define dSAP_AXES_XZY  ((0)|(2<<2)|(1<<4))
#define dSAP_AXES_YXZ  ((1)|(0<<2)|(2<<4))
#define dSAP_AXES_YZX  ((1)|(2<<2)|(0<<4))
#define dSAP_AXES_ZXY  ((2)|(0<<2)|(1<<4))
#define dSAP_AXES_ZYX  ((2)|(1<<2)|(0<<4))

ODE_API const char*     (*dGetConfiguration)(void);
ODE_API int             (*dCheckConfiguration)( const char* token );
ODE_API int             (*dInitODE2)(unsigned int uiInitFlags);
ODE_API int             (*dAllocateODEDataForThread)(unsigned int uiAllocateFlags);
ODE_API void            (*dCleanupODEAllDataForThread)(void);
ODE_API void            (*dCloseODE)(void);

//ODE_API int             (*dMassCheck)(const dMass *m);
//ODE_API void            (*dMassSetZero)(dMass *);
//ODE_API void            (*dMassSetParameters)(dMass *, dReal themass, dReal cgx, dReal cgy, dReal cgz, dReal I11, dReal I22, dReal I33, dReal I12, dReal I13, dReal I23);
//ODE_API void            (*dMassSetSphere)(dMass *, dReal density, dReal radius);
ODE_API void            (*dMassSetSphereTotal)(dMass *, dReal total_mass, dReal radius);
//ODE_API void            (*dMassSetCapsule)(dMass *, dReal density, int direction, dReal radius, dReal length);
ODE_API void            (*dMassSetCapsuleTotal)(dMass *, dReal total_mass, int direction, dReal radius, dReal length);
//ODE_API void            (*dMassSetCylinder)(dMass *, dReal density, int direction, dReal radius, dReal length);
//ODE_API void            (*dMassSetCylinderTotal)(dMass *, dReal total_mass, int direction, dReal radius, dReal length);
//ODE_API void            (*dMassSetBox)(dMass *, dReal density, dReal lx, dReal ly, dReal lz);
ODE_API void            (*dMassSetBoxTotal)(dMass *, dReal total_mass, dReal lx, dReal ly, dReal lz);
//ODE_API void            (*dMassSetTrimesh)(dMass *, dReal density, dGeomID g);
//ODE_API void            (*dMassSetTrimeshTotal)(dMass *m, dReal total_mass, dGeomID g);
//ODE_API void            (*dMassAdjust)(dMass *, dReal newmass);
//ODE_API void            (*dMassTranslate)(dMass *, dReal x, dReal y, dReal z);
//ODE_API void            (*dMassRotate)(dMass *, const dMatrix3 R);
//ODE_API void            (*dMassAdd)(dMass *a, const dMass *b);
//
ODE_API dWorldID        (*dWorldCreate)(void);
ODE_API void            (*dWorldDestroy)(dWorldID world);
ODE_API void            (*dWorldSetGravity)(dWorldID, dReal x, dReal y, dReal z);
//ODE_API void            (*dWorldGetGravity)(dWorldID, dVector3 gravity);
//ODE_API void            (*dWorldSetERP)(dWorldID, dReal erp);
//ODE_API dReal           (*dWorldGetERP)(dWorldID);
//ODE_API void            (*dWorldSetCFM)(dWorldID, dReal cfm);
//ODE_API dReal           (*dWorldGetCFM)(dWorldID);
ODE_API void            (*dWorldStep)(dWorldID, dReal stepsize);
//ODE_API void            (*dWorldImpulseToForce)(dWorldID, dReal stepsize, dReal ix, dReal iy, dReal iz, dVector3 force);
ODE_API void            (*dWorldQuickStep)(dWorldID w, dReal stepsize);
ODE_API void            (*dWorldSetQuickStepNumIterations)(dWorldID, int num);
//ODE_API int             (*dWorldGetQuickStepNumIterations)(dWorldID);
//ODE_API void            (*dWorldSetQuickStepW)(dWorldID, dReal over_relaxation);
//ODE_API dReal           (*dWorldGetQuickStepW)(dWorldID);
//ODE_API void            (*dWorldSetContactMaxCorrectingVel)(dWorldID, dReal vel);
//ODE_API dReal           (*dWorldGetContactMaxCorrectingVel)(dWorldID);
ODE_API void            (*dWorldSetContactSurfaceLayer)(dWorldID, dReal depth);
//ODE_API dReal           (*dWorldGetContactSurfaceLayer)(dWorldID);
ODE_API void            (*dWorldStepFast1)(dWorldID, dReal stepsize, int maxiterations);
//ODE_API void            (*dWorldSetAutoEnableDepthSF1)(dWorldID, int autoEnableDepth);
//ODE_API int             (*dWorldGetAutoEnableDepthSF1)(dWorldID);
//ODE_API dReal           (*dWorldGetAutoDisableLinearThreshold)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableLinearThreshold)(dWorldID, dReal linear_threshold);
//ODE_API dReal           (*dWorldGetAutoDisableAngularThreshold)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableAngularThreshold)(dWorldID, dReal angular_threshold);
//ODE_API dReal           (*dWorldGetAutoDisableLinearAverageThreshold)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableLinearAverageThreshold)(dWorldID, dReal linear_average_threshold);
//ODE_API dReal           (*dWorldGetAutoDisableAngularAverageThreshold)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableAngularAverageThreshold)(dWorldID, dReal angular_average_threshold);
//ODE_API int             (*dWorldGetAutoDisableAverageSamplesCount)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableAverageSamplesCount)(dWorldID, unsigned int average_samples_count );
//ODE_API int             (*dWorldGetAutoDisableSteps)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableSteps)(dWorldID, int steps);
//ODE_API dReal           (*dWorldGetAutoDisableTime)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableTime)(dWorldID, dReal time);
//ODE_API int             (*dWorldGetAutoDisableFlag)(dWorldID);
//ODE_API void            (*dWorldSetAutoDisableFlag)(dWorldID, int do_auto_disable);
//ODE_API dReal           (*dWorldGetLinearDampingThreshold)(dWorldID w);
//ODE_API void            (*dWorldSetLinearDampingThreshold)(dWorldID w, dReal threshold);
//ODE_API dReal           (*dWorldGetAngularDampingThreshold)(dWorldID w);
//ODE_API void            (*dWorldSetAngularDampingThreshold)(dWorldID w, dReal threshold);
//ODE_API dReal           (*dWorldGetLinearDamping)(dWorldID w);
//ODE_API void            (*dWorldSetLinearDamping)(dWorldID w, dReal scale);
//ODE_API dReal           (*dWorldGetAngularDamping)(dWorldID w);
//ODE_API void            (*dWorldSetAngularDamping)(dWorldID w, dReal scale);
//ODE_API void            (*dWorldSetDamping)(dWorldID w, dReal linear_scale, dReal angular_scale);
//ODE_API dReal           (*dWorldGetMaxAngularSpeed)(dWorldID w);
//ODE_API void            (*dWorldSetMaxAngularSpeed)(dWorldID w, dReal max_speed);
//ODE_API dReal           (*dBodyGetAutoDisableLinearThreshold)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableLinearThreshold)(dBodyID, dReal linear_average_threshold);
//ODE_API dReal           (*dBodyGetAutoDisableAngularThreshold)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableAngularThreshold)(dBodyID, dReal angular_average_threshold);
//ODE_API int             (*dBodyGetAutoDisableAverageSamplesCount)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableAverageSamplesCount)(dBodyID, unsigned int average_samples_count);
//ODE_API int             (*dBodyGetAutoDisableSteps)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableSteps)(dBodyID, int steps);
//ODE_API dReal           (*dBodyGetAutoDisableTime)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableTime)(dBodyID, dReal time);
//ODE_API int             (*dBodyGetAutoDisableFlag)(dBodyID);
//ODE_API void            (*dBodySetAutoDisableFlag)(dBodyID, int do_auto_disable);
//ODE_API void            (*dBodySetAutoDisableDefaults)(dBodyID);
//ODE_API dWorldID        (*dBodyGetWorld)(dBodyID);
ODE_API dBodyID         (*dBodyCreate)(dWorldID);
ODE_API void            (*dBodyDestroy)(dBodyID);
ODE_API void            (*dBodySetData)(dBodyID, void *data);
//ODE_API void *          (*dBodyGetData)(dBodyID);
ODE_API void            (*dBodySetPosition)(dBodyID, dReal x, dReal y, dReal z);
ODE_API void            (*dBodySetRotation)(dBodyID, const dMatrix3 R);
//ODE_API void            (*dBodySetQuaternion)(dBodyID, const dQuaternion q);
ODE_API void            (*dBodySetLinearVel)(dBodyID, dReal x, dReal y, dReal z);
ODE_API void            (*dBodySetAngularVel)(dBodyID, dReal x, dReal y, dReal z);
ODE_API const dReal *   (*dBodyGetPosition)(dBodyID);
//ODE_API void            (*dBodyCopyPosition)(dBodyID body, dVector3 pos);
ODE_API const dReal *   (*dBodyGetRotation)(dBodyID);
//ODE_API void            (*dBodyCopyRotation)(dBodyID, dMatrix3 R);
//ODE_API const dReal *   (*dBodyGetQuaternion)(dBodyID);
//ODE_API void            (*dBodyCopyQuaternion)(dBodyID body, dQuaternion quat);
ODE_API const dReal *   (*dBodyGetLinearVel)(dBodyID);
ODE_API const dReal *   (*dBodyGetAngularVel)(dBodyID);
ODE_API void            (*dBodySetMass)(dBodyID, const dMass *mass);
//ODE_API void            (*dBodyGetMass)(dBodyID, dMass *mass);
//ODE_API void            (*dBodyAddForce)(dBodyID, dReal fx, dReal fy, dReal fz);
//ODE_API void            (*dBodyAddTorque)(dBodyID, dReal fx, dReal fy, dReal fz);
//ODE_API void            (*dBodyAddRelForce)(dBodyID, dReal fx, dReal fy, dReal fz);
//ODE_API void            (*dBodyAddRelTorque)(dBodyID, dReal fx, dReal fy, dReal fz);
//ODE_API void            (*dBodyAddForceAtPos)(dBodyID, dReal fx, dReal fy, dReal fz, dReal px, dReal py, dReal pz);
//ODE_API void            (*dBodyAddForceAtRelPos)(dBodyID, dReal fx, dReal fy, dReal fz, dReal px, dReal py, dReal pz);
//ODE_API void            (*dBodyAddRelForceAtPos)(dBodyID, dReal fx, dReal fy, dReal fz, dReal px, dReal py, dReal pz);
//ODE_API void            (*dBodyAddRelForceAtRelPos)(dBodyID, dReal fx, dReal fy, dReal fz, dReal px, dReal py, dReal pz);
//ODE_API const dReal *   (*dBodyGetForce)(dBodyID);
//ODE_API const dReal *   (*dBodyGetTorque)(dBodyID);
//ODE_API void            (*dBodySetForce)(dBodyID b, dReal x, dReal y, dReal z);
//ODE_API void            (*dBodySetTorque)(dBodyID b, dReal x, dReal y, dReal z);
//ODE_API void            (*dBodyGetRelPointPos)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodyGetRelPointVel)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodyGetPointVel)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodyGetPosRelPoint)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodyVectorToWorld)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodyVectorFromWorld)(dBodyID, dReal px, dReal py, dReal pz, dVector3 result);
//ODE_API void            (*dBodySetFiniteRotationMode)(dBodyID, int mode);
//ODE_API void            (*dBodySetFiniteRotationAxis)(dBodyID, dReal x, dReal y, dReal z);
//ODE_API int             (*dBodyGetFiniteRotationMode)(dBodyID);
//ODE_API void            (*dBodyGetFiniteRotationAxis)(dBodyID, dVector3 result);
//ODE_API int             (*dBodyGetNumJoints)(dBodyID b);
//ODE_API dJointID        (*dBodyGetJoint)(dBodyID, int index);
//ODE_API void            (*dBodySetDynamic)(dBodyID);
//ODE_API void            (*dBodySetKinematic)(dBodyID);
//ODE_API int             (*dBodyIsKinematic)(dBodyID);
//ODE_API void            (*dBodyEnable)(dBodyID);
//ODE_API void            (*dBodyDisable)(dBodyID);
//ODE_API int             (*dBodyIsEnabled)(dBodyID);
//ODE_API void            (*dBodySetGravityMode)(dBodyID b, int mode);
//ODE_API int             (*dBodyGetGravityMode)(dBodyID b);
//ODE_API void            (*dBodySetMovedCallback)(dBodyID b, void(*callback)(dBodyID));
//ODE_API dGeomID         (*dBodyGetFirstGeom)(dBodyID b);
//ODE_API dGeomID         (*dBodyGetNextGeom)(dGeomID g);
//ODE_API void            (*dBodySetDampingDefaults)(dBodyID b);
//ODE_API dReal           (*dBodyGetLinearDamping)(dBodyID b);
//ODE_API void            (*dBodySetLinearDamping)(dBodyID b, dReal scale);
//ODE_API dReal           (*dBodyGetAngularDamping)(dBodyID b);
//ODE_API void            (*dBodySetAngularDamping)(dBodyID b, dReal scale);
//ODE_API void            (*dBodySetDamping)(dBodyID b, dReal linear_scale, dReal angular_scale);
//ODE_API dReal           (*dBodyGetLinearDampingThreshold)(dBodyID b);
//ODE_API void            (*dBodySetLinearDampingThreshold)(dBodyID b, dReal threshold);
//ODE_API dReal           (*dBodyGetAngularDampingThreshold)(dBodyID b);
//ODE_API void            (*dBodySetAngularDampingThreshold)(dBodyID b, dReal threshold);
//ODE_API dReal           (*dBodyGetMaxAngularSpeed)(dBodyID b);
//ODE_API void            (*dBodySetMaxAngularSpeed)(dBodyID b, dReal max_speed);
//ODE_API int             (*dBodyGetGyroscopicMode)(dBodyID b);
//ODE_API void            (*dBodySetGyroscopicMode)(dBodyID b, int enabled);
//ODE_API dJointID        (*dJointCreateBall)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateHinge)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateSlider)(dWorldID, dJointGroupID);
ODE_API dJointID        (*dJointCreateContact)(dWorldID, dJointGroupID, const dContact *);
//ODE_API dJointID        (*dJointCreateHinge2)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateUniversal)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreatePR)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreatePU)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreatePiston)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateFixed)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateNull)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateAMotor)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreateLMotor)(dWorldID, dJointGroupID);
//ODE_API dJointID        (*dJointCreatePlane2D)(dWorldID, dJointGroupID);
//ODE_API void            (*dJointDestroy)(dJointID);
ODE_API dJointGroupID   (*dJointGroupCreate)(int max_size);
ODE_API void            (*dJointGroupDestroy)(dJointGroupID);
ODE_API void            (*dJointGroupEmpty)(dJointGroupID);
//ODE_API int             (*dJointGetNumBodies)(dJointID);
ODE_API void            (*dJointAttach)(dJointID, dBodyID body1, dBodyID body2);
//ODE_API void            (*dJointEnable)(dJointID);
//ODE_API void            (*dJointDisable)(dJointID);
//ODE_API int             (*dJointIsEnabled)(dJointID);
//ODE_API void            (*dJointSetData)(dJointID, void *data);
//ODE_API void *          (*dJointGetData)(dJointID);
//ODE_API dJointType      (*dJointGetType)(dJointID);
//ODE_API dBodyID         (*dJointGetBody)(dJointID, int index);
//ODE_API void            (*dJointSetFeedback)(dJointID, dJointFeedback *);
//ODE_API dJointFeedback *(*dJointGetFeedback)(dJointID);
//ODE_API void            (*dJointSetBallAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetBallAnchor2)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetBallParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetHingeAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetHingeAnchorDelta)(dJointID, dReal x, dReal y, dReal z, dReal ax, dReal ay, dReal az);
//ODE_API void            (*dJointSetHingeAxis)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetHingeAxisOffset)(dJointID j, dReal x, dReal y, dReal z, dReal angle);
//ODE_API void            (*dJointSetHingeParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddHingeTorque)(dJointID joint, dReal torque);
//ODE_API void            (*dJointSetSliderAxis)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetSliderAxisDelta)(dJointID, dReal x, dReal y, dReal z, dReal ax, dReal ay, dReal az);
//ODE_API void            (*dJointSetSliderParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddSliderForce)(dJointID joint, dReal force);
//ODE_API void            (*dJointSetHinge2Anchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetHinge2Axis1)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetHinge2Axis2)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetHinge2Param)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddHinge2Torques)(dJointID joint, dReal torque1, dReal torque2);
//ODE_API void            (*dJointSetUniversalAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetUniversalAxis1)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetUniversalAxis1Offset)(dJointID, dReal x, dReal y, dReal z, dReal offset1, dReal offset2);
//ODE_API void            (*dJointSetUniversalAxis2)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetUniversalAxis2Offset)(dJointID, dReal x, dReal y, dReal z, dReal offset1, dReal offset2);
//ODE_API void            (*dJointSetUniversalParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddUniversalTorques)(dJointID joint, dReal torque1, dReal torque2);
//ODE_API void            (*dJointSetPRAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPRAxis1)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPRAxis2)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPRParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddPRTorque)(dJointID j, dReal torque);
//ODE_API void            (*dJointSetPUAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPUAnchorOffset)(dJointID, dReal x, dReal y, dReal z, dReal dx, dReal dy, dReal dz);
//ODE_API void            (*dJointSetPUAxis1)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPUAxis2)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPUAxis3)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPUAxisP)(dJointID id, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPUParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddPUTorque)(dJointID j, dReal torque);
//ODE_API void            (*dJointSetPistonAnchor)(dJointID, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetPistonAnchorOffset)(dJointID j, dReal x, dReal y, dReal z, dReal dx, dReal dy, dReal dz);
//ODE_API void            (*dJointSetPistonParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointAddPistonForce)(dJointID joint, dReal force);
//ODE_API void            (*dJointSetFixed)(dJointID);
//ODE_API void            (*dJointSetFixedParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetAMotorNumAxes)(dJointID, int num);
//ODE_API void            (*dJointSetAMotorAxis)(dJointID, int anum, int rel, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetAMotorAngle)(dJointID, int anum, dReal angle);
//ODE_API void            (*dJointSetAMotorParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetAMotorMode)(dJointID, int mode);
//ODE_API void            (*dJointAddAMotorTorques)(dJointID, dReal torque1, dReal torque2, dReal torque3);
//ODE_API void            (*dJointSetLMotorNumAxes)(dJointID, int num);
//ODE_API void            (*dJointSetLMotorAxis)(dJointID, int anum, int rel, dReal x, dReal y, dReal z);
//ODE_API void            (*dJointSetLMotorParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetPlane2DXParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetPlane2DYParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointSetPlane2DAngleParam)(dJointID, int parameter, dReal value);
//ODE_API void            (*dJointGetBallAnchor)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetBallAnchor2)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetBallParam)(dJointID, int parameter);
//ODE_API void            (*dJointGetHingeAnchor)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetHingeAnchor2)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetHingeAxis)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetHingeParam)(dJointID, int parameter);
//ODE_API dReal           (*dJointGetHingeAngle)(dJointID);
//ODE_API dReal           (*dJointGetHingeAngleRate)(dJointID);
//ODE_API dReal           (*dJointGetSliderPosition)(dJointID);
//ODE_API dReal           (*dJointGetSliderPositionRate)(dJointID);
//ODE_API void            (*dJointGetSliderAxis)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetSliderParam)(dJointID, int parameter);
//ODE_API void            (*dJointGetHinge2Anchor)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetHinge2Anchor2)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetHinge2Axis1)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetHinge2Axis2)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetHinge2Param)(dJointID, int parameter);
//ODE_API dReal           (*dJointGetHinge2Angle1)(dJointID);
//ODE_API dReal           (*dJointGetHinge2Angle1Rate)(dJointID);
//ODE_API dReal           (*dJointGetHinge2Angle2Rate)(dJointID);
//ODE_API void            (*dJointGetUniversalAnchor)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetUniversalAnchor2)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetUniversalAxis1)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetUniversalAxis2)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetUniversalParam)(dJointID, int parameter);
//ODE_API void            (*dJointGetUniversalAngles)(dJointID, dReal *angle1, dReal *angle2);
//ODE_API dReal           (*dJointGetUniversalAngle1)(dJointID);
//ODE_API dReal           (*dJointGetUniversalAngle2)(dJointID);
//ODE_API dReal           (*dJointGetUniversalAngle1Rate)(dJointID);
//ODE_API dReal           (*dJointGetUniversalAngle2Rate)(dJointID);
//ODE_API void            (*dJointGetPRAnchor)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetPRPosition)(dJointID);
//ODE_API dReal           (*dJointGetPRPositionRate)(dJointID);
//ODE_API dReal           (*dJointGetPRAngle)(dJointID);
//ODE_API dReal           (*dJointGetPRAngleRate)(dJointID);
//ODE_API void            (*dJointGetPRAxis1)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPRAxis2)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetPRParam)(dJointID, int parameter);
//ODE_API void            (*dJointGetPUAnchor)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetPUPosition)(dJointID);
//ODE_API dReal           (*dJointGetPUPositionRate)(dJointID);
//ODE_API void            (*dJointGetPUAxis1)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPUAxis2)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPUAxis3)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPUAxisP)(dJointID id, dVector3 result);
//ODE_API void            (*dJointGetPUAngles)(dJointID, dReal *angle1, dReal *angle2);
//ODE_API dReal           (*dJointGetPUAngle1)(dJointID);
//ODE_API dReal           (*dJointGetPUAngle1Rate)(dJointID);
//ODE_API dReal           (*dJointGetPUAngle2)(dJointID);
//ODE_API dReal           (*dJointGetPUAngle2Rate)(dJointID);
//ODE_API dReal           (*dJointGetPUParam)(dJointID, int parameter);
//ODE_API dReal           (*dJointGetPistonPosition)(dJointID);
//ODE_API dReal           (*dJointGetPistonPositionRate)(dJointID);
//ODE_API dReal           (*dJointGetPistonAngle)(dJointID);
//ODE_API dReal           (*dJointGetPistonAngleRate)(dJointID);
//ODE_API void            (*dJointGetPistonAnchor)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPistonAnchor2)(dJointID, dVector3 result);
//ODE_API void            (*dJointGetPistonAxis)(dJointID, dVector3 result);
//ODE_API dReal           (*dJointGetPistonParam)(dJointID, int parameter);
//ODE_API int             (*dJointGetAMotorNumAxes)(dJointID);
//ODE_API void            (*dJointGetAMotorAxis)(dJointID, int anum, dVector3 result);
//ODE_API int             (*dJointGetAMotorAxisRel)(dJointID, int anum);
//ODE_API dReal           (*dJointGetAMotorAngle)(dJointID, int anum);
//ODE_API dReal           (*dJointGetAMotorAngleRate)(dJointID, int anum);
//ODE_API dReal           (*dJointGetAMotorParam)(dJointID, int parameter);
//ODE_API int             (*dJointGetAMotorMode)(dJointID);
//ODE_API int             (*dJointGetLMotorNumAxes)(dJointID);
//ODE_API void            (*dJointGetLMotorAxis)(dJointID, int anum, dVector3 result);
//ODE_API dReal           (*dJointGetLMotorParam)(dJointID, int parameter);
//ODE_API dReal           (*dJointGetFixedParam)(dJointID, int parameter);
//ODE_API dJointID        (*dConnectingJoint)(dBodyID, dBodyID);
//ODE_API int             (*dConnectingJointList)(dBodyID, dBodyID, dJointID*);
ODE_API int             (*dAreConnected)(dBodyID, dBodyID);
ODE_API int             (*dAreConnectedExcluding)(dBodyID body1, dBodyID body2, int joint_type);
//
ODE_API dSpaceID        (*dSimpleSpaceCreate)(dSpaceID space);
ODE_API dSpaceID        (*dHashSpaceCreate)(dSpaceID space);
ODE_API dSpaceID        (*dQuadTreeSpaceCreate)(dSpaceID space, const dVector3 Center, const dVector3 Extents, int Depth);
ODE_API dSpaceID        (*dSweepAndPruneSpaceCreate)( dSpaceID space, int axisorder );
ODE_API void            (*dSpaceDestroy)(dSpaceID);
//ODE_API void            (*dHashSpaceSetLevels)(dSpaceID space, int minlevel, int maxlevel);
//ODE_API void            (*dHashSpaceGetLevels)(dSpaceID space, int *minlevel, int *maxlevel);
//ODE_API void            (*dSpaceSetCleanup)(dSpaceID space, int mode);
//ODE_API int             (*dSpaceGetCleanup)(dSpaceID space);
//ODE_API void            (*dSpaceSetSublevel)(dSpaceID space, int sublevel);
//ODE_API int             (*dSpaceGetSublevel)(dSpaceID space);
//ODE_API void            (*dSpaceSetManualCleanup)(dSpaceID space, int mode);
//ODE_API int             (*dSpaceGetManualCleanup)(dSpaceID space);
//ODE_API void            (*dSpaceAdd)(dSpaceID, dGeomID);
//ODE_API void            (*dSpaceRemove)(dSpaceID, dGeomID);
//ODE_API int             (*dSpaceQuery)(dSpaceID, dGeomID);
//ODE_API void            (*dSpaceClean)(dSpaceID);
//ODE_API int             (*dSpaceGetNumGeoms)(dSpaceID);
//ODE_API dGeomID         (*dSpaceGetGeom)(dSpaceID, int i);
//ODE_API int             (*dSpaceGetClass)(dSpaceID space);
//
ODE_API void            (*dGeomDestroy)(dGeomID geom);
//ODE_API void            (*dGeomSetData)(dGeomID geom, void* data);
//ODE_API void *          (*dGeomGetData)(dGeomID geom);
ODE_API void            (*dGeomSetBody)(dGeomID geom, dBodyID body);
ODE_API dBodyID         (*dGeomGetBody)(dGeomID geom);
//ODE_API void            (*dGeomSetPosition)(dGeomID geom, dReal x, dReal y, dReal z);
ODE_API void            (*dGeomSetRotation)(dGeomID geom, const dMatrix3 R);
//ODE_API void            (*dGeomSetQuaternion)(dGeomID geom, const dQuaternion Q);
//ODE_API const dReal *   (*dGeomGetPosition)(dGeomID geom);
//ODE_API void            (*dGeomCopyPosition)(dGeomID geom, dVector3 pos);
//ODE_API const dReal *   (*dGeomGetRotation)(dGeomID geom);
//ODE_API void            (*dGeomCopyRotation)(dGeomID geom, dMatrix3 R);
//ODE_API void            (*dGeomGetQuaternion)(dGeomID geom, dQuaternion result);
//ODE_API void            (*dGeomGetAABB)(dGeomID geom, dReal aabb[6]);
ODE_API int             (*dGeomIsSpace)(dGeomID geom);
//ODE_API dSpaceID        (*dGeomGetSpace)(dGeomID);
//ODE_API int             (*dGeomGetClass)(dGeomID geom);
//ODE_API void            (*dGeomSetCategoryBits)(dGeomID geom, unsigned long bits);
//ODE_API void            (*dGeomSetCollideBits)(dGeomID geom, unsigned long bits);
//ODE_API unsigned long   (*dGeomGetCategoryBits)(dGeomID);
//ODE_API unsigned long   (*dGeomGetCollideBits)(dGeomID);
//ODE_API void            (*dGeomEnable)(dGeomID geom);
//ODE_API void            (*dGeomDisable)(dGeomID geom);
//ODE_API int             (*dGeomIsEnabled)(dGeomID geom);
//ODE_API void            (*dGeomSetOffsetPosition)(dGeomID geom, dReal x, dReal y, dReal z);
//ODE_API void            (*dGeomSetOffsetRotation)(dGeomID geom, const dMatrix3 R);
//ODE_API void            (*dGeomSetOffsetQuaternion)(dGeomID geom, const dQuaternion Q);
//ODE_API void            (*dGeomSetOffsetWorldPosition)(dGeomID geom, dReal x, dReal y, dReal z);
//ODE_API void            (*dGeomSetOffsetWorldRotation)(dGeomID geom, const dMatrix3 R);
//ODE_API void            (*dGeomSetOffsetWorldQuaternion)(dGeomID geom, const dQuaternion);
//ODE_API void            (*dGeomClearOffset)(dGeomID geom);
//ODE_API int             (*dGeomIsOffset)(dGeomID geom);
//ODE_API const dReal *   (*dGeomGetOffsetPosition)(dGeomID geom);
//ODE_API void            (*dGeomCopyOffsetPosition)(dGeomID geom, dVector3 pos);
//ODE_API const dReal *   (*dGeomGetOffsetRotation)(dGeomID geom);
//ODE_API void            (*dGeomCopyOffsetRotation)(dGeomID geom, dMatrix3 R);
//ODE_API void            (*dGeomGetOffsetQuaternion)(dGeomID geom, dQuaternion result);
ODE_API int             (*dCollide)(dGeomID o1, dGeomID o2, int flags, dContactGeom *contact, int skip);
//
ODE_API void            (*dSpaceCollide)(dSpaceID space, void *data, dNearCallback *callback);
ODE_API void            (*dSpaceCollide2)(dGeomID space1, dGeomID space2, void *data, dNearCallback *callback);
//
ODE_API dGeomID         (*dCreateSphere)(dSpaceID space, dReal radius);
//ODE_API void            (*dGeomSphereSetRadius)(dGeomID sphere, dReal radius);
//ODE_API dReal           (*dGeomSphereGetRadius)(dGeomID sphere);
//ODE_API dReal           (*dGeomSpherePointDepth)(dGeomID sphere, dReal x, dReal y, dReal z);
//
//ODE_API dGeomID         (*dCreateConvex)(dSpaceID space, dReal *_planes, unsigned int _planecount, dReal *_points, unsigned int _pointcount,unsigned int *_polygons);
//ODE_API void            (*dGeomSetConvex)(dGeomID g, dReal *_planes, unsigned int _count, dReal *_points, unsigned int _pointcount,unsigned int *_polygons);
//
ODE_API dGeomID         (*dCreateBox)(dSpaceID space, dReal lx, dReal ly, dReal lz);
//ODE_API void            (*dGeomBoxSetLengths)(dGeomID box, dReal lx, dReal ly, dReal lz);
//ODE_API void            (*dGeomBoxGetLengths)(dGeomID box, dVector3 result);
//ODE_API dReal           (*dGeomBoxPointDepth)(dGeomID box, dReal x, dReal y, dReal z);
//ODE_API dReal           (*dGeomBoxPointDepth)(dGeomID box, dReal x, dReal y, dReal z);
//
//ODE_API dGeomID         (*dCreatePlane)(dSpaceID space, dReal a, dReal b, dReal c, dReal d);
//ODE_API void            (*dGeomPlaneSetParams)(dGeomID plane, dReal a, dReal b, dReal c, dReal d);
//ODE_API void            (*dGeomPlaneGetParams)(dGeomID plane, dVector4 result);
//ODE_API dReal           (*dGeomPlanePointDepth)(dGeomID plane, dReal x, dReal y, dReal z);
//
ODE_API dGeomID         (*dCreateCapsule)(dSpaceID space, dReal radius, dReal length);
//ODE_API void            (*dGeomCapsuleSetParams)(dGeomID ccylinder, dReal radius, dReal length);
//ODE_API void            (*dGeomCapsuleGetParams)(dGeomID ccylinder, dReal *radius, dReal *length);
//ODE_API dReal           (*dGeomCapsulePointDepth)(dGeomID ccylinder, dReal x, dReal y, dReal z);
//
//ODE_API dGeomID         (*dCreateCylinder)(dSpaceID space, dReal radius, dReal length);
//ODE_API void            (*dGeomCylinderSetParams)(dGeomID cylinder, dReal radius, dReal length);
//ODE_API void            (*dGeomCylinderGetParams)(dGeomID cylinder, dReal *radius, dReal *length);
//
//ODE_API dGeomID         (*dCreateRay)(dSpaceID space, dReal length);
//ODE_API void            (*dGeomRaySetLength)(dGeomID ray, dReal length);
//ODE_API dReal           (*dGeomRayGetLength)(dGeomID ray);
//ODE_API void            (*dGeomRaySet)(dGeomID ray, dReal px, dReal py, dReal pz, dReal dx, dReal dy, dReal dz);
//ODE_API void            (*dGeomRayGet)(dGeomID ray, dVector3 start, dVector3 dir);
//
ODE_API dGeomID         (*dCreateGeomTransform)(dSpaceID space);
ODE_API void            (*dGeomTransformSetGeom)(dGeomID g, dGeomID obj);
//ODE_API dGeomID         (*dGeomTransformGetGeom)(dGeomID g);
ODE_API void            (*dGeomTransformSetCleanup)(dGeomID g, int mode);
//ODE_API int             (*dGeomTransformGetCleanup)(dGeomID g);
//ODE_API void            (*dGeomTransformSetInfo)(dGeomID g, int mode);
//ODE_API int             (*dGeomTransformGetInfo)(dGeomID g);

enum { TRIMESH_FACE_NORMALS };
typedef int dTriCallback(dGeomID TriMesh, dGeomID RefObject, int TriangleIndex);
typedef void dTriArrayCallback(dGeomID TriMesh, dGeomID RefObject, const int* TriIndices, int TriCount);
typedef int dTriRayCallback(dGeomID TriMesh, dGeomID Ray, int TriangleIndex, dReal u, dReal v);
typedef int dTriTriMergeCallback(dGeomID TriMesh, int FirstTriangleIndex, int SecondTriangleIndex);

ODE_API dTriMeshDataID  (*dGeomTriMeshDataCreate)(void);
ODE_API void            (*dGeomTriMeshDataDestroy)(dTriMeshDataID g);
//ODE_API void            (*dGeomTriMeshDataSet)(dTriMeshDataID g, int data_id, void* in_data);
//ODE_API void*           (*dGeomTriMeshDataGet)(dTriMeshDataID g, int data_id);
//ODE_API void            (*dGeomTriMeshSetLastTransform)( (*dGeomID g, dMatrix4 last_trans );
//ODE_API dReal*          (*dGeomTriMeshGetLastTransform)( (*dGeomID g );
ODE_API void            (*dGeomTriMeshDataBuildSingle)(dTriMeshDataID g, const void* Vertices, int VertexStride, int VertexCount,  const void* Indices, int IndexCount, int TriStride);
//ODE_API void            (*dGeomTriMeshDataBuildSingle1)(dTriMeshDataID g, const void* Vertices, int VertexStride, int VertexCount,  const void* Indices, int IndexCount, int TriStride, const void* Normals);
//ODE_API void            (*dGeomTriMeshDataBuildDouble)(dTriMeshDataID g,  const void* Vertices,  int VertexStride, int VertexCount,  const void* Indices, int IndexCount, int TriStride);
//ODE_API void            (*dGeomTriMeshDataBuildDouble1)(dTriMeshDataID g,  const void* Vertices,  int VertexStride, int VertexCount,  const void* Indices, int IndexCount, int TriStride, const void* Normals);
//ODE_API void            (*dGeomTriMeshDataBuildSimple)(dTriMeshDataID g, const dReal* Vertices, int VertexCount, const dTriIndex* Indices, int IndexCount);
//ODE_API void            (*dGeomTriMeshDataBuildSimple1)(dTriMeshDataID g, const dReal* Vertices, int VertexCount, const dTriIndex* Indices, int IndexCount, const int* Normals);
//ODE_API void            (*dGeomTriMeshDataPreprocess)(dTriMeshDataID g);
//ODE_API void            (*dGeomTriMeshDataGetBuffer)(dTriMeshDataID g, unsigned char** buf, int* bufLen);
//ODE_API void            (*dGeomTriMeshDataSetBuffer)(dTriMeshDataID g, unsigned char* buf);
//ODE_API void            (*dGeomTriMeshSetCallback)(dGeomID g, dTriCallback* Callback);
//ODE_API dTriCallback*   (*dGeomTriMeshGetCallback)(dGeomID g);
//ODE_API void            (*dGeomTriMeshSetArrayCallback)(dGeomID g, dTriArrayCallback* ArrayCallback);
//ODE_API dTriArrayCallback* (*dGeomTriMeshGetArrayCallback)(dGeomID g);
//ODE_API void            (*dGeomTriMeshSetRayCallback)(dGeomID g, dTriRayCallback* Callback);
//ODE_API dTriRayCallback* (*dGeomTriMeshGetRayCallback)(dGeomID g);
//ODE_API void            (*dGeomTriMeshSetTriMergeCallback)(dGeomID g, dTriTriMergeCallback* Callback);
//ODE_API dTriTriMergeCallback* (*dGeomTriMeshGetTriMergeCallback)(dGeomID g);
ODE_API dGeomID         (*dCreateTriMesh)(dSpaceID space, dTriMeshDataID Data, dTriCallback* Callback, dTriArrayCallback* ArrayCallback, dTriRayCallback* RayCallback);
//ODE_API void            (*dGeomTriMeshSetData)(dGeomID g, dTriMeshDataID Data);
//ODE_API dTriMeshDataID  (*dGeomTriMeshGetData)(dGeomID g);
//ODE_API void            (*dGeomTriMeshEnableTC)(dGeomID g, int geomClass, int enable);
//ODE_API int             (*dGeomTriMeshIsTCEnabled)(dGeomID g, int geomClass);
//ODE_API void            (*dGeomTriMeshClearTCCache)(dGeomID g);
//ODE_API dTriMeshDataID  (*dGeomTriMeshGetTriMeshDataID)(dGeomID g);
//ODE_API void            (*dGeomTriMeshGetTriangle)(dGeomID g, int Index, dVector3* v0, dVector3* v1, dVector3* v2);
//ODE_API void            (*dGeomTriMeshGetPoint)(dGeomID g, int Index, dReal u, dReal v, dVector3 Out);
//ODE_API int             (*dGeomTriMeshGetTriangleCount )(dGeomID g);
//ODE_API void            (*dGeomTriMeshDataUpdate)(dTriMeshDataID g);

static dllfunction_t odefuncs[] =
{
	{"dGetConfiguration",							(void **) &dGetConfiguration},
	{"dCheckConfiguration",							(void **) &dCheckConfiguration},
	{"dInitODE2",									(void **) &dInitODE2},
	{"dAllocateODEDataForThread",					(void **) &dAllocateODEDataForThread},
	{"dCleanupODEAllDataForThread",					(void **) &dCleanupODEAllDataForThread},
	{"dCloseODE",									(void **) &dCloseODE},
//	{"dMassCheck",									(void **) &dMassCheck},
//	{"dMassSetZero",								(void **) &dMassSetZero},
//	{"dMassSetParameters",							(void **) &dMassSetParameters},
//	{"dMassSetSphere",								(void **) &dMassSetSphere},
	{"dMassSetSphereTotal",							(void **) &dMassSetSphereTotal},
//	{"dMassSetCapsule",								(void **) &dMassSetCapsule},
	{"dMassSetCapsuleTotal",						(void **) &dMassSetCapsuleTotal},
//	{"dMassSetCylinder",							(void **) &dMassSetCylinder},
//	{"dMassSetCylinderTotal",						(void **) &dMassSetCylinderTotal},
//	{"dMassSetBox",									(void **) &dMassSetBox},
	{"dMassSetBoxTotal",							(void **) &dMassSetBoxTotal},
//	{"dMassSetTrimesh",								(void **) &dMassSetTrimesh},
//	{"dMassSetTrimeshTotal",						(void **) &dMassSetTrimeshTotal},
//	{"dMassAdjust",									(void **) &dMassAdjust},
//	{"dMassTranslate",								(void **) &dMassTranslate},
//	{"dMassRotate",									(void **) &dMassRotate},
//	{"dMassAdd",									(void **) &dMassAdd},

	{"dWorldCreate",								(void **) &dWorldCreate},
	{"dWorldDestroy",								(void **) &dWorldDestroy},
	{"dWorldSetGravity",							(void **) &dWorldSetGravity},
//	{"dWorldGetGravity",							(void **) &dWorldGetGravity},
//	{"dWorldSetERP",								(void **) &dWorldSetERP},
//	{"dWorldGetERP",								(void **) &dWorldGetERP},
//	{"dWorldSetCFM",								(void **) &dWorldSetCFM},
//	{"dWorldGetCFM",								(void **) &dWorldGetCFM},
	{"dWorldStep",									(void **) &dWorldStep},
//	{"dWorldImpulseToForce",						(void **) &dWorldImpulseToForce},
	{"dWorldQuickStep",								(void **) &dWorldQuickStep},
	{"dWorldSetQuickStepNumIterations",				(void **) &dWorldSetQuickStepNumIterations},
//	{"dWorldGetQuickStepNumIterations",				(void **) &dWorldGetQuickStepNumIterations},
//	{"dWorldSetQuickStepW",							(void **) &dWorldSetQuickStepW},
//	{"dWorldGetQuickStepW",							(void **) &dWorldGetQuickStepW},
//	{"dWorldSetContactMaxCorrectingVel",			(void **) &dWorldSetContactMaxCorrectingVel},
//	{"dWorldGetContactMaxCorrectingVel",			(void **) &dWorldGetContactMaxCorrectingVel},
	{"dWorldSetContactSurfaceLayer",				(void **) &dWorldSetContactSurfaceLayer},
//	{"dWorldGetContactSurfaceLayer",				(void **) &dWorldGetContactSurfaceLayer},
	{"dWorldStepFast1",								(void **) &dWorldStepFast1},
//	{"dWorldSetAutoEnableDepthSF1",					(void **) &dWorldSetAutoEnableDepthSF1},
//	{"dWorldGetAutoEnableDepthSF1",					(void **) &dWorldGetAutoEnableDepthSF1},
//	{"dWorldGetAutoDisableLinearThreshold",			(void **) &dWorldGetAutoDisableLinearThreshold},
//	{"dWorldSetAutoDisableLinearThreshold",			(void **) &dWorldSetAutoDisableLinearThreshold},
//	{"dWorldGetAutoDisableAngularThreshold",		(void **) &dWorldGetAutoDisableAngularThreshold},
//	{"dWorldSetAutoDisableAngularThreshold",		(void **) &dWorldSetAutoDisableAngularThreshold},
//	{"dWorldGetAutoDisableLinearAverageThreshold",	(void **) &dWorldGetAutoDisableLinearAverageThreshold},
//	{"dWorldSetAutoDisableLinearAverageThreshold",	(void **) &dWorldSetAutoDisableLinearAverageThreshold},
//	{"dWorldGetAutoDisableAngularAverageThreshold",	(void **) &dWorldGetAutoDisableAngularAverageThreshold},
//	{"dWorldSetAutoDisableAngularAverageThreshold",	(void **) &dWorldSetAutoDisableAngularAverageThreshold},
//	{"dWorldGetAutoDisableAverageSamplesCount",		(void **) &dWorldGetAutoDisableAverageSamplesCount},
//	{"dWorldSetAutoDisableAverageSamplesCount",		(void **) &dWorldSetAutoDisableAverageSamplesCount},
//	{"dWorldGetAutoDisableSteps",					(void **) &dWorldGetAutoDisableSteps},
//	{"dWorldSetAutoDisableSteps",					(void **) &dWorldSetAutoDisableSteps},
//	{"dWorldGetAutoDisableTime",					(void **) &dWorldGetAutoDisableTime},
//	{"dWorldSetAutoDisableTime",					(void **) &dWorldSetAutoDisableTime},
//	{"dWorldGetAutoDisableFlag",					(void **) &dWorldGetAutoDisableFlag},
//	{"dWorldSetAutoDisableFlag",					(void **) &dWorldSetAutoDisableFlag},
//	{"dWorldGetLinearDampingThreshold",				(void **) &dWorldGetLinearDampingThreshold},
//	{"dWorldSetLinearDampingThreshold",				(void **) &dWorldSetLinearDampingThreshold},
//	{"dWorldGetAngularDampingThreshold",			(void **) &dWorldGetAngularDampingThreshold},
//	{"dWorldSetAngularDampingThreshold",			(void **) &dWorldSetAngularDampingThreshold},
//	{"dWorldGetLinearDamping",						(void **) &dWorldGetLinearDamping},
//	{"dWorldSetLinearDamping",						(void **) &dWorldSetLinearDamping},
//	{"dWorldGetAngularDamping",						(void **) &dWorldGetAngularDamping},
//	{"dWorldSetAngularDamping",						(void **) &dWorldSetAngularDamping},
//	{"dWorldSetDamping",							(void **) &dWorldSetDamping},
//	{"dWorldGetMaxAngularSpeed",					(void **) &dWorldGetMaxAngularSpeed},
//	{"dWorldSetMaxAngularSpeed",					(void **) &dWorldSetMaxAngularSpeed},
//	{"dBodyGetAutoDisableLinearThreshold",			(void **) &dBodyGetAutoDisableLinearThreshold},
//	{"dBodySetAutoDisableLinearThreshold",			(void **) &dBodySetAutoDisableLinearThreshold},
//	{"dBodyGetAutoDisableAngularThreshold",			(void **) &dBodyGetAutoDisableAngularThreshold},
//	{"dBodySetAutoDisableAngularThreshold",			(void **) &dBodySetAutoDisableAngularThreshold},
//	{"dBodyGetAutoDisableAverageSamplesCount",		(void **) &dBodyGetAutoDisableAverageSamplesCount},
//	{"dBodySetAutoDisableAverageSamplesCount",		(void **) &dBodySetAutoDisableAverageSamplesCount},
//	{"dBodyGetAutoDisableSteps",					(void **) &dBodyGetAutoDisableSteps},
//	{"dBodySetAutoDisableSteps",					(void **) &dBodySetAutoDisableSteps},
//	{"dBodyGetAutoDisableTime",						(void **) &dBodyGetAutoDisableTime},
//	{"dBodySetAutoDisableTime",						(void **) &dBodySetAutoDisableTime},
//	{"dBodyGetAutoDisableFlag",						(void **) &dBodyGetAutoDisableFlag},
//	{"dBodySetAutoDisableFlag",						(void **) &dBodySetAutoDisableFlag},
//	{"dBodySetAutoDisableDefaults",					(void **) &dBodySetAutoDisableDefaults},
//	{"dBodyGetWorld",								(void **) &dBodyGetWorld},
	{"dBodyCreate",									(void **) &dBodyCreate},
	{"dBodyDestroy",								(void **) &dBodyDestroy},
	{"dBodySetData",								(void **) &dBodySetData},
//	{"dBodyGetData",								(void **) &dBodyGetData},
	{"dBodySetPosition",							(void **) &dBodySetPosition},
	{"dBodySetRotation",							(void **) &dBodySetRotation},
//	{"dBodySetQuaternion",							(void **) &dBodySetQuaternion},
	{"dBodySetLinearVel",							(void **) &dBodySetLinearVel},
	{"dBodySetAngularVel",							(void **) &dBodySetAngularVel},
	{"dBodyGetPosition",							(void **) &dBodyGetPosition},
//	{"dBodyCopyPosition",							(void **) &dBodyCopyPosition},
	{"dBodyGetRotation",							(void **) &dBodyGetRotation},
//	{"dBodyCopyRotation",							(void **) &dBodyCopyRotation},
//	{"dBodyGetQuaternion",							(void **) &dBodyGetQuaternion},
//	{"dBodyCopyQuaternion",							(void **) &dBodyCopyQuaternion},
	{"dBodyGetLinearVel",							(void **) &dBodyGetLinearVel},
	{"dBodyGetAngularVel",							(void **) &dBodyGetAngularVel},
	{"dBodySetMass",								(void **) &dBodySetMass},
//	{"dBodyGetMass",								(void **) &dBodyGetMass},
//	{"dBodyAddForce",								(void **) &dBodyAddForce},
//	{"dBodyAddTorque",								(void **) &dBodyAddTorque},
//	{"dBodyAddRelForce",							(void **) &dBodyAddRelForce},
//	{"dBodyAddRelTorque",							(void **) &dBodyAddRelTorque},
//	{"dBodyAddForceAtPos",							(void **) &dBodyAddForceAtPos},
//	{"dBodyAddForceAtRelPos",						(void **) &dBodyAddForceAtRelPos},
//	{"dBodyAddRelForceAtPos",						(void **) &dBodyAddRelForceAtPos},
//	{"dBodyAddRelForceAtRelPos",					(void **) &dBodyAddRelForceAtRelPos},
//	{"dBodyGetForce",								(void **) &dBodyGetForce},
//	{"dBodyGetTorque",								(void **) &dBodyGetTorque},
//	{"dBodySetForce",								(void **) &dBodySetForce},
//	{"dBodySetTorque",								(void **) &dBodySetTorque},
//	{"dBodyGetRelPointPos",							(void **) &dBodyGetRelPointPos},
//	{"dBodyGetRelPointVel",							(void **) &dBodyGetRelPointVel},
//	{"dBodyGetPointVel",							(void **) &dBodyGetPointVel},
//	{"dBodyGetPosRelPoint",							(void **) &dBodyGetPosRelPoint},
//	{"dBodyVectorToWorld",							(void **) &dBodyVectorToWorld},
//	{"dBodyVectorFromWorld",						(void **) &dBodyVectorFromWorld},
//	{"dBodySetFiniteRotationMode",					(void **) &dBodySetFiniteRotationMode},
//	{"dBodySetFiniteRotationAxis",					(void **) &dBodySetFiniteRotationAxis},
//	{"dBodyGetFiniteRotationMode",					(void **) &dBodyGetFiniteRotationMode},
//	{"dBodyGetFiniteRotationAxis",					(void **) &dBodyGetFiniteRotationAxis},
//	{"dBodyGetNumJoints",							(void **) &dBodyGetNumJoints},
//	{"dBodyGetJoint",								(void **) &dBodyGetJoint},
//	{"dBodySetDynamic",								(void **) &dBodySetDynamic},
//	{"dBodySetKinematic",							(void **) &dBodySetKinematic},
//	{"dBodyIsKinematic",							(void **) &dBodyIsKinematic},
//	{"dBodyEnable",									(void **) &dBodyEnable},
//	{"dBodyDisable",								(void **) &dBodyDisable},
//	{"dBodyIsEnabled",								(void **) &dBodyIsEnabled},
//	{"dBodySetGravityMode",							(void **) &dBodySetGravityMode},
//	{"dBodyGetGravityMode",							(void **) &dBodyGetGravityMode},
//	{"dBodySetMovedCallback",						(void **) &dBodySetMovedCallback},
//	{"dBodyGetFirstGeom",							(void **) &dBodyGetFirstGeom},
//	{"dBodyGetNextGeom",							(void **) &dBodyGetNextGeom},
//	{"dBodySetDampingDefaults",						(void **) &dBodySetDampingDefaults},
//	{"dBodyGetLinearDamping",						(void **) &dBodyGetLinearDamping},
//	{"dBodySetLinearDamping",						(void **) &dBodySetLinearDamping},
//	{"dBodyGetAngularDamping",						(void **) &dBodyGetAngularDamping},
//	{"dBodySetAngularDamping",						(void **) &dBodySetAngularDamping},
//	{"dBodySetDamping",								(void **) &dBodySetDamping},
//	{"dBodyGetLinearDampingThreshold",				(void **) &dBodyGetLinearDampingThreshold},
//	{"dBodySetLinearDampingThreshold",				(void **) &dBodySetLinearDampingThreshold},
//	{"dBodyGetAngularDampingThreshold",				(void **) &dBodyGetAngularDampingThreshold},
//	{"dBodySetAngularDampingThreshold",				(void **) &dBodySetAngularDampingThreshold},
//	{"dBodyGetMaxAngularSpeed",						(void **) &dBodyGetMaxAngularSpeed},
//	{"dBodySetMaxAngularSpeed",						(void **) &dBodySetMaxAngularSpeed},
//	{"dBodyGetGyroscopicMode",						(void **) &dBodyGetGyroscopicMode},
//	{"dBodySetGyroscopicMode",						(void **) &dBodySetGyroscopicMode},
//	{"dJointCreateBall",							(void **) &dJointCreateBall},
//	{"dJointCreateHinge",							(void **) &dJointCreateHinge},
//	{"dJointCreateSlider",							(void **) &dJointCreateSlider},
	{"dJointCreateContact",							(void **) &dJointCreateContact},
//	{"dJointCreateHinge2",							(void **) &dJointCreateHinge2},
//	{"dJointCreateUniversal",						(void **) &dJointCreateUniversal},
//	{"dJointCreatePR",								(void **) &dJointCreatePR},
//	{"dJointCreatePU",								(void **) &dJointCreatePU},
//	{"dJointCreatePiston",							(void **) &dJointCreatePiston},
//	{"dJointCreateFixed",							(void **) &dJointCreateFixed},
//	{"dJointCreateNull",							(void **) &dJointCreateNull},
//	{"dJointCreateAMotor",							(void **) &dJointCreateAMotor},
//	{"dJointCreateLMotor",							(void **) &dJointCreateLMotor},
//	{"dJointCreatePlane2D",							(void **) &dJointCreatePlane2D},
//	{"dJointDestroy",								(void **) &dJointDestroy},
	{"dJointGroupCreate",							(void **) &dJointGroupCreate},
	{"dJointGroupDestroy",							(void **) &dJointGroupDestroy},
	{"dJointGroupEmpty",							(void **) &dJointGroupEmpty},
//	{"dJointGetNumBodies",							(void **) &dJointGetNumBodies},
	{"dJointAttach",								(void **) &dJointAttach},
//	{"dJointEnable",								(void **) &dJointEnable},
//	{"dJointDisable",								(void **) &dJointDisable},
//	{"dJointIsEnabled",								(void **) &dJointIsEnabled},
//	{"dJointSetData",								(void **) &dJointSetData},
//	{"dJointGetData",								(void **) &dJointGetData},
//	{"dJointGetType",								(void **) &dJointGetType},
//	{"dJointGetBody",								(void **) &dJointGetBody},
//	{"dJointSetFeedback",							(void **) &dJointSetFeedback},
//	{"dJointGetFeedback",							(void **) &dJointGetFeedback},
//	{"dJointSetBallAnchor",							(void **) &dJointSetBallAnchor},
//	{"dJointSetBallAnchor2",						(void **) &dJointSetBallAnchor2},
//	{"dJointSetBallParam",							(void **) &dJointSetBallParam},
//	{"dJointSetHingeAnchor",						(void **) &dJointSetHingeAnchor},
//	{"dJointSetHingeAnchorDelta",					(void **) &dJointSetHingeAnchorDelta},
//	{"dJointSetHingeAxis",							(void **) &dJointSetHingeAxis},
//	{"dJointSetHingeAxisOffset",					(void **) &dJointSetHingeAxisOffset},
//	{"dJointSetHingeParam",							(void **) &dJointSetHingeParam},
//	{"dJointAddHingeTorque",						(void **) &dJointAddHingeTorque},
//	{"dJointSetSliderAxis",							(void **) &dJointSetSliderAxis},
//	{"dJointSetSliderAxisDelta",					(void **) &dJointSetSliderAxisDelta},
//	{"dJointSetSliderParam",						(void **) &dJointSetSliderParam},
//	{"dJointAddSliderForce",						(void **) &dJointAddSliderForce},
//	{"dJointSetHinge2Anchor",						(void **) &dJointSetHinge2Anchor},
//	{"dJointSetHinge2Axis1",						(void **) &dJointSetHinge2Axis1},
//	{"dJointSetHinge2Axis2",						(void **) &dJointSetHinge2Axis2},
//	{"dJointSetHinge2Param",						(void **) &dJointSetHinge2Param},
//	{"dJointAddHinge2Torques",						(void **) &dJointAddHinge2Torques},
//	{"dJointSetUniversalAnchor",					(void **) &dJointSetUniversalAnchor},
//	{"dJointSetUniversalAxis1",						(void **) &dJointSetUniversalAxis1},
//	{"dJointSetUniversalAxis1Offset",				(void **) &dJointSetUniversalAxis1Offset},
//	{"dJointSetUniversalAxis2",						(void **) &dJointSetUniversalAxis2},
//	{"dJointSetUniversalAxis2Offset",				(void **) &dJointSetUniversalAxis2Offset},
//	{"dJointSetUniversalParam",						(void **) &dJointSetUniversalParam},
//	{"dJointAddUniversalTorques",					(void **) &dJointAddUniversalTorques},
//	{"dJointSetPRAnchor",							(void **) &dJointSetPRAnchor},
//	{"dJointSetPRAxis1",							(void **) &dJointSetPRAxis1},
//	{"dJointSetPRAxis2",							(void **) &dJointSetPRAxis2},
//	{"dJointSetPRParam",							(void **) &dJointSetPRParam},
//	{"dJointAddPRTorque",							(void **) &dJointAddPRTorque},
//	{"dJointSetPUAnchor",							(void **) &dJointSetPUAnchor},
//	{"dJointSetPUAnchorOffset",						(void **) &dJointSetPUAnchorOffset},
//	{"dJointSetPUAxis1",							(void **) &dJointSetPUAxis1},
//	{"dJointSetPUAxis2",							(void **) &dJointSetPUAxis2},
//	{"dJointSetPUAxis3",							(void **) &dJointSetPUAxis3},
//	{"dJointSetPUAxisP",							(void **) &dJointSetPUAxisP},
//	{"dJointSetPUParam",							(void **) &dJointSetPUParam},
//	{"dJointAddPUTorque",							(void **) &dJointAddPUTorque},
//	{"dJointSetPistonAnchor",						(void **) &dJointSetPistonAnchor},
//	{"dJointSetPistonAnchorOffset",					(void **) &dJointSetPistonAnchorOffset},
//	{"dJointSetPistonParam",						(void **) &dJointSetPistonParam},
//	{"dJointAddPistonForce",						(void **) &dJointAddPistonForce},
//	{"dJointSetFixed",								(void **) &dJointSetFixed},
//	{"dJointSetFixedParam",							(void **) &dJointSetFixedParam},
//	{"dJointSetAMotorNumAxes",						(void **) &dJointSetAMotorNumAxes},
//	{"dJointSetAMotorAxis",							(void **) &dJointSetAMotorAxis},
//	{"dJointSetAMotorAngle",						(void **) &dJointSetAMotorAngle},
//	{"dJointSetAMotorParam",						(void **) &dJointSetAMotorParam},
//	{"dJointSetAMotorMode",							(void **) &dJointSetAMotorMode},
//	{"dJointAddAMotorTorques",						(void **) &dJointAddAMotorTorques},
//	{"dJointSetLMotorNumAxes",						(void **) &dJointSetLMotorNumAxes},
//	{"dJointSetLMotorAxis",							(void **) &dJointSetLMotorAxis},
//	{"dJointSetLMotorParam",						(void **) &dJointSetLMotorParam},
//	{"dJointSetPlane2DXParam",						(void **) &dJointSetPlane2DXParam},
//	{"dJointSetPlane2DYParam",						(void **) &dJointSetPlane2DYParam},
//	{"dJointSetPlane2DAngleParam",					(void **) &dJointSetPlane2DAngleParam},
//	{"dJointGetBallAnchor",							(void **) &dJointGetBallAnchor},
//	{"dJointGetBallAnchor2",						(void **) &dJointGetBallAnchor2},
//	{"dJointGetBallParam",							(void **) &dJointGetBallParam},
//	{"dJointGetHingeAnchor",						(void **) &dJointGetHingeAnchor},
//	{"dJointGetHingeAnchor2",						(void **) &dJointGetHingeAnchor2},
//	{"dJointGetHingeAxis",							(void **) &dJointGetHingeAxis},
//	{"dJointGetHingeParam",							(void **) &dJointGetHingeParam},
//	{"dJointGetHingeAngle",							(void **) &dJointGetHingeAngle},
//	{"dJointGetHingeAngleRate",						(void **) &dJointGetHingeAngleRate},
//	{"dJointGetSliderPosition",						(void **) &dJointGetSliderPosition},
//	{"dJointGetSliderPositionRate",					(void **) &dJointGetSliderPositionRate},
//	{"dJointGetSliderAxis",							(void **) &dJointGetSliderAxis},
//	{"dJointGetSliderParam",						(void **) &dJointGetSliderParam},
//	{"dJointGetHinge2Anchor",						(void **) &dJointGetHinge2Anchor},
//	{"dJointGetHinge2Anchor2",						(void **) &dJointGetHinge2Anchor2},
//	{"dJointGetHinge2Axis1",						(void **) &dJointGetHinge2Axis1},
//	{"dJointGetHinge2Axis2",						(void **) &dJointGetHinge2Axis2},
//	{"dJointGetHinge2Param",						(void **) &dJointGetHinge2Param},
//	{"dJointGetHinge2Angle1",						(void **) &dJointGetHinge2Angle1},
//	{"dJointGetHinge2Angle1Rate",					(void **) &dJointGetHinge2Angle1Rate},
//	{"dJointGetHinge2Angle2Rate",					(void **) &dJointGetHinge2Angle2Rate},
//	{"dJointGetUniversalAnchor",					(void **) &dJointGetUniversalAnchor},
//	{"dJointGetUniversalAnchor2",					(void **) &dJointGetUniversalAnchor2},
//	{"dJointGetUniversalAxis1",						(void **) &dJointGetUniversalAxis1},
//	{"dJointGetUniversalAxis2",						(void **) &dJointGetUniversalAxis2},
//	{"dJointGetUniversalParam",						(void **) &dJointGetUniversalParam},
//	{"dJointGetUniversalAngles",					(void **) &dJointGetUniversalAngles},
//	{"dJointGetUniversalAngle1",					(void **) &dJointGetUniversalAngle1},
//	{"dJointGetUniversalAngle2",					(void **) &dJointGetUniversalAngle2},
//	{"dJointGetUniversalAngle1Rate",				(void **) &dJointGetUniversalAngle1Rate},
//	{"dJointGetUniversalAngle2Rate",				(void **) &dJointGetUniversalAngle2Rate},
//	{"dJointGetPRAnchor",							(void **) &dJointGetPRAnchor},
//	{"dJointGetPRPosition",							(void **) &dJointGetPRPosition},
//	{"dJointGetPRPositionRate",						(void **) &dJointGetPRPositionRate},
//	{"dJointGetPRAngle",							(void **) &dJointGetPRAngle},
//	{"dJointGetPRAngleRate",						(void **) &dJointGetPRAngleRate},
//	{"dJointGetPRAxis1",							(void **) &dJointGetPRAxis1},
//	{"dJointGetPRAxis2",							(void **) &dJointGetPRAxis2},
//	{"dJointGetPRParam",							(void **) &dJointGetPRParam},
//	{"dJointGetPUAnchor",							(void **) &dJointGetPUAnchor},
//	{"dJointGetPUPosition",							(void **) &dJointGetPUPosition},
//	{"dJointGetPUPositionRate",						(void **) &dJointGetPUPositionRate},
//	{"dJointGetPUAxis1",							(void **) &dJointGetPUAxis1},
//	{"dJointGetPUAxis2",							(void **) &dJointGetPUAxis2},
//	{"dJointGetPUAxis3",							(void **) &dJointGetPUAxis3},
//	{"dJointGetPUAxisP",							(void **) &dJointGetPUAxisP},
//	{"dJointGetPUAngles",							(void **) &dJointGetPUAngles},
//	{"dJointGetPUAngle1",							(void **) &dJointGetPUAngle1},
//	{"dJointGetPUAngle1Rate",						(void **) &dJointGetPUAngle1Rate},
//	{"dJointGetPUAngle2",							(void **) &dJointGetPUAngle2},
//	{"dJointGetPUAngle2Rate",						(void **) &dJointGetPUAngle2Rate},
//	{"dJointGetPUParam",							(void **) &dJointGetPUParam},
//	{"dJointGetPistonPosition",						(void **) &dJointGetPistonPosition},
//	{"dJointGetPistonPositionRate",					(void **) &dJointGetPistonPositionRate},
//	{"dJointGetPistonAngle",						(void **) &dJointGetPistonAngle},
//	{"dJointGetPistonAngleRate",					(void **) &dJointGetPistonAngleRate},
//	{"dJointGetPistonAnchor",						(void **) &dJointGetPistonAnchor},
//	{"dJointGetPistonAnchor2",						(void **) &dJointGetPistonAnchor2},
//	{"dJointGetPistonAxis",							(void **) &dJointGetPistonAxis},
//	{"dJointGetPistonParam",						(void **) &dJointGetPistonParam},
//	{"dJointGetAMotorNumAxes",						(void **) &dJointGetAMotorNumAxes},
//	{"dJointGetAMotorAxis",							(void **) &dJointGetAMotorAxis},
//	{"dJointGetAMotorAxisRel",						(void **) &dJointGetAMotorAxisRel},
//	{"dJointGetAMotorAngle",						(void **) &dJointGetAMotorAngle},
//	{"dJointGetAMotorAngleRate",					(void **) &dJointGetAMotorAngleRate},
//	{"dJointGetAMotorParam",						(void **) &dJointGetAMotorParam},
//	{"dJointGetAMotorMode",							(void **) &dJointGetAMotorMode},
//	{"dJointGetLMotorNumAxes",						(void **) &dJointGetLMotorNumAxes},
//	{"dJointGetLMotorAxis",							(void **) &dJointGetLMotorAxis},
//	{"dJointGetLMotorParam",						(void **) &dJointGetLMotorParam},
//	{"dJointGetFixedParam",							(void **) &dJointGetFixedParam},
//	{"dConnectingJoint",							(void **) &dConnectingJoint},
//	{"dConnectingJointList",						(void **) &dConnectingJointList},
	{"dAreConnected",								(void **) &dAreConnected},
	{"dAreConnectedExcluding",						(void **) &dAreConnectedExcluding},
	{"dSimpleSpaceCreate",							(void **) &dSimpleSpaceCreate},
	{"dHashSpaceCreate",							(void **) &dHashSpaceCreate},
	{"dQuadTreeSpaceCreate",						(void **) &dQuadTreeSpaceCreate},
	{"dSweepAndPruneSpaceCreate",					(void **) &dSweepAndPruneSpaceCreate},
	{"dSpaceDestroy",								(void **) &dSpaceDestroy},
//	{"dHashSpaceSetLevels",							(void **) &dHashSpaceSetLevels},
//	{"dHashSpaceGetLevels",							(void **) &dHashSpaceGetLevels},
//	{"dSpaceSetCleanup",							(void **) &dSpaceSetCleanup},
//	{"dSpaceGetCleanup",							(void **) &dSpaceGetCleanup},
//	{"dSpaceSetSublevel",							(void **) &dSpaceSetSublevel},
//	{"dSpaceGetSublevel",							(void **) &dSpaceGetSublevel},
//	{"dSpaceSetManualCleanup",						(void **) &dSpaceSetManualCleanup},
//	{"dSpaceGetManualCleanup",						(void **) &dSpaceGetManualCleanup},
//	{"dSpaceAdd",									(void **) &dSpaceAdd},
//	{"dSpaceRemove",								(void **) &dSpaceRemove},
//	{"dSpaceQuery",									(void **) &dSpaceQuery},
//	{"dSpaceClean",									(void **) &dSpaceClean},
//	{"dSpaceGetNumGeoms",							(void **) &dSpaceGetNumGeoms},
//	{"dSpaceGetGeom",								(void **) &dSpaceGetGeom},
//	{"dSpaceGetClass",								(void **) &dSpaceGetClass},
	{"dGeomDestroy",								(void **) &dGeomDestroy},
//	{"dGeomSetData",								(void **) &dGeomSetData},
//	{"dGeomGetData",								(void **) &dGeomGetData},
	{"dGeomSetBody",								(void **) &dGeomSetBody},
	{"dGeomGetBody",								(void **) &dGeomGetBody},
//	{"dGeomSetPosition",							(void **) &dGeomSetPosition},
	{"dGeomSetRotation",							(void **) &dGeomSetRotation},
//	{"dGeomSetQuaternion",							(void **) &dGeomSetQuaternion},
//	{"dGeomGetPosition",							(void **) &dGeomGetPosition},
//	{"dGeomCopyPosition",							(void **) &dGeomCopyPosition},
//	{"dGeomGetRotation",							(void **) &dGeomGetRotation},
//	{"dGeomCopyRotation",							(void **) &dGeomCopyRotation},
//	{"dGeomGetQuaternion",							(void **) &dGeomGetQuaternion},
//	{"dGeomGetAABB",								(void **) &dGeomGetAABB},
	{"dGeomIsSpace",								(void **) &dGeomIsSpace},
//	{"dGeomGetSpace",								(void **) &dGeomGetSpace},
//	{"dGeomGetClass",								(void **) &dGeomGetClass},
//	{"dGeomSetCategoryBits",						(void **) &dGeomSetCategoryBits},
//	{"dGeomSetCollideBits",							(void **) &dGeomSetCollideBits},
//	{"dGeomGetCategoryBits",						(void **) &dGeomGetCategoryBits},
//	{"dGeomGetCollideBits",							(void **) &dGeomGetCollideBits},
//	{"dGeomEnable",									(void **) &dGeomEnable},
//	{"dGeomDisable",								(void **) &dGeomDisable},
//	{"dGeomIsEnabled",								(void **) &dGeomIsEnabled},
//	{"dGeomSetOffsetPosition",						(void **) &dGeomSetOffsetPosition},
//	{"dGeomSetOffsetRotation",						(void **) &dGeomSetOffsetRotation},
//	{"dGeomSetOffsetQuaternion",					(void **) &dGeomSetOffsetQuaternion},
//	{"dGeomSetOffsetWorldPosition",					(void **) &dGeomSetOffsetWorldPosition},
//	{"dGeomSetOffsetWorldRotation",					(void **) &dGeomSetOffsetWorldRotation},
//	{"dGeomSetOffsetWorldQuaternion",				(void **) &dGeomSetOffsetWorldQuaternion},
//	{"dGeomClearOffset",							(void **) &dGeomClearOffset},
//	{"dGeomIsOffset",								(void **) &dGeomIsOffset},
//	{"dGeomGetOffsetPosition",						(void **) &dGeomGetOffsetPosition},
//	{"dGeomCopyOffsetPosition",						(void **) &dGeomCopyOffsetPosition},
//	{"dGeomGetOffsetRotation",						(void **) &dGeomGetOffsetRotation},
//	{"dGeomCopyOffsetRotation",						(void **) &dGeomCopyOffsetRotation},
//	{"dGeomGetOffsetQuaternion",					(void **) &dGeomGetOffsetQuaternion},
	{"dCollide",									(void **) &dCollide},
	{"dSpaceCollide",								(void **) &dSpaceCollide},
	{"dSpaceCollide2",								(void **) &dSpaceCollide2},
	{"dCreateSphere",								(void **) &dCreateSphere},
//	{"dGeomSphereSetRadius",						(void **) &dGeomSphereSetRadius},
//	{"dGeomSphereGetRadius",						(void **) &dGeomSphereGetRadius},
//	{"dGeomSpherePointDepth",						(void **) &dGeomSpherePointDepth},
//	{"dCreateConvex",								(void **) &dCreateConvex},
//	{"dGeomSetConvex",								(void **) &dGeomSetConvex},
	{"dCreateBox",									(void **) &dCreateBox},
//	{"dGeomBoxSetLengths",							(void **) &dGeomBoxSetLengths},
//	{"dGeomBoxGetLengths",							(void **) &dGeomBoxGetLengths},
//	{"dGeomBoxPointDepth",							(void **) &dGeomBoxPointDepth},
//	{"dGeomBoxPointDepth",							(void **) &dGeomBoxPointDepth},
//	{"dCreatePlane",								(void **) &dCreatePlane},
//	{"dGeomPlaneSetParams",							(void **) &dGeomPlaneSetParams},
//	{"dGeomPlaneGetParams",							(void **) &dGeomPlaneGetParams},
//	{"dGeomPlanePointDepth",						(void **) &dGeomPlanePointDepth},
	{"dCreateCapsule",								(void **) &dCreateCapsule},
//	{"dGeomCapsuleSetParams",						(void **) &dGeomCapsuleSetParams},
//	{"dGeomCapsuleGetParams",						(void **) &dGeomCapsuleGetParams},
//	{"dGeomCapsulePointDepth",						(void **) &dGeomCapsulePointDepth},
//	{"dCreateCylinder",								(void **) &dCreateCylinder},
//	{"dGeomCylinderSetParams",						(void **) &dGeomCylinderSetParams},
//	{"dGeomCylinderGetParams",						(void **) &dGeomCylinderGetParams},
//	{"dCreateRay",									(void **) &dCreateRay},
//	{"dGeomRaySetLength",							(void **) &dGeomRaySetLength},
//	{"dGeomRayGetLength",							(void **) &dGeomRayGetLength},
//	{"dGeomRaySet",									(void **) &dGeomRaySet},
//	{"dGeomRayGet",									(void **) &dGeomRayGet},
	{"dCreateGeomTransform",						(void **) &dCreateGeomTransform},
	{"dGeomTransformSetGeom",						(void **) &dGeomTransformSetGeom},
//	{"dGeomTransformGetGeom",						(void **) &dGeomTransformGetGeom},
	{"dGeomTransformSetCleanup",					(void **) &dGeomTransformSetCleanup},
//	{"dGeomTransformGetCleanup",					(void **) &dGeomTransformGetCleanup},
//	{"dGeomTransformSetInfo",						(void **) &dGeomTransformSetInfo},
//	{"dGeomTransformGetInfo",						(void **) &dGeomTransformGetInfo},
	{"dGeomTriMeshDataCreate",                      (void **) &dGeomTriMeshDataCreate},
	{"dGeomTriMeshDataDestroy",                     (void **) &dGeomTriMeshDataDestroy},
//	{"dGeomTriMeshDataSet",                         (void **) &dGeomTriMeshDataSet},
//	{"dGeomTriMeshDataGet",                         (void **) &dGeomTriMeshDataGet},
//	{"dGeomTriMeshSetLastTransform",                (void **) &dGeomTriMeshSetLastTransform},
//	{"dGeomTriMeshGetLastTransform",                (void **) &dGeomTriMeshGetLastTransform},
	{"dGeomTriMeshDataBuildSingle",                 (void **) &dGeomTriMeshDataBuildSingle},
//	{"dGeomTriMeshDataBuildSingle1",                (void **) &dGeomTriMeshDataBuildSingle1},
//	{"dGeomTriMeshDataBuildDouble",                 (void **) &dGeomTriMeshDataBuildDouble},
//	{"dGeomTriMeshDataBuildDouble1",                (void **) &dGeomTriMeshDataBuildDouble1},
//	{"dGeomTriMeshDataBuildSimple",                 (void **) &dGeomTriMeshDataBuildSimple},
//	{"dGeomTriMeshDataBuildSimple1",                (void **) &dGeomTriMeshDataBuildSimple1},
//	{"dGeomTriMeshDataPreprocess",                  (void **) &dGeomTriMeshDataPreprocess},
//	{"dGeomTriMeshDataGetBuffer",                   (void **) &dGeomTriMeshDataGetBuffer},
//	{"dGeomTriMeshDataSetBuffer",                   (void **) &dGeomTriMeshDataSetBuffer},
//	{"dGeomTriMeshSetCallback",                     (void **) &dGeomTriMeshSetCallback},
//	{"dGeomTriMeshGetCallback",                     (void **) &dGeomTriMeshGetCallback},
//	{"dGeomTriMeshSetArrayCallback",                (void **) &dGeomTriMeshSetArrayCallback},
//	{"dGeomTriMeshGetArrayCallback",                (void **) &dGeomTriMeshGetArrayCallback},
//	{"dGeomTriMeshSetRayCallback",                  (void **) &dGeomTriMeshSetRayCallback},
//	{"dGeomTriMeshGetRayCallback",                  (void **) &dGeomTriMeshGetRayCallback},
//	{"dGeomTriMeshSetTriMergeCallback",             (void **) &dGeomTriMeshSetTriMergeCallback},
//	{"dGeomTriMeshGetTriMergeCallback",             (void **) &dGeomTriMeshGetTriMergeCallback},
	{"dCreateTriMesh",                              (void **) &dCreateTriMesh},
//	{"dGeomTriMeshSetData",                         (void **) &dGeomTriMeshSetData},
//	{"dGeomTriMeshGetData",                         (void **) &dGeomTriMeshGetData},
//	{"dGeomTriMeshEnableTC",                        (void **) &dGeomTriMeshEnableTC},
//	{"dGeomTriMeshIsTCEnabled",                     (void **) &dGeomTriMeshIsTCEnabled},
//	{"dGeomTriMeshClearTCCache",                    (void **) &dGeomTriMeshClearTCCache},
//	{"dGeomTriMeshGetTriMeshDataID",                (void **) &dGeomTriMeshGetTriMeshDataID},
//	{"dGeomTriMeshGetTriangle",                     (void **) &dGeomTriMeshGetTriangle},
//	{"dGeomTriMeshGetPoint",                        (void **) &dGeomTriMeshGetPoint},
//	{"dGeomTriMeshGetTriangleCount",                (void **) &dGeomTriMeshGetTriangleCount},
//	{"dGeomTriMeshDataUpdate",                      (void **) &dGeomTriMeshDataUpdate},
	{NULL, NULL}
};

// Handle for ODE DLL
dllhandle_t ode_dll = NULL;
#endif
#endif

static void World_Physics_Init(void)
{
#ifdef USEODE
#ifndef ODE_STATIC
	const char* dllnames [] =
	{
# if defined(WIN64)
		"libode1_64.dll",
# elif defined(WIN32)
		"libode1.dll",
# elif defined(MACOSX)
		"libode.1.dylib",
# else
		"libode.so.1",
# endif
		NULL
	};
#endif

	Cvar_RegisterVariable(&physics_ode_quadtree_depth);
	Cvar_RegisterVariable(&physics_ode_contactsurfacelayer);
	Cvar_RegisterVariable(&physics_ode_worldquickstep);
	Cvar_RegisterVariable(&physics_ode_worldquickstep_iterations);
	Cvar_RegisterVariable(&physics_ode_worldstepfast);
	Cvar_RegisterVariable(&physics_ode_worldstepfast_iterations);
	Cvar_RegisterVariable(&physics_ode_contact_mu);
	Cvar_RegisterVariable(&physics_ode_contact_erp);
	Cvar_RegisterVariable(&physics_ode_contact_cfm);
	Cvar_RegisterVariable(&physics_ode_iterationsperframe);
	Cvar_RegisterVariable(&physics_ode_movelimit);
	Cvar_RegisterVariable(&physics_ode_spinlimit);

#ifndef ODE_STATIC
	// Load the DLL
	if (Sys_LoadLibrary (dllnames, &ode_dll, odefuncs))
#endif
	{
		dInitODE2(0);
#ifndef ODE_STATIC
# ifdef dSINGLE
		if (!dCheckConfiguration("ODE_single_precision"))
# else
		if (!dCheckConfiguration("ODE_double_precision"))
# endif
		{
# ifdef dSINGLE
			Con_Printf("ode library not compiled for single precision - incompatible!  Not using ODE physics.\n");
# else
			Con_Printf("ode library not compiled for double precision - incompatible!  Not using ODE physics.\n");
# endif
			Sys_UnloadLibrary(ode_dll);
			ode_dll = NULL;
		}
#endif
	}
#endif
}

static void World_Physics_Shutdown(void)
{
#ifdef USEODE
#ifndef ODE_STATIC
	if (ode_dll)
#endif
	{
		dCloseODE();
#ifndef ODE_STATIC
		Sys_UnloadLibrary(ode_dll);
		ode_dll = NULL;
#endif
	}
#endif
}

#ifdef USEODE
static void World_Physics_EnableODE(world_t *world)
{
	dVector3 center, extents;
	if (world->physics.ode)
		return;
#ifndef ODE_STATIC
	if (!ode_dll)
		return;
#endif
	world->physics.ode = true;
	VectorMAM(0.5f, world->mins, 0.5f, world->maxs, center);
	VectorSubtract(world->maxs, center, extents);
	world->physics.ode_world = dWorldCreate();
	world->physics.ode_space = dQuadTreeSpaceCreate(NULL, center, extents, bound(1, physics_ode_quadtree_depth.integer, 10));
	world->physics.ode_contactgroup = dJointGroupCreate(0);
	// we don't currently set dWorldSetCFM or dWorldSetERP because the defaults seem fine
}
#endif

static void World_Physics_Start(world_t *world)
{
#ifdef USEODE
	if (world->physics.ode)
		return;
	World_Physics_EnableODE(world);
#endif
}

static void World_Physics_End(world_t *world)
{
#ifdef USEODE
	if (world->physics.ode)
	{
		dWorldDestroy(world->physics.ode_world);
		dSpaceDestroy(world->physics.ode_space);
		dJointGroupDestroy(world->physics.ode_contactgroup);
		world->physics.ode = false;
	}
#endif
}

void World_Physics_RemoveFromEntity(world_t *world, prvm_edict_t *ed)
{
#ifdef USEODE
	// entity is not physics controlled, free any physics data
	ed->priv.server->ode_physics = false;
	if (ed->priv.server->ode_geom)
		dGeomDestroy((dGeomID)ed->priv.server->ode_geom);
	ed->priv.server->ode_geom = NULL;
	if (ed->priv.server->ode_body)
		dBodyDestroy((dBodyID)ed->priv.server->ode_body);
	ed->priv.server->ode_body = NULL;
	if (ed->priv.server->ode_vertex3f)
		Mem_Free(ed->priv.server->ode_vertex3f);
	ed->priv.server->ode_vertex3f = NULL;
	ed->priv.server->ode_numvertices = 0;
	if (ed->priv.server->ode_element3i)
		Mem_Free(ed->priv.server->ode_element3i);
	ed->priv.server->ode_element3i = NULL;
	ed->priv.server->ode_numtriangles = 0;
#endif
}

#ifdef USEODE
static void World_Physics_Frame_BodyToEntity(world_t *world, prvm_edict_t *ed)
{
	const dReal *avel;
	const dReal *o;
	const dReal *r; // for some reason dBodyGetRotation returns a [3][4] matrix
	const dReal *vel;
	dBodyID body = (dBodyID)ed->priv.server->ode_body;
	int movetype;
	matrix4x4_t bodymatrix;
	matrix4x4_t entitymatrix;
	prvm_eval_t *val;
	vec3_t forward, left, up;
	vec3_t origin;
	vec3_t spinvelocity;
	vec3_t velocity;
	if (!body)
		return;
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.movetype);
	movetype = (int)val->_float;
	if (movetype != MOVETYPE_PHYSICS)
		return;
	// store the physics engine data into the entity
	o = dBodyGetPosition(body);
	r = dBodyGetRotation(body);
	vel = dBodyGetLinearVel(body);
	avel = dBodyGetAngularVel(body);
	VectorCopy(o, origin);
	forward[0] = r[0];
	forward[1] = r[4];
	forward[2] = r[8];
	left[0] = r[1];
	left[1] = r[5];
	left[2] = r[9];
	up[0] = r[2];
	up[1] = r[6];
	up[2] = r[10];
	VectorCopy(vel, velocity);
	VectorCopy(avel, spinvelocity);
	Matrix4x4_FromVectors(&bodymatrix, forward, left, up, origin);
	Matrix4x4_Concat(&entitymatrix, &bodymatrix, &ed->priv.server->ode_offsetimatrix);
	Matrix4x4_ToVectors(&entitymatrix, forward, left, up, origin);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.origin);if (val) VectorCopy(origin, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_forward);if (val) VectorCopy(forward, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_left);if (val) VectorCopy(left, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_up);if (val) VectorCopy(up, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.velocity);if (val) VectorCopy(velocity, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.spinvelocity);if (val) VectorCopy(spinvelocity, val->vector);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.angles);if (val) AnglesFromVectors(val->vector, forward, up, true);
}

static void World_Physics_Frame_BodyFromEntity(world_t *world, prvm_edict_t *ed)
{
	const float *iv;
	const int *ie;
	dBodyID body = (dBodyID)ed->priv.server->ode_body;
	dMass mass;
	dReal test;
	void *dataID;
	dVector3 capsulerot[3];
	dp_model_t *model;
	float *ov;
	int *oe;
	int axisindex;
	int modelindex = 0;
	int movetype;
	int numtriangles;
	int numvertices;
	int solid;
	int triangleindex;
	int vertexindex;
	mempool_t *mempool;
	prvm_eval_t *val;
	vec3_t angles;
	vec3_t avelocity;
	vec3_t entmaxs;
	vec3_t entmins;
	vec3_t forward;
	vec3_t geomcenter;
	vec3_t geomsize;
	vec3_t left;
	vec3_t origin;
	vec3_t spinvelocity;
	vec3_t up;
	vec3_t velocity;
	vec_t f;
	vec_t length;
	vec_t massval = 1.0f;
	vec_t movelimit;
	vec_t radius;
	vec_t spinlimit;
#ifndef ODE_STATIC
	if (!ode_dll)
		return;
#endif
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.solid);
	solid = (int)val->_float;
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.movetype);
	movetype = (int)val->_float;
	switch(solid)
	{
	case SOLID_BSP:
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.modelindex);
		modelindex = (int)val->_float;
		if (world == &sv.world && modelindex >= 1 && modelindex < MAX_MODELS)
		{
			model = sv.models[modelindex];
			mempool = sv_mempool;
		}
		else if (world == &cl.world && modelindex >= 1 && modelindex < MAX_MODELS)
		{
			model = cl.model_precache[modelindex];
			mempool = cls.levelmempool;
		}
		else
		{
			model = NULL;
			mempool = NULL;
			modelindex = 0;
		}
		if (model)
		{
			VectorCopy(model->normalmins, entmins);
			VectorCopy(model->normalmaxs, entmaxs);
			val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.mass);if (val) massval = val->_float;
		}
		else
		{
			modelindex = 0;
			massval = 1.0f;
		}
		break;
	case SOLID_BBOX:
	//case SOLID_SLIDEBOX:
	case SOLID_CORPSE:
	case SOLID_PHYSICS_BOX:
	case SOLID_PHYSICS_SPHERE:
	case SOLID_PHYSICS_CAPSULE:
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.mins);if (val) VectorCopy(val->vector, entmins);
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.maxs);if (val) VectorCopy(val->vector, entmaxs);
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.mass);if (val) massval = val->_float;
		break;
	default:
		if (ed->priv.server->ode_physics)
			World_Physics_RemoveFromEntity(world, ed);
		return;
	}

	VectorSubtract(entmaxs, entmins, geomsize);
	if (VectorLength2(geomsize) == 0)
	{
		// we don't allow point-size physics objects...
		if (ed->priv.server->ode_physics)
			World_Physics_RemoveFromEntity(world, ed);
		return;
	}

	if (movetype != MOVETYPE_PHYSICS)
		massval = 1.0f;

	// check if we need to create or replace the geom
	if (!ed->priv.server->ode_physics
	 || !VectorCompare(ed->priv.server->ode_mins, entmins)
	 || !VectorCompare(ed->priv.server->ode_maxs, entmaxs)
	 || ed->priv.server->ode_mass != massval
	 || ed->priv.server->ode_modelindex != modelindex)
	{
		World_Physics_RemoveFromEntity(world, ed);
		ed->priv.server->ode_physics = true;
		VectorCopy(entmins, ed->priv.server->ode_mins);
		VectorCopy(entmaxs, ed->priv.server->ode_maxs);
		ed->priv.server->ode_mass = massval;
		ed->priv.server->ode_modelindex = modelindex;
		VectorMAM(0.5f, entmins, 0.5f, entmaxs, geomcenter);
		ed->priv.server->ode_movelimit = min(geomsize[0], min(geomsize[1], geomsize[2]));

		if (massval * geomsize[0] * geomsize[1] * geomsize[2] == 0)
		{
			if (movetype == MOVETYPE_PHYSICS)
				Con_Printf("entity %i (classname %s) .mass * .size_x * .size_y * .size_z == 0\n", PRVM_NUM_FOR_EDICT(ed), PRVM_GetString(PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.classname)->string));
			massval = 1.0f;
			VectorSet(geomsize, 1.0f, 1.0f, 1.0f);
		}

		switch(solid)
		{
		case SOLID_BSP:
			ed->priv.server->ode_offsetmatrix = identitymatrix;
			if (!model)
			{
				Con_Printf("entity %i (classname %s) has no model\n", PRVM_NUM_FOR_EDICT(ed), PRVM_GetString(PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.classname)->string));
				break;
			}
			// add an optimized mesh to the model containing only the SUPERCONTENTS_SOLID surfaces
			if (!model->brush.collisionmesh)
				Mod_CreateCollisionMesh(model);
			if (!model->brush.collisionmesh || !model->brush.collisionmesh->numtriangles)
			{
				Con_Printf("entity %i (classname %s) has no geometry\n", PRVM_NUM_FOR_EDICT(ed), PRVM_GetString(PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.classname)->string));
				break;
			}
			// ODE requires persistent mesh storage, so we need to copy out
			// the data from the model because renderer restarts could free it
			// during the game, additionally we need to flip the triangles...
			// note: ODE does preprocessing of the mesh for culling, removing
			// concave edges, etc., so this is not a lightweight operation
			ed->priv.server->ode_numvertices = numvertices = model->brush.collisionmesh->numverts;
			ed->priv.server->ode_vertex3f = (float *)Mem_Alloc(mempool, numvertices * sizeof(float[3]));
			for (vertexindex = 0, ov = ed->priv.server->ode_vertex3f, iv = model->brush.collisionmesh->vertex3f;vertexindex < numvertices;vertexindex++, ov += 3, iv += 3)
			{
				ov[0] = iv[0] - geomcenter[0];
				ov[1] = iv[1] - geomcenter[1];
				ov[2] = iv[2] - geomcenter[2];
			}
			ed->priv.server->ode_numtriangles = numtriangles = model->brush.collisionmesh->numtriangles;
			ed->priv.server->ode_element3i = (int *)Mem_Alloc(mempool, numtriangles * sizeof(int[3]));
			//memcpy(ed->priv.server->ode_element3i, model->brush.collisionmesh->element3i, ed->priv.server->ode_numtriangles * sizeof(int[3]));
			for (triangleindex = 0, oe = ed->priv.server->ode_element3i, ie = model->brush.collisionmesh->element3i;triangleindex < numtriangles;triangleindex++, oe += 3, ie += 3)
			{
				oe[0] = ie[2];
				oe[1] = ie[1];
				oe[2] = ie[0];
			}
			Matrix4x4_CreateTranslate(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2]);
			// now create the geom
			dataID = dGeomTriMeshDataCreate();
			dGeomTriMeshDataBuildSingle(dataID, (void*)ed->priv.server->ode_vertex3f, sizeof(float[3]), ed->priv.server->ode_numvertices, ed->priv.server->ode_element3i, ed->priv.server->ode_numtriangles*3, sizeof(int[3]));
			ed->priv.server->ode_body = (void *)(body = dBodyCreate(world->physics.ode_world));
			ed->priv.server->ode_geom = (void *)dCreateTriMesh(world->physics.ode_space, dataID, NULL, NULL, NULL);
			dGeomSetBody(ed->priv.server->ode_geom, body);
			dMassSetBoxTotal(&mass, massval, geomsize[0], geomsize[1], geomsize[2]);
			dBodySetMass(body, &mass);
			dBodySetData(body, (void*)ed);
			break;
		case SOLID_BBOX:
		case SOLID_SLIDEBOX:
		case SOLID_CORPSE:
		case SOLID_PHYSICS_BOX:
			Matrix4x4_CreateTranslate(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2]);
			ed->priv.server->ode_body = (void *)(body = dBodyCreate(world->physics.ode_world));
			ed->priv.server->ode_geom = (void *)dCreateBox(world->physics.ode_space, geomsize[0], geomsize[1], geomsize[2]);
			dMassSetBoxTotal(&mass, massval, geomsize[0], geomsize[1], geomsize[2]);
			dGeomSetBody(ed->priv.server->ode_geom, body);
			dBodySetMass(body, &mass);
			dBodySetData(body, (void*)ed);
			break;
		case SOLID_PHYSICS_SPHERE:
			Matrix4x4_CreateTranslate(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2]);
			ed->priv.server->ode_body = (void *)(body = dBodyCreate(world->physics.ode_world));
			ed->priv.server->ode_geom = (void *)dCreateSphere(world->physics.ode_space, geomsize[0] * 0.5f);
			dMassSetSphereTotal(&mass, massval, geomsize[0] * 0.5f);
			dGeomSetBody(ed->priv.server->ode_geom, body);
			dBodySetMass(body, &mass);
			dBodySetData(body, (void*)ed);
			break;
		case SOLID_PHYSICS_CAPSULE:
			axisindex = 0;
			if (geomsize[axisindex] < geomsize[1])
				axisindex = 1;
			if (geomsize[axisindex] < geomsize[2])
				axisindex = 2;
			// the qc gives us 3 axis radius, the longest axis is the capsule
			// axis, since ODE doesn't like this idea we have to create a
			// capsule which uses the standard orientation, and apply a
			// transform to it
			memset(capsulerot, 0, sizeof(capsulerot));
			if (axisindex == 0)
				Matrix4x4_CreateFromQuakeEntity(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2], 0, 0, 90, 1);
			else if (axisindex == 1)
				Matrix4x4_CreateFromQuakeEntity(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2], 90, 0, 0, 1);
			else
				Matrix4x4_CreateFromQuakeEntity(&ed->priv.server->ode_offsetmatrix, geomcenter[0], geomcenter[1], geomcenter[2], 0, 0, 0, 1);
			radius = geomsize[!axisindex] * 0.5f; // any other axis is the radius
			length = geomsize[axisindex] - radius*2;
			// because we want to support more than one axisindex, we have to
			// create a transform, and turn on its cleanup setting (which will
			// cause the child to be destroyed when it is destroyed)
			ed->priv.server->ode_body = (void *)(body = dBodyCreate(world->physics.ode_world));
			ed->priv.server->ode_geom = (void *)dCreateCapsule(world->physics.ode_space, radius, length);
			dMassSetCapsuleTotal(&mass, massval, axisindex+1, radius, length);
			dGeomSetBody(ed->priv.server->ode_geom, body);
			dBodySetMass(body, &mass);
			dBodySetData(body, (void*)ed);
			break;
		default:
			Sys_Error("World_Physics_BodyFromEntity: unrecognized solid value %i was accepted by filter\n", solid);
		}
		Matrix4x4_Invert_Simple(&ed->priv.server->ode_offsetimatrix, &ed->priv.server->ode_offsetmatrix);
	}

	// get current data from entity
	VectorClear(origin);
	VectorClear(forward);
	VectorClear(left);
	VectorClear(up);
	VectorClear(velocity);
	VectorClear(spinvelocity);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.origin);if (val) VectorCopy(val->vector, origin);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_forward);if (val) VectorCopy(val->vector, forward);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_left);if (val) VectorCopy(val->vector, left);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.axis_up);if (val) VectorCopy(val->vector, up);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.velocity);if (val) VectorCopy(val->vector, velocity);
	val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.spinvelocity);if (val) VectorCopy(val->vector, spinvelocity);

	// compatibility for legacy entities
	switch (solid)
	{
	case SOLID_BSP:
		//VectorClear(velocity);
		VectorClear(angles);
		VectorClear(avelocity);
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.angles);if (val) VectorCopy(val->vector, angles);
		val = PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.velocity);if (val) VectorCopy(val->vector, avelocity);
		AngleVectorsFLU(angles, forward, left, up);
		// convert single-axis rotations in avelocity to spinvelocity
		// FIXME: untested math - check signs
		VectorSet(spinvelocity, avelocity[PITCH] * ((float)M_PI / 180.0f), avelocity[ROLL] * ((float)M_PI / 180.0f), avelocity[YAW] * ((float)M_PI / 180.0f));
		break;
	case SOLID_BBOX:
	case SOLID_SLIDEBOX:
	case SOLID_CORPSE:
		//VectorClear(velocity);
		VectorSet(forward, 1, 0, 0);
		VectorSet(left, 0, 1, 0);
		VectorSet(up, 0, 0, 1);
		VectorSet(spinvelocity, 0, 0, 0);
		break;
	}

	// we must prevent NANs...
	test = VectorLength2(origin) + VectorLength2(forward) + VectorLength2(left) + VectorLength2(up) + VectorLength2(velocity) + VectorLength2(spinvelocity);
	if (IS_NAN(test))
	{
		Con_Printf("Fixing NAN values on entity %i : .classname = \"%s\" .origin = '%f %f %f' .axis_forward = '%f %f %f' .axis_left = '%f %f %f' .axis_up = '%f %f %f' .velocity = '%f %f %f' .spinvelocity = '%f %f %f'\n", PRVM_NUM_FOR_EDICT(ed), PRVM_GetString(PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.classname)->string), origin[0], origin[1], origin[2], forward[0], forward[1], forward[2], left[0], left[1], left[2], up[0], up[1], up[2], velocity[0], velocity[1], velocity[2], spinvelocity[0], spinvelocity[1], spinvelocity[2]);
		test = VectorLength2(origin);
		if (IS_NAN(test))
			VectorClear(origin);
		test = VectorLength2(forward) * VectorLength2(left) * VectorLength2(up);
		if (IS_NAN(test))
		{
			VectorSet(forward, 1, 0, 0);
			VectorSet(left, 0, 1, 0);
			VectorSet(up, 0, 0, 1);
		}
		test = VectorLength2(velocity);
		if (IS_NAN(test))
			VectorClear(velocity);
		test = VectorLength2(spinvelocity);
		if (IS_NAN(test))
		{
			VectorClear(spinvelocity);
		}
	}

	// limit movement speed to prevent missed collisions at high speed
	movelimit = ed->priv.server->ode_movelimit * world->physics.ode_movelimit;
	test = VectorLength2(velocity);
	if (test > movelimit*movelimit)
	{
		// scale down linear velocity to the movelimit
		// scale down angular velocity the same amount for consistency
		f = movelimit / sqrt(test);
		VectorScale(velocity, f, velocity);
		VectorScale(spinvelocity, f, spinvelocity);
	}

	// make sure the angular velocity is not exploding
	spinlimit = physics_ode_spinlimit.value;
	test = VectorLength2(spinvelocity);
	if (test > spinlimit)
		VectorClear(spinvelocity);

	// store the values into the physics engine
	body = ed->priv.server->ode_body;
	if (body)
	{
		dVector3 r[3];
		matrix4x4_t entitymatrix;
		matrix4x4_t bodymatrix;
		Matrix4x4_FromVectors(&entitymatrix, forward, left, up, origin);
		Matrix4x4_Concat(&bodymatrix, &entitymatrix, &ed->priv.server->ode_offsetmatrix);
		Matrix4x4_ToVectors(&bodymatrix, forward, left, up, origin);
		r[0][0] = forward[0];
		r[1][0] = forward[1];
		r[2][0] = forward[2];
		r[0][1] = left[0];
		r[1][1] = left[1];
		r[2][1] = left[2];
		r[0][2] = up[0];
		r[1][2] = up[1];
		r[2][2] = up[2];
		dGeomSetBody(ed->priv.server->ode_geom, ed->priv.server->ode_body);
		dBodySetPosition(body, origin[0], origin[1], origin[2]);
		dBodySetRotation(body, r[0]);
		dBodySetLinearVel(body, velocity[0], velocity[1], velocity[2]);
		dBodySetAngularVel(body, spinvelocity[0], spinvelocity[1], spinvelocity[2]);
		// setting body to NULL makes an immovable object
		if (movetype != MOVETYPE_PHYSICS)
			dGeomSetBody(ed->priv.server->ode_geom, 0);
	}
}

#define MAX_CONTACTS 16
static void nearCallback (void *data, dGeomID o1, dGeomID o2)
{
	world_t *world = (world_t *)data;
	dContact contact[MAX_CONTACTS]; // max contacts per collision pair
	dBodyID b1;
	dBodyID b2;
	dJointID c;
	int i;
	int numcontacts;

	if (dGeomIsSpace(o1) || dGeomIsSpace(o2))
	{
		// colliding a space with something
		dSpaceCollide2(o1, o2, data, &nearCallback);
		// Note we do not want to test intersections within a space,
		// only between spaces.
		//if (dGeomIsSpace(o1)) dSpaceCollide(o1, data, &nearCallback);
		//if (dGeomIsSpace(o2)) dSpaceCollide(o2, data, &nearCallback);
		return;
	}

	b1 = dGeomGetBody(o1);
	b2 = dGeomGetBody(o2);

	// at least one object has to be using MOVETYPE_PHYSICS or we just don't care
	if (!b1 && !b2)
		return;

	// exit without doing anything if the two bodies are connected by a joint
	if (b1 && b2 && dAreConnectedExcluding(b1, b2, dJointTypeContact))
		return;

	// generate contact points between the two non-space geoms
	numcontacts = dCollide(o1, o2, MAX_CONTACTS, &(contact[0].geom), sizeof(contact[0]));
	// add these contact points to the simulation
	for (i = 0;i < numcontacts;i++)
	{
		contact[i].surface.mode = (physics_ode_contact_mu.value != -1 ? dContactApprox1 : 0) | (physics_ode_contact_erp.value != -1 ? dContactSoftERP : 0) | (physics_ode_contact_cfm.value != -1 ? dContactSoftCFM : 0);
		contact[i].surface.mu = physics_ode_contact_mu.value;
		contact[i].surface.soft_erp = physics_ode_contact_erp.value;
		contact[i].surface.soft_cfm = physics_ode_contact_cfm.value;
		c = dJointCreateContact(world->physics.ode_world, world->physics.ode_contactgroup, contact + i);
		dJointAttach(c, b1, b2);
	}
}
#endif

void World_Physics_Frame(world_t *world, double frametime, double gravity)
{
#ifdef USEODE
	if (world->physics.ode)
	{
		int i;
		prvm_edict_t *ed;

		// copy physics properties from entities to physics engine
		if (prog)
			for (i = 0, ed = prog->edicts + i;i < prog->num_edicts;i++, ed++)
				if (!prog->edicts[i].priv.required->free)
					World_Physics_Frame_BodyFromEntity(world, ed);

		world->physics.ode_iterations = bound(1, physics_ode_iterationsperframe.integer, 1000);
		world->physics.ode_step = frametime / world->physics.ode_iterations;
		world->physics.ode_movelimit = physics_ode_movelimit.value / world->physics.ode_step;
		for (i = 0;i < world->physics.ode_iterations;i++)
		{
			// set the gravity
			dWorldSetGravity(world->physics.ode_world, 0, 0, -gravity);
			// set the tolerance for closeness of objects
			dWorldSetContactSurfaceLayer(world->physics.ode_world, max(0, physics_ode_contactsurfacelayer.value));

			// run collisions for the current world state, creating JointGroup
			dSpaceCollide(world->physics.ode_space, (void *)world, nearCallback);

			// run physics (move objects, calculate new velocities)
			if (physics_ode_worldquickstep.integer)
			{
				dWorldSetQuickStepNumIterations(world->physics.ode_world, bound(1, physics_ode_worldquickstep_iterations.integer, 200));
				dWorldQuickStep(world->physics.ode_world, world->physics.ode_step);
			}
			else if (physics_ode_worldstepfast.integer)
				dWorldStepFast1(world->physics.ode_world, world->physics.ode_step, bound(1, physics_ode_worldstepfast_iterations.integer, 200));
			else
				dWorldStep(world->physics.ode_world, world->physics.ode_step);

			// clear the JointGroup now that we're done with it
			dJointGroupEmpty(world->physics.ode_contactgroup);
		}

		// copy physics properties from physics engine to entities
		if (prog)
			for (i = 1, ed = prog->edicts + i;i < prog->num_edicts;i++, ed++)
				if (!prog->edicts[i].priv.required->free && PRVM_EDICTFIELDVALUE(ed, prog->fieldoffsets.movetype)->_float == MOVETYPE_PHYSICS)
					World_Physics_Frame_BodyToEntity(world, ed);
	}
#endif
}
