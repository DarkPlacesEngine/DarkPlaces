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

int		lightmap_textures;

#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256
// LordHavoc: increased lightmap limit from 64 to 1024
#define	MAX_LIGHTMAPS	1024
#define LIGHTMAPSIZE	(BLOCK_WIDTH*BLOCK_HEIGHT*4)

int			active_lightmaps;

short allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

byte *lightmaps[MAX_LIGHTMAPS];
short lightmapupdate[MAX_LIGHTMAPS][2];

signed int blocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3]; // LordHavoc: *3 for colored lighting

byte templight[BLOCK_WIDTH*BLOCK_HEIGHT*4];

int lightmapalign, lightmapalignmask; // LordHavoc: align texsubimage updates on 4 byte boundaries
cvar_t gl_lightmapalign = {0, "gl_lightmapalign", "4"}; // align texsubimage updates on 4 byte boundaries
cvar_t gl_lightmaprgba = {0, "gl_lightmaprgba", "1"};
cvar_t gl_nosubimagefragments = {0, "gl_nosubimagefragments", "0"};
cvar_t gl_nosubimage = {0, "gl_nosubimage", "0"};
cvar_t r_ambient = {0, "r_ambient", "0"};
cvar_t gl_vertex = {0, "gl_vertex", "0"};
cvar_t r_dlightmap = {CVAR_SAVE, "r_dlightmap", "1"};
cvar_t r_drawportals = {0, "r_drawportals", "0"};
cvar_t r_testvis = {0, "r_testvis", "0"};

qboolean lightmaprgba, nosubimagefragments, nosubimage;
int lightmapbytes;

int wateralpha;

void gl_surf_start(void)
{
}

void gl_surf_shutdown(void)
{
}

void gl_surf_newmap(void)
{
}

void GL_Surf_Init(void)
{
	int i;
	for (i = 0;i < MAX_LIGHTMAPS;i++)
		lightmaps[i] = NULL;
	Cvar_RegisterVariable(&gl_lightmapalign);
	Cvar_RegisterVariable(&gl_lightmaprgba);
	Cvar_RegisterVariable(&gl_nosubimagefragments);
	Cvar_RegisterVariable(&gl_nosubimage);
	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&gl_vertex);
	Cvar_RegisterVariable(&r_dlightmap);
	Cvar_RegisterVariable(&r_drawportals);
	Cvar_RegisterVariable(&r_testvis);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

int dlightdivtable[32768];

/*
	R_AddDynamicLights
*/
int R_AddDynamicLights (msurface_t *surf)
{
	int         sdtable[18], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, red, green, blue, lit, dist2, impacts, impactt;
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

	if (!dlightdivtable[1])
	{
		dlightdivtable[0] = 4194304;
		for (s = 1; s < 32768; s++)
			dlightdivtable[s] = 4194304 / (s << 7);
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits[lnum >> 5] & (1 << (lnum & 31))))
			continue;					// not lit by this light

		VectorSubtract (cl_dlights[lnum].origin, currentrenderentity->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;

		// for comparisons to minimum acceptable light
		maxdist = (int) ((cl_dlights[lnum].radius * cl_dlights[lnum].radius));

		// clamp radius to avoid exceeding 32768 entry division table
		if (maxdist > 4194304)
			maxdist = 4194304;

		dist2 = dist * dist;
		if (dist2 >= maxdist)
			continue;

		impact[0] = cl_dlights[lnum].origin[0] - surf->plane->normal[0] * dist;
		impact[1] = cl_dlights[lnum].origin[1] - surf->plane->normal[1] * dist;
		impact[2] = cl_dlights[lnum].origin[2] - surf->plane->normal[2] * dist;

		impacts = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		impactt = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];

		s = bound(0, impacts, smax * 16) - impacts;
		t = bound(0, impactt, tmax * 16) - impactt;
		i = s * s + t * t + dist2;
		if (i > maxdist)
			continue;

		// reduce calculations
		for (s = 0, i = impacts; s < smax; s++, i -= 16)
			sdtable[s] = i * i + dist2 + LIGHTOFFSET;

		maxdist3 = maxdist - (int) (dist * dist);

		// convert to 8.8 blocklights format and scale up by radius
		red = cl_dlights[lnum].color[0] * maxdist;
		green = cl_dlights[lnum].color[1] * maxdist;
		blue = cl_dlights[lnum].color[2] * maxdist;
		bl = blocklights;

		i = impactt;
		for (t = 0; t < tmax; t++, i -= 16)
		{
			td = i * i;
			// make sure some part of it is visible on this line
			if (td < maxdist3)
			{
				maxdist2 = maxdist - td;
				for (s = 0; s < smax; s++)
				{
					if (sdtable[s] < maxdist2)
					{
						k = dlightdivtable[(sdtable[s] + td) >> 7];
						bl[0] += (red   * k) >> 9;
						bl[1] += (green * k) >> 9;
						bl[2] += (blue  * k) >> 9;
						lit = true;
					}
					bl += 3;
				}
			}
			else // skip line
				bl += smax * 3;
		}
	}
	return lit;
}


void R_ConvertLightmap (int *in, byte *out, int width, int height, int stride)
{
	int i, j, shift;
	stride -= (width*lightmapbytes);
	// deal with lightmap brightness scale
	shift = 7 + lightscalebit;
	if (lightmaprgba)
	{
		for (i = 0;i < height;i++, out += stride)
		{
			for (j = 0;j < width;j++, in += 3, out += 4)
			{
				out[0] = min(in[0] >> shift, 255);
				out[1] = min(in[1] >> shift, 255);
				out[2] = min(in[2] >> shift, 255);
				out[3] = 255;
			}
		}
	}
	else
	{
		for (i = 0;i < height;i++, out += stride)
		{
			for (j = 0;j < width;j++, in += 3, out += 3)
			{
				out[0] = min(in[0] >> shift, 255);
				out[1] = min(in[1] >> shift, 255);
				out[2] = min(in[2] >> shift, 255);
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
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride, int dlightchanged)
{
	int		smax, tmax;
	int		i, j, size, size3;
	byte	*lightmap;
	int		scale;
	int		maps;
	int		*bl;

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

		if (r_dlightmap.value && surf->dlightframe == r_framecount)
		{
			if ((surf->cached_dlight = R_AddDynamicLights(surf)))
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

	R_ConvertLightmap(blocklights, dest, smax, tmax, stride);
}

void R_UpdateLightmap(msurface_t *s, int lnum, int dlightschanged)
{
	int smax, tmax;
	// upload the new lightmap texture fragment
	if(r_upload.value)
		glBindTexture(GL_TEXTURE_2D, lightmap_textures + lnum);
	if (nosubimage || nosubimagefragments)
	{
		if (lightmapupdate[lnum][0] > s->light_t)
			lightmapupdate[lnum][0] = s->light_t;
		if (lightmapupdate[lnum][1] < (s->light_t + ((s->extents[1]>>4)+1)))
			lightmapupdate[lnum][1] = (s->light_t + ((s->extents[1]>>4)+1));
		if (lightmaprgba)
			R_BuildLightMap (s, lightmaps[s->lightmaptexturenum] + (s->light_t * BLOCK_WIDTH + s->light_s) * 4, BLOCK_WIDTH * 4, false);
		else
			R_BuildLightMap (s, lightmaps[s->lightmaptexturenum] + (s->light_t * BLOCK_WIDTH + s->light_s) * 3, BLOCK_WIDTH * 3, false);
	}
	else
	{
		smax = ((s->extents[0]>>4)+lightmapalign) & lightmapalignmask;
		tmax = (s->extents[1]>>4)+1;
		if (lightmaprgba)
		{
			R_BuildLightMap (s, templight, smax * 4, false);
			if(r_upload.value)
				glTexSubImage2D(GL_TEXTURE_2D, 0, s->light_s, s->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
		}
		else
		{
			R_BuildLightMap (s, templight, smax * 3, false);
			if(r_upload.value)
				glTexSubImage2D(GL_TEXTURE_2D, 0, s->light_s, s->light_t, smax, tmax, GL_RGB , GL_UNSIGNED_BYTE, templight);
		}
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	if (currentrenderentity->frame && base->alternate_anims != NULL)
		base = base->alternate_anims;

	if (base->anim_total < 2)
		return base;

	return base->anim_frames[(int)(cl.time * 5.0f) % base->anim_total];
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


float	turbsin[256] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))


void UploadLightmaps(void)
{
	int i;
	if (nosubimage || nosubimagefragments)
	{
		for (i = 0;i < MAX_LIGHTMAPS;i++)
		{
			if (lightmapupdate[i][0] < lightmapupdate[i][1])
			{
				if(r_upload.value)
				{
					glBindTexture(GL_TEXTURE_2D, lightmap_textures + i);
					if (nosubimage)
					{
						if (lightmaprgba)
							glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i]);
						else
							glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmaps[i]);
					}
					else
					{
						if (lightmaprgba)
							glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lightmapupdate[i][0], BLOCK_WIDTH, lightmapupdate[i][1] - lightmapupdate[i][0], GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i] + (BLOCK_WIDTH * 4 * lightmapupdate[i][0]));
						else
							glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lightmapupdate[i][0], BLOCK_WIDTH, lightmapupdate[i][1] - lightmapupdate[i][0], GL_RGB, GL_UNSIGNED_BYTE, lightmaps[i] + (BLOCK_WIDTH * 3 * lightmapupdate[i][0]));
					}
				}
			}
			lightmapupdate[i][0] = BLOCK_HEIGHT;
			lightmapupdate[i][1] = 0;
		}
	}
}

float	wvert[1024*6]; // used by the following functions

void RSurf_DrawSky(msurface_t *s, int transform)
{
	glpoly_t *p;
	int i;
	float *v;

	// LordHavoc: HalfLife maps have freaky skypolys...
	if (hlbsp)
		return;

	for (p=s->polys ; p ; p=p->next)
	{
		if (currentskypoly < MAX_SKYPOLYS && currentskyvert + p->numverts <= MAX_SKYVERTS)
		{
			skypoly[currentskypoly].firstvert = currentskyvert;
			skypoly[currentskypoly++].verts = p->numverts;
			if (transform)
			{
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
				{
					softwaretransform(v, skyvert[currentskyvert].v);
					currentskyvert++;
				}
			}
			else
			{
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
				{
					VectorCopy(v, skyvert[currentskyvert].v);
					currentskyvert++;
				}
			}
		}
	}
}

int RSurf_Light(int *dlightbits, glpoly_t *polys)
{
	float		cr, cg, cb, radius, radius2, f, *v, *wv;
	int			i, a, b, lit = false;
	unsigned int c, d;
	dlight_t	*light;
	vec_t		*lightorigin;
	glpoly_t	*p;
	for (a = 0;a < 8;a++)
	{
		if ((c = dlightbits[a]))
		{
			for (b = 0, d = 1;c;b++, d <<= 1)
			{
				if (c & d)
				{
					c -= d;
					light = &cl_dlights[a * 32 + b];
					lightorigin = light->origin;
					cr = light->color[0];
					cg = light->color[1];
					cb = light->color[2];
					radius = light->radius*light->radius;
					radius2 = radius * 256.0f;
					wv = wvert;
					for (p = polys;p;p = p->next)
					{
						for (i = 0, v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
						{
							f = VectorDistance2(wv, lightorigin);
							if (f < radius)
							{
								f = radius2 / (f + LIGHTOFFSET);
								wv[3] += cr * f;
								wv[4] += cg * f;
								wv[5] += cb * f;
								lit = true;
							}
							wv += 6;
						}
					}
				}
			}
		}
	}
	return lit;
}

void RSurf_DrawWater(msurface_t *s, texture_t *t, int transform, int alpha)
{
	int		i;
	float	os = turbsin[(int)(cl.time * TURBSCALE) & 255], ot = turbsin[(int)(cl.time * TURBSCALE + 96.0) & 255];
	glpoly_t *p;
	float	*v;
	// FIXME: make fog texture if water texture is transparent?

	if (s->dlightframe != r_framecount)
	{
		vec3_t temp;
		// LordHavoc: fast path for no vertex lighting cases
		if (transform)
		{
			if (r_waterripple.value)
			{
				for (p=s->polys ; p ; p=p->next)
				{
					transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, TPOLYTYPE_ALPHA);
					for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
					{
						softwaretransform(v, temp);
						transpolyvert(temp[0], temp[1], temp[2] + r_waterripple.value * turbsin[(int)((temp[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((temp[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * (1.0f / 64.0f), (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
					}
					transpolyend();
				}
			}
			else
			{
				for (p=s->polys ; p ; p=p->next)
				{
					transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, TPOLYTYPE_ALPHA);
					for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
					{
						softwaretransform(v, temp);
						transpolyvert(temp[0], temp[1], temp[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
					}
					transpolyend();
				}
			}
		}
		else
		{
			if (r_waterripple.value)
			{
				for (p=s->polys ; p ; p=p->next)
				{
					transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, TPOLYTYPE_ALPHA);
					for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
						transpolyvert(v[0], v[1], v[2] + r_waterripple.value * turbsin[(int)((v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * (1.0f / 64.0f), (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
					transpolyend();
				}
			}
			else
			{
				for (p=s->polys ; p ; p=p->next)
				{
					transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, TPOLYTYPE_ALPHA);
					for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
						transpolyvert(v[0], v[1], v[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
					transpolyend();
				}
			}
		}
	}
	else
	{
		float *wv;
		wv = wvert;
		for (p = s->polys;p;p = p->next)
		{
			for (i = 0, v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
			{
				if (transform)
					softwaretransform(v, wv);
				else
					VectorCopy(v, wv);
				if (r_waterripple.value)
					wv[2] += r_waterripple.value * turbsin[(int)((wv[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((wv[1]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * (1.0f / 64.0f);
				wv[3] = wv[4] = wv[5] = 128.0f;
				wv += 6;
			}
		}
		if (s->dlightframe == r_framecount)
			RSurf_Light(s->dlightbits, s->polys);
		wv = wvert;
		for (p=s->polys ; p ; p=p->next)
		{
			transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), wv[3], wv[4], wv[5], alpha);
			transpolyend();
		}
	}
}

void RSurf_DrawWall(msurface_t *s, texture_t *t, int transform)
{
	int		i, lit = false, polys = 0, verts = 0;
	float	*v;
	glpoly_t *p;
	wallpoly_t *wp;
	wallvert_t *out;
	wallvertcolor_t *outcolor;
	// check for lightmap modification
	if (s->cached_dlight
	 || r_ambient.value != s->cached_ambient
	 || lightscalebit != s->cached_lightscalebit
	 || (r_dynamic.value
	 && (d_lightstylevalue[s->styles[0]] != s->cached_light[0]
	 ||  d_lightstylevalue[s->styles[1]] != s->cached_light[1]
	 ||  d_lightstylevalue[s->styles[2]] != s->cached_light[2]
	 ||  d_lightstylevalue[s->styles[3]] != s->cached_light[3])))
		R_UpdateLightmap(s, s->lightmaptexturenum, false); // base lighting changed
	else if (r_dynamic.value && r_dlightmap.value && s->dlightframe == r_framecount)
		R_UpdateLightmap(s, s->lightmaptexturenum, true); // only dlights

	if (s->dlightframe != r_framecount || r_dlightmap.value)
	{
		// LordHavoc: fast path version for no vertex lighting cases
		out = &wallvert[currentwallvert];
		for (p = s->polys;p;p = p->next)
		{
			if ((currentwallpoly >= MAX_WALLPOLYS) || (currentwallvert+p->numverts > MAX_WALLVERTS))
				return;
			wp = &wallpoly[currentwallpoly++];
			wp->texnum = (unsigned short) R_GetTexture(t->texture);
			wp->lighttexnum = (unsigned short) (lightmap_textures + s->lightmaptexturenum);
			wp->glowtexnum = (unsigned short) R_GetTexture(t->glowtexture);
			wp->firstvert = currentwallvert;
			wp->numverts = p->numverts;
			wp->lit = false;
			wp++;
			currentwallvert += p->numverts;
			v = p->verts[0];
			if (transform)
			{
				for (i = 0;i < p->numverts;i++, v += VERTEXSIZE, out++)
				{
					softwaretransform(v, out->vert);
					out->vert[3] = v[3];
					out->vert[4] = v[4];
					out->vert[5] = v[5];
					out->vert[6] = v[6];
				}
			}
			else
			{
				/*
				for (i = 0;i < p->numverts;i++, v += VERTEXSIZE, out++)
				{
					VectorCopy(v, out->vert);
					out->vert[3] = v[3];
					out->vert[4] = v[4];
					out->vert[5] = v[5];
					out->vert[6] = v[6];
				}
				*/
				memcpy(out, v, sizeof(vec_t) * VERTEXSIZE * p->numverts);
				out += p->numverts;
			}
		}
	}
	else
	{
		float *wv;
		wv = wvert;
		for (p = s->polys;p;p = p->next)
		{
			for (i = 0, v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
			{
				if (transform)
					softwaretransform(v, wv);
				else
					VectorCopy(v, wv);
				wv[3] = wv[4] = wv[5] = 0.0f;
				wv += 6;
			}
			verts += p->numverts;
			polys++;
		}
		if ((currentwallpoly + polys > MAX_WALLPOLYS) || (currentwallvert+verts > MAX_WALLVERTS))
			return;
		if ((!r_dlightmap.value) && s->dlightframe == r_framecount)
			lit = RSurf_Light(s->dlightbits, s->polys);
		wv = wvert;
		wp = &wallpoly[currentwallpoly];
		out = &wallvert[currentwallvert];
		outcolor = &wallvertcolor[currentwallvert];
		currentwallpoly += polys;
		for (p = s->polys;p;p = p->next)
		{
			v = p->verts[0];
			wp->texnum = (unsigned short) R_GetTexture(t->texture);
			wp->lighttexnum = (unsigned short) (lightmap_textures + s->lightmaptexturenum);
			wp->glowtexnum = (unsigned short) R_GetTexture(t->glowtexture);
			wp->firstvert = currentwallvert;
			wp->numverts = p->numverts;
			wp->lit = lit;
			wp++;
			currentwallvert += p->numverts;
			for (i = 0;i < p->numverts;i++, v += VERTEXSIZE, wv += 6, out++, outcolor++)
			{
				if (lit)
				{
					if (lighthalf)
					{
						outcolor->r = (byte) (bound(0, (int) wv[3] >> 1, 255));
						outcolor->g = (byte) (bound(0, (int) wv[4] >> 1, 255));
						outcolor->b = (byte) (bound(0, (int) wv[5] >> 1, 255));
						outcolor->a = 255;
					}
					else
					{
						outcolor->r = (byte) (bound(0, (int) wv[3], 255));
						outcolor->g = (byte) (bound(0, (int) wv[4], 255));
						outcolor->b = (byte) (bound(0, (int) wv[5], 255));
						outcolor->a = 255;
					}
				}
				out->vert[0] = wv[0];
				out->vert[1] = wv[1];
				out->vert[2] = wv[2];
				out->vert[3] = v[3];
				out->vert[4] = v[4];
				out->vert[5] = v[5];
				out->vert[6] = v[6];
			}
		}
	}
}

// LordHavoc: transparent brush models
void RSurf_DrawWallVertex(msurface_t *s, texture_t *t, int transform, int isbmodel)
{
	int i, alpha, size3;
	float *v, *wv, scale;
	glpoly_t *p;
	byte *lm;
	alpha = (int) (currentrenderentity->alpha * 255.0f);
	size3 = ((s->extents[0]>>4)+1)*((s->extents[1]>>4)+1)*3; // *3 for colored lighting
	wv = wvert;
	for (p = s->polys;p;p = p->next)
	{
		for (i = 0, v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
		{
			if (transform)
				softwaretransform(v, wv);
			else
				VectorCopy(v, wv);
			wv[3] = wv[4] = wv[5] = r_ambient.value * 2.0f;
			if (s->styles[0] != 255)
			{
				lm = (byte *)((long) s->samples + (int) v[7]);
				scale = d_lightstylevalue[s->styles[0]] * (1.0f / 128.0f);wv[3] += lm[size3*0+0] * scale;wv[4] += lm[size3*0+1] * scale;wv[5] += lm[size3*0+2] * scale;
				if (s->styles[1] != 255)
				{
					scale = d_lightstylevalue[s->styles[1]] * (1.0f / 128.0f);wv[3] += lm[size3*1+0] * scale;wv[4] += lm[size3*1+1] * scale;wv[5] += lm[size3*1+2] * scale;
					if (s->styles[2] != 255)
					{
						scale = d_lightstylevalue[s->styles[2]] * (1.0f / 128.0f);wv[3] += lm[size3*2+0] * scale;wv[4] += lm[size3*2+1] * scale;wv[5] += lm[size3*2+2] * scale;
						if (s->styles[3] != 255)
						{
							scale = d_lightstylevalue[s->styles[3]] * (1.0f / 128.0f);wv[3] += lm[size3*3+0] * scale;wv[4] += lm[size3*3+1] * scale;wv[5] += lm[size3*3+2] * scale;
						}
					}
				}
			}
			wv += 6;
		}
	}
	if (s->dlightframe == r_framecount)
		RSurf_Light(s->dlightbits, s->polys);
	wv = wvert;
	if (alpha != 255 || currentrenderentity->colormod[0] != 1 || currentrenderentity->colormod[1] != 1 || currentrenderentity->colormod[2] != 1)
	{
		for (p = s->polys;p;p = p->next)
		{
			v = p->verts[0];
			transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, currentrenderentity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], wv[3] * currentrenderentity->colormod[0], wv[4] * currentrenderentity->colormod[1], wv[5] * currentrenderentity->colormod[2], alpha);
			transpolyend();
		}
	}
	else
	{
		for (p = s->polys;p;p = p->next)
		{
			v = p->verts[0];
			transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, currentrenderentity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], wv[3], wv[4], wv[5], alpha);
			transpolyend();
		}
	}
}

void R_NoVisMarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, model_t *model);

float bmverts[256*3];

int vertexworld;

// LordHavoc: disabled clipping on bmodels because they tend to intersect things sometimes
/*
void RBrushModelSurf_DoVisible(msurface_t *surf)
{
//	float *v, *bmv, *endbmv;
//	glpoly_t *p;
//	for (p = surf->polys;p;p = p->next)
//	{
//		for (v = p->verts[0], bmv = bmpoints, endbmv = bmv + p->numverts * 3;bmv < endbmv;v += VERTEXSIZE, bmv += 3)
//			softwaretransform(v, bmv);
//		if (R_Clip_Polygon(bmpoints, p->numverts, sizeof(float) * 3, surf->flags & SURF_CLIPSOLID))
			surf->visframe = r_framecount;
//	}
}
*/

/*
void RBrushModelSurf_Callback(void *data, void *data2)
{
	msurface_t *surf = data;
	texture_t *t;

	currentrenderentity = data2;
*/
	/*
	// FIXME: implement better dupe prevention in AddPolygon callback code
	if (ent->render.model->firstmodelsurface != 0)
	{
		// it's not an instanced model, so we already rely on there being only one of it (usually a valid assumption, but QC can break this)
		if (surf->visframe == r_framecount)
			return;
	}
	*/
/*
	surf->visframe = r_framecount;

	c_faces++;

	softwaretransformforbrushentity (currentrenderentity);

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
	{
		// sky and liquid don't need sorting (skypoly/transpoly)
		if (surf->flags & SURF_DRAWSKY)
			RSurf_DrawSky(surf, true);
		else
			RSurf_DrawWater(surf, R_TextureAnimation(surf->texinfo->texture), true, surf->flags & SURF_DRAWNOALPHA ? 255 : wateralpha);
	}
	else
	{
		t = R_TextureAnimation(surf->texinfo->texture);
		if (t->transparent || vertexworld || ent->render.alpha != 1 || ent->render.model->firstmodelsurface == 0 || (ent->render.effects & EF_FULLBRIGHT) || ent->render.colormod[0] != 1 || ent->render.colormod[2] != 1 || ent->render.colormod[2] != 1)
			RSurf_DrawWallVertex(surf, t, true, true);
		else
			RSurf_DrawWall(surf, t, true);
	}
}
*/

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (void)
{
	int			i/*, j*/, vertexlit, rotated, transform;
	msurface_t	*s;
	model_t		*model;
	vec3_t		org, temp, forward, right, up;
//	glpoly_t	*p;
	texture_t	*t;

	model = currentrenderentity->model;

	c_bmodels++;

	VectorSubtract (r_origin, currentrenderentity->origin, modelorg);
	rotated = false;
	transform = false;
	if (currentrenderentity->angles[0] || currentrenderentity->angles[1] || currentrenderentity->angles[2])
	{
		transform = true;
		rotated = true;
		VectorCopy (modelorg, temp);
		AngleVectors (currentrenderentity->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}
	else if (currentrenderentity->origin[0] || currentrenderentity->origin[1] || currentrenderentity->origin[2] || currentrenderentity->scale)
		transform = true;

	if (transform)
		softwaretransformforbrushentity (currentrenderentity);

	for (i = 0, s = &model->surfaces[model->firstmodelsurface];i < model->nummodelsurfaces;i++, s++)
	{
		s->visframe = -1;
		if (((s->flags & SURF_PLANEBACK) == 0) == (PlaneDiff(modelorg, s->plane) >= 0))
			s->visframe = r_framecount;
	}

// calculate dynamic lighting for bmodel if it's not an instanced model
	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		if (!cl_dlights[i].radius)
			continue;

		if (rotated)
		{
			VectorSubtract(cl_dlights[i].origin, currentrenderentity->origin, temp);
			org[0] = DotProduct (temp, forward);
			org[1] = -DotProduct (temp, right);
			org[2] = DotProduct (temp, up);
		}
		else
			VectorSubtract(cl_dlights[i].origin, currentrenderentity->origin, org);
		R_NoVisMarkLights (org, &cl_dlights[i], 1<<(i&31), i >> 5, model);
	}
	vertexlit = vertexworld || currentrenderentity->alpha != 1 || model->firstmodelsurface == 0 || (currentrenderentity->effects & EF_FULLBRIGHT) || currentrenderentity->colormod[0] != 1 || currentrenderentity->colormod[2] != 1 || currentrenderentity->colormod[2] != 1;

	// draw texture
	for (i = 0, s = &model->surfaces[model->firstmodelsurface];i < model->nummodelsurfaces;i++, s++)
	{
		if (s->visframe == r_framecount)
		{
//			R_DrawSurf(s, true, vertexlit || s->texinfo->texture->transparent);
			/*
			if (r_ser.value)
			{
				for (p = s->polys;p;p = p->next)
				{
					for (j = 0;j < p->numverts;j++)
						softwaretransform(&p->verts[j][0], bmverts + j * 3);
					R_Clip_AddPolygon(bmverts, p->numverts, 3 * sizeof(float), (s->flags & SURF_CLIPSOLID) != 0 && currentrenderentity->alpha == 1, RBrushModelSurf_Callback, s, e, NULL);
				}
			}
			else
			{
			*/
				c_faces++;
				t = R_TextureAnimation(s->texinfo->texture);
				if (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
				{
					// sky and liquid don't need sorting (skypoly/transpoly)
					if (s->flags & SURF_DRAWSKY)
						RSurf_DrawSky(s, transform);
					else
						RSurf_DrawWater(s, t, transform, s->flags & SURF_DRAWNOALPHA ? 255 : wateralpha);
				}
				else
				{
					if (t->transparent || vertexlit)
						RSurf_DrawWallVertex(s, t, transform, true);
					else
						RSurf_DrawWall(s, t, transform);
				}
			//}
		}
	}
	UploadLightmaps();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
static byte *worldvis;

void R_MarkLeaves (void)
{
	static float noviscache;
	if (r_oldviewleaf == r_viewleaf && noviscache == r_novis.value)
		return;

	r_oldviewleaf = r_viewleaf;
	noviscache = r_novis.value;

	worldvis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
}
*/

void RSurf_Callback(void *data, void *junk)
{
	((msurface_t *)data)->visframe = r_framecount;
}

/*
void RSurf_Callback(void *data, void *junk)
{
	msurface_t *surf = data;
	texture_t *t;

//	if (surf->visframe == r_framecount)
//		return;

	surf->visframe = r_framecount;

	c_faces++;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
	{
		// sky and liquid don't need sorting (skypoly/transpoly)
		if (surf->flags & SURF_DRAWSKY)
			RSurf_DrawSky(surf, false);
		else
			RSurf_DrawWater(surf, R_TextureAnimation(surf->texinfo->texture), false, surf->flags & SURF_DRAWNOALPHA ? 255 : wateralpha);
	}
	else
	{
		t = R_TextureAnimation(surf->texinfo->texture);
		if (vertexworld)
			RSurf_DrawWallVertex(surf, t, false, false);
		else
			RSurf_DrawWall(surf, t, false);
	}
}
*/

/*
mleaf_t *r_oldviewleaf;
int r_markvisframecount = 0;

void R_MarkLeaves (void)
{
	static float noviscache;
	int i, l, k, c;
	mleaf_t *leaf;
	msurface_t *surf, **mark, **endmark;
	model_t *model = cl.worldmodel;
//	mportal_t *portal;
	glpoly_t *p;
	byte	*in;
	int		row;

	// ignore testvis if the map just changed
	if (r_testvis.value && model->nodes->markvisframe == r_markvisframecount)
		return;

	if (r_oldviewleaf == r_viewleaf && noviscache == r_novis.value)
		return;

	r_oldviewleaf = r_viewleaf;
	noviscache = r_novis.value;

	if ((in = r_viewleaf->compressed_vis))
	{
		row = (model->numleafs+7)>>3;

		if (!r_testvis.value)
			r_markvisframecount++;

		// LordHavoc: mark the root node as visible, it will terminate all other ascensions
		model->nodes->markvisframe = r_markvisframecount;

		k = 0;
		while (k < row)
		{
			c = *in++;
			if (c)
			{
				l = model->numleafs - (k << 3);
				if (l > 8)
					l = 8;
				for (i=0 ; i<l ; i++)
				{
					if (c & (1<<i))
					{
						leaf = &model->leafs[(k << 3)+i+1];
						node = (mnode_t *)leaf;
						do
						{
							node->markvisframe = r_markvisframecount;
							node = node->parent;
						}
						while (node->markvisframecount != r_markvisframecount);
					}
				}
				k++;
			}
			else
				k += *in++;
		}
	}
	else
	{
		// LordHavoc: no vis data, mark everything as visible
		model->nodes->markvisframe = r_markvisframecount;

		for (i = 1;i < model->numleafs;i++)
		{
			node = (mnode_t *)&model->leafs[i];
			do
			{
				node->markvisframe = r_markvisframecount;
				node = node->parent;
			}
			while (node->markvisframecount != r_markvisframecount);
		}
	}
}
*/

void R_SolidWorldNode (void)
{
	if (r_viewleaf->contents != CONTENTS_SOLID)
	{
		int portalstack;
		mportal_t *p, *pstack[8192];
		msurface_t *surf, **mark, **endmark;
		mleaf_t *leaf;
		glpoly_t *poly;
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
			if (r_ser.value)
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
							for (poly = surf->polys;poly;poly = poly->next)
								R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, &plane);
						}
					}
					else
					{
						if (!(surf->flags & SURF_PLANEBACK))
							for (poly = surf->polys;poly;poly = poly->next)
								R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
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
		glpoly_t *poly;
		// LordHavoc: recursive descending worldnode; if portals are not
		// available, this is a good last resort, can cull large amounts of
		// geometry, but is more time consuming than portal-passage and renders
		// things behind walls

loc2:
		if (R_NotCulledBox(node->mins, node->maxs))
		{
			if (node->numsurfaces)
			{
				if (r_ser.value)
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
								for (poly = surf->polys;poly;poly = poly->next)
									R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), surf->flags & SURF_CLIPSOLID, RSurf_Callback, surf, NULL, &plane);
							}
						}
					}
					else
					{
						for (;surf < surfend;surf++)
						{
							if (!(surf->flags & SURF_PLANEBACK))
								for (poly = surf->polys;poly;poly = poly->next)
									R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), surf->flags & SURF_CLIPSOLID, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
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

/*
void RSurf_Callback(void *data, void *junk)
{
	((msurface_t *)data)->visframe = r_framecount;
}

int R_FrustumTestPolygon(float *points, int numpoints, int stride);

void RSurf_DoVisible(msurface_t *surf)
{
	glpoly_t *p;
	for (p = surf->polys;p;p = p->next)
		if (R_FrustumTestPolygon((float *) p->verts, p->numverts, VERTEXSIZE * sizeof(float)) >= 3)
//		R_Clip_Polygon((float *) p->verts, p->numverts, VERTEXSIZE * sizeof(float), true, RSurf_Callback, surf, 1);
//		if (R_Clip_Polygon((float *) p->verts, p->numverts, VERTEXSIZE * sizeof(float), surf->flags & SURF_CLIPSOLID))
			surf->visframe = r_framecount;
}
*/

//mleaf_t *llistbuffer[32768], *l, **llist;

/*
void RSurfLeaf_Callback(void *data)
{
	int portalstackpos = 0;
	mleaf_t *leaf;
	mportal_t *p, *portalstack[32768];
	msurface_t *surf, **mark, **endmark;
	do
	{

		leaf = data;
		if (leaf->visframe == r_framecount)
			return;
		leaf->visframe = r_framecount;

		c_leafs++;

		if (leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			endmark = mark + leaf->nummarksurfaces;
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
						RSurf_DoVisible(surf);
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						RSurf_DoVisible(surf);
				}
			}
			while (mark < endmark);
		}

		// follow portals into other leafs
		for (p = leaf->portals;p;p = p->next)
		{
			if (p->past->visframe != r_framecount && DotProduct(r_origin, p->plane.normal) < p->plane.dist)
			{
	//			R_Clip_Portal((float *) p->points, p->numpoints, sizeof(float) * 3, RSurfLeaf_Callback, p->past, 1);
				if (R_Clip_Portal((float *) p->points, p->numpoints, sizeof(float) * 3))
					portalstack[portalstackpos++] = p;
			}
		}
	}
	while(portalstackpos);
	RSurfLeaf_Callback(p->past);
	// upon returning, R_ProcessSpans will notice that the spans have changed and restart the line, this is ok because we're not adding any polygons that aren't already behind the portal
}
*/

/*
// experimental and inferior to the other in recursion depth allowances
void R_PortalWorldNode (void)
{
//	int i, j;
	mportal_t *p;
	msurface_t *surf, **mark, **endmark;
	mleaf_t *leaf, *llistbuffer[32768], **l, **llist;

	leaf = r_viewleaf;
	leaf->visframe = r_framecount;
	l = llist = &llistbuffer[0];
	*llist++ = r_viewleaf;
	while (l < llist)
	{
		leaf = *l++;

		c_leafs++;

		if (leaf->nummarksurfaces)
		{
			mark = leaf->firstmarksurface;
			endmark = mark + leaf->nummarksurfaces;
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
						RSurf_DoVisible(surf);
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						RSurf_DoVisible(surf);
				}
			}
			while (mark < endmark);
		}

		// follow portals into other leafs
		for (p = leaf->portals;p;p = p->next)
		{
			if (p->past->visframe != r_framecount)
			{
				if (R_Clip_Portal((float *) p->points, p->numpoints, sizeof(float) * 3))
				{
					p->past->visframe = r_framecount;
					*llist++ = p->past;
				}
			}
		}

//		for (p = leaf->portals;p;p = p->next)
//		{
//			leaf = p->past;
//			if (leaf->worldnodeframe != r_framecount)
//			{
//				leaf->worldnodeframe = r_framecount;
//				i = (leaf - cl.worldmodel->leafs) - 1;
//				if ((worldvis[i>>3] & (1<<(i&7))) && R_NotCulledBox(leaf->mins, leaf->maxs))
//					*llist++ = leaf;
//			}
//		}
	}

//	i = 0;
//	j = 0;
//	p = r_viewleaf->portals;
//	for (;p;p = p->next)
//	{
//		j++;
//		if (p->past->worldnodeframe != r_framecount)
//			i++;
//	}
//	if (i)
//		Con_Printf("%i portals of viewleaf (%i portals) were not checked\n", i, j);
}
*/


int r_portalframecount = 0;

/*
void R_Portal_Callback(void *data, void *data2)
{
	mleaf_t *leaf = data;
	if (!r_testvis.value)
		((mportal_t *)data2)->visframe = r_portalframecount;
	if (leaf->visframe != r_framecount)
	{
		c_leafs++;
		leaf->visframe = r_framecount;
	}
}
*/

void R_PVSWorldNode()
{
	int portalstack, i;
	mportal_t *p, *pstack[8192];
	msurface_t *surf, **mark, **endmark;
	mleaf_t *leaf;
	tinyplane_t plane;
	glpoly_t *poly;
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
		if (r_ser.value)
		{
			do
			{
				surf = *mark++;
				// make sure surfaces are only processed once
				if (surf->worldnodeframe == r_framecount)
					continue;
				surf->worldnodeframe = r_framecount;
				if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
				{
					if (surf->flags & SURF_PLANEBACK)
					{
						VectorNegate(surf->plane->normal, plane.normal);
						plane.dist = -surf->plane->dist;
						for (poly = surf->polys;poly;poly = poly->next)
							R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, &plane);
					}
				}
				else
				{
					if (!(surf->flags & SURF_PLANEBACK))
						for (poly = surf->polys;poly;poly = poly->next)
							R_Clip_AddPolygon((float *)poly->verts, poly->numverts, VERTEXSIZE * sizeof(float), (surf->flags & SURF_CLIPSOLID) != 0, RSurf_Callback, surf, NULL, (tinyplane_t *)surf->plane);
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
				if (PlaneDist(modelorg, surf->plane) < surf->plane->dist)
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

entity_t clworldent;

void R_DrawSurfaces (void)
{
	msurface_t	*surf, *endsurf;
	texture_t	*t, *currentt;
	int vertex = gl_vertex.value;

	currentrenderentity = &clworldent.render;
	softwaretransformidentity();
	surf = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	endsurf = surf + cl.worldmodel->nummodelsurfaces;
	t = currentt = NULL;
	for (;surf < endsurf;surf++)
	{
		if (surf->visframe == r_framecount)
		{
			c_faces++;
			if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
			{
				if (surf->flags & SURF_DRAWSKY)
					RSurf_DrawSky(surf, false);
				else
				{
					if (currentt != surf->texinfo->texture)
					{
						currentt = surf->texinfo->texture;
						t = R_TextureAnimation(surf->texinfo->texture);
					}
					RSurf_DrawWater(surf, t, false, surf->flags & SURF_DRAWNOALPHA ? 255 : wateralpha);
				}
			}
			else
			{
				if (currentt != surf->texinfo->texture)
				{
					currentt = surf->texinfo->texture;
					t = R_TextureAnimation(surf->texinfo->texture);
				}
				if (vertex)
					RSurf_DrawWallVertex(surf, t, false, false);
				else
					RSurf_DrawWall(surf, t, false);
			}
		}
	}
}

void R_DrawPortals(void)
{
	int drawportals, i, r, g, b;
//	mleaf_t *leaf, *endleaf;
	mportal_t *portal, *endportal;
	mvertex_t *point/*, *endpoint*/;
	drawportals = (int)r_drawportals.value;
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
	portal = cl.worldmodel->portals;
	endportal = portal + cl.worldmodel->numportals;
	for (;portal < endportal;portal++)
	{
		if (portal->visframe == r_portalframecount)
		{
			i = portal - cl.worldmodel->portals;
			r = (i & 0x0007) << 5;
			g = (i & 0x0038) << 2;
			b = (i & 0x01C0) >> 1;
			transpolybegin(0, 0, 0, TPOLYTYPE_ALPHA);
			point = portal->points;
			if (PlaneDiff(r_origin, (&portal->plane)) > 0)
			{
				for (i = portal->numpoints - 1;i >= 0;i--)
					transpolyvertub(point[i].position[0], point[i].position[1], point[i].position[2], 0, 0, r, g, b, 32);
			}
			else
			{
				for (i = 0;i < portal->numpoints;i++)
					transpolyvertub(point[i].position[0], point[i].position[1], point[i].position[2], 0, 0, r, g, b, 32);
			}
			transpolyend();
		}
	}
}

void R_SetupWorldEnt(void)
{
	memset (&clworldent, 0, sizeof(clworldent));
	clworldent.render.model = cl.worldmodel;
	clworldent.render.colormod[0] = clworldent.render.colormod[1] = clworldent.render.colormod[2] = 1;
	clworldent.render.alpha = 1;
	clworldent.render.scale = 1;

	VectorCopy (r_origin, modelorg);

	currentrenderentity = &clworldent.render;
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	wateralpha = bound(0, r_wateralpha.value*255.0f, 255);
	vertexworld = gl_vertex.value;

	R_SetupWorldEnt();

	softwaretransformidentity(); // LordHavoc: clear transform

	if (r_viewleaf->contents == CONTENTS_SOLID || r_novis.value || r_viewleaf->compressed_vis == NULL)
		R_SolidWorldNode ();
	else
		R_PVSWorldNode ();
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock (int w, int h, short *x, short *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum = 0;texnum < MAX_LIGHTMAPS;texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0;i < BLOCK_WIDTH - w;i += lightmapalign) // LordHavoc: align updates on 4 byte boundaries
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		if (nosubimagefragments || nosubimage)
		{
			if (!lightmaps[texnum])
			{
				lightmaps[texnum] = qmalloc(BLOCK_WIDTH*BLOCK_HEIGHT*4);
				memset(lightmaps[texnum], 0, BLOCK_WIDTH*BLOCK_HEIGHT*4);
			}
		}
		// LordHavoc: clear texture to blank image, fragments are uploaded using subimage
		else if (!allocated[texnum][0])
		{
			memset(templight, 0, sizeof(templight));
			if(r_upload.value)
			{
				glBindTexture(GL_TEXTURE_2D, lightmap_textures + texnum);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				if (lightmaprgba)
					glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, templight);
				else
					glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, templight);
			}
		}

		for (i = 0;i < w;i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Host_Error ("AllocBlock: full, unable to find room for %i by %i lightmap", w, h);
	return 0;
}


//int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (model_t *model, mvertex_t *vertices, msurface_t *fa)
{
	int			i, j, lindex, lnumverts;
	medge_t		*pedges;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = model->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_AllocName (sizeof(glpolysizeof_t) + lnumverts * sizeof(float[VERTEXSIZE]), "surfaces");
	poly->next = fa->polys;
	fa->polys = poly;
//	poly->flags = fa->flags;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = model->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = vertices[pedges[lindex].v[0]].position;
		else
			vec = vertices[pedges[-lindex].v[1]].position;

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s / fa->texinfo->texture->width;
		poly->verts[i][4] = t / fa->texinfo->texture->height;

		//
		// lightmap texture coordinates
		//
		s -= fa->texturemins[0];
		t -= fa->texturemins[1];
		s += 8;
		t += 8;
		// LordHavoc: calc lightmap data offset
		j = (bound(0l, (int)t>>4, fa->extents[1]>>4) * ((fa->extents[0]>>4)+1) + bound(0l, (int)s>>4, fa->extents[0]>>4)) * 3;
		poly->verts[i][7] = j;
		s += fa->light_s*16;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t += fa->light_t*16;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	/*
	if (!gl_keeptjunctions.value)
	{
		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract( this, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001
			if ((fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON) &&
				(fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
		poly->numverts = lnumverts;
	}
	*/
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	if (nosubimage || nosubimagefragments)
		return;
	glBindTexture(GL_TEXTURE_2D, lightmap_textures + surf->lightmaptexturenum);
	smax = ((surf->extents[0]>>4)+lightmapalign) & lightmapalignmask;
	if (lightmaprgba)
	{
		R_BuildLightMap (surf, templight, smax * 4, false);
		if(r_upload.value)
			glTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
	}
	else
	{
		R_BuildLightMap (surf, templight, smax * 3, false);
		if(r_upload.value)
			glTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_RGB , GL_UNSIGNED_BYTE, templight);
	}
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (gl_nosubimagefragments.value)
		nosubimagefragments = 1;
	else
		nosubimagefragments = 0;

	if (gl_nosubimage.value)
		nosubimage = 1;
	else
		nosubimage = 0;

	if (gl_lightmaprgba.value)
	{
		lightmaprgba = true;
		lightmapbytes = 4;
	}
	else
	{
		lightmaprgba = false;
		lightmapbytes = 3;
	}

	// LordHavoc: TexSubImage2D needs data aligned on 4 byte boundaries unless
	// I specify glPixelStorei(GL_UNPACK_ALIGNMENT, 1), I suspect 4 byte may be
	// faster anyway, so I implemented an adjustable lightmap alignment...

	// validate the lightmap alignment
	i = 1;
	while (i < 16 && i < gl_lightmapalign.value)
		i <<= 1;
	Cvar_SetValue("gl_lightmapalign", i);

	// find the lowest pixel count which satisfies the byte alignment
	lightmapalign = 1;
	j = lightmaprgba ? 4 : 3; // bytes per pixel
	while ((lightmapalign * j) & (i - 1))
		lightmapalign <<= 1;
	lightmapalignmask = ~(lightmapalign - 1);

	// alignment is irrelevant if using fallback modes
	if (nosubimagefragments || nosubimage)
	{
		lightmapalign = 1;
		lightmapalignmask = ~0;
	}

	if (!lightmap_textures)
		lightmap_textures = R_GetTextureSlots(MAX_LIGHTMAPS);

	// need a world entity for lightmap code
	R_SetupWorldEnt();

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m, m->vertexes, m->surfaces + i);
		}
	}

	if (nosubimage || nosubimagefragments)
	{
		// LordHavoc: switch to second TMU as an upload hint for voodoo2
		// (don't know if it really pays attention or not, but original
		// glquake did this...)
		if(r_upload.value)
			if (gl_mtexable)
				qglActiveTexture(GL_TEXTURE1_ARB);
		for (i = 0;i < MAX_LIGHTMAPS;i++)
		{
			if (!allocated[i][0])
				break;
			lightmapupdate[i][0] = BLOCK_HEIGHT;
			lightmapupdate[i][1] = 0;
			if(r_upload.value)
			{
				glBindTexture(GL_TEXTURE_2D, lightmap_textures + i);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				if (lightmaprgba)
					glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i]);
				else
					glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmaps[i]);
			}
		}
		if(r_upload.value)
			if (gl_mtexable)
				qglActiveTexture(GL_TEXTURE0_ARB);
	}
}

