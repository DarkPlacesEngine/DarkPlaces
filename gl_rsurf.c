/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_surf.c: surface-related refresh code

#include "quakedef.h"

#define MAX_LIGHTMAP_SIZE 256

static signed int blocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*3]; // LordHavoc: *3 for colored lighting

static byte templight[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE*4];

cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t r_vertexsurfaces = {0, "r_vertexsurfaces", "0"};
cvar_t r_dlightmap = {CVAR_SAVE, "r_dlightmap", "1"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};

static void gl_surf_start(void)
{
}

static void gl_surf_shutdown(void)
{
}

static void gl_surf_newmap(void)
{
}

static int dlightdivtable[32768];

void GL_Surf_Init(void)
{
	int i;
	dlightdivtable[0] = 4194304;
	for (i = 1;i < 32768;i++)
		dlightdivtable[i] = 4194304 / (i << 7);

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_vertexsurfaces);
	Cvar_RegisterVariable(&r_dlightmap);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

static int R_AddDynamicLights (msurface_t *surf)
{
	int         sdtable[256], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, red, green, blue, lit, dist2, impacts, impactt, subtract;
	unsigned int *bl;
	float       dist;
	vec3_t      impact, local;

	// LordHavoc: use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64     k;
#else
	long long   k;
#endif

	lit = false;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (lnum = 0; lnum < r_numdlights; lnum++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		softwareuntransform(r_dlight[lnum].origin, local);
//		VectorSubtract (r_dlight[lnum].origin, currentrenderentity->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		// compensate for LIGHTOFFSET
		maxdist = (int) r_dlight[lnum].cullradius2 + LIGHTOFFSET;

		// already clamped, skip this
		// clamp radius to avoid exceeding 32768 entry division table
		//if (maxdist > 4194304)
		//	maxdist = 4194304;

		dist2 = dist * dist;
		dist2 += LIGHTOFFSET;
		if (dist2 >= maxdist)
			continue;

		impact[0] = local[0] - surf->plane->normal[0] * dist;
		impact[1] = local[1] - surf->plane->normal[1] * dist;
		impact[2] = local[2] - surf->plane->normal[2] * dist;

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		s = bound(0, impacts, smax * 16) - impacts;
		t = bound(0, impactt, tmax * 16) - impactt;
		i = s * s + t * t + dist2;
		if (i > maxdist)
			continue;

		// reduce calculations
		for (s = 0, i = impacts; s < smax; s++, i -= 16)
			sdtable[s] = i * i + dist2;

		maxdist3 = maxdist - dist2;

		// convert to 8.8 blocklights format
		red = r_dlight[lnum].light[0];
		green = r_dlight[lnum].light[1];
		blue = r_dlight[lnum].light[2];
		subtract = (int) (r_dlight[lnum].lightsubtract * 4194304.0f);
		bl = blocklights;
		smax3 = smax * 3;

		i = impactt;
		for (t = 0;t < tmax;t++, i -= 16)
		{
			td = i * i;
			// make sure some part of it is visible on this line
			if (td < maxdist3)
			{
				maxdist2 = maxdist - td;
				for (s = 0;s < smax;s++)
				{
					if (sdtable[s] < maxdist2)
					{
						k = dlightdivtable[(sdtable[s] + td) >> 7] - subtract;
						if (k > 0)
						{
							bl[0] += (red   * k) >> 8;
							bl[1] += (green * k) >> 8;
							bl[2] += (blue  * k) >> 8;
							lit = true;
						}
					}
					bl += 3;
				}
			}
			else // skip line
				bl += smax3;
		}
	}
	return lit;
}

void R_StainNode (mnode_t *node, model_t *model, vec3_t origin, float radius, int icolor[8])
{
	float ndist;
	msurface_t *surf, *endsurf;
	int sdtable[256], td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, smax3, dist2, impacts, impactt, subtract, a, stained, cr, cg, cb, ca, ratio;
	byte *bl;
	vec3_t impact;
	// LordHavoc: use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64     k;
#else
	long long   k;
#endif


	// for comparisons to minimum acceptable light
	// compensate for 4096 offset
	maxdist = radius * radius + 4096;

	// clamp radius to avoid exceeding 32768 entry division table
	if (maxdist > 4194304)
		maxdist = 4194304;

	subtract = (int) ((1.0f / maxdist) * 4194304.0f);

loc0:
	if (node->contents < 0)
		return;
	ndist = PlaneDiff(origin, node->plane);
	if (ndist > radius)
	{
		node = node->children[0];
		goto loc0;
	}
	if (ndist < -radius)
	{
		node = node->children[1];
		goto loc0;
	}

	dist2 = ndist * ndist;
	dist2 += 4096.0f;
	if (dist2 < maxdist)
	{
		maxdist3 = maxdist - dist2;

		impact[0] = origin[0] - node->plane->normal[0] * ndist;
		impact[1] = origin[1] - node->plane->normal[1] * ndist;
		impact[2] = origin[2] - node->plane->normal[2] * ndist;

		for (surf = model->surfaces + node->firstsurface, endsurf = surf + node->numsurfaces;surf < endsurf;surf++)
		{
			if (surf->stainsamples)
			{
				smax = (surf->extents[0] >> 4) + 1;
				tmax = (surf->extents[1] >> 4) + 1;

				impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
				impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

				s = bound(0, impacts, smax * 16) - impacts;
				t = bound(0, impactt, tmax * 16) - impactt;
				i = s * s + t * t + dist2;
				if (i > maxdist)
					continue;

				// reduce calculations
				for (s = 0, i = impacts; s < smax; s++, i -= 16)
					sdtable[s] = i * i + dist2;

				// convert to 8.8 blocklights format
				bl = surf->stainsamples;
				smax3 = smax * 3;
				stained = false;

				i = impactt;
				for (t = 0;t < tmax;t++, i -= 16)
				{
					td = i * i;
					// make sure some part of it is visible on this line
					if (td < maxdist3)
					{
						maxdist2 = maxdist - td;
						for (s = 0;s < smax;s++)
						{
							if (sdtable[s] < maxdist2)
							{
								k = dlightdivtable[(sdtable[s] + td) >> 7] - subtract;
								if (k > 0)
								{
									ratio = rand() & 255;
									ca = (((icolor[7] - icolor[3]) * ratio) >> 8) + icolor[3];
									a = (ca * k) >> 8;
									if (a > 0)
									{
										a = bound(0, a, 256);
										cr = (((icolor[4] - icolor[0]) * ratio) >> 8) + icolor[0];
										cg = (((icolor[5] - icolor[1]) * ratio) >> 8) + icolor[1];
										cb = (((icolor[6] - icolor[2]) * ratio) >> 8) + icolor[2];
										bl[0] = (byte) ((((cr - (int) bl[0]) * a) >> 8) + (int) bl[0]);
										bl[1] = (byte) ((((cg - (int) bl[1]) * a) >> 8) + (int) bl[1]);
										bl[2] = (byte) ((((cb - (int) bl[2]) * a) >> 8) + (int) bl[2]);
										stained = true;
									}
								}
							}
							bl += 3;
						}
					}
					else // skip line
						bl += smax3;
				}
				// force lightmap upload
				if (stained)
					surf->cached_dlight = true;
			}
		}
	}

	if (node->children[0]->contents >= 0)
	{
		if (node->children[1]->contents >= 0)
		{
			R_StainNode(node->children[0], model, origin, radius, icolor);
			node = node->children[1];
			goto loc0;
		}
		else
		{
			node = node->children[0];
			goto loc0;
		}
	}
	else if (node->children[1]->contents >= 0)
	{
		node = node->children[1];
		goto loc0;
	}
}

void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2)
{
	int n, icolor[8];
	entity_render_t *ent;
	model_t *model;
	vec3_t org;
	icolor[0] = cr1;
	icolor[1] = cg1;
	icolor[2] = cb1;
	icolor[3] = ca1;
	icolor[4] = cr2;
	icolor[5] = cg2;
	icolor[6] = cb2;
	icolor[7] = ca2;

	model = cl.worldmodel;
	softwaretransformidentity();
	R_StainNode(model->nodes + model->hulls[0].firstclipnode, model, origin, radius, icolor);

	// look for embedded bmodels
	for (n = 1;n < MAX_EDICTS;n++)
	{
		ent = &cl_entities[n].render;
		model = ent->model;
		if (model && model->name[0] == '*')
		{
			Mod_CheckLoaded(model);
			if (model->type == mod_brush)
			{
				softwaretransformforentity(ent);
				softwareuntransform(origin, org);
				R_StainNode(model->nodes + model->hulls[0].firstclipnode, model, org, radius, icolor);
			}
		}
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void R_BuildLightMap (msurface_t *surf, int dlightchanged)
{
	int		smax, tmax, i, j, size, size3, shift, scale, maps, *bl, stride, l;
	byte	*lightmap, *out, *stain;

	// update cached lighting info
	surf->cached_dlight = 0;
	surf->cached_lightscalebit = lightscalebit;
	surf->cached_ambient = r_ambient.value;
	surf->cached_light[0] = d_lightstylevalue[surf->styles[0]];
	surf->cached_light[1] = d_lightstylevalue[surf->styles[1]];
	surf->cached_light[2] = d_lightstylevalue[surf->styles[2]];
	surf->cached_light[3] = d_lightstylevalue[surf->styles[3]];

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;
	lightmap = surf->samples;

// set to full bright if no light data
	if ((currentrenderentity->effects & EF_FULLBRIGHT) || !cl.worldmodel->lightdata)
	{
		bl = blocklights;
		for (i = 0;i < size;i++)
		{
			*bl++ = 255*256;
			*bl++ = 255*256;
			*bl++ = 255*256;
		}
	}
	else
	{
// clear to no light
		j = r_ambient.value * 512.0f; // would be 256.0f logically, but using 512.0f to match winquake style
		if (j)
		{
			bl = blocklights;
			for (i = 0;i < size3;i++)
				*bl++ = j;
		}
		else
			memset(&blocklights[0], 0, size*3*sizeof(int));

		if (surf->dlightframe == r_framecount && r_dlightmap.integer)
		{
			surf->cached_dlight = R_AddDynamicLights(surf);
			if (surf->cached_dlight)
				c_light_polys++;
			else if (dlightchanged)
				return; // don't upload if only updating dlights and none mattered
		}

// add all the lightmaps
		if (lightmap)
			for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
				for (scale = d_lightstylevalue[surf->styles[maps]], bl = blocklights, i = 0;i < size3;i++)
					*bl++ += *lightmap++ * scale;
	}

	stain = surf->stainsamples;
	if (stain)
		for (bl = blocklights, i = 0;i < size3;i++)
			if (stain[i] < 255)
				bl[i] = (bl[i] * stain[i]) >> 8;

	bl = blocklights;
	out = templight;
	// deal with lightmap brightness scale
	shift = 7 + lightscalebit;
	if (currentrenderentity->model->lightmaprgba)
	{
		stride = (surf->lightmaptexturestride - smax) * 4;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = *bl++ >> shift;*out++ = min(l, 255);
				l = *bl++ >> shift;*out++ = min(l, 255);
				l = *bl++ >> shift;*out++ = min(l, 255);
				*out++ = 255;
			}
		}
	}
	else
	{
		stride = (surf->lightmaptexturestride - smax) * 3;
		for (i = 0;i < tmax;i++, out += stride)
		{
			for (j = 0;j < smax;j++)
			{
				l = *bl++ >> shift;*out++ = min(l, 255);
				l = *bl++ >> shift;*out++ = min(l, 255);
				l = *bl++ >> shift;*out++ = min(l, 255);
			}
		}
	}

	R_UpdateTexture(surf->lightmaptexture, templight);
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
/*
// note: this was manually inlined in R_PrepareSurfaces
static texture_t *R_TextureAnimation (texture_t *base)
{
	if (currentrenderentity->frame && base->alternate_anims != NULL)
		base = base->alternate_anims;

	if (base->anim_total < 2)
		return base;

	return base->anim_frames[(int)(cl.time * 5.0f) % base->anim_total];
}
*/


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


static float turbsin[256] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

#define MAX_SURFVERTS 1024
typedef struct
{
	float v[4];
	float st[2];
	float uv[2];
	float c[4];
}
surfvert_t;
static surfvert_t svert[MAX_SURFVERTS]; // used by the following functions

static int RSurfShader_Sky(int stage, msurface_t *s)
{
	int				i;
	float			number, length, dir[3], speedscale;
	surfvertex_t	*v;
	surfvert_t 		*sv;
	rmeshinfo_t		m;

	// LordHavoc: HalfLife maps have freaky skypolys...
	if (currentrenderentity->model->ishlbsp)
		return true;

	if (stage == 0)
	{
		if (skyrendermasked)
		{
			if (skyrendernow)
			{
				skyrendernow = false;
				R_Sky();
			}
			// draw depth-only polys
			memset(&m, 0, sizeof(m));
			m.transparent = false;
			m.blendfunc1 = GL_ZERO;
			m.blendfunc2 = GL_ONE;
			m.depthwrite = true;
			m.numtriangles = s->mesh.numtriangles;
			m.numverts = s->mesh.numverts;
			m.index = s->mesh.index;
			//m.cr = 0;
			//m.cg = 0;
			//m.cb = 0;
			//m.ca = 0;
			if (softwaretransform_complexity)
			{
				m.vertex = &svert[0].v[0];
				m.vertexstep = sizeof(surfvert_t);
				for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
					softwaretransform(v->v, sv->v);
			}
			else
			{
				m.vertex = &s->mesh.vertex[0].v[0];
				m.vertexstep = sizeof(surfvertex_t);
			}
			R_Mesh_Draw(&m);
		}
		else if (skyrenderglquake)
		{
			memset(&m, 0, sizeof(m));
			m.transparent = false;
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ZERO;
			m.numtriangles = s->mesh.numtriangles;
			m.numverts = s->mesh.numverts;
			m.index = s->mesh.index;
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			m.cr = 1;
			m.cg = 1;
			m.cb = 1;
			m.ca = 1;
			if (r_mergesky.integer)
				m.tex[0] = R_GetTexture(mergeskytexture);
			else
				m.tex[0] = R_GetTexture(solidskytexture);
			m.texcoords[0] = &svert[0].st[0];
			m.texcoordstep[0] = sizeof(surfvert_t);
			speedscale = cl.time * (8.0/128.0);
			speedscale -= (int)speedscale;
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				softwaretransform(v->v, sv->v);
				VectorSubtract (sv->v, r_origin, dir);
				// flatten the sphere
				dir[2] *= 3;

				number = DotProduct(dir, dir);
				#if SLOWMATH
				length = 3.0f / sqrt(number);
				#else
				*((long *)&length) = 0x5f3759df - ((* (long *) &number) >> 1);
				length = 3.0f * (length * (1.5f - (number * 0.5f * length * length)));
				#endif

				sv->st[0] = speedscale + dir[0] * length;
				sv->st[1] = speedscale + dir[1] * length;
			}
			R_Mesh_Draw(&m);
		}
		else
		{
			// flat color
			memset(&m, 0, sizeof(m));
			m.transparent = false;
			m.blendfunc1 = GL_ONE;
			m.blendfunc2 = GL_ZERO;
			m.numtriangles = s->mesh.numtriangles;
			m.numverts = s->mesh.numverts;
			m.index = s->mesh.index;
			m.cr = fogcolor[0];
			m.cg = fogcolor[1];
			m.cb = fogcolor[2];
			m.ca = 1;
			if (softwaretransform_complexity)
			{
				m.vertex = &svert[0].v[0];
				m.vertexstep = sizeof(surfvert_t);
				for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
					softwaretransform(v->v, sv->v);
			}
			else
			{
				m.vertex = &s->mesh.vertex[0].v[0];
				m.vertexstep = sizeof(surfvertex_t);
			}
			R_Mesh_Draw(&m);
		}
		return false;
	}
	else if (stage == 1)
	{
		if (skyrenderglquake && !r_mergesky.integer)
		{
			memset(&m, 0, sizeof(m));
			m.transparent = false;
			m.blendfunc1 = GL_SRC_ALPHA;
			m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
			m.numtriangles = s->mesh.numtriangles;
			m.numverts = s->mesh.numverts;
			m.index = s->mesh.index;
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			m.cr = 1;
			m.cg = 1;
			m.cb = 1;
			m.ca = 1;
			m.tex[0] = R_GetTexture(alphaskytexture);
			m.texcoords[0] = &svert[0].st[0];
			m.texcoordstep[0] = sizeof(surfvert_t);
			speedscale = cl.time * (16.0/128.0);
			speedscale -= (int)speedscale;
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				softwaretransform(v->v, sv->v);
				VectorSubtract (sv->v, r_origin, dir);
				// flatten the sphere
				dir[2] *= 3;

				number = DotProduct(dir, dir);
				#if SLOWMATH
				length = 3.0f / sqrt(number);
				#else
				*((long *)&length) = 0x5f3759df - ((* (long *) &number) >> 1);
				length = 3.0f * (length * (1.5f - (number * 0.5f * length * length)));
				#endif

				sv->st[0] = speedscale + dir[0] * length;
				sv->st[1] = speedscale + dir[1] * length;
			}
			R_Mesh_Draw(&m);
			return false;
		}
		return true;
	}
	else
		return true;
}

static int RSurf_Light(int *dlightbits, int numverts)
{
	float		f;
	int			i, l, lit = false;
	rdlight_t	*rd;
	vec3_t		lightorigin;
	surfvert_t	*sv;
	for (l = 0;l < r_numdlights;l++)
	{
		if (dlightbits[l >> 5] & (1 << (l & 31)))
		{
			rd = &r_dlight[l];
			// FIXME: support softwareuntransform here and make bmodels use hardware transform?
			VectorCopy(rd->origin, lightorigin);
			for (i = 0, sv = svert;i < numverts;i++, sv++)
			{
				f = VectorDistance2(sv->v, lightorigin) + LIGHTOFFSET;
				if (f < rd->cullradius2)
				{
					f = (1.0f / f) - rd->lightsubtract;
					sv->c[0] += rd->light[0] * f;
					sv->c[1] += rd->light[1] * f;
					sv->c[2] += rd->light[2] * f;
					lit = true;
				}
			}
		}
	}
	return lit;
}

static void RSurfShader_Water_Pass_Base(msurface_t *s)
{
	int				i;
	float			diff[3], alpha, ifog;
	surfvertex_t	*v;
	surfvert_t 		*sv;
	rmeshinfo_t		m;
	alpha = currentrenderentity->alpha * (s->flags & SURF_DRAWNOALPHA ? 1 : r_wateralpha.value);

	memset(&m, 0, sizeof(m));
	if (alpha != 1 || s->currenttexture->fogtexture != NULL)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE; //_MINUS_SRC_ALPHA;
	}
	else
	{
		m.transparent = false;
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.color = &svert[0].c[0];
	m.colorstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.texcoords[0] = &svert[0].st[0];
	m.texcoordstep[0] = sizeof(surfvert_t);
	for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
	{
		softwaretransform(v->v, sv->v);
		if (r_waterripple.value)
			sv->v[2] += r_waterripple.value * (1.0f / 64.0f) * turbsin[(int)((v->v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v->v[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255];
		if (s->flags & SURF_DRAWFULLBRIGHT)
		{
			sv->c[0] = 1;
			sv->c[1] = 1;
			sv->c[2] = 1;
			sv->c[3] = alpha;
		}
		else
		{
			sv->c[0] = 0.5f;
			sv->c[1] = 0.5f;
			sv->c[2] = 0.5f;
			sv->c[3] = alpha;
		}
		sv->st[0] = (v->st[0] + turbsin[(int)((v->st[1]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
		sv->st[1] = (v->st[1] + turbsin[(int)((v->st[0]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
	}
	if (s->dlightframe == r_framecount && !(s->flags & SURF_DRAWFULLBRIGHT))
		RSurf_Light(s->dlightbits, m.numverts);
	if (fogenabled/* && m.blendfunc2 == GL_ONE_MINUS_SRC_ALPHA*/)
	{
		for (i = 0, sv = svert;i < m.numverts;i++, sv++)
		{
			VectorSubtract(sv->v, r_origin, diff);
			ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
			sv->c[0] *= ifog;
			sv->c[1] *= ifog;
			sv->c[2] *= ifog;
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Water_Pass_Glow(msurface_t *s)
{
	int				i;
	float			diff[3], alpha, ifog;
	surfvertex_t	*v;
	surfvert_t 		*sv;
	rmeshinfo_t		m;
	alpha = currentrenderentity->alpha * (s->flags & SURF_DRAWNOALPHA ? 1 : r_wateralpha.value);

	memset(&m, 0, sizeof(m));
	m.transparent = alpha != 1 || s->currenttexture->fogtexture != NULL;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = alpha;
	m.tex[0] = R_GetTexture(s->currenttexture->glowtexture);
	m.texcoords[0] = &svert[0].st[0];
	m.texcoordstep[0] = sizeof(surfvert_t);
	if (fogenabled)
	{
		m.color = &svert[0].c[0];
		m.colorstep = sizeof(surfvert_t);
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			softwaretransform(v->v, sv->v);
			if (r_waterripple.value)
				sv->v[2] += r_waterripple.value * (1.0f / 64.0f) * turbsin[(int)((v->v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v->v[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255];
			sv->st[0] = (v->st[0] + turbsin[(int)((v->st[1]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
			sv->st[1] = (v->st[1] + turbsin[(int)((v->st[0]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
			VectorSubtract(sv->v, r_origin, diff);
			ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
			sv->c[0] = m.cr * ifog;
			sv->c[1] = m.cg * ifog;
			sv->c[2] = m.cb * ifog;
			sv->c[3] = m.ca;
		}
	}
	else
	{
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			softwaretransform(v->v, sv->v);
			if (r_waterripple.value)
				sv->v[2] += r_waterripple.value * (1.0f / 64.0f) * turbsin[(int)((v->v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v->v[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255];
			sv->st[0] = (v->st[0] + turbsin[(int)((v->st[1]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
			sv->st[1] = (v->st[1] + turbsin[(int)((v->st[0]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Water_Pass_Fog(msurface_t *s)
{
	int				i;
	float			alpha;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;
	vec3_t			diff;
	alpha = currentrenderentity->alpha * (s->flags & SURF_DRAWNOALPHA ? 1 : r_wateralpha.value);

	memset(&m, 0, sizeof(m));
	m.transparent = alpha != 1 || s->currenttexture->fogtexture != NULL;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.color = &svert[0].c[0];
	m.colorstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->fogtexture);
	m.texcoords[0] = &svert[0].st[0];
	m.texcoordstep[0] = sizeof(surfvert_t);

	for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
	{
		softwaretransform(v->v, sv->v);
		if (r_waterripple.value)
			sv->v[2] += r_waterripple.value * (1.0f / 64.0f) * turbsin[(int)((v->v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v->v[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255];
		if (m.tex[0])
		{
			sv->st[0] = (v->st[0] + turbsin[(int)((v->st[1]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
			sv->st[1] = (v->st[1] + turbsin[(int)((v->st[0]*0.125f+cl.time) * TURBSCALE) & 255]) * (1.0f / 64.0f);
		}
		VectorSubtract(sv->v, r_origin, diff);
		sv->c[0] = fogcolor[0];
		sv->c[1] = fogcolor[1];
		sv->c[2] = fogcolor[2];
		sv->c[3] = alpha * exp(fogdensity/DotProduct(diff, diff));
	}
	R_Mesh_Draw(&m);
}

static int RSurfShader_Water(int stage, msurface_t *s)
{
	switch(stage)
	{
	case 0:
		RSurfShader_Water_Pass_Base(s);
		return false;
	case 1:
		if (s->currenttexture->glowtexture)
			RSurfShader_Water_Pass_Glow(s);
		return false;
	case 2:
		if (fogenabled && (s->flags & SURF_DRAWNOALPHA))
		{
			RSurfShader_Water_Pass_Fog(s);
			return false;
		}
		else
			return true;
	default:
		return true;
	}
}

static void RSurfShader_Wall_Pass_BaseMTex(msurface_t *s)
{
	int				i;
	float			diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.transparent = false;
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.cr = 1;
	if (lighthalf)
		m.cr *= 2;
	if (gl_combine.integer)
		m.cr *= 4;
	m.cg = m.cr;
	m.cb = m.cr;
	m.ca = currentrenderentity->alpha;
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.tex[1] = R_GetTexture(s->lightmaptexture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoords[1] = &s->mesh.vertex->uv[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	m.texcoordstep[1] = sizeof(surfvertex_t);
	if (fogenabled)
	{
		m.color = &svert[0].c[0];
		m.colorstep = sizeof(surfvert_t);
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				softwaretransform(v->v, sv->v);
				VectorSubtract(sv->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				VectorSubtract(v->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
	}
	else
	{
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
				softwaretransform(v->v, sv->v);
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_BaseTexture(msurface_t *s)
{
	int				i;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	if (lighthalf)
	{
		m.cr = 2;
		m.cg = 2;
		m.cb = 2;
	}
	else
	{
		m.cr = 1;
		m.cg = 1;
		m.cb = 1;
	}
	m.ca = 1;
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	if (softwaretransform_complexity)
	{
		m.vertex = &svert[0].v[0];
		m.vertexstep = sizeof(surfvert_t);
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			softwaretransform(v->v, sv->v);
	}
	else
	{
		m.vertex = &s->mesh.vertex->v[0];
		m.vertexstep = sizeof(surfvertex_t);
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_BaseLightmap(msurface_t *s)
{
	int				i;
	float			diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_ZERO;
	m.blendfunc2 = GL_SRC_COLOR;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.cr = 1;
	if (lighthalf)
		m.cr *= 2.0f;
	m.cg = m.cr;
	m.cb = m.cr;
	m.ca = 1;
	m.tex[0] = R_GetTexture(s->lightmaptexture);
	m.texcoords[0] = &s->mesh.vertex->uv[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	if (fogenabled)
	{
		m.color = &svert[0].c[0];
		m.colorstep = sizeof(surfvert_t);
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				softwaretransform(v->v, sv->v);
				VectorSubtract(sv->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				VectorSubtract(v->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
	}
	else
	{
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
				softwaretransform(v->v, sv->v);
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_BaseVertex(msurface_t *s)
{
	int				i, size3;
	float			c[3], base[3], scale, diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;
	byte			*lm;

	size3 = ((s->extents[0]>>4)+1)*((s->extents[1]>>4)+1)*3;

	base[0] = base[1] = base[2] = r_ambient.value * (1.0f / 128.0f);

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.transparent = false;
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.color = &svert[0].c[0];
	m.colorstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
	{
		softwaretransform(v->v, sv->v);
		VectorCopy(base, c);
		if (s->styles[0] != 255)
		{
			lm = s->samples + v->lightmapoffset;
			scale = d_lightstylevalue[s->styles[0]] * (1.0f / 32768.0f);
			VectorMA(c, scale, lm, c);
			if (s->styles[1] != 255)
			{
				lm += size3;
				scale = d_lightstylevalue[s->styles[1]] * (1.0f / 32768.0f);
				VectorMA(c, scale, lm, c);
				if (s->styles[2] != 255)
				{
					lm += size3;
					scale = d_lightstylevalue[s->styles[2]] * (1.0f / 32768.0f);
					VectorMA(c, scale, lm, c);
					if (s->styles[3] != 255)
					{
						lm += size3;
						scale = d_lightstylevalue[s->styles[3]] * (1.0f / 32768.0f);
						VectorMA(c, scale, lm, c);
					}
				}
			}
		}
		sv->c[0] = c[0];
		sv->c[1] = c[1];
		sv->c[2] = c[2];
		sv->c[3] = currentrenderentity->alpha;
	}
	if (s->dlightframe == r_framecount)
		RSurf_Light(s->dlightbits, m.numverts);
	if (fogenabled)
	{
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			VectorSubtract(sv->v, r_origin, diff);
			ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
			sv->c[0] *= ifog;
			sv->c[1] *= ifog;
			sv->c[2] *= ifog;
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_BaseFullbright(msurface_t *s)
{
	int				i;
	float			diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE;
	}
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
	{
		m.transparent = true;
		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		m.transparent = false;
		m.blendfunc1 = GL_ONE;
		m.blendfunc2 = GL_ZERO;
	}
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	if (fogenabled)
	{
		m.color = &svert[0].c[0];
		m.colorstep = sizeof(surfvert_t);
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			softwaretransform(v->v, sv->v);
			VectorSubtract(sv->v, r_origin, diff);
			ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
			sv->c[0] = ifog;
			sv->c[1] = ifog;
			sv->c[2] = ifog;
			sv->c[3] = currentrenderentity->alpha;
		}
	}
	else
	{
		m.cr = m.cg = m.cb = 1;
		m.ca = currentrenderentity->alpha;
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			softwaretransform(v->v, sv->v);
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_Light(msurface_t *s)
{
	int				i;
	float			diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
		m.transparent = true;
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
		m.transparent = true;
	else
		m.transparent = false;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.vertex = &svert[0].v[0];
	m.vertexstep = sizeof(surfvert_t);
	m.color = &svert[0].c[0];
	m.colorstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->texture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
	{
		softwaretransform(v->v, sv->v);
		sv->c[0] = 0;
		sv->c[1] = 0;
		sv->c[2] = 0;
		sv->c[3] = currentrenderentity->alpha;
	}
	if (RSurf_Light(s->dlightbits, m.numverts))
	{
		if (fogenabled)
		{
			for (i = 0, sv = svert;i < m.numverts;i++, sv++)
			{
				VectorSubtract(sv->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] *= ifog;
				sv->c[1] *= ifog;
				sv->c[2] *= ifog;
			}
		}
		R_Mesh_Draw(&m);
	}
}

static void RSurfShader_Wall_Pass_Glow(msurface_t *s)
{
	int				i;
	float			diff[3], ifog;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
		m.transparent = true;
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
		m.transparent = true;
	else
		m.transparent = false;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = currentrenderentity->alpha;
	m.tex[0] = R_GetTexture(s->currenttexture->glowtexture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	if (fogenabled)
	{
		m.color = &svert[0].c[0];
		m.colorstep = sizeof(surfvert_t);
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				softwaretransform(v->v, sv->v);
				VectorSubtract(sv->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
			{
				VectorSubtract(v->v, r_origin, diff);
				ifog = 1 - exp(fogdensity/DotProduct(diff, diff));
				sv->c[0] = m.cr * ifog;
				sv->c[1] = m.cg * ifog;
				sv->c[2] = m.cb * ifog;
				sv->c[3] = m.ca;
			}
		}
	}
	else
	{
		if (softwaretransform_complexity)
		{
			m.vertex = &svert[0].v[0];
			m.vertexstep = sizeof(surfvert_t);
			for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
				softwaretransform(v->v, sv->v);
		}
		else
		{
			m.vertex = &s->mesh.vertex->v[0];
			m.vertexstep = sizeof(surfvertex_t);
		}
	}
	R_Mesh_Draw(&m);
}

static void RSurfShader_Wall_Pass_Fog(msurface_t *s)
{
	int				i;
	surfvertex_t	*v;
	surfvert_t		*sv;
	rmeshinfo_t		m;
	vec3_t			diff;

	memset(&m, 0, sizeof(m));
	if (currentrenderentity->effects & EF_ADDITIVE)
		m.transparent = true;
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1)
		m.transparent = true;
	else
		m.transparent = false;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.numtriangles = s->mesh.numtriangles;
	m.numverts = s->mesh.numverts;
	m.index = s->mesh.index;
	m.color = &svert[0].c[0];
	m.colorstep = sizeof(surfvert_t);
	m.tex[0] = R_GetTexture(s->currenttexture->fogtexture);
	m.texcoords[0] = &s->mesh.vertex->st[0];
	m.texcoordstep[0] = sizeof(surfvertex_t);
	if (softwaretransform_complexity)
	{
		m.vertex = &svert[0].v[0];
		m.vertexstep = sizeof(surfvert_t);
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			softwaretransform(v->v, sv->v);
			VectorSubtract(sv->v, r_origin, diff);
			sv->c[0] = fogcolor[0];
			sv->c[1] = fogcolor[1];
			sv->c[2] = fogcolor[2];
			sv->c[3] = currentrenderentity->alpha * exp(fogdensity/DotProduct(diff,diff));
		}
	}
	else
	{
		m.vertex = &s->mesh.vertex->v[0];
		m.vertexstep = sizeof(surfvertex_t);
		for (i = 0, sv = svert, v = s->mesh.vertex;i < m.numverts;i++, sv++, v++)
		{
			VectorSubtract(v->v, r_origin, diff);
			sv->c[0] = fogcolor[0];
			sv->c[1] = fogcolor[1];
			sv->c[2] = fogcolor[2];
			sv->c[3] = currentrenderentity->alpha * exp(fogdensity/DotProduct(diff,diff));
		}
	}
	R_Mesh_Draw(&m);
}

static int RSurfShader_Wall_Fullbright(int stage, msurface_t *s)
{
	switch(stage)
	{
	case 0:
		RSurfShader_Wall_Pass_BaseFullbright(s);
		return false;
	case 1:
		if (s->currenttexture->glowtexture)
			RSurfShader_Wall_Pass_Glow(s);
		return false;
	default:
		return true;
	}
}

static int RSurfShader_Wall_Vertex(int stage, msurface_t *s)
{
	switch(stage)
	{
	case 0:
		RSurfShader_Wall_Pass_BaseVertex(s);
		return false;
	case 1:
		if (s->currenttexture->glowtexture)
			RSurfShader_Wall_Pass_Glow(s);
		return false;
	default:
		return true;
	}
}

static int RSurfShader_Wall_Lightmap(int stage, msurface_t *s)
{
	if (r_vertexsurfaces.integer)
	{
		switch(stage)
		{
		case 0:
			RSurfShader_Wall_Pass_BaseVertex(s);
			return false;
		case 1:
			if (s->currenttexture->glowtexture)
				RSurfShader_Wall_Pass_Glow(s);
			return false;
		default:
			return true;
		}
	}
	else if (r_multitexture.integer)
	{
		if (r_dlightmap.integer)
		{
			switch(stage)
			{
			case 0:
				RSurfShader_Wall_Pass_BaseMTex(s);
				return false;
			case 1:
				if (s->currenttexture->glowtexture)
					RSurfShader_Wall_Pass_Glow(s);
				return false;
			default:
				return true;
			}
		}
		else
		{
			switch(stage)
			{
			case 0:
				RSurfShader_Wall_Pass_BaseMTex(s);
				return false;
			case 1:
				if (s->dlightframe == r_framecount)
					RSurfShader_Wall_Pass_Light(s);
				return false;
			case 2:
				if (s->currenttexture->glowtexture)
					RSurfShader_Wall_Pass_Glow(s);
				return false;
			default:
				return true;
			}
		}
	}
	else if (s->currenttexture->fogtexture != NULL || currentrenderentity->alpha != 1 || currentrenderentity->effects & EF_ADDITIVE)
	{
		switch(stage)
		{
		case 0:
			RSurfShader_Wall_Pass_BaseVertex(s);
			return false;
		case 1:
			if (s->currenttexture->glowtexture)
				RSurfShader_Wall_Pass_Glow(s);
			return false;
		default:
			return true;
		}
	}
	else
	{
		if (r_dlightmap.integer)
		{
			switch(stage)
			{
			case 0:
				RSurfShader_Wall_Pass_BaseTexture(s);
				return false;
			case 1:
				RSurfShader_Wall_Pass_BaseLightmap(s);
				return false;
			case 2:
				if (s->currenttexture->glowtexture)
					RSurfShader_Wall_Pass_Glow(s);
				return false;
			default:
				return true;
			}
		}
		else
		{
			switch(stage)
			{
			case 0:
				RSurfShader_Wall_Pass_BaseTexture(s);
				return false;
			case 1:
				RSurfShader_Wall_Pass_BaseLightmap(s);
				return false;
			case 2:
				if (s->dlightframe == r_framecount)
					RSurfShader_Wall_Pass_Light(s);
				return false;
			case 3:
				if (s->currenttexture->glowtexture)
					RSurfShader_Wall_Pass_Glow(s);
				return false;
			default:
				return true;
			}
		}
	}
}

static int RSurfShader_Wall_Fog(int stage, msurface_t *s)
{
	if (stage == 0 && fogenabled)
	{
		RSurfShader_Wall_Pass_Fog(s);
		return false;
	}
	else
		return true;
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

static void RSurf_Callback(void *data, void *junk)
{
	((msurface_t *)data)->visframe = r_framecount;
}

static void R_SolidWorldNode (void)
{
	if (r_viewleaf->contents != CONTENTS_SOLID)
	{
		int portalstack;
		mportal_t *p, *pstack[8192];
		msurface_t *surf, **mark, **endmark;
		mleaf_t *leaf;
		tinyplane_t plane;
		// LordHavoc: portal-passage worldnode; follows portals leading
		// outward from viewleaf, if a portal leads offscreen it is not
		// followed, in indoor maps this can often cull a great deal of
		// geometry away when pvs data is not present (useful with pvs as well)

		leaf = r_viewleaf;
		leaf->worldnodeframe = r_framecount;
		portalstack = 0;
	loc0:
		c_leafs++;

		leaf->visframe = r_framecount;

		if (leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			endmark = mark + leaf->nummarksurfaces;
			if (r_ser.integer)
			{
				do
				{
					surf = *mark++;
					// make sure surfaces are only processed once
					if (surf->worldnodeframe == r_framecount)
						continue;
					surf->worldnodeframe = r_framecount;
					if (PlaneDist(r_origin, surf->plane) < surf->plane->dist)
					{
						if (surf->flags & SURF_PLANEBACK)
						{
							VectorNegate(surf->plane->normal, plane.normal);
							plane.dist = -surf->plane->dist;
							R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, &plane);
						}
					}
					else
					{
						if (!(surf->flags & SURF_PLANEBACK))
							R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
					}
				}
				while (mark < endmark);
			}
			else
			{
				do
				{
					surf = *mark++;
					// make sure surfaces are only processed once
					if (surf->worldnodeframe == r_framecount)
						continue;
					surf->worldnodeframe = r_framecount;
					if (PlaneDist(r_origin, surf->plane) < surf->plane->dist)
					{
						if (surf->flags & SURF_PLANEBACK)
							surf->visframe = r_framecount;
					}
					else
					{
						if (!(surf->flags & SURF_PLANEBACK))
							surf->visframe = r_framecount;
					}
				}
				while (mark < endmark);
			}
		}

		// follow portals into other leafs
		p = leaf->portals;
		for (;p;p = p->next)
		{
			if (DotProduct(r_origin, p->plane.normal) < p->plane.dist)
			{
				leaf = p->past;
				if (leaf->worldnodeframe != r_framecount)
				{
					leaf->worldnodeframe = r_framecount;
					if (leaf->contents != CONTENTS_SOLID)
					{
						if (R_NotCulledBox(leaf->mins, leaf->maxs))
						{
							p->visframe = r_framecount;
							pstack[portalstack++] = p;
							goto loc0;

	loc1:
							p = pstack[--portalstack];
						}
					}
				}
			}
		}

		if (portalstack)
			goto loc1;
	}
	else
	{
		mnode_t *nodestack[8192], *node = cl.worldmodel->nodes;
		int nodestackpos = 0;
		// LordHavoc: recursive descending worldnode; if portals are not
		// available, this is a good last resort, can cull large amounts of
		// geometry, but is more time consuming than portal-passage and renders
		// things behind walls

loc2:
		if (R_NotCulledBox(node->mins, node->maxs))
		{
			if (node->numsurfaces)
			{
				if (r_ser.integer)
				{
					msurface_t *surf = cl.worldmodel->surfaces + node->firstsurface, *surfend = surf + node->numsurfaces;
					tinyplane_t plane;
					if (PlaneDiff (r_origin, node->plane) < 0)
					{
						for (;surf < surfend;surf++)
						{
							if (surf->flags & SURF_PLANEBACK)
							{
								VectorNegate(surf->plane->normal, plane.normal);
								plane.dist = -surf->plane->dist;
								R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), surf->flags & SURF_CLIPSOLID, RSurf_Callback, surf, NULL, &plane);
							}
						}
					}
					else
					{
						for (;surf < surfend;surf++)
						{
							if (!(surf->flags & SURF_PLANEBACK))
								R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), surf->flags & SURF_CLIPSOLID, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
						}
					}
				}
				else
				{
					msurface_t *surf = cl.worldmodel->surfaces + node->firstsurface, *surfend = surf + node->numsurfaces;
					if (PlaneDiff (r_origin, node->plane) < 0)
					{
						for (;surf < surfend;surf++)
						{
							if (surf->flags & SURF_PLANEBACK)
								surf->visframe = r_framecount;
						}
					}
					else
					{
						for (;surf < surfend;surf++)
						{
							if (!(surf->flags & SURF_PLANEBACK))
								surf->visframe = r_framecount;
						}
					}
				}
			}

			// recurse down the children
			if (node->children[0]->contents >= 0)
			{
				if (node->children[1]->contents >= 0)
				{
					if (nodestackpos < 8192)
						nodestack[nodestackpos++] = node->children[1];
					node = node->children[0];
					goto loc2;
				}
				else
					((mleaf_t *)node->children[1])->visframe = r_framecount;
				node = node->children[0];
				goto loc2;
			}
			else
			{
				((mleaf_t *)node->children[0])->visframe = r_framecount;
				if (node->children[1]->contents >= 0)
				{
					node = node->children[1];
					goto loc2;
				}
				else if (nodestackpos > 0)
				{
					((mleaf_t *)node->children[1])->visframe = r_framecount;
					node = nodestack[--nodestackpos];
					goto loc2;
				}
			}
		}
		else if (nodestackpos > 0)
		{
			node = nodestack[--nodestackpos];
			goto loc2;
		}
	}
}

static int r_portalframecount = 0;

static void R_PVSWorldNode()
{
	int portalstack, i;
	mportal_t *p, *pstack[8192];
	msurface_t *surf, **mark, **endmark;
	mleaf_t *leaf;
	tinyplane_t plane;
	byte *worldvis;

	worldvis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	leaf = r_viewleaf;
	leaf->worldnodeframe = r_framecount;
	portalstack = 0;
loc0:
	c_leafs++;

	leaf->visframe = r_framecount;

	if (leaf->nummarksurfaces)
	{
		mark = leaf->firstmarksurface;
		endmark = mark + leaf->nummarksurfaces;
		if (r_ser.integer)
		{
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(r_origin, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
					{
						VectorNegate(surf->plane->normal, plane.normal);
						plane.dist = -surf->plane->dist;
						R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, &plane);
					}
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						R_Clip_AddPolygon((float *)surf->poly_verts, surf->poly_numverts, sizeof(float[3]), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
				}
			}
			while (mark < endmark);
		}
		else
		{
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(r_origin, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
						surf->visframe = r_framecount;
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						surf->visframe = r_framecount;
				}
			}
			while (mark < endmark);
		}
	}

	// follow portals into other leafs
	for (p = leaf->portals;p;p = p->next)
	{
		if (DotProduct(r_origin, p->plane.normal) < p->plane.dist)
		{
			leaf = p->past;
			if (leaf->worldnodeframe != r_framecount)
			{
				leaf->worldnodeframe = r_framecount;
				if (leaf->contents != CONTENTS_SOLID)
				{
					i = (leaf - cl.worldmodel->leafs) - 1;
					if (worldvis[i>>3] & (1<<(i&7)))
					{
						if (R_NotCulledBox(leaf->mins, leaf->maxs))
						{
							pstack[portalstack++] = p;
							goto loc0;

loc1:
							p = pstack[--portalstack];
						}
					}
				}
			}
		}
	}

	if (portalstack)
		goto loc1;
}

Cshader_t Cshader_wall_vertex = {{NULL, RSurfShader_Wall_Vertex, RSurfShader_Wall_Fog}, NULL};
Cshader_t Cshader_wall_lightmap = {{NULL, RSurfShader_Wall_Lightmap, RSurfShader_Wall_Fog}, NULL};
Cshader_t Cshader_wall_fullbright = {{NULL, RSurfShader_Wall_Fullbright, RSurfShader_Wall_Fog}, NULL};
Cshader_t Cshader_water = {{NULL, RSurfShader_Water, NULL}, NULL};
Cshader_t Cshader_sky = {{RSurfShader_Sky, NULL, NULL}, NULL};

int Cshader_count = 5;
Cshader_t *Cshaders[5] =
{
	&Cshader_wall_vertex,
	&Cshader_wall_lightmap,
	&Cshader_wall_fullbright,
	&Cshader_water,
	&Cshader_sky
};

void R_PrepareSurfaces(void)
{
	int i;
	texture_t *t;
	model_t *model;
	msurface_t *surf;

	for (i = 0;i < Cshader_count;i++)
		Cshaders[i]->chain = NULL;

	model = currentrenderentity->model;

	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		surf = model->modelsortedsurfaces[i];
		if (surf->visframe == r_framecount)
		{
			if (surf->insertframe != r_framecount)
			{
				surf->insertframe = r_framecount;
				c_faces++;
				// manually inlined R_TextureAnimation
				//t = R_TextureAnimation(surf->texinfo->texture);
				t = surf->texinfo->texture;
				if (t->alternate_anims != NULL && currentrenderentity->frame)
					t = t->alternate_anims;
				if (t->anim_total >= 2)
					t = t->anim_frames[(int)(cl.time * 5.0f) % t->anim_total];
				surf->currenttexture = t;
			}

			surf->chain = surf->shader->chain;
			surf->shader->chain = surf;
		}
	}
}

void R_DrawSurfaces (int type)
{
	int			i, stage;
	msurface_t	*surf;
	Cshader_t	*shader;

	for (i = 0;i < Cshader_count;i++)
	{
		shader = Cshaders[i];
		if (shader->chain && shader->shaderfunc[type])
			for (stage = 0;stage < 1000;stage++)
				for (surf = shader->chain;surf;surf = surf->chain)
					if (shader->shaderfunc[type](stage, surf))
						goto done;
done:;
	}
}

void R_DrawSurfacesAll (void)
{
	R_DrawSurfaces(SHADERSTAGE_SKY);
	R_DrawSurfaces(SHADERSTAGE_NORMAL);
	R_DrawSurfaces(SHADERSTAGE_FOG);
}

static float portalpointbuffer[256][3];

void R_DrawPortals(void)
{
	int drawportals, i;
//	mleaf_t *leaf, *endleaf;
	mportal_t *portal, *endportal;
	mvertex_t *point/*, *endpoint*/;
	rmeshinfo_t m;
	drawportals = r_drawportals.integer;
	if (drawportals < 1)
		return;
	/*
	leaf = cl.worldmodel->leafs;
	endleaf = leaf + cl.worldmodel->numleafs;
	for (;leaf < endleaf;leaf++)
	{
		if (leaf->visframe == r_framecount && leaf->portals)
		{
			i = leaf - cl.worldmodel->leafs;
			r = (i & 0x0007) << 5;
			g = (i & 0x0038) << 2;
			b = (i & 0x01C0) >> 1;
			portal = leaf->portals;
			while (portal)
			{
				transpolybegin(0, 0, 0, TPOLYTYPE_ALPHA);
				point = portal->points + portal->numpoints - 1;
				endpoint = portal->points;
				for (;point >= endpoint;point--)
					transpolyvertub(point->position[0], point->position[1], point->position[2], 0, 0, r, g, b, 32);
				transpolyend();
				portal = portal->next;
			}
		}
	}
	*/
	memset(&m, 0, sizeof(m));
	m.transparent = true;
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.vertex = &portalpointbuffer[0][0];
	m.vertexstep = sizeof(float[3]);
	m.ca = 0.125;
	for (portal = cl.worldmodel->portals, endportal = portal + cl.worldmodel->numportals;portal < endportal;portal++)
	{
		if (portal->visframe == r_portalframecount)
		{
			if (portal->numpoints <= 256)
			{
				i = portal - cl.worldmodel->portals;
				m.cr = ((i & 0x0007) >> 0) * (1.0f / 7.0f);
				m.cg = ((i & 0x0038) >> 3) * (1.0f / 7.0f);
				m.cb = ((i & 0x01C0) >> 6) * (1.0f / 7.0f);
				point = portal->points;
				if (PlaneDiff(r_origin, (&portal->plane)) > 0)
				{
					for (i = portal->numpoints - 1;i >= 0;i--)
						VectorCopy(point[i].position, portalpointbuffer[i]);
				}
				else
				{
					for (i = 0;i < portal->numpoints;i++)
						VectorCopy(point[i].position, portalpointbuffer[i]);
				}
				R_Mesh_DrawPolygon(&m, portal->numpoints);
			}
		}
	}
}

void R_SetupForBModelRendering(void)
{
	int			i;
	msurface_t	*s;
	model_t		*model;
	vec3_t		modelorg;

	// because bmodels can be reused, we have to decide which things to render
	// from scratch every time

	model = currentrenderentity->model;

	softwaretransformforentity (currentrenderentity);
	softwareuntransform(r_origin, modelorg);

	for (i = 0;i < model->nummodelsurfaces;i++)
	{
		s = model->modelsortedsurfaces[i];
		if (((s->flags & SURF_PLANEBACK) == 0) == (PlaneDiff(modelorg, s->plane) >= 0))
			s->visframe = r_framecount;
		else
			s->visframe = -1;
		s->worldnodeframe = -1;
		s->lightframe = -1;
		s->dlightframe = -1;
		s->insertframe = -1;
	}
}

void R_SetupForWorldRendering(void)
{
	// there is only one instance of the world, but it can be rendered in
	// multiple stages

	currentrenderentity = &cl_entities[0].render;
	softwaretransformidentity();
}

static void R_SurfMarkLights (void)
{
	int			i;
	msurface_t	*s;

	if (r_dynamic.integer)
		R_MarkLights();

	if (!r_vertexsurfaces.integer)
	{
		for (i = 0;i < currentrenderentity->model->nummodelsurfaces;i++)
		{
			s = currentrenderentity->model->modelsortedsurfaces[i];
			if (s->visframe == r_framecount && s->lightmaptexture != NULL)
			{
				if (s->cached_dlight
				 || s->cached_ambient != r_ambient.value
				 || s->cached_lightscalebit != lightscalebit)
					R_BuildLightMap(s, false); // base lighting changed
				else if (r_dynamic.integer)
				{
					if  (s->styles[0] != 255 && (d_lightstylevalue[s->styles[0]] != s->cached_light[0]
					 || (s->styles[1] != 255 && (d_lightstylevalue[s->styles[1]] != s->cached_light[1]
					 || (s->styles[2] != 255 && (d_lightstylevalue[s->styles[2]] != s->cached_light[2]
					 || (s->styles[3] != 255 && (d_lightstylevalue[s->styles[3]] != s->cached_light[3]))))))))
					//if (s->cached_light[0] != d_lightstylevalue[s->styles[0]]
					// || s->cached_light[1] != d_lightstylevalue[s->styles[1]]
					// || s->cached_light[2] != d_lightstylevalue[s->styles[2]]
					// || s->cached_light[3] != d_lightstylevalue[s->styles[3]])
						R_BuildLightMap(s, false); // base lighting changed
					else if (s->dlightframe == r_framecount && r_dlightmap.integer)
						R_BuildLightMap(s, true); // only dlights
				}
			}
		}
	}
}

void R_MarkWorldLights(void)
{
	R_SetupForWorldRendering();
	R_SurfMarkLights();
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	R_SetupForWorldRendering();

	if (r_viewleaf->contents == CONTENTS_SOLID || r_novis.integer || r_viewleaf->compressed_vis == NULL)
		R_SolidWorldNode ();
	else
		R_PVSWorldNode ();
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelSky (void)
{
	R_SetupForBModelRendering();

	R_PrepareSurfaces();
	R_DrawSurfaces(SHADERSTAGE_SKY);
}

void R_DrawBrushModelNormal (void)
{
	c_bmodels++;

	// have to flush queue because of possible lightmap reuse
	R_Mesh_Render();

	R_SetupForBModelRendering();

	R_SurfMarkLights();

	R_PrepareSurfaces();

	if (!skyrendermasked)
		R_DrawSurfaces(SHADERSTAGE_SKY);
	R_DrawSurfaces(SHADERSTAGE_NORMAL);
	R_DrawSurfaces(SHADERSTAGE_FOG);
}
