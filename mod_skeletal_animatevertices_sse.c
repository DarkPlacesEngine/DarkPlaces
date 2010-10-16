#include "mod_skeletal_animatevertices_sse.h"

#ifdef SSE_POSSIBLE

#ifdef MATRIX4x4_OPENGLORIENTATION
#error "SSE skeletal requires D3D matrix layout"
#endif

#include <xmmintrin.h>

void Mod_Skeletal_AnimateVertices_SSE(const dp_model_t * RESTRICT model, const frameblend_t * RESTRICT frameblend, const skeleton_t *skeleton, float * RESTRICT vertex3f, float * RESTRICT normal3f, float * RESTRICT svector3f, float * RESTRICT tvector3f)
{
	// vertex weighted skeletal
	int i, k;
	int blends;
	matrix4x4_t *bonepose;
	matrix4x4_t *boneposerelative;
	float m[12];
	matrix4x4_t mm, mm2;
	const blendweights_t * RESTRICT weights;
	int num_vertices_minus_one;

	if (!model->surfmesh.num_vertices)
		return;

	num_vertices_minus_one = model->surfmesh.num_vertices - 1;

	//unsigned long long ts = rdtsc();
	bonepose = (matrix4x4_t *) Mod_Skeletal_AnimateVertices_AllocBuffers(sizeof(matrix4x4_t) * (model->num_bones*2 + model->surfmesh.num_blends));
	boneposerelative = bonepose + model->num_bones;

	if (skeleton && !skeleton->relativetransforms)
		skeleton = NULL;

	// interpolate matrices
	if (skeleton)
	{
		for (i = 0;i < model->num_bones;i++)
		{
			// relativetransforms is in GL column-major order, which is what we need for SSE
			// transposed style processing
			if (model->data_bones[i].parent >= 0)
				Matrix4x4_Concat(&bonepose[i], &bonepose[model->data_bones[i].parent], &skeleton->relativetransforms[i]);
			else
				memcpy(&bonepose[i], &skeleton->relativetransforms[i], sizeof(matrix4x4_t));

			// create a relative deformation matrix to describe displacement
			// from the base mesh, which is used by the actual weighting
			Matrix4x4_FromArray12FloatD3D(&mm, model->data_baseboneposeinverse + i * 12); // baseboneposeinverse is 4x3 row-major
			Matrix4x4_Concat(&mm2, &bonepose[i], &mm);
			Matrix4x4_Transpose(&boneposerelative[i], &mm2); // TODO: Eliminate this transpose
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
			Matrix4x4_FromArray12FloatD3D(&mm, m);
			if (model->data_bones[i].parent >= 0)
				Matrix4x4_Concat(&bonepose[i], &bonepose[model->data_bones[i].parent], &mm);
			else
				memcpy(&bonepose[i], &mm, sizeof(mm));
			// create a relative deformation matrix to describe displacement
			// from the base mesh, which is used by the actual weighting
			Matrix4x4_FromArray12FloatD3D(&mm, model->data_baseboneposeinverse + i * 12); // baseboneposeinverse is 4x3 row-major
			Matrix4x4_Concat(&mm2, &bonepose[i], &mm);
			Matrix4x4_Transpose(&boneposerelative[i], &mm2); // TODO: Eliminate this transpose
		}
	}

	// generate matrices for all blend combinations
	weights = model->surfmesh.data_blendweights;
	for (i = 0;i < model->surfmesh.num_blends;i++, weights++)
	{
		float * RESTRICT b = &boneposerelative[model->num_bones + i].m[0][0];
		const float * RESTRICT m = &boneposerelative[weights->index[0]].m[0][0];
		float f = weights->influence[0] * (1.0f / 255.0f);
		__m128 fv = _mm_set_ps1(f);
		__m128 b0 = _mm_load_ps(m);
		__m128 b1 = _mm_load_ps(m+4);
		__m128 b2 = _mm_load_ps(m+8);
		__m128 b3 = _mm_load_ps(m+12);
		__m128 m0, m1, m2, m3;
		b0 = _mm_mul_ps(b0, fv);
		b1 = _mm_mul_ps(b1, fv);
		b2 = _mm_mul_ps(b2, fv);
		b3 = _mm_mul_ps(b3, fv);
		for (k = 1;k < 4 && weights->influence[k];k++)
		{
			m = &boneposerelative[weights->index[k]].m[0][0];
			f = weights->influence[k] * (1.0f / 255.0f);
			fv = _mm_set_ps1(f);
			m0 = _mm_load_ps(m);
			m1 = _mm_load_ps(m+4);
			m2 = _mm_load_ps(m+8);
			m3 = _mm_load_ps(m+12);
			m0 = _mm_mul_ps(m0, fv);
			m1 = _mm_mul_ps(m1, fv);
			m2 = _mm_mul_ps(m2, fv);
			m3 = _mm_mul_ps(m3, fv);
			b0 = _mm_add_ps(m0, b0);
			b1 = _mm_add_ps(m1, b1);
			b2 = _mm_add_ps(m2, b2);
			b3 = _mm_add_ps(m3, b3);
		}
		_mm_store_ps(b, b0);
		_mm_store_ps(b+4, b1);
		_mm_store_ps(b+8, b2);
		_mm_store_ps(b+12, b3);
	}

#define LOAD_MATRIX_SCALAR() const float * RESTRICT m = &boneposerelative[*b].m[0][0]

#define LOAD_MATRIX3() \
	const float * RESTRICT m = &boneposerelative[*b].m[0][0]; \
	/* bonepose array is 16 byte aligned */ \
	__m128 m1 = _mm_load_ps((m)); \
	__m128 m2 = _mm_load_ps((m)+4); \
	__m128 m3 = _mm_load_ps((m)+8);
#define LOAD_MATRIX4() \
	const float * RESTRICT m = &boneposerelative[*b].m[0][0]; \
	/* bonepose array is 16 byte aligned */ \
	__m128 m1 = _mm_load_ps((m)); \
	__m128 m2 = _mm_load_ps((m)+4); \
	__m128 m3 = _mm_load_ps((m)+8); \
	__m128 m4 = _mm_load_ps((m)+12)

	/* Note that matrix is 4x4 and transposed compared to non-USE_SSE codepath */
#define TRANSFORM_POSITION_SCALAR(in, out) \
	(out)[0] = ((in)[0] * m[0] + (in)[1] * m[4] + (in)[2] * m[ 8] + m[12]); \
	(out)[1] = ((in)[0] * m[1] + (in)[1] * m[5] + (in)[2] * m[ 9] + m[13]); \
	(out)[2] = ((in)[0] * m[2] + (in)[1] * m[6] + (in)[2] * m[10] + m[14]);
#define TRANSFORM_VECTOR_SCALAR(in, out) \
	(out)[0] = ((in)[0] * m[0] + (in)[1] * m[4] + (in)[2] * m[ 8]); \
	(out)[1] = ((in)[0] * m[1] + (in)[1] * m[5] + (in)[2] * m[ 9]); \
	(out)[2] = ((in)[0] * m[2] + (in)[1] * m[6] + (in)[2] * m[10]);

#define TRANSFORM_POSITION(in, out) { \
		__m128 pin = _mm_loadu_ps(in); /* we ignore the value in the last element (x from the next vertex) */ \
		__m128 x = _mm_shuffle_ps(pin, pin, 0x0); \
		__m128 t1 = _mm_mul_ps(x, m1); \
		\
		/* y, + x */ \
		__m128 y = _mm_shuffle_ps(pin, pin, 0x55); \
		__m128 t2 = _mm_mul_ps(y, m2); \
		__m128 t3 = _mm_add_ps(t1, t2); \
		\
		/* z, + (y+x) */ \
		__m128 z = _mm_shuffle_ps(pin, pin, 0xaa); \
		__m128 t4 = _mm_mul_ps(z, m3); \
		__m128 t5 = _mm_add_ps(t3, t4); \
		\
		/* + m3 */ \
		__m128 pout = _mm_add_ps(t5, m4); \
		_mm_storeu_ps((out), pout); \
	}

#define TRANSFORM_VECTOR(in, out) { \
		__m128 vin = _mm_loadu_ps(in); \
		\
		/* x */ \
		__m128 x = _mm_shuffle_ps(vin, vin, 0x0); \
		__m128 t1 = _mm_mul_ps(x, m1); \
		\
		/* y, + x */ \
		__m128 y = _mm_shuffle_ps(vin, vin, 0x55); \
		__m128 t2 = _mm_mul_ps(y, m2); \
		__m128 t3 = _mm_add_ps(t1, t2); \
		\
		/* nz, + (ny + nx) */ \
		__m128 z = _mm_shuffle_ps(vin, vin, 0xaa); \
		__m128 t4 = _mm_mul_ps(z, m3); \
		__m128 vout = _mm_add_ps(t3, t4); \
		_mm_storeu_ps((out), vout); \
	}

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
				for (i = 0; i < num_vertices_minus_one; i++, v += 3, n += 3, sv += 3, tv += 3, b++,
						vertex3f += 3, normal3f += 3, svector3f += 3, tvector3f += 3)
				{
					LOAD_MATRIX4();
					TRANSFORM_POSITION(v, vertex3f);
					TRANSFORM_VECTOR(n, normal3f);
					TRANSFORM_VECTOR(sv, svector3f);
					TRANSFORM_VECTOR(tv, tvector3f);
				}

				// Last vertex needs to be done with scalars to avoid reading/writing 1 word past end of arrays
				{
					LOAD_MATRIX_SCALAR();
					TRANSFORM_POSITION_SCALAR(v, vertex3f);
					TRANSFORM_VECTOR_SCALAR(n, normal3f);
					TRANSFORM_VECTOR_SCALAR(sv, svector3f);
					TRANSFORM_VECTOR_SCALAR(tv, tvector3f);
				}
				//printf("elapsed ticks: %llu\n", rdtsc() - ts); // XXX
				return;
			}

			for (i = 0;i < num_vertices_minus_one; i++, v += 3, n += 3, b++, vertex3f += 3, normal3f += 3)
			{
				LOAD_MATRIX4();
				TRANSFORM_POSITION(v, vertex3f);
				TRANSFORM_VECTOR(n, normal3f);
			}
			{
				LOAD_MATRIX_SCALAR();
				TRANSFORM_POSITION_SCALAR(v, vertex3f);
				TRANSFORM_VECTOR_SCALAR(n, normal3f);
			}
		}
		else
		{
			for (i = 0;i < num_vertices_minus_one; i++, v += 3, b++, vertex3f += 3)
			{
				LOAD_MATRIX4();
				TRANSFORM_POSITION(v, vertex3f);
			}
			{
				LOAD_MATRIX_SCALAR();
				TRANSFORM_POSITION_SCALAR(v, vertex3f);
			}
		}
	}

	else if (normal3f)
	{
		const float * RESTRICT n = model->surfmesh.data_normal3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < num_vertices_minus_one; i++, n += 3, b++, normal3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(n, normal3f);
		}
		{
			LOAD_MATRIX_SCALAR();
			TRANSFORM_VECTOR_SCALAR(n, normal3f);
		}
	}

	if (svector3f)
	{
		const float * RESTRICT sv = model->surfmesh.data_svector3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < num_vertices_minus_one; i++, sv += 3, b++, svector3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(sv, svector3f);
		}
		{
			LOAD_MATRIX_SCALAR();
			TRANSFORM_VECTOR_SCALAR(sv, svector3f);
		}
	}

	if (tvector3f)
	{
		const float * RESTRICT tv = model->surfmesh.data_tvector3f;
		const unsigned short * RESTRICT b = model->surfmesh.blends;
		for (i = 0; i < num_vertices_minus_one; i++, tv += 3, b++, tvector3f += 3)
		{
			LOAD_MATRIX3();
			TRANSFORM_VECTOR(tv, tvector3f);
		}
		{
			LOAD_MATRIX_SCALAR();
			TRANSFORM_VECTOR_SCALAR(tv, tvector3f);
		}
	}

#undef LOAD_MATRIX3
#undef LOAD_MATRIX4
#undef TRANSFORM_POSITION
#undef TRANSFORM_VECTOR
#undef LOAD_MATRIX_SCALAR
#undef TRANSFORM_POSITION_SCALAR
#undef TRANSFORM_VECTOR_SCALAR
}

#endif
