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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

char *svc_strings[128] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value
	
	"svc_serverinfo",		// [long] version
						// [string] signon string
						// [string]..[0]model cache [string]...[0]sounds cache
						// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from
	
	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",
	
	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
	"svc_showlmp",	// [string] iconlabel [string] lmpfile [byte] x [byte] y
	"svc_hidelmp",	// [string] iconlabel
	"svc_skybox", // [string] skyname
	"?", // 38
	"?", // 39
	"?", // 40
	"?", // 41
	"?", // 42
	"?", // 43
	"?", // 44
	"?", // 45
	"?", // 46
	"?", // 47
	"?", // 48
	"?", // 49
	"svc_farclip", // [coord] size
	"svc_fog", // [byte] enable <optional past this point, only included if enable is true> [short] density*4096 [byte] red [byte] green [byte] blue
	"svc_playerposition" // [float] x [float] y [float] z
};

//=============================================================================

qboolean Nehahrademcompatibility; // LordHavoc: to allow playback of the early Nehahra movie segments
qboolean dpprotocol; // LordHavoc: whether or not the current network stream is the enhanced DarkPlaces protocol

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t	*CL_EntityNum (int num)
{
	if (num >= cl.num_entities)
	{
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %i is an invalid number",num);
		while (cl.num_entities<=num)
		{
			cl_entities[cl.num_entities].colormap = -1; // no special coloring
			cl.num_entities++;
		}
	}
		
	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
 	int		i;
	           
    field_mask = MSG_ReadByte(); 

    if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	
	channel = MSG_ReadShort ();
	sound_num = MSG_ReadByte ();

	ent = channel >> 3;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);
	
	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();
 
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void CL_KeepaliveMessage (void)
{
	float	time;
	static float lastmsg;
	int		ret;
	sizebuf_t	old;
	byte		olddata[8192];
	
	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);
	
	do
	{
		ret = CL_GetMessage ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");		
		case 0:
			break;	// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			if (MSG_ReadByte() != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = Sys_FloatTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

extern qboolean isworldmodel;
extern char skyname[];
extern void R_SetSkyBox (char *sky);
extern void FOG_clear();
extern cvar_t r_farclip;

void CL_ParseEntityLump(char *entdata)
{
	char *data;
	char key[128], value[1024];
	char wadname[128];
	int i, j, k;
	FOG_clear(); // LordHavoc: no fog until set
	skyname[0] = 0; // LordHavoc: no enviroment mapped sky until set
	r_farclip.value = 6144; // LordHavoc: default farclip distance
	data = entdata;
	if (!data)
		return;
	data = COM_Parse(data);
	if (!data)
		return; // valid exit
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			return; // since we're just parsing the first ent (worldspawn), exit
		strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_Parse(data);
		if (!data)
			return; // error
		strcpy(value, com_token);
		if (!strcmp("sky", key))
			R_SetSkyBox(value);
		else if (!strcmp("skyname", key)) // non-standard, introduced by QuakeForge... sigh.
			R_SetSkyBox(value);
		else if (!strcmp("qlsky", key)) // non-standard, introduced by QuakeLives (EEK)
			R_SetSkyBox(value);
		else if (!strcmp("farclip", key))
		{
			r_farclip.value = atof(value);
			if (r_farclip.value < 64)
				r_farclip.value = 64;
		}
		else if (!strcmp("fog", key))
		{
			scanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
			j = 0;
		}
		else if (!strcmp("fog_density", key))
			fog_density = atof(value);
		else if (!strcmp("fog_red", key))
			fog_red = atof(value);
		else if (!strcmp("fog_green", key))
			fog_green = atof(value);
		else if (!strcmp("fog_blue", key))
			fog_blue = atof(value);
		else if (!strcmp("wad", key)) // for HalfLife maps
		{
			j = 0;
			for (i = 0;i < 128;i++)
				if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
					break;
			if (value[i])
			{
				for (;i < 128;i++)
				{
					// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
					if (value[i] == '\\' || value[i] == '/' || value[i] == ':')
						j = i+1;
					else if (value[i] == ';' || value[i] == 0)
					{
						k = value[i];
						value[i] = 0;
						strcpy(wadname, "textures/");
						strcat(wadname, &value[j]);
						W_LoadTextureWadFile (wadname, FALSE);
						j = i+1;
						if (!k)
							break;
					}
				}
			}
		}
	}
}

/*
==================
CL_ParseServerInfo
==================
*/
extern cvar_t demo_nehahra;
void CL_ParseServerInfo (void)
{
	char	*str;
	int		i;
	int		nummodels, numsounds;
	char	model_precache[MAX_MODELS][MAX_QPATH];
	char	sound_precache[MAX_SOUNDS][MAX_QPATH];
	
	Con_DPrintf ("Serverinfo packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	if (i != PROTOCOL_VERSION && i != DPPROTOCOL_VERSION && i != 250)
	{
		Con_Printf ("Server returned version %i, not %i or %i", i, DPPROTOCOL_VERSION, PROTOCOL_VERSION);
		return;
	}
	Nehahrademcompatibility = false;
	if (i == 250)
		Nehahrademcompatibility = true;
	if (cls.demoplayback && demo_nehahra.value)
		Nehahrademcompatibility = true;
	dpprotocol = i == DPPROTOCOL_VERSION;

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	cl.scores = Hunk_AllocName (cl.maxclients*sizeof(*cl.scores), "scores");

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

// seperate the printfs so the server message can have a color
	if (!Nehahrademcompatibility) // no messages when playing the Nehahra movie
	{
		Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Con_Printf ("%c%s\n", 2, str);
	}

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels==MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches\n");
			return;
		}
		if (strlen(str) >= MAX_QPATH)
			Host_Error ("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

// precache sounds
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds==MAX_SOUNDS)
		{
			Con_Printf ("Server sent too many sound precaches\n");
			return;
		}
		if (strlen(str) >= MAX_QPATH)
			Host_Error ("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strcpy (sound_precache[numsounds], str);
		S_TouchSound (str);
	}

//
// now we try to load everything else until a cache allocation fails
//

	for (i=1 ; i<nummodels ; i++)
	{
		isworldmodel = i == 1; // LordHavoc: first model is the world model
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == NULL)
		{
			Con_Printf("Model %s not found\n", model_precache[i]);
			return;
		}
		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();
	for (i=1 ; i<numsounds ; i++)
	{
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();


// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];
	
	R_NewMap ();

	Hunk_Check ();		// make sure nothing is hurt
	
	noclip_anglehack = false;		// noclip is turned off at start	
}


/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
void CL_ParseUpdate (int bits)
{
	int			i, modnum, num, alpha, scale, glowsize, glowcolor, colormod;
	model_t		*model;
	qboolean	forcelink;
	entity_t	*ent;
	entity_state_t *baseline;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte()<<8);
	if (bits & U_EXTEND1 && !Nehahrademcompatibility)
	{
		bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}

	if (bits & U_LONGENTITY)	
		num = MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

	ent = CL_EntityNum (num);

	forcelink = ent->msgtime != cl.mtime[1]; // no previous frame to lerp from

	ent->msgtime = cl.mtime[0];
	
	// LordHavoc: new protocol stuff
	baseline = &ent->baseline;
	if (bits & U_DELTA)
		baseline = &ent->deltabaseline;

	if (forcelink)
	{
		ent->deltabaseline.origin[0] = ent->deltabaseline.origin[1] = ent->deltabaseline.origin[2] = 0;
		ent->deltabaseline.angles[0] = ent->deltabaseline.angles[1] = ent->deltabaseline.angles[2] = 0;
		ent->deltabaseline.effects = 0;
		ent->deltabaseline.modelindex = 0;
		ent->deltabaseline.frame = 0;
		ent->deltabaseline.colormap = 0;
		ent->deltabaseline.skin = 0;
		ent->deltabaseline.alpha = 255;
		ent->deltabaseline.scale = 16;
		ent->deltabaseline.glowsize = 0;
		ent->deltabaseline.glowcolor = 254;
		ent->deltabaseline.colormod = 255;
	}

	modnum = bits & U_MODEL ? MSG_ReadByte() : baseline->modelindex;
	if (modnum >= MAX_MODELS)
		Host_Error ("CL_ParseModel: bad modnum");
	ent->deltabaseline.modelindex = modnum;
		
	model = cl.model_precache[modnum];
	if (model != ent->model)
	{
		ent->model = model;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (model)
			ent->syncbase = model->synctype == ST_RAND ? (float)(rand()&0x7fff) / 0x7fff : 0.0;
		else
			forcelink = true;	// hack to make null model players work
	}

	ent->frame = ((bits & U_FRAME) ? MSG_ReadByte() : (baseline->frame & 0xFF));

	i = bits & U_COLORMAP ? MSG_ReadByte() : baseline->colormap;
	ent->deltabaseline.colormap = i;
	if (!i)
		ent->colormap = -1; // no special coloring
	else
	{
		if (i > cl.maxclients)
			Host_Error ("i >= cl.maxclients");
		ent->colormap = cl.scores[i-1].colors; // color it
	}

	ent->deltabaseline.skin = ent->skinnum = bits & U_SKIN ? MSG_ReadByte() : baseline->skin;

	ent->effects = ((bits & U_EFFECTS) ? MSG_ReadByte() : (baseline->effects & 0xFF));

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
	VectorCopy (baseline->origin, ent->msg_origins[0]);
	VectorCopy (baseline->angles, ent->msg_angles[0]);

	if (bits & U_ORIGIN1) ent->msg_origins[0][0] = MSG_ReadCoord ();
	if (bits & U_ANGLE1) ent->msg_angles[0][0] = MSG_ReadAngle();
	if (bits & U_ORIGIN2) ent->msg_origins[0][1] = MSG_ReadCoord ();
	if (bits & U_ANGLE2) ent->msg_angles[0][1] = MSG_ReadAngle();
	if (bits & U_ORIGIN3) ent->msg_origins[0][2] = MSG_ReadCoord ();
	if (bits & U_ANGLE3) ent->msg_angles[0][2] = MSG_ReadAngle();

	VectorCopy(ent->msg_origins[0], ent->deltabaseline.origin);
	VectorCopy(ent->msg_angles[0], ent->deltabaseline.angles);

	alpha = bits & U_ALPHA ? MSG_ReadByte() : baseline->alpha;
	scale = bits & U_SCALE ? MSG_ReadByte() : baseline->scale;
	ent->effects |= ((bits & U_EFFECTS2) ? (MSG_ReadByte() << 8) : (baseline->effects & 0xFF00));
	glowsize = bits & U_GLOWSIZE ? MSG_ReadByte() : baseline->glowsize;
	glowcolor = bits & U_GLOWCOLOR ? MSG_ReadByte() : baseline->glowcolor;
	colormod = bits & U_COLORMOD ? MSG_ReadByte() : baseline->colormod;
	ent->frame |= ((bits & U_FRAME2) ? (MSG_ReadByte() << 8) : (baseline->frame & 0xFF00));
	ent->deltabaseline.alpha = alpha;
	ent->deltabaseline.scale = scale;
	ent->deltabaseline.effects = ent->effects;
	ent->deltabaseline.glowsize = glowsize;
	ent->deltabaseline.glowcolor = glowcolor;
	ent->deltabaseline.colormod = colormod;
	ent->deltabaseline.frame = ent->frame;
	ent->alpha = (float) alpha * (1.0 / 255.0);
	ent->scale = (float) scale * (1.0 / 16.0);
	ent->glowsize = glowsize * 4.0;
	ent->glowcolor = glowcolor;
	ent->colormod[0] = (float) ((colormod >> 5) & 7) * (1.0 / 7.0);
	ent->colormod[1] = (float) ((colormod >> 2) & 7) * (1.0 / 7.0);
	ent->colormod[2] = (float) (colormod & 3) * (1.0 / 3.0);
	if (bits & U_EXTEND1 && Nehahrademcompatibility) // LordHavoc: to allow playback of the early Nehahra movie segments
	{
		i = MSG_ReadFloat();
		ent->alpha = MSG_ReadFloat();
		if (i == 2 && MSG_ReadFloat() != 0.0)
			ent->effects |= EF_FULLBRIGHT;
		if (ent->alpha == 0)
			ent->alpha = 1;
	}

	//if ( bits & U_NOLERP )
	//	ent->forcelink = true;
	//if (bits & U_STEP) // FIXME: implement clientside interpolation of monsters

	if ( forcelink )
	{	// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent)
{
	int			i;
	
	ent->baseline.modelindex = MSG_ReadByte ();
	ent->baseline.frame = MSG_ReadByte ();
	ent->baseline.colormap = MSG_ReadByte();
	ent->baseline.skin = MSG_ReadByte();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord ();
		ent->baseline.angles[i] = MSG_ReadAngle ();
	}
	ent->baseline.alpha = 255;
	ent->baseline.scale = 16;
	ent->baseline.glowsize = 0;
	ent->baseline.glowcolor = 254;
	ent->baseline.colormod = 255;
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (int bits)
{
	int		i, j;
	
	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = MSG_ReadChar ();
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = MSG_ReadChar ();
	else
		cl.idealpitch = 0;
	
	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) )
			cl.punchangle[i] = MSG_ReadChar();
		else
			cl.punchangle[i] = 0;
		if (bits & (SU_VELOCITY1<<i) )
			cl.mvelocity[0][i] = MSG_ReadChar()*16;
		else
			cl.mvelocity[0][i] = 0;
	}

	i = MSG_ReadLong ();
	if (cl.items != i)
	{	// set flash times
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.items & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}
		
	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadByte() : 0;
	cl.stats[STAT_ARMOR] = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;
	cl.stats[STAT_WEAPON] = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;
	cl.stats[STAT_HEALTH] = MSG_ReadShort();
	cl.stats[STAT_AMMO] = MSG_ReadByte();

	cl.stats[STAT_SHELLS] = MSG_ReadByte();
	cl.stats[STAT_NAILS] = MSG_ReadByte();
	cl.stats[STAT_ROCKETS] = MSG_ReadByte();
	cl.stats[STAT_CELLS] = MSG_ReadByte();

	i = MSG_ReadByte ();

	if (standard_quake)
		cl.stats[STAT_ACTIVEWEAPON] = i;
	else
		cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (void)
{
	entity_t *ent;
	int		i;
		
	i = cl.num_statics;
	if (i >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[i];
	cl.num_statics++;
	CL_ParseBaseline (ent);

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->frame = ent->baseline.frame;
	ent->colormap = -1; // no special coloring
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->alpha = 1;
	ent->scale = 1;
	ent->alpha = 1;
	ent->glowsize = 0;
	ent->glowcolor = 254;
	ent->colormod[0] = ent->colormod[1] = ent->colormod[2] = 1;

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);	
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (void)
{
	vec3_t		org;
	int			sound_num, vol, atten;
	int			i;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();
	
	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);

extern void SHOWLMP_decodehide();
extern void SHOWLMP_decodeshow();
extern void R_SetSkyBox(char* sky);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	int			i;
	byte		cmdlog[32];
	char		*cmdlogname[32];
	int			cmdindex, cmdcount = 0;
	
//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf ("%i ",net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	
//
// parse the message
//
	MSG_BeginReading ();
	
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

		cmdindex = cmdcount & 31;
		cmdcount++;
		cmdlog[cmdindex] = cmd;

		// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128)
		{
			cmdlogname[cmdindex] = &("svc_entity");
			SHOWNET("fast update");
			CL_ParseUpdate (cmd&127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);
		cmdlogname[cmdindex] = svc_strings[cmd];
		if (!cmdlogname[cmdindex])
			cmdlogname[cmdindex] = &("<unknown>");
	
		// other commands
		switch (cmd)
		{
		default:
			{
				char description[32*64], temp[64];
				int count;
				strcpy(description, "packet dump: ");
				i = cmdcount - 32;
				if (i < 0)
					i = 0;
				count = cmdcount - i;
				i &= 31;
				while(count > 0)
				{
					sprintf(temp, "%3i:%s ", cmdlog[i], cmdlogname[i]);
					strcat(description, temp);
					count--;
					i++;
					i &= 31;
				}
				description[strlen(description)-1] = '\n'; // replace the last space with a newline
				Con_Printf(description);
				Host_Error ("CL_ParseServerMessage: Illegible server message\n");
			}
			break;
			
		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;
			
		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();			
			break;
			
		case svc_clientdata:
			i = MSG_ReadShort ();
			CL_ParseClientdata (i);
			break;
		
		case svc_version:
			i = MSG_ReadLong ();
			if (i != PROTOCOL_VERSION && i != 250)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i instead of %i\n", i, PROTOCOL_VERSION);
			Nehahrademcompatibility = i == 250;
			break;
			
		case svc_disconnect:
			Host_EndGame ("Server disconnected\n");

		case svc_print:
			Con_Printf ("%s", MSG_ReadString ());
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString ());
			break;
			
		case svc_stufftext:
			Cbuf_AddText (MSG_ReadString ());
			break;
			
		case svc_damage:
			V_ParseDamage ();
			break;
			
		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;
			
		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle ();
			break;
			
		case svc_setview:
			cl.viewentity = MSG_ReadShort ();
			break;
					
		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
			strcpy (cl_lightstyle[i].map,  MSG_ReadString());
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;
		
		case svc_updatename:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
			strcpy (cl.scores[i].name, MSG_ReadString ());
			break;
			
		case svc_updatefrags:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;			

		case svc_updatecolors:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			break;
			
		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i));
			break;
		case svc_spawnstatic:
			CL_ParseStatic ();
			break;			
		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;
			
		case svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= cls.signon)
				Host_Error ("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			CL_SignonReply ();
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Host_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadLong ();;
			break;
			
		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			cl.looptrack = MSG_ReadByte ();
			if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
				CDAudio_Play ((byte)cls.forcetrack, true);
			else
				CDAudio_Play ((byte)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString ());			
			break;

		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString ());			
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;
		case svc_hidelmp:
			SHOWLMP_decodehide();
			break;
		case svc_showlmp:
			SHOWLMP_decodeshow();
			break;
	// LordHavoc: extra worldspawn fields (fog, sky, farclip)
		case svc_skybox:
			R_SetSkyBox(MSG_ReadString());
			break;
		case svc_farclip:
			r_farclip.value = MSG_ReadCoord();
			break;
		case svc_fog:
			if (MSG_ReadByte())
			{
				fog_density = MSG_ReadShort() * (1.0f / 4096.0f);
				fog_red = MSG_ReadByte() * (1.0 / 255.0);
				fog_green = MSG_ReadByte() * (1.0 / 255.0);
				fog_blue = MSG_ReadByte() * (1.0 / 255.0);
			}
			else
				fog_density = 0.0f;
			break;
		}
	}
}

