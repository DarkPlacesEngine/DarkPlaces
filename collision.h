
#ifndef COLLISION_H
#define COLLISION_H

typedef struct plane_s
{
	vec3_t	normal;
	float	dist;
}
plane_t;

typedef struct trace_s
{
	// if true, the entire trace was in solid
	int allsolid;
	// if true, the initial point was in solid
	int startsolid;
	// if true, the trace passed through empty somewhere
	int inopen;
	// if true, the trace passed through water somewhere
	int inwater;
	// fraction of the total distance that was traveled before impact
	// (1.0 = did not hit anything)
	double fraction;
	// final position
	double endpos[3];
	// surface normal at impact
	plane_t plane;
	// entity the surface is on
	void *ent;
	// if not zero, treats this value as empty, and all others as solid (impact
	// on content change)
	int thiscontents;
	// the contents at the impact or end point
	int endcontents;
}
trace_t;

void Collision_Init(void);
void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end);

typedef struct colpointf_s
{
	float v[3];
}
colpointf_t;

typedef struct colplanef_s
{
	float normal[3];
	float dist;
}
colplanef_t;

typedef struct colbrushf_s
{
	int numplanes;
	int numpoints;
	colplanef_t *planes;
	colpointf_t *points;
}
colbrushf_t;

colbrushf_t *Collision_AllocBrushFloat(mempool_t *mempool, int numpoints, int numplanes);
void Collision_CalcPlanesForPolygonBrushFloat(colbrushf_t *brush);
colbrushf_t *Collision_AllocBrushFromPermanentPolygonFloat(mempool_t *mempool, int numpoints, float *points);
void Collision_TraceBrushBrushFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end);
void Collision_TraceBrushPolygonFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points);
void Collision_TraceBrushPolygonTransformFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4_t *polygonmatrixstart, const matrix4x4_t *polygonmatrixend);

colbrushf_t *Collision_BrushForBox(const matrix4x4_t *matrix, const vec3_t mins, const vec3_t maxs);

#endif
