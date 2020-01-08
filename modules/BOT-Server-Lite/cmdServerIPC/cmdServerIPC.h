/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     cmdServerIPC.h

  File Description:

     This file contains the definitions and declarations of constants,
     structures, and functions used in IPC tool to communicate with BOT server.

  Version:

     2.0, 20191227

  Abstract:

    GUI of BOT system makes use of this IPC tool to communicate with BOT server.
    The communications include notifying BOT server of configuration changes.

  Authors:

     Chun Yu Lai   , chunyu1202@gmail.com
*/

#ifndef CMD_SERVER_IPC
#define CMD_SERVER_IPC

#include "getopt.h"

#include <winsock2.h>
#include <windows.h>
#pragma comment(lib,"WS2_32.lib")
#include <WS2tcpip.h>

#include "Common.h"

/* IP address of local host. This IPC tool is limited to communicate with 
BOT server installed on the same machine. */
#define LOCAL_SERVER_IP "127.0.0.1"

/* Readable sentence to help users of IPC tool specify IPC commands. */
const char * const IPCCommand_String[] = {

    "cmd_none",

    "cmd_reload_geo_fence_setting",

    "cmd_reload_monitor_setting",

    "cmd_max",
};

/* Readable sentence to help users of IPC tool specify arguments of 
geofence settings. */
const char * const ReloadGeoFenceSetting_String[] = {
    
    "geofence_none",

    "geofence_all",

    "geofence_list",

    "geofence_object",

    "geofence_max"
};

/* Readable sentence to help users of IPC tool specify the area scope to 
be applied with IPC command. */
const char * const AreaScope_String[] = {
    
    "area_none",

    "area_all",

    "area_one",

    "area_max"
};

#endif