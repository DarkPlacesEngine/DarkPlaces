
#include "quakedef.h"
#include "cl_collision.h"

cvar_t crosshair_brightness = {CVAR_SAVE, "crosshair_brightness", "1"};
cvar_t crosshair_alpha = {CVAR_SAVE, "crosshair_alpha", "1"};
cvar_t crosshair_flashspeed = {CVAR_SAVE, "crosshair_flashspeed", "2"};
cvar_t crosshair_flashrange = {CVAR_SAVE, "crosshair_flashrange", "0.1"};
cvar_t crosshair_size = {CVAR_SAVE, "crosshair_size", "1"};
cvar_t crosshair_static = {CVAR_SAVE, "crosshair_static", "0"};

// must match NUMCROSSHAIRS in gl_draw.c
#define NUMCROSSHAIRS 5

static qbyte *crosshairtexdata[NUMCROSSHAIRS] =
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
	".......77......."
	".......77......."
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
	"................"
	"........7777777."
	"........752....."
	"........72......"
	"........7......."
	"........7......."
	"........7......."
	"................"
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
};

rtexturepool_t *crosshairtexturepool;
rtexture_t *crosshairtexture[NUMCROSSHAIRS];

void r_crosshairs_start(void)
{
	int num;
	int i;
	char *in;
	qbyte data[16*16][4];
	crosshairtexturepool = R_AllocTexturePool();
	for (num = 0;num < 5;num++)
	{
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
				data[i][3] = (qbyte) ((int) (in[i] - '0') * 255 / 7);
			}
		}
		crosshairtexture[num] = R_LoadTexture(crosshairtexturepool, va("crosshair%i", num), 16, 16, &data[0][0], TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
	}
}

void r_crosshairs_shutdown(void)
{
	R_FreeTexturePool(&crosshairtexturepool);
}

void r_crosshairs_newmap(void)
{
}

void R_Crosshairs_Init(void)
{
	Cvar_RegisterVariable(&crosshair_brightness);
	Cvar_RegisterVariable(&crosshair_alpha);
	Cvar_RegisterVariable(&crosshair_flashspeed);
	Cvar_RegisterVariable(&crosshair_flashrange);
	Cvar_RegisterVariable(&crosshair_size);
	Cvar_RegisterVariable(&crosshair_static);
	R_RegisterModule("R_Crosshair", r_crosshairs_start, r_crosshairs_shutdown, r_crosshairs_newmap);
}

void R_DrawCrosshairSprite(rtexture_t *texture, vec3_t origin, vec_t scale, float cr, float cg, float cb, float ca)
{
	rmeshstate_t m;
	float diff[3];

	if (fogenabled)
	{
		VectorSubtract(origin, r_origin, diff);
		ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
	}

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.wantoverbright = false;
	m.depthdisable = true;
	m.tex[0] = R_GetTexture(texture);
	R_Mesh_Matrix(&r_identitymatrix);
	R_Mesh_State(&m);

	varray_color[0] = varray_color[4] = varray_color[8] = varray_color[12] = cr * mesh_colorscale;
	varray_color[1] = varray_color[5] = varray_color[9] = varray_color[13] = cg * mesh_colorscale;
	varray_color[2] = varray_color[6] = varray_color[10] = varray_color[14] = cb * mesh_colorscale;
	varray_color[3] = varray_color[7] = varray_color[11] = varray_color[15] = ca;
	varray_texcoord[0][0] = 0;
	varray_texcoord[0][1] = 0;
	varray_texcoord[0][2] = 0;
	varray_texcoord[0][3] = 1;
	varray_texcoord[0][4] = 1;
	varray_texcoord[0][5] = 1;
	varray_texcoord[0][6] = 1;
	varray_texcoord[0][7] = 0;
	varray_vertex[0] = origin[0] - vright[0] * scale - vup[0] * scale;
	varray_vertex[1] = origin[1] - vright[1] * scale - vup[1] * scale;
	varray_vertex[2] = origin[2] - vright[2] * scale - vup[2] * scale;
	varray_vertex[4] = origin[0] - vright[0] * scale + vup[0] * scale;
	varray_vertex[5] = origin[1] - vright[1] * scale + vup[1] * scale;
	varray_vertex[6] = origin[2] - vright[2] * scale + vup[2] * scale;
	varray_vertex[8] = origin[0] + vright[0] * scale + vup[0] * scale;
	varray_vertex[9] = origin[1] + vright[1] * scale + vup[1] * scale;
	varray_vertex[10] = origin[2] + vright[2] * scale + vup[2] * scale;
	varray_vertex[12] = origin[0] + vright[0] * scale - vup[0] * scale;
	varray_vertex[13] = origin[1] + vright[1] * scale - vup[1] * scale;
	varray_vertex[14] = origin[2] + vright[2] * scale - vup[2] * scale;
	R_Mesh_Draw(4, 2, polygonelements);
}

void R_DrawCrosshair(void)
{
	int i, num;
	qbyte *color;
	float scale, base;
	vec3_t v1, v2, spriteorigin;
	vec_t spritescale;
	float cr, cg, cb, ca;
	num = crosshair.integer - 1;
	if (num < 0 || cl.intermission)
		return;
	if (num >= NUMCROSSHAIRS)
		num = 0;
	if (cl.viewentity >= 1 && cl.viewentity <= cl.maxclients)
	{
		i = (cl.scores[cl.viewentity-1].colors & 0xF) << 4;
		if (i >= 208 && i < 224) // blue
			i += 8;
		else if (i < 128 || i >= 224) // 128-224 are backwards ranges (bright to dark, rather than dark to bright)
			i += 15;
	}
	else
		i = 15;
	color = (qbyte *) &d_8to24table[i];
	if (crosshair_flashspeed.value >= 0.01f)
		base = (sin(realtime * crosshair_flashspeed.value * (M_PI*2.0f)) * crosshair_flashrange.value);
	else
		base = 0.0f;
	scale = crosshair_brightness.value * (1.0f / 255.0f);

	if (crosshair_static.integer)
	{
		char *picname;
		cachepic_t *pic;

		picname = va("gfx/crosshair%i.tga", num + 1);
		pic = Draw_CachePic(picname);
		if (pic)
			DrawQ_Pic((vid.conwidth - pic->width * crosshair_size.value) * 0.5f, (vid.conheight - pic->height * crosshair_size.value) * 0.5f, picname, pic->width * crosshair_size.value, pic->height * crosshair_size.value, color[0] * scale + base, color[1] * scale + base, color[2] * scale + base, crosshair_alpha.value, 0);
	}
	else
	{
		// trace the shot path up to a certain distance
		VectorCopy(cl_entities[cl.viewentity].render.origin, v1);
		v1[2] += 16; // HACK: this depends on the QC
		// get the forward vector for the gun (not the view)
		AngleVectors(cl.viewangles, v2, NULL, NULL);
		//VectorCopy(r_origin, v1);
		VectorMA(v1, 8192, v2, v2);
		spritescale = 4.0f + (CL_TraceLine(v1, v2, spriteorigin, NULL, 0, true) * 8192.0f) * (1.0f / 48.0f);
		spritescale = bound(0.0f, spritescale, 32.0f);
		//VectorMA(spriteorigin, -4, vpn, spriteorigin);

		cr = color[0] * scale + base;
		cg = color[1] * scale + base;
		cb = color[2] * scale + base;
		ca = crosshair_alpha.value;

		// clamp the colors so they don't go negative
		cr = max(0, cr);
		cg = max(0, cg);
		cb = max(0, cb);
		// might as well clamp the alpha
		ca = bound(0, ca, 1.0f);

		// finally draw the sprite
		R_DrawCrosshairSprite(crosshairtexture[num], spriteorigin, spritescale, cr, cg, cb, ca);
	}
}


