#include "quakedef.h"

dlight_t cl_dlights[MAX_DLIGHTS];

/*
===============
CL_AllocDlight

===============
*/
void CL_AllocDlight (entity_render_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime)
{
	int		i;
	dlight_t	*dl;

// first look for an exact key match
	if (ent)
	{
		dl = cl_dlights;
		for (i = 0;i < MAX_DLIGHTS;i++, dl++)
			if (dl->ent == ent)
				goto dlightsetup;
	}

// then look for anything else
	dl = cl_dlights;
	for (i = 0;i < MAX_DLIGHTS;i++, dl++)
		if (!dl->radius)
			goto dlightsetup;

	// unable to find one
	return;

dlightsetup:
	memset (dl, 0, sizeof(*dl));
	dl->ent = ent;
	VectorCopy(org, dl->origin);
	dl->radius = radius;
	dl->color[0] = red;
	dl->color[1] = green;
	dl->color[2] = blue;
	dl->decay = decay;
	if (lifetime)
		dl->die = cl.time + lifetime;
	else
		dl->die = 0;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;
	float		time;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			continue;
		}

		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


