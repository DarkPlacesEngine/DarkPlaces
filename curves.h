
#ifndef CURVES_H
#define CURVES_H

void QuadraticSplineSubdivideFloat(int inpoints, int components, const float *in, int instride, float *out, int outstride);
void QuadraticSplinePatchSubdivideFloatBuffer(int cpwidth, int cpheight, int xlevel, int ylevel, int components, const float *in, float *out);
float QuadraticSplinePatchLargestDeviationOnX(int cpwidth, int cpheight, int components, const float *in);
float QuadraticSplinePatchLargestDeviationOnY(int cpwidth, int cpheight, int components, const float *in);
int QuadraticSplinePatchSubdivisionLevelForDeviation(float deviation, float level1tolerance, int levellimit);
int QuadraticSplinePatchSubdivisionLevelOnX(int cpwidth, int cpheight, int components, const float *in, float level1tolerance, int levellimit);
int QuadraticSplinePatchSubdivisionLevelOnY(int cpwidth, int cpheight, int components, const float *in, float level1tolerance, int levellimit);

#endif

