
#include "quakedef.h"
#include "r_shadow.h"

mempool_t *r_shadow_mempool;

int maxshadowelements;
int *shadowelements;
int maxtrianglefacinglight;
qbyte *trianglefacinglight;

void r_shadow_start(void)
{
	// allocate vertex processing arrays
	r_shadow_mempool = Mem_AllocPool("R_Shadow");
	maxshadowelements = 0;
	shadowelements = NULL;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
}

void r_shadow_shutdown(void)
{
	maxshadowelements = 0;
	shadowelements = NULL;
	maxtrianglefacinglight = 0;
	trianglefacinglight = NULL;
	Mem_FreePool(&r_shadow_mempool);
}

void r_shadow_newmap(void)
{
}

void R_Shadow_Init(void)
{
	R_RegisterModule("R_Shadow", r_shadow_start, r_shadow_shutdown, r_shadow_newmap);
}

void R_Shadow_Volume(int numverts, int numtris, float *vertex, int *elements, int *neighbors, vec3_t relativelightorigin, float lightradius, float projectdistance, int visiblevolume)
{
	int i, *e, *n, *out, tris;
	float *v0, *v1, *v2, temp[3], f;
	if (projectdistance < 0.1)
	{
		Con_Printf("R_Shadow_Volume: projectdistance %f\n");
		return;
	}
// terminology:
//
// frontface:
// a triangle facing the light source
//
// backface:
// a triangle not facing the light source
//
// shadow volume:
// an extrusion of the backfaces, beginning at the original geometry and
// ending further from the light source than the original geometry
// (presumably at least as far as the light's radius, if the light has a
// radius at all), capped at both front and back to avoid any problems
//
// description:
// draws the shadow volumes of the model.
// requirements:
// vertex loations must already be in vertex before use.
// vertex must have capacity for numverts * 2.

	// make sure trianglefacinglight is big enough for this volume
	if (maxtrianglefacinglight < numtris)
	{
		maxtrianglefacinglight = numtris;
		if (trianglefacinglight)
			Mem_Free(trianglefacinglight);
		trianglefacinglight = Mem_Alloc(r_shadow_mempool, maxtrianglefacinglight);
	}

	// make sure shadowelements is big enough for this volume
	if (maxshadowelements < numtris * 24)
	{
		maxshadowelements = numtris * 24;
		if (shadowelements)
			Mem_Free(shadowelements);
		shadowelements = Mem_Alloc(r_shadow_mempool, maxshadowelements * sizeof(int));
	}

	// make projected vertices
	// by clever use of elements we'll construct the whole shadow from
	// the unprojected vertices and these projected vertices
	for (i = 0, v0 = vertex, v1 = vertex + numverts * 4;i < numverts;i++, v0 += 4, v1 += 4)
	{
		VectorSubtract(v0, relativelightorigin, temp);
#if 0
		f = lightradius / sqrt(DotProduct(temp,temp));
		if (f < 1)
			f = 1;
		VectorMA(relativelightorigin, f, temp, v1);
#else
		f = projectdistance / sqrt(DotProduct(temp,temp));
		VectorMA(v0, f, temp, v1);
#endif
	}

	// check which triangles are facing the light
	for (i = 0, e = elements;i < numtris;i++, e += 3)
	{
		// calculate triangle facing flag
		v0 = vertex + e[0] * 4;
		v1 = vertex + e[1] * 4;
		v2 = vertex + e[2] * 4;
		// we do not need to normalize the surface normal because both sides
		// of the comparison use it, therefore they are both multiplied the
		// same amount...  furthermore the subtract can be done on the
		// vectors, saving a little bit of math in the dotproducts
#if 1
		// fast version
		// subtracts v1 from v0 and v2, combined into a crossproduct,
		// combined with a dotproduct of the light location relative to the
		// first point of the triangle (any point works, since the triangle
		// is obviously flat), and finally a comparison to determine if the
		// light is infront of the triangle (the goal of this statement)
		trianglefacinglight[i] =
		   (relativelightorigin[0] - v0[0]) * ((v0[1] - v1[1]) * (v2[2] - v1[2]) - (v0[2] - v1[2]) * (v2[1] - v1[1]))
		 + (relativelightorigin[1] - v0[1]) * ((v0[2] - v1[2]) * (v2[0] - v1[0]) - (v0[0] - v1[0]) * (v2[2] - v1[2]))
		 + (relativelightorigin[2] - v0[2]) * ((v0[0] - v1[0]) * (v2[1] - v1[1]) - (v0[1] - v1[1]) * (v2[0] - v1[0])) > 0;
#else
		// readable version
		{
		float dir0[3], dir1[3];

		// calculate two mostly perpendicular edge directions
		VectorSubtract(v0, v1, dir0);
		VectorSubtract(v2, v1, dir1);

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal (it's magnitude
		// is not 1 however)
		CrossProduct(dir0, dir1, temp);

		// this is entirely unnecessary, but kept for clarity
		//VectorNormalize(temp);

		// compare distance of light along normal, with distance of any point
		// of the triangle along the same normal (the triangle is planar,
		// I.E. flat, so all points give the same answer)
		// the normal is not normalized because it is used on both sides of
		// the comparison, so it's magnitude does not matter
		trianglefacinglight[i] = DotProduct(relativelightorigin, temp) >= DotProduct(v0, temp);
#endif
	}

	// output triangle elements
	out = shadowelements;
	tris = 0;

	// check each backface for bordering frontfaces,
	// and cast shadow polygons from those edges,
	// also create front and back caps for shadow volume
	for (i = 0, e = elements, n = neighbors;i < numtris;i++, e += 3, n += 3)
	{
		if (!trianglefacinglight[i])
		{
			// triangle is backface and therefore casts shadow,
			// output front and back caps for shadow volume
#if 1
			// front cap (with flipped winding order)
			out[0] = e[0];
			out[1] = e[2];
			out[2] = e[1];
			// rear cap
			out[3] = e[0] + numverts;
			out[4] = e[1] + numverts;
			out[5] = e[2] + numverts;
			out += 6;
			tris += 2;
#else
			// rear cap
			out[0] = e[0] + numverts;
			out[1] = e[1] + numverts;
			out[2] = e[2] + numverts;
			out += 3;
			tris += 1;
#endif
			// check the edges
			if (n[0] < 0 || trianglefacinglight[n[0]])
			{
				out[0] = e[0];
				out[1] = e[1];
				out[2] = e[1] + numverts;
				out[3] = e[0];
				out[4] = e[1] + numverts;
				out[5] = e[0] + numverts;
				out += 6;
				tris += 2;
			}
			if (n[1] < 0 || trianglefacinglight[n[1]])
			{
				out[0] = e[1];
				out[1] = e[2];
				out[2] = e[2] + numverts;
				out[3] = e[1];
				out[4] = e[2] + numverts;
				out[5] = e[1] + numverts;
				out += 6;
				tris += 2;
			}
			if (n[2] < 0 || trianglefacinglight[n[2]])
			{
				out[0] = e[2];
				out[1] = e[0];
				out[2] = e[0] + numverts;
				out[3] = e[2];
				out[4] = e[0] + numverts;
				out[5] = e[2] + numverts;
				out += 6;
				tris += 2;
			}
		}
	}
	R_Shadow_RenderVolume(numverts * 2, tris, shadowelements, visiblevolume);
}

void R_Shadow_RenderVolume(int numverts, int numtris, int *elements, int visiblevolume)
{
	// draw the volume
	if (visiblevolume)
	{
		qglDisable(GL_CULL_FACE);
		R_Mesh_Draw(numverts, numtris, elements);
		qglEnable(GL_CULL_FACE);
	}
	else
	{
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		R_Mesh_Draw(numverts, numtris, elements);
		// decrement stencil if frontface is behind depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		R_Mesh_Draw(numverts, numtris, elements);
	}
}

void R_Shadow_Stage_Depth(void)
{
	rmeshstate_t m;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	R_Mesh_State(&m);
	GL_Color(0, 0, 0, 1);
}

void R_Shadow_Stage_ShadowVolumes(void)
{
	GL_Color(1, 1, 1, 1);
	qglColorMask(0, 0, 0, 0);
	qglDisable(GL_BLEND);
	qglDepthMask(0);
	qglDepthFunc(GL_LEQUAL);
	qglClearStencil(0);
	qglClear(GL_STENCIL_BUFFER_BIT);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 0, 0xFF);
}

void R_Shadow_Stage_Light(void)
{
	qglEnable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ONE);
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthMask(0);
	qglDepthFunc(GL_EQUAL);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw light where this geometry was already rendered AND the
	// stencil is 0 (non-zero means shadow)
	qglStencilFunc(GL_EQUAL, 0, 0xFF);
}

void R_Shadow_Stage_Textures(void)
{
	rmeshstate_t m;
	// attempt to restore state to what Mesh_State thinks it is
	qglDisable(GL_BLEND);
	qglBlendFunc(GL_ONE, GL_ZERO);
	qglDepthMask(1);

	// now change to a more useful state
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_DST_COLOR;
	m.blendfunc2 = GL_SRC_COLOR;
	R_Mesh_State(&m);

	// now hack some more
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthFunc(GL_EQUAL);
	qglEnable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// only draw in lit areas
	qglStencilFunc(GL_EQUAL, 0, 0xFF);
}

void R_Shadow_Stage_End(void)
{
	rmeshstate_t m;
	GL_Color(1, 1, 1, 1);
	qglColorMask(1, 1, 1, 1);
	qglDepthFunc(GL_LEQUAL);
	qglDisable(GL_STENCIL_TEST);
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	qglStencilFunc(GL_ALWAYS, 0, 0xFF);

	// now change to a more useful state
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	R_Mesh_State(&m);
}

void R_Shadow_VertexLight(int numverts, float *vertex, float *normals, vec3_t relativelightorigin, float lightradius2, float lightdistbias, float lightsubtract, float *lightcolor)
{
	int i;
	float *n, *v, *c, f, dist, temp[3], light[3];
	// calculate vertex colors
	VectorCopy(lightcolor, light);
	for (i = 0, v = vertex, c = varray_color, n = normals;i < numverts;i++, v += 4, c += 4, n += 3)
	{
		VectorSubtract(relativelightorigin, v, temp);
		c[0] = 0;
		c[1] = 0;
		c[2] = 0;
		c[3] = 1;
		f = DotProduct(n, temp);
		if (f > 0)
		{
			dist = DotProduct(temp, temp);
			if (dist < lightradius2)
			{
				f = ((1.0f / (dist + lightdistbias)) - lightsubtract) * (f / sqrt(dist));
				c[0] = f * light[0];
				c[1] = f * light[1];
				c[2] = f * light[2];
			}
		}
	}
}
