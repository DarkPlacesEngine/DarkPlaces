
typedef struct
{
	int frame;
	float lerp;
}
frameblend_t;

void R_LerpAnimation(model_t *mod, int frame1, int frame2, double frame1start, double frame2start, double framelerp, frameblend_t *blend);
