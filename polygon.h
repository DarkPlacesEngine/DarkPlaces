
#ifndef POLYGON_H
#define POLYGON_H

/*
Polygon clipping routines written by Forest Hale and placed into public domain.
*/

void PolygonF_QuadForPlane(float *outpoints, float planenormalx, float planenormaly, float planenormalz, float planedist, float quadsize);
void PolygonD_QuadForPlane(double *outpoints, double planenormalx, double planenormaly, double planenormalz, double planedist, double quadsize);
void PolygonF_Divide(unsigned int innumpoints, const float *inpoints, float planenormalx, float planenormaly, float planenormalz, float planedist, float epsilon, unsigned int outfrontmaxpoints, float *outfrontpoints, unsigned int *neededfrontpoints, unsigned int outbackmaxpoints, float *outbackpoints, unsigned int *neededbackpoints);
void PolygonD_Divide(unsigned int innumpoints, const double *inpoints, double planenormalx, double planenormaly, double planenormalz, double planedist, double epsilon, unsigned int outfrontmaxpoints, double *outfrontpoints, unsigned int *neededfrontpoints, unsigned int outbackmaxpoints, double *outbackpoints, unsigned int *neededbackpoints);

#endif
