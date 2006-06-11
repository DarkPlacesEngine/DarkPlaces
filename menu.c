/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"
#include "cdaudio.h"
#include "image.h"
#include "progsvm.h"

#include "mprogdefs.h"

#define TYPE_DEMO 1
#define TYPE_GAME 2
#define TYPE_BOTH 3

static int NehGameType;

enum m_state_e m_state;
char m_return_reason[32];

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Transfusion_Episode_f (void);
			void M_Menu_Transfusion_Skill_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
	void M_Menu_Options_f (void);
	void M_Menu_Options_Effects_f (void);
	void M_Menu_Options_Graphics_f (void);
	void M_Menu_Options_ColorControl_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Reset_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Credits_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_ServerList_f (void);

static void M_Main_Draw (void);
	static void M_SinglePlayer_Draw (void);
		static void M_Transfusion_Episode_Draw (void);
			static void M_Transfusion_Skill_Draw (void);
		static void M_Load_Draw (void);
		static void M_Save_Draw (void);
	static void M_MultiPlayer_Draw (void);
		static void M_Setup_Draw (void);
	static void M_Options_Draw (void);
	static void M_Options_Effects_Draw (void);
	static void M_Options_Graphics_Draw (void);
	static void M_Options_ColorControl_Draw (void);
		static void M_Keys_Draw (void);
		static void M_Reset_Draw (void);
		static void M_Video_Draw (void);
	static void M_Help_Draw (void);
	static void M_Credits_Draw (void);
	static void M_Quit_Draw (void);
static void M_LanConfig_Draw (void);
static void M_GameOptions_Draw (void);
static void M_ServerList_Draw (void);

static void M_Main_Key (int key, char ascii);
	static void M_SinglePlayer_Key (int key, char ascii);
		static void M_Transfusion_Episode_Key (int key, char ascii);
			static void M_Transfusion_Skill_Key (int key, char ascii);
		static void M_Load_Key (int key, char ascii);
		static void M_Save_Key (int key, char ascii);
	static void M_MultiPlayer_Key (int key, char ascii);
		static void M_Setup_Key (int key, char ascii);
	static void M_Options_Key (int key, char ascii);
	static void M_Options_Effects_Key (int key, char ascii);
	static void M_Options_Graphics_Key (int key, char ascii);
	static void M_Options_ColorControl_Key (int key, char ascii);
		static void M_Keys_Key (int key, char ascii);
		static void M_Reset_Key (int key, char ascii);
		static void M_Video_Key (int key, char ascii);
	static void M_Help_Key (int key, char ascii);
	static void M_Credits_Key (int key, char ascii);
	static void M_Quit_Key (int key, char ascii);
static void M_LanConfig_Key (int key, char ascii);
static void M_GameOptions_Key (int key, char ascii);
static void M_ServerList_Key (int key, char ascii);

static qboolean	m_entersound;		// play after drawing a frame, so caching won't disrupt the sound

void M_Update_Return_Reason(char *s)
{
	strlcpy(m_return_reason, s, sizeof(m_return_reason));
	if (s)
		Con_Printf("%s\n", s);
}

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)

// Nehahra
#define NumberOfNehahraDemos 34
typedef struct nehahrademonames_s
{
	char *name;
	char *desc;
} nehahrademonames_t;

static nehahrademonames_t NehahraDemos[NumberOfNehahraDemos] =
{
	{"intro", "Prologue"},
	{"genf", "The Beginning"},
	{"genlab", "A Doomed Project"},
	{"nehcre", "The New Recruits"},
	{"maxneh", "Breakthrough"},
	{"maxchar", "Renewal and Duty"},
	{"crisis", "Worlds Collide"},
	{"postcris", "Darkening Skies"},
	{"hearing", "The Hearing"},
	{"getjack", "On a Mexican Radio"},
	{"prelude", "Honor and Justice"},
	{"abase", "A Message Sent"},
	{"effect", "The Other Side"},
	{"uhoh", "Missing in Action"},
	{"prepare", "The Response"},
	{"vision", "Farsighted Eyes"},
	{"maxturns", "Enter the Immortal"},
	{"backlot", "Separate Ways"},
	{"maxside", "The Ancient Runes"},
	{"counter", "The New Initiative"},
	{"warprep", "Ghosts to the World"},
	{"counter1", "A Fate Worse Than Death"},
	{"counter2", "Friendly Fire"},
	{"counter3", "Minor Setback"},
	{"madmax", "Scores to Settle"},
	{"quake", "One Man"},
	{"cthmm", "Shattered Masks"},
	{"shades", "Deal with the Dead"},
	{"gophil", "An Unlikely Hero"},
	{"cstrike", "War in Hell"},
	{"shubset", "The Conspiracy"},
	{"shubdie", "Even Death May Die"},
	{"newranks", "An Empty Throne"},
	{"seal", "The Seal is Broken"}
};

static float menu_x, menu_y, menu_width, menu_height;

static void M_Background(int width, int height)
{
	menu_width = bound(1, width, vid_conwidth.integer);
	menu_height = bound(1, height, vid_conheight.integer);
	menu_x = (vid_conwidth.integer - menu_width) * 0.5;
	menu_y = (vid_conheight.integer - menu_height) * 0.5;
	//DrawQ_Pic(menu_x, menu_y, NULL, menu_width, menu_height, 0, 0, 0, 0.5, 0);
	DrawQ_Pic(0, 0, NULL, vid_conwidth.integer, vid_conheight.integer, 0, 0, 0, 0.5, 0);
}

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
static void M_DrawCharacter (float cx, float cy, int num)
{
	char temp[2];
	temp[0] = num;
	temp[1] = 0;
	DrawQ_String(menu_x + cx, menu_y + cy, temp, 1, 8, 8, 1, 1, 1, 1, 0);
}

static void M_PrintColored(float cx, float cy, const char *str)
{
	DrawQ_ColoredString(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0, NULL);
}

static void M_Print(float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
}

static void M_PrintRed(float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 0, 0, 1, 0);
}

static void M_ItemPrint(float cx, float cy, const char *str, int unghosted)
{
	if (unghosted)
		DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
	else
		DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 0.4, 0.4, 0.4, 1, 0);
}

static void M_DrawPic(float cx, float cy, const char *picname)
{
	DrawQ_Pic(menu_x + cx, menu_y + cy, Draw_CachePic(picname, true), 0, 0, 1, 1, 1, 1, 0);
}

static unsigned char identityTable[256];
static unsigned char translationTable[256];

static void M_BuildTranslationTable(int top, int bottom)
{
	int j;
	unsigned char *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	// LordHavoc: corrected skin color ranges
	if (top < 128 || (top >= 224 && top < 240))	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	// LordHavoc: corrected skin color ranges
	if (bottom < 128 || (bottom >= 224 && bottom < 240))
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}

static void M_DrawTextBox(float x, float y, float width, float height)
{
	int n;
	float cx, cy;

	// draw left side
	cx = x;
	cy = y;
	M_DrawPic (cx, cy, "gfx/box_tl");
	for (n = 0; n < height; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, "gfx/box_ml");
	}
	M_DrawPic (cx, cy+8, "gfx/box_bl");

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawPic (cx, cy, "gfx/box_tm");
		for (n = 0; n < height; n++)
		{
			cy += 8;
			if (n >= 1)
				M_DrawPic (cx, cy, "gfx/box_mm2");
			else
				M_DrawPic (cx, cy, "gfx/box_mm");
		}
		M_DrawPic (cx, cy+8, "gfx/box_bm");
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	M_DrawPic (cx, cy, "gfx/box_tr");
	for (n = 0; n < height; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, "gfx/box_mr");
	}
	M_DrawPic (cx, cy+8, "gfx/box_br");
}

//=============================================================================

//int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest != key_menu || m_state != m_main)
	{
		if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "1"))
			return;
		M_Menu_Main_f ();
	}
	else
	{
		if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "0"))
			return;
		key_dest = key_game;
		m_state = m_none;
	}
}


static int demo_cursor;
static void M_Demo_Draw (void)
{
	int i;

	M_Background(320, 200);

	for (i = 0;i < NumberOfNehahraDemos;i++)
		M_Print(16, 16 + 8*i, NehahraDemos[i].desc);

	// line cursor
	M_DrawCharacter (8, 16 + demo_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Menu_Demos_f (void)
{
	key_dest = key_menu;
	m_state = m_demo;
	m_entersound = true;
}


static void M_Demo_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		S_LocalSound ("sound/misc/menu2.wav");
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va ("playdemo %s\n", NehahraDemos[demo_cursor].name));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		demo_cursor--;
		if (demo_cursor < 0)
			demo_cursor = NumberOfNehahraDemos-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		demo_cursor++;
		if (demo_cursor >= NumberOfNehahraDemos)
			demo_cursor = 0;
		break;
	}
}

//=============================================================================
/* MAIN MENU */

static int	m_main_cursor;

static int MAIN_ITEMS = 4; // Nehahra: Menu Disable


void M_Menu_Main_f (void)
{
	if (gamemode == GAME_NEHAHRA)
	{
		if (NehGameType == TYPE_DEMO)
			MAIN_ITEMS = 4;
		else if (NehGameType == TYPE_GAME)
			MAIN_ITEMS = 5;
		else
			MAIN_ITEMS = 6;
	}
	else if (gamemode == GAME_NETHERWORLD)//VORTEX: menu restarting item
		MAIN_ITEMS = 6;
	else if (gamemode == GAME_TRANSFUSION) {
		if (sv.active && !cl.intermission && cl.islocalgame)
			MAIN_ITEMS = 8;
		else
			MAIN_ITEMS = 7;
	}
	else if (gamemode == GAME_NEXUIZ) {
		MAIN_ITEMS = 2;
	}
	else
		MAIN_ITEMS = 5;

	/*
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	*/
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}


static void M_Main_Draw (void)
{
	int		f;
	cachepic_t	*p;

	if (gamemode == GAME_TRANSFUSION) {
		int y1, y2, y3;
		M_Background(640, 480);
		p = Draw_CachePic ("gfx/menu/tb-transfusion", true);
		M_DrawPic (640/2 - p->width/2, 40, "gfx/menu/tb-transfusion");
		y2 = 120;
		// 8 rather than MAIN_ITEMS to skip a number and not miss the last option
		for (y1 = 1; y1 <= 8; y1++)
		{
			if (MAIN_ITEMS == 7 && y1 == 4)
				y1++;
			M_DrawPic (0, y2, va("gfx/menu/mainmenu%i", y1));
			y2 += 40;
		}
		if (MAIN_ITEMS == 7 && m_main_cursor > 2)
			y3 = m_main_cursor + 2;
		else
			y3 = m_main_cursor + 1;
		M_DrawPic (0, 120 + m_main_cursor * 40, va("gfx/menu/mainmenu%iselected", y3));
		return;
	}
	else if (gamemode == GAME_NEXUIZ)
	{
		M_Background(640, 480); //fall back is always to 640x480, this makes it most readable at that.
		M_PrintRed (40, (480/3)-16, "You have reached this menu due to missing or unlocatable content/data");
		M_Print ((640/2)-92, (480/3), "You may consider adding");
		M_Print ((640/2)-136, (480/3)+8, "-basedir /path/to/nexuiz");
		M_Print ((640/2)-76, (480/3)+16, "to your launch commandline");
		M_Print (640/2 - 48, 480/2, "Open Console"); //The console usually better shows errors (failures)
		M_Print (640/2 - 48, 480/2 + 8, "Quit");
		M_DrawCharacter(640/2 - 56, 480/2 + (8 * m_main_cursor), 12+((int)(realtime*4)&1));
		return;
	}
	M_Background(320, 200);
	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/ttl_main", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_main");
// Nehahra
	if (gamemode == GAME_NEHAHRA)
	{
		if (NehGameType == TYPE_BOTH)
			M_DrawPic (72, 32, "gfx/mainmenu");
		else if (NehGameType == TYPE_GAME)
			M_DrawPic (72, 32, "gfx/gamemenu");
		else
			M_DrawPic (72, 32, "gfx/demomenu");
	}
	else
		M_DrawPic (72, 32, "gfx/mainmenu");

	f = (int)(realtime * 10)%6;

	M_DrawPic (54, 32 + m_main_cursor * 20, va("gfx/menudot%i", f+1));
}


static void M_Main_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		//cls.demonum = m_save_demonum;
		//if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
		//	CL_NextDemo ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		if (gamemode == GAME_NEHAHRA)
		{
			switch (NehGameType)
			{
			case TYPE_BOTH:
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_Demos_f ();
					break;

				case 2:
					M_Menu_MultiPlayer_f ();
					break;

				case 3:
					M_Menu_Options_f ();
					break;

				case 4:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo endcred\n");
					break;

				case 5:
					M_Menu_Quit_f ();
					break;
				}
				break;
			case TYPE_GAME:
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_SinglePlayer_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo endcred\n");
					break;

				case 4:
					M_Menu_Quit_f ();
					break;
				}
				break;
			case TYPE_DEMO:
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_Demos_f ();
					break;

				case 1:
					key_dest = key_game;
					if (sv.active)
						Cbuf_AddText ("disconnect\n");
					Cbuf_AddText ("playdemo endcred\n");
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					M_Menu_Quit_f ();
					break;
				}
				break;
			}
		}
		else if (gamemode == GAME_NETHERWORLD)//VORTEX: menu restarting item
		{
			switch (m_main_cursor)
			{
			case 0:
				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				M_Menu_MultiPlayer_f ();
				break;

			case 2:
				M_Menu_Options_f ();
				break;

			case 3:
				M_Menu_Help_f ();
				break;

			case 4:
				M_Menu_Quit_f ();
				break;
			case 5:
				MR_Restart();
				break;
			}
		}
		else if (gamemode == GAME_TRANSFUSION) {
			if (MAIN_ITEMS == 7)
			{
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_Transfusion_Episode_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					M_Menu_Load_f ();
					break;

				case 4:
					M_Menu_Help_f ();
					break;

				case 5:
					M_Menu_Credits_f ();
					break;

				case 6:
					M_Menu_Quit_f ();
					break;
				}
			}
			else
			{
				switch (m_main_cursor)
				{
				case 0:
					M_Menu_Transfusion_Episode_f ();
					break;

				case 1:
					M_Menu_MultiPlayer_f ();
					break;

				case 2:
					M_Menu_Options_f ();
					break;

				case 3:
					M_Menu_Save_f ();
					break;

				case 4:
					M_Menu_Load_f ();
					break;

				case 5:
					M_Menu_Help_f ();
					break;

				case 6:
					M_Menu_Credits_f ();
					break;

				case 7:
					M_Menu_Quit_f ();
					break;
				}
			}
		}
		else if (gamemode == GAME_NEXUIZ) {
			switch (m_main_cursor)
			{
			case 0:
				if (cls.state == ca_connected)
				{
					m_state = m_none;
					key_dest = key_game;
				}
				Con_ToggleConsole_f ();
				break;
			case 1:
				M_Menu_Quit_f ();
				break;
			}
		}
		else
		{
			switch (m_main_cursor)
			{
			case 0:
				M_Menu_SinglePlayer_f ();
				break;

			case 1:
				M_Menu_MultiPlayer_f ();
				break;

			case 2:
				M_Menu_Options_f ();
				break;

			case 3:
				M_Menu_Help_f ();
				break;

			case 4:
				M_Menu_Quit_f ();
				break;
			}
		}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

static int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3


void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


static void M_SinglePlayer_Draw (void)
{
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/ttl_sgl", true);

	// Some mods don't have a single player mode
	if (gamemode == GAME_GOODVSBAD2 || gamemode == GAME_BATTLEMECH)
	{
		M_DrawPic ((320 - p->width) / 2, 4, "gfx/ttl_sgl");

		M_DrawTextBox (60, 8 * 8, 23, 4);
		if (gamemode == GAME_GOODVSBAD2)
			M_Print(95, 10 * 8, "Good Vs Bad 2 is for");
		else  // if (gamemode == GAME_BATTLEMECH)
			M_Print(95, 10 * 8, "Battlemech is for");
		M_Print(83, 11 * 8, "multiplayer play only");
	}
	else
	{
		int		f;

		M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_sgl");
		M_DrawPic (72, 32, "gfx/sp_menu");

		f = (int)(realtime * 10)%6;

		M_DrawPic (54, 32 + m_singleplayer_cursor * 20, va("gfx/menudot%i", f+1));
	}
}


static void M_SinglePlayer_Key (int key, char ascii)
{
	if (gamemode == GAME_GOODVSBAD2 || gamemode == GAME_BATTLEMECH)
	{
		if (key == K_ESCAPE || key == K_ENTER)
			m_state = m_main;
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("deathmatch 0\n");
			Cbuf_AddText ("coop 0\n");
			if (gamemode == GAME_TRANSFUSION)
			{
				key_dest = key_menu;
				M_Menu_Transfusion_Episode_f ();
				break;
			}
			Cbuf_AddText ("startmap_sp\n");
			break;

		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

static int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
static char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
static int		loadable[MAX_SAVEGAMES];

static void M_ScanSaves (void)
{
	int		i, j, len;
	char	name[MAX_OSPATH];
	char	buf[SAVEGAME_COMMENT_LENGTH + 256];
	const char *t;
	qfile_t	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "s%i.sav", (int)i);
		f = FS_Open (name, "rb", false, false);
		if (!f)
			continue;
		// read enough to get the comment
		len = FS_Read(f, buf, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = 0;
		t = buf;
		// version
		COM_ParseToken(&t, false);
		version = atoi(com_token);
		// description
		COM_ParseToken(&t, false);
		strlcpy (m_filenames[i], com_token, sizeof (m_filenames[i]));

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		FS_Close (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (!cl.islocalgame)
		return;
	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves ();
}


static void M_Load_Draw (void)
{
	int		i;
	cachepic_t	*p;

	M_Background(320, 200);

	p = Draw_CachePic ("gfx/p_load", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_load" );

	for (i=0 ; i< MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


static void M_Save_Draw (void)
{
	int		i;
	cachepic_t	*p;

	M_Background(320, 200);

	p = Draw_CachePic ("gfx/p_save", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_save");

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print(16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


static void M_Load_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		if (gamemode == GAME_TRANSFUSION)
			M_Menu_Main_f ();
		else
			M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		S_LocalSound ("sound/misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		// issue the load command
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


static void M_Save_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		if (gamemode == GAME_TRANSFUSION)
			M_Menu_Main_f ();
		else
			M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

//=============================================================================
/* Transfusion Single Player Episode Menu */

static int	m_episode_cursor;
#define	EPISODE_ITEMS	6

void M_Menu_Transfusion_Episode_f (void)
{
	m_entersound = true;
	m_state = m_transfusion_episode;
	key_dest = key_menu;
}

static void M_Transfusion_Episode_Draw (void)
{
	int y;
	cachepic_t *p;
	M_Background(640, 480);

	p = Draw_CachePic ("gfx/menu/tb-episodes", true);
	M_DrawPic (640/2 - p->width/2, 40, "gfx/menu/tb-episodes");
	for (y = 0; y < EPISODE_ITEMS; y++){
		M_DrawPic (0, 160 + y * 40, va("gfx/menu/episode%i", y+1));
	}

	M_DrawPic (0, 120 + (m_episode_cursor + 1) * 40, va("gfx/menu/episode%iselected", m_episode_cursor + 1));
}

static void M_Transfusion_Episode_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		m_episode_cursor++;
		if (m_episode_cursor >= EPISODE_ITEMS)
			m_episode_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		m_episode_cursor--;
		if (m_episode_cursor < 0)
			m_episode_cursor = EPISODE_ITEMS - 1;
		break;

	case K_ENTER:
		Cbuf_AddText ("deathmatch 0\n");
		m_entersound = true;
		M_Menu_Transfusion_Skill_f ();
	}
}

//=============================================================================
/* Transfusion Single Player Skill Menu */

static int	m_skill_cursor = 2;
#define	SKILL_ITEMS	5

void M_Menu_Transfusion_Skill_f (void)
{
	m_entersound = true;
	m_state = m_transfusion_skill;
	key_dest = key_menu;
}

static void M_Transfusion_Skill_Draw (void)
{
	int y;
	cachepic_t	*p;
	M_Background(640, 480);

	p = Draw_CachePic ("gfx/menu/tb-difficulty", true);
	M_DrawPic(640/2 - p->width/2, 40, "gfx/menu/tb-difficulty");

	for (y = 0; y < SKILL_ITEMS; y++)
	{
		M_DrawPic (0, 180 + y * 40, va("gfx/menu/difficulty%i", y+1));
	}
	M_DrawPic (0, 140 + (m_skill_cursor + 1) *40, va("gfx/menu/difficulty%iselected", m_skill_cursor + 1));
}

static void M_Transfusion_Skill_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Transfusion_Episode_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		m_skill_cursor++;
		if (m_skill_cursor >= SKILL_ITEMS)
			m_skill_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		m_skill_cursor--;
		if (m_skill_cursor < 0)
			m_skill_cursor = SKILL_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_skill_cursor)
		{
		case 0:
			Cbuf_AddText ("skill 1\n");
			break;
		case 1:
			Cbuf_AddText ("skill 2\n");
			break;
		case 2:
			Cbuf_AddText ("skill 3\n");
			break;
		case 3:
			Cbuf_AddText ("skill 4\n");
			break;
		case 4:
			Cbuf_AddText ("skill 5\n");
			break;
		}
		key_dest = key_game;
		if (sv.active)
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText ("maxplayers 1\n");
		Cbuf_AddText ("deathmatch 0\n");
		Cbuf_AddText ("coop 0\n");
		switch (m_episode_cursor)
		{
		case 0:
			Cbuf_AddText ("map e1m1\n");
			break;
		case 1:
			Cbuf_AddText ("map e2m1\n");
			break;
		case 2:
			Cbuf_AddText ("map e3m1\n");
			break;
		case 3:
			Cbuf_AddText ("map e4m1\n");
			break;
		case 4:
			Cbuf_AddText ("map e6m1\n");
			break;
		case 5:
			Cbuf_AddText ("map cp01\n");
			break;
		}
	}
}
//=============================================================================
/* MULTIPLAYER MENU */

static int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


static void M_MultiPlayer_Draw (void)
{
	int		f;
	cachepic_t	*p;

	if (gamemode == GAME_TRANSFUSION)
	{
		M_Background(640, 480);
		p = Draw_CachePic ("gfx/menu/tb-online", true);
		M_DrawPic (640/2 - p->width/2, 140, "gfx/menu/tb-online");
		for (f = 1; f <= MULTIPLAYER_ITEMS; f++)
			M_DrawPic (0, 180 + f*40, va("gfx/menu/online%i", f));
		M_DrawPic (0, 220 + m_multiplayer_cursor * 40, va("gfx/menu/online%iselected", m_multiplayer_cursor + 1));
		return;
	}
	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/p_multi", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi");
	M_DrawPic (72, 32, "gfx/mp_menu");

	f = (int)(realtime * 10)%6;

	M_DrawPic (54, 32 + m_multiplayer_cursor * 20, va("gfx/menudot%i", f+1));
}


static void M_MultiPlayer_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
		case 1:
			M_Menu_LanConfig_f ();
			break;

		case 2:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

static int		setup_cursor = 4;
static int		setup_cursor_table[] = {40, 64, 88, 124, 140};

static char	setup_myname[32];
static int		setup_oldtop;
static int		setup_oldbottom;
static int		setup_top;
static int		setup_bottom;
static int		setup_rate;
static int		setup_oldrate;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	strcpy(setup_myname, cl_name.string);
	setup_top = setup_oldtop = cl_color.integer >> 4;
	setup_bottom = setup_oldbottom = cl_color.integer & 15;
	setup_rate = cl_rate.integer;
}

static int menuplyr_width, menuplyr_height, menuplyr_top, menuplyr_bottom, menuplyr_load;
static unsigned char *menuplyr_pixels;
static unsigned int *menuplyr_translated;

typedef struct ratetable_s
{
	int rate;
	char *name;
}
ratetable_t;

#define RATES ((int)(sizeof(setup_ratetable)/sizeof(setup_ratetable[0])))
static ratetable_t setup_ratetable[] =
{
	{1000, "28.8 bad"},
	{1500, "28.8 mediocre"},
	{2000, "28.8 good"},
	{2500, "33.6 mediocre"},
	{3000, "33.6 good"},
	{3500, "56k bad"},
	{4000, "56k mediocre"},
	{4500, "56k adequate"},
	{5000, "56k good"},
	{7000, "64k ISDN"},
	{15000, "128k ISDN"},
	{25000, "broadband"}
};

static int setup_rateindex(int rate)
{
	int i;
	for (i = 0;i < RATES;i++)
		if (setup_ratetable[i].rate > setup_rate)
			break;
	return bound(1, i, RATES) - 1;
}

static void M_Setup_Draw (void)
{
	int i;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/p_multi", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi");

	M_Print(64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintColored(168, 40, setup_myname);

	if (gamemode != GAME_GOODVSBAD2)
	{
		M_Print(64, 64, "Shirt color");
		M_Print(64, 88, "Pants color");
	}

	M_Print(64, 124-8, "Network speed limit");
	M_Print(168, 124, va("%i (%s)", setup_rate, setup_ratetable[setup_rateindex(setup_rate)].name));

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print(72, 140, "Accept Changes");

	// LordHavoc: rewrote this code greatly
	if (menuplyr_load)
	{
		unsigned char *data, *f;
		fs_offset_t filesize;
		menuplyr_load = false;
		menuplyr_top = -1;
		menuplyr_bottom = -1;
		if ((f = FS_LoadFile("gfx/menuplyr.lmp", tempmempool, true, &filesize)))
		{
			data = LoadLMP (f, filesize, 0, 0, true);
			menuplyr_width = image_width;
			menuplyr_height = image_height;
			Mem_Free(f);
			menuplyr_pixels = (unsigned char *)Mem_Alloc(cls.permanentmempool, menuplyr_width * menuplyr_height);
			menuplyr_translated = (unsigned int *)Mem_Alloc(cls.permanentmempool, menuplyr_width * menuplyr_height * 4);
			memcpy(menuplyr_pixels, data, menuplyr_width * menuplyr_height);
			Mem_Free(data);
		}
	}

	if (menuplyr_pixels)
	{
		if (menuplyr_top != setup_top || menuplyr_bottom != setup_bottom)
		{
			menuplyr_top = setup_top;
			menuplyr_bottom = setup_bottom;
			M_BuildTranslationTable(menuplyr_top*16, menuplyr_bottom*16);
			for (i = 0;i < menuplyr_width * menuplyr_height;i++)
				menuplyr_translated[i] = palette_transparent[translationTable[menuplyr_pixels[i]]];
			Draw_NewPic("gfx/menuplyr", menuplyr_width, menuplyr_height, true, (unsigned char *)menuplyr_translated);
		}
		M_DrawPic(160, 48, "gfx/bigbox");
		M_DrawPic(172, 56, "gfx/menuplyr");
	}

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
	else
		M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));
}


static void M_Setup_Key (int k, char ascii)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 1)
			return;
		S_LocalSound ("sound/misc/menu3.wav");
		if (setup_cursor == 1)
			setup_top = setup_top - 1;
		if (setup_cursor == 2)
			setup_bottom = setup_bottom - 1;
		if (setup_cursor == 3)
		{
			l = setup_rateindex(setup_rate) - 1;
			if (l < 0)
				l = RATES - 1;
			setup_rate = setup_ratetable[l].rate;
		}
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 1)
			return;
forward:
		S_LocalSound ("sound/misc/menu3.wav");
		if (setup_cursor == 1)
			setup_top = setup_top + 1;
		if (setup_cursor == 2)
			setup_bottom = setup_bottom + 1;
		if (setup_cursor == 3)
		{
			l = setup_rateindex(setup_rate) + 1;
			if (l >= RATES)
				l = 0;
			setup_rate = setup_ratetable[l].rate;
		}
		break;

	case K_ENTER:
		if (setup_cursor == 0)
			return;

		if (setup_cursor == 1 || setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (Accept changes)
		if (strcmp(cl_name.string, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
		if (setup_rate != setup_oldrate)
			Cbuf_AddText(va("rate %i\n", setup_rate));

		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (ascii < 32)
			break;
		if (setup_cursor == 0)
		{
			l = (int)strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = ascii;
			}
		}
	}

	if (setup_top > 15)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 15;
	if (setup_bottom > 15)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 15;
}

//=============================================================================
/* OPTIONS MENU */

#define	SLIDER_RANGE	10

static void M_DrawSlider (int x, int y, float num, float rangemin, float rangemax)
{
	char text[16];
	int i;
	float range;
	range = bound(0, (num - rangemin) / (rangemax - rangemin), 1);
	M_DrawCharacter (x-8, y, 128);
	for (i = 0;i < SLIDER_RANGE;i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
	if (fabs((int)num - num) < 0.01)
		sprintf(text, "%i", (int)num);
	else
		sprintf(text, "%.2f", num);
	M_Print(x + (SLIDER_RANGE+2) * 8, y, text);
}

static void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print(x, y, "on");
	else
		M_Print(x, y, "off");
}


#define OPTIONS_ITEMS 39

static int options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

extern cvar_t slowmo;
extern dllhandle_t jpeg_dll;
extern cvar_t gl_texture_anisotropy;
extern cvar_t r_textshadow;

static void M_Menu_Options_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("sound/misc/menu3.wav");

	optnum = 7;
	if (options_cursor == optnum++)
		Cvar_SetValueQuick (&vid_conwidth, bound(320, vid_conwidth.value + dir * 64, 2048));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&vid_conheight, bound(240, vid_conheight.value + dir * 48, 1536));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_conalpha, bound(0, scr_conalpha.value + dir * 0.2, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_conbrightness, bound(0, scr_conbrightness.value + dir * 0.2, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&sbar_alpha_bg, bound(0, sbar_alpha_bg.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&sbar_alpha_fg, bound(0, sbar_alpha_fg.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_viewsize, bound(30, scr_viewsize.value + dir * 10, 120));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_fov, bound(1, scr_fov.integer + dir * 1, 170));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_screenshot_jpeg, !scr_screenshot_jpeg.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_screenshot_jpeg_quality, bound(0, scr_screenshot_jpeg_quality.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_screenshot_gammaboost, bound(0.1, scr_screenshot_gammaboost.value + dir * 0.1, 4));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&r_sky, !r_sky.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&gl_dither, !gl_dither.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&gl_texture_anisotropy, bound(1, gl_texture_anisotropy.integer + dir, gl_max_anisotropy));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&slowmo, bound(0, slowmo.value + dir * 0.25, 5));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&bgmvolume, bound(0, bgmvolume.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&volume, bound(0, volume.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&snd_staticvolume, bound(0, snd_staticvolume.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&r_textshadow, !r_textshadow.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&crosshair, bound(0, crosshair.integer + dir, 6));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&crosshair_size, bound(1, crosshair_size.value + dir, 5));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&showfps, !showfps.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&showtime, !showtime.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&showdate, !showdate.integer);
	else if (options_cursor == optnum++)
	{
		if (cl_forwardspeed.value > 200)
		{
			Cvar_SetValueQuick (&cl_forwardspeed, 200);
			Cvar_SetValueQuick (&cl_backspeed, 200);
		}
		else
		{
			Cvar_SetValueQuick (&cl_forwardspeed, 400);
			Cvar_SetValueQuick (&cl_backspeed, 400);
		}
	}
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&lookspring, !lookspring.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&lookstrafe, !lookstrafe.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&sensitivity, bound(1, sensitivity.value + dir * 0.5, 50));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&freelook, !freelook.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&m_pitch, -m_pitch.value);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&vid_mouse, !vid_mouse.integer);
}

static int optnum;
static int opty;
static int optcursor;

static void M_Options_PrintCommand(const char *s, int enabled)
{
	if (opty >= 32)
	{
		DrawQ_Pic(menu_x, menu_y + opty, NULL, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
	}
	opty += 8;
	optnum++;
}

static void M_Options_PrintCheckbox(const char *s, int enabled, int yes)
{
	if (opty >= 32)
	{
		DrawQ_Pic(menu_x, menu_y + opty, NULL, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
		M_DrawCheckbox(0 + (int)strlen(s) * 8 + 8, opty, yes);
	}
	opty += 8;
	optnum++;
}

static void M_Options_PrintSlider(const char *s, int enabled, float value, float minvalue, float maxvalue)
{
	if (opty >= 32)
	{
		DrawQ_Pic(menu_x, menu_y + opty, NULL, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
		M_DrawSlider(0 + (int)strlen(s) * 8 + 8, opty, value, minvalue, maxvalue);
	}
	opty += 8;
	optnum++;
}

static void M_Options_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, bound(200, 32 + OPTIONS_ITEMS * 8, vid_conheight.integer));

	M_DrawPic(16, 4, "gfx/qplaque");
	p = Draw_CachePic("gfx/p_option", true);
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option");

	optnum = 0;
	optcursor = options_cursor;
	visible = (int)((menu_height - 32) / 8);
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_ITEMS - visible)) * 8;

	M_Options_PrintCommand( "Customize controls", true);
	M_Options_PrintCommand( "     Go to console", true);
	M_Options_PrintCommand( " Reset to defaults", true);
	M_Options_PrintCommand( "             Video", true);
	M_Options_PrintCommand( "           Effects", true);
	M_Options_PrintCommand( "          Graphics", true);
	M_Options_PrintCommand( "     Color Control", true);
	M_Options_PrintSlider(  "  2D Screen Width ", true, vid_conwidth.value, 320, 2048);
	M_Options_PrintSlider(  "  2D Screen Height", true, vid_conheight.value, 240, 1536);
	M_Options_PrintSlider(  "     Console Alpha", true, scr_conalpha.value, 0, 1);
	M_Options_PrintSlider(  "Conback Brightness", true, scr_conbrightness.value, 0, 1);
	M_Options_PrintSlider(  "     Sbar Alpha BG", true, sbar_alpha_bg.value, 0, 1);
	M_Options_PrintSlider(  "     Sbar Alpha FG", true, sbar_alpha_fg.value, 0, 1);
	M_Options_PrintSlider(  "       Screen size", true, scr_viewsize.value, 30, 120);
	M_Options_PrintSlider(  "     Field of View", true, scr_fov.integer, 1, 170);
	M_Options_PrintCheckbox("  JPEG screenshots", jpeg_dll != NULL, scr_screenshot_jpeg.integer);
	M_Options_PrintSlider(  "      JPEG quality", jpeg_dll != NULL, scr_screenshot_jpeg_quality.value, 0, 1);
	M_Options_PrintSlider(  "  Screenshot Gamma", jpeg_dll != NULL, scr_screenshot_gammaboost.value, 0.1, 4);
	M_Options_PrintCheckbox("               Sky", true, r_sky.integer);
	M_Options_PrintCheckbox("         Dithering", true, gl_dither.integer);
	M_Options_PrintSlider(  "Anisotropic Filter", gl_support_anisotropy, gl_texture_anisotropy.integer, 1, gl_max_anisotropy);
	M_Options_PrintSlider(  "        Game Speed", sv.active, slowmo.value, 0, 5);
	M_Options_PrintSlider(  "   CD Music Volume", cdaudioinitialized.integer, bgmvolume.value, 0, 1);
	M_Options_PrintSlider(  "      Sound Volume", snd_initialized.integer, volume.value, 0, 1);
	M_Options_PrintSlider(gamemode == GAME_GOODVSBAD2 ? "      Music Volume" : "    Ambient Volume", snd_initialized.integer, snd_staticvolume.value, 0, 1);
	M_Options_PrintCheckbox("       Text Shadow", true, r_textshadow.integer);
	M_Options_PrintSlider(  "         Crosshair", true, crosshair.value, 0, 5);
	M_Options_PrintSlider(  "    Crosshair Size", true, crosshair_size.value, 1, 5);
	M_Options_PrintCheckbox("    Show Framerate", true, showfps.integer);
	M_Options_PrintCheckbox("         Show Time", true, showtime.integer);
	M_Options_PrintCheckbox("         Show Date", true, showdate.integer);
	M_Options_PrintCheckbox("        Always Run", true, cl_forwardspeed.value > 200);
	M_Options_PrintCheckbox("        Lookspring", true, lookspring.integer);
	M_Options_PrintCheckbox("        Lookstrafe", true, lookstrafe.integer);
	M_Options_PrintSlider(  "       Mouse Speed", true, sensitivity.value, 1, 50);
	M_Options_PrintCheckbox("        Mouse Look", true, freelook.integer);
	M_Options_PrintCheckbox("      Invert Mouse", true, m_pitch.value < 0);
	M_Options_PrintCheckbox("         Use Mouse", true, vid_mouse.integer);
}


static void M_Options_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			m_state = m_none;
			key_dest = key_game;
			Con_ToggleConsole_f ();
			break;
		case 2:
			M_Menu_Reset_f ();
			break;
		case 3:
			M_Menu_Video_f ();
			break;
		case 4:
			M_Menu_Options_Effects_f ();
			break;
		case 5:
			M_Menu_Options_Graphics_f ();
			break;
		case 6:
			M_Menu_Options_ColorControl_f ();
			break;
		default:
			M_Menu_Options_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_Menu_Options_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_Menu_Options_AdjustSliders (1);
		break;
	}
}

#define	OPTIONS_EFFECTS_ITEMS	36

static int options_effects_cursor;

void M_Menu_Options_Effects_f (void)
{
	key_dest = key_menu;
	m_state = m_options_effects;
	m_entersound = true;
}


extern cvar_t cl_stainmaps;
extern cvar_t cl_stainmaps_clearonload;
extern cvar_t r_explosionclip;
extern cvar_t r_coronas;
extern cvar_t gl_flashblend;
extern cvar_t cl_beams_polygons;
extern cvar_t cl_beams_quakepositionhack;
extern cvar_t cl_beams_instantaimhack;
extern cvar_t cl_beams_lightatend;
extern cvar_t r_lightningbeam_thickness;
extern cvar_t r_lightningbeam_scroll;
extern cvar_t r_lightningbeam_repeatdistance;
extern cvar_t r_lightningbeam_color_red;
extern cvar_t r_lightningbeam_color_green;
extern cvar_t r_lightningbeam_color_blue;
extern cvar_t r_lightningbeam_qmbtexture;

static void M_Menu_Options_Effects_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("sound/misc/menu3.wav");

	optnum = 0;
	     if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_coronas, bound(0, r_coronas.value + dir * 0.125, 4));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&gl_flashblend, !gl_flashblend.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles, !cl_particles.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_quake, !cl_particles_quake.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_quality, bound(1, cl_particles_quality.value + dir * 0.5, 4));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_explosions_shell, !cl_particles_explosions_shell.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_explosionclip, !r_explosionclip.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_stainmaps, !cl_stainmaps.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_stainmaps_clearonload, !cl_stainmaps_clearonload.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_decals, !cl_decals.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_bulletimpacts, !cl_particles_bulletimpacts.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_smoke, !cl_particles_smoke.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_sparks, !cl_particles_sparks.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_bubbles, !cl_particles_bubbles.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood, !cl_particles_blood.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood_alpha, bound(0.2, cl_particles_blood_alpha.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood_bloodhack, !cl_particles_blood_bloodhack.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_beams_polygons, !cl_beams_polygons.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_beams_instantaimhack, !cl_beams_instantaimhack.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_beams_quakepositionhack, !cl_beams_quakepositionhack.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_beams_lightatend, !cl_beams_lightatend.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_thickness, bound(1, r_lightningbeam_thickness.integer + dir, 10));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_scroll, bound(0, r_lightningbeam_scroll.integer + dir, 10));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_repeatdistance, bound(64, r_lightningbeam_repeatdistance.integer + dir * 64, 1024));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_color_red, bound(0, r_lightningbeam_color_red.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_color_green, bound(0, r_lightningbeam_color_green.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_color_blue, bound(0, r_lightningbeam_color_blue.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lightningbeam_qmbtexture, !r_lightningbeam_qmbtexture.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lerpmodels, !r_lerpmodels.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_lerpsprites, !r_lerpsprites.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&gl_polyblend, bound(0, gl_polyblend.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_skyscroll1, bound(-8, r_skyscroll1.value + dir * 0.1, 8));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_skyscroll2, bound(-8, r_skyscroll2.value + dir * 0.1, 8));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_waterwarp, bound(0, r_waterwarp.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_wateralpha, bound(0, r_wateralpha.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_waterscroll, bound(0, r_waterscroll.value + dir * 0.5, 10));
}

static void M_Options_Effects_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, bound(200, 32 + OPTIONS_EFFECTS_ITEMS * 8, vid_conheight.integer));

	M_DrawPic(16, 4, "gfx/qplaque");
	p = Draw_CachePic("gfx/p_option", true);
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option");

	optcursor = options_effects_cursor;
	optnum = 0;
	visible = (int)((menu_height - 32) / 8);
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_EFFECTS_ITEMS - visible)) * 8;

	M_Options_PrintSlider(  "      Corona Intensity", true, r_coronas.value, 0, 4);
	M_Options_PrintCheckbox("      Use Only Coronas", true, gl_flashblend.integer);
	M_Options_PrintCheckbox("             Particles", true, cl_particles.integer);
	M_Options_PrintCheckbox(" Quake-style Particles", true, cl_particles_quake.integer);
	M_Options_PrintSlider(  "     Particles Quality", true, cl_particles_quality.value, 1, 4);
	M_Options_PrintCheckbox("       Explosion Shell", true, cl_particles_explosions_shell.integer);
	M_Options_PrintCheckbox("  Explosion Shell Clip", true, r_explosionclip.integer);
	M_Options_PrintCheckbox("             Stainmaps", true, cl_stainmaps.integer);
	M_Options_PrintCheckbox("Onload Clear Stainmaps", true, cl_stainmaps_clearonload.integer);
	M_Options_PrintCheckbox("                Decals", true, cl_decals.integer);
	M_Options_PrintCheckbox("        Bullet Impacts", true, cl_particles_bulletimpacts.integer);
	M_Options_PrintCheckbox("                 Smoke", true, cl_particles_smoke.integer);
	M_Options_PrintCheckbox("                Sparks", true, cl_particles_sparks.integer);
	M_Options_PrintCheckbox("               Bubbles", true, cl_particles_bubbles.integer);
	M_Options_PrintCheckbox("                 Blood", true, cl_particles_blood.integer);
	M_Options_PrintSlider(  "         Blood Opacity", true, cl_particles_blood_alpha.value, 0.2, 1);
	M_Options_PrintCheckbox("Force New Blood Effect", true, cl_particles_blood_bloodhack.integer);
	M_Options_PrintCheckbox("     Polygon Lightning", true, cl_beams_polygons.integer);
	M_Options_PrintCheckbox("Smooth Sweep Lightning", true, cl_beams_instantaimhack.integer);
	M_Options_PrintCheckbox(" Waist-level Lightning", true, cl_beams_quakepositionhack.integer);
	M_Options_PrintCheckbox("   Lightning End Light", true, cl_beams_lightatend.integer);
	M_Options_PrintSlider(  "   Lightning Thickness", cl_beams_polygons.integer, r_lightningbeam_thickness.integer, 1, 10);
	M_Options_PrintSlider(  "      Lightning Scroll", cl_beams_polygons.integer, r_lightningbeam_scroll.integer, 0, 10);
	M_Options_PrintSlider(  " Lightning Repeat Dist", cl_beams_polygons.integer, r_lightningbeam_repeatdistance.integer, 64, 1024);
	M_Options_PrintSlider(  "   Lightning Color Red", cl_beams_polygons.integer, r_lightningbeam_color_red.value, 0, 1);
	M_Options_PrintSlider(  " Lightning Color Green", cl_beams_polygons.integer, r_lightningbeam_color_green.value, 0, 1);
	M_Options_PrintSlider(  "  Lightning Color Blue", cl_beams_polygons.integer, r_lightningbeam_color_blue.value, 0, 1);
	M_Options_PrintCheckbox(" Lightning QMB Texture", cl_beams_polygons.integer, r_lightningbeam_qmbtexture.integer);
	M_Options_PrintCheckbox("   Model Interpolation", true, r_lerpmodels.integer);
	M_Options_PrintCheckbox("  Sprite Interpolation", true, r_lerpsprites.integer);
	M_Options_PrintSlider(  "            View Blend", true, gl_polyblend.value, 0, 1);
	M_Options_PrintSlider(  "Upper Sky Scroll Speed", true, r_skyscroll1.value, -8, 8);
	M_Options_PrintSlider(  "Lower Sky Scroll Speed", true, r_skyscroll2.value, -8, 8);
	M_Options_PrintSlider(  "  Underwater View Warp", true, r_waterwarp.value, 0, 1);
	M_Options_PrintSlider(  " Water Alpha (opacity)", true, r_wateralpha.value, 0, 1);
	M_Options_PrintSlider(  "        Water Movement", true, r_waterscroll.value, 0, 10);
}


static void M_Options_Effects_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_ENTER:
		M_Menu_Options_Effects_AdjustSliders (1);
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_effects_cursor--;
		if (options_effects_cursor < 0)
			options_effects_cursor = OPTIONS_EFFECTS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_effects_cursor++;
		if (options_effects_cursor >= OPTIONS_EFFECTS_ITEMS)
			options_effects_cursor = 0;
		break;

	case K_LEFTARROW:
		M_Menu_Options_Effects_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_Menu_Options_Effects_AdjustSliders (1);
		break;
	}
}


#define	OPTIONS_GRAPHICS_ITEMS	18

static int options_graphics_cursor;

void M_Menu_Options_Graphics_f (void)
{
	key_dest = key_menu;
	m_state = m_options_graphics;
	m_entersound = true;
}

extern cvar_t r_shadow_gloss;
extern cvar_t r_shadow_realtime_dlight;
extern cvar_t r_shadow_realtime_dlight_shadows;
extern cvar_t r_shadow_realtime_world;
extern cvar_t r_shadow_realtime_world_dlightshadows;
extern cvar_t r_shadow_realtime_world_lightmaps;
extern cvar_t r_shadow_realtime_world_shadows;
extern cvar_t r_bloom;
extern cvar_t r_bloom_intensity;
extern cvar_t r_bloom_power;
extern cvar_t r_bloom_blur;
extern cvar_t r_bloom_resolution;
extern cvar_t r_hdr;
extern cvar_t r_hdr_bloomintensity;
extern cvar_t r_hdr_scenebrightness;
extern cvar_t r_hdr_glowintensity;
extern cvar_t gl_picmip;

static void M_Menu_Options_Graphics_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("sound/misc/menu3.wav");

	optnum = 0;

		 if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_gloss,							bound(0, r_shadow_gloss.integer + dir, 2));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_dlight,				!r_shadow_realtime_dlight.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_dlight_shadows,		!r_shadow_realtime_dlight_shadows.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world,					!r_shadow_realtime_world.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_dlightshadows,	!r_shadow_realtime_world_dlightshadows.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_lightmaps,		bound(0, r_shadow_realtime_world_lightmaps.value + dir * 0.1, 1));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_shadows,			!r_shadow_realtime_world_shadows.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_bloom,                                 !r_bloom.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_hdr,                                   !r_hdr.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_hdr_bloomintensity,                    bound(0.125, r_hdr_bloomintensity.value + dir * 0.125, 4));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_hdr_glowintensity,                     bound(0, r_hdr_glowintensity.value + dir * 0.25, 4));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_bloom_power,                           bound(1, r_bloom_power.value + dir * 1, 16));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_bloom_intensity,                       bound(1, r_bloom_intensity.value + dir * 0.25, 16));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_bloom_blur,                            bound(1, r_bloom_blur.value + dir * 1, 16));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_bloom_resolution,                      bound(64, r_bloom_resolution.value + dir * 64, 2048));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_hdr_scenebrightness,                   bound(0.25, r_hdr_scenebrightness.value + dir * 0.125, 4));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&gl_picmip,                               bound(0, gl_picmip.value - dir, 3));
	else if (options_graphics_cursor == optnum++) Cbuf_AddText ("r_restart\n");
}


static void M_Options_Graphics_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, bound(200, 32 + OPTIONS_GRAPHICS_ITEMS * 8, vid_conheight.integer));

	M_DrawPic(16, 4, "gfx/qplaque");
	p = Draw_CachePic("gfx/p_option", true);
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option");

	optcursor = options_graphics_cursor;
	optnum = 0;
	visible = (int)((menu_height - 32) / 8);
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_GRAPHICS_ITEMS - visible)) * 8;

	M_Options_PrintSlider(  "             Gloss Mode", true, r_shadow_gloss.integer, 0, 2);
	M_Options_PrintCheckbox("             RT DLights", true, r_shadow_realtime_dlight.integer);
	M_Options_PrintCheckbox("      RT DLight Shadows", true, r_shadow_realtime_dlight_shadows.integer);
	M_Options_PrintCheckbox("               RT World", true, r_shadow_realtime_world.integer);
	M_Options_PrintCheckbox("RT World DLight Shadows", true, r_shadow_realtime_world_dlightshadows.integer);
	M_Options_PrintSlider(  "     RT World Lightmaps", true, r_shadow_realtime_world_lightmaps.value, 0, 1);
	M_Options_PrintCheckbox("        RT World Shadow", true, r_shadow_realtime_world_shadows.integer);
	M_Options_PrintCheckbox("           Bloom Effect", !r_hdr.integer, r_bloom.integer);
	M_Options_PrintCheckbox("       HDR Bloom Effect", r_hdr.integer, r_hdr.integer);
	M_Options_PrintSlider(  "    HDR Bloom Intensity", r_hdr.integer, r_hdr_bloomintensity.value, 0.125, 4);
	M_Options_PrintSlider(  "     HDR Glow Intensity", r_hdr.integer, r_hdr_glowintensity.value, 0, 4);
	M_Options_PrintSlider(  "Non-HDR Bloom Darkening", !r_hdr.integer && r_bloom.integer, r_bloom_power.value, 1, 16);
	M_Options_PrintSlider(  "        Bloom Intensity", r_hdr.integer || r_bloom.integer, r_bloom_intensity.value, 1, 16);
	M_Options_PrintSlider(  "             Bloom Blur", r_hdr.integer || r_bloom.integer, r_bloom_blur.value, 1, 16);
	M_Options_PrintSlider(  "       Bloom Resolution", r_hdr.integer || r_bloom.integer, r_bloom_resolution.value, 64, 2048);
	M_Options_PrintSlider(  "       Scene Brightness", true, r_hdr_scenebrightness.value, 0.25, 4);
	M_Options_PrintSlider(  "        Texture Quality", true, gl_picmip.value, 3, 0);
	M_Options_PrintCommand( "       Restart Renderer", true);
}


static void M_Options_Graphics_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_ENTER:
		M_Menu_Options_Graphics_AdjustSliders (1);
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_graphics_cursor--;
		if (options_graphics_cursor < 0)
			options_graphics_cursor = OPTIONS_GRAPHICS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_graphics_cursor++;
		if (options_graphics_cursor >= OPTIONS_GRAPHICS_ITEMS)
			options_graphics_cursor = 0;
		break;

	case K_LEFTARROW:
		M_Menu_Options_Graphics_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_Menu_Options_Graphics_AdjustSliders (1);
		break;
	}
}


#define	OPTIONS_COLORCONTROL_ITEMS	18

static int		options_colorcontrol_cursor;

// intensity value to match up to 50% dither to 'correct' quake
static cvar_t menu_options_colorcontrol_correctionvalue = {0, "menu_options_colorcontrol_correctionvalue", "0.5", "intensity value that matches up to white/black dither pattern, should be 0.5 for linear color"};

void M_Menu_Options_ColorControl_f (void)
{
	key_dest = key_menu;
	m_state = m_options_colorcontrol;
	m_entersound = true;
}


static void M_Menu_Options_ColorControl_AdjustSliders (int dir)
{
	int optnum;
	float f;
	S_LocalSound ("sound/misc/menu3.wav");

	optnum = 1;
	if (options_colorcontrol_cursor == optnum++)
		Cvar_SetValueQuick (&v_hwgamma, !v_hwgamma.integer);
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_gamma, bound(1, v_gamma.value + dir * 0.125, 5));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_contrast, bound(1, v_contrast.value + dir * 0.125, 5));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_brightness, bound(0, v_brightness.value + dir * 0.05, 0.8));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, !v_color_enable.integer);
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_r, bound(0, v_color_black_r.value + dir * 0.0125, 0.8));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_g, bound(0, v_color_black_g.value + dir * 0.0125, 0.8));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_b, bound(0, v_color_black_b.value + dir * 0.0125, 0.8));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(0, (v_color_black_r.value + v_color_black_g.value + v_color_black_b.value) / 3 + dir * 0.0125, 0.8);
		Cvar_SetValueQuick (&v_color_black_r, f);
		Cvar_SetValueQuick (&v_color_black_g, f);
		Cvar_SetValueQuick (&v_color_black_b, f);
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_r, bound(0, v_color_grey_r.value + dir * 0.0125, 0.95));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_g, bound(0, v_color_grey_g.value + dir * 0.0125, 0.95));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_b, bound(0, v_color_grey_b.value + dir * 0.0125, 0.95));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(0, (v_color_grey_r.value + v_color_grey_g.value + v_color_grey_b.value) / 3 + dir * 0.0125, 0.95);
		Cvar_SetValueQuick (&v_color_grey_r, f);
		Cvar_SetValueQuick (&v_color_grey_g, f);
		Cvar_SetValueQuick (&v_color_grey_b, f);
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_r, bound(1, v_color_white_r.value + dir * 0.125, 5));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_g, bound(1, v_color_white_g.value + dir * 0.125, 5));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_b, bound(1, v_color_white_b.value + dir * 0.125, 5));
	}
	else if (options_colorcontrol_cursor == optnum++)
	{
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(1, (v_color_white_r.value + v_color_white_g.value + v_color_white_b.value) / 3 + dir * 0.125, 5);
		Cvar_SetValueQuick (&v_color_white_r, f);
		Cvar_SetValueQuick (&v_color_white_g, f);
		Cvar_SetValueQuick (&v_color_white_b, f);
	}
}

static void M_Options_ColorControl_Draw (void)
{
	int visible;
	float x, c, s, t, u, v;
	cachepic_t	*p, *dither;

	dither = Draw_CachePic("gfx/colorcontrol/ditherpattern", true);

	M_Background(320, 256);

	M_DrawPic(16, 4, "gfx/qplaque");
	p = Draw_CachePic("gfx/p_option", true);
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option");

	optcursor = options_colorcontrol_cursor;
	optnum = 0;
	visible = (int)((menu_height - 32) / 8);
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_COLORCONTROL_ITEMS - visible)) * 8;

	M_Options_PrintCommand( "     Reset to defaults", true);
	M_Options_PrintCheckbox("Hardware Gamma Control", vid_hardwaregammasupported.integer, v_hwgamma.integer);
	M_Options_PrintSlider(  "                 Gamma", !v_color_enable.integer && vid_hardwaregammasupported.integer && v_hwgamma.integer, v_gamma.value, 1, 5);
	M_Options_PrintSlider(  "              Contrast", !v_color_enable.integer, v_contrast.value, 1, 5);
	M_Options_PrintSlider(  "            Brightness", !v_color_enable.integer, v_brightness.value, 0, 0.8);
	M_Options_PrintCheckbox("  Color Level Controls", true, v_color_enable.integer);
	M_Options_PrintSlider(  "          Black: Red  ", v_color_enable.integer, v_color_black_r.value, 0, 0.8);
	M_Options_PrintSlider(  "          Black: Green", v_color_enable.integer, v_color_black_g.value, 0, 0.8);
	M_Options_PrintSlider(  "          Black: Blue ", v_color_enable.integer, v_color_black_b.value, 0, 0.8);
	M_Options_PrintSlider(  "          Black: Grey ", v_color_enable.integer, (v_color_black_r.value + v_color_black_g.value + v_color_black_b.value) / 3, 0, 0.8);
	M_Options_PrintSlider(  "           Grey: Red  ", v_color_enable.integer && vid_hardwaregammasupported.integer && v_hwgamma.integer, v_color_grey_r.value, 0, 0.95);
	M_Options_PrintSlider(  "           Grey: Green", v_color_enable.integer && vid_hardwaregammasupported.integer && v_hwgamma.integer, v_color_grey_g.value, 0, 0.95);
	M_Options_PrintSlider(  "           Grey: Blue ", v_color_enable.integer && vid_hardwaregammasupported.integer && v_hwgamma.integer, v_color_grey_b.value, 0, 0.95);
	M_Options_PrintSlider(  "           Grey: Grey ", v_color_enable.integer && vid_hardwaregammasupported.integer && v_hwgamma.integer, (v_color_grey_r.value + v_color_grey_g.value + v_color_grey_b.value) / 3, 0, 0.95);
	M_Options_PrintSlider(  "          White: Red  ", v_color_enable.integer, v_color_white_r.value, 1, 5);
	M_Options_PrintSlider(  "          White: Green", v_color_enable.integer, v_color_white_g.value, 1, 5);
	M_Options_PrintSlider(  "          White: Blue ", v_color_enable.integer, v_color_white_b.value, 1, 5);
	M_Options_PrintSlider(  "          White: Grey ", v_color_enable.integer, (v_color_white_r.value + v_color_white_g.value + v_color_white_b.value) / 3, 1, 5);

	opty += 4;
	DrawQ_Pic(menu_x, menu_y + opty, NULL, 320, 4 + 64 + 8 + 64 + 4, 0, 0, 0, 1, 0);opty += 4;
	s = (float) 312 / 2 * vid.width / vid_conwidth.integer;
	t = (float) 4 / 2 * vid.height / vid_conheight.integer;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, dither, 312, 4, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL  , 312, 4, 0,0, 0,0,0,1, 1,0, 1,0,0,1, 0,1, 0,0,0,1, 1,1, 1,0,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, dither, 312, 4, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL  , 312, 4, 0,0, 0,0,0,1, 1,0, 0,1,0,1, 0,1, 0,0,0,1, 1,1, 0,1,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, dither, 312, 4, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL  , 312, 4, 0,0, 0,0,0,1, 1,0, 0,0,1,1, 0,1, 0,0,0,1, 1,1, 0,0,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, dither, 312, 4, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL  , 312, 4, 0,0, 0,0,0,1, 1,0, 1,1,1,1, 0,1, 0,0,0,1, 1,1, 1,1,1,1, 0);opty += 4;

	c = menu_options_colorcontrol_correctionvalue.value; // intensity value that should be matched up to a 50% dither to 'correct' quake
	s = (float) 48 / 2 * vid.width / vid_conwidth.integer;
	t = (float) 48 / 2 * vid.height / vid_conheight.integer;
	u = s * 0.5;
	v = t * 0.5;
	opty += 8;
	x = 4;
	DrawQ_Pic(menu_x + x, menu_y + opty, NULL, 64, 48, c, 0, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, dither, 16, 16, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, dither, 16, 16, 0,0, 1,0,0,1, u,0, 1,0,0,1, 0,v, 1,0,0,1, u,v, 1,0,0,1, 0);
	x += 80;
	DrawQ_Pic(menu_x + x, menu_y + opty, NULL, 64, 48, 0, c, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, dither, 16, 16, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, dither, 16, 16, 0,0, 0,1,0,1, u,0, 0,1,0,1, 0,v, 0,1,0,1, u,v, 0,1,0,1, 0);
	x += 80;
	DrawQ_Pic(menu_x + x, menu_y + opty, NULL, 64, 48, 0, 0, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, dither, 16, 16, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, dither, 16, 16, 0,0, 0,0,1,1, u,0, 0,0,1,1, 0,v, 0,0,1,1, u,v, 0,0,1,1, 0);
	x += 80;
	DrawQ_Pic(menu_x + x, menu_y + opty, NULL, 64, 48, c, c, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, dither, 16, 16, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, dither, 16, 16, 0,0, 1,1,1,1, u,0, 1,1,1,1, 0,v, 1,1,1,1, u,v, 1,1,1,1, 0);
}


static void M_Options_ColorControl_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_ENTER:
		m_entersound = true;
		switch (options_colorcontrol_cursor)
		{
		case 0:
			Cvar_SetValueQuick(&v_hwgamma, 1);
			Cvar_SetValueQuick(&v_gamma, 1);
			Cvar_SetValueQuick(&v_contrast, 1);
			Cvar_SetValueQuick(&v_brightness, 0);
			Cvar_SetValueQuick(&v_color_enable, 0);
			Cvar_SetValueQuick(&v_color_black_r, 0);
			Cvar_SetValueQuick(&v_color_black_g, 0);
			Cvar_SetValueQuick(&v_color_black_b, 0);
			Cvar_SetValueQuick(&v_color_grey_r, 0);
			Cvar_SetValueQuick(&v_color_grey_g, 0);
			Cvar_SetValueQuick(&v_color_grey_b, 0);
			Cvar_SetValueQuick(&v_color_white_r, 1);
			Cvar_SetValueQuick(&v_color_white_g, 1);
			Cvar_SetValueQuick(&v_color_white_b, 1);
			break;
		default:
			M_Menu_Options_ColorControl_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_colorcontrol_cursor--;
		if (options_colorcontrol_cursor < 0)
			options_colorcontrol_cursor = OPTIONS_COLORCONTROL_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		options_colorcontrol_cursor++;
		if (options_colorcontrol_cursor >= OPTIONS_COLORCONTROL_ITEMS)
			options_colorcontrol_cursor = 0;
		break;

	case K_LEFTARROW:
		M_Menu_Options_ColorControl_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_Menu_Options_ColorControl_AdjustSliders (1);
		break;
	}
}


//=============================================================================
/* KEYS MENU */

static char *quakebindnames[][2] =
{
{"+attack", 		"attack"},
{"impulse 10", 		"next weapon"},
{"impulse 12", 		"previous weapon"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

static char *transfusionbindnames[][2] =
{
{"",				"Movement"},		// Movement commands
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+jump", 			"jump / swim up"},
{"+movedown",		"swim down"},
{"",				"Combat"},			// Combat commands
{"impulse 1",		"Pitch Fork"},
{"impulse 2",		"Flare Gun"},
{"impulse 3",		"Shotgun"},
{"impulse 4",		"Machine Gun"},
{"impulse 5",		"Incinerator"},
{"impulse 6",		"Bombs (TNT)"},
{"impulse 35",		"Proximity Bomb"},
{"impulse 36",		"Remote Detonator"},
{"impulse 7",		"Aerosol Can"},
{"impulse 8",		"Tesla Cannon"},
{"impulse 9",		"Life Leech"},
{"impulse 10",		"Voodoo Doll"},
{"impulse 21",		"next weapon"},
{"impulse 22",		"previous weapon"},
{"+attack", 		"attack"},
{"+button3",		"altfire"},
{"",				"Inventory"},		// Inventory commands
{"impulse 40",		"Dr.'s Bag"},
{"impulse 41",		"Crystal Ball"},
{"impulse 42",		"Beast Vision"},
{"impulse 43",		"Jump Boots"},
{"impulse 23",		"next item"},
{"impulse 24",		"previous item"},
{"impulse 25",		"use item"},
{"",				"Misc"},			// Misc commands
{"+button4",		"use"},
{"impulse 50",		"add bot (red)"},
{"impulse 51",		"add bot (blue)"},
{"impulse 52",		"kick a bot"},
{"impulse 26",		"next armor type"},
{"impulse 27",		"identify player"},
{"impulse 55",		"voting menu"},
{"impulse 56",		"observer mode"},
{"",				"Taunts"},            // Taunts
{"impulse 70",		"taunt 0"},
{"impulse 71",		"taunt 1"},
{"impulse 72",		"taunt 2"},
{"impulse 73",		"taunt 3"},
{"impulse 74",		"taunt 4"},
{"impulse 75",		"taunt 5"},
{"impulse 76",		"taunt 6"},
{"impulse 77",		"taunt 7"},
{"impulse 78",		"taunt 8"},
{"impulse 79",		"taunt 9"}
};

static char *goodvsbad2bindnames[][2] =
{
{"impulse 69",		"Power 1"},
{"impulse 70",		"Power 2"},
{"impulse 71",		"Power 3"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"kill", 			"kill yourself"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

static int numcommands;
static char *(*bindnames)[2];

/*
typedef struct binditem_s
{
	char *command, *description;
	struct binditem_s *next;
}
binditem_t;

typedef struct bindcategory_s
{
	char *name;
	binditem_t *binds;
	struct bindcategory_s *next;
}
bindcategory_t;

static bindcategory_t *bindcategories = NULL;

static void M_ClearBinds (void)
{
	for (c = bindcategories;c;c = cnext)
	{
		cnext = c->next;
		for (b = c->binds;b;b = bnext)
		{
			bnext = b->next;
			Z_Free(b);
		}
		Z_Free(c);
	}
	bindcategories = NULL;
}

static void M_AddBindToCategory(bindcategory_t *c, char *command, char *description)
{
	for (b = &c->binds;*b;*b = &(*b)->next);
	*b = Z_Alloc(sizeof(binditem_t) + strlen(command) + 1 + strlen(description) + 1);
	*b->command = (char *)((*b) + 1);
	*b->description = *b->command + strlen(command) + 1;
	strcpy(*b->command, command);
	strcpy(*b->description, description);
}

static void M_AddBind (char *category, char *command, char *description)
{
	for (c = &bindcategories;*c;c = &(*c)->next)
	{
		if (!strcmp(category, (*c)->name))
		{
			M_AddBindToCategory(*c, command, description);
			return;
		}
	}
	*c = Z_Alloc(sizeof(bindcategory_t));
	M_AddBindToCategory(*c, command, description);
}

static void M_DefaultBinds (void)
{
	M_ClearBinds();
	M_AddBind("movement", "+jump", "jump / swim up");
	M_AddBind("movement", "+forward", "walk forward");
	M_AddBind("movement", "+back", "backpedal");
	M_AddBind("movement", "+left", "turn left");
	M_AddBind("movement", "+right", "turn right");
	M_AddBind("movement", "+speed", "run");
	M_AddBind("movement", "+moveleft", "step left");
	M_AddBind("movement", "+moveright", "step right");
	M_AddBind("movement", "+strafe", "sidestep");
	M_AddBind("movement", "+lookup", "look up");
	M_AddBind("movement", "+lookdown", "look down");
	M_AddBind("movement", "centerview", "center view");
	M_AddBind("movement", "+mlook", "mouse look");
	M_AddBind("movement", "+klook", "keyboard look");
	M_AddBind("movement", "+moveup", "swim up");
	M_AddBind("movement", "+movedown", "swim down");
	M_AddBind("weapons", "+attack", "attack");
	M_AddBind("weapons", "impulse 10", "next weapon");
	M_AddBind("weapons", "impulse 12", "previous weapon");
	M_AddBind("weapons", "impulse 1", "select weapon 1 (axe)");
	M_AddBind("weapons", "impulse 2", "select weapon 2 (shotgun)");
	M_AddBind("weapons", "impulse 3", "select weapon 3 (super )");
	M_AddBind("weapons", "impulse 4", "select weapon 4 (nailgun)");
	M_AddBind("weapons", "impulse 5", "select weapon 5 (super nailgun)");
	M_AddBind("weapons", "impulse 6", "select weapon 6 (grenade launcher)");
	M_AddBind("weapons", "impulse 7", "select weapon 7 (rocket launcher)");
	M_AddBind("weapons", "impulse 8", "select weapon 8 (lightning gun)");
}
*/


static int		keys_cursor;
static int		bind_grab;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

#define NUMKEYS 5

void M_FindKeysForCommand (const char *command, int *keys)
{
	int		count;
	int		j;
	char	*b;

	for (j = 0;j < NUMKEYS;j++)
		keys[j] = -1;

	count = 0;

	for (j = 0; j < (int)sizeof (keybindings[0]) / (int)sizeof (keybindings[0][0]); j++)
	{
		b = keybindings[0][j];
		if (!b)
			continue;
		if (!strcmp (b, command) )
		{
			keys[count++] = j;
			if (count == NUMKEYS)
				break;
		}
	}
}

static void M_UnbindCommand (char *command)
{
	int		j;
	char	*b;

	for (j = 0; j < (int)sizeof (keybindings[0]) / (int)sizeof (keybindings[0][0]); j++)
	{
		b = keybindings[0][j];
		if (!b)
			continue;
		if (!strcmp (b, command))
			Key_SetBinding (j, 0, "");
	}
}


static void M_Keys_Draw (void)
{
	int		i, j;
	int		keys[NUMKEYS];
	int		y;
	cachepic_t	*p;
	char	keystring[MAX_INPUTLINE];

	M_Background(320, 48 + 8 * numcommands);

	p = Draw_CachePic ("gfx/ttl_cstm", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_cstm");

	if (bind_grab)
		M_Print(12, 32, "Press a key or button for this action");
	else
		M_Print(18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<numcommands ; i++)
	{
		y = 48 + 8*i;

		// If there's no command, it's just a section
		if (bindnames[i][0][0] == '\0')
		{
			M_PrintRed (4, y, "\x0D");  // #13 is the little arrow pointing to the right
			M_PrintRed (16, y, bindnames[i][1]);
			continue;
		}
		else
			M_Print(16, y, bindnames[i][1]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		// LordHavoc: redesigned to print more than 2 keys, inspired by Tomaz's MiniRacer
		if (keys[0] == -1)
			strcpy(keystring, "???");
		else
		{
			keystring[0] = 0;
			for (j = 0;j < NUMKEYS;j++)
			{
				if (keys[j] != -1)
				{
					if (j > 0)
						strcat(keystring, " or ");
					strcat(keystring, Key_KeynumToString (keys[j]));
				}
			}
		}
		M_Print(150, y, keystring);
	}

	if (bind_grab)
		M_DrawCharacter (140, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (140, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


static void M_Keys_Key (int k, char ascii)
{
	char	cmd[80];
	int		keys[NUMKEYS];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("sound/misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else //if (k != '`')
		{
			sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = false;
		return;
	}

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		do
		{
			keys_cursor--;
			if (keys_cursor < 0)
				keys_cursor = numcommands-1;
		}
		while (bindnames[keys_cursor][0][0] == '\0');  // skip sections
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		do
		{
			keys_cursor++;
			if (keys_cursor >= numcommands)
				keys_cursor = 0;
		}
		while (bindnames[keys_cursor][0][0] == '\0');  // skip sections
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("sound/misc/menu2.wav");
		if (keys[NUMKEYS - 1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("sound/misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

void M_Menu_Reset_f (void)
{
	key_dest = key_menu;
	m_state = m_reset;
	m_entersound = true;
}


static void M_Reset_Key (int key, char ascii)
{
	switch (key)
	{
	case 'Y':
	case 'y':
		Cbuf_AddText ("cvar_resettodefaults_all;exec default.cfg\n");
		// no break here since we also exit the menu

	case K_ESCAPE:
	case 'n':
	case 'N':
		m_state = m_options;
		m_entersound = true;
		break;

	default:
		break;
	}
}

static void M_Reset_Draw (void)
{
	int lines = 2, linelength = 20;
	M_Background(linelength * 8 + 16, lines * 8 + 16);
	M_DrawTextBox(0, 0, linelength, lines);
	M_Print(8 + 4 * (linelength - 19),  8, "Really wanna reset?");
	M_Print(8 + 4 * (linelength - 11), 16, "Press y / n");
}

//=============================================================================
/* VIDEO MENU */

// note: if modes are added to the beginning of this list, update VID_DEFAULT
typedef struct video_resolution_s
{
	const char *type;
	int width, height;
	int conwidth, conheight;
	double pixelheight; // pixel aspect
}
video_resolution_t;
video_resolution_t video_resolutions[] =
{
{"Standard 4x3"              ,  320, 240, 320, 240, 1     },
{"Standard 4x3"              ,  400, 300, 400, 300, 1     },
{"Standard 4x3"              ,  512, 384, 512, 384, 1     },
{"Standard 4x3"              ,  640, 480, 640, 480, 1     },
{"Standard 4x3"              ,  800, 600, 640, 480, 1     },
{"Standard 4x3"              , 1024, 768, 640, 480, 1     },
{"Standard 4x3"              , 1152, 864, 640, 480, 1     },
{"Standard 4x3"              , 1280, 960, 640, 480, 1     },
{"Standard 4x3"              , 1400,1050, 640, 480, 1     },
{"Standard 4x3"              , 1600,1200, 640, 480, 1     },
{"Standard 4x3"              , 1792,1344, 640, 480, 1     },
{"Standard 4x3"              , 1856,1392, 640, 480, 1     },
{"Standard 4x3"              , 1920,1440, 640, 480, 1     },
{"Standard 4x3"              , 2048,1536, 640, 480, 1     },
{"Short Pixel (CRT) 5x4"     ,  320, 256, 320, 256, 0.9375},
{"Short Pixel (CRT) 5x4"     ,  640, 512, 640, 512, 0.9375},
{"Short Pixel (CRT) 5x4"     , 1280,1024, 640, 512, 0.9375},
{"Tall Pixel (CRT) 8x5"      ,  320, 200, 320, 200, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  640, 400, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  840, 525, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      ,  960, 600, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      , 1680,1050, 640, 400, 1.2   },
{"Tall Pixel (CRT) 8x5"      , 1920,1200, 640, 400, 1.2   },
{"Square Pixel (LCD) 5x4"    ,  320, 256, 320, 256, 1     },
{"Square Pixel (LCD) 5x4"    ,  640, 512, 640, 512, 1     },
{"Square Pixel (LCD) 5x4"    , 1280,1024, 640, 512, 1     },
{"WideScreen 5x3"            ,  640, 384, 640, 384, 1     },
{"WideScreen 5x3"            , 1280, 768, 640, 384, 1     },
{"WideScreen 8x5"            ,  320, 200, 320, 200, 1     },
{"WideScreen 8x5"            ,  640, 400, 640, 400, 1     },
{"WideScreen 8x5"            ,  720, 450, 720, 450, 1     },
{"WideScreen 8x5"            ,  840, 525, 640, 400, 1     },
{"WideScreen 8x5"            ,  960, 600, 640, 400, 1     },
{"WideScreen 8x5"            , 1280, 800, 640, 400, 1     },
{"WideScreen 8x5"            , 1440, 900, 720, 450, 1     },
{"WideScreen 8x5"            , 1680,1050, 640, 400, 1     },
{"WideScreen 8x5"            , 1920,1200, 640, 400, 1     },
{"WideScreen 8x5"            , 2560,1600, 640, 400, 1     },
{"WideScreen 8x5"            , 3840,2400, 640, 400, 1     },
{"WideScreen 14x9"           ,  840, 540, 640, 400, 1     },
{"WideScreen 14x9"           , 1680,1080, 640, 400, 1     },
{"WideScreen 16x9"           ,  640, 360, 640, 360, 1     },
{"WideScreen 16x9"           ,  683, 384, 683, 384, 1     },
{"WideScreen 16x9"           ,  960, 540, 640, 360, 1     },
{"WideScreen 16x9"           , 1280, 720, 640, 360, 1     },
{"WideScreen 16x9"           , 1366, 768, 683, 384, 1     },
{"WideScreen 16x9"           , 1920,1080, 640, 360, 1     },
{"WideScreen 16x9"           , 2560,1440, 640, 360, 1     },
{"WideScreen 16x9"           , 3840,2160, 640, 360, 1     },
{"NTSC 3x2"                  ,  360, 240, 360, 240, 1.125 },
{"NTSC 3x2"                  ,  720, 480, 720, 480, 1.125 },
{"PAL 14x11"                 ,  360, 283, 360, 283, 0.9545},
{"PAL 14x11"                 ,  720, 566, 720, 566, 0.9545},
{"NES 8x7"                   ,  256, 224, 256, 224, 1.1667},
{"SNES 8x7"                  ,  512, 448, 512, 448, 1.1667},
{NULL, 0, 0, 0, 0, 0}
};
// this is the number of the default mode (640x480) in the list above
#define VID_DEFAULT 3
#define VID_RES_COUNT ((int)(sizeof(video_resolutions) / sizeof(video_resolutions[0])) - 1)

#define VIDEO_ITEMS 7
static int video_cursor = 0;
static int video_cursor_table[] = {56, 68, 88, 100, 112, 132, 162};
static int video_resolution;

void M_Menu_Video_f (void)
{
	int i;

	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	// Look for the closest match to the current resolution
	video_resolution = 0;
	for (i = 1;i < VID_RES_COUNT;i++)
	{
		// if the new mode would be a worse match in width, skip it
		if (fabs(video_resolutions[i].width - vid.width) > fabs(video_resolutions[video_resolution].width - vid.width))
			continue;
		// if it is equal in width, check height
		if (video_resolutions[i].width == vid.width && video_resolutions[video_resolution].width == vid.width)
		{
			// if the new mode would be a worse match in height, skip it
			if (fabs(video_resolutions[i].height - vid.height) > fabs(video_resolutions[video_resolution].height - vid.height))
				continue;
			// if it is equal in width and height, check pixel aspect
			if (video_resolutions[i].height == vid.height && video_resolutions[video_resolution].height == vid.height)
			{
				// if the new mode would be a worse match in pixel aspect, skip it
				if (fabs(video_resolutions[i].pixelheight - vid_pixelheight.value) > fabs(video_resolutions[video_resolution].pixelheight - vid_pixelheight.value))
					continue;
				// if it is equal in everything, skip it (prefer earlier modes)
				if (video_resolutions[i].pixelheight == vid_pixelheight.value && video_resolutions[video_resolution].pixelheight == vid_pixelheight.value)
					continue;
				// better match for width, height, and pixel aspect
				video_resolution = i;
			}
			else // better match for width and height
				video_resolution = i;
		}
		else // better match for width
			video_resolution = i;
	}
}


static void M_Video_Draw (void)
{
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic(16, 4, "gfx/qplaque");
	p = Draw_CachePic("gfx/vidmodes", true);
	M_DrawPic((320-p->width)/2, 4, "gfx/vidmodes");

	// Current Resolution
	M_Print(16, video_cursor_table[0], "    Current Resolution");
	if (vid_supportrefreshrate && vid.fullscreen)
		M_Print(220, video_cursor_table[0], va("%dx%d %dhz", vid.width, vid.height, vid.refreshrate));
	else
		M_Print(220, video_cursor_table[0], va("%dx%d", vid.width, vid.height));

	// Proposed Resolution
	M_Print(16, video_cursor_table[1], "        New Resolution");
	M_Print(220, video_cursor_table[1], va("%dx%d", video_resolutions[video_resolution].width, video_resolutions[video_resolution].height));
	M_Print(96, video_cursor_table[1] + 8, va("Type: %s", video_resolutions[video_resolution].type));

	// Bits per pixel
	M_Print(16, video_cursor_table[2], "        Bits per pixel");
	M_Print(220, video_cursor_table[2], (vid_bitsperpixel.integer == 32) ? "32" : "16");

	// Refresh Rate
	M_ItemPrint(16, video_cursor_table[3], "          Refresh Rate", vid_supportrefreshrate);
	M_DrawSlider(220, video_cursor_table[3], vid_refreshrate.integer, 60, 150);

	// Fullscreen
	M_Print(16, video_cursor_table[4], "            Fullscreen");
	M_DrawCheckbox(220, video_cursor_table[4], vid_fullscreen.integer);

	// "Apply" button
	M_Print(220, video_cursor_table[5], "Apply");

	// Vertical Sync
	M_ItemPrint(16, video_cursor_table[6], "         Vertical Sync", gl_videosyncavailable);
	M_DrawCheckbox(220, video_cursor_table[6], vid_vsync.integer);

	// Cursor
	M_DrawCharacter(200, video_cursor_table[video_cursor], 12+((int)(realtime*4)&1));
}


static void M_Menu_Video_AdjustSliders (int dir)
{
	S_LocalSound ("sound/misc/menu3.wav");

	switch (video_cursor)
	{
		// Resolution
		case 1:
		{
			int r;
			for(r = 0;r < VID_RES_COUNT;r++)
			{
				video_resolution += dir;
				if (video_resolution >= VID_RES_COUNT)
					video_resolution = 0;
				if (video_resolution < 0)
					video_resolution = VID_RES_COUNT - 1;
				if (video_resolutions[video_resolution].width >= vid_minwidth.integer && video_resolutions[video_resolution].height >= vid_minheight.integer)
					break;
			}
			break;
		}

		// Bits per pixel
		case 2:
			Cvar_SetValueQuick (&vid_bitsperpixel, (vid_bitsperpixel.integer == 32) ? 16 : 32);
			break;
		// Refresh Rate
		case 3:
			Cvar_SetValueQuick (&vid_refreshrate, vid_refreshrate.integer + dir);
			break;
		case 4:
			Cvar_SetValueQuick (&vid_fullscreen, !vid_fullscreen.integer);
			break;

		case 6:
			Cvar_SetValueQuick (&vid_vsync, !vid_vsync.integer);
			break;
	}
}


static void M_Video_Key (int key, char ascii)
{
	switch (key)
	{
		case K_ESCAPE:
			// vid_shared.c has a copy of the current video config. We restore it
			Cvar_SetValueQuick(&vid_fullscreen, vid.fullscreen);
			Cvar_SetValueQuick(&vid_bitsperpixel, vid.bitsperpixel);
			if (vid_supportrefreshrate)
				Cvar_SetValueQuick(&vid_refreshrate, vid.refreshrate);

			S_LocalSound ("sound/misc/menu1.wav");
			M_Menu_Options_f ();
			break;

		case K_ENTER:
			m_entersound = true;
			switch (video_cursor)
			{
				case 5:
					Cvar_SetValueQuick (&vid_width, video_resolutions[video_resolution].width);
					Cvar_SetValueQuick (&vid_height, video_resolutions[video_resolution].height);
					Cvar_SetValueQuick (&vid_conwidth, video_resolutions[video_resolution].conwidth);
					Cvar_SetValueQuick (&vid_conheight, video_resolutions[video_resolution].conheight);
					Cvar_SetValueQuick (&vid_pixelheight, video_resolutions[video_resolution].pixelheight);
					Cbuf_AddText ("vid_restart\n");
					M_Menu_Options_f ();
					break;
				default:
					M_Menu_Video_AdjustSliders (1);
			}
			break;

		case K_UPARROW:
			S_LocalSound ("sound/misc/menu1.wav");
			video_cursor--;
			if (video_cursor < 0)
				video_cursor = VIDEO_ITEMS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("sound/misc/menu1.wav");
			video_cursor++;
			if (video_cursor >= VIDEO_ITEMS)
				video_cursor = 0;
			break;

		case K_LEFTARROW:
			M_Menu_Video_AdjustSliders (-1);
			break;

		case K_RIGHTARROW:
			M_Menu_Video_AdjustSliders (1);
			break;
	}
}

//=============================================================================
/* HELP MENU */

static int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



static void M_Help_Draw (void)
{
	M_Background(320, 200);
	M_DrawPic (0, 0, va("gfx/help%i", help_page));
}


static void M_Help_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* CEDITS MENU */

void M_Menu_Credits_f (void)
{
	key_dest = key_menu;
	m_state = m_credits;
	m_entersound = true;
}



static void M_Credits_Draw (void)
{
	M_Background(640, 480);
	M_DrawPic (0, 0, "gfx/creditsmiddle");
	M_Print (640/2 - 14/2*8, 236, "Coming soon...");
	M_DrawPic (0, 0, "gfx/creditstop");
	M_DrawPic (0, 433, "gfx/creditsbottom");
}


static void M_Credits_Key (int key, char ascii)
{
		M_Menu_Main_f ();
}

//=============================================================================
/* QUIT MENU */

static char *m_quit_message[9];
static int		m_quit_prevstate;
static qboolean	wasInMenus;


static int M_QuitMessage(char *line1, char *line2, char *line3, char *line4, char *line5, char *line6, char *line7, char *line8)
{
	m_quit_message[0] = line1;
	m_quit_message[1] = line2;
	m_quit_message[2] = line3;
	m_quit_message[3] = line4;
	m_quit_message[4] = line5;
	m_quit_message[5] = line6;
	m_quit_message[6] = line7;
	m_quit_message[7] = line8;
	m_quit_message[8] = NULL;
	return 1;
}

static int M_ChooseQuitMessage(int request)
{
	switch (gamemode)
	{
	case GAME_NORMAL:
	case GAME_HIPNOTIC:
	case GAME_ROGUE:
	case GAME_NEHAHRA:
	case GAME_DEFEATINDETAIL2:
		if (request-- == 0) return M_QuitMessage("Are you gonna quit","this game just like","everything else?",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Milord, methinks that","thou art a lowly","quitter. Is this true?",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Do I need to bust your","face open for trying","to quit?",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Man, I oughta smack you","for trying to quit!","Press Y to get","smacked out.",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Press Y to quit like a","big loser in life.","Press N to stay proud","and successful!",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("If you press Y to","quit, I will summon","Satan all over your","hard drive!",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Um, Asmodeus dislikes","his children trying to","quit. Press Y to return","to your Tinkertoys.",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("If you quit now, I'll","throw a blanket-party","for you next time!",NULL,NULL,NULL,NULL,NULL);
		break;
	case GAME_GOODVSBAD2:
		if (request-- == 0) return M_QuitMessage("Press Yes To Quit","...","Yes",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Do you really want to","Quit?","Play Good vs bad 3!",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("All your quit are","belong to long duck","dong",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Press Y to quit","","But are you too legit?",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("This game was made by","e@chip-web.com","It is by far the best","game ever made.",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Even I really dont","know of a game better","Press Y to quit","like rougue chedder",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("After you stop playing","tell the guys who made","counterstrike to just","kill themselves now",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Press Y to exit to DOS","","SSH login as user Y","to exit to Linux",NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Press Y like you","were waanderers","from Ys'",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("This game was made in","Nippon like the SS","announcer's saying ipon",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("you","want to quit?",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Please stop playing","this stupid game",NULL,NULL,NULL,NULL,NULL,NULL);
		break;
	case GAME_BATTLEMECH:
		if (request-- == 0) return M_QuitMessage("? WHY ?","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Leave now and your mech is scrap!","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Accept Defeat?","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Wait! There are more mechs to destroy!","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Where's your bloodlust?","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Your mech here is way more impressive","than your car out there...","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Quitting won't reduce your debt","Press Y to quit, N to keep fraggin'",NULL,NULL,NULL,NULL,NULL,NULL);
		break;
	case GAME_OPENQUARTZ:
		if (request-- == 0) return M_QuitMessage("There is nothing like free beer!","Press Y to quit, N to stay",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("GNU is not Unix!","Press Y to quit, N to stay",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("You prefer free beer over free speech?","Press Y to quit, N to stay",NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Is OpenQuartz Propaganda?","Press Y to quit, N to stay",NULL,NULL,NULL,NULL,NULL,NULL);
		break;
	case GAME_NEXUIZ: //frag related quit messages are pointless for a fallback menu!
		if (request-- == 0) return M_QuitMessage("Are you sure you want to quit?","Press Y to quit, N to stay",NULL,NULL,NULL,NULL,NULL,NULL);
		break;
	default:
		if (request-- == 0) return M_QuitMessage("Tired of fragging already?",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Quit now and forfeit your bodycount?",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Are you sure you want to quit?",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
		if (request-- == 0) return M_QuitMessage("Off to do something constructive?",NULL,NULL,NULL,NULL,NULL,NULL,NULL);
		break;
	}
	return 0;
};

void M_Menu_Quit_f (void)
{
	int n;
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	// count how many there are
	for (n = 0;M_ChooseQuitMessage(n);n++);
	// choose one
	M_ChooseQuitMessage(rand() % n);
}


static void M_Quit_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = (enum m_state_e)m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'Y':
	case 'y':
		Host_Quit_f ();
		break;

	default:
		break;
	}
}

static void M_Quit_Draw (void)
{
	int i, l, linelength, firstline, lastline, lines;
	for (i = 0, linelength = 0, firstline = 9999, lastline = -1;m_quit_message[i];i++)
	{
		if ((l = (int)strlen(m_quit_message[i])))
		{
			if (firstline > i)
				firstline = i;
			if (lastline < i)
				lastline = i;
			if (linelength < l)
				linelength = l;
		}
	}
	lines = (lastline - firstline) + 1;
	M_Background(linelength * 8 + 16, lines * 8 + 16);
	if (gamemode != GAME_NEXUIZ) //since this is a fallback menu for Nexuiz (no graphics), it is very hard to read with the box
		M_DrawTextBox(0, 0, linelength, lines); //this is less obtrusive than hacking up the M_DrawTextBox function for Nexuiz
	for (i = 0, l = firstline;i < lines;i++, l++)
		M_Print(8 + 4 * (linelength - strlen(m_quit_message[l])), 8 + 8 * i, m_quit_message[l]);
}

//=============================================================================
/* LAN CONFIG MENU */

static int		lanConfig_cursor = -1;
static int		lanConfig_cursor_table [] = {56, 76, 84, 120};
#define NUM_LANCONFIG_CMDS	4

static int 	lanConfig_port;
static char	lanConfig_portname[6];
static char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame)
			lanConfig_cursor = 1;
	}
	if (StartingGame)
		lanConfig_cursor = 1;
	lanConfig_port = 26000;
	sprintf(lanConfig_portname, "%u", lanConfig_port);

	M_Update_Return_Reason("");
}


static void M_LanConfig_Draw (void)
{
	cachepic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/p_multi", true);
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, "gfx/p_multi");

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	protocol = "TCP/IP";
	M_Print(basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print(basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print(basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print(basex, lanConfig_cursor_table[1], "Search for DarkPlaces games...");
		M_Print(basex, lanConfig_cursor_table[2], "Search for QuakeWorld games...");
		M_Print(basex, lanConfig_cursor_table[3]-16, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[3]-8, 22, 1);
		M_Print(basex+16, lanConfig_cursor_table[3], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print(basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [lanConfig_cursor], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 3)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [lanConfig_cursor], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_Print(basex, 168, m_return_reason);
}


static void M_LanConfig_Key (int key, char ascii)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		// when in start game menu, skip the unused search qw servers item
		if (StartingGame && lanConfig_cursor == 2)
			lanConfig_cursor = 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		// when in start game menu, skip the unused search qw servers item
		if (StartingGame && lanConfig_cursor == 1)
			lanConfig_cursor = 2;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		Cbuf_AddText ("stopdemo\n");

		Cvar_SetValue("port", lanConfig_port);

		if (lanConfig_cursor == 1 || lanConfig_cursor == 2)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_ServerList_f();
			break;
		}

		if (lanConfig_cursor == 3)
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 3)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (ascii < 32)
			break;

		if (lanConfig_cursor == 3)
		{
			l = (int)strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = ascii;
			}
		}

		if (ascii < '0' || ascii > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = (int)strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = ascii;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 3)
	{
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l =  atoi(lanConfig_portname);
	if (l <= 65535)
		lanConfig_port = l;
	sprintf(lanConfig_portname, "%u", lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct level_s
{
	char	*name;
	char	*description;
} level_t;

typedef struct episode_s
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

typedef struct gamelevels_s
{
	char *gamename;
	level_t *levels;
	episode_t *episodes;
	int numepisodes;
}
gamelevels_t;

static level_t quakelevels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

static episode_t quakeepisodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

 //MED 01/06/97 added hipnotic levels
static level_t     hipnoticlevels[] =
{
   {"start", "Command HQ"},  // 0

   {"hip1m1", "The Pumping Station"},          // 1
   {"hip1m2", "Storage Facility"},
   {"hip1m3", "The Lost Mine"},
   {"hip1m4", "Research Facility"},
   {"hip1m5", "Military Complex"},

   {"hip2m1", "Ancient Realms"},          // 6
   {"hip2m2", "The Black Cathedral"},
   {"hip2m3", "The Catacombs"},
   {"hip2m4", "The Crypt"},
   {"hip2m5", "Mortum's Keep"},
   {"hip2m6", "The Gremlin's Domain"},

   {"hip3m1", "Tur Torment"},       // 12
   {"hip3m2", "Pandemonium"},
   {"hip3m3", "Limbo"},
   {"hip3m4", "The Gauntlet"},

   {"hipend", "Armagon's Lair"},       // 16

   {"hipdm1", "The Edge of Oblivion"}           // 17
};

//MED 01/06/97  added hipnotic episodes
static episode_t   hipnoticepisodes[] =
{
   {"Scourge of Armagon", 0, 1},
   {"Fortress of the Dead", 1, 5},
   {"Dominion of Darkness", 6, 6},
   {"The Rift", 12, 4},
   {"Final Level", 16, 1},
   {"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
static level_t		roguelevels[] =
{
	{"start",	"Split Decision"},
	{"r1m1",	"Deviant's Domain"},
	{"r1m2",	"Dread Portal"},
	{"r1m3",	"Judgement Call"},
	{"r1m4",	"Cave of Death"},
	{"r1m5",	"Towers of Wrath"},
	{"r1m6",	"Temple of Pain"},
	{"r1m7",	"Tomb of the Overlord"},
	{"r2m1",	"Tempus Fugit"},
	{"r2m2",	"Elemental Fury I"},
	{"r2m3",	"Elemental Fury II"},
	{"r2m4",	"Curse of Osiris"},
	{"r2m5",	"Wizard's Keep"},
	{"r2m6",	"Blood Sacrifice"},
	{"r2m7",	"Last Bastion"},
	{"r2m8",	"Source of Evil"},
	{"ctf1",    "Division of Change"}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
static episode_t	rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

static level_t		nehahralevels[] =
{
	{"nehstart",	"Welcome to Nehahra"},
	{"neh1m1",	"Forge City1: Slipgates"},
	{"neh1m2",	"Forge City2: Boiler"},
	{"neh1m3",	"Forge City3: Escape"},
	{"neh1m4",	"Grind Core"},
	{"neh1m5",	"Industrial Silence"},
	{"neh1m6",	"Locked-Up Anger"},
	{"neh1m7",	"Wanderer of the Wastes"},
	{"neh1m8",	"Artemis System Net"},
	{"neh1m9",	"To the Death"},
	{"neh2m1",	"The Gates of Ghoro"},
	{"neh2m2",	"Sacred Trinity"},
	{"neh2m3",	"Realm of the Ancients"},
	{"neh2m4",	"Temple of the Ancients"},
	{"neh2m5",	"Dreams Made Flesh"},
	{"neh2m6",	"Your Last Cup of Sorrow"},
	{"nehsec",	"Ogre's Bane"},
	{"nehahra",	"Nehahra's Den"},
	{"nehend",	"Quintessence"}
};

static episode_t	nehahraepisodes[] =
{
	{"Welcome to Nehahra", 0, 1},
	{"The Fall of Forge", 1, 9},
	{"The Outlands", 10, 7},
	{"Dimension of the Lost", 17, 2}
};

// Map list for Transfusion
static level_t		transfusionlevels[] =
{
	{"e1m1",		"Cradle to Grave"},
	{"e1m2",		"Wrong Side of the Tracks"},
	{"e1m3",		"Phantom Express"},
	{"e1m4",		"Dark Carnival"},
	{"e1m5",		"Hallowed Grounds"},
	{"e1m6",		"The Great Temple"},
	{"e1m7",		"Altar of Stone"},
	{"e1m8",		"House of Horrors"},

	{"e2m1",		"Shipwrecked"},
	{"e2m2",		"The Lumber Mill"},
	{"e2m3",		"Rest for the Wicked"},
	{"e2m4",		"The Overlooked Hotel"},
	{"e2m5",		"The Haunting"},
	{"e2m6",		"The Cold Rush"},
	{"e2m7",		"Bowels of the Earth"},
	{"e2m8",		"The Lair of Shial"},
	{"e2m9",		"Thin Ice"},

	{"e3m1",		"Ghost Town"},
	{"e3m2",		"The Siege"},
	{"e3m3",		"Raw Sewage"},
	{"e3m4",		"The Sick Ward"},
	{"e3m5",		"Spare Parts"},
	{"e3m6",		"Monster Bait"},
	{"e3m7",		"The Pit of Cerberus"},
	{"e3m8",		"Catacombs"},

	{"e4m1",		"Butchery Loves Company"},
	{"e4m2",		"Breeding Grounds"},
	{"e4m3",		"Charnel House"},
	{"e4m4",		"Crystal Lake"},
	{"e4m5",		"Fire and Brimstone"},
	{"e4m6",		"The Ganglion Depths"},
	{"e4m7",		"In the Flesh"},
	{"e4m8",		"The Hall of the Epiphany"},
	{"e4m9",		"Mall of the Dead"},

	{"bb1",			"The Stronghold"},
	{"bb2",			"Winter Wonderland"},
	{"bb3",			"Bodies"},
	{"bb4",			"The Tower"},
	{"bb5",			"Click!"},
	{"bb6",			"Twin Fortress"},
	{"bb7",			"Midgard"},
	{"bb8",			"Fun With Heads"},
	{"dm1",			"Monolith Building 11"},
	{"dm2",			"Power!"},
	{"dm3",			"Area 15"},

	{"e6m1",		"Welcome to Your Life"},
	{"e6m2",		"They Are Here"},
	{"e6m3",		"Public Storage"},
	{"e6m4",		"Aqueducts"},
	{"e6m5",		"The Ruined Temple"},
	{"e6m6",		"Forbidden Rituals"},
	{"e6m7",		"The Dungeon"},
	{"e6m8",		"Beauty and the Beast"},
	{"e6m9",		"Forgotten Catacombs"},

	{"cp01",		"Boat Docks"},
	{"cp02",		"Old Opera House"},
	{"cp03",		"Gothic Library"},
	{"cp04",		"Lost Monastery"},
	{"cp05",		"Steamboat"},
	{"cp06",		"Graveyard"},
	{"cp07",		"Mountain Pass"},
	{"cp08",		"Abysmal Mine"},
	{"cp09",		"Castle"},
	{"cps1",		"Boggy Creek"},

	{"cpbb01",		"Crypt of Despair"},
	{"cpbb02",		"Pits of Blood"},
	{"cpbb03",		"Unholy Cathedral"},
	{"cpbb04",		"Deadly Inspirations"},

	{"b2a15",		"Area 15 (B2)"},
	{"b2bodies",	"BB_Bodies (B2)"},
	{"b2cabana",	"BB_Cabana"},
	{"b2power",		"BB_Power"},
	{"barena",		"Blood Arena"},
	{"bkeep",		"Blood Keep"},
	{"bstar",		"Brown Star"},
	{"crypt",		"The Crypt"},

	{"bb3_2k1",		"Bodies Infusion"},
	{"captasao",	"Captasao"},
	{"curandero",	"Curandero"},
	{"dcamp",		"DeathCamp"},
	{"highnoon",	"HighNoon"},
	{"qbb1",		"The Confluence"},
	{"qbb2",		"KathartiK"},
	{"qbb3",		"Caleb's Woodland Retreat"},
	{"zoo",			"Zoo"},

	{"dranzbb6",	"Black Coffee"},
	{"fragm",		"Frag'M"},
	{"maim",		"Maim"},
	{"qe1m7",		"The House of Chthon"},
	{"qdm1",		"Place of Two Deaths"},
	{"qdm4",		"The Bad Place"},
	{"qdm5",		"The Cistern"},
	{"qmorbias",	"DM-Morbias"},
	{"simple",		"Dead Simple"}
};

static episode_t	transfusionepisodes[] =
{
	{"The Way of All Flesh", 0, 8},
	{"Even Death May Die", 8, 9},
	{"Farewell to Arms", 17, 8},
	{"Dead Reckoning", 25, 9},
	{"BloodBath", 34, 11},
	{"Post Mortem", 45, 9},
	{"Cryptic Passage", 54, 10},
	{"Cryptic BloodBath", 64, 4},
	{"Blood 2", 68, 8},
	{"Transfusion", 76, 9},
	{"Conversions", 85, 9}
};

static level_t goodvsbad2levels[] =
{
	{"rts", "Many Paths"},  // 0
	{"chess", "Chess, Scott Hess"},                         // 1
	{"dot", "Big Wall"},
	{"city2", "The Big City"},
	{"bwall", "0 G like Psychic TV"},
	{"snow", "Wireframed"},
	{"telep", "Infinite Falling"},
	{"faces", "Facing Bases"},
	{"island", "Adventure Islands"},
};

static episode_t goodvsbad2episodes[] =
{
	{"Levels? Bevels!", 0, 8},
};

static level_t battlemechlevels[] =
{
	{"start", "Parking Level"},
	{"dm1", "Hot Dump"},                        // 1
	{"dm2", "The Pits"},
	{"dm3", "Dimber Died"},
	{"dm4", "Fire in the Hole"},
	{"dm5", "Clubhouses"},
	{"dm6", "Army go Underground"},
};

static episode_t battlemechepisodes[] =
{
	{"Time for Battle", 0, 7},
};

static level_t openquartzlevels[] =
{
	{"start", "Welcome to Openquartz"},

	{"void1", "The center of nowhere"},                        // 1
	{"void2", "The place with no name"},
	{"void3", "The lost supply base"},
	{"void4", "Past the outer limits"},
	{"void5", "Into the nonexistance"},
	{"void6", "Void walk"},

	{"vtest", "Warp Central"},
	{"box", "The deathmatch box"},
	{"bunkers", "Void command"},
	{"house", "House of chaos"},
	{"office", "Overnight office kill"},
	{"am1", "The nameless chambers"},
};

static episode_t openquartzepisodes[] =
{
	{"Single Player", 0, 1},
	{"Void Deathmatch", 1, 6},
	{"Contrib", 7, 6},
};

static level_t defeatindetail2levels[] =
{
	{"atac3",	"River Crossing"},
	{"atac4",	"Canyon Chaos"},
	{"atac7",	"Desert Stormer"},
};

static episode_t defeatindetail2episodes[] =
{
	{"ATAC Campaign", 0, 3},
};

static gamelevels_t sharewarequakegame = {"Shareware Quake", quakelevels, quakeepisodes, 2};
static gamelevels_t registeredquakegame = {"Quake", quakelevels, quakeepisodes, 7};
static gamelevels_t hipnoticgame = {"Scourge of Armagon", hipnoticlevels, hipnoticepisodes, 6};
static gamelevels_t roguegame = {"Dissolution of Eternity", roguelevels, rogueepisodes, 4};
static gamelevels_t nehahragame = {"Nehahra", nehahralevels, nehahraepisodes, 4};
static gamelevels_t transfusiongame = {"Transfusion", transfusionlevels, transfusionepisodes, 11};
static gamelevels_t goodvsbad2game = {"Good Vs. Bad 2", goodvsbad2levels, goodvsbad2episodes, 1};
static gamelevels_t battlemechgame = {"Battlemech", battlemechlevels, battlemechepisodes, 1};
static gamelevels_t openquartzgame = {"OpenQuartz", openquartzlevels, openquartzepisodes, 3};
static gamelevels_t defeatindetail2game = {"Defeat In Detail 2", defeatindetail2levels, defeatindetail2episodes, 1};

typedef struct gameinfo_s
{
	gamemode_t gameid;
	gamelevels_t *notregistered;
	gamelevels_t *registered;
}
gameinfo_t;

static gameinfo_t gamelist[] =
{
	{GAME_NORMAL, &sharewarequakegame, &registeredquakegame},
	{GAME_HIPNOTIC, &hipnoticgame, &hipnoticgame},
	{GAME_ROGUE, &roguegame, &roguegame},
	{GAME_NEHAHRA, &nehahragame, &nehahragame},
	{GAME_TRANSFUSION, &transfusiongame, &transfusiongame},
	{GAME_GOODVSBAD2, &goodvsbad2game, &goodvsbad2game},
	{GAME_BATTLEMECH, &battlemechgame, &battlemechgame},
	{GAME_OPENQUARTZ, &openquartzgame, &openquartzgame},
	{GAME_DEFEATINDETAIL2, &defeatindetail2game, &defeatindetail2game},
	{(gamemode_t)-1, &sharewarequakegame, &registeredquakegame} // final fallback
};

static gamelevels_t *lookupgameinfo(void)
{
	int i;
	for (i = 0;gamelist[i].gameid >= 0 && gamelist[i].gameid != gamemode;i++);
	if (registered.integer)
		return gamelist[i].registered;
	else
		return gamelist[i].notregistered;
}

static int	startepisode;
static int	startlevel;
static int maxplayers;
static qboolean m_serverInfoMessage = false;
static double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = min(8, MAX_SCOREBOARD);
}


static int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 104, 112, 140, 160, 168};
#define	NUM_GAMEOPTIONS	12
static int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	cachepic_t	*p;
	int		x;
	gamelevels_t *g;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque");
	p = Draw_CachePic ("gfx/p_multi", true);
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi");

	M_DrawTextBox (152, 32, 10, 1);
	M_Print(160, 40, "begin game");

	M_Print(0, 56, "      Max players");
	M_Print(160, 56, va("%i", maxplayers) );

	if (gamemode != GAME_GOODVSBAD2)
	{
		M_Print(0, 64, "        Game Type");
		if (gamemode == GAME_TRANSFUSION)
		{
			if (!coop.integer && !deathmatch.integer)
				Cvar_SetValue("deathmatch", 1);
			if (deathmatch.integer == 0)
				M_Print(160, 64, "Cooperative");
			else if (deathmatch.integer == 2)
				M_Print(160, 64, "Capture the Flag");
			else
				M_Print(160, 64, "Blood Bath");
		}
		else if (gamemode == GAME_BATTLEMECH)
		{
			if (!deathmatch.integer)
				Cvar_SetValue("deathmatch", 1);
			if (deathmatch.integer == 2)
				M_Print(160, 64, "Rambo Match");
			else
				M_Print(160, 64, "Deathmatch");
		}
		else
		{
			if (!coop.integer && !deathmatch.integer)
				Cvar_SetValue("deathmatch", 1);
			if (coop.integer)
				M_Print(160, 64, "Cooperative");
			else
				M_Print(160, 64, "Deathmatch");
		}

		M_Print(0, 72, "        Teamplay");
		if (gamemode == GAME_ROGUE)
		{
			char *msg;

			switch((int)teamplay.integer)
			{
				case 1: msg = "No Friendly Fire"; break;
				case 2: msg = "Friendly Fire"; break;
				case 3: msg = "Tag"; break;
				case 4: msg = "Capture the Flag"; break;
				case 5: msg = "One Flag CTF"; break;
				case 6: msg = "Three Team CTF"; break;
				default: msg = "Off"; break;
			}
			M_Print(160, 72, msg);
		}
		else
		{
			char *msg;

			switch (teamplay.integer)
			{
				case 0: msg = "Off"; break;
				case 2: msg = "Friendly Fire"; break;
				default: msg = "No Friendly Fire"; break;
			}
			M_Print(160, 72, msg);
		}
		M_Print(0, 80, "            Skill");
		if (gamemode == GAME_TRANSFUSION)
		{
			if (skill.integer == 1)
				M_Print(160, 80, "Still Kicking");
			else if (skill.integer == 2)
				M_Print(160, 80, "Pink On The Inside");
			else if (skill.integer == 3)
				M_Print(160, 80, "Lightly Broiled");
			else if (skill.integer == 4)
				M_Print(160, 80, "Well Done");
			else
				M_Print(160, 80, "Extra Crispy");
		}
		else
		{
			if (skill.integer == 0)
				M_Print(160, 80, "Easy difficulty");
			else if (skill.integer == 1)
				M_Print(160, 80, "Normal difficulty");
			else if (skill.integer == 2)
				M_Print(160, 80, "Hard difficulty");
			else
				M_Print(160, 80, "Nightmare difficulty");
		}
		M_Print(0, 88, "       Frag Limit");
		if (fraglimit.integer == 0)
			M_Print(160, 88, "none");
		else
			M_Print(160, 88, va("%i frags", fraglimit.integer));

		M_Print(0, 96, "       Time Limit");
		if (timelimit.integer == 0)
			M_Print(160, 96, "none");
		else
			M_Print(160, 96, va("%i minutes", timelimit.integer));
	}

	M_Print(0, 104, "    Public server");
	M_Print(160, 104, (sv_public.integer == 0) ? "no" : "yes");

	M_Print(0, 112, "   Server maxrate");
	M_Print(160, 112, va("%i", sv_maxrate.integer));

	M_Print(0, 128, "      Server name");
	M_DrawTextBox (0, 132, 38, 1);
	M_Print(8, 140, hostname.string);

	g = lookupgameinfo();

	if (gamemode != GAME_GOODVSBAD2)
	{
		M_Print(0, 160, "         Episode");
		M_Print(160, 160, g->episodes[startepisode].description);
	}

	M_Print(0, 168, "           Level");
	M_Print(160, 168, g->levels[g->episodes[startepisode].firstLevel + startlevel].description);
	M_Print(160, 176, g->levels[g->episodes[startepisode].firstLevel + startlevel].name);

// line cursor
	if (gameoptions_cursor == 9)
		M_DrawCharacter (8 + 8 * strlen(hostname.string), gameoptions_cursor_table[gameoptions_cursor], 10+((int)(realtime*4)&1));
	else
		M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print(x, 146, " More than 255 players??");
			M_Print(x, 154, "  First, question your  ");
			M_Print(x, 162, "   sanity, then email   ");
			M_Print(x, 170, " lordhavoc@ghdigital.com");
		}
		else
			m_serverInfoMessage = false;
	}
}


static void M_NetStart_Change (int dir)
{
	gamelevels_t *g;
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > MAX_SCOREBOARD)
		{
			maxplayers = MAX_SCOREBOARD;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		if (gamemode == GAME_TRANSFUSION)
		{
			switch (deathmatch.integer)
			{
				// From Cooperative to BloodBath
				case 0:
					Cvar_SetValueQuick (&coop, 0);
					Cvar_SetValueQuick (&deathmatch, 1);
					break;

				// From BloodBath to CTF
				case 1:
					Cvar_SetValueQuick (&coop, 0);
					Cvar_SetValueQuick (&deathmatch, 2);
					break;

				// From CTF to Cooperative
				//case 2:
				default:
					Cvar_SetValueQuick (&coop, 1);
					Cvar_SetValueQuick (&deathmatch, 0);
			}
		}
		else if (gamemode == GAME_BATTLEMECH)
		{
			if (deathmatch.integer == 2) // changing from Rambo to Deathmatch
				Cvar_SetValueQuick (&deathmatch, 0);
			else // changing from Deathmatch to Rambo
				Cvar_SetValueQuick (&deathmatch, 2);
		}
		else
		{
			if (deathmatch.integer) // changing from deathmatch to coop
			{
				Cvar_SetValueQuick (&coop, 1);
				Cvar_SetValueQuick (&deathmatch, 0);
			}
			else // changing from coop to deathmatch
			{
				Cvar_SetValueQuick (&coop, 0);
				Cvar_SetValueQuick (&deathmatch, 1);
			}
		}
		break;

	case 3:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		if (gamemode == GAME_ROGUE)
			count = 6;
		else
			count = 2;

		Cvar_SetValueQuick (&teamplay, teamplay.integer + dir);
		if (teamplay.integer > count)
			Cvar_SetValueQuick (&teamplay, 0);
		else if (teamplay.integer < 0)
			Cvar_SetValueQuick (&teamplay, count);
		break;

	case 4:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		Cvar_SetValueQuick (&skill, skill.integer + dir);
		if (gamemode == GAME_TRANSFUSION)
		{
			if (skill.integer > 5)
				Cvar_SetValueQuick (&skill, 1);
			if (skill.integer < 1)
				Cvar_SetValueQuick (&skill, 5);
		}
		else
		{
			if (skill.integer > 3)
				Cvar_SetValueQuick (&skill, 0);
			if (skill.integer < 0)
				Cvar_SetValueQuick (&skill, 3);
		}
		break;

	case 5:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		Cvar_SetValueQuick (&fraglimit, fraglimit.integer + dir*10);
		if (fraglimit.integer > 100)
			Cvar_SetValueQuick (&fraglimit, 0);
		if (fraglimit.integer < 0)
			Cvar_SetValueQuick (&fraglimit, 100);
		break;

	case 6:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		Cvar_SetValueQuick (&timelimit, timelimit.value + dir*5);
		if (timelimit.value > 60)
			Cvar_SetValueQuick (&timelimit, 0);
		if (timelimit.value < 0)
			Cvar_SetValueQuick (&timelimit, 60);
		break;

	case 7:
		Cvar_SetValueQuick (&sv_public, !sv_public.integer);
		break;

	case 8:
		Cvar_SetValueQuick (&sv_maxrate, sv_maxrate.integer + dir*500);
		if (sv_maxrate.integer > NET_MAXRATE)
			Cvar_SetValueQuick (&sv_maxrate, NET_MAXRATE);
		if (sv_maxrate.integer < NET_MINRATE)
			Cvar_SetValueQuick (&sv_maxrate, NET_MINRATE);
		break;

	case 9:
		break;

	case 10:
		if (gamemode == GAME_GOODVSBAD2)
			break;
		startepisode += dir;
		g = lookupgameinfo();

		if (startepisode < 0)
			startepisode = g->numepisodes - 1;

		if (startepisode >= g->numepisodes)
			startepisode = 0;

		startlevel = 0;
		break;

	case 11:
		startlevel += dir;
		g = lookupgameinfo();

		if (startlevel < 0)
			startlevel = g->episodes[startepisode].levels - 1;

		if (startlevel >= g->episodes[startepisode].levels)
			startlevel = 0;
		break;
	}
}

static void M_GameOptions_Key (int key, char ascii)
{
	gamelevels_t *g;
	int l;
	char hostnamebuf[128];

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("sound/misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("sound/misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("sound/misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );

			g = lookupgameinfo();
			Cbuf_AddText ( va ("map %s\n", g->levels[g->episodes[startepisode].firstLevel + startlevel].name) );
			return;
		}

		M_NetStart_Change (1);
		break;

	case K_BACKSPACE:
		if (gameoptions_cursor == 9)
		{
			l = (int)strlen(hostname.string);
			if (l)
			{
				l = min(l - 1, 37);
				memcpy(hostnamebuf, hostname.string, l);
				hostnamebuf[l] = 0;
				Cvar_Set("hostname", hostnamebuf);
			}
		}
		break;

	default:
		if (ascii < 32)
			break;
		if (gameoptions_cursor == 9)
		{
			l = (int)strlen(hostname.string);
			if (l < 37)
			{
				memcpy(hostnamebuf, hostname.string, l);
				hostnamebuf[l] = ascii;
				hostnamebuf[l+1] = 0;
				Cvar_Set("hostname", hostnamebuf);
			}
		}
	}
}

//=============================================================================
/* SLIST MENU */

static int slist_cursor;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	M_Update_Return_Reason("");
	if (lanConfig_cursor == 2)
		Net_SlistQW_f();
	else
		Net_Slist_f();
}


static void M_ServerList_Draw (void)
{
	int n, y, visible, start, end;
	cachepic_t *p;
	const char *s;

	// use as much vertical space as available
	if (gamemode == GAME_TRANSFUSION)
		M_Background(640, vid_conheight.integer - 80);
	else
		M_Background(640, vid_conheight.integer);
	// scroll the list as the cursor moves
	s = va("%i/%i masters %i/%i servers", masterreplycount, masterquerycount, serverreplycount, serverquerycount);
	M_PrintRed((640 - strlen(s) * 8) / 2, 32, s);
	if (*m_return_reason)
		M_Print(16, menu_height - 8, m_return_reason);
	y = 48;
	visible = (int)((menu_height - 16 - y) / 8 / 2);
	start = bound(0, slist_cursor - (visible >> 1), serverlist_viewcount - visible);
	end = min(start + visible, serverlist_viewcount);

	p = Draw_CachePic("gfx/p_multi", true);
	M_DrawPic((640 - p->width) / 2, 4, "gfx/p_multi");
	if (end > start)
	{
		for (n = start;n < end;n++)
		{
			DrawQ_Pic(menu_x, menu_y + y, NULL, 640, 16, n == slist_cursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
			M_PrintColored(0, y, serverlist_viewlist[n]->line1);y += 8;
			M_PrintColored(0, y, serverlist_viewlist[n]->line2);y += 8;
		}
	}
	else if (realtime - masterquerytime > 10)
	{
		if (masterquerycount)
			M_Print(0, y, "No servers found");
		else
			M_Print(0, y, "No master servers found (network problem?)");
	}
	else
	{
		if (serverquerycount)
			M_Print(0, y, "Querying servers");
		else
			M_Print(0, y, "Querying master servers");
	}
}


static void M_ServerList_Key(int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f();
		break;

	case K_SPACE:
		if (lanConfig_cursor == 2)
			Net_SlistQW_f();
		else
			Net_Slist_f();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = serverlist_viewcount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("sound/misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= serverlist_viewcount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound ("sound/misc/menu2.wav");
		if (serverlist_viewcount)
			Cbuf_AddText(va("connect \"%s\"\n", serverlist_viewlist[slist_cursor]->info.cname));
		break;

	default:
		break;
	}

}

//=============================================================================
/* Menu Subsystem */

static void M_KeyEvent(int key, char ascii, qboolean downevent);
static void M_Draw(void);
void M_ToggleMenu_f(void);
static void M_Shutdown(void);

void M_Init (void)
{
	menuplyr_load = true;
	menuplyr_pixels = NULL;

	Cmd_AddCommand ("menu_main", M_Menu_Main_f, "open the main menu");
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f, "open the singleplayer menu");
	Cmd_AddCommand ("menu_load", M_Menu_Load_f, "open the loadgame menu");
	Cmd_AddCommand ("menu_save", M_Menu_Save_f, "open the savegame menu");
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f, "open the multiplayer menu");
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f, "open the player setup menu");
	Cmd_AddCommand ("menu_options", M_Menu_Options_f, "open the options menu");
	Cmd_AddCommand ("menu_options_effects", M_Menu_Options_Effects_f, "open the effects options menu");
	Cmd_AddCommand ("menu_options_graphics", M_Menu_Options_Graphics_f, "open the graphics options menu");
	Cmd_AddCommand ("menu_options_colorcontrol", M_Menu_Options_ColorControl_f, "open the color control menu");
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f, "open the key binding menu");
	Cmd_AddCommand ("menu_video", M_Menu_Video_f, "open the video options menu");
	Cmd_AddCommand ("menu_reset", M_Menu_Reset_f, "open the reset to defaults menu");
	Cmd_AddCommand ("help", M_Menu_Help_f, "open the help menu");
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f, "open the quit menu");
	Cmd_AddCommand ("menu_transfusion_episode", M_Menu_Transfusion_Episode_f, "open the transfusion episode select menu");
	Cmd_AddCommand ("menu_transfusion_skill", M_Menu_Transfusion_Skill_f, "open the transfusion skill select menu");
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f, "open the credits menu");

	if (gamemode == GAME_TRANSFUSION)
	{
		numcommands = sizeof(transfusionbindnames) / sizeof(transfusionbindnames[0]);
		bindnames = transfusionbindnames;
	}
	else if (gamemode == GAME_GOODVSBAD2)
	{
		numcommands = sizeof(goodvsbad2bindnames) / sizeof(goodvsbad2bindnames[0]);
		bindnames = goodvsbad2bindnames;
	}
	else
	{
		numcommands = sizeof(quakebindnames) / sizeof(quakebindnames[0]);
		bindnames = quakebindnames;
	}

	// Make sure "keys_cursor" doesn't start on a section in the binding list
	keys_cursor = 0;
	while (bindnames[keys_cursor][0][0] == '\0')
	{
		keys_cursor++;

		// Only sections? There may be a problem somewhere...
		if (keys_cursor >= numcommands)
			Sys_Error ("M_Init: The key binding list only contains sections");
	}


	if (gamemode == GAME_NEHAHRA)
	{
		if (FS_FileExists("maps/neh1m4.bsp"))
		{
			if (FS_FileExists("hearing.dem"))
			{
				Con_Print("Nehahra movie and game detected.\n");
				NehGameType = TYPE_BOTH;
			}
			else
			{
				Con_Print("Nehahra game detected.\n");
				NehGameType = TYPE_GAME;
			}
		}
		else
		{
			if (FS_FileExists("hearing.dem"))
			{
				Con_Print("Nehahra movie detected.\n");
				NehGameType = TYPE_DEMO;
			}
			else
			{
				Con_Print("Nehahra not found.\n");
				NehGameType = TYPE_GAME; // could just complain, but...
			}
		}
	}
}

void M_Draw (void)
{
	if (key_dest != key_menu)
		m_state = m_none;

	if (m_state == m_none)
		return;

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_demo:
		M_Demo_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_transfusion_episode:
		M_Transfusion_Episode_Draw ();
		break;

	case m_transfusion_skill:
		M_Transfusion_Skill_Draw ();
		break;

	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_options_effects:
		M_Options_Effects_Draw ();
		break;

	case m_options_graphics:
		M_Options_Graphics_Draw ();
		break;

	case m_options_colorcontrol:
		M_Options_ColorControl_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_reset:
		M_Reset_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_credits:
		M_Credits_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

	case m_lanconfig:
		M_LanConfig_Draw ();
		break;

	case m_gameoptions:
		M_GameOptions_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;
	}

	if (gamemode == GAME_TRANSFUSION) {
		if (m_state != m_credits) {
			cachepic_t	*p, *drop1, *drop2, *drop3;
			int g, scale_x, scale_y, scale_y_repeat, top_offset;
			float scale_y_rate;
			scale_y_repeat = vid_conheight.integer * 2;
			g = (int)(realtime * 64)%96;
			scale_y_rate = (float)(g+1) / 96;
			top_offset = (g+12)/12;
			p = Draw_CachePic (va("gfx/menu/blooddrip%i", top_offset), true);
			drop1 = Draw_CachePic("gfx/menu/blooddrop1", true);
			drop2 = Draw_CachePic("gfx/menu/blooddrop2", true);
			drop3 = Draw_CachePic("gfx/menu/blooddrop3", true);
			for (scale_x = 0; scale_x <= vid_conwidth.integer; scale_x += p->width) {
				for (scale_y = -scale_y_repeat; scale_y <= vid_conheight.integer; scale_y += scale_y_repeat) {
					DrawQ_Pic (scale_x + 21, scale_y_repeat * .5 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x +  116, scale_y_repeat + scale_y + scale_y_rate * scale_y_repeat, drop1, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 180, scale_y_repeat * .275 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 242, scale_y_repeat * .75 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 304, scale_y_repeat * .25 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 362, scale_y_repeat * .46125 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 402, scale_y_repeat * .1725 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 438, scale_y_repeat * .9 + scale_y + scale_y_rate * scale_y_repeat, drop1, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 484, scale_y_repeat * .5 + scale_y + scale_y_rate * scale_y_repeat, drop3, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 557, scale_y_repeat * .9425 + scale_y + scale_y_rate * scale_y_repeat, drop1, 0, 0, 1, 1, 1, 1, 0);
					DrawQ_Pic (scale_x + 606, scale_y_repeat * .5 + scale_y + scale_y_rate * scale_y_repeat, drop2, 0, 0, 1, 1, 1, 1, 0);
				}
				DrawQ_Pic (scale_x, -1, Draw_CachePic(va("gfx/menu/blooddrip%i", top_offset), true), 0, 0, 1, 1, 1, 1, 0);
			}
		}
	}

	if (m_entersound)
	{
		S_LocalSound ("sound/misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}


void M_KeyEvent (int key, char ascii, qboolean downevent)
{
	if (!downevent)
		return;
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key, ascii);
		return;

	case m_demo:
		M_Demo_Key (key, ascii);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key, ascii);
		return;

	case m_transfusion_episode:
		M_Transfusion_Episode_Key (key, ascii);
		return;

	case m_transfusion_skill:
		M_Transfusion_Skill_Key (key, ascii);
		return;

	case m_load:
		M_Load_Key (key, ascii);
		return;

	case m_save:
		M_Save_Key (key, ascii);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key, ascii);
		return;

	case m_setup:
		M_Setup_Key (key, ascii);
		return;

	case m_options:
		M_Options_Key (key, ascii);
		return;

	case m_options_effects:
		M_Options_Effects_Key (key, ascii);
		return;

	case m_options_graphics:
		M_Options_Graphics_Key (key, ascii);
		return;

	case m_options_colorcontrol:
		M_Options_ColorControl_Key (key, ascii);
		return;

	case m_keys:
		M_Keys_Key (key, ascii);
		return;

	case m_reset:
		M_Reset_Key (key, ascii);
		return;


	case m_video:
		M_Video_Key (key, ascii);
		return;

	case m_help:
		M_Help_Key (key, ascii);
		return;

	case m_credits:
		M_Credits_Key (key, ascii);
		return;

	case m_quit:
		M_Quit_Key (key, ascii);
		return;

	case m_lanconfig:
		M_LanConfig_Key (key, ascii);
		return;

	case m_gameoptions:
		M_GameOptions_Key (key, ascii);
		return;

	case m_slist:
		M_ServerList_Key (key, ascii);
		return;
	}
}

void M_Shutdown(void)
{
	// reset key_dest
	key_dest = key_game;
}

void M_Restart(void)
{
}

//============================================================================
// Menu prog handling
mfunction_t *PRVM_ED_FindFunction(const char *);

#define M_F_INIT		"m_init"
#define M_F_KEYDOWN		"m_keydown"
#define M_F_KEYUP		"m_keyup"
#define M_F_DRAW		"m_draw"
// normal menu names (rest)
#define M_F_TOGGLE		"m_toggle"
#define M_F_SHUTDOWN	"m_shutdown"

static char *m_required_func[] = {
M_F_INIT,
M_F_KEYDOWN,
M_F_DRAW,
M_F_TOGGLE,
M_F_SHUTDOWN,
};

#ifdef NG_MENU
static qboolean m_displayed;
#endif

static int m_numrequiredfunc = sizeof(m_required_func) / sizeof(char*);

static func_t m_draw, m_keydown;
static mfunction_t *m_keyup;

void MR_SetRouting (qboolean forceold);

void MP_Error(const char *format, ...)
{
	static qboolean processingError = false;
	char errorstring[MAX_INPUTLINE];
	va_list argptr;

	va_start (argptr, format);
	dpvsnprintf (errorstring, sizeof(errorstring), format, argptr);
	va_end (argptr);
	Con_Printf( "Menu_Error: %s\n", errorstring );

	if( !processingError ) {
		processingError = true;
		PRVM_Crash();
		processingError = false;
	} else {
		Con_Printf( "Menu_Error: Recursive call to MP_Error (from PRVM_Crash)!\n" );
	}

	// fall back to the normal menu

	// say it
	Con_Print("Falling back to normal menu\n");

	key_dest = key_game;

	// init the normal menu now -> this will also correct the menu router pointers
	MR_SetRouting (TRUE);

	Host_AbortCurrentFrame();
}

void MP_KeyEvent (int key, char ascii, qboolean downevent)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = realtime;

	// pass key
	prog->globals.generic[OFS_PARM0] = (float) key;
	prog->globals.generic[OFS_PARM1] = (float) ascii;
	if (downevent)
		PRVM_ExecuteProgram(m_keydown, M_F_KEYDOWN"(float key, float ascii) required\n");
	else if (m_keyup)
		PRVM_ExecuteProgram((func_t)(m_keyup - prog->functions), M_F_KEYUP"(float key, float ascii) required\n");

	PRVM_End;
}

void MP_Draw (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = realtime;

	PRVM_ExecuteProgram(m_draw,"");

	PRVM_End;
}

void MP_ToggleMenu_f (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = realtime;

#ifdef NG_MENU
	m_displayed = !m_displayed;
	if( m_displayed )
		PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_DISPLAY) - prog->functions),"");
	else
		PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_HIDE) - prog->functions),"");
#else
	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_TOGGLE) - prog->functions),"");
#endif

	PRVM_End;
}

void MP_Shutdown (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = realtime;

	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_SHUTDOWN) - prog->functions),"");

	// reset key_dest
	key_dest = key_game;

	// AK not using this cause Im not sure whether this is useful at all instead :
	PRVM_ResetProg();

	PRVM_End;
}

void MP_Fallback (void)
{
	MP_Shutdown();

	key_dest = key_game;

	// init the normal menu now -> this will also correct the menu router pointers
	MR_SetRouting (TRUE);
}

void MP_Init (void)
{
	PRVM_Begin;
	PRVM_InitProg(PRVM_MENUPROG);

	prog->headercrc = M_PROGHEADER_CRC;
	prog->edictprivate_size = 0; // no private struct used
	prog->name = M_NAME;
	prog->num_edicts = 1;
	prog->limit_edicts = M_MAX_EDICTS;
	prog->extensionstring = vm_m_extensions;
	prog->builtins = vm_m_builtins;
	prog->numbuiltins = vm_m_numbuiltins;
	prog->init_cmd = VM_M_Cmd_Init;
	prog->reset_cmd = VM_M_Cmd_Reset;
	prog->error_cmd = MP_Error;

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool(M_PROG_FILENAME, 0, NULL);

	PRVM_LoadProgs(M_PROG_FILENAME, m_numrequiredfunc, m_required_func, 0, NULL);

	// set m_draw and m_keydown
	m_draw = (func_t) (PRVM_ED_FindFunction(M_F_DRAW) - prog->functions);
	m_keydown = (func_t) (PRVM_ED_FindFunction(M_F_KEYDOWN) - prog->functions);
	m_keyup = PRVM_ED_FindFunction(M_F_KEYUP);

#ifdef NG_MENU
	m_displayed = false;
#endif

	// set time
	*prog->time = realtime;

	// call the prog init
	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_INIT) - prog->functions),"");

	PRVM_End;
}

void MP_Restart(void)
{
	MP_Init();
}

//============================================================================
// Menu router

static cvar_t forceqmenu = { 0, "forceqmenu", "0", "enables the quake menu instead of the quakec menu.dat (if present)" };

void (*MR_KeyEvent) (int key, char ascii, qboolean downevent);
void (*MR_Draw) (void);
void (*MR_ToggleMenu_f) (void);
void (*MR_Shutdown) (void);

void MR_SetRouting(qboolean forceold)
{
	static qboolean m_init = FALSE, mp_init = FALSE;

	// if the menu prog isnt available or forceqmenu ist set, use the old menu
	if(!FS_FileExists(M_PROG_FILENAME) || forceqmenu.integer || forceold)
	{
		// set menu router function pointers
		MR_KeyEvent = M_KeyEvent;
		MR_Draw = M_Draw;
		MR_ToggleMenu_f = M_ToggleMenu_f;
		MR_Shutdown = M_Shutdown;

		// init
		if(!m_init)
		{
			M_Init();
			m_init = TRUE;
		}
		else
			M_Restart();
	}
	else
	{
		// set menu router function pointers
		MR_KeyEvent = MP_KeyEvent;
		MR_Draw = MP_Draw;
		MR_ToggleMenu_f = MP_ToggleMenu_f;
		MR_Shutdown = MP_Shutdown;

		if(!mp_init)
		{
			MP_Init();
			mp_init = TRUE;
		}
		else
			MP_Restart();
	}
}

void MR_Restart(void)
{
	MR_Shutdown ();
	MR_SetRouting (FALSE);
}

void Call_MR_ToggleMenu_f(void)
{
	Host_StartVideo();
	if(MR_ToggleMenu_f)
		MR_ToggleMenu_f();
}

void MR_Init_Commands(void)
{
	// set router console commands
	Cvar_RegisterVariable (&forceqmenu);
	Cvar_RegisterVariable (&menu_options_colorcontrol_correctionvalue);
	if (gamemode == GAME_NETHERWORLD)
		Cmd_AddCommand ("menu_fallback", MP_Fallback, "switch to engine menu (unload menu.dat)"); //Force to old-style menu
	Cmd_AddCommand ("menu_restart",MR_Restart, "restart menu system (reloads menu.dat");
	Cmd_AddCommand ("togglemenu", Call_MR_ToggleMenu_f, "opens or closes menu");
}

void MR_Init(void)
{
	// use -forceqmenu to use always the normal quake menu (it sets forceqmenu to 1)
// COMMANDLINEOPTION: Client: -forceqmenu disables menu.dat (same as +forceqmenu 1)
	if(COM_CheckParm("-forceqmenu"))
		Cvar_SetValueQuick(&forceqmenu,1);
	// use -useqmenu for debugging proposes, cause it starts
	// the normal quake menu only the first time
// COMMANDLINEOPTION: Client: -useqmenu causes the first time you open the menu to use the quake menu, then reverts to menu.dat (if forceqmenu is 0)
	if(COM_CheckParm("-useqmenu"))
		MR_SetRouting (TRUE);
	else
		MR_SetRouting (FALSE);
}
