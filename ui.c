
#include "quakedef.h"

// here is the real ui drawing engine

/*
#define FRAME_THICKNESS	2
#define FRAME_COLOR1	0.2, 0.2, 0.5, 0, 0
#define FRAME_COLOR2	0, 0, 0, 0.6, 0
#define TEXT_FONTSIZE_X	10
#define TEXT_FONTSIZE_Y 10

static void UIG_DrawFrame(float x, float y, float w, float h)
{
	// bottom
	DrawQ_Fill(x - FRAME_THICKNESS, y - FRAME_THICKNESS, w + 2 * FRAME_THICKNESS, FRAME_THICKNESS, FRAME_COLOR1);
	// top
	DrawQ_Fill(x - FRAME_THICKNESS, y + h, w + 2 * FRAME_THICKNESS, FRAME_THICKNESS, FRAME_COLOR1);
	// left
	DrawQ_Fill(x - FRAME_THICKNESS, y, FRAME_THICKNESS, h, FRAME_COLOR1);
	// right
	DrawQ_Fill(x + w, y, FRAME_THICKNESS, h, FRAME_COLOR1);
	// area
	DrawQ_Fill(x, y, w, h, FRAME_COLOR2);
}

static void UIG_DrawText(const char *text, float x, float y, float w, float h, float r, float g, float b, float a, float f)
{
	if(w != 0 && h != 0)
		DrawQ_SetClipArea(x, y, w, h);
	DrawQ_String(x, y, text, 0, TEXT_FONTSIZE_X, TEXT_FONTSIZE_Y, r, g, b, a, f);
	if(w != 0 && h != 0)
		DrawQ_ResetClipArea();
}

#define UIG_DrawPicture		DrawQ_Pic
#define UIG_Fill			DrawQ_Fill

static void UIG_DrawCursor(float x, float y, float r, float g, float b, float a, float f)
{
	DrawQ_Fill(x,y,1, TEXT_FONTSIZE_Y, r, g, b, a, f);
}
*/

//#define UI_MEM_SIZE (1 << 10) << 9 // 512 KByte
#define UI_MEM_SIZE 1

void UI_Init(void)
{
}

#define UI_Alloc(size)	Mem_Alloc(cl_mempool, size)
#define UI_Free(ptr)	Mem_Free(ptr)

void UI_Event(ui_itemlist_t list, ui_message_t *in)
{
	ui_message_queue_t out;
	ui_item_t  item;
	int processed = true;

	if(list->list)
		for(item = list->list; item != 0 && !processed; item = item->next)
		{
			unsigned int i;

			processed = item->eventhandler(list, item, in, &out);

			// process posted messages
			for(i = 0; i < out.used; i++)
				list->eventhandler(list, &out.queue[i]);

			if(in->type == UI_EVENT_FRAME)
				processed = false;
		}

	if(!processed)
		list->eventhandler(list, in);
}

void UI_Draw(ui_itemlist_t list)
{
	// firstly we create the frame event here
	ui_message_t msg;
	ui_item_t item;

	msg.type = UI_EVENT_FRAME;

	UI_Event(list, &msg);

	// now draw everything
	if(list->list)
	{
		unsigned int depth = 0, nextdepth = ~0;

		while(depth != nextdepth)
		{
			for(item = list->list; item != 0; item = item->next)
			{
				if(item->zorder == depth)
					item->draw(list, item);
				if(item->zorder > depth && item->zorder < nextdepth)
					nextdepth = item->zorder;
			}
			depth = nextdepth;
			nextdepth = ~0;
		}
	}
}

void UI_Mouse(ui_itemlist_t list, float x, float y)
{
	ui_message_t msg;

	msg.type = UI_EVENT_MOUSE;

	msg.data.mouse.x = x;
	msg.data.mouse.y = y;

	UI_Event(list, &msg);
}

void UI_Key(ui_itemlist_t list, int key, int ascii)
{
	ui_message_t msg;

	msg.type = UI_EVENT_KEY;

	msg.data.key.key = key;
	msg.data.key.ascii = ascii;

	UI_Event(list, &msg);
}


// item stuff
ui_item_t UI_CloneItem(ui_item_t item)
{
	ui_item_t clone;
	clone = (ui_item_t)UI_Alloc(item->size);
	memcpy(clone, item, item->size);

	return clone;
}

ui_item_t UI_FindItemByName(ui_itemlist_t list, const char *name)
{
	ui_item_t item, found = 0;

	if(list->list)
		for(item = list->list; item != 0; item = item->next)
			if(!strcmp(name, item->name))
			{
				found = item;
				break;
			}

	return found;
}

void UI_FreeItem(ui_itemlist_t list, ui_item_t item)
{
	if(!item->prev)
	{
		// this is the first item
		list->list = item->next;
	}

	item->prev->next = item->next;
	item->next->prev = item->prev;

	UI_Free(item);
}

void UI_FreeItemByName(ui_itemlist_t list, const char *name)
{
	ui_item_t item;

	item = UI_FindItemByName(list, name);
	if(item)
		UI_Free(item);
}


// itemlist stuff
ui_itemlist_t UI_CreateItemList(void)
{
	return (ui_itemlist_t)UI_Alloc(sizeof(ui_itemlist_t));
}

ui_itemlist_t UI_CloneItemList(ui_itemlist_t list)
{
	ui_itemlist_t clone;
	ui_item_t	  item;

	clone = UI_CreateItemList();

	if(list->list)
		for(item = list->list; item != 0; item = item->next)
			UI_AddItem(clone, UI_CloneItem(item));

	return clone;
}


void UI_FreeItemList(ui_itemlist_t list)
{
	UI_Free((void*)list);
}

void UI_AddItem(ui_itemlist_t list, ui_item_t item)
{
	item->prev = 0;
	item->next = list->list;
	list->list->prev = item;
	list->list = item;
}

// controls
ui_item_t UI_CreateButton(void)
{
	return NULL;
}

ui_item_t UI_CreateLabel(void)
{
	return NULL;
}

ui_item_t UI_CreateText(void)
{
	return NULL;
}
// AK: callback system stuff
static ui_callback_t ui_callback_list[UI_MAX_CALLBACK_COUNT];

void UI_Callback_Init(void)
{
	memset(ui_callback_list, 0, sizeof(ui_callback_list));
}

int  UI_Callback_GetFreeSlot(void)
{
	int i;
	for(i = 0; ui_callback_list[i].flag & UI_SLOTUSED && i < UI_MAX_CALLBACK_COUNT; i++);

	if(i == UI_MAX_CALLBACK_COUNT)
		return -1;
	else
		return i;
}

int UI_Callback_IsSlotUsed(int slotnr)
{
	if(slotnr < 0 || slotnr >= UI_MAX_CALLBACK_COUNT)
		return false;
	return (ui_callback_list[slotnr].flag & UI_SLOTUSED);
}

void UI_Callback_SetupSlot(int slotnr, void(*keydownf)(int num, char ascii), void(*drawf)(void))
{
	ui_callback_list[slotnr].flag = UI_SLOTUSED;
	ui_callback_list[slotnr].draw = drawf;
	ui_callback_list[slotnr].keydown = keydownf;
}

void UI_Callback_ResetSlot(int slotnr)
{
	ui_callback_list[slotnr].flag = 0;
}

void UI_Callback_Draw(void)
{
	int i;
	for(i = 0; i < UI_MAX_CALLBACK_COUNT; i++)
		if(ui_callback_list[i].flag & UI_SLOTUSED && ui_callback_list[i].draw)
			ui_callback_list[i].draw();
}

void UI_Callback_KeyDown(int num, char ascii)
{
	if(ui_callback_list[key_dest - 3].flag & UI_SLOTUSED && ui_callback_list[key_dest - 3].keydown)
		ui_callback_list[key_dest - 3].keydown(num, ascii);
}
