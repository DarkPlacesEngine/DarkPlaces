// for physics functions shared by the client and server

#include "phys.h"

#include "quakedef.h"
#include "cl_collision.h"


int PHYS_NudgeOutOfSolid(prvm_prog_t *prog, prvm_edict_t *ent)
{
	int bump, pass;
	trace_t stucktrace;
	vec3_t stuckorigin;
	vec3_t stuckmins, stuckmaxs;
	vec_t separation;
	model_t *worldmodel;

	if (prog == SVVM_prog)
	{
		worldmodel = sv.worldmodel;
		separation = sv_gameplayfix_nudgeoutofsolid_separation.value;
	}
	else if (prog == CLVM_prog)
	{
		worldmodel = cl.worldmodel;
		separation = cl_gameplayfix_nudgeoutofsolid_separation.value;
	}
	else
		Sys_Error("PHYS_NudgeOutOfSolid: cannot be called from %s VM\n", prog->name);

	VectorCopy(PRVM_serveredictvector(ent, mins), stuckmins);
	VectorCopy(PRVM_serveredictvector(ent, maxs), stuckmaxs);
	if (worldmodel && worldmodel->brushq1.numclipnodes)
		separation = 0.0f; // when using hulls, it can not be enlarged
	else
	{
		stuckmins[0] -= separation;
		stuckmins[1] -= separation;
		stuckmins[2] -= separation;
		stuckmaxs[0] += separation;
		stuckmaxs[1] += separation;
		stuckmaxs[2] += separation;
	}

	// first pass we try to get it out of brush entities
	// second pass we try to get it out of world only (can't win them all)
	for (pass = 0;pass < 2;pass++)
	{
		VectorCopy(PRVM_serveredictvector(ent, origin), stuckorigin);
		for (bump = 0;bump < 10;bump++)
		{
			if (prog == SVVM_prog) // TODO: can we refactor to use a shared TraceBox or at least a func ptr for these cases?
				stucktrace = SV_TraceBox(stuckorigin, stuckmins, stuckmaxs, stuckorigin, pass ? MOVE_WORLDONLY : MOVE_NOMONSTERS, ent, SV_GenericHitSuperContentsMask(ent), 0, 0, collision_extendmovelength.value);
			else
				stucktrace = CL_TraceBox(stuckorigin, stuckmins, stuckmaxs, stuckorigin, pass ? MOVE_WORLDONLY : MOVE_NOMONSTERS, ent, SV_GenericHitSuperContentsMask(ent), 0, 0, collision_extendmovelength.value, pass ? false : true, false, NULL, false);

			// Separation compared here to ensure a good location will be recognised reliably.
			if (-stucktrace.startdepth <= separation
			|| (!stucktrace.bmodelstartsolid && !stucktrace.worldstartsolid)
			|| (pass && !stucktrace.worldstartsolid))
			{
				// found a good location, use it
				VectorCopy(stuckorigin, PRVM_serveredictvector(ent, origin));
				return bump || pass ? 1 : -1; // -1 means it wasn't stuck
			}

			VectorMA(stuckorigin, -stucktrace.startdepth, stucktrace.startdepthnormal, stuckorigin);
		}
	}
	return 0;
}
