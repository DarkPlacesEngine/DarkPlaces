
#ifndef UI_H
#define UI_H
/*
// these defines and structures are for internal use only
// (ui_t is passed around by users of the system, but should not be altered)
#define MAX_UI_COUNT 16
#define MAX_UI_ITEMS 256

typedef struct
{
	char name[32];
	int flags;
	char *draw_picname;
	char *draw_string;
	int draw_x, draw_y;
	int click_x, click_y, click_x2, click_y2;
	void(*leftkey)(void *nativedata1, void *nativedata2, float data1, float data2);
	void(*rightkey)(void *nativedata1, void *nativedata2, float data1, float data2);
	void(*enterkey)(void *nativedata1, void *nativedata2, float data1, float data2);
	void(*mouseclick)(void *nativedata1, void *nativedata2, float data1, float data2, float xfrac, float yfrac);
	void *nativedata1, *nativedata2;
	float data1, data2;
}
ui_item_t;

typedef struct
{
	int item_count;
	int pad;
	ui_item_t items[MAX_UI_ITEMS];
}
ui_t;

// engine use:
// initializes the ui system
void ui_init(void);
// updates the mouse position, given an absolute loation (some input systems use this)
void ui_mouseupdate(float x, float y);
// updates the mouse position, by an offset from the previous location (some input systems use this)
void ui_mouseupdaterelative(float x, float y);
// left key update
void ui_leftkeyupdate(int pressed);
// right key update
void ui_rightkeyupdate(int pressed);
// up key update
void ui_upkeyupdate(int pressed);
// down key update
void ui_downkeyupdate(int pressed);
// mouse button update (note: 0 = left, 1 = right, 2 = middle, 3+ not supported yet)
void ui_mousebuttonupdate(int button, int pressed);
// perform input updates and check for clicks on items (note: calls callbacks)
void ui_update(void);
// draw all items of all panels
void ui_draw(void);

// intentionally public functions:
// creates a panel
ui_t *ui_create(void);
// frees a panel
void ui_free(ui_t *ui);
// empties a panel, removing all the items
void ui_clear(ui_t *ui);
// sets an item in a panel (adds or replaces the item)
void ui_item
(
	ui_t *ui, char *basename, int number,
	float x, float y, char *picname, char *string,
	float left, float top, float width, float height,
	void(*leftkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*rightkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*enterkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*mouseclick)(void *nativedata1, void *nativedata2, float data1, float data2, float xfrac, float yfrac),
	void *nativedata1, void *nativedata2, float data1, float data2
);
// removes an item from a panel
void ui_item_remove(ui_t *ui, char *basename, int number);
// checks if a panel is enabled
int ui_uiactive(ui_t *ui);
// enables/disables a panel on the screen
void ui_activate(ui_t *ui, int yes);*/

// AK: new passive ui (like the menu stuff)
#define UI_TEXT_DEFAULT_LENGTH 255
typedef void * ui_item_t;
typedef void * ui_itemlist_t;

void UI_Init(void);

void UI_Key(ui_itemlist_t, int key, int ascii);
void UI_Draw(ui_itemlist_t);

void UI_SetFocus(ui_itemlist_t, ui_item_t);
void UI_SetNeighbors(ui_item_t left, ui_item_t right, ui_item_t up, ui_item_t down);

// item stuff
ui_item_t UI_CreateButton(const char *caption, float x, float y, void(*action)(ui_item_t)); 
ui_item_t UI_CreateLabel(const char *caption, float x, float y);
ui_item_t UI_CreateText(const char *caption, float x, float y, const char *allowed, int maxlen, int scrolllen);
void UI_FreeItem(ui_item_t);

const char* UI_GetCaption(ui_item_t);
void UI_SetCaption(ui_item_t, const char *);

// itemlist stuff
ui_itemlist_t UI_CreateItemList(float x, float y);
void UI_FreeItemList(ui_itemlist_t);

void UI_AddItem(ui_itemlist_t, ui_item_t);

// AK: new callback system
#define UI_MAX_CALLBACK_COUNT 10

#define UI_SLOTUSED	1
typedef struct ui_callback_s
{
	unsigned int flag;
	void (*keydown) (int num, char ascii);
	void (*draw)	(void);
} ui_callback_t;

// functions which should be used
void UI_Callback_Init(void);
void UI_Callback_Reset(void);

void UI_Callback_SetupSlot(int slotnr, void(*keydownf)(int num, char ascii), void(*drawf)(void));
void UI_Callback_ResetSlot(int slotnr);
int  UI_Callback_GetFreeSlot(void);
int	 UI_Callback_IsSlotUsed(int slotnr);

void UI_Callback_Draw(void);
void UI_Callback_KeyDown(int num, char ascii);

#endif

