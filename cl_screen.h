
#ifndef CL_SCREEN_H
#define CL_SCREEN_H

// drawqueue stuff for use by client to feed 2D art to renderer
#define DRAWQUEUE_STRING 0
#define DRAWQUEUE_MESH 1

typedef struct drawqueue_s
{
	unsigned short size;
	qbyte command, flags;
	unsigned int color;
	float x, y, scalex, scaley;
}
drawqueue_t;

// a triangle mesh... embedded in the drawqueue
// each vertex is 4 floats (3 are used)
// each texcoord is 4 floats (3 are used)
// each color is 4 floats (4 are used)
typedef struct drawqueuemesh_s
{
	rtexture_t *texture;
	int numtriangles;
	int numvertices;
	int *element3i;
	float *vertex3f;
	float *texcoord2f;
	float *color4f;
}
drawqueuemesh_t;

#define DRAWFLAG_ADDITIVE 1

// clear the draw queue
void DrawQ_Clear(void);
// draw an image
void DrawQ_Pic(float x, float y, char *picname, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a text string
void DrawQ_String(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags);
// draw a filled rectangle
void DrawQ_Fill(float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags);
// draw a very fancy pic (per corner texcoord/color control), the order is tl, tr, bl, br
void DrawQ_SuperPic(float x, float y, char *picname, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags);
// draw a triangle mesh
void DrawQ_Mesh(drawqueuemesh_t *mesh, int flags);

void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);
void SHOWLMP_clear(void);

extern cvar_t scr_2dresolution;
extern cvar_t scr_screenshot_jpeg;

void CL_Screen_NewMap(void);
void CL_Screen_Init(void);
void CL_UpdateScreen(void);

#endif

