
#ifndef WINDING_H
#define WINDING_H

typedef struct
{
	int numpoints;
	int maxpoints;
	double points[8][3]; // variable sized
}
winding_t;

winding_t *Winding_New(int points);
void Winding_Free(winding_t *w);
winding_t *Winding_NewFromPlane(double normalx, double normaly, double normalz, double dist);
winding_t *Winding_Clip(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, int keepon);
void Winding_Divide(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, winding_t **front, winding_t **back);
void BufWinding_NewFromPlane(winding_t *w, double normalx, double normaly, double normalz, double dist);
void BufWinding_Divide(winding_t *in, double splitnormalx, double splitnormaly, double splitnormalz, double splitdist, winding_t *outfront, int *neededfrontpoints, winding_t *outback, int *neededbackpoints);

#endif

