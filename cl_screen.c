
#include "quakedef.h"

static byte menuplyr_pixels[4096];

void CL_Screen_Init(void)
{
	qpic_t *dat;

	// HACK HACK HACK
	// load the image data for the player image in the config menu
	dat = (qpic_t *)COM_LoadFile ("gfx/menuplyr.lmp", false);
	if (!dat)
		Sys_Error("unable to load gfx/menuplyr.lmp");
	SwapPic (dat);

	if (dat->width*dat->height <= 4096)
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);
	else
		Con_Printf("gfx/menuplyr.lmp larger than 4k buffer");
	free(dat);
}

void DrawQ_Clear(void)
{
	r_refdef.drawqueuesize = 0;
}

void DrawQ_Pic(float x, float y, char *picname, float width, float height, float red, float green, float blue, float alpha, int flags)
{
	int size;
	drawqueue_t *dq;
	if (alpha < (1.0f / 255.0f))
		return;
	size = sizeof(*dq) + ((strlen(picname) + 1 + 3) & ~3);
	if (r_refdef.drawqueuesize + size > MAX_DRAWQUEUE)
		return;
	red = bound(0, red, 1);
	green = bound(0, green, 1);
	blue = bound(0, blue, 1);
	alpha = bound(0, alpha, 1);
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_PIC;
	dq->flags = flags;
	dq->color = ((unsigned int) (red * 255.0f) << 24) | ((unsigned int) (green * 255.0f) << 16) | ((unsigned int) (blue * 255.0f) << 8) | ((unsigned int) (alpha * 255.0f));
	dq->x = x;
	dq->y = y;
	// if these are not zero, they override the pic's size
	dq->scalex = width;
	dq->scaley = height;
	strcpy((char *)(dq + 1), picname);
	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_String(float x, float y, char *string, int maxlen, float scalex, float scaley, float red, float green, float blue, float alpha, int flags)
{
	int size, len;
	drawqueue_t *dq;
	char *out;
	if (alpha < (1.0f / 255.0f))
		return;
	if (maxlen < 1)
		len = strlen(string);
	else
		for (len = 0;len < maxlen && string[len];len++);
	for (;len > 0 && string[0] == ' ';string++, x += scalex, len--);
	for (;len > 0 && string[len - 1] == ' ';len--);
	if (len < 1)
		return;
	if (x >= vid.conwidth || y >= vid.conheight || x < (-scalex * maxlen) || y < (-scaley))
		return;
	size = sizeof(*dq) + ((len + 1 + 3) & ~3);
	if (r_refdef.drawqueuesize + size > MAX_DRAWQUEUE)
		return;
	red = bound(0, red, 1);
	green = bound(0, green, 1);
	blue = bound(0, blue, 1);
	alpha = bound(0, alpha, 1);
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_STRING;
	dq->flags = flags;
	dq->color = ((unsigned int) (red * 255.0f) << 24) | ((unsigned int) (green * 255.0f) << 16) | ((unsigned int) (blue * 255.0f) << 8) | ((unsigned int) (alpha * 255.0f));
	dq->x = x;
	dq->y = y;
	dq->scalex = scalex;
	dq->scaley = scaley;
	out = (char *)(dq + 1);
	memcpy(out, string, len);
	out[len] = 0;
	r_refdef.drawqueuesize += dq->size;
}

void DrawQ_Fill (float x, float y, float w, float h, float red, float green, float blue, float alpha, int flags)
{
	int size;
	drawqueue_t *dq;
	if (alpha < (1.0f / 255.0f))
		return;
	size = sizeof(*dq) + 4;
	if (r_refdef.drawqueuesize + size > MAX_DRAWQUEUE)
		return;
	red = bound(0, red, 1);
	green = bound(0, green, 1);
	blue = bound(0, blue, 1);
	alpha = bound(0, alpha, 1);
	dq = (void *)(r_refdef.drawqueue + r_refdef.drawqueuesize);
	dq->size = size;
	dq->command = DRAWQUEUE_PIC;
	dq->flags = flags;
	dq->color = ((unsigned int) (red * 255.0f) << 24) | ((unsigned int) (green * 255.0f) << 16) | ((unsigned int) (blue * 255.0f) << 8) | ((unsigned int) (alpha * 255.0f));
	dq->x = x;
	dq->y = y;
	dq->scalex = w;
	dq->scaley = h;
	// empty pic name
	*((char *)(dq + 1)) = 0;
	r_refdef.drawqueuesize += dq->size;
}

//only used for the player color selection menu
void DrawQ_PicTranslate (int x, int y, char *picname, byte *translation)
{
	int i, c;
	unsigned int trans[4096];
	cachepic_t *pic;

	pic = Draw_CachePic(picname);
	if (pic == NULL)
		return;

	c = pic->width * pic->height;
	if (c > 4096)
	{
		Con_Printf("DrawQ_PicTranslate: image larger than 4k buffer\n");
		return;
	}

	for (i = 0;i < c;i++)
		trans[i] = d_8to24table[translation[menuplyr_pixels[i]]];

	// FIXME: this is renderer stuff?
	R_UpdateTexture (pic->tex, (byte *)trans);

	DrawQ_Pic(x, y, picname, 0, 0, 1, 1, 1, 1, 0);
}

void V_CalcRefdef (void);
void CL_UpdateScreen(void)
{
	DrawQ_Clear();

	SHOWLMP_drawall();

	V_UpdateBlends();
	V_CalcRefdef ();

	SCR_UpdateScreen();
}

void CL_Screen_NewMap(void)
{
	SHOWLMP_clear();
}

//=============================================================================

// LordHavoc: SHOWLMP stuff
#define SHOWLMP_MAXLABELS 256
typedef struct showlmp_s
{
	qboolean	isactive;
	float		x;
	float		y;
	char		label[32];
	char		pic[128];
}
showlmp_t;

showlmp_t showlmp[SHOWLMP_MAXLABELS];

void SHOWLMP_decodehide(void)
{
	int i;
	byte *lmplabel;
	lmplabel = MSG_ReadString();
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive && strcmp(showlmp[i].label, lmplabel) == 0)
		{
			showlmp[i].isactive = false;
			return;
		}
}

void SHOWLMP_decodeshow(void)
{
	int i, k;
	byte lmplabel[256], picname[256];
	float x, y;
	strcpy(lmplabel,MSG_ReadString());
	strcpy(picname, MSG_ReadString());
	if (gamemode == GAME_NEHAHRA) // LordHavoc: nasty old legacy junk
	{
		x = MSG_ReadByte();
		y = MSG_ReadByte();
	}
	else
	{
		x = MSG_ReadShort();
		y = MSG_ReadShort();
	}
	k = -1;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		if (showlmp[i].isactive)
		{
			if (strcmp(showlmp[i].label, lmplabel) == 0)
			{
				k = i;
				break; // drop out to replace it
			}
		}
		else if (k < 0) // find first empty one to replace
			k = i;
	if (k < 0)
		return; // none found to replace
	// change existing one
	showlmp[k].isactive = true;
	strcpy(showlmp[k].label, lmplabel);
	strcpy(showlmp[k].pic, picname);
	showlmp[k].x = x;
	showlmp[k].y = y;
}

void SHOWLMP_drawall(void)
{
	int i;
	if (cl.worldmodel)
		for (i = 0;i < SHOWLMP_MAXLABELS;i++)
			if (showlmp[i].isactive)
				DrawQ_Pic(showlmp[i].x, showlmp[i].y, showlmp[i].pic, 0, 0, 1, 1, 1, 1, 0);
}

void SHOWLMP_clear(void)
{
	int i;
	for (i = 0;i < SHOWLMP_MAXLABELS;i++)
		showlmp[i].isactive = false;
}
