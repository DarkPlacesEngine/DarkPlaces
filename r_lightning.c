
#include "quakedef.h"
#include "image.h"

cvar_t r_lightningbeam_thickness = {CVAR_SAVE, "r_lightningbeam_thickness", "4", "thickness of the lightning beam effect"};
cvar_t r_lightningbeam_scroll = {CVAR_SAVE, "r_lightningbeam_scroll", "5", "speed of texture scrolling on the lightning beam effect"};
cvar_t r_lightningbeam_repeatdistance = {CVAR_SAVE, "r_lightningbeam_repeatdistance", "128", "how far to stretch the texture along the lightning beam effect"};
cvar_t r_lightningbeam_color_red = {CVAR_SAVE, "r_lightningbeam_color_red", "1", "color of the lightning beam effect"};
cvar_t r_lightningbeam_color_green = {CVAR_SAVE, "r_lightningbeam_color_green", "1", "color of the lightning beam effect"};
cvar_t r_lightningbeam_color_blue = {CVAR_SAVE, "r_lightningbeam_color_blue", "1", "color of the lightning beam effect"};
cvar_t r_lightningbeam_qmbtexture = {CVAR_SAVE, "r_lightningbeam_qmbtexture", "0", "load the qmb textures/particles/lightning.pcx texture instead of generating one, can look better"};

skinframe_t *r_lightningbeamtexture;
skinframe_t *r_lightningbeamqmbtexture;

int r_lightningbeamelement3i[18] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};
unsigned short r_lightningbeamelement3s[18] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};

void r_lightningbeams_start(void)
{
	r_lightningbeamtexture = NULL;
	r_lightningbeamqmbtexture = NULL;
}

void r_lightningbeams_setupqmbtexture(void)
{
	r_lightningbeamqmbtexture = R_SkinFrame_LoadExternal("textures/particles/lightning.pcx", TEXF_ALPHA | TEXF_FORCELINEAR, false);
	if (r_lightningbeamqmbtexture == NULL)
		Cvar_SetValueQuick(&r_lightningbeam_qmbtexture, false);
}

void r_lightningbeams_setuptexture(void)
{
#if 0
#define BEAMWIDTH 128
#define BEAMHEIGHT 64
#define PATHPOINTS 8
	int i, j, px, py, nearestpathindex, imagenumber;
	float particlex, particley, particlexv, particleyv, dx, dy, s, maxpathstrength;
	unsigned char *pixels;
	int *image;
	struct lightningpathnode_s
	{
		float x, y, strength;
	}
	path[PATHPOINTS], temppath;

	image = Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * sizeof(int));
	pixels = Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * sizeof(unsigned char[4]));

	for (imagenumber = 0, maxpathstrength = 0.0339476;maxpathstrength < 0.5;imagenumber++, maxpathstrength += 0.01)
	{
		for (i = 0;i < PATHPOINTS;i++)
		{
			path[i].x = lhrandom(0, 1);
			path[i].y = lhrandom(0.2, 0.8);
			path[i].strength = lhrandom(0, 1);
		}
		for (i = 0;i < PATHPOINTS;i++)
		{
			for (j = i + 1;j < PATHPOINTS;j++)
			{
				if (path[j].x < path[i].x)
				{
					temppath = path[j];
					path[j] = path[i];
					path[i] = temppath;
				}
			}
		}
		particlex = path[0].x;
		particley = path[0].y;
		particlexv = lhrandom(0, 0.02);
		particlexv = lhrandom(-0.02, 0.02);
		memset(image, 0, BEAMWIDTH * BEAMHEIGHT * sizeof(int));
		for (i = 0;i < 65536;i++)
		{
			for (nearestpathindex = 0;nearestpathindex < PATHPOINTS;nearestpathindex++)
				if (path[nearestpathindex].x > particlex)
					break;
			nearestpathindex %= PATHPOINTS;
			dx = path[nearestpathindex].x + lhrandom(-0.01, 0.01);dx = bound(0, dx, 1) - particlex;if (dx < 0) dx += 1;
			dy = path[nearestpathindex].y + lhrandom(-0.01, 0.01);dy = bound(0, dy, 1) - particley;
			s = path[nearestpathindex].strength / sqrt(dx*dx+dy*dy);
			particlexv = particlexv /* (1 - lhrandom(0.08, 0.12))*/ + dx * s;
			particleyv = particleyv /* (1 - lhrandom(0.08, 0.12))*/ + dy * s;
			particlex += particlexv * maxpathstrength;particlex -= (int) particlex;
			particley += particleyv * maxpathstrength;particley = bound(0, particley, 1);
			px = particlex * BEAMWIDTH;
			py = particley * BEAMHEIGHT;
			if (px >= 0 && py >= 0 && px < BEAMWIDTH && py < BEAMHEIGHT)
				image[py*BEAMWIDTH+px] += 16;
		}

		for (py = 0;py < BEAMHEIGHT;py++)
		{
			for (px = 0;px < BEAMWIDTH;px++)
			{
				pixels[(py*BEAMWIDTH+px)*4+2] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
				pixels[(py*BEAMWIDTH+px)*4+1] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
				pixels[(py*BEAMWIDTH+px)*4+0] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
				pixels[(py*BEAMWIDTH+px)*4+3] = 255;
			}
		}

		Image_WriteTGABGRA(va("lightningbeam%i.tga", imagenumber), BEAMWIDTH, BEAMHEIGHT, pixels);
	}

	r_lightningbeamtexture = R_LoadTexture2D(r_lightningbeamtexturepool, "lightningbeam", BEAMWIDTH, BEAMHEIGHT, pixels, TEXTYPE_BGRA, TEXF_FORCELINEAR, NULL);

	Mem_Free(pixels);
	Mem_Free(image);
#else
#define BEAMWIDTH 64
#define BEAMHEIGHT 128
	float r, g, b, intensity, fx, width, center;
	int x, y;
	unsigned char *data, *noise1, *noise2;

	data = (unsigned char *)Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * 4);
	noise1 = (unsigned char *)Mem_Alloc(tempmempool, BEAMHEIGHT * BEAMHEIGHT);
	noise2 = (unsigned char *)Mem_Alloc(tempmempool, BEAMHEIGHT * BEAMHEIGHT);
	fractalnoise(noise1, BEAMHEIGHT, BEAMHEIGHT / 8);
	fractalnoise(noise2, BEAMHEIGHT, BEAMHEIGHT / 16);

	for (y = 0;y < BEAMHEIGHT;y++)
	{
		width = 0.15;//((noise1[y * BEAMHEIGHT] * (1.0f / 256.0f)) * 0.1f + 0.1f);
		center = (noise1[y * BEAMHEIGHT + (BEAMHEIGHT / 2)] / 256.0f) * (1.0f - width * 2.0f) + width;
		for (x = 0;x < BEAMWIDTH;x++, fx++)
		{
			fx = (((float) x / BEAMWIDTH) - center) / width;
			intensity = 1.0f - sqrt(fx * fx);
			if (intensity > 0)
				intensity = pow(intensity, 2) * ((noise2[y * BEAMHEIGHT + x] * (1.0f / 256.0f)) * 0.33f + 0.66f);
			intensity = bound(0, intensity, 1);
			r = intensity * 1.0f;
			g = intensity * 1.0f;
			b = intensity * 1.0f;
			data[(y * BEAMWIDTH + x) * 4 + 2] = (unsigned char)(bound(0, r, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 1] = (unsigned char)(bound(0, g, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 0] = (unsigned char)(bound(0, b, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 3] = (unsigned char)255;
		}
	}

	r_lightningbeamtexture = R_SkinFrame_LoadInternalBGRA("lightningbeam", TEXF_FORCELINEAR, data, BEAMWIDTH, BEAMHEIGHT, false);
	Mem_Free(noise1);
	Mem_Free(noise2);
	Mem_Free(data);
#endif
}

void r_lightningbeams_shutdown(void)
{
	r_lightningbeamtexture = NULL;
	r_lightningbeamqmbtexture = NULL;
}

void r_lightningbeams_newmap(void)
{
	if (r_lightningbeamtexture)
		R_SkinFrame_MarkUsed(r_lightningbeamtexture);
	if (r_lightningbeamqmbtexture)
		R_SkinFrame_MarkUsed(r_lightningbeamqmbtexture);
}

void R_LightningBeams_Init(void)
{
	Cvar_RegisterVariable(&r_lightningbeam_thickness);
	Cvar_RegisterVariable(&r_lightningbeam_scroll);
	Cvar_RegisterVariable(&r_lightningbeam_repeatdistance);
	Cvar_RegisterVariable(&r_lightningbeam_color_red);
	Cvar_RegisterVariable(&r_lightningbeam_color_green);
	Cvar_RegisterVariable(&r_lightningbeam_color_blue);
	Cvar_RegisterVariable(&r_lightningbeam_qmbtexture);
	R_RegisterModule("R_LightningBeams", r_lightningbeams_start, r_lightningbeams_shutdown, r_lightningbeams_newmap, NULL, NULL);
}

void R_CalcLightningBeamPolygonVertex3f(float *v, const float *start, const float *end, const float *offset)
{
	// near right corner
	VectorAdd     (start, offset, (v + 0));
	// near left corner
	VectorSubtract(start, offset, (v + 3));
	// far left corner
	VectorSubtract(end  , offset, (v + 6));
	// far right corner
	VectorAdd     (end  , offset, (v + 9));
}

void R_CalcLightningBeamPolygonTexCoord2f(float *tc, float t1, float t2)
{
	if (r_lightningbeam_qmbtexture.integer)
	{
		// near right corner
		tc[0] = t1;tc[1] = 0;
		// near left corner
		tc[2] = t1;tc[3] = 1;
		// far left corner
		tc[4] = t2;tc[5] = 1;
		// far right corner
		tc[6] = t2;tc[7] = 0;
	}
	else
	{
		// near right corner
		tc[0] = 0;tc[1] = t1;
		// near left corner
		tc[2] = 1;tc[3] = t1;
		// far left corner
		tc[4] = 1;tc[5] = t2;
		// far right corner
		tc[6] = 0;tc[7] = t2;
	}
}

float beamrepeatscale;

void R_DrawLightningBeam_TransparentCallback(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int surfacelistindex;
	float vertex3f[12*3];
	float texcoord2f[12*2];

	RSurf_ActiveCustomEntity(&identitymatrix, &identitymatrix, 0, 0, r_lightningbeam_color_red.value, r_lightningbeam_color_green.value, r_lightningbeam_color_blue.value, 1, 12, vertex3f, texcoord2f, NULL, NULL, NULL, NULL, 6, r_lightningbeamelement3i, r_lightningbeamelement3s, false, false);

	if (r_lightningbeam_qmbtexture.integer && r_lightningbeamqmbtexture == NULL)
		r_lightningbeams_setupqmbtexture();
	if (!r_lightningbeam_qmbtexture.integer && r_lightningbeamtexture == NULL)
		r_lightningbeams_setuptexture();

	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const beam_t *b = cl.beams + surfacelist[surfacelistindex];
		vec3_t beamdir, right, up, offset, start, end;
		float length, t1, t2;

		CL_Beam_CalculatePositions(b, start, end);

		// calculate beam direction (beamdir) vector and beam length
		// get difference vector
		VectorSubtract(end, start, beamdir);
		// find length of difference vector
		length = sqrt(DotProduct(beamdir, beamdir));
		// calculate scale to make beamdir a unit vector (normalized)
		t1 = 1.0f / length;
		// scale beamdir so it is now normalized
		VectorScale(beamdir, t1, beamdir);

		// calculate up vector such that it points toward viewer, and rotates around the beamdir
		// get direction from start of beam to viewer
		VectorSubtract(r_refdef.view.origin, start, up);
		// remove the portion of the vector that moves along the beam
		// (this leaves only a vector pointing directly away from the beam)
		t1 = -DotProduct(up, beamdir);
		VectorMA(up, t1, beamdir, up);
		// generate right vector from forward and up, the result is unnormalized
		CrossProduct(beamdir, up, right);
		// now normalize the right vector and up vector
		VectorNormalize(right);
		VectorNormalize(up);

		// calculate T coordinate scrolling (start and end texcoord along the beam)
		t1 = r_refdef.scene.time * -r_lightningbeam_scroll.value;// + beamrepeatscale * DotProduct(start, beamdir);
		t1 = t1 - (int) t1;
		t2 = t1 + beamrepeatscale * length;

		// the beam is 3 polygons in this configuration:
		//  *   2
		//   * *
		// 1******
		//   * *
		//  *   3
		// they are showing different portions of the beam texture, creating an
		// illusion of a beam that appears to curl around in 3D space
		// (and realize that the whole polygon assembly orients itself to face
		//  the viewer)

		// polygon 1, verts 0-3
		VectorScale(right, r_lightningbeam_thickness.value, offset);
		R_CalcLightningBeamPolygonVertex3f(vertex3f + 0, start, end, offset);
		// polygon 2, verts 4-7
		VectorAdd(right, up, offset);
		VectorScale(offset, r_lightningbeam_thickness.value * 0.70710681f, offset);
		R_CalcLightningBeamPolygonVertex3f(vertex3f + 12, start, end, offset);
		// polygon 3, verts 8-11
		VectorSubtract(right, up, offset);
		VectorScale(offset, r_lightningbeam_thickness.value * 0.70710681f, offset);
		R_CalcLightningBeamPolygonVertex3f(vertex3f + 24, start, end, offset);
		R_CalcLightningBeamPolygonTexCoord2f(texcoord2f + 0, t1, t2);
		R_CalcLightningBeamPolygonTexCoord2f(texcoord2f + 8, t1 + 0.33, t2 + 0.33);
		R_CalcLightningBeamPolygonTexCoord2f(texcoord2f + 16, t1 + 0.66, t2 + 0.66);

		// draw the 3 polygons as one batch of 6 triangles using the 12 vertices
		R_DrawCustomSurface(r_lightningbeam_qmbtexture.integer ? r_lightningbeamqmbtexture : r_lightningbeamtexture, &identitymatrix, MATERIALFLAG_ADD | MATERIALFLAG_BLENDED | MATERIALFLAG_FULLBRIGHT | MATERIALFLAG_NOCULLFACE, 0, 12, 0, 6, false, false);
	}
}

extern cvar_t cl_beams_polygons;
void R_DrawLightningBeams(void)
{
	int i;
	beam_t *b;

	if (!cl_beams_polygons.integer)
		return;

	beamrepeatscale = 1.0f / r_lightningbeam_repeatdistance.value;
	for (i = 0, b = cl.beams;i < cl.num_beams;i++, b++)
	{
		if (b->model && b->lightning)
		{
			vec3_t org, start, end, dir;
			vec_t dist;
			CL_Beam_CalculatePositions(b, start, end);
			// calculate the nearest point on the line (beam) for depth sorting
			VectorSubtract(end, start, dir);
			dist = (DotProduct(r_refdef.view.origin, dir) - DotProduct(start, dir)) / (DotProduct(end, dir) - DotProduct(start, dir));
			dist = bound(0, dist, 1);
			VectorLerp(start, dist, end, org);
			// now we have the nearest point on the line, so sort with it
			R_MeshQueue_AddTransparent(org, R_DrawLightningBeam_TransparentCallback, NULL, i, NULL);
		}
	}
}

