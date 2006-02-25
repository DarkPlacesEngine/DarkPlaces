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
#include "cdaudio.h"
#include "cl_collision.h"
#include "csprogs.h"

char *svc_strings[128] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [int] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value

	"svc_serverinfo",		// [int] version
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
	"svc_skybox", // [string] skyname
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
	"svc_cgame", //				50		// [short] length [bytes] data
	"svc_updatestatubyte", //			51		// [byte] stat [byte] value
	"svc_effect", //			52		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
	"svc_effect2", //			53		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
	"svc_sound2", //			54		// short soundindex instead of byte
	"svc_spawnbaseline2", //	55		// short modelindex instead of byte
	"svc_spawnstatic2", //		56		// short modelindex instead of byte
	"svc_entities", //			57		// [int] deltaframe [int] thisframe [float vector] eye [variable length] entitydata
	"svc_csqcentities", //		58		// [short] entnum [variable length] entitydata ... [short] 0x0000
	"svc_spawnstaticsound2", //	59		// [coord3] [short] samp [byte] vol [byte] aten
};

char *qw_svc_strings[128] =
{
	"qw_svc_bad",					// 0
	"qw_svc_nop",					// 1
	"qw_svc_disconnect",			// 2
	"qw_svc_updatestat",			// 3	// [byte] [byte]
	"",								// 4
	"qw_svc_setview",				// 5	// [short] entity number
	"qw_svc_sound",					// 6	// <see code>
	"",								// 7
	"qw_svc_print",					// 8	// [byte] id [string] null terminated string
	"qw_svc_stufftext",				// 9	// [string] stuffed into client's console buffer
	"qw_svc_setangle",				// 10	// [angle3] set the view angle to this absolute value
	"qw_svc_serverdata",			// 11	// [long] protocol ...
	"qw_svc_lightstyle",			// 12	// [byte] [string]
	"",								// 13
	"qw_svc_updatefrags",			// 14	// [byte] [short]
	"",								// 15
	"qw_svc_stopsound",				// 16	// <see code>
	"",								// 17
	"",								// 18
	"qw_svc_damage",				// 19
	"qw_svc_spawnstatic",			// 20
	"",								// 21
	"qw_svc_spawnbaseline",			// 22
	"qw_svc_temp_entity",			// 23	// variable
	"qw_svc_setpause",				// 24	// [byte] on / off
	"",								// 25
	"qw_svc_centerprint",			// 26	// [string] to put in center of the screen
	"qw_svc_killedmonster",			// 27
	"qw_svc_foundsecret",			// 28
	"qw_svc_spawnstaticsound",		// 29	// [coord3] [byte] samp [byte] vol [byte] aten
	"qw_svc_intermission",			// 30		// [vec3_t] origin [vec3_t] angle
	"qw_svc_finale",				// 31		// [string] text
	"qw_svc_cdtrack",				// 32		// [byte] track
	"qw_svc_sellscreen",			// 33
	"qw_svc_smallkick",				// 34		// set client punchangle to 2
	"qw_svc_bigkick",				// 35		// set client punchangle to 4
	"qw_svc_updateping",			// 36		// [byte] [short]
	"qw_svc_updateentertime",		// 37		// [byte] [float]
	"qw_svc_updatestatlong",		// 38		// [byte] [long]
	"qw_svc_muzzleflash",			// 39		// [short] entity
	"qw_svc_updateuserinfo",		// 40		// [byte] slot [long] uid
	"qw_svc_download",				// 41		// [short] size [size bytes]
	"qw_svc_playerinfo",			// 42		// variable
	"qw_svc_nails",					// 43		// [byte] num [48 bits] xyzpy 12 12 12 4 8
	"qw_svc_chokecount",			// 44		// [byte] packets choked
	"qw_svc_modellist",				// 45		// [strings]
	"qw_svc_soundlist",				// 46		// [strings]
	"qw_svc_packetentities",		// 47		// [...]
	"qw_svc_deltapacketentities",	// 48		// [...]
	"qw_svc_maxspeed",				// 49		// maxspeed change, for prediction
	"qw_svc_entgravity",			// 50		// gravity change, for prediction
	"qw_svc_setinfo",				// 51		// setinfo on a client
	"qw_svc_serverinfo",			// 52		// serverinfo
	"qw_svc_updatepl",				// 53		// [byte] [byte]
};

//=============================================================================

cvar_t demo_nehahra = {0, "demo_nehahra", "0", "reads all quake demos as nehahra movie protocol"};
cvar_t developer_networkentities = {0, "developer_networkentities", "0", "prints received entities, value is 0-4 (higher for more info)"};

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

	field_mask = MSG_ReadByte();

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if (field_mask & SND_LARGEENTITY)
	{
		ent = (unsigned short) MSG_ReadShort ();
		channel = MSG_ReadByte ();
	}
	else
	{
		channel = (unsigned short) MSG_ReadShort ();
		ent = channel >> 3;
		channel &= 7;
	}

	if (largesoundindex || field_mask & SND_LARGESOUND)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();

	MSG_ReadVector(pos, cls.protocol);

	if (sound_num >= MAX_SOUNDS)
	{
		Con_Printf("CL_ParseStartSoundPacket: sound_num (%i) >= MAX_SOUNDS (%i)\n", sound_num, MAX_SOUNDS);
		return;
	}

	if (ent >= MAX_EDICTS)
	{
		Con_Printf("CL_ParseStartSoundPacket: ent = %i", ent);
		return;
	}

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0f, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/

static unsigned char olddata[NET_MAXMESSAGE];
void CL_KeepaliveMessage (void)
{
	float time;
	static double nextmsg = -1;
	int oldreadcount;
	qboolean oldbadread;
	sizebuf_t old;

	// no need if server is local and definitely not if this is a demo
	if (sv.active || !cls.netcon || cls.protocol == PROTOCOL_QUAKEWORLD)
		return;

// read messages from server, should just be nops
	oldreadcount = msg_readcount;
	oldbadread = msg_badread;
	old = net_message;
	memcpy(olddata, net_message.data, net_message.cursize);

	NetConn_ClientFrame();

	msg_readcount = oldreadcount;
	msg_badread = oldbadread;
	net_message = old;
	memcpy(net_message.data, olddata, net_message.cursize);

	if (cls.netcon && (time = Sys_DoubleTime()) >= nextmsg)
	{
		sizebuf_t	msg;
		unsigned char		buf[4];
		nextmsg = time + 5;
		// write out a nop
		// LordHavoc: must use unreliable because reliable could kill the sigon message!
		Con_Print("--> client to server keepalive\n");
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.maxsize = sizeof(buf);
		MSG_WriteChar(&msg, svc_nop);
		NetConn_SendUnreliableMessage(cls.netcon, &msg, cls.protocol);
	}
}

void CL_ParseEntityLump(char *entdata)
{
	const char *data;
	char key[128], value[MAX_INPUTLINE];
	FOG_clear(); // LordHavoc: no fog until set
	// LordHavoc: default to the map's sky (q3 shader parsing sets this)
	R_SetSkyBox(cl.worldmodel->brush.skybox);
	data = entdata;
	if (!data)
		return;
	if (!COM_ParseToken(&data, false))
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		if (!COM_ParseToken(&data, false))
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			strlcpy (key, com_token + 1, sizeof (key));
		else
			strlcpy (key, com_token, sizeof (key));
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		if (!COM_ParseToken(&data, false))
			return; // error
		strlcpy (value, com_token, sizeof (value));
		if (!strcmp("sky", key))
			R_SetSkyBox(value);
		else if (!strcmp("skyname", key)) // non-standard, introduced by QuakeForge... sigh.
			R_SetSkyBox(value);
		else if (!strcmp("qlsky", key)) // non-standard, introduced by QuakeLives (EEK)
			R_SetSkyBox(value);
		else if (!strcmp("fog", key))
			sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
		else if (!strcmp("fog_density", key))
			fog_density = atof(value);
		else if (!strcmp("fog_red", key))
			fog_red = atof(value);
		else if (!strcmp("fog_green", key))
			fog_green = atof(value);
		else if (!strcmp("fog_blue", key))
			fog_blue = atof(value);
	}
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
static void CL_SignonReply (void)
{
	Con_DPrintf("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		if (cls.netcon)
		{
			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, "prespawn");
		}
		break;

	case 2:
		if (cls.netcon)
		{
			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, va("name \"%s\"", cl_name.string));

			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, va("color %i %i", cl_color.integer >> 4, cl_color.integer & 15));

			if (cl_pmodel.integer)
			{
				MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
				MSG_WriteString (&cls.netcon->message, va("pmodel %i", cl_pmodel.integer));
			}
			if (*cl_playermodel.string)
			{
				MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
				MSG_WriteString (&cls.netcon->message, va("playermodel %s", cl_playermodel.string));
			}
			if (*cl_playerskin.string)
			{
				MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
				MSG_WriteString (&cls.netcon->message, va("playerskin %s", cl_playerskin.string));
			}

			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, va("rate %i", cl_rate.integer));

			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, "spawn");
		}
		break;

	case 3:
		if (cls.netcon)
		{
			MSG_WriteByte (&cls.netcon->message, clc_stringcmd);
			MSG_WriteString (&cls.netcon->message, "begin");
		}
		break;

	case 4:
		Con_ClearNotify();
		break;
	}
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo (void)
{
	char *str;
	int i;
	protocolversion_t protocol;
	int nummodels, numsounds;
	entity_t *ent;

	Con_DPrint("Serverinfo packet received.\n");

	// check memory integrity
	Mem_CheckSentinelsGlobal();

//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	protocol = Protocol_EnumForNumber(i);
	if (protocol == PROTOCOL_UNKNOWN)
	{
		Host_Error("CL_ParseServerInfo: Server is unrecognized protocol number (%i)", i);
		return;
	}
	// hack for unmarked Nehahra movie demos which had a custom protocol
	if (protocol == PROTOCOL_QUAKEDP && cls.demoplayback && demo_nehahra.integer)
		protocol = PROTOCOL_NEHAHRAMOVIE;
	cls.protocol = protocol;
	Con_DPrintf("Server protocol is %s\n", Protocol_NameForEnum(cls.protocol));

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Host_Error("Bad maxclients (%u) from server", cl.maxclients);
		return;
	}
	cl.scores = (scoreboard_t *)Mem_Alloc(cl_mempool, cl.maxclients*sizeof(*cl.scores));

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof(cl.levelname));

// seperate the printfs so the server message can have a color
	if (cls.protocol != PROTOCOL_NEHAHRAMOVIE) // no messages when playing the Nehahra movie
		Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n\2%s\n", str);

	// check memory integrity
	Mem_CheckSentinelsGlobal();

	S_StopAllSounds();
	// if server is active, we already began a loading plaque
	if (!sv.active)
		SCR_BeginLoadingPlaque();

	// disable until we get textures for it
	R_ResetSkyBox();

	memset(cl.csqc_model_precache, 0, sizeof(cl.csqc_model_precache));	//[515]: csqc
	memset(cl.model_precache, 0, sizeof(cl.model_precache));
	memset(cl.sound_precache, 0, sizeof(cl.sound_precache));

	// parse model precache list
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (nummodels==MAX_MODELS)
			Host_Error ("Server sent too many model precaches");
		if (strlen(str) >= MAX_QPATH)
			Host_Error ("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strlcpy (cl.model_name[nummodels], str, sizeof (cl.model_name[nummodels]));
	}
	// parse sound precache list
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (numsounds==MAX_SOUNDS)
			Host_Error("Server sent too many sound precaches");
		if (strlen(str) >= MAX_QPATH)
			Host_Error("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strlcpy (cl.sound_name[numsounds], str, sizeof (cl.sound_name[numsounds]));
	}

	// touch all of the precached models that are still loaded so we can free
	// anything that isn't needed
	Mod_ClearUsed();
	for (i = 1;i < nummodels;i++)
		Mod_FindName(cl.model_name[i]);
	// precache any models used by the client (this also marks them used)
	cl.model_bolt = Mod_ForName("progs/bolt.mdl", false, false, false);
	cl.model_bolt2 = Mod_ForName("progs/bolt2.mdl", false, false, false);
	cl.model_bolt3 = Mod_ForName("progs/bolt3.mdl", false, false, false);
	cl.model_beam = Mod_ForName("progs/beam.mdl", false, false, false);
	Mod_PurgeUnused();

	// do the same for sounds
	// FIXME: S_ServerSounds does not know about cl.sfx_ sounds
	S_ServerSounds (cl.sound_name, numsounds);

	// precache any sounds used by the client
	cl.sfx_wizhit = S_PrecacheSound("sound/wizard/hit.wav", false, true);
	cl.sfx_knighthit = S_PrecacheSound("sound/hknight/hit.wav", false, true);
	cl.sfx_tink1 = S_PrecacheSound("sound/weapons/tink1.wav", false, true);
	cl.sfx_ric1 = S_PrecacheSound("sound/weapons/ric1.wav", false, true);
	cl.sfx_ric2 = S_PrecacheSound("sound/weapons/ric2.wav", false, true);
	cl.sfx_ric3 = S_PrecacheSound("sound/weapons/ric3.wav", false, true);
	cl.sfx_r_exp3 = S_PrecacheSound("sound/weapons/r_exp3.wav", false, true);

	// now we try to load everything that is new

	// world model
	CL_KeepaliveMessage ();
	cl.model_precache[1] = Mod_ForName(cl.model_name[1], false, false, true);
	if (cl.model_precache[1]->Draw == NULL)
		Con_Printf("Map %s not found\n", cl.model_name[1]);

	// normal models
	for (i=2 ; i<nummodels ; i++)
	{
		CL_KeepaliveMessage();
		if ((cl.model_precache[i] = Mod_ForName(cl.model_name[i], false, false, false))->Draw == NULL)
			Con_Printf("Model %s not found\n", cl.model_name[i]);
	}

	// sounds
	for (i=1 ; i<numsounds ; i++)
	{
		CL_KeepaliveMessage();

		// Don't lock the sfx here, S_ServerSounds already did that
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i], true, false);
	}

	// local state
	ent = &cl_entities[0];
	// entire entity array was cleared, so just fill in a few fields
	ent->state_current.active = true;
	ent->render.model = cl.worldmodel = cl.model_precache[1];
	ent->render.scale = 1; // some of the renderer still relies on scale
	ent->render.alpha = 1;
	ent->render.colormap = -1; // no special coloring
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);

	cl_num_entities = 1;

	R_Modules_NewMap();
	CL_CGVM_Start();

	// noclip is turned off at start
	noclip_anglehack = false;

	// check memory integrity
	Mem_CheckSentinelsGlobal();
}

void CL_ValidateState(entity_state_t *s)
{
	model_t *model;

	if (!s->active)
		return;

	if (s->modelindex >= MAX_MODELS && (65536-s->modelindex) >= MAX_MODELS)
		Host_Error("CL_ValidateState: modelindex (%i) >= MAX_MODELS (%i)\n", s->modelindex, MAX_MODELS);

	// colormap is client index + 1
	if ((!s->flags & RENDER_COLORMAPPED) && s->colormap > cl.maxclients)
	{
		Con_DPrintf("CL_ValidateState: colormap (%i) > cl.maxclients (%i)\n", s->colormap, cl.maxclients);
		s->colormap = 0;
	}

	model = cl.model_precache[s->modelindex];
	if (model && model->type && s->frame >= model->numframes)
	{
		Con_DPrintf("CL_ValidateState: no such frame %i in \"%s\" (which has %i frames)\n", s->frame, model->name, model->numframes);
		s->frame = 0;
	}
	if (model && model->type && s->skin > 0 && s->skin >= model->numskins && !(s->lightpflags & PFLAGS_FULLDYNAMIC))
	{
		Con_DPrintf("CL_ValidateState: no such skin %i in \"%s\" (which has %i skins)\n", s->skin, model->name, model->numskins);
		s->skin = 0;
	}
}

void CL_MoveLerpEntityStates(entity_t *ent)
{
	float odelta[3], adelta[3];
	CL_ValidateState(&ent->state_current);
	VectorSubtract(ent->state_current.origin, ent->persistent.neworigin, odelta);
	VectorSubtract(ent->state_current.angles, ent->persistent.newangles, adelta);
	if (!ent->state_previous.active || ent->state_previous.modelindex != ent->state_current.modelindex)
	{
		// reset all persistent stuff if this is a new entity
		ent->persistent.lerpdeltatime = 0;
		ent->persistent.lerpstarttime = cl.mtime[1];
		VectorCopy(ent->state_current.origin, ent->persistent.oldorigin);
		VectorCopy(ent->state_current.angles, ent->persistent.oldangles);
		VectorCopy(ent->state_current.origin, ent->persistent.neworigin);
		VectorCopy(ent->state_current.angles, ent->persistent.newangles);
		// reset animation interpolation as well
		ent->render.frame = ent->render.frame1 = ent->render.frame2 = ent->state_current.frame;
		ent->render.frame1time = ent->render.frame2time = cl.time;
		ent->render.framelerp = 1;
		// reset various persistent stuff
		ent->persistent.muzzleflash = 0;
		VectorCopy(ent->state_current.origin, ent->persistent.trail_origin);
	}
	else if (cls.timedemo || cl_nolerp.integer || DotProduct(odelta, odelta) > 1000*1000)
	{
		// don't interpolate the move
		ent->persistent.lerpdeltatime = 0;
		ent->persistent.lerpstarttime = cl.mtime[1];
		VectorCopy(ent->state_current.origin, ent->persistent.oldorigin);
		VectorCopy(ent->state_current.angles, ent->persistent.oldangles);
		VectorCopy(ent->state_current.origin, ent->persistent.neworigin);
		VectorCopy(ent->state_current.angles, ent->persistent.newangles);
	}
	else if (ent->state_current.flags & RENDER_STEP)
	{
		// monster interpolation
		if (DotProduct(odelta, odelta) + DotProduct(adelta, adelta) > 0.01)
		{
			ent->persistent.lerpdeltatime = bound(0, cl.mtime[1] - ent->persistent.lerpstarttime, 0.1);
			ent->persistent.lerpstarttime = cl.mtime[1];
			VectorCopy(ent->persistent.neworigin, ent->persistent.oldorigin);
			VectorCopy(ent->persistent.newangles, ent->persistent.oldangles);
			VectorCopy(ent->state_current.origin, ent->persistent.neworigin);
			VectorCopy(ent->state_current.angles, ent->persistent.newangles);
		}
	}
	else
	{
		// not a monster
		ent->persistent.lerpstarttime = ent->state_previous.time;
		// no lerp if it's singleplayer
		if (cl.islocalgame && !sv_fixedframeratesingleplayer.integer)
			ent->persistent.lerpdeltatime = 0;
		else
			ent->persistent.lerpdeltatime = bound(0, ent->state_current.time - ent->state_previous.time, 0.1);
		VectorCopy(ent->persistent.neworigin, ent->persistent.oldorigin);
		VectorCopy(ent->persistent.newangles, ent->persistent.oldangles);
		VectorCopy(ent->state_current.origin, ent->persistent.neworigin);
		VectorCopy(ent->state_current.angles, ent->persistent.newangles);
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int large)
{
	int i;

	ent->state_baseline = defaultstate;
	// FIXME: set ent->state_baseline.number?
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
		ent->state_baseline.origin[i] = MSG_ReadCoord(cls.protocol);
		ent->state_baseline.angles[i] = MSG_ReadAngle(cls.protocol);
	}
	CL_ValidateState(&ent->state_baseline);
	ent->state_previous = ent->state_current = ent->state_baseline;
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (void)
{
	int i, j, bits;

	VectorCopy (cl.mpunchangle[0], cl.mpunchangle[1]);
	VectorCopy (cl.mpunchvector[0], cl.mpunchvector[1]);
	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	cl.mviewzoom[1] = cl.mviewzoom[0];

	if (cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE || cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4 || cls.protocol == PROTOCOL_DARKPLACES5)
	{
		cl.stats[STAT_VIEWHEIGHT] = DEFAULT_VIEWHEIGHT;
		cl.stats[STAT_ITEMS] = 0;
		cl.stats[STAT_VIEWZOOM] = 255;
	}
	cl.idealpitch = 0;
	cl.mpunchangle[0][0] = 0;
	cl.mpunchangle[0][1] = 0;
	cl.mpunchangle[0][2] = 0;
	cl.mpunchvector[0][0] = 0;
	cl.mpunchvector[0][1] = 0;
	cl.mpunchvector[0][2] = 0;
	cl.mvelocity[0][0] = 0;
	cl.mvelocity[0][1] = 0;
	cl.mvelocity[0][2] = 0;
	cl.mviewzoom[0] = 1;

	bits = (unsigned short) MSG_ReadShort ();
	if (bits & SU_EXTEND1)
		bits |= (MSG_ReadByte() << 16);
	if (bits & SU_EXTEND2)
		bits |= (MSG_ReadByte() << 24);

	if (bits & SU_VIEWHEIGHT)
		cl.stats[STAT_VIEWHEIGHT] = MSG_ReadChar ();

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = MSG_ReadChar ();

	for (i = 0;i < 3;i++)
	{
		if (bits & (SU_PUNCH1<<i) )
		{
			if (cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE)
				cl.mpunchangle[0][i] = MSG_ReadChar();
			else
				cl.mpunchangle[0][i] = MSG_ReadAngle16i();
		}
		if (bits & (SU_PUNCHVEC1<<i))
		{
			if (cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4)
				cl.mpunchvector[0][i] = MSG_ReadCoord16i();
			else
				cl.mpunchvector[0][i] = MSG_ReadCoord32f();
		}
		if (bits & (SU_VELOCITY1<<i) )
		{
			if (cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE || cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4)
				cl.mvelocity[0][i] = MSG_ReadChar()*16;
			else
				cl.mvelocity[0][i] = MSG_ReadCoord32f();
		}
	}

	// LordHavoc: hipnotic demos don't have this bit set but should
	if (bits & SU_ITEMS || cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE || cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4 || cls.protocol == PROTOCOL_DARKPLACES5)
		cl.stats[STAT_ITEMS] = MSG_ReadLong ();

	cl.onground = (bits & SU_ONGROUND) != 0;
	csqc_onground = cl.onground;	//[515]: cause without this csqc will receive not right value on svc_print =/
	cl.inwater = (bits & SU_INWATER) != 0;

	if (cls.protocol == PROTOCOL_DARKPLACES5)
	{
		cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadShort() : 0;
		cl.stats[STAT_ARMOR] = (bits & SU_ARMOR) ? MSG_ReadShort() : 0;
		cl.stats[STAT_WEAPON] = (bits & SU_WEAPON) ? MSG_ReadShort() : 0;
		cl.stats[STAT_HEALTH] = MSG_ReadShort();
		cl.stats[STAT_AMMO] = MSG_ReadShort();
		cl.stats[STAT_SHELLS] = MSG_ReadShort();
		cl.stats[STAT_NAILS] = MSG_ReadShort();
		cl.stats[STAT_ROCKETS] = MSG_ReadShort();
		cl.stats[STAT_CELLS] = MSG_ReadShort();
		cl.stats[STAT_ACTIVEWEAPON] = (unsigned short) MSG_ReadShort ();
	}
	else if (cls.protocol == PROTOCOL_QUAKE || cls.protocol == PROTOCOL_QUAKEDP || cls.protocol == PROTOCOL_NEHAHRAMOVIE || cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4)
	{
		cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadByte() : 0;
		cl.stats[STAT_ARMOR] = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;
		cl.stats[STAT_WEAPON] = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;
		cl.stats[STAT_HEALTH] = MSG_ReadShort();
		cl.stats[STAT_AMMO] = MSG_ReadByte();
		cl.stats[STAT_SHELLS] = MSG_ReadByte();
		cl.stats[STAT_NAILS] = MSG_ReadByte();
		cl.stats[STAT_ROCKETS] = MSG_ReadByte();
		cl.stats[STAT_CELLS] = MSG_ReadByte();
		if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE || gamemode == GAME_NEXUIZ)
			cl.stats[STAT_ACTIVEWEAPON] = (1<<MSG_ReadByte ());
		else
			cl.stats[STAT_ACTIVEWEAPON] = MSG_ReadByte ();
	}

	if (bits & SU_VIEWZOOM)
	{
		if (cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3 || cls.protocol == PROTOCOL_DARKPLACES4)
			cl.stats[STAT_VIEWZOOM] = MSG_ReadByte();
		else
			cl.stats[STAT_VIEWZOOM] = (unsigned short) MSG_ReadShort();
	}

	// check for important changes

	// set flash times
	if (cl.olditems != cl.stats[STAT_ITEMS])
		for (j = 0;j < 32;j++)
			if ((cl.stats[STAT_ITEMS] & (1<<j)) && !(cl.olditems & (1<<j)))
				cl.item_gettime[j] = cl.time;
	cl.olditems = cl.stats[STAT_ITEMS];

	// GAME_NEXUIZ hud needs weapon change time
	if (cl.activeweapon != cl.stats[STAT_ACTIVEWEAPON])
		cl.weapontime = cl.time;
	cl.activeweapon = cl.stats[STAT_ACTIVEWEAPON];

	// viewzoom interpolation
	cl.mviewzoom[0] = (float) max(cl.stats[STAT_VIEWZOOM], 2) * (1.0f / 255.0f);
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int large)
{
	entity_t *ent;

	if (cl_num_static_entities >= cl_max_static_entities)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[cl_num_static_entities++];
	CL_ParseBaseline (ent, large);

// copy it to the current state
	ent->render.model = cl.model_precache[ent->state_baseline.modelindex];
	ent->render.frame = ent->render.frame1 = ent->render.frame2 = ent->state_baseline.frame;
	ent->render.framelerp = 0;
	// make torchs play out of sync
	ent->render.frame1time = ent->render.frame2time = lhrandom(-10, -1);
	ent->render.colormap = -1; // no special coloring
	ent->render.skinnum = ent->state_baseline.skin;
	ent->render.effects = ent->state_baseline.effects;
	ent->render.alpha = 1;
	//ent->render.scale = 1;

	//VectorCopy (ent->state_baseline.origin, ent->render.origin);
	//VectorCopy (ent->state_baseline.angles, ent->render.angles);

	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, ent->state_baseline.origin[0], ent->state_baseline.origin[1], ent->state_baseline.origin[2], ent->state_baseline.angles[0], ent->state_baseline.angles[1], ent->state_baseline.angles[2], 1);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);

	// This is definitely cheating...
	if (ent->render.model == NULL)
		cl_num_static_entities--;
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

	MSG_ReadVector(org, cls.protocol);
	if (large)
		sound_num = (unsigned short) MSG_ReadShort ();
	else
		sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	S_StaticSound (cl.sound_precache[sound_num], org, vol/255.0f, atten);
}

void CL_ParseEffect (void)
{
	vec3_t		org;
	int			modelindex, startframe, framecount, framerate;

	MSG_ReadVector(org, cls.protocol);
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

	MSG_ReadVector(org, cls.protocol);
	modelindex = (unsigned short) MSG_ReadShort ();
	startframe = (unsigned short) MSG_ReadShort ();
	framecount = MSG_ReadByte ();
	framerate = MSG_ReadByte ();

	CL_Effect(org, modelindex, startframe, framecount, framerate);
}

void CL_ParseBeam (model_t *m, int lightning)
{
	int i, ent;
	vec3_t start, end;
	beam_t *b = NULL;

	ent = (unsigned short) MSG_ReadShort ();
	MSG_ReadVector(start, cls.protocol);
	MSG_ReadVector(end, cls.protocol);

	if (ent >= MAX_EDICTS)
	{
		Con_Printf("CL_ParseBeam: invalid entity number %i\n", ent);
		ent = 0;
	}

	if (ent >= cl_max_entities)
		CL_ExpandEntities(ent);

	// override any beam with the same entity
	i = cl_max_beams;
	if (ent)
		for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
			if (b->entity == ent)
				break;
	// if the entity was not found then just replace an unused beam
	if (i == cl_max_beams)
		for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
			if (!b->model || b->endtime < cl.time)
				break;
	if (i < cl_max_beams)
	{
		b->entity = ent;
		b->lightning = lightning;
		b->model = m;
		b->endtime = cl.time + 0.2;
		VectorCopy (start, b->start);
		VectorCopy (end, b->end);
		b->relativestartvalid = 0;
		if (ent && cl_entities[ent].state_current.active)
		{
			entity_state_t *p;
			matrix4x4_t matrix, imatrix;
			if (ent == cl.viewentity && cl.movement)
				p = &cl_entities[b->entity].state_previous;
			else
				p = &cl_entities[b->entity].state_current;
			// not really valid yet, we need to get the orientation now
			// (ParseBeam flagged this because it is received before
			//  entities are received, by now they have been received)
			// note: because players create lightning in their think
			// function (which occurs before movement), they actually
			// have some lag in it's location, so compare to the
			// previous player state, not the latest
			Matrix4x4_CreateFromQuakeEntity(&matrix, p->origin[0], p->origin[1], p->origin[2], -p->angles[0], p->angles[1], p->angles[2], 1);
			Matrix4x4_Invert_Simple(&imatrix, &matrix);
			Matrix4x4_Transform(&imatrix, b->start, b->relativestart);
			Matrix4x4_Transform(&imatrix, b->end, b->relativeend);
			b->relativestartvalid = 1;
		}
	}
	else
		Con_Print("beam list overflow!\n");
}

void CL_ParseTempEntity(void)
{
	int type;
	vec3_t pos;
	vec3_t dir;
	vec3_t pos2;
	vec3_t color;
	int rnd;
	int colorStart, colorLength, count;
	float velspeed, radius;
	unsigned char *tempcolor;
	matrix4x4_t tempmatrix;

	type = MSG_ReadByte();
	switch (type)
	{
	case TE_WIZSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		//CL_AllocDlight(NULL, &tempmatrix, 100, 0.12f, 0.50f, 0.12f, 500, 0.2, 0, -1, false, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		CL_RunParticleEffect(pos, vec3_origin, 20, 30);
		S_StartSound(-1, 0, cl.sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		//CL_AllocDlight(NULL, &tempmatrix, 100, 0.50f, 0.30f, 0.10f, 500, 0.2, 0, -1, false, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		CL_RunParticleEffect(pos, vec3_origin, 226, 20);
		S_StartSound(-1, 0, cl.sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:
		// spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 10);
		else if (cl_particles_bulletimpacts.integer)
		{
			CL_SparkShower(pos, vec3_origin, 15, 1);
			CL_Smoke(pos, vec3_origin, 15);
		}
		CL_BulletMark(pos);
		if (rand() % 5)
			S_StartSound(-1, 0, cl.sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl.sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl.sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl.sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SPIKEQUAD:
		// quad spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 10);
		else if (cl_particles_bulletimpacts.integer)
		{
			CL_SparkShower(pos, vec3_origin, 15, 1);
			CL_Smoke(pos, vec3_origin, 15);
		}
		CL_BulletMark(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (rand() % 5)
			S_StartSound(-1, 0, cl.sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl.sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl.sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl.sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:
		// super spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 20);
		else if (cl_particles_bulletimpacts.integer)
		{
			CL_SparkShower(pos, vec3_origin, 30, 1);
			CL_Smoke(pos, vec3_origin, 30);
		}
		CL_BulletMark(pos);
		if (rand() % 5)
			S_StartSound(-1, 0, cl.sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl.sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl.sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl.sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKEQUAD:
		// quad super spike hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 20);
		else if (cl_particles_bulletimpacts.integer)
		{
			CL_SparkShower(pos, vec3_origin, 30, 1);
			CL_Smoke(pos, vec3_origin, 30);
		}
		CL_BulletMark(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (rand() % 5)
			S_StartSound(-1, 0, cl.sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl.sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl.sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl.sfx_ric3, pos, 1, 1);
		}
		break;
		// LordHavoc: added for improved blood splatters
	case TE_BLOOD:
		// blood puff
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		dir[0] = MSG_ReadChar();
		dir[1] = MSG_ReadChar();
		dir[2] = MSG_ReadChar();
		count = MSG_ReadByte();
		CL_BloodPuff(pos, dir, count);
		break;
	case TE_SPARK:
		// spark shower
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		dir[0] = MSG_ReadChar();
		dir[1] = MSG_ReadChar();
		dir[2] = MSG_ReadChar();
		count = MSG_ReadByte();
		CL_SparkShower(pos, dir, count, 1);
		break;
	case TE_PLASMABURN:
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 200, 1, 1, 1, 1000, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		CL_PlasmaBurn(pos);
		break;
		// LordHavoc: added for improved gore
	case TE_BLOODSHOWER:
		// vaporized body
		MSG_ReadVector(pos, cls.protocol); // mins
		MSG_ReadVector(pos2, cls.protocol); // maxs
		velspeed = MSG_ReadCoord(cls.protocol); // speed
		count = (unsigned short) MSG_ReadShort(); // number of particles
		CL_BloodShower(pos, pos2, velspeed, count);
		break;
	case TE_PARTICLECUBE:
		// general purpose particle effect
		MSG_ReadVector(pos, cls.protocol); // mins
		MSG_ReadVector(pos2, cls.protocol); // maxs
		MSG_ReadVector(dir, cls.protocol); // dir
		count = (unsigned short) MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		colorLength = MSG_ReadByte(); // gravity (1 or 0)
		velspeed = MSG_ReadCoord(cls.protocol); // randomvel
		CL_ParticleCube(pos, pos2, dir, count, colorStart, colorLength, velspeed);
		break;

	case TE_PARTICLERAIN:
		// general purpose particle effect
		MSG_ReadVector(pos, cls.protocol); // mins
		MSG_ReadVector(pos2, cls.protocol); // maxs
		MSG_ReadVector(dir, cls.protocol); // dir
		count = (unsigned short) MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		CL_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		break;

	case TE_PARTICLESNOW:
		// general purpose particle effect
		MSG_ReadVector(pos, cls.protocol); // mins
		MSG_ReadVector(pos2, cls.protocol); // maxs
		MSG_ReadVector(dir, cls.protocol); // dir
		count = (unsigned short) MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		CL_ParticleRain(pos, pos2, dir, count, colorStart, 1);
		break;

	case TE_GUNSHOT:
		// bullet hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 20);
		else
		{
			CL_SparkShower(pos, vec3_origin, 15, 1);
			CL_Smoke(pos, vec3_origin, 15);
		}
		CL_BulletMark(pos);
		break;

	case TE_GUNSHOTQUAD:
		// quad bullet hitting wall
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		if (cl_particles_quake.integer)
			CL_RunParticleEffect(pos, vec3_origin, 0, 20);
		else
		{
			CL_SparkShower(pos, vec3_origin, 15, 1);
			CL_Smoke(pos, vec3_origin, 15);
		}
		CL_BulletMark(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		break;

	case TE_EXPLOSION:
		// rocket explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		// LordHavoc: boosted color from 1.0, 0.8, 0.4 to 1.25, 1.0, 0.5
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, 4.0f, 2.0f, 0.50f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONQUAD:
		// quad rocket explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, 2.5f, 2.0f, 4.0f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSION3:
		// Nehahra movie colored lighting explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		color[0] = MSG_ReadCoord(cls.protocol) * (2.0f / 1.0f);
		color[1] = MSG_ReadCoord(cls.protocol) * (2.0f / 1.0f);
		color[2] = MSG_ReadCoord(cls.protocol) * (2.0f / 1.0f);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONRGB:
		// colored lighting explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:
		// tarbaby explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_BlobExplosion(pos);

		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 600, 1.6f, 0.8f, 2.0f, 1200, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		break;

	case TE_SMALLFLASH:
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 200, 2, 2, 2, 1000, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		break;

	case TE_CUSTOMFLASH:
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 4);
		radius = (MSG_ReadByte() + 1) * 8;
		velspeed = (MSG_ReadByte() + 1) * (1.0 / 256.0);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, radius, color[0], color[1], color[2], radius / velspeed, velspeed, 0, -1, true, 1, 0.25, 1, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		break;

	case TE_FLAMEJET:
		MSG_ReadVector(pos, cls.protocol);
		MSG_ReadVector(dir, cls.protocol);
		count = MSG_ReadByte();
		CL_Flames(pos, dir, count);
		break;

	case TE_LIGHTNING1:
		// lightning bolts
		CL_ParseBeam(cl.model_bolt, true);
		break;

	case TE_LIGHTNING2:
		// lightning bolts
		CL_ParseBeam(cl.model_bolt2, true);
		break;

	case TE_LIGHTNING3:
		// lightning bolts
		CL_ParseBeam(cl.model_bolt3, false);
		break;

// PGM 01/21/97
	case TE_BEAM:
		// grappling hook beam
		CL_ParseBeam(cl.model_beam, false);
		break;
// PGM 01/21/97

// LordHavoc: for compatibility with the Nehahra movie...
	case TE_LIGHTNING4NEH:
		CL_ParseBeam(Mod_ForName(MSG_ReadString(), true, false, false), false);
		break;

	case TE_LAVASPLASH:
		MSG_ReadVector(pos, cls.protocol);
		CL_LavaSplash(pos);
		break;

	case TE_TELEPORT:
		MSG_ReadVector(pos, cls.protocol);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 200, 1.0f, 1.0f, 1.0f, 600, 99.0f, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		CL_TeleportSplash(pos);
		break;

	case TE_EXPLOSION2:
		// color mapped explosion
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		CL_ParticleExplosion2(pos, colorStart, colorLength);
		tempcolor = (unsigned char *)&palette_complete[(rand()%colorLength) + colorStart];
		color[0] = tempcolor[0] * (2.0f / 255.0f);
		color[1] = tempcolor[1] * (2.0f / 255.0f);
		color[2] = tempcolor[2] * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TEI_G3:
		MSG_ReadVector(pos, cls.protocol);
		MSG_ReadVector(pos2, cls.protocol);
		MSG_ReadVector(dir, cls.protocol);
		CL_BeamParticle(pos, pos2, 8, 1, 1, 1, 1, 1);
		break;

	case TE_TEI_SMOKE:
		MSG_ReadVector(pos, cls.protocol);
		MSG_ReadVector(dir, cls.protocol);
		count = MSG_ReadByte();
		CL_FindNonSolidLocation(pos, pos, 4);
		CL_Tei_Smoke(pos, dir, count);
		break;

	case TE_TEI_BIGEXPLOSION:
		MSG_ReadVector(pos, cls.protocol);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 500, 2.5f, 2.0f, 1.0f, 500, 9999, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		if (gamemode != GAME_NEXUIZ)
			S_StartSound(-1, 0, cl.sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TEI_PLASMAHIT:
		MSG_ReadVector(pos, cls.protocol);
		MSG_ReadVector(dir, cls.protocol);
		count = MSG_ReadByte();
		CL_FindNonSolidLocation(pos, pos, 5);
		CL_Tei_PlasmaHit(pos, dir, count);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 500, 0.6, 1.2, 2.0f, 2000, 9999, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		break;

	default:
		Host_Error("CL_ParseTempEntity: bad type %d (hex %02X)", type, type);
	}
}

#define SHOWNET(x) if(cl_shownet.integer==2)Con_Printf("%3i:%s\n", msg_readcount-1, x);

//[515]: csqc
void CL_VM_Init (void);
qboolean CL_VM_Parse_TempEntity (void);
void CL_VM_Parse_StuffCmd (const char *msg);
void CL_VM_Parse_CenterPrint (const char *msg);
void CSQC_AddPrintText (const char *msg);
void CSQC_ReadEntities (void);
//
static unsigned char cgamenetbuffer[65536];

/*
=====================
CL_ParseServerMessage
=====================
*/
int parsingerror = false;
void CL_ParseServerMessage(void)
{
	int			cmd;
	int			i;
	protocolversion_t protocol;
	unsigned char		cmdlog[32];
	char		*cmdlogname[32], *temp;
	int			cmdindex, cmdcount = 0;

	if (cls.demorecording)
		CL_WriteDemoMessage ();

	cl.last_received_message = realtime;

//
// if recording demos, copy the message out
//
	if (cl_shownet.integer == 1)
		Con_Printf("%f %i\n", realtime, net_message.cursize);
	else if (cl_shownet.integer == 2)
		Con_Print("------------------\n");

	cl.onground = false;	// unless the server says otherwise
//
// parse the message
//
	//MSG_BeginReading ();

	parsingerror = true;

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
			if (cls.signon == SIGNONS - 1)
			{
				// first update is the final signon stage
				cls.signon = SIGNONS;
				CL_SignonReply ();
			}
			EntityFrameQuake_ReadEntity (cmd&127);
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
				strcpy (description, "packet dump: ");
				i = cmdcount - 32;
				if (i < 0)
					i = 0;
				count = cmdcount - i;
				i &= 31;
				while(count > 0)
				{
					dpsnprintf (temp, sizeof (temp), "%3i:%s ", cmdlog[i], cmdlogname[i]);
					strlcat (description, temp, sizeof (description));
					count--;
					i++;
					i &= 31;
				}
				description[strlen(description)-1] = '\n'; // replace the last space with a newline
				Con_Print(description);
				Host_Error ("CL_ParseServerMessage: Illegible server message");
			}
			break;

		case svc_nop:
			if (cls.signon < SIGNONS)
				Con_Print("<-- server to client keepalive\n");
			break;

		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			cl.movement_needupdate = true;
			break;

		case svc_clientdata:
			CL_ParseClientdata();
			break;

		case svc_version:
			i = MSG_ReadLong ();
			protocol = Protocol_EnumForNumber(i);
			if (protocol == PROTOCOL_UNKNOWN)
				Host_Error("CL_ParseServerMessage: Server is unrecognized protocol number (%i)", i);
			// hack for unmarked Nehahra movie demos which had a custom protocol
			if (protocol == PROTOCOL_QUAKEDP && cls.demoplayback && demo_nehahra.integer)
				protocol = PROTOCOL_NEHAHRAMOVIE;
			cls.protocol = protocol;
			break;

		case svc_disconnect:
			Con_Printf ("Server disconnected\n");
			if (cls.demonum != -1)
				CL_NextDemo ();
			else
				CL_Disconnect ();
			break;

		case svc_print:
			CSQC_AddPrintText(MSG_ReadString());	//[515]: csqc
			break;

		case svc_centerprint:
			CL_VM_Parse_CenterPrint(MSG_ReadString ());	//[515]: csqc
			break;

		case svc_stufftext:
			CL_VM_Parse_StuffCmd(MSG_ReadString ());	//[515]: csqc
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverinfo:
			CL_ParseServerInfo ();
			CL_VM_Init();	//[515]: init csqc
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle (cls.protocol);
			break;

		case svc_setview:
			cl.viewentity = (unsigned short)MSG_ReadShort ();
			if (cl.viewentity >= MAX_EDICTS)
				Host_Error("svc_setview >= MAX_EDICTS");
			if (cl.viewentity >= cl_max_entities)
				CL_ExpandEntities(cl.viewentity);
			// LordHavoc: assume first setview recieved is the real player entity
			if (!cl.playerentity)
				cl.playerentity = cl.viewentity;
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= cl_max_lightstyle)
			{
				Con_Printf ("svc_lightstyle >= MAX_LIGHTSTYLES");
				break;
			}
			strlcpy (cl_lightstyle[i].map,  MSG_ReadString(), sizeof (cl_lightstyle[i].map));
			cl_lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
			cl_lightstyle[i].length = (int)strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			CL_ParseStartSoundPacket(false);
			break;

		case svc_precache:
			if (cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3)
			{
				// was svc_sound2 in protocols 1, 2, 3, removed in 4, 5, changed to svc_precache in 6
				CL_ParseStartSoundPacket(true);
			}
			else
			{
				int i = (unsigned short)MSG_ReadShort();
				char *s = MSG_ReadString();
				if (i < 32768)
				{
					if (i >= 1 && i < MAX_MODELS)
					{
						model_t *model = Mod_ForName(s, false, false, i == 1);
						if (!model)
							Con_Printf("svc_precache: Mod_ForName(\"%s\") failed\n", s);
						cl.model_precache[i] = model;
					}
					else
						Con_Printf("svc_precache: index %i outside range %i...%i\n", i, 1, MAX_MODELS);
				}
				else
				{
					i -= 32768;
					if (i >= 1 && i < MAX_SOUNDS)
					{
						sfx_t *sfx = S_PrecacheSound (s, true, false);
						if (!sfx && snd_initialized.integer)
							Con_Printf("svc_precache: S_PrecacheSound(\"%s\") failed\n", s);
						cl.sound_precache[i] = sfx;
					}
					else
						Con_Printf("svc_precache: index %i outside range %i...%i\n", i, 1, MAX_SOUNDS);
				}
			}
			break;

		case svc_stopsound:
			i = (unsigned short) MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;

		case svc_updatename:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename >= cl.maxclients");
			strlcpy (cl.scores[i].name, MSG_ReadString (), sizeof (cl.scores[i].name));
			break;

		case svc_updatefrags:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags >= cl.maxclients");
			cl.scores[i].frags = (signed short) MSG_ReadShort ();
			break;

		case svc_updatecolors:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors >= cl.maxclients");
			cl.scores[i].colors = MSG_ReadByte ();
			break;

		case svc_particle:
			CL_ParseParticleEffect ();
			break;

		case svc_effect:
			CL_ParseEffect ();
			break;

		case svc_effect2:
			CL_ParseEffect2 ();
			break;

		case svc_spawnbaseline:
			i = (unsigned short) MSG_ReadShort ();
			if (i < 0 || i >= MAX_EDICTS)
				Host_Error ("CL_ParseServerMessage: svc_spawnbaseline: invalid entity number %i", i);
			if (i >= cl_max_entities)
				CL_ExpandEntities(i);
			CL_ParseBaseline (cl_entities + i, false);
			break;
		case svc_spawnbaseline2:
			i = (unsigned short) MSG_ReadShort ();
			if (i < 0 || i >= MAX_EDICTS)
				Host_Error ("CL_ParseServerMessage: svc_spawnbaseline2: invalid entity number %i", i);
			if (i >= cl_max_entities)
				CL_ExpandEntities(i);
			CL_ParseBaseline (cl_entities + i, true);
			break;
		case svc_spawnstatic:
			CL_ParseStatic (false);
			break;
		case svc_spawnstatic2:
			CL_ParseStatic (true);
			break;
		case svc_temp_entity:
			if(!CL_VM_Parse_TempEntity())
				CL_ParseTempEntity ();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			S_PauseGameSounds (cl.paused);
			break;

		case svc_signonnum:
			i = MSG_ReadByte ();
			// LordHavoc: it's rude to kick off the client if they missed the
			// reconnect somehow, so allow signon 1 even if at signon 1
			if (i <= cls.signon && i != 1)
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

		case svc_updatestatubyte:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				Host_Error ("svc_updatestat: %i is invalid", i);
			cl.stats[i] = MSG_ReadByte ();
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
				CDAudio_Play ((unsigned char)cls.forcetrack, true);
			else
				CDAudio_Play ((unsigned char)cl.cdtrack, true);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			SCR_CenterPrint(MSG_ReadString ());
			break;

		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			SCR_CenterPrint(MSG_ReadString ());
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;
		case svc_hidelmp:
			if (gamemode == GAME_TENEBRAE)
			{
				// repeating particle effect
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadByte();
				MSG_ReadLong();
				MSG_ReadLong();
				MSG_ReadString();
			}
			else
				SHOWLMP_decodehide();
			break;
		case svc_showlmp:
			if (gamemode == GAME_TENEBRAE)
			{
				// particle effect
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadCoord(cls.protocol);
				MSG_ReadByte();
				MSG_ReadString();
			}
			else
				SHOWLMP_decodeshow();
			break;
		case svc_skybox:
			R_SetSkyBox(MSG_ReadString());
			break;
		case svc_cgame:
			{
				int length;
				length = (int) ((unsigned short) MSG_ReadShort());
				for (i = 0;i < length;i++)
					cgamenetbuffer[i] = MSG_ReadByte();
				if (!msg_badread)
					CL_CGVM_ParseNetwork(cgamenetbuffer, length);
			}
			break;
		case svc_entities:
			if (cls.signon == SIGNONS - 1)
			{
				// first update is the final signon stage
				cls.signon = SIGNONS;
				CL_SignonReply ();
			}
			if (cls.protocol == PROTOCOL_DARKPLACES1 || cls.protocol == PROTOCOL_DARKPLACES2 || cls.protocol == PROTOCOL_DARKPLACES3)
				EntityFrame_CL_ReadFrame();
			else if (cls.protocol == PROTOCOL_DARKPLACES4)
				EntityFrame4_CL_ReadFrame();
			else
				EntityFrame5_CL_ReadFrame();
			break;
		case svc_csqcentities:
			CSQC_ReadEntities();
			break;
		}
	}

	EntityFrameQuake_ISeeDeadEntities();

	parsingerror = false;
}

void CL_Parse_DumpPacket(void)
{
	if (!parsingerror)
		return;
	Con_Print("Packet dump:\n");
	SZ_HexDumpToConsole(&net_message);
	parsingerror = false;
}

void CL_Parse_Init(void)
{
	// LordHavoc: added demo_nehahra cvar
	Cvar_RegisterVariable (&demo_nehahra);
	if (gamemode == GAME_NEHAHRA)
		Cvar_SetValue("demo_nehahra", 1);
	Cvar_RegisterVariable(&developer_networkentities);
}

void CL_Parse_Shutdown(void)
{
}
