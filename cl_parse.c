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
	"svc_showlmp",	// [string] iconlabel [string] lmpfile [short] x [short] y
	"svc_hidelmp",	// [string] iconlabel
	"", // 37
	"", // 38
	"", // 39
	"", // 40
	"", // 41
	"", // 42
	"", // 43
	"", // 44
	"", // 45
	"", // 46
	"", // 47
	"", // 48
	"", // 49
	"", // 50
	"svc_fog", // 51
	"svc_effect", // [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
	"svc_effect2", // [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
};

//=============================================================================

cvar_t demo_nehahra = {"demo_nehahra", "0"};

void CL_Parse_Init(void)
{
	// LordHavoc: added demo_nehahra cvar
	Cvar_RegisterVariable (&demo_nehahra);
	if (nehahra)
		Cvar_SetValue("demo_nehahra", 1);
}

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
	/*
	if (num >= cl.num_entities)
	{
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %i is an invalid number",num);
		cl.num_entities = num;
//		while (cl.num_entities <= num)
//		{
//			cl_entities[cl.num_entities].colormap = -1; // no special coloring
//			cl.num_entities++;
//		}
	}
	*/
	if (num >= MAX_EDICTS)
		Host_Error ("CL_EntityNum: %i is an invalid number",num);
		
	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(int largesoundindex)
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
	if (largesoundindex)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();

	if (sound_num >= MAX_SOUNDS)
		Host_Error("CL_ParseStartSoundPacket: sound_num (%i) >= MAX_SOUNDS (%i)\n", sound_num, MAX_SOUNDS);

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
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

void CL_ParseEntityLump(char *entdata)
{
	char *data;
	char key[128], value[4096];
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
			if (hlbsp)
			{
				j = 0;
				for (i = 0;i < 4096;i++)
					if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
						break;
				if (value[i])
				{
					for (;i < 4096;i++)
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
							W_LoadTextureWadFile (wadname, false);
							j = i+1;
							if (!k)
								break;
						}
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
	cl_entities[0].render.model = cl.worldmodel = cl.model_precache[1];
	
	R_NewMap ();

	Hunk_Check ();		// make sure nothing is hurt
	
	noclip_anglehack = false;		// noclip is turned off at start	
}

void CL_ValidateState(entity_state_t *s)
{
	model_t *model;

	if (!s->active)
		return;

	if (s->modelindex >= MAX_MODELS)
		Host_Error("CL_ValidateState: modelindex (%i) >= MAX_MODELS (%i)\n", s->modelindex, MAX_MODELS);

	// colormap is client index + 1
	if (s->colormap > cl.maxclients)
		Host_Error ("CL_ValidateState: colormap (%i) > cl.maxclients (%i)", s->colormap, cl.maxclients);

	model = cl.model_precache[s->modelindex];
	if (model && s->frame >= model->numframes)
	{
		Con_DPrintf("CL_ValidateState: no such frame %i in \"%s\"\n", s->frame, model->name);
		s->frame = 0;
	}
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
byte entkill[MAX_EDICTS];
int bitprofile[32], bitprofilecount = 0;
void CL_ParseUpdate (int bits)
{
	int i, num, deltadie;
	entity_t *ent;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte()<<8);
	if ((bits & U_EXTEND1) && (!Nehahrademcompatibility))
	{
		bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}

	if (bits & U_LONGENTITY)	
		num = (unsigned) MSG_ReadShort ();
	else
		num = (unsigned) MSG_ReadByte ();

	if (num >= MAX_EDICTS)
		Host_Error("CL_ParseUpdate: entity number (%i) >= MAX_EDICTS (%i)\n", num, MAX_EDICTS);
	if (num < 1)
		Host_Error("CL_ParseUpdate: invalid entity number (%i)\n", num);

	// mark as visible (no kill)
	entkill[num] = 0;

	ent = CL_EntityNum (num);

	for (i = 0;i < 32;i++)
		if (bits & (1 << i))
			bitprofile[i]++;
	bitprofilecount++;

	ent->state_previous = ent->state_current;
	deltadie = false;
	if (bits & U_DELTA)
	{
		if (!ent->state_current.active)
			deltadie = true; // was not present in previous frame, leave hidden until next full update
	}
	else
		ent->state_current = ent->state_baseline;

	ent->state_current.time = cl.mtime[0];

	ent->state_current.flags = 0;
	ent->state_current.active = true;
	if (bits & U_MODEL)		ent->state_current.modelindex = (ent->state_current.modelindex & 0xFF00) | MSG_ReadByte();
	if (bits & U_FRAME)		ent->state_current.frame = (ent->state_current.frame & 0xFF00) | MSG_ReadByte();
	if (bits & U_COLORMAP)	ent->state_current.colormap = MSG_ReadByte();
	if (bits & U_SKIN)		ent->state_current.skin = MSG_ReadByte();
	if (bits & U_EFFECTS)	ent->state_current.effects = (ent->state_current.effects & 0xFF00) | MSG_ReadByte();
	if (bits & U_ORIGIN1)	ent->state_current.origin[0] = MSG_ReadCoord();
	if (bits & U_ANGLE1)	ent->state_current.angles[0] = MSG_ReadAngle();
	if (bits & U_ORIGIN2)	ent->state_current.origin[1] = MSG_ReadCoord();
	if (bits & U_ANGLE2)	ent->state_current.angles[1] = MSG_ReadAngle();
	if (bits & U_ORIGIN3)	ent->state_current.origin[2] = MSG_ReadCoord();
	if (bits & U_ANGLE3)	ent->state_current.angles[2] = MSG_ReadAngle();
	if (bits & U_STEP)		ent->state_current.flags |= RENDER_STEP;
	if (bits & U_ALPHA)		ent->state_current.alpha = MSG_ReadByte();
	if (bits & U_SCALE)		ent->state_current.scale = MSG_ReadByte();
	if (bits & U_EFFECTS2)	ent->state_current.effects = (ent->state_current.effects & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_GLOWSIZE)	ent->state_current.glowsize = MSG_ReadByte();
	if (bits & U_GLOWCOLOR)	ent->state_current.glowcolor = MSG_ReadByte();
	if (bits & U_GLOWTRAIL) ent->state_current.flags |= RENDER_GLOWTRAIL;
	if (bits & U_COLORMOD)	ent->state_current.colormod = MSG_ReadByte();
	if (bits & U_FRAME2)	ent->state_current.frame = (ent->state_current.frame & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_MODEL2)	ent->state_current.modelindex = (ent->state_current.modelindex & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_VIEWMODEL)	ent->state_current.flags |= RENDER_VIEWMODEL;
	if (bits & U_EXTERIORMODEL)	ent->state_current.flags |= RENDER_EXTERIORMODEL;

	// LordHavoc: to allow playback of the Nehahra movie
	if (Nehahrademcompatibility && (bits & U_EXTEND1))
	{
		// LordHavoc: evil format
		int i = MSG_ReadFloat();
		int j = MSG_ReadFloat() * 255.0f;
		if (i == 2)
		{
			if (MSG_ReadFloat())
				ent->state_current.effects |= EF_FULLBRIGHT;
		}
		if (j < 0)
			ent->state_current.alpha = 0;
		else if (j == 0 || j >= 255)
			ent->state_current.alpha = 255;
		else
			ent->state_current.alpha = j;
	}

	if (deltadie)
	{
		// hide the entity
		ent->state_current.active = false;
	}
	else
	{
		CL_ValidateState(&ent->state_current);

		/*
		if (!ent->state_current.active)
		{
			if (bits & U_DELTA)
			{
				if (bits & U_MODEL)
					Con_Printf("CL_ParseUpdate: delta NULL model on %i: %i %i\n", num, ent->state_previous.modelindex, ent->state_current.modelindex);
				else
					Con_Printf("CL_ParseUpdate: delta NULL model on %i: %i\n", num, ent->state_previous.modelindex);
			}
			else
			{
				if (bits & U_MODEL)
					Con_Printf("CL_ParseUpdate:       NULL model on %i: %i %i\n", num, ent->state_baseline.modelindex, ent->state_current.modelindex);
				else
					Con_Printf("CL_ParseUpdate:       NULL model on %i: %i\n", num, ent->state_baseline.modelindex);
			}
		}
		*/
	}
}

char *bitprofilenames[32] =
{
	"U_MOREBITS",
	"U_ORIGIN1",
	"U_ORIGIN2",
	"U_ORIGIN3",
	"U_ANGLE2",
	"U_STEP",
	"U_FRAME",
	"U_SIGNAL",
	"U_ANGLE1",
	"U_ANGLE3",
	"U_MODEL",
	"U_COLORMAP",
	"U_SKIN",
	"U_EFFECTS",
	"U_LONGENTITY",
	"U_EXTEND1",
	"U_DELTA",
	"U_ALPHA",
	"U_SCALE",
	"U_EFFECTS2",
	"U_GLOWSIZE",
	"U_GLOWCOLOR",
	"U_COLORMOD",
	"U_EXTEND2",
	"U_GLOWTRAIL",
	"U_VIEWMODEL",
	"U_FRAME2",
	"U_MODEL2",
	"U_EXTERIORMODEL",
	"U_UNUSED29",
	"U_UNUSED30",
	"U_EXTEND3",
};

void CL_BitProfile_f(void)
{
	int i;
	Con_Printf("bitprofile: %i updates\n");
	if (bitprofilecount)
		for (i = 0;i < 32;i++)
//			if (bitprofile[i])
				Con_Printf("%s: %i %3.2f%%\n", bitprofilenames[i], bitprofile[i], bitprofile[i] * 100.0 / bitprofilecount);
	Con_Printf("\n");
	for (i = 0;i < 32;i++)
		bitprofile[i] = 0;
	bitprofilecount = 0;
}

void CL_EntityUpdateSetup(void)
{
	memset(entkill, 1, MAX_EDICTS);
}

void CL_EntityUpdateEnd(void)
{
	int i;
	for (i = 1;i < MAX_EDICTS;i++)
		if (entkill[i])
			cl_entities[i].state_previous.active = cl_entities[i].state_current.active = 0;
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int large)
{
	int i;

	memset(&ent->state_baseline, 0, sizeof(entity_state_t));
	ent->state_baseline.active = true;
	if (large)
	{
		ent->state_baseline.modelindex = (unsigned short) MSG_ReadShort ();
		ent->state_baseline.frame = (unsigned short) MSG_ReadShort ();
	}
	else
	{
		ent->state_baseline.modelindex = MSG_ReadByte ();
		ent->state_baseline.frame = MSG_ReadByte ();
	}
	ent->state_baseline.colormap = MSG_ReadByte();
	ent->state_baseline.skin = MSG_ReadByte();
	for (i = 0;i < 3;i++)
	{
		ent->state_baseline.origin[i] = MSG_ReadCoord ();
		ent->state_baseline.angles[i] = MSG_ReadAngle ();
	}
	ent->state_baseline.alpha = 255;
	ent->state_baseline.scale = 16;
	ent->state_baseline.glowsize = 0;
	ent->state_baseline.glowcolor = 254;
	ent->state_baseline.colormod = 255;
	ent->state_previous = ent->state_current = ent->state_baseline;

	CL_ValidateState(&ent->state_baseline);
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (int bits)
{
	int i, j;

	bits &= 0xFFFF;
	if (bits & SU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & SU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);

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
		{
			if (dpprotocol)
				cl.punchangle[i] = MSG_ReadPreciseAngle();
			else
				cl.punchangle[i] = MSG_ReadChar();
		}
		else
			cl.punchangle[i] = 0;
		if (bits & (SU_PUNCHVEC1<<i))
			cl.punchvector[i] = MSG_ReadFloatCoord();
		else
			cl.punchvector[i] = 0;
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
void CL_ParseStatic (int large)
{
	entity_t *ent;
		
	if (cl.num_statics >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl.num_statics++];
	CL_ParseBaseline (ent, large);

// copy it to the current state
	ent->render.model = cl.model_precache[ent->state_baseline.modelindex];
	ent->render.frame = ent->render.frame1 = ent->render.frame2 = ent->state_baseline.frame;
	ent->render.framelerp = 0;
	ent->render.lerp_starttime = -1;
	// make torchs play out of sync
	ent->render.frame1start = ent->render.frame2start = -(rand() & 32767);
	ent->render.colormap = -1; // no special coloring
	ent->render.skinnum = ent->state_baseline.skin;
	ent->render.effects = ent->state_baseline.effects;
	ent->render.alpha = 1;
	ent->render.scale = 1;
	ent->render.alpha = 1;
	ent->render.glowsize = 0;
	ent->render.glowcolor = 254;
	ent->render.colormod[0] = ent->render.colormod[1] = ent->render.colormod[2] = 1;

	VectorCopy (ent->state_baseline.origin, ent->render.origin);
	VectorCopy (ent->state_baseline.angles, ent->render.angles);	
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int large)
{
	vec3_t		org;
	int			sound_num, vol, atten;

	MSG_ReadVector(org);
	if (large)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();
	
	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

void CL_ParseEffect (void)
{
	vec3_t		org;
	int			modelindex, startframe, framecount, framerate;

	MSG_ReadVector(org);
	modelindex = MSG_ReadByte ();
	startframe = MSG_ReadByte ();
	framecount = MSG_ReadByte ();
	framerate = MSG_ReadByte ();

	CL_Effect(org, modelindex, startframe, framecount, framerate);
}

void CL_ParseEffect2 (void)
{
	vec3_t		org;
	int			modelindex, startframe, framecount, framerate;

	MSG_ReadVector(org);
	modelindex = MSG_ReadShort ();
	startframe = MSG_ReadShort ();
	framecount = MSG_ReadByte ();
	framerate = MSG_ReadByte ();

	CL_Effect(org, modelindex, startframe, framecount, framerate);
}


#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", msg_readcount-1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	int			i, entitiesupdated;
	byte		cmdlog[32];
	char		*cmdlogname[32], *temp;
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

	entitiesupdated = false;
	CL_EntityUpdateSetup();
	
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;		// end of message
		}

		cmdindex = cmdcount & 31;
		cmdcount++;
		cmdlog[cmdindex] = cmd;

		// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128)
		{
			// LordHavoc: fix for bizarre problem in MSVC that I do not understand (if I assign the string pointer directly it ends up storing a NULL pointer)
			temp = "entity";
			cmdlogname[cmdindex] = temp;
			SHOWNET("fast update");
			CL_ParseUpdate (cmd&127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);
		cmdlogname[cmdindex] = svc_strings[cmd];
		if (!cmdlogname[cmdindex])
		{
			// LordHavoc: fix for bizarre problem in MSVC that I do not understand (if I assign the string pointer directly it ends up storing a NULL pointer)
			temp = "<unknown>";
			cmdlogname[cmdindex] = temp;
		}
	
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
			// handle old protocols which do not have entity update ranges
			entitiesupdated = true;
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();			
			break;
			
		case svc_clientdata:
			i = MSG_ReadShort ();
			CL_ParseClientdata (i);
			break;
		
		case svc_version:
			i = MSG_ReadLong ();
			if (i != PROTOCOL_VERSION && i != DPPROTOCOL_VERSION && i != 250)
				Host_Error ("CL_ParseServerMessage: Server is protocol %i, not %i or %i", i, DPPROTOCOL_VERSION, PROTOCOL_VERSION);
			Nehahrademcompatibility = false;
			if (i == 250)
				Nehahrademcompatibility = true;
			if (cls.demoplayback && demo_nehahra.value)
				Nehahrademcompatibility = true;
			dpprotocol = i == DPPROTOCOL_VERSION;
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
				Host_Error ("svc_lightstyle >= MAX_LIGHTSTYLES");
			strncpy (cl_lightstyle[i].map,  MSG_ReadString(), MAX_STYLESTRING - 1);
			cl_lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket(false);
			break;

		case svc_sound2:
			CL_ParseStartSoundPacket(true);
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;
		
		case svc_updatename:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename >= MAX_SCOREBOARD");
			strcpy (cl.scores[i].name, MSG_ReadString ());
			break;
			
		case svc_updatefrags:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags >= MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;			

		case svc_updatecolors:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors >= MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			break;
			
		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_effect:
			CL_ParseEffect ();
			break;

		case svc_effect2:
			CL_ParseEffect2 ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), false);
			break;
		case svc_spawnbaseline2:
			i = MSG_ReadShort ();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), true);
			break;
		case svc_spawnstatic:
			CL_ParseStatic (false);
			break;
		case svc_spawnstatic2:
			CL_ParseStatic (true);
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
			cl.stats[i] = MSG_ReadLong ();
			break;
			
		case svc_spawnstaticsound:
			CL_ParseStaticSound (false);
			break;

		case svc_spawnstaticsound2:
			CL_ParseStaticSound (true);
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
		}
	}

	if (entitiesupdated)
		CL_EntityUpdateEnd();
}
