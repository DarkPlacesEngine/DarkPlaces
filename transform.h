
#ifndef TRANSFORM_H
#define TRANSFORM_H

extern vec_t softwaretransform_rotatematrix[3][4];
extern vec_t softwaretransform_matrix[3][4];
extern vec_t softwaretransform_invmatrix[3][4];
extern int softwaretransform_complexity;

void softwaretransformidentity (void);
void softwaretransformset (vec3_t origin, vec3_t angles, vec_t scale);
void softwaretransformforentity (entity_render_t *r);

// #defines for speed reasons
#define softwaretransform(in, out)\
{\
	if (softwaretransform_complexity == 0)\
	{\
		VectorCopy(in, out);\
	}\
	else if (softwaretransform_complexity == 1)\
	{\
		out[0] = in[0] + softwaretransform_matrix[0][3];\
		out[1] = in[1] + softwaretransform_matrix[1][3];\
		out[2] = in[2] + softwaretransform_matrix[2][3];\
	}\
	else\
	{\
		out[0] = DotProduct(in, softwaretransform_matrix[0]) + softwaretransform_matrix[0][3];\
		out[1] = DotProduct(in, softwaretransform_matrix[1]) + softwaretransform_matrix[1][3];\
		out[2] = DotProduct(in, softwaretransform_matrix[2]) + softwaretransform_matrix[2][3];\
	}\
}

#define softwaretransformdirection(in, out)\
{\
	if (softwaretransform_complexity == 2)\
	{\
		out[0] = DotProduct(in, softwaretransform_rotatematrix[0]);\
		out[1] = DotProduct(in, softwaretransform_rotatematrix[1]);\
		out[2] = DotProduct(in, softwaretransform_rotatematrix[2]);\
	}\
	else\
		VectorCopy(in, out);\
}

#define softwareuntransform(in, out)\
{\
	if (softwaretransform_complexity == 0)\
	{\
		VectorCopy(in, out);\
	}\
	else if (softwaretransform_complexity == 1)\
	{\
		out[0] = in[0] - softwaretransform_invmatrix[0][3];\
		out[1] = in[1] - softwaretransform_invmatrix[1][3];\
		out[2] = in[2] - softwaretransform_invmatrix[2][3];\
	}\
	else\
	{\
		vec3_t soft_v;\
		soft_v[0] = in[0] - softwaretransform_invmatrix[0][3];\
		soft_v[1] = in[1] - softwaretransform_invmatrix[1][3];\
		soft_v[2] = in[2] - softwaretransform_invmatrix[2][3];\
		out[0] = DotProduct(soft_v, softwaretransform_invmatrix[0]);\
		out[1] = DotProduct(soft_v, softwaretransform_invmatrix[1]);\
		out[2] = DotProduct(soft_v, softwaretransform_invmatrix[2]);\
	}\
}

#endif

