
#include "quakedef.h"
#include "image.h"

// FIXME: fix skybox after vid_restart
cvar_t r_sky = {CVAR_SAVE, "r_sky", "1"};
qboolean skyavailable_quake;
qboolean skyavailable_box;
int skyrendernow;
int skyrendermasked;

static rtexture_t *solidskytexture;
static rtexture_t *alphaskytexture;
static int skyrendersphere;
static int skyrenderbox;
static rtexturepool_t *skytexturepool;
static char skyname[256];
static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static rtexture_t *skyboxside[6];

void R_SkyStartFrame(void)
{
	skyrendernow = false;
	skyrendersphere = false;
	skyrenderbox = false;
	skyrendermasked = false;
	if (r_sky.integer && !fogenabled)
	{
		if (skyavailable_box)
			skyrenderbox = true;
		else if (skyavailable_quake)
			skyrendersphere = true;
		// for depth-masked sky, render the sky on the first sky surface encountered
		skyrendernow = true;
		skyrendermasked = true;
	}
}

/*
==================
R_SetSkyBox
==================
*/
int R_SetSkyBox(const char *sky)
{
	int i;
	char name[1024];
	qbyte *image_rgba;

	if (strcmp(sky, skyname) == 0) // no change
		return true;

	skyboxside[0] = skyboxside[1] = skyboxside[2] = skyboxside[3] = skyboxside[4] = skyboxside[5] = NULL;
	skyavailable_box = false;
	skyname[0] = 0;

	if (!sky[0])
		return true;

	if (strlen(sky) > 1000)
	{
		Con_Printf ("sky name too long (%i, max is 1000)\n", strlen(sky));
		return false;
	}

	for (i = 0;i < 6;i++)
	{
		sprintf (name, "env/%s%s", sky, suf[i]);
		if (!(image_rgba = loadimagepixels(name, false, 0, 0)))
		{
			sprintf (name, "gfx/env/%s%s", sky, suf[i]);
			if (!(image_rgba = loadimagepixels(name, false, 0, 0)))
			{
				Con_Printf ("Couldn't load env/%s%s or gfx/env/%s%s\n", sky, suf[i], sky, suf[i]);
				continue;
			}
		}
		skyboxside[i] = R_LoadTexture2D(skytexturepool, va("skyboxside%d", i), image_width, image_height, image_rgba, TEXTYPE_RGBA, TEXF_CLAMP | TEXF_PRECACHE, NULL);
		Mem_Free(image_rgba);
	}

	if (skyboxside[0] || skyboxside[1] || skyboxside[2] || skyboxside[3] || skyboxside[4] || skyboxside[5])
	{
		skyavailable_box = true;
		strcpy(skyname, sky);
		return true;
	}
	return false;
}

// LordHavoc: added LoadSky console command
void LoadSky_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		if (skyname[0])
			Con_Printf("current sky: %s\n", skyname);
		else
			Con_Printf("no skybox has been set\n");
		break;
	case 2:
		if (R_SetSkyBox(Cmd_Argv(1)))
		{
			if (skyname[0])
				Con_Printf("skybox set to %s\n", skyname);
			else
				Con_Printf("skybox disabled\n");
		}
		else
			Con_Printf("failed to load skybox %s\n", Cmd_Argv(1));
		break;
	default:
		Con_Printf("usage: loadsky skyname\n");
		break;
	}
}

float skyboxvertex3f[6*4*3] =
{
	// skyside[0]
	 16,  16,  16,
	 16,  16, -16,
	-16,  16, -16,
	-16,  16,  16,
	// skyside[1]
	-16,  16,  16,
	-16,  16, -16,
	-16, -16, -16,
	-16, -16,  16,
	// skyside[2]
	-16, -16,  16,
	-16, -16, -16,
	 16, -16, -16,
	 16, -16,  16,
	// skyside[3]
	 16, -16,  16,
	 16, -16, -16,
	 16,  16, -16,
	 16,  16,  16,
	// skyside[4]
	 16, -16,  16,
	 16,  16,  16,
	-16,  16,  16,
	-16, -16,  16,
	// skyside[5]
	 16,  16, -16,
	 16, -16, -16,
	-16, -16, -16,
	-16,  16, -16
};

float skyboxtexcoord2f[6*4*2] =
{
	// skyside[0]
	1, 0,
	1, 1,
	0, 1,
	0, 0,
	// skyside[1]
	1, 0,
	1, 1,
	0, 1,
	0, 0,
	// skyside[2]
	1, 0,
	1, 1,
	0, 1,
	0, 0,
	// skyside[3]
	1, 0,
	1, 1,
	0, 1,
	0, 0,
	// skyside[4]
	1, 0,
	1, 1,
	0, 1,
	0, 0,
	// skyside[5]
	1, 0,
	1, 1,
	0, 1,
	0, 0
};

int skyboxelements[6*2*3] =
{
	// skyside[3]
	 0,  1,  2,
	 0,  2,  3,
	// skyside[1]
	 4,  5,  6,
	 4,  6,  7,
	// skyside[0]
	 8,  9, 10,
	 8, 10, 11,
	// skyside[2]
	12, 13, 14,
	12, 14, 15,
	// skyside[4]
	16, 17, 18,
	16, 18, 19,
	// skyside[5]
	20, 21, 22,
	20, 22, 23
};

static void R_SkyBox(void)
{
	int i;
	rmeshstate_t m;
	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(false); // don't modify or read zbuffer
	GL_VertexPointer(skyboxvertex3f);
	m.pointer_texcoord[0] = skyboxtexcoord2f;
	for (i = 0;i < 6;i++)
	{
		m.tex[0] = R_GetTexture(skyboxside[i]);
		R_Mesh_State_Texture(&m);
		R_Mesh_Draw(6*4, 2, skyboxelements + i * 6);
	}
}

#define skygridx 32
#define skygridx1 (skygridx + 1)
#define skygridxrecip (1.0f / (skygridx))
#define skygridy 32
#define skygridy1 (skygridy + 1)
#define skygridyrecip (1.0f / (skygridy))
#define skysphere_numverts (skygridx1 * skygridy1)
#define skysphere_numtriangles (skygridx * skygridy * 2)
static float skysphere_vertex3f[skysphere_numverts * 3];
static float skysphere_texcoord2f[skysphere_numverts * 2];
static int skysphere_element3i[skysphere_numtriangles * 3];

static void skyspherecalc(void)
{
	int i, j, *e;
	float a, b, x, ax, ay, v[3], length, *vertex3f, *texcoord2f;
	float dx, dy, dz;
	dx = 16;
	dy = 16;
	dz = 16 / 3;
	vertex3f = skysphere_vertex3f;
	texcoord2f = skysphere_texcoord2f;
	for (j = 0;j <= skygridy;j++)
	{
		a = j * skygridyrecip;
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (i = 0;i <= skygridx;i++)
		{
			b = i * skygridxrecip;
			x = cos((b + 0.5) * M_PI);
			v[0] = ax*x * dx;
			v[1] = ay*x * dy;
			v[2] = -sin((b + 0.5) * M_PI) * dz;
			length = 3.0f / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));
			*texcoord2f++ = v[0] * length;
			*texcoord2f++ = v[1] * length;
			*vertex3f++ = v[0];
			*vertex3f++ = v[1];
			*vertex3f++ = v[2];
		}
	}
	e = skysphere_element3i;
	for (j = 0;j < skygridy;j++)
	{
		for (i = 0;i < skygridx;i++)
		{
			*e++ =  j      * skygridx1 + i;
			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;

			*e++ =  j      * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i + 1;
			*e++ = (j + 1) * skygridx1 + i;
		}
	}
}

static void R_SkySphere(void)
{
	float speedscale;
	static qboolean skysphereinitialized = false;
	rmeshstate_t m;
	matrix4x4_t scroll1matrix, scroll2matrix, identitymatrix;
	if (!skysphereinitialized)
	{
		skysphereinitialized = true;
		skyspherecalc();
	}

	// scroll speed for upper layer
	speedscale = cl.time*8.0/128.0;
	// wrap the scroll just to be extra kind to float accuracy
	speedscale -= (int)speedscale;

	// scroll the lower cloud layer twice as fast (just like quake did)
	Matrix4x4_CreateTranslate(&scroll1matrix, speedscale, speedscale, 0);
	Matrix4x4_CreateTranslate(&scroll2matrix, speedscale * 2, speedscale * 2, 0);
	Matrix4x4_CreateIdentity(&identitymatrix);

	GL_VertexPointer(skysphere_vertex3f);
	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(false); // don't modify or read zbuffer
	memset(&m, 0, sizeof(m));
	m.tex[0] = R_GetTexture(solidskytexture);
	m.pointer_texcoord[0] = skysphere_texcoord2f;
	R_Mesh_TextureMatrix(0, &scroll1matrix);
	if (r_colorscale == 1 && r_textureunits.integer >= 2)
	{
		// one pass using GL_DECAL or GL_INTERPOLATE_ARB for alpha layer
		// LordHavoc: note that color is not set here because it does not
		// matter with GL_REPLACE
		m.tex[1] = R_GetTexture(alphaskytexture);
		m.texcombinergb[1] = gl_combine.integer ? GL_INTERPOLATE_ARB : GL_DECAL;
		m.pointer_texcoord[1] = skysphere_texcoord2f;
		R_Mesh_State_Texture(&m);
		R_Mesh_TextureMatrix(1, &scroll2matrix);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);
		R_Mesh_TextureMatrix(1, &identitymatrix);
	}
	else
	{
		// two pass
		R_Mesh_State_Texture(&m);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);

		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		m.tex[0] = R_GetTexture(alphaskytexture);
		R_Mesh_State_Texture(&m);
		R_Mesh_TextureMatrix(0, &scroll2matrix);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);
	}
	R_Mesh_TextureMatrix(0, &identitymatrix);
}

void R_Sky(void)
{
	matrix4x4_t skymatrix;
	if (skyrendermasked)
	{
		Matrix4x4_CreateTranslate(&skymatrix, r_origin[0], r_origin[1], r_origin[2]);
		R_Mesh_Matrix(&skymatrix);
		if (skyrendersphere)
		{
			// this does not modify depth buffer
			R_SkySphere();
		}
		else if (skyrenderbox)
		{
			// this does not modify depth buffer
			R_SkyBox();
		}
		/* this will be skyroom someday
		else
		{
			// this modifies the depth buffer so we have to clear it afterward
			//R_SkyRoom();
			// clear the depthbuffer that was used while rendering the skyroom
			//qglClear(GL_DEPTH_BUFFER_BIT);
		}
		*/
	}
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (qbyte *src, int bytesperpixel)
{
	int i, j, p, r, g, b;
	qbyte skyupperlayerpixels[128*128*4], skylowerlayerpixels[128*128*4];
	unsigned trans[128*128], transpix, *rgba;
	union
	{
		int i;
		qbyte b[4];
	}
	transpixunion;

	skyavailable_quake = true;

	// flush skytexturepool so we won't build up a leak from uploading textures multiple times
	R_FreeTexturePool(&skytexturepool);
	skytexturepool = R_AllocTexturePool();
	solidskytexture = NULL;
	alphaskytexture = NULL;

	if (bytesperpixel == 4)
	{
		transpixunion.i = 0;
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = ((unsigned *)src)[i*256+j+128];
	}
	else
	{
		// make an average value for the back to avoid
		// a fringe on the top level
		r = g = b = 0;
		for (i=0 ; i<128 ; i++)
		{
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j + 128];
				rgba = &palette_complete[p];
				trans[(i*128) + j] = *rgba;
				r += ((qbyte *)rgba)[0];
				g += ((qbyte *)rgba)[1];
				b += ((qbyte *)rgba)[2];
			}
		}

		transpixunion.i = 0;
		transpixunion.b[0] = r/(128*128);
		transpixunion.b[1] = g/(128*128);
		transpixunion.b[2] = b/(128*128);
		transpixunion.b[3] = 0;
	}
	transpix = transpixunion.i;

	memcpy(skyupperlayerpixels, trans, 128*128*4);

	solidskytexture = R_LoadTexture2D(skytexturepool, "sky_solidtexture", 128, 128, (qbyte *) trans, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = ((unsigned *)src)[i*256+j];
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			for (j=0 ; j<128 ; j++)
			{
				p = src[i*256 + j];
				if (p == 0)
					trans[(i*128) + j] = transpix;
				else
					trans[(i*128) + j] = palette_complete[p];
			}
		}
	}

	memcpy(skylowerlayerpixels, trans, 128*128*4);

	alphaskytexture = R_LoadTexture2D(skytexturepool, "sky_alphatexture", 128, 128, (qbyte *) trans, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
}

void R_ResetQuakeSky(void)
{
	skyavailable_quake = false;
}

void R_ResetSkyBox(void)
{
	skyboxside[0] = skyboxside[1] = skyboxside[2] = skyboxside[3] = skyboxside[4] = skyboxside[5] = NULL;
	skyname[0] = 0;
	skyavailable_box = false;
}

static void r_sky_start(void)
{
	skytexturepool = R_AllocTexturePool();
	solidskytexture = NULL;
	alphaskytexture = NULL;
}

static void r_sky_shutdown(void)
{
	R_FreeTexturePool(&skytexturepool);
	solidskytexture = NULL;
	alphaskytexture = NULL;
}

static void r_sky_newmap(void)
{
}

void R_Sky_Init(void)
{
	Cmd_AddCommand ("loadsky", &LoadSky_f);
	Cvar_RegisterVariable (&r_sky);
	R_RegisterModule("R_Sky", r_sky_start, r_sky_shutdown, r_sky_newmap);
}

