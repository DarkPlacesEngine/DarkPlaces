#include "quakedef.h"

void LoadSky_f(void);

cvar_t r_skyquality = {CVAR_SAVE, "r_skyquality", "2"};
cvar_t r_mergesky = {CVAR_SAVE, "r_mergesky", "0"};
cvar_t r_skyflush = {0, "r_skyflush", "0"};

static char skyworldname[1024];
rtexture_t *mergeskytexture;
rtexture_t *solidskytexture;
rtexture_t *alphaskytexture;
static qboolean skyavailable_quake;
static qboolean skyavailable_box;
static rtexturepool_t *skytexturepool;

int skyrendernow;
int skyrendermasked;
int skyrenderglquake;

static void R_BuildSky (int scrollupper, int scrolllower);

static void r_sky_start(void)
{
	skytexturepool = R_AllocTexturePool();
	mergeskytexture = NULL;
	solidskytexture = NULL;
	alphaskytexture = NULL;
}

static void r_sky_shutdown(void)
{
	R_FreeTexturePool(&skytexturepool);
	mergeskytexture = NULL;
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
	Cvar_RegisterVariable (&r_mergesky);
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
		if (r_mergesky.integer && (skyrenderglquake || skyrendersphere))
		{
	//		R_BuildSky((int) (cl.time * 8.0), (int) (cl.time * 16.0));
	//		R_BuildSky((int) (cl.time * -8.0), 0);
			R_BuildSky(0, (int) (cl.time * 8.0));
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
	int		i;
	char	name[1024];
	byte*	image_rgba;

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
	vert[i][4] = (s) * (254.0f/256.0f) + (1.0f/256.0f);\
	vert[i][5] = (t) * (254.0f/256.0f) + (1.0f/256.0f);

int skyboxindex[6] = {0, 1, 2, 0, 2, 3};

static void R_SkyBox(void)
{
	float vert[4][6];
	rmeshinfo_t m;
	memset(&m, 0, sizeof(m));
	m.transparent = false;
	m.blendfunc1 = GL_ONE;
	m.blendfunc2 = GL_ZERO;
	m.numtriangles = 2;
	m.numverts = 4;
	m.index = skyboxindex;
	m.vertex = &vert[0][0];
	m.vertexstep = sizeof(float[6]);
	m.cr = 1;
	m.cg = 1;
	m.cb = 1;
	m.ca = 1;
	m.texcoords[0] = &vert[0][4];
	m.texcoordstep[0] = sizeof(float[6]);
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
	R_Mesh_Render();
	if (r_skyflush.integer)
		glFlush();
	// clear the zbuffer that was used while rendering the sky
	glClear(GL_DEPTH_BUFFER_BIT);
	if (r_skyflush.integer)
		glFlush();
}

static float skysphere[33*33*5];
static int skysphereindices[32*32*6];
static void skyspherecalc(float *sphere, float dx, float dy, float dz)
{
	float a, b, x, ax, ay, v[3], length;
	int i, j, *index;
	for (a = 0;a <= 1;a += (1.0 / 32.0))
	{
		ax = cos(a * M_PI * 2);
		ay = -sin(a * M_PI * 2);
		for (b = 0;b <= 1;b += (1.0 / 32.0))
		{
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
	for (j = 0;j < 32;j++)
	{
		for (i = 0;i < 32;i++)
		{
			*index++ =  j      * 33 + i;
			*index++ =  j      * 33 + i + 1;
			*index++ = (j + 1) * 33 + i;

			*index++ =  j      * 33 + i + 1;
			*index++ = (j + 1) * 33 + i + 1;
			*index++ = (j + 1) * 33 + i;
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
	for (i = 0;i < (33*33);i++)
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
	float vert[33*33*4], tex[33*33*2], tex2[33*33*2];
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
	m.numtriangles = 32*32*2;
	m.numverts = 33*33;
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
	// do not lock the texcoord array, because it will be switched
	if (r_mergesky.integer)
	{
		m.tex[0] = R_GetTexture(mergeskytexture);
		R_Mesh_Draw(&m);
	}
	else
	{
		m.tex[0] = R_GetTexture(solidskytexture);
		R_Mesh_Draw(&m);

		m.blendfunc1 = GL_SRC_ALPHA;
		m.blendfunc2 = GL_ONE_MINUS_SRC_ALPHA;
		m.tex[0] = R_GetTexture(alphaskytexture);
		m.texcoords[0] = tex2;
		R_Mesh_Draw(&m);
	}
	R_Mesh_Render();
	if (r_skyflush.integer)
		glFlush();
	// clear the zbuffer that was used while rendering the sky
	glClear(GL_DEPTH_BUFFER_BIT);
	if (r_skyflush.integer)
		glFlush();
}

void R_Sky(void)
{
	if (skyrendersphere)
		R_SkySphere();
	else if (skyrenderbox)
		R_SkyBox();
}

//===============================================================

static byte skyupperlayerpixels[128*128*4];
static byte skylowerlayerpixels[128*128*4];
static byte skymergedpixels[128*128*4];

static void R_BuildSky (int scrollupper, int scrolllower)
{
	int x, y, ux, uy, lx, ly;
	byte *m, *u, *l;
	m = skymergedpixels;
	for (y = 0;y < 128;y++)
	{
		uy = (y + scrollupper) & 127;
		ly = (y + scrolllower) & 127;
		for (x = 0;x < 128;x++)
		{
			ux = (x + scrollupper) & 127;
			lx = (x + scrolllower) & 127;
			u = &skyupperlayerpixels[(uy * 128 + ux) * 4];
			l = &skylowerlayerpixels[(ly * 128 + lx) * 4];
			if (l[3])
			{
				if (l[3] == 255)
					*((int *)m) = *((int *)l);
				else
				{
					m[0] = ((((int) l[0] - (int) u[0]) * (int) l[3]) >> 8) + (int) u[0];
					m[1] = ((((int) l[1] - (int) u[1]) * (int) l[3]) >> 8) + (int) u[1];
					m[2] = ((((int) l[2] - (int) u[2]) * (int) l[3]) >> 8) + (int) u[2];
					m[3] = 255;
				}
			}
			else
				*((int *)m) = *((int *)u);
			m += 4;
		}
	}
	if (mergeskytexture)
		R_UpdateTexture(mergeskytexture, skymergedpixels);
	else
		mergeskytexture = R_LoadTexture(skytexturepool, "mergedskytexture", 128, 128, skymergedpixels, TEXTYPE_RGBA, TEXF_ALWAYSPRECACHE);
}

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (byte *src, int bytesperpixel)
{
	int			i, j, p;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;

	strcpy(skyworldname, loadmodel->name);
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
				r += ((byte *)rgba)[0];
				g += ((byte *)rgba)[1];
				b += ((byte *)rgba)[2];
			}
		}

		((byte *)&transpix)[0] = r/(128*128);
		((byte *)&transpix)[1] = g/(128*128);
		((byte *)&transpix)[2] = b/(128*128);
		((byte *)&transpix)[3] = 0;
	}

	memcpy(skyupperlayerpixels, trans, 128*128*4);

	solidskytexture = R_LoadTexture (skytexturepool, "sky_solidtexture", 128, 128, (byte *) trans, TEXTYPE_RGBA, TEXF_PRECACHE);
	/*
	for (i = 0;i < 128*128;i++)
	{
		((byte *)&trans[i])[0] >>= 1;
		((byte *)&trans[i])[1] >>= 1;
		((byte *)&trans[i])[2] >>= 1;
	}
	solidskytexture_half = R_LoadTexture (skytexturepool, "sky_solidtexture_half", 128, 128, (byte *) trans, TEXTYPE_RGBA, TEXF_PRECACHE);
	*/

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

	alphaskytexture = R_LoadTexture (skytexturepool, "sky_alphatexture", 128, 128, (byte *) trans, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
	/*
	for (i = 0;i < 128*128;i++)
	{
		((byte *)&trans[i])[0] >>= 1;
		((byte *)&trans[i])[1] >>= 1;
		((byte *)&trans[i])[2] >>= 1;
	}
	alphaskytexture_half = R_LoadTexture (skytexturepool, "sky_alphatexture_half", 128, 128, (byte *) trans, TEXTYPE_RGBA, TEXF_ALPHA | TEXF_PRECACHE);
	*/
}
