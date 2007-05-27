
#include "quakedef.h"
#include "image.h"

// FIXME: fix skybox after vid_restart
cvar_t r_sky = {CVAR_SAVE, "r_sky", "1", "enables sky rendering (black otherwise)"};
cvar_t r_skyscroll1 = {CVAR_SAVE, "r_skyscroll1", "1", "speed at which upper clouds layer scrolls in quake sky"};
cvar_t r_skyscroll2 = {CVAR_SAVE, "r_skyscroll2", "2", "speed at which lower clouds layer scrolls in quake sky"};
int skyrendernow;
int skyrendermasked;

static int skyrendersphere;
static int skyrenderbox;
static rtexturepool_t *skytexturepool;
static char skyname[MAX_QPATH];

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
		{"rt",   false, false,  true},
		{"lf",    true,  true,  true},
		{"bk",   false,  true, false},
		{"ft",    true, false, false},
		{"up",   false, false,  true},
		{"dn",   false, false,  true}
	}
};

static rtexture_t *skyboxside[6];

void R_SkyStartFrame(void)
{
	skyrendernow = false;
	skyrendersphere = false;
	skyrenderbox = false;
	skyrendermasked = false;
	if (r_sky.integer && !r_refdef.fogenabled)
	{
		if (skyboxside[0] || skyboxside[1] || skyboxside[2] || skyboxside[3] || skyboxside[4] || skyboxside[5])
			skyrenderbox = true;
		else if (r_refdef.worldmodel && r_refdef.worldmodel->brush.solidskytexture)
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

int R_LoadSkyBox(void)
{
	int i, j, success;
	int indices[4] = {0,1,2,3};
	char name[MAX_INPUTLINE];
	unsigned char *image_rgba;
	unsigned char *temp;

	R_UnloadSkyBox();

	if (!skyname[0])
		return true;

	for (j=0; j<3; j++)
	{
		success = 0;
		for (i=0; i<6; i++)
		{
			if (dpsnprintf(name, sizeof(name), "%s_%s", skyname, suffix[j][i].suffix) < 0 || !(image_rgba = loadimagepixels(name, false, 0, 0)))
			{
				if (dpsnprintf(name, sizeof(name), "%s%s", skyname, suffix[j][i].suffix) < 0 || !(image_rgba = loadimagepixels(name, false, 0, 0)))
				{
					if (dpsnprintf(name, sizeof(name), "env/%s%s", skyname, suffix[j][i].suffix) < 0 || !(image_rgba = loadimagepixels(name, false, 0, 0)))
					{
						if (dpsnprintf(name, sizeof(name), "gfx/env/%s%s", skyname, suffix[j][i].suffix) < 0 || !(image_rgba = loadimagepixels(name, false, 0, 0)))
							continue;
					}
				}
			}
			temp = (unsigned char *)Mem_Alloc(tempmempool, image_width*image_height*4);
			Image_CopyMux (temp, image_rgba, image_width, image_height, suffix[j][i].flipx, suffix[j][i].flipy, suffix[j][i].flipdiagonal, 4, 4, indices);
			skyboxside[i] = R_LoadTexture2D(skytexturepool, va("skyboxside%d", i), image_width, image_height, temp, TEXTYPE_RGBA, TEXF_CLAMP | TEXF_PRECACHE, NULL);
			Mem_Free(image_rgba);
			Mem_Free(temp);
			success++;
		}

		if (success)
			break;
	}

	if (j == 3)
		return false;

	return true;
}

int R_SetSkyBox(const char *sky)
{
	if (strcmp(sky, skyname) == 0) // no change
		return true;

	if (strlen(sky) > 1000)
	{
		Con_Printf("sky name too long (%i, max is 1000)\n", (int)strlen(sky));
		return false;
	}

	strlcpy(skyname, sky, sizeof(skyname));

	return R_LoadSkyBox();
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
	// FIXME: fixed function path can't properly handle r_view.colorscale > 1
	GL_Color(1 * r_view.colorscale, 1 * r_view.colorscale, 1 * r_view.colorscale, 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(false);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(false); // don't modify or read zbuffer
	R_Mesh_VertexPointer(skyboxvertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_Mesh_TexCoordPointer(0, 2, skyboxtexcoord2f, 0, 0);
	GL_LockArrays(0, 6*4);
	for (i = 0;i < 6;i++)
	{
		R_Mesh_TexBind(0, R_GetTexture(skyboxside[i]));
		R_Mesh_Draw(0, 6*4, 2, skyboxelements + i * 6, 0, 0);
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
	matrix4x4_t scroll1matrix, scroll2matrix;
	if (!skysphereinitialized)
	{
		skysphereinitialized = true;
		skyspherecalc();
	}

	// wrap the scroll values just to be extra kind to float accuracy

	// scroll speed for upper layer
	speedscale = r_refdef.time*r_skyscroll1.value*8.0/128.0;
	speedscale -= (int)speedscale;
	Matrix4x4_CreateTranslate(&scroll1matrix, speedscale, speedscale, 0);
	// scroll speed for lower layer (transparent layer)
	speedscale = r_refdef.time*r_skyscroll2.value*8.0/128.0;
	speedscale -= (int)speedscale;
	Matrix4x4_CreateTranslate(&scroll2matrix, speedscale, speedscale, 0);

	// FIXME: fixed function path can't properly handle r_view.colorscale > 1
	GL_Color(1 * r_view.colorscale, 1 * r_view.colorscale, 1 * r_view.colorscale, 1);
	GL_BlendFunc(GL_ONE, GL_ZERO);
	GL_DepthMask(true);
	GL_DepthRange(0, 1);
	GL_PolygonOffset(0, 0);
	GL_DepthTest(false); // don't modify or read zbuffer
	R_Mesh_VertexPointer(skysphere_vertex3f, 0, 0);
	R_Mesh_ColorPointer(NULL, 0, 0);
	R_Mesh_ResetTextureState();
	R_Mesh_TexBind(0, R_GetTexture(r_refdef.worldmodel->brush.solidskytexture));
	R_Mesh_TexCoordPointer(0, 2, skysphere_texcoord2f, 0, 0);
	R_Mesh_TexMatrix(0, &scroll1matrix);
	if (r_textureunits.integer >= 2 && r_view.colorscale == 1)
	{
		// one pass using GL_DECAL or GL_INTERPOLATE_ARB for alpha layer
		R_Mesh_TexBind(1, R_GetTexture(r_refdef.worldmodel->brush.alphaskytexture));
		R_Mesh_TexCombine(1, gl_combine.integer ? GL_INTERPOLATE_ARB : GL_DECAL, GL_MODULATE, 1, 1);
		R_Mesh_TexCoordPointer(1, 2, skysphere_texcoord2f, 0, 0);
		R_Mesh_TexMatrix(1, &scroll2matrix);
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(0, skysphere_numverts, skysphere_numtriangles, skysphere_element3i, 0, 0);
		GL_LockArrays(0, 0);
	}
	else
	{
		// two pass
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(0, skysphere_numverts, skysphere_numtriangles, skysphere_element3i, 0, 0);

		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		R_Mesh_TexBind(0, R_GetTexture(r_refdef.worldmodel->brush.alphaskytexture));
		R_Mesh_TexMatrix(0, &scroll2matrix);
		GL_LockArrays(0, skysphere_numverts);
		R_Mesh_Draw(0, skysphere_numverts, skysphere_numtriangles, skysphere_element3i, 0, 0);
		GL_LockArrays(0, 0);
	}
}

void R_Sky(void)
{
	matrix4x4_t skymatrix;
	if (skyrendermasked)
	{
		Matrix4x4_CreateTranslate(&skymatrix, r_view.origin[0], r_view.origin[1], r_view.origin[2]);
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
		GL_DepthRange(0, 1);
		GL_DepthTest(true);
		GL_DepthMask(true);
	}
}

//===============================================================

void R_ResetSkyBox(void)
{
	R_UnloadSkyBox();
	skyname[0] = 0;
	R_LoadSkyBox();
}

static void r_sky_start(void)
{
	skytexturepool = R_AllocTexturePool();
	R_LoadSkyBox();
}

static void r_sky_shutdown(void)
{
	R_UnloadSkyBox();
	R_FreeTexturePool(&skytexturepool);
}

static void r_sky_newmap(void)
{
}


void R_Sky_Init(void)
{
	Cmd_AddCommand ("loadsky", &LoadSky_f, "load a skybox by basename (for example loadsky mtnsun_ loads mtnsun_ft.tga and so on)");
	Cvar_RegisterVariable (&r_sky);
	Cvar_RegisterVariable (&r_skyscroll1);
	Cvar_RegisterVariable (&r_skyscroll2);
	memset(&skyboxside, 0, sizeof(skyboxside));
	skyname[0] = 0;
	R_RegisterModule("R_Sky", r_sky_start, r_sky_shutdown, r_sky_newmap);
}

