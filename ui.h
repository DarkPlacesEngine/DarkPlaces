
#ifndef UI_H
#define UI_H

// AK: new passive ui (like the menu stuff)
/* some ideas:
1. two different structs (one for the ui core code and one for the rest)
2. each item has a size field
*/

#define UI_EVENT_QUEUE_SIZE 32

typedef enum ui_control_type_e { UI_BUTTON, UI_LABEL } ui_control_type;

typedef struct ui_message_s			ui_message_t;
typedef struct ui_item_s			*ui_item_t;
typedef struct ui_itemlist_s		*ui_itemlist_t;
typedef struct ui_message_queue_s	ui_message_queue_t;

struct ui_item_s
{
	unsigned int size;

	ui_control_type	type;

	const char *name; // used for debugging purposes and to identify an object

//private:
	// used to build the item list
	struct ui_item_s *prev, *next; // items are allowed to be freed everywhere

	// called for system events (true means message processed)
	int	(*eventhandler)(ui_itemlist_t list, ui_item_t self, ui_message_t *in, ui_message_queue_t *out);

	// z-order (the higher, the later it is drawn)
	unsigned int zorder;

	// called to draw the object
	void (*draw)(ui_itemlist_t list, struct ui_item_s * self);

};

struct ui_message_s;

struct ui_itemlist_s
{
	float org_x, org_y;

	ui_item_t selected;

	void (*eventhandler)(struct ui_itemlist_s * list, struct ui_message_s *msg);

// private:
	ui_item_t	list;
};

// this is structure contains *all* possible messages
enum ui_message_type_e { UI_EVENT_FRAME, UI_EVENT_KEY, UI_EVENT_MOUSE, UI_BUTTON_PRESSED };

struct ui_ev_key_s
{
	int key, ascii;
};

// in_mouse_x and in_mouse_y can be also used...
struct ui_ev_mouse_s
{
	float x, y;
};

union ui_message_data_u
{
	unsigned char reserved;
	struct ui_ev_key_s key;
	struct ui_ev_mouse_s mouse;
};

struct ui_message_s
{
	// empty for input messages, but contains a valid item for all other events
	ui_item_t				target;

	// used to determine which data struct was used
	enum ui_message_type_e	type;

	union ui_message_data_u data;
};

struct ui_message_queue_s
{
	unsigned int used;
	ui_message_t queue[UI_EVENT_QUEUE_SIZE];
};

void UI_Init(void);

#define UI_MOUSEEVENT	1
#define UI_KEYEVENT		2
#define UI_FRAME		4
void UI_Draw(ui_itemlist_t list);

void UI_Mouse(ui_itemlist_t list, float x, float y);
void UI_Key(ui_itemlist_t list, int key, int ascii);

// item stuff
#define UI_ITEM(item)	((ui_item_t*)item)

ui_item_t UI_CloneItem(ui_item_t);

ui_item_t UI_FindItemByName(ui_itemlist_t, const char *);

void UI_FreeItem(ui_itemlist_t, ui_item_t);
void UI_FreeItemByName(ui_itemlist_t, const char *);

// itemlist stuff
ui_itemlist_t UI_CreateItemList();
ui_itemlist_t UI_CloneItemList(ui_itemlist_t);
void UI_FreeItemList(ui_itemlist_t);

void UI_AddItem(ui_itemlist_t list, ui_item_t item);

// controls
#define UI_TEXT_DEFAULT_LENGTH 255

typedef struct ui_button_s	*ui_button_t;
typedef struct ui_label_s	*ui_label_t;
typedef struct ui_text_s	*ui_text_t;

struct ui_label_s
{
	struct ui_item_s item;

	const char *text;
	float x, y;
	float r, g, b, a, f;
};

struct ui_button
{
	struct ui_item_s item;

	const char *caption;
};

ui_item_t UI_CreateButton(void);
ui_item_t UI_CreateLabel(void);
ui_item_t UI_CreateText(void);

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

