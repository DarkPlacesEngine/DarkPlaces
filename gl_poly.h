
extern int R_SetSkyBox(char* sky);
extern cvar_t r_skyquality;

#define TPOLYTYPE_ALPHA 0
#define TPOLYTYPE_ADD 1

extern void transpolyclear(void);
extern void transpolyrender(void);
extern void transpolybegin(int texnum, int glowtexnum, int fogtexnum, int transpolytype);
extern void transpolyend(void);
extern void transpolyparticle(vec3_t org, vec3_t right, vec3_t up, vec_t scale, unsigned short texnum, unsigned short transpolytype, int ir, int ig, int ib, float alphaf, float s1, float t1, float s2, float t2);

extern void wallpolyclear(void);
extern void wallpolyrender1(void);
extern void wallpolyrender2(void);

extern void skypolyclear(void);
extern void skypolyrender(void);
extern void skypolybegin(void);
extern void skypolyvert(float x, float y, float z);
extern void skypolyend(void);

#define MAX_TRANSPOLYS 65536
#define MAX_TRANSVERTS (MAX_TRANSPOLYS*4)
#define MAX_WALLPOLYS 65536
#define MAX_WALLVERTS (MAX_WALLPOLYS*3)
#define MAX_SKYPOLYS 2048
#define MAX_SKYVERTS (MAX_SKYPOLYS*4)

typedef struct
{
	vec_t s, t;
	byte r,g,b,a;
	vec3_t v;
}
transvert_t;

typedef struct
{
//	vec_t mindistance, maxdistance; // closest and farthest distance along v_forward
//	vec_t distance; // distance to center
//	vec3_t n; // normal
//	vec_t ndist; // distance from origin along that normal
	unsigned short texnum;
	unsigned short glowtexnum;
	unsigned short fogtexnum;
	unsigned short firstvert;
	unsigned short verts;
	unsigned short transpolytype;
}
transpoly_t;

// note: must match format of glpoly_t vertices due to a memcpy used in RSurf_DrawWall
typedef struct
{
	vec_t vert[VERTEXSIZE]; // xyz st uv
}
wallvert_t;

typedef struct
{
	byte r,g,b,a;
}
wallvertcolor_t;

typedef struct
{
	unsigned short texnum, lighttexnum, glowtexnum;
	unsigned short firstvert;
	unsigned short numverts;
	unsigned short lit; // doesn't need to be an unsigned short, but to keep the structure consistent...
}
wallpoly_t;

typedef struct
{
	float tex[2];
	float tex2[2];
	float v[4]; // 4th item is only for padding
}
skyvert_t;

typedef struct
{
	unsigned short firstvert;
	unsigned short verts;
}
skypoly_t;

extern transvert_t *transvert;
extern transpoly_t *transpoly;
extern int *transpolyindex;
extern int *transvertindex;
extern transpoly_t **transpolylist;
extern wallvert_t *wallvert;
extern wallvertcolor_t *wallvertcolor;
extern wallpoly_t *wallpoly;
extern skyvert_t *skyvert;
extern skypoly_t *skypoly;

extern int currenttranspoly;
extern int currenttransvert;
extern int currentwallpoly;
extern int currentwallvert;
extern int currentskypoly;
extern int currentskyvert;

#define transpolybegin(ttexnum, tglowtexnum, tfogtexnum, ttranspolytype)\
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transpoly[currenttranspoly].texnum = (unsigned short) (ttexnum);\
		transpoly[currenttranspoly].glowtexnum = (unsigned short) (tglowtexnum);\
		transpoly[currenttranspoly].fogtexnum = (unsigned short) (tfogtexnum);\
		transpoly[currenttranspoly].transpolytype = (unsigned short) (ttranspolytype);\
		transpoly[currenttranspoly].firstvert = currenttransvert;\
		transpoly[currenttranspoly].verts = 0;\
	}\
}

#define transpolyvert(vx,vy,vz,vs,vt,vr,vg,vb,va) \
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transvert[currenttransvert].s = (vs);\
		transvert[currenttransvert].t = (vt);\
		if (lighthalf)\
		{\
			transvert[currenttransvert].r = (byte) (bound(0, (int) (vr) >> 1, 255));\
			transvert[currenttransvert].g = (byte) (bound(0, (int) (vg) >> 1, 255));\
			transvert[currenttransvert].b = (byte) (bound(0, (int) (vb) >> 1, 255));\
		}\
		else\
		{\
			transvert[currenttransvert].r = (byte) (bound(0, (int) (vr), 255));\
			transvert[currenttransvert].g = (byte) (bound(0, (int) (vg), 255));\
			transvert[currenttransvert].b = (byte) (bound(0, (int) (vb), 255));\
		}\
		transvert[currenttransvert].a = (byte) (bound(0, (int) (va), 255));\
		transvert[currenttransvert].v[0] = (vx);\
		transvert[currenttransvert].v[1] = (vy);\
		transvert[currenttransvert].v[2] = (vz);\
		currenttransvert++;\
		transpoly[currenttranspoly].verts++;\
	}\
}

#define transpolyvertub(vx,vy,vz,vs,vt,vr,vg,vb,va) \
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transvert[currenttransvert].s = (vs);\
		transvert[currenttransvert].t = (vt);\
		if (lighthalf)\
		{\
			transvert[currenttransvert].r = (vr) >> 1;\
			transvert[currenttransvert].g = (vg) >> 1;\
			transvert[currenttransvert].b = (vb) >> 1;\
		}\
		else\
		{\
			transvert[currenttransvert].r = (vr);\
			transvert[currenttransvert].g = (vg);\
			transvert[currenttransvert].b = (vb);\
		}\
		transvert[currenttransvert].a = (va);\
		transvert[currenttransvert].v[0] = (vx);\
		transvert[currenttransvert].v[1] = (vy);\
		transvert[currenttransvert].v[2] = (vz);\
		currenttransvert++;\
		transpoly[currenttranspoly].verts++;\
	}\
}
