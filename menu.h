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

#define M_PROG_FILENAME "menu.dat"
#define M_NAME	"menu"

enum m_state_e {
	m_none,
	m_main,
	m_demo,
	m_singleplayer,
	m_transfusion_episode,
	m_transfusion_skill,
	m_load,
	m_save,
	m_multiplayer,
	m_setup,
	m_options,
	m_video,
	m_keys,
	m_help,
	m_credits,
	m_quit,
	m_lanconfig,
	m_gameoptions,
	m_slist,
	m_options_effects,
	m_options_graphics,
	m_options_colorcontrol,
	m_reset,
	m_modlist
};

extern enum m_state_e m_state;
extern char m_return_reason[128];
void M_Update_Return_Reason(const char *s);

/*
// hard-coded menus
//
void M_Init (void);
void M_KeyEvent (int key);
void M_Draw (void);
void M_ToggleMenu (int mode);

//
// menu prog menu
//
void MP_Init (void);
void MP_KeyEvent (int key);
void MP_Draw (void);
void MP_ToggleMenu (int mode);
void MP_Shutdown (void);*/

//
// menu router
//

void MR_Init_Commands (void);
void MR_Init (void);
void MR_Restart (void);
extern void (*MR_KeyEvent) (int key, int ascii, qboolean downevent);
extern void (*MR_Draw) (void);
extern void (*MR_ToggleMenu) (int mode);
extern void (*MR_Shutdown) (void);
extern void (*MR_NewMap) (void);

typedef struct video_resolution_s
{
	const char *type;
	int width, height;
	int conwidth, conheight;
	double pixelheight; ///< pixel aspect
}
video_resolution_t;
extern video_resolution_t *video_resolutions;
extern int video_resolutions_count;
extern video_resolution_t video_resolutions_hardcoded[];
extern int video_resolutions_hardcoded_count;
#endif

