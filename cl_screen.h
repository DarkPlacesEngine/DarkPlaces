
#ifndef CL_SCREEN_H
#define CL_SCREEN_H

// drawqueue stuff for use by client to feed 2D art to renderer
#define MAX_DRAWQUEUE 262144

#define DRAWQUEUE_PIC 0
#define DRAWQUEUE_STRING 1

typedef struct drawqueue_s
{
	unsigned short size;
	qbyte command, flags;
	unsigned int color;
	float x, y, scalex, scaley;
}
drawqueue_t;

#define DRAWFLAG_ADDITIVE 1

void DrawQ_Clear(void);
void DrawQ_Pic(float x, float y, char *picname, float width, float height, float red, float green, float blue, float alpha, int flags);
void DrawQ_String(float x, float y, char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags);
void DrawQ_Fill (float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags);
// only used for player config menu
void DrawQ_PicTranslate (int x, int y, char *picname, qbyte *translation);

void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);
void SHOWLMP_clear(void);

extern cvar_t scr_2dresolution;

void CL_Screen_NewMap(void);
void CL_Screen_Init(void);
void CL_UpdateScreen(void);

#endif

