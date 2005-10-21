/*
	$RCSfile$

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifndef __KEYS_H
#define __KEYS_H

#include "qtypes.h"

//
// these are the key numbers that should be passed to Key_Event
//
extern enum keynum_e
{
	K_TAB			= 9,
	K_ENTER			= 13,
	K_ESCAPE		= 27,
	K_SPACE			= 32,

	// normal keys should be passed as lowercased ascii

	K_BACKSPACE		= 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_PAUSE,

	K_NUMLOCK,
	K_CAPSLOCK,
	K_SCROLLOCK,

	K_KP_0,
	K_KP_INS = K_KP_0,
	K_KP_1,
	K_KP_END = K_KP_1,
	K_KP_2,
	K_KP_DOWNARROW = K_KP_2,
	K_KP_3,
	K_KP_PGDN = K_KP_3,
	K_KP_4,
	K_KP_LEFTARROW = K_KP_4,
	K_KP_5,
	K_KP_6,
	K_KP_RIGHTARROW = K_KP_6,
	K_KP_7,
	K_KP_HOME = K_KP_7,
	K_KP_8,
	K_KP_UPARROW = K_KP_8,
	K_KP_9,
	K_KP_PGUP = K_KP_9,
	K_KP_PERIOD,
	K_KP_DEL = K_KP_PERIOD,
	K_KP_DIVIDE,
	K_KP_SLASH = K_KP_DIVIDE,
	K_KP_MULTIPLY,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_ENTER,
	K_KP_EQUALS,

	// mouse buttons generate virtual keys

	K_MOUSE1 = 512,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MWHEELUP		= K_MOUSE4,
	K_MOUSE5,
	K_MWHEELDOWN	= K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MOUSE9,
	K_MOUSE10,
	K_MOUSE11,
	K_MOUSE12,
	K_MOUSE13,
	K_MOUSE14,
	K_MOUSE15,
	K_MOUSE16,

//
// joystick buttons
//
	K_JOY1 = 768,
	K_JOY2,
	K_JOY3,
	K_JOY4,
	K_JOY5,
	K_JOY6,
	K_JOY7,
	K_JOY8,
	K_JOY9,
	K_JOY10,
	K_JOY11,
	K_JOY12,
	K_JOY13,
	K_JOY14,
	K_JOY15,
	K_JOY16,

//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28,
	K_AUX29,
	K_AUX30,
	K_AUX31,
	K_AUX32,

}
keynum_t;

typedef enum keydest_e { key_game, key_message, key_menu } keydest_t;

#define MAX_INPUTLINES 32
#define MAX_INPUTLINE 256
#define MAX_BINDMAPS 8
#define MAX_KEYS 1024
extern	int			edit_line;
extern	int			history_line;
extern	char		key_lines[MAX_INPUTLINES][MAX_INPUTLINE];
extern	int			key_linepos;
extern	qboolean	key_insert;	// insert key toggle (for editing)
extern	keydest_t	key_dest;
// key_consoleactive bits
// user wants console (halfscreen)
#define KEY_CONSOLEACTIVE_USER 1
// console forced because there's nothing else active (fullscreen)
#define KEY_CONSOLEACTIVE_FORCED 4
extern	int			key_consoleactive;
extern	char		*keybindings[MAX_BINDMAPS][MAX_KEYS];

extern void Key_ClearEditLine(int edit_line);
extern qboolean chat_team;
extern char chat_buffer[256];
extern unsigned int chat_bufferlen;

void Key_WriteBindings(qfile_t *f);
void Key_Init(void);
void Key_Init_Cvars(void);
void Key_Event(int key, char ascii, qboolean down);
void Key_ClearStates (void);
void Key_SetBinding (int keynum, int bindmap, const char *binding);

#endif // __KEYS_H

