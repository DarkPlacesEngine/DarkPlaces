
void R_Clip_StartFrame(void);
void R_Clip_EndFrame(void);
//int R_Clip_Polygon(float *inv, int numpoints, int stride, int solid);
//int R_Clip_Portal(float *inv, int numpoints, int stride);
// This takes a polygon in the form of vec_t triplets, at a caller specified
// stride (structure size), it can be solid (obscures other geometry) or not,
// a callback and two parameters must be provided (will be called if it is
// determined visible), and a plane can be provided (this also indicates that
// the surface is not a backface, so if a plane is provided, the caller must
// check that it is not a backface, it is not likely to be fatal if the caller
// does not, but it is inadvisable)
void R_Clip_AddPolygon (vec_t *points, int numverts, int stride, int solid, void (*callback)(void *nativedata, void *nativedata2), void *nativedata, void *nativedata2, tinyplane_t *polyplane);
void R_Clip_AddBox(float *mins, float *maxs, void (*callback)(void *nativedata, void *nativedata2), void *nativedata, void *nativedata2);
