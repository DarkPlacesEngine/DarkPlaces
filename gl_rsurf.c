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
#define LIGHTMAPSIZE	(BLOCK_WIDTH*BLOCK_HEIGHT*3)

int			active_lightmaps;

short allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

byte *lightmaps[MAX_LIGHTMAPS];
short lightmapupdate[MAX_LIGHTMAPS][2];

int lightmapalign, lightmapalignmask; // LordHavoc: NVIDIA's broken subimage fix, see BuildLightmaps for notes
cvar_t gl_lightmapalign = {"gl_lightmapalign", "4"};
cvar_t gl_lightmaprgba = {"gl_lightmaprgba", "1"};
cvar_t gl_nosubimagefragments = {"gl_nosubimagefragments", "0"};
cvar_t gl_nosubimage = {"gl_nosubimage", "0"};

qboolean lightmaprgba, nosubimagefragments, nosubimage;
int lightmapbytes;

qboolean skyisvisible;
extern qboolean gl_arrays;

void glrsurf_init()
{
	int i;
	for (i = 0;i < MAX_LIGHTMAPS;i++)
		lightmaps[i] = NULL;
	Cvar_RegisterVariable(&gl_lightmapalign);
	Cvar_RegisterVariable(&gl_lightmaprgba);
	Cvar_RegisterVariable(&gl_nosubimagefragments);
	Cvar_RegisterVariable(&gl_nosubimage);
	// check if it's the glquake minigl driver
	if (strncasecmp(gl_vendor,"3Dfx",4)==0)
	if (!gl_arrays)
	{
		Cvar_SetValue("gl_nosubimagefragments", 1);
//		Cvar_SetValue("gl_nosubimage", 1);
		Cvar_SetValue("gl_lightmode", 0);
	}
}

int dlightdivtable[8192];
int dlightdivtableinitialized = 0;

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			sdtable[18], lnum, td, maxdist, maxdist2, maxdist3, i, s, t, smax, tmax, red, green, blue, j;
	unsigned	*bl;
	float		dist, f;
	vec3_t		impact, local;
	// use 64bit integer...  shame it's not very standardized...
#if _MSC_VER || __BORLANDC__
	__int64		k; // MSVC
#else
	long long	k; // GCC
#endif

	if (!dlightdivtableinitialized)
	{
		dlightdivtable[0] = 1048576 >> 7;
		for (s = 1;s < 8192;s++)
			dlightdivtable[s] = 1048576 / (s << 7);
		dlightdivtableinitialized = 1;
	}

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & (1<<(lnum&31)) ) )
			continue;		// not lit by this light

		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, local);
		dist = DotProduct (local, surf->plane->normal) - surf->plane->dist;
		for (i=0 ; i<3 ; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i]*dist;

		f = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		i = f;

		// reduce calculations
		t = dist*dist;
		for (s = 0;s < smax;s++, i -= 16)
			sdtable[s] = i*i + t;

		f = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
		i = f;

		maxdist = (int) (cl_dlights[lnum].radius*cl_dlights[lnum].radius); // for comparisons to minimum acceptable light
		// clamp radius to avoid exceeding 8192 entry division table
		if (maxdist > 1048576)
			maxdist = 1048576;
		maxdist3 = maxdist - (int) (dist*dist);
		// convert to 8.8 blocklights format
		if (!cl_dlights[lnum].dark)
		{
			f = cl_dlights[lnum].color[0] * maxdist;red = f;
			f = cl_dlights[lnum].color[1] * maxdist;green = f;
			f = cl_dlights[lnum].color[2] * maxdist;blue = f;
		}
		else // negate for darklight
		{
			f = cl_dlights[lnum].color[0] * -maxdist;red = f;
			f = cl_dlights[lnum].color[1] * -maxdist;green = f;
			f = cl_dlights[lnum].color[2] * -maxdist;blue = f;
		}
		bl = blocklights;
		for (t = 0;t < tmax;t++,i -= 16)
		{
			td = i*i;
			if (td < maxdist3) // make sure some part of it is visible on this line
			{
				maxdist2 = maxdist - td;
				for (s = 0;s < smax;s++)
				{
					if (sdtable[s] < maxdist2)
					{
						j = dlightdivtable[(sdtable[s]+td) >> 7];
						k = (red   * j) >> 8;bl[0] += k;
						k = (green * j) >> 8;bl[1] += k;
						k = (blue  * j) >> 8;bl[2] += k;
					}
					bl += 3;
				}
			}
			else
				bl+=smax*3; // skip line
		}
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

	surf->cached_dlight = (surf->dlightframe == r_framecount);
	surf->cached_lighthalf = lighthalf;

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
		for (i=0 ; i<size ; i++)
		{
			*bl++ = 0;
			*bl++ = 0;
			*bl++ = 0;
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

// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights (surf);
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
extern int r_dlightframecount;
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
						glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i]);
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

/*
================
DrawTextureChains
================
*/
extern qboolean hlbsp;
extern void R_Sky();
extern char skyname[];
void DrawTextureChains (void)
{
	int		i, j, maps;
	msurface_t	*s;
	texture_t	*t;
	glpoly_t	*p;
	float		*v;
	float		os = turbsin[(int)(realtime * TURBSCALE) & 255], ot = turbsin[(int)(realtime * TURBSCALE + 96.0) & 255];

	// first the sky
	skypolyclear();
	for (j = 0;j < cl.worldmodel->numtextures;j++)
	{
		if (!cl.worldmodel->textures[j] || !(s = cl.worldmodel->textures[j]->texturechain))
			continue;
		// LordHavoc: decide the render type only once, because the surface properties were determined by texture anyway
		// subdivided water surface warp
		if (s->flags & SURF_DRAWSKY)
		{
			cl.worldmodel->textures[j]->texturechain = NULL;
			t = R_TextureAnimation (cl.worldmodel->textures[j]);
			skyisvisible = true;
			if (!hlbsp) // LordHavoc: HalfLife maps have freaky skypolys...
			{
				for (;s;s = s->texturechain)
				{
					for (p=s->polys ; p ; p=p->next)
					{
						if (currentskypoly < MAX_SKYPOLYS && currentskyvert + p->numverts <= MAX_SKYVERTS)
						{
							skypoly[currentskypoly].firstvert = currentskyvert;
							skypoly[currentskypoly++].verts = p->numverts;
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
		}
	}
	skypolyrender(); // fogged sky polys, affects depth

	if (skyname[0] && skyisvisible && !fogenabled)
		R_Sky(); // does not affect depth, draws over the sky polys

	// then walls
	wallpolyclear();
	for (j = 0;j < cl.worldmodel->numtextures;j++)
	{
		if (!cl.worldmodel->textures[j] || !(s = cl.worldmodel->textures[j]->texturechain))
			continue;
		if (!(s->flags & SURF_DRAWTURB))
		{
			cl.worldmodel->textures[j]->texturechain = NULL;
			t = R_TextureAnimation (cl.worldmodel->textures[j]);
			for (;s;s = s->texturechain)
			{
				if (currentwallpoly < MAX_WALLPOLYS && currentwallvert < MAX_WALLVERTS && (currentwallvert + s->polys->numverts) <= MAX_WALLVERTS)
				{
					// check for lightmap modification
					if (r_dynamic.value)
					{
						if (s->dlightframe == r_framecount || s->cached_dlight || lighthalf != s->cached_lighthalf) // dynamic this frame or previously, or lighthalf changed
							R_UpdateLightmap(s, s->lightmaptexturenum);
						else
							for (maps = 0 ; maps < MAXLIGHTMAPS && s->styles[maps] != 255 ; maps++)
								if (d_lightstylevalue[s->styles[maps]] != s->cached_light[maps])
								{
									R_UpdateLightmap(s, s->lightmaptexturenum);
									break;
								}
					}
					wallpoly[currentwallpoly].texnum = (unsigned short) t->gl_texturenum;
					wallpoly[currentwallpoly].lighttexnum = (unsigned short) lightmap_textures + s->lightmaptexturenum;
					wallpoly[currentwallpoly].glowtexnum = (unsigned short) t->gl_glowtexturenum;
					wallpoly[currentwallpoly].firstvert = currentwallvert;
					wallpoly[currentwallpoly++].verts = s->polys->numverts;
					for (i = 0,v = s->polys->verts[0];i<s->polys->numverts;i++, v += VERTEXSIZE)
					{
						wallvert[currentwallvert].vert[0] = v[0];
						wallvert[currentwallvert].vert[1] = v[1];
						wallvert[currentwallvert].vert[2] = v[2];
						wallvert[currentwallvert].s = v[3];
						wallvert[currentwallvert].t = v[4];
						wallvert[currentwallvert].u = v[5];
						wallvert[currentwallvert++].v = v[6];
					}
				}
			}
		}
	}
	UploadLightmaps();
	wallpolyrender();

	// then water (water gets diverted to transpoly list)
	for (j = 0;j < cl.worldmodel->numtextures;j++)
	{
		if (!cl.worldmodel->textures[j] || !(s = cl.worldmodel->textures[j]->texturechain))
			continue;
		cl.worldmodel->textures[j]->texturechain = NULL;
		t = R_TextureAnimation (cl.worldmodel->textures[j]);
		// LordHavoc: decide the render type only once, because the surface properties were determined by texture anyway
		// subdivided water surface warp
		if (s->flags & SURF_DRAWTURB)
		{
			int light, alpha, r = 0, g = 0, b = 0;
			vec3_t nv, shadecolor;
			alpha = s->flags & SURF_DRAWNOALPHA ? 255 : r_wateralpha.value*255.0f;
			light = false;
			if (s->flags & SURF_DRAWFULLBRIGHT)
				r = g = b = lighthalf ? 128 : 255;
			else if (s->dlightframe == r_dlightframecount && r_dynamic.value)
				light = true;
			else
				r = g = b = lighthalf ? 64 : 128;
			if (r_waterripple.value)
			{
				if (lighthalf)
				{
					if (light)
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									nv[0] = v[0];
									nv[1] = v[1];
									nv[2] = v[2] + r_waterripple.value * turbsin[(int)((v[3]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((v[4]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
									shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;
									R_DynamicLightPoint(shadecolor, nv, s->dlightbits);
									transpolyvert(nv[0], nv[1], nv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), (int) shadecolor[0] >> 1,(int) shadecolor[1] >> 1,(int) shadecolor[2] >> 1,alpha);
								}
								transpolyend();
							}
						}
					}
					else
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									nv[0] = v[0];
									nv[1] = v[1];
									nv[2] = v[2] + r_waterripple.value * turbsin[(int)((v[3]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((v[4]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
									transpolyvert(nv[0], nv[1], nv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), r,g,b,alpha);
								}
								transpolyend();
							}
						}
					}
				}
				else
				{
					if (light)
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									nv[0] = v[0];
									nv[1] = v[1];
									nv[2] = v[2] + r_waterripple.value * turbsin[(int)((v[3]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((v[4]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
									shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;
									R_DynamicLightPoint(shadecolor, nv, s->dlightbits);
									transpolyvert(nv[0], nv[1], nv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), shadecolor[0],shadecolor[1],shadecolor[2],alpha);
								}
								transpolyend();
							}
						}
					}
					else
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									nv[0] = v[0];
									nv[1] = v[1];
									nv[2] = v[2] + r_waterripple.value * turbsin[(int)((v[3]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((v[4]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
									transpolyvert(nv[0], nv[1], nv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), r,g,b,alpha);
								}
								transpolyend();
							}
						}
					}
				}
			}
			else
			{
				if (lighthalf)
				{
					if (light)
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;
									R_DynamicLightPoint(shadecolor, v, s->dlightbits);
									transpolyvert(v[0], v[1], v[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), (int) shadecolor[0] >> 1,(int) shadecolor[1] >> 1,(int) shadecolor[2] >> 1,alpha);
								}
								transpolyend();
							}
						}
					}
					else
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									transpolyvert(v[0], v[1], v[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), r,g,b,alpha);
								}
								transpolyend();
							}
						}
					}
				}
				else
				{
					if (light)
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;
									R_DynamicLightPoint(shadecolor, v, s->dlightbits);
									transpolyvert(v[0], v[1], v[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), shadecolor[0],shadecolor[1],shadecolor[2],alpha);
								}
								transpolyend();
							}
						}
					}
					else
					{
						for (;s;s = s->texturechain)
						{
							for (p=s->polys ; p ; p=p->next)
							{
								// FIXME: could be a transparent water texture
								transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, TPOLYTYPE_ALPHA);
								for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
								{
									transpolyvert(v[0], v[1], v[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), r,g,b,alpha);
								}
								transpolyend();
							}
						}
					}
				}
			}
		}
	}
}

// LordHavoc: transparent brush models
extern int r_dlightframecount;
extern float modelalpha;
extern vec3_t shadecolor;
//qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_DynamicLightPoint(vec3_t color, vec3_t org, int *dlightbits);
void R_DynamicLightPointNoMask(vec3_t color, vec3_t org);
void EmitWaterPolys (msurface_t *fa);
void R_MarkLights (vec3_t lightorigin, dlight_t *light, int bit, int bitindex, mnode_t *node);

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			i, j, k, smax, tmax, size3, maps;
	vec3_t		mins, maxs, nv;
	msurface_t	*s;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated, vertexlit = false;
	float		dot, *v, scale;
	texture_t	*t;
	byte		*lm;
	float		os = turbsin[(int)(realtime * TURBSCALE) & 255], ot = turbsin[(int)(realtime * TURBSCALE + 96.0) & 255];

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
	if (modelalpha == 1 && clmodel->firstmodelsurface != 0 && !(currententity->effects & EF_FULLBRIGHT) && currententity->colormod[0] == 1 && currententity->colormod[2] == 1 && currententity->colormod[2] == 1)
	{
//		if (!gl_flashblend.value)
//		{
			vec3_t org;
			for (k=0 ; k<MAX_DLIGHTS ; k++)
			{
				if ((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
					continue;

				VectorSubtract(cl_dlights[k].origin, currententity->origin, org);
				R_MarkLights (org, &cl_dlights[k], 1<<(k&31), k >> 5, clmodel->nodes + clmodel->hulls[0].firstclipnode);
			}
//		}
	}
	else
		vertexlit = true;

e->angles[0] = -e->angles[0];	// stupid quake bug
	softwaretransformforentity (e);
e->angles[0] = -e->angles[0];	// stupid quake bug

	// draw texture
	for (j = 0;j < clmodel->nummodelsurfaces;j++, s++)
	{
	// find which side of the node we are on
		pplane = s->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((s->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(s->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (s->flags & SURF_DRAWSKY)
				continue;
			if (s->flags & SURF_DRAWTURB)
			{
				glpoly_t	*p;
				int			light, alpha, r = 0, g = 0, b = 0;
				vec3_t		shadecolor;

				if (s->flags & SURF_DRAWNOALPHA)
					alpha = modelalpha*255.0f;
				else
					alpha = r_wateralpha.value*modelalpha*255.0f;
				light = false;
				if (s->flags & SURF_DRAWFULLBRIGHT || currententity->effects & EF_FULLBRIGHT)
				{
					if (lighthalf)
					{
						r = 128.0f * currententity->colormod[0];
						g = 128.0f * currententity->colormod[1];
						b = 128.0f * currententity->colormod[2];
					}
					else
					{
						r = 255.0f * currententity->colormod[0];
						g = 255.0f * currententity->colormod[1];
						b = 255.0f * currententity->colormod[2];
					}
				}
				else if (s->dlightframe == r_dlightframecount && r_dynamic.value)
					light = true;
				else
				{
					if (lighthalf)
					{
						r = 64.0f * currententity->colormod[0];
						g = 64.0f * currententity->colormod[1];
						b = 64.0f * currententity->colormod[2];
					}
					else
					{
						r = 128.0f * currententity->colormod[0];
						g = 128.0f * currententity->colormod[1];
						b = 128.0f * currententity->colormod[2];
					}
				}
				for (p=s->polys ; p ; p=p->next)
				{
					// FIXME: could be a transparent water texture
					transpolybegin(s->texinfo->texture->gl_texturenum, s->texinfo->texture->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
					for (i = 0,v = p->verts[0];i < p->numverts;i++, v += VERTEXSIZE)
					{
						softwaretransform(v, nv);
						if (r_waterripple.value)
							nv[2] += r_waterripple.value * turbsin[(int)((v[3]*0.125f+realtime) * TURBSCALE) & 255] * turbsin[(int)((v[4]*0.125f+realtime) * TURBSCALE) & 255] * (1.0f / 64.0f);
						if (light)
						{
							shadecolor[0] = shadecolor[1] = shadecolor[2] = 128;
							R_DynamicLightPoint(shadecolor, nv, s->dlightbits);
							if (lighthalf)
							{
								r = (int) ((float) (shadecolor[0] * currententity->colormod[0])) >> 1;
								g = (int) ((float) (shadecolor[1] * currententity->colormod[1])) >> 1;
								b = (int) ((float) (shadecolor[2] * currententity->colormod[2])) >> 1;
							}
							else
							{
								r = (int) ((float) (shadecolor[0] * currententity->colormod[0]));
								g = (int) ((float) (shadecolor[1] * currententity->colormod[1]));
								b = (int) ((float) (shadecolor[2] * currententity->colormod[2]));
							}
						}
						transpolyvert(nv[0], nv[1], nv[2], (v[3] + os) * (1.0f/64.0f), (v[4] + ot) * (1.0f/64.0f), r,g,b,alpha);
					}
					transpolyend();
				}
				continue;
			}
			t = R_TextureAnimation (s->texinfo->texture);
			v = s->polys->verts[0];
			if (vertexlit || s->texinfo->texture->transparent)
			{
				// FIXME: could be a transparent water texture
				transpolybegin(t->gl_texturenum, t->gl_glowtexturenum, 0, currententity->effects & EF_ADDITIVE ? TPOLYTYPE_ADD : TPOLYTYPE_ALPHA);
				if ((currententity->effects & EF_FULLBRIGHT) || !s->samples)
				{
					for (i = 0;i < s->polys->numverts;i++, v += VERTEXSIZE)
					{
						softwaretransform(v, nv);
						transpolyvert(nv[0], nv[1], nv[2], v[3], v[4], 255,255,255,modelalpha*255.0f);
					}
				}
				else
				{
					smax = (s->extents[0]>>4)+1;
					tmax = (s->extents[1]>>4)+1;
					size3 = smax*tmax*3; // *3 for colored lighting
					for (i = 0;i < s->polys->numverts;i++, v += VERTEXSIZE)
					{
						shadecolor[0] = shadecolor[1] = shadecolor[2] = 0;
						lm = (byte *)((long) s->samples + ((int) v[8] * smax + (int) v[7]) * 3); // LordHavoc: *3 for colored lighting
						for (maps = 0;maps < MAXLIGHTMAPS && s->styles[maps] != 255;maps++)
						{
							scale = d_lightstylevalue[s->styles[maps]] * (1.0 / 128.0);
							shadecolor[0] += lm[0] * scale;
							shadecolor[1] += lm[1] * scale;
							shadecolor[2] += lm[2] * scale;
							lm += size3; // LordHavoc: *3 for colored lighting
						}
						softwaretransform(v, nv);
						R_DynamicLightPointNoMask(shadecolor, nv); // LordHavoc: dynamic lighting
						if (lighthalf)
						{
							transpolyvert(nv[0], nv[1], nv[2], v[3], v[4], (int) shadecolor[0] >> 1, (int) shadecolor[1] >> 1, (int) shadecolor[2] >> 1, modelalpha*255.0f);
						}
						else
						{
							transpolyvert(nv[0], nv[1], nv[2], v[3], v[4], shadecolor[0], shadecolor[1], shadecolor[2], modelalpha*255.0f);
						}
					}
				}
				transpolyend();
			}
			else
			{
				// check for lightmap modification
				if (r_dynamic.value)
				{
					if (s->dlightframe == r_framecount || s->cached_dlight || lighthalf != s->cached_lighthalf) // dynamic this frame or previously, or lighthalf changed
						R_UpdateLightmap(s, s->lightmaptexturenum);
					else
						for (maps = 0 ; maps < MAXLIGHTMAPS && s->styles[maps] != 255 ; maps++)
							if (d_lightstylevalue[s->styles[maps]] != s->cached_light[maps])
							{
								R_UpdateLightmap(s, s->lightmaptexturenum);
								break;
							}
				}
				if (currentwallpoly < MAX_WALLPOLYS && (currentwallvert + s->polys->numverts) <= MAX_WALLVERTS)
				{
					wallpoly[currentwallpoly].texnum = (unsigned short) t->gl_texturenum;
					wallpoly[currentwallpoly].lighttexnum = (unsigned short) lightmap_textures + s->lightmaptexturenum;
					wallpoly[currentwallpoly].glowtexnum = (unsigned short) t->gl_glowtexturenum;
					wallpoly[currentwallpoly].firstvert = currentwallvert;
					wallpoly[currentwallpoly++].verts = s->polys->numverts;
					for (i = 0;i<s->polys->numverts;i++, v += VERTEXSIZE)
					{
						softwaretransform(v, wallvert[currentwallvert].vert);
						wallvert[currentwallvert].s = v[3];
						wallvert[currentwallvert].t = v[4];
						wallvert[currentwallvert].u = v[5];
						wallvert[currentwallvert++].v = v[6];
					}
				}
			}
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
R_RecursiveWorldNode
================
*/
//extern qboolean R_CullBox (vec3_t mins, vec3_t maxs);
/*
void R_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	double		dot;

loc0:
// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		mleaf_t		*pleaf;
		pleaf = (mleaf_t *)node;

		if (c = pleaf->nummarksurfaces)
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

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	dot = (node->plane->type < 3 ? modelorg[node->plane->type] : DotProduct (modelorg, node->plane->normal)) - node->plane->dist;

// recurse down the children, front side first
	side = dot < 0;
	// LordHavoc: save a stack frame by avoiding a call
//	if (node->children[side]->contents != CONTENTS_SOLID && node->children[side]->visframe == r_visframecount && !R_CullBox (node->children[side]->minmaxs, node->children[side]->minmaxs+3))
	// LordHavoc: inlined further to reduce conditions
	if (node->children[side]->contents != CONTENTS_SOLID
	 && node->children[side]->visframe == r_visframecount
	 && frustum[0].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[0]) != 2
	 && frustum[1].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[1]) != 2
	 && frustum[2].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[2]) != 2
	 && frustum[3].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[3]) != 2)
		R_RecursiveWorldNode (node->children[side]);

	// backside
	side = dot >= 0;
// draw stuff
	if (c = node->numsurfaces)
	{
		msurface_t	*surf;
		surf = cl.worldmodel->surfaces + node->firstsurface;

		// LordHavoc: caused a crash due to texsort (it could render twice...)
		// back side
		//side = dot >= -BACKFACE_EPSILON;
		if (dot < 0)
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
		else
		{
			for (;c;c--, surf++)
			{
				if (surf->visframe == r_framecount && (!(surf->flags & SURF_PLANEBACK)))
				{
					surf->texturechain = surf->texinfo->texture->texturechain;
					surf->texinfo->texture->texturechain = surf;
				}
			}
		}
	}

// recurse down the back side
	// LordHavoc: save a stack frame by avoiding a call
//	if (node->children[side]->contents != CONTENTS_SOLID && node->children[side]->visframe == r_visframecount && !R_CullBox (node->children[side]->minmaxs, node->children[side]->minmaxs+3))
	// LordHavoc: inlined further to reduce conditions
	if (node->children[side]->contents != CONTENTS_SOLID
	 && node->children[side]->visframe == r_visframecount
	 && frustum[0].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[0]) != 2
	 && frustum[1].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[1]) != 2
	 && frustum[2].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[2]) != 2
	 && frustum[3].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[3]) != 2)
	{
		node = node->children[side];
		goto loc0;
	}
//		R_RecursiveWorldNode (node->children[side]);
}
*/

extern int c_nodes;
void R_WorldNode ()
{
	int		c, side;
	double	dot;
	struct
	{
		double dot;
		mnode_t *node;
	} nodestack[1024];
	int		s = 0;
	mnode_t *node;

	if (!(node = cl.worldmodel->nodes))
		return;

	while(1)
	{
	// if a leaf node, draw stuff
		c_nodes++;
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				mleaf_t		*pleaf;
				pleaf = (mleaf_t *)node;

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

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
		dot = (node->plane->type < 3 ? modelorg[node->plane->type] : DotProduct (modelorg, node->plane->normal)) - node->plane->dist;

	// recurse down the children, front side first
		side = dot < 0;
		if (node->children[side]->visframe == r_visframecount
		 && frustum[0].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[0]) != 2
		 && frustum[1].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[1]) != 2
		 && frustum[2].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[2]) != 2
		 && frustum[3].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[3]) != 2)
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
		if (node->children[side]->visframe == r_visframecount
		 && frustum[0].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[0]) != 2
		 && frustum[1].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[1]) != 2
		 && frustum[2].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[2]) != 2
		 && frustum[3].BoxOnPlaneSideFunc(node->children[side]->minmaxs, node->children[side]->minmaxs+3, &frustum[3]) != 2)
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
	skyisvisible = false;

	if (cl.worldmodel)
		R_WorldNode ();

	glClear (GL_DEPTH_BUFFER_BIT);

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
				glTexImage2D (GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank);
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
	int			i, lindex, lnumverts;
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
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		poly->verts[i][7] = bound(0l, ((int)s>>4), (fa->extents[0]>>4)); // LordHavoc: raw lightmap coordinates
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		poly->verts[i][8] = bound(0l, ((int)t>>4), (fa->extents[1]>>4)); // LordHavoc: raw lightmap coordinates
		t += fa->light_t*16;
		t += 8;
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
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, lightmaps[i]);
			else
				glTexImage2D(GL_TEXTURE_2D, 0, 3, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmaps[i]);
		}
		if (gl_mtexable)
			qglSelectTexture(gl_mtex_enum+0);
	}
}

