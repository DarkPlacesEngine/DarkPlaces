
#ifndef CURVES_H
#define CURVES_H

// usage:
// to expand a 5x5 patch to 21x21 vertices (4x4 tesselation), one might use this call:
// Q3PatchSubdivideFloat(3, sizeof(float[3]), outvertices, 5, 5, sizeof(float[3]), patchvertices, 4, 4);
void Q3PatchTesselateFloat(int numcomponents, int outputstride, float *outputvertices, int patchwidth, int patchheight, int inputstride, float *patchvertices, int tesselationwidth, int tesselationheight);
// returns how much tesselation of each segment is needed to remain under tolerance
int Q3PatchTesselationOnX(int patchwidth, int patchheight, int components, const float *in, float tolerance);
// returns how much tesselation of each segment is needed to remain under tolerance
int Q3PatchTesselationOnY(int patchwidth, int patchheight, int components, const float *in, float tolerance);
// calculates elements for a grid of vertices
// (such as those produced by Q3PatchTesselate)
// (note: width and height are the actual vertex size, this produces
//  (width-1)*(height-1)*2 triangles, 3 elements each)
void Q3PatchTriangleElements(int *elements, int width, int height, int firstvertex);

#endif

