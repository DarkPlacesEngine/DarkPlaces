
#include "quakedef.h"
#include "image.h"

// FIXME: fix skybox after vid_restart
cvar_t r_sky = {CVAR_SAVE, "r_sky", "1"};
cvar_t r_skyscroll1 = {CVAR_SAVE, "r_skyscroll1", "1"};
cvar_t r_skyscroll2 = {CVAR_SAVE, "r_skyscroll2", "2"};
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

typedef struct suffixinfo_s
{
	char *suffix;
	qboolean flipx, flipy, flipdiagonal;
}
suffixinfo_t;
static suffixinfo_t suffix[3][6] =
{
	{
		{"px",   false, false, false},
		{"nx",   false, false, false},
		{"py",   false, false, false},
		{"ny",   false, false, false},
		{"pz",   false, false, false},
		{"nz",   false, false, false}
	},
	{
		{"posx", false, false, false},
		{"negx", false, false, false},
		{"posy", false, false, false},
		{"negy", false, false, false},
		{"posz", false, false, false},
		{"negz", false, false, false}
	},
	{
		{"rt",    true, false,  true},
		{"lf",   false,  true,  true},
		{"ft",    true,  true, false},
		{"bk",   false, false, false},
		{"up",    true, false,  true},
		{"dn",    true, false,  true}
	}
};

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
void R_UnloadSkyBox(void)
{
	int i;
	for (i = 0;i < 6;i++)
	{
		if (skyboxside[i])
			R_FreeTexture(skyboxside[i]);
		skyboxside[i] = NULL;
	}
}

void R_LoadSkyBox(void)
{
	int i, j;
	int indices[4] = {0,1,2,3};
	char name[1024];
	qbyte *image_rgba;
	qbyte *temp;
	R_UnloadSkyBox();
	if (!skyname[0])
		return;

	for (j=0; j<3; j++)
	{
		for (i=0; i<6; i++)
		{
			if (snprintf(name, sizeof(name), "%s_%s", skyname, suffix[j][i].suffix) >= (int)sizeof(name) || !(image_rgba = loadimagepixels(name, false, 0, 0)))
			{
				if (snprintf(name, sizeof(name), "%s%s", skyname, suffix[j][i].suffix) >= (int)sizeof(name) || !(image_rgba = loadimagepixels(name, false, 0, 0)))
				{
					if (snprintf(name, sizeof(name), "env/%s%s", skyname, suffix[j][i].suffix) >= (int)sizeof(name) || !(image_rgba = loadimagepixels(name, false, 0, 0)))
					{
						if (snprintf(name, sizeof(name), "gfx/env/%s%s", skyname, suffix[j][i].suffix) >= (int)sizeof(name) || !(image_rgba = loadimagepixels(name, false, 0, 0)))
							continue;
					}
				}
			}
			temp = Mem_Alloc(tempmempool, image_width*image_height*4);
			Image_CopyMux (temp, image_rgba, image_width, image_height, suffix[j][i].flipx, suffix[j][i].flipy, suffix[j][i].flipdiagonal, 4, 4, indices);
			skyboxside[i] = R_LoadTexture2D(skytexturepool, va("skyboxside%d", i), image_width, image_height, temp, TEXTYPE_RGBA, TEXF_CLAMP | TEXF_PRECACHE, NULL);
			Mem_Free(image_rgba);
			Mem_Free(temp);
		}

		for (i=0; i<6; i++)
		{
			if (skyboxside[i] == NULL)
			{
				R_UnloadSkyBox();
				break;
			}
		}

		if (skyboxside[0] != NULL)
			return;
	}

	Con_Printf ("Failed to load %s as skybox\n", skyname);
}

int R_SetSkyBox(const char *sky)
{
	if (strcmp(sky, skyname) == 0) // no change
		return true;

	if (strlen(sky) > 1000)
	{
		Con_Printf("sky name too long (%i, max is 1000)\n", strlen(sky));
		return false;
	}

	skyavailable_box = false;
	strcpy(skyname, sky);

	R_UnloadSkyBox();
	R_LoadSkyBox();

	if (!skyname[0])
		return true;

	if (skyboxside[0] || skyboxside[1] || skyboxside[2] || skyboxside[3] || skyboxside[4] || skyboxside[5])
	{
		skyavailable_box = true;
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
			Con_Print("no skybox has been set\n");
		break;
	case 2:
		if (R_SetSkyBox(Cmd_Argv(1)))
		{
			if (skyname[0])
				Con_Printf("skybox set to %s\n", skyname);
			else
				Con_Print("skybox disabled\n");
		}
		else
			Con_Printf("failed to load skybox %s\n", Cmd_Argv(1));
		break;
	default:
		Con_Print("usage: loadsky skyname\n");
		break;
	}
}

float skyboxvertex3f[6*4*3] =
{
	// skyside[0]
	 16, -16,  16,
	 16, -16, -16,
	 16,  16, -16,
	 16,  16,  16,
	// skyside[1]
	-16,  16,  16,
	-16,  16, -16,
	-16, -16, -16,
	-16, -16,  16,
	// skyside[2]
	 16,  16,  16,
	 16,  16, -16,
	-16,  16, -16,
	-16,  16,  16,
	// skyside[3]
	-16, -16,  16,
	-16, -16, -16,
	 16, -16, -16,
	 16, -16,  16,
	// skyside[4]
	-16, -16,  16,
	 16, -16,  16,
	 16,  16,  16,
	-16,  16,  16,
	// skyside[5]
	 16, -16, -16,
	-16, -16, -16,
	-16,  16, -16,
	 16,  16, -16
};

float skyboxtexcoord2f[6*4*2] =
{
	// skyside[0]
	0, 1,
	1, 1,
	1, 0,
	0, 0,
	// skyside[1]
	1, 0,
	0, 0,
	0, 1,
	1, 1,
	// skyside[2]
	1, 1,
	1, 0,
	0, 0,
	0, 1,
	// skyside[3]
	0, 0,
	0, 1,
	1, 1,
	1, 0,
	// skyside[4]
	0, 1,
	1, 1,
	1, 0,
	0, 0,
	// skyside[5]
	0, 1,
	1, 1,
	1, 0,
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
	GL_Color(1, 1, 1, 1);
	memset(&m, 0, sizeof(m));
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(false); // don't modify or read zbuffer
	m.pointer_vertex = skyboxvertex3f;
	m.pointer_texcoord[0] = skyboxtexcoord2f;
	GL_LockArrays(0, 6*4);
	for (i = 0;i < 6;i++)
	{
		m.tex[0] = R_GetTexture(skyboxside[i]);
		R_Mesh_State(&m);
		R_Mesh_Draw(6*4, 2, skyboxelements + i * 6);
	}
	GL_LockArrays(0, 0);
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
	matrix4x4_t scroll1matrix, scroll2matrix;
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
	Matrix4x4_CreateTranslate(&scroll1matrix, speedscale * r_skyscroll1.value, speedscale * r_skyscroll1.value, 0);
	Matrix4x4_CreateTranslate(&scroll2matrix, speedscale * r_skyscroll2.value, speedscale * r_skyscroll2.value, 0);

	GL_Color(1, 1, 1, 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthTest(false); // don't modify or read zbuffer
	memset(&m, 0, sizeof(m));
	m.pointer_vertex = skysphere_vertex3f;
	m.tex[0] = R_GetTexture(solidskytexture);
	m.pointer_texcoord[0] = skysphere_texcoord2f;
	m.texmatrix[0] = scroll1matrix;
	if (r_textureunits.integer >= 2)
	{
		// one pass using GL_DECAL or GL_INTERPOLATE_ARB for alpha layer
		m.tex[1] = R_GetTexture(alphaskytexture);
		m.texcombinergb[1] = gl_combine.integer ? GL_INTERPOLATE_ARB : GL_DECAL;
		m.pointer_texcoord[1] = skysphere_texcoord2f;
		m.texmatrix[1] = scroll2matrix;
		R_Mesh_State(&m);
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);
		GL_LockArrays(0, 0);
	}
	else
	{
		// two pass
		R_Mesh_State(&m);
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);
		GL_LockArrays(0, 0);

		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		m.tex[0] = R_GetTexture(alphaskytexture);
		m.texmatrix[0] = scroll2matrix;
		R_Mesh_State(&m);
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(skysphere_numverts, skysphere_numtriangles, skysphere_element3i);
		GL_LockArrays(0, 0);
	}
}

void R_Sky(void)
{
	matrix4x4_t skymatrix;
	if (skyrendermasked)
	{
		Matrix4x4_CreateTranslate(&skymatrix, r_vieworigin[0], r_vieworigin[1], r_vieworigin[2]);
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
			//GL_Clear(GL_DEPTH_BUFFER_BIT);
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
	int i, j;
	unsigned solidpixels[128*128], alphapixels[128*128];

	skyavailable_quake = true;

	// flush skytexturepool so we won't build up a leak from uploading textures multiple times
	R_FreeTexturePool(&skytexturepool);
	skytexturepool = R_AllocTexturePool();
	solidskytexture = NULL;
	alphaskytexture = NULL;

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				solidpixels[(i*128) + j] = ((unsigned *)src)[i*256+j+128];
				alphapixels[(i*128) + j] = ((unsigned *)src)[i*256+j];
			}
		}
	}
	else
	{
		// make an average value for the back to avoid
		// a fringe on the top level
		int p, r, g, b;
		union
		{
			unsigned int i;
			unsigned char b[4];
		}
		rgba;
		r = g = b = 0;
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				rgba.i = palette_complete[src[i*256 + j + 128]];
				r += rgba.b[0];
				g += rgba.b[1];
				b += rgba.b[2];
			}
		}
		rgba.b[0] = r/(128*128);
		rgba.b[1] = g/(128*128);
		rgba.b[2] = b/(128*128);
		rgba.b[3] = 0;
		for (i = 0;i < 128;i++)
		{
			for (j = 0;j < 128;j++)
			{
				solidpixels[(i*128) + j] = palette_complete[src[i*256 + j + 128]];
				alphapixels[(i*128) + j] = (p = src[i*256 + j]) ? palette_complete[p] : rgba.i;
			}
		}
	}

	solidskytexture = R_LoadTexture2D(skytexturepool, "sky_solidtexture", 128, 128, (qbyte *) solidpixels, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	alphaskytexture = R_LoadTexture2D(skytexturepool, "sky_alphatexture", 128, 128, (qbyte *) alphapixels, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE, NULL);
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
	R_LoadSkyBox();
}

static void r_sky_shutdown(void)
{
	R_UnloadSkyBox();
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
	Cvar_RegisterVariable (&r_skyscroll1);
	Cvar_RegisterVariable (&r_skyscroll2);
	R_ResetSkyBox();
	R_ResetQuakeSky();
	R_RegisterModule("R_Sky", r_sky_start, r_sky_shutdown, r_sky_newmap);
}

