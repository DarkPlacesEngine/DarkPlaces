
#define MAX_UI_COUNT 16
#define MAX_UI_ITEMS 256

typedef struct
{
	char name[32];
	int flags;
	qpic_t *draw_pic;
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

void ui_init(void);
void ui_mouseupdate(float x, float y);
void ui_mouseupdaterelative(float x, float y);
ui_t *ui_create(void);
void ui_free(ui_t *ui);
void ui_clear(ui_t *ui);
void ui_item
(
	ui_t *ui, char *basename, int number,
	float x, float y, qpic_t *pic,
	float left, float top, float width, float height,
	void(*leftkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*rightkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*enterkey)(void *nativedata1, void *nativedata2, float data1, float data2),
	void(*mouseclick)(void *nativedata1, void *nativedata2, float data1, float data2, float xfrac, float yfrac),
	void *nativedata1, void *nativedata2, float data1, float data2
);
void ui_item_remove(ui_t *ui, char *basename, int number);
int ui_uiactive(ui_t *ui);
void ui_activate(ui_t *ui, int yes);
void ui_leftkeyupdate(int pressed);
void ui_rightkeyupdate(int pressed);
void ui_upkeyupdate(int pressed);
void ui_downkeyupdate(int pressed);
void ui_mousebuttonupdate(int button, int pressed);
void ui_update(void);
void ui_draw(void);
