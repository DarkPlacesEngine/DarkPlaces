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
#include "image.h"

#include "mprogdefs.h"

#define TYPE_DEMO 1
#define TYPE_GAME 2
#define TYPE_BOTH 3

mempool_t *menu_mempool;

int NehGameType;

enum m_state_e m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
	void M_Menu_Options_f (void);
	void M_Menu_Options_Effects_f (void);
	void M_Menu_Options_Graphics_f (void);
	void M_Menu_Options_ColorControl_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_ServerList_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
	void M_Options_Draw (void);
	void M_Options_Effects_Draw (void);
	void M_Options_Graphics_Draw (void);
	void M_Options_ColorControl_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_ServerList_Draw (void);

void M_Main_Key (int key, char ascii);
	void M_SinglePlayer_Key (int key, char ascii);
		void M_Load_Key (int key, char ascii);
		void M_Save_Key (int key, char ascii);
	void M_MultiPlayer_Key (int key, char ascii);
		void M_Setup_Key (int key, char ascii);
	void M_Options_Key (int key, char ascii);
	void M_Options_Effects_Key (int key, char ascii);
	void M_Options_Graphics_Key (int key, char ascii);
	void M_Options_ColorControl_Key (int key, char ascii);
		void M_Keys_Key (int key, char ascii);
		void M_Video_Key (int key, char ascii);
	void M_Help_Key (int key, char ascii);
	void M_Quit_Key (int key, char ascii);
void M_LanConfig_Key (int key, char ascii);
void M_GameOptions_Key (int key, char ascii);
void M_ServerList_Key (int key, char ascii);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)

// Nehahra
#define NumberOfNehahraDemos 34
typedef struct
{
	char *name;
	char *desc;
} nehahrademonames_t;

nehahrademonames_t NehahraDemos[NumberOfNehahraDemos] =
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

float menu_x, menu_y, menu_width, menu_height;

void M_Background(int width, int height)
{
	menu_width = width;
	menu_height = height;
	menu_x = (vid.conwidth - menu_width) * 0.5;
	menu_y = (vid.conheight - menu_height) * 0.5;
	//DrawQ_Fill(menu_x, menu_y, menu_width, menu_height, 0, 0, 0, 0.5, 0);
	DrawQ_Fill(0, 0, vid.conwidth, vid.conheight, 0, 0, 0, 0.5, 0);
}

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (float cx, float cy, int num)
{
	char temp[2];
	temp[0] = num;
	temp[1] = 0;
	DrawQ_String(menu_x + cx, menu_y + cy, temp, 1, 8, 8, 1, 1, 1, 1, 0);
}

void M_Print(float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
}

void M_PrintRed (float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 0, 0, 1, 0);
}

void M_ItemPrint(float cx, float cy, char *str, int unghosted)
{
	if (unghosted)
		DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
	else
		DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 0.4, 0.4, 0.4, 1, 0);
}

void M_DrawPic (float cx, float cy, char *picname)
{
	DrawQ_Pic (menu_x + cx, menu_y + cy, picname, 0, 0, 1, 1, 1, 1, 0);
}

qbyte identityTable[256];
qbyte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int j;
	qbyte *dest, *source;

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


void M_DrawTextBox (float x, float y, float width, float height)
{
	int n;
	float cx, cy;

	// draw left side
	cx = x;
	cy = y;
	M_DrawPic (cx, cy, "gfx/box_tl.lmp");
	for (n = 0; n < height; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, "gfx/box_ml.lmp");
	}
	M_DrawPic (cx, cy+8, "gfx/box_bl.lmp");

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawPic (cx, cy, "gfx/box_tm.lmp");
		for (n = 0; n < height; n++)
		{
			cy += 8;
			if (n >= 1)
				M_DrawPic (cx, cy, "gfx/box_mm2.lmp");
			else
				M_DrawPic (cx, cy, "gfx/box_mm.lmp");
		}
		M_DrawPic (cx, cy+8, "gfx/box_bm.lmp");
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	M_DrawPic (cx, cy, "gfx/box_tr.lmp");
	for (n = 0; n < height; n++)
	{
		cy += 8;
		M_DrawPic (cx, cy, "gfx/box_mr.lmp");
	}
	M_DrawPic (cx, cy+8, "gfx/box_br.lmp");
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
		M_Menu_Main_f ();
	else
	{
		key_dest = key_game;
		m_state = m_none;
	}
}


int demo_cursor;
void M_Demo_Draw (void)
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

void M_Demo_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav", true);
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va ("playdemo %s\n", NehahraDemos[demo_cursor].name));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		demo_cursor--;
		if (demo_cursor < 0)
			demo_cursor = NumberOfNehahraDemos-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		demo_cursor++;
		if (demo_cursor >= NumberOfNehahraDemos)
			demo_cursor = 0;
		break;
	}
}

//=============================================================================
/* MAIN MENU */

int	m_main_cursor;

int MAIN_ITEMS = 4; // Nehahra: Menu Disable

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


void M_Main_Draw (void)
{
	int		f;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_main.lmp");
// Nehahra
	if (gamemode == GAME_NEHAHRA)
	{
		if (NehGameType == TYPE_BOTH)
			M_DrawPic (72, 32, "gfx/mainmenu.lmp");
		else if (NehGameType == TYPE_GAME)
			M_DrawPic (72, 32, "gfx/gamemenu.lmp");
		else
			M_DrawPic (72, 32, "gfx/demomenu.lmp");
	}
	else
		M_DrawPic (72, 32, "gfx/mainmenu.lmp");

	f = (int)(realtime * 10)%6;

	M_DrawPic (54, 32 + m_main_cursor * 20, va("gfx/menudot%i.lmp", f+1));
}


void M_Main_Key (int key, char ascii)
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
		S_LocalSound ("misc/menu1.wav", true);
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav", true);
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

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3


void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


void M_SinglePlayer_Draw (void)
{
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");

	// Transfusion doesn't have a single player mode
	if (gamemode == GAME_TRANSFUSION || gamemode == GAME_NEXUIZ || gamemode == GAME_GOODVSBAD2 || gamemode == GAME_BATTLEMECH)
	{
		M_DrawPic ((320 - p->width) / 2, 4, "gfx/ttl_sgl.lmp");

		M_DrawTextBox (60, 8 * 8, 23, 4);
		if (gamemode == GAME_NEXUIZ)
			M_Print(95, 10 * 8, "Nexuiz is for");
		else if (gamemode == GAME_GOODVSBAD2)
			M_Print(95, 10 * 8, "Good Vs Bad 2 is for");
		else if (gamemode == GAME_BATTLEMECH)
			M_Print(95, 10 * 8, "Battlemech is for");
		else
			M_Print(95, 10 * 8, "Transfusion is for");
		M_Print(83, 11 * 8, "multiplayer play only");
	}
	else
	{
		int		f;

		M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_sgl.lmp");
		M_DrawPic (72, 32, "gfx/sp_menu.lmp");

		f = (int)(realtime * 10)%6;

		M_DrawPic (54, 32 + m_singleplayer_cursor * 20, va("gfx/menudot%i.lmp", f+1));
	}
}


void M_SinglePlayer_Key (int key, char ascii)
{
	if (gamemode == GAME_TRANSFUSION || gamemode == GAME_NEXUIZ || gamemode == GAME_GOODVSBAD2 || gamemode == GAME_BATTLEMECH)
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
		S_LocalSound ("misc/menu1.wav", true);
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav", true);
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
			if (gamemode == GAME_NEHAHRA)
				Cbuf_AddText ("map nehstart\n");
			else
				Cbuf_AddText ("map start\n");
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

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int		i, j;
	char	name[MAX_OSPATH];
	char	*str;
	qfile_t	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "s%i.sav", i);
		f = FS_Open (name, "r", false);
		if (!f)
			continue;
		str = FS_Getline (f);
		sscanf (str, "%i\n", &version);
		str = FS_Getline (f);
		strlcpy (m_filenames[i], str, sizeof (m_filenames[i]));

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


void M_Load_Draw (void)
{
	int		i;
	cachepic_t	*p;

	M_Background(320, 200);

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_load.lmp");

	for (i=0 ; i< MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	cachepic_t	*p;

	M_Background(320, 200);

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_save.lmp");

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print(16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav", true);
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		// issue the load command
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


void M_Save_Key (int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav", true);
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


void M_MultiPlayer_Draw (void)
{
	int		f;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");
	M_DrawPic (72, 32, "gfx/mp_menu.lmp");

	f = (int)(realtime * 10)%6;

	M_DrawPic (54, 32 + m_multiplayer_cursor * 20, va("gfx/menudot%i.lmp", f+1));
}


void M_MultiPlayer_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav", true);
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

int		setup_cursor = 4;
int		setup_cursor_table[] = {40, 64, 88, 124, 140};

char	setup_myname[32];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;
int		setup_rate;
int		setup_oldrate;

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
static qbyte *menuplyr_pixels;
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

void M_Setup_Draw (void)
{
	int i;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");

	M_Print(64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print(168, 40, setup_myname);

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
		qbyte *data, *f;
		menuplyr_load = false;
		menuplyr_top = -1;
		menuplyr_bottom = -1;
		if ((f = FS_LoadFile("gfx/menuplyr.lmp", tempmempool, true)))
		{
			data = LoadLMPAs8Bit (f, 0, 0);
			menuplyr_width = image_width;
			menuplyr_height = image_height;
			Mem_Free(f);
			menuplyr_pixels = Mem_Alloc(menu_mempool, menuplyr_width * menuplyr_height);
			menuplyr_translated = Mem_Alloc(menu_mempool, menuplyr_width * menuplyr_height * 4);
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
				menuplyr_translated[i] = palette_complete[translationTable[menuplyr_pixels[i]]];
			Draw_NewPic("gfx/menuplyr.lmp", menuplyr_width, menuplyr_height, true, (qbyte *)menuplyr_translated);
		}
		M_DrawPic(160, 48, "gfx/bigbox.lmp");
		M_DrawPic(172, 56, "gfx/menuplyr.lmp");
	}

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
	else
		M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));
}


void M_Setup_Key (int k, char ascii)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav", true);
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 1)
			return;
		S_LocalSound ("misc/menu3.wav", true);
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
		S_LocalSound ("misc/menu3.wav", true);
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
		if (ascii < 32 || ascii > 126)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_myname);
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

void M_DrawSlider (int x, int y, float num, float rangemin, float rangemax)
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

void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print(x, y, "on");
	else
		M_Print(x, y, "off");
}


#define OPTIONS_ITEMS 37

int options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

extern cvar_t snd_staticvolume;
extern cvar_t slowmo;
extern dllhandle_t jpeg_dll;
extern cvar_t gl_texture_anisotropy;

void M_Menu_Options_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("misc/menu3.wav", true);

	optnum = 7;
	if (options_cursor == optnum++)
		Cvar_SetValueQuick (&vid_conwidth, bound(320, vid_conwidth.value + dir * 64, 2048));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&vid_conheight, bound(240, vid_conheight.value + dir * 48, 1536));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_conspeed, bound(0, scr_conspeed.value + dir * 100, 1000));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_conalpha, bound(0, scr_conalpha.value + dir * 0.2, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_conbrightness, bound(0, scr_conbrightness.value + dir * 0.2, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_viewsize, bound(30, scr_viewsize.value + dir * 10, 120));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_fov, bound(1, scr_fov.integer + dir * 1, 170));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_screenshot_jpeg, !scr_screenshot_jpeg.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&scr_screenshot_jpeg_quality, bound(0, scr_screenshot_jpeg_quality.value + dir * 0.1, 1));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&r_sky, !r_sky.integer);
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&gl_combine, !gl_combine.integer);
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
		Cvar_SetValueQuick (&crosshair, bound(0, crosshair.integer + dir, 5));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&crosshair_size, bound(1, crosshair_size.value + dir, 5));
	else if (options_cursor == optnum++)
		Cvar_SetValueQuick (&crosshair_static, !crosshair_static.integer);
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

int optnum;
int opty;
int optcursor;

void M_Options_PrintCommand(char *s, int enabled)
{
	if (opty >= 32)
	{
		DrawQ_Fill(menu_x, menu_y + opty, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
	}
	opty += 8;
	optnum++;
}

void M_Options_PrintCheckbox(char *s, int enabled, int yes)
{
	if (opty >= 32)
	{
		DrawQ_Fill(menu_x, menu_y + opty, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
		M_DrawCheckbox(0 + strlen(s) * 8 + 8, opty, yes);
	}
	opty += 8;
	optnum++;
}

void M_Options_PrintSlider(char *s, int enabled, float value, float minvalue, float maxvalue)
{
	if (opty >= 32)
	{
		DrawQ_Fill(menu_x, menu_y + opty, 320, 8, optnum == optcursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
		M_ItemPrint(0, opty, s, enabled);
		M_DrawSlider(0 + strlen(s) * 8 + 8, opty, value, minvalue, maxvalue);
	}
	opty += 8;
	optnum++;
}

void M_Options_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, 240);

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	optnum = 0;
	optcursor = options_cursor;
	visible = (vid.conheight - 32) / 8;
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
	M_Options_PrintSlider(  "     Console Speed", true, scr_conspeed.value, 0, 1000);
	M_Options_PrintSlider(  "     Console Alpha", true, scr_conalpha.value, 0, 1);
	M_Options_PrintSlider(  "Conback Brightness", true, scr_conbrightness.value, 0, 1);
	M_Options_PrintSlider(  "       Screen size", true, scr_viewsize.value, 30, 120);
	M_Options_PrintSlider(  "     Field of View", true, scr_fov.integer, 1, 170);
	M_Options_PrintCheckbox("  JPEG screenshots", jpeg_dll != NULL, scr_screenshot_jpeg.integer);
	M_Options_PrintSlider(  "      JPEG quality", jpeg_dll != NULL, scr_screenshot_jpeg_quality.value, 0, 1);
	M_Options_PrintCheckbox("               Sky", true, r_sky.integer);
	M_Options_PrintCheckbox("   Texture Combine", true, gl_combine.integer);
	M_Options_PrintCheckbox("         Dithering", true, gl_dither.integer);
	M_Options_PrintSlider(  "Anisotropic Filter", gl_support_anisotropy, gl_texture_anisotropy.integer, 1, gl_max_anisotropy);
	M_Options_PrintSlider(  "        Game Speed", sv.active, slowmo.value, 0, 5);
	M_Options_PrintSlider(  "   CD Music Volume", cdaudioinitialized.integer, bgmvolume.value, 0, 1);
	M_Options_PrintSlider(  "      Sound Volume", snd_initialized.integer, volume.value, 0, 1);
	M_Options_PrintSlider(gamemode == GAME_GOODVSBAD2 ? "      Music Volume" : "    Ambient Volume", snd_initialized.integer, snd_staticvolume.value, 0, 1);
	M_Options_PrintSlider(  "         Crosshair", true, crosshair.value, 0, 5);
	M_Options_PrintSlider(  "    Crosshair Size", true, crosshair_size.value, 1, 5);
	M_Options_PrintCheckbox("  Static Crosshair", true, crosshair_static.integer);
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


void M_Options_Key (int k, char ascii)
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
			Cbuf_AddText ("exec default.cfg\n");
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
		S_LocalSound ("misc/menu1.wav", true);
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
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

#define	OPTIONS_EFFECTS_ITEMS	33

int options_effects_cursor;

void M_Menu_Options_Effects_f (void)
{
	key_dest = key_menu;
	m_state = m_options_effects;
	m_entersound = true;
}


extern cvar_t r_detailtextures;
extern cvar_t cl_particles;
extern cvar_t cl_explosions;
extern cvar_t cl_stainmaps;
extern cvar_t cl_decals;
extern cvar_t r_explosionclip;
extern cvar_t r_modellights;
extern cvar_t r_coronas;
extern cvar_t gl_flashblend;
extern cvar_t cl_particles_quality;
extern cvar_t cl_particles_bulletimpacts;
extern cvar_t cl_particles_smoke;
extern cvar_t cl_particles_sparks;
extern cvar_t cl_particles_bubbles;
extern cvar_t cl_particles_blood;
extern cvar_t cl_particles_blood_alpha;
extern cvar_t cl_particles_blood_bloodhack;
extern cvar_t r_lightningbeam_thickness;
extern cvar_t r_lightningbeam_scroll;
extern cvar_t r_lightningbeam_repeatdistance;
extern cvar_t r_lightningbeam_color_red;
extern cvar_t r_lightningbeam_color_green;
extern cvar_t r_lightningbeam_color_blue;
extern cvar_t r_lightningbeam_qmbtexture;

void M_Menu_Options_Effects_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("misc/menu3.wav", true);

	optnum = 0;
	     if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_modellights, bound(0, r_modellights.value + dir, 8));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_coronas, bound(0, r_coronas.value + dir * 0.125, 4));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&gl_flashblend, !gl_flashblend.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles, !cl_particles.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_quality, bound(1, cl_particles_quality.value + dir * 0.5, 4));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_explosions, !cl_explosions.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_explosionclip, !r_explosionclip.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_stainmaps, !cl_stainmaps.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_decals, !cl_decals.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_detailtextures, !r_detailtextures.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_bulletimpacts, !cl_particles_bulletimpacts.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_smoke, !cl_particles_smoke.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_sparks, !cl_particles_sparks.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_bubbles, !cl_particles_bubbles.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood, !cl_particles_blood.integer);
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood_alpha, bound(0.2, cl_particles_blood_alpha.value + dir * 0.1, 1));
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&cl_particles_blood_bloodhack, !cl_particles_blood_bloodhack.integer);
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
	else if (options_effects_cursor == optnum++) Cvar_SetValueQuick (&r_watershader, bound(0, r_watershader.value + dir * 0.25, 10));
}

void M_Options_Effects_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	optcursor = options_effects_cursor;
	optnum = 0;
	visible = (vid.conheight - 32) / 8;
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_EFFECTS_ITEMS - visible)) * 8;

	M_Options_PrintSlider(  "      Lights Per Model", true, r_modellights.value, 0, 8);
	M_Options_PrintSlider(  "      Corona Intensity", true, r_coronas.value, 0, 4);
	M_Options_PrintCheckbox("      Use Only Coronas", true, gl_flashblend.integer);
	M_Options_PrintCheckbox("             Particles", true, cl_particles.integer);
	M_Options_PrintSlider(  "     Particles Quality", true, cl_particles_quality.value, 1, 4);
	M_Options_PrintCheckbox("            Explosions", true, cl_explosions.integer);
	M_Options_PrintCheckbox("    Explosion Clipping", true, r_explosionclip.integer);
	M_Options_PrintCheckbox("             Stainmaps", true, cl_stainmaps.integer);
	M_Options_PrintCheckbox("                Decals", true, cl_decals.integer);
	M_Options_PrintCheckbox("      Detail Texturing", true, r_detailtextures.integer);
	M_Options_PrintCheckbox("        Bullet Impacts", true, cl_particles_bulletimpacts.integer);
	M_Options_PrintCheckbox("                 Smoke", true, cl_particles_smoke.integer);
	M_Options_PrintCheckbox("                Sparks", true, cl_particles_sparks.integer);
	M_Options_PrintCheckbox("               Bubbles", true, cl_particles_bubbles.integer);
	M_Options_PrintCheckbox("                 Blood", true, cl_particles_blood.integer);
	M_Options_PrintSlider(  "         Blood Opacity", true, cl_particles_blood_alpha.value, 0.2, 1);
	M_Options_PrintCheckbox("Force New Blood Effect", true, cl_particles_blood_bloodhack.integer);
	M_Options_PrintSlider(  "   Lightning Thickness", true, r_lightningbeam_thickness.integer, 1, 10);
	M_Options_PrintSlider(  "      Lightning Scroll", true, r_lightningbeam_scroll.integer, 0, 10);
	M_Options_PrintSlider(  " Lightning Repeat Dist", true, r_lightningbeam_repeatdistance.integer, 64, 1024);
	M_Options_PrintSlider(  "   Lightning Color Red", true, r_lightningbeam_color_red.value, 0, 1);
	M_Options_PrintSlider(  " Lightning Color Green", true, r_lightningbeam_color_green.value, 0, 1);
	M_Options_PrintSlider(  "  Lightning Color Blue", true, r_lightningbeam_color_blue.value, 0, 1);
	M_Options_PrintCheckbox(" Lightning QMB Texture", true, r_lightningbeam_qmbtexture.integer);
	M_Options_PrintCheckbox("   Model Interpolation", true, r_lerpmodels.integer);
	M_Options_PrintCheckbox("  Sprite Interpolation", true, r_lerpsprites.integer);
	M_Options_PrintSlider(  "            View Blend", true, gl_polyblend.value, 0, 1);
	M_Options_PrintSlider(  "Upper Sky Scroll Speed", true, r_skyscroll1.value, -8, 8);
	M_Options_PrintSlider(  "Lower Sky Scroll Speed", true, r_skyscroll2.value, -8, 8);
	M_Options_PrintSlider(  "  Underwater View Warp", true, r_waterwarp.value, 0, 1);
	M_Options_PrintSlider(  " Water Alpha (opacity)", true, r_wateralpha.value, 0, 1);
	M_Options_PrintSlider(  "        Water Movement", true, r_waterscroll.value, 0, 10);
	M_Options_PrintSlider(  " GeForce3 Water Shader", true, r_watershader.value, 0, 10);
}


void M_Options_Effects_Key (int k, char ascii)
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
		S_LocalSound ("misc/menu1.wav", true);
		options_effects_cursor--;
		if (options_effects_cursor < 0)
			options_effects_cursor = OPTIONS_EFFECTS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
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


#define	OPTIONS_GRAPHICS_ITEMS	7

int options_graphics_cursor;

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

void M_Menu_Options_Graphics_AdjustSliders (int dir)
{
	int optnum;
	S_LocalSound ("misc/menu3.wav", true);
 
	optnum = 0;

		 if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_gloss,							bound(0, r_shadow_gloss.integer + dir, 2));
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_dlight,				!r_shadow_realtime_dlight.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_dlight_shadows,		!r_shadow_realtime_dlight_shadows.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world,					!r_shadow_realtime_world.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_dlightshadows,	!r_shadow_realtime_world_dlightshadows.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_lightmaps,		!r_shadow_realtime_world_lightmaps.integer);
	else if (options_graphics_cursor == optnum++) Cvar_SetValueQuick (&r_shadow_realtime_world_shadows,			!r_shadow_realtime_world_shadows.integer);
}


void M_Options_Graphics_Draw (void)
{
	int visible;
	cachepic_t	*p;

	M_Background(320, 200);

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	optcursor = options_graphics_cursor;
	optnum = 0;
	visible = (vid.conheight - 32) / 8;
	opty = 32 - bound(0, optcursor - (visible >> 1), max(0, OPTIONS_GRAPHICS_ITEMS - visible)) * 8;

	M_Options_PrintSlider(  "             Gloss Mode", true, r_shadow_gloss.integer, 0, 2);
	M_Options_PrintCheckbox("             RT DLights", true, r_shadow_realtime_dlight.integer);
	M_Options_PrintCheckbox("      RT DLight Shadows", true, r_shadow_realtime_dlight_shadows.integer);
	M_Options_PrintCheckbox("               RT World", true, r_shadow_realtime_world.integer);
	M_Options_PrintCheckbox("RT World DLight Shadows", true, r_shadow_realtime_world_dlightshadows.integer);
	M_Options_PrintCheckbox("     RT World Lightmaps", true, r_shadow_realtime_world_lightmaps.integer);
	M_Options_PrintCheckbox("        RT World Shadow", true, r_shadow_realtime_world_shadows.integer);
}


void M_Options_Graphics_Key (int k, char ascii)
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
		S_LocalSound ("misc/menu1.wav", true);
		options_graphics_cursor--;
		if (options_graphics_cursor < 0)
			options_graphics_cursor = OPTIONS_GRAPHICS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
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

int		options_colorcontrol_cursor;

// intensity value to match up to 50% dither to 'correct' quake
cvar_t menu_options_colorcontrol_correctionvalue = {0, "menu_options_colorcontrol_correctionvalue", "0.25"};

void M_Menu_Options_ColorControl_f (void)
{
	key_dest = key_menu;
	m_state = m_options_colorcontrol;
	m_entersound = true;
}


void M_Menu_Options_ColorControl_AdjustSliders (int dir)
{
	int optnum;
	float f;
	S_LocalSound ("misc/menu3.wav", true);

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

void M_Options_ColorControl_Draw (void)
{
	int visible;
	float x, c, s, t, u, v;
	cachepic_t	*p;

	M_Background(320, 256);

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	optcursor = options_colorcontrol_cursor;
	optnum = 0;
	visible = (vid.conheight - 32) / 8;
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
	DrawQ_Fill(menu_x, menu_y + opty, 320, 4 + 64 + 8 + 64 + 4, 0, 0, 0, 1, 0);opty += 4;
	s = (float) 312 / 2 * vid.realwidth / vid.conwidth;
	t = (float) 4 / 2 * vid.realheight / vid.conheight;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 1,0,0,1, 0,1, 0,0,0,1, 1,1, 1,0,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 0,1,0,1, 0,1, 0,0,0,1, 1,1, 0,1,0,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 0,0,1,1, 0,1, 0,0,0,1, 1,1, 0,0,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);opty += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + opty, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 1,1,1,1, 0,1, 0,0,0,1, 1,1, 1,1,1,1, 0);opty += 4;

	c = menu_options_colorcontrol_correctionvalue.value; // intensity value that should be matched up to a 50% dither to 'correct' quake
	s = (float) 48 / 2 * vid.realwidth / vid.conwidth;
	t = (float) 48 / 2 * vid.realheight / vid.conheight;
	u = s * 0.5;
	v = t * 0.5;
	opty += 8;
	x = 4;
	DrawQ_Fill(menu_x + x, menu_y + opty, 64, 48, c, 0, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,0,0,1, u,0, 1,0,0,1, 0,v, 1,0,0,1, u,v, 1,0,0,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + opty, 64, 48, 0, c, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,1,0,1, u,0, 0,1,0,1, 0,v, 0,1,0,1, u,v, 0,1,0,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + opty, 64, 48, 0, 0, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,0,1,1, u,0, 0,0,1,1, 0,v, 0,0,1,1, u,v, 0,0,1,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + opty, 64, 48, c, c, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + opty + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,1,1,1, u,0, 1,1,1,1, 0,v, 1,1,1,1, u,v, 1,1,1,1, 0);
}


void M_Options_ColorControl_Key (int k, char ascii)
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
		S_LocalSound ("misc/menu1.wav", true);
		options_colorcontrol_cursor--;
		if (options_colorcontrol_cursor < 0)
			options_colorcontrol_cursor = OPTIONS_COLORCONTROL_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
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

char *quakebindnames[][2] =
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

char *transfusionbindnames[][2] =
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

char *goodvsbad2bindnames[][2] =
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

int numcommands;
char *(*bindnames)[2];

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

bindcategory_t *bindcategories = NULL;

void M_ClearBinds (void)
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

void M_AddBindToCategory(bindcategory_t *c, char *command, char *description)
{
	for (b = &c->binds;*b;*b = &(*b)->next);
	*b = Z_Alloc(sizeof(binditem_t) + strlen(command) + 1 + strlen(description) + 1);
	*b->command = (char *)((*b) + 1);
	*b->description = *b->command + strlen(command) + 1;
	strcpy(*b->command, command);
	strcpy(*b->description, description);
}

void M_AddBind (char *category, char *command, char *description)
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

void M_DefaultBinds (void)
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


int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

#define NUMKEYS 5

void M_FindKeysForCommand (char *command, int *keys)
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

void M_UnbindCommand (char *command)
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


void M_Keys_Draw (void)
{
	int		i, j;
	int		keys[NUMKEYS];
	int		y;
	cachepic_t	*p;
	char	keystring[1024];

	M_Background(320, 48 + 8 * numcommands);

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_cstm.lmp");

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


void M_Keys_Key (int k, char ascii)
{
	char	cmd[80];
	int		keys[NUMKEYS];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav", true);
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
		S_LocalSound ("misc/menu1.wav", true);
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
		S_LocalSound ("misc/menu1.wav", true);
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
		S_LocalSound ("misc/menu2.wav", true);
		if (keys[NUMKEYS - 1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav", true);
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

#define VIDEO_ITEMS 4

int video_cursor = 0;
int video_cursor_table[] = {56, 68, 80, 100};
// note: if modes are added to the beginning of this list, update the
// video_resolution = x; in M_Menu_Video_f below
unsigned short video_resolutions[][2] = {{320,240}, {400,300}, {512,384}, {640,480}, {800,600}, {1024,768}, {1152,864}, {1280,960}, {1280,1024}, {1600,1200}, {1792,1344}, {1920,1440}, {2048,1536}, {0,0}};
// this is the number of the 640x480 mode in the list
#define VID_640 3
#define VID_RES_COUNT ((int)(sizeof(video_resolutions) / sizeof(video_resolutions[0])) - 1)
int video_resolution;

extern int current_vid_fullscreen;
extern int current_vid_width;
extern int current_vid_height;
extern int current_vid_bitsperpixel;


void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	// Look for the current resolution
	for (video_resolution = 0; video_resolution < VID_RES_COUNT; video_resolution++)
	{
		if (video_resolutions[video_resolution][0] == current_vid_width &&
			video_resolutions[video_resolution][1] == current_vid_height)
			break;
	}

	// Default to VID_640 if we didn't find it
	if (video_resolution == VID_RES_COUNT)
	{
		// may need to update this number if mode list changes
		video_resolution = VID_640;
		Cvar_SetValueQuick (&vid_width, video_resolutions[video_resolution][0]);
		Cvar_SetValueQuick (&vid_height, video_resolutions[video_resolution][1]);
	}
}


void M_Video_Draw (void)
{
	cachepic_t	*p;
	const char* string;

	M_Background(320, 200);

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/vidmodes.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/vidmodes.lmp");

	// Resolution
	M_Print(16, video_cursor_table[0], "            Resolution");
	string = va("%dx%d", video_resolutions[video_resolution][0], video_resolutions[video_resolution][1]);
	M_Print(220, video_cursor_table[0], string);

	// Bits per pixel
	M_Print(16, video_cursor_table[1], "        Bits per pixel");
	M_Print(220, video_cursor_table[1], (vid_bitsperpixel.integer == 32) ? "32" : "16");

	// Fullscreen
	M_Print(16, video_cursor_table[2], "            Fullscreen");
	M_DrawCheckbox(220, video_cursor_table[2], vid_fullscreen.integer);

	// "Apply" button
	M_Print(220, video_cursor_table[3], "Apply");

	// Cursor
	M_DrawCharacter(200, video_cursor_table[video_cursor], 12+((int)(realtime*4)&1));
}


void M_Menu_Video_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav", true);

	switch (video_cursor)
	{
		// Resolution
		case 0:
		{
			int new_resolution = video_resolution + dir;
			if (gamemode == GAME_FNIGGIUM ? new_resolution < VID_640 : new_resolution < 0)
				video_resolution = VID_RES_COUNT - 1;
			else if (new_resolution > VID_RES_COUNT - 1)
				video_resolution = gamemode == GAME_FNIGGIUM ? VID_640 : 0;
			else
				video_resolution = new_resolution;

			Cvar_SetValueQuick (&vid_width, video_resolutions[video_resolution][0]);
			Cvar_SetValueQuick (&vid_height, video_resolutions[video_resolution][1]);
			break;
		}

		// Bits per pixel
		case 1:
			Cvar_SetValueQuick (&vid_bitsperpixel, (vid_bitsperpixel.integer == 32) ? 16 : 32);
			break;
		case 2:
			Cvar_SetValueQuick (&vid_fullscreen, !vid_fullscreen.integer);
			break;
	}
}


void M_Video_Key (int key, char ascii)
{
	switch (key)
	{
		case K_ESCAPE:
			// vid_shared.c has a copy of the current video config. We restore it
			Cvar_SetValueQuick(&vid_fullscreen, current_vid_fullscreen);
			Cvar_SetValueQuick(&vid_width, current_vid_width);
			Cvar_SetValueQuick(&vid_height, current_vid_height);
			Cvar_SetValueQuick(&vid_bitsperpixel, current_vid_bitsperpixel);

			S_LocalSound ("misc/menu1.wav", true);
			M_Menu_Options_f ();
			break;

		case K_ENTER:
			m_entersound = true;
			switch (video_cursor)
			{
				case 3:
					Cbuf_AddText ("vid_restart\n");
					M_Menu_Options_f ();
					break;
				default:
					M_Menu_Video_AdjustSliders (1);
			}
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav", true);
			video_cursor--;
			if (video_cursor < 0)
				video_cursor = VIDEO_ITEMS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav", true);
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

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_Background(320, 200);
	M_DrawPic (0, 0, va("gfx/help%i.lmp", help_page));
}


void M_Help_Key (int key, char ascii)
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
/* QUIT MENU */

char *m_quit_message[9];
int		m_quit_prevstate;
qboolean	wasInMenus;


int M_QuitMessage(char *line1, char *line2, char *line3, char *line4, char *line5, char *line6, char *line7, char *line8)
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

int M_ChooseQuitMessage(int request)
{
	switch (gamemode)
	{
	case GAME_NORMAL:
	case GAME_HIPNOTIC:
	case GAME_ROGUE:
	case GAME_NEHAHRA:
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


void M_Quit_Key (int key, char ascii)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
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

void M_Quit_Draw (void)
{
	int i, l, linelength, firstline, lastline, lines;
	for (i = 0, linelength = 0, firstline = 9999, lastline = -1;m_quit_message[i];i++)
	{
		if ((l = strlen(m_quit_message[i])))
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
	M_DrawTextBox(0, 0, linelength, lines);
	for (i = 0, l = firstline;i < lines;i++, l++)
		M_Print(8 + 4 * (linelength - strlen(m_quit_message[l])), 8 + 8 * i, m_quit_message[l]);
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
int		lanConfig_cursor_table [] = {56, 76, 112};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

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

	m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
	cachepic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, "gfx/p_multi.lmp");

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
		M_Print(basex, lanConfig_cursor_table[1], "Search for games...");
		M_Print(basex, lanConfig_cursor_table[2]-16, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[2]-8, 22, 1);
		M_Print(basex+16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print(basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_Print(basex, 168, m_return_reason);
}


void M_LanConfig_Key (int key, char ascii)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav", true);
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		Cbuf_AddText ("stopdemo\n");

		Cvar_SetValue("port", lanConfig_port);

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_ServerList_f();
			break;
		}

		if (lanConfig_cursor == 2)
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (ascii < 32 || ascii > 126)
			break;

		if (lanConfig_cursor == 2)
		{
			l = strlen(lanConfig_joinname);
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
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = ascii;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
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

typedef struct
{
	char	*name;
	char	*description;
} level_t;

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

typedef struct
{
	char *gamename;
	level_t *levels;
	episode_t *episodes;
	int numepisodes;
}
gamelevels_t;

level_t quakelevels[] =
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

episode_t quakeepisodes[] =
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
level_t     hipnoticlevels[] =
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
episode_t   hipnoticepisodes[] =
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
level_t		roguelevels[] =
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
episode_t	rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

level_t		nehahralevels[] =
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

episode_t	nehahraepisodes[] =
{
	{"Welcome to Nehahra", 0, 1},
	{"The Fall of Forge", 1, 9},
	{"The Outlands", 10, 7},
	{"Dimension of the Lost", 17, 2}
};

// Map list for Transfusion
level_t		transfusionlevels[] =
{
	{"bb1",			"The Stronghold"},
	{"bb2",			"Winter Wonderland"},
	{"bb3",			"Bodies"},
	{"bb4",			"The Tower"},
	{"bb5",			"Click!"},
	{"bb6",			"Twin Fortress"},
	{"bb7",			"Midgard"},
	{"bb8",			"Fun With Heads"},

	{"e1m1",		"Cradle to Grave"},
	{"e1m2",		"Wrong Side of the Tracks"},
	{"e1m7",		"Altar of Stone"},
	{"e2m8",		"The Lair of Shial"},
	{"e3m1",		"Ghost Town"},
	{"e3m7",		"The Pit of Cerberus"},
	{"e4m1",		"Butchery Loves Company"},
	{"e4m7",		"In the Flesh"},
	{"e4m8",		"The Hall of the Epiphany"},
	{"e4m9",		"Mall of the Dead"},

	{"dm1",			"Monolith Building 11"},
	{"dm2",			"Power!"},
	{"dm3",			"Area 15"},
	{"e6m1",		"Welcome to Your Life"},
	{"e6m8",		"Beauty and the Beast"},
	{"e6m9",		"Forgotten Catacombs"},

	{"cpbb01",		"Crypt of Despair"},
	{"cpbb03",		"Unholy Cathedral"},

	{"b2a15",		"Area 15 (B2)"},
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
	{"qmorbias",	"Dm-Morbias"},
	{"simple",		"Dead Simple"}
};

episode_t	transfusionepisodes[] =
{
	{"Blood", 0, 8},
	{"Blood Single Player", 8, 10},
	{"Plasma Pack", 18, 6},
	{"Cryptic Passage", 24, 2},
	{"Blood 2", 26, 5},
	{"Transfusion", 31, 9},
	{"Conversions", 40, 6}
};

level_t goodvsbad2levels[] =
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

episode_t goodvsbad2episodes[] =
{
	{"Levels? Bevels!", 0, 8},
};

level_t battlemechlevels[] =
{
	{"start", "Parking Level"},
	{"dm1", "Hot Dump"},                        // 1
	{"dm2", "The Pits"},
	{"dm3", "Dimber Died"},
	{"dm4", "Fire in the Hole"},
	{"dm5", "Clubhouses"},
	{"dm6", "Army go Underground"},
};

episode_t battlemechepisodes[] =
{
	{"Time for Battle", 0, 7},
};

level_t openquartzlevels[] =
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

episode_t openquartzepisodes[] =
{
	{"Single Player", 0, 1},
	{"Void Deathmatch", 1, 6},
	{"Contrib", 7, 6},
};

gamelevels_t sharewarequakegame = {"Shareware Quake", quakelevels, quakeepisodes, 2};
gamelevels_t registeredquakegame = {"Quake", quakelevels, quakeepisodes, 7};
gamelevels_t hipnoticgame = {"Scourge of Armagon", hipnoticlevels, hipnoticepisodes, 6};
gamelevels_t roguegame = {"Dissolution of Eternity", roguelevels, rogueepisodes, 4};
gamelevels_t nehahragame = {"Nehahra", nehahralevels, nehahraepisodes, 4};
gamelevels_t transfusiongame = {"Transfusion", transfusionlevels, transfusionepisodes, 7};
gamelevels_t goodvsbad2game = {"Good Vs. Bad 2", goodvsbad2levels, goodvsbad2episodes, 1};
gamelevels_t battlemechgame = {"Battlemech", battlemechlevels, battlemechepisodes, 1};
gamelevels_t openquartzgame = {"OpenQuartz", openquartzlevels, openquartzepisodes, 3};

typedef struct
{
	int gameid;
	gamelevels_t *notregistered;
	gamelevels_t *registered;
}
gameinfo_t;

gameinfo_t gamelist[] =
{
	{GAME_NORMAL, &sharewarequakegame, &registeredquakegame},
	{GAME_HIPNOTIC, &hipnoticgame, &hipnoticgame},
	{GAME_ROGUE, &roguegame, &roguegame},
	{GAME_NEHAHRA, &nehahragame, &nehahragame},
	{GAME_TRANSFUSION, &transfusiongame, &transfusiongame},
	{GAME_GOODVSBAD2, &goodvsbad2game, &goodvsbad2game},
	{GAME_BATTLEMECH, &battlemechgame, &battlemechgame},
	{GAME_OPENQUARTZ, &openquartzgame, &openquartzgame},
	{-1, &sharewarequakegame, &registeredquakegame} // final fallback
};

gamelevels_t *lookupgameinfo(void)
{
	int i;
	for (i = 0;gamelist[i].gameid >= 0 && gamelist[i].gameid != gamemode;i++);
	if (registered.integer)
		return gamelist[i].registered;
	else
		return gamelist[i].notregistered;
}

int	startepisode;
int	startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

extern cvar_t sv_public;

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


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 104, 132, 152, 160};
#define	NUM_GAMEOPTIONS	11
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	cachepic_t	*p;
	int		x;
	gamelevels_t *g;

	M_Background(320, 200);

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");

	M_DrawTextBox (152, 32, 10, 1);
	M_Print(160, 40, "begin game");

	M_Print(0, 56, "      Max players");
	M_Print(160, 56, va("%i", maxplayers) );

	if (gamemode != GAME_GOODVSBAD2)
	{
		M_Print(0, 64, "        Game Type");
		if (gamemode == GAME_TRANSFUSION)
		{
			if (!deathmatch.integer)
				Cvar_SetValue("deathmatch", 1);
			if (deathmatch.integer == 2)
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
		if (skill.integer == 0)
			M_Print(160, 80, "Easy difficulty");
		else if (skill.integer == 1)
			M_Print(160, 80, "Normal difficulty");
		else if (skill.integer == 2)
			M_Print(160, 80, "Hard difficulty");
		else
			M_Print(160, 80, "Nightmare difficulty");

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

	M_Print(0, 120, "      Server name");
	M_DrawTextBox (0, 124, 38, 1);
	M_Print(8, 132, hostname.string);

	g = lookupgameinfo();

	if (gamemode != GAME_GOODVSBAD2)
	{
		M_Print(0, 152, "         Episode");
		M_Print(160, 152, g->episodes[startepisode].description);
	}

	M_Print(0, 160, "           Level");
	M_Print(160, 160, g->levels[g->episodes[startepisode].firstLevel + startlevel].description);
	M_Print(160, 168, g->levels[g->episodes[startepisode].firstLevel + startlevel].name);

// line cursor
	if (gameoptions_cursor == 8)
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
			M_Print(x, 146, " More than 64 players?? ");
			M_Print(x, 154, "  First, question your  ");
			M_Print(x, 162, "   sanity, then email   ");
			M_Print(x, 170, " havoc@telefragged.com  ");
		}
		else
			m_serverInfoMessage = false;
	}
}


void M_NetStart_Change (int dir)
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
			if (deathmatch.integer == 2) // changing from CTF to BloodBath
				Cvar_SetValueQuick (&deathmatch, 0);
			else // changing from BloodBath to CTF
				Cvar_SetValueQuick (&deathmatch, 2);
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
		if (skill.integer > 3)
			Cvar_SetValueQuick (&skill, 0);
		if (skill.integer < 0)
			Cvar_SetValueQuick (&skill, 3);
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
		break;

	case 9:
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

	case 10:
		startlevel += dir;
		g = lookupgameinfo();

		if (startlevel < 0)
			startlevel = g->episodes[startepisode].levels - 1;

		if (startlevel >= g->episodes[startepisode].levels)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key, char ascii)
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
		S_LocalSound ("misc/menu1.wav", true);
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav", true);
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav", true);
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav", true);
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav", true);
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
		if (gameoptions_cursor == 8)
		{
			l = strlen(hostname.string);
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
		if (ascii < 32 || ascii > 126)
			break;
		if (gameoptions_cursor == 8)
		{
			l = strlen(hostname.string);
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

int slist_cursor;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_reason[0] = 0;
	Net_Slist_f();
}


void M_ServerList_Draw (void)
{
	int n, y, visible, start, end;
	cachepic_t *p;
	const char *s;

	// use as much vertical space as available
	M_Background(640, vid.conheight);
	// scroll the list as the cursor moves
	s = va("%i/%i masters %i/%i servers", masterreplycount, masterquerycount, serverreplycount, serverquerycount);
	M_PrintRed((640 - strlen(s) * 8) / 2, 32, s);
	if (*m_return_reason)
		M_Print(16, vid.conheight - 8, m_return_reason);
	y = 48;
	visible = (vid.conheight - 16 - y) / 8;
	start = bound(0, slist_cursor - (visible >> 1), hostCacheCount - visible);
	end = min(start + visible, hostCacheCount);

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((640 - p->width) / 2, 4, "gfx/p_multi.lmp");
	if (end > start)
	{
		for (n = start;n < end;n++)
		{
			DrawQ_Fill(menu_x, menu_y + y, 640, 16, n == slist_cursor ? (0.5 + 0.2 * sin(realtime * M_PI)) : 0, 0, 0, 0.5, 0);
			M_Print(0, y, hostcache[n].line1);y += 8;
			M_Print(0, y, hostcache[n].line2);y += 8;
		}
	}
	else if (realtime - masterquerytime < 3)
	{
		if (masterquerycount)
			M_Print(0, y, "No servers found");
		else
			M_Print(0, y, "No master servers found (network problem?)");
	}
}


void M_ServerList_Key(int k, char ascii)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f();
		break;

	case K_SPACE:
		Net_Slist_f();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav", true);
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav", true);
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound("misc/menu2.wav", true);
		Cbuf_AddText(va("connect \"%s\"\n", hostcache[slist_cursor].cname));
		break;

	default:
		break;
	}

}

//=============================================================================
/* Menu Subsystem */

void M_Keydown(int key, char ascii);
void M_Draw(void);
void M_ToggleMenu_f(void);
void M_Shutdown(void);

void M_Init (void)
{
	menu_mempool = Mem_AllocPool("Menu");
	menuplyr_load = true;
	menuplyr_pixels = NULL;

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_options_effects", M_Menu_Options_Effects_f);
	Cmd_AddCommand ("menu_options_graphics", M_Menu_Options_Graphics_f);
	Cmd_AddCommand ("menu_options_colorcontrol", M_Menu_Options_ColorControl_f);
	Cvar_RegisterVariable (&menu_options_colorcontrol_correctionvalue);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);

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

	case m_video:
		M_Video_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
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

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav", true);
		m_entersound = false;
	}

	S_ExtraUpdate ();
}


void M_Keydown (int key, char ascii)
{
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

	case m_video:
		M_Video_Key (key, ascii);
		return;

	case m_help:
		M_Help_Key (key, ascii);
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
#define M_F_DRAW		"m_draw"
// ng_menu function names
#define	M_F_DISPLAY		"m_display"
#define	M_F_HIDE		"m_hide"
// normal menu names (rest)
#define M_F_TOGGLE		"m_toggle"
#define M_F_SHUTDOWN	"m_shutdown"

static char *m_required_func[] = {
M_F_INIT,
M_F_KEYDOWN,
M_F_DRAW,
#ifdef NG_MENU
M_F_DISPLAY,
M_F_HIDE,
#else
M_F_TOGGLE,
#endif
M_F_SHUTDOWN,
};

#ifdef NG_MENU
qboolean m_displayed;
#endif

static int m_numrequiredfunc = sizeof(m_required_func) / sizeof(char*);

static func_t m_draw, m_keydown;

void MR_SetRouting (qboolean forceold);

void MP_Error(void)
{
	// fall back to the normal menu

	// say it
	Con_Print("Falling back to normal menu\n");

	key_dest = key_game;

	//PRVM_ResetProg();

	// init the normal menu now -> this will also correct the menu router pointers
	MR_SetRouting (TRUE);
}

void MP_Keydown (int key, char ascii)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = realtime;

	// pass key
	prog->globals[OFS_PARM0] = (float) key;
	prog->globals[OFS_PARM1] = (float) ascii;
	PRVM_ExecuteProgram(m_keydown, M_F_KEYDOWN"(float key, float ascii) required\n");

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

void MP_Init (void)
{
	PRVM_Begin;
	PRVM_InitProg(PRVM_MENUPROG);

	prog->crc = M_PROGHEADER_CRC;
	prog->edictprivate_size = 0; // no private struct used
	prog->name = M_NAME;
	prog->limit_edicts = M_MAX_EDICTS;
	prog->extensionstring = vm_m_extensions;
	prog->builtins = vm_m_builtins;
	prog->numbuiltins = vm_m_numbuiltins;
	prog->init_cmd = VM_M_Cmd_Init;
	prog->reset_cmd = VM_M_Cmd_Reset;
	prog->error_cmd = MP_Error;

	// allocate the mempools
	prog->edicts_mempool = Mem_AllocPool(M_NAME " edicts mempool");
	prog->edictstring_mempool = Mem_AllocPool( M_NAME " edict string mempool");
	prog->progs_mempool = Mem_AllocPool(M_PROG_FILENAME);

	PRVM_LoadProgs(M_PROG_FILENAME, m_numrequiredfunc, m_required_func);

	// set m_draw and m_keydown
	m_draw = (func_t) (PRVM_ED_FindFunction(M_F_DRAW) - prog->functions);
	m_keydown = (func_t) (PRVM_ED_FindFunction(M_F_KEYDOWN) - prog->functions);

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

static cvar_t forceqmenu = { 0, "forceqmenu", "0" };

void MR_SetRouting(qboolean forceold)
{
	static qboolean m_init = FALSE, mp_init = FALSE;

	// if the menu prog isnt available or forceqmenu ist set, use the old menu
	if(!FS_FileExists(M_PROG_FILENAME) || forceqmenu.integer || forceold)
	{
		// set menu router function pointers
		MR_Keydown = M_Keydown;
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
		MR_Keydown = MP_Keydown;
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
	if(MR_ToggleMenu_f)
		MR_ToggleMenu_f();
}

void MR_Init()
{
	// set router console commands
	Cvar_RegisterVariable (&forceqmenu);
	Cmd_AddCommand ("menu_restart",MR_Restart);
	Cmd_AddCommand ("togglemenu", Call_MR_ToggleMenu_f);

	// use -forceqmenu to use always the normal quake menu (it sets forceqmenu to 1)
	if(COM_CheckParm("-forceqmenu"))
		Cvar_SetValueQuick(&forceqmenu,1);
	// use -useqmenu for debugging proposes, cause it starts
	// the normal quake menu only the first time
	else if(COM_CheckParm("-useqmenu"))
		MR_SetRouting (TRUE);
	
	MR_SetRouting (FALSE);
}




