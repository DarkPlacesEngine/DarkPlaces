
#ifndef CURVES_H
#define CURVES_H

void QuadraticBSplineSubdivideFloat(int inpoints, int components, const float *in, int instride, float *out, int outstride);
void QuadraticBSplinePatchSubdivideFloatBuffer(int cpwidth, int cpheight, int xlevel, int ylevel, int components, const float *in, float *out);
float QuadraticBSplinePatchLargestDeviationOnX(int cpwidth, int cpheight, int components, const float *in);
float QuadraticBSplinePatchLargestDeviationOnY(int cpwidth, int cpheight, int components, const float *in);
int QuadraticBSplinePatchSubdivisionLevelForDeviation(float deviation, float level1tolerance, int levellimit);
int QuadraticBSplinePatchSubdivisionLevelOnX(int cpwidth, int cpheight, int components, const float *in, float level1tolerance, int levellimit);
int QuadraticBSplinePatchSubdivisionLevelOnY(int cpwidth, int cpheight, int components, const float *in, float level1tolerance, int levellimit);

#endif

