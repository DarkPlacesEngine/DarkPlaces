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


#define TYPE_DEMO 1
#define TYPE_GAME 2
#define TYPE_BOTH 3

int NehGameType;

enum m_state_e m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
	void M_Menu_Options_Effects_f (void);
	void M_Menu_Options_ColorControl_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_InetSearch_f (void);
void M_Menu_ServerList_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
	void M_Options_Effects_Draw (void);
	void M_Options_ColorControl_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_InetSearch_Draw (void);
void M_ServerList_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
	void M_Options_Effects_Key (int key);
	void M_Options_ColorControl_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_InetSearch_Key (int key);
void M_ServerList_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define	IPXConfig		(m_net_cursor == 0)
#define	TCPIPConfig		(m_net_cursor == 1)

void M_ConfigureNetSubsystem(void);

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

void M_DrawBackground(void)
{
	menu_width = 320;
	menu_height = 200;
	menu_x = (vid.conwidth - menu_width) * 0.5;
	menu_y = (vid.conheight - menu_height) * 0.5;
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

void M_Print (float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
}

void M_PrintWhite (float cx, float cy, const char *str)
{
	DrawQ_String(menu_x + cx, menu_y + cy, str, 0, 8, 8, 1, 1, 1, 1, 0);
}

void M_ItemPrint (float cx, float cy, char *str, int unghosted)
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

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	//if (key_dest == key_console)
	//	Con_ToggleConsole_f ();
	//else
		M_Menu_Main_f ();
}


int demo_cursor;
void M_Demo_Draw (void)
{
	int i;

	for (i = 0;i < NumberOfNehahraDemos;i++)
		M_Print (16, 16 + 8*i, NehahraDemos[i].desc);

	// line cursor
	M_DrawCharacter (8, 16 + demo_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Menu_Demos_f (void)
{
	key_dest = key_menu;
	m_state = m_demo;
	m_entersound = true;
}

void M_Demo_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText (va ("playdemo %s\n", NehahraDemos[demo_cursor].name));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor--;
		if (demo_cursor < 0)
			demo_cursor = NumberOfNehahraDemos;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor++;
		if (demo_cursor > NumberOfNehahraDemos)
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


void M_Main_Key (int key)
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
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
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

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");

	// Transfusion doesn't have a single player mode
	if (gamemode == GAME_TRANSFUSION)
	{
		M_DrawPic ((320 - p->width) / 2, 4, "gfx/ttl_sgl.lmp");

		M_DrawTextBox (60, 8 * 8, 23, 4);
		M_PrintWhite (95, 10 * 8, "Transfusion is for");
		M_PrintWhite (83, 11 * 8, "multiplayer play only");
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


void M_SinglePlayer_Key (int key)
{
	if (gamemode == GAME_TRANSFUSION)
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
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
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
	QFile	*f;
	int		version;

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf (name, "%s/s%i.sav", com_gamedir, i);
		f = Qopen (name, "rz");
		if (!f)
			continue;
		str = Qgetline (f);
		sscanf (str, "%i\n", &version);
		str = Qgetline (f);
		strncpy (m_filenames[i], str, sizeof(m_filenames[i])-1);

	// change _ back to space
		for (j=0 ; j<SAVEGAME_COMMENT_LENGTH ; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		Qclose (f);
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
	if (svs.maxclients != 1)
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

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_load.lmp");

	for (i=0 ; i< MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	cachepic_t	*p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_save.lmp");

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		// issue the load command
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


void M_Save_Key (int k)
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
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
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

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");
	M_DrawPic (72, 32, "gfx/mp_menu.lmp");

	f = (int)(realtime * 10)%6;

	M_DrawPic (54, 32 + m_multiplayer_cursor * 20, va("gfx/menudot%i.lmp", f+1));

	if (ipxAvailable || tcpipAvailable)
		return;
	M_PrintWhite ((320/2) - ((27*8)/2), 168, "No Communications Available");
}


void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 1:
			if (ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
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
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_hostname[16];
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	strcpy(setup_myname, cl_name.string);
	strcpy(setup_hostname, hostname.string);
	setup_top = setup_oldtop = cl_color.integer >> 4;
	setup_bottom = setup_oldbottom = cl_color.integer & 15;
}

// LordHavoc: rewrote this code greatly
void M_MenuPlayerTranslate (qbyte *translation, int top, int bottom)
{
	int i;
	unsigned int trans[4096];
	qbyte *data, *f;
	static qbyte pixels[4096];
	static int menuplyr_width, menuplyr_height, menuplyr_top, menuplyr_bottom, menuplyr_load = true, menuplyr_failed = false;

	if (menuplyr_failed)
		return;
	if (menuplyr_top == top && menuplyr_bottom == bottom)
		return;

	menuplyr_top = top;
	menuplyr_bottom = bottom;

	if (menuplyr_load)
	{
		menuplyr_load = false;
		f = COM_LoadFile("gfx/menuplyr.lmp", true);
		if (!f)
		{
			menuplyr_failed = true;
			return;
		}
		data = LoadLMPAs8Bit (f, 0, 0);
		Mem_Free(f);
		if (image_width * image_height > 4096)
		{
			Con_Printf("M_MenuPlayerTranslate: image larger than 4096 pixel buffer\n");
			Mem_Free(data);
			menuplyr_failed = true;
			return;
		}
		menuplyr_width = image_width;
		menuplyr_height = image_height;
		memcpy(pixels, data, menuplyr_width * menuplyr_height);
		Mem_Free(data);
	}

	M_BuildTranslationTable (menuplyr_top*16, menuplyr_bottom*16);

	for (i = 0;i < menuplyr_width * menuplyr_height;i++)
		trans[i] = palette_complete[translation[pixels[i]]];

	Draw_NewPic("gfx/menuplyr.lmp", menuplyr_width, menuplyr_height, true, (qbyte *)trans);
}

void M_Setup_Draw (void)
{
	cachepic_t	*p;

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");

	M_Print (64, 40, "Hostname");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_DrawTextBox (160, 48, 16, 1);
	M_Print (168, 56, setup_myname);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	M_DrawPic (160, 64, "gfx/bigbox.lmp");

	// LordHavoc: rewrote this code greatly
	M_MenuPlayerTranslate (translationTable, setup_top, setup_bottom);
	M_DrawPic (172, 72, "gfx/menuplyr.lmp");

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}


void M_Setup_Key (int k)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (strcmp(cl_name.string, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (strcmp(hostname.string, setup_hostname) != 0)
			Cvar_Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l+1] = 0;
				setup_hostname[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = k;
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
/* NET MENU */

int	m_net_cursor;
int m_net_items;
int m_net_saveHeight;

char *net_helpMessage [] =
{
/* .........1.........2.... */
  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 2;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
	int		f;
	cachepic_t	*p;

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");

	f = 32;

	if (ipxAvailable)
		M_DrawPic (72, f, "gfx/netmen3.lmp");
	else
		M_DrawPic (72, f, "gfx/dim_ipx.lmp");

	f += 19;
	if (tcpipAvailable)
		M_DrawPic (72, f, "gfx/netmen4.lmp");
	else
		M_DrawPic (72, f, "gfx/dim_tcp.lmp");

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		M_DrawPic (72, f, "gfx/netmen5.lmp");
	}

	f = (320-26*8)/2;
	M_DrawTextBox (f, 134, 24, 4);
	f += 8;
	M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);

	f = (int)(realtime * 10)%6;
	M_DrawPic (54, 32 + m_net_cursor * 20, va("gfx/menudot%i.lmp", f+1));
}


void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_net_cursor)
		{
		case 0:
			M_Menu_LanConfig_f ();
			break;

		case 1:
			M_Menu_LanConfig_f ();
			break;

		case 2:
// multiprotocol
			break;
		}
	}

	if (m_net_cursor == 0 && !ipxAvailable)
		goto again;
	if (m_net_cursor == 1 && !tcpipAvailable)
		goto again;
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
	sprintf(text, "%g", num);
	M_Print(x + (SLIDER_RANGE+2) * 8, y, text);
}

void M_DrawCheckbox (int x, int y, int on)
{
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}


#define OPTIONS_ITEMS 27

int options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

extern cvar_t gl_delayfinish;
extern cvar_t slowmo;

void M_Menu_Options_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 6:
		Cvar_SetValueQuick (&scr_2dresolution, bound(0, scr_2dresolution.value + dir * 0.2, 1));
		break;
	case 7:
		Cvar_SetValueQuick (&scr_viewsize, bound(30, scr_viewsize.value + dir * 10, 120));
		break;
	case 8:
		Cvar_SetValueQuick (&r_sky, !r_sky.integer);
		break;
	case 9:
		Cvar_SetValueQuick (&v_overbrightbits, bound(0, v_overbrightbits.integer + dir, 4));
		break;
	case 10:
		Cvar_SetValueQuick (&gl_combine, !gl_combine.integer);
		break;
	case 11:
		Cvar_SetValueQuick (&gl_dither, !gl_dither.integer);
		break;
	case 12:
		Cvar_SetValueQuick (&gl_delayfinish, !gl_delayfinish.integer);
		break;
	case 13:
		Cvar_SetValueQuick (&slowmo, bound(0, slowmo.value + dir * 0.25, 5));
		break;
	case 14: // music volume
		#ifdef _WIN32
		Cvar_SetValueQuick (&bgmvolume, bound(0, bgmvolume.value + dir * 1.0, 1));
		#else
		Cvar_SetValueQuick (&bgmvolume, bound(0, bgmvolume.value + dir * 0.1, 1));
		#endif
		break;
	case 15: // sfx volume
		Cvar_SetValueQuick (&volume, bound(0, volume.value + dir * 0.1, 1));
		break;
	case 16:
		Cvar_SetValueQuick (&crosshair, bound(0, crosshair.integer + dir, 5));
		break;
	case 17:
		Cvar_SetValueQuick (&crosshair_size, bound(1, crosshair_size.value + dir, 5));
		break;
	case 18: // static crosshair
		Cvar_SetValueQuick (&crosshair_static, !crosshair_static.integer);
		break;
	case 19: // show framerate
		Cvar_SetValueQuick (&showfps, !showfps.integer);
		break;
	case 20: // always run
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
		break;
	case 21: // lookspring
		Cvar_SetValueQuick (&lookspring, !lookspring.integer);
		break;
	case 22: // lookstrafe
		Cvar_SetValueQuick (&lookstrafe, !lookstrafe.integer);
		break;
	case 23: // mouse speed
		Cvar_SetValueQuick (&sensitivity, bound(1, sensitivity.value + dir * 0.5, 50));
		break;
	case 24: // mouse look
		Cvar_SetValueQuick (&freelook, !freelook.integer);
		break;
	case 25: // invert mouse
		Cvar_SetValueQuick (&m_pitch, -m_pitch.value);
		break;
	case 26: // windowed mouse
		Cvar_SetValueQuick (&vid_mouse, !vid_mouse.integer);
		break;
	}
}

void M_Options_Draw (void)
{
	float y;
	cachepic_t	*p;

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	y = 32;
	M_Print(16, y, "    Customize controls");y += 8;
	M_Print(16, y, "         Go to console");y += 8;
	M_Print(16, y, "     Reset to defaults");y += 8;
	M_Print(16, y, "         Video Options");y += 8;
	M_Print(16, y, "       Effects Options");y += 8;
	M_Print(16, y, " Color Control Options");y += 8;
	M_Print(16, y, "         2D Resolution");M_DrawSlider(220, y, scr_2dresolution.value, 0, 1);y += 8;
	M_Print(16, y, "           Screen size");M_DrawSlider(220, y, scr_viewsize.value, 30, 120);y += 8;
	M_Print(16, y, "                   Sky");M_DrawCheckbox(220, y, r_sky.integer);y += 8;
	M_Print(16, y, "       Overbright Bits");M_DrawSlider(220, y, v_overbrightbits.value, 0, 4);y += 8;
	M_Print(16, y, "       Texture Combine");M_DrawCheckbox(220, y, gl_combine.integer);y += 8;
	M_Print(16, y, "             Dithering");M_DrawCheckbox(220, y, gl_dither.integer);y += 8;
	M_Print(16, y, "Delay refresh (faster)");M_DrawCheckbox(220, y, gl_delayfinish.integer);y += 8;
	M_ItemPrint(16, y, "        Game Speed", sv.active);M_DrawSlider(220, y, slowmo.value, 0, 5);y += 8;
	M_ItemPrint(16, y, "       CD Music Volume", cdaudioinitialized);M_DrawSlider(220, y, bgmvolume.value, 0, 1);y += 8;
	M_ItemPrint(16, y, "          Sound Volume", snd_initialized);M_DrawSlider(220, y, volume.value, 0, 1);y += 8;
	M_Print(16, y, "             Crosshair");M_DrawSlider(220, y, crosshair.value, 0, 5);y += 8;
	M_Print(16, y, "        Crosshair Size");M_DrawSlider(220, y, crosshair_size.value, 1, 5);y += 8;
	M_Print(16, y, "      Static Crosshair");M_DrawCheckbox(220, y, crosshair_static.integer);y += 8;
	M_Print(16, y, "        Show Framerate");M_DrawCheckbox(220, y, showfps.integer);y += 8;
	M_Print(16, y, "            Always Run");M_DrawCheckbox(220, y, cl_forwardspeed.value > 200);y += 8;
	M_Print(16, y, "            Lookspring");M_DrawCheckbox(220, y, lookspring.integer);y += 8;
	M_Print(16, y, "            Lookstrafe");M_DrawCheckbox(220, y, lookstrafe.integer);y += 8;
	M_Print(16, y, "           Mouse Speed");M_DrawSlider(220, y, sensitivity.value, 1, 50);y += 8;
	M_Print(16, y, "            Mouse Look");M_DrawCheckbox(220, y, freelook.integer);y += 8;
	M_Print(16, y, "          Invert Mouse");M_DrawCheckbox(220, y, m_pitch.value < 0);y += 8;
	M_Print(16, y, "             Use Mouse");M_DrawCheckbox(220, y, vid_mouse.integer);y += 8;

	// cursor
	M_DrawCharacter(200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int k)
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
			M_Menu_Options_ColorControl_f ();
			break;
		default:
			M_Menu_Options_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
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

#define	OPTIONS_EFFECTS_ITEMS	16

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
extern cvar_t r_explosionclip;
extern cvar_t r_dlightmap;
extern cvar_t r_modellights;
extern cvar_t r_coronas;
extern cvar_t gl_flashblend;
extern cvar_t cl_particles_bulletimpacts;
extern cvar_t cl_particles_smoke;
extern cvar_t cl_particles_sparks;
extern cvar_t cl_particles_bubbles;
extern cvar_t cl_particles_blood;
extern cvar_t cl_particles_blood_size;
extern cvar_t cl_particles_blood_alpha;

void M_Menu_Options_Effects_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_effects_cursor)
	{
	case 0:
		Cvar_SetValueQuick (&r_modellights, bound(0, r_modellights.value + dir, 8));
		break;
	case 1:
		Cvar_SetValueQuick (&r_dlightmap, !r_dlightmap.integer);
		break;
	case 2:
		Cvar_SetValueQuick (&r_coronas, !r_coronas.integer);
		break;
	case 3:
		Cvar_SetValueQuick (&gl_flashblend, !gl_flashblend.integer);
		break;
	case 4:
		Cvar_SetValueQuick (&cl_particles, !cl_particles.integer);
		break;
	case 5:
		Cvar_SetValueQuick (&cl_explosions, !cl_explosions.integer);
		break;
	case 6:
		Cvar_SetValueQuick (&r_explosionclip, !r_explosionclip.integer);
		break;
	case 7:
		Cvar_SetValueQuick (&cl_stainmaps, !cl_stainmaps.integer);
		break;
	case 8:
		Cvar_SetValueQuick (&r_detailtextures, !r_detailtextures.integer);
		break;
	case 9:
		Cvar_SetValueQuick (&cl_particles_bulletimpacts, !cl_particles_bulletimpacts.integer);
		break;
	case 10:
		Cvar_SetValueQuick (&cl_particles_smoke, !cl_particles_smoke.integer);
		break;
	case 11:
		Cvar_SetValueQuick (&cl_particles_sparks, !cl_particles_sparks.integer);
		break;
	case 12:
		Cvar_SetValueQuick (&cl_particles_bubbles, !cl_particles_bubbles.integer);
		break;
	case 13:
		Cvar_SetValueQuick (&cl_particles_blood, !cl_particles_blood.integer);
		break;
	case 14:
		Cvar_SetValueQuick (&cl_particles_blood_size, bound(2, cl_particles_blood_size.value + dir * 1, 20));
		break;
	case 15:
		Cvar_SetValueQuick (&cl_particles_blood_alpha, bound(0.2, cl_particles_blood_alpha.value + dir * 0.1, 1));
		break;
	}
}

void M_Options_Effects_Draw (void)
{
	float y;
	cachepic_t	*p;

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	y = 32;
	M_Print(16, y, "      Lights Per Model");M_DrawSlider(220, y, r_modellights.value, 0, 8);y += 8;
	M_Print(16, y, " Fast Dynamic Lighting");M_DrawCheckbox(220, y, !r_dlightmap.integer);y += 8;
	M_Print(16, y, "               Coronas");M_DrawCheckbox(220, y, r_coronas.integer);y += 8;
	M_Print(16, y, "      Use Only Coronas");M_DrawCheckbox(220, y, gl_flashblend.integer);y += 8;
	M_Print(16, y, "             Particles");M_DrawCheckbox(220, y, cl_particles.integer);y += 8;
	M_Print(16, y, "            Explosions");M_DrawCheckbox(220, y, cl_explosions.integer);y += 8;
	M_Print(16, y, "    Explosion Clipping");M_DrawCheckbox(220, y, r_explosionclip.integer);y += 8;
	M_Print(16, y, "             Stainmaps");M_DrawCheckbox(220, y, cl_stainmaps.integer);y += 8;
	M_Print(16, y, "      Detail Texturing");M_DrawCheckbox(220, y, r_detailtextures.integer);y += 8;
	M_Print(16, y, "        Bullet Impacts");M_DrawCheckbox(220, y, cl_particles_bulletimpacts.integer);y += 8;
	M_Print(16, y, "                 Smoke");M_DrawCheckbox(220, y, cl_particles_smoke.integer);y += 8;
	M_Print(16, y, "                Sparks");M_DrawCheckbox(220, y, cl_particles_sparks.integer);y += 8;
	M_Print(16, y, "               Bubbles");M_DrawCheckbox(220, y, cl_particles_bubbles.integer);y += 8;
	M_Print(16, y, "                 Blood");M_DrawCheckbox(220, y, cl_particles_blood.integer);y += 8;
	M_Print(16, y, "            Blood Size");M_DrawSlider(220, y, cl_particles_blood_size.value, 2, 20);y += 8;
	M_Print(16, y, "         Blood Opacity");M_DrawSlider(220, y, cl_particles_blood_alpha.value, 0.2, 1);y += 8;

	// cursor
	M_DrawCharacter(200, 32 + options_effects_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Effects_Key (int k)
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
		S_LocalSound ("misc/menu1.wav");
		options_effects_cursor--;
		if (options_effects_cursor < 0)
			options_effects_cursor = OPTIONS_EFFECTS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
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
	float f;
	S_LocalSound ("misc/menu3.wav");

	switch (options_colorcontrol_cursor)
	{
	case 1:
		Cvar_SetValueQuick (&v_hwgamma, !v_hwgamma.integer);
		break;
	case 2:
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_gamma, bound(1, v_gamma.value + dir * 0.125, 5));
		break;
	case 3:
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_contrast, bound(1, v_contrast.value + dir * 0.125, 5));
		break;
	case 4:
		Cvar_SetValueQuick (&v_color_enable, 0);
		Cvar_SetValueQuick (&v_brightness, bound(0, v_brightness.value + dir * 0.05, 0.8));
		break;
	case 5:
		Cvar_SetValueQuick (&v_color_enable, !v_color_enable.integer);
		break;
	case 6:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_r, bound(0, v_color_black_r.value + dir * 0.0125, 0.8));
		break;
	case 7:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_g, bound(0, v_color_black_g.value + dir * 0.0125, 0.8));
		break;
	case 8:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_black_b, bound(0, v_color_black_b.value + dir * 0.0125, 0.8));
		break;
	case 9:
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(0, (v_color_black_r.value + v_color_black_g.value + v_color_black_b.value) / 3 + dir * 0.0125, 0.8);
		Cvar_SetValueQuick (&v_color_black_r, f);
		Cvar_SetValueQuick (&v_color_black_g, f);
		Cvar_SetValueQuick (&v_color_black_b, f);
		break;
	case 10:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_r, bound(0, v_color_grey_r.value + dir * 0.0125, 0.95));
		break;
	case 11:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_g, bound(0, v_color_grey_g.value + dir * 0.0125, 0.95));
		break;
	case 12:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_grey_b, bound(0, v_color_grey_b.value + dir * 0.0125, 0.95));
		break;
	case 13:
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(0, (v_color_grey_r.value + v_color_grey_g.value + v_color_grey_b.value) / 3 + dir * 0.0125, 0.95);
		Cvar_SetValueQuick (&v_color_grey_r, f);
		Cvar_SetValueQuick (&v_color_grey_g, f);
		Cvar_SetValueQuick (&v_color_grey_b, f);
		break;
	case 14:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_r, bound(1, v_color_white_r.value + dir * 0.125, 5));
		break;
	case 15:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_g, bound(1, v_color_white_g.value + dir * 0.125, 5));
		break;
	case 16:
		Cvar_SetValueQuick (&v_color_enable, 1);
		Cvar_SetValueQuick (&v_color_white_b, bound(1, v_color_white_b.value + dir * 0.125, 5));
		break;
	case 17:
		Cvar_SetValueQuick (&v_color_enable, 1);
		f = bound(1, (v_color_white_r.value + v_color_white_g.value + v_color_white_b.value) / 3 + dir * 0.125, 5);
		Cvar_SetValueQuick (&v_color_white_r, f);
		Cvar_SetValueQuick (&v_color_white_g, f);
		Cvar_SetValueQuick (&v_color_white_b, f);
		break;
	}
}

void M_Options_ColorControl_Draw (void)
{
	float x, y, c, s, t, u, v;
	cachepic_t	*p;

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/p_option.lmp");

	y = 32;
	M_Print(16, y, "     Reset to defaults");y += 8;
	M_ItemPrint(16, y, "Hardware Gamma Control", vid_hardwaregammasupported);M_DrawCheckbox(220, y, v_hwgamma.integer);y += 8;
	M_ItemPrint(16, y, "                 Gamma", !v_color_enable.integer && vid_hardwaregammasupported && v_hwgamma.integer);M_DrawSlider(220, y, v_gamma.value, 1, 5);y += 8;
	M_ItemPrint(16, y, "              Contrast", !v_color_enable.integer);M_DrawSlider(220, y, v_contrast.value, 1, 5);y += 8;
	M_ItemPrint(16, y, "            Brightness", !v_color_enable.integer);M_DrawSlider(220, y, v_brightness.value, 0, 0.8);y += 8;
	M_Print(16, y, "  Color Level Controls");M_DrawCheckbox(220, y, v_color_enable.integer);y += 8;
	M_ItemPrint(16, y, "          Black: Red  ", v_color_enable.integer);M_DrawSlider(220, y, v_color_black_r.value, 0, 0.8);y += 8;
	M_ItemPrint(16, y, "          Black: Green", v_color_enable.integer);M_DrawSlider(220, y, v_color_black_g.value, 0, 0.8);y += 8;
	M_ItemPrint(16, y, "          Black: Blue ", v_color_enable.integer);M_DrawSlider(220, y, v_color_black_b.value, 0, 0.8);y += 8;
	M_ItemPrint(16, y, "          Black: Grey ", v_color_enable.integer);M_DrawSlider(220, y, (v_color_black_r.value + v_color_black_g.value + v_color_black_b.value) / 3, 0, 0.8);y += 8;
	M_ItemPrint(16, y, "           Grey: Red  ", v_color_enable.integer && vid_hardwaregammasupported && v_hwgamma.integer);M_DrawSlider(220, y, v_color_grey_r.value, 0, 0.95);y += 8;
	M_ItemPrint(16, y, "           Grey: Green", v_color_enable.integer && vid_hardwaregammasupported && v_hwgamma.integer);M_DrawSlider(220, y, v_color_grey_g.value, 0, 0.95);y += 8;
	M_ItemPrint(16, y, "           Grey: Blue ", v_color_enable.integer && vid_hardwaregammasupported && v_hwgamma.integer);M_DrawSlider(220, y, v_color_grey_b.value, 0, 0.95);y += 8;
	M_ItemPrint(16, y, "           Grey: Grey ", v_color_enable.integer && vid_hardwaregammasupported && v_hwgamma.integer);M_DrawSlider(220, y, (v_color_grey_r.value + v_color_grey_g.value + v_color_grey_b.value) / 3, 0, 0.95);y += 8;
	M_ItemPrint(16, y, "          White: Red  ", v_color_enable.integer);M_DrawSlider(220, y, v_color_white_r.value, 1, 5);y += 8;
	M_ItemPrint(16, y, "          White: Green", v_color_enable.integer);M_DrawSlider(220, y, v_color_white_g.value, 1, 5);y += 8;
	M_ItemPrint(16, y, "          White: Blue ", v_color_enable.integer);M_DrawSlider(220, y, v_color_white_b.value, 1, 5);y += 8;
	M_ItemPrint(16, y, "          White: Grey ", v_color_enable.integer);M_DrawSlider(220, y, (v_color_white_r.value + v_color_white_g.value + v_color_white_b.value) / 3, 1, 5);y += 8;

	y += 4;
	DrawQ_Fill(menu_x, menu_y + y, 320, 4 + 64 + 8 + 64 + 4, 0, 0, 0, 1, 0);y += 4;
	s = (float) 312 / 2 * vid.realwidth / vid.conwidth;
	t = (float) 4 / 2 * vid.realheight / vid.conheight;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 1,0,0,1, 0,1, 0,0,0,1, 1,1, 1,0,0,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 0,1,0,1, 0,1, 0,0,0,1, 1,1, 0,1,0,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 0,0,1,1, 0,1, 0,0,0,1, 1,1, 0,0,1,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, "gfx/colorcontrol/ditherpattern.tga", 312, 4, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);y += 4;
	DrawQ_SuperPic(menu_x + 4, menu_y + y, NULL                                , 312, 4, 0,0, 0,0,0,1, 1,0, 1,1,1,1, 0,1, 0,0,0,1, 1,1, 1,1,1,1, 0);y += 4;

	c = menu_options_colorcontrol_correctionvalue.value; // intensity value that should be matched up to a 50% dither to 'correct' quake
	s = (float) 48 / 2 * vid.realwidth / vid.conwidth;
	t = (float) 48 / 2 * vid.realheight / vid.conheight;
	u = s * 0.5;
	v = t * 0.5;
	y += 8;
	x = 4;
	DrawQ_Fill(menu_x + x, menu_y + y, 64, 48, c, 0, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,0,0,1, s,0, 1,0,0,1, 0,t, 1,0,0,1, s,t, 1,0,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,0,0,1, u,0, 1,0,0,1, 0,v, 1,0,0,1, u,v, 1,0,0,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + y, 64, 48, 0, c, 0, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,1,0,1, s,0, 0,1,0,1, 0,t, 0,1,0,1, s,t, 0,1,0,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,1,0,1, u,0, 0,1,0,1, 0,v, 0,1,0,1, u,v, 0,1,0,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + y, 64, 48, 0, 0, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,0,1,1, s,0, 0,0,1,1, 0,t, 0,0,1,1, s,t, 0,0,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 0,0,1,1, u,0, 0,0,1,1, 0,v, 0,0,1,1, u,v, 0,0,1,1, 0);
	x += 80;
	DrawQ_Fill(menu_x + x, menu_y + y, 64, 48, c, c, c, 1, 0);
	DrawQ_SuperPic(menu_x + x + 16, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,1,1,1, s,0, 1,1,1,1, 0,t, 1,1,1,1, s,t, 1,1,1,1, 0);
	DrawQ_SuperPic(menu_x + x + 32, menu_y + y + 16, "gfx/colorcontrol/ditherpattern.tga", 16, 16, 0,0, 1,1,1,1, u,0, 1,1,1,1, 0,v, 1,1,1,1, u,v, 1,1,1,1, 0);

	// cursor
	M_DrawCharacter(200, 32 + options_colorcontrol_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_ColorControl_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
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
			Cbuf_AddText ("exec default.cfg\n");
			break;
		default:
			M_Menu_Options_ColorControl_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_colorcontrol_cursor--;
		if (options_colorcontrol_cursor < 0)
			options_colorcontrol_cursor = OPTIONS_COLORCONTROL_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
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
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+jump", 			"jump / swim up"},
{"+movedown",		"swim down"},
{"+attack", 		"attack"},
{"+button3",		"altfire"},
{"impulse 1",		"Pitch Fork"},
{"impulse 2",		"Flare Gun"},
{"impulse 3",		"Shotgun"},
{"impulse 4",		"Machine Gun"},
{"impulse 5",		"Incinerator"},
{"impulse 6",		"Bombs"},
{"impulse 7",		"Aerosol Can"},
{"impulse 8",		"Tesla Cannon"},
{"impulse 9",		"Life Leech"},
{"impulse 17",		"Voodoo Doll"},
{"impulse 11",		"previous weapon"},
{"impulse 10",		"next weapon"},
{"impulse 14",		"previous item"},
{"impulse 15",		"next item"},
{"impulse 13",		"use item"},
{"impulse 100",		"add bot (red)"},
{"impulse 101",		"add bot (blue)"},
{"impulse 102",		"kick a bot"},
{"impulse 50",		"voting menu"},
{"impulse 141",		"identify player"},
{"impulse 16",		"next armor type"},
{"impulse 20",		"observer mode"}
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

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
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

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strcmp (b, command))
			Key_SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	int		i, j;
	int		keys[NUMKEYS];
	int		y;
	cachepic_t	*p;
	char	keystring[1024];

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/ttl_cstm.lmp");

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<numcommands ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

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
		M_Print (150, y, keystring);
	}

	if (bind_grab)
		M_DrawCharacter (140, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (140, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[NUMKEYS];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
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
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = numcommands-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= numcommands)
			keys_cursor = 0;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[NUMKEYS - 1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

#define VIDEO_ITEMS 5

int video_cursor = 0;
int video_cursor_table[] = {56, 68, 80, 92, 116};
// note: if modes are added to the beginning of this list, update the
// video_resolution = x; in M_Menu_Video_f below
unsigned short video_resolutions[][2] = {{320,240}, {400,300}, {512,384}, {640,480}, {800,600}, {1024,768}, {1152,864}, {1280,960}, {1280,1024}, {1600,1200}, {1792,1344}, {1920,1440}, {2048,1536}};
int video_resolution;

extern int current_vid_fullscreen;
extern int current_vid_width;
extern int current_vid_height;
extern int current_vid_bitsperpixel;
extern int current_vid_stencil;


void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	// Look for the current resolution
	for (video_resolution = 0; video_resolution < (int) (sizeof (video_resolutions) / sizeof (video_resolutions[0])); video_resolution++)
	{
		if (video_resolutions[video_resolution][0] == current_vid_width &&
			video_resolutions[video_resolution][1] == current_vid_height)
			break;
	}

	// Default to 800x600 if we didn't find it
	if (video_resolution == sizeof (video_resolutions) / sizeof (video_resolutions[0]))
	{
		// may need to update this number if mode list changes
		video_resolution = 4;
		Cvar_SetValueQuick (&vid_width, video_resolutions[video_resolution][0]);
		Cvar_SetValueQuick (&vid_height, video_resolutions[video_resolution][1]);
	}
}


void M_Video_Draw (void)
{
	cachepic_t	*p;
	const char* string;

	M_DrawPic(16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic("gfx/vidmodes.lmp");
	M_DrawPic((320-p->width)/2, 4, "gfx/vidmodes.lmp");

	// Resolution
	M_Print(16, video_cursor_table[0], "            Resolution");
	string = va("%dx%d", video_resolutions[video_resolution][0], video_resolutions[video_resolution][1]);
	M_Print (220, video_cursor_table[0], string);

	// Bits per pixel
	M_Print(16, video_cursor_table[1], "        Bits per pixel");
	M_Print (220, video_cursor_table[1], (vid_bitsperpixel.integer == 32) ? "32" : "16");

	// Fullscreen
	M_Print(16, video_cursor_table[2], "            Fullscreen");
	M_DrawCheckbox(220, video_cursor_table[2], vid_fullscreen.integer);

	// Stencil
	M_Print(16, video_cursor_table[3], "               Stencil");
	M_DrawCheckbox(220, video_cursor_table[3], vid_stencil.integer);

	// "Apply" button
	M_Print(220, video_cursor_table[4], "Apply");

	// Cursor
	M_DrawCharacter(200, video_cursor_table[video_cursor], 12+((int)(realtime*4)&1));
}


void M_Menu_Video_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (video_cursor)
	{
		// Resolution
		case 0:
		{
			int new_resolution = video_resolution + dir;
			if (new_resolution < 0)
				video_resolution = sizeof (video_resolutions) / sizeof (video_resolutions[0]) - 1;
			else if (new_resolution > (int) (sizeof (video_resolutions) / sizeof (video_resolutions[0]) - 1))
				video_resolution = 0;
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
		case 3:
			Cvar_SetValueQuick (&vid_stencil, !vid_stencil.integer);
			break;
	}
}


void M_Video_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
			// vid_shared.c has a copy of the current video config. We restore it
			Cvar_SetValueQuick(&vid_fullscreen, current_vid_fullscreen);
			Cvar_SetValueQuick(&vid_width, current_vid_width);
			Cvar_SetValueQuick(&vid_height, current_vid_height);
			Cvar_SetValueQuick(&vid_bitsperpixel, current_vid_bitsperpixel);
			Cvar_SetValueQuick(&vid_stencil, current_vid_stencil);

			S_LocalSound ("misc/menu1.wav");
			M_Menu_Options_f ();
			break;

		case K_ENTER:
			m_entersound = true;
			switch (video_cursor)
			{
				case 4:
					Cbuf_AddText ("vid_restart\n");
					M_Menu_Options_f ();
					break;
				default:
					M_Menu_Video_AdjustSliders (1);
			}
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			video_cursor--;
			if (video_cursor < 0)
				video_cursor = VIDEO_ITEMS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
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
	M_DrawPic (0, 0, va("gfx/help%i.lmp", help_page));
}


void M_Help_Key (int key)
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

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

char *quitMessage [] =
{
/* .........1.........2.... */
/*
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",

  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",

  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",

  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",

  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",

  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
  */

/* .........1.........2.... */
  "                        ",
  "    Tired of fragging   ",
  "        already?        ",
  "                        ",

  "                        ",
  "  Quit now and forfeit  ",
  "     your bodycount?    ",
  "                        ",

  "                        ",
  "    Are you sure you    ",
  "      want to quit?     ",
  "                        ",

  "                        ",
  "   Off to do something  ",
  "      constructive?     ",
  "                        ",
};

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&3; //&7;
}


void M_Quit_Key (int key)
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
	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0]);
	M_Print (64, 92,  quitMessage[msgNumber*4+1]);
	M_Print (64, 100, quitMessage[msgNumber*4+2]);
	M_Print (64, 108, quitMessage[msgNumber*4+3]);
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
int		lanConfig_cursor_table [] = {72, 92, 112, 144};
#define NUM_LANCONFIG_CMDS	4

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
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	sprintf(lanConfig_portname, "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
	cachepic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, "gfx/p_multi.lmp");

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (IPXConfig)
		protocol = "IPX";
	else
		protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	if (IPXConfig)
		M_Print (basex+9*8, 52, my_ipx_address);
	else
		M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print (basex, lanConfig_cursor_table[2], "Search for internet games...");
		M_Print (basex, 128, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[3]-8, 22, 1);
		M_Print (basex+16, lanConfig_cursor_table[3], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print (basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 3)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [3], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 168, m_return_reason);
}


void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1 || lanConfig_cursor == 2)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			if (lanConfig_cursor == 1)
				M_Menu_Search_f();
			else
				M_Menu_InetSearch_f();
			break;
		}

		if (lanConfig_cursor == 3)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

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
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == 3)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = key;
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
	if (l > 65535)
		l = lanConfig_port;
	else
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
	{"e3m7",		"The Pit of Cerberus"},
	{"e4m8",		"The Hall of the Epiphany"},
	{"e4m9",		"Mall of the Dead"},

	{"dm1",			"Monolith Building 11"},
	{"dm2",			"Power!"},
	{"dm3",			"Area 15"},
	{"e6m1",		"Welcome to Your Life"},
	{"e6m8",		"Beauty and the Beast"},

	{"cpbb01",		"Crypt of Despair"},
	{"cpbb03",		"Unholy Cathedral"},

	{"b2a15",		"Area 15 (B2)"},
	{"barena",		"Blood Arena"},
	{"bkeep",		"Blood Keep"},
	{"bstar",		"Brown Star"},
	{"crypt",		"The Crypt"},

	{"bb3_2k1",		"Bodies Infusion"},
	{"dcamp",		"DeathCamp"},
	{"highnoon",	"HighNoon"},
	{"qbb1",		"The Confluence"},
	{"qbb2",		"KathartiK"},
	{"qbb3",		"Caleb's Woodland Retreat"},

	{"dranzbb6",	"Black Coffee"},
	{"fragm",		"Frag'M"},
	{"maim",		"Maim"},
	{"qe1m7",		"The House of Chthon"},
	{"simple",		"Dead Simple"}
};

episode_t	transfusionepisodes[] =
{
	{"Blood", 0, 8},
	{"Blood Single Player", 8, 7},
	{"Plasma Pack", 15, 5},
	{"Cryptic Passage", 20, 2},
	{"Blood 2", 22, 5},
	{"Transfusion", 27, 6},
	{"Conversions", 33, 5}
};

gamelevels_t sharewarequakegame = {"Shareware Quake", quakelevels, quakeepisodes, 2};
gamelevels_t registeredquakegame = {"Quake", quakelevels, quakeepisodes, 7};
gamelevels_t hipnoticgame = {"Scourge of Armagon", hipnoticlevels, hipnoticepisodes, 6};
gamelevels_t roguegame = {"Dissolution of Eternity", roguelevels, rogueepisodes, 4};
gamelevels_t nehahragame = {"Nehahra", nehahralevels, nehahraepisodes, 4};
gamelevels_t transfusiongame = {"Transfusion", transfusionlevels, transfusionepisodes, 7};

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

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = MAX_SCOREBOARD;
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	cachepic_t	*p;
	int		x;
	gamelevels_t *g;

	M_DrawPic (16, 4, "gfx/qplaque.lmp");
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers) );

	M_Print (0, 64, "        Game Type");
	if (gamemode == GAME_TRANSFUSION)
	{
		if (!deathmatch.integer)
			Cvar_SetValue("deathmatch", 1);
		if (deathmatch.integer == 2)
			M_Print (160, 64, "Capture the Flag");
		else
			M_Print (160, 64, "Blood Bath");
	}
	else
	{
		if (!coop.integer && !deathmatch.integer)
			Cvar_SetValue("deathmatch", 1);
		if (coop.integer)
			M_Print (160, 64, "Cooperative");
		else
			M_Print (160, 64, "Deathmatch");
	}

	M_Print (0, 72, "        Teamplay");
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
		M_Print (160, 72, msg);
	}
	else if (gamemode == GAME_TRANSFUSION)
	{
		char *msg;

		switch (teamplay.integer)
		{
			case 0: msg = "Off"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "No Friendly Fire"; break;
		}
		M_Print (160, 72, msg);
	}
	else
	{
		char *msg;

		switch((int)teamplay.integer)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill.integer == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill.integer == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill.integer == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit.integer == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va("%i frags", fraglimit.integer));

	M_Print (0, 96, "       Time Limit");
	if (timelimit.integer == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va("%i minutes", timelimit.integer));

	g = lookupgameinfo();

	M_Print (0, 112, "         Episode");
	M_Print (160, 112, g->episodes[startepisode].description);

	M_Print (0, 120, "           Level");
	M_Print (160, 120, g->levels[g->episodes[startepisode].firstLevel + startlevel].description);
	M_Print (160, 128, g->levels[g->episodes[startepisode].firstLevel + startlevel].name);

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, " More than 64 players?? ");
			M_Print (x, 154, "  First, question your  ");
			M_Print (x, 162, "   sanity, then email   ");
			M_Print (x, 170, " havoc@telefragged.com  ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
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
		if (gamemode == GAME_TRANSFUSION)
		{
			if (deathmatch.integer == 2) // changing from CTF to BloodBath
				Cvar_SetValueQuick (&deathmatch, 0);
			else // changing from BloodBath to CTF
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
		Cvar_SetValueQuick (&skill, skill.integer + dir);
		if (skill.integer > 3)
			Cvar_SetValueQuick (&skill, 0);
		if (skill.integer < 0)
			Cvar_SetValueQuick (&skill, 3);
		break;

	case 5:
		Cvar_SetValueQuick (&fraglimit, fraglimit.integer + dir*10);
		if (fraglimit.integer > 100)
			Cvar_SetValueQuick (&fraglimit, 0);
		if (fraglimit.integer < 0)
			Cvar_SetValueQuick (&fraglimit, 100);
		break;

	case 6:
		Cvar_SetValueQuick (&timelimit, timelimit.value + dir*5);
		if (timelimit.value > 60)
			Cvar_SetValueQuick (&timelimit, 0);
		if (timelimit.value < 0)
			Cvar_SetValueQuick (&timelimit, 60);
		break;

	case 7:
		startepisode += dir;
		g = lookupgameinfo();

		if (startepisode < 0)
			startepisode = g->numepisodes - 1;

		if (startepisode >= g->numepisodes)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		g = lookupgameinfo();

		if (startlevel < 0)
			startlevel = g->episodes[startepisode].levels - 1;

		if (startlevel >= g->episodes[startepisode].levels)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	gamelevels_t *g;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );

			g = lookupgameinfo();
			Cbuf_AddText ( va ("map %s\n", g->levels[g->episodes[startepisode].firstLevel + startlevel].name) );
			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();

}


void M_Search_Draw (void)
{
	const char* string;
	cachepic_t	*p;
	int x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if(slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (! searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	if (gamemode == GAME_TRANSFUSION)
		string = "No Transfusion servers found";
	else
		string = "No Quake servers found";
	M_PrintWhite ((320/2) - ((22*8)/2), 64, string);
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* INTERNET SEARCH MENU */

void M_Menu_InetSearch_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_InetSlist_f();

}


void M_InetSearch_Draw (void)
{
	M_Search_Draw ();  // it's the same one, so why bother?
}


void M_InetSearch_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int		slist_cursor;
qboolean slist_sorted;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}


void M_ServerList_Draw (void)
{
	int		n;
	char	string [64];
	cachepic_t	*p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			int	i,j;
			hostcache_t temp;
			for (i = 0; i < hostCacheCount; i++)
				for (j = i+1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
					}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, "gfx/p_multi.lmp");
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			sprintf(string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			sprintf(string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8*n, string);
	}
	M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 168, m_return_reason);
}


void M_ServerList_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		slist_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].cname) );
		break;

	default:
		break;
	}

}

//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_options_effects", M_Menu_Options_Effects_f);
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
	else
	{
		numcommands = sizeof(quakebindnames) / sizeof(quakebindnames[0]);
		bindnames = quakebindnames;
	}

	if (gamemode == GAME_NEHAHRA)
	{
		if (COM_FileExists("maps/neh1m4.bsp"))
		{
			if (COM_FileExists("hearing.dem"))
			{
				Con_Printf("Nehahra movie and game detected.\n");
				NehGameType = TYPE_BOTH;
			}
			else
			{
				Con_Printf("Nehahra game detected.\n");
				NehGameType = TYPE_GAME;
			}
		}
		else
		{
			if (COM_FileExists("hearing.dem"))
			{
				Con_Printf("Nehahra movie detected.\n");
				NehGameType = TYPE_DEMO;
			}
			else
			{
				Con_Printf("Nehahra not found.\n");
				NehGameType = TYPE_GAME; // could just complain, but...
			}
		}
	}
}

void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	M_DrawBackground();

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

	case m_net:
		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_options_effects:
		M_Options_Effects_Draw ();
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

	case m_search:
		M_Search_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}


void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_demo:
		M_Demo_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
		M_Load_Key (key);
		return;

	case m_save:
		M_Save_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
		M_Setup_Key (key);
		return;

	case m_net:
		M_Net_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_options_effects:
		M_Options_Effects_Key (key);
		return;

	case m_options_colorcontrol:
		M_Options_ColorControl_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_lanconfig:
		M_LanConfig_Key (key);
		return;

	case m_gameoptions:
		M_GameOptions_Key (key);
		return;

	case m_search:
		M_Search_Key (key);
		break;

	case m_slist:
		M_ServerList_Key (key);
		return;
	}
}


void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}

