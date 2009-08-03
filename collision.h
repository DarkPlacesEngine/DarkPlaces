
#ifndef COLLISION_H
#define COLLISION_H

typedef struct plane_s
{
	vec3_t	normal;
	float	dist;
}
plane_t;

struct texture_s;
typedef struct trace_s
{
	// if true, the entire trace was in solid (see hitsupercontentsmask)
	int allsolid;
	// if true, the initial point was in solid (see hitsupercontentsmask)
	int startsolid;
	// this is set to true in world.c if startsolid was set in a trace against a SOLID_BSP entity, in other words this is true if the entity is stuck in a door or wall, but not if stuck in another normal entity
	int bmodelstartsolid;
	// if true, the trace passed through empty somewhere
	// (set only by Q1BSP tracing)
	int inopen;
	// if true, the trace passed through water/slime/lava somewhere
	// (set only by Q1BSP tracing)
	int inwater;
	// fraction of the total distance that was traveled before impact
	// (1.0 = did not hit anything)
	double fraction;
	// like fraction but is not nudged away from the surface (better for
	// comparisons between two trace structs, as only one nudge for the final
	// result is ever needed)
	double realfraction;
	// final position of the trace (simply a point between start and end)
	double endpos[3];
	// surface normal at impact (not really correct for edge collisions)
	plane_t plane;
	// entity the surface is on
	// (not set by trace functions, only by physics)
	void *ent;
	// which SUPERCONTENTS bits to collide with, I.E. to consider solid
	// (this also affects startsolid/allsolid)
	int hitsupercontentsmask;
	// the supercontents mask at the start point
	int startsupercontents;
	// the supercontents of the impacted surface
	int hitsupercontents;
	// the q3 surfaceflags of the impacted surface
	int hitq3surfaceflags;
	// the texture of the impacted surface
	struct texture_s *hittexture;
	// initially false, set when the start leaf is found
	// (set only by Q1BSP tracing and entity box tracing)
	int startfound;
}
trace_t;

void Collision_Init(void);
void Collision_ClipTrace_Box(trace_t *trace, const vec3_t cmins, const vec3_t cmaxs, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask, int boxsupercontents, int boxq3surfaceflags, texture_t *boxtexture);

typedef struct colpointf_s
{
	float v[3];
}
colpointf_t;

typedef struct colplanef_s
{
	struct texture_s *texture;
	int q3surfaceflags;
	float normal[3];
	float dist;
}
colplanef_t;

typedef struct colbrushf_s
{
	// the content flags of this brush
	int supercontents;
	// the number of bounding planes on this brush
	int numplanes;
	// the number of corner points on this brush
	int numpoints;
	// the number of renderable triangles on this brush
	int numtriangles;
	// array of bounding planes on this brush
	colplanef_t *planes;
	// array of corner points on this brush
	colpointf_t *points;
	// renderable triangles, as int[3] elements indexing the points
	int *elements;
	// used to avoid tracing against the same brush more than once
	int markframe;
	// culling box
	vec3_t mins;
	vec3_t maxs;
}
colbrushf_t;

void Collision_CalcPlanesForPolygonBrushFloat(colbrushf_t *brush);
colbrushf_t *Collision_AllocBrushFromPermanentPolygonFloat(mempool_t *mempool, int numpoints, float *points, int supercontents);
colbrushf_t *Collision_NewBrushFromPlanes(mempool_t *mempool, int numoriginalplanes, const colplanef_t *originalplanes, int supercontents);
void Collision_TraceBrushBrushFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end);
void Collision_TraceBrushPolygonFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, int supercontents);
void Collision_TraceBrushTriangleMeshFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numtriangles, const int *element3i, const float *vertex3f, int stride, float *bbox6f, int supercontents, int q3surfaceflags, texture_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs);
void Collision_TraceLineBrushFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, const colbrushf_t *thatbrush_start, const colbrushf_t *thatbrush_end);
void Collision_TraceLinePolygonFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numpoints, const float *points, int supercontents);
void Collision_TraceLineTriangleMeshFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numtriangles, const int *element3i, const float *vertex3f, int stride, float *bbox6f, int supercontents, int q3surfaceflags, texture_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs);
void Collision_TracePointBrushFloat(trace_t *trace, const vec3_t point, const colbrushf_t *thatbrush);
qboolean Collision_PointInsideBrushFloat(const vec3_t point, const colbrushf_t *brush);

void Collision_TraceBrushPolygonTransformFloat(trace_t *trace, const colbrushf_t *thisbrush_start, const colbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4_t *polygonmatrixstart, const matrix4x4_t *polygonmatrixend, int supercontents, int q3surfaceflags, texture_t *texture);

colbrushf_t *Collision_BrushForBox(const matrix4x4_t *matrix, const vec3_t mins, const vec3_t maxs, int supercontents, int q3surfaceflags, texture_t *texture);

void Collision_BoundingBoxOfBrushTraceSegment(const colbrushf_t *start, const colbrushf_t *end, vec3_t mins, vec3_t maxs, float startfrac, float endfrac);

float Collision_ClipTrace_Line_Sphere(double *linestart, double *lineend, double *sphereorigin, double sphereradius, double *impactpoint, double *impactnormal);
void Collision_TraceLineTriangleFloat(trace_t *trace, const vec3_t linestart, const vec3_t lineend, const float *point0, const float *point1, const float *point2, int supercontents, int q3surfaceflags, texture_t *texture);

// traces a box move against a single entity
// mins and maxs are relative
//
// if the entire move stays in a single solid brush, trace.allsolid will be set
//
// if the starting point is in a solid, it will be allowed to move out to an
// open area, and trace.startsolid will be set
//
// type is one of the MOVE_ values such as MOVE_NOMONSTERS which skips box
// entities, only colliding with SOLID_BSP entities (doors, lifts)
//
// passedict is excluded from clipping checks
void Collision_ClipToGenericEntity(trace_t *trace, dp_model_t *model, int frame, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4_t *matrix, matrix4x4_t *inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontentsmask);
// like above but does not do a transform and does nothing if model is NULL
void Collision_ClipToWorld(trace_t *trace, dp_model_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontents);
// combines data from two traces:
// merges contents flags, startsolid, allsolid, inwater
// updates fraction, endpos, plane and surface info if new fraction is shorter
void Collision_CombineTraces(trace_t *cliptrace, const trace_t *trace, void *touch, qboolean isbmodel);

// shorten a trace by the given factor
void Collision_ShortenTrace(trace_t *trace, float shorten_factor, const vec3_t end);

// this enables rather large debugging spew!
// settings:
// 0 = no spew
// 1 = spew trace calls if something odd is happening
// 2 = spew trace calls always
// 3 = spew detailed trace flow (bsp tree recursion info)
#define COLLISIONPARANOID 0

// make every trace 1qu longer, and shorten the result, to work around a stupid bug somewhere
#define COLLISION_STUPID_TRACE_ENDPOS_IN_SOLID_WORKAROUND


#endif
