
#ifndef CL_LIGHT_H
#define CL_LIGHT_H

// LordHavoc: 256 dynamic lights
#define	MAX_DLIGHTS		256
typedef struct
{
	// location
	vec3_t	origin;
	// stop lighting after this time
	float	die;
	// color of light
	vec3_t	color;
	// brightness (not really radius anymore)
	float	radius;
	// drop this each second
	float	decay;
	// the entity that spawned this light (can be NULL if it will never be replaced)
	entity_render_t *ent;
}
dlight_t;

// LordHavoc: this affects the lighting scale of the whole game
#define LIGHTOFFSET 1024.0f

extern dlight_t cl_dlights[MAX_DLIGHTS];

extern void CL_AllocDlight (entity_render_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime);
extern void CL_DecayLights (void);

#endif

