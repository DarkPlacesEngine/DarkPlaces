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

// LordHavoc: skinny but tall lightmaps for quicker subimage uploads
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

int lightmapalign, lightmapalignmask; // LordHavoc: NVIDIA's broken subimage fix, see BuildLightmaps for notes
cvar_t gl_lightmapalign = {"gl_lightmapalign", "4"};
cvar_t gl_lightmaprgba = {"gl_lightmaprgba", "1"};
cvar_t gl_nosubimagefragments = {"gl_nosubimagefragments", "0"};
cvar_t gl_nosubimage = {"gl_nosubimage", "0"};
cvar_t r_ambient = {"r_ambient", "0"};
cvar_t gl_vertex = {"gl_vertex", "0"};
cvar_t gl_texsort = {"gl_texsort", "1"};
cvar_t r_newworldnode = {"r_newworldnode", "0"};
cvar_t r_oldclip = {"r_oldclip", "1"};
cvar_t r_dlightmap = {"r_dlightmap", "1"};

qboolean lightmaprgba, nosubimagefragments, nosubimage, skyisvisible;
int lightmapbytes;

void gl_surf_start()
{
}

void gl_surf_shutdown()
{
}

void gl_surf_newmap()
{
}

void GL_Surf_Init()
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
	Cvar_RegisterVariable(&gl_texsort);
	Cvar_RegisterVariable(&r_newworldnode);
	Cvar_RegisterVariable(&r_oldclip);
	Cvar_RegisterVariable(&r_dlightmap);

	R_RegisterModule("GL_Surf", gl_surf_start, gl_surf_shutdown, gl_surf_newmap);
}

int         dlightdivtable[32768];

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

		VectorSubtract (cl_dlights[lnum].origin, currententity->origin, local);
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
	int i, j;
	stride -= (width*lightmapbytes);
	if (lighthalf)
	{
		// LordHavoc: I shift down by 8 unlike GLQuake's 7,
		// the image is brightened as a processing pass
		if (lightmaprgba)
		{
			for (i = 0;i < height;i++, out += stride)
			{
				for (j = 0;j < width;j++, in += 3, out += 4)
				{
					out[0] = min(in[0] >> 8, 255);
					out[1] = min(in[1] >> 8, 255);
					out[2] = min(in[2] >> 8, 255);
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
					out[0] = min(in[0] >> 8, 255);
					out[1] = min(in[1] >> 8, 255);
					out[2] = min(in[2] >> 8, 255);
				}
			}
		}
	}
	else
	{
		if (lightmaprgba)
		{
			for (i = 0;i < height;i++, out += stride)
			{
				for (j = 0;j < width;j++, in += 3, out += 4)
				{
					out[0] = min(in[0] >> 7, 255);
					out[1] = min(in[1] >> 7, 255);
					out[2] = min(in[2] >> 7, 255);
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
					out[0] = min(in[0] >> 7, 255);
					out[1] = min(in[1] >> 7, 255);
					out[2] = min(in[2] >> 7, 255);
				}
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
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			i, j, size, size3;
	byte		*lightmap;
	int			scale;
	int			maps;
	int			*bl;

	surf->cached_dlight = 0;
	surf->cached_lighthalf = lighthalf;
	surf->cached_ambient = r_ambient.value;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	size3 = size*3;
	lightmap = surf->samples;

// set to full bright if no light data
	if ((currententity && (currententity->effects & EF_FULLBRIGHT)) || !cl.worldmodel->lightdata)
	{
		bl = blocklights;
		for (i=0 ; i<size ; i++)
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

// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction
				bl = blocklights;
				for (i = 0;i < size3;i++)
					*bl++ += *lightmap++ * scale;
			}
		}
		if (r_dlightmap.value && surf->dlightframe == r_dlightframecount)
			if ((surf->cached_dlight = R_AddDynamicLights(surf)))
				c_light_polys++;
	}
	R_ConvertLightmap(blocklights, dest, smax, tmax, stride);
}

byte templight[BLOCK_WIDTH*BLOCK_HEIGHT*4];

void R_UpdateLightmap(msurface_t *s, int lnum)
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
			R_BuildLightMap (s, lightmaps[s->lightmaptexturenum] + (s->light_t * BLOCK_WIDTH + s->light_s) * 4, BLOCK_WIDTH * 4);
		else
			R_BuildLightMap (s, lightmaps[s->lightmaptexturenum] + (s->light_t * BLOCK_WIDTH + s->light_s) * 3, BLOCK_WIDTH * 3);
	}
	else
	{
		smax = ((s->extents[0]>>4)+lightmapalign) & lightmapalignmask;
		tmax = (s->extents[1]>>4)+1;
		if (lightmaprgba)
		{
			R_BuildLightMap (s, templight, smax * 4);
			if(r_upload.value)
				glTexSubImage2D(GL_TEXTURE_2D, 0, s->light_s, s->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
		}
		else
		{
			R_BuildLightMap (s, templight, smax * 3);
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
	texture_t *original;
	int		relative;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	original = base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
		{
			Con_Printf("R_TextureAnimation: broken cycle");
			return original;
		}
		if (++count > 100)
		{
			Con_Printf("R_TextureAnimation: infinite cycle");
			return original;
		}
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

extern char skyname[];

float	turbsin[256] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))


void UploadLightmaps()
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
	float	os = turbsin[(int)(realtime * TURBSCALE) & 255], ot = turbsin[(int)(cl.time * TURBSCALE + 96.0) & 255];
	glpoly_t *p;
	float	*v;
	// FIXME: make fog texture if water texture is transparent?

	if (s->dlightframe != r_dlightframecount)
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
						transpolyvert(temp[0], temp[1], temp[2] + r_waterripple.value * turbsin[(int)((temp[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((temp[1]*(1.0f/32.0f)+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f), (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
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
						transpolyvert(v[0], v[1], v[2] + r_waterripple.value * turbsin[(int)((v[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((v[1]*(1.0f/32.0f)+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f), (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), 128, 128, 128, alpha);
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
					wv[2] += r_waterripple.value * turbsin[(int)((wv[0]*(1.0f/32.0f)+cl.time) * TURBSCALE) & 255] * turbsin[(int)((wv[1]*(1.0f/32.0f)+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
				wv[3] = wv[4] = wv[5] = 128.0f;
				wv += 6;
			}
		}
		if (s->dlightframe == r_dlightframecount)
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
	 || (r_dynamic.value && r_dlightmap.value && s->dlightframe == r_dlightframecount)
	 || r_ambient.value != s->cached_ambient
	 || lighthalf != s->cached_lighthalf
	 || (r_dynamic.value
	 && ((s->styles[0] != 255 && d_lightstylevalue[s->styles[0]] != s->cached_light[0])
	 || (s->styles[1] != 255 && d_lightstylevalue[s->styles[1]] != s->cached_light[1])
	 || (s->styles[2] != 255 && d_lightstylevalue[s->styles[2]] != s->cached_light[2])
	 || (s->styles[3] != 255 && d_lightstylevalue[s->styles[3]] != s->cached_light[3]))))
		R_UpdateLightmap(s, s->lightmaptexturenum);
	if (r_dlightmap.value || s->dlightframe != r_dlightframecount)
	{
		// LordHavoc: fast path version for no vertex lighting cases
		wp = &wallpoly[currentwallpoly];
		out = &wallvert[currentwallvert];
		for (p = s->polys;p;p = p->next)
		{
			if ((currentwallpoly >= MAX_WALLPOLYS) || (currentwallvert+p->numverts > MAX_WALLVERTS))
				return;
			wp->texnum = (unsigned short) R_GetTexture(t->texture);
			wp->lighttexnum = (unsigned short) (lightmap_textures + s->lightmaptexturenum);
			wp->glowtexnum = (unsigned short) R_GetTexture(t->glowtexture);
			wp->firstvert = currentwallvert;
			wp->numverts = p->numverts;
			wp->lit = lit;
			wp++;
			currentwallpoly++;
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
		if ((!r_dlightmap.value) && s->dlightframe == r_dlightframecount)
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
extern float modelalpha;

void RSurf_DrawWallVertex(msurface_t *s, texture_t *t, int transform, int isbmodel)
{
	int i, alpha, size3;
	float *v, *wv, scale;
	glpoly_t *p;
	byte *lm;
	alpha = (int) (modelalpha * 255.0f);
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
	if (s->dlightframe == r_dlightframecount)
		RSurf_Light(s->dlightbits, s->polys);
	wv = wvert;
	if (isbmodel && (currententity->colormod[0] != 1 || currententity->colormod[1] != 1 || currententity->colormod[2] != 1))
	{
		for (p = s->polys;p;p = p->next)
		{
			v = p->verts[0];
			transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], wv[3] * currententity->colormod[0], wv[4] * currententity->colormod[1], wv[5] * currententity->colormod[2], alpha);
			transpolyend();
		}
	}
	else
	{
		for (p = s->polys;p;p = p->next)
		{
			v = p->verts[0];
			transpolybegin(R_GetTexture(t->texture), R_GetTexture(t->glowtexture), 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], wv[3], wv[4], wv[5], alpha);
			transpolyend();
		}
	}
}

/*
================
DrawTextureChains
================
*/
extern qboolean hlbsp;
extern char skyname[];
void R_DrawSurf(msurface_t *s, int isbmodel, int vertexlit)
{
	texture_t *t;
	if (s->flags & SURF_DRAWSKY)
	{
		skyisvisible = true;
		if (!hlbsp) // LordHavoc: HalfLife maps have freaky skypolys...
			RSurf_DrawSky(s, false);
		return;
	}
	t = R_TextureAnimation (s->texinfo->texture);
	if (s->flags & SURF_DRAWTURB)
	{
		RSurf_DrawWater(s, t, false, s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f);
		return;
	}
	if (vertexlit)
		RSurf_DrawWallVertex(s, t, false, false);
	else
		RSurf_DrawWall(s, t, false);
}

void DrawTextureChains (void)
{
	int			n;
	msurface_t	*s;
	texture_t	*t;

	for (n = 0;n < cl.worldmodel->numtextures;n++)
	{
		if (!cl.worldmodel->textures[n] || !(s = cl.worldmodel->textures[n]->texturechain))
			continue;
		cl.worldmodel->textures[n]->texturechain = NULL;
//		for (;s;s = s->texturechain)
//			R_DrawSurf(s, false, gl_vertex.value);
		// LordHavoc: decide the render type only once, because the surface properties were determined by texture anyway
		// sky
		if (s->flags & SURF_DRAWSKY)
		{
			skyisvisible = true;
			if (!hlbsp) // LordHavoc: HalfLife maps have freaky skypolys...
				for (;s;s = s->texturechain)
					RSurf_DrawSky(s, false);
			continue;
		}
		t = R_TextureAnimation (cl.worldmodel->textures[n]);
		// subdivided water surface warp
		if (s->flags & SURF_DRAWTURB)
		{
			int alpha = s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f;
			for (;s;s = s->texturechain)
				RSurf_DrawWater(s, t, false, alpha);
			continue;
		}
		if (gl_vertex.value)
			for (;s;s = s->texturechain)
				RSurf_DrawWallVertex(s, t, false, false);
		else
			for (;s;s = s->texturechain)
				RSurf_DrawWall(s, t, false);
	}
}

void R_NoVisMarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, model_t *model);

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			i;
	vec3_t		mins, maxs;
	msurface_t	*s;
	model_t		*clmodel;
	int	rotated, vertexlit = false;
	texture_t	*t;
	vec3_t		org;

	currententity = e;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	c_bmodels++;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	s = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	for (i = 0;i < MAX_DLIGHTS;i++)
	{
		if (!cl_dlights[i].radius)
			continue;

		VectorSubtract(cl_dlights[i].origin, currententity->origin, org);
		R_NoVisMarkLights (org, &cl_dlights[i], 1<<(i&31), i >> 5, clmodel);
	}
	vertexlit = modelalpha != 1 || clmodel->firstmodelsurface == 0 || (currententity->effects & EF_FULLBRIGHT) || currententity->colormod[0] != 1 || currententity->colormod[2] != 1 || currententity->colormod[2] != 1;

e->angles[0] = -e->angles[0];	// stupid quake bug
	softwaretransformforentity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	// draw texture
	for (i = 0;i < clmodel->nummodelsurfaces;i++, s++)
	{
		if (((s->flags & SURF_PLANEBACK) == 0) == (PlaneDiff(modelorg, s->plane) >= 0))
		{
//			R_DrawSurf(s, true, vertexlit || s->texinfo->texture->transparent);
			if (s->flags & SURF_DRAWSKY)
			{
				RSurf_DrawSky(s, true);
				continue;
			}
			t = R_TextureAnimation (s->texinfo->texture);
			if (s->flags & SURF_DRAWTURB)
			{
				RSurf_DrawWater(s, t, true, s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f);
				continue;
			}
			if (vertexlit || s->texinfo->texture->transparent)
				RSurf_DrawWallVertex(s, t, true, true);
			else
				RSurf_DrawWall(s, t, true);
		}
	}
	UploadLightmaps();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

void R_StoreEfrags (efrag_t **ppefrag);

void R_NewWorldNode ()
{
	int l, texsort = gl_texsort.value, vertex = gl_vertex.value;
	mleaf_t *leaf;
	msurface_t *surf, **mark, **endmark;

	for (l = 0, leaf = cl.worldmodel->leafs;l < cl.worldmodel->numleafs;l++, leaf++)
	{
		if ((leaf->visframe == r_visframecount) && (leaf->efrags || leaf->nummarksurfaces))
		{
			if (R_CullBox(leaf->minmaxs, leaf->minmaxs+3))
				continue;

			c_leafs++;

			// deal with model fragments in this leaf
			if (leaf->efrags)
				R_StoreEfrags (&leaf->efrags);

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
						if ( (surf->flags & SURF_PLANEBACK))
						{
							surf->visframe = r_framecount;
							c_faces++;
							if (texsort)
							{
								surf->texturechain = surf->texinfo->texture->texturechain;
								surf->texinfo->texture->texturechain = surf;
							}
							else
								R_DrawSurf(surf, false, vertex);
						}
					}
					else
					{
						if (!(surf->flags & SURF_PLANEBACK))
						{
							surf->visframe = r_framecount;
							c_faces++;
							if (texsort)
							{
								surf->texturechain = surf->texinfo->texture->texturechain;
								surf->texinfo->texture->texturechain = surf;
							}
							else
								R_DrawSurf(surf, false, vertex);
						}
					}
				}
				while (mark < endmark);
			}
		}
	}
}

struct nodestack_s
{
	int side;
	mnode_t *node;
	int noclipping;
} nodestack[8192];

/*
================
R_WorldNode
================
*/
void R_WorldNode ()
{
	int side, texsort = gl_texsort.value, vertex = gl_vertex.value, ca, cb, cc, cd, noclipping = false, oldclip = r_oldclip.value;
	struct nodestack_s *nstack;
	mnode_t *node;
	mleaf_t *pleaf;
	msurface_t *surf, *endsurf, **mark, **endmark;
	nstack = nodestack;

	if (!(node = cl.worldmodel->nodes))
		return;

	while(1)
	{
		if (oldclip)
		{
			if (R_CullBox(node->minmaxs, node->minmaxs+3))
			{
backupstack:
				if (nstack <= nodestack)
					break;
				nstack--;
				node = nstack->node;
				side = nstack->side;
				noclipping = nstack->noclipping;
				goto loc0;
			}
		}
		else
		if (!noclipping)
		{
			ca = frustum[0].BoxOnPlaneSideFunc(node->minmaxs, node->minmaxs+3, &frustum[0]);if (ca == 2) goto backupstack; // completely clipped away
			cb = frustum[1].BoxOnPlaneSideFunc(node->minmaxs, node->minmaxs+3, &frustum[1]);if (cb == 2) goto backupstack; // completely clipped away
			cc = frustum[2].BoxOnPlaneSideFunc(node->minmaxs, node->minmaxs+3, &frustum[2]);if (cc == 2) goto backupstack; // completely clipped away
			cd = frustum[3].BoxOnPlaneSideFunc(node->minmaxs, node->minmaxs+3, &frustum[3]);if (cd == 2) goto backupstack; // completely clipped away
			if (ca == 0 && cb == 0 && cc == 0 && cd == 0)
				noclipping = true; // not clipped at all, no need to clip any children of this node
			// partially clipped node
		}
	// if a leaf node, draw stuff
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pleaf = (mleaf_t *)node;

				c_leafs++;
				if (pleaf->nummarksurfaces)
				{
					mark = pleaf->firstmarksurface;
					endmark = mark + pleaf->nummarksurfaces;
					do
					{
						(*mark)->visframe = r_framecount;
						mark++;
					}
					while (mark < endmark);
				}

				// deal with model fragments in this leaf
				if (pleaf->efrags)
					R_StoreEfrags (&pleaf->efrags);
			}

			if (nstack <= nodestack)
				break;
			nstack--;
			node = nstack->node;
			side = nstack->side;
			noclipping = nstack->noclipping;
			goto loc0;
		}

		c_nodes++;

		// node is just a decision point, so go down the apropriate sides

		// find which side of the node we are on
		side = PlaneDist(modelorg, node->plane) < node->plane->dist;

		// recurse down the children, front side first
		if (node->children[side]->visframe == r_visframecount)
		{
			nstack->node = node;
			nstack->side = !side; // go down back side when we come back up
			nstack->noclipping = noclipping;
			nstack++;
			node = node->children[side];
			continue;
		}
		side = !side;
loc0:

	// draw stuff
		if (node->numsurfaces)
		{
			surf = cl.worldmodel->surfaces + node->firstsurface;
			endsurf = surf + node->numsurfaces;

			if (texsort)
			{
				if (side)
				{
					do
					{
						if (surf->visframe == r_framecount && !(surf->flags & SURF_PLANEBACK))
						{
							c_faces++;
							surf->texturechain = surf->texinfo->texture->texturechain;
							surf->texinfo->texture->texturechain = surf;
						}
						else
							surf->visframe = -1; // LordHavoc: mark as not visible, so lighting will not touch it
						surf++;
					}
					while (surf < endsurf);
				}
				else
				{
					do
					{
						if (surf->visframe == r_framecount && (surf->flags & SURF_PLANEBACK))
						{
							c_faces++;
							surf->texturechain = surf->texinfo->texture->texturechain;
							surf->texinfo->texture->texturechain = surf;
						}
						else
							surf->visframe = -1; // LordHavoc: mark as not visible, so lighting will not touch it
						surf++;
					}
					while (surf < endsurf);
				}
			}
			else
			{
				if (side)
				{
					do
					{
						if (surf->visframe == r_framecount && !(surf->flags & SURF_PLANEBACK))
						{
							c_faces++;
							R_DrawSurf(surf, false, vertex);
						}
						else
							surf->visframe = -1; // LordHavoc: mark as not visible, so lighting will not touch it
						surf++;
					}
					while (surf < endsurf);
				}
				else
				{
					do
					{
						if (surf->visframe == r_framecount && (surf->flags & SURF_PLANEBACK))
						{
							c_faces++;
							R_DrawSurf(surf, false, vertex);
						}
						else
							surf->visframe = -1; // LordHavoc: mark as not visible, so lighting will not touch it
						surf++;
					}
					while (surf < endsurf);
				}
			}
		}

	// recurse down the back side
		if (node->children[side]->visframe == r_visframecount)
		{
			node = node->children[side];
			continue;
		}

		if (nstack <= nodestack)
			break;
		nstack--;
		node = nstack->node;
		side = nstack->side;
		noclipping = nstack->noclipping;
		goto loc0;
	}
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;
	ent.colormod[0] = ent.colormod[1] = ent.colormod[2] = 1;
	modelalpha = ent.alpha = 1;
	ent.scale = 1;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;

	softwaretransformidentity(); // LordHavoc: clear transform

	if (cl.worldmodel)
	{
		if (r_newworldnode.value)
			R_NewWorldNode ();
		else
			R_WorldNode ();
	}

	R_PushDlights (); // now mark the lit surfaces

	DrawTextureChains ();
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;
	
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
	else
	{
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
		
		for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		{
			if (vis[i>>3] & (1<<(i&7)))
			{
				node = (mnode_t *)&cl.worldmodel->leafs[i+1];
				do
				{
					if (node->visframe == r_visframecount)
						break;
					node->visframe = r_visframecount;
					node = node->parent;
				} while (node);
			}
		}
	}
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

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i+=lightmapalign) // LordHavoc: NVIDIA has broken subimage, so align the lightmaps
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
			byte blank[BLOCK_WIDTH*BLOCK_HEIGHT*4];
			memset(blank, 0, sizeof(blank));
			if(r_upload.value)
			{
				glBindTexture(GL_TEXTURE_2D, lightmap_textures + texnum);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				if (lightmaprgba)
					glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank);
				else
					glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, blank);
			}
		}

		for (i=0 ; i<w ; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Host_Error ("AllocBlock: full, unable to find room for %i by %i lightmap", w, h);
	return 0;
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, j, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture
	//
	poly = Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "surfaces");
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
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
	}
	*/
	poly->numverts = lnumverts;
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
		R_BuildLightMap (surf, templight, smax * 4);
		if(r_upload.value)
			glTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
	}
	else
	{
		R_BuildLightMap (surf, templight, smax * 3);
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

	// LordHavoc: NVIDIA seems to have a broken glTexSubImage2D,
	//            it needs to be aligned on 4 pixel boundaries...
	//            so I implemented an adjustable lightmap alignment
	if (gl_lightmapalign.value < 1)
		gl_lightmapalign.value = 1;
	if (gl_lightmapalign.value > 16)
		gl_lightmapalign.value = 16;
	lightmapalign = 1;
	while (lightmapalign < gl_lightmapalign.value)
		lightmapalign <<= 1;
	gl_lightmapalign.value = lightmapalign;
	lightmapalignmask = ~(lightmapalign - 1);
	if (nosubimagefragments || nosubimage)
	{
		lightmapalign = 1;
		lightmapalignmask = ~0;
	}

	if (!lightmap_textures)
		lightmap_textures = R_GetTextureSlots(MAX_LIGHTMAPS);

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;
			GL_CreateSurfaceLightmap (m->surfaces + i);
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	if (nosubimage || nosubimagefragments)
	{
		if(r_upload.value)
			if (gl_mtexable)
				qglSelectTexture(gl_mtex_enum+1);
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
				qglSelectTexture(gl_mtex_enum+0);
	}
}

