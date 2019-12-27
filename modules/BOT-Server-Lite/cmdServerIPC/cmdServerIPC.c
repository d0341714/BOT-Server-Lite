/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     cmdServerIPC.c

  File Description:

     This file contains the implementation of IPC tool to communicate with BOT 
     server. 

  Version:

     2.0, 20191227

  Abstract:

    This file contains the implementation of IPC tool to communicate with BOT
    server. The IPC tool first parsed the input arguments which users specify 
    and then sends command packets to BOT server via UDP protocol.

  Authors:

     Chun Yu Lai   , chunyu1202@gmail.com
*/

#include "cmdServerIPC.h"

void display_usage(){
    printf("\n");
    printf("The support commands are:\n");
    printf("\n");
    printf("cmdServerIPC -p [server_port] -c RELOAD_GEO_FENCE_SETTING " \
           "-r [geofence_setting] -f area_all\n");
    printf("cmdServerIPC -p [server_port] -c RELOAD_GEO_FENCE_SETTING " \
           "-r [geofence_setting] -f area_one -a [area_id]\n");
    printf("\n");
    printf("\n");
    printf("-p: specify the listening port of the destination server\n\n");
    printf("\n");
    printf("-c: specify the IPC command. The supported values are:\n");
    printf("    %s: reload geofence setting. Please specify option -r as well\n", 
           IPCCommand_String[1]);
    printf("\n");
    printf("-r: specify the settings to be loaded. The supported settings are:\n");
    printf("    %s: reload both geofence list and geofence objects\n", 
           ReloadGeoFenceSetting_String[1]);
    printf("    %s  : reload geofence list only\n", 
           ReloadGeoFenceSetting_String[2]);
    printf("    %s: reload geofence objects only\n", 
           ReloadGeoFenceSetting_String[3]);
    printf("\n");
    printf("-f: specify whether to reload settings under all covered areas. " \
           "The supported values are\n");
    printf("    %s: to reload setting for all covered areas\n", 
           AreaScope_String[1]);
    printf("    %s: to reload setting for single specified area_id by -a argument\n",
           AreaScope_String[2]);
    printf("\n");
    printf("-a: specify the area_id to reload settings\n");
    printf("\n");
}

int main(int argc, char **argv)
{

#ifdef _WIN32
    WSADATA wsaData;

    WORD sockVersion;
#endif

    int ch;
    int i;
    int server_port = -1;
    int area_id = -1;
    IPCCommand ipc_command = CMD_NONE;
    ReloadGeoFenceSetting geofence_setting = GEO_FENCE_NONE;
    AreaScope area_scope = AREA_NONE;

    int send_socket = 0;
    struct sockaddr_in si_send;
    char message_content[WIFI_MESSAGE_LENGTH];

    int verbose_mode = 0;
    
    while((ch = getopt(argc, argv, "p:c:r:f:a:vh")) != -1){
        switch(ch){
            case 'p':
                server_port = atoi(optarg);
                break;        
            case 'c':
                for(i = 0; i < CMD_MAX; i++){
                    if(strcmp(optarg, IPCCommand_String[i]) == 0){
                        ipc_command = (IPCCommand) i;
                        break;
                    }
                }      
                break;
            case 'r':
                geofence_setting = GEO_FENCE_NONE;
                for(i = 0 ; i < GEO_FENCE_MAX; i++){
                    if(strcmp(optarg, ReloadGeoFenceSetting_String[i]) == 0){
                        geofence_setting = (ReloadGeoFenceSetting)i;
                        break;
                    }
                }
                break;
            case 'f':
                area_scope = AREA_NONE;
                for(i = 0 ; i < AREA_MAX; i++){
                    if(strcmp(optarg, AreaScope_String[i]) == 0){
                        area_scope = (AreaScope)i;
                        break;
                    }
                }
                break;
            case 'a':
                area_id = atoi(optarg);
                break;
            case 'v':
                verbose_mode = 1;
                break;
            case 'h':
                display_usage();
                return 1;
            case '?':
                return -1;
            default:
                return -1;
        }
    }

    memset(message_content, 0, sizeof(message_content));

    if(server_port >= 0 &&  ipc_command == CMD_RELOAD_GEO_FENCE_SETTING){
        if(geofence_setting > GEO_FENCE_NONE){
            if(area_scope == AREA_ALL && area_id == -1){
                sprintf(message_content, "%d;%d;%s;%d;%d;%d", 
                        from_gui, 
                        setting_reload, 
                        BOT_SERVER_API_VERSION_LATEST, 
                        CMD_RELOAD_GEO_FENCE_SETTING,
                        geofence_setting,
                        area_scope);
            }else if(area_scope == AREA_ONE && area_id > 0){
                sprintf(message_content, "%d;%d;%s;%d;%d;%d;%d", 
                        from_gui, 
                        setting_reload, 
                        BOT_SERVER_API_VERSION_LATEST, 
                        CMD_RELOAD_GEO_FENCE_SETTING,
                        area_scope,
                        area_id,
                        geofence_setting);
            }else{
                printf("invalid argument for option -f or -a, " \
                       "use option -h to see the usage.\n");
                //display_usage();
                return -1;
            }
        }else{
            printf("invalid argument for option -f or -r, " \
                   "use option -h to see the usage.\n");
            //display_usage();
            return -1;
        }    
    }else{
        printf("invalid argument: option -p or -c, " \
               "use option -h to see the usage.\n");
        //display_usage();
        return -1;
    }

    if(verbose_mode){
        printf("IPC message content = [%s]\n", message_content);
    }

    sockVersion = MAKEWORD(2,2);

    if(WSAStartup(sockVersion, &wsaData) != 0)
         return -1;

    /* create a send UDP socket */
    if ((send_socket = 
         socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        if(verbose_mode){
            printf("create send socket failed\n");
        }
        return -1;
    }

    memset(&si_send, 0, sizeof(si_send));
    si_send.sin_family = AF_INET;
    si_send.sin_port = htons(server_port);
    si_send.sin_addr.s_addr = inet_addr(LOCAL_SERVER_IP);
  
    if (sendto(send_socket, message_content, 
               sizeof(message_content), 0,
               (struct sockaddr *)&si_send, sizeof(struct sockaddr)) == -1){
        if(verbose_mode){
            printf("sendto error.[%s]\n", strerror(errno));
        }
        closesocket(send_socket);
        WSACleanup();
        return -1;

    }else{
        if(verbose_mode){
            printf("sent successfully\n");
        }
    }

    closesocket(send_socket);
    WSACleanup();

	return 0;
}

