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

extern cvar_t sv_adminnick;
extern cvar_t sv_status_privacy;
extern cvar_t sv_status_show_qcstatus;
extern cvar_t sv_namechangetimer;
cvar_t rcon_password = {CVAR_CLIENT | CVAR_SERVER | CVAR_PRIVATE, "rcon_password", "", "password to authenticate rcon commands; NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password; may be set to a string of the form user1:pass1 user2:pass2 user3:pass3 to allow multiple user accounts - the client then has to specify ONE of these combinations"};
cvar_t rcon_secure = {CVAR_CLIENT | CVAR_SERVER, "rcon_secure", "0", "force secure rcon authentication (1 = time based, 2 = challenge based); NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password"};
cvar_t rcon_secure_challengetimeout = {CVAR_CLIENT, "rcon_secure_challengetimeout", "5", "challenge-based secure rcon: time out requests if no challenge came within this time interval"};
cvar_t rcon_address = {CVAR_CLIENT, "rcon_address", "", "server address to send rcon commands to (when not connected to a server)"};
cvar_t cl_name = {CVAR_CLIENT | CVAR_SAVE | CVAR_USERINFO, "name", "player", "change your player name"};
cvar_t cl_topcolor = {CVAR_CLIENT | CVAR_SAVE | CVAR_USERINFO, "topcolor", "0", "change the color of your shirt"};
cvar_t cl_bottomcolor = {CVAR_CLIENT | CVAR_SAVE | CVAR_USERINFO, "bottomcolor", "0", "change the color of your pants"};
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

/*
==================
CL_Color_f
==================
*/
cvar_t cl_color = {CVAR_READONLY | CVAR_CLIENT | CVAR_SAVE, "_cl_color", "0", "internal storage cvar for current player colors (changed by color command)"};

// Ignore the callbacks so this two-to-three way synchronization doesn't cause an infinite loop.
static void CL_Color_c(cvar_t *var)
{
	char vabuf[1024];
	
	Cvar_Set_NoCallback(&cl_topcolor, va(vabuf, sizeof(vabuf), "%i", ((var->integer >> 4) & 15)));
	Cvar_Set_NoCallback(&cl_bottomcolor, va(vabuf, sizeof(vabuf), "%i", (var->integer & 15)));
}

static void CL_Topcolor_c(cvar_t *var)
{
	char vabuf[1024];
	
	Cvar_Set_NoCallback(&cl_color, va(vabuf, sizeof(vabuf), "%i", var->integer*16 + cl_bottomcolor.integer));
}

static void CL_Bottomcolor_c(cvar_t *var)
{
	char vabuf[1024];

	Cvar_Set_NoCallback(&cl_color, va(vabuf, sizeof(vabuf), "%i", cl_topcolor.integer*16 + var->integer));
}

static void CL_Color_f(cmd_state_t *cmd)
{
	int top, bottom;

	if (Cmd_Argc(cmd) == 1)
	{
		if (cmd->source == src_command)
		{
			Con_Printf("\"color\" is \"%i %i\"\n", cl_topcolor.integer, cl_bottomcolor.integer);
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
	/*
	 * This is just a convenient way to change topcolor and bottomcolor
	 * We can't change cl_color from here directly because topcolor and
	 * bottomcolor may be changed separately and do not call this function.
	 * So it has to be changed when the userinfo strings are updated, which
	 * happens twice here. Perhaps find a cleaner way?
	 */

	top = top >= 0 ? top : cl_topcolor.integer;
	bottom = bottom >= 0 ? bottom : cl_bottomcolor.integer;

	top &= 15;
	bottom &= 15;

	// LadyHavoc: allowing skin colormaps 14 and 15 by commenting this out
	//if (top > 13)
	//	top = 13;
	//if (bottom > 13)
	//	bottom = 13;

	if (cmd->source == src_command)
	{
		Cvar_SetValueQuick(&cl_topcolor, top);
		Cvar_SetValueQuick(&cl_bottomcolor, bottom);
		return;
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

static void CL_RCon_ClearPassword_c(cvar_t *var)
{
	// whenever rcon_secure is changed to 0, clear rcon_password for
	// security reasons (prevents a send-rcon-password-as-plaintext
	// attack based on NQ protocol session takeover and svc_stufftext)
	if(var->integer <= 0)
		Cvar_SetQuick(&rcon_password, "");
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
	Cvar_RegisterAlias(&cl_name, "_cl_name");
	Cvar_RegisterVariable(&cl_color);
	Cvar_RegisterCallback(&cl_color, CL_Color_c);
	Cvar_RegisterVariable(&cl_topcolor);
	Cvar_RegisterCallback(&cl_topcolor, CL_Topcolor_c);
	Cvar_RegisterVariable(&cl_bottomcolor);
	Cvar_RegisterCallback(&cl_bottomcolor, CL_Bottomcolor_c);
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
	Cvar_RegisterVariable(&rcon_address);
	Cvar_RegisterVariable(&rcon_secure);
	Cvar_RegisterCallback(&rcon_secure, CL_RCon_ClearPassword_c);
	Cvar_RegisterVariable(&rcon_secure_challengetimeout);
	Cvar_RegisterVariable(&r_fixtrans_auto);
	Cvar_RegisterVariable(&cl_team);
	Cvar_RegisterVariable(&cl_skin);
	Cvar_RegisterVariable(&cl_noaim);

	Cmd_AddCommand(CMD_CLIENT, "color", CL_Color_f, "change your player shirt and pants colors");
	Cmd_AddCommand(CMD_USERINFO, "pmodel", CL_PModel_f, "(Nehahra-only) change your player model choice");
	Cmd_AddCommand(CMD_USERINFO, "playermodel", CL_Playermodel_f, "change your player model");
	Cmd_AddCommand(CMD_USERINFO, "playerskin", CL_Playerskin_f, "change your player skin number");

	Cmd_AddCommand(CMD_CLIENT, "sendcvar", CL_SendCvar_f, "sends the value of a cvar to the server as a sentcvar command, for use by QuakeC");
	Cmd_AddCommand(CMD_CLIENT, "rcon", CL_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); note: if rcon_secure is set, client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand(CMD_CLIENT, "srcon", CL_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); this always works as if rcon_secure is set; note: client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand(CMD_CLIENT, "pqrcon", CL_PQRcon_f, "sends a command to a proquake server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's)");
	Cmd_AddCommand(CMD_CLIENT, "fullinfo", CL_FullInfo_f, "allows client to modify their userinfo");
	Cmd_AddCommand(CMD_CLIENT, "setinfo", CL_SetInfo_f, "modifies your userinfo");
	Cmd_AddCommand(CMD_CLIENT, "packet", CL_Packet_f, "send a packet to the specified address:port containing a text string");
	Cmd_AddCommand(CMD_CLIENT, "fixtrans", Image_FixTransparentPixels_f, "change alpha-zero pixels in an image file to sensible values, and write out a new TGA (warning: SLOW)");

	// commands that are only sent by server to client for execution
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "pingplreport", CL_PingPLReport_f, "command sent by server containing client ping and packet loss values for scoreboard, triggered by pings command from client (not used by QW servers)");
	Cmd_AddCommand(CMD_CLIENT_FROM_SERVER, "fullserverinfo", CL_FullServerinfo_f, "internal use only, sent by server to client to update client's local copy of serverinfo string");
}

void Host_NoOperation_f(cmd_state_t *cmd)
{
}
