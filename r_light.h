
// LordHavoc: 256 dynamic lights
#define	MAX_DLIGHTS		256
typedef struct
{
	vec3_t	origin;
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	int		key;
	vec3_t	color;				// LordHavoc: colored lighting
	qboolean	dark;			// subtracts light instead of adding
} dlight_t;

// LordHavoc: this affects the lighting scale of the whole game
#define LIGHTOFFSET 4096.0f

extern void R_CompleteLightPoint (vec3_t color, vec3_t p);
extern void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
extern void R_DynamicLightPointNoMask(vec3_t color, vec3_t org);
extern void R_LightPoint (vec3_t color, vec3_t p);
