
#include "quakedef.h"

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

void R_Shadow_Volume(int numverts, int numtris, int *elements, int *neighbors, vec3_t relativelightorigin, float projectdistance, int visiblevolume)
{
	int i, *e, *n, *out, tris;
	float *v0, *v1, *v2, dir0[3], dir1[3], temp[3], f;
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
// vertex loations must already be in varray_vertex before use.
// varray_vertex must have capacity for numverts * 2.

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
	for (i = 0, v0 = varray_vertex, v1 = varray_vertex + numverts * 4;i < numverts;i++, v0 += 4, v1 += 4)
	{
		VectorSubtract(v0, relativelightorigin, temp);
		f = projectdistance / sqrt(DotProduct(temp,temp));
		VectorMA(v0, f, temp, v1);
	}

	// check which triangles are facing the light
	for (i = 0, e = elements;i < numtris;i++, e += 3)
	{
		// calculate surface plane
		v0 = varray_vertex + e[0] * 4;
		v1 = varray_vertex + e[1] * 4;
		v2 = varray_vertex + e[2] * 4;
		VectorSubtract(v0, v1, dir0);
		VectorSubtract(v2, v1, dir1);
		CrossProduct(dir0, dir1, temp);
		// we do not need to normalize the surface normal because both sides
		// of the comparison use it, therefore they are both multiplied the
		// same amount...
		trianglefacinglight[i] = DotProduct(relativelightorigin, temp) >= DotProduct(v0, temp);
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
	// draw the volume
	if (visiblevolume)
	{
		qglDisable(GL_CULL_FACE);
		R_Mesh_Draw(numverts * 2, tris, shadowelements);
		qglEnable(GL_CULL_FACE);
	}
	else
	{
		qglColorMask(0,0,0,0);
		qglEnable(GL_STENCIL_TEST);
		// increment stencil if backface is behind depthbuffer
		qglCullFace(GL_BACK); // quake is backwards, this culls front faces
		qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		R_Mesh_Draw(numverts * 2, tris, shadowelements);
		// decrement stencil if frontface is infront of depthbuffer
		qglCullFace(GL_FRONT); // quake is backwards, this culls back faces
		qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
		R_Mesh_Draw(numverts * 2, tris, shadowelements);
		// restore to normal quake rendering
		qglDisable(GL_STENCIL_TEST);
		qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		qglColorMask(1,1,1,1);
	}
}

void R_Shadow_VertexLight(int numverts, float *normals, vec3_t relativelightorigin, float lightradius2, float lightdistbias, float lightsubtract, float *lightcolor)
{
	int i;
	float *n, *v, *c, f, dist, temp[3];
	// calculate vertex colors
	for (i = 0, v = varray_vertex, c = varray_color, n = normals;i < numverts;i++, v += 4, c += 4, n += 3)
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
				c[0] = f * lightcolor[0];
				c[1] = f * lightcolor[1];
				c[2] = f * lightcolor[2];
			}
		}
	}
}

void R_Shadow_RenderLightThroughStencil(int numverts, int numtris, int *elements, vec3_t relativelightorigin, float *normals)
{
	// only draw light where this geometry was already rendered AND the
	// stencil is 0 (non-zero means shadow)
	qglDepthFunc(GL_EQUAL);
	qglEnable(GL_STENCIL_TEST);
	qglStencilFunc(GL_EQUAL, 0, 0xFF);
	R_Mesh_Draw(numverts, numtris, elements);
	qglDisable(GL_STENCIL_TEST);
	qglDepthFunc(GL_LEQUAL);
}

void R_Shadow_ClearStencil(void)
{
	qglClearStencil(0);
	qglClear(GL_STENCIL_BUFFER_BIT);
}
