/*
  Copyright (c) 2016 Academia Sinica, Institute of Information Science

  License:

     GPL 3.0 : The content of this file is subject to the terms and cnditions
     defined in file 'COPYING.txt', which is part of this source code package.

  Project Name:

     BeDIS

  File Name:

     Geo-Fencing.c

  File Description:

     This header file contains the function declarations and variables used in
     the Geo-Fencing.c file.

  Version:

     1.0, 20190407

  Abstract:

     BeDIS uses LBeacons to deliver 3D coordinates and textual descriptions of
     their locations to users' devices. Basically, a LBeacon is an inexpensive,
     Bluetooth Smart Ready device. The 3D coordinates and location description
     of every LBeacon are retrieved from BeDIS (Building/environment Data and
     Information System) and stored locally during deployment and maintenance
     times. Once initialized, each LBeacon broadcasts its coordinates and
     location description to Bluetooth enabled user devices within its coverage
     area.

  Authors:

     Gary Xiao     , garyh0205@hotmail.com

 */

#ifndef GEO_FENCING_H
#define GEO_FENCING_H

#include "BeDIS.h"


typedef struct {

    int decision_threshold;

    /* UDP for receiving or sending message from and to the BOT server */
    sudp_config udp_config;

    /* The port number which use for receiving message from the BOT server */
    int recv_port;

    /* The port number which is use for the api module to receive message from
       modules such as Geo-Fencing. */
    int api_recv_port;

    /* Worker threads for processing data from the BOT server by assign a
       worker thread. */
    Threadpool worker_thread;

    Memory_Pool pkt_content_mempool;

    Memory_Pool tracked_mac_list_head_mempool;

    Memory_Pool uuid_list_node_mempool;

    /* Number of schedule_worker allow to use */
    int number_worker_threads;

    bool is_running;

    pthread_t process_api_recv_thread;

} sgeo_fence_config;

typedef sgeo_fence_config *pgeo_fence_config;


typedef struct {

    char ip_address[NETWORK_ADDR_LENGTH];

    char content[WIFI_MESSAGE_LENGTH];

    int content_size;

    pgeo_fence_config geo_fence_config;

} spkt_content;

typedef spkt_content *ppkt_content;


typedef struct {

    char mac_address[LENGTH_OF_MAC_ADDRESS];

    List_Entry mac_list_entry;

    List_Entry uuid_list_head;

} stracked_mac_list_head;

typedef stracked_mac_list_head *ptracked_mac_list_head;


typedef struct {

    char uuid[UUID_LENGTH];

    int rssi;

    int first_time;

    int last_time;

    List_Entry uuid_list_node;

} suuid_list_node;

typedef suuid_list_node *puuid_list_node;


/*
  geo_fence_initial:

     This function initialize API and sockets in Geo-Fence system.
     * Sockets including UDP sender and receiver.

  Parameters:

     geo_fence_config - The pointer points to the geo_fence_config.
     number_worker_threads - The number of worker thread allow to use.
     recv_port - The port number to which the api is sent.
     api_recv_port - The port number of the api for receiving data from modules.

  Return value:

     ErrorCode

 */
ErrorCode geo_fence_initial(pgeo_fence_config geo_fence_config,
                            int number_worker_threads, int recv_port,
                            int api_recv_port, int decision_threshold);


/*
  geo_fence_free:

     The function release sockets for sending and receiving.

  Parameters:

     geo_fence_config - The pointer points to the geo fence config.

  Return value:

     ErrorCode

 */
ErrorCode geo_fence_free(pgeo_fence_config geo_fence_config);


/*
  process_geo_fence_routine:

     This function is executed by worker threads when processing filtering of
     invalid mac addresses.

  Parameters:

     _pkt_content - The pointer points to the pkt content.

  Return value:

     None

 */
static void *process_geo_fence_routine(void *_pkt_content);


/*
  process_api_recv:

     This function is executed by worker threads when reveiving msgs from
     the BOT server.

  Parameters:

     _geo_fence_config - The pointer points to the geo fence config.

  Return value:

     None

 */
static void *process_api_recv(void *_geo_fence_config);


/*
  init_tracked_mac_list_head:

     This function initialize tracked mac list head by intializing mac list
     entry and lbeacon list head;

  Parameters:

     tracked_mac_list_head - The pointer points to the tracked mac list head.

  Return value:

     ErrorCode

 */
static ErrorCode init_tracked_mac_list_head(
                                  ptracked_mac_list_head tracked_mac_list_head);


/*
  init_uuid_list_node:

     This function initialize tracked uuid list node by initializing lbeacon
     list node.

  Parameters:

     uuid_list_node - The pointer points to the uuid list node.

  Return value:

     ErrorCode

 */
static ErrorCode init_uuid_list_node(puuid_list_node uuid_list_node);


/*
  is_in_mac_list:

     This function check whether the mac address is in the tracked mac list.

  Parameters:

     geo_fence_config - The pointer points to the geo fence config.
     mac_address - The pointer points to the mac address.

  Return value:

     If it find the mac, it will return its pointer of the node or it will
     return NULL.

 */
static ptracked_mac_list_head is_in_mac_list(pgeo_fence_config geo_fence_config,
                                             char *mac_address);

/*
  is_in_uuid_list:

     This function check whether the mac address is in the tracked mac list.

  Parameters:

     tracked_mac_list_head - The pointer points to the tracked mac list head.
     uuid - The pointer points to the uuid.

  Return value:

     If it find the uuid, it will return its pointer of the node or it will
     return NULL.

 */
static puuid_list_node is_in_uuid_list(
                                   ptracked_mac_list_head tracked_mac_list_head,
                                   char *uuid);


#endif
