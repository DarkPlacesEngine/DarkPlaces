
#include "quakedef.h"

void LoadSky_f(void);

cvar_t r_skyquality = {CVAR_SAVE, "r_skyquality", "2"};
cvar_t r_skyflush = {0, "r_skyflush", "0"};

static char skyworldname[1024];
rtexture_t *solidskytexture;
rtexture_t *alphaskytexture;
static qboolean skyavailable_quake;
static qboolean skyavailable_box;
static rtexturepool_t *skytexturepool;

int skyrendernow;
int skyrendermasked;
int skyrenderglquake;

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

int R_SetSkyBox(char *sky);

static void r_sky_newmap(void)
{
	skyavailable_quake = false;
	if (!strcmp(skyworldname, cl.worldmodel->name))
		skyavailable_quake = true;
}

void R_Sky_Init(void)
{
	Cmd_AddCommand ("loadsky", &LoadSky_f);
	Cvar_RegisterVariable (&r_skyquality);
	Cvar_RegisterVariable (&r_skyflush);
	R_RegisterModule("R_Sky", r_sky_start, r_sky_shutdown, r_sky_newmap);
}

static int skyrendersphere;
static int skyrenderbox;

void R_SkyStartFrame(void)
{
	skyrendernow = false;
	skyrendersphere = false;
	skyrenderbox = false;
	skyrenderglquake = false;
	skyrendermasked = false;
	if (r_skyquality.integer >= 1 && !fogenabled)
	{
		if (skyavailable_box)
			skyrenderbox = true;
		else if (skyavailable_quake)
		{
			switch(r_skyquality.integer)
			{
			case 1:
				skyrenderglquake = true;
				break;
			default:
			case 2:
				skyrendersphere = true;
				break;
			}
		}
		if (skyrenderbox || skyrendersphere)
		{
			// for depth-masked sky, render the sky on the first sky surface encountered
			skyrendernow = true;
			skyrendermasked = true;
		}
	}
}

static char skyname[256];

/*
==================
R_SetSkyBox
==================
*/
static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static rtexture_t *skyboxside[6];
int R_SetSkyBox(char *sky)
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

#define R_SkyBoxPolyVec(i,s,t,x,y,z) \
	vert[i][0] = (x) * 1024.0f + r_origin[0];\
	vert[i][1] = (y) * 1024.0f + r_origin[1];\
	vert[i][2] = (z) * 1024.0f + r_origin[2];\
	st[i][0] = (s) * (254.0f/256.0f) + (1.0f/256.0f);\
	st[i][1] = (t) * (254.0f/256.0f) + (1.0f/256.0f);

int skyboxindex[6] = {0, 1, 2, 0, 2, 3};

static void R_SkyBox(void)
{
	float vert[4][4], st[4][2];
	rmeshinfo_t m;
	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.numtriangles = 2;
	m.numverts = 4;
	m.index = skyboxindex;
	m.vertex = &vert[0][0];
	m.vertexstep = sizeof(float[4]);
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = 1;
	m.texcoords[0] = &st[0][0];
	m.texcoordstep[0] = sizeof(float[2]);
	m.tex[0] = R_GetTexture(skyboxside[3]); // front
	R_SkyBoxPolyVec(0, 1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1,  1,  1, -1);
	R_SkyBoxPolyVec(3, 0, 0,  1,  1,  1);
	R_Mesh_Draw(&m);
	m.tex[0] = R_GetTexture(skyboxside[1]); // back
	R_SkyBoxPolyVec(0, 1, 0, -1,  1,  1);
	R_SkyBoxPolyVec(1, 1, 1, -1,  1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1, -1,  1);
	R_Mesh_Draw(&m);
	m.tex[0] = R_GetTexture(skyboxside[0]); // right
	R_SkyBoxPolyVec(0, 1, 0,  1,  1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1,  1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1,  1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1,  1,  1);
	R_Mesh_Draw(&m);
	m.tex[0] = R_GetTexture(skyboxside[2]); // left
	R_SkyBoxPolyVec(0, 1, 0, -1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1, -1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1,  1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0,  1, -1,  1);
	R_Mesh_Draw(&m);
	m.tex[0] = R_GetTexture(skyboxside[4]); // up
	R_SkyBoxPolyVec(0, 1, 0,  1, -1,  1);
	R_SkyBoxPolyVec(1, 1, 1,  1,  1,  1);
	R_SkyBoxPolyVec(2, 0, 1, -1,  1,  1);
	R_SkyBoxPolyVec(3, 0, 0, -1, -1,  1);
	R_Mesh_Draw(&m);
	m.tex[0] = R_GetTexture(skyboxside[5]); // down
	R_SkyBoxPolyVec(0, 1, 0,  1,  1, -1);
	R_SkyBoxPolyVec(1, 1, 1,  1, -1, -1);
	R_SkyBoxPolyVec(2, 0, 1, -1, -1, -1);
	R_SkyBoxPolyVec(3, 0, 0, -1,  1, -1);
	R_Mesh_Draw(&m);
}

#define skygridx 16
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

static void skyspherearrays(float *vert, float *tex, float *tex2, float *source, float s, float s2)
{
	float *v, *t, *t2, radius;
	int i;
	v = vert;
	t = tex;
	t2 = tex2;
	radius = r_farclip - 8;
	for (i = 0;i < (skygridx1*skygridy1);i++)
	{
		*t++ = source[0] + s;
		*t++ = source[1] + s;
		*t2++ = source[0] + s2;
		*t2++ = source[1] + s2;
		*v++ = source[2] + r_origin[0];
		*v++ = source[3] + r_origin[1];
		*v++ = source[4] + r_origin[2];
		*v++ = 0;
		source += 5;
	}
}

static void R_SkySphere(void)
{
	float speedscale, speedscale2;
	float vert[skygridx1*skygridy1*4], tex[skygridx1*skygridy1*2], tex2[skygridx1*skygridy1*2];
	static qboolean skysphereinitialized = false;
	rmeshinfo_t m;
	if (!skysphereinitialized)
	{
		skysphereinitialized = true;
		skyspherecalc(skysphere, 1024, 1024, 1024 / 3);
	}
	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.numtriangles = skygridx*skygridy*2;
	m.numverts = skygridx1*skygridy1;
	m.index = skysphereindices;
	m.vertex = vert;
	m.vertexstep = sizeof(float[4]);
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = 1;
	m.texcoords[0] = tex;
	m.texcoordstep[0] = sizeof(float[2]);
	speedscale = cl.time*8.0/128.0;
	speedscale -= (int)speedscale;
	speedscale2 = cl.time*16.0/128.0;
	speedscale2 -= (int)speedscale2;
	skyspherearrays(vert, tex, tex2, skysphere, speedscale, speedscale2);
	m.tex[0] = R_GetTexture(solidskytexture);
	R_Mesh_Draw(&m);

	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
	m.tex[0] = R_GetTexture(alphaskytexture);
	m.texcoords[0] = tex2;
	R_Mesh_Draw(&m);
}

void R_Sky(void)
{
	if (skyrendermasked)
	{
		if (skyrendersphere)
			R_SkySphere();
		else if (skyrenderbox)
			R_SkyBox();

		// clear the zbuffer that was used while rendering the sky
		R_Mesh_ClearDepth();
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

	strcpy(skyworldname, loadmodel->name);

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

