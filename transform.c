
#include "quakedef.h"

vec_t softwaretransform_rotatematrix[3][4];
vec_t softwaretransform_matrix[3][4];
vec_t softwaretransform_invmatrix[3][4];
int softwaretransform_complexity;

vec_t softwaretransform_identitymatrix[3][4] =
{
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0}
};

void softwaretransformidentity(void)
{
	memcpy(softwaretransform_rotatematrix, softwaretransform_identitymatrix, sizeof(vec_t[3][4]));
	memcpy(softwaretransform_matrix      , softwaretransform_identitymatrix, sizeof(vec_t[3][4]));
	memcpy(softwaretransform_invmatrix   , softwaretransform_identitymatrix, sizeof(vec_t[3][4]));
	softwaretransform_complexity = 0;
}

void softwaretransformset (vec3_t origin, vec3_t angles, vec_t scale)
{
	float invscale;
	invscale = 1.0f / scale;
	if (scale == 0)
		Host_Error("softwaretransformset: 0 scale\n");

	AngleMatrix(angles, origin, softwaretransform_rotatematrix);

	softwaretransform_matrix[0][0] = softwaretransform_rotatematrix[0][0] * scale;
	softwaretransform_matrix[0][1] = softwaretransform_rotatematrix[0][1] * scale;
	softwaretransform_matrix[0][2] = softwaretransform_rotatematrix[0][2] * scale;
	softwaretransform_matrix[1][0] = softwaretransform_rotatematrix[1][0] * scale;
	softwaretransform_matrix[1][1] = softwaretransform_rotatematrix[1][1] * scale;
	softwaretransform_matrix[1][2] = softwaretransform_rotatematrix[1][2] * scale;
	softwaretransform_matrix[2][0] = softwaretransform_rotatematrix[2][0] * scale;
	softwaretransform_matrix[2][1] = softwaretransform_rotatematrix[2][1] * scale;
	softwaretransform_matrix[2][2] = softwaretransform_rotatematrix[2][2] * scale;
	softwaretransform_matrix[0][3] = softwaretransform_rotatematrix[0][3];
	softwaretransform_matrix[1][3] = softwaretransform_rotatematrix[1][3];
	softwaretransform_matrix[2][3] = softwaretransform_rotatematrix[2][3];

	softwaretransform_invmatrix[0][0] = softwaretransform_rotatematrix[0][0] * invscale;
	softwaretransform_invmatrix[0][1] = softwaretransform_rotatematrix[1][0] * invscale;
	softwaretransform_invmatrix[0][2] = softwaretransform_rotatematrix[2][0] * invscale;
	softwaretransform_invmatrix[1][0] = softwaretransform_rotatematrix[0][1] * invscale;
	softwaretransform_invmatrix[1][1] = softwaretransform_rotatematrix[1][1] * invscale;
	softwaretransform_invmatrix[1][2] = softwaretransform_rotatematrix[2][1] * invscale;
	softwaretransform_invmatrix[2][0] = softwaretransform_rotatematrix[0][2] * invscale;
	softwaretransform_invmatrix[2][1] = softwaretransform_rotatematrix[1][2] * invscale;
	softwaretransform_invmatrix[2][2] = softwaretransform_rotatematrix[2][2] * invscale;
	softwaretransform_invmatrix[0][3] = softwaretransform_rotatematrix[0][3];
	softwaretransform_invmatrix[1][3] = softwaretransform_rotatematrix[1][3];
	softwaretransform_invmatrix[2][3] = softwaretransform_rotatematrix[2][3];

	// choose transform mode
	if (softwaretransform_matrix[0][0] != 1 || softwaretransform_matrix[0][1] != 0 || softwaretransform_matrix[0][2] != 0
	 || softwaretransform_matrix[1][0] != 0 || softwaretransform_matrix[1][1] != 1 || softwaretransform_matrix[1][2] != 0
	 || softwaretransform_matrix[2][0] != 0 || softwaretransform_matrix[2][1] != 0 || softwaretransform_matrix[2][2] != 1)
	 	softwaretransform_complexity = 2;
	else if (softwaretransform_matrix[0][3] != 0 || softwaretransform_matrix[1][3] != 0 || softwaretransform_matrix[2][3] != 0)
		softwaretransform_complexity = 1;
	else
		softwaretransform_complexity = 0;
}

void softwaretransformforentity (entity_render_t *r)
{
	vec3_t angles;
	if (r->model->type == mod_brush)
	{
		angles[0] = r->angles[0];
		angles[1] = r->angles[1];
		angles[2] = r->angles[2];
		softwaretransformset(r->origin, angles, r->scale);
	}
	else
	{
		angles[0] = -r->angles[0];
		angles[1] = r->angles[1];
		angles[2] = r->angles[2];
		softwaretransformset(r->origin, angles, r->scale);
	}
}

