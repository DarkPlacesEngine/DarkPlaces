
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
#define LIGHTOFFSET 16384.0f
#define LIGHTSCALE 4.0f
