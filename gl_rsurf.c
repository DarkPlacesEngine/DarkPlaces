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

extern int                     skytexturenum;

int		lightmap_textures;

signed blocklights[18*18*3]; // LordHavoc: *3 for colored lighting

// LordHavoc: skinny but tall lightmaps for quicker subimage uploads
#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128
// LordHavoc: increased lightmap limit from 64 to 1024
#define	MAX_LIGHTMAPS	1024
#define LIGHTMAPSIZE	(BLOCK_WIDTH*BLOCK_HEIGHT*4)

int			active_lightmaps;

short allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

byte *lightmaps[MAX_LIGHTMAPS];
short lightmapupdate[MAX_LIGHTMAPS][2];

int lightmapalign, lightmapalignmask; // LordHavoc: NVIDIA's broken subimage fix, see BuildLightmaps for notes
cvar_t gl_lightmapalign = {"gl_lightmapalign", "4"};
cvar_t gl_lightmaprgba = {"gl_lightmaprgba", "0"};
cvar_t gl_nosubimagefragments = {"gl_nosubimagefragments", "0"};
cvar_t gl_nosubimage = {"gl_nosubimage", "0"};
cvar_t r_ambient = {"r_ambient", "0"};
cvar_t gl_vertex = {"gl_vertex", "0"};
//cvar_t gl_funnywalls = {"gl_funnywalls", "0"}; // LordHavoc: see BuildSurfaceDisplayList

qboolean lightmaprgba, nosubimagefragments, nosubimage;
int lightmapbytes;

qboolean skyisvisible;
extern qboolean gl_arrays;

extern int r_dlightframecount;

void glrsurf_init()
{
	int i;
	for (i = 0;i < MAX_LIGHTMAPS;i++)
		lightmaps[i] = NULL;
	Cvar_RegisterVariable(&gl_lightmapalign);
	Cvar_RegisterVariable(&gl_lightmaprgba);
	Cvar_RegisterVariable(&gl_nosubimagefragments);
	Cvar_RegisterVariable(&gl_nosubimage);
	Cvar_RegisterVariable(&r_ambient);
//	Cvar_RegisterVariable(&gl_funnywalls);
	Cvar_RegisterVariable(&gl_vertex);
	// check if it's the glquake minigl driver
	if (strncasecmp(gl_vendor,"3Dfx",4)==0)
	if (!gl_arrays)
	{
//		Cvar_SetValue("gl_nosubimagefragments", 1);
//		Cvar_SetValue("gl_nosubimage", 1);
		Cvar_SetValue("gl_lightmode", 0);
	}
}

extern qboolean lighthalf;
/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	int			scale;
	int			maps;
	int			*bl;

	surf->cached_lighthalf = lighthalf;
	surf->cached_ambient = r_ambient.value;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (currententity->effects & EF_FULLBRIGHT || !cl.worldmodel->lightdata)
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
		bl = blocklights;
		j = r_ambient.value * 512.0f; // would be 256.0f logically, but using 512.0f to match winquake style
		for (i=0 ; i<size ; i++)
		{
			*bl++ = j;
			*bl++ = j;
			*bl++ = j;
		}

// add all the lightmaps
		if (lightmap)
			for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255;maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction
				bl = blocklights;
				for (i=0 ; i<size ; i++)
				{
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
				}
			}
	}
	stride -= (smax*lightmapbytes);
	bl = blocklights;
	if (lighthalf)
	{
		// LordHavoc: I shift down by 8 unlike GLQuake's 7,
		// the image is brightened as a processing pass
		if (lightmaprgba)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					*dest++ = 255;
				}
			}
		}
		else
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 8;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
				}
			}
		}
	}
	else
	{
		if (lightmaprgba)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					*dest++ = 255;
				}
			}
		}
		else
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
					t = *bl++ >> 7;if (t > 255) t = 255;else if (t < 0) t = 0;*dest++ = t;
				}
			}
		}
	}
}

byte templight[32*32*4];

void R_UpdateLightmap(msurface_t *s, int lnum)
{
	int smax, tmax;
	// upload the new lightmap texture fragment
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
			glTexSubImage2D(GL_TEXTURE_2D, 0, s->light_s, s->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
		}
		else
		{
			R_BuildLightMap (s, templight, smax * 3);
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

qboolean mtexenabled = false;

extern char skyname[];

void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
//extern cvar_t r_dynamicwater;
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
			lightmapupdate[i][0] = BLOCK_HEIGHT;
			lightmapupdate[i][1] = 0;
		}
	}
}

void R_SkySurf(msurface_t *s, qboolean transform)
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
					skyvert[currentskyvert].v[0] = v[0];
					skyvert[currentskyvert].v[1] = v[1];
					skyvert[currentskyvert++].v[2] = v[2];
				}
			}
		}
	}
}

void R_LightSurface(int *dlightbits, glpoly_t *polys, float *wvert)
{
	float		cr, cg, cb, radius, radius2, f, *v, *wv;
	int			i, a, b;
	unsigned int c;
	dlight_t	*light;
	vec_t		*lightorigin;
	glpoly_t	*p;
	for (a = 0;a < 8;a++)
	{
		if ((c = dlightbits[a]))
		{
			for (b = 0;c && b < 32;b++)
			{
				if (c & (1 << b))
				{
					c -= (1 << b);
					light = &cl_dlights[a * 32 + b];
					lightorigin = light->origin;
					cr = light->color[0];
					cg = light->color[1];
					cb = light->color[2];
					radius = light->radius*light->radius*16.0f;
					radius2 = radius * 16.0f;
					wv = wvert;
					for (p = polys;p;p = p->next)
					{
						for (i = 0, v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
						{
							f = VectorDistance2(wv, lightorigin);
							if (f < radius)
							{
								f = radius2 / (f + 65536.0f);
								wv[3] += cr * f;
								wv[4] += cg * f;
								wv[5] += cb * f;
							}
							wv += 6;
						}
					}
				}
				c >>= 1;
				b++;
			}
		}
	}
}

void R_WaterSurf(msurface_t *s, texture_t *t, qboolean transform, int alpha)
{
	int		i;
	float	os = turbsin[(int)(realtime * TURBSCALE) & 255], ot = turbsin[(int)(realtime * TURBSCALE + 96.0) & 255];
	glpoly_t *p;
	float	wvert[64*6], *wv, *v;
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
				wv[2] += r_waterripple.value * turbsin[(int)((wv[0]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((wv[1]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
			wv[3] = wv[4] = wv[5] = 128.0f;
			wv += 6;
		}
	}
	if (s->dlightframe == r_dlightframecount && r_dynamic.value)
		R_LightSurface(s->dlightbits, s->polys, wvert);
	wv = wvert;
	// FIXME: make fog texture if water texture is transparent?
	if (lighthalf)
	{
		for (p=s->polys ; p ; p=p->next)
		{
			transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), (int) wv[3] >> 1,(int) wv[4] >> 1,(int) wv[5] >> 1,alpha);
			transpolyend();
		}
	}
	else
	{
		for (p=s->polys ; p ; p=p->next)
		{
			transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
			for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
				transpolyvert(wv[0], wv[1], wv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), (int) wv[3],(int) wv[4],(int) wv[5],alpha);
			transpolyend();
		}
	}
}

void R_WallSurf(msurface_t *s, texture_t *t, qboolean transform)
{
	int			i;
	glpoly_t	*p;
	float		wvert[64*6], *wv, *v;
	// check for lightmap modification
	if (r_dynamic.value)
	{
		if (r_ambient.value != s->cached_ambient || lighthalf != s->cached_lighthalf
		|| (s->styles[0] != 255 && d_lightstylevalue[s->styles[0]] != s->cached_light[0])
		|| (s->styles[1] != 255 && d_lightstylevalue[s->styles[1]] != s->cached_light[1])
		|| (s->styles[2] != 255 && d_lightstylevalue[s->styles[2]] != s->cached_light[2])
		|| (s->styles[3] != 255 && d_lightstylevalue[s->styles[3]] != s->cached_light[3]))
			R_UpdateLightmap(s, s->lightmaptexturenum);
	}
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
	}
	if (s->dlightframe == r_dlightframecount && r_dynamic.value)
		R_LightSurface(s->dlightbits, s->polys, wvert);
	wv = wvert;
	for (p = s->polys;p;p = p->next)
	{
		if (currentwallpoly >= MAX_WALLPOLYS)
			break;
		v = p->verts[0];
		wallpoly[currentwallpoly].texnum = (unsigned short) t->gl_texturenum;
		wallpoly[currentwallpoly].lighttexnum = (unsigned short) lightmap_textures + s->lightmaptexturenum;
		wallpoly[currentwallpoly].glowtexnum = (unsigned short) t->gl_glowtexturenum;
		wallpoly[currentwallpoly].firstvert = currentwallvert;
		wallpoly[currentwallpoly].numverts = p->numverts;
		wallpoly[currentwallpoly++].lit = true;
		for (i = 0;i<p->numverts;i++, v += VERTEXSIZE)
		{
			if (lighthalf)
			{
				wallvert[currentwallvert].r = (byte) (bound(0, (int) wv[3] >> 1, 255));
				wallvert[currentwallvert].g = (byte) (bound(0, (int) wv[4] >> 1, 255));
				wallvert[currentwallvert].b = (byte) (bound(0, (int) wv[5] >> 1, 255));
				wallvert[currentwallvert].a = 255;
			}
			else
			{
				wallvert[currentwallvert].r = (byte) (bound(0, (int) wv[3], 255));
				wallvert[currentwallvert].g = (byte) (bound(0, (int) wv[4], 255));
				wallvert[currentwallvert].b = (byte) (bound(0, (int) wv[5], 255));
				wallvert[currentwallvert].a = 255;
			}
			wallvert[currentwallvert].vert[0] = wv[0];
			wallvert[currentwallvert].vert[1] = wv[1];
			wallvert[currentwallvert].vert[2] = wv[2];
			wallvert[currentwallvert].s = v[3];
			wallvert[currentwallvert].t = v[4];
			wallvert[currentwallvert].u = v[5];
			wallvert[currentwallvert++].v = v[6];
			wv += 6;
		}
	}
}

// LordHavoc: transparent brush models
extern int r_dlightframecount;
extern float modelalpha;
//extern vec3_t shadecolor;
//qboolean R_CullBox (vec3_t mins, vec3_t maxs);
//void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
//void R_DynamicLightPointNoMask(vec3_t color, vec3_t org);
//void EmitWaterPolys (msurface_t *fa);

void R_WallSurfVertex(msurface_t *s, texture_t *t, qboolean transform, qboolean isbmodel)
{
	int			i, alpha;
	glpoly_t	*p;
	float		wvert[64*6], *wv, *v;
	int			size3;
	float		scale;
	byte		*lm;
	size3 = ((s->extents[0]>>4)+1)*((s->extents[1]>>4)+1)*3; // *3 for colored lighting
	alpha = (int) (modelalpha * 255.0f);
	// check for lightmap modification
	if (r_dynamic.value)
	{
		if (r_ambient.value != s->cached_ambient || lighthalf != s->cached_lighthalf
		|| (s->styles[0] != 255 && d_lightstylevalue[s->styles[0]] != s->cached_light[0])
		|| (s->styles[1] != 255 && d_lightstylevalue[s->styles[1]] != s->cached_light[1])
		|| (s->styles[2] != 255 && d_lightstylevalue[s->styles[2]] != s->cached_light[2])
		|| (s->styles[3] != 255 && d_lightstylevalue[s->styles[3]] != s->cached_light[3]))
			R_UpdateLightmap(s, s->lightmaptexturenum);
	}
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
	if (s->dlightframe == r_dlightframecount && r_dynamic.value)
		R_LightSurface(s->dlightbits, s->polys, wvert);
	wv = wvert;
	if (isbmodel && (currententity->colormod[0] != 1 || currententity->colormod[1] != 1 || currententity->colormod[2] != 1))
	{
		if (lighthalf)
		{
			for (p = s->polys;p;p = p->next)
			{
				v = p->verts[0];
				transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
					transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], (int) (wv[3] * currententity->colormod[0]) >> 1,(int) (wv[4] * currententity->colormod[1]) >> 1,(int) (wv[5] * currententity->colormod[0]) >> 1,alpha);
				transpolyend();
			}
		}
		else
		{
			for (p = s->polys;p;p = p->next)
			{
				v = p->verts[0];
				transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
					transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], (int) (wv[3] * currententity->colormod[0]),(int) (wv[4] * currententity->colormod[1]),(int) (wv[5] * currententity->colormod[2]),alpha);
				transpolyend();
			}
		}
	}
	else
	{
		if (lighthalf)
		{
			for (p = s->polys;p;p = p->next)
			{
				v = p->verts[0];
				transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
					transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], (int) wv[3] >> 1,(int) wv[4] >> 1,(int) wv[5] >> 1,alpha);
				transpolyend();
			}
		}
		else
		{
			for (p = s->polys;p;p = p->next)
			{
				v = p->verts[0];
				transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
				for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE, wv += 6)
					transpolyvert(wv[0], wv[1], wv[2], v[3], v[4], (int) wv[3],(int) wv[4],(int) wv[5],alpha);
				transpolyend();
			}
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
//extern qboolean SV_TestLine (hull_t *hull, int num, vec3_t p1, vec3_t p2);
void DrawTextureChains (void)
{
	int			n;
	msurface_t	*s;
	texture_t	*t;

	for (n = 0;n < cl.worldmodel->numtextures;n++)
	{
		if (!cl.worldmodel->textures[n] || !(s = cl.worldmodel->textures[n]->texturechain))
			continue;
		// LordHavoc: decide the render type only once, because the surface properties were determined by texture anyway
		cl.worldmodel->textures[n]->texturechain = NULL;
		t = R_TextureAnimation (cl.worldmodel->textures[n]);
		// sky
		if (s->flags & SURF_DRAWSKY)
		{
			skyisvisible = true;
			if (!hlbsp) // LordHavoc: HalfLife maps have freaky skypolys...
				for (;s;s = s->texturechain)
					R_SkySurf(s, false);
			continue;
		}
		// subdivided water surface warp
		if (s->flags & SURF_DRAWTURB)
		{
			int alpha = s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f;
			for (;s;s = s->texturechain)
				R_WaterSurf(s, t, false, alpha);
			continue;
		}
		if (gl_vertex.value)
			for (;s;s = s->texturechain)
				R_WallSurfVertex(s, t, false, false);
		else
			for (;s;s = s->texturechain)
				R_WallSurf(s, t, false);
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
	qboolean	rotated, vertexlit = false;
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
		if ((cl_dlights[i].die < cl.time) || (!cl_dlights[i].radius))
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
			if (s->flags & SURF_DRAWSKY)
			{
				R_SkySurf(s, true);
				continue;
			}
			t = R_TextureAnimation (s->texinfo->texture);
			if (s->flags & SURF_DRAWTURB)
			{
				R_WaterSurf(s, t, true, s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f);
				continue;
			}
			if (vertexlit || s->texinfo->texture->transparent)
				R_WallSurfVertex(s, t, true, true);
			else
				R_WallSurf(s, t, true);
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

/*
================
R_WorldNode
================
*/
void R_WorldNode ()
{
	int		c, side, s = 0;
	double	dot;
	struct
	{
		double dot;
		mnode_t *node;
	} nodestack[8192];
	mnode_t *node;

	if (!(node = cl.worldmodel->nodes))
		return;

	while(1)
	{
	// if a leaf node, draw stuff
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				mleaf_t		*pleaf;
				pleaf = (mleaf_t *)node;

				c_leafs++;
				if ((c = pleaf->nummarksurfaces))
				{
					msurface_t	**mark;
					mark = pleaf->firstmarksurface;
					do
					{
						(*mark)->visframe = r_framecount;
						mark++;
					} while (--c);
				}

				// deal with model fragments in this leaf
				if (pleaf->efrags)
					R_StoreEfrags (&pleaf->efrags);
			}

			if (!s)
				break;
			node = nodestack[--s].node;
			dot = nodestack[s].dot;
			goto loc0;
		}

		c_nodes++;

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
		dot = (node->plane->type < 3 ? modelorg[node->plane->type] : DotProduct (modelorg, node->plane->normal)) - node->plane->dist;

	// recurse down the children, front side first
		side = dot < 0;
		if (node->children[side]->visframe == r_visframecount && R_NotCulledBox(node->children[side]->minmaxs, node->children[side]->minmaxs+3))
		{
			nodestack[s].node = node;
			nodestack[s++].dot = dot;
			node = node->children[side];
			continue;
		}
loc0:

		// backside
		side = dot >= 0;
	// draw stuff
		if ((c = node->numsurfaces))
		{
			msurface_t	*surf;
			surf = cl.worldmodel->surfaces + node->firstsurface;

			if (side)
			{
				for (;c;c--, surf++)
				{
					if (surf->visframe == r_framecount && !(surf->flags & SURF_PLANEBACK))
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}
			}
			else
			{
				for (;c;c--, surf++)
				{
					if (surf->visframe == r_framecount && (surf->flags & SURF_PLANEBACK))
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}
			}
		}

	// recurse down the back side
		if (node->children[side]->visframe == r_visframecount && R_NotCulledBox(node->children[side]->minmaxs, node->children[side]->minmaxs+3))
		{
			node = node->children[side];
			continue;
		}

		if (!s)
			break;
		node = nodestack[--s].node;
		dot = nodestack[s].dot;
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
		R_WorldNode ();

	glClear (GL_DEPTH_BUFFER_BIT);

	R_PushDlights (); // now mark the lit surfaces

	DrawTextureChains ();

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
int AllocBlock (int w, int h, int *x, int *y)
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
				lightmaps[texnum] = calloc(BLOCK_WIDTH*BLOCK_HEIGHT*4, 1);
		}
		// LordHavoc: clear texture to blank image, fragments are uploaded using subimage
		else if (!allocated[texnum][0])
		{
			byte blank[BLOCK_WIDTH*BLOCK_HEIGHT*3];
			memset(blank, 0, sizeof(blank));
			glBindTexture(GL_TEXTURE_2D, lightmap_textures + texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if (lightmaprgba)
				glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank);
			else
				glTexImage2D (GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, blank);
		}

		for (i=0 ; i<w ; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
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
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
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
		glTexSubImage2D(GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, templight);
	}
	else
	{
		R_BuildLightMap (surf, templight, smax * 3);
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
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

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
		if (gl_mtexable)
			qglSelectTexture(gl_mtex_enum+1);
		for (i = 0;i < MAX_LIGHTMAPS;i++)
		{
			if (!allocated[i][0])
				break;
			lightmapupdate[i][0] = BLOCK_HEIGHT;
			lightmapupdate[i][1] = 0;
			glBindTexture(GL_TEXTURE_2D, lightmap_textures + i);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			if (lightmaprgba)
				glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i]);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmaps[i]);
		}
		if (gl_mtexable)
			qglSelectTexture(gl_mtex_enum+0);
	}
}

