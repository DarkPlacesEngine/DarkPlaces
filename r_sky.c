
#include "quakedef.h"

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
		skyboxside[i] = R_LoadTexture(skytexturepool, va("skyboxside%d", i), image_width, image_height, image_rgba, TEXTYPE_RGBA, TEXF_PRECACHE);
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

static void R_SkyBox(void)
{
	rmeshstate_t m;

#define R_SkyBoxPolyVec(i,s,t,x,y,z) \
	varray_vertex[i * 4 + 0] = (x) * 16.0f;\
	varray_vertex[i * 4 + 1] = (y) * 16.0f;\
	varray_vertex[i * 4 + 2] = (z) * 16.0f;\
	varray_texcoord[0][i * 2 + 0] = (s) * (254.0f/256.0f) + (1.0f/256.0f);\
	varray_texcoord[0][i * 2 + 1] = (t) * (254.0f/256.0f) + (1.0f/256.0f);

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.depthdisable = true; // don't modify or read zbuffer
	m.tex[0] = R_GetTexture(skyboxside[3]); // front
	R_Mesh_State(&m);

	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);
	
	R_SkyBoxPolyVec(0, 1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1,  1,  1, -1);
	R_SkyBoxPolyVec(3, 0, 0,  1,  1,  1);
	R_Mesh_Draw(4, 2, polygonelements);
	m.tex[0] = R_GetTexture(skyboxside[1]); // back
	R_Mesh_State(&m);
	R_SkyBoxPolyVec(0, 1, 0, -1,  1,  1);
	R_SkyBoxPolyVec(1, 1, 1, -1,  1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1, -1,  1);
	R_Mesh_Draw(4, 2, polygonelements);
	m.tex[0] = R_GetTexture(skyboxside[0]); // right
	R_Mesh_State(&m);
	R_SkyBoxPolyVec(0, 1, 0,  1,  1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1,  1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1,  1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1,  1,  1);
	R_Mesh_Draw(4, 2, polygonelements);
	m.tex[0] = R_GetTexture(skyboxside[2]); // left
	R_Mesh_State(&m);
	R_SkyBoxPolyVec(0, 1, 0, -1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1, -1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1,  1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0,  1, -1,  1);
	R_Mesh_Draw(4, 2, polygonelements);
	m.tex[0] = R_GetTexture(skyboxside[4]); // up
	R_Mesh_State(&m);
	R_SkyBoxPolyVec(0, 1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1,  1,  1);
	R_SkyBoxPolyVec(2, 0, 1, -1,  1,  1);
	R_SkyBoxPolyVec(3, 0, 0, -1, -1,  1);
	R_Mesh_Draw(4, 2, polygonelements);
	m.tex[0] = R_GetTexture(skyboxside[5]); // down
	R_Mesh_State(&m);
	R_SkyBoxPolyVec(0, 1, 0,  1,  1, -1);
	R_SkyBoxPolyVec(1, 1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1,  1, -1);
	R_Mesh_Draw(4, 2, polygonelements);
}

#define skygridx 32
#define skygridx1 (skygridx + 1)
#define skygridxrecip (1.0f / (skygridx))
#define skygridy 32
#define skygridy1 (skygridy + 1)
#define skygridyrecip (1.0f / (skygridy))

static float skysphere[skygridx1*skygridy1*5];
static int skysphereindices[skygridx*skygridy*6];
static void skyspherecalc(float *sphere, float dx, float dy, float dz)
{
	float a, b, x, ax, ay, v[3], length;
	int i, j, *index;
	for (j = 0;j <= skygridy;j++)
	{
		a = j * skygridyrecip;
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (i = 0;i <= skygridx;i++)
		{
			b = i * skygridxrecip;
			x = cos(b * M_PI * 2);
			v[0] = ax*x * dx;
			v[1] = ay*x * dy;
			v[2] = -sin(b * M_PI * 2) * dz;
			length = 3.0f / sqrt(v[0]*v[0]+v[1]*v[1]+(v[2]*v[2]*9));
			*sphere++ = v[0] * length;
			*sphere++ = v[1] * length;
			*sphere++ = v[0];
			*sphere++ = v[1];
			*sphere++ = v[2];
		}
	}
	index = skysphereindices;
	for (j = 0;j < skygridy;j++)
	{
		for (i = 0;i < skygridx;i++)
		{
			*index++ =  j      * skygridx1 + i;
			*index++ =  j      * skygridx1 + i + 1;
			*index++ = (j + 1) * skygridx1 + i;

			*index++ =  j      * skygridx1 + i + 1;
			*index++ = (j + 1) * skygridx1 + i + 1;
			*index++ = (j + 1) * skygridx1 + i;
		}
		i++;
	}
}

static void skyspherearrays(float *v, float *t, float *source, float s)
{
	int i;
	for (i = 0;i < (skygridx1*skygridy1);i++, t += 2, v += 4, source += 5)
	{
		t[0] = source[0] + s;
		t[1] = source[1] + s;
		v[0] = source[2];
		v[1] = source[3];
		v[2] = source[4];
	}
}

static void R_SkySphere(void)
{
	int numverts, numtriangles;
	float speedscale, speedscale2;
	static qboolean skysphereinitialized = false;
	rmeshstate_t m;
	if (!skysphereinitialized)
	{
		skysphereinitialized = true;
		skyspherecalc(skysphere, 16, 16, 16 / 3);
	}

	speedscale = cl.time*8.0/128.0;
	speedscale -= (int)speedscale;
	speedscale2 = cl.time*16.0/128.0;
	speedscale2 -= (int)speedscale2;

	numverts = skygridx1*skygridy1;
	numtriangles = skygridx*skygridy*2;

	R_Mesh_ResizeCheck(numverts);

	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.depthdisable = true; // don't modify or read zbuffer
	m.tex[0] = R_GetTexture(solidskytexture);
	R_Mesh_State(&m);

	GL_Color(r_colorscale, r_colorscale, r_colorscale, 1);

	skyspherearrays(varray_vertex, varray_texcoord[0], skysphere, speedscale);
	R_Mesh_Draw(numverts, numtriangles, skysphereindices);

	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.tex[0] = R_GetTexture(alphaskytexture);
	R_Mesh_State(&m);

	skyspherearrays(varray_vertex, varray_texcoord[0], skysphere, speedscale2);
	R_Mesh_Draw(numverts, numtriangles, skysphereindices);
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

	skyavailable_quake = true;

	// flush skytexturepool so we won't build up a leak from uploading textures multiple times
	R_FreeTexturePool(&skytexturepool);
	skytexturepool = R_AllocTexturePool();
	solidskytexture = NULL;
	alphaskytexture = NULL;

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j+128];
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
				rgba = &d_8to24table[p];
				trans[(i*128) + j] = *rgba;
				r += ((qbyte *)rgba)[0];
				g += ((qbyte *)rgba)[1];
				b += ((qbyte *)rgba)[2];
			}
		}

		((qbyte *)&transpix)[0] = r/(128*128);
		((qbyte *)&transpix)[1] = g/(128*128);
		((qbyte *)&transpix)[2] = b/(128*128);
		((qbyte *)&transpix)[3] = 0;
	}

	memcpy(skyupperlayerpixels, trans, 128*128*4);

	solidskytexture = R_LoadTexture (skytexturepool, "sky_solidtexture", 128, 128, (qbyte *) trans, TEXTYPE_RGBA, TEXF_PRECACHE);

	if (bytesperpixel == 4)
	{
		for (i = 0;i < 128;i++)
			for (j = 0;j < 128;j++)
				trans[(i*128) + j] = src[i*256+j];
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
					trans[(i*128) + j] = d_8to24table[p];
			}
		}
	}

	memcpy(skylowerlayerpixels, trans, 128*128*4);

	alphaskytexture = R_LoadTexture (skytexturepool, "sky_alphatexture", 128, 128, (qbyte *) trans, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
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
