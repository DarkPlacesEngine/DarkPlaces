/*
	in_svgalib.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "quakedef.h"
#include "sys.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <vga.h>
#include <vgakeyboard.h>
#include <vgamouse.h>


static int	UseKeyboard = 1;
static int	UseMouse = 1;
static int	in_svgalib_inited = 0;

static unsigned char scantokey[128];
static int	mouse_buttons;
static int	mouse_buttonstate;
static int	mouse_oldbuttonstate;
static float	mouse_x, mouse_y;
static float	old_mouse_x, old_mouse_y;
static int	mx, my;

static void IN_init_kb(void);
static void IN_init_mouse(void);

cvar_t m_filter = {"m_filter","0"};

static void keyhandler(int scancode, int state)
{
	int sc;

	sc = scancode & 0x7f;
#if 0
	Con_Printf("scancode=%x (%d%s)\n", scancode, sc, scancode&0x80?"+128":"");
#endif
	Key_Event(scantokey[sc], state == KEY_EVENTPRESS);
}


static void mousehandler(int buttonstate, int dx, int dy, int dz, int drx, int dry, int drz)
{
	mouse_buttonstate = buttonstate;
	mx += dx;
	my += dy;
	if (drx > 0) {
		Key_Event(K_MWHEELUP, 1);
		Key_Event(K_MWHEELUP, 0);
	} else if (drx < 0) {
		Key_Event(K_MWHEELDOWN, 1);
		Key_Event(K_MWHEELDOWN, 0);
	}
}


void Force_CenterView_f(void)
{
	cl.viewangles[PITCH] = 0;
}


void IN_Init(void)
{
	if (COM_CheckParm("-nokbd")) UseKeyboard = 0;
	if (COM_CheckParm("-nomouse")) UseMouse = 0;

	if (UseKeyboard)
		IN_init_kb();
	if (UseMouse)
		IN_init_mouse();

	in_svgalib_inited = 1;
}

static void IN_init_kb(void)
{
	int i;

	for (i=0 ; i<128 ; i++) {
		scantokey[i] = ' ';
	}

	scantokey[  1] = K_ESCAPE;
	scantokey[  2] = '1';
	scantokey[  3] = '2';
	scantokey[  4] = '3';
	scantokey[  5] = '4';
	scantokey[  6] = '5';
	scantokey[  7] = '6';
	scantokey[  8] = '7';
	scantokey[  9] = '8';
	scantokey[ 10] = '9';
	scantokey[ 11] = '0';
	scantokey[ 12] = '-';
	scantokey[ 13] = '=';
	scantokey[ 14] = K_BACKSPACE;
	scantokey[ 15] = K_TAB;
	scantokey[ 16] = 'q';
	scantokey[ 17] = 'w';
	scantokey[ 18] = 'e';
	scantokey[ 19] = 'r';
	scantokey[ 20] = 't';
	scantokey[ 21] = 'y';
	scantokey[ 22] = 'u';
	scantokey[ 23] = 'i';
	scantokey[ 24] = 'o';
	scantokey[ 25] = 'p';
	scantokey[ 26] = '[';
	scantokey[ 27] = ']';
	scantokey[ 28] = K_ENTER;
	scantokey[ 29] = K_CTRL;	/*left */
	scantokey[ 30] = 'a';
	scantokey[ 31] = 's';
	scantokey[ 32] = 'd';
	scantokey[ 33] = 'f';
	scantokey[ 34] = 'g';
	scantokey[ 35] = 'h';
	scantokey[ 36] = 'j';
	scantokey[ 37] = 'k';
	scantokey[ 38] = 'l';
	scantokey[ 39] = ';';
	scantokey[ 40] = '\'';
	scantokey[ 41] = '`';
	scantokey[ 42] = K_SHIFT;	/*left */
	scantokey[ 43] = '\\';
	scantokey[ 44] = 'z';
	scantokey[ 45] = 'x';
	scantokey[ 46] = 'c';
	scantokey[ 47] = 'v';
	scantokey[ 48] = 'b';
	scantokey[ 49] = 'n';
	scantokey[ 50] = 'm';
	scantokey[ 51] = ',';
	scantokey[ 52] = '.';
	scantokey[ 53] = '/';
	scantokey[ 54] = K_SHIFT;	/*right */
	scantokey[ 55] = KP_MULTIPLY;
	scantokey[ 56] = K_ALT;		/*left */
	scantokey[ 57] = ' ';
	scantokey[ 58] = K_CAPSLOCK;
	scantokey[ 59] = K_F1;
	scantokey[ 60] = K_F2;
	scantokey[ 61] = K_F3;
	scantokey[ 62] = K_F4;
	scantokey[ 63] = K_F5;
	scantokey[ 64] = K_F6;
	scantokey[ 65] = K_F7;
	scantokey[ 66] = K_F8;
	scantokey[ 67] = K_F9;
	scantokey[ 68] = K_F10;
	scantokey[ 69] = KP_NUMLCK;
	scantokey[ 70] = K_SCRLCK;
	scantokey[ 71] = KP_HOME;
	scantokey[ 72] = KP_UPARROW;
	scantokey[ 73] = KP_PGUP;
	scantokey[ 74] = KP_MINUS;
	scantokey[ 75] = KP_LEFTARROW;
	scantokey[ 76] = KP_5;
	scantokey[ 77] = KP_RIGHTARROW;
	scantokey[ 79] = KP_END;
	scantokey[ 78] = KP_PLUS;
	scantokey[ 80] = KP_DOWNARROW;
	scantokey[ 81] = KP_PGDN;
	scantokey[ 82] = KP_INS;
	scantokey[ 83] = KP_DEL;
	/* 84 to 86 not used */
	scantokey[ 87] = K_F11;
	scantokey[ 88] = K_F12;
	/* 89 to 95 not used */
	scantokey[ 96] = KP_ENTER;	/* keypad enter */
	scantokey[ 97] = K_CTRL;	/* right */
	scantokey[ 98] = KP_DIVIDE;
	scantokey[ 99] = K_PRNTSCR;	/* print screen */
	scantokey[100] = K_ALT;		/* right */

	scantokey[101] = K_PAUSE;	/* break */
	scantokey[102] = K_HOME;
	scantokey[103] = K_UPARROW;
	scantokey[104] = K_PGUP;
	scantokey[105] = K_LEFTARROW;
	scantokey[106] = K_RIGHTARROW;
	scantokey[107] = K_END;
	scantokey[108] = K_DOWNARROW;
	scantokey[109] = K_PGDN;
	scantokey[110] = K_INS;
	scantokey[111] = K_DEL;
	scantokey[119] = K_PAUSE;

	if (keyboard_init()) {
		Sys_Error("keyboard_init() failed");
	}
	keyboard_seteventhandler(keyhandler);
}

static void IN_init_mouse(void)
{
	int mtype;
	char *mousedev;
	int mouserate = MOUSE_DEFAULTSAMPLERATE;

	Cvar_RegisterVariable (&m_filter);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);

	mouse_buttons = 3;

	mtype = vga_getmousetype();

	mousedev = "/dev/mouse";
	if (getenv("MOUSEDEV")) mousedev = getenv("MOUSEDEV");
	if (COM_CheckParm("-mdev")) {
		mousedev = com_argv[COM_CheckParm("-mdev")+1];
	}

	if (getenv("MOUSERATE")) mouserate = atoi(getenv("MOUSERATE"));
	if (COM_CheckParm("-mrate")) {
		mouserate = atoi(com_argv[COM_CheckParm("-mrate")+1]);
	}

#if 0
	printf("Mouse: dev=%s,type=%s,speed=%d\n",
		mousedev, mice[mtype].name, mouserate);
#endif
	if (mouse_init(mousedev, mtype, mouserate)) {
		Con_Printf("No mouse found\n");
		UseMouse = 0;
	} else{
		mouse_seteventhandler((void*)mousehandler);
	}
}

void IN_Shutdown(void)
{
	Con_Printf("IN_Shutdown\n");

	if (UseMouse) mouse_close();
	if (UseKeyboard) keyboard_close();
	in_svgalib_inited = 0;
}


void Sys_SendKeyEvents(void)
{
	if (!in_svgalib_inited) return;

	if (UseKeyboard) {
		while ((keyboard_update()));
	}
}


void IN_Commands(void)
{
	if (UseMouse)
	{
		/* Poll mouse values */
		while (mouse_update())
			;

		/* Perform button actions */
		if ((mouse_buttonstate & MOUSE_LEFTBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_LEFTBUTTON))
			Key_Event (K_MOUSE1, true);
		else if (!(mouse_buttonstate & MOUSE_LEFTBUTTON) &&
			(mouse_oldbuttonstate & MOUSE_LEFTBUTTON))
			Key_Event (K_MOUSE1, false);

		if ((mouse_buttonstate & MOUSE_RIGHTBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_RIGHTBUTTON))
			Key_Event (K_MOUSE2, true);
		else if (!(mouse_buttonstate & MOUSE_RIGHTBUTTON) &&
			(mouse_oldbuttonstate & MOUSE_RIGHTBUTTON))
			Key_Event (K_MOUSE2, false);

		if ((mouse_buttonstate & MOUSE_MIDDLEBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_MIDDLEBUTTON))
			Key_Event (K_MOUSE3, true);
		else if (!(mouse_buttonstate & MOUSE_MIDDLEBUTTON) &&
			(mouse_oldbuttonstate & MOUSE_MIDDLEBUTTON))
			Key_Event (K_MOUSE3, false);

		mouse_oldbuttonstate = mouse_buttonstate;
	}
}


void IN_Move(usercmd_t *cmd)
{
	int mouselook = (in_mlook.state & 1) || freelook.value;
	if (!UseMouse) return;

	/* Poll mouse values */
	while (mouse_update())
		;

	if (m_filter.value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}
	old_mouse_x = mx;
	old_mouse_y = my;
	/* Clear for next update */
	mx = my = 0;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	/* Add mouse X/Y movement to cmd */
	if ( (in_strafe.state & 1) || (lookstrafe.value && mouselook))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mouselook) V_StopPitchDrift();

	// LordHavoc: changed limits on pitch from -70 to 80, to -90 to 90
	if (mouselook && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 90)
			cl.viewangles[PITCH] = 90;
		if (cl.viewangles[PITCH] < -90)
			cl.viewangles[PITCH] = -90;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
}

void IN_HandlePause (qboolean pause)
{
}
