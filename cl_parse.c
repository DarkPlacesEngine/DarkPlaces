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
#include "cl_collision.h"

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
	"svc_unusedlh1", //			51		// unused
	"svc_effect", //			52		// [vector] org [byte] modelindex [byte] startframe [byte] framecount [byte] framerate
	"svc_effect2", //			53		// [vector] org [short] modelindex [short] startframe [byte] framecount [byte] framerate
	"svc_sound2", //			54		// short soundindex instead of byte
	"svc_spawnbaseline2", //	55		// short modelindex instead of byte
	"svc_spawnstatic2", //		56		// short modelindex instead of byte
	"svc_entities", //			57		// [int] deltaframe [int] thisframe [float vector] eye [variable length] entitydata
	"svc_unusedlh3", //			58
	"svc_spawnstaticsound2", //	59		// [coord3] [short] samp [byte] vol [byte] aten
};

//=============================================================================

cvar_t demo_nehahra = {0, "demo_nehahra", "0"};
cvar_t developer_networkentities = {0, "developer_networkentities", "0"};

mempool_t *cl_scores_mempool;

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

	if (sound_num >= MAX_SOUNDS)
		Host_Error("CL_ParseStartSoundPacket: sound_num (%i) >= MAX_SOUNDS (%i)\n", sound_num, MAX_SOUNDS);


	if (ent >= MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	MSG_ReadVector(pos);

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/

static qbyte olddata[NET_MAXMESSAGE];
void CL_KeepaliveMessage (void)
{
	float time;
	static float lastmsg;
	int oldreadcount;
	qboolean oldbadread;
	sizebuf_t old;

	// no need if server is local and definitely not if this is a demo
	if (sv.active || cls.demoplayback)
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

	if (cls.netcon && NetConn_CanSendMessage(cls.netcon) && (time = Sys_DoubleTime()) - lastmsg >= 5)
	{
		sizebuf_t	msg;
		qbyte		buf[4];
		lastmsg = time;
		// write out a nop
		// LordHavoc: must use unreliable because reliable could kill the sigon message!
		Con_Print("--> client to server keepalive\n");
		msg.data = buf;
		msg.maxsize = sizeof(buf);
		msg.cursize = 0;
		MSG_WriteChar(&msg, svc_nop);
		NetConn_SendUnreliableMessage(cls.netcon, &msg);
		// try not to utterly crush the computer with work, that's just rude
		Sys_Sleep(1);
	}
}

void CL_ParseEntityLump(char *entdata)
{
	const char *data;
	char key[128], value[4096];
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
	//char 	str[8192];

Con_DPrintf("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

	case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", cl_color.integer >> 4, cl_color.integer & 15));

		if (cl_pmodel.integer)
		{
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, va("pmodel %i\n", cl_pmodel.integer));
		}

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("rate %i\n", cl_rate.integer));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "spawn");
		break;

	case 3:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
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
qbyte entlife[MAX_EDICTS];
// FIXME: this is a lot of memory to be keeping around, this needs to be dynamically allocated and freed
static char parse_model_precache[MAX_MODELS][MAX_QPATH];
static char parse_sound_precache[MAX_SOUNDS][MAX_QPATH];
void CL_ParseServerInfo (void)
{
	char *str;
	int i;
	int nummodels, numsounds;
	entity_t *ent;

	Con_DPrint("Serverinfo packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	// hack for unmarked Nehahra movie demos which had a custom protocol
	if (i == PROTOCOL_QUAKE && cls.demoplayback && demo_nehahra.integer)
		i = PROTOCOL_NEHAHRAMOVIE;
	if (i != PROTOCOL_QUAKE && i != PROTOCOL_DARKPLACES1 && i != PROTOCOL_DARKPLACES2 && i != PROTOCOL_DARKPLACES3 && i != PROTOCOL_DARKPLACES4 && i != PROTOCOL_DARKPLACES5 && i != PROTOCOL_NEHAHRAMOVIE)
	{
		Host_Error("CL_ParseServerInfo: Server is protocol %i, not %i (Quake), %i (DP1), %i (DP2), %i (DP3), %i (DP4), %i (DP5), or %i (Nehahra movie)", i, PROTOCOL_QUAKE, PROTOCOL_DARKPLACES1, PROTOCOL_DARKPLACES2, PROTOCOL_DARKPLACES3, PROTOCOL_DARKPLACES4, PROTOCOL_DARKPLACES5, PROTOCOL_NEHAHRAMOVIE);
		return;
	}
	cl.protocol = i;

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	Mem_EmptyPool(cl_scores_mempool);
	cl.scores = Mem_Alloc(cl_scores_mempool, cl.maxclients*sizeof(*cl.scores));

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strlcpy (cl.levelname, str, sizeof(cl.levelname));

// seperate the printfs so the server message can have a color
	if (cl.protocol != PROTOCOL_NEHAHRAMOVIE) // no messages when playing the Nehahra movie
		Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n\2%s\n", str);

	// check memory integrity
	Mem_CheckSentinelsGlobal();

	// disable until we get textures for it
	R_ResetSkyBox();

	memset(cl.model_precache, 0, sizeof(cl.model_precache));
	memset(cl.sound_precache, 0, sizeof(cl.sound_precache));

	// parse model precache list
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (nummodels==MAX_MODELS)
			Host_Error ("Server sent too many model precaches\n");
		if (strlen(str) >= MAX_QPATH)
			Host_Error ("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strlcpy (parse_model_precache[nummodels], str, sizeof (parse_model_precache[nummodels]));
	}
	// parse sound precache list
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (numsounds==MAX_SOUNDS)
			Host_Error("Server sent too many sound precaches\n");
		if (strlen(str) >= MAX_QPATH)
			Host_Error("Server sent a precache name of %i characters (max %i)", strlen(str), MAX_QPATH - 1);
		strlcpy (parse_sound_precache[numsounds], str, sizeof (parse_sound_precache[numsounds]));
	}

	// touch all of the precached models that are still loaded so we can free
	// anything that isn't needed
	Mod_ClearUsed();
	for (i = 1;i < nummodels;i++)
	{
		CL_KeepaliveMessage();
		Mod_TouchModel(parse_model_precache[i]);
	}
	Mod_PurgeUnused();

	// do the same for sounds
	S_ClearUsed();
	for (i = 1;i < numsounds;i++)
	{
		CL_KeepaliveMessage();
		S_TouchSound(parse_sound_precache[i]);
	}
	S_PurgeUnused();

	// now we try to load everything that is new

	// world model
	CL_KeepaliveMessage ();
	cl.model_precache[1] = Mod_ForName(parse_model_precache[1], false, false, true);
	if (cl.model_precache[1] == NULL)
		Con_Printf("Map %s not found\n", parse_model_precache[1]);

	// normal models
	for (i=2 ; i<nummodels ; i++)
	{
		CL_KeepaliveMessage();
		if ((cl.model_precache[i] = Mod_ForName(parse_model_precache[i], false, false, false)) == NULL)
			Con_Printf("Model %s not found\n", parse_model_precache[i]);
	}

	// sounds
	for (i=1 ; i<numsounds ; i++)
	{
		CL_KeepaliveMessage();
		cl.sound_precache[i] = S_PrecacheSound(parse_sound_precache[i], true);
	}

	// local state
	ent = &cl_entities[0];
	// entire entity array was cleared, so just fill in a few fields
	ent->state_current.active = true;
	ent->render.model = cl.worldmodel = cl.model_precache[1];
	ent->render.scale = 1; // some of the renderer still relies on scale
	ent->render.alpha = 1;
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);
	// clear entlife array
	memset(entlife, 0, MAX_EDICTS);

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

	if (s->modelindex >= MAX_MODELS)
		Host_Error("CL_ValidateState: modelindex (%i) >= MAX_MODELS (%i)\n", s->modelindex, MAX_MODELS);

	// colormap is client index + 1
	if (s->colormap > cl.maxclients)
		Host_Error ("CL_ValidateState: colormap (%i) > cl.maxclients (%i)", s->colormap, cl.maxclients);

	model = cl.model_precache[s->modelindex];
	Mod_CheckLoaded(model);
	if (model && s->frame >= model->numframes)
	{
		Con_DPrintf("CL_ValidateState: no such frame %i in \"%s\"\n", s->frame, model->name);
		s->frame = 0;
	}
	if (model && s->skin > 0 && s->skin >= model->numskins)
	{
		Con_DPrintf("CL_ValidateState: no such skin %i in \"%s\"\n", s->skin, model->name);
		s->skin = 0;
	}
}

void CL_MoveLerpEntityStates(entity_t *ent)
{
	float odelta[3], adelta[3];
	VectorSubtract(ent->state_current.origin, ent->persistent.neworigin, odelta);
	VectorSubtract(ent->state_current.angles, ent->persistent.newangles, adelta);
	if (!ent->state_previous.active || cls.timedemo || DotProduct(odelta, odelta) > 1000*1000 || cl_nolerp.integer)
	{
		// we definitely shouldn't lerp
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
		ent->persistent.lerpstarttime = cl.mtime[1];
		// no lerp if it's singleplayer
		if (cl.islocalgame)
			ent->persistent.lerpdeltatime = 0;
		else
			ent->persistent.lerpdeltatime = cl.mtime[0] - cl.mtime[1];
		VectorCopy(ent->persistent.neworigin, ent->persistent.oldorigin);
		VectorCopy(ent->persistent.newangles, ent->persistent.oldangles);
		VectorCopy(ent->state_current.origin, ent->persistent.neworigin);
		VectorCopy(ent->state_current.angles, ent->persistent.newangles);
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
void CL_ParseUpdate (int bits)
{
	int num;
	entity_t *ent;
	entity_state_t new;

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte()<<8);
	if ((bits & U_EXTEND1) && cl.protocol != PROTOCOL_NEHAHRAMOVIE)
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

	ent = cl_entities + num;

	// note: this inherits the 'active' state of the baseline chosen
	// (state_baseline is always active, state_current may not be active if
	// the entity was missing in the last frame)
	if (bits & U_DELTA)
		new = ent->state_current;
	else
	{
		new = ent->state_baseline;
		new.active = true;
	}

	new.number = num;
	new.time = cl.mtime[0];
	new.flags = 0;
	if (bits & U_MODEL)		new.modelindex = (new.modelindex & 0xFF00) | MSG_ReadByte();
	if (bits & U_FRAME)		new.frame = (new.frame & 0xFF00) | MSG_ReadByte();
	if (bits & U_COLORMAP)	new.colormap = MSG_ReadByte();
	if (bits & U_SKIN)		new.skin = MSG_ReadByte();
	if (bits & U_EFFECTS)	new.effects = (new.effects & 0xFF00) | MSG_ReadByte();
	if (bits & U_ORIGIN1)	new.origin[0] = MSG_ReadCoord();
	if (bits & U_ANGLE1)	new.angles[0] = MSG_ReadAngle();
	if (bits & U_ORIGIN2)	new.origin[1] = MSG_ReadCoord();
	if (bits & U_ANGLE2)	new.angles[1] = MSG_ReadAngle();
	if (bits & U_ORIGIN3)	new.origin[2] = MSG_ReadCoord();
	if (bits & U_ANGLE3)	new.angles[2] = MSG_ReadAngle();
	if (bits & U_STEP)		new.flags |= RENDER_STEP;
	if (bits & U_ALPHA)		new.alpha = MSG_ReadByte();
	if (bits & U_SCALE)		new.scale = MSG_ReadByte();
	if (bits & U_EFFECTS2)	new.effects = (new.effects & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_GLOWSIZE)	new.glowsize = MSG_ReadByte();
	if (bits & U_GLOWCOLOR)	new.glowcolor = MSG_ReadByte();
	// apparently the dpcrush demo uses this (unintended, and it uses white anyway)
	if (bits & U_COLORMOD)	MSG_ReadByte();
	if (bits & U_GLOWTRAIL) new.flags |= RENDER_GLOWTRAIL;
	if (bits & U_FRAME2)	new.frame = (new.frame & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_MODEL2)	new.modelindex = (new.modelindex & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_VIEWMODEL)	new.flags |= RENDER_VIEWMODEL;
	if (bits & U_EXTERIORMODEL)	new.flags |= RENDER_EXTERIORMODEL;

	// LordHavoc: to allow playback of the Nehahra movie
	if (cl.protocol == PROTOCOL_NEHAHRAMOVIE && (bits & U_EXTEND1))
	{
		// LordHavoc: evil format
		int i = MSG_ReadFloat();
		int j = MSG_ReadFloat() * 255.0f;
		if (i == 2)
		{
			i = MSG_ReadFloat();
			if (i)
				new.effects |= EF_FULLBRIGHT;
		}
		if (j < 0)
			new.alpha = 0;
		else if (j == 0 || j >= 255)
			new.alpha = 255;
		else
			new.alpha = j;
	}

	if (new.active)
		CL_ValidateState(&new);

	ent->state_previous = ent->state_current;
	ent->state_current = new;
	if (ent->state_current.active)
	{
		CL_MoveLerpEntityStates(ent);
		cl_entities_active[ent->state_current.number] = true;
		// mark as visible (no kill this frame)
		entlife[ent->state_current.number] = 2;
	}
}

static entity_frame_t entityframe;
extern mempool_t *cl_entities_mempool;
void CL_ReadEntityFrame(void)
{
	if (cl.protocol == PROTOCOL_DARKPLACES1 || cl.protocol == PROTOCOL_DARKPLACES2 || cl.protocol == PROTOCOL_DARKPLACES3)
	{
		int i;
		entity_t *ent;
		EntityFrame_Read(&cl.entitydatabase);
		EntityFrame_FetchFrame(&cl.entitydatabase, EntityFrame_MostRecentlyRecievedFrameNum(&cl.entitydatabase), &entityframe);
		for (i = 0;i < entityframe.numentities;i++)
		{
			// copy the states
			ent = &cl_entities[entityframe.entitydata[i].number];
			ent->state_previous = ent->state_current;
			ent->state_current = entityframe.entitydata[i];
			CL_MoveLerpEntityStates(ent);
			// the entity lives again...
			entlife[ent->state_current.number] = 2;
			cl_entities_active[ent->state_current.number] = true;
		}
	}
	else
	{
		if (!cl.entitydatabase4)
			cl.entitydatabase4 = EntityFrame4_AllocDatabase(cl_entities_mempool);
		EntityFrame4_CL_ReadFrame(cl.entitydatabase4);
	}
}

void CL_EntityUpdateSetup(void)
{
}

void CL_EntityUpdateEnd(void)
{
	if (cl.protocol == PROTOCOL_QUAKE || cl.protocol == PROTOCOL_NEHAHRAMOVIE || cl.protocol == PROTOCOL_DARKPLACES1 || cl.protocol == PROTOCOL_DARKPLACES2 || cl.protocol == PROTOCOL_DARKPLACES3)
	{
		int i;
		// disable entities that disappeared this frame
		for (i = 1;i < MAX_EDICTS;i++)
		{
			// clear only the entities that were active last frame but not this
			// frame, don't waste time clearing all entities (which would cause
			// cache misses)
			if (entlife[i])
			{
				entlife[i]--;
				if (!entlife[i])
					cl_entities[i].state_previous.active = cl_entities[i].state_current.active = 0;
			}
		}
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

	ClearStateToDefault(&ent->state_baseline);
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
	CL_ValidateState(&ent->state_baseline);
	ent->state_previous = ent->state_current = ent->state_baseline;
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
	if (cl.protocol == PROTOCOL_DARKPLACES5)
	{
		for (i = 0;i < 3;i++)
		{
			if (bits & (SU_PUNCH1<<i) )
				cl.punchangle[i] = MSG_ReadPreciseAngle();
			else
				cl.punchangle[i] = 0;
			if (bits & (SU_PUNCHVEC1<<i))
				cl.punchvector[i] = MSG_ReadFloat();
			else
				cl.punchvector[i] = 0;
			if (bits & (SU_VELOCITY1<<i) )
				cl.mvelocity[0][i] = MSG_ReadFloat();
			else
				cl.mvelocity[0][i] = 0;
		}
	}
	else
	{
		for (i = 0;i < 3;i++)
		{
			if (bits & (SU_PUNCH1<<i) )
			{
				if (cl.protocol == PROTOCOL_DARKPLACES1 || cl.protocol == PROTOCOL_DARKPLACES2 || cl.protocol == PROTOCOL_DARKPLACES3 || cl.protocol == PROTOCOL_DARKPLACES4 || cl.protocol == PROTOCOL_DARKPLACES5)
					cl.punchangle[i] = MSG_ReadPreciseAngle();
				else
					cl.punchangle[i] = MSG_ReadChar();
			}
			else
				cl.punchangle[i] = 0;
			if (bits & (SU_PUNCHVEC1<<i))
				cl.punchvector[i] = MSG_ReadCoord();
			else
				cl.punchvector[i] = 0;
			if (bits & (SU_VELOCITY1<<i) )
				cl.mvelocity[0][i] = MSG_ReadChar()*16;
			else
				cl.mvelocity[0][i] = 0;
		}
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

	if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE)
		i = (1<<i);
	// GAME_NEXUIZ hud needs weapon change time
	// GAME_NEXUIZ uses a bit number as it's STAT_ACTIVEWEAPON, not a bitfield
	// like other modes
	if (cl.stats[STAT_ACTIVEWEAPON] != i)
		cl.weapontime = cl.time;
	cl.stats[STAT_ACTIVEWEAPON] = i;

	cl.viewzoomold = cl.viewzoomnew; // for interpolation
	if (bits & SU_VIEWZOOM)
	{
		i = MSG_ReadByte();
		if (i < 2)
			i = 2;
		cl.viewzoomnew = (float) i * (1.0f / 255.0f);
	}
	else
		cl.viewzoomnew = 1;

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

model_t *cl_model_bolt = NULL;
model_t *cl_model_bolt2 = NULL;
model_t *cl_model_bolt3 = NULL;
model_t *cl_model_beam = NULL;

sfx_t *cl_sfx_wizhit;
sfx_t *cl_sfx_knighthit;
sfx_t *cl_sfx_tink1;
sfx_t *cl_sfx_ric1;
sfx_t *cl_sfx_ric2;
sfx_t *cl_sfx_ric3;
sfx_t *cl_sfx_r_exp3;

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav", false);
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav", false);
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav", false);
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav", false);
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav", false);
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav", false);
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav", false);
}

void CL_ParseBeam (model_t *m, int lightning)
{
	int i, ent;
	vec3_t start, end;
	beam_t *b;

	ent = MSG_ReadShort ();
	MSG_ReadVector(start);
	MSG_ReadVector(end);

	if (ent >= MAX_EDICTS)
	{
		Con_Printf("CL_ParseBeam: invalid entity number %i\n", ent);
		ent = 0;
	}

	// override any beam with the same entity
	for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
	{
		if (b->entity == ent)
		{
			//b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl_entities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

	// find a free beam
	for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl_entities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
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
	qbyte *tempcolor;
	matrix4x4_t tempmatrix;

	type = MSG_ReadByte();
	switch (type)
	{
	case TE_WIZSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.12f, 0.50f, 0.12f, 500, 0.2, 0, 0, false, 1);
		CL_RunParticleEffect(pos, vec3_origin, 20, 30);
		S_StartSound(-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.50f, 0.30f, 0.10f, 500, 0.2, 0, 0, false, 1);
		CL_RunParticleEffect(pos, vec3_origin, 226, 20);
		S_StartSound(-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;

	case TE_SPIKE:
		// spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		// LordHavoc: changed to spark shower
		CL_SparkShower(pos, vec3_origin, 15);
		if (rand() % 5)
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SPIKEQUAD:
		// quad spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		// LordHavoc: changed to spark shower
		CL_SparkShower(pos, vec3_origin, 15);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		if (rand() % 5)
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKE:
		// super spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		// LordHavoc: changed to dust shower
		CL_SparkShower(pos, vec3_origin, 30);
		if (rand() % 5)
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
	case TE_SUPERSPIKEQUAD:
		// quad super spike hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		// LordHavoc: changed to dust shower
		CL_SparkShower(pos, vec3_origin, 30);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, 0, true, 1);
		if (rand() % 5)
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		// LordHavoc: added for improved blood splatters
	case TE_BLOOD:
		// blood puff
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		dir[0] = MSG_ReadChar();
		dir[1] = MSG_ReadChar();
		dir[2] = MSG_ReadChar();
		count = MSG_ReadByte();
		CL_BloodPuff(pos, dir, count);
		break;
	case TE_SPARK:
		// spark shower
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		dir[0] = MSG_ReadChar();
		dir[1] = MSG_ReadChar();
		dir[2] = MSG_ReadChar();
		count = MSG_ReadByte();
		CL_SparkShower(pos, dir, count);
		break;
	case TE_PLASMABURN:
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 200, 1, 1, 1, 1000, 0.2, 0, 0, true, 1);
		CL_PlasmaBurn(pos);
		break;
		// LordHavoc: added for improved gore
	case TE_BLOODSHOWER:
		// vaporized body
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		velspeed = MSG_ReadCoord(); // speed
		count = MSG_ReadShort(); // number of particles
		CL_BloodShower(pos, pos2, velspeed, count);
		break;
	case TE_PARTICLECUBE:
		// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		colorLength = MSG_ReadByte(); // gravity (1 or 0)
		velspeed = MSG_ReadCoord(); // randomvel
		CL_ParticleCube(pos, pos2, dir, count, colorStart, colorLength, velspeed);
		break;

	case TE_PARTICLERAIN:
		// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		CL_ParticleRain(pos, pos2, dir, count, colorStart, 0);
		break;

	case TE_PARTICLESNOW:
		// general purpose particle effect
		MSG_ReadVector(pos); // mins
		MSG_ReadVector(pos2); // maxs
		MSG_ReadVector(dir); // dir
		count = MSG_ReadShort(); // number of particles
		colorStart = MSG_ReadByte(); // color
		CL_ParticleRain(pos, pos2, dir, count, colorStart, 1);
		break;

	case TE_GUNSHOT:
		// bullet hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		// LordHavoc: changed to dust shower
		CL_SparkShower(pos, vec3_origin, 15);
		break;

	case TE_GUNSHOTQUAD:
		// quad bullet hitting wall
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		CL_SparkShower(pos, vec3_origin, 15);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, 0, true, 1);
		break;

	case TE_EXPLOSION:
		// rocket explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		// LordHavoc: boosted color from 1.0, 0.8, 0.4 to 1.25, 1.0, 0.5
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, 2.5f, 2.0f, 1.0f, 700, 0.5, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONQUAD:
		// quad rocket explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, 2.5f, 2.0f, 4.0f, 700, 0.5, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSION3:
		// Nehahra movie colored lighting explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		color[0] = MSG_ReadCoord() * (2.0f / 1.0f);
		color[1] = MSG_ReadCoord() * (2.0f / 1.0f);
		color[2] = MSG_ReadCoord() * (2.0f / 1.0f);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, 0, true, 1);	
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_EXPLOSIONRGB:
		// colored lighting explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TAREXPLOSION:
		// tarbaby explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_BlobExplosion(pos);

		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 600, 1.6f, 0.8f, 2.0f, 1200, 0.5, 0, 0, true, 1);
		break;

	case TE_SMALLFLASH:
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 200, 2, 2, 2, 1000, 0.2, 0, 0, true, 1);
		break;

	case TE_CUSTOMFLASH:
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 4);
		radius = MSG_ReadByte() * 8;
		velspeed = (MSG_ReadByte() + 1) * (1.0 / 256.0);
		color[0] = MSG_ReadByte() * (2.0f / 255.0f);
		color[1] = MSG_ReadByte() * (2.0f / 255.0f);
		color[2] = MSG_ReadByte() * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, radius, color[0], color[1], color[2], radius / velspeed, velspeed, 0, 0, true, 1);
		break;

	case TE_FLAMEJET:
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte();
		CL_Flames(pos, dir, count);
		break;

	case TE_LIGHTNING1:
		// lightning bolts
		if (!cl_model_bolt)
			cl_model_bolt = Mod_ForName("progs/bolt.mdl", false, false, false);
		CL_ParseBeam(cl_model_bolt, true);
		break;

	case TE_LIGHTNING2:
		// lightning bolts
		if (!cl_model_bolt2)
			cl_model_bolt2 = Mod_ForName("progs/bolt2.mdl", false, false, false);
		CL_ParseBeam(cl_model_bolt2, true);
		break;

	case TE_LIGHTNING3:
		// lightning bolts
		if (!cl_model_bolt3)
			cl_model_bolt3 = Mod_ForName("progs/bolt3.mdl", true, false, false);
		CL_ParseBeam(cl_model_bolt3, false);
		break;

// PGM 01/21/97
	case TE_BEAM:
		// grappling hook beam
		if (!cl_model_beam)
			cl_model_beam = Mod_ForName("progs/beam.mdl", true, false, false);
		CL_ParseBeam(cl_model_beam, false);
		break;
// PGM 01/21/97

// LordHavoc: for compatibility with the Nehahra movie...
	case TE_LIGHTNING4NEH:
		CL_ParseBeam(Mod_ForName(MSG_ReadString(), true, false, false), false);
		break;

	case TE_LAVASPLASH:
		MSG_ReadVector(pos);
		CL_LavaSplash(pos);
		break;

	case TE_TELEPORT:
		MSG_ReadVector(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 500, 1.0f, 1.0f, 1.0f, 1500, 99.0f, 0, 0, true, 1);
//		CL_TeleportSplash(pos);
		break;

	case TE_EXPLOSION2:
		// color mapped explosion
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		CL_ParticleExplosion2(pos, colorStart, colorLength);
		tempcolor = (qbyte *)&palette_complete[(rand()%colorLength) + colorStart];
		color[0] = tempcolor[0] * (2.0f / 255.0f);
		color[1] = tempcolor[1] * (2.0f / 255.0f);
		color[2] = tempcolor[2] * (2.0f / 255.0f);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TEI_G3:
		MSG_ReadVector(pos);
		MSG_ReadVector(pos2);
		MSG_ReadVector(dir);
		CL_BeamParticle(pos, pos2, 12, 1, 0.3, 0.1, 1, 1);
		CL_BeamParticle(pos, pos2, 5, 1, 0.9, 0.3, 1, 1);
		break;

	case TE_TEI_SMOKE:
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte();
		CL_FindNonSolidLocation(pos, pos, 4);
		CL_Tei_Smoke(pos, dir, count);
		break;

	case TE_TEI_BIGEXPLOSION:
		MSG_ReadVector(pos);
		CL_FindNonSolidLocation(pos, pos, 10);
		CL_ParticleExplosion(pos);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 500, 2.5f, 2.0f, 1.0f, 500, 9999, 0, 0, true, 1);
		S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;

	case TE_TEI_PLASMAHIT:
		MSG_ReadVector(pos);
		MSG_ReadVector(dir);
		count = MSG_ReadByte();
		CL_FindNonSolidLocation(pos, pos, 5);
		CL_Tei_PlasmaHit(pos, dir, count);
		Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
		CL_AllocDlight(NULL, &tempmatrix, 500, 0.6, 1.2, 2.0f, 2000, 9999, 0, 0, true, 1);
		break;

	default:
		Host_Error("CL_ParseTempEntity: bad type %d (hex %02X)", type, type);
	}
}

#define SHOWNET(x) if(cl_shownet.integer==2)Con_Printf("%3i:%s\n", msg_readcount-1, x);

static qbyte cgamenetbuffer[65536];

/*
=====================
CL_ParseServerMessage
=====================
*/
int parsingerror = false;
void CL_ParseServerMessage(void)
{
	int			cmd;
	int			i, entitiesupdated;
	qbyte		cmdlog[32];
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

	entitiesupdated = false;

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
				strcpy (description, "packet dump: ");
				i = cmdcount - 32;
				if (i < 0)
					i = 0;
				count = cmdcount - i;
				i &= 31;
				while(count > 0)
				{
					snprintf (temp, sizeof (temp), "%3i:%s ", cmdlog[i], cmdlogname[i]);
					strlcat (description, temp, sizeof (description));
					count--;
					i++;
					i &= 31;
				}
				description[strlen(description)-1] = '\n'; // replace the last space with a newline
				Con_Print(description);
				Host_Error ("CL_ParseServerMessage: Illegible server message\n");
			}
			break;

		case svc_nop:
			if (cls.signon < SIGNONS)
				Con_Print("<-- server to client keepalive\n");
			break;

		case svc_time:
			if (!entitiesupdated)
			{
				// this is a new frame, we'll be seeing entities,
				// so prepare for entity updates
				CL_EntityUpdateSetup();
				entitiesupdated = true;
			}
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();
			break;

		case svc_clientdata:
			i = MSG_ReadShort ();
			CL_ParseClientdata (i);
			break;

		case svc_version:
			i = MSG_ReadLong ();
			// hack for unmarked Nehahra movie demos which had a custom protocol
			if (i == PROTOCOL_QUAKE && cls.demoplayback && demo_nehahra.integer)
				i = PROTOCOL_NEHAHRAMOVIE;
			if (i != PROTOCOL_QUAKE && i != PROTOCOL_DARKPLACES1 && i != PROTOCOL_DARKPLACES2 && i != PROTOCOL_DARKPLACES3 && i != PROTOCOL_DARKPLACES4 && i != PROTOCOL_DARKPLACES5 && i != PROTOCOL_NEHAHRAMOVIE)
				Host_Error("CL_ParseServerMessage: Server is protocol %i, not %i (Quake), %i (DP1), %i (DP2), %i (DP3), %i (DP4), %i (DP5), or %i (Nehahra movie)", i, PROTOCOL_QUAKE, PROTOCOL_DARKPLACES1, PROTOCOL_DARKPLACES2, PROTOCOL_DARKPLACES3, PROTOCOL_DARKPLACES4, PROTOCOL_DARKPLACES5, PROTOCOL_NEHAHRAMOVIE);
			cl.protocol = i;
			break;

		case svc_disconnect:
			Con_Printf ("Server disconnected\n");
			if (cls.demonum != -1)
				CL_NextDemo ();
			else
				CL_Disconnect ();
			break;

		case svc_print:
			Con_Print(MSG_ReadString());
			break;

		case svc_centerprint:
			SCR_CenterPrint(MSG_ReadString ());
			break;

		case svc_stufftext:
			Cbuf_AddText (MSG_ReadString ());
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverinfo:
			CL_ParseServerInfo ();
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle ();
			break;

		case svc_setview:
			cl.viewentity = (unsigned short)MSG_ReadShort ();
			if (cl.viewentity >= MAX_EDICTS)
				Host_Error("svc_setview >= MAX_EDICTS\n");
			// LordHavoc: assume first setview recieved is the real player entity
			if (!cl.playerentity)
				cl.playerentity = cl.viewentity;
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
				Host_Error ("svc_lightstyle >= MAX_LIGHTSTYLES");
			strlcpy (cl_lightstyle[i].map,  MSG_ReadString(), sizeof (cl_lightstyle[i].map));
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
				Host_Error ("CL_ParseServerMessage: svc_updatename >= cl.maxclients");
			strlcpy (cl.scores[i].name, MSG_ReadString (), sizeof (cl.scores[i].name));
			break;

		case svc_updatefrags:
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags >= cl.maxclients");
			cl.scores[i].frags = MSG_ReadShort ();
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
			i = MSG_ReadShort ();
			if (i < 0 || i >= MAX_EDICTS)
				Host_Error ("CL_ParseServerMessage: svc_spawnbaseline: invalid entity number %i", i);
			CL_ParseBaseline (cl_entities + i, false);
			break;
		case svc_spawnbaseline2:
			i = MSG_ReadShort ();
			if (i < 0 || i >= MAX_EDICTS)
				Host_Error ("CL_ParseServerMessage: svc_spawnbaseline2: invalid entity number %i", i);
			CL_ParseBaseline (cl_entities + i, true);
			break;
		case svc_spawnstatic:
			CL_ParseStatic (false);
			break;
		case svc_spawnstatic2:
			CL_ParseStatic (true);
			break;
		case svc_temp_entity:
			CL_ParseTempEntity ();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
			{
				CDAudio_Pause ();
				S_PauseGameSounds ();
			}
			else
			{
				CDAudio_Resume ();
				S_ResumeGameSounds ();
			}
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
				CDAudio_Play ((qbyte)cls.forcetrack, true);
			else
				CDAudio_Play ((qbyte)cl.cdtrack, true);
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
				MSG_ReadCoord();
				MSG_ReadCoord();
				MSG_ReadCoord();
				MSG_ReadCoord();
				MSG_ReadCoord();
				MSG_ReadCoord();
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
				MSG_ReadCoord();
				MSG_ReadCoord();
				MSG_ReadCoord();
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
			CL_ReadEntityFrame();
			break;
		}
	}

	if (entitiesupdated)
		CL_EntityUpdateEnd();

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
	cl_scores_mempool = Mem_AllocPool("client player info");
	Cvar_RegisterVariable (&demo_nehahra);
	if (gamemode == GAME_NEHAHRA)
		Cvar_SetValue("demo_nehahra", 1);
	Cvar_RegisterVariable(&developer_networkentities);
}
