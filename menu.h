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

#ifndef MENU_H
#define MENU_H

enum m_state_e {
	m_none,
	m_main,
	m_demo,
	m_singleplayer,
	m_load,
	m_save,
	m_multiplayer,
	m_setup,
	m_options,
	m_video,
	m_keys,
	m_help,
	m_quit,
	m_lanconfig,
	m_gameoptions,
	m_slist,
	m_options_effects,
	m_options_colorcontrol
};

extern enum m_state_e m_state;
extern char m_return_reason[32];

//
// menus
//
void M_Init (void);
void M_Keydown (int key);
void M_Draw (void);
void M_ToggleMenu_f (void);

#endif

