
#include "quakedef.h"

cvar_t ui_showname = {0, "ui_showname", "0"};

#define ITEM_CLICKABLE 1
#define ITEM_DRAWABLE 2

#define UIKEY_LEFT 1
#define UIKEY_RIGHT 2
#define UIKEY_UP 3
#define UIKEY_DOWN 4
#define UIKEY_ENTER 5

#define UI_MOUSEBUTTONS 3

static int ui_alive, ui_active;
static float ui_mouse_x, ui_mouse_y;
static int ui_mousebutton[UI_MOUSEBUTTONS], ui_mouseclick;
static int ui_keyui, ui_keyitem;
static ui_item_t *ui_keyrealitem;

static ui_t *ui_list[MAX_UI_COUNT];

static void ui_start(void)
{
	ui_mouse_x = vid.conwidth * 0.5;
	ui_mouse_y = vid.conheight * 0.5;
	ui_alive = true;
}

static void ui_shutdown(void)
{
	ui_alive = false;
}

static void ui_newmap(void)
{
}

static mempool_t *uimempool;

void ui_init(void)
{
	uimempool = Mem_AllocPool("UI");

	Cvar_RegisterVariable(&ui_showname);
	R_RegisterModule("UI", ui_start, ui_shutdown, ui_newmap);
}

void ui_mouseupdate(float x, float y)
{
	if (ui_alive)
	{
		ui_mouse_x = bound(0, x, vid.conwidth);
		ui_mouse_y = bound(0, y, vid.conheight);
	}
}

void ui_mouseupdaterelative(float x, float y)
{
	if (ui_alive)
	{
		ui_mouse_x += x;
		ui_mouse_y += y;
		ui_mouse_x = bound(0, ui_mouse_x, vid.conwidth);
		ui_mouse_y = bound(0, ui_mouse_y, vid.conheight);
	}
}

ui_t *ui_create(void)
{
	ui_t *ui;
	ui = Mem_Alloc(uimempool, sizeof(*ui));
	if (ui == NULL)
		Sys_Error("ui_create: unable to allocate memory for new ui\n");
	memset(ui, 0, sizeof(*ui));
	return ui;
}

void ui_free(ui_t *ui)
{
	if (ui)
		Mem_Free(ui);
}

void ui_clear(ui_t *ui)
{
	ui->item_count = 0;
}

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
)
{
	int i;
	ui_item_t *it;
	char itemname[32];
	snprintf(itemname, sizeof(itemname), "%s%04d", basename, number);
	for (it = ui->items, i = 0;i < ui->item_count;it++, i++)
		if (it->name == NULL || !strncmp(itemname, it->name, 32))
			break;
	if (i == ui->item_count)
	{
		if (i == MAX_UI_ITEMS)
		{
			Con_Printf("ui_item: ran out of UI item slots\n");
			return;
 		}
		ui->item_count++;
	}
	memset(it, 0, sizeof(ui_item_t));
	strncpy(it->name, itemname, 32);
	it->flags = 0;
	if (picname || string)
	{
		it->flags |= ITEM_DRAWABLE;
		it->draw_picname = picname;
		it->draw_string = string;
		it->draw_x = x;
		it->draw_y = y;
	}
	if (leftkey || rightkey || enterkey || mouseclick)
	{
		it->flags |= ITEM_CLICKABLE;
		it->click_x = x + left;
		it->click_y = y + top;
		it->click_x2 = it->click_x + width;
		it->click_y2 = it->click_y + height;
		it->leftkey = leftkey;
		it->rightkey = rightkey;
		it->enterkey = enterkey;
		it->mouseclick = mouseclick;
		if (it->mouseclick == NULL)
			it->mouseclick = (void *)it->enterkey;
		if (it->leftkey == NULL)
			it->leftkey = it->enterkey;
		if (it->rightkey == NULL)
			it->rightkey = it->enterkey;
		it->nativedata1 = nativedata1;
		it->nativedata2 = nativedata2;
	}
}

void ui_item_remove(ui_t *ui, char *basename, int number)
{
	int i;
	ui_item_t *it;
	char itemname[32];
	snprintf(itemname, sizeof(itemname), "%s%04d", basename, number);
	for (it = ui->items, i = 0;i < ui->item_count;it++, i++)
		if (it->name && !strncmp(itemname, it->name, 32))
			break;
	if (i < ui->item_count)
		it->name[0] = 0;
}

ui_item_t *ui_hititem(float x, float y)
{
	int i, j;
	ui_item_t *it;
	ui_t *ui;
	for (j = 0;j < MAX_UI_COUNT;j++)
		if ((ui = ui_list[j]))
			for (it = ui->items, i = 0;i < ui->item_count;it++, i++)
				if (it->name[0] && (it->flags & ITEM_CLICKABLE))
					if (x >= it->click_x && y >= it->click_y && x < it->click_x2 && y < it->click_y2)
						return it;
	return NULL;
}

int ui_uiactive(ui_t *ui)
{
	int i;
	for (i = 0;i < MAX_UI_COUNT;i++)
		if (ui_list[i] == ui)
			return true;
	return false;
}

void ui_activate(ui_t *ui, int yes)
{
	int i;
	if (yes)
	{
		if (ui_uiactive(ui))
			return;

		for (i = 0;i < MAX_UI_COUNT;i++)
		{
			if (ui_list[i] == NULL)
			{
				ui_list[i] = ui;
				return;
			}
		}

		Con_Printf("ui_activate: ran out of active ui list items\n");
	}
	else
	{
		for (i = 0;i < MAX_UI_COUNT;i++)
		{
			if (ui_list[i] == ui)
			{
				ui_list[i] = NULL;
				return;
			}
		}
	}
}

int ui_isactive(void)
{
	int j;
	ui_t *ui;
	if (ui_alive)
	{
		for (j = 0;j < MAX_UI_COUNT;j++)
			if ((ui = ui_list[j]))
				if (ui->item_count)
					return true;
	}
	return false;
}

#define UI_QUEUE_SIZE 256
static qbyte ui_keyqueue[UI_QUEUE_SIZE];
static int ui_keyqueuepos = 0;

void ui_leftkeyupdate(int pressed)
{
	static int key = false;
	if (pressed && !key && ui_keyqueuepos < UI_QUEUE_SIZE)
		ui_keyqueue[ui_keyqueuepos++] = UIKEY_LEFT;
	key = pressed;
}

void ui_rightkeyupdate(int pressed)
{
	static int key = false;
	if (pressed && !key && ui_keyqueuepos < UI_QUEUE_SIZE)
		ui_keyqueue[ui_keyqueuepos++] = UIKEY_RIGHT;
	key = pressed;
}

void ui_upkeyupdate(int pressed)
{
	static int key = false;
	if (pressed && !key && ui_keyqueuepos < UI_QUEUE_SIZE)
		ui_keyqueue[ui_keyqueuepos++] = UIKEY_UP;
	key = pressed;
}

void ui_downkeyupdate(int pressed)
{
	static int key = false;
	if (pressed && !key && ui_keyqueuepos < UI_QUEUE_SIZE)
		ui_keyqueue[ui_keyqueuepos++] = UIKEY_DOWN;
	key = pressed;
}

void ui_mousebuttonupdate(int button, int pressed)
{
	if (button < 0 || button >= UI_MOUSEBUTTONS)
		return;
	if (button == 0 && ui_mousebutton[button] && !pressed)
		ui_mouseclick = true;
	ui_mousebutton[button] = pressed;
}

void ui_update(void)
{
	ui_item_t *startitem, *it;
	if (ui_alive)
	{
		ui_mouse_x = bound(0, ui_mouse_x, vid.conwidth);
		ui_mouse_y = bound(0, ui_mouse_y, vid.conheight);

		if ((ui_active = ui_isactive()))
		{
			// validate currently selected item
			if(ui_list[ui_keyui] == NULL)
			{
				while (ui_list[ui_keyui] == NULL)
					ui_keyui = (ui_keyui + 1) % MAX_UI_COUNT;
				ui_keyitem = 0;
			}
			ui_keyitem = bound(0, ui_keyitem, ui_list[ui_keyui]->item_count - 1);
			startitem = ui_keyrealitem = &ui_list[ui_keyui]->items[ui_keyitem];
			if ((ui_keyrealitem->flags & ITEM_CLICKABLE) == 0)
			{
				do
				{
					// FIXME: cycle through UIs as well as items in a UI
					ui_keyitem = (ui_keyitem - 1) % ui_list[ui_keyui]->item_count - 1;
					ui_keyrealitem = &ui_list[ui_keyui]->items[ui_keyitem];
				}
				while (ui_keyrealitem != startitem && (ui_keyrealitem->flags & ITEM_CLICKABLE) == 0);
			}

			if (ui_keyqueuepos)
			{
				int i;
				for (i = 0;i < ui_keyqueuepos;i++)
				{
					startitem = ui_keyrealitem;
					switch(ui_keyqueue[i])
					{
					case UIKEY_UP:
						do
						{
							ui_keyitem--;
							if (ui_keyitem < 0)
							{
								do
									ui_keyui = (ui_keyui - 1) % MAX_UI_COUNT;
								while(ui_list[ui_keyui] == NULL);
								ui_keyitem = ui_list[ui_keyui]->item_count - 1;
							}
							ui_keyrealitem = &ui_list[ui_keyui]->items[ui_keyitem];
						}
						while (ui_keyrealitem != startitem && (ui_keyrealitem->flags & ITEM_CLICKABLE) == 0);
						break;
					case UIKEY_DOWN:
						do
						{
							ui_keyitem++;
							if (ui_keyitem >= ui_list[ui_keyui]->item_count)
							{
								do
									ui_keyui = (ui_keyui + 1) % MAX_UI_COUNT;
								while(ui_list[ui_keyui] == NULL);
								ui_keyitem = 0;
							}
							ui_keyrealitem = &ui_list[ui_keyui]->items[ui_keyitem];
						}
						while (ui_keyrealitem != startitem && (ui_keyrealitem->flags & ITEM_CLICKABLE) == 0);
						break;
					case UIKEY_LEFT:
						if (ui_keyrealitem->leftkey)
							ui_keyrealitem->leftkey(ui_keyrealitem->nativedata1, ui_keyrealitem->nativedata2, ui_keyrealitem->data1, ui_keyrealitem->data2);
						break;
					case UIKEY_RIGHT:
						if (ui_keyrealitem->rightkey)
							ui_keyrealitem->rightkey(ui_keyrealitem->nativedata1, ui_keyrealitem->nativedata2, ui_keyrealitem->data1, ui_keyrealitem->data2);
						break;
					case UIKEY_ENTER:
						if (ui_keyrealitem->enterkey)
							ui_keyrealitem->enterkey(ui_keyrealitem->nativedata1, ui_keyrealitem->nativedata2, ui_keyrealitem->data1, ui_keyrealitem->data2);
						break;
					}
				}
			}
			ui_keyqueuepos = 0;

			if (ui_mouseclick && (it = ui_hititem(ui_mouse_x, ui_mouse_y)) && it->mouseclick)
				it->mouseclick(it->nativedata1, it->nativedata2, it->data1, it->data2, ui_mouse_x - it->click_x, ui_mouse_y - it->click_y);
    	}
	}
	ui_mouseclick = false;
}

void ui_draw(void)
{
	int i, j;
	ui_item_t *it;
	ui_t *ui;
	if (ui_alive && ui_active)
	{
		for (j = 0;j < MAX_UI_COUNT;j++)
			if ((ui = ui_list[j]))
				if (ui->item_count)
					for (i = 0, it = ui->items;i < ui->item_count;i++, it++)
						if (it->flags & ITEM_DRAWABLE)
						{
							if (it->draw_picname)
								DrawQ_Pic(it->draw_x, it->draw_y, it->draw_picname, 0, 0, 1, 1, 1, 1, 0);
							if (it->draw_string)
								DrawQ_String(it->draw_x, it->draw_y, it->draw_string, 0, 8, 8, 1, 1, 1, 1, 0);
						}

		if ((it = ui_hititem(ui_mouse_x, ui_mouse_y)))
		{
			if (it->draw_picname)
				DrawQ_Pic(it->draw_x, it->draw_y, it->draw_picname, 0, 0, 1, 1, 1, 1, DRAWFLAG_ADDITIVE);
			if (it->draw_string)
				DrawQ_String(it->draw_x, it->draw_y, it->draw_string, 0, 8, 8, 1, 1, 1, 1, DRAWFLAG_ADDITIVE);
			if (ui_showname.integer)
				DrawQ_String(ui_mouse_x, ui_mouse_y + 16, it->name, 0, 8, 8, 1, 1, 1, 1, 0);
    	}

		it = ui_keyrealitem;
		if (it->draw_picname)
			DrawQ_Pic(it->draw_x, it->draw_y, it->draw_picname, 0, 0, 1, 1, 1, 1, DRAWFLAG_ADDITIVE);
		if (it->draw_string)
			DrawQ_String(it->draw_x, it->draw_y, it->draw_string, 0, 8, 8, 1, 1, 1, 1, DRAWFLAG_ADDITIVE);

		DrawQ_Pic(ui_mouse_x, ui_mouse_y, "ui/mousepointer.tga", 0, 0, 1, 1, 1, 1, 0);
	}
}

