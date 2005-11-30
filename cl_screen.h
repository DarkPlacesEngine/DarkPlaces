
#ifndef CL_SCREEN_H
#define CL_SCREEN_H

// drawqueue stuff for use by client to feed 2D art to renderer
#define DRAWQUEUE_STRING 0
#define DRAWQUEUE_MESH 1
#define DRAWQUEUE_SETCLIP 2
#define DRAWQUEUE_RESETCLIP 3

typedef struct drawqueue_s
{
	unsigned short size;
	unsigned char command, flags;
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
	int num_triangles;
	int num_vertices;
	int *data_element3i;
	float *data_vertex3f;
	float *data_texcoord2f;
	float *data_color4f;
}
drawqueuemesh_t;

enum drawqueue_drawflag_e {
DRAWFLAG_NORMAL,
DRAWFLAG_ADDITIVE,
DRAWFLAG_MODULATE,
DRAWFLAG_2XMODULATE,
DRAWFLAG_NUMFLAGS
};

// shared color tag printing constants
#define STRING_COLOR_TAG			'^'
#define STRING_COLOR_DEFAULT		7
#define STRING_COLOR_DEFAULT_STR	"^7"

// clear the draw queue
void DrawQ_Clear(void);
// draw an image
void DrawQ_Pic(float x, float y, const char *picname, float width, float height, float red, float green, float blue, float alpha, int flags);
// draw a text string
void DrawQ_String(float x, float y, const char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags);
// draw a text string that supports color tags (colorindex can either be NULL, -1 to make it choose the default color or valid index to start with)
void DrawQ_ColoredString( float x, float y, const char *text, int maxlen, float scalex, float scaley, float basered, float basegreen, float baseblue, float basealpha, int flags, int *outcolor );
// draw a filled rectangle
void DrawQ_Fill(float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags);
// draw a very fancy pic (per corner texcoord/color control), the order is tl, tr, bl, br
void DrawQ_SuperPic(float x, float y, const char *picname, float width, float height, float s1, float t1, float r1, float g1, float b1, float a1, float s2, float t2, float r2, float g2, float b2, float a2, float s3, float t3, float r3, float g3, float b3, float a3, float s4, float t4, float r4, float g4, float b4, float a4, int flags);
// draw a triangle mesh
void DrawQ_Mesh(drawqueuemesh_t *mesh, int flags);
// set the clipping area
void DrawQ_SetClipArea(float x, float y, float width, float height);
// reset the clipping area
void DrawQ_ResetClipArea(void);

void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);
void SHOWLMP_clear(void);

extern cvar_t vid_conwidth;
extern cvar_t vid_conheight;
extern cvar_t vid_pixelheight;
extern cvar_t scr_screenshot_jpeg;
extern cvar_t scr_screenshot_jpeg_quality;
extern cvar_t scr_screenshot_gamma;
extern cvar_t scr_screenshot_name;

void CL_Screen_NewMap(void);
void CL_Screen_Init(void);
void CL_UpdateScreen(void);

#endif

