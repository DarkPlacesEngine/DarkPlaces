
#include "quakedef.h"

void ClearStateToDefault(entity_state_t *s)
{
	s->time = 0;
	VectorClear(s->origin);
	VectorClear(s->angles);
	s->effects = 0;
	s->modelindex = 0;
	s->frame = 0;
	s->colormap = 0;
	s->skin = 0;
	s->alpha = 255;
	s->scale = 16;
	s->glowsize = 0;
	s->glowcolor = 254;
	s->flags = 0;
	s->active = 0;
}

