#include "mod_skeletal_animatevertices_generic.h"

typedef struct
{
	float f[12];
}
float12_t;

void Mod_Skeletal_AnimateVertices_Generic(const dp_model_t * RESTRICT model, const frameblend_t * RESTRICT frameblend, const skeleton_t *skeleton, float * RESTRICT vertex3f, float * RESTRICT normal3f, float * RESTRICT svector3f, float * RESTRICT tvector3f)
{
	// vertex weighted skeletal
	int i, k;
	int blends;
	float12_t *bonepose;
	float12_t *boneposerelative;
	float m[12];
	const blendweights_t * RESTRICT weights;

	if (!model->surfmesh.num_vertices)
		return;

	//unsigned long long ts = rdtsc();
	bonepose = (float12_t *) Mod_Skeletal_AnimateVertices_AllocBuffers(sizeof(float12_t) * (model->num_bones*2 + model->surfmesh.num_blends));
	boneposerelative = bonepose + model->num_bones;

	if (skeleton && !skeleton->relativetransforms)
		skeleton = NULL;

	// interpolate matrices
	if (skeleton)
	{
		for (i = 0;i < model->num_bones;i++)
		{
			Matrix4x4_ToArray12FloatD3D(&skeleton->relativetransforms[i], m);
			if (model->data_bones[i].parent >= 0)
				R_ConcatTransforms(bonepose[model->data_bones[i].parent].f, m, bonepose[i].f);
			else
				memcpy(bonepose[i].f, m, sizeof(m));

			// create a relative deformation matrix to describe displacement
			// from the base mesh, which is used by the actual weighting
			R_ConcatTransforms(bonepose[i].f, model->data_baseboneposeinverse + i * 12, boneposerelative[i].f);
		}
	}
	else
	{
		float originscale = model->num_posescale;
		float x,y,z,w,lerp;
		const short * RESTRICT pose6s;

		for (i = 0;i < model->num_bones;i++)
		{
			memset(m, 0, sizeof(m));
			for (blends = 0;blends < MAX_FRAMEBLENDS && frameblend[blends].lerp > 0;blends++)
			{
				pose6s = model->data_poses6s + 6 * (frameblend[blends].subframe * model->num_bones + i);
				lerp = frameblend[blends].lerp;
				x = pose6s[3] * (1.0f / 32767.0f);
				y = pose6s[4] * (1.0f / 32767.0f);
				z = pose6s[5] * (1.0f / 32767.0f);
				w = 1.0f - (x*x+y*y+z*z);
				w = w > 0.0f ? -sqrt(w) : 0.0f;
				m[ 0] += (1-2*(y*y+z*z)) * lerp;
				m[ 1] += (  2*(x*y-z*w)) * lerp;
				m[ 2] += (  2*(x*z+y*w)) * lerp;
				m[ 3] += (pose6s[0] * originscale) * lerp;
				m[ 4] += (  2*(x*y+z*w)) * lerp;
				m[ 5] += (1-2*(x*x+z*z)) * lerp;
				m[ 6] += (  2*(y*z-x*w)) * lerp;
				m[ 7] += (pose6s[1] * originscale) * lerp;
				m[ 8] += (  2*(x*z-y*w)) * lerp;
				m[ 9] += (  2*(y*z+x*w)) * lerp;
				m[10] += (1-2*(x*x+y*y)) * lerp;
				m[11] += (pose6s[2] * originscale) * lerp;
			}
			VectorNormalize(m	);
			VectorNormalize(m + 4);
			VectorNormalize(m + 8);
			if (i == r_skeletal_debugbone.integer)
				m[r_skeletal_debugbonecomponent.integer % 12] += r_skeletal_debugbonevalue.value;
			m[3] *= r_skeletal_debugtranslatex.value;
			m[7] *= r_skeletal_debugtranslatey.value;
			m[11] *= r_skeletal_debugtranslatez.value;
			if (model->data_bones[i].parent >= 0)
				R_ConcatTransforms(bonepose[model->data_bones[i].parent].f, m, bonepose[i].f);
			else
				memcpy(bonepose[i].f, m, sizeof(m));
			// create a relative deformation matrix to describe displacement
			// from the base mesh, which is used by the actual weighting
			R_ConcatTransforms(bonepose[i].f, model->data_baseboneposeinverse + i * 12, boneposerelative[i].f);
		}
	}

	// generate matrices for all blend combinations
	weights = model->surfmesh.data_blendweights;
	for (i = 0;i < model->surfmesh.num_blends;i++, weights++)
	{
		float * RESTRICT b = boneposerelative[model->num_bones + i].f;
		const float * RESTRICT m = boneposerelative[weights->index[0]].f;
		float f = weights->influence[0] * (1.0f / 255.0f);
		b[ 0] = f*m[ 0]; b[ 1] = f*m[ 1]; b[ 2] = f*m[ 2]; b[ 3] = f*m[ 3];
		b[ 4] = f*m[ 4]; b[ 5] = f*m[ 5]; b[ 6] = f*m[ 6]; b[ 7] = f*m[ 7];
		b[ 8] = f*m[ 8]; b[ 9] = f*m[ 9]; b[10] = f*m[10]; b[11] = f*m[11];
		for (k = 1;k < 4 && weights->influence[k];k++)
		{
			m = boneposerelative[weights->index[k]].f;
			f = weights->influence[k] * (1.0f / 255.0f);
			b[ 0] += f*m[ 0]; b[ 1] += f*m[ 1]; b[ 2] += f*m[ 2]; b[ 3] += f*m[ 3];
			b[ 4] += f*m[ 4]; b[ 5] += f*m[ 5]; b[ 6] += f*m[ 6]; b[ 7] += f*m[ 7];
			b[ 8] += f*m[ 8]; b[ 9] += f*m[ 9]; b[10] += f*m[10]; b[11] += f*m[11];
		}
	}

#define LOAD_MATRIX_SCALAR() const float * RESTRICT m = boneposerelative[*b].f

#define LOAD_MATRIX3() \
	LOAD_MATRIX_SCALAR()
#define LOAD_MATRIX4() \
	LOAD_MATRIX_SCALAR()

#define TRANSFORM_POSITION_SCALAR(in, out) \
	(out)[0] = ((in)[0] * m[0] + (in)[1] * m[1] + (in)[2] * m[ 2] + m[3]); \
	(out)[1] = ((in)[0] * m[4] + (in)[1] * m[5] + (in)[2] * m[ 6] + m[7]); \
	(out)[2] = ((in)[0] * m[8] + (in)[1] * m[9] + (in)[2] * m[10] + m[11]);
#define TRANSFORM_VECTOR_SCALAR(in, out) \
	(out)[0] = ((in)[0] * m[0] + (in)[1] * m[1] + (in)[2] * m[ 2]); \
	(out)[1] = ((in)[0] * m[4] + (in)[1] * m[5] + (in)[2] * m[ 6]); \
	(out)[2] = ((in)[0] * m[8] + (in)[1] * m[9] + (in)[2] * m[10]);

#define TRANSFORM_POSITION(in, out) \
	TRANSFORM_POSITION_SCALAR(in, out)
#define TRANSFORM_VECTOR(in, out) \
	TRANSFORM_VECTOR_SCALAR(in, out)

	// transform vertex attributes by blended matrices
	if (vertex3f)
	{
		const float * RESTRICT v = model->surfmesh.data_vertex3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		// special case common combinations of attributes to avoid repeated loading of matrices
		if (normal3f)
		{
			const float * RESTRICT n = model->surfmesh.data_normal3f;
			if (svector3f && tvector3f)
			{
				const float * RESTRICT sv = model->surfmesh.data_svector3f;
				const float * RESTRICT tv = model->surfmesh.data_tvector3f;

				// Note that for SSE each iteration stores one element past end, so we break one vertex short
				// and handle that with scalars in that case
				for (i = 0; i < model->surfmesh.num_vertices; i++, v += 3, n += 3, sv += 3, tv += 3, b++,
						vertex3f += 3, normal3f += 3, svector3f += 3, tvector3f += 3)
				{
					LOAD_MATRIX4();
					TRANSFORM_POSITION(v, vertex3f);
					TRANSFORM_VECTOR(n, normal3f);
					TRANSFORM_VECTOR(sv, svector3f);
					TRANSFORM_VECTOR(tv, tvector3f);
				}

				return;
			}

			for (i = 0;i < model->surfmesh.num_vertices; i++, v += 3, n += 3, b++, vertex3f += 3, normal3f += 3)
			{
				LOAD_MATRIX4();
				TRANSFORM_POSITION(v, vertex3f);
				TRANSFORM_VECTOR(n, normal3f);
			}
		}
		else
		{
			for (i = 0;i < model->surfmesh.num_vertices; i++, v += 3, b++, vertex3f += 3)
			{
				LOAD_MATRIX4();
				TRANSFORM_POSITION(v, vertex3f);
			}
		}
	}

	else if (normal3f)
	{
		const float * RESTRICT n = model->surfmesh.data_normal3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < model->surfmesh.num_vertices; i++, n += 3, b++, normal3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(n, normal3f);
		}
	}

	if (svector3f)
	{
		const float * RESTRICT sv = model->surfmesh.data_svector3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < model->surfmesh.num_vertices; i++, sv += 3, b++, svector3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(sv, svector3f);
		}
	}

	if (tvector3f)
	{
		const float * RESTRICT tv = model->surfmesh.data_tvector3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < model->surfmesh.num_vertices; i++, tv += 3, b++, tvector3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(tv, tvector3f);
		}
	}
}
