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
	char name[16];
}
sbarpic_t;

static sbarpic_t sbarpics[256];
static int numsbarpics;

static sbarpic_t *Sbar_NewPic(char *name)
{
	strcpy(sbarpics[numsbarpics].name, name);
	// precache it
	// FIXME: precache on every renderer restart (or move this to client)
	Draw_CachePic(name);
	return sbarpics + (numsbarpics++);
}

sbarpic_t *sb_disc;

#define STAT_MINUS 10 // num frame for '-' stats digit
sbarpic_t *sb_nums[2][11];
sbarpic_t *sb_colon, *sb_slash;
sbarpic_t *sb_ibar;
sbarpic_t *sb_sbar;
sbarpic_t *sb_scorebar;

sbarpic_t *sb_weapons[7][8]; // 0 is active, 1 is owned, 2-5 are flashes
sbarpic_t *sb_ammo[4];
sbarpic_t *sb_sigil[4];
sbarpic_t *sb_armor[3];
sbarpic_t *sb_items[32];

// 0 is gibbed, 1 is dead, 2-6 are alive
// 0 is static, 1 is temporary animation
sbarpic_t *sb_faces[7][2];

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

/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{
	int i;

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);
	Cvar_RegisterVariable (&showfps);

	numsbarpics = 0;

	sb_disc = Sbar_NewPic("disc");

	for (i=0 ; i<10 ; i++)
	{
		sb_nums[0][i] = Sbar_NewPic (va("num_%i",i));
		sb_nums[1][i] = Sbar_NewPic (va("anum_%i",i));
	}

	sb_nums[0][10] = Sbar_NewPic ("num_minus");
	sb_nums[1][10] = Sbar_NewPic ("anum_minus");

	sb_colon = Sbar_NewPic ("num_colon");
	sb_slash = Sbar_NewPic ("num_slash");

	sb_weapons[0][0] = Sbar_NewPic ("inv_shotgun");
	sb_weapons[0][1] = Sbar_NewPic ("inv_sshotgun");
	sb_weapons[0][2] = Sbar_NewPic ("inv_nailgun");
	sb_weapons[0][3] = Sbar_NewPic ("inv_snailgun");
	sb_weapons[0][4] = Sbar_NewPic ("inv_rlaunch");
	sb_weapons[0][5] = Sbar_NewPic ("inv_srlaunch");
	sb_weapons[0][6] = Sbar_NewPic ("inv_lightng");

	sb_weapons[1][0] = Sbar_NewPic ("inv2_shotgun");
	sb_weapons[1][1] = Sbar_NewPic ("inv2_sshotgun");
	sb_weapons[1][2] = Sbar_NewPic ("inv2_nailgun");
	sb_weapons[1][3] = Sbar_NewPic ("inv2_snailgun");
	sb_weapons[1][4] = Sbar_NewPic ("inv2_rlaunch");
	sb_weapons[1][5] = Sbar_NewPic ("inv2_srlaunch");
	sb_weapons[1][6] = Sbar_NewPic ("inv2_lightng");

	for (i=0 ; i<5 ; i++)
	{
		sb_weapons[2+i][0] = Sbar_NewPic (va("inva%i_shotgun",i+1));
		sb_weapons[2+i][1] = Sbar_NewPic (va("inva%i_sshotgun",i+1));
		sb_weapons[2+i][2] = Sbar_NewPic (va("inva%i_nailgun",i+1));
		sb_weapons[2+i][3] = Sbar_NewPic (va("inva%i_snailgun",i+1));
		sb_weapons[2+i][4] = Sbar_NewPic (va("inva%i_rlaunch",i+1));
		sb_weapons[2+i][5] = Sbar_NewPic (va("inva%i_srlaunch",i+1));
		sb_weapons[2+i][6] = Sbar_NewPic (va("inva%i_lightng",i+1));
	}

	sb_ammo[0] = Sbar_NewPic ("sb_shells");
	sb_ammo[1] = Sbar_NewPic ("sb_nails");
	sb_ammo[2] = Sbar_NewPic ("sb_rocket");
	sb_ammo[3] = Sbar_NewPic ("sb_cells");

	sb_armor[0] = Sbar_NewPic ("sb_armor1");
	sb_armor[1] = Sbar_NewPic ("sb_armor2");
	sb_armor[2] = Sbar_NewPic ("sb_armor3");

	sb_items[0] = Sbar_NewPic ("sb_key1");
	sb_items[1] = Sbar_NewPic ("sb_key2");
	sb_items[2] = Sbar_NewPic ("sb_invis");
	sb_items[3] = Sbar_NewPic ("sb_invuln");
	sb_items[4] = Sbar_NewPic ("sb_suit");
	sb_items[5] = Sbar_NewPic ("sb_quad");

	sb_sigil[0] = Sbar_NewPic ("sb_sigil1");
	sb_sigil[1] = Sbar_NewPic ("sb_sigil2");
	sb_sigil[2] = Sbar_NewPic ("sb_sigil3");
	sb_sigil[3] = Sbar_NewPic ("sb_sigil4");

	sb_faces[4][0] = Sbar_NewPic ("face1");
	sb_faces[4][1] = Sbar_NewPic ("face_p1");
	sb_faces[3][0] = Sbar_NewPic ("face2");
	sb_faces[3][1] = Sbar_NewPic ("face_p2");
	sb_faces[2][0] = Sbar_NewPic ("face3");
	sb_faces[2][1] = Sbar_NewPic ("face_p3");
	sb_faces[1][0] = Sbar_NewPic ("face4");
	sb_faces[1][1] = Sbar_NewPic ("face_p4");
	sb_faces[0][0] = Sbar_NewPic ("face5");
	sb_faces[0][1] = Sbar_NewPic ("face_p5");

	sb_face_invis = Sbar_NewPic ("face_invis");
	sb_face_invuln = Sbar_NewPic ("face_invul2");
	sb_face_invis_invuln = Sbar_NewPic ("face_inv2");
	sb_face_quad = Sbar_NewPic ("face_quad");

	sb_sbar = Sbar_NewPic ("sbar");
	sb_ibar = Sbar_NewPic ("ibar");
	sb_scorebar = Sbar_NewPic ("scorebar");

//MED 01/04/97 added new hipnotic weapons
	if (gamemode == GAME_HIPNOTIC)
	{
		hsb_weapons[0][0] = Sbar_NewPic ("inv_laser");
		hsb_weapons[0][1] = Sbar_NewPic ("inv_mjolnir");
		hsb_weapons[0][2] = Sbar_NewPic ("inv_gren_prox");
		hsb_weapons[0][3] = Sbar_NewPic ("inv_prox_gren");
		hsb_weapons[0][4] = Sbar_NewPic ("inv_prox");

		hsb_weapons[1][0] = Sbar_NewPic ("inv2_laser");
		hsb_weapons[1][1] = Sbar_NewPic ("inv2_mjolnir");
		hsb_weapons[1][2] = Sbar_NewPic ("inv2_gren_prox");
		hsb_weapons[1][3] = Sbar_NewPic ("inv2_prox_gren");
		hsb_weapons[1][4] = Sbar_NewPic ("inv2_prox");

		for (i=0 ; i<5 ; i++)
		{
			hsb_weapons[2+i][0] = Sbar_NewPic (va("inva%i_laser",i+1));
			hsb_weapons[2+i][1] = Sbar_NewPic (va("inva%i_mjolnir",i+1));
			hsb_weapons[2+i][2] = Sbar_NewPic (va("inva%i_gren_prox",i+1));
			hsb_weapons[2+i][3] = Sbar_NewPic (va("inva%i_prox_gren",i+1));
			hsb_weapons[2+i][4] = Sbar_NewPic (va("inva%i_prox",i+1));
		}

		hsb_items[0] = Sbar_NewPic ("sb_wsuit");
		hsb_items[1] = Sbar_NewPic ("sb_eshld");
	}
	else if (gamemode == GAME_ROGUE)
	{
		rsb_invbar[0] = Sbar_NewPic ("r_invbar1");
		rsb_invbar[1] = Sbar_NewPic ("r_invbar2");

		rsb_weapons[0] = Sbar_NewPic ("r_lava");
		rsb_weapons[1] = Sbar_NewPic ("r_superlava");
		rsb_weapons[2] = Sbar_NewPic ("r_gren");
		rsb_weapons[3] = Sbar_NewPic ("r_multirock");
		rsb_weapons[4] = Sbar_NewPic ("r_plasma");

		rsb_items[0] = Sbar_NewPic ("r_shield1");
		rsb_items[1] = Sbar_NewPic ("r_agrav1");

// PGM 01/19/97 - team color border
		rsb_teambord = Sbar_NewPic ("r_teambord");
// PGM 01/19/97 - team color border

		rsb_ammo[0] = Sbar_NewPic ("r_ammolava");
		rsb_ammo[1] = Sbar_NewPic ("r_ammomulti");
		rsb_ammo[2] = Sbar_NewPic ("r_ammoplasma");
	}
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

//=============================================================================

int		fragsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
Sbar_SortFrags
===============
*/
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
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{
	int		i, k;
	int		top, bottom;
	scoreboard_t	*s;

	Sbar_SortFrags ();

// draw the text
	memset (scoreboardtext, 0, sizeof(scoreboardtext));

	for (i=0 ; i<scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		sprintf (&scoreboardtext[i][1], "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) <<4;
		scoreboardtop[i] = top + 8;
		scoreboardbottom[i] = bottom + 8;
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

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (void)
{
	int		i;
	char	num[6];
	float	time;
	int		flashon;

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
		c = (qbyte *)&d_8to24table[(s->colors & 0xf0) + 8];
		DrawQ_Fill (sbar_x + x + 10, sbar_y     - 23, 28, 4, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&d_8to24table[((s->colors & 15)<<4) + 8];
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
	if (gamemode == GAME_ROGUE && (cl.maxclients != 1) && (teamplay.integer > 3) && (teamplay.integer < 7))
	{
		char num[12];
		scoreboard_t *s;
		qbyte *c;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		Sbar_DrawPic (112, 0, rsb_teambord);
		c = (qbyte *)&d_8to24table[(s->colors & 0xf0) + 8];
		DrawQ_Fill (sbar_x + 113, vid.conheight-SBAR_HEIGHT+3, 22, 9, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&d_8to24table[((s->colors & 15)<<4) + 8];
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
//void DrawCrosshair(int num);
void Sbar_Draw (void)
{
	if (scr_con_current == vid.conheight)
		return;		// console is full screen

	sbar_y = vid.conheight - SBAR_HEIGHT;
	if (cl.gametype == GAME_DEATHMATCH)
		sbar_x = 0;
	else
		sbar_x = (vid.conwidth - 320)/2;

	if (sb_lines > 24)
	{
		Sbar_DrawInventory ();
		if (cl.maxclients != 1)
			Sbar_DrawFrags ();
	}

	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
	{
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

	// face
		Sbar_DrawFace ();

	// health
		Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

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

	if (vid.conwidth > 320 && cl.gametype == GAME_DEATHMATCH)
		Sbar_MiniDeathmatchOverlay ();

//	if (crosshair.integer >= 1)
//		DrawCrosshair(crosshair.integer - 1);

	if (cl.intermission == 1)
		Sbar_IntermissionOverlay();
	else if (cl.intermission == 2)
		Sbar_FinaleOverlay();

	Sbar_ShowFPS();
}

//=============================================================================

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_DeathmatchOverlay (void)
{
	cachepic_t *pic;
	int i, k, l, x, y, total, n, minutes, tens, units, fph;
	char num[128];
	scoreboard_t *s;
	qbyte *c;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	DrawQ_Pic ((vid.conwidth - pic->width)/2, 8, "gfx/ranking.lmp", 0, 0, 1, 1, 1, 1, 0);

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;

	x = (vid.conwidth - 280)>>1;
	y = 40;
	for (i = 0;i < l;i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		c = (qbyte *)&d_8to24table[(s->colors & 0xf0) + 8];
		DrawQ_Fill ( x + 8, y+1, 88, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&d_8to24table[((s->colors & 15)<<4) + 8];
		DrawQ_Fill ( x + 8, y+4, 88, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);

		total = cl.time - s->entertime;
		minutes = (int)total/60;
		n = total - minutes*60;
		tens = '0' + n/10;
		units = '0' + n%10;

		fph = total ? (int) ((float) s->frags * 3600.0 / total) : 0;
		if (fph < -999) fph = -999;
		if (fph > 9999) fph = 9999;

		// put it together
		sprintf (num, "%c %4i:%4i %4i:%c%c %s", k == cl.viewentity - 1 ? 12 : ' ', (int) s->frags, fph, minutes, tens, units, s->name);
		DrawQ_String(x, y, num, 0, 8, 8, 1, 1, 1, 1, 0);

		y += 8;
	}
}

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{
	int i, l, k, x, y, fph, numlines;
	char num[128];
	scoreboard_t *s;
	qbyte *c;

	if (vid.conwidth < 512 || !sb_lines)
		return;

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;
	y = vid.conheight - sb_lines;
	numlines = sb_lines/8;
	if (numlines < 3)
		return;

	//find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;

	if (i == scoreboardlines) // we're not there
		i = 0;
	else // figure out start
		i = i - numlines/2;

	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	if (i < 0)
		i = 0;

	x = 324;
	for (;i < scoreboardlines && y < vid.conheight - 8;i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		c = (qbyte *)&d_8to24table[(s->colors & 0xf0) + 8];
		DrawQ_Fill ( x, y+1, 72, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);
		c = (qbyte *)&d_8to24table[((s->colors & 15)<<4) + 8];
		DrawQ_Fill ( x, y+4, 72, 3, c[0] * (1.0f / 255.0f), c[1] * (1.0f / 255.0f), c[2] * (1.0f / 255.0f), c[3] * (1.0f / 255.0f), 0);

		fph = (cl.time - s->entertime) ? (int) ((float) s->frags * 3600.0 / (cl.time - s->entertime)) : 0;
		if (fph < -999) fph = -999;
		if (fph > 9999) fph = 9999;

		// put it together
		sprintf (num, "%c%4i:%4i%c %s", k == cl.viewentity - 1 ? 16 : ' ', (int) s->frags, fph, k == cl.viewentity - 1 ? 17 : ' ', s->name);
		DrawQ_String(x - 8, y, num, 0, 8, 8, 1, 1, 1, 1, 0);

		y += 8;
	}
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

