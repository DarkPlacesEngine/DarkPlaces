/*
Copyright (C) 2002 Mathieu Olivier

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
// net_master.h

#ifndef NET_MASTER_H
#define NET_MASTER_H

#define MASTER_PORT 27950

char* Master_BuildGetServers (void);
char* Master_BuildHeartbeat (void);
int Master_HandleMessage (void);
void Master_Init (void);
void Master_ParseServerList (net_landriver_t* dfunc);

#endif
