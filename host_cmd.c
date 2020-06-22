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

// for secure rcon authentication
#include "hmac.h"
#include "mdfour.h"
#include <time.h>

int current_skill;
extern cvar_t sv_adminnick;
extern cvar_t sv_status_privacy;
extern cvar_t sv_status_show_qcstatus;
extern cvar_t sv_namechangetimer;
cvar_t rcon_password = {CVAR_CLIENT | CVAR_SERVER | CVAR_PRIVATE, "rcon_password", "", "password to authenticate rcon commands; NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password; may be set to a string of the form user1:pass1 user2:pass2 user3:pass3 to allow multiple user accounts - the client then has to specify ONE of these combinations"};
cvar_t rcon_secure = {CVAR_CLIENT | CVAR_SERVER | CVAR_NQUSERINFOHACK, "rcon_secure", "0", "force secure rcon authentication (1 = time based, 2 = challenge based); NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password"};
cvar_t rcon_secure_challengetimeout = {CVAR_CLIENT, "rcon_secure_challengetimeout", "5", "challenge-based secure rcon: time out requests if no challenge came within this time interval"};
cvar_t rcon_address = {CVAR_CLIENT, "rcon_address", "", "server address to send rcon commands to (when not connected to a server)"};
cvar_t team = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "team", "none", "QW team (4 character limit, example: blue)"};
cvar_t skin = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "skin", "", "QW player skin name (example: base)"};
cvar_t noaim = {CVAR_CLIENT | CVAR_USERINFO | CVAR_SAVE, "noaim", "1", "QW option to disable vertical autoaim"};
cvar_t r_fixtrans_auto = {CVAR_CLIENT, "r_fixtrans_auto", "0", "automatically fixtrans textures (when set to 2, it also saves the fixed versions to a fixtrans directory)"};

/*
==================
CL_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void CL_Reconnect_f(cmd_state_t *cmd)
{
	char temp[128];
	// if not connected, reconnect to the most recent server
	if (!cls.netcon)
	{
		// if we have connected to a server recently, the userinfo
		// will still contain its IP address, so get the address...
		InfoString_GetValue(cls.userinfo, "*ip", temp, sizeof(temp));
		if (temp[0])
			CL_EstablishConnection(temp, -1);
		else
			Con_Printf("Reconnect to what server?  (you have not connected to a server yet)\n");
		return;
	}
	// if connected, do something based on protocol
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
	{
		// quakeworld can just re-login
		if (cls.qw_downloadmemory)  // don't change when downloading
			return;

		S_StopAllSounds();

		if (cls.state == ca_connected)
		{
			Con_Printf("Server is changing level...\n");
			MSG_WriteChar(&cls.netcon->message, qw_clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, "new");
		}
	}
	else
	{
		// netquake uses reconnect on level changes (silly)
		if (Cmd_Argc(cmd) != 1)
		{
			Con_Print("reconnect : wait for signon messages again\n");
			return;
		}
		if (!cls.signon)
		{
			Con_Print("reconnect: no signon, ignoring reconnect\n");
			return;
		}
		cls.signon = 0;		// need new connection messages
	}
}

/*
=====================
CL_Connect_f

User command to connect to server
=====================
*/
static void CL_Connect_f(cmd_state_t *cmd)
{
	if (Cmd_Argc(cmd) < 2)
	{
		Con_Print("connect <serveraddress> [<key> <value> ...]: connect to a multiplayer game\n");
		return;
	}
	// clear the rcon password, to prevent vulnerability by stuffcmd-ing a connect command
	if(rcon_secure.integer <= 0)
		Cvar_SetQuick(&rcon_password, "");
	CL_EstablishConnection(Cmd_Argv(cmd, 1), 2);
}


//============================================================================

/*
======================
CL_Name_f
======================
*/
cvar_t cl_name = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_name", "player", "internal storage cvar for current player name (changed by name command)"};
static void CL_Name_f(cmd_state_t *cmd)
{
	prvm_prog_t *prog = SVVM_prog;
	int i, j;
	qboolean valid_colors;
	const char *newNameSource;
	char newName[sizeof(host_client->name)];

	if (Cmd_Argc (cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("name: %s\n", cl_name.string);
		}
		return;
	}

	if (Cmd_Argc (cmd) == 2)
		newNameSource = Cmd_Argv(cmd, 1);
	else
		newNameSource = Cmd_Args(cmd);

	strlcpy(newName, newNameSource, sizeof(newName));

	if (cmd->source == src_command)
	{
		Cvar_Set (&cvars_all, "_cl_name", newName);
		if (strlen(newNameSource) >= sizeof(newName)) // overflowed
		{
			Con_Printf("Your name is longer than %i chars! It has been truncated.\n", (int) (sizeof(newName) - 1));
			Con_Printf("name: %s\n", cl_name.string);
		}
		return;
	}

	if (host.realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change name more than once every %.1f seconds!\n", max(0.0f, sv_namechangetimer.value));
		return;
	}

	host_client->nametime = host.realtime + max(0.0f, sv_namechangetimer.value);

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->name, newName, sizeof (host_client->name));

	for (i = 0, j = 0;host_client->name[i];i++)
		if (host_client->name[i] != '\r' && host_client->name[i] != '\n')
			host_client->name[j++] = host_client->name[i];
	host_client->name[j] = 0;

	if(host_client->name[0] == 1 || host_client->name[0] == 2)
	// may interfere with chat area, and will needlessly beep; so let's add a ^7
	{
		memmove(host_client->name + 2, host_client->name, sizeof(host_client->name) - 2);
		host_client->name[sizeof(host_client->name) - 1] = 0;
		host_client->name[0] = STRING_COLOR_TAG;
		host_client->name[1] = '0' + STRING_COLOR_DEFAULT;
	}

	u8_COM_StringLengthNoColors(host_client->name, 0, &valid_colors);
	if(!valid_colors) // NOTE: this also proves the string is not empty, as "" is a valid colored string
	{
		size_t l;
		l = strlen(host_client->name);
		if(l < sizeof(host_client->name) - 1)
		{
			// duplicate the color tag to escape it
			host_client->name[i] = STRING_COLOR_TAG;
			host_client->name[i+1] = 0;
			//Con_DPrintf("abuse detected, adding another trailing color tag\n");
		}
		else
		{
			// remove the last character to fix the color code
			host_client->name[l-1] = 0;
			//Con_DPrintf("abuse detected, removing a trailing color tag\n");
		}
	}

	// find the last color tag offset and decide if we need to add a reset tag
	for (i = 0, j = -1;host_client->name[i];i++)
	{
		if (host_client->name[i] == STRING_COLOR_TAG)
		{
			if (host_client->name[i+1] >= '0' && host_client->name[i+1] <= '9')
			{
				j = i;
				// if this happens to be a reset  tag then we don't need one
				if (host_client->name[i+1] == '0' + STRING_COLOR_DEFAULT)
					j = -1;
				i++;
				continue;
			}
			if (host_client->name[i+1] == STRING_COLOR_RGB_TAG_CHAR && isxdigit(host_client->name[i+2]) && isxdigit(host_client->name[i+3]) && isxdigit(host_client->name[i+4]))
			{
				j = i;
				i += 4;
				continue;
			}
			if (host_client->name[i+1] == STRING_COLOR_TAG)
			{
				i++;
				continue;
			}
		}
	}
	// does not end in the default color string, so add it
	if (j >= 0 && strlen(host_client->name) < sizeof(host_client->name) - 2)
		memcpy(host_client->name + strlen(host_client->name), STRING_COLOR_DEFAULT_STR, strlen(STRING_COLOR_DEFAULT_STR) + 1);

	PRVM_serveredictstring(host_client->edict, netname) = PRVM_SetEngineString(prog, host_client->name);
	if (strcmp(host_client->old_name, host_client->name))
	{
		if (host_client->begun)
			SV_BroadcastPrintf("%s ^7changed name to %s\n", host_client->old_name, host_client->name);
		strlcpy(host_client->old_name, host_client->name, sizeof(host_client->old_name));
		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->name);
		SV_WriteNetnameIntoDemo(host_client);
	}
}

/*
======================
CL_Playermodel_f
======================
*/
cvar_t cl_playermodel = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_playermodel", "", "internal storage cvar for current player model in Nexuiz/Xonotic (changed by playermodel command)"};
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
cvar_t cl_playerskin = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_playerskin", "", "internal storage cvar for current player skin in Nexuiz/Xonotic (changed by playerskin command)"};
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

/*
==================
CL_Color_f
==================
*/
cvar_t cl_color = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_color", "0", "internal storage cvar for current player colors (changed by color command)"};
static void CL_Color(cmd_state_t *cmd, int changetop, int changebottom)
{
	prvm_prog_t *prog = SVVM_prog;
	int top, bottom, playercolor;

	// get top and bottom either from the provided values or the current values
	// (allows changing only top or bottom, or both at once)
	top = changetop >= 0 ? changetop : (cl_color.integer >> 4);
	bottom = changebottom >= 0 ? changebottom : cl_color.integer;

	top &= 15;
	bottom &= 15;
	// LadyHavoc: allowing skin colormaps 14 and 15 by commenting this out
	//if (top > 13)
	//	top = 13;
	//if (bottom > 13)
	//	bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd->source == src_command)
	{
		Cvar_SetValueQuick(&cl_color, playercolor);
		return;
	}

	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		return;

	if (host_client->edict && PRVM_serverfunction(SV_ChangeTeam))
	{
		Con_DPrint("Calling SV_ChangeTeam\n");
		prog->globals.fp[OFS_PARM0] = playercolor;
		PRVM_serverglobalfloat(time) = sv.time;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		prog->ExecuteProgram(prog, PRVM_serverfunction(SV_ChangeTeam), "QC function SV_ChangeTeam is missing");
	}
	else
	{
		if (host_client->edict)
		{
			PRVM_serveredictfloat(host_client->edict, clientcolors) = playercolor;
			PRVM_serveredictfloat(host_client->edict, team) = bottom + 1;
		}
		host_client->colors = playercolor;
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
			MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
		}
	}
}

static void CL_Color_f(cmd_state_t *cmd)
{
	int		top, bottom;

	if (Cmd_Argc(cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"color\" is \"%i %i\"\n", cl_color.integer >> 4, cl_color.integer & 15);
			Con_Print("color <0-15> [0-15]\n");
		}
		return;
	}

	if (Cmd_Argc(cmd) == 2)
		top = bottom = atoi(Cmd_Argv(cmd, 1));
	else
	{
		top = atoi(Cmd_Argv(cmd, 1));
		bottom = atoi(Cmd_Argv(cmd, 2));
	}
	CL_Color(cmd, top, bottom);
}

static void CL_TopColor_f(cmd_state_t *cmd)
{
	if (Cmd_Argc(cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"topcolor\" is \"%i\"\n", (cl_color.integer >> 4) & 15);
			Con_Print("topcolor <0-15>\n");
		}
		return;
	}

	CL_Color(cmd, atoi(Cmd_Argv(cmd, 1)), -1);
}

static void CL_BottomColor_f(cmd_state_t *cmd)
{
	if (Cmd_Argc(cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"bottomcolor\" is \"%i\"\n", cl_color.integer & 15);
			Con_Print("bottomcolor <0-15>\n");
		}
		return;
	}

	CL_Color(cmd, -1, atoi(Cmd_Argv(cmd, 1)));
}

cvar_t cl_rate = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_rate", "20000", "internal storage cvar for current rate (changed by rate command)"};
cvar_t cl_rate_burstsize = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_rate_burstsize", "1024", "internal storage cvar for current rate control burst size (changed by rate_burstsize command)"};
static void CL_Rate_f(cmd_state_t *cmd)
{
	int rate;

	if (Cmd_Argc(cmd) != 2)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"rate\" is \"%i\"\n", cl_rate.integer);
			Con_Print("rate <bytespersecond>\n");
		}
		return;
	}

	rate = atoi(Cmd_Argv(cmd, 1));

	if (cmd->source == src_command)
	{
		Cvar_SetValue (&cvars_all, "_cl_rate", max(NET_MINRATE, rate));
		return;
	}

	host_client->rate = rate;
}

static void CL_Rate_BurstSize_f(cmd_state_t *cmd)
{
	int rate_burstsize;

	if (Cmd_Argc(cmd) != 2)
	{
		Con_Printf("\"rate_burstsize\" is \"%i\"\n", cl_rate_burstsize.integer);
		Con_Print("rate_burstsize <bytes>\n");
		return;
	}

	rate_burstsize = atoi(Cmd_Argv(cmd, 1));

	if (cmd->source == src_command)
	{
		Cvar_SetValue (&cvars_all, "_cl_rate_burstsize", rate_burstsize);
		return;
	}

	host_client->rate_burstsize = rate_burstsize;
}

/*
======================
CL_PModel_f
LadyHavoc: only supported for Nehahra, I personally think this is dumb, but Mindcrime won't listen.
LadyHavoc: correction, Mindcrime will be removing pmodel in the future, but it's still stuck here for compatibility.
======================
*/
cvar_t cl_pmodel = {CVAR_CLIENT | CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_pmodel", "0", "internal storage cvar for current player model number in nehahra (changed by pmodel command)"};
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
			Cmd_ForwardToServer_f(cmd);
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
			Cmd_ForwardStringToServer(va(vabuf, sizeof(vabuf), "sentcvar %s", cvarname));
		else
			Cmd_ForwardStringToServer(va(vabuf, sizeof(vabuf), "sentcvar %s \"%s\"", c->name, c->string));
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

/*
=====================
CL_PQRcon_f

ProQuake rcon support
=====================
*/
static void CL_PQRcon_f(cmd_state_t *cmd)
{
	int n;
	const char *e;
	lhnetsocket_t *mysocket;

	if (Cmd_Argc(cmd) == 1)
	{
		Con_Printf("%s: Usage: %s command\n", Cmd_Argv(cmd, 0), Cmd_Argv(cmd, 0));
		return;
	}

	if (!rcon_password.string || !rcon_password.string[0] || rcon_secure.integer > 0)
	{
		Con_Printf ("You must set rcon_password before issuing an pqrcon command, and rcon_secure must be 0.\n");
		return;
	}

	e = strchr(rcon_password.string, ' ');
	n = e ? e-rcon_password.string : (int)strlen(rcon_password.string);

	if (cls.netcon)
		cls.rcon_address = cls.netcon->peeraddress;
	else
	{
		if (!rcon_address.string[0])
		{
			Con_Printf ("You must either be connected, or set the rcon_address cvar to issue rcon commands\n");
			return;
		}
		LHNETADDRESS_FromString(&cls.rcon_address, rcon_address.string, sv_netport.integer);
	}
	mysocket = NetConn_ChooseClientSocketForAddress(&cls.rcon_address);
	if (mysocket)
	{
		sizebuf_t buf;
		unsigned char bufdata[64];
		buf.data = bufdata;
		SZ_Clear(&buf);
		MSG_WriteLong(&buf, 0);
		MSG_WriteByte(&buf, CCREQ_RCON);
		SZ_Write(&buf, (const unsigned char*)rcon_password.string, n);
		MSG_WriteByte(&buf, 0); // terminate the (possibly partial) string
		MSG_WriteString(&buf, Cmd_Args(cmd));
		StoreBigLong(buf.data, NETFLAG_CTL | (buf.cursize & NETFLAG_LENGTH_MASK));
		NetConn_Write(mysocket, buf.data, buf.cursize, &cls.rcon_address);
		SZ_Clear(&buf);
	}
}

//=============================================================================

// QuakeWorld commands

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
static void CL_Rcon_f(cmd_state_t *cmd) // credit: taken from QuakeWorld
{
	int i, n;
	const char *e;
	lhnetsocket_t *mysocket;

	if (Cmd_Argc(cmd) == 1)
	{
		Con_Printf("%s: Usage: %s command\n", Cmd_Argv(cmd, 0), Cmd_Argv(cmd, 0));
		return;
	}

	if (!rcon_password.string || !rcon_password.string[0])
	{
		Con_Printf ("You must set rcon_password before issuing an rcon command.\n");
		return;
	}

	e = strchr(rcon_password.string, ' ');
	n = e ? e-rcon_password.string : (int)strlen(rcon_password.string);

	if (cls.netcon)
		cls.rcon_address = cls.netcon->peeraddress;
	else
	{
		if (!rcon_address.string[0])
		{
			Con_Printf ("You must either be connected, or set the rcon_address cvar to issue rcon commands\n");
			return;
		}
		LHNETADDRESS_FromString(&cls.rcon_address, rcon_address.string, sv_netport.integer);
	}
	mysocket = NetConn_ChooseClientSocketForAddress(&cls.rcon_address);
	if (mysocket && Cmd_Args(cmd)[0])
	{
		// simply put together the rcon packet and send it
		if(Cmd_Argv(cmd, 0)[0] == 's' || rcon_secure.integer > 1)
		{
			if(cls.rcon_commands[cls.rcon_ringpos][0])
			{
				char s[128];
				LHNETADDRESS_ToString(&cls.rcon_addresses[cls.rcon_ringpos], s, sizeof(s), true);
				Con_Printf("rcon to %s (for command %s) failed: too many buffered commands (possibly increase MAX_RCONS)\n", s, cls.rcon_commands[cls.rcon_ringpos]);
				cls.rcon_commands[cls.rcon_ringpos][0] = 0;
				--cls.rcon_trying;
			}
			for (i = 0;i < MAX_RCONS;i++)
				if(cls.rcon_commands[i][0])
					if (!LHNETADDRESS_Compare(&cls.rcon_address, &cls.rcon_addresses[i]))
						break;
			++cls.rcon_trying;
			if(i >= MAX_RCONS)
				NetConn_WriteString(mysocket, "\377\377\377\377getchallenge", &cls.rcon_address); // otherwise we'll request the challenge later
			strlcpy(cls.rcon_commands[cls.rcon_ringpos], Cmd_Args(cmd), sizeof(cls.rcon_commands[cls.rcon_ringpos]));
			cls.rcon_addresses[cls.rcon_ringpos] = cls.rcon_address;
			cls.rcon_timeout[cls.rcon_ringpos] = host.realtime + rcon_secure_challengetimeout.value;
			cls.rcon_ringpos = (cls.rcon_ringpos + 1) % MAX_RCONS;
		}
		else if(rcon_secure.integer > 0)
		{
			char buf[1500];
			char argbuf[1500];
			dpsnprintf(argbuf, sizeof(argbuf), "%ld.%06d %s", (long) time(NULL), (int) (rand() % 1000000), Cmd_Args(cmd));
			memcpy(buf, "\377\377\377\377srcon HMAC-MD4 TIME ", 24);
			if(HMAC_MDFOUR_16BYTES((unsigned char *) (buf + 24), (unsigned char *) argbuf, (int)strlen(argbuf), (unsigned char *) rcon_password.string, n))
			{
				buf[40] = ' ';
				strlcpy(buf + 41, argbuf, sizeof(buf) - 41);
				NetConn_Write(mysocket, buf, 41 + (int)strlen(buf + 41), &cls.rcon_address);
			}
		}
		else
		{
			char buf[1500];
			memcpy(buf, "\377\377\377\377", 4);
			dpsnprintf(buf+4, sizeof(buf)-4, "rcon %.*s %s",  n, rcon_password.string, Cmd_Args(cmd));
			NetConn_WriteString(mysocket, buf, &cls.rcon_address);
		}
	}
}

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

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
static void CL_Packet_f(cmd_state_t *cmd) // credit: taken from QuakeWorld
{
	char send[2048];
	int i, l;
	const char *in;
	char *out;
	lhnetaddress_t address;
	lhnetsocket_t *mysocket;

	if (Cmd_Argc(cmd) != 3)
	{
		Con_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!LHNETADDRESS_FromString (&address, Cmd_Argv(cmd, 1), sv_netport.integer))
	{
		Con_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv(cmd, 2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = -1;

	l = (int)strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (out >= send + sizeof(send) - 1)
			break;
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '0')
		{
			*out++ = '\0';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == 't')
		{
			*out++ = '\t';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == 'r')
		{
			*out++ = '\r';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '"')
		{
			*out++ = '\"';
			i++;
		}
		else
			*out++ = in[i];
	}

	mysocket = NetConn_ChooseClientSocketForAddress(&address);
	if (!mysocket)
		mysocket = NetConn_ChooseServerSocketForAddress(&address);
	if (mysocket)
		NetConn_Write(mysocket, send, out - send, &address);
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
	Cvar_RegisterVariable(&cl_color);
	Cvar_RegisterVariable(&cl_rate);
	Cvar_RegisterVariable(&cl_rate_burstsize);
	Cvar_RegisterVariable(&cl_pmodel);
	Cvar_RegisterVariable(&cl_playermodel);
	Cvar_RegisterVariable(&cl_playerskin);
	Cvar_RegisterVariable(&rcon_password);
	Cvar_RegisterVariable(&rcon_address);
	Cvar_RegisterVariable(&rcon_secure);
	Cvar_RegisterVariable(&rcon_secure_challengetimeout);
	Cvar_RegisterVariable(&r_fixtrans_auto);
	Cvar_RegisterVariable(&team);
	Cvar_RegisterVariable(&skin);
	Cvar_RegisterVariable(&noaim);

	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "name", CL_Name_f, "change your player name");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "color", CL_Color_f, "change your player shirt and pants colors");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "rate", CL_Rate_f, "change your network connection speed");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "rate_burstsize", CL_Rate_BurstSize_f, "change your network connection speed");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "pmodel", CL_PModel_f, "(Nehahra-only) change your player model choice");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "playermodel", CL_Playermodel_f, "change your player model");
	Cmd_AddCommand(CMD_CLIENT | CMD_SERVER_FROM_CLIENT, "playerskin", CL_Playerskin_f, "change your player skin number");

	Cmd_AddCommand(CMD_CLIENT, "connect", CL_Connect_f, "connect to a server by IP address or hostname");
	Cmd_AddCommand(CMD_CLIENT | CMD_CLIENT_FROM_SERVER, "reconnect", CL_Reconnect_f, "reconnect to the last server you were on, or resets a quakeworld connection (do not use if currently playing on a netquake server)");
	Cmd_AddCommand(CMD_CLIENT, "sendcvar", CL_SendCvar_f, "sends the value of a cvar to the server as a sentcvar command, for use by QuakeC");
	Cmd_AddCommand(CMD_CLIENT, "rcon", CL_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); note: if rcon_secure is set, client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand(CMD_CLIENT, "srcon", CL_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); this always works as if rcon_secure is set; note: client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand(CMD_CLIENT, "pqrcon", CL_PQRcon_f, "sends a command to a proquake server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's)");
	Cmd_AddCommand(CMD_CLIENT, "fullinfo", CL_FullInfo_f, "allows client to modify their userinfo");
	Cmd_AddCommand(CMD_CLIENT, "setinfo", CL_SetInfo_f, "modifies your userinfo");
	Cmd_AddCommand(CMD_CLIENT | CMD_CLIENT_FROM_SERVER, "packet", CL_Packet_f, "send a packet to the specified address:port containing a text string");
	Cmd_AddCommand(CMD_CLIENT | CMD_CLIENT_FROM_SERVER, "topcolor", CL_TopColor_f, "QW command to set top color without changing bottom color");
	Cmd_AddCommand(CMD_CLIENT, "bottomcolor", CL_BottomColor_f, "QW command to set bottom color without changing top color");
	Cmd_AddCommand(CMD_CLIENT, "fixtrans", Image_FixTransparentPixels_f, "change alpha-zero pixels in an image file to sensible values, and write out a new TGA (warning: SLOW)");

	// commands that are only sent by server to client for execution
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "pingplreport", CL_PingPLReport_f, "command sent by server containing client ping and packet loss values for scoreboard, triggered by pings command from client (not used by QW servers)");
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "fullserverinfo", CL_FullServerinfo_f, "internal use only, sent by server to client to update client's local copy of serverinfo string");
}

void Host_NoOperation_f(cmd_state_t *cmd)
{
}
