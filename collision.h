
#ifndef COLLISION_H
#define COLLISION_H

typedef struct
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct
{
	// if true, the entire trace was in solid
	qboolean	allsolid;
	// if true, the initial point was in solid
	qboolean	startsolid;
	// if true, the trace passed through empty somewhere
	qboolean	inopen;
	// if true, the trace passed through water somewhere
	qboolean	inwater;
	// fraction of the total distance that was traveled before impact
	// (1.0 = did not hit anything)
	double		fraction;
	// final position
	double		endpos[3];
	// surface normal at impact
	plane_t		plane;
	// entity the surface is on
	void		*ent;
	// if not zero, treats this value as empty, and all others as solid (impact
	// on content change)
	int			startcontents;
	// the contents that was hit at the end or impact point
	int			endcontents;
}
trace_t;

void Collision_RoundUpToHullSize(const model_t *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs);
void Collision_Init (void);
void Collision_ClipTrace (trace_t *trace, const void *cent, const model_t *cmodel, const vec3_t corigin, const vec3_t cangles, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end);

#endif
