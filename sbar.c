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
// sbar.c -- status bar code

#include "quakedef.h"

typedef struct
{
	char name[32];
}
sbarpic_t;

static sbarpic_t sbarpics[256];
static int numsbarpics;

static sbarpic_t *Sbar_NewPic(const char *name)
{
	strcpy(sbarpics[numsbarpics].name, name);
	// precache it
	// FIXME: precache on every renderer restart (or move this to client)
	Draw_CachePic(sbarpics[numsbarpics].name);
	return sbarpics + (numsbarpics++);
}

sbarpic_t *sb_disc;

#define STAT_MINUS 10 // num frame for '-' stats digit
sbarpic_t *sb_nums[2][11];
sbarpic_t *sb_colon, *sb_slash;
sbarpic_t *sb_ibar;
sbarpic_t *sb_sbar;
sbarpic_t *sb_scorebar;
// AK only used by NEX(and only if everybody agrees)
sbarpic_t *sb_sbar_overlay;

// AK changed the bound to 9
sbarpic_t *sb_weapons[7][9]; // 0 is active, 1 is owned, 2-5 are flashes
sbarpic_t *sb_ammo[4];
sbarpic_t *sb_sigil[4];
sbarpic_t *sb_armor[3];
sbarpic_t *sb_items[32];

// 0-4 are based on health (in 20 increments)
// 0 is static, 1 is temporary animation
sbarpic_t *sb_faces[5][2];

sbarpic_t *sb_face_invis;
sbarpic_t *sb_face_quad;
sbarpic_t *sb_face_invuln;
sbarpic_t *sb_face_invis_invuln;

qboolean sb_showscores;

int sb_lines;			// scan lines to draw

sbarpic_t *rsb_invbar[2];
sbarpic_t *rsb_weapons[5];
sbarpic_t *rsb_items[2];
sbarpic_t *rsb_ammo[3];
sbarpic_t *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
sbarpic_t *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
sbarpic_t *hsb_items[2];

cvar_t	showfps = {CVAR_SAVE, "showfps", "0"};

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_DeathmatchOverlay (void);
void Sbar_IntermissionOverlay (void);
void Sbar_FinaleOverlay (void);


/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;
	sb_showscores = true;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{
	sb_showscores = false;
}

void sbar_start(void)
{
	int i;

	numsbarpics = 0;
		
	sb_disc = Sbar_NewPic("gfx/disc");

	for (i = 0;i < 10;i++)
		sb_nums[0][i] = Sbar_NewPic (va("gfx/num_%i",i));

	sb_nums[0][10] = Sbar_NewPic ("gfx/num_minus");
	sb_nums[1][10] = Sbar_NewPic ("gfx/anum_minus");

	sb_colon = Sbar_NewPic ("gfx/num_colon");
	sb_slash = Sbar_NewPic ("gfx/num_slash");

	//AK NX uses its own hud
	if(gamemode == GAME_NEXUIZ)
	{
		sb_ammo[0] = Sbar_NewPic ("gfx/sb_shells");
		sb_ammo[1] = Sbar_NewPic ("gfx/sb_bullets");
		sb_ammo[2] = Sbar_NewPic ("gfx/sb_rocket");
		sb_ammo[3] = Sbar_NewPic ("gfx/sb_cells");
		
		sb_items[2] = Sbar_NewPic ("gfx/sb_slowmo");
		sb_items[3] = Sbar_NewPic ("gfx/sb_invinc");
		sb_items[4] = Sbar_NewPic ("gfx/sb_energy");
		sb_items[5] = Sbar_NewPic ("gfx/sb_str");
		
		sb_sbar = Sbar_NewPic("gfx/sbar");
		sb_sbar_overlay = Sbar_NewPic("gfx/sbar_overlay");

		for(i = 0; i < 9;i++)
			sb_weapons[0][i] = Sbar_NewPic(va("gfx/inv_weapon%i",i));

		return;
	}	

	sb_weapons[0][0] = Sbar_NewPic ("gfx/inv_shotgun");
	sb_weapons[0][1] = Sbar_NewPic ("gfx/inv_sshotgun");
	sb_weapons[0][2] = Sbar_NewPic ("gfx/inv_nailgun");
	sb_weapons[0][3] = Sbar_NewPic ("gfx/inv_snailgun");
	sb_weapons[0][4] = Sbar_NewPic ("gfx/inv_rlaunch");
	sb_weapons[0][5] = Sbar_NewPic ("gfx/inv_srlaunch");
	sb_weapons[0][6] = Sbar_NewPic ("gfx/inv_lightng");

	sb_weapons[1][0] = Sbar_NewPic ("gfx/inv2_shotgun");
	sb_weapons[1][1] = Sbar_NewPic ("gfx/inv2_sshotgun");
	sb_weapons[1][2] = Sbar_NewPic ("gfx/inv2_nailgun");
	sb_weapons[1][3] = Sbar_NewPic ("gfx/inv2_snailgun");
	sb_weapons[1][4] = Sbar_NewPic ("gfx/inv2_rlaunch");
	sb_weapons[1][5] = Sbar_NewPic ("gfx/inv2_srlaunch");
	sb_weapons[1][6] = Sbar_NewPic ("gfx/inv2_lightng");

	for (i = 0;i < 5;i++)
	{
		sb_weapons[2+i][0] = Sbar_NewPic (va("gfx/inva%i_shotgun",i+1));
		sb_weapons[2+i][1] = Sbar_NewPic (va("gfx/inva%i_sshotgun",i+1));
		sb_weapons[2+i][2] = Sbar_NewPic (va("gfx/inva%i_nailgun",i+1));
		sb_weapons[2+i][3] = Sbar_NewPic (va("gfx/inva%i_snailgun",i+1));
		sb_weapons[2+i][4] = Sbar_NewPic (va("gfx/inva%i_rlaunch",i+1));
		sb_weapons[2+i][5] = Sbar_NewPic (va("gfx/inva%i_srlaunch",i+1));
		sb_weapons[2+i][6] = Sbar_NewPic (va("gfx/inva%i_lightng",i+1));
	}

	sb_ammo[0] = Sbar_NewPic ("gfx/sb_shells");
	sb_ammo[1] = Sbar_NewPic ("gfx/sb_nails");
	sb_ammo[2] = Sbar_NewPic ("gfx/sb_rocket");
	sb_ammo[3] = Sbar_NewPic ("gfx/sb_cells");

	sb_armor[0] = Sbar_NewPic ("gfx/sb_armor1");
	sb_armor[1] = Sbar_NewPic ("gfx/sb_armor2");
	sb_armor[2] = Sbar_NewPic ("gfx/sb_armor3");

	sb_items[0] = Sbar_NewPic ("gfx/sb_key1");
	sb_items[1] = Sbar_NewPic ("gfx/sb_key2");
	sb_items[2] = Sbar_NewPic ("gfx/sb_invis");
	sb_items[3] = Sbar_NewPic ("gfx/sb_invuln");
	sb_items[4] = Sbar_NewPic ("gfx/sb_suit");
	sb_items[5] = Sbar_NewPic ("gfx/sb_quad");

	sb_sigil[0] = Sbar_NewPic ("gfx/sb_sigil1");
	sb_sigil[1] = Sbar_NewPic ("gfx/sb_sigil2");
	sb_sigil[2] = Sbar_NewPic ("gfx/sb_sigil3");
	sb_sigil[3] = Sbar_NewPic ("gfx/sb_sigil4");

	sb_faces[4][0] = Sbar_NewPic ("gfx/face1");
	sb_faces[4][1] = Sbar_NewPic ("gfx/face_p1");
	sb_faces[3][0] = Sbar_NewPic ("gfx/face2");
	sb_faces[3][1] = Sbar_NewPic ("gfx/face_p2");
	sb_faces[2][0] = Sbar_NewPic ("gfx/face3");
	sb_faces[2][1] = Sbar_NewPic ("gfx/face_p3");
	sb_faces[1][0] = Sbar_NewPic ("gfx/face4");
	sb_faces[1][1] = Sbar_NewPic ("gfx/face_p4");
	sb_faces[0][0] = Sbar_NewPic ("gfx/face5");
	sb_faces[0][1] = Sbar_NewPic ("gfx/face_p5");

	sb_face_invis = Sbar_NewPic ("gfx/face_invis");
	sb_face_invuln = Sbar_NewPic ("gfx/face_invul2");
	sb_face_invis_invuln = Sbar_NewPic ("gfx/face_inv2");
	sb_face_quad = Sbar_NewPic ("gfx/face_quad");

	sb_sbar = Sbar_NewPic ("gfx/sbar");
	sb_ibar = Sbar_NewPic ("gfx/ibar");
	sb_scorebar = Sbar_NewPic ("gfx/scorebar");

//MED 01/04/97 added new hipnotic weapons
	if (gamemode == GAME_HIPNOTIC)
	{
		hsb_weapons[0][0] = Sbar_NewPic ("gfx/inv_laser");
		hsb_weapons[0][1] = Sbar_NewPic ("gfx/inv_mjolnir");
		hsb_weapons[0][2] = Sbar_NewPic ("gfx/inv_gren_prox");
		hsb_weapons[0][3] = Sbar_NewPic ("gfx/inv_prox_gren");
		hsb_weapons[0][4] = Sbar_NewPic ("gfx/inv_prox");

		hsb_weapons[1][0] = Sbar_NewPic ("gfx/inv2_laser");
		hsb_weapons[1][1] = Sbar_NewPic ("gfx/inv2_mjolnir");
		hsb_weapons[1][2] = Sbar_NewPic ("gfx/inv2_gren_prox");
		hsb_weapons[1][3] = Sbar_NewPic ("gfx/inv2_prox_gren");
		hsb_weapons[1][4] = Sbar_NewPic ("gfx/inv2_prox");

		for (i = 0;i < 5;i++)
		{
			hsb_weapons[2+i][0] = Sbar_NewPic (va("gfx/inva%i_laser",i+1));
			hsb_weapons[2+i][1] = Sbar_NewPic (va("gfx/inva%i_mjolnir",i+1));
			hsb_weapons[2+i][2] = Sbar_NewPic (va("gfx/inva%i_gren_prox",i+1));
			hsb_weapons[2+i][3] = Sbar_NewPic (va("gfx/inva%i_prox_gren",i+1));
			hsb_weapons[2+i][4] = Sbar_NewPic (va("gfx/inva%i_prox",i+1));
		}

		hsb_items[0] = Sbar_NewPic ("gfx/sb_wsuit");
		hsb_items[1] = Sbar_NewPic ("gfx/sb_eshld");
	}
	else if (gamemode == GAME_ROGUE)
	{
		rsb_invbar[0] = Sbar_NewPic ("gfx/r_invbar1");
		rsb_invbar[1] = Sbar_NewPic ("gfx/r_invbar2");

		rsb_weapons[0] = Sbar_NewPic ("gfx/r_lava");
		rsb_weapons[1] = Sbar_NewPic ("gfx/r_superlava");
		rsb_weapons[2] = Sbar_NewPic ("gfx/r_gren");
		rsb_weapons[3] = Sbar_NewPic ("gfx/r_multirock");
		rsb_weapons[4] = Sbar_NewPic ("gfx/r_plasma");

		rsb_items[0] = Sbar_NewPic ("gfx/r_shield1");
		rsb_items[1] = Sbar_NewPic ("gfx/r_agrav1");

// PGM 01/19/97 - team color border
		rsb_teambord = Sbar_NewPic ("gfx/r_teambord");
// PGM 01/19/97 - team color border

		rsb_ammo[0] = Sbar_NewPic ("gfx/r_ammolava");
		rsb_ammo[1] = Sbar_NewPic ("gfx/r_ammomulti");
		rsb_ammo[2] = Sbar_NewPic ("gfx/r_ammoplasma");
	}
}

void sbar_shutdown(void)
{
}

void sbar_newmap(void)
{
}

void Sbar_Init (void)
{
	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);
	Cvar_RegisterVariable (&showfps);

	R_RegisterModule("sbar", sbar_start, sbar_shutdown, sbar_newmap);
}


//=============================================================================

// drawing routines are relative to the status bar location

int sbar_x, sbar_y;

/*
=============
Sbar_DrawPic
=============
*/
void Sbar_DrawPic (int x, int y, sbarpic_t *sbarpic)
{
	DrawQ_Pic (sbar_x + x, sbar_y + y, sbarpic->name, 0, 0, 1, 1, 1, 1, 0);
}

void Sbar_DrawAlphaPic (int x, int y, sbarpic_t *sbarpic, float alpha)
{
	DrawQ_Pic (sbar_x + x, sbar_y + y, sbarpic->name, 0, 0, 1, 1, 1, alpha, 0);
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{
	DrawQ_String (sbar_x + x + 4 , sbar_y + y, va("%c", num), 0, 8, 8, 1, 1, 1, 1, 0);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{
	DrawQ_String (sbar_x + x, sbar_y + y, str, 0, 8, 8, 1, 1, 1, 1, 0);
}

/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char str[32], *ptr;
	int l, frame;

	l = sprintf(str, "%i", num);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Sbar_DrawPic (x, y, sb_nums[color][frame]);
		x += 24;

		ptr++;
	}
}

/*
=============
Sbar_DrawXNum
=============
*/

void Sbar_DrawXNum (int x, int y, int num, int digits, int lettersize, float r, float g, float b, float a, int flags)
{
	char str[32], *ptr;
	int l, frame;
	
	l = sprintf(str, "%i", num);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l) * lettersize;
		
	while (*ptr)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';
		
		DrawQ_Pic (sbar_x + x, sbar_y + y, sb_nums[0][frame]->name,lettersize,lettersize,r,g,b,a,flags);
		x += lettersize;

		ptr++;
	}
}

//=============================================================================


/*
===============
Sbar_SortFrags
===============
*/
static int fragsort[MAX_SCOREBOARD];
static int scoreboardlines;
void Sbar_SortFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<cl.maxclients ; i++)
	{
		if (cl.scores[i].name[0])
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

/*
===============
Sbar_SoloScoreboard
===============
*/
void Sbar_SoloScoreboard (void)
{
	char	str[80];
	int		minutes, seconds, tens, units;
	int		l;

	sprintf (str,"Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	sprintf (str,"Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

// time
	minutes = cl.time / 60;
	seconds = cl.time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	sprintf (str,"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

// draw level name
	l = strlen (cl.levelname);
	Sbar_DrawString (232 - l*4, 12, cl.levelname);
}

/*
===============
Sbar_DrawScoreboard
===============
*/
void Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay ();
}

//=============================================================================

// AK to make DrawInventory smaller
static void Sbar_DrawWeapon(int nr, float fade, int active)
{
	// width = 300, height = 100
	const int w_width = 300, w_height = 100, w_space = 10, font_size = 10;
	const float w_scale = 0.4;

	DrawQ_Pic(vid.conwidth - (w_width + w_space) * w_scale, (w_height + w_space) * w_scale * nr + w_space, sb_weapons[0][nr]->name, w_width * w_scale, w_height * w_scale, (active) ? 1 : 0.6, active ? 1 : 0.6, active ? 1 : 1, fade, DRAWFLAG_ADDITIVE);
	DrawQ_String(vid.conwidth - (w_space + font_size ), (w_height + w_space) * w_scale * nr + w_space, va("%i",nr+1), 0, font_size, font_size, 1, 0, 0, fade, 0);

	if (active)
		DrawQ_Fill(vid.conwidth - (w_width + w_space) * w_scale, (w_height + w_space) * w_scale * nr + w_space, w_width * w_scale, w_height * w_scale, 0.3, 0.3, 0.3, fade, DRAWFLAG_ADDITIVE);
}

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (void)
{
	int i;
	char num[6];
	float time;
	int flashon;
	// AK 2003
	float fade;

	if(gamemode == GAME_NEXUIZ)
	{
		num[0] = cl.stats[STAT_ACTIVEWEAPON];
		// we have a max time 2s (min time = 0)
		if ((time = cl.time - cl.weapontime) > 2)
			return;

		fade = (1.0 - 0.5 * time);
		fade *= fade;
		for (i = 0; i < 8;i++)
		{
			if (!(cl.items & (1 << i)))
				continue;
			Sbar_DrawWeapon(i + 1, fade, (i == num[0]));
		}

		if(!(cl.items & (1<<12)))
			return;
		Sbar_DrawWeapon(0, fade, (num[0] == 12));
		return;
	}

	if (gamemode == GAME_ROGUE)
	{
		if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
			Sbar_DrawAlphaPic (0, -24, rsb_invbar[0], 0.4);
		else
			Sbar_DrawAlphaPic (0, -24, rsb_invbar[1], 0.4);
	}
	else
		Sbar_DrawAlphaPic (0, -24, sb_ibar, 0.4);

	// weapons
	for (i=0 ; i<7 ; i++)
	{
		if (cl.items & (IT_SHOTGUN<<i) )
		{
			time = cl.item_gettime[i];
			flashon = (int)((cl.time - time)*10);
			if (flashon >= 10)
			{
				if ( cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN<<i)  )
					flashon = 1;
				else
					flashon = 0;
			}
			else
				flashon = (flashon%5) + 2;

			Sbar_DrawAlphaPic (i*24, -16, sb_weapons[flashon][i], 0.4);
		}
	}

	// MED 01/04/97
	// hipnotic weapons
	if (gamemode == GAME_HIPNOTIC)
	{
		int grenadeflashing=0;
		for (i=0 ; i<4 ; i++)
		{
			if (cl.items & (1<<hipweapons[i]) )
			{
				time = cl.item_gettime[hipweapons[i]];
				flashon = (int)((cl.time - time)*10);
				if (flashon >= 10)
				{
					if ( cl.stats[STAT_ACTIVEWEAPON] == (1<<hipweapons[i])  )
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon%5) + 2;

				// check grenade launcher
				if (i==2)
				{
					if (cl.items & HIT_PROXIMITY_GUN)
					{
						if (flashon)
						{
							grenadeflashing = 1;
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
						}
					}
				}
				else if (i==3)
				{
					if (cl.items & (IT_SHOTGUN<<4))
					{
						if (!grenadeflashing)
							Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
					}
					else
						Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
				}
				else
					Sbar_DrawPic (176 + (i*24), -16, hsb_weapons[flashon][i]);
			}
		}
	}

	if (gamemode == GAME_ROGUE)
	{
		// check for powered up weapon.
		if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
			for (i=0;i<5;i++)
				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
					Sbar_DrawPic ((i+2)*24, -16, rsb_weapons[i]);
	}

	// ammo counts
	for (i=0 ; i<4 ; i++)
	{
		sprintf (num, "%3i",cl.stats[STAT_SHELLS+i] );
		if (num[0] != ' ')
			Sbar_DrawCharacter ( (6*i+1)*8 - 2, -24, 18 + num[0] - '0');
		if (num[1] != ' ')
			Sbar_DrawCharacter ( (6*i+2)*8 - 2, -24, 18 + num[1] - '0');
		if (num[2] != ' ')
			Sbar_DrawCharacter ( (6*i+3)*8 - 2, -24, 18 + num[2] - '0');
	}

	// items
	for (i=0 ; i<6 ; i++)
		if (cl.items & (1<<(17+i)))
		{
			//MED 01/04/97 changed keys
			if (gamemode != GAME_HIPNOTIC || (i>1))
				Sbar_DrawPic (192 + i*16, -16, sb_items[i]);
		}

	//MED 01/04/97 added hipnotic items
	// hipnotic items
	if (gamemode == GAME_HIPNOTIC)
	{
		for (i=0 ; i<2 ; i++)
			if (cl.items & (1<<(24+i)))
				Sbar_DrawPic (288 + i*16, -16, hsb_items[i]);
	}

	if (gamemode == GAME_ROGUE)
	{
		// new rogue items
		for (i=0 ; i<2 ; i++)
			if (cl.items & (1<<(29+i)))
				Sbar_DrawPic (288 + i*16, -16, rsb_items[i]);
	}
	else
	{
		// sigils
		for (i=0 ; i<4 ; i++)
			if (cl.items & (1<<(28+i)))
				Sbar_DrawPic (320-32 + i*8, -16, sb_sigil[i]);
	}
}

//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{
	int i, k, l, x, f;
	char num[12];
	scoreboard_t *s;
	qbyte *c;

	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23 * 8;

	for (i = 0;i < l;i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		c = (qbyte *)&palette_complete[(s->colors & 0xf0) + 8];
		DrawQ_Fill (sbar_x + x + 10, sbar_y     - 23, 28, 4, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&palette_complete[((s->colors & 15)<<4) + 8];
		DrawQ_Fill (sbar_x + x + 10, sbar_y + 4 - 23, 28, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		Sbar_DrawCharacter (x +  8, -24, num[0]);
		Sbar_DrawCharacter (x + 16, -24, num[1]);
		Sbar_DrawCharacter (x + 24, -24, num[2]);

		if (k == cl.viewentity - 1)
		{
			Sbar_DrawCharacter ( x      + 2, -24, 16);
			Sbar_DrawCharacter ( x + 32 - 4, -24, 17);
		}
		x += 32;
	}
}

//=============================================================================


/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (void)
{
	int f;

// PGM 01/19/97 - team color drawing
// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (gamemode == GAME_ROGUE && !cl.islocalgame && (teamplay.integer > 3) && (teamplay.integer < 7))
	{
		char num[12];
		scoreboard_t *s;
		qbyte *c;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		Sbar_DrawPic (112, 0, rsb_teambord);
		c = (qbyte *)&palette_complete[(s->colors & 0xf0) + 8];
		DrawQ_Fill (sbar_x + 113, vid.conheight-SBAR_HEIGHT+3, 22, 9, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&palette_complete[((s->colors & 15)<<4) + 8];
		DrawQ_Fill (sbar_x + 113, vid.conheight-SBAR_HEIGHT+12, 22, 9, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);

		// draw number
		f = s->frags;
		sprintf (num, "%3i",f);

		if ((s->colors & 0xf0)==0)
		{
			if (num[0] != ' ')
				Sbar_DrawCharacter(109, 3, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter(116, 3, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter(123, 3, 18 + num[2] - '0');
		}
		else
		{
			Sbar_DrawCharacter ( 109, 3, num[0]);
			Sbar_DrawCharacter ( 116, 3, num[1]);
			Sbar_DrawCharacter ( 123, 3, num[2]);
		}

		return;
	}
// PGM 01/19/97 - team color drawing

	if ( (cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY) ) == (IT_INVISIBILITY | IT_INVULNERABILITY) )
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
	else if (cl.items & IT_QUAD)
		Sbar_DrawPic (112, 0, sb_face_quad );
	else if (cl.items & IT_INVISIBILITY)
		Sbar_DrawPic (112, 0, sb_face_invis );
	else if (cl.items & IT_INVULNERABILITY)
		Sbar_DrawPic (112, 0, sb_face_invuln);
	else
	{
		f = cl.stats[STAT_HEALTH] / 20;
		f = bound(0, f, 4);
		Sbar_DrawPic (112, 0, sb_faces[f][cl.time <= cl.faceanimtime]);
	}
}

void Sbar_ShowFPS(void)
{
	if (showfps.integer)
	{
		int calc;
		char temp[32];
		float fps_x, fps_y, fps_scalex, fps_scaley;
		if (showfps.integer > 1)
		{
			static double currtime, frametimes[32];
			double newtime, total;
			int count, i;
			static int framecycle = 0;

			newtime = Sys_DoubleTime();
			frametimes[framecycle] = newtime - currtime;
			total = 0;
			count = 0;
			while(total < 0.2 && count < 32 && frametimes[i = (framecycle - count) & 31])
			{
				total += frametimes[i];
				count++;
			}
			framecycle++;
			framecycle &= 31;
			if (showfps.integer == 2)
				calc = (int) (((double) count / total) + 0.5);
			else // showfps 3, rapid update
				calc = (int) ((1.0 / (newtime - currtime)) + 0.5);
			currtime = newtime;
		}
		else
		{
			static double nexttime = 0, lasttime = 0;
			static int framerate = 0, framecount = 0;
			double newtime;
			newtime = Sys_DoubleTime();
			if (newtime < nexttime)
				framecount++;
			else
			{
				framerate = (int) (framecount / (newtime - lasttime) + 0.5);
				lasttime = newtime;
				nexttime = lasttime + 0.2;
				framecount = 1;
			}
			calc = framerate;
		}
		sprintf(temp, "%4i", calc);
		fps_scalex = 12;
		fps_scaley = 12;
		fps_x = vid.conwidth - (fps_scalex * strlen(temp));
		fps_y = vid.conheight - sb_lines/* - 8*/; // yes this might draw over the sbar
		if (fps_y > vid.conheight - fps_scaley)
			fps_y = vid.conheight - fps_scaley;
		DrawQ_Fill(fps_x, fps_y, fps_scalex * strlen(temp), fps_scaley, 0, 0, 0, 0.5, 0);
		DrawQ_String(fps_x, fps_y, temp, 0, fps_scalex, fps_scaley, 1, 1, 1, 1, 0);
	}
}

/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{
	if (scr_con_current == vid.conheight)
		return;		// console is full screen
	
	if (cl.intermission == 1)
	{
		Sbar_IntermissionOverlay();
		return;
	}
	else if (cl.intermission == 2)
	{
		Sbar_FinaleOverlay();
		return;
	}

	if (gamemode == GAME_NEXUIZ)
	{
		sbar_y = vid.conheight - 47;
		sbar_x = (vid.conwidth - 640)/2;
		
		if (sb_lines)
		{
			Sbar_DrawInventory(); 
			if (!cl.islocalgame)
				Sbar_DrawFrags ();
		}
		
		if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
		{
			Sbar_DrawAlphaPic (0, 0, sb_scorebar, 0.4);
			Sbar_DrawScoreboard ();
		}
		else if (sb_lines)
		{
			Sbar_DrawPic (0, 0, sb_sbar);
			
			// special items
			if (cl.items & IT_INVULNERABILITY)
			{
				Sbar_DrawNum (36, 0, 666, 3, 1);
				Sbar_DrawPic (0, 0, sb_disc);
			}

			// armor
			Sbar_DrawXNum ((340-3*24), 12, cl.stats[STAT_ARMOR], 3, 24, 0.6,0.7,0.8,1,0);
			
			// health
			if(cl.stats[STAT_HEALTH] > 100)
				Sbar_DrawXNum((154-3*24),12,cl.stats[STAT_HEALTH],3,24,1,1,1,1,0);
			else if(cl.stats[STAT_HEALTH] <= 25 && cl.time - (int)cl.time > 0.5)
				Sbar_DrawXNum((154-3*24),12,cl.stats[STAT_HEALTH],3,24,0.7,0,0,1,0);
			else	
				Sbar_DrawXNum((154-3*24),12,cl.stats[STAT_HEALTH],3,24,0.6,0.7,0.8,1,0);

			// AK dont draw ammo for the laser
			if(cl.stats[STAT_ACTIVEWEAPON] != 12)
			{
				if (cl.items & NEX_IT_SHELLS)
					Sbar_DrawPic (519, 0, sb_ammo[0]);
				else if (cl.items & NEX_IT_BULLETS)
					Sbar_DrawPic (519, 0, sb_ammo[1]);
				else if (cl.items & NEX_IT_ROCKETS)
					Sbar_DrawPic (519, 0, sb_ammo[2]);
				else if (cl.items & NEX_IT_CELLS)
					Sbar_DrawPic (519, 0, sb_ammo[3]);
				
				if(cl.stats[STAT_AMMO] <= 10) 
					Sbar_DrawXNum ((519-3*24), 12, cl.stats[STAT_AMMO], 3, 24, 0.7, 0,0,1,0);
				else
					Sbar_DrawXNum ((519-3*24), 12, cl.stats[STAT_AMMO], 3, 24, 0.6, 0.7,0.8,1,0);
				
			}

			DrawQ_Pic(sbar_x,sbar_y,sb_sbar_overlay->name,0,0,1,1,1,1,DRAWFLAG_MODULATE);
		}
	}
	else
	{
		sbar_y = vid.conheight - SBAR_HEIGHT;
		if (cl.gametype == GAME_DEATHMATCH)
			sbar_x = 0;
		else
			sbar_x = (vid.conwidth - 320)/2;

		if (sb_lines > 24)
		{
			if (gamemode != GAME_GOODVSBAD2)
				Sbar_DrawInventory ();
			if (!cl.islocalgame)
				Sbar_DrawFrags ();
		}

		if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
		{
			if (gamemode != GAME_GOODVSBAD2)
				Sbar_DrawAlphaPic (0, 0, sb_scorebar, 0.4);
			Sbar_DrawScoreboard ();
		}
		else if (sb_lines)
		{
			Sbar_DrawAlphaPic (0, 0, sb_sbar, 0.4);

			// keys (hipnotic only)
			//MED 01/04/97 moved keys here so they would not be overwritten
			if (gamemode == GAME_HIPNOTIC)
			{
				if (cl.items & IT_KEY1)
					Sbar_DrawPic (209, 3, sb_items[0]);
				if (cl.items & IT_KEY2)
					Sbar_DrawPic (209, 12, sb_items[1]);
			}
			// armor
			if (gamemode != GAME_GOODVSBAD2)
			{
				if (cl.items & IT_INVULNERABILITY)
				{
					Sbar_DrawNum (24, 0, 666, 3, 1);
					Sbar_DrawPic (0, 0, sb_disc);
				}
				else
				{
					if (gamemode == GAME_ROGUE)
					{
						Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
						if (cl.items & RIT_ARMOR3)
							Sbar_DrawPic (0, 0, sb_armor[2]);
						else if (cl.items & RIT_ARMOR2)
							Sbar_DrawPic (0, 0, sb_armor[1]);
						else if (cl.items & RIT_ARMOR1)
							Sbar_DrawPic (0, 0, sb_armor[0]);
					}
					else
					{
						Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
						if (cl.items & IT_ARMOR3)
							Sbar_DrawPic (0, 0, sb_armor[2]);
						else if (cl.items & IT_ARMOR2)
							Sbar_DrawPic (0, 0, sb_armor[1]);
						else if (cl.items & IT_ARMOR1)
							Sbar_DrawPic (0, 0, sb_armor[0]);
					}
				}
			}

			// face
			Sbar_DrawFace ();

			// health
			Sbar_DrawNum (154, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

			// ammo icon
			if (gamemode == GAME_ROGUE)
			{
				if (cl.items & RIT_SHELLS)
					Sbar_DrawPic (224, 0, sb_ammo[0]);
				else if (cl.items & RIT_NAILS)
					Sbar_DrawPic (224, 0, sb_ammo[1]);
				else if (cl.items & RIT_ROCKETS)
					Sbar_DrawPic (224, 0, sb_ammo[2]);
				else if (cl.items & RIT_CELLS)
					Sbar_DrawPic (224, 0, sb_ammo[3]);
				else if (cl.items & RIT_LAVA_NAILS)
					Sbar_DrawPic (224, 0, rsb_ammo[0]);
				else if (cl.items & RIT_PLASMA_AMMO)
					Sbar_DrawPic (224, 0, rsb_ammo[1]);
				else if (cl.items & RIT_MULTI_ROCKETS)
					Sbar_DrawPic (224, 0, rsb_ammo[2]);
			}
			else
			{
				if (cl.items & IT_SHELLS)
					Sbar_DrawPic (224, 0, sb_ammo[0]);
				else if (cl.items & IT_NAILS)
					Sbar_DrawPic (224, 0, sb_ammo[1]);
				else if (cl.items & IT_ROCKETS)
					Sbar_DrawPic (224, 0, sb_ammo[2]);
				else if (cl.items & IT_CELLS)
					Sbar_DrawPic (224, 0, sb_ammo[3]);
			}

			Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);

		}
	}

	if (vid.conwidth > 320 && cl.gametype == GAME_DEATHMATCH)
		Sbar_MiniDeathmatchOverlay ();

	Sbar_ShowFPS();

	R_Draw2DCrosshair();
}

//=============================================================================

/*
==================
Sbar_DeathmatchOverlay

==================
*/
float Sbar_PrintScoreboardItem(scoreboard_t *s, float x, float y)
{
	qbyte *c;
	if (s->name[0] || s->frags || s->colors || (s - cl.scores) == cl.playerentity - 1)
	{
		// draw colors behind score
		c = (qbyte *)&palette_complete[(s->colors & 0xf0) + 8];
		DrawQ_Fill(x + 8, y+1, 32, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&palette_complete[((s->colors & 15)<<4) + 8];
		DrawQ_Fill(x + 8, y+4, 32, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		// print the text
		DrawQ_String(x, y, va("%c%4i %s", (s - cl.scores) == cl.playerentity - 1 ? 13 : ' ', (int) s->frags, s->name), 0, 8, 8, 1, 1, 1, 1, 0);
		return 8;
	}
	else
		return 0;
}

void Sbar_DeathmatchOverlay (void)
{
	int i, x, y;
	cachepic_t *pic;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	DrawQ_Pic ((vid.conwidth - pic->width)/2, 8, "gfx/ranking.lmp", 0, 0, 1, 1, 1, 1, 0);

	// scores
	Sbar_SortFrags ();
	// draw the text
	x = (vid.conwidth - (6 + 15) * 8) / 2;
	y = 40;
	for (i = 0;i < scoreboardlines && y < vid.conheight;i++)
		y += Sbar_PrintScoreboardItem(cl.scores + fragsort[i], x, y);
}

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{
	int i, x, y, numlines;

	// decide where to print
	x = 0;
	// AK Nex wants its scores on the upper left
	if(gamemode == GAME_NEXUIZ)
		y = 0; 
	else
		y = vid.conheight - sb_lines;

	numlines = (vid.conheight - y) / 8;
	// give up if there isn't room
	if (x + (6 + 15) * 8 > vid.conwidth || numlines < 1)
		return;

	// scores
	Sbar_SortFrags ();

	//find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.playerentity - 1)
			break;

	if (i == scoreboardlines) // we're not there
		i = 0;
	else // figure out start
	{
		i -= numlines/2;
		i = bound(0, i, scoreboardlines - numlines);
	}

	for (;i < scoreboardlines && y < vid.conheight;i++)
		y += Sbar_PrintScoreboardItem(cl.scores + fragsort[i], x, y);
}

/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (void)
{
	int		dig;
	int		num;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay ();
		return;
	}

	sbar_x = (vid.conwidth - 320) >> 1;
	sbar_y = (vid.conheight - 200) >> 1;

	DrawQ_Pic (sbar_x + 64, sbar_y + 24, "gfx/complete.lmp", 0, 0, 1, 1, 1, 1, 0);
	DrawQ_Pic (sbar_x + 0, sbar_y + 56, "gfx/inter.lmp", 0, 0, 1, 1, 1, 1, 0);

// time
	dig = cl.completed_time/60;
	Sbar_DrawNum (160, 64, dig, 3, 0);
	num = cl.completed_time - dig*60;
	Sbar_DrawPic (234,64,sb_colon);
	Sbar_DrawPic (246,64,sb_nums[0][num/10]);
	Sbar_DrawPic (266,64,sb_nums[0][num%10]);

	Sbar_DrawNum (160, 104, cl.stats[STAT_SECRETS], 3, 0);
	Sbar_DrawPic (232, 104, sb_slash);
	Sbar_DrawNum (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	Sbar_DrawNum (160, 144, cl.stats[STAT_MONSTERS], 3, 0);
	Sbar_DrawPic (232, 144, sb_slash);
	Sbar_DrawNum (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);

}


/*
==================
Sbar_FinaleOverlay

==================
*/
void Sbar_FinaleOverlay (void)
{
	cachepic_t	*pic;

	pic = Draw_CachePic ("gfx/finale.lmp");
	DrawQ_Pic((vid.conwidth - pic->width)/2, 16, "gfx/finale.lmp", 0, 0, 1, 1, 1, 1, 0);
}

