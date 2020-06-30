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

#include "quakedef.h"
#include "sv_demo.h"
#include "image.h"

#include "prvm_cmds.h"
#include "utf8lib.h"

extern cvar_t sv_adminnick;
extern cvar_t sv_status_privacy;
extern cvar_t sv_status_show_qcstatus;
extern cvar_t sv_namechangetimer;
cvar_t cl_team = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "team", "none", "QW team (4 character limit, example: blue)"};
cvar_t cl_skin = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "skin", "", "QW player skin name (example: base)"};
cvar_t cl_playermodel = {CVAR_CLIENT | CVAR_SERVER | CVAR_USERINFO | CVAR_SAVE, "playermodel", "", "current player model in Nexuiz/Xonotic"};
cvar_t cl_playerskin = {CVAR_CLIENT | CVAR_SERVER | CVAR_USERINFO | CVAR_SAVE, "playerskin", "", "current player skin in Nexuiz/Xonotic"};
cvar_t cl_noaim = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "noaim", "1", "QW option to disable vertical autoaim"};
cvar_t cl_pmodel = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "pmodel", "0", "current player model number in nehahra"};
cvar_t r_fixtrans_auto = {CVAR_CLIENT, "r_fixtrans_auto", "0", "automatically fixtrans textures (when set to 2, it also saves the fixed versions to a fixtrans directory)"};

//============================================================================

/*
======================
CL_Playermodel_f
======================
*/
// the old cl_playermodel in cl_main has been renamed to __cl_playermodel
static void CL_Playermodel_f(cmd_state_t *cmd)
{
	prvm_prog_t *prog = SVVM_prog;
	int i, j;
	char newPath[sizeof(host_client->playermodel)];

	if (Cmd_Argc (cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"playermodel\" is \"%s\"\n", cl_playermodel.string);
		}
		return;
	}

	if (Cmd_Argc (cmd) == 2)
		strlcpy (newPath, Cmd_Argv(cmd, 1), sizeof (newPath));
	else
		strlcpy (newPath, Cmd_Args(cmd), sizeof (newPath));

	for (i = 0, j = 0;newPath[i];i++)
		if (newPath[i] != '\r' && newPath[i] != '\n')
			newPath[j++] = newPath[i];
	newPath[j] = 0;

	if (cmd->source == src_command)
	{
		Cvar_Set (&cvars_all, "_cl_playermodel", newPath);
		return;
	}

	/*
	if (host.realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = host.realtime + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playermodel, newPath, sizeof (host_client->playermodel));
	PRVM_serveredictstring(host_client->edict, playermodel) = PRVM_SetEngineString(prog, host_client->playermodel);
	if (strcmp(host_client->old_model, host_client->playermodel))
	{
		strlcpy(host_client->old_model, host_client->playermodel, sizeof(host_client->old_model));
		/*// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatepmodel);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->playermodel);*/
	}
}

/*
======================
CL_Playerskin_f
======================
*/
static void CL_Playerskin_f(cmd_state_t *cmd)
{
	prvm_prog_t *prog = SVVM_prog;
	int i, j;
	char newPath[sizeof(host_client->playerskin)];

	if (Cmd_Argc (cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"playerskin\" is \"%s\"\n", cl_playerskin.string);
		}
		return;
	}

	if (Cmd_Argc (cmd) == 2)
		strlcpy (newPath, Cmd_Argv(cmd, 1), sizeof (newPath));
	else
		strlcpy (newPath, Cmd_Args(cmd), sizeof (newPath));

	for (i = 0, j = 0;newPath[i];i++)
		if (newPath[i] != '\r' && newPath[i] != '\n')
			newPath[j++] = newPath[i];
	newPath[j] = 0;

	if (cmd->source == src_command)
	{
		Cvar_Set (&cvars_all, "_cl_playerskin", newPath);
		return;
	}

	/*
	if (host.realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = host.realtime + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playerskin, newPath, sizeof (host_client->playerskin));
	PRVM_serveredictstring(host_client->edict, playerskin) = PRVM_SetEngineString(prog, host_client->playerskin);
	if (strcmp(host_client->old_skin, host_client->playerskin))
	{
		//if (host_client->begun)
		//	SV_BroadcastPrintf("%s changed skin to %s\n", host_client->name, host_client->playerskin);
		strlcpy(host_client->old_skin, host_client->playerskin, sizeof(host_client->old_skin));
		/*// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatepskin);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->playerskin);*/
	}
}

cvar_t cl_rate = {CVAR_CLIENT | CVAR_SAVE | CVAR_USERINFO, "rate", "20000", "change your connection speed"};
cvar_t cl_rate_burstsize = {CVAR_CLIENT | CVAR_SAVE | CVAR_USERINFO, "rate_burstsize", "1024", "internal storage cvar for current rate control burst size (changed by rate_burstsize command)"};

/*
======================
CL_PModel_f
LadyHavoc: only supported for Nehahra, I personally think this is dumb, but Mindcrime won't listen.
LadyHavoc: correction, Mindcrime will be removing pmodel in the future, but it's still stuck here for compatibility.
======================
*/
static void CL_PModel_f(cmd_state_t *cmd)
{
	prvm_prog_t *prog = SVVM_prog;
	int i;

	if (Cmd_Argc (cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"pmodel\" is \"%s\"\n", cl_pmodel.string);
		}
		return;
	}
	i = atoi(Cmd_Argv(cmd, 1));

	if (cmd->source == src_command)
	{
		if (cl_pmodel.integer == i)
			return;
		Cvar_SetValue (&cvars_all, "_cl_pmodel", i);
		if (cls.state == ca_connected)
			CL_ForwardToServer_f(cmd);
		return;
	}

	PRVM_serveredictfloat(host_client->edict, pmodel) = i;
}

//===========================================================================

//===========================================================================

static void CL_SendCvar_f(cmd_state_t *cmd)
{
	int		i;
	cvar_t	*c;
	const char *cvarname;
	client_t *old;
	char vabuf[1024];

	if(Cmd_Argc(cmd) != 2)
		return;
	cvarname = Cmd_Argv(cmd, 1);
	if (cls.state == ca_connected)
	{
		c = Cvar_FindVar(&cvars_all, cvarname, CVAR_CLIENT | CVAR_SERVER);
		// LadyHavoc: if there is no such cvar or if it is private, send a
		// reply indicating that it has no value
		if(!c || (c->flags & CVAR_PRIVATE))
			CL_ForwardToServer(va(vabuf, sizeof(vabuf), "sentcvar %s", cvarname));
		else
			CL_ForwardToServer(va(vabuf, sizeof(vabuf), "sentcvar %s \"%s\"", c->name, c->string));
		return;
	}
	if(!sv.active)// || !PRVM_serverfunction(SV_ParseClientCommand))
		return;

	old = host_client;
	if (cls.state != ca_dedicated)
		i = 1;
	else
		i = 0;
	for(;i<svs.maxclients;i++)
		if(svs.clients[i].active && svs.clients[i].netconnection)
		{
			host_client = &svs.clients[i];
			SV_ClientCommands("sendcvar %s\n", cvarname);
		}
	host_client = old;
}

//=============================================================================

// QuakeWorld commands

/*
==================
CL_FullServerinfo_f

Sent by server when serverinfo changes
==================
*/
// TODO: shouldn't this be a cvar instead?
static void CL_FullServerinfo_f(cmd_state_t *cmd) // credit: taken from QuakeWorld
{
	char temp[512];
	if (Cmd_Argc(cmd) != 2)
	{
		Con_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	strlcpy (cl.qw_serverinfo, Cmd_Argv(cmd, 1), sizeof(cl.qw_serverinfo));
	InfoString_GetValue(cl.qw_serverinfo, "teamplay", temp, sizeof(temp));
	cl.qw_teamplay = atoi(temp);
}

/*
==================
CL_FullInfo_f

Allow clients to change userinfo
==================
Casey was here :)
*/
static void CL_FullInfo_f(cmd_state_t *cmd) // credit: taken from QuakeWorld
{
	char key[512];
	char value[512];
	const char *s;

	if (Cmd_Argc(cmd) != 2)
	{
		Con_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv(cmd, 1);
	if (*s == '\\')
		s++;
	while (*s)
	{
		size_t len = strcspn(s, "\\");
		if (len >= sizeof(key)) {
			len = sizeof(key) - 1;
		}
		strlcpy(key, s, len + 1);
		s += len;
		if (!*s)
		{
			Con_Printf ("MISSING VALUE\n");
			return;
		}
		++s; // Skip over backslash.

		len = strcspn(s, "\\");
		if (len >= sizeof(value)) {
			len = sizeof(value) - 1;
		}
		strlcpy(value, s, len + 1);

		CL_SetInfo(key, value, false, false, false, false);

		s += len;
		if (!*s)
		{
			break;
		}
		++s; // Skip over backslash.
	}
}

/*
==================
CL_SetInfo_f

Allow clients to change userinfo
==================
*/
static void CL_SetInfo_f(cmd_state_t *cmd) // credit: taken from QuakeWorld
{
	if (Cmd_Argc(cmd) == 1)
	{
		InfoString_Print(cls.userinfo);
		return;
	}
	if (Cmd_Argc(cmd) != 3)
	{
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	CL_SetInfo(Cmd_Argv(cmd, 1), Cmd_Argv(cmd, 2), true, false, false, false);
}

static void CL_PingPLReport_f(cmd_state_t *cmd)
{
	char *errbyte;
	int i;
	int l = Cmd_Argc(cmd);
	if (l > cl.maxclients)
		l = cl.maxclients;
	for (i = 0;i < l;i++)
	{
		cl.scores[i].qw_ping = atoi(Cmd_Argv(cmd, 1+i*2));
		cl.scores[i].qw_packetloss = strtol(Cmd_Argv(cmd, 1+i*2+1), &errbyte, 0);
		if(errbyte && *errbyte == ',')
			cl.scores[i].qw_movementloss = atoi(errbyte + 1);
		else
			cl.scores[i].qw_movementloss = 0;
	}
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	dpsnprintf(cls.userinfo, sizeof(cls.userinfo), "\\name\\player\\team\\none\\topcolor\\0\\bottomcolor\\0\\rate\\10000\\msg\\1\\noaim\\1\\*ver\\dp");

	Cvar_RegisterVariable(&cl_name);
	Cvar_RegisterAlias(&cl_name, "_cl_name");
	Cvar_RegisterVariable(&cl_rate);
	Cvar_RegisterAlias(&cl_rate, "_cl_rate");
	Cvar_RegisterVariable(&cl_rate_burstsize);
	Cvar_RegisterAlias(&cl_rate_burstsize, "_cl_rate_burstsize");
	Cvar_RegisterVariable(&cl_pmodel);
	Cvar_RegisterAlias(&cl_pmodel, "_cl_pmodel");
	Cvar_RegisterVariable(&cl_playermodel);
	Cvar_RegisterAlias(&cl_playermodel, "_cl_playermodel");
	Cvar_RegisterVariable(&cl_playerskin);
	Cvar_RegisterAlias(&cl_playerskin, "_cl_playerskin");
	Cvar_RegisterVariable(&rcon_password);
	Cvar_RegisterVariable(&r_fixtrans_auto);
	Cvar_RegisterVariable(&cl_team);
	Cvar_RegisterVariable(&cl_skin);
	Cvar_RegisterVariable(&cl_noaim);

	Cmd_AddCommand(CMD_USERINFO, "pmodel", CL_PModel_f, "(Nehahra-only) change your player model choice");
	Cmd_AddCommand(CMD_USERINFO, "playermodel", CL_Playermodel_f, "change your player model");
	Cmd_AddCommand(CMD_USERINFO, "playerskin", CL_Playerskin_f, "change your player skin number");

	Cmd_AddCommand(CMD_CLIENT, "sendcvar", CL_SendCvar_f, "sends the value of a cvar to the server as a sentcvar command, for use by QuakeC");
	Cmd_AddCommand(CMD_CLIENT, "fullinfo", CL_FullInfo_f, "allows client to modify their userinfo");
	Cmd_AddCommand(CMD_CLIENT, "setinfo", CL_SetInfo_f, "modifies your userinfo");
	Cmd_AddCommand(CMD_CLIENT, "fixtrans", Image_FixTransparentPixels_f, "change alpha-zero pixels in an image file to sensible values, and write out a new TGA (warning: SLOW)");

	// commands that are only sent by server to client for execution
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "pingplreport", CL_PingPLReport_f, "command sent by server containing client ping and packet loss values for scoreboard, triggered by pings command from client (not used by QW servers)");
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "fullserverinfo", CL_FullServerinfo_f, "internal use only, sent by server to client to update client's local copy of serverinfo string");
}

void Host_NoOperation_f(cmd_state_t *cmd)
{
}
