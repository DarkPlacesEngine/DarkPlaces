
#ifndef CL_SCREEN_H
#define CL_SCREEN_H

// drawqueue stuff for use by client to feed 2D art to renderer
#define DRAWQUEUE_PIC 0
#define DRAWQUEUE_STRING 1
#define DRAWQUEUE_MESH 2

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
// each texcoord pair is 2 floats
// each color is 4 floats
typedef struct drawqueuemesh_s
{
	rtexture_t *texture;
	int numindices;
	int numvertices;
	int *indices;
	float *vertices;
	float *texcoords;
	float *colors;
}
drawqueuemesh_t;

#define DRAWFLAG_ADDITIVE 1

// clear the draw queue
void DrawQ_Clear(void);
// draw an image
void DrawQ_Pic(float x, float y, char *picname, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a text string
void DrawQ_String(float x, float y, char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags);
// draw a filled rectangle
void DrawQ_Fill (float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags);
// draw a triangle mesh
void DrawQ_Mesh (drawqueuemesh_t *mesh, int flags);

void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);
void SHOWLMP_clear(void);

extern cvar_t scr_2dresolution;

void CL_Screen_NewMap(void);
void CL_Screen_Init(void);
void CL_UpdateScreen(void);

#endif

