
#ifndef CURVES_H
#define CURVES_H

void QuadraticSplineSubdivideFloat(int inpoints, int components, const float *in, int instride, float *out, int outstride);
void QuadraticSplinePatchSubdivideFloatBuffer(int cpwidth, int cpheight, int xlevel, int ylevel, int components, const float *in, float *out);

#endif

