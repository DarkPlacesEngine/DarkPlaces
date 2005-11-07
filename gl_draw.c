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

#include "quakedef.h"
#include "image.h"
#include "wad.h"

#include "cl_video.h"


static rtexture_t *char_texture;

//=============================================================================
/* Support Routines */

#define FONT_FILESIZE 13468
#define MAX_CACHED_PICS 256
#define CACHEPICHASHSIZE 256
static cachepic_t *cachepichash[CACHEPICHASHSIZE];
static cachepic_t cachepics[MAX_CACHED_PICS];
static int numcachepics;

static rtexturepool_t *drawtexturepool;

static unsigned char concharimage[FONT_FILESIZE] =
{
#include "lhfont.h"
};

static rtexture_t *draw_generateconchars(void)
{
	int i;
	unsigned char buffer[65536][4], *data = NULL;
	double random;

	data = LoadTGA (concharimage, FONT_FILESIZE, 256, 256);
// Gold numbers
	for (i = 0;i < 8192;i++)
	{
		random = lhrandom (0.0,1.0);
		buffer[i][0] = 83 + (unsigned char)(random * 64);
		buffer[i][1] = 71 + (unsigned char)(random * 32);
		buffer[i][2] = 23 + (unsigned char)(random * 16);
		buffer[i][3] = data[i*4+0];
	}
// White chars
	for (i = 8192;i < 32768;i++)
	{
		random = lhrandom (0.0,1.0);
		buffer[i][0] = 95 + (unsigned char)(random * 64);
		buffer[i][1] = 95 + (unsigned char)(random * 64);
		buffer[i][2] = 95 + (unsigned char)(random * 64);
		buffer[i][3] = data[i*4+0];
	}
// Gold numbers
	for (i = 32768;i < 40960;i++)
	{
		random = lhrandom (0.0,1.0);
		buffer[i][0] = 83 + (unsigned char)(random * 64);
		buffer[i][1] = 71 + (unsigned char)(random * 32);
		buffer[i][2] = 23 + (unsigned char)(random * 16);
		buffer[i][3] = data[i*4+0];
	}
// Red chars
	for (i = 40960;i < 65536;i++)
	{
		random = lhrandom (0.0,1.0);
		buffer[i][0] = 96 + (unsigned char)(random * 64);
		buffer[i][1] = 43 + (unsigned char)(random * 32);
		buffer[i][2] = 27 + (unsigned char)(random * 32);
		buffer[i][3] = data[i*4+0];
	}

#if 0
	Image_WriteTGARGBA ("gfx/generated_conchars.tga", 256, 256, &buffer[0][0]);
#endif

	Mem_Free(data);
	return R_LoadTexture2D(drawtexturepool, "conchars", 256, 256, &buffer[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
}

static char *pointerimage =
	"333333332......."
	"26777761........"
	"2655541........."
	"265541.........."
	"2654561........."
	"26414561........"
	"251.14561......."
	"21...14561......"
	"1.....141......."
	".......1........"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
;

static rtexture_t *draw_generatemousepointer(void)
{
	int i;
	unsigned char buffer[256][4];
	for (i = 0;i < 256;i++)
	{
		if (pointerimage[i] == '.')
		{
			buffer[i][0] = 0;
			buffer[i][1] = 0;
			buffer[i][2] = 0;
			buffer[i][3] = 0;
		}
		else
		{
			buffer[i][0] = (pointerimage[i] - '0') * 16;
			buffer[i][1] = (pointerimage[i] - '0') * 16;
			buffer[i][2] = (pointerimage[i] - '0') * 16;
			buffer[i][3] = 255;
		}
	}
	return R_LoadTexture2D(drawtexturepool, "mousepointer", 16, 16, &buffer[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
}

// must match NUMCROSSHAIRS in r_crosshairs.c
#define NUMCROSSHAIRS 6

static char *crosshairtexdata[NUMCROSSHAIRS] =
{
	"................"
	"................"
	"................"
	"...33......33..."
	"...355....553..."
	"....577..775...."
	".....77..77....."
	"................"
	"................"
	".....77..77....."
	"....577..775...."
	"...355....553..."
	"...33......33..."
	"................"
	"................"
	"................"
	,
	"................"
	"................"
	"................"
	"...3........3..."
	"....5......5...."
	".....7....7....."
	"......7..7......"
	"................"
	"................"
	"......7..7......"
	".....7....7....."
	"....5......5...."
	"...3........3..."
	"................"
	"................"
	"................"
	,
	"................"
	".......77......."
	".......77......."
	"................"
	"................"
	".......44......."
	".......44......."
	".77..44..44..77."
	".77..44..44..77."
	".......44......."
	".......44......."
	"................"
	"................"
	".......77......."
	".......77......."
	"................"
	,
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"........7777777."
	"........752....."
	"........72......"
	"........7......."
	"........7......."
	"........7......."
	"........7......."
	"................"
	,
	"................"
	"................"
	"................"
	"................"
	"................"
	"........7......."
	"................"
	"........4......."
	".....7.4.4.7...."
	"........4......."
	"................"
	"........7......."
	"................"
	"................"
	"................"
	"................"
	,
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	".......55......."
	".......55......."
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
	"................"
};

static rtexture_t *draw_generatecrosshair(int num)
{
	int i;
	char *in;
	unsigned char data[16*16][4];
	in = crosshairtexdata[num];
	for (i = 0;i < 16*16;i++)
	{
		if (in[i] == '.')
		{
			data[i][0] = 255;
			data[i][1] = 255;
			data[i][2] = 255;
			data[i][3] = 0;
		}
		else
		{
			data[i][0] = 255;
			data[i][1] = 255;
			data[i][2] = 255;
			data[i][3] = (unsigned char) ((int) (in[i] - '0') * 255 / 7);
		}
	}
	return R_LoadTexture2D(drawtexturepool, va("crosshair%i", num), 16, 16, &data[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
}

static rtexture_t *draw_generateditherpattern(void)
{
#if 1
	int x, y;
	unsigned char data[8*8*4];
	for (y = 0;y < 8;y++)
	{
		for (x = 0;x < 8;x++)
		{
			data[(y*8+x)*4+0] = data[(y*8+x)*4+1] = data[(y*8+x)*4+2] = ((x^y) & 4) ? 255 : 0;
			data[(y*8+x)*4+3] = 255;
		}
	}
	return R_LoadTexture2D(drawtexturepool, "ditherpattern", 8, 8, data, TEXTYPE_RGBA, TEXF_FORCENEAREST | TEXF_PRECACHE, NULL);
#else
	unsigned char data[16];
	memset(data, 255, sizeof(data));
	data[0] = data[1] = data[2] = data[12] = data[13] = data[14] = 0;
	return R_LoadTexture2D(drawtexturepool, "ditherpattern", 2, 2, data, TEXTYPE_RGBA, TEXF_FORCENEAREST | TEXF_PRECACHE, NULL);
#endif
}

/*
================
Draw_CachePic
================
*/
// FIXME: move this to client somehow
cachepic_t	*Draw_CachePic (const char *path, qboolean persistent)
{
	int i, crc, hashkey;
	cachepic_t *pic;
	qpic_t *p;
	int flags;

	if (!strncmp(CLVIDEOPREFIX, path, sizeof(CLVIDEOPREFIX) - 1))

	{
		clvideo_t *video;

		video = CL_GetVideo(path);
		if( video )
			return &video->cpif;
	}

	crc = CRC_Block((unsigned char *)path, strlen(path));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
		if (!strcmp (path, pic->name))
			return pic;

	if (numcachepics == MAX_CACHED_PICS)
	{
		Con_Printf ("Draw_CachePic: numcachepics == MAX_CACHED_PICS\n");
		// FIXME: support NULL in callers?
		return cachepics; // return the first one
	}
	pic = cachepics + (numcachepics++);
	strlcpy (pic->name, path, sizeof(pic->name));
	// link into list
	pic->chain = cachepichash[hashkey];
	cachepichash[hashkey] = pic;

	flags = TEXF_ALPHA;
	if (persistent)
		flags |= TEXF_PRECACHE;
	if (!strcmp(path, "gfx/colorcontrol/ditherpattern"))
		flags |= TEXF_CLAMP;

	// load the pic from disk
	pic->tex = loadtextureimage(drawtexturepool, path, 0, 0, false, flags);
	if (pic->tex == NULL && !strncmp(path, "gfx/", 4))
	{
		// compatibility with older versions
		pic->tex = loadtextureimage(drawtexturepool, path + 4, 0, 0, false, flags);
		// failed to find gfx/whatever.tga or similar, try the wad
		if (pic->tex == NULL && (p = (qpic_t *)W_GetLumpName (path + 4)))
		{
			if (!strcmp(path, "gfx/conchars"))
			{
				unsigned char *pix;
				// conchars is a raw image and with the wrong transparent color
				pix = (unsigned char *)p;
				for (i = 0;i < 128 * 128;i++)
					if (pix[i] == 0)
						pix[i] = 255;
				pic->tex = R_LoadTexture2D(drawtexturepool, path, 128, 128, pix, TEXTYPE_PALETTE, flags, palette_complete);
			}
			else
				pic->tex = R_LoadTexture2D(drawtexturepool, path, p->width, p->height, p->data, TEXTYPE_PALETTE, flags, palette_complete);
		}
	}

	if (pic->tex == NULL && !strcmp(path, "gfx/conchars"))
		pic->tex = draw_generateconchars();
	if (pic->tex == NULL && !strcmp(path, "ui/mousepointer"))
		pic->tex = draw_generatemousepointer();
	if (pic->tex == NULL && !strcmp(path, "gfx/prydoncursor001"))
		pic->tex = draw_generatemousepointer();
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair1"))
		pic->tex = draw_generatecrosshair(0);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair2"))
		pic->tex = draw_generatecrosshair(1);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair3"))
		pic->tex = draw_generatecrosshair(2);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair4"))
		pic->tex = draw_generatecrosshair(3);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair5"))
		pic->tex = draw_generatecrosshair(4);
	if (pic->tex == NULL && !strcmp(path, "gfx/crosshair6"))
		pic->tex = draw_generatecrosshair(5);
	if (pic->tex == NULL && !strcmp(path, "gfx/colorcontrol/ditherpattern"))
		pic->tex = draw_generateditherpattern();
	if (pic->tex == NULL)
	{
		Con_Printf("Draw_CachePic: failed to load %s\n", path);
		pic->tex = r_texture_notexture;
	}

	pic->width = R_TextureWidth(pic->tex);
	pic->height = R_TextureHeight(pic->tex);
	return pic;
}

cachepic_t *Draw_NewPic(const char *picname, int width, int height, int alpha, unsigned char *pixels)
{
	int crc, hashkey;
	cachepic_t *pic;

	crc = CRC_Block((unsigned char *)picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
		if (!strcmp (picname, pic->name))
			break;

	if (pic)
	{
		if (pic->tex && pic->width == width && pic->height == height)
		{
			R_UpdateTexture(pic->tex, pixels);
			return pic;
		}
	}
	else
	{
		if (pic == NULL)
		{
			if (numcachepics == MAX_CACHED_PICS)
			{
				Con_Printf ("Draw_NewPic: numcachepics == MAX_CACHED_PICS\n");
				// FIXME: support NULL in callers?
				return cachepics; // return the first one
			}
			pic = cachepics + (numcachepics++);
			strcpy (pic->name, picname);
			// link into list
			pic->chain = cachepichash[hashkey];
			cachepichash[hashkey] = pic;
		}
	}

	pic->width = width;
	pic->height = height;
	if (pic->tex)
		R_FreeTexture(pic->tex);
	pic->tex = R_LoadTexture2D(drawtexturepool, picname, width, height, pixels, TEXTYPE_RGBA, alpha ? TEXF_ALPHA : 0, NULL);
	return pic;
}

void Draw_FreePic(const char *picname)
{
	int crc;
	int hashkey;
	cachepic_t *pic;
	// this doesn't really free the pic, but does free it's texture
	crc = CRC_Block((unsigned char *)picname, strlen(picname));
	hashkey = ((crc >> 8) ^ crc) % CACHEPICHASHSIZE;
	for (pic = cachepichash[hashkey];pic;pic = pic->chain)
	{
		if (!strcmp (picname, pic->name) && pic->tex)
		{
			R_FreeTexture(pic->tex);
			pic->width = 0;
			pic->height = 0;
			return;
		}
	}
}

/*
===============
Draw_Init
===============
*/
static void gl_draw_start(void)
{
	drawtexturepool = R_AllocTexturePool();

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));

	char_texture = Draw_CachePic("gfx/conchars", true)->tex;
}

static void gl_draw_shutdown(void)
{
	R_FreeTexturePool(&drawtexturepool);

	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));
}

static void gl_draw_newmap(void)
{
}

void GL_Draw_Init (void)
{
	numcachepics = 0;
	memset(cachepichash, 0, sizeof(cachepichash));

	R_RegisterModule("GL_Draw", gl_draw_start, gl_draw_shutdown, gl_draw_newmap);
}

float blendvertex3f[9] = {-5000, -5000, 10, 10000, -5000, 10, -5000, 10000, 10};

int quadelements[768];
void R_DrawQueue(void)
{
	int pos, num, chartexnum, texnum, batch;
	float x, y, w, h, s, t, u, v, *av, *at, c[4];
	cachepic_t *pic;
	drawqueue_t *dq;
	char *str;
	int batchcount;
	unsigned int color;
	drawqueuemesh_t *mesh;
	rmeshstate_t m;

	if (!r_render.integer)
		return;

	if (!quadelements[1])
	{
		// elements for rendering a series of quads as triangles
		for (batch = 0, pos = 0, num = 0;batch < 128;batch++, num += 4)
		{
			quadelements[pos++] = num;
			quadelements[pos++] = num + 1;
			quadelements[pos++] = num + 2;
			quadelements[pos++] = num;
			quadelements[pos++] = num + 2;
			quadelements[pos++] = num + 3;
		}
	}

	r_view_width = bound(0, r_refdef.width, vid.width);
	r_view_height = bound(0, r_refdef.height, vid.height);
	r_view_depth = 1;
	r_view_x = bound(0, r_refdef.x, vid.width - r_refdef.width);
	r_view_y = bound(0, r_refdef.y, vid.height - r_refdef.height);
	r_view_z = 0;
	r_view_fov_x = bound(0.1, r_refdef.fov_x, 170);
	r_view_fov_y = bound(0.1, r_refdef.fov_y, 170);
	r_view_matrix = r_refdef.viewentitymatrix;
	GL_ColorMask(r_refdef.colormask[0], r_refdef.colormask[1], r_refdef.colormask[2], 1);

	qglViewport(r_view_x, vid.height - (r_view_y + r_view_height), r_view_width, r_view_height);
	GL_SetupView_Mode_Ortho(0, 0, vid_conwidth.integer, vid_conheight.integer, -10, 100);
	qglDepthFunc(GL_LEQUAL);
	R_Mesh_Matrix(&r_identitymatrix);

	chartexnum = R_GetTexture(char_texture);

	memset(&m, 0, sizeof(m));

	pic = NULL;
	texnum = 0;
	color = 0;
	GL_Color(1,1,1,1);

	batch = false;
	batchcount = 0;
	for (pos = 0;pos < r_refdef.drawqueuesize;pos += ((drawqueue_t *)(r_refdef.drawqueue + pos))->size)
	{
		dq = (drawqueue_t *)(r_refdef.drawqueue + pos);
		color = dq->color;

		if(dq->flags == DRAWFLAG_ADDITIVE)
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		else if(dq->flags == DRAWFLAG_MODULATE)
			GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		else if(dq->flags == DRAWFLAG_2XMODULATE)
			GL_BlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
		else
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		GL_DepthMask(true);
		GL_DepthTest(false);

		c[0] = (float) ((color >> 24) & 0xFF) * (1.0f / 255.0f);
		c[1] = (float) ((color >> 16) & 0xFF) * (1.0f / 255.0f);
		c[2] = (float) ((color >>  8) & 0xFF) * (1.0f / 255.0f);
		c[3] = (float) ( color        & 0xFF) * (1.0f / 255.0f);
		x = dq->x;
		y = dq->y;
		w = dq->scalex;
		h = dq->scaley;

		switch(dq->command)
		{
		case DRAWQUEUE_STRING:
			GL_Color(c[0], c[1], c[2], c[3]);
			str = (char *)(dq + 1);
			batchcount = 0;
			m.pointer_vertex = varray_vertex3f;
			m.pointer_color = NULL;
			m.pointer_texcoord[0] = varray_texcoord2f[0];
			m.tex[0] = chartexnum;
			R_Mesh_State(&m);
			at = varray_texcoord2f[0];
			av = varray_vertex3f;
			while ((num = *str++) && x < vid_conwidth.integer)
			{
				if (num != ' ')
				{
					s = (num & 15)*0.0625f + (0.5f / 256.0f);
					t = (num >> 4)*0.0625f + (0.5f / 256.0f);
					u = 0.0625f - (1.0f / 256.0f);
					v = 0.0625f - (1.0f / 256.0f);
					at[ 0] = s  ;at[ 1] = t  ;
					at[ 2] = s+u;at[ 3] = t  ;
					at[ 4] = s+u;at[ 5] = t+v;
					at[ 6] = s  ;at[ 7] = t+v;
					av[ 0] = x  ;av[ 1] = y  ;av[ 2] = 10;
					av[ 3] = x+w;av[ 4] = y  ;av[ 5] = 10;
					av[ 6] = x+w;av[ 7] = y+h;av[ 8] = 10;
					av[ 9] = x  ;av[10] = y+h;av[11] = 10;
					at += 8;
					av += 12;
					batchcount++;
					if (batchcount >= 128)
					{
						GL_LockArrays(0, batchcount * 4);
						R_Mesh_Draw(0, batchcount * 4, batchcount * 2, quadelements);
						GL_LockArrays(0, 0);
						batchcount = 0;
						at = varray_texcoord2f[0];
						av = varray_vertex3f;
					}
				}
				x += w;
			}
			if (batchcount > 0)
			{
				GL_LockArrays(0, batchcount * 4);
				R_Mesh_Draw(0, batchcount * 4, batchcount * 2, quadelements);
				GL_LockArrays(0, 0);
			}
			break;
		case DRAWQUEUE_MESH:
			mesh = (drawqueuemesh_t *)(dq + 1);
			m.pointer_vertex = mesh->data_vertex3f;
			m.pointer_color = mesh->data_color4f;
			m.pointer_texcoord[0] = mesh->data_texcoord2f;
			m.tex[0] = R_GetTexture(mesh->texture);
			if (!m.tex[0])
				m.pointer_texcoord[0] = NULL;
			R_Mesh_State(&m);
			GL_LockArrays(0, mesh->num_vertices);
			R_Mesh_Draw(0, mesh->num_vertices, mesh->num_triangles, mesh->data_element3i);
			GL_LockArrays(0, 0);
			break;
		case DRAWQUEUE_SETCLIP:
			{
				// We have to convert the con coords into real coords
				int x , y, width, height;
				x = dq->x * ((float)vid.width / vid_conwidth.integer);
				// OGL uses top to bottom
				y = dq->y * ((float) vid.height / vid_conheight.integer);
				width = dq->scalex * ((float)vid.width / vid_conwidth.integer);
				height = dq->scaley * ((float)vid.height / vid_conheight.integer);

				GL_Scissor(x, y, width, height);

				GL_ScissorTest(true);
			}
			break;
		case DRAWQUEUE_RESETCLIP:
			GL_ScissorTest(false);
			break;
		}
	}

	if (!vid_usinghwgamma)
	{
		// all the blends ignore depth
		memset(&m, 0, sizeof(m));
		m.pointer_vertex = blendvertex3f;
		R_Mesh_State(&m);
		GL_DepthMask(true);
		GL_DepthTest(false);
		if (v_color_enable.integer)
		{
			c[0] = v_color_white_r.value;
			c[1] = v_color_white_g.value;
			c[2] = v_color_white_b.value;
		}
		else
			c[0] = c[1] = c[2] = v_contrast.value;
		if (c[0] >= 1.01f || c[1] >= 1.01f || c[2] >= 1.01f)
		{
			GL_BlendFunc(GL_DST_COLOR, GL_ONE);
			while (c[0] >= 1.01f || c[1] >= 1.01f || c[2] >= 1.01f)
			{
				GL_Color(bound(0, c[0] - 1, 1), bound(0, c[1] - 1, 1), bound(0, c[2] - 1, 1), 1);
				R_Mesh_Draw(0, 3, 1, polygonelements);
				VectorScale(c, 0.5, c);
			}
		}
		if (v_color_enable.integer)
		{
			c[0] = v_color_black_r.value;
			c[1] = v_color_black_g.value;
			c[2] = v_color_black_b.value;
		}
		else
			c[0] = c[1] = c[2] = v_brightness.value;
		if (c[0] >= 0.01f || c[1] >= 0.01f || c[2] >= 0.01f)
		{
			GL_BlendFunc(GL_ONE, GL_ONE);
			GL_Color(c[0], c[1], c[2], 1);
			R_Mesh_Draw(0, 3, 1, polygonelements);
		}
	}
}

