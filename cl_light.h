
// LordHavoc: 256 dynamic lights
#define	MAX_DLIGHTS		256
typedef struct
{
	vec3_t	origin;
	float	radius;
	float	die;			// stop lighting after this time
	float	decay;			// drop this each second
	entity_render_t *ent;	// the entity that spawned this light (can be NULL if it will never be replaced)
	vec3_t	color;			// LordHavoc: colored lighting
} dlight_t;

// LordHavoc: this affects the lighting scale of the whole game
#define LIGHTOFFSET 4096.0f

extern	dlight_t		cl_dlights[MAX_DLIGHTS];

extern void CL_AllocDlight (entity_render_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime);
extern void CL_DecayLights (void);

