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
// sv_main.c -- server main program

#include "quakedef.h"
#include "libcurl.h"

void SV_VM_Init();
void SV_VM_Setup();

void VM_CustomStats_Clear (void);
void EntityFrameCSQC_ClearVersions (void);
void EntityFrameCSQC_InitClientVersions (int client, qboolean clear);
void VM_SV_UpdateCustomStats (client_t *client, prvm_edict_t *ent, sizebuf_t *msg, int *stats);
void EntityFrameCSQC_WriteFrame (sizebuf_t *msg, int numstates, const entity_state_t *states);


// select which protocol to host, this is fed to Protocol_EnumForName
cvar_t sv_protocolname = {0, "sv_protocolname", "DP7", "selects network protocol to host for (values include QUAKE, QUAKEDP, NEHAHRAMOVIE, DP1 and up)"};
cvar_t sv_ratelimitlocalplayer = {0, "sv_ratelimitlocalplayer", "0", "whether to apply rate limiting to the local player in a listen server (only useful for testing)"};
cvar_t sv_maxrate = {CVAR_SAVE | CVAR_NOTIFY, "sv_maxrate", "10000", "upper limit on client rate cvar, should reflect your network connection quality"};
cvar_t sv_allowdownloads = {0, "sv_allowdownloads", "1", "whether to allow clients to download files from the server (does not affect http downloads)"};
cvar_t sv_allowdownloads_inarchive = {0, "sv_allowdownloads_inarchive", "0", "whether to allow downloads from archives (pak/pk3)"};
cvar_t sv_allowdownloads_archive = {0, "sv_allowdownloads_archive", "0", "whether to allow downloads of archives (pak/pk3)"};
cvar_t sv_allowdownloads_config = {0, "sv_allowdownloads_config", "0", "whether to allow downloads of config files (cfg)"};
cvar_t sv_allowdownloads_dlcache = {0, "sv_allowdownloads_dlcache", "0", "whether to allow downloads of dlcache files (dlcache/)"};

extern cvar_t sv_random_seed;

static cvar_t sv_cullentities_pvs = {0, "sv_cullentities_pvs", "1", "fast but loose culling of hidden entities"}; // fast but loose
static cvar_t sv_cullentities_trace = {0, "sv_cullentities_trace", "0", "somewhat slow but very tight culling of hidden entities, minimizes network traffic and makes wallhack cheats useless"}; // tends to get false negatives, uses a timeout to keep entities visible a short time after becoming hidden
static cvar_t sv_cullentities_trace_samples = {0, "sv_cullentities_trace_samples", "1", "number of samples to test for entity culling"};
static cvar_t sv_cullentities_trace_samples_extra = {0, "sv_cullentities_trace_samples_extra", "2", "number of samples to test for entity culling when the entity affects its surroundings by e.g. dlight"};
static cvar_t sv_cullentities_trace_enlarge = {0, "sv_cullentities_trace_enlarge", "0", "box enlargement for entity culling"};
static cvar_t sv_cullentities_trace_delay = {0, "sv_cullentities_trace_delay", "1", "number of seconds until the entity gets actually culled"};
static cvar_t sv_cullentities_trace_prediction = {0, "sv_cullentities_trace_prediction", "1", "also trace from the predicted player position"};
static cvar_t sv_cullentities_nevercullbmodels = {0, "sv_cullentities_nevercullbmodels", "0", "if enabled the clients are always notified of moving doors and lifts and other submodels of world (warning: eats a lot of network bandwidth on some levels!)"};
static cvar_t sv_cullentities_stats = {0, "sv_cullentities_stats", "0", "displays stats on network entities culled by various methods for each client"};
static cvar_t sv_entpatch = {0, "sv_entpatch", "1", "enables loading of .ent files to override entities in the bsp (for example Threewave CTF server pack contains .ent patch files enabling play of CTF on id1 maps)"};

cvar_t sv_gameplayfix_grenadebouncedownslopes = {0, "sv_gameplayfix_grenadebouncedownslopes", "1", "prevents MOVETYPE_BOUNCE (grenades) from getting stuck when fired down a downward sloping surface"};
cvar_t sv_gameplayfix_noairborncorpse = {0, "sv_gameplayfix_noairborncorpse", "1", "causes entities (corpses) sitting ontop of moving entities (players) to fall when the moving entity (player) is no longer supporting them"};
cvar_t sv_gameplayfix_stepdown = {0, "sv_gameplayfix_stepdown", "0", "attempts to step down stairs, not just up them (prevents the familiar thud..thud..thud.. when running down stairs and slopes)"};
cvar_t sv_gameplayfix_stepwhilejumping = {0, "sv_gameplayfix_stepwhilejumping", "1", "applies step-up onto a ledge even while airborn, useful if you would otherwise just-miss the floor when running across small areas with gaps (for instance running across the moving platforms in dm2, or jumping to the megahealth and red armor in dm2 rather than using the bridge)"};
cvar_t sv_gameplayfix_swiminbmodels = {0, "sv_gameplayfix_swiminbmodels", "1", "causes pointcontents (used to determine if you are in a liquid) to check bmodel entities as well as the world model, so you can swim around in (possibly moving) water bmodel entities"};
cvar_t sv_gameplayfix_setmodelrealbox = {0, "sv_gameplayfix_setmodelrealbox", "1", "fixes a bug in Quake that made setmodel always set the entity box to ('-16 -16 -16', '16 16 16') rather than properly checking the model box, breaks some poorly coded mods"};
cvar_t sv_gameplayfix_blowupfallenzombies = {0, "sv_gameplayfix_blowupfallenzombies", "1", "causes findradius to detect SOLID_NOT entities such as zombies and corpses on the floor, allowing splash damage to apply to them"};
cvar_t sv_gameplayfix_findradiusdistancetobox = {0, "sv_gameplayfix_findradiusdistancetobox", "1", "causes findradius to check the distance to the corner of a box rather than the center of the box, makes findradius detect bmodels such as very large doors that would otherwise be unaffected by splash damage"};
cvar_t sv_gameplayfix_qwplayerphysics = {0, "sv_gameplayfix_qwplayerphysics", "1", "changes water jumping to make it easier to get out of water, and prevents friction on landing when bunnyhopping"};
cvar_t sv_gameplayfix_upwardvelocityclearsongroundflag = {0, "sv_gameplayfix_upwardvelocityclearsongroundflag", "1", "prevents monsters, items, and most other objects from being stuck to the floor when pushed around by damage, and other situations in mods"};
cvar_t sv_gameplayfix_droptofloorstartsolid = {0, "sv_gameplayfix_droptofloorstartsolid", "1", "prevents items and monsters that start in a solid area from falling out of the level (makes droptofloor treat trace_startsolid as an acceptable outcome)"};

cvar_t sv_progs = {0, "sv_progs", "progs.dat", "selects which quakec progs.dat file to run" };

// TODO: move these cvars here
extern cvar_t sv_clmovement_enable;
extern cvar_t sv_clmovement_minping;
extern cvar_t sv_clmovement_minping_disabletime;
extern cvar_t sv_clmovement_waitforinput;

server_t sv;
server_static_t svs;

mempool_t *sv_mempool = NULL;

//============================================================================

extern void SV_Phys_Init (void);
static void SV_SaveEntFile_f(void);
static void SV_StartDownload_f(void);
static void SV_Download_f(void);

void SV_AreaStats_f(void)
{
	World_PrintAreaStats(&sv.world, "server");
}

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	// init the csqc progs cvars, since they are updated/used by the server code
	// TODO: fix this since this is a quick hack to make some of [515]'s broken code run ;) [9/13/2006 Black]
	extern cvar_t csqc_progname;	//[515]: csqc crc check and right csprogs name according to progs.dat
	extern cvar_t csqc_progcrc;
	extern cvar_t csqc_progsize;
	Cvar_RegisterVariable (&csqc_progname);
	Cvar_RegisterVariable (&csqc_progcrc);
	Cvar_RegisterVariable (&csqc_progsize);

	Cmd_AddCommand("sv_saveentfile", SV_SaveEntFile_f, "save map entities to .ent file (to allow external editing)");
	Cmd_AddCommand("sv_areastats", SV_AreaStats_f, "prints statistics on entity culling during collision traces");
	Cmd_AddCommand_WithClientCommand("sv_startdownload", NULL, SV_StartDownload_f, "begins sending a file to the client (network protocol use only)");
	Cmd_AddCommand_WithClientCommand("download", NULL, SV_Download_f, "downloads a specified file from the server");
	Cvar_RegisterVariable (&sv_maxvelocity);
	Cvar_RegisterVariable (&sv_gravity);
	Cvar_RegisterVariable (&sv_friction);
	Cvar_RegisterVariable (&sv_waterfriction);
	Cvar_RegisterVariable (&sv_edgefriction);
	Cvar_RegisterVariable (&sv_stopspeed);
	Cvar_RegisterVariable (&sv_maxspeed);
	Cvar_RegisterVariable (&sv_maxairspeed);
	Cvar_RegisterVariable (&sv_accelerate);
	Cvar_RegisterVariable (&sv_airaccelerate);
	Cvar_RegisterVariable (&sv_wateraccelerate);
	Cvar_RegisterVariable (&sv_clmovement_enable);
	Cvar_RegisterVariable (&sv_clmovement_minping);
	Cvar_RegisterVariable (&sv_clmovement_minping_disabletime);
	Cvar_RegisterVariable (&sv_clmovement_waitforinput);
	Cvar_RegisterVariable (&sv_idealpitchscale);
	Cvar_RegisterVariable (&sv_aim);
	Cvar_RegisterVariable (&sv_nostep);
	Cvar_RegisterVariable (&sv_cullentities_pvs);
	Cvar_RegisterVariable (&sv_cullentities_trace);
	Cvar_RegisterVariable (&sv_cullentities_trace_samples);
	Cvar_RegisterVariable (&sv_cullentities_trace_samples_extra);
	Cvar_RegisterVariable (&sv_cullentities_trace_enlarge);
	Cvar_RegisterVariable (&sv_cullentities_trace_delay);
	Cvar_RegisterVariable (&sv_cullentities_trace_prediction);
	Cvar_RegisterVariable (&sv_cullentities_nevercullbmodels);
	Cvar_RegisterVariable (&sv_cullentities_stats);
	Cvar_RegisterVariable (&sv_entpatch);
	Cvar_RegisterVariable (&sv_gameplayfix_grenadebouncedownslopes);
	Cvar_RegisterVariable (&sv_gameplayfix_noairborncorpse);
	Cvar_RegisterVariable (&sv_gameplayfix_stepdown);
	Cvar_RegisterVariable (&sv_gameplayfix_stepwhilejumping);
	Cvar_RegisterVariable (&sv_gameplayfix_swiminbmodels);
	Cvar_RegisterVariable (&sv_gameplayfix_setmodelrealbox);
	Cvar_RegisterVariable (&sv_gameplayfix_blowupfallenzombies);
	Cvar_RegisterVariable (&sv_gameplayfix_findradiusdistancetobox);
	Cvar_RegisterVariable (&sv_gameplayfix_qwplayerphysics);
	Cvar_RegisterVariable (&sv_gameplayfix_upwardvelocityclearsongroundflag);
	Cvar_RegisterVariable (&sv_gameplayfix_droptofloorstartsolid);
	Cvar_RegisterVariable (&sv_protocolname);
	Cvar_RegisterVariable (&sv_ratelimitlocalplayer);
	Cvar_RegisterVariable (&sv_maxrate);
	Cvar_RegisterVariable (&sv_allowdownloads);
	Cvar_RegisterVariable (&sv_allowdownloads_inarchive);
	Cvar_RegisterVariable (&sv_allowdownloads_archive);
	Cvar_RegisterVariable (&sv_allowdownloads_config);
	Cvar_RegisterVariable (&sv_allowdownloads_dlcache);
	Cvar_RegisterVariable (&sv_progs);

	SV_VM_Init();
	SV_Phys_Init();

	sv_mempool = Mem_AllocPool("server", 0, NULL);
}

static void SV_SaveEntFile_f(void)
{
	char basename[MAX_QPATH];
	if (!sv.active || !sv.worldmodel)
	{
		Con_Print("Not running a server\n");
		return;
	}
	FS_StripExtension(sv.worldmodel->name, basename, sizeof(basename));
	FS_WriteFile(va("%s.ent", basename), sv.worldmodel->brush.entities, (fs_offset_t)strlen(sv.worldmodel->brush.entities));
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int i;

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-18)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
	for (i=0 ; i<3 ; i++)
		MSG_WriteChar (&sv.datagram, (int)bound(-128, dir[i]*16, 127));
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
	SV_FlushBroadcastMessages();
}

/*
==================
SV_StartEffect

Make sure the event gets sent to all clients
==================
*/
void SV_StartEffect (vec3_t org, int modelindex, int startframe, int framecount, int framerate)
{
	if (modelindex >= 256 || startframe >= 256)
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-19)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect2);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteShort (&sv.datagram, modelindex);
		MSG_WriteShort (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	else
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-17)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteByte (&sv.datagram, modelindex);
		MSG_WriteByte (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	SV_FlushBroadcastMessages();
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (prvm_edict_t *entity, int channel, const char *sample, int volume, float attenuation)
{
	int sound_num, field_mask, i, ent;

	if (volume < 0 || volume > 255)
	{
		Con_Printf ("SV_StartSound: volume = %i\n", volume);
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		Con_Printf ("SV_StartSound: attenuation = %f\n", attenuation);
		return;
	}

	if (channel < 0 || channel > 7)
	{
		Con_Printf ("SV_StartSound: channel = %i\n", channel);
		return;
	}

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-21)
		return;

// find precache number for sound
	sound_num = SV_SoundIndex(sample, 1);
	if (!sound_num)
		return;

	ent = PRVM_NUM_FOR_EDICT(entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;
	if (ent >= 8192)
		field_mask |= SND_LARGEENTITY;
	if (sound_num >= 256 || channel >= 8)
		field_mask |= SND_LARGESOUND;

// directed messages go only to the entity they are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, (int)(attenuation*64));
	if (field_mask & SND_LARGEENTITY)
	{
		MSG_WriteShort (&sv.datagram, ent);
		MSG_WriteByte (&sv.datagram, channel);
	}
	else
		MSG_WriteShort (&sv.datagram, (ent<<3) | channel);
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort (&sv.datagram, sound_num);
	else
		MSG_WriteByte (&sv.datagram, sound_num);
	for (i = 0;i < 3;i++)
		MSG_WriteCoord (&sv.datagram, entity->fields.server->origin[i]+0.5*(entity->fields.server->mins[i]+entity->fields.server->maxs[i]), sv.protocol);
	SV_FlushBroadcastMessages();
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	int i;
	char message[128];

	// we know that this client has a netconnection and thus is not a bot

	// edicts get reallocated on level changes, so we need to update it here
	client->edict = PRVM_EDICT_NUM((client - svs.clients) + 1);

	// clear cached stuff that depends on the level
	client->weaponmodel[0] = 0;
	client->weaponmodelindex = 0;

	// LordHavoc: clear entityframe tracking
	client->latestframenum = 0;

	if (client->entitydatabase)
		EntityFrame_FreeDatabase(client->entitydatabase);
	if (client->entitydatabase4)
		EntityFrame4_FreeDatabase(client->entitydatabase4);
	if (client->entitydatabase5)
		EntityFrame5_FreeDatabase(client->entitydatabase5);

	memset(client->stats, 0, sizeof(client->stats));
	memset(client->statsdeltabits, 0, sizeof(client->statsdeltabits));

	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE)
	{
		if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3)
			client->entitydatabase = EntityFrame_AllocDatabase(sv_mempool);
		else if (sv.protocol == PROTOCOL_DARKPLACES4)
			client->entitydatabase4 = EntityFrame4_AllocDatabase(sv_mempool);
		else
			client->entitydatabase5 = EntityFrame5_AllocDatabase(sv_mempool);
	}

	SZ_Clear (&client->netconnection->message);
	MSG_WriteByte (&client->netconnection->message, svc_print);
	dpsnprintf (message, sizeof (message), "\nServer: %s build %s (progs %i crc)", gamename, buildstring, prog->filecrc);
	MSG_WriteString (&client->netconnection->message,message);

	//[515]: init csprogs according to version of svprogs, check the crc, etc.
	if (sv.csqc_progname[0])
	{
		prvm_eval_t *val;
		Con_DPrintf("sending csqc info to client (\"%s\" with size %i and crc %i)\n", sv.csqc_progname, sv.csqc_progsize, sv.csqc_progcrc);
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progname %s\n", sv.csqc_progname));
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progsize %i\n", sv.csqc_progsize));
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progcrc %i\n", sv.csqc_progcrc));
		//[515]: init stufftext string (it is sent before svc_serverinfo)
		val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.SV_InitCmd);
		if (val)
		{
			MSG_WriteByte (&client->netconnection->message, svc_stufftext);
			MSG_WriteString (&client->netconnection->message, va("%s\n", PRVM_GetString(val->string)));
		}
	}

	if (sv_allowdownloads.integer)
	{
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, "cl_serverextension_download 1");
	}

	// send at this time so it's guaranteed to get executed at the right time
	{
		client_t *save;
		save = host_client;
		host_client = client;
		Curl_SendRequirements();
		host_client = save;
	}

	MSG_WriteByte (&client->netconnection->message, svc_serverinfo);
	MSG_WriteLong (&client->netconnection->message, Protocol_NumberForEnum(sv.protocol));
	MSG_WriteByte (&client->netconnection->message, svs.maxclients);

	if (!coop.integer && deathmatch.integer)
		MSG_WriteByte (&client->netconnection->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->netconnection->message, GAME_COOP);

	MSG_WriteString (&client->netconnection->message,PRVM_GetString(prog->edicts->fields.server->message));

	for (i = 1;i < MAX_MODELS && sv.model_precache[i][0];i++)
		MSG_WriteString (&client->netconnection->message, sv.model_precache[i]);
	MSG_WriteByte (&client->netconnection->message, 0);

	for (i = 1;i < MAX_SOUNDS && sv.sound_precache[i][0];i++)
		MSG_WriteString (&client->netconnection->message, sv.sound_precache[i]);
	MSG_WriteByte (&client->netconnection->message, 0);

// send music
	MSG_WriteByte (&client->netconnection->message, svc_cdtrack);
	MSG_WriteByte (&client->netconnection->message, (int)prog->edicts->fields.server->sounds);
	MSG_WriteByte (&client->netconnection->message, (int)prog->edicts->fields.server->sounds);

// set view
	MSG_WriteByte (&client->netconnection->message, svc_setview);
	MSG_WriteShort (&client->netconnection->message, PRVM_NUM_FOR_EDICT(client->edict));

	MSG_WriteByte (&client->netconnection->message, svc_signonnum);
	MSG_WriteByte (&client->netconnection->message, 1);

	client->spawned = false;		// need prespawn, spawn, etc

	// clear movement info until client enters the new level properly
	memset(&client->cmd, 0, sizeof(client->cmd));
	client->movesequence = 0;
#ifdef NUM_PING_TIMES
	for (i = 0;i < NUM_PING_TIMES;i++)
		client->ping_times[i] = 0;
	client->num_pings = 0;
#endif
	client->ping = 0;
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum, netconn_t *netconnection)
{
	client_t		*client;
	int				i;
	float			spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

	if(netconnection)//[515]: bots don't play with csqc =)
		EntityFrameCSQC_InitClientVersions(clientnum, false);

// set up the client_t
	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->active = true;
	client->netconnection = netconnection;

	Con_DPrintf("Client %s connected\n", client->netconnection ? client->netconnection->address : "botclient");

	strlcpy(client->name, "unconnected", sizeof(client->name));
	strlcpy(client->old_name, "unconnected", sizeof(client->old_name));
	client->spawned = false;
	client->edict = PRVM_EDICT_NUM(clientnum+1);
	if (client->netconnection)
		client->netconnection->message.allowoverflow = true;		// we can catch it
	// prepare the unreliable message buffer
	client->unreliablemsg.data = client->unreliablemsg_data;
	client->unreliablemsg.maxsize = sizeof(client->unreliablemsg_data);
	// updated by receiving "rate" command from client
	client->rate = NET_MINRATE;
	// no limits for local player
	if (client->netconnection && LHNETADDRESS_GetAddressType(&client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP)
		client->rate = 1000000000;
	client->connecttime = realtime;

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	else
	{
		// call the progs to get default spawn parms for the new client
		// set self to world to intentionally cause errors with broken SetNewParms code in some mods
		prog->globals.server->self = 0;
		PRVM_ExecuteProgram (prog->globals.server->SetNewParms, "QC function SetNewParms is missing");
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&prog->globals.server->parm1)[i];

		// set up the entity for this client (including .colormap, .team, etc)
		PRVM_ED_ClearEdict(client->edict);
	}

	// don't call SendServerinfo for a fresh botclient because its fields have
	// not been set up by the qc yet
	if (client->netconnection)
		SV_SendServerinfo (client);
	else
		client->spawned = true;
}


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int sv_writeentitiestoclient_pvsbytes;
unsigned char sv_writeentitiestoclient_pvs[MAX_MAP_LEAFS/8];

static int numsendentities;
static entity_state_t sendentities[MAX_EDICTS];
static entity_state_t *sendentitiesindex[MAX_EDICTS];

static int sententitiesmark = 0;
static int sententities[MAX_EDICTS];
static int sententitiesconsideration[MAX_EDICTS];
static int sv_writeentitiestoclient_culled_pvs;
static int sv_writeentitiestoclient_culled_trace;
static int sv_writeentitiestoclient_visibleentities;
static int sv_writeentitiestoclient_totalentities;
//static entity_frame_t sv_writeentitiestoclient_entityframe;
static int sv_writeentitiestoclient_clentnum;
static vec3_t sv_writeentitiestoclient_testeye;
static client_t *sv_writeentitiestoclient_client;

qboolean SV_PrepareEntityForSending (prvm_edict_t *ent, entity_state_t *cs, int e)
{
	int i;
	unsigned int modelindex, effects, flags, glowsize, lightstyle, lightpflags, light[4], specialvisibilityradius;
	unsigned int customizeentityforclient;
	float f;
	vec3_t cullmins, cullmaxs;
	model_t *model;
	prvm_eval_t *val;

	// EF_NODRAW prevents sending for any reason except for your own
	// client, so we must keep all clients in this superset
	effects = (unsigned)ent->fields.server->effects;

	// we can omit invisible entities with no effects that are not clients
	// LordHavoc: this could kill tags attached to an invisible entity, I
	// just hope we never have to support that case
	i = (int)ent->fields.server->modelindex;
	modelindex = (i >= 1 && i < MAX_MODELS && *PRVM_GetString(ent->fields.server->model)) ? i : 0;

	flags = 0;
	i = (int)(PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.glow_size)->_float * 0.25f);
	glowsize = (unsigned char)bound(0, i, 255);
	if (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.glow_trail)->_float)
		flags |= RENDER_GLOWTRAIL;

	f = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.color)->vector[0]*256;
	light[0] = (unsigned short)bound(0, f, 65535);
	f = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.color)->vector[1]*256;
	light[1] = (unsigned short)bound(0, f, 65535);
	f = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.color)->vector[2]*256;
	light[2] = (unsigned short)bound(0, f, 65535);
	f = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.light_lev)->_float;
	light[3] = (unsigned short)bound(0, f, 65535);
	lightstyle = (unsigned char)PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.style)->_float;
	lightpflags = (unsigned char)PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.pflags)->_float;

	if (gamemode == GAME_TENEBRAE)
	{
		// tenebrae's EF_FULLDYNAMIC conflicts with Q2's EF_NODRAW
		if (effects & 16)
		{
			effects &= ~16;
			lightpflags |= PFLAGS_FULLDYNAMIC;
		}
		// tenebrae's EF_GREEN conflicts with DP's EF_ADDITIVE
		if (effects & 32)
		{
			effects &= ~32;
			light[0] = (int)(0.2*256);
			light[1] = (int)(1.0*256);
			light[2] = (int)(0.2*256);
			light[3] = 200;
			lightpflags |= PFLAGS_FULLDYNAMIC;
		}
	}

	specialvisibilityradius = 0;
	if (lightpflags & PFLAGS_FULLDYNAMIC)
		specialvisibilityradius = max(specialvisibilityradius, light[3]);
	if (glowsize)
		specialvisibilityradius = max(specialvisibilityradius, glowsize * 4);
	if (flags & RENDER_GLOWTRAIL)
		specialvisibilityradius = max(specialvisibilityradius, 100);
	if (effects & (EF_BRIGHTFIELD | EF_MUZZLEFLASH | EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_RED | EF_BLUE | EF_FLAME | EF_STARDUST))
	{
		if (effects & EF_BRIGHTFIELD)
			specialvisibilityradius = max(specialvisibilityradius, 80);
		if (effects & EF_MUZZLEFLASH)
			specialvisibilityradius = max(specialvisibilityradius, 100);
		if (effects & EF_BRIGHTLIGHT)
			specialvisibilityradius = max(specialvisibilityradius, 400);
		if (effects & EF_DIMLIGHT)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_RED)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_BLUE)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_FLAME)
			specialvisibilityradius = max(specialvisibilityradius, 250);
		if (effects & EF_STARDUST)
			specialvisibilityradius = max(specialvisibilityradius, 100);
	}

	// early culling checks
	// (final culling is done by SV_MarkWriteEntityStateToClient)
	customizeentityforclient = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.customizeentityforclient)->function;
	if (!customizeentityforclient)
	{
		if (e > svs.maxclients && (!modelindex && !specialvisibilityradius))
			return false;
		// this 2 billion unit check is actually to detect NAN origins
		// (we really don't want to send those)
		if (VectorLength2(ent->fields.server->origin) > 2000000000.0*2000000000.0)
			return false;
	}


	*cs = defaultstate;
	cs->active = true;
	cs->number = e;
	VectorCopy(ent->fields.server->origin, cs->origin);
	VectorCopy(ent->fields.server->angles, cs->angles);
	cs->flags = flags;
	cs->effects = effects;
	cs->colormap = (unsigned)ent->fields.server->colormap;
	cs->modelindex = modelindex;
	cs->skin = (unsigned)ent->fields.server->skin;
	cs->frame = (unsigned)ent->fields.server->frame;
	cs->viewmodelforclient = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.viewmodelforclient)->edict;
	cs->exteriormodelforclient = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.exteriormodeltoclient)->edict;
	cs->nodrawtoclient = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.nodrawtoclient)->edict;
	cs->drawonlytoclient = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.drawonlytoclient)->edict;
	cs->customizeentityforclient = customizeentityforclient;
	cs->tagentity = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.tag_entity)->edict;
	cs->tagindex = (unsigned char)PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.tag_index)->_float;
	cs->glowsize = glowsize;

	// don't need to init cs->colormod because the defaultstate did that for us
	//cs->colormod[0] = cs->colormod[1] = cs->colormod[2] = 32;
	val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.colormod);
	if (val->vector[0] || val->vector[1] || val->vector[2])
	{
		i = (int)(val->vector[0] * 32.0f);cs->colormod[0] = bound(0, i, 255);
		i = (int)(val->vector[1] * 32.0f);cs->colormod[1] = bound(0, i, 255);
		i = (int)(val->vector[2] * 32.0f);cs->colormod[2] = bound(0, i, 255);
	}

	cs->modelindex = modelindex;

	cs->alpha = 255;
	f = (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.alpha)->_float * 255.0f);
	if (f)
	{
		i = (int)f;
		cs->alpha = (unsigned char)bound(0, i, 255);
	}
	// halflife
	f = (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.renderamt)->_float);
	if (f)
	{
		i = (int)f;
		cs->alpha = (unsigned char)bound(0, i, 255);
	}

	cs->scale = 16;
	f = (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.scale)->_float * 16.0f);
	if (f)
	{
		i = (int)f;
		cs->scale = (unsigned char)bound(0, i, 255);
	}

	cs->glowcolor = 254;
	f = (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.glow_color)->_float);
	if (f)
		cs->glowcolor = (int)f;

	if (PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.fullbright)->_float)
		cs->effects |= EF_FULLBRIGHT;

	if (ent->fields.server->movetype == MOVETYPE_STEP)
		cs->flags |= RENDER_STEP;
	if (cs->number != sv_writeentitiestoclient_clentnum/* && (cs->effects & EF_LOWPRECISION)*/ && cs->origin[0] >= -32768 && cs->origin[1] >= -32768 && cs->origin[2] >= -32768 && cs->origin[0] <= 32767 && cs->origin[1] <= 32767 && cs->origin[2] <= 32767)
		cs->flags |= RENDER_LOWPRECISION;
	if (ent->fields.server->colormap >= 1024)
		cs->flags |= RENDER_COLORMAPPED;
	if (cs->viewmodelforclient)
		cs->flags |= RENDER_VIEWMODEL; // show relative to the view

	cs->light[0] = light[0];
	cs->light[1] = light[1];
	cs->light[2] = light[2];
	cs->light[3] = light[3];
	cs->lightstyle = lightstyle;
	cs->lightpflags = lightpflags;

	cs->specialvisibilityradius = specialvisibilityradius;

	// calculate the visible box of this entity (don't use the physics box
	// as that is often smaller than a model, and would not count
	// specialvisibilityradius)
	if ((model = sv.models[modelindex]))
	{
		float scale = cs->scale * (1.0f / 16.0f);
		if (cs->angles[0] || cs->angles[2]) // pitch and roll
		{
			VectorMA(cs->origin, scale, model->rotatedmins, cullmins);
			VectorMA(cs->origin, scale, model->rotatedmaxs, cullmaxs);
		}
		else if (cs->angles[1])
		{
			VectorMA(cs->origin, scale, model->yawmins, cullmins);
			VectorMA(cs->origin, scale, model->yawmaxs, cullmaxs);
		}
		else
		{
			VectorMA(cs->origin, scale, model->normalmins, cullmins);
			VectorMA(cs->origin, scale, model->normalmaxs, cullmaxs);
		}
	}
	else
	{
		// if there is no model (or it could not be loaded), use the physics box
		VectorAdd(cs->origin, ent->fields.server->mins, cullmins);
		VectorAdd(cs->origin, ent->fields.server->maxs, cullmaxs);
	}
	if (specialvisibilityradius)
	{
		cullmins[0] = min(cullmins[0], cs->origin[0] - specialvisibilityradius);
		cullmins[1] = min(cullmins[1], cs->origin[1] - specialvisibilityradius);
		cullmins[2] = min(cullmins[2], cs->origin[2] - specialvisibilityradius);
		cullmaxs[0] = max(cullmaxs[0], cs->origin[0] + specialvisibilityradius);
		cullmaxs[1] = max(cullmaxs[1], cs->origin[1] + specialvisibilityradius);
		cullmaxs[2] = max(cullmaxs[2], cs->origin[2] + specialvisibilityradius);
	}
	// calculate center of bbox for network prioritization purposes
	VectorMAM(0.5f, cullmins, 0.5f, cullmaxs, cs->netcenter);
	// if culling box has moved, update pvs cluster links
	if (!VectorCompare(cullmins, ent->priv.server->cullmins) || !VectorCompare(cullmaxs, ent->priv.server->cullmaxs))
	{
		VectorCopy(cullmins, ent->priv.server->cullmins);
		VectorCopy(cullmaxs, ent->priv.server->cullmaxs);
		// a value of -1 for pvs_numclusters indicates that the links are not
		// cached, and should be re-tested each time, this is the case if the
		// culling box touches too many pvs clusters to store, or if the world
		// model does not support FindBoxClusters
		ent->priv.server->pvs_numclusters = -1;
		if (sv.worldmodel && sv.worldmodel->brush.FindBoxClusters)
		{
			i = sv.worldmodel->brush.FindBoxClusters(sv.worldmodel, cullmins, cullmaxs, MAX_ENTITYCLUSTERS, ent->priv.server->pvs_clusterlist);
			if (i <= MAX_ENTITYCLUSTERS)
				ent->priv.server->pvs_numclusters = i;
		}
	}

	return true;
}

void SV_PrepareEntitiesForSending(void)
{
	int e;
	prvm_edict_t *ent;
	// send all entities that touch the pvs
	numsendentities = 0;
	sendentitiesindex[0] = NULL;
	memset(sendentitiesindex, 0, prog->num_edicts * sizeof(entity_state_t *));
	for (e = 1, ent = PRVM_NEXT_EDICT(prog->edicts);e < prog->num_edicts;e++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (!ent->priv.server->free && SV_PrepareEntityForSending(ent, sendentities + numsendentities, e))
		{
			sendentitiesindex[e] = sendentities + numsendentities;
			numsendentities++;
		}
	}
}

void SV_MarkWriteEntityStateToClient(entity_state_t *s)
{
	int isbmodel;
	model_t *model;
	prvm_edict_t *ed;
	if (sententitiesconsideration[s->number] == sententitiesmark)
		return;
	sententitiesconsideration[s->number] = sententitiesmark;
	sv_writeentitiestoclient_totalentities++;

	if (s->customizeentityforclient)
	{
		prog->globals.server->self = s->number;
		prog->globals.server->other = sv_writeentitiestoclient_clentnum;
		PRVM_ExecuteProgram(s->customizeentityforclient, "customizeentityforclient: NULL function");
		if(!PRVM_G_FLOAT(OFS_RETURN) || !SV_PrepareEntityForSending(PRVM_EDICT_NUM(s->number), s, s->number))
			return;
	}

	// never reject player
	if (s->number != sv_writeentitiestoclient_clentnum)
	{
		// check various rejection conditions
		if (s->nodrawtoclient == sv_writeentitiestoclient_clentnum)
			return;
		if (s->drawonlytoclient && s->drawonlytoclient != sv_writeentitiestoclient_clentnum)
			return;
		if (s->effects & EF_NODRAW)
			return;
		// LordHavoc: only send entities with a model or important effects
		if (!s->modelindex && s->specialvisibilityradius == 0)
			return;

		isbmodel = (model = sv.models[s->modelindex]) != NULL && model->name[0] == '*';
		// viewmodels don't have visibility checking
		if (s->viewmodelforclient)
		{
			if (s->viewmodelforclient != sv_writeentitiestoclient_clentnum)
				return;
		}
		else if (s->tagentity)
		{
			// tag attached entities simply check their parent
			if (!sendentitiesindex[s->tagentity])
				return;
			SV_MarkWriteEntityStateToClient(sendentitiesindex[s->tagentity]);
			if (sententities[s->tagentity] != sententitiesmark)
				return;
		}
		// always send world submodels in newer protocols because they don't
		// generate much traffic (in old protocols they hog bandwidth)
		// but only if sv_cullentities_alwayssendbmodels is on
		else if (!(s->effects & EF_NODEPTHTEST) && (!isbmodel || !sv_cullentities_nevercullbmodels.integer || sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE))
		{
			// entity has survived every check so far, check if visible
			ed = PRVM_EDICT_NUM(s->number);

			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && sv_writeentitiestoclient_pvsbytes)
			{
				if (ed->priv.server->pvs_numclusters < 0)
				{
					// entity too big for clusters list
					if (sv.worldmodel && sv.worldmodel->brush.BoxTouchingPVS && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, sv_writeentitiestoclient_pvs, ed->priv.server->cullmins, ed->priv.server->cullmaxs))
					{
						sv_writeentitiestoclient_culled_pvs++;
						return;
					}
				}
				else
				{
					int i;
					// check cached clusters list
					for (i = 0;i < ed->priv.server->pvs_numclusters;i++)
						if (CHECKPVSBIT(sv_writeentitiestoclient_pvs, ed->priv.server->pvs_clusterlist[i]))
							break;
					if (i == ed->priv.server->pvs_numclusters)
					{
						sv_writeentitiestoclient_culled_pvs++;
						return;
					}
				}
			}

			// or not seen by random tracelines
			if (sv_cullentities_trace.integer && !isbmodel)
			{
				int samples = s->specialvisibilityradius ? sv_cullentities_trace_samples_extra.integer : sv_cullentities_trace_samples.integer;
				float enlarge = sv_cullentities_trace_enlarge.value;

				qboolean visible = TRUE;

				do
				{
					if(Mod_CanSeeBox_Trace(samples, enlarge, sv.worldmodel, sv_writeentitiestoclient_testeye, ed->priv.server->cullmins, ed->priv.server->cullmaxs))
						break; // directly visible from the server's view

					if(sv_cullentities_trace_prediction.integer)
					{
						vec3_t predeye;

						// get player velocity
						float predtime = bound(0, host_client->ping, 0.2); // / 2
							// sorry, no wallhacking by high ping please, and at 200ms
							// ping a FPS is annoying to play anyway and a player is
							// likely to have changed his direction
						VectorMA(sv_writeentitiestoclient_testeye, predtime, host_client->edict->fields.server->velocity, predeye);
						if(sv.worldmodel->brush.TraceLineOfSight(sv.worldmodel, sv_writeentitiestoclient_testeye, predeye)) // must be able to go there...
						{
							if(Mod_CanSeeBox_Trace(samples, enlarge, sv.worldmodel, predeye, ed->priv.server->cullmins, ed->priv.server->cullmaxs))
								break; // directly visible from the predicted view
						}
						else
						{
							//Con_DPrintf("Trying to walk into solid in a pingtime... not predicting for culling\n");
						}
					}

					// when we get here, we can't see the entity
					visible = FALSE;
				}
				while(0);

				if(visible)
					sv_writeentitiestoclient_client->visibletime[s->number] = realtime + sv_cullentities_trace_delay.value;

				if (realtime > sv_writeentitiestoclient_client->visibletime[s->number])
				{
					sv_writeentitiestoclient_culled_trace++;
					return;
				}
			}
		}
	}

	// this just marks it for sending
	// FIXME: it would be more efficient to send here, but the entity
	// compressor isn't that flexible
	sv_writeentitiestoclient_visibleentities++;
	sententities[s->number] = sententitiesmark;
}

entity_state_t sendstates[MAX_EDICTS];
extern int csqc_clientnum;

void SV_WriteEntitiesToClient(client_t *client, prvm_edict_t *clent, sizebuf_t *msg)
{
	int i, numsendstates;
	entity_state_t *s;

	// if there isn't enough space to accomplish anything, skip it
	if (msg->cursize + 25 > msg->maxsize)
		return;

	sv_writeentitiestoclient_client = client;

	sv_writeentitiestoclient_culled_pvs = 0;
	sv_writeentitiestoclient_culled_trace = 0;
	sv_writeentitiestoclient_visibleentities = 0;
	sv_writeentitiestoclient_totalentities = 0;

// find the client's PVS
	// the real place being tested from
	VectorAdd(clent->fields.server->origin, clent->fields.server->view_ofs, sv_writeentitiestoclient_testeye);
	sv_writeentitiestoclient_pvsbytes = 0;
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		sv_writeentitiestoclient_pvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, sv_writeentitiestoclient_testeye, 8, sv_writeentitiestoclient_pvs, sizeof(sv_writeentitiestoclient_pvs));

	sv_writeentitiestoclient_clentnum = PRVM_EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes
	csqc_clientnum = sv_writeentitiestoclient_clentnum - 1;

	sententitiesmark++;

	for (i = 0;i < numsendentities;i++)
		SV_MarkWriteEntityStateToClient(sendentities + i);

	numsendstates = 0;
	for (i = 0;i < numsendentities;i++)
	{
		if (sententities[sendentities[i].number] == sententitiesmark)
		{
			s = &sendstates[numsendstates++];
			*s = sendentities[i];
			if (s->exteriormodelforclient && s->exteriormodelforclient == sv_writeentitiestoclient_clentnum)
				s->flags |= RENDER_EXTERIORMODEL;
		}
	}

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d trace\n", client->name, sv_writeentitiestoclient_totalentities, sv_writeentitiestoclient_visibleentities, sv_writeentitiestoclient_culled_pvs + sv_writeentitiestoclient_culled_trace, sv_writeentitiestoclient_culled_pvs, sv_writeentitiestoclient_culled_trace);

	EntityFrameCSQC_WriteFrame(msg, numsendstates, sendstates);

	if (client->entitydatabase5)
		EntityFrame5_WriteFrame(msg, client->entitydatabase5, numsendstates, sendstates, client - svs.clients + 1, client->movesequence);
	else if (client->entitydatabase4)
	{
		EntityFrame4_WriteFrame(msg, client->entitydatabase4, numsendstates, sendstates);
		Protocol_WriteStatsReliable();
	}
	else if (client->entitydatabase)
	{
		EntityFrame_WriteFrame(msg, client->entitydatabase, numsendstates, sendstates, client - svs.clients + 1);
		Protocol_WriteStatsReliable();
	}
	else
	{
		EntityFrameQuake_WriteFrame(msg, numsendstates, sendstates);
		Protocol_WriteStatsReliable();
	}
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
	int		e;
	prvm_edict_t	*ent;

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (e=1 ; e<prog->num_edicts ; e++, ent = PRVM_NEXT_EDICT(ent))
		ent->fields.server->effects = (int)ent->fields.server->effects & ~EF_MUZZLEFLASH;
}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (client_t *client, prvm_edict_t *ent, sizebuf_t *msg, int *stats)
{
	int		bits;
	int		i;
	prvm_edict_t	*other;
	int		items;
	prvm_eval_t	*val;
	vec3_t	punchvector;
	int		viewzoom;
	const char *s;

//
// send a damage message
//
	if (ent->fields.server->dmg_take || ent->fields.server->dmg_save)
	{
		other = PRVM_PROG_TO_EDICT(ent->fields.server->dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, (int)ent->fields.server->dmg_save);
		MSG_WriteByte (msg, (int)ent->fields.server->dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->fields.server->origin[i] + 0.5*(other->fields.server->mins[i] + other->fields.server->maxs[i]), sv.protocol);

		ent->fields.server->dmg_take = 0;
		ent->fields.server->dmg_save = 0;
	}

//
// send the current viewpos offset from the view entity
//
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
	if(ent->fields.server->fixangle)
	{
		// angle fixing was requested by global thinking code...
		// so store the current angles for later use
		memcpy(host_client->fixangle_angles, ent->fields.server->angles, sizeof(host_client->fixangle_angles));
		host_client->fixangle_angles_set = TRUE;

		// and clear fixangle for the next frame
		ent->fields.server->fixangle = 0;
	}

	if (host_client->fixangle_angles_set)
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, host_client->fixangle_angles[i], sv.protocol);
		host_client->fixangle_angles_set = FALSE;
	}

	// stuff the sigil bits into the high bits of items for sbar, or else
	// mix in items2
	val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.items2);
	if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE)
		items = (int)ent->fields.server->items | ((int)val->_float << 23);
	else
		items = (int)ent->fields.server->items | ((int)prog->globals.server->serverflags << 28);

	VectorClear(punchvector);
	if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.punchvector)))
		VectorCopy(val->vector, punchvector);

	// cache weapon model name and index in client struct to save time
	// (this search can be almost 1% of cpu time!)
	s = PRVM_GetString(ent->fields.server->weaponmodel);
	if (strcmp(s, client->weaponmodel))
	{
		strlcpy(client->weaponmodel, s, sizeof(client->weaponmodel));
		client->weaponmodelindex = SV_ModelIndex(s, 1);
	}

	viewzoom = 255;
	if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.viewzoom)))
		viewzoom = (int)(val->_float * 255.0f);
	if (viewzoom == 0)
		viewzoom = 255;

	bits = 0;

	if ((int)ent->fields.server->flags & FL_ONGROUND)
		bits |= SU_ONGROUND;
	if (ent->fields.server->waterlevel >= 2)
		bits |= SU_INWATER;
	if (ent->fields.server->idealpitch)
		bits |= SU_IDEALPITCH;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->fields.server->punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE)
			if (punchvector[i])
				bits |= (SU_PUNCHVEC1<<i);
		if (ent->fields.server->velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}

	memset(stats, 0, sizeof(int[MAX_CL_STATS]));
	stats[STAT_VIEWHEIGHT] = (int)ent->fields.server->view_ofs[2];
	stats[STAT_ITEMS] = items;
	stats[STAT_WEAPONFRAME] = (int)ent->fields.server->weaponframe;
	stats[STAT_ARMOR] = (int)ent->fields.server->armorvalue;
	stats[STAT_WEAPON] = client->weaponmodelindex;
	stats[STAT_HEALTH] = (int)ent->fields.server->health;
	stats[STAT_AMMO] = (int)ent->fields.server->currentammo;
	stats[STAT_SHELLS] = (int)ent->fields.server->ammo_shells;
	stats[STAT_NAILS] = (int)ent->fields.server->ammo_nails;
	stats[STAT_ROCKETS] = (int)ent->fields.server->ammo_rockets;
	stats[STAT_CELLS] = (int)ent->fields.server->ammo_cells;
	stats[STAT_ACTIVEWEAPON] = (int)ent->fields.server->weapon;
	stats[STAT_VIEWZOOM] = viewzoom;
	stats[STAT_TOTALSECRETS] = prog->globals.server->total_secrets;
	stats[STAT_TOTALMONSTERS] = prog->globals.server->total_monsters;
	// the QC bumps these itself by sending svc_'s, so we have to keep them
	// zero or they'll be corrected by the engine
	//stats[STAT_SECRETS] = prog->globals.server->found_secrets;
	//stats[STAT_MONSTERS] = prog->globals.server->killed_monsters;

	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
	{
		if (stats[STAT_VIEWHEIGHT] != DEFAULT_VIEWHEIGHT) bits |= SU_VIEWHEIGHT;
		bits |= SU_ITEMS;
		if (stats[STAT_WEAPONFRAME]) bits |= SU_WEAPONFRAME;
		if (stats[STAT_ARMOR]) bits |= SU_ARMOR;
		bits |= SU_WEAPON;
		// FIXME: which protocols support this?  does PROTOCOL_DARKPLACES3 support viewzoom?
		if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
			if (viewzoom != 255)
				bits |= SU_VIEWZOOM;
	}

	if (bits >= 65536)
		bits |= SU_EXTEND1;
	if (bits >= 16777216)
		bits |= SU_EXTEND2;

	// send the data
	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);
	if (bits & SU_EXTEND1)
		MSG_WriteByte(msg, bits >> 16);
	if (bits & SU_EXTEND2)
		MSG_WriteByte(msg, bits >> 24);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, stats[STAT_VIEWHEIGHT]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, (int)ent->fields.server->idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE)
				MSG_WriteChar(msg, (int)ent->fields.server->punchangle[i]);
			else
				MSG_WriteAngle16i(msg, ent->fields.server->punchangle[i]);
		}
		if (bits & (SU_PUNCHVEC1<<i))
		{
			if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteCoord16i(msg, punchvector[i]);
			else
				MSG_WriteCoord32f(msg, punchvector[i]);
		}
		if (bits & (SU_VELOCITY1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteChar(msg, (int)(ent->fields.server->velocity[i] * (1.0f / 16.0f)));
			else
				MSG_WriteCoord32f(msg, ent->fields.server->velocity[i]);
		}
	}

	if (bits & SU_ITEMS)
		MSG_WriteLong (msg, stats[STAT_ITEMS]);

	if (sv.protocol == PROTOCOL_DARKPLACES5)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteShort (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteShort (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
			MSG_WriteShort (msg, stats[STAT_WEAPON]);
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteShort (msg, stats[STAT_AMMO]);
		MSG_WriteShort (msg, stats[STAT_SHELLS]);
		MSG_WriteShort (msg, stats[STAT_NAILS]);
		MSG_WriteShort (msg, stats[STAT_ROCKETS]);
		MSG_WriteShort (msg, stats[STAT_CELLS]);
		MSG_WriteShort (msg, stats[STAT_ACTIVEWEAPON]);
		if (bits & SU_VIEWZOOM)
			MSG_WriteShort (msg, bound(0, stats[STAT_VIEWZOOM], 65535));
	}
	else if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteByte (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteByte (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
			MSG_WriteByte (msg, stats[STAT_WEAPON]);
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteByte (msg, stats[STAT_AMMO]);
		MSG_WriteByte (msg, stats[STAT_SHELLS]);
		MSG_WriteByte (msg, stats[STAT_NAILS]);
		MSG_WriteByte (msg, stats[STAT_ROCKETS]);
		MSG_WriteByte (msg, stats[STAT_CELLS]);
		if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE || gamemode == GAME_NEXUIZ)
		{
			for (i = 0;i < 32;i++)
				if (stats[STAT_WEAPON] & (1<<i))
					break;
			MSG_WriteByte (msg, i);
		}
		else
			MSG_WriteByte (msg, stats[STAT_WEAPON]);
		if (bits & SU_VIEWZOOM)
		{
			if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteByte (msg, bound(0, stats[STAT_VIEWZOOM], 255));
			else
				MSG_WriteShort (msg, bound(0, stats[STAT_VIEWZOOM], 65535));
		}
	}
}

void SV_FlushBroadcastMessages(void)
{
	int i;
	client_t *client;
	if (sv.datagram.cursize <= 0)
		return;
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->spawned || !client->netconnection || client->unreliablemsg.cursize + sv.datagram.cursize > client->unreliablemsg.maxsize || client->unreliablemsg_splitpoints >= (int)(sizeof(client->unreliablemsg_splitpoint)/sizeof(client->unreliablemsg_splitpoint[0])))
			continue;
		SZ_Write(&client->unreliablemsg, sv.datagram.data, sv.datagram.cursize);
		client->unreliablemsg_splitpoint[client->unreliablemsg_splitpoints++] = client->unreliablemsg.cursize;
	}
	SZ_Clear(&sv.datagram);
}

void SV_WriteUnreliableMessages(client_t *client, sizebuf_t *msg)
{
	// scan the splitpoints to find out how many we can fit in
	int numsegments, j, split;
	if (!client->unreliablemsg_splitpoints)
		return;
	// always accept the first one if it's within 1400 bytes, this ensures
	// that very big datagrams which are over the rate limit still get
	// through, just to keep it working
	if (msg->cursize + client->unreliablemsg_splitpoint[0] > msg->maxsize && msg->maxsize < 1400)
	{
		numsegments = 1;
		msg->maxsize = 1400;
	}
	else
		for (numsegments = 0;numsegments < client->unreliablemsg_splitpoints;numsegments++)
			if (msg->cursize + client->unreliablemsg_splitpoint[numsegments] > msg->maxsize)
				break;
	if (numsegments > 0)
	{
		// some will fit, so add the ones that will fit
		split = client->unreliablemsg_splitpoint[numsegments-1];
		// note this discards ones that were accepted by the segments scan but
		// can not fit, such as a really huge first one that will never ever
		// fit in a packet...
		if (msg->cursize + split <= msg->maxsize)
			SZ_Write(msg, client->unreliablemsg.data, split);
		// remove the part we sent, keeping any remaining data
		client->unreliablemsg.cursize -= split;
		if (client->unreliablemsg.cursize > 0)
			memmove(client->unreliablemsg.data, client->unreliablemsg.data + split, client->unreliablemsg.cursize);
		// adjust remaining splitpoints
		client->unreliablemsg_splitpoints -= numsegments;
		for (j = 0;j < client->unreliablemsg_splitpoints;j++)
			client->unreliablemsg_splitpoint[j] = client->unreliablemsg_splitpoint[numsegments + j] - split;
	}
}

/*
=======================
SV_SendClientDatagram
=======================
*/
static unsigned char sv_sendclientdatagram_buf[NET_MAXMESSAGE]; // FIXME?
void SV_SendClientDatagram (client_t *client)
{
	int clientrate, maxrate, maxsize, maxsize2, downloadsize;
	sizebuf_t msg;
	int stats[MAX_CL_STATS];

	// PROTOCOL_DARKPLACES5 and later support packet size limiting of updates
	maxrate = max(NET_MINRATE, sv_maxrate.integer);
	if (sv_maxrate.integer != maxrate)
		Cvar_SetValueQuick(&sv_maxrate, maxrate);
	// clientrate determines the 'cleartime' of a packet
	// (how long to wait before sending another, based on this packet's size)
	clientrate = bound(NET_MINRATE, client->rate, maxrate);

	if (LHNETADDRESS_GetAddressType(&host_client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP && !sv_ratelimitlocalplayer.integer)
	{
		// for good singleplayer, send huge packets and never limit frequency
		clientrate = 1000000000;
		maxsize = sizeof(sv_sendclientdatagram_buf);
		maxsize2 = sizeof(sv_sendclientdatagram_buf);
	}
	else if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
	{
		// no packet size limit support on older protocols because DP1-4 kick
		// the client off if they overflow, and quake protocol shows less than
		// the full entity set if rate limited
		maxsize = 1400;
		maxsize2 = 1400;
	}
	else
	{
		// DP5 and later protocols support packet size limiting which is a
		// better method than limiting packet frequency as QW does
		//
		// this rate limiting does not understand sys_ticrate 0
		// (but no one should be running that on a server!)
		maxsize = (int)(clientrate * sys_ticrate.value);
		maxsize = bound(100, maxsize, 1400);
		maxsize2 = 1400;
	}

	// while downloading, limit entity updates to half the packet
	// (any leftover space will be used for downloading)
	if (host_client->download_file)
		maxsize /= 2;

	msg.data = sv_sendclientdatagram_buf;
	msg.maxsize = maxsize;
	msg.cursize = 0;

	// obey rate limit by limiting packet frequency if the packet size
	// limiting fails
	// (usually this is caused by reliable messages)
	if (!NetConn_CanSend(client->netconnection))
	{
		// send the datagram
		//NetConn_SendUnreliableMessage (client->netconnection, &msg, sv.protocol, clientrate);
		return;
	}
	else if (host_client->spawned)
	{
		MSG_WriteByte (&msg, svc_time);
		MSG_WriteFloat (&msg, sv.time);

		// add the client specific data to the datagram
		SV_WriteClientdataToMessage (client, client->edict, &msg, stats);
		// now update the stats[] array using any registered custom fields
		VM_SV_UpdateCustomStats (client, client->edict, &msg, stats);
		// set host_client->statsdeltabits
		Protocol_UpdateClientStats (stats);

		// add as many queued unreliable messages (effects) as we can fit
		// limit effects to half of the remaining space
		msg.maxsize -= (msg.maxsize - msg.cursize) / 2;
		if (client->unreliablemsg.cursize)
			SV_WriteUnreliableMessages (client, &msg);

		msg.maxsize = maxsize;

		// now write as many entities as we can fit, and also sends stats
		SV_WriteEntitiesToClient (client, client->edict, &msg);
	}
	else if (realtime > client->keepalivetime)
	{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		msg.maxsize = maxsize2;
		client->keepalivetime = realtime + 5;
		MSG_WriteChar (&msg, svc_nop);
	}

	msg.maxsize = maxsize2;

	// if a download is active, see if there is room to fit some download data
	// in this packet
	downloadsize = maxsize * 2 - msg.cursize - 7;
	if (host_client->download_file && host_client->download_started && downloadsize > 0)
	{
		fs_offset_t downloadstart;
		unsigned char data[1400];
		downloadstart = FS_Tell(host_client->download_file);
		downloadsize = min(downloadsize, (int)sizeof(data));
		downloadsize = FS_Read(host_client->download_file, data, downloadsize);
		// note this sends empty messages if at the end of the file, which is
		// necessary to keep the packet loss logic working
		// (the last blocks may be lost and need to be re-sent, and that will
		//  only occur if the client acks the empty end messages, revealing
		//  a gap in the download progress, causing the last blocks to be
		//  sent again)
		MSG_WriteChar (&msg, svc_downloaddata);
		MSG_WriteLong (&msg, downloadstart);
		MSG_WriteShort (&msg, downloadsize);
		if (downloadsize > 0)
			SZ_Write (&msg, data, downloadsize);
	}

// send the datagram
	NetConn_SendUnreliableMessage (client->netconnection, &msg, sv.protocol, clientrate);
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	int i, j;
	client_t *client;
	prvm_eval_t *val;
	const char *name;
	const char *model;
	const char *skin;

// check for changes to be sent over the reliable streams
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		// update the host_client fields we care about according to the entity fields
		host_client->edict = PRVM_EDICT_NUM(i+1);

		// DP_SV_CLIENTNAME
		name = PRVM_GetString(host_client->edict->fields.server->netname);
		if (name == NULL)
			name = "";
		// always point the string back at host_client->name to keep it safe
		strlcpy (host_client->name, name, sizeof (host_client->name));
		host_client->edict->fields.server->netname = PRVM_SetEngineString(host_client->name);
		if (strcmp(host_client->old_name, host_client->name))
		{
			if (host_client->spawned)
				SV_BroadcastPrintf("%s changed name to %s\n", host_client->old_name, host_client->name);
			strlcpy(host_client->old_name, host_client->name, sizeof(host_client->old_name));
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteString (&sv.reliable_datagram, host_client->name);
		}

		// DP_SV_CLIENTCOLORS
		// this is always found (since it's added by the progs loader)
		if ((val = PRVM_EDICTFIELDVALUE(host_client->edict, prog->fieldoffsets.clientcolors)))
			host_client->colors = (int)val->_float;
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
		}

		// NEXUIZ_PLAYERMODEL
		if( prog->fieldoffsets.playermodel >= 0 ) {
			model = PRVM_GetString(PRVM_EDICTFIELDVALUE(host_client->edict, prog->fieldoffsets.playermodel)->string);
			if (model == NULL)
				model = "";
			// always point the string back at host_client->name to keep it safe
			strlcpy (host_client->playermodel, model, sizeof (host_client->playermodel));
			PRVM_EDICTFIELDVALUE(host_client->edict, prog->fieldoffsets.playermodel)->string = PRVM_SetEngineString(host_client->playermodel);
		}

		// NEXUIZ_PLAYERSKIN
		if( prog->fieldoffsets.playerskin >= 0 ) {
			skin = PRVM_GetString(PRVM_EDICTFIELDVALUE(host_client->edict, prog->fieldoffsets.playerskin)->string);
			if (skin == NULL)
				skin = "";
			// always point the string back at host_client->name to keep it safe
			strlcpy (host_client->playerskin, skin, sizeof (host_client->playerskin));
			PRVM_EDICTFIELDVALUE(host_client->edict, prog->fieldoffsets.playerskin)->string = PRVM_SetEngineString(host_client->playerskin);
		}

		// frags
		host_client->frags = (int)host_client->edict->fields.server->frags;
		if (host_client->old_frags != host_client->frags)
		{
			host_client->old_frags = host_client->frags;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatefrags);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteShort (&sv.reliable_datagram, host_client->frags);
		}
	}

	for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
		if (client->netconnection)
			SZ_Write (&client->netconnection->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int i, prepared = false;

	if (sv.protocol == PROTOCOL_QUAKEWORLD)
		Sys_Error("SV_SendClientMessages: no quakeworld support\n");

	SV_FlushBroadcastMessages();

// update frags, names, etc
	SV_UpdateToReliableMessages();

// build individual updates
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;
		if (!host_client->netconnection)
			continue;

		if (host_client->netconnection->message.overflowed)
		{
			SV_DropClient (true);	// if the message couldn't send, kick off
			continue;
		}

		if (!prepared)
		{
			prepared = true;
			// only prepare entities once per frame
			SV_PrepareEntitiesForSending();
		}
		SV_SendClientDatagram (host_client);
	}

// clear muzzle flashes
	SV_CleanupEnts();
}

void SV_StartDownload_f(void)
{
	if (host_client->download_file)
		host_client->download_started = true;
}

void SV_Download_f(void)
{
	const char *whichpack, *whichpack2, *extension;

	if (Cmd_Argc() != 2)
	{
		SV_ClientPrintf("usage: download <filename>\n");
		return;
	}

	if (FS_CheckNastyPath(Cmd_Argv(1), false))
	{
		SV_ClientPrintf("Download rejected: nasty filename \"%s\"\n", Cmd_Argv(1));
		return;
	}

	if (host_client->download_file)
	{
		// at this point we'll assume the previous download should be aborted
		Con_DPrintf("Download of %s aborted by %s starting a new download\n", host_client->download_name, host_client->name);
		Host_ClientCommands("\nstopdownload\n");

		// close the file and reset variables
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		host_client->download_name[0] = 0;
		host_client->download_expectedposition = 0;
		host_client->download_started = false;
	}

	if (!sv_allowdownloads.integer)
	{
		SV_ClientPrintf("Downloads are disabled on this server\n");
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	strlcpy(host_client->download_name, Cmd_Argv(1), sizeof(host_client->download_name));
	extension = FS_FileExtension(host_client->download_name);

	// host_client is asking to download a specified file
	if (developer.integer >= 100)
		Con_Printf("Download request for %s by %s\n", host_client->download_name, host_client->name);

	if (!FS_FileExists(host_client->download_name))
	{
		SV_ClientPrintf("Download rejected: server does not have the file \"%s\"\nYou may need to separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	// check if the user is trying to download part of registered Quake(r)
	whichpack = FS_WhichPack(host_client->download_name);
	whichpack2 = FS_WhichPack("gfx/pop.lmp");
	if ((whichpack && whichpack2 && !strcasecmp(whichpack, whichpack2)) || FS_IsRegisteredQuakePack(host_client->download_name))
	{
		SV_ClientPrintf("Download rejected: file \"%s\" is part of registered Quake(r)\nYou must purchase Quake(r) from id Software or a retailer to get this file\nPlease go to http://www.idsoftware.com/games/quake/quake/index.php?game_section=buy\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	// check if the server has forbidden archive downloads entirely
	if (!sv_allowdownloads_inarchive.integer)
	{
		whichpack = FS_WhichPack(host_client->download_name);
		if (whichpack)
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is in an archive (\"%s\")\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name, whichpack);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_config.integer)
	{
		if (!strcasecmp(extension, "cfg"))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is a .cfg file which is forbidden for security reasons\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_dlcache.integer)
	{
		if (!strncasecmp(host_client->download_name, "dlcache/", 8))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is in the dlcache/ directory which is forbidden for security reasons\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_archive.integer)
	{
		if (!strcasecmp(extension, "pak") || !strcasecmp(extension, "pk3"))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is an archive\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	host_client->download_file = FS_Open(host_client->download_name, "rb", true, false);
	if (!host_client->download_file)
	{
		SV_ClientPrintf("Download rejected: server could not open the file \"%s\"\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	if (FS_FileSize(host_client->download_file) > 1<<30)
	{
		SV_ClientPrintf("Download rejected: file \"%s\" is very large\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		return;
	}

	Con_DPrintf("Downloading %s to %s\n", host_client->download_name, host_client->name);

	Host_ClientCommands("\ncl_downloadbegin %i %s\n", (int)FS_FileSize(host_client->download_file), host_client->download_name);

	host_client->download_expectedposition = 0;
	host_client->download_started = false;

	// the rest of the download process is handled in SV_SendClientDatagram
	// and other code dealing with svc_downloaddata and clc_ackdownloaddata
	//
	// no svc_downloaddata messages will be sent until sv_startdownload is
	// sent by the client
}

/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex(const char *s, int precachemode)
{
	int i, limit = ((sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE) ? 256 : MAX_MODELS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 2;i < limit;i++)
	{
		if (!sv.model_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_ModelIndex(\"%s\"): precache_model can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_ModelIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.model_precache[i], filename, sizeof(sv.model_precache[i]));
				sv.models[i] = Mod_ForName (sv.model_precache[i], true, false, false);
				if (sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_ModelIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.model_precache[i], filename))
			return i;
	}
	Con_Printf("SV_ModelIndex(\"%s\"): i (%i) == MAX_MODELS (%i)\n", filename, i, MAX_MODELS);
	return 0;
}

/*
================
SV_SoundIndex

================
*/
int SV_SoundIndex(const char *s, int precachemode)
{
	int i, limit = ((sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE) ? 256 : MAX_SOUNDS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 1;i < limit;i++)
	{
		if (!sv.sound_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_SoundIndex(\"%s\"): precache_sound can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_SoundIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.sound_precache[i], filename, sizeof(sv.sound_precache[i]));
				if (sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i + 32768);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_SoundIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.sound_precache[i], filename))
			return i;
	}
	Con_Printf("SV_SoundIndex(\"%s\"): i (%i) == MAX_SOUNDS (%i)\n", filename, i, MAX_SOUNDS);
	return 0;
}

// MUST match effectnameindex_t in client.h
static const char *standardeffectnames[EFFECT_TOTAL] =
{
	"",
	"TE_GUNSHOT",
	"TE_GUNSHOTQUAD",
	"TE_SPIKE",
	"TE_SPIKEQUAD",
	"TE_SUPERSPIKE",
	"TE_SUPERSPIKEQUAD",
	"TE_WIZSPIKE",
	"TE_KNIGHTSPIKE",
	"TE_EXPLOSION",
	"TE_EXPLOSIONQUAD",
	"TE_TAREXPLOSION",
	"TE_TELEPORT",
	"TE_LAVASPLASH",
	"TE_SMALLFLASH",
	"TE_FLAMEJET",
	"EF_FLAME",
	"TE_BLOOD",
	"TE_SPARK",
	"TE_PLASMABURN",
	"TE_TEI_G3",
	"TE_TEI_SMOKE",
	"TE_TEI_BIGEXPLOSION",
	"TE_TEI_PLASMAHIT",
	"EF_STARDUST",
	"TR_ROCKET",
	"TR_GRENADE",
	"TR_BLOOD",
	"TR_WIZSPIKE",
	"TR_SLIGHTBLOOD",
	"TR_KNIGHTSPIKE",
	"TR_VORESPIKE",
	"TR_NEHAHRASMOKE",
	"TR_NEXUIZPLASMA",
	"TR_GLOWTRAIL",
	"SVC_PARTICLE"
};

/*
================
SV_ParticleEffectIndex

================
*/
int SV_ParticleEffectIndex(const char *name)
{
	int i, argc, linenumber, effectnameindex;
	fs_offset_t filesize;
	unsigned char *filedata;
	const char *text, *textstart, *textend;
	char argv[16][1024];
	if (!sv.particleeffectnamesloaded)
	{
		sv.particleeffectnamesloaded = true;
		memset(sv.particleeffectname, 0, sizeof(sv.particleeffectname));
		for (i = 0;i < EFFECT_TOTAL;i++)
			strlcpy(sv.particleeffectname[i], standardeffectnames[i], sizeof(sv.particleeffectname[i]));
		filedata = FS_LoadFile("effectinfo.txt", tempmempool, true, &filesize);
		if (filedata)
		{
			textstart = (const char *)filedata;
			textend = (const char *)filedata + filesize;
			text = textstart;
			for (linenumber = 1;;linenumber++)
			{
				argc = 0;
				for (;;)
				{
					if (!COM_ParseToken(&text, true) || !strcmp(com_token, "\n"))
						break;
					if (argc < 16)
					{
						strlcpy(argv[argc], com_token, sizeof(argv[argc]));
						argc++;
					}
				}
				if (com_token[0] == 0)
					break; // if the loop exited and it's not a \n, it's EOF
				if (argc < 1)
					continue;
				if (!strcmp(argv[0], "effect"))
				{
					if (argc == 2)
					{
						for (effectnameindex = 1;effectnameindex < SV_MAX_PARTICLEEFFECTNAME;effectnameindex++)
						{
							if (sv.particleeffectname[effectnameindex][0])
							{
								if (!strcmp(sv.particleeffectname[effectnameindex], argv[1]))
									break;
							}
							else
							{
								strlcpy(sv.particleeffectname[effectnameindex], argv[1], sizeof(sv.particleeffectname[effectnameindex]));
								break;
							}
						}
						// if we run out of names, abort
						if (effectnameindex == SV_MAX_PARTICLEEFFECTNAME)
						{
							Con_Printf("effectinfo.txt:%i: too many effects!\n", linenumber);
							break;
						}
					}
				}
			}
			Mem_Free(filedata);
		}
	}
	// search for the name
	for (effectnameindex = 1;effectnameindex < SV_MAX_PARTICLEEFFECTNAME && sv.particleeffectname[effectnameindex][0];effectnameindex++)
		if (!strcmp(sv.particleeffectname[effectnameindex], name))
			return effectnameindex;
	// return 0 if we couldn't find it
	return 0;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
	int i, entnum, large;
	prvm_edict_t *svent;

	// LordHavoc: clear *all* states (note just active ones)
	for (entnum = 0;entnum < prog->max_edicts;entnum++)
	{
		// get the current server version
		svent = PRVM_EDICT_NUM(entnum);

		// LordHavoc: always clear state values, whether the entity is in use or not
		svent->priv.server->baseline = defaultstate;

		if (svent->priv.server->free)
			continue;
		if (entnum > svs.maxclients && !svent->fields.server->modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->fields.server->origin, svent->priv.server->baseline.origin);
		VectorCopy (svent->fields.server->angles, svent->priv.server->baseline.angles);
		svent->priv.server->baseline.frame = (int)svent->fields.server->frame;
		svent->priv.server->baseline.skin = (int)svent->fields.server->skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->priv.server->baseline.colormap = entnum;
			svent->priv.server->baseline.modelindex = SV_ModelIndex("progs/player.mdl", 1);
		}
		else
		{
			svent->priv.server->baseline.colormap = 0;
			svent->priv.server->baseline.modelindex = (int)svent->fields.server->modelindex;
		}

		large = false;
		if (svent->priv.server->baseline.modelindex & 0xFF00 || svent->priv.server->baseline.frame & 0xFF00)
			large = true;

		// add to the message
		if (large)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		if (large)
		{
			MSG_WriteShort (&sv.signon, svent->priv.server->baseline.modelindex);
			MSG_WriteShort (&sv.signon, svent->priv.server->baseline.frame);
		}
		else
		{
			MSG_WriteByte (&sv.signon, svent->priv.server->baseline.modelindex);
			MSG_WriteByte (&sv.signon, svent->priv.server->baseline.frame);
		}
		MSG_WriteByte (&sv.signon, svent->priv.server->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->priv.server->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->priv.server->baseline.origin[i], sv.protocol);
			MSG_WriteAngle(&sv.signon, svent->priv.server->baseline.angles[i], sv.protocol);
		}
	}
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = (int)prog->globals.server->serverflags;

	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (prog->globals.server->SetChangeParms, "QC function SetChangeParms is missing");
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&prog->globals.server->parm1)[j];
	}
}
/*
void SV_IncreaseEdicts(void)
{
	int i;
	prvm_edict_t *ent;
	int oldmax_edicts = prog->max_edicts;
	void *oldedictsengineprivate = prog->edictprivate;
	void *oldedictsfields = prog->edictsfields;
	void *oldmoved_edicts = sv.moved_edicts;

	if (prog->max_edicts >= MAX_EDICTS)
		return;

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
	{
		if (!ent->priv.server->free)
			SV_UnlinkEdict(prog->edicts + i);
		memset(&ent->priv.server->areagrid, 0, sizeof(ent->priv.server->areagrid));
	}
	World_Clear(&sv.world);

	prog->max_edicts   = min(prog->max_edicts + 256, MAX_EDICTS);
	prog->edictprivate = PR_Alloc(prog->max_edicts * sizeof(edict_engineprivate_t));
	prog->edictsfields = PR_Alloc(prog->max_edicts * prog->edict_size);
	sv.moved_edicts = PR_Alloc(prog->max_edicts * sizeof(prvm_edict_t *));

	memcpy(prog->edictprivate, oldedictsengineprivate, oldmax_edicts * sizeof(edict_engineprivate_t));
	memcpy(prog->edictsfields, oldedictsfields, oldmax_edicts * prog->edict_size);

	for (i = 0, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
	{
		ent->priv.vp = (unsigned char*) prog->edictprivate + i * prog->edictprivate_size;
		ent->fields.server = (void *)((unsigned char *)prog->edictsfields + i * prog->edict_size);
		// link every entity except world
		if (!ent->priv.server->free)
			SV_LinkEdict(ent, false);
	}

	PR_Free(oldedictsengineprivate);
	PR_Free(oldedictsfields);
	PR_Free(oldmoved_edicts);
}*/

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (const char *server)
{
	prvm_edict_t *ent;
	int i;
	char *entities;
	model_t *worldmodel;
	char modelname[sizeof(sv.modelname)];

	Con_DPrintf("SpawnServer: %s\n", server);

	if (cls.state != ca_dedicated)
		SCR_BeginLoadingPlaque();

	dpsnprintf (modelname, sizeof(modelname), "maps/%s.bsp", server);
	worldmodel = Mod_ForName(modelname, false, true, true);
	if (!worldmodel || !worldmodel->TraceBox)
	{
		Con_Printf("Couldn't load map %s\n", modelname);
		return;
	}

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	svs.changelevel_issued = false;		// now safe to issue another

	// make the map a required file for clients
	Curl_ClearRequirements();
	Curl_RequireFile(modelname);

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
	{
		client_t *client;
		for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
		{
			if (client->netconnection)
			{
				MSG_WriteByte(&client->netconnection->message, svc_stufftext);
				MSG_WriteString(&client->netconnection->message, "reconnect\n");
			}
		}
	}
	else
	{
		// open server port
		NetConn_OpenServerPorts(true);
	}

//
// make cvars consistant
//
	if (coop.integer)
		Cvar_SetValue ("deathmatch", 0);
	// LordHavoc: it can be useful to have skills outside the range 0-3...
	//current_skill = bound(0, (int)(skill.value + 0.5), 3);
	//Cvar_SetValue ("skill", (float)current_skill);
	current_skill = (int)(skill.value + 0.5);

//
// set up the new server
//
	memset (&sv, 0, sizeof(sv));
	// if running a local client, make sure it doesn't try to access the last
	// level's data which is no longer valiud
	cls.signon = 0;

	if(*sv_random_seed.string)
	{
		srand(sv_random_seed.integer);
		Con_Printf("NOTE: random seed is %d; use for debugging/benchmarking only!\nUnset sv_random_seed to get real random numbers again.\n", sv_random_seed.integer);
	}

	SV_VM_Setup();

	sv.active = true;

	strlcpy (sv.name, server, sizeof (sv.name));

	sv.protocol = Protocol_EnumForName(sv_protocolname.string);
	if (sv.protocol == PROTOCOL_UNKNOWN)
	{
		char buffer[1024];
		Protocol_Names(buffer, sizeof(buffer));
		Con_Printf("Unknown sv_protocolname \"%s\", valid values are:\n%s\n", sv_protocolname.string, buffer);
		sv.protocol = PROTOCOL_QUAKE;
	}

	SV_VM_Begin();

// load progs to get entity field count
	//PR_LoadProgs ( sv_progs.string );

	// allocate server memory
	/*// start out with just enough room for clients and a reasonable estimate of entities
	prog->max_edicts = max(svs.maxclients + 1, 512);
	prog->max_edicts = min(prog->max_edicts, MAX_EDICTS);

	// prvm_edict_t structures (hidden from progs)
	prog->edicts = PR_Alloc(MAX_EDICTS * sizeof(prvm_edict_t));
	// engine private structures (hidden from progs)
	prog->edictprivate = PR_Alloc(prog->max_edicts * sizeof(edict_engineprivate_t));
	// progs fields, often accessed by server
	prog->edictsfields = PR_Alloc(prog->max_edicts * prog->edict_size);*/
	// used by PushMove to move back pushed entities
	sv.moved_edicts = (prvm_edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(prvm_edict_t *));
	/*for (i = 0;i < prog->max_edicts;i++)
	{
		ent = prog->edicts + i;
		ent->priv.vp = (unsigned char*) prog->edictprivate + i * prog->edictprivate_size;
		ent->fields.server = (void *)((unsigned char *)prog->edictsfields + i * prog->edict_size);
	}*/

	// reset client csqc entity versions right away.
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		EntityFrameCSQC_InitClientVersions(i, true);

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
	//prog->num_edicts = svs.maxclients+1;

	sv.state = ss_loading;
	prog->allowworldwrites = true;
	sv.paused = false;

	prog->globals.server->time = sv.time = 1.0;

	Mod_ClearUsed();
	worldmodel->used = true;

	strlcpy (sv.name, server, sizeof (sv.name));
	strlcpy(sv.modelname, modelname, sizeof(sv.modelname));
	sv.worldmodel = worldmodel;
	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	VectorCopy(sv.worldmodel->normalmins, sv.world.areagrid_mins);
	VectorCopy(sv.worldmodel->normalmaxs, sv.world.areagrid_maxs);
	World_Clear(&sv.world);

	strlcpy(sv.sound_precache[0], "", sizeof(sv.sound_precache[0]));

	strlcpy(sv.model_precache[0], "", sizeof(sv.model_precache[0]));
	strlcpy(sv.model_precache[1], sv.modelname, sizeof(sv.model_precache[1]));
	for (i = 1;i < sv.worldmodel->brush.numsubmodels;i++)
	{
		dpsnprintf(sv.model_precache[i+1], sizeof(sv.model_precache[i+1]), "*%i", i);
		sv.models[i+1] = Mod_ForName (sv.model_precache[i+1], false, false, false);
	}

//
// load the rest of the entities
//
	// AK possible hack since num_edicts is still 0
	ent = PRVM_EDICT_NUM(0);
	memset (ent->fields.server, 0, prog->progs->entityfields * 4);
	ent->priv.server->free = false;
	ent->fields.server->model = PRVM_SetEngineString(sv.modelname);
	ent->fields.server->modelindex = 1;		// world model
	ent->fields.server->solid = SOLID_BSP;
	ent->fields.server->movetype = MOVETYPE_PUSH;
	VectorCopy(sv.worldmodel->normalmins, ent->fields.server->mins);
	VectorCopy(sv.worldmodel->normalmaxs, ent->fields.server->maxs);
	VectorCopy(sv.worldmodel->normalmins, ent->fields.server->absmin);
	VectorCopy(sv.worldmodel->normalmaxs, ent->fields.server->absmax);

	if (coop.value)
		prog->globals.server->coop = coop.integer;
	else
		prog->globals.server->deathmatch = deathmatch.integer;

	prog->globals.server->mapname = PRVM_SetEngineString(sv.name);

// serverflags are for cross level information (sigils)
	prog->globals.server->serverflags = svs.serverflags;

	// we need to reset the spawned flag on all connected clients here so that
	// their thinks don't run during startup (before PutClientInServer)
	// we also need to set up the client entities now
	// and we need to set the ->edict pointers to point into the progs edicts
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		host_client->spawned = false;
		host_client->edict = PRVM_EDICT_NUM(i + 1);
		PRVM_ED_ClearEdict(host_client->edict);
	}

	// load replacement entity file if found
	if (sv_entpatch.integer && (entities = (char *)FS_LoadFile(va("maps/%s.ent", sv.name), tempmempool, true, NULL)))
	{
		Con_Printf("Loaded maps/%s.ent\n", sv.name);
		PRVM_ED_LoadFromFile (entities);
		Mem_Free(entities);
	}
	else
		PRVM_ED_LoadFromFile (sv.worldmodel->brush.entities);


	// LordHavoc: clear world angles (to fix e3m3.bsp)
	VectorClear(prog->edicts->fields.server->angles);

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;
	prog->allowworldwrites = false;

// run two frames to allow everything to settle
	for (i = 0;i < 2;i++)
	{
		sv.frametime = 0.1;
		SV_Physics ();
	}

	Mod_PurgeUnused();

// create a baseline for more efficient communications
	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE)
		SV_CreateBaseline ();

// send serverinfo to all connected clients, and set up botclients coming back from a level change
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;
		if (host_client->netconnection)
			SV_SendServerinfo(host_client);
		else
		{
			int j;
			// if client is a botclient coming from a level change, we need to
			// set up client info that normally requires networking

			// copy spawn parms out of the client_t
			for (j=0 ; j< NUM_SPAWN_PARMS ; j++)
				(&prog->globals.server->parm1)[j] = host_client->spawn_parms[j];

			// call the spawn function
			host_client->clientconnectcalled = true;
			prog->globals.server->time = sv.time;
			prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
			PRVM_ExecuteProgram (prog->globals.server->ClientConnect, "QC function ClientConnect is missing");
			PRVM_ExecuteProgram (prog->globals.server->PutClientInServer, "QC function PutClientInServer is missing");
			host_client->spawned = true;
		}
	}

	Con_DPrint("Server spawned.\n");
	NetConn_Heartbeat (2);

	SV_VM_End();
}

/////////////////////////////////////////////////////
// SV VM stuff

void SV_VM_CB_BeginIncreaseEdicts(void)
{
	int i;
	prvm_edict_t *ent;

	PRVM_Free( sv.moved_edicts );
	sv.moved_edicts = (prvm_edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(prvm_edict_t *));

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
	{
		if (!ent->priv.server->free)
			World_UnlinkEdict(prog->edicts + i);
		memset(&ent->priv.server->areagrid, 0, sizeof(ent->priv.server->areagrid));
	}
	World_Clear(&sv.world);
}

void SV_VM_CB_EndIncreaseEdicts(void)
{
	int i;
	prvm_edict_t *ent;

	// link every entity except world
	for (i = 1, ent = prog->edicts;i < prog->max_edicts;i++, ent++)
		if (!ent->priv.server->free)
			SV_LinkEdict(ent, false);
}

void SV_VM_CB_InitEdict(prvm_edict_t *e)
{
	// LordHavoc: for consistency set these here
	int num = PRVM_NUM_FOR_EDICT(e) - 1;

	e->priv.server->move = false; // don't move on first frame

	if (num >= 0 && num < svs.maxclients)
	{
		prvm_eval_t *val;
		// set colormap and team on newly created player entity
		e->fields.server->colormap = num + 1;
		e->fields.server->team = (svs.clients[num].colors & 15) + 1;
		// set netname/clientcolors back to client values so that
		// DP_SV_CLIENTNAME and DP_SV_CLIENTCOLORS will not immediately
		// reset them
		e->fields.server->netname = PRVM_SetEngineString(svs.clients[num].name);
		if ((val = PRVM_EDICTFIELDVALUE(e, prog->fieldoffsets.clientcolors)))
			val->_float = svs.clients[num].colors;
		// NEXUIZ_PLAYERMODEL and NEXUIZ_PLAYERSKIN
		if( prog->fieldoffsets.playermodel >= 0 )
			PRVM_EDICTFIELDVALUE(e, prog->fieldoffsets.playermodel)->string = PRVM_SetEngineString(svs.clients[num].playermodel);
		if( prog->fieldoffsets.playerskin >= 0 )
			PRVM_EDICTFIELDVALUE(e, prog->fieldoffsets.playerskin)->string = PRVM_SetEngineString(svs.clients[num].playerskin);
	}
}

void SV_VM_CB_FreeEdict(prvm_edict_t *ed)
{
	World_UnlinkEdict(ed);		// unlink from world bsp

	ed->fields.server->model = 0;
	ed->fields.server->takedamage = 0;
	ed->fields.server->modelindex = 0;
	ed->fields.server->colormap = 0;
	ed->fields.server->skin = 0;
	ed->fields.server->frame = 0;
	VectorClear(ed->fields.server->origin);
	VectorClear(ed->fields.server->angles);
	ed->fields.server->nextthink = -1;
	ed->fields.server->solid = 0;
}

void SV_VM_CB_CountEdicts(void)
{
	int		i;
	prvm_edict_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for (i=0 ; i<prog->num_edicts ; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.server->free)
			continue;
		active++;
		if (ent->fields.server->solid)
			solid++;
		if (ent->fields.server->model)
			models++;
		if (ent->fields.server->movetype == MOVETYPE_STEP)
			step++;
	}

	Con_Printf("num_edicts:%3i\n", prog->num_edicts);
	Con_Printf("active    :%3i\n", active);
	Con_Printf("view      :%3i\n", models);
	Con_Printf("touch     :%3i\n", solid);
	Con_Printf("step      :%3i\n", step);
}

qboolean SV_VM_CB_LoadEdict(prvm_edict_t *ent)
{
	// remove things from different skill levels or deathmatch
	if (gamemode != GAME_TRANSFUSION) //Transfusion does this in QC
	{
		if (deathmatch.integer)
		{
			if (((int)ent->fields.server->spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				return false;
			}
		}
		else if ((current_skill <= 0 && ((int)ent->fields.server->spawnflags & SPAWNFLAG_NOT_EASY  ))
			|| (current_skill == 1 && ((int)ent->fields.server->spawnflags & SPAWNFLAG_NOT_MEDIUM))
			|| (current_skill >= 2 && ((int)ent->fields.server->spawnflags & SPAWNFLAG_NOT_HARD  )))
		{
			return false;
		}
	}
	return true;
}

cvar_t pr_checkextension = {CVAR_READONLY, "pr_checkextension", "1", "indicates to QuakeC that the standard quakec extensions system is available (if 0, quakec should not attempt to use extensions)"};
cvar_t nomonsters = {0, "nomonsters", "0", "unused cvar in quake, can be used by mods"};
cvar_t gamecfg = {0, "gamecfg", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch1 = {0, "scratch1", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch2 = {0,"scratch2", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch3 = {0, "scratch3", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch4 = {0, "scratch4", "0", "unused cvar in quake, can be used by mods"};
cvar_t savedgamecfg = {CVAR_SAVE, "savedgamecfg", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved1 = {CVAR_SAVE, "saved1", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved2 = {CVAR_SAVE, "saved2", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved3 = {CVAR_SAVE, "saved3", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved4 = {CVAR_SAVE, "saved4", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t nehx00 = {0, "nehx00", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx01 = {0, "nehx01", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx02 = {0, "nehx02", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx03 = {0, "nehx03", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx04 = {0, "nehx04", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx05 = {0, "nehx05", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx06 = {0, "nehx06", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx07 = {0, "nehx07", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx08 = {0, "nehx08", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx09 = {0, "nehx09", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx10 = {0, "nehx10", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx11 = {0, "nehx11", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx12 = {0, "nehx12", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx13 = {0, "nehx13", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx14 = {0, "nehx14", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx15 = {0, "nehx15", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx16 = {0, "nehx16", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx17 = {0, "nehx17", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx18 = {0, "nehx18", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx19 = {0, "nehx19", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t cutscene = {0, "cutscene", "1", "enables cutscenes in nehahra, can be used by other mods"};

void SV_VM_Init(void)
{
	Cvar_RegisterVariable (&pr_checkextension);
	Cvar_RegisterVariable (&nomonsters);
	Cvar_RegisterVariable (&gamecfg);
	Cvar_RegisterVariable (&scratch1);
	Cvar_RegisterVariable (&scratch2);
	Cvar_RegisterVariable (&scratch3);
	Cvar_RegisterVariable (&scratch4);
	Cvar_RegisterVariable (&savedgamecfg);
	Cvar_RegisterVariable (&saved1);
	Cvar_RegisterVariable (&saved2);
	Cvar_RegisterVariable (&saved3);
	Cvar_RegisterVariable (&saved4);
	// LordHavoc: Nehahra uses these to pass data around cutscene demos
	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_RegisterVariable (&nehx00);
		Cvar_RegisterVariable (&nehx01);
		Cvar_RegisterVariable (&nehx02);
		Cvar_RegisterVariable (&nehx03);
		Cvar_RegisterVariable (&nehx04);
		Cvar_RegisterVariable (&nehx05);
		Cvar_RegisterVariable (&nehx06);
		Cvar_RegisterVariable (&nehx07);
		Cvar_RegisterVariable (&nehx08);
		Cvar_RegisterVariable (&nehx09);
		Cvar_RegisterVariable (&nehx10);
		Cvar_RegisterVariable (&nehx11);
		Cvar_RegisterVariable (&nehx12);
		Cvar_RegisterVariable (&nehx13);
		Cvar_RegisterVariable (&nehx14);
		Cvar_RegisterVariable (&nehx15);
		Cvar_RegisterVariable (&nehx16);
		Cvar_RegisterVariable (&nehx17);
		Cvar_RegisterVariable (&nehx18);
		Cvar_RegisterVariable (&nehx19);
	}
	Cvar_RegisterVariable (&cutscene); // for Nehahra but useful to other mods as well
}

#define REQFIELDS (sizeof(reqfields) / sizeof(prvm_required_field_t))

prvm_required_field_t reqfields[] =
{
	{ev_entity, "cursor_trace_ent"},
	{ev_entity, "drawonlytoclient"},
	{ev_entity, "exteriormodeltoclient"},
	{ev_entity, "nodrawtoclient"},
	{ev_entity, "tag_entity"},
	{ev_entity, "viewmodelforclient"},
	{ev_float, "alpha"},
	{ev_float, "ammo_cells1"},
	{ev_float, "ammo_lava_nails"},
	{ev_float, "ammo_multi_rockets"},
	{ev_float, "ammo_nails1"},
	{ev_float, "ammo_plasma"},
	{ev_float, "ammo_rockets1"},
	{ev_float, "ammo_shells1"},
	{ev_float, "button3"},
	{ev_float, "button4"},
	{ev_float, "button5"},
	{ev_float, "button6"},
	{ev_float, "button7"},
	{ev_float, "button8"},
	{ev_float, "button9"},
	{ev_float, "button10"},
	{ev_float, "button11"},
	{ev_float, "button12"},
	{ev_float, "button13"},
	{ev_float, "button14"},
	{ev_float, "button15"},
	{ev_float, "button16"},
	{ev_float, "buttonchat"},
	{ev_float, "buttonuse"},
	{ev_float, "clientcolors"},
	{ev_float, "cursor_active"},
	{ev_float, "disableclientprediction"},
	{ev_float, "fullbright"},
	{ev_float, "glow_color"},
	{ev_float, "glow_size"},
	{ev_float, "glow_trail"},
	{ev_float, "gravity"},
	{ev_float, "idealpitch"},
	{ev_float, "items2"},
	{ev_float, "light_lev"},
	{ev_float, "pflags"},
	{ev_float, "ping"},
	{ev_float, "pitch_speed"},
	{ev_float, "pmodel"},
	{ev_float, "renderamt"}, // HalfLife support
	{ev_float, "rendermode"}, // HalfLife support
	{ev_float, "scale"},
	{ev_float, "style"},
	{ev_float, "tag_index"},
	{ev_float, "Version"},
	{ev_float, "viewzoom"},
	{ev_vector, "color"},
	{ev_vector, "colormod"},
	{ev_vector, "cursor_screen"},
	{ev_vector, "cursor_trace_endpos"},
	{ev_vector, "cursor_trace_start"},
	{ev_vector, "movement"},
	{ev_vector, "punchvector"},
	{ev_string, "playermodel"},
	{ev_string, "playerskin"},
	{ev_function, "SendEntity"},
	{ev_function, "customizeentityforclient"},
	// DRESK - Support for Entity Contents Transition Event
	{ev_function, "contentstransition"},
};

void SV_VM_Setup(void)
{
	extern cvar_t csqc_progname;	//[515]: csqc crc check and right csprogs name according to progs.dat
	extern cvar_t csqc_progcrc;
	extern cvar_t csqc_progsize;
	size_t csprogsdatasize;
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );

	// allocate the mempools
	// TODO: move the magic numbers/constants into #defines [9/13/2006 Black]
	prog->progs_mempool = Mem_AllocPool("Server Progs", 0, NULL);
	prog->builtins = vm_sv_builtins;
	prog->numbuiltins = vm_sv_numbuiltins;
	prog->headercrc = PROGHEADER_CRC;
	prog->max_edicts = 512;
	prog->limit_edicts = MAX_EDICTS;
	prog->reserved_edicts = svs.maxclients;
	prog->edictprivate_size = sizeof(edict_engineprivate_t);
	prog->name = "server";
	prog->extensionstring = vm_sv_extensions;
	prog->loadintoworld = true;

	prog->begin_increase_edicts = SV_VM_CB_BeginIncreaseEdicts;
	prog->end_increase_edicts = SV_VM_CB_EndIncreaseEdicts;
	prog->init_edict = SV_VM_CB_InitEdict;
	prog->free_edict = SV_VM_CB_FreeEdict;
	prog->count_edicts = SV_VM_CB_CountEdicts;
	prog->load_edict = SV_VM_CB_LoadEdict;
	prog->init_cmd = VM_SV_Cmd_Init;
	prog->reset_cmd = VM_SV_Cmd_Reset;
	prog->error_cmd = Host_Error;

	// TODO: add a requiredfuncs list (ask LH if this is necessary at all)
	PRVM_LoadProgs( sv_progs.string, 0, NULL, REQFIELDS, reqfields, 0, NULL );

	// some mods compiled with scrambling compilers lack certain critical
	// global names and field names such as "self" and "time" and "nextthink"
	// so we have to set these offsets manually, matching the entvars_t
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, angles);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, chain);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, classname);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, frame);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, groundentity);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ideal_yaw);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, nextthink);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, think);
	PRVM_ED_FindFieldOffset_FromStruct(entvars_t, yaw_speed);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, self);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, time);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_forward);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_right);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_up);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_allsolid);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_startsolid);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_fraction);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_inwater);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_inopen);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_endpos);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_plane_normal);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_plane_dist);
	PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_ent);
	// OP_STATE is always supported on server (due to entvars_t)
	prog->flag |= PRVM_OP_STATE;

	VM_CustomStats_Clear();//[515]: csqc
	EntityFrameCSQC_ClearVersions();//[515]: csqc

	PRVM_End;

	// see if there is a csprogs.dat installed, and if so, set the csqc_progcrc accordingly, this will be sent to connecting clients to tell them to only load a matching csprogs.dat file
	sv.csqc_progname[0] = 0;
	sv.csqc_progcrc = FS_CRCFile(csqc_progname.string, &csprogsdatasize);
	sv.csqc_progsize = csprogsdatasize;
	if (sv.csqc_progsize > 0)
	{
		strlcpy(sv.csqc_progname, csqc_progname.string, sizeof(sv.csqc_progname));
		Con_DPrintf("server detected csqc progs file \"%s\" with size %i and crc %i\n", sv.csqc_progname, sv.csqc_progsize, sv.csqc_progcrc);
	}
}

void SV_VM_Begin(void)
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_SERVERPROG );

	prog->globals.server->time = (float) sv.time;
}

void SV_VM_End(void)
{
	PRVM_End;
}
