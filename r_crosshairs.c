
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
	rmeshbufferinfo_t m;
	float diff[3];

	if (fogenabled)
	{
		VectorSubtract(origin, r_origin, diff);
		ca *= 1 - exp(fogdensity/DotProduct(diff,diff));
	}

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	m.depthdisable = true;
	m.numtriangles = 2;
	m.numverts = 4;
	m.tex[0] = R_GetTexture(texture);
	Matrix4x4_CreateIdentity(&m.matrix);
	if (R_Mesh_Draw_GetBuffer(&m, false))
	{
		m.index[0] = 0;
		m.index[1] = 1;
		m.index[2] = 2;
		m.index[3] = 0;
		m.index[4] = 2;
		m.index[5] = 3;
		m.color[0] = m.color[4] = m.color[8] = m.color[12] = cr * m.colorscale;
		m.color[1] = m.color[5] = m.color[9] = m.color[13] = cg * m.colorscale;
		m.color[2] = m.color[6] = m.color[10] = m.color[14] = cb * m.colorscale;
		m.color[3] = m.color[7] = m.color[11] = m.color[15] = ca;
		m.texcoords[0][0] = 0;
		m.texcoords[0][1] = 0;
		m.texcoords[0][2] = 0;
		m.texcoords[0][3] = 1;
		m.texcoords[0][4] = 1;
		m.texcoords[0][5] = 1;
		m.texcoords[0][6] = 1;
		m.texcoords[0][7] = 0;
		m.vertex[0] = origin[0] - vright[0] * scale - vup[0] * scale;
		m.vertex[1] = origin[1] - vright[1] * scale - vup[1] * scale;
		m.vertex[2] = origin[2] - vright[2] * scale - vup[2] * scale;
		m.vertex[4] = origin[0] - vright[0] * scale + vup[0] * scale;
		m.vertex[5] = origin[1] - vright[1] * scale + vup[1] * scale;
		m.vertex[6] = origin[2] - vright[2] * scale + vup[2] * scale;
		m.vertex[8] = origin[0] + vright[0] * scale + vup[0] * scale;
		m.vertex[9] = origin[1] + vright[1] * scale + vup[1] * scale;
		m.vertex[10] = origin[2] + vright[2] * scale + vup[2] * scale;
		m.vertex[12] = origin[0] + vright[0] * scale - vup[0] * scale;
		m.vertex[13] = origin[1] + vright[1] * scale - vup[1] * scale;
		m.vertex[14] = origin[2] + vright[2] * scale - vup[2] * scale;
		R_Mesh_Render();
	}
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


