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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"

cvar_t r_glowinglightning = {CVAR_SAVE, "r_glowinglightning", "1"};

int			num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

sfx_t			*cl_sfx_wizhit;
sfx_t			*cl_sfx_knighthit;
sfx_t			*cl_sfx_tink1;
sfx_t			*cl_sfx_ric1;
sfx_t			*cl_sfx_ric2;
sfx_t			*cl_sfx_ric3;
sfx_t			*cl_sfx_r_exp3;

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts (void)
{
	Cvar_RegisterVariable(&r_glowinglightning);
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
}

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam (model_t *m)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;
	
	ent = MSG_ReadShort ();
	MSG_ReadVector(start);
	MSG_ReadVector(end);

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow!\n");	
}

//void R_BlastParticles(vec3_t org, vec_t radius, vec_t power);
void R_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count);
void R_ParticleCube (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int gravity, int randomvel);
void R_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type);

// attempts to find the nearest non-solid location, used for explosions mainly
void FindNonSolidLocation(vec3_t pos)
{
	if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[0]-=1;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[0]+=2;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[0]-=1;
	pos[1]-=1;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[1]+=2;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[1]-=1;
	pos[2]-=1;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[2]+=2;if (SV_HullPointContents (cl.worldmodel->hulls, 0, pos) != CONTENTS_SOLID) return;
	pos[2]-=1;
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	int		type;
	vec3_t	pos;
	vec3_t	dir;
	vec3_t	pos2;
	int		rnd;
	int		colorStart, colorLength, count;
	float	velspeed, radius;
	byte *tempcolor;

	type = MSG_ReadByte ();
	switch (type)
	{
	case TE_WIZSPIKE:			// spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;
		
	case TE_KNIGHTSPIKE:			// spike hitting wall
		MSG_ReadVector(pos);
		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;
		
	case TE_SPIKE:			// spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to spark shower
		R_SparkShower(pos, vec3_origin, 15);
		//R_RunParticleEffect (pos, vec3_origin, 0, 10);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SPIKEQUAD:			// quad spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to spark shower
		R_SparkShower(pos, vec3_origin, 15);
		//R_RunParticleEffect (pos, vec3_origin, 0, 10);
		CL_AllocDlight (NULL, pos, 200, 0.1f, 0.1f, 1.0f, 1000, 0.2);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:			// super spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to dust shower
		R_SparkShower(pos, vec3_origin, 30);
		//R_RunParticleEffect (pos, vec3_origin, 0, 20);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKEQUAD:			// quad super spike hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to dust shower
		R_SparkShower(pos, vec3_origin, 30);
		//R_RunParticleEffect (pos, vec3_origin, 0, 20);
		CL_AllocDlight (NULL, pos, 200, 0.1f, 0.1f, 1.0f, 1000, 0.2);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		// LordHavoc: added for improved blood splatters
	case TE_BLOOD:	// blood puff
		MSG_ReadVector(pos);
		dir[0] = MSG_ReadChar ();
		dir[1] = MSG_ReadChar ();
		dir[2] = MSG_ReadChar ();
		count = MSG_ReadByte (); // amount of particles
		R_BloodPuff(pos, dir, count);
		break;
	case TE_BLOOD2:	// blood puff
		MSG_ReadVector(pos);
		R_BloodPuff(pos, vec3_origin, 10);
		break;
	case TE_SPARK:	// spark shower
		MSG_ReadVector(pos);
		dir[0] = MSG_ReadChar ();
		dir[1] = MSG_ReadChar ();
		dir[2] = MSG_ReadChar ();
		count = MSG_ReadByte (); // amount of particles
		R_SparkShower(pos, dir, count);
		break;
		// LordHavoc: added for improved gore
	case TE_BLOODSHOWER:	// vaporized body
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		velspeed = MSG_ReadCoord (); // speed
		count = MSG_ReadShort (); // number of particles
		R_BloodShower(pos, pos2, velspeed, count);
		break;
	case TE_PARTICLECUBE:	// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort (); // number of particles
		colorStart = MSG_ReadByte (); // color
		colorLength = MSG_ReadByte (); // gravity (1 or 0)
		velspeed = MSG_ReadCoord (); // randomvel
		R_ParticleCube(pos, pos2, dir, count, colorStart, colorLength, velspeed);
		break;

	case TE_PARTICLERAIN:	// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort (); // number of particles
		colorStart = MSG_ReadByte (); // color
		R_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		break;

	case TE_PARTICLESNOW:	// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort (); // number of particles
		colorStart = MSG_ReadByte (); // color
		R_ParticleRain(pos, pos2, dir, count, colorStart, 1);
		break;

	case TE_GUNSHOT:			// bullet hitting wall
		MSG_ReadVector(pos);
		// LordHavoc: changed to dust shower
		R_SparkShower(pos, vec3_origin, 15);
		//R_RunParticleEffect (pos, vec3_origin, 0, 20);
		break;

	case TE_GUNSHOTQUAD:			// quad bullet hitting wall
		MSG_ReadVector(pos);
		R_SparkShower(pos, vec3_origin, 15);
		CL_AllocDlight (NULL, pos, 200, 0.1f, 0.1f, 1.0f, 1000, 0.2);
		break;

	case TE_EXPLOSION:			// rocket explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_ParticleExplosion (pos, false);
//		R_BlastParticles (pos, 120, 120);
		CL_AllocDlight (NULL, pos, 350, 1.0f, 0.8f, 0.4f, 700, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONQUAD:			// quad rocket explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_ParticleExplosion (pos, false);
//		R_BlastParticles (pos, 120, 480);
		CL_AllocDlight (NULL, pos, 600, 0.5f, 0.4f, 1.0f, 1200, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

		/*
	case TE_SMOKEEXPLOSION:			// rocket explosion with a cloud of smoke
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_ParticleExplosion (pos, true);
		CL_AllocDlight (NULL, pos, 350, 1.0f, 0.8f, 0.4f, 700, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		*/

	case TE_EXPLOSION3:				// Nehahra movie colored lighting explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_ParticleExplosion (pos, false);
//		R_BlastParticles (pos, 120, 120);
		CL_AllocDlight (NULL, pos, 350, MSG_ReadCoord(), MSG_ReadCoord(), MSG_ReadCoord(), 700, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONRGB:			// colored lighting explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_ParticleExplosion (pos, false);
//		R_BlastParticles (pos, 120, 120);
		CL_AllocDlight (NULL, pos, 350, MSG_ReadByte() * (1.0 / 255.0), MSG_ReadByte() * (1.0 / 255.0), MSG_ReadByte() * (1.0 / 255.0), 700, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:			// tarbaby explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		R_BlobExplosion (pos);
//		R_BlastParticles (pos, 120, 120);

		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		CL_AllocDlight (NULL, pos, 600, 0.8f, 0.4f, 1.0f, 1200, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_SMALLFLASH:
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		CL_AllocDlight (NULL, pos, 200, 1, 1, 1, 1000, 0.2);
		break;

	case TE_CUSTOMFLASH:
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		radius = MSG_ReadByte() * 8;
		velspeed = (MSG_ReadByte() + 1) * (1.0 / 256.0);
		CL_AllocDlight (NULL, pos, radius, MSG_ReadByte() * (1.0 / 255.0), MSG_ReadByte() * (1.0 / 255.0), MSG_ReadByte() * (1.0 / 255.0), radius / velspeed, velspeed);
		break;

	case TE_FLAMEJET:
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte();
		R_Flames(pos, dir, count);
		break;

	case TE_LIGHTNING1:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt.mdl", true));
		break;
	
	case TE_LIGHTNING2:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt2.mdl", true));
		break;
	
	case TE_LIGHTNING3:				// lightning bolts
		CL_ParseBeam (Mod_ForName("progs/bolt3.mdl", true));
		break;

// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		CL_ParseBeam (Mod_ForName("progs/beam.mdl", true));
		break;
// PGM 01/21/97

// LordHavoc: for compatibility with the Nehahra movie...
	case TE_LIGHTNING4NEH:
		CL_ParseBeam (Mod_ForName(MSG_ReadString(), true));
		break;

	case TE_LAVASPLASH:	
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_LavaSplash (pos);
		break;
	
	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord ();
		pos[1] = MSG_ReadCoord ();
		pos[2] = MSG_ReadCoord ();
		R_TeleportSplash (pos);
		break;
		
	case TE_EXPLOSION2:				// color mapped explosion
		MSG_ReadVector(pos);
		FindNonSolidLocation(pos);
		colorStart = MSG_ReadByte ();
		colorLength = MSG_ReadByte ();
		R_ParticleExplosion2 (pos, colorStart, colorLength);
//		R_BlastParticles (pos, 80, 80);
		tempcolor = (byte *)&d_8to24table[(rand()%colorLength) + colorStart];
		CL_AllocDlight (NULL, pos, 350, tempcolor[0] * (1.0f / 255.0f), tempcolor[1] * (1.0f / 255.0f), tempcolor[2] * (1.0f / 255.0f), 700, 0.5);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
	default:
		Host_Error ("CL_ParseTEnt: bad type %d", type);
	}
}


/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;

	if (cl_numvisedicts >= MAX_VISEDICTS)
		return NULL;
	if (num_temp_entities >= MAX_TEMP_ENTITIES)
		return NULL;
	ent = &cl_temp_entities[num_temp_entities++];
	memset (ent, 0, sizeof(*ent));
	cl_visedicts[cl_numvisedicts++] = ent;

	ent->render.colormap = -1; // no special coloring
	ent->render.scale = 1;
	ent->render.alpha = 1;
	ent->render.colormod[0] = ent->render.colormod[1] = ent->render.colormod[2] = 1;
	return ent;
}


/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int			i;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	*ent;
	float		yaw, pitch;
	float		forward;

	num_temp_entities = 0;

// update lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
			VectorCopy (cl_entities[cl.viewentity].render.origin, b->start);

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalizeLength(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			VectorCopy (org, ent->render.origin);
			ent->render.model = b->model;
			ent->render.effects = EF_FULLBRIGHT;
			ent->render.angles[0] = pitch;
			ent->render.angles[1] = yaw;
			ent->render.angles[2] = rand()%360;

			if (r_glowinglightning.value > 0)
				CL_AllocDlight(ent, ent->render.origin, lhrandom(100, 120), r_glowinglightning.value * 0.25f, r_glowinglightning.value * 0.25f, r_glowinglightning.value * 0.25f, 0, 0);

			VectorMA(org, 30, dist, org);
			d -= 30;
		}
	}

}


