
#define TPOLYTYPE_ALPHA 0
#define TPOLYTYPE_ADD 1

extern void transpolyclear();
extern void transpolyrender();
extern void transpolybegin(int texnum, int glowtexnum, int fogtexnum, int transpolytype);
extern void transpolyend();

extern void wallpolyclear();
extern void wallpolyrender();

extern void skypolyclear();
extern void skypolyrender();
extern void skypolybegin();
extern void skypolyvert(float x, float y, float z);
extern void skypolyend();

#define MAX_TRANSPOLYS 8192
#define MAX_TRANSVERTS (MAX_TRANSPOLYS*4)
#define MAX_WALLPOLYS 16384
#define MAX_WALLVERTS (MAX_WALLPOLYS*4)
#define MAX_SKYPOLYS 2048
#define MAX_SKYVERTS (MAX_SKYPOLYS*4)

typedef struct
{
	vec_t s, t;
	byte r,g,b,a;
	vec3_t v;
} transvert_t;

typedef struct
{
	vec_t mindistance, maxdistance; // closest and farthest distance along v_forward
	vec_t distance; // distance to center
//	vec3_t n; // normal
//	vec_t ndist; // distance from origin along that normal
	unsigned short texnum;
	unsigned short glowtexnum;
	unsigned short fogtexnum;
	unsigned short firstvert;
	unsigned short verts;
	unsigned short transpolytype;
} transpoly_t;

typedef struct
{
	vec3_t vert;
	vec_t s, t, u, v;
} wallvert_t;

typedef struct
{
	unsigned short texnum, lighttexnum, glowtexnum;
	unsigned short firstvert;
	unsigned short verts;
} wallpoly_t;

typedef struct
{
	vec3_t v;
} skyvert_t;

typedef struct
{
	unsigned short firstvert;
	unsigned short verts;
} skypoly_t;

extern transvert_t *transvert;
extern transpoly_t *transpoly;
extern unsigned short *transpolyindex;
extern wallvert_t *wallvert;
extern wallpoly_t *wallpoly;
extern skyvert_t *skyvert;
extern skypoly_t *skypoly;

extern unsigned short currenttranspoly;
extern unsigned short currenttransvert;
extern unsigned short currentwallpoly;
extern unsigned short currentwallvert;
extern unsigned short currentskypoly;
extern unsigned short currentskyvert;

#define transpolyvert(vx,vy,vz,vs,vt,vr,vg,vb,va) \
{\
	if (currenttranspoly < MAX_TRANSPOLYS && currenttransvert < MAX_TRANSVERTS)\
	{\
		transvert[currenttransvert].s = (vs);\
		transvert[currenttransvert].t = (vt);\
		transvert[currenttransvert].r = (byte) (bound(0, (int) (vr), 255));\
		transvert[currenttransvert].g = (byte) (bound(0, (int) (vg), 255));\
		transvert[currenttransvert].b = (byte) (bound(0, (int) (vb), 255));\
		transvert[currenttransvert].a = (byte) (bound(0, (int) (va), 255));\
		transvert[currenttransvert].v[0] = (vx);\
		transvert[currenttransvert].v[1] = (vy);\
		transvert[currenttransvert].v[2] = (vz);\
		currenttransvert++;\
		transpoly[currenttranspoly].verts++;\
	}\
}
